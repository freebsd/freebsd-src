
/* Debug macros for development.
   Copyright 1997
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _DEBUGIFY_H_
#define _DEBUGIFY_H_

#include "ansidecl.h"

#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef DEBUGIFY
#include <assert.h>
#ifdef TO_SCREEN
#ifdef _Win32
#define DBG(x) OutputDebugString x
#else
#define DBG(x) printf x
#endif
#elif TO_GDB
#define DBG(x) printf_unfiltered x
#elif TO_POPUP
#ifdef _Win32
#define DBG(x) MessageBox x
#else
#define DBG(x) printf x
#endif
#else /* default: TO_FILE "gdb.log" */
#define DBG(x) printf_dbg x
#endif

#define ASSERT(x) assert(x)

#else /* DEBUGIFY */
#define DBG(x)
#define ASSERT(x)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef REDIRECT
#define printf_unfiltered printf_dbg
#define fputs_unfiltered fputs_dbg
  extern void fputs_dbg PARAMS ((const char *fmt, FILE * fakestream));
#endif /* REDIRECT */

  extern void puts_dbg PARAMS ((const char *fmt));
#ifdef ANSI_PROTOTYPES
  extern void printf_dbg PARAMS ((const char *fmt,...));
#else
  extern void printf_dbg PARAMS ((va_alist va_dcl));
#endif


#ifdef __cplusplus
}
#endif

#endif /* _DEBUGIFY_H_ */
