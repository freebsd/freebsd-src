/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)breakpoint.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/* Everything about breakpoints, for GDB.
   Copyright (C) 1986, 1987, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "frame.h"

/* This is the sequence of bytes we insert for a breakpoint.  */

static char break_insn[] = BREAKPOINT;

/* States of enablement of breakpoint.
   `temporary' means disable when hit.
   `delete' means delete when hit.  */

enum enable { disabled, enabled, temporary, delete};

/* Not that the ->silent field is not currently used by any commands
   (though the code is in there if it was to be and set_raw_breakpoint
   does set it to 0).  I implemented it because I thought it would be
   useful for a hack I had to put in; I'm going to leave it in because
   I can see how there might be times when it would indeed be useful */

struct breakpoint
{
  struct breakpoint *next;
  /* Number assigned to distinguish breakpoints.  */
  int number;
  /* Address to break at.  */
  CORE_ADDR address;
  /* Line number of this address.  Redundant.  */
  int line_number;
  /* Symtab of file of this address.  Redundant.  */
  struct symtab *symtab;
  /* Zero means disabled; remember the info but don't break here.  */
  enum enable enable;
  /* Non-zero means a silent breakpoint (don't print frame info
     if we stop here). */
  unsigned char silent;
  /* Number of stops at this breakpoint that should
     be continued automatically before really stopping.  */
  int ignore_count;
  /* "Real" contents of byte where breakpoint has been inserted.
     Valid only when breakpoints are in the program.  */
  char shadow_contents[sizeof break_insn];
  /* Nonzero if this breakpoint is now inserted.  */
  char inserted;
  /* Nonzero if this is not the first breakpoint in the list
     for the given address.  */
  char duplicate;
  /* Chain of command lines to execute when this breakpoint is hit.  */
  struct command_line *commands;
  /* Stack depth (address of frame).  If nonzero, break only if fp
     equals this.  */
  FRAME_ADDR frame;
  /* Conditional.  Break only if this expression's value is nonzero.  */
  struct expression *cond;
};

#define ALL_BREAKPOINTS(b)  for (b = breakpoint_chain; b; b = b->next)

/* Chain of all breakpoints defined.  */

struct breakpoint *breakpoint_chain;

/* Number of last breakpoint made.  */

static int breakpoint_count;

/* Default address, symtab and line to put a breakpoint at
   for "break" command with no arg.
   if default_breakpoint_valid is zero, the other three are
   not valid, and "break" with no arg is an error.

   This set by print_stack_frame, which calls set_default_breakpoint.  */

int default_breakpoint_valid;
CORE_ADDR default_breakpoint_address;
struct symtab *default_breakpoint_symtab;
int default_breakpoint_line;

/* Remaining commands (not yet executed)
   of last breakpoint hit.  */

struct command_line *breakpoint_commands;

static void delete_breakpoint ();
void clear_momentary_breakpoints ();
void breakpoint_auto_delete ();

/* Flag indicating extra verbosity for xgdb.  */
extern int xgdb_verbose;

/* condition N EXP -- set break condition of breakpoint N to EXP.  */

static void
condition_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register struct breakpoint *b;
  register char *p;
  register int bnum;
  register struct expression *expr;

  if (arg == 0)
    error_no_arg ("breakpoint number");

  p = arg;
  while (*p >= '0' && *p <= '9') p++;
  if (p == arg)
    /* There is no number here.  (e.g. "cond a == b").  */
    error_no_arg ("breakpoint number");
  bnum = atoi (arg);

  ALL_BREAKPOINTS (b)
    if (b->number == bnum)
      {
	if (b->cond)
	  {
	    free (b->cond);
	    b->cond = 0;  /* parse_c_1 can leave this unchanged. */
	  }
	if (*p == 0)
	  {
	    b->cond = 0;
	    if (from_tty)
	      printf ("Breakpoint %d now unconditional.\n", bnum);
	  }
	else
	  {
	    if (*p != ' ' && *p != '\t')
	      error ("Arguments must be an integer (breakpoint number) and an expression.");

	    /* Find start of expression */
	    while (*p == ' ' || *p == '\t') p++;

	    arg = p;
	    b->cond = (struct expression *) parse_c_1 (&arg, block_for_pc (b->address), 0);
	    if (*arg)
	      error ("Junk at end of expression");
	  }
	return;
      }

  error ("No breakpoint number %d.", bnum);
}

