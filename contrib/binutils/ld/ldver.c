/* ldver.c -- Print linker version.
   Copyright (C) 1991, 92, 93, 94, 95, 1996, 1998 Free Software Foundation, Inc.

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
#include "ldemul.h"
#include "ldmain.h"

const char *ld_program_version = VERSION;

void
ldversion (noisy)
     int noisy;
{
  fprintf (stdout, _("GNU ld version %s (with BFD %s)\n"),
	   ld_program_version, BFD_VERSION);

  if (noisy) 
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
