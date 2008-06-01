/*
 test/demo of Vecs, AVecs
*/

#include <stream.h>
#include "iVec.h"
#include "iAVec.h"

int int_compare(int a, int b)
{
  return a - b;
}

int plus(int a, int b)
{
  return a + b;
}

int inc(int a)
{
  return a + 1;
}

void printint(int a)
{
  cout << a << " ";
}

void print(intVec a)
{
  a.apply(printint);
  cout << "\n";
}

#include <MLCG.h>

MLCG randgen;
    
int main()
{
  intVec a(20);
  int i;
  for (i = 0; i < a.capacity(); ++i) a[i] = randgen.asLong() % 100;
  cout << "a: "; print(a);
  a.sort(int_compare);
  cout << "a.sort():"; print(a);
  intVec b = map(inc, a);
  cout << "b = map(inc, a): "; print(b);
  intVec c = merge(a, b, int_compare);
  cout << "c = merge(a, b): "; print(c);
  intVec d = concat(a, b);
  cout << "d = concat(a, b): "; print(d);
  d.resize(10);
  cout << "d.resize(10): "; print(d);
  d.reverse();
  cout << "d.reverse(): "; print(d);
  d.fill(0, 4, 4);
  cout << "d.fill(0, 4, 4): "; print(d);
  cout << "d.reduce(plus, 0) = " <<   d.reduce(plus, 0) << "\n";
  intVec e = d.at(2, 5);
  cout << "e = d.at(2, 5): "; print(e);

  intAVec x(20);
  for (i = 0; i < x.capacity(); ++i) x[i] = i;
  cout << "x: "; print(x);
  intAVec y(20);
  for (i = 0; i < y.capacity(); ++i) y[i] = randgen.asLong() % 100 + 1;
  cout << "y: "; print(y);

  cout << "x + y: "; print(x + y);
  cout << "x - y: "; print(x - y);
  cout << "product(x, y): "; print(product(x,y));
  cout << "quotient(x, y): "; print(quotient(x,y));
  cout << "x * y: " << (x * y) << "\n";

  cout << "x + 2: "; print(x + 2);
  cout << "x - 2: "; print(x - 2);
  cout << "x * 2: "; print(x * 2);
  cout << "x / 2: "; print(x / 2);

  intAVec z(20, 1);
  cout << "z(20, 1): "; print(z);
  cout << "z = -z: "; print(z = -z);
  cout << "z += x: "; print(z += x);
  cout << "z -= x: "; print(z -= x);

  cout << "x.sum(): " << x.sum() << "\n";
  cout << "x.sumsq(): " << x.sumsq() << "\n";
  cout << "x.min(): " << x.min() << "\n";
  cout << "x.max(): " << x.max() << "\n";
  cout << "x.min_index(): " << x.min_index() << "\n";
  cout << "x.max_index(): " << x.max_index() << "\n";

  cout << "\nEnd of test\n";
  return 0;
}
