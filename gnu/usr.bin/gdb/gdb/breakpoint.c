/* Everything about breakpoints, for GDB.
   Copyright 1986, 1987, 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

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
#include <ctype.h>
#include "symtab.h"
#include "frame.h"
#include "breakpoint.h"
#include "gdbtypes.h"
#include "expression.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "value.h"
#include "ctype.h"
#include "command.h"
#include "inferior.h"
#include "target.h"
#include "language.h"
#include <string.h>
#include "demangle.h"

/* local function prototypes */

static void
catch_command_1 PARAMS ((char *, int, int));

static void
enable_delete_command PARAMS ((char *, int));

static void
enable_delete_breakpoint PARAMS ((struct breakpoint *));

static void
enable_once_command PARAMS ((char *, int));

static void
enable_once_breakpoint PARAMS ((struct breakpoint *));

static void
disable_command PARAMS ((char *, int));

static void
disable_breakpoint PARAMS ((struct breakpoint *));

static void
enable_command PARAMS ((char *, int));

static void
enable_breakpoint PARAMS ((struct breakpoint *));

static void
map_breakpoint_numbers PARAMS ((char *,	void (*)(struct breakpoint *)));

static void
ignore_command PARAMS ((char *, int));

static int
breakpoint_re_set_one PARAMS ((char *));

static void
delete_command PARAMS ((char *, int));

static void
clear_command PARAMS ((char *, int));

static void
catch_command PARAMS ((char *, int));

static struct symtabs_and_lines
get_catch_sals PARAMS ((int));

static void
watch_command PARAMS ((char *, int));

static void
tbreak_command PARAMS ((char *, int));

static void
break_command_1 PARAMS ((char *, int, int));

static void
mention PARAMS ((struct breakpoint *));

static struct breakpoint *
set_raw_breakpoint PARAMS ((struct symtab_and_line));

static void
check_duplicates PARAMS ((CORE_ADDR));

static void
describe_other_breakpoints PARAMS ((CORE_ADDR));

static void
breakpoints_info PARAMS ((char *, int));

static void
breakpoint_1 PARAMS ((int, int));

static bpstat
bpstat_alloc PARAMS ((struct breakpoint *, bpstat));

static int
breakpoint_cond_eval PARAMS ((char *));

static void
cleanup_executing_breakpoints PARAMS ((int));

static void
commands_command PARAMS ((char *, int));

static void
condition_command PARAMS ((char *, int));

static int
get_number PARAMS ((char **));

static void
set_breakpoint_count PARAMS ((int));


extern int addressprint;		/* Print machine addresses? */
extern int demangle;			/* Print de-mangled symbol names? */

/* Are we executing breakpoint commands?  */
static int executing_breakpoint_commands;

/* Walk the following statement or block through all breakpoints.
   ALL_BREAKPOINTS_SAFE does so even if the statment deletes the current
   breakpoint.  */

#define ALL_BREAKPOINTS(b)  for (b = breakpoint_chain; b; b = b->next)

#define ALL_BREAKPOINTS_SAFE(b,tmp)	\
	for (b = breakpoint_chain;	\
	     b? (tmp=b->next, 1): 0;	\
	     b = tmp)

/* Chain of all breakpoints defined.  */

struct breakpoint *breakpoint_chain;

/* Number of last breakpoint made.  */

static int breakpoint_count;

/* Set breakpoint count to NUM.  */
static void
set_breakpoint_count (num)
     int num;
{
  breakpoint_count = num;
  set_internalvar (lookup_internalvar ("bpnum"),
		   value_from_longest (builtin_type_int, (LONGEST) num));
}

/* Default address, symtab and line to put a breakpoint at
   for "break" command with no arg.
   if default_breakpoint_valid is zero, the other three are
   not valid, and "break" with no arg is an error.

   This set by print_stack_frame, which calls set_default_breakpoint.  */

int default_breakpoint_valid;
CORE_ADDR default_breakpoint_address;
struct symtab *default_breakpoint_symtab;
int default_breakpoint_line;

/* Flag indicating extra verbosity for xgdb.  */
extern int xgdb_verbose;

/* *PP is a string denoting a breakpoint.  Get the number of the breakpoint.
   Advance *PP after the string and any trailing whitespace.

   Currently the string can either be a number or "$" followed by the name
   of a convenience variable.  Making it an expression wouldn't work well
   for map_breakpoint_numbers (e.g. "4 + 5 + 6").  */
static int
get_number (pp)
     char **pp;
{
  int retval;
  char *p = *pp;

  if (p == NULL)
    /* Empty line means refer to the last breakpoint.  */
    return breakpoint_count;
  else if (*p == '$')
    {
      /* Make a copy of the name, so we can null-terminate it
	 to pass to lookup_internalvar().  */
      char *varname;
      char *start = ++p;
      value val;

      while (isalnum (*p) || *p == '_')
	p++;
      varname = (char *) alloca (p - start + 1);
      strncpy (varname, start, p - start);
      varname[p - start] = '\0';
      val = value_of_internalvar (lookup_internalvar (varname));
      if (TYPE_CODE (VALUE_TYPE (val)) != TYPE_CODE_INT)
	error (
"Convenience variables used to specify breakpoints must have integer values."
	       );
      retval = (int) value_as_long (val);
    }
  else
    {
      if (*p == '-')
	++p;
      while (*p >= '0' && *p <= '9')
	++p;
      if (p == *pp)
	/* There is no number here.  (e.g. "cond a == b").  */
	error_no_arg ("breakpoint number");
      retval = atoi (*pp);
    }
  if (!(isspace (*p) || *p == '\0'))
    error ("breakpoint number expected");
  while (isspace (*p))
    p++;
  *pp = p;
  return retval;
}

/* condition N EXP -- set break condition of breakpoint N to EXP.  */

static void
condition_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register struct breakpoint *b;
  char *p;
  register int bnum;

  if (arg == 0)
    error_no_arg ("breakpoint number");

  p = arg;
  bnum = get_number (&p);

  ALL_BREAKPOINTS (b)
    if (b->number == bnum)
      {
	if (b->cond)
	  {
	    free ((PTR)b->cond);
	    b->cond = 0;
	  }
	if (b->cond_string != NULL)
	  free ((PTR)b->cond_string);

	if (*p == 0)
	  {
	    b->cond = 0;
	    b->cond_string = NULL;
	    if (from_tty)
	      printf_filtered ("Breakpoint %d now unconditional.\n", bnum);
	  }
	else
	  {
	    arg = p;
	    /* I don't know if it matters whether this is the string the user
	       typed in or the decompiled expression.  */
	    b->cond_string = savestring (arg, strlen (arg));
	    b->cond = parse_exp_1 (&arg, block_for_pc (b->address), 0);
	    if (*arg)
	      error ("Junk at end of expression");
	  }
	return;
      }

  error ("No breakpoint number %d.", bnum);
}

/* ARGSUSED */
static void
commands_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  register struct breakpoint *b;
  char *p;
  register int bnum;
  struct command_line *l;

  /* If we allowed this, we would have problems with when to
     free the storage, if we change the commands currently
     being read from.  */

  if (executing_breakpoint_commands)
    error ("Can't use the \"commands\" command among a breakpoint's commands.");

  p = arg;
  bnum = get_number (&p);
  if (p && *p)
    error ("Unexpected extra arguments following breakpoint number.");
      
  ALL_BREAKPOINTS (b)
    if (b->number == bnum)
      {
	if (from_tty && input_from_terminal_p ())
	  printf_filtered ("Type commands for when breakpoint %d is hit, one per line.\n\
End with a line saying just \"end\".\n", bnum);
	l = read_command_lines ();
	free_command_lines (&b->commands);
	b->commands = l;
	return;
      }
  error ("No breakpoint number %d.", bnum);
}

extern int memory_breakpoint_size; /* from mem-break.c */

/* Like target_read_memory() but if breakpoints are inserted, return
   the shadow contents instead of the breakpoints themselves.

   Read "memory data" from whatever target or inferior we have. 
   Returns zero if successful, errno value if not.  EIO is used
   for address out of bounds.  If breakpoints are inserted, returns
   shadow contents, not the breakpoints themselves.  From breakpoint.c.  */

