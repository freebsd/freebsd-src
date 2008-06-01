//
// testFix.cc : test Fix (variable length) classes
//

#include <Fix.h>

void check(const char* x, Fix y) { cout << x << " = " << (y) << "\n"; }

void check(const char* x, int y) { cout << x << " = " << (y) << "\n"; }

void check(const char* x, long y) { cout << x << " = " << (y) << "\n"; }

void check(const char* x, double y) { cout << x << " = " << (y) << "\n"; }

void checkb(const char* x, const Fix y)
{
  cout << x << " = " << (y) << " [" << length(y) << "]"<< "\n";
}



int main() {
  cout << "Fix: identities should be displayed\n"
    << "[X] displays the precision of a given value\n"
    << "[*] indicates that the full precision is not used for coding reasons\n";

  Fix a;		checkb("0 [16]",a);
  Fix b = .5;		checkb(".5 [16]",b);
  Fix c(17,-.5);	checkb("-.5 [17]",c);
  Fix d(33,.1);		checkb(".1 [33]",d);
  Fix e = c;		checkb("-.5 [17]",e);

  checkb(".3 [16]",a = .3);
  checkb(".5 [16]",a = b);
  checkb(".1 [16]",a = d);
  checkb(".1 [33*]",d = a);
  checkb("-.2 [17]",c = -.2);
  checkb("-.5 [17]",e);

  check(".1 [16] == .1 [33*]",a == d);
  d = .1;
  check(".1 [16] == .1 [33]",a == d);
  check(".1 [33] != .5 [16]",d != b);
  check(".1 [33] > .5 [16]",d > b);
  check(".1 [33] <= -.2 [17]",d <= c);

  e = .5;
  check("1073741824",mantissa(e).as_double());
  check(".5",value(e));

  checkb(".5 [17]",+e);
  checkb("-.5 [17]",-e);

  checkb(".1 [33] + .5 [16]",d+b);
  checkb(".1 [33] - .5 [16]",d-b);
  checkb(".1 [33] * .5 [16]",d*b);
  checkb(".1 [33] *  3",d*3);
  checkb(".1 [33] * -3",d*-3);
  checkb("-.1 [33] *  3",(-d)*3);
  checkb("-.1 [33] * -3",(-d)*-3);
  checkb(".5 [17] * -2",e*-2);
  checkb(".1 [33] % 25",d%25);
  checkb(".1 [33] % -25",d%-25);
  checkb(".1 [33] / .5 [16]",d/b);
  checkb(".1 [33] << 1",d<<1);
  checkb("-.1 [33] >> 2",(-d)>>2);

  checkb("abs(-.2)",abs(c));
  checkb("abs(.2)",abs(-c));
  check("sgn(-.2)",sgn(c));
  check("sgn(.2)",sgn(-c));
  
  cout << "\nshow .1 [33]\n";
  show(d);

  Fix g = .95;

  cout << "\nFix: range errors warned\n";

  Fix f = 1.1;	checkb("1.1 [16]",f);

  checkb(".5 [16] / .1 [33]",b/d);
  checkb(".5 [16] / 0. [16]",b/Fix(0.));
  checkb(".5 [17] * 32768",e*32768);

  cout << "\nFix: overflows saturated\n";
  Fix::set_overflow_handler(Fix::overflow_saturate);

  checkb(".95 [16] + .1 [33]",g+d);
  checkb("-.1 [33] - .95 [16]",-d-g);
  checkb(".5 [17] * 2",e*2);

  cout << "\nFix: overflows generate warnings\n";
  Fix::set_overflow_handler(Fix::overflow_warning);

  checkb(".95 [16] + .1 [33]",g+d);
  checkb("-.1 [33] - .95 [16]",-d-g);
  checkb(".5 [17] * 2",e*2);

  return 0;
}
