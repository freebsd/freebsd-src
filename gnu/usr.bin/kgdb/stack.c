/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)stack.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/* Print and select stack frames for GDB, the GNU debugger.
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

/* modified by rjc Thu Nov  1 16:46:57 1990, fixed return_command so that
   it can return values, it still has problems when running on pmax,
   cannot write register 65 */

#include <stdio.h>

#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "frame.h"
#include "value.h"


/* Thie "selected" stack frame is used by default for local and arg access.
   May be zero, for no selected frame.  */

FRAME selected_frame;

/* Level of the selected frame:
   0 for innermost, 1 for its caller, ...
   or -1 for frame specified by address with no defined level.  */

int selected_frame_level;

/* Nonzero means print the full filename and linenumber
   when a frame is printed, and do so in a format programs can parse.  */

int frame_file_full_name = 0;

static void select_calling_frame ();

void print_frame_info ();

/* Print a stack frame briefly.  FRAME should be the frame id
   and LEVEL should be its level in the stack (or -1 for level not defined).
   This prints the level, the function executing, the arguments,
   and the file name and line number.
   If the pc is not at the beginning of the source line,
   the actual pc is printed at the beginning.

   If SOURCE is 1, print the source line as well.
   If SOURCE is -1, print ONLY the source line.  */

static void
print_stack_frame (frame, level, source)
     FRAME frame;
     int level;
     int source;
{
  struct frame_info *fi;

  fi = get_frame_info (frame);

  print_frame_info (fi, level, source, 1);
}

/* Flag which will indicate when the frame has been changed
   by and "up" or "down" command.  */
static int frame_changed;

void
print_frame_info (fi, level, source, args)
     struct frame_info *fi;
     register int level;
     int source;
     int args;
{
  struct symtab_and_line sal;
  struct symbol *func;
  register char *funname = 0;
  int numargs;
  struct partial_symtab *pst;

  /* Don't give very much information if we haven't readin the
     symbol table yet.  */
  pst = find_pc_psymtab (fi->pc);
  if (pst && !pst->readin)
    {
      /* Abbreviated information.  */
      char *fname;

      if (!find_pc_partial_function (fi->pc, &fname, 0))
	fname = "??";
	
      printf_filtered ("#%-2d ", level);
      printf_filtered ("0x%x in ", fi->pc);

      fputs_demangled(fname, stdout, -1);
      fputs_filtered(" (...)\n", stdout);
      
      return;
    }

  sal = find_pc_line (fi->pc, fi->next_frame);
  func = find_pc_function (fi->pc);
  if (func)
    {
      /* In certain pathological cases, the symtabs give the wrong
	 function (when we are in the first function in a file which
	 is compiled without debugging symbols, the previous function
	 is compiled with debugging symbols, and the "foo.o" symbol
	 that is supposed to tell us where the file with debugging symbols
	 ends has been truncated by ar because it is longer than 15
	 characters).

	 So look in the misc_function_vector as well, and if it comes
	 up with a larger address for the function use that instead.
	 I don't think this can ever cause any problems;
	 there shouldn't be any
	 misc_function_vector symbols in the middle of a function.  */
      int misc_index = find_pc_misc_function (fi->pc);
      if (misc_index >= 0
	  && (misc_function_vector[misc_index].address
	      > BLOCK_START (SYMBOL_BLOCK_VALUE (func))))
	{
	  /* In this case we have no way of knowing the source file
	     and line number, so don't print them.  */
	  sal.symtab = 0;
	  /* We also don't know anything about the function besides
	     its address and name.  */
	  func = 0;
	  funname = misc_function_vector[misc_index].name;
	}
      else
	funname = SYMBOL_NAME (func);
    }
  else
    {
      register int misc_index = find_pc_misc_function (fi->pc);
      if (misc_index >= 0)
	funname = misc_function_vector[misc_index].name;
    }

  if (frame_changed || source >= 0 || !sal.symtab)
    {
      if (level >= 0)
	printf_filtered ("#%-2d ", level);
      else if (frame_changed)
	printf ("#%-2d ", 0);
      if (fi->pc != sal.pc || !sal.symtab)
	printf_filtered ("0x%x in ", fi->pc);
      fputs_demangled(funname ? funname : "??", stdout, -1);
      printf_filtered(" (");
      if (args)
        {
	  if (func)
	    numargs = -1;
	  else
	    FRAME_NUM_ARGS (numargs, fi);

	  print_frame_args (func, fi, numargs, stdout);
	}
      printf_filtered (")");
      if (sal.symtab)
	printf_filtered (" (%s line %d)", sal.symtab->filename, sal.line);
      printf_filtered ("\n");
    }

  if ((frame_changed || source != 0) && sal.symtab)
    {
      int done = 0;
      int mid_statement = source < 0 && fi->pc != sal.pc;
      if (frame_file_full_name)
	done = identify_source_line (sal.symtab, sal.line, mid_statement);
      if (!done)
	{
	  if (mid_statement)
	    printf_filtered ("0x%x\t", fi->pc);
	  print_source_lines (sal.symtab, sal.line, sal.line + 1, 1);
	}
      current_source_line = max (sal.line - 5, 1);
    }
  frame_changed = 0;
  if (source != 0)
    set_default_breakpoint (1, fi->pc, sal.symtab, sal.line);

  fflush (stdout);
}

