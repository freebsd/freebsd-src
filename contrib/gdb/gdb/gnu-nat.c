/* Interface GDB to the GNU Hurd
   Copyright (C) 1992, 1995, 1996 Free Software Foundation, Inc.

   This file is part of GDB.

   Written by Miles Bader <miles@gnu.ai.mit.edu>

   Some code and ideas from m3-nat.c by Jukka Virtanen <jtv@hut.fi>

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <setjmp.h>
#include <limits.h>
#include <sys/ptrace.h>

/* We include this because we don't need the access macros and they conflict
   with gdb's definitions (ick).  This is very non standard!  */
#include <waitflags.h>

#include <mach.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach_error.h>
#include <mach/exception.h>
#include <mach/vm_attributes.h>

#include <hurd/process.h>
#include <hurd/process_request.h>
#include <hurd/msg.h>
#include <hurd/msg_request.h>
#include <hurd/signal.h>
#include <hurd/interrupt.h>
#include <hurd/sigpreempt.h>

#include "defs.h"
#include "inferior.h"
#include "symtab.h"
#include "value.h"
#include "language.h"
#include "target.h"
#include "wait.h"
#include "gdbcmd.h"
#include "gdbcore.h"

#include "gnu-nat.h"

#include "exc_request_S.h"
#include "notify_S.h"
#include "process_reply_S.h"
#include "msg_reply_S.h"

#include "exc_request_U.h"
#include "msg_U.h"

static process_t proc_server = MACH_PORT_NULL;

/* If we've sent a proc_wait_request to the proc server, the pid of the
   process we asked about.  We can only ever have one outstanding.  */
int proc_wait_pid = 0;

/* The number of wait requests we've sent, and expect replies from.  */
int proc_waits_pending = 0;

int gnu_debug_flag = 0;

/* Forward decls */

extern struct target_ops gnu_ops;

struct inf *make_inf ();
void inf_clear_wait (struct inf *inf);
void inf_cleanup (struct inf *inf);
void inf_startup (struct inf *inf, int pid, task_t task);
int inf_update_suspends (struct inf *inf);
void inf_set_task (struct inf *inf, task_t port);
void inf_validate_procs (struct inf *inf);
void inf_steal_exc_ports (struct inf *inf);
void inf_restore_exc_ports (struct inf *inf);
int inf_update_procs (struct inf *inf);
struct proc *inf_tid_to_proc (struct inf *inf, int tid);
inline void inf_set_threads_resume_sc (struct inf *inf, struct proc
				       *run_thread, int run_others);
inline int inf_set_threads_resume_sc_for_signal_thread (struct inf *inf);
inline void inf_suspend (struct inf *inf);
inline void inf_resume (struct inf *inf);
void inf_set_step_thread (struct inf *inf, struct proc *proc);
void inf_detach (struct inf *inf);
void inf_attach (struct inf *inf, int pid);
void inf_signal (struct inf *inf, enum target_signal sig);

