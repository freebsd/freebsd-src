/* Intel 387 floating point stuff.
   Copyright (C) 1988, 1989, 1991 Free Software Foundation, Inc.

This file is part of GDB.

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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "language.h"
#include "gdbcore.h"
#include "floatformat.h"

/* FIXME:  Eliminate these routines when we have the time to change all
   the callers.  */

void
i387_to_double (from, to)
     char *from;
     char *to;
{
  floatformat_to_double (&floatformat_i387_ext, from, (double *)to);
}

void
double_to_i387 (from, to)
     char *from;
     char *to;
{
  floatformat_from_double (&floatformat_i387_ext, (double *)from, to);
}

void
print_387_control_word (control)
     unsigned int control;
{
  printf_unfiltered ("control %s: ", local_hex_string(control));
  printf_unfiltered ("compute to ");
  switch ((control >> 8) & 3) 
    {
    case 0: printf_unfiltered ("24 bits; "); break;
    case 1: printf_unfiltered ("(bad); "); break;
    case 2: printf_unfiltered ("53 bits; "); break;
    case 3: printf_unfiltered ("64 bits; "); break;
    }
  printf_unfiltered ("round ");
  switch ((control >> 10) & 3) 
    {
    case 0: printf_unfiltered ("NEAREST; "); break;
    case 1: printf_unfiltered ("DOWN; "); break;
    case 2: printf_unfiltered ("UP; "); break;
    case 3: printf_unfiltered ("CHOP; "); break;
    }
  if (control & 0x3f) 
    {
      printf_unfiltered ("mask:");
      if (control & 0x0001) printf_unfiltered (" INVALID");
      if (control & 0x0002) printf_unfiltered (" DENORM");
      if (control & 0x0004) printf_unfiltered (" DIVZ");
      if (control & 0x0008) printf_unfiltered (" OVERF");
      if (control & 0x0010) printf_unfiltered (" UNDERF");
      if (control & 0x0020) printf_unfiltered (" LOS");
      printf_unfiltered (";");
    }
  printf_unfiltered ("\n");
  if (control & 0xe080) warning ("reserved bits on: %s\n",
				local_hex_string(control & 0xe080));
}

void
print_387_status_word (status)
     unsigned int status;
{
  printf_unfiltered ("status %s: ", local_hex_string (status));
  if (status & 0xff) 
    {
      printf_unfiltered ("exceptions:");
      if (status & 0x0001) printf_unfiltered (" INVALID");
      if (status & 0x0002) printf_unfiltered (" DENORM");
      if (status & 0x0004) printf_unfiltered (" DIVZ");
      if (status & 0x0008) printf_unfiltered (" OVERF");
      if (status & 0x0010) printf_unfiltered (" UNDERF");
      if (status & 0x0020) printf_unfiltered (" LOS");
      if (status & 0x0040) printf_unfiltered (" FPSTACK");
      printf_unfiltered ("; ");
    }
  printf_unfiltered ("flags: %d%d%d%d; ",
	  (status & 0x4000) != 0,
	  (status & 0x0400) != 0,
	  (status & 0x0200) != 0,
	  (status & 0x0100) != 0);

  printf_unfiltered ("top %d\n", (status >> 11) & 7);
}