int
read_memory_nobpt (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     unsigned len;
{
  int status;
  struct breakpoint *b;

  if (memory_breakpoint_size < 0)
    /* No breakpoints on this machine.  FIXME: This should be
       dependent on the debugging target.  Probably want
       target_insert_breakpoint to return a size, saying how many
       bytes of the shadow contents are used, or perhaps have
       something like target_xfer_shadow.  */
    return target_read_memory (memaddr, myaddr, len);
  
  ALL_BREAKPOINTS (b)
    {
      if (b->type == bp_watchpoint || !b->inserted)
	continue;
      else if (b->address + memory_breakpoint_size <= memaddr)
	/* The breakpoint is entirely before the chunk of memory
	   we are reading.  */
	continue;
      else if (b->address >= memaddr + len)
	/* The breakpoint is entirely after the chunk of memory we
	   are reading.  */
	continue;
      else
	{
	  /* Copy the breakpoint from the shadow contents, and recurse
	     for the things before and after.  */
	  
	  /* Addresses and length of the part of the breakpoint that
	     we need to copy.  */
	  CORE_ADDR membpt = b->address;
	  unsigned int bptlen = memory_breakpoint_size;
	  /* Offset within shadow_contents.  */
	  int bptoffset = 0;
	  
	  if (membpt < memaddr)
	    {
	      /* Only copy the second part of the breakpoint.  */
	      bptlen -= memaddr - membpt;
	      bptoffset = memaddr - membpt;
	      membpt = memaddr;
	    }

	  if (membpt + bptlen > memaddr + len)
	    {
	      /* Only copy the first part of the breakpoint.  */
	      bptlen -= (membpt + bptlen) - (memaddr + len);
	    }

	  memcpy (myaddr + membpt - memaddr, 
		  b->shadow_contents + bptoffset, bptlen);

	  if (membpt > memaddr)
	    {
	      /* Copy the section of memory before the breakpoint.  */
	      status = read_memory_nobpt (memaddr, myaddr, membpt - memaddr);
	      if (status != 0)
		return status;
	    }

	  if (membpt + bptlen < memaddr + len)
	    {
	      /* Copy the section of memory after the breakpoint.  */
	      status = read_memory_nobpt
		(membpt + bptlen,
		 myaddr + membpt + bptlen - memaddr,
		 memaddr + len - (membpt + bptlen));
	      if (status != 0)
		return status;
	    }
	  return 0;
	}
    }
  /* Nothing overlaps.  Just call read_memory_noerr.  */
  return target_read_memory (memaddr, myaddr, len);
}

/* insert_breakpoints is used when starting or continuing the program.
   remove_breakpoints is used when the program stops.
   Both return zero if successful,
   or an `errno' value if could not write the inferior.  */

int
insert_breakpoints ()
{
  register struct breakpoint *b;
  int val = 0;
  int disabled_breaks = 0;

  ALL_BREAKPOINTS (b)
    if (b->type != bp_watchpoint
	&& b->enable != disabled
	&& ! b->inserted
	&& ! b->duplicate)
      {
	val = target_insert_breakpoint(b->address, b->shadow_contents);
	if (val)
	  {
	    /* Can't set the breakpoint.  */
#if defined (DISABLE_UNSETTABLE_BREAK)
	    if (DISABLE_UNSETTABLE_BREAK (b->address))
	      {
		val = 0;
		b->enable = disabled;
		if (!disabled_breaks)
		  {
		    fprintf (stderr,
			 "Cannot insert breakpoint %d:\n", b->number);
		    printf_filtered ("Disabling shared library breakpoints:\n");
		  }
		disabled_breaks = 1;
		printf_filtered ("%d ", b->number);
	      }
	    else
#endif
	      {
		fprintf (stderr, "Cannot insert breakpoint %d:\n", b->number);
#ifdef ONE_PROCESS_WRITETEXT
		fprintf (stderr,
		  "The same program may be running in another process.\n");
#endif
		memory_error (val, b->address);	/* which bombs us out */
	      }
	  }
	else
	  b->inserted = 1;
      }
  if (disabled_breaks)
    printf_filtered ("\n");
  return val;
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
    if (b->type != bp_watchpoint && b->inserted)
      {
	val = target_remove_breakpoint(b->address, b->shadow_contents);
	if (val)
	  return val;
	b->inserted = 0;
#ifdef BREAKPOINT_DEBUG
	printf ("Removed breakpoint at %s",
		local_hex_string((unsigned long) b->address));
	printf (", shadow %s",
		local_hex_string((unsigned long) b->shadow_contents[0]));
	printf (", %s.\n",
		local_hex_string((unsigned long) b->shadow_contents[1]));
#endif /* BREAKPOINT_DEBUG */
      }

  return 0;
}

/* Clear the "inserted" flag in all breakpoints.  */

void
mark_breakpoints_out ()
{
  register struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    b->inserted = 0;
}

/* Clear the "inserted" flag in all breakpoints and delete any breakpoints
   which should go away between runs of the program.  */

void
breakpoint_init_inferior ()
{
  register struct breakpoint *b, *temp;

  ALL_BREAKPOINTS_SAFE (b, temp)
    {
      b->inserted = 0;

      /* If the call dummy breakpoint is at the entry point it will
	 cause problems when the inferior is rerun, so we better
	 get rid of it.  */
      if (b->type == bp_call_dummy)
	delete_breakpoint (b);
    }
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

/* breakpoint_match_thread (PC, PID) returns true if the breakpoint at PC
   is valid for process/thread PID.  */

int
breakpoint_thread_match (pc, pid)
     CORE_ADDR pc;
     int pid;
{
  struct breakpoint *b;
  int thread;

  thread = pid_to_thread_id (pid);

  ALL_BREAKPOINTS (b)
    if (b->enable != disabled
	&& b->address == pc
	&& (b->thread == -1 || b->thread == thread))
      return 1;

  return 0;
}


/* bpstat stuff.  External routines' interfaces are documented
   in breakpoint.h.  */

/* Clear a bpstat so that it says we are not at any breakpoint.
   Also free any storage that is part of a bpstat.  */

void
bpstat_clear (bsp)
     bpstat *bsp;
{
  bpstat p;
  bpstat q;

  if (bsp == 0)
    return;
  p = *bsp;
  while (p != NULL)
    {
      q = p->next;
      if (p->old_val != NULL)
	value_free (p->old_val);
      free ((PTR)p);
      p = q;
    }
  *bsp = NULL;
}

/* Return a copy of a bpstat.  Like "bs1 = bs2" but all storage that
   is part of the bpstat is copied as well.  */

bpstat
bpstat_copy (bs)
     bpstat bs;
{
  bpstat p = NULL;
  bpstat tmp;
  bpstat retval = NULL;

  if (bs == NULL)
    return bs;

  for (; bs != NULL; bs = bs->next)
    {
      tmp = (bpstat) xmalloc (sizeof (*tmp));
      memcpy (tmp, bs, sizeof (*tmp));
      if (p == NULL)
	/* This is the first thing in the chain.  */
	retval = tmp;
      else
	p->next = tmp;
      p = tmp;
    }
  p->next = NULL;
  return retval;
}

/* Find the bpstat associated with this breakpoint */

bpstat
bpstat_find_breakpoint(bsp, breakpoint)
     bpstat bsp;
     struct breakpoint *breakpoint;
{
  if (bsp == NULL) return NULL;

  for (;bsp != NULL; bsp = bsp->next) {
    if (bsp->breakpoint_at == breakpoint) return bsp;
  }
  return NULL;
}

/* Return the breakpoint number of the first breakpoint we are stopped
   at.  *BSP upon return is a bpstat which points to the remaining
   breakpoints stopped at (but which is not guaranteed to be good for
   anything but further calls to bpstat_num).
   Return 0 if passed a bpstat which does not indicate any breakpoints.  */

int
bpstat_num (bsp)
     bpstat *bsp;
{
  struct breakpoint *b;

  if ((*bsp) == NULL)
    return 0;			/* No more breakpoint values */
  else
    {
      b = (*bsp)->breakpoint_at;
      *bsp = (*bsp)->next;
      if (b == NULL)
	return -1;		/* breakpoint that's been deleted since */
      else
        return b->number;	/* We have its number */
    }
}

/* Modify BS so that the actions will not be performed.  */

void
bpstat_clear_actions (bs)
     bpstat bs;
{
  for (; bs != NULL; bs = bs->next)
    {
      bs->commands = NULL;
      if (bs->old_val != NULL)
	{
	  value_free (bs->old_val);
	  bs->old_val = NULL;
	}
    }
}

/* Stub for cleaning up our state if we error-out of a breakpoint command */
/* ARGSUSED */
static void
cleanup_executing_breakpoints (ignore)
     int ignore;
{
  executing_breakpoint_commands = 0;
}

/* Execute all the commands associated with all the breakpoints at this
   location.  Any of these commands could cause the process to proceed
   beyond this point, etc.  We look out for such changes by checking
   the global "breakpoint_proceeded" after each command.  */

void
bpstat_do_actions (bsp)
     bpstat *bsp;
{
  bpstat bs;
  struct cleanup *old_chain;

  executing_breakpoint_commands = 1;
  old_chain = make_cleanup (cleanup_executing_breakpoints, 0);

top:
  bs = *bsp;

  breakpoint_proceeded = 0;
  for (; bs != NULL; bs = bs->next)
    {
      while (bs->commands)
	{
	  char *line = bs->commands->line;
	  bs->commands = bs->commands->next;
	  execute_command (line, 0);
	  /* If the inferior is proceeded by the command, bomb out now.
	     The bpstat chain has been blown away by wait_for_inferior.
	     But since execution has stopped again, there is a new bpstat
	     to look at, so start over.  */
	  if (breakpoint_proceeded)
	    goto top;
	}
    }

  executing_breakpoint_commands = 0;
  discard_cleanups (old_chain);
}

/* This is the normal print_it function for a bpstat.  In the future,
   much of this logic could (should?) be moved to bpstat_stop_status,
   by having it set different print_it functions.  */

static int
print_it_normal (bs)
     bpstat bs;
{
  /* bs->breakpoint_at can be NULL if it was a momentary breakpoint
     which has since been deleted.  */
  if (bs->breakpoint_at == NULL
      || (bs->breakpoint_at->type != bp_breakpoint
	  && bs->breakpoint_at->type != bp_watchpoint))
    return 0;

  if (bs->breakpoint_at->type == bp_breakpoint)
    {
      /* I think the user probably only wants to see one breakpoint
	 number, not all of them.  */
      printf_filtered ("\nBreakpoint %d, ", bs->breakpoint_at->number);
      return 0;
    }
      
  if (bs->old_val != NULL)
    {
      printf_filtered ("\nWatchpoint %d, ", bs->breakpoint_at->number);
      print_expression (bs->breakpoint_at->exp, stdout);
      printf_filtered ("\nOld value = ");
      value_print (bs->old_val, stdout, 0, Val_pretty_default);
      printf_filtered ("\nNew value = ");
      value_print (bs->breakpoint_at->val, stdout, 0,
		   Val_pretty_default);
      printf_filtered ("\n");
      value_free (bs->old_val);
      bs->old_val = NULL;
      return 0;
    }
  /* We can't deal with it.  Maybe another member of the bpstat chain can.  */
  return -1;
}

/* Print a message indicating what happened.  Returns nonzero to
   say that only the source line should be printed after this (zero
   return means print the frame as well as the source line).  */
/* Currently we always return zero.  */
int
bpstat_print (bs)
     bpstat bs;
{
  int val;
  
  if (bs == NULL)
    return 0;

  val = (*bs->print_it) (bs);
  if (val >= 0)
    return val;
  
  /* Maybe another breakpoint in the chain caused us to stop.
     (Currently all watchpoints go on the bpstat whether hit or
     not.  That probably could (should) be changed, provided care is taken
     with respect to bpstat_explains_signal).  */
  if (bs->next)
    return bpstat_print (bs->next);

  /* We reached the end of the chain without printing anything.  */
  return 0;
}

/* Evaluate the expression EXP and return 1 if value is zero.
   This is used inside a catch_errors to evaluate the breakpoint condition. 
   The argument is a "struct expression *" that has been cast to char * to 
   make it pass through catch_errors.  */

static int
breakpoint_cond_eval (exp)
     char *exp;
{
  return !value_true (evaluate_expression ((struct expression *)exp));
}

/* Allocate a new bpstat and chain it to the current one.  */

static bpstat
bpstat_alloc (b, cbs)
     register struct breakpoint *b;
     bpstat cbs;			/* Current "bs" value */
{
  bpstat bs;

  bs = (bpstat) xmalloc (sizeof (*bs));
  cbs->next = bs;
  bs->breakpoint_at = b;
  /* If the condition is false, etc., don't do the commands.  */
  bs->commands = NULL;
  bs->old_val = NULL;
  bs->print_it = print_it_normal;
  return bs;
}

/* Return the frame which we can use to evaluate the expression
   whose valid block is valid_block, or NULL if not in scope.

   This whole concept is probably not the way to do things (it is incredibly
   slow being the main reason, not to mention fragile (e.g. the sparc
   frame pointer being fetched as 0 bug causes it to stop)).  Instead,
   introduce a version of "struct frame" which survives over calls to the
   inferior, but which is better than FRAME_ADDR in the sense that it lets
   us evaluate expressions relative to that frame (on some machines, it
   can just be a FRAME_ADDR).  Save one of those instead of (or in addition
   to) the exp_valid_block, and then use it to evaluate the watchpoint
   expression, with no need to do all this backtracing every time.

   Or better yet, what if it just copied the struct frame and its next
   frame?  Off the top of my head, I would think that would work
   because things like (a29k) rsize and msize, or (sparc) bottom just
   depend on the frame, and aren't going to be different just because
   the inferior has done something.  Trying to recalculate them
   strikes me as a lot of work, possibly even impossible.  Saving the
   next frame is needed at least on a29k, where get_saved_register
   uses fi->next->saved_msp.  For figuring out whether that frame is
   still on the stack, I guess this needs to be machine-specific (e.g.
   a29k) but I think

      read_fp () INNER_THAN watchpoint_frame->frame

   would generally work.

   Of course the scope of the expression could be less than a whole
   function; perhaps if the innermost frame is the one which the
   watchpoint is relative to (another machine-specific thing, usually

      FRAMELESS_FUNCTION_INVOCATION (get_current_frame(), fromleaf)
      read_fp () == wp_frame->frame
      && !fromleaf

   ), *then* it could do a

      contained_in (get_current_block (), wp->exp_valid_block).

      */

FRAME
within_scope (valid_block)
     struct block *valid_block;
{
  FRAME fr = get_current_frame ();
  struct frame_info *fi = get_frame_info (fr);
  CORE_ADDR func_start;

  /* If caller_pc_valid is true, we are stepping through
     a function prologue, which is bounded by callee_func_start
     (inclusive) and callee_prologue_end (exclusive).
     caller_pc is the pc of the caller.

     Yes, this is hairy.  */
  static int caller_pc_valid = 0;
  static CORE_ADDR caller_pc;
  static CORE_ADDR callee_func_start;
  static CORE_ADDR callee_prologue_end;
  
  find_pc_partial_function (fi->pc, (PTR)NULL, &func_start, (CORE_ADDR *)NULL);
  func_start += FUNCTION_START_OFFSET;
  if (fi->pc == func_start)
    {
      /* We just called a function.  The only other case I
	 can think of where the pc would equal the pc of the
	 start of a function is a frameless function (i.e.
	 no prologue) where we branch back to the start
	 of the function.  In that case, SKIP_PROLOGUE won't
	 find one, and we'll clear caller_pc_valid a few lines
	 down.  */
      caller_pc_valid = 1;
      caller_pc = SAVED_PC_AFTER_CALL (fr);
      callee_func_start = func_start;
      SKIP_PROLOGUE (func_start);
      callee_prologue_end = func_start;
    }
  if (caller_pc_valid)
    {
      if (fi->pc < callee_func_start
	  || fi->pc >= callee_prologue_end)
	caller_pc_valid = 0;
    }
	  
  if (contained_in (block_for_pc (caller_pc_valid
				  ? caller_pc
				  : fi->pc),
		    valid_block))
    {
      return fr;
    }
  fr = get_prev_frame (fr);
	  
  /* If any active frame is in the exp_valid_block, then it's
     OK.  Note that this might not be the same invocation of
     the exp_valid_block that we were watching a little while
     ago, or the same one as when the watchpoint was set (e.g.
     we are watching a local variable in a recursive function.
     When we return from a recursive invocation, then we are
     suddenly watching a different instance of the variable).

     At least for now I am going to consider this a feature.  */
  for (; fr != NULL; fr = get_prev_frame (fr))
    {
      fi = get_frame_info (fr);
      if (contained_in (block_for_pc (fi->pc),
			valid_block))
	{
	  return fr;
	}
    }
  return NULL;
}

/* Possible return values for watchpoint_check (this can't be an enum
   because of check_errors).  */
/* The watchpoint has been disabled.  */
#define WP_DISABLED 1
/* The value has changed.  */
#define WP_VALUE_CHANGED 2
/* The value has not changed.  */
#define WP_VALUE_NOT_CHANGED 3

/* Check watchpoint condition.  */
static int
watchpoint_check (p)
     char *p;
{
  bpstat bs = (bpstat) p;
  FRAME fr;

  int within_current_scope;
  if (bs->breakpoint_at->exp_valid_block == NULL)
    within_current_scope = 1;
  else
    {
      fr = within_scope (bs->breakpoint_at->exp_valid_block);
      within_current_scope = fr != NULL;
      if (within_current_scope)
	/* If we end up stopping, the current frame will get selected
	   in normal_stop.  So this call to select_frame won't affect
	   the user.  */
	select_frame (fr, -1);
    }
      
  if (within_current_scope)
    {
      /* We use value_{,free_to_}mark because it could be a
         *long* time before we return to the command level and
	 call free_all_values.  We can't call free_all_values because
	 we might be in the middle of evaluating a function call.  */

      value mark = value_mark ();
      value new_val = evaluate_expression (bs->breakpoint_at->exp);
      if (!value_equal (bs->breakpoint_at->val, new_val))
	{
	  release_value (new_val);
	  value_free_to_mark (mark);
	  bs->old_val = bs->breakpoint_at->val;
	  bs->breakpoint_at->val = new_val;
	  /* We will stop here */
	  return WP_VALUE_CHANGED;
	}
      else
	{
	  /* Nothing changed, don't do anything.  */
	  value_free_to_mark (mark);
	  /* We won't stop here */
	  return WP_VALUE_NOT_CHANGED;
	}
    }
  else
    {
      /* This seems like the only logical thing to do because
	 if we temporarily ignored the watchpoint, then when
	 we reenter the block in which it is valid it contains
	 garbage (in the case of a function, it may have two
	 garbage values, one before and one after the prologue).
	 So we can't even detect the first assignment to it and
	 watch after that (since the garbage may or may not equal
	 the first value assigned).  */
      bs->breakpoint_at->enable = disabled;
      printf_filtered ("\
Watchpoint %d disabled because the program has left the block in\n\
which its expression is valid.\n", bs->breakpoint_at->number);
      return WP_DISABLED;
    }
}

/* This is used when everything which needs to be printed has
   already been printed.  But we still want to print the frame.  */
static int
print_it_done (bs)
     bpstat bs;
{
  return 0;
}

/* This is used when nothing should be printed for this bpstat entry.  */

static int
print_it_noop (bs)
     bpstat bs;
{
  return -1;
}

/* Get a bpstat associated with having just stopped at address *PC
   and frame address FRAME_ADDRESS.  Update *PC to point at the
   breakpoint (if we hit a breakpoint).  NOT_A_BREAKPOINT is nonzero
   if this is known to not be a real breakpoint (it could still be a
   watchpoint, though).  */

/* Determine whether we stopped at a breakpoint, etc, or whether we
   don't understand this stop.  Result is a chain of bpstat's such that:

	if we don't understand the stop, the result is a null pointer.

	if we understand why we stopped, the result is not null.

	Each element of the chain refers to a particular breakpoint or
	watchpoint at which we have stopped.  (We may have stopped for
	several reasons concurrently.)

	Each element of the chain has valid next, breakpoint_at,
	commands, FIXME??? fields.

 */

bpstat
bpstat_stop_status (pc, frame_address, not_a_breakpoint)
     CORE_ADDR *pc;
     FRAME_ADDR frame_address;
     int not_a_breakpoint;
{
  register struct breakpoint *b;
  CORE_ADDR bp_addr;
#if DECR_PC_AFTER_BREAK != 0 || defined (SHIFT_INST_REGS)
  /* True if we've hit a breakpoint (as opposed to a watchpoint).  */
  int real_breakpoint = 0;
#endif
  /* Root of the chain of bpstat's */
  struct bpstat root_bs[1];
  /* Pointer to the last thing in the chain currently.  */
  bpstat bs = root_bs;

  /* Get the address where the breakpoint would have been.  */
  bp_addr = *pc - DECR_PC_AFTER_BREAK;

  ALL_BREAKPOINTS (b)
    {
      if (b->enable == disabled)
	continue;

      if (b->type != bp_watchpoint && b->address != bp_addr)
	continue;

      if (b->type != bp_watchpoint && not_a_breakpoint)
	continue;

      /* Come here if it's a watchpoint, or if the break address matches */

      bs = bpstat_alloc (b, bs);	/* Alloc a bpstat to explain stop */

      bs->stop = 1;
      bs->print = 1;

      if (b->type == bp_watchpoint)
	{
	  static char message1[] =
	    "Error evaluating expression for watchpoint %d\n";
	  char message[sizeof (message1) + 30 /* slop */];
	  sprintf (message, message1, b->number);
	  switch (catch_errors (watchpoint_check, (char *) bs, message,
				RETURN_MASK_ALL))
	    {
	    case WP_DISABLED:
	      /* We've already printed what needs to be printed.  */
	      bs->print_it = print_it_done;
	      /* Stop.  */
	      break;
	    case WP_VALUE_CHANGED:
	      /* Stop.  */
	      break;
	    case WP_VALUE_NOT_CHANGED:
	      /* Don't stop.  */
	      bs->print_it = print_it_noop;
	      bs->stop = 0;
	      continue;
	    default:
	      /* Can't happen.  */
	      /* FALLTHROUGH */
	    case 0:
	      /* Error from catch_errors.  */
	      b->enable = disabled;
	      printf_filtered ("Watchpoint %d disabled.\n", b->number);
	      /* We've already printed what needs to be printed.  */
	      bs->print_it = print_it_done;
	      /* Stop.  */
	      break;
	    }
	}
#if DECR_PC_AFTER_BREAK != 0 || defined (SHIFT_INST_REGS)
      else
	real_breakpoint = 1;
#endif

      if (b->frame && b->frame != frame_address)
	bs->stop = 0;
      else
	{
	  int value_is_zero = 0;

	  if (b->cond)
	    {
	      /* Need to select the frame, with all that implies
		 so that the conditions will have the right context.  */
	      select_frame (get_current_frame (), 0);
	      value_is_zero
		= catch_errors (breakpoint_cond_eval, (char *)(b->cond),
				"Error in testing breakpoint condition:\n",
				RETURN_MASK_ALL);
				/* FIXME-someday, should give breakpoint # */
	      free_all_values ();
	    }
	  if (b->cond && value_is_zero)
	    {
	      bs->stop = 0;
	    }
	  else if (b->ignore_count > 0)
	    {
	      b->ignore_count--;
	      bs->stop = 0;
	    }
	  else
	    {
	      /* We will stop here */
	      if (b->disposition == disable)
		b->enable = disabled;
	      bs->commands = b->commands;
	      if (b->silent)
		bs->print = 0;
	      if (bs->commands && STREQ ("silent", bs->commands->line))
		{
		  bs->commands = bs->commands->next;
		  bs->print = 0;
		}
	    }
	}
      /* Print nothing for this entry if we dont stop or if we dont print.  */
      if (bs->stop == 0 || bs->print == 0)
	bs->print_it = print_it_noop;
    }

  bs->next = NULL;		/* Terminate the chain */
  bs = root_bs->next;		/* Re-grab the head of the chain */
#if DECR_PC_AFTER_BREAK != 0 || defined (SHIFT_INST_REGS)
  if (bs)
    {
      if (real_breakpoint)
	{
	  *pc = bp_addr;
#if defined (SHIFT_INST_REGS)
	  SHIFT_INST_REGS();
#else /* No SHIFT_INST_REGS.  */
	  write_pc (bp_addr);
#endif /* No SHIFT_INST_REGS.  */
	}
    }
#endif /* DECR_PC_AFTER_BREAK != 0.  */
  return bs;
}

/* Tell what to do about this bpstat.  */
struct bpstat_what
bpstat_what (bs)
     bpstat bs;
{
  /* Classify each bpstat as one of the following.  */
  enum class {
    /* This bpstat element has no effect on the main_action.  */
    no_effect = 0,

    /* There was a watchpoint, stop but don't print.  */
    wp_silent,

    /* There was a watchpoint, stop and print.  */
    wp_noisy,

    /* There was a breakpoint but we're not stopping.  */
    bp_nostop,

    /* There was a breakpoint, stop but don't print.  */
    bp_silent,

    /* There was a breakpoint, stop and print.  */
    bp_noisy,

    /* We hit the longjmp breakpoint.  */
    long_jump,

    /* We hit the longjmp_resume breakpoint.  */
    long_resume,

    /* This is just used to count how many enums there are.  */
    class_last
    };

  /* Here is the table which drives this routine.  So that we can
     format it pretty, we define some abbreviations for the
     enum bpstat_what codes.  */
#define keep_c BPSTAT_WHAT_KEEP_CHECKING
#define stop_s BPSTAT_WHAT_STOP_SILENT
#define stop_n BPSTAT_WHAT_STOP_NOISY
#define single BPSTAT_WHAT_SINGLE
#define setlr BPSTAT_WHAT_SET_LONGJMP_RESUME
#define clrlr BPSTAT_WHAT_CLEAR_LONGJMP_RESUME
#define clrlrs BPSTAT_WHAT_CLEAR_LONGJMP_RESUME_SINGLE
/* "Can't happen."  Might want to print an error message.
   abort() is not out of the question, but chances are GDB is just
   a bit confused, not unusable.  */
#define err BPSTAT_WHAT_STOP_NOISY

  /* Given an old action and a class, come up with a new action.  */
  /* One interesting property of this table is that wp_silent is the same
     as bp_silent and wp_noisy is the same as bp_noisy.  That is because
     after stopping, the check for whether to step over a breakpoint
     (BPSTAT_WHAT_SINGLE type stuff) is handled in proceed() without
     reference to how we stopped.  We retain separate wp_silent and bp_silent
     codes in case we want to change that someday.  */
  static const enum bpstat_what_main_action
    table[(int)class_last][(int)BPSTAT_WHAT_LAST] =
      {
	/*                              old action */
	/*       keep_c  stop_s  stop_n  single  setlr   clrlr   clrlrs */

/*no_effect*/	{keep_c, stop_s, stop_n, single, setlr , clrlr , clrlrs},
/*wp_silent*/	{stop_s, stop_s, stop_n, stop_s, stop_s, stop_s, stop_s},
/*wp_noisy*/    {stop_n, stop_n, stop_n, stop_n, stop_n, stop_n, stop_n},
/*bp_nostop*/	{single, stop_s, stop_n, single, setlr , clrlrs, clrlrs},
/*bp_silent*/	{stop_s, stop_s, stop_n, stop_s, stop_s, stop_s, stop_s},
/*bp_noisy*/    {stop_n, stop_n, stop_n, stop_n, stop_n, stop_n, stop_n},
/*long_jump*/	{setlr , stop_s, stop_n, setlr , err   , err   , err   },
/*long_resume*/	{clrlr , stop_s, stop_n, clrlrs, err   , err   , err   }
	      };
#undef keep_c
#undef stop_s
#undef stop_n
#undef single
#undef setlr
#undef clrlr
#undef clrlrs
#undef err
  enum bpstat_what_main_action current_action = BPSTAT_WHAT_KEEP_CHECKING;
  struct bpstat_what retval;

  retval.call_dummy = 0;
  retval.step_resume = 0;
  for (; bs != NULL; bs = bs->next)
    {
      enum class bs_class = no_effect;
      if (bs->breakpoint_at == NULL)
	/* I suspect this can happen if it was a momentary breakpoint
	   which has since been deleted.  */
	continue;
      switch (bs->breakpoint_at->type)
	{
	case bp_breakpoint:
	case bp_until:
	case bp_finish:
	  if (bs->stop)
	    {
	      if (bs->print)
		bs_class = bp_noisy;
	      else
		bs_class = bp_silent;
	    }
	  else
	    bs_class = bp_nostop;
	  break;
	case bp_watchpoint:
	  if (bs->stop)
	    {
	      if (bs->print)
		bs_class = wp_noisy;
	      else
		bs_class = wp_silent;
	    }
	  else
	    /* There was a watchpoint, but we're not stopping.  This requires
	       no further action.  */
	    bs_class = no_effect;
	  break;
	case bp_longjmp:
	  bs_class = long_jump;
	  break;
	case bp_longjmp_resume:
	  bs_class = long_resume;
	  break;
	case bp_step_resume:
#if 0
	  /* Need to temporarily disable this until we can fix the bug
	     with nexting over a breakpoint with ->stop clear causing
	     an infinite loop.  For now, treat the breakpoint as having
	     been hit even if the frame is wrong.  */
	  if (bs->stop)
	    {
#endif
	      retval.step_resume = 1;
	      /* We don't handle this via the main_action.  */
	      bs_class = no_effect;
#if 0
	    }
	  else
	    /* It is for the wrong frame.  */
	    bs_class = bp_nostop;
#endif
	  break;
	case bp_call_dummy:
	  /* Make sure the action is stop (silent or noisy), so infrun.c
	     pops the dummy frame.  */
	  bs_class = bp_silent;
	  retval.call_dummy = 1;
	  break;
	}
      current_action = table[(int)bs_class][(int)current_action];
    }
  retval.main_action = current_action;
  return retval;
}

/* Nonzero if we should step constantly (e.g. watchpoints on machines
   without hardware support).  This isn't related to a specific bpstat,
   just to things like whether watchpoints are set.  */

int 
bpstat_should_step ()
{
  struct breakpoint *b;
  ALL_BREAKPOINTS (b)
    if (b->enable == enabled && b->type == bp_watchpoint)
      return 1;
  return 0;
}

/* Print information on breakpoint number BNUM, or -1 if all.
   If WATCHPOINTS is zero, process only breakpoints; if WATCHPOINTS
   is nonzero, process only watchpoints.  */

static void
breakpoint_1 (bnum, allflag)
     int bnum;
     int allflag;
{
  register struct breakpoint *b;
  register struct command_line *l;
  register struct symbol *sym;
  CORE_ADDR last_addr = (CORE_ADDR)-1;
  int found_a_breakpoint = 0;
  static char *bptypes[] = {"breakpoint", "until", "finish", "watchpoint",
			      "longjmp", "longjmp resume", "step resume",
			      "call dummy" };
  static char *bpdisps[] = {"del", "dis", "keep"};
  static char bpenables[] = "ny";
  char wrap_indent[80];

  ALL_BREAKPOINTS (b)
    if (bnum == -1
	|| bnum == b->number)
      {
/*  We only print out user settable breakpoints unless the allflag is set. */
	if (!allflag
	    && b->type != bp_breakpoint
	    && b->type != bp_watchpoint)
	  continue;

	if (!found_a_breakpoint++)
	  printf_filtered ("Num Type           Disp Enb %sWhat\n",
			   addressprint ? "Address    " : "");

	printf_filtered ("%-3d %-14s %-4s %-3c ",
			 b->number,
			 bptypes[(int)b->type],
			 bpdisps[(int)b->disposition],
			 bpenables[(int)b->enable]);
	strcpy (wrap_indent, "                           ");
	if (addressprint)
	  strcat (wrap_indent, "           ");
	switch (b->type)
	  {
	  case bp_watchpoint:
	    print_expression (b->exp, stdout);
	    break;

	  case bp_breakpoint:
	  case bp_until:
	  case bp_finish:
	  case bp_longjmp:
	  case bp_longjmp_resume:
	  case bp_step_resume:
	  case bp_call_dummy:
	    if (addressprint)
	      printf_filtered ("%s ", local_hex_string_custom ((unsigned long) b->address, "08l"));

	    last_addr = b->address;
	    if (b->source_file)
	      {
		sym = find_pc_function (b->address);
		if (sym)
		  {
		    fputs_filtered ("in ", stdout);
		    fputs_filtered (SYMBOL_SOURCE_NAME (sym), stdout);
		    wrap_here (wrap_indent);
		    fputs_filtered (" at ", stdout);
		  }
		fputs_filtered (b->source_file, stdout);
		printf_filtered (":%d", b->line_number);
	      }
	    else
	      print_address_symbolic (b->address, stdout, demangle, " ");
	    break;
	  }

	printf_filtered ("\n");

	if (b->frame)
	  printf_filtered ("\tstop only in stack frame at %s\n",
			   local_hex_string((unsigned long) b->frame));
	if (b->cond)
	  {
	    printf_filtered ("\tstop only if ");
	    print_expression (b->cond, stdout);
	    printf_filtered ("\n");
	  }
	if (b->ignore_count)
	  printf_filtered ("\tignore next %d hits\n", b->ignore_count);
	if ((l = b->commands))
	  while (l)
	    {
	      fputs_filtered ("\t", stdout);
	      fputs_filtered (l->line, stdout);
	      fputs_filtered ("\n", stdout);
	      l = l->next;
	    }
      }

  if (!found_a_breakpoint)
    {
      if (bnum == -1)
        printf_filtered ("No breakpoints or watchpoints.\n");
      else
        printf_filtered ("No breakpoint or watchpoint number %d.\n", bnum);
    }
  else
    /* Compare against (CORE_ADDR)-1 in case some compiler decides
       that a comparison of an unsigned with -1 is always false.  */
    if (last_addr != (CORE_ADDR)-1)
      set_next_address (last_addr);
}

/* ARGSUSED */
static void
breakpoints_info (bnum_exp, from_tty)
     char *bnum_exp;
     int from_tty;
{
  int bnum = -1;

  if (bnum_exp)
    bnum = parse_and_eval_address (bnum_exp);

  breakpoint_1 (bnum, 0);
}

#if MAINTENANCE_CMDS

/* ARGSUSED */
static void
maintenance_info_breakpoints (bnum_exp, from_tty)
     char *bnum_exp;
     int from_tty;
{
  int bnum = -1;

  if (bnum_exp)
    bnum = parse_and_eval_address (bnum_exp);

  breakpoint_1 (bnum, 1);
}

#endif

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
      printf ("also set at pc %s.\n", local_hex_string((unsigned long) pc));
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

  if (address == 0)		/* Watchpoints are uninteresting */
    return;

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
   Does not print anything.

   ==> This routine should not be called if there is a chance of later
   error(); otherwise it leaves a bogus breakpoint on the chain.  Validate
   your arguments BEFORE calling this routine!  */

static struct breakpoint *
set_raw_breakpoint (sal)
     struct symtab_and_line sal;
{
  register struct breakpoint *b, *b1;

  b = (struct breakpoint *) xmalloc (sizeof (struct breakpoint));
  memset (b, 0, sizeof (*b));
  b->address = sal.pc;
  if (sal.symtab == NULL)
    b->source_file = NULL;
  else
    b->source_file = savestring (sal.symtab->filename,
				 strlen (sal.symtab->filename));
  b->thread = -1;
  b->line_number = sal.line;
  b->enable = enabled;
  b->next = 0;
  b->silent = 0;
  b->ignore_count = 0;
  b->commands = NULL;
  b->frame = 0;

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

static void
create_longjmp_breakpoint(func_name)
     char *func_name;
{
  struct symtab_and_line sal;
  struct breakpoint *b;
  static int internal_breakpoint_number = -1;

  if (func_name != NULL)
    {
      struct minimal_symbol *m;

      m = lookup_minimal_symbol(func_name, (struct objfile *)NULL);
      if (m)
	sal.pc = SYMBOL_VALUE_ADDRESS (m);
      else
	return;
    }
  else
    sal.pc = 0;

  sal.symtab = NULL;
  sal.line = 0;

  b = set_raw_breakpoint(sal);
  if (!b) return;

  b->type = func_name != NULL ? bp_longjmp : bp_longjmp_resume;
  b->disposition = donttouch;
  b->enable = disabled;
  b->silent = 1;
  if (func_name)
    b->addr_string = strsave(func_name);
  b->number = internal_breakpoint_number--;
}

/* Call this routine when stepping and nexting to enable a breakpoint if we do
   a longjmp().  When we hit that breakpoint, call
   set_longjmp_resume_breakpoint() to figure out where we are going. */

void
enable_longjmp_breakpoint()
{
  register struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->type == bp_longjmp)
      {
	b->enable = enabled;
	check_duplicates (b->address);
      }
}

void
disable_longjmp_breakpoint()
{
  register struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (   b->type == bp_longjmp
	|| b->type == bp_longjmp_resume)
      {
	b->enable = disabled;
	check_duplicates (b->address);
      }
}

/* Call this after hitting the longjmp() breakpoint.  Use this to set a new
   breakpoint at the target of the jmp_buf.

   FIXME - This ought to be done by setting a temporary breakpoint that gets
   deleted automatically...
*/

void
set_longjmp_resume_breakpoint(pc, frame)
     CORE_ADDR pc;
     FRAME frame;
{
  register struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    if (b->type == bp_longjmp_resume)
      {
	b->address = pc;
	b->enable = enabled;
	if (frame != NULL)
	  b->frame = FRAME_FP(frame);
	else
	  b->frame = 0;
	check_duplicates (b->address);
	return;
      }
}

/* Set a breakpoint that will evaporate an end of command
   at address specified by SAL.
   Restrict it to frame FRAME if FRAME is nonzero.  */

struct breakpoint *
set_momentary_breakpoint (sal, frame, type)
     struct symtab_and_line sal;
     FRAME frame;
     enum bptype type;
{
  register struct breakpoint *b;
  b = set_raw_breakpoint (sal);
  b->type = type;
  b->enable = enabled;
  b->disposition = donttouch;
  b->frame = (frame ? FRAME_FP (frame) : 0);
  return b;
}

#if 0
void
clear_momentary_breakpoints ()
{
  register struct breakpoint *b;
  ALL_BREAKPOINTS (b)
    if (b->disposition == delete)
      {
	delete_breakpoint (b);
	break;
      }
}
#endif

/* Tell the user we have just set a breakpoint B.  */
static void
mention (b)
     struct breakpoint *b;
{
  switch (b->type)
    {
    case bp_watchpoint:
      printf_filtered ("Watchpoint %d: ", b->number);
      print_expression (b->exp, stdout);
      break;
    case bp_breakpoint:
      printf_filtered ("Breakpoint %d at %s", b->number,
		       local_hex_string((unsigned long) b->address));
      if (b->source_file)
	printf_filtered (": file %s, line %d.",
			 b->source_file, b->line_number);
      break;
    case bp_until:
    case bp_finish:
    case bp_longjmp:
    case bp_longjmp_resume:
    case bp_step_resume:
      break;
    }
  printf_filtered ("\n");
}

#if 0
/* Nobody calls this currently. */
/* Set a breakpoint from a symtab and line.
   If TEMPFLAG is nonzero, it is a temporary breakpoint.
   ADDR_STRING is a malloc'd string holding the name of where we are
   setting the breakpoint.  This is used later to re-set it after the
   program is relinked and symbols are reloaded.
   Print the same confirmation messages that the breakpoint command prints.  */

void
set_breakpoint (s, line, tempflag, addr_string)
     struct symtab *s;
     int line;
     int tempflag;
     char *addr_string;
{
  register struct breakpoint *b;
  struct symtab_and_line sal;
  
  sal.symtab = s;
  sal.line = line;
  sal.pc = 0;
  resolve_sal_pc (&sal);			/* Might error out */
  describe_other_breakpoints (sal.pc);

  b = set_raw_breakpoint (sal);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->type = bp_breakpoint;
  b->cond = 0;
  b->addr_string = addr_string;
  b->enable = enabled;
  b->disposition = tempflag ? delete : donttouch;

  mention (b);
}
#endif /* 0 */

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

  /* Pointers in arg to the start, and one past the end, of the condition.  */
  char *cond_start = NULL;
  char *cond_end = NULL;
  /* Pointers in arg to the start, and one past the end,
     of the address part.  */
  char *addr_start = NULL;
  char *addr_end = NULL;
  struct cleanup *old_chain;
  struct cleanup *canonical_strings_chain = NULL;
  char **canonical = (char **)NULL;
  int i;
  int thread;

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
	    xmalloc (sizeof (struct symtab_and_line));
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
    {
      addr_start = arg;

      /* Force almost all breakpoints to be in terms of the
	 current_source_symtab (which is decode_line_1's default).  This
	 should produce the results we want almost all of the time while
	 leaving default_breakpoint_* alone.  */
      if (default_breakpoint_valid
	  && (!current_source_symtab
	      || (arg && (*arg == '+' || *arg == '-'))))
	sals = decode_line_1 (&arg, 1, default_breakpoint_symtab,
			      default_breakpoint_line, &canonical);
      else
	sals = decode_line_1 (&arg, 1, (struct symtab *)NULL, 0, &canonical);

      addr_end = arg;
    }
  
  if (! sals.nelts) 
    return;

  /* Make sure that all storage allocated in decode_line_1 gets freed in case
     the following `for' loop errors out.  */
  old_chain = make_cleanup (free, sals.sals);
  if (canonical != (char **)NULL)
    {
      make_cleanup (free, canonical);
      canonical_strings_chain = make_cleanup (null_cleanup, 0);
      for (i = 0; i < sals.nelts; i++)
	{
	  if (canonical[i] != NULL)
	    make_cleanup (free, canonical[i]);
	}
    }

  thread = -1;			/* No specific thread yet */

  /* Resolve all line numbers to PC's, and verify that conditions
     can be parsed, before setting any breakpoints.  */
  for (i = 0; i < sals.nelts; i++)
    {
      char *tok, *end_tok;
      int toklen;

      resolve_sal_pc (&sals.sals[i]);
      
      tok = arg;

      while (tok && *tok)
	{
	  while (*tok == ' ' || *tok == '\t')
	    tok++;

	  end_tok = tok;

	  while (*end_tok != ' ' && *end_tok != '\t' && *end_tok != '\000')
	    end_tok++;

	  toklen = end_tok - tok;

	  if (toklen >= 1 && strncmp (tok, "if", toklen) == 0)
	    {
	      tok = cond_start = end_tok + 1;
	      cond = parse_exp_1 (&tok, block_for_pc (sals.sals[i].pc), 0);
	      cond_end = tok;
	    }
	  else if (toklen >= 1 && strncmp (tok, "thread", toklen) == 0)
	    {
	      char *tmptok;

	      tok = end_tok + 1;
	      tmptok = tok;
	      thread = strtol (tok, &tok, 0);
	      if (tok == tmptok)
		error ("Junk after thread keyword.");
	      if (!valid_thread_id (thread))
		error ("Unknown thread %d\n", thread);
	    }
	  else
	    error ("Junk at end of arguments.");
	}
    }

  /* Remove the canonical strings from the cleanup, they are needed below.  */
  if (canonical != (char **)NULL)
    discard_cleanups (canonical_strings_chain);

  /* Now set all the breakpoints.  */
  for (i = 0; i < sals.nelts; i++)
    {
      sal = sals.sals[i];

      if (from_tty)
	describe_other_breakpoints (sal.pc);

      b = set_raw_breakpoint (sal);
      set_breakpoint_count (breakpoint_count + 1);
      b->number = breakpoint_count;
      b->type = bp_breakpoint;
      b->cond = cond;
      b->thread = thread;

      /* If a canonical line spec is needed use that instead of the
	 command string.  */
      if (canonical != (char **)NULL && canonical[i] != NULL)
	b->addr_string = canonical[i];
      else if (addr_start)
	b->addr_string = savestring (addr_start, addr_end - addr_start);
      if (cond_start)
	b->cond_string = savestring (cond_start, cond_end - cond_start);
				     
      b->enable = enabled;
      b->disposition = tempflag ? delete : donttouch;

      mention (b);
    }

  if (sals.nelts > 1)
    {
      printf ("Multiple breakpoints were set.\n");
      printf ("Use the \"delete\" command to delete unwanted breakpoints.\n");
    }
  do_cleanups (old_chain);
}

/* Helper function for break_command_1 and disassemble_command.  */

void
resolve_sal_pc (sal)
     struct symtab_and_line *sal;
{
  CORE_ADDR pc;

  if (sal->pc == 0 && sal->symtab != 0)
    {
      pc = find_line_pc (sal->symtab, sal->line);
      if (pc == 0)
	error ("No line %d in file \"%s\".",
	       sal->line, sal->symtab->filename);
      sal->pc = pc;
    }
}

void
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

/* ARGSUSED */
static void
watch_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct breakpoint *b;
  struct symtab_and_line sal;
  struct expression *exp;
  struct block *exp_valid_block;
  struct value *val;

  sal.pc = 0;
  sal.symtab = NULL;
  sal.line = 0;
  
  /* Parse arguments.  */
  innermost_block = NULL;
  exp = parse_expression (arg);
  exp_valid_block = innermost_block;
  val = evaluate_expression (exp);
  release_value (val);
  if (VALUE_LAZY (val))
    value_fetch_lazy (val);

  /* Now set up the breakpoint.  */
  b = set_raw_breakpoint (sal);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->type = bp_watchpoint;
  b->disposition = donttouch;
  b->exp = exp;
  b->exp_valid_block = exp_valid_block;
  b->val = val;
  b->cond = 0;
  b->cond_string = NULL;
  b->exp_string = savestring (arg, strlen (arg));
  mention (b);
}

