/* locnm.c
   Get the local node name.

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
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_locnm_rcsid[] = "$Id: locnm.c,v 1.2 1994/05/07 18:12:38 ache Exp $";
#endif

/* Get the local node name.  */

int
uuconf_localname (pglobal, pzname)
     pointer pglobal;
     const char **pzname;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;

  *pzname = qglobal->qprocess->zlocalname;
  if (*pzname != NULL)
    return UUCONF_SUCCESS;
  else
    return UUCONF_NOT_FOUND;
}
