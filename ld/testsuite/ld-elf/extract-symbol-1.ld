ENTRY(_entry)
PHDRS
{
  data PT_LOAD AT (0);
}
SECTIONS
{
  . = 0x10000;
  .foo : { *(.foo) } :data

  . = 0x20000;
  .bar : { *(.bar) } :data

  /DISCARD/ : { *(*) }

  _entry = 0x30000;
  linker_symbol = 0x40000;
}
