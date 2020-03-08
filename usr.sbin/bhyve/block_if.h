/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
 * $FreeBSD: head/usr.sbin/bhyve/block_if.h 347033 2019-05-02 22:46:37Z jhb $
 */

/*
 * The block API to be used by bhyve block-device emulations. The routines
 * are thread safe, with no assumptions about the context of the completion
 * callback - it may occur in the caller's context, or asynchronously in
 * another thread.
 */

#ifndef _BLOCK_IF_H_
#define _BLOCK_IF_H_

#include <stdint.h>

#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/unistd.h>

/*
 * BLOCKIF_IOV_MAX is the maximum number of scatter/gather entries in
 * a single request.  BLOCKIF_RING_MAX is the maxmimum number of
 * pending requests that can be queued.
 */
#define	BLOCKIF_IOV_MAX		128	/* not practical to be IOV_MAX */
#define	BLOCKIF_RING_MAX	128

#define BLOCKIF_SIG     0xb109b109

#define BLOCKIF_NUMTHR  8
#define BLOCKIF_MAXREQ  (BLOCKIF_RING_MAX + BLOCKIF_NUMTHR)

enum blockop {
        BOP_READ,
        BOP_WRITE,
        BOP_FLUSH,
        BOP_DELETE
};

enum blockstat {
        BST_FREE,
        BST_BLOCK,
        BST_PEND,
        BST_BUSY,
        BST_DONE
};

struct blockif_req {
	int		br_iovcnt;
	off_t		br_offset;
	ssize_t		br_resid;
	void		(*br_callback)(struct blockif_req *req, int err);
	void		*br_param;
	struct iovec	br_iov[BLOCKIF_IOV_MAX];
};

struct blockif_elem {
        TAILQ_ENTRY(blockif_elem) be_link;
        struct blockif_req  *be_req;
        enum blockop         be_op;
        enum blockstat       be_status;
        pthread_t            be_tid;
        off_t                be_block;
};

struct blockif_ctxt {
        int                     bc_magic;
       	/* For data specific for this instance of the backend*/
        intptr_t		bc_desc;
        int                     bc_ischr;
        int                     bc_isgeom;
        int                     bc_candelete;
        int                     bc_rdonly;
        off_t                   bc_size;
        int                     bc_sectsz;
        int                     bc_psectsz;
        int                     bc_psectoff;
        int                     bc_closing;
        pthread_t               bc_btid[BLOCKIF_NUMTHR];
        pthread_mutex_t         bc_mtx;
        pthread_cond_t          bc_cond;

        struct block_backend	*be;

        /* Request elements and free/pending/busy queues */
        TAILQ_HEAD(, blockif_elem) bc_freeq;
        TAILQ_HEAD(, blockif_elem) bc_pendq;
        TAILQ_HEAD(, blockif_elem) bc_busyq;
        struct blockif_elem  bc_reqs[BLOCKIF_MAXREQ];
};

struct blockif_ctxt *blockif_open(char *optstr, const char *ident);
off_t	blockif_size(struct blockif_ctxt *bc);
void	blockif_chs(struct blockif_ctxt *bc, uint16_t *c, uint8_t *h, uint8_t *s);
int	blockif_sectsz(struct blockif_ctxt *bc);
void	blockif_psectsz(struct blockif_ctxt *bc, int *size, int *off);
int	blockif_queuesz(struct blockif_ctxt *bc);
int	blockif_is_ro(struct blockif_ctxt *bc);
int	blockif_candelete(struct blockif_ctxt *bc);
int	blockif_read(struct blockif_ctxt *bc, struct blockif_req *breq);
int	blockif_write(struct blockif_ctxt *bc, struct blockif_req *breq);
int	blockif_flush(struct blockif_ctxt *bc, struct blockif_req *breq);
int	blockif_delete(struct blockif_ctxt *bc, struct blockif_req *breq);
int	blockif_cancel(struct blockif_ctxt *bc, struct blockif_req *breq);
int	blockif_close(struct blockif_ctxt *bc);

/*
 * Each block device backend registers a set of function pointers that are
 * used to implement the net backends API.
 */
struct block_backend {
	const char *bb_name;		/* identifier used parse the option string */
	/*
	 * Routines used to initialize and cleanup the resources needed
	 * by a backend. The init and cleanup function are used internally,
	 * and should not be called by the frontend.
	 */
	void	(*bb_init)(void);

	void	(*bb_cleanup)(struct blockif_ctxt *bc);

	struct blockif_ctxt * (*bb_open)(const char *optstr, const char *ident);

        off_t	(*bb_size)(struct blockif_ctxt *bc);

        void	(*bb_chs)(struct blockif_ctxt *bc, uint16_t *c, uint8_t *h, 
 		       uint8_t *s);

	int	(*bb_sectsz)(struct blockif_ctxt *bc);

        void	(*bb_psectsz)(struct blockif_ctxt *bc, int *size, int *off);

        int	(*bb_queuesz)(struct blockif_ctxt *bc);

        int	(*bb_is_ro)(struct blockif_ctxt *bc);

        int	(*bb_candelete)(struct blockif_ctxt *bc);

        int	(*bb_read)(struct blockif_ctxt *bc, struct blockif_req *breq);

        int	(*bb_write)(struct blockif_ctxt *bc, struct blockif_req *breq);

        int	(*bb_flush)(struct blockif_ctxt *bc, struct blockif_req *breq);

        int	(*bb_delete)(struct blockif_ctxt *bc, struct blockif_req *breq);

        int	(*bb_cancel)(struct blockif_ctxt *bc, struct blockif_req *breq);

        int	(*bb_close)(struct blockif_ctxt *bc);

	/* Room for backend-specific data. */
	void *bb_opaque;
};

#endif /* _BLOCK_IF_H_ */
