//
// testFix24.cc : test Fix24/48 classes
//

#include <Fix24.h>

// This a set of inlines instead of a macro to force the side effects
// of evaluating y to happen before x is printed.

inline void check(char *x, int y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, unsigned int y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, long y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, unsigned long y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, double y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, Fix24 y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, Fix48 y) { cout << x << " = " << (y) << "\n"; }

void test24() {
  cout << "Fix24: identities should be displayed\n";

  Fix24 a;		check("0",a);
  Fix24 b = .5;		check(".5",b);
  Fix24 c = -.5;	check("-.5",c);
  Fix24 d = .1;		check(".1",d);
  Fix24 e = b;		check(".5",e);

  check(".5",a = b);
  check(".25",a = .25);
  check("536870912",mantissa(a));
  mantissa(a)=536870912;
  check(".25",a);
  check(".25",value(a));

  check(".25",+a);
  check("-.25",-a);

  check(".1 + .5",d+b);
  check(".1 - .5",d-b);
  check(".1 * .5",d*b);
  check(".1 *  3",d*3);
  check(".1 * -3",d*-3);
  check(".1 / .5",d/b);
  check(".1 << 1",d<<1);
  check("-.5 >> 2",c>>2);

  check(".1 == .5",d == b);
  check(".1 != .5",d != b);
  check(".1 > .5",d > b);
  check(".5 <= -.5",b <= c);

  cout << "Fix24: range errors ignored and overflows saturated\n";
  set_Fix24_overflow_handler(Fix24_overflow_saturate);
  set_Fix24_range_error_handler(Fix24_ignore);

  Fix24 f = 1.1;	check("1.1",f);

  Fix24 g = .7;
  check(".7 + .5",g+b);
  check("-.5 - .7",c-g);
  check(".5 / .1",b/d);
}

void test48() {
  cout << "Fix48: identities should be displayed\n";

  Fix48 a;		check("0",a);
  Fix48 b = .5;		check(".5",b);
  Fix48 c = -.5;	check("-.5",c);
  Fix48 d = .1;		check(".1",d);
  Fix48 e = b;		check(".5",e);

  check(".5",a = b);
  check(".25",a = .25);
  twolongs t;
  t = mantissa(a);
  check("536870912",t.u);
  check("0",t.l);
  mantissa(a)=t;
  check(".25",a);
  check(".25",value(a));

  check(".25",+a);
  check("-.25",-a);

  check(".1 + .5",d+b);
  check(".1 - .5",d-b);
  check(".1 *  3",d*3);
  check(".1 * -3",d*-3);
  check(".1 << 1",d<<1);
  check("-.5 >> 2",c>>2);

  check(".1 == .5",d == b);
  check(".1 != .5",d != b);
  check(".1 > .5",d > b);
  check(".5 <= -.5",b <= c);

  cout << "Fix48: range errors reported and overflows reported\n";
  set_Fix48_overflow_handler(Fix48_warning);
  set_Fix48_range_error_handler(Fix48_warning);

  Fix48 f = 1.1;	check("1.1",f);

  Fix48 g = .7;
  check(".7 + .5",g+b);
  check("-.5 - .7",c-g);
}

int main() {
  test24();
  test48();
  return 0;
}

