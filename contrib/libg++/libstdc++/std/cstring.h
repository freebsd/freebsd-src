// The -*- C++ -*- null-terminated string header.
// This file is part of the GNU ANSI C++ Library.

#ifndef __CSTRING__
#define __CSTRING__

#if 0 // Let's not bother with this just yet.
// The ANSI C prototypes for these functions have a const argument type and
// non-const return type, so we can't use them.

#define strchr  __hide_strchr
#define strpbrk __hide_strpbrk
#define strrchr __hide_strrchr
#define strstr  __hide_strstr
#define memchr  __hide_memchr
#endif // 0

#include <string.h>

#if 0 // Let's not bother with this just yet.
#undef strchr
#undef strpbrk
#undef strrchr
#undef strstr
#undef memchr

#include <std/cstddef.h>

#ifdef __GNUG__
#pragma interface "std/cstring.h"
#endif

extern "C++" {
extern "C" const char *strchr (const char *, int);
inline char *
strchr (char *s, int c)
{
  return const_cast<char *> (strchr (static_cast<const char *> (s), c));
}

extern "C" const char *strpbrk (const char *, const char *);
inline char *
strpbrk (char *s1, const char *s2)
{
  return const_cast<char *> (strpbrk (static_cast<const char *> (s1), s2));
}

extern "C" const char *strrchr (const char *, int);
inline char *
strrchr (char *s, int c)
{
  return const_cast<char *> (strrchr (static_cast<const char *> (s), c));
}

extern "C" const char *strstr (const char *, const char *);
inline char *
strstr (char *s1, const char *s2)
{
  return const_cast<char *> (strstr (static_cast<const char *> (s1), s2));
}

extern "C" const void *memchr (const void *, int, size_t);
inline void *
memchr (void *s, int c, size_t n)
{
  return const_cast<void *> (memchr (static_cast<const void *> (s), c, n));
}
} // extern "C++"

#endif // 0
#endif // !defined (__CSTRING__)
