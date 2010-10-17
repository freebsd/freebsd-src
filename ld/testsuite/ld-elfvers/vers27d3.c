extern void ref ();
extern void foo ();

void
_start ()
{
  foo ();
  ref ();
}

void
__start ()
{
  _start ();
}

void
start ()
{
  __start ();
}
