SECTIONS
{
  .text : { *(.text) }
  /* Alignment at beginning shouldn't result in empty section being kept.  */
  .text1 ALIGN (4096) :
  {
    *(.text1)
  }
  /* Same for alignment at beginning and end.  */
  .text2 ALIGN (4096) :
  {
    *(.text2)
    . = ALIGN (4096);
  }
  /* Same for alignment just at end, although we need to be careful in
     the expression used to align.  */
  .text3 :
  {
    *(.text3)
    . = ALIGN (. != 0 ? 4096 : 1);
  }
  /* Same when setting vma and lma.  This also shouldn't result in
     .text3 being kept.  */
  .text4 ADDR (.text3) + SIZEOF (.text3) + 8192 : AT (LOADADDR (.text3))
  {
    *(.text4)
  }
  /DISCARD/ : { *(*) }
}
