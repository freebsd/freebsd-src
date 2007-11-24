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
static char sccsid[] = "@(#)mkpar.c	5.3 (Berkeley) 1/20/91";
#endif
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include "defs.h"

action **parser;
int SRexpect;
int SRtotal;
int RRtotal;
short *SRconflicts;
short *RRconflicts;
short *defred;
short *rules_used;
short nunused;
short final_state;

static int SRcount;
static int RRcount;

static action *add_reduce(action *, int, int);
static action *add_reductions(int, action *);
static void defreds(void);
static void find_final_state(void);
static void free_action_row(action *);
static action *get_shifts(int);
static action *parse_actions(int);
static void remove_conflicts(void);
static int sole_reduction(int);
static void total_conflicts(void);
static void unused_rules(void);


void
make_parser()
{
    int i;

    parser = NEW2(nstates, action *);
    for (i = 0; i < nstates; i++)
	parser[i] = parse_actions(i);

    find_final_state();
    remove_conflicts();
    unused_rules();
    if (SRtotal + RRtotal > 0) total_conflicts();
    defreds();
}


static action *
parse_actions(stateno)
int stateno;
{
    action *actions;

    actions = get_shifts(stateno);
    actions = add_reductions(stateno, actions);
    return (actions);
}


static action *
get_shifts(stateno)
int stateno;
{
    action *actions, *temp;
    shifts *sp;
    short *tostate;
    int i, k;
    int symbol;

    actions = 0;
    sp = shift_table[stateno];
    if (sp)
    {
	tostate = sp->shift;
	for (i = sp->nshifts - 1; i >= 0; i--)
	{
	    k = tostate[i];
	    symbol = accessing_symbol[k];
	    if (ISTOKEN(symbol))
	    {
		temp = NEW(action);
		temp->next = actions;
		temp->symbol = symbol;
		temp->number = k;
		temp->prec = symbol_prec[symbol];
		temp->action_code = SHIFT;
		temp->assoc = symbol_assoc[symbol];
		actions = temp;
	    }
	}
    }
    return (actions);
}

static action *
add_reductions(stateno, actions)
int stateno;
action *actions;
{
    int i, j, m, n;
    int ruleno, tokensetsize;
    unsigned *rowp;

    tokensetsize = WORDSIZE(ntokens);
    m = lookaheads[stateno];
    n = lookaheads[stateno + 1];
    for (i = m; i < n; i++)
    {
	ruleno = LAruleno[i];
	rowp = LA + i * tokensetsize;
	for (j = ntokens - 1; j >= 0; j--)
	{
	    if (BIT(rowp, j))
		actions = add_reduce(actions, ruleno, j);
	}
    }
    return (actions);
}


static action *
add_reduce(actions, ruleno, symbol)
action *actions;
int ruleno, symbol;
{
    action *temp, *prev, *next;

    prev = 0;
    for (next = actions; next && next->symbol < symbol; next = next->next)
	prev = next;

    while (next && next->symbol == symbol && next->action_code == SHIFT)
    {
	prev = next;
	next = next->next;
    }

    while (next && next->symbol == symbol &&
	    next->action_code == REDUCE && next->number < ruleno)
    {
	prev = next;
	next = next->next;
    }

    temp = NEW(action);
    temp->next = next;
    temp->symbol = symbol;
    temp->number = ruleno;
    temp->prec = rprec[ruleno];
    temp->action_code = REDUCE;
    temp->assoc = rassoc[ruleno];

    if (prev)
	prev->next = temp;
    else
	actions = temp;

    return (actions);
}


static void
find_final_state()
{
    int goal, i;
    short *tostate;
    shifts *p;

    p = shift_table[0];
    tostate = p->shift;
    goal = ritem[1];
    for (i = p->nshifts - 1; i >= 0; --i)
    {
	final_state = tostate[i];
	if (accessing_symbol[final_state] == goal) break;
    }
}


