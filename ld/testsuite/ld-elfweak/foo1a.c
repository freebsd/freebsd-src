int deallocate_foo = 0;

int *
foo ()
{
  return &deallocate_foo;
}
