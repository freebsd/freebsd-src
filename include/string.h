/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)string.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD$
 */

#ifndef _STRING_H_
#define	_STRING_H_

#include <sys/cdefs.h>
#include <sys/_types.h>

/*
 * Prototype functions which were historically defined in <string.h>, but
 * are required by POSIX to be prototyped in <strings.h>.
 */
#if __BSD_VISIBLE
#include <strings.h>
#endif

#ifndef _SIZE_T_DECLARED
typedef	__size_t	size_t;
#define	_SIZE_T_DECLARED
#endif

#ifndef	NULL
#define	NULL	0
#endif

__BEGIN_DECLS
void	*memchr(const void *, int, size_t);
int	 memcmp(const void *, const void *, size_t);
void	*memcpy(void * __restrict, const void * __restrict, size_t);
void	*memmove(void *, const void *, size_t);
void	*memset(void *, int, size_t);
char	*strcat(char * __restrict, const char * __restrict);
char	*strchr(const char *, int);
int	 strcmp(const char *, const char *);
int	 strcoll(const char *, const char *);
char	*strcpy(char * __restrict, const char * __restrict);
char	*stpcpy(char *, const char *);
size_t	 strcspn(const char *, const char *);
char	*strerror(int);
size_t	 strlen(const char *);
char	*strncat(char * __restrict, const char * __restrict, size_t);
int	 strncmp(const char *, const char *, size_t);
char	*strncpy(char * __restrict, const char * __restrict, size_t);
char	*strpbrk(const char *, const char *);
char	*strrchr(const char *, int);
size_t	 strspn(const char *, const char *);
char	*strstr(const char *, const char *);
char	*strtok(char * __restrict, const char * __restrict);
size_t	 strxfrm(char * __restrict, const char * __restrict, size_t);

#if __POSIX_VISIBLE >= 200112 || __XSI_VISIBLE >= 500
void	*memccpy(void * __restrict, const void * __restrict, int, size_t);
char	*strdup(const char *);
char	*strtok_r(char *, const char *, char **);
#endif

#if __POSIX_VISIBLE >= 200112
int	 strerror_r(int, char *, size_t);
#endif

#if __BSD_VISIBLE
char	*strcasestr(const char *, const char *);
size_t	 strlcat(char *, const char *, size_t);
size_t	 strlcpy(char *, const char *, size_t);
void	 strmode(int, char *);
char	*strnstr(const char *, const char *, size_t);
char	*strsep(char **, const char *);
char	*strsignal(int);
void	 swab(const void *, void *, size_t);
#endif
__END_DECLS

#endif /* _STRING_H_ */
