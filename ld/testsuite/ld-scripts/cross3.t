NOCROSSREFS(.nocrossrefs .text)

SECTIONS
{
  .text : { *(.text) }
  .nocrossrefs : { *(.nocrossrefs) }
  .data : { *(.data) }
  .bss : { *(.bss) *(COMMON) }
  /DISCARD/ : { *(*) }
}
