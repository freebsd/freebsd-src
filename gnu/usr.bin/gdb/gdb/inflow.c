/* Low level interface to ptrace, for GDB when running under Unix.
   Copyright 1986, 1987, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "command.h"
#include "signals.h"
#include "serial.h"
#include "terminal.h"
#include "target.h"

#include <signal.h>
#include <fcntl.h>

#if !defined (HAVE_TERMIOS) && !defined (HAVE_TERMIO) && !defined (HAVE_SGTTY) && !defined (__GO32__)
#define HAVE_SGTTY
#endif

#if defined (HAVE_TERMIOS)
#include <termios.h>
#include <unistd.h>
#endif

#ifdef HAVE_TERMIOS
#define PROCESS_GROUP_TYPE pid_t
#endif

#ifdef HAVE_SGTTY
#ifdef SHORT_PGRP
/* This is only used for the ultra.  Does it have pid_t?  */
#define PROCESS_GROUP_TYPE short
#else
#define PROCESS_GROUP_TYPE int
#endif
#endif /* sgtty */

static void
kill_command PARAMS ((char *, int));

static void
terminal_ours_1 PARAMS ((int));

/* Nonzero if we are debugging an attached outside process
   rather than an inferior.  */

int attach_flag;


/* Record terminal status separately for debugger and inferior.  */

static serial_t stdin_serial;

/* TTY state for the inferior.  We save it whenever the inferior stops, and
   restore it when it resumes.  */
static serial_ttystate inferior_ttystate;

/* Our own tty state, which we restore every time we need to deal with the
   terminal.  We only set it once, when GDB first starts.  The settings of
   flags which readline saves and restores and unimportant.  */
static serial_ttystate our_ttystate;

/* fcntl flags for us and the inferior.  Saved and restored just like
   {our,inferior}_ttystate.  */
static int tflags_inferior;
static int tflags_ours;

#ifdef PROCESS_GROUP_TYPE
/* Process group for us and the inferior.  Saved and restored just like
   {our,inferior}_ttystate.  */
PROCESS_GROUP_TYPE our_process_group;
PROCESS_GROUP_TYPE inferior_process_group;
#endif

/* While the inferior is running, we want SIGINT and SIGQUIT to go to the
   inferior only.  If we have job control, that takes care of it.  If not,
   we save our handlers in these two variables and set SIGINT and SIGQUIT
   to SIG_IGN.  */
static void (*sigint_ours) ();
static void (*sigquit_ours) ();

/* The name of the tty (from the `tty' command) that we gave to the inferior
   when it was last started.  */

static char *inferior_thisrun_terminal;

/* Nonzero if our terminal settings are in effect.  Zero if the
   inferior's settings are in effect.  Ignored if !gdb_has_a_terminal
   ().  */

static int terminal_is_ours;

enum {yes, no, have_not_checked} gdb_has_a_terminal_flag = have_not_checked;

/* Does GDB have a terminal (on stdin)?  */
int
gdb_has_a_terminal ()
{
  switch (gdb_has_a_terminal_flag)
    {
    case yes:
      return 1;
    case no:
      return 0;
    case have_not_checked:
      /* Get all the current tty settings (including whether we have a tty at
	 all!).  Can't do this in _initialize_inflow because SERIAL_FDOPEN
	 won't work until the serial_ops_list is initialized.  */

#ifdef F_GETFL
      tflags_ours = fcntl (0, F_GETFL, 0);
#endif

      gdb_has_a_terminal_flag = no;
      stdin_serial = SERIAL_FDOPEN (0);
      if (stdin_serial != NULL)
	{
	  our_ttystate = SERIAL_GET_TTY_STATE (stdin_serial);

	  if (our_ttystate != NULL)
	    {
	      gdb_has_a_terminal_flag = yes;
#ifdef HAVE_TERMIOS
	      our_process_group = tcgetpgrp (0);
#endif
#ifdef HAVE_SGTTY
	      ioctl (0, TIOCGPGRP, &our_process_group);
#endif
	    }
	}

      return gdb_has_a_terminal_flag == yes;
    }
}

