NOCROSSREFS ( .text .data )
SECTIONS
{
  .text : { *(.text) *(.text.*) *(.pr) }
  .data : { *(.data) *(.data.*) *(.sdata) *(.rw) *(.tc0) *(.tc) *(.toc) }
}
