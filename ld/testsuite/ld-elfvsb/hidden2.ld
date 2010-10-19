SECTIONS
{
  . = 0x1000;
  PROVIDE (foo = .);
  .data : { *(.data) }
  .dynamic : { *(.dynamic) }
}
