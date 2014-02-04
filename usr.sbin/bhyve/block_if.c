/*-
 * Copyright (c) 2013  Peter Grehan <grehan@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <pthread_np.h>
#include <unistd.h>

#include "bhyverun.h"
#include "block_if.h"

#define BLOCKIF_SIG	0xb109b109

#define BLOCKIF_MAXREQ	16

enum blockop {
	BOP_READ,
	BOP_WRITE,
	BOP_FLUSH,
	BOP_CANCEL
};

enum blockstat {
	BST_FREE,
	BST_INUSE
};

struct blockif_elem {
	TAILQ_ENTRY(blockif_elem) be_link;
	struct blockif_req  *be_req;
	enum blockop	     be_op;
	enum blockstat	     be_status;
};

struct blockif_ctxt {
	int			bc_magic;
	int			bc_fd;
	int			bc_rdonly;
	off_t			bc_size;
	int			bc_sectsz;
	pthread_t		bc_btid;
        pthread_mutex_t		bc_mtx;
        pthread_cond_t		bc_cond;
	int			bc_closing;

	/* Request elements and free/inuse queues */
	TAILQ_HEAD(, blockif_elem) bc_freeq;       
	TAILQ_HEAD(, blockif_elem) bc_inuseq;       
	u_int			bc_req_count;
	struct blockif_elem	bc_reqs[BLOCKIF_MAXREQ];
};

static int
blockif_enqueue(struct blockif_ctxt *bc, struct blockif_req *breq,
		enum blockop op)
{
	struct blockif_elem *be;

	assert(bc->bc_req_count < BLOCKIF_MAXREQ);

	be = TAILQ_FIRST(&bc->bc_freeq);
	assert(be != NULL);
	assert(be->be_status == BST_FREE);

	TAILQ_REMOVE(&bc->bc_freeq, be, be_link);
	be->be_status = BST_INUSE;
	be->be_req = breq;
	be->be_op = op;
	TAILQ_INSERT_TAIL(&bc->bc_inuseq, be, be_link);

	bc->bc_req_count++;

	return (0);
}

static int
blockif_dequeue(struct blockif_ctxt *bc, struct blockif_elem *el)
{
	struct blockif_elem *be;

	if (bc->bc_req_count == 0)
		return (ENOENT);

	be = TAILQ_FIRST(&bc->bc_inuseq);
	assert(be != NULL);
	assert(be->be_status == BST_INUSE);
	*el = *be;

	TAILQ_REMOVE(&bc->bc_inuseq, be, be_link);
	be->be_status = BST_FREE;
	be->be_req = NULL;
	TAILQ_INSERT_TAIL(&bc->bc_freeq, be, be_link);
	
	bc->bc_req_count--;

	return (0);
}

static void
blockif_proc(struct blockif_ctxt *bc, struct blockif_elem *be)
{
	struct blockif_req *br;
	int err;

	br = be->be_req;
	err = 0;

	switch (be->be_op) {
	case BOP_READ:
		if (preadv(bc->bc_fd, br->br_iov, br->br_iovcnt,
			   br->br_offset) < 0)
			err = errno;
		break;
	case BOP_WRITE:
		if (bc->bc_rdonly)
			err = EROFS;
		else if (pwritev(bc->bc_fd, br->br_iov, br->br_iovcnt,
			     br->br_offset) < 0)
			err = errno;
		break;
	case BOP_FLUSH:
		break;
	case BOP_CANCEL:
		err = EINTR;
		break;
	default:
		err = EINVAL;
		break;
	}

	(*br->br_callback)(br, err);
}

static void *
blockif_thr(void *arg)
{
	struct blockif_ctxt *bc;
	struct blockif_elem req;

	bc = arg;

	for (;;) {
		pthread_mutex_lock(&bc->bc_mtx);
		while (!blockif_dequeue(bc, &req)) {
			pthread_mutex_unlock(&bc->bc_mtx);
			blockif_proc(bc, &req);
			pthread_mutex_lock(&bc->bc_mtx);
		}
		pthread_cond_wait(&bc->bc_cond, &bc->bc_mtx);
		pthread_mutex_unlock(&bc->bc_mtx);

		/*
		 * Check ctxt status here to see if exit requested
		 */
		if (bc->bc_closing)
			pthread_exit(NULL);
	}

	/* Not reached */
	return (NULL);
}

