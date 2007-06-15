/* tempname.c - generate the name of a temporary file.

   Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <assert.h>

#include <errno.h>
#ifndef __set_errno
# define __set_errno(Val) errno = (Val)
#endif

#include <stdio.h>
#ifndef P_tmpdir
# define P_tmpdir "/tmp"
#endif
#ifndef TMP_MAX
# define TMP_MAX 238328
#endif
#ifndef __GT_FILE
# define __GT_FILE	0
# define __GT_BIGFILE	1
# define __GT_DIR	2
# define __GT_NOCREATE	3
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_FCNTL_H || _LIBC
# include <fcntl.h>
#endif

#if HAVE_SYS_TIME_H || _LIBC
# include <sys/time.h>
#endif

#if HAVE_STDINT_H || _LIBC
# include <stdint.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#if HAVE_UNISTD_H || _LIBC
# include <unistd.h>
#endif

#include <sys/stat.h>
#if STAT_MACROS_BROKEN
# undef S_ISDIR
#endif
#if !defined S_ISDIR && defined S_IFDIR
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#if !S_IRUSR && S_IREAD
# define S_IRUSR S_IREAD
#endif
#if !S_IRUSR
# define S_IRUSR 00400
#endif
#if !S_IWUSR && S_IWRITE
# define S_IWUSR S_IWRITE
#endif
#if !S_IWUSR
# define S_IWUSR 00200
#endif
#if !S_IXUSR && S_IEXEC
# define S_IXUSR S_IEXEC
#endif
#if !S_IXUSR
# define S_IXUSR 00100
#endif

#if _LIBC
# define struct_stat64 struct stat64
#else
# define struct_stat64 struct stat
# define __getpid getpid
# define __gettimeofday gettimeofday
# define __mkdir mkdir
# define __open open
# define __open64 open
# define __lxstat64(version, path, buf) lstat (path, buf)
# define __xstat64(version, path, buf) stat (path, buf)
#endif

#if ! (HAVE___SECURE_GETENV || _LIBC)
# define __secure_getenv getenv
#endif

#ifdef _LIBC
# include <hp-timing.h>
# if HP_TIMING_AVAIL
#  define RANDOM_BITS(Var) \
  if (__builtin_expect (value == UINT64_C (0), 0))			      \
    {									      \
      /* If this is the first time this function is used initialize	      \
	 the variable we accumulate the value in to some somewhat	      \
	 random value.  If we'd not do this programs at startup time	      \
	 might have a reduced set of possible names, at least on slow	      \
	 machines.  */							      \
      struct timeval tv;						      \
      __gettimeofday (&tv, NULL);					      \
      value = ((uint64_t) tv.tv_usec << 16) ^ tv.tv_sec;		      \
    }									      \
  HP_TIMING_NOW (Var)
# endif
#endif

/* Use the widest available unsigned type if uint64_t is not
   available.  The algorithm below extracts a number less than 62**6
   (approximately 2**35.725) from uint64_t, so ancient hosts where
   uintmax_t is only 32 bits lose about 3.725 bits of randomness,
   which is better than not having mkstemp at all.  */
#if !defined UINT64_MAX && !defined uint64_t
# define uint64_t uintmax_t
#endif

/* Return nonzero if DIR is an existent directory.  */
static int
direxists (const char *dir)
{
  struct_stat64 buf;
  return __xstat64 (_STAT_VER, dir, &buf) == 0 && S_ISDIR (buf.st_mode);
}

/* Path search algorithm, for tmpnam, tmpfile, etc.  If DIR is
   non-null and exists, uses it; otherwise uses the first of $TMPDIR,
   P_tmpdir, /tmp that exists.  Copies into TMPL a template suitable
   for use with mk[s]temp.  Will fail (-1) if DIR is non-null and
   doesn't exist, none of the searched dirs exists, or there's not
   enough space in TMPL. */
int
__path_search (char *tmpl, size_t tmpl_len, const char *dir, const char *pfx,
	       int try_tmpdir)
{
  const char *d;
  size_t dlen, plen;

  if (!pfx || !pfx[0])
    {
      pfx = "file";
      plen = 4;
    }
  else
    {
      plen = strlen (pfx);
      if (plen > 5)
	plen = 5;
    }

  if (try_tmpdir)
    {
      d = __secure_getenv ("TMPDIR");
      if (d != NULL && direxists (d))
	dir = d;
      else if (dir != NULL && direxists (dir))
	/* nothing */ ;
      else
	dir = NULL;
    }
  if (dir == NULL)
    {
      if (direxists (P_tmpdir))
	dir = P_tmpdir;
      else if (strcmp (P_tmpdir, "/tmp") != 0 && direxists ("/tmp"))
	dir = "/tmp";
      else
	{
	  __set_errno (ENOENT);
	  return -1;
	}
    }

  dlen = strlen (dir);
  while (dlen > 1 && dir[dlen - 1] == '/')
    dlen--;			/* remove trailing slashes */

  /* check we have room for "${dir}/${pfx}XXXXXX\0" */
  if (tmpl_len < dlen + 1 + plen + 6 + 1)
    {
      __set_errno (EINVAL);
      return -1;
    }

  sprintf (tmpl, "%.*s/%.*sXXXXXX", (int) dlen, dir, (int) plen, pfx);
  return 0;
}

