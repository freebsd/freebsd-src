/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)lalr.c	5.3 (Berkeley) 6/1/90";
#endif
#endif

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD$");
#endif

#include <stdlib.h>
#include "defs.h"

typedef
  struct shorts
    {
      struct shorts *next;
      short value;
    }
  shorts;

int tokensetsize;
short *lookaheads;
short *LAruleno;
unsigned *LA;
short *accessing_symbol;
core **state_table;
shifts **shift_table;
reductions **reduction_table;
short *goto_map;
short *from_state;
short *to_state;

static void add_lookback_edge(int, int, int);
static void build_relations(void);
static void compute_FOLLOWS(void);
static void compute_lookaheads(void);
static void digraph(short **);
static void initialize_F(void);
static void initialize_LA(void);
static int map_goto(int, int);
static void set_accessing_symbol(void);
static void set_goto_map(void);
static void set_maxrhs(void);
static void set_reduction_table(void);
static void set_shift_table(void);
static void set_state_table(void);
static short **transpose(short **, int);
static void traverse(int);

static int infinity;
static int maxrhs;
static int ngotos;
static unsigned *F;
static short **includes;
static shorts **lookback;
static short **R;
static short *INDEX;
static short *VERTICES;
static int top;


void
lalr()
{
    tokensetsize = WORDSIZE(ntokens);

    set_state_table();
    set_accessing_symbol();
    set_shift_table();
    set_reduction_table();
    set_maxrhs();
    initialize_LA();
    set_goto_map();
    initialize_F();
    build_relations();
    compute_FOLLOWS();
    compute_lookaheads();
}



static void
set_state_table()
{
    core *sp;

    state_table = NEW2(nstates, core *);
    for (sp = first_state; sp; sp = sp->next)
	state_table[sp->number] = sp;
}



static void
set_accessing_symbol()
{
    core *sp;

    accessing_symbol = NEW2(nstates, short);
    for (sp = first_state; sp; sp = sp->next)
	accessing_symbol[sp->number] = sp->accessing_symbol;
}



static void
set_shift_table()
{
    shifts *sp;

    shift_table = NEW2(nstates, shifts *);
    for (sp = first_shift; sp; sp = sp->next)
	shift_table[sp->number] = sp;
}



static void
set_reduction_table()
{
    reductions *rp;

    reduction_table = NEW2(nstates, reductions *);
    for (rp = first_reduction; rp; rp = rp->next)
	reduction_table[rp->number] = rp;
}



static void
set_maxrhs()
{
  short *itemp;
  short *item_end;
  int length;
  int max;

  length = 0;
  max = 0;
  item_end = ritem + nitems;
  for (itemp = ritem; itemp < item_end; itemp++)
    {
      if (*itemp >= 0)
	{
	  length++;
	}
      else
	{
	  if (length > max) max = length;
	  length = 0;
	}
    }

  maxrhs = max;
}



static void
initialize_LA()
{
  int i, j, k;
  reductions *rp;

  lookaheads = NEW2(nstates + 1, short);

  k = 0;
  for (i = 0; i < nstates; i++)
    {
      lookaheads[i] = k;
      rp = reduction_table[i];
      if (rp)
	k += rp->nreds;
    }
  lookaheads[nstates] = k;

  LA = NEW2(k * tokensetsize, unsigned);
  LAruleno = NEW2(k, short);
  lookback = NEW2(k, shorts *);

  k = 0;
  for (i = 0; i < nstates; i++)
    {
      rp = reduction_table[i];
      if (rp)
	{
	  for (j = 0; j < rp->nreds; j++)
	    {
	      LAruleno[k] = rp->rules[j];
	      k++;
	    }
	}
    }
}


static void
set_goto_map()
{
  shifts *sp;
  int i;
  int symbol;
  int k;
  short *temp_map;
  int state2;
  int state1;

  goto_map = NEW2(nvars + 1, short) - ntokens;
  temp_map = NEW2(nvars + 1, short) - ntokens;

  ngotos = 0;
  for (sp = first_shift; sp; sp = sp->next)
    {
      for (i = sp->nshifts - 1; i >= 0; i--)
	{
	  symbol = accessing_symbol[sp->shift[i]];

	  if (ISTOKEN(symbol)) break;

	  if (ngotos == MAXSHORT)
	    fatal("too many gotos");

	  ngotos++;
	  goto_map[symbol]++;
        }
    }

  k = 0;
  for (i = ntokens; i < nsyms; i++)
    {
      temp_map[i] = k;
      k += goto_map[i];
    }

  for (i = ntokens; i < nsyms; i++)
    goto_map[i] = temp_map[i];

  goto_map[nsyms] = ngotos;
  temp_map[nsyms] = ngotos;

  from_state = NEW2(ngotos, short);
  to_state = NEW2(ngotos, short);

  for (sp = first_shift; sp; sp = sp->next)
    {
      state1 = sp->number;
      for (i = sp->nshifts - 1; i >= 0; i--)
	{
	  state2 = sp->shift[i];
	  symbol = accessing_symbol[state2];

	  if (ISTOKEN(symbol)) break;

	  k = temp_map[symbol]++;
	  from_state[k] = state1;
	  to_state[k] = state2;
	}
    }

  FREE(temp_map + ntokens);
}



