// Exception vectors!
.extern crash
.global unknown_exception
unknown_exception:
    call crash
    iret

.extern page_fault_handler
.global page_fault_entry
page_fault_entry:
    // CR2 contains the faulty address
    movl %cr2, %eax
    pushl %eax
    call page_fault_handler
    iret

.extern double_fault_handler
.global double_fault_entry
double_fault_entry:
    call double_fault_handler
    iret

.extern gen_protect_fault_handler
.global gen_protect_fault_entry
gen_protect_fault_entry:
    # Apparently top of stack is bad address
    # But I don't care about that
    #movl (%esp), %eax
    #pushl %eax
    call gen_protect_fault_handler
    iret

page_fault:
    iret