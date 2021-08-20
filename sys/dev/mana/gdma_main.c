/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/eventhandler.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/in_cksum.h>

#include <net/if.h>
#include <net/if_var.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "gdma_util.h"
#include "mana.h"


static mana_vendor_id_t mana_id_table[] = {
    { PCI_VENDOR_ID_MICROSOFT, PCI_DEV_ID_MANA_VF},
    /* Last entry */
    { 0, 0}
};

static inline uint32_t
mana_gd_r32(struct gdma_context *g, uint64_t offset)
{
	uint32_t v = bus_space_read_4(g->gd_bus.bar0_t,
	    g->gd_bus.bar0_h, offset);
	rmb();
	return (v);
}

#if defined(__amd64__)
static inline uint64_t
mana_gd_r64(struct gdma_context *g, uint64_t offset)
{
	uint64_t v = bus_space_read_8(g->gd_bus.bar0_t,
	    g->gd_bus.bar0_h, offset);
	rmb();
	return (v);
}
#else
static inline uint64_t
mana_gd_r64(struct gdma_context *g, uint64_t offset)
{
	uint64_t v;
	uint32_t *vp = (uint32_t *)&v;

	*vp =  mana_gd_r32(g, offset);
	*(vp + 1) = mana_gd_r32(g, offset + 4);
	rmb();
	return (v);
}
#endif

static int
mana_gd_query_max_resources(device_t dev)
{
	struct gdma_context *gc = device_get_softc(dev);
	struct gdma_query_max_resources_resp resp = {};
	struct gdma_general_req req = {};
	int err;

	mana_gd_init_req_hdr(&req.hdr, GDMA_QUERY_MAX_RESOURCES,
	    sizeof(req), sizeof(resp));

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp), &resp);
	if (err || resp.hdr.status) {
		device_printf(gc->dev,
		   "Failed to query resource info: %d, 0x%x\n",
		   err, resp.hdr.status);
		return err ? err : EPROTO;
	}

	mana_dbg(NULL, "max_msix %u, max_eq %u, max_cq %u, "
	    "max_sq %u, max_rq %u\n",
	    resp.max_msix, resp.max_eq, resp.max_cq,
	    resp.max_sq, resp.max_rq);

	if (gc->num_msix_usable > resp.max_msix)
		gc->num_msix_usable = resp.max_msix;

	if (gc->num_msix_usable <= 1)
		return ENOSPC;

	gc->max_num_queues = mp_ncpus;
	if (gc->max_num_queues > MANA_MAX_NUM_QUEUES)
		gc->max_num_queues = MANA_MAX_NUM_QUEUES;

	if (gc->max_num_queues > resp.max_eq)
		gc->max_num_queues = resp.max_eq;

	if (gc->max_num_queues > resp.max_cq)
		gc->max_num_queues = resp.max_cq;

	if (gc->max_num_queues > resp.max_sq)
		gc->max_num_queues = resp.max_sq;

	if (gc->max_num_queues > resp.max_rq)
		gc->max_num_queues = resp.max_rq;

	return 0;
}

static int
mana_gd_detect_devices(device_t dev)
{
	struct gdma_context *gc = device_get_softc(dev);
	struct gdma_list_devices_resp resp = {};
	struct gdma_general_req req = {};
	struct gdma_dev_id gd_dev;
	uint32_t i, max_num_devs;
	uint16_t dev_type;
	int err;

	mana_gd_init_req_hdr(&req.hdr, GDMA_LIST_DEVICES, sizeof(req),
	    sizeof(resp));

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp), &resp);
	if (err || resp.hdr.status) {
		device_printf(gc->dev,
		    "Failed to detect devices: %d, 0x%x\n", err,
		    resp.hdr.status);
		return err ? err : EPROTO;
	}

	max_num_devs = min_t(uint32_t, MAX_NUM_GDMA_DEVICES, resp.num_of_devs);

	for (i = 0; i < max_num_devs; i++) {
		gd_dev = resp.devs[i];
		dev_type = gd_dev.type;

		mana_dbg(NULL, "gdma dev %d, type %u\n",
		    i, dev_type);

		/* HWC is already detected in mana_hwc_create_channel(). */
		if (dev_type == GDMA_DEVICE_HWC)
			continue;

		if (dev_type == GDMA_DEVICE_MANA) {
			gc->mana.gdma_context = gc;
			gc->mana.dev_id = gd_dev;
		}
	}

	return gc->mana.dev_id.type == 0 ? ENODEV : 0;
}

int
mana_gd_send_request(struct gdma_context *gc, uint32_t req_len,
    const void *req, uint32_t resp_len, void *resp)
{
	struct hw_channel_context *hwc = gc->hwc.driver_data;

	return mana_hwc_send_request(hwc, req_len, req, resp_len, resp);
}

void
mana_gd_dma_map_paddr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *paddr = arg;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many segments %d!", nseg));
	*paddr = segs->ds_addr;
}

int
mana_gd_alloc_memory(struct gdma_context *gc, unsigned int length,
    struct gdma_mem_info *gmi)
{
	bus_addr_t dma_handle;
	void *buf;
	int err;

	if (!gc || !gmi)
		return EINVAL;

	if (length < PAGE_SIZE || (length != roundup_pow_of_two(length)))
		return EINVAL;

	err = bus_dma_tag_create(bus_get_dma_tag(gc->dev),	/* parent */
	    PAGE_SIZE, 0,		/* alignment, boundary	*/
	    BUS_SPACE_MAXADDR,		/* lowaddr		*/
	    BUS_SPACE_MAXADDR,		/* highaddr		*/
	    NULL, NULL,			/* filter, filterarg	*/
	    length,			/* maxsize		*/
	    1,				/* nsegments		*/
	    length,			/* maxsegsize		*/
	    0,				/* flags		*/
	    NULL, NULL,			/* lockfunc, lockfuncarg*/
	    &gmi->dma_tag);
	if (err) {
		device_printf(gc->dev,
		    "failed to create dma tag, err: %d\n", err);
		return (err);
	}

	/*
	 * Must have BUS_DMA_ZERO flag to clear the dma memory.
	 * Otherwise the queue overflow detection mechanism does
	 * not work.
	 */
	err = bus_dmamem_alloc(gmi->dma_tag, &buf,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT | BUS_DMA_ZERO, &gmi->dma_map);
	if (err) {
		device_printf(gc->dev,
		    "failed to alloc dma mem, err: %d\n", err);
		bus_dma_tag_destroy(gmi->dma_tag);
		return (err);
	}

	err = bus_dmamap_load(gmi->dma_tag, gmi->dma_map, buf,
	    length, mana_gd_dma_map_paddr, &dma_handle, BUS_DMA_NOWAIT);
	if (err) {
		device_printf(gc->dev,
		    "failed to load dma mem, err: %d\n", err);
		bus_dmamem_free(gmi->dma_tag, buf, gmi->dma_map);
		bus_dma_tag_destroy(gmi->dma_tag);
		return (err);
	}

	gmi->dev = gc->dev;
	gmi->dma_handle = dma_handle;
	gmi->virt_addr = buf;
	gmi->length = length;

	return 0;
}

void
mana_gd_free_memory(struct gdma_mem_info *gmi)
{
	bus_dmamap_unload(gmi->dma_tag, gmi->dma_map);
	bus_dmamem_free(gmi->dma_tag, gmi->virt_addr, gmi->dma_map);
	bus_dma_tag_destroy(gmi->dma_tag);
}

static int
mana_gd_create_hw_eq(struct gdma_context *gc,
    struct gdma_queue *queue)
{
	struct gdma_create_queue_resp resp = {};
	struct gdma_create_queue_req req = {};
	int err;

