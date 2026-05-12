/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/taskqueue.h>
#include <sys/refcount.h>
#include <vm/vm.h>
#include <vm/vm_page.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/iommu/iommu.h>
#include <riscv/iommu/iommu_pmap.h>
#include <riscv/iommu/iommu.h>

#include "iommu_if.h"

#define	dprintf(fmt, ...)

MALLOC_DEFINE(M_IOMMU, "RISCV_IOMMU", "RISC-V IOMMU");

#define	RD4(sc, reg)		bus_read_4(sc->res[0], (reg))
#define	WR4(sc, reg, val)	bus_write_4(sc->res[0], (reg), (val))
#define	RD8(sc, reg)		bus_read_8(sc->res[0], (reg))
#define	WR8(sc, reg, val)	bus_write_8(sc->res[0], (reg), (val))

#define	CQ_ENTRY_DWORDS		2	/* 16-byte */
#define	CQ_ENTRY_COUNT		8192	/* Amount of 16-byte entries. */
#define	FQ_ENTRY_DWORDS		4	/* 32-byte */
#define	FQ_ENTRY_COUNT		8192	/* Amount of 32-byte entries. */
#define	PQ_ENTRY_DWORDS		2	/* 16-byte */
#define	PQ_ENTRY_COUNT		8192	/* Amount of 16-byte entries. */

#define	DDT_NON_LEAF_DWORDS	1
#define	DDT_DC_STD_DWORDS	4	/* Standard-format DC. */
#define	DDT_DC_EXT_DWORDS	8	/* Extended-format DC. */
#define	DDT_L1_DID_BITS		9	/* All formats. */

#define	QUEUE_ALIGN		(1024 * 1024)	/* TODO */
#define	QUEUE_HEAD(q)		((q)->csr + RISCV_IOMMU_CQH - RISCV_IOMMU_CQB)
#define	QUEUE_TAIL(q)		((q)->csr + RISCV_IOMMU_CQT - RISCV_IOMMU_CQB)
#define	QUEUE_IPSR(q)		(1 << (q)->idx)

#define	PHYS_TO_PPN(p)		((p) >> 12)

struct riscv_iommu_fq_event {
	uint16_t cause_id;
	char *descr;
};

static struct riscv_iommu_fq_event fq_events[] = {
	{ FQ_CAUSE_INST_FAULT,		"Instruction access fault" },
	{ FQ_CAUSE_RD_ADDR_MISALIGNED,	"Read address misaligned" },
	{ FQ_CAUSE_RD_FAULT,		"Read access fault" },
	{ FQ_CAUSE_WR_ADDR_MISALIGNED,	"Write/AMO address misaligned" },
	{ FQ_CAUSE_WR_FAULT,		"Write/AMO access fault" },
	{ FQ_CAUSE_INST_FAULT_S,	"Instruction page fault" },
	{ FQ_CAUSE_RD_FAULT_S,		"Read page fault" },
	{ FQ_CAUSE_WR_FAULT_S,		"Write/AMO page fault" },
	{ FQ_CAUSE_INST_FAULT_VS,	"Instruction guest page fault" },
	{ FQ_CAUSE_RD_FAULT_VS,		"Read guest-page fault" },
	{ FQ_CAUSE_WR_FAULT_VS,		"Write/AMO guest-page fault" },
	{ FQ_CAUSE_DMA_DISABLED,	"All inbound transactions disallowed" },
	{ FQ_CAUSE_DDT_LOAD_FAULT,	"DDT entry load access fault" },
	{ FQ_CAUSE_DDT_INVALID,		"DDT entry not valid" },
	{ FQ_CAUSE_DDT_MISCONFIGURED,	"DDT entry misconfigured" },
	{ FQ_CAUSE_TR_TYPE_DISALLOWED,	"Transaction type disallowed" },
	{ FQ_CAUSE_MSI_LOAD_FAULT,	"MSI PTE load access fault" },
	{ FQ_CAUSE_MSI_INVALID, 	"MSI PTE not valid" },
	{ FQ_CAUSE_MSI_MISCONFIGURED,	"MSI PTE misconfigured" },
	{ FQ_CAUSE_MRIF_FAULT,		"MRIF access fault" },
	{ FQ_CAUSE_PDT_LOAD_FAULT,	"PDT entry load access fault" },
	{ FQ_CAUSE_PDT_INVALID,		"PDT entry not valid" },
	{ FQ_CAUSE_PDT_MISCONFIGURED,	"PDT entry misconfigured" },
	{ FQ_CAUSE_DDT_CORRUPTED,	"DDT data corruption" },
	{ FQ_CAUSE_PDT_CORRUPTED,	"PDT data corruption" },
	{ FQ_CAUSE_MSI_PT_CORRUPTED,	"MSI PT data corruption" },
	{ FQ_CAUSE_MRIF_CORRUPTED,	"MSI MRIF data corruption" },
	{ FQ_CAUSE_INTERNAL_DP_ERROR,	"Internal data path error" },
	{ FQ_CAUSE_MSI_WR_FAULT,	"IOMMU MSI write access fault" },
	{ FQ_CAUSE_PT_CORRUPTED,	"1st/2nd-stage PT data corruption" },
	{ 0, NULL },
};

static void
riscv_iommu_init_pscids(struct riscv_iommu_softc *sc)
{

	sc->pscid_set_size = (1 << sc->pscid_bits);
	sc->pscid_set = bit_alloc(sc->pscid_set_size, M_IOMMU, M_WAITOK);
	mtx_init(&sc->pscid_set_mutex, "pscid set", NULL, MTX_SPIN);
}

