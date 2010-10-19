ENTRY(__start)
SECTIONS
{
  . = 0x0400000;
  .text : { *(.text) }
  . = 0x0800000;
  .rodata : { *(.rodata.*) }
  /DISCARD/ : { *(*) }
}
