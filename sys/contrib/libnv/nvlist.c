/*-
 * Copyright (c) 2009-2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/queue.h>

#ifdef _KERNEL

#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/stdarg.h>

#else
#include <sys/socket.h>

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#define	_WITH_DPRINTF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "msgio.h"
#endif

#ifdef HAVE_PJDLOG
#include <pjdlog.h>
#endif

#include <sys/nv.h>

#include "nv_impl.h"
#include "nvlist_impl.h"
#include "nvpair_impl.h"

#ifndef	HAVE_PJDLOG
#ifdef _KERNEL
#define	PJDLOG_ASSERT(...)		MPASS(__VA_ARGS__)
#define	PJDLOG_RASSERT(expr, ...)	KASSERT(expr, (__VA_ARGS__))
#define	PJDLOG_ABORT(...)		panic(__VA_ARGS__)
#else
#include <assert.h>
#define	PJDLOG_ASSERT(...)		assert(__VA_ARGS__)
#define	PJDLOG_RASSERT(expr, ...)	assert(expr)
#define	PJDLOG_ABORT(...)		do {				\
	fprintf(stderr, "%s:%u: ", __FILE__, __LINE__);			\
	fprintf(stderr, __VA_ARGS__);					\
	fprintf(stderr, "\n");						\
	abort();							\
} while (0)
#endif
#endif

#define	NV_FLAG_PRIVATE_MASK	(NV_FLAG_BIG_ENDIAN)
#define	NV_FLAG_PUBLIC_MASK	(NV_FLAG_IGNORE_CASE | NV_FLAG_NO_UNIQUE)
#define	NV_FLAG_ALL_MASK	(NV_FLAG_PRIVATE_MASK | NV_FLAG_PUBLIC_MASK)

#define	NVLIST_MAGIC	0x6e766c	/* "nvl" */
struct nvlist {
	int		 nvl_magic;
	int		 nvl_error;
	int		 nvl_flags;
	nvpair_t	*nvl_parent;
	struct nvl_head	 nvl_head;
};

#define	NVLIST_ASSERT(nvl)	do {					\
	PJDLOG_ASSERT((nvl) != NULL);					\
	PJDLOG_ASSERT((nvl)->nvl_magic == NVLIST_MAGIC);		\
} while (0)

#ifdef _KERNEL
MALLOC_DEFINE(M_NVLIST, "nvlist", "kernel nvlist");
#endif

#define	NVPAIR_ASSERT(nvp)	nvpair_assert(nvp)

#define	NVLIST_HEADER_MAGIC	0x6c
#define	NVLIST_HEADER_VERSION	0x00
struct nvlist_header {
	uint8_t		nvlh_magic;
	uint8_t		nvlh_version;
	uint8_t		nvlh_flags;
	uint64_t	nvlh_descriptors;
	uint64_t	nvlh_size;
} __packed;

nvlist_t *
nvlist_create(int flags)
{
	nvlist_t *nvl;

	PJDLOG_ASSERT((flags & ~(NV_FLAG_PUBLIC_MASK)) == 0);

	nvl = nv_malloc(sizeof(*nvl));
	if (nvl == NULL)
		return (NULL);
	nvl->nvl_error = 0;
	nvl->nvl_flags = flags;
	nvl->nvl_parent = NULL;
	TAILQ_INIT(&nvl->nvl_head);
	nvl->nvl_magic = NVLIST_MAGIC;

	return (nvl);
}

void
nvlist_destroy(nvlist_t *nvl)
{
	nvpair_t *nvp;

	if (nvl == NULL)
		return;

	ERRNO_SAVE();

	NVLIST_ASSERT(nvl);

	while ((nvp = nvlist_first_nvpair(nvl)) != NULL) {
		nvlist_remove_nvpair(nvl, nvp);
		nvpair_free(nvp);
	}
	nvl->nvl_magic = 0;
	nv_free(nvl);

	ERRNO_RESTORE();
}

void
nvlist_set_error(nvlist_t *nvl, int error)
{

	PJDLOG_ASSERT(error != 0);

	/*
	 * Check for error != 0 so that we don't do the wrong thing if somebody
	 * tries to abuse this API when asserts are disabled.
	 */
	if (nvl != NULL && error != 0 && nvl->nvl_error == 0)
		nvl->nvl_error = error;
}

int
nvlist_error(const nvlist_t *nvl)
{

	if (nvl == NULL)
		return (ENOMEM);

	NVLIST_ASSERT(nvl);

	return (nvl->nvl_error);
}

nvpair_t *
nvlist_get_nvpair_parent(const nvlist_t *nvl)
{

	NVLIST_ASSERT(nvl);

	return (nvl->nvl_parent);
}

const nvlist_t *
nvlist_get_parent(const nvlist_t *nvl, void **cookiep)
{
	nvpair_t *nvp;

	NVLIST_ASSERT(nvl);

	nvp = nvl->nvl_parent;
	if (cookiep != NULL)
		*cookiep = nvp;
	if (nvp == NULL)
		return (NULL);

	return (nvpair_nvlist(nvp));
}

