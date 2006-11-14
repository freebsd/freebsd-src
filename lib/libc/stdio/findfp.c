/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)findfp.c	8.2 (Berkeley) 1/4/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <machine/atomic.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spinlock.h>

#include "libc_private.h"
#include "local.h"
#include "glue.h"

int	__sdidinit;

#define	NDYNAMIC 10		/* add ten more whenever necessary */

#define	std(flags, file) \
  	{0,0,0,flags,file,{0},0,__sF+file,__sclose,__sread,__sseek,__swrite, \
	 {0}, __sFX + file}
  /*	 p r w flags file _bf z  cookie      close    read    seek    write */
  /*     _ub _extra */
				/* the usual - (stdin + stdout + stderr) */
static FILE usual[FOPEN_MAX - 3];
static struct __sFILEX usual_extra[FOPEN_MAX - 3];
static struct glue uglue = { NULL, FOPEN_MAX - 3, usual };

static struct __sFILEX __sFX[3];

/*
 * We can't make this 'static' until 6.0-current due to binary
 * compatibility concerns.  This also means we cannot change the
 * sizeof(FILE) until that time either and must continue to use the
 * __sFILEX stuff to add to FILE.
 */
FILE __sF[3] = {
	std(__SRD, STDIN_FILENO),
	std(__SWR, STDOUT_FILENO),
	std(__SWR|__SNBF, STDERR_FILENO)
};

/*
 * The following kludge is done to ensure enough binary compatibility
 * with future versions of libc.  Or rather it allows us to work with
 * libraries that have been built with a newer libc that defines these
 * symbols and expects libc to provide them.  We only have need to support
 * i386 and alpha because they are the only "old" systems we have deployed.
 */
FILE *__stdinp = &__sF[0];
FILE *__stdoutp = &__sF[1];
FILE *__stderrp = &__sF[2];

struct glue __sglue = { &uglue, 3, __sF };
static struct glue *lastglue = &uglue;

static struct glue *	moreglue(int);

static spinlock_t thread_lock = _SPINLOCK_INITIALIZER;
#define THREAD_LOCK()	if (__isthreaded) _SPINLOCK(&thread_lock)
#define THREAD_UNLOCK()	if (__isthreaded) _SPINUNLOCK(&thread_lock)

#if NOT_YET
#define	SET_GLUE_PTR(ptr, val)	atomic_set_rel_ptr(&(ptr), (uintptr_t)(val))
#else
#define	SET_GLUE_PTR(ptr, val)	ptr = val
#endif

static struct glue *
moreglue(n)
	int n;
{
	struct glue *g;
	static FILE empty;
	static struct __sFILEX emptyx;
	FILE *p;
	struct __sFILEX *fx;

	g = (struct glue *)malloc(sizeof(*g) + ALIGNBYTES + n * sizeof(FILE) +
	    n * sizeof(struct __sFILEX));
	if (g == NULL)
		return (NULL);
	p = (FILE *)ALIGN(g + 1);
	fx = (struct __sFILEX *)&p[n];
	g->next = NULL;
	g->niobs = n;
	g->iobs = p;
	while (--n >= 0) {
		*p = empty;
		p->_extra = fx;
		*p->_extra = emptyx;
		p++, fx++;
	}
	return (g);
}

/*
 * Find a free FILE for fopen et al.
 */
FILE *
__sfp()
{
	FILE	*fp;
	int	n;
	struct glue *g;

	if (!__sdidinit)
		__sinit();
	/*
	 * The list must be locked because a FILE may be updated.
	 */
	THREAD_LOCK();
	for (g = &__sglue; g != NULL; g = g->next) {
		for (fp = g->iobs, n = g->niobs; --n >= 0; fp++)
			if (fp->_flags == 0)
				goto found;
	}
	THREAD_UNLOCK();	/* don't hold lock while malloc()ing. */
	if ((g = moreglue(NDYNAMIC)) == NULL)
		return (NULL);
	THREAD_LOCK();		/* reacquire the lock */
	SET_GLUE_PTR(lastglue->next, g); /* atomically append glue to list */
	lastglue = g;		/* not atomic; only accessed when locked */
	fp = g->iobs;
found:
	fp->_flags = 1;		/* reserve this slot; caller sets real flags */
	THREAD_UNLOCK();
	fp->_p = NULL;		/* no current pointer */
	fp->_w = 0;		/* nothing to read or write */
	fp->_r = 0;
	fp->_bf._base = NULL;	/* no buffer */
	fp->_bf._size = 0;
	fp->_lbfsize = 0;	/* not line buffered */
	fp->_file = -1;		/* no file */
/*	fp->_cookie = <any>; */	/* caller sets cookie, _read/_write etc */
	fp->_ub._base = NULL;	/* no ungetc buffer */
	fp->_ub._size = 0;
	fp->_lb._base = NULL;	/* no line buffer */
	fp->_lb._size = 0;
/*	fp->_lock = NULL; */	/* once set always set (reused) */
	fp->_extra->orientation = 0;
	memset(&fp->_extra->mbstate, 0, sizeof(mbstate_t));
	return (fp);
}

/*
 * XXX.  Force immediate allocation of internal memory.  Not used by stdio,
 * but documented historically for certain applications.  Bad applications.
 */
__warn_references(f_prealloc, 
	"warning: this program uses f_prealloc(), which is not recommended.");

void
f_prealloc()
{
	struct glue *g;
	int n;

	n = getdtablesize() - FOPEN_MAX + 20;		/* 20 for slop. */
	/*
	 * It should be safe to walk the list without locking it;
	 * new nodes are only added to the end and none are ever
	 * removed.
	 */
	for (g = &__sglue; (n -= g->niobs) > 0 && g->next; g = g->next)
		/* void */;
	if ((n > 0) && ((g = moreglue(n)) != NULL)) {
		THREAD_LOCK();
		SET_GLUE_PTR(lastglue->next, g);
		lastglue = g;
		THREAD_UNLOCK();
	}
}

/*
 * exit() calls _cleanup() through *__cleanup, set whenever we
 * open or buffer a file.  This chicanery is done so that programs
 * that do not use stdio need not link it all in.
 *
 * The name `_cleanup' is, alas, fairly well known outside stdio.
 */
void
_cleanup()
{
	/* (void) _fwalk(fclose); */
	(void) _fwalk(__sflush);		/* `cheating' */
}

/*
 * __sinit() is called whenever stdio's internal variables must be set up.
 */
void
__sinit()
{
	int	i;

	THREAD_LOCK();
	if (__sdidinit == 0) {
		/* Set _extra for the usual suspects. */
		for (i = 0; i < FOPEN_MAX - 3; i++)
			usual[i]._extra = &usual_extra[i];

		/* Make sure we clean up on exit. */
		__cleanup = _cleanup;		/* conservative */
		__sdidinit = 1;
	}
	THREAD_UNLOCK();
}
