/* Common things used by the various *gnu-nat.c files

   Copyright (C) 1995 Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.ai.mit.edu>

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef __GNU_NAT_H__
#define __GNU_NAT_H__

#include <unistd.h>
#include <mach.h>

struct inf;

extern struct inf *current_inferior;

/* Converts a GDB pid to a struct proc.  */
struct proc *inf_tid_to_thread (struct inf *inf, int tid);

/* A proc is either a thread, or the task (there can only be one task proc
   because it always has the same TID, PROC_TID_TASK).  */
struct proc
{
  thread_t port;		/* The task or thread port.  */
  int tid;			/* The GDB pid (actually a thread id).  */
  int num;			/* An id number for threads, to print.  */

  mach_port_t saved_exc_port;	/* The task/thread's real exception port.  */
  mach_port_t exc_port;		/* Our replacement, which for.  */

  int sc;			/* Desired suspend count.   */
  int cur_sc;			/* Implemented suspend count.  */
  int run_sc;			/* Default sc when the program is running. */
  int pause_sc;			/* Default sc when gdb has control. */
  int resume_sc;		/* Sc resulting form the last resume. */

  thread_state_data_t state;	/* Registers, &c. */
  int state_valid : 1;		/* True if STATE is up to date. */
  int state_changed : 1;

  int aborted : 1;		/* True if thread_abort has been called.  */

  /* Bit mask of registers fetched by gdb.  This is used when we re-fetch
     STATE after aborting the thread, to detect that gdb may have out-of-date
     information.  */
  unsigned long fetched_regs;

  struct inf *inf;		/* Where we come from.  */

  struct proc *next;
};

/* The task has a thread entry with this TID.  */
#define PROC_TID_TASK 	(-1)

#define proc_is_task(proc) ((proc)->tid == PROC_TID_TASK)
#define proc_is_thread(proc) ((proc)->tid != PROC_TID_TASK)

extern int __proc_pid (struct proc *proc);

extern thread_state_t proc_get_state (struct proc *proc, int will_modify);

#define proc_debug(_proc, msg, args...) \
  do { struct proc *__proc = (_proc); \
       debug ("{proc %d/%d %p}: " msg, \
	      __proc_pid (__proc), __proc->tid, __proc , ##args); } while (0)

#if MAINTENANCE_CMDS
extern int gnu_debug_flag;
#define debug(msg, args...) \
 do { if (gnu_debug_flag) \
        fprintf (stderr, "%s: " msg "\r\n", __FUNCTION__ , ##args); } while (0)
#else
#define debug(msg, args...) (void)0
#endif

#endif /* __GNU_NAT_H__ */
