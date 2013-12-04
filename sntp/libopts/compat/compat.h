/*  -*- Mode: C -*-  */

/**
 * \file compat.h --- fake the preprocessor into handlng portability
 *
 *  Time-stamp:      "2010-07-16 15:11:57 bkorb"
 *
 *  compat.h is free software.
 *  This file is part of AutoGen.
 *
 *  AutoGen Copyright (c) 1992-2011 by Bruce Korb - all rights reserved
 *
 *  AutoGen is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AutoGen is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As a special exception, Bruce Korb gives permission for additional
 *  uses of the text contained in the release of compat.h.
 *
 *  The exception is that, if you link the compat.h library with other
 *  files to produce an executable, this does not by itself cause the
 *  resulting executable to be covered by the GNU General Public License.
 *  Your use of that executable is in no way restricted on account of
 *  linking the compat.h library code into it.
 *
 *  This exception does not however invalidate any other reasons why
 *  the executable file might be covered by the GNU General Public License.
 *
 *  This exception applies only to the code released by Bruce Korb under
 *  the name compat.h.  If you copy code from other sources under the
 *  General Public License into a copy of compat.h, as the General Public
 *  License permits, the exception does not apply to the code that you add
 *  in this way.  To avoid misleading anyone as to the status of such
 *  modified files, you must delete this exception notice from them.
 *
 *  If you write modifications of your own for compat.h, it is your choice
 *  whether to permit this exception to apply to your modifications.
 *  If you do not wish that, delete this exception notice.
 */
#ifndef COMPAT_H_GUARD
#define COMPAT_H_GUARD 1

#if defined(HAVE_CONFIG_H)
#  include <config.h>

#elif defined(_WIN32) && !defined(__CYGWIN__)
#  include "windows-config.h"

#else
#  error "compat.h" requires "config.h"
   choke me.
#endif


#ifndef HAVE_STRSIGNAL
   char * strsignal( int signo );
#endif

#define  _GNU_SOURCE    1 /* for strsignal in GNU's libc */
#define  __USE_GNU      1 /* exact same thing as above   */
#define  __EXTENSIONS__ 1 /* and another way to call for it */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  SYSTEM HEADERS:
 */
#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif
#include <sys/param.h>
#if HAVE_SYS_PROCSET_H
#  include <sys/procset.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>

#if defined( HAVE_SOLARIS_SYSINFO )
#  include <sys/systeminfo.h>
#elif defined( HAVE_UNAME_SYSCALL )
#  include <sys/utsname.h>
#endif

#ifdef DAEMON_ENABLED
#  if HAVE_SYS_STROPTS_H
#  include <sys/stropts.h>
#  endif

#  if HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#  endif

#  if ! defined(HAVE_SYS_POLL_H) && ! defined(HAVE_SYS_SELECT_H)
#    error This system cannot support daemon processing
     Choke Me.
#  endif

#  if HAVE_SYS_POLL_H
#  include <sys/poll.h>
#  endif

#  if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#  endif

#  if HAVE_NETINET_IN_H
#  include <netinet/in.h>
#  endif

#  if HAVE_SYS_UN_H
#  include <sys/un.h>
#  endif
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  USER HEADERS:
 */
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

/*
 *  Directory opening stuff:
 */
# if defined (_POSIX_SOURCE)
/* Posix does not require that the d_ino field be present, and some
   systems do not provide it. */
#    define REAL_DIR_ENTRY(dp) 1
# else /* !_POSIX_SOURCE */
#    define REAL_DIR_ENTRY(dp) (dp->d_ino != 0)
# endif /* !_POSIX_SOURCE */

# if defined (HAVE_DIRENT_H)
#   include <dirent.h>
#   define D_NAMLEN(dirent) strlen((dirent)->d_name)
# else /* !HAVE_DIRENT_H */
#   define dirent direct
#   define D_NAMLEN(dirent) (dirent)->d_namlen
#   if defined (HAVE_SYS_NDIR_H)
#     include <sys/ndir.h>
#   endif /* HAVE_SYS_NDIR_H */
#   if defined (HAVE_SYS_DIR_H)
#     include <sys/dir.h>
#   endif /* HAVE_SYS_DIR_H */
#   if defined (HAVE_NDIR_H)
#     include <ndir.h>
#   endif /* HAVE_NDIR_H */
# endif /* !HAVE_DIRENT_H */

#include <errno.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifndef O_NONBLOCK
# define O_NONBLOCK FNDELAY
#endif

#if defined(HAVE_LIBGEN) && defined(HAVE_LIBGEN_H)
#  include <libgen.h>
#endif

#if defined(HAVE_LIMITS_H)  /* this is also in options.h */
#  include <limits.h>
#elif defined(HAVE_SYS_LIMITS_H)
#  include <sys/limits.h>
#endif /* HAVE_LIMITS/SYS_LIMITS_H */

#include <memory.h>
#include <setjmp.h>
#include <signal.h>

#if defined( HAVE_STDINT_H )
#  include <stdint.h>
#elif defined( HAVE_INTTYPES_H )
#  include <inttypes.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <time.h>

#ifdef HAVE_UTIME_H
#  include <utime.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  FIXUPS and CONVIENCE STUFF:
 */
