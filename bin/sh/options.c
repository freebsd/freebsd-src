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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)options.c	8.2 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "shell.h"
#define DEFINE_OPTIONS
#include "options.h"
#undef DEFINE_OPTIONS
#include "nodes.h"	/* for other header files */
#include "eval.h"
#include "jobs.h"
#include "input.h"
#include "output.h"
#include "trap.h"
#include "var.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"
#ifndef NO_HISTORY
#include "myhistedit.h"
#endif

char *arg0;			/* value of $0 */
struct shparam shellparam;	/* current positional parameters */
char **argptr;			/* argument list for builtin commands */
char *shoptarg;			/* set by nextopt (like getopt) */
char *optptr;			/* used by nextopt */

char *minusc;			/* argument to -c option */


STATIC void options __P((int));
STATIC void minus_o __P((char *, int));
STATIC void setoption __P((int, int));
STATIC int getopts __P((char *, char *, char **, char ***, char **));


/*
 * Process the shell command line arguments.
 */

void
procargs(argc, argv)
	int argc;
	char **argv;
{
	int i;

	argptr = argv;
	if (argc > 0)
		argptr++;
	for (i = 0; i < NOPTS; i++)
		optlist[i].val = 2;
	privileged = (getuid() != geteuid() || getgid() != getegid());
	options(1);
	if (*argptr == NULL && minusc == NULL)
		sflag = 1;
	if (iflag == 2 && sflag == 1 && isatty(0) && isatty(1))
		iflag = 1;
	if (mflag == 2)
		mflag = iflag;
	for (i = 0; i < NOPTS; i++)
		if (optlist[i].val == 2)
			optlist[i].val = 0;
	arg0 = argv[0];
	if (sflag == 0 && minusc == NULL) {
		commandname = arg0 = *argptr++;
		setinputfile(commandname, 0);
	}
	/* POSIX 1003.2: first arg after -c cmd is $0, remainder $1... */
	if (argptr && minusc && *argptr)
		arg0 = *argptr++;

	shellparam.p = argptr;
	shellparam.reset = 1;
	/* assert(shellparam.malloc == 0 && shellparam.nparam == 0); */
	while (*argptr) {
		shellparam.nparam++;
		argptr++;
	}
	optschanged();
}


void
optschanged()
{
	setinteractive(iflag);
#ifndef NO_HISTORY
	histedit();
#endif
	setjobctl(mflag);
}

/*
 * Process shell options.  The global variable argptr contains a pointer
 * to the argument list; we advance it past the options.
 */

STATIC void
options(cmdline)
	int cmdline;
{
	char *p;
	int val;
	int c;

	if (cmdline)
		minusc = NULL;
	while ((p = *argptr) != NULL) {
		argptr++;
		if ((c = *p++) == '-') {
			val = 1;
                        if (p[0] == '\0' || (p[0] == '-' && p[1] == '\0')) {
                                if (!cmdline) {
                                        /* "-" means turn off -x and -v */
                                        if (p[0] == '\0')
                                                xflag = vflag = 0;
                                        /* "--" means reset params */
                                        else if (*argptr == NULL)
						setparam(argptr);
                                }
				break;	  /* "-" or  "--" terminates options */
			}
		} else if (c == '+') {
			val = 0;
		} else {
			argptr--;
			break;
		}
		while ((c = *p++) != '\0') {
			if (c == 'c' && cmdline) {
				char *q;
#ifdef NOHACK	/* removing this code allows sh -ce 'foo' for compat */
				if (*p == '\0')
#endif
					q = *argptr++;
				if (q == NULL || minusc != NULL)
					error("Bad -c option");
				minusc = q;
#ifdef NOHACK
				break;
#endif
			} else if (c == 'o') {
				minus_o(*argptr, val);
				if (*argptr)
					argptr++;
			} else {
				if (c == 'p' && !val && privileged) {
					(void) setuid(getuid());
					(void) setgid(getgid());
				}
				setoption(c, val);
			}
		}
	}
}

