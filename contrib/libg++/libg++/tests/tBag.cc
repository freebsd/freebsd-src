/*
 a test file for Bags
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

#include "iBag.h"

unsigned int hash(int x) { return multiplicativehash(x) ; }

int SIZE;

int *nums;
int *odds;
int *dups;

void add(int x[], intBag& a)
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

void printBag(intBag& a)
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


void generictest(intBag& a, intBag& b, intBag& c)
{
  c.clear();
  assert(c.empty());
  Pix k;
  for (k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());
}

#include "iXPBag.h"

void XPtest()
{
  intXPBag a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  for (int j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intXPBag b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intXPBag c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE);
  intXPBag d(a);
  add(nums, d);
  assert(d.length() == SIZE*2);
  cout << "a: "; printBag(a);
  cout << "b: "; printBag(b);
  cout << "c: "; printBag(c);
  cout << "d: "; printBag(d);
  for (int j = 1; j <= SIZE; ++j) assert(d.nof(j) == 2);
  d.del(1);
  assert(d.nof(1) == 1);
  d.del(1);
  assert(d.nof(1) == 0);
  d.remove(2);
  assert(!d.contains(2));
  for (Pix l = c.first(); l; c.next(l)) d.remove(c(l));
  assert(d.length() == SIZE);
  
  c.clear();
  assert(c.empty());
  for (Pix k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (Pix k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}


#include "iSLBag.h"

void SLtest()
{
  intSLBag a;
  add(nums, a);
  assert(a.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intSLBag b;
  add(odds, b);
  assert(b.length() == SIZE);
  intSLBag c;
  add(dups, c); 
  assert(c.length() == SIZE);
  intSLBag d(a);
  add(nums, d);
  assert(d.length() == SIZE*2);
  cout << "a: "; printBag(a);
  cout << "b: "; printBag(b);
  cout << "c: "; printBag(c);
  cout << "d: "; printBag(d);
  for (j = 1; j <= SIZE; ++j) assert(d.nof(j) == 2);
  d.del(1);
  assert(d.nof(1) == 1);
  d.del(1);
  assert(d.nof(1) == 0);
  d.remove(2);
  assert(!d.contains(2));
  for (Pix l = c.first(); l; c.next(l)) d.remove(c(l));
  assert(d.length() == SIZE);
  
  c.clear();
  assert(c.empty());
  for (Pix k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (Pix k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}


#include "iVHBag.h"

void VHtest()
{
  intVHBag a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intVHBag b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intVHBag c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE);
  intVHBag d(a);
  add(nums, d);
  assert(d.length() == SIZE*2);
  cout << "a: "; printBag(a);
  cout << "b: "; printBag(b);
  cout << "c: "; printBag(c);
  cout << "d: "; printBag(d);
  for (j = 1; j <= SIZE; ++j) assert(d.nof(j) == 2);
  d.del(1);
  assert(d.nof(1) == 1);
  d.del(1);
  assert(d.nof(1) == 0);
  d.remove(2);
  assert(!d.contains(2));
  for (Pix l = c.first(); l; c.next(l)) d.remove(c(l));
  assert(d.length() == SIZE);
  
  c.clear();
  assert(c.empty());
  for (Pix k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (Pix k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iCHBag.h"

void CHtest()
{
  intCHBag a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intCHBag b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intCHBag c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE);
  intCHBag d(a);
  add(nums, d);
  assert(d.length() == SIZE*2);
  cout << "a: "; printBag(a);
  cout << "b: "; printBag(b);
  cout << "c: "; printBag(c);
  cout << "d: "; printBag(d);
  for (j = 1; j <= SIZE; ++j) assert(d.nof(j) == 2);
  d.del(1);
  assert(d.nof(1) == 1);
  d.del(1);
  assert(d.nof(1) == 0);
  d.remove(2);
  assert(!d.contains(2));
  for (Pix l = c.first(); l; c.next(l)) d.remove(c(l));
  assert(d.length() == SIZE);
  
  c.clear();
  assert(c.empty());
  for (Pix k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (Pix k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iOXPBag.h"

void OXPtest()
{
  intOXPBag a(SIZE);
  add(nums, a);
  assert(a.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intOXPBag b(SIZE);
  add(odds, b);
  assert(b.length() == SIZE);
  intOXPBag c(SIZE);
  add(dups, c); 
  assert(c.length() == SIZE);
  intOXPBag d(a);
  add(nums, d);
  assert(d.length() == SIZE*2);
  cout << "a: "; printBag(a);
  cout << "b: "; printBag(b);
  cout << "c: "; printBag(c);
  cout << "d: "; printBag(d);
  for (j = 1; j <= SIZE; ++j) assert(d.nof(j) == 2);
  d.del(1);
  assert(d.nof(1) == 1);
  d.del(1);
  assert(d.nof(1) == 0);
  d.remove(2);
  assert(!d.contains(2));
  for (Pix l = c.first(); l; c.next(l)) d.remove(c(l));
  assert(d.length() == SIZE);
  
  c.clear();
  assert(c.empty());
  for (Pix k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (Pix k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}


#include "iOSLBag.h"

void OSLtest()
{
  intOSLBag a;
  add(nums, a);
  assert(a.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intOSLBag b;
  add(odds, b);
  assert(b.length() == SIZE);
  intOSLBag c;
  add(dups, c); 
  assert(c.length() == SIZE);
  intOSLBag d(a);
  add(nums, d);
  assert(d.length() == SIZE*2);
  cout << "a: "; printBag(a);
  cout << "b: "; printBag(b);
  cout << "c: "; printBag(c);
  cout << "d: "; printBag(d);
  for (j = 1; j <= SIZE; ++j) assert(d.nof(j) == 2);
  d.del(1);
  assert(d.nof(1) == 1);
  d.del(1);
  assert(d.nof(1) == 0);
  d.remove(2);
  assert(!d.contains(2));
  for (Pix l = c.first(); l; c.next(l)) d.remove(c(l));
  assert(d.length() == SIZE);
  
  c.clear();
  assert(c.empty());
  for (Pix k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (Pix k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}

#include "iSplayBag.h"

void Splaytest()
{
  intSplayBag a;
  add(nums, a);
  assert(a.length() == SIZE);
  int j;
  for (j = 1; j <= SIZE; ++j) assert(a.contains(j));
  intSplayBag b;
  add(odds, b);
  assert(b.length() == SIZE);
  intSplayBag c;
  add(dups, c); 
  assert(c.length() == SIZE);
  intSplayBag d(a);
  add(nums, d);
  assert(d.length() == SIZE*2);
  cout << "a: "; printBag(a);
  cout << "b: "; printBag(b);
  cout << "c: "; printBag(c);
  cout << "d: "; printBag(d);
  for (j = 1; j <= SIZE; ++j) assert(d.nof(j) == 2);
  d.del(1);
  assert(d.nof(1) == 1);
  d.del(1);
  assert(d.nof(1) == 0);
  d.remove(2);
  assert(!d.contains(2));
  for (Pix l = c.first(); l; c.next(l)) d.remove(c(l));
  assert(d.length() == SIZE);
  
  c.clear();
  assert(c.empty());
  for (Pix k = a.first(); k != 0; a.next(k)) c.add(a(k));
  for (Pix k = a.first(); k != 0; a.next(k)) assert(c.contains(a(k)));
  c.del(a(a.first()));
  Pix i = a.first();
  assert(!c.contains(a(i)));
  for (a.next(i); i != 0; a.next(i)) assert(c.contains(a(i)));
  c.add(a(a.first()));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = b.first(); k != 0; b.next(k)) c.add(b(k));
  for (i = b.first(); i != 0; b.next(i)) assert(c.contains(b(i)));
  for (i = a.first(); i != 0; a.next(i)) assert(c.contains(a(i)));
  for (Pix k = a.first(); k != 0; a.next(k)) c.remove(a(k));
  for (i = a.first(); i != 0; a.next(i)) assert(!c.contains(a(i)));
  for (i = b.first(); i != 0; b.next(i)) c.del(b(i));
  assert(c.empty());
  assert(a.OK());
  assert(b.OK());
  assert(c.OK());

  generictest(a, b, c);
}


double return_elapsed_time ( double );
double start_timer ( void );

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
  cout << "VHtest\n"; VHtest();
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
  cout << "Splaytest\n"; Splaytest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "OSLtest\n"; OSLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "OXPtest\n"; OXPtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";

  return 0;
}
