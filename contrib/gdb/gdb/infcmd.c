/* Memory-access and commands for "inferior" process, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001, 2002
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include <signal.h>
#include "gdb_string.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "frame.h"
#include "inferior.h"
#include "environ.h"
#include "value.h"
#include "gdbcmd.h"
#include "symfile.h"
#include "gdbcore.h"
#include "target.h"
#include "language.h"
#include "symfile.h"
#include "objfiles.h"
#include "completer.h"
#include "ui-out.h"
#include "event-top.h"
#include "parser-defs.h"

/* Functions exported for general use: */

void nofp_registers_info (char *, int);

void all_registers_info (char *, int);

void registers_info (char *, int);

/* Local functions: */

void continue_command (char *, int);

static void print_return_value (int struct_return, struct type *value_type);

static void finish_command_continuation (struct continuation_arg *);

static void until_next_command (int);

static void until_command (char *, int);

static void path_info (char *, int);

static void path_command (char *, int);

static void unset_command (char *, int);

static void float_info (char *, int);

static void detach_command (char *, int);

static void interrupt_target_command (char *args, int from_tty);

static void unset_environment_command (char *, int);

static void set_environment_command (char *, int);

static void environment_info (char *, int);

static void program_info (char *, int);

static void finish_command (char *, int);

static void signal_command (char *, int);

static void jump_command (char *, int);

static void step_1 (int, int, char *);
static void step_once (int skip_subroutines, int single_inst, int count);
static void step_1_continuation (struct continuation_arg *arg);

void nexti_command (char *, int);

void stepi_command (char *, int);

static void next_command (char *, int);

static void step_command (char *, int);

static void run_command (char *, int);

static void run_no_args_command (char *args, int from_tty);

static void go_command (char *line_no, int from_tty);

static int strip_bg_char (char **);

void _initialize_infcmd (void);

#define GO_USAGE   "Usage: go <location>\n"

static void breakpoint_auto_delete_contents (PTR);

#define ERROR_NO_INFERIOR \
   if (!target_has_execution) error ("The program is not being run.");

/* String containing arguments to give to the program, separated by spaces.
   Empty string (pointer to '\0') means no args.  */

static char *inferior_args;

/* The inferior arguments as a vector.  If INFERIOR_ARGC is nonzero,
   then we must compute INFERIOR_ARGS from this (via the target).  */

static int inferior_argc;
static char **inferior_argv;

/* File name for default use for standard in/out in the inferior.  */

char *inferior_io_terminal;

/* Pid of our debugged inferior, or 0 if no inferior now.
   Since various parts of infrun.c test this to see whether there is a program
   being debugged it should be nonzero (currently 3 is used) for remote
   debugging.  */

ptid_t inferior_ptid;

/* Last signal that the inferior received (why it stopped).  */

enum target_signal stop_signal;

/* Address at which inferior stopped.  */

CORE_ADDR stop_pc;

/* Chain containing status of breakpoint(s) that we have stopped at.  */

bpstat stop_bpstat;

/* Flag indicating that a command has proceeded the inferior past the
   current breakpoint.  */

int breakpoint_proceeded;

/* Nonzero if stopped due to a step command.  */

int stop_step;

/* Nonzero if stopped due to completion of a stack dummy routine.  */

int stop_stack_dummy;

/* Nonzero if stopped due to a random (unexpected) signal in inferior
   process.  */

int stopped_by_random_signal;

/* Range to single step within.
   If this is nonzero, respond to a single-step signal
   by continuing to step if the pc is in this range.  */

CORE_ADDR step_range_start;	/* Inclusive */
CORE_ADDR step_range_end;	/* Exclusive */

/* Stack frame address as of when stepping command was issued.
   This is how we know when we step into a subroutine call,
   and how to set the frame for the breakpoint used to step out.  */

CORE_ADDR step_frame_address;

/* Our notion of the current stack pointer.  */

CORE_ADDR step_sp;

enum step_over_calls_kind step_over_calls;

/* If stepping, nonzero means step count is > 1
   so don't print frame next time inferior stops
   if it stops due to stepping.  */

int step_multi;

/* Environment to use for running inferior,
   in format described in environ.h.  */

struct environ *inferior_environ;

/* Accessor routines. */

char *
get_inferior_args (void)
{
  if (inferior_argc != 0)
    {
      char *n, *old;

      n = gdbarch_construct_inferior_arguments (current_gdbarch,
						inferior_argc, inferior_argv);
      old = set_inferior_args (n);
      xfree (old);
    }

  if (inferior_args == NULL)
    inferior_args = xstrdup ("");

  return inferior_args;
}

char *
set_inferior_args (char *newargs)
{
  char *saved_args = inferior_args;

  inferior_args = newargs;
  inferior_argc = 0;
  inferior_argv = 0;

  return saved_args;
}

void
set_inferior_args_vector (int argc, char **argv)
{
  inferior_argc = argc;
  inferior_argv = argv;
}

/* Notice when `set args' is run.  */
static void
notice_args_set (char *args, int from_tty, struct cmd_list_element *c)
{
  inferior_argc = 0;
  inferior_argv = 0;
}

/* Notice when `show args' is run.  */
static void
notice_args_read (char *args, int from_tty, struct cmd_list_element *c)
{
  /* Might compute the value.  */
  get_inferior_args ();
}


/* Compute command-line string given argument vector.  This does the
   same shell processing as fork_inferior.  */
/* ARGSUSED */
char *
construct_inferior_arguments (struct gdbarch *gdbarch, int argc, char **argv)
{
  char *result;

  if (STARTUP_WITH_SHELL)
    {
      /* This holds all the characters considered special to the
	 typical Unix shells.  We include `^' because the SunOS
	 /bin/sh treats it as a synonym for `|'.  */
      char *special = "\"!#$&*()\\|[]{}<>?'\"`~^; \t\n";
      int i;
      int length = 0;
      char *out, *cp;

      /* We over-compute the size.  It shouldn't matter.  */
      for (i = 0; i < argc; ++i)
	length += 2 * strlen (argv[i]) + 1;

      result = (char *) xmalloc (length);
      out = result;

      for (i = 0; i < argc; ++i)
	{
	  if (i > 0)
	    *out++ = ' ';

	  for (cp = argv[i]; *cp; ++cp)
	    {
	      if (strchr (special, *cp) != NULL)
		*out++ = '\\';
	      *out++ = *cp;
	    }
	}
      *out = '\0';
    }
  else
    {
      /* In this case we can't handle arguments that contain spaces,
	 tabs, or newlines -- see breakup_args().  */
      int i;
      int length = 0;

      for (i = 0; i < argc; ++i)
	{
	  char *cp = strchr (argv[i], ' ');
	  if (cp == NULL)
	    cp = strchr (argv[i], '\t');
	  if (cp == NULL)
	    cp = strchr (argv[i], '\n');
	  if (cp != NULL)
	    error ("can't handle command-line argument containing whitespace");
	  length += strlen (argv[i]) + 1;
	}

      result = (char *) xmalloc (length);
      result[0] = '\0';
      for (i = 0; i < argc; ++i)
	{
	  if (i > 0)
	    strcat (result, " ");
	  strcat (result, argv[i]);
	}
    }

  return result;
}


