/* Generate the nondeterministic finite state machine for bison,
   Copyright (C) 1984, 1986, 1989 Free Software Foundation, Inc.

This file is part of Bison, the GNU Compiler Compiler.

Bison is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Bison is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Bison; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */


/* See comments in state.h for the data structures that represent it.
   The entry point is generate_states.  */

#include <stdio.h>
#include "system.h"
#include "machine.h"
#include "alloc.h"
#include "gram.h"
#include "state.h"


extern char *nullable;
extern short *itemset;
extern short *itemsetend;


int nstates;
int final_state;
core *first_state;
shifts *first_shift;
reductions *first_reduction;

int get_state PARAMS((int));
core *new_state PARAMS((int));

void allocate_itemsets PARAMS((void));
void allocate_storage PARAMS((void));
void free_storage PARAMS((void));
void generate_states PARAMS((void));
void new_itemsets PARAMS((void));
void append_states PARAMS((void));
void initialize_states PARAMS((void));
void save_shifts PARAMS((void));
void save_reductions PARAMS((void));
void augment_automaton PARAMS((void));
void insert_start_shift PARAMS((void));
extern void initialize_closure PARAMS((int));
extern void closure PARAMS((short *, int));
extern void finalize_closure PARAMS((void));
extern void toomany PARAMS((char *));

static core *this_state;
static core *last_state;
static shifts *last_shift;
static reductions *last_reduction;

static int nshifts;
static short *shift_symbol;

static short *redset;
static short *shiftset;

static short **kernel_base;
static short **kernel_end;
static short *kernel_items;

/* hash table for states, to recognize equivalent ones.  */

#define	STATE_TABLE_SIZE	1009
static core **state_table;



void
allocate_itemsets (void)
{
  register short *itemp;
  register int symbol;
  register int i;
  register int count;
  register short *symbol_count;

  count = 0;
  symbol_count = NEW2(nsyms, short);

  itemp = ritem;
  symbol = *itemp++;
  while (symbol)
    {
      if (symbol > 0)
	{
	  count++;
	  symbol_count[symbol]++;
	}
      symbol = *itemp++;
    }

  /* see comments before new_itemsets.  All the vectors of items
     live inside kernel_items.  The number of active items after
     some symbol cannot be more than the number of times that symbol
     appears as an item, which is symbol_count[symbol].
     We allocate that much space for each symbol.  */

  kernel_base = NEW2(nsyms, short *);
  kernel_items = NEW2(count, short);

  count = 0;
  for (i = 0; i < nsyms; i++)
    {
      kernel_base[i] = kernel_items + count;
      count += symbol_count[i];
    }

  shift_symbol = symbol_count;
  kernel_end = NEW2(nsyms, short *);
}


void
allocate_storage (void)
{
  allocate_itemsets();

  shiftset = NEW2(nsyms, short);
  redset = NEW2(nrules + 1, short);
  state_table = NEW2(STATE_TABLE_SIZE, core *);
}


void
free_storage (void)
{
  FREE(shift_symbol);
  FREE(redset);
  FREE(shiftset);
  FREE(kernel_base);
  FREE(kernel_end);
  FREE(kernel_items);
  FREE(state_table);
}



/* compute the nondeterministic finite state machine (see state.h for details)
from the grammar.  */
void
generate_states (void)
{
  allocate_storage();
  initialize_closure(nitems);
  initialize_states();

  while (this_state)
    {
      /* Set up ruleset and itemset for the transitions out of this state.
         ruleset gets a 1 bit for each rule that could reduce now.
	 itemset gets a vector of all the items that could be accepted next.  */
      closure(this_state->items, this_state->nitems);
      /* record the reductions allowed out of this state */
      save_reductions();
      /* find the itemsets of the states that shifts can reach */
      new_itemsets();
      /* find or create the core structures for those states */
      append_states();

      /* create the shifts structures for the shifts to those states,
         now that the state numbers transitioning to are known */
      if (nshifts > 0)
        save_shifts();

      /* states are queued when they are created; process them all */
      this_state = this_state->next;
    }

  /* discard various storage */
  finalize_closure();
  free_storage();

  /* set up initial and final states as parser wants them */
  augment_automaton();
}



