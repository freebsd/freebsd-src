/* tmpfil.c
   Get a temporary file name.

   Copyright (C) 1991, 1992, 1993 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#include "uudefs.h"
#include "uuconf.h"
#include "system.h"
#include "sysdep.h"

#define ZDIGS \
  "0123456789abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-"
#define CDIGS (sizeof ZDIGS - 1)

/*ARGSUSED*/
char *
zstemp_file (qsys)
     const struct uuconf_system *qsys;
{
  static int icount;
  const char *const zdigs = ZDIGS;
  char ab[14];
  pid_t ime;
  int iset;

  ab[0] = 'T';
  ab[1] = 'M';
  ab[2] = '.';

  ime = getpid ();
  iset = 3;
  while (ime > 0 && iset < 10)
    {
      ab[iset] = zdigs[ime % CDIGS];
      ime /= CDIGS;
      ++iset;
    }

  ab[iset] = '.';
  ++iset;

  ab[iset] = zdigs[icount / CDIGS];
  ++iset;
  ab[iset] = zdigs[icount % CDIGS];
  ++iset;

  ab[iset] = '\0';

  ++icount;
  if (icount >= CDIGS * CDIGS)
    icount = 0;

#if SPOOLDIR_V2 || SPOOLDIR_BSD42
  return zbufcpy (ab);
#endif
#if SPOOLDIR_BSD43 || SPOOLDIR_ULTRIX || SPOOLDIR_TAYLOR
  return zsysdep_in_dir (".Temp", ab);
#endif
#if SPOOLDIR_HDB || SPOOLDIR_SVR4
  return zsysdep_in_dir (qsys->uuconf_zname, ab);
#endif
}
