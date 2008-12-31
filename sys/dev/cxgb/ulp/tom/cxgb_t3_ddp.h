/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


$FreeBSD: src/sys/dev/cxgb/ulp/tom/cxgb_t3_ddp.h,v 1.3.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

***************************************************************************/

#ifndef T3_DDP_H
#define T3_DDP_H

/* Should be 1 or 2 indicating single or double kernel buffers. */
#define NUM_DDP_KBUF 2

/* min receive window for a connection to be considered for DDP */
#define MIN_DDP_RCV_WIN (48 << 10)

/* amount of Rx window not available to DDP to avoid window exhaustion */
#define DDP_RSVD_WIN (16 << 10)

/* # of sentinel invalid page pods at the end of a group of valid page pods */
#define NUM_SENTINEL_PPODS 0

/* # of pages a pagepod can hold without needing another pagepod */
#define PPOD_PAGES 4

/* page pods are allocated in groups of this size (must be power of 2) */
#define PPOD_CLUSTER_SIZE 16

/* for each TID we reserve this many page pods up front */
#define RSVD_PPODS_PER_TID 1

struct pagepod {
	uint32_t pp_vld_tid;
	uint32_t pp_pgsz_tag_color;
	uint32_t pp_max_offset;
	uint32_t pp_page_offset;
	uint64_t pp_rsvd;
	uint64_t pp_addr[5];
};

#define PPOD_SIZE sizeof(struct pagepod)

#define S_PPOD_TID    0
#define M_PPOD_TID    0xFFFFFF
#define V_PPOD_TID(x) ((x) << S_PPOD_TID)

#define S_PPOD_VALID    24
#define V_PPOD_VALID(x) ((x) << S_PPOD_VALID)
#define F_PPOD_VALID    V_PPOD_VALID(1U)

#define S_PPOD_COLOR    0
#define M_PPOD_COLOR    0x3F
#define V_PPOD_COLOR(x) ((x) << S_PPOD_COLOR)

#define S_PPOD_TAG    6
#define M_PPOD_TAG    0xFFFFFF
#define V_PPOD_TAG(x) ((x) << S_PPOD_TAG)

#define S_PPOD_PGSZ    30
#define M_PPOD_PGSZ    0x3
#define V_PPOD_PGSZ(x) ((x) << S_PPOD_PGSZ)

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <machine/bus.h>

/* DDP gather lists can specify an offset only for the first page. */
struct ddp_gather_list {
	unsigned int	dgl_length;
	unsigned int	dgl_offset;
	unsigned int	dgl_nelem;
	vm_page_t   	dgl_pages[0];
};

struct ddp_buf_state {
	unsigned int cur_offset;     /* offset of latest DDP notification */
	unsigned int flags;
	struct ddp_gather_list *gl;
};

struct ddp_state {
	struct ddp_buf_state buf_state[2];   /* per buffer state */
	int cur_buf;
	unsigned short kbuf_noinval;
	unsigned short kbuf_idx;        /* which HW buffer is used for kbuf */
	struct ddp_gather_list *ubuf;
	int user_ddp_pending;
	unsigned int ubuf_nppods;       /* # of page pods for buffer 1 */
	unsigned int ubuf_tag;
	unsigned int ubuf_ddp_ready;
	int cancel_ubuf;
	int get_tcb_count;
	unsigned int kbuf_posted;
	unsigned int kbuf_nppods[NUM_DDP_KBUF];
	unsigned int kbuf_tag[NUM_DDP_KBUF];
	struct ddp_gather_list *kbuf[NUM_DDP_KBUF]; /* kernel buffer for DDP prefetch */
};

/* buf_state flags */
enum {
	DDP_BF_NOINVAL = 1 << 0,   /* buffer is set to NO_INVALIDATE */
	DDP_BF_NOCOPY  = 1 << 1,   /* DDP to final dest, no copy needed */
	DDP_BF_NOFLIP  = 1 << 2,   /* buffer flips after GET_TCB_RPL */
	DDP_BF_PSH     = 1 << 3,   /* set in skb->flags if the a DDP was 
	                              completed with a segment having the
				      PSH flag set */
	DDP_BF_NODATA  = 1 << 4,   /* buffer completed before filling */ 
};

#include <dev/cxgb/ulp/tom/cxgb_toepcb.h>
struct sockbuf;

/*
 * Returns 1 if a UBUF DMA buffer might be active.
 */
static inline int
t3_ddp_ubuf_pending(struct toepcb *toep)
{
	struct ddp_state *p = &toep->tp_ddp_state;

	/* When the TOM_TUNABLE(ddp) is enabled, we're always in ULP_MODE DDP,
	 * but DDP_STATE() is only valid if the connection actually enabled
	 * DDP.
	 */
	if (p->kbuf[0] == NULL)
		return (0);

	return (p->buf_state[0].flags & (DDP_BF_NOFLIP | DDP_BF_NOCOPY)) || 
	       (p->buf_state[1].flags & (DDP_BF_NOFLIP | DDP_BF_NOCOPY));
}

int t3_setup_ppods(struct toepcb *toep, const struct ddp_gather_list *gl,
		   unsigned int nppods, unsigned int tag, unsigned int maxoff,
		   unsigned int pg_off, unsigned int color);
int t3_alloc_ppods(struct tom_data *td, unsigned int n, int *tag);
void t3_free_ppods(struct tom_data *td, unsigned int tag, unsigned int n);
void t3_free_ddp_gl(struct ddp_gather_list *gl);
int t3_ddp_copy(const struct mbuf *m, int offset, struct uio *uio, int len);
//void t3_repost_kbuf(struct socket *so, int modulate, int activate);
void t3_post_kbuf(struct toepcb *toep, int modulate, int nonblock);
int t3_post_ubuf(struct toepcb *toep, const struct uio *uio, int nonblock,
		 int rcv_flags, int modulate, int post_kbuf);
void t3_cancel_ubuf(struct toepcb *toep, struct sockbuf *rcv);
int t3_overlay_ubuf(struct toepcb *toep, struct sockbuf *rcv,
    const struct uio *uio, int nonblock,
    int rcv_flags, int modulate, int post_kbuf);
int t3_enter_ddp(struct toepcb *toep, unsigned int kbuf_size, unsigned int waitall, int nonblock);
void t3_cleanup_ddp(struct toepcb *toep);
void t3_release_ddp_resources(struct toepcb *toep);
void t3_cancel_ddpbuf(struct toepcb *, unsigned int bufidx);
void t3_overlay_ddpbuf(struct toepcb *, unsigned int bufidx, unsigned int tag0,
		       unsigned int tag1, unsigned int len);
void t3_setup_ddpbufs(struct toepcb *, unsigned int len0, unsigned int offset0,
		      unsigned int len1, unsigned int offset1,
		      uint64_t ddp_flags, uint64_t flag_mask, int modulate);
#endif  /* T3_DDP_H */
