/* tsnams.c
   Get all known system names from the Taylor UUCP configuration files.

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
const char _uuconf_tsnams_rcsid[] = "$Id: tsnams.c,v 1.1 1993/08/05 18:26:14 conklin Exp $";
#endif

/* Get all the system names from the Taylor UUCP configuration files.
   These were actually already recorded by uuconf_taylor_init, so this
   function is pretty simple.  */

int
uuconf_taylor_system_names (pglobal, ppzsystems, falias)
     pointer pglobal;
     char ***ppzsystems;
     int falias;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  int iret;
  register struct stsysloc *q;
  char **pz;
  int c, i;

  if (! qglobal->qprocess->fread_syslocs)
    {
      iret = _uuconf_iread_locations (qglobal);
      if (iret != UUCONF_SUCCESS)
	return iret;
    }

  *ppzsystems = NULL;
  c = 0;

  for (q = qglobal->qprocess->qsyslocs; q != NULL; q = q->qnext)
    {
      if (! falias && q->falias)
	continue;

      iret = _uuconf_iadd_string (qglobal, (char *) q->zname, TRUE, FALSE,
				  ppzsystems, (pointer) NULL);
      if (iret != UUCONF_SUCCESS)
	return iret;
      ++c;
    }

  /* The order of the qSyslocs list is reversed from the list in the
     configuration files.  Reverse the returned list in order to make
     uuname output more intuitive.  */
  pz = *ppzsystems;
  for (i = c / 2 - 1; i >= 0; i--)
    {
      char *zhold;

      zhold = pz[i];
      pz[i] = pz[c - i - 1];
      pz[c - i - 1] = zhold;
    }

  return UUCONF_SUCCESS;
}
