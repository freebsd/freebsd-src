/*
  test of Deques
*/

#ifdef PTIMES
const int ptimes = 1;
#else
const int ptimes = 0;
#endif

#include <stream.h>
#include <assert.h>

#include "iDeque.h"

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }


int SIZE;

void print(intDeque& a)
{
  int maxprint = 20;
  cout << "[";
  int k = 0;
  while (!a.empty() && k++ < maxprint)
    cout << a.deq() << " ";
  if (k == maxprint) 
    cout << "]\n";
  else
  {
    while (!a.empty()) a.del_front();
    cout << "...]\n";
  }
  assert(a.empty());
}

#include "iXPDeque.h"

void XPtest () 
{
  intXPDeque d(SIZE);
  assert(d.OK());
  for (int i = 0; i < SIZE; ++i)
  {
    if (i % 2 == 0)
      d.enq(i);
    else
      d.push(i);
  }
  assert(d.length() == SIZE);
  assert(d.front() == (SIZE-1));
  assert(d.rear() == (SIZE-2));
  assert(!d.full());
  intXPDeque d1(SIZE/2);
  for (int i = (SIZE-1); i >= 0; --i)
  {
    int x;
    if (i % 2 == 0)
    {
      x = d.rear();
      d.del_rear();
    }
    else
    {
      x = d.front();
      d.del_front();
    }
    d1.enq(x);
  }
  assert(d.empty());
  assert(d1.length() == SIZE);
  assert(d1.front() == (SIZE-1));
  assert(d1.rear() == 0);
  assert(d.OK());
  assert(d1.OK());
  intXPDeque d2 (d1);
  assert(d2.length() == SIZE);
  assert(d2.front() == (SIZE-1));
  assert(d2.OK());
  d1.clear();
  assert(d1.empty());
  d1 = d2;
  assert(d1.length() == SIZE);
  assert(d1.front() == (SIZE-1));
  cout << "d1:"; print(d1);
  assert(d.OK());
  assert(d1.OK());
  assert(d2.OK());
}


#include "iDLDeque.h"

void DLtest () 
{
  intDLDeque d;
  assert(d.OK());
  for (int i = 0; i < SIZE; ++i)
  {
    if (i % 2 == 0)
      d.enq(i);
    else
      d.push(i);
  }
  assert(d.length() == SIZE);
  assert(d.front() == (SIZE-1));
  assert(d.rear() == (SIZE-2));
  assert(!d.full());
  intDLDeque d1;
  for (int i = (SIZE-1); i >= 0; --i)
  {
    int x;
    if (i % 2 == 0)
    {
      x = d.rear();
      d.del_rear();
    }
    else
    {
      x = d.front();
      d.del_front();
    }
    d1.enq(x);
  }
  assert(d.empty());
  assert(d1.length() == SIZE);
  assert(d1.front() == (SIZE-1));
  assert(d1.rear() == 0);
  assert(d.OK());
  assert(d1.OK());
  intDLDeque d2 (d1);
  assert(d2.length() == SIZE);
  assert(d2.front() == (SIZE-1));
  assert(d2.OK());
  d1.clear();
  assert(d1.empty());
  d1 = d2;
  assert(d1.length() == SIZE);
  assert(d1.front() == (SIZE-1));
  cout << "d1:"; print(d1);

  assert(d.OK());
  assert(d1.OK());
  assert(d2.OK());
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

  start_timer();
  cout << "XP deques:\n"; XPtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "DL deques:\n"; DLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  cout << "\nEnd of test\n";
  return 0;
}
