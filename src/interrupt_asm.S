// Interrupt vectors!

// IRQ 0: PIT Interrupt (scheduler)
.extern scheduler_pass
.globl scheduler_entry
scheduler_entry:
    pushfl
    pushal
    call scheduler_pass
    popal
    popfl
    iret

// IRQ 1: Keyboard
.extern keyboard_handler
.global keyboard_handler_entry
keyboard_handler_entry:
    pushfl
    pushal
    call keyboard_handler
    popal
    popfl
    iret

// IRQ 8: RTC
.extern rtc_handler
.global rtc_handler_entry
rtc_handler_entry:
    pushfl
    pushal
    call rtc_handler
    popal
    popfl
    iret
