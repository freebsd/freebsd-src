#pragma weak bar

extern void bar ();

void
foo ()
{
  bar ();
}
