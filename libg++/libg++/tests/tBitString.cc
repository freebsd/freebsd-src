/*
 a test/demo of BitStrings
*/

#include <assert.h>
#define tassert(ex) { cerr << #ex; \
                       if ((ex)) cerr << " OK\n"; \
                       else cerr << " Fail\n"; }


#include <BitString.h>

void doubletest(BitString a)
{
  BitString x;
  BitString y;
  x = a + reverse(a);
  for (int i = 0; i < 7; ++i)
  {
    y = x;
    x += x;
    assert(x == reverse(x));
    assert(x.index(y) == 0);
    assert(x.index(y, -1) == x.length() / 2);
    assert(x.OK());
  }
}

// identities for equal-length args

void identitytest(BitString a, BitString b, BitString c)
{
// neg
  assert(~(~a) == a);
// commutative
  assert((a | b) ==  (b | a));
  assert((a & b) ==  (b & a));
// associative
  assert((a | (b | c)) == ((a | b) | c));
  assert((a & (b & c)) == ((a & b) & c));
// distrib
  assert((a & (b | c)) == ((a & b) | (a & c)));
  assert((a | (b & c)) == ((a | b) & (a | c)));
// absorption
  assert((a & (a | b)) == a);
  assert((a | (a & b)) == a);
// demorgan
  assert((a | b) == ~(~a & ~b));
  assert((a & b) == ~(~a | ~b));
// def of -
  assert((a - b) == (a & ~b));
  assert(((a - b) | b) == (a | b));
// def of disjoint union
  assert((a ^ b) == ((a | b) & ~(a & b)));
  assert((a ^ b) == ((a - b) | (b - a)));
// shift
  assert(((a << 1) >> 1) == a);
// concat
  assert((a + (b + c)) == ((a + b) + c));

  BitString x;
  x = a + b;
  assert(x.after(a) == b);
  assert(x.before(b, -1) == a);

  x = a;
  int l = x.length();
  x.set(l);
  assert(x == (a + 1));
  x.clear(l);
  assert(x == (a + 0));
}

void accumtest(const BitString& a, const BitString& b, const BitString& c)
{
  BitString x = a;
  x &= b;
  assert(x == (a & b));
  x |= c;
  assert(x == ((a & b) | c));
  x -= a;
  assert(x == (((a & b) | c) - a));
  x ^= b;
  assert(x == ((((a & b) | c) - a) ^ b));
  x += c;
  assert(x == (((((a & b) | c) - a) ^ b) + c));
  x <<= 7;
  assert(x == ((((((a & b) | c) - a) ^ b) + c) << 7));
  x >>= 5;
  assert(x == (((((((a & b) | c) - a) ^ b) + c) << 7) >> 5));
  x += 0;
  assert(x == ((((((((a & b) | c) - a) ^ b) + c) << 7) >> 5) + 0));

  assert(x.OK());
}


void cmptest(BitString& x)
{
  BitString a = x;
  a[0] = 0;
  BitString b = a;

  assert(a == b);
  assert(a <= b);
  assert(a >= b);
  assert(!(a != b));
  assert(!(a > b));
  assert(!(a < b));
  assert(lcompare(a, b) == 0);
  assert(a.matches(b));
  assert(a.contains(b));

  b[0] = 1;
  cout << "b.set(0)       :" << b << "\n";

  assert(!(a == b));
  assert(!(a >= b));
  assert(!(a > b));
  assert((a != b));
  assert(a <= b);
  assert((a < b));
  assert(lcompare(a, b) < 0);
  assert(!a.matches(b));
  assert(!a.contains(b));
  assert(a.after(0) == b.after(0));

  b.set(65);
  cout << "b.set(65):\n" << b << "\n";
  assert(b[65] == 1);
  assert(b[64] == 0);
  assert(b.length() == 66);
  b.clear(2);
  cout << "b.clear(2):\n" << b << "\n";
  assert(b[2] == 0);
  b.set(11);
  b.invert(11,20);
  cout << "b.invert(11,20):\n" << b << "\n";
  assert(b[11] == 0);
  b.set(21,30);
  cout << "b.set(21,30):\n" << b << "\n";
  assert(b[21] == 1);
  b.clear(31,40);
  cout << "b.clear(31, 40):\n" << b << "\n";
  assert(b.test(33, 38) == 0);
}

void subtest(BitString c)
{
  BitString k = c.at(1, 4);
  cout << "k = " << k << "\n";
  assert(c.index(k) == 1);
  assert(c.index(k, -1) != -1);
  
  cout << "c.before(k) = " << c.before(k) << "\n";
  assert(c.before(k) == c.before(1));
  cout << "c.at(k)     = " <<  c.at(k) << "\n";
  assert(c.at(k) == k);
  cout << "c.after(k)  = " << c.after(k) << "\n";
  assert(c.after(k) == c.after(4));
  c.after(k) = k;
  cout << "c.after(k)=k :" << c << "\n";
  assert(c.after(4) == k);
  c.before(k) = k;
  cout << "c.before(k)=k:" << c << "\n";
  assert(c.after(c.after(k)) == k);
  
  assert(c.contains(k, 0));
  assert(common_prefix(c, k) == k);
  assert(common_suffix(c, k) == k);
  cout << "reverse(k)           = " << reverse(k) << "\n";
  k.left_trim(0);
  assert(k[0] == 1);
  cout << "k.left_trim(0)       : " << k << "\n";
  k.right_trim(1);
  assert(k[k.length() - 1] == 0);
  cout << "k.right_trim(1)      : " << k << "\n";
}

