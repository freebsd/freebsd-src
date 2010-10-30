SECTIONS
{
  TEST (NOLOAD) :
  {
    *(TEST)
  }
  /DISCARD/ : { *(.*) }
}
