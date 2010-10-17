/* Second source file in relaxation test.  */

int bar2 ()
{
  int i;

  for (i = 0; i < 100; i++)
    foo ();
  return foo () + foo () + foo () + foo ();
}

int bar (int i)
{
  while (1)
    if (i)
      return foo ();
    else
      return foo ();
}
