/* Low level interface for debugging Solaris threads for GDB, the GNU debugger.
   Copyright 1996, 1997, 1998 Free Software Foundation, Inc.

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

/* This module implements a sort of half target that sits between the
   machine-independent parts of GDB and the /proc interface (procfs.c) to
   provide access to the Solaris user-mode thread implementation.

   Solaris threads are true user-mode threads, which are invoked via the thr_*
   and pthread_* (native and Posix respectivly) interfaces.  These are mostly
   implemented in user-space, with all thread context kept in various
   structures that live in the user's heap.  These should not be confused with
   lightweight processes (LWPs), which are implemented by the kernel, and
   scheduled without explicit intervention by the process.

   Just to confuse things a little, Solaris threads (both native and Posix) are
   actually implemented using LWPs.  In general, there are going to be more
   threads than LWPs.  There is no fixed correspondence between a thread and an
   LWP.  When a thread wants to run, it gets scheduled onto the first available
   LWP and can therefore migrate from one LWP to another as time goes on.  A
   sleeping thread may not be associated with an LWP at all!

   To make it possible to mess with threads, Sun provides a library called
   libthread_db.so.1 (not to be confused with libthread_db.so.0, which doesn't
   have a published interface).  This interface has an upper part, which it
   provides, and a lower part which I provide.  The upper part consists of the
   td_* routines, which allow me to find all the threads, query their state,
   etc...  The lower part consists of all of the ps_*, which are used by the
   td_* routines to read/write memory, manipulate LWPs, lookup symbols, etc...
   The ps_* routines actually do most of their work by calling functions in
   procfs.c.  */

#include "defs.h"
#include <thread.h>
#include <proc_service.h>
#include <thread_db.h>
#include "gdbthread.h"
#include "target.h"
#include "inferior.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "gdbcmd.h"

extern struct target_ops sol_thread_ops; /* Forward declaration */
extern struct target_ops sol_core_ops; /* Forward declaration */

/* place to store core_ops before we overwrite it */
static struct target_ops orig_core_ops;

struct target_ops sol_thread_ops;
struct target_ops sol_core_ops;

extern int procfs_suppress_run;
extern struct target_ops procfs_ops; /* target vector for procfs.c */
extern struct target_ops core_ops; /* target vector for corelow.c */
extern char *procfs_pid_to_str PARAMS ((int pid));

/* Note that these prototypes differ slightly from those used in procfs.c
   for of two reasons.  One, we can't use gregset_t, as that's got a whole
   different meaning under Solaris (also, see above).  Two, we can't use the
   pointer form here as these are actually arrays of ints (for Sparc's at
   least), and are automatically coerced into pointers to ints when used as
   parameters.  That makes it impossible to avoid a compiler warning when
   passing pr{g fp}regset_t's from a parameter to an argument of one of
   these functions.  */

extern void supply_gregset PARAMS ((const prgregset_t));
extern void fill_gregset PARAMS ((prgregset_t, int));
extern void supply_fpregset PARAMS ((const prfpregset_t *));
extern void fill_fpregset PARAMS ((prfpregset_t *, int));

/* This struct is defined by us, but mainly used for the proc_service interface.
   We don't have much use for it, except as a handy place to get a real pid
   for memory accesses.  */

struct ps_prochandle
{
  pid_t pid;
};

struct string_map
{
  int num;
  char *str;
};

static struct ps_prochandle main_ph;
static td_thragent_t *main_ta;
static int sol_thread_active = 0;

static struct cleanup * save_inferior_pid PARAMS ((void));
static void restore_inferior_pid PARAMS ((int pid));
static char *td_err_string PARAMS ((td_err_e errcode));
static char *td_state_string PARAMS ((td_thr_state_e statecode));
static int  thread_to_lwp PARAMS ((int thread_id, int default_lwp));
static void sol_thread_resume PARAMS ((int pid, int step,
				       enum target_signal signo));
static int lwp_to_thread PARAMS ((int lwp));
static int sol_thread_alive PARAMS ((int pid));
static void sol_core_close PARAMS ((int quitting));

static void init_sol_thread_ops PARAMS ((void));
static void init_sol_core_ops PARAMS ((void));

#define THREAD_FLAG 0x80000000
#define is_thread(ARG) (((ARG) & THREAD_FLAG) != 0)
#define is_lwp(ARG) (((ARG) & THREAD_FLAG) == 0)
#define GET_LWP(LWP_ID) (TIDGET(LWP_ID))
#define GET_THREAD(THREAD_ID) (((THREAD_ID) >> 16) & 0x7fff)
#define BUILD_LWP(LWP_ID, PID) ((LWP_ID) << 16 | (PID))
#define BUILD_THREAD(THREAD_ID, PID) (THREAD_FLAG | BUILD_LWP (THREAD_ID, PID))

/* Pointers to routines from lithread_db resolved by dlopen() */

static void
  (*p_td_log) (const int on_off);
static td_err_e
  (*p_td_ta_new) (const struct ps_prochandle *ph_p, td_thragent_t **ta_pp);
static td_err_e
  (*p_td_ta_delete) (td_thragent_t *ta_p);
static td_err_e
  (*p_td_init) (void);
static td_err_e
  (*p_td_ta_get_ph) (const td_thragent_t *ta_p, struct ps_prochandle **ph_pp);
static td_err_e
  (*p_td_ta_get_nthreads) (const td_thragent_t *ta_p, int *nthread_p);
static td_err_e
  (*p_td_ta_tsd_iter) (const td_thragent_t *ta_p, td_key_iter_f *cb, void *cbdata_p);
static td_err_e
  (*p_td_ta_thr_iter) (const td_thragent_t *ta_p, td_thr_iter_f *cb, void *cbdata_p, td_thr_state_e state,
		       int ti_pri, sigset_t *ti_sigmask_p, unsigned ti_user_flags);
static td_err_e
  (*p_td_thr_validate) (const td_thrhandle_t *th_p);
static td_err_e
  (*p_td_thr_tsd) (const td_thrhandle_t *th_p, const thread_key_t key, void **data_pp);
static td_err_e
  (*p_td_thr_get_info) (const td_thrhandle_t *th_p, td_thrinfo_t *ti_p);
static td_err_e
  (*p_td_thr_getfpregs) (const td_thrhandle_t *th_p, prfpregset_t *fpregset);
static td_err_e
  (*p_td_thr_getxregsize) (const td_thrhandle_t *th_p, int *xregsize);
static td_err_e
  (*p_td_thr_getxregs) (const td_thrhandle_t *th_p, const caddr_t xregset);
