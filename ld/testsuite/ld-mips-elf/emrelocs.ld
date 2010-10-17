ENTRY(ext_fun)
SECTIONS
{
  .text 0x00100000: 
  {
    *(.text)
  }
  .data 0x00200000: 
  {
    *(.sdata)
  }
  .rel.sdata 0x00300000: 
  {
    *(.rel.sdata)
  }
}
