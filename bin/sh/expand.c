/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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
 */

#ifndef lint
/*static char sccsid[] = "from: @(#)expand.c	5.1 (Berkeley) 3/7/91";*/
static char rcsid[] = "expand.c,v 1.5 1993/08/01 18:58:16 mycroft Exp";
#endif /* not lint */

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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

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
#if UDIR
/*
 * Set if the last argument processed had /u/logname expanded.  This
 * variable is read by the cd command.
 */
int didudir;
#endif

#ifdef __STDC__
STATIC void argstr(char *, int);
STATIC void expbackq(union node *, int, int);
STATIC char *evalvar(char *, int);
STATIC int varisset(int);
STATIC void varvalue(int, int, int);
STATIC void recordregion(int, int, int);
STATIC void ifsbreakup(char *, struct arglist *);
STATIC void expandmeta(struct strlist *);
STATIC void expmeta(char *, char *);
STATIC void addfname(char *);
STATIC struct strlist *expsort(struct strlist *);
STATIC struct strlist *msort(struct strlist *, int);
STATIC int pmatch(char *, char *);
#else
STATIC void argstr();
STATIC void expbackq();
STATIC char *evalvar();
STATIC int varisset();
STATIC void varvalue();
STATIC void recordregion();
STATIC void ifsbreakup();
STATIC void expandmeta();
STATIC void expmeta();
STATIC void addfname();
STATIC struct strlist *expsort();
STATIC struct strlist *msort();
STATIC int pmatch();
#endif
#if UDIR
#ifdef __STDC__
STATIC char *expudir(char *);
#else
STATIC char *expudir();
#endif
#endif /* UDIR */



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
 * placing the resulting list of arguments in arglist.  If full is true,
 * perform splitting and file name expansion.  When arglist is NULL, perform
 * here document expansion.
 */

void
expandarg(arg, arglist, full)
	union node *arg;
	struct arglist *arglist;
	{
	struct strlist *sp;
	char *p;

#if UDIR
	didudir = 0;
#endif
	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	ifsfirst.next = NULL;
	ifslastp = NULL;
	argstr(arg->narg.text, full);
	if (arglist == NULL)
		return;			/* here document expanded */
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	exparg.lastp = &exparg.list;
	if (full) {
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
 * Perform variable and command substitution.  If full is set, output CTLESC
 * characters to allow for further processing.  If full is not set, treat
 * $@ like $* since no splitting will be performed.
 */

STATIC void
argstr(p, full)
	register char *p;
	{
	char c;

	for (;;) {
		switch (c = *p++) {
		case '\0':
		case CTLENDVAR:
			goto breakloop;
		case CTLESC:
			if (full)
				STPUTC(c, expdest);
			c = *p++;
			STPUTC(c, expdest);
			break;
		case CTLVAR:
			p = evalvar(p, full);
			break;
		case CTLBACKQ:
		case CTLBACKQ|CTLQUOTE:
			expbackq(argbackq->n, c & CTLQUOTE, full);
			argbackq = argbackq->next;
			break;
		default:
			STPUTC(c, expdest);
		}
	}
breakloop:;
}


/*
 * Expand stuff in backwards quotes.
 */

STATIC void
expbackq(cmd, quoted, full)
	union node *cmd;
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
			if (full && syntax[lastc] == CCTL)
				STPUTC(CTLESC, dest);
			STPUTC(lastc, dest);
		}
	}
	if (lastc == '\n') {
		STUNPUTC(dest);
	}
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



/*
 * Expand a variable, and return a pointer to the next character in the
 * input string.
 */

