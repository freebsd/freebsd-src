#include <stdio.h>

#pragma weak undef_func

extern int undef_func (void);
int (*ptr_to_func)(void) = undef_func;

int
main (void)
{
  if (ptr_to_func == NULL)
    printf ("PASSED\n");

  return 0;
}
