/* $FreeBSD$ */
/* Low level interface for debugging FreeBSD user threads for GDB, the GNU debugger.
   Copyright 1996, 1999 Free Software Foundation, Inc.

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
   machine-independent parts of GDB and the ptrace interface (infptrace.c) to
   provide access to the FreeBSD user-mode thread implementation.

   FreeBSD threads are true user-mode threads, which are invoked via
   the pthread_* interfaces.  These are mostly implemented in
   user-space, with all thread context kept in various structures that
   live in the user's heap.  For the most part, the kernel has no
   knowlege of these threads.

   Based largely on hpux-thread.c

   */


#include "defs.h"
#include <sys/queue.h>
#include <signal.h>
#include <setjmp.h>
#include "gdbthread.h"
#include "target.h"
#include "inferior.h"
#include <fcntl.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/stat.h>
#include "gdbcore.h"

extern int child_suppress_run;
extern struct target_ops child_ops; /* target vector for inftarg.c */

extern void _initialize_freebsd_uthread PARAMS ((void));

static int main_pid = -1;	/* Real process ID */

/* Set to true while we are part-way through attaching */
static int freebsd_uthread_attaching;

static int freebsd_uthread_active = 0;
static CORE_ADDR P_thread_list;
static CORE_ADDR P_thread_run;

static struct cleanup * save_inferior_pid PARAMS ((void));

static void restore_inferior_pid PARAMS ((int pid));

static void freebsd_uthread_resume PARAMS ((int pid, int step,
					enum target_signal signo));

static void init_freebsd_uthread_ops PARAMS ((void));

static struct target_ops freebsd_uthread_ops;
static struct target_thread_vector freebsd_uthread_vec;

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
  return make_cleanup ((make_cleanup_func) restore_inferior_pid,
		       (void *)(intptr_t) inferior_pid);
}

static void
restore_inferior_pid (pid)
     int pid;
{
  inferior_pid = pid;
}

static int find_active_thread PARAMS ((void));

struct cached_pthread {
  u_int64_t		uniqueid;
  int			state;
  CORE_ADDR		name;
  union {
    ucontext_t	uc;
    jmp_buf	jb;
  }			ctx;
};

static int cached_thread;
static struct cached_pthread cached_pthread;
static CORE_ADDR cached_pthread_addr;

#define THREADID_TID(id)	((id) >> 17)
#define THREADID_PID(id)	((id) & ((1 << 17) - 1))

LIST_HEAD(idmaplist, idmap);

struct idmap {
    LIST_ENTRY(idmap)	link;
    u_int64_t		uniqueid;
    int			tid;
};

#define MAPHASH_SIZE	257
#define TID_MIN		1
#define TID_MAX		16383

static int tid_to_hash[TID_MAX + 1];		/* set to map_hash index */
static struct idmaplist map_hash[MAPHASH_SIZE];
static int next_free_tid = TID_MIN;		/* first available tid */
static int last_free_tid = TID_MIN;		/* first unavailable */

static CORE_ADDR P_thread_next_offset;
static CORE_ADDR P_thread_uniqueid_offset;
static CORE_ADDR P_thread_state_offset;
static CORE_ADDR P_thread_name_offset;
static CORE_ADDR P_thread_ctx_offset;
static CORE_ADDR P_thread_PS_RUNNING_value;
static CORE_ADDR P_thread_PS_DEAD_value;

static int next_offset;
static int uniqueid_offset;
static int state_offset;
static int name_offset;
static int ctx_offset;
static int PS_RUNNING_value;
static int PS_DEAD_value;

#define UNIQUEID_HASH(id)	(id % MAPHASH_SIZE)
#define TID_ADD1(tid)		(((tid) + 1) == TID_MAX + 1 \
				 ? TID_MIN : (tid) + 1)
#define IS_TID_FREE(tid)	(tid_to_hash[tid] == -1)