static td_err_e
  (*p_td_thr_sigsetmask) (const td_thrhandle_t *th_p, const sigset_t ti_sigmask);
static td_err_e
  (*p_td_thr_setprio) (const td_thrhandle_t *th_p, const int ti_pri);
static td_err_e
  (*p_td_thr_setsigpending) (const td_thrhandle_t *th_p, const uchar_t ti_pending_flag, const sigset_t ti_pending);
static td_err_e
  (*p_td_thr_setfpregs) (const td_thrhandle_t *th_p, const prfpregset_t *fpregset);
static td_err_e
  (*p_td_thr_setxregs) (const td_thrhandle_t *th_p, const caddr_t xregset);
static td_err_e
  (*p_td_ta_map_id2thr) (const td_thragent_t *ta_p, thread_t tid, td_thrhandle_t *th_p);
static td_err_e
  (*p_td_ta_map_lwp2thr) (const td_thragent_t *ta_p, lwpid_t lwpid, td_thrhandle_t *th_p);
static td_err_e
  (*p_td_thr_getgregs) (const td_thrhandle_t *th_p, prgregset_t regset);
static td_err_e
  (*p_td_thr_setgregs) (const td_thrhandle_t *th_p, const prgregset_t regset);

/*

LOCAL FUNCTION

	td_err_string - Convert a thread_db error code to a string

SYNOPSIS

	char * td_err_string (errcode)

DESCRIPTION

	Return the thread_db error string associated with errcode.  If errcode
	is unknown, then return a message.

 */

static char *
td_err_string (errcode)
     td_err_e errcode;
{
  static struct string_map
    td_err_table[] = {
      {TD_OK,		"generic \"call succeeded\""},
      {TD_ERR,		"generic error."},
      {TD_NOTHR,	"no thread can be found to satisfy query"},
      {TD_NOSV,		"no synch. variable can be found to satisfy query"},
      {TD_NOLWP,	"no lwp can be found to satisfy query"},
      {TD_BADPH,	"invalid process handle"},
      {TD_BADTH,	"invalid thread handle"},
      {TD_BADSH,	"invalid synchronization handle"},
      {TD_BADTA,	"invalid thread agent"},
      {TD_BADKEY,	"invalid key"},
      {TD_NOMSG,	"td_thr_event_getmsg() called when there was no message"},
      {TD_NOFPREGS,	"FPU register set not available for given thread"},
      {TD_NOLIBTHREAD,	"application not linked with libthread"},
      {TD_NOEVENT,	"requested event is not supported"},
      {TD_NOCAPAB,	"capability not available"},
      {TD_DBERR,	"Debugger service failed"},
      {TD_NOAPLIC,	"Operation not applicable to"},
      {TD_NOTSD,	"No thread specific data for this thread"},
      {TD_MALLOC,	"Malloc failed"},
      {TD_PARTIALREG,	"Only part of register set was writen/read"},
      {TD_NOXREGS,	"X register set not available for given thread"}
    };
  const int td_err_size = sizeof td_err_table / sizeof (struct string_map);
  int i;
  static char buf[50];

  for (i = 0; i < td_err_size; i++)
    if (td_err_table[i].num == errcode)
      return td_err_table[i].str;
		  
  sprintf (buf, "Unknown thread_db error code: %d", errcode);

  return buf;
}

/*

LOCAL FUNCTION

	td_state_string - Convert a thread_db state code to a string

SYNOPSIS

	char * td_state_string (statecode)

DESCRIPTION

	Return the thread_db state string associated with statecode.  If
	statecode is unknown, then return a message.

 */

static char *
td_state_string (statecode)
     td_thr_state_e statecode;
{
  static struct string_map
    td_thr_state_table[] = {
      {TD_THR_ANY_STATE, "any state"},
      {TD_THR_UNKNOWN,	"unknown"},
      {TD_THR_STOPPED,	"stopped"},
      {TD_THR_RUN,	"run"},
      {TD_THR_ACTIVE,	"active"},
      {TD_THR_ZOMBIE,	"zombie"},
      {TD_THR_SLEEP,	"sleep"},
      {TD_THR_STOPPED_ASLEEP, "stopped asleep"}
    };
  const int td_thr_state_table_size = sizeof td_thr_state_table / sizeof (struct string_map);
  int i;
  static char buf[50];

  for (i = 0; i < td_thr_state_table_size; i++)
    if (td_thr_state_table[i].num == statecode)
      return td_thr_state_table[i].str;
		  
  sprintf (buf, "Unknown thread_db state code: %d", statecode);

  return buf;
}

/*

LOCAL FUNCTION

	thread_to_lwp - Convert a Posix or Solaris thread id to a LWP id.

SYNOPSIS

	int thread_to_lwp (thread_id, default_lwp)

DESCRIPTION

	This function converts a Posix or Solaris thread id to a lightweight
	process id.  If thread_id is non-existent, that's an error.  If it's
	an inactive thread, then we return default_lwp.

NOTES

	This function probably shouldn't call error()...

 */

static int
thread_to_lwp (thread_id, default_lwp)
     int thread_id;
     int default_lwp;
{
  td_thrinfo_t ti;
  td_thrhandle_t th;
  td_err_e val;

  if (is_lwp (thread_id))
    return thread_id;			/* It's already an LWP id */

  /* It's a thread.  Convert to lwp */

  val = p_td_ta_map_id2thr (main_ta, GET_THREAD (thread_id), &th);
  if (val == TD_NOTHR)
    return -1;		/* thread must have terminated */
  else if (val != TD_OK)
    error ("thread_to_lwp: td_ta_map_id2thr %s", td_err_string (val));

  val = p_td_thr_get_info (&th, &ti);
  if (val == TD_NOTHR)
    return -1;		/* thread must have terminated */
  else if (val != TD_OK)
    error ("thread_to_lwp: td_thr_get_info: %s", td_err_string (val));

  if (ti.ti_state != TD_THR_ACTIVE)
    {
      if (default_lwp != -1)
	return default_lwp;
      error ("thread_to_lwp: thread state not active: %s",
	     td_state_string (ti.ti_state));
    }

  return BUILD_LWP (ti.ti_lid, PIDGET (thread_id));
}

/*

LOCAL FUNCTION

	lwp_to_thread - Convert a LWP id to a Posix or Solaris thread id.

SYNOPSIS

	int lwp_to_thread (lwp_id)

DESCRIPTION

	This function converts a lightweight process id to a Posix or Solaris
	thread id.  If thread_id is non-existent, that's an error.

NOTES

	This function probably shouldn't call error()...

 */

