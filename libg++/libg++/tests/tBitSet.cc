/*
 a test/demo of BitSets
*/

#include <assert.h>

#define tassert(ex) { cerr << #ex; \
                       if ((ex)) cerr << "OK\n"; \
                       else cerr << "Fail\n"; }



#include <BitSet.h>


void test3S(BitSet a, BitSet b, BitSet c)
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

  BitSet x = a;
  x &= b;
  assert(x == (a & b));
  x |= c;
  assert(x == ((a & b) | c));
  x -= a;
  assert(x == (((a & b) | c) - a));
  x ^= b;
  assert(x == ((((a & b) | c) - a) ^ b));
  assert(x.OK());
}

/* This regression test found a bug in BitSetresize.
   Based on a bug report from Joaquim Jorge <jorgej@avs.cs.rpi.edu>.*/

void
test4()
{
  BitSet a;    cout << "a: " << a << endl;
  a.set(1, 2); cout << "after set(1,2): " << a << endl;
  a = BitSet();cout << "after copy: " << a << endl;
  a.set(1);    cout << "after set(1): " << a << endl;
}

int main()
{
  cout << "BitSet tests:\n";

  BitSet a;
  cout << "a = " << a << "\n";
  assert(a.OK());

  BitSet b = longtoBitSet(1024);
  cout << "b = " << b << "\n";
  assert(b.OK());
  assert(b[10] == 1);
  assert(b.count() == 1);
  b[0] = b[10];
  assert(b[0] == 1);
  assert(b.count() == 2);

  BitSet c = atoBitSet("1010101010101010101010101010101010101010");
  cout << "c = " << c << "\n";
  assert(c.OK());
  assert(c.count() == 20);
  for (int i = 0; i < 40; i += 2)
  {
    assert(c[i] == 1);
    assert(c[i+1] == 0);
  }
  for (int p = 0; p < 5; ++p)
    cout << "c[" << p << "] =" << int(c[p]) << "\n";

  BitSet d = atoBitSet("0011001100110011001100110011001100110011");
  cout << "d = " << d << "\n";
  assert(d.OK());
  assert(d.count() == 20);
  assert(d.count(0) == -1);

  BitSet e = atoBitSet("1111000011110000111100001111000011110000");
  cout << "e = " << e << "\n";
  assert(e.OK());
  assert(e.count() == 20);

  BitSet u = ~a;
  cout << "u = ~a = " << u << "\n";
  assert(a == ~u);

  BitSet g = ~e;
  cout << "g = ~e = " << g << "\n";

  cout << "~c = " << (~c) << "\n";
  cout << "c & d = " << (c & d) << "\n";
  cout << "c | d = " << (c | d) << "\n";
  cout << "c - d = " << (c - d) << "\n";
  cout << "c ^ d = " << (c ^ d) << "\n";

  test3S(b, c, d);
  test3S(a, a, a);
  test3S(a, b, c);
  test3S(a, c, b);
  test3S(c, b, a);
  test3S(c, c, c);
  test3S(c, d, e);
  test3S(e, d, c);

  BitSet f = b;
  cout << "f = b = " << f << "\n";
  f &= c;
  cout << "f &= c = " << f << "\n";
  f |= d;
  cout << "f |= d = " << f << "\n";
  f -= e;
  cout << "f -= e = " << f << "\n";
  f ^= u;
  cout << "f ^= u = " << f << "\n";
  assert(f.OK());

  assert(c != d);
  assert(!(c == d));
  assert(!(c < d));
  assert(!(c > d));
  assert(!(c <= d));
  assert(!(c >= d));


  BitSet h = d;
  cout << "h = d\n:" << h << "\n";

  assert(d == h);
  assert(d <= h);
  assert(d >= h);
  assert(!(d != h));
  assert(!(d > h));
  assert(!(d < h));

  h.set(0);
  cout << "h.set(0):\n" << h << "\n";

  assert(!(d == h));
  assert(!(d >= h));
  assert(!(d > h));
  assert((d != h));
  assert(d <= h);
  assert((d < h));

  h.set(65);
  cout << "h.set(65):\n" << h << "\n";
  assert(h[65] == 1);
  assert(h[64] == 0);
  assert(h[66] == 0);
  h.clear(2);
  cout << "h.clear(2):\n" << h << "\n";
  assert(h[2] == 0);
  assert(h[3] == 1);
  assert(h[11] == 1);
  h.invert(11,20);
  cout << "h.invert(11,20):\n" << h << "\n";
  assert(h[11] == 0);
  h.set(21,30);
  cout << "h.set(21,30):\n" << h << "\n";
  assert(h[21] == 1);
  h.clear(31,40);
  cout << "h.clear(31, 40):\n" << h << "\n";
  assert(h[33] == 0);
  cout << "h.test(0,5) = " << h.test(0, 5) << "\n";
  cout << "h.test(31,40) = " << h.test(31, 40) << "\n";

  cout << "set bits in e:\n";
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

  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());
  assert(e.OK());
  assert(f.OK());
  assert(g.OK());
  assert(h.OK());

  test4();

  cout << "\nEnd of test.\n";
  return 0;
}