/* Find which symbols can be shifted in the current state,
   and for each one record which items would be active after that shift.
   Uses the contents of itemset.
   shift_symbol is set to a vector of the symbols that can be shifted.
   For each symbol in the grammar, kernel_base[symbol] points to
   a vector of item numbers activated if that symbol is shifted,
   and kernel_end[symbol] points after the end of that vector.  */
void
new_itemsets (void)
{
  register int i;
  register int shiftcount;
  register short *isp;
  register short *ksp;
  register int symbol;

#ifdef	TRACE
  fprintf(stderr, "Entering new_itemsets\n");
#endif

  for (i = 0; i < nsyms; i++)
    kernel_end[i] = NULL;

  shiftcount = 0;

  isp = itemset;

  while (isp < itemsetend)
    {
      i = *isp++;
      symbol = ritem[i];
      if (symbol > 0)
	{
          ksp = kernel_end[symbol];

          if (!ksp)
	    {
	      shift_symbol[shiftcount++] = symbol;
	      ksp = kernel_base[symbol];
	    }

          *ksp++ = i + 1;
          kernel_end[symbol] = ksp;
	}
    }

  nshifts = shiftcount;
}



/* Use the information computed by new_itemsets to find the state numbers
   reached by each shift transition from the current state.

   shiftset is set up as a vector of state numbers of those states.  */
void
append_states (void)
{
  register int i;
  register int j;
  register int symbol;

#ifdef	TRACE
  fprintf(stderr, "Entering append_states\n");
#endif

  /* first sort shift_symbol into increasing order */

  for (i = 1; i < nshifts; i++)
    {
      symbol = shift_symbol[i];
      j = i;
      while (j > 0 && shift_symbol[j - 1] > symbol)
	{
	  shift_symbol[j] = shift_symbol[j - 1];
	  j--;
	}
      shift_symbol[j] = symbol;
    }

  for (i = 0; i < nshifts; i++)
    {
      symbol = shift_symbol[i];
      shiftset[i] = get_state(symbol);
    }
}



/* find the state number for the state we would get to
(from the current state) by shifting symbol.
Create a new state if no equivalent one exists already.
Used by append_states  */

int
get_state (int symbol)
{
  register int key;
  register short *isp1;
  register short *isp2;
  register short *iend;
  register core *sp;
  register int found;

  int n;

#ifdef	TRACE
  fprintf(stderr, "Entering get_state, symbol = %d\n", symbol);
#endif

  isp1 = kernel_base[symbol];
  iend = kernel_end[symbol];
  n = iend - isp1;

  /* add up the target state's active item numbers to get a hash key */
  key = 0;
  while (isp1 < iend)
    key += *isp1++;

  key = key % STATE_TABLE_SIZE;

  sp = state_table[key];

  if (sp)
    {
      found = 0;
      while (!found)
	{
	  if (sp->nitems == n)
	    {
	      found = 1;
	      isp1 = kernel_base[symbol];
	      isp2 = sp->items;

	      while (found && isp1 < iend)
		{
		  if (*isp1++ != *isp2++)
		    found = 0;
		}
	    }

	  if (!found)
	    {
	      if (sp->link)
		{
		  sp = sp->link;
		}
	      else   /* bucket exhausted and no match */
		{
		  sp = sp->link = new_state(symbol);
		  found = 1;
		}
	    }
	}
    }
  else      /* bucket is empty */
    {
      state_table[key] = sp = new_state(symbol);
    }

  return (sp->number);
}



/* subroutine of get_state.  create a new state for those items, if necessary.  */

core *
new_state (int symbol)
{
  register int n;
  register core *p;
  register short *isp1;
  register short *isp2;
  register short *iend;

#ifdef	TRACE
  fprintf(stderr, "Entering new_state, symbol = %d\n", symbol);
#endif

  if (nstates >= MAXSHORT)
    toomany("states");

  isp1 = kernel_base[symbol];
  iend = kernel_end[symbol];
  n = iend - isp1;

  p = (core *) xmalloc((unsigned) (sizeof(core) + (n - 1) * sizeof(short)));
  p->accessing_symbol = symbol;
  p->number = nstates;
  p->nitems = n;

  isp2 = p->items;
  while (isp1 < iend)
    *isp2++ = *isp1++;

  last_state->next = p;
  last_state = p;

  nstates++;

  return (p);
}


