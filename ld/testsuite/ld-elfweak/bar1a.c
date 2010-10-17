#include <stdio.h>

extern int deallocate_foo;

extern int * bar ();
extern int * foo ();
extern void abort ();
extern void foobar ();

void
foobar ()
{
  if (&deallocate_foo != bar () || &deallocate_foo != foo ())
    abort ();

  if (deallocate_foo)
    printf ("weak deallocate_foo\n");
  else
    printf ("strong deallocate_foo\n");
}

int *
bar()
{
  return &deallocate_foo;
}