static int
lwp_to_thread (lwp)
     int lwp;
{
  td_thrinfo_t ti;
  td_thrhandle_t th;
  td_err_e val;

  if (is_thread (lwp))
    return lwp;			/* It's already a thread id */

  /* It's an lwp.  Convert it to a thread id.  */

  if (!sol_thread_alive (lwp))
    return -1;			/* defunct lwp */

  val = p_td_ta_map_lwp2thr (main_ta, GET_LWP (lwp), &th);
  if (val == TD_NOTHR)
    return -1;		/* thread must have terminated */
  else if (val != TD_OK)
    error ("lwp_to_thread: td_ta_map_lwp2thr: %s.", td_err_string (val));

  val = p_td_thr_validate (&th);
  if (val == TD_NOTHR)
    return lwp;			/* libthread doesn't know about it, just return lwp */
  else if (val != TD_OK)
    error ("lwp_to_thread: td_thr_validate: %s.", td_err_string (val));

  val = p_td_thr_get_info (&th, &ti);
  if (val == TD_NOTHR)
    return -1;		/* thread must have terminated */
  else if (val != TD_OK)
    error ("lwp_to_thread: td_thr_get_info: %s.", td_err_string (val));

  return BUILD_THREAD (ti.ti_tid, PIDGET (lwp));
}

/*

LOCAL FUNCTION

	save_inferior_pid - Save inferior_pid on the cleanup list
	restore_inferior_pid - Restore inferior_pid from the cleanup list

SYNOPSIS

	struct cleanup *save_inferior_pid ()
	void restore_inferior_pid (int pid)

DESCRIPTION

	These two functions act in unison to restore inferior_pid in
	case of an error.

NOTES

	inferior_pid is a global variable that needs to be changed by many of
	these routines before calling functions in procfs.c.  In order to
	guarantee that inferior_pid gets restored (in case of errors), you
	need to call save_inferior_pid before changing it.  At the end of the
	function, you should invoke do_cleanups to restore it.

 */


static struct cleanup *
save_inferior_pid ()
{
  return make_cleanup (restore_inferior_pid, inferior_pid);
}

static void
restore_inferior_pid (pid)
     int pid;
{
  inferior_pid = pid;
}


/* Most target vector functions from here on actually just pass through to
   procfs.c, as they don't need to do anything specific for threads.  */


/* ARGSUSED */
static void
sol_thread_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  procfs_ops.to_open (arg, from_tty);
}

/* Attach to process PID, then initialize for debugging it
   and wait for the trace-trap that results from attaching.  */

static void
sol_thread_attach (args, from_tty)
     char *args;
     int from_tty;
{
  procfs_ops.to_attach (args, from_tty);
  /* Must get symbols from solibs before libthread_db can run! */
  SOLIB_ADD ((char *)0, from_tty, (struct target_ops *)0);
  if (sol_thread_active)
    {
      printf_filtered ("sol-thread active.\n");
      main_ph.pid = inferior_pid; /* Save for xfer_memory */
      push_target (&sol_thread_ops);
      inferior_pid = lwp_to_thread (inferior_pid);
      if (inferior_pid == -1)
	inferior_pid = main_ph.pid;
      else
	add_thread (inferior_pid);
    }
  /* XXX - might want to iterate over all the threads and register them. */
}

/* Take a program previously attached to and detaches it.
   The program resumes execution and will no longer stop
   on signals, etc.  We'd better not have left any breakpoints
   in the program or it'll die when it hits one.  For this
   to work, it may be necessary for the process to have been
   previously attached.  It *might* work if the program was
   started via the normal ptrace (PTRACE_TRACEME).  */

static void
sol_thread_detach (args, from_tty)
     char *args;
     int from_tty;
{
  unpush_target (&sol_thread_ops);
  procfs_ops.to_detach (args, from_tty);
}

/* Resume execution of process PID.  If STEP is nozero, then
   just single step it.  If SIGNAL is nonzero, restart it with that
   signal activated.  We may have to convert pid from a thread-id to an LWP id
   for procfs.  */

static void
sol_thread_resume (pid, step, signo)
     int pid;
     int step;
     enum target_signal signo;
{
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  inferior_pid = thread_to_lwp (inferior_pid, main_ph.pid);
  if (inferior_pid == -1)
    inferior_pid = procfs_first_available ();

  if (pid != -1)
    {
      int save_pid = pid;

      pid = thread_to_lwp (pid, -2);
      if (pid == -2)		/* Inactive thread */
	error ("This version of Solaris can't start inactive threads.");
      if (info_verbose && pid == -1)
	warning ("Specified thread %d seems to have terminated", 
		 GET_THREAD (save_pid));
    }

  procfs_ops.to_resume (pid, step, signo);

  do_cleanups (old_chain);
}

/* Wait for any threads to stop.  We may have to convert PID from a thread id
   to a LWP id, and vice versa on the way out.  */

static int
sol_thread_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  int rtnval;
  int save_pid;
  struct cleanup *old_chain;

  save_pid = inferior_pid;
  old_chain = save_inferior_pid ();

  inferior_pid = thread_to_lwp (inferior_pid, main_ph.pid);
  if (inferior_pid == -1)
    inferior_pid = procfs_first_available ();

  if (pid != -1)
    {
      int save_pid = pid;

      pid = thread_to_lwp (pid, -2);
      if (pid == -2)		/* Inactive thread */
	error ("This version of Solaris can't start inactive threads.");
      if (info_verbose && pid == -1)
	warning ("Specified thread %d seems to have terminated", 
		 GET_THREAD (save_pid));
    }

  rtnval = procfs_ops.to_wait (pid, ourstatus);

  if (ourstatus->kind != TARGET_WAITKIND_EXITED)
    {
      /* Map the LWP of interest back to the appropriate thread ID */
      rtnval = lwp_to_thread (rtnval);
      if (rtnval == -1)
	rtnval = save_pid;

      /* See if we have a new thread */
      if (is_thread (rtnval)
	  && rtnval != save_pid
	  && !in_thread_list (rtnval))
	{
	  printf_filtered ("[New %s]\n", target_pid_to_str (rtnval));
	  add_thread (rtnval);
	}
    }

  /* During process initialization, we may get here without the thread package
     being initialized, since that can only happen after we've found the shared
     libs.  */

  do_cleanups (old_chain);

  return rtnval;
}

