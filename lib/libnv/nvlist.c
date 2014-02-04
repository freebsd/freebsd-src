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

#ifdef HAVE_PJDLOG
#include <pjdlog.h>
#endif

#include "msgio.h"
#include "nv.h"
#include "nv_impl.h"
#include "nvlist_impl.h"
#include "nvpair_impl.h"

#ifndef	HAVE_PJDLOG
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

#define	NV_FLAG_PRIVATE_MASK	(NV_FLAG_BIG_ENDIAN)
#define	NV_FLAG_PUBLIC_MASK	(NV_FLAG_IGNORE_CASE)
#define	NV_FLAG_ALL_MASK	(NV_FLAG_PRIVATE_MASK | NV_FLAG_PUBLIC_MASK)

#define	NVLIST_MAGIC	0x6e766c	/* "nvl" */
struct nvlist {
	int		nvl_magic;
	int		nvl_error;
	int		nvl_flags;
	struct nvl_head	nvl_head;
};

#define	NVLIST_ASSERT(nvl)	do {					\
	PJDLOG_ASSERT((nvl) != NULL);					\
	PJDLOG_ASSERT((nvl)->nvl_magic == NVLIST_MAGIC);		\
} while (0)

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

	nvl = malloc(sizeof(*nvl));
	nvl->nvl_error = 0;
	nvl->nvl_flags = flags;
	TAILQ_INIT(&nvl->nvl_head);
	nvl->nvl_magic = NVLIST_MAGIC;

	return (nvl);
}

void
nvlist_destroy(nvlist_t *nvl)
{
	nvpair_t *nvp;
	int serrno;

	if (nvl == NULL)
		return;

	serrno = errno;

	NVLIST_ASSERT(nvl);

	while ((nvp = nvlist_first_nvpair(nvl)) != NULL)
		nvlist_remove_nvpair(nvl, nvp);
	nvl->nvl_magic = 0;
	free(nvl);

	errno = serrno;
}

int
nvlist_error(const nvlist_t *nvl)
{

	if (nvl == NULL)
		return (ENOMEM);

	NVLIST_ASSERT(nvl);

	return (nvl->nvl_error);
}

bool
nvlist_empty(const nvlist_t *nvl)
{

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);

	return (nvlist_first_nvpair(nvl) == NULL);
}

static void
nvlist_report_missing(int type, const char *namefmt, va_list nameap)
{
	char *name;

	vasprintf(&name, namefmt, nameap);
	PJDLOG_ABORT("Element '%s' of type %s doesn't exist.",
	    name != NULL ? name : "N/A", nvpair_type_string(type));
}

static nvpair_t *
nvlist_findv(const nvlist_t *nvl, int type, const char *namefmt, va_list nameap)
{
	nvpair_t *nvp;
	char *name;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(type == NV_TYPE_NONE ||
	    (type >= NV_TYPE_FIRST && type <= NV_TYPE_LAST));

	if (vasprintf(&name, namefmt, nameap) < 0)
		return (NULL);

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

	free(name);

	if (nvp == NULL)
		errno = ENOENT;

	return (nvp);
}

bool
nvlist_exists_type(const nvlist_t *nvl, const char *name, int type)
{

	return (nvlist_existsf_type(nvl, type, "%s", name));
}

bool
nvlist_existsf_type(const nvlist_t *nvl, int type, const char *namefmt, ...)
{
	va_list nameap;
	bool ret;

	va_start(nameap, namefmt);
	ret = nvlist_existsv_type(nvl, type, namefmt, nameap);
	va_end(nameap);

	return (ret);
}

bool
nvlist_existsv_type(const nvlist_t *nvl, int type, const char *namefmt,
    va_list nameap)
{

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(type == NV_TYPE_NONE ||
	    (type >= NV_TYPE_FIRST && type <= NV_TYPE_LAST));

	return (nvlist_findv(nvl, type, namefmt, nameap) != NULL);
}

void
nvlist_free_type(nvlist_t *nvl, const char *name, int type)
{

	nvlist_freef_type(nvl, type, "%s", name);
}

