/* Target-vector operations for controlling Unix child processes, for GDB.
   Copyright 1990-1996, 1998, 1999 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

## Contains temporary hacks..

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
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"
#include "command.h"
#include "gdb_stat.h"
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef HAVE_WAIT_H
# include <wait.h>
#else
# ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif
#endif

/* "wait.h" fills in the gaps left by <wait.h> */
#include "wait.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

extern struct symtab_and_line *
child_enable_exception_callback PARAMS ((enum exception_event_kind, int));

extern struct exception_event_record *
child_get_current_exception_event PARAMS ((void));

extern void _initialize_inftarg PARAMS ((void));

static void
child_prepare_to_store PARAMS ((void));

#ifndef CHILD_WAIT
static int child_wait PARAMS ((int, struct target_waitstatus *));
#endif /* CHILD_WAIT */

#if !defined(CHILD_POST_WAIT)
void
child_post_wait PARAMS ((int, int));
#endif

static void child_open PARAMS ((char *, int));

static void
child_files_info PARAMS ((struct target_ops *));

static void
child_detach PARAMS ((char *, int));

static void
child_detach_from_process PARAMS ((int, char *, int, int));

static void
child_attach PARAMS ((char *, int));

static void
child_attach_to_process PARAMS ((char *, int, int));

#if !defined(CHILD_POST_ATTACH)
extern void child_post_attach PARAMS ((int));
#endif

static void
child_require_attach PARAMS ((char *, int));

static void
child_require_detach PARAMS ((int, char *, int));

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

static void
child_stop PARAMS ((void));

#ifndef CHILD_THREAD_ALIVE
int child_thread_alive PARAMS ((int));
#endif

static void init_child_ops PARAMS ((void));

extern char **environ;

struct target_ops child_ops;

int child_suppress_run = 0;	/* Non-zero if inftarg should pretend not to
				   be a runnable target.  Used by targets
				   that can sit atop inftarg, such as HPUX
				   thread support.  */

#ifndef CHILD_WAIT

/*##*/
/* Enable HACK for ttrace work.  In
 * infttrace.c/require_notification_of_events,
 * this is set to 0 so that the loop in child_wait
 * won't loop.
 */
int not_same_real_pid = 1;
/*##*/


/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer OURSTATUS.  */

static int
child_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  int save_errno;
  int status;
  char *  execd_pathname;
  int  exit_status;
  int  related_pid;
  int  syscall_id;
  enum target_waitkind  kind;

  do {
    set_sigint_trap();	/* Causes SIGINT to be passed on to the
			   attached process. */
    set_sigio_trap ();

    pid = ptrace_wait (inferior_pid, &status);

    save_errno = errno;

    clear_sigio_trap ();

    clear_sigint_trap();

    if (pid == -1)
      {
	if (save_errno == EINTR)
	  continue;

	fprintf_unfiltered (gdb_stderr, "Child process unexpectedly missing: %s.\n",
		 safe_strerror (save_errno));

	/* Claim it exited with unknown signal.  */
	ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
	ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
        return -1;
      }

    /* Did it exit?
     */
    if (target_has_exited (pid, status, &exit_status))
      {
        /* ??rehrauer: For now, ignore this. */
        continue;
      }

    if (!target_thread_alive (pid))
      {
        ourstatus->kind = TARGET_WAITKIND_SPURIOUS;
        return pid;
      }
      
    if (target_has_forked (pid, &related_pid)
	 && ((pid == inferior_pid) || (related_pid == inferior_pid)))
      {
        ourstatus->kind = TARGET_WAITKIND_FORKED;
        ourstatus->value.related_pid = related_pid;
        return pid;
      }

    if (target_has_vforked (pid, &related_pid)
	 && ((pid == inferior_pid) || (related_pid == inferior_pid)))
      {
        ourstatus->kind = TARGET_WAITKIND_VFORKED;
        ourstatus->value.related_pid = related_pid;
        return pid;
      }

    if (target_has_execd (pid, &execd_pathname))
      {
        /* Are we ignoring initial exec events?  (This is likely because
           we're in the process of starting up the inferior, and another
           (older) mechanism handles those.)  If so, we'll report this
           as a regular stop, not an exec.
           */
        if (inferior_ignoring_startup_exec_events)
          {
            inferior_ignoring_startup_exec_events--;
          }
        else
          {
            ourstatus->kind = TARGET_WAITKIND_EXECD;
            ourstatus->value.execd_pathname = execd_pathname;
            return pid;
          }
      }

    /* All we must do with these is communicate their occurrence
       to wait_for_inferior...
       */
    if (target_has_syscall_event (pid, &kind, &syscall_id))
      {
        ourstatus->kind = kind;
        ourstatus->value.syscall_id = syscall_id;
        return pid;
      }

/*##  } while (pid != inferior_pid); ##*/ /* Some other child died or stopped */
/* hack for thread testing */
      } while( (pid != inferior_pid) && not_same_real_pid );