/*
 * Helper routine for the until_command routine in infcmd.c.  Here
 * because it uses the mechanisms of breakpoints.
 */
/* ARGSUSED */
void
until_break_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  FRAME prev_frame = get_prev_frame (selected_frame);
  struct breakpoint *breakpoint;
  struct cleanup *old_chain;

  clear_proceed_status ();

  /* Set a breakpoint where the user wants it and at return from
     this function */
  
  if (default_breakpoint_valid)
    sals = decode_line_1 (&arg, 1, default_breakpoint_symtab,
			  default_breakpoint_line, (char ***)NULL);
  else
    sals = decode_line_1 (&arg, 1, (struct symtab *)NULL, 0, (char ***)NULL);
  
  if (sals.nelts != 1)
    error ("Couldn't get information on specified line.");
  
  sal = sals.sals[0];
  free ((PTR)sals.sals);		/* malloc'd, so freed */
  
  if (*arg)
    error ("Junk at end of arguments.");
  
  resolve_sal_pc (&sal);
  
  breakpoint = set_momentary_breakpoint (sal, selected_frame, bp_until);
  
  old_chain = make_cleanup(delete_breakpoint, breakpoint);

  /* Keep within the current frame */
  
  if (prev_frame)
    {
      struct frame_info *fi;
      
      fi = get_frame_info (prev_frame);
      sal = find_pc_line (fi->pc, 0);
      sal.pc = fi->pc;
      breakpoint = set_momentary_breakpoint (sal, prev_frame, bp_until);
      make_cleanup(delete_breakpoint, breakpoint);
    }
  
  proceed (-1, -1, 0);
  do_cleanups(old_chain);
}

