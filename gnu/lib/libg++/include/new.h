#ifndef _new_h
#ifdef __GNUG__
#pragma interface
#endif
#define _new_h 1

#include <defines.h>

#ifndef NO_LIBGXX_MALLOC
#define MALLOC_ALIGN_MASK   7 /* ptrs aligned at 8 byte boundaries */
#define MALLOC_MIN_OVERHEAD 8 /* 8 bytes of overhead per pointer */
#endif

extern "C" fvoid_t *set_new_handler(fvoid_t *);

#ifdef __GNUG__
extern fvoid_t *__new_handler;
extern "C" void __default_new_handler();

#define NEW(where) new ( where )
#endif

// default placement version of operator new
inline void *operator new(size_t, void *place) { return place; }
inline void *operator new[](size_t, void *place) { return place; }

#endif
