/* cond.c - conditional assembly pseudo-ops, and .include
   Copyright (C) 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef lint
static char rcsid[] = "$FreeBSD: src/gnu/usr.bin/as/cond.c,v 1.6 1999/08/27 23:34:13 peter Exp $";
#endif

#include "as.h"

#include "obstack.h"

/* This is allocated to grow and shrink as .ifdef/.endif pairs are scanned. */
struct obstack cond_obstack;

struct file_line
{
  char *file;
  unsigned int line;
};				/* file_line */

/* This is what we push and pop. */
struct conditional_frame
  {
    struct file_line if_file_line;	/* the source file & line number of the "if" */
    struct file_line else_file_line;	/* the source file & line of the "else" */
    struct conditional_frame *previous_cframe;
    int else_seen;		/* have we seen an else yet? */
    int ignoring;		/* if we are currently ignoring input. */
    int dead_tree;		/* if a conditional at a higher level is ignoring input. */
  };				/* conditional_frame */

static void initialize_cframe PARAMS ((struct conditional_frame *cframe));

static struct conditional_frame *current_cframe = NULL;

void
s_ifdef (arg)
     int arg;
{
  register char *name;		/* points to name of symbol */
  register struct symbol *symbolP;	/* Points to symbol */
  struct conditional_frame cframe;

  SKIP_WHITESPACE ();		/* Leading whitespace is part of operand. */
  name = input_line_pointer;

  if (!is_name_beginner (*name))
    {
      as_bad ("invalid identifier for \".ifdef\"");
      obstack_1grow (&cond_obstack, 0);
    }
  else
    {
      get_symbol_end ();
      ++input_line_pointer;
      symbolP = symbol_find (name);

      initialize_cframe (&cframe);
      cframe.ignoring = cframe.dead_tree || !((symbolP != 0) ^ arg);
      current_cframe = (struct conditional_frame *) obstack_copy (&cond_obstack, &cframe, sizeof (cframe));
    }				/* if a valid identifyer name */

  return;
}				/* s_ifdef() */

void
s_if (arg)
     int arg;
{
  expressionS operand;
  struct conditional_frame cframe;

  SKIP_WHITESPACE ();		/* Leading whitespace is part of operand. */
  expression (&operand);

#ifdef notyet
  if (operand.X_op != O_constant)
    as_bad ("non-constant expression in \".if\" statement");
#else
  if (operand.X_add_symbol != NULL || operand.X_subtract_symbol != NULL) {
	as_bad("non-constant expression in \".if\" statement");
  } /* bad condition */
#endif

  /* If the above error is signaled, this will dispatch
     using an undefined result.  No big deal.  */
  initialize_cframe (&cframe);
  cframe.ignoring = cframe.dead_tree || !((operand.X_add_number != 0) ^ arg);
  current_cframe = (struct conditional_frame *) obstack_copy (&cond_obstack, &cframe, sizeof (cframe));
  return;
}				/* s_if() */

void
s_endif (arg)
     int arg;
{
  struct conditional_frame *hold;

  if (current_cframe == NULL)
    {
      as_bad ("\".endif\" without \".if\"");
    }
  else
    {
      hold = current_cframe;
      current_cframe = current_cframe->previous_cframe;
      obstack_free (&cond_obstack, hold);
    }				/* if one pop too many */

  return;
}				/* s_endif() */

void
s_else (arg)
     int arg;
{
  if (current_cframe == NULL)
    {
      as_bad (".else without matching .if - ignored");

    }
  else if (current_cframe->else_seen)
    {
      as_bad ("duplicate \"else\" - ignored");
      as_bad_where (current_cframe->else_file_line.file,
		    current_cframe->else_file_line.line,
		    "here is the previous \"else\"");
      as_bad_where (current_cframe->if_file_line.file,
		    current_cframe->if_file_line.line,
		    "here is the previous \"if\"");
    }
  else
    {
      as_where (&current_cframe->else_file_line.file,
		&current_cframe->else_file_line.line);

      if (!current_cframe->dead_tree)
	{
	  current_cframe->ignoring = !current_cframe->ignoring;
	}			/* if not a dead tree */

      current_cframe->else_seen = 1;
    }				/* if error else do it */

  return;
}				/* s_else() */

void
s_ifeqs (arg)
     int arg;
{
  as_bad ("ifeqs not implemented.");

  return;
}				/* s_ifeqs() */

void
s_end (arg)
     int arg;
{
  return;
}				/* s_end() */

int
ignore_input ()
{
  /* We cannot ignore certain pseudo ops.  */
  if (input_line_pointer[-1] == '.'
      && ((input_line_pointer[0] == 'i'
	   && (!strncmp (input_line_pointer, "if", 2)
	       || !strncmp (input_line_pointer, "ifdef", 5)
	       || !strncmp (input_line_pointer, "ifndef", 6)))
	  || (input_line_pointer[0] == 'e'
	      && (!strncmp (input_line_pointer, "else", 4)
		  || !strncmp (input_line_pointer, "endif", 5)))))
    {
      return 0;
    }

  return ((current_cframe != NULL) && (current_cframe->ignoring));
}				/* ignore_input() */

static void
initialize_cframe (cframe)
     struct conditional_frame *cframe;
{
  memset (cframe, 0, sizeof (*cframe));
  as_where (&cframe->if_file_line.file,
	    &cframe->if_file_line.line);
  cframe->previous_cframe = current_cframe;
  cframe->dead_tree = current_cframe != NULL && current_cframe->ignoring;

  return;
}				/* initialize_cframe() */

/*
 * Local Variables:
 * fill-column: 131
 * comment-column: 0
 * End:
 */

/* end of cond.c */
