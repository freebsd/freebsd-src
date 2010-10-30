NOCROSSREFS(.nocrossrefs .text)

SECTIONS
{
  .text : { *(.text) }
  .nocrossrefs : { *(.nocrossrefs) }
  .data : { *(.data) *(.opd) }
  .bss : { *(.bss) *(COMMON) }
  /DISCARD/ : { *(*) }
}
