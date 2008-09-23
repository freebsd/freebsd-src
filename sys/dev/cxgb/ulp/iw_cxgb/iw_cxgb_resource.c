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

***************************************************************************/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/pciio.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/libkern.h>

#include <netinet/in.h>

#include <contrib/rdma/ib_verbs.h>
#include <contrib/rdma/ib_umem.h>
#include <contrib/rdma/ib_user_verbs.h>

#include <cxgb_include.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>
#include <ulp/iw_cxgb/iw_cxgb_resource.h>
#include <ulp/iw_cxgb/iw_cxgb_user.h>

#ifdef needed
static struct buf_ring *rhdl_fifo;
static struct mtx rhdl_fifo_lock;
#endif

#define RANDOM_SIZE 16

static int __cxio_init_resource_fifo(struct buf_ring **fifo,
				   struct mtx *fifo_lock,
				   u32 nr, u32 skip_low,
				   u32 skip_high,
				   int randomize)
{
	u32 i, j, idx;
	u32 random_bytes;
	u32 rarray[16];
	mtx_init(fifo_lock, "cxio fifo", NULL, MTX_DEF|MTX_DUPOK);

	*fifo = buf_ring_alloc(nr, M_NOWAIT);
	if (*fifo == NULL)
		return (-ENOMEM);
#if 0
	for (i = 0; i < skip_low + skip_high; i++) {
		u32 entry = 0;
		
		buf_ring_enqueue(*fifo, (uintptr_t) entry);
	}
#endif	
	if (randomize) {
		j = 0;
		random_bytes = random();
		for (i = 0; i < RANDOM_SIZE; i++)
			rarray[i] = i + skip_low;
		for (i = skip_low + RANDOM_SIZE; i < nr - skip_high; i++) {
			if (j >= RANDOM_SIZE) {
				j = 0;
				random_bytes = random();
			}
			idx = (random_bytes >> (j * 2)) & 0xF;
			buf_ring_enqueue(*fifo, (void *)(uintptr_t)rarray[idx]);
			rarray[idx] = i;
			j++;
		}
		for (i = 0; i < RANDOM_SIZE; i++)
			buf_ring_enqueue(*fifo, (void *) (uintptr_t)rarray[i]);
	} else
		for (i = skip_low; i < nr - skip_high; i++)
			buf_ring_enqueue(*fifo, (void *) (uintptr_t)i);
#if 0
	for (i = 0; i < skip_low + skip_high; i++)
		buf_ring_dequeue(*fifo);
#endif	
	return 0;
}

static int cxio_init_resource_fifo(struct buf_ring **fifo, struct mtx * fifo_lock,
				   u32 nr, u32 skip_low, u32 skip_high)
{
	return (__cxio_init_resource_fifo(fifo, fifo_lock, nr, skip_low,
					  skip_high, 0));
}

static int cxio_init_resource_fifo_random(struct buf_ring **fifo,
				  struct mtx * fifo_lock,
				   u32 nr, u32 skip_low, u32 skip_high)
{

	return (__cxio_init_resource_fifo(fifo, fifo_lock, nr, skip_low,
					  skip_high, 1));
}

static int cxio_init_qpid_fifo(struct cxio_rdev *rdev_p)
{
	u32 i;

	mtx_init(&rdev_p->rscp->qpid_fifo_lock, "qpid fifo", NULL, MTX_DEF);

	rdev_p->rscp->qpid_fifo = buf_ring_alloc(T3_MAX_NUM_QP, M_NOWAIT);
	if (rdev_p->rscp->qpid_fifo == NULL)
		return (-ENOMEM);

	for (i = 16; i < T3_MAX_NUM_QP; i++)
		if (!(i & rdev_p->qpmask))
			buf_ring_enqueue(rdev_p->rscp->qpid_fifo, (void *) (uintptr_t)i);
	return 0;
}

#ifdef needed
int cxio_hal_init_rhdl_resource(u32 nr_rhdl)
{
	return cxio_init_resource_fifo(&rhdl_fifo, &rhdl_fifo_lock, nr_rhdl, 1,
				       0);
}

