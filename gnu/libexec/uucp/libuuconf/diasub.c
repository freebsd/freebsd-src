/* diasub.c
   Dialer information subroutines.

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
const char _uuconf_diasub_rcsid[] = "$Id: diasub.c,v 1.2 1994/05/07 18:12:09 ache Exp $";
#endif

/* Clear the information in a dialer.  */

#define INIT_CHAT(q) \
  ((q)->uuconf_pzchat = NULL, \
   (q)->uuconf_pzprogram = NULL, \
   (q)->uuconf_ctimeout = 60, \
   (q)->uuconf_pzfail = NULL, \
   (q)->uuconf_fstrip = TRUE)

void
_uuconf_uclear_dialer (qdialer)
     struct uuconf_dialer *qdialer;
{
  qdialer->uuconf_zname = NULL;
  INIT_CHAT (&qdialer->uuconf_schat);
  qdialer->uuconf_zdialtone = (char *) ",";
  qdialer->uuconf_zpause = (char *) ",";
  qdialer->uuconf_fcarrier = TRUE;
  qdialer->uuconf_ccarrier_wait = 60;
  qdialer->uuconf_fdtr_toggle = FALSE;
  qdialer->uuconf_fdtr_toggle_wait = FALSE;
  INIT_CHAT (&qdialer->uuconf_scomplete);
  INIT_CHAT (&qdialer->uuconf_sabort);
  qdialer->uuconf_qproto_params = NULL;
  /* Note that we do not set RELIABLE_SPECIFIED; this just sets
     defaults, so that ``seven-bit true'' does not imply ``reliable
     false''.  */
  qdialer->uuconf_ireliable = (UUCONF_RELIABLE_RELIABLE
			       | UUCONF_RELIABLE_EIGHT
			       | UUCONF_RELIABLE_FULLDUPLEX);
  qdialer->uuconf_palloc = NULL;
}
