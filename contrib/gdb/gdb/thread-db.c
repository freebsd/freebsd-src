/* libthread_db assisted debugging support, generic parts.
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.

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

#include "gdb_assert.h"
#include <dlfcn.h>
#include "gdb_proc_service.h"
#include "gdb_thread_db.h"

#include "bfd.h"
#include "gdbthread.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "regcache.h"

#ifndef LIBTHREAD_DB_SO
#define LIBTHREAD_DB_SO "libthread_db.so.1"
#endif

/* If we're running on GNU/Linux, we must explicitly attach to any new
   threads.  */

/* FIXME: There is certainly some room for improvements:
   - Cache LWP ids.
   - Bypass libthread_db when fetching or storing registers for
   threads bound to a LWP.  */

/* This module's target vector.  */
static struct target_ops thread_db_ops;

/* The target vector that we call for things this module can't handle.  */
static struct target_ops *target_beneath;

/* Pointer to the next function on the objfile event chain.  */
static void (*target_new_objfile_chain) (struct objfile *objfile);

/* Non-zero if we're using this module's target vector.  */
static int using_thread_db;

/* Non-zero if we have to keep this module's target vector active
   across re-runs.  */
static int keep_thread_db;

/* Non-zero if we have determined the signals used by the threads
   library.  */
static int thread_signals;
static sigset_t thread_stop_set;
static sigset_t thread_print_set;

/* Structure that identifies the child process for the
   <proc_service.h> interface.  */
static struct ps_prochandle proc_handle;

/* Connection to the libthread_db library.  */
static td_thragent_t *thread_agent;

/* Pointers to the libthread_db functions.  */

static td_err_e (*td_init_p) (void);

static td_err_e (*td_ta_new_p) (struct ps_prochandle *ps, td_thragent_t **ta);
static td_err_e (*td_ta_map_id2thr_p) (const td_thragent_t *ta, thread_t pt,
				       td_thrhandle_t *__th);
static td_err_e (*td_ta_map_lwp2thr_p) (const td_thragent_t *ta, lwpid_t lwpid,
					td_thrhandle_t *th);
static td_err_e (*td_ta_thr_iter_p) (const td_thragent_t *ta,
				     td_thr_iter_f *callback,
				     void *cbdata_p, td_thr_state_e state,
				     int ti_pri, sigset_t *ti_sigmask_p,
				     unsigned int ti_user_flags);
static td_err_e (*td_ta_event_addr_p) (const td_thragent_t *ta,
				       td_event_e event, td_notify_t *ptr);
static td_err_e (*td_ta_set_event_p) (const td_thragent_t *ta,
				      td_thr_events_t *event);
static td_err_e (*td_ta_event_getmsg_p) (const td_thragent_t *ta,
					 td_event_msg_t *msg);

static td_err_e (*td_thr_validate_p) (const td_thrhandle_t *th);
static td_err_e (*td_thr_get_info_p) (const td_thrhandle_t *th,
				      td_thrinfo_t *infop);
static td_err_e (*td_thr_getfpregs_p) (const td_thrhandle_t *th,
				       gdb_prfpregset_t *regset);
static td_err_e (*td_thr_getgregs_p) (const td_thrhandle_t *th,
				      prgregset_t gregs);
static td_err_e (*td_thr_setfpregs_p) (const td_thrhandle_t *th,
				       const gdb_prfpregset_t *fpregs);
static td_err_e (*td_thr_setgregs_p) (const td_thrhandle_t *th,
				      prgregset_t gregs);
static td_err_e (*td_thr_event_enable_p) (const td_thrhandle_t *th, int event);

/* Location of the thread creation event breakpoint.  The code at this
   location in the child process will be called by the pthread library
   whenever a new thread is created.  By setting a special breakpoint
   at this location, GDB can detect when a new thread is created.  We
   obtain this location via the td_ta_event_addr call.  */
static CORE_ADDR td_create_bp_addr;

/* Location of the thread death event breakpoint.  */
static CORE_ADDR td_death_bp_addr;

