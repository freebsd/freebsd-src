/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	$Id: expand.c,v 1.16 1997/02/22 13:58:26 peter Exp $
 */

#ifndef lint
static char const sccsid[] = "@(#)expand.c	8.5 (Berkeley) 5/15/95";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <limits.h>

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

/*
 * Structure specifying which parts of the string should be searched
 * for IFS characters.
 */

struct ifsregion {
	struct ifsregion *next;	/* next region in list */
	int begoff;		/* offset of start of region */
	int endoff;		/* offset of end of region */
	int nulonly;		/* search for nul bytes only */
};


char *expdest;			/* output of current string */
struct nodelist *argbackq;	/* list of back quote expressions */
struct ifsregion ifsfirst;	/* first struct in list of ifs regions */
struct ifsregion *ifslastp;	/* last struct in list */
struct arglist exparg;		/* holds expanded arg list */

STATIC void argstr __P((char *, int));
STATIC char *exptilde __P((char *, int));
STATIC void expbackq __P((union node *, int, int));
STATIC int subevalvar __P((char *, char *, int, int, int, int));
STATIC char *evalvar __P((char *, int));
STATIC int varisset __P((char *, int));
STATIC void varvalue __P((char *, int, int));
STATIC void recordregion __P((int, int, int));
STATIC void ifsbreakup __P((char *, struct arglist *));
STATIC void expandmeta __P((struct strlist *, int));
STATIC void expmeta __P((char *, char *));
STATIC void addfname __P((char *));
STATIC struct strlist *expsort __P((struct strlist *));
STATIC struct strlist *msort __P((struct strlist *, int));
STATIC int pmatch __P((char *, char *));
STATIC char *cvtnum __P((int, char *));
STATIC int collate_range_cmp __P((int, int));

STATIC int collate_range_cmp (c1, c2)
	int c1, c2;
{
	static char s1[2], s2[2];
	int ret;

	c1 &= UCHAR_MAX;
	c2 &= UCHAR_MAX;
	if (c1 == c2)
		return (0);
	s1[0] = c1;
	s2[0] = c2;
	if ((ret = strcoll(s1, s2)) != 0)
		return (ret);
	return (c1 - c2);
}

/*
 * Expand shell variables and backquotes inside a here document.
 */

void
expandhere(arg, fd)
	union node *arg;	/* the document */
	int fd;			/* where to write the expanded version */
	{
	herefd = fd;
	expandarg(arg, (struct arglist *)NULL, 0);
	xwrite(fd, stackblock(), expdest - stackblock());
}


/*
 * Perform variable substitution and command substitution on an argument,
 * placing the resulting list of arguments in arglist.  If EXP_FULL is true,
 * perform splitting and file name expansion.  When arglist is NULL, perform
 * here document expansion.
 */