static void
commands_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register struct breakpoint *b;
  register char *p, *p1;
  register int bnum;
  struct command_line *l;

  /* If we allowed this, we would have problems with when to
     free the storage, if we change the commands currently
     being read from.  */

  if (breakpoint_commands)
    error ("Can't use the \"commands\" command among a breakpoint's commands.");

  /* Allow commands by itself to refer to the last breakpoint.  */
  if (arg == 0)
    bnum = breakpoint_count;
  else
    {
      p = arg;
      if (! (*p >= '0' && *p <= '9'))
	error ("Argument must be integer (a breakpoint number).");
      
      while (*p >= '0' && *p <= '9') p++;
      if (*p)
	error ("Unexpected extra arguments following breakpoint number.");
      
      bnum = atoi (arg);
    }

  ALL_BREAKPOINTS (b)
    if (b->number == bnum)
      {
	if (from_tty && input_from_terminal_p ())
	  {
	    printf ("Type commands for when breakpoint %d is hit, one per line.\n\
End with a line saying just \"end\".\n", bnum);
	    fflush (stdout);
	  }
	l = read_command_lines (from_tty);
	free_command_lines (b->commands);
	b->commands = l;
	return;
      }
  error ("No breakpoint number %d.", bnum);
}

/* Called from command loop to execute the commands
   associated with the breakpoint we just stopped at.  */

void
do_breakpoint_commands ()
{
  struct command_line *cmd;

  while (cmd = breakpoint_commands)
    {
      breakpoint_commands = 0;
      execute_command_lines(cmd);
      /* If command was "cont", breakpoint_commands is now 0,
	 of if we stopped at yet another breakpoint which has commands,
	 it is now the commands for the new breakpoint.  */
    }
  clear_momentary_breakpoints ();
}

/* Used when the program is proceeded, to eliminate any remaining
   commands attached to the previous breakpoint we stopped at.  */

void
clear_breakpoint_commands ()
{
  breakpoint_commands = 0;
  breakpoint_auto_delete (0);
}

/* Functions to get and set the current list of pending
   breakpoint commands.  These are used by run_stack_dummy
   to preserve the commands around a function call.  */

struct command_line *
get_breakpoint_commands ()
{
  return breakpoint_commands;
}

void
set_breakpoint_commands (cmds)
     struct command_line *cmds;
{
  breakpoint_commands = cmds;
}

/* insert_breakpoints is used when starting or continuing the program.
   remove_breakpoints is used when the program stops.
   Both return zero if successful,
   or an `errno' value if could not write the inferior.  */

int
insert_breakpoints ()
{
  register struct breakpoint *b;
  int val;

#ifdef BREAKPOINT_DEBUG
  printf ("Inserting breakpoints.\n");
#endif /* BREAKPOINT_DEBUG */

  ALL_BREAKPOINTS (b)
    if (b->enable != disabled && ! b->inserted && ! b->duplicate)
      {
	read_memory (b->address, b->shadow_contents, sizeof break_insn);
	val = write_memory (b->address, break_insn, sizeof break_insn);
	if (val)
	  return val;
#ifdef BREAKPOINT_DEBUG
	printf ("Inserted breakpoint at 0x%x, shadow 0x%x, 0x%x.\n",
		b->address, b->shadow_contents[0], b->shadow_contents[1]);
#endif /* BREAKPOINT_DEBUG */
	b->inserted = 1;
      }
  return 0;
}

int
remove_breakpoints ()
{
  register struct breakpoint *b;
  int val;

#ifdef BREAKPOINT_DEBUG
  printf ("Removing breakpoints.\n");
#endif /* BREAKPOINT_DEBUG */

  ALL_BREAKPOINTS (b)
    if (b->inserted)
      {
	val = write_memory (b->address, b->shadow_contents, sizeof break_insn);
	if (val)
	  return val;
	b->inserted = 0;
#ifdef BREAKPOINT_DEBUG
	printf ("Removed breakpoint at 0x%x, shadow 0x%x, 0x%x.\n",
		b->address, b->shadow_contents[0], b->shadow_contents[1]);
#endif /* BREAKPOINT_DEBUG */
      }

  return 0;
}

/* Clear the "inserted" flag in all breakpoints.
   This is done when the inferior is loaded.  */

void
mark_breakpoints_out ()
{
  register struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    b->inserted = 0;
}

/* breakpoint_here_p (PC) returns 1 if an enabled breakpoint exists at PC.
   When continuing from a location with a breakpoint,
   we actually single step once before calling insert_breakpoints.  */

int
breakpoint_here_p (pc)
     CORE_ADDR pc;
{
  register struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->enable != disabled && b->address == pc)
      return 1;

  return 0;
}

/* Evaluate the expression EXP and return 1 if value is zero.
   This is used inside a catch_errors to evaluate the breakpoint condition.  */

int
breakpoint_cond_eval (exp)
     struct expression *exp;
{
  return value_zerop (evaluate_expression (exp));
}

/* Return 0 if PC is not the address just after a breakpoint,
   or -1 if breakpoint says do not stop now,
   or -2 if breakpoint says it has deleted itself and don't stop,
   or -3 if hit a breakpoint number -3 (delete when program stops),
   or else the number of the breakpoint,
   with 0x1000000 added (or subtracted, for a negative return value) for
   a silent breakpoint.  */

