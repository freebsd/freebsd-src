ENTRY(_entry)
PHDRS
{
  data PT_LOAD;
}
SECTIONS
{
  . = 0x1000000;
  .foo : { *(.foo) } :data
  . = 0x2000000;
  .bar : { *(.bar) } :data
  /DISCARD/ : { *(*) }
}
