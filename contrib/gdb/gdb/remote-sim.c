/* Generic remote debugging interface for simulators.
   Copyright 1993, 1994 Free Software Foundation, Inc.
   Contributed by Cygnus Support.
   Steve Chamberlain (sac@cygnus.com).

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
#include "inferior.h"
#include "wait.h"
#include "value.h"
#include "gdb_string.h"
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include "terminal.h"
#include "target.h"
#include "gdbcore.h"
#include "remote-sim.h"
#include "remote-utils.h"
#include "callback.h"

/* Naming convention:

   sim_* are the interface to the simulator (see remote-sim.h).
   sim_callback_* are the stuff which the simulator can see inside GDB.
   gdbsim_* are stuff which is internal to gdb.  */

/* Forward data declarations */
extern struct target_ops gdbsim_ops;

static int program_loaded = 0;

static void
dump_mem (buf, len)
     char *buf;
     int len;
{
  if (len <= 8)
    {
      if (len == 8 || len == 4)
	{
	  long l[2];
	  memcpy (l, buf, len);
	  printf_filtered ("\t0x%x", l[0]);
	  printf_filtered (len == 8 ? " 0x%x\n" : "\n", l[1]);
	}
      else
	{
	  int i;
	  printf_filtered ("\t");
	  for (i = 0; i < len; i++)
	    printf_filtered ("0x%x ", buf[i]);
	  printf_filtered ("\n");
	}
    }
}

static void
gdbsim_fetch_register (regno)
int regno;
{
  if (regno == -1) 
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	gdbsim_fetch_register (regno);
    }
  else
    {
      char buf[MAX_REGISTER_RAW_SIZE];

      sim_fetch_register (regno, buf);
      supply_register (regno, buf);
      if (sr_get_debug ())
	{
	  printf_filtered ("gdbsim_fetch_register: %d", regno);
	  /* FIXME: We could print something more intelligible.  */
	  dump_mem (buf, REGISTER_RAW_SIZE (regno));
	}
    }
}


static void
gdbsim_store_register (regno)
int regno;
{
  if (regno  == -1) 
    {
      for (regno = 0; regno < NUM_REGS; regno++)
	gdbsim_store_register (regno);
    }
  else
    {
      /* FIXME: Until read_register() returns LONGEST, we have this.  */
      char tmp[MAX_REGISTER_RAW_SIZE];
      read_register_gen (regno, tmp);
      sim_store_register (regno, tmp);
      if (sr_get_debug ())
	{
	  printf_filtered ("gdbsim_store_register: %d", regno);
	  /* FIXME: We could print something more intelligible.  */
	  dump_mem (tmp, REGISTER_RAW_SIZE (regno));
	}
    }
}

/* Kill the running program.  This may involve closing any open files
   and releasing other resources acquired by the simulated program.  */

static void
gdbsim_kill ()
{
  if (sr_get_debug ())
    printf_filtered ("gdbsim_kill\n");

  sim_kill ();	/* close fd's, remove mappings */
  inferior_pid = 0;
}

/* Load an executable file into the target process.  This is expected to
   not only bring new code into the target process, but also to update
   GDB's symbol tables to match.  */

static void
gdbsim_load (prog, fromtty)
     char *prog;
     int fromtty;
{
  if (sr_get_debug ())
    printf_filtered ("gdbsim_load: prog \"%s\"\n", prog);

  inferior_pid = 0;

  /* This must be done before calling gr_load_image.  */
  program_loaded = 1;

  if (sim_load (prog, fromtty) != 0)
    generic_load (prog, fromtty);
}


/* Start an inferior process and set inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().
   On VxWorks and various standalone systems, we ignore exec_file.  */
/* This is called not only when we first attach, but also when the
   user types "run" after having attached.  */

