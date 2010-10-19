SECTIONS
{
  . = 0x80000;
  .interp : { *(.interp) }
  .hash : { *(.hash) }
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .rel.dyn : { *(.rel.dyn) }
  .MIPS.stubs : { *(.MIPS.stubs) }
  .text : { *(.text) }

  . = ALIGN (0x10000);
  _gp = . + 0x7ff0;
  .got : { *(.got) }

  /DISCARD/ : { *(.reginfo) }
}
