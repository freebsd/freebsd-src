/* uucp.h
   Header file for the UUCP package.

   Copyright (C) 1991, 1992, 1993, 1994, 1995 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

/* Get the system configuration parameters.  */
#include "config.h"
#include "policy.h"

/* Get a definition for ANSI_C if we weren't given one.  */
#ifndef ANSI_C
#ifdef __STDC__
#define ANSI_C 1
#else /* ! defined (__STDC__) */
#define ANSI_C 0
#endif /* ! defined (__STDC__) */
#endif /* ! defined (ANSI_C) */

/* Pass this definition into uuconf.h.  */
#define UUCONF_ANSI_C ANSI_C

/* We always include some standard header files.  We need <signal.h>
   to define sig_atomic_t.  */
#include <stdio.h>
#include <signal.h>
#if HAVE_STDDEF_H
#include <stddef.h>
#endif

/* On some systems we need <sys/types.h> to get sig_atomic_t or
   size_t or time_t.  */
#if ! HAVE_SIG_ATOMIC_T_IN_SIGNAL_H && HAVE_SIG_ATOMIC_T_IN_TYPES_H
#define USE_TYPES_H 1
#else
#if ! HAVE_SIZE_T_IN_STDDEF_H && HAVE_SIZE_T_IN_TYPES_H
#define USE_TYPES_H 1
#else
#if ! HAVE_TIME_T_IN_TIME_H && HAVE_TIME_T_IN_TYPES_H
#define USE_TYPES_H 1
#endif
#endif
#endif

#ifndef USE_TYPES_H
#define USE_TYPES_H 0
#endif

#if USE_TYPES_H
#include <sys/types.h>
#endif

/* Make sure we have sig_atomic_t.  */
#if ! HAVE_SIG_ATOMIC_T_IN_SIGNAL_H && ! HAVE_SIG_ATOMIC_T_IN_TYPES_H
#ifndef SIG_ATOMIC_T
/* There is no portable definition for sig_atomic_t.  */
#define SIG_ATOMIC_T char
#endif /* ! defined (SIG_ATOMIC_T) */
typedef SIG_ATOMIC_T sig_atomic_t;
#endif /* ! HAVE_SIG_ATOMIC_T_IN_SIGNAL_H && ! HAVE_SIG_ATOMIC_T_IN_TYPES_H */

/* Make sure we have size_t.  */
#if ! HAVE_SIZE_T_IN_STDDEF_H && ! HAVE_SIZE_T_IN_TYPES_H
#ifndef SIZE_T
#define SIZE_T unsigned
#endif /* ! defined (SIZE_T) */
typedef SIZE_T size_t;
#endif /* ! HAVE_SIZE_T_IN_STDDEF_H && ! HAVE_SIZE_T_IN_TYPES_H */

/* Make sure we have time_t.  We use long as the default.  We don't
   bother to let conf.h override this, since on a system which doesn't
   define time_t long must be correct.  */
#if ! HAVE_TIME_T_IN_TIME_H && ! HAVE_TIME_T_IN_TYPES_H
typedef long time_t;
#endif

/* Set up some definitions for both ANSI C and Classic C.

   P() -- for function prototypes (e.g. extern int foo P((int)) ).
   pointer -- for a generic pointer (i.e. void *).
   constpointer -- for a generic pointer to constant data.
   BUCHAR -- to convert a character to unsigned.  */
#if ANSI_C
#if ! HAVE_VOID || ! HAVE_UNSIGNED_CHAR || ! HAVE_PROTOTYPES
 #error ANSI C compiler without void or unsigned char or prototypes
#endif
#define P(x) x
typedef void *pointer;
typedef const void *constpointer;
#define BUCHAR(b) ((unsigned char) (b))
#else /* ! ANSI_C */
/* Handle uses of volatile and void in Classic C.  */
#define volatile
#if ! HAVE_VOID
#define void int
#endif
#if HAVE_PROTOTYPES
#define P(x) x
#else
#define P(x) ()
#endif
typedef char *pointer;
typedef const char *constpointer;
#if HAVE_UNSIGNED_CHAR
#define BUCHAR(b) ((unsigned char) (b))
#else /* ! HAVE_UNSIGNED_CHAR */
/* This should work on most systems, but not necessarily all.  */
#define BUCHAR(b) ((b) & 0xff)
#endif /* ! HAVE_UNSIGNED_CHAR */
#endif /* ! ANSI_C */

/* Make sure we have a definition for offsetof.  */
#ifndef offsetof
#define offsetof(type, field) \
  ((size_t) ((char *) &(((type *) 0)->field) - (char *) (type *) 0))
