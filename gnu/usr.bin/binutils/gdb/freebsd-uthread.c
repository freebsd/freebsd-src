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

/* Set to true while we are part-way through attaching */
static int freebsd_uthread_attaching;

static int freebsd_uthread_active = 0;
static CORE_ADDR P_thread_list;
static CORE_ADDR P_thread_run;

/* Pointer to the next function on the objfile event chain.  */
static void (*target_new_objfile_chain) (struct objfile *objfile);

static void freebsd_uthread_resume PARAMS ((ptid_t pid, int step,
					enum target_signal signo));

static void init_freebsd_uthread_ops PARAMS ((void));

static struct target_ops freebsd_uthread_ops;

static ptid_t find_active_ptid PARAMS ((void));

struct cached_pthread {
  u_int64_t		uniqueid;
  int			state;
  CORE_ADDR		name;
  union {
    ucontext_t	uc;
    jmp_buf	jb;
  }			ctx;
};

static ptid_t cached_ptid;
static struct cached_pthread cached_pthread;
static CORE_ADDR cached_pthread_addr;

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
get_new_tid(int h)
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

static ptid_t
find_ptid(u_int64_t uniqueid)
{
  int h = UNIQUEID_HASH(uniqueid);
  struct idmap *im;

  LIST_FOREACH(im, &map_hash[h], link)
    if (im->uniqueid == uniqueid)
      return MERGEPID(PIDGET(inferior_ptid), im->tid);

  im = xmalloc(sizeof(struct idmap));
  im->uniqueid = uniqueid;
  im->tid = get_new_tid(h);
  LIST_INSERT_HEAD(&map_hash[h], im, link);

  return MERGEPID(PIDGET(inferior_ptid), im->tid);
}

