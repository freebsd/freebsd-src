#include <stdio.h>

extern int foo1;
extern int foo2;
extern void bar (void);

void
xxx1 (void)
{
  printf ("MAIN1\n");
}

void
xxx2 (void)
{
  printf ("MAIN2\n");
}

int
main (void)
{
  foo1 = 1;
  foo2 = 1;
  bar ();
  if (foo1 == -1)
    printf ("OK1\n");
  else if (foo1 == 1)
    printf ("OK2\n");
  if (foo2 == -1)
    printf ("OK3\n");
  else if (foo2 == 1)
    printf ("OK4\n");
  return 0;
}
