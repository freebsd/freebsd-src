/* Memory-access and commands for "inferior" (child) process, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include <signal.h>
#include <sys/param.h>
#include <string.h>
#include "symtab.h"
#include "gdbtypes.h"
#include "frame.h"
#include "inferior.h"
#include "environ.h"
#include "value.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "target.h"

static void
continue_command PARAMS ((char *, int));

static void
until_next_command PARAMS ((int));

static void 
until_command PARAMS ((char *, int));

static void
path_info PARAMS ((char *, int));

static void
path_command PARAMS ((char *, int));

static void
unset_command PARAMS ((char *, int));

static void
float_info PARAMS ((char *, int));

static void
detach_command PARAMS ((char *, int));

static void
nofp_registers_info PARAMS ((char *, int));

static void
all_registers_info PARAMS ((char *, int));

static void
registers_info PARAMS ((char *, int));

static void
do_registers_info PARAMS ((int, int));

static void
unset_environment_command PARAMS ((char *, int));

static void
set_environment_command PARAMS ((char *, int));

static void
environment_info PARAMS ((char *, int));

static void
program_info PARAMS ((char *, int));

static void
finish_command PARAMS ((char *, int));

static void
signal_command PARAMS ((char *, int));

static void
jump_command PARAMS ((char *, int));

static void
step_1 PARAMS ((int, int, char *));

static void
nexti_command PARAMS ((char *, int));

static void
stepi_command PARAMS ((char *, int));

static void
next_command PARAMS ((char *, int));

static void
step_command PARAMS ((char *, int));

static void
run_command PARAMS ((char *, int));

#define ERROR_NO_INFERIOR \
   if (!target_has_execution) error ("The program is not being run.");

/* String containing arguments to give to the program, separated by spaces.
   Empty string (pointer to '\0') means no args.  */

static char *inferior_args;

/* File name for default use for standard in/out in the inferior.  */

char *inferior_io_terminal;

/* Pid of our debugged inferior, or 0 if no inferior now.
   Since various parts of infrun.c test this to see whether there is a program
   being debugged it should be nonzero (currently 3 is used) for remote
   debugging.  */

int inferior_pid;

/* Last signal that the inferior received (why it stopped).  */

int stop_signal;

/* Address at which inferior stopped.  */

CORE_ADDR stop_pc;

/* Stack frame when program stopped.  */

FRAME_ADDR stop_frame_address;

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

CORE_ADDR step_range_start; /* Inclusive */
CORE_ADDR step_range_end; /* Exclusive */

/* Stack frame address as of when stepping command was issued.
   This is how we know when we step into a subroutine call,
   and how to set the frame for the breakpoint used to step out.  */

FRAME_ADDR step_frame_address;

/* 1 means step over all subroutine calls.
   0 means don't step over calls (used by stepi).
   -1 means step over calls to undebuggable functions.  */

int step_over_calls;

/* If stepping, nonzero means step count is > 1
   so don't print frame next time inferior stops
   if it stops due to stepping.  */

int step_multi;

/* Environment to use for running inferior,
   in format described in environ.h.  */

struct environ *inferior_environ;


/* ARGSUSED */
void
tty_command (file, from_tty)
     char *file;
     int from_tty;
{
  if (file == 0)
    error_no_arg ("terminal name for running target process");

  inferior_io_terminal = savestring (file, strlen (file));
}

static void
run_command (args, from_tty)
     char *args;
     int from_tty;
{
  char *exec_file;

  dont_repeat ();

  /* Shouldn't this be target_has_execution?  FIXME.  */
  if (inferior_pid)
    {
      if (
	  !query ("The program being debugged has been started already.\n\
Start it from the beginning? "))
	error ("Program not restarted.");
      target_kill ();
    }

  exec_file = (char *) get_exec_file (0);

  /* The exec file is re-read every time we do a generic_mourn_inferior, so
     we just have to worry about the symbol file.  */
  reread_symbols ();

  if (args)
    {
      char *cmd;
      cmd = concat ("set args ", args, NULL);
      make_cleanup (free, cmd);
      execute_command (cmd, from_tty);
    }

  if (from_tty)
    {
      puts_filtered("Starting program: ");
      if (exec_file)
	puts_filtered(exec_file);
      puts_filtered(" ");
      puts_filtered(inferior_args);
      puts_filtered("\n");
      fflush (stdout);
    }

  target_create_inferior (exec_file, inferior_args,
			  environ_vector (inferior_environ));
}