void
nvlist_set_parent(nvlist_t *nvl, nvpair_t *parent)
{

	NVLIST_ASSERT(nvl);

	nvl->nvl_parent = parent;
}

bool
nvlist_empty(const nvlist_t *nvl)
{

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);

	return (nvlist_first_nvpair(nvl) == NULL);
}

int
nvlist_flags(const nvlist_t *nvl)
{

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT((nvl->nvl_flags & ~(NV_FLAG_PUBLIC_MASK)) == 0);

	return (nvl->nvl_flags);
}

static void
nvlist_report_missing(int type, const char *name)
{

	PJDLOG_ABORT("Element '%s' of type %s doesn't exist.",
	    name, nvpair_type_string(type));
}

static nvpair_t *
nvlist_find(const nvlist_t *nvl, int type, const char *name)
{
	nvpair_t *nvp;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(type == NV_TYPE_NONE ||
	    (type >= NV_TYPE_FIRST && type <= NV_TYPE_LAST));

	for (nvp = nvlist_first_nvpair(nvl); nvp != NULL;
	    nvp = nvlist_next_nvpair(nvl, nvp)) {
		if (type != NV_TYPE_NONE && nvpair_type(nvp) != type)
			continue;
		if ((nvl->nvl_flags & NV_FLAG_IGNORE_CASE) != 0) {
			if (strcasecmp(nvpair_name(nvp), name) != 0)
				continue;
		} else {
			if (strcmp(nvpair_name(nvp), name) != 0)
				continue;
		}
		break;
	}

	if (nvp == NULL)
		ERRNO_SET(ENOENT);

	return (nvp);
}

bool
nvlist_exists_type(const nvlist_t *nvl, const char *name, int type)
{

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(type == NV_TYPE_NONE ||
	    (type >= NV_TYPE_FIRST && type <= NV_TYPE_LAST));

	return (nvlist_find(nvl, type, name) != NULL);
}

void
nvlist_free_type(nvlist_t *nvl, const char *name, int type)
{
	nvpair_t *nvp;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(type == NV_TYPE_NONE ||
	    (type >= NV_TYPE_FIRST && type <= NV_TYPE_LAST));

	nvp = nvlist_find(nvl, type, name);
	if (nvp != NULL)
		nvlist_free_nvpair(nvl, nvp);
	else
		nvlist_report_missing(type, name);
}

nvlist_t *
nvlist_clone(const nvlist_t *nvl)
{
	nvlist_t *newnvl;
	nvpair_t *nvp, *newnvp;

	NVLIST_ASSERT(nvl);

	if (nvl->nvl_error != 0) {
		ERRNO_SET(nvl->nvl_error);
		return (NULL);
	}

	newnvl = nvlist_create(nvl->nvl_flags & NV_FLAG_PUBLIC_MASK);
	for (nvp = nvlist_first_nvpair(nvl); nvp != NULL;
	    nvp = nvlist_next_nvpair(nvl, nvp)) {
		newnvp = nvpair_clone(nvp);
		if (newnvp == NULL)
			break;
		nvlist_move_nvpair(newnvl, newnvp);
	}
	if (nvp != NULL) {
		nvlist_destroy(newnvl);
		return (NULL);
	}
	return (newnvl);
}

#ifndef _KERNEL
static bool
nvlist_dump_error_check(const nvlist_t *nvl, int fd, int level)
{

	if (nvlist_error(nvl) != 0) {
		dprintf(fd, "%*serror: %d\n", level * 4, "",
		    nvlist_error(nvl));
		return (true);
	}

	return (false);
}

/*
 * Dump content of nvlist.
 */