	if (queue->type != GDMA_EQ)
		return EINVAL;

	mana_gd_init_req_hdr(&req.hdr, GDMA_CREATE_QUEUE,
			     sizeof(req), sizeof(resp));

	req.hdr.dev_id = queue->gdma_dev->dev_id;
	req.type = queue->type;
	req.pdid = queue->gdma_dev->pdid;
	req.doolbell_id = queue->gdma_dev->doorbell;
	req.gdma_region = queue->mem_info.gdma_region;
	req.queue_size = queue->queue_size;
	req.log2_throttle_limit = queue->eq.log2_throttle_limit;
	req.eq_pci_msix_index = queue->eq.msix_index;

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp), &resp);
	if (err || resp.hdr.status) {
		device_printf(gc->dev,
		    "Failed to create queue: %d, 0x%x\n",
		    err, resp.hdr.status);
		return err ? err : EPROTO;
	}

	queue->id = resp.queue_index;
	queue->eq.disable_needed = true;
	queue->mem_info.gdma_region = GDMA_INVALID_DMA_REGION;
	return 0;
}

static
int mana_gd_disable_queue(struct gdma_queue *queue)
{
	struct gdma_context *gc = queue->gdma_dev->gdma_context;
	struct gdma_disable_queue_req req = {};
	struct gdma_general_resp resp = {};
	int err;

	if (queue->type != GDMA_EQ)
		mana_warn(NULL, "Not event queue type 0x%x\n",
		    queue->type);

	mana_gd_init_req_hdr(&req.hdr, GDMA_DISABLE_QUEUE,
	    sizeof(req), sizeof(resp));

	req.hdr.dev_id = queue->gdma_dev->dev_id;
	req.type = queue->type;
	req.queue_index =  queue->id;
	req.alloc_res_id_on_creation = 1;

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp), &resp);
	if (err || resp.hdr.status) {
		device_printf(gc->dev,
		    "Failed to disable queue: %d, 0x%x\n", err,
		    resp.hdr.status);
		return err ? err : EPROTO;
	}

	return 0;
}

#define DOORBELL_OFFSET_SQ	0x0
#define DOORBELL_OFFSET_RQ	0x400
#define DOORBELL_OFFSET_CQ	0x800
#define DOORBELL_OFFSET_EQ	0xFF8

static void
mana_gd_ring_doorbell(struct gdma_context *gc, uint32_t db_index,
    enum gdma_queue_type q_type, uint32_t qid,
    uint32_t tail_ptr, uint8_t num_req)
{
	union gdma_doorbell_entry e = {};
	void __iomem *addr;

	addr = (char *)gc->db_page_base + gc->db_page_size * db_index;
	switch (q_type) {
	case GDMA_EQ:
		e.eq.id = qid;
		e.eq.tail_ptr = tail_ptr;
		e.eq.arm = num_req;

		addr = (char *)addr + DOORBELL_OFFSET_EQ;
		break;

	case GDMA_CQ:
		e.cq.id = qid;
		e.cq.tail_ptr = tail_ptr;
		e.cq.arm = num_req;

		addr = (char *)addr + DOORBELL_OFFSET_CQ;
		break;

	case GDMA_RQ:
		e.rq.id = qid;
		e.rq.tail_ptr = tail_ptr;
		e.rq.wqe_cnt = num_req;

		addr = (char *)addr + DOORBELL_OFFSET_RQ;
		break;

	case GDMA_SQ:
		e.sq.id = qid;
		e.sq.tail_ptr = tail_ptr;

		addr = (char *)addr + DOORBELL_OFFSET_SQ;
		break;

	default:
		mana_warn(NULL, "Invalid queue type 0x%x\n", q_type);
		return;
	}

	/* Ensure all writes are done before ring doorbell */
	wmb();

#if defined(__amd64__)
	writeq(addr, e.as_uint64);
#else
	uint32_t *p = (uint32_t *)&e.as_uint64;
	writel(addr, *p);
	writel((char *)addr + 4, *(p + 1));
#endif
}

void
mana_gd_wq_ring_doorbell(struct gdma_context *gc, struct gdma_queue *queue)
{
	mana_gd_ring_doorbell(gc, queue->gdma_dev->doorbell, queue->type,
	    queue->id, queue->head * GDMA_WQE_BU_SIZE, 1);
}

void
mana_gd_arm_cq(struct gdma_queue *cq)
{
	struct gdma_context *gc = cq->gdma_dev->gdma_context;

	uint32_t num_cqe = cq->queue_size / GDMA_CQE_SIZE;

	uint32_t head = cq->head % (num_cqe << GDMA_CQE_OWNER_BITS);

	mana_gd_ring_doorbell(gc, cq->gdma_dev->doorbell, cq->type, cq->id,
	    head, SET_ARM_BIT);
}

static void
mana_gd_process_eqe(struct gdma_queue *eq)
{
	uint32_t head = eq->head % (eq->queue_size / GDMA_EQE_SIZE);
	struct gdma_context *gc = eq->gdma_dev->gdma_context;
	struct gdma_eqe *eq_eqe_ptr = eq->queue_mem_ptr;
	union gdma_eqe_info eqe_info;
	enum gdma_eqe_type type;
	struct gdma_event event;
	struct gdma_queue *cq;
	struct gdma_eqe *eqe;
	uint32_t cq_id;

	eqe = &eq_eqe_ptr[head];
	eqe_info.as_uint32 = eqe->eqe_info;
	type = eqe_info.type;

	switch (type) {
	case GDMA_EQE_COMPLETION:
		cq_id = eqe->details[0] & 0xFFFFFF;
		if (cq_id >= gc->max_num_cqs) {
			mana_warn(NULL,
			    "failed: cq_id %u > max_num_cqs %u\n",
			    cq_id, gc->max_num_cqs);
			break;
		}

		cq = gc->cq_table[cq_id];
		if (!cq || cq->type != GDMA_CQ || cq->id != cq_id) {
			mana_warn(NULL,
			    "failed: invalid cq_id %u\n", cq_id);
			break;
		}

		if (cq->cq.callback)
			cq->cq.callback(cq->cq.context, cq);

		break;

	case GDMA_EQE_TEST_EVENT:
		gc->test_event_eq_id = eq->id;

		mana_dbg(NULL,
		    "EQE TEST EVENT received for EQ %u\n", eq->id);

		complete(&gc->eq_test_event);
		break;

	case GDMA_EQE_HWC_INIT_EQ_ID_DB:
	case GDMA_EQE_HWC_INIT_DATA:
	case GDMA_EQE_HWC_INIT_DONE:
		if (!eq->eq.callback)
			break;

		event.type = type;
		memcpy(&event.details, &eqe->details, GDMA_EVENT_DATA_SIZE);
		eq->eq.callback(eq->eq.context, eq, &event);
		break;

	default:
		break;
	}
}