/*##*/

  store_waitstatus (ourstatus, status);
  return pid;
}
#endif /* CHILD_WAIT */

#if !defined(CHILD_POST_WAIT)
void
child_post_wait (pid, wait_status)
  int  pid;
  int  wait_status;
{
  /* This version of Unix doesn't require a meaningful "post wait"
     operation.
     */
}
#endif
 

#ifndef CHILD_THREAD_ALIVE

/* Check to see if the given thread is alive.

   FIXME: Is kill() ever the right way to do this?  I doubt it, but
   for now we're going to try and be compatable with the old thread
   code.  */
int
child_thread_alive (pid)
     int pid;
{
  return (kill (pid, 0) != -1);
}

#endif

static void
child_attach_to_process (args, from_tty, after_fork)
  char *  args;
  int  from_tty;
  int  after_fork;
{
  if (!args)
    error_no_arg ("process-id to attach");

#ifndef ATTACH_DETACH
  error ("Can't attach to a process on this machine.");
#else
  {
    char *exec_file;
    int pid;
    char *dummy;

    dummy = args;
    pid = strtol (args, &dummy, 0);
    /* Some targets don't set errno on errors, grrr! */
    if ((pid == 0) && (args == dummy))
      error ("Illegal process-id: %s\n", args);

    if (pid == getpid())		/* Trying to masturbate? */
      error ("I refuse to debug myself!");

    if (from_tty)
      {
	exec_file = (char *) get_exec_file (0);

	if (after_fork)
	  printf_unfiltered ("Attaching after fork to %s\n", 
		  target_pid_to_str (pid));
	else if (exec_file)
	  printf_unfiltered ("Attaching to program: %s, %s\n", exec_file,
                  target_pid_to_str (pid));
        else
          printf_unfiltered ("Attaching to %s\n", target_pid_to_str (pid));

	gdb_flush (gdb_stdout);
      }

    if (!after_fork)
      attach (pid);
    else
      REQUIRE_ATTACH (pid);

    inferior_pid = pid;
    push_target (&child_ops);
  }
#endif  /* ATTACH_DETACH */
}


/* Attach to process PID, then initialize for debugging it.  */

static void
child_attach (args, from_tty)
     char *args;
     int from_tty;
{
  child_attach_to_process (args, from_tty, 0);
}

#if !defined(CHILD_POST_ATTACH)
void
child_post_attach (pid)
  int  pid;
{
  /* This version of Unix doesn't require a meaningful "post attach"
     operation by a debugger.  */
}
#endif

static void
child_require_attach (args, from_tty)
     char *args;
     int from_tty;
{
  child_attach_to_process (args, from_tty, 1);
} 