void
nvlist_dump(const nvlist_t *nvl, int fd)
{
	const nvlist_t *tmpnvl;
	nvpair_t *nvp, *tmpnvp;
	void *cookie;
	int level;

	level = 0;
	if (nvlist_dump_error_check(nvl, fd, level))
		return;

	nvp = nvlist_first_nvpair(nvl);
	while (nvp != NULL) {
		dprintf(fd, "%*s%s (%s):", level * 4, "", nvpair_name(nvp),
		    nvpair_type_string(nvpair_type(nvp)));
		switch (nvpair_type(nvp)) {
		case NV_TYPE_NULL:
			dprintf(fd, " null\n");
			break;
		case NV_TYPE_BOOL:
			dprintf(fd, " %s\n", nvpair_get_bool(nvp) ?
			    "TRUE" : "FALSE");
			break;
		case NV_TYPE_NUMBER:
			dprintf(fd, " %ju (%jd) (0x%jx)\n",
			    (uintmax_t)nvpair_get_number(nvp),
			    (intmax_t)nvpair_get_number(nvp),
			    (uintmax_t)nvpair_get_number(nvp));
			break;
		case NV_TYPE_STRING:
			dprintf(fd, " [%s]\n", nvpair_get_string(nvp));
			break;
		case NV_TYPE_NVLIST:
			dprintf(fd, "\n");
			tmpnvl = nvpair_get_nvlist(nvp);
			if (nvlist_dump_error_check(tmpnvl, fd, level + 1))
				break;
			tmpnvp = nvlist_first_nvpair(tmpnvl);
			if (tmpnvp != NULL) {
				nvl = tmpnvl;
				nvp = tmpnvp;
				level++;
				continue;
			}
			break;
		case NV_TYPE_DESCRIPTOR:
			dprintf(fd, " %d\n", nvpair_get_descriptor(nvp));
			break;
		case NV_TYPE_BINARY:
		    {
			const unsigned char *binary;
			unsigned int ii;
			size_t size;

			binary = nvpair_get_binary(nvp, &size);
			dprintf(fd, " %zu ", size);
			for (ii = 0; ii < size; ii++)
				dprintf(fd, "%02hhx", binary[ii]);
			dprintf(fd, "\n");
			break;
		    }
		default:
			PJDLOG_ABORT("Unknown type: %d.", nvpair_type(nvp));
		}

		while ((nvp = nvlist_next_nvpair(nvl, nvp)) == NULL) {
			cookie = NULL;
			nvl = nvlist_get_parent(nvl, &cookie);
			if (nvl == NULL)
				return;
			nvp = cookie;
			level--;
		}
	}
}

void
nvlist_fdump(const nvlist_t *nvl, FILE *fp)
{

	fflush(fp);
	nvlist_dump(nvl, fileno(fp));
}
#endif

/*
 * The function obtains size of the nvlist after nvlist_pack().
 */
size_t
nvlist_size(const nvlist_t *nvl)
{
	const nvlist_t *tmpnvl;
	const nvpair_t *nvp, *tmpnvp;
	void *cookie;
	size_t size;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);

	size = sizeof(struct nvlist_header);
	nvp = nvlist_first_nvpair(nvl);
	while (nvp != NULL) {
		size += nvpair_header_size();
		size += strlen(nvpair_name(nvp)) + 1;
		if (nvpair_type(nvp) == NV_TYPE_NVLIST) {
			size += sizeof(struct nvlist_header);
			size += nvpair_header_size() + 1;
			tmpnvl = nvpair_get_nvlist(nvp);
			PJDLOG_ASSERT(tmpnvl->nvl_error == 0);
			tmpnvp = nvlist_first_nvpair(tmpnvl);
			if (tmpnvp != NULL) {
				nvl = tmpnvl;
				nvp = tmpnvp;
				continue;
			}
		} else {
			size += nvpair_size(nvp);
		}

		while ((nvp = nvlist_next_nvpair(nvl, nvp)) == NULL) {
			cookie = NULL;
			nvl = nvlist_get_parent(nvl, &cookie);
			if (nvl == NULL)
				goto out;
			nvp = cookie;
		}
	}

out:
	return (size);
}

#ifndef _KERNEL
static int *
nvlist_xdescriptors(const nvlist_t *nvl, int *descs)
{
	nvpair_t *nvp;
	const char *name;
	int type;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);

	nvp = NULL;
	do {
		while ((name = nvlist_next(nvl, &type, (void**)&nvp)) != NULL) {
			switch (type) {
			case NV_TYPE_DESCRIPTOR:
				*descs = nvpair_get_descriptor(nvp);
				descs++;
				break;
			case NV_TYPE_NVLIST:
				nvl = nvpair_get_nvlist(nvp);
				nvp = NULL;
				break;
			}
		}
	} while ((nvl = nvlist_get_parent(nvl, (void**)&nvp)) != NULL);

	return (descs);
}
#endif

#ifndef _KERNEL
int *
nvlist_descriptors(const nvlist_t *nvl, size_t *nitemsp)
{
	size_t nitems;
	int *fds;

	nitems = nvlist_ndescriptors(nvl);
	fds = nv_malloc(sizeof(fds[0]) * (nitems + 1));
	if (fds == NULL)
		return (NULL);
	if (nitems > 0)
		nvlist_xdescriptors(nvl, fds);
	fds[nitems] = -1;
	if (nitemsp != NULL)
		*nitemsp = nitems;
	return (fds);
}
#endif

size_t
nvlist_ndescriptors(const nvlist_t *nvl)
{
#ifndef _KERNEL
	nvpair_t *nvp;
	const char *name;
	size_t ndescs;
	int type;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);

	ndescs = 0;
	nvp = NULL;
	do {
		while ((name = nvlist_next(nvl, &type, (void**)&nvp)) != NULL) {
			switch (type) {
			case NV_TYPE_DESCRIPTOR:
				ndescs++;
				break;
			case NV_TYPE_NVLIST:
				nvl = nvpair_get_nvlist(nvp);
				nvp = NULL;
				break;
			}
		}
	} while ((nvl = nvlist_get_parent(nvl, (void**)&nvp)) != NULL);

	return (ndescs);