static void
mana_gd_process_eq_events(void *arg)
{
	uint32_t owner_bits, new_bits, old_bits;
	union gdma_eqe_info eqe_info;
	struct gdma_eqe *eq_eqe_ptr;
	struct gdma_queue *eq = arg;
	struct gdma_context *gc;
	uint32_t head, num_eqe;
	struct gdma_eqe *eqe;
	unsigned int arm_bit;
	int i, j;

	gc = eq->gdma_dev->gdma_context;

	num_eqe = eq->queue_size / GDMA_EQE_SIZE;
	eq_eqe_ptr = eq->queue_mem_ptr;

	bus_dmamap_sync(eq->mem_info.dma_tag, eq->mem_info.dma_map,
	    BUS_DMASYNC_POSTREAD);

	/* Process up to 5 EQEs at a time, and update the HW head. */
	for (i = 0; i < 5; i++) {
		eqe = &eq_eqe_ptr[eq->head % num_eqe];
		eqe_info.as_uint32 = eqe->eqe_info;
		owner_bits = eqe_info.owner_bits;

		old_bits = (eq->head / num_eqe - 1) & GDMA_EQE_OWNER_MASK;

		/* No more entries */
		if (owner_bits == old_bits)
			break;

		new_bits = (eq->head / num_eqe) & GDMA_EQE_OWNER_MASK;
		if (owner_bits != new_bits) {
			/* Something wrong. Log for debugging purpose */
			device_printf(gc->dev,
			    "EQ %d: overflow detected, "
			    "i = %d, eq->head = %u "
			    "got owner_bits = %u, new_bits = %u "
			    "eqe addr %p, eqe->eqe_info 0x%x, "
			    "eqe type = %x, reserved1 = %x, client_id = %x, "
			    "reserved2 = %x, owner_bits = %x\n",
			    eq->id, i, eq->head,
			    owner_bits, new_bits,
			    eqe, eqe->eqe_info,
			    eqe_info.type, eqe_info.reserved1,
			    eqe_info.client_id, eqe_info.reserved2,
			    eqe_info.owner_bits);

			uint32_t *eqe_dump = (uint32_t *) eq_eqe_ptr;
			for (j = 0; j < 20; j++) {
				device_printf(gc->dev, "%p: %x\t%x\t%x\t%x\n",
				    &eqe_dump[j * 4], eqe_dump[j * 4], eqe_dump[j * 4 + 1],
				    eqe_dump[j * 4 + 2], eqe_dump[j * 4 + 3]);
			}
			break;
		}

		mana_gd_process_eqe(eq);

		eq->head++;
	}

	bus_dmamap_sync(eq->mem_info.dma_tag, eq->mem_info.dma_map,
	    BUS_DMASYNC_PREREAD);

	/* Always rearm the EQ for HWC. */
	if (mana_gd_is_hwc(eq->gdma_dev)) {
		arm_bit = SET_ARM_BIT;
	} else if (eq->eq.work_done < eq->eq.budget &&
	    eq->eq.do_not_ring_db == false) {
		arm_bit = SET_ARM_BIT;
	} else {
		arm_bit = 0;
	}

	head = eq->head % (num_eqe << GDMA_EQE_OWNER_BITS);

	mana_gd_ring_doorbell(gc, eq->gdma_dev->doorbell, eq->type, eq->id,
	    head, arm_bit);
}

#define MANA_POLL_BUDGET	8
#define MANA_RX_BUDGET		256

static void
mana_poll(void *arg, int pending)
{
	struct gdma_queue *eq = arg;
	int i;

	eq->eq.work_done = 0;
	eq->eq.budget = MANA_RX_BUDGET;

	for (i = 0; i < MANA_POLL_BUDGET; i++) {
		/*
		 * If this is the last loop, set the budget big enough
		 * so it will arm the EQ any way.
		 */
		if (i == (MANA_POLL_BUDGET - 1))
			eq->eq.budget = CQE_POLLING_BUFFER + 1;

		mana_gd_process_eq_events(eq);

		if (eq->eq.work_done < eq->eq.budget)
			break;

		eq->eq.work_done = 0;
	}
}

static void
mana_gd_schedule_task(void *arg)
{
	struct gdma_queue *eq = arg;

	taskqueue_enqueue(eq->eq.cleanup_tq, &eq->eq.cleanup_task);
}

static int
mana_gd_register_irq(struct gdma_queue *queue,
    const struct gdma_queue_spec *spec)
{
	static int mana_last_bind_cpu = -1;
	struct gdma_dev *gd = queue->gdma_dev;
	bool is_mana = mana_gd_is_mana(gd);
	struct gdma_irq_context *gic;
	struct gdma_context *gc;
	struct gdma_resource *r;
	unsigned int msi_index;
	int err;

	gc = gd->gdma_context;
	r = &gc->msix_resource;

	mtx_lock_spin(&r->lock_spin);

	msi_index = find_first_zero_bit(r->map, r->size);
	if (msi_index >= r->size) {
		err = ENOSPC;
	} else {
		bitmap_set(r->map, msi_index, 1);
		queue->eq.msix_index = msi_index;
		err = 0;
	}

	mtx_unlock_spin(&r->lock_spin);

	if (err)
		return err;

	if (unlikely(msi_index >= gc->num_msix_usable)) {
		device_printf(gc->dev,
		    "chose an invalid msix index %d, usable %d\n",
		    msi_index, gc->num_msix_usable);
		return ENOSPC;
	}

	gic = &gc->irq_contexts[msi_index];

	if (is_mana) {
		struct mana_port_context *apc = if_getsoftc(spec->eq.ndev);
		queue->eq.do_not_ring_db = false;

		NET_TASK_INIT(&queue->eq.cleanup_task, 0, mana_poll, queue);
		queue->eq.cleanup_tq =
		    taskqueue_create_fast("mana eq cleanup",
		    M_WAITOK, taskqueue_thread_enqueue,
		    &queue->eq.cleanup_tq);

		if (mana_last_bind_cpu < 0)
			mana_last_bind_cpu = CPU_FIRST();
		queue->eq.cpu = mana_last_bind_cpu;
		mana_last_bind_cpu = CPU_NEXT(mana_last_bind_cpu);

		/* XXX Name is not optimal. However we have to start
		 * the task here. Otherwise, test eq will have no
		 * handler.
		 */
		if (apc->bind_cleanup_thread_cpu) {
			cpuset_t cpu_mask;
			CPU_SETOF(queue->eq.cpu, &cpu_mask);
			taskqueue_start_threads_cpuset(&queue->eq.cleanup_tq,
			    1, PI_NET, &cpu_mask,
			    "mana eq poll msix %u on cpu %d",
			    msi_index, queue->eq.cpu);
		} else {

			taskqueue_start_threads(&queue->eq.cleanup_tq, 1,
			    PI_NET, "mana eq poll on msix %u", msi_index);
		}
	}

	if (unlikely(gic->handler || gic->arg)) {
		device_printf(gc->dev,
		    "interrupt handler or arg already assigned, "
		    "msix index: %d\n", msi_index);
	}

	gic->arg = queue;

	if (is_mana)
		gic->handler = mana_gd_schedule_task;
	else
		gic->handler = mana_gd_process_eq_events;

	mana_dbg(NULL, "registered msix index %d vector %d irq %ju\n",
	    msi_index, gic->msix_e.vector, rman_get_start(gic->res));

	return 0;
}

static void
mana_gd_deregiser_irq(struct gdma_queue *queue)
{
	struct gdma_dev *gd = queue->gdma_dev;
	struct gdma_irq_context *gic;
	struct gdma_context *gc;
	struct gdma_resource *r;
	unsigned int msix_index;

	gc = gd->gdma_context;
	r = &gc->msix_resource;

	/* At most num_online_cpus() + 1 interrupts are used. */
	msix_index = queue->eq.msix_index;
	if (unlikely(msix_index >= gc->num_msix_usable))
		return;

	gic = &gc->irq_contexts[msix_index];
	gic->handler = NULL;
	gic->arg = NULL;

	mtx_lock_spin(&r->lock_spin);
	bitmap_clear(r->map, msix_index, 1);
	mtx_unlock_spin(&r->lock_spin);

	queue->eq.msix_index = INVALID_PCI_MSIX_INDEX;

	mana_dbg(NULL, "deregistered msix index %d vector %d irq %ju\n",
	    msix_index, gic->msix_e.vector, rman_get_start(gic->res));
}

