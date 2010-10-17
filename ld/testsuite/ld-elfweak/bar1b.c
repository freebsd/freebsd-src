int deallocate_foo = 0;

int *
bar()
{
  return &deallocate_foo;
}