/* This function detects whether or not a '&' character (indicating
   background execution) has been added as *the last* of the arguments ARGS
   of a command. If it has, it removes it and returns 1. Otherwise it
   does nothing and returns 0. */
static int
strip_bg_char (char **args)
{
  char *p = NULL;

  p = strchr (*args, '&');

  if (p)
    {
      if (p == (*args + strlen (*args) - 1))
	{
	  if (strlen (*args) > 1)
	    {
	      do
		p--;
	      while (*p == ' ' || *p == '\t');
	      *(p + 1) = '\0';
	    }
	  else
	    *args = 0;
	  return 1;
	}
    }
  return 0;
}

/* ARGSUSED */
void
tty_command (char *file, int from_tty)
{
  if (file == 0)
    error_no_arg ("terminal name for running target process");

  inferior_io_terminal = savestring (file, strlen (file));
}

static void
run_command (char *args, int from_tty)
{
  char *exec_file;

  dont_repeat ();

  if (! ptid_equal (inferior_ptid, null_ptid) && target_has_execution)
    {
      if (from_tty
	  && !query ("The program being debugged has been started already.\n\
Start it from the beginning? "))
	error ("Program not restarted.");
      target_kill ();
#if defined(SOLIB_RESTART)
      SOLIB_RESTART ();
#endif
      init_wait_for_inferior ();
    }

  clear_breakpoint_hit_counts ();

  /* Purge old solib objfiles. */
  objfile_purge_solibs ();

  do_run_cleanups (NULL);

  /* The comment here used to read, "The exec file is re-read every
     time we do a generic_mourn_inferior, so we just have to worry
     about the symbol file."  The `generic_mourn_inferior' function
     gets called whenever the program exits.  However, suppose the
     program exits, and *then* the executable file changes?  We need
     to check again here.  Since reopen_exec_file doesn't do anything
     if the timestamp hasn't changed, I don't see the harm.  */
  reopen_exec_file ();
  reread_symbols ();

  exec_file = (char *) get_exec_file (0);

  /* We keep symbols from add-symbol-file, on the grounds that the
     user might want to add some symbols before running the program
     (right?).  But sometimes (dynamic loading where the user manually
     introduces the new symbols with add-symbol-file), the code which
     the symbols describe does not persist between runs.  Currently
     the user has to manually nuke all symbols between runs if they
     want them to go away (PR 2207).  This is probably reasonable.  */

  if (!args)
    {
      if (event_loop_p && target_can_async_p ())
	async_disable_stdin ();
    }
  else
    {
      int async_exec = strip_bg_char (&args);

      /* If we get a request for running in the bg but the target
         doesn't support it, error out. */
      if (event_loop_p && async_exec && !target_can_async_p ())
	error ("Asynchronous execution not supported on this target.");

      /* If we don't get a request of running in the bg, then we need
         to simulate synchronous (fg) execution. */
      if (event_loop_p && !async_exec && target_can_async_p ())
	{
	  /* Simulate synchronous execution */
	  async_disable_stdin ();
	}

      /* If there were other args, beside '&', process them. */
      if (args)
	{
          char *old_args = set_inferior_args (xstrdup (args));
          xfree (old_args);
	}
    }

  if (from_tty)
    {
      ui_out_field_string (uiout, NULL, "Starting program");
      ui_out_text (uiout, ": ");
      if (exec_file)
	ui_out_field_string (uiout, "execfile", exec_file);
      ui_out_spaces (uiout, 1);
      /* We call get_inferior_args() because we might need to compute
	 the value now.  */
      ui_out_field_string (uiout, "infargs", get_inferior_args ());
      ui_out_text (uiout, "\n");
      ui_out_flush (uiout);
    }

  /* We call get_inferior_args() because we might need to compute
     the value now.  */
  target_create_inferior (exec_file, get_inferior_args (),
			  environ_vector (inferior_environ));
}


static void
run_no_args_command (char *args, int from_tty)
{
  char *old_args = set_inferior_args (xstrdup (""));
  xfree (old_args);
}


void
continue_command (char *proc_count_exp, int from_tty)
{
  int async_exec = 0;
  ERROR_NO_INFERIOR;

  /* Find out whether we must run in the background. */
  if (proc_count_exp != NULL)
    async_exec = strip_bg_char (&proc_count_exp);

  /* If we must run in the background, but the target can't do it,
     error out. */
  if (event_loop_p && async_exec && !target_can_async_p ())
    error ("Asynchronous execution not supported on this target.");

  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously. */
  if (event_loop_p && !async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  /* If have argument (besides '&'), set proceed count of breakpoint
     we stopped at.  */
  if (proc_count_exp != NULL)
    {
      bpstat bs = stop_bpstat;
      int num = bpstat_num (&bs);
      if (num == 0 && from_tty)
	{
	  printf_filtered
	    ("Not stopped at any breakpoint; argument ignored.\n");
	}
      while (num != 0)
	{
	  set_ignore_count (num,
			    parse_and_eval_long (proc_count_exp) - 1,
			    from_tty);
	  /* set_ignore_count prints a message ending with a period.
	     So print two spaces before "Continuing.".  */
	  if (from_tty)
	    printf_filtered ("  ");
	  num = bpstat_num (&bs);
	}
    }

  if (from_tty)
    printf_filtered ("Continuing.\n");

  clear_proceed_status ();

  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);
}

/* Step until outside of current statement.  */

/* ARGSUSED */
static void
step_command (char *count_string, int from_tty)
{
  step_1 (0, 0, count_string);
}

/* Likewise, but skip over subroutine calls as if single instructions.  */

/* ARGSUSED */
static void
next_command (char *count_string, int from_tty)
{
  step_1 (1, 0, count_string);
}

/* Likewise, but step only one instruction.  */

/* ARGSUSED */
void
stepi_command (char *count_string, int from_tty)
{
  step_1 (0, 1, count_string);
}

/* ARGSUSED */
void
nexti_command (char *count_string, int from_tty)
{
  step_1 (1, 1, count_string);
}

static void
disable_longjmp_breakpoint_cleanup (void *ignore)
{
  disable_longjmp_breakpoint ();
}