int
mana_gd_test_eq(struct gdma_context *gc, struct gdma_queue *eq)
{
	struct gdma_generate_test_event_req req = {};
	struct gdma_general_resp resp = {};
	device_t dev = gc->dev;
	int err;

	sx_xlock(&gc->eq_test_event_sx);

	init_completion(&gc->eq_test_event);
	gc->test_event_eq_id = INVALID_QUEUE_ID;

	mana_gd_init_req_hdr(&req.hdr, GDMA_GENERATE_TEST_EQE,
			     sizeof(req), sizeof(resp));

	req.hdr.dev_id = eq->gdma_dev->dev_id;
	req.queue_index = eq->id;

	err = mana_gd_send_request(gc, sizeof(req), &req,
	    sizeof(resp), &resp);
	if (err) {
		device_printf(dev, "test_eq failed: %d\n", err);
		goto out;
	}

	err = EPROTO;

	if (resp.hdr.status) {
		device_printf(dev, "test_eq failed: 0x%x\n",
		    resp.hdr.status);
		goto out;
	}

	if (wait_for_completion_timeout(&gc->eq_test_event, 30 * hz)) {
		device_printf(dev, "test_eq timed out on queue %d\n",
		    eq->id);
		goto out;
	}

	if (eq->id != gc->test_event_eq_id) {
		device_printf(dev,
		    "test_eq got an event on wrong queue %d (%d)\n",
		    gc->test_event_eq_id, eq->id);
		goto out;
	}

	err = 0;
out:
	sx_xunlock(&gc->eq_test_event_sx);
	return err;
}

static void
mana_gd_destroy_eq(struct gdma_context *gc, bool flush_evenets,
    struct gdma_queue *queue)
{
	int err;

	if (flush_evenets) {
		err = mana_gd_test_eq(gc, queue);
		if (err)
			device_printf(gc->dev,
			    "Failed to flush EQ: %d\n", err);
	}

	mana_gd_deregiser_irq(queue);

	if (mana_gd_is_mana(queue->gdma_dev)) {
		while (taskqueue_cancel(queue->eq.cleanup_tq,
		    &queue->eq.cleanup_task, NULL))
			taskqueue_drain(queue->eq.cleanup_tq,
			    &queue->eq.cleanup_task);

		taskqueue_free(queue->eq.cleanup_tq);
	}

	if (queue->eq.disable_needed)
		mana_gd_disable_queue(queue);
}

static int mana_gd_create_eq(struct gdma_dev *gd,
    const struct gdma_queue_spec *spec,
    bool create_hwq, struct gdma_queue *queue)
{
	struct gdma_context *gc = gd->gdma_context;
	device_t dev = gc->dev;
	uint32_t log2_num_entries;
	int err;

	queue->eq.msix_index = INVALID_PCI_MSIX_INDEX;

	log2_num_entries = ilog2(queue->queue_size / GDMA_EQE_SIZE);

	if (spec->eq.log2_throttle_limit > log2_num_entries) {
		device_printf(dev,
		    "EQ throttling limit (%lu) > maximum EQE (%u)\n",
		    spec->eq.log2_throttle_limit, log2_num_entries);
		return EINVAL;
	}

	err = mana_gd_register_irq(queue, spec);
	if (err) {
		device_printf(dev, "Failed to register irq: %d\n", err);
		return err;
	}

	queue->eq.callback = spec->eq.callback;
	queue->eq.context = spec->eq.context;
	queue->head |= INITIALIZED_OWNER_BIT(log2_num_entries);
	queue->eq.log2_throttle_limit = spec->eq.log2_throttle_limit ?: 1;

	if (create_hwq) {
		err = mana_gd_create_hw_eq(gc, queue);
		if (err)
			goto out;

		err = mana_gd_test_eq(gc, queue);
		if (err)
			goto out;
	}

	return 0;
out:
	device_printf(dev, "Failed to create EQ: %d\n", err);
	mana_gd_destroy_eq(gc, false, queue);
	return err;
}

static void
mana_gd_create_cq(const struct gdma_queue_spec *spec,
    struct gdma_queue *queue)
{
	uint32_t log2_num_entries = ilog2(spec->queue_size / GDMA_CQE_SIZE);

	queue->head |= INITIALIZED_OWNER_BIT(log2_num_entries);
	queue->cq.parent = spec->cq.parent_eq;
	queue->cq.context = spec->cq.context;
	queue->cq.callback = spec->cq.callback;
}

static void
mana_gd_destroy_cq(struct gdma_context *gc,
    struct gdma_queue *queue)
{
	uint32_t id = queue->id;

	if (id >= gc->max_num_cqs)
		return;

	if (!gc->cq_table[id])
		return;

	gc->cq_table[id] = NULL;
}

int mana_gd_create_hwc_queue(struct gdma_dev *gd,
    const struct gdma_queue_spec *spec,
    struct gdma_queue **queue_ptr)
{
	struct gdma_context *gc = gd->gdma_context;
	struct gdma_mem_info *gmi;
	struct gdma_queue *queue;
	int err;

	queue = malloc(sizeof(*queue), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!queue)
		return ENOMEM;

	gmi = &queue->mem_info;
	err = mana_gd_alloc_memory(gc, spec->queue_size, gmi);
	if (err)
		goto free_q;

	queue->head = 0;
	queue->tail = 0;
	queue->queue_mem_ptr = gmi->virt_addr;
	queue->queue_size = spec->queue_size;
	queue->monitor_avl_buf = spec->monitor_avl_buf;
	queue->type = spec->type;
	queue->gdma_dev = gd;

	if (spec->type == GDMA_EQ)
		err = mana_gd_create_eq(gd, spec, false, queue);
	else if (spec->type == GDMA_CQ)
		mana_gd_create_cq(spec, queue);

	if (err)
		goto out;

	*queue_ptr = queue;
	return 0;
out:
	mana_gd_free_memory(gmi);
free_q:
	free(queue, M_DEVBUF);
	return err;
}

static void
mana_gd_destroy_dma_region(struct gdma_context *gc, uint64_t gdma_region)
{
	struct gdma_destroy_dma_region_req req = {};
	struct gdma_general_resp resp = {};
	int err;

	if (gdma_region == GDMA_INVALID_DMA_REGION)
		return;

	mana_gd_init_req_hdr(&req.hdr, GDMA_DESTROY_DMA_REGION, sizeof(req),
	    sizeof(resp));
	req.gdma_region = gdma_region;

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp),
	    &resp);
	if (err || resp.hdr.status)
		device_printf(gc->dev,
		    "Failed to destroy DMA region: %d, 0x%x\n",
		    err, resp.hdr.status);
}

static int
mana_gd_create_dma_region(struct gdma_dev *gd,
    struct gdma_mem_info *gmi)
{
	unsigned int num_page = gmi->length / PAGE_SIZE;
	struct gdma_create_dma_region_req *req = NULL;
	struct gdma_create_dma_region_resp resp = {};
	struct gdma_context *gc = gd->gdma_context;
	struct hw_channel_context *hwc;
	uint32_t length = gmi->length;
	uint32_t req_msg_size;
	int err;
	int i;

	if (length < PAGE_SIZE || !is_power_of_2(length)) {
		mana_err(NULL, "gmi size incorrect: %u\n", length);
		return EINVAL;
	}

	if (offset_in_page((uint64_t)gmi->virt_addr) != 0) {
		mana_err(NULL, "gmi not page aligned: %p\n",
		    gmi->virt_addr);
		return EINVAL;
	}