#else
	return (0);
#endif
}

static unsigned char *
nvlist_pack_header(const nvlist_t *nvl, unsigned char *ptr, size_t *leftp)
{
	struct nvlist_header nvlhdr;

	NVLIST_ASSERT(nvl);

	nvlhdr.nvlh_magic = NVLIST_HEADER_MAGIC;
	nvlhdr.nvlh_version = NVLIST_HEADER_VERSION;
	nvlhdr.nvlh_flags = nvl->nvl_flags;
#if BYTE_ORDER == BIG_ENDIAN
	nvlhdr.nvlh_flags |= NV_FLAG_BIG_ENDIAN;
#endif
	nvlhdr.nvlh_descriptors = nvlist_ndescriptors(nvl);
	nvlhdr.nvlh_size = *leftp - sizeof(nvlhdr);
	PJDLOG_ASSERT(*leftp >= sizeof(nvlhdr));
	memcpy(ptr, &nvlhdr, sizeof(nvlhdr));
	ptr += sizeof(nvlhdr);
	*leftp -= sizeof(nvlhdr);

	return (ptr);
}

static void *
nvlist_xpack(const nvlist_t *nvl, int64_t *fdidxp, size_t *sizep)
{
	unsigned char *buf, *ptr;
	size_t left, size;
	const nvlist_t *tmpnvl;
	nvpair_t *nvp, *tmpnvp;
	void *cookie;

	NVLIST_ASSERT(nvl);

	if (nvl->nvl_error != 0) {
		ERRNO_SET(nvl->nvl_error);
		return (NULL);
	}

	size = nvlist_size(nvl);
	buf = nv_malloc(size);
	if (buf == NULL)
		return (NULL);

	ptr = buf;
	left = size;

	ptr = nvlist_pack_header(nvl, ptr, &left);

	nvp = nvlist_first_nvpair(nvl);
	while (nvp != NULL) {
		NVPAIR_ASSERT(nvp);

		nvpair_init_datasize(nvp);
		ptr = nvpair_pack_header(nvp, ptr, &left);
		if (ptr == NULL) {
			nv_free(buf);
			return (NULL);
		}
		switch (nvpair_type(nvp)) {
		case NV_TYPE_NULL:
			ptr = nvpair_pack_null(nvp, ptr, &left);
			break;
		case NV_TYPE_BOOL:
			ptr = nvpair_pack_bool(nvp, ptr, &left);
			break;
		case NV_TYPE_NUMBER:
			ptr = nvpair_pack_number(nvp, ptr, &left);
			break;
		case NV_TYPE_STRING:
			ptr = nvpair_pack_string(nvp, ptr, &left);
			break;
		case NV_TYPE_NVLIST:
			tmpnvl = nvpair_get_nvlist(nvp);
			ptr = nvlist_pack_header(tmpnvl, ptr, &left);
			if (ptr == NULL)
				goto out;
			tmpnvp = nvlist_first_nvpair(tmpnvl);
			if (tmpnvp != NULL) {
				nvl = tmpnvl;
				nvp = tmpnvp;
				continue;
			}
			ptr = nvpair_pack_nvlist_up(ptr, &left);
			break;
#ifndef _KERNEL
		case NV_TYPE_DESCRIPTOR:
			ptr = nvpair_pack_descriptor(nvp, ptr, fdidxp, &left);
			break;
#endif
		case NV_TYPE_BINARY:
			ptr = nvpair_pack_binary(nvp, ptr, &left);
			break;
		default:
			PJDLOG_ABORT("Invalid type (%d).", nvpair_type(nvp));
		}
		if (ptr == NULL) {
			nv_free(buf);
			return (NULL);
		}
		while ((nvp = nvlist_next_nvpair(nvl, nvp)) == NULL) {
			cookie = NULL;
			nvl = nvlist_get_parent(nvl, &cookie);
			if (nvl == NULL)
				goto out;
			nvp = cookie;
			ptr = nvpair_pack_nvlist_up(ptr, &left);
			if (ptr == NULL)
				goto out;
		}
	}

out:
	if (sizep != NULL)
		*sizep = size;
	return (buf);
}

void *
nvlist_pack(const nvlist_t *nvl, size_t *sizep)
{

	NVLIST_ASSERT(nvl);

	if (nvl->nvl_error != 0) {
		ERRNO_SET(nvl->nvl_error);
		return (NULL);
	}

	if (nvlist_ndescriptors(nvl) > 0) {
		ERRNO_SET(EOPNOTSUPP);
		return (NULL);
	}

	return (nvlist_xpack(nvl, NULL, sizep));
}

