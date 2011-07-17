/* $FreeBSD$ */
/* $NetBSD: iconv.c,v 1.11 2009/03/03 16:22:33 explorer Exp $ */

/*-
 * Copyright (c) 2003 Citrus Project,
 * Copyright (c) 2009, 2010 Gabor Kovesdan <gabor@FreeBSD.org>,
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
 */

#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <iconv.h>
#include <limits.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "citrus_types.h"
#include "citrus_module.h"
#include "citrus_esdb.h"
#include "citrus_hash.h"
#include "citrus_iconv.h"

#ifdef __weak_alias
__weak_alias(libiconv, _iconv)
__weak_alias(libiconv_open, _iconv_open)
__weak_alias(libiconv_open_into, _iconv_open_into)
__weak_alias(libiconv_close, _iconv_close)
__weak_alias(libiconvlist, _iconvlist)
__weak_alias(libiconvctl, _iconvctl)
__weak_alias(libiconv_set_relocation_prefix, _iconv_set_relocation_prefix)
__weak_alias(iconv_canonicalize, _iconv_canonicalize)
#endif

#define ISBADF(_h_)	(!(_h_) || (_h_) == (iconv_t)-1)

int _libiconv_version = _LIBICONV_VERSION;

iconv_t		 _iconv_open(const char *out, const char *in,
		    struct _citrus_iconv *prealloc);

iconv_t
_iconv_open(const char *out, const char *in, struct _citrus_iconv *prealloc)
{
	struct _citrus_iconv *handle;
	char *out_truncated, *p;
	int ret;

	handle = prealloc;

	/*
	 * Remove anything following a //, as these are options (like
	 * //ignore, //translate, etc) and we just don't handle them.
	 * This is for compatibilty with software that uses thees
	 * blindly.
	 */
	out_truncated = strdup(out);
	if (out_truncated == NULL) {
		errno = ENOMEM;
		return ((iconv_t)-1);
	}

	p = out_truncated;
        while (*p != 0) {
                if (p[0] == '/' && p[1] == '/') {
                        *p = '\0';
                        break;
                }
                p++;
        }

	ret = _citrus_iconv_open(&handle, in, out_truncated);
	free(out_truncated);
	if (ret) {
		errno = ret == ENOENT ? EINVAL : ret;
		return ((iconv_t)-1);
	}

	handle->cv_shared->ci_discard_ilseq = strcasestr(out, "//IGNORE");
	handle->cv_shared->ci_hooks = NULL;

	return ((iconv_t)(void *)handle);
}

iconv_t
libiconv_open(const char *out, const char *in)
{

	return (_iconv_open(out, in, NULL));
}

int
libiconv_open_into(const char *out, const char *in, iconv_allocation_t *ptr)
{
	struct _citrus_iconv *handle;

	handle = (struct _citrus_iconv *)ptr;
	return ((_iconv_open(out, in, handle) == (iconv_t)-1) ? -1 : 0);
}

int
libiconv_close(iconv_t handle)
{

	if (ISBADF(handle)) {
		errno = EBADF;
		return (-1);
	}

	_citrus_iconv_close((struct _citrus_iconv *)(void *)handle);

	return (0);
}

size_t
libiconv(iconv_t handle, char **in, size_t *szin, char **out, size_t *szout)
{
	size_t ret;
	int err;

	if (ISBADF(handle)) {
		errno = EBADF;
		return ((size_t)-1);
	}

	err = _citrus_iconv_convert((struct _citrus_iconv *)(void *)handle,
	    in, szin, out, szout, 0, &ret);
	if (err) {
		errno = err;
		ret = (size_t)-1;
	}

	return (ret);
}

size_t
__iconv(iconv_t handle, char **in, size_t *szin, char **out,
    size_t *szout, uint32_t flags, size_t *invalids)
{
	size_t ret;
	int err;

	if (ISBADF(handle)) {
		errno = EBADF;
		return ((size_t)-1);
	}

	err = _citrus_iconv_convert((struct _citrus_iconv *)(void *)handle,
	    in, szin, out, szout, flags, &ret);
	if (invalids)
		*invalids = ret;
	if (err) {
		errno = err;
		ret = (size_t)-1;
	}

	return (ret);
}

