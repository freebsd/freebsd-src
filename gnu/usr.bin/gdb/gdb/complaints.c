/* Support for complaint handling during symbol reading in GDB.
   Copyright (C) 1990, 1991, 1992  Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "complaints.h"
#include "gdbcmd.h"
#include <varargs.h>

/* Structure to manage complaints about symbol file contents.  */

struct complaint complaint_root[1] = {
  {
    (char *) NULL,	/* Complaint message */
    0,			/* Complaint counter */
    complaint_root	/* Next complaint. */
  }
};

/* How many complaints about a particular thing should be printed before
   we stop whining about it?  Default is no whining at all, since so many
   systems have ill-constructed symbol files.  */

static unsigned int stop_whining = 0;

/* Should each complaint be self explanatory, or should we assume that
   a series of complaints is being produced? 
   case 0:  self explanatory message.
   case 1:  First message of a series that must start off with explanation.
   case 2:  Subsequent message, when user already knows we are reading
            symbols and we can just state our piece.  */

static int complaint_series = 0;

/* External variables and functions referenced. */

extern int info_verbose;


/* Functions to handle complaints during symbol reading.  */

/* Print a complaint about the input symbols, and link the complaint block
   into a chain for later handling.  */

/* VARARGS */
void
complain (va_alist)
     va_dcl
{
  va_list args;
  struct complaint *complaint;

  va_start (args);
  complaint = va_arg (args, struct complaint *);
  complaint -> counter++;
  if (complaint -> next == NULL)
    {
      complaint -> next = complaint_root -> next;
      complaint_root -> next = complaint;
    }
  if (complaint -> counter > stop_whining)
    {
      return;
    }
  wrap_here ("");

  switch (complaint_series + (info_verbose << 1))
    {

      /* Isolated messages, must be self-explanatory.  */
      case 0:
        begin_line ();
        puts_filtered ("During symbol reading, ");
	wrap_here ("");
	vprintf_filtered (complaint -> message, args);
	puts_filtered (".\n");
	break;

      /* First of a series, without `set verbose'.  */
      case 1:
        begin_line ();
	puts_filtered ("During symbol reading...");
	vprintf_filtered (complaint -> message, args);
	puts_filtered ("...");
	wrap_here ("");
	complaint_series++;
	break;

      /* Subsequent messages of a series, or messages under `set verbose'.
	 (We'll already have produced a "Reading in symbols for XXX..."
	 message and will clean up at the end with a newline.)  */
      default:
	vprintf_filtered (complaint -> message, args);
	puts_filtered ("...");
	wrap_here ("");
    }
  /* If GDB dumps core, we'd like to see the complaints first.  Presumably
     GDB will not be sending so many complaints that this becomes a
     performance hog.  */
  gdb_flush (gdb_stdout);
  va_end (args);
}

/* Clear out all complaint counters that have ever been incremented.
   If sym_reading is 1, be less verbose about successive complaints,
   since the messages are appearing all together during a command that
   reads symbols (rather than scattered around as psymtabs get fleshed
   out into symtabs at random times).  If noisy is 1, we are in a
   noisy symbol reading command, and our caller will print enough
   context for the user to figure it out.  */

void
clear_complaints (sym_reading, noisy)
     int sym_reading;
     int noisy;
{
  struct complaint *p;

  for (p = complaint_root -> next; p != complaint_root; p = p -> next)
    {
      p -> counter = 0;
    }

  if (!sym_reading && !noisy && complaint_series > 1)
    {
      /* Terminate previous series, since caller won't.  */
      puts_filtered ("\n");
    }

  complaint_series = sym_reading ? 1 + noisy : 0;
}

void
_initialize_complaints ()
{
  add_show_from_set
    (add_set_cmd ("complaints", class_support, var_zinteger,
		  (char *) &stop_whining,
		  "Set max number of complaints about incorrect symbols.",
		  &setlist),
     &showlist);

}
