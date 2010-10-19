SECTIONS
{
  lo16_carry = 0x56789abc;
  lo16_nocarry = 0x12345678;
  split_lo16_carry = 0x789abcde;
  split_lo16_nocarry = 0x00123456;
  odd = 0x23456789;
  . = 0x10000;
  .text : { *(.text); }
  /DISCARD/ : { *(*); }
}
