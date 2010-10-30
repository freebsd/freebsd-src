#include <stdio.h>

void
foo (void)
{
  printf ("TEST2\n");
}

static void (*const init_array []) (void)
  __attribute__ ((used, section (".init_array"), aligned (sizeof (void *))))
  = { foo };
