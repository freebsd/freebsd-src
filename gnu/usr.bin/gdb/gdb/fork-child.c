/* Fork a Unix child process, and set up to debug it, for GDB.
   Copyright 1990, 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
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
#include <string.h>
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "wait.h"
#include "gdbcore.h"
#include "terminal.h"
#include "thread.h"

#include <signal.h>

extern char **environ;

#ifndef SHELL_FILE
#define SHELL_FILE "/bin/sh"
#endif

/* Start an inferior Unix child process and sets inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  SHELL_FILE is the shell file,
   or NULL if we should pick one.  Errors reported with error().  */

void
fork_inferior (exec_file, allargs, env, traceme_fun, init_trace_fun,
	       shell_file)
     char *exec_file;
     char *allargs;
     char **env;
     void (*traceme_fun) PARAMS ((void));
     void (*init_trace_fun) PARAMS ((int));
     char *shell_file;
{
  int pid;
  char *shell_command;
  static char default_shell_file[] = SHELL_FILE;
  int len;
  /* Set debug_fork then attach to the child while it sleeps, to debug. */
  static int debug_fork = 0;
  /* This is set to the result of setpgrp, which if vforked, will be visible
     to you in the parent process.  It's only used by humans for debugging.  */
  static int debug_setpgrp = 657473;
  char **save_our_env;

  /* If no exec file handed to us, get it from the exec-file command -- with
     a good, common error message if none is specified.  */
  if (exec_file == 0)
    exec_file = get_exec_file(1);

  /* The user might want tilde-expansion, and in general probably wants
     the program to behave the same way as if run from
     his/her favorite shell.  So we let the shell run it for us.
     FIXME-maybe, we might want a "set shell" command so the user can change
     the shell from within GDB (if so, change callers which pass in a non-NULL
     shell_file too).  */
  if (shell_file == NULL)
    shell_file = getenv ("SHELL");
  if (shell_file == NULL)
    shell_file = default_shell_file;

  /* Multiplying the length of exec_file by 4 is to account for the fact
     that it may expand when quoted; it is a worst-case number based on
     every character being '.  */
  len = 5 + 4 * strlen (exec_file) + 1 + strlen (allargs) + 1 + /*slop*/ 12;
  /* If desired, concat something onto the front of ALLARGS.
     SHELL_COMMAND is the result.  */
#ifdef SHELL_COMMAND_CONCAT
  shell_command = (char *) alloca (strlen (SHELL_COMMAND_CONCAT) + len);
  strcpy (shell_command, SHELL_COMMAND_CONCAT);
#else
  shell_command = (char *) alloca (len);
  shell_command[0] = '\0';
#endif
  strcat (shell_command, "exec ");

  /* Now add exec_file, quoting as necessary.  */
  {
    char *p;
    int need_to_quote;

    /* Quoting in this style is said to work with all shells.  But csh
       on IRIX 4.0.1 can't deal with it.  So we only quote it if we need
       to.  */
    p = exec_file;
    while (1)
      {
	switch (*p)
	  {
	  case '\'':
	  case '"':
	  case '(':
	  case ')':
	  case '$':
	  case '&':
	  case ';':
	  case '<':
	  case '>':
	  case ' ':
	  case '\n':
	  case '\t':
	    need_to_quote = 1;
	    goto end_scan;

	  case '\0':
	    need_to_quote = 0;
	    goto end_scan;

	  default:
	    break;
	  }
	++p;
      }
  end_scan:
    if (need_to_quote)
      {
	strcat (shell_command, "'");
	for (p = exec_file; *p != '\0'; ++p)
	  {
	    if (*p == '\'')
	      strcat (shell_command, "'\\''");
	    else
	      strncat (shell_command, p, 1);
	  }
	strcat (shell_command, "'");
      }
    else
      strcat (shell_command, exec_file);
  }

  strcat (shell_command, " ");
  strcat (shell_command, allargs);

  /* exec is said to fail if the executable is open.  */
  close_exec_file ();

  /* Retain a copy of our environment variables, since the child will
     replace the value of  environ  and if we're vforked, we have to 
     restore it.  */
  save_our_env = environ;

  /* Tell the terminal handling subsystem what tty we plan to run on;
     it will just record the information for later.  */

  new_tty_prefork (inferior_io_terminal);

  /* It is generally good practice to flush any possible pending stdio
     output prior to doing a fork, to avoid the possibility of both the
     parent and child flushing the same data after the fork. */

  gdb_flush (gdb_stdout);
  gdb_flush (gdb_stderr);

#if defined(USG) && !defined(HAVE_VFORK)
  pid = fork ();
#else
  if (debug_fork)
    pid = fork ();
  else
    pid = vfork ();
#endif

  if (pid < 0)
    perror_with_name ("vfork");

  if (pid == 0)
    {
      if (debug_fork) 
	sleep (debug_fork);

      /* Run inferior in a separate process group.  */
      debug_setpgrp = gdb_setpgid ();
      if (debug_setpgrp == -1)
	 perror("setpgrp failed in child");

      /* Ask the tty subsystem to switch to the one we specified earlier
	 (or to share the current terminal, if none was specified).  */

      new_tty ();

      /* Changing the signal handlers for the inferior after
	 a vfork can also change them for the superior, so we don't mess
	 with signals here.  See comments in
	 initialize_signals for how we get the right signal handlers
	 for the inferior.  */

      /* "Trace me, Dr. Memory!" */
      (*traceme_fun) ();

      /* There is no execlpe call, so we have to set the environment
	 for our child in the global variable.  If we've vforked, this
	 clobbers the parent, but environ is restored a few lines down
	 in the parent.  By the way, yes we do need to look down the
	 path to find $SHELL.  Rich Pixley says so, and I agree.  */
      environ = env;
      execlp (shell_file, shell_file, "-c", shell_command, (char *)0);

      fprintf_unfiltered (gdb_stderr, "Cannot exec %s: %s.\n", shell_file,
	       safe_strerror (errno));
      gdb_flush (gdb_stderr);
      _exit (0177);
    }

  /* Restore our environment in case a vforked child clob'd it.  */
  environ = save_our_env;

  init_thread_list();

  inferior_pid = pid;		/* Needed for wait_for_inferior stuff below */

  /* Now that we have a child process, make it our target, and
     initialize anything target-vector-specific that needs initializing.  */
  (*init_trace_fun)(pid);

  /* We are now in the child process of interest, having exec'd the
     correct program, and are poised at the first instruction of the
     new program.  */
#ifdef SOLIB_CREATE_INFERIOR_HOOK
  SOLIB_CREATE_INFERIOR_HOOK (pid);
#endif
}

