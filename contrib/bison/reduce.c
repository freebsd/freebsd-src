/* Grammar reduction for Bison.
   Copyright (C) 1988, 1989 Free Software Foundation, Inc.

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


/*
 * Reduce the grammar:  Find and eliminate unreachable terminals,
 * nonterminals, and productions.  David S. Bakin.
 */

/*
 * Don't eliminate unreachable terminals:  They may be used by the user's
 * parser.
 */

#include <stdio.h>
#include "system.h"
#include "files.h"
#include "gram.h"
#include "machine.h"
#include "alloc.h"


extern char   **tags;		/* reader.c */
extern int      verboseflag;	/* getargs.c */
static int      statisticsflag;	/* XXXXXXX */
extern int      fixed_outfiles;

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif
typedef int bool;
typedef unsigned *BSet;
typedef short  *rule;


/*
 * N is set of all nonterminals which are not useless.  P is set of all rules
 * which have no useless nonterminals in their RHS.  V is the set of all
 * accessible symbols.
 */

static BSet     N, P, V, V1;

static int      nuseful_productions, nuseless_productions,
                nuseful_nonterminals, nuseless_nonterminals;


bool bits_equal PARAMS((BSet, BSet, int));
int nbits PARAMS((unsigned));
int bits_size PARAMS((BSet, int));
void reduce_grammar PARAMS((void));
static void useless_nonterminals PARAMS((void));
static void inaccessable_symbols PARAMS((void));
static void reduce_grammar_tables PARAMS((void));
static void print_results PARAMS((void));
static void print_notices PARAMS((void));
void dump_grammar PARAMS((void));

extern void fatals PARAMS((char *, char *));


bool
bits_equal (BSet L, BSet R, int n)
{
  int i;

  for (i = n - 1; i >= 0; i--)
    if (L[i] != R[i])
      return FALSE;
  return TRUE;
}


int
nbits (unsigned i)
{
  int count = 0;

  while (i != 0) {
    i ^= (i & ((unsigned) (- (int) i)));
    ++count;
  }
  return count;
}


int
bits_size (BSet S, int n)
{
  int i, count = 0;

  for (i = n - 1; i >= 0; i--)
    count += nbits(S[i]);
  return count;
}

void
reduce_grammar (void)
{
  bool reduced;

  /* Allocate the global sets used to compute the reduced grammar */

  N = NEW2(WORDSIZE(nvars), unsigned);
  P = NEW2(WORDSIZE(nrules + 1), unsigned);
  V = NEW2(WORDSIZE(nsyms), unsigned);
  V1 = NEW2(WORDSIZE(nsyms), unsigned);

  useless_nonterminals();
  inaccessable_symbols();

  reduced = (bool) (nuseless_nonterminals + nuseless_productions > 0);

  if (verboseflag)
    print_results();

  if (reduced == FALSE)
    goto done_reducing;

  print_notices();

  if (!BITISSET(N, start_symbol - ntokens))
    fatals(_("Start symbol %s does not derive any sentence"),
	   tags[start_symbol]);

  reduce_grammar_tables();
  /* if (verboseflag) {
     fprintf(foutput, "REDUCED GRAMMAR\n\n");
     dump_grammar();
     }
     */

  /**/ statisticsflag = FALSE; /* someday getopts should handle this */
  if (statisticsflag == TRUE)
    fprintf(stderr,
	    _("reduced %s defines %d terminal%s, %d nonterminal%s\
, and %d production%s.\n"), infile,
	    ntokens, (ntokens == 1 ? "" : "s"),
	    nvars,   (nvars   == 1 ? "" : "s"),
	    nrules,  (nrules  == 1 ? "" : "s"));

 done_reducing:

  /* Free the global sets used to compute the reduced grammar */

  FREE(N);
  FREE(V);
  FREE(P);

}

/*
 * Another way to do this would be with a set for each production and then do
 * subset tests against N0, but even for the C grammar the whole reducing
 * process takes only 2 seconds on my 8Mhz AT.
 */

