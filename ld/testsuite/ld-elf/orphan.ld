SECTIONS
{
  .text : { *(.text) }
  .data : { *(.data) }
  .bss : { *(.bss) *(COMMON) }
  .note : { *(.note) }
  .reginfo : { *(.reginfo) }
}
