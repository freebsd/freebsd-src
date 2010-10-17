SECTIONS
{
  . = 0x1000 + SIZEOF_HEADERS;
  .text ALIGN (0x20) : { *(.text) }
  .data 0x2000 : {
    *(.data)
    LONG (SIZEOF (.text))
    LONG (SIZEOF (.data))
  }
}
