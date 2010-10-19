#include <stdio.h>

static void
preinit_0 (void)
{
  printf ("preinit array 0\n");
}

static void
preinit_1 (void)
{
  printf ("preinit array 1\n");
}

static void
preinit_2 (void)
{
  printf ("preinit array 2\n");
}

void (*const preinit_array []) (void)
     __attribute__ ((section (".preinit_array"),
		     aligned (sizeof (void *)))) =
{
  &preinit_0,
  &preinit_1,
  &preinit_2
};

int
main (void)
{
  return 0;
}