static void
step_1 (int skip_subroutines, int single_inst, char *count_string)
{
  register int count = 1;
  struct frame_info *frame;
  struct cleanup *cleanups = 0;
  int async_exec = 0;

  ERROR_NO_INFERIOR;

  if (count_string)
    async_exec = strip_bg_char (&count_string);

  /* If we get a request for running in the bg but the target
     doesn't support it, error out. */
  if (event_loop_p && async_exec && !target_can_async_p ())
    error ("Asynchronous execution not supported on this target.");

  /* If we don't get a request of running in the bg, then we need
     to simulate synchronous (fg) execution. */
  if (event_loop_p && !async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  count = count_string ? parse_and_eval_long (count_string) : 1;

  if (!single_inst || skip_subroutines)		/* leave si command alone */
    {
      enable_longjmp_breakpoint ();
      if (!event_loop_p || !target_can_async_p ())
	cleanups = make_cleanup (disable_longjmp_breakpoint_cleanup, 0 /*ignore*/);
      else
        make_exec_cleanup (disable_longjmp_breakpoint_cleanup, 0 /*ignore*/);
    }

  /* In synchronous case, all is well, just use the regular for loop. */
  if (!event_loop_p || !target_can_async_p ())
    {
      for (; count > 0; count--)
	{
	  clear_proceed_status ();

	  frame = get_current_frame ();
	  if (!frame)		/* Avoid coredump here.  Why tho? */
	    error ("No current frame");
	  step_frame_address = FRAME_FP (frame);
	  step_sp = read_sp ();

	  if (!single_inst)
	    {
	      find_pc_line_pc_range (stop_pc, &step_range_start, &step_range_end);
	      if (step_range_end == 0)
		{
		  char *name;
		  if (find_pc_partial_function (stop_pc, &name, &step_range_start,
						&step_range_end) == 0)
		    error ("Cannot find bounds of current function");

		  target_terminal_ours ();
		  printf_filtered ("\
Single stepping until exit from function %s, \n\
which has no line number information.\n", name);
		}
	    }
	  else
	    {
	      /* Say we are stepping, but stop after one insn whatever it does.  */
	      step_range_start = step_range_end = 1;
	      if (!skip_subroutines)
		/* It is stepi.
		   Don't step over function calls, not even to functions lacking
		   line numbers.  */
		step_over_calls = STEP_OVER_NONE;
	    }

	  if (skip_subroutines)
	    step_over_calls = STEP_OVER_ALL;

	  step_multi = (count > 1);
	  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);

	  if (!stop_step)
	    break;

	  /* FIXME: On nexti, this may have already been done (when we hit the
	     step resume break, I think).  Probably this should be moved to
	     wait_for_inferior (near the top).  */
#if defined (SHIFT_INST_REGS)
	  SHIFT_INST_REGS ();
#endif
	}

      if (!single_inst || skip_subroutines)
	do_cleanups (cleanups);
      return;
    }
  /* In case of asynchronous target things get complicated, do only
     one step for now, before returning control to the event loop. Let
     the continuation figure out how many other steps we need to do,
     and handle them one at the time, through step_once(). */
  else
    {
      if (event_loop_p && target_can_async_p ())
	step_once (skip_subroutines, single_inst, count);
    }
}

/* Called after we are done with one step operation, to check whether
   we need to step again, before we print the prompt and return control
   to the user. If count is > 1, we will need to do one more call to
   proceed(), via step_once(). Basically it is like step_once and
   step_1_continuation are co-recursive. */
static void
step_1_continuation (struct continuation_arg *arg)
{
  int count;
  int skip_subroutines;
  int single_inst;

  skip_subroutines = arg->data.integer;
  single_inst      = arg->next->data.integer;
  count            = arg->next->next->data.integer;

  if (stop_step)
    {
      /* FIXME: On nexti, this may have already been done (when we hit the
	 step resume break, I think).  Probably this should be moved to
	 wait_for_inferior (near the top).  */
#if defined (SHIFT_INST_REGS)
      SHIFT_INST_REGS ();
#endif
      step_once (skip_subroutines, single_inst, count - 1);
    }
  else
    if (!single_inst || skip_subroutines)
      do_exec_cleanups (ALL_CLEANUPS);
}

/* Do just one step operation. If count >1 we will have to set up a
   continuation to be done after the target stops (after this one
   step). This is useful to implement the 'step n' kind of commands, in
   case of asynchronous targets. We had to split step_1 into two parts,
   one to be done before proceed() and one afterwards. This function is
   called in case of step n with n>1, after the first step operation has
   been completed.*/
static void 
step_once (int skip_subroutines, int single_inst, int count)
{ 
  struct continuation_arg *arg1; 
  struct continuation_arg *arg2;
  struct continuation_arg *arg3; 
  struct frame_info *frame;

  if (count > 0)
    {
      clear_proceed_status ();

      frame = get_current_frame ();
      if (!frame)		/* Avoid coredump here.  Why tho? */
	error ("No current frame");
      step_frame_address = FRAME_FP (frame);
      step_sp = read_sp ();

      if (!single_inst)
	{
	  find_pc_line_pc_range (stop_pc, &step_range_start, &step_range_end);

	  /* If we have no line info, switch to stepi mode.  */
	  if (step_range_end == 0 && step_stop_if_no_debug)
	    {
	      step_range_start = step_range_end = 1;
	    }
	  else if (step_range_end == 0)
	    {
	      char *name;
	      if (find_pc_partial_function (stop_pc, &name, &step_range_start,
					    &step_range_end) == 0)
		error ("Cannot find bounds of current function");

	      target_terminal_ours ();
	      printf_filtered ("\
Single stepping until exit from function %s, \n\
which has no line number information.\n", name);
	    }
	}
      else
	{
	  /* Say we are stepping, but stop after one insn whatever it does.  */
	  step_range_start = step_range_end = 1;
	  if (!skip_subroutines)
	    /* It is stepi.
	       Don't step over function calls, not even to functions lacking
	       line numbers.  */
	    step_over_calls = STEP_OVER_NONE;
	}

      if (skip_subroutines)
	step_over_calls = STEP_OVER_ALL;

      step_multi = (count > 1);
      arg1 =
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg2 =
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg3 =
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg1->next = arg2;
      arg1->data.integer = skip_subroutines;
      arg2->next = arg3;
      arg2->data.integer = single_inst;
      arg3->next = NULL;
      arg3->data.integer = count;
      add_intermediate_continuation (step_1_continuation, arg1);
      proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
    }
}


/* Continue program at specified address.  */