/* Macro for printing errors from ioctl operations */

#define	OOPSY(what)	\
  if (result == -1)	\
    fprintf(stderr, "[%s failed in terminal_inferior: %s]\n", \
	    what, strerror (errno))

static void terminal_ours_1 PARAMS ((int));

/* Initialize the terminal settings we record for the inferior,
   before we actually run the inferior.  */

void
terminal_init_inferior ()
{
  if (gdb_has_a_terminal ())
    {
      /* We could just as well copy our_ttystate (if we felt like adding
	 a new function SERIAL_COPY_TTY_STATE).  */
      if (inferior_ttystate)
	free (inferior_ttystate);
      inferior_ttystate = SERIAL_GET_TTY_STATE (stdin_serial);
#ifdef PROCESS_GROUP_TYPE
      inferior_process_group = inferior_pid;
#endif

      /* Make sure that next time we call terminal_inferior (which will be
	 before the program runs, as it needs to be), we install the new
	 process group.  */
      terminal_is_ours = 1;
    }
}

/* Put the inferior's terminal settings into effect.
   This is preparation for starting or resuming the inferior.  */

void
terminal_inferior ()
{
  if (gdb_has_a_terminal () && terminal_is_ours
      && inferior_thisrun_terminal == 0)
    {
      int result;

#ifdef F_GETFL
      /* Is there a reason this is being done twice?  It happens both
	 places we use F_SETFL, so I'm inclined to think perhaps there
	 is some reason, however perverse.  Perhaps not though...  */
      result = fcntl (0, F_SETFL, tflags_inferior);
      result = fcntl (0, F_SETFL, tflags_inferior);
      OOPSY ("fcntl F_SETFL");
#endif

      /* Because we were careful to not change in or out of raw mode in
	 terminal_ours, we will not change in our out of raw mode with
	 this call, so we don't flush any input.  */
      result = SERIAL_SET_TTY_STATE (stdin_serial, inferior_ttystate);
      OOPSY ("setting tty state");

      if (!job_control)
	{
	  sigint_ours = (void (*) ()) signal (SIGINT, SIG_IGN);
	  sigquit_ours = (void (*) ()) signal (SIGQUIT, SIG_IGN);
	}

      /* If attach_flag is set, we don't know whether we are sharing a
	 terminal with the inferior or not.  (attaching a process
	 without a terminal is one case where we do not; attaching a
	 process which we ran from the same shell as GDB via `&' is
	 one case where we do, I think (but perhaps this is not
	 `sharing' in the sense that we need to save and restore tty
	 state)).  I don't know if there is any way to tell whether we
	 are sharing a terminal.  So what we do is to go through all
	 the saving and restoring of the tty state, but ignore errors
	 setting the process group, which will happen if we are not
	 sharing a terminal).  */

      if (job_control)
	{
#ifdef HAVE_TERMIOS
	  result = tcsetpgrp (0, inferior_process_group);
	  if (!attach_flag)
	    OOPSY ("tcsetpgrp");
#endif

#ifdef HAVE_SGTTY
	  result = ioctl (0, TIOCSPGRP, &inferior_process_group);
	  if (!attach_flag)
	    OOPSY ("TIOCSPGRP");
#endif
	}

    }
  terminal_is_ours = 0;
}

/* Put some of our terminal settings into effect,
   enough to get proper results from our output,
   but do not change into or out of RAW mode
   so that no input is discarded.

   After doing this, either terminal_ours or terminal_inferior
   should be called to get back to a normal state of affairs.  */

void
terminal_ours_for_output ()
{
  terminal_ours_1 (1);
}

/* Put our terminal settings into effect.
   First record the inferior's terminal settings
   so they can be restored properly later.  */

