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
#if 0
static char sccsid[] = "@(#)mkheaders.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Make all the .h files for the optional entries
 */

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "config.h"
#include "y.tab.h"

static	struct users {
	int	u_default;
	int	u_min;
	int	u_max;
} users[] = {
	{ 8, 2, 512 },			/* MACHINE_I386 */
	{ 8, 2, 512 },			/* MACHINE_PC98 */
	{ 8, 2, 512 },			/* MACHINE_ALPHA */
	{ 8, 2, 512 },			/* MACHINE_IA64 */
};
#define	NUSERS	(sizeof (users) / sizeof (users[0]))

static char *lower(char *);
static void read_options(void);
static void do_option(char *);
static char *tooption(char *);

void
options(void)
{
	char buf[40];
	struct cputype *cp;
	struct opt_list *ol;
	struct opt *op;
	struct users *up;

	/* Fake the cpu types as options. */
	for (cp = cputype; cp != NULL; cp = cp->cpu_next) {
		op = (struct opt *)malloc(sizeof(*op));
		memset(op, 0, sizeof(*op));
		op->op_name = ns(cp->cpu_name);
		op->op_next = opt;
		opt = op;
	}	

	/* Initialize `maxusers'. */
	if ((unsigned)machine > NUSERS) {
		printf("maxusers config info isn't present, using i386\n");
		up = &users[MACHINE_I386 - 1];
	} else
		up = &users[machine - 1];
	if (maxusers == 0) {
		printf("maxusers not specified; %d assumed\n", up->u_default);
		maxusers = up->u_default;
	} else if (maxusers < up->u_min) {
		printf("minimum of %d maxusers assumed\n", up->u_min);
		maxusers = up->u_min;
	} else if (maxusers > up->u_max)
		printf("warning: maxusers > %d (%d)\n", up->u_max, maxusers);

	/* Fake MAXUSERS as an option. */
	op = (struct opt *)malloc(sizeof(*op));
	memset(op, 0, sizeof(*op));
	op->op_name = "MAXUSERS";
	snprintf(buf, sizeof(buf), "%d", maxusers);
	op->op_value = ns(buf);
	op->op_next = opt;
	opt = op;

	read_options();
	for (ol = otab; ol != 0; ol = ol->o_next)
		do_option(ol->o_name);
	for (op = opt; op; op = op->op_next) {
		if (!op->op_ownfile) {
			printf("%s:%d: unknown option \"%s\"\n",
			       PREFIX, op->op_line, op->op_name);
			exit(1);
		}
	}
}

/*
 * Generate an <options>.h file
 */