static void
gdbsim_create_inferior (exec_file, args, env)
     char *exec_file;
     char *args;
     char **env;
{
  int len;
  char *arg_buf,**argv;
  CORE_ADDR entry_pt;

  if (! program_loaded)
    error ("No program loaded.");

  if (sr_get_debug ())
    printf_filtered ("gdbsim_create_inferior: exec_file \"%s\", args \"%s\"\n",
      exec_file, args);

  if (exec_file == 0 || exec_bfd == 0)
   error ("No exec file specified.");

  entry_pt = (CORE_ADDR) bfd_get_start_address (exec_bfd);

  gdbsim_kill (NULL, NULL);	 
  remove_breakpoints ();
  init_wait_for_inferior ();

  len = 5 + strlen (exec_file) + 1 + strlen (args) + 1 + /*slop*/ 10;
  arg_buf = (char *) alloca (len);
  arg_buf[0] = '\0';
  strcat (arg_buf, exec_file);
  strcat (arg_buf, " ");
  strcat (arg_buf, args);
  argv = buildargv (arg_buf);
  make_cleanup (freeargv, (char *) argv);
  sim_create_inferior (entry_pt, argv, env);

  inferior_pid = 42;
  insert_breakpoints ();	/* Needed to get correct instruction in cache */
  proceed (entry_pt, TARGET_SIGNAL_DEFAULT, 0);
}

/* The open routine takes the rest of the parameters from the command,
   and (if successful) pushes a new target onto the stack.
   Targets should supply this routine, if only to provide an error message.  */
/* Called when selecting the simulator. EG: (gdb) target sim name.  */

static void
gdbsim_open (args, from_tty)
     char *args;
     int from_tty;
{
  if (sr_get_debug ())
    printf_filtered ("gdbsim_open: args \"%s\"\n", args ? args : "(null)");

  sim_set_callbacks (&default_callback);
  default_callback.init (&default_callback);

  sim_open (args);

  push_target (&gdbsim_ops);
  target_fetch_registers (-1);
  printf_filtered ("Connected to the simulator.\n");
}

/* Does whatever cleanup is required for a target that we are no longer
   going to be calling.  Argument says whether we are quitting gdb and
   should not get hung in case of errors, or whether we want a clean
   termination even if it takes a while.  This routine is automatically
   always called just before a routine is popped off the target stack.
   Closing file descriptors and freeing memory are typical things it should
   do.  */
/* Close out all files and local state before this target loses control. */

static void
gdbsim_close (quitting)
     int quitting;
{
  if (sr_get_debug ())
    printf_filtered ("gdbsim_close: quitting %d\n", quitting);

  program_loaded = 0;

  sim_close (quitting);
}

/* Takes a program previously attached to and detaches it.
   The program may resume execution (some targets do, some don't) and will
   no longer stop on signals, etc.  We better not have left any breakpoints
   in the program or it'll die when it hits one.  ARGS is arguments
   typed by the user (e.g. a signal to send the process).  FROM_TTY
   says whether to be verbose or not.  */
/* Terminate the open connection to the remote debugger.
   Use this when you want to detach and do something else with your gdb.  */

static void
gdbsim_detach (args,from_tty)
     char *args;
     int from_tty;
{
  if (sr_get_debug ())
    printf_filtered ("gdbsim_detach: args \"%s\"\n", args);

  pop_target ();		/* calls gdbsim_close to do the real work */
  if (from_tty)
    printf_filtered ("Ending simulator %s debugging\n", target_shortname);
}
 
/* Resume execution of the target process.  STEP says whether to single-step
   or to run free; SIGGNAL is the signal value (e.g. SIGINT) to be given
   to the target, or zero for no signal.  */

static void
gdbsim_resume (pid, step, siggnal)
     int pid, step;
     enum target_signal siggnal;
{
  if (sr_get_debug ())
    printf_filtered ("gdbsim_resume: step %d, signal %d\n", step, siggnal);

  sim_resume (step, target_signal_to_host (siggnal));
}

/* Wait for inferior process to do something.  Return pid of child,
   or -1 in case of error; store status through argument pointer STATUS,
   just as `wait' would.  */

