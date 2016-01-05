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
 *	@(#)options.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD$
 */

struct shparam {
	int nparam;		/* # of positional parameters (without $0) */
	unsigned char malloc;	/* if parameter list dynamically allocated */
	unsigned char reset;	/* if getopts has been reset */
	char **p;		/* parameter list */
	char **optp;		/* parameter list for getopts */
	char **optnext;		/* next parameter to be processed by getopts */
	char *optptr;		/* used by getopts */
};



#define eflag optlist[0].val
#define fflag optlist[1].val
#define Iflag optlist[2].val
#define iflag optlist[3].val
#define mflag optlist[4].val
#define nflag optlist[5].val
#define sflag optlist[6].val
#define xflag optlist[7].val
#define vflag optlist[8].val
#define Vflag optlist[9].val
#define	Eflag optlist[10].val
#define	Cflag optlist[11].val
#define	aflag optlist[12].val
#define	bflag optlist[13].val
#define	uflag optlist[14].val
#define	privileged optlist[15].val
#define	Tflag optlist[16].val
#define	Pflag optlist[17].val
#define	hflag optlist[18].val
#define	nologflag optlist[19].val

#define NSHORTOPTS	19
#define NOPTS		20

struct optent {
	const char *name;
	const char letter;
	char val;
};

extern struct optent optlist[NOPTS];
#ifdef DEFINE_OPTIONS
struct optent optlist[NOPTS] = {
	{ "errexit",	'e',	0 },
	{ "noglob",	'f',	0 },
	{ "ignoreeof",	'I',	0 },
	{ "interactive",'i',	0 },
	{ "monitor",	'm',	0 },
	{ "noexec",	'n',	0 },
	{ "stdin",	's',	0 },
	{ "xtrace",	'x',	0 },
	{ "verbose",	'v',	0 },
	{ "vi",		'V',	0 },
	{ "emacs",	'E',	0 },
	{ "noclobber",	'C',	0 },
	{ "allexport",	'a',	0 },
	{ "notify",	'b',	0 },
	{ "nounset",	'u',	0 },
	{ "privileged",	'p',	0 },
	{ "trapsasync",	'T',	0 },
	{ "physical",	'P',	0 },
	{ "trackall",	'h',	0 },
	{ "nolog",	'\0',	0 },
};
#endif


extern char *minusc;		/* argument to -c option */
extern char *arg0;		/* $0 */
extern struct shparam shellparam;  /* $@ */
extern char **argptr;		/* argument list for builtin commands */
extern char *shoptarg;		/* set by nextopt */
extern char *nextopt_optptr;	/* used by nextopt */

void procargs(int, char **);
void optschanged(void);
void setparam(char **);
void freeparam(struct shparam *);
int nextopt(const char *);
void getoptsreset(const char *);