void
terminal_ours ()
{
  terminal_ours_1 (0);
}

/* output_only is not used, and should not be used unless we introduce
   separate terminal_is_ours and terminal_is_ours_for_output
   flags.  */

static void
terminal_ours_1 (output_only)
     int output_only;
{
  /* Checking inferior_thisrun_terminal is necessary so that
     if GDB is running in the background, it won't block trying
     to do the ioctl()'s below.  Checking gdb_has_a_terminal
     avoids attempting all the ioctl's when running in batch.  */
  if (inferior_thisrun_terminal != 0 || gdb_has_a_terminal () == 0)
    return;

  if (!terminal_is_ours)
    {
      /* Ignore this signal since it will happen when we try to set the
	 pgrp.  */
      void (*osigttou) ();
      int result;

      terminal_is_ours = 1;

#ifdef SIGTTOU
      if (job_control)
	osigttou = (void (*) ()) signal (SIGTTOU, SIG_IGN);
#endif

      if (inferior_ttystate)
	free (inferior_ttystate);
      inferior_ttystate = SERIAL_GET_TTY_STATE (stdin_serial);
#ifdef HAVE_TERMIOS
      inferior_process_group = tcgetpgrp (0);
#endif
#ifdef HAVE_SGTTY
      ioctl (0, TIOCGPGRP, &inferior_process_group);
#endif

      /* Here we used to set ICANON in our ttystate, but I believe this
	 was an artifact from before when we used readline.  Readline sets
	 the tty state when it needs to.  */

      /* Set tty state to our_ttystate.  We don't change in our out of raw
	 mode, to avoid flushing input.  We need to do the same thing
	 regardless of output_only, because we don't have separate
	 terminal_is_ours and terminal_is_ours_for_output flags.  It's OK,
	 though, since readline will deal with raw mode when/if it needs to.
	 */
      SERIAL_NOFLUSH_SET_TTY_STATE (stdin_serial, our_ttystate,
				    inferior_ttystate);

      if (job_control)
	{
#ifdef HAVE_TERMIOS
	  result = tcsetpgrp (0, our_process_group);
#if 0
	  /* This fails on Ultrix with EINVAL if you run the testsuite
	     in the background with nohup, and then log out.  GDB never
	     used to check for an error here, so perhaps there are other
	     such situations as well.  */
	  if (result == -1)
	    fprintf (stderr, "[tcsetpgrp failed in terminal_ours: %s]\n",
		     strerror (errno));
#endif
#endif /* termios */

#ifdef HAVE_SGTTY
	  result = ioctl (0, TIOCSPGRP, &our_process_group);
#endif
	}

#ifdef SIGTTOU
      if (job_control)
	signal (SIGTTOU, osigttou);
#endif

      if (!job_control)
	{
	  signal (SIGINT, sigint_ours);
	  signal (SIGQUIT, sigquit_ours);
	}

#ifdef F_GETFL
      tflags_inferior = fcntl (0, F_GETFL, 0);

      /* Is there a reason this is being done twice?  It happens both
	 places we use F_SETFL, so I'm inclined to think perhaps there
	 is some reason, however perverse.  Perhaps not though...  */
      result = fcntl (0, F_SETFL, tflags_ours);
      result = fcntl (0, F_SETFL, tflags_ours);
#endif

      result = result;	/* lint */
    }
}

/* ARGSUSED */
void
term_info (arg, from_tty)
     char *arg;
     int from_tty;
{
  target_terminal_info (arg, from_tty);
}

