/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1997-2005
 *	Herbert Xu <herbert@gondor.apana.org.au>.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
static char sccsid[] = "@(#)expand.c	8.5 (Berkeley) 5/15/95";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

/*
 * Routines to expand arguments to commands.  We have to deal with
 * backquotes, shell variables, and file metacharacters.
 */

#include "shell.h"
#include "main.h"
#include "nodes.h"
#include "eval.h"
#include "expand.h"
#include "syntax.h"
#include "parser.h"
#include "jobs.h"
#include "options.h"
#include "var.h"
#include "input.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"
#include "arith.h"
#include "show.h"
#include "builtins.h"

/*
 * Structure specifying which parts of the string should be searched
 * for IFS characters.
 */

struct ifsregion {
	struct ifsregion *next;	/* next region in list */
	int begoff;		/* offset of start of region */
	int endoff;		/* offset of end of region */
	int inquotes;		/* search for nul bytes only */
};


static char *expdest;			/* output of current string */
static struct nodelist *argbackq;	/* list of back quote expressions */
static struct ifsregion ifsfirst;	/* first struct in list of ifs regions */
static struct ifsregion *ifslastp;	/* last struct in list */
static struct arglist exparg;		/* holds expanded arg list */

static char *argstr(char *, int);
static char *exptilde(char *, int);
static char *expari(char *);
static void expbackq(union node *, int, int);
static int subevalvar(char *, char *, int, int, int, int, int);
static char *evalvar(char *, int);
static int varisset(const char *, int);
static void strtodest(const char *, int, int, int);
static void varvalue(const char *, int, int, int);
static void recordregion(int, int, int);
static void removerecordregions(int);
static void ifsbreakup(char *, struct arglist *);
static void expandmeta(struct strlist *);
static void expmeta(char *, char *);
static void addfname(char *);
static struct strlist *expsort(struct strlist *);
static struct strlist *msort(struct strlist *, int);
static int patmatch(const char *, const char *, int);
static char *cvtnum(int, char *);
static int collate_range_cmp(wchar_t, wchar_t);

static int
collate_range_cmp(wchar_t c1, wchar_t c2)
{
	static wchar_t s1[2], s2[2];

	s1[0] = c1;
	s2[0] = c2;
	return (wcscoll(s1, s2));
}

static char *
stputs_quotes(const char *data, const char *syntax, char *p)
{
	while (*data) {
		CHECKSTRSPACE(2, p);
		if (syntax[(int)*data] == CCTL)
			USTPUTC(CTLESC, p);
		USTPUTC(*data++, p);
	}
	return (p);
}
#define STPUTS_QUOTES(data, syntax, p) p = stputs_quotes((data), syntax, p)

/*
 * Perform expansions on an argument, placing the resulting list of arguments
 * in arglist.  Parameter expansion, command substitution and arithmetic
 * expansion are always performed; additional expansions can be requested
 * via flag (EXP_*).
 * The result is left in the stack string.
 * When arglist is NULL, perform here document expansion.
 *
 * Caution: this function uses global state and is not reentrant.
 * However, a new invocation after an interrupted invocation is safe
 * and will reset the global state for the new call.
 */
void
expandarg(union node *arg, struct arglist *arglist, int flag)
{
	struct strlist *sp;
	char *p;

	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	ifsfirst.next = NULL;
	ifslastp = NULL;
	argstr(arg->narg.text, flag);
	if (arglist == NULL) {
		STACKSTRNUL(expdest);
		return;			/* here document expanded */
	}
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	exparg.lastp = &exparg.list;
	if (flag & EXP_FULL) {
		ifsbreakup(p, &exparg);
		*exparg.lastp = NULL;
		exparg.lastp = &exparg.list;
		expandmeta(exparg.list);
	} else {
		sp = (struct strlist *)stalloc(sizeof (struct strlist));
		sp->text = p;
		*exparg.lastp = sp;
		exparg.lastp = &sp->next;
	}
	while (ifsfirst.next != NULL) {
		struct ifsregion *ifsp;
		INTOFF;
		ifsp = ifsfirst.next->next;
		ckfree(ifsfirst.next);
		ifsfirst.next = ifsp;
		INTON;
	}
	*exparg.lastp = NULL;
	if (exparg.list) {
		*arglist->lastp = exparg.list;
		arglist->lastp = exparg.lastp;
	}
}



/*
 * Perform parameter expansion, command substitution and arithmetic
 * expansion, and tilde expansion if requested via EXP_TILDE/EXP_VARTILDE.
 * Processing ends at a CTLENDVAR or CTLENDARI character as well as '\0'.
 * This is used to expand word in ${var+word} etc.
 * If EXP_FULL or EXP_CASE are set, keep and/or generate CTLESC
 * characters to allow for further processing.
 * If EXP_FULL is set, also preserve CTLQUOTEMARK characters.
 */
