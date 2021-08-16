/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 * $FreeBSD$
 */

/*
 * The block API to be used by bhyve block-device emulations. The routines
 * are thread safe, with no assumptions about the context of the completion
 * callback - it may occur in the caller's context, or asynchronously in
 * another thread.
 */

#ifndef _BLOCK_IF_H_
#define _BLOCK_IF_H_

#include <sys/nv.h>
#include <sys/uio.h>
#include <sys/unistd.h>

struct vm_snapshot_meta;


/*
 * BLOCKIF_IOV_MAX is the maximum number of scatter/gather entries in
 * a single request.  BLOCKIF_RING_MAX is the maxmimum number of
 * pending requests that can be queued.
 */
#define	BLOCKIF_IOV_MAX		128	/* not practical to be IOV_MAX */
#define	BLOCKIF_RING_MAX	128

struct blockif_req {
	int		br_iovcnt;
	off_t		br_offset;
	ssize_t		br_resid;
	void		(*br_callback)(struct blockif_req *req, int err);
	void		*br_param;
	struct iovec	br_iov[BLOCKIF_IOV_MAX];
};

struct pci_devinst;
struct blockif_ctxt;

typedef void blockif_resize_cb(struct blockif_ctxt *, void *, size_t);

int	blockif_legacy_config(nvlist_t *nvl, const char *opts);
int 	blockif_add_boot_device(struct pci_devinst *const pi, struct blockif_ctxt *const bc);
struct blockif_ctxt *blockif_open(nvlist_t *nvl, const char *ident);
int	blockif_register_resize_callback(struct blockif_ctxt *bc,
    blockif_resize_cb *cb, void *cb_arg);
off_t	blockif_size(struct blockif_ctxt *bc);
void	blockif_chs(struct blockif_ctxt *bc, uint16_t *c, uint8_t *h,
    uint8_t *s);
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
#ifdef BHYVE_SNAPSHOT
void	blockif_pause(struct blockif_ctxt *bc);
void	blockif_resume(struct blockif_ctxt *bc);
#endif

#endif /* _BLOCK_IF_H_ */
