/* bcdefs.h:  The single file to include all constants and type definitions. */

/*  This file is part of GNU bc.
    Copyright (C) 1991, 1992, 1993, 1994, 1997 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

/* Include the configuration file. */
#include "config.h"

/* Standard includes for all files. */
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#else
#include <string.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* Include the other definitions. */
#include "const.h"
#include "number.h"


/* These definitions define all the structures used in
   code and data storage.  This includes the representation of
   labels.   The "guiding" principle is to make structures that
   take a minimum of space when unused but can be built to contain
   the full structures.  */

/* Labels are first.  Labels are generated sequentially in functions
   and full code.  They just "point" to a single bye in the code.  The
   "address" is the byte number.  The byte number is used to get an
   actual character pointer. */

typedef struct bc_label_group
    {
      long l_adrs [ BC_LABEL_GROUP ];
      struct bc_label_group *l_next;
    } bc_label_group;

/* Argument list.  Recorded in the function so arguments can
   be checked at call time. */

typedef struct arg_list
    {
      int av_name;
      int arg_is_var;		/* Extension ... variable parameters. */
      struct arg_list *next;
    } arg_list;

/* Each function has its own code segments and labels.  There can be
   no jumps between functions so labels are unique to a function. */

typedef struct 
    {
      char f_defined;   /* Is this function defined yet. */
      char *f_body[BC_MAX_SEGS];
      int   f_code_size;
      bc_label_group *f_label;
      arg_list *f_params;
      arg_list *f_autos;
    } bc_function;

/* Code addresses. */
typedef struct {
      int pc_func;
      int pc_addr;
    } program_counter;


/* Variables are "pushable" (auto) and thus we need a stack mechanism.
   This is built into the variable record. */

typedef struct bc_var
    {
      bc_num v_value;
      struct bc_var *v_next;
    }  bc_var;


/* bc arrays can also be "auto" variables and thus need the same
   kind of stacking mechanisms. */

typedef struct bc_array_node
    {
      union
	{
	  bc_num n_num [NODE_SIZE];
	  struct bc_array_node *n_down [NODE_SIZE];
	} n_items;
    } bc_array_node;

typedef struct bc_array
    {
      bc_array_node *a_tree;
      short a_depth;
    } bc_array;

typedef struct bc_var_array
    {
      bc_array *a_value;
      char      a_param;
      struct bc_var_array *a_next;
    } bc_var_array;


/* For the stacks, execution and function, we need records to allow
   for arbitrary size. */

typedef struct estack_rec {
	bc_num s_num;
	struct estack_rec *s_next;
} estack_rec;

typedef struct fstack_rec {
	int  s_val;
	struct fstack_rec *s_next;
} fstack_rec;


/* The following are for the name tree. */

typedef struct id_rec {
	char  *id;      /* The program name. */
			/* A name == 0 => nothing assigned yet. */
	int   a_name;   /* The array variable name (number). */
	int   f_name;   /* The function name (number).  */
	int   v_name;   /* The variable name (number).  */
        short balance;  /* For the balanced tree. */
	struct id_rec *left, *right; /* Tree pointers. */
} id_rec;


/* A list of files to process. */

typedef struct file_node {
	char *name;
	struct file_node *next;
} file_node;

