#pragma weak deallocate_foo
int deallocate_foo = 1;

extern void foobar ();

int
main ()
{
  foobar ();
  return 0;
}
