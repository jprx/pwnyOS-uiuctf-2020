#ifndef SCHEDULER_H
#define SCHEDULER_H

/*
 *
 * Chose a new process to run, and does the following:
 *  - Saves current process kernel stack & page configuration
 *  - Maps pages to match the process
 *  - Swaps kernel stack to the next process's kernel stack
 *  - Executes a return instruction
 *     -> If this was called by kernel code, we good (continues where we left off)
 *     -> If this was called by an interrupt, we also good (pops down to asm linkage)
 */
void scheduler_pass(void);

// Scheduler interrupt entrypoint:
extern void scheduler_entry(void);

// Configure PIT (IRQ 0)
void init_pit (void);

#endif
