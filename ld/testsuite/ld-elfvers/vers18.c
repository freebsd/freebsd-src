int
bar ()
{
  return 3;
}

#pragma weak hide_original_foo

int
hide_original_foo ()
{
  return 1 + bar ();
}

#pragma weak hide_old_foo

int
hide_old_foo ()
{
  return 10 + bar();
}

#pragma weak hide_old_foo1

int
hide_old_foo1 ()
{
  return 100 + bar ();
}

#pragma weak hide_new_foo

int
hide_new_foo ()
{
  return 1000 + bar ();
}

__asm__(".symver hide_original_foo,show_foo@");
__asm__(".symver hide_old_foo,show_foo@VERS_1.1");
__asm__(".symver hide_old_foo1,show_foo@VERS_1.2");
__asm__(".symver hide_new_foo,show_foo@@VERS_2.0");