static void
jump_command (char *arg, int from_tty)
{
  register CORE_ADDR addr;
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  struct symbol *fn;
  struct symbol *sfn;
  int async_exec = 0;

  ERROR_NO_INFERIOR;

  /* Find out whether we must run in the background. */
  if (arg != NULL)
    async_exec = strip_bg_char (&arg);

  /* If we must run in the background, but the target can't do it,
     error out. */
  if (event_loop_p && async_exec && !target_can_async_p ())
    error ("Asynchronous execution not supported on this target.");

  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously. */
  if (event_loop_p && !async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  if (!arg)
    error_no_arg ("starting address");

  sals = decode_line_spec_1 (arg, 1);
  if (sals.nelts != 1)
    {
      error ("Unreasonable jump request");
    }

  sal = sals.sals[0];
  xfree (sals.sals);

  if (sal.symtab == 0 && sal.pc == 0)
    error ("No source file has been specified.");

  resolve_sal_pc (&sal);	/* May error out */

  /* See if we are trying to jump to another function. */
  fn = get_frame_function (get_current_frame ());
  sfn = find_pc_function (sal.pc);
  if (fn != NULL && sfn != fn)
    {
      if (!query ("Line %d is not in `%s'.  Jump anyway? ", sal.line,
		  SYMBOL_SOURCE_NAME (fn)))
	{
	  error ("Not confirmed.");
	  /* NOTREACHED */
	}
    }

  if (sfn != NULL)
    {
      fixup_symbol_section (sfn, 0);
      if (section_is_overlay (SYMBOL_BFD_SECTION (sfn)) &&
	  !section_is_mapped (SYMBOL_BFD_SECTION (sfn)))
	{
	  if (!query ("WARNING!!!  Destination is in unmapped overlay!  Jump anyway? "))
	    {
	      error ("Not confirmed.");
	      /* NOTREACHED */
	    }
	}
    }

  addr = sal.pc;

  if (from_tty)
    {
      printf_filtered ("Continuing at ");
      print_address_numeric (addr, 1, gdb_stdout);
      printf_filtered (".\n");
    }

  clear_proceed_status ();
  proceed (addr, TARGET_SIGNAL_0, 0);
}


/* Go to line or address in current procedure */
static void
go_command (char *line_no, int from_tty)
{
  if (line_no == (char *) NULL || !*line_no)
    printf_filtered (GO_USAGE);
  else
    {
      tbreak_command (line_no, from_tty);
      jump_command (line_no, from_tty);
    }
}


/* Continue program giving it specified signal.  */

static void
signal_command (char *signum_exp, int from_tty)
{
  enum target_signal oursig;

  dont_repeat ();		/* Too dangerous.  */
  ERROR_NO_INFERIOR;

  if (!signum_exp)
    error_no_arg ("signal number");

  /* It would be even slicker to make signal names be valid expressions,
     (the type could be "enum $signal" or some such), then the user could
     assign them to convenience variables.  */
  oursig = target_signal_from_name (signum_exp);

  if (oursig == TARGET_SIGNAL_UNKNOWN)
    {
      /* No, try numeric.  */
      int num = parse_and_eval_long (signum_exp);

      if (num == 0)
	oursig = TARGET_SIGNAL_0;
      else
	oursig = target_signal_from_command (num);
    }

  if (from_tty)
    {
      if (oursig == TARGET_SIGNAL_0)
	printf_filtered ("Continuing with no signal.\n");
      else
	printf_filtered ("Continuing with signal %s.\n",
			 target_signal_to_name (oursig));
    }

  clear_proceed_status ();
  /* "signal 0" should not get stuck if we are stopped at a breakpoint.
     FIXME: Neither should "signal foo" but when I tried passing
     (CORE_ADDR)-1 unconditionally I got a testsuite failure which I haven't
     tried to track down yet.  */
  proceed (oursig == TARGET_SIGNAL_0 ? (CORE_ADDR) -1 : stop_pc, oursig, 0);
}

/* Call breakpoint_auto_delete on the current contents of the bpstat
   pointed to by arg (which is really a bpstat *).  */

static void
breakpoint_auto_delete_contents (PTR arg)
{
  breakpoint_auto_delete (*(bpstat *) arg);
}


/* Execute a "stack dummy", a piece of code stored in the stack
   by the debugger to be executed in the inferior.

   To call: first, do PUSH_DUMMY_FRAME.
   Then push the contents of the dummy.  It should end with a breakpoint insn.
   Then call here, passing address at which to start the dummy.

   The contents of all registers are saved before the dummy frame is popped
   and copied into the buffer BUFFER.

   The dummy's frame is automatically popped whenever that break is hit.
   If that is the first time the program stops, run_stack_dummy
   returns to its caller with that frame already gone and returns 0.
   
   Otherwise, run_stack-dummy returns a non-zero value.
   If the called function receives a random signal, we do not allow the user
   to continue executing it as this may not work.  The dummy frame is poped
   and we return 1.
   If we hit a breakpoint, we leave the frame in place and return 2 (the frame
   will eventually be popped when we do hit the dummy end breakpoint).  */

int
run_stack_dummy (CORE_ADDR addr, char *buffer)
{
  struct cleanup *old_cleanups = make_cleanup (null_cleanup, 0);
  int saved_async = 0;

  /* Now proceed, having reached the desired place.  */
  clear_proceed_status ();

  if (CALL_DUMMY_BREAKPOINT_OFFSET_P)
    {
      struct breakpoint *bpt;
      struct symtab_and_line sal;

      INIT_SAL (&sal);		/* initialize to zeroes */
      if (CALL_DUMMY_LOCATION == AT_ENTRY_POINT)
	{
	  sal.pc = CALL_DUMMY_ADDRESS ();
	}
      else
	{
	  sal.pc = addr - CALL_DUMMY_START_OFFSET + CALL_DUMMY_BREAKPOINT_OFFSET;
	}
      sal.section = find_pc_overlay (sal.pc);

      /* Set up a FRAME for the dummy frame so we can pass it to
         set_momentary_breakpoint.  We need to give the breakpoint a
         frame in case there is only one copy of the dummy (e.g.
         CALL_DUMMY_LOCATION == AFTER_TEXT_END).  */
      flush_cached_frames ();
      set_current_frame (create_new_frame (read_fp (), sal.pc));

      /* If defined, CALL_DUMMY_BREAKPOINT_OFFSET is where we need to put
         a breakpoint instruction.  If not, the call dummy already has the
         breakpoint instruction in it.

         addr is the address of the call dummy plus the CALL_DUMMY_START_OFFSET,
         so we need to subtract the CALL_DUMMY_START_OFFSET.  */
      bpt = set_momentary_breakpoint (sal,
				      get_current_frame (),
				      bp_call_dummy);
      bpt->disposition = disp_del;

      /* If all error()s out of proceed ended up calling normal_stop (and
         perhaps they should; it already does in the special case of error
         out of resume()), then we wouldn't need this.  */
      make_cleanup (breakpoint_auto_delete_contents, &stop_bpstat);
    }

  disable_watchpoints_before_interactive_call_start ();
  proceed_to_finish = 1;	/* We want stop_registers, please... */

  if (target_can_async_p ())
    saved_async = target_async_mask (0);

  proceed (addr, TARGET_SIGNAL_0, 0);

  if (saved_async)
    target_async_mask (saved_async);

  enable_watchpoints_after_interactive_call_stop ();

  discard_cleanups (old_cleanups);

  /* We can stop during an inferior call because a signal is received. */
  if (stopped_by_random_signal)
    return 1;
    
  /* We may also stop prematurely because we hit a breakpoint in the
     called routine. */
  if (!stop_stack_dummy)
    return 2;

  /* On normal return, the stack dummy has been popped already.  */

  memcpy (buffer, stop_registers, REGISTER_BYTES);
  return 0;
}

/* Proceed until we reach a different source line with pc greater than
   our current one or exit the function.  We skip calls in both cases.

   Note that eventually this command should probably be changed so
   that only source lines are printed out when we hit the breakpoint
   we set.  This may involve changes to wait_for_inferior and the
   proceed status code.  */

/* ARGSUSED */
static void
until_next_command (int from_tty)
{
  struct frame_info *frame;
  CORE_ADDR pc;
  struct symbol *func;
  struct symtab_and_line sal;

  clear_proceed_status ();

  frame = get_current_frame ();

  /* Step until either exited from this function or greater
     than the current line (if in symbolic section) or pc (if
     not). */

  pc = read_pc ();
  func = find_pc_function (pc);

  if (!func)
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (pc);

      if (msymbol == NULL)
	error ("Execution is not within a known function.");

      step_range_start = SYMBOL_VALUE_ADDRESS (msymbol);
      step_range_end = pc;
    }
  else
    {
      sal = find_pc_line (pc, 0);

      step_range_start = BLOCK_START (SYMBOL_BLOCK_VALUE (func));
      step_range_end = sal.end;
    }

  step_over_calls = STEP_OVER_ALL;
  step_frame_address = FRAME_FP (frame);
  step_sp = read_sp ();

  step_multi = 0;		/* Only one call to proceed */

  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
}