static void
continue_command (proc_count_exp, from_tty)
     char *proc_count_exp;
     int from_tty;
{
  ERROR_NO_INFERIOR;

  /* If have argument, set proceed count of breakpoint we stopped at.  */

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
			    parse_and_eval_address (proc_count_exp) - 1,
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

  proceed ((CORE_ADDR) -1, -1, 0);
}

/* Step until outside of current statement.  */

/* ARGSUSED */
static void
step_command (count_string, from_tty)
     char *count_string;
     int from_tty;
{
  step_1 (0, 0, count_string);
}

/* Likewise, but skip over subroutine calls as if single instructions.  */

/* ARGSUSED */
static void
next_command (count_string, from_tty)
     char *count_string;
     int from_tty;
{
  step_1 (1, 0, count_string);
}

/* Likewise, but step only one instruction.  */

/* ARGSUSED */
static void
stepi_command (count_string, from_tty)
     char *count_string;
     int from_tty;
{
  step_1 (0, 1, count_string);
}

/* ARGSUSED */
static void
nexti_command (count_string, from_tty)
     char *count_string;
     int from_tty;
{
  step_1 (1, 1, count_string);
}

static void
step_1 (skip_subroutines, single_inst, count_string)
     int skip_subroutines;
     int single_inst;
     char *count_string;
{
  register int count = 1;
  FRAME fr;
  struct cleanup *cleanups = 0;

  ERROR_NO_INFERIOR;
  count = count_string ? parse_and_eval_address (count_string) : 1;

  if (!single_inst || skip_subroutines) /* leave si command alone */
    {
      enable_longjmp_breakpoint();
      cleanups = make_cleanup(disable_longjmp_breakpoint, 0);
    }

  for (; count > 0; count--)
    {
      clear_proceed_status ();

      fr = get_current_frame ();
      if (!fr)				/* Avoid coredump here.  Why tho? */
	error ("No current frame");
      step_frame_address = FRAME_FP (fr);

      if (! single_inst)
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
	      fflush (stdout);
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
	    step_over_calls = 0;
	}

      if (skip_subroutines)
	step_over_calls = 1;

      step_multi = (count > 1);
      proceed ((CORE_ADDR) -1, -1, 1);
      if (! stop_step)
	break;

      /* FIXME: On nexti, this may have already been done (when we hit the
	 step resume break, I think).  Probably this should be moved to
	 wait_for_inferior (near the top).  */
#if defined (SHIFT_INST_REGS)
      SHIFT_INST_REGS();
#endif
    }

  if (!single_inst || skip_subroutines)
    do_cleanups(cleanups);
}

/* Continue program at specified address.  */

static void
jump_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register CORE_ADDR addr;
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  struct symbol *fn;
  struct symbol *sfn;

  ERROR_NO_INFERIOR;

  if (!arg)
    error_no_arg ("starting address");

  sals = decode_line_spec_1 (arg, 1);
  if (sals.nelts != 1)
    {
      error ("Unreasonable jump request");
    }

  sal = sals.sals[0];
  free ((PTR)sals.sals);

  if (sal.symtab == 0 && sal.pc == 0)
    error ("No source file has been specified.");

  resolve_sal_pc (&sal);			/* May error out */

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

  addr = sal.pc;

  if (from_tty)
    printf_filtered ("Continuing at %s.\n",
		     local_hex_string((unsigned long) addr));

  clear_proceed_status ();
  proceed (addr, 0, 0);
}

/* Continue program giving it specified signal.  */