static bool
nvlist_check_header(struct nvlist_header *nvlhdrp)
{

	if (nvlhdrp->nvlh_magic != NVLIST_HEADER_MAGIC) {
		ERRNO_SET(EINVAL);
		return (false);
	}
	if ((nvlhdrp->nvlh_flags & ~NV_FLAG_ALL_MASK) != 0) {
		ERRNO_SET(EINVAL);
		return (false);
	}
#if BYTE_ORDER == BIG_ENDIAN
	if ((nvlhdrp->nvlh_flags & NV_FLAG_BIG_ENDIAN) == 0) {
		nvlhdrp->nvlh_size = le64toh(nvlhdrp->nvlh_size);
		nvlhdrp->nvlh_descriptors = le64toh(nvlhdrp->nvlh_descriptors);
	}
#else
	if ((nvlhdrp->nvlh_flags & NV_FLAG_BIG_ENDIAN) != 0) {
		nvlhdrp->nvlh_size = be64toh(nvlhdrp->nvlh_size);
		nvlhdrp->nvlh_descriptors = be64toh(nvlhdrp->nvlh_descriptors);
	}
#endif
	return (true);
}

const unsigned char *
nvlist_unpack_header(nvlist_t *nvl, const unsigned char *ptr, size_t nfds,
    bool *isbep, size_t *leftp)
{
	struct nvlist_header nvlhdr;

	if (*leftp < sizeof(nvlhdr))
		goto failed;

	memcpy(&nvlhdr, ptr, sizeof(nvlhdr));

	if (!nvlist_check_header(&nvlhdr))
		goto failed;

	if (nvlhdr.nvlh_size != *leftp - sizeof(nvlhdr))
		goto failed;

	/*
	 * nvlh_descriptors might be smaller than nfds in embedded nvlists.
	 */
	if (nvlhdr.nvlh_descriptors > nfds)
		goto failed;

	if ((nvlhdr.nvlh_flags & ~NV_FLAG_ALL_MASK) != 0)
		goto failed;

	nvl->nvl_flags = (nvlhdr.nvlh_flags & NV_FLAG_PUBLIC_MASK);

	ptr += sizeof(nvlhdr);
	if (isbep != NULL)
		*isbep = (((int)nvlhdr.nvlh_flags & NV_FLAG_BIG_ENDIAN) != 0);
	*leftp -= sizeof(nvlhdr);

	return (ptr);
failed:
	ERRNO_SET(EINVAL);
	return (NULL);
}

static nvlist_t *
nvlist_xunpack(const void *buf, size_t size, const int *fds, size_t nfds,
    int flags)
{
	const unsigned char *ptr;
	nvlist_t *nvl, *retnvl, *tmpnvl;
	nvpair_t *nvp;
	size_t left;
	bool isbe;

	PJDLOG_ASSERT((flags & ~(NV_FLAG_PUBLIC_MASK)) == 0);

	left = size;
	ptr = buf;

	tmpnvl = NULL;
	nvl = retnvl = nvlist_create(0);
	if (nvl == NULL)
		goto failed;

	ptr = nvlist_unpack_header(nvl, ptr, nfds, &isbe, &left);
	if (ptr == NULL)
		goto failed;
	if (nvl->nvl_flags != flags) {
		ERRNO_SET(EILSEQ);
		goto failed;
	}

	while (left > 0) {
		ptr = nvpair_unpack(isbe, ptr, &left, &nvp);
		if (ptr == NULL)
			goto failed;
		switch (nvpair_type(nvp)) {
		case NV_TYPE_NULL:
			ptr = nvpair_unpack_null(isbe, nvp, ptr, &left);
			break;
		case NV_TYPE_BOOL:
			ptr = nvpair_unpack_bool(isbe, nvp, ptr, &left);
			break;
		case NV_TYPE_NUMBER:
			ptr = nvpair_unpack_number(isbe, nvp, ptr, &left);
			break;
		case NV_TYPE_STRING:
			ptr = nvpair_unpack_string(isbe, nvp, ptr, &left);
			break;
		case NV_TYPE_NVLIST:
			ptr = nvpair_unpack_nvlist(isbe, nvp, ptr, &left, nfds,
			    &tmpnvl);
			nvlist_set_parent(tmpnvl, nvp);
			break;
#ifndef _KERNEL
		case NV_TYPE_DESCRIPTOR:
			ptr = nvpair_unpack_descriptor(isbe, nvp, ptr, &left,
			    fds, nfds);
			break;
#endif
		case NV_TYPE_BINARY:
			ptr = nvpair_unpack_binary(isbe, nvp, ptr, &left);
			break;
		case NV_TYPE_NVLIST_UP:
			if (nvl->nvl_parent == NULL)
				goto failed;
			nvl = nvpair_nvlist(nvl->nvl_parent);
			nvpair_free_structure(nvp);
			continue;
		default:
			PJDLOG_ABORT("Invalid type (%d).", nvpair_type(nvp));
		}
		if (ptr == NULL)
			goto failed;
		nvlist_move_nvpair(nvl, nvp);
		if (tmpnvl != NULL) {
			nvl = tmpnvl;
			tmpnvl = NULL;
		}
	}

	return (retnvl);
failed:
	nvlist_destroy(retnvl);
	return (NULL);
}