/* Call here to print info on selected frame, after a trap.  */

void
print_sel_frame (just_source)
     int just_source;
{
  print_stack_frame (selected_frame, -1, just_source ? -1 : 1);
}

/* Print info on the selected frame, including level number
   but not source.  */

void
print_selected_frame ()
{
  print_stack_frame (selected_frame, selected_frame_level, 0);
}

void flush_cached_frames ();

#ifdef FRAME_SPECIFICATION_DYADIC
extern FRAME setup_arbitrary_frame ();
#endif

/*
 * Read a frame specification in whatever the appropriate format is.
 */
static FRAME
parse_frame_specification (frame_exp)
     char *frame_exp;
{
  int numargs = 0;
  int arg1, arg2;
  
  if (frame_exp)
    {
      char *addr_string, *p;
      struct cleanup *tmp_cleanup;
      struct frame_info *fci;

      while (*frame_exp == ' ') frame_exp++;
      for (p = frame_exp; *p && *p != ' '; p++)
	;

      if (*frame_exp)
	{
	  numargs = 1;
	  addr_string = savestring(frame_exp, p - frame_exp);

	  {
	    tmp_cleanup = make_cleanup (free, addr_string);
	    arg1 = parse_and_eval_address (addr_string);
	    do_cleanups (tmp_cleanup);
	  }

	  while (*p == ' ') p++;
	  
	  if (*p)
	    {
	      numargs = 2;
	      arg2 = parse_and_eval_address (p);
	    }
	}
    }

  switch (numargs)
    {
    case 0:
      return selected_frame;
      /* NOTREACHED */
    case 1:
      {
	int level = arg1;
	FRAME fid = find_relative_frame (get_current_frame (), &level);
	FRAME tfid;

	if (level == 0)
	  /* find_relative_frame was successful */
	  return fid;

	/* If (s)he specifies the frame with an address, he deserves what
	   (s)he gets.  Still, give the highest one that matches.  */

	for (fid = get_current_frame ();
	     fid && FRAME_FP (fid) != arg1;
	     fid = get_prev_frame (fid))
	  ;

	if (fid)
	  while ((tfid = get_prev_frame (fid)) &&
		 (FRAME_FP (tfid) == arg1))
	    fid = tfid;
	  
#ifdef FRAME_SPECIFICATION_DYADIC
	if (!fid)
	  error ("Incorrect number of args in frame specification");

	return fid;
#else
	return create_new_frame (arg1, 0);
#endif
      }
      /* NOTREACHED */
    case 2:
      /* Must be addresses */
#ifndef FRAME_SPECIFICATION_DYADIC
      error ("Incorrect number of args in frame specification");
#else
      return setup_arbitrary_frame (arg1, arg2);
#endif
      /* NOTREACHED */
    }
  fatal ("Internal: Error in parsing in parse_frame_specification");
  /* NOTREACHED */
}

/* FRAME_ARGS_ADDRESS_CORRECT is just like FRAME_ARGS_ADDRESS except
   that if it is unsure about the answer, it returns Frame_unknown
   instead of guessing (this happens on the VAX, for example).

   On most machines, we never have to guess about the args address,
   so FRAME_ARGS_ADDRESS{,_CORRECT} are the same.  */
#if !defined (FRAME_ARGS_ADDRESS_CORRECT)
#define FRAME_ARGS_ADDRESS_CORRECT FRAME_ARGS_ADDRESS
#endif

/* Print verbosely the selected frame or the frame at address ADDR.
   This means absolutely all information in the frame is printed.  */

