/* int.c
   Parse a string into an int or a long.

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
const char _uuconf_int_rcsid[] = "$Id: int.c,v 1.1 1993/08/05 18:25:37 conklin Exp $";
#endif

/* Parse a string into a variable.  This is called by uuconf_cmd_args,
   as well as other functions.  The parsing is done in a single place
   to make it easy to change.  This should return an error code,
   including both UUCONF_CMDTABRET_KEEP and UUCONF_CMDTABRET_EXIT if
   appropriate.  */

/*ARGSIGNORED*/
int
_uuconf_iint (qglobal, zval, p, fint)
     struct sglobal *qglobal;
     const char *zval;
     pointer p;
     boolean fint;
{
  long i;
  char *zend;

  i = strtol ((char *) zval, &zend, 10);
  if (*zend != '\0')
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  if (fint)
    *(int *) p = (int) i;
  else
    *(long *) p = i;

  return UUCONF_CMDTABRET_CONTINUE;
}
