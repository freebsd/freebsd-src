#include <stdio.h>
#include "dl3header.h"

extern void f (void);

int
main (void)
{
  try
    {
      f();
    }
  catch (A a)
    {
      if (a.i == 42)
	printf ("OK\n");
      else
	printf ("BAD1\n");
    }
  catch (...)
    {
      printf ("BAD2\n");
    }
  return 0;
}
