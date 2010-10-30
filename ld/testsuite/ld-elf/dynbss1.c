#include <stdio.h>
#include <stdlib.h>
#include "data1.h"

int
main (void)
{
  if ((((long) (&a1)) & (ALIGNMENT1 - 1)))
    abort ();
  if ((((long) (&a2)) & (ALIGNMENT2 - 1)))
    abort ();
  if ((((long) (&a2)) & (ALIGNMENT3 - 1)))
    abort ();
  if ((((long) (&a3)) & (ALIGNMENT4 - 1)))
    abort ();

  printf ("PASS\n");

  return(0) ;
}
