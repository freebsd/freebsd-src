/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _MISC_H_
#define _MISC_H_

#include <openssl/md5.h>

#include <sys/types.h>

/* If we're not compiling in a C99 environment, define the C99 types. */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901

#ifdef uint32_t
#undef uint32_t
#endif
#define	uint32_t	u_int32_t

#ifdef uint16_t
#undef uint16_t
#endif
#define	uint16_t	u_int16_t

#ifdef uint8_t
#undef uint8_t
#endif
#define	uint8_t		u_int8_t

#else
#include <stdint.h>
#endif

/* This is a GCC-specific keyword but some other compilers (namely icc)
   understand it, and the code won't work if we can't disable padding
   anyways. */
#undef __packed
#define	__packed		__attribute__((__packed__))

/* We explicitely don't define this with icc because it defines __GNUC__
   but doesn't support it. */
#undef __printflike
#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && \
    (__GNUC__ > 2 || __GNUC__ == 2 && __GNUC__MINOR__ >= 7)
#define	__printflike(fmtarg, firstvararg) \
	    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define	__printflike(fmtarg, firstvararg)
#endif

/* Exit codes. */
#define	STATUS_SUCCESS		0
#define	STATUS_FAILURE		1
#define	STATUS_TRANSIENTFAILURE	2
#define	STATUS_INTERRUPTED	3

struct config;
struct stream;

/* Thread parameters. */
struct thread_args {
	struct config *config;
	struct stream *rd;
	struct stream *wr;
	int status;
	char *errmsg;
};

/* Minimum size for MD5_File() and MD5_End() buffers. */
#define	MD5_DIGEST_SIZE		33

#define	min(a, b)		((a) > (b) ? (b) : (a))
#define	max(a, b)		((a) < (b) ? (b) : (a))

struct backoff_timer;
struct pattlist;
struct tm;

int	 asciitoint(const char *, int *, int);
int	 lprintf(int, const char *, ...) __printflike(2, 3);
int	 MD5_File(char *, char *);
void	 MD5_End(char *, MD5_CTX *);
int	 rcsdatetotm(const char *, struct tm *);
time_t	 rcsdatetotime(const char *);
int	 pathcmp(const char *, const char *);
size_t	 commonpathlength(const char *, size_t, const char *, size_t);
char	*pathlast(char *);
char	*checkoutpath(const char *, const char *);
int	 mkdirhier(char *, mode_t);
char	*tempname(const char *);
void	*xmalloc(size_t);
void	*xrealloc(void *, size_t);
char	*xstrdup(const char *);
int	 xasprintf(char **, const char *, ...) __printflike(2, 3);

struct pattlist		*pattlist_new(void);
void			 pattlist_add(struct pattlist *, const char *);
char			*pattlist_get(struct pattlist *, size_t);
size_t			 pattlist_size(struct pattlist *);
void			 pattlist_free(struct pattlist *);

struct backoff_timer	*bt_new(time_t, time_t, float, float);
time_t			 bt_get(struct backoff_timer *);
void			 bt_pause(struct backoff_timer *);
void			 bt_free(struct backoff_timer *);

#endif /* !_MISC_H_ */
