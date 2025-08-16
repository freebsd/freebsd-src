/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/domainset.h>
#include <sys/module.h>

#include "sys/kassert.h"
#include "ufshci_private.h"
#include "ufshci_reg.h"

static void
ufshci_req_sdb_cmd_desc_destroy(struct ufshci_req_queue *req_queue)
{
	struct ufshci_hw_queue *hwq = &req_queue->hwq[UFSHCI_SDB_Q];
	struct ufshci_tracker *tr;
	int i;

	for (i = 0; i < req_queue->num_trackers; i++) {
		tr = hwq->act_tr[i];
		bus_dmamap_destroy(req_queue->dma_tag_payload,
		    tr->payload_dma_map);
	}

	if (req_queue->ucd) {
		bus_dmamap_unload(req_queue->dma_tag_ucd,
		    req_queue->ucdmem_map);
		bus_dmamem_free(req_queue->dma_tag_ucd, req_queue->ucd,
		    req_queue->ucdmem_map);
		req_queue->ucd = NULL;
	}

	if (req_queue->dma_tag_ucd) {
		bus_dma_tag_destroy(req_queue->dma_tag_ucd);
		req_queue->dma_tag_ucd = NULL;
	}
}

static void
ufshci_ucd_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	struct ufshci_hw_queue *hwq = arg;
	int i;

	if (error != 0) {
		printf("ufshci: Failed to map UCD, error = %d\n", error);
		return;
	}

	if (hwq->num_trackers != nseg) {
		printf(
		    "ufshci: Failed to map UCD, num_trackers = %d, nseg = %d\n",
		    hwq->num_trackers, nseg);
		return;
	}

	for (i = 0; i < nseg; i++) {
		hwq->ucd_bus_addr[i] = seg[i].ds_addr;
	}
}