#define inf_debug(_inf, msg, args...) \
  do { struct inf *__inf = (_inf); \
       debug ("{inf %d %p}: " msg, __inf->pid, __inf , ##args); } while (0)

struct proc *make_proc (struct inf *inf, mach_port_t port, int tid);
struct proc *_proc_free (struct proc *proc);
int proc_update_sc (struct proc *proc);
void proc_abort (struct proc *proc, int force);
thread_state_t proc_get_state (struct proc *proc, int force);
error_t proc_get_exception_port (struct proc *proc, mach_port_t *port);
error_t proc_set_exception_port (struct proc *proc, mach_port_t port);
static mach_port_t _proc_get_exc_port (struct proc *proc);
void proc_steal_exc_port (struct proc *proc, mach_port_t exc_port);
void proc_restore_exc_port (struct proc *proc);
int proc_trace (struct proc *proc, int set);
char *proc_string (struct proc *proc);

/* Evaluate RPC_EXPR in a scope with the variables MSGPORT and REFPORT bound
   to INF's msg port and task port respectively.  If it has no msg port,
   EIEIO is returned.  INF must refer to a running process!  */
#define INF_MSGPORT_RPC(inf, rpc_expr) \
  HURD_MSGPORT_RPC (proc_getmsgport (proc_server, inf->pid, &msgport), \
		    (refport = inf->task->port, 0), 0, \
		    msgport ? (rpc_expr) : EIEIO)

/* Like INF_MSGPORT_RPC, but will also resume the signal thread to ensure
   there's someone around to deal with the RPC (and resuspend things
   afterwards).  This effects INF's threads' resume_sc count.  */
#define INF_RESUME_MSGPORT_RPC(inf, rpc_expr) \
  (inf_set_threads_resume_sc_for_signal_thread (inf) \
   ? ({ error_t __e; \
	inf_resume (inf); \
	__e = INF_MSGPORT_RPC (inf, rpc_expr); \
	inf_suspend (inf); \
	__e; }) \
   : EIEIO)

#define MIG_SERVER_DIED EMIG_SERVER_DIED /* XXX */

/* The state passed by an exception message.  */
struct exc_state
{
  int exception;		/* The exception code */
  int code, subcode;
  mach_port_t handler;		/* The real exception port to handle this. */
  mach_port_t reply;		/* The reply port from the exception call. */
};

/* The results of the last wait an inf did. */
struct inf_wait
{
  struct target_waitstatus status; /* The status returned to gdb.  */
  struct exc_state exc;		/* The exception that caused us to return. */
  struct proc *thread;		/* The thread in question.  */
  int suppress;			/* Something trivial happened.  */
};

/* The state of an inferior.  */
struct inf
{
  /* Fields describing the current inferior.  */

  struct proc *task;		/* The mach task.   */
  struct proc *threads;		/* A linked list of all threads in TASK.  */

  /* True if THREADS needn't be validated by querying the task.  We assume that
     we and the task in question are the only ones frobbing the thread list,
     so as long as we don't let any code run, we don't have to worry about
     THREADS changing.  */
  int threads_up_to_date;

  pid_t pid;			/* The real system PID. */

  struct inf_wait wait;		/* What to return from target_wait.  */

  /* One thread proc in INF may be in `single-stepping mode'.  This is it.  */
  struct proc *step_thread;

  /* The thread we think is the signal thread.  */
  struct proc *signal_thread;

  mach_port_t event_port;	/* Where we receive various msgs.  */

  /* True if we think at least one thread in the inferior could currently be
     running.  */
  int running : 1;

  /* True if the process has stopped (in the proc server sense).  Note that
     since a proc server `stop' leaves the signal thread running, the inf can
     be RUNNING && STOPPED...  */
  int stopped : 1;

  /* True if the inferior is traced.  */
  int traced : 1;

  /* True if we shouldn't try waiting for the inferior, usually because we
     can't for some reason.  */
  int no_wait : 1;

  /* When starting a new inferior, we don't try to validate threads until all
     the proper execs have been done.  This is a count of how many execs we
     expect to happen.  */
  unsigned pending_execs;

  /* Fields describing global state */

  /* The task suspend count used when gdb has control.  This is normally 1 to
     make things easier for us, but sometimes (like when attaching to vital
     system servers) it may be desirable to let the task continue to run
     (pausing individual threads as necessary).  */
  int pause_sc;

  /* The initial values used for the run_sc and pause_sc of newly discovered
     threads -- see the definition of those fields in struct proc.  */
  int default_thread_run_sc;
  int default_thread_pause_sc;

  /* True if the process should be traced when started/attached.  Newly
     started processes *must* be traced at first to exec them properly, but
     if this is false, tracing is turned off as soon it has done so.  */
  int want_signals;

  /* True if exceptions from the inferior process should be trapped.  This
     must be on to use breakpoints.  */
  int want_exceptions;
};

int __proc_pid (struct proc *proc)
{
  return proc->inf->pid;
}

/* Update PROC's real suspend count to match it's desired one.  Returns true
   if we think PROC is now in a runnable state.  */
int
proc_update_sc (struct proc *proc)
{
  int running;
  int err =  0;
  int delta = proc->sc - proc->cur_sc;

  if (delta)
    proc_debug (proc, "sc: %d --> %d", proc->cur_sc, proc->sc);

  if (proc->sc == 0 && proc->state_changed)
    /* Since PROC may start running, we must write back any state changes. */
    {
      assert (proc_is_thread (proc));
      proc_debug (proc, "storing back changed thread state");
      err = thread_set_state (proc->port, THREAD_STATE_FLAVOR,
			      &proc->state, THREAD_STATE_SIZE);
      if (! err)
	proc->state_changed = 0;
    }

  if (delta > 0)
    while (delta-- > 0 && !err)
      if (proc_is_task (proc))
	err = task_suspend (proc->port);
      else
	err = thread_suspend (proc->port);
  else
    while (delta++ < 0 && !err)
      if (proc_is_task (proc))
	err = task_resume (proc->port);
      else
	err = thread_resume (proc->port);

  if (! err)
    proc->cur_sc = proc->sc;

  /* If we got an error, then the task/thread has disappeared.  */
  running = !err && proc->sc == 0;

  proc_debug (proc, "is %s", err ? "dead" : running ? "running" : "suspended");
  if (err)
    proc_debug (proc, "err = %s", strerror (err));

  if (running)
    {
      proc->aborted = 0;
      proc->state_valid = proc->state_changed = 0;
      proc->fetched_regs = 0;
    }

  return running;
}

/* Thread_abort is called on PROC if needed.  PROC must be a thread proc.
   If PROC is deemed `precious', then nothing is done unless FORCE is true.
   In particular, a thread is precious if it's running (in which case forcing
   it includes suspending it first), or if it has an exception pending.  */
void
proc_abort (struct proc *proc, int force)
{
  assert (proc_is_thread (proc));

  if (! proc->aborted)
    {
      struct inf *inf = proc->inf;
      int running = (proc->cur_sc == 0 && inf->task->cur_sc == 0);

      if (running && force)
	{
	  proc->sc = 1;
	  inf_update_suspends (proc->inf);
	  running = 0;
	  warning ("Stopped %s.", proc_string (proc));
	}
      else if (proc == inf->wait.thread && inf->wait.exc.reply && !force)
	/* An exception is pending on PROC, which don't mess with.  */
	running = 1;

      if (! running)
	/* We only abort the thread if it's not actually running.  */
	{
	  thread_abort (proc->port);
	  proc_debug (proc, "aborted");
	  proc->aborted = 1;
	}
      else
	proc_debug (proc, "not aborting");
    }
}

/* Make sure that the state field in PROC is up to date, and return a pointer
   to it, or 0 if something is wrong.  If WILL_MODIFY is true, makes sure
   that the thread is stopped and aborted first, and sets the state_changed
   field in PROC to true.  */
thread_state_t
proc_get_state (struct proc *proc, int will_modify)
{
  int was_aborted = proc->aborted;

  proc_debug (proc, "updating state info%s",
	      will_modify ? " (with intention to modify)" : "");

  proc_abort (proc, will_modify);

  if (! was_aborted && proc->aborted)
    /* PROC's state may have changed since we last fetched it.  */
    proc->state_valid = 0;

  if (! proc->state_valid)
    {
      mach_msg_type_number_t state_size = THREAD_STATE_SIZE;
      error_t err =
	thread_get_state (proc->port, THREAD_STATE_FLAVOR,
			  &proc->state, &state_size);
      proc_debug (proc, "getting thread state");
      proc->state_valid = !err;
    }

  if (proc->state_valid)
    {
      if (will_modify)
	proc->state_changed = 1;
      return &proc->state;
    }
  else
    return 0;
}

error_t
proc_get_exception_port (struct proc *proc, mach_port_t *port)
{
  if (proc_is_task (proc))
    return task_get_exception_port (proc->port, port);
  else
    return thread_get_exception_port (proc->port, port);
}

error_t
proc_set_exception_port (struct proc *proc, mach_port_t port)
{
  proc_debug (proc, "setting exception port: %d", port);
  if (proc_is_task (proc))
    return task_set_exception_port (proc->port, port);
  else
    return thread_set_exception_port (proc->port, port);
}

/* Get PROC's exception port, cleaning up a bit if proc has died.  */
static mach_port_t
_proc_get_exc_port (struct proc *proc)
{
  mach_port_t exc_port;
  error_t err = proc_get_exception_port (proc, &exc_port);

  if (err)
    /* PROC must be dead.  */
    {
      if (proc->exc_port)
	mach_port_deallocate (mach_task_self (), proc->exc_port);
      proc->exc_port = MACH_PORT_NULL;
      if (proc->saved_exc_port)
	mach_port_deallocate (mach_task_self (), proc->saved_exc_port);
      proc->saved_exc_port = MACH_PORT_NULL;
    }

  return exc_port;
}

/* Replace PROC's exception port with EXC_PORT, unless it's already been
   done.  Stash away any existing exception port so we can restore it later. */
void
proc_steal_exc_port (struct proc *proc, mach_port_t exc_port)
{
  mach_port_t cur_exc_port = _proc_get_exc_port (proc);

  if (cur_exc_port)
    {
      error_t err;

      proc_debug (proc, "inserting exception port: %d", exc_port);

      if (cur_exc_port != exc_port)
	/* Put in our exception port.  */
	err = proc_set_exception_port (proc, exc_port);

      if (err || cur_exc_port == proc->exc_port)
	/* We previously set the exception port, and it's still set.  So we
	   just keep the old saved port which is what the proc set.  */
	{
	  if (cur_exc_port)
	    mach_port_deallocate (mach_task_self (), cur_exc_port);
	}
      else
	/* Keep a copy of PROC's old exception port so it can be restored. */
	{
	  if (proc->saved_exc_port)
	    mach_port_deallocate (mach_task_self (), proc->saved_exc_port);
	  proc->saved_exc_port = cur_exc_port;
	}

      proc_debug (proc, "saved exception port: %d", proc->saved_exc_port);

      if (!err)
	proc->exc_port = exc_port;
      else
	warning ("Error setting exception port for %s: %s",
		 proc_string (proc), strerror (err));
    }
}

/* If we previously replaced PROC's exception port, put back what we found
   there at the time, unless *our* exception port has since be overwritten,
   in which case who knows what's going on.  */
void
proc_restore_exc_port (struct proc *proc)
{
  mach_port_t cur_exc_port = _proc_get_exc_port (proc);

  if (cur_exc_port)
    {
      error_t err = 0;

      proc_debug (proc, "restoring real exception port");

      if (proc->exc_port == cur_exc_port)
	/* Our's is still there.  */
	err = proc_set_exception_port (proc, proc->saved_exc_port);

      if (proc->saved_exc_port)
	mach_port_deallocate (mach_task_self (), proc->saved_exc_port);
      proc->saved_exc_port = MACH_PORT_NULL;

      if (!err)
	proc->exc_port = MACH_PORT_NULL;
      else
	warning ("Error setting exception port for %s: %s",
		 proc_string (proc), strerror (err));
    }
}

/* Turns hardware tracing in PROC on or off when SET is true or fals,
   respectively.  Returns true on success.  */
int
proc_trace (struct proc *proc, int set)
{
  thread_state_t state = proc_get_state (proc, 1);

  if (! state)
    return 0;			/* the thread must be dead.  */

  proc_debug (proc, "tracing %s", set ? "on" : "off");
  
  if (set)
    {
      /* XXX We don't get the exception unless the thread has its own
	 exception port???? */
      if (proc->exc_port == MACH_PORT_NULL)
	proc_steal_exc_port (proc, proc->inf->event_port);
      THREAD_STATE_SET_TRACED (state);
    }
  else
    THREAD_STATE_CLEAR_TRACED (state);

  return 1;
}

/* A variable from which to assign new TIDs.  */
static int next_thread_id = 1;

/* Returns a new proc structure with the given fields.  Also adds a
   notification for PORT becoming dead to be sent to INF's notify port.  */
struct proc *
make_proc (struct inf *inf, mach_port_t port, int tid)
{
  error_t err;
  mach_port_t prev_port = MACH_PORT_NULL;
  struct proc *proc = malloc (sizeof (struct proc));

  proc->port = port;
  proc->tid = tid;
  proc->inf = inf;
  proc->next = 0;
  proc->saved_exc_port = MACH_PORT_NULL;
  proc->exc_port = MACH_PORT_NULL;
  proc->sc = 0;
  proc->cur_sc = 0;
  proc->run_sc = inf->default_thread_run_sc;
  proc->pause_sc = inf->default_thread_pause_sc;
  proc->resume_sc = proc->run_sc;
  proc->aborted = 0;
  proc->state_valid = 0;
  proc->state_changed = 0;

  proc_debug (proc, "is new");

  /* Get notified when things die.  */
  err =
    mach_port_request_notification (mach_task_self(), port,
				    MACH_NOTIFY_DEAD_NAME, 1,
				    inf->event_port,
				    MACH_MSG_TYPE_MAKE_SEND_ONCE,
				    &prev_port);
  if (err)
    warning ("Couldn't request notification for port %d: %s",
	     port, strerror (err));
  else
    {
      proc_debug (proc, "notifications to: %d", inf->event_port);
      if (prev_port != MACH_PORT_NULL)
	mach_port_deallocate (mach_task_self (), prev_port);
    }

  if (inf->want_exceptions)
    if (proc_is_task (proc))
      /* Make the task exception port point to us.  */
      proc_steal_exc_port (proc, inf->event_port);
    else
      /* Just clear thread exception ports -- they default to the task one.  */
      proc_steal_exc_port (proc, MACH_PORT_NULL);

  return proc;
}

/* Frees PROC and any resources it uses, and returns the value of PROC's next
   field.  */
struct proc *
_proc_free (struct proc *proc)
{
  struct inf *inf = proc->inf;
  struct proc *next = proc->next;

  proc_debug (proc, "freeing...");

  if (proc == inf->step_thread)
    /* Turn off single stepping.  */
    inf_set_step_thread (inf, 0);
  if (proc == inf->wait.thread)
    inf_clear_wait (inf);
  if (proc == inf->signal_thread)
    inf->signal_thread = 0;

  if (proc->port != MACH_PORT_NULL)
    {
      if (proc->exc_port != MACH_PORT_NULL)
	/* Restore the original exception port.  */
	proc_restore_exc_port (proc);
      if (proc->cur_sc != 0)
	/* Resume the thread/task.  */
	{
	  proc->sc = 0;
	  proc_update_sc (proc);
	}
      mach_port_deallocate (mach_task_self (), proc->port);
    }

  free (proc);
  return next;
}

struct inf *make_inf ()
{
  struct inf *inf = malloc (sizeof (struct inf));

  if (!inf)
    return 0;

  inf->task = 0;
  inf->threads = 0;
  inf->threads_up_to_date = 0;
  inf->pid = 0;
  inf->wait.status.kind = TARGET_WAITKIND_SPURIOUS;
  inf->wait.thread = 0;
  inf->wait.exc.handler = MACH_PORT_NULL;
  inf->wait.exc.reply = MACH_PORT_NULL;
  inf->step_thread = 0;
  inf->signal_thread = 0;
  inf->event_port = MACH_PORT_NULL;
  inf->stopped = 0;
  inf->running = 0;
  inf->traced = 0;
  inf->no_wait = 0;
  inf->pending_execs = 0;
  inf->pause_sc = 1;
  inf->default_thread_run_sc = 0;
  inf->default_thread_pause_sc = 0;
  inf->want_signals = 1;	/* By default */
  inf->want_exceptions = 1;	/* By default */

  return inf;
}

void
inf_clear_wait (struct inf *inf)
{
  inf_debug (inf, "clearing wait");
  inf->wait.status.kind = TARGET_WAITKIND_SPURIOUS;
  inf->wait.thread = 0;
  inf->wait.suppress = 0;
  if (inf->wait.exc.handler != MACH_PORT_NULL)
    {
      mach_port_deallocate (mach_task_self (), inf->wait.exc.handler);
      inf->wait.exc.handler = MACH_PORT_NULL;
    }
  if (inf->wait.exc.reply != MACH_PORT_NULL)
    {
      mach_port_deallocate (mach_task_self (), inf->wait.exc.reply);
      inf->wait.exc.reply = MACH_PORT_NULL;
    }
}

void
inf_cleanup (struct inf *inf)
{
  inf_debug (inf, "cleanup");

  inf_clear_wait (inf);

  inf_set_task (inf, MACH_PORT_NULL);
  inf->pid = 0;
  inf->traced = 0;
  inf->no_wait = 0;
  inf->stopped = 0;
  inf->running = 0;
  inf->pending_execs = 0;

  if (inf->event_port)
    {
      mach_port_destroy (mach_task_self (), inf->event_port);
      inf->event_port = MACH_PORT_NULL;
    }
}

void
inf_startup (struct inf *inf, int pid, task_t task)
{
  error_t err;

  inf_debug (inf, "startup: pid = %d, task = %d", pid, task);

  inf_cleanup (inf);

  /* Make the port on which we receive all events.  */
  err = mach_port_allocate (mach_task_self (),
			    MACH_PORT_RIGHT_RECEIVE, &inf->event_port);
  if (err)
    error ("Error allocating event port: %s", strerror (err));

  /* Make a send right for it, so we can easily copy it for other people.  */
  mach_port_insert_right (mach_task_self (), inf->event_port,
			  inf->event_port, MACH_MSG_TYPE_MAKE_SEND);

  if (inf->pause_sc)
    task_suspend (task);

  inf_set_task (inf, task); 

  if (inf->task)
    {
      inf->pid = pid;
      if (inf->pause_sc)
	inf->task->sc = inf->task->cur_sc = 1; /* Reflect task_suspend above */
    }
}

void 
inf_set_task (struct inf *inf, mach_port_t port)
{
  struct proc *task = inf->task;

  inf_debug (inf, "setting task: %d", port);

  if (task && task->port != port)
    {
      inf->task = 0;
      inf_validate_procs (inf);	/* Trash all the threads. */
      _proc_free (task);	/* And the task. */
    }

  if (port != MACH_PORT_NULL)
    {
      inf->task = make_proc (inf, port, PROC_TID_TASK);
      inf->threads_up_to_date = 0;
    }
}

/* Validates INF's stopped field from the actual proc server state.  */
static void
inf_validate_stopped (struct inf *inf)
{
  char *noise;
  mach_msg_type_number_t noise_len = 0;
  struct procinfo *pi;
  mach_msg_type_number_t pi_len = 0;
  error_t err =
    proc_getprocinfo (proc_server, inf->pid, 0,
		      (procinfo_t *)&pi, &pi_len, &noise, &noise_len);

  if (! err)
    {
      inf->stopped = !!(pi->state & PI_STOPPED);
      vm_deallocate (mach_task_self (), (vm_address_t)pi, pi_len);
      if (noise_len > 0)
	vm_deallocate (mach_task_self (), (vm_address_t)noise, noise_len);
    }
}

/* Validates INF's task suspend count.  */
static void
inf_validate_task_sc (struct inf *inf)
{
  struct task_basic_info info;
  mach_msg_type_number_t info_len = TASK_BASIC_INFO_COUNT;
  error_t err = task_info (inf->task->port, TASK_BASIC_INFO, &info, &info_len);
  if (! err)
    {
      if (inf->task->cur_sc < info.suspend_count)
	warning ("Pid %d is suspended; continuing will clear existing suspend count.", inf->pid);
      inf->task->cur_sc = info.suspend_count;
    }
}

/* Turns tracing for INF on or off, depending on ON, unless it already is.
   If INF is running, the resume_sc count of INF's threads will be modified,
   and the signal thread will briefly be run to change the trace state.  */
void
inf_set_traced (struct inf *inf, int on)
{
  if (on != inf->traced)
    if (inf->task)
      /* Make it take effect immediately.  */
      {
	error_t (*f)(mach_port_t, mach_port_t, int) =
	  on ? msg_set_some_exec_flags : msg_clear_some_exec_flags;
	error_t err =
	  INF_RESUME_MSGPORT_RPC (inf, (*f)(msgport, refport, EXEC_TRACED));
	if (err == EIEIO)
	  warning ("Can't modify tracing state for pid %d: No signal thread",
		   inf->pid);
	else if (err)
	  warning ("Can't modify tracing state for pid %d: %s",
		   inf->pid, strerror (err));
	else
	  inf->traced = on;
      }
    else
      inf->traced = on;
}

/* Makes all the real suspend count deltas of all the procs in INF match the
   desired values.  Careful to always do thread/task suspend counts in the
   safe order.  Returns true if at least one thread is thought to be running.*/
int
inf_update_suspends (struct inf *inf)
{
  struct proc *task = inf->task;
  /* We don't have to update INF->threads even though we're iterating over it
     because we'll change a thread only if it already has an existing proc
     entry.  */

  inf_debug (inf, "updating suspend counts");

  if (task)
    {
      struct proc *thread;
      int task_running = (task->sc == 0), thread_running = 0;

      if (task->sc > task->cur_sc)
	/* The task is becoming _more_ suspended; do before any threads.  */
	task_running = proc_update_sc (task);

      if (inf->pending_execs)
	/* When we're waiting for an exec, things may be happening behind our
	   back, so be conservative.  */
	thread_running = 1;

      /* Do all the thread suspend counts.  */
      for (thread = inf->threads; thread; thread = thread->next)
	thread_running |= proc_update_sc (thread);

      if (task->sc != task->cur_sc)
	/* We didn't do the task first, because we wanted to wait for the
	   threads; do it now.  */
	task_running = proc_update_sc (task);

      inf_debug (inf, "%srunning...",
		 (thread_running && task_running) ? "" : "not ");

      inf->running = thread_running && task_running;

      /* Once any thread has executed some code, we can't depend on the
	 threads list any more.  */
      if (inf->running)
	inf->threads_up_to_date = 0;

      return inf->running;
    }

  return 0;
}

/* Converts a GDB pid to a struct proc.  */
struct proc *
inf_tid_to_thread (struct inf *inf, int tid)
{
  struct proc *thread = inf->threads;
  while (thread)
    if (thread->tid == tid)
      return thread;
    else
      thread = thread->next;
  return 0;
}

/* Converts a thread port to a struct proc.  */
struct proc *
inf_port_to_thread (struct inf *inf, mach_port_t port)
{
  struct proc *thread = inf->threads;
  while (thread)
    if (thread->port == port)
      return thread;
    else
      thread = thread->next;
  return 0;
}

/* Make INF's list of threads be consistent with reality of TASK.  */
void
inf_validate_procs (struct inf *inf)
{
  int i;
  thread_array_t threads;
  unsigned num_threads;
  struct proc *task = inf->task;

  inf->threads_up_to_date = !inf->running;

  if (task)
    {
      error_t err = task_threads (task->port, &threads, &num_threads);
      inf_debug (inf, "fetching threads");
      if (err)
	/* TASK must be dead.  */
	{
	  task->port = MACH_PORT_NULL;
	  _proc_free (task);
	  task = inf->task = 0;
	}
    }

  if (!task)
    {
      num_threads = 0;
      inf_debug (inf, "no task");
    }

  {
    unsigned search_start = 0;	/* Make things normally linear.  */
    /* Which thread in PROCS corresponds to each task thread, & the task.  */
    struct proc *matched[num_threads + 1];
    /* The last thread in INF->threads, so we can add to the end.  */
    struct proc *last = 0;
    /* The current thread we're considering. */
    struct proc *thread = inf->threads;

    bzero (matched, sizeof (matched));

    while (thread)
      {
	unsigned left;

	for (i = search_start, left = num_threads; left; i++, left--)
	  {
	    if (i >= num_threads)
	      i -= num_threads; /* I wrapped around.  */
	    if (thread->port == threads[i])
	      /* We already know about this thread.  */
	      {
		matched[i] = thread;
		last = thread;
		thread = thread->next;
		search_start++;
		break;
	      }
	  }

	if (! left)
	  {
	    proc_debug (thread, "died!");
	    thread->port = MACH_PORT_NULL;
	    thread = _proc_free (thread); /* THREAD is dead.  */
	    (last ? last->next : inf->threads) = thread;
	  }
      }

    for (i = 0; i < num_threads; i++)
      if (matched[i])
	/* Throw away the duplicate send right.  */
	mach_port_deallocate (mach_task_self (), threads[i]);
      else
	/* THREADS[I] is a thread we don't know about yet!  */
	{
	  thread = make_proc (inf, threads[i], next_thread_id++);
	  (last ? last->next : inf->threads) = thread;
	  last = thread;
	  proc_debug (thread, "new thread: %d", threads[i]);
	  add_thread (thread->tid); /* Tell GDB's generic thread code.  */
	}

    vm_deallocate(mach_task_self(),
		  (vm_address_t)threads, (num_threads * sizeof(thread_t)));
  }
}

/* Makes sure that INF's thread list is synced with the actual process.  */
inline int
inf_update_procs (struct inf *inf)
{
  if (! inf->task)
    return 0;
  if (! inf->threads_up_to_date)
    inf_validate_procs (inf);
  return !!inf->task;
}

/* Sets the resume_sc of each thread in inf.  That of RUN_THREAD is set to 0,
   and others are set to their run_sc if RUN_OTHERS is true, and otherwise
   their pause_sc.  */
inline void 
inf_set_threads_resume_sc (struct inf *inf,
			   struct proc *run_thread, int run_others)
{
  struct proc *thread;
  inf_update_procs (inf);
  for (thread = inf->threads; thread; thread = thread->next)
    if (thread == run_thread)
      thread->resume_sc = 0;
    else if (run_others)
      thread->resume_sc = thread->run_sc;
    else
      thread->resume_sc = thread->pause_sc;
}

/* Cause INF to continue execution immediately; individual threads may still
   be suspended (but their suspend counts will be updated).  */
inline void
inf_resume (struct inf *inf)
{
  struct proc *thread;

  inf_update_procs (inf);

  for (thread = inf->threads; thread; thread = thread->next)
    thread->sc = thread->resume_sc;

  if (inf->task)
    inf->task->sc = 0;

  inf_update_suspends (inf);
}

/* Cause INF to stop execution immediately; individual threads may still
   be running.  */
inline void
inf_suspend (struct inf *inf)
{
  struct proc *thread;

  inf_update_procs (inf);

  for (thread = inf->threads; thread; thread = thread->next)
    thread->sc = thread->pause_sc;

  if (inf->task)
    inf->task->sc = inf->pause_sc;

  inf_update_suspends (inf);
}

/* INF has one thread PROC that is in single-stepping mode.  This functions
   changes it to be PROC, changing any old step_thread to be a normal one.  A
   PROC of 0 clears an any existing value.  */
void
inf_set_step_thread (struct inf *inf, struct proc *thread)
{
  assert (!thread || proc_is_thread (thread));

  if (thread)
    inf_debug (inf, "setting step thread: %d/%d", inf->pid, thread->tid);
  else
    inf_debug (inf, "clearing step thread");

  if (inf->step_thread != thread)
    {
      if (inf->step_thread && inf->step_thread->port != MACH_PORT_NULL)
	if (! proc_trace (inf->step_thread, 0))
	  return;
      if (thread && proc_trace (thread, 1))
	inf->step_thread = thread;
      else
	inf->step_thread = 0;
    }
}

/* Set up the thread resume_sc's so that only the signal thread is running
   (plus whatever other thread are set to always run).  Returns true if we
   did so, or false if we can't find a signal thread.  */
inline int
inf_set_threads_resume_sc_for_signal_thread (struct inf *inf)
{
  if (inf->signal_thread)
    {
      inf_set_threads_resume_sc (inf, inf->signal_thread, 0);
      return 1;
    }
  else
    return 0;
}

static void
inf_update_signal_thread (struct inf *inf)
{
  /* XXX for now we assume that if there's a msgport, the 2nd thread is
     the signal thread.  */
  inf->signal_thread = inf->threads ? inf->threads->next : 0;
}

/* Detachs from INF's inferior task, letting it run once again...  */
void
inf_detach (struct inf *inf)
{
  struct proc *task = inf->task;

  inf_debug (inf, "detaching...");

  inf_clear_wait (inf);
  inf_set_step_thread (inf, 0);

  if (task)
    {
      struct proc *thread;

      inf_set_traced (inf, 0);
      if (inf->stopped)
	inf_signal (inf, TARGET_SIGNAL_0);

      proc_restore_exc_port (task);
      task->sc = 0;

      for (thread = inf->threads; thread; thread = thread->next)
	{
	  proc_restore_exc_port (thread);
	  thread->sc = 0;
	}

      inf_update_suspends (inf);
    }

  inf_cleanup (inf);
}

/* Attaches INF to the process with process id PID, returning it in a suspended
   state suitable for debugging.  */
void
inf_attach (struct inf *inf, int pid)
{
  error_t err;
  task_t task;

  inf_debug (inf, "attaching: %d", pid);

  err = proc_pid2task (proc_server, pid, &task);
  if (err)
    error ("Error getting task for pid %d: %s", pid, strerror (err));

  if (inf->pid)
    inf_detach (inf);

  inf_startup (inf, pid, task);
}

/* Makes sure that we've got our exception ports entrenched in the process. */
void inf_steal_exc_ports (struct inf *inf)
{
  struct proc *thread;

  inf_debug (inf, "stealing exception ports");

  inf_set_step_thread (inf, 0);	/* The step thread is special. */

  proc_steal_exc_port (inf->task, inf->event_port);
  for (thread = inf->threads; thread; thread = thread->next)
    proc_steal_exc_port (thread, MACH_PORT_NULL);
}

/* Makes sure the process has its own exception ports.  */
void inf_restore_exc_ports (struct inf *inf)
{
  struct proc *thread;

  inf_debug (inf, "restoring exception ports");

  inf_set_step_thread (inf, 0);	/* The step thread is special. */

  proc_restore_exc_port (inf->task);
  for (thread = inf->threads; thread; thread = thread->next)
    proc_restore_exc_port (thread);
}

/* Deliver signal SIG to INF.  If INF is stopped, delivering a signal, even
   signal 0, will continue it.  INF is assumed to be in a paused state, and
   the resume_sc's of INF's threads may be affected.  */
void
inf_signal (struct inf *inf, enum target_signal sig)
{
  error_t err = 0;
  int host_sig = target_signal_to_host (sig);

#define NAME target_signal_to_name (sig)

  if (host_sig >= _NSIG)
    /* A mach exception.  Exceptions are encoded in the signal space by
       putting them after _NSIG; this assumes they're positive (and not
       extremely large)!  */
    {
      struct inf_wait *w = &inf->wait;
      if (w->status.kind == TARGET_WAITKIND_STOPPED
	  && w->status.value.sig == sig
	  && w->thread && !w->thread->aborted)
	/* We're passing through the last exception we received.  This is
	   kind of bogus, because exceptions are per-thread whereas gdb
	   treats signals as per-process.  We just forward the exception to
	   the correct handler, even it's not for the same thread as TID --
	   i.e., we pretend it's global.  */
	{
	  struct exc_state *e = &w->exc;
	  inf_debug (inf, "passing through exception:"
		     " task = %d, thread = %d, exc = %d"
		     ", code = %d, subcode = %d",
		     w->thread->port, inf->task->port,
		     e->exception, e->code, e->subcode);
	  err =
	    exception_raise_request (e->handler,
				     e->reply, MACH_MSG_TYPE_MOVE_SEND_ONCE,
				     w->thread->port, inf->task->port,
				     e->exception, e->code, e->subcode);
	}
      else
	warning ("Can't forward spontaneous exception (%s).", NAME);
    }
  else
    /* A Unix signal.  */
    if (inf->stopped)
      /* The process is stopped an expecting a signal.  Just send off a
	 request and let it get handled when we resume everything.  */
      {
	inf_debug (inf, "sending %s to stopped process", NAME);
	err =
	  INF_MSGPORT_RPC (inf,
			   msg_sig_post_untraced_request (msgport,
							  inf->event_port,
							  MACH_MSG_TYPE_MAKE_SEND_ONCE,
							  host_sig,
							  refport));
	if (! err)
	  /* Posting an untraced signal automatically continues it.
	     We clear this here rather than when we get the reply
	     because we'd rather assume it's not stopped when it
	     actually is, than the reverse.  */
	  inf->stopped = 0;
      }
    else
      /* It's not expecting it.  We have to let just the signal thread
	 run, and wait for it to get into a reasonable state before we
	 can continue the rest of the process.  When we finally resume the
	 process the signal we request will be the very first thing that
	 happens. */
      {
	inf_debug (inf, "sending %s to unstopped process (so resuming signal thread)", NAME);
	err = 
	  INF_RESUME_MSGPORT_RPC (inf,
				  msg_sig_post_untraced (msgport,
							 host_sig, refport));
      }

  if (err == EIEIO)
    /* Can't do too much... */
    warning ("Can't deliver signal %s: No signal thread.", NAME);
  else if (err)
    warning ("Delivering signal %s: %s", NAME, strerror (err));

#undef NAME
}

/* The inferior used for all gdb target ops.  */
struct inf *current_inferior = 0;

/* The inferior being waited for by gnu_wait.  Since GDB is decidely not
   multi-threaded, we don't bother to lock this.  */
struct inf *waiting_inf;

/* Wait for something to happen in the inferior, returning what in STATUS. */
static int
gnu_wait (int tid, struct target_waitstatus *status)
{
  struct msg {
    mach_msg_header_t hdr;
    mach_msg_type_t type;
    int data[8000];
  } msg;
  error_t err;
  struct proc *thread;
  struct inf *inf = current_inferior;

  waiting_inf = inf;

  inf_debug (inf, "waiting for: %d", tid);

 rewait:
  if (proc_wait_pid != inf->pid && !inf->no_wait)
    /* Always get information on events from the proc server.  */
    {
      inf_debug (inf, "requesting wait on pid %d", inf->pid);

      if (proc_wait_pid)
	/* The proc server is single-threaded, and only allows a single
	   outstanding wait request, so we have to cancel the previous one. */
	{
	  inf_debug (inf, "cancelling previous wait on pid %d", proc_wait_pid);
	  interrupt_operation (proc_server);
	}

      err =
	proc_wait_request (proc_server, inf->event_port, inf->pid, WUNTRACED);
      if (err)
	warning ("wait request failed: %s", strerror (err));
      else
	{
	  inf_debug (inf, "waits pending: %d", proc_waits_pending);
	  proc_wait_pid = inf->pid;
	  /* Even if proc_waits_pending was > 0 before, we still won't get
	     any other replies, because it was either from a different INF,
	     or a different process attached to INF -- and the event port,
	     which is the wait reply port, changes when you switch processes.*/
	  proc_waits_pending = 1;
	}
    }

  inf_clear_wait (inf);

  /* What can happen? (1) Dead name notification; (2) Exceptions arrive;
     (3) wait reply from the proc server.  */

  inf_debug (inf, "waiting for an event...");
  err = _hurd_intr_rpc_mach_msg (&msg.hdr, MACH_RCV_MSG, 0,
				 sizeof (struct msg),
				 inf->event_port, MACH_PORT_NULL);

  /* Re-suspend the task.  */
  inf_suspend (inf);

  if (err == EINTR)
    inf_debug (inf, "interrupted");
  else if (err)
    error ("Couldn't wait for an event: %s", strerror (err));
  else
    {
      struct {
	mach_msg_header_t hdr;
	mach_msg_type_t err_type;
	kern_return_t err;
	char noise[200];
      } reply;

      inf_debug (inf, "event: msgid = %d", msg.hdr.msgh_id);

      /* Handle what we got.  */
      if (! notify_server (&msg.hdr, &reply.hdr)
	  && ! exc_server (&msg.hdr, &reply.hdr)
	  && ! process_reply_server (&msg.hdr, &reply.hdr)
	  && ! msg_reply_server (&msg.hdr, &reply.hdr))
	/* Whatever it is, it's something strange.  */
	error ("Got a strange event, msg id = %d.", msg.hdr.msgh_id);

      if (reply.err)
	error ("Handling event, msgid = %d: %s",
	       msg.hdr.msgh_id, strerror (reply.err));
    }

  if (inf->pending_execs)
    /* We're waiting for the inferior to finish execing.  */
    {
      struct inf_wait *w = &inf->wait;
      enum target_waitkind kind = w->status.kind;

      if (kind == TARGET_WAITKIND_SPURIOUS)
	/* Since gdb is actually counting the number of times the inferior
	   stops, expecting one stop per exec, we only return major events
	   while execing.  */
	w->suppress = 1;
      else if (kind == TARGET_WAITKIND_STOPPED
	       && w->status.value.sig == TARGET_SIGNAL_TRAP)
	/* Ah hah!  A SIGTRAP from the inferior while starting up probably
	   means we've succesfully completed an exec!  */
	if (--inf->pending_execs == 0)
	  /* We're done!  */
	  {
	    prune_threads (1);	/* Get rid of the old shell threads */
	    renumber_threads (0); /* Give our threads reasonable names. */
	  }
    }

  if (inf->wait.suppress)
    /* Some totally spurious event happened that we don't consider
       worth returning to gdb.  Just keep waiting.  */
    {
      inf_debug (inf, "suppressing return, rewaiting...");
      inf_resume (inf);
      goto rewait;
    }

  /* Pass back out our results.  */
  bcopy (&inf->wait.status, status, sizeof (*status));

  thread = inf->wait.thread;
  if (thread)
    tid = thread->tid;
  else
    thread = inf_tid_to_thread (inf, tid);

  if (!thread || thread->port == MACH_PORT_NULL)
    /* TID is dead; try and find a new thread.  */
    if (inf_update_procs (inf) && inf->threads)
      tid = inf->threads->tid;	/* The first available thread.  */
    else
      tid = -1;

  if (thread && tid >= 0 && status->kind != TARGET_WAITKIND_SPURIOUS
      && inf->pause_sc == 0 && thread->pause_sc == 0)
    /* If something actually happened to THREAD, make sure we suspend it.  */
    {
      thread->sc = 1;
      inf_update_suspends (inf);
    }      

  inf_debug (inf, "returning tid = %d, status = %s (%d)", tid,
	     status->kind == TARGET_WAITKIND_EXITED ? "EXITED"
	     : status->kind == TARGET_WAITKIND_STOPPED ? "STOPPED"
	     : status->kind == TARGET_WAITKIND_SIGNALLED ? "SIGNALLED"
	     : status->kind == TARGET_WAITKIND_LOADED ? "LOADED"
	     : status->kind == TARGET_WAITKIND_SPURIOUS ? "SPURIOUS"
	     : "?",
	     status->value.integer);

  return tid;
}

/* The rpc handler called by exc_server.  */
error_t
S_exception_raise_request (mach_port_t port, mach_port_t reply_port,
			   thread_t thread_port, task_t task_port,
			   int exception, int code, int subcode)
{
  struct inf *inf = waiting_inf;
  struct proc *thread = inf_port_to_thread (inf, thread_port);

  inf_debug (waiting_inf,
	     "thread = %d, task = %d, exc = %d, code = %d, subcode = %d",
	     thread_port, task_port, exception, code);

  if (!thread)
    /* We don't know about thread?  */
    {
      inf_update_procs (inf);
      thread = inf_port_to_thread (inf, thread_port);
      if (!thread)
	/* Give up, the generating thread is gone.  */
	return 0;
    }

  mach_port_deallocate (mach_task_self (), thread_port);
  mach_port_deallocate (mach_task_self (), task_port);

  if (! thread->aborted)
    /* THREAD hasn't been aborted since this exception happened (abortion
       clears any exception state), so it must be real.  */
    {
      /* Store away the details; this will destroy any previous info.  */
      inf->wait.thread = thread;

      inf->wait.status.kind = TARGET_WAITKIND_STOPPED;

      if (exception == EXC_BREAKPOINT)
	/* GDB likes to get SIGTRAP for breakpoints.  */
	{
	  inf->wait.status.value.sig = TARGET_SIGNAL_TRAP;
	  mach_port_deallocate (mach_task_self (), reply_port);
	}
      else
	/* Record the exception so that we can forward it later.  */
	{
	  if (thread->exc_port == port)
	    inf->wait.exc.handler = thread->saved_exc_port;
	  else
	    {
	      inf->wait.exc.handler = inf->task->saved_exc_port;
	      assert (inf->task->exc_port == port);
	    }
	  if (inf->wait.exc.handler != MACH_PORT_NULL)
	    /* Add a reference to the exception handler. */
	    mach_port_mod_refs (mach_task_self (),
				inf->wait.exc.handler, MACH_PORT_RIGHT_SEND,
				1);

	  inf->wait.exc.exception = exception;
	  inf->wait.exc.code = code;
	  inf->wait.exc.subcode = subcode;
	  inf->wait.exc.reply = reply_port;

	  /* Exceptions are encoded in the signal space by putting them after
	     _NSIG; this assumes they're positive (and not extremely large)! */
	  inf->wait.status.value.sig =
	    target_signal_from_host (_NSIG + exception);
	}
    }
  else
    /* A supppressed exception, which ignore.  */
    {
      inf->wait.suppress = 1;
      mach_port_deallocate (mach_task_self (), reply_port);
    }

  return 0;
}

/* Fill in INF's wait field after a task has died without giving us more
   detailed information.  */
void
inf_task_died_status (struct inf *inf)
{
  warning ("Pid %d died with unknown exit status, using SIGKILL.", inf->pid);
  inf->wait.status.kind = TARGET_WAITKIND_SIGNALLED;
  inf->wait.status.value.sig = TARGET_SIGNAL_KILL;
}

/* Notify server routines.  The only real one is dead name notification.  */

error_t
do_mach_notify_dead_name (mach_port_t notify, mach_port_t dead_port)
{
  struct inf *inf = waiting_inf;

  inf_debug (waiting_inf, "port = %d", dead_port);

  if (inf->task && inf->task->port == dead_port)
    {
      proc_debug (inf->task, "is dead");
      inf->task->port = MACH_PORT_NULL;
      if (proc_wait_pid == inf->pid)
	/* We have a wait outstanding on the process, which will return more
	   detailed information, so delay until we get that.  */
	inf->wait.suppress = 1;
      else
	/* We never waited for the process (maybe it wasn't a child), so just
	   pretend it got a SIGKILL.  */
	inf_task_died_status (inf);
    }
  else
    {
      struct proc *thread = inf_port_to_thread (inf, dead_port);
      if (thread)
	{
	  proc_debug (thread, "is dead");
	  thread->port = MACH_PORT_NULL;
	}
    }

  mach_port_deallocate (mach_task_self (), dead_port);
  inf->threads_up_to_date = 0; /* Just in case */

  return 0;
}

static error_t
ill_rpc (char *fun)
{
  warning ("illegal rpc: %s", fun);
  return 0;
}

error_t
do_mach_notify_no_senders (mach_port_t notify, mach_port_mscount_t count)
{
  return ill_rpc (__FUNCTION__);
}

error_t
do_mach_notify_port_deleted (mach_port_t notify, mach_port_t name)
{
  return ill_rpc (__FUNCTION__);
}

error_t
do_mach_notify_msg_accepted (mach_port_t notify, mach_port_t name)
{
  return ill_rpc (__FUNCTION__);
}

error_t
do_mach_notify_port_destroyed (mach_port_t notify, mach_port_t name)
{
  return ill_rpc (__FUNCTION__);
}

error_t
do_mach_notify_send_once (mach_port_t notify)
{
  return ill_rpc (__FUNCTION__);
}

/* Process_reply server routines.  We only use process_wait_reply.  */

error_t
S_proc_wait_reply (mach_port_t reply, error_t err,
		 int status, rusage_t rusage, pid_t pid)
{
  struct inf *inf = waiting_inf;

  inf_debug (inf, "err = %s, pid = %d, status = 0x%x",
	     err ? strerror (err) : "0", pid, status);

  if (err && proc_wait_pid && (!inf->task || !inf->task->port))
    /* Ack.  The task has died, but the task-died notification code didn't
       tell anyone because it thought a more detailed reply from the
       procserver was forthcoming.  However, we now learn that won't
       happen...  So we have to act like the task just died, and this time,
       tell the world.  */
    inf_task_died_status (inf);

  if (--proc_waits_pending == 0)
    /* PROC_WAIT_PID represents the most recent wait.  We will always get
       replies in order because the proc server is single threaded.  */
    proc_wait_pid = 0;

  inf_debug (inf, "waits pending now: %d", proc_waits_pending);

  if (err)
    {
      if (err != EINTR)
	{
	  warning ("Can't wait for pid %d: %s", inf->pid, strerror (err));
	  inf->no_wait = 1;

	  /* Since we can't see the inferior's signals, don't trap them.  */
	  inf_set_traced (inf, 0);
	}
    }
  else if (pid == inf->pid)
    {
      store_waitstatus (&inf->wait.status, status);
      if (inf->wait.status.kind == TARGET_WAITKIND_STOPPED)
	/* The process has sent us a signal, and stopped itself in a sane
	   state pending our actions.  */
	{
	  inf_debug (inf, "process has stopped itself");
	  inf->stopped = 1;

	  /* We recheck the task suspend count here because the crash server
	     messes with it in an unfriendly way, right before `stopping'.  */
	  inf_validate_task_sc (inf);
	}
    }
  else
    inf->wait.suppress = 1;	/* Something odd happened.  Ignore.  */

  return 0;
}

error_t
S_proc_setmsgport_reply (mach_port_t reply, error_t err,
			 mach_port_t old_msg_port)
{
  return ill_rpc (__FUNCTION__);
}

error_t
S_proc_getmsgport_reply (mach_port_t reply, error_t err, mach_port_t msg_port)
{
  return ill_rpc (__FUNCTION__);
}

/* Msg_reply server routines.  We only use msg_sig_post_untraced_reply.  */

error_t
S_msg_sig_post_untraced_reply (mach_port_t reply, error_t err)
{
  struct inf *inf = waiting_inf;

  if (err == EBUSY)
    /* EBUSY is what we get when the crash server has grabbed control of the
       process and doesn't like what signal we tried to send it.  Just act
       like the process stopped (using a signal of 0 should mean that the
       *next* time the user continues, it will pass signal 0, which the crash
       server should like).  */
    {
      inf->wait.status.kind = TARGET_WAITKIND_STOPPED;
      inf->wait.status.value.sig = TARGET_SIGNAL_0;
    }
  else if (err)
    warning ("Signal delivery failed: %s", strerror (err));

  if (err)
    /* We only get this reply when we've posted a signal to a process which we
       thought was stopped, and which we expected to continue after the signal.
       Given that the signal has failed for some reason, it's reasonable to
       assume it's still stopped.  */
    inf->stopped = 1;
  else
    inf->wait.suppress = 1;

  return 0;
}

error_t
S_msg_sig_post_reply (mach_port_t reply, error_t err)
{
  return ill_rpc (__FUNCTION__);
}

/* Returns the number of messages queued for the receive right PORT.  */
static mach_port_msgcount_t
port_msgs_queued (mach_port_t port)
{
  struct mach_port_status status;
  error_t err =
    mach_port_get_receive_status (mach_task_self (), port, &status);

  if (err)
    return 0;
  else
    return status.mps_msgcount;
}

/* Resume execution of the inferior process.

   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.

   TID  STEP:
   -1   true   Single step the current thread allowing other threads to run.
   -1   false  Continue the current thread allowing other threads to run.
   X    true   Single step the given thread, don't allow any others to run.
   X    false  Continue the given thread, do not allow any others to run.
   (Where X, of course, is anything except -1)

   Note that a resume may not `take' if there are pending exceptions/&c
   still unprocessed from the last resume we did (any given resume may result
   in multiple events returned by wait).
*/
static void
gnu_resume (int tid, int step, enum target_signal sig)
{
  struct proc *step_thread = 0;
  struct inf *inf = current_inferior;

  inf_debug (inf, "tid = %d, step = %d, sig = %d", tid, step, sig);

  if (sig != TARGET_SIGNAL_0 || inf->stopped)
    inf_signal (inf, sig);
  else if (inf->wait.exc.reply != MACH_PORT_NULL)
    /* We received an exception to which we have chosen not to forward, so
       abort the faulting thread, which will perhaps retake it.  */
    {
      proc_abort (inf->wait.thread, 1);
      warning ("Aborting %s with unforwarded exception %s.",
	       proc_string (inf->wait.thread),
	       target_signal_to_name (inf->wait.status.value.sig));
    }

  if (port_msgs_queued (inf->event_port))
    /* If there are still messages in our event queue, don't bother resuming
       the process, as we're just going to stop it right away anyway. */
    return;

  if (tid < 0)
    /* Allow all threads to run, except perhaps single-stepping one.  */
    {
      inf_debug (inf, "running all threads; tid = %d", inferior_pid);
      tid = inferior_pid;	/* What to step. */
      inf_set_threads_resume_sc (inf, 0, 1);
    }
  else
    /* Just allow a single thread to run.  */
    {
      struct proc *thread = inf_tid_to_thread (inf, tid);
      assert (thread);

      inf_debug (inf, "running one thread: %d/%d", inf->pid, thread->tid);
      inf_set_threads_resume_sc (inf, thread, 0);
    }

  if (step)
    {
      step_thread = inf_tid_to_thread (inf, tid);
      assert (step_thread);
      inf_debug (inf, "stepping thread: %d/%d", inf->pid, step_thread->tid);
    }
  if (step_thread != inf->step_thread)
    inf_set_step_thread (inf, step_thread);

  inf_debug (inf, "here we go...");
  inf_resume (inf);
}

static void
gnu_kill_inferior ()
{
  struct proc *task = current_inferior->task;
  if (task)
    {
      proc_debug (task, "terminating...");
      task_terminate (task->port);
      task->port = MACH_PORT_NULL;
      inf_validate_procs (current_inferior); /* Clear out the thread list &c */
    }
  target_mourn_inferior ();
}

/* Clean up after the inferior dies.  */

static void
gnu_mourn_inferior ()
{
  inf_debug (current_inferior, "rip");
  inf_detach (current_inferior);
  unpush_target (&gnu_ops);
  generic_mourn_inferior ();
}

/* Fork an inferior process, and start debugging it.  */

/* Set INFERIOR_PID to the first thread available in the child, if any.  */
static void
pick_first_thread ()
{
  if (current_inferior->task && current_inferior->threads)
    /* The first thread.  */
    inferior_pid = current_inferior->threads->tid;
  else
    /* What may be the next thread.  */
    inferior_pid = next_thread_id;
}

static struct inf *
cur_inf ()
{
  if (! current_inferior)
    current_inferior = make_inf ();
  return current_inferior;
}

static void
gnu_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  struct inf *inf = cur_inf ();

  void trace_me ()
    {
      /* We're in the child; make this process stop as soon as it execs.  */
      inf_debug (inf, "tracing self");
      ptrace (PTRACE_TRACEME, 0, 0, 0);
    }
  void attach_to_child (int pid)
    {
      /* Attach to the now stopped child, which is actually a shell...  */
      inf_debug (inf, "attaching to child: %d", pid);

      inf_attach (inf, pid);
      pick_first_thread ();

      attach_flag = 0;
      push_target (&gnu_ops);

      inf->pending_execs = 2;
      inf->traced = 1;

      /* Now let the child run again, knowing that it will stop immediately
	 because of the ptrace. */
      inf_resume (inf);

      startup_inferior (pid, inf->pending_execs);
    }

  inf_debug (inf, "creating inferior");

  fork_inferior (exec_file, allargs, env, trace_me, attach_to_child, NULL);

  inf_update_signal_thread (inf);
  inf_set_traced (inf, inf->want_signals);

  /* Execing the process will have trashed our exception ports; steal them
     back (or make sure they're restored if the user wants that).  */
  if (inf->want_exceptions)
    inf_steal_exc_ports (inf);
  else
    inf_restore_exc_ports (inf);

  /* Here we go!  */
  proceed ((CORE_ADDR) -1, 0, 0);
}

/* Mark our target-struct as eligible for stray "run" and "attach"
   commands.  */
static int
gnu_can_run ()
{
  return 1;
}

#ifdef ATTACH_DETACH

/* Attach to process PID, then initialize for debugging it
   and wait for the trace-trap that results from attaching.  */
static void
gnu_attach (args, from_tty)
     char *args;
     int from_tty;
{
  int pid;
  char *exec_file;
  struct inf *inf = cur_inf ();

  if (!args)
    error_no_arg ("PID to attach");

  pid = atoi (args);

  if (pid == getpid())		/* Trying to masturbate? */
    error ("I refuse to debug myself!");

  if (from_tty)
    {
      exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_unfiltered ("Attaching to program `%s', pid %d\n",
			   exec_file, pid);
      else
	printf_unfiltered ("Attaching to pid %d\n", pid);

      gdb_flush (gdb_stdout);
    }

  inf_debug (inf, "attaching to pid: %d", pid);

  inf_attach (inf, pid);
  inf_update_procs (inf);

  pick_first_thread ();

  attach_flag = 1;
  push_target (&gnu_ops);

  inf_update_signal_thread (inf);
  inf_set_traced (inf, inf->want_signals);

  /* If the process was stopped before we attached, make it continue the next
     time the user does a continue.  */
  inf_validate_stopped (inf);
  inf_validate_task_sc (inf);
}

/* Take a program previously attached to and detaches it.
   The program resumes execution and will no longer stop
   on signals, etc.  We'd better not have left any breakpoints
   in the program or it'll die when it hits one.  For this
   to work, it may be necessary for the process to have been
   previously attached.  It *might* work if the program was
   started via fork.  */
static void
gnu_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file)
	printf_unfiltered ("Detaching from program `%s' pid %d\n",
			   exec_file, current_inferior->pid);
      else
	printf_unfiltered ("Detaching from pid %d\n", current_inferior->pid);
      gdb_flush (gdb_stdout);
    }
  
  inf_detach (current_inferior);

  inferior_pid = 0;

  unpush_target (&gnu_ops);		/* Pop out of handling an inferior */
}
#endif /* ATTACH_DETACH */

