/* cg_print.c -  Print routines for displaying call graphs.

   Copyright 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

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

#include "libiberty.h"
#include "gprof.h"
#include "search_list.h"
#include "source.h"
#include "symtab.h"
#include "cg_arcs.h"
#include "cg_print.h"
#include "hist.h"
#include "utils.h"
#include "corefile.h"

/* Return value of comparison functions used to sort tables.  */
#define	LESSTHAN	-1
#define	EQUALTO		0
#define	GREATERTHAN	1

static void print_header (void);
static void print_cycle (Sym *);
static int cmp_member (Sym *, Sym *);
static void sort_members (Sym *);
static void print_members (Sym *);
static int cmp_arc (Arc *, Arc *);
static void sort_parents (Sym *);
static void print_parents (Sym *);
static void sort_children (Sym *);
static void print_children (Sym *);
static void print_line (Sym *);
static int cmp_name (const PTR, const PTR);
static int cmp_arc_count (const PTR, const PTR);
static int cmp_fun_nuses (const PTR, const PTR);
static void order_and_dump_functions_by_arcs
  (Arc **, unsigned long, int, Arc **, unsigned long *);

/* Declarations of automatically generated functions to output blurbs.  */
extern void bsd_callg_blurb (FILE * fp);
extern void fsf_callg_blurb (FILE * fp);

double print_time = 0.0;


static void
print_header ()
{
  if (first_output)
    first_output = FALSE;
  else
    printf ("\f\n");

  if (!bsd_style_output)
    {
      if (print_descriptions)
	printf (_("\t\t     Call graph (explanation follows)\n\n"));
      else
	printf (_("\t\t\tCall graph\n\n"));
    }

  printf (_("\ngranularity: each sample hit covers %ld byte(s)"),
	  (long) hist_scale * sizeof (UNIT));

  if (print_time > 0.0)
    printf (_(" for %.2f%% of %.2f seconds\n\n"),
	    100.0 / print_time, print_time / hz);
  else
    {
      printf (_(" no time propagated\n\n"));

      /* This doesn't hurt, since all the numerators will be 0.0.  */
      print_time = 1.0;
    }

  if (bsd_style_output)
    {
      printf ("%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n",
	      "", "", "", "", _("called"), _("total"), _("parents"));
      printf ("%-6.6s %5.5s %7.7s %11.11s %7.7s+%-7.7s %-8.8s\t%5.5s\n",
	      _("index"), _("%time"), _("self"), _("descendants"),
	      _("called"), _("self"), _("name"), _("index"));
      printf ("%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n",
	      "", "", "", "", _("called"), _("total"), _("children"));
      printf ("\n");
    }
  else
    {
      printf (_("index %% time    self  children    called     name\n"));
    }
}

/* Print a cycle header.  */

static void
print_cycle (Sym *cyc)
{
  char buf[BUFSIZ];

  sprintf (buf, "[%d]", cyc->cg.index);
  printf (bsd_style_output
	  ? "%-6.6s %5.1f %7.2f %11.2f %7lu"
	  : "%-6.6s %5.1f %7.2f %7.2f %7lu", buf,
	  100 * (cyc->cg.prop.self + cyc->cg.prop.child) / print_time,
	  cyc->cg.prop.self / hz, cyc->cg.prop.child / hz, cyc->ncalls);

  if (cyc->cg.self_calls != 0)
    printf ("+%-7lu", cyc->cg.self_calls);
  else
    printf (" %7.7s", "");

  printf (_(" <cycle %d as a whole> [%d]\n"), cyc->cg.cyc.num, cyc->cg.index);
}

/* Compare LEFT and RIGHT membmer.  Major comparison key is
   CG.PROP.SELF+CG.PROP.CHILD, secondary key is NCALLS+CG.SELF_CALLS.  */

static int
cmp_member (Sym *left, Sym *right)
{
  double left_time = left->cg.prop.self + left->cg.prop.child;
  double right_time = right->cg.prop.self + right->cg.prop.child;
  unsigned long left_calls = left->ncalls + left->cg.self_calls;
  unsigned long right_calls = right->ncalls + right->cg.self_calls;

  if (left_time > right_time)
    return GREATERTHAN;

  if (left_time < right_time)
    return LESSTHAN;

  if (left_calls > right_calls)
    return GREATERTHAN;

  if (left_calls < right_calls)
    return LESSTHAN;

  return EQUALTO;
}

/* Sort members of a cycle.  */

