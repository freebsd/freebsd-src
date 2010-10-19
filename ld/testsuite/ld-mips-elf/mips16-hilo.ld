SECTIONS
{
  . = 0x0500000;
  .text : { *(.text) }
  . = 0x0654320;
  .data : { *(.data) }
  .sdata : { *(.sdata) }
  . = 0x0765430;
  .bss : { *(.bss) *(COMMON) }
  .sbss : { *(.sbss) *(.scommon) }
  /DISCARD/ : { *(*) }
}
