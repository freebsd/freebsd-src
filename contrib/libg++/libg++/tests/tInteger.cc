/*
 a test file for Integer class
 */

#include <Integer.h>
#include <assert.h>

#define tassert(ex) { cerr << #ex; \
                       if ((ex)) cerr << " OK\n"; \
                       else cerr << " Fail\n"; }


Integer factorial(Integer n)
{
  Integer f;
  if (n < 0)
    f = 0;
  else
  {
    f = 1;
    while (n > 0)
    {
      f *= n;
      --n;
    }
  }
  return f;
}

Integer fibonacci(long n)
{
  Integer f;
  if (n <= 0)
    f = 0;
  else
  {
    f = 1;
    Integer prev = 0;
    Integer tmp;
    while (n > 1)
    {
      tmp = f;
      f += prev;
      prev = tmp;
      --n;
    }
  }
  return f;
}


void identitytest(Integer& a, Integer& b, Integer& c)
{
  assert( -(-a) ==  a);
  assert( (a + b) ==  (b + a));
  assert( (a + (-b)) ==  (a - b));
  assert( (a * b) ==  (b * a));
  assert( (a * (-b)) ==  -(a * b));
  assert( (a / (-b)) ==  -(a / b));
  assert( (a - b) ==  -(b - a));
  assert( (a + (b + c)) ==  ((a + b) + c));
  assert( (a * (b * c)) ==  ((a * b) * c));
  assert( (a * (b + c)) ==  ((a * b) + (a * c)));
  assert( ((a - b) + b) ==  a);
  assert( ((a + b) - b) ==  a);
  assert( ((a * b) / b) ==  a);
  assert( ((a * b) % b) ==  0);
  assert( (b * (a / b) + (a % b)) ==  a);
  assert( ((a + b) % c) ==  ((a % c) + (b % c)) % c);
}

void utiltest(Integer& a)
{
  assert(sqrt(sqr(a)) == a);
  assert(sqr(sqrt(a)) <= a);

  Integer x = 1;
  for (int i = 0; i < 10; ++i)
  {
    assert(pow(a, i) == x);
    x *= a;
  }
  setbit(x, 0);
  assert(testbit(x, 0));
  assert(odd(x));
  assert(!even(x));
  clearbit(x, 0);
  clearbit(x, 1);
  assert(even(x));
  assert(!odd(x));
  assert(x % 4 == 0);

}

void bittest(Integer& a, Integer& b, Integer& c)
{
  assert( (a | b) ==  (b | a));
  assert( (a & b) ==  (b & a));
  assert( (a ^ b) ==  (b ^ a));
  assert( (a | (b | c)) ==  ((a | b) | c));
  assert( (a & (b & c)) ==  ((a & b) & c));
  assert( (a & (b | c)) ==  ((a & b) | (a & c)));
  assert( (a | (b & c)) ==  ((a | b) & (a | c)));
  assert( (a & (a | b)) ==  a);
  assert( (a | (a & b)) ==  a);
}

void accumtest(Integer& a, Integer& b, Integer& c)
{
  Integer x = a;
  x *= b;
  assert(x == (a * b));
  x += c;
  assert(x == ((a * b) + c));
  x -= a;
  assert(x == (((a * b) + c) - a));
  x /= b;
  assert(x == ((((a * b) + c) - a) / b));
  x %= c;
  assert(x == (((((a * b) + c) - a) / b) % c));
  x &= a;
  assert(x == ((((((a * b) + c) - a) / b) % c) & a));
  x |= b;
  assert(x == (((((((a * b) + c) - a) / b) % c) & a) | b));
  x ^= c;
  assert(x == ((((((((a * b) + c) - a) / b) % c) & a) | b) ^ c));

  assert(x.OK());
}

void longidentitytest(Integer& a, long b, long c)
{
  assert( (a + b) ==  (b + a));
  assert( (a + (-b)) ==  (a - b));
  assert( (a * b) ==  (b * a));
  assert( (a * (-b)) ==  -(a * b));
  assert( (a / (-b)) ==  -(a / b));
  assert( (a - b) ==  -(b - a));
  assert( (a + (b + c)) ==  ((a + b) + c));
  assert( (a * (b * c)) ==  ((a * b) * c));
  assert( (a * (b + c)) ==  ((a * b) + (a * c)));
  assert( ((a - b) + b) ==  a);
  assert( ((a + b) - b) ==  a);
  assert( ((a * b) / b) ==  a);
  assert( ((a * b) % b) ==  0);
  assert( (b * (a / b) + (a % b)) ==  a);
  assert( ((a + b) % c) ==  ((a % c) + (b % c)) % c);
}