static void
sol_thread_fetch_registers (regno)
     int regno;
{
  thread_t thread;
  td_thrhandle_t thandle;
  td_err_e val;
  prgregset_t gregset;
  prfpregset_t fpregset;
#if 0
  int xregsize;
  caddr_t xregset;
#endif

  if (!is_thread (inferior_pid))
    { /* LWP: pass the request on to procfs.c */
      if (target_has_execution)
	procfs_ops.to_fetch_registers (regno);
      else
	orig_core_ops.to_fetch_registers (regno);
      return;
    }

  /* Solaris thread: convert inferior_pid into a td_thrhandle_t */

  thread = GET_THREAD (inferior_pid);

  if (thread == 0)
    error ("sol_thread_fetch_registers:  thread == 0");

  val = p_td_ta_map_id2thr (main_ta, thread, &thandle);
  if (val != TD_OK)
    error ("sol_thread_fetch_registers: td_ta_map_id2thr: %s",
	   td_err_string (val));

  /* Get the integer regs */

  val = p_td_thr_getgregs (&thandle, gregset);
  if (val != TD_OK
      && val != TD_PARTIALREG)
    error ("sol_thread_fetch_registers: td_thr_getgregs %s",
	   td_err_string (val));

  /* For the sparc, TD_PARTIALREG means that only i0->i7, l0->l7, pc and sp
     are saved (by a thread context switch).  */

  /* And, now the fp regs */

  val = p_td_thr_getfpregs (&thandle, &fpregset);
  if (val != TD_OK
      && val != TD_NOFPREGS)
    error ("sol_thread_fetch_registers: td_thr_getfpregs %s",
	   td_err_string (val));

/* Note that we must call supply_{g fp}regset *after* calling the td routines
   because the td routines call ps_lget* which affect the values stored in the
   registers array.  */

  supply_gregset (gregset);
  supply_fpregset (&fpregset);

#if 0
/* thread_db doesn't seem to handle this right */
  val = td_thr_getxregsize (&thandle, &xregsize);
  if (val != TD_OK && val != TD_NOXREGS)
    error ("sol_thread_fetch_registers: td_thr_getxregsize %s",
	   td_err_string (val));

  if (val == TD_OK)
    {
      xregset = alloca (xregsize);
      val = td_thr_getxregs (&thandle, xregset);
      if (val != TD_OK)
	error ("sol_thread_fetch_registers: td_thr_getxregs %s",
	       td_err_string (val));
    }
#endif
}

