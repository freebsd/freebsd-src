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
#ifndef  __CXIO_HAL_H__
#define  __CXIO_HAL_H__
#include <sys/condvar.h>
#include <sys/ktr.h>

#define T3_CTRL_QP_ID    FW_RI_SGEEC_START
#define T3_CTL_QP_TID	 FW_RI_TID_START
#define T3_CTRL_QP_SIZE_LOG2  8
#define T3_CTRL_CQ_ID    0

/* TBD */
#define T3_MAX_NUM_RI (1<<15)
#define T3_MAX_NUM_QP (1<<15)
#define T3_MAX_NUM_CQ (1<<15)
#define T3_MAX_NUM_PD (1<<15)
#define T3_MAX_PBL_SIZE 256
#define T3_MAX_RQ_SIZE 1024
#define T3_MAX_QP_DEPTH (T3_MAX_RQ_SIZE-1)
#define T3_MAX_CQ_DEPTH 65536
#define T3_MAX_NUM_STAG (1<<15)
#define T3_MAX_MR_SIZE 0x100000000ULL
#define T3_PAGESIZE_MASK 0xffff000  /* 4KB-128MB */

#define T3_STAG_UNSET 0xffffffff

#define T3_MAX_DEV_NAME_LEN 32

struct cxio_hal_ctrl_qp {
	u32 wptr;
	u32 rptr;
	struct mtx lock;	/* for the wtpr, can sleep */
	union t3_wr *workq;	/* the work request queue */
	bus_addr_t dma_addr;	/* pci bus address of the workq */
	void *doorbell;
};

struct cxio_hal_resource {
	struct buf_ring *tpt_fifo;
	struct mtx tpt_fifo_lock;
	struct buf_ring *qpid_fifo;
	struct mtx qpid_fifo_lock;
	struct buf_ring *cqid_fifo;
	struct mtx cqid_fifo_lock;
	struct buf_ring *pdid_fifo;
	struct mtx pdid_fifo_lock;
};

struct cxio_qpid {
	TAILQ_ENTRY(cxio_qpid) entry;
	u32 qpid;
};

struct cxio_ucontext {
	TAILQ_HEAD(, cxio_qpid) qpids;
	struct mtx lock;
};

struct cxio_rdev {
	struct adapter *adap;
	struct rdma_info rnic_info;
	struct cxio_hal_resource *rscp;
	struct cxio_hal_ctrl_qp ctrl_qp;
	unsigned long qpshift;
	u32 qpnr;
	u32 qpmask;
	struct cxio_ucontext uctx;
	struct gen_pool *pbl_pool;
	struct gen_pool *rqt_pool;
	struct ifnet *ifp;
	TAILQ_ENTRY(cxio_rdev) entry;
};

static __inline int
cxio_num_stags(struct cxio_rdev *rdev_p)
{
	return min((int)T3_MAX_NUM_STAG, (int)((rdev_p->rnic_info.tpt_top - rdev_p->rnic_info.tpt_base) >> 5));
}

typedef void (*cxio_hal_ev_callback_func_t) (struct cxio_rdev * rdev_p,
					     struct mbuf * m);

#define RSPQ_CQID(rsp) (be32toh(rsp->cq_ptrid) & 0xffff)
#define RSPQ_CQPTR(rsp) ((be32toh(rsp->cq_ptrid) >> 16) & 0xffff)
#define RSPQ_GENBIT(rsp) ((be32toh(rsp->flags) >> 16) & 1)
#define RSPQ_OVERFLOW(rsp) ((be32toh(rsp->flags) >> 17) & 1)
#define RSPQ_AN(rsp) ((be32toh(rsp->flags) >> 18) & 1)
#define RSPQ_SE(rsp) ((be32toh(rsp->flags) >> 19) & 1)
#define RSPQ_NOTIFY(rsp) ((be32toh(rsp->flags) >> 20) & 1)
#define RSPQ_CQBRANCH(rsp) ((be32toh(rsp->flags) >> 21) & 1)
#define RSPQ_CREDIT_THRESH(rsp) ((be32toh(rsp->flags) >> 22) & 1)

struct respQ_msg_t {
	__be32 flags;		/* flit 0 */
	__be32 cq_ptrid;
	__be64 rsvd;		/* flit 1 */
	struct t3_cqe cqe;	/* flits 2-3 */
};

enum t3_cq_opcode {
	CQ_ARM_AN = 0x2,
	CQ_ARM_SE = 0x6,
	CQ_FORCE_AN = 0x3,
	CQ_CREDIT_UPDATE = 0x7
};

int cxio_rdev_open(struct cxio_rdev *rdev);
void cxio_rdev_close(struct cxio_rdev *rdev);
int cxio_hal_cq_op(struct cxio_rdev *rdev, struct t3_cq *cq,
		   enum t3_cq_opcode op, u32 credit);
int cxio_create_cq(struct cxio_rdev *rdev, struct t3_cq *cq, int kernel);
int cxio_destroy_cq(struct cxio_rdev *rdev, struct t3_cq *cq);
void cxio_release_ucontext(struct cxio_rdev *rdev, struct cxio_ucontext *uctx);
void cxio_init_ucontext(struct cxio_rdev *rdev, struct cxio_ucontext *uctx);
int cxio_create_qp(struct cxio_rdev *rdev, u32 kernel_domain, struct t3_wq *wq,
		   struct cxio_ucontext *uctx);
int cxio_destroy_qp(struct cxio_rdev *rdev, struct t3_wq *wq,
		    struct cxio_ucontext *uctx);
