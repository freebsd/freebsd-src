/* base.c
   Subroutine to turn a cmdtab_offset table into a uuconf_cmdtab table.

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
const char _uuconf_base_rcsid[] = "$Id: base.c,v 1.2 1994/05/07 18:11:57 ache Exp $";
#endif

/* This turns a cmdtab_offset table into a uuconf_cmdtab table.  Each
   offset is adjusted by a base value.  */

void
_uuconf_ucmdtab_base (qoff, celes, pbase, qset)
     register const struct cmdtab_offset *qoff;
     size_t celes;
     char *pbase;
     register struct uuconf_cmdtab *qset;
{
  register size_t i;

  for (i = 0; i < celes; i++, qoff++, qset++)
    {
      qset->uuconf_zcmd = qoff->zcmd;
      qset->uuconf_itype = qoff->itype;
      if (qoff->ioff == (size_t) -1)
	qset->uuconf_pvar = NULL;
      else
	qset->uuconf_pvar = pbase + qoff->ioff;
      qset->uuconf_pifn = qoff->pifn;
    }
}
