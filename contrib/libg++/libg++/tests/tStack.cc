/*
  test of stacks
*/

#ifdef PTIMES
const int ptimes = 1;
#else
const int ptimes = 0;
#endif

#include <stream.h>
#include <assert.h>
#include "iStack.h"

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }


int SIZE;

void print(intStack& a)
{
  int maxprint = 20;
  cout << "[";
  int k = 0;
  while (!a.empty() && k++ < maxprint)
    cout << a.pop() << " ";
  if (k == maxprint) 
    cout << "]\n";
  else
  {
    while (!a.empty()) a.del_top();
    cout << "...]\n";
  }
  assert(a.empty());
}

#include "iXPStack.h"

void XPtest () 
{
  intXPStack s(SIZE/2);
  assert(s.OK());
  for (int i = 0; i < SIZE; ++i)
    s.push(i);
  assert(s.length() == SIZE);
  assert(s.top() == (SIZE-1));
  assert(!s.full());
  intXPStack s1(SIZE*2);
  for (int i = 0; i < SIZE; ++i)
  {
    int x = s.pop();
    assert(x == (SIZE-1) - i);
    s1.push(x);
  }
  assert(s.empty());
  assert(s1.length() == SIZE);
  assert(s1.top() == 0);
  assert(s.OK());
  assert(s1.OK());
  intXPStack s2 (s1);
  assert(s2.length() == SIZE);
  assert(s2.top() == 0);
  assert(s2.OK());
  s1.clear();
  assert(s1.empty());
  s1 = s2;
  assert(s1.length() == SIZE);
  assert(s1.top() == 0);
  assert(s1.OK());
  s1.del_top();
  assert(s1.length() == (SIZE-1));
  assert(s1.top() == 1);
  cout << "s1:"; print(s1);
  assert(s.OK());
  assert(s1.OK());
  assert(s2.OK());
}

#include "iVStack.h"


void Vtest () 
{
  intVStack s(SIZE);
  assert(s.OK());
  for (int i = 0; i < SIZE; ++i)
    s.push(i);
  assert(s.length() == SIZE);
  assert(s.top() == (SIZE-1));
  assert(s.full());
  intVStack s1(SIZE);
  for (int i = 0; i < SIZE; ++i)
  {
    int x = s.pop();
    assert(x == (SIZE-1) - i);
    s1.push(x);
  }
  assert(s.empty());
  assert(s1.length() == SIZE);
  assert(s1.top() == 0);
  assert(s.OK());
  assert(s1.OK());
  intVStack s2 (s1);
  assert(s2.length() == SIZE);
  assert(s2.top() == 0);
  assert(s2.OK());
  s1.clear();
  assert(s1.empty());
  s1 = s2;
  assert(s1.length() == SIZE);
  assert(s1.top() == 0);
  assert(s1.OK());
  s1.del_top();
  assert(s1.length() == (SIZE-1));
  assert(s1.top() == 1);
  cout << "s1:"; print(s1);

  assert(s.OK());
  assert(s1.OK());
  assert(s2.OK());
}

#include "iSLStack.h"

void SLtest () 
{
  intSLStack s;
  assert(s.OK());
  for (int i = 0; i < SIZE; ++i)
    s.push(i);
  assert(s.length() == SIZE);
  assert(s.top() == (SIZE-1));
  assert(!s.full());
  intSLStack s1;
  for (int i = 0; i < SIZE; ++i)
  {
    int x = s.pop();
    assert(x == (SIZE-1) - i);
    s1.push(x);
  }
  assert(s.empty());
  assert(s1.length() == SIZE);
  assert(s1.top() == 0);
  assert(s.OK());
  assert(s1.OK());
  intSLStack s2 (s1);
  assert(s2.length() == SIZE);
  assert(s2.top() == 0);
  assert(s2.OK());
  s1.clear();
  assert(s1.empty());
  s1 = s2;
  assert(s1.length() == SIZE);
  assert(s1.top() == 0);
  assert(s1.OK());
  s1.del_top();
  assert(s1.length() == (SIZE-1));
  assert(s1.top() == 1);

  cout << "s1:"; print(s1);
  assert(s.OK());
  assert(s1.OK());
  assert(s2.OK());
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
  cout << "XP stacks:\n"; XPtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "V stacks:\n"; Vtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "SL stacks:\n"; SLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  cout << "\nEnd of test\n";
  return 0;
}
