#ifdef __GNUG__
#pragma implementation
#endif
#include "<T>.Stack.h"

<T>Stack::~<T>Stack() {}

void <T>Stack::error(const char* msg)
{
  (*lib_error_handler)("Stack", msg);
}