int
breakpoint_stop_status (pc, frame_address)
     CORE_ADDR pc;
     FRAME_ADDR frame_address;
{
  register struct breakpoint *b;
  register int cont = 0;

  /* Get the address where the breakpoint would have been.  */
  pc -= DECR_PC_AFTER_BREAK;

  ALL_BREAKPOINTS (b)
    if (b->enable != disabled && b->address == pc)
      {
	if (b->frame && b->frame != frame_address)
	  cont = -1;
	else
	  {
	    int value_zero;
	    if (b->cond)
	      {
		/* Need to select the frame, with all that implies
		   so that the conditions will have the right context.  */
		select_frame (get_current_frame (), 0);
		value_zero
		  = catch_errors (breakpoint_cond_eval, b->cond,
				  "Error occurred in testing breakpoint condition.");
		free_all_values ();
	      }
	    if (b->cond && value_zero)
	      {
		cont = -1;
	      }
	    else if (b->ignore_count > 0)
	      {
		b->ignore_count--;
		cont = -1;
	      }
	    else
	      {
		if (b->enable == temporary)
		  b->enable = disabled;
		breakpoint_commands = b->commands;
		if (b->silent
		    || (breakpoint_commands
			&& !strcmp ("silent", breakpoint_commands->line)))
		  {
		    if (breakpoint_commands)
		      breakpoint_commands = breakpoint_commands->next;
		    return (b->number > 0 ?
			    0x1000000 + b->number :
			    b->number - 0x1000000);
		  }
		return b->number;
	      }
	  }
      }

  return cont;
}

static void
breakpoint_1 (bnum)
     int bnum;
{
  register struct breakpoint *b;
  register struct command_line *l;
  register struct symbol *sym;
  CORE_ADDR last_addr = (CORE_ADDR)-1;

  ALL_BREAKPOINTS (b)
    if (bnum == -1 || bnum == b->number)
      {
	printf_filtered ("#%-3d %c  0x%08x", b->number,
		"nyod"[(int) b->enable],
		b->address);
	last_addr = b->address;
	if (b->symtab)
	  {
	    sym = find_pc_function (b->address);
	    if (sym)
	      {
		fputs_filtered (" in ", stdout);
		fputs_demangled (SYMBOL_NAME (sym), stdout, 1);
		fputs_filtered (" (", stdout);
	      }
	    fputs_filtered (b->symtab->filename, stdout);
	    printf_filtered (" line %d", b->line_number);
	    if (sym) fputs_filtered(")", stdout);
	  }
	else
	      print_address_symbolic (b->address, stdout);
	      
	printf_filtered ("\n");

	if (b->ignore_count)
	  printf_filtered ("\tignore next %d hits\n", b->ignore_count);
	if (b->frame)
	  printf_filtered ("\tstop only in stack frame at 0x%x\n", b->frame);
	if (b->cond)
	  {
	    printf_filtered ("\tbreak only if ");
	    print_expression (b->cond, stdout);
	    printf_filtered ("\n");
	  }
	if (l = b->commands)
	  while (l)
	    {
	      printf_filtered ("\t%s\n", l->line);
	      l = l->next;
	    }
      }

  /* Compare against (CORE_ADDR)-1 in case some compiler decides
     that a comparison of an unsigned with -1 is always false.  */
  if (last_addr != (CORE_ADDR)-1)
    set_next_address (last_addr);
}

static void
breakpoints_info (bnum_exp)
     char *bnum_exp;
{
  int bnum = -1;

  if (bnum_exp)
    bnum = parse_and_eval_address (bnum_exp);
  else if (breakpoint_chain == 0)
    printf_filtered ("No breakpoints.\n");
  else
    printf_filtered ("Breakpoints:\n\
Num Enb   Address    Where\n");

  breakpoint_1 (bnum);
}

/* Print a message describing any breakpoints set at PC.  */

static void
describe_other_breakpoints (pc)
     register CORE_ADDR pc;
{
  register int others = 0;
  register struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->address == pc)
      others++;
  if (others > 0)
    {
      printf ("Note: breakpoint%s ", (others > 1) ? "s" : "");
      ALL_BREAKPOINTS (b)
	if (b->address == pc)
	  {
	    others--;
	    printf ("%d%s%s ",
		    b->number,
		    (b->enable == disabled) ? " (disabled)" : "",
		    (others > 1) ? "," : ((others == 1) ? " and" : ""));
	  }
      printf ("also set at pc 0x%x.\n", pc);
    }
}

/* Set the default place to put a breakpoint
   for the `break' command with no arguments.  */

