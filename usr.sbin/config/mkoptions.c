/*
 * Copyright (c) 1995  Peter Wemm
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static char sccsid[] = "@(#)mkheaders.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Make all the .h files for the optional entries
 */

#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "y.tab.h"

options()
{
	struct opt_list *ol;

	/* fake the cpu types as options */
	/* Please forgive me for this hack.. :-) */
	struct opt *op;
	struct cputype *cp;

	for (cp = cputype; cp; cp = cp->cpu_next) {
		struct opt *op = (struct opt *)malloc(sizeof (struct opt));
		memset(op, 0, sizeof(*op));
		op->op_name = cp->cpu_name;
		op->op_value = 0;
		op->op_next = opt;
		opt = op;
	}	

	read_options();
	for (ol = otab; ol != 0; ol = ol->o_next)
		do_option(ol->o_name);
}

/*
 * Generate an <options>.h file
 */

do_option(name)
	char *name;
{
	char *file, *inw, *tooption();
	struct opt *op, *op_head, *topp;
	FILE *inf, *outf;
	char *value;
	char *oldvalue;
	int seen;

	file = tooption(name);

	/*
	 * Check to see if the option was specified..
	 */
	value = NULL;
	for (op = opt; op; op = op->op_next) {
		if (eq(name, op->op_name)) {
			value = op->op_value;
			if (!value)
				value = ns("1");
			op->op_ownfile++;
		}
	}

	inf = fopen(file, "r");
	if (inf == 0) {
		outf = fopen(file, "w");
		if (outf == 0) {
			perror(file);
			exit(1);
		}

		/* was the option in the config file? */
		if (value) {
			fprintf(outf, "#define %s %s\n", name, value);
		} /* else empty file */

		(void) fclose(outf);
		return;
	}
	oldvalue = NULL;
	op_head = NULL;
	seen = 0;
	for (;;) {
		char *cp;
		char *invalue;

		/* get the #define */
		if ((inw = get_word(inf)) == 0 || inw == (char *)EOF)
			break;
		/* get the option name */
		if ((inw = get_word(inf)) == 0 || inw == (char *)EOF)
			break;
		inw = ns(inw);
		cp = get_word(inf);
		if (cp == 0 || cp == (char *)EOF)
			break;
		/* option value */
		invalue = ns(cp); /* malloced */
		if (eq(inw, name)) {
			oldvalue = invalue;
			invalue = value;
			seen++;
		}
		op = (struct opt *) malloc(sizeof *op);
		bzero(op, sizeof(*op));
		op->op_name = inw;
		op->op_value = invalue;
		op->op_next = op_head;
		op_head = op;

		/* EOL? */
		cp = get_word(inf);
		if (cp == (char *)EOF)
			break;
	}
	(void) fclose(inf);
	if ((value == NULL && oldvalue == NULL) ||
	    (value && oldvalue && eq(value,oldvalue))) {	
		for (op = op_head; op != NULL; op = topp) {
			topp = op->op_next;
			free(op->op_name);
			free(op->op_value);
			free(op);
		}
		return;
	}

	if (value && !seen) {
		/* New option appears */
		op = (struct opt *) malloc(sizeof *op);
		bzero(op, sizeof(*op));
		op->op_name = ns(name);
		op->op_value = value ? ns(value) : NULL;
		op->op_next = op_head;
		op_head = op;
	}

	outf = fopen(file, "w");
	if (outf == 0) {
		perror(file);
		exit(1);
	}
	for (op = op_head; op != NULL; op = topp) {
		/* was the option in the config file? */
		if (op->op_value) {
			fprintf(outf, "#define %s %s\n",
				op->op_name, op->op_value);
		}
		topp = op->op_next;
		free(op->op_name);
		free(op->op_value);
		free(op);
	}
	(void) fclose(outf);
}

/*
 * Find the filename to store the option spec into.
 */
char *
tooption(name)
	char *name;
{
	static char hbuf[80];
	char nbuf[80];
	struct opt_list *po;

	/* "cannot happen"?  the otab list should be complete.. */
	(void) strcpy(nbuf, "options.h");

	for (po = otab ; po != 0; po = po->o_next) {
		if (eq(po->o_name, name)) {
			strcpy(nbuf, po->o_file);
			break;
		}
	}

	(void) strcpy(hbuf, path(nbuf));
	return (hbuf);
}

/*
 * read the options and options.<machine> files
 */
read_options()
{
	FILE *fp;
	char fname[80];
	char *wd, *this, *val;
	struct opt_list *po;
	int first = 1;
	char genopt[80];
	char *lower();

	otab = 0;
	(void) snprintf(fname, sizeof fname, "../../conf/options");
openit:
	fp = fopen(fname, "r");
	if (fp == 0) {
		return;
	}
	if(ident == NULL) {
		printf("no ident line specified\n");
		exit(1);
	}
next:
	wd = get_word(fp);
	if (wd == (char *)EOF) {
		(void) fclose(fp);
		if (first == 1) {
			(void) snprintf(fname, sizeof fname, "options.%s", machinename);
			first++;
			goto openit;
		}
		if (first == 2) {
			(void) snprintf(fname, sizeof fname, "options.%s", raise(ident));
			first++;
			fp = fopen(fname, "r");
			if (fp != 0)
				goto next;
		}
		return;
	}
	if (wd == 0)
		goto next;
	/*************************************************\
	* If it's a comment ignore to the end of the line *
	\*************************************************/
	if(wd[0] == '#')
	{
		while( ((wd = get_word(fp)) != (char *)EOF) && wd)
		;
		goto next;
	}
	this = ns(wd);
	val = get_word(fp);
	if (val == (char *)EOF)
		return;
	if (val == 0) {
		char *s = ns(this);
		(void) snprintf(genopt, sizeof genopt, "opt_%s.h", lower(s));
		val = genopt;
		free(s);
	}
	val = ns(val);

	for (po = otab ; po != 0; po = po->o_next) {
		if (eq(po->o_name, this)) {
			printf("%s: Duplicate option %s.\n",
			       fname, this);
			exit(1);
		}
	}
	
	po = (struct opt_list *) malloc(sizeof *po);
	bzero(po, sizeof(*po));
	po->o_name = this;
	po->o_file = val;
	po->o_next = otab;
	otab = po;

	goto next;
}

char *
lower(str)
	register char *str;
{
	register char *cp = str;

	while (*str) {
		if (isupper(*str))
			*str = tolower(*str);
		str++;
	}
	return (cp);
}

