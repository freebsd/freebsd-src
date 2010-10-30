#include <stdio.h>

extern int bar;

void
foo (void)
{
  if (bar == -20)
    printf ("OK\n");
}
