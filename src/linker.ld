ENTRY(_start)
SECTIONS
{
  . = 4M;
  .text.start : ALIGN(0x1000) { kernel_slide = .; start.o(.text); *(.text); }
  .data : ALIGN(0x1000) { * (.data); }
  .bss : ALIGN(0x1000) { * (.bss); }
}