/* ldver.c -- Print linker version.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000, 2001, 2002
   Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "bfd.h"
#include "sysdep.h"

#include "ld.h"
#include "ldver.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"
#include "ldmain.h"

void
ldversion (noisy)
     int noisy;
{
  /* Output for noisy == 2 is intended to follow the GNU standards.  */
  fprintf (stdout, _("GNU ld version %s\n"), BFD_VERSION_STRING);

  if (noisy & 2)
    {
      printf (_("Copyright 2002 Free Software Foundation, Inc.\n"));
      printf (_("\
This program is free software; you may redistribute it under the terms of\n\
the GNU General Public License.  This program has absolutely no warranty.\n"));
    }

  if (noisy & 1)
    {
      ld_emulation_xfer_type **ptr = ld_emulations;

      printf (_("  Supported emulations:\n"));
      while (*ptr)
	{
	  printf ("   %s\n", (*ptr)->emulation_name);
	  ptr++;
	}
    }
}
