ENTRY(main)

SECTIONS
{
  . = 0x100 + SIZEOF_HEADERS;
  .text : { *(.text) }
  .data : { *(.data) }
}
