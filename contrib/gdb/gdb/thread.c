/* Multi-process/thread control for GDB, the GNU debugger.
   Copyright 1986, 1987, 1988, 1993

   Contributed by Lynx Real-Time Systems, Inc.  Los Gatos, CA.
   Free Software Foundation, Inc.

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
#include "symtab.h"
#include "frame.h"
#include "inferior.h"
#include "environ.h"
#include "value.h"
#include "target.h"
#include "thread.h"
#include "command.h"
#include "gdbcmd.h"

#include <ctype.h>
#include <sys/types.h>
#include <signal.h>

/*#include "lynxos-core.h"*/

struct thread_info
{
  struct thread_info *next;
  int pid;			/* Actual process id */
  int num;			/* Convenient handle */
  CORE_ADDR prev_pc;		/* State from wait_for_inferior */
  CORE_ADDR prev_func_start;
  char *prev_func_name;
  struct breakpoint *step_resume_breakpoint;
  struct breakpoint *through_sigtramp_breakpoint;
  CORE_ADDR step_range_start;
  CORE_ADDR step_range_end;
  CORE_ADDR step_frame_address;
  int trap_expected;
  int handling_longjmp;
  int another_trap;
};

static struct thread_info *thread_list = NULL;
static int highest_thread_num;

static void thread_command PARAMS ((char * tidstr, int from_tty));

static void prune_threads PARAMS ((void));

static void thread_switch PARAMS ((int pid));

static struct thread_info * find_thread_id PARAMS ((int num));

void
init_thread_list ()
{
  struct thread_info *tp, *tpnext;

  if (!thread_list)
    return;

  for (tp = thread_list; tp; tp = tpnext)
    {
      tpnext = tp->next;
      free (tp);
    }

  thread_list = NULL;
  highest_thread_num = 0;
}

void
add_thread (pid)
     int pid;
{
  struct thread_info *tp;

  tp = (struct thread_info *) xmalloc (sizeof (struct thread_info));

  tp->pid = pid;
  tp->num = ++highest_thread_num;
  tp->prev_pc = 0;
  tp->prev_func_start = 0;
  tp->prev_func_name = NULL;
  tp->step_range_start = 0;
  tp->step_range_end = 0;
  tp->step_frame_address =0;
  tp->step_resume_breakpoint = 0;
  tp->through_sigtramp_breakpoint = 0;
  tp->handling_longjmp = 0;
  tp->trap_expected = 0;
  tp->another_trap = 0;
  tp->next = thread_list;
  thread_list = tp;
}

static struct thread_info *
find_thread_id (num)
    int num;
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->num == num)
      return tp;

  return NULL;
}

int
valid_thread_id (num)
    int num;
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->num == num)
      return 1;

  return 0;
}

int
pid_to_thread_id (pid)
    int pid;
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->pid == pid)
      return tp->num;

  return 0;
}

int
in_thread_list (pid)
    int pid;
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->pid == pid)
      return 1;

  return 0;			/* Never heard of 'im */
}

/* Load infrun state for the thread PID.  */

void load_infrun_state (pid, prev_pc, prev_func_start, prev_func_name,
			trap_expected, step_resume_breakpoint,
			through_sigtramp_breakpoint, step_range_start,
			step_range_end, step_frame_address,
			handling_longjmp, another_trap)
     int pid;
     CORE_ADDR *prev_pc;
     CORE_ADDR *prev_func_start;
     char **prev_func_name;
     int *trap_expected;
     struct breakpoint **step_resume_breakpoint;
     struct breakpoint **through_sigtramp_breakpoint;
     CORE_ADDR *step_range_start;
     CORE_ADDR *step_range_end;
     CORE_ADDR *step_frame_address;
     int *handling_longjmp;
     int *another_trap;
{
  struct thread_info *tp;

  /* If we can't find the thread, then we're debugging a single threaded
     process.  No need to do anything in that case.  */
  tp = find_thread_id (pid_to_thread_id (pid));
  if (tp == NULL)
    return;

  *prev_pc = tp->prev_pc;
  *prev_func_start = tp->prev_func_start;
  *prev_func_name = tp->prev_func_name;
  *step_resume_breakpoint = tp->step_resume_breakpoint;
  *step_range_start = tp->step_range_start;
  *step_range_end = tp->step_range_end;
  *step_frame_address = tp->step_frame_address;
  *through_sigtramp_breakpoint = tp->through_sigtramp_breakpoint;
  *handling_longjmp = tp->handling_longjmp;
  *trap_expected = tp->trap_expected;
  *another_trap = tp->another_trap;
}

