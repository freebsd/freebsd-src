SECTIONS
{
  . = 0x208000;
  .text : { *(.text) }
  . = 0x400000;
  _gp = 0x401234;
  .sdata : { *(.sdata) }
  /DISCARD/ : { *(*) }
}
