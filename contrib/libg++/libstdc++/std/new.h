// The -*- C++ -*- dynamic memory management header.
// Copyright (C) 1994 Free Software Foundation

#ifndef __NEW__
#define __NEW__

#ifdef __GNUG__
#pragma interface "std/new.h"
#endif

#include <std/cstddef.h>

extern "C++" {
typedef void (*new_handler)();
extern "C" new_handler set_new_handler (new_handler);

#if defined(__GNUG__) && !defined (__STRICT_ANSI__)
// G++ implementation internals
extern new_handler __new_handler;
extern "C" void __default_new_handler (void);
#endif

// replaceable signatures
void *operator new (size_t);
void *operator new[] (size_t);
void operator delete (void *);
void operator delete[] (void *);

// default placement versions of operator new
inline void *operator new(size_t, void *place) { return place; }
inline void *operator new[](size_t, void *place) { return place; }
} // extern "C++"

#endif