static void
until_command (char *arg, int from_tty)
{
  int async_exec = 0;

  if (!target_has_execution)
    error ("The program is not running.");

  /* Find out whether we must run in the background. */
  if (arg != NULL)
    async_exec = strip_bg_char (&arg);

  /* If we must run in the background, but the target can't do it,
     error out. */
  if (event_loop_p && async_exec && !target_can_async_p ())
    error ("Asynchronous execution not supported on this target.");

  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously. */
  if (event_loop_p && !async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  if (arg)
    until_break_command (arg, from_tty);
  else
    until_next_command (from_tty);
}


/* Print the result of a function at the end of a 'finish' command. */
static void
print_return_value (int structure_return, struct type *value_type)
{
  struct value *value;
  static struct ui_stream *stb = NULL;

  if (!structure_return)
    {
      value = value_being_returned (value_type, stop_registers, structure_return);
      stb = ui_out_stream_new (uiout);
      ui_out_text (uiout, "Value returned is ");
      ui_out_field_fmt (uiout, "gdb-result-var", "$%d", record_latest_value (value));
      ui_out_text (uiout, " = ");
      value_print (value, stb->stream, 0, Val_no_prettyprint);
      ui_out_field_stream (uiout, "return-value", stb);
      ui_out_text (uiout, "\n");
    }
  else
    {
      /* We cannot determine the contents of the structure because
	 it is on the stack, and we don't know where, since we did not
	 initiate the call, as opposed to the call_function_by_hand case */
#ifdef VALUE_RETURNED_FROM_STACK
      value = 0;
      ui_out_text (uiout, "Value returned has type: ");
      ui_out_field_string (uiout, "return-type", TYPE_NAME (value_type));
      ui_out_text (uiout, ".");
      ui_out_text (uiout, " Cannot determine contents\n");
#else
      value = value_being_returned (value_type, stop_registers, structure_return);
      stb = ui_out_stream_new (uiout);
      ui_out_text (uiout, "Value returned is ");
      ui_out_field_fmt (uiout, "gdb-result-var", "$%d", record_latest_value (value));
      ui_out_text (uiout, " = ");
      value_print (value, stb->stream, 0, Val_no_prettyprint);
      ui_out_field_stream (uiout, "return-value", stb);
      ui_out_text (uiout, "\n");
#endif
    }
}

/* Stuff that needs to be done by the finish command after the target
   has stopped.  In asynchronous mode, we wait for the target to stop in
   the call to poll or select in the event loop, so it is impossible to
   do all the stuff as part of the finish_command function itself. The
   only chance we have to complete this command is in
   fetch_inferior_event, which is called by the event loop as soon as it
   detects that the target has stopped. This function is called via the
   cmd_continuation pointer. */
void
finish_command_continuation (struct continuation_arg *arg)
{
  register struct symbol *function;
  struct breakpoint *breakpoint;
  struct cleanup *cleanups;

  breakpoint = (struct breakpoint *) arg->data.pointer;
  function   = (struct symbol *)     arg->next->data.pointer;
  cleanups   = (struct cleanup *)    arg->next->next->data.pointer;

  if (bpstat_find_breakpoint (stop_bpstat, breakpoint) != NULL
      && function != 0)
    {
      struct type *value_type;
      CORE_ADDR funcaddr;
      int struct_return;

      value_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (function));
      if (!value_type)
	internal_error (__FILE__, __LINE__,
			"finish_command: function has no target type");

      if (TYPE_CODE (value_type) == TYPE_CODE_VOID)
	{
	  do_exec_cleanups (cleanups);
	  return;
	}

      funcaddr = BLOCK_START (SYMBOL_BLOCK_VALUE (function));

      struct_return = using_struct_return (value_of_variable (function, NULL),
					   funcaddr,
					   check_typedef (value_type),
					   BLOCK_GCC_COMPILED (SYMBOL_BLOCK_VALUE (function)));

      print_return_value (struct_return, value_type); 
    }
  do_exec_cleanups (cleanups);
}

/* "finish": Set a temporary breakpoint at the place
   the selected frame will return to, then continue.  */

static void
finish_command (char *arg, int from_tty)
{
  struct symtab_and_line sal;
  register struct frame_info *frame;
  register struct symbol *function;
  struct breakpoint *breakpoint;
  struct cleanup *old_chain;
  struct continuation_arg *arg1, *arg2, *arg3;

  int async_exec = 0;

  /* Find out whether we must run in the background. */
  if (arg != NULL)
    async_exec = strip_bg_char (&arg);

  /* If we must run in the background, but the target can't do it,
     error out. */
  if (event_loop_p && async_exec && !target_can_async_p ())
    error ("Asynchronous execution not supported on this target.");

  /* If we are not asked to run in the bg, then prepare to run in the
     foreground, synchronously. */
  if (event_loop_p && !async_exec && target_can_async_p ())
    {
      /* Simulate synchronous execution */
      async_disable_stdin ();
    }

  if (arg)
    error ("The \"finish\" command does not take any arguments.");
  if (!target_has_execution)
    error ("The program is not running.");
  if (selected_frame == NULL)
    error ("No selected frame.");

  frame = get_prev_frame (selected_frame);
  if (frame == 0)
    error ("\"finish\" not meaningful in the outermost frame.");

  clear_proceed_status ();

  sal = find_pc_line (frame->pc, 0);
  sal.pc = frame->pc;

  breakpoint = set_momentary_breakpoint (sal, frame, bp_finish);

  if (!event_loop_p || !target_can_async_p ())
    old_chain = make_cleanup_delete_breakpoint (breakpoint);
  else
    old_chain = make_exec_cleanup_delete_breakpoint (breakpoint);

  /* Find the function we will return from.  */

  function = find_pc_function (selected_frame->pc);

  /* Print info on the selected frame, including level number
     but not source.  */
  if (from_tty)
    {
      printf_filtered ("Run till exit from ");
      print_stack_frame (selected_frame, selected_frame_level, 0);
    }

  /* If running asynchronously and the target support asynchronous
     execution, set things up for the rest of the finish command to be
     completed later on, when gdb has detected that the target has
     stopped, in fetch_inferior_event. */
  if (event_loop_p && target_can_async_p ())
    {
      arg1 =
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg2 =
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg3 =
	(struct continuation_arg *) xmalloc (sizeof (struct continuation_arg));
      arg1->next = arg2;
      arg2->next = arg3;
      arg3->next = NULL;
      arg1->data.pointer = breakpoint;
      arg2->data.pointer = function;
      arg3->data.pointer = old_chain;
      add_continuation (finish_command_continuation, arg1);
    }

  proceed_to_finish = 1;	/* We want stop_registers, please... */
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 0);

  /* Do this only if not running asynchronously or if the target
     cannot do async execution. Otherwise, complete this command when
     the target actually stops, in fetch_inferior_event. */
  if (!event_loop_p || !target_can_async_p ())
    {

      /* Did we stop at our breakpoint? */
      if (bpstat_find_breakpoint (stop_bpstat, breakpoint) != NULL
	  && function != 0)
	{
	  struct type *value_type;
	  CORE_ADDR funcaddr;
	  int struct_return;

	  value_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (function));
	  if (!value_type)
	    internal_error (__FILE__, __LINE__,
			    "finish_command: function has no target type");

	  /* FIXME: Shouldn't we do the cleanups before returning? */
	  if (TYPE_CODE (value_type) == TYPE_CODE_VOID)
	    return;

	  funcaddr = BLOCK_START (SYMBOL_BLOCK_VALUE (function));

	  struct_return =
	    using_struct_return (value_of_variable (function, NULL),
				 funcaddr,
				 check_typedef (value_type),
			BLOCK_GCC_COMPILED (SYMBOL_BLOCK_VALUE (function)));

	  print_return_value (struct_return, value_type); 
	}
      do_cleanups (old_chain);
    }
}

