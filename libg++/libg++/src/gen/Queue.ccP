#ifdef __GNUG__
#pragma implementation
#endif
#include "<T>.Queue.h"

<T>Queue::~<T>Queue() {}


// error handling

void <T>Queue::error(const char* msg)
{
  (*lib_error_handler)("Queue", msg);
}
