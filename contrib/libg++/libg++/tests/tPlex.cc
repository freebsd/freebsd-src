/*
  test of Plexes
*/

#include <stream.h>
#include <assert.h>
#include "iPlex.h"

#define tassert(ex) {if ((ex)) cerr << #ex << "\n"; \
                       else _assert(#ex, __FILE__,__LINE__); }


void printplex(intPlex& a)
{
  cout << "[";
  int maxprint = 20;
  int k = 0;
  int i;
  for (i = a.low(); i < a.fence() && k < maxprint; ++i, ++k)
    cout << a[i] << " ";
  if (i < a.fence()) cout << "]\n";
  else cout << "...]\n";
}

#include "iFPlex.h"

void FPtest () 
{
  intFPlex p(50);
  assert(p.OK());
  assert (p.empty());

  p.add_high (1000);
  Pix px = p.index_to_Pix(0);
  assert (p.length() == 1);
  assert(p.owns(px));
  assert(p(px) == 1000);
  assert(p.Pix_to_index(px) == 0);

  p.reset_low (10);
  assert (p.length() == 1);
  assert (p.low() == 10);
  assert (p[10] == 1000);
  assert(p(px) == 1000);
  assert(p.Pix_to_index(px) == 10);
  assert(p.index_to_Pix(10) == px);
  assert(p.OK());

  int h = p.high_element();
  int l = p.low_element();
  assert ( (h==l) && (h==1000));
  p.fill(222);
  assert(p(px) == 222);

  p.del_high();   
  assert(p.empty());
  assert(!p.owns(px));

  intFPlex q(10, -50);
  q.add_low (-1000);
  assert (q[9] == -1000);
  q[9] = 21;
  assert(!q.valid(10));
  assert(q.OK());

  q.del_low();
  assert (q.empty());

  p.reset_low (0);
  q.reset_low (0);

  int i;
  for (i = 0; i < 50; i++) 
  {
    if (i % 2 == 0) 
    { 
      p.add_high (i);
      assert (p.high() == i/2);
      p[i/2] = p[i/4];
      assert (p.high_element() == i/4);
      p[i/2] = i/2;
    } 
    else 
    {
      q.add_low (-i);
      int ii = - (i/2) -1;
      assert (q.low() == ii);
      q.low_element() = ii;
      assert (q[ii] == ii);
    }
  }

  cout << "q:"; printplex(q);
  assert (p.length() == 25);
  assert (q.length() == 25);

  assert(p.valid(0));
  assert(p.owns(px));
  
  px = p.first();
  i = 0;
  for (int it1 = p.low(); it1 < p.fence(); p.next(it1))
  {
    assert (p[it1] == it1);
    assert(p(px) == p[it1]);
    p.next(px);
    ++i;
  }
  assert(px == 0);
  px = q.last();
  for (int it1 = q.high(); it1 > q.ecnef(); q.prev(it1))
  {
    assert (q[it1] == it1);
    assert(p(px) == q[it1]);
    q.prev(px);
    ++i;
  }
  assert(i == 50);
  q.reset_low (0);  
  assert (p.high() == q.high());
  assert(p.low() == 0);
  
  intFPlex p1 = p;
  intFPlex p2 (p);
  intFPlex p3 = p2;
  assert (p1.length() == 25);
  assert (p1.high() == 24);
  assert(p1.low() == 0);
  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());

  i = 0;
  for (int it5 = p.low(); it5 < p.fence(); p.next(it5)) 
  {
    assert(p1.low() == it5);
    p1.del_low();
    assert(!p1.valid(it5));
    p2.del_high ();
    p3 [it5] = -it5;
    ++i;
  }
  assert(i == 25);

  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());
  assert (p1.empty());
  assert (p2.empty());

  p3.append (p);
  assert(p3.OK());

  p1.prepend (p);
  p2.append (p);
  assert(p1.length() == p.length());
  assert(p1.length() == p2.length());
  assert(p1.OK());
  assert(p2.OK());

  p2.clear();
  assert(p2.OK());
  assert(p2.empty());
  p2 = p1;
  assert(p2.OK());

  p1 = p;
  assert(p1.OK());
  p1.reset_low (p1.low_element());
  for (int it6 = p1.low(); it6 < p1.fence(); it6++)
  {
    assert (p1[it6] == it6);
  }
  p1[13] = 1313;
  p1[7] = -7777;
  p1[24] = 24242424;
  assert(!p1.valid(25));
  assert(!p1.valid(-1));
  assert(p1.OK());

}