nvlist_t *
nvlist_unpack(const void *buf, size_t size, int flags)
{

	return (nvlist_xunpack(buf, size, NULL, 0, flags));
}

#ifndef _KERNEL
int
nvlist_send(int sock, const nvlist_t *nvl)
{
	size_t datasize, nfds;
	int *fds;
	void *data;
	int64_t fdidx;
	int ret;

	if (nvlist_error(nvl) != 0) {
		ERRNO_SET(nvlist_error(nvl));
		return (-1);
	}

	fds = nvlist_descriptors(nvl, &nfds);
	if (fds == NULL)
		return (-1);

	ret = -1;
	data = NULL;
	fdidx = 0;

	data = nvlist_xpack(nvl, &fdidx, &datasize);
	if (data == NULL)
		goto out;

	if (buf_send(sock, data, datasize) == -1)
		goto out;

	if (nfds > 0) {
		if (fd_send(sock, fds, nfds) == -1)
			goto out;
	}

	ret = 0;
out:
	ERRNO_SAVE();
	nv_free(fds);
	nv_free(data);
	ERRNO_RESTORE();
	return (ret);
}

nvlist_t *
nvlist_recv(int sock, int flags)
{
	struct nvlist_header nvlhdr;
	nvlist_t *nvl, *ret;
	unsigned char *buf;
	size_t nfds, size, i;
	int *fds;

	if (buf_recv(sock, &nvlhdr, sizeof(nvlhdr)) == -1)
		return (NULL);

	if (!nvlist_check_header(&nvlhdr))
		return (NULL);

	nfds = (size_t)nvlhdr.nvlh_descriptors;
	size = sizeof(nvlhdr) + (size_t)nvlhdr.nvlh_size;

	buf = nv_malloc(size);
	if (buf == NULL)
		return (NULL);

	memcpy(buf, &nvlhdr, sizeof(nvlhdr));

	ret = NULL;
	fds = NULL;

	if (buf_recv(sock, buf + sizeof(nvlhdr), size - sizeof(nvlhdr)) == -1)
		goto out;

	if (nfds > 0) {
		fds = nv_malloc(nfds * sizeof(fds[0]));
		if (fds == NULL)
			goto out;
		if (fd_recv(sock, fds, nfds) == -1)
			goto out;
	}

	nvl = nvlist_xunpack(buf, size, fds, nfds, flags);
	if (nvl == NULL) {
		ERRNO_SAVE();
		for (i = 0; i < nfds; i++)
			close(fds[i]);
		ERRNO_RESTORE();
		goto out;
	}

	ret = nvl;
out:
	ERRNO_SAVE();
	nv_free(buf);
	nv_free(fds);
	ERRNO_RESTORE();

	return (ret);
}

nvlist_t *
nvlist_xfer(int sock, nvlist_t *nvl, int flags)
{

	if (nvlist_send(sock, nvl) < 0) {
		nvlist_destroy(nvl);
		return (NULL);
	}
	nvlist_destroy(nvl);
	return (nvlist_recv(sock, flags));
}
#endif

nvpair_t *
nvlist_first_nvpair(const nvlist_t *nvl)
{

	NVLIST_ASSERT(nvl);

	return (TAILQ_FIRST(&nvl->nvl_head));
}

nvpair_t *
nvlist_next_nvpair(const nvlist_t *nvl, const nvpair_t *nvp)
{
	nvpair_t *retnvp;

	NVLIST_ASSERT(nvl);
	NVPAIR_ASSERT(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(nvp) == nvl);

	retnvp = nvpair_next(nvp);
	PJDLOG_ASSERT(retnvp == NULL || nvpair_nvlist(retnvp) == nvl);

	return (retnvp);

}

nvpair_t *
nvlist_prev_nvpair(const nvlist_t *nvl, const nvpair_t *nvp)
{
	nvpair_t *retnvp;

	NVLIST_ASSERT(nvl);
	NVPAIR_ASSERT(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(nvp) == nvl);

	retnvp = nvpair_prev(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(retnvp) == nvl);

	return (retnvp);
}

const char *
nvlist_next(const nvlist_t *nvl, int *typep, void **cookiep)
{
	nvpair_t *nvp;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(cookiep != NULL);

	if (*cookiep == NULL)
		nvp = nvlist_first_nvpair(nvl);
	else
		nvp = nvlist_next_nvpair(nvl, *cookiep);
	if (nvp == NULL)
		return (NULL);
	if (typep != NULL)
		*typep = nvpair_type(nvp);
	*cookiep = nvp;
	return (nvpair_name(nvp));
}

bool
nvlist_exists(const nvlist_t *nvl, const char *name)
{

	return (nvlist_find(nvl, NV_TYPE_NONE, name) != NULL);
}

