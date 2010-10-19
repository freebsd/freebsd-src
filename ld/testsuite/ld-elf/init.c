#include <stdio.h>

static void
init_0 (void)
{
  printf ("init array 0\n");
}

static void
init_1 (void)
{
  printf ("init array 1\n");
}

static void
init_2 (void)
{
  printf ("init array 2\n");
}

void (*const init_array []) (void)
     __attribute__ ((section (".init_array"),
		     aligned (sizeof (void *)))) =
{
  &init_0,
  &init_1,
  &init_2
};

int
main (void)
{
  return 0;
}
