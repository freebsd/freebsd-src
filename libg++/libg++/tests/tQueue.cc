/*
  test of Queues
*/

#ifdef PTIMES
const int ptimes = 1;
#else
const int ptimes = 0;
#endif

#include <stream.h>
#include <assert.h>
#include "iQueue.h"

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }


int SIZE;

void print(intQueue& a)
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

#include "iXPQueue.h"

void XPtest () 
{
  intXPQueue q(SIZE/2);
  assert(q.OK());
  for (int i = 0; i < SIZE; ++i)
    q.enq(i);
  assert(q.length() == SIZE);
  assert(q.front() == 0);
  assert(!q.full());
  intXPQueue q1(SIZE*2);
  for (int i = 0; i < SIZE; ++i)
  {
    int x = q.deq();
    assert(x == i);
    q1.enq(x);
  }
  assert(q.empty());
  assert(q1.length() == SIZE);
  assert(q1.front() == 0);
  assert(q.OK());
  assert(q1.OK());
  intXPQueue q2 (q1);
  assert(q2.length() == SIZE);
  assert(q2.front() == 0);
  assert(q2.OK());
  q1.clear();
  assert(q1.empty());
  q1 = q2;
  assert(q1.length() == SIZE);
  assert(q1.front() == 0);
  assert(q1.OK());
  q1.del_front();
  assert(q1.length() == (SIZE-1));
  assert(q1.front() == 1);
  cout << "q1:"; print(q1);
  assert(q.OK());
  assert(q1.OK());
  assert(q2.OK());
}

#include "iVQueue.h"

void Vtest () 
{
  intVQueue q(SIZE);
  assert(q.OK());
  for (int i = 0; i < SIZE; ++i)
    q.enq(i);
  assert(q.length() == SIZE);
  assert(q.front() == 0);
  assert(q.full());
  intVQueue q1(SIZE);
  for (int i = 0; i < SIZE; ++i)
  {
    int x = q.deq();
    assert(x == i);
    q1.enq(x);
  }
  assert(q.empty());
  assert(q1.length() == SIZE);
  assert(q1.front() == 0);
  assert(q.OK());
  assert(q1.OK());
  intVQueue q2 (q1);
  assert(q2.length() == SIZE);
  assert(q2.front() == 0);
  assert(q2.OK());
  q1.clear();
  assert(q1.empty());
  q1 = q2;
  assert(q1.length() == SIZE);
  assert(q1.front() == 0);
  assert(q1.OK());
  q1.del_front();
  assert(q1.length() == (SIZE-1));
  assert(q1.front() == 1);
  cout << "q1:"; print(q1);
  assert(q.OK());
  assert(q1.OK());
  assert(q2.OK());
}

#include "iSLQueue.h"

void SLtest () 
{
  intXPQueue q;
  assert(q.OK());
  for (int i = 0; i < SIZE; ++i)
    q.enq(i);
  assert(q.length() == SIZE);
  assert(q.front() == 0);
  assert(!q.full());
  intXPQueue q1;
  for (int i = 0; i < SIZE; ++i)
  {
    int x = q.deq();
    assert(x == i);
    q1.enq(x);
  }
  assert(q.empty());
  assert(q1.length() == SIZE);
  assert(q1.front() == 0);
  assert(q.OK());
  assert(q1.OK());
  intXPQueue q2 (q1);
  assert(q2.length() == SIZE);
  assert(q2.front() == 0);
  assert(q2.OK());
  q1.clear();
  assert(q1.empty());
  q1 = q2;
  assert(q1.length() == SIZE);
  assert(q1.front() == 0);
  assert(q1.OK());
  q1.del_front();
  assert(q1.length() == (SIZE-1));
  assert(q1.front() == 1);
  cout << "q1:"; print(q1);
  assert(q.OK());
  assert(q1.OK());
  assert(q2.OK());
}

/* Test case from Jocelyn Serot <jserot@alize.univ-bpclermont.fr> */
void
test_resize ()
{
	intVQueue Q(4);
	Q.enq(1);
	Q.enq(2);
	cout << Q.deq() << endl;
	Q.enq(3);
	Q.enq(4);
	cout << Q.deq() << endl;
	Q.enq(5);
	Q.enq(6);
	cout << Q.deq() << endl;
	Q.enq(7);		// Q is now full
	Q.resize(8);		// Lets grow it
	Q.enq(8);		// *** bug manifest at this point
	cout << Q.deq() << endl;
	cout << Q.deq() << endl;
	cout << Q.deq() << endl;
	cout << Q.deq() << endl;	// Q is now empty
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
  cout << "XP queues:\n"; XPtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "V queues:\n"; Vtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";
  start_timer();
  cout << "SL queues:\n"; SLtest();
  if (ptimes) cout << "\ntime = " << return_elapsed_time(0.0) << "\n";

  test_resize ();
  cout << "\nEnd of test\n";
  return 0;
}