#define	NVLIST_EXISTS(type, TYPE)					\
bool									\
nvlist_exists_##type(const nvlist_t *nvl, const char *name)		\
{									\
									\
	return (nvlist_find(nvl, NV_TYPE_##TYPE, name) != NULL);	\
}

NVLIST_EXISTS(null, NULL)
NVLIST_EXISTS(bool, BOOL)
NVLIST_EXISTS(number, NUMBER)
NVLIST_EXISTS(string, STRING)
NVLIST_EXISTS(nvlist, NVLIST)
#ifndef _KERNEL
NVLIST_EXISTS(descriptor, DESCRIPTOR)
#endif
NVLIST_EXISTS(binary, BINARY)

#undef	NVLIST_EXISTS

void
nvlist_add_nvpair(nvlist_t *nvl, const nvpair_t *nvp)
{
	nvpair_t *newnvp;

	NVPAIR_ASSERT(nvp);

	if (nvlist_error(nvl) != 0) {
		ERRNO_SET(nvlist_error(nvl));
		return;
	}
	if ((nvl->nvl_flags & NV_FLAG_NO_UNIQUE) == 0) {
		if (nvlist_exists(nvl, nvpair_name(nvp))) {
			nvl->nvl_error = EEXIST;
			ERRNO_SET(nvlist_error(nvl));
			return;
		}
	}

	newnvp = nvpair_clone(nvp);
	if (newnvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvpair_insert(&nvl->nvl_head, newnvp, nvl);
}

void
nvlist_add_stringf(nvlist_t *nvl, const char *name, const char *valuefmt, ...)
{
	va_list valueap;

	va_start(valueap, valuefmt);
	nvlist_add_stringv(nvl, name, valuefmt, valueap);
	va_end(valueap);
}

void
nvlist_add_stringv(nvlist_t *nvl, const char *name, const char *valuefmt,
    va_list valueap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvp = nvpair_create_stringv(name, valuefmt, valueap);
	if (nvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvl->nvl_error);
	} else {
		nvlist_move_nvpair(nvl, nvp);
	}
}

void
nvlist_add_null(nvlist_t *nvl, const char *name)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvp = nvpair_create_null(name);
	if (nvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvl->nvl_error);
	} else {
		nvlist_move_nvpair(nvl, nvp);
	}
}

void
nvlist_add_binary(nvlist_t *nvl, const char *name, const void *value,
    size_t size)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvp = nvpair_create_binary(name, value, size);
	if (nvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvl->nvl_error);
	} else {
		nvlist_move_nvpair(nvl, nvp);
	}
}


#define	NVLIST_ADD(vtype, type)						\
void									\
nvlist_add_##type(nvlist_t *nvl, const char *name, vtype value)		\
{									\
	nvpair_t *nvp;							\
									\
	if (nvlist_error(nvl) != 0) {					\
		ERRNO_SET(nvlist_error(nvl));				\
		return;							\
	}								\
									\
	nvp = nvpair_create_##type(name, value);			\
	if (nvp == NULL) {						\
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);		\
		ERRNO_SET(nvl->nvl_error);				\
	} else {							\
		nvlist_move_nvpair(nvl, nvp);				\
	}								\
}

NVLIST_ADD(bool, bool)
NVLIST_ADD(uint64_t, number)
NVLIST_ADD(const char *, string)
NVLIST_ADD(const nvlist_t *, nvlist)
#ifndef _KERNEL
NVLIST_ADD(int, descriptor);
#endif

#undef	NVLIST_ADD

void
nvlist_move_nvpair(nvlist_t *nvl, nvpair_t *nvp)
{

	NVPAIR_ASSERT(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(nvp) == NULL);

	if (nvlist_error(nvl) != 0) {
		nvpair_free(nvp);
		ERRNO_SET(nvlist_error(nvl));
		return;
	}
	if ((nvl->nvl_flags & NV_FLAG_NO_UNIQUE) == 0) {
		if (nvlist_exists(nvl, nvpair_name(nvp))) {
			nvpair_free(nvp);
			nvl->nvl_error = EEXIST;
			ERRNO_SET(nvl->nvl_error);
			return;
		}
	}

	nvpair_insert(&nvl->nvl_head, nvp, nvl);
}

void
nvlist_move_string(nvlist_t *nvl, const char *name, char *value)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		nv_free(value);
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvp = nvpair_move_string(name, value);
	if (nvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvl->nvl_error);
	} else {
		nvlist_move_nvpair(nvl, nvp);
	}
}

void
nvlist_move_nvlist(nvlist_t *nvl, const char *name, nvlist_t *value)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		if (value != NULL && nvlist_get_nvpair_parent(value) != NULL)
			nvlist_destroy(value);
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvp = nvpair_move_nvlist(name, value);
	if (nvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvl->nvl_error);
	} else {
		nvlist_move_nvpair(nvl, nvp);
	}
}

#ifndef _KERNEL
void
nvlist_move_descriptor(nvlist_t *nvl, const char *name, int value)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		close(value);
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvp = nvpair_move_descriptor(name, value);
	if (nvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvl->nvl_error);
	} else {
		nvlist_move_nvpair(nvl, nvp);
	}
}
#endif

