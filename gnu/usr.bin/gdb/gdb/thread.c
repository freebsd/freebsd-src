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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

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

  for (tp = thread_list; tp; tp = tp->next)
    {
      if (target_has_execution
	  && kill (tp->pid, 0) == -1)
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
  set_current_frame (create_new_frame (read_fp (), stop_pc));
  stop_frame_address = FRAME_FP (get_current_frame ());
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