static int
gdbsim_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  int sigrc;
  enum sim_stop reason;

  if (sr_get_debug ())
    printf_filtered ("gdbsim_wait\n");

  sim_stop_reason (&reason, &sigrc);
  switch (reason)
    {
    case sim_exited:
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = sigrc;
      break;
    case sim_stopped:
      status->kind = TARGET_WAITKIND_STOPPED;
      /* The signal in sigrc is a host signal.  That probably
	 should be fixed.  */
      status->value.sig = target_signal_from_host (sigrc);
      break;
    case sim_signalled:
      status->kind = TARGET_WAITKIND_SIGNALLED;
      /* The signal in sigrc is a host signal.  That probably
	 should be fixed.  */
      status->value.sig = target_signal_from_host (sigrc);
      break;
    }

  return inferior_pid;
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
gdbsim_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static int
gdbsim_xfer_inferior_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;			/* ignored */
{
  if (! program_loaded)
    error ("No program loaded.");

  if (sr_get_debug ())
    {
      printf_filtered ("gdbsim_xfer_inferior_memory: myaddr 0x%x, memaddr 0x%x, len %d, write %d\n",
		       myaddr, memaddr, len, write);
      if (sr_get_debug () && write)
	dump_mem(myaddr, len);
    }

  if (write)
    {
      len = sim_write (memaddr, myaddr, len);
    }
  else 
    {
      len = sim_read (memaddr, myaddr, len);
      if (sr_get_debug () && len > 0)
	dump_mem(myaddr, len);
    } 
  return len;
}

static void
gdbsim_files_info (target)
     struct target_ops *target;
{
  char *file = "nothing";

  if (exec_bfd)
    file = bfd_get_filename (exec_bfd);

  if (sr_get_debug ())
    printf_filtered ("gdbsim_files_info: file \"%s\"\n", file);

  if (exec_bfd)
    {
      printf_filtered ("\tAttached to %s running program %s\n",
		       target_shortname, file);
      sim_info (0);
    }
}

/* Clear the simulator's notion of what the break points are.  */

static void
gdbsim_mourn_inferior () 
{ 
  if (sr_get_debug ())
    printf_filtered ("gdbsim_mourn_inferior:\n");

  remove_breakpoints ();
  generic_mourn_inferior ();
}

/* Put a command string, in args, out to MONITOR.  Output from MONITOR
   is placed on the users terminal until the prompt is seen. FIXME: We
   read the characters ourseleves here cause of a nasty echo.  */

static void
simulator_command (args, from_tty)
     char *args;
     int from_tty;
{
  sim_do_command (args);
}

/* Define the target subroutine names */

struct target_ops gdbsim_ops = {
  "sim",			/* to_shortname */
  "simulator",			/* to_longname */
  "Use the compiled-in simulator.",  /* to_doc */
  gdbsim_open,			/* to_open */
  gdbsim_close,			/* to_close */
  NULL,				/* to_attach */
  gdbsim_detach,		/* to_detach */
  gdbsim_resume,		/* to_resume */
  gdbsim_wait,			/* to_wait */
  gdbsim_fetch_register,	/* to_fetch_registers */
  gdbsim_store_register,	/* to_store_registers */
  gdbsim_prepare_to_store,	/* to_prepare_to_store */
  gdbsim_xfer_inferior_memory,	/* to_xfer_memory */
  gdbsim_files_info,		/* to_files_info */
  memory_insert_breakpoint,	/* to_insert_breakpoint */
  memory_remove_breakpoint,	/* to_remove_breakpoint */
  NULL,				/* to_terminal_init */
  NULL,				/* to_terminal_inferior */
  NULL,				/* to_terminal_ours_for_output */
  NULL,				/* to_terminal_ours */
  NULL,				/* to_terminal_info */
  gdbsim_kill,			/* to_kill */
  gdbsim_load,			/* to_load */
  NULL,				/* to_lookup_symbol */
  gdbsim_create_inferior,	/* to_create_inferior */ 
  gdbsim_mourn_inferior,	/* to_mourn_inferior */
  0,				/* to_can_run */
  0,				/* to_notice_signals */
  0,				/* to_thread_alive */
  0,				/* to_stop */
  process_stratum,		/* to_stratum */
  NULL,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  NULL,				/* sections */
  NULL,				/* sections_end */
  OPS_MAGIC,			/* to_magic */
};

void
_initialize_remote_sim ()
{
  add_target (&gdbsim_ops);

  add_com ("sim <command>", class_obscure, simulator_command,
	   "Send a command to the simulator."); 
}
