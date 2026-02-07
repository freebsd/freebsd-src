/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 *
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#ifndef _STDIO_LOCAL_H
#define	_STDIO_LOCAL_H

#include <sys/types.h>	/* for off_t */
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
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
extern wint_t	__fgetwc_mbs(FILE *, mbstate_t *, int *, locale_t);
extern wint_t	__fputwc(wchar_t, FILE *, locale_t);
extern int	__sflush(FILE *);
extern FILE	*__sfp(void);
extern int	__slbexpand(FILE *, size_t);
extern int	__srefill(FILE *);
extern int	__sread(void *, char *, int);
extern int	__swrite(void *, char const *, int);
extern fpos_t	__sseek(void *, fpos_t, int);
extern int	__sclose(void *);
extern void	_cleanup(void);
extern void	__smakebuf(FILE *);
extern int	__swhatbuf(FILE *, size_t *, int *);
extern int	_fwalk(int (*)(FILE *));
extern int	__svfscanf(FILE *, locale_t, const char *, __va_list);
extern int	__swsetup(FILE *);
extern int	__sflags(const char *, int *);
extern int	__ungetc(int, FILE *);
extern wint_t	__ungetwc(wint_t, FILE *, locale_t);
extern int	__vfprintf(FILE *, locale_t, int, const char *, __va_list);
extern int	__vfscanf(FILE *, const char *, __va_list);
extern int	__vfwprintf(FILE *, locale_t, const wchar_t *, __va_list);
extern int	__vfwscanf(FILE * __restrict, locale_t, const wchar_t * __restrict,
		    __va_list);
extern size_t	__fread(void * __restrict buf, size_t size, size_t count,
		FILE * __restrict fp);

extern bool	__stdio_force_short_fildes;

static inline wint_t
__fgetwc(FILE *fp, locale_t locale)
{
	int nread;

	return (__fgetwc_mbs(fp, &fp->_mbstate, &nread, locale));
}

static inline bool
__sforce_short_fildes(bool short_only)
{
	return (short_only || __stdio_force_short_fildes);
}

/*
 * The following macros and functions support encoding 32-bit
 * file descriptors in a backward compatible way in FILE objects
 * using the existing 16-bit _file field as well as a 16-bit
 * slice of the _flags2 field.
 *
 * A signed 32-bit file descriptor 'F' is encoded into two 16-bit
 * parts 'L' and 'H' as follows:
 *
 * 'L' is simply the lower sixteen bits of 'F':
 *
 *     short L = (short)F
 *
 * 'H' is the upper sixteen bits of 'F' exclusive or'd with the fifteenth
 * bit of 'F' (i.e. the sign bit of 'L'), or equivalently, the upper
 * sixteen bits of the result of exclusive or'ing 'F' with the lower
 * sixteen bits of 'F' sign extended to thirty two bits.
 *
 *     short H = (short)((F ^ (int)(short)F) >> 16)
 *
 * 'L' is stored in FILE->_file and 'H' is stored in a new 16-bit slice
 * in FILE->_flags2. Note that for values of 'F' between SHRT_MIN and
 * SHRT_MAX, 'H' will be zero and thus this encoding has the advantage
 * of preserving binary encodings of FILE->_file and FILE->_flags2 for
 * file descriptors in this range.
 */
#define __S2FDX_SHFT			(1)
#define __S2FDX_EXTRACT(_flags2)	\
	    ((int)((unsigned)((_flags2) & __S2FDX) >> __S2FDX_SHFT))

#define __S2FDX_INSERT(_flags2, val)	\
	    ((_flags2) & (~__S2FDX) | (__S2FDX & ((val) << __S2FDX_SHFT)))

#define __SFD_TO_LOW(fd)		((short)(fd))
#define __SFD_TO_HSX(fd)		\
	    ((unsigned)((fd) ^ (int)(short)(fd)) >> 16)

#define __SLOW_HSX_TO_FD(low, hsx)	((int)(short)(low) ^ ((int)(hsx) << 16))

