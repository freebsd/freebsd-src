/* Reentrant time functions like localtime_r.

   Copyright (C) 2003 Free Software Foundation, Inc.

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

/* Written by Paul Eggert.  */

#ifndef _TIME_R_H
#define _TIME_R_H

/* Include <time.h> first, since it may declare these functions with
   signatures that disagree with POSIX, and we don't want to rename
   those declarations.  */
#include <time.h>

#if !HAVE_TIME_R_POSIX
# undef asctime_r
# undef ctime_r
# undef gmtime_r
# undef localtime_r

# define asctime_r rpl_asctime_r
# define ctime_r rpl_ctime_r
# define gmtime_r rpl_gmtime_r
# define localtime_r rpl_localtime_r

char *asctime_r (struct tm const * restrict, char * restrict);
char *ctime_r (time_t const *, char *);
struct tm *gmtime_r (time_t const * restrict, struct tm * restrict);
struct tm *localtime_r (time_t const * restrict, struct tm * restrict);
#endif

#endif