#if 0
/* These aren't used; I don't konw what they were for.  */
/* Set a breakpoint at the catch clause for NAME.  */
static int
catch_breakpoint (name)
     char *name;
{
}

static int
disable_catch_breakpoint ()
{
}

static int
delete_catch_breakpoint ()
{
}

static int
enable_catch_breakpoint ()
{
}
#endif /* 0 */

struct sal_chain
{
  struct sal_chain *next;
  struct symtab_and_line sal;
};

#if 0
/* This isn't used; I don't know what it was for.  */
/* For each catch clause identified in ARGS, run FUNCTION
   with that clause as an argument.  */
static struct symtabs_and_lines
map_catch_names (args, function)
     char *args;
     int (*function)();
{
  register char *p = args;
  register char *p1;
  struct symtabs_and_lines sals;
#if 0
  struct sal_chain *sal_chain = 0;
#endif

  if (p == 0)
    error_no_arg ("one or more catch names");

  sals.nelts = 0;
  sals.sals = NULL;

  while (*p)
    {
      p1 = p;
      /* Don't swallow conditional part.  */
      if (p1[0] == 'i' && p1[1] == 'f'
	  && (p1[2] == ' ' || p1[2] == '\t'))
	break;

      if (isalpha (*p1))
	{
	  p1++;
	  while (isalnum (*p1) || *p1 == '_' || *p1 == '$')
	    p1++;
	}

      if (*p1 && *p1 != ' ' && *p1 != '\t')
	error ("Arguments must be catch names.");

      *p1 = 0;
#if 0
      if (function (p))
	{
	  struct sal_chain *next
	    = (struct sal_chain *)alloca (sizeof (struct sal_chain));
	  next->next = sal_chain;
	  next->sal = get_catch_sal (p);
	  sal_chain = next;
	  goto win;
	}
#endif
      printf ("No catch clause for exception %s.\n", p);
#if 0
    win:
#endif
      p = p1;
      while (*p == ' ' || *p == '\t') p++;
    }
}
#endif /* 0 */

