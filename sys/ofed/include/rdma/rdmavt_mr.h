#ifndef DEF_RDMAVT_INCMR_H
#define DEF_RDMAVT_INCMR_H

/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * Copyright(c) 2016 Intel Corporation.
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * For Memory Regions. This stuff should probably be moved into rdmavt/mr.h once
 * drivers no longer need access to the MR directly.
 */

/*
 * A segment is a linear region of low physical memory.
 * Used by the verbs layer.
 */
struct rvt_seg {
	void *vaddr;
	size_t length;
};

/* The number of rvt_segs that fit in a page. */
#define RVT_SEGSZ     (PAGE_SIZE / sizeof(struct rvt_seg))

struct rvt_segarray {
	struct rvt_seg segs[RVT_SEGSZ];
};

struct rvt_mregion {
	struct ib_pd *pd;       /* shares refcnt of ibmr.pd */
	u64 user_base;          /* User's address for this region */
	u64 iova;               /* IB start address of this region */
	size_t length;
	u32 lkey;
	u32 offset;             /* offset (bytes) to start of region */
	int access_flags;
	u32 max_segs;           /* number of rvt_segs in all the arrays */
	u32 mapsz;              /* size of the map array */
	u8  page_shift;         /* 0 - non unform/non powerof2 sizes */
	u8  lkey_published;     /* in global table */
	atomic_t lkey_invalid;	/* true if current lkey is invalid */
	struct completion comp; /* complete when refcount goes to zero */
	atomic_t refcount;
	struct rvt_segarray *map[0];    /* the segments */
};

#define RVT_MAX_LKEY_TABLE_BITS 23

struct rvt_lkey_table {
	spinlock_t lock; /* protect changes in this struct */
	u32 next;               /* next unused index (speeds search) */
	u32 gen;                /* generation count */
	u32 max;                /* size of the table */
	struct rvt_mregion __rcu **table;
};

/*
 * These keep track of the copy progress within a memory region.
 * Used by the verbs layer.
 */
struct rvt_sge {
	struct rvt_mregion *mr;
	void *vaddr;            /* kernel virtual address of segment */
	u32 sge_length;         /* length of the SGE */
	u32 length;             /* remaining length of the segment */
	u16 m;                  /* current index: mr->map[m] */
	u16 n;                  /* current index: mr->map[m]->segs[n] */
};

struct rvt_sge_state {
	struct rvt_sge *sg_list;      /* next SGE to be used if any */
	struct rvt_sge sge;   /* progress state for the current SGE */
	u32 total_len;
	u8 num_sge;
};

static inline void rvt_put_mr(struct rvt_mregion *mr)
{
	if (unlikely(atomic_dec_and_test(&mr->refcount)))
		complete(&mr->comp);
}

static inline void rvt_get_mr(struct rvt_mregion *mr)
{
	atomic_inc(&mr->refcount);
}

static inline void rvt_put_ss(struct rvt_sge_state *ss)
{
	while (ss->num_sge) {
		rvt_put_mr(ss->sge.mr);
		if (--ss->num_sge)
			ss->sge = *ss->sg_list++;
	}
}

#endif          /* DEF_RDMAVT_INCMRH */