void cxio_hal_destroy_rhdl_resource(void)
{
	buf_ring_free(rhdl_fifo);
}
#endif

/* nr_* must be power of 2 */
int cxio_hal_init_resource(struct cxio_rdev *rdev_p,
			   u32 nr_tpt, u32 nr_pbl,
			   u32 nr_rqt, u32 nr_qpid, u32 nr_cqid, u32 nr_pdid)
{
	int err = 0;
	struct cxio_hal_resource *rscp;

	rscp = malloc(sizeof(*rscp), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!rscp)
		return (-ENOMEM);
	rdev_p->rscp = rscp;
	err = cxio_init_resource_fifo_random(&rscp->tpt_fifo,
				      &rscp->tpt_fifo_lock,
				      nr_tpt, 1, 0);
	if (err)
		goto tpt_err;
	err = cxio_init_qpid_fifo(rdev_p);
	if (err)
		goto qpid_err;
	err = cxio_init_resource_fifo(&rscp->cqid_fifo, &rscp->cqid_fifo_lock,
				      nr_cqid, 1, 0);
	if (err)
		goto cqid_err;
	err = cxio_init_resource_fifo(&rscp->pdid_fifo, &rscp->pdid_fifo_lock,
				      nr_pdid, 1, 0);
	if (err)
		goto pdid_err;
	return 0;
pdid_err:
	buf_ring_free(rscp->cqid_fifo);
cqid_err:
	buf_ring_free(rscp->qpid_fifo);
qpid_err:
	buf_ring_free(rscp->tpt_fifo);
tpt_err:
	return (-ENOMEM);
}

/*
 * returns 0 if no resource available
 */
static u32 cxio_hal_get_resource(struct buf_ring *fifo, struct mtx *lock)
{
	u32 entry;
	
	mtx_lock(lock);
	entry = (u32)(uintptr_t)buf_ring_dequeue(fifo);
	mtx_unlock(lock);
	return entry;
}

static void cxio_hal_put_resource(struct buf_ring *fifo, u32 entry, struct mtx *lock)
{
	mtx_lock(lock);
	buf_ring_enqueue(fifo, (void *) (uintptr_t)entry);
	mtx_unlock(lock);
}

u32 cxio_hal_get_stag(struct cxio_hal_resource *rscp)
{
	return cxio_hal_get_resource(rscp->tpt_fifo, &rscp->tpt_fifo_lock);
}

void cxio_hal_put_stag(struct cxio_hal_resource *rscp, u32 stag)
{
	cxio_hal_put_resource(rscp->tpt_fifo, stag, &rscp->tpt_fifo_lock);
}

u32 cxio_hal_get_qpid(struct cxio_hal_resource *rscp)
{
	u32 qpid = cxio_hal_get_resource(rscp->qpid_fifo, &rscp->qpid_fifo_lock);
	CTR2(KTR_IW_CXGB, "%s qpid 0x%x", __FUNCTION__, qpid);
	return qpid;
}

void cxio_hal_put_qpid(struct cxio_hal_resource *rscp, u32 qpid)
{
	CTR2(KTR_IW_CXGB, "%s qpid 0x%x", __FUNCTION__, qpid);
	cxio_hal_put_resource(rscp->qpid_fifo, qpid, &rscp->qpid_fifo_lock);
}

u32 cxio_hal_get_cqid(struct cxio_hal_resource *rscp)
{
	return cxio_hal_get_resource(rscp->cqid_fifo, &rscp->cqid_fifo_lock);
}

void cxio_hal_put_cqid(struct cxio_hal_resource *rscp, u32 cqid)
{
	cxio_hal_put_resource(rscp->cqid_fifo, cqid, &rscp->cqid_fifo_lock);
}

u32 cxio_hal_get_pdid(struct cxio_hal_resource *rscp)
{
	return cxio_hal_get_resource(rscp->pdid_fifo, &rscp->pdid_fifo_lock);
}

void cxio_hal_put_pdid(struct cxio_hal_resource *rscp, u32 pdid)
{
	cxio_hal_put_resource(rscp->pdid_fifo, pdid, &rscp->pdid_fifo_lock);
}