/* Save infrun state for the thread PID.  */

void save_infrun_state (pid, prev_pc, prev_func_start, prev_func_name,
			trap_expected, step_resume_breakpoint,
			through_sigtramp_breakpoint, step_range_start,
			step_range_end, step_frame_address,
			handling_longjmp, another_trap)
     int pid;
     CORE_ADDR prev_pc;
     CORE_ADDR prev_func_start;
     char *prev_func_name;
     int trap_expected;
     struct breakpoint *step_resume_breakpoint;
     struct breakpoint *through_sigtramp_breakpoint;
     CORE_ADDR step_range_start;
     CORE_ADDR step_range_end;
     CORE_ADDR step_frame_address;
     int handling_longjmp;
     int another_trap;
{
  struct thread_info *tp;

  /* If we can't find the thread, then we're debugging a single-threaded
     process.  Nothing to do in that case.  */
  tp = find_thread_id (pid_to_thread_id (pid));
  if (tp == NULL)
    return;

  tp->prev_pc = prev_pc;
  tp->prev_func_start = prev_func_start;
  tp->prev_func_name = prev_func_name;
  tp->step_resume_breakpoint = step_resume_breakpoint;
  tp->step_range_start = step_range_start;
  tp->step_range_end = step_range_end;
  tp->step_frame_address = step_frame_address;
  tp->through_sigtramp_breakpoint = through_sigtramp_breakpoint;
  tp->handling_longjmp = handling_longjmp;
  tp->trap_expected = trap_expected;
  tp->another_trap = another_trap;
}

static void
prune_threads ()
{
  struct thread_info *tp, *tpprev;

  tpprev = 0;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->pid == -1)
      {
 	if (tpprev)
	  tpprev->next = tp->next;
 	else
	  thread_list = NULL;

	free (tp);
      }
    else
      tpprev = tp;
}

/* Print information about currently known threads */

static void
info_threads_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct thread_info *tp;
  int current_pid = inferior_pid;

  /* Avoid coredumps which would happen if we tried to access a NULL
     selected_frame.  */
  if (!target_has_stack) error ("No stack.");

  for (tp = thread_list; tp; tp = tp->next)
    {
      if (! target_thread_alive (tp->pid))
 	{
	  tp->pid = -1;	/* Mark it as dead */
	  continue;
 	}

      if (tp->pid == current_pid)
	printf_filtered ("* ");
      else
	printf_filtered ("  ");

      printf_filtered ("%d %s  ", tp->num, target_pid_to_str (tp->pid));

      thread_switch (tp->pid);
      print_stack_frame (selected_frame, -1, 0);
    }

  thread_switch (current_pid);
  prune_threads ();
}

/* Switch from one thread to another. */

static void
thread_switch (pid)
     int pid;
{
  if (pid == inferior_pid)
    return;

  inferior_pid = pid;
  flush_cached_frames ();
  registers_changed ();
  stop_pc = read_pc();
  select_frame (get_current_frame (), 0);
}

static void
restore_current_thread (pid)
     int pid;
{
  if (pid != inferior_pid)
    thread_switch (pid);
}

/* Apply a GDB command to a list of threads.  List syntax is a whitespace
   seperated list of numbers, or ranges, or the keyword `all'.  Ranges consist
   of two numbers seperated by a hyphen.  Examples:

	thread apply 1 2 7 4 backtrace	Apply backtrace cmd to threads 1,2,7,4
	thread apply 2-7 9 p foo(1)	Apply p foo(1) cmd to threads 2->7 & 9
	thread apply all p x/i $pc	Apply x/i $pc cmd to all threads
*/

