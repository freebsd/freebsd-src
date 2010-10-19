/* call_graph.c  -  Create call graphs.

   Copyright 1999, 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "gprof.h"
#include "search_list.h"
#include "source.h"
#include "symtab.h"
#include "cg_arcs.h"
#include "call_graph.h"
#include "corefile.h"
#include "gmon_io.h"
#include "gmon_out.h"
#include "sym_ids.h"

void
cg_tally (bfd_vma from_pc, bfd_vma self_pc, unsigned long count)
{
  Sym *parent;
  Sym *child;

  parent = sym_lookup (&symtab, from_pc);
  child = sym_lookup (&symtab, self_pc);

  if (child == NULL || parent == NULL)
    return;

  /* If we're doing line-by-line profiling, both the parent and the
     child will probably point to line symbols instead of function
     symbols.  For the parent this is fine, since this identifies the
     line number in the calling routing, but the child should always
     point to a function entry point, so we back up in the symbol
     table until we find it.

     For normal profiling, is_func will be set on all symbols, so this
     code will do nothing.  */
  while (child >= symtab.base && ! child->is_func)
    --child;

  if (child < symtab.base)
    return;

  /* Keep arc if it is on INCL_ARCS table or if the INCL_ARCS table
     is empty and it is not in the EXCL_ARCS table.  */
  if (sym_id_arc_is_present (&syms[INCL_ARCS], parent, child)
      || (syms[INCL_ARCS].len == 0
	  && !sym_id_arc_is_present (&syms[EXCL_ARCS], parent, child)))
    {
      child->ncalls += count;
      DBG (TALLYDEBUG,
	   printf (_("[cg_tally] arc from %s to %s traversed %lu times\n"),
		   parent->name, child->name, count));
      arc_add (parent, child, count);
    }
}

/* Read a record from file IFP describing an arc in the function
   call-graph and the count of how many times the arc has been
   traversed.  FILENAME is the name of file IFP and is provided
   for formatting error-messages only.  */

void
cg_read_rec (FILE *ifp, const char *filename)
{
  bfd_vma from_pc, self_pc;
  unsigned int count;

  if (gmon_io_read_vma (ifp, &from_pc)
      || gmon_io_read_vma (ifp, &self_pc)
      || gmon_io_read_32 (ifp, &count))
    {
      fprintf (stderr, _("%s: %s: unexpected end of file\n"),
	       whoami, filename);
      done (1);
    }

  DBG (SAMPLEDEBUG,
       printf ("[cg_read_rec] frompc 0x%lx selfpc 0x%lx count %lu\n",
	       (unsigned long) from_pc, (unsigned long) self_pc,
	       (unsigned long) count));
  /* Add this arc:  */
  cg_tally (from_pc, self_pc, count);
}

/* Write all the arcs in the call-graph to file OFP.  FILENAME is
   the name of OFP and is provided for formatting error-messages
   only.  */

void
cg_write_arcs (FILE *ofp, const char *filename)
{
  Arc *arc;
  Sym *sym;

  for (sym = symtab.base; sym < symtab.limit; sym++)
    {
      for (arc = sym->cg.children; arc; arc = arc->next_child)
	{
	  if (gmon_io_write_8 (ofp, GMON_TAG_CG_ARC)
	      || gmon_io_write_vma (ofp, arc->parent->addr)
	      || gmon_io_write_vma (ofp, arc->child->addr)
	      || gmon_io_write_32 (ofp, arc->count))
	    {
	      perror (filename);
	      done (1);
	    }
	  DBG (SAMPLEDEBUG,
	     printf ("[cg_write_arcs] frompc 0x%lx selfpc 0x%lx count %lu\n",
		     (unsigned long) arc->parent->addr,
		     (unsigned long) arc->child->addr, arc->count));
	}
    }
}
