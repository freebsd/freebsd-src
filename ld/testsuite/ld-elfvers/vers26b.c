#pragma weak foo 

void foo ();

void
ref ()
{
  if (foo)
    foo ();
}