/* ARGSUSED */
static void
program_info (char *args, int from_tty)
{
  bpstat bs = stop_bpstat;
  int num = bpstat_num (&bs);

  if (!target_has_execution)
    {
      printf_filtered ("The program being debugged is not being run.\n");
      return;
    }

  target_files_info ();
  printf_filtered ("Program stopped at %s.\n",
		   local_hex_string ((unsigned long) stop_pc));
  if (stop_step)
    printf_filtered ("It stopped after being stepped.\n");
  else if (num != 0)
    {
      /* There may be several breakpoints in the same place, so this
         isn't as strange as it seems.  */
      while (num != 0)
	{
	  if (num < 0)
	    {
	      printf_filtered ("It stopped at a breakpoint that has ");
	      printf_filtered ("since been deleted.\n");
	    }
	  else
	    printf_filtered ("It stopped at breakpoint %d.\n", num);
	  num = bpstat_num (&bs);
	}
    }
  else if (stop_signal != TARGET_SIGNAL_0)
    {
      printf_filtered ("It stopped with signal %s, %s.\n",
		       target_signal_to_name (stop_signal),
		       target_signal_to_string (stop_signal));
    }

  if (!from_tty)
    {
      printf_filtered ("Type \"info stack\" or \"info registers\" ");
      printf_filtered ("for more information.\n");
    }
}

static void
environment_info (char *var, int from_tty)
{
  if (var)
    {
      register char *val = get_in_environ (inferior_environ, var);
      if (val)
	{
	  puts_filtered (var);
	  puts_filtered (" = ");
	  puts_filtered (val);
	  puts_filtered ("\n");
	}
      else
	{
	  puts_filtered ("Environment variable \"");
	  puts_filtered (var);
	  puts_filtered ("\" not defined.\n");
	}
    }
  else
    {
      register char **vector = environ_vector (inferior_environ);
      while (*vector)
	{
	  puts_filtered (*vector++);
	  puts_filtered ("\n");
	}
    }
}

static void
set_environment_command (char *arg, int from_tty)
{
  register char *p, *val, *var;
  int nullset = 0;

  if (arg == 0)
    error_no_arg ("environment variable and value");

  /* Find seperation between variable name and value */
  p = (char *) strchr (arg, '=');
  val = (char *) strchr (arg, ' ');

  if (p != 0 && val != 0)
    {
      /* We have both a space and an equals.  If the space is before the
         equals, walk forward over the spaces til we see a nonspace 
         (possibly the equals). */
      if (p > val)
	while (*val == ' ')
	  val++;

      /* Now if the = is after the char following the spaces,
         take the char following the spaces.  */
      if (p > val)
	p = val - 1;
    }
  else if (val != 0 && p == 0)
    p = val;

  if (p == arg)
    error_no_arg ("environment variable to set");

  if (p == 0 || p[1] == 0)
    {
      nullset = 1;
      if (p == 0)
	p = arg + strlen (arg);	/* So that savestring below will work */
    }
  else
    {
      /* Not setting variable value to null */
      val = p + 1;
      while (*val == ' ' || *val == '\t')
	val++;
    }

  while (p != arg && (p[-1] == ' ' || p[-1] == '\t'))
    p--;

  var = savestring (arg, p - arg);
  if (nullset)
    {
      printf_filtered ("Setting environment variable ");
      printf_filtered ("\"%s\" to null value.\n", var);
      set_in_environ (inferior_environ, var, "");
    }
  else
    set_in_environ (inferior_environ, var, val);
  xfree (var);
}

static void
unset_environment_command (char *var, int from_tty)
{
  if (var == 0)
    {
      /* If there is no argument, delete all environment variables.
         Ask for confirmation if reading from the terminal.  */
      if (!from_tty || query ("Delete all environment variables? "))
	{
	  free_environ (inferior_environ);
	  inferior_environ = make_environ ();
	}
    }
  else
    unset_in_environ (inferior_environ, var);
}

/* Handle the execution path (PATH variable) */

static const char path_var_name[] = "PATH";

/* ARGSUSED */
static void
path_info (char *args, int from_tty)
{
  puts_filtered ("Executable and object file path: ");
  puts_filtered (get_in_environ (inferior_environ, path_var_name));
  puts_filtered ("\n");
}

/* Add zero or more directories to the front of the execution path.  */

static void
path_command (char *dirname, int from_tty)
{
  char *exec_path;
  char *env;
  dont_repeat ();
  env = get_in_environ (inferior_environ, path_var_name);
  /* Can be null if path is not set */
  if (!env)
    env = "";
  exec_path = xstrdup (env);
  mod_path (dirname, &exec_path);
  set_in_environ (inferior_environ, path_var_name, exec_path);
  xfree (exec_path);
  if (from_tty)
    path_info ((char *) NULL, from_tty);
}


#ifdef REGISTER_NAMES
char *gdb_register_names[] = REGISTER_NAMES;
#endif
/* Print out the machine register regnum. If regnum is -1,
   print all registers (fpregs == 1) or all non-float registers
   (fpregs == 0).

   For most machines, having all_registers_info() print the
   register(s) one per line is good enough. If a different format
   is required, (eg, for MIPS or Pyramid 90x, which both have
   lots of regs), or there is an existing convention for showing
   all the registers, define the macro DO_REGISTERS_INFO(regnum, fp)
   to provide that format.  */

void
do_registers_info (int regnum, int fpregs)
{
  register int i;
  int numregs = NUM_REGS + NUM_PSEUDO_REGS;
  char *raw_buffer = (char*) alloca (MAX_REGISTER_RAW_SIZE);
  char *virtual_buffer = (char*) alloca (MAX_REGISTER_VIRTUAL_SIZE);

  for (i = 0; i < numregs; i++)
    {
      /* Decide between printing all regs, nonfloat regs, or specific reg.  */
      if (regnum == -1)
	{
	  if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT && !fpregs)
	    continue;
	}
      else
	{
	  if (i != regnum)
	    continue;
	}

      /* If the register name is empty, it is undefined for this
         processor, so don't display anything.  */
      if (REGISTER_NAME (i) == NULL || *(REGISTER_NAME (i)) == '\0')
	continue;

      fputs_filtered (REGISTER_NAME (i), gdb_stdout);
      print_spaces_filtered (15 - strlen (REGISTER_NAME (i)), gdb_stdout);

      /* Get the data in raw format.  */
      if (read_relative_register_raw_bytes (i, raw_buffer))
	{
	  printf_filtered ("*value not available*\n");
	  continue;
	}

      /* Convert raw data to virtual format if necessary.  */
      if (REGISTER_CONVERTIBLE (i))
	{
	  REGISTER_CONVERT_TO_VIRTUAL (i, REGISTER_VIRTUAL_TYPE (i),
				       raw_buffer, virtual_buffer);
	}
      else
	{
	  memcpy (virtual_buffer, raw_buffer,
		  REGISTER_VIRTUAL_SIZE (i));
	}

      /* If virtual format is floating, print it that way, and in raw hex.  */
      if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT)
	{
	  register int j;

	  val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0,
		     gdb_stdout, 0, 1, 0, Val_pretty_default);

	  printf_filtered ("\t(raw 0x");
	  for (j = 0; j < REGISTER_RAW_SIZE (i); j++)
	    {
	      register int idx = TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? j
	      : REGISTER_RAW_SIZE (i) - 1 - j;
	      printf_filtered ("%02x", (unsigned char) raw_buffer[idx]);
	    }
	  printf_filtered (")");
	}
      /* Else print as integer in hex and in decimal.  */
      else
	{
	  val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0,
		     gdb_stdout, 'x', 1, 0, Val_pretty_default);
	  printf_filtered ("\t");
	  val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0, 0,
		     gdb_stdout, 0, 1, 0, Val_pretty_default);
	}

      /* The SPARC wants to print even-numbered float regs as doubles
         in addition to printing them as floats.  */
