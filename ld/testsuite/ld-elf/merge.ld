SECTIONS
{
  . = 0x1000;
  .text : { *(.text .text.* .gnu.linkonce.t.*) }
  .rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  .junk : { *(*) }
}