#include "iXPlex.h"

void XPtest () 
{
  intXPlex p(3);
  assert(p.OK());
  assert (p.empty());

  p.add_high (1000);
  Pix px = p.index_to_Pix(0);
  assert(p.Pix_to_index(px) == 0);
  assert (p.length() == 1);
  assert(p.owns(px));
  assert(p(px) == 1000);

  p.reset_low(10);
  assert (p.length() == 1);
  assert (p.low() == 10);
  assert (p[10] == 1000);
  assert(p(px) == 1000);
  assert(p.Pix_to_index(px) == 10);
  assert(p.index_to_Pix(10) == px);
  assert(p.OK());

  int h = p.high_element();
  int l = p.low_element();
  assert ( (h==l) && (h==1000));
  p.fill(222);
  assert(p(px) == 222);

  p.del_high();   
  assert(p.empty());
  assert(!p.owns(px));

  p.add_low(-1000);
  assert (p[9] == -1000);
  p[9] = 21;
  assert(!p.valid(10));
  assert(p.OK());

  p.del_low();
  assert (p.empty());
  p.reset_low (0);

  int i;
  for (i = 0; i < 50; i++) 
  {
    if (i % 2 == 0) 
    { 
      p.add_high (i);
      assert (p.high() == i/2);
      p[i/2] = p[i/4];
      assert (p.high_element() == i/4);
      p[i/2] = i/2;
    } 
    else 
    {
      p.add_low (-i);
      int ii = - (i/2) -1;
      assert (p.low() == ii);
      p.low_element() = ii;
      assert (p[ii] == ii);
    }
  }

  assert (p.length() == 50);
  cout << "p:"; printplex(p);

  assert(p.valid(0));
  assert(p.owns(px));

  px = p.first();
  i = 0;
  for (int it1 = p.low(); it1 < p.fence(); p.next(it1))
  {
    assert (p[it1] == it1);
    assert(p(px) == p[it1]);
    p.next(px);
    ++i;
  }
  assert(i == 50);
  assert(px == 0);
  p.reset_low (0);  
  assert (p.high() == 49);
  assert(p.low() == 0);
  
  i = 0;
  for (int it2 = p.high(); it2 > p.ecnef(); p.prev(it2))
  {
    assert ( p[it2] == it2-25 );
    ++i;
  }
  assert(i == 50);
  assert(p.OK());
  
  intXPlex p1 = p;
  intXPlex p2 (p);
  intXPlex p3 = p2;
  assert (p1.length() == 50);
  assert (p1.high() == 49);
  assert(p1.low() == 0);
  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());

  i = 0;
  for (int it5 = p.low(); it5 < p.fence(); p.next(it5)) 
  {
    assert(p1.low() == it5);
    p1.del_low();
    assert(!p1.valid(it5));
    p2.del_high ();
    p3 [it5] = -it5;
    ++i;
  }
  assert(i == 50);

  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());
  assert (p1.empty());
  assert (p2.empty());

  p3.append (p);
  assert(p3.OK());

  p1.prepend (p);
  p2.append (p);
  assert(p1.length() == p.length());
  assert(p1.length() == p2.length());
  assert(p1.OK());
  assert(p2.OK());

  p2.clear();
  assert(p2.OK());
  assert(p2.empty());
  p2 = p1;
  assert(p2.OK());


  p1 = p;
  assert(p1.OK());
  p1.reset_low(p1.low_element());
  p1 [13] = 1313;
  p1 [-7] = -7777;
  p1 [-25] = -252525;
  p1 [24] = 24242424;
  assert(!p1.valid(25));
  assert(!p1.valid(-26));
  assert(p1.OK());

  p1 = p;
  p1.reset_low (p1.low_element());
  for (int it6 = p1.low(); it6 < p1.fence(); it6++)
  {
    assert (p1[it6] == it6);
  }
  p1.reverse();
  i = p1.high();
  for (int it6 = p1.low(); it6 < p1.fence(); it6++)
  {
    assert (p1[it6] == i);
    --i;
  }
  assert(p1.OK());

}

#include "iMPlex.h"