STATIC void
minus_o(name, val)
	char *name;
	int val;
{
	int i;

	if (name == NULL) {
		out1str("Current option settings\n");
		for (i = 0; i < NOPTS; i++)
			out1fmt("%-16s%s\n", optlist[i].name,
				optlist[i].val ? "on" : "off");
	} else {
		for (i = 0; i < NOPTS; i++)
			if (equal(name, optlist[i].name)) {
				if (!val && privileged && equal(name, "privileged")) {
					(void) setuid(getuid());
					(void) setgid(getgid());
				}
				setoption(optlist[i].letter, val);
				return;
			}
		error("Illegal option -o %s", name);
	}
}


STATIC void
setoption(flag, val)
	char flag;
	int val;
	{
	int i;

	for (i = 0; i < NOPTS; i++)
		if (optlist[i].letter == flag) {
			optlist[i].val = val;
			if (val) {
				/* #%$ hack for ksh semantics */
				if (flag == 'V')
					Eflag = 0;
				else if (flag == 'E')
					Vflag = 0;
			}
			return;
		}
	error("Illegal option -%c", flag);
}



#ifdef mkinit
INCLUDE "options.h"

SHELLPROC {
	int i;

	for (i = 0; i < NOPTS; i++)
		optlist[i].val = 0;
	optschanged();

}
#endif


/*
 * Set the shell parameters.
 */

void
setparam(argv)
	char **argv;
	{
	char **newparam;
	char **ap;
	int nparam;

	for (nparam = 0 ; argv[nparam] ; nparam++);
	ap = newparam = ckmalloc((nparam + 1) * sizeof *ap);
	while (*argv) {
		*ap++ = savestr(*argv++);
	}
	*ap = NULL;
	freeparam(&shellparam);
	shellparam.malloc = 1;
	shellparam.nparam = nparam;
	shellparam.p = newparam;
	shellparam.optnext = NULL;
}


/*
 * Free the list of positional parameters.
 */

void
freeparam(param)
	struct shparam *param;
	{
	char **ap;

	if (param->malloc) {
		for (ap = param->p ; *ap ; ap++)
			ckfree(*ap);
		ckfree(param->p);
	}
}



/*
 * The shift builtin command.
 */

int
shiftcmd(argc, argv)
	int argc;
	char **argv;
{
	int n;
	char **ap1, **ap2;

	n = 1;
	if (argc > 1)
		n = number(argv[1]);
	if (n > shellparam.nparam)
		error("can't shift that many");
	INTOFF;
	shellparam.nparam -= n;
	for (ap1 = shellparam.p ; --n >= 0 ; ap1++) {
		if (shellparam.malloc)
			ckfree(*ap1);
	}
	ap2 = shellparam.p;
	while ((*ap2++ = *ap1++) != NULL);
	shellparam.optnext = NULL;
	INTON;
	return 0;
}



/*
 * The set command builtin.
 */

int
setcmd(argc, argv)
	int argc;
	char **argv;
{
	if (argc == 1)
		return showvarscmd(argc, argv);
	INTOFF;
	options(0);
	optschanged();
	if (*argptr != NULL) {
		setparam(argptr);
	}
	INTON;
	return 0;
}


void
getoptsreset(value)
	const char *value;
{
	if (number(value) == 1) {
		shellparam.optnext = NULL;
		shellparam.reset = 1;
	}
}

/*
 * The getopts builtin.  Shellparam.optnext points to the next argument
 * to be processed.  Shellparam.optptr points to the next character to
 * be processed in the current argument.  If shellparam.optnext is NULL,
 * then it's the first time getopts has been called.
 */