static int
riscv_iommu_pscid_alloc(struct riscv_iommu_softc *sc, int *new_pscid)
{

	mtx_lock_spin(&sc->pscid_set_mutex);
	bit_ffc(sc->pscid_set, sc->pscid_set_size, new_pscid);
	if (*new_pscid == -1) {
		mtx_unlock_spin(&sc->pscid_set_mutex);
		return (ENOMEM);
	}
	bit_set(sc->pscid_set, *new_pscid);
	mtx_unlock_spin(&sc->pscid_set_mutex);

	return (0);
}

static void
riscv_iommu_pscid_free(struct riscv_iommu_softc *sc, int pscid)
{

	mtx_lock_spin(&sc->pscid_set_mutex);
	bit_clear(sc->pscid_set, pscid);
	mtx_unlock_spin(&sc->pscid_set_mutex);
}

static uint32_t
riscv_iommu_q_inc_tail(struct riscv_iommu_queue *q)
{

	return ((q->lc.tail + 1) & q->mask);
}

static uint32_t
riscv_iommu_q_inc_head(struct riscv_iommu_queue *q)
{

	return ((q->lc.head + 1) & q->mask);
}

static int
riscv_iommu_q_has_space(struct riscv_iommu_queue *q)
{

	if (riscv_iommu_q_inc_tail(q) != q->lc.head)
		return (1);

	return (0);
}

static int
riscv_iommu_q_empty(struct riscv_iommu_queue *q)
{

	if (q->lc.tail == q->lc.head)
		return (1);

	return (0);
}

static int
riscv_iommu_dequeue(struct riscv_iommu_softc *sc, struct riscv_iommu_queue *q,
    void *data)
{
	void *entry_addr;

	q->lc.val = RD8(sc, q->head_off);
	entry_addr = (void *)((uint64_t)q->vaddr + q->lc.head * q->entry_size);
	memcpy(data, entry_addr, q->entry_size);
	q->lc.head = riscv_iommu_q_inc_head(q);
	WR4(sc, q->head_off, q->lc.head);

	return (0);
}

static int
riscv_iommu_enqueue(struct riscv_iommu_softc *sc, struct riscv_iommu_queue *q,
    void *data)
{
	void *entry_addr;

	RISCV_IOMMU_LOCK(sc);

	/* Ensure that a space is available. */
	do {
		q->lc.head = RD4(sc, q->head_off);
	} while (riscv_iommu_q_has_space(q) == 0);

	/* Write the command to the current tail entry. */
	entry_addr = (void *)((uint64_t)q->vaddr + q->lc.tail * q->entry_size);
	memcpy(entry_addr, data, q->entry_size);

	/* Increment tail index. */
	q->lc.tail = riscv_iommu_q_inc_tail(q);
	WR4(sc, q->tail_off, q->lc.tail);

	RISCV_IOMMU_UNLOCK(sc);

	return (0);
}

static void
riscv_iommu_sync(struct riscv_iommu_softc *sc, struct riscv_iommu_queue *q)
{
	struct riscv_iommu_command cmd;
	uint64_t reg;

	bzero(&cmd, sizeof(struct riscv_iommu_command));
	reg = COMMAND_OPCODE_IOFENCE;
	reg |= FUNC_IOFENCE_FUNC_C | FUNC_IOFENCE_PR | FUNC_IOFENCE_PW;
	cmd.dword0 = reg;

	riscv_iommu_enqueue(sc, &sc->cq, (void *)&cmd);

	/*
	 * FUNC_IOFENCE_WSI does not seem to be implemented in QEMU,
	 * so ensure all requests are processed in polling mode;
	 */
	do {
		q->lc.head = RD4(sc, q->head_off);
	} while (riscv_iommu_q_empty(q) == 0);
}

static int
riscv_iommu_inval_ddt(struct riscv_iommu_softc *sc)
{
	struct riscv_iommu_command cmd;
	uint64_t reg;

	bzero(&cmd, sizeof(struct riscv_iommu_command));
	reg = COMMAND_OPCODE_IODIR;
	reg |= FUNC_IODIR_INVAL_DDT;
	cmd.dword0 = reg;

	riscv_iommu_enqueue(sc, &sc->cq, (void *)&cmd);

	return (0);
}

static int
riscv_iommu_inval_ddt_did(struct riscv_iommu_softc *sc, int did)
{
	struct riscv_iommu_command cmd;
	uint64_t reg;

	bzero(&cmd, sizeof(struct riscv_iommu_command));
	reg = COMMAND_OPCODE_IODIR;
	reg |= FUNC_IODIR_INVAL_DDT;
	reg |= FUNC_IODIR_DV;
	reg |= (uint64_t)did << FUNC_IODIR_DID_S;
	cmd.dword0 = reg;

	riscv_iommu_enqueue(sc, &sc->cq, (void *)&cmd);

	return (0);
}

/* Invalidate entire address space. */
static int
riscv_iommu_inval_vma(struct riscv_iommu_softc *sc)
{
	struct riscv_iommu_command cmd;
	uint64_t reg;

	bzero(&cmd, sizeof(struct riscv_iommu_command));
	reg = COMMAND_OPCODE_IOTINVAL;
	reg |= FUNC_IOTINVAL_VMA;
	cmd.dword0 = reg;

	riscv_iommu_enqueue(sc, &sc->cq, (void *)&cmd);

	return (0);
}