/* ARGSUSED */
void
child_terminal_info (args, from_tty)
     char *args;
     int from_tty;
{
  if (!gdb_has_a_terminal ())
    {
      printf_filtered ("This GDB does not control a terminal.\n");
      return;
    }

  printf_filtered ("Inferior's terminal status (currently saved by GDB):\n");

  /* First the fcntl flags.  */
  {
    int flags;
    
    flags = tflags_inferior;

    printf_filtered ("File descriptor flags = ");

#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif
    /* (O_ACCMODE) parens are to avoid Ultrix header file bug */
    switch (flags & (O_ACCMODE))
      {
      case O_RDONLY: printf_filtered ("O_RDONLY"); break;
      case O_WRONLY: printf_filtered ("O_WRONLY"); break;
      case O_RDWR: printf_filtered ("O_RDWR"); break;
      }
    flags &= ~(O_ACCMODE);

#ifdef O_NONBLOCK
    if (flags & O_NONBLOCK) 
      printf_filtered (" | O_NONBLOCK");
    flags &= ~O_NONBLOCK;
#endif
    
#if defined (O_NDELAY)
    /* If O_NDELAY and O_NONBLOCK are defined to the same thing, we will
       print it as O_NONBLOCK, which is good cause that is what POSIX
       has, and the flag will already be cleared by the time we get here.  */
    if (flags & O_NDELAY)
      printf_filtered (" | O_NDELAY");
    flags &= ~O_NDELAY;
#endif

    if (flags & O_APPEND)
      printf_filtered (" | O_APPEND");
    flags &= ~O_APPEND;

#if defined (O_BINARY)
    if (flags & O_BINARY)
      printf_filtered (" | O_BINARY");
    flags &= ~O_BINARY;
#endif

    if (flags)
      printf_filtered (" | 0x%x", flags);
    printf_filtered ("\n");
  }

#ifdef PROCESS_GROUP_TYPE
  printf_filtered ("Process group = %d\n", inferior_process_group);
#endif

  SERIAL_PRINT_TTY_STATE (stdin_serial, inferior_ttystate);
}

/* NEW_TTY_PREFORK is called before forking a new child process,
   so we can record the state of ttys in the child to be formed.
   TTYNAME is null if we are to share the terminal with gdb;
   or points to a string containing the name of the desired tty.

   NEW_TTY is called in new child processes under Unix, which will
   become debugger target processes.  This actually switches to
   the terminal specified in the NEW_TTY_PREFORK call.  */

void
new_tty_prefork (ttyname)
     char *ttyname;
{
  /* Save the name for later, for determining whether we and the child
     are sharing a tty.  */
  inferior_thisrun_terminal = ttyname;
}

void
new_tty ()
{
  register int tty;

  if (inferior_thisrun_terminal == 0)
    return;
#if !defined(__GO32__)
#ifdef TIOCNOTTY
  /* Disconnect the child process from our controlling terminal.  On some
     systems (SVR4 for example), this may cause a SIGTTOU, so temporarily
     ignore SIGTTOU. */
  tty = open("/dev/tty", O_RDWR);
  if (tty > 0)
    {
      void (*osigttou) ();

      osigttou = (void (*)()) signal(SIGTTOU, SIG_IGN);
      ioctl(tty, TIOCNOTTY, 0);
      close(tty);
      signal(SIGTTOU, osigttou);
    }
#endif

  /* Now open the specified new terminal.  */

#ifdef USE_O_NOCTTY
  tty = open(inferior_thisrun_terminal, O_RDWR | O_NOCTTY);
#else
  tty = open(inferior_thisrun_terminal, O_RDWR);
#endif
  if (tty == -1)
    {
      print_sys_errmsg (inferior_thisrun_terminal, errno);
      _exit(1);
    }

  /* Avoid use of dup2; doesn't exist on all systems.  */
  if (tty != 0)
    { close (0); dup (tty); }
  if (tty != 1)
    { close (1); dup (tty); }
  if (tty != 2)
    { close (2); dup (tty); }
  if (tty > 2)
    close(tty);
#endif /* !go32 */
}

/* Kill the inferior process.  Make us have no inferior.  */