/* Accept NTRAPS traps from the inferior.  */

void
startup_inferior (ntraps)
     int ntraps;
{
  int pending_execs = ntraps;
  int terminal_initted;

  /* The process was started by the fork that created it,
     but it will have stopped one instruction after execing the shell.
     Here we must get it up to actual execution of the real program.  */

  clear_proceed_status ();

  init_wait_for_inferior ();

  terminal_initted = 0;

#ifdef STARTUP_INFERIOR
  STARTUP_INFERIOR (pending_execs);
#else
  while (1)
    {
      stop_soon_quietly = 1;	/* Make wait_for_inferior be quiet */
      wait_for_inferior ();
      if (stop_signal != TARGET_SIGNAL_TRAP)
	{
	  /* Let shell child handle its own signals in its own way */
	  /* FIXME, what if child has exit()ed?  Must exit loop somehow */
	  resume (0, stop_signal);
	}
      else
	{
	  /* We handle SIGTRAP, however; it means child did an exec.  */
	  if (!terminal_initted)
	    {
	      /* Now that the child has exec'd we know it has already set its
		 process group.  On POSIX systems, tcsetpgrp will fail with
		 EPERM if we try it before the child's setpgid.  */

	      /* Set up the "saved terminal modes" of the inferior
		 based on what modes we are starting it with.  */
	      target_terminal_init ();

	      /* Install inferior's terminal modes.  */
	      target_terminal_inferior ();

	      terminal_initted = 1;
	    }
	  if (0 == --pending_execs)
	    break;
	  resume (0, TARGET_SIGNAL_0);		/* Just make it go on */
	}
    }
#endif /* STARTUP_INFERIOR */
  stop_soon_quietly = 0;
}
