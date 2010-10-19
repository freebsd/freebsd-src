NOCROSSREFS ( .text .data )
SECTIONS
{
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .hash : { *(.hash) }
  .toc  : { *(.toc) }
  .text : { tmpdir/cross1.o }
  .data : { tmpdir/cross2.o }
}
