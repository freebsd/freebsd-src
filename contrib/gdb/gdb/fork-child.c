/* Fork a Unix child process, and set up to debug it, for GDB.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999, 2000,
   2001 Free Software Foundation, Inc.
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdb_string.h"
#include "frame.h"		/* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "gdb_wait.h"
#include "gdb_vfork.h"
#include "gdbcore.h"
#include "terminal.h"
#include "gdbthread.h"
#include "command.h" /* for dont_repeat () */

#include <signal.h>

/* This just gets used as a default if we can't find SHELL */
#ifndef SHELL_FILE
#define SHELL_FILE "/bin/sh"
#endif

extern char **environ;

/* This function breaks up an argument string into an argument
 * vector suitable for passing to execvp().
 * E.g., on "run a b c d" this routine would get as input
 * the string "a b c d", and as output it would fill in argv with
 * the four arguments "a", "b", "c", "d".
 */
static void
breakup_args (char *scratch, char **argv)
{
  char *cp = scratch;

  for (;;)
    {

      /* Scan past leading separators */
      while (*cp == ' ' || *cp == '\t' || *cp == '\n')
	{
	  cp++;
	}

      /* Break if at end of string */
      if (*cp == '\0')
	break;

      /* Take an arg */
      *argv++ = cp;

      /* Scan for next arg separator */
      cp = strchr (cp, ' ');
      if (cp == NULL)
	cp = strchr (cp, '\t');
      if (cp == NULL)
	cp = strchr (cp, '\n');

      /* No separators => end of string => break */
      if (cp == NULL)
	break;

      /* Replace the separator with a terminator */
      *cp++ = '\0';
    }

  /* execv requires a null-terminated arg vector */
  *argv = NULL;

}


/* Start an inferior Unix child process and sets inferior_ptid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  SHELL_FILE is the shell file,
   or NULL if we should pick one.  Errors reported with error().  */

/* This function is NOT-REENTRANT.  Some of the variables have been
   made static to ensure that they survive the vfork() call.  */

void
fork_inferior (char *exec_file_arg, char *allargs, char **env,
	       void (*traceme_fun) (void), void (*init_trace_fun) (int),
	       void (*pre_trace_fun) (void), char *shell_file_arg)
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
  static char *shell_file;
  static char *exec_file;
  char **save_our_env;
  int shell = 0;
  static char **argv;

  /* If no exec file handed to us, get it from the exec-file command -- with
     a good, common error message if none is specified.  */
  exec_file = exec_file_arg;
  if (exec_file == 0)
    exec_file = get_exec_file (1);

  /* STARTUP_WITH_SHELL is defined in inferior.h.
   * If 0, we'll just do a fork/exec, no shell, so don't
   * bother figuring out what shell.
   */
  shell_file = shell_file_arg;
  if (STARTUP_WITH_SHELL)
    {
      /* Figure out what shell to start up the user program under. */
      if (shell_file == NULL)
	shell_file = getenv ("SHELL");
      if (shell_file == NULL)
	shell_file = default_shell_file;
      shell = 1;
    }

  /* Multiplying the length of exec_file by 4 is to account for the fact
     that it may expand when quoted; it is a worst-case number based on
     every character being '.  */
  len = 5 + 4 * strlen (exec_file) + 1 + strlen (allargs) + 1 + /*slop */ 12;
  /* If desired, concat something onto the front of ALLARGS.
     SHELL_COMMAND is the result.  */
#ifdef SHELL_COMMAND_CONCAT
  shell_command = (char *) alloca (strlen (SHELL_COMMAND_CONCAT) + len);
  strcpy (shell_command, SHELL_COMMAND_CONCAT);
#else
  shell_command = (char *) alloca (len);
  shell_command[0] = '\0';