static int
riscv_iommu_inval_vma_page(struct riscv_iommu_softc *sc, vm_offset_t addr,
    int pscid)
{
	struct riscv_iommu_command cmd;
	uint64_t reg;

	bzero(&cmd, sizeof(struct riscv_iommu_command));
	reg = COMMAND_OPCODE_IOTINVAL;
	reg |= FUNC_IOTINVAL_VMA;
	reg |= FUNC_IOTINVAL_AV;
	reg |= FUNC_IOTINVAL_PSCV;
	reg |= pscid << FUNC_IOTINVAL_PSCID_S;
	cmd.dword0 = reg;
	cmd.dword1 = PHYS_TO_PPN(addr) << FUNC_IOTINVAL_ADDR_S;

	riscv_iommu_enqueue(sc, &sc->cq, (void *)&cmd);

	return (0);
}

static int
riscv_iommu_inval_vma_pscid(struct riscv_iommu_softc *sc, int pscid)
{
	struct riscv_iommu_command cmd;
	uint64_t reg;

	bzero(&cmd, sizeof(struct riscv_iommu_command));
	reg = COMMAND_OPCODE_IOTINVAL;
	reg |= FUNC_IOTINVAL_VMA;
	reg |= FUNC_IOTINVAL_PSCV;
	reg |= pscid << FUNC_IOTINVAL_PSCID_S;
	cmd.dword0 = reg;

	riscv_iommu_enqueue(sc, &sc->cq, (void *)&cmd);

	return (0);
}

static int
riscv_iommu_set_mode(struct riscv_iommu_softc *sc)
{
	struct riscv_iommu_ddt *ddt;
	uint64_t reg;
	uint64_t base;

	reg = RD8(sc, RISCV_IOMMU_DDTP);
	if (reg & DDTP_BUSY)
		return (ENXIO);

	ddt = &sc->ddt;
	base = ddt->base | (sc->iommu_mode << DDTP_IOMMU_MODE_S);
	WR8(sc, RISCV_IOMMU_DDTP, base);

	reg = RD8(sc, RISCV_IOMMU_DDTP);
	if (reg != base) {
		device_printf(sc->dev, "could not set mode\n");
		return (ENXIO);
	}

	riscv_iommu_inval_ddt(sc);
	riscv_iommu_inval_vma(sc);

	return (0);
}

static int
riscv_iommu_enable_queue(struct riscv_iommu_softc *sc,
    struct riscv_iommu_queue *q)
{
	uint32_t reg;
	int timeout;

	if (q == &sc->cq)
		WR4(sc, QUEUE_TAIL(q), 0);
	else
		WR4(sc, QUEUE_HEAD(q), 0);

	reg = CQCSR_CQEN | CQCSR_CIE | CQCSR_CQMF;
	WR4(sc, q->csr, reg);

	timeout = 1000;
	do {
		reg = RD4(sc, RISCV_IOMMU_CQCSR);
		if ((reg & CQCSR_BUSY) == 0)
			break;
		DELAY(10);
	} while (timeout--);

	if (timeout <= 0) {
		device_printf(sc->dev, "could not enable command queue\n");
		return (-1);
	}

	if ((reg & CQCSR_CQON) == 0) {
		device_printf(sc->dev, "could not activate command queue\n");
		return (-1);
	}

	/* RW1C interrupt pending bit. */
	WR4(sc, RISCV_IOMMU_IPSR, QUEUE_IPSR(q));

	return (0);
}

static int
riscv_iommu_init_queue(struct riscv_iommu_softc *sc,
    struct riscv_iommu_queue *q, uint64_t base, uint32_t dwords)
{
	uint64_t reg;
	int sz;

	q->entry_size = dwords * 8;
	sz = (1 << q->size_log2) * q->entry_size;

	/* Set up the command circular buffer */
	q->vaddr = contigmalloc(sz, M_IOMMU, M_WAITOK | M_ZERO, 0,
	    (1ul << 48) - 1, QUEUE_ALIGN, 0);
	if (q->vaddr == NULL) {
		device_printf(sc->dev, "failed to allocate %d bytes\n", sz);
		return (-1);
	}

	q->mask = (1 << q->size_log2) - 1;
	q->head_off = (uint32_t)base - RISCV_IOMMU_CQB + RISCV_IOMMU_CQH;
	q->tail_off = (uint32_t)base - RISCV_IOMMU_CQB + RISCV_IOMMU_CQT;
	q->paddr = vtophys(q->vaddr);
	q->base = (sc->cq.size_log2 - 1) << CQB_LOG2SZ_1_S;
	q->base |= PHYS_TO_PPN(q->paddr) << CQB_PPN_S;
	WR8(sc, base, q->base);

	/* Verify it sticks. */
	reg = RD8(sc, base);
	if (reg != q->base) {
		device_printf(sc->dev, "could not init queue\n");
		return (ENXIO);
	}

	return (0);
}

