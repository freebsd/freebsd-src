// The -*- C++ -*- standard definitions header.
// This file is part of the GNU ANSI C++ Library.

#ifndef __STDDEF__
#define __STDDEF__

#ifdef __GNUG__
#pragma interface "std/stddef.h"
#endif

#include <_G_config.h>
#include <std/cstddef.h>

extern "C++" {
const size_t NPOS = (size_t)(-1);
typedef void fvoid_t();

#ifndef _WINT_T
#define _WINT_T
typedef _G_wint_t wint_t;
#endif

} // extern "C++"

#endif