void MPtest () 
{
  intMPlex p(3);
  assert(p.OK());
  assert (p.empty());

  p.add_high (1000);
  Pix px = p.index_to_Pix(0);
  assert (p.length() == 1);
  assert(p.owns(px));
  assert(p(px) == 1000);
  assert(p.Pix_to_index(px) == 0);

  p.reset_low (10);
  assert (p.length() == 1);
  assert (p.low() == 10);
  assert (p[10] == 1000);
  assert(p(px) == 1000);
  assert(p.Pix_to_index(px) == 10);
  assert(p.index_to_Pix(10) == px);
  assert(p.OK());

  int h = p.high_element();
  int l = p.low_element();
  assert ( (h==l) && (h==1000));
  p.fill(222);
  assert(p(px) == 222);

  p.del_high();   
  assert(p.empty());
  assert(!p.owns(px));

  p.add_low (-1000);
  assert (p[9] == -1000);
  p[9] = 21;
  assert(!p.valid(10));
  assert(p.OK());

  p.del_low();
  assert (p.empty());
  p.reset_low (0);

  int i;
  for (i = 0; i < 50; i++) 
  {
    if (i % 2 == 0) 
    { 
      p.add_high (i);
      assert (p.high() == i/2);
      p[i/2] = p[i/4];
      assert (p.high_element() == i/4);
      p[i/2] = i/2;
    } 
    else 
    {
      p.add_low (-i);
      int ii = - (i/2) -1;
      assert (p.low() == ii);
      p.low_element() = ii;
      assert (p[ii] == ii);
    }
  }

  cout << "p:"; printplex(p);
  assert (p.length() == 50);
  assert (p.count() == 50);

  assert(p.available() == 0);
  assert(p.valid(0));
  px = &p[0];
  assert(p.owns(px));
  p.del_index(0);
  assert(p.count() == 49);
  assert(p.available() == 1);
  assert(p.unused_index() == 0);
  assert(!p.valid(0));
  assert(!p.owns(px));
  p.undel_index(0);
  p[0] = 0;
  assert(p.count() == 50);
  assert(p.available() == 0);
  assert(p.valid(0));
  assert(p.owns(px));
  assert(p.OK());

  p.del_index(0);
  
  px = p.first();
  i = 0;
  for (int it1 = p.low(); it1 < p.fence(); p.next(it1))
  {
    assert (p[it1] == it1);
    assert(p(px) == p[it1]);
    p.next(px);
    ++i;
  }
  assert(i == 49);
  assert(px == 0);
  p.reset_low (0);  
  assert (p.high() == 49);
  assert(p.low() == 0);
  
  i = 0;
  for (int it2 = p.high(); it2 > p.ecnef();  p.prev(it2))
  {
    assert ( p[it2] == it2-25 );
    assert(px != &p[it2]);
    ++i;
  }
  assert(i == 49);
  assert(p.OK());
  
  p.del_index(1);
  p.del_index(2);
  assert (p.OK());
  assert (p.count() == 47);

  intMPlex p1 = p;
  intMPlex p2 (p);
  intMPlex p3 = p2;
  assert (p1.length() == 50);
  assert (p1.count() == 47);
  assert (p1.high() == 49);
  assert(p1.low() == 0);
  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());

  i = 0;
  for (int it5 = p.low(); it5 < p.fence(); p.next(it5)) 
  {
    assert(p1.low() == it5);
    p1.del_low();
    assert(!p1.valid(it5));
    p2.del_high ();
    p3 [it5] = -it5;
    ++i;
  }
  assert(i == 47);

  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());
  assert (p1.empty());
  assert (p2.empty());

  p3.append (p);
  assert(p3.OK());

  p1.prepend (p);
  p2.append (p);
  assert(p1.count() == p.count());
  assert(p1.count() == p2.count());
  assert(p1.OK());
  assert(p2.OK());

  p2.clear();
  assert(p2.OK());
  assert(p2.empty());
  p2 = p1;
  assert(p2.OK());

  p1 = p;
  p1.del_index(3);
  p1.del_index(4);
  p1.del_index(5);
  p1.del_index(6);
  p1.del_index(7);
  p1.del_index(8);
  p1.undel_index(6);
  p1[6] = 6666;
  p1[9] = 9999;
  p1[0] = 0;
  assert(p1[6] == 6666);
  assert(!p1.valid(5));
  assert(!p1.valid(7));
  p1.del_low();
  assert(p1.low() == 6);
  assert(p1.OK());

}

#include "iRPlex.h"

