#include "process.h"
#include "x86_stuff.h"
#include "util.h"
#include "types.h"
#include "filesystem.h"
#include "paging.h"
#include "stdio.h"
#include "scheduler.h"
#include "sandbox.h"

pcb_t processes[MAX_PROCESSES];

#define USER_STACK_OFFSET ((0x200000))

// Current process running
pcb_t *current_proc;

// Mark all pcbs as free
void setup_pcb_table() {
    uint32_t i = 0;
    for (i = 0; i < MAX_PROCESSES; i++) {
        processes[i].in_use = false;
    }
    current_proc = NULL;
}

// Setup a PCB
pcb_t *alloc_pcb() {
    uint32_t i;
    uint32_t flags = cli_and_save();
    pcb_t *new_pcb = NULL;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (!processes[i].in_use) {
            processes[i].in_use = true;
            new_pcb = &(processes[i]);
            break;
        }
    }

    if (!new_pcb) return NULL;

    new_pcb->ksp = 0;
    new_pcb->kbp = 0;
    new_pcb->parent_ksp = 0;
    new_pcb->parent_kbp = 0;
    new_pcb->phys_addr = NULL;
    new_pcb->mmap_phys_addr = NULL;
    new_pcb->uid = 0;
    new_pcb->kern_proc = false;
    new_pcb->blocking_execute = false;
    new_pcb->nonblocking = false;
    new_pcb->sleeping = false;
    new_pcb->num_ticks_remaining = 0;
    new_pcb->should_die = false;
    new_pcb->set_uid_enabled = false;
    new_pcb->set_uid_blocking = false;
    new_pcb->set_uid_val = 0;

    for (i = 0; i < NUM_FDS; i++) {
        new_pcb->fds[i].in_use = false;
    }

    memset((char *)&new_pcb->name, '\0', FS_NAME_LEN);

    restore_flags(flags);
    return new_pcb;
}

// Setup FD 0 for a new process
void _process_setup_stdio (pcb_t *new_pcb) {
    new_pcb->fds[0].mount = &stdio_mount;
    new_pcb->fds[0].in_use = true;
    new_pcb->fds[0].mount->ops.open(&new_pcb->fds[0], STDIO_MOUNT);
}

// Setup a user PCB
pcb_t *_process_create_user (char *filename, uid_t uid) {
    void *huge_page = NULL;

    // Locate file and read it
    // @TODO: Use open(), read() abstractions
    if (!filename) return NULL;

    // Try to open file:
    xentry *found_entry = filesys_lookup(filename);
    if (!found_entry) return NULL;

    // Try to allocate a huge page:
    huge_page = alloc_huge_page();
    if (!huge_page) return NULL; // ENOMEM

    // Map this huge page & read into it:
    map_huge_page(PROC_VIRT_ADDR, (uint32_t)huge_page, true, true);

    // Just 0 out the first address, in case the file we are reading is empty
    // If it is empty, and this page used to be an ELF, then we will be executing that code
    #ifndef UIUCTF
    *(uint32_t *)PROC_VIRT_ADDR = 0;
    #endif

    filesys_read_bytes(found_entry, 0, (int8_t*)PROC_VIRT_ADDR, (1 << 22));

    // Check the file we are opening to see if its actually an ELF file:
    if (((*(uint32_t *)PROC_VIRT_ADDR) & 0xFFFFFF00) != (ELF_MAGIC & 0xFFFFFF00)) {
        goto PROCESS_CREATE_CLEANUP;
    }

    // Read ELF permissions bit:
    char elf_special_mode = *(uint32_t *)PROC_VIRT_ADDR & 0x0FF;

    // Setup PCB:
    pcb_t *new_pcb = alloc_pcb();
    if (!new_pcb) {
        goto PROCESS_CREATE_CLEANUP;
    }

    new_pcb->phys_addr = huge_page;

    // Setup standard io
    _process_setup_stdio(new_pcb);

    // This isn't a kernel process
    new_pcb->kern_proc = false;

    new_pcb->uid = uid;

    // Setup Set UID bit config
    if (elf_special_mode == 0x7f) {
        // Regular ELF
        new_pcb->set_uid_enabled = false;
    }
    else {
        // If bit 8 set, then this is blocking. Else non blocking
        new_pcb->set_uid_enabled = true;
        new_pcb->set_uid_blocking = ((elf_special_mode & 0x80) != 0) ? true : false;
        new_pcb->set_uid_val = 0x07F & (elf_special_mode & (~0x80));
    }

    return new_pcb;

PROCESS_CREATE_CLEANUP:
    // If something failed, free huge page
    free_huge_page(huge_page);
    return NULL;
}

