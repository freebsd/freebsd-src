/* Subroutines for bison
   Copyright (C) 1984, 1989 Free Software Foundation, Inc.

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


/* subroutines of file LR0.c.

Entry points:

  closure (items, n)

Given a vector of item numbers items, of length n,
set up ruleset and itemset to indicate what rules could be run
and which items could be accepted when those items are the active ones.

ruleset contains a bit for each rule.  closure sets the bits
for all rules which could potentially describe the next input to be read.

itemset is a vector of item numbers; itemsetend points to just beyond the end
 of the part of it that is significant.
closure places there the indices of all items which represent units of
input that could arrive next.

  initialize_closure (n)

Allocates the itemset and ruleset vectors,
and precomputes useful data so that closure can be called.
n is the number of elements to allocate for itemset.

  finalize_closure ()

Frees itemset, ruleset and internal data.

*/

#include <stdio.h>
#include "system.h"
#include "machine.h"
#include "alloc.h"
#include "gram.h"


extern short **derives;
extern char **tags;

void initialize_closure PARAMS((int));
void set_fderives PARAMS((void));
void set_firsts PARAMS((void));
void closure PARAMS((short *, int));
void finalize_closure PARAMS((void));

extern void RTC PARAMS((unsigned *, int));

short *itemset;
short *itemsetend;
static unsigned *ruleset;

/* internal data.  See comments before set_fderives and set_firsts.  */
static unsigned *fderives;
static unsigned *firsts;

/* number of words required to hold a bit for each rule */
static int rulesetsize;

/* number of words required to hold a bit for each variable */
static int varsetsize;


void
initialize_closure (int n)
{
  itemset = NEW2(n, short);

  rulesetsize = WORDSIZE(nrules + 1);
  ruleset = NEW2(rulesetsize, unsigned);

  set_fderives();
}



/* set fderives to an nvars by nrules matrix of bits
   indicating which rules can help derive the beginning of the data
   for each nonterminal.  For example, if symbol 5 can be derived as
   the sequence of symbols 8 3 20, and one of the rules for deriving
   symbol 8 is rule 4, then the [5 - ntokens, 4] bit in fderives is set.  */
void
set_fderives (void)
{
  register unsigned *rrow;
  register unsigned *vrow;
  register int j;
  register unsigned cword;
  register short *rp;
  register int b;

  int ruleno;
  int i;

  fderives = NEW2(nvars * rulesetsize, unsigned) - ntokens * rulesetsize;

  set_firsts();

  rrow = fderives + ntokens * rulesetsize;

  for (i = ntokens; i < nsyms; i++)
    {
      vrow = firsts + ((i - ntokens) * varsetsize);
      cword = *vrow++;
      b = 0;
      for (j = ntokens; j < nsyms; j++)
	{
	  if (cword & (1 << b))
	    {
	      rp = derives[j];
	      while ((ruleno = *rp++) > 0)
		{
		  SETBIT(rrow, ruleno);
		}
	    }

	  b++;
	  if (b >= BITS_PER_WORD && j + 1 < nsyms)
	    {
	      cword = *vrow++;
	      b = 0;
	    }
	}

      rrow += rulesetsize;
    }

#ifdef	DEBUG
  print_fderives();
#endif

  FREE(firsts);
}



/* set firsts to be an nvars by nvars bit matrix indicating which items
   can represent the beginning of the input corresponding to which other items.
   For example, if some rule expands symbol 5 into the sequence of symbols 8 3 20,
   the symbol 8 can be the beginning of the data for symbol 5,
   so the bit [8 - ntokens, 5 - ntokens] in firsts is set. */
void
set_firsts (void)
{
  register unsigned *row;
/*   register int done; JF unused */
  register int symbol;
  register short *sp;
  register int rowsize;

  int i;

  varsetsize = rowsize = WORDSIZE(nvars);

  firsts = NEW2(nvars * rowsize, unsigned);

  row = firsts;
  for (i = ntokens; i < nsyms; i++)
    {
      sp = derives[i];
      while (*sp >= 0)
	{
	  symbol = ritem[rrhs[*sp++]];
	  if (ISVAR(symbol))
	    {
	      symbol -= ntokens;
	      SETBIT(row, symbol);
	    }
	}

      row += rowsize;
    }

  RTC(firsts, nvars);

#ifdef	DEBUG
  print_firsts();
#endif
}


void
closure (short *core, int n)
{
  register int ruleno;
  register unsigned word;
  register short *csp;
  register unsigned *dsp;
  register unsigned *rsp;

  short *csend;
  unsigned *rsend;
  int symbol;
  int itemno;

  rsp = ruleset;
  rsend = ruleset + rulesetsize;
  csend = core + n;

  if (n == 0)
    {
      dsp = fderives + start_symbol * rulesetsize;
      while (rsp < rsend)
	*rsp++ = *dsp++;
    }
  else
    {
      while (rsp < rsend)
	*rsp++ = 0;

      csp = core;
      while (csp < csend)
	{
	  symbol = ritem[*csp++];
	  if (ISVAR(symbol))
	    {
	      dsp = fderives + symbol * rulesetsize;
	      rsp = ruleset;
	      while (rsp < rsend)
		*rsp++ |= *dsp++;
	    }
	}
    }

  ruleno = 0;
  itemsetend = itemset;
  csp = core;
  rsp = ruleset;
  while (rsp < rsend)
    {
      word = *rsp++;
      if (word == 0)
	{
	  ruleno += BITS_PER_WORD;
	}
      else
	{
	  register int b;

	  for (b = 0; b < BITS_PER_WORD; b++)
	    {
	      if (word & (1 << b))
		{
		  itemno = rrhs[ruleno];
		  while (csp < csend && *csp < itemno)
		    *itemsetend++ = *csp++;
		  *itemsetend++ = itemno;
		}

	      ruleno++;
	    }
	}
    }

  while (csp < csend)
    *itemsetend++ = *csp++;

#ifdef	DEBUG
  print_closure(n);
#endif
}


void
finalize_closure (void)
{
  FREE(itemset);
  FREE(ruleset);
  FREE(fderives + ntokens * rulesetsize);
}



#ifdef	DEBUG

print_closure(n)
int n;
{
  register short *isp;

  printf("\n\nn = %d\n\n", n);
  for (isp = itemset; isp < itemsetend; isp++)
    printf("   %d\n", *isp);
}


void
print_firsts (void)
{
  register int i;
  register int j;
  register unsigned *rowp;

  printf(_("\n\n\nFIRSTS\n\n"));

  for (i = ntokens; i < nsyms; i++)
    {
      printf(_("\n\n%s firsts\n\n"), tags[i]);

      rowp = firsts + ((i - ntokens) * varsetsize);

      for (j = 0; j < nvars; j++)
	if (BITISSET (rowp, j))
	  printf("   %s\n", tags[j + ntokens]);
    }
}


void
print_fderives (void)
{
  register int i;
  register int j;
  register unsigned *rp;

  printf(_("\n\n\nFDERIVES\n"));

  for (i = ntokens; i < nsyms; i++)
    {
      printf(_("\n\n%s derives\n\n"), tags[i]);
      rp = fderives + i * rulesetsize;

      for (j = 0; j <= nrules; j++)
	if (BITISSET (rp, j))
	  printf("   %d\n", j);
    }

  fflush(stdout);
}

#endif
