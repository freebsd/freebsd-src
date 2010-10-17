__asm__(".symver _old_foo,foo@VERS.0");
__asm__(".symver _old_bar,bar@VERS.0");
__asm__(".symver _old_foobar,foobar@VERS.0");
__asm__(".weak  _old_bar");

int
bar () 
{
  return 1;
}

int
_old_bar () 
{
  return bar ();
}

int
foo () 
{
  return 2;
}

int
_old_foo () 
{
  return foo ();
}

int _old_foobar = -1;

int foobar = 1;