_Static_assert(USHRT_MAX << __S2FDX_SHFT	== __S2FDX,	 "__S2FDX");
_Static_assert(__S2FDX_EXTRACT(__S2FDX)		== USHRT_MAX,	 "__S2FDX");
_Static_assert(__S2FDX_INSERT(~0, 0)		== ~__S2FDX,	 "__S2FDX");
_Static_assert(__SFD_TO_LOW(SHRT_MIN - 1)	== SHRT_MAX,	 "__S2FDX");
_Static_assert(__SFD_TO_LOW(SHRT_MIN)		== SHRT_MIN,	 "__S2FDX");
_Static_assert(__SFD_TO_LOW(-1)			== -1,		 "__S2FDX");
_Static_assert(__SFD_TO_LOW(0)			== 0,		 "__S2FDX");
_Static_assert(__SFD_TO_LOW(SHRT_MAX)		== SHRT_MAX,	 "__S2FDX");
_Static_assert(__SFD_TO_LOW(SHRT_MAX + 1)	== SHRT_MIN,	 "__S2FDX");
_Static_assert(__SFD_TO_HSX(SHRT_MIN - 1)	== USHRT_MAX,	 "__S2FDX");
_Static_assert(__SFD_TO_HSX(SHRT_MIN)		== 0,		 "__S2FDX");
_Static_assert(__SFD_TO_HSX(-1)			== 0,		 "__S2FDX");
_Static_assert(__SFD_TO_HSX(0)			== 0,		 "__S2FDX");
_Static_assert(__SFD_TO_HSX(SHRT_MAX)		== 0,		 "__S2FDX");
_Static_assert(__SFD_TO_HSX(SHRT_MAX + 1)	== USHRT_MAX,	 "__S2FDX");
_Static_assert(__SLOW_HSX_TO_FD(0, SHRT_MIN)	== INT_MIN,	 "__S2FDX");
_Static_assert(__SLOW_HSX_TO_FD(SHRT_MIN, 0)	== SHRT_MIN,	 "__S2FDX");
_Static_assert(__SLOW_HSX_TO_FD(-1, 0)		== -1,		 "__S2FDX");
_Static_assert(__SLOW_HSX_TO_FD(0, 0)		== 0,		 "__S2FDX");
_Static_assert(__SLOW_HSX_TO_FD(SHRT_MAX, 0)	== SHRT_MAX,	 "__S2FDX");
_Static_assert(__SLOW_HSX_TO_FD(SHRT_MIN, -1)	== SHRT_MAX + 1, "__S2FDX");
_Static_assert(__SLOW_HSX_TO_FD(-1, SHRT_MIN)	== INT_MAX,	 "__S2FDX");

static inline int
__sfileno(const FILE *fp)
{
	int fd;

	if (__stdio_force_short_fildes)
		fd = fp->_file;
	else {
		fd = __S2FDX_EXTRACT(fp->_flags2);
		fd = __SLOW_HSX_TO_FD(fp->_file, fd);
	}
	return (fd);
}

static inline void
__sfileno_set(FILE *fp, int fd)
{
	if (__stdio_force_short_fildes)
		fp->_file = (unsigned)fd > SHRT_MAX ? -1 : (short)fd;
	else {
		fp->_file = __SFD_TO_LOW(fd);
		fp->_flags2 = __S2FDX_INSERT(fp->_flags2, __SFD_TO_HSX(fd));
	}
}

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

void __stdio_cancel_cleanup(void *);
#define	FLOCKFILE_CANCELSAFE(fp)					\
	{								\
		struct _pthread_cleanup_info __cleanup_info__;		\
		if (__isthreaded) {					\
			_FLOCKFILE(fp);					\
			___pthread_cleanup_push_imp(			\
			    __stdio_cancel_cleanup, (fp), 		\
			    &__cleanup_info__);				\
		} else {						\
			___pthread_cleanup_push_imp(			\
			    __stdio_cancel_cleanup, NULL, 		\
			    &__cleanup_info__);				\
		}							\
		{
#define	FUNLOCKFILE_CANCELSAFE()					\
			(void)0;					\
		}							\
		___pthread_cleanup_pop_imp(1);				\
	}

#endif /* _STDIO_LOCAL_H */
