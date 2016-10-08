/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/pic/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)symtab.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "pic.h"
#include "y.tab.h"

YYSTYPE getvar(char *s)	/* return value of variable s (usually pointer) */
{
	struct symtab *p;
	static YYSTYPE bug;

	p = lookup(s);
	if (p == NULL) {
		if (islower((int)s[0]))
			WARNING("no such variable as %s", s);
		else
			WARNING("no such place as %s", s);
		return(bug);
	}
	return(p->s_val);
}

double getfval(char *s)	/* return float value of variable s */
{
	YYSTYPE y;

	y = getvar(s);
	return y.f;
}

void setfval(char *s, double f)	/* set variable s to f */
{
	struct symtab *p;

	if ((p = lookup(s)) != NULL)
		p->s_val.f = f;
}

struct symtab *makevar(char *s, int t, YYSTYPE v)	/* make variable named s in table */
		/* assumes s is static or from tostring */
{
	struct symtab *p;

	for (p = stack[nstack].p_symtab; p != NULL; p = p->s_next)
		if (strcmp(s, p->s_name) == 0)
			break;
	if (p == NULL) {	/* it's a new one */
		p = (struct symtab *) malloc(sizeof(struct symtab));
		if (p == NULL)
			FATAL("out of symtab space with %s", s);
		p->s_next = stack[nstack].p_symtab;
		stack[nstack].p_symtab = p;	/* stick it at front */
	}
	p->s_name = s;
	p->s_type = t;
	p->s_val = v;
	return(p);
}

struct symtab *lookup(char *s)	/* find s in symtab */
{
	int i;
	struct symtab *p;

	for (i = nstack; i >= 0; i--)	/* look in each active symtab */
		for (p = stack[i].p_symtab; p != NULL; p = p->s_next)
			if (strcmp(s, p->s_name) == 0)
				return(p);
	return(NULL);
}

void freesymtab(struct symtab *p)	/* free space used by symtab at p */
{
	struct symtab *q;

	for ( ; p != NULL; p = q) {
		q = p->s_next;
		free(p->s_name);	/* assumes done with tostring */
		free((char *)p);
	}
}

void freedef(char *s)	/* free definition for string s */
{
	struct symtab *p, *q, *op;

	for (p = op = q = stack[nstack].p_symtab; p != NULL; p = p->s_next) {
		if (strcmp(s, p->s_name) == 0) { 	/* got it */
			if (p->s_type != DEFNAME)
				break;
			if (p == op)	/* 1st elem */
				stack[nstack].p_symtab = p->s_next;
			else
				q->s_next = p->s_next;
			free(p->s_name);
			free(p->s_val.p);
			free((char *)p);
			return;
		}
		q = p;
	}
	/* WARNING("%s is not defined at this point", s); */
}
