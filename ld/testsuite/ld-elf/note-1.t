ENTRY(_entry)
PHDRS
{
  data PT_LOAD;
  note PT_NOTE;
}
SECTIONS
{
  . = 0x1000000;
  .foo : { *(.foo) } :data
  . = 0x2000000;
  .note : { *(.note) } :note
  /DISCARD/ : { *(*) }
}