// Setup a kernel PCB
pcb_t *_process_create_kern(uid_t uid) {
    pcb_t *new_pcb = alloc_pcb();
    new_pcb->kern_proc = true;
    new_pcb->uid = uid;

    // Setup standard io
    _process_setup_stdio(new_pcb);

    return new_pcb;
}

/*
 * process_create
 *
 * Create a new process by allocating a PCB, allocating any necessary memory, and
 * for user processes, read the ELF file to run into memory.
 *
 * filename- File to execute (irrelevant for kernel processes)
 * uid- The user ID to run this process on behalf of
 * kernel_proc- When true, we setup the PCB to be a kernel PCB
 *
 * For user processes:
 * Sets up a new PCB, loads ELF into memory, and updates page mappings accordingly.
 * Can fail because filename doesn't exist, isn't an ELF, or no more PCBs free
 * WARNING: Even if this fails, the page table mappings will remain modified!
 * @TODO: Make this clean page tables up
 *
 * For kernel processes:
 * Allocates a new PCB with no file descriptors and marks it as a kernel process.
 * All we care about in that case is the KSP field- that's what scheduler uses to
 * find where to jump to to continue executing code.
 *
 * Return Value: A pointer to the new PCB
 */
pcb_t *process_create (char *filename, uid_t uid, bool kernel_proc) {
    pcb_t *new_pcb = NULL;

    // To save the name, we need to copy it before we call _process_create_* because those change
    // the page tables, if the name comes from userspace it'll be lost
    char name_buf[FS_NAME_LEN];
    strncpy((char *)&name_buf, filename, FS_NAME_LEN);

    if (!kernel_proc) {
        // User process
        new_pcb = _process_create_user(filename, uid);
    }
    else {
        // Kernel process
        new_pcb = _process_create_kern(uid);
    }

    if (!new_pcb) {
        return new_pcb;
    }

    // Common code for both:

    // Copy first FS_NAME_LEN chars to this name
    // That's enough for any given file, but filename is an absolute path, so it may get truncated
    // Name is only used by proclist syscall though so its ok for now
    // @TODO: Copy just the name of the binary, not whole path, or make more room for name
    strncpy((char *)&new_pcb->name, name_buf, FS_NAME_LEN);

    return new_pcb;
}

/*
 * process_switch
 *
 * process- process to switch to
 *
 * Switch this PCB to be the active PCB
 * Configure TSS, current_proc, and pages
 * NEVER write directly to current_proc- use this method instead!
 */
void process_switch (pcb_t *process) {
    // Set stack frame for future syscalls:
    tss.esp0 = (uint32_t)&process->kern_stack[KERNEL_STACK_SIZE-1];

    // Setup pages:
    map_huge_page(PROC_VIRT_ADDR, (uint32_t)process->phys_addr, true, true);

    if (process->mmap_phys_addr) {
        map_huge_page_user(MMAP_VIRT_ADDR, (uint32_t)process->mmap_phys_addr);
    }
    else {
        #ifdef UIUCTF
        // If we are the sandbox user, don't unmap any existing mmapp'ed pages
        if (process->uid != SANDBOX_USER) {
            unmap_huge_page(MMAP_VIRT_ADDR);
        }
        #else
        unmap_huge_page(MMAP_VIRT_ADDR);
        #endif
    }

    // Mark this process as the active one:
    current_proc = process;

    // @TODO: Switch typeables?
}