void
set_default_breakpoint (valid, addr, symtab, line)
     int valid;
     CORE_ADDR addr;
     struct symtab *symtab;
     int line;
{
  default_breakpoint_valid = valid;
  default_breakpoint_address = addr;
  default_breakpoint_symtab = symtab;
  default_breakpoint_line = line;
}

/* Rescan breakpoints at address ADDRESS,
   marking the first one as "first" and any others as "duplicates".
   This is so that the bpt instruction is only inserted once.  */

static void
check_duplicates (address)
     CORE_ADDR address;
{
  register struct breakpoint *b;
  register int count = 0;

  ALL_BREAKPOINTS (b)
    if (b->enable != disabled && b->address == address)
      {
	count++;
	b->duplicate = count > 1;
      }
}

/* Low level routine to set a breakpoint.
   Takes as args the three things that every breakpoint must have.
   Returns the breakpoint object so caller can set other things.
   Does not set the breakpoint number!
   Does not print anything.  */

static struct breakpoint *
set_raw_breakpoint (sal)
     struct symtab_and_line sal;
{
  register struct breakpoint *b, *b1;

  b = (struct breakpoint *) xmalloc (sizeof (struct breakpoint));
  bzero (b, sizeof *b);
  b->address = sal.pc;
  b->symtab = sal.symtab;
  b->line_number = sal.line;
  b->enable = enabled;
  b->next = 0;
  b->silent = 0;

  /* Add this breakpoint to the end of the chain
     so that a list of breakpoints will come out in order
     of increasing numbers.  */

  b1 = breakpoint_chain;
  if (b1 == 0)
    breakpoint_chain = b;
  else
    {
      while (b1->next)
	b1 = b1->next;
      b1->next = b;
    }

  check_duplicates (sal.pc);

  return b;
}

/* Set a breakpoint that will evaporate an end of command
   at address specified by SAL.
   Restrict it to frame FRAME if FRAME is nonzero.  */

void
set_momentary_breakpoint (sal, frame)
     struct symtab_and_line sal;
     FRAME frame;
{
  register struct breakpoint *b;
  b = set_raw_breakpoint (sal);
  b->number = -3;
  b->enable = delete;
  b->frame = (frame ? FRAME_FP (frame) : 0);
}

void
clear_momentary_breakpoints ()
{
  register struct breakpoint *b;
  ALL_BREAKPOINTS (b)
    if (b->number == -3)
      {
	delete_breakpoint (b);
	break;
      }
}

/* Set a breakpoint from a symtab and line.
   If TEMPFLAG is nonzero, it is a temporary breakpoint.
   Print the same confirmation messages that the breakpoint command prints.  */

void
set_breakpoint (s, line, tempflag)
     struct symtab *s;
     int line;
     int tempflag;
{
  register struct breakpoint *b;
  struct symtab_and_line sal;
  
  sal.symtab = s;
  sal.line = line;
  sal.pc = find_line_pc (sal.symtab, sal.line);
  if (sal.pc == 0)
    error ("No line %d in file \"%s\".\n", sal.line, sal.symtab->filename);
  else
    {
      describe_other_breakpoints (sal.pc);

      b = set_raw_breakpoint (sal);
      b->number = ++breakpoint_count;
      b->cond = 0;
      if (tempflag)
	b->enable = temporary;

      printf ("Breakpoint %d at 0x%x", b->number, b->address);
      if (b->symtab)
	printf (": file %s, line %d.", b->symtab->filename, b->line_number);
      printf ("\n");
    }
}

/* Set a breakpoint according to ARG (function, linenum or *address)
   and make it temporary if TEMPFLAG is nonzero. */