void
initialize_states (void)
{
  register core *p;
/*  register unsigned *rp1; JF unused */
/*  register unsigned *rp2; JF unused */
/*  register unsigned *rend; JF unused */

  p = (core *) xmalloc((unsigned) (sizeof(core) - sizeof(short)));
  first_state = last_state = this_state = p;
  nstates = 1;
}


void
save_shifts (void)
{
  register shifts *p;
  register short *sp1;
  register short *sp2;
  register short *send;

  p = (shifts *) xmalloc((unsigned) (sizeof(shifts) +
				     (nshifts - 1) * sizeof(short)));

  p->number = this_state->number;
  p->nshifts = nshifts;

  sp1 = shiftset;
  sp2 = p->shifts;
  send = shiftset + nshifts;

  while (sp1 < send)
    *sp2++ = *sp1++;

  if (last_shift)
    {
      last_shift->next = p;
      last_shift = p;
    }
  else
    {
      first_shift = p;
      last_shift = p;
    }
}



/* find which rules can be used for reduction transitions from the current state
   and make a reductions structure for the state to record their rule numbers.  */
void
save_reductions (void)
{
  register short *isp;
  register short *rp1;
  register short *rp2;
  register int item;
  register int count;
  register reductions *p;

  short *rend;

  /* find and count the active items that represent ends of rules */

  count = 0;
  for (isp = itemset; isp < itemsetend; isp++)
    {
      item = ritem[*isp];
      if (item < 0)
	{
	  redset[count++] = -item;
	}
    }

  /* make a reductions structure and copy the data into it.  */

  if (count)
    {
      p = (reductions *) xmalloc((unsigned) (sizeof(reductions) +
					     (count - 1) * sizeof(short)));

      p->number = this_state->number;
      p->nreds = count;

      rp1 = redset;
      rp2 = p->rules;
      rend = rp1 + count;

      while (rp1 < rend)
	*rp2++ = *rp1++;

      if (last_reduction)
	{
	  last_reduction->next = p;
	  last_reduction = p;
	}
      else
	{
	  first_reduction = p;
	  last_reduction = p;
	}
    }
}