int main()
{
  BitString a;
  BitString b = atoBitString("1000000001");
  BitString c = atoBitString("10101010101010101010");
  BitString d = atoBitString("00110011001100110011");
  BitString e = atoBitString("11110000111100001111");
  BitString f = b;
  BitString g = ~e;
  BitString h = d;
  BitString zz;

  assert(a.OK());
  assert(a.empty());
  assert(b.OK());
  assert(!b.empty());
  assert(c.OK());
  assert(c.count(1) == 10);
  assert(c.count(0) == 10);
  assert(d.OK());
  assert(c.count(1) == 10);
  assert(c.count(0) == 10);
  assert(e.OK());
  assert(e.count(1) == 12);
  assert(e.count(0) == 8);
  assert(f == b);
  assert(h == d);
  assert(g == ~e);
  assert(~g == e);
  assert(f.OK());
  assert(g.OK());
  assert(h.OK());

  cout << "a      = " << a << "\n";
  cout << "b      = " << b << "\n";
  cout << "c      = " << c << "\n";
  cout << "d      = " << d << "\n";
  cout << "e      = " << e << "\n";
  cout << "f = b  = " << f << "\n";
  cout << "g = ~e = " << g << "\n";
  cout << "h = d  = " << h << "\n";

  for (int i = 0; i < 20; ++i)
  {
    assert(h[i] == d[i]);
    assert(g[i] != e[i]);
    assert(c[i] == !(i % 2));
  }

  cout << "bits in e:\n";
  for (int p = e.first(); p >= 0; p = e.next(p)) 
  {
    assert(e[p] == 1);
    cout << p << " ";
  }
  cout << "\n";

  cout << "clear bits in g (reverse order):\n";
  for (int p = g.last(0); p >= 0; p = g.prev(p, 0)) 
  {
    assert(g[p] == 0);
    cout << p << " ";
  }
  cout << "\n";

  cout << "~c     = " << (~c) << "\n";
  cout << "c & d  = " << (c & d) << "\n";
  cout << "c | d  = " << (c | d) << "\n";
  cout << "c - d  = " << (c - d) << "\n";
  cout << "c ^ d  = " << (c ^ d) << "\n";
  cout << "c + d  = " << (c + d) << "\n";
  cout << "c <<2  = " << (c << 2) << "\n";
  cout << "c >>2  = " << (c >> 2) << "\n";

  f &= c;
  cout << "f &= c = " << f << "\n";
  f |= d;
  cout << "f |= d = " << f << "\n";
  f -= d;
  cout << "f -= e = " << f << "\n";
  f ^= c;
  cout << "f ^= c = " << f << "\n";
  f += b;
  cout << "f += b = " << f << "\n";
  f <<= 5;
  cout << "f <<=5 = " << f << "\n";
  f >>= 10;
  cout << "f >>=10= " << f << "\n";

  assert(c != d);
  assert(!(c == d));
  assert(!(c < d));
  assert(!(c > d));
  assert(!(c <= d));
  assert(!(c >= d));
  assert(lcompare(c, d) > 0);


  BitString l = c + d + c;
  cout << "l = " << l << "\n";
  BitPattern pat(d, e);
  assert(pat.OK());
  cout << "BitPattern pat = " << pat << "\n";
  cout << "pat.pattern    = " << pat.pattern << "\n";
  cout << "pat.mask       = " << pat.mask << "\n";
  assert(d.matches(pat));
  cout << "l.index(pat)   = " << l.index(pat) << "\n";
  cout << "l.index(pat,-1)= " << l.index(pat, -1) << "\n";
  cout << "l.before(pat)  = " << l.before(pat) << "\n";
  cout << "l.at(pat)      = " << l.at(pat) << "\n";
  cout << "l.after(pat)   = " << l.after(pat) << "\n";
  int eind = l.index(pat);
  l.at(pat) = e;
  assert(l.index(e) == eind);

  identitytest(d, g, h);
  identitytest(a, a, a);
  identitytest(c, d, e);
  identitytest(e, d, c);
  identitytest(longtoBitString(0), longtoBitString((unsigned)(~(0L))), 
               shorttoBitString(1025));
  identitytest(a+b+c+d+e+f+g+h, h+g+f+e+d+c+b+a, a+c+e+g+b+d+f+h);

  accumtest(d, g, h);
  accumtest(a, b, c);
  accumtest(c, d, e);
  accumtest(e, d, c);
  accumtest(a+b+c+d+e+f+g+h+l, f+e+d+c+b+a+pat.mask, e+g+b+d+f+h+pat.pattern);

  doubletest(a);
  doubletest(b);
  doubletest(c);
  doubletest(a+b+c+d+e+f+g+h);

  cmptest(b);
  cmptest(d);

  subtest(c);
  subtest(d);

  for (int i=0; i<64; i++) {
      zz += 1;
  }
  cout << "zz = " << zz << "\n";

  cout << "\nEnd of test.\n";
  return 0;
}
