#include <stdio.h>

int foo1;
int foo2;

extern void xxx1 (void);
extern void xxx2 (void);

void
bar (int x)
{
  if (foo1 == 1)
    printf ("bar OK1\n");
  else if (foo1 == 0)
    printf ("bar OK2\n");
  if (foo2 == 1)
    printf ("bar OK3\n");
  else if (foo2 == 0)
    printf ("bar OK4\n");
  foo1 = -1;
  foo2 = -1;
  xxx1 ();
  xxx2 ();
}
