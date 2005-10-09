/* timespec -- System time interface

   Copyright (C) 2000, 2002, 2004 Free Software Foundation, Inc.

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
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if ! defined TIMESPEC_H
# define TIMESPEC_H

/* You must include config.h before including this file.  */

# include <sys/types.h>
# if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  if HAVE_SYS_TIME_H
#   include <sys/time.h>
#  else
#   include <time.h>
#  endif
# endif

# if ! HAVE_STRUCT_TIMESPEC
/* Some systems don't define this struct, e.g., AIX 4.1, Ultrix 4.3.  */
struct timespec
{
  time_t tv_sec;
  long tv_nsec;
};
# endif

# ifdef ST_MTIM_NSEC
#  define ST_TIME_CMP_NS(a, b, ns) ((a).ns < (b).ns ? -1 : (a).ns > (b).ns)
# else
#  define ST_TIME_CMP_NS(a, b, ns) 0
# endif
# define ST_TIME_CMP(a, b, s, ns) \
   ((a).s < (b).s ? -1 : (a).s > (b).s ? 1 : ST_TIME_CMP_NS(a, b, ns))
# define ATIME_CMP(a, b) ST_TIME_CMP (a, b, st_atime, st_atim.ST_MTIM_NSEC)
# define CTIME_CMP(a, b) ST_TIME_CMP (a, b, st_ctime, st_ctim.ST_MTIM_NSEC)
# define MTIME_CMP(a, b) ST_TIME_CMP (a, b, st_mtime, st_mtim.ST_MTIM_NSEC)

# ifdef ST_MTIM_NSEC
#  define TIMESPEC_NS(timespec) ((timespec).ST_MTIM_NSEC)
# else
#  define TIMESPEC_NS(timespec) 0
# endif

# if ! HAVE_DECL_NANOSLEEP
/* Don't specify a prototype here.  Some systems (e.g., OSF) declare
   nanosleep with a conflicting one (const-less first parameter).  */
int nanosleep ();
# endif

int gettime (struct timespec *);
int settime (struct timespec const *);

#endif
