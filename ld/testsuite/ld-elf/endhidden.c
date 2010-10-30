#include <stdio.h>

__attribute__ ((visibility ("hidden")))
void
foo ()
{
  printf ("TEST1\n");
}