static void
free_ptid(ptid_t ptid)
{
  int tid = TIDGET(ptid);
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
read_thread_offsets (void)
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
read_pthread_uniqueid (CORE_ADDR ptr)
{
  u_int64_t uniqueid;
  READ_FIELD(ptr, u_int64_t, uniqueid, uniqueid);
  return uniqueid;
}

static CORE_ADDR
read_pthread_next (CORE_ADDR ptr)
{
  CORE_ADDR next;
  READ_FIELD(ptr, CORE_ADDR, next, next);
  return next;
}

static void
read_cached_pthread (CORE_ADDR ptr, struct cached_pthread *cache)
{
  READ_FIELD(ptr, u_int64_t,	uniqueid,	cache->uniqueid);
  READ_FIELD(ptr, int,		state,		cache->state);
  READ_FIELD(ptr, CORE_ADDR,	name,		cache->name);
  READ_FIELD(ptr, ucontext_t,	ctx,		cache->ctx);
}

static ptid_t
find_active_ptid (void)
{
  CORE_ADDR ptr;

  read_memory ((CORE_ADDR)P_thread_run,
	       (char *)&ptr,
	       sizeof ptr);

  return find_ptid(read_pthread_uniqueid(ptr));
}

static CORE_ADDR find_pthread_addr PARAMS ((ptid_t ptid));
static struct cached_pthread * find_pthread PARAMS ((ptid_t ptid));

static CORE_ADDR
find_pthread_addr (ptid_t ptid)
{
  CORE_ADDR ptr;

  if (ptid_equal(ptid, cached_ptid))
    return cached_pthread_addr;

  read_memory ((CORE_ADDR)P_thread_list,
	       (char *)&ptr,
	       sizeof ptr);

  while (ptr != 0)
    {
      if (ptid_equal(find_ptid(read_pthread_uniqueid(ptr)), ptid))
	{
	  cached_ptid = ptid;
	  cached_pthread_addr = ptr;
	  read_cached_pthread(ptr, &cached_pthread);
	  return ptr;
	}
      ptr = read_pthread_next(ptr);
    }

  return NULL;
}

static struct cached_pthread *
find_pthread (ptid_t ptid)
{
  CORE_ADDR ptr;

  if (ptid_equal(ptid, cached_ptid))
    return &cached_pthread;

  read_memory ((CORE_ADDR)P_thread_list,
	       (char *)&ptr,
	       sizeof ptr);

  while (ptr != 0)
    {
      if (ptid_equal(find_ptid(read_pthread_uniqueid(ptr)), ptid))
	{
	  cached_ptid = ptid;
	  cached_pthread_addr = ptr;
	  read_cached_pthread(ptr, &cached_pthread);
	  return &cached_pthread;
	}
      ptr = read_pthread_next(ptr);
    }

#if 0
  error ("Can't find pthread %d,%d", PIDGET(ptid), TIDGET(ptid));
#endif
  return NULL;
}


/* Most target vector functions from here on actually just pass through to
   inftarg.c, as they don't need to do anything specific for threads.  */

/* ARGSUSED */
static void
freebsd_uthread_open (char *arg, int from_tty)
{
  child_ops.to_open (arg, from_tty);
}

/* Attach to process PID, then initialize for debugging it
   and wait for the trace-trap that results from attaching.  */

static void
freebsd_uthread_attach (char *args, int from_tty)
{
  child_ops.to_attach (args, from_tty);
  push_target (&freebsd_uthread_ops);
  freebsd_uthread_attaching = 1;
}

/* After an attach, see if the target is threaded */

static void
freebsd_uthread_post_attach (int pid)
{
  if (freebsd_uthread_active)
    {
      read_thread_offsets ();
      inferior_ptid = find_active_ptid ();
      add_thread (inferior_ptid);
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
freebsd_uthread_detach (char *args, int from_tty)
{
  child_ops.to_detach (args, from_tty);
}

/* Resume execution of process PID.  If STEP is nozero, then
   just single step it.  If SIGNAL is nonzero, restart it with that
   signal activated.  We may have to convert pid from a thread-id to an LWP id
   for procfs.  */

static void
freebsd_uthread_resume (ptid_t ptid, int step, enum target_signal signo)
{
  if (freebsd_uthread_attaching)
    {
      child_ops.to_resume (ptid, step, signo);
      return;
    }

  child_ops.to_resume (ptid, step, signo);
  cached_ptid = MERGEPID(0, 0);
}

/* Wait for any threads to stop.  We may have to convert PID from a thread id
   to a LWP id, and vice versa on the way out.  */

static ptid_t
freebsd_uthread_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  ptid_t rtnval;

  if (freebsd_uthread_attaching)
    {
      return child_ops.to_wait (ptid, ourstatus);
    }

  rtnval = child_ops.to_wait (ptid, ourstatus);

  if (PIDGET(rtnval) >= 0)
    {
      rtnval = find_active_ptid ();
      if (!in_thread_list (rtnval))
	add_thread (rtnval);
    }

  return rtnval;
}

/* XXX: this needs to be selected by target, not [build] host */
#ifdef __i386__

static char jmpmap[MAX_NUM_REGS] = /* map reg to jmp_buf */
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
  -1, -1, -1, -1, -1, -1, -1,	/* st0-st7 */
  -1, -1, -1, -1, -1, -1, -1,	/* fctrl-fop */
  -1, -1, -1, -1, -1, -1, -1,	/* xmm0-xmm7 */
  -1,				/* mxcsr */
};

#endif

#ifdef __alpha__

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

#ifdef __sparc64__

static char jmpmap[125] = {
  -1
};

#endif

static void
freebsd_uthread_fetch_registers (int regno)
{
  struct cached_pthread *thread;
  int active;
  int first_regno, last_regno;
  register_t *regbase;
  char *regmap;

  if (freebsd_uthread_attaching || TIDGET(inferior_ptid) == 0)
    {
      child_ops.to_fetch_registers (regno);
      return;
    }

  thread = find_pthread (inferior_ptid);
  active = (ptid_equal(inferior_ptid, find_active_ptid()));

  if (active)
    {
      child_ops.to_fetch_registers (regno);
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
	if (thread)
	  supply_register (regno, (char*) &regbase[regmap[regno]]);
	else
	  supply_register (regno, NULL);
    }
}

static void
freebsd_uthread_store_registers (int regno)
{
  struct cached_pthread *thread;
  CORE_ADDR ptr;
  int first_regno, last_regno;
  u_int32_t *regbase;
  char *regmap;

  if (freebsd_uthread_attaching)
    {
      child_ops.to_store_registers (regno);
      return;
    }

  thread = find_pthread (inferior_ptid);

  if (thread->state == PS_RUNNING_value)
    {
      child_ops.to_store_registers (regno);
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

  ptr = find_pthread_addr (inferior_ptid);
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
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
freebsd_uthread_prepare_to_store (void)
{
  child_ops.to_prepare_to_store ();
}

static int
freebsd_uthread_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len,
			     int dowrite, struct mem_attrib *attrib,
			     struct target_ops *target)
{
  return child_ops.to_xfer_memory (memaddr, myaddr, len, dowrite,
				   attrib, target);
}

/* Print status information about what we're accessing.  */

static void
freebsd_uthread_files_info (struct target_ops *ignore)
{
  child_ops.to_files_info (ignore);
}

static void
freebsd_uthread_kill_inferior (void)
{
  child_ops.to_kill ();
}

static void
freebsd_uthread_notice_signals (ptid_t ptid)
{
  child_ops.to_notice_signals (ptid);
}

/* Fork an inferior process, and start debugging it with /proc.  */

static void
freebsd_uthread_create_inferior (char *exec_file, char *allargs, char **env)
{
  child_ops.to_create_inferior (exec_file, allargs, env);

  if (PIDGET(inferior_ptid) && freebsd_uthread_active)
    {
      read_thread_offsets ();
      push_target (&freebsd_uthread_ops);
      inferior_ptid = find_active_ptid ();
      add_thread (inferior_ptid);
    }
}

/* This routine is called to find out if the inferior is using threads.
   We check for the _thread_run and _thread_list globals. */

void
freebsd_uthread_new_objfile (struct objfile *objfile)
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

/* Clean up after the inferior dies.  */

static void
freebsd_uthread_mourn_inferior ()
{
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
freebsd_uthread_thread_alive (ptid_t ptid)
{
  struct cached_pthread *thread;
  int ret = 0;

  if (freebsd_uthread_attaching)
    return 1;

  /*
   * We can get called from child_ops.to_wait() which passes the underlying
   * pid (without a thread number).
   */
  if (TIDGET(ptid) == 0)
    return 1;

  if (find_pthread_addr (ptid) != 0)
    {
      thread = find_pthread (ptid);
      ret = (thread->state != PS_DEAD_value);
    }

  if (!ret)
    free_ptid(ptid);

  return ret;
}

static void
freebsd_uthread_stop (void)
{
  child_ops.to_stop ();
}

static void
freebsd_uthread_find_new_threads (void)
{
  CORE_ADDR ptr;
  int state;
  u_int64_t uniqueid;

  read_memory ((CORE_ADDR)P_thread_list,
	       (char *)&ptr,
	       sizeof ptr);

  while (ptr != 0)
    {
      READ_FIELD(ptr, int, state, state);
      READ_FIELD(ptr, u_int64_t, uniqueid, uniqueid);
      if (state != PS_DEAD_value &&
	  !in_thread_list (find_ptid(uniqueid)))
	add_thread (find_ptid(uniqueid));
      ptr = read_pthread_next(ptr);
    }
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

#if 0

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

#endif

char *
freebsd_uthread_pid_to_str (ptid_t ptid)
{
  static char buf[30];

  if (STREQ (current_target.to_shortname, "freebsd-uthreads"))
    sprintf (buf, "Process %d, Thread %ld",
	     PIDGET(ptid), TIDGET(ptid));
  else
    sprintf (buf, "Process %d", PIDGET(ptid));

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
  freebsd_uthread_ops.to_find_new_threads = freebsd_uthread_find_new_threads;
  freebsd_uthread_ops.to_pid_to_str = freebsd_uthread_pid_to_str;
#if 0
  freebsd_uthread_vec.get_thread_info = freebsd_uthread_get_thread_info;
#endif
}

void
_initialize_freebsd_uthread ()
{
  init_freebsd_uthread_ops ();
  add_target (&freebsd_uthread_ops);

  target_new_objfile_chain = target_new_objfile_hook;
  target_new_objfile_hook = freebsd_uthread_new_objfile;

  child_suppress_run = 1;
}
