/* Copyright 2004, 2008 Bob Proulx <bob@proulx.com>
Distributed under the two-clause BSD licence;
see the COPYING file for details. */

#include <stdio.h>
#include <stdlib.h>

#if 0
/* This code is commented out. "#if 0 then" */
#else
/* This code is passed through. "#if 0 else" */
#endif

#if 1
/* This code is passed through. "#if 1 then" */
#else
/* This code is commented out. "#if 1 else" */
#endif

#if FOO
int foo() { return 0; }
#else
#error FOO not defined
#endif

#if BAR
int foo() { return 0; }
#elif FOO
int bar() { return 0; }
#else
#error FOO not defined
#endif

int main()
{
  foo();
  bar();
}