static void
break_command_1 (arg, tempflag, from_tty)
     char *arg;
     int tempflag, from_tty;
{
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  register struct expression *cond = 0;
  register struct breakpoint *b;
  char *save_arg;
  int i;
  CORE_ADDR pc;

  sals.sals = NULL;
  sals.nelts = 0;

  sal.line = sal.pc = sal.end = 0;
  sal.symtab = 0;

  /* If no arg given, or if first arg is 'if ', use the default breakpoint. */

  if (!arg || (arg[0] == 'i' && arg[1] == 'f' 
	       && (arg[2] == ' ' || arg[2] == '\t')))
    {
      if (default_breakpoint_valid)
	{
	  sals.sals = (struct symtab_and_line *) 
	    malloc (sizeof (struct symtab_and_line));
	  sal.pc = default_breakpoint_address;
	  sal.line = default_breakpoint_line;
	  sal.symtab = default_breakpoint_symtab;
	  sals.sals[0] = sal;
	  sals.nelts = 1;
	}
      else
	error ("No default breakpoint address now.");
    }
  else
    /* Force almost all breakpoints to be in terms of the
       current_source_symtab (which is decode_line_1's default).  This
       should produce the results we want almost all of the time while
       leaving default_breakpoint_* alone.  */
    if (default_breakpoint_valid
	&& (!current_source_symtab
	    || (arg && (*arg == '+' || *arg == '-'))))
      sals = decode_line_1 (&arg, 1, default_breakpoint_symtab,
			    default_breakpoint_line);
    else
      sals = decode_line_1 (&arg, 1, 0, 0);
  
  if (! sals.nelts) 
    return;

  save_arg = arg;
  for (i = 0; i < sals.nelts; i++)
    {
      sal = sals.sals[i];
      if (sal.pc == 0 && sal.symtab != 0)
	{
	  pc = find_line_pc (sal.symtab, sal.line);
	  if (pc == 0)
	    error ("No line %d in file \"%s\".",
		   sal.line, sal.symtab->filename);
	}
      else 
	pc = sal.pc;
      
      while (arg && *arg)
	{
	  if (arg[0] == 'i' && arg[1] == 'f'
	      && (arg[2] == ' ' || arg[2] == '\t'))
	    cond = (struct expression *) parse_c_1 ((arg += 2, &arg),
						    block_for_pc (pc), 0);
	  else
	    error ("Junk at end of arguments.");
	}
      arg = save_arg;
      sals.sals[i].pc = pc;
    }

  for (i = 0; i < sals.nelts; i++)
    {
      sal = sals.sals[i];

      if (from_tty)
	describe_other_breakpoints (sal.pc);

      b = set_raw_breakpoint (sal);
      b->number = ++breakpoint_count;
      b->cond = cond;
      if (tempflag)
	b->enable = temporary;

      printf ("Breakpoint %d at 0x%x", b->number, b->address);
      if (b->symtab)
	printf (": file %s, line %d.", b->symtab->filename, b->line_number);
      printf ("\n");
    }

  if (sals.nelts > 1)
    {
      printf ("Multiple breakpoints were set.\n");
      printf ("Use the \"delete\" command to delete unwanted breakpoints.\n");
    }
  free (sals.sals);
}

static void
break_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  break_command_1 (arg, 0, from_tty);
}

static void
tbreak_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  break_command_1 (arg, 1, from_tty);
}

/*
 * Helper routine for the until_command routine in infcmd.c.  Here
 * because it uses the mechanisms of breakpoints.
 */
void
until_break_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  FRAME prev_frame = get_prev_frame (selected_frame);

  clear_proceed_status ();

  /* Set a breakpoint where the user wants it and at return from
     this function */
  
  if (default_breakpoint_valid)
    sals = decode_line_1 (&arg, 1, default_breakpoint_symtab,
			  default_breakpoint_line);
  else
    sals = decode_line_1 (&arg, 1, 0, 0);
  
  if (sals.nelts != 1)
    error ("Couldn't get information on specified line.");
  
  sal = sals.sals[0];
  free (sals.sals);		/* malloc'd, so freed */
  
  if (*arg)
    error ("Junk at end of arguments.");
  
  if (sal.pc == 0 && sal.symtab != 0)
    sal.pc = find_line_pc (sal.symtab, sal.line);
  
  if (sal.pc == 0)
    error ("No line %d in file \"%s\".", sal.line, sal.symtab->filename);
  
  set_momentary_breakpoint (sal, selected_frame);
  
  /* Keep within the current frame */
  
  if (prev_frame)
    {
      struct frame_info *fi;
      
      fi = get_frame_info (prev_frame);
      sal = find_pc_line (fi->pc, 0);
      sal.pc = fi->pc;
      set_momentary_breakpoint (sal, prev_frame);
    }
  
  proceed (-1, -1, 0);
}

static void
clear_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register struct breakpoint *b, *b1;
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  register struct breakpoint *found;
  int i;

  if (arg)
    {
      sals = decode_line_spec (arg, 1);
    }
  else
    {
      sals.sals = (struct symtab_and_line *) malloc (sizeof (struct symtab_and_line));
      sal.line = default_breakpoint_line;
      sal.symtab = default_breakpoint_symtab;
      sal.pc = 0;
      if (sal.symtab == 0)
	error ("No source file specified.");

      sals.sals[0] = sal;
      sals.nelts = 1;
    }

  for (i = 0; i < sals.nelts; i++)
    {
      /* If exact pc given, clear bpts at that pc.
	 But if sal.pc is zero, clear all bpts on specified line.  */
      sal = sals.sals[i];
      found = (struct breakpoint *) 0;
      while (breakpoint_chain
	     && (sal.pc ? breakpoint_chain->address == sal.pc
		 : (breakpoint_chain->symtab == sal.symtab
		    && breakpoint_chain->line_number == sal.line)))
	{
	  b1 = breakpoint_chain;
	  breakpoint_chain = b1->next;
	  b1->next = found;
	  found = b1;
	}

      ALL_BREAKPOINTS (b)
	while (b->next
	       && (sal.pc ? b->next->address == sal.pc
		   : (b->next->symtab == sal.symtab
		      && b->next->line_number == sal.line)))
	  {
	    b1 = b->next;
	    b->next = b1->next;
	    b1->next = found;
	    found = b1;
	  }

      if (found == 0)
	error ("No breakpoint at %s.", arg);

      if (found->next) from_tty = 1; /* Always report if deleted more than one */
      if (from_tty) printf ("Deleted breakpoint%s ", found->next ? "s" : "");
      while (found)
	{
	  if (from_tty) printf ("%d ", found->number);
	  b1 = found->next;
	  delete_breakpoint (found);
	  found = b1;
	}
      if (from_tty) putchar ('\n');
    }
  free (sals.sals);
}