static void
child_detach_from_process (pid, args, from_tty, after_fork)
  int  pid;
  char *  args;
  int  from_tty;
  int  after_fork;
{
#ifdef ATTACH_DETACH
  {
    int siggnal = 0;

    if (from_tty)
      {
	char *exec_file = get_exec_file (0);
	if (exec_file == 0)
	  exec_file = "";
        if (after_fork)
          printf_unfiltered ("Detaching after fork from %s\n",
                             target_pid_to_str (pid));
        else
	  printf_unfiltered ("Detaching from program: %s, %s\n", exec_file,
		             target_pid_to_str (pid));
	gdb_flush (gdb_stdout);
      }
    if (args)
      siggnal = atoi (args);

    if (!after_fork)
      detach (siggnal);
    else
      REQUIRE_DETACH (pid, siggnal);
  }
#else
  error ("This version of Unix does not support detaching a process.");
#endif
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
  child_detach_from_process (inferior_pid, args, from_tty, 0);
  inferior_pid = 0;
  unpush_target (&child_ops);
}

static void
child_require_detach (pid, args, from_tty)
  int  pid;
  char *  args;
  int  from_tty;
{
  child_detach_from_process (pid, args, from_tty, 1);
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
  printf_unfiltered ("\tUsing the running image of %s %s.\n",
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

  /* On some targets, there must be some explicit synchronization
     between the parent and child processes after the debugger
     forks, and before the child execs the debuggee program.  This
     call basically gives permission for the child to exec.
     */

  target_acknowledge_created_inferior (pid);

  /* START_INFERIOR_TRAPS_EXPECTED is defined in inferior.h,
   * and will be 1 or 2 depending on whether we're starting
   * without or with a shell.
   */
  startup_inferior (START_INFERIOR_TRAPS_EXPECTED);

  /* On some targets, there must be some explicit actions taken after
     the inferior has been started up.
     */
  target_post_startup_inferior (pid);
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

#ifdef HPUXHPPA
  char *tryname;
  char *shell_file;
  char *p;
  char *p1;
  char *path = getenv ("PATH");
  int len;
  struct stat statbuf;

  /* On HP-UX, we have a possible bad interaction between
   * the start-up-with-shell code and our catch-fork/catch-exec
   * logic. To avoid the bad interaction, we start up with the
   * C shell ("csh") and pass it the "-f" flag (fast start-up,
   * don't run .cshrc code).
   * See further comments in inferior.h toward the bottom
   * (STARTUP_WITH_SHELL flag) and in fork-child.c
   */

  /* Rather than passing in a hard-wired path like "/bin/csh",
   * we look down the PATH to find csh. I took this code from
   * procfs.c, which is the file in the Sun-specific part of GDB
   * analogous to inftarg.c. See procfs.c for more detailed 
   * comments. - RT
   */
  shell_file = "csh";
  if (path == NULL)
    path = "/bin:/usr/bin";
  tryname = alloca (strlen (path) + strlen (shell_file) + 2);
  for (p = path; p != NULL; p = p1 ? p1 + 1: NULL)
    {
    p1 = strchr (p, ':');
    if (p1 != NULL)
      len = p1 - p;
    else
      len = strlen (p);
    strncpy (tryname, p, len);
    tryname[len] = '\0';
    strcat (tryname, "/");
    strcat (tryname, shell_file);
    if (access (tryname, X_OK) < 0)
      continue;
    if (stat (tryname, &statbuf) < 0)
      continue;
    if (!S_ISREG (statbuf.st_mode))
      /* We certainly need to reject directories.  I'm not quite
         as sure about FIFOs, sockets, etc., but I kind of doubt
         that people want to exec() these things.  */
      continue;
      break;
    }
  if (p == NULL)
    /* Not found. I replaced the error() which existed in procfs.c
     * with simply passing in NULL and hoping fork_inferior() 
     * can deal with it. - RT
     */ 
    /* error ("Can't find shell %s in PATH", shell_file); */
    shell_file = NULL;
  else
    shell_file = tryname;

  fork_inferior (exec_file, allargs, env, ptrace_me, ptrace_him, pre_fork_inferior, NULL);
#else
 fork_inferior (exec_file, allargs, env, ptrace_me, ptrace_him, NULL, NULL);
#endif
  /* We are at the first instruction we care about.  */
  /* Pedal to the metal... */
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_0, 0);
}