void longbittest(Integer& a, long b, long c)
{
  assert( (a | b) ==  (b | a));
  assert( (a & b) ==  (b & a));
  assert( (a ^ b) ==  (b ^ a));
  assert( (a | (b | c)) ==  ((a | b) | c));
  assert( (a & (b & c)) ==  ((a & b) & c));
  assert( (a & (b | c)) ==  ((a & b) | (a & c)));
  assert( (a | (b & c)) ==  ((a | b) & (a | c)));
  assert( (a & (a | b)) ==  a);
  assert( (a | (a & b)) ==  a);
}

void longaccumtest(Integer& a, long b, long c)
{
  Integer x = a;
  x *= b;
  assert(x == (a * b));
  x += c;
  assert(x == ((a * b) + c));
  x -= a;
  assert(x == (((a * b) + c) - a));
  x /= b;
  assert(x == ((((a * b) + c) - a) / b));
  x %= c;
  assert(x == (((((a * b) + c) - a) / b) % c));
  x &= a;
  assert(x == ((((((a * b) + c) - a) / b) % c) & a));
  x |= b;
  assert(x == (((((((a * b) + c) - a) / b) % c) & a) | b));
  x ^= c;
  assert(x == ((((((((a * b) + c) - a) / b) % c) & a) | b) ^ c));

  assert(x.OK());
}

void anothertest()
{
  Integer pow64 = Ipow(2, 64);
  cout << "pow64 = Ipow(2, 64) = " << pow64 << "\n";
  assert(pow64.OK());
  cout << "lg(pow64) = " << lg(pow64) << "\n";
  assert(lg(pow64) == 64);
  int k;
  for (k = 0; k < 64; ++k) assert(testbit(pow64, k) == 0);
  assert(testbit(pow64, k) != 0);

  Integer s64 = 1;
  s64 <<= 64;
  cout << "s64 = 1 << 64 = " << s64 << "\n";
  assert(s64.OK());

  assert(s64 == pow64);
  assert(s64 >= pow64);
  assert(s64 <= pow64);
  assert(!(s64 != pow64));
  assert(!(s64 > pow64));
  assert(!(s64 < pow64));

  Integer s32 = s64 >> 32;
  cout << "s32 = s64 >> 32 = " << s32 << "\n";
  assert(s32.OK());
  assert(lg(s32) == 32);
  assert(!(pow64 == s32));
  assert(!(pow64 < s32));
  assert(!(pow64 <= s32));
  assert(pow64 != s32);
  assert(pow64 >= s32);
  assert(pow64 > s32);

  Integer comps64 = ~s64;
  cout << "comps64 = ~s64 = " << comps64 << "\n";
  for (k = 0; k < 64; ++k) assert(testbit(comps64, k) == !testbit(s64, k));
  Integer result = (comps64 & s32);
  cout << "comps64 & s32 = " << result << "\n";
  assert(result.OK());
  result = (comps64 | s32);
  cout << "comps64 | s32 = " << result << "\n";
  assert(result.OK());
  result = (comps64 ^ s32);
  cout << "comps64 ^ s32 = " << result << "\n";
  assert(result.OK());

  identitytest(s64, s32, comps64);
  bittest(s32, s64, comps64);
  accumtest(comps64, s32, pow64);
  utiltest(s32);
  longidentitytest(s64, 1000, 50);
  longbittest(s64, 12345, 67890);
  longaccumtest(s32, 100000, 1);

}

void iotest()
{
  Integer result;

  cout << "\nenter an Integer: ";
  cin.setf(0, ios::basefield);
  cin >> result;
  cout << "number = " << hex << result << dec << "\n";
  assert(result.OK());

  cout << "enter another Integer: ";
  cin >> result;
  cout << "number = " << result << "\n";
  assert(result.OK());

  cout << "enter another Integer: ";
  cin >> dec >> result;
  cout << "number = " << result << "\n";
  assert(result.OK());

}

void fibtest()
{
  Integer fib50 = fibonacci(50);
  cout << "fib50 = fibonacci(50) = " << fib50 << "\n";
  assert(fib50.OK());
  Integer fib48 = fibonacci(48);
  cout << "fib48 = fibonacci(48) = " << fib48 << "\n";
  assert(fib48.OK());

  Integer result = fib48 + fib50;
  cout << "fib48 + fib50 = " << result << "\n";
  result = fib48 - fib50;
  cout << "fib48 - fib50 = " << result << "\n";
  result = fib48 * fib50;
  cout << "fib48 * fib50 = " << result << "\n";
  result = fib48 / fib50;
  cout << "fib48 / fib50 = " << result << "\n";
  result = fib48 % fib50;
  cout << "fib48 % fib50 = " << result << "\n";
  result = gcd(fib50, fib48);
  cout << "gcd(fib50, fib48) = " << result << "\n";
  result = sqrt(fib50);
  cout << "sqrt(fib50) = " << result << "\n";

  identitytest(result, fib50, fib48);
  bittest(result, fib50, fib48);
  accumtest(result, fib50, fib48);
  utiltest(fib48);
  longidentitytest(fib50, 1000, 50);
  longaccumtest(fib48, 100000, 1);
}


