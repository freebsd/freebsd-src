/* output-file.c -  Deal with the output file
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Confines all details of emitting object bytes to this module.
 * All O/S specific crocks should live here.
 * What we lose in "efficiency" we gain in modularity.
 * Note we don't need to #include the "as.h" file. No common coupling!
 */

/* note that we do need config info.  xoxorich. */

#ifndef lint
static char rcsid[] = "$FreeBSD: src/gnu/usr.bin/as/output-file.c,v 1.6 1999/08/27 23:34:20 peter Exp $";
#endif

#include <stdio.h>

#include "as.h"

#include "output-file.h"
#ifdef BFD_HEADERS
#include "bfd.h"
bfd *stdoutput;
void output_file_create(name)
char *name;
{
	if (name[0] == '-' && name[1] == '\0')  {
		as_perror("FATAL: Can't open a bfd on stdout %s ", name);
	}
	else if ( ! (stdoutput = bfd_openw( name, TARGET_FORMAT )) )
	    {
		    as_perror ("FATAL: Can't create %s", name);
		    exit(42);
	    }
	bfd_set_format(stdoutput, bfd_object);
}
/* output_file_create() */


void output_file_close(filename)
char *filename;
{
	/* Close the bfd without getting bfd to write out anything by itself */
	if ( bfd_close_all_done( stdoutput ) == 0 )
	    {
		    as_perror ("FATAL: Can't close %s\n", filename);
		    exit(42);
	    }
	stdoutput = NULL;		/* Trust nobody! */
}				/* output_file_close() */

void output_file_append(where, length, filename)
char *where;
long length;
char *filename;
{
	abort(); /* Never do this */
}

#else

static FILE *stdoutput;

void output_file_create(name)
char *name;
{
	if (name[0] == '-' && name[1] == '\0')
	    stdoutput=stdout;
	else if (!(stdoutput = fopen(name, "wb"))) {
		as_perror("FATAL: Can't create %s", name);
		exit(42);
	}
} /* output_file_create() */



void output_file_close(filename)
char *filename;
{
	if (EOF == fclose(stdoutput)) {
		as_perror ("FATAL: Can't close %s", filename);
		exit(42);
	}
	stdoutput = NULL;		/* Trust nobody! */
} /* output_file_close() */

void output_file_append(where, length, filename)
char *where;
long length;
char *filename;
{

	for (; length; length--, where++) {
		(void) putc(*where, stdoutput);
		if (ferror(stdoutput))
		    /* if ( EOF == (putc( *where, stdoutput )) ) */
		    {
			    as_perror("Failed to emit an object byte", filename);
			    as_fatal("Can't continue");
		    }
	}
} /* output_file_append() */
#endif

/* end of output-file.c */
