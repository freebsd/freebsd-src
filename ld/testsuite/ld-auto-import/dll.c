int var = 123;
int foo = 121;

int var2[2]= { 123, 456 };

#include <stdio.h>

void
print_var (void)
{
  printf ("DLL sees var = %d\n", var);
}

void
print_foo (void)
{
  printf ("DLL sees foo = %d\n", foo);
}

void (* func_ptr)(void) = print_foo;
