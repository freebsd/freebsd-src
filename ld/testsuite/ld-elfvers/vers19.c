#include <stdio.h>

extern int show_foo ();

int
main ()
{
  printf ("%d\n", show_foo ());
  return 0;
}