static void
gnu_terminal_init_inferior ()
{
  assert (current_inferior);
  terminal_init_inferior_with_pgrp (current_inferior->pid);
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
gnu_prepare_to_store ()
{
#ifdef CHILD_PREPARE_TO_STORE
  CHILD_PREPARE_TO_STORE ();
#endif
}

static void
gnu_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a Unix child process.");
}

static void
gnu_stop ()
{
  error ("to_stop target function not implemented");
}

static int
gnu_thread_alive (int tid)
{
  inf_update_procs (current_inferior);
  return !!inf_tid_to_thread (current_inferior, tid);
}

/*
 * Read inferior task's LEN bytes from ADDR and copy it to MYADDR
 * in gdb's address space.
 *
 * Return 0 on failure; number of bytes read otherwise.
 */
int
gnu_read_inferior (task, addr, myaddr, length)
     task_t task;
     CORE_ADDR addr;
     char *myaddr;
     int length;
{
  error_t err;
  vm_address_t low_address = (vm_address_t) trunc_page (addr);
  vm_size_t aligned_length =
    (vm_size_t) round_page (addr+length) - low_address;
  pointer_t    copied;
  int	       copy_count;

  /* Get memory from inferior with page aligned addresses */
  err = vm_read (task, low_address, aligned_length, &copied, &copy_count);
  if (err)
    return 0;

  err = hurd_safe_copyin (myaddr, (void*)addr - low_address + copied, length);
  if (err)
    {
      warning ("Read from inferior faulted: %s", strerror (err));
      length = 0;
    }

  err = vm_deallocate (mach_task_self (), copied, copy_count);
  if (err)
    warning ("gnu_read_inferior vm_deallocate failed: %s", strerror (err));

  return length;
}