static void
frame_info (addr_exp)
     char *addr_exp;
{
  FRAME frame;
  struct frame_info *fi;
  struct frame_saved_regs fsr;
  struct symtab_and_line sal;
  struct symbol *func;
  FRAME calling_frame;
  int i, count;
  char *funname = 0;

  if (!(have_inferior_p () || have_core_file_p ()))
    error ("No inferior or core file.");

  frame = parse_frame_specification (addr_exp);
  if (!frame)
    error ("Invalid frame specified.");

  fi = get_frame_info (frame);
  get_frame_saved_regs (fi, &fsr);
  sal = find_pc_line (fi->pc, fi->next_frame);
  func = get_frame_function (frame);
  if (func)
    funname = SYMBOL_NAME (func);
  else
    {
      register int misc_index = find_pc_misc_function (fi->pc);
      if (misc_index >= 0)
	funname = misc_function_vector[misc_index].name;
    }
  calling_frame = get_prev_frame (frame);

  if (!addr_exp && selected_frame_level >= 0)
    printf ("Stack level %d, frame at 0x%x:\n pc = 0x%x",
	    selected_frame_level, FRAME_FP(frame), fi->pc);
  else
    printf ("Stack frame at 0x%x:\n pc = 0x%x",
	    FRAME_FP(frame), fi->pc);

  if (funname)
    printf (" in %s", funname);
  if (sal.symtab)
    printf (" (%s line %d)", sal.symtab->filename, sal.line);
  printf ("; saved pc 0x%x\n", FRAME_SAVED_PC (frame));
  if (calling_frame)
    printf (" called by frame at 0x%x", FRAME_FP (calling_frame));
  if (fi->next_frame && calling_frame)
    printf (",");
  if (fi->next_frame)
    printf (" caller of frame at 0x%x", fi->next_frame);
  if (fi->next_frame || calling_frame)
    printf ("\n");

  {
    /* Address of the argument list for this frame, or Frame_unknown.  */
    CORE_ADDR arg_list = FRAME_ARGS_ADDRESS_CORRECT (fi);
    /* Number of args for this frame, or -1 if unknown.  */
    int numargs;

    if (arg_list != Frame_unknown)
      {
	printf (" Arglist at 0x%x,", arg_list);

	FRAME_NUM_ARGS (numargs, fi);
	if (numargs < 0)
	  printf (" args: ");
	else if (numargs == 0)
	  printf (" no args.");
	else if (numargs == 1)
	  printf (" 1 arg: ");
	else
	  printf (" %d args: ", numargs);
	print_frame_args (func, fi, numargs, stdout);
	printf ("\n");
      }
  }
  
  /* The sp is special; what's returned isn't the save address, but
     actually the value of the previous frame's sp.  */
  printf (" Previous frame's sp is 0x%x\n", fsr.regs[SP_REGNUM]);
  count = 0;
  for (i = 0; i < NUM_REGS; i++)
    if (fsr.regs[i] && i != SP_REGNUM)
      {
	if (count % 4 != 0)
	  printf (", ");
	else
	  {
	    if (count == 0)
	      printf (" Saved registers:");
	    printf ("\n  ");
	  }
	printf ("%s at 0x%x", reg_names[i], fsr.regs[i]);
	count++;
      }
  if (count)
    printf ("\n");
}

#if 0
/* Set a limit on the number of frames printed by default in a
   backtrace.  */

static int backtrace_limit;

static void
set_backtrace_limit_command (count_exp, from_tty)
     char *count_exp;
     int from_tty;
{
  int count = parse_and_eval_address (count_exp);

  if (count < 0)
    error ("Negative argument not meaningful as backtrace limit.");

  backtrace_limit = count;
}

static void
backtrace_limit_info (arg, from_tty)
     char *arg;
     int from_tty;
{
  if (arg)
    error ("\"Info backtrace-limit\" takes no arguments.");

  printf ("Backtrace limit: %d.\n", backtrace_limit);
}
#endif

/* Print briefly all stack frames or just the innermost COUNT frames.  */