/* Prototypes for local functions.  */
static void thread_db_find_new_threads (void);


/* Building process ids.  */

#define GET_PID(ptid)		ptid_get_pid (ptid)
#define GET_LWP(ptid)		ptid_get_lwp (ptid)
#define GET_THREAD(ptid)	ptid_get_tid (ptid)

#define is_lwp(ptid)		(GET_LWP (ptid) != 0)
#define is_thread(ptid)		(GET_THREAD (ptid) != 0)

#define BUILD_LWP(lwp, pid)	ptid_build (pid, lwp, 0)
#define BUILD_THREAD(tid, pid)	ptid_build (pid, 0, tid)


struct private_thread_info
{
  /* Cached LWP id.  Must come first, see lin-lwp.c.  */
  lwpid_t lwpid;
};


static char *
thread_db_err_str (td_err_e err)
{
  static char buf[64];

  switch (err)
    {
    case TD_OK:
      return "generic 'call succeeded'";
    case TD_ERR:
      return "generic error";
    case TD_NOTHR:
      return "no thread to satisfy query";
    case TD_NOSV:
      return "no sync handle to satisfy query";
    case TD_NOLWP:
      return "no LWP to satisfy query";
    case TD_BADPH:
      return "invalid process handle";
    case TD_BADTH:
      return "invalid thread handle";
    case TD_BADSH:
      return "invalid synchronization handle";
    case TD_BADTA:
      return "invalid thread agent";
    case TD_BADKEY:
      return "invalid key";
    case TD_NOMSG:
      return "no event message for getmsg";
    case TD_NOFPREGS:
      return "FPU register set not available";
    case TD_NOLIBTHREAD:
      return "application not linked with libthread";
    case TD_NOEVENT:
      return "requested event is not supported";
    case TD_NOCAPAB:
      return "capability not available";
    case TD_DBERR:
      return "debugger service failed";
    case TD_NOAPLIC:
      return "operation not applicable to";
    case TD_NOTSD:
      return "no thread-specific data for this thread";
    case TD_MALLOC:
      return "malloc failed";
    case TD_PARTIALREG:
      return "only part of register set was written/read";
    case TD_NOXREGS:
      return "X register set not available for this thread";
    default:
      snprintf (buf, sizeof (buf), "unknown thread_db error '%d'", err);
      return buf;
    }
}

static char *
thread_db_state_str (td_thr_state_e state)
{
  static char buf[64];

  switch (state)
    {
    case TD_THR_STOPPED:
      return "stopped by debugger";
    case TD_THR_RUN:
      return "runnable";
    case TD_THR_ACTIVE:
      return "active";
    case TD_THR_ZOMBIE:
      return "zombie";
    case TD_THR_SLEEP:
      return "sleeping";
    case TD_THR_STOPPED_ASLEEP:
      return "stopped by debugger AND blocked";
    default:
      snprintf (buf, sizeof (buf), "unknown thread_db state %d", state);
      return buf;
    }
}


/* Convert between user-level thread ids and LWP ids.  */

static ptid_t
thread_from_lwp (ptid_t ptid)
{
  td_thrinfo_t ti;
  td_thrhandle_t th;
  td_err_e err;

  if (GET_LWP (ptid) == 0)
    ptid = BUILD_LWP (GET_PID (ptid), GET_PID (ptid));

  gdb_assert (is_lwp (ptid));

  err = td_ta_map_lwp2thr_p (thread_agent, GET_LWP (ptid), &th);
  if (err != TD_OK)
    error ("Cannot find user-level thread for LWP %d: %s",
	   GET_LWP (ptid), thread_db_err_str (err));

  err = td_thr_get_info_p (&th, &ti);
  if (err != TD_OK)
    error ("Cannot get thread info: %s", thread_db_err_str (err));

  return BUILD_THREAD (ti.ti_tid, GET_PID (ptid));
}

