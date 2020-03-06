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
 * $FreeBSD: head/usr.sbin/bhyve/block_if.c 356523 2020-01-08 22:55:22Z vmaffione $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/usr.sbin/bhyve/block_if.c 356523 2020-01-08 22:55:22Z vmaffione $");

#include <sys/param.h>
#include <unistd.h>
#include <string.h>

#include <sys/linker_set.h>

#include "block_if.h"

SET_DECLARE(block_backend_set, block_backend_t);

blockif_ctxt_t *
blockif_open(const char *optstr, const char *ident)
{
	block_backend_t **bbe = NULL;
	blockif_ctxt_t *ret = NULL;
        char *optrest; 		/* if found, optstr without scheme: */

	optrest = strchr(optstr, ':');
	/* try the legacy style local reference */
	if (optrest == NULL) {
		if (( ret = blocklocal_backend.bb_open(optstr, ident)) != NULL) {
			/* fill in the backend that is used to open this request */
			ret->be = &blocklocal_backend;
			return (ret);
		} else {
			return (NULL);
		}
        }
	*optrest = '\0';
	optrest++;

	/*
	 * Find the block device backend that matches the user-provided
	 * device name. block_backend_set is built using a linker set.
	 */
	SET_FOREACH(bbe, block_backend_set) {
		/*
		 * We match the first part of optstr against the names of the 
		 * backends until we have a match
		 */
		/*
		 * Local access has a pattern like:
		 *   3:0,virtio-blk,file/somewhere/guest.img
		 *   3:0,virtio-blk,/dev/xxxx
                 * Or new style:
		 *   3:0,virtio-blk,file:file/somewhere/guest.img
		 *   3:0,virtio-blk,file:/dev/xxxxx
		 * Requesting a RADOS block device should look something like:
		 *   3:0,virtio-blk,rbd:pool/image,option_1,option_2=val,....
		 * For local filesystem references in optstr shall exist.
		 * This is handled by the blk-local backend.
		 * If this does not match then other backends in the block_backend_set have
		 * their bb_open() called. The first one returning a non-NULL backend pointer 
		 * is a match and is used with the specification in optstr
		 */
		if (strcmp(optstr, (*bbe)->bb_name) == 0) {
		        ret = (*bbe)->bb_open(optrest, ident);
			/* fill in the backend that is used to open this request */
			if (ret != NULL) {
				ret->be = *bbe;
				return (ret);
			}
			break;
		}
	
	}
	return (NULL);
}

int
blockif_read(blockif_ctxt_t *bc, blockif_req_t *breq)
{
	return ((bc->be)->bb_read(bc, breq));
}

int
blockif_write(blockif_ctxt_t *bc, blockif_req_t *breq)
{
	return ((bc->be)->bb_write(bc, breq));
}

int
blockif_flush(blockif_ctxt_t *bc, blockif_req_t *breq)
{
	return ((bc->be)->bb_flush(bc, breq));
}

int
blockif_delete(blockif_ctxt_t *bc, blockif_req_t *breq)
{
	return ((bc->be)->bb_delete(bc, breq));
}

int
blockif_cancel(blockif_ctxt_t *bc, blockif_req_t *breq)
{
	return ((bc->be)->bb_cancel(bc, breq));
}

int
blockif_close(blockif_ctxt_t *bc)
{
	return ((bc->be)->bb_close(bc));
}

void
blockif_chs(blockif_ctxt_t *bc, uint16_t *c, uint8_t *h, uint8_t *s)
{
	(bc->be)->bb_chs(bc, c, h, s);
}

off_t
blockif_size(blockif_ctxt_t *bc)
{
	return ((bc->be)->bb_size(bc));
}

int
blockif_sectsz(blockif_ctxt_t *bc)
{
	return ((bc->be)->bb_sectsz(bc));
}

void
blockif_psectsz(blockif_ctxt_t *bc, int *size, int *off)
{
	(bc->be)->bb_psectsz(bc, size, off);
}

int
blockif_queuesz(blockif_ctxt_t *bc)
{
	return ((bc->be)->bb_queuesz(bc));
}

int
blockif_is_ro(blockif_ctxt_t *bc)
{
	return ((bc->be)->bb_is_ro(bc));
}

int
blockif_candelete(blockif_ctxt_t *bc)
{
	return ((bc->be)->bb_candelete(bc));
}