static void
sort_members (Sym *cyc)
{
  Sym *todo, *doing, *prev;

  /* Detach cycle members from cyclehead,
     and insertion sort them back on.  */
  todo = cyc->cg.cyc.next;
  cyc->cg.cyc.next = 0;

  for (doing = todo; doing && doing->cg.cyc.next; doing = todo)
    {
      todo = doing->cg.cyc.next;

      for (prev = cyc; prev->cg.cyc.next; prev = prev->cg.cyc.next)
	{
	  if (cmp_member (doing, prev->cg.cyc.next) == GREATERTHAN)
	    break;
	}

      doing->cg.cyc.next = prev->cg.cyc.next;
      prev->cg.cyc.next = doing;
    }
}

/* Print the members of a cycle.  */

static void
print_members (Sym *cyc)
{
  Sym *member;

  sort_members (cyc);

  for (member = cyc->cg.cyc.next; member; member = member->cg.cyc.next)
    {
      printf (bsd_style_output
	      ? "%6.6s %5.5s %7.2f %11.2f %7lu"
	      : "%6.6s %5.5s %7.2f %7.2f %7lu",
	      "", "", member->cg.prop.self / hz, member->cg.prop.child / hz,
	      member->ncalls);

      if (member->cg.self_calls != 0)
	printf ("+%-7lu", member->cg.self_calls);
      else
	printf (" %7.7s", "");

      printf ("     ");
      print_name (member);
      printf ("\n");
    }
}

/* Compare two arcs to/from the same child/parent.
	- if one arc is a self arc, it's least.
	- if one arc is within a cycle, it's less than.
	- if both arcs are within a cycle, compare arc counts.
	- if neither arc is within a cycle, compare with
		time + child_time as major key
		arc count as minor key.  */

static int
cmp_arc (Arc *left, Arc *right)
{
  Sym *left_parent = left->parent;
  Sym *left_child = left->child;
  Sym *right_parent = right->parent;
  Sym *right_child = right->child;
  double left_time, right_time;

  DBG (TIMEDEBUG,
       printf ("[cmp_arc] ");
       print_name (left_parent);
       printf (" calls ");
       print_name (left_child);
       printf (" %f + %f %lu/%lu\n", left->time, left->child_time,
	       left->count, left_child->ncalls);
       printf ("[cmp_arc] ");
       print_name (right_parent);
       printf (" calls ");
       print_name (right_child);
       printf (" %f + %f %lu/%lu\n", right->time, right->child_time,
	       right->count, right_child->ncalls);
       printf ("\n");
    );

  if (left_parent == left_child)
    return LESSTHAN;		/* Left is a self call.  */

  if (right_parent == right_child)
    return GREATERTHAN;		/* Right is a self call.  */

  if (left_parent->cg.cyc.num != 0 && left_child->cg.cyc.num != 0
      && left_parent->cg.cyc.num == left_child->cg.cyc.num)
    {
      /* Left is a call within a cycle.  */
      if (right_parent->cg.cyc.num != 0 && right_child->cg.cyc.num != 0
	  && right_parent->cg.cyc.num == right_child->cg.cyc.num)
	{
	  /* Right is a call within the cycle, too.  */
	  if (left->count < right->count)
	    return LESSTHAN;

	  if (left->count > right->count)
	    return GREATERTHAN;

	  return EQUALTO;
	}
      else
	{
	  /* Right isn't a call within the cycle.  */
	  return LESSTHAN;
	}
    }
  else
    {
      /* Left isn't a call within a cycle.  */
      if (right_parent->cg.cyc.num != 0 && right_child->cg.cyc.num != 0
	  && right_parent->cg.cyc.num == right_child->cg.cyc.num)
	{
	  /* Right is a call within a cycle.  */
	  return GREATERTHAN;
	}
      else
	{
	  /* Neither is a call within a cycle.  */
	  left_time = left->time + left->child_time;
	  right_time = right->time + right->child_time;

	  if (left_time < right_time)
	    return LESSTHAN;

	  if (left_time > right_time)
	    return GREATERTHAN;

	  if (left->count < right->count)
	    return LESSTHAN;

	  if (left->count > right->count)
	    return GREATERTHAN;

	  return EQUALTO;
	}
    }
}


static void
sort_parents (Sym * child)
{
  Arc *arc, *detached, sorted, *prev;

  /* Unlink parents from child, then insertion sort back on to
     sorted's parents.
	  *arc        the arc you have detached and are inserting.
	  *detached   the rest of the arcs to be sorted.
	  sorted      arc list onto which you insertion sort.
	  *prev       arc before the arc you are comparing.  */
  sorted.next_parent = 0;

  for (arc = child->cg.parents; arc; arc = detached)
    {
      detached = arc->next_parent;

      /* Consider *arc as disconnected; insert it into sorted.  */
      for (prev = &sorted; prev->next_parent; prev = prev->next_parent)
	{
	  if (cmp_arc (arc, prev->next_parent) != GREATERTHAN)
	    break;
	}

      arc->next_parent = prev->next_parent;
      prev->next_parent = arc;
    }

  /* Reattach sorted arcs to child.  */
  child->cg.parents = sorted.next_parent;
}


