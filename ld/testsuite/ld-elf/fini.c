#include <stdio.h>

static void
fini_0 (void)
{
  printf ("fini array 0\n");
}

static void
fini_1 (void)
{
  printf ("fini array 1\n");
}

static void
fini_2 (void)
{
  printf ("fini array 2\n");
}

void (*const fini_array []) (void)
     __attribute__ ((section (".fini_array"),
		     aligned (sizeof (void *)))) =
{
  &fini_0,
  &fini_1,
  &fini_2
};

int
main (void)
{
  return 0;
}