static void
sol_thread_store_registers (regno)
     int regno;
{
  thread_t thread;
  td_thrhandle_t thandle;
  td_err_e val;
  prgregset_t regset;
  prfpregset_t fpregset;
#if 0
  int xregsize;
  caddr_t xregset;
#endif

  if (!is_thread (inferior_pid))
    { /* LWP: pass the request on to procfs.c */
      procfs_ops.to_store_registers (regno);
      return;
    }

  /* Solaris thread: convert inferior_pid into a td_thrhandle_t */

  thread = GET_THREAD (inferior_pid);

  val = p_td_ta_map_id2thr (main_ta, thread, &thandle);
  if (val != TD_OK)
    error ("sol_thread_store_registers: td_ta_map_id2thr %s",
	   td_err_string (val));

  if (regno != -1)
    {				/* Not writing all the regs */
      /* save new register value */
      char old_value[REGISTER_SIZE];
      memcpy(old_value, & registers[REGISTER_BYTE(regno)], REGISTER_SIZE);

      val = p_td_thr_getgregs (&thandle, regset);
      if (val != TD_OK)
	error ("sol_thread_store_registers: td_thr_getgregs %s",
	       td_err_string (val));
      val = p_td_thr_getfpregs (&thandle, &fpregset);
      if (val != TD_OK)
	error ("sol_thread_store_registers: td_thr_getfpregs %s",
	       td_err_string (val));

      /* restore new register value */
      memcpy(& registers[REGISTER_BYTE(regno)], old_value, REGISTER_SIZE);

#if 0
/* thread_db doesn't seem to handle this right */
      val = td_thr_getxregsize (&thandle, &xregsize);
      if (val != TD_OK && val != TD_NOXREGS)
	error ("sol_thread_store_registers: td_thr_getxregsize %s",
	       td_err_string (val));

      if (val == TD_OK)
	{
	  xregset = alloca (xregsize);
	  val = td_thr_getxregs (&thandle, xregset);
	  if (val != TD_OK)
	    error ("sol_thread_store_registers: td_thr_getxregs %s",
		   td_err_string (val));
	}
#endif
    }

  fill_gregset (regset, regno);
  fill_fpregset (&fpregset, regno);

  val = p_td_thr_setgregs (&thandle, regset);
  if (val != TD_OK)
    error ("sol_thread_store_registers: td_thr_setgregs %s",
	   td_err_string (val));
  val = p_td_thr_setfpregs (&thandle, &fpregset);
  if (val != TD_OK)
    error ("sol_thread_store_registers: td_thr_setfpregs %s",
	   td_err_string (val));

#if 0
/* thread_db doesn't seem to handle this right */
  val = td_thr_getxregsize (&thandle, &xregsize);
  if (val != TD_OK && val != TD_NOXREGS)
    error ("sol_thread_store_registers: td_thr_getxregsize %s",
	   td_err_string (val));

  /* Should probably do something about writing the xregs here, but what are
     they? */
#endif
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
sol_thread_prepare_to_store ()
{
  procfs_ops.to_prepare_to_store ();
}

static int
sol_thread_xfer_memory (memaddr, myaddr, len, dowrite, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int dowrite;
     struct target_ops *target; /* ignored */
{
  int retval;
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  if (is_thread (inferior_pid) ||		/* A thread */
      !target_thread_alive (inferior_pid))	/* An lwp, but not alive */
    inferior_pid = procfs_first_available ();	/* Find any live lwp.  */
  /* Note: don't need to call switch_to_thread; we're just reading memory.  */

  if (target_has_execution)
    retval = procfs_ops.to_xfer_memory (memaddr, myaddr, len, dowrite, target);
  else
    retval = orig_core_ops.to_xfer_memory (memaddr, myaddr, len,
					   dowrite, target);

  do_cleanups (old_chain);

  return retval;
}

/* Print status information about what we're accessing.  */

static void
sol_thread_files_info (ignore)
     struct target_ops *ignore;
{
  procfs_ops.to_files_info (ignore);
}

static void
sol_thread_kill_inferior ()
{
  procfs_ops.to_kill ();
}

static void
sol_thread_notice_signals (pid)
     int pid;
{
  procfs_ops.to_notice_signals (PIDGET (pid));
}

/* Fork an inferior process, and start debugging it with /proc.  */

static void
sol_thread_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  procfs_ops.to_create_inferior (exec_file, allargs, env);

  if (sol_thread_active && inferior_pid != 0)
    {
      main_ph.pid = inferior_pid; /* Save for xfer_memory */

      push_target (&sol_thread_ops);

      inferior_pid = lwp_to_thread (inferior_pid);
      if (inferior_pid == -1)
	inferior_pid = main_ph.pid;

      add_thread (inferior_pid);
    }
}

/* This routine is called whenever a new symbol table is read in, or when all
   symbol tables are removed.  libthread_db can only be initialized when it
   finds the right variables in libthread.so.  Since it's a shared library,
   those variables don't show up until the library gets mapped and the symbol
   table is read in.  */

void
sol_thread_new_objfile (objfile)
     struct objfile *objfile;
{
  td_err_e val;

  if (!objfile)
    {
      sol_thread_active = 0;

      return;
    }

  /* don't do anything if init failed to resolve the libthread_db library */
  if (!procfs_suppress_run)
    return;

  /* Now, initialize the thread debugging library.  This needs to be done after
     the shared libraries are located because it needs information from the
     user's thread library.  */

  val = p_td_init ();
  if (val != TD_OK)
    error ("target_new_objfile: td_init: %s", td_err_string (val));

  val = p_td_ta_new (&main_ph, &main_ta);
  if (val == TD_NOLIBTHREAD)
    return;
  else if (val != TD_OK)
    error ("target_new_objfile: td_ta_new: %s", td_err_string (val));

  sol_thread_active = 1;
}

/* Clean up after the inferior dies.  */

static void
sol_thread_mourn_inferior ()
{
  unpush_target (&sol_thread_ops);
  procfs_ops.to_mourn_inferior ();
}

/* Mark our target-struct as eligible for stray "run" and "attach" commands.  */

static int
sol_thread_can_run ()
{
  return procfs_suppress_run;
}

/* 

LOCAL FUNCTION

	sol_thread_alive     - test thread for "aliveness"

SYNOPSIS

	static bool sol_thread_alive (int pid);

DESCRIPTION

	returns true if thread still active in inferior.

 */

static int
sol_thread_alive (pid)
     int pid;
{
  if (is_thread (pid))		/* non-kernel thread */
    {
      td_err_e val;
      td_thrhandle_t th;

      pid = GET_THREAD (pid);
      if ((val = p_td_ta_map_id2thr (main_ta, pid, &th)) != TD_OK)
	return 0;	/* thread not found */
      if ((val = p_td_thr_validate (&th)) != TD_OK)
	return 0;	/* thread not valid */
      return 1;		/* known thread: return true */
    }
  else			/* kernel thread (LWP): let procfs test it */
    {
      if (target_has_execution)
	return procfs_ops.to_thread_alive (pid);
      else
	return orig_core_ops.to_thread_alive (pid);
    }
}

static void
sol_thread_stop ()
{
  procfs_ops.to_stop ();
}

/* These routines implement the lower half of the thread_db interface.  Ie: the
   ps_* routines.  */

/* Various versions of <proc_service.h> have slightly
   different function prototypes.  In particular, we have

      NEWER		    	OLDER
      struct ps_prochandle *  	const struct ps_prochandle *
      void*			char*
      const void*		char*
      int			size_t

   Which one you have depends on solaris version and what
   patches you've applied.  On the theory that there are
   only two major variants, we have configure check the
   prototype of ps_pdwrite (), and use that info to make
   appropriate typedefs here. */

#ifdef PROC_SERVICE_IS_OLD
typedef const struct ps_prochandle * gdb_ps_prochandle_t;
typedef char * gdb_ps_read_buf_t;
typedef char * gdb_ps_write_buf_t;
typedef int gdb_ps_size_t;
#else
typedef struct ps_prochandle * gdb_ps_prochandle_t;
typedef void * gdb_ps_read_buf_t;
typedef const void * gdb_ps_write_buf_t;
typedef size_t gdb_ps_size_t;
#endif


/* The next four routines are called by thread_db to tell us to stop and stop
   a particular process or lwp.  Since GDB ensures that these are all stopped
   by the time we call anything in thread_db, these routines need to do
   nothing.  */

ps_err_e
ps_pstop (gdb_ps_prochandle_t ph)
{
  return PS_OK;
}

ps_err_e
ps_pcontinue (gdb_ps_prochandle_t ph)
{
  return PS_OK;
}

ps_err_e
ps_lstop (gdb_ps_prochandle_t ph, lwpid_t lwpid)
{
  return PS_OK;
}

ps_err_e
ps_lcontinue (gdb_ps_prochandle_t ph, lwpid_t lwpid)
{
  return PS_OK;
}

ps_err_e
ps_pglobal_lookup (gdb_ps_prochandle_t ph, const char *ld_object_name,
		   const char *ld_symbol_name, paddr_t *ld_symbol_addr)
{
  struct minimal_symbol *ms;

  ms = lookup_minimal_symbol (ld_symbol_name, NULL, NULL);

  if (!ms)
    return PS_NOSYM;

  *ld_symbol_addr = SYMBOL_VALUE_ADDRESS (ms);

  return PS_OK;
}

/* Common routine for reading and writing memory.  */

static ps_err_e
rw_common (int dowrite, const struct ps_prochandle *ph, paddr_t addr,
	   char *buf, int size)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  if (is_thread (inferior_pid) ||		/* A thread */
      !target_thread_alive (inferior_pid))	/* An lwp, but not alive */
    inferior_pid = procfs_first_available ();	/* Find any live lwp.  */
  /* Note: don't need to call switch_to_thread; we're just reading memory.  */

  while (size > 0)
    {
      int cc;

      if (target_has_execution)
	cc = procfs_ops.to_xfer_memory (addr, buf, size, dowrite, &procfs_ops);
      else
	cc = orig_core_ops.to_xfer_memory (addr, buf, size, dowrite, &core_ops);

      if (cc < 0)
	{
	  if (dowrite == 0)
	    print_sys_errmsg ("rw_common (): read", errno);
	  else
	    print_sys_errmsg ("rw_common (): write", errno);

	  do_cleanups (old_chain);

	  return PS_ERR;
	}
      size -= cc;
      buf += cc;
    }

  do_cleanups (old_chain);

  return PS_OK;
}