static void
signal_command (signum_exp, from_tty)
     char *signum_exp;
     int from_tty;
{
  register int signum;

  dont_repeat ();		/* Too dangerous.  */
  ERROR_NO_INFERIOR;

  if (!signum_exp)
    error_no_arg ("signal number");

  /* It would be even slicker to make signal names be valid expressions,
     (the type could be "enum $signal" or some such), then the user could
     assign them to convenience variables.  */
  signum = strtosigno (signum_exp);

  if (signum == 0)
    /* Not found as a name, try it as an expression.  */
    signum = parse_and_eval_address (signum_exp);

  if (from_tty)
    {
      char *signame = strsigno (signum);
      printf_filtered ("Continuing with signal ");
      if (signame == NULL || signum == 0)
	printf_filtered ("%d.\n", signum);
      else
	/* Do we need to print the number as well as the name?  */
	printf_filtered ("%s (%d).\n", signame, signum);
    }

  clear_proceed_status ();
  proceed (stop_pc, signum, 0);
}

/* Call breakpoint_auto_delete on the current contents of the bpstat
   pointed to by arg (which is really a bpstat *).  */
void
breakpoint_auto_delete_contents (arg)
     PTR arg;
{
  breakpoint_auto_delete (*(bpstat *)arg);
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
   Otherwise, run_stack-dummy returns 1 (the frame will eventually be popped
   when we do hit that breakpoint).  */

/* DEBUG HOOK:  4 => return instead of letting the stack dummy run.  */

static int stack_dummy_testing = 0;

int
run_stack_dummy (addr, buffer)
     CORE_ADDR addr;
     char buffer[REGISTER_BYTES];
{
  struct cleanup *old_cleanups = make_cleanup (null_cleanup, 0);

  /* Now proceed, having reached the desired place.  */
  clear_proceed_status ();
  if (stack_dummy_testing & 4)
    {
      POP_FRAME;
      return(0);
    }
#ifdef CALL_DUMMY_BREAKPOINT_OFFSET
  {
    struct breakpoint *bpt;
    struct symtab_and_line sal;

#if CALL_DUMMY_LOCATION != AT_ENTRY_POINT
    sal.pc = addr - CALL_DUMMY_START_OFFSET + CALL_DUMMY_BREAKPOINT_OFFSET;
#else
    sal.pc = entry_point_address ();
#endif
    sal.symtab = NULL;
    sal.line = 0;

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
    bpt->disposition = delete;

    /* If all error()s out of proceed ended up calling normal_stop (and
       perhaps they should; it already does in the special case of error
       out of resume()), then we wouldn't need this.  */
    make_cleanup (breakpoint_auto_delete_contents, &stop_bpstat);
  }
#endif /* CALL_DUMMY_BREAKPOINT_OFFSET.  */

  proceed_to_finish = 1;	/* We want stop_registers, please... */
  proceed (addr, 0, 0);

  discard_cleanups (old_cleanups);

  if (!stop_stack_dummy)
    return 1;

  /* On return, the stack dummy has been popped already.  */

  memcpy (buffer, stop_registers, sizeof stop_registers);
  return 0;
}

/* Proceed until we reach a different source line with pc greater than
   our current one or exit the function.  We skip calls in both cases.

   Note that eventually this command should probably be changed so
   that only source lines are printed out when we hit the breakpoint
   we set.  I'm going to postpone this until after a hopeful rewrite
   of wait_for_inferior and the proceed status code. -- randy */

/* ARGSUSED */
static void
until_next_command (from_tty)
     int from_tty;
{
  FRAME frame;
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
  
  step_over_calls = 1;
  step_frame_address = FRAME_FP (frame);
  
  step_multi = 0;		/* Only one call to proceed */
  
  proceed ((CORE_ADDR) -1, -1, 1);
}

static void 
until_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (!target_has_execution)
    error ("The program is not running.");
  if (arg)
    until_break_command (arg, from_tty);
  else
    until_next_command (from_tty);
}

/* "finish": Set a temporary breakpoint at the place
   the selected frame will return to, then continue.  */