void
expandarg(arg, arglist, flag)
	union node *arg;
	struct arglist *arglist;
	int flag;
{
	struct strlist *sp;
	char *p;

	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	ifsfirst.next = NULL;
	ifslastp = NULL;
	argstr(arg->narg.text, flag);
	if (arglist == NULL) {
		return;			/* here document expanded */
	}
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	exparg.lastp = &exparg.list;
	/*
	 * TODO - EXP_REDIR
	 */
	if (flag & EXP_FULL) {
		ifsbreakup(p, &exparg);
		*exparg.lastp = NULL;
		exparg.lastp = &exparg.list;
		expandmeta(exparg.list, flag);
	} else {
		if (flag & EXP_REDIR) /*XXX - for now, just remove escapes */
			rmescapes(p);
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
 * Perform variable and command substitution.  If EXP_FULL is set, output CTLESC
 * characters to allow for further processing.  Otherwise treat
 * $@ like $* since no splitting will be performed.
 */

STATIC void
argstr(p, flag)
	char *p;
	int flag;
{
	char c;
	int quotes = flag & (EXP_FULL | EXP_CASE);	/* do CTLESC */
	int firsteq = 1;

	if (*p == '~' && (flag & (EXP_TILDE | EXP_VARTILDE)))
		p = exptilde(p, flag);
	for (;;) {
		switch (c = *p++) {
		case '\0':
		case CTLENDVAR: /* ??? */
			goto breakloop;
		case CTLESC:
			if (quotes)
				STPUTC(c, expdest);
			c = *p++;
			STPUTC(c, expdest);
			break;
		case CTLVAR:
			p = evalvar(p, flag);
			break;
		case CTLBACKQ:
		case CTLBACKQ|CTLQUOTE:
			expbackq(argbackq->n, c & CTLQUOTE, flag);
			argbackq = argbackq->next;
			break;
		case CTLENDARI:
			expari(flag);
			break;
		case ':':
		case '=':
			/*
			 * sort of a hack - expand tildes in variable
			 * assignments (after the first '=' and after ':'s).
			 */
			STPUTC(c, expdest);
			if (flag & EXP_VARTILDE && *p == '~') {
				if (c == '=') {
					if (firsteq)
						firsteq = 0;
					else
						break;
				}
				p = exptilde(p, flag);
			}
			break;
		default:
			STPUTC(c, expdest);
		}
	}
breakloop:;
}

STATIC char *
exptilde(p, flag)
	char *p;
	int flag;
{
	char c, *startp = p;
	struct passwd *pw;
	char *home;
	int quotes = flag & (EXP_FULL | EXP_CASE);

	while ((c = *p) != '\0') {
		switch(c) {
		case CTLESC:
			return (startp);
		case ':':
			if (flag & EXP_VARTILDE)
				goto done;
			break;
		case '/':
			goto done;
		}
		p++;
	}
done:
	*p = '\0';
	if (*(startp+1) == '\0') {
		if ((home = lookupvar("HOME")) == NULL)
			goto lose;
	} else {
		if ((pw = getpwnam(startp+1)) == NULL)
			goto lose;
		home = pw->pw_dir;
	}
	if (*home == '\0')
		goto lose;
	*p = c;
	while ((c = *home++) != '\0') {
		if (quotes && SQSYNTAX[c] == CCTL)
			STPUTC(CTLESC, expdest);
		STPUTC(c, expdest);
	}
	return (p);
lose:
	*p = c;
	return (startp);
}


/*
 * Expand arithmetic expression.  Backup to start of expression,
 * evaluate, place result in (backed up) result, adjust string position.
 */
void
expari(flag)
	int flag;
{
	char *p, *start;
	int result;
	int quotes = flag & (EXP_FULL | EXP_CASE);

	while (ifsfirst.next != NULL) {
		struct ifsregion *ifsp;
		INTOFF;
		ifsp = ifsfirst.next->next;
		ckfree(ifsfirst.next);
		ifsfirst.next = ifsp;
		INTON;
	}
	ifslastp = NULL;

	/*
	 * This routine is slightly over-compilcated for
	 * efficiency.  First we make sure there is
	 * enough space for the result, which may be bigger
	 * than the expression if we add exponentation.  Next we
	 * scan backwards looking for the start of arithmetic.  If the
	 * next previous character is a CTLESC character, then we
	 * have to rescan starting from the beginning since CTLESC
	 * characters have to be processed left to right.
	 */
#if INT_MAX / 1000000000 >= 10 || INT_MIN / 1000000000 <= -10
#error "integers with more than 10 digits are not supported"
#endif
	CHECKSTRSPACE(12 - 2, expdest);
	USTPUTC('\0', expdest);
	start = stackblock();
	p = expdest;
	while (*p != CTLARI && p >= start)
		--p;
	if (*p != CTLARI)
		error("missing CTLARI (shouldn't happen)");
	if (p > start && *(p-1) == CTLESC)
		for (p = start; *p != CTLARI; p++)
			if (*p == CTLESC)
				p++;
	if (quotes)
		rmescapes(p+1);
	result = arith(p+1);
	fmtstr(p, 12, "%d", result);
	while (*p++)
		;
	result = expdest - p + 1;
	STADJUST(-result, expdest);
}


/*
 * Expand stuff in backwards quotes.
 */

STATIC void
expbackq(cmd, quoted, flag)
	union node *cmd;
	int quoted;
	int flag;
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
	int saveherefd;
	int quotes = flag & (EXP_FULL | EXP_CASE);

	INTOFF;
	saveifs = ifsfirst;
	savelastp = ifslastp;
	saveargbackq = argbackq;
	saveherefd = herefd;
	herefd = -1;
	p = grabstackstr(dest);
	evalbackcmd(cmd, &in);
	ungrabstackstr(p, dest);
	ifsfirst = saveifs;
	ifslastp = savelastp;
	argbackq = saveargbackq;
	herefd = saveherefd;

	p = in.buf;
	lastc = '\0';
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
			if (quotes && syntax[lastc] == CCTL)
				STPUTC(CTLESC, dest);
			STPUTC(lastc, dest);
		}
	}

	/* Eat all trailing newlines */
	for (p--; lastc == '\n'; lastc = *--p)
		STUNPUTC(dest);

	if (in.fd >= 0)
		close(in.fd);
	if (in.buf)
		ckfree(in.buf);
	if (in.jp)
		exitstatus = waitforjob(in.jp);
	if (quoted == 0)
		recordregion(startloc, dest - stackblock(), 0);
	TRACE(("evalbackq: size=%d: \"%.*s\"\n",
		(dest - stackblock()) - startloc,
		(dest - stackblock()) - startloc,
		stackblock() + startloc));
	expdest = dest;
	INTON;
}



