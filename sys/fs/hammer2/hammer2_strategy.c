/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>

#include <vm/uma.h>

#include "hammer2.h"
#include "hammer2_lz4.h"

#include "zlib/hammer2_zlib.h"

static int hammer2_strategy_read(struct vop_strategy_args *);
static void hammer2_strategy_read_completion(hammer2_chain_t *,
    const char *, struct buf *);

int
hammer2_strategy(struct vop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;

	switch (bp->b_iocmd) {
	case BIO_READ:
		hammer2_strategy_read(ap);
		break;
	default:
		bp->b_error = EOPNOTSUPP;
		bp->b_ioflags |= BIO_ERROR;
		bufdone(bp);
		break;
	}

	return (0);
}

/*
 * Callback used in read path in case that a block is compressed with LZ4.
 */
static void
hammer2_decompress_LZ4_callback(const char *data, unsigned int bytes,
    struct buf *bp)
{
	char *compressed_buffer;
	int compressed_size;
	int result;

	KKASSERT(bp->b_bufsize <= HAMMER2_PBUFSIZE);
	compressed_size = *(const int *)data;
	KKASSERT((uint32_t)compressed_size <= bytes - sizeof(int));

	compressed_buffer = uma_zalloc(zone_buffer_read, M_WAITOK);
	result = LZ4_decompress_safe(__DECONST(char *, &data[sizeof(int)]),
	    compressed_buffer, compressed_size, bp->b_bufsize);
	if (result < 0) {
		hprintf("error during decompression: buf %016jx/%d\n",
		    (intmax_t)bp->b_offset, bytes);
		/* Make sure it isn't random garbage. */
		bzero(compressed_buffer, bp->b_bufsize);
	}

	KKASSERT(result <= bp->b_bufsize);
	bcopy(compressed_buffer, bp->b_data, bp->b_bufsize);
	if (result < bp->b_bufsize)
		bzero(bp->b_data + result, bp->b_bufsize - result);
	uma_zfree(zone_buffer_read, compressed_buffer);
	bp->b_resid = 0;
}

/*
 * Callback used in read path in case that a block is compressed with ZLIB.
 */
static void
hammer2_decompress_ZLIB_callback(const char *data, unsigned int bytes,
    struct buf *bp)
{
	char *compressed_buffer;
	z_stream strm_decompress;
	int result;

	KKASSERT(bp->b_bufsize <= HAMMER2_PBUFSIZE);
	strm_decompress.avail_in = 0;
	strm_decompress.next_in = Z_NULL;

	result = inflateInit(&strm_decompress);
	if (result != Z_OK)
		hprintf("fatal error in inflateInit\n");

	compressed_buffer = uma_zalloc(zone_buffer_read, M_WAITOK);
	strm_decompress.next_in = __DECONST(char *, data);

	/* XXX Supply proper size, subset of device bp. */
	strm_decompress.avail_in = bytes;
	strm_decompress.next_out = compressed_buffer;
	strm_decompress.avail_out = bp->b_bufsize;

	result = inflate(&strm_decompress, Z_FINISH);
	if (result != Z_STREAM_END) {
		hprintf("fatal error during decompression\n");
		bzero(compressed_buffer, bp->b_bufsize);
	}
	bcopy(compressed_buffer, bp->b_data, bp->b_bufsize);
	result = bp->b_bufsize - strm_decompress.avail_out;
	if (result < bp->b_bufsize)
		bzero(bp->b_data + result, strm_decompress.avail_out);
	uma_zfree(zone_buffer_read, compressed_buffer);
	inflateEnd(&strm_decompress);

	bp->b_resid = 0;
}

/*
 * Logical buffer I/O.
 */