static void
finish_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct symtab_and_line sal;
  register FRAME frame;
  struct frame_info *fi;
  register struct symbol *function;
  struct breakpoint *breakpoint;
  struct cleanup *old_chain;

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

  fi = get_frame_info (frame);
  sal = find_pc_line (fi->pc, 0);
  sal.pc = fi->pc;

  breakpoint = set_momentary_breakpoint (sal, frame, bp_finish);

  old_chain = make_cleanup(delete_breakpoint, breakpoint);

  /* Find the function we will return from.  */

  fi = get_frame_info (selected_frame);
  function = find_pc_function (fi->pc);

  /* Print info on the selected frame, including level number
     but not source.  */
  if (from_tty)
    {
      printf_filtered ("Run till exit from ");
      print_stack_frame (selected_frame, selected_frame_level, 0);
    }

  proceed_to_finish = 1;		/* We want stop_registers, please... */
  proceed ((CORE_ADDR) -1, -1, 0);

  /* Did we stop at our breakpoint? */
  if (bpstat_find_breakpoint(stop_bpstat, breakpoint) != NULL
      && function != 0)
    {
      struct type *value_type;
      register value val;
      CORE_ADDR funcaddr;

      value_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (function));
      if (!value_type)
	fatal ("internal: finish_command: function has no target type");
      
      if (TYPE_CODE (value_type) == TYPE_CODE_VOID)
	return;

      funcaddr = BLOCK_START (SYMBOL_BLOCK_VALUE (function));

      val = value_being_returned (value_type, stop_registers,
	      using_struct_return (value_of_variable (function, NULL),
				   funcaddr,
				   value_type,
		BLOCK_GCC_COMPILED (SYMBOL_BLOCK_VALUE (function))));

      printf_filtered ("Value returned is $%d = ", record_latest_value (val));
      value_print (val, stdout, 0, Val_no_prettyprint);
      printf_filtered ("\n");
    }
  do_cleanups(old_chain);
}

/* ARGSUSED */
static void
program_info (args, from_tty)
    char *args;
    int from_tty;
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
		   local_hex_string((unsigned long) stop_pc));
  if (stop_step)
    printf_filtered ("It stopped after being stepped.\n");
  else if (num != 0)
    {
      /* There may be several breakpoints in the same place, so this
	 isn't as strange as it seems.  */
      while (num != 0)
	{
	  if (num < 0)
	    printf_filtered ("It stopped at a breakpoint that has since been deleted.\n");
	  else
	    printf_filtered ("It stopped at breakpoint %d.\n", num);
	  num = bpstat_num (&bs);
	}
    }
  else if (stop_signal)
    {
#ifdef PRINT_RANDOM_SIGNAL
      PRINT_RANDOM_SIGNAL (stop_signal);
#else
      char *signame = strsigno (stop_signal);
      printf_filtered ("It stopped with signal ");
      if (signame == NULL)
	printf_filtered ("%d", stop_signal);
      else
	/* Do we need to print the number as well as the name?  */
	printf_filtered ("%s (%d)", signame, stop_signal);
      printf_filtered (", %s.\n", safe_strsignal (stop_signal));
#endif
  }

  if (!from_tty)
    printf_filtered ("Type \"info stack\" or \"info registers\" for more information.\n");
}

static void
environment_info (var, from_tty)
     char *var;
     int from_tty;
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
set_environment_command (arg, from_tty)
     char *arg;
     int from_tty;
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

  while (p != arg && (p[-1] == ' ' || p[-1] == '\t')) p--;

  var = savestring (arg, p - arg);
  if (nullset)
    {
      printf_filtered ("Setting environment variable \"%s\" to null value.\n", var);
      set_in_environ (inferior_environ, var, "");
    }
  else
    set_in_environ (inferior_environ, var, val);
  free (var);
}

static void
unset_environment_command (var, from_tty)
     char *var;
     int from_tty;
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
path_info (args, from_tty)
     char *args;
     int from_tty;
{
  puts_filtered ("Executable and object file path: ");
  puts_filtered (get_in_environ (inferior_environ, path_var_name));
  puts_filtered ("\n");
}

/* Add zero or more directories to the front of the execution path.  */

