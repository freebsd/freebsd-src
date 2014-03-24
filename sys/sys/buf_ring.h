/*-
 * Copyright (c) 2007-2009 Kip Macy <kmacy@freebsd.org>
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
 * $FreeBSD$
 *
 */

#ifndef	_SYS_BUF_RING_H_
#define	_SYS_BUF_RING_H_

#include <machine/cpu.h>

#if defined(INVARIANTS) && !defined(DEBUG_BUFRING)
#define DEBUG_BUFRING 1
#endif

#ifdef DEBUG_BUFRING
#include <sys/lock.h>
#include <sys/mutex.h>
#endif

struct buf_ring {
	volatile uint32_t	br_prod_head;
	volatile uint32_t	br_prod_tail;	
	int              	br_prod_size;
	int              	br_prod_mask;
	uint64_t		br_drops;
	volatile uint32_t	br_cons_head __aligned(CACHE_LINE_SIZE);
	volatile uint32_t	br_cons_tail;
	int		 	br_cons_size;
	int              	br_cons_mask;
#ifdef DEBUG_BUFRING
	struct mtx		*br_lock;
#endif	
	void			*br_ring[0] __aligned(CACHE_LINE_SIZE);
};

/*
 * multi-producer safe lock-free ring buffer enqueue
 *
 */
int buf_ring_enqueue(struct buf_ring *br, void *buf);
/*
 * multi-consumer safe dequeue 
 *
 */
void *buf_ring_dequeue_mc(struct buf_ring *br);
/*
 * single-consumer dequeue 
 * use where dequeue is protected by a lock
 * e.g. a network driver's tx queue lock
 */
void *buf_ring_dequeue_sc(struct buf_ring *br);
/*
 * single-consumer advance after a peek
 * use where it is protected by a lock
 * e.g. a network driver's tx queue lock
 */
void buf_ring_advance_sc(struct buf_ring *br);
void buf_ring_advance_mc(struct buf_ring *br);
/*
 * Used to return a buffer (most likely already there)
 * to the top od the ring. The caller should *not*
 * have used any dequeue to pull it out of the ring
 * but instead should have used the peek() function.
 * This is normally used where the transmit queue
 * of a driver is full, and an mubf must be returned.
 * Most likely whats in the ring-buffer is what
 * is being put back (since it was not removed), but
 * sometimes the lower transmit function may have
 * done a pullup or other function that will have
 * changed it. As an optimzation we always put it
 * back (since jhb says the store is probably cheaper),
 * if we have to do a multi-queue version we will need
 * the compare and an atomic.
 */
void buf_ring_putback_mc(struct buf_ring *br, void *new);
void buf_ring_putback_sc(struct buf_ring *br, void *new);
/*
 * return a pointer to the first entry in the ring
 * without modifying it, or NULL if the ring is empty
 * race-prone if not protected by a lock
 */
void *buf_ring_peek(struct buf_ring *br);

int buf_ring_full(struct buf_ring *br);

int buf_ring_empty(struct buf_ring *br);

int buf_ring_count(struct buf_ring *br);

struct buf_ring *buf_ring_alloc(int count, struct malloc_type *type, int flags,
    struct mtx *);

void buf_ring_free(struct buf_ring *br, struct malloc_type *type);

int buf_ring_mbufon(struct buf_ring *br, void *buf);


#endif
