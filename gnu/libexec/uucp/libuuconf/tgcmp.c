/* tgcmp.c
   A comparison function for grades for _uuconf_time_parse.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_tgcmp_rcsid[] = "$Id: tgcmp.c,v 1.1 1993/08/05 18:26:05 conklin Exp $";
#endif

/* A comparison function to pass to _uuconf_itime_parse.  This
   compares grades.  We can't just pass uuconf_grade_cmp, since
   _uuconf_itime_parse wants a function takes longs as arguments.  */

int
_uuconf_itime_grade_cmp (i1, i2)
     long i1;
     long i2;
{
  return UUCONF_GRADE_CMP ((int) i1, (int) i2);
}