static char *
argstr(char *p, int flag)
{
	char c;
	int quotes = flag & (EXP_FULL | EXP_CASE);	/* do CTLESC */
	int firsteq = 1;
	int split_lit;
	int lit_quoted;

	split_lit = flag & EXP_SPLIT_LIT;
	lit_quoted = flag & EXP_LIT_QUOTED;
	flag &= ~(EXP_SPLIT_LIT | EXP_LIT_QUOTED);
	if (*p == '~' && (flag & (EXP_TILDE | EXP_VARTILDE)))
		p = exptilde(p, flag);
	for (;;) {
		CHECKSTRSPACE(2, expdest);
		switch (c = *p++) {
		case '\0':
			return (p - 1);
		case CTLENDVAR:
		case CTLENDARI:
			return (p);
		case CTLQUOTEMARK:
			lit_quoted = 1;
			/* "$@" syntax adherence hack */
			if (p[0] == CTLVAR && p[2] == '@' && p[3] == '=')
				break;
			if ((flag & EXP_FULL) != 0)
				USTPUTC(c, expdest);
			break;
		case CTLQUOTEEND:
			lit_quoted = 0;
			break;
		case CTLESC:
			if (quotes)
				USTPUTC(c, expdest);
			c = *p++;
			USTPUTC(c, expdest);
			if (split_lit && !lit_quoted)
				recordregion(expdest - stackblock() -
				    (quotes ? 2 : 1),
				    expdest - stackblock(), 0);
			break;
		case CTLVAR:
			p = evalvar(p, flag);
			break;
		case CTLBACKQ:
		case CTLBACKQ|CTLQUOTE:
			expbackq(argbackq->n, c & CTLQUOTE, flag);
			argbackq = argbackq->next;
			break;
		case CTLARI:
			p = expari(p);
			break;
		case ':':
		case '=':
			/*
			 * sort of a hack - expand tildes in variable
			 * assignments (after the first '=' and after ':'s).
			 */
			USTPUTC(c, expdest);
			if (split_lit && !lit_quoted)
				recordregion(expdest - stackblock() - 1,
				    expdest - stackblock(), 0);
			if (flag & EXP_VARTILDE && *p == '~' &&
			    (c != '=' || firsteq)) {
				if (c == '=')
					firsteq = 0;
				p = exptilde(p, flag);
			}
			break;
		default:
			USTPUTC(c, expdest);
			if (split_lit && !lit_quoted)
				recordregion(expdest - stackblock() - 1,
				    expdest - stackblock(), 0);
		}
	}
}

/*
 * Perform tilde expansion, placing the result in the stack string and
 * returning the next position in the input string to process.
 */
static char *
exptilde(char *p, int flag)
{
	char c, *startp = p;
	struct passwd *pw;
	char *home;

	for (;;) {
		c = *p;
		switch(c) {
		case CTLESC: /* This means CTL* are always considered quoted. */
		case CTLVAR:
		case CTLBACKQ:
		case CTLBACKQ | CTLQUOTE:
		case CTLARI:
		case CTLENDARI:
		case CTLQUOTEMARK:
			return (startp);
		case ':':
			if ((flag & EXP_VARTILDE) == 0)
				break;
			/* FALLTHROUGH */
		case '\0':
		case '/':
		case CTLENDVAR:
			*p = '\0';
			if (*(startp+1) == '\0') {
				home = lookupvar("HOME");
			} else {
				pw = getpwnam(startp+1);
				home = pw != NULL ? pw->pw_dir : NULL;
			}
			*p = c;
			if (home == NULL || *home == '\0')
				return (startp);
			strtodest(home, flag, VSNORMAL, 1);
			return (p);
		}
		p++;
	}
}


static void
removerecordregions(int endoff)
{
	if (ifslastp == NULL)
		return;

	if (ifsfirst.endoff > endoff) {
		while (ifsfirst.next != NULL) {
			struct ifsregion *ifsp;
			INTOFF;
			ifsp = ifsfirst.next->next;
			ckfree(ifsfirst.next);
			ifsfirst.next = ifsp;
			INTON;
		}
		if (ifsfirst.begoff > endoff)
			ifslastp = NULL;
		else {
			ifslastp = &ifsfirst;
			ifsfirst.endoff = endoff;
		}
		return;
	}

	ifslastp = &ifsfirst;
	while (ifslastp->next && ifslastp->next->begoff < endoff)
		ifslastp=ifslastp->next;
	while (ifslastp->next != NULL) {
		struct ifsregion *ifsp;
		INTOFF;
		ifsp = ifslastp->next->next;
		ckfree(ifslastp->next);
		ifslastp->next = ifsp;
		INTON;
	}
	if (ifslastp->endoff > endoff)
		ifslastp->endoff = endoff;
}

/*
 * Expand arithmetic expression.
 * Note that flag is not required as digits never require CTLESC characters.
 */
static char *
expari(char *p)
{
	char *q, *start;
	arith_t result;
	int begoff;
	int quoted;
	int adj;

	quoted = *p++ == '"';
	begoff = expdest - stackblock();
	p = argstr(p, 0);
	removerecordregions(begoff);
	STPUTC('\0', expdest);
	start = stackblock() + begoff;

	q = grabstackstr(expdest);
	result = arith(start);
	ungrabstackstr(q, expdest);

	start = stackblock() + begoff;
	adj = start - expdest;
	STADJUST(adj, expdest);

	CHECKSTRSPACE((int)(DIGITS(result) + 1), expdest);
	fmtstr(expdest, DIGITS(result), ARITH_FORMAT_STR, result);
	adj = strlen(expdest);
	STADJUST(adj, expdest);
	if (!quoted)
		recordregion(begoff, expdest - stackblock(), 0);
	return p;
}


