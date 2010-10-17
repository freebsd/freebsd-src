int foo;
asm (".hidden foo");

int
_start (void)
{
  return foo;
}

int
__start (void)
{
  return _start ();
}
