/* ed.h: type and constant definitions for the ed editor. */
/*
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Andrew Moore, Talke Studio.
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
 *	@(#)ed.h	5.5 (Berkeley) 3/28/93
 */

#include <unistd.h>
#include <errno.h>
#if defined(BSD) && BSD >= 199103 || defined(__386BSD__)
# include <sys/param.h>		/* for MAXPATHLEN */
#endif
#include <regex.h>
#include <signal.h>

#define BITSPERBYTE 8
#define BITS(type)  (BITSPERBYTE * (int)sizeof(type))
#define CHARBITS    BITS(char)
#define INTBITS     BITS(int)
#define INTHIBIT    (1 << (INTBITS - 1))

#define ERR		(-2)
#define EMOD		(-3)
#define FATAL		(-4)

#ifndef MAXPATHLEN
# define MAXPATHLEN 255		/* _POSIX_PATH_MAX */
#endif

#define MAXFNAME MAXPATHLEN	/* max file name size */
#define MINBUFSZ 512		/* minimum buffer size - must be > 0 */
#define LINECHARS (INTHIBIT - 1) /* max chars per line */
#define SE_MAX 30		/* max subexpressions in a regular expression */

typedef regex_t pattern_t;

/* Line node */
typedef struct	line {
	struct line	*next;
	struct line	*prev;
	off_t		seek;		/* address of line in scratch buffer */

#define ACTV INTHIBIT			/* active bit: high bit of len */

	int		len;		/* length of line */
} line_t;


typedef struct undo {

/* type of undo nodes */
#define UADD	0
#define UDEL 	1
#define UMOV	2
#define VMOV	3

	int type;			/* command type */
	line_t	*h;			/* head of list */
	line_t  *t;			/* tail of list */
} undo_t;

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/* nextln: return line after l mod k */
#define nextln(l,k)	((l)+1 > (k) ? 0 : (l)+1)

/* nextln: return line before l mod k */
#define prevln(l,k)	((l)-1 < 0 ? (k) : (l)-1)

#define	skipblanks() while (isspace(*ibufp) && *ibufp != '\n') ibufp++

/* spl1: disable some interrupts (requires reliable signals) */
#define spl1() mutex++

/* spl0: enable all interrupts; check sigflags (requires reliable signals) */
#define spl0() \
if (--mutex == 0) { \
	if (sigflags & (1 << SIGHUP)) dohup(SIGHUP); \
	if (sigflags & (1 << SIGINT)) dointr(SIGINT); \
}

#if defined(sun) || defined(NO_REALLOC_NULL)
/* CKBUF: assure at least a minimum size for buffer b */
#define CKBUF(b,n,i,err) \
if ((i) > (n)) { \
	int ti = (n); \
	char *ts; \
	spl1(); \
	if ((b) != NULL) { \
		if ((ts = (char *) realloc((b), ti += max((i), MINBUFSZ))) == NULL) { \
			fprintf(stderr, "%s\n", strerror(errno)); \
			sprintf(errmsg, "out of memory"); \
			spl0(); \
			return err; \
		} \
	} else { \
		if ((ts = (char *) malloc(ti += max((i), MINBUFSZ))) == NULL) { \
			fprintf(stderr, "%s\n", strerror(errno)); \
			sprintf(errmsg, "out of memory"); \
			spl0(); \
			return err; \
		} \
	} \
	(n) = ti; \
	(b) = ts; \
	spl0(); \
}
#else /* NO_REALLOC_NULL */
/* CKBUF: assure at least a minimum size for buffer b */
#define CKBUF(b,n,i,err) \
if ((i) > (n)) { \
	int ti = (n); \
	char *ts; \
	spl1(); \
	if ((ts = (char *) realloc((b), ti += max((i), MINBUFSZ))) == NULL) { \
		fprintf(stderr, "%s\n", strerror(errno)); \
		sprintf(errmsg, "out of memory"); \
		spl0(); \
		return err; \
	} \
	(n) = ti; \
	(b) = ts; \
	spl0(); \
}
#endif /* NO_REALLOC_NULL */

/* requeue: link pred before succ */
#define requeue(pred, succ) (pred)->next = (succ), (succ)->prev = (pred)

/* insqueue: insert elem in circular queue after pred */
#define insqueue(elem, pred) \
{ \
	requeue((elem), (pred)->next); \
	requeue((pred), elem); \
}

/* remqueue: remove elem from circular queue */
#define remqueue(elem) requeue((elem)->prev, (elem)->next);

/* nultonl: overwrite ASCII NULs with newlines */
#define nultonl(s, l) translit(s, l, '\0', '\n')

/* nltonul: overwrite newlines with ASCII NULs */
#define nltonul(s, l) translit(s, l, '\n', '\0')

#ifndef strerror
# define strerror(n) sys_errlist[n]
#endif

#ifndef __P
# ifndef __STDC__
#  define __P(proto) ()
# else
#  define __P(proto) proto
# endif
#endif

/* local function declarations */
int append __P((long, int));
int cbcdec __P((char *, FILE *));
int cbcenc __P((char *, int, FILE *));
char *ckfn __P((char *));
int ckglob __P((void));
int ckrange __P((long, long));
int desflush __P((FILE *));
int desgetc __P((FILE *));
void desinit __P((void));
int desputc __P((int, FILE *));
int docmd __P((int));
void err __P((char *));
char *ccl __P((char *));
void cvtkey __P((char *, char *));
long doglob __P((int));
void dohup __P((int));
void dointr __P((int));
void dowinch __P((int));
int doprint __P((long, long, int));
long doread __P((long, char *));
long dowrite __P((long, long, char *, char *));
char *esctos __P((char *));
long patscan __P((pattern_t *, int));
long getaddr __P((line_t *));
char *getcmdv __P((int *, int));
char *getfn __P((void));
int getkey __P((void));
char *getlhs __P((int));
int getline __P((void));
int getlist __P((void));
long getnum __P((int));
long getone __P((void));
line_t *getlp __P((long));
int getrhs __P((int));
int getshcmd __P((void));
char *gettxt __P((line_t *));
void init_buf __P((void));
int join __P((long, long));
int lndelete __P((long, long));
line_t *lpdup __P((line_t *));
void lpqueue __P((line_t *));
void makekey __P((char *));
char *makesub __P((int));
char *translit __P((char *, int, int, int));
int move __P((long));
int oddesc __P((char *, char *));
void onhup __P((int));
void onintr __P((int));
pattern_t *optpat __P((void));
void putstr __P((char *, int, long, int));
char *puttxt __P((char *));
void quit __P((int));
int regsub __P((pattern_t *, line_t *, int));
int sbclose __P((void));
int sbopen __P((void));
int sgetline __P((FILE *));
int catsub __P((char *, regmatch_t *, int));
int subst __P((pattern_t *, int));
int tobinhex __P((int, int));
int transfer __P((long));
int undo __P((void));
undo_t *upush __P((int, long, long));
void ureset __P((void));


extern char *sys_errlist[];
extern int mutex;
extern int sigflags;