#ifdef PRINT_REGISTER_HOOK
      PRINT_REGISTER_HOOK (i);
#endif

      printf_filtered ("\n");
    }
}

void
registers_info (char *addr_exp, int fpregs)
{
  int regnum, numregs;
  register char *end;

  if (!target_has_registers)
    error ("The program has no registers now.");
  if (selected_frame == NULL)
    error ("No selected frame.");

  if (!addr_exp)
    {
      DO_REGISTERS_INFO (-1, fpregs);
      return;
    }

  do
    {
      if (addr_exp[0] == '$')
	addr_exp++;
      end = addr_exp;
      while (*end != '\0' && *end != ' ' && *end != '\t')
	++end;
      numregs = NUM_REGS + NUM_PSEUDO_REGS;

      regnum = target_map_name_to_register (addr_exp, end - addr_exp);
      if (regnum >= 0)
	goto found;

      regnum = numregs;

      if (*addr_exp >= '0' && *addr_exp <= '9')
	regnum = atoi (addr_exp);	/* Take a number */
      if (regnum >= numregs)	/* Bad name, or bad number */
	error ("%.*s: invalid register", end - addr_exp, addr_exp);

    found:
      DO_REGISTERS_INFO (regnum, fpregs);

      addr_exp = end;
      while (*addr_exp == ' ' || *addr_exp == '\t')
	++addr_exp;
    }
  while (*addr_exp != '\0');
}

void
all_registers_info (char *addr_exp, int from_tty)
{
  registers_info (addr_exp, 1);
}

void
nofp_registers_info (char *addr_exp, int from_tty)
{
  registers_info (addr_exp, 0);
}


/*
 * TODO:
 * Should save/restore the tty state since it might be that the
 * program to be debugged was started on this tty and it wants
 * the tty in some state other than what we want.  If it's running
 * on another terminal or without a terminal, then saving and
 * restoring the tty state is a harmless no-op.
 * This only needs to be done if we are attaching to a process.
 */

/*
   attach_command --
   takes a program started up outside of gdb and ``attaches'' to it.
   This stops it cold in its tracks and allows us to start debugging it.
   and wait for the trace-trap that results from attaching.  */

void
attach_command (char *args, int from_tty)
{
  char *exec_file;
  char *full_exec_path = NULL;

  dont_repeat ();		/* Not for the faint of heart */

  if (target_has_execution)
    {
      if (query ("A program is being debugged already.  Kill it? "))
	target_kill ();
      else
	error ("Not killed.");
    }

  target_attach (args, from_tty);

  /* Set up the "saved terminal modes" of the inferior
     based on what modes we are starting it with.  */
  target_terminal_init ();

  /* Install inferior's terminal modes.  */
  target_terminal_inferior ();

  /* Set up execution context to know that we should return from
     wait_for_inferior as soon as the target reports a stop.  */
  init_wait_for_inferior ();
  clear_proceed_status ();

  /* No traps are generated when attaching to inferior under Mach 3
     or GNU hurd.  */
#ifndef ATTACH_NO_WAIT
  stop_soon_quietly = 1;
  wait_for_inferior ();
#endif

  /*
   * If no exec file is yet known, try to determine it from the
   * process itself.
   */
  exec_file = (char *) get_exec_file (0);
  if (!exec_file)
    {
      exec_file = target_pid_to_exec_file (PIDGET (inferior_ptid));
      if (exec_file)
	{
	  /* It's possible we don't have a full path, but rather just a
	     filename.  Some targets, such as HP-UX, don't provide the
	     full path, sigh.

	     Attempt to qualify the filename against the source path.
	     (If that fails, we'll just fall back on the original
	     filename.  Not much more we can do...)
	   */
	  if (!source_full_path_of (exec_file, &full_exec_path))
	    full_exec_path = savestring (exec_file, strlen (exec_file));

	  exec_file_attach (full_exec_path, from_tty);
	  symbol_file_add_main (full_exec_path, from_tty);
	}
    }

#ifdef SOLIB_ADD
  /* Add shared library symbols from the newly attached process, if any.  */
  SOLIB_ADD ((char *) 0, from_tty, &current_target, auto_solib_add);
  re_enable_breakpoints_in_shlibs ();
#endif

  /* Take any necessary post-attaching actions for this platform.
   */
  target_post_attach (PIDGET (inferior_ptid));

  normal_stop ();

  if (attach_hook)
    attach_hook ();
}

/*
 * detach_command --
 * takes a program previously attached to and detaches it.
 * The program resumes execution and will no longer stop
 * on signals, etc.  We better not have left any breakpoints
 * in the program or it'll die when it hits one.  For this
 * to work, it may be necessary for the process to have been
 * previously attached.  It *might* work if the program was
 * started via the normal ptrace (PTRACE_TRACEME).
 */

static void
detach_command (char *args, int from_tty)
{
  dont_repeat ();		/* Not for the faint of heart */
  target_detach (args, from_tty);
#if defined(SOLIB_RESTART)
  SOLIB_RESTART ();
#endif
  if (detach_hook)
    detach_hook ();
}

/* Stop the execution of the target while running in async mode, in
   the backgound. */

void
interrupt_target_command_wrapper (char *args, int from_tty)
{
  interrupt_target_command (args, from_tty);
}

static void
interrupt_target_command (char *args, int from_tty)
{
  if (event_loop_p && target_can_async_p ())
    {
      dont_repeat ();		/* Not for the faint of heart */
      target_stop ();
    }
}

/* ARGSUSED */
static void
float_info (char *addr_exp, int from_tty)
{
  PRINT_FLOAT_INFO ();
}

/* ARGSUSED */
static void
unset_command (char *args, int from_tty)
{
  printf_filtered ("\"unset\" must be followed by the name of ");
  printf_filtered ("an unset subcommand.\n");
  help_list (unsetlist, "unset ", -1, gdb_stdout);
}