/*
 * Perform command substitution.
 */
static void
expbackq(union node *cmd, int quoted, int flag)
{
	struct backcmd in;
	int i;
	char buf[128];
	char *p;
	char *dest = expdest;
	struct ifsregion saveifs, *savelastp;
	struct nodelist *saveargbackq;
	char lastc;
	int startloc = dest - stackblock();
	char const *syntax = quoted? DQSYNTAX : BASESYNTAX;
	int quotes = flag & (EXP_FULL | EXP_CASE);
	size_t nnl;

	INTOFF;
	saveifs = ifsfirst;
	savelastp = ifslastp;
	saveargbackq = argbackq;
	p = grabstackstr(dest);
	evalbackcmd(cmd, &in);
	ungrabstackstr(p, dest);
	ifsfirst = saveifs;
	ifslastp = savelastp;
	argbackq = saveargbackq;

	p = in.buf;
	lastc = '\0';
	nnl = 0;
	/* Don't copy trailing newlines */
	for (;;) {
		if (--in.nleft < 0) {
			if (in.fd < 0)
				break;
			while ((i = read(in.fd, buf, sizeof buf)) < 0 && errno == EINTR);
			TRACE(("expbackq: read returns %d\n", i));
			if (i <= 0)
				break;
			p = buf;
			in.nleft = i - 1;
		}
		lastc = *p++;
		if (lastc != '\0') {
			if (lastc == '\n') {
				nnl++;
			} else {
				CHECKSTRSPACE(nnl + 2, dest);
				while (nnl > 0) {
					nnl--;
					USTPUTC('\n', dest);
				}
				if (quotes && syntax[(int)lastc] == CCTL)
					USTPUTC(CTLESC, dest);
				USTPUTC(lastc, dest);
			}
		}
	}

	if (in.fd >= 0)
		close(in.fd);
	if (in.buf)
		ckfree(in.buf);
	if (in.jp)
		exitstatus = waitforjob(in.jp, (int *)NULL);
	if (quoted == 0)
		recordregion(startloc, dest - stackblock(), 0);
	TRACE(("expbackq: size=%td: \"%.*s\"\n",
		((dest - stackblock()) - startloc),
		(int)((dest - stackblock()) - startloc),
		stackblock() + startloc));
	expdest = dest;
	INTON;
}



static void
recordleft(const char *str, const char *loc, char *startp)
{
	int amount;

	amount = ((str - 1) - (loc - startp)) - expdest;
	STADJUST(amount, expdest);
	while (loc != str - 1)
		*startp++ = *loc++;
}

static int
subevalvar(char *p, char *str, int strloc, int subtype, int startloc,
  int varflags, int quotes)
{
	char *startp;
	char *loc = NULL;
	char *q;
	int c = 0;
	struct nodelist *saveargbackq = argbackq;
	int amount;

	argstr(p, (subtype == VSTRIMLEFT || subtype == VSTRIMLEFTMAX ||
	    subtype == VSTRIMRIGHT || subtype == VSTRIMRIGHTMAX ?
	    EXP_CASE : 0) | EXP_TILDE);
	STACKSTRNUL(expdest);
	argbackq = saveargbackq;
	startp = stackblock() + startloc;
	if (str == NULL)
	    str = stackblock() + strloc;

	switch (subtype) {
	case VSASSIGN:
		setvar(str, startp, 0);
		amount = startp - expdest;
		STADJUST(amount, expdest);
		varflags &= ~VSNUL;
		return 1;

	case VSQUESTION:
		if (*p != CTLENDVAR) {
			outfmt(out2, "%s\n", startp);
			error((char *)NULL);
		}
		error("%.*s: parameter %snot set", (int)(p - str - 1),
		      str, (varflags & VSNUL) ? "null or " : "");
		return 0;

	case VSTRIMLEFT:
		for (loc = startp; loc < str; loc++) {
			c = *loc;
			*loc = '\0';
			if (patmatch(str, startp, quotes)) {
				*loc = c;
				recordleft(str, loc, startp);
				return 1;
			}
			*loc = c;
			if (quotes && *loc == CTLESC)
				loc++;
		}
		return 0;

	case VSTRIMLEFTMAX:
		for (loc = str - 1; loc >= startp;) {
			c = *loc;
			*loc = '\0';
			if (patmatch(str, startp, quotes)) {
				*loc = c;
				recordleft(str, loc, startp);
				return 1;
			}
			*loc = c;
			loc--;
			if (quotes && loc > startp && *(loc - 1) == CTLESC) {
				for (q = startp; q < loc; q++)
					if (*q == CTLESC)
						q++;
				if (q > loc)
					loc--;
			}
		}
		return 0;

	case VSTRIMRIGHT:
		for (loc = str - 1; loc >= startp;) {
			if (patmatch(str, loc, quotes)) {
				amount = loc - expdest;
				STADJUST(amount, expdest);
				return 1;
			}
			loc--;
			if (quotes && loc > startp && *(loc - 1) == CTLESC) {
				for (q = startp; q < loc; q++)
					if (*q == CTLESC)
						q++;
				if (q > loc)
					loc--;
			}
		}
		return 0;

	case VSTRIMRIGHTMAX:
		for (loc = startp; loc < str - 1; loc++) {
			if (patmatch(str, loc, quotes)) {
				amount = loc - expdest;
				STADJUST(amount, expdest);
				return 1;
			}
			if (quotes && *loc == CTLESC)
				loc++;
		}
		return 0;


	default:
		abort();
	}
}


