/*
 * Copyright (c) 2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __BNXT_RE_MEMORY_H__
#define __BNXT_RE_MEMORY_H__

#include <pthread.h>

#include "main.h"

struct bnxt_re_mem {
	void *va_head;
	void *va_tail;
	uint32_t head;
	uint32_t tail;
	uint32_t size;
	uint32_t pad;
};

#define BNXT_RE_QATTR_SQ_INDX	0
#define BNXT_RE_QATTR_RQ_INDX	1
struct bnxt_re_qattr {
	uint32_t esize;
	uint32_t slots;
	uint32_t nwr;
	uint32_t sz_ring;
	uint32_t sz_shad;
};

/* spin lock wrapper struct */
struct bnxt_spinlock {
	pthread_spinlock_t lock;
	int in_use;
	int need_lock;
};

struct bnxt_re_queue {
	struct bnxt_spinlock qlock;
	uint32_t flags;
	uint32_t *dbtail;
	void *va;
	uint32_t head;
	uint32_t depth; /* no of entries */
	void *pad;
	uint32_t pad_stride_log2;
	uint32_t tail;
	uint32_t max_slots;
	/* Represents the difference between the real queue depth allocated in
	 * HW and the user requested queue depth and is used to correctly flag
	 * queue full condition based on user supplied queue depth.
	 * This value can vary depending on the type of queue and any HW
	 * requirements that mandate keeping a fixed gap between the producer
	 * and the consumer indices in the queue
	 */
	uint32_t diff;
	uint32_t stride;
	uint32_t msn;
	uint32_t msn_tbl_sz;
};

static inline unsigned long get_aligned(uint32_t size, uint32_t al_size)
{
	return (unsigned long) (size + al_size - 1) & ~(al_size - 1);
}

static inline unsigned long roundup_pow_of_two(unsigned long val)
{
	unsigned long roundup = 1;

	if (val == 1)
		return (roundup << 1);

	while (roundup < val)
		roundup <<= 1;

	return roundup;
}

#define iowrite64(dst, val)	(*((volatile __u64 *) (dst)) = val)
#define iowrite32(dst, val)	(*((volatile __u32 *) (dst)) = val)

/* Basic queue operation */
static inline void *bnxt_re_get_hwqe(struct bnxt_re_queue *que, uint32_t idx)
{
	idx += que->tail;
	if (idx >= que->depth)
		idx -= que->depth;
	return (void *)(que->va + (idx << 4));
}

static inline void *bnxt_re_get_hwqe_hdr(struct bnxt_re_queue *que)
{
	return (void *)(que->va + ((que->tail) << 4));
}

static inline uint32_t bnxt_re_is_que_full(struct bnxt_re_queue *que,
					   uint32_t slots)
{
	int32_t avail, head, tail;

	head = que->head;
	tail = que->tail;
	avail = head - tail;
	if (head <= tail)
		avail += que->depth;
	return avail <= (slots + que->diff);
}

static inline uint32_t bnxt_re_is_que_empty(struct bnxt_re_queue *que)
{
	return que->tail == que->head;
}

static inline void bnxt_re_incr_tail(struct bnxt_re_queue *que, uint8_t cnt)
{
	que->tail += cnt;
	if (que->tail >= que->depth) {
		que->tail %= que->depth;
		/* Rolled over, Toggle Tail bit in epoch flags */
		que->flags ^= 1UL << BNXT_RE_FLAG_EPOCH_TAIL_SHIFT;
	}
}

static inline void bnxt_re_incr_head(struct bnxt_re_queue *que, uint8_t cnt)
{
	que->head += cnt;
	if (que->head >= que->depth) {
		que->head %= que->depth;
		/* Rolled over, Toggle HEAD bit in epoch flags */
		que->flags ^= 1UL << BNXT_RE_FLAG_EPOCH_HEAD_SHIFT;
	}

}

void bnxt_re_free_mem(struct bnxt_re_mem *mem);
void *bnxt_re_alloc_mem(size_t size, uint32_t pg_size);
void *bnxt_re_get_obj(struct bnxt_re_mem *mem, size_t req);
void *bnxt_re_get_ring(struct bnxt_re_mem *mem, size_t req);

#endif