#define CHK_GOTO_OUT(str,ret) \
  do if (ret != KERN_SUCCESS) { errstr = #str; goto out; } while(0)

struct vm_region_list {
  struct vm_region_list *next;
  vm_prot_t	protection;
  vm_address_t  start;
  vm_size_t	length;
};

struct obstack  region_obstack;

/*
 * Write inferior task's LEN bytes from ADDR and copy it to MYADDR
 * in gdb's address space.
 */
int
gnu_write_inferior (task, addr, myaddr, length)
     task_t task;
     CORE_ADDR addr;
     char *myaddr;
     int length;
{
  error_t err = 0;
  vm_address_t low_address       = (vm_address_t) trunc_page (addr);
  vm_size_t    aligned_length = 
    			(vm_size_t) round_page (addr+length) - low_address;
  pointer_t    copied;
  int	       copy_count;
  int	       deallocate = 0;

  char         *errstr = "Bug in gnu_write_inferior";

  struct vm_region_list *region_element;
  struct vm_region_list *region_head = (struct vm_region_list *)NULL;

  /* Get memory from inferior with page aligned addresses */
  err = vm_read (task,
		 low_address,
		 aligned_length,
		 &copied,
		 &copy_count);
  CHK_GOTO_OUT ("gnu_write_inferior vm_read failed", err);

  deallocate++;

  err = hurd_safe_copyout ((void*)addr - low_address + copied, myaddr, length);
  CHK_GOTO_OUT ("Write to inferior faulted", err);

  obstack_init (&region_obstack);

  /* Do writes atomically.
   * First check for holes and unwritable memory.
   */
  {
    vm_size_t    remaining_length  = aligned_length;
    vm_address_t region_address    = low_address;

    struct vm_region_list *scan;

    while(region_address < low_address + aligned_length)
      {
	vm_prot_t protection;
	vm_prot_t max_protection;
	vm_inherit_t inheritance;
	boolean_t shared;
	mach_port_t object_name;
	vm_offset_t offset;
	vm_size_t   region_length = remaining_length;
	vm_address_t old_address  = region_address;
    
	err = vm_region (task,
			 &region_address,
			 &region_length,
			 &protection,
			 &max_protection,
			 &inheritance,
			 &shared,
			 &object_name,
			 &offset);
	CHK_GOTO_OUT ("vm_region failed", err);

	/* Check for holes in memory */
	if (old_address != region_address)
	  {
	    warning ("No memory at 0x%x. Nothing written",
		     old_address);
	    err = KERN_SUCCESS;
	    length = 0;
	    goto out;
	  }

	if (!(max_protection & VM_PROT_WRITE))
	  {
	    warning ("Memory at address 0x%x is unwritable. Nothing written",
		     old_address);
	    err = KERN_SUCCESS;
	    length = 0;
	    goto out;
	  }

	/* Chain the regions for later use */
	region_element = 
	  (struct vm_region_list *)
	    obstack_alloc (&region_obstack, sizeof (struct vm_region_list));
    
	region_element->protection = protection;
	region_element->start      = region_address;
	region_element->length     = region_length;

	/* Chain the regions along with protections */
	region_element->next = region_head;
	region_head          = region_element;
	
	region_address += region_length;
	remaining_length = remaining_length - region_length;
      }

    /* If things fail after this, we give up.
     * Somebody is messing up inferior_task's mappings.
     */
    
    /* Enable writes to the chained vm regions */
    for (scan = region_head; scan; scan = scan->next)
      {
	boolean_t protection_changed = FALSE;
	
	if (!(scan->protection & VM_PROT_WRITE))
	  {
	    err = vm_protect (task,
			      scan->start,
			      scan->length,
			      FALSE,
			      scan->protection | VM_PROT_WRITE);
	    CHK_GOTO_OUT ("vm_protect: enable write failed", err);
	  }
      }

    err = vm_write (task,
		    low_address,
		    copied,
		    aligned_length);
    CHK_GOTO_OUT ("vm_write failed", err);
	
    /* Set up the original region protections, if they were changed */
    for (scan = region_head; scan; scan = scan->next)
      {
	boolean_t protection_changed = FALSE;
	
	if (!(scan->protection & VM_PROT_WRITE))
	  {
	    err = vm_protect (task,
			      scan->start,
			      scan->length,
			      FALSE,
			      scan->protection);
	    CHK_GOTO_OUT ("vm_protect: enable write failed", err);
	  }
      }
  }

 out:
  if (deallocate)
    {
      obstack_free (&region_obstack, 0);
      
      (void) vm_deallocate (mach_task_self (),
			    copied,
			    copy_count);
    }

  if (err != KERN_SUCCESS)
    {
      warning ("%s: %s", errstr, mach_error_string (err));
      return 0;
    }

  return length;
}

/* Return 0 on failure, number of bytes handled otherwise.  */
static int
gnu_xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;	/* IGNORED */
{
  int result;
  task_t task =
    current_inferior
      ? (current_inferior->task ? current_inferior->task->port : 0)
      : 0;

  if (task == MACH_PORT_NULL)
    return 0;
  else
    {
      inf_debug (current_inferior, "%s %p[%d] %s %p",
		 write ? "writing" : "reading", memaddr, len,
		 write ? "<--" : "-->", myaddr);
      if (write)
	return gnu_write_inferior (task, memaddr, myaddr, len);
      else
	return gnu_read_inferior  (task, memaddr, myaddr, len);
    }
}

extern void gnu_store_registers (int regno);
extern void gnu_fetch_registers (int regno);

struct target_ops gnu_ops = {
  "GNU",			/* to_shortname */
  "GNU Hurd process",		/* to_longname */
  "GNU Hurd process",		/* to_doc */
  gnu_open,			/* to_open */
  0,				/* to_close */
  gnu_attach,			/* to_attach */
  gnu_detach,	 		/* to_detach */
  gnu_resume,			/* to_resume */
  gnu_wait,			/* to_wait */
  gnu_fetch_registers,		/* to_fetch_registers */
  gnu_store_registers,		/* to_store_registers */
  gnu_prepare_to_store,		/* to_prepare_to_store */
  gnu_xfer_memory,		/* to_xfer_memory */
  0,				/* to_files_info */
  memory_insert_breakpoint,	/* to_insert_breakpoint */
  memory_remove_breakpoint,	/* to_remove_breakpoint */
  gnu_terminal_init_inferior,	/* to_terminal_init */
  terminal_inferior, 		/* to_terminal_inferior */
  terminal_ours_for_output,	/* to_terminal_ours_for_output */
  terminal_ours,		/* to_terminal_ours */
  child_terminal_info,		/* to_terminal_info */
  gnu_kill_inferior,		/* to_kill */
  0,				/* to_load */
  0,				/* to_lookup_symbol */

  gnu_create_inferior,		/* to_create_inferior */
  gnu_mourn_inferior,		/* to_mourn_inferior */
  gnu_can_run,			/* to_can_run */
  0,				/* to_notice_signals */
  gnu_thread_alive,		/* to_thread_alive */
  gnu_stop,			/* to_stop */
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

char *proc_string (struct proc *proc)
{
  static char tid_str[80];
  if (proc_is_task (proc))
    sprintf (tid_str, "process %d", proc->inf->pid);
  else
    sprintf (tid_str, "thread %d.%d",
	     proc->inf->pid,
	     pid_to_thread_id (proc->tid));
  return tid_str;
}

char *
gnu_target_pid_to_str (int tid)
{
  struct inf *inf = current_inferior;
  struct proc *thread = inf_tid_to_thread (inf, tid);

  if (thread)
    return proc_string (thread);
  else
    {
      static char tid_str[80];
      sprintf (tid_str, "bogus thread id %d", tid);
      return tid_str;
    }
}

/* User task commands.  */

struct cmd_list_element *set_task_cmd_list = 0;
struct cmd_list_element *show_task_cmd_list = 0;

extern struct cmd_list_element *set_thread_default_cmd_list;
extern struct cmd_list_element *show_thread_default_cmd_list;

static int
_parse_bool_arg (char *args, char *t_val, char *f_val, char *cmd_prefix)
{
  if (!args || strcmp (args, t_val) == 0)
    return 1;
  else if (strcmp (args, f_val) == 0)
    return 0;
  else
    error ("Illegal argument for \"%s\" command, should be \"%s\" or \"%s\".",
	   cmd_prefix, t_val, f_val);
}

#define parse_bool_arg(args, cmd_prefix) \
  _parse_bool_arg (args, "on", "off", cmd_prefix)

static void
check_empty (char *args, char *cmd_prefix)
{
  if (args)
    error ("Garbage after \"%s\" command: `%s'", cmd_prefix, args);
}

/* Returns the alive thread named by INFERIOR_PID, or signals an error.  */
static struct proc *
cur_thread ()
{
  struct inf *inf = cur_inf ();
  struct proc *thread = inf_tid_to_thread (inf, inferior_pid);
  if (!thread)
    error ("No current thread.");
  return thread;
}

static void
set_task_pause_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  int old_sc = inf->pause_sc;

  inf->pause_sc = parse_bool_arg (args, "set task pause");

  if (old_sc == 0 && inf->pause_sc != 0)
    /* If the task is currently unsuspended, immediately suspend it,
       otherwise wait until the next time it gets control.  */
    inf_suspend (inf);
}

static void
show_task_pause_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  check_empty (args, "show task pause");
  printf_unfiltered ("The inferior task %s suspended while gdb has control.\n",
		     inf->task
		     ? (inf->pause_sc == 0 ? "isn't" : "is")
		     : (inf->pause_sc == 0 ? "won't be" : "will be"));
}

