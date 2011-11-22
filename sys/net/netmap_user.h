/*
 * Copyright (C) 2011 Matteo Landi, Luigi Rizzo. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the
 *      distribution.
 * 
 *   3. Neither the name of the authors nor the names of their contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY MATTEO LANDI AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTEO LANDI OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $FreeBSD$
 * $Id: netmap_user.h 9495 2011-10-18 15:28:23Z luigi $
 *
 * This header contains the macros used to manipulate netmap structures
 * and packets in userspace. See netmap(4) for more information.
 *
 * The address of the struct netmap_if, say nifp, is determined
 * by the value returned from ioctl(.., NIOCREG, ...) and the mmap
 * region:
 *	ioctl(fd, NIOCREG, &req);
 *	mem = mmap(0, ... );
 *	nifp = NETMAP_IF(mem, req.nr_nifp);
 *		(so simple, we could just do it manually)
 *
 * From there:
 *	struct netmap_ring *NETMAP_TXRING(nifp, index)
 *	struct netmap_ring *NETMAP_RXRING(nifp, index)
 *		we can access ring->nr_cur, ring->nr_avail, ring->nr_flags
 *
 *	ring->slot[i] gives us the i-th slot (we can access
 *		directly plen, flags, bufindex)
 *
 *	char *buf = NETMAP_BUF(ring, index) returns a pointer to
 *		the i-th buffer
 *
 * Since rings are circular, we have macros to compute the next index
 *	i = NETMAP_RING_NEXT(ring, i);
 */

#ifndef _NET_NETMAP_USER_H_
#define _NET_NETMAP_USER_H_

#define NETMAP_IF(b, o)	(struct netmap_if *)((char *)(b) + (o))

#define NETMAP_TXRING(nifp, index)			\
	((struct netmap_ring *)((char *)(nifp) +	\
		(nifp)->ring_ofs[index] ) )

#define NETMAP_RXRING(nifp, index)			\
	((struct netmap_ring *)((char *)(nifp) +	\
	    (nifp)->ring_ofs[index + (nifp)->ni_num_queues+1] ) )

#if NETMAP_BUF_SIZE != 2048
#error cannot handle odd size
#define NETMAP_BUF(ring, index)				\
	((char *)(ring) + (ring)->buf_ofs + ((index)*NETMAP_BUF_SIZE))
#else
#define NETMAP_BUF(ring, index)				\
	((char *)(ring) + (ring)->buf_ofs + ((index)<<11))
#endif

#define	NETMAP_RING_NEXT(r, i)				\
	((i)+1 == (r)->num_slots ? 0 : (i) + 1 )

/*
 * Return 1 if the given tx ring is empty.
 *
 * @r netmap_ring descriptor pointer.
 * Special case, a negative value in hwavail indicates that the
 * transmit queue is idle.
 * XXX revise
 */
#define NETMAP_TX_RING_EMPTY(r)	((r)->avail >= (r)->num_slots - 1)

#endif /* _NET_NETMAP_USER_H_ */
