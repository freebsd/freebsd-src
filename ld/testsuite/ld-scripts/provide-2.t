SECTIONS 
{
  PROVIDE (foo = 1);
  PROVIDE (bar = 2);
  PROVIDE (baz = 3);
  .data :
  {
    *(.data)
  }
}
