/* Target-machine dependent code for the NINDY monitor running on the Intel 960
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 2000
   Free Software Foundation, Inc.
   Contributed by Intel Corporation.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Miscellaneous NINDY-dependent routines.
   Some replace macros normally defined in "tm.h".  */

#include "defs.h"
#include "symtab.h"
#include "frame.h"
#include "gdbcore.h"

/* 'start_frame' is a variable in the NINDY runtime startup routine
   that contains the frame pointer of the 'start' routine (the routine
   that calls 'main').  By reading its contents out of remote memory,
   we can tell where the frame chain ends:  backtraces should halt before
   they display this frame.  */

int
nindy_frame_chain_valid (CORE_ADDR chain, struct frame_info *curframe)
{
  struct symbol *sym;
  struct minimal_symbol *msymbol;

  /* crtnindy.o is an assembler module that is assumed to be linked
   * first in an i80960 executable.  It contains the true entry point;
   * it performs startup up initialization and then calls 'main'.
   *
   * 'sf' is the name of a variable in crtnindy.o that is set
   *      during startup to the address of the first frame.
   *
   * 'a' is the address of that variable in 80960 memory.
   */
  static char sf[] = "start_frame";
  CORE_ADDR a;


  chain &= ~0x3f;		/* Zero low 6 bits because previous frame pointers
				   contain return status info in them.  */
  if (chain == 0)
    {
      return 0;
    }

  sym = lookup_symbol (sf, 0, VAR_NAMESPACE, (int *) NULL,
		       (struct symtab **) NULL);
  if (sym != 0)
    {
      a = SYMBOL_VALUE (sym);
    }
  else
    {
      msymbol = lookup_minimal_symbol (sf, NULL, NULL);
      if (msymbol == NULL)
	return 0;
      a = SYMBOL_VALUE_ADDRESS (msymbol);
    }

  return (chain != read_memory_integer (a, 4));
}