static ptid_t
lwp_from_thread (ptid_t ptid)
{
  td_thrinfo_t ti;
  td_thrhandle_t th;
  td_err_e err;

  if (!is_thread (ptid))
    return ptid;

  err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (ptid), &th);
  if (err != TD_OK)
    error ("Cannot find thread %ld: %s",
	   (long) GET_THREAD (ptid), thread_db_err_str (err));

  err = td_thr_get_info_p (&th, &ti);
  if (err != TD_OK)
    error ("Cannot get thread info: %s", thread_db_err_str (err));

  return BUILD_LWP (ti.ti_lid, GET_PID (ptid));
}


void
thread_db_init (struct target_ops *target)
{
  target_beneath = target;
}

static int
thread_db_load (void)
{
  void *handle;
  td_err_e err;

  handle = dlopen (LIBTHREAD_DB_SO, RTLD_NOW);
  if (handle == NULL)
    return 0;

  /* Initialize pointers to the dynamic library functions we will use.
     Essential functions first.  */

  td_init_p = dlsym (handle, "td_init");
  if (td_init_p == NULL)
    return 0;

  td_ta_new_p = dlsym (handle, "td_ta_new");
  if (td_ta_new_p == NULL)
    return 0;

  td_ta_map_id2thr_p = dlsym (handle, "td_ta_map_id2thr");
  if (td_ta_map_id2thr_p == NULL)
    return 0;

  td_ta_map_lwp2thr_p = dlsym (handle, "td_ta_map_lwp2thr");
  if (td_ta_map_lwp2thr_p == NULL)
    return 0;

  td_ta_thr_iter_p = dlsym (handle, "td_ta_thr_iter");
  if (td_ta_thr_iter_p == NULL)
    return 0;

  td_thr_validate_p = dlsym (handle, "td_thr_validate");
  if (td_thr_validate_p == NULL)
    return 0;

  td_thr_get_info_p = dlsym (handle, "td_thr_get_info");
  if (td_thr_get_info_p == NULL)
    return 0;

  td_thr_getfpregs_p = dlsym (handle, "td_thr_getfpregs");
  if (td_thr_getfpregs_p == NULL)
    return 0;

  td_thr_getgregs_p = dlsym (handle, "td_thr_getgregs");
  if (td_thr_getgregs_p == NULL)
    return 0;

  td_thr_setfpregs_p = dlsym (handle, "td_thr_setfpregs");
  if (td_thr_setfpregs_p == NULL)
    return 0;

  td_thr_setgregs_p = dlsym (handle, "td_thr_setgregs");
  if (td_thr_setgregs_p == NULL)
    return 0;

  /* Initialize the library.  */
  err = td_init_p ();
  if (err != TD_OK)
    {
      warning ("Cannot initialize libthread_db: %s", thread_db_err_str (err));
      return 0;
    }

  /* These are not essential.  */
  td_ta_event_addr_p = dlsym (handle, "td_ta_event_addr");
  td_ta_set_event_p = dlsym (handle, "td_ta_set_event");
  td_ta_event_getmsg_p = dlsym (handle, "td_ta_event_getmsg");
  td_thr_event_enable_p = dlsym (handle, "td_thr_event_enable");

  return 1;
}

