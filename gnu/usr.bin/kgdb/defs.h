/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 *
 *	@(#)defs.h	6.3 (Berkeley) 5/8/91
 */

/* Basic definitions for GDB, the GNU debugger.
   Copyright (C) 1986, 1989 Free Software Foundation, Inc.

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

#define CORE_ADDR unsigned int

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

extern char *savestring ();
extern char *concat ();
extern char *xmalloc (), *xrealloc ();
extern int parse_escape ();
extern char *reg_names[];

/* Various possibilities for alloca.  */
#ifdef sparc
#include <alloca.h>
#else
#ifdef __GNUC__
#define alloca __builtin_alloca
#else
extern char *alloca ();
#endif
#endif

extern int quit_flag;

extern int immediate_quit;

#define QUIT { if (quit_flag) quit (); }

/* Notes on classes: class_alias is for alias commands which are not
   abbreviations of the original command.  */

enum command_class
{
  no_class = -1, class_run = 0, class_vars, class_stack,
  class_files, class_support, class_info, class_breakpoint,
  class_alias, class_obscure, class_user,
};

/* the cleanup list records things that have to be undone
   if an error happens (descriptors to be closed, memory to be freed, etc.)
   Each link in the chain records a function to call and an
   argument to give it.

   Use make_cleanup to add an element to the cleanup chain.
   Use do_cleanups to do all cleanup actions back to a given
   point in the chain.  Use discard_cleanups to remove cleanups
   from the chain back to a given point, not doing them.  */

struct cleanup
{
  struct cleanup *next;
  void (*function) ();
  int arg;
};

extern void do_cleanups ();
extern void discard_cleanups ();
extern struct cleanup *make_cleanup ();
extern struct cleanup *save_cleanups ();
extern void restore_cleanups ();
extern void free_current_contents ();
extern void reinitialize_more_filter ();
extern void fputs_filtered ();
extern void fprintf_filtered ();
extern void printf_filtered ();
extern void print_spaces_filtered ();
extern char *tilde_expand ();

/* Structure for saved commands lines
   (for breakpoints, defined commands, etc).  */

struct command_line
{
  struct command_line *next;
  char *line;
  int type;			/* statement type */
#define CL_END 0
#define CL_NORMAL 1
#define CL_WHILE 2
#define CL_IF 3
#define CL_EXITLOOP 4
#define CL_NOP 5
  struct command_line *body;  /* body of loop for while, body of if */
  struct command_line *elsebody; /* body of else part of if */
};

extern struct command_line *read_command_lines ();
extern void do_command_lines();

/* String containing the current directory (what getwd would return).  */

char *current_directory;

