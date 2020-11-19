ENTRY(_start)
SECTIONS
{
  . = 4M;
  .text.start : { start.o(.text); }

  .text : ALIGN(0x1000) {
  _TEXT_START = .;
    * (.text);
  _TEXT_END = .;
  }

  .data : ALIGN(0x1000) {
    * (.data);
  }

  .bss : ALIGN(0x1000) {
    * (.bss);
  }
}