#endif

/* Only use inline with gcc.  */
#ifndef __GNUC__
#define __inline__
#endif

/* Get the string functions, which are used throughout the code.  */
#if HAVE_MEMORY_H
#include <memory.h>
#else
/* We really need a definition for memchr, and this should not
   conflict with anything in <string.h>.  I hope.  */
extern pointer memchr ();
#endif

#if HAVE_STRING_H
#include <string.h>
#else /* ! HAVE_STRING_H */
#if HAVE_STRINGS_H
#include <strings.h>
#else /* ! HAVE_STRINGS_H */
extern char *strcpy (), *strncpy (), *strchr (), *strrchr (), *strtok ();
extern char *strcat (), *strerror (), *strstr ();
extern size_t strlen (), strspn (), strcspn ();
#if ! HAVE_MEMORY_H
extern pointer memcpy (), memchr ();
#endif /* ! HAVE_MEMORY_H */
#endif /* ! HAVE_STRINGS_H */
#endif /* ! HAVE_STRING_H */

/* Get what we need from <stdlib.h>.  */
#if HAVE_STDLIB_H
#include <stdlib.h>
#else /* ! HAVE_STDLIB_H */
extern pointer malloc (), realloc (), bsearch ();
extern long strtol ();
extern unsigned long strtoul ();
extern char *getenv ();
#endif /* ! HAVE_STDLIB_H */

/* NeXT uses <libc.h> to declare a bunch of functions.  */
#if HAVE_LIBC_H
#include <libc.h>
#endif

/* Make sure we have the EXIT_ macros.  */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS (0)
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (1)
#endif

/* If we need to declare errno, do so.  I don't want to always do
   this, because some system might theoretically have a different
   declaration for errno.  On a POSIX system this is sure to work.  */
#if ! HAVE_ERRNO_DECLARATION
extern int errno;
#endif

/* If the system has the socket call, guess that we can compile the
   TCP code.  */
#define HAVE_TCP HAVE_SOCKET

/* If the system has the t_open call, guess that we can compile the
   TLI code.  */
#define HAVE_TLI HAVE_T_OPEN

/* The boolean type holds boolean values.  */
typedef int boolean;
#undef TRUE
#undef FALSE
#define TRUE (1)
#define FALSE (0)

/* The openfile_t type holds an open file.  This depends on whether we
   are using stdio or not.  */
#if USE_STDIO

typedef FILE *openfile_t;
#define EFILECLOSED ((FILE *) NULL)
#define ffileisopen(e) ((e) != NULL)
#define ffileeof(e) feof (e)
#define cfileread(e, z, c) fread ((z), 1, (c), (e))
#define cfilewrite(e, z, c) fwrite ((z), 1, (c), (e))
#define ffileioerror(e, c) ferror (e)
#ifdef SEEK_SET
#define ffileseek(e, i) (fseek ((e), (long) (i), SEEK_SET) == 0)
#define ffilerewind(e) (fseek ((e), (long) 0, SEEK_SET) == 0)
#else
#define ffileseek(e, i) (fseek ((e), (long) (i), 0) == 0)
#define ffilerewind(e) (fseek ((e), (long) 0, 0) == 0)
#endif
#ifdef SEEK_END
#define ffileseekend(e) (fseek ((e), (long) 0, SEEK_END) == 0)
#else
#define ffileseekend(e) (fseek ((e), (long) 0, 2) == 0)
#endif
#define ffileclose(e) (fclose (e) == 0)

#define fstdiosync(e, z) (fsysdep_sync (e, z))

#else /* ! USE_STDIO */

#if ! USE_TYPES_H
#undef USE_TYPES_H
#define USE_TYPES_H 1
#include <sys/types.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef OFF_T
typedef OFF_T off_t;
#undef OFF_T
#endif

typedef int openfile_t;
#define EFILECLOSED (-1)
#define ffileisopen(e) ((e) >= 0)
#define ffileeof(e) (FALSE)
#define cfileread(e, z, c) read ((e), (z), (c))
#define cfilewrite(e, z, c) write ((e), (z), (c))
#define ffileioerror(e, c) ((c) < 0)
#ifdef SEEK_SET
#define ffileseek(e, i) (lseek ((e), (off_t) i, SEEK_SET) >= 0)
#define ffilerewind(e) (lseek ((e), (off_t) 0, SEEK_SET) >= 0)
#else
#define ffileseek(e, i) (lseek ((e), (off_t) i, 0) >= 0)
#define ffilerewind(e) (lseek ((e), (off_t) 0, 0) >= 0)
#endif
#ifdef SEEK_END
#define ffileseekend(e) (lseek ((e), (off_t) 0, SEEK_END) >= 0)
#else
#define ffileseekend(e) (lseek ((e), (off_t) 0, 2) >= 0)
#endif
#define ffileclose(e) (close (e) >= 0)