static int
hammer2_strategy_read(struct vop_strategy_args *ap)
{
	hammer2_xop_strategy_t *xop;
	hammer2_inode_t *ip = VTOI(ap->a_vp);
	struct buf *bp = ap->a_bp;
	hammer2_key_t lbase;

	lbase = bp->b_offset;
	KKASSERT(((int)lbase & HAMMER2_PBUFMASK) == 0);

	xop = hammer2_xop_alloc(ip);
	xop->bp = bp;
	xop->lbase = lbase;
	hammer2_xop_start(&xop->head, &hammer2_strategy_read_desc);

	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);

	return (0);
}

/*
 * Backend for hammer2_strategy_read().
 * Do a synchronous lookup of the chain and its data.
 */
void
hammer2_xop_strategy_read(hammer2_xop_t *arg, int clindex)
{
	hammer2_xop_strategy_t *xop = &arg->xop_strategy;
	hammer2_chain_t *chain, *parent;
	hammer2_key_t lbase, key_dummy;
	struct buf *bp;
	const char *data;
	int error;

	lbase = xop->lbase;

	parent = hammer2_inode_chain(xop->head.ip1, clindex,
	    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
	if (parent) {
		chain = hammer2_chain_lookup(&parent, &key_dummy, lbase, lbase,
		    &error, HAMMER2_LOOKUP_ALWAYS | HAMMER2_LOOKUP_SHARED);
		if (chain)
			error = chain->error;
	} else {
		error = HAMMER2_ERROR_EIO;
		chain = NULL;
	}
	error = hammer2_xop_feed(&xop->head, chain, clindex, error);
	if (chain) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}

	bp = xop->bp;
	error = hammer2_xop_collect(&xop->head, 0);
	switch (error) {
	case 0:
		data = hammer2_xop_gdata(&xop->head)->buf;
		hammer2_strategy_read_completion(xop->head.cluster.focus,
		    data, xop->bp);
		hammer2_xop_pdata(&xop->head);
		bufdone(bp);
		break;
	case HAMMER2_ERROR_ENOENT:
		bp->b_resid = 0;
		bp->b_error = 0;
		bzero(bp->b_data, bp->b_bcount);
		bufdone(bp);
		break;
	default:
		hprintf("error %08x at b_offset %016jx\n", error, bp->b_offset);
		bp->b_error = EIO;
		bp->b_ioflags |= BIO_ERROR;
		bufdone(bp);
		break;
	}
}

static void
hammer2_strategy_read_completion(hammer2_chain_t *focus, const char *data,
    struct buf *bp)
{
	if (focus->bref.type == HAMMER2_BREF_TYPE_INODE) {
		/* Copy from in-memory inode structure. */
		bcopy(((const hammer2_inode_data_t *)data)->u.data, bp->b_data,
		    HAMMER2_EMBEDDED_BYTES);
		bzero(bp->b_data + HAMMER2_EMBEDDED_BYTES,
		    bp->b_bcount - HAMMER2_EMBEDDED_BYTES);
		bp->b_resid = 0;
		bp->b_error = 0;
	} else if (focus->bref.type == HAMMER2_BREF_TYPE_DATA) {
		atomic_set_int(&focus->flags, HAMMER2_CHAIN_RELEASE);
		/* Decompression and copy. */
		switch (HAMMER2_DEC_COMP(focus->bref.methods)) {
		case HAMMER2_COMP_LZ4:
			hammer2_decompress_LZ4_callback(data, focus->bytes, bp);
			/* b_resid set by call */
			break;
		case HAMMER2_COMP_ZLIB:
			hammer2_decompress_ZLIB_callback(data, focus->bytes, bp);
			/* b_resid set by call */
			break;
		case HAMMER2_COMP_NONE:
			KKASSERT(focus->bytes <= bp->b_bcount);
			bcopy(data, bp->b_data, focus->bytes);
			if (focus->bytes < bp->b_bcount)
				bzero(bp->b_data + focus->bytes,
				    bp->b_bcount - focus->bytes);
			bp->b_resid = 0;
			bp->b_error = 0;
			break;
		default:
			hpanic("unknown compression type");
		}
	} else {
		hpanic("unknown blockref type %d", focus->bref.type);
	}
}
