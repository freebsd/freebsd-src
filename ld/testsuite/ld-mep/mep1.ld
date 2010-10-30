SECTIONS
{
  /* This is beyond the normal range of a PCREL24 (bsr) relocation.  */
  . = 0x100000;
  .text1 : { *(.text1) }
  . = 0x900000;
  .text2 : { *(.text2) }
}