/* Make sure that the initial state has a shift that accepts the
grammar's start symbol and goes to the next-to-final state,
which has a shift going to the final state, which has a shift
to the termination state.
Create such states and shifts if they don't happen to exist already.  */
void
augment_automaton (void)
{
  register int i;
  register int k;
/*  register int found; JF unused */
  register core *statep;
  register shifts *sp;
  register shifts *sp2;
  register shifts *sp1 = NULL;

  sp = first_shift;

  if (sp)
    {
      if (sp->number == 0)
	{
	  k = sp->nshifts;
	  statep = first_state->next;

	  /* The states reached by shifts from first_state are numbered 1...K.
	     Look for one reached by start_symbol.  */
	  while (statep->accessing_symbol < start_symbol
		  && statep->number < k)
	    statep = statep->next;

	  if (statep->accessing_symbol == start_symbol)
	    {
	      /* We already have a next-to-final state.
		 Make sure it has a shift to what will be the final state.  */
	      k = statep->number;

	      while (sp && sp->number < k)
		{
		  sp1 = sp;
		  sp = sp->next;
		}

	      if (sp && sp->number == k)
		{
		  sp2 = (shifts *) xmalloc((unsigned) (sizeof(shifts)
						       + sp->nshifts * sizeof(short)));
		  sp2->number = k;
		  sp2->nshifts = sp->nshifts + 1;
		  sp2->shifts[0] = nstates;
		  for (i = sp->nshifts; i > 0; i--)
		    sp2->shifts[i] = sp->shifts[i - 1];

		  /* Patch sp2 into the chain of shifts in place of sp,
		     following sp1.  */
		  sp2->next = sp->next;
		  sp1->next = sp2;
		  if (sp == last_shift)
		    last_shift = sp2;
		  FREE(sp);
		}
	      else
		{
		  sp2 = NEW(shifts);
		  sp2->number = k;
		  sp2->nshifts = 1;
		  sp2->shifts[0] = nstates;

		  /* Patch sp2 into the chain of shifts between sp1 and sp.  */
		  sp2->next = sp;
		  sp1->next = sp2;
		  if (sp == 0)
		    last_shift = sp2;
		}
	    }
	  else
	    {
	      /* There is no next-to-final state as yet.  */
	      /* Add one more shift in first_shift,
		 going to the next-to-final state (yet to be made).  */
	      sp = first_shift;

	      sp2 = (shifts *) xmalloc(sizeof(shifts)
					 + sp->nshifts * sizeof(short));
	      sp2->nshifts = sp->nshifts + 1;

	      /* Stick this shift into the vector at the proper place.  */
	      statep = first_state->next;
	      for (k = 0, i = 0; i < sp->nshifts; k++, i++)
		{
		  if (statep->accessing_symbol > start_symbol && i == k)
		    sp2->shifts[k++] = nstates;
		  sp2->shifts[k] = sp->shifts[i];
		  statep = statep->next;
		}
	      if (i == k)
		sp2->shifts[k++] = nstates;

	      /* Patch sp2 into the chain of shifts
		 in place of sp, at the beginning.  */
	      sp2->next = sp->next;
	      first_shift = sp2;
	      if (last_shift == sp)
		last_shift = sp2;

	      FREE(sp);

	      /* Create the next-to-final state, with shift to
		 what will be the final state.  */
	      insert_start_shift();
	    }
	}
      else
	{
	  /* The initial state didn't even have any shifts.
	     Give it one shift, to the next-to-final state.  */
	  sp = NEW(shifts);
	  sp->nshifts = 1;
	  sp->shifts[0] = nstates;

	  /* Patch sp into the chain of shifts at the beginning.  */
	  sp->next = first_shift;
	  first_shift = sp;

	  /* Create the next-to-final state, with shift to
	     what will be the final state.  */
	  insert_start_shift();
	}
    }
  else
    {
      /* There are no shifts for any state.
	 Make one shift, from the initial state to the next-to-final state.  */

      sp = NEW(shifts);
      sp->nshifts = 1;
      sp->shifts[0] = nstates;

      /* Initialize the chain of shifts with sp.  */
      first_shift = sp;
      last_shift = sp;

      /* Create the next-to-final state, with shift to
	 what will be the final state.  */
      insert_start_shift();
    }

  /* Make the final state--the one that follows a shift from the
     next-to-final state.
     The symbol for that shift is 0 (end-of-file).  */
  statep = (core *) xmalloc((unsigned) (sizeof(core) - sizeof(short)));
  statep->number = nstates;
  last_state->next = statep;
  last_state = statep;

  /* Make the shift from the final state to the termination state.  */
  sp = NEW(shifts);
  sp->number = nstates++;
  sp->nshifts = 1;
  sp->shifts[0] = nstates;
  last_shift->next = sp;
  last_shift = sp;

  /* Note that the variable `final_state' refers to what we sometimes call
     the termination state.  */
  final_state = nstates;

  /* Make the termination state.  */
  statep = (core *) xmalloc((unsigned) (sizeof(core) - sizeof(short)));
  statep->number = nstates++;
  last_state->next = statep;
  last_state = statep;
}


/* subroutine of augment_automaton.
   Create the next-to-final state, to which a shift has already been made in
   the initial state.  */
void
insert_start_shift (void)
{
  register core *statep;
  register shifts *sp;

  statep = (core *) xmalloc((unsigned) (sizeof(core) - sizeof(short)));
  statep->number = nstates;
  statep->accessing_symbol = start_symbol;

  last_state->next = statep;
  last_state = statep;

  /* Make a shift from this state to (what will be) the final state.  */
  sp = NEW(shifts);
  sp->number = nstates++;
  sp->nshifts = 1;
  sp->shifts[0] = nstates;

  last_shift->next = sp;
  last_shift = sp;
}