STATIC char *
evalvar(p, full)
	char *p;
	{
	int subtype;
	int flags;
	char *var;
	char *val;
	int c;
	int set;
	int special;
	int startloc;

	flags = *p++;
	subtype = flags & VSTYPE;
	var = p;
	special = 0;
	if (! is_name(*p))
		special = 1;
	p = strchr(p, '=') + 1;
again: /* jump here after setting a variable with ${var=text} */
	if (special) {
		set = varisset(*var);
		val = NULL;
	} else {
		val = lookupvar(var);
		if (val == NULL || (flags & VSNUL) && val[0] == '\0') {
			val = NULL;
			set = 0;
		} else
			set = 1;
	}
	startloc = expdest - stackblock();
	if (set && subtype != VSPLUS) {
		/* insert the value of the variable */
		if (special) {
			varvalue(*var, flags & VSQUOTE, full);
		} else {
			char const *syntax = (flags & VSQUOTE)? DQSYNTAX : BASESYNTAX;

			while (*val) {
				if (full && syntax[*val] == CCTL)
					STPUTC(CTLESC, expdest);
				STPUTC(*val++, expdest);
			}
		}
	}
	if (subtype == VSPLUS)
		set = ! set;
	if (((flags & VSQUOTE) == 0 || (*var == '@' && shellparam.nparam != 1))
	 && (set || subtype == VSNORMAL))
		recordregion(startloc, expdest - stackblock(), flags & VSQUOTE);
	if (! set && subtype != VSNORMAL) {
		if (subtype == VSPLUS || subtype == VSMINUS) {
			argstr(p, full);
		} else {
			char *startp;
			int saveherefd = herefd;
			herefd = -1;
			argstr(p, 0);
			STACKSTRNUL(expdest);
			herefd = saveherefd;
			startp = stackblock() + startloc;
			if (subtype == VSASSIGN) {
				setvar(var, startp, 0);
				STADJUST(startp - expdest, expdest);
				flags &=~ VSNUL;
				goto again;
			}
			/* subtype == VSQUESTION */
			if (*p != CTLENDVAR) {
				outfmt(&errout, "%s\n", startp);
				error((char *)NULL);
			}
			error("%.*s: parameter %snot set", p - var - 1,
				var, (flags & VSNUL)? "null or " : nullstr);
		}
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
varisset(name)
	char name;
	{
	char **ap;

	if (name == '!') {
		if (backgndpid == -1)
			return 0;
	} else if (name == '@' || name == '*') {
		if (*shellparam.p == NULL)
			return 0;
	} else if ((unsigned)(name -= '1') <= '9' - '1') {
		ap = shellparam.p;
		do {
			if (*ap++ == NULL)
				return 0;
		} while (--name >= 0);
	}
	return 1;
}



/*
 * Add the value of a specialized variable to the stack string.
 */

STATIC void
varvalue(name, quoted, allow_split)
	char name;
	{
	int num;
	char temp[32];
	char *p;
	int i;
	extern int exitstatus;
	char sep;
	char **ap;
	char const *syntax;

	switch (name) {
	case '$':
		num = rootpid;
		goto numvar;
	case '?':
		num = exitstatus;
		goto numvar;
	case '#':
		num = shellparam.nparam;
		goto numvar;
	case '!':
		num = backgndpid;
numvar:
		p = temp + 31;
		temp[31] = '\0';
		do {
			*--p = num % 10 + '0';
		} while ((num /= 10) != 0);
		while (*p)
			STPUTC(*p++, expdest);
		break;
	case '-':
		for (i = 0 ; optchar[i] ; i++) {
			if (optval[i])
				STPUTC(optchar[i], expdest);
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
		/* Only emit CTLESC if we will do further processing,
		   i.e. if allow_split is set.  */
		syntax = quoted && allow_split ? DQSYNTAX : BASESYNTAX;
		for (ap = shellparam.p ; (p = *ap++) != NULL ; ) {
			/* should insert CTLESC characters */
			while (*p) {
				if (syntax[*p] == CCTL)
					STPUTC(CTLESC, expdest);
				STPUTC(*p++, expdest);
			}
			if (*ap)
				STPUTC(sep, expdest);
		}
		break;
	case '0':
		p = arg0;
string:
		/* Only emit CTLESC if we will do further processing,
		   i.e. if allow_split is set.  */
		syntax = quoted && allow_split ? DQSYNTAX : BASESYNTAX;
		while (*p) {
			if (syntax[*p] == CCTL)
				STPUTC(CTLESC, expdest);
			STPUTC(*p++, expdest);
		}
		break;
	default:
		if ((unsigned)(name -= '1') <= '9' - '1') {
			p = shellparam.p[name];
			goto string;
		}
		break;
	}
}



/*
 * Record the the fact that we have to scan this region of the
 * string for IFS characters.
 */

STATIC void
recordregion(start, end, nulonly) {
	register struct ifsregion *ifsp;

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
	register char *p;
	char *q;
	char *ifs;

	start = string;
	if (ifslastp != NULL) {
		ifsp = &ifsfirst;
		do {
			p = string + ifsp->begoff;
			ifs = ifsp->nulonly? nullstr : ifsval();
			while (p < string + ifsp->endoff) {
				q = p;
				if (*p == CTLESC)
					p++;
				if (strchr(ifs, *p++)) {
					if (q > start || *ifs != ' ') {
						*q = '\0';
						sp = (struct strlist *)stalloc(sizeof *sp);
						sp->text = start;
						*arglist->lastp = sp;
						arglist->lastp = &sp->next;
					}
					if (*ifs == ' ') {
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
		if (*start || (*ifs != ' ' && start > string)) {
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
expandmeta(str)
	struct strlist *str;
	{
	char *p;
	struct strlist **savelastp;
	struct strlist *sp;
	char c;

	while (str) {
		if (fflag)
			goto nometa;
		p = str->text;
#if UDIR
		if (p[0] == '/' && p[1] == 'u' && p[2] == '/')
			str->text = p = expudir(p);
#endif
		for (;;) {			/* fast check for meta chars */
			if ((c = *p++) == '\0')
				goto nometa;
			if (c == '*' || c == '?' || c == '[' || c == '!')
				break;
		}
		savelastp = exparg.lastp;
		INTOFF;
		if (expdir == NULL)
			expdir = ckmalloc(8192); /* I hope this is big enough */
		expmeta(expdir, str->text);
		if(strlen(expdir) >= 8192)
			error("malloc overflow in sh:expand.c in ckmalloc(8192)\n");
		ckfree(expdir);
		expdir = NULL;
		INTON;
		if (exparg.lastp == savelastp) {
			if (! zflag) {
nometa:
				*exparg.lastp = str;
				rmescapes(str->text);
				exparg.lastp = &str->next;
			}
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


#if UDIR
/*
 * Expand /u/username into the home directory for the specified user.
 * We could use the getpw stuff here, but then we would have to load
 * in stdio and who knows what else.
 */

#define MAXLOGNAME 32
#define MAXPWLINE 128

char *pfgets();


STATIC char *
expudir(path)
	char *path;
	{
	register char *p, *q, *r;
	char name[MAXLOGNAME];
	char line[MAXPWLINE];
	int i;

	r = path;				/* result on failure */
	p = r + 3;			/* the 3 skips "/u/" */
	q = name;
	while (*p && *p != '/') {
		if (q >= name + MAXLOGNAME - 1)
			return r;		/* fail, name too long */
		*q++ = *p++;
	}
	*q = '\0';
	setinputfile("/etc/passwd", 1);
	q = line + strlen(name);
	while (pfgets(line, MAXPWLINE) != NULL) {
		if (line[0] == name[0] && prefix(name, line) && *q == ':') {
			/* skip to start of home directory */
			i = 4;
			do {
				while (*++q && *q != ':');
			} while (--i > 0);
			if (*q == '\0')
				break;		/* fail, corrupted /etc/passwd */
			q++;
			for (r = q ; *r && *r != '\n' && *r != ':' ; r++);
			*r = '\0';		/* nul terminate home directory */
			i = r - q;		/* i = strlen(q) */
			r = stalloc(i + strlen(p) + 1);
			scopy(q, r);
			scopy(p, r + i);
			TRACE(("expudir converts %s to %s\n", path, r));
			didudir = 1;
			path = r;		/* succeed */
			break;
		}
	}
	popfile();
	return r;
}
#endif


/*
 * Do metacharacter (i.e. *, ?, [...]) expansion.
 */

STATIC void
expmeta(enddir, name)
	char *enddir;
	char *name;
	{
	register char *p;
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
	if (start[0] == '.' || start[0] == CTLESC && start[1] == '.')
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
				for (p = enddir, q = dp->d_name ; *p++ = *q++ ;);
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
	{
	struct strlist *p, *q;
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
	if (pattern[0] == '!' && pattern[1] == '!')
		return 1 - pmatch(pattern + 2, string);
	else
		return pmatch(pattern, string);
}


STATIC int
pmatch(pattern, string)
	char *pattern;
	char *string;
	{
	register char *p, *q;
	register char c;

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
			c = *p++;
			do {
				if (c == CTLESC)
					c = *p++;
				if (*p == '-' && p[1] != ']') {
					p++;
					if (*p == CTLESC)
						p++;
					if (chr >= c && chr <= *p)
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
dft:	    default:
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
	register char *p, *q;

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
	/* Preserve any CTLESC characters inserted previously, so that
	   we won't expand reg exps which are inside strings.  */
	argstr(pattern->narg.text, 1);
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	result = patmatch(p, val);
	popstackmark(&smark);
	return result;
}
