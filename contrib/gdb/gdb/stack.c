/* Print and select stack frames for GDB, the GNU debugger.
   Copyright 1986, 87, 89, 91, 92, 93, 94, 95, 96, 98, 1999
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

#include <ctype.h>
#include "defs.h"
#include "gdb_string.h"
#include "value.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "language.h"
#include "frame.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "target.h"
#include "breakpoint.h"
#include "demangle.h"
#include "inferior.h"
#include "annotate.h"
#include "symfile.h"
#include "objfiles.h"

/* Prototypes for exported functions. */

void args_info PARAMS ((char *, int));

void locals_info PARAMS ((char *, int));

void (*selected_frame_level_changed_hook) PARAMS ((int));

void _initialize_stack PARAMS ((void));

/* Prototypes for local functions. */

static void return_command PARAMS ((char *, int));

static void down_command PARAMS ((char *, int));

static void down_silently_base PARAMS ((char *));

static void down_silently_command PARAMS ((char *, int));

static void up_command PARAMS ((char *, int));

static void up_silently_base PARAMS ((char *));

static void up_silently_command PARAMS ((char *, int));

void frame_command PARAMS ((char *, int));

static void select_frame_command PARAMS ((char *, int));

static void print_frame_arg_vars PARAMS ((struct frame_info *, GDB_FILE *));

static void catch_info PARAMS ((char *, int));

static void args_plus_locals_info PARAMS ((char *, int));

static void print_frame_label_vars PARAMS ((struct frame_info *,
					    int,
					    GDB_FILE *));

static void print_frame_local_vars PARAMS ((struct frame_info *,
					    int,
					    GDB_FILE *));

static int print_block_frame_labels PARAMS ((struct block *, int *,
					     GDB_FILE *));

static int print_block_frame_locals PARAMS ((struct block *,
					     struct frame_info *,
					     int,
					     GDB_FILE *));

static void print_frame_info_base PARAMS ((struct frame_info *, int, int, int));
  
static void print_stack_frame_base PARAMS ((struct frame_info *, int, int));

static void backtrace_command PARAMS ((char *, int));

struct frame_info *parse_frame_specification PARAMS ((char *));

static void frame_info PARAMS ((char *, int));

extern int addressprint;	/* Print addresses, or stay symbolic only? */
extern int info_verbose;	/* Verbosity of symbol reading msgs */
extern int lines_to_list;	/* # of lines "list" command shows by default */

/* The "selected" stack frame is used by default for local and arg access.
   May be zero, for no selected frame.  */

struct frame_info *selected_frame;

/* Level of the selected frame:
   0 for innermost, 1 for its caller, ...
   or -1 for frame specified by address with no defined level.  */

int selected_frame_level;

/* Zero means do things normally; we are interacting directly with the
   user.  One means print the full filename and linenumber when a
   frame is printed, and do so in a format emacs18/emacs19.22 can
   parse.  Two means print similar annotations, but in many more
   cases and in a slightly different syntax.  */

int annotation_level = 0;


struct print_stack_frame_args {
  struct frame_info *fi;
  int level;
  int source;
  int args;
};

static int print_stack_frame_base_stub PARAMS ((char *));

/* Show and print the frame arguments.
   Pass the args the way catch_errors wants them.  */
static int
show_and_print_stack_frame_stub (args)
     char *args;
{
  struct print_stack_frame_args *p = (struct print_stack_frame_args *)args;

  /* Reversed order of these so tuiDo() doesn't occur
   * in the middle of "Breakpoint 1 ... [location]" printing = RT
   */
  if (tui_version)
    print_frame_info_base (p->fi, p->level, p->source, p->args);
  print_frame_info (p->fi, p->level, p->source, p->args);

  return 0;
}

/* Show or print the frame arguments.
   Pass the args the way catch_errors wants them.  */
static int
print_stack_frame_stub (args)
     char *args;
{
  struct print_stack_frame_args *p = (struct print_stack_frame_args *)args;

  if (tui_version)
    print_frame_info (p->fi, p->level, p->source, p->args);
  else
    print_frame_info_base (p->fi, p->level, p->source, p->args);
  return 0;
}

/* Print a stack frame briefly.  FRAME_INFI should be the frame info
   and LEVEL should be its level in the stack (or -1 for level not
   defined). */

/* Pass the args the way catch_errors wants them.  */
static int
print_stack_frame_base_stub (args)
     char *args;
{
  struct print_stack_frame_args *p = (struct print_stack_frame_args *)args;

  print_frame_info_base (p->fi, p->level, p->source, p->args);
  return 0;
}

/* print the frame arguments to the terminal.  
   Pass the args the way catch_errors wants them.  */
static int
print_only_stack_frame_stub (args)
     char *args;
{
  struct print_stack_frame_args *p = (struct print_stack_frame_args *)args;

  print_frame_info_base (p->fi, p->level, p->source, p->args);
  return 0;
}

/* Print a stack frame briefly.  FRAME_INFI should be the frame info
   and LEVEL should be its level in the stack (or -1 for level not defined).
   This prints the level, the function executing, the arguments,
   and the file name and line number.
   If the pc is not at the beginning of the source line,
   the actual pc is printed at the beginning.

   If SOURCE is 1, print the source line as well.
   If SOURCE is -1, print ONLY the source line.  */

static void
print_stack_frame_base (fi, level, source)
     struct frame_info *fi;
     int level;
     int source;
{
  struct print_stack_frame_args args;

  args.fi = fi;
  args.level = level;
  args.source = source;
  args.args = 1;

  catch_errors (print_stack_frame_stub, (char *)&args, "", RETURN_MASK_ALL);
}

/* Show and print a stack frame briefly.  FRAME_INFI should be the frame info
   and LEVEL should be its level in the stack (or -1 for level not defined).
   This prints the level, the function executing, the arguments,
   and the file name and line number.
   If the pc is not at the beginning of the source line,
   the actual pc is printed at the beginning.

   If SOURCE is 1, print the source line as well.
   If SOURCE is -1, print ONLY the source line.  */

void
show_and_print_stack_frame (fi, level, source)
     struct frame_info *fi;
     int level;
     int source;
{
  struct print_stack_frame_args args;

  args.fi = fi;
  args.level = level;
  args.source = source;
  args.args = 1;

  catch_errors (show_and_print_stack_frame_stub, (char *)&args, "", RETURN_MASK_ALL);
}


/* Show or print a stack frame briefly.  FRAME_INFI should be the frame info
   and LEVEL should be its level in the stack (or -1 for level not defined).
   This prints the level, the function executing, the arguments,
   and the file name and line number.
   If the pc is not at the beginning of the source line,
   the actual pc is printed at the beginning.

   If SOURCE is 1, print the source line as well.
   If SOURCE is -1, print ONLY the source line.  */

