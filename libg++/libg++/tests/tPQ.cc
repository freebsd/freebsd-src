/*
 a test file for PQs
*/

#ifdef PTIMES
const int ptimes = 1;
#else
const int ptimes = 0;
#endif

#include <stream.h>
#include <assert.h>
#include <builtin.h>

#define tassert(ex) { cerr << #ex; \
                       if ((ex)) cerr << " OK\n"; \
                       else cerr << " Fail\n"; }

#include "iPQ.h"


int SIZE;

int *nums;
int *odds;
int *dups;

void add(int x[], intPQ& a)
{
  for (int i = 0; i < SIZE; ++i) a.enq(x[i]);
}

#include <MLCG.h>

MLCG randgen;

void permute(int x[])
{
  for (int i = 1; i < SIZE; ++i)
  {
    int j = randgen.asLong() % (i + 1);
    int tmp = x[i]; x[i] = x[j]; x[j] = tmp;
  }
}

void makenums()
{
  for (int i = 0; i < SIZE; ++i) nums[i] = i + 1;
  permute(nums);
}

void makeodds()
{
  for (int i = 0; i < SIZE; ++i) odds[i] = 2 * i + 1;
  permute(odds);
}

void makedups()
{
  for (int i = 0; i < SIZE; i += 2) dups[i] = dups[i+1] = i/2 + 1;
  permute(dups);
}

void printPQ(intPQ& a)
{
  int maxprint = 20;
  cout << "[";
  int k = 0;
  Pix i;
  for (i = a.first(); i != 0 && k < maxprint; a.next(i),++k) 
    cout << a(i) << " ";
  if (i != 0) cout << "...]\n";
  else cout << "]\n";
}

#include "iXPPQ.h"

void XPtest()
{
  intXPPQ a(SIZE);
  add(nums, a);
  intXPPQ b(SIZE);
  add(odds, b);
  intXPPQ c(SIZE);
  add(dups, c); 
  intXPPQ d(a);
  add(nums, d);
  cout << "a: "; printPQ(a);
  cout << "b: "; printPQ(b);
  cout << "c: "; printPQ(c);
  cout << "d: "; printPQ(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE*2);
  assert(a.front() == 1);
  assert(b.front() == 1);
  assert(c.front() == 1);
  assert(d.front() == 1);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  d.del_front();
  assert(d.front() == 1);
  for (j = 1; j <= SIZE; ++j) assert(a.deq() == j);
  assert(a.empty());
  for (j = 1; j <= SIZE*2; j+=2) assert(b.deq() == j);
  assert(b.empty());
  Pix* indices = new Pix [SIZE];
  int m = 0;
  for (Pix i = c.first(); i != 0; c.next(i), c.next(i)) indices[m++] = i;
  assert(m == SIZE/2);
  while (--m >= 0) c.del(indices[m]);
  assert(c.length() == SIZE/2);
  int last = -1;
  j = 0;
  while (!c.empty())
  {
    int current = c.deq();
    assert(last <= current);
    last = current;
    ++j;
  }
  assert(j == SIZE/2);

  delete [] indices;
  d.clear();
  assert(d.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());
}

#include "iPHPQ.h"

void PHtest()
{
  intPHPQ a(SIZE);
  add(nums, a);
  intPHPQ b(SIZE);
  add(odds, b);
  intPHPQ c(SIZE);
  add(dups, c); 
  intPHPQ d(a);
  add(nums, d);
  cout << "a: "; printPQ(a);
  cout << "b: "; printPQ(b);
  cout << "c: "; printPQ(c);
  cout << "d: "; printPQ(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE*2);
  assert(a.front() == 1);
  assert(b.front() == 1);
  assert(c.front() == 1);
  assert(d.front() == 1);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  d.del_front();
  assert(d.front() == 1);
  for (j = 1; j <= SIZE; ++j) assert(a.deq() == j);
  assert(a.empty());
  for (j = 1; j <= SIZE*2; j+=2) assert(b.deq() == j);
  assert(b.empty());
  Pix* indices = new Pix [SIZE];
  int m = 0;
  for (Pix i = c.first(); i != 0; c.next(i), c.next(i)) indices[m++] = i;
  assert(m == SIZE/2);
  while (--m >= 0) c.del(indices[m]);
  assert(c.length() == SIZE/2);
  int last = -1;
  j = 0;
  while (!c.empty())
  {
    int current = c.deq();
    assert(last <= current);
    last = current;
    ++j;
  }
  assert(j == SIZE/2);
  delete [] indices;
  d.clear();
  assert(d.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());
}

#include "iSplayPQ.h"

void Splaytest()
{
  intSplayPQ a;
  add(nums, a);
  intSplayPQ b;
  add(odds, b);
  intSplayPQ c;
  add(dups, c); 
  intSplayPQ d(a);
  add(nums, d);
  cout << "a: "; printPQ(a);
  cout << "b: "; printPQ(b);
  cout << "c: "; printPQ(c);
  cout << "d: "; printPQ(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE*2);
  assert(a.front() == 1);
  assert(b.front() == 1);
  assert(c.front() == 1);
  assert(d.front() == 1);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  d.del_front();
  assert(d.front() == 1);
  for (j = 1; j <= SIZE; ++j) assert(a.deq() == j);
  assert(a.empty());
  for (j = 1; j <= SIZE*2; j+=2) assert(b.deq() == j);
  assert(b.empty());
  Pix* indices = new Pix[SIZE];
  int m = 0;
  for (Pix i = c.first(); i != 0; c.next(i), c.next(i)) indices[m++] = i;
  assert(m == SIZE/2);
  while (--m >= 0) c.del(indices[m]);
  assert(c.length() == SIZE/2);
  int last = -1;
  j = 0;
  while (!c.empty())
  {
    int current = c.deq();
    assert(last <= current);
    last = current;
    ++j;
  }
  assert(j == SIZE/2);
  delete [] indices;
  d.clear();
  assert(d.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());
}



int main(int argv, char** argc)
{
  if (argv > 1)
  {
    SIZE = abs(atoi(argc[1]));
    SIZE &= ~1;
  }
  else
    SIZE = 100;
  nums = new int[SIZE];
  odds = new int[SIZE];
  dups = new int[SIZE];
  makenums();
  makeodds();
  makedups();
  start_timer();
  cout << "Splaytest\n"; Splaytest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "PHtest\n"; PHtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "XPtest\n"; XPtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  return 0;
}