static void
enable_thread_event_reporting (void)
{
  td_thr_events_t events;
  td_notify_t notify;
  td_err_e err;

  /* We cannot use the thread event reporting facility if these
     functions aren't available.  */
  if (td_ta_event_addr_p == NULL || td_ta_set_event_p == NULL
      || td_ta_event_getmsg_p == NULL || td_thr_event_enable_p == NULL)
    return;

  /* Set the process wide mask saying which events we're interested in.  */
  td_event_emptyset (&events);
  td_event_addset (&events, TD_CREATE);
#if 0
  /* FIXME: kettenis/2000-04-23: The event reporting facility is
     broken for TD_DEATH events in glibc 2.1.3, so don't enable it for
     now.  */
  td_event_addset (&events, TD_DEATH);
#endif

  err = td_ta_set_event_p (thread_agent, &events);
  if (err != TD_OK)
    {
      warning ("Unable to set global thread event mask: %s",
	       thread_db_err_str (err));
      return;
    }

  /* Delete previous thread event breakpoints, if any.  */
  remove_thread_event_breakpoints ();

  /* Get address for thread creation breakpoint.  */
  err = td_ta_event_addr_p (thread_agent, TD_CREATE, &notify);
  if (err != TD_OK)
    {
      warning ("Unable to get location for thread creation breakpoint: %s",
	       thread_db_err_str (err));
      return;
    }

  /* Set up the breakpoint.  */
  td_create_bp_addr = (CORE_ADDR) notify.u.bptaddr;
  create_thread_event_breakpoint (td_create_bp_addr);

  /* Get address for thread death breakpoint.  */
  err = td_ta_event_addr_p (thread_agent, TD_DEATH, &notify);
  if (err != TD_OK)
    {
      warning ("Unable to get location for thread death breakpoint: %s",
	       thread_db_err_str (err));
      return;
    }

  /* Set up the breakpoint.  */
  td_death_bp_addr = (CORE_ADDR) notify.u.bptaddr;
  create_thread_event_breakpoint (td_death_bp_addr);
}

static void
disable_thread_event_reporting (void)
{
  td_thr_events_t events;

  /* Set the process wide mask saying we aren't interested in any
     events anymore.  */
  td_event_emptyset (&events);
  td_ta_set_event_p (thread_agent, &events);

  /* Delete thread event breakpoints, if any.  */
  remove_thread_event_breakpoints ();
  td_create_bp_addr = 0;
  td_death_bp_addr = 0;
}

static void
check_thread_signals (void)
{
#ifdef GET_THREAD_SIGNALS
  if (!thread_signals)
    {
      sigset_t mask;
      int i;

      GET_THREAD_SIGNALS (&mask);
      sigemptyset (&thread_stop_set);
      sigemptyset (&thread_print_set);

      for (i = 1; i < NSIG; i++)
	{
	  if (sigismember (&mask, i))
	    {
	      if (signal_stop_update (target_signal_from_host (i), 0))
		sigaddset (&thread_stop_set, i);
	      if (signal_print_update (target_signal_from_host (i), 0))
		sigaddset (&thread_print_set, i);
	      thread_signals = 1;
	    }
	}
    }
#endif
}

static void
disable_thread_signals (void)
{
#ifdef GET_THREAD_SIGNALS
  if (thread_signals)
    {
      int i;

      for (i = 1; i < NSIG; i++)
	{
	  if (sigismember (&thread_stop_set, i))
	    signal_stop_update (target_signal_from_host (i), 1);
	  if (sigismember (&thread_print_set, i))
	    signal_print_update (target_signal_from_host (i), 1);
	}

      thread_signals = 0;
    }
#endif
}

static void
thread_db_new_objfile (struct objfile *objfile)
{
  td_err_e err;

  /* Don't attempt to use thread_db on targets which can not run
     (core files).  */
  if (objfile == NULL || !target_has_execution)
    {
      /* All symbols have been discarded.  If the thread_db target is
         active, deactivate it now.  */
      if (using_thread_db)
	{
	  gdb_assert (proc_handle.pid == 0);
	  unpush_target (&thread_db_ops);
	  using_thread_db = 0;
	}

      keep_thread_db = 0;

      goto quit;
    }

  if (using_thread_db)
    /* Nothing to do.  The thread library was already detected and the
       target vector was already activated.  */
    goto quit;

  /* Initialize the structure that identifies the child process.  Note
     that at this point there is no guarantee that we actually have a
     child process.  */
  proc_handle.pid = GET_PID (inferior_ptid);

  /* Now attempt to open a connection to the thread library.  */
  err = td_ta_new_p (&proc_handle, &thread_agent);
  switch (err)
    {
    case TD_NOLIBTHREAD:
      /* No thread library was detected.  */
      break;

    case TD_OK:
      /* The thread library was detected.  Activate the thread_db target.  */
      push_target (&thread_db_ops);
      using_thread_db = 1;

      /* If the thread library was detected in the main symbol file
         itself, we assume that the program was statically linked
         against the thread library and well have to keep this
         module's target vector activated until forever...  Well, at
         least until all symbols have been discarded anyway (see
         above).  */
      if (objfile == symfile_objfile)
	{
	  gdb_assert (proc_handle.pid == 0);
	  keep_thread_db = 1;
	}

      /* We can only poke around if there actually is a child process.
         If there is no child process alive, postpone the steps below
         until one has been created.  */
      if (proc_handle.pid != 0)
	{
	  enable_thread_event_reporting ();
	  thread_db_find_new_threads ();
	}
      break;

    default:
      warning ("Cannot initialize thread debugging library: %s",
	       thread_db_err_str (err));
      break;
    }

 quit:
  if (target_new_objfile_chain)
    target_new_objfile_chain (objfile);
}

