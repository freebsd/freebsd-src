#include <stdio.h>
#include <stdlib.h>
#include <new>

int pass = 0;

void *
operator new (size_t sz, const std::nothrow_t&) throw ()
{
  void *p;
  pass++;
  p = malloc(sz);
  return p;
}

void *
operator new (size_t sz) throw (std::bad_alloc)
{
  void *p;
  pass++;
  p = malloc(sz);
  return p;
}

void
operator delete (void *ptr) throw ()
{
  pass++;
  if (ptr)
    free (ptr);
}

class A 
{
public:
  A() {}
  ~A() { }
  int a;
  int b;
};


int
main (void)
{
  A *bb = new A[10];
  delete [] bb;
  bb = new (std::nothrow) A [10];
  delete [] bb;

  if (pass == 4)
    {
      printf ("PASS\n");
      return 0;
    }
  else
    {
      printf ("FAIL\n");
      return 1;
    }
}