static void
path_command (dirname, from_tty)
     char *dirname;
     int from_tty;
{
  char *exec_path;

  dont_repeat ();
  exec_path = strsave (get_in_environ (inferior_environ, path_var_name));
  mod_path (dirname, &exec_path);
  set_in_environ (inferior_environ, path_var_name, exec_path);
  free (exec_path);
  if (from_tty)
    path_info ((char *)NULL, from_tty);
}

/* This routine is getting awfully cluttered with #if's.  It's probably
   time to turn this into READ_PC and define it in the tm.h file.
   Ditto for write_pc.  */

CORE_ADDR
read_pc ()
{
#ifdef TARGET_READ_PC
  return TARGET_READ_PC ();
#else
  return ADDR_BITS_REMOVE ((CORE_ADDR) read_register (PC_REGNUM));
#endif
}

void
write_pc (val)
     CORE_ADDR val;
{
#ifdef TARGET_WRITE_PC
  TARGET_WRITE_PC (val);
#else
  write_register (PC_REGNUM, (long) val);
#ifdef NPC_REGNUM
  write_register (NPC_REGNUM, (long) val + 4);
#ifdef NNPC_REGNUM
  write_register (NNPC_REGNUM, (long) val + 8);
#endif
#endif
#endif
}

/* Cope with strage ways of getting to the stack and frame pointers */

CORE_ADDR
read_sp ()
{
#ifdef TARGET_READ_SP
  return TARGET_READ_SP ();
#else
  return read_register (SP_REGNUM);
#endif
}

void
write_sp (val)
     CORE_ADDR val;
{
#ifdef TARGET_WRITE_SP
  TARGET_WRITE_SP (val);
#else
  write_register (SP_REGNUM, val);
#endif
}


CORE_ADDR
read_fp ()
{
#ifdef TARGET_READ_FP
  return TARGET_READ_FP ();
#else
  return read_register (FP_REGNUM);
#endif
}

void
write_fp (val)
     CORE_ADDR val;
{
#ifdef TARGET_WRITE_FP
  TARGET_WRITE_FP (val);
#else
  write_register (FP_REGNUM, val);
#endif
}

const char * const reg_names[] = REGISTER_NAMES;

/* Print out the machine register regnum. If regnum is -1,
   print all registers (fpregs == 1) or all non-float registers
   (fpregs == 0).

   For most machines, having all_registers_info() print the
   register(s) one per line is good enough. If a different format
   is required, (eg, for MIPS or Pyramid 90x, which both have
   lots of regs), or there is an existing convention for showing
   all the registers, define the macro DO_REGISTERS_INFO(regnum, fp)
   to provide that format.  */  

#if !defined (DO_REGISTERS_INFO)
#define DO_REGISTERS_INFO(regnum, fp) do_registers_info(regnum, fp)
static void
do_registers_info (regnum, fpregs)
     int regnum;
     int fpregs;
{
  register int i;

  for (i = 0; i < NUM_REGS; i++)
    {
      char raw_buffer[MAX_REGISTER_RAW_SIZE];
      char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];

      /* Decide between printing all regs, nonfloat regs, or specific reg.  */
      if (regnum == -1) {
	if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT && !fpregs)
	  continue;
      } else {
        if (i != regnum)
	  continue;
      }

      fputs_filtered (reg_names[i], stdout);
      print_spaces_filtered (15 - strlen (reg_names[i]), stdout);

      /* Get the data in raw format, then convert also to virtual format.  */
      if (read_relative_register_raw_bytes (i, raw_buffer))
	{
	  printf_filtered ("Invalid register contents\n");
	  continue;
	}
      
      REGISTER_CONVERT_TO_VIRTUAL (i, raw_buffer, virtual_buffer);

      /* If virtual format is floating, print it that way, and in raw hex.  */
      if (TYPE_CODE (REGISTER_VIRTUAL_TYPE (i)) == TYPE_CODE_FLT
	  && ! INVALID_FLOAT (virtual_buffer, REGISTER_VIRTUAL_SIZE (i)))
	{
	  register int j;

	  val_print (REGISTER_VIRTUAL_TYPE (i), virtual_buffer, 0,
		     stdout, 0, 1, 0, Val_pretty_default);

	  printf_filtered ("\t(raw 0x");
	  for (j = 0; j < REGISTER_RAW_SIZE (i); j++)
	    printf_filtered ("%02x", (unsigned char)raw_buffer[j]);
	  printf_filtered (")");
	}

