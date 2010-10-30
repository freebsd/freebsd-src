#include <stdio.h>

int bar = 10;

void
foo (void)
{
  if (bar == 10)
    printf ("bar is in DSO.\n");
  else if (bar == -20)
    printf ("bar is in main.\n");
  else
    printf ("FAIL\n");
}