/*
 * Expand a variable, and return a pointer to the next character in the
 * input string.
 */

static char *
evalvar(char *p, int flag)
{
	int subtype;
	int varflags;
	char *var;
	const char *val;
	int patloc;
	int c;
	int set;
	int special;
	int startloc;
	int varlen;
	int varlenb;
	int easy;
	int quotes = flag & (EXP_FULL | EXP_CASE);
	int record = 0;

	varflags = (unsigned char)*p++;
	subtype = varflags & VSTYPE;
	var = p;
	special = 0;
	if (! is_name(*p))
		special = 1;
	p = strchr(p, '=') + 1;
again: /* jump here after setting a variable with ${var=text} */
	if (varflags & VSLINENO) {
		set = 1;
		special = 1;
		val = NULL;
	} else if (special) {
		set = varisset(var, varflags & VSNUL);
		val = NULL;
	} else {
		val = bltinlookup(var, 1);
		if (val == NULL || ((varflags & VSNUL) && val[0] == '\0')) {
			val = NULL;
			set = 0;
		} else
			set = 1;
	}
	varlen = 0;
	startloc = expdest - stackblock();
	if (!set && uflag && *var != '@' && *var != '*') {
		switch (subtype) {
		case VSNORMAL:
		case VSTRIMLEFT:
		case VSTRIMLEFTMAX:
		case VSTRIMRIGHT:
		case VSTRIMRIGHTMAX:
		case VSLENGTH:
			error("%.*s: parameter not set", (int)(p - var - 1),
			    var);
		}
	}
	if (set && subtype != VSPLUS) {
		/* insert the value of the variable */
		if (special) {
			if (varflags & VSLINENO)
				STPUTBIN(var, p - var - 1, expdest);
			else
				varvalue(var, varflags & VSQUOTE, subtype, flag);
			if (subtype == VSLENGTH) {
				varlenb = expdest - stackblock() - startloc;
				varlen = varlenb;
				if (localeisutf8) {
					val = stackblock() + startloc;
					for (;val != expdest; val++)
						if ((*val & 0xC0) == 0x80)
							varlen--;
				}
				STADJUST(-varlenb, expdest);
			}
		} else {
			if (subtype == VSLENGTH) {
				for (;*val; val++)
					if (!localeisutf8 ||
					    (*val & 0xC0) != 0x80)
						varlen++;
			}
			else
				strtodest(val, flag, subtype,
				    varflags & VSQUOTE);
		}
	}

	if (subtype == VSPLUS)
		set = ! set;

	easy = ((varflags & VSQUOTE) == 0 ||
		(*var == '@' && shellparam.nparam != 1));


	switch (subtype) {
	case VSLENGTH:
		expdest = cvtnum(varlen, expdest);
		record = 1;
		break;

	case VSNORMAL:
		record = easy;
		break;

	case VSPLUS:
	case VSMINUS:
		if (!set) {
			argstr(p, flag | (flag & EXP_FULL ? EXP_SPLIT_LIT : 0) |
			    (varflags & VSQUOTE ? EXP_LIT_QUOTED : 0));
			break;
		}
		record = easy;
		break;

	case VSTRIMLEFT:
	case VSTRIMLEFTMAX:
	case VSTRIMRIGHT:
	case VSTRIMRIGHTMAX:
		if (!set)
			break;
		/*
		 * Terminate the string and start recording the pattern
		 * right after it
		 */
		STPUTC('\0', expdest);
		patloc = expdest - stackblock();
		if (subevalvar(p, NULL, patloc, subtype,
		    startloc, varflags, quotes) == 0) {
			int amount = (expdest - stackblock() - patloc) + 1;
			STADJUST(-amount, expdest);
		}
		/* Remove any recorded regions beyond start of variable */
		removerecordregions(startloc);
		record = 1;
		break;

	case VSASSIGN:
	case VSQUESTION:
		if (!set) {
			if (subevalvar(p, var, 0, subtype, startloc, varflags,
			    quotes)) {
				varflags &= ~VSNUL;
				/*
				 * Remove any recorded regions beyond
				 * start of variable
				 */
				removerecordregions(startloc);
				goto again;
			}
			break;
		}
		record = easy;
		break;

	case VSERROR:
		c = p - var - 1;
		error("${%.*s%s}: Bad substitution", c, var,
		    (c > 0 && *p != CTLENDVAR) ? "..." : "");

	default:
		abort();
	}

	if (record)
		recordregion(startloc, expdest - stackblock(),
		    varflags & VSQUOTE || (ifsset() && ifsval()[0] == '\0' &&
		    (*var == '@' || *var == '*')));

	if (subtype != VSNORMAL) {	/* skip to end of alternative */
		int nesting = 1;
		for (;;) {
			if ((c = *p++) == CTLESC)
				p++;
			else if (c == CTLBACKQ || c == (CTLBACKQ|CTLQUOTE)) {
				if (set)
					argbackq = argbackq->next;
			} else if (c == CTLVAR) {
				if ((*p++ & VSTYPE) != VSNORMAL)
					nesting++;
			} else if (c == CTLENDVAR) {
				if (--nesting == 0)
					break;
			}
		}
	}
	return p;
}



