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
 *
 *	from: @(#)options.h	5.1 (Berkeley) 3/7/91
 *	options.h,v 1.4 1993/08/01 18:58:39 mycroft Exp
 */

struct shparam {
	int nparam;	/* number of positional parameters (without $0) */
	char malloc;	/* true if parameter list dynamicly allocated */
	char **p;		/* parameter list */
	char **optnext;	/* next parameter to be processed by getopts */
	char *optptr;	/* used by getopts */
};



#define eflag optval[0]
#define fflag optval[1]
#define Iflag optval[2]
#define iflag optval[3]
#define jflag optval[4]
#define nflag optval[5]
#define sflag optval[6]
#define xflag optval[7]
#define zflag optval[8]
#define vflag optval[9]

#define NOPTS	10

#ifdef DEFINE_OPTIONS
const char optchar[NOPTS+1] = "efIijnsxzv";       /* shell flags */
char optval[NOPTS+1];           /* values of option flags */
#else
extern const char optchar[NOPTS+1];
extern char optval[NOPTS+1];
#endif


extern char *minusc;		/* argument to -c option */
extern char *arg0;		/* $0 */
extern struct shparam shellparam;  /* $@ */
extern char **argptr;		/* argument list for builtin commands */
extern char *optarg;		/* set by nextopt */
extern char *optptr;		/* used by nextopt */


#ifdef __STDC__
void procargs(int, char **);
void setparam(char **);
void freeparam(struct shparam *);
int nextopt(char *);
#else
void procargs();
void setparam();
void freeparam();
int nextopt();
#endif
