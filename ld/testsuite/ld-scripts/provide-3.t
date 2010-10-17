SECTIONS 
{
  .data :
  {
    LONG (foo)
    LONG (bar)
    *(.data)
  }
  foo = .;
  bar = .;
}
