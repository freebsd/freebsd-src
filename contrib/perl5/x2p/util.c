/* $RCSfile: util.c,v $$Revision: 4.1 $$Date: 92/08/07 18:29:29 $
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	util.c,v $
 */

#include "EXTERN.h"
#include "a2p.h"
#include "INTERN.h"
#include "util.h"

#include <stdarg.h>
#define FLUSH

static char nomem[] = "Out of memory!\n";

/* paranoid version of malloc */


Malloc_t
safemalloc(MEM_SIZE size)
{
    Malloc_t ptr;

    /* malloc(0) is NASTY on some systems */
    ptr = malloc(size ? size : 1);
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%lx: (%05d) malloc %ld bytes\n",(unsigned long)ptr,
    	    	an++,(long)size);
#endif
    if (ptr != Nullch)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
    return 0;
}

/* paranoid version of realloc */

Malloc_t
saferealloc(Malloc_t where, MEM_SIZE size)
{
    Malloc_t ptr;

    /* realloc(0) is NASTY on some systems */
    ptr = realloc(where, size ? size : 1);
#ifdef DEBUGGING
    if (debug & 128) {
	fprintf(stderr,"0x%lx: (%05d) rfree\n",(unsigned long)where,an++);
	fprintf(stderr,"0x%lx: (%05d) realloc %ld bytes\n",(unsigned long)ptr,an++,(long)size);
    }
#endif
    if (ptr != Nullch)
	return ptr;
    else {
	fputs(nomem,stdout) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
    return 0;
}

/* safe version of free */

Free_t
safefree(Malloc_t where)
{
#ifdef DEBUGGING
    if (debug & 128)
	fprintf(stderr,"0x%lx: (%05d) free\n",(unsigned long)where,an++);
#endif
    free(where);
}

/* safe version of string copy */

char *
safecpy(char *to, register char *from, register int len)
{
    register char *dest = to;

    if (from != Nullch) 
	for (len--; len && (*dest++ = *from++); len--) ;
    *dest = '\0';
    return to;
}

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
cpytill(register char *to, register char *from, register int delim)
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
cpy2(register char *to, register char *from, register int delim)
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
instr(char *big, char *little)
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
savestr(char *str)
{
    register char *newaddr = (char *) safemalloc((MEM_SIZE)(strlen(str)+1));

    (void)strcpy(newaddr,str);
    return newaddr;
}

/* grow a static string to at least a certain length */

void
growstr(char **strptr, int *curlen, int newlen)
{
    if (newlen > *curlen) {		/* need more room? */
	if (*curlen)
	    *strptr = (char *) saferealloc(*strptr,(MEM_SIZE)newlen);
	else
	    *strptr = (char *) safemalloc((MEM_SIZE)newlen);
	*curlen = newlen;
    }
}

void
croak(char *pat,...)
{
#if defined(HAS_VPRINTF)
    va_list args;

    va_start(args, pat);
    vfprintf(stderr,pat,args);
#else
    fprintf(stderr,pat,a1,a2,a3,a4);
#endif
    exit(1);
}

void
fatal(char *pat,...)
{
#if defined(HAS_VPRINTF)
    va_list args;

    va_start(args, pat);
    vfprintf(stderr,pat,args);
#else
    fprintf(stderr,pat,a1,a2,a3,a4);
#endif
    exit(1);
}

void
warn(char *pat,...)
{
#if defined(HAS_VPRINTF)
    va_list args;

    va_start(args, pat);
    vfprintf(stderr,pat,args);
#else
    fprintf(stderr,pat,a1,a2,a3,a4);
#endif
}

