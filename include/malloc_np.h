/*-
 * Copyright (C) 2006 Jason Evans <jasone@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MALLOC_NP_H_
#define	_MALLOC_NP_H_
#include <sys/cdefs.h>
#include <sys/types.h>
#include <strings.h>

__BEGIN_DECLS
size_t	malloc_usable_size(const void *ptr);

void	malloc_stats_print(void (*write_cb)(void *, const char *),
    void *cbopaque, const char *opts);
int	mallctl(const char *name, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen);
int	mallctlnametomib(const char *name, size_t *mibp, size_t *miblenp);
int	mallctlbymib(const size_t *mib, size_t miblen, void *oldp,
    size_t *oldlenp, void *newp, size_t newlen);

#define	ALLOCM_LG_ALIGN(la)	(la)
#define	ALLOCM_ALIGN(a)	(ffsl(a)-1)
#define	ALLOCM_ZERO	((int)0x40)
#define	ALLOCM_NO_MOVE	((int)0x80)

#define	ALLOCM_SUCCESS		0
#define	ALLOCM_ERR_OOM		1
#define	ALLOCM_ERR_NOT_MOVED	2

int	allocm(void **ptr, size_t *rsize, size_t size, int flags) __nonnull(1);
int	rallocm(void **ptr, size_t *rsize, size_t size, size_t extra,
    int flags) __nonnull(1);
int	sallocm(const void *ptr, size_t *rsize, int flags) __nonnull(1);
int	dallocm(void *ptr, int flags) __nonnull(1);
int	nallocm(size_t *rsize, size_t size, int flags);

void *	__calloc(size_t, size_t) __malloc_like;
void *	__malloc(size_t) __malloc_like;
void *	__realloc(void *, size_t);
void	__free(void *);
int	__posix_memalign(void **, size_t, size_t);
size_t	__malloc_usable_size(const void *);
int	__allocm(void **, size_t *, size_t, int) __nonnull(1);
int	__rallocm(void **, size_t *, size_t, size_t, int) __nonnull(1);
int	__sallocm(const void *, size_t *, int) __nonnull(1);
int	__dallocm(void *, int) __nonnull(1);
int	__nallocm(size_t *, size_t, int);
__END_DECLS

#endif /* _MALLOC_NP_H_ */