static void
attach_thread (ptid_t ptid, const td_thrhandle_t *th_p,
	       const td_thrinfo_t *ti_p, int verbose)
{
  struct thread_info *tp;
  td_err_e err;

  check_thread_signals ();

  if (verbose)
    printf_unfiltered ("[New %s]\n", target_pid_to_str (ptid));

  /* Add the thread to GDB's thread list.  */
  tp = add_thread (ptid);
  tp->private = xmalloc (sizeof (struct private_thread_info));
  tp->private->lwpid = ti_p->ti_lid;

  if (ti_p->ti_state == TD_THR_UNKNOWN || ti_p->ti_state == TD_THR_ZOMBIE)
    return;			/* A zombie thread -- do not attach.  */

  /* Under GNU/Linux, we have to attach to each and every thread.  */
#ifdef ATTACH_LWP
  ATTACH_LWP (BUILD_LWP (ti_p->ti_lid, GET_PID (ptid)), 0);
#endif

  /* Enable thread event reporting for this thread.  */
  err = td_thr_event_enable_p (th_p, 1);
  if (err != TD_OK)
    error ("Cannot enable thread event reporting for %s: %s",
	   target_pid_to_str (ptid), thread_db_err_str (err));
}

static void
thread_db_attach (char *args, int from_tty)
{
  target_beneath->to_attach (args, from_tty);

  /* Destroy thread info; it's no longer valid.  */
  init_thread_list ();

  /* The child process is now the actual multi-threaded
     program.  Snatch its process ID...  */
  proc_handle.pid = GET_PID (inferior_ptid);

  /* ...and perform the remaining initialization steps.  */
  enable_thread_event_reporting ();
  thread_db_find_new_threads();
}

static void
detach_thread (ptid_t ptid, int verbose)
{
  if (verbose)
    printf_unfiltered ("[%s exited]\n", target_pid_to_str (ptid));
}

static void
thread_db_detach (char *args, int from_tty)
{
  disable_thread_event_reporting ();

  /* There's no need to save & restore inferior_ptid here, since the
     inferior is supposed to be survive this function call.  */
  inferior_ptid = lwp_from_thread (inferior_ptid);

  /* Forget about the child's process ID.  We shouldn't need it
     anymore.  */
  proc_handle.pid = 0;

  target_beneath->to_detach (args, from_tty);
}

static void
thread_db_resume (ptid_t ptid, int step, enum target_signal signo)
{
  struct cleanup *old_chain = save_inferior_ptid ();

  if (GET_PID (ptid) == -1)
    inferior_ptid = lwp_from_thread (inferior_ptid);
  else if (is_thread (ptid))
    ptid = lwp_from_thread (ptid);

  target_beneath->to_resume (ptid, step, signo);

  do_cleanups (old_chain);
}

/* Check if PID is currently stopped at the location of a thread event
   breakpoint location.  If it is, read the event message and act upon
   the event.  */