struct blockif_ctxt *
blockif_open(const char *optstr, const char *ident)
{
	char tname[MAXCOMLEN + 1];
	char *nopt, *xopts;
	struct blockif_ctxt *bc;
	struct stat sbuf;
	off_t size;
	int extra, fd, i, sectsz;
	int nocache, sync, ro;

	nocache = 0;
	sync = 0;
	ro = 0;

	/*
	 * The first element in the optstring is always a pathname.
	 * Optional elements follow
	 */
	nopt = strdup(optstr);
	for (xopts = strtok(nopt, ",");
	     xopts != NULL;
	     xopts = strtok(NULL, ",")) {
		if (!strcmp(xopts, "nocache"))
			nocache = 1;
		else if (!strcmp(xopts, "sync"))
			sync = 1;
		else if (!strcmp(xopts, "ro"))
			ro = 1;
	}

	extra = 0;
	if (nocache)
		extra |= O_DIRECT;
	if (sync)
		extra |= O_SYNC;

	fd = open(nopt, (ro ? O_RDONLY : O_RDWR) | extra);
	if (fd < 0 && !ro) {
		/* Attempt a r/w fail with a r/o open */
		fd = open(nopt, O_RDONLY | extra);
		ro = 1;
	}

	if (fd < 0) {
		perror("Could not open backing file");
		return (NULL);
	}

        if (fstat(fd, &sbuf) < 0) {
                perror("Could not stat backing file");
                close(fd);
                return (NULL);
        }

        /*
	 * Deal with raw devices
	 */
        size = sbuf.st_size;
	sectsz = DEV_BSIZE;
	if (S_ISCHR(sbuf.st_mode)) {
		if (ioctl(fd, DIOCGMEDIASIZE, &size) < 0 ||
		    ioctl(fd, DIOCGSECTORSIZE, &sectsz)) {
			perror("Could not fetch dev blk/sector size");
			close(fd);
			return (NULL);
		}
		assert(size != 0);
		assert(sectsz != 0);
	}

	bc = malloc(sizeof(struct blockif_ctxt));
	if (bc == NULL) {
		close(fd);
		return (NULL);
	}

	memset(bc, 0, sizeof(*bc));
	bc->bc_magic = BLOCKIF_SIG;
	bc->bc_fd = fd;
	bc->bc_size = size;
	bc->bc_sectsz = sectsz;
	pthread_mutex_init(&bc->bc_mtx, NULL);
	pthread_cond_init(&bc->bc_cond, NULL);
	TAILQ_INIT(&bc->bc_freeq);
	TAILQ_INIT(&bc->bc_inuseq);
	bc->bc_req_count = 0;
	for (i = 0; i < BLOCKIF_MAXREQ; i++) {
		bc->bc_reqs[i].be_status = BST_FREE;
		TAILQ_INSERT_HEAD(&bc->bc_freeq, &bc->bc_reqs[i], be_link);
	}

	pthread_create(&bc->bc_btid, NULL, blockif_thr, bc);

	snprintf(tname, sizeof(tname), "blk-%s", ident);
	pthread_set_name_np(bc->bc_btid, tname);

	return (bc);
}

static int
blockif_request(struct blockif_ctxt *bc, struct blockif_req *breq,
		enum blockop op)
{
	int err;

	err = 0;

	pthread_mutex_lock(&bc->bc_mtx);
	if (bc->bc_req_count < BLOCKIF_MAXREQ) {
		/*
		 * Enqueue and inform the block i/o thread
		 * that there is work available
		 */
		blockif_enqueue(bc, breq, op);
		pthread_cond_signal(&bc->bc_cond);
	} else {
		/*
		 * Callers are not allowed to enqueue more than
		 * the specified blockif queue limit. Return an
		 * error to indicate that the queue length has been
		 * exceeded.
		 */
		err = E2BIG;
	}
	pthread_mutex_unlock(&bc->bc_mtx);

	return (err);
}

int
blockif_read(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_READ));
}

int
blockif_write(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_WRITE));
}

int
blockif_flush(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_FLUSH));
}

int
blockif_cancel(struct blockif_ctxt *bc, struct blockif_req *breq)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (blockif_request(bc, breq, BOP_CANCEL));
}

int
blockif_close(struct blockif_ctxt *bc)
{
	void *jval;
	int err;

	err = 0;

	assert(bc->bc_magic == BLOCKIF_SIG);

	/*
	 * Stop the block i/o thread
	 */
	bc->bc_closing = 1;
	pthread_cond_signal(&bc->bc_cond);
	pthread_join(bc->bc_btid, &jval);

	/* XXX Cancel queued i/o's ??? */

	/*
	 * Release resources
	 */
	bc->bc_magic = 0;
	close(bc->bc_fd);
	free(bc);

	return (0);
}

/*
 * Accessors
 */
off_t
blockif_size(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (bc->bc_size);
}

int
blockif_sectsz(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (bc->bc_sectsz);
}

int
blockif_queuesz(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (BLOCKIF_MAXREQ);
}

int
blockif_is_ro(struct blockif_ctxt *bc)
{

	assert(bc->bc_magic == BLOCKIF_SIG);
	return (bc->bc_rdonly);
}
