
#ifndef _memory_h
#define _memory_h 1

#include "_G_config.h"
#include <stddef.h>

extern "C" {

void*     memalign _G_ARGS((_G_size_t, _G_size_t));
void*     memccpy _G_ARGS((void*, const void*, int, _G_size_t));
void*     memchr _G_ARGS((const void*, int, _G_size_t));
int       memcmp _G_ARGS((const void*, const void*, _G_size_t));
void*     memcpy _G_ARGS((void*, const void*, _G_size_t));
void*     memmove _G_ARGS((void*, const void*, _G_size_t));
void*     memset _G_ARGS((void*, int, _G_size_t));
int       ffs _G_ARGS((int));
#if defined(__OSF1__) || defined(__386BSD__)
int	  getpagesize _G_ARGS((void));
#else
_G_size_t    getpagesize _G_ARGS((void));
#endif
void*     valloc _G_ARGS((_G_size_t));

void      bcopy _G_ARGS((const void*, void*, _G_size_t));
int       bcmp _G_ARGS((const void*, const void*, int));
void      bzero _G_ARGS((void*, int));
}

#ifdef __GNUG__
#ifndef alloca
#define alloca(x)  __builtin_alloca(x)
#endif
#else
#ifndef IV
extern "C" void* alloca(_G_size_t);
#else
extern "C" void* alloca(unsigned long);
#endif /* IV */
#endif

#endif