/*
 * Test whether a specialized variable is set.
 */

static int
varisset(const char *name, int nulok)
{

	if (*name == '!')
		return backgndpidset();
	else if (*name == '@' || *name == '*') {
		if (*shellparam.p == NULL)
			return 0;

		if (nulok) {
			char **av;

			for (av = shellparam.p; *av; av++)
				if (**av != '\0')
					return 1;
			return 0;
		}
	} else if (is_digit(*name)) {
		char *ap;
		long num;

		errno = 0;
		num = strtol(name, NULL, 10);
		if (errno != 0 || num > shellparam.nparam)
			return 0;

		if (num == 0)
			ap = arg0;
		else
			ap = shellparam.p[num - 1];

		if (nulok && (ap == NULL || *ap == '\0'))
			return 0;
	}
	return 1;
}

static void
strtodest(const char *p, int flag, int subtype, int quoted)
{
	if (flag & (EXP_FULL | EXP_CASE) && subtype != VSLENGTH)
		STPUTS_QUOTES(p, quoted ? DQSYNTAX : BASESYNTAX, expdest);
	else
		STPUTS(p, expdest);
}

/*
 * Add the value of a specialized variable to the stack string.
 */

static void
varvalue(const char *name, int quoted, int subtype, int flag)
{
	int num;
	char *p;
	int i;
	char sep[2];
	char **ap;

	switch (*name) {
	case '$':
		num = rootpid;
		break;
	case '?':
		num = oexitstatus;
		break;
	case '#':
		num = shellparam.nparam;
		break;
	case '!':
		num = backgndpidval();
		break;
	case '-':
		for (i = 0 ; i < NOPTS ; i++) {
			if (optlist[i].val)
				STPUTC(optlist[i].letter, expdest);
		}
		return;
	case '@':
		if (flag & EXP_FULL && quoted) {
			for (ap = shellparam.p ; (p = *ap++) != NULL ; ) {
				strtodest(p, flag, subtype, quoted);
				if (*ap)
					STPUTC('\0', expdest);
			}
			return;
		}
		/* FALLTHROUGH */
	case '*':
		if (ifsset())
			sep[0] = ifsval()[0];
		else
			sep[0] = ' ';
		sep[1] = '\0';
		for (ap = shellparam.p ; (p = *ap++) != NULL ; ) {
			strtodest(p, flag, subtype, quoted);
			if (!*ap)
				break;
			if (sep[0])
				strtodest(sep, flag, subtype, quoted);
			else if (flag & EXP_FULL && !quoted && **ap != '\0')
				STPUTC('\0', expdest);
		}
		return;
	default:
		if (is_digit(*name)) {
			num = atoi(name);
			if (num == 0)
				p = arg0;
			else if (num > 0 && num <= shellparam.nparam)
				p = shellparam.p[num - 1];
			else
				return;
			strtodest(p, flag, subtype, quoted);
		}
		return;
	}
	expdest = cvtnum(num, expdest);
}



/*
 * Record the fact that we have to scan this region of the
 * string for IFS characters.
 */

static void
recordregion(int start, int end, int inquotes)
{
	struct ifsregion *ifsp;

	INTOFF;
	if (ifslastp == NULL) {
		ifsp = &ifsfirst;
	} else {
		if (ifslastp->endoff == start
		    && ifslastp->inquotes == inquotes) {
			/* extend previous area */
			ifslastp->endoff = end;
			INTON;
			return;
		}
		ifsp = (struct ifsregion *)ckmalloc(sizeof (struct ifsregion));
		ifslastp->next = ifsp;
	}
	ifslastp = ifsp;
	ifslastp->next = NULL;
	ifslastp->begoff = start;
	ifslastp->endoff = end;
	ifslastp->inquotes = inquotes;
	INTON;
}



/*
 * Break the argument string into pieces based upon IFS and add the
 * strings to the argument list.  The regions of the string to be
 * searched for IFS characters have been stored by recordregion.
 * CTLESC characters are preserved but have little effect in this pass
 * other than escaping CTL* characters.  In particular, they do not escape
 * IFS characters: that should be done with the ifsregion mechanism.
 * CTLQUOTEMARK characters are used to preserve empty quoted strings.
 * This pass treats them as a regular character, making the string non-empty.
 * Later, they are removed along with the other CTL* characters.
 */