#endif

  if (!shell)
    {
      /* We're going to call execvp. Create argv */
      /* Largest case: every other character is a separate arg */
      argv = (char **) xmalloc (((strlen (allargs) + 1) / (unsigned) 2 + 2) * sizeof (*argv));
      argv[0] = exec_file;
      breakup_args (allargs, &argv[1]);

    }
  else
    {

      /* We're going to call a shell */

      /* Now add exec_file, quoting as necessary.  */

      char *p;
      int need_to_quote;

      strcat (shell_command, "exec ");

      /* Quoting in this style is said to work with all shells.  But csh
         on IRIX 4.0.1 can't deal with it.  So we only quote it if we need
         to.  */
      p = exec_file;
      while (1)
	{
	  switch (*p)
	    {
	    case '\'':
	    case '!':
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
	      else if (*p == '!')
		strcat (shell_command, "\\!");
	      else
		strncat (shell_command, p, 1);
	    }
	  strcat (shell_command, "'");
	}
      else
	strcat (shell_command, exec_file);

      strcat (shell_command, " ");
      strcat (shell_command, allargs);

    }

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

  /* If there's any initialization of the target layers that must happen
     to prepare to handle the child we're about fork, do it now...
   */
  if (pre_trace_fun != NULL)
    (*pre_trace_fun) ();

  /* Create the child process.  Note that the apparent call to vfork()
     below *might* actually be a call to fork() due to the fact that
     autoconf will ``#define vfork fork'' on certain platforms.  */
  if (debug_fork)
    pid = fork ();
  else
    pid = vfork ();

  if (pid < 0)
    perror_with_name ("vfork");

  if (pid == 0)
    {
      if (debug_fork)
	sleep (debug_fork);

      /* Run inferior in a separate process group.  */
      debug_setpgrp = gdb_setpgid ();
      if (debug_setpgrp == -1)
	perror ("setpgrp failed in child");

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
      /* The call above set this process (the "child") as debuggable
       * by the original gdb process (the "parent").  Since processes
       * (unlike people) can have only one parent, if you are
       * debugging gdb itself (and your debugger is thus _already_ the
       * controller/parent for this child),  code from here on out
       * is undebuggable.  Indeed, you probably got an error message
       * saying "not parent".  Sorry--you'll have to use print statements!
       */

      /* There is no execlpe call, so we have to set the environment
         for our child in the global variable.  If we've vforked, this
         clobbers the parent, but environ is restored a few lines down
         in the parent.  By the way, yes we do need to look down the
         path to find $SHELL.  Rich Pixley says so, and I agree.  */
      environ = env;

      /* If we decided above to start up with a shell,
       * we exec the shell,
       * "-c" says to interpret the next arg as a shell command
       * to execute, and this command is "exec <target-program> <args>".
       * "-f" means "fast startup" to the c-shell, which means
       * don't do .cshrc file. Doing .cshrc may cause fork/exec
       * events which will confuse debugger start-up code.
       */
      if (shell)
	{
	  execlp (shell_file, shell_file, "-c", shell_command, (char *) 0);

	  /* If we get here, it's an error */
	  fprintf_unfiltered (gdb_stderr, "Cannot exec %s: %s.\n", shell_file,
			      safe_strerror (errno));
	  gdb_flush (gdb_stderr);
	  _exit (0177);
	}
      else
	{
	  /* Otherwise, we directly exec the target program with execvp. */
	  int i;
	  char *errstring;

	  execvp (exec_file, argv);

	  /* If we get here, it's an error */
	  errstring = safe_strerror (errno);
	  fprintf_unfiltered (gdb_stderr, "Cannot exec %s ", exec_file);

	  i = 1;
	  while (argv[i] != NULL)
	    {
	      if (i != 1)
		fprintf_unfiltered (gdb_stderr, " ");
	      fprintf_unfiltered (gdb_stderr, "%s", argv[i]);
	      i++;
	    }
	  fprintf_unfiltered (gdb_stderr, ".\n");
	  /* This extra info seems to be useless
	     fprintf_unfiltered (gdb_stderr, "Got error %s.\n", errstring);
	   */
	  gdb_flush (gdb_stderr);
	  _exit (0177);
	}
    }

  /* Restore our environment in case a vforked child clob'd it.  */
  environ = save_our_env;

  init_thread_list ();

  inferior_ptid = pid_to_ptid (pid);	/* Needed for wait_for_inferior stuff below */

  /* Now that we have a child process, make it our target, and
     initialize anything target-vector-specific that needs initializing.  */

  (*init_trace_fun) (pid);

  /* We are now in the child process of interest, having exec'd the
     correct program, and are poised at the first instruction of the
     new program.  */

  /* Allow target dependent code to play with the new process.  This might be
     used to have target-specific code initialize a variable in the new process
     prior to executing the first instruction.  */
  TARGET_CREATE_INFERIOR_HOOK (pid);

#ifdef SOLIB_CREATE_INFERIOR_HOOK
  SOLIB_CREATE_INFERIOR_HOOK (pid);
#endif
}

/* An inferior Unix process CHILD_PID has been created by a call to
   fork() (or variants like vfork).  It is presently stopped, and waiting
   to be resumed.  clone_and_follow_inferior will fork the debugger,
   and that clone will "follow" (attach to) CHILD_PID.  The original copy
   of the debugger will not touch CHILD_PID again.

   Also, the original debugger will set FOLLOWED_CHILD FALSE, while the
   clone will set it TRUE.
 */
