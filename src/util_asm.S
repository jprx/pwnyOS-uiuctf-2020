// Fast utility functions
.global _memset_long, _memset_byte
.global _memcpy_long, _memcpy_byte
.global get_eip

// void memset(char *dest, char copyval, size_t size)
_memset_byte:
    // Use x86 string instructions to be fast
    pushl %edi

    // Set EDI to place to store
    // Set ECX to number of bytes
    // Set AL to byte to write
    movl 16(%esp), %ecx
    mov 12(%esp), %al
    movl 8(%esp), %edi

    // Repeatedly store this byte:
    rep stosb

    popl %edi

    ret

// void memcpy(uint8_t *to, uint8_t *from, size_t max_bytes)
_memcpy_byte:
    // Use x86 string instructions to be fast
    pushl %edi
    pushl %esi

    // Set EDI to place to store
    // Set ECX to number of bytes
    // Set ESI to source string
    movl 20(%esp), %ecx
    movl 16(%esp), %esi
    movl 12(%esp), %edi

    // String copy
    rep movsb

    popl %esi
    popl %edi
    ret

// void memset(char *dest, uint32_t copyval, size_t size)
_memset_long:
    // Use x86 string instructions to be fast
    pushl %edi

    // Set EDI to place to store
    // Set ECX to number of bytes
    // Set EAX to value to write
    movl 16(%esp), %ecx
    movl 12(%esp), %eax
    movl 8(%esp), %edi

    // Repeatedly store this 32 bit value (long):
    rep stosl

    popl %edi

    ret

// void memcpy(uint32_t *to, uint32_t *from, size_t max_integers)
_memcpy_long:
    // Use x86 string instructions to be fast
    pushl %edi
    pushl %esi

    // Set EDI to place to store
    // Set ECX to number of bytes
    // Set ESI to source string
    movl 20(%esp), %ecx
    movl 16(%esp), %esi
    movl 12(%esp), %edi

    // String copy
    rep movsd

    popl %esi
    popl %edi
    ret

get_eip:
    movl (%esp), %eax
    ret