/* Delete breakpoint number BNUM if it is a `delete' breakpoint.
   This is called after breakpoint BNUM has been hit.
   Also delete any breakpoint numbered -3 unless there are breakpoint
   commands to be executed.  */

void
breakpoint_auto_delete (bnum)
     int bnum;
{
  register struct breakpoint *b;
  if (bnum != 0)
    ALL_BREAKPOINTS (b)
      if (b->number == bnum)
	{
	  if (b->enable == delete)
	    delete_breakpoint (b);
	  break;
	}
  if (breakpoint_commands == 0)
    clear_momentary_breakpoints ();
}

static void
delete_breakpoint (bpt)
     struct breakpoint *bpt;
{
  register struct breakpoint *b;

  if (bpt->inserted)
    write_memory (bpt->address, bpt->shadow_contents, sizeof break_insn);

  if (breakpoint_chain == bpt)
    breakpoint_chain = bpt->next;

  ALL_BREAKPOINTS (b)
    if (b->next == bpt)
      {
	b->next = bpt->next;
	break;
      }

  check_duplicates (bpt->address);

  free_command_lines (bpt->commands);
  if (bpt->cond)
    free (bpt->cond);

  if (xgdb_verbose && bpt->number >=0)
    printf ("breakpoint #%d deleted\n", bpt->number);

  free (bpt);
}

static void map_breakpoint_numbers ();

static void
delete_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register struct breakpoint *b, *b1;

  if (arg == 0)
    {
      /* Ask user only if there are some breakpoints to delete.  */
      if (!from_tty
	  || breakpoint_chain && query ("Delete all breakpoints? "))
	{
	  /* No arg; clear all breakpoints.  */
	  while (breakpoint_chain)
	    delete_breakpoint (breakpoint_chain);
	}
    }
  else
    map_breakpoint_numbers (arg, delete_breakpoint);
}

/* Delete all breakpoints.
   Done when new symtabs are loaded, since the break condition expressions
   may become invalid, and the breakpoints are probably wrong anyway.  */

void
clear_breakpoints ()
{
  delete_command (0, 0);
}

/* Set ignore-count of breakpoint number BPTNUM to COUNT.
   If from_tty is nonzero, it prints a message to that effect,
   which ends with a period (no newline).  */

void
set_ignore_count (bptnum, count, from_tty)
     int bptnum, count, from_tty;
{
  register struct breakpoint *b;

  if (count < 0)
    count = 0;

  ALL_BREAKPOINTS (b)
    if (b->number == bptnum)
      {
	b->ignore_count = count;
	if (!from_tty)
	  return;
	else if (count == 0)
	  printf ("Will stop next time breakpoint %d is reached.", bptnum);
	else if (count == 1)
	  printf ("Will ignore next crossing of breakpoint %d.", bptnum);
	else
	  printf ("Will ignore next %d crossings of breakpoint %d.",
		  count, bptnum);
	return;
      }

  error ("No breakpoint number %d.", bptnum);
}

/* Clear the ignore counts of all breakpoints.  */
void
breakpoint_clear_ignore_counts ()
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    b->ignore_count = 0;
}

/* Command to set ignore-count of breakpoint N to COUNT.  */

static void
ignore_command (args, from_tty)
     char *args;
     int from_tty;
{
  register char *p = args;
  register int num;

  if (p == 0)
    error_no_arg ("a breakpoint number");
  
  while (*p >= '0' && *p <= '9') p++;
  if (*p && *p != ' ' && *p != '\t')
    error ("First argument must be a breakpoint number.");

  num = atoi (args);

  if (*p == 0)
    error ("Second argument (specified ignore-count) is missing.");

  set_ignore_count (num, parse_and_eval_address (p), from_tty);
  printf ("\n");
}

/* Call FUNCTION on each of the breakpoints
   whose numbers are given in ARGS.  */

