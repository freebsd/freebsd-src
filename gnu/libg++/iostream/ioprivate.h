//    This is part of the iostream library, providing -*- C++ -*- input/output.
//    Copyright (C) 1991 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "streambuf.h"
#include <stdarg.h>
#include <stddef.h>

#define _fstat(x, y) fstat(x,y)
#define _isatty(fd) isatty(fd)

extern int __cvt_double(double number, register int prec, int flags,
			int *signp, int fmtch, char *startp, char *endp);

/*#define USE_MALLOC_BUF*/

#ifndef USE_MALLOC_BUF
#define ALLOC_BUF(size) new char[size]
#define FREE_BUF(ptr) delete [] (ptr)
#else
#define ALLOC_BUF(size) (char*)malloc(size)
#define FREE_BUF(ptr) free(ptr)
#endif

#define USE_DTOA

// Advantages:
// - Input gets closest value
// - Output emits string that when read yields identical value.
// - Handles Infinity and NaNs (but not re-readable).
// Disadvantages of dtoa:
// - May not work for all double formats.
// - Big chunck of code.
// - Not reentrant - uses atatic variables freelist,
//   result, result_k in dtoa
//   (plus initializes p5s, HOWORD, and LOWORD).

#ifdef USE_DTOA
extern "C" double _Xstrtod(const char *s00, char **se);
#define strtod(s, e) _Xstrtod(s, e)
extern "C" char *dtoa(double d, int mode, int ndigits,
                        int *decpt, int *sign, char **rve);
extern int __outfloat(double value, streambuf *sb, char mode,
	       int width, int precision, __fmtflags flags,
	       char sign_mode, char fill);
#endif