	hwc = gc->hwc.driver_data;
	req_msg_size = sizeof(*req) + num_page * sizeof(uint64_t);
	if (req_msg_size > hwc->max_req_msg_size) {
		mana_err(NULL, "req msg size too large: %u, %u\n",
		    req_msg_size, hwc->max_req_msg_size);
		return EINVAL;
	}

	req = malloc(req_msg_size, M_DEVBUF, M_WAITOK | M_ZERO);
	if (!req)
		return ENOMEM;

	mana_gd_init_req_hdr(&req->hdr, GDMA_CREATE_DMA_REGION,
	    req_msg_size, sizeof(resp));
	req->length = length;
	req->offset_in_page = 0;
	req->gdma_page_type = GDMA_PAGE_TYPE_4K;
	req->page_count = num_page;
	req->page_addr_list_len = num_page;

	for (i = 0; i < num_page; i++)
		req->page_addr_list[i] = gmi->dma_handle +  i * PAGE_SIZE;

	err = mana_gd_send_request(gc, req_msg_size, req, sizeof(resp), &resp);
	if (err)
		goto out;

	if (resp.hdr.status || resp.gdma_region == GDMA_INVALID_DMA_REGION) {
		device_printf(gc->dev, "Failed to create DMA region: 0x%x\n",
			resp.hdr.status);
		err = EPROTO;
		goto out;
	}

	gmi->gdma_region = resp.gdma_region;
out:
	free(req, M_DEVBUF);
	return err;
}

int
mana_gd_create_mana_eq(struct gdma_dev *gd,
    const struct gdma_queue_spec *spec,
    struct gdma_queue **queue_ptr)
{
	struct gdma_context *gc = gd->gdma_context;
	struct gdma_mem_info *gmi;
	struct gdma_queue *queue;
	int err;

	if (spec->type != GDMA_EQ)
		return EINVAL;

	queue = malloc(sizeof(*queue),  M_DEVBUF, M_WAITOK | M_ZERO);
	if (!queue)
		return ENOMEM;

	gmi = &queue->mem_info;
	err = mana_gd_alloc_memory(gc, spec->queue_size, gmi);
	if (err)
		goto free_q;

	err = mana_gd_create_dma_region(gd, gmi);
	if (err)
		goto out;

	queue->head = 0;
	queue->tail = 0;
	queue->queue_mem_ptr = gmi->virt_addr;
	queue->queue_size = spec->queue_size;
	queue->monitor_avl_buf = spec->monitor_avl_buf;
	queue->type = spec->type;
	queue->gdma_dev = gd;

	err = mana_gd_create_eq(gd, spec, true, queue);
	if (err)
		goto out;

	*queue_ptr = queue;
	return 0;

out:
	mana_gd_free_memory(gmi);
free_q:
	free(queue, M_DEVBUF);
	return err;
}

int mana_gd_create_mana_wq_cq(struct gdma_dev *gd,
    const struct gdma_queue_spec *spec,
    struct gdma_queue **queue_ptr)
{
	struct gdma_context *gc = gd->gdma_context;
	struct gdma_mem_info *gmi;
	struct gdma_queue *queue;
	int err;

	if (spec->type != GDMA_CQ && spec->type != GDMA_SQ &&
	    spec->type != GDMA_RQ)
		return EINVAL;

	queue = malloc(sizeof(*queue), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!queue)
		return ENOMEM;

	gmi = &queue->mem_info;
	err = mana_gd_alloc_memory(gc, spec->queue_size, gmi);
	if (err)
		goto free_q;

	err = mana_gd_create_dma_region(gd, gmi);
	if (err)
		goto out;

	queue->head = 0;
	queue->tail = 0;
	queue->queue_mem_ptr = gmi->virt_addr;
	queue->queue_size = spec->queue_size;
	queue->monitor_avl_buf = spec->monitor_avl_buf;
	queue->type = spec->type;
	queue->gdma_dev = gd;

	if (spec->type == GDMA_CQ)
		mana_gd_create_cq(spec, queue);

	*queue_ptr = queue;
	return 0;

out:
	mana_gd_free_memory(gmi);
free_q:
	free(queue, M_DEVBUF);
	return err;
}

void
mana_gd_destroy_queue(struct gdma_context *gc, struct gdma_queue *queue)
{
	struct gdma_mem_info *gmi = &queue->mem_info;

	switch (queue->type) {
	case GDMA_EQ:
		mana_gd_destroy_eq(gc, queue->eq.disable_needed, queue);
		break;

	case GDMA_CQ:
		mana_gd_destroy_cq(gc, queue);
		break;

	case GDMA_RQ:
		break;

	case GDMA_SQ:
		break;

	default:
		device_printf(gc->dev,
		    "Can't destroy unknown queue: type = %d\n",
		    queue->type);
		return;
	}

	mana_gd_destroy_dma_region(gc, gmi->gdma_region);
	mana_gd_free_memory(gmi);
	free(queue, M_DEVBUF);
}

int
mana_gd_verify_vf_version(device_t dev)
{
	struct gdma_context *gc = device_get_softc(dev);
	struct gdma_verify_ver_resp resp = {};
	struct gdma_verify_ver_req req = {};
	int err;

	mana_gd_init_req_hdr(&req.hdr, GDMA_VERIFY_VF_DRIVER_VERSION,
	    sizeof(req), sizeof(resp));

	req.protocol_ver_min = GDMA_PROTOCOL_FIRST;
	req.protocol_ver_max = GDMA_PROTOCOL_LAST;

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp), &resp);
	if (err || resp.hdr.status) {
		device_printf(gc->dev,
		    "VfVerifyVersionOutput: %d, status=0x%x\n",
		    err, resp.hdr.status);
		return err ? err : EPROTO;
	}

	return 0;
}

int
mana_gd_register_device(struct gdma_dev *gd)
{
	struct gdma_context *gc = gd->gdma_context;
	struct gdma_register_device_resp resp = {};
	struct gdma_general_req req = {};
	int err;

	gd->pdid = INVALID_PDID;
	gd->doorbell = INVALID_DOORBELL;
	gd->gpa_mkey = INVALID_MEM_KEY;

	mana_gd_init_req_hdr(&req.hdr, GDMA_REGISTER_DEVICE, sizeof(req),
	    sizeof(resp));

	req.hdr.dev_id = gd->dev_id;

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp), &resp);
	if (err || resp.hdr.status) {
		device_printf(gc->dev,
		    "gdma_register_device_resp failed: %d, 0x%x\n",
		    err, resp.hdr.status);
		return err ? err : -EPROTO;
	}

	gd->pdid = resp.pdid;
	gd->gpa_mkey = resp.gpa_mkey;
	gd->doorbell = resp.db_id;

	mana_dbg(NULL, "mana device pdid %u, gpa_mkey %u, doorbell %u \n",
	    gd->pdid, gd->gpa_mkey, gd->doorbell);

	return 0;
}

int
mana_gd_deregister_device(struct gdma_dev *gd)
{
	struct gdma_context *gc = gd->gdma_context;
	struct gdma_general_resp resp = {};
	struct gdma_general_req req = {};
	int err;

	if (gd->pdid == INVALID_PDID)
		return EINVAL;

	mana_gd_init_req_hdr(&req.hdr, GDMA_DEREGISTER_DEVICE, sizeof(req),
	    sizeof(resp));

	req.hdr.dev_id = gd->dev_id;

	err = mana_gd_send_request(gc, sizeof(req), &req, sizeof(resp), &resp);
	if (err || resp.hdr.status) {
		device_printf(gc->dev,
		    "Failed to deregister device: %d, 0x%x\n",
		    err, resp.hdr.status);
		if (!err)
			err = EPROTO;
	}

	gd->pdid = INVALID_PDID;
	gd->doorbell = INVALID_DOORBELL;
	gd->gpa_mkey = INVALID_MEM_KEY;

	return err;
}

