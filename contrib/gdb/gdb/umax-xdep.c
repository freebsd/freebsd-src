/* umax host stuff.
   Copyright (C) 1986, 1987, 1989, 1991 Free Software Foundation, Inc.

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

#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "gdbcore.h"
#include <sys/ptrace.h>
#define PTRACE_ATTACH PT_ATTACH
#define PTRACE_DETACH PT_FREEPROC

#include <sys/file.h>
#include "gdb_stat.h"

/* Work with core dump and executable files, for GDB. 
   This code would be in corefile.c if it weren't machine-dependent. */

void
core_file_command (filename, from_tty)
     char *filename;
     int from_tty;
{
  int val;
  extern char registers[];

  /* Discard all vestiges of any previous core file
     and mark data and stack spaces as empty.  */

  if (corefile)
    free (corefile);
  corefile = 0;

  if (corechan >= 0)
    close (corechan);
  corechan = -1;

  data_start = 0;
  data_end = 0;
  stack_start = STACK_END_ADDR;
  stack_end = STACK_END_ADDR;

  /* Now, if a new core file was specified, open it and digest it.  */

  if (filename)
    {
      filename = tilde_expand (filename);
      make_cleanup (free, filename);
      
      if (have_inferior_p ())
	error ("To look at a core file, you must kill the program with \"kill\".");
      corechan = open (filename, O_RDONLY, 0);
      if (corechan < 0)
	perror_with_name (filename);
      /* 4.2-style (and perhaps also sysV-style) core dump file.  */
      {
	struct ptrace_user u;
	int reg_offset;

	val = myread (corechan, &u, sizeof u);
	if (val < 0)
	  perror_with_name (filename);
	data_start = exec_data_start;

	data_end = data_start + u.pt_dsize;
	stack_start = stack_end - u.pt_ssize;
	data_offset = sizeof u;
	stack_offset = data_offset + u.pt_dsize;
	reg_offset = 0;

	memcpy (&core_aouthdr, &u.pt_aouthdr, sizeof (AOUTHDR));
	printf_unfiltered ("Core file is from \"%s\".\n", u.pt_comm);
	if (u.pt_signal > 0)
	  printf_unfiltered ("Program terminated with signal %d, %s.\n",
		  u.pt_signal, safe_strsignal (u.pt_signal));

	/* Read the register values out of the core file and store
	   them where `read_register' will find them.  */

	{
	  register int regno;

	  for (regno = 0; regno < NUM_REGS; regno++)
	    {
	      char buf[MAX_REGISTER_RAW_SIZE];

	      val = lseek (corechan, register_addr (regno, reg_offset), 0);
	      if (val < 0)
		perror_with_name (filename);

 	      val = myread (corechan, buf, sizeof buf);
	      if (val < 0)
		perror_with_name (filename);
	      supply_register (regno, buf);
	    }
	}
      }
      if (filename[0] == '/')
	corefile = savestring (filename, strlen (filename));
      else
	{
	  corefile = concat (current_directory, "/", filename, NULL);
	}

      flush_cached_frames ();
      select_frame (get_current_frame (), 0);
      validate_files ();
    }
  else if (from_tty)
    printf_unfiltered ("No core file now.\n");
}
