SECTIONS
{
  .text : {*(.text)}
  .data ALIGN(0x40) : AT (ALIGN (LOADADDR (.text) + SIZEOF (.text), 0x80))
    {}
  ASSERT (LOADADDR(.data) == 0x80, "dyadic ALIGN broken")
  ASSERT (ADDR(.data) == 0x40, "monadic ALIGN broken")
}
