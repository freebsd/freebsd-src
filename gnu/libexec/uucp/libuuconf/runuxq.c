/* maxuxq.c
   Return how often to spawn a uuxqt process.

   Copyright (C) 1994 Ian Lance Taylor

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
const char _uuconf_runuxq_rcsid[] = "$Id: runuxq.c,v 1.1 1994/05/07 18:12:51 ache Exp $";
#endif

/* Return how often to spawn a uuxqt process.  This is either a
   positive number representing the number of execution files to be
   received between spawns, or a special code.  When using
   TAYLOR_CONFIG, this is from the ``run-uuxqt'' command in config
   (the default is UUCONF_RUNUUXQT_ONCE, for compatibility).
   Otherwise, we return UUCONF_RUNUUXQT_PERCALL for HDB_CONFIG and 10
   for V2_CONFIG, to emulate traditional HDB and V2 emulations.  */

int
uuconf_runuuxqt (pglobal, pirunuuxqt)
     pointer pglobal;
     int *pirunuuxqt;
{
#if HAVE_TAYLOR_CONFIG
  {
    struct sglobal *qglobal = (struct sglobal *) pglobal;
    const char *zrun;

    zrun = qglobal->qprocess->zrunuuxqt;
    if (zrun == NULL
	|| strcasecmp (zrun, "once") == 0)
      *pirunuuxqt = UUCONF_RUNUUXQT_ONCE;
    else if (strcasecmp (zrun, "never") == 0)
      *pirunuuxqt = UUCONF_RUNUUXQT_NEVER;
    else if (strcasecmp (zrun, "percall") == 0)
      *pirunuuxqt = UUCONF_RUNUUXQT_PERCALL;
    else
      {
	char *zend;

	*pirunuuxqt = strtol ((char *) qglobal->qprocess->zrunuuxqt,
			      &zend, 10);
	if (*zend != '\0' || *pirunuuxqt <= 0)
	  *pirunuuxqt = UUCONF_RUNUUXQT_ONCE;
      }
  }
#else /* ! HAVE_TAYLOR_CONFIG */
#if HAVE_HDB_CONFIG
  *pirunuuxqt = UUCONF_RUNUUXQT_PERCALL;
#else /* ! HAVE_HDB_CONFIG */
  *pirunuuxqt = 10;
#endif /* ! HAVE_HDB_CONFIG */
#endif /* ! HAVE_TAYLOR_CONFIG */

  return UUCONF_SUCCESS;
}
