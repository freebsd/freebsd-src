#include <stdio.h>

extern int foo;
extern void bar (void);

void
xxx (void)
{
  printf ("MAIN\n");
}

int
main (void)
{
  foo = 1;
  bar ();
  if (foo == -1)
    printf ("OK1\n");
  else if (foo == 1)
    printf ("OK2\n");
  return 0;
}