static void
set_thread_default_pause_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  inf->default_thread_pause_sc =
    parse_bool_arg (args, "set thread default pause") ? 0 : 1;
}

static void
show_thread_default_pause_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  int sc = inf->default_thread_pause_sc;
  check_empty (args, "show thread default pause");
  printf_unfiltered ("New threads %s suspended while gdb has control%s.\n",
		     sc ? "are" : "aren't",
		     !sc && inf->pause_sc ? "(but the task is)" : "");
}

static void
set_thread_default_run_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  inf->default_thread_run_sc =
    parse_bool_arg (args, "set thread default run") ? 0 : 1;
}

static void
show_thread_default_run_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  check_empty (args, "show thread default run");
  printf_unfiltered ("New threads %s allowed to run.\n",
		     inf->default_thread_run_sc == 0 ? "are" : "aren't");
}

/* Steal a send right called NAME in the inferior task, and make it PROC's
   saved exception port.  */
static void
steal_exc_port (struct proc *proc, mach_port_t name)
{
  error_t err;
  mach_port_t port;
  mach_msg_type_name_t port_type;

  if (!proc || !proc->inf->task)
    error ("No inferior task.");

  err = mach_port_extract_right (proc->inf->task->port,
				 name, MACH_MSG_TYPE_COPY_SEND,
				 &port, &port_type);
  if (err)
    error ("Couldn't extract send right %d from inferior: %s",
	   name, strerror (err));

  if (proc->saved_exc_port)
    /* Get rid of our reference to the old one.  */
    mach_port_deallocate (mach_task_self (), proc->saved_exc_port);

  proc->saved_exc_port = port;

  if (! proc->exc_port)
    /* If PROC is a thread, we may not have set its exception port before.
       We can't use proc_steal_exc_port because it also sets saved_exc_port. */
    {
       proc->exc_port = proc->inf->event_port;
       err = proc_set_exception_port (proc, proc->exc_port);
       error ("Can't set exception port for %s: %s",
	      proc_string (proc), strerror (err));
    }
}