/* These are the characters used in temporary filenames.  */
static const char letters[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Generate a temporary file name based on TMPL.  TMPL must match the
   rules for mk[s]temp (i.e. end in "XXXXXX").  The name constructed
   does not exist at the time of the call to __gen_tempname.  TMPL is
   overwritten with the result.

   KIND may be one of:
   __GT_NOCREATE:	simply verify that the name does not exist
			at the time of the call.
   __GT_FILE:		create the file using open(O_CREAT|O_EXCL)
			and return a read-write fd.  The file is mode 0600.
   __GT_BIGFILE:	same as __GT_FILE but use open64().
   __GT_DIR:		create a directory, which will be mode 0700.

   We use a clever algorithm to get hard-to-predict names. */
int
__gen_tempname (char *tmpl, int kind)
{
  int len;
  char *XXXXXX;
  static uint64_t value;
  uint64_t random_time_bits;
  unsigned int count;
  int fd = -1;
  int save_errno = errno;
  struct_stat64 st;

  /* A lower bound on the number of temporary files to attempt to
     generate.  The maximum total number of temporary file names that
     can exist for a given template is 62**6.  It should never be
     necessary to try all these combinations.  Instead if a reasonable
     number of names is tried (we define reasonable as 62**3) fail to
     give the system administrator the chance to remove the problems.  */
  unsigned int attempts_min = 62 * 62 * 62;

  /* The number of times to attempt to generate a temporary file.  To
     conform to POSIX, this must be no smaller than TMP_MAX.  */
  unsigned int attempts = attempts_min < TMP_MAX ? TMP_MAX : attempts_min;

  len = strlen (tmpl);
  if (len < 6 || strcmp (&tmpl[len - 6], "XXXXXX"))
    {
      __set_errno (EINVAL);
      return -1;
    }

  /* This is where the Xs start.  */
  XXXXXX = &tmpl[len - 6];

  /* Get some more or less random data.  */
#ifdef RANDOM_BITS
  RANDOM_BITS (random_time_bits);
#else
# if HAVE_GETTIMEOFDAY || _LIBC
  {
    struct timeval tv;
    __gettimeofday (&tv, NULL);
    random_time_bits = ((uint64_t) tv.tv_usec << 16) ^ tv.tv_sec;
  }
# else
  random_time_bits = time (NULL);
# endif
#endif
  value += random_time_bits ^ __getpid ();

  for (count = 0; count < attempts; value += 7777, ++count)
    {
      uint64_t v = value;

      /* Fill in the random bits.  */
      XXXXXX[0] = letters[v % 62];
      v /= 62;
      XXXXXX[1] = letters[v % 62];
      v /= 62;
      XXXXXX[2] = letters[v % 62];
      v /= 62;
      XXXXXX[3] = letters[v % 62];
      v /= 62;
      XXXXXX[4] = letters[v % 62];
      v /= 62;
      XXXXXX[5] = letters[v % 62];

      switch (kind)
	{
	case __GT_FILE:
	  fd = __open (tmpl, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	  break;

	case __GT_BIGFILE:
	  fd = __open64 (tmpl, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	  break;

	case __GT_DIR:
	  fd = __mkdir (tmpl, S_IRUSR | S_IWUSR | S_IXUSR);
	  break;

	case __GT_NOCREATE:
	  /* This case is backward from the other three.  __gen_tempname
	     succeeds if __xstat fails because the name does not exist.
	     Note the continue to bypass the common logic at the bottom
	     of the loop.  */
	  if (__lxstat64 (_STAT_VER, tmpl, &st) < 0)
	    {
	      if (errno == ENOENT)
		{
		  __set_errno (save_errno);
		  return 0;
		}
	      else
		/* Give up now. */
		return -1;
	    }
	  continue;

	default:
	  assert (! "invalid KIND in __gen_tempname");
	}

      if (fd >= 0)
	{
	  __set_errno (save_errno);
	  return fd;
	}
      else if (errno != EEXIST)
	return -1;
    }

  /* We got out of the loop because we ran out of combinations to try.  */
  __set_errno (EEXIST);
  return -1;
}
