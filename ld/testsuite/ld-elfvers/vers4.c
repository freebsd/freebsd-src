/*
 * Testcase to make sure that a versioned symbol definition in an
 * application correctly defines the version node, if and only if
 * the actual symbol is exported.  This is built both with and without
 * -export-dynamic.
 */
#include <stdio.h>

extern int foo ();

int
bar()
{
	return 3;
}

int
new_foo()
{
	return 1000+bar();

}

__asm__(".symver new_foo,foo@@VERS_2.0");

int
main()
{
  printf("%d\n", foo());
  return 0;
}
