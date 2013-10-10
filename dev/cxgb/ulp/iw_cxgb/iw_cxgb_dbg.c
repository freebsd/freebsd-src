
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

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
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

#include <rdma/ib_verbs.h>
#include <rdma/ib_umem.h>
#include <rdma/ib_user_verbs.h>
#include <linux/idr.h>
#include <ulp/iw_cxgb/iw_cxgb_ib_intfc.h>

#if defined(INVARIANTS) && defined(TCP_OFFLOAD)
#include <cxgb_include.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>
#include <ulp/iw_cxgb/iw_cxgb_resource.h>
#include <ulp/iw_cxgb/iw_cxgb_user.h>

static int
cxio_rdma_get_mem(struct cxio_rdev *rdev, struct ch_mem_range *m)
{
	struct adapter *sc = rdev->adap;
	struct mc7 *mem;

	if ((m->addr & 7) || (m->len & 7))
		return (EINVAL);
	if (m->mem_id == MEM_CM)
		mem = &sc->cm;
	else if (m->mem_id == MEM_PMRX)
		mem = &sc->pmrx;
	else if (m->mem_id == MEM_PMTX)
		mem = &sc->pmtx;
	else
		return (EINVAL);

	return (t3_mc7_bd_read(mem, m->addr/8, m->len/8, (u64 *)m->buf));
}

void cxio_dump_tpt(struct cxio_rdev *rdev, uint32_t stag)
{
	struct ch_mem_range m;
	u64 *data;
	u32 addr;
	int rc;
	int size = 32;

	m.buf = malloc(size, M_DEVBUF, M_NOWAIT);
	if (m.buf == NULL) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m.mem_id = MEM_PMRX;
	m.addr = (stag >> 8) * 32 + rdev->rnic_info.tpt_base;
	m.len = size;
	CTR3(KTR_IW_CXGB, "%s TPT addr 0x%x len %d", __FUNCTION__, m.addr, m.len);

	rc = cxio_rdma_get_mem(rdev, &m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m.buf, M_DEVBUF);
		return;
	}

	data = (u64 *)m.buf;
	addr = m.addr;
	while (size > 0) {
		CTR2(KTR_IW_CXGB, "TPT %08x: %016llx", addr, (unsigned long long) *data);
		size -= 8;
		data++;
		addr += 8;
	}
	free(m.buf, M_DEVBUF);
}

void cxio_dump_pbl(struct cxio_rdev *rdev, uint32_t pbl_addr, uint32_t len, u8 shift)
{
	struct ch_mem_range m;
	u64 *data;
	u32 addr;
	int rc;
	int size, npages;

	shift += 12;
	npages = (len + (1ULL << shift) - 1) >> shift;
	size = npages * sizeof(u64);
	m.buf = malloc(size, M_DEVBUF, M_NOWAIT);
	if (m.buf == NULL) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m.mem_id = MEM_PMRX;
	m.addr = pbl_addr;
	m.len = size;
	CTR4(KTR_IW_CXGB, "%s PBL addr 0x%x len %d depth %d",
		__FUNCTION__, m.addr, m.len, npages);

	rc = cxio_rdma_get_mem(rdev, &m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m.buf, M_DEVBUF);
		return;
	}

	data = (u64 *)m.buf;
	addr = m.addr;
	while (size > 0) {
		CTR2(KTR_IW_CXGB, "PBL %08x: %016llx", addr, (unsigned long long) *data);
		size -= 8;
		data++;
		addr += 8;
	}
	free(m.buf, M_DEVBUF);
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
	struct ch_mem_range m;
	int size = nents * 64;
	u64 *data;
	u32 addr;
	int rc;

	m.buf = malloc(size, M_DEVBUF, M_NOWAIT);
	if (m.buf == NULL) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m.mem_id = MEM_PMRX;
	m.addr = ((hwtid)<<10) + rdev->rnic_info.rqt_base;
	m.len = size;
	CTR3(KTR_IW_CXGB, "%s RQT addr 0x%x len %d", __FUNCTION__, m.addr, m.len);

	rc = cxio_rdma_get_mem(rdev, &m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m.buf, M_DEVBUF);
		return;
	}

	data = (u64 *)m.buf;
	addr = m.addr;
	while (size > 0) {
		CTR2(KTR_IW_CXGB, "RQT %08x: %016llx", addr, (unsigned long long) *data);
		size -= 8;
		data++;
		addr += 8;
	}
	free(m.buf, M_DEVBUF);
}

void cxio_dump_tcb(struct cxio_rdev *rdev, uint32_t hwtid)
{
	struct ch_mem_range m;
	int size = TCB_SIZE;
	uint32_t *data;
	uint32_t addr;
	int rc;

	m.buf = malloc(size, M_DEVBUF, M_NOWAIT);
	if (m.buf == NULL) {
		CTR1(KTR_IW_CXGB, "%s couldn't allocate memory.", __FUNCTION__);
		return;
	}
	m.mem_id = MEM_CM;
	m.addr = hwtid * size;
	m.len = size;
	CTR3(KTR_IW_CXGB, "%s TCB %d len %d", __FUNCTION__, m.addr, m.len);

	rc = cxio_rdma_get_mem(rdev, &m);
	if (rc) {
		CTR2(KTR_IW_CXGB, "%s toectl returned error %d", __FUNCTION__, rc);
		free(m.buf, M_DEVBUF);
		return;
	}

	data = (uint32_t *)m.buf;
	addr = m.addr;
	while (size > 0) {
		printf("%2u: %08x %08x %08x %08x %08x %08x %08x %08x\n",
			addr,
			*(data+2), *(data+3), *(data),*(data+1),
			*(data+6), *(data+7), *(data+4), *(data+5));
		size -= 32;
		data += 8;
		addr += 32;
	}
	free(m.buf, M_DEVBUF);
}
#endif