/* ARGSUSED */
static void
kill_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  /* Shouldn't this be target_has_execution?  FIXME.  */
  if (inferior_pid == 0)
    error ("The program is not being run.");
  if (!query ("Kill the program being debugged? "))
    error ("Not confirmed.");
  target_kill ();

  init_thread_list();		/* Destroy thread info */

  /* Killing off the inferior can leave us with a core file.  If so,
     print the state we are left in.  */
  if (target_has_stack) {
    printf_filtered ("In %s,\n", current_target->to_longname);
    if (selected_frame == NULL)
      fputs_filtered ("No selected stack frame.\n", stdout);
    else
      print_stack_frame (selected_frame, selected_frame_level, 1);
  }
}

/* The inferior process has died.  Long live the inferior!  */

void
generic_mourn_inferior ()
{
  inferior_pid = 0;
  attach_flag = 0;
  breakpoint_init_inferior ();
  registers_changed ();

#ifdef CLEAR_DEFERRED_STORES
  /* Delete any pending stores to the inferior... */
  CLEAR_DEFERRED_STORES;
#endif

  reopen_exec_file ();
  reinit_frame_cache ();

  /* It is confusing to the user for ignore counts to stick around
     from previous runs of the inferior.  So clear them.  */
  breakpoint_clear_ignore_counts ();
}

/* Call set_sigint_trap when you need to pass a signal on to an attached
   process when handling SIGINT */

/* ARGSUSED */
static void
pass_signal (signo)
    int signo;
{
  kill (inferior_pid, SIGINT);
}

static void (*osig)();

void
set_sigint_trap()
{
  osig = (void (*) ()) signal (SIGINT, pass_signal);
}

void
clear_sigint_trap()
{
  signal (SIGINT, osig);
}


int job_control;

/* This is here because this is where we figure out whether we (probably)
   have job control.  Just using job_control only does part of it because
   setpgid or setpgrp might not exist on a system without job control.
   It might be considered misplaced (on the other hand, process groups and
   job control are closely related to ttys).

   For a more clean implementation, in libiberty, put a setpgid which merely
   calls setpgrp and a setpgrp which does nothing (any system with job control
   will have one or the other).  */
int
gdb_setpgid ()
{
  int retval = 0;
  if (job_control)
    {
#if defined (NEED_POSIX_SETPGID) || defined (HAVE_TERMIOS)
      /* Do all systems with termios have setpgid?  I hope so.  */
      /* setpgid (0, 0) is supposed to work and mean the same thing as
	 this, but on Ultrix 4.2A it fails with EPERM (and
	 setpgid (getpid (), getpid ()) succeeds).  */
      retval = setpgid (getpid (), getpid ());
#else
#if defined (TIOCGPGRP)
#if defined(USG) && !defined(SETPGRP_ARGS)
      retval = setpgrp ();
#else
      retval = setpgrp (getpid (), getpid ());
#endif /* USG */
#endif /* TIOCGPGRP.  */
#endif /* NEED_POSIX_SETPGID */
    }
  return retval;
}

void
_initialize_inflow ()
{
  add_info ("terminal", term_info,
	   "Print inferior's saved terminal status.");

  add_com ("kill", class_run, kill_command,
	   "Kill execution of program being debugged.");

  inferior_pid = 0;

  terminal_is_ours = 1;

  /* OK, figure out whether we have job control.  If neither termios nor
     sgtty (i.e. termio or go32), leave job_control 0.  */

#if defined (HAVE_TERMIOS)
  /* Do all systems with termios have the POSIX way of identifying job
     control?  I hope so.  */
#ifdef _POSIX_JOB_CONTROL
  job_control = 1;
#else
  job_control = sysconf (_SC_JOB_CONTROL);
#endif
#endif /* termios */

#ifdef HAVE_SGTTY
#ifdef TIOCGPGRP
  job_control = 1;
#else
  job_control = 0;
#endif /* TIOCGPGRP */
#endif /* sgtty */
}