static bool
useful_production (int i, BSet N0)
{
  rule  r;
  short n;

  /*
   * A production is useful if all of the nonterminals in its RHS
   * appear in the set of useful nonterminals.
   */

  for (r = &ritem[rrhs[i]]; *r > 0; r++)
    if (ISVAR(n = *r))
      if (!BITISSET(N0, n - ntokens))
	return FALSE;
  return TRUE;
}


/* Remember that rules are 1-origin, symbols are 0-origin. */

static void
useless_nonterminals (void)
{
  BSet Np, Ns;
  int  i, n;

  /*
   * N is set as built.  Np is set being built this iteration. P is set
   * of all productions which have a RHS all in N.
   */

  Np = NEW2(WORDSIZE(nvars), unsigned);

  /*
   * The set being computed is a set of nonterminals which can derive
   * the empty string or strings consisting of all terminals. At each
   * iteration a nonterminal is added to the set if there is a
   * production with that nonterminal as its LHS for which all the
   * nonterminals in its RHS are already in the set.  Iterate until the
   * set being computed remains unchanged.  Any nonterminals not in the
   * set at that point are useless in that they will never be used in
   * deriving a sentence of the language.
   *
   * This iteration doesn't use any special traversal over the
   * productions.  A set is kept of all productions for which all the
   * nonterminals in the RHS are in useful.  Only productions not in
   * this set are scanned on each iteration.  At the end, this set is
   * saved to be used when finding useful productions: only productions
   * in this set will appear in the final grammar.
   */

  n = 0;
  while (1)
    {
      for (i = WORDSIZE(nvars) - 1; i >= 0; i--)
	Np[i] = N[i];
      for (i = 1; i <= nrules; i++)
	{
	  if (!BITISSET(P, i))
	    {
	      if (useful_production(i, N))
		{
		  SETBIT(Np, rlhs[i] - ntokens);
		  SETBIT(P, i);
		}
	    }
	}
      if (bits_equal(N, Np, WORDSIZE(nvars)))
	break;
      Ns = Np;
      Np = N;
      N = Ns;
    }
  FREE(N);
  N = Np;
}

static void
inaccessable_symbols (void)
{
  BSet  Vp, Vs, Pp;
  int   i, n;
  short t;
  rule  r;

  /*
   * Find out which productions are reachable and which symbols are
   * used.  Starting with an empty set of productions and a set of
   * symbols which only has the start symbol in it, iterate over all
   * productions until the set of productions remains unchanged for an
   * iteration.  For each production which has a LHS in the set of
   * reachable symbols, add the production to the set of reachable
   * productions, and add all of the nonterminals in the RHS of the
   * production to the set of reachable symbols.
   *
   * Consider only the (partially) reduced grammar which has only
   * nonterminals in N and productions in P.
   *
   * The result is the set P of productions in the reduced grammar, and
   * the set V of symbols in the reduced grammar.
   *
   * Although this algorithm also computes the set of terminals which are
   * reachable, no terminal will be deleted from the grammar. Some
   * terminals might not be in the grammar but might be generated by
   * semantic routines, and so the user might want them available with
   * specified numbers.  (Is this true?)  However, the nonreachable
   * terminals are printed (if running in verbose mode) so that the user
   * can know.
   */

  Vp = NEW2(WORDSIZE(nsyms), unsigned);
  Pp = NEW2(WORDSIZE(nrules + 1), unsigned);

  /* If the start symbol isn't useful, then nothing will be useful. */
  if (!BITISSET(N, start_symbol - ntokens))
    goto end_iteration;

  SETBIT(V, start_symbol);

  n = 0;
  while (1)
    {
      for (i = WORDSIZE(nsyms) - 1; i >= 0; i--)
	Vp[i] = V[i];
      for (i = 1; i <= nrules; i++)
	{
	  if (!BITISSET(Pp, i) && BITISSET(P, i) &&
	      BITISSET(V, rlhs[i]))
	    {
	      for (r = &ritem[rrhs[i]]; *r >= 0; r++)
		{
		  if (ISTOKEN(t = *r)
		      || BITISSET(N, t - ntokens))
		    {
		      SETBIT(Vp, t);
		    }
		}
	      SETBIT(Pp, i);
	    }
	}
      if (bits_equal(V, Vp, WORDSIZE(nsyms)))
	{
	  break;
	}
      Vs = Vp;
      Vp = V;
      V = Vs;
    }
 end_iteration:

  FREE(V);
  V = Vp;

  /* Tokens 0, 1, and 2 are internal to Bison.  Consider them useful. */
  SETBIT(V, 0);			/* end-of-input token */
  SETBIT(V, 1);			/* error token */
  SETBIT(V, 2);			/* some undefined token */

  FREE(P);
  P = Pp;

  nuseful_productions = bits_size(P, WORDSIZE(nrules + 1));
  nuseless_productions = nrules - nuseful_productions;

  nuseful_nonterminals = 0;
  for (i = ntokens; i < nsyms; i++)
    if (BITISSET(V, i))
      nuseful_nonterminals++;
  nuseless_nonterminals = nvars - nuseful_nonterminals;

  /* A token that was used in %prec should not be warned about.  */
  for (i = 1; i < nrules; i++)
    if (rprecsym[i] != 0)
      SETBIT(V1, rprecsym[i]);
}