void
clone_and_follow_inferior (int child_pid, int *followed_child)
{
  int debugger_pid;
  int status;
  char pid_spelling[100];	/* Arbitrary but sufficient length. */

  /* This semaphore is used to coordinate the two debuggers' handoff
     of CHILD_PID.  The original debugger will detach from CHILD_PID,
     and then the clone debugger will attach to it.  (It must be done
     this way because on some targets, only one process at a time can
     trace another.  Thus, the original debugger must relinquish its
     tracing rights before the clone can pick them up.)
   */
#define SEM_TALK (1)
#define SEM_LISTEN (0)
  int handoff_semaphore[2];	/* Original "talks" to [1], clone "listens" to [0] */
  int talk_value = 99;
  int listen_value;

  /* Set debug_fork then attach to the child while it sleeps, to debug. */
  static int debug_fork = 0;

  /* It is generally good practice to flush any possible pending stdio
     output prior to doing a fork, to avoid the possibility of both the
     parent and child flushing the same data after the fork. */

  gdb_flush (gdb_stdout);
  gdb_flush (gdb_stderr);

  /* Open the semaphore pipes.
   */
  status = pipe (handoff_semaphore);
  if (status < 0)
    error ("error getting pipe for handoff semaphore");

  /* Clone the debugger.  Note that the apparent call to vfork()
     below *might* actually be a call to fork() due to the fact that
     autoconf will ``#define vfork fork'' on certain platforms.  */
  if (debug_fork)
    debugger_pid = fork ();
  else
    debugger_pid = vfork ();

  if (debugger_pid < 0)
    perror_with_name ("fork");

  /* Are we the original debugger?  If so, we must relinquish all claims
     to CHILD_PID. */
  if (debugger_pid != 0)
    {
      char signal_spelling[100];	/* Arbitrary but sufficient length */

      /* Detach from CHILD_PID.  Deliver a "stop" signal when we do, though,
         so that it remains stopped until the clone debugger can attach
         to it.
       */
      detach_breakpoints (child_pid);

      sprintf (signal_spelling, "%d", target_signal_to_host (TARGET_SIGNAL_STOP));
      target_require_detach (child_pid, signal_spelling, 1);

      /* Notify the clone debugger that it should attach to CHILD_PID. */
      write (handoff_semaphore[SEM_TALK], &talk_value, sizeof (talk_value));

      *followed_child = 0;
    }

  /* We're the child. */
  else
    {
      if (debug_fork)
	sleep (debug_fork);

      /* The child (i.e., the cloned debugger) must now attach to
         CHILD_PID.  inferior_ptid is presently set to the parent process
         of the fork, while CHILD_PID should be the child process of the
         fork.

         Wait until the original debugger relinquishes control of CHILD_PID,
         though.
       */
      read (handoff_semaphore[SEM_LISTEN], &listen_value, sizeof (listen_value));

      /* Note that we DON'T want to actually detach from inferior_ptid,
         because that would allow it to run free.  The original
         debugger wants to retain control of the process.  So, we
         just reset inferior_ptid to CHILD_PID, and then ensure that all
         breakpoints are really set in CHILD_PID.
       */
      target_mourn_inferior ();

      /* Ask the tty subsystem to switch to the one we specified earlier
         (or to share the current terminal, if none was specified).  */

      new_tty ();

      dont_repeat ();
      sprintf (pid_spelling, "%d", child_pid);
      target_require_attach (pid_spelling, 1);

      /* Perform any necessary cleanup, after attachment.  (This form
         of attaching can behave differently on some targets than the
         standard method, where a process formerly not under debugger
         control was suddenly attached to..)
       */
      target_post_follow_inferior_by_clone ();

      *followed_child = 1;
    }

  /* Discard the handoff sempahore. */
  (void) close (handoff_semaphore[SEM_LISTEN]);
  (void) close (handoff_semaphore[SEM_TALK]);
}

/* Accept NTRAPS traps from the inferior.  */

void
startup_inferior (int ntraps)
{
  int pending_execs = ntraps;
  int terminal_initted;

  /* The process was started by the fork that created it,
     but it will have stopped one instruction after execing the shell.
     Here we must get it up to actual execution of the real program.  */

  clear_proceed_status ();

  init_wait_for_inferior ();

  terminal_initted = 0;

  if (STARTUP_WITH_SHELL)
    inferior_ignoring_startup_exec_events = ntraps;
  else
    inferior_ignoring_startup_exec_events = 0;
  inferior_ignoring_leading_exec_events =
    target_reported_exec_events_per_exec_call () - 1;

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

	  pending_execs = pending_execs - 1;
	  if (0 == pending_execs)
	    break;

	  resume (0, TARGET_SIGNAL_0);	/* Just make it go on */
	}
    }
#endif /* STARTUP_INFERIOR */
  stop_soon_quietly = 0;
}