static void
check_event (ptid_t ptid)
{
  td_event_msg_t msg;
  td_thrinfo_t ti;
  td_err_e err;
  CORE_ADDR stop_pc;

  /* Bail out early if we're not at a thread event breakpoint.  */
  stop_pc = read_pc_pid (ptid) - DECR_PC_AFTER_BREAK;
  if (stop_pc != td_create_bp_addr && stop_pc != td_death_bp_addr)
    return;

  err = td_ta_event_getmsg_p (thread_agent, &msg);
  if (err != TD_OK)
    {
      if (err == TD_NOMSG)
	return;

      error ("Cannot get thread event message: %s", thread_db_err_str (err));
    }

  err = td_thr_get_info_p (msg.th_p, &ti);
  if (err != TD_OK)
    error ("Cannot get thread info: %s", thread_db_err_str (err));

  ptid = BUILD_THREAD (ti.ti_tid, GET_PID (ptid));

  switch (msg.event)
    {
    case TD_CREATE:
#if 0
      /* FIXME: kettenis/2000-08-26: Since we use td_ta_event_getmsg,
         there is no guarantee that the breakpoint will match the
         event.  Should we use td_thr_event_getmsg instead?  */

      if (stop_pc != td_create_bp_addr)
	error ("Thread creation event doesn't match breakpoint.");
#endif

      /* We may already know about this thread, for instance when the
         user has issued the `info threads' command before the SIGTRAP
         for hitting the thread creation breakpoint was reported.  */
      if (!in_thread_list (ptid))
	attach_thread (ptid, msg.th_p, &ti, 1);
      return;

    case TD_DEATH:
#if 0
      /* FIXME: See TD_CREATE.  */

      if (stop_pc != td_death_bp_addr)
	error ("Thread death event doesn't match breakpoint.");
#endif

      if (!in_thread_list (ptid))
	error ("Spurious thread death event.");

      detach_thread (ptid, 1);
      return;

    default:
      error ("Spurious thread event.");
    }
}

static ptid_t
thread_db_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  extern ptid_t trap_ptid;

  if (GET_PID (ptid) != -1 && is_thread (ptid))
    ptid = lwp_from_thread (ptid);

  ptid = target_beneath->to_wait (ptid, ourstatus);

  if (proc_handle.pid == 0)
    /* The current child process isn't the actual multi-threaded
       program yet, so don't try to do any special thread-specific
       post-processing and bail out early.  */
    return ptid;

  if (ourstatus->kind == TARGET_WAITKIND_EXITED)
    return pid_to_ptid (-1);

  if (ourstatus->kind == TARGET_WAITKIND_STOPPED
      && ourstatus->value.sig == TARGET_SIGNAL_TRAP)
    /* Check for a thread event.  */
    check_event (ptid);

  if (!ptid_equal (trap_ptid, null_ptid))
    trap_ptid = thread_from_lwp (trap_ptid);

  return thread_from_lwp (ptid);
}

static int
thread_db_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write,
		       struct mem_attrib *attrib,
		       struct target_ops *target)
{
  struct cleanup *old_chain = save_inferior_ptid ();
  int xfer;

  if (is_thread (inferior_ptid))
    {
      /* FIXME: This seems to be necessary to make sure breakpoints
         are removed.  */
      if (!target_thread_alive (inferior_ptid))
	inferior_ptid = pid_to_ptid (GET_PID (inferior_ptid));
      else
	inferior_ptid = lwp_from_thread (inferior_ptid);
    }

  xfer = target_beneath->to_xfer_memory (memaddr, myaddr, len, write, attrib, target);

  do_cleanups (old_chain);
  return xfer;
}

static void
thread_db_fetch_registers (int regno)
{
  td_thrhandle_t th;
  prgregset_t gregset;
  gdb_prfpregset_t fpregset;
  td_err_e err;

  if (!is_thread (inferior_ptid))
    {
      /* Pass the request to the target beneath us.  */
      target_beneath->to_fetch_registers (regno);
      return;
    }

  err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (inferior_ptid), &th);
  if (err != TD_OK)
    error ("Cannot find thread %ld: %s",
	   (long) GET_THREAD (inferior_ptid), thread_db_err_str (err));

  err = td_thr_getgregs_p (&th, gregset);
  if (err != TD_OK)
    error ("Cannot fetch general-purpose registers for thread %ld: %s",
	   (long) GET_THREAD (inferior_ptid), thread_db_err_str (err));

  err = td_thr_getfpregs_p (&th, &fpregset);
  if (err != TD_OK)
    error ("Cannot get floating-point registers for thread %ld: %s",
	   (long) GET_THREAD (inferior_ptid), thread_db_err_str (err));

  /* Note that we must call supply_gregset after calling the thread_db
     routines because the thread_db routines call ps_lgetgregs and
     friends which clobber GDB's register cache.  */
  supply_gregset ((gdb_gregset_t *) gregset);
  supply_fpregset (&fpregset);
}

