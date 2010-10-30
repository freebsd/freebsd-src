SECTIONS
{
  .bar : AT ((ADDR(.foo) + 4095) & ~(4095)) { *(.bar) }
  .foo : { *(.foo) }
  /DISCARD/ : { *(.*) }
}
