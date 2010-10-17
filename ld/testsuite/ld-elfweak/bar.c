#include <stdio.h>

extern void foo ();
extern void foobar ();

void
foo ()
{
  printf ("strong foo\n");
}

void
foobar ()
{
  foo ();
}
