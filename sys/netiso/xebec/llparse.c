/*
 *	from: llparse.c,v 2.2 88/09/19 12:54:59 nhall Exp
 *	$Id: llparse.c,v 1.2 1993/10/16 21:33:09 rgrimes Exp $
 */

/*
 * ************************* NOTICE *******************************
 * This code is in the public domain.  It cannot be copyrighted.
 * This ll parser was originally written by Keith Thompson for the 
 * University of Wisconsin Crystal project.
 * It was based on an FMQ lr parser written by Jon Mauney at the
 * University of Wisconsin.
 * It was subsequently modified very slightly by Nancy Hall at the 
 * University of Wisconsin for the Crystal project.
 * ****************************************************************
 */
#include "xebec.h"
#include "llparse.h"
#include "main.h"
#include <stdio.h>

#include "debug.h"

#define LLMINACTION -LLINF

short		llparsestack[STACKSIZE];
short		llstackptr = 0;
LLtoken		lltoken;

llparse()
{
	register		havetoken = FALSE;
	register		sym;
	register LLtoken	*t = &lltoken;
	register		parseaction;
	register		accepted = FALSE;

	llpushprod(llnprods-1); /* $$$ ::= <start symbol>  */

	do {
		sym = llparsestack[llstackptr];
	IFDEBUG(L)
		printf("llparse() top of loop, llstackptr=%d, sym=%d\n",
			llstackptr, sym);
	ENDDEBUG

		if(sym < 0) {
			/* action symbol */
			if(sym <= LLMINACTION) {
				for(;sym<=LLMINACTION;sym++) {
					llaction(1, t); /* calls llfinprod */
				}
				llstackptr--;
				continue;
			} else { llaction(-sym, t);
				llstackptr--;
				continue;
			}
		}

		if(sym < llnterms) {

			/* it's a terminal symbol */

			if(!havetoken) {
				llgettoken(t);
				havetoken = TRUE;
			}

			if(sym == t->llterm) {
				llpushattr(t->llattrib);
				llaccept(t);
				llstackptr--; /* pop terminal */
				if(t->llterm == llnterms-1) { /* end symbol $$$ */
					accepted = TRUE;
				} else {
					havetoken = FALSE;
				}
			} else {
				llparsererror(t); /* wrong terminal on input */
				havetoken = FALSE;
			}
			continue;
		}

		/* non terminal */

		if(!havetoken) {
			llgettoken(t);
			havetoken = TRUE;
		}

		/* consult parse table  for new production */
		parseaction = llfindaction(sym, t->llterm);

		if(parseaction == 0) {
			/* error entry */
			llparsererror(t);
			havetoken = FALSE;
			continue;
		}

		if(llepsilon[parseaction]) {
			/* epsilon production */
			if(llepsilonok(t->llterm)) {
				llstackptr--; /* pop nonterminal */
				llpushprod(parseaction); /* push rhs of production */
			} else {
				llparsererror(t);
				havetoken = FALSE;
			}
		} else {
			llstackptr--; /* pop nonterminal */
			llpushprod(parseaction); /* push rhs of production */
		}
	} while(!accepted);

	return(0);
}

llpushprod(prod) 	/* recognize production prod - push rhs on stack */
short prod;
{
	register	start;
	register	length;
	register	count;

	start = llprodindex[prod].llprodstart;
	length = llprodindex[prod].llprodlength;

	IFDEBUG(L)
		printf("llpushprod(%d) llstackptr=0x%x(%d), length = 0x%x(%d)\n",
		prod, llstackptr, llstackptr, length , length);
		/*
		dump_parse_stack();
		*/
	ENDDEBUG
	if(llstackptr+length >= STACKSIZE) {
		fprintf(stderr,"Parse stack overflow. llstackptr=0x%x, length=0x%x\n",
		llstackptr, length);
		Exit(-1);
	}


	llsetattr(llprodindex[prod].llprodtlen);

	/* put a marker on the stack to mark beginning of production */
	if(llparsestack[llstackptr] <= LLMINACTION) {
		(llparsestack[llstackptr]) --; /* if there's already one there, don't
								put another on; just let it represent all of
								the adjacent markers */
	}
	else {
		llstackptr++;
		llparsestack[llstackptr] = LLMINACTION;
	}

	for(count=0; count<length; count++) {
		llstackptr++;
		llparsestack[llstackptr] = llproductions[start++];
	}
	if(llstackptr > STACKSIZE) {
		fprintf(stderr, "PARSE STACK OVERFLOW! \n"); Exit(-1);
		Exit(-1);
	}
}


