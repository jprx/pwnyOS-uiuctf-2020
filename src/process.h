#ifndef PROCESS_H
#define PROCESS_H
#include "defines.h"
#include "types.h"
#include "file.h"
#include "user.h"
#include "paging.h"

#define KERNEL_STACK_SIZE ((PAGE_SIZE * 4))

#define PROC_VIRT_ADDR ((0x08048000))
#define MMAP_VIRT_ADDR ((0x0D048000))

#define NUM_FDS ((32))

#define MAX_PROCESSES ((32))

// 0x7F followed by "ELF" but in little-endian order
#define ELF_MAGIC ((0x464c457f))

// Process Control Block (PCB)
typedef struct pcb_t {

    // Saved kernel stack pointer:
    // Switch to this during context switch from process to process
    /* 
     * When process A (some other process) is descheduled, it will enter an interrupt handler using
     * esp0 from TSS. To swap to process B (this process), we need to put this KSP into TSS esp0.
     * When this process is interrupted we are now in the correct kernel stack. Namely, we will be
     * exactly back in the kernel right where we last left this process. The only place a process can
     * be swapped away from is during a call to context_switch. This means our stack frame is the same
     * frame that called context_switch. This is an interrupt context where we can return to, and then
     * perform IRET back into the user app that called that interrupt handler.
     */
    // These are mainly used by the scheduler, but are also written to in execute if this process is a parent
    // of another process. In execute, they point to a frame in process_launch (so that if the execution is
    // non-blocking, when scheduler reschedules this process we come out of process_launch right away).
    uint32_t ksp;
    uint32_t kbp;

    // Kernel stack to return to when this process halts:
    /*
     * When this process is complete, we can't leave context_switch and IRET back to userspace, because
     * userspace is gone! Some other kernel stack had to be responsible for calling the original context_switch
     * that created this process and we should return to that context. That's what parent_ksp does. During
     * SYSRET we swap to this parent stack and return to it- this will bring us back to wherever in the kernel
     * called this child process, allowing us to pass back the user process return value to it.
     */
    // These are only read on sysret, and only set during execute
    uint32_t parent_ksp;
    uint32_t parent_kbp;

    /*
     * phys_addr
     *
     * Physical address of the memory storing this process
     */
    void *phys_addr;

    /*
     * mmap_phys_addr
     *
     * Physical address of the page mapped by mmap, if any
     */
    void *mmap_phys_addr;

    // File descriptor array:
    fd_t fds[NUM_FDS];

    // The name of this process:
    char name[FS_NAME_LEN];

    // UID of this process:
    // (Ignore this for kernel processes, even if it is 0)
    // Root user is still a user
    uid_t uid;

    // Are we in use?
    bool in_use;

    // Is this a kernel process?
    // If we encounter an exception while current_proc is a kernel process, we can't just call sysret
    bool kern_proc;

    // Is this process running a blocking execute? (This is parent-process-specific)
    bool blocking_execute;

    // Is this a nonblocking process execution?
    // When true, this causes sysret to just destroy the process and call scheduler_pass,
    // insted of trying to return to its caller.
    // (This is child-process-specific)
    bool nonblocking;

    // Are we asleep?
    bool sleeping;
    uint32_t num_ticks_remaining;

    // Next time this process is scheduled it'll execute sysret
    bool should_die;

    // Does this process have the set user ID bit set?
    bool set_uid_enabled;

    // User ID config:
    bool set_uid_blocking; // is this supposed to be run as a blocking process?
    uid_t set_uid_val;  // UID val


    // Kernel stack for this process:
    uint8_t kern_stack[KERNEL_STACK_SIZE];
} pcb_t;

// Setup a PCB
pcb_t *alloc_pcb();

// Process methods:
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
pcb_t *process_create (char *filename, uid_t uid, bool kernel_proc);

void process_destroy(pcb_t *process);

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
uint32_t process_launch(pcb_t *process, void *code_addr, void *stack_addr, pcb_t *parent, bool nonblocking, bool kernel_proc);

/*
 * process_launch_user
 *
 * Sets up and launches a new user process. This function will appear to return with the
 * return value asked for by the caller.
 *
 * Uses the ELF entrypoint as code address, and USER_STACK_OFFSET into the huge page as the stack base.
 *
 * process- the PCB to use.
 */
uint32_t process_launch_user(pcb_t *process);

/*
 * process_switch
 *
 * process- process to switch to
 *
 * Switch this PCB to be the active PCB
 * Configure TSS, current_proc, and pages
 * NEVER write directly to current_proc- use this method instead!
 */
void process_switch (pcb_t *process);

// Mark all pcbs as free
void setup_pcb_table(void);

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
void context_switch(void *virt_addr, void *stack_addr, bool kernel_task);

/********************
 * Execute Variants *
 ********************/
uint32_t execute_user(char *fname, uid_t user_id, bool nonblocking);
uint32_t execute_kern(void *addr, uid_t user_id, char *thread_name, bool nonblocking);
uint32_t execute_user_blocking (char *fname, uid_t user_id);
uint32_t execute_user_nonblocking (char *fname, uid_t user_id);
uint32_t execute_kern_blocking (void *addr, uid_t user_id, char *thread_name);
uint32_t execute_kern_nonblocking (void *addr, uid_t user_id, char *thread_name);

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
uint32_t execute(char *fname, uid_t user_id, bool nonblocking, bool kernel_proc, void *kernel_start_addr);

// Sysret syscall:
uint32_t sysret(uint32_t retval);

// Exec syscall:
uint32_t sysexec(char *fname);

// mmap syscall:
uint32_t sys_mmap(void);

// Remote switch user:


/*
 * create_kernel_process_here
 *
 * Convert the current location to a scheduable kernel process
 * Calling this allocates a new PCB and makes it scheduable, corresponding to exactly
 * where we are in the kernel.
 *
 * This would allow us to use entry for everything and not have to rely on setting up a
 * dedicated launcher thread
 *
 * @TODO: Make this work
 */
/*
static inline int32_t create_kernel_process_here () {
    pcb_t *new_pcb = alloc_pcb();

    if (!new_pcb) return -1;

    new_pcb->kern_proc = true;
    new_pcb->uid = 0;

    uint32_t cur_eip, cur_esp;
    asm volatile (
        "movl %%esp, %1\n" \
        "call get_eip\n" \
        "movl %%eax, %0\n" \
        : "=r"(cur_eip), "=r"(cur_esp)
    );

    process_launch_kernel(new_pcb, cur_eip, cur_esp);
}
*/

extern pcb_t *current_proc;

extern pcb_t processes[MAX_PROCESSES];

#include "sandbox.h"
#ifdef UIUCTF
void process_sleep(uint32_t time);
#endif

#endif