static void
thread_apply_all_command (cmd, from_tty)
     char *cmd;
     int from_tty;
{
  struct thread_info *tp;
  struct cleanup *old_chain;

  if (cmd == NULL || *cmd == '\000')
    error ("Please specify a command following the thread ID list");

  old_chain = make_cleanup (restore_current_thread, inferior_pid);

  for (tp = thread_list; tp; tp = tp->next)
    {
      thread_switch (tp->pid);
      printf_filtered ("\nThread %d (%s):\n", tp->num,
		       target_pid_to_str (inferior_pid));
      execute_command (cmd, from_tty);
    }
}

static void
thread_apply_command (tidlist, from_tty)
     char *tidlist;
     int from_tty;
{
  char *cmd;
  char *p;
  struct cleanup *old_chain;

  if (tidlist == NULL || *tidlist == '\000')
    error ("Please specify a thread ID list");

  for (cmd = tidlist; *cmd != '\000' && !isalpha(*cmd); cmd++);

  if (*cmd == '\000')
    error ("Please specify a command following the thread ID list");

  old_chain = make_cleanup (restore_current_thread, inferior_pid);

  while (tidlist < cmd)
    {
      struct thread_info *tp;
      int start, end;

      start = strtol (tidlist, &p, 10);
      if (p == tidlist)
	error ("Error parsing %s", tidlist);
      tidlist = p;

      while (*tidlist == ' ' || *tidlist == '\t')
	tidlist++;

      if (*tidlist == '-')	/* Got a range of IDs? */
	{
	  tidlist++;	/* Skip the - */
	  end = strtol (tidlist, &p, 10);
	  if (p == tidlist)
	    error ("Error parsing %s", tidlist);
	  tidlist = p;

	  while (*tidlist == ' ' || *tidlist == '\t')
	    tidlist++;
	}
      else
	end = start;

      for (; start <= end; start++)
	{
	  tp = find_thread_id (start);

	  if (!tp)
	    {
	      warning ("Unknown thread %d.", start);
	      continue;
	    }

	  thread_switch (tp->pid);
	  printf_filtered ("\nThread %d (%s):\n", tp->num,
			   target_pid_to_str (inferior_pid));
	  execute_command (cmd, from_tty);
	}
    }
}

/* Switch to the specified thread.  Will dispatch off to thread_apply_command
   if prefix of arg is `apply'.  */

static void
thread_command (tidstr, from_tty)
     char *tidstr;
     int from_tty;
{
  int num;
  struct thread_info *tp;

  if (!tidstr)
    error ("Please specify a thread ID.  Use the \"info threads\" command to\n\
see the IDs of currently known threads.");

  num = atoi (tidstr);

  tp = find_thread_id (num);

  if (!tp)
    error ("Thread ID %d not known.  Use the \"info threads\" command to\n\
see the IDs of currently known threads.", num);

  thread_switch (tp->pid);

  printf_filtered ("[Switching to %s]\n", target_pid_to_str (inferior_pid));
  print_stack_frame (selected_frame, selected_frame_level, 1);
}

void
_initialize_thread ()
{
  static struct cmd_list_element *thread_cmd_list = NULL;
  static struct cmd_list_element *thread_apply_list = NULL;
  extern struct cmd_list_element *cmdlist;

  add_info ("threads", info_threads_command,
	    "IDs of currently known threads.");

  add_prefix_cmd ("thread", class_run, thread_command,
		  "Use this command to switch between threads.\n\
The new thread ID must be currently known.", &thread_cmd_list, "thread ", 1,
		  &cmdlist);

  add_prefix_cmd ("apply", class_run, thread_apply_command,
		  "Apply a command to a list of threads.",
		  &thread_apply_list, "apply ", 1, &thread_cmd_list);

  add_cmd ("all", class_run, thread_apply_all_command,
	   "Apply a command to all threads.",
	   &thread_apply_list);

  add_com_alias ("t", "thread", class_run, 1);
}