static void
ifsbreakup(char *string, struct arglist *arglist)
{
	struct ifsregion *ifsp;
	struct strlist *sp;
	char *start;
	char *p;
	char *q;
	const char *ifs;
	const char *ifsspc;
	int had_param_ch = 0;

	start = string;

	if (ifslastp == NULL) {
		/* Return entire argument, IFS doesn't apply to any of it */
		sp = (struct strlist *)stalloc(sizeof *sp);
		sp->text = start;
		*arglist->lastp = sp;
		arglist->lastp = &sp->next;
		return;
	}

	ifs = ifsset() ? ifsval() : " \t\n";

	for (ifsp = &ifsfirst; ifsp != NULL; ifsp = ifsp->next) {
		p = string + ifsp->begoff;
		while (p < string + ifsp->endoff) {
			q = p;
			if (*p == CTLESC)
				p++;
			if (ifsp->inquotes) {
				/* Only NULs (should be from "$@") end args */
				had_param_ch = 1;
				if (*p != 0) {
					p++;
					continue;
				}
				ifsspc = NULL;
			} else {
				if (!strchr(ifs, *p)) {
					had_param_ch = 1;
					p++;
					continue;
				}
				ifsspc = strchr(" \t\n", *p);

				/* Ignore IFS whitespace at start */
				if (q == start && ifsspc != NULL) {
					p++;
					start = p;
					continue;
				}
				had_param_ch = 0;
			}

			/* Save this argument... */
			*q = '\0';
			sp = (struct strlist *)stalloc(sizeof *sp);
			sp->text = start;
			*arglist->lastp = sp;
			arglist->lastp = &sp->next;
			p++;

			if (ifsspc != NULL) {
				/* Ignore further trailing IFS whitespace */
				for (; p < string + ifsp->endoff; p++) {
					q = p;
					if (*p == CTLESC)
						p++;
					if (strchr(ifs, *p) == NULL) {
						p = q;
						break;
					}
					if (strchr(" \t\n", *p) == NULL) {
						p++;
						break;
					}
				}
			}
			start = p;
		}
	}

	/*
	 * Save anything left as an argument.
	 * Traditionally we have treated 'IFS=':'; set -- x$IFS' as
	 * generating 2 arguments, the second of which is empty.
	 * Some recent clarification of the Posix spec say that it
	 * should only generate one....
	 */
	if (had_param_ch || *start != 0) {
		sp = (struct strlist *)stalloc(sizeof *sp);
		sp->text = start;
		*arglist->lastp = sp;
		arglist->lastp = &sp->next;
	}
}


static char expdir[PATH_MAX];
#define expdir_end (expdir + sizeof(expdir))

/*
 * Perform pathname generation and remove control characters.
 * At this point, the only control characters should be CTLESC and CTLQUOTEMARK.
 * The results are stored in the list exparg.
 */
static void
expandmeta(struct strlist *str)
{
	char *p;
	struct strlist **savelastp;
	struct strlist *sp;
	char c;

	while (str) {
		savelastp = exparg.lastp;
		if (!fflag) {
			p = str->text;
			for (; (c = *p) != '\0'; p++) {
				/* fast check for meta chars */
				if (c == '*' || c == '?' || c == '[') {
					INTOFF;
					expmeta(expdir, str->text);
					INTON;
					break;
				}
			}
		}
		if (exparg.lastp == savelastp) {
			/*
			 * no matches
			 */
			*exparg.lastp = str;
			rmescapes(str->text);
			exparg.lastp = &str->next;
		} else {
			*exparg.lastp = NULL;
			*savelastp = sp = expsort(*savelastp);
			while (sp->next != NULL)
				sp = sp->next;
			exparg.lastp = &sp->next;
		}
		str = str->next;
	}
}


/*
 * Do metacharacter (i.e. *, ?, [...]) expansion.
 */

