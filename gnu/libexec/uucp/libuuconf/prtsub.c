/* prtsub.c
   Port information subroutines.

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
const char _uuconf_prtsub_rcsid[] = "$FreeBSD: src/gnu/libexec/uucp/libuuconf/prtsub.c,v 1.6 1999/08/27 23:33:27 peter Exp $";
#endif

/* Clear the information in a port.  This can only clear the type
   independent information; the port type specific information is
   cleared when the type of the port is set.  */

void
_uuconf_uclear_port (qport)
     struct uuconf_port *qport;
{
  qport->uuconf_zname = NULL;
  qport->uuconf_ttype = UUCONF_PORTTYPE_UNKNOWN;
  qport->uuconf_zprotocols = NULL;
  qport->uuconf_qproto_params = NULL;

  /* Note that we do not set RELIABLE_SPECIFIED; this just sets
     defaults, so that ``seven-bit true'' does not imply ``reliable
     false''.  */
  qport->uuconf_ireliable = (UUCONF_RELIABLE_RELIABLE
			     | UUCONF_RELIABLE_EIGHT
			     | UUCONF_RELIABLE_FULLDUPLEX);

  qport->uuconf_zlockname = NULL;
  qport->uuconf_palloc = NULL;
}
