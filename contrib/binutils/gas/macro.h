/* macro.h - header file for macro support for gas and gasp
   Copyright 1994, 1995, 1996, 1997, 1998, 2000
   Free Software Foundation, Inc.

   Written by Steve and Judy Chamberlain of Cygnus Support,
      sac@cygnus.com

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
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef MACRO_H

#define MACRO_H

#include "ansidecl.h"
#include "sb.h"

/* Structures used to store macros.

   Each macro knows its name and included text.  It gets built with a
   list of formal arguments, and also keeps a hash table which points
   into the list to speed up formal search.  Each formal knows its
   name and its default value.  Each time the macro is expanded, the
   formals get the actual values attatched to them.  */

/* describe the formal arguments to a macro */

typedef struct formal_struct {
  struct formal_struct *next;	/* next formal in list */
  sb name;			/* name of the formal */
  sb def;			/* the default value */
  sb actual;			/* the actual argument (changed on each expansion) */
  int index;			/* the index of the formal 0..formal_count-1 */
} formal_entry;

/* Other values found in the index field of a formal_entry.  */
#define QUAL_INDEX (-1)
#define NARG_INDEX (-2)
#define LOCAL_INDEX (-3)

/* describe the macro.  */

typedef struct macro_struct {
  sb sub;			/* substitution text.  */
  int formal_count;		/* number of formal args.  */
  formal_entry *formals;	/* pointer to list of formal_structs */
  struct hash_control *formal_hash; /* hash table of formals.  */
} macro_entry;

/* Whether any macros have been defined.  */

extern int macro_defined;

/* The macro nesting level.  */

extern int macro_nest;

extern int buffer_and_nest
  PARAMS ((const char *, const char *, sb *, int (*) PARAMS ((sb *))));
extern void macro_init
  PARAMS ((int alternate, int mri, int strip_at,
	   int (*) PARAMS ((const char *, int, sb *, int *))));
extern void macro_mri_mode PARAMS ((int));
extern const char *define_macro
  PARAMS ((int idx, sb *in, sb *label, int (*get_line) PARAMS ((sb *)),
	   const char **namep));
extern int check_macro PARAMS ((const char *, sb *, int, const char **,
				macro_entry **));
extern void delete_macro PARAMS ((const char *));
extern const char *expand_irp
  PARAMS ((int, int, sb *, sb *, int (*) PARAMS ((sb *)), int));

#endif
