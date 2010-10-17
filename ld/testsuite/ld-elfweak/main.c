#include <stdio.h>

#pragma weak foo

extern void foo ();
extern void foobar ();

void
foo ()
{
  printf ("weak foo\n");
}

int
main ()
{
  foobar ();
  return 0;
}
