#include <_G_config.h>

#ifdef _G_NO_TEMPLATESS

main()
{
    fprintf(stderr, "(template-based classes not available)\n");
    return 0;
}
#else

/*
 test/demo of linked structures
*/


#include <assert.h>

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }

#include <iostream.h>
#include "SLList.h"
#include "DLList.h"

void printlist(SLList<int>& l)
{
  for (Pix p = l.first(); p != 0; l.next(p)) cout << l(p) << " ";
  cout << "\n";
}


void SLtest()
{
  int i;
  SLList<int> a;
  assert(a.OK());
  assert(a.empty());
  cout << "prepending...\n";
  for (i = 0; i < 10; ++i)
  {
    assert(a.length() == i);
    a.prepend(i);
    assert(a.front() == i);
  }
  cout << "a: "; printlist(a);
  cout << "appending...\n";
  for (i = 0; i < 10; ++i)
  {
    assert(a.length() == 10 + i);
    a.append(i);
    assert(a.rear() == i);
  }
  cout << "a: "; printlist(a);
  SLList<int> b = a;
  cout << "b = a: " << "\n"; printlist(b);
  assert(b.OK());
  assert(b.length() == a.length());
  assert(b.front() == a.front());
  assert(b.rear() == a.rear());
  cout << "remove_front of first 10 elements:\n";
  for (i = 0; i < 10; ++i) 
  {
    assert(b.length() == 20 - i);
    assert(b.front() == 9 - i);
    b.remove_front();
  }
  assert(b.length() == 10);
  cout << "b: "; printlist(b);

  cout << "inserting 100 after sixth element...\n";
  Pix bp = b.first();
  for (i = 0; i < 5; ++i) b.next(bp);
  b.ins_after(bp, 100);
  assert(b.length() == 11);
  cout << "b: "; printlist(b);
  a.join(b);
  cout << "after a.join(b)\n"; printlist(a);
  assert(b.empty());
  assert(a.length() == 31);
  cout << "b: " << "\n"; printlist(b);
  b.prepend(999);
  cout << "b: " << "\n"; printlist(b);
  assert(b.length() == 1);
  assert(b.front() == 999);
  assert(b.rear() == 999);
  assert(b.OK());
  SLList<int> bb = b;
  cout << "bb: " << "\n"; printlist(bb);
  assert(bb.OK());
  assert(bb.length() == 1);
  assert(bb.front() == 999);
  assert(bb.rear() == 999);
  assert(bb.remove_front() == 999);
  b.prepend(1234);
  assert(b.length() == 2);
  b.del_after(b.first());
  assert(b.rear() == 1234);
  assert(b.length() == 1);
  b.del_after(0);
  assert(b.length() == 0);

  assert(a.OK());
  assert(b.OK());
  assert(bb.OK());
}

void printDlist(DLList<int>& l)
{
  for (Pix p = l.first(); p != 0; l.next(p)) cout << l(p) << " ";
  cout << "\n";
}

void DLtest()
{
  int i;
  DLList<int> a;
  assert(a.OK());
  assert(a.empty());
  assert(a.length() == 0);
  cout << "prepending...\n";
  for (i = 0; i < 10; ++i)
  {
    assert(a.length() == i);
    a.prepend(i);
    assert(a.front() == i);
  }
  cout << "a: " << "\n"; printDlist(a);
  cout << "appending...\n";
  for (i = 0; i < 10; ++i)
  {
    assert(a.length() == 10 + i);
    a.append(i);
    assert(a.rear() == i);
  }
  cout << "a: "; printDlist(a);
  DLList<int> b = a;
  assert(b.OK());
  assert(b.length() == a.length());
  assert(b.front() == a.front());
  assert(b.rear() == a.rear());
  cout << "b = a: "; printDlist(b);
  cout << "remove_front of first 10 elements:\n";
  for (i = 0; i < 10; ++i) 
  {
    assert(b.length() == 20 - i);
    assert(b.front() == 9 - i);
    b.remove_front();
  }
  assert(b.length() == 10);
  cout << "b: "; printDlist(b);

  cout << "inserting 100 after sixth element...\n";
  Pix bp = b.first();
  for (i = 0; i < 5; ++i) b.next(bp);
  b.ins_after(bp, 100);
  assert(b.length() == 11);
  cout << "b: "; printDlist(b);
  DLList<int> aa = a;
  aa.join(b);
  cout << "after aa = a; aa.join(b)\n"; printDlist(aa);
  assert(aa.length() == 31);
  assert(b.empty());
  cout << "b: " << "\n"; printDlist(b);
  b.prepend(999);
  cout << "b: " << "\n"; printDlist(b);
  assert(b.length() == 1);
  assert(b.front() == 999);
  assert(b.rear() == 999);
  assert(b.OK());
  DLList<int> bb = b;
  cout << "bb: " << "\n"; printDlist(bb);
  assert(bb.OK());
  assert(bb.length() == 1);
  assert(bb.front() == 999);
  assert(bb.rear() == 999);
  assert(bb.remove_front() == 999);
  assert(bb.OK());
  b.prepend(1234);
  assert(b.length() == 2);
  bp = b.first();
  b.next(bp);
  b.del(bp, -1);
  assert(b.rear() == 1234);
  assert(b.length() == 1);
  b.del(bp);
  assert(b.length() == 0);

  DLList<int> z = a;
  cout << "z = a: "; printDlist(z);
  assert(z.OK());
  assert(z.length() == 20);
  cout << "remove_rear of last 10 elements:\n";
  for (i = 0; i < 10; ++i) 
  {
    assert(z.length() == 20 - i);
    assert(z.rear() == 9 - i);
    z.remove_rear();
  }
  assert(z.length() == 10);

  cout << "z: "; printDlist(z);

  cout << "inserting 100 before alternate elements...\n";
  Pix zp;
  for (zp = z.first(); zp; z.next(zp))
  {
    z.ins_before(zp, 100);
  }
  assert(z.length() == 20);
  cout << "z: "; printDlist(z);

  cout << "inserting 200 after sixth element...\n";
  zp = z.first();
  for (i = 0; i < 5; ++i) z.next(zp);
  z.ins_after(zp, 200);
  assert(z.length() == 21);
  cout << "z: "; printDlist(z);

  cout << "deleting alternate elements of z...";
  for (zp = z.first(); zp; z.next(zp))
  {
    cout << z(zp) << " ";
    z.del(zp);
  }
  cout << "\n";
  assert(z.length() == 10);
  cout << "z: "; printDlist(z);
  
  cout << "z in reverse order:\n";
  for (zp = z.last(); zp; z.prev(zp)) cout << z(zp) << " ";
  cout << "\n";
  z.clear();
  assert(z.OK());
  assert(z.empty());
  assert(a.OK());
  assert(b.OK());
}

class X
{
public:
  X() { cout << "default ctor called\n"; }
  X(const X&) { cout << "copy ctor called\n"; }
  ~X() { cout << "dtor called\n"; }
};

main()
{
  SLtest();
  DLtest();

  cout << "\n";
  /* Test case from Magnus Nordborg <magnus@mimer.cap.ed.ac.uk> */
  DLList<X> l;
  l.append(X());
  Pix f = l.first();
  l.del(f);

  cout << "\nEnd of test\n";
  return 0;
}

template class SLNode<int>;
template class SLList<int>;
template class DLNode<int>;
template class DLList<int>;

template class DLNode<X>;
template class DLList<X>;
#endif
