NOCROSSREFS ( .text .data )
SECTIONS
{
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .hash : { *(.hash) }
  .toc  : { *(.toc) }
  .opd  : { *(.opd) }
  .text : { tmpdir/cross1.o }
  .data : { tmpdir/cross2.o }
}