static void
reduce_grammar_tables (void)
{
/* This is turned off because we would need to change the numbers
   in the case statements in the actions file.  */
#if 0
  /* remove useless productions */
  if (nuseless_productions > 0)
    {
      short np, pn, ni, pi;

      np = 0;
      ni = 0;
      for (pn = 1; pn <= nrules; pn++)
	{
	  if (BITISSET(P, pn))
	    {
	      np++;
	      if (pn != np)
		{
		  rlhs[np] = rlhs[pn];
		  rline[np] = rline[pn];
		  rprec[np] = rprec[pn];
		  rassoc[np] = rassoc[pn];
		  rrhs[np] = rrhs[pn];
		  if (rrhs[np] != ni)
		    {
		      pi = rrhs[np];
		      rrhs[np] = ni;
		      while (ritem[pi] >= 0)
			ritem[ni++] = ritem[pi++];
		      ritem[ni++] = -np;
		    }
		} else {
		  while (ritem[ni++] >= 0);
		}
	    }
	}
      ritem[ni] = 0;
      nrules -= nuseless_productions;
      nitems = ni;

      /*
       * Is it worth it to reduce the amount of memory for the
       * grammar? Probably not.
       */

    }
#endif /* 0 */
  /* Disable useless productions,
     since they may contain useless nonterms
     that would get mapped below to -1 and confuse everyone.  */
  if (nuseless_productions > 0)
    {
      int pn;

      for (pn = 1; pn <= nrules; pn++)
	{
	  if (!BITISSET(P, pn))
	    {
	      rlhs[pn] = -1;
	    }
	}
    }

  /* remove useless symbols */
  if (nuseless_nonterminals > 0)
    {

      int    i, n;
/*      short  j; JF unused */
      short *nontermmap;
      rule   r;

      /*
       * create a map of nonterminal number to new nonterminal
       * number. -1 in the map means it was useless and is being
       * eliminated.
       */

      nontermmap = NEW2(nvars, short) - ntokens;
      for (i = ntokens; i < nsyms; i++)
	nontermmap[i] = -1;

      n = ntokens;
      for (i = ntokens; i < nsyms; i++)
	if (BITISSET(V, i))
	  nontermmap[i] = n++;

      /* Shuffle elements of tables indexed by symbol number.  */

      for (i = ntokens; i < nsyms; i++)
	{
	  n = nontermmap[i];
	  if (n >= 0)
	    {
	      sassoc[n] = sassoc[i];
	      sprec[n] = sprec[i];
	      tags[n] = tags[i];
	    } else {
	      free(tags[i]);
	    }
	}

      /* Replace all symbol numbers in valid data structures.  */

      for (i = 1; i <= nrules; i++)
	{
	  /* Ignore the rules disabled above.  */
	  if (rlhs[i] >= 0)
	    rlhs[i] = nontermmap[rlhs[i]];
	  if (ISVAR (rprecsym[i]))
	    /* Can this happen?  */
	    rprecsym[i] = nontermmap[rprecsym[i]];
	}

      for (r = ritem; *r; r++)
	if (ISVAR(*r))
	  *r = nontermmap[*r];

      start_symbol = nontermmap[start_symbol];

      nsyms -= nuseless_nonterminals;
      nvars -= nuseless_nonterminals;

      free(&nontermmap[ntokens]);
    }
}