/* This shares a lot of code with `print_frame_label_vars' from stack.c.  */

static struct symtabs_and_lines
get_catch_sals (this_level_only)
     int this_level_only;
{
  register struct blockvector *bl;
  register struct block *block;
  int index, have_default = 0;
  struct frame_info *fi;
  CORE_ADDR pc;
  struct symtabs_and_lines sals;
  struct sal_chain *sal_chain = 0;
  char *blocks_searched;

  /* Not sure whether an error message is always the correct response,
     but it's better than a core dump.  */
  if (selected_frame == NULL)
    error ("No selected frame.");
  block = get_frame_block (selected_frame);
  fi = get_frame_info (selected_frame);
  pc = fi->pc;

  sals.nelts = 0;
  sals.sals = NULL;

  if (block == 0)
    error ("No symbol table info available.\n");

  bl = blockvector_for_pc (BLOCK_END (block) - 4, &index);
  blocks_searched = (char *) alloca (BLOCKVECTOR_NBLOCKS (bl) * sizeof (char));
  memset (blocks_searched, 0, BLOCKVECTOR_NBLOCKS (bl) * sizeof (char));

  while (block != 0)
    {
      CORE_ADDR end = BLOCK_END (block) - 4;
      int last_index;

      if (bl != blockvector_for_pc (end, &index))
	error ("blockvector blotch");
      if (BLOCKVECTOR_BLOCK (bl, index) != block)
	error ("blockvector botch");
      last_index = BLOCKVECTOR_NBLOCKS (bl);
      index += 1;

      /* Don't print out blocks that have gone by.  */
      while (index < last_index
	     && BLOCK_END (BLOCKVECTOR_BLOCK (bl, index)) < pc)
	index++;

      while (index < last_index
	     && BLOCK_END (BLOCKVECTOR_BLOCK (bl, index)) < end)
	{
	  if (blocks_searched[index] == 0)
	    {
	      struct block *b = BLOCKVECTOR_BLOCK (bl, index);
	      int nsyms;
	      register int i;
	      register struct symbol *sym;

	      nsyms = BLOCK_NSYMS (b);

	      for (i = 0; i < nsyms; i++)
		{
		  sym = BLOCK_SYM (b, i);
		  if (STREQ (SYMBOL_NAME (sym), "default"))
		    {
		      if (have_default)
			continue;
		      have_default = 1;
		    }
		  if (SYMBOL_CLASS (sym) == LOC_LABEL)
		    {
		      struct sal_chain *next = (struct sal_chain *)
			alloca (sizeof (struct sal_chain));
		      next->next = sal_chain;
		      next->sal = find_pc_line (SYMBOL_VALUE_ADDRESS (sym), 0);
		      sal_chain = next;
		    }
		}
	      blocks_searched[index] = 1;
	    }
	  index++;
	}
      if (have_default)
	break;
      if (sal_chain && this_level_only)
	break;

      /* After handling the function's top-level block, stop.
	 Don't continue to its superblock, the block of
	 per-file symbols.  */
      if (BLOCK_FUNCTION (block))
	break;
      block = BLOCK_SUPERBLOCK (block);
    }

  if (sal_chain)
    {
      struct sal_chain *tmp_chain;

      /* Count the number of entries.  */
      for (index = 0, tmp_chain = sal_chain; tmp_chain;
	   tmp_chain = tmp_chain->next)
	index++;

      sals.nelts = index;
      sals.sals = (struct symtab_and_line *)
	xmalloc (index * sizeof (struct symtab_and_line));
      for (index = 0; sal_chain; sal_chain = sal_chain->next, index++)
	sals.sals[index] = sal_chain->sal;
    }

  return sals;
}