STATIC int
subevalvar(p, str, strloc, subtype, startloc, varflags)
	char *p;
	char *str;
	int strloc;
	int subtype;
	int startloc;
	int varflags;
{
	char *startp;
	char *loc = NULL;
	int c = 0;
	int saveherefd = herefd;
	struct nodelist *saveargbackq = argbackq;
	int amount;

	herefd = -1;
	argstr(p, 0);
	STACKSTRNUL(expdest);
	herefd = saveherefd;
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
		if (c != 0)
			*loc = c;
		return 1;

	case VSQUESTION:
		if (*p != CTLENDVAR) {
			outfmt(&errout, "%s\n", startp);
			error((char *)NULL);
		}
		error("%.*s: parameter %snot set", p - str - 1,
		      str, (varflags & VSNUL) ? "null or "
					      : nullstr);
		return 0;

	case VSTRIMLEFT:
		for (loc = startp; loc < str; loc++) {
			c = *loc;
			*loc = '\0';
			if (patmatch(str, startp)) {
				*loc = c;
				goto recordleft;
			}
			*loc = c;
		}
		return 0;

	case VSTRIMLEFTMAX:
		for (loc = str - 1; loc >= startp; loc--) {
			c = *loc;
			*loc = '\0';
			if (patmatch(str, startp)) {
				*loc = c;
				goto recordleft;
			}
			*loc = c;
		}
		return 0;

	case VSTRIMRIGHT:
		for (loc = str - 1; loc >= startp; loc--) {
			if (patmatch(str, loc)) {
				amount = loc - expdest;
				STADJUST(amount, expdest);
				return 1;
			}
		}
		return 0;

	case VSTRIMRIGHTMAX:
		for (loc = startp; loc < str - 1; loc++) {
			if (patmatch(str, loc)) {
				amount = loc - expdest;
				STADJUST(amount, expdest);
				return 1;
			}
		}
		return 0;


	default:
		abort();
	}

recordleft:
	amount = ((str - 1) - (loc - startp)) - expdest;
	STADJUST(amount, expdest);
	while (loc != str - 1)
		*startp++ = *loc++;
	return 1;
}


/*
 * Expand a variable, and return a pointer to the next character in the
 * input string.
 */

