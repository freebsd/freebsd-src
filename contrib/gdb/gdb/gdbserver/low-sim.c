/* Low level interface to simulators, for the remote server for GDB.
   Copyright (C) 1995, 1996 Free Software Foundation, Inc.

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
#include "bfd.h"
#include "server.h"
#include "callback.h"   /* GDB simulator callback interface */
#include "remote-sim.h" /* GDB simulator interface */

extern int remote_debug;

extern host_callback default_callback;	/* in sim/common/callback.c */

char registers[REGISTER_BYTES] __attribute__ ((aligned));

int target_byte_order;	/* used by simulator */

/* We record the result of sim_open so we can pass it
   back to the other sim_foo routines.  */
static SIM_DESC gdbsim_desc = 0;

/* This version of "load" should be usable for any simulator that
   does not support loading itself.  */

static void
generic_load (loadfile_bfd)
    bfd *loadfile_bfd;
{
  asection *s;

  for (s = loadfile_bfd->sections; s; s = s->next) 
    {
      if (s->flags & SEC_LOAD) 
	{
	  bfd_size_type size;

	  size = bfd_get_section_size_before_reloc (s);
	  if (size > 0)
	    {
	      char *buffer;
	      bfd_vma lma;	/* use load address, not virtual address */

	      buffer = xmalloc (size);
	      lma = s->lma;

	      /* Is this really necessary?  I guess it gives the user something
		 to look at during a long download.  */
	      printf ("Loading section %s, size 0x%lx lma 0x%lx\n",
		      bfd_get_section_name (loadfile_bfd, s),
		      (unsigned long) size,
		      (unsigned long) lma); /* chops high 32 bits.  FIXME!! */

	      bfd_get_section_contents (loadfile_bfd, s, buffer, 0, size);

	      write_inferior_memory (lma, buffer, size);
	      free (buffer);
	    }
	}
    }

  printf ("Start address 0x%lx\n",
	  (unsigned long)loadfile_bfd->start_address);

  /* We were doing this in remote-mips.c, I suspect it is right
     for other targets too.  */
  /* write_pc (loadfile_bfd->start_address); */	/* FIXME!! */
}

int
create_inferior (program, argv)
     char *program;
     char **argv;
{
  bfd *abfd;
  int pid = 0;
#ifdef TARGET_BYTE_ORDER_SELECTABLE
  char **new_argv;
  int nargs;
#endif

  abfd = bfd_openr (program, 0);
  if (!abfd) 
    {
      fprintf (stderr, "gdbserver: can't open %s: %s\n", 
	       program, bfd_errmsg (bfd_get_error ()));
      exit (1);
    }

  if (!bfd_check_format (abfd, bfd_object))
    {
      fprintf (stderr, "gdbserver: unknown load format for %s: %s\n",
	       program, bfd_errmsg (bfd_get_error ()));
      exit (1);
    }

#ifdef TARGET_BYTE_ORDER_SELECTABLE
  /* Add "-E big" or "-E little" to the argument list depending on the
     endianness of the program to be loaded.  */
  for (nargs = 0; argv[nargs] != NULL; nargs++)		/* count the args */
    ;
  new_argv = alloca (sizeof (char *) * (nargs + 3));	/* allocate new args */
  for (nargs = 0; argv[nargs] != NULL; nargs++)		/* copy old to new */
    new_argv[nargs] = argv[nargs];
  new_argv[nargs] = "-E";
  new_argv[nargs + 1] = bfd_big_endian (abfd) ? "big" : "little";
  new_argv[nargs + 2] = NULL;
  argv = new_argv;
#endif

  /* Create an instance of the simulator.  */
  default_callback.init (&default_callback);
  gdbsim_desc = sim_open (SIM_OPEN_STANDALONE, &default_callback, abfd, argv);
  if (gdbsim_desc == 0)
    exit (1);

  /* Load the program into the simulator.  */
  if (abfd)
    if (sim_load (gdbsim_desc, program, NULL, 0) == SIM_RC_FAIL)
      generic_load (abfd);

  /* Create an inferior process in the simulator.  This initializes SP.  */
  sim_create_inferior (gdbsim_desc, abfd, argv, /* env */ NULL);
  sim_resume (gdbsim_desc, 1, 0);	/* execute one instr */
  return pid;
}

/* Kill the inferior process.  Make us have no inferior.  */

void
kill_inferior ()
{
  sim_close (gdbsim_desc, 0);
  default_callback.shutdown (&default_callback);
}

/* Fetch one register.  */

static void
fetch_register (regno)
     int regno;
{
  sim_fetch_register (gdbsim_desc, regno, &registers[REGISTER_BYTE (regno)],
		      REGISTER_RAW_SIZE (regno));
}

/* Fetch all registers, or just one, from the child process.  */

void
fetch_inferior_registers (regno)
     int regno;
{
  if (regno == -1 || regno == 0)
    for (regno = 0; regno < NUM_REGS/*-NUM_FREGS*/; regno++)
      fetch_register (regno);
  else
    fetch_register (regno);
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  if (regno  == -1) 
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	store_inferior_registers (regno);
    }
  else
    sim_store_register (gdbsim_desc, regno, &registers[REGISTER_BYTE (regno)],
			REGISTER_RAW_SIZE (regno));
}

/* Return nonzero if the given thread is still alive.  */
int
mythread_alive (pid)
     int pid;
{
  return 1;
}

/* Wait for process, returns status */

unsigned char
mywait (status)
     char *status;
{
  int sigrc;
  enum sim_stop reason;

  sim_stop_reason (gdbsim_desc, &reason, &sigrc);
  switch (reason)
    {
    case sim_exited:
      if (remote_debug)
	printf ("\nChild exited with retcode = %x \n", sigrc);
      *status = 'W';
      return sigrc;

#if 0
    case sim_stopped:
      if (remote_debug)
	printf ("\nChild terminated with signal = %x \n", sigrc);
      *status = 'X';
      return sigrc;
#endif

    default:   /* should this be sim_signalled or sim_stopped?  FIXME!! */
      if (remote_debug)
	printf ("\nChild received signal = %x \n", sigrc);
      fetch_inferior_registers (0);
      *status = 'T';
      return (unsigned char) sigrc;
    }
}

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

void
myresume (step, signo)
     int step;
     int signo;
{
  /* Should be using target_signal_to_host() or signal numbers in target.h
     to convert GDB signal number to target signal number.  */
  sim_resume (gdbsim_desc, step, signo);
}

/* Copy LEN bytes from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.  */

void
read_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  sim_read (gdbsim_desc, memaddr, myaddr, len);
}

/* Copy LEN bytes of data from debugger memory at MYADDR
   to inferior's memory at MEMADDR.
   On failure (cannot write the inferior)
   returns the value of errno.  */

int
write_inferior_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  sim_write (gdbsim_desc, memaddr, myaddr, len);  /* should check for error.  FIXME!! */
  return 0;
}

#if 0
void
initialize ()
{
  inferior_pid = 0;
}

int
have_inferior_p ()
{
  return inferior_pid != 0;
}
#endif
