SECTIONS
{
  .text 0x00000000: { *(.text) }
  .data ALIGN(0x1000) + (. & (0x1000 - 1)):
  {
    PROVIDE (__data_start = .);
    *(.data)
  }
  __data_end = .;
  /DISCARD/ : { *(.*) }
}