void facttest(Integer& one, Integer& two)
{
  Integer fact30(factorial(30));
  cout << "fact30 = factorial(30) = " << fact30 << "\n";
  assert(fact30.OK());

  Integer fact28(factorial(28));
  cout << "fact28 = factorial(28) = " << fact28 << "\n";
  assert(fact28.OK());
  assert(fact30 == fact28 * 870);

  Integer result = fact30 + fact28;
  cout << "fact30 + fact28 = " <<  result << "\n";
  result = fact30 - fact28;
  cout << "fact30 - fact28 = " << result << "\n";
  result = fact30 * fact28;
  cout << "fact30 * fact28 = " << result << "\n";
  result = fact30 / fact28;
  cout << "fact30 / fact28 = " << result << "\n";
  result = fact30 % fact28;
  cout << "fact30 % fact28 = " << result << "\n";

  result = -fact30;
  cout << "-fact30 = " << result << "\n";
  assert(abs(result) == fact30);

  cout << "lg(fact30) = " << lg(fact30) << "\n";
  assert(lg(fact30) == 107);

  result = gcd(fact30, fact28);
  cout << "gcd(fact30, fact28) = " << result << "\n";
  assert(result == fact28);

  result = sqrt(fact30);
  cout << "sqrt(fact30) = " << result << "\n";

  Integer negfact31 = fact30 * -31;
  Integer posfact31 = abs(negfact31);
  assert(negfact31.OK());
  assert(posfact31.OK());
  cout << "negfact31 = " << negfact31 << "\n";
  result = fact30 + negfact31;
  cout << "fact30 + negfact31 = " << result << "\n";
  result = fact30 - negfact31;
  cout << "fact30 - negfact31 = " << result << "\n";
  result = fact30 * negfact31;
  cout << "fact30 * negfact31 = " << result << "\n";
  result = fact30 / negfact31;
  cout << "fact30 / negfact31 = " << result << "\n";
  result = fact30 % negfact31;
  cout << "fact30 % negfact31 = " << result << "\n";
  result = gcd(fact30, negfact31);
  cout << "gcd(fact30, negfact31) = " << result << "\n";
  assert(result == fact30);

  identitytest(one, one, one);
  identitytest(one, one, one);
  identitytest(one, two, fact30);
  identitytest(fact30, posfact31, fact28);
  identitytest(fact30, negfact31, fact28);
  identitytest(negfact31, posfact31, fact28);

  bittest(one, one, one);
  bittest(one, one, one);
  bittest(one, two, fact30);
  bittest(fact30, posfact31, fact28);

  accumtest(one, one, one);
  accumtest(one, one, one);
  accumtest(one, two, fact30);
  accumtest(fact30, posfact31, fact28);

  utiltest(one);
  utiltest(fact30);
  utiltest(posfact31);

  longidentitytest(one, 1, 1);
  longidentitytest(one, 2, 3);
  longidentitytest(fact30, 3, -20);
  longidentitytest(fact30, 4, 20000);
  longidentitytest(negfact31, -100, 20000);

  longbittest(one, 1, 1);
  longbittest(one, 2, 3);
  longbittest(fact30, 4, 20000);
  longbittest(fact28, 1000, 50);

  longaccumtest(one, 1, 1);
  longaccumtest(one, 2, 3);
  longaccumtest(fact30, 4, 20000);
  longaccumtest(fact30, 1000, 50);
  longaccumtest(fact28, 10000000, 100000000);
}

void modtest()
{
  Integer b, e, m;

  m = 1; m <<= 32;
  b = m + 1;

  e = Ipow( 2, 32 );
  b = Ipow( 2, 32 );                  // use b as a comparison
  cout << "2^32 = " << e << "\n";

  e %= (e-1);                         // do same op two ways...
  b = b % (b - 1);

  cout << "2^32 % (2^32-1) = " << e << "\n"; // e is incorrect here
  cout << "2^32 % (2^32-1) = " << b << "\n"; // but b is ok
}

int main()
{
  Integer one = 1;
  cout << "one = " << one << "\n";
  assert(one.OK());
  assert(one == 1);
  cout << "one + 1 = " << (one + 1) << "\n";

  Integer two = 2;
  cout << "two = " << two << "\n";
  assert(two.OK());
  assert(two == 2);

/* inbox/1782 */
  Integer n (0);
  setbit (n, 8);
  clearbit (n, 16);
  cout << "twofiftysix = " << n << '\n';

  facttest(one, two);
  fibtest();
  anothertest();
  iotest();
  modtest();

  cout << "\nEnd of test\n";
  return 0;
}
