/*
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * This implements pools of DMA-able buffers that conserve DMA address space
 * by putting several buffers into one page and that allow to map between
 * 32-bit handles for the buffer and buffer addresses (to use 32-bit network
 * interfaces on 64bit machines). This assists network interfaces that may need
 * huge numbers of mbufs.
 *
 * $FreeBSD$
 */
#ifndef _SYS_MBPOOL_H_
#define	_SYS_MBPOOL_H_

#ifdef _KERNEL

#include <sys/queue.h>

/* opaque */
struct mbpool;

/* size of reserved area at end of each chunk */
#define	MBPOOL_TRAILER_SIZE	4

/* maximum value of max_pages */
#define	MBPOOL_MAX_MAXPAGES	((1 << 14) - 1)

/* maximum number of chunks per page */
#define	MBPOOL_MAX_CHUNKS	(1 << 9)

/* initialize a pool */
int mbp_create(struct mbpool **, const char *, bus_dma_tag_t, u_int,
	size_t, size_t);

/* destroy a pool */
void mbp_destroy(struct mbpool *);

/* allocate a chunk and set used and on card */
void *mbp_alloc(struct mbpool *, bus_addr_t *, uint32_t *);

/* free a chunk */
void mbp_free(struct mbpool *, void *);

/* free a chunk that is an external mbuf */
void mbp_ext_free(void *, void *);

/* free all buffers that are marked to be on the card */
void mbp_card_free(struct mbpool *);

/* count used buffers and buffers on card */
void mbp_count(struct mbpool *, u_int *, u_int *, u_int *);

/* get the buffer from a handle and clear card bit */
void *mbp_get(struct mbpool *, uint32_t);

/* get the buffer from a handle and don't clear card bit */
void *mbp_get_keep(struct mbpool *, uint32_t);

/* sync the chunk */
void mbp_sync(struct mbpool *, uint32_t, bus_addr_t, bus_size_t, u_int);

#endif	/* _KERNEL */
#endif	/* _SYS_MBPOOL_H_ */