int
__iconv_get_list(char ***rlist, size_t *rsz, bool sorted)
{
	int ret;

	ret = _citrus_esdb_get_list(rlist, rsz, sorted);
	if (ret) {
		errno = ret;
		return (-1);
	}

	return (0);
}

void
__iconv_free_list(char **list, size_t sz)
{

	_citrus_esdb_free_list(list, sz);
}

/*
 * GNU-compatibile non-standard interfaces.
 */
static int
qsort_helper(const void *first, const void *second)
{
	const char * const *s1;
	const char * const *s2;

	s1 = first;
	s2 = second;
	return (strcmp(*s1, *s2));
}

void
libiconvlist(int (*do_one) (unsigned int, const char * const *,
    void *), void *data)
{
	char **list, **names;
	const char * const *np;
	char *curitem, *curkey, *slashpos;
	size_t sz;
	unsigned int i, j;

	i = 0;

	if (__iconv_get_list(&list, &sz, true))
		list = NULL;
	qsort((void *)list, sz, sizeof(char *), qsort_helper);
	while (i < sz) {
		j = 0;
		slashpos = strchr(list[i], '/');
		curkey = (char *)malloc(slashpos - list[i] + 2);
		names = (char **)malloc(sz * sizeof(char *));
		if ((curkey == NULL) || (names == NULL)) {
			__iconv_free_list(list, sz);
			return;
		}
		strlcpy(curkey, list[i], slashpos - list[i] + 1);
		names[j++] = strdup(curkey);
		for (; (i < sz) && (memcmp(curkey, list[i], strlen(curkey)) == 0); i++) {
			slashpos = strchr(list[i], '/');
			curitem = (char *)malloc(strlen(slashpos) + 1);
			if (curitem == NULL) {
				__iconv_free_list(list, sz);
				return;
			}
			strlcpy(curitem, &slashpos[1], strlen(slashpos) + 1);
			if (strcmp(curkey, curitem) == 0) {
				continue;
			}
			names[j++] = strdup(curitem);
		}
		np = (const char * const *)names;
		do_one(j, np, data);
		free(names);
	}

	__iconv_free_list(list, sz);
}

__inline const char
*iconv_canonicalize(const char *name)
{

	return (_citrus_iconv_canonicalize(name));
}

int
libiconvctl(iconv_t cd, int request, void *argument)
{
	struct _citrus_iconv *cv;
	struct iconv_hooks *hooks;
	const char *convname;
	char src[PATH_MAX], *dst;
	int *i;

	cv = (struct _citrus_iconv *)(void *)cd;
	hooks = (struct iconv_hooks *)argument;
	i = (int *)argument;

	if (ISBADF(cd)) {
		errno = EBADF;
		return (-1);
	}

	switch (request) {
	case ICONV_TRIVIALP:
		convname = cv->cv_shared->ci_convname;
		dst = strchr(convname, '/');

		strlcpy(src, convname, dst - convname + 1);
		dst++;
		if ((convname == NULL) || (src == NULL) || (dst == NULL))
			return (-1);
		*i = strcmp(src, dst) == 0 ? 1 : 0;
		return (0);
	case ICONV_GET_TRANSLITERATE:
		*i = 1;
		return (0);
	case ICONV_SET_TRANSLITERATE:
		return  ((*i == 1) ? 0 : -1);
	case ICONV_GET_DISCARD_ILSEQ:
		*i = cv->cv_shared->ci_discard_ilseq ? 1 : 0;
		return (0);
	case ICONV_SET_DISCARD_ILSEQ:
		cv->cv_shared->ci_discard_ilseq = *i;
		return (0);
	case ICONV_SET_HOOKS:
		cv->cv_shared->ci_hooks = hooks;
		return (0);
	case ICONV_SET_FALLBACKS:
		errno = EOPNOTSUPP;
		return (-1);
	default:
		errno = EINVAL;
		return (-1);
	}
}

void
libiconv_set_relocation_prefix(const char *orig_prefix __unused,
    const char *curr_prefix __unused)
{

}