#if !defined(CHILD_POST_STARTUP_INFERIOR)
void
child_post_startup_inferior (pid)
  int  pid;
{
  /* This version of Unix doesn't require a meaningful "post startup inferior"
     operation by a debugger.
     */
}
#endif

#if !defined(CHILD_ACKNOWLEDGE_CREATED_INFERIOR)
void
child_acknowledge_created_inferior (pid)
  int  pid;
{
  /* This version of Unix doesn't require a meaningful "acknowledge created inferior"
     operation by a debugger.
     */
}
#endif


void
child_clone_and_follow_inferior (child_pid, followed_child)
  int  child_pid;
  int  *followed_child;
{
  clone_and_follow_inferior (child_pid, followed_child);

  /* Don't resume CHILD_PID; it's stopped where it ought to be, until
     the decision gets made elsewhere how to continue it.
     */
}


#if !defined(CHILD_POST_FOLLOW_INFERIOR_BY_CLONE)
void
child_post_follow_inferior_by_clone ()
{
  /* This version of Unix doesn't require a meaningful "post follow inferior"
     operation by a clone debugger.
     */
}
#endif

#if !defined(CHILD_INSERT_FORK_CATCHPOINT)
int
child_insert_fork_catchpoint (pid)
     int pid;
{
  /* This version of Unix doesn't support notification of fork events.  */
  return 0;
}
#endif

#if !defined(CHILD_REMOVE_FORK_CATCHPOINT)
int
child_remove_fork_catchpoint (pid)
     int pid;
{
  /* This version of Unix doesn't support notification of fork events.  */
  return 0;
}
#endif

#if !defined(CHILD_INSERT_VFORK_CATCHPOINT)
int
child_insert_vfork_catchpoint (pid)
     int pid;
{
  /* This version of Unix doesn't support notification of vfork events.  */
  return 0;
}
#endif

#if !defined(CHILD_REMOVE_VFORK_CATCHPOINT)
int
child_remove_vfork_catchpoint (pid)
     int pid;
{
  /* This version of Unix doesn't support notification of vfork events.  */
  return 0;
}
#endif

#if !defined(CHILD_HAS_FORKED)
int
child_has_forked (pid, child_pid)
     int pid;
     int *child_pid;
{
  /* This version of Unix doesn't support notification of fork events.  */
  return 0;
}
#endif


#if !defined(CHILD_HAS_VFORKED)
int
child_has_vforked (pid, child_pid)
  int  pid;
  int *  child_pid;
{
  /* This version of Unix doesn't support notification of vfork events.
     */
  return 0;
}
#endif


#if !defined(CHILD_CAN_FOLLOW_VFORK_PRIOR_TO_EXEC)
int
child_can_follow_vfork_prior_to_exec ()
{
  /* This version of Unix doesn't support notification of vfork events.
     However, if it did, it probably wouldn't allow vforks to be followed
     before the following exec.
     */
  return 0;
}
#endif


#if !defined(CHILD_POST_FOLLOW_VFORK)
void
child_post_follow_vfork (parent_pid, followed_parent, child_pid, followed_child)
  int  parent_pid;
  int  followed_parent;
  int  child_pid;
  int  followed_child;
{
  /* This version of Unix doesn't require a meaningful "post follow vfork"
     operation by a clone debugger.
     */
}
#endif

#if !defined(CHILD_INSERT_EXEC_CATCHPOINT)
int
child_insert_exec_catchpoint (pid)
     int pid;
{
  /* This version of Unix doesn't support notification of exec events.  */
  return 0;
}
#endif