static void
set_task_exc_port_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  if (!args)
    error ("No argument to \"set task exception-port\" command.");
  steal_exc_port (inf->task, parse_and_eval_address (args));
}

static void 
set_signals_cmd (char *args, int from_tty)
{
  int trace;
  struct inf *inf = cur_inf ();

  inf->want_signals = parse_bool_arg (args, "set signals");

  if (inf->task && inf->want_signals != inf->traced)
    /* Make this take effect immediately in a running process.  */
    inf_set_traced (inf, inf->want_signals);
}

static void
show_signals_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  check_empty (args, "show signals");
  printf_unfiltered ("The inferior process's signals %s intercepted.\n",
		     inf->task
		     ? (inf->traced ? "are" : "aren't")
		     : (inf->want_signals ? "will be" : "won't be"));
}

static void 
set_stopped_cmd (char *args, int from_tty)
{
  cur_inf ()->stopped = _parse_bool_arg (args, "yes", "no", "set stopped");
}

static void
show_stopped_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  check_empty (args, "show stopped");
  if (! inf->task)
    error ("No current process.");
  printf_unfiltered ("The inferior process %s stopped.\n",
		     inf->stopped ? "is" : "isn't");
}

static void 
set_sig_thread_cmd (char *args, int from_tty)
{
  int tid;
  struct inf *inf = cur_inf ();

  if (!args || (! isdigit (*args) && strcmp (args, "none") != 0))
    error ("Illegal argument to \"set signal-thread\" command.\n"
	   "Should be an integer thread ID, or `none'.");

  if (strcmp (args, "none") == 0)
    inf->signal_thread = 0;
  else
    {
      int tid = thread_id_to_pid (atoi (args));
      if (tid < 0)
	error ("Thread ID %s not known.  Use the \"info threads\" command to\n"
	       "see the IDs of currently known threads.", args);
      inf->signal_thread = inf_tid_to_thread (inf, tid);
    }
}