int
getoptscmd(argc, argv)
	int argc;
	char **argv;
{
	char **optbase = NULL;

	if (argc < 3)
		error("Usage: getopts optstring var [arg]");
	else if (argc == 3)
		optbase = shellparam.p;
	else
		optbase = &argv[3];

	if (shellparam.reset == 1) {
		shellparam.optnext = optbase;
		shellparam.optptr = NULL;
		shellparam.reset = 0;
	}

	return getopts(argv[1], argv[2], optbase, &shellparam.optnext,
		       &shellparam.optptr);
}

STATIC int
getopts(optstr, optvar, optfirst, optnext, optptr)
	char *optstr;
	char *optvar;
	char **optfirst;
	char ***optnext;
	char **optptr;
{
	char *p, *q;
	char c = '?';
	int done = 0;
	int ind = 0;
	int err = 0;
	char s[10];

	if ((p = *optptr) == NULL || *p == '\0') {
		/* Current word is done, advance */
		if (*optnext == NULL)
			return 1;
		p = **optnext;
		if (p == NULL || *p != '-' || *++p == '\0') {
atend:
			ind = *optnext - optfirst + 1;
			*optnext = NULL;
			p = NULL;
			done = 1;
			goto out;
		}
		(*optnext)++;
		if (p[0] == '-' && p[1] == '\0')	/* check for "--" */
			goto atend;
	}

	c = *p++;
	for (q = optstr; *q != c; ) {
		if (*q == '\0') {
			if (optstr[0] == ':') {
				s[0] = c;
				s[1] = '\0';
				err |= setvarsafe("OPTARG", s, 0);
			}
			else {
				out1fmt("Illegal option -%c\n", c);
				(void) unsetvar("OPTARG");
			}
			c = '?';
			goto bad;
		}
		if (*++q == ':')
			q++;
	}

	if (*++q == ':') {
		if (*p == '\0' && (p = **optnext) == NULL) {
			if (optstr[0] == ':') {
				s[0] = c;
				s[1] = '\0';
				err |= setvarsafe("OPTARG", s, 0);
				c = ':';
			}
			else {
				out1fmt("No arg for -%c option\n", c);
				(void) unsetvar("OPTARG");
				c = '?';
			}
			goto bad;
		}

		if (p == **optnext)
			(*optnext)++;
		setvarsafe("OPTARG", p, 0);
		p = NULL;
	}
	else
		setvarsafe("OPTARG", "", 0);
	ind = *optnext - optfirst + 1;
	goto out;

bad:
	ind = 1;
	*optnext = NULL;
	p = NULL;
out:
	*optptr = p;
	fmtstr(s, sizeof(s), "%d", ind);
	err |= setvarsafe("OPTIND", s, VNOFUNC);
	s[0] = c;
	s[1] = '\0';
	err |= setvarsafe(optvar, s, 0);
	if (err) {
		*optnext = NULL;
		*optptr = NULL;
		flushall();
		exraise(EXERROR);
	}
	return done;
}

/*
 * XXX - should get rid of.  have all builtins use getopt(3).  the
 * library getopt must have the BSD extension static variable "optreset"
 * otherwise it can't be used within the shell safely.
 *
 * Standard option processing (a la getopt) for builtin routines.  The
 * only argument that is passed to nextopt is the option string; the
 * other arguments are unnecessary.  It return the character, or '\0' on
 * end of input.
 */

int
nextopt(optstring)
	char *optstring;
	{
	char *p, *q;
	char c;

	if ((p = optptr) == NULL || *p == '\0') {
		p = *argptr;
		if (p == NULL || *p != '-' || *++p == '\0')
			return '\0';
		argptr++;
		if (p[0] == '-' && p[1] == '\0')	/* check for "--" */
			return '\0';
	}
	c = *p++;
	for (q = optstring ; *q != c ; ) {
		if (*q == '\0')
			error("Illegal option -%c", c);
		if (*++q == ':')
			q++;
	}
	if (*++q == ':') {
		if (*p == '\0' && (p = *argptr++) == NULL)
			error("No arg for -%c option", c);
		shoptarg = p;
		p = NULL;
	}
	optptr = p;
	return c;
}
