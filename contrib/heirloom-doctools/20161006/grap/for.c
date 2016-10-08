/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/grap/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)for.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <stdlib.h>
#include "grap.h"
#include "y.tab.h"

typedef struct {
	Obj	*var;	/* index variable */
	double	to;	/* limit */
	double	by;
	int	op;	/* operator */
	char	*str;	/* string to push back */
} For;

#define	MAXFOR	10

For	forstk[MAXFOR];	/* stack of for loops */
For	*forp = forstk;	/* pointer to current top */

void forloop(Obj *var, double from, double to, int op, double by, char *str)	/* set up a for loop */
{
	fprintf(tfd, "# for %s from %g to %g by %c %g \n",
		var->name, from, to, op, by);
	if (++forp >= forstk+MAXFOR)
		FATAL("for loop nested too deep");
	forp->var = var;
	forp->to = to;
	forp->op = op;
	forp->by = by;
	forp->str = str;
	setvar(var, from);
	nextfor();
	unput('\n');
}

void nextfor(void)	/* do one iteration of a for loop */
{
	/* BUG:  this should depend on op and direction */
	if (forp->var->fval > SLOP * forp->to) {	/* loop is done */
		free(forp->str);
		if (--forp < forstk)
			FATAL("forstk popped too far");
	} else {		/* another iteration */
		pushsrc(String, "\nEndfor\n");
		pushsrc(String, forp->str);
	}
}

void endfor(void)	/* end one iteration of for loop */
{
	switch (forp->op) {
	case '+':
	case ' ':
		forp->var->fval += forp->by;
		break;
	case '-':
		forp->var->fval -= forp->by;
		break;
	case '*':
		forp->var->fval *= forp->by;
		break;
	case '/':
		forp->var->fval /= forp->by;
		break;
	}
	nextfor();
}

char *ifstat(double expr, char *thenpart, char *elsepart)
{
	dprintf("if %g then <%s> else <%s>\n", expr, thenpart, elsepart? elsepart : "");
	if (expr) {
		unput('\n');
		pushsrc(Free, thenpart);
		pushsrc(String, thenpart);
		unput('\n');
  		if (elsepart)
			free(elsepart);
		return thenpart;	/* to be freed later */
	} else {
		free(thenpart);
		if (elsepart) {
			unput('\n');
			pushsrc(Free, elsepart);
			pushsrc(String, elsepart);
			unput('\n');
		}
		return elsepart;
	}
}