/* Commands to deal with catching exceptions.  */

static void
catch_command_1 (arg, tempflag, from_tty)
     char *arg;
     int tempflag;
     int from_tty;
{
  /* First, translate ARG into something we can deal with in terms
     of breakpoints.  */

  struct symtabs_and_lines sals;
  struct symtab_and_line sal;
  register struct expression *cond = 0;
  register struct breakpoint *b;
  char *save_arg;
  int i;

  sal.line = sal.pc = sal.end = 0;
  sal.symtab = 0;

  /* If no arg given, or if first arg is 'if ', all active catch clauses
     are breakpointed. */

  if (!arg || (arg[0] == 'i' && arg[1] == 'f' 
	       && (arg[2] == ' ' || arg[2] == '\t')))
    {
      /* Grab all active catch clauses.  */
      sals = get_catch_sals (0);
    }
  else
    {
      /* Grab selected catch clauses.  */
      error ("catch NAME not implemented");
#if 0
      /* This isn't used; I don't know what it was for.  */
      sals = map_catch_names (arg, catch_breakpoint);
#endif
    }

  if (! sals.nelts) 
    return;

  save_arg = arg;
  for (i = 0; i < sals.nelts; i++)
    {
      resolve_sal_pc (&sals.sals[i]);
      
      while (arg && *arg)
	{
	  if (arg[0] == 'i' && arg[1] == 'f'
	      && (arg[2] == ' ' || arg[2] == '\t'))
	    cond = parse_exp_1 ((arg += 2, &arg), 
				block_for_pc (sals.sals[i].pc), 0);
	  else
	    error ("Junk at end of arguments.");
	}
      arg = save_arg;
    }

  for (i = 0; i < sals.nelts; i++)
    {
      sal = sals.sals[i];

      if (from_tty)
	describe_other_breakpoints (sal.pc);

      b = set_raw_breakpoint (sal);
      set_breakpoint_count (breakpoint_count + 1);
      b->number = breakpoint_count;
      b->type = bp_breakpoint;
      b->cond = cond;
      b->enable = enabled;
      b->disposition = tempflag ? delete : donttouch;

      mention (b);
    }

  if (sals.nelts > 1)
    {
      printf ("Multiple breakpoints were set.\n");
      printf ("Use the \"delete\" command to delete unwanted breakpoints.\n");
    }
  free ((PTR)sals.sals);
}

