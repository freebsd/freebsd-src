/*
 test/demo of generic lists
*/

#include <assert.h>

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }

#include <iostream.h>
#include "iList.h"

int int_compare(int a, int b)
{
  return a - b;
}

int inc(int x)
{
  return x + 1;
}

int plus(int x, int y)
{
  return x + y;
}

void printint(int x)
{
  cout << x << " ";
}

void print(intList& l)
{
  l.apply(printint);
  cout << "\n";
}

int is_odd(int x)
{
  return x & 1;
}

int is_even(int x)
{
  return (x & 1) == 0;
}

intList sequence(int lo, int hi)
{
  if (lo > hi)
    return intList();
  else
    return intList(lo, sequence(lo+1, hi));
}

int old_rand = 9999;

int get_rand()
{
    old_rand = ((long)old_rand * (long)1243) % (long)971;
    return old_rand;
}

intList randseq(int n)
{
  if (n <= 0)
    return intList();
  int value = get_rand() % 50;
  return intList(value, randseq(--n));
}

main()
{
  intList a = sequence(1, 20);
  cout << "\nintList a = sequence(1, 20);\n"; print(a);
  assert(a.OK());
  for (int i = 0; i < 20; ++i) assert(a[i] == i + 1);
  assert(a.position(2) == 1);
  intList b = randseq(20);
  cout << "\nintList b = randseq(20);\n"; print(b);
  intList c = concat(a, b);
  cout << "\nintList c = concat(a, b);\n"; print(c);
  assert(c.contains(a));
  assert(c.contains(b));
  assert(!(c.find(a).null()));
  assert(c.find(b) == b);
  intList d = map(inc, a);
  for (int i = 0; i < 20; ++i) assert(d[i] == a[i] + 1);
  cout << "\nintList d = map(inc, a);\n"; print(d);
  intList e = reverse(a);
  cout << "\nintList e = reverse(a);\n"; print(e);
  for (int i = 0; i < 20; ++i) assert(e[i] == a[19 - i]);
  intList f = select(is_odd, a);
  cout << "\nintList f = select(is_odd, a);\n"; print(f);
  intList ff = select(is_even, f);
  assert(ff.null());
  int  red = a.reduce(plus, 0);
  cout << "\nint  red = a.reduce(plus, 0);\n"; cout << red;
  int second = a[2];
  cout << "\nint second = a[2];\n"; cout << second;
  intList g = combine(plus, a, b);
  cout << "\nintList g = combine(plus, a, b);\n"; print(g);
  for (int i = 0; i < 20; ++i) assert(g[i] == a[i] + b[i]);
  g.del((intPredicate)is_odd);
  cout << "\ng.del(is_odd);\n"; print(g);
  ff = select(is_odd, g);
  assert(ff.null());
  b.sort(int_compare);
  for (int i = 1; i < 20; ++i) assert(b[i] >= b[i-1]);
  cout << "\nb.sort(int_compare);\n"; print(b);
  intList h = merge(a, b, int_compare);
  cout << "\nintList h = merge(a, b, int_compare);\n"; print(h);
  for (int i = 1; i < 40; ++i) assert(h[i] >= h[i-1]);
  for (Pix p = a.first(); p; a.next(p)) assert(h.contains(a(p)));
  for (Pix p = b.first(); p; b.next(p)) assert(h.contains(b(p)));
  cout << "\nh via Pix:\n";
  for (Pix p = h.first(); p; h.next(p)) cout << h(p) << ", ";
  cout << "\n";
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());
  assert(e.OK());
  assert(f.OK());
  assert(g.OK());
  assert(h.OK());
  cout << "\ndone\n";
}
