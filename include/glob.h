/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
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
 *	@(#)glob.h	5.8 (Berkeley) 12/2/92
 */

#ifndef _GLOB_H_
#define	_GLOB_H_

typedef struct {
	int gl_pathc;		/* count of total paths so far */
	int gl_matchc;		/* count of paths matching pattern */
	int gl_offs;		/* reserved at beginning of gl_pathv */
	int gl_flags;		/* copy of flags parameter to glob() */
	int (*gl_errfunc)();	/* copy of errfunc parameter to glob() */
	void *(*gl_opendir)();	/* alternate opendir() function for glob() */
	struct dirent *(*gl_readdir)();	/* alternate readdir() function */
	void (*gl_closedir)();	/* alternate closedir() function for glob() */
	int (*gl_lstat)();	/* alternate lstat() function for glob() */
	int (*gl_stat)();	/* alternate stat() function for glob() */
	char **gl_pathv;	/* list of paths matching pattern */
} glob_t;

#define	GLOB_APPEND	0x001	/* append to output from previous call */
#define	GLOB_DOOFFS	0x002	/* use gl_offs */
#define	GLOB_ERR	0x004	/* return on error */
#define	GLOB_MARK	0x008	/* append / to matching directories */
#define	GLOB_NOCHECK	0x010	/* return pattern itself if nothing matches */
#define	GLOB_NOSORT	0x020	/* don't sort */

#ifndef _POSIX_SOURCE
#define	GLOB_MAGCHAR	0x040	/* pattern had globbing characters */
#define	GLOB_NOMAGIC	0x080	/* GLOB_NOCHECK without magic chars (csh) */
#define	GLOB_QUOTE	0x100	/* quote special chars with \ */
#define	GLOB_ALTDIRFUNC	0x200	/* use alternately specified directory funcs */
#endif

#define	GLOB_NOSPACE	(-1)	/* malloc call failed */
#define	GLOB_ABEND	(-2)	/* unignored error */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	glob __P((const char *, int, int (*)(char *, int), glob_t *));
void	globfree __P((glob_t *));
__END_DECLS

#endif /* !_GLOB_H_ */