static void
print_results (void)
{
  int   i;
/*  short j; JF unused */
  rule  r;
  bool  b;

  if (nuseless_nonterminals > 0)
    {
      fprintf(foutput, _("Useless nonterminals:\n\n"));
      for (i = ntokens; i < nsyms; i++)
	if (!BITISSET(V, i))
	  fprintf(foutput, "   %s\n", tags[i]);
    }
  b = FALSE;
  for (i = 0; i < ntokens; i++)
    {
      if (!BITISSET(V, i) && !BITISSET(V1, i))
	{
	  if (!b)
	    {
	      fprintf(foutput, _("\n\nTerminals which are not used:\n\n"));
	      b = TRUE;
	    }
	  fprintf(foutput, "   %s\n", tags[i]);
	}
    }

  if (nuseless_productions > 0)
    {
      fprintf(foutput, _("\n\nUseless rules:\n\n"));
      for (i = 1; i <= nrules; i++)
	{
	  if (!BITISSET(P, i))
	    {
	      fprintf(foutput, "#%-4d  ", i);
	      fprintf(foutput, "%s :\t", tags[rlhs[i]]);
	      for (r = &ritem[rrhs[i]]; *r >= 0; r++)
		{
		  fprintf(foutput, " %s", tags[*r]);
		}
	      fprintf(foutput, ";\n");
	    }
	}
    }
  if (nuseless_nonterminals > 0 || nuseless_productions > 0 || b)
    fprintf(foutput, "\n\n");
}

void
dump_grammar (void)
{
  int i;
  rule r;

  fprintf(foutput,
	  "ntokens = %d, nvars = %d, nsyms = %d, nrules = %d, nitems = %d\n\n",
	  ntokens, nvars, nsyms, nrules, nitems);
  fprintf(foutput, _("Variables\n---------\n\n"));
  fprintf(foutput, _("Value  Sprec    Sassoc    Tag\n"));
  for (i = ntokens; i < nsyms; i++)
    fprintf(foutput, "%5d  %5d  %5d  %s\n",
	    i, sprec[i], sassoc[i], tags[i]);
  fprintf(foutput, "\n\n");
  fprintf(foutput, _("Rules\n-----\n\n"));
  for (i = 1; i <= nrules; i++)
    {
      fprintf(foutput, "%-5d(%5d%5d)%5d : (@%-5d)",
	      i, rprec[i], rassoc[i], rlhs[i], rrhs[i]);
      for (r = &ritem[rrhs[i]]; *r > 0; r++)
	fprintf(foutput, "%5d", *r);
      fprintf(foutput, " [%d]\n", -(*r));
    }
  fprintf(foutput, "\n\n");
  fprintf(foutput, _("Rules interpreted\n-----------------\n\n"));
  for (i = 1; i <= nrules; i++)
    {
      fprintf(foutput, "%-5d  %s :", i, tags[rlhs[i]]);
      for (r = &ritem[rrhs[i]]; *r > 0; r++)
	fprintf(foutput, " %s", tags[*r]);
      fprintf(foutput, "\n");
    }
  fprintf(foutput, "\n\n");
}


static void
print_notices (void)
{
  if (fixed_outfiles && nuseless_productions)
    fprintf(stderr, _("%d rules never reduced\n"), nuseless_productions);

  fprintf(stderr, _("%s contains "), infile);

  if (nuseless_nonterminals > 0)
    {
      fprintf(stderr, _("%d useless nonterminal%s"),
	      nuseless_nonterminals,
	      (nuseless_nonterminals == 1 ? "" : "s"));
    }
  if (nuseless_nonterminals > 0 && nuseless_productions > 0)
    fprintf(stderr, _(" and "));

  if (nuseless_productions > 0)
    {
      fprintf(stderr, _("%d useless rule%s"),
	      nuseless_productions,
	      (nuseless_productions == 1 ? "" : "s"));
    }
  fprintf(stderr, "\n");
  fflush(stderr);
}
