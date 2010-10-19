#include <stdio.h>

void
bar ()
{
#ifdef SIZE_BIG
  printf ("1\n");
  printf ("2\n");
  printf ("3\n");
#endif
}