static int
ufshci_req_sdb_cmd_desc_construct(struct ufshci_req_queue *req_queue,
    uint32_t num_entries, struct ufshci_controller *ctrlr)
{
	struct ufshci_hw_queue *hwq = &req_queue->hwq[UFSHCI_SDB_Q];
	size_t ucd_allocsz, payload_allocsz;
	uint8_t *ucdmem;
	int i, error;

	/*
	 * Each component must be page aligned, and individual PRP lists
	 * cannot cross a page boundary.
	 */
	ucd_allocsz = num_entries * sizeof(struct ufshci_utp_cmd_desc);
	ucd_allocsz = roundup2(ucd_allocsz, ctrlr->page_size);
	payload_allocsz = num_entries * ctrlr->max_xfer_size;

	/*
	 * Allocate physical memory for UTP Command Descriptor (UCD)
	 * Note: UFSHCI UCD format is restricted to 128-byte alignment.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev), 128, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, ucd_allocsz,
	    howmany(ucd_allocsz, sizeof(struct ufshci_utp_cmd_desc)),
	    sizeof(struct ufshci_utp_cmd_desc), 0, NULL, NULL,
	    &req_queue->dma_tag_ucd);
	if (error != 0) {
		ufshci_printf(ctrlr, "request cmd desc tag create failed %d\n",
		    error);
		goto out;
	}

	if (bus_dmamem_alloc(req_queue->dma_tag_ucd, (void **)&ucdmem,
		BUS_DMA_COHERENT | BUS_DMA_NOWAIT, &req_queue->ucdmem_map)) {
		ufshci_printf(ctrlr, "failed to allocate cmd desc memory\n");
		goto out;
	}

	if (bus_dmamap_load(req_queue->dma_tag_ucd, req_queue->ucdmem_map,
		ucdmem, ucd_allocsz, ufshci_ucd_map, hwq, 0) != 0) {
		ufshci_printf(ctrlr, "failed to load cmd desc memory\n");
		bus_dmamem_free(req_queue->dma_tag_ucd, req_queue->ucd,
		    req_queue->ucdmem_map);
		goto out;
	}

	req_queue->ucd = (struct ufshci_utp_cmd_desc *)ucdmem;

	/*
	 * Allocate physical memory for PRDT
	 * Note: UFSHCI PRDT format is restricted to 8-byte alignment.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev), 8,
	    ctrlr->page_size, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    payload_allocsz, howmany(payload_allocsz, ctrlr->page_size) + 1,
	    ctrlr->page_size, 0, NULL, NULL, &req_queue->dma_tag_payload);
	if (error != 0) {
		ufshci_printf(ctrlr, "request prdt tag create failed %d\n",
		    error);
		goto out;
	}

	for (i = 0; i < req_queue->num_trackers; i++) {
		bus_dmamap_create(req_queue->dma_tag_payload, 0,
		    &hwq->act_tr[i]->payload_dma_map);

		hwq->act_tr[i]->ucd = (struct ufshci_utp_cmd_desc *)ucdmem;
		hwq->act_tr[i]->ucd_bus_addr = hwq->ucd_bus_addr[i];

		ucdmem += sizeof(struct ufshci_utp_cmd_desc);
	}

	return (0);
out:
	ufshci_req_sdb_cmd_desc_destroy(req_queue);
	return (ENOMEM);
}

int
ufshci_req_sdb_construct(struct ufshci_controller *ctrlr,
    struct ufshci_req_queue *req_queue, uint32_t num_entries, bool is_task_mgmt)
{
	struct ufshci_hw_queue *hwq;
	size_t desc_size, alloc_size;
	uint64_t queuemem_phys;
	uint8_t *queuemem;
	struct ufshci_tracker *tr;
	int i, error;

	req_queue->ctrlr = ctrlr;
	req_queue->is_task_mgmt = is_task_mgmt;
	req_queue->num_entries = num_entries;
	/*
	 * In Single Doorbell mode, the number of queue entries and the number
	 * of trackers are the same.
	 */
	req_queue->num_trackers = num_entries;

	/* Single Doorbell mode uses only one queue. (UFSHCI_SDB_Q = 0) */
	req_queue->hwq = malloc(sizeof(struct ufshci_hw_queue), M_UFSHCI,
	    M_ZERO | M_NOWAIT);
	hwq = &req_queue->hwq[UFSHCI_SDB_Q];
	hwq->num_entries = req_queue->num_entries;
	hwq->num_trackers = req_queue->num_trackers;
	req_queue->hwq->ucd_bus_addr = malloc(sizeof(bus_addr_t) *
		req_queue->num_trackers,
	    M_UFSHCI, M_ZERO | M_NOWAIT);

	mtx_init(&hwq->qlock, "ufshci req_queue lock", NULL, MTX_DEF);

	/*
	 * Allocate physical memory for request queue (UTP Transfer Request
	 * Descriptor (UTRD) or UTP Task Management Request Descriptor (UTMRD))
	 * Note: UTRD/UTMRD format is restricted to 1024-byte alignment.
	 */
	desc_size = is_task_mgmt ?
	    sizeof(struct ufshci_utp_task_mgmt_req_desc) :
	    sizeof(struct ufshci_utp_xfer_req_desc);
	alloc_size = num_entries * desc_size;
	error = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev), 1024,
	    ctrlr->page_size, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    alloc_size, 1, alloc_size, 0, NULL, NULL, &hwq->dma_tag_queue);
	if (error != 0) {
		ufshci_printf(ctrlr, "request queue tag create failed %d\n",
		    error);
		goto out;
	}

	if (bus_dmamem_alloc(hwq->dma_tag_queue, (void **)&queuemem,
		BUS_DMA_COHERENT | BUS_DMA_NOWAIT, &hwq->queuemem_map)) {
		ufshci_printf(ctrlr,
		    "failed to allocate request queue memory\n");
		goto out;
	}

	if (bus_dmamap_load(hwq->dma_tag_queue, hwq->queuemem_map, queuemem,
		alloc_size, ufshci_single_map, &queuemem_phys, 0) != 0) {
		ufshci_printf(ctrlr, "failed to load request queue memory\n");
		bus_dmamem_free(hwq->dma_tag_queue, hwq->utrd,
		    hwq->queuemem_map);
		goto out;
	}

	hwq->num_cmds = 0;
	hwq->num_intr_handler_calls = 0;
	hwq->num_retries = 0;
	hwq->num_failures = 0;
	hwq->req_queue_addr = queuemem_phys;

	/* Allocate trackers */
	hwq->act_tr = malloc_domainset(sizeof(struct ufshci_tracker *) *
		req_queue->num_entries,
	    M_UFSHCI, DOMAINSET_PREF(req_queue->domain), M_ZERO | M_WAITOK);

	for (i = 0; i < req_queue->num_trackers; i++) {
		tr = malloc_domainset(sizeof(struct ufshci_tracker), M_UFSHCI,
		    DOMAINSET_PREF(req_queue->domain), M_ZERO | M_WAITOK);

		tr->req_queue = req_queue;
		tr->slot_num = i;
		tr->slot_state = UFSHCI_SLOT_STATE_FREE;

		hwq->act_tr[i] = tr;
	}

	if (is_task_mgmt) {
		/* UTP Task Management Request (UTMR) */
		uint32_t utmrlba, utmrlbau;

		hwq->utmrd = (struct ufshci_utp_task_mgmt_req_desc *)queuemem;

		utmrlba = hwq->req_queue_addr & 0xffffffff;
		utmrlbau = hwq->req_queue_addr >> 32;
		ufshci_mmio_write_4(ctrlr, utmrlba, utmrlba);
		ufshci_mmio_write_4(ctrlr, utmrlbau, utmrlbau);
	} else {
		/* UTP Transfer Request (UTR) */
		uint32_t utrlba, utrlbau;

		hwq->utrd = (struct ufshci_utp_xfer_req_desc *)queuemem;

		/*
		 * Allocate physical memory for the command descriptor.
		 * UTP Transfer Request (UTR) requires memory for a separate
		 * command in addition to the queue.
		 */
		if (ufshci_req_sdb_cmd_desc_construct(req_queue, num_entries,
			ctrlr) != 0) {
			ufshci_printf(ctrlr,
			    "failed to construct cmd descriptor memory\n");
			bus_dmamem_free(hwq->dma_tag_queue, hwq->utrd,
			    hwq->queuemem_map);
			goto out;
		}

		utrlba = hwq->req_queue_addr & 0xffffffff;
		utrlbau = hwq->req_queue_addr >> 32;
		ufshci_mmio_write_4(ctrlr, utrlba, utrlba);
		ufshci_mmio_write_4(ctrlr, utrlbau, utrlbau);
	}

	return (0);