llepsilonok(term)
{
	register	ptr;
	register	sym;
	register	pact;
	register	nomore;
	register	rval;

	IFDEBUG(L)
		printf("llepsilonok() enter\n");
	ENDDEBUG
	rval = TRUE;

	ptr = llstackptr;

	do {
		sym = llparsestack[ptr];

		if(sym < 0) {
			ptr--;
			nomore = ptr == 0;
			continue;
		}

		if(sym < llnterms) {
			nomore = TRUE;
			rval = sym == term;
			continue;
		}

		pact = llfindaction(sym, term);

		if(pact == 0) {
			nomore = TRUE;
			rval = FALSE;
			continue;
		}

		if(llepsilon[pact] == TRUE) {
			ptr--;
			nomore = ptr == 0;
		}
		else {
			nomore = TRUE;
		}

	} while(!nomore);

	return(rval);
}


short llfindaction(sym, term)
{
	register	index;

	IFDEBUG(L)
		printf("llfindaction(sym=%d, term=%d) enter \n", sym, term);
	ENDDEBUG
	index = llparseindex[sym];

	while(llparsetable[index].llterm != 0) {
		if(llparsetable[index].llterm == term) {
			return(llparsetable[index].llprod);
		}
		index++;
	}
	return(0);
}


llparsererror(token)
LLtoken *token;
{
	IFDEBUG(L)
		fprintf(stderr,"llparsererror() enter\n");
		prt_token(token);
	ENDDEBUG

	fprintf(stderr, "Syntax error: ");
	prt_token(token);
	dump_buffer();
	Exit(-1);
}


llgettoken(token)
LLtoken *token;
{
	llscan(token);
	token->llstate = NORMAL;
	IFDEBUG(L)
		printf("llgettoken(): ");
		prt_token(token);
	ENDDEBUG
}


/******************************************************************************

	Attribute support routines

******************************************************************************/
/*
**	attribute stack
**
**	AttrStack =	stack of record
**				values : array of values;
**				ptr	: index;
**	end;
**
*/

LLattrib	llattributes[LLMAXATTR];
int		llattrtop = 0;

struct llattr	llattrdesc[LLMAXDESC];

int	lldescindex = 1;


llsetattr(n)
{
	register struct llattr *ptr;

	IFDEBUG(L)
		printf("llsetattr(%d) enter\n",n);
	ENDDEBUG
	if(lldescindex >= LLMAXDESC) {
		fprintf(stdout, "llattribute stack overflow: desc\n");
		fprintf(stdout, 
			"lldescindex=0x%x, llattrtop=0x%x\n",lldescindex, llattrtop);
		Exit(-1);
	}
	ptr = &llattrdesc[lldescindex];
	ptr->llabase = &llattributes[llattrtop];
	ptr->lloldtop = ++llattrtop; 
	ptr->llaindex = 1;
	ptr->llacnt = n+1; /* the lhs ALWAYS uses an attr; it remains on the
						stack when the production is recognized */
	lldescindex++;
}

llpushattr(attr)
LLattrib attr;
{
	struct llattr *a;

	IFDEBUG(L)
		printf("llpushattr() enter\n");
	ENDDEBUG
	if(llattrtop + 1 > LLMAXATTR) {
		fprintf(stderr, "ATTRIBUTE STACK OVERFLOW!\n");
		Exit(-1);
	}
	a = &llattrdesc[lldescindex-1];
	llattributes[llattrtop++] = attr;
	a->llaindex++; /* inc count of attrs on the stack for this prod */
}

llfinprod()
{
	IFDEBUG(L)
		printf("llfinprod() enter\n");
	ENDDEBUG
	lldescindex--;
	llattrtop = llattrdesc[lldescindex].lloldtop;
	llattrdesc[lldescindex-1].llaindex++; /* lhs-of-prod.attr stays on
		the stack; it is now one of the rhs attrs of the now-top production
		on the stack */
}

#ifndef LINT
#ifdef DEBUG
dump_parse_stack()
{
	int ind;

	printf("PARSE STACK:\n");
	for(ind=llstackptr; ind>=0; ind--) {
		printf("%d\t%d\t%s\n",
		ind, llparsestack[ind],
		llparsestack[ind]<0? "Action symbol" : llstrings[llparsestack[ind]]);
	}
}

#endif DEBUG
#endif LINT

prt_token(t)
LLtoken *t;
{
	fprintf(stdout, "t at 0x%x\n", t);
	fprintf(stdout, "t->llterm=0x%x\n", t->llterm); (void) fflush(stdout);
	fprintf(stdout, "TOK: %s\n", llstrings[t->llterm]);
	(void) fflush(stdout);
#ifdef LINT
	/* to make lint shut up */
	fprintf(stdout, "", llnterms, llnsyms, llnprods, llinfinite);
#endif LINT
}
