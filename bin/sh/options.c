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
/*static char sccsid[] = "from: @(#)options.c	5.2 (Berkeley) 3/13/91";*/
static char rcsid[] = "options.c,v 1.4 1993/08/01 18:58:04 mycroft Exp";
#endif /* not lint */

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

char *arg0;			/* value of $0 */
struct shparam shellparam;	/* current positional parameters */
char **argptr;			/* argument list for builtin commands */
char *optarg;			/* set by nextopt (like getopt) */
char *optptr;			/* used by nextopt */

char *minusc;			/* argument to -c option */


#ifdef __STDC__
STATIC void options(int);
STATIC void setoption(int, int);
#else
STATIC void options();
STATIC void setoption();
#endif



/*
 * Process the shell command line arguments.
 */

void
procargs(argc, argv)
	char **argv;
	{
	char *p;

	argptr = argv;
	if (argc > 0)
		argptr++;
	for (p = optval ; p < optval + sizeof optval - 1 ; p++)
		*p = 2;
	options(1);
	if (*argptr == NULL && minusc == NULL)
		sflag = 1;
	if (iflag == 2 && sflag == 1 && isatty(0) && isatty(1))
		iflag = 1;
	if (jflag == 2)
		jflag = iflag;
	for (p = optval ; p < optval + sizeof optval - 1 ; p++)
		if (*p == 2)
			*p = 0;
	arg0 = argv[0];
	if (sflag == 0 && minusc == NULL) {
		commandname = arg0 = *argptr++;
		setinputfile(commandname, 0);
	}
	shellparam.p = argptr;
	/* assert(shellparam.malloc == 0 && shellparam.nparam == 0); */
	while (*argptr) {
		shellparam.nparam++;
		argptr++;
	}
	setinteractive(iflag);
	setjobctl(jflag);
}



/*
 * Process shell options.  The global variable argptr contains a pointer
 * to the argument list; we advance it past the options.
 */

STATIC void
options(cmdline) {
	register char *p;
	int val;
	int c;

	if (cmdline)
		minusc = NULL;
	while ((p = *argptr) != NULL) {
		argptr++;
		if ((c = *p++) == '-') {
			val = 1;
                        if (p[0] == '\0' || p[0] == '-' && p[1] == '\0') {
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
			} else {
				setoption(c, val);
			}
		}
		if (! cmdline)
			break;
	}
}


STATIC void
setoption(flag, val)
	char flag;
	int val;
	{
	register char *p;

	if ((p = strchr(optchar, flag)) == NULL)
		error("Illegal option -%c", flag);
	optval[p - optchar] = val;
}



#ifdef mkinit
INCLUDE "options.h"

SHELLPROC {
	char *p;

	for (p = optval ; p < optval + sizeof optval ; p++)
		*p = 0;
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

shiftcmd(argc, argv)  char **argv; {
	int n;
	char **ap1, **ap2;

	n = 1;
	if (argc > 1)
		n = number(argv[1]);
	if (n > shellparam.nparam)
		n = shellparam.nparam;
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

setcmd(argc, argv)  char **argv; {
	if (argc == 1)
		return showvarscmd(argc, argv);
	INTOFF;
	options(0);
	setinteractive(iflag);
	setjobctl(jflag);
	if (*argptr != NULL) {
		setparam(argptr);
	}
	INTON;
	return 0;
}


/*
 * The getopts builtin.  Shellparam.optnext points to the next argument
 * to be processed.  Shellparam.optptr points to the next character to
 * be processed in the current argument.  If shellparam.optnext is NULL,
 * then it's the first time getopts has been called.
 */

getoptscmd(argc, argv)  char **argv; {
	register char *p, *q;
	char c;
	char s[10];

	if (argc != 3)
		error("Usage: getopts optstring var");
	if (shellparam.optnext == NULL) {
		shellparam.optnext = shellparam.p;
		shellparam.optptr = NULL;
	}
	if ((p = shellparam.optptr) == NULL || *p == '\0') {
		p = *shellparam.optnext;
		if (p == NULL || *p != '-' || *++p == '\0') {
atend:
			fmtstr(s, 10, "%d", shellparam.optnext - shellparam.p + 1);
			setvar("OPTIND", s, 0);
			shellparam.optnext = NULL;
			return 1;
		}
		shellparam.optnext++;
		if (p[0] == '-' && p[1] == '\0')	/* check for "--" */
			goto atend;
	}
	c = *p++;
	for (q = argv[1] ; *q != c ; ) {
		if (*q == '\0') {
			out1fmt("Illegal option -%c\n", c);
			c = '?';
			goto out;
		}
		if (*++q == ':')
			q++;
	}
	if (*++q == ':') {
		if (*p == '\0' && (p = *shellparam.optnext) == NULL) {
			out1fmt("No arg for -%c option\n", c);
			c = '?';
			goto out;
		}
		shellparam.optnext++;
		setvar("OPTARG", p, 0);
		p = NULL;
	}
out:
	shellparam.optptr = p;
	s[0] = c;
	s[1] = '\0';
	setvar(argv[2], s, 0);
	return 0;
}

/*
 * Standard option processing (a la getopt) for builtin routines.  The
 * only argument that is passed to nextopt is the option string; the
 * other arguments are unnecessary.  It return the character, or '\0' on
 * end of input.
 */

int
nextopt(optstring)
	char *optstring;
	{
	register char *p, *q;
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
		optarg = p;
		p = NULL;
	}
	optptr = p;
	return c;
}