void
print_stack_frame (fi, level, source)
     struct frame_info *fi;
     int level;
     int source;
{
  struct print_stack_frame_args args;

  args.fi = fi;
  args.level = level;
  args.source = source;
  args.args = 1;

  catch_errors (print_stack_frame_stub, (char *)&args, "", RETURN_MASK_ALL);
}

/* Print a stack frame briefly.  FRAME_INFI should be the frame info
   and LEVEL should be its level in the stack (or -1 for level not defined).
   This prints the level, the function executing, the arguments,
   and the file name and line number.
   If the pc is not at the beginning of the source line,
   the actual pc is printed at the beginning.

   If SOURCE is 1, print the source line as well.
   If SOURCE is -1, print ONLY the source line.  */

void
print_only_stack_frame (fi, level, source)
     struct frame_info *fi;
     int level;
     int source;
{
  struct print_stack_frame_args args;

  args.fi = fi;
  args.level = level;
  args.source = source;
  args.args = 1;

  catch_errors (print_only_stack_frame_stub, 
                (char *)&args, "", RETURN_MASK_ALL);
}

struct print_args_args {
  struct symbol *func;
  struct frame_info *fi;
};

static int print_args_stub PARAMS ((PTR));

/* Pass the args the way catch_errors wants them.  */

static int
print_args_stub (args)
     PTR args;
{
  int numargs;
  struct print_args_args *p = (struct print_args_args *)args;

  FRAME_NUM_ARGS (numargs, (p->fi));
  print_frame_args (p->func, p->fi, numargs, gdb_stdout);
  return 0;
}

/* Print information about a frame for frame "fi" at level "level".
 * Used in "where" output, also used to emit breakpoint or step messages.
 * LEVEL is the level of the frame, or -1 if it is the innermost frame
 * but we don't want to print the level.
 * The meaning of the SOURCE argument is:
 * -1: Print only source line
 *  0: Print only location
 *  1: Print location and source line
 */

