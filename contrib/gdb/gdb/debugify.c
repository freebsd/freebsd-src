
/* Debug macros for developemnt.
   Copyright 1997
   Free Software Foundation, Inc.

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

#define DEBUGIFY
#include "debugify.h"
#include "config.h"

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#define REDIRECT

static FILE *fout = NULL;
static char fname[128];
static int file_cnt = 0;	/* count number of open files */

void 
puts_dbg (data)
  const char *data;
{
  FILE *fdbg;

  fdbg = fopen ("dbg.log", "a+");
  if (fdbg == NULL)
    return;
  fprintf (fdbg, data);
  fclose (fdbg);
}

/* Can't easily get the message back to gdb... write to a log instead. */
void 
fputs_dbg (data, fakestream)
  const char *data;
  FILE *fakestream;
{
#ifdef REDIRECT
  puts_dbg (data);
#else /* REDIRECT */

  if (fakestream == stdout || fakestream == stderr || fakestream == NULL)
    {
      if (fout == NULL)
	{
	  for (file_cnt = 0; file_cnt < 20; file_cnt++)
	    {
	      sprintf (fname, "dbg%d.log", file_cnt);
	      if ((fout = fopen (fname), "r") != NULL)
		fclose (fout);
	      else
		break;
	    }
	  fout = fopen (fname, "w");
	  if (fout == NULL)
	    return;
	}
      fakestream = fout;
    }
  fprintf (fakestream, data);
  fflush (fakestream);
#endif /* REDIRECT */
}

void 
#ifdef ANSI_PROTOTYPES
printf_dbg (const char *format,...)
#else
printf_dbg (va_alist) 
     va_dcl
#endif
{
  va_list args;
  char buf[256];
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  char *format;

  va_start (args);
  format = va_arg (args, char *); 
#endif
  vsprintf (buf, format, args);
  puts_dbg (buf);
  va_end (args);
}
