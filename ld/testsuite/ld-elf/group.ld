SECTIONS
{
  . = 0x1000;
  .text : { *(.text) *(.rodata.brlt) }
  /DISCARD/ : { *(.reginfo) }
}
