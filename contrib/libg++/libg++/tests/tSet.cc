/*
 a test file for sets
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

unsigned int hash(int x) { return multiplicativehash(x) ; }

#include "iSet.h"

int SIZE;

int *nums;
int *odds;
int *dups;

void printset(intSet& a)
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

void add(int x[], intSet& a)
{
  for (int i = 0; i < SIZE; ++i) a.add(x[i]);
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

void makedups()
{
  for (int i = 0; i < SIZE; i += 2) dups[i] = dups[i+1] = i/2 + 1;
  permute(dups);
}
               

void generictest(intSet& a, intSet& b, intSet& c)
{
  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
}

#include "iXPSet.h"

void XPtest()
{
  intXPSet a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intXPSet b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intXPSet c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intXPSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}


#include "iSLSet.h"

void SLtest()
{
  intSLSet a;
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intSLSet b;
  add(odds, b);
  assert(b.length() == SIZE);
  intSLSet c;
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intSLSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}


#include "iVHSet.h"

void VHtest()
{
  intVHSet a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intVHSet b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intVHSet c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intVHSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iVOHSet.h"

void VOHtest()
{
  intVOHSet a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intVOHSet b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intVOHSet c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intVOHSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iCHSet.h"

void CHtest()
{
  intCHSet a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intCHSet b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intCHSet c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intCHSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iOXPSet.h"

void OXPtest()
{
  intOXPSet a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intOXPSet b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intOXPSet c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intOXPSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}



#include "iOSLSet.h"

void OSLtest()
{
  intOSLSet a;
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intOSLSet b;
  add(odds, b);
  assert(b.length() == SIZE);
  intOSLSet c;
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intOSLSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iBSTSet.h"

void BSTtest()
{
  intBSTSet a;
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  a.balance();
  assert(a.OK());
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intBSTSet b;
  add(odds, b);
  assert(b.length() == SIZE);
  intBSTSet c;
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intBSTSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iAVLSet.h"

void AVLtest()
{
  intAVLSet a;
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intAVLSet b;
  add(odds, b);
  assert(b.length() == SIZE);
  intAVLSet c;
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intAVLSet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iSplaySet.h"

void Splaytest()
{
  intSplaySet a;
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intSplaySet b;
  add(odds, b);
  assert(b.length() == SIZE);
  intSplaySet c;
  add(dups, c); 
  assert(c.length() == SIZE/2);
  assert(c <= a);
  intSplaySet d(a);
  d &= b;
  cout << "a: "; printset(a);
  cout << "b: "; printset(b);
  cout << "c: "; printset(c);
  cout << "d: "; printset(d);
  assert(d.length() == SIZE/2);
  for (Pix p = d.first(); p; d.next(p)) assert((d(p) & 1) != 0);
  a.del(1);
  assert(a.length() == SIZE-1);
  assert(!a.contains(1));

  c.clear();
  assert(c.empty());
  c |= a;
  assert(c == a);
  assert(c <= a);
  c.del(a(a.first()));
  assert(c <= a);
  assert(c != a);
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c |= b;
  assert(b <= c);
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c &= a;
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  c -= a;
  assert(!(a <= c));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}


int main(int argc, char** argv)
{
  if (argc > 1)
  {
    SIZE = abs(atoi(argv[1]));
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
  cout << "VHtest\n"; VHtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "VOHtest\n"; VOHtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "CHtest\n"; CHtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "SLtest\n"; SLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "XPtest\n"; XPtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "OXPtest\n"; OXPtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "OSLtest\n"; OSLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "BSTtest\n"; BSTtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "AVLtest\n"; AVLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "Splaytest\n"; Splaytest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";

  return 0;
}
