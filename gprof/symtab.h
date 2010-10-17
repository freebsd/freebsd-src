/* symtab.h

   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.

This file is part of GNU Binutils.

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

#ifndef symtab_h
#define symtab_h

/* For a profile to be intelligible to a human user, it is necessary
   to map code-addresses into source-code information.  Source-code
   information can be any combination of: (i) function-name, (ii)
   source file-name, and (iii) source line number.

   The symbol table is used to map addresses into source-code
   information.  */

#define NBBS 10

/* Symbol-entry.  For each external in the specified file we gather
   its address, the number of calls and compute its share of cpu time.  */
typedef struct sym
  {
    /* Common information:

       In the symbol-table, fields ADDR and FUNC_NAME are guaranteed
       to contain valid information.  FILE may be 0, if unknown and
       LINE_NUM maybe 0 if unknown.  */

    bfd_vma addr;		/* Address of entry point.  */
    bfd_vma end_addr;		/* End-address.  */
    const char *name;		/* Name of function this sym is from.  */
    Source_File *file;		/* Source file symbol comes from.  */
    int line_num;		/* Source line number.  */
    unsigned int		/* Boolean fields:  */
      is_func:1,		/*  Is this a function entry point?  */
      is_static:1,		/*  Is this a local (static) symbol?  */
      is_bb_head:1,		/*  Is this the head of a basic-blk?  */
      mapped:1,			/*  This symbol was mapped to another name.  */
      has_been_placed:1;	/*  Have we placed this symbol?  */
    unsigned long ncalls;	/* How many times executed  */
    int nuses;			/* How many times this symbol appears in
				   a particular context.  */
    bfd_vma bb_addr[NBBS];	/* Address of basic-block start.  */
    unsigned long bb_calls[NBBS];/* How many times basic-block was called.  */
    struct sym *next;		/* For building chains of syms.  */
    struct sym *prev;		/* For building chains of syms.  */

    /* Profile specific information:  */

    /* Histogram specific information:  */
    struct
      {
	double time;		/* (Weighted) ticks in this routine.  */
	bfd_vma scaled_addr;	/* Scaled entry point.  */
      }
    hist;

    /* Call-graph specific information:  */
    struct
      {
	unsigned long self_calls; /* How many calls to self.  */
	double child_time;	/* Cumulative ticks in children.  */
	int index;		/* Index in the graph list.  */
	int top_order;		/* Graph call chain top-sort order.  */
	bfd_boolean print_flag;	/* Should this be printed?  */
	struct
	  {
	    double fract;	/* What % of time propagates.  */
	    double self;	/* How much self time propagates.  */
	    double child;	/* How much child time propagates.  */
	  }
	prop;
	struct
	  {
	    int num;		/* Internal number of cycle on.  */
	    struct sym *head;	/* Head of cycle.  */
	    struct sym *next;	/* Next member of cycle.  */
	  }
	cyc;
	struct arc *parents;	/* List of caller arcs.  */
	struct arc *children;	/* List of callee arcs.  */
      }
    cg;
  }
Sym;

/* Symbol-tables are always assumed to be sorted
   in increasing order of addresses.  */
typedef struct
  {
    unsigned int len;		/* # of symbols in this table.  */
    Sym *base;			/* First element in symbol table.  */
    Sym *limit;			/* Limit = base + len.  */
  }
Sym_Table;

extern Sym_Table symtab;	/* The symbol table.  */

extern void sym_init        PARAMS ((Sym *));
extern void symtab_finalize PARAMS ((Sym_Table *));
#ifdef DEBUG
extern Sym *dbg_sym_lookup  PARAMS ((Sym_Table *, bfd_vma));
#endif
extern Sym *sym_lookup      PARAMS ((Sym_Table *, bfd_vma));
extern void find_call       PARAMS ((Sym *, bfd_vma, bfd_vma));

#endif /* symtab_h */