static void
thread_db_store_registers (int regno)
{
  td_thrhandle_t th;
  prgregset_t gregset;
  gdb_prfpregset_t fpregset;
  td_err_e err;

  if (!is_thread (inferior_ptid))
    {
      /* Pass the request to the target beneath us.  */
      target_beneath->to_store_registers (regno);
      return;
    }

  err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (inferior_ptid), &th);
  if (err != TD_OK)
    error ("Cannot find thread %ld: %s",
	   (long) GET_THREAD (inferior_ptid), thread_db_err_str (err));

  if (regno != -1)
    {
      char raw[MAX_REGISTER_RAW_SIZE];

      read_register_gen (regno, raw);
      thread_db_fetch_registers (-1);
      supply_register (regno, raw);
    }

  fill_gregset ((gdb_gregset_t *) gregset, -1);
  fill_fpregset (&fpregset, -1);

  err = td_thr_setgregs_p (&th, gregset);
  if (err != TD_OK)
    error ("Cannot store general-purpose registers for thread %ld: %s",
	   (long) GET_THREAD (inferior_ptid), thread_db_err_str (err));
  err = td_thr_setfpregs_p (&th, &fpregset);
  if (err != TD_OK)
    error ("Cannot store floating-point registers  for thread %ld: %s",
	   (long) GET_THREAD (inferior_ptid), thread_db_err_str (err));
}

static void
thread_db_kill (void)
{
  /* There's no need to save & restore inferior_ptid here, since the
     inferior isn't supposed to survive this function call.  */
  inferior_ptid = lwp_from_thread (inferior_ptid);
  target_beneath->to_kill ();
}

static void
thread_db_create_inferior (char *exec_file, char *allargs, char **env)
{
  if (!keep_thread_db)
    {
      unpush_target (&thread_db_ops);
      using_thread_db = 0;
    }

  target_beneath->to_create_inferior (exec_file, allargs, env);
}

static void
thread_db_post_startup_inferior (ptid_t ptid)
{
  if (proc_handle.pid == 0)
    {
      /* The child process is now the actual multi-threaded
         program.  Snatch its process ID...  */
      proc_handle.pid = GET_PID (ptid);

      /* ...and perform the remaining initialization steps.  */
      enable_thread_event_reporting ();
      thread_db_find_new_threads ();
    }
}

static void
thread_db_mourn_inferior (void)
{
  remove_thread_event_breakpoints ();

  /* Forget about the child's process ID.  We shouldn't need it
     anymore.  */
  proc_handle.pid = 0;

  target_beneath->to_mourn_inferior ();
}

static int
thread_db_thread_alive (ptid_t ptid)
{
  td_thrhandle_t th;
  td_thrinfo_t ti;
  td_err_e err;

  if (is_thread (ptid))
    {
      err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (ptid), &th);
      if (err != TD_OK)
	return 0;

      err = td_thr_validate_p (&th);
      if (err != TD_OK)
	return 0;

      err = td_thr_get_info_p (&th, &ti);
      if (err != TD_OK)
	return 0;

      if (ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE)
	return 0;		/* A zombie thread.  */

      return 1;
    }

  if (target_beneath->to_thread_alive)
    return target_beneath->to_thread_alive (ptid);

  return 0;
}