/*  Map_goto maps a state/symbol pair into its numeric representation.	*/

static int
map_goto(state, symbol)
int state;
int symbol;
{
    int high;
    int low;
    int middle;
    int s;

    low = goto_map[symbol];
    high = goto_map[symbol + 1];

    for (;;)
    {
	assert(low <= high);
	middle = (low + high) >> 1;
	s = from_state[middle];
	if (s == state)
	    return (middle);
	else if (s < state)
	    low = middle + 1;
	else
	    high = middle - 1;
    }
}



static void
initialize_F()
{
  int i;
  int j;
  int k;
  shifts *sp;
  short *edge;
  unsigned *rowp;
  short *rp;
  short **reads;
  int nedges;
  int stateno;
  int symbol;
  int nwords;

  nwords = ngotos * tokensetsize;
  F = NEW2(nwords, unsigned);

  reads = NEW2(ngotos, short *);
  edge = NEW2(ngotos + 1, short);
  nedges = 0;

  rowp = F;
  for (i = 0; i < ngotos; i++)
    {
      stateno = to_state[i];
      sp = shift_table[stateno];

      if (sp)
	{
	  k = sp->nshifts;

	  for (j = 0; j < k; j++)
	    {
	      symbol = accessing_symbol[sp->shift[j]];
	      if (ISVAR(symbol))
		break;
	      SETBIT(rowp, symbol);
	    }

	  for (; j < k; j++)
	    {
	      symbol = accessing_symbol[sp->shift[j]];
	      if (nullable[symbol])
		edge[nedges++] = map_goto(stateno, symbol);
	    }

	  if (nedges)
	    {
	      reads[i] = rp = NEW2(nedges + 1, short);

	      for (j = 0; j < nedges; j++)
		rp[j] = edge[j];

	      rp[nedges] = -1;
	      nedges = 0;
	    }
	}

      rowp += tokensetsize;
    }

  SETBIT(F, 0);
  digraph(reads);

  for (i = 0; i < ngotos; i++)
    {
      if (reads[i])
	FREE(reads[i]);
    }

  FREE(reads);
  FREE(edge);
}



static void
build_relations()
{
  int i;
  int j;
  int k;
  short *rulep;
  short *rp;
  shifts *sp;
  int length;
  int nedges;
  int done1;
  int state1;
  int stateno;
  int symbol1;
  int symbol2;
  short *shortp;
  short *edge;
  short *states;
  short **new_includes;

  includes = NEW2(ngotos, short *);
  edge = NEW2(ngotos + 1, short);
  states = NEW2(maxrhs + 1, short);

  for (i = 0; i < ngotos; i++)
    {
      nedges = 0;
      state1 = from_state[i];
      symbol1 = accessing_symbol[to_state[i]];

      for (rulep = derives[symbol1]; *rulep >= 0; rulep++)
	{
	  length = 1;
	  states[0] = state1;
	  stateno = state1;

	  for (rp = ritem + rrhs[*rulep]; *rp >= 0; rp++)
	    {
	      symbol2 = *rp;
	      sp = shift_table[stateno];
	      k = sp->nshifts;

	      for (j = 0; j < k; j++)
		{
		  stateno = sp->shift[j];
		  if (accessing_symbol[stateno] == symbol2) break;
		}

	      states[length++] = stateno;
	    }

	  add_lookback_edge(stateno, *rulep, i);

	  length--;
	  done1 = 0;
	  while (!done1)
	    {
	      done1 = 1;
	      rp--;
	      if (ISVAR(*rp))
		{
		  stateno = states[--length];
		  edge[nedges++] = map_goto(stateno, *rp);
		  if (nullable[*rp] && length > 0) done1 = 0;
		}
	    }
	}

      if (nedges)
	{
	  includes[i] = shortp = NEW2(nedges + 1, short);
	  for (j = 0; j < nedges; j++)
	    shortp[j] = edge[j];
	  shortp[nedges] = -1;
	}
    }

  new_includes = transpose(includes, ngotos);

  for (i = 0; i < ngotos; i++)
    if (includes[i])
      FREE(includes[i]);

  FREE(includes);

  includes = new_includes;

  FREE(edge);
  FREE(states);
}