#ifdef __cplusplus
#   define EXTERN extern "C"
#else
#   define EXTERN extern
#endif

/* some systems #def errno! and others do not declare it!! */
#ifndef errno
   extern int errno;
#endif

/* Some machines forget this! */

# ifndef EXIT_FAILURE
#   define EXIT_SUCCESS 0
#   define EXIT_FAILURE 1
# endif

#ifndef NUL
#  define NUL '\0'
#endif

#ifndef NULL
#  define NULL 0
#endif

#if !defined (MAXPATHLEN) && defined (HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif /* !MAXPATHLEN && HAVE_SYS_PARAM_H */

#if !defined (MAXPATHLEN) && defined (PATH_MAX)
#  define MAXPATHLEN PATH_MAX
#endif /* !MAXPATHLEN && PATH_MAX */

#if !defined (MAXPATHLEN) && defined(_MAX_PATH)
#  define PATH_MAX _MAX_PATH
#  define MAXPATHLEN _MAX_PATH
#endif

#if !defined (MAXPATHLEN)
#  define MAXPATHLEN ((size_t)4096)
#endif /* MAXPATHLEN */

#define AG_PATH_MAX  ((size_t)MAXPATHLEN)

#ifndef LONG_MAX
#  define LONG_MAX      ~(1L << (8*sizeof(long) -1))
#  define INT_MAX       ~(1 << (8*sizeof(int) -1))
#endif

#ifndef ULONG_MAX
#  define ULONG_MAX     ~(OUL)
#  define UINT_MAX      ~(OU)
#endif

#ifndef SHORT_MAX
#  define SHORT_MAX     ~(1 << (8*sizeof(short) - 1))
#else
#  define USHORT_MAX    ~(OUS)
#endif

#ifndef HAVE_INT8_T
  typedef signed char           int8_t;
# define  HAVE_INT8_T           1
#endif
#ifndef HAVE_UINT8_T
  typedef unsigned char         uint8_t;
# define  HAVE_UINT8_T          1
#endif
#ifndef HAVE_INT16_T
  typedef signed short          int16_t;
# define  HAVE_INT16_T          1
#endif
#ifndef HAVE_UINT16_T
  typedef unsigned short        uint16_t;
# define  HAVE_UINT16_T         1
#endif

#ifndef HAVE_INT32_T
# if SIZEOF_INT ==              4
    typedef signed int          int32_t;
# elif SIZEOF_LONG ==           4
    typedef signed long         int32_t;
# endif
# define  HAVE_INT32_T          1
#endif

#ifndef HAVE_UINT32_T
# if SIZEOF_INT ==              4
    typedef unsigned int        uint32_t;
# elif SIZEOF_LONG ==           4
    typedef unsigned long       uint32_t;
# else
#   error Cannot create a uint32_t type.
    Choke Me.
# endif
# define  HAVE_UINT32_T         1
#endif

#ifndef HAVE_INTPTR_T
# if SIZEOF_CHARP == SIZEOF_LONG
    typedef signed long         intptr_t;
# else
    typedef signed int          intptr_t;
# endif
# define  HAVE_INTPTR_T         1
#endif

#ifndef HAVE_UINTPTR_T
# if SIZEOF_CHARP == SIZEOF_LONG
    typedef unsigned long       intptr_t;
# else
    typedef unsigned int        intptr_t;
# endif
# define  HAVE_INTPTR_T         1
#endif

#ifndef HAVE_UINT_T
  typedef unsigned int          uint_t;
# define  HAVE_UINT_T           1
#endif

#ifndef HAVE_SIZE_T
  typedef unsigned int          size_t;
# define  HAVE_SIZE_T           1
#endif
#ifndef HAVE_WINT_T
  typedef unsigned int          wint_t;
# define  HAVE_WINT_T           1
#endif
#ifndef HAVE_PID_T
  typedef signed int            pid_t;
# define  HAVE_PID_T            1
#endif

/* redefine these for BSD style string libraries */
#ifndef HAVE_STRCHR
#  define strchr            index
#  define strrchr           rindex
#endif

#ifdef USE_FOPEN_BINARY
#  ifndef FOPEN_BINARY_FLAG
#    define FOPEN_BINARY_FLAG   "b"
#  endif
#  ifndef FOPEN_TEXT_FLAG
#    define FOPEN_TEXT_FLAG     "t"
#  endif
#else
#  ifndef FOPEN_BINARY_FLAG
#    define FOPEN_BINARY_FLAG
#  endif
#  ifndef FOPEN_TEXT_FLAG
#    define FOPEN_TEXT_FLAG
#  endif
#endif

#ifndef STR
#  define _STR(s) #s
#  define STR(s)  _STR(s)
#endif

/* ##### Pointer sized word ##### */

/* FIXME:  the MAX stuff in here is broken! */
#if SIZEOF_CHARP > SIZEOF_INT
   typedef long t_word;
   #define WORD_MAX  LONG_MAX
   #define WORD_MIN  LONG_MIN
#else /* SIZEOF_CHARP <= SIZEOF_INT */
   typedef int t_word;
   #define WORD_MAX  INT_MAX
   #define WORD_MIN  INT_MIN
#endif

#endif /* COMPAT_H_GUARD */

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of compat/compat.h */