static int
get_new_tid(h)
     int h;
{
  int tid = next_free_tid;

  tid_to_hash[tid] = h;
  next_free_tid = TID_ADD1(next_free_tid);
  if (next_free_tid == last_free_tid)
    {
      int i;

      for (i = last_free_tid; TID_ADD1(i) != last_free_tid; i = TID_ADD1(i))
	if (IS_TID_FREE(i))
	  break;
      if (TID_ADD1(i) == last_free_tid)
	{
	  error("too many threads");
	  return 0;
	}
      next_free_tid = i;
      for (i = TID_ADD1(i); IS_TID_FREE(i); i = TID_ADD1(i))
	;
      last_free_tid = i;
    }

  return tid;
}

static int
find_pid(uniqueid)
     u_int64_t uniqueid;
{
  int h = UNIQUEID_HASH(uniqueid);
  struct idmap *im;

  LIST_FOREACH(im, &map_hash[h], link)
    if (im->uniqueid == uniqueid)
      return (im->tid << 17) + main_pid;

  im = xmalloc(sizeof(struct idmap));
  im->uniqueid = uniqueid;
  im->tid = get_new_tid(h);
  LIST_INSERT_HEAD(&map_hash[h], im, link);

  return (im->tid << 17) + main_pid;
}

static void
free_pid(pid)
	int pid;
{
  int tid = THREADID_TID(pid);
  int h = tid_to_hash[tid];
  struct idmap *im;

  if (!tid) return;

  LIST_FOREACH(im, &map_hash[h], link)
    if (im->tid == tid)
      break;

  if (!im) return;

  LIST_REMOVE(im, link);
  tid_to_hash[tid] = -1;
  free(im);
}

