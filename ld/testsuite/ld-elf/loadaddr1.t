SECTIONS
{
  . = -0x7ff00000;
  .text : {*(.text .text.*)}
  . = ALIGN(64);
  .foo : { *(.foo) }
  .bar -0xa00000 : AT ((LOADADDR(.foo) + SIZEOF(.foo) + 4095) & ~(4095))
    { *(.bar) }
  . = LOADADDR(.bar) + 4096;
  . = ALIGN(8192);
  .data : AT (ADDR(.data)) { *(.data) }
  /DISCARD/ : { *(.*) }
}
