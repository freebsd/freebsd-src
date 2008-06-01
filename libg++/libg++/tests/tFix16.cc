//
// testFix16.cc : test Fix16/32 classes
//

#include <Fix16.h>

// This set of inlines (instead of a macro) is to force the side effects
// of evaluating y to happen before x is printed.

inline void check(char *x, int y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, long y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, double y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, Fix16 y) { cout << x << " = " << (y) << "\n"; }
inline void check(char *x, Fix32 y) { cout << x << " = " << (y) << "\n"; }

void test16() {
  cout << "Fix16: identities should be displayed\n";

  Fix16 a;		check("0",a);
  Fix16 b = .5;		check(".5",b);
  Fix16 c = -.5;	check("-.5",c);
  Fix16 d = .1;		check(".1",d);
  Fix16 e = b;		check(".5",e);

  check(".5",a = b);
  check(".25",a = .25);
  check("8192",mantissa(a));
  mantissa(a)=8192;
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

  cout << "Fix16: range errors ignored and overflows saturated\n";
  set_Fix16_overflow_handler(Fix16_overflow_saturate);
  set_Fix16_range_error_handler(Fix16_ignore);

  Fix16 f = 1.1;	check("1.1",f);

  Fix16 g = .7;
  check(".7 + .5",g+b);
  check("-.5 - .7",c-g);
  check(".5 / .1",b/d);
}

void test32() {
  cout << "Fix32: identities should be displayed\n";

  Fix32 a;		check("0",a);
  Fix32 b = .5;		check(".5",b);
  Fix32 c = -.5;	check("-.5",c);
  Fix32 d = .1;		check(".1",d);
  Fix32 e = b;		check(".5",e);

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

  cout << "Fix32: range errors reported and overflows reported\n";
  set_Fix32_overflow_handler(Fix32_warning);
  set_Fix32_range_error_handler(Fix32_warning);

  Fix32 f = 1.1;	check("1.1",f);

  Fix32 g = .7;
  check(".7 + .5",g+b);
  check("-.5 - .7",c-g);
  check(".5 / .1",b/d);
}

int main() {
  test16();
  test32();
  return 0;
}
