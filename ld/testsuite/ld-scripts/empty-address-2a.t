SECTIONS
{
  .text : { *(.text) }
  .data : { *(.data) }
  __data_end = .;
  /DISCARD/ : { *(.*) }
}