uint32_t
mana_gd_wq_avail_space(struct gdma_queue *wq)
{
	uint32_t used_space = (wq->head - wq->tail) * GDMA_WQE_BU_SIZE;
	uint32_t wq_size = wq->queue_size;

	if (used_space > wq_size) {
		mana_warn(NULL, "failed: used space %u > queue size %u\n",
		    used_space, wq_size);
	}

	return wq_size - used_space;
}

uint8_t *
mana_gd_get_wqe_ptr(const struct gdma_queue *wq, uint32_t wqe_offset)
{
	uint32_t offset =
	    (wqe_offset * GDMA_WQE_BU_SIZE) & (wq->queue_size - 1);

	if ((offset + GDMA_WQE_BU_SIZE) > wq->queue_size) {
		mana_warn(NULL, "failed: write end out of queue bound %u, "
		    "queue size %u\n",
		    offset + GDMA_WQE_BU_SIZE, wq->queue_size);
	}

	return (uint8_t *)wq->queue_mem_ptr + offset;
}

static uint32_t
mana_gd_write_client_oob(const struct gdma_wqe_request *wqe_req,
    enum gdma_queue_type q_type,
    uint32_t client_oob_size, uint32_t sgl_data_size,
    uint8_t *wqe_ptr)
{
	bool oob_in_sgl = !!(wqe_req->flags & GDMA_WR_OOB_IN_SGL);
	bool pad_data = !!(wqe_req->flags & GDMA_WR_PAD_BY_SGE0);
	struct gdma_wqe *header = (struct gdma_wqe *)wqe_ptr;
	uint8_t *ptr;

	memset(header, 0, sizeof(struct gdma_wqe));
	header->num_sge = wqe_req->num_sge;
	header->inline_oob_size_div4 = client_oob_size / sizeof(uint32_t);

	if (oob_in_sgl) {
		if (!pad_data || wqe_req->num_sge < 2) {
			mana_warn(NULL, "no pad_data or num_sge < 2\n");
		}

		header->client_oob_in_sgl = 1;

		if (pad_data)
			header->last_vbytes = wqe_req->sgl[0].size;
	}

	if (q_type == GDMA_SQ)
		header->client_data_unit = wqe_req->client_data_unit;

	/*
	 * The size of gdma_wqe + client_oob_size must be less than or equal
	 * to one Basic Unit (i.e. 32 bytes), so the pointer can't go beyond
	 * the queue memory buffer boundary.
	 */
	ptr = wqe_ptr + sizeof(header);

	if (wqe_req->inline_oob_data && wqe_req->inline_oob_size > 0) {
		memcpy(ptr, wqe_req->inline_oob_data, wqe_req->inline_oob_size);

		if (client_oob_size > wqe_req->inline_oob_size)
			memset(ptr + wqe_req->inline_oob_size, 0,
			       client_oob_size - wqe_req->inline_oob_size);
	}

	return sizeof(header) + client_oob_size;
}

static void
mana_gd_write_sgl(struct gdma_queue *wq, uint8_t *wqe_ptr,
    const struct gdma_wqe_request *wqe_req)
{
	uint32_t sgl_size = sizeof(struct gdma_sge) * wqe_req->num_sge;
	const uint8_t *address = (uint8_t *)wqe_req->sgl;
	uint8_t *base_ptr, *end_ptr;
	uint32_t size_to_end;

	base_ptr = wq->queue_mem_ptr;
	end_ptr = base_ptr + wq->queue_size;
	size_to_end = (uint32_t)(end_ptr - wqe_ptr);

	if (size_to_end < sgl_size) {
		memcpy(wqe_ptr, address, size_to_end);

		wqe_ptr = base_ptr;
		address += size_to_end;
		sgl_size -= size_to_end;
	}

	memcpy(wqe_ptr, address, sgl_size);
}

int
mana_gd_post_work_request(struct gdma_queue *wq,
    const struct gdma_wqe_request *wqe_req,
    struct gdma_posted_wqe_info *wqe_info)
{
	uint32_t client_oob_size = wqe_req->inline_oob_size;
	struct gdma_context *gc;
	uint32_t sgl_data_size;
	uint32_t max_wqe_size;
	uint32_t wqe_size;
	uint8_t *wqe_ptr;

	if (wqe_req->num_sge == 0)
		return EINVAL;

	if (wq->type == GDMA_RQ) {
		if (client_oob_size != 0)
			return EINVAL;

		client_oob_size = INLINE_OOB_SMALL_SIZE;

		max_wqe_size = GDMA_MAX_RQE_SIZE;
	} else {
		if (client_oob_size != INLINE_OOB_SMALL_SIZE &&
		    client_oob_size != INLINE_OOB_LARGE_SIZE)
			return EINVAL;

		max_wqe_size = GDMA_MAX_SQE_SIZE;
	}

	sgl_data_size = sizeof(struct gdma_sge) * wqe_req->num_sge;
	wqe_size = ALIGN(sizeof(struct gdma_wqe) + client_oob_size +
	    sgl_data_size, GDMA_WQE_BU_SIZE);
	if (wqe_size > max_wqe_size)
		return EINVAL;

	if (wq->monitor_avl_buf && wqe_size > mana_gd_wq_avail_space(wq)) {
		gc = wq->gdma_dev->gdma_context;
		device_printf(gc->dev, "unsuccessful flow control!\n");
		return ENOSPC;
	}

	if (wqe_info)
		wqe_info->wqe_size_in_bu = wqe_size / GDMA_WQE_BU_SIZE;

	wqe_ptr = mana_gd_get_wqe_ptr(wq, wq->head);
	wqe_ptr += mana_gd_write_client_oob(wqe_req, wq->type, client_oob_size,
	    sgl_data_size, wqe_ptr);
	if (wqe_ptr >= (uint8_t *)wq->queue_mem_ptr + wq->queue_size)
		wqe_ptr -= wq->queue_size;

	mana_gd_write_sgl(wq, wqe_ptr, wqe_req);

	wq->head += wqe_size / GDMA_WQE_BU_SIZE;

	bus_dmamap_sync(wq->mem_info.dma_tag, wq->mem_info.dma_map,
	    BUS_DMASYNC_PREWRITE);

	return 0;
}

int
mana_gd_post_and_ring(struct gdma_queue *queue,
    const struct gdma_wqe_request *wqe_req,
    struct gdma_posted_wqe_info *wqe_info)
{
	struct gdma_context *gc = queue->gdma_dev->gdma_context;
	int err;

	err = mana_gd_post_work_request(queue, wqe_req, wqe_info);
	if (err)
		return err;

	mana_gd_wq_ring_doorbell(gc, queue);

	return 0;
}

static int
mana_gd_read_cqe(struct gdma_queue *cq, struct gdma_comp *comp)
{
	unsigned int num_cqe = cq->queue_size / sizeof(struct gdma_cqe);
	struct gdma_cqe *cq_cqe = cq->queue_mem_ptr;
	uint32_t owner_bits, new_bits, old_bits;
	struct gdma_cqe *cqe;

	cqe = &cq_cqe[cq->head % num_cqe];
	owner_bits = cqe->cqe_info.owner_bits;

	old_bits = (cq->head / num_cqe - 1) & GDMA_CQE_OWNER_MASK;
	/* Return 0 if no more entries. */
	if (owner_bits == old_bits)
		return 0;

	new_bits = (cq->head / num_cqe) & GDMA_CQE_OWNER_MASK;
	/* Return -1 if overflow detected. */
	if (owner_bits != new_bits)
		return -1;

	comp->wq_num = cqe->cqe_info.wq_num;
	comp->is_sq = cqe->cqe_info.is_sq;
	memcpy(comp->cqe_data, cqe->cqe_data, GDMA_COMP_DATA_SIZE);

	return 1;
}