static void
expmeta(char *enddir, char *name)
{
	const char *p;
	const char *q;
	const char *start;
	char *endname;
	int metaflag;
	struct stat statb;
	DIR *dirp;
	struct dirent *dp;
	int atend;
	int matchdot;
	int esc;
	int namlen;

	metaflag = 0;
	start = name;
	for (p = name; esc = 0, *p; p += esc + 1) {
		if (*p == '*' || *p == '?')
			metaflag = 1;
		else if (*p == '[') {
			q = p + 1;
			if (*q == '!' || *q == '^')
				q++;
			for (;;) {
				while (*q == CTLQUOTEMARK)
					q++;
				if (*q == CTLESC)
					q++;
				if (*q == '/' || *q == '\0')
					break;
				if (*++q == ']') {
					metaflag = 1;
					break;
				}
			}
		} else if (*p == '\0')
			break;
		else if (*p == CTLQUOTEMARK)
			continue;
		else {
			if (*p == CTLESC)
				esc++;
			if (p[esc] == '/') {
				if (metaflag)
					break;
				start = p + esc + 1;
			}
		}
	}
	if (metaflag == 0) {	/* we've reached the end of the file name */
		if (enddir != expdir)
			metaflag++;
		for (p = name ; ; p++) {
			if (*p == CTLQUOTEMARK)
				continue;
			if (*p == CTLESC)
				p++;
			*enddir++ = *p;
			if (*p == '\0')
				break;
			if (enddir == expdir_end)
				return;
		}
		if (metaflag == 0 || lstat(expdir, &statb) >= 0)
			addfname(expdir);
		return;
	}
	endname = name + (p - name);
	if (start != name) {
		p = name;
		while (p < start) {
			while (*p == CTLQUOTEMARK)
				p++;
			if (*p == CTLESC)
				p++;
			*enddir++ = *p++;
			if (enddir == expdir_end)
				return;
		}
	}
	if (enddir == expdir) {
		p = ".";
	} else if (enddir == expdir + 1 && *expdir == '/') {
		p = "/";
	} else {
		p = expdir;
		enddir[-1] = '\0';
	}
	if ((dirp = opendir(p)) == NULL)
		return;
	if (enddir != expdir)
		enddir[-1] = '/';
	if (*endname == 0) {
		atend = 1;
	} else {
		atend = 0;
		*endname = '\0';
		endname += esc + 1;
	}
	matchdot = 0;
	p = start;
	while (*p == CTLQUOTEMARK)
		p++;
	if (*p == CTLESC)
		p++;
	if (*p == '.')
		matchdot++;
	while (! int_pending() && (dp = readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.' && ! matchdot)
			continue;
		if (patmatch(start, dp->d_name, 0)) {
			namlen = dp->d_namlen;
			if (enddir + namlen + 1 > expdir_end)
				continue;
			memcpy(enddir, dp->d_name, namlen + 1);
			if (atend)
				addfname(expdir);
			else {
				if (dp->d_type != DT_UNKNOWN &&
				    dp->d_type != DT_DIR &&
				    dp->d_type != DT_LNK)
					continue;
				if (enddir + namlen + 2 > expdir_end)
					continue;
				enddir[namlen] = '/';
				enddir[namlen + 1] = '\0';
				expmeta(enddir + namlen + 1, endname);
			}
		}
	}
	closedir(dirp);
	if (! atend)
		endname[-esc - 1] = esc ? CTLESC : '/';
}


/*
 * Add a file name to the list.
 */

static void
addfname(char *name)
{
	char *p;
	struct strlist *sp;

	p = stsavestr(name);
	sp = (struct strlist *)stalloc(sizeof *sp);
	sp->text = p;
	*exparg.lastp = sp;
	exparg.lastp = &sp->next;
}


/*
 * Sort the results of file name expansion.  It calculates the number of
 * strings to sort and then calls msort (short for merge sort) to do the
 * work.
 */

static struct strlist *
expsort(struct strlist *str)
{
	int len;
	struct strlist *sp;

	len = 0;
	for (sp = str ; sp ; sp = sp->next)
		len++;
	return msort(str, len);
}


static struct strlist *
msort(struct strlist *list, int len)
{
	struct strlist *p, *q = NULL;
	struct strlist **lpp;
	int half;
	int n;

	if (len <= 1)
		return list;
	half = len >> 1;
	p = list;
	for (n = half ; --n >= 0 ; ) {
		q = p;
		p = p->next;
	}
	q->next = NULL;			/* terminate first half of list */
	q = msort(list, half);		/* sort first half of list */
	p = msort(p, len - half);		/* sort second half */
	lpp = &list;
	for (;;) {
		if (strcmp(p->text, q->text) < 0) {
			*lpp = p;
			lpp = &p->next;
			if ((p = *lpp) == NULL) {
				*lpp = q;
				break;
			}
		} else {
			*lpp = q;
			lpp = &q->next;
			if ((q = *lpp) == NULL) {
				*lpp = p;
				break;
			}
		}
	}
	return list;
}



static wchar_t
get_wc(const char **p)
{
	wchar_t c;
	int chrlen;

	chrlen = mbtowc(&c, *p, 4);
	if (chrlen == 0)
		return 0;
	else if (chrlen == -1)
		c = 0;
	else
		*p += chrlen;
	return c;
}


/*
 * See if a character matches a character class, starting at the first colon
 * of "[:class:]".
 * If a valid character class is recognized, a pointer to the next character
 * after the final closing bracket is stored into *end, otherwise a null
 * pointer is stored into *end.
 */
static int
match_charclass(const char *p, wchar_t chr, const char **end)
{
	char name[20];
	const char *nameend;
	wctype_t cclass;

	*end = NULL;
	p++;
	nameend = strstr(p, ":]");
	if (nameend == NULL || (size_t)(nameend - p) >= sizeof(name) ||
	    nameend == p)
		return 0;
	memcpy(name, p, nameend - p);
	name[nameend - p] = '\0';
	*end = nameend + 2;
	cclass = wctype(name);
	/* An unknown class matches nothing but is valid nevertheless. */
	if (cclass == 0)
		return 0;
	return iswctype(chr, cclass);
}


/*
 * Returns true if the pattern matches the string.
 */

static int
patmatch(const char *pattern, const char *string, int squoted)
{
	const char *p, *q, *end;
	const char *bt_p, *bt_q;
	char c;
	wchar_t wc, wc2;

	p = pattern;
	q = string;
	bt_p = NULL;
	bt_q = NULL;
	for (;;) {
		switch (c = *p++) {
		case '\0':
			if (*q != '\0')
				goto backtrack;
			return 1;
		case CTLESC:
			if (squoted && *q == CTLESC)
				q++;
			if (*q++ != *p++)
				goto backtrack;
			break;
		case CTLQUOTEMARK:
			continue;
		case '?':
			if (squoted && *q == CTLESC)
				q++;
			if (*q == '\0')
				return 0;
			if (localeisutf8) {
				wc = get_wc(&q);
				/*
				 * A '?' does not match invalid UTF-8 but a
				 * '*' does, so backtrack.
				 */
				if (wc == 0)
					goto backtrack;
			} else
				wc = (unsigned char)*q++;
			break;
		case '*':
			c = *p;
			while (c == CTLQUOTEMARK || c == '*')
				c = *++p;
			/*
			 * If the pattern ends here, we know the string
			 * matches without needing to look at the rest of it.
			 */
			if (c == '\0')
				return 1;
			/*
			 * First try the shortest match for the '*' that
			 * could work. We can forget any earlier '*' since
			 * there is no way having it match more characters
			 * can help us, given that we are already here.
			 */
			bt_p = p;
			bt_q = q;
			break;
		case '[': {
			const char *savep, *saveq;
			int invert, found;
			wchar_t chr;

			savep = p, saveq = q;
			invert = 0;
			if (*p == '!' || *p == '^') {
				invert++;
				p++;
			}
			found = 0;
			if (squoted && *q == CTLESC)
				q++;
			if (*q == '\0')
				return 0;
			if (localeisutf8) {
				chr = get_wc(&q);
				if (chr == 0)
					goto backtrack;
			} else
				chr = (unsigned char)*q++;
			c = *p++;
			do {
				if (c == '\0') {
					p = savep, q = saveq;
					c = '[';
					goto dft;
				}
				if (c == CTLQUOTEMARK)
					continue;
				if (c == '[' && *p == ':') {
					found |= match_charclass(p, chr, &end);
					if (end != NULL)
						p = end;
				}
				if (c == CTLESC)
					c = *p++;
				if (localeisutf8 && c & 0x80) {
					p--;
					wc = get_wc(&p);
					if (wc == 0) /* bad utf-8 */
						return 0;
				} else
					wc = (unsigned char)c;
				if (*p == '-' && p[1] != ']') {
					p++;
					while (*p == CTLQUOTEMARK)
						p++;
					if (*p == CTLESC)
						p++;
					if (localeisutf8) {
						wc2 = get_wc(&p);
						if (wc2 == 0) /* bad utf-8 */
							return 0;
					} else
						wc2 = (unsigned char)*p++;
					if (   collate_range_cmp(chr, wc) >= 0
					    && collate_range_cmp(chr, wc2) <= 0
					   )
						found = 1;
				} else {
					if (chr == wc)
						found = 1;
				}
			} while ((c = *p++) != ']');
			if (found == invert)
				goto backtrack;
			break;
		}
dft:	        default:
			if (squoted && *q == CTLESC)
				q++;
			if (*q == '\0')
				return 0;
			if (*q++ == c)
				break;
backtrack:
			/*
			 * If we have a mismatch (other than hitting the end
			 * of the string), go back to the last '*' seen and
			 * have it match one additional character.
			 */
			if (bt_p == NULL)
				return 0;
			if (squoted && *bt_q == CTLESC)
				bt_q++;
			if (*bt_q == '\0')
				return 0;
			bt_q++;
			p = bt_p;
			q = bt_q;
			break;
		}
	}
}



/*
 * Remove any CTLESC and CTLQUOTEMARK characters from a string.
 */

void
rmescapes(char *str)
{
	char *p, *q;

	p = str;
	while (*p != CTLESC && *p != CTLQUOTEMARK && *p != CTLQUOTEEND) {
		if (*p++ == '\0')
			return;
	}
	q = p;
	while (*p) {
		if (*p == CTLQUOTEMARK || *p == CTLQUOTEEND) {
			p++;
			continue;
		}
		if (*p == CTLESC)
			p++;
		*q++ = *p++;
	}
	*q = '\0';
}



/*
 * See if a pattern matches in a case statement.
 */

int
casematch(union node *pattern, const char *val)
{
	struct stackmark smark;
	int result;
	char *p;

	setstackmark(&smark);
	argbackq = pattern->narg.backquote;
	STARTSTACKSTR(expdest);
	ifslastp = NULL;
	argstr(pattern->narg.text, EXP_TILDE | EXP_CASE);
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	result = patmatch(p, val, 0);
	popstackmark(&smark);
	return result;
}

/*
 * Our own itoa().
 */

static char *
cvtnum(int num, char *buf)
{
	char temp[32];
	int neg = num < 0;
	char *p = temp + 31;

	temp[31] = '\0';

	do {
		*--p = num % 10 + '0';
	} while ((num /= 10) != 0);

	if (neg)
		*--p = '-';

	STPUTS(p, buf);
	return buf;
}

/*
 * Do most of the work for wordexp(3).
 */

int
wordexpcmd(int argc, char **argv)
{
	size_t len;
	int i;

	out1fmt("%08x", argc - 1);
	for (i = 1, len = 0; i < argc; i++)
		len += strlen(argv[i]);
	out1fmt("%08x", (int)len);
	for (i = 1; i < argc; i++)
		outbin(argv[i], strlen(argv[i]) + 1, out1);
        return (0);
}