static void
backtrace_command (count_exp)
     char *count_exp;
{
  struct frame_info *fi;
  register int count;
  register FRAME frame;
  register int i;
  register FRAME trailing;
  register int trailing_level;

  /* The following code must do two things.  First, it must
     set the variable TRAILING to the frame from which we should start
     printing.  Second, it must set the variable count to the number
     of frames which we should print, or -1 if all of them.  */
  trailing = get_current_frame ();
  trailing_level = 0;
  if (count_exp)
    {
      count = parse_and_eval_address (count_exp);
      if (count < 0)
	{
	  FRAME current;

	  count = -count;

	  current = trailing;
	  while (current && count--)
	    current = get_prev_frame (current);
	  
	  /* Will stop when CURRENT reaches the top of the stack.  TRAILING
	     will be COUNT below it.  */
	  while (current)
	    {
	      trailing = get_prev_frame (trailing);
	      current = get_prev_frame (current);
	      trailing_level++;
	    }
	  
	  count = -1;
	}
    }
  else
    count = -1;

  for (i = 0, frame = trailing;
       frame && count--;
       i++, frame = get_prev_frame (frame))
    {
      QUIT;
      fi = get_frame_info (frame);
      print_frame_info (fi, trailing_level + i, 0, 1);
    }

  /* If we've stopped before the end, mention that.  */
  if (frame)
    printf_filtered ("(More stack frames follow...)\n");
}

/* Print the local variables of a block B active in FRAME.
   Return 1 if any variables were printed; 0 otherwise.  */

static int
print_block_frame_locals (b, frame, stream)
     struct block *b;
     register FRAME frame;
     register FILE *stream;
{
  int nsyms;
  register int i;
  register struct symbol *sym;
  register int values_printed = 0;

  nsyms = BLOCK_NSYMS (b);

  for (i = 0; i < nsyms; i++)
    {
      sym = BLOCK_SYM (b, i);
      if (SYMBOL_CLASS (sym) == LOC_LOCAL
	  || SYMBOL_CLASS (sym) == LOC_REGISTER
	  || SYMBOL_CLASS (sym) == LOC_STATIC)
	{
	  values_printed = 1;
	  fputs_filtered (SYMBOL_NAME (sym), stream);
	  fputs_filtered (" = ", stream);
	  print_variable_value (sym, frame, stream);
	  fprintf_filtered (stream, "\n");
	  fflush (stream);
	}
    }
  return values_printed;
}

/* Print on STREAM all the local variables in frame FRAME,
   including all the blocks active in that frame
   at its current pc.

   Returns 1 if the job was done,
   or 0 if nothing was printed because we have no info
   on the function running in FRAME.  */

static int
print_frame_local_vars (frame, stream)
     register FRAME frame;
     register FILE *stream;
{
  register struct block *block = get_frame_block (frame);
  register int values_printed = 0;

  if (block == 0)
    {
      fprintf_filtered (stream, "No symbol table info available.\n");
      fflush (stream);
      return 0;
    }
  
  while (block != 0)
    {
      if (print_block_frame_locals (block, frame, stream))
	values_printed = 1;
      /* After handling the function's top-level block, stop.
	 Don't continue to its superblock, the block of
	 per-file symbols.  */
      if (BLOCK_FUNCTION (block))
	break;
      block = BLOCK_SUPERBLOCK (block);
    }

  if (!values_printed)
    {
      fprintf_filtered (stream, "No locals.\n");
      fflush (stream);
    }
  
  return 1;
}

static void
locals_info ()
{
  if (!have_inferior_p () && !have_core_file_p ())
    error ("No inferior or core file.");

  print_frame_local_vars (selected_frame, stdout);
}

static int
print_frame_arg_vars (frame, stream)
     register FRAME frame;
     register FILE *stream;
{
  struct symbol *func = get_frame_function (frame);
  register struct block *b;
  int nsyms;
  register int i;
  register struct symbol *sym;
  register int values_printed = 0;

  if (func == 0)
    {
      fprintf_filtered (stream, "No symbol table info available.\n");
      fflush (stream);
      return 0;
    }

  b = SYMBOL_BLOCK_VALUE (func);
  nsyms = BLOCK_NSYMS (b);

  for (i = 0; i < nsyms; i++)
    {
      sym = BLOCK_SYM (b, i);
      if (SYMBOL_CLASS (sym) == LOC_ARG
	  || SYMBOL_CLASS (sym) == LOC_REF_ARG
	  || SYMBOL_CLASS (sym) == LOC_REGPARM)
	{
	  values_printed = 1;
	  fputs_filtered (SYMBOL_NAME (sym), stream);
	  fputs_filtered (" = ", stream);
	  print_variable_value (sym, frame, stream);
	  fprintf_filtered (stream, "\n");
	  fflush (stream);
	}
    }

  if (!values_printed)
    {
      fprintf_filtered (stream, "No arguments.\n");
      fflush (stream);
    }

  return 1;
}