static void
print_parents (Sym *child)
{
  Sym *parent;
  Arc *arc;
  Sym *cycle_head;

  if (child->cg.cyc.head != 0)
    cycle_head = child->cg.cyc.head;
  else
    cycle_head = child;

  if (!child->cg.parents)
    {
      printf (bsd_style_output
	      ? _("%6.6s %5.5s %7.7s %11.11s %7.7s %7.7s     <spontaneous>\n")
	      : _("%6.6s %5.5s %7.7s %7.7s %7.7s %7.7s     <spontaneous>\n"),
	      "", "", "", "", "", "");
      return;
    }

  sort_parents (child);

  for (arc = child->cg.parents; arc; arc = arc->next_parent)
    {
      parent = arc->parent;
      if (child == parent || (child->cg.cyc.num != 0
			      && parent->cg.cyc.num == child->cg.cyc.num))
	{
	  /* Selfcall or call among siblings.  */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.7s %11.11s %7lu %7.7s     "
		  : "%6.6s %5.5s %7.7s %7.7s %7lu %7.7s     ",
		  "", "", "", "",
		  arc->count, "");
	  print_name (parent);
	  printf ("\n");
	}
      else
	{
	  /* Regular parent of child.  */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.2f %11.2f %7lu/%-7lu     "
		  : "%6.6s %5.5s %7.2f %7.2f %7lu/%-7lu     ",
		  "", "",
		  arc->time / hz, arc->child_time / hz,
		  arc->count, cycle_head->ncalls);
	  print_name (parent);
	  printf ("\n");
	}
    }
}


static void
sort_children (Sym *parent)
{
  Arc *arc, *detached, sorted, *prev;

  /* Unlink children from parent, then insertion sort back on to
     sorted's children.
	  *arc        the arc you have detached and are inserting.
	  *detached   the rest of the arcs to be sorted.
	  sorted      arc list onto which you insertion sort.
	  *prev       arc before the arc you are comparing.  */
  sorted.next_child = 0;

  for (arc = parent->cg.children; arc; arc = detached)
    {
      detached = arc->next_child;

      /* Consider *arc as disconnected; insert it into sorted.  */
      for (prev = &sorted; prev->next_child; prev = prev->next_child)
	{
	  if (cmp_arc (arc, prev->next_child) != LESSTHAN)
	    break;
	}

      arc->next_child = prev->next_child;
      prev->next_child = arc;
    }

  /* Reattach sorted children to parent.  */
  parent->cg.children = sorted.next_child;
}


static void
print_children (Sym *parent)
{
  Sym *child;
  Arc *arc;

  sort_children (parent);
  arc = parent->cg.children;

  for (arc = parent->cg.children; arc; arc = arc->next_child)
    {
      child = arc->child;
      if (child == parent || (child->cg.cyc.num != 0
			      && child->cg.cyc.num == parent->cg.cyc.num))
	{
	  /* Self call or call to sibling.  */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.7s %11.11s %7lu %7.7s     "
		  : "%6.6s %5.5s %7.7s %7.7s %7lu %7.7s     ",
		  "", "", "", "", arc->count, "");
	  print_name (child);
	  printf ("\n");
	}
      else
	{
	  /* Regular child of parent.  */
	  printf (bsd_style_output
		  ? "%6.6s %5.5s %7.2f %11.2f %7lu/%-7lu     "
		  : "%6.6s %5.5s %7.2f %7.2f %7lu/%-7lu     ",
		  "", "",
		  arc->time / hz, arc->child_time / hz,
		  arc->count, child->cg.cyc.head->ncalls);
	  print_name (child);
	  printf ("\n");
	}
    }
}


static void
print_line (Sym *np)
{
  char buf[BUFSIZ];

  sprintf (buf, "[%d]", np->cg.index);
  printf (bsd_style_output
	  ? "%-6.6s %5.1f %7.2f %11.2f"
	  : "%-6.6s %5.1f %7.2f %7.2f", buf,
	  100 * (np->cg.prop.self + np->cg.prop.child) / print_time,
	  np->cg.prop.self / hz, np->cg.prop.child / hz);

  if ((np->ncalls + np->cg.self_calls) != 0)
    {
      printf (" %7lu", np->ncalls);

      if (np->cg.self_calls != 0)
	  printf ("+%-7lu ", np->cg.self_calls);
      else
	  printf (" %7.7s ", "");
    }
  else
    {
      printf (" %7.7s %7.7s ", "", "");
    }

  print_name (np);
  printf ("\n");
}


