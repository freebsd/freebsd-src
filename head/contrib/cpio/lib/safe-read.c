/* An interface to read and write that retries after interrupts.

   Copyright (C) 1993, 1994, 1998, 2002, 2003, 2004, 2005, 2006 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

/* Specification.  */
#ifdef SAFE_WRITE
# include "safe-write.h"
#else
# include "safe-read.h"
#endif

/* Get ssize_t.  */
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>

#ifdef EINTR
# define IS_EINTR(x) ((x) == EINTR)
#else
# define IS_EINTR(x) 0
#endif

#include <limits.h>

#ifdef SAFE_WRITE
# define safe_rw safe_write
# define rw write
#else
# define safe_rw safe_read
# define rw read
# undef const
# define const /* empty */
#endif

/* Read(write) up to COUNT bytes at BUF from(to) descriptor FD, retrying if
   interrupted.  Return the actual number of bytes read(written), zero for EOF,
   or SAFE_READ_ERROR(SAFE_WRITE_ERROR) upon error.  */
size_t
safe_rw (int fd, void const *buf, size_t count)
{
  /* Work around a bug in Tru64 5.1.  Attempting to read more than
     INT_MAX bytes fails with errno == EINVAL.  See
     <http://lists.gnu.org/archive/html/bug-gnu-utils/2002-04/msg00010.html>.
     When decreasing COUNT, keep it block-aligned.  */
  enum { BUGGY_READ_MAXIMUM = INT_MAX & ~8191 };

  for (;;)
    {
      ssize_t result = rw (fd, buf, count);

      if (0 <= result)
	return result;
      else if (IS_EINTR (errno))
	continue;
      else if (errno == EINVAL && BUGGY_READ_MAXIMUM < count)
	count = BUGGY_READ_MAXIMUM;
      else
	return result;
    }
}