out:
	ufshci_req_sdb_destroy(ctrlr, req_queue);
	return (ENOMEM);
}

void
ufshci_req_sdb_destroy(struct ufshci_controller *ctrlr,
    struct ufshci_req_queue *req_queue)
{
	struct ufshci_hw_queue *hwq = &req_queue->hwq[UFSHCI_SDB_Q];
	struct ufshci_tracker *tr;
	int i;

	if (!req_queue->is_task_mgmt)
		ufshci_req_sdb_cmd_desc_destroy(&ctrlr->transfer_req_queue);

	for (i = 0; i < req_queue->num_trackers; i++) {
		tr = hwq->act_tr[i];
		free(tr, M_UFSHCI);
	}

	if (hwq->act_tr) {
		free(hwq->act_tr, M_UFSHCI);
		hwq->act_tr = NULL;
	}

	if (hwq->utrd != NULL) {
		bus_dmamap_unload(hwq->dma_tag_queue, hwq->queuemem_map);
		bus_dmamem_free(hwq->dma_tag_queue, hwq->utrd,
		    hwq->queuemem_map);
		hwq->utrd = NULL;
	}

	if (hwq->dma_tag_queue) {
		bus_dma_tag_destroy(hwq->dma_tag_queue);
		hwq->dma_tag_queue = NULL;
	}

	if (mtx_initialized(&hwq->qlock))
		mtx_destroy(&hwq->qlock);

	free(req_queue->hwq->ucd_bus_addr, M_UFSHCI);
	free(req_queue->hwq, M_UFSHCI);
}

struct ufshci_hw_queue *
ufshci_req_sdb_get_hw_queue(struct ufshci_req_queue *req_queue)
{
	return &req_queue->hwq[UFSHCI_SDB_Q];
}

int
ufshci_req_sdb_enable(struct ufshci_controller *ctrlr,
    struct ufshci_req_queue *req_queue)
{
	if (req_queue->is_task_mgmt) {
		uint32_t hcs, utmrldbr, utmrlrsr;

		hcs = ufshci_mmio_read_4(ctrlr, hcs);
		if (!(hcs & UFSHCIM(UFSHCI_HCS_REG_UTMRLRDY))) {
			ufshci_printf(ctrlr,
			    "UTP task management request list is not ready\n");
			return (ENXIO);
		}

		utmrldbr = ufshci_mmio_read_4(ctrlr, utmrldbr);
		if (utmrldbr != 0) {
			ufshci_printf(ctrlr,
			    "UTP task management request list door bell is not ready\n");
			return (ENXIO);
		}

		utmrlrsr = UFSHCIM(UFSHCI_UTMRLRSR_REG_UTMRLRSR);
		ufshci_mmio_write_4(ctrlr, utmrlrsr, utmrlrsr);
	} else {
		uint32_t hcs, utrldbr, utrlcnr, utrlrsr;

		hcs = ufshci_mmio_read_4(ctrlr, hcs);
		if (!(hcs & UFSHCIM(UFSHCI_HCS_REG_UTRLRDY))) {
			ufshci_printf(ctrlr,
			    "UTP transfer request list is not ready\n");
			return (ENXIO);
		}

		utrldbr = ufshci_mmio_read_4(ctrlr, utrldbr);
		if (utrldbr != 0) {
			ufshci_printf(ctrlr,
			    "UTP transfer request list door bell is not ready\n");
			ufshci_printf(ctrlr,
			    "Clear the UTP transfer request list door bell\n");
			ufshci_mmio_write_4(ctrlr, utrldbr, utrldbr);
		}

		utrlcnr = ufshci_mmio_read_4(ctrlr, utrlcnr);
		if (utrlcnr != 0) {
			ufshci_printf(ctrlr,
			    "UTP transfer request list notification is not ready\n");
			ufshci_printf(ctrlr,
			    "Clear the UTP transfer request list notification\n");
			ufshci_mmio_write_4(ctrlr, utrlcnr, utrlcnr);
		}

		utrlrsr = UFSHCIM(UFSHCI_UTRLRSR_REG_UTRLRSR);
		ufshci_mmio_write_4(ctrlr, utrlrsr, utrlrsr);
	}

