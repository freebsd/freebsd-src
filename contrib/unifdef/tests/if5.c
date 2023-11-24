/* Copyright 2004 Bob Proulx <bob@proulx.com>
Distributed under the two-clause BSD licence;
see the COPYING file for details. */

#include <stdio.h>
#include <stdlib.h>

#if FOOB == 42
int foo1() { return 0; }
#else
#error FOOB not 42
#endif

#if FOOB != 42
#error FOO is 2
#else
int foo2() { return 0; }
#endif

#if FOOB == 42 || FOO == 1
int foo3() { return 0; }
#else
#error FOO not 1 or BAR not 1
#endif

#if FOOB != 42 && FOO != 1
#error FOOB not 42 and FOO not 1
#else
int foo4() { return 0; }
#endif

#if FOOB == 42 || FOO != 1
int foo5() { return 0; }
#else
#error FOOB is 42 or FOO is not 1
#endif

#if FOO != 1 || FOOB != 42
#error FOO is 1 or FOOB is 42
#else
int foo6() { return 0; }
#endif

#if FOO > FOOB
#error FOO is greater than FOOB
#else
int foo7() { return 0; }
#endif

#if FOOB < FOO
#error FOOB is less than FOO
#else
int foo8() { return 0; }
#endif

#if FOO >= FOOB
#error FOO is greater than or equal FOOB
#else
int foo9() { return 0; }
#endif

#if FOOB <= FOO
#error FOOB is less than or equal FOO
#else
int foo10() { return 0; }
#endif

int main()
{
  foo1();
  foo2();
  foo3();
  foo4();
  foo5();
  foo6();
  foo7();
  foo8();
  foo9();
  foo10();
}