ps_err_e
ps_pdread (gdb_ps_prochandle_t ph, paddr_t addr,
	   gdb_ps_read_buf_t buf, gdb_ps_size_t size)
{
  return rw_common (0, ph, addr, buf, size);
}

ps_err_e
ps_pdwrite (gdb_ps_prochandle_t ph, paddr_t addr,
	    gdb_ps_write_buf_t buf, gdb_ps_size_t size)
{
  return rw_common (1, ph, addr, (char*) buf, size);
}

ps_err_e
ps_ptread (gdb_ps_prochandle_t ph, paddr_t addr,
	   gdb_ps_read_buf_t buf, gdb_ps_size_t size)
{
  return rw_common (0, ph, addr, buf, size);
}

ps_err_e
ps_ptwrite (gdb_ps_prochandle_t ph, paddr_t addr,
	    gdb_ps_write_buf_t buf, gdb_ps_size_t size)
{
  return rw_common (1, ph, addr, (char*) buf, size);
}

/* Get integer regs */

ps_err_e
ps_lgetregs (gdb_ps_prochandle_t ph, lwpid_t lwpid,
	     prgregset_t gregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  inferior_pid = BUILD_LWP (lwpid, PIDGET (inferior_pid));
  
  if (target_has_execution)
    procfs_ops.to_fetch_registers (-1);
  else
    orig_core_ops.to_fetch_registers (-1);
  fill_gregset (gregset, -1);

  do_cleanups (old_chain);

  return PS_OK;
}

/* Set integer regs */

ps_err_e
ps_lsetregs (gdb_ps_prochandle_t ph, lwpid_t lwpid,
	     const prgregset_t gregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  inferior_pid = BUILD_LWP (lwpid, PIDGET (inferior_pid));
  
  supply_gregset (gregset);
  if (target_has_execution)
    procfs_ops.to_store_registers (-1);
  else
    orig_core_ops.to_store_registers (-1);

  do_cleanups (old_chain);

  return PS_OK;
}

void
ps_plog (const char *fmt, ...)
{
  va_list args;

  va_start (args, fmt);

  vfprintf_filtered (gdb_stderr, fmt, args);
}

/* Get size of extra register set.  Currently a noop.  */

ps_err_e
ps_lgetxregsize (gdb_ps_prochandle_t ph, lwpid_t lwpid, int *xregsize)
{
#if 0
  int lwp_fd;
  int regsize;
  ps_err_e val;

  val = get_lwp_fd (ph, lwpid, &lwp_fd);
  if (val != PS_OK)
    return val;

  if (ioctl (lwp_fd, PIOCGXREGSIZE, &regsize))
    {
      if (errno == EINVAL)
	return PS_NOFREGS;	/* XXX Wrong code, but this is the closest
				   thing in proc_service.h  */

      print_sys_errmsg ("ps_lgetxregsize (): PIOCGXREGSIZE", errno);
      return PS_ERR;
    }
#endif

  return PS_OK;
}

/* Get extra register set.  Currently a noop.  */

ps_err_e
ps_lgetxregs (gdb_ps_prochandle_t ph, lwpid_t lwpid, caddr_t xregset)
{
#if 0
  int lwp_fd;
  ps_err_e val;

  val = get_lwp_fd (ph, lwpid, &lwp_fd);
  if (val != PS_OK)
    return val;

  if (ioctl (lwp_fd, PIOCGXREG, xregset))
    {
      print_sys_errmsg ("ps_lgetxregs (): PIOCGXREG", errno);
      return PS_ERR;
    }
#endif

  return PS_OK;
}

/* Set extra register set.  Currently a noop.  */

ps_err_e
ps_lsetxregs (gdb_ps_prochandle_t ph, lwpid_t lwpid, caddr_t xregset)
{
#if 0
  int lwp_fd;
  ps_err_e val;

  val = get_lwp_fd (ph, lwpid, &lwp_fd);
  if (val != PS_OK)
    return val;

  if (ioctl (lwp_fd, PIOCSXREG, xregset))
    {
      print_sys_errmsg ("ps_lsetxregs (): PIOCSXREG", errno);
      return PS_ERR;
    }
#endif

  return PS_OK;
}

/* Get floating-point regs.  */

ps_err_e
ps_lgetfpregs (gdb_ps_prochandle_t ph, lwpid_t lwpid,
	       prfpregset_t *fpregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  inferior_pid = BUILD_LWP (lwpid, PIDGET (inferior_pid));

  if (target_has_execution)
    procfs_ops.to_fetch_registers (-1);
  else
    orig_core_ops.to_fetch_registers (-1);
  fill_fpregset (fpregset, -1);

  do_cleanups (old_chain);

  return PS_OK;
}

/* Set floating-point regs.  */

ps_err_e
ps_lsetfpregs (gdb_ps_prochandle_t ph, lwpid_t lwpid,
	       const prfpregset_t *fpregset)
{
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();

  inferior_pid = BUILD_LWP (lwpid, PIDGET (inferior_pid));
  
  supply_fpregset (fpregset);
  if (target_has_execution)
    procfs_ops.to_store_registers (-1);
  else
    orig_core_ops.to_store_registers (-1);

  do_cleanups (old_chain);

  return PS_OK;
}

#ifdef TM_I386SOL2_H

/* Get local descriptor table.  */

#include <sys/procfs.h>
#include <sys/reg.h>
#include <sys/sysi86.h>

static int nldt_allocated = 0;
static struct ssd *ldt_bufp = NULL;

ps_err_e
ps_lgetLDT (gdb_ps_prochandle_t ph, lwpid_t lwpid,
	    struct ssd *pldt)
{
  gregset_t gregset;
  int lwp_fd;
  ps_err_e val;
  int nldt;
  int i;

  /* Get procfs file descriptor for the LWP.  */
  lwp_fd = procfs_get_pid_fd (BUILD_LWP (lwpid, PIDGET (inferior_pid)));
  if (lwp_fd < 0)
    return PS_BADLID;

  /* Fetch registers und LDT descriptors.  */
  if (ioctl (lwp_fd, PIOCGREG, &gregset) == -1)
    return PS_ERR;

  if (ioctl (lwp_fd, PIOCNLDT, &nldt) == -1)
    return PS_ERR;

  if (nldt_allocated < nldt)
    {
      ldt_bufp
	= (struct ssd *) xrealloc (ldt_bufp, (nldt + 1) * sizeof (struct ssd));
      nldt_allocated = nldt;
    }

  if (ioctl (lwp_fd, PIOCLDT, ldt_bufp) == -1)
    return PS_ERR;