static void
print_frame_info_base (fi, level, source, args)
     struct frame_info *fi;
     int level;
     int source;
     int args;
{
  struct symtab_and_line sal;
  struct symbol *func;
  register char *funname = 0;
  enum language funlang = language_unknown;

#if 0
  char buf[MAX_REGISTER_RAW_SIZE];
  CORE_ADDR sp;

  /* On the 68k, this spends too much time in m68k_find_saved_regs.  */

  /* Get the value of SP_REGNUM relative to the frame.  */
  get_saved_register (buf, (int *)NULL, (CORE_ADDR *)NULL,
		      FRAME_INFO_ID (fi), SP_REGNUM, (enum lval_type *)NULL);
  sp = extract_address (buf, REGISTER_RAW_SIZE (SP_REGNUM));

  /* This is not a perfect test, because if a function alloca's some
     memory, puts some code there, and then jumps into it, then the test
     will succeed even though there is no call dummy.  Probably best is
     to check for a bp_call_dummy breakpoint.  */
  if (PC_IN_CALL_DUMMY (fi->pc, sp, fi->frame))
#else
  if (frame_in_dummy (fi))
#endif
    {
      annotate_frame_begin (level == -1 ? 0 : level, fi->pc);

      /* Do this regardless of SOURCE because we don't have any source
	 to list for this frame.  */
      if (level >= 0)
	printf_filtered ("#%-2d ", level);
      annotate_function_call ();
      printf_filtered ("<function called from gdb>\n");
      annotate_frame_end ();
      return;
    }
  if (fi->signal_handler_caller)
    {
      annotate_frame_begin (level == -1 ? 0 : level, fi->pc);

      /* Do this regardless of SOURCE because we don't have any source
	 to list for this frame.  */
      if (level >= 0)
	printf_filtered ("#%-2d ", level);
      annotate_signal_handler_caller ();
      printf_filtered ("<signal handler called>\n");
      annotate_frame_end ();
      return;
    }

  /* If fi is not the innermost frame, that normally means that fi->pc
     points to *after* the call instruction, and we want to get the line
     containing the call, never the next line.  But if the next frame is
     a signal_handler_caller or a dummy frame, then the next frame was
     not entered as the result of a call, and we want to get the line
     containing fi->pc.  */
  sal =
    find_pc_line (fi->pc,
		  fi->next != NULL
		  && !fi->next->signal_handler_caller
		  && !frame_in_dummy (fi->next));

  func = find_pc_function (fi->pc);
  if (func)
    {
      /* In certain pathological cases, the symtabs give the wrong
	 function (when we are in the first function in a file which
	 is compiled without debugging symbols, the previous function
	 is compiled with debugging symbols, and the "foo.o" symbol
	 that is supposed to tell us where the file with debugging symbols
	 ends has been truncated by ar because it is longer than 15
	 characters).  This also occurs if the user uses asm() to create
	 a function but not stabs for it (in a file compiled -g).

	 So look in the minimal symbol tables as well, and if it comes
	 up with a larger address for the function use that instead.
	 I don't think this can ever cause any problems; there shouldn't
	 be any minimal symbols in the middle of a function; if this is
	 ever changed many parts of GDB will need to be changed (and we'll
	 create a find_pc_minimal_function or some such).  */

      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (fi->pc);
      if (msymbol != NULL
	  && (SYMBOL_VALUE_ADDRESS (msymbol) 
	      > BLOCK_START (SYMBOL_BLOCK_VALUE (func))))
	{
#if 0
	  /* There is no particular reason to think the line number
	     information is wrong.  Someone might have just put in
	     a label with asm() but left the line numbers alone.  */
	  /* In this case we have no way of knowing the source file
	     and line number, so don't print them.  */
	  sal.symtab = 0;
#endif
	  /* We also don't know anything about the function besides
	     its address and name.  */
	  func = 0;
	  funname = SYMBOL_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
      else
	{
          /* I'd like to use SYMBOL_SOURCE_NAME() here, to display
           * the demangled name that we already have stored in
           * the symbol table, but we stored a version with
           * DMGL_PARAMS turned on, and here we don't want
           * to display parameters. So call the demangler again,
           * with DMGL_ANSI only. RT
           * (Yes, I know that printf_symbol_filtered() will
           * again try to demangle the name on the fly, but
           * the issue is that if cplus_demangle() fails here,
           * it'll fail there too. So we want to catch the failure
           * ("demangled==NULL" case below) here, while we still
           * have our hands on the function symbol.)
           */
          char * demangled;
	  funname = SYMBOL_NAME (func);
	  funlang = SYMBOL_LANGUAGE (func);
          if (funlang == language_cplus) {
            demangled = cplus_demangle (funname, DMGL_ANSI);
            if (demangled == NULL)
              /* If the demangler fails, try the demangled name
               * from the symbol table. This'll have parameters,
               * but that's preferable to diplaying a mangled name.
               */
	      funname = SYMBOL_SOURCE_NAME (func);
          }
	}
    }
  else
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (fi->pc);
      if (msymbol != NULL)
	{
	  funname = SYMBOL_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
    }

  if (source >= 0 || !sal.symtab)
    {
      annotate_frame_begin (level == -1 ? 0 : level, fi->pc);

      if (level >= 0)
	printf_filtered ("#%-2d ", level);
      if (addressprint)
	if (fi->pc != sal.pc || !sal.symtab)
	  {
	    annotate_frame_address ();
	    print_address_numeric (fi->pc, 1, gdb_stdout);
	    annotate_frame_address_end ();
	    printf_filtered (" in ");
	  }
      annotate_frame_function_name ();
      fprintf_symbol_filtered (gdb_stdout, funname ? funname : "??", funlang,
			       DMGL_ANSI);
      wrap_here ("   ");
      annotate_frame_args ();
      fputs_filtered (" (", gdb_stdout);
      if (args)
	{
	  struct print_args_args args;
	  args.fi = fi;
	  args.func = func;
	  catch_errors (print_args_stub, &args, "", RETURN_MASK_ALL);
	  QUIT;
	}
      printf_filtered (")");
      if (sal.symtab && sal.symtab->filename)
	{
	  annotate_frame_source_begin ();
          wrap_here ("   ");
	  printf_filtered (" at ");
	  annotate_frame_source_file ();
	  printf_filtered ("%s", sal.symtab->filename);
	  annotate_frame_source_file_end ();
	  printf_filtered (":");
	  annotate_frame_source_line ();
	  printf_filtered ("%d", sal.line);
	  annotate_frame_source_end ();
	}

#ifdef PC_LOAD_SEGMENT
     /* If we couldn't print out function name but if can figure out what
        load segment this pc value is from, at least print out some info
	about its load segment. */
      if (!funname)
	{
	  annotate_frame_where ();
	  wrap_here ("  ");
	  printf_filtered (" from %s", PC_LOAD_SEGMENT (fi->pc));
	}
#endif
#ifdef PC_SOLIB
      if (!funname || (!sal.symtab || !sal.symtab->filename))
	{
	  char *lib = PC_SOLIB (fi->pc);
	  if (lib)
	    {
	      annotate_frame_where ();
	      wrap_here ("  ");
	      printf_filtered (" from %s", lib);
	    }
	}
#endif
      printf_filtered ("\n");
    }

  if ((source != 0) && sal.symtab)
    {
      int done = 0;
      int mid_statement = source < 0 && fi->pc != sal.pc;
      if (annotation_level)
	done = identify_source_line (sal.symtab, sal.line, mid_statement,
				     fi->pc);
      if (!done)
	{
	  if (addressprint && mid_statement && !tui_version)
	    {
	      print_address_numeric (fi->pc, 1, gdb_stdout);
	      printf_filtered ("\t");
	    }
	  if (print_frame_info_listing_hook)
	    print_frame_info_listing_hook (sal.symtab, sal.line, sal.line + 1, 0);
	  else if (!tui_version)
	    print_source_lines (sal.symtab, sal.line, sal.line + 1, 0);
	}
      current_source_line = max (sal.line - lines_to_list/2, 1);
    }
  if (source != 0)
    set_default_breakpoint (1, fi->pc, sal.symtab, sal.line);

  annotate_frame_end ();

  gdb_flush (gdb_stdout);
}


void
stack_publish_stopped_with_no_frame()
{
  TUIDO(((TuiOpaqueFuncPtr)tuiUpdateOnEnd));

  return;
}

/* Show or print the frame info.  If this is the tui, it will be shown in 
   the source display */
void
print_frame_info(fi, level, source, args)
     struct frame_info *fi;
     register int level;
     int source;
     int args;
{
  if (!tui_version)
    print_frame_info_base(fi, level, source, args);
  else
  {
    if (fi && (frame_in_dummy(fi) || fi->signal_handler_caller))
      print_frame_info_base(fi, level, source, args);
    else
      {
	TUIDO(((TuiOpaqueFuncPtr)tui_vShowFrameInfo, fi));
      }
  }
}

/* Show the frame info.  If this is the tui, it will be shown in 
   the source display otherwise, nothing is done */
void
show_stack_frame(fi)
     struct frame_info *fi;
{
  TUIDO(((TuiOpaqueFuncPtr)tui_vShowFrameInfo, fi));
}


/* Read a frame specification in whatever the appropriate format is.
   Call error() if the specification is in any way invalid (i.e.
   this function never returns NULL).  */

struct frame_info *
parse_frame_specification (frame_exp)
     char *frame_exp;
{
  int numargs = 0;
#define	MAXARGS	4
  CORE_ADDR args[MAXARGS];
  
  if (frame_exp)
    {
      char *addr_string, *p;
      struct cleanup *tmp_cleanup;

      while (*frame_exp == ' ') frame_exp++;

      while (*frame_exp)
	{
	  if (numargs > MAXARGS)
	    error ("Too many args in frame specification");
	  /* Parse an argument.  */
          for (p = frame_exp; *p && *p != ' '; p++)
	    ;
	  addr_string = savestring(frame_exp, p - frame_exp);

	  {
	    tmp_cleanup = make_cleanup (free, addr_string);
	    args[numargs++] = parse_and_eval_address (addr_string);
	    do_cleanups (tmp_cleanup);
	  }

	  /* Skip spaces, move to possible next arg.  */
	  while (*p == ' ') p++;
	  frame_exp = p;
	}
    }

  switch (numargs)
    {
    case 0:
      if (selected_frame == NULL)
	error ("No selected frame.");
      return selected_frame;
      /* NOTREACHED */
    case 1:
      {
	int level = args[0];
	struct frame_info *fid =
	  find_relative_frame (get_current_frame (), &level);
	struct frame_info *tfid;

	if (level == 0)
	  /* find_relative_frame was successful */
	  return fid;

	/* If SETUP_ARBITRARY_FRAME is defined, then frame specifications
	   take at least 2 addresses.  It is important to detect this case
	   here so that "frame 100" does not give a confusing error message
	   like "frame specification requires two addresses".  This of course
	   does not solve the "frame 100" problem for machines on which
	   a frame specification can be made with one address.  To solve
	   that, we need a new syntax for a specifying a frame by address.
	   I think the cleanest syntax is $frame(0x45) ($frame(0x23,0x45) for
	   two args, etc.), but people might think that is too much typing,
	   so I guess *0x23,0x45 would be a possible alternative (commas
	   really should be used instead of spaces to delimit; using spaces
	   normally works in an expression).  */
#ifdef SETUP_ARBITRARY_FRAME
	error ("No frame %d", args[0]);
#endif

	/* If (s)he specifies the frame with an address, he deserves what
	   (s)he gets.  Still, give the highest one that matches.  */

	for (fid = get_current_frame ();
	     fid && fid->frame != args[0];
	     fid = get_prev_frame (fid))
	  ;

	if (fid)
	  while ((tfid = get_prev_frame (fid)) &&
		 (tfid->frame == args[0]))
	    fid = tfid;
	  
	/* We couldn't identify the frame as an existing frame, but
	   perhaps we can create one with a single argument.  */
      }

     default:
#ifdef SETUP_ARBITRARY_FRAME
      return SETUP_ARBITRARY_FRAME (numargs, args);
#else
      /* Usual case.  Do it here rather than have everyone supply
	 a SETUP_ARBITRARY_FRAME that does this.  */
      if (numargs == 1)
	return create_new_frame (args[0], 0);
      error ("Too many args in frame specification");
#endif
      /* NOTREACHED */
    }
  /* NOTREACHED */
}

/* FRAME_ARGS_ADDRESS_CORRECT is just like FRAME_ARGS_ADDRESS except
   that if it is unsure about the answer, it returns 0
   instead of guessing (this happens on the VAX and i960, for example).

   On most machines, we never have to guess about the args address,
   so FRAME_ARGS_ADDRESS{,_CORRECT} are the same.  */
#if !defined (FRAME_ARGS_ADDRESS_CORRECT)
#define FRAME_ARGS_ADDRESS_CORRECT FRAME_ARGS_ADDRESS
#endif

/* Print verbosely the selected frame or the frame at address ADDR.
   This means absolutely all information in the frame is printed.  */

static void
frame_info (addr_exp, from_tty)
     char *addr_exp;
     int from_tty;
{
  struct frame_info *fi;
  struct symtab_and_line sal;
  struct symbol *func;
  struct symtab *s;
  struct frame_info *calling_frame_info;
  int i, count, numregs;
  char *funname = 0;
  enum language funlang = language_unknown;

  if (!target_has_stack)
    error ("No stack.");

  fi = parse_frame_specification (addr_exp);
  if (fi == NULL)
    error ("Invalid frame specified.");

  sal = find_pc_line (fi->pc,
		      fi->next != NULL
		      && !fi->next->signal_handler_caller
		      && !frame_in_dummy (fi->next));
  func = get_frame_function (fi);
  s = find_pc_symtab(fi->pc);
  if (func)
    {
      /* I'd like to use SYMBOL_SOURCE_NAME() here, to display
       * the demangled name that we already have stored in
       * the symbol table, but we stored a version with
       * DMGL_PARAMS turned on, and here we don't want
       * to display parameters. So call the demangler again,
       * with DMGL_ANSI only. RT
       * (Yes, I know that printf_symbol_filtered() will
       * again try to demangle the name on the fly, but
       * the issue is that if cplus_demangle() fails here,
       * it'll fail there too. So we want to catch the failure
       * ("demangled==NULL" case below) here, while we still
       * have our hands on the function symbol.)
       */
       char * demangled;
       funname = SYMBOL_NAME (func);
       funlang = SYMBOL_LANGUAGE (func);
       if (funlang == language_cplus)
	 {
	   demangled = cplus_demangle (funname, DMGL_ANSI);
	   /* If the demangler fails, try the demangled name
	    * from the symbol table. This'll have parameters,
	    * but that's preferable to diplaying a mangled name.
	    */
	   if (demangled == NULL)
	     funname = SYMBOL_SOURCE_NAME (func);
	 }
    }
  else
    {
      register struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (fi->pc);
      if (msymbol != NULL)
	{
	  funname = SYMBOL_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
    }
  calling_frame_info = get_prev_frame (fi);

  if (!addr_exp && selected_frame_level >= 0)
    {
      printf_filtered ("Stack level %d, frame at ", selected_frame_level);
      print_address_numeric (fi->frame, 1, gdb_stdout);
      printf_filtered (":\n");
    }
  else
    {
      printf_filtered ("Stack frame at ");
      print_address_numeric (fi->frame, 1, gdb_stdout);
      printf_filtered (":\n");
    }
  printf_filtered (" %s = ", REGISTER_NAME (PC_REGNUM));
  print_address_numeric (fi->pc, 1, gdb_stdout);

  wrap_here ("   ");
  if (funname)
    {
      printf_filtered (" in ");
      fprintf_symbol_filtered (gdb_stdout, funname, funlang,
			       DMGL_ANSI | DMGL_PARAMS);
    }
  wrap_here ("   ");
  if (sal.symtab)
    printf_filtered (" (%s:%d)", sal.symtab->filename, sal.line);
  puts_filtered ("; ");
  wrap_here ("    ");
  printf_filtered ("saved %s ", REGISTER_NAME (PC_REGNUM));
  print_address_numeric (FRAME_SAVED_PC (fi), 1, gdb_stdout);
  printf_filtered ("\n");

  {
    int frameless = 0;
#ifdef FRAMELESS_FUNCTION_INVOCATION
    FRAMELESS_FUNCTION_INVOCATION (fi, frameless);
#endif
    if (frameless)
      printf_filtered (" (FRAMELESS),");
  }

  if (calling_frame_info)
    {
      printf_filtered (" called by frame at ");
      print_address_numeric (calling_frame_info->frame, 1, gdb_stdout);
    }
  if (fi->next && calling_frame_info)
    puts_filtered (",");
  wrap_here ("   ");
  if (fi->next)
    {
      printf_filtered (" caller of frame at ");
      print_address_numeric (fi->next->frame, 1, gdb_stdout);
    }
  if (fi->next || calling_frame_info)
    puts_filtered ("\n");
  if (s)
    printf_filtered (" source language %s.\n", language_str (s->language));

#ifdef PRINT_EXTRA_FRAME_INFO
  PRINT_EXTRA_FRAME_INFO (fi);
#endif

  {
    /* Address of the argument list for this frame, or 0.  */
    CORE_ADDR arg_list = FRAME_ARGS_ADDRESS_CORRECT (fi);
    /* Number of args for this frame, or -1 if unknown.  */
    int numargs;

    if (arg_list == 0)
      printf_filtered (" Arglist at unknown address.\n");
    else
      {
	printf_filtered (" Arglist at ");
	print_address_numeric (arg_list, 1, gdb_stdout);
	printf_filtered (",");

	FRAME_NUM_ARGS (numargs, fi);
	if (numargs < 0)
	  puts_filtered (" args: ");
	else if (numargs == 0)
	  puts_filtered (" no args.");
	else if (numargs == 1)
	  puts_filtered (" 1 arg: ");
	else
	  printf_filtered (" %d args: ", numargs);
	print_frame_args (func, fi, numargs, gdb_stdout);
	puts_filtered ("\n");
      }
  }
  {
    /* Address of the local variables for this frame, or 0.  */
    CORE_ADDR arg_list = FRAME_LOCALS_ADDRESS (fi);

    if (arg_list == 0)
      printf_filtered (" Locals at unknown address,");
    else
      {
	printf_filtered (" Locals at ");
	print_address_numeric (arg_list, 1, gdb_stdout);
	printf_filtered (",");
      }
  }

  FRAME_INIT_SAVED_REGS (fi);
  if (fi->saved_regs != NULL)
    {
      /* The sp is special; what's returned isn't the save address, but
	 actually the value of the previous frame's sp.  */
      printf_filtered (" Previous frame's sp is ");
      print_address_numeric (fi->saved_regs[SP_REGNUM], 1, gdb_stdout);
      printf_filtered ("\n");
      count = 0;
      numregs = ARCH_NUM_REGS;
      for (i = 0; i < numregs; i++)
	if (fi->saved_regs[i] && i != SP_REGNUM)
	  {
	    if (count == 0)
	      puts_filtered (" Saved registers:\n ");
	    else
	      puts_filtered (",");
	    wrap_here (" ");
	    printf_filtered (" %s at ", REGISTER_NAME (i));
	    print_address_numeric (fi->saved_regs[i], 1, gdb_stdout);
	    count++;
	  }
      if (count)
	puts_filtered ("\n");
    }
  else
    {
      /* We could get some information about saved registers by
	 calling get_saved_register on each register.  Which info goes
	 with which frame is necessarily lost, however, and I suspect
	 that the users don't care whether they get the info.  */
      puts_filtered ("\n");
    }
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

  printf_unfiltered ("Backtrace limit: %d.\n", backtrace_limit);
}
#endif

/* Print briefly all stack frames or just the innermost COUNT frames.  */

static void
backtrace_command_1 (count_exp, show_locals, from_tty)
     char *count_exp;
     int show_locals;
     int from_tty;
{
  struct frame_info *fi;
  register int count;
  register int i;
  register struct frame_info *trailing;
  register int trailing_level;

  if (!target_has_stack)
    error ("No stack.");

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
	  struct frame_info *current;

	  count = -count;

	  current = trailing;
	  while (current && count--)
	    {
	      QUIT;
	      current = get_prev_frame (current);
	    }
	  
	  /* Will stop when CURRENT reaches the top of the stack.  TRAILING
	     will be COUNT below it.  */
	  while (current)
	    {
	      QUIT;
	      trailing = get_prev_frame (trailing);
	      current = get_prev_frame (current);
	      trailing_level++;
	    }
	  
	  count = -1;
	}
    }
  else
    count = -1;

  if (info_verbose)
    {
      struct partial_symtab *ps;
      
      /* Read in symbols for all of the frames.  Need to do this in
	 a separate pass so that "Reading in symbols for xxx" messages
	 don't screw up the appearance of the backtrace.  Also
	 if people have strong opinions against reading symbols for
	 backtrace this may have to be an option.  */
      i = count;
      for (fi = trailing;
	   fi != NULL && i--;
	   fi = get_prev_frame (fi))
	{
	  QUIT;
	  ps = find_pc_psymtab (fi->pc);
	  if (ps)
	    PSYMTAB_TO_SYMTAB (ps);	/* Force syms to come in */
	}
    }

  for (i = 0, fi = trailing;
       fi && count--;
       i++, fi = get_prev_frame (fi))
    {
      QUIT;

      /* Don't use print_stack_frame; if an error() occurs it probably
	 means further attempts to backtrace would fail (on the other
	 hand, perhaps the code does or could be fixed to make sure
	 the frame->prev field gets set to NULL in that case).  */
      print_frame_info_base (fi, trailing_level + i, 0, 1);
      if (show_locals)
	print_frame_local_vars(fi, 1, gdb_stdout);
    }

  /* If we've stopped before the end, mention that.  */
  if (fi && from_tty)
    printf_filtered ("(More stack frames follow...)\n");
}

