/**************************************************************************

Copyright (c) 2007, 2008 Chelsio Inc.
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

$FreeBSD$

***************************************************************************/

#ifndef __IWCH_H__
#define __IWCH_H__

struct iwch_pd;
struct iwch_cq;
struct iwch_qp;
struct iwch_mr;


struct iwch_rnic_attributes {
	u32 vendor_id;
	u32 vendor_part_id;
	u32 max_qps;
	u32 max_wrs;				/* Max for any SQ/RQ */
	u32 max_sge_per_wr;
	u32 max_sge_per_rdma_write_wr;	/* for RDMA Write WR */
	u32 max_cqs;
	u32 max_cqes_per_cq;
	u32 max_mem_regs;
	u32 max_phys_buf_entries;		/* for phys buf list */
	u32 max_pds;

	/*
	 * The memory page sizes supported by this RNIC.
	 * Bit position i in bitmap indicates page of
	 * size (4k)^i.  Phys block list mode unsupported.
	 */
	u32 mem_pgsizes_bitmask;
	u8 can_resize_wq;

	/*
	 * The maximum number of RDMA Reads that can be outstanding
	 * per QP with this RNIC as the target.
	 */
	u32 max_rdma_reads_per_qp;

	/*
	 * The maximum number of resources used for RDMA Reads
	 * by this RNIC with this RNIC as the target.
	 */
	u32 max_rdma_read_resources;

	/*
	 * The max depth per QP for initiation of RDMA Read
	 * by this RNIC.
	 */
	u32 max_rdma_read_qp_depth;

	/*
	 * The maximum depth for initiation of RDMA Read
	 * operations by this RNIC on all QPs
	 */
	u32 max_rdma_read_depth;
	u8 rq_overflow_handled;
	u32 can_modify_ird;
	u32 can_modify_ord;
	u32 max_mem_windows;
	u32 stag0_value;
	u8 zbva_support;
	u8 local_invalidate_fence;
	u32 cq_overflow_detection;
};

struct iwch_dev {
	struct ib_device ibdev;
	struct cxio_rdev rdev;
	u32 device_cap_flags;
	struct iwch_rnic_attributes attr;
	struct kvl cqidr;
	struct kvl qpidr;
	struct kvl mmidr;
	struct mtx lock;
	TAILQ_ENTRY(iwch_dev) entry;
};

#ifndef container_of
#define container_of(p, stype, field) ((stype *)(((uint8_t *)(p)) - offsetof(stype, field)))
#endif

static inline struct iwch_dev *to_iwch_dev(struct ib_device *ibdev)
{
	return container_of(ibdev, struct iwch_dev, ibdev);
}

static inline int t3b_device(const struct iwch_dev *rhp)
{
	return rhp->rdev.t3cdev_p->type == T3B;
}

static inline int t3a_device(const struct iwch_dev *rhp)
{
	return rhp->rdev.t3cdev_p->type == T3A;
}

static inline struct iwch_cq *get_chp(struct iwch_dev *rhp, u32 cqid)
{
	return kvl_lookup(&rhp->cqidr, cqid);
}

static inline struct iwch_qp *get_qhp(struct iwch_dev *rhp, u32 qpid)
{
	return kvl_lookup(&rhp->qpidr, qpid);
}

static inline struct iwch_mr *get_mhp(struct iwch_dev *rhp, u32 mmid)
{
	return kvl_lookup(&rhp->mmidr, mmid);
}

static inline int insert_handle(struct iwch_dev *rhp, struct kvl *kvlp,
				void *handle, u32 id)
{
	int ret;
	u32 newid;

	do {
		mtx_lock(&rhp->lock);
		ret = kvl_alloc_above(kvlp, handle, id, &newid);
		WARN_ON(ret != 0);
		WARN_ON(!ret && newid != id);
		mtx_unlock(&rhp->lock);
	} while (ret == -EAGAIN);

	return ret;
}

static inline void remove_handle(struct iwch_dev *rhp, struct kvl *kvlp, u32 id)
{
	mtx_lock(&rhp->lock);
	kvl_delete(kvlp, id);
	mtx_unlock(&rhp->lock);
}

extern struct cxgb_client t3c_client;
extern cxgb_cpl_handler_func t3c_handlers[NUM_CPL_CMDS];
extern void iwch_ev_dispatch(struct cxio_rdev *rdev_p, struct mbuf *m);
#endif
