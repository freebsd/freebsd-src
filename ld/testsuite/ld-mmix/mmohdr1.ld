OUTPUT_ARCH(mmix)
OUTPUT_FORMAT("mmo")
ENTRY(Main)
SECTIONS
{
  .text 0x100 + sizeof_headers :
  { *(.text); Main = _start; }

  .MMIX.reg_contents :
  { *(.MMIX.reg_contents); }
}