static void
args_info ()
{
  if (!have_inferior_p () && !have_core_file_p ())
    error ("No inferior or core file.");
  print_frame_arg_vars (selected_frame, stdout);
}

/* Select frame FRAME, and note that its stack level is LEVEL.
   LEVEL may be -1 if an actual level number is not known.  */

void
select_frame (frame, level)
     FRAME frame;
     int level;
{
  selected_frame = frame;
  selected_frame_level = level;
  /* Ensure that symbols for this frame are readin.  */
  if (frame)
    find_pc_symtab (get_frame_info (frame)->pc);
}

/* Store the selected frame and its level into *FRAMEP and *LEVELP.  */

void
record_selected_frame (frameaddrp, levelp)
     FRAME_ADDR *frameaddrp;
     int *levelp;
{
  *frameaddrp = FRAME_FP (selected_frame);
  *levelp = selected_frame_level;
}

/* Return the symbol-block in which the selected frame is executing.
   Can return zero under various legitimate circumstances.  */

struct block *
get_selected_block ()
{
  if (!have_inferior_p () && !have_core_file_p ())
    return 0;

  if (!selected_frame)
    return get_current_block ();
  return get_frame_block (selected_frame);
}

/* Find a frame a certain number of levels away from FRAME.
   LEVEL_OFFSET_PTR points to an int containing the number of levels.
   Positive means go to earlier frames (up); negative, the reverse.
   The int that contains the number of levels is counted toward
   zero as the frames for those levels are found.
   If the top or bottom frame is reached, that frame is returned,
   but the final value of *LEVEL_OFFSET_PTR is nonzero and indicates
   how much farther the original request asked to go.  */

FRAME
find_relative_frame (frame, level_offset_ptr)
     register FRAME frame;
     register int* level_offset_ptr;
{
  register FRAME prev;
  register FRAME frame1, frame2;

  /* Going up is simple: just do get_prev_frame enough times
     or until initial frame is reached.  */
  while (*level_offset_ptr > 0)
    {
      prev = get_prev_frame (frame);
      if (prev == 0)
	break;
      (*level_offset_ptr)--;
      frame = prev;
    }
  /* Going down could be done by iterating get_frame_info to
     find the next frame, but that would be quadratic
     since get_frame_info must scan all the way from the current frame.
     The following algorithm is linear.  */
  if (*level_offset_ptr < 0)
    {
      /* First put frame1 at innermost frame
	 and frame2 N levels up from there.  */
      frame1 = get_current_frame ();
      frame2 = frame1;
      while (*level_offset_ptr < 0 && frame2 != frame)
	{
	  frame2 = get_prev_frame (frame2);
	  (*level_offset_ptr) ++;
	}
      /* Then slide frame1 and frame2 up in synchrony
	 and when frame2 reaches our starting point
	 frame1 must be N levels down from there.  */
      while (frame2 != frame)
	{
	  frame1 = get_prev_frame (frame1);
	  frame2 = get_prev_frame (frame2);
	}
      return frame1;
    }
  return frame;
}

/* The "frame" command.  With no arg, print selected frame briefly.
   With arg LEVEL_EXP, select the frame at level LEVEL if it is a
   valid level.  Otherwise, treat level_exp as an address expression
   and print it.  See parse_frame_specification for more info on proper
   frame expressions. */

static void
frame_command (level_exp, from_tty)
     char *level_exp;
     int from_tty;
{
  register FRAME frame, frame1;
  unsigned int level = 0;

  if (!have_inferior_p () && ! have_core_file_p ())
    error ("No inferior or core file.");

  frame = parse_frame_specification (level_exp);

  for (frame1 = get_prev_frame (0);
       frame1 && frame1 != frame;
       frame1 = get_prev_frame (frame1))
    level++;

  if (!frame1)
    level = 0;

  frame_changed = level;
  select_frame (frame, level);

  if (!from_tty)
    return;

  print_stack_frame (selected_frame, selected_frame_level, 1);
}

/* Select the frame up one or COUNT stack levels
   from the previously selected frame, and print it briefly.  */

static void
up_command (count_exp)
     char *count_exp;
{
  register FRAME frame;
  int count = 1, count1;
  if (count_exp)
    count = parse_and_eval_address (count_exp);
  count1 = count;
  
  if (!have_inferior_p () && !have_core_file_p ())
    error ("No inferior or core file.");

  frame = find_relative_frame (selected_frame, &count1);
  if (count1 != 0 && count_exp == 0)
    error ("Initial frame selected; you cannot go up.");
  select_frame (frame, selected_frame_level + count - count1);

  print_stack_frame (selected_frame, selected_frame_level, 1);
  frame_changed++;
}