/* Print dynamic call graph.  */

void
cg_print (Sym ** timesortsym)
{
  unsigned int index;
  Sym *parent;

  if (print_descriptions && bsd_style_output)
    bsd_callg_blurb (stdout);

  print_header ();

  for (index = 0; index < symtab.len + num_cycles; ++index)
    {
      parent = timesortsym[index];

      if ((ignore_zeros && parent->ncalls == 0
	   && parent->cg.self_calls == 0 && parent->cg.prop.self == 0
	   && parent->cg.prop.child == 0)
	  || !parent->cg.print_flag
	  || (line_granularity && ! parent->is_func))
	continue;

      if (!parent->name && parent->cg.cyc.num != 0)
	{
	  /* Cycle header.  */
	  print_cycle (parent);
	  print_members (parent);
	}
      else
	{
	  print_parents (parent);
	  print_line (parent);
	  print_children (parent);
	}

      if (bsd_style_output)
	printf ("\n");

      printf ("-----------------------------------------------\n");

      if (bsd_style_output)
	printf ("\n");
    }

  free (timesortsym);

  if (print_descriptions && !bsd_style_output)
    fsf_callg_blurb (stdout);
}


static int
cmp_name (const PTR left, const PTR right)
{
  const Sym **npp1 = (const Sym **) left;
  const Sym **npp2 = (const Sym **) right;

  return strcmp ((*npp1)->name, (*npp2)->name);
}


void
cg_print_index ()
{
  unsigned int index;
  unsigned int nnames, todo, i, j;
  int col, starting_col;
  Sym **name_sorted_syms, *sym;
  const char *filename;
  char buf[20];
  int column_width = (output_width - 1) / 3;	/* Don't write in last col!  */

  /* Now, sort regular function name
     alphabetically to create an index.  */
  name_sorted_syms = (Sym **) xmalloc ((symtab.len + num_cycles) * sizeof (Sym *));

  for (index = 0, nnames = 0; index < symtab.len; index++)
    {
      if (ignore_zeros && symtab.base[index].ncalls == 0
	  && symtab.base[index].hist.time == 0)
	continue;

      name_sorted_syms[nnames++] = &symtab.base[index];
    }

  qsort (name_sorted_syms, nnames, sizeof (Sym *), cmp_name);

  for (index = 1, todo = nnames; index <= num_cycles; index++)
    name_sorted_syms[todo++] = &cycle_header[index];

  printf ("\f\n");
  printf (_("Index by function name\n\n"));
  index = (todo + 2) / 3;

  for (i = 0; i < index; i++)
    {
      col = 0;
      starting_col = 0;

      for (j = i; j < todo; j += index)
	{
	  sym = name_sorted_syms[j];

	  if (sym->cg.print_flag)
	    sprintf (buf, "[%d]", sym->cg.index);
	  else
	    sprintf (buf, "(%d)", sym->cg.index);

	  if (j < nnames)
	    {
	      if (bsd_style_output)
		{
		  printf ("%6.6s %-19.19s", buf, sym->name);
		}
	      else
		{
		  col += strlen (buf);

		  for (; col < starting_col + 5; ++col)
		    putchar (' ');

		  printf (" %s ", buf);
		  col += print_name_only (sym);

		  if (!line_granularity && sym->is_static && sym->file)
		    {
		      filename = sym->file->name;

		      if (!print_path)
			{
			  filename = strrchr (filename, '/');

			  if (filename)
			    ++filename;
			  else
			    filename = sym->file->name;
			}

		      printf (" (%s)", filename);
		      col += strlen (filename) + 3;
		    }
		}
	    }
	  else
	    {
	      if (bsd_style_output)
		{
		  printf ("%6.6s ", buf);
		  sprintf (buf, _("<cycle %d>"), sym->cg.cyc.num);
		  printf ("%-19.19s", buf);
		}
	      else
		{
		  col += strlen (buf);
		  for (; col < starting_col + 5; ++col)
		    putchar (' ');
		  printf (" %s ", buf);
		  sprintf (buf, _("<cycle %d>"), sym->cg.cyc.num);
		  printf ("%s", buf);
		  col += strlen (buf);
		}
	    }

	  starting_col += column_width;
	}

      printf ("\n");
    }

  free (name_sorted_syms);
}

/* Compare two arcs based on their usage counts.
   We want to sort in descending order.  */

