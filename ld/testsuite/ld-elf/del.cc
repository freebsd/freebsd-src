#include <new>

extern "C" void free (void *);

void
operator delete (void *ptr, const std::nothrow_t&) throw ()
{
  if (ptr)
    free (ptr);
}

void
operator delete (void *ptr) throw ()
{
  if (ptr)
    free (ptr);
}

void
operator delete[] (void *ptr) throw ()
{
  ::operator delete (ptr);
}

void
operator delete[] (void *ptr, const std::nothrow_t&) throw ()
{
  ::operator delete (ptr);
}