	return (0);
}

int
ufshci_req_sdb_reserve_slot(struct ufshci_req_queue *req_queue,
    struct ufshci_tracker **tr)
{
	struct ufshci_hw_queue *hwq = &req_queue->hwq[UFSHCI_SDB_Q];
	uint8_t i;

	for (i = 0; i < req_queue->num_entries; i++) {
		if (hwq->act_tr[i]->slot_state == UFSHCI_SLOT_STATE_FREE) {
			*tr = hwq->act_tr[i];
			(*tr)->hwq = hwq;
			return (0);
		}
	}
	return (EBUSY);
}

void
ufshci_req_sdb_utmr_clear_cpl_ntf(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr)
{
	/*
	 * NOP
	 * UTP Task Management does not have a Completion Notification
	 * Register.
	 */
}

void
ufshci_req_sdb_utr_clear_cpl_ntf(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr)
{
	uint32_t utrlcnr;

	utrlcnr = 1 << tr->slot_num;
	ufshci_mmio_write_4(ctrlr, utrlcnr, utrlcnr);
}

void
ufshci_req_sdb_utmr_ring_doorbell(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr)
{
	uint32_t utmrldbr = 0;

	utmrldbr |= 1 << tr->slot_num;
	ufshci_mmio_write_4(ctrlr, utmrldbr, utmrldbr);

	tr->req_queue->hwq[UFSHCI_SDB_Q].num_cmds++;
}

void
ufshci_req_sdb_utr_ring_doorbell(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr)
{
	uint32_t utrldbr = 0;

	utrldbr |= 1 << tr->slot_num;
	ufshci_mmio_write_4(ctrlr, utrldbr, utrldbr);

	tr->req_queue->hwq[UFSHCI_SDB_Q].num_cmds++;
}

bool
ufshci_req_sdb_utmr_is_doorbell_cleared(struct ufshci_controller *ctrlr,
    uint8_t slot)
{
	uint32_t utmrldbr;

	utmrldbr = ufshci_mmio_read_4(ctrlr, utmrldbr);
	return (!(utmrldbr & (1 << slot)));
}

bool
ufshci_req_sdb_utr_is_doorbell_cleared(struct ufshci_controller *ctrlr,
    uint8_t slot)
{
	uint32_t utrldbr;

	utrldbr = ufshci_mmio_read_4(ctrlr, utrldbr);
	return (!(utrldbr & (1 << slot)));
}

bool
ufshci_req_sdb_process_cpl(struct ufshci_req_queue *req_queue)
{
	struct ufshci_hw_queue *hwq = &req_queue->hwq[UFSHCI_SDB_Q];
	struct ufshci_tracker *tr;
	uint8_t slot;
	bool done = false;

	hwq->num_intr_handler_calls++;

	bus_dmamap_sync(hwq->dma_tag_queue, hwq->queuemem_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (slot = 0; slot < req_queue->num_entries; slot++) {
		tr = hwq->act_tr[slot];

		KASSERT(tr, ("there is no tracker assigned to the slot"));
		/*
		 * When the response is delivered from the device, the doorbell
		 * is cleared.
		 */
		if (tr->slot_state == UFSHCI_SLOT_STATE_SCHEDULED &&
		    req_queue->qops.is_doorbell_cleared(req_queue->ctrlr,
			slot)) {
			ufshci_req_queue_complete_tracker(tr);
			done = true;
		}
	}

	return (done);
}

int
ufshci_req_sdb_get_inflight_io(struct ufshci_controller *ctrlr)
{
	/* TODO: Implement inflight io*/

	return (0);
}