int
mana_gd_poll_cq(struct gdma_queue *cq, struct gdma_comp *comp, int num_cqe)
{
	int cqe_idx;
	int ret;

	bus_dmamap_sync(cq->mem_info.dma_tag, cq->mem_info.dma_map,
	    BUS_DMASYNC_POSTREAD);

	for (cqe_idx = 0; cqe_idx < num_cqe; cqe_idx++) {
		ret = mana_gd_read_cqe(cq, &comp[cqe_idx]);

		if (ret < 0) {
			cq->head -= cqe_idx;
			return ret;
		}

		if (ret == 0)
			break;

		cq->head++;
	}

	return cqe_idx;
}

static void
mana_gd_intr(void *arg)
{
	struct gdma_irq_context *gic = arg;

	if (gic->handler) {
		gic->handler(gic->arg);
	}
}

int
mana_gd_alloc_res_map(uint32_t res_avail,
    struct gdma_resource *r, const char *lock_name)
{
	int n = howmany(res_avail, BITS_PER_LONG);

	r->map =
	    malloc(n * sizeof(unsigned long), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!r->map)
		return ENOMEM;

	r->size = res_avail;
	mtx_init(&r->lock_spin, lock_name, NULL, MTX_SPIN);

	mana_dbg(NULL,
	    "total res %u, total number of unsigned longs %u\n",
	    r->size, n);
	return (0);
}

void
mana_gd_free_res_map(struct gdma_resource *r)
{
	if (!r || !r->map)
		return;

	free(r->map, M_DEVBUF);
	r->map = NULL;
	r->size = 0;
}

static void
mana_gd_init_registers(struct gdma_context *gc)
{
	uint64_t bar0_va = rman_get_bushandle(gc->bar0);

	gc->db_page_size = mana_gd_r32(gc, GDMA_REG_DB_PAGE_SIZE) & 0xFFFF;

	gc->db_page_base =
	    (void *) (bar0_va + mana_gd_r64(gc, GDMA_REG_DB_PAGE_OFFSET));

	gc->shm_base =
	    (void *) (bar0_va + mana_gd_r64(gc, GDMA_REG_SHM_OFFSET));

	mana_dbg(NULL, "db_page_size 0x%xx, db_page_base %p,"
		    " shm_base %p\n",
		    gc->db_page_size, gc->db_page_base, gc->shm_base);
}

static struct resource *
mana_gd_alloc_bar(device_t dev, int bar)
{
	struct resource *res = NULL;
	struct pci_map *pm;
	int rid, type;

	if (bar < 0 || bar > PCIR_MAX_BAR_0)
		goto alloc_bar_out;

	pm = pci_find_bar(dev, PCIR_BAR(bar));
	if (!pm)
		goto alloc_bar_out;

	if (PCI_BAR_IO(pm->pm_value))
		type = SYS_RES_IOPORT;
	else
		type = SYS_RES_MEMORY;
	if (type < 0)
		goto alloc_bar_out;

	rid = PCIR_BAR(bar);
	res = bus_alloc_resource_any(dev, type, &rid, RF_ACTIVE);
#if defined(__amd64__)
	if (res)
		mana_dbg(NULL, "bar %d: rid 0x%x, type 0x%jx,"
		    " handle 0x%jx\n",
		    bar, rid, res->r_bustag, res->r_bushandle);
#endif

alloc_bar_out:
	return (res);
}

static void
mana_gd_free_pci_res(struct gdma_context *gc)
{
	if (!gc || gc->dev)
		return;

	if (gc->bar0 != NULL) {
		bus_release_resource(gc->dev, SYS_RES_MEMORY,
		    PCIR_BAR(GDMA_BAR0), gc->bar0);
	}

	if (gc->msix != NULL) {
		bus_release_resource(gc->dev, SYS_RES_MEMORY,
		    gc->msix_rid, gc->msix);
	}
}

static int
mana_gd_setup_irqs(device_t dev)
{
	unsigned int max_queues_per_port = mp_ncpus;
	struct gdma_context *gc = device_get_softc(dev);
	struct gdma_irq_context *gic;
	unsigned int max_irqs;
	int nvec;
	int rc, rcc, i;

	if (max_queues_per_port > MANA_MAX_NUM_QUEUES)
		max_queues_per_port = MANA_MAX_NUM_QUEUES;

	max_irqs = max_queues_per_port * MAX_PORTS_IN_MANA_DEV;

	/* Need 1 interrupt for the Hardware communication Channel (HWC) */
	max_irqs++;

	nvec = max_irqs;
	rc = pci_alloc_msix(dev, &nvec);
	if (unlikely(rc != 0)) {
		device_printf(dev,
		    "Failed to allocate MSIX, vectors %d, error: %d\n",
		    nvec, rc);
		rc = ENOSPC;
		goto err_setup_irq_alloc;
	}

	if (nvec != max_irqs) {
		if (nvec == 1) {
			device_printf(dev,
			    "Not enough number of MSI-x allocated: %d\n",
			    nvec);
			rc = ENOSPC;
			goto err_setup_irq_release;
		}
		device_printf(dev, "Allocated only %d MSI-x (%d requested)\n",
		    nvec, max_irqs);
	}

	gc->irq_contexts = malloc(nvec * sizeof(struct gdma_irq_context),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (!gc->irq_contexts) {
		rc = ENOMEM;
		goto err_setup_irq_release;
	}

	for (i = 0; i < nvec; i++) {
		gic = &gc->irq_contexts[i];
		gic->msix_e.entry = i;
		/* Vector starts from 1. */
		gic->msix_e.vector = i + 1;
		gic->handler = NULL;
		gic->arg = NULL;

		gic->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &gic->msix_e.vector, RF_ACTIVE | RF_SHAREABLE);
		if (unlikely(gic->res == NULL)) {
			rc = ENOMEM;
			device_printf(dev, "could not allocate resource "
			    "for irq vector %d\n", gic->msix_e.vector);
			goto err_setup_irq;
		}

		rc = bus_setup_intr(dev, gic->res,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, mana_gd_intr,
		    gic, &gic->cookie);
		if (unlikely(rc != 0)) {
			device_printf(dev, "failed to register interrupt "
			    "handler for irq %ju vector %d: error %d\n",
			    rman_get_start(gic->res), gic->msix_e.vector, rc);
			goto err_setup_irq;
		}
		gic->requested = true;

		mana_dbg(NULL, "added msix vector %d irq %ju\n",
		    gic->msix_e.vector, rman_get_start(gic->res));
	}

	rc = mana_gd_alloc_res_map(nvec, &gc->msix_resource,
	    "gdma msix res lock");
	if (rc != 0) {
		device_printf(dev, "failed to allocate memory "
		    "for msix bitmap\n");
		goto err_setup_irq;
	}

	gc->max_num_msix = nvec;
	gc->num_msix_usable = nvec;

	mana_dbg(NULL, "setup %d msix interrupts\n", nvec);

	return (0);

err_setup_irq:
	for (; i >= 0; i--) {
		gic = &gc->irq_contexts[i];
		rcc = 0;

		/*
		 * If gic->requested is true, we need to free both intr and
		 * resources.
		 */
		if (gic->requested)
			rcc = bus_teardown_intr(dev, gic->res, gic->cookie);
		if (unlikely(rcc != 0))
			device_printf(dev, "could not release "
			    "irq vector %d, error: %d\n",
			    gic->msix_e.vector, rcc);

		rcc = 0;
		if (gic->res != NULL) {
			rcc = bus_release_resource(dev, SYS_RES_IRQ,
			    gic->msix_e.vector, gic->res);
		}
		if (unlikely(rcc != 0))
			device_printf(dev, "dev has no parent while "
			    "releasing resource for irq vector %d\n",
			    gic->msix_e.vector);
		gic->requested = false;
		gic->res = NULL;
	}

	free(gc->irq_contexts, M_DEVBUF);
	gc->irq_contexts = NULL;
err_setup_irq_release:
	pci_release_msi(dev);
err_setup_irq_alloc:
	return (rc);
}