static void
map_breakpoint_numbers (args, function)
     char *args;
     void (*function) ();
{
  register char *p = args;
  register char *p1;
  register int num;
  register struct breakpoint *b;

  if (p == 0)
    error_no_arg ("one or more breakpoint numbers");

  while (*p)
    {
      p1 = p;
      while (*p1 >= '0' && *p1 <= '9') p1++;
      if (*p1 && *p1 != ' ' && *p1 != '\t')
	error ("Arguments must be breakpoint numbers.");

      num = atoi (p);

      ALL_BREAKPOINTS (b)
	if (b->number == num)
	  {
	    function (b);
	    goto win;
	  }
      printf ("No breakpoint number %d.\n", num);
    win:
      p = p1;
      while (*p == ' ' || *p == '\t') p++;
    }
}

static void
enable_breakpoint (bpt)
     struct breakpoint *bpt;
{
  bpt->enable = enabled;

  if (xgdb_verbose && bpt->number >= 0)
    printf ("breakpoint #%d enabled\n", bpt->number);

  check_duplicates (bpt->address);
}

static void
enable_command (args)
     char *args;
{
  struct breakpoint *bpt;
  if (args == 0)
    ALL_BREAKPOINTS (bpt)
      enable_breakpoint (bpt);
  else
    map_breakpoint_numbers (args, enable_breakpoint);
}

static void
disable_breakpoint (bpt)
     struct breakpoint *bpt;
{
  bpt->enable = disabled;

  if (xgdb_verbose && bpt->number >= 0)
    printf ("breakpoint #%d disabled\n", bpt->number);

  check_duplicates (bpt->address);
}

static void
disable_command (args)
     char *args;
{
  register struct breakpoint *bpt;
  if (args == 0)
    ALL_BREAKPOINTS (bpt)
      disable_breakpoint (bpt);
  else
    map_breakpoint_numbers (args, disable_breakpoint);
}

static void
enable_once_breakpoint (bpt)
     struct breakpoint *bpt;
{
  bpt->enable = temporary;

  check_duplicates (bpt->address);
}

static void
enable_once_command (args)
     char *args;
{
  map_breakpoint_numbers (args, enable_once_breakpoint);
}

static void
enable_delete_breakpoint (bpt)
     struct breakpoint *bpt;
{
  bpt->enable = delete;

  check_duplicates (bpt->address);
}

static void
enable_delete_command (args)
     char *args;
{
  map_breakpoint_numbers (args, enable_delete_breakpoint);
}

/*
 * Use default_breakpoint_'s, or nothing if they aren't valid.
 */
struct symtabs_and_lines
decode_line_spec_1 (string, funfirstline)
     char *string;
     int funfirstline;
{
  struct symtabs_and_lines sals;
  if (string == 0)
    error ("Empty line specification.");
  if (default_breakpoint_valid)
    sals = decode_line_1 (&string, funfirstline,
			  default_breakpoint_symtab, default_breakpoint_line);
  else
    sals = decode_line_1 (&string, funfirstline, 0, 0);
  if (*string)
    error ("Junk at end of line specification: %s", string);
  return sals;
}


/* Chain containing all defined enable commands.  */

extern struct cmd_list_element 
  *enablelist, *disablelist,
  *deletelist, *enablebreaklist;

extern struct cmd_list_element *cmdlist;