static int
cmp_arc_count (const PTR left, const PTR right)
{
  const Arc **npp1 = (const Arc **) left;
  const Arc **npp2 = (const Arc **) right;

  if ((*npp1)->count > (*npp2)->count)
    return -1;
  else if ((*npp1)->count < (*npp2)->count)
    return 1;
  else
    return 0;
}

/* Compare two funtions based on their usage counts.
   We want to sort in descending order.  */

static int
cmp_fun_nuses (const PTR left, const PTR right)
{
  const Sym **npp1 = (const Sym **) left;
  const Sym **npp2 = (const Sym **) right;

  if ((*npp1)->nuses > (*npp2)->nuses)
    return -1;
  else if ((*npp1)->nuses < (*npp2)->nuses)
    return 1;
  else
    return 0;
}

/* Print a suggested function ordering based on the profiling data.

   We perform 4 major steps when ordering functions:

	* Group unused functions together and place them at the
	end of the function order.

	* Search the highest use arcs (those which account for 90% of
	the total arc count) for functions which have several parents.

	Group those with the most call sites together (currently the
	top 1.25% which have at least five different call sites).

	These are emitted at the start of the function order.

	* Use a greedy placement algorithm to place functions which
	occur in the top 99% of the arcs in the profile.  Some provisions
	are made to handle high usage arcs where the parent and/or
	child has already been placed.

	* Run the same greedy placement algorithm on the remaining
	arcs to place the leftover functions.


   The various "magic numbers" should (one day) be tuneable by command
   line options.  They were arrived at by benchmarking a few applications
   with various values to see which values produced better overall function
   orderings.

   Of course, profiling errors, machine limitations (PA long calls), and
   poor cutoff values for the placement algorithm may limit the usefullness
   of the resulting function order.  Improvements would be greatly appreciated.

   Suggestions:

	* Place the functions with many callers near the middle of the
	list to reduce long calls.

	* Propagate arc usage changes as functions are placed.  Ie if
	func1 and func2 are placed together, arcs to/from those arcs
	to the same parent/child should be combined, then resort the
	arcs to choose the next one.

	* Implement some global positioning algorithm to place the
	chains made by the greedy local positioning algorithm.  Probably
	by examining arcs which haven't been placed yet to tie two
	chains together.

	* Take a function's size and time into account in the algorithm;
	size in particular is important on the PA (long calls).  Placing
	many small functions onto their own page may be wise.

	* Use better profiling information; many published algorithms
	are based on call sequences through time, rather than just
	arc counts.

	* Prodecure cloning could improve performance when a small number
	of arcs account for most of the calls to a particular function.

	* Use relocation information to avoid moving unused functions
	completely out of the code stream; this would avoid severe lossage
	when the profile data bears little resemblance to actual runs.

	* Propagation of arc usages should also improve .o link line
	ordering which shares the same arc placement algorithm with
	the function ordering code (in fact it is a degenerate case
	of function ordering).  */