  /* Search LDT for the LWP via register GS.  */
  for (i = 0; i < nldt; i++)
    {
      if (ldt_bufp[i].sel == (gregset[GS] & 0xffff))
	{
	  *pldt = ldt_bufp[i];
	  return PS_OK;
	}
    }

  /* LDT not found.  */
  return PS_ERR;
}        
#endif /* TM_I386SOL2_H */

/* Convert a pid to printable form. */

char *
solaris_pid_to_str (pid)
     int pid;
{
  static char buf[100];

  /* in case init failed to resolve the libthread_db library */
  if (!procfs_suppress_run)
    return procfs_pid_to_str (pid);

  if (is_thread (pid))
    {
      int lwp;

      lwp = thread_to_lwp (pid, -2);

      if (lwp == -1)
	sprintf (buf, "Thread %d (defunct)", GET_THREAD (pid));
      else if (lwp != -2)
	sprintf (buf, "Thread %d (LWP %d)", GET_THREAD (pid), GET_LWP (lwp));
      else
	sprintf (buf, "Thread %d        ", GET_THREAD (pid));
    }
  else if (GET_LWP (pid) != 0)
    sprintf (buf, "LWP    %d        ", GET_LWP (pid));
  else
    sprintf (buf, "process %d    ", PIDGET (pid));

  return buf;
}


/* Worker bee for find_new_threads
   Callback function that gets called once per USER thread (i.e., not
   kernel) thread. */

static int
sol_find_new_threads_callback(th, ignored)
     const td_thrhandle_t *th;
     void *ignored;
{
  td_err_e retval;
  td_thrinfo_t ti;
  int pid;

  if ((retval = p_td_thr_get_info(th, &ti)) != TD_OK)
    {
      return -1;
    }
  pid = BUILD_THREAD(ti.ti_tid, PIDGET(inferior_pid));
  if (!in_thread_list(pid))
    add_thread(pid);

  return 0;
}

