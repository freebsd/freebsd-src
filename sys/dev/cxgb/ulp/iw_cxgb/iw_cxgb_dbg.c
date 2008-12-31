
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
__FBSDID("$FreeBSD: src/sys/dev/cxgb/ulp/iw_cxgb/iw_cxgb_dbg.c,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

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

#ifdef DEBUG
#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>
#include <ulp/iw_cxgb/iw_cxgb_resource.h>
#include <ulp/iw_cxgb/iw_cxgb_user.h>
#else
#include <dev/cxgb/cxgb_include.h>
#include <dev/cxgb/ulp/iw_cxgb/iw_cxgb_wr.h>
#include <dev/cxgb/ulp/iw_cxgb/iw_cxgb_hal.h>
#include <dev/cxgb/ulp/iw_cxgb/iw_cxgb_provider.h>
#include <dev/cxgb/ulp/iw_cxgb/iw_cxgb_cm.h>
#include <dev/cxgb/ulp/iw_cxgb/iw_cxgb.h>
#include <dev/cxgb/ulp/iw_cxgb/iw_cxgb_resource.h>
#include <dev/cxgb/ulp/iw_cxgb/iw_cxgb_user.h>
#endif

void cxio_dump_tpt(struct cxio_rdev *rdev, uint32_t stag)
{
	struct ch_mem_range *m;
	u64 *data;
	int rc;
	int size = 32;

	m = kmalloc(sizeof(*m) + size, M_NOWAIT);
	if (!m) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m->mem_id = MEM_PMRX;
	m->addr = (stag>>8) * 32 + rdev->rnic_info.tpt_base;
	m->len = size;
	CTR3(KTR_IW_CXGB, "%s TPT addr 0x%x len %d", __FUNCTION__, m->addr, m->len);
	rc = rdev->t3cdev_p->ctl(rdev->t3cdev_p, RDMA_GET_MEM, m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m, M_DEVBUF);
		return;
	}

	data = (u64 *)m->buf;
	while (size > 0) {
		CTR2(KTR_IW_CXGB, "TPT %08x: %016llx", m->addr, (unsigned long long) *data);
		size -= 8;
		data++;
		m->addr += 8;
	}
	free(m, M_DEVBUF);
}

void cxio_dump_pbl(struct cxio_rdev *rdev, uint32_t pbl_addr, uint32_t len, u8 shift)
{
	struct ch_mem_range *m;
	u64 *data;
	int rc;
	int size, npages;

	shift += 12;
	npages = (len + (1ULL << shift) - 1) >> shift;
	size = npages * sizeof(u64);

	m = kmalloc(sizeof(*m) + size, M_NOWAIT);
	if (!m) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m->mem_id = MEM_PMRX;
	m->addr = pbl_addr;
	m->len = size;
	CTR4(KTR_IW_CXGB, "%s PBL addr 0x%x len %d depth %d",
		__FUNCTION__, m->addr, m->len, npages);
	rc = rdev->t3cdev_p->ctl(rdev->t3cdev_p, RDMA_GET_MEM, m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m, M_DEVBUF);
		return;
	}

	data = (u64 *)m->buf;
	while (size > 0) {
		CTR2(KTR_IW_CXGB, "PBL %08x: %016llx", m->addr, (unsigned long long) *data);
		size -= 8;
		data++;
		m->addr += 8;
	}
	free(m, M_DEVBUF);
}

void cxio_dump_wqe(union t3_wr *wqe)
{
	uint64_t *data = (uint64_t *)wqe;
	uint32_t size = (uint32_t)(be64toh(*data) & 0xff);

	if (size == 0)
		size = 8;
	while (size > 0) {
		CTR2(KTR_IW_CXGB, "WQE %p: %016llx", data,
		     (unsigned long long) be64toh(*data));
		size--;
		data++;
	}
}

void cxio_dump_wce(struct t3_cqe *wce)
{
	uint64_t *data = (uint64_t *)wce;
	int size = sizeof(*wce);

	while (size > 0) {
		CTR2(KTR_IW_CXGB, "WCE %p: %016llx", data,
		     (unsigned long long) be64toh(*data));
		size -= 8;
		data++;
	}
}

void cxio_dump_rqt(struct cxio_rdev *rdev, uint32_t hwtid, int nents)
{
	struct ch_mem_range *m;
	int size = nents * 64;
	u64 *data;
	int rc;

	m = kmalloc(sizeof(*m) + size, M_NOWAIT);
	if (!m) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m->mem_id = MEM_PMRX;
	m->addr = ((hwtid)<<10) + rdev->rnic_info.rqt_base;
	m->len = size;
	CTR3(KTR_IW_CXGB, "%s RQT addr 0x%x len %d", __FUNCTION__, m->addr, m->len);
	rc = rdev->t3cdev_p->ctl(rdev->t3cdev_p, RDMA_GET_MEM, m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m, M_DEVBUF);
		return;
	}

	data = (u64 *)m->buf;
	while (size > 0) {
		CTR2(KTR_IW_CXGB, "RQT %08x: %016llx", m->addr, (unsigned long long) *data);
		size -= 8;
		data++;
		m->addr += 8;
	}
	free(m, M_DEVBUF);
}

void cxio_dump_tcb(struct cxio_rdev *rdev, uint32_t hwtid)
{
	struct ch_mem_range *m;
	int size = TCB_SIZE;
	uint32_t *data;
	int rc;

	m = kmalloc(sizeof(*m) + size, M_NOWAIT);
	if (!m) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m->mem_id = MEM_CM;
	m->addr = hwtid * size;
	m->len = size;
	CTR3(KTR_IW_CXGB, "%s TCB %d len %d", __FUNCTION__, m->addr, m->len);
	rc = rdev->t3cdev_p->ctl(rdev->t3cdev_p, RDMA_GET_MEM, m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m, M_DEVBUF);
		return;
	}

	data = (uint32_t *)m->buf;
	while (size > 0) {
		printf("%2u: %08x %08x %08x %08x %08x %08x %08x %08x\n",
			m->addr,
			*(data+2), *(data+3), *(data),*(data+1),
			*(data+6), *(data+7), *(data+4), *(data+5));
		size -= 32;
		data += 8;
		m->addr += 32;
	}
	free(m, M_DEVBUF);
}
#endif
