/* Target-vector operations for controlling Unix child processes, for GDB.
   Copyright 1990, 1991, 1992 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "wait.h"
#include "gdbcore.h"

#include <signal.h>

static void
child_prepare_to_store PARAMS ((void));

#ifndef CHILD_WAIT
static int
child_wait PARAMS ((int, int *));
#endif /* CHILD_WAIT */

static void
child_open PARAMS ((char *, int));

static void
child_files_info PARAMS ((struct target_ops *));

static void
child_detach PARAMS ((char *, int));

static void
child_attach PARAMS ((char *, int));

static void
ptrace_me PARAMS ((void));

static void
ptrace_him PARAMS ((int));

static void
child_create_inferior PARAMS ((char *, char *, char **));

static void
child_mourn_inferior PARAMS ((void));

static int
child_can_run PARAMS ((void));

extern char **environ;

/* Forward declaration */
extern struct target_ops child_ops;

#ifndef CHILD_WAIT

/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer STATUS.  */

static int
child_wait (pid, status)
     int pid;
     int *status;
{
  int save_errno;

  do {
    if (attach_flag)
      set_sigint_trap();	/* Causes SIGINT to be passed on to the
				   attached process. */
    pid = wait (status);
    save_errno = errno;

    if (attach_flag)
      clear_sigint_trap();

    if (pid == -1)
      {
	if (save_errno == EINTR)
	  continue;
	fprintf (stderr, "Child process unexpectedly missing: %s.\n",
		 safe_strerror (save_errno));
	*status = 42;	/* Claim it exited with signal 42 */
        return -1;
      }
  } while (pid != inferior_pid); /* Some other child died or stopped */
  return pid;
}
#endif /* CHILD_WAIT */

/* Attach to process PID, then initialize for debugging it.  */

static void
child_attach (args, from_tty)
     char *args;
     int from_tty;
{
  char *exec_file;
  int pid;

  if (!args)
    error_no_arg ("process-id to attach");

#ifndef ATTACH_DETACH
  error ("Can't attach to a process on this machine.");
#else
  pid = atoi (args);

  if (pid == getpid())		/* Trying to masturbate? */
    error ("I refuse to debug myself!");

  if (from_tty)
    {
      exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf ("Attaching to program `%s', %s\n", exec_file, target_pid_to_str (pid));
      else
	printf ("Attaching to %s\n", target_pid_to_str (pid));

      fflush (stdout);
    }

  attach (pid);
  inferior_pid = pid;
  push_target (&child_ops);
#endif  /* ATTACH_DETACH */
}


/* Take a program previously attached to and detaches it.
   The program resumes execution and will no longer stop
   on signals, etc.  We'd better not have left any breakpoints
   in the program or it'll die when it hits one.  For this
   to work, it may be necessary for the process to have been
   previously attached.  It *might* work if the program was
   started via the normal ptrace (PTRACE_TRACEME).  */

static void
child_detach (args, from_tty)
     char *args;
     int from_tty;
{
  int siggnal = 0;

#ifdef ATTACH_DETACH
  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf ("Detaching from program: %s %s\n", exec_file,
	      target_pid_to_str (inferior_pid));
      fflush (stdout);
    }
  if (args)
    siggnal = atoi (args);
  
  detach (siggnal);
  inferior_pid = 0;
  unpush_target (&child_ops);		/* Pop out of handling an inferior */
#else
    error ("This version of Unix does not support detaching a process.");
#endif
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
child_prepare_to_store ()
{
#ifdef CHILD_PREPARE_TO_STORE
  CHILD_PREPARE_TO_STORE ();
#endif
}

/* Print status information about what we're accessing.  */

static void
child_files_info (ignore)
     struct target_ops *ignore;
{
  printf ("\tUsing the running image of %s %s.\n",
	  attach_flag? "attached": "child", target_pid_to_str (inferior_pid));
}

/* ARGSUSED */
static void
child_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a Unix child process.");
}

/* Stub function which causes the inferior that runs it, to be ptrace-able
   by its parent process.  */

static void
ptrace_me ()
{
  /* "Trace me, Dr. Memory!" */
  call_ptrace (0, 0, (PTRACE_ARG3_TYPE) 0, 0);
}

/* Stub function which causes the GDB that runs it, to start ptrace-ing
   the child process.  */

static void
ptrace_him (pid)
     int pid;
{
  push_target (&child_ops);
}

/* Start an inferior Unix child process and sets inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().  */

static void
child_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  fork_inferior (exec_file, allargs, env, ptrace_me, ptrace_him);
  /* We are at the first instruction we care about.  */
  /* Pedal to the metal... */
  proceed ((CORE_ADDR) -1, 0, 0);
}

static void
child_mourn_inferior ()
{
  unpush_target (&child_ops);
  generic_mourn_inferior ();
}

static int
child_can_run ()
{
  return(1);
}

struct target_ops child_ops = {
  "child",			/* to_shortname */
  "Unix child process",		/* to_longname */
  "Unix child process (started by the \"run\" command).",	/* to_doc */
  child_open,			/* to_open */
  0,				/* to_close */
  child_attach,			/* to_attach */
  child_detach, 		/* to_detach */
  child_resume,			/* to_resume */
  child_wait,			/* to_wait */
  fetch_inferior_registers,	/* to_fetch_registers */
  store_inferior_registers,	/* to_store_registers */
  child_prepare_to_store,	/* to_prepare_to_store */
  child_xfer_memory,		/* to_xfer_memory */
  child_files_info,		/* to_files_info */
  memory_insert_breakpoint,	/* to_insert_breakpoint */
  memory_remove_breakpoint,	/* to_remove_breakpoint */
  terminal_init_inferior,	/* to_terminal_init */
  terminal_inferior, 		/* to_terminal_inferior */
  terminal_ours_for_output,	/* to_terminal_ours_for_output */
  terminal_ours,		/* to_terminal_ours */
  child_terminal_info,		/* to_terminal_info */
  kill_inferior,		/* to_kill */
  0,				/* to_load */
  0,				/* to_lookup_symbol */
  child_create_inferior,	/* to_create_inferior */
  child_mourn_inferior,		/* to_mourn_inferior */
  child_can_run,		/* to_can_run */
  0, 				/* to_notice_signals */
  process_stratum,		/* to_stratum */
  0,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  0,				/* sections */
  0,				/* sections_end */
  OPS_MAGIC			/* to_magic */
};

void
_initialize_inftarg ()
{
  add_target (&child_ops);
}
