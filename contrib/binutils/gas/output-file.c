/* output-file.c -  Deal with the output file
   Copyright 1987, 1990, 1991, 1992, 1993, 1994, 1996, 1998, 1999, 2001,
   2003, 2004, 2005 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include <stdio.h>

#include "as.h"

#include "output-file.h"

#ifndef TARGET_MACH
#define TARGET_MACH 0
#endif

#include "bfd.h"
bfd *stdoutput;

void
output_file_create (char *name)
{
  if (name[0] == '-' && name[1] == '\0')
    as_fatal (_("can't open a bfd on stdout %s"), name);

  else if (!(stdoutput = bfd_openw (name, TARGET_FORMAT)))
    {
      if (bfd_get_error () == bfd_error_invalid_target)
	as_perror (_("Selected target format '%s' unknown"), TARGET_FORMAT);
      else
	as_perror (_("FATAL: can't create %s"), name);
      exit (EXIT_FAILURE);
    }

  bfd_set_format (stdoutput, bfd_object);
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, TARGET_MACH);
  if (flag_traditional_format)
    stdoutput->flags |= BFD_TRADITIONAL_FORMAT;
}

void
output_file_close (char *filename)
{
  /* Close the bfd.  */
  if (bfd_close (stdoutput) == 0)
    {
      bfd_perror (filename);
      as_perror (_("FATAL: can't close %s\n"), filename);
      exit (EXIT_FAILURE);
    }
  stdoutput = NULL;		/* Trust nobody!  */
}