void cxio_hal_destroy_resource(struct cxio_hal_resource *rscp)
{
	buf_ring_free(rscp->tpt_fifo);
	buf_ring_free(rscp->cqid_fifo);
	buf_ring_free(rscp->qpid_fifo);
	buf_ring_free(rscp->pdid_fifo);
	free(rscp, M_DEVBUF);
}

/*
 * PBL Memory Manager.  Uses Linux generic allocator.
 */

#define MIN_PBL_SHIFT 8			/* 256B == min PBL size (32 entries) */
#define PBL_CHUNK 2*1024*1024

u32 cxio_hal_pblpool_alloc(struct cxio_rdev *rdev_p, int size)
{
	unsigned long addr = gen_pool_alloc(rdev_p->pbl_pool, size);
	CTR3(KTR_IW_CXGB, "%s addr 0x%x size %d", __FUNCTION__, (u32)addr, size);
	return (u32)addr;
}

void cxio_hal_pblpool_free(struct cxio_rdev *rdev_p, u32 addr, int size)
{
	CTR3(KTR_IW_CXGB, "%s addr 0x%x size %d", __FUNCTION__, addr, size);
	gen_pool_free(rdev_p->pbl_pool, (unsigned long)addr, size);
}

int cxio_hal_pblpool_create(struct cxio_rdev *rdev_p)
{

	rdev_p->pbl_pool = gen_pool_create(rdev_p->rnic_info.pbl_base, MIN_PBL_SHIFT,
	    rdev_p->rnic_info.pbl_top - rdev_p->rnic_info.pbl_base);
#if 0	
	if (rdev_p->pbl_pool) {
		
		unsigned long i;
		for (i = rdev_p->rnic_info.pbl_base;
		     i <= rdev_p->rnic_info.pbl_top - PBL_CHUNK + 1;
		     i += PBL_CHUNK)
			gen_pool_add(rdev_p->pbl_pool, i, PBL_CHUNK, -1);
	}
#endif	
	return rdev_p->pbl_pool ? 0 : (-ENOMEM);
}

void cxio_hal_pblpool_destroy(struct cxio_rdev *rdev_p)
{
	gen_pool_destroy(rdev_p->pbl_pool);
}

/*
 * RQT Memory Manager.  Uses Linux generic allocator.
 */

#define MIN_RQT_SHIFT 10	/* 1KB == mini RQT size (16 entries) */
#define RQT_CHUNK 2*1024*1024

u32 cxio_hal_rqtpool_alloc(struct cxio_rdev *rdev_p, int size)
{
	unsigned long addr = gen_pool_alloc(rdev_p->rqt_pool, size << 6);
	CTR3(KTR_IW_CXGB, "%s addr 0x%x size %d", __FUNCTION__, (u32)addr, size << 6);
	return (u32)addr;
}

void cxio_hal_rqtpool_free(struct cxio_rdev *rdev_p, u32 addr, int size)
{
	CTR3(KTR_IW_CXGB, "%s addr 0x%x size %d", __FUNCTION__, addr, size << 6);
	gen_pool_free(rdev_p->rqt_pool, (unsigned long)addr, size << 6);
}

int cxio_hal_rqtpool_create(struct cxio_rdev *rdev_p)
{
	
	rdev_p->rqt_pool = gen_pool_create(rdev_p->rnic_info.rqt_base,
	    MIN_RQT_SHIFT, rdev_p->rnic_info.rqt_top - rdev_p->rnic_info.rqt_base);
#if 0
	if (rdev_p->rqt_pool) {
		unsigned long i;

		for (i = rdev_p->rnic_info.rqt_base;
		     i <= rdev_p->rnic_info.rqt_top - RQT_CHUNK + 1;
		     i += RQT_CHUNK)
			gen_pool_add(rdev_p->rqt_pool, i, RQT_CHUNK, -1);
	}
#endif	
	return rdev_p->rqt_pool ? 0 : (-ENOMEM);
}

void cxio_hal_rqtpool_destroy(struct cxio_rdev *rdev_p)
{
	gen_pool_destroy(rdev_p->rqt_pool);
}
