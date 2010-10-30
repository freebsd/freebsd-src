SECTIONS
{
  .text : { *(.text) }
  OVERLAY 0x1000 : AT (0x4000)
  {
    .text1 {*(.text1)}
    .text2 {*(.text2)}
  }
  /DISCARD/ : { *(.*) }
}
