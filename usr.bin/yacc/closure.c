#include "defs.h"

short *itemset;
short *itemsetend;
unsigned *ruleset;

static unsigned *first_derives;
static unsigned *EFF;


set_EFF()
{
    register unsigned *row;
    register int symbol;
    register short *sp;
    register int rowsize;
    register int i;
    register int rule;

    rowsize = WORDSIZE(nvars);
    EFF = NEW2(nvars * rowsize, unsigned);

    row = EFF;
    for (i = start_symbol; i < nsyms; i++)
    {
	sp = derives[i];
	for (rule = *sp; rule > 0; rule = *++sp)
	{
	    symbol = ritem[rrhs[rule]];
	    if (ISVAR(symbol))
	    {
		symbol -= start_symbol;
		SETBIT(row, symbol);
	    }
	}
	row += rowsize;
    }

    reflexive_transitive_closure(EFF, nvars);

#ifdef	DEBUG
    print_EFF();
#endif
}


set_first_derives()
{
    register unsigned *rrow;
    register unsigned *vrow;
    register int j;
    register unsigned k;
    register unsigned cword;
    register short *rp;

    int rule;
    int i;
    int rulesetsize;
    int varsetsize;

    rulesetsize = WORDSIZE(nrules);
    varsetsize = WORDSIZE(nvars);
    first_derives = NEW2(nvars * rulesetsize, unsigned) - ntokens * rulesetsize;

    set_EFF();

    rrow = first_derives + ntokens * rulesetsize;
    for (i = start_symbol; i < nsyms; i++)
    {
	vrow = EFF + ((i - ntokens) * varsetsize);
	k = BITS_PER_WORD;
	for (j = start_symbol; j < nsyms; k++, j++)
	{
	    if (k >= BITS_PER_WORD)
	    {
		cword = *vrow++;
		k = 0;
	    }

	    if (cword & (1 << k))
	    {
		rp = derives[j];
		while ((rule = *rp++) >= 0)
		{
		    SETBIT(rrow, rule);
		}
	    }
	}

	vrow += varsetsize;
	rrow += rulesetsize;
    }

#ifdef	DEBUG
    print_first_derives();
#endif

    FREE(EFF);
}


closure(nucleus, n)
short *nucleus;
int n;
{
    register int ruleno;
    register unsigned word;
    register unsigned i;
    register short *csp;
    register unsigned *dsp;
    register unsigned *rsp;
    register int rulesetsize;

    short *csend;
    unsigned *rsend;
    int symbol;
    int itemno;

    rulesetsize = WORDSIZE(nrules);
    rsp = ruleset;
    rsend = ruleset + rulesetsize;
    for (rsp = ruleset; rsp < rsend; rsp++)
	*rsp = 0;

    csend = nucleus + n;
    for (csp = nucleus; csp < csend; ++csp)
    {
	symbol = ritem[*csp];
	if (ISVAR(symbol))
	{
	    dsp = first_derives + symbol * rulesetsize;
	    rsp = ruleset;
	    while (rsp < rsend)
		*rsp++ |= *dsp++;
	}
    }

    ruleno = 0;
    itemsetend = itemset;
    csp = nucleus;
    for (rsp = ruleset; rsp < rsend; ++rsp)
    {
	word = *rsp;
	if (word)
	{
	    for (i = 0; i < BITS_PER_WORD; ++i)
	    {
		if (word & (1 << i))
		{
		    itemno = rrhs[ruleno+i];
		    while (csp < csend && *csp < itemno)
			*itemsetend++ = *csp++;
		    *itemsetend++ = itemno;
		    while (csp < csend && *csp == itemno)
			++csp;
		}
	    }
	}
	ruleno += BITS_PER_WORD;
    }

    while (csp < csend)
	*itemsetend++ = *csp++;

#ifdef	DEBUG
  print_closure(n);
#endif
}



finalize_closure()
{
  FREE(itemset);
  FREE(ruleset);
  FREE(first_derives + ntokens * WORDSIZE(nrules));
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


print_EFF()
{
    register int i, j;
    register unsigned *rowp;
    register unsigned word;
    register unsigned k;

    printf("\n\nEpsilon Free Firsts\n");

    for (i = start_symbol; i < nsyms; i++)
    {
	printf("\n%s", symbol_name[i]);
	rowp = EFF + ((i - start_symbol) * WORDSIZE(nvars));
	word = *rowp++;

	k = BITS_PER_WORD;
	for (j = 0; j < nvars; k++, j++)
	{
	    if (k >= BITS_PER_WORD)
	    {
		word = *rowp++;
		k = 0;
	    }

	    if (word & (1 << k))
		printf("  %s", symbol_name[start_symbol + j]);
	}
    }
}


print_first_derives()
{
    register int i;
    register int j;
    register unsigned *rp;
    register unsigned cword;
    register unsigned k;

    printf("\n\n\nFirst Derives\n");

    for (i = start_symbol; i < nsyms; i++)
    {
	printf("\n%s derives\n", symbol_name[i]);
	rp = first_derives + i * WORDSIZE(nrules);
	k = BITS_PER_WORD;
	for (j = 0; j <= nrules; k++, j++)
        {
	  if (k >= BITS_PER_WORD)
	  {
	      cword = *rp++;
	      k = 0;
	  }

	  if (cword & (1 << k))
	    printf("   %d\n", j);
	}
    }

  fflush(stdout);
}

#endif