void
nvlist_move_binary(nvlist_t *nvl, const char *name, void *value, size_t size)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		nv_free(value);
		ERRNO_SET(nvlist_error(nvl));
		return;
	}

	nvp = nvpair_move_binary(name, value, size);
	if (nvp == NULL) {
		nvl->nvl_error = ERRNO_OR_DEFAULT(ENOMEM);
		ERRNO_SET(nvl->nvl_error);
	} else {
		nvlist_move_nvpair(nvl, nvp);
	}
}

const nvpair_t *
nvlist_get_nvpair(const nvlist_t *nvl, const char *name)
{

	return (nvlist_find(nvl, NV_TYPE_NONE, name));
}

#define	NVLIST_GET(ftype, type, TYPE)					\
ftype									\
nvlist_get_##type(const nvlist_t *nvl, const char *name)		\
{									\
	const nvpair_t *nvp;						\
									\
	nvp = nvlist_find(nvl, NV_TYPE_##TYPE, name);			\
	if (nvp == NULL)						\
		nvlist_report_missing(NV_TYPE_##TYPE, name);		\
	return (nvpair_get_##type(nvp));				\
}

NVLIST_GET(bool, bool, BOOL)
NVLIST_GET(uint64_t, number, NUMBER)
NVLIST_GET(const char *, string, STRING)
NVLIST_GET(const nvlist_t *, nvlist, NVLIST)
#ifndef _KERNEL
NVLIST_GET(int, descriptor, DESCRIPTOR)
#endif

#undef	NVLIST_GET

const void *
nvlist_get_binary(const nvlist_t *nvl, const char *name, size_t *sizep)
{
	nvpair_t *nvp;

	nvp = nvlist_find(nvl, NV_TYPE_BINARY, name);
	if (nvp == NULL)
		nvlist_report_missing(NV_TYPE_BINARY, name);

	return (nvpair_get_binary(nvp, sizep));
}

#define	NVLIST_TAKE(ftype, type, TYPE)					\
ftype									\
nvlist_take_##type(nvlist_t *nvl, const char *name)			\
{									\
	nvpair_t *nvp;							\
	ftype value;							\
									\
	nvp = nvlist_find(nvl, NV_TYPE_##TYPE, name);			\
	if (nvp == NULL)						\
		nvlist_report_missing(NV_TYPE_##TYPE, name);		\
	value = (ftype)(intptr_t)nvpair_get_##type(nvp);		\
	nvlist_remove_nvpair(nvl, nvp);					\
	nvpair_free_structure(nvp);					\
	return (value);							\
}

NVLIST_TAKE(bool, bool, BOOL)
NVLIST_TAKE(uint64_t, number, NUMBER)
NVLIST_TAKE(char *, string, STRING)
NVLIST_TAKE(nvlist_t *, nvlist, NVLIST)
#ifndef _KERNEL
NVLIST_TAKE(int, descriptor, DESCRIPTOR)
#endif

#undef	NVLIST_TAKE

void *
nvlist_take_binary(nvlist_t *nvl, const char *name, size_t *sizep)
{
	nvpair_t *nvp;
	void *value;

	nvp = nvlist_find(nvl, NV_TYPE_BINARY, name);
	if (nvp == NULL)
		nvlist_report_missing(NV_TYPE_BINARY, name);

	value = (void *)(intptr_t)nvpair_get_binary(nvp, sizep);
	nvlist_remove_nvpair(nvl, nvp);
	nvpair_free_structure(nvp);
	return (value);
}

void
nvlist_remove_nvpair(nvlist_t *nvl, nvpair_t *nvp)
{

	NVLIST_ASSERT(nvl);
	NVPAIR_ASSERT(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(nvp) == nvl);

	nvpair_remove(&nvl->nvl_head, nvp, nvl);
}

void
nvlist_free(nvlist_t *nvl, const char *name)
{

	nvlist_free_type(nvl, name, NV_TYPE_NONE);
}

#define	NVLIST_FREE(type, TYPE)						\
void									\
nvlist_free_##type(nvlist_t *nvl, const char *name)			\
{									\
									\
	nvlist_free_type(nvl, name, NV_TYPE_##TYPE);			\
}

NVLIST_FREE(null, NULL)
NVLIST_FREE(bool, BOOL)
NVLIST_FREE(number, NUMBER)
NVLIST_FREE(string, STRING)
NVLIST_FREE(nvlist, NVLIST)
#ifndef _KERNEL
NVLIST_FREE(descriptor, DESCRIPTOR)
#endif
NVLIST_FREE(binary, BINARY)

#undef	NVLIST_FREE

void
nvlist_free_nvpair(nvlist_t *nvl, nvpair_t *nvp)
{

	NVLIST_ASSERT(nvl);
	NVPAIR_ASSERT(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(nvp) == nvl);

	nvlist_remove_nvpair(nvl, nvp);
	nvpair_free(nvp);
}

