/* rldefs.h -- an attempt to isolate some of the system-specific defines
   for readline.  This should be included after any files that define
   system-specific constants like _POSIX_VERSION or USG. */

/* Copyright (C) 1987,1989 Free Software Foundation, Inc.

   This file contains the Readline Library (the Library), a set of
   routines for providing Emacs style line input to programs that ask
   for it.

   The Library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   The Library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   The GNU General Public License is often shipped with GNU software, and
   is generally kept in a file called COPYING or LICENSE.  If you do not
   have a copy of the license, write to the Free Software Foundation,
   675 Mass Ave, Cambridge, MA 02139, USA. */

#if !defined (_RLDEFS_H)
#define _RLDEFS_H

#if !defined (PRAGMA_ALLOCA)
#  include "memalloc.h"
#endif

#define NEW_TTY_DRIVER
#define HAVE_BSD_SIGNALS
/* #define USE_XON_XOFF */

#if defined (__linux__)
#  include <termcap.h>
#endif /* __linux__ */

/* Some USG machines have BSD signal handling (sigblock, sigsetmask, etc.) */
#if defined (USG) && !defined (hpux)
#  undef HAVE_BSD_SIGNALS
#endif

/* System V machines use termio. */
#if !defined (_POSIX_VERSION)
#  if defined (USG) || defined (hpux) || defined (Xenix) || defined (sgi) || defined (DGUX)
#    undef NEW_TTY_DRIVER
#    define TERMIO_TTY_DRIVER
#    include <termio.h>
#    if !defined (TCOON)
#      define TCOON 1
#    endif
#  endif /* USG || hpux || Xenix || sgi || DUGX */
#endif /* !_POSIX_VERSION */

/* Posix systems use termios and the Posix signal functions. */
#if defined (_POSIX_VERSION)
#  if !defined (TERMIOS_MISSING)
#    undef NEW_TTY_DRIVER
#    define TERMIOS_TTY_DRIVER
#    include <termios.h>
#  endif /* !TERMIOS_MISSING */
#  define HAVE_POSIX_SIGNALS
#  if !defined (O_NDELAY)
#    define O_NDELAY O_NONBLOCK	/* Posix-style non-blocking i/o */
#  endif /* O_NDELAY */
#endif /* _POSIX_VERSION */

/* System V.3 machines have the old 4.1 BSD `reliable' signal interface. */
#if !defined (HAVE_BSD_SIGNALS) && !defined (HAVE_POSIX_SIGNALS)
#  if defined (USGr3) && !defined (XENIX_22)
#    if !defined (HAVE_USG_SIGHOLD)
#      define HAVE_USG_SIGHOLD
#    endif /* !HAVE_USG_SIGHOLD */
#  endif /* USGr3 && !XENIX_22 */
#endif /* !HAVE_BSD_SIGNALS && !HAVE_POSIX_SIGNALS */

/* Other (BSD) machines use sgtty. */
#if defined (NEW_TTY_DRIVER)
#  include <sgtty.h>
#endif

/* Define _POSIX_VDISABLE if we are not using the `new' tty driver and
   it is not already defined.  It is used both to determine if a
   special character is disabled and to disable certain special
   characters.  Posix systems should set to 0, USG systems to -1. */
#if !defined (NEW_TTY_DRIVER) && !defined (_POSIX_VDISABLE)
#  if defined (_POSIX_VERSION)
#    define _POSIX_VDISABLE 0
#  else /* !_POSIX_VERSION */
#    define _POSIX_VDISABLE -1
#  endif /* !_POSIX_VERSION */
#endif /* !NEW_TTY_DRIVER && !_POSIX_VDISABLE */

#if !defined (SHELL) && (defined (_POSIX_VERSION) || defined (USGr3))
#  if !defined (HAVE_DIRENT_H)
#    define HAVE_DIRENT_H
#  endif /* !HAVE_DIRENT_H */
#endif /* !SHELL && (_POSIX_VERSION || USGr3) */

#if defined (HAVE_DIRENT_H)
#  include <dirent.h>
#  if !defined (direct)
#    define direct dirent
#  endif /* !direct */
#  define D_NAMLEN(d) strlen ((d)->d_name)
#else /* !HAVE_DIRENT_H */
#  define D_NAMLEN(d) ((d)->d_namlen)
#  if defined (USG)
#    if defined (Xenix)
#      include <sys/ndir.h>
#    else /* !Xenix (but USG...) */
#      include "ndir.h"
#    endif /* !Xenix */
#  else /* !USG */
#    include <sys/dir.h>
#  endif /* !USG */
#endif /* !HAVE_DIRENT_H */

#if defined (USG) && defined (TIOCGWINSZ) && !defined (Linux)
#  if defined (HAVE_SYS_STREAM_H)
#    include <sys/stream.h>
#  endif /* HAVE_SYS_STREAM_H */
#  if defined (HAVE_SYS_PTEM_H)
#    include <sys/ptem.h>
#  endif /* HAVE_SYS_PTEM_H */
#  if defined (HAVE_SYS_PTE_H)
#    include <sys/pte.h>
#  endif /* HAVE_SYS_PTE_H */
#endif /* USG && TIOCGWINSZ && !Linux */

/* Posix macro to check file in statbuf for directory-ness.
   This requires that <sys/stat.h> be included before this test. */
#if defined (S_IFDIR) && !defined (S_ISDIR)
#  define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif

/* Decide which flavor of the header file describing the C library
   string functions to include and include it. */

#if defined (USG) || defined (NeXT)
#  if !defined (HAVE_STRING_H)
#    define HAVE_STRING_H
#  endif /* !HAVE_STRING_H */
#endif /* USG || NeXT */

#if defined (HAVE_STRING_H)
#  include <string.h>
#else /* !HAVE_STRING_H */
#  include <strings.h>
#endif /* !HAVE_STRING_H */

#if !defined (strchr) && !defined (__STDC__)
extern char *strchr (), *strrchr ();
#endif /* !strchr && !__STDC__ */

#if defined (HAVE_VARARGS_H)
#  include <varargs.h>
#endif /* HAVE_VARARGS_H */

#if !defined (emacs_mode)
#  define no_mode -1
#  define vi_mode 0
#  define emacs_mode 1
#endif

/* If you cast map[key].function to type (Keymap) on a Cray,
   the compiler takes the value of map[key].function and
   divides it by 4 to convert between pointer types (pointers
   to functions and pointers to structs are different sizes).
   This is not what is wanted. */
#if defined (CRAY)
#  define FUNCTION_TO_KEYMAP(map, key)	(Keymap)((int)map[key].function)
#  define KEYMAP_TO_FUNCTION(data)	(Function *)((int)(data))
#else
#  define FUNCTION_TO_KEYMAP(map, key)	(Keymap)(map[key].function)
#  define KEYMAP_TO_FUNCTION(data)	(Function *)(data)
#endif

/* Possible values for _rl_bell_preference. */
#define NO_BELL 0
#define AUDIBLE_BELL 1
#define VISIBLE_BELL 2

/* CONFIGURATION SECTION */
#include "rlconf.h"

#endif /* !_RLDEFS_H */