void
cg_print_function_ordering ()
{
  unsigned long index, used, unused, scratch_index;
  unsigned long  unplaced_arc_count, high_arc_count, scratch_arc_count;
#ifdef __GNUC__
  unsigned long long total_arcs, tmp_arcs_count;
#else
  unsigned long total_arcs, tmp_arcs_count;
#endif
  Sym **unused_syms, **used_syms, **scratch_syms;
  Arc **unplaced_arcs, **high_arcs, **scratch_arcs;

  index = 0;
  used = 0;
  unused = 0;
  scratch_index = 0;
  unplaced_arc_count = 0;
  high_arc_count = 0;
  scratch_arc_count = 0;

  /* First group all the unused functions together.  */
  unused_syms = (Sym **) xmalloc (symtab.len * sizeof (Sym *));
  used_syms = (Sym **) xmalloc (symtab.len * sizeof (Sym *));
  scratch_syms = (Sym **) xmalloc (symtab.len * sizeof (Sym *));
  high_arcs = (Arc **) xmalloc (numarcs * sizeof (Arc *));
  scratch_arcs = (Arc **) xmalloc (numarcs * sizeof (Arc *));
  unplaced_arcs = (Arc **) xmalloc (numarcs * sizeof (Arc *));

  /* Walk through all the functions; mark those which are never
     called as placed (we'll emit them as a group later).  */
  for (index = 0, used = 0, unused = 0; index < symtab.len; index++)
    {
      if (symtab.base[index].ncalls == 0)
	{
	  /* Filter out gprof generated names.  */
	  if (strcmp (symtab.base[index].name, "<locore>")
	      && strcmp (symtab.base[index].name, "<hicore>"))
	    {
	      unused_syms[unused++] = &symtab.base[index];
	      symtab.base[index].has_been_placed = 1;
	    }
	}
      else
	{
	  used_syms[used++] = &symtab.base[index];
	  symtab.base[index].has_been_placed = 0;
	  symtab.base[index].next = 0;
	  symtab.base[index].prev = 0;
	  symtab.base[index].nuses = 0;
	}
    }

  /* Sort the arcs from most used to least used.  */
  qsort (arcs, numarcs, sizeof (Arc *), cmp_arc_count);

  /* Compute the total arc count.  Also mark arcs as unplaced.

     Note we don't compensate for overflow if that happens!
     Overflow is much less likely when this file is compiled
     with GCC as it can double-wide integers via long long.  */
  total_arcs = 0;
  for (index = 0; index < numarcs; index++)
    {
      total_arcs += arcs[index]->count;
      arcs[index]->has_been_placed = 0;
    }

  /* We want to pull out those functions which are referenced
     by many highly used arcs and emit them as a group.  This
     could probably use some tuning.  */
  tmp_arcs_count = 0;
  for (index = 0; index < numarcs; index++)
    {
      tmp_arcs_count += arcs[index]->count;

      /* Count how many times each parent and child are used up
	 to our threshhold of arcs (90%).  */
      if ((double)tmp_arcs_count / (double)total_arcs > 0.90)
	break;

      arcs[index]->child->nuses++;
    }

  /* Now sort a temporary symbol table based on the number of
     times each function was used in the highest used arcs.  */
  memcpy (scratch_syms, used_syms, used * sizeof (Sym *));
  qsort (scratch_syms, used, sizeof (Sym *), cmp_fun_nuses);

  /* Now pick out those symbols we're going to emit as
     a group.  We take up to 1.25% of the used symbols.  */
  for (index = 0; index < used / 80; index++)
    {
      Sym *sym = scratch_syms[index];
      Arc *arc;

      /* If we hit symbols that aren't used from many call sites,
	 then we can quit.  We choose five as the low limit for
	 no particular reason.  */
      if (sym->nuses == 5)
	break;

      /* We're going to need the arcs between these functions.
	 Unfortunately, we don't know all these functions
	 until we're done.  So we keep track of all the arcs
	 to the functions we care about, then prune out those
	 which are uninteresting.

	 An interesting variation would be to quit when we found
	 multi-call site functions which account for some percentage
	 of the arcs.  */
      arc = sym->cg.children;

      while (arc)
	{
	  if (arc->parent != arc->child)
	    scratch_arcs[scratch_arc_count++] = arc;
	  arc->has_been_placed = 1;
	  arc = arc->next_child;
	}

      arc = sym->cg.parents;

      while (arc)
	{
	  if (arc->parent != arc->child)
	    scratch_arcs[scratch_arc_count++] = arc;
	  arc->has_been_placed = 1;
	  arc = arc->next_parent;
	}

      /* Keep track of how many symbols we're going to place.  */
      scratch_index = index;

      /* A lie, but it makes identifying
	 these functions easier later.  */
      sym->has_been_placed = 1;
    }

  /* Now walk through the temporary arcs and copy
     those we care about into the high arcs array.  */
  for (index = 0; index < scratch_arc_count; index++)
    {
      Arc *arc = scratch_arcs[index];

      /* If this arc refers to highly used functions, then
	 then we want to keep it.  */
      if (arc->child->has_been_placed
	  && arc->parent->has_been_placed)
	{
	  high_arcs[high_arc_count++] = scratch_arcs[index];

	  /* We need to turn of has_been_placed since we're going to
	     use the main arc placement algorithm on these arcs.  */
	  arc->child->has_been_placed = 0;
	  arc->parent->has_been_placed = 0;
	}
    }

  /* Dump the multi-site high usage functions which are not
     going to be ordered by the main ordering algorithm.  */
  for (index = 0; index < scratch_index; index++)
    {
      if (scratch_syms[index]->has_been_placed)
	printf ("%s\n", scratch_syms[index]->name);
    }

  /* Now we can order the multi-site high use
     functions based on the arcs between them.  */
  qsort (high_arcs, high_arc_count, sizeof (Arc *), cmp_arc_count);
  order_and_dump_functions_by_arcs (high_arcs, high_arc_count, 1,
				    unplaced_arcs, &unplaced_arc_count);

  /* Order and dump the high use functions left,
     these typically have only a few call sites.  */
  order_and_dump_functions_by_arcs (arcs, numarcs, 0,
				    unplaced_arcs, &unplaced_arc_count);

  /* Now place the rarely used functions.  */
  order_and_dump_functions_by_arcs (unplaced_arcs, unplaced_arc_count, 1,
				    scratch_arcs, &scratch_arc_count);

  /* Output any functions not emitted by the order_and_dump calls.  */
  for (index = 0; index < used; index++)
    if (used_syms[index]->has_been_placed == 0)
      printf("%s\n", used_syms[index]->name);

  /* Output the unused functions.  */
  for (index = 0; index < unused; index++)
    printf("%s\n", unused_syms[index]->name);

  unused_syms = (Sym **) xmalloc (symtab.len * sizeof (Sym *));
  used_syms = (Sym **) xmalloc (symtab.len * sizeof (Sym *));
  scratch_syms = (Sym **) xmalloc (symtab.len * sizeof (Sym *));
  high_arcs = (Arc **) xmalloc (numarcs * sizeof (Arc *));
  scratch_arcs = (Arc **) xmalloc (numarcs * sizeof (Arc *));
  unplaced_arcs = (Arc **) xmalloc (numarcs * sizeof (Arc *));

  free (unused_syms);
  free (used_syms);
  free (scratch_syms);
  free (high_arcs);
  free (scratch_arcs);
  free (unplaced_arcs);
}