static int
riscv_iommu_init_queues(struct riscv_iommu_softc *sc)
{
	int error;

	sc->cq.size_log2 = ilog2(CQ_ENTRY_COUNT);
	sc->fq.size_log2 = ilog2(FQ_ENTRY_COUNT);
	sc->pq.size_log2 = ilog2(PQ_ENTRY_COUNT);

	sc->cq.csr = RISCV_IOMMU_CQCSR;
	sc->fq.csr = RISCV_IOMMU_FQCSR;
	sc->pq.csr = RISCV_IOMMU_PQCSR;

	sc->cq.idx = 0;
	sc->fq.idx = 1;
	sc->pq.idx = 3;

	/* Command queue (CQ). */
	error = riscv_iommu_init_queue(sc, &sc->cq, RISCV_IOMMU_CQB,
	    CQ_ENTRY_DWORDS);
	if (error)
		return (error);

	/* Fault queue (FQ). */
	error = riscv_iommu_init_queue(sc, &sc->fq, RISCV_IOMMU_FQB,
	    FQ_ENTRY_DWORDS);
	if (error)
		return (error);

	/* Page request queue (PQ). */
	error = riscv_iommu_init_queue(sc, &sc->pq, RISCV_IOMMU_PQB,
	    PQ_ENTRY_DWORDS);
	if (error)
		return (error);

	error = riscv_iommu_enable_queue(sc, &sc->cq);
	if (error)
		return (error);

	error = riscv_iommu_enable_queue(sc, &sc->fq);
	if (error)
		return (error);

	error = riscv_iommu_enable_queue(sc, &sc->pq);
	if (error)
		return (error);

	return (0);
}

static int
riscv_iommu_init_pagedir(struct riscv_iommu_softc *sc)
{

	return (0);
}

static void
riscv_iommu_print_fault(struct riscv_iommu_softc *sc,
    struct riscv_iommu_fq_record *rec)
{
	struct riscv_iommu_fq_event *ev;
	uint16_t cause_id;
	uint16_t ttyp;
	uint32_t did;
	uint32_t pid;
	bool pv, priv;
	int i;

	cause_id = (rec->hdr & FQR_HDR_CAUSE_M) >> FQR_HDR_CAUSE_S;
	ttyp = (rec->hdr & FQR_HDR_TTYP_M) >> FQR_HDR_TTYP_S;
	did = (rec->hdr & FQR_HDR_DID_M) >> FQR_HDR_DID_S;
	pid = (rec->hdr & FQR_HDR_PID_M) >> FQR_HDR_PID_S;
	pv = (rec->hdr & FQR_HDR_PV) ? 1 : 0;
	priv = (rec->hdr & FQR_HDR_PRIV) ? 1 : 0;

	ev = NULL;
	for (i = 0; fq_events[i].cause_id != 0; i++) {
		if (fq_events[i].cause_id == cause_id) {
			ev = &fq_events[i];
			break;
		}
	}

	if (ev == NULL) {
		device_printf(sc->dev, "Fault: unknown fault 0x%x received\n",
		    cause_id);
		return;
	}

	device_printf(sc->dev, "Fault: event 0x%x received: %s\n",
	    ev->cause_id, ev->descr);
	device_printf(sc->dev, "    hdr 0x%lx\n", rec->hdr);
	device_printf(sc->dev, "    iotval 0x%lx\n", rec->iotval);
	device_printf(sc->dev, "    iotval2 0x%lx\n", rec->iotval2);
	device_printf(sc->dev, "    ttyp 0x%x did 0x%x pid 0x%x pv %d priv %d"
	    "\n", ttyp, did, pid, pv, priv);
}

static int
riscv_cq_intr(void *arg)
{
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_queue *q;
	uint32_t reg;

	sc = arg;
	q = &sc->cq;

	reg = RD4(sc, q->csr);
	printf("%s: pending %x\n", __func__, reg);

	/* Clear pending bit. */
	WR4(sc, RISCV_IOMMU_IPSR, IPSR_CIP);

	return (FILTER_HANDLED);
}

static int
riscv_fq_intr(void *arg)
{
	struct riscv_iommu_fq_record rec;
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_queue *q;
	uint32_t reg;

	sc = arg;
	q = &sc->fq;

	reg = RD4(sc, q->csr);
	printf("%s: pending %x\n", __func__, reg);

	/* Clear pending bit. */
	WR4(sc, RISCV_IOMMU_IPSR, IPSR_FIP);

	do {
		riscv_iommu_dequeue(sc, q, &rec);
		riscv_iommu_print_fault(sc, &rec);
	} while (!riscv_iommu_q_empty(q));

	return (FILTER_HANDLED);
}

static int
riscv_pm_intr(void *arg)
{
	struct riscv_iommu_softc *sc;

	sc = arg;

	printf("%s\n", __func__);

	/* Clear pending bit. */
	WR4(sc, RISCV_IOMMU_IPSR, IPSR_PMIP);

	return (FILTER_HANDLED);
}

static int
riscv_pq_intr(void *arg)
{
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_queue *q;
	uint32_t reg;

	sc = arg;
	q = &sc->pq;

	reg = RD4(sc, q->csr);
	printf("%s: pending %x\n", __func__, reg);

	/* Clear pending bit. */
	WR4(sc, RISCV_IOMMU_IPSR, IPSR_PIP);

	return (FILTER_HANDLED);
}

static int
riscv_iommu_init_ddt_linear(struct riscv_iommu_softc *sc)
{
	struct riscv_iommu_ddt *ddt;
	uint64_t size;
	uint64_t reg;

	ddt = &sc->ddt;
	ddt->num_top_entries = (1 << sc->l0_did_bits);

	size = ddt->num_top_entries * (sc->dc_dwords << 3);

	if (bootverbose)
		device_printf(sc->dev, "linear ddt size %ld, num_top_entries "
		    "%d\n", size, ddt->num_top_entries);

	ddt->vaddr = contigmalloc(size, M_IOMMU,
	    M_WAITOK | M_ZERO,	/* flags */
	    0,			/* low */
	    (1ul << 48) - 1,	/* high */
	    size,		/* alignment */
	    0);			/* boundary */
	if (ddt->vaddr == NULL) {
		device_printf(sc->dev, "failed to allocate ddt\n");
		return (ENXIO);
	}

	reg = vtophys(ddt->vaddr);
	if (bootverbose)
		device_printf(sc->dev, "ddt base %p size %lx\n", ddt->vaddr,
		    size);
	ddt->base = PHYS_TO_PPN(reg) << DDTP_PPN_S;

	return (0);
}

