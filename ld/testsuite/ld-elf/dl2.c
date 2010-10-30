#include <stdio.h>

int foo;

extern void xxx (void);

void
bar (int x)
{
  if (foo == 1)
    printf ("OK1\n");
  else if (foo == 0)
    printf ("OK2\n");
  foo = -1;
  xxx ();
}