void
_initialize_infcmd (void)
{
  struct cmd_list_element *c;

  c = add_com ("tty", class_run, tty_command,
	       "Set terminal for future runs of program being debugged.");
  c->completer = filename_completer;

  c = add_set_cmd ("args", class_run, var_string_noescape,
		   (char *) &inferior_args,
		   "Set argument list to give program being debugged when it is started.\n\
Follow this command with any number of args, to be passed to the program.",
		   &setlist);
  c->completer = filename_completer;
  set_cmd_sfunc (c, notice_args_set);
  c = add_show_from_set (c, &showlist);
  set_cmd_sfunc (c, notice_args_read);

  c = add_cmd
    ("environment", no_class, environment_info,
     "The environment to give the program, or one variable's value.\n\
With an argument VAR, prints the value of environment variable VAR to\n\
give the program being debugged.  With no arguments, prints the entire\n\
environment to be given to the program.", &showlist);
  c->completer = noop_completer;

  add_prefix_cmd ("unset", no_class, unset_command,
		  "Complement to certain \"set\" commands",
		  &unsetlist, "unset ", 0, &cmdlist);

  c = add_cmd ("environment", class_run, unset_environment_command,
	       "Cancel environment variable VAR for the program.\n\
This does not affect the program until the next \"run\" command.",
	       &unsetlist);
  c->completer = noop_completer;

  c = add_cmd ("environment", class_run, set_environment_command,
	       "Set environment variable value to give the program.\n\
Arguments are VAR VALUE where VAR is variable name and VALUE is value.\n\
VALUES of environment variables are uninterpreted strings.\n\
This does not affect the program until the next \"run\" command.",
	       &setlist);
  c->completer = noop_completer;

  c = add_com ("path", class_files, path_command,
	       "Add directory DIR(s) to beginning of search path for object files.\n\
$cwd in the path means the current working directory.\n\
This path is equivalent to the $PATH shell variable.  It is a list of\n\
directories, separated by colons.  These directories are searched to find\n\
fully linked executable files and separately compiled object files as needed.");
  c->completer = filename_completer;

  c = add_cmd ("paths", no_class, path_info,
	       "Current search path for finding object files.\n\
$cwd in the path means the current working directory.\n\
This path is equivalent to the $PATH shell variable.  It is a list of\n\
directories, separated by colons.  These directories are searched to find\n\
fully linked executable files and separately compiled object files as needed.",
	       &showlist);
  c->completer = noop_completer;

  add_com ("attach", class_run, attach_command,
	   "Attach to a process or file outside of GDB.\n\
This command attaches to another target, of the same type as your last\n\
\"target\" command (\"info files\" will show your target stack).\n\
The command may take as argument a process id or a device file.\n\
For a process id, you must have permission to send the process a signal,\n\
and it must have the same effective uid as the debugger.\n\
When using \"attach\" with a process id, the debugger finds the\n\
program running in the process, looking first in the current working\n\
directory, or (if not found there) using the source file search path\n\
(see the \"directory\" command).  You can also use the \"file\" command\n\
to specify the program, and to load its symbol table.");

  add_com ("detach", class_run, detach_command,
	   "Detach a process or file previously attached.\n\
If a process, it is no longer traced, and it continues its execution.  If\n\
you were debugging a file, the file is closed and gdb no longer accesses it.");

  add_com ("signal", class_run, signal_command,
	   "Continue program giving it signal specified by the argument.\n\
An argument of \"0\" means continue program without giving it a signal.");

  add_com ("stepi", class_run, stepi_command,
	   "Step one instruction exactly.\n\
Argument N means do this N times (or till program stops for another reason).");
  add_com_alias ("si", "stepi", class_alias, 0);

  add_com ("nexti", class_run, nexti_command,
	   "Step one instruction, but proceed through subroutine calls.\n\
Argument N means do this N times (or till program stops for another reason).");
  add_com_alias ("ni", "nexti", class_alias, 0);

  add_com ("finish", class_run, finish_command,
	   "Execute until selected stack frame returns.\n\
Upon return, the value returned is printed and put in the value history.");

  add_com ("next", class_run, next_command,
	   "Step program, proceeding through subroutine calls.\n\
Like the \"step\" command as long as subroutine calls do not happen;\n\
when they do, the call is treated as one instruction.\n\
Argument N means do this N times (or till program stops for another reason).");
  add_com_alias ("n", "next", class_run, 1);
  if (xdb_commands)
    add_com_alias ("S", "next", class_run, 1);

  add_com ("step", class_run, step_command,
	   "Step program until it reaches a different source line.\n\
Argument N means do this N times (or till program stops for another reason).");
  add_com_alias ("s", "step", class_run, 1);

  c = add_com ("until", class_run, until_command,
	       "Execute until the program reaches a source line greater than the current\n\
or a specified line or address or function (same args as break command).\n\
Execution will also stop upon exit from the current stack frame.");
  c->completer = location_completer;
  add_com_alias ("u", "until", class_run, 1);

  c = add_com ("jump", class_run, jump_command,
	       "Continue program being debugged at specified line or address.\n\
Give as argument either LINENUM or *ADDR, where ADDR is an expression\n\
for an address to start at.");
  c->completer = location_completer;

  if (xdb_commands)
    {
      c = add_com ("go", class_run, go_command,
		   "Usage: go <location>\n\
Continue program being debugged, stopping at specified line or \n\
address.\n\
Give as argument either LINENUM or *ADDR, where ADDR is an \n\
expression for an address to start at.\n\
This command is a combination of tbreak and jump.");
      c->completer = location_completer;
    }

  if (xdb_commands)
    add_com_alias ("g", "go", class_run, 1);

  add_com ("continue", class_run, continue_command,
	   "Continue program being debugged, after signal or breakpoint.\n\
If proceeding from breakpoint, a number N may be used as an argument,\n\
which means to set the ignore count of that breakpoint to N - 1 (so that\n\
the breakpoint won't break until the Nth time it is reached).");
  add_com_alias ("c", "cont", class_run, 1);
  add_com_alias ("fg", "cont", class_run, 1);

  c = add_com ("run", class_run, run_command,
	   "Start debugged program.  You may specify arguments to give it.\n\
Args may include \"*\", or \"[...]\"; they are expanded using \"sh\".\n\
Input and output redirection with \">\", \"<\", or \">>\" are also allowed.\n\n\
With no arguments, uses arguments last specified (with \"run\" or \"set args\").\n\
To cancel previous arguments and run with no arguments,\n\
use \"set args\" without arguments.");
  c->completer = filename_completer;
  add_com_alias ("r", "run", class_run, 1);
  if (xdb_commands)
    add_com ("R", class_run, run_no_args_command,
	     "Start debugged program with no arguments.");

  add_com ("interrupt", class_run, interrupt_target_command,
	   "Interrupt the execution of the debugged program.");

  add_info ("registers", nofp_registers_info,
	    "List of integer registers and their contents, for selected stack frame.\n\
Register name as argument means describe only that register.");
  add_info_alias ("r", "registers", 1);

  if (xdb_commands)
    add_com ("lr", class_info, nofp_registers_info,
	     "List of integer registers and their contents, for selected stack frame.\n\
  Register name as argument means describe only that register.");
  add_info ("all-registers", all_registers_info,
	    "List of all registers and their contents, for selected stack frame.\n\
Register name as argument means describe only that register.");

  add_info ("program", program_info,
	    "Execution status of the program.");

  add_info ("float", float_info,
	    "Print the status of the floating point unit\n");

  inferior_environ = make_environ ();
  init_environ (inferior_environ);
}
