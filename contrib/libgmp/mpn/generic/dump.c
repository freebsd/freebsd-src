#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"

void
mpn_dump (ptr, size)
     mp_srcptr ptr;
     mp_size_t size;
{
  if (size == 0)
    printf ("0\n");
  {
    while (size)
      {
	size--;
	printf ("%0*lX", (int) (2 * BYTES_PER_MP_LIMB), ptr[size]);
      }
    printf ("\n");
  }
}
