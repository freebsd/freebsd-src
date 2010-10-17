OUTPUT_ARCH(mmix)
ENTRY(Main)
SECTIONS
{
   .text 0x100 :
   { *(.text); Main = _start; }

   .other 0x1000000000000000 :
   { *(secname); *(anothersec); *(thirdsec); *(.a.fourth.section); }

   .data 0x2000000000000000 :
   { *(.data); }

  .MMIX.reg_contents :
  { *(.MMIX.reg_contents); }
}
