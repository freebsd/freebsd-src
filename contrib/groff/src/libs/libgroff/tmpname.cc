/* Copyright (C) 2001 Free Software Foundation, Inc.
     Written by Werner Lemberg (wl@gnu.org)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */


/* This file is heavily based on the function __gen_tempname() in the
   file tempname.c which is part of the fileutils package. */


#include "lib.h"

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "posix.h"
#include "nonposix.h"

#ifndef TMP_MAX
# define TMP_MAX 238328
#endif

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_GETTIMEOFDAY
#ifdef NEED_DECLARATION_GETTIMEOFDAY
extern "C" {
  int gettimeofday(struct timeval *, void *);
}
#endif
#endif

#if HAVE_STDINT_H
# include <stdint.h>
#endif

/* Use the widest available unsigned type if uint64_t is not
   available.  The algorithm below extracts a number less than 62**6
   (approximately 2**35.725) from uint64_t, so ancient hosts where
   uintmax_t is only 32 bits lose about 3.725 bits of randomness,
   which is better than not having mkstemp at all.  */
#if !defined UINT64_MAX && !defined uint64_t
# define uint64_t uintmax_t
#endif

/* These are the characters used in temporary filenames.  */
static const char letters[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

int gen_tempname(char *tmpl, int dir)
{
  static uint64_t value;

  size_t len = strlen(tmpl);
  if (len < 6 || strcmp(&tmpl[len - 6], "XXXXXX"))
    return -1; /* EINVAL */

  /* This is where the Xs start.  */
  char *XXXXXX = &tmpl[len - 6];

  /* Get some more or less random data.  */
#if HAVE_GETTIMEOFDAY
  timeval tv;
  gettimeofday(&tv, NULL);
  uint64_t random_time_bits = ((uint64_t)tv.tv_usec << 16) ^ tv.tv_sec;
#else
  uint64_t random_time_bits = time(NULL);
#endif
  value += random_time_bits ^ getpid();

  for (int count = 0; count < TMP_MAX; value += 7777, ++count) {
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

    int fd = dir ? mkdir(tmpl, S_IRUSR | S_IWUSR | S_IXUSR)
		 : open(tmpl, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

    if (fd >= 0)
      return fd;
    else if (errno != EEXIST)
      return -1;
  }

  /* We got out of the loop because we ran out of combinations to try.  */
  return -1; /* EEXIST */
}
