#include "scheduler.h"
#include "process.h"
#include "interrupt.h"
#include "util.h"

void scheduler_pass() {
    hw_pic_eoi(IRQ_PIT);

    // Step 1: Save current process kernel stack
    // I think we can get away with just using esp and forgetting ebp; it'll get corrected
    // during linkage at the end of this call
    if (NULL != current_proc) {
        asm volatile (
            "movl %%esp, %0\n" \
            "movl %%ebp, %1\n" \
            : "=r"(current_proc->ksp), "=r"(current_proc->kbp)
        );
    }

    // Step 2: Choose the next process to run
    uint32_t current_proc_idx = 0;

    // Set current_proc_idx to the index in the process table that current_proc is
    current_proc_idx = (current_proc != NULL) ? (current_proc - (pcb_t*)processes) : 0;

    // Ensure the index is in range (ie, current_proc points to something in the table)
    if (current_proc_idx >= MAX_PROCESSES) {
        current_proc_idx = 0;
    }

    pcb_t *next_proc = current_proc;
    uint32_t j;
    for (j = 0; j < MAX_PROCESSES; j++) {
        // Map i of 0 to current proc, and i of MAX_PROCESSES-1 to the process right before current_proc,
        // wrapping around as necessary:
        uint32_t idx = (current_proc_idx + 1 + j) % MAX_PROCESSES;
        
        if (processes[idx].in_use && !processes[idx].blocking_execute && !processes[idx].sleeping) {
            // This is the process we want
            next_proc = &processes[idx];
            break;
        }
    }

    if (next_proc != NULL) {
        // Step 3: Configure page mappings for the new process
        process_switch(next_proc);

        // Step 3.5: If this process should be killed, kill it
        if (next_proc->should_die) {
            sysret(-8);
        }

        // Step 4: Swap to that process's kernel stack (restore KSP)
        // Step 5: GTFO!
        asm volatile (
            "movl %0, %%esp\n" \
            "movl %1, %%ebp\n" \
            "leave\n" \
            "ret\n" \
            :
            : "rm"(current_proc->ksp), "rm"(current_proc->kbp)
        );
    }
    else {
        // No available processes, go to sleep
        // sti();
        // asm volatile ("hlt");
        return;
    }
}

// Configure PIT (IRQ 0)
void init_pit () {
    // outb(0x43, 0x36);
    // outb(0x40, 0x9b);
    // outb(0x40, 0x2e);
}