STATIC char *
evalvar(p, flag)
	char *p;
	int flag;
{
	int subtype;
	int varflags;
	char *var;
	char *val;
	char *pat;
	int c;
	int set;
	int special;
	int startloc;
	int varlen;
	int easy;
	int quotes = flag & (EXP_FULL | EXP_CASE);

	varflags = *p++;
	subtype = varflags & VSTYPE;
	var = p;
	special = 0;
	if (! is_name(*p))
		special = 1;
	p = strchr(p, '=') + 1;
again: /* jump here after setting a variable with ${var=text} */
	if (special) {
		set = varisset(var, varflags & VSNUL);
		val = NULL;
	} else {
		val = lookupvar(var);
		if (val == NULL || ((varflags & VSNUL) && val[0] == '\0')) {
			val = NULL;
			set = 0;
		} else
			set = 1;
	}
	varlen = 0;
	startloc = expdest - stackblock();
	if (set && subtype != VSPLUS) {
		/* insert the value of the variable */
		if (special) {
			varvalue(var, varflags & VSQUOTE, flag & EXP_FULL);
			if (subtype == VSLENGTH) {
				varlen = expdest - stackblock() - startloc;
				STADJUST(-varlen, expdest);
			}
		} else {
			char const *syntax = (varflags & VSQUOTE) ? DQSYNTAX
								  : BASESYNTAX;

			if (subtype == VSLENGTH) {
				for (;*val; val++)
					varlen++;
			}
			else {
				while (*val) {
					if (quotes && syntax[*val] == CCTL)
						STPUTC(CTLESC, expdest);
					STPUTC(*val++, expdest);
				}

			}
		}
	}

	if (subtype == VSPLUS)
		set = ! set;

	easy = ((varflags & VSQUOTE) == 0 ||
		(*var == '@' && shellparam.nparam != 1));


	switch (subtype) {
	case VSLENGTH:
		expdest = cvtnum(varlen, expdest);
		goto record;

	case VSNORMAL:
		if (!easy)
			break;
record:
		recordregion(startloc, expdest - stackblock(),
			     varflags & VSQUOTE);
		break;

	case VSPLUS:
	case VSMINUS:
		if (!set) {
			argstr(p, flag);
			break;
		}
		if (easy)
			goto record;
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
		pat = expdest;
		if (subevalvar(p, NULL, expdest - stackblock(), subtype,
			       startloc, varflags))
			goto record;
		else {
			int amount = (expdest - pat) + 1;
			STADJUST(-amount, expdest);
		}
		break;

	case VSASSIGN:
	case VSQUESTION:
		if (!set) {
			if (subevalvar(p, var, 0, subtype, startloc, varflags)) {
				varflags &= ~VSNUL;
				goto again;
			}
			break;
		}
		if (easy)
			goto record;
		break;

	default:
		abort();
	}

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

STATIC int
varisset(name, nulok)
	char *name;
	int nulok;
{

	if (*name == '!')
		return backgndpid != -1;
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
		int num = atoi(name);

		if (num > shellparam.nparam)
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



/*
 * Add the value of a specialized variable to the stack string.
 */

STATIC void
varvalue(name, quoted, allow_split)
	char *name;
	int quoted;
	int allow_split;
{
	int num;
	char *p;
	int i;
	extern int oexitstatus;
	char sep;
	char **ap;
	char const *syntax;

#define STRTODEST(p) \
	do {\
	if (allow_split) { \
		syntax = quoted? DQSYNTAX : BASESYNTAX; \
		while (*p) { \
			if (syntax[*p] == CCTL) \
				STPUTC(CTLESC, expdest); \
			STPUTC(*p++, expdest); \
		} \
	} else \
		while (*p) \
			STPUTC(*p++, expdest); \
	} while (0)


	switch (*name) {
	case '$':
		num = rootpid;
		goto numvar;
	case '?':
		num = oexitstatus;
		goto numvar;
	case '#':
		num = shellparam.nparam;
		goto numvar;
	case '!':
		num = backgndpid;
numvar:
		expdest = cvtnum(num, expdest);
		break;
	case '-':
		for (i = 0 ; i < NOPTS ; i++) {
			if (optlist[i].val)
				STPUTC(optlist[i].letter, expdest);
		}
		break;
	case '@':
		if (allow_split) {
			sep = '\0';
			goto allargs;
		}
		/* fall through */
	case '*':
		sep = ' ';
allargs:
		for (ap = shellparam.p ; (p = *ap++) != NULL ; ) {
			STRTODEST(p);
			if (*ap)
				STPUTC(sep, expdest);
		}
		break;
	case '0':
		p = arg0;
		STRTODEST(p);
		break;
	default:
		if (is_digit(*name)) {
			num = atoi(name);
			if (num > 0 && num <= shellparam.nparam) {
				p = shellparam.p[num - 1];
				STRTODEST(p);
			}
		}
		break;
	}
}



/*
 * Record the the fact that we have to scan this region of the
 * string for IFS characters.
 */

STATIC void
recordregion(start, end, nulonly)
	int start;
	int end;
	int nulonly;
{
	struct ifsregion *ifsp;

	if (ifslastp == NULL) {
		ifsp = &ifsfirst;
	} else {
		ifsp = (struct ifsregion *)ckmalloc(sizeof (struct ifsregion));
		ifslastp->next = ifsp;
	}
	ifslastp = ifsp;
	ifslastp->next = NULL;
	ifslastp->begoff = start;
	ifslastp->endoff = end;
	ifslastp->nulonly = nulonly;
}



/*
 * Break the argument string into pieces based upon IFS and add the
 * strings to the argument list.  The regions of the string to be
 * searched for IFS characters have been stored by recordregion.
 */
STATIC void
ifsbreakup(string, arglist)
	char *string;
	struct arglist *arglist;
	{
	struct ifsregion *ifsp;
	struct strlist *sp;
	char *start;
	char *p;
	char *q;
	char *ifs;
	int ifsspc;


	start = string;
	if (ifslastp != NULL) {
		ifsp = &ifsfirst;
		do {
			p = string + ifsp->begoff;
			ifs = ifsp->nulonly? nullstr : ifsval();
			ifsspc = strchr(ifs, ' ') != NULL;
			while (p < string + ifsp->endoff) {
				q = p;
				if (*p == CTLESC)
					p++;
				if (strchr(ifs, *p++)) {
					if (q > start || !ifsspc) {
						*q = '\0';
						sp = (struct strlist *)stalloc(sizeof *sp);
						sp->text = start;
						*arglist->lastp = sp;
						arglist->lastp = &sp->next;
					}
					if (ifsspc) {
						for (;;) {
							if (p >= string + ifsp->endoff)
								break;
							q = p;
							if (*p == CTLESC)
								p++;
							if (strchr(ifs, *p++) == NULL) {
								p = q;
								break;
							}
						}
					}
					start = p;
				}
			}
		} while ((ifsp = ifsp->next) != NULL);
		if (*start || (!ifsspc && start > string)) {
			sp = (struct strlist *)stalloc(sizeof *sp);
			sp->text = start;
			*arglist->lastp = sp;
			arglist->lastp = &sp->next;
		}
	} else {
		sp = (struct strlist *)stalloc(sizeof *sp);
		sp->text = start;
		*arglist->lastp = sp;
		arglist->lastp = &sp->next;
	}
}



/*
 * Expand shell metacharacters.  At this point, the only control characters
 * should be escapes.  The results are stored in the list exparg.
 */

char *expdir;


STATIC void
expandmeta(str, flag)
	struct strlist *str;
	int flag;
{
	char *p;
	struct strlist **savelastp;
	struct strlist *sp;
	char c;
	/* TODO - EXP_REDIR */

	while (str) {
		if (fflag)
			goto nometa;
		p = str->text;
		for (;;) {			/* fast check for meta chars */
			if ((c = *p++) == '\0')
				goto nometa;
			if (c == '*' || c == '?' || c == '[' || c == '!')
				break;
		}
		savelastp = exparg.lastp;
		INTOFF;
		if (expdir == NULL) {
			int i = strlen(str->text);
			expdir = ckmalloc(i < 2048 ? 2048 : i); /* XXX */
		}

		expmeta(expdir, str->text);
		ckfree(expdir);
		expdir = NULL;
		INTON;
		if (exparg.lastp == savelastp) {
			/*
			 * no matches
			 */
nometa:
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

STATIC void
expmeta(enddir, name)
	char *enddir;
	char *name;
	{
	char *p;
	char *q;
	char *start;
	char *endname;
	int metaflag;
	struct stat statb;
	DIR *dirp;
	struct dirent *dp;
	int atend;
	int matchdot;

	metaflag = 0;
	start = name;
	for (p = name ; ; p++) {
		if (*p == '*' || *p == '?')
			metaflag = 1;
		else if (*p == '[') {
			q = p + 1;
			if (*q == '!')
				q++;
			for (;;) {
				if (*q == CTLESC)
					q++;
				if (*q == '/' || *q == '\0')
					break;
				if (*++q == ']') {
					metaflag = 1;
					break;
				}
			}
		} else if (*p == '!' && p[1] == '!'	&& (p == name || p[-1] == '/')) {
			metaflag = 1;
		} else if (*p == '\0')
			break;
		else if (*p == CTLESC)
			p++;
		if (*p == '/') {
			if (metaflag)
				break;
			start = p + 1;
		}
	}
	if (metaflag == 0) {	/* we've reached the end of the file name */
		if (enddir != expdir)
			metaflag++;
		for (p = name ; ; p++) {
			if (*p == CTLESC)
				p++;
			*enddir++ = *p;
			if (*p == '\0')
				break;
		}
		if (metaflag == 0 || stat(expdir, &statb) >= 0)
			addfname(expdir);
		return;
	}
	endname = p;
	if (start != name) {
		p = name;
		while (p < start) {
			if (*p == CTLESC)
				p++;
			*enddir++ = *p++;
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
		*endname++ = '\0';
	}
	matchdot = 0;
	if (start[0] == '.' || (start[0] == CTLESC && start[1] == '.'))
		matchdot++;
	while (! int_pending() && (dp = readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.' && ! matchdot)
			continue;
		if (patmatch(start, dp->d_name)) {
			if (atend) {
				scopy(dp->d_name, enddir);
				addfname(expdir);
			} else {
				char *q;
				for (p = enddir, q = dp->d_name;
				     (*p++ = *q++) != '\0';)
					continue;
				p[-1] = '/';
				expmeta(p, endname);
			}
		}
	}
	closedir(dirp);
	if (! atend)
		endname[-1] = '/';
}


/*
 * Add a file name to the list.
 */

STATIC void
addfname(name)
	char *name;
	{
	char *p;
	struct strlist *sp;

	p = stalloc(strlen(name) + 1);
	scopy(name, p);
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

STATIC struct strlist *
expsort(str)
	struct strlist *str;
	{
	int len;
	struct strlist *sp;

	len = 0;
	for (sp = str ; sp ; sp = sp->next)
		len++;
	return msort(str, len);
}


STATIC struct strlist *
msort(list, len)
	struct strlist *list;
	int len;
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



/*
 * Returns true if the pattern matches the string.
 */

int
patmatch(pattern, string)
	char *pattern;
	char *string;
	{
#ifdef notdef
	if (pattern[0] == '!' && pattern[1] == '!')
		return 1 - pmatch(pattern + 2, string);
	else
#endif
		return pmatch(pattern, string);
}


STATIC int
pmatch(pattern, string)
	char *pattern;
	char *string;
	{
	char *p, *q;
	char c;

	p = pattern;
	q = string;
	for (;;) {
		switch (c = *p++) {
		case '\0':
			goto breakloop;
		case CTLESC:
			if (*q++ != *p++)
				return 0;
			break;
		case '?':
			if (*q++ == '\0')
				return 0;
			break;
		case '*':
			c = *p;
			if (c != CTLESC && c != '?' && c != '*' && c != '[') {
				while (*q != c) {
					if (*q == '\0')
						return 0;
					q++;
				}
			}
			do {
				if (pmatch(p, q))
					return 1;
			} while (*q++ != '\0');
			return 0;
		case '[': {
			char *endp;
			int invert, found;
			char chr;

			endp = p;
			if (*endp == '!')
				endp++;
			for (;;) {
				if (*endp == '\0')
					goto dft;		/* no matching ] */
				if (*endp == CTLESC)
					endp++;
				if (*++endp == ']')
					break;
			}
			invert = 0;
			if (*p == '!') {
				invert++;
				p++;
			}
			found = 0;
			chr = *q++;
			if (chr == '\0')
				return 0;
			c = *p++;
			do {
				if (c == CTLESC)
					c = *p++;
				if (*p == '-' && p[1] != ']') {
					p++;
					if (*p == CTLESC)
						p++;
					if (   collate_range_cmp(chr, c) >= 0
					    && collate_range_cmp(chr, *p) <= 0
					   )
						found = 1;
					p++;
				} else {
					if (chr == c)
						found = 1;
				}
			} while ((c = *p++) != ']');
			if (found == invert)
				return 0;
			break;
		}
dft:	        default:
			if (*q++ != c)
				return 0;
			break;
		}
	}
breakloop:
	if (*q != '\0')
		return 0;
	return 1;
}



/*
 * Remove any CTLESC characters from a string.
 */

void
rmescapes(str)
	char *str;
	{
	char *p, *q;

	p = str;
	while (*p != CTLESC) {
		if (*p++ == '\0')
			return;
	}
	q = p;
	while (*p) {
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
casematch(pattern, val)
	union node *pattern;
	char *val;
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
	result = patmatch(p, val);
	popstackmark(&smark);
	return result;
}

/*
 * Our own itoa().
 */

STATIC char *
cvtnum(num, buf)
	int num;
	char *buf;
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

	while (*p)
		STPUTC(*p++, buf);
	return buf;
}