/* Place functions based on the arcs in THE_ARCS with ARC_COUNT entries;
   place unused arcs into UNPLACED_ARCS/UNPLACED_ARC_COUNT.

   If ALL is nonzero, then place all functions referenced by THE_ARCS,
   else only place those referenced in the top 99% of the arcs in THE_ARCS.  */

#define MOST 0.99
static void
order_and_dump_functions_by_arcs (the_arcs, arc_count, all,
				  unplaced_arcs, unplaced_arc_count)
     Arc **the_arcs;
     unsigned long arc_count;
     int all;
     Arc **unplaced_arcs;
     unsigned long *unplaced_arc_count;
{
#ifdef __GNUC__
  unsigned long long tmp_arcs, total_arcs;
#else
  unsigned long tmp_arcs, total_arcs;
#endif
  unsigned int index;

  /* If needed, compute the total arc count.

     Note we don't compensate for overflow if that happens!  */
  if (! all)
    {
      total_arcs = 0;
      for (index = 0; index < arc_count; index++)
	total_arcs += the_arcs[index]->count;
    }
  else
    total_arcs = 0;

  tmp_arcs = 0;

  for (index = 0; index < arc_count; index++)
    {
      Sym *sym1, *sym2;
      Sym *child, *parent;

      tmp_arcs += the_arcs[index]->count;

      /* Ignore this arc if it's already been placed.  */
      if (the_arcs[index]->has_been_placed)
	continue;

      child = the_arcs[index]->child;
      parent = the_arcs[index]->parent;

      /* If we're not using all arcs, and this is a rarely used
	 arc, then put it on the unplaced_arc list.  Similarly
	 if both the parent and child of this arc have been placed.  */
      if ((! all && (double)tmp_arcs / (double)total_arcs > MOST)
	  || child->has_been_placed || parent->has_been_placed)
	{
	  unplaced_arcs[(*unplaced_arc_count)++] = the_arcs[index];
	  continue;
	}

      /* If all slots in the parent and child are full, then there isn't
	 anything we can do right now.  We'll place this arc on the
	 unplaced arc list in the hope that a global positioning
	 algorithm can use it to place function chains.  */
      if (parent->next && parent->prev && child->next && child->prev)
	{
	  unplaced_arcs[(*unplaced_arc_count)++] = the_arcs[index];
	  continue;
	}

      /* If the parent is unattached, then find the closest
	 place to attach it onto child's chain.   Similarly
	 for the opposite case.  */
      if (!parent->next && !parent->prev)
	{
	  int next_count = 0;
	  int prev_count = 0;
	  Sym *prev = child;
	  Sym *next = child;

	  /* Walk to the beginning and end of the child's chain.  */
	  while (next->next)
	    {
	      next = next->next;
	      next_count++;
	    }

	  while (prev->prev)
	    {
	      prev = prev->prev;
	      prev_count++;
	    }

	  /* Choose the closest.  */
	  child = next_count < prev_count ? next : prev;
	}
      else if (! child->next && !child->prev)
	{
	  int next_count = 0;
	  int prev_count = 0;
	  Sym *prev = parent;
	  Sym *next = parent;

	  while (next->next)
	    {
	      next = next->next;
	      next_count++;
	    }

	  while (prev->prev)
	    {
	      prev = prev->prev;
	      prev_count++;
	    }

	  parent = prev_count < next_count ? prev : next;
	}
      else
	{
	  /* Couldn't find anywhere to attach the functions,
	     put the arc on the unplaced arc list.  */
	  unplaced_arcs[(*unplaced_arc_count)++] = the_arcs[index];
	  continue;
	}

      /* Make sure we don't tie two ends together.  */
      sym1 = parent;
      if (sym1->next)
	while (sym1->next)
	  sym1 = sym1->next;
      else
	while (sym1->prev)
	  sym1 = sym1->prev;

      sym2 = child;
      if (sym2->next)
	while (sym2->next)
	  sym2 = sym2->next;
      else
	while (sym2->prev)
	  sym2 = sym2->prev;

      if (sym1 == child
	  && sym2 == parent)
	{
	  /* This would tie two ends together.  */
	  unplaced_arcs[(*unplaced_arc_count)++] = the_arcs[index];
	  continue;
	}

      if (parent->next)
	{
	  /* Must attach to the parent's prev field.  */
	  if (! child->next)
	    {
	      /* parent-prev and child-next */
	      parent->prev = child;
	      child->next = parent;
	      the_arcs[index]->has_been_placed = 1;
	    }
	}
      else if (parent->prev)
	{
	  /* Must attach to the parent's next field.  */
	  if (! child->prev)
	    {
	      /* parent-next and child-prev */
	      parent->next = child;
	      child->prev = parent;
	      the_arcs[index]->has_been_placed = 1;
	    }
	}
      else
	{
	  /* Can attach to either field in the parent, depends
	     on where we've got space in the child.  */
	  if (child->prev)
	    {
	      /* parent-prev and child-next.  */
	      parent->prev = child;
	      child->next = parent;
	      the_arcs[index]->has_been_placed = 1;
	    }
	  else
	    {
	      /* parent-next and child-prev.  */
	      parent->next = child;
	      child->prev = parent;
	      the_arcs[index]->has_been_placed = 1;
	    }
	}
    }

  /* Dump the chains of functions we've made.  */
  for (index = 0; index < arc_count; index++)
    {
      Sym *sym;
      if (the_arcs[index]->parent->has_been_placed
	  || the_arcs[index]->child->has_been_placed)
	continue;

      sym = the_arcs[index]->parent;

      /* If this symbol isn't attached to any other
	 symbols, then we've got a rarely used arc.

	 Skip it for now, we'll deal with them later.  */
      if (sym->next == NULL
	  && sym->prev == NULL)
	continue;

      /* Get to the start of this chain.  */
      while (sym->prev)
	sym = sym->prev;

      while (sym)
	{
	  /* Mark it as placed.  */
	  sym->has_been_placed = 1;
	  printf ("%s\n", sym->name);
	  sym = sym->next;
	}
    }

  /* If we want to place all the arcs, then output
     those which weren't placed by the main algorithm.  */
  if (all)
    for (index = 0; index < arc_count; index++)
      {
	Sym *sym;
	if (the_arcs[index]->parent->has_been_placed
	    || the_arcs[index]->child->has_been_placed)
	  continue;

	sym = the_arcs[index]->parent;

	sym->has_been_placed = 1;
	printf ("%s\n", sym->name);
      }
}