#if 0
/* These aren't used; I don't know what they were for.  */
/* Disable breakpoints on all catch clauses described in ARGS.  */
static void
disable_catch (args)
     char *args;
{
  /* Map the disable command to catch clauses described in ARGS.  */
}

/* Enable breakpoints on all catch clauses described in ARGS.  */
static void
enable_catch (args)
     char *args;
{
  /* Map the disable command to catch clauses described in ARGS.  */
}

/* Delete breakpoints on all catch clauses in the active scope.  */
static void
delete_catch (args)
     char *args;
{
  /* Map the delete command to catch clauses described in ARGS.  */
}
#endif /* 0 */

static void
catch_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  catch_command_1 (arg, 0, from_tty);
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
      sals.sals = (struct symtab_and_line *) xmalloc (sizeof (struct symtab_and_line));
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
	     && (sal.pc
		 ? breakpoint_chain->address == sal.pc
		 : (breakpoint_chain->source_file != NULL
		    && sal.symtab != NULL
		    && STREQ (breakpoint_chain->source_file,
			      sal.symtab->filename)
		    && breakpoint_chain->line_number == sal.line)))
	{
	  b1 = breakpoint_chain;
	  breakpoint_chain = b1->next;
	  b1->next = found;
	  found = b1;
	}

      ALL_BREAKPOINTS (b)
	while (b->next
	       && b->next->type != bp_watchpoint
	       && (sal.pc
		   ? b->next->address == sal.pc
		   : (b->next->source_file != NULL
		      && sal.symtab != NULL
		      && STREQ (b->next->source_file, sal.symtab->filename)
		      && b->next->line_number == sal.line)))
	  {
	    b1 = b->next;
	    b->next = b1->next;
	    b1->next = found;
	    found = b1;
	  }

      if (found == 0)
	{
	  if (arg)
	    error ("No breakpoint at %s.", arg);
	  else
	    error ("No breakpoint at this line.");
	}

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
  free ((PTR)sals.sals);
}

/* Delete breakpoint in BS if they are `delete' breakpoints.
   This is called after any breakpoint is hit, or after errors.  */

void
breakpoint_auto_delete (bs)
     bpstat bs;
{
  for (; bs; bs = bs->next)
    if (bs->breakpoint_at && bs->breakpoint_at->disposition == delete
	&& bs->stop)
      delete_breakpoint (bs->breakpoint_at);
}

/* Delete a breakpoint and clean up all traces of it in the data structures. */

void
delete_breakpoint (bpt)
     struct breakpoint *bpt;
{
  register struct breakpoint *b;
  register bpstat bs;

  if (bpt->inserted)
    target_remove_breakpoint(bpt->address, bpt->shadow_contents);

  if (breakpoint_chain == bpt)
    breakpoint_chain = bpt->next;

  ALL_BREAKPOINTS (b)
    if (b->next == bpt)
      {
	b->next = bpt->next;
	break;
      }

  check_duplicates (bpt->address);
  /* If this breakpoint was inserted, and there is another breakpoint
     at the same address, we need to insert the other breakpoint.  */
  if (bpt->inserted)
    {
      ALL_BREAKPOINTS (b)
	if (b->address == bpt->address
	    && !b->duplicate
	    && b->enable != disabled)
	  {
	    int val;
	    val = target_insert_breakpoint (b->address, b->shadow_contents);
	    if (val != 0)
	      {
		fprintf (stderr, "Cannot insert breakpoint %d:\n", b->number);
		memory_error (val, b->address);	/* which bombs us out */
	      }
	    else
	      b->inserted = 1;
	  }
    }

  free_command_lines (&bpt->commands);
  if (bpt->cond)
    free (bpt->cond);
  if (bpt->cond_string != NULL)
    free (bpt->cond_string);
  if (bpt->addr_string != NULL)
    free (bpt->addr_string);
  if (bpt->exp_string != NULL)
    free (bpt->exp_string);
  if (bpt->source_file != NULL)
    free (bpt->source_file);

  if (xgdb_verbose && bpt->type == bp_breakpoint)
    printf ("breakpoint #%d deleted\n", bpt->number);

  /* Be sure no bpstat's are pointing at it after it's been freed.  */
  /* FIXME, how can we find all bpstat's?  We just check stop_bpstat for now. */
  for (bs = stop_bpstat; bs; bs = bs->next)
    if (bs->breakpoint_at == bpt)
      bs->breakpoint_at = NULL;
  free ((PTR)bpt);
}

static void
delete_command (arg, from_tty)
     char *arg;
     int from_tty;
{

  if (arg == 0)
    {
      /* Ask user only if there are some breakpoints to delete.  */
      if (!from_tty
	  || (breakpoint_chain && query ("Delete all breakpoints? ", 0, 0)))
	{
	  /* No arg; clear all breakpoints.  */
	  while (breakpoint_chain)
	    delete_breakpoint (breakpoint_chain);
	}
    }
  else
    map_breakpoint_numbers (arg, delete_breakpoint);
}

/* Reset a breakpoint given it's struct breakpoint * BINT.
   The value we return ends up being the return value from catch_errors.
   Unused in this case.  */

static int
breakpoint_re_set_one (bint)
     char *bint;
{
  struct breakpoint *b = (struct breakpoint *)bint;  /* get past catch_errs */
  int i;
  struct symtabs_and_lines sals;
  char *s;
  enum enable save_enable;

  switch (b->type)
    {
    case bp_breakpoint:
      if (b->addr_string == NULL)
	{
	  /* Anything without a string can't be re-set. */
	  delete_breakpoint (b);
	  return 0;
	}
      /* In case we have a problem, disable this breakpoint.  We'll restore
	 its status if we succeed.  */
      save_enable = b->enable;
      b->enable = disabled;

      s = b->addr_string;
      sals = decode_line_1 (&s, 1, (struct symtab *)NULL, 0, (char ***)NULL);
      for (i = 0; i < sals.nelts; i++)
	{
	  resolve_sal_pc (&sals.sals[i]);

	  /* Reparse conditions, they might contain references to the
	     old symtab.  */
	  if (b->cond_string != NULL)
	    {
	      s = b->cond_string;
	      if (b->cond)
		free ((PTR)b->cond);
	      b->cond = parse_exp_1 (&s, block_for_pc (sals.sals[i].pc), 0);
	    }

	  /* We need to re-set the breakpoint if the address changes...*/
	  if (b->address != sals.sals[i].pc
	      /* ...or new and old breakpoints both have source files, and
		 the source file name or the line number changes...  */
	      || (b->source_file != NULL
		  && sals.sals[i].symtab != NULL
		  && (!STREQ (b->source_file, sals.sals[i].symtab->filename)
		      || b->line_number != sals.sals[i].line)
		  )
	      /* ...or we switch between having a source file and not having
		 one.  */
	      || ((b->source_file == NULL) != (sals.sals[i].symtab == NULL))
	      )
	    {
	      if (b->source_file != NULL)
		free (b->source_file);
	      if (sals.sals[i].symtab == NULL)
		b->source_file = NULL;
	      else
		b->source_file =
		  savestring (sals.sals[i].symtab->filename,
			      strlen (sals.sals[i].symtab->filename));
	      b->line_number = sals.sals[i].line;
	      b->address = sals.sals[i].pc;
	  
	      check_duplicates (b->address);

	      mention (b);
	    }
	  b->enable = save_enable;	/* Restore it, this worked. */
	}
      free ((PTR)sals.sals);
      break;

    case bp_watchpoint:
      innermost_block = NULL;
      /* The issue arises of what context to evaluate this in.  The same
	 one as when it was set, but what does that mean when symbols have
	 been re-read?  We could save the filename and functionname, but
	 if the context is more local than that, the best we could do would
	 be something like how many levels deep and which index at that
	 particular level, but that's going to be less stable than filenames
	 or functionnames.  */
      /* So for now, just use a global context.  */
      b->exp = parse_expression (b->exp_string);
      b->exp_valid_block = innermost_block;
      b->val = evaluate_expression (b->exp);
      release_value (b->val);
      if (VALUE_LAZY (b->val))
	value_fetch_lazy (b->val);

      if (b->cond_string != NULL)
	{
	  s = b->cond_string;
	  b->cond = parse_exp_1 (&s, (struct block *)0, 0);
	}
      if (b->enable == enabled)
	mention (b);
      break;

    default:
      printf_filtered ("Deleting unknown breakpoint type %d\n", b->type);
      /* fall through */
    case bp_until:
    case bp_finish:
    case bp_longjmp:
    case bp_longjmp_resume:
    case bp_call_dummy:
      delete_breakpoint (b);
      break;
    }

  return 0;
}

