#ifdef __GNUG__
#pragma implementation
#endif
#include "<T>.Deque.h"

<T>Deque::~<T>Deque() {}

void <T>Deque::error(const char* msg)
{
  (*lib_error_handler)("Deque", msg);
}