#if !defined(CHILD_REMOVE_EXEC_CATCHPOINT)
int
child_remove_exec_catchpoint (pid)
     int pid;
{
  /* This version of Unix doesn't support notification of exec events.  */
  return 0;
}
#endif

#if !defined(CHILD_HAS_EXECD)
int
child_has_execd (pid, execd_pathname)
  int  pid;
  char **  execd_pathname;
{
  /* This version of Unix doesn't support notification of exec events.
     */
  return 0;
}
#endif


#if !defined(CHILD_REPORTED_EXEC_EVENTS_PER_EXEC_CALL)
int
child_reported_exec_events_per_exec_call ()
{
  /* This version of Unix doesn't support notification of exec events.
     */
  return 1;
}
#endif


#if !defined(CHILD_HAS_SYSCALL_EVENT)
int
child_has_syscall_event (pid, kind, syscall_id)
  int  pid;
  enum target_waitkind *  kind;
  int *  syscall_id;
{
  /* This version of Unix doesn't support notification of syscall events.
     */
  return 0;
}
#endif


#if !defined(CHILD_HAS_EXITED)
int
child_has_exited (pid, wait_status, exit_status)
  int  pid;
  int  wait_status;
  int *  exit_status;
{
  if (WIFEXITED (wait_status))
    {
      *exit_status = WEXITSTATUS (wait_status);
      return 1;
    }

  if (WIFSIGNALED (wait_status))
    {
      *exit_status = 0;  /* ?? Don't know what else to say here. */
      return 1;
    }

  /* ?? Do we really need to consult the event state, too?  Assume the
   wait_state alone suffices.
   */
  return 0;
}
#endif


static void
child_mourn_inferior ()
{
  /* FIXME: Should be in a header file */
  extern void proc_remove_foreign PARAMS ((int));

  unpush_target (&child_ops);
  proc_remove_foreign (inferior_pid);
  generic_mourn_inferior ();
}

static int
child_can_run ()
{
  /* This variable is controlled by modules that sit atop inftarg that may layer
     their own process structure atop that provided here.  hpux-thread.c does
     this because of the Hpux user-mode level thread model.  */

  return !child_suppress_run;
}

/* Send a SIGINT to the process group.  This acts just like the user typed a
   ^C on the controlling terminal.

   XXX - This may not be correct for all systems.  Some may want to use
   killpg() instead of kill (-pgrp). */

static void
child_stop ()
{
  extern pid_t inferior_process_group;

  kill (-inferior_process_group, SIGINT);
}

#if !defined(CHILD_ENABLE_EXCEPTION_CALLBACK)
struct symtab_and_line *
child_enable_exception_callback (kind, enable)
  enum exception_event_kind kind;
  int enable;
{
  return (struct symtab_and_line *) NULL;
}
#endif

#if !defined(CHILD_GET_CURRENT_EXCEPTION_EVENT)
struct exception_event_record *
child_get_current_exception_event ()
{
  return (struct exception_event_record *) NULL;
}
#endif


#if !defined(CHILD_PID_TO_EXEC_FILE)
char *
child_pid_to_exec_file (pid)
  int  pid;
{
  /* This version of Unix doesn't support translation of a process ID
     to the filename of the executable file.
     */
  return NULL;
}
#endif

char *
child_core_file_to_sym_file (core)
  char *  core;
{
  /* The target stratum for a running executable need not support
     this operation.
     */
  return NULL;
}



