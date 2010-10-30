SECTIONS
{
  . = SIZEOF_HEADERS;
  . = ALIGN(__section_alignment__);
  .text  __image_base__ + ( __section_alignment__ < 0x1000 ? . : __section_alignment__ ) :
  {
    *(.text)
  }
  . = . + 0x1000;
  .data BLOCK(__section_alignment__) :
  {
    *(.data)
  }
  /DISCARD/ : { *(.*) }
}
