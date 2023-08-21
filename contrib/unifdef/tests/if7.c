/*
Distributed under the two-clause BSD licence;
see the COPYING file for details. */

#include <stdio.h>
#include <stdlib.h>

#if (FOOB | FOO) == 43
int foo1() { return 0; }
#else
#error FOOB bitwise-or FOO is not 43
#endif

#if (FOO ^ FOO) == 0
int foo2() { return 0; }
#else
#error FOO bitwise-xor FOO is not 0
#endif

#if (FOOB & 2) == 2
int foo3() { return 0; }
#else
#error FOOB bitwise-and 2 is not 2
#endif

#if (FOO << 1) == 2
int foo4() { return 0; }
#else
#error FOO left-shift 2 is not 2
#endif

#if (FOOB >> 4) == 2
int foo5() { return 0; }
#else
#error FOOB right-shift 2 is not 2
#endif

#if (FOOB + FOO) == 43
int foo6() { return 0; }
#else
#error FOOB add FOO is not 43
#endif

#if (FOOB - FOO) == 41
int foo7() { return 0; }
#else
#error FOOB subtract FOO is not 41
#endif

#if (FOOB * 2) == 84
int foo8() { return 0; }
#else
#error FOOB multiply 2 is not 84
#endif

#if (FOOB / 2) == 21
int foo9() { return 0; }
#else
#error FOOB divided 2 is not 21
#endif

#if (FOOB % FOO) == 0
int foo10() { return 0; }
#else
#error FOOB modulo FOO is not 0
#endif

#if ~(FOOB) == -43
int foo11() { return 0; }
#else
#error bitwise-not FOOB is not -43
#endif

#if -(FOOB) == -42
int foo12() { return 0; }
#else
#error negate FOOB is not -42
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
  foo11();
  foo12();
}