/* Re-set all breakpoints after symbols have been re-loaded.  */
void
breakpoint_re_set ()
{
  struct breakpoint *b, *temp;
  static char message1[] = "Error in re-setting breakpoint %d:\n";
  char message[sizeof (message1) + 30 /* slop */];
  
  ALL_BREAKPOINTS_SAFE (b, temp)
    {
      sprintf (message, message1, b->number);	/* Format possible error msg */
      catch_errors (breakpoint_re_set_one, (char *) b, message,
		    RETURN_MASK_ALL);
    }

  create_longjmp_breakpoint("longjmp");
  create_longjmp_breakpoint("_longjmp");
  create_longjmp_breakpoint("siglongjmp");
  create_longjmp_breakpoint(NULL);

#if 0
  /* Took this out (temporaliy at least), since it produces an extra 
     blank line at startup. This messes up the gdbtests. -PB */
  /* Blank line to finish off all those mention() messages we just printed.  */
  printf_filtered ("\n");
#endif
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
	  printf_filtered ("Will stop next time breakpoint %d is reached.",
			   bptnum);
	else if (count == 1)
	  printf_filtered ("Will ignore next crossing of breakpoint %d.",
			   bptnum);
	else
	  printf_filtered ("Will ignore next %d crossings of breakpoint %d.",
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
  char *p = args;
  register int num;

  if (p == 0)
    error_no_arg ("a breakpoint number");
  
  num = get_number (&p);

  if (*p == 0)
    error ("Second argument (specified ignore-count) is missing.");

  set_ignore_count (num,
		    longest_to_int (value_as_long (parse_and_eval (p))),
		    from_tty);
  printf_filtered ("\n");
}

/* Call FUNCTION on each of the breakpoints
   whose numbers are given in ARGS.  */

static void
map_breakpoint_numbers (args, function)
     char *args;
     void (*function) PARAMS ((struct breakpoint *));
{
  register char *p = args;
  char *p1;
  register int num;
  register struct breakpoint *b;

  if (p == 0)
    error_no_arg ("one or more breakpoint numbers");

  while (*p)
    {
      p1 = p;
      
      num = get_number (&p1);

      ALL_BREAKPOINTS (b)
	if (b->number == num)
	  {
	    function (b);
	    goto win;
	  }
      printf ("No breakpoint number %d.\n", num);
    win:
      p = p1;
    }
}

static void
enable_breakpoint (bpt)
     struct breakpoint *bpt;
{
  FRAME save_selected_frame = NULL;
  int save_selected_frame_level = -1;
  
  bpt->enable = enabled;

  if (xgdb_verbose && bpt->type == bp_breakpoint)
    printf ("breakpoint #%d enabled\n", bpt->number);

  check_duplicates (bpt->address);
  if (bpt->type == bp_watchpoint)
    {
      if (bpt->exp_valid_block != NULL)
	{
	  FRAME fr = within_scope (bpt->exp_valid_block);
	  if (fr == NULL)
	    {
	      printf_filtered ("\
Cannot enable watchpoint %d because the block in which its expression\n\
is valid is not currently in scope.\n", bpt->number);
	      bpt->enable = disabled;
	      return;
	    }
	  save_selected_frame = selected_frame;
	  save_selected_frame_level = selected_frame_level;
	  select_frame (fr, -1);
	}

      value_free (bpt->val);

      bpt->val = evaluate_expression (bpt->exp);
      release_value (bpt->val);
      if (VALUE_LAZY (bpt->val))
	value_fetch_lazy (bpt->val);

      if (save_selected_frame_level >= 0)
	select_frame (save_selected_frame, save_selected_frame_level);
    }
}

/* ARGSUSED */
static void
enable_command (args, from_tty)
     char *args;
     int from_tty;
{
  struct breakpoint *bpt;
  if (args == 0)
    ALL_BREAKPOINTS (bpt)
      switch (bpt->type)
	{
	case bp_breakpoint:
	case bp_watchpoint:
	  enable_breakpoint (bpt);
	default:
	  continue;
	}
  else
    map_breakpoint_numbers (args, enable_breakpoint);
}

static void
disable_breakpoint (bpt)
     struct breakpoint *bpt;
{
  bpt->enable = disabled;

  if (xgdb_verbose && bpt->type == bp_breakpoint)
    printf_filtered ("breakpoint #%d disabled\n", bpt->number);

  check_duplicates (bpt->address);
}

/* ARGSUSED */
static void
disable_command (args, from_tty)
     char *args;
     int from_tty;
{
  register struct breakpoint *bpt;
  if (args == 0)
    ALL_BREAKPOINTS (bpt)
      switch (bpt->type)
	{
	case bp_breakpoint:
	case bp_watchpoint:
	  disable_breakpoint (bpt);
	default:
	  continue;
	}
  else
    map_breakpoint_numbers (args, disable_breakpoint);
}

static void
enable_once_breakpoint (bpt)
     struct breakpoint *bpt;
{
  bpt->enable = enabled;
  bpt->disposition = disable;

  check_duplicates (bpt->address);
}

/* ARGSUSED */
static void
enable_once_command (args, from_tty)
     char *args;
     int from_tty;
{
  map_breakpoint_numbers (args, enable_once_breakpoint);
}

static void
enable_delete_breakpoint (bpt)
     struct breakpoint *bpt;
{
  bpt->enable = enabled;
  bpt->disposition = delete;

  check_duplicates (bpt->address);
}

/* ARGSUSED */
static void
enable_delete_command (args, from_tty)
     char *args;
     int from_tty;
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
			  default_breakpoint_symtab, default_breakpoint_line,
			  (char ***)NULL);
  else
    sals = decode_line_1 (&string, funfirstline,
			  (struct symtab *)NULL, 0, (char ***)NULL);
  if (*string)
    error ("Junk at end of line specification: %s", string);
  return sals;
}

void
_initialize_breakpoint ()
{
  breakpoint_chain = 0;
  /* Don't bother to call set_breakpoint_count.  $bpnum isn't useful
     before a breakpoint is set.  */
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
N is an integer; COND is an expression to be evaluated whenever\n\
breakpoint N is reached.  ");

  add_com ("tbreak", class_breakpoint, tbreak_command,
	   "Set a temporary breakpoint.  Args like \"break\" command.\n\
Like \"break\" except the breakpoint is only enabled temporarily,\n\
so it will be disabled when hit.  Equivalent to \"break\" followed\n\
by using \"enable once\" on the breakpoint number.");

  add_prefix_cmd ("enable", class_breakpoint, enable_command,
		  "Enable some breakpoints.\n\
Give breakpoint numbers (separated by spaces) as arguments.\n\
With no subcommand, breakpoints are enabled until you command otherwise.\n\
This is used to cancel the effect of the \"disable\" command.\n\
With a subcommand you can enable temporarily.",
		  &enablelist, "enable ", 1, &cmdlist);

  add_abbrev_prefix_cmd ("breakpoints", class_breakpoint, enable_command,
		  "Enable some breakpoints.\n\
Give breakpoint numbers (separated by spaces) as arguments.\n\
This is used to cancel the effect of the \"disable\" command.\n\
May be abbreviated to simply \"enable\".\n",
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
	   "Disable some breakpoints.\n\
Arguments are breakpoint numbers with spaces in between.\n\
To disable all breakpoints, give no argument.\n\
A disabled breakpoint is not forgotten, but has no effect until reenabled.",
		  &disablelist, "disable ", 1, &cmdlist);
  add_com_alias ("dis", "disable", class_breakpoint, 1);
  add_com_alias ("disa", "disable", class_breakpoint, 1);

  add_cmd ("breakpoints", class_alias, disable_command,
	   "Disable some breakpoints.\n\
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
	    "Status of user-settable breakpoints, or breakpoint number NUMBER.\n\
The \"Type\" column indicates one of:\n\
\tbreakpoint     - normal breakpoint\n\
\twatchpoint     - watchpoint\n\
The \"Disp\" column contains one of \"keep\", \"del\", or \"dis\" to indicate\n\
the disposition of the breakpoint after it gets hit.  \"dis\" means that the\n\
breakpoint will be disabled.  The \"Address\" and \"What\" columns indicate the\n\
address and file/line number respectively.\n\n\
Convenience variable \"$_\" and default examine address for \"x\"\n\
are set to the address of the last breakpoint listed.\n\n\
Convenience variable \"$bpnum\" contains the number of the last\n\
breakpoint set.");

#if MAINTENANCE_CMDS

  add_cmd ("breakpoints", class_maintenance, maintenance_info_breakpoints,
	    "Status of all breakpoints, or breakpoint number NUMBER.\n\
The \"Type\" column indicates one of:\n\
\tbreakpoint     - normal breakpoint\n\
\twatchpoint     - watchpoint\n\
\tlongjmp        - internal breakpoint used to step through longjmp()\n\
\tlongjmp resume - internal breakpoint at the target of longjmp()\n\
\tuntil          - internal breakpoint used by the \"until\" command\n\
\tfinish         - internal breakpoint used by the \"finish\" command\n\
The \"Disp\" column contains one of \"keep\", \"del\", or \"dis\" to indicate\n\
the disposition of the breakpoint after it gets hit.  \"dis\" means that the\n\
breakpoint will be disabled.  The \"Address\" and \"What\" columns indicate the\n\
address and file/line number respectively.\n\n\
Convenience variable \"$_\" and default examine address for \"x\"\n\
are set to the address of the last breakpoint listed.\n\n\
Convenience variable \"$bpnum\" contains the number of the last\n\
breakpoint set.",
	   &maintenanceinfolist);

#endif	/* MAINTENANCE_CMDS */

  add_com ("catch", class_breakpoint, catch_command,
         "Set breakpoints to catch exceptions that are raised.\n\
Argument may be a single exception to catch, multiple exceptions\n\
to catch, or the default exception \"default\".  If no arguments\n\
are given, breakpoints are set at all exception handlers catch clauses\n\
within the current scope.\n\
\n\
A condition specified for the catch applies to all breakpoints set\n\
with this command\n\
\n\
Do \"help breakpoints\" for info on other commands dealing with breakpoints.");

  add_com ("watch", class_breakpoint, watch_command,
	   "Set a watchpoint for an expression.\n\
A watchpoint stops execution of your program whenever the value of\n\
an expression changes.");

  add_info ("watchpoints", breakpoints_info,
	    "Synonym for ``info breakpoints''.");
}

/* OK, when we call objfile_relocate, we need to relocate breakpoints
   too.  breakpoint_re_set is not a good choice--for example, if
   addr_string contains just a line number without a file name the
   breakpoint might get set in a different file.  In general, there is
   no need to go all the way back to the user's string (though this might
   work if some effort were made to canonicalize it), since symtabs and
   everything except addresses are still valid.

   Probably the best way to solve this is to have each breakpoint save
   the objfile and the section number that was used to set it (if set
   by "*addr", probably it is best to use find_pc_line to get a symtab
   and use the objfile and block_line_section for that symtab).  Then
   objfile_relocate can call fixup_breakpoints with the objfile and
   the new_offsets, and it can relocate only the appropriate breakpoints.  */

#ifdef IBM6000_TARGET
/* But for now, just kludge it based on the concept that before an
   objfile is relocated the breakpoint is below 0x10000000, and afterwards
   it is higher, so that way we only relocate each breakpoint once.  */

void
fixup_breakpoints (low, high, delta)
  CORE_ADDR low;
  CORE_ADDR high;
  CORE_ADDR delta;
{
  struct breakpoint *b;

  ALL_BREAKPOINTS (b)
    {
     if (b->address >= low && b->address <= high)
       b->address += delta;
    }
}
#endif