/* Print a suggested .o ordering for files on a link line based
   on profiling information.  This uses the function placement
   code for the bulk of its work.  */

void
cg_print_file_ordering ()
{
  unsigned long scratch_arc_count, index;
  Arc **scratch_arcs;
  char *last;

  scratch_arc_count = 0;

  scratch_arcs = (Arc **) xmalloc (numarcs * sizeof (Arc *));
  for (index = 0; index < numarcs; index++)
    {
      if (! arcs[index]->parent->mapped
	  || ! arcs[index]->child->mapped)
	arcs[index]->has_been_placed = 1;
    }

  order_and_dump_functions_by_arcs (arcs, numarcs, 0,
				    scratch_arcs, &scratch_arc_count);

  /* Output .o's not handled by the main placement algorithm.  */
  for (index = 0; index < symtab.len; index++)
    {
      if (symtab.base[index].mapped
	  && ! symtab.base[index].has_been_placed)
	printf ("%s\n", symtab.base[index].name);
    }

  /* Now output any .o's that didn't have any text symbols.  */
  last = NULL;
  for (index = 0; index < symbol_map_count; index++)
    {
      unsigned int index2;

      /* Don't bother searching if this symbol
	 is the same as the previous one.  */
      if (last && !strcmp (last, symbol_map[index].file_name))
	continue;

      for (index2 = 0; index2 < symtab.len; index2++)
	{
	  if (! symtab.base[index2].mapped)
	    continue;

	  if (!strcmp (symtab.base[index2].name, symbol_map[index].file_name))
	    break;
	}

      /* If we didn't find it in the symbol table, then it must
	 be a .o with no text symbols.  Output it last.  */
      if (index2 == symtab.len)
	printf ("%s\n", symbol_map[index].file_name);
      last = symbol_map[index].file_name;
    }
}