/*
 * process_launch
 *
 * Sets up and launches a new process. This function will appear to return with the
 * return value asked for by the caller.
 *
 * process- the PCB to use.
 * code_addr- Address to execute
 * stack_addr- Address to use for the stack
 * parent- Parent process, leave NULL if this process doesn't return to a parent (IE called from entry in kernel.c)
 * nonblocking- When this is true, process_launch will appear to return next time the parent process is called
 *              This field is irrelevant if parent is NULL.
 * kernel_proc- is this process a kernel process?
 */
uint32_t process_launch(pcb_t *process, void *code_addr, void *stack_addr, pcb_t *parent, bool nonblocking, bool kernel_proc) {
    if (!process) return 0;
    asm volatile (
        "movl %%esp, %0\n"  \
        "movl %%ebp, %1\n"  \
        : "=r"(process->parent_ksp), "=r"(process->parent_kbp)
    );

    if (parent != NULL) {
        // KSP is only read by scheduler_pass
        // For nonblocking processes, this KSP needs to point to this method
        // When the parent process is scheduled, scheduler_pass will call leave/ ret on this stack frame,
        // returning to wherever process_launch was called.
        asm volatile (
            "movl %%esp, %0\n" \
            "movl %%ebp, %1\n" \
            : "=r"(parent->ksp), "=r"(parent->kbp)
        );

        // If child is nonblocking, then this isn't a blocking execute
        parent->blocking_execute = !nonblocking;
    }

    // Configure process settings per the launch request:
    process->kern_proc = kernel_proc;
    process->nonblocking = nonblocking;

    if (!kernel_proc) {
        display_user_colorscheme(process->uid);
    }

    process_switch(process);
    context_switch(code_addr, stack_addr, kernel_proc);

    // We actually never get here:
    // SYSRET will handle returning from this stack frame with the correct return value
    // It does this by restoring esp and ebp and then performing leave, ret
    // Whoever called process_start will see a return value appear when sysret is called by the new process
    return 0;
}

// Destroy a PCB
// This is called by sysret
void process_destroy(pcb_t *process) {
    if (!process) return;

    // Free the huge page associated with this process:
    if (process->phys_addr) {
        free_huge_page(process->phys_addr);
    }
    if (process->mmap_phys_addr) {
        free_huge_page(process->mmap_phys_addr);
    }

    process->in_use = false;

    if (current_proc == process) {
        current_proc = NULL;
    }
}

/*
 * context_switch
 *
 * This is the method responsible for dispatching new processes at the CPU level.
 * It is capable of switching to either a user process or a kernel process using the IRET mechanism.
 *
 * It will jump to a new execution context at code_addr with stack at stack_addr
 *
 * code_addr: New EIP
 * stack_addr: New ESP
 * kernel_task: When set, new task executes in ring 0, not ring 3.
 *
 * It never returns.
 */