static void
mana_gd_remove_irqs(device_t dev)
{
	struct gdma_context *gc = device_get_softc(dev);
	struct gdma_irq_context *gic;
	int rc, i;

	mana_gd_free_res_map(&gc->msix_resource);

	for (i = 0; i < gc->max_num_msix; i++) {
		gic = &gc->irq_contexts[i];
		if (gic->requested) {
			rc = bus_teardown_intr(dev, gic->res, gic->cookie);
			if (unlikely(rc != 0)) {
				device_printf(dev, "failed to tear down "
				    "irq vector %d, error: %d\n",
				    gic->msix_e.vector, rc);
			}
			gic->requested = false;
		}

		if (gic->res != NULL) {
			rc = bus_release_resource(dev, SYS_RES_IRQ,
			    gic->msix_e.vector, gic->res);
			if (unlikely(rc != 0)) {
				device_printf(dev, "dev has no parent while "
				    "releasing resource for irq vector %d\n",
				    gic->msix_e.vector);
			}
			gic->res = NULL;
		}
	}

	gc->max_num_msix = 0;
	gc->num_msix_usable = 0;
	free(gc->irq_contexts, M_DEVBUF);
	gc->irq_contexts = NULL;

	pci_release_msi(dev);
}

static int
mana_gd_probe(device_t dev)
{
	mana_vendor_id_t *ent;
	char		adapter_name[60];
	uint16_t	pci_vendor_id = 0;
	uint16_t	pci_device_id = 0;

	pci_vendor_id = pci_get_vendor(dev);
	pci_device_id = pci_get_device(dev);

	ent = mana_id_table;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id)) {
			mana_dbg(NULL, "vendor=%x device=%x\n",
			    pci_vendor_id, pci_device_id);

			sprintf(adapter_name, DEVICE_DESC);
			device_set_desc_copy(dev, adapter_name);
			return (BUS_PROBE_DEFAULT);
		}

		ent++;
	}

	return (ENXIO);
}

/**
 * mana_attach - Device Initialization Routine
 * @dev: device information struct
 *
 * Returns 0 on success, otherwise on failure.
 *
 * mana_attach initializes a GDMA adapter identified by a device structure.
 **/
static int
mana_gd_attach(device_t dev)
{
	struct gdma_context *gc;
	int msix_rid;
	int rc;

	gc = device_get_softc(dev);
	gc->dev = dev;

	pci_enable_io(dev, SYS_RES_IOPORT);
	pci_enable_io(dev, SYS_RES_MEMORY);

	pci_enable_busmaster(dev);

	gc->bar0 = mana_gd_alloc_bar(dev, GDMA_BAR0);
	if (unlikely(gc->bar0 == NULL)) {
		device_printf(dev,
		    "unable to allocate bus resource for bar0!\n");
		rc = ENOMEM;
		goto err_disable_dev;
	}

	/* Store bar0 tage and handle for quick access */
	gc->gd_bus.bar0_t = rman_get_bustag(gc->bar0);
	gc->gd_bus.bar0_h = rman_get_bushandle(gc->bar0);

	/* Map MSI-x vector table */
	msix_rid = pci_msix_table_bar(dev);

	mana_dbg(NULL, "msix_rid 0x%x\n", msix_rid);

	gc->msix = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &msix_rid, RF_ACTIVE);
	if (unlikely(gc->msix == NULL)) {
		device_printf(dev,
		    "unable to allocate bus resource for msix!\n");
		rc = ENOMEM;
		goto err_free_pci_res;
	}
	gc->msix_rid = msix_rid;

	if (unlikely(gc->gd_bus.bar0_h  == 0)) {
		device_printf(dev, "failed to map bar0!\n");
		rc = ENXIO;
		goto err_free_pci_res;
	}

	mana_gd_init_registers(gc);

	mana_smc_init(&gc->shm_channel, gc->dev, gc->shm_base);

	rc = mana_gd_setup_irqs(dev);
	if (rc) {
		goto err_free_pci_res;
	}

	sx_init(&gc->eq_test_event_sx, "gdma test event sx");

	rc = mana_hwc_create_channel(gc);
	if (rc) {
		mana_dbg(NULL, "Failed to create hwc channel\n");
		if (rc == EIO)
			goto err_clean_up_gdma;
		else
			goto err_remove_irq;
	}

	rc = mana_gd_verify_vf_version(dev);
	if (rc) {
		mana_dbg(NULL, "Failed to verify vf\n");
		goto err_clean_up_gdma;
	}

	rc = mana_gd_query_max_resources(dev);
	if (rc) {
		mana_dbg(NULL, "Failed to query max resources\n");
		goto err_clean_up_gdma;
	}

	rc = mana_gd_detect_devices(dev);
	if (rc) {
		mana_dbg(NULL, "Failed to detect  mana device\n");
		goto err_clean_up_gdma;
	}

	rc = mana_probe(&gc->mana);
	if (rc) {
		mana_dbg(NULL, "Failed to probe mana device\n");
		goto err_clean_up_gdma;
	}

	return (0);

err_clean_up_gdma:
	mana_hwc_destroy_channel(gc);
	if (gc->cq_table)
		free(gc->cq_table, M_DEVBUF);
	gc->cq_table = NULL;
err_remove_irq:
	mana_gd_remove_irqs(dev);
err_free_pci_res:
	mana_gd_free_pci_res(gc);
err_disable_dev:
	pci_disable_busmaster(dev);

	return(rc);
}

/**
 * mana_detach - Device Removal Routine
 * @pdev: device information struct
 *
 * mana_detach is called by the device subsystem to alert the driver
 * that it should release a PCI device.
 **/
static int
mana_gd_detach(device_t dev)
{
	struct gdma_context *gc = device_get_softc(dev);

	mana_remove(&gc->mana);

	mana_hwc_destroy_channel(gc);
	free(gc->cq_table, M_DEVBUF);
	gc->cq_table = NULL;

	mana_gd_remove_irqs(dev);

	mana_gd_free_pci_res(gc);

	pci_disable_busmaster(dev);

	return (bus_generic_detach(dev));
}


/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t mana_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe, mana_gd_probe),
    DEVMETHOD(device_attach, mana_gd_attach),
    DEVMETHOD(device_detach, mana_gd_detach),
    DEVMETHOD_END
};

static driver_t mana_driver = {
    "mana", mana_methods, sizeof(struct gdma_context),
};

devclass_t mana_devclass;
DRIVER_MODULE(mana, pci, mana_driver, mana_devclass, 0, 0);
MODULE_PNP_INFO("U16:vendor;U16:device", pci, mana, mana_id_table,
    nitems(mana_id_table) - 1);
MODULE_DEPEND(mana, pci, 1, 1, 1);
MODULE_DEPEND(mana, ether, 1, 1, 1);

/*********************************************************************/
