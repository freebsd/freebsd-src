/* $RCSfile: util.c,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:55 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: util.c,v $
 * Revision 1.1.1.1  1994/09/10  06:27:55  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:30:10  nate
 * PERL!
 *
 * Revision 4.0.1.1  91/06/07  12:20:35  lwall
 * patch4: new copyright notice
 *
 * Revision 4.0  91/03/20  01:58:25  lwall
 * 4.0 baseline.
 *
 */

#include <stdio.h>

#include "handy.h"
#include "EXTERN.h"
#include "a2p.h"
#include "INTERN.h"
#include "util.h"

#define FLUSH
#define MEM_SIZE unsigned int

static char nomem[] = "Out of memory!\n";

/* paranoid version of malloc */

static int an = 0;

char *
safemalloc(size)
MEM_SIZE size;
{
    char *ptr;
    char *malloc();

    ptr = malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%x: (%05d) malloc %d bytes\n",ptr,an++,size);
#endif
    if (ptr != Nullch)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
}

/* paranoid version of realloc */

char *
saferealloc(where,size)
char *where;
MEM_SIZE size;
{
    char *ptr;
    char *realloc();

    ptr = realloc(where,size?size:1);	/* realloc(0) is NASTY on our system */
#ifdef DEBUGGING
    if (debug & 128) {
	fprintf(stderr,"0x%x: (%05d) rfree\n",where,an++);
	fprintf(stderr,"0x%x: (%05d) realloc %d bytes\n",ptr,an++,size);
    }
#endif
    if (ptr != Nullch)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
}

/* safe version of free */

safefree(where)
char *where;
{
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%x: (%05d) free\n",where,an++);
#endif
    free(where);
}

/* safe version of string copy */

char *
safecpy(to,from,len)
char *to;
register char *from;
register int len;
{
    register char *dest = to;

    if (from != Nullch)
	for (len--; len && (*dest++ = *from++); len--) ;
    *dest = '\0';
    return to;
}

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
cpytill(to,from,delim)
register char *to, *from;
register int delim;
{
    for (; *from; from++,to++) {
	if (*from == '\\') {
	    if (from[1] == delim)
		from++;
	    else if (from[1] == '\\')
		*to++ = *from++;
	}
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    return from;
}


char *
cpy2(to,from,delim)
register char *to, *from;
register int delim;
{
    for (; *from; from++,to++) {
	if (*from == '\\')
	    *to++ = *from++;
	else if (*from == '$')
	    *to++ = '\\';
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    return from;
}

/* return ptr to little string in big string, NULL if not found */

char *
instr(big, little)
char *big, *little;

{
    register char *t, *s, *x;

    for (t = big; *t; t++) {
	for (x=t,s=little; *s; x++,s++) {
	    if (!*x)
		return Nullch;
	    if (*s != *x)
		break;
	}
	if (!*s)
	    return t;
    }
    return Nullch;
}

/* copy a string to a safe spot */

char *
savestr(str)
char *str;
{
    register char *newaddr = safemalloc((MEM_SIZE)(strlen(str)+1));

    (void)strcpy(newaddr,str);
    return newaddr;
}

/* grow a static string to at least a certain length */

void
growstr(strptr,curlen,newlen)
char **strptr;
int *curlen;
int newlen;
{
    if (newlen > *curlen) {		/* need more room? */
	if (*curlen)
	    *strptr = saferealloc(*strptr,(MEM_SIZE)newlen);
	else
	    *strptr = safemalloc((MEM_SIZE)newlen);
	*curlen = newlen;
    }
}

/*VARARGS1*/
fatal(pat,a1,a2,a3,a4)
char *pat;
{
    fprintf(stderr,pat,a1,a2,a3,a4);
    exit(1);
}

/*VARARGS1*/
warn(pat,a1,a2,a3,a4)
char *pat;
{
    fprintf(stderr,pat,a1,a2,a3,a4);
}

static bool firstsetenv = TRUE;
extern char **environ;

void
setenv(nam,val)
char *nam, *val;
{
    register int i=envix(nam);		/* where does it go? */

    if (!environ[i]) {			/* does not exist yet */
	if (firstsetenv) {		/* need we copy environment? */
	    int j;
#ifndef lint
	    char **tmpenv = (char**)	/* point our wand at memory */
		safemalloc((i+2) * sizeof(char*));
#else
	    char **tmpenv = Null(char **);
#endif /* lint */

	    firstsetenv = FALSE;
	    for (j=0; j<i; j++)		/* copy environment */
		tmpenv[j] = environ[j];
	    environ = tmpenv;		/* tell exec where it is now */
	}
#ifndef lint
	else
	    environ = (char**) saferealloc((char*) environ,
		(i+2) * sizeof(char*));
					/* just expand it a bit */
#endif /* lint */
	environ[i+1] = Nullch;	/* make sure it's null terminated */
    }
    environ[i] = safemalloc(strlen(nam) + strlen(val) + 2);
					/* this may or may not be in */
					/* the old environ structure */
    sprintf(environ[i],"%s=%s",nam,val);/* all that work just for this */
}

int
envix(nam)
char *nam;
{
    register int i, len = strlen(nam);

    for (i = 0; environ[i]; i++) {
	if (strnEQ(environ[i],nam,len) && environ[i][len] == '=')
	    break;			/* strnEQ must come first to avoid */
    }					/* potential SEGV's */
    return i;
}