static void
show_sig_thread_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  check_empty (args, "show signal-thread");
  if (! inf->task)
    error ("No current process.");
  if (inf->signal_thread)
    printf_unfiltered ("The signal thread is %s.\n",
		       proc_string (inf->signal_thread));
  else
    printf_unfiltered ("There is no signal thread.\n");
}

static void 
set_exceptions_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  int val = parse_bool_arg (args, "set exceptions");

  if (inf->task && inf->want_exceptions != val)
    /* Make this take effect immediately in a running process.  */
    /* XXX */;

  inf->want_exceptions = val;
}

static void
show_exceptions_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();
  check_empty (args, "show exceptions");
  printf_unfiltered ("Exceptions in the inferior %s trapped.\n",
		     inf->task
		     ? (inf->want_exceptions ? "are" : "aren't")
		     : (inf->want_exceptions ? "will be" : "won't be"));
}

static void
set_task_cmd (char *args, int from_tty)
{
  printf_unfiltered ("\"set task\" must be followed by the name of a task property.\n");
}

static void
show_task_cmd (char *args, int from_tty)
{
  struct inf *inf = cur_inf ();

  check_empty (args, "show task");

  show_signals_cmd (0, from_tty);
  show_exceptions_cmd (0, from_tty);
  show_task_pause_cmd (0, from_tty);

  if (inf->pause_sc == 0)
    show_thread_default_pause_cmd (0, from_tty);
  show_thread_default_run_cmd (0, from_tty);

  if (inf->task)
    {
      show_stopped_cmd (0, from_tty);
      show_sig_thread_cmd (0, from_tty);
    }
}

