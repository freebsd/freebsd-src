/* grdcmp.c
   Compare two grades.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_grdcmp_rcsid[] = "$FreeBSD$";
#endif

#include <ctype.h>

/* Compare two grades, returning < 0 if b1 should be executed before
   b2, == 0 if they are the same, or > 0 if b1 should be executed
   after b2.  This can not fail, and does not return a standard uuconf
   error code.

   This implementation assumes that the upper case letters are in
   sequence, and that the lower case letters are in sequence.  */

int
uuconf_grade_cmp (barg1, barg2)
     int barg1;
     int barg2;
{
  int b1, b2;

  /* Make sure the arguments are unsigned.  */
  b1 = (int) BUCHAR (barg1);
  b2 = (int) BUCHAR (barg2);

  if (isdigit (b1))
    {
      if (isdigit (b2))
	return b1 - b2;
      else
	return -1;
    }
  else if (isupper (b1))
    {
      if (isdigit (b2))
	return 1;
      else if (isupper (b2))
	return b1 - b2;
      else
	return -1;
    }
  else
    {
      if (! islower (b2))
	return 1;
      else
	return b1 - b2;
    }
}