static int
riscv_iommu_init_ddt_2lvl(struct riscv_iommu_softc *sc)
{
	struct riscv_iommu_ddt *ddt;
	uint64_t size;
	uint64_t reg;
	uint64_t sz;

	ddt = &sc->ddt;
	ddt->num_top_entries = (1 << DDT_L1_DID_BITS);

	size = ddt->num_top_entries * (DDT_NON_LEAF_DWORDS << 3);

	if (bootverbose)
		device_printf(sc->dev, "%s: size %lu, l1 entries %d, size "
		    "%lu\n", __func__, size, ddt->num_top_entries, size);

	ddt->vaddr = contigmalloc(size, M_IOMMU,
	    M_WAITOK | M_ZERO,	/* flags */
	    0,			/* low */
	    (1ul << 48) - 1,	/* high */
	    size,		/* alignment */
	    0);			/* boundary */
	if (ddt->vaddr == NULL) {
		device_printf(sc->dev, "Failed to allocate 2lvl ddt.\n");
		return (ENOMEM);
	}

	sz = ddt->num_top_entries * sizeof(struct l1_desc);
	ddt->l1 = malloc(sz, M_IOMMU, M_WAITOK | M_ZERO);

	reg = vtophys(ddt->vaddr);
	if (bootverbose)
		device_printf(sc->dev, "ddt base %p size %lx\n", ddt->vaddr,
		    size);
	ddt->base = PHYS_TO_PPN(reg) << DDTP_PPN_S;

	return (0);
}

static int
riscv_iommu_init_l0_directory(struct riscv_iommu_softc *sc, int sid)
{
	struct riscv_iommu_ddt *ddt;
	struct l1_desc *l1_desc;
	uint64_t *l1e;
	uint64_t val;
	size_t size;
	int i;

	ddt = &sc->ddt;
	l1_desc = &ddt->l1[sid >> sc->l0_did_bits];
	if (l1_desc->va) {
		/* Already allocated. */
		return (0);
	}

	size = (1 << sc->l0_did_bits) * (sc->dc_dwords << 3);

	l1_desc->va = contigmalloc(size, M_IOMMU,
	    M_WAITOK | M_ZERO,	/* flags */
	    0,			/* low */
	    (1ul << 48) - 1,	/* high */
	    size,		/* alignment */
	    0);			/* boundary */
	if (l1_desc->va == NULL) {
		device_printf(sc->dev, "failed to allocate l0 directory\n");
		return (ENXIO);
	}

	l1_desc->pa = vtophys(l1_desc->va);

	i = sid >> sc->l0_did_bits;
	l1e = (void *)((uint64_t)ddt->vaddr + DDT_NON_LEAF_DWORDS * 8 * i);

	/* Install the L1 entry. */
	val = PHYS_TO_PPN(l1_desc->pa) << DC_NON_LEAF_ENTRY_PPN_S;
	val |= DC_NON_LEAF_ENTRY_VALID;
	*l1e = val;

	return (0);
}

static void *
riscv_iommu_get_dc_addr(struct riscv_iommu_softc *sc, int did)
{
	struct riscv_iommu_ddt *ddt;
	struct l1_desc *l1_desc;
	uintptr_t l0_base;
	void *addr;
	int l0_offs;
	int l1_idx;

	ddt = &sc->ddt;

	l0_offs = sc->dc_dwords * 8 * (did & ((1 << sc->l0_did_bits) - 1));

	if (sc->iommu_mode == DDTP_IOMMU_MODE_2LVL) {
		l1_idx = (did >> sc->l0_did_bits) &
		    ((1 << DDT_L1_DID_BITS) - 1);
		l1_desc = &ddt->l1[l1_idx];
		l0_base = (uintptr_t)l1_desc->va;
	} else
		l0_base = (uintptr_t)ddt->vaddr;

	addr = (void *)(l0_base + l0_offs);

	dprintf("ddt vaddr %p addr %p\n", ddt->vaddr, addr);

	return (addr);
}

static int
riscv_iommu_init_dc(struct riscv_iommu_softc *sc,
    struct riscv_iommu_domain *domain, int did, bool bypass)
{
	struct riscv_iommu_dc_base *dc_base;
	struct riscv_iommu_dc *dc;
	struct riscv_iommu_pmap *p;

	dc = riscv_iommu_get_dc_addr(sc, did);
	dc_base = &dc->base;

	device_printf(sc->dev, "address translation for device id"
	    " 0x%x is %s.\n", did, bypass ? "bypassed" : "enabled");

	p = &domain->p;

	bzero(dc_base, sizeof(struct riscv_iommu_dc_base));
	if (bypass == false)
		dc_base->fsc = p->pm_satp;
	dc_base->ta = (domain->pscid << DC_TA_PSCID_S) | DC_TA_V;

	riscv_iommu_inval_ddt_did(sc, did);
	riscv_iommu_sync(sc, &sc->cq);
	dc_base->tc |= DC_TC_V;
	riscv_iommu_inval_ddt_did(sc, did);
	riscv_iommu_inval_vma(sc);
	riscv_iommu_sync(sc, &sc->cq);