void context_switch(void *code_addr, void *stack_addr, bool kernel_task) {
    uint32_t flags = cli_and_save();

    // Create iret context:
    volatile iret_context iretctx;

    iretctx.eip = (uint32_t)code_addr;
    iretctx.cs = USER_CS;
    iretctx.esp = (uint32_t)stack_addr;
    iretctx.ss = USER_DS;

    if (!kernel_task) {
        // Launch a user task
        asm volatile(
            /* Reconfigure selectors */
            /* 16(eax) contains DS */
            "movl 16(%%eax), %%ebx\n" \
            "mov %%ebx, %%ds\n" \
            "mov %%ebx, %%es\n" \
            "mov %%ebx, %%fs\n" \
            "mov %%ebx, %%gs\n" \

            /* Push SS */
            "pushl 16(%%eax)\n" \

            /* Push ESP for target process */
            "pushl 12(%%eax)\n" \
            /*"pushl %%esp\n" \ */

            /* Push flags */
            /*"pushl 8(%%eax)\n" \*/
            /* Use current eflags value instead of whats in iretctx */
            "pushfl\n" \

            /* Push CS */
            "pushl 4(%%eax)\n" \

            /* Push EIP */
            "pushl (%%eax)\n" \

            /* Set the interrupt bit (mask 0x200, bit 9) of the eflags so user app can receive interrupts */
            "movl 8(%%esp), %%eax\n" \
            "orl $0x200,%%eax\n" \
            "movl %%eax, 8(%%esp)\n" \

            /* Do it! */
            "iret"

            :
            : "a"(&iretctx)
            : "ebx"
        );
    }
    else {
        // Launch a kernel task
        // Actually don't need to use IRET here; IRET for same-privilege-level transitions
        // won't let you set new stack pointer, so its not really useful.
        asm volatile(
            /* Setup stack */
            "movl 12(%%eax), %%esp\n" \

            /* Push return address to new stack and ret to it */
            /* Flags don't need to be configured because they don't change */
            /* I could use CALL here, but that would mean if the kernel task ever
             * returns, it comes back to context_switch with an invalid stack. 
             * Since kernel tasks shouldn't ever return (they should call sysret),
             * it's cool to do this instead so if they try to return they'll try to go to NULL */

            /* Where the kernel task returns to if it ever returns: */
            "pushl $0\n" \

            /* Jump to the new kernel task using its stack as a springboard */
            "pushl (%%eax)\n" \
            "ret\n" \
            :
            : "a"(&iretctx)
            :
        );
    }

    restore_flags(flags);
}

// Return from a process:
uint32_t sysret(uint32_t retval) {
    // Destroy the current process:
    // First save stack frame info for later (after process_destroy, current_proc is NULL)
    uint32_t ret_ksp = current_proc->parent_ksp;
    uint32_t ret_kbp = current_proc->parent_kbp;
    bool was_nonblocking = current_proc->nonblocking;

    if (current_proc->mmap_phys_addr) {
        unmap_huge_page(MMAP_VIRT_ADDR);
    }

    // After this, current_proc is NULL!
    process_destroy(current_proc);

    // If this was a nonblocking process, don't worry about returning back to process_launch context
    if (was_nonblocking) {
        // After calling this, we will never be rescheduled:
        scheduler_pass();

        // If we got here, we were the last process
        // End of the line, folks
        asm volatile ("hlt");
    }

    // This will place us back in the process_launch context
    asm volatile(
        /* Need to move retval before swapping stacks, otherwise */
        /* retval will be wrong (stack changed and retval is on stack) */
        /* Also need to mark eax as a clobber so assembler won't use it for setting SP and BP */
        "movl %0, %%eax\n"      \
        "movl %1, %%esp\n"      \
        "movl %2, %%ebp\n"      \
        "leave\n"               \
        "ret"
        :
        : "rm"(retval), "rm"(ret_ksp), "rm"(ret_kbp)
        : "eax"
    );

    // Never hits this:
    return retval;
}

/*
 * execute
 *
 * This is the main method for launching any kind of process.
 *
 * It can launch processes under a variety of conditions
 *
 * nonblocking: If set, the old process continues running alongside the new one.
 * kernel_proc: If set, the process runs as a kernel process
 * kernel_start_addr: For kernel processes, where the kernel process runs from
 */