/* Select the frame down one or COUNT stack levels
   from the previously selected frame, and print it briefly.  */

static void
down_command (count_exp)
     char *count_exp;
{
  register FRAME frame;
  int count = -1, count1;
  if (count_exp)
    count = - parse_and_eval_address (count_exp);
  count1 = count;
  
  frame = find_relative_frame (selected_frame, &count1);
  if (count1 != 0 && count_exp == 0)
    error ("Bottom (i.e., innermost) frame selected; you cannot go down.");
  select_frame (frame, selected_frame_level + count - count1);

  print_stack_frame (selected_frame, selected_frame_level, 1);
  frame_changed--;
}

static void
return_command (retval_exp, from_tty)
     char *retval_exp;
     int from_tty;
{
    value return_value;
  struct symbol *thisfun = get_frame_function (selected_frame);
  FRAME_ADDR selected_frame_addr = FRAME_FP (selected_frame);

  /* If interactive, require confirmation.  */

  if (from_tty)
    {
      if (thisfun != 0)
	{
	  if (!query ("Make %s return now? ", SYMBOL_NAME (thisfun)))
	    error ("Not confirmed.");
	}
      else
	if (!query ("Make selected stack frame return now? "))
	  error ("Not confirmed.");
    }

  /* Do the real work.  Pop until the specified frame is current.  We
     use this method because the selected_frame is not valid after
     a POP_FRAME.  Note that this will not work if the selected frame
     shares it's fp with another frame.  */

  while (selected_frame_addr != FRAME_FP (get_current_frame()))
    POP_FRAME;

  /* get the return value while still in this frame */
  if (retval_exp) 
      return_value = parse_and_eval (retval_exp);

  /* Then pop that frame.  */
  POP_FRAME;

  /* Store the return value if there was one */

  if (retval_exp)
    set_return_value (return_value);

  /* If interactive, print the frame that is now current.  */

  if (from_tty)
    frame_command ("0", 1);
}

extern struct cmd_list_element *setlist;

void
_initialize_stack ()
{
#if 0  
  backtrace_limit = 30;
#endif

  add_com ("return", class_stack, return_command,
	   "Make selected stack frame return to its caller.\n\
Control remains in the debugger, but when you continue\n\
execution will resume in the frame above the one now selected.\n\
If an argument is given, it is an expression for the value to return.");

  add_com ("up", class_stack, up_command,
	   "Select and print stack frame that called this one.\n\
An argument says how many frames up to go.");

  add_com ("down", class_stack, down_command,
	   "Select and print stack frame called by this one.\n\
An argument says how many frames down to go.");
  add_com_alias ("do", "down", class_stack, 1);

  add_com ("frame", class_stack, frame_command,
	   "Select and print a stack frame.\n\
With no argument, print the selected stack frame.  (See also \"info frame\").\n\
An argument specifies the frame to select.\n\
It can be a stack frame number or the address of the frame.\n\
With argument, nothing is printed if input is coming from\n\
a command file or a user-defined command.");

  add_com_alias ("f", "frame", class_stack, 1);

  add_com ("backtrace", class_stack, backtrace_command,
	   "Print backtrace of all stack frames, or innermost COUNT frames.\n\
With a negative argument, print outermost -COUNT frames.");
  add_com_alias ("bt", "backtrace", class_stack, 0);
  add_com_alias ("where", "backtrace", class_alias, 0);
  add_info ("stack", backtrace_command,
	    "Backtrace of the stack, or innermost COUNT frames.");
  add_info_alias ("s", "stack", 1);
  add_info ("frame", frame_info,
	    "All about selected stack frame, or frame at ADDR.");
  add_info_alias ("f", "frame", 1);
  add_info ("locals", locals_info,
	    "Local variables of current stack frame.");
  add_info ("args", args_info,
	    "Argument variables of current stack frame.");

#if 0
  add_cmd ("backtrace-limit", class_stack, set_backtrace_limit_command, 
	   "Specify maximum number of frames for \"backtrace\" to print by default.",
	   &setlist);
  add_info ("backtrace-limit", backtrace_limit_info,
	    "The maximum number of frames for \"backtrace\" to print by default.");
#endif
}