void
nvlist_freef_type(nvlist_t *nvl, int type, const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_freev_type(nvl, type, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_freev_type(nvlist_t *nvl, int type, const char *namefmt, va_list nameap)
{
	va_list cnameap;
	nvpair_t *nvp;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(type == NV_TYPE_NONE ||
	    (type >= NV_TYPE_FIRST && type <= NV_TYPE_LAST));

	va_copy(cnameap, nameap);
	nvp = nvlist_findv(nvl, type, namefmt, cnameap);
	va_end(cnameap);
	if (nvp != NULL)
		nvlist_free_nvpair(nvl, nvp);
	else
		nvlist_report_missing(type, namefmt, nameap);
}

nvlist_t *
nvlist_clone(const nvlist_t *nvl)
{
	nvlist_t *newnvl;
	nvpair_t *nvp, *newnvp;

	NVLIST_ASSERT(nvl);

	if (nvl->nvl_error != 0) {
		errno = nvl->nvl_error;
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

/*
 * Dump content of nvlist.
 */
static void
nvlist_xdump(const nvlist_t *nvl, int fd, int level)
{
	nvpair_t *nvp;

	PJDLOG_ASSERT(level < 3);

	if (nvlist_error(nvl) != 0) {
		dprintf(fd, "%*serror: %d\n", level * 4, "",
		    nvlist_error(nvl));
		return;
	}

	for (nvp = nvlist_first_nvpair(nvl); nvp != NULL;
	    nvp = nvlist_next_nvpair(nvl, nvp)) {
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
			nvlist_xdump(nvpair_get_nvlist(nvp), fd, level + 1);
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
	}
}

void
nvlist_dump(const nvlist_t *nvl, int fd)
{

	nvlist_xdump(nvl, fd, 0);
}

void
nvlist_fdump(const nvlist_t *nvl, FILE *fp)
{

	fflush(fp);
	nvlist_dump(nvl, fileno(fp));
}

/*
 * The function obtains size of the nvlist after nvlist_pack().
 * Additional argument 'level' allows to track how deep are we as we obtain
 * size of the NV_TYPE_NVLIST elements using recursion. We allow at most
 * three levels of recursion.
 */
static size_t
nvlist_xsize(const nvlist_t *nvl, int level)
{
	const nvpair_t *nvp;
	size_t size;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(level < 3);

	size = sizeof(struct nvlist_header);
	for (nvp = nvlist_first_nvpair(nvl); nvp != NULL;
	    nvp = nvlist_next_nvpair(nvl, nvp)) {
		size += nvpair_header_size();
		size += strlen(nvpair_name(nvp)) + 1;
		if (nvpair_type(nvp) == NV_TYPE_NVLIST)
			size += nvlist_xsize(nvpair_get_nvlist(nvp), level + 1);
		else
			size += nvpair_size(nvp);
	}

	return (size);
}

size_t
nvlist_size(const nvlist_t *nvl)
{

	return (nvlist_xsize(nvl, 0));
}

static int *
nvlist_xdescriptors(const nvlist_t *nvl, int *descs, int level)
{
	const nvpair_t *nvp;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(level < 3);

	for (nvp = nvlist_first_nvpair(nvl); nvp != NULL;
	    nvp = nvlist_next_nvpair(nvl, nvp)) {
		switch (nvpair_type(nvp)) {
		case NV_TYPE_DESCRIPTOR:
			*descs = nvpair_get_descriptor(nvp);
			descs++;
			break;
		case NV_TYPE_NVLIST:
			descs = nvlist_xdescriptors(nvpair_get_nvlist(nvp),
			    descs, level + 1);
			break;
		}
	}

	return (descs);
}

int *
nvlist_descriptors(const nvlist_t *nvl, size_t *nitemsp)
{
	size_t nitems;
	int *fds;

	nitems = nvlist_ndescriptors(nvl);
	fds = malloc(sizeof(fds[0]) * (nitems + 1));
	if (fds == NULL)
		return (NULL);
	if (nitems > 0)
		nvlist_xdescriptors(nvl, fds, 0);
	fds[nitems] = -1;
	if (nitemsp != NULL)
		*nitemsp = nitems;
	return (fds);
}

static size_t
nvlist_xndescriptors(const nvlist_t *nvl, int level)
{
	const nvpair_t *nvp;
	size_t ndescs;

	NVLIST_ASSERT(nvl);
	PJDLOG_ASSERT(nvl->nvl_error == 0);
	PJDLOG_ASSERT(level < 3);

	ndescs = 0;
	for (nvp = nvlist_first_nvpair(nvl); nvp != NULL;
	    nvp = nvlist_next_nvpair(nvl, nvp)) {
		switch (nvpair_type(nvp)) {
		case NV_TYPE_DESCRIPTOR:
			ndescs++;
			break;
		case NV_TYPE_NVLIST:
			ndescs += nvlist_xndescriptors(nvpair_get_nvlist(nvp),
			    level + 1);
			break;
		}
	}

	return (ndescs);
}

size_t
nvlist_ndescriptors(const nvlist_t *nvl)
{

	return (nvlist_xndescriptors(nvl, 0));
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

void *
nvlist_xpack(const nvlist_t *nvl, int64_t *fdidxp, size_t *sizep)
{
	unsigned char *buf, *ptr;
	size_t left, size;
	nvpair_t *nvp;

	NVLIST_ASSERT(nvl);

	if (nvl->nvl_error != 0) {
		errno = nvl->nvl_error;
		return (NULL);
	}

	size = nvlist_size(nvl);
	buf = malloc(size);
	if (buf == NULL)
		return (NULL);

	ptr = buf;
	left = size;

	ptr = nvlist_pack_header(nvl, ptr, &left);

	for (nvp = nvlist_first_nvpair(nvl); nvp != NULL;
	    nvp = nvlist_next_nvpair(nvl, nvp)) {
		ptr = nvpair_pack(nvp, ptr, fdidxp, &left);
		if (ptr == NULL) {
			free(buf);
			return (NULL);
		}
	}

	if (sizep != NULL)
		*sizep = size;
	return (buf);
}

void *
nvlist_pack(const nvlist_t *nvl, size_t *sizep)
{

	NVLIST_ASSERT(nvl);

	if (nvl->nvl_error != 0) {
		errno = nvl->nvl_error;
		return (NULL);
	}

	if (nvlist_ndescriptors(nvl) > 0) {
		errno = EOPNOTSUPP;
		return (NULL);
	}

	return (nvlist_xpack(nvl, NULL, sizep));
}

static bool
nvlist_check_header(struct nvlist_header *nvlhdrp)
{

	if (nvlhdrp->nvlh_magic != NVLIST_HEADER_MAGIC) {
		errno = EINVAL;
		return (false);
	}
	if ((nvlhdrp->nvlh_flags &= ~NV_FLAG_ALL_MASK) != 0) {
		errno = EINVAL;
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

static const unsigned char *
nvlist_unpack_header(nvlist_t *nvl, const unsigned char *ptr, size_t nfds,
    int *flagsp, size_t *leftp)
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
	*flagsp = (int)nvlhdr.nvlh_flags;
	*leftp -= sizeof(nvlhdr);

	return (ptr);
failed:
	errno = EINVAL;
	return (NULL);
}

nvlist_t *
nvlist_xunpack(const void *buf, size_t size, const int *fds, size_t nfds)
{
	const unsigned char *ptr;
	nvlist_t *nvl;
	nvpair_t *nvp;
	size_t left;
	int flags;

	left = size;
	ptr = buf;

	nvl = nvlist_create(0);
	if (nvl == NULL)
		goto failed;

	ptr = nvlist_unpack_header(nvl, ptr, nfds, &flags, &left);
	if (ptr == NULL)
		goto failed;

	while (left > 0) {
		ptr = nvpair_unpack(flags, ptr, &left, fds, nfds, &nvp);
		if (ptr == NULL)
			goto failed;
		nvlist_move_nvpair(nvl, nvp);
	}

	return (nvl);
failed:
	nvlist_destroy(nvl);
	return (NULL);
}

nvlist_t *
nvlist_unpack(const void *buf, size_t size)
{

	return (nvlist_xunpack(buf, size, NULL, 0));
}

int
nvlist_send(int sock, const nvlist_t *nvl)
{
	size_t datasize, nfds;
	int *fds;
	void *data;
	int64_t fdidx;
	int serrno, ret;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
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
	serrno = errno;
	free(fds);
	free(data);
	errno = serrno;
	return (ret);
}

nvlist_t *
nvlist_recv(int sock)
{
	struct nvlist_header nvlhdr;
	nvlist_t *nvl, *ret;
	size_t nfds, size;
	void *buf;
	int serrno, *fds;

	if (msg_peek(sock, &nvlhdr, sizeof(nvlhdr)) == -1)
		return (NULL);

	if (!nvlist_check_header(&nvlhdr))
		return (NULL);

	nfds = (size_t)nvlhdr.nvlh_descriptors;
	size = sizeof(nvlhdr) + (size_t)nvlhdr.nvlh_size;

	buf = malloc(size);
	if (buf == NULL)
		return (NULL);

	ret = NULL;
	fds = NULL;

	if (buf_recv(sock, buf, size) == -1)
		goto out;

	if (nfds > 0) {
		fds = malloc(nfds * sizeof(fds[0]));
		if (fds == NULL)
			goto out;
		if (fd_recv(sock, fds, nfds) == -1)
			goto out;
	}

	nvl = nvlist_xunpack(buf, size, fds, nfds);
	if (nvl == NULL)
		goto out;

	ret = nvl;
out:
	serrno = errno;
	free(buf);
	free(fds);
	errno = serrno;

	return (ret);
}

nvlist_t *
nvlist_xfer(int sock, nvlist_t *nvl)
{

	if (nvlist_send(sock, nvl) < 0) {
		nvlist_destroy(nvl);
		return (NULL);
	}
	nvlist_destroy(nvl);
	return (nvlist_recv(sock));
}

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

	return (nvlist_existsf(nvl, "%s", name));
}

#define	NVLIST_EXISTS(type)						\
bool									\
nvlist_exists_##type(const nvlist_t *nvl, const char *name)		\
{									\
									\
	return (nvlist_existsf_##type(nvl, "%s", name));		\
}

NVLIST_EXISTS(null)
NVLIST_EXISTS(bool)
NVLIST_EXISTS(number)
NVLIST_EXISTS(string)
NVLIST_EXISTS(nvlist)
NVLIST_EXISTS(descriptor)
NVLIST_EXISTS(binary)

#undef	NVLIST_EXISTS

bool
nvlist_existsf(const nvlist_t *nvl, const char *namefmt, ...)
{
	va_list nameap;
	bool ret;

	va_start(nameap, namefmt);
	ret = nvlist_existsv(nvl, namefmt, nameap);
	va_end(nameap);
	return (ret);
}

#define	NVLIST_EXISTSF(type)						\
bool									\
nvlist_existsf_##type(const nvlist_t *nvl, const char *namefmt, ...)	\
{									\
	va_list nameap;							\
	bool ret;							\
									\
	va_start(nameap, namefmt);					\
	ret = nvlist_existsv_##type(nvl, namefmt, nameap);		\
	va_end(nameap);							\
	return (ret);							\
}

NVLIST_EXISTSF(null)
NVLIST_EXISTSF(bool)
NVLIST_EXISTSF(number)
NVLIST_EXISTSF(string)
NVLIST_EXISTSF(nvlist)
NVLIST_EXISTSF(descriptor)
NVLIST_EXISTSF(binary)

#undef	NVLIST_EXISTSF

bool
nvlist_existsv(const nvlist_t *nvl, const char *namefmt, va_list nameap)
{

	return (nvlist_findv(nvl, NV_TYPE_NONE, namefmt, nameap) != NULL);
}

#define	NVLIST_EXISTSV(type, TYPE)					\
bool									\
nvlist_existsv_##type(const nvlist_t *nvl, const char *namefmt,		\
    va_list nameap)							\
{									\
									\
	return (nvlist_findv(nvl, NV_TYPE_##TYPE, namefmt, nameap) !=	\
	    NULL);							\
}

NVLIST_EXISTSV(null, NULL)
NVLIST_EXISTSV(bool, BOOL)
NVLIST_EXISTSV(number, NUMBER)
NVLIST_EXISTSV(string, STRING)
NVLIST_EXISTSV(nvlist, NVLIST)
NVLIST_EXISTSV(descriptor, DESCRIPTOR)
NVLIST_EXISTSV(binary, BINARY)

#undef	NVLIST_EXISTSV

void
nvlist_add_nvpair(nvlist_t *nvl, const nvpair_t *nvp)
{
	nvpair_t *newnvp;

	NVPAIR_ASSERT(nvp);

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}
	if (nvlist_exists(nvl, nvpair_name(nvp))) {
		nvl->nvl_error = errno = EEXIST;
		return;
	}

	newnvp = nvpair_clone(nvp);
	if (newnvp == NULL) {
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
		return;
	}

	nvpair_insert(&nvl->nvl_head, newnvp, nvl);
}

void
nvlist_add_null(nvlist_t *nvl, const char *name)
{

	nvlist_addf_null(nvl, "%s", name);
}

void
nvlist_add_bool(nvlist_t *nvl, const char *name, bool value)
{

	nvlist_addf_bool(nvl, value, "%s", name);
}

void
nvlist_add_number(nvlist_t *nvl, const char *name, uint64_t value)
{

	nvlist_addf_number(nvl, value, "%s", name);
}

void
nvlist_add_string(nvlist_t *nvl, const char *name, const char *value)
{

	nvlist_addf_string(nvl, value, "%s", name);
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
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_create_stringv(name, valuefmt, valueap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_add_nvlist(nvlist_t *nvl, const char *name, const nvlist_t *value)
{

	nvlist_addf_nvlist(nvl, value, "%s", name);
}

void
nvlist_add_descriptor(nvlist_t *nvl, const char *name, int value)
{

	nvlist_addf_descriptor(nvl, value, "%s", name);
}

void
nvlist_add_binary(nvlist_t *nvl, const char *name, const void *value,
    size_t size)
{

	nvlist_addf_binary(nvl, value, size, "%s", name);
}

void
nvlist_addf_null(nvlist_t *nvl, const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_addv_null(nvl, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_addf_bool(nvlist_t *nvl, bool value, const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_addv_bool(nvl, value, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_addf_number(nvlist_t *nvl, uint64_t value, const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_addv_number(nvl, value, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_addf_string(nvlist_t *nvl, const char *value, const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_addv_string(nvl, value, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_addf_nvlist(nvlist_t *nvl, const nvlist_t *value, const char *namefmt,
    ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_addv_nvlist(nvl, value, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_addf_descriptor(nvlist_t *nvl, int value, const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_addv_descriptor(nvl, value, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_addf_binary(nvlist_t *nvl, const void *value, size_t size,
    const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_addv_binary(nvl, value, size, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_addv_null(nvlist_t *nvl, const char *namefmt, va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_createv_null(namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_addv_bool(nvlist_t *nvl, bool value, const char *namefmt, va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_createv_bool(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_addv_number(nvlist_t *nvl, uint64_t value, const char *namefmt,
    va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_createv_number(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_addv_string(nvlist_t *nvl, const char *value, const char *namefmt,
    va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_createv_string(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_addv_nvlist(nvlist_t *nvl, const nvlist_t *value, const char *namefmt,
    va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_createv_nvlist(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_addv_descriptor(nvlist_t *nvl, int value, const char *namefmt,
    va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_createv_descriptor(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_addv_binary(nvlist_t *nvl, const void *value, size_t size,
    const char *namefmt, va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_createv_binary(value, size, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_move_nvpair(nvlist_t *nvl, nvpair_t *nvp)
{

	NVPAIR_ASSERT(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(nvp) == NULL);

	if (nvlist_error(nvl) != 0) {
		nvpair_free(nvp);
		errno = nvlist_error(nvl);
		return;
	}
	if (nvlist_exists(nvl, nvpair_name(nvp))) {
		nvpair_free(nvp);
		nvl->nvl_error = errno = EEXIST;
		return;
	}

	nvpair_insert(&nvl->nvl_head, nvp, nvl);
}

#define	NVLIST_MOVE(vtype, type)					\
void									\
nvlist_move_##type(nvlist_t *nvl, const char *name, vtype value)	\
{									\
									\
	nvlist_movef_##type(nvl, value, "%s", name);			\
}

NVLIST_MOVE(char *, string)
NVLIST_MOVE(nvlist_t *, nvlist)
NVLIST_MOVE(int, descriptor)

#undef	NVLIST_MOVE

void
nvlist_move_binary(nvlist_t *nvl, const char *name, void *value, size_t size)
{

	nvlist_movef_binary(nvl, value, size, "%s", name);
}

#define	NVLIST_MOVEF(vtype, type)					\
void									\
nvlist_movef_##type(nvlist_t *nvl, vtype value, const char *namefmt,	\
    ...)								\
{									\
	va_list nameap;							\
									\
	va_start(nameap, namefmt);					\
	nvlist_movev_##type(nvl, value, namefmt, nameap);		\
	va_end(nameap);							\
}

NVLIST_MOVEF(char *, string)
NVLIST_MOVEF(nvlist_t *, nvlist)
NVLIST_MOVEF(int, descriptor)

#undef	NVLIST_MOVEF

void
nvlist_movef_binary(nvlist_t *nvl, void *value, size_t size,
    const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_movev_binary(nvl, value, size, namefmt, nameap);
	va_end(nameap);
}

void
nvlist_movev_string(nvlist_t *nvl, char *value, const char *namefmt,
    va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		free(value);
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_movev_string(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_movev_nvlist(nvlist_t *nvl, nvlist_t *value, const char *namefmt,
    va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		nvlist_destroy(value);
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_movev_nvlist(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_movev_descriptor(nvlist_t *nvl, int value, const char *namefmt,
    va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		close(value);
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_movev_descriptor(value, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

void
nvlist_movev_binary(nvlist_t *nvl, void *value, size_t size,
    const char *namefmt, va_list nameap)
{
	nvpair_t *nvp;

	if (nvlist_error(nvl) != 0) {
		free(value);
		errno = nvlist_error(nvl);
		return;
	}

	nvp = nvpair_movev_binary(value, size, namefmt, nameap);
	if (nvp == NULL)
		nvl->nvl_error = errno = (errno != 0 ? errno : ENOMEM);
	else
		nvlist_move_nvpair(nvl, nvp);
}

#define	NVLIST_GET(ftype, type)						\
ftype									\
nvlist_get_##type(const nvlist_t *nvl, const char *name)		\
{									\
									\
	return (nvlist_getf_##type(nvl, "%s", name));			\
}

NVLIST_GET(const nvpair_t *, nvpair)
NVLIST_GET(bool, bool)
NVLIST_GET(uint64_t, number)
NVLIST_GET(const char *, string)
NVLIST_GET(const nvlist_t *, nvlist)
NVLIST_GET(int, descriptor)

#undef	NVLIST_GET

const void *
nvlist_get_binary(const nvlist_t *nvl, const char *name, size_t *sizep)
{

	return (nvlist_getf_binary(nvl, sizep, "%s", name));
}

#define	NVLIST_GETF(ftype, type)					\
ftype									\
nvlist_getf_##type(const nvlist_t *nvl, const char *namefmt, ...)	\
{									\
	va_list nameap;							\
	ftype value;							\
									\
	va_start(nameap, namefmt);					\
	value = nvlist_getv_##type(nvl, namefmt, nameap);		\
	va_end(nameap);							\
									\
	return (value);							\
}

NVLIST_GETF(const nvpair_t *, nvpair)
NVLIST_GETF(bool, bool)
NVLIST_GETF(uint64_t, number)
NVLIST_GETF(const char *, string)
NVLIST_GETF(const nvlist_t *, nvlist)
NVLIST_GETF(int, descriptor)

#undef	NVLIST_GETF

const void *
nvlist_getf_binary(const nvlist_t *nvl, size_t *sizep, const char *namefmt, ...)
{
	va_list nameap;
	const void *value;

	va_start(nameap, namefmt);
	value = nvlist_getv_binary(nvl, sizep, namefmt, nameap);
	va_end(nameap);

	return (value);
}

const nvpair_t *
nvlist_getv_nvpair(const nvlist_t *nvl, const char *namefmt, va_list nameap)
{

	return (nvlist_findv(nvl, NV_TYPE_NONE, namefmt, nameap));
}

#define	NVLIST_GETV(ftype, type, TYPE)					\
ftype									\
nvlist_getv_##type(const nvlist_t *nvl, const char *namefmt,		\
    va_list nameap)							\
{									\
	va_list cnameap;						\
	const nvpair_t *nvp;						\
									\
	va_copy(cnameap, nameap);					\
	nvp = nvlist_findv(nvl, NV_TYPE_##TYPE, namefmt, cnameap);	\
	va_end(cnameap);						\
	if (nvp == NULL)						\
		nvlist_report_missing(NV_TYPE_##TYPE, namefmt, nameap);	\
	return (nvpair_get_##type(nvp));				\
}

NVLIST_GETV(bool, bool, BOOL)
NVLIST_GETV(uint64_t, number, NUMBER)
NVLIST_GETV(const char *, string, STRING)
NVLIST_GETV(const nvlist_t *, nvlist, NVLIST)
NVLIST_GETV(int, descriptor, DESCRIPTOR)

#undef	NVLIST_GETV

const void *
nvlist_getv_binary(const nvlist_t *nvl, size_t *sizep, const char *namefmt,
    va_list nameap)
{
	va_list cnameap;
	const nvpair_t *nvp;

	va_copy(cnameap, nameap);
	nvp = nvlist_findv(nvl, NV_TYPE_BINARY, namefmt, cnameap);
	va_end(cnameap);
	if (nvp == NULL)
		nvlist_report_missing(NV_TYPE_BINARY, namefmt, nameap);

	return (nvpair_get_binary(nvp, sizep));
}

#define	NVLIST_TAKE(ftype, type)					\
ftype									\
nvlist_take_##type(nvlist_t *nvl, const char *name)			\
{									\
									\
	return (nvlist_takef_##type(nvl, "%s", name));			\
}

NVLIST_TAKE(nvpair_t *, nvpair)
NVLIST_TAKE(bool, bool)
NVLIST_TAKE(uint64_t, number)
NVLIST_TAKE(char *, string)
NVLIST_TAKE(nvlist_t *, nvlist)
NVLIST_TAKE(int, descriptor)

#undef	NVLIST_TAKE

void *
nvlist_take_binary(nvlist_t *nvl, const char *name, size_t *sizep)
{

	return (nvlist_takef_binary(nvl, sizep, "%s", name));
}

#define	NVLIST_TAKEF(ftype, type)					\
ftype									\
nvlist_takef_##type(nvlist_t *nvl, const char *namefmt, ...)		\
{									\
	va_list nameap;							\
	ftype value;							\
									\
	va_start(nameap, namefmt);					\
	value = nvlist_takev_##type(nvl, namefmt, nameap);		\
	va_end(nameap);							\
									\
	return (value);							\
}

NVLIST_TAKEF(nvpair_t *, nvpair)
NVLIST_TAKEF(bool, bool)
NVLIST_TAKEF(uint64_t, number)
NVLIST_TAKEF(char *, string)
NVLIST_TAKEF(nvlist_t *, nvlist)
NVLIST_TAKEF(int, descriptor)

#undef	NVLIST_TAKEF

void *
nvlist_takef_binary(nvlist_t *nvl, size_t *sizep, const char *namefmt, ...)
{
	va_list nameap;
	void *value;

	va_start(nameap, namefmt);
	value = nvlist_takev_binary(nvl, sizep, namefmt, nameap);
	va_end(nameap);

	return (value);
}

nvpair_t *
nvlist_takev_nvpair(nvlist_t *nvl, const char *namefmt, va_list nameap)
{
	nvpair_t *nvp;

	nvp = nvlist_findv(nvl, NV_TYPE_NONE, namefmt, nameap);
	if (nvp != NULL)
		nvlist_remove_nvpair(nvl, nvp);
	return (nvp);
}

#define	NVLIST_TAKEV(ftype, type, TYPE)					\
ftype									\
nvlist_takev_##type(nvlist_t *nvl, const char *namefmt, va_list nameap)	\
{									\
	va_list cnameap;						\
	nvpair_t *nvp;							\
	ftype value;							\
									\
	va_copy(cnameap, nameap);					\
	nvp = nvlist_findv(nvl, NV_TYPE_##TYPE, namefmt, cnameap);	\
	va_end(cnameap);						\
	if (nvp == NULL)						\
		nvlist_report_missing(NV_TYPE_##TYPE, namefmt, nameap);	\
	value = (ftype)(intptr_t)nvpair_get_##type(nvp);		\
	nvlist_remove_nvpair(nvl, nvp);					\
	nvpair_free_structure(nvp);					\
	return (value);							\
}

NVLIST_TAKEV(bool, bool, BOOL)
NVLIST_TAKEV(uint64_t, number, NUMBER)
NVLIST_TAKEV(char *, string, STRING)
NVLIST_TAKEV(nvlist_t *, nvlist, NVLIST)
NVLIST_TAKEV(int, descriptor, DESCRIPTOR)

#undef	NVLIST_TAKEV

void *
nvlist_takev_binary(nvlist_t *nvl, size_t *sizep, const char *namefmt,
    va_list nameap)
{
	va_list cnameap;
	nvpair_t *nvp;
	void *value;

	va_copy(cnameap, nameap);
	nvp = nvlist_findv(nvl, NV_TYPE_BINARY, namefmt, cnameap);
	va_end(cnameap);
	if (nvp == NULL)
		nvlist_report_missing(NV_TYPE_BINARY, namefmt, nameap);

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

	nvlist_freef(nvl, "%s", name);
}

#define	NVLIST_FREE(type)						\
void									\
nvlist_free_##type(nvlist_t *nvl, const char *name)			\
{									\
									\
	nvlist_freef_##type(nvl, "%s", name);				\
}

NVLIST_FREE(null)
NVLIST_FREE(bool)
NVLIST_FREE(number)
NVLIST_FREE(string)
NVLIST_FREE(nvlist)
NVLIST_FREE(descriptor)
NVLIST_FREE(binary)

#undef	NVLIST_FREE

void
nvlist_freef(nvlist_t *nvl, const char *namefmt, ...)
{
	va_list nameap;

	va_start(nameap, namefmt);
	nvlist_freev(nvl, namefmt, nameap);
	va_end(nameap);
}

#define	NVLIST_FREEF(type)						\
void									\
nvlist_freef_##type(nvlist_t *nvl, const char *namefmt, ...)		\
{									\
	va_list nameap;							\
									\
	va_start(nameap, namefmt);					\
	nvlist_freev_##type(nvl, namefmt, nameap);			\
	va_end(nameap);							\
}

NVLIST_FREEF(null)
NVLIST_FREEF(bool)
NVLIST_FREEF(number)
NVLIST_FREEF(string)
NVLIST_FREEF(nvlist)
NVLIST_FREEF(descriptor)
NVLIST_FREEF(binary)

#undef	NVLIST_FREEF

void
nvlist_freev(nvlist_t *nvl, const char *namefmt, va_list nameap)
{

	nvlist_freev_type(nvl, NV_TYPE_NONE, namefmt, nameap);
}

#define	NVLIST_FREEV(type, TYPE)					\
void									\
nvlist_freev_##type(nvlist_t *nvl, const char *namefmt, va_list nameap)	\
{									\
									\
	nvlist_freev_type(nvl, NV_TYPE_##TYPE, namefmt, nameap);	\
}

NVLIST_FREEV(null, NULL)
NVLIST_FREEV(bool, BOOL)
NVLIST_FREEV(number, NUMBER)
NVLIST_FREEV(string, STRING)
NVLIST_FREEV(nvlist, NVLIST)
NVLIST_FREEV(descriptor, DESCRIPTOR)
NVLIST_FREEV(binary, BINARY)
#undef	NVLIST_FREEV

void
nvlist_free_nvpair(nvlist_t *nvl, nvpair_t *nvp)
{

	NVLIST_ASSERT(nvl);
	NVPAIR_ASSERT(nvp);
	PJDLOG_ASSERT(nvpair_nvlist(nvp) == nvl);

	nvlist_remove_nvpair(nvl, nvp);
	nvpair_free(nvp);
}
