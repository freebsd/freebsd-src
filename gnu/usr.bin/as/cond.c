/* cond.c - conditional assembly pseudo-ops, and .include
   Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.
   
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
static char rcsid[] = "$Id: cond.c,v 1.1 1993/11/03 00:51:20 paul Exp $";
#endif

#include "as.h"

#include "obstack.h"

/* This is allocated to grow and shrink as .ifdef/.endif pairs are scanned. */
struct obstack cond_obstack;

struct file_line {
	char *logical_file;
	int logical_line;
	char *physical_file;
	int physical_line;
}; /* file_line */

/* This is what we push and pop. */
struct conditional_frame {
	struct file_line if_file_line;	/* the source file & line number of the "if" */
	struct file_line else_file_line; /* the source file & line of the "else" */
	struct conditional_frame *previous_cframe;
	int else_seen; /* have we seen an else yet? */
	int ignoring;	     /* if we are currently ignoring input. */
	int dead_tree;	     /* if a conditional at a higher level is ignoring input. */
}; /* conditional_frame */

#if __STDC__ == 1

static void get_file_line(struct file_line *into);
static void initialize_cframe(struct conditional_frame *cframe);
static void set_file_line(struct file_line *from);

#else

static void get_file_line();
static void initialize_cframe();
static void set_file_line();

#endif

static struct conditional_frame *current_cframe = NULL;

void s_ifdef(arg)
int arg;
{
	register char *name; /* points to name of symbol */
	register struct symbol *symbolP; /* Points to symbol */
	struct conditional_frame cframe;
	
	SKIP_WHITESPACE();		/* Leading whitespace is part of operand. */
	name = input_line_pointer;
	
	if (!is_name_beginner(*name)) {
		as_bad("invalid identifier for \".ifdef\"");
		obstack_1grow(&cond_obstack, 0);
	} else {
		get_symbol_end();
		++input_line_pointer;
		symbolP = symbol_find(name);
		
		initialize_cframe(&cframe);
		cframe.ignoring = cframe.dead_tree && !((symbolP != 0) ^ arg);
		current_cframe = (struct conditional_frame *) obstack_copy(&cond_obstack, &cframe, sizeof(cframe));
	} /* if a valid identifyer name */
	
	return;
} /* s_ifdef() */

void s_if(arg)
int arg;
{
	expressionS operand;
	struct conditional_frame cframe;
	
	SKIP_WHITESPACE(); /* Leading whitespace is part of operand. */
	expr(0, &operand);
	
	if (operand.X_add_symbol != NULL
	    || operand.X_subtract_symbol != NULL) {
		as_bad("non-constant expression in \".if\" statement");
	} /* bad condition */
	
	/* If the above error is signaled, this will dispatch
	   using an undefined result.  No big deal.  */
	initialize_cframe(&cframe);
	cframe.ignoring = cframe.dead_tree || !((operand.X_add_number != 0) ^ arg);
	current_cframe = (struct conditional_frame *) obstack_copy(&cond_obstack, &cframe, sizeof(cframe));
	return;
} /* s_if() */

void s_endif(arg)
int arg;
{
	struct conditional_frame *hold;
	
	if (current_cframe == NULL) {
		as_bad("\".endif\" without \".if\"");
	} else {
		hold = current_cframe;
		current_cframe = current_cframe->previous_cframe;
		obstack_free(&cond_obstack, hold);
	} /* if one pop too many */
	
	return;
} /* s_endif() */

void s_else(arg)
int arg;
{
	if (current_cframe == NULL) {
		as_bad(".else without matching .if - ignored");
		
	} else if (current_cframe->else_seen) {
		struct file_line hold;
		as_bad("duplicate \"else\" - ignored");
		
		get_file_line(&hold);
		set_file_line(&current_cframe->else_file_line);
		as_bad("here is the previous \"else\".");
		set_file_line(&current_cframe->if_file_line);
		as_bad("here is the matching \".if\".");
		set_file_line(&hold);
		
	} else {
		get_file_line(&current_cframe->else_file_line);
		
		if (!current_cframe->dead_tree) {
			current_cframe->ignoring = !current_cframe->ignoring;
		} /* if not a dead tree */
		
		current_cframe->else_seen = 1;
	} /* if error else do it */
	
	return;
} /* s_else() */

void s_ifeqs(arg)
int arg;
{
	as_bad("ifeqs not implemented.");
	
	return;
} /* s_ifeqs() */

void s_end(arg)
int arg;
{
	return;
} /* s_end() */

int ignore_input() {
	char *ptr = obstack_next_free (&cond_obstack);
	
	/* We cannot ignore certain pseudo ops.  */
	if (input_line_pointer[-1] == '.'
	    && ((input_line_pointer[0] == 'i'
		 && (!strncmp (input_line_pointer, "if", 2)
		     || !strncmp (input_line_pointer, "ifdef", 5)
		     || !strncmp (input_line_pointer, "ifndef", 6)))
		|| (input_line_pointer[0] == 'e'
		    && (!strncmp (input_line_pointer, "else", 4)
			|| !strncmp (input_line_pointer, "endif", 5))))) {
		return 0;
	}
	
	return((current_cframe != NULL) && (current_cframe->ignoring));
} /* ignore_input() */

static void initialize_cframe(cframe)
struct conditional_frame *cframe;
{
	memset(cframe, 0, sizeof(*cframe));
	get_file_line(&(cframe->if_file_line));
	cframe->previous_cframe = current_cframe;
	cframe->dead_tree = current_cframe != NULL && current_cframe->ignoring;
	
	return;
} /* initialize_cframe() */

static void get_file_line(into)
struct file_line *into;
{
	extern char *logical_input_file;
	extern char *physical_input_file;
	extern int logical_input_line;
	extern int physical_input_line;
	
	into->logical_file = logical_input_file;
	into->logical_line = logical_input_line;
	into->physical_file = physical_input_file;
	into->physical_line = physical_input_line;
	
	return;
} /* get_file_line() */

static void set_file_line(from)
struct file_line *from;
{
	extern char *logical_input_file;
	extern char *physical_input_file;
	extern int logical_input_line;
	extern int physical_input_line;
	
	logical_input_file = from->logical_file;
	logical_input_line = from->logical_line;
	physical_input_file = from->physical_file;
	physical_input_line = from->physical_line;
	return;
} /* set_file_line() */

/*
 * Local Variables:
 * fill-column: 131
 * comment-column: 0
 * End:
 */

/* end of cond.c */