uint32_t execute(char *fname, uid_t user_id, bool nonblocking, bool kernel_proc, void *kernel_start_addr) {
    int retcode = 0;

    // Save current process for later:
    pcb_t *our_proc = current_proc;

    // Try to create new process
    // *NOTE*: Now the page tables have been modified and need to be restored
    // before we return!
    // Inherit the UID from current proc
    pcb_t *new_proc = process_create(fname, user_id, kernel_proc);

    // If process creation failed, return with -1 code
    if (!new_proc) {
        retcode = -1;
    }
    else {
        // Read set uid bit config:
        if (new_proc->set_uid_enabled) {
            nonblocking = !new_proc->set_uid_blocking;
            new_proc->uid = new_proc->set_uid_val;
        }

        // Call the process:
        if (!kernel_proc) {
            // Launch user process:
            // First, read the entrypoint (need to map pages to do this)
            // Map pages using a technically redundant process_switch
            process_switch(new_proc);
            void *code_addr = (int8_t*)((uint32_t *)PROC_VIRT_ADDR)[6];
            void *stack_addr = ((int8_t*)(PROC_VIRT_ADDR + USER_STACK_OFFSET));

            // Launch user process:
            retcode = process_launch(new_proc, code_addr, stack_addr, our_proc, nonblocking, false);
        }
        else {
            // Launch kernel process:
            // To get stack, first use process_switch to setup TSS correctly, and then just read esp0 from that
            process_switch(new_proc);
            retcode = process_launch(new_proc, kernel_start_addr, (void *)tss.esp0, our_proc, nonblocking, true);
        }

        // Swap back to the previous process:
        process_switch(our_proc);

        if (NULL != our_proc) {
            // Mark ourselves as scheduable again:
            our_proc->blocking_execute = false;

            // Fix cosmetic styling (in case we crossed a user/ superuser boundary)
            if (!our_proc->kern_proc) {
                display_user_colorscheme(our_proc->uid);
            }
        }

        return retcode;
    }

    // Swap back to the previous process:
    process_switch(our_proc);

    // Return retcode to caller:
    return retcode;
}

/*
 * mmap
 *
 * Request another page of memory to do whatever with.
 *
 * This will grant a single huge page, and can only grant 1 page per process.
 * This returns the virtual address of the new huge page (or 0 if unsuccessful).
 */
uint32_t sys_mmap () {
    // A process only gets 1 mmap page
    if (current_proc->mmap_phys_addr) return NULL;
    current_proc->mmap_phys_addr = alloc_huge_page();

    // Map the new page:
    map_huge_page_user(MMAP_VIRT_ADDR, (uint32_t)current_proc->mmap_phys_addr);

    return MMAP_VIRT_ADDR;
}

/********************
 * Execute Variants *
 ********************/
uint32_t execute(char *fname, uid_t user_id, bool nonblocking, bool kernel_proc, void *kernel_start_addr);
uint32_t execute_user(char *fname, uid_t user_id, bool nonblocking) {
    return execute(fname, user_id, nonblocking, false, NULL);
}

uint32_t execute_kern(void *addr, uid_t user_id, char *thread_name, bool nonblocking) {
    return execute(thread_name == NULL ? "Unnamed Kernel Thread" : thread_name, user_id, nonblocking, true, addr);
}

uint32_t execute_user_blocking (char *fname, uid_t user_id) {
    return execute_user(fname, user_id, false);
}

uint32_t execute_user_nonblocking (char *fname, uid_t user_id) {
    return execute_user(fname, user_id, true);
}

uint32_t execute_kern_blocking (void *addr, uid_t user_id, char *thread_name) {
    return execute_kern(addr, user_id, thread_name, false);
}

uint32_t execute_kern_nonblocking (void *addr, uid_t user_id, char *thread_name) {
    return execute_kern(addr, user_id, thread_name, true);
}

// Execute a new process (blocking execute):
uint32_t sysexec(char *fname) {
    return execute_user_blocking(fname, current_proc->uid);
}

#ifdef UIUCTF
void process_sleep(uint32_t time) {
    if (current_proc) {
        current_proc->num_ticks_remaining = time;
        current_proc->sleeping = true;
        while (current_proc->sleeping) {
            asm volatile("hlt");
        }
    }
}
#endif
