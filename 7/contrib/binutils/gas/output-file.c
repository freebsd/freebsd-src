/* output-file.c -  Deal with the output file
   Copyright 1987, 1990, 1991, 1992, 1993, 1994, 1996, 1998, 1999, 2001, 2003
   Free Software Foundation, Inc.

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
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include <stdio.h>

#include "as.h"

#include "output-file.h"

#ifdef BFD_HEADERS
#define USE_BFD
#endif

#ifdef BFD_ASSEMBLER
#define USE_BFD
#ifndef TARGET_MACH
#define TARGET_MACH 0
#endif
#endif

#ifdef USE_BFD
#include "bfd.h"
bfd *stdoutput;

void
output_file_create (char *name)
{
  if (name[0] == '-' && name[1] == '\0')
    as_fatal (_("can't open a bfd on stdout %s"), name);

  else if (!(stdoutput = bfd_openw (name, TARGET_FORMAT)))
    {
      as_perror (_("FATAL: can't create %s"), name);
      exit (EXIT_FAILURE);
    }

  bfd_set_format (stdoutput, bfd_object);
#ifdef BFD_ASSEMBLER
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, TARGET_MACH);
#endif
  if (flag_traditional_format)
    stdoutput->flags |= BFD_TRADITIONAL_FORMAT;
}

void
output_file_close (char *filename)
{
#ifdef BFD_ASSEMBLER
  /* Close the bfd.  */
  if (bfd_close (stdoutput) == 0)
    {
      bfd_perror (filename);
      as_perror (_("FATAL: can't close %s\n"), filename);
      exit (EXIT_FAILURE);
    }
#else
  /* Close the bfd without getting bfd to write out anything by itself.  */
  if (bfd_close_all_done (stdoutput) == 0)
    {
      as_perror (_("FATAL: can't close %s\n"), filename);
      exit (EXIT_FAILURE);
    }
#endif
  stdoutput = NULL;		/* Trust nobody!  */
}

#ifndef BFD_ASSEMBLER
void
output_file_append (char *where ATTRIBUTE_UNUSED,
		    long length ATTRIBUTE_UNUSED,
		    char *filename ATTRIBUTE_UNUSED)
{
  abort ();
}
#endif

#else

static FILE *stdoutput;

void
output_file_create (char *name)
{
  if (name[0] == '-' && name[1] == '\0')
    {
      stdoutput = stdout;
      return;
    }

  stdoutput = fopen (name, FOPEN_WB);
  if (stdoutput == NULL)
    {
#ifdef BFD_ASSEMBLER
      bfd_set_error (bfd_error_system_call);
#endif
      as_perror (_("FATAL: can't create %s"), name);
      exit (EXIT_FAILURE);
    }
}

void
output_file_close (char *filename)
{
  if (EOF == fclose (stdoutput))
    {
#ifdef BFD_ASSEMBLER
      bfd_set_error (bfd_error_system_call);
#endif
      as_perror (_("FATAL: can't close %s"), filename);
      exit (EXIT_FAILURE);
    }

  /* Trust nobody!  */
  stdoutput = NULL;
}

void
output_file_append (char * where, long length, char * filename)
{
  for (; length; length--, where++)
    {
      (void) putc (*where, stdoutput);

      if (ferror (stdoutput))
	{
#ifdef BFD_ASSEMBLER
	  bfd_set_error (bfd_error_system_call);
#endif
	  as_perror (_("Failed to emit an object byte"), filename);
	  as_fatal (_("can't continue"));
	}
    }
}

#endif

