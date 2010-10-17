OUTPUT_ARCH(mmix)
ENTRY(Main)
SECTIONS
{
  .text 0x100 :
  { *(.text.x); *(.text.x2); }

  .text.away 0x4000000000001060 :
  { *(.text); Main = _start; }

  .MMIX.reg_contents :
  { *(.MMIX.reg_contents.linker_allocated); *(.MMIX.reg_contents); }
}
