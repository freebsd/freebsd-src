/* Includes for memory limit warnings.
   Copyright (C) 1990, 1993 Free Software Foundation, Inc.


This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#if defined(__osf__) && (defined(__mips) || defined(mips))
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef __bsdi__
#define BSD4_2
#endif

#ifndef BSD4_2
#ifndef USG
#include <sys/vlimit.h>
#endif /* not USG */
#else /* if BSD4_2 */
#include <sys/time.h>
#include <sys/resource.h>
#endif /* BSD4_2 */

#ifdef emacs
/* The important properties of this type are that 1) it's a pointer, and
   2) arithmetic on it should work as if the size of the object pointed
   to has a size of 1.  */
#ifdef __STDC__
typedef void *POINTER;
#else
typedef char *POINTER;
#endif

typedef unsigned long SIZE;

#ifdef NULL
#undef NULL
#endif
#define NULL ((POINTER) 0)

extern POINTER start_of_data ();
#ifdef DATA_SEG_BITS
#define EXCEEDS_LISP_PTR(ptr) \
  (((unsigned int) (ptr) & ~DATA_SEG_BITS) >> VALBITS)
#else
#define EXCEEDS_LISP_PTR(ptr) ((unsigned int) (ptr) >> VALBITS)
#endif

#ifdef BSD
#ifndef DATA_SEG_BITS
extern char etext;
#define start_of_data() &etext
#endif
#endif

#else  /* Not emacs */ 
extern char etext;
#define start_of_data() &etext
#endif /* Not emacs */

  

/* start of data space; can be changed by calling malloc_init */
static POINTER data_space_start;

/* Number of bytes of writable memory we can expect to be able to get */
static unsigned int lim_data;

#ifdef USG

static void
get_lim_data ()
{
  extern long ulimit ();

  lim_data = -1;

  /* Use the ulimit call, if we seem to have it.  */
#if !defined (ULIMIT_BREAK_VALUE) || defined (LINUX)
  lim_data = ulimit (3, 0);
#endif

  /* If that didn't work, just use the macro's value.  */
#ifdef ULIMIT_BREAK_VALUE
  if (lim_data == -1)
    lim_data = ULIMIT_BREAK_VALUE;
#endif

  lim_data -= (long) data_space_start;
}

#else /* not USG */
#if !defined(BSD4_2) && !defined(__osf__)

static void
get_lim_data ()
{
  lim_data = vlimit (LIM_DATA, -1);
}

#else /* BSD4_2 */

static void
get_lim_data ()
{
  struct rlimit XXrlimit;

  getrlimit (RLIMIT_DATA, &XXrlimit);
#ifdef RLIM_INFINITY
  lim_data = XXrlimit.rlim_cur & RLIM_INFINITY; /* soft limit */
#else
  lim_data = XXrlimit.rlim_cur;	/* soft limit */
#endif
}
#endif /* BSD4_2 */
#endif /* not USG */