static void
add_lookback_edge(stateno, ruleno, gotono)
int stateno, ruleno, gotono;
{
    int i, k;
    int found;
    shorts *sp;

    i = lookaheads[stateno];
    k = lookaheads[stateno + 1];
    found = 0;
    while (!found && i < k)
    {
	if (LAruleno[i] == ruleno)
	    found = 1;
	else
	    ++i;
    }
    assert(found);

    sp = NEW(shorts);
    sp->next = lookback[i];
    sp->value = gotono;
    lookback[i] = sp;
}



static short **
transpose(r, n)
short **r;
int n;
{
  short **new_r;
  short **temp_r;
  short *nedges;
  short *sp;
  int i;
  int k;

  nedges = NEW2(n, short);

  for (i = 0; i < n; i++)
    {
      sp = r[i];
      if (sp)
	{
	  while (*sp >= 0)
	    nedges[*sp++]++;
	}
    }

  new_r = NEW2(n, short *);
  temp_r = NEW2(n, short *);

  for (i = 0; i < n; i++)
    {
      k = nedges[i];
      if (k > 0)
	{
	  sp = NEW2(k + 1, short);
	  new_r[i] = sp;
	  temp_r[i] = sp;
	  sp[k] = -1;
	}
    }

  FREE(nedges);

  for (i = 0; i < n; i++)
    {
      sp = r[i];
      if (sp)
	{
	  while (*sp >= 0)
	    *temp_r[*sp++]++ = i;
	}
    }

  FREE(temp_r);

  return (new_r);
}



static void
compute_FOLLOWS()
{
  digraph(includes);
}


static void
compute_lookaheads()
{
  int i, n;
  unsigned *fp1, *fp2, *fp3;
  shorts *sp, *next;
  unsigned *rowp;

  rowp = LA;
  n = lookaheads[nstates];
  for (i = 0; i < n; i++)
    {
      fp3 = rowp + tokensetsize;
      for (sp = lookback[i]; sp; sp = sp->next)
	{
	  fp1 = rowp;
	  fp2 = F + tokensetsize * sp->value;
	  while (fp1 < fp3)
	    *fp1++ |= *fp2++;
	}
      rowp = fp3;
    }

  for (i = 0; i < n; i++)
    for (sp = lookback[i]; sp; sp = next)
      {
        next = sp->next;
        FREE(sp);
      }

  FREE(lookback);
  FREE(F);
}


static void
digraph(relation)
short **relation;
{
  int i;

  infinity = ngotos + 2;
  INDEX = NEW2(ngotos + 1, short);
  VERTICES = NEW2(ngotos + 1, short);
  top = 0;

  R = relation;

  for (i = 0; i < ngotos; i++)
    INDEX[i] = 0;

  for (i = 0; i < ngotos; i++)
    {
      if (INDEX[i] == 0 && R[i])
	traverse(i);
    }

  FREE(INDEX);
  FREE(VERTICES);
}



static void
traverse(i)
int i;
{
  unsigned *fp1;
  unsigned *fp2;
  unsigned *fp3;
  int j;
  short *rp;

  int height;
  unsigned *base;

  VERTICES[++top] = i;
  INDEX[i] = height = top;

  base = F + i * tokensetsize;
  fp3 = base + tokensetsize;

  rp = R[i];
  if (rp)
    {
      while ((j = *rp++) >= 0)
	{
	  if (INDEX[j] == 0)
	    traverse(j);

	  if (INDEX[i] > INDEX[j])
	    INDEX[i] = INDEX[j];

	  fp1 = base;
	  fp2 = F + j * tokensetsize;

	  while (fp1 < fp3)
	    *fp1++ |= *fp2++;
	}
    }

  if (INDEX[i] == height)
    {
      for (;;)
	{
	  j = VERTICES[top--];
	  INDEX[j] = infinity;

	  if (i == j)
	    break;

	  fp1 = base;
	  fp2 = F + j * tokensetsize;

	  while (fp1 < fp3)
	    *fp2++ = *fp1++;
	}
    }
}