static void
backtrace_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct cleanup    *old_chain = (struct cleanup *)NULL;
  char              **argv = (char **)NULL;
  int               argIndicatingFullTrace = (-1), totArgLen = 0, argc = 0;
  char              *argPtr = arg;

  if (arg != (char *)NULL)
    {
      int i;

      argv = buildargv(arg);
      old_chain = make_cleanup ((make_cleanup_func) freeargv, (char *)argv);
      argc = 0;
      for (i = 0; (argv[i] != (char *)NULL); i++)
        {
          int j;

          for (j = 0; (j < strlen(argv[i])); j++)
            argv[i][j] = tolower(argv[i][j]);

          if (argIndicatingFullTrace < 0 && subsetCompare(argv[i], "full"))
            argIndicatingFullTrace = argc;
          else
            {
              argc++;
              totArgLen += strlen(argv[i]);
            }
        }
      totArgLen += argc;
      if (argIndicatingFullTrace >= 0)
        {
          if (totArgLen > 0)
            {
              argPtr = (char *)xmalloc(totArgLen + 1);
              if (!argPtr)
                nomem(0);
              else
                {
                  memset(argPtr, 0, totArgLen + 1);
                  for (i = 0; (i < (argc + 1)); i++)
                    {
                      if (i != argIndicatingFullTrace)
                        {
                          strcat(argPtr, argv[i]);
                          strcat(argPtr, " ");
                        }
                    }
                }
            }
          else
            argPtr = (char *)NULL;
        }
    }

  backtrace_command_1 (argPtr, (argIndicatingFullTrace >= 0), from_tty);

  if (argIndicatingFullTrace >= 0 && totArgLen > 0)
    free(argPtr);

  if (old_chain)
    do_cleanups(old_chain);
}

