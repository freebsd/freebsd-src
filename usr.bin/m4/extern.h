/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
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
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 */

char	*basename __P((char *));
char	*xalloc __P((unsigned long));
int	expr __P((char *));
ndptr	addent __P((char *));
void	chrsave __P((int));
void	dochc __P((char *[], int));
void	dochq __P((char *[], int));
void	dodefine __P((char *, char *));
void	dodefn __P((char *));
void	dodiv __P((int));
void	dodump __P((char *[], int));
void	doifelse __P((char *[], int));
int	doincl __P((char *));
int	dopaste __P((char *));
void	dopushdef __P((char *, char *));
void	dosub __P((char *[], int));
void	doundiv __P((char *[], int));
void	eval __P((char *[], int, int));
void	expand __P((char *[], int));
void	getdiv __P((int));
char	*xstrdup __P((const char *));
int	hash __P((char *));
int	indx __P((char *, char *));
void	killdiv __P((void));
ndptr	lookup __P((char *));
void	map __P((char *, char *, char *, char *));
void	onintr __P((int));
void	oops __P((const char *, ...));
void	pbnum __P((int));
void	pbstr __P((char *));
void	putback __P((int));
void	remhash __P((char *, int));
void	usage __P((void));

extern ndptr hashtab[];		/* hash table for macros etc. */
extern stae mstack[];		/* stack of m4 machine */
extern FILE *active;		/* active output file pointer */
extern FILE *infile[];		/* input file stack (0=stdin) */
extern FILE *outfile[];		/* diversion array(0=bitbucket) */
extern int fp; 			/* m4 call frame pointer */
extern int ilevel;		/* input file stack pointer */
extern int oindex;		/* diversion index. */
extern int sp;			/* current m4 stack pointer */
extern char *bp;		/* first available character */
extern char buf[];		/* push-back buffer */
extern char *bufbase;		/* buffer base for this ilevel */
extern char *bbase[];		/* buffer base per ilevel */
extern char ecommt;		/* end character for comment */
extern char *endest;		/* end of string space */
extern char *endpbb;		/* end of push-back buffer */
extern char *ep;		/* first free char in strspace */
extern char lquote;		/* left quote character (`) */
extern char *m4temp;		/* filename for diversions */
extern char *m4wraps;		/* m4wrap string default. */
extern char *null;		/* as it says.. just a null. */
extern char *progname;		/* program name */
extern char rquote;		/* right quote character (') */
extern char scommt;		/* start character for comment */
