/*
 a test file for Rational class
 */

#include <Rational.h>

#include <assert.h>

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }


void identtest(Rational& a, Rational& b, Rational& c)
{
  Rational one(1, 1);

  assert(-(-a) == a);
  assert((a + b) ==  (b + a));
  assert((a + (-b)) ==  (a - b));
  assert((a * b) ==  (b * a));
  assert((a * (-b)) == -(a * b));
  assert((a / (-b)) == -(a / b));
  assert((a / b) == (a * (one / b)));
  assert((a / b) == (one / (b / a)));
  assert((a - b) ==  -(b - a));
  assert((a + (b + c)) == ((a + b) + c));
  assert((a * (b * c)) == ((a * b) * c));
  assert((a * (b + c)) == ((a * b) + (a * c)));
  assert(((a - b) + b) == a);
  assert(((a + b) - b) == a);
  assert(((a * b) / b) == a);
  assert(((a / b) * b) == a);

  Rational x = a;
  x *= b;
  assert(x == (a * b));
  x += c;
  assert(x == ((a * b) + c));
  x -= a;
  assert(x == (((a * b) + c) - a));
  x /= b;
  assert(x == ((((a * b) + c) - a) / b));

  assert(x.OK());
}



void simpletest()
{
  Rational one = 1;
  assert(one.OK());
  Rational third(1, 3);
  assert(third.OK());
  Rational half(1, 2);
  assert(half.OK());

  Rational two(2);
  Rational zero(0);
  Rational r;
  r = two+zero;

  cout << "one = " << one << "\n";
  cout << "two = " << r << "\n";
  cout << "third = " << third << "\n";
  cout << "half = " << half << "\n";

  cout << "third + half = " << third + half << "\n";
  cout << "third - half = " << third - half << "\n";
  cout << "third * half = " << third * half << "\n";
  cout << "third / half = " << third / half << "\n";

  Rational onePointTwo = 1.2;
  cout << "onePointTwo = " << onePointTwo << "\n";
  cout << "double(onePointTwo) = " << double(onePointTwo) << "\n";

  Rational a = one;
  cout << "a = " << a << "\n";
  assert(a.OK());
  a += half;
  cout << "a += half = " << a << "\n";
  assert(a == Rational(3, 2));
  a -= half;
  cout << "a -= half = " << a << "\n";
  assert(a == Rational(1));
  a *= half;
  cout << "a *= half = " << a << "\n";
  assert(a == half);
  a /= half;
  cout << "a /= half = " << a << "\n";
  assert(a == Rational(1));
  assert(a.OK());

  identtest(one, one, one);
  identtest(one, third, half);
  identtest(third, half, one);
  identtest(onePointTwo, half, a);
}

void pitest()
{
  Rational half(1, 2);
  Rational approxpi(355, 113);
  assert(approxpi.OK());
  cout << "approxpi = " << approxpi << "\n";
  cout << "double(approxpi) = " << double(approxpi) << "\n";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
  Rational rpi = Rational(M_PI);
  cout << "rpi = Rational(PI) = " << rpi << "\n";
  assert(rpi.OK());
  cout << "double(rpi) = " << double(rpi) << "\n";

  cout << "approxpi + rpi = " << approxpi + rpi << "\n";
  cout << "approxpi - rpi = " << approxpi - rpi << "\n";
  cout << "approxpi * rpi = " << approxpi * rpi << "\n";
  cout << "approxpi / rpi = " << approxpi / rpi << "\n";

  Rational negapproxpi = -approxpi;

  cout << "-approxpi = " << negapproxpi << "\n";
  assert(sign(negapproxpi) < 0);
  cout << "abs(negapproxpi) = " << abs(negapproxpi) << "\n";
  assert(abs(negapproxpi) == approxpi);

  assert(approxpi != rpi);
  assert(approxpi >= rpi);
  assert(approxpi > rpi);
  assert(!(approxpi == rpi));
  assert(!(approxpi <= rpi));
  assert(!(approxpi < rpi));
#if defined (__GNUC__) && ! defined (__STRICT_ANSI__)
  assert((approxpi >? rpi) == approxpi);
  assert((approxpi <? rpi) == rpi);
#endif

  assert(floor(approxpi) == 3);
  assert(ceil(approxpi) == 4);
  assert(trunc(approxpi) == 3);
  assert(round(approxpi) == 3);

  assert(floor(negapproxpi + half) == -3);
  assert(ceil(negapproxpi + half) == -2);
  assert(trunc(negapproxpi + half) == -2);
  assert(round(negapproxpi + half) == -3);

  identtest(approxpi, rpi, negapproxpi);
  identtest(rpi, approxpi, rpi);
  identtest(negapproxpi, half, rpi);
}



void IOtest()
{
  Rational a;
  cout << "\nenter a Rational in form a/b or a: ";
  cin >> a;
  cout << "number = " << a << "\n";
  assert(a.OK());
}

// as a fct just to test Rational fcts
Rational estimate_e(long n)
{
  Rational x = Rational(n + 1, n);
  Rational e = pow(x, n);
  return e;
}

void etest(long n)
{
  cout << "approximating e as pow(1+1/n),n) for n =" << n << "\n";
  Rational approxe = estimate_e(n);
  assert(approxe.OK());
  cout << "double(approxe) = " << double(approxe) << "\n";
  cout << "log(approxe) = " << log(approxe) << "\n";
  assert(log(approxe) <= 1.0);
  cout << "approxe = " << approxe << "\n";
}

int main()
{
  simpletest();
  pitest();
  IOtest();
  etest(10);
  etest(100);
  etest(1000);
  cout << "\nEnd of test\n";
  return 0;
}
