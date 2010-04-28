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
 *	@(#)local.h	8.3 (Berkeley) 7/3/94
 * $FreeBSD$
 */

#include <sys/types.h>	/* for off_t */
#include <pthread.h>
#include <string.h>
#include <wchar.h>

/*
 * Information local to this implementation of stdio,
 * in particular, macros and private variables.
 */

extern int	_sread(FILE *, char *, int);
extern int	_swrite(FILE *, char const *, int);
extern fpos_t	_sseek(FILE *, fpos_t, int);
extern int	_ftello(FILE *, fpos_t *);
extern int	_fseeko(FILE *, off_t, int, int);
extern int	__fflush(FILE *fp);
extern void	__fcloseall(void);
extern wint_t	__fgetwc(FILE *);
extern wint_t	__fputwc(wchar_t, FILE *);
extern int	__sflush(FILE *);
extern FILE	*__sfp(void);
extern int	__slbexpand(FILE *, size_t);
extern int	__srefill(FILE *);
extern int	__sread(void *, char *, int);
extern int	__swrite(void *, char const *, int);
extern fpos_t	__sseek(void *, fpos_t, int);
extern int	__sclose(void *);
extern void	__sinit(void);
extern void	_cleanup(void);
extern void	__smakebuf(FILE *);
extern int	__swhatbuf(FILE *, size_t *, int *);
extern int	_fwalk(int (*)(FILE *));
extern int	__svfscanf(FILE *, const char *, __va_list);
extern int	__swsetup(FILE *);
extern int	__sflags(const char *, int *);
extern int	__ungetc(int, FILE *);
extern wint_t	__ungetwc(wint_t, FILE *);
extern int	__vfprintf(FILE *, const char *, __va_list);
extern int	__vfscanf(FILE *, const char *, __va_list);
extern int	__vfwprintf(FILE *, const wchar_t *, __va_list);
extern int	__vfwscanf(FILE * __restrict, const wchar_t * __restrict,
		    __va_list);
extern size_t	__fread(void * __restrict buf, size_t size, size_t count,
		FILE * __restrict fp);
extern int	__sdidinit;


/*
 * Prepare the given FILE for writing, and return 0 iff it
 * can be written now.  Otherwise, return EOF and set errno.
 */
#define	prepwrite(fp) \
 	((((fp)->_flags & __SWR) == 0 || \
 	    ((fp)->_bf._base == NULL && ((fp)->_flags & __SSTR) == 0)) && \
	 __swsetup(fp))

/*
 * Test whether the given stdio file has an active ungetc buffer;
 * release such a buffer, without restoring ordinary unread data.
 */
#define	HASUB(fp) ((fp)->_ub._base != NULL)
#define	FREEUB(fp) { \
	if ((fp)->_ub._base != (fp)->_ubuf) \
		free((char *)(fp)->_ub._base); \
	(fp)->_ub._base = NULL; \
}

/*
 * test for an fgetln() buffer.
 */
#define	HASLB(fp) ((fp)->_lb._base != NULL)
#define	FREELB(fp) { \
	free((char *)(fp)->_lb._base); \
	(fp)->_lb._base = NULL; \
}

/*
 * Structure initializations for 'fake' FILE objects.
 */
#define	FAKE_FILE {				\
	._file = -1,				\
	._fl_mutex = PTHREAD_MUTEX_INITIALIZER, \
}

/*
 * Set the orientation for a stream. If o > 0, the stream has wide-
 * orientation. If o < 0, the stream has byte-orientation.
 */
#define	ORIENT(fp, o)	do {				\
	if ((fp)->_orientation == 0)			\
		(fp)->_orientation = (o);		\
} while (0)
