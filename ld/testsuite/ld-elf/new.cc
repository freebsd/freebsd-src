#include <new>
#include <exception_defines.h>

using std::bad_alloc;

extern "C" void *malloc (std::size_t);
extern "C" void abort (void);

void *
operator new (std::size_t sz, const std::nothrow_t&) throw()
{
  void *p;

  /* malloc (0) is unpredictable; avoid it.  */
  if (sz == 0)
    sz = 1;
  p = (void *) malloc (sz);
  return p;
}

void *
operator new (std::size_t sz) throw (std::bad_alloc)
{
  void *p;

  /* malloc (0) is unpredictable; avoid it.  */
  if (sz == 0)
    sz = 1;
  p = (void *) malloc (sz);
  while (p == 0)
    {
      ::abort();
    }

  return p;
}

void*
operator new[] (std::size_t sz) throw (std::bad_alloc)
{
  return ::operator new(sz);
}

void *
operator new[] (std::size_t sz, const std::nothrow_t& nothrow) throw()
{
  return ::operator new(sz, nothrow);
}
