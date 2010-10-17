int deallocate_foo;

int *
bar()
{
  return &deallocate_foo;
}