static void
unused_rules()
{
    int i;
    action *p;

    rules_used = (short *) MALLOC(nrules*sizeof(short));
    if (rules_used == 0) no_space();

    for (i = 0; i < nrules; ++i)
	rules_used[i] = 0;

    for (i = 0; i < nstates; ++i)
    {
	for (p = parser[i]; p; p = p->next)
	{
	    if (p->action_code == REDUCE && p->suppressed == 0)
		rules_used[p->number] = 1;
	}
    }

    nunused = 0;
    for (i = 3; i < nrules; ++i)
	if (!rules_used[i]) ++nunused;

    if (nunused) {
	if (nunused == 1)
	    warnx("1 rule never reduced");
	else
	    warnx("%d rules never reduced", nunused);
    }
}


static void
remove_conflicts()
{
    int i;
    int symbol;
    action *p, *pref;

    pref = NULL;
    SRtotal = 0;
    RRtotal = 0;
    SRconflicts = NEW2(nstates, short);
    RRconflicts = NEW2(nstates, short);
    for (i = 0; i < nstates; i++)
    {
	SRcount = 0;
	RRcount = 0;
	symbol = -1;
	for (p = parser[i]; p; p = p->next)
	{
	    if (p->symbol != symbol)
	    {
		pref = p;
		symbol = p->symbol;
	    }
	    else if (i == final_state && symbol == 0)
	    {
		SRcount++;
		p->suppressed = 1;
	    }
	    else if (pref->action_code == SHIFT)
	    {
		if (pref->prec > 0 && p->prec > 0)
		{
		    if (pref->prec < p->prec)
		    {
			pref->suppressed = 2;
			pref = p;
		    }
		    else if (pref->prec > p->prec)
		    {
			p->suppressed = 2;
		    }
		    else if (pref->assoc == LEFT)
		    {
			pref->suppressed = 2;
			pref = p;
		    }
		    else if (pref->assoc == RIGHT)
		    {
			p->suppressed = 2;
		    }
		    else
		    {
			pref->suppressed = 2;
			p->suppressed = 2;
		    }
		}
		else
		{
		    SRcount++;
		    p->suppressed = 1;
		}
	    }
	    else
	    {
		RRcount++;
		p->suppressed = 1;
	    }
	}
	SRtotal += SRcount;
	RRtotal += RRcount;
	SRconflicts[i] = SRcount;
	RRconflicts[i] = RRcount;
    }
}


static void
total_conflicts()
{
    /* Warn if s/r != expect or if any r/r */
    if ((SRtotal != SRexpect) || RRtotal)
    {
	    if (SRtotal == 1)
	    warnx("1 shift/reduce conflict");
	    else if (SRtotal > 1)
	    warnx("%d shift/reduce conflicts", SRtotal);
    }

    if (RRtotal == 1)
	warnx("1 reduce/reduce conflict");
    else if (RRtotal > 1)
	warnx("%d reduce/reduce conflicts", RRtotal);
}


static int
sole_reduction(stateno)
int stateno;
{
    int count, ruleno;
    action *p;

    count = 0;
    ruleno = 0;
    for (p = parser[stateno]; p; p = p->next)
    {
	if (p->action_code == SHIFT && p->suppressed == 0)
	    return (0);
	else if (p->action_code == REDUCE && p->suppressed == 0)
	{
	    if (ruleno > 0 && p->number != ruleno)
		return (0);
	    if (p->symbol != 1)
		++count;
	    ruleno = p->number;
	}
    }

    if (count == 0)
	return (0);
    return (ruleno);
}


static void
defreds()
{
    int i;

    defred = NEW2(nstates, short);
    for (i = 0; i < nstates; i++)
	defred[i] = sole_reduction(i);
}

static void
free_action_row(p)
action *p;
{
  action *q;

  while (p)
    {
      q = p->next;
      FREE(p);
      p = q;
    }
}

void
free_parser()
{
  int i;

  for (i = 0; i < nstates; i++)
    free_action_row(parser[i]);

  FREE(parser);
}
