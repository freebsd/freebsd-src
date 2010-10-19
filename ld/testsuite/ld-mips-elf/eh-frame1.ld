SECTIONS
{
  . = 0x10000;
  .dynamic : { *(.dynamic) }
  .hash : { *(.hash) }
  .dynsym : { *(.dynsym) }
  .dynstr : { *(.dynstr) }
  .rel.dyn : { *(.rel.dyn) }

  . = 0x20000;
  .text : { *(.text) }

  . = 0x30000;
  .eh_frame : { *(.eh_frame) }
  .got : { *(.got) }
  .gcc_compiled_long32 : { *(.gcc_compiled_long32) }

  /DISCARD/ : { *(*) }
}
