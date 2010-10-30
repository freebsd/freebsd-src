#include <stdio.h>

extern void par (void);

void __real_par (void)
{
  printf ("__real_par \n");
  par ();
}

void
__wrap_par (void)
{
  printf ("__wrap_par \n");
  __real_par ();
}