static void
init_child_ops ()
{
  child_ops.to_shortname = "child";
  child_ops.to_longname = "Unix child process";
  child_ops.to_doc = "Unix child process (started by the \"run\" command).";
  child_ops.to_open = child_open;
  child_ops.to_attach = child_attach;
  child_ops.to_post_attach = child_post_attach;
  child_ops.to_require_attach = child_require_attach;
  child_ops.to_detach = child_detach;
  child_ops.to_require_detach = child_require_detach;
  child_ops.to_resume = child_resume;
  child_ops.to_wait = child_wait;
  child_ops.to_post_wait = child_post_wait;
  child_ops.to_fetch_registers = fetch_inferior_registers;
  child_ops.to_store_registers = store_inferior_registers;
  child_ops.to_prepare_to_store = child_prepare_to_store;
  child_ops.to_xfer_memory = child_xfer_memory;
  child_ops.to_files_info = child_files_info;
  child_ops.to_insert_breakpoint = memory_insert_breakpoint;
  child_ops.to_remove_breakpoint = memory_remove_breakpoint;
  child_ops.to_terminal_init = terminal_init_inferior;
  child_ops.to_terminal_inferior = terminal_inferior;
  child_ops.to_terminal_ours_for_output = terminal_ours_for_output;
  child_ops.to_terminal_ours = terminal_ours;
  child_ops.to_terminal_info = child_terminal_info;
  child_ops.to_kill = kill_inferior;
  child_ops.to_create_inferior = child_create_inferior;
  child_ops.to_post_startup_inferior = child_post_startup_inferior;
  child_ops.to_acknowledge_created_inferior = child_acknowledge_created_inferior;
  child_ops.to_clone_and_follow_inferior = child_clone_and_follow_inferior;
  child_ops.to_post_follow_inferior_by_clone = child_post_follow_inferior_by_clone;
  child_ops.to_insert_fork_catchpoint = child_insert_fork_catchpoint;
  child_ops.to_remove_fork_catchpoint = child_remove_fork_catchpoint;
  child_ops.to_insert_vfork_catchpoint = child_insert_vfork_catchpoint;
  child_ops.to_remove_vfork_catchpoint = child_remove_vfork_catchpoint;
  child_ops.to_has_forked = child_has_forked;
  child_ops.to_has_vforked = child_has_vforked;
  child_ops.to_can_follow_vfork_prior_to_exec = child_can_follow_vfork_prior_to_exec;
  child_ops.to_post_follow_vfork = child_post_follow_vfork;
  child_ops.to_insert_exec_catchpoint = child_insert_exec_catchpoint;
  child_ops.to_remove_exec_catchpoint = child_remove_exec_catchpoint;
  child_ops.to_has_execd = child_has_execd;
  child_ops.to_reported_exec_events_per_exec_call = child_reported_exec_events_per_exec_call;
  child_ops.to_has_syscall_event = child_has_syscall_event;
  child_ops.to_has_exited = child_has_exited;
  child_ops.to_mourn_inferior = child_mourn_inferior;
  child_ops.to_can_run = child_can_run;
  child_ops.to_thread_alive = child_thread_alive;
  child_ops.to_stop = child_stop;
  child_ops.to_enable_exception_callback = child_enable_exception_callback;
  child_ops.to_get_current_exception_event = child_get_current_exception_event;
  child_ops.to_pid_to_exec_file = child_pid_to_exec_file;
  child_ops.to_core_file_to_sym_file = child_core_file_to_sym_file;
  child_ops.to_stratum = process_stratum;
  child_ops.to_has_all_memory = 1;
  child_ops.to_has_memory = 1;
  child_ops.to_has_stack = 1;
  child_ops.to_has_registers = 1;
  child_ops.to_has_execution = 1;
  child_ops.to_magic = OPS_MAGIC;
}

void
_initialize_inftarg ()
{
#ifdef HAVE_OPTIONAL_PROC_FS
  char procname[32];
  int fd;

  /* If we have an optional /proc filesystem (e.g. under OSF/1),
     don't add ptrace support if we can access the running GDB via /proc.  */
#ifndef PROC_NAME_FMT
#define PROC_NAME_FMT "/proc/%05d"
#endif
  sprintf (procname, PROC_NAME_FMT, getpid ());
  if ((fd = open (procname, O_RDONLY)) >= 0)
    {
      close (fd);
      return;
    }
#endif

  init_child_ops ();
  add_target (&child_ops);
}