static void
do_option(char *name)
{
	char *basefile, *file, *inw;
	struct opt_list *ol;
	struct opt *op, *op_head, *topp;
	FILE *inf, *outf;
	char *value;
	char *oldvalue;
	int seen;
	int tidy;

	file = tooption(name);

	/*
	 * Check to see if the option was specified..
	 */
	value = NULL;
	for (op = opt; op; op = op->op_next) {
		if (eq(name, op->op_name)) {
			oldvalue = value;
			value = op->op_value;
			if (value == NULL)
				value = ns("1");
			if (oldvalue != NULL && !eq(value, oldvalue))
				printf(
			    "%s:%d: option \"%s\" redefined from %s to %s\n",
				   PREFIX, op->op_line, op->op_name, oldvalue,
				   value);
			op->op_ownfile++;
		}
	}

	inf = fopen(file, "r");
	if (inf == 0) {
		outf = fopen(file, "w");
		if (outf == 0)
			err(1, "%s", file);

		/* was the option in the config file? */
		if (value) {
			fprintf(outf, "#define %s %s\n", name, value);
		} /* else empty file */

		(void) fclose(outf);
		return;
	}
	basefile = "";
	for (ol = otab; ol != 0; ol = ol->o_next)
		if (eq(name, ol->o_name)) {
			basefile = ol->o_file;
			break;
		}
	oldvalue = NULL;
	op_head = NULL;
	seen = 0;
	tidy = 0;
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
		/* get the option value */
		if ((cp = get_word(inf)) == 0 || cp == (char *)EOF)
			break;
		/* option value */
		invalue = ns(cp); /* malloced */
		if (eq(inw, name)) {
			oldvalue = invalue;
			invalue = value;
			seen++;
		}
		for (ol = otab; ol != 0; ol = ol->o_next)
			if (eq(inw, ol->o_name))
				break;
		if (!eq(inw, name) && !ol) {
			printf("WARNING: unknown option `%s' removed from %s\n",
				inw, file);
			tidy++;
		} else if (ol != NULL && !eq(basefile, ol->o_file)) {
			printf("WARNING: option `%s' moved from %s to %s\n",
				inw, basefile, ol->o_file);
			tidy++;
		} else {
			op = (struct opt *) malloc(sizeof *op);
			bzero(op, sizeof(*op));
			op->op_name = inw;
			op->op_value = invalue;
			op->op_next = op_head;
			op_head = op;
		}

		/* EOL? */
		cp = get_word(inf);
		if (cp == (char *)EOF)
			break;
	}
	(void) fclose(inf);
	if (!tidy && ((value == NULL && oldvalue == NULL) ||
	    (value && oldvalue && eq(value, oldvalue)))) {	
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
	if (outf == 0)
		err(1, "%s", file);
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
static char *
tooption(char *name)
{
	static char hbuf[MAXPATHLEN];
	char nbuf[MAXPATHLEN];
	struct opt_list *po;

	/* "cannot happen"?  the otab list should be complete.. */
	(void) strlcpy(nbuf, "options.h", sizeof(nbuf));

	for (po = otab ; po != 0; po = po->o_next) {
		if (eq(po->o_name, name)) {
			strlcpy(nbuf, po->o_file, sizeof(nbuf));
			break;
		}
	}

	(void) strlcpy(hbuf, path(nbuf), sizeof(hbuf));
	return (hbuf);
}

/*
 * read the options and options.<machine> files
 */
static void
read_options(void)
{
	FILE *fp;
	char fname[MAXPATHLEN];
	char *wd, *this, *val;
	struct opt_list *po;
	int first = 1;
	char genopt[MAXPATHLEN];

	otab = 0;
	if (ident == NULL) {
		printf("no ident line specified\n");
		exit(1);
	}
	(void) snprintf(fname, sizeof(fname), "../../conf/options");
openit:
	fp = fopen(fname, "r");
	if (fp == 0) {
		return;
	}
next:
	wd = get_word(fp);
	if (wd == (char *)EOF) {
		(void) fclose(fp);
		if (first == 1) {
			first++;
			(void) snprintf(fname, sizeof fname, "../../conf/options.%s", machinename);
			fp = fopen(fname, "r");
			if (fp != 0)
				goto next;
			(void) snprintf(fname, sizeof fname, "options.%s", machinename);
			goto openit;
		}
		if (first == 2) {
			first++;
			(void) snprintf(fname, sizeof fname, "options.%s", raisestr(ident));
			fp = fopen(fname, "r");
			if (fp != 0)
				goto next;
		}
		return;
	}
	if (wd == 0)
		goto next;
	if (wd[0] == '#')
	{
		while (((wd = get_word(fp)) != (char *)EOF) && wd)
		;
		goto next;
	}
	this = ns(wd);
	val = get_word(fp);
	if (val == (char *)EOF)
		return;
	if (val == 0) {
		char *s = ns(this);
		(void) snprintf(genopt, sizeof(genopt), "opt_%s.h", lower(s));
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

static char *
lower(char *str)
{
	char *cp = str;

	while (*str) {
		if (isupper(*str))
			*str = tolower(*str);
		str++;
	}
	return (cp);
}