#define READ_OFFSET(field) read_memory(P_thread_##field##_offset,	\
				       (char *) &field##_offset,	\
				       sizeof(field##_offset))

#define READ_VALUE(name) read_memory(P_thread_##name##_value,	\
				     (char *) &name##_value,	\
				     sizeof(name##_value))

static void
read_thread_offsets ()
{
  READ_OFFSET(next);
  READ_OFFSET(uniqueid);
  READ_OFFSET(state);
  READ_OFFSET(name);
  READ_OFFSET(ctx);

  READ_VALUE(PS_RUNNING);
  READ_VALUE(PS_DEAD);
}

#define READ_FIELD(ptr, T, field, result) \
  read_memory ((ptr) + field##_offset, (char *) &(result), sizeof result)

static u_int64_t
read_pthread_uniqueid (ptr)
     CORE_ADDR ptr;
{
  u_int64_t uniqueid;
  READ_FIELD(ptr, u_int64_t, uniqueid, uniqueid);
  return uniqueid;
}

static CORE_ADDR
read_pthread_next (ptr)
     CORE_ADDR ptr;
{
  CORE_ADDR next;
  READ_FIELD(ptr, CORE_ADDR, next, next);
  return next;
}

static void
read_cached_pthread (ptr, cache)
     CORE_ADDR ptr;
     struct cached_pthread *cache;
{
  READ_FIELD(ptr, u_int64_t,	uniqueid,	cache->uniqueid);
  READ_FIELD(ptr, int,		state,		cache->state);
  READ_FIELD(ptr, CORE_ADDR,	name,		cache->name);
  READ_FIELD(ptr, ucontext_t,	ctx,		cache->ctx);
}

static int
find_active_thread ()
{
  CORE_ADDR ptr;

  if (main_pid == -1)
    return -1;

  read_memory ((CORE_ADDR)P_thread_run,
	       (char *)&ptr,
	       sizeof ptr);

  return find_pid(read_pthread_uniqueid(ptr));
}

static CORE_ADDR find_pthread_addr PARAMS ((int thread));
static struct cached_pthread * find_pthread PARAMS ((int thread));

static CORE_ADDR
find_pthread_addr (thread)
     int thread;
{
  CORE_ADDR ptr;

  if (thread == cached_thread)
    return cached_pthread_addr;

  read_memory ((CORE_ADDR)P_thread_list,
	       (char *)&ptr,
	       sizeof ptr);

  while (ptr != 0)
    {
      if (find_pid(read_pthread_uniqueid(ptr)) == thread)
	{
	  cached_thread = thread;
	  cached_pthread_addr = ptr;
	  read_cached_pthread(ptr, &cached_pthread);
	  return ptr;
	}
      ptr = read_pthread_next(ptr);
    }

  return NULL;
}

static struct cached_pthread *
find_pthread (thread)
     int thread;
{
  CORE_ADDR ptr;

  if (thread == cached_thread)
    return &cached_pthread;

  read_memory ((CORE_ADDR)P_thread_list,
	       (char *)&ptr,
	       sizeof ptr);

  while (ptr != 0)
    {
      if (find_pid(read_pthread_uniqueid(ptr)) == thread)
	{
	  cached_thread = thread;
	  cached_pthread_addr = ptr;
	  read_cached_pthread(ptr, &cached_pthread);
	  return &cached_pthread;
	}
      ptr = read_pthread_next(ptr);
    }

#if 0
  error ("Can't find pthread %d,%d",
	 THREADID_TID(thread), THREADID_PID(thread));
#endif
  return NULL;
}


/* Most target vector functions from here on actually just pass through to
   inftarg.c, as they don't need to do anything specific for threads.  */

/* ARGSUSED */
static void
freebsd_uthread_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  child_ops.to_open (arg, from_tty);
}

/* Attach to process PID, then initialize for debugging it
   and wait for the trace-trap that results from attaching.  */

static void
freebsd_uthread_attach (args, from_tty)
     char *args;
     int from_tty;
{
  child_ops.to_attach (args, from_tty);
  push_target (&freebsd_uthread_ops);
  freebsd_uthread_attaching = 1;
}

/* After an attach, see if the target is threaded */

static void
freebsd_uthread_post_attach (pid)
     int pid;
{
  if (freebsd_uthread_active)
    {
      read_thread_offsets ();

      main_pid = pid;

      bind_target_thread_vector (&freebsd_uthread_vec);

      inferior_pid = find_active_thread ();

      add_thread (inferior_pid);
    }
  else
    {
      unpush_target (&freebsd_uthread_ops);
      push_target (&child_ops);
    }

  freebsd_uthread_attaching = 0;
}

/* Take a program previously attached to and detaches it.
   The program resumes execution and will no longer stop
   on signals, etc.  We'd better not have left any breakpoints
   in the program or it'll die when it hits one.  For this
   to work, it may be necessary for the process to have been
   previously attached.  It *might* work if the program was
   started via the normal ptrace (PTRACE_TRACEME).  */

static void
freebsd_uthread_detach (args, from_tty)
     char *args;
     int from_tty;
{
  child_ops.to_detach (args, from_tty);
}

/* Resume execution of process PID.  If STEP is nozero, then
   just single step it.  If SIGNAL is nonzero, restart it with that
   signal activated.  We may have to convert pid from a thread-id to an LWP id
   for procfs.  */

static void
freebsd_uthread_resume (pid, step, signo)
     int pid;
     int step;
     enum target_signal signo;
{
  struct cleanup *old_chain;

  if (freebsd_uthread_attaching)
    {
      child_ops.to_resume (pid, step, signo);
      return;
    }

  old_chain = save_inferior_pid ();

  pid = inferior_pid = main_pid;

  child_ops.to_resume (pid, step, signo);

  cached_thread = 0;

  do_cleanups (old_chain);
}

/* Wait for any threads to stop.  We may have to convert PID from a thread id
   to a LWP id, and vice versa on the way out.  */

static int
freebsd_uthread_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  int rtnval;
  struct cleanup *old_chain;

  if (freebsd_uthread_attaching)
    {
      return child_ops.to_wait (pid, ourstatus);
    }

  old_chain = save_inferior_pid ();

  inferior_pid = main_pid;

  if (pid != -1)
    pid = main_pid;

  rtnval = child_ops.to_wait (pid, ourstatus);

  if (rtnval >= 0)
    {
      rtnval = find_active_thread ();
      if (!in_thread_list (rtnval))
	add_thread (rtnval);
    }

  do_cleanups (old_chain);

  return rtnval;
}

#ifdef __i386__

static char sigmap[NUM_REGS] =	/* map reg to sigcontext  */
{
  12,				/* eax */
  11,				/* ecx */
  10,				/* edx */
  9,				/* ebx */
  8,				/* esp */
  7,				/* ebp */
  6,				/* esi */
  5,				/* edi */
  15,				/* eip */
  17,				/* eflags */
  16,				/* cs */
  19,				/* ss */
  4,				/* ds */
  3,				/* es */
  2,				/* fs */
  1,				/* gs */
};

static char jmpmap[NUM_REGS] = /* map reg to jmp_buf */
{
  6,				/* eax */
  -1,				/* ecx */
  -1,				/* edx */
  1,				/* ebx */
  2,				/* esp */
  3,				/* ebp */
  4,				/* esi */
  5,				/* edi */
  0,				/* eip */
  -1,				/* eflags */
  -1,				/* cs */
  -1,				/* ss */
  -1,				/* ds */
  -1,				/* es */
  -1,				/* fs */
  -1,				/* gs */
};

#endif

#ifdef __alpha__

static char sigmap[NUM_REGS] =	/* map reg to sigcontext  */
{
  1,  2,  3,  4,  5,  6,  7,  8,  /* v0 - t6 */
  9,  10, 11, 12, 13, 14, 15, 16, /* t7 - fp */
  17, 18, 19, 20, 21, 22, 23, 24, /* a0 - t9 */
  25, 26, 27, 28, 29, 30, 31, 32, /* t10 - zero */
  38, 39, 40, 41, 42, 43, 44, 45, /* f0 - f7 */
  46, 47, 48, 49, 50, 51, 52, 53, /* f8 - f15 */
  54, 55, 56, 57, 58, 59, 60, 61, /* f16 - f23 */
  62, 63, 64, 65, 66, 67, 68, 69, /* f24 - f31 */
  33, -1			  /* pc, vfp */
};
static char jmpmap[NUM_REGS] = {
  4,  5,  6,  7,  8,  9,  10, 11, /* v0 - t6 */
  12, 13, 14, 15, 16, 17, 18, 19, /* t7 - fp */
  20, 21, 22, 23, 24, 25, 26, 27, /* a0 - t9 */
  28, 29, 30, 31, 32, 33, 34, 35, /* t10 - zero */
  37, 38, 39, 40, 41, 42, 43, 44, /* f0 - f7 */
  45, 46, 47, 48, 49, 50, 51, 52, /* f8 - f15 */
  53, 54, 55, 56, 57, 58, 59, 60, /* f16 - f23 */
  61, 62, 63, 64, 65, 66, 67, 68, /* f24 - f31 */
  2,  -1,			  /* pc, vfp */
};

#endif

static void
freebsd_uthread_fetch_registers (regno)
     int regno;
{
  struct cached_pthread *thread;
  struct cleanup *old_chain;
  int active;
  int first_regno, last_regno;
  register_t *regbase;
  char *regmap;

  if (freebsd_uthread_attaching)
    {
      child_ops.to_fetch_registers (regno);
      return;
    }

  thread = find_pthread (inferior_pid);

  old_chain = save_inferior_pid ();

  active = (inferior_pid == find_active_thread());

  inferior_pid = main_pid;

  if (active)
    {
      child_ops.to_fetch_registers (regno);

      do_cleanups (old_chain);

      return;
    }

  if (regno == -1)
    {
      first_regno = 0;
      last_regno = NUM_REGS - 1;
    }
  else
    {
      first_regno = regno;
      last_regno = regno;
    }

  regbase = (register_t*) &thread->ctx.jb[0];
  regmap = jmpmap;

  for (regno = first_regno; regno <= last_regno; regno++)
    {
      if (regmap[regno] == -1)
	child_ops.to_fetch_registers (regno);
      else
	supply_register (regno, (char*) &regbase[regmap[regno]]);
    }

  do_cleanups (old_chain);
}

static void
freebsd_uthread_store_registers (regno)
     int regno;
{
  struct cached_pthread *thread;
  CORE_ADDR ptr;
  struct cleanup *old_chain;
  int first_regno, last_regno;
  u_int32_t *regbase;
  char *regmap;

  if (freebsd_uthread_attaching)
    {
      child_ops.to_store_registers (regno);
      return;
    }

  thread = find_pthread (inferior_pid);

  old_chain = save_inferior_pid ();

  inferior_pid = main_pid;

  if (thread->state == PS_RUNNING_value)
    {
      child_ops.to_store_registers (regno);

      do_cleanups (old_chain);

      return;
    }

  if (regno == -1)
    {
      first_regno = 0;
      last_regno = NUM_REGS - 1;
    }
  else
    {
      first_regno = regno;
      last_regno = regno;
    }

  regbase = (u_int32_t*) &thread->ctx.jb[0];
  regmap = jmpmap;

  ptr = find_pthread_addr (inferior_pid);
  for (regno = first_regno; regno <= last_regno; regno++)
    {
      if (regmap[regno] == -1)
	child_ops.to_store_registers (regno);
      else
	{
	  u_int32_t *reg = &regbase[regmap[regno]];
	  int off;

	  /* Hang onto cached value */
	  memcpy(reg, registers + REGISTER_BYTE (regno),
		 REGISTER_RAW_SIZE (regno));

	  /* And push out to inferior */
	  off = (char *) reg - (char *) thread;
	  write_memory (ptr + off, 
			registers + REGISTER_BYTE (regno),
			REGISTER_RAW_SIZE (regno));
	}
    }

  do_cleanups (old_chain);
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
freebsd_uthread_prepare_to_store ()
{
  struct cleanup *old_chain;

  if (freebsd_uthread_attaching)
    {
      child_ops.to_prepare_to_store ();
      return;
    }

  old_chain = save_inferior_pid ();
  inferior_pid = main_pid;

  child_ops.to_prepare_to_store ();

  do_cleanups (old_chain);
}

static int
freebsd_uthread_xfer_memory (memaddr, myaddr, len, dowrite, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int dowrite;
     struct target_ops *target; /* ignored */
{
  int retval;
  struct cleanup *old_chain;

  if (freebsd_uthread_attaching)
    {
      return child_ops.to_xfer_memory (memaddr, myaddr, len, dowrite, target);
    }

  old_chain = save_inferior_pid ();

  inferior_pid = main_pid;

  retval = child_ops.to_xfer_memory (memaddr, myaddr, len, dowrite, target);

  do_cleanups (old_chain);

  return retval;
}

/* Print status information about what we're accessing.  */

static void
freebsd_uthread_files_info (ignore)
     struct target_ops *ignore;
{
  child_ops.to_files_info (ignore);
}

static void
freebsd_uthread_kill_inferior ()
{
  inferior_pid = main_pid;
  child_ops.to_kill ();
}

static void
freebsd_uthread_notice_signals (pid)
     int pid;
{
  struct cleanup *old_chain;
  old_chain = save_inferior_pid ();
  inferior_pid = main_pid;

  child_ops.to_notice_signals (pid);

  do_cleanups (old_chain);
}

/* Fork an inferior process, and start debugging it with /proc.  */

static void
freebsd_uthread_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  child_ops.to_create_inferior (exec_file, allargs, env);

  if (inferior_pid && freebsd_uthread_active)
    {
      read_thread_offsets ();

      main_pid = inferior_pid;

      push_target (&freebsd_uthread_ops);
      bind_target_thread_vector (&freebsd_uthread_vec);

      inferior_pid = find_active_thread ();

      add_thread (inferior_pid);
    }
}

/* This routine is called to find out if the inferior is using threads.
   We check for the _thread_run and _thread_list globals. */

void
freebsd_uthread_new_objfile (objfile)
     struct objfile *objfile;
{
  struct minimal_symbol *ms;

  if (!objfile)
    {
      freebsd_uthread_active = 0;
      return;
    }

  ms = lookup_minimal_symbol ("_thread_run", NULL, objfile);

  if (!ms)
    return;

  P_thread_run = SYMBOL_VALUE_ADDRESS (ms);

  ms = lookup_minimal_symbol ("_thread_list", NULL, objfile);

  if (!ms)
    return;

  P_thread_list = SYMBOL_VALUE_ADDRESS (ms);

#define OFFSET_SYM(field)	"_thread_" #field "_offset"
#define LOOKUP_OFFSET(field)						\
  do {									\
      ms = lookup_minimal_symbol (OFFSET_SYM(field), NULL, objfile);	\
      if (!ms)								\
	return;								\
      P_thread_##field##_offset = SYMBOL_VALUE_ADDRESS (ms);		\
  } while (0);

#define VALUE_SYM(name)		"_thread_" #name "_value"
#define LOOKUP_VALUE(name)						\
  do {									\
       ms = lookup_minimal_symbol (VALUE_SYM(name), NULL, objfile);	\
      if (!ms)								\
	return;								\
      P_thread_##name##_value = SYMBOL_VALUE_ADDRESS (ms);		\
  } while (0);

  LOOKUP_OFFSET(next);
  LOOKUP_OFFSET(uniqueid);
  LOOKUP_OFFSET(state);
  LOOKUP_OFFSET(name);
  LOOKUP_OFFSET(ctx);

  LOOKUP_VALUE(PS_RUNNING);
  LOOKUP_VALUE(PS_DEAD);

  freebsd_uthread_active = 1;
}

int
freebsd_uthread_has_exited (pid, wait_status, exit_status)
  int  pid;
  int  wait_status;
  int *  exit_status;
{
  int t = child_ops.to_has_exited (pid, wait_status, exit_status);
  if (t)
    main_pid = -1;
  return t;
}

/* Clean up after the inferior dies.  */

static void
freebsd_uthread_mourn_inferior ()
{
  inferior_pid = main_pid;	/* don't bother to restore inferior_pid */
  child_ops.to_mourn_inferior ();
  unpush_target (&freebsd_uthread_ops);
}

/* Mark our target-struct as eligible for stray "run" and "attach" commands.  */

static int
freebsd_uthread_can_run ()
{
  return child_suppress_run;
}

static int
freebsd_uthread_thread_alive (pid)
     int pid;
{
  struct cleanup *old_chain;
  struct cached_pthread *thread;
  int ret = 0;

  if (freebsd_uthread_attaching)
    return 1;

  /*
   * We can get called from child_ops.to_wait() which passes the underlying
   * pid (without a thread number).
   */
  if (THREADID_TID(pid) == 0)
    return 1;

  old_chain = save_inferior_pid ();
  inferior_pid = main_pid;

  if (find_pthread_addr (pid) != 0)
    {
      thread = find_pthread (pid);
      ret = (thread->state != PS_DEAD_value);
    }

  do_cleanups (old_chain);

  if (!ret)
    free_pid(pid);

  return ret;
}

static void
freebsd_uthread_stop ()
{
  struct cleanup *old_chain;
  old_chain = save_inferior_pid ();
  inferior_pid = main_pid;

  child_ops.to_stop ();

  do_cleanups (old_chain);
}

static int
freebsd_uthread_find_new_threads ()
{
  CORE_ADDR ptr;
  int state;
  u_int64_t uniqueid;
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();
  inferior_pid = main_pid;

  read_memory ((CORE_ADDR)P_thread_list,
	       (char *)&ptr,
	       sizeof ptr);

  while (ptr != 0)
    {
      READ_FIELD(ptr, int, state, state);
      READ_FIELD(ptr, u_int64_t, uniqueid, uniqueid);
      if (state != PS_DEAD_value &&
	  !in_thread_list (find_pid(uniqueid)))
	add_thread (find_pid(uniqueid));
      ptr = read_pthread_next(ptr);
    }

  do_cleanups (old_chain);

  return 0;
}

/* MUST MATCH enum pthread_state */
static const char *statenames[] = {
  "RUNNING",
  "SIGTHREAD",
  "MUTEX_WAIT",
  "COND_WAIT",
  "FDLR_WAIT",
  "FDLW_WAIT",
  "FDR_WAIT",
  "FDW_WAIT",
  "POLL_WAIT",
  "FILE_WAIT",
  "SELECT_WAIT",
  "SLEEP_WAIT",
  "WAIT_WAIT",
  "SIGSUSPEND",
  "SIGWAIT",
  "SPINBLOCK",
  "JOIN",
  "SUSPENDED",
  "DEAD",
  "DEADLOCK",
};

static int
freebsd_uthread_get_thread_info (ref, selection, info)
     gdb_threadref *ref;
     int selection;
     struct gdb_ext_thread_info *info;
{
  int pid = *ref;
  struct cached_pthread *thread = find_pthread (pid);
  struct cleanup *old_chain;

  old_chain = save_inferior_pid ();
  inferior_pid = main_pid;

  memset(&info->threadid, 0, OPAQUETHREADBYTES);

  memcpy(&info->threadid, ref, sizeof *ref);
  info->active = thread->state == PS_RUNNING_value;
  strcpy(info->display, statenames[thread->state]);
  if (thread->name)
    read_memory ((CORE_ADDR) thread->name, info->shortname, 32);
  else
    strcpy(info->shortname, "");

  do_cleanups (old_chain);
  return (0);
}

char *
freebsd_uthread_pid_to_str (pid)
     int pid;
{
  static char buf[30];

  if (STREQ (current_target.to_shortname, "freebsd-uthreads"))
    sprintf (buf, "process %d, thread %d\0",
	     THREADID_PID(pid), THREADID_TID(pid));
  else
    sprintf (buf, "process %d\0", pid);

  return buf;
}


static void
init_freebsd_uthread_ops ()
{
  freebsd_uthread_ops.to_shortname = "freebsd-uthreads";
  freebsd_uthread_ops.to_longname = "FreeBSD uthreads";
  freebsd_uthread_ops.to_doc = "FreeBSD user threads support.";
  freebsd_uthread_ops.to_open = freebsd_uthread_open;
  freebsd_uthread_ops.to_attach = freebsd_uthread_attach;
  freebsd_uthread_ops.to_post_attach = freebsd_uthread_post_attach;
  freebsd_uthread_ops.to_detach = freebsd_uthread_detach;
  freebsd_uthread_ops.to_resume = freebsd_uthread_resume;
  freebsd_uthread_ops.to_wait = freebsd_uthread_wait;
  freebsd_uthread_ops.to_fetch_registers = freebsd_uthread_fetch_registers;
  freebsd_uthread_ops.to_store_registers = freebsd_uthread_store_registers;
  freebsd_uthread_ops.to_prepare_to_store = freebsd_uthread_prepare_to_store;
  freebsd_uthread_ops.to_xfer_memory = freebsd_uthread_xfer_memory;
  freebsd_uthread_ops.to_files_info = freebsd_uthread_files_info;
  freebsd_uthread_ops.to_insert_breakpoint = memory_insert_breakpoint;
  freebsd_uthread_ops.to_remove_breakpoint = memory_remove_breakpoint;
  freebsd_uthread_ops.to_terminal_init = terminal_init_inferior;
  freebsd_uthread_ops.to_terminal_inferior = terminal_inferior;
  freebsd_uthread_ops.to_terminal_ours_for_output = terminal_ours_for_output;
  freebsd_uthread_ops.to_terminal_ours = terminal_ours;
  freebsd_uthread_ops.to_terminal_info = child_terminal_info;
  freebsd_uthread_ops.to_kill = freebsd_uthread_kill_inferior;
  freebsd_uthread_ops.to_create_inferior = freebsd_uthread_create_inferior;
  freebsd_uthread_ops.to_has_exited = freebsd_uthread_has_exited;
  freebsd_uthread_ops.to_mourn_inferior = freebsd_uthread_mourn_inferior;
  freebsd_uthread_ops.to_can_run = freebsd_uthread_can_run;
  freebsd_uthread_ops.to_notice_signals = freebsd_uthread_notice_signals;
  freebsd_uthread_ops.to_thread_alive = freebsd_uthread_thread_alive;
  freebsd_uthread_ops.to_stop = freebsd_uthread_stop;
  freebsd_uthread_ops.to_stratum = process_stratum;
  freebsd_uthread_ops.to_has_all_memory = 1;
  freebsd_uthread_ops.to_has_memory = 1;
  freebsd_uthread_ops.to_has_stack = 1;
  freebsd_uthread_ops.to_has_registers = 1;
  freebsd_uthread_ops.to_has_execution = 1;
  freebsd_uthread_ops.to_has_thread_control = 0;
  freebsd_uthread_ops.to_magic = OPS_MAGIC;

  freebsd_uthread_vec.find_new_threads = freebsd_uthread_find_new_threads;
  freebsd_uthread_vec.get_thread_info = freebsd_uthread_get_thread_info;
}

void
_initialize_freebsd_uthread ()
{
  init_freebsd_uthread_ops ();
  add_target (&freebsd_uthread_ops);

  child_suppress_run = 1;
}
