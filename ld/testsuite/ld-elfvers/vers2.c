/*
 * Test function.  This is built into a shared library, and references a
 * versioned symbol foo that is in test.so.
 */
#include <stdio.h>

extern int show_foo ();

void
show_xyzzy()
{
  printf("%d", show_foo());
}