int cxio_peek_cq(struct t3_wq *wr, struct t3_cq *cq, int opcode);
int cxio_write_pbl(struct cxio_rdev *rdev_p, __be64 *pbl,
		   u32 pbl_addr, u32 pbl_size);
int cxio_register_phys_mem(struct cxio_rdev *rdev, u32 * stag, u32 pdid,
			   enum tpt_mem_perm perm, u32 zbva, u64 to, u32 len,
			   u8 page_size, u32 pbl_size, u32 pbl_addr);
int cxio_reregister_phys_mem(struct cxio_rdev *rdev, u32 * stag, u32 pdid,
			   enum tpt_mem_perm perm, u32 zbva, u64 to, u32 len,
			   u8 page_size, u32 pbl_size, u32 pbl_addr);
int cxio_dereg_mem(struct cxio_rdev *rdev, u32 stag, u32 pbl_size,
		   u32 pbl_addr);
int cxio_allocate_window(struct cxio_rdev *rdev, u32 * stag, u32 pdid);
int cxio_deallocate_window(struct cxio_rdev *rdev, u32 stag);
int cxio_rdma_init(struct cxio_rdev *rdev, struct t3_rdma_init_attr *attr,
    struct socket *so);
u32 cxio_hal_get_pdid(struct cxio_hal_resource *rscp);
void cxio_hal_put_pdid(struct cxio_hal_resource *rscp, u32 pdid);
int cxio_hal_init(struct adapter *);
void cxio_hal_uninit(struct adapter *);
void cxio_hal_exit(void);
int cxio_flush_rq(struct t3_wq *wq, struct t3_cq *cq, int count);
int cxio_flush_sq(struct t3_wq *wq, struct t3_cq *cq, int count);
void cxio_count_rcqes(struct t3_cq *cq, struct t3_wq *wq, int *count);
void cxio_count_scqes(struct t3_cq *cq, struct t3_wq *wq, int *count);
void cxio_flush_hw_cq(struct t3_cq *cq);
int cxio_poll_cq(struct t3_wq *wq, struct t3_cq *cq, struct t3_cqe *cqe,
		     u8 *cqe_flushed, u64 *cookie, u32 *credit);

#define MOD "iw_cxgb: "

#ifdef INVARIANTS
void cxio_dump_tpt(struct cxio_rdev *rev, u32 stag);
void cxio_dump_pbl(struct cxio_rdev *rev, u32 pbl_addr, uint32_t len, u8 shift);
void cxio_dump_wqe(union t3_wr *wqe);
void cxio_dump_wce(struct t3_cqe *wce);
void cxio_dump_rqt(struct cxio_rdev *rdev, u32 hwtid, int nents);
void cxio_dump_tcb(struct cxio_rdev *rdev, u32 hwtid);
#endif

#define cxfree(a) free((a), M_DEVBUF);

#include <sys/blist.h>
struct gen_pool {
	blist_t  	gen_list;
	daddr_t  	gen_base;
	int      	gen_chunk_shift;
	struct mtx 	gen_lock;
};

static __inline struct gen_pool *
gen_pool_create(daddr_t base, u_int chunk_shift, u_int len)
{
	struct gen_pool *gp;

	gp = malloc(sizeof(struct gen_pool), M_DEVBUF, M_NOWAIT);
	if (gp == NULL)
		return (NULL);
	
	memset(gp, 0, sizeof(struct gen_pool));
	gp->gen_list = blist_create(len >> chunk_shift, M_NOWAIT);
	if (gp->gen_list == NULL) {
		free(gp, M_DEVBUF);
		return (NULL);
	}
	blist_free(gp->gen_list, 0, len >> chunk_shift);
	gp->gen_base = base;
	gp->gen_chunk_shift = chunk_shift;
	mtx_init(&gp->gen_lock, "genpool", NULL, MTX_DUPOK|MTX_DEF);

	return (gp);
}

static __inline unsigned long
gen_pool_alloc(struct gen_pool *gp, int size)
{
	int chunks;
	daddr_t blkno; 

	chunks = (size + (1<<gp->gen_chunk_shift) - 1) >> gp->gen_chunk_shift;
	mtx_lock(&gp->gen_lock);
	blkno = blist_alloc(gp->gen_list, chunks);
	mtx_unlock(&gp->gen_lock);

	if (blkno == SWAPBLK_NONE)
		return (0);

	return (gp->gen_base + ((1 << gp->gen_chunk_shift) * blkno));
}

static __inline void
gen_pool_free(struct gen_pool *gp, daddr_t address, int size)
{
	int chunks;
	daddr_t blkno;
	
	chunks = (size + (1<<gp->gen_chunk_shift) - 1) >> gp->gen_chunk_shift;
	blkno = (address - gp->gen_base) / (1 << gp->gen_chunk_shift);
	mtx_lock(&gp->gen_lock);
	blist_free(gp->gen_list, blkno, chunks);
	mtx_unlock(&gp->gen_lock);
}

static __inline void
gen_pool_destroy(struct gen_pool *gp)
{
	blist_destroy(gp->gen_list);
	free(gp, M_DEVBUF);
}

#define cxio_wait(ctx, lockp, cond) \
({ \
	int __ret = 0; \
	mtx_lock(lockp); \
	while (!cond) { \
                msleep(ctx, lockp, 0, "cxio_wait", hz); \
                if (SIGPENDING(curthread)) { \
			__ret = ERESTART; \
                        break; \
                } \
	} \
	mtx_unlock(lockp); \
	__ret; \
}) 

#define KTR_IW_CXGB KTR_SPARE3

#endif