static void add_task_commands ()
{
  add_cmd ("pause", class_run, set_thread_default_pause_cmd,
	   "Set whether the new threads are suspended while gdb has control.\n"
	   "This property normally has no effect because the whole task is\n"
	   "suspended, however, that may be disabled with \"set task pause off\".\n"
	   "The default value is \"off\".",
	   &set_thread_default_cmd_list);
  add_cmd ("pause", no_class, show_thread_default_pause_cmd,
	   "Show whether new threads are suspended while gdb has control.",
	   &show_thread_default_cmd_list);
  add_cmd ("run", class_run, set_thread_default_run_cmd,
	   "Set whether new threads are allowed to run (once gdb has noticed them).",
	   &set_thread_default_cmd_list);
  add_cmd ("run", no_class, show_thread_default_run_cmd,
	   "Show whether new threads are allowed to run (once gdb has noticed
them).",
	   &show_thread_default_cmd_list);

  add_cmd ("signals", class_run, set_signals_cmd,
	   "Set whether the inferior process's signals will be intercepted.\n"
	   "Mach exceptions (such as breakpoint traps) are not affected.",
	   &setlist);
  add_alias_cmd ("sigs", "signals", class_run, 1, &setlist);
  add_cmd ("signals", no_class, show_signals_cmd,
	   "Show whether the inferior process's signals will be intercepted.",
	   &showlist);
  add_alias_cmd ("sigs", "signals", no_class, 1, &showlist);

  add_cmd ("signal-thread", class_run, set_sig_thread_cmd,
	   "Set the thread that gdb thinks is the libc signal thread.\n"
	   "This thread is run when delivering a signal to a non-stopped process.",
	   &setlist);
  add_alias_cmd ("sigthread", "signal-thread", class_run, 1, &setlist);
  add_cmd ("signal-thread", no_class, show_sig_thread_cmd,
	   "Set the thread that gdb thinks is the libc signal thread.",
	   &showlist);
  add_alias_cmd ("sigthread", "signal-thread", no_class, 1, &showlist);

  add_cmd ("stopped", class_run, set_stopped_cmd,
	   "Set whether gdb thinks the inferior process is stopped as with SIGSTOP.\n"
	   "Stopped process will be continued by sending them a signal.",
	   &setlist);
  add_cmd ("stopped", no_class, show_signals_cmd,
	   "Show whether gdb thinks the inferior process is stopped as with SIGSTOP.",
	   &showlist);

  add_cmd ("exceptions", class_run, set_exceptions_cmd,
	   "Set whether exceptions in the inferior process will be trapped.\n"
	   "When exceptions are turned off, neither breakpoints nor single-stepping\n"
	   "will work.",
	   &setlist);
  /* Allow `set exc' despite conflict with `set exception-port'.  */
  add_alias_cmd ("exc", "exceptions", class_run, 1, &setlist);
  add_cmd ("exceptions", no_class, show_exceptions_cmd,
	   "Show whether exceptions in the inferior process will be trapped.",
	   &showlist);

  

  add_prefix_cmd ("task", no_class, set_task_cmd,
		  "Command prefix for setting task attributes.",
		  &set_task_cmd_list, "set task ", 0, &setlist);
  add_prefix_cmd ("task", no_class, show_task_cmd,
		  "Command prefix for showing task attributes.",
		  &show_task_cmd_list, "show task ", 0, &showlist);

  add_cmd ("pause", class_run, set_task_pause_cmd,
	   "Set whether the task is suspended while gdb has control.\n"
	   "A value of \"on\" takes effect immediately, otherwise nothing\n"
	   "happens until the next time the program is continued.\n"
	   "When setting this to \"off\", \"set thread default pause on\"\n"
	   "can be used to pause individual threads by default instead.",
	   &set_task_cmd_list);
  add_cmd ("pause", no_class, show_task_pause_cmd,
	   "Show whether the task is suspended while gdb has control.",
	   &show_task_cmd_list);

  add_cmd ("exception-port", no_class, set_task_exc_port_cmd,
	   "Set the task exception port to which we forward exceptions.\n"
	   "The argument should be the value of the send right in the task.",
	   &set_task_cmd_list);
  add_alias_cmd ("excp", "exception-port", no_class, 1, &set_task_cmd_list);
  add_alias_cmd ("exc-port", "exception-port", no_class, 1, &set_task_cmd_list);
}

/* User thread commands.  */

extern struct cmd_list_element *set_thread_cmd_list;
extern struct cmd_list_element *show_thread_cmd_list;

static void
set_thread_pause_cmd (char *args, int from_tty)
{
  struct proc *thread = cur_thread ();
  int old_sc = thread->pause_sc;
  thread->pause_sc = parse_bool_arg (args, "set thread pause");
  if (old_sc == 0 && thread->pause_sc != 0 && thread->inf->pause_sc == 0)
    /* If the task is currently unsuspended, immediately suspend it,
       otherwise wait until the next time it gets control.  */
    inf_suspend (thread->inf);
}

static void
show_thread_pause_cmd (char *args, int from_tty)
{
  struct proc *thread = cur_thread ();
  int sc = thread->pause_sc;
  check_empty (args, "show task pause");
  printf_unfiltered ("Thread %s %s suspended while gdb has control%s.\n",
		     proc_string (thread),
		     sc ? "is" : "isn't",
		     !sc && thread->inf->pause_sc ? "(but the task is)" : "");
}

static void
set_thread_run_cmd (char *args, int from_tty)
{
  struct proc *thread = cur_thread ();
  thread->run_sc = parse_bool_arg (args, "set thread run") ? 0 : 1;
}

static void
show_thread_run_cmd (char *args, int from_tty)
{
  struct proc *thread = cur_thread ();
  check_empty (args, "show thread run");
  printf_unfiltered ("Thread %s allowed to run.",
		     proc_string (thread),
		     thread->run_sc == 0 ? "is" : "isn't");
}

static void
set_thread_exc_port_cmd (char *args, int from_tty)
{
  struct proc *thread = cur_thread ();
  if (!args)
    error ("No argument to \"set thread exception-port\" command.");
  steal_exc_port (thread, parse_and_eval_address (args));
}

static void 
set_thread_cmd (char *args, int from_tty)
{
  printf_unfiltered ("\"set thread\" must be followed by the name of a thread property.\n");
}

static void
show_thread_cmd (char *args, int from_tty)
{
  check_empty (args, "show thread");
  show_thread_run_cmd (0, from_tty);
  show_thread_pause_cmd (0, from_tty);
}

add_thread_commands ()
{
  add_cmd ("pause", class_run, set_thread_pause_cmd,
	   "Set whether the current thread is suspended while gdb has control.\n"
	   "A value of \"on\" takes effect immediately, otherwise nothing\n"
	   "happens until the next time the program is continued.  This\n"
	   "property normally has no effect because the whole task is suspended,\n"
	   "however, that may be disabled with \"set task pause off\".\n"
	   "The default value is \"off\".",
	   &set_thread_cmd_list);
  add_cmd ("pause", no_class, show_thread_pause_cmd,
	   "Show whether the current thread is suspended while gdb has control.",
	   &show_thread_cmd_list);

  add_cmd ("run", class_run, set_thread_run_cmd,
	   "Set whether the current thread is allowed to run.",
	   &set_thread_cmd_list);
  add_cmd ("run", no_class, show_thread_run_cmd,
	   "Show whether the current thread is allowed to run.",
	   &show_thread_cmd_list);

  add_cmd ("exception-port", no_class, set_thread_exc_port_cmd,
	   "Set the exception port to which we forward exceptions for the\n"
	   "current thread, overriding the task exception port.\n"
	   "The argument should be the value of the send right in the task.",
	   &set_thread_cmd_list);
  add_alias_cmd ("excp", "exception-port", no_class, 1, &set_thread_cmd_list);
  add_alias_cmd ("exc-port", "exception-port", no_class, 1, &set_thread_cmd_list);
}

void
_initialize_gnu_nat ()
{
  proc_server = getproc ();

  add_target (&gnu_ops);

  add_task_commands ();
  add_thread_commands ();

#if MAINTENANCE_CMDS
  add_set_cmd ("gnu-debug", class_maintenance,
	       var_boolean, (char *)&gnu_debug_flag,
	       "Set debugging output for the gnu backend.", &maintenancelist);
#endif
}

#ifdef	FLUSH_INFERIOR_CACHE

/* When over-writing code on some machines the I-Cache must be flushed
   explicitly, because it is not kept coherent by the lazy hardware.
   This definitely includes breakpoints, for instance, or else we
   end up looping in mysterious Bpt traps */

void
flush_inferior_icache(pc, amount)
     CORE_ADDR pc;
{
  vm_machine_attribute_val_t flush = MATTR_VAL_ICACHE_FLUSH;
  error_t   ret;
  
  ret = vm_machine_attribute (current_inferior->task->port,
			      pc,
			      amount,
			      MATTR_CACHE,
			      &flush);
  if (ret != KERN_SUCCESS)
    warning ("Error flushing inferior's cache : %s", strerror (ret));
}
#endif	FLUSH_INFERIOR_CACHE
