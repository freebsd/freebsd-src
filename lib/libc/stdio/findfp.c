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
#if 0
static char sccsid[] = "@(#)findfp.c	8.2 (Berkeley) 1/4/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <libc_private.h>
#include <spinlock.h>

#include "local.h"
#include "glue.h"

int	__sdidinit;

#define	NDYNAMIC 10		/* add ten more whenever necessary */

#define	std(flags, file) \
	{0,0,0,flags,file,{0},0,__sF+file,__sclose,__sread,__sseek,__swrite}
/*	 p r w flags file _bf z  cookie      close    read    seek    write */

				/* the usual - (stdin + stdout + stderr) */
static FILE usual[FOPEN_MAX - 3];
static struct glue uglue = { 0, FOPEN_MAX - 3, usual };

FILE __sF[3] = {
	std(__SRD, STDIN_FILENO),		/* stdin */
	std(__SWR, STDOUT_FILENO),		/* stdout */
	std(__SWR|__SNBF, STDERR_FILENO)	/* stderr */
};
struct glue __sglue = { &uglue, 3, __sF };

/*
 * The following kludge is done to ensure enough binary compatibility
 * with future versions of libc.  Or rather it allows us to work with
 * libraries that have been built with a newer libc that defines these
 * symbols and expects libc to provide them.  We only have need to support
 * i386 and alpha because they are the only "old" systems we have deployed.
 */
#if defined(__i386__)
#define FILE_SIZE 88
#elif defined(__alpha__)
#define FILE_SIZE 152
#endif
#ifndef FILE_SIZE
#error "You must define FILE_SIZE for this platform"
#endif
#define X(loc, sym)	__strong_reference(loc, sym)
X(__sF + FILE_SIZE * 0, __stdin);
X(__sF + FILE_SIZE * 1, __stdout);
X(__sF + FILE_SIZE * 2, __stderr);

static struct glue *	moreglue __P((int));

static spinlock_t thread_lock = _SPINLOCK_INITIALIZER;
#define THREAD_LOCK()	if (__isthreaded) _SPINLOCK(&thread_lock)
#define THREAD_UNLOCK()	if (__isthreaded) _SPINUNLOCK(&thread_lock)

static struct glue *
moreglue(n)
	register int n;
{
	register struct glue *g;
	register FILE *p;
	static FILE empty;

	g = (struct glue *)malloc(sizeof(*g) + ALIGNBYTES + n * sizeof(FILE));
	if (g == NULL)
		return (NULL);
	p = (FILE *)ALIGN(g + 1);
	g->next = NULL;
	g->niobs = n;
	g->iobs = p;
	while (--n >= 0)
		*p++ = empty;
	return (g);
}

/*
 * Find a free FILE for fopen et al.
 */
FILE *
__sfp()
{
	register FILE *fp;
	register int n;
	register struct glue *g;

	if (!__sdidinit)
		__sinit();
	THREAD_LOCK();
	for (g = &__sglue;; g = g->next) {
		for (fp = g->iobs, n = g->niobs; --n >= 0; fp++)
			if (fp->_flags == 0)
				goto found;
		if (g->next == NULL && (g->next = moreglue(NDYNAMIC)) == NULL)
			break;
	}
	THREAD_UNLOCK();
	return (NULL);
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
	register struct glue *g;
	int n;

	n = getdtablesize() - FOPEN_MAX + 20;		/* 20 for slop. */
	for (g = &__sglue; (n -= g->niobs) > 0 && g->next; g = g->next)
		/* void */;
	if (n > 0)
		g->next = moreglue(n);
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
	/* make sure we clean up on exit */
	__cleanup = _cleanup;		/* conservative */
	__sdidinit = 1;
}
