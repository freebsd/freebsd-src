OUTPUT_ARCH(mmix)
ENTRY(Main)
SECTIONS
{
   .text 0x1000000000000000 :
   { *(.text); Main = _start; }

   .data 0x2000000000000000 :
   { *(.data); }

  .MMIX.reg_contents :
  { *(.MMIX.reg_contents); }
}