	return (0);
}

static void
riscv_iommu_deinit_dc(struct riscv_iommu_softc *sc, int did)
{
	struct riscv_iommu_dc_base *dc_base;
	struct riscv_iommu_dc *dc;

	dc = riscv_iommu_get_dc_addr(sc, did);
	dc_base = &dc->base;
	dc_base->tc &= ~DC_TC_V;

	riscv_iommu_inval_ddt_did(sc, did);
	riscv_iommu_sync(sc, &sc->cq);
}

static int
riscv_iommu_setup_interrupts(struct riscv_iommu_softc *sc)
{
	device_t dev;
	int error;

	dev = sc->dev;

	if (sc->res[1] == NULL || sc->res[2] == NULL ||
	    sc->res[3] == NULL || sc->res[4] == NULL) {
		device_printf(dev, "Warning: no interrupt resources "
		    "provided.\n");
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_MISC,
	    riscv_cq_intr, NULL, sc, &sc->intr_cookie[0]);
	if (error) {
		device_printf(dev, "Couldn't setup cq interrupt handler\n");
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->res[2], INTR_TYPE_MISC,
	    riscv_fq_intr, NULL, sc, &sc->intr_cookie[1]);
	if (error) {
		device_printf(dev, "Couldn't setup fq interrupt handler\n");
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->res[3], INTR_TYPE_MISC,
	    riscv_pm_intr, NULL, sc, &sc->intr_cookie[2]);
	if (error) {
		device_printf(dev, "Couldn't setup pm interrupt handler\n");
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->res[4], INTR_TYPE_MISC,
	    riscv_pq_intr, NULL, sc, &sc->intr_cookie[3]);
	if (error) {
		device_printf(dev, "Couldn't setup pq interrupt handler\n");
		return (ENXIO);
	}

	WR8(sc, RISCV_IOMMU_ICVEC, 0 << 0 | 1 << 4 | 2 << 8 | 3 << 12);

	return (0);
}

