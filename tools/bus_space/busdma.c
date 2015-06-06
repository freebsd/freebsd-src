/*-
 * Copyright (c) 2015 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busdma.h"

#include "../../sys/dev/proto/proto_dev.h"

struct tag {
	int	tid;
	u_int	refcnt;
	int	fd;
	struct tag *ptag;
	u_long	key;
	u_long	align;
	u_long	bndry;
	u_long	maxaddr;
};

static struct tag **tidtbl = NULL;
static int ntids = 0;

static struct tag *
tag_alloc(void)
{
	struct tag **newtbl, *tag;
	int tid;

	tag = malloc(sizeof(struct tag));
	tag->refcnt = 0;

	for (tid = 0; tid < ntids; tid++) {
		if (tidtbl[tid] == 0)
			break;
	}
	if (tid == ntids) {
		newtbl = realloc(tidtbl, sizeof(struct tag *) * (ntids + 1));
		if (newtbl == NULL) {
			free(tag);
			return (NULL);
		}
		tidtbl = newtbl;
		ntids++;
	}
	tidtbl[tid] = tag;
	tag->tid = tid;
	return (tag);
}

static int
tag_free(struct tag *tag)
{

	tidtbl[tag->tid] = NULL;
	free(tag);
	return (0);
}

static struct tag *
tid_lookup(int tid)
{
	struct tag *tag;

	if (tid < 0 || tid >= ntids) {
		errno = EINVAL;
		return (NULL);
	}
	tag = tidtbl[tid];
	if (tag->refcnt == 0) {
		errno = ENXIO;
		return (NULL);
	}
	return (tag);
}

struct tag *
bd_tag_new(struct tag *ptag, int fd, u_long align, u_long bndry,
    u_long maxaddr, u_long maxsz, u_int nsegs, u_long maxsegsz,
    u_int datarate, u_int flags)
{
	struct proto_ioc_busdma ioc;
	struct tag *tag;

	tag = tag_alloc();
	if (tag == NULL)
		return (NULL);

	memset(&ioc, 0, sizeof(ioc));
	ioc.request = (ptag != NULL) ? PROTO_IOC_BUSDMA_TAG_DERIVE :
	    PROTO_IOC_BUSDMA_TAG_CREATE;
	ioc.key = (ptag != NULL) ? ptag->key : 0;
	ioc.u.tag.align = align;
	ioc.u.tag.bndry = bndry;
	ioc.u.tag.maxaddr = maxaddr;
	ioc.u.tag.maxsz = maxsz;
	ioc.u.tag.nsegs = nsegs;
	ioc.u.tag.maxsegsz = maxsegsz;
	ioc.u.tag.datarate = datarate;
	ioc.u.tag.flags = flags;
	if (ioctl(fd, PROTO_IOC_BUSDMA, &ioc) == -1) {
		tag_free(tag);
		return (NULL);
	}
	tag->refcnt = 1;
	tag->fd = fd;
	tag->ptag = ptag;
	tag->key = ioc.key;
	tag->align = ioc.u.tag.align;
	tag->bndry = ioc.u.tag.bndry;
	tag->maxaddr = ioc.u.tag.maxaddr;
	return (tag);
}

int
bd_tag_create(const char *dev, u_long align, u_long bndry, u_long maxaddr,
    u_long maxsz, u_int nsegs, u_long maxsegsz, u_int datarate, u_int flags)
{
	struct tag *tag;
	int fd;

	fd = open(dev, O_RDWR);
	if (fd == -1)
		return (-1);

	tag = bd_tag_new(NULL, fd, align, bndry, maxaddr, maxsz, nsegs,
	    maxsegsz, datarate, flags);
	if (tag == NULL) {
		close(fd);
		return (-1);
	}
	return (tag->tid);
}

int
bd_tag_derive(int ptid, u_long align, u_long bndry, u_long maxaddr,
    u_long maxsz, u_int nsegs, u_long maxsegsz, u_int datarate, u_int flags)
{
	struct tag *ptag, *tag;

	ptag = tid_lookup(ptid);
	if (ptag == NULL)
		return (-1);

	tag = bd_tag_new(ptag, ptag->fd, align, bndry, maxaddr, maxsz, nsegs,
	    maxsegsz, datarate, flags);
	if (tag == NULL)
		return (-1);
	while (ptag != NULL) {
		ptag->refcnt++;
		ptag = ptag->ptag;
	}
	return (tag->tid);
}

int
bd_tag_destroy(int tid)
{
	struct proto_ioc_busdma ioc;
	struct tag *ptag, *tag;

	tag = tid_lookup(tid);
	if (tag == NULL)
		return (errno);
	if (tag->refcnt > 1)
		return (EBUSY);

	memset(&ioc, 0, sizeof(ioc));
	ioc.request = PROTO_IOC_BUSDMA_TAG_DESTROY;
	ioc.key = tag->key;
	if (ioctl(tag->fd, PROTO_IOC_BUSDMA, &ioc) == -1)
		return (errno);

	ptag = tag->ptag;
	if (ptag == NULL)
		close(tag->fd);
	else {
		do {
			ptag->refcnt--;
			ptag = ptag->ptag;
		} while (ptag != NULL);
	}
	tag_free(tag);
	return (0);
}
