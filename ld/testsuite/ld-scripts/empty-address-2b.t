SECTIONS
{
  .text 0x0000000: { *(.text) }
  .data :
  {
    PROVIDE (__data_start = .);
    *(.data)
  }
  __data_end = .;
  /DISCARD/ : { *(.*) }
}