int
riscv_iommu_attach(device_t dev)
{
	struct riscv_iommu_softc *sc;
	uint64_t caps;
	int error;

	sc = device_get_softc(dev);

	caps = bus_read_8(sc->res[0], RISCV_IOMMU_CAPABILITIES);
	if (bootverbose)
		device_printf(sc->dev, "IOMMU Capabilities: %lx\n", caps);

	device_printf(sc->dev, "Device-Context structure is %s.\n",
	    caps & CAPABILITIES_MSI_FLAT ?
	    "64-bytes (ext format)" : "32-bytes (std format)");

	if (caps & CAPABILITIES_MSI_FLAT) {
		sc->dc_dwords = DDT_DC_EXT_DWORDS;
		sc->l0_did_bits = 6;
	} else {
		sc->dc_dwords = DDT_DC_STD_DWORDS;
		sc->l0_did_bits = 7;
	}

	if (caps & CAPABILITIES_SV48)
		sc->pm_mode = PMAP_MODE_SV48;
	else if (caps & CAPABILITIES_SV39)
		sc->pm_mode = PMAP_MODE_SV39;
	else {
		device_printf(sc->dev, "Unsupported virtual memory system\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), "riscv_iommu",
	    MTX_DEF);

	WR4(sc, RISCV_IOMMU_FCTL, FCTL_WSI);

	error = riscv_iommu_setup_interrupts(sc);
	if (error) {
		device_printf(sc->dev, "Could not setup interrupts. "
		    "Continuing with no interrupts support.");
	}

	error = riscv_iommu_init_pagedir(sc);
	if (error)
		return (error);

	error = riscv_iommu_init_queues(sc);
	if (error)
		return (error);

	sc->iommu_mode = DDTP_IOMMU_MODE_2LVL;

	switch (sc->iommu_mode) {
	case DDTP_IOMMU_MODE_1LVL:
		error = riscv_iommu_init_ddt_linear(sc);
		break;
	case DDTP_IOMMU_MODE_2LVL:
		error = riscv_iommu_init_ddt_2lvl(sc);
		break;
	default:
		error = ENXIO;
	}
	if (error)
		return (error);

	sc->pscid_bits = 8;

	riscv_iommu_init_pscids(sc);
	if (error)
		return (error);

	error = riscv_iommu_set_mode(sc);
	if (error)
		return (error);

	return (0);
}

static int
riscv_iommu_set_buswide(device_t dev, struct riscv_iommu_domain *domain,
    struct riscv_iommu_ctx *ctx)
{
	struct riscv_iommu_softc *sc;
	int i;

	sc = device_get_softc(dev);

	printf("%s\n", __func__);

	for (i = 0; i < PCI_SLOTMAX; i++)
		riscv_iommu_init_dc(sc, domain, (ctx->did | i),
		    ctx->bypass);

	return (0);
}

static int
riscv_iommu_pci_get_did(device_t child, uintptr_t *xref0, u_int *did0)
{
	struct pci_id_ofw_iommu pi;
	int err;

	dprintf("%s\n", __func__);

	err = pci_get_id(child, PCI_ID_OFW_IOMMU, (uintptr_t *)&pi);
	if (err == 0) {
		if (did0)
			*did0 = pi.id;
		if (xref0)
			*xref0 = pi.xref;
	}

	return (err);
}

static int
riscv_iommu_find(device_t dev, device_t child)
{
	struct riscv_iommu_softc *sc;
	uintptr_t xref;
	int err;

	dprintf("%s\n", __func__);

	sc = device_get_softc(dev);

	err = riscv_iommu_pci_get_did(child, &xref, NULL);
	if (err)
		return (ENOENT);

	/* Check if xref is ours. */
	dprintf("xref %lx sc->xref %lx\n", xref, sc->xref);
	if (xref != sc->xref)
		return (EFAULT);

	return (0);
}

struct riscv_iommu_ctx *
riscv_iommu_ctx_lookup_by_did(device_t dev, u_int did)
{
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_domain *domain;
	struct riscv_iommu_unit *unit;
	struct riscv_iommu_ctx *ctx;

	dprintf("%s\n", __func__);
	sc = device_get_softc(dev);

	unit = &sc->unit;

	LIST_FOREACH(domain, &unit->domain_list, next) {
		LIST_FOREACH(ctx, &domain->ctx_list, next) {
			if (ctx->did == did) {
				refcount_acquire(&ctx->refcnt);
				return (ctx);
			}
		}
	}

	return (NULL);
}

static struct iommu_ctx *
riscv_iommu_ctx_lookup(device_t dev, device_t child)
{
	struct iommu_unit *iommu __diagused;
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_domain *domain;
	struct riscv_iommu_unit *unit;
	struct riscv_iommu_ctx *ctx;

	dprintf("%s\n", __func__);
	sc = device_get_softc(dev);

	unit = &sc->unit;
	iommu = &unit->iommu;

	IOMMU_ASSERT_LOCKED(iommu);

	LIST_FOREACH(domain, &unit->domain_list, next) {
		IOMMU_DOMAIN_LOCK(&domain->iodom);
		LIST_FOREACH(ctx, &domain->ctx_list, next) {
			if (ctx->dev == child) {
				refcount_acquire(&ctx->refcnt);
				IOMMU_DOMAIN_UNLOCK(&domain->iodom);
				return (&ctx->ioctx);
			}
		}
		IOMMU_DOMAIN_UNLOCK(&domain->iodom);
	}

	return (NULL);
}

static int
riscv_iommu_unmap(device_t dev, struct iommu_domain *iodom,
    vm_offset_t va, bus_size_t size)
{
	struct riscv_iommu_domain *domain;
	struct riscv_iommu_softc *sc;
	int err;
	int i;

	sc = device_get_softc(dev);

	domain = (struct riscv_iommu_domain *)iodom;

	err = 0;

	dprintf("%s: %lx, %ld, domain %d\n", __func__, va, size, domain->pscid);

	for (i = 0; i < size; i += PAGE_SIZE) {
		if (iommu_pmap_remove(&domain->p, va) == 0) {
			/* pmap entry removed, invalidate TLB. */
			riscv_iommu_inval_vma_page(sc, va, domain->pscid);
		} else {
			err = ENOENT;
			break;
		}
		va += PAGE_SIZE;
	}

	riscv_iommu_sync(sc, &sc->cq);

	return (err);
}

static int
riscv_iommu_map(device_t dev, struct iommu_domain *iodom,
    vm_offset_t va, vm_page_t *ma, vm_size_t size,
    vm_prot_t prot)
{
	struct riscv_iommu_domain *domain;
	struct riscv_iommu_softc *sc;
	vm_paddr_t pa;
	int error;
	int i;

	sc = device_get_softc(dev);

	domain = (struct riscv_iommu_domain *)iodom;

	for (i = 0; size > 0; size -= PAGE_SIZE) {
		pa = VM_PAGE_TO_PHYS(ma[i++]);
		dprintf("%s: %lx -> %lx, %ld, domain %d\n", __func__, va, pa,
		    size, domain->pscid);
		error = iommu_pmap_enter(&domain->p, va, pa, prot, 0);
		if (error)
			return (error);
		riscv_iommu_inval_vma_page(sc, va, domain->pscid);
		va += PAGE_SIZE;
	}

	riscv_iommu_sync(sc, &sc->cq);

	return (0);
}

static struct iommu_domain *
riscv_iommu_domain_alloc(device_t dev, struct iommu_unit *iommu)
{
	struct iommu_domain *iodom;
	struct riscv_iommu_domain *domain;
	struct riscv_iommu_unit *unit;
	struct riscv_iommu_softc *sc;
	int new_pscid;
	int va_bits;
	int error;

	sc = device_get_softc(dev);

	dprintf("%s\n", __func__);

	unit = (struct riscv_iommu_unit *)iommu;

	error = riscv_iommu_pscid_alloc(sc, &new_pscid);
	if (error) {
		device_printf(sc->dev,
		   "Could not allocate PSCID for a new domain.\n");
		return (NULL);
	}

	domain = malloc(sizeof(*domain), M_IOMMU, M_WAITOK | M_ZERO);
	domain->pscid = (uint16_t)new_pscid;

	iommu_pmap_pinit(&domain->p, sc->pm_mode);

	riscv_iommu_inval_vma_pscid(sc, domain->pscid);

	LIST_INIT(&domain->ctx_list);

	IOMMU_LOCK(iommu);
	LIST_INSERT_HEAD(&unit->domain_list, domain, next);
	IOMMU_UNLOCK(iommu);

	iodom = &domain->iodom;

	va_bits = sc->pm_mode == PMAP_MODE_SV48 ? 48 : 39;

	/* Avoid sign-extension. */
	va_bits -= 1;

	iodom->end = (1ULL << va_bits) - 1;

	return (iodom);
}

static void
riscv_iommu_domain_free(device_t dev, struct iommu_domain *iodom)
{
	struct riscv_iommu_domain *domain;
	struct riscv_iommu_softc *sc;

	sc = device_get_softc(dev);

	dprintf("%s\n", __func__);

	domain = (struct riscv_iommu_domain *)iodom;

	LIST_REMOVE(domain, next);

	iommu_pmap_remove_pages(&domain->p);
	iommu_pmap_release(&domain->p);

	riscv_iommu_inval_vma_pscid(sc, domain->pscid);
	riscv_iommu_pscid_free(sc, domain->pscid);

	free(domain, M_IOMMU);
}

static struct iommu_ctx *
riscv_iommu_ctx_alloc(device_t dev, struct iommu_domain *iodom, device_t child,
    bool disabled)
{
	struct riscv_iommu_domain *domain;
	struct riscv_iommu_ctx *ctx;

	dprintf("%s\n", __func__);

	domain = (struct riscv_iommu_domain *)iodom;

	ctx = malloc(sizeof(struct riscv_iommu_ctx), M_IOMMU,
	    M_WAITOK | M_ZERO);
	ctx->dev = child;
	ctx->domain = domain;
	refcount_init(&ctx->refcnt, 1);
	if (disabled)
		ctx->bypass = true;

	IOMMU_DOMAIN_LOCK(iodom);
	LIST_INSERT_HEAD(&domain->ctx_list, ctx, next);
	IOMMU_DOMAIN_UNLOCK(iodom);

	return (&ctx->ioctx);
}

static int
riscv_iommu_ctx_init(device_t dev, struct iommu_ctx *ioctx)
{
	struct riscv_iommu_domain *domain;
	struct iommu_domain *iodom;
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_ctx *ctx;
	devclass_t pci_class;
	u_int did;
	int error;

	ctx = (struct riscv_iommu_ctx *)ioctx;

	dprintf("%s\n", __func__);

	sc = device_get_softc(dev);

	domain = ctx->domain;
	iodom = (struct iommu_domain *)domain;

	pci_class = devclass_find("pci");
	if (device_get_devclass(device_get_parent(ctx->dev)) == pci_class) {
		error = riscv_iommu_pci_get_did(ctx->dev, NULL, &did);
		if (error)
			return (error);

		ioctx->rid = pci_get_rid(dev);
		ctx->did = did;
		ctx->vendor = pci_get_vendor(ctx->dev);
		ctx->device = pci_get_device(ctx->dev);
	}

	if (sc->iommu_mode == DDTP_IOMMU_MODE_2LVL) {
		error = riscv_iommu_init_l0_directory(sc, ctx->did);
		if (error)
			return (error);
	}
	riscv_iommu_init_dc(sc, domain, ctx->did, ctx->bypass);

	if (device_get_devclass(device_get_parent(ctx->dev)) == pci_class)
		if (iommu_is_buswide_ctx(iodom->iommu, pci_get_bus(ctx->dev)))
			riscv_iommu_set_buswide(dev, domain, ctx);

	return (0);
}

static bool
riscv_iommu_ctx_free(device_t dev, struct iommu_ctx *ioctx)
{
	struct riscv_iommu_softc *sc;
	struct riscv_iommu_ctx *ctx;

	dprintf("%s\n", __func__);

	IOMMU_ASSERT_LOCKED(ioctx->domain->iommu);

	sc = device_get_softc(dev);

	ctx = (struct riscv_iommu_ctx *)ioctx;
	if (refcount_release(&ctx->refcnt)) {
		riscv_iommu_deinit_dc(sc, ctx->did);
		LIST_REMOVE(ctx, next);
		free(ctx, M_IOMMU);
		return (true);
	}

	return (false);
}

#ifdef FDT
static int
riscv_iommu_ofw_md_data(device_t dev, struct iommu_ctx *ioctx, pcell_t *cells,
    int ncells)
{
	struct riscv_iommu_ctx *ctx;

	printf("%s\n", __func__);
	ctx = (struct riscv_iommu_ctx *)ioctx;

	if (ncells != 1)
		return (-1);

	ctx->did = cells[0];

	return (0);
}
#endif

static int
riscv_iommu_read_ivar(device_t dev, device_t child, int which,
    uintptr_t *result)
{
	struct riscv_iommu_softc *sc;

	sc = device_get_softc(dev);

	device_printf(sc->dev, "%s\n", __func__);

	return (ENOENT);
}

static device_method_t riscv_iommu_methods[] = {
	/* IOMMU interface */
	DEVMETHOD(iommu_find,		riscv_iommu_find),
	DEVMETHOD(iommu_map,		riscv_iommu_map),
	DEVMETHOD(iommu_unmap,		riscv_iommu_unmap),
	DEVMETHOD(iommu_domain_alloc,	riscv_iommu_domain_alloc),
	DEVMETHOD(iommu_domain_free,	riscv_iommu_domain_free),
	DEVMETHOD(iommu_ctx_alloc,	riscv_iommu_ctx_alloc),
	DEVMETHOD(iommu_ctx_init,	riscv_iommu_ctx_init),
	DEVMETHOD(iommu_ctx_free,	riscv_iommu_ctx_free),
	DEVMETHOD(iommu_ctx_lookup,	riscv_iommu_ctx_lookup),
#ifdef FDT
	DEVMETHOD(iommu_ofw_md_data,	riscv_iommu_ofw_md_data),
#endif

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	riscv_iommu_read_ivar),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(riscv_iommu, riscv_iommu_driver, riscv_iommu_methods,
    sizeof(struct riscv_iommu_softc));