static void
backtrace_full_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  backtrace_command_1 (arg, 1, from_tty);
}


/* Print the local variables of a block B active in FRAME.
   Return 1 if any variables were printed; 0 otherwise.  */

static int
print_block_frame_locals (b, fi, num_tabs, stream)
     struct block *b;
     register struct frame_info *fi;
     int num_tabs;
     register GDB_FILE *stream;
{
  int nsyms;
  register int i, j;
  register struct symbol *sym;
  register int values_printed = 0;

  nsyms = BLOCK_NSYMS (b);

  for (i = 0; i < nsyms; i++)
    {
      sym = BLOCK_SYM (b, i);
      switch (SYMBOL_CLASS (sym))
	{
	case LOC_LOCAL:
	case LOC_REGISTER:
	case LOC_STATIC:
	case LOC_BASEREG:
	  values_printed = 1;
	  for (j = 0; j < num_tabs; j++)
	    fputs_filtered("\t", stream);
	  fputs_filtered (SYMBOL_SOURCE_NAME (sym), stream);
	  fputs_filtered (" = ", stream);
	  print_variable_value (sym, fi, stream);
	  fprintf_filtered (stream, "\n");
	  break;

	default:
	  /* Ignore symbols which are not locals.  */
	  break;
	}
    }
  return values_printed;
}

/* Same, but print labels.  */

