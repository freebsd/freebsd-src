#ifndef _new_h
#ifdef __GNUG__
#pragma interface
#endif
#define _new_h 1

#include <stddef.h>
#include <std.h>

#ifndef NO_LIBGXX_MALLOC
#define MALLOC_ALIGN_MASK   7 /* ptrs aligned at 8 byte boundaries */
#define MALLOC_MIN_OVERHEAD 8 /* 8 bytes of overhead per pointer */
#endif

typedef void (*new_handler_t)();
extern new_handler_t __new_handler;
extern "C" void default_new_handler();
extern "C" new_handler_t set_new_handler(new_handler_t);

#ifdef __GNUG__
#define NEW(where) new { where }
#endif

// default placement version of operator new
static inline void *operator new(size_t, void *place) { return place; }

// provide a C++ interface to vector-resize via realloc 
inline void *operator new(size_t size, void *ptr, size_t new_len)
{
  return realloc(ptr, new_len * size);
}

#endif
