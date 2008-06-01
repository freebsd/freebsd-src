/*
 a test file for Maps
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

#include "iMap.h"

unsigned int hash(int x) { return multiplicativehash(x) ; }

int SIZE;

int *nums;
int *odds;
int *perm;

void add(int x[], int y[], intintMap& a)
{
  for (int i = 0; i < SIZE; ++i) a[x[i]] = y[i];
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
}

void makeodds()
{
  for (int i = 0; i < SIZE; ++i) odds[i] = 2 * i + 1;
  permute(odds);
}

void makeperm()
{
  for (int i = 0; i < SIZE; ++i) perm[i] = i + 1;
  permute(perm);
}

void printMap(intintMap& a)
{
  int maxprint = 20;
  cout << "[";
  int k = 0;
  Pix i;
  for (i = a.first(); i != 0 && k < maxprint; a.next(i),++k) 
    cout << "(" << a.key(i) << ", " <<  a.contents(i) << ") ";
  if (i != 0) cout << "...]\n";
  else cout << "]\n";
}

#include "iSplayMap.h"

void Splaytest()
{
  intintSplayMap a(-1);
  add(nums, perm, a);
  intintSplayMap b(-1);
  add(perm, nums, b);
  intintSplayMap c(-1);
  add(perm, odds, c); 
  intintSplayMap d(a);
  add(nums, nums, d);
  cout << "a: "; printMap(a);
  cout << "b: "; printMap(b);
  cout << "c: "; printMap(c);
  cout << "d: "; printMap(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  assert(a[SIZE+1] = -1);

  for (j = 1; j <= SIZE; ++j) assert(b.contains(j));

  for (j = 1; j <= SIZE; ++j) assert(b[a[j]] == j);
  for (j = 1; j <= SIZE; ++j) assert(a[b[j]] == j);

  for (j = 1; j <= SIZE; ++j) assert((c[j] & 1) != 0);

  for (j = 1; j <= SIZE; ++j) assert(d[j] == j);

  d.del(1);
  assert(!d.contains(1));
  for (j = 1; j <= SIZE; ++j) d.del(j);
  assert(d.empty());

  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());

}

#include "iVHMap.h"

void VHtest()
{
  intintVHMap a(-1, SIZE);
  add(nums, perm, a);
  intintVHMap b(-1, SIZE);
  add(perm, nums, b);
  intintVHMap c(-1, SIZE);
  add(perm, odds, c); 
  intintVHMap d(a);
  add(nums, nums, d);
  cout << "a: "; printMap(a);
  cout << "b: "; printMap(b);
  cout << "c: "; printMap(c);
  cout << "d: "; printMap(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  assert(a[SIZE+1] = -1);

  for (j = 1; j <= SIZE; ++j) assert(b.contains(j));

  for (j = 1; j <= SIZE; ++j) assert(b[a[j]] == j);
  for (j = 1; j <= SIZE; ++j) assert(a[b[j]] == j);

  for (j = 1; j <= SIZE; ++j) assert((c[j] & 1) != 0);

  for (j = 1; j <= SIZE; ++j) assert(d[j] == j);

  d.del(1);
  assert(!d.contains(1));
  for (j = 1; j <= SIZE; ++j) d.del(j);
  assert(d.empty());

  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());

}

#include "iCHMap.h"

void CHtest()
{
  intintCHMap a(-1, SIZE);
  add(nums, perm, a);
  intintCHMap b(-1, SIZE);
  add(perm, nums, b);
  intintCHMap c(-1, SIZE);
  add(perm, odds, c); 
  intintCHMap d(a);
  add(nums, nums, d);
  cout << "a: "; printMap(a);
  cout << "b: "; printMap(b);
  cout << "c: "; printMap(c);
  cout << "d: "; printMap(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  assert(a[SIZE+1] = -1);

  for (j = 1; j <= SIZE; ++j) assert(b.contains(j));

  for (j = 1; j <= SIZE; ++j) assert(b[a[j]] == j);
  for (j = 1; j <= SIZE; ++j) assert(a[b[j]] == j);

  for (j = 1; j <= SIZE; ++j) assert((c[j] & 1) != 0);

  for (j = 1; j <= SIZE; ++j) assert(d[j] == j);

  d.del(1);
  assert(!d.contains(1));
  for (j = 1; j <= SIZE; ++j) d.del(j);
  assert(d.empty());

  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());

}

#include "iAVLMap.h"

void AVLtest()
{
  intintAVLMap a(-1);
  add(nums, perm, a);
  intintAVLMap b(-1);
  add(perm, nums, b);
  intintAVLMap c(-1);
  add(perm, odds, c); 
  intintAVLMap d(a);
  add(nums, nums, d);
  cout << "a: "; printMap(a);
  cout << "b: "; printMap(b);
  cout << "c: "; printMap(c);
  cout << "d: "; printMap(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  assert(a[SIZE+1] = -1);

  for (j = 1; j <= SIZE; ++j) assert(b.contains(j));

  for (j = 1; j <= SIZE; ++j) assert(b[a[j]] == j);
  for (j = 1; j <= SIZE; ++j) assert(a[b[j]] == j);

  for (j = 1; j <= SIZE; ++j) assert((c[j] & 1) != 0);

  for (j = 1; j <= SIZE; ++j) assert(d[j] == j);

  d.del(1);
  assert(!d.contains(1));
  for (j = 1; j <= SIZE; ++j) d.del(j);
  assert(d.empty());

  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());

}

#include "iRAVLMap.h"

void RAVLtest()
{
  intintRAVLMap a(-1);
  add(nums, perm, a);
  intintRAVLMap b(-1);
  add(perm, nums, b);
  intintRAVLMap c(-1);
  add(perm, odds, c); 
  intintRAVLMap d(a);
  add(nums, nums, d);
  cout << "a: "; printMap(a);
  cout << "b: "; printMap(b);
  cout << "c: "; printMap(c);
  cout << "d: "; printMap(d);
  assert(a.length() == SIZE);
  assert(b.length() == SIZE);
  assert(c.length() == SIZE);
  assert(d.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  for (j = 1; j <= a.length(); ++j) assert(a.rankof(j) == j);
  for (j = 1; j <= a.length(); ++j) assert(a.key(a.ranktoPix(j)) == j);
  assert(a[SIZE+1] = -1);

  for (j = 1; j <= SIZE; ++j) assert(b.contains(j));

  for (j = 1; j <= SIZE; ++j) assert(b[a[j]] == j);
  for (j = 1; j <= SIZE; ++j) assert(a[b[j]] == j);

  for (j = 1; j <= SIZE; ++j) assert((c[j] & 1) != 0);

  for (j = 1; j <= SIZE; ++j) assert(d[j] == j);

  d.del(1);
  assert(!d.contains(1));
  for (j = 1; j <= SIZE; ++j) d.del(j);
  assert(d.empty());

  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
  assert(d.OK());

}

double return_elapsed_time ( double );
double start_timer ( );

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
  perm = new int[SIZE];
  makenums();
  makeodds();
  makeperm();
  start_timer();
  cout << "Splaytest\n"; Splaytest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "VHtest\n"; VHtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "CHtest\n"; CHtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "AVLtest\n"; AVLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "RAVLtest\n"; RAVLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";

  return 0;
}