void
_initialize_breakpoint ()
{
  breakpoint_chain = 0;
  breakpoint_count = 0;

  add_com ("ignore", class_breakpoint, ignore_command,
	   "Set ignore-count of breakpoint number N to COUNT.");

  add_com ("commands", class_breakpoint, commands_command,
	   "Set commands to be executed when a breakpoint is hit.\n\
Give breakpoint number as argument after \"commands\".\n\
With no argument, the targeted breakpoint is the last one set.\n\
The commands themselves follow starting on the next line.\n\
Type a line containing \"end\" to indicate the end of them.\n\
Give \"silent\" as the first line to make the breakpoint silent;\n\
then no output is printed when it is hit, except what the commands print.");

  add_com ("condition", class_breakpoint, condition_command,
	   "Specify breakpoint number N to break only if COND is true.\n\
N is an integer; COND is a C expression to be evaluated whenever\n\
breakpoint N is reached.  Actually break only when COND is nonzero.");

  add_com ("tbreak", class_breakpoint, tbreak_command,
	   "Set a temporary breakpoint.  Args like \"break\" command.\n\
Like \"break\" except the breakpoint is only enabled temporarily,\n\
so it will be disabled when hit.  Equivalent to \"break\" followed\n\
by using \"enable once\" on the breakpoint number.");

  add_prefix_cmd ("enable", class_breakpoint, enable_command,
		  "Enable some breakpoints or auto-display expressions.\n\
Give breakpoint numbers (separated by spaces) as arguments.\n\
With no subcommand, breakpoints are enabled until you command otherwise.\n\
This is used to cancel the effect of the \"disable\" command.\n\
With a subcommand you can enable temporarily.\n\
\n\
The \"display\" subcommand applies to auto-displays instead of breakpoints.",
		  &enablelist, "enable ", 1, &cmdlist);

  add_abbrev_prefix_cmd ("breakpoints", class_breakpoint, enable_command,
		  "Enable some breakpoints or auto-display expressions.\n\
Give breakpoint numbers (separated by spaces) as arguments.\n\
With no subcommand, breakpoints are enabled until you command otherwise.\n\
This is used to cancel the effect of the \"disable\" command.\n\
May be abbreviates to simply \"enable\".\n\
With a subcommand you can enable temporarily.",
		  &enablebreaklist, "enable breakpoints ", 1, &enablelist);

  add_cmd ("once", no_class, enable_once_command,
	   "Enable breakpoints for one hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it becomes disabled.\n\
See the \"tbreak\" command which sets a breakpoint and enables it once.",
	   &enablebreaklist);

  add_cmd ("delete", no_class, enable_delete_command,
	   "Enable breakpoints and delete when hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it is deleted.",
	   &enablebreaklist);

  add_cmd ("delete", no_class, enable_delete_command,
	   "Enable breakpoints and delete when hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it is deleted.",
	   &enablelist);

  add_cmd ("once", no_class, enable_once_command,
	   "Enable breakpoints for one hit.  Give breakpoint numbers.\n\
If a breakpoint is hit while enabled in this fashion, it becomes disabled.\n\
See the \"tbreak\" command which sets a breakpoint and enables it once.",
	   &enablelist);

  add_prefix_cmd ("disable", class_breakpoint, disable_command,
	   "Disable some breakpoints or auto-display expressions.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To disable all breakpoints, give no argument.\n\
A disabled breakpoint is not forgotten, but has no effect until reenabled.\n\
\n\
The \"display\" subcommand applies to auto-displays instead of breakpoints.",
		  &disablelist, "disable ", 1, &cmdlist);
  add_com_alias ("dis", "disable", class_breakpoint, 1);
  add_com_alias ("disa", "disable", class_breakpoint, 1);

  add_abbrev_cmd ("breakpoints", class_breakpoint, disable_command,
	   "Disable some breakpoints or auto-display expressions.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To disable all breakpoints, give no argument.\n\
A disabled breakpoint is not forgotten, but has no effect until reenabled.\n\
This command may be abbreviated \"disable\".",
	   &disablelist);

  add_prefix_cmd ("delete", class_breakpoint, delete_command,
	   "Delete some breakpoints or auto-display expressions.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To delete all breakpoints, give no argument.\n\
\n\
Also a prefix command for deletion of other GDB objects.\n\
The \"unset\" command is also an alias for \"delete\".",
		  &deletelist, "delete ", 1, &cmdlist);
  add_com_alias ("d", "delete", class_breakpoint, 1);
  add_com_alias ("unset", "delete", class_alias, 1);

  add_cmd ("breakpoints", class_alias, delete_command,
	   "Delete some breakpoints or auto-display expressions.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To delete all breakpoints, give no argument.\n\
This command may be abbreviated \"delete\".",
	   &deletelist);

  add_com ("clear", class_breakpoint, clear_command,
	   "Clear breakpoint at specified line or function.\n\
Argument may be line number, function name, or \"*\" and an address.\n\
If line number is specified, all breakpoints in that line are cleared.\n\
If function is specified, breakpoints at beginning of function are cleared.\n\
If an address is specified, breakpoints at that address are cleared.\n\n\
With no argument, clears all breakpoints in the line that the selected frame\n\
is executing in.\n\
\n\
See also the \"delete\" command which clears breakpoints by number.");

  add_com ("break", class_breakpoint, break_command,
	   "Set breakpoint at specified line or function.\n\
Argument may be line number, function name, or \"*\" and an address.\n\
If line number is specified, break at start of code for that line.\n\
If function is specified, break at start of code for that function.\n\
If an address is specified, break at that exact address.\n\
With no arg, uses current execution address of selected stack frame.\n\
This is useful for breaking on return to a stack frame.\n\
\n\
Multiple breakpoints at one place are permitted, and useful if conditional.\n\
\n\
Do \"help breakpoints\" for info on other commands dealing with breakpoints.");
  add_com_alias ("b", "break", class_run, 1);
  add_com_alias ("br", "break", class_run, 1);
  add_com_alias ("bre", "break", class_run, 1);
  add_com_alias ("brea", "break", class_run, 1);

  add_info ("breakpoints", breakpoints_info,
	    "Status of all breakpoints, or breakpoint number NUMBER.\n\
Second column is \"y\" for enabled breakpoint, \"n\" for disabled,\n\
\"o\" for enabled once (disable when hit), \"d\" for enable but delete when hit.\n\
Then come the address and the file/line number.\n\n\
Convenience variable \"$_\" and default examine address for \"x\"\n\
are set to the address of the last breakpoint listed.");
}