static int
find_new_threads_callback (const td_thrhandle_t *th_p, void *data)
{
  td_thrinfo_t ti;
  td_err_e err;
  ptid_t ptid;

  err = td_thr_get_info_p (th_p, &ti);
  if (err != TD_OK)
    error ("Cannot get thread info: %s", thread_db_err_str (err));

  if (ti.ti_state == TD_THR_UNKNOWN || ti.ti_state == TD_THR_ZOMBIE)
    return 0;			/* A zombie -- ignore.  */

  ptid = BUILD_THREAD (ti.ti_tid, GET_PID (inferior_ptid));

  if (!in_thread_list (ptid))
    attach_thread (ptid, th_p, &ti, 1);

  return 0;
}

static void
thread_db_find_new_threads (void)
{
  td_err_e err;

  /* Iterate over all user-space threads to discover new threads.  */
  err = td_ta_thr_iter_p (thread_agent, find_new_threads_callback, NULL,
			  TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
			  TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
  if (err != TD_OK)
    error ("Cannot find new threads: %s", thread_db_err_str (err));
}

static char *
thread_db_pid_to_str (ptid_t ptid)
{
  if (is_thread (ptid))
    {
      static char buf[64];
      td_thrhandle_t th;
      td_thrinfo_t ti;
      td_err_e err;

      err = td_ta_map_id2thr_p (thread_agent, GET_THREAD (ptid), &th);
      if (err != TD_OK)
	error ("Cannot find thread %ld: %s",
	       (long) GET_THREAD (ptid), thread_db_err_str (err));

      err = td_thr_get_info_p (&th, &ti);
      if (err != TD_OK)
	error ("Cannot get thread info for thread %ld: %s",
	       (long) GET_THREAD (ptid), thread_db_err_str (err));

      if (ti.ti_state == TD_THR_ACTIVE && ti.ti_lid != 0)
	{
	  snprintf (buf, sizeof (buf), "Thread %ld (LWP %d)",
		    (long) ti.ti_tid, ti.ti_lid);
	}
      else
	{
	  snprintf (buf, sizeof (buf), "Thread %ld (%s)",
		    (long) ti.ti_tid, thread_db_state_str (ti.ti_state));
	}

      return buf;
    }

  if (target_beneath->to_pid_to_str (ptid))
    return target_beneath->to_pid_to_str (ptid);

  return normal_pid_to_str (ptid);
}

static void
init_thread_db_ops (void)
{
  thread_db_ops.to_shortname = "multi-thread";
  thread_db_ops.to_longname = "multi-threaded child process.";
  thread_db_ops.to_doc = "Threads and pthreads support.";
  thread_db_ops.to_attach = thread_db_attach;
  thread_db_ops.to_detach = thread_db_detach;
  thread_db_ops.to_resume = thread_db_resume;
  thread_db_ops.to_wait = thread_db_wait;
  thread_db_ops.to_fetch_registers = thread_db_fetch_registers;
  thread_db_ops.to_store_registers = thread_db_store_registers;
  thread_db_ops.to_xfer_memory = thread_db_xfer_memory;
  thread_db_ops.to_kill = thread_db_kill;
  thread_db_ops.to_create_inferior = thread_db_create_inferior;
  thread_db_ops.to_post_startup_inferior = thread_db_post_startup_inferior;
  thread_db_ops.to_mourn_inferior = thread_db_mourn_inferior;
  thread_db_ops.to_thread_alive = thread_db_thread_alive;
  thread_db_ops.to_find_new_threads = thread_db_find_new_threads;
  thread_db_ops.to_pid_to_str = thread_db_pid_to_str;
  thread_db_ops.to_stratum = thread_stratum;
  thread_db_ops.to_has_thread_control = tc_schedlock;
  thread_db_ops.to_magic = OPS_MAGIC;
}

void
_initialize_thread_db (void)
{
  /* Only initialize the module if we can load libthread_db.  */
  if (thread_db_load ())
    {
      init_thread_db_ops ();
      add_target (&thread_db_ops);

      /* Add ourselves to objfile event chain.  */
      target_new_objfile_chain = target_new_objfile_hook;
      target_new_objfile_hook = thread_db_new_objfile;
    }
}