/* FIXME!  val_print probably can handle all of these cases now...  */

      /* Else if virtual format is too long for printf,
	 print in hex a byte at a time.  */
      else if (REGISTER_VIRTUAL_SIZE (i) > sizeof (long))
	{
	  register int j;
	  printf_filtered ("0x");
	  for (j = 0; j < REGISTER_VIRTUAL_SIZE (i); j++)
	    printf_filtered ("%02x", (unsigned char)virtual_buffer[j]);
	}
      /* Else print as integer in hex and in decimal.  */
      else
	{
	  val_print (REGISTER_VIRTUAL_TYPE (i), raw_buffer, 0,
		     stdout, 'x', 1, 0, Val_pretty_default);
	  printf_filtered ("\t");
	  val_print (REGISTER_VIRTUAL_TYPE (i), raw_buffer, 0,
		     stdout,   0, 1, 0, Val_pretty_default);
	}

      /* The SPARC wants to print even-numbered float regs as doubles
	 in addition to printing them as floats.  */
#ifdef PRINT_REGISTER_HOOK
      PRINT_REGISTER_HOOK (i);
#endif

      printf_filtered ("\n");
    }
}
#endif /* no DO_REGISTERS_INFO.  */

static void
registers_info (addr_exp, fpregs)
     char *addr_exp;
     int fpregs;
{
  int regnum;
  register char *end;

  if (!target_has_registers)
    error ("The program has no registers now.");

  if (!addr_exp)
    {
      DO_REGISTERS_INFO(-1, fpregs);
      return;
    }

  do
    {      
      if (addr_exp[0] == '$')
	addr_exp++;
      end = addr_exp;
      while (*end != '\0' && *end != ' ' && *end != '\t')
	++end;
      for (regnum = 0; regnum < NUM_REGS; regnum++)
	if (!strncmp (addr_exp, reg_names[regnum], end - addr_exp)
	    && strlen (reg_names[regnum]) == end - addr_exp)
	  goto found;
      if (*addr_exp >= '0' && *addr_exp <= '9')
	regnum = atoi (addr_exp);		/* Take a number */
      if (regnum >= NUM_REGS)		/* Bad name, or bad number */
	error ("%.*s: invalid register", end - addr_exp, addr_exp);

found:
      DO_REGISTERS_INFO(regnum, fpregs);

      addr_exp = end;
      while (*addr_exp == ' ' || *addr_exp == '\t')
	++addr_exp;
    } while (*addr_exp != '\0');
}

static void
all_registers_info (addr_exp, from_tty)
     char *addr_exp;
     int from_tty;
{
  registers_info (addr_exp, 1);
}