void RPtest () 
{
  intRPlex p(3);
  assert(p.OK());
  assert (p.empty());

  p.add_high (1000);
  Pix px = p.index_to_Pix(0);
  assert(p.Pix_to_index(px) == 0);
  assert (p.length() == 1);
  assert(p.owns(px));
  assert(p(px) == 1000);

  p.reset_low(10);
  assert (p.length() == 1);
  assert (p.low() == 10);
  assert (p[10] == 1000);
  assert(p(px) == 1000);
  assert(p.Pix_to_index(px) == 10);
  assert(p.index_to_Pix(10) == px);
  assert(p.OK());

  int h = p.high_element();
  int l = p.low_element();
  assert ( (h==l) && (h==1000));
  p.fill(222);
  assert(p(px) == 222);

  p.del_high();   
  assert(p.empty());
  assert(!p.owns(px));

  p.add_low(-1000);
  assert (p[9] == -1000);
  p[9] = 21;
  assert(!p.valid(10));
  assert(p.OK());

  p.del_low();
  assert (p.empty());
  p.reset_low (0);

  int i;
  for (i = 0; i < 50; i++) 
  {
    if (i % 2 == 0) 
    { 
      p.add_high (i);
      assert (p.high() == i/2);
      p[i/2] = p[i/4];
      assert (p.high_element() == i/4);
      p[i/2] = i/2;
    } 
    else 
    {
      p.add_low (-i);
      int ii = - (i/2) -1;
      assert (p.low() == ii);
      p.low_element() = ii;
      assert (p[ii] == ii);
    }
  }

  assert (p.length() == 50);
  cout << "p:"; printplex(p);

  assert(p.valid(0));
  assert(p.owns(px));

  px = p.first();
  i = 0;
  for (int it1 = p.low(); it1 < p.fence(); p.next(it1))
  {
    assert (p[it1] == it1);
    assert(p(px) == p[it1]);
    p.next(px);
    ++i;
  }
  assert(i == 50);
  assert(px == 0);
  p.reset_low (0);  
  assert (p.high() == 49);
  assert(p.low() == 0);
  
  i = 0;
  for (int it2 = p.high(); it2 > p.ecnef(); p.prev(it2))
  {
    assert ( p[it2] == it2-25 );
    ++i;
  }
  assert(i == 50);
  assert(p.OK());
  
  intRPlex p1 = p;
  intRPlex p2 (p);
  intRPlex p3 = p2;
  assert (p1.length() == 50);
  assert (p1.high() == 49);
  assert(p1.low() == 0);
  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());

  i = 0;
  for (int it5 = p.low(); it5 < p.fence(); p.next(it5)) 
  {
    assert(p1.low() == it5);
    p1.del_low();
    assert(!p1.valid(it5));
    p2.del_high ();
    p3 [it5] = -it5;
    ++i;
  }
  assert(i == 50);

  assert(p.OK());
  assert(p1.OK());
  assert(p2.OK());
  assert(p3.OK());
  assert (p1.empty());
  assert (p2.empty());

  p3.append (p);
  assert(p3.OK());

  p1.prepend (p);
  p2.append (p);
  assert(p1.length() == p.length());
  assert(p1.length() == p2.length());
  assert(p1.OK());
  assert(p2.OK());

  p2.clear();
  assert(p2.OK());
  assert(p2.empty());
  p2 = p1;
  assert(p2.OK());


  p1 = p;
  assert(p1.OK());
  p1.reset_low(p1.low_element());
  p1 [13] = 1313;
  p1 [-7] = -7777;
  p1 [-25] = -252525;
  p1 [24] = 24242424;
  assert(!p1.valid(25));
  assert(!p1.valid(-26));
  assert(p1.OK());

  p1 = p;
  p1.reset_low (p1.low_element());
  for (int it6 = p1.low(); it6 < p1.fence(); it6++)
  {
    assert (p1[it6] == it6);
  }
  p1.reverse();
  i = p1.high();
  for (int it6 = p1.low(); it6 < p1.fence(); it6++)
  {
    assert (p1[it6] == i);
    --i;
  }
  assert(p1.OK());

}


main()
{
  cout << "FPtest\n"; FPtest();
  cout << "XPtest\n"; XPtest();
  cout << "MPtest\n"; MPtest();
  cout << "RPtest\n"; RPtest();
  cout << "\nend of tests\n";
}
