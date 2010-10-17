/* First source file in relaxation test.  */

extern int bar ();
static int foo2 ();

int foo (int i)
{
  switch (i)
    {
    case 0: bar (0); break;
    case 1: bar (1); break;
    case 2: bar (2); break;
    case 3: bar (3); break;
    case 4: bar (foo2); break;
    case 5: bar (bar); break;
    }
  while (1)
    if (i)
      return bar ();
}

static int foo2 () { }