void
sol_find_new_threads()
{
  /* don't do anything if init failed to resolve the libthread_db library */
  if (!procfs_suppress_run)
    return;

  if (inferior_pid == -1)
    {
      printf_filtered("No process.\n");
      return;
    }
  p_td_ta_thr_iter (main_ta, sol_find_new_threads_callback, (void *)0,
		    TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
		    TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
}

static void
sol_core_open (filename, from_tty)
     char *filename;
     int from_tty;
{
  orig_core_ops.to_open (filename, from_tty);
}

static void
sol_core_close (quitting)
     int quitting;
{
  orig_core_ops.to_close (quitting);
}

static void
sol_core_detach (args, from_tty)
     char *args;
     int from_tty;
{
  unpush_target (&core_ops);
  orig_core_ops.to_detach (args, from_tty);
}

static void
sol_core_files_info (t)
     struct target_ops *t;
{
  orig_core_ops.to_files_info (t);
}

#ifdef MAINTENANCE_CMDS
/* Worker bee for info sol-thread command.  This is a callback function that
   gets called once for each Solaris thread (ie. not kernel thread) in the 
   inferior.  Print anything interesting that we can think of.  */

static int 
info_cb (th, s)
     const td_thrhandle_t *th;
     void *s;
{
  td_err_e ret;
  td_thrinfo_t ti;
  struct minimal_symbol *msym;

  if ((ret = p_td_thr_get_info (th, &ti)) == TD_OK)
    {
      printf_filtered ("%s thread #%d, lwp %d, ", 
		       ti.ti_type == TD_THR_SYSTEM ? "system" : "user  ", 
		       ti.ti_tid, ti.ti_lid);
      switch (ti.ti_state) {
	default:
	case TD_THR_UNKNOWN: printf_filtered ("<unknown state>");	break;
	case TD_THR_STOPPED: printf_filtered ("(stopped)");	break;
	case TD_THR_RUN:     printf_filtered ("(run)    ");	break;
	case TD_THR_ACTIVE:  printf_filtered ("(active) ");	break;
	case TD_THR_ZOMBIE:  printf_filtered ("(zombie) ");	break;
	case TD_THR_SLEEP:   printf_filtered ("(asleep) ");	break;
	case TD_THR_STOPPED_ASLEEP: 
	  printf_filtered ("(stopped asleep)");			break;
      }
      /* Print thr_create start function: */
      if (ti.ti_startfunc != 0)
	if (msym = lookup_minimal_symbol_by_pc (ti.ti_startfunc))
	  printf_filtered ("   startfunc: %s\n", SYMBOL_NAME (msym));
	else
	  printf_filtered ("   startfunc: 0x%08x\n", ti.ti_startfunc);

      /* If thread is asleep, print function that went to sleep: */
      if (ti.ti_state == TD_THR_SLEEP)
	if (msym = lookup_minimal_symbol_by_pc (ti.ti_pc))
	  printf_filtered (" - Sleep func: %s\n", SYMBOL_NAME (msym));
	else
	  printf_filtered (" - Sleep func: 0x%08x\n", ti.ti_startfunc);

      /* Wrap up line, if necessary */
      if (ti.ti_state != TD_THR_SLEEP && ti.ti_startfunc == 0)
	printf_filtered ("\n");	/* don't you hate counting newlines? */
    }
  else
    warning ("info sol-thread: failed to get info for thread.");

  return 0;    
}

/* List some state about each Solaris user thread in the inferior.  */

static void
info_solthreads (args, from_tty)
     char *args;
     int from_tty;
{
  p_td_ta_thr_iter (main_ta, info_cb, args, 
		    TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
		    TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);
}
#endif /* MAINTENANCE_CMDS */

static int
ignore (addr, contents)
     CORE_ADDR addr;
     char *contents;
{
  return 0;
}


static void
init_sol_thread_ops ()
{
  sol_thread_ops.to_shortname = "solaris-threads";
  sol_thread_ops.to_longname = "Solaris threads and pthread.";
  sol_thread_ops.to_doc = "Solaris threads and pthread support.";
  sol_thread_ops.to_open = sol_thread_open;
  sol_thread_ops.to_close = 0;
  sol_thread_ops.to_attach = sol_thread_attach;
  sol_thread_ops.to_detach = sol_thread_detach;
  sol_thread_ops.to_resume = sol_thread_resume;
  sol_thread_ops.to_wait = sol_thread_wait;
  sol_thread_ops.to_fetch_registers = sol_thread_fetch_registers;
  sol_thread_ops.to_store_registers = sol_thread_store_registers;
  sol_thread_ops.to_prepare_to_store = sol_thread_prepare_to_store;
  sol_thread_ops.to_xfer_memory = sol_thread_xfer_memory;
  sol_thread_ops.to_files_info = sol_thread_files_info;
  sol_thread_ops.to_insert_breakpoint = memory_insert_breakpoint;
  sol_thread_ops.to_remove_breakpoint = memory_remove_breakpoint;
  sol_thread_ops.to_terminal_init = terminal_init_inferior;
  sol_thread_ops.to_terminal_inferior = terminal_inferior;
  sol_thread_ops.to_terminal_ours_for_output = terminal_ours_for_output;
  sol_thread_ops.to_terminal_ours = terminal_ours;
  sol_thread_ops.to_terminal_info = child_terminal_info;
  sol_thread_ops.to_kill = sol_thread_kill_inferior;
  sol_thread_ops.to_load = 0;
  sol_thread_ops.to_lookup_symbol = 0;
  sol_thread_ops.to_create_inferior = sol_thread_create_inferior;
  sol_thread_ops.to_mourn_inferior = sol_thread_mourn_inferior;
  sol_thread_ops.to_can_run = sol_thread_can_run;
  sol_thread_ops.to_notice_signals = sol_thread_notice_signals;
  sol_thread_ops.to_thread_alive = sol_thread_alive;
  sol_thread_ops.to_stop = sol_thread_stop;
  sol_thread_ops.to_stratum = process_stratum;
  sol_thread_ops.to_has_all_memory = 1;
  sol_thread_ops.to_has_memory = 1;
  sol_thread_ops.to_has_stack = 1;
  sol_thread_ops.to_has_registers = 1;
  sol_thread_ops.to_has_execution = 1;
  sol_thread_ops.to_has_thread_control = tc_none;
  sol_thread_ops.to_sections = 0;
  sol_thread_ops.to_sections_end = 0;
  sol_thread_ops.to_magic = OPS_MAGIC;
}


static void
init_sol_core_ops ()
{
  sol_core_ops.to_shortname  = "solaris-core";
  sol_core_ops.to_longname  = "Solaris core threads and pthread.";
  sol_core_ops.to_doc  = "Solaris threads and pthread support for core files.";
  sol_core_ops.to_open  = sol_core_open;
  sol_core_ops.to_close  = sol_core_close;
  sol_core_ops.to_attach  = sol_thread_attach;
  sol_core_ops.to_detach  = sol_core_detach;
  /* sol_core_ops.to_resume  = 0; */
  /* sol_core_ops.to_wait  = 0;	 */
  sol_core_ops.to_fetch_registers  = sol_thread_fetch_registers;
  /* sol_core_ops.to_store_registers  = 0; */
  /* sol_core_ops.to_prepare_to_store  = 0; */
  sol_core_ops.to_xfer_memory  = sol_thread_xfer_memory;
  sol_core_ops.to_files_info  = sol_core_files_info;
  sol_core_ops.to_insert_breakpoint  = ignore;
  sol_core_ops.to_remove_breakpoint  = ignore;
  /* sol_core_ops.to_terminal_init  = 0; */
  /* sol_core_ops.to_terminal_inferior  = 0; */
  /* sol_core_ops.to_terminal_ours_for_output  = 0; */
  /* sol_core_ops.to_terminal_ours  = 0; */
  /* sol_core_ops.to_terminal_info  = 0; */
  /* sol_core_ops.to_kill  = 0; */
  /* sol_core_ops.to_load  = 0; */
  /* sol_core_ops.to_lookup_symbol  = 0; */
  sol_core_ops.to_create_inferior  = sol_thread_create_inferior;
  sol_core_ops.to_stratum  = core_stratum;
  sol_core_ops.to_has_all_memory  = 0;
  sol_core_ops.to_has_memory  = 1;
  sol_core_ops.to_has_stack  = 1;
  sol_core_ops.to_has_registers  = 1;
  sol_core_ops.to_has_execution  = 0;
  sol_core_ops.to_has_thread_control  = tc_none;
  sol_core_ops.to_sections  = 0;
  sol_core_ops.to_sections_end  = 0;
  sol_core_ops.to_magic  = OPS_MAGIC;
}

/* we suppress the call to add_target of core_ops in corelow because
   if there are two targets in the stratum core_stratum, find_core_target
   won't know which one to return.  see corelow.c for an additonal
   comment on coreops_suppress_target. */
int coreops_suppress_target = 1;

void
_initialize_sol_thread ()
{
  void *dlhandle;

  init_sol_thread_ops ();
  init_sol_core_ops ();

  dlhandle = dlopen ("libthread_db.so.1", RTLD_NOW);
  if (!dlhandle)
    goto die;

#define resolve(X) \
  if (!(p_##X = dlsym (dlhandle, #X))) \
    goto die;

  resolve (td_log);
  resolve (td_ta_new);
  resolve (td_ta_delete);
  resolve (td_init);
  resolve (td_ta_get_ph);
  resolve (td_ta_get_nthreads);
  resolve (td_ta_tsd_iter);
  resolve (td_ta_thr_iter);
  resolve (td_thr_validate);
  resolve (td_thr_tsd);
  resolve (td_thr_get_info);
  resolve (td_thr_getfpregs);
  resolve (td_thr_getxregsize);
  resolve (td_thr_getxregs);
  resolve (td_thr_sigsetmask);
  resolve (td_thr_setprio);
  resolve (td_thr_setsigpending);
  resolve (td_thr_setfpregs);
  resolve (td_thr_setxregs);
  resolve (td_ta_map_id2thr);
  resolve (td_ta_map_lwp2thr);
  resolve (td_thr_getgregs);
  resolve (td_thr_setgregs);

  add_target (&sol_thread_ops);

  procfs_suppress_run = 1;

#ifdef MAINTENANCE_CMDS
  add_cmd ("sol-threads", class_maintenance, info_solthreads, 
	    "Show info on Solaris user threads.\n", &maintenanceinfolist);
#endif /* MAINTENANCE_CMDS */

  memcpy(&orig_core_ops, &core_ops, sizeof (struct target_ops));
  memcpy(&core_ops, &sol_core_ops, sizeof (struct target_ops));
  add_target (&core_ops);

  return;

 die:

  fprintf_unfiltered (gdb_stderr, "[GDB will not be able to debug user-mode threads: %s]\n", dlerror ());

  if (dlhandle)
    dlclose (dlhandle);

  /* allow the user to debug non-threaded core files */
  add_target(&core_ops);

  return;
}
