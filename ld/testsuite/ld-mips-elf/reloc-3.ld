SECTIONS
{
  . = 0x208000;
  .text : { *(.text) }
  . = 0x310000;
  .text2 : { *(.text2) }
  /DISCARD/ : { *(*) }
}
