/*
 * Copyright (c) 1983, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "libiberty.h"
#include "gprof.h"
#include "search_list.h"
#include "source.h"
#include "symtab.h"
#include "cg_arcs.h"
#include "cg_dfn.h"
#include "utils.h"

#define	DFN_INCR_DEPTH (128)

typedef struct
  {
    Sym *sym;
    int cycle_top;
  }
DFN_Stack;

static bfd_boolean is_numbered PARAMS ((Sym *));
static bfd_boolean is_busy PARAMS ((Sym *));
static void find_cycle PARAMS ((Sym *));
static void pre_visit PARAMS ((Sym *));
static void post_visit PARAMS ((Sym *));

DFN_Stack *dfn_stack = NULL;
int dfn_maxdepth = 0;
int dfn_depth = 0;
int dfn_counter = DFN_NAN;


/*
 * Is CHILD already numbered?
 */
static bfd_boolean
is_numbered (child)
     Sym *child;
{
  return child->cg.top_order != DFN_NAN && child->cg.top_order != DFN_BUSY;
}


/*
 * Is CHILD already busy?
 */
static bfd_boolean
is_busy (child)
     Sym *child;
{
  if (child->cg.top_order == DFN_NAN)
    {
      return FALSE;
    }
  return TRUE;
}


/*
 * CHILD is part of a cycle.  Find the top caller into this cycle
 * that is not part of the cycle and make all functions in cycle
 * members of that cycle (top caller == caller with smallest
 * depth-first number).
 */
static void
find_cycle (child)
     Sym *child;
{
  Sym *head = 0;
  Sym *tail;
  int cycle_top;
  int index;

  for (cycle_top = dfn_depth; cycle_top > 0; --cycle_top)
    {
      head = dfn_stack[cycle_top].sym;
      if (child == head)
	{
	  break;
	}
      if (child->cg.cyc.head != child && child->cg.cyc.head == head)
	{
	  break;
	}
    }
  if (cycle_top <= 0)
    {
      fprintf (stderr, "[find_cycle] couldn't find head of cycle\n");
      done (1);
    }
#ifdef DEBUG
  if (debug_level & DFNDEBUG)
    {
      printf ("[find_cycle] dfn_depth %d cycle_top %d ",
	      dfn_depth, cycle_top);
      if (head)
	{
	  print_name (head);
	}
      else
	{
	  printf ("<unknown>");
	}
      printf ("\n");
    }
#endif
  if (cycle_top == dfn_depth)
    {
      /*
       * This is previous function, e.g. this calls itself.  Sort of
       * boring.
       *
       * Since we are taking out self-cycles elsewhere no need for
       * the special case, here.
       */
      DBG (DFNDEBUG,
	   printf ("[find_cycle] ");
	   print_name (child);
	   printf ("\n"));
    }
  else
    {
      /*
       * Glom intervening functions that aren't already glommed into
       * this cycle.  Things have been glommed when their cyclehead
       * field points to the head of the cycle they are glommed
       * into.
       */
      for (tail = head; tail->cg.cyc.next; tail = tail->cg.cyc.next)
	{
	  /* void: chase down to tail of things already glommed */
	  DBG (DFNDEBUG,
	       printf ("[find_cycle] tail ");
	       print_name (tail);
	       printf ("\n"));
	}
      /*
       * If what we think is the top of the cycle has a cyclehead
       * field, then it's not really the head of the cycle, which is
       * really what we want.
       */
      if (head->cg.cyc.head != head)
	{
	  head = head->cg.cyc.head;
	  DBG (DFNDEBUG, printf ("[find_cycle] new cyclehead ");
	       print_name (head);
	       printf ("\n"));
	}
      for (index = cycle_top + 1; index <= dfn_depth; ++index)
	{
	  child = dfn_stack[index].sym;
	  if (child->cg.cyc.head == child)
	    {
	      /*
	       * Not yet glommed anywhere, glom it and fix any
	       * children it has glommed.
	       */
	      tail->cg.cyc.next = child;
	      child->cg.cyc.head = head;
	      DBG (DFNDEBUG, printf ("[find_cycle] glomming ");
		   print_name (child);
		   printf (" onto ");
		   print_name (head);
		   printf ("\n"));
	      for (tail = child; tail->cg.cyc.next; tail = tail->cg.cyc.next)
		{
		  tail->cg.cyc.next->cg.cyc.head = head;
		  DBG (DFNDEBUG, printf ("[find_cycle] and its tail ");
		       print_name (tail->cg.cyc.next);
		       printf (" onto ");
		       print_name (head);
		       printf ("\n"));
		}
	    }
	  else if (child->cg.cyc.head != head /* firewall */ )
	    {
	      fprintf (stderr, "[find_cycle] glommed, but not to head\n");
	      done (1);
	    }
	}
    }
}


/*
 * Prepare for visiting the children of PARENT.  Push a parent onto
 * the stack and mark it busy.
 */
static void
pre_visit (parent)
     Sym *parent;
{
  ++dfn_depth;

  if (dfn_depth >= dfn_maxdepth)
    {
      dfn_maxdepth += DFN_INCR_DEPTH;
      dfn_stack = xrealloc (dfn_stack, dfn_maxdepth * sizeof *dfn_stack);
    }

  dfn_stack[dfn_depth].sym = parent;
  dfn_stack[dfn_depth].cycle_top = dfn_depth;
  parent->cg.top_order = DFN_BUSY;
  DBG (DFNDEBUG, printf ("[pre_visit]\t\t%d:", dfn_depth);
       print_name (parent);
       printf ("\n"));
}


/*
 * Done with visiting node PARENT.  Pop PARENT off dfn_stack
 * and number functions if PARENT is head of a cycle.
 */
static void
post_visit (parent)
     Sym *parent;
{
  Sym *member;

  DBG (DFNDEBUG, printf ("[post_visit]\t%d: ", dfn_depth);
       print_name (parent);
       printf ("\n"));
  /*
   * Number functions and things in their cycles unless the function
   * is itself part of a cycle:
   */
  if (parent->cg.cyc.head == parent)
    {
      ++dfn_counter;
      for (member = parent; member; member = member->cg.cyc.next)
	{
	  member->cg.top_order = dfn_counter;
	  DBG (DFNDEBUG, printf ("[post_visit]\t\tmember ");
	       print_name (member);
	       printf ("-> cg.top_order = %d\n", dfn_counter));
	}
    }
  else
    {
      DBG (DFNDEBUG, printf ("[post_visit]\t\tis part of a cycle\n"));
    }
  --dfn_depth;
}


/*
 * Given this PARENT, depth first number its children.
 */
void
cg_dfn (parent)
     Sym *parent;
{
  Arc *arc;

  DBG (DFNDEBUG, printf ("[dfn] dfn( ");
       print_name (parent);
       printf (")\n"));
  /*
   * If we're already numbered, no need to look any further:
   */
  if (is_numbered (parent))
    {
      return;
    }
  /*
   * If we're already busy, must be a cycle:
   */
  if (is_busy (parent))
    {
      find_cycle (parent);
      return;
    }
  pre_visit (parent);
  /*
   * Recursively visit children:
   */
  for (arc = parent->cg.children; arc; arc = arc->next_child)
    {
      cg_dfn (arc->child);
    }
  post_visit (parent);
}