static void
nofp_registers_info (addr_exp, from_tty)
     char *addr_exp;
     int from_tty;
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
attach_command (args, from_tty)
     char *args;
     int from_tty;
{
  dont_repeat ();			/* Not for the faint of heart */

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
  stop_soon_quietly = 1;

  wait_for_inferior ();

#ifdef SOLIB_ADD
  /* Add shared library symbols from the newly attached process, if any.  */
  SOLIB_ADD ((char *)0, from_tty, (struct target_ops *)0);
#endif

  normal_stop ();
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
detach_command (args, from_tty)
     char *args;
     int from_tty;
{
  dont_repeat ();			/* Not for the faint of heart */
  target_detach (args, from_tty);
}

/* ARGSUSED */
static void
float_info (addr_exp, from_tty)
     char *addr_exp;
     int from_tty;
{
#ifdef FLOAT_INFO
	FLOAT_INFO;
#else
	printf_filtered ("No floating point info available for this processor.\n");
#endif
}

/* ARGSUSED */
static void
unset_command (args, from_tty)
     char *args;
     int from_tty;
{
  printf_filtered ("\"unset\" must be followed by the name of an unset subcommand.\n");
  help_list (unsetlist, "unset ", -1, stdout);
}

void
_initialize_infcmd ()
{
  struct cmd_list_element *c;
  
  add_com ("tty", class_run, tty_command,
	   "Set terminal for future runs of program being debugged.");

  add_show_from_set
    (add_set_cmd ("args", class_run, var_string_noescape, (char *)&inferior_args,
		  
"Set arguments to give program being debugged when it is started.\n\
Follow this command with any number of args, to be passed to the program.",
		  &setlist),
     &showlist);

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
 
  add_com ("path", class_files, path_command,
       "Add directory DIR(s) to beginning of search path for object files.\n\
$cwd in the path means the current working directory.\n\
This path is equivalent to the $PATH shell variable.  It is a list of\n\
directories, separated by colons.  These directories are searched to find\n\
fully linked executable files and separately compiled object files as needed.");

  c = add_cmd ("paths", no_class, path_info,
	    "Current search path for finding object files.\n\
$cwd in the path means the current working directory.\n\
This path is equivalent to the $PATH shell variable.  It is a list of\n\
directories, separated by colons.  These directories are searched to find\n\
fully linked executable files and separately compiled object files as needed.", &showlist);
  c->completer = noop_completer;

 add_com ("attach", class_run, attach_command,
 	   "Attach to a process or file outside of GDB.\n\
This command attaches to another target, of the same type as your last\n\
`target' command (`info files' will show your target stack).\n\
The command may take as argument a process id or a device file.\n\
For a process id, you must have permission to send the process a signal,\n\
and it must have the same effective uid as the debugger.\n\
When using \"attach\", you should use the \"file\" command to specify\n\
the program running in the process, and to load its symbol table.");

  add_com ("detach", class_run, detach_command,
	   "Detach a process or file previously attached.\n\
If a process, it is no longer traced, and it continues its execution.  If you\n\
were debugging a file, the file is closed and gdb no longer accesses it.");

  add_com ("signal", class_run, signal_command,
	   "Continue program giving it signal number SIGNUMBER.");

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

  add_com ("step", class_run, step_command,
	   "Step program until it reaches a different source line.\n\
Argument N means do this N times (or till program stops for another reason).");
  add_com_alias ("s", "step", class_run, 1);

  add_com ("until", class_run, until_command,
	   "Execute until the program reaches a source line greater than the current\n\
or a specified line or address or function (same args as break command).\n\
Execution will also stop upon exit from the current stack frame.");
  add_com_alias ("u", "until", class_run, 1);
  
  add_com ("jump", class_run, jump_command,
	   "Continue program being debugged at specified line or address.\n\
Give as argument either LINENUM or *ADDR, where ADDR is an expression\n\
for an address to start at.");

  add_com ("continue", class_run, continue_command,
	   "Continue program being debugged, after signal or breakpoint.\n\
If proceeding from breakpoint, a number N may be used as an argument,\n\
which means to set the ignore count of that breakpoint to N - 1 (so that\n\
the breakpoint won't break until the Nth time it is reached).");
  add_com_alias ("c", "cont", class_run, 1);
  add_com_alias ("fg", "cont", class_run, 1);

  add_com ("run", class_run, run_command,
	   "Start debugged program.  You may specify arguments to give it.\n\
Args may include \"*\", or \"[...]\"; they are expanded using \"sh\".\n\
Input and output redirection with \">\", \"<\", or \">>\" are also allowed.\n\n\
With no arguments, uses arguments last specified (with \"run\" or \"set args\").\n\
To cancel previous arguments and run with no arguments,\n\
use \"set args\" without arguments.");
  add_com_alias ("r", "run", class_run, 1);

  add_info ("registers", nofp_registers_info,
    "List of integer registers and their contents, for selected stack frame.\n\
Register name as argument means describe only that register.");

  add_info ("all-registers", all_registers_info,
"List of all registers and their contents, for selected stack frame.\n\
Register name as argument means describe only that register.");

  add_info ("program", program_info,
	    "Execution status of the program.");

  add_info ("float", float_info,
	    "Print the status of the floating point unit\n");

  inferior_args = savestring ("", 1);	/* Initially no args */
  inferior_environ = make_environ ();
  init_environ (inferior_environ);
}