#define fstdiosync(e, z) (fsysdep_sync (fileno (e), z))

#endif /* ! USE_STDIO */

/* A prototype for main to avoid warnings from gcc 2.0
   -Wmissing-prototype option.  */
extern int main P((int argc, char **argv));

/* Some standard routines which we only define if they are not present
   on the system we are compiling on.  */

#if ! HAVE_GETLINE
/* Read a line from a file.  */
extern int getline P((char **pz, size_t *pc, FILE *e));
#endif

#if ! HAVE_REMOVE
/* Erase a file.  */
#undef remove
extern int remove P((const char *zfile));
#endif

#if ! HAVE_STRDUP
/* Copy a string into memory.  */
extern char *strdup P((const char *z));
#endif

#if ! HAVE_STRSTR
/* Look for one string within another.  */
extern char *strstr P((const char *zouter, const char *zinner));
#endif

#if ! HAVE_STRCASECMP
#if HAVE_STRICMP
#define strcasecmp stricmp
#else /* ! HAVE_STRICMP */
/* Rename strcasecmp to avoid ANSI C name space.  */
#define strcasecmp xstrcasecmp
extern int strcasecmp P((const char *z1, const char *z2));
#endif /* ! HAVE_STRICMP */
#endif /* ! HAVE_STRCASECMP */

#if ! HAVE_STRNCASECMP
#if HAVE_STRNICMP
#define strncasecmp strnicmp
#else /* ! HAVE_STRNICMP */
/* Rename strncasecmp to avoid ANSI C name space.  */
#define strncasecmp xstrncasecmp
extern int strncasecmp P((const char *z1, const char *z2, size_t clen));
#endif /* ! HAVE_STRNICMP */
#endif /* ! HAVE_STRNCASECMP */

#if ! HAVE_STRERROR
/* Get a string corresponding to an error message.  */
#undef strerror
extern char *strerror P((int ierr));
#endif

/* Get the appropriate definitions for memcmp, memcpy, memchr and
   bzero.  */
#if ! HAVE_MEMCMP
#if HAVE_BCMP
#define memcmp(p1, p2, c) bcmp ((p1), (p2), (c))
#else /* ! HAVE_BCMP */
extern int memcmp P((constpointer p1, constpointer p2, size_t c));
#endif /* ! HAVE_BCMP */
#endif /* ! HAVE_MEMCMP */

#if ! HAVE_MEMCPY
#if HAVE_BCOPY
#define memcpy(pto, pfrom, c) bcopy ((pfrom), (pto), (c))
#else /* ! HAVE_BCOPY */
extern pointer memcpy P((pointer pto, constpointer pfrom, size_t c));
#endif /* ! HAVE_BCOPY */
#endif /* ! HAVE_MEMCPY */

#if ! HAVE_MEMCHR
extern pointer memchr P((constpointer p, int b, size_t c));
#endif

#if ! HAVE_BZERO
#if HAVE_MEMSET
#define bzero(p, c) memset ((p), 0, (c))
#else /* ! HAVE_MEMSET */
extern void bzero P((pointer p, int c));
#endif /* ! HAVE_MEMSET */
#endif /* ! HAVE_BZERO */

/* Look up a character in a string.  */
#if ! HAVE_STRCHR
#if HAVE_INDEX
#define strchr index
extern char *index ();
#else /* ! HAVE_INDEX */
extern char *strchr P((const char *z, int b));
#endif /* ! HAVE_INDEX */
#endif /* ! HAVE_STRCHR */

#if ! HAVE_STRRCHR
#if HAVE_RINDEX
#define strrchr rindex
extern char *rindex ();
#else /* ! HAVE_RINDEX */
extern char *strrchr P((const char *z, int b));
#endif /* ! HAVE_RINDEX */
#endif /* ! HAVE_STRRCHR */

/* Turn a string into a long integer.  */
#if ! HAVE_STRTOL
extern long strtol P((const char *, char **, int));
#endif

/* Turn a string into a long unsigned integer.  */
#if ! HAVE_STRTOUL
extern unsigned long strtoul P((const char *, char **, int));
#endif

/* Lookup a key in a sorted array.  */
#if ! HAVE_BSEARCH
extern pointer bsearch P((constpointer pkey, constpointer parray,
			  size_t celes, size_t cbytes,
			  int (*pficmp) P((constpointer, constpointer))));
#endif
