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
/* This code is passed through. "#if 1 else" */
#endif

#if defined(FOO) || defined(FOOB)
int foo1() { return 0; }
#else
#error FOO or FOOB not defined
#endif

#if defined(FOOB) || defined(FOO)
int foo2() { return 0; }
#else
#error FOO or FOOB not defined
#endif

#if defined(FOO) && defined(FOOB)
int foo3() { return 0; }
#else
#error FOO and FOOB not defined
#endif

#if defined(FOOB) && defined(FOO)
int foo4() { return 0; }
#else
#error FOO and FOOB not defined
#endif

int main()
{
  foo1();
  foo2();
  foo3();
  foo4();
}
