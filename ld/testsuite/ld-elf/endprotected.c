#include <stdio.h>

__attribute__ ((visibility ("protected")))
void
foo ()
{
  printf ("TEST1\n");
}