static int
print_block_frame_labels (b, have_default, stream)
     struct block *b;
     int *have_default;
     register GDB_FILE *stream;
{
  int nsyms;
  register int i;
  register struct symbol *sym;
  register int values_printed = 0;

  nsyms = BLOCK_NSYMS (b);

  for (i = 0; i < nsyms; i++)
    {
      sym = BLOCK_SYM (b, i);
      if (STREQ (SYMBOL_NAME (sym), "default"))
	{
	  if (*have_default)
	    continue;
	  *have_default = 1;
	}
      if (SYMBOL_CLASS (sym) == LOC_LABEL)
	{
	  struct symtab_and_line sal;
	  sal = find_pc_line (SYMBOL_VALUE_ADDRESS (sym), 0);
	  values_printed = 1;
	  fputs_filtered (SYMBOL_SOURCE_NAME (sym), stream);
	  if (addressprint)
	    {
	      fprintf_filtered (stream, " ");
	      print_address_numeric (SYMBOL_VALUE_ADDRESS (sym), 1, stream);
	    }
	  fprintf_filtered (stream, " in file %s, line %d\n",
			    sal.symtab->filename, sal.line);
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

static void
print_frame_local_vars (fi, num_tabs, stream)
     register struct frame_info *fi;
     register int num_tabs;
     register GDB_FILE *stream;
{
  register struct block *block = get_frame_block (fi);
  register int values_printed = 0;

  if (block == 0)
    {
      fprintf_filtered (stream, "No symbol table info available.\n");
      return;
    }
  
  while (block != 0)
    {
      if (print_block_frame_locals (block, fi, num_tabs, stream))
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
    }
}

/* Same, but print labels.  */

static void
print_frame_label_vars (fi, this_level_only, stream)
     register struct frame_info *fi;
     int this_level_only;
     register GDB_FILE *stream;
{
  register struct blockvector *bl;
  register struct block *block = get_frame_block (fi);
  register int values_printed = 0;
  int index, have_default = 0;
  char *blocks_printed;
  CORE_ADDR pc = fi->pc;

  if (block == 0)
    {
      fprintf_filtered (stream, "No symbol table info available.\n");
      return;
    }

  bl = blockvector_for_pc (BLOCK_END (block) - 4, &index);
  blocks_printed = (char *) alloca (BLOCKVECTOR_NBLOCKS (bl) * sizeof (char));
  memset (blocks_printed, 0, BLOCKVECTOR_NBLOCKS (bl) * sizeof (char));

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
	  if (blocks_printed[index] == 0)
	    {
	      if (print_block_frame_labels (BLOCKVECTOR_BLOCK (bl, index), &have_default, stream))
		values_printed = 1;
	      blocks_printed[index] = 1;
	    }
	  index++;
	}
      if (have_default)
	return;
      if (values_printed && this_level_only)
	return;

      /* After handling the function's top-level block, stop.
	 Don't continue to its superblock, the block of
	 per-file symbols.  */
      if (BLOCK_FUNCTION (block))
	break;
      block = BLOCK_SUPERBLOCK (block);
    }

  if (!values_printed && !this_level_only)
    {
      fprintf_filtered (stream, "No catches.\n");
    }
}

/* ARGSUSED */
void
locals_info (args, from_tty)
     char *args;
     int from_tty;
{
  if (!selected_frame)
    error ("No frame selected.");
  print_frame_local_vars (selected_frame, 0, gdb_stdout);
}

static void
catch_info (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  struct symtab_and_line * sal;

  /* Check for target support for exception handling */ 
  sal = target_enable_exception_callback (EX_EVENT_CATCH, 1);
  if (sal)
    {
      /* Currently not handling this */
      /* Ideally, here we should interact with the C++ runtime
         system to find the list of active handlers, etc. */
      fprintf_filtered (gdb_stdout, "Info catch not supported with this target/compiler combination.\n");
#if 0
  if (!selected_frame)
    error ("No frame selected.");
#endif
    }
  else
    {
      /* Assume g++ compiled code -- old v 4.16 behaviour */ 
      if (!selected_frame)
        error ("No frame selected.");
      
      print_frame_label_vars (selected_frame, 0, gdb_stdout);
    }
}

static void
print_frame_arg_vars (fi, stream)
     register struct frame_info *fi;
     register GDB_FILE *stream;
{
  struct symbol *func = get_frame_function (fi);
  register struct block *b;
  int nsyms;
  register int i;
  register struct symbol *sym, *sym2;
  register int values_printed = 0;

  if (func == 0)
    {
      fprintf_filtered (stream, "No symbol table info available.\n");
      return;
    }

  b = SYMBOL_BLOCK_VALUE (func);
  nsyms = BLOCK_NSYMS (b);

  for (i = 0; i < nsyms; i++)
    {
      sym = BLOCK_SYM (b, i);
      switch (SYMBOL_CLASS (sym))
	{
	case LOC_ARG:
	case LOC_LOCAL_ARG:
	case LOC_REF_ARG:
	case LOC_REGPARM:
	case LOC_REGPARM_ADDR:
	case LOC_BASEREG_ARG:
	  values_printed = 1;
	  fputs_filtered (SYMBOL_SOURCE_NAME (sym), stream);
	  fputs_filtered (" = ", stream);

	  /* We have to look up the symbol because arguments can have
	     two entries (one a parameter, one a local) and the one we
	     want is the local, which lookup_symbol will find for us.
	     This includes gcc1 (not gcc2) on the sparc when passing a
	     small structure and gcc2 when the argument type is float
	     and it is passed as a double and converted to float by
	     the prologue (in the latter case the type of the LOC_ARG
	     symbol is double and the type of the LOC_LOCAL symbol is
	     float).  There are also LOC_ARG/LOC_REGISTER pairs which
	     are not combined in symbol-reading.  */

	  sym2 = lookup_symbol (SYMBOL_NAME (sym),
			b, VAR_NAMESPACE, (int *)NULL, (struct symtab **)NULL);
	  print_variable_value (sym2, fi, stream);
	  fprintf_filtered (stream, "\n");
	  break;

	default:
	  /* Don't worry about things which aren't arguments.  */
	  break;
	}
    }

  if (!values_printed)
    {
      fprintf_filtered (stream, "No arguments.\n");
    }
}

void
args_info (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  if (!selected_frame)
    error ("No frame selected.");
  print_frame_arg_vars (selected_frame, gdb_stdout);
}


static void
args_plus_locals_info (ignore, from_tty)
     char *ignore;
     int from_tty;
{
  args_info(ignore, from_tty);
  locals_info(ignore, from_tty);
}


/* Select frame FI, and note that its stack level is LEVEL.
   LEVEL may be -1 if an actual level number is not known.  */

void
select_frame (fi, level)
     struct frame_info *fi;
     int level;
{
  register struct symtab *s;

  selected_frame = fi;
  selected_frame_level = level;
  if (selected_frame_level_changed_hook)
    selected_frame_level_changed_hook (level);

  /* Ensure that symbols for this frame are read in.  Also, determine the
     source language of this frame, and switch to it if desired.  */
  if (fi)
  {
    s = find_pc_symtab (fi->pc);
    if (s 
	&& s->language != current_language->la_language
	&& s->language != language_unknown
	&& language_mode == language_mode_auto) {
      set_language(s->language);
    }
    /* elz: this if here fixes the problem with the pc not being displayed
       in the tui asm layout, with no debug symbols. The value of s 
       would be 0 here, and select_source_symtab would abort the
       command by calling the 'error' function*/
    if (s)
      {
	TUIDO(((TuiOpaqueFuncPtr)tui_vSelectSourceSymtab, s));
      }
  }
}


/* Select frame FI, noting that its stack level is LEVEL.  Also print
   the stack frame and show the source if this is the tui version.  */
void
select_and_print_frame(fi, level)
     struct frame_info *fi;
     int level;
{
  select_frame(fi, level);
  if (fi)
    {
      print_stack_frame(fi, level, 1);
      TUIDO(((TuiOpaqueFuncPtr)tui_vCheckDataValues, fi));
    }
}


/* Select frame FI, noting that its stack level is LEVEL.  Be silent if
   not the TUI */
void
select_and_maybe_print_frame (fi, level)
     struct frame_info *fi;
     int level;
{
  if (!tui_version)
    select_frame(fi, level);
  else
    select_and_print_frame(fi, level);
}


/* Store the selected frame and its level into *FRAMEP and *LEVELP.
   If there is no selected frame, *FRAMEP is set to NULL.  */

void
record_selected_frame (frameaddrp, levelp)
     CORE_ADDR *frameaddrp;
     int *levelp;
{
  *frameaddrp = selected_frame ? selected_frame->frame : 0;
  *levelp = selected_frame_level;
}

/* Return the symbol-block in which the selected frame is executing.
   Can return zero under various legitimate circumstances.  */

struct block *
get_selected_block ()
{
  if (!target_has_stack)
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

struct frame_info *
find_relative_frame (frame, level_offset_ptr)
     register struct frame_info *frame;
     register int *level_offset_ptr;
{
  register struct frame_info *prev;
  register struct frame_info *frame1;

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
  /* Going down is just as simple.  */
  if (*level_offset_ptr < 0)
    {
      while (*level_offset_ptr < 0) {
	frame1 = get_next_frame (frame);
	if (!frame1)
	  break;
	frame = frame1;
	(*level_offset_ptr)++;
      }
    }
  return frame;
}

/* The "select_frame" command.  With no arg, NOP.
   With arg LEVEL_EXP, select the frame at level LEVEL if it is a
   valid level.  Otherwise, treat level_exp as an address expression
   and select it.  See parse_frame_specification for more info on proper
   frame expressions. */

/* ARGSUSED */
static void
select_frame_command (level_exp, from_tty)
     char *level_exp;
     int from_tty;
{
  register struct frame_info *frame, *frame1;
  unsigned int level = 0;

  if (!target_has_stack)
    error ("No stack.");

  frame = parse_frame_specification (level_exp);

  /* Try to figure out what level this frame is.  But if there is
     no current stack, don't error out -- let the user set one.  */
  frame1 = 0;
  if (get_current_frame()) {
    for (frame1 = get_prev_frame (0);
	 frame1 && frame1 != frame;
	 frame1 = get_prev_frame (frame1))
      level++;
  }

  if (!frame1)
    level = 0;

  select_frame (frame, level);
}

/* The "frame" command.  With no arg, print selected frame briefly.
   With arg, behaves like select_frame and then prints the selected
   frame.  */

void
frame_command (level_exp, from_tty)
     char *level_exp;
     int from_tty;
{
  select_frame_command (level_exp, from_tty);
  show_and_print_stack_frame (selected_frame, selected_frame_level, 1);
}

/* The XDB Compatibility command to print the current frame. */

void
current_frame_command (level_exp, from_tty)
     char *level_exp;
     int from_tty;
{
  if (target_has_stack == 0 || selected_frame == 0)
    error ("No stack."); 
 print_only_stack_frame (selected_frame, selected_frame_level, 1);
  }

/* Select the frame up one or COUNT stack levels
   from the previously selected frame, and print it briefly.  */

/* ARGSUSED */
static void
up_silently_base (count_exp)
     char *count_exp;
{
  register struct frame_info *fi;
  int count = 1, count1;
  if (count_exp)
    count = parse_and_eval_address (count_exp);
  count1 = count;
  
  if (target_has_stack == 0 || selected_frame == 0)
    error ("No stack.");

  fi = find_relative_frame (selected_frame, &count1);
  if (count1 != 0 && count_exp == 0)
    error ("Initial frame selected; you cannot go up.");
  select_frame (fi, selected_frame_level + count - count1);
}

static void
up_silently_command (count_exp, from_tty)
     char *count_exp;
     int from_tty;
{
  up_silently_base(count_exp);
  if (tui_version)
    print_stack_frame (selected_frame, selected_frame_level, 1);
}

static void
up_command (count_exp, from_tty)
     char *count_exp;
     int from_tty;
{
  up_silently_base (count_exp);
  show_and_print_stack_frame (selected_frame, selected_frame_level, 1);
}

/* Select the frame down one or COUNT stack levels
   from the previously selected frame, and print it briefly.  */

/* ARGSUSED */
static void
down_silently_base (count_exp)
     char *count_exp;
{
  register struct frame_info *frame;
  int count = -1, count1;
  if (count_exp)
    count = - parse_and_eval_address (count_exp);
  count1 = count;
  
  if (target_has_stack == 0 || selected_frame == 0)
    error ("No stack.");

  frame = find_relative_frame (selected_frame, &count1);
  if (count1 != 0 && count_exp == 0)
    {

      /* We only do this if count_exp is not specified.  That way "down"
	 means to really go down (and let me know if that is
	 impossible), but "down 9999" can be used to mean go all the way
	 down without getting an error.  */

      error ("Bottom (i.e., innermost) frame selected; you cannot go down.");
    }

  select_frame (frame, selected_frame_level + count - count1);
}

/* ARGSUSED */
static void
down_silently_command (count_exp, from_tty)
     char *count_exp;
     int from_tty;
{
  down_silently_base (count_exp);
  if (tui_version)
    print_stack_frame (selected_frame, selected_frame_level, 1);
}

static void
down_command (count_exp, from_tty)
     char *count_exp;
     int from_tty;
{
  down_silently_base (count_exp);
  show_and_print_stack_frame (selected_frame, selected_frame_level, 1);
}

static void
return_command (retval_exp, from_tty)
     char *retval_exp;
     int from_tty;
{
  struct symbol *thisfun;
  CORE_ADDR selected_frame_addr;
  CORE_ADDR selected_frame_pc;
  struct frame_info *frame;
  value_ptr return_value = NULL;

  if (selected_frame == NULL)
    error ("No selected frame.");
  thisfun = get_frame_function (selected_frame);
  selected_frame_addr = FRAME_FP (selected_frame);
  selected_frame_pc = selected_frame->pc;

  /* Compute the return value (if any -- possibly getting errors here).  */

  if (retval_exp)
    {
      struct type *return_type = NULL;

      return_value = parse_and_eval (retval_exp);

      /* Cast return value to the return type of the function.  */
      if (thisfun != NULL)
	return_type = TYPE_TARGET_TYPE (SYMBOL_TYPE (thisfun));
      if (return_type == NULL)
	return_type = builtin_type_int;
      return_value = value_cast (return_type, return_value);

      /* Make sure we have fully evaluated it, since
	 it might live in the stack frame we're about to pop.  */
      if (VALUE_LAZY (return_value))
	value_fetch_lazy (return_value);
    }

  /* If interactive, require confirmation.  */

  if (from_tty)
    {
      if (thisfun != 0)
	{
	  if (!query ("Make %s return now? ", SYMBOL_SOURCE_NAME (thisfun)))
	    {
	      error ("Not confirmed.");
	      /* NOTREACHED */
	    }
	}
      else
	if (!query ("Make selected stack frame return now? "))
	  error ("Not confirmed.");
    }

  /* Do the real work.  Pop until the specified frame is current.  We
     use this method because the selected_frame is not valid after
     a POP_FRAME.  The pc comparison makes this work even if the
     selected frame shares its fp with another frame.  */

  while (selected_frame_addr != (frame = get_current_frame())->frame
	 || selected_frame_pc != frame->pc)
    POP_FRAME;

  /* Then pop that frame.  */

  POP_FRAME;

  /* Compute the return value (if any) and store in the place
     for return values.  */

  if (retval_exp)
    set_return_value (return_value);

  /* If interactive, print the frame that is now current.  */

  if (from_tty)
    frame_command ("0", 1);
  else
    select_frame_command ("0", 0);
}

/* Sets the scope to input function name, provided that the
   function is within the current stack frame */

struct function_bounds
{
  CORE_ADDR low, high;
};

static void
func_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  struct frame_info *fp;
  int found = 0;
  struct symtabs_and_lines sals;
  int i;
  int level = 1;
  struct function_bounds *func_bounds = (struct function_bounds *) NULL;

  if (arg != (char *) NULL)
    return;

  fp = parse_frame_specification ("0");
  sals = decode_line_spec (arg, 1);
  func_bounds = (struct function_bounds *) xmalloc (
			      sizeof (struct function_bounds) * sals.nelts);
  for (i = 0; (i < sals.nelts && !found); i++)
    {
      if (sals.sals[i].pc == (CORE_ADDR) 0 ||
	  find_pc_partial_function (sals.sals[i].pc,
				    (char **) NULL,
				    &func_bounds[i].low,
				    &func_bounds[i].high) == 0)
	{
	  func_bounds[i].low =
	    func_bounds[i].high = (CORE_ADDR) NULL;
	}
    }

  do
    {
      for (i = 0; (i < sals.nelts && !found); i++)
	found = (fp->pc >= func_bounds[i].low &&
		 fp->pc < func_bounds[i].high);
      if (!found)
	{
	  level = 1;
	  fp = find_relative_frame (fp, &level);
	}
    }
  while (!found && level == 0);

  if (func_bounds)
    free (func_bounds);

  if (!found)
    printf_filtered ("'%s' not within current stack frame.\n", arg);
  else if (fp != selected_frame)
    select_and_print_frame (fp, level);
}

/* Gets the language of the current frame.  */

enum language
get_frame_language ()
{
  register struct symtab *s;
  enum language flang;		/* The language of the current frame */
   
  if (selected_frame)
    {
      s = find_pc_symtab(selected_frame->pc);
      if (s)
	flang = s->language;
      else
	flang = language_unknown;
    }
  else
    flang = language_unknown;

  return flang;
}

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
  add_com ("up-silently", class_support, up_silently_command,
	   "Same as the `up' command, but does not print anything.\n\
This is useful in command scripts.");

  add_com ("down", class_stack, down_command,
	   "Select and print stack frame called by this one.\n\
An argument says how many frames down to go.");
  add_com_alias ("do", "down", class_stack, 1);
  add_com_alias ("dow", "down", class_stack, 1);
  add_com ("down-silently", class_support, down_silently_command,
	   "Same as the `down' command, but does not print anything.\n\
This is useful in command scripts.");

  add_com ("frame", class_stack, frame_command,
	   "Select and print a stack frame.\n\
With no argument, print the selected stack frame.  (See also \"info frame\").\n\
An argument specifies the frame to select.\n\
It can be a stack frame number or the address of the frame.\n\
With argument, nothing is printed if input is coming from\n\
a command file or a user-defined command.");

  add_com_alias ("f", "frame", class_stack, 1);

  if (xdb_commands)
    {
      add_com("L", class_stack, current_frame_command, 
              "Print the current stack frame.\n");
      add_com_alias ("V", "frame", class_stack, 1);
    }
  add_com ("select-frame", class_stack, select_frame_command,
	   "Select a stack frame without printing anything.\n\
An argument specifies the frame to select.\n\
It can be a stack frame number or the address of the frame.\n");

  add_com ("backtrace", class_stack, backtrace_command,
	   "Print backtrace of all stack frames, or innermost COUNT frames.\n\
With a negative argument, print outermost -COUNT frames.\n\
Use of the 'full' qualifier also prints the values of the local variables.\n");
  add_com_alias ("bt", "backtrace", class_stack, 0);
  if (xdb_commands)
    {
      add_com_alias ("t", "backtrace", class_stack, 0);
      add_com ("T", class_stack, backtrace_full_command,
	   "Print backtrace of all stack frames, or innermost COUNT frames \n\
and the values of the local variables.\n\
With a negative argument, print outermost -COUNT frames.\n\
Usage: T <count>\n");
    }

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
  if (xdb_commands)
      add_com("l", class_info, args_plus_locals_info, 
	    "Argument and local variables of current stack frame.");

  if (dbx_commands)
      add_com("func", class_stack, func_command, 
         "Select the stack frame that contains <func>.\nUsage: func <name>\n");

  add_info ("catch", catch_info,
	    "Exceptions that can be caught in the current stack frame.");

#if 0
  add_cmd ("backtrace-limit", class_stack, set_backtrace_limit_command, 
	   "Specify maximum number of frames for \"backtrace\" to print by default.",
	   &setlist);
  add_info ("backtrace-limit", backtrace_limit_info,
	    "The maximum number of frames for \"backtrace\" to print by default.");
#endif
}
