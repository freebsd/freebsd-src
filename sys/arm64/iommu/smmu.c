/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

/*
 * Hardware overview.
 *
 * An incoming transaction from a peripheral device has an address, size,
 * attributes and StreamID.
 *
 * In case of PCI-based devices, StreamID is a PCI rid.
 *
 * The StreamID is used to select a Stream Table Entry (STE) in a Stream table,
 * which contains per-device configuration.
 *
 * Stream table is a linear or 2-level walk table (this driver supports both).
 * Note that a linear table could occupy 1GB or more of memory depending on
 * sid_bits value.
 *
 * STE is used to locate a Context Descriptor, which is a struct in memory
 * that describes stages of translation, translation table type, pointer to
 * level 0 of page tables, ASID, etc.
 *
 * Hardware supports two stages of translation: Stage1 (S1) and Stage2 (S2):
 *  o S1 is used for the host machine traffic translation
 *  o S2 is for a hypervisor
 *
 * This driver enables S1 stage with standard AArch64 page tables.
 *
 * Note that SMMU does not share TLB with a main CPU.
 * Command queue is used by this driver to Invalidate SMMU TLB, STE cache.
 *
 * An arm64 SoC could have more than one SMMU instance.
 * ACPI IORT table describes which SMMU unit is assigned for a particular
 * peripheral device.
 *
 * Queues.
 *
 * Register interface and Memory-based circular buffer queues are used
 * to interface SMMU.
 *
 * These are a Command queue for commands to send to the SMMU and an Event
 * queue for event/fault reports from the SMMU. Optionally PRI queue is
 * designed for PCIe page requests reception.
 *
 * Note that not every hardware supports PRI services. For instance they were
 * not found in Neoverse N1 SDP machine.
 * (This drivers does not implement PRI queue.)
 *
 * All SMMU queues are arranged as circular buffers in memory. They are used
 * in a producer-consumer fashion so that an output queue contains data
 * produced by the SMMU and consumed by software.
 * An input queue contains data produced by software, consumed by the SMMU.
 *
 * Interrupts.
 *
 * Interrupts are not required by this driver for normal operation.
 * The standard wired interrupt is only triggered when an event comes from
 * the SMMU, which is only in a case of errors (e.g. translation fault).
 */

#include "opt_platform.h"
#include "opt_acpi.h"

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
#include <vm/vm.h>
#include <vm/vm_page.h>
#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/iommu/iommu.h>
#include <arm64/iommu/iommu_pmap.h>

#include <machine/bus.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "iommu.h"
#include "iommu_if.h"

#include "smmureg.h"
#include "smmuvar.h"

#define	STRTAB_L1_SZ_SHIFT	20
#define	STRTAB_SPLIT		8

#define	STRTAB_L1_DESC_L2PTR_M	(0x3fffffffffff << 6)
#define	STRTAB_L1_DESC_DWORDS	1

#define	STRTAB_STE_DWORDS	8

#define	CMDQ_ENTRY_DWORDS	2
#define	EVTQ_ENTRY_DWORDS	4
#define	PRIQ_ENTRY_DWORDS	2

#define	CD_DWORDS		8

#define	Q_WRP(q, p)		((p) & (1 << (q)->size_log2))
#define	Q_IDX(q, p)		((p) & ((1 << (q)->size_log2) - 1))
#define	Q_OVF(p)		((p) & (1 << 31)) /* Event queue overflowed */

#define	SMMU_Q_ALIGN		(64 * 1024)

#define		MAXADDR_48BIT	0xFFFFFFFFFFFFUL
#define		MAXADDR_52BIT	0xFFFFFFFFFFFFFUL

static struct resource_spec smmu_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ SYS_RES_IRQ, 0, RF_ACTIVE },
	{ SYS_RES_IRQ, 1, RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ, 2, RF_ACTIVE },
	{ SYS_RES_IRQ, 3, RF_ACTIVE },
	RESOURCE_SPEC_END
};

MALLOC_DEFINE(M_SMMU, "SMMU", SMMU_DEVSTR);

#define	dprintf(fmt, ...)

struct smmu_event {
	int ident;
	char *str;
	char *msg;
};

static struct smmu_event events[] = {
	{ 0x01, "F_UUT",
		"Unsupported Upstream Transaction."},
	{ 0x02, "C_BAD_STREAMID",
		"Transaction StreamID out of range."},
	{ 0x03, "F_STE_FETCH",
		"Fetch of STE caused external abort."},
	{ 0x04, "C_BAD_STE",
		"Used STE invalid."},
	{ 0x05, "F_BAD_ATS_TREQ",
		"Address Translation Request disallowed for a StreamID "
		"and a PCIe ATS Translation Request received."},
	{ 0x06, "F_STREAM_DISABLED",
		"The STE of a transaction marks non-substream transactions "
		"disabled."},
	{ 0x07, "F_TRANSL_FORBIDDEN",
		"An incoming PCIe transaction is marked Translated but "
		"SMMU bypass is disallowed for this StreamID."},
	{ 0x08, "C_BAD_SUBSTREAMID",
		"Incoming SubstreamID present, but configuration is invalid."},
	{ 0x09, "F_CD_FETCH",
		"Fetch of CD caused external abort."},
	{ 0x0a, "C_BAD_CD",
		"Fetched CD invalid."},
	{ 0x0b, "F_WALK_EABT",
		"An external abort occurred fetching (or updating) "
		"a translation table descriptor."},
	{ 0x10, "F_TRANSLATION",
		"Translation fault."},
	{ 0x11, "F_ADDR_SIZE",
		"Address Size fault."},
	{ 0x12, "F_ACCESS",
		"Access flag fault due to AF == 0 in a page or block TTD."},
	{ 0x13, "F_PERMISSION",
		"Permission fault occurred on page access."},
	{ 0x20, "F_TLB_CONFLICT",
		"A TLB conflict occurred because of the transaction."},
	{ 0x21, "F_CFG_CONFLICT",
		"A configuration cache conflict occurred due to "
		"the transaction."},
	{ 0x24, "E_PAGE_REQUEST",
		"Speculative page request hint."},
	{ 0x25, "F_VMS_FETCH",
		"Fetch of VMS caused external abort."},
	{ 0, NULL, NULL },
};

static int
smmu_q_has_space(struct smmu_queue *q)
{

	/*
	 * See 6.3.27 SMMU_CMDQ_PROD
	 *
	 * There is space in the queue for additional commands if:
	 *  SMMU_CMDQ_CONS.RD != SMMU_CMDQ_PROD.WR ||
	 *  SMMU_CMDQ_CONS.RD_WRAP == SMMU_CMDQ_PROD.WR_WRAP
	 */

	if (Q_IDX(q, q->lc.cons) != Q_IDX(q, q->lc.prod) ||
	    Q_WRP(q, q->lc.cons) == Q_WRP(q, q->lc.prod))
		return (1);

	return (0);
}

static int
smmu_q_empty(struct smmu_queue *q)
{

	if (Q_IDX(q, q->lc.cons) == Q_IDX(q, q->lc.prod) &&
	    Q_WRP(q, q->lc.cons) == Q_WRP(q, q->lc.prod))
		return (1);

	return (0);
}

static int __unused
smmu_q_consumed(struct smmu_queue *q, uint32_t prod)
{

	if ((Q_WRP(q, q->lc.cons) == Q_WRP(q, prod)) &&
	    (Q_IDX(q, q->lc.cons) >= Q_IDX(q, prod)))
		return (1);

	if ((Q_WRP(q, q->lc.cons) != Q_WRP(q, prod)) &&
	    (Q_IDX(q, q->lc.cons) <= Q_IDX(q, prod)))
		return (1);

	return (0);
}

static uint32_t
smmu_q_inc_cons(struct smmu_queue *q)
{
	uint32_t cons;
	uint32_t val;

	cons = (Q_WRP(q, q->lc.cons) | Q_IDX(q, q->lc.cons)) + 1;
	val = (Q_OVF(q->lc.cons) | Q_WRP(q, cons) | Q_IDX(q, cons));

	return (val);
}

static uint32_t
smmu_q_inc_prod(struct smmu_queue *q)
{
	uint32_t prod;
	uint32_t val;

	prod = (Q_WRP(q, q->lc.prod) | Q_IDX(q, q->lc.prod)) + 1;
	val = (Q_OVF(q->lc.prod) | Q_WRP(q, prod) | Q_IDX(q, prod));

	return (val);
}

static int
smmu_write_ack(struct smmu_softc *sc, uint32_t reg,
    uint32_t reg_ack, uint32_t val)
{
	uint32_t v;
	int timeout;

	timeout = 100000;

	bus_write_4(sc->res[0], reg, val);

	do {
		v = bus_read_4(sc->res[0], reg_ack);
		if (v == val)
			break;
	} while (timeout--);

	if (timeout <= 0) {
		device_printf(sc->dev, "Failed to write reg.\n");
		return (-1);
	}

	return (0);
}

static inline int
ilog2(long x)
{

	KASSERT(x > 0 && powerof2(x), ("%s: invalid arg %ld", __func__, x));

	return (flsl(x) - 1);
}

static int
smmu_init_queue(struct smmu_softc *sc, struct smmu_queue *q,
    uint32_t prod_off, uint32_t cons_off, uint32_t dwords)
{
	int sz;

	sz = (1 << q->size_log2) * dwords * 8;

	/* Set up the command circular buffer */
	q->vaddr = contigmalloc(sz, M_SMMU,
	    M_WAITOK | M_ZERO, 0, (1ul << 48) - 1, SMMU_Q_ALIGN, 0);
	if (q->vaddr == NULL) {
		device_printf(sc->dev, "failed to allocate %d bytes\n", sz);
		return (-1);
	}

	q->prod_off = prod_off;
	q->cons_off = cons_off;
	q->paddr = vtophys(q->vaddr);

	q->base = CMDQ_BASE_RA | EVENTQ_BASE_WA | PRIQ_BASE_WA;
	q->base |= q->paddr & Q_BASE_ADDR_M;
	q->base |= q->size_log2 << Q_LOG2SIZE_S;

	return (0);
}

static int
smmu_init_queues(struct smmu_softc *sc)
{
	int err;

	/* Command queue. */
	err = smmu_init_queue(sc, &sc->cmdq,
	    SMMU_CMDQ_PROD, SMMU_CMDQ_CONS, CMDQ_ENTRY_DWORDS);
	if (err)
		return (ENXIO);

	/* Event queue. */
	err = smmu_init_queue(sc, &sc->evtq,
	    SMMU_EVENTQ_PROD, SMMU_EVENTQ_CONS, EVTQ_ENTRY_DWORDS);
	if (err)
		return (ENXIO);

	if (!(sc->features & SMMU_FEATURE_PRI))
		return (0);

	/* PRI queue. */
	err = smmu_init_queue(sc, &sc->priq,
	    SMMU_PRIQ_PROD, SMMU_PRIQ_CONS, PRIQ_ENTRY_DWORDS);
	if (err)
		return (ENXIO);

	return (0);
}

/*
 * Dump 2LVL or linear STE.
 */
static void
smmu_dump_ste(struct smmu_softc *sc, int sid)
{
	struct smmu_strtab *strtab;
	struct l1_desc *l1_desc;
	uint64_t *ste, *l1;
	int i;

	strtab = &sc->strtab;

	if (sc->features & SMMU_FEATURE_2_LVL_STREAM_TABLE) {
		i = sid >> STRTAB_SPLIT;
		l1 = (void *)((uint64_t)strtab->vaddr +
		    STRTAB_L1_DESC_DWORDS * 8 * i);
		device_printf(sc->dev, "L1 ste == %lx\n", l1[0]);

		l1_desc = &strtab->l1[i];
		ste = l1_desc->va;
		if (ste == NULL) /* L2 is not initialized */
			return;
	} else {
		ste = (void *)((uint64_t)strtab->vaddr +
		    sid * (STRTAB_STE_DWORDS << 3));
	}

	/* Dump L2 or linear STE. */
	for (i = 0; i < STRTAB_STE_DWORDS; i++)
		device_printf(sc->dev, "ste[%d] == %lx\n", i, ste[i]);
}

static void __unused
smmu_dump_cd(struct smmu_softc *sc, struct smmu_cd *cd)
{
	uint64_t *vaddr;
	int i;

	device_printf(sc->dev, "%s\n", __func__);

	vaddr = cd->vaddr;
	for (i = 0; i < CD_DWORDS; i++)
		device_printf(sc->dev, "cd[%d] == %lx\n", i, vaddr[i]);
}

static void
smmu_evtq_dequeue(struct smmu_softc *sc, uint32_t *evt)
{
	struct smmu_queue *evtq;
	void *entry_addr;

	evtq = &sc->evtq;

	evtq->lc.val = bus_read_8(sc->res[0], evtq->prod_off);
	entry_addr = (void *)((uint64_t)evtq->vaddr +
	    evtq->lc.cons * EVTQ_ENTRY_DWORDS * 8);
	memcpy(evt, entry_addr, EVTQ_ENTRY_DWORDS * 8);
	evtq->lc.cons = smmu_q_inc_cons(evtq);
	bus_write_4(sc->res[0], evtq->cons_off, evtq->lc.cons);
}

static void
smmu_print_event(struct smmu_softc *sc, uint32_t *evt)
{
	struct smmu_event *ev;
	uintptr_t input_addr;
	uint8_t event_id;
	device_t dev;
	int sid;
	int i;

	dev = sc->dev;

	ev = NULL;
	event_id = evt[0] & 0xff;
	for (i = 0; events[i].ident != 0; i++) {
		if (events[i].ident == event_id) {
			ev = &events[i];
			break;
		}
	}

	sid = evt[1];
	input_addr = evt[5];
	input_addr <<= 32;
	input_addr |= evt[4];

	if (smmu_quirks_check(dev, sid, event_id, input_addr)) {
		/* The event is known. Don't print anything. */
		return;
	}

	if (ev) {
		device_printf(sc->dev,
		    "Event %s (%s) received.\n", ev->str, ev->msg);
	} else
		device_printf(sc->dev, "Event 0x%x received\n", event_id);

	device_printf(sc->dev, "SID %x, Input Address: %jx\n",
	    sid, input_addr);

	for (i = 0; i < 8; i++)
		device_printf(sc->dev, "evt[%d] %x\n", i, evt[i]);

	smmu_dump_ste(sc, sid);
}

static void
make_cmd(struct smmu_softc *sc, uint64_t *cmd,
    struct smmu_cmdq_entry *entry)
{

	memset(cmd, 0, CMDQ_ENTRY_DWORDS * 8);
	cmd[0] = entry->opcode << CMD_QUEUE_OPCODE_S;

	switch (entry->opcode) {
	case CMD_TLBI_NH_VA:
		cmd[0] |= (uint64_t)entry->tlbi.asid << TLBI_0_ASID_S;
		cmd[1] = entry->tlbi.addr & TLBI_1_ADDR_M;
		if (entry->tlbi.leaf) {
			/*
			 * Leaf flag means that only cached entries
			 * for the last level of translation table walk
			 * are required to be invalidated.
			 */
			cmd[1] |= TLBI_1_LEAF;
		}
		break;
	case CMD_TLBI_NH_ASID:
		cmd[0] |= (uint64_t)entry->tlbi.asid << TLBI_0_ASID_S;
		break;
	case CMD_TLBI_NSNH_ALL:
	case CMD_TLBI_NH_ALL:
	case CMD_TLBI_EL2_ALL:
		break;
	case CMD_CFGI_CD:
		cmd[0] |= ((uint64_t)entry->cfgi.ssid << CFGI_0_SSID_S);
		/* FALLTROUGH */
	case CMD_CFGI_STE:
		cmd[0] |= ((uint64_t)entry->cfgi.sid << CFGI_0_STE_SID_S);
		cmd[1] |= ((uint64_t)entry->cfgi.leaf << CFGI_1_LEAF_S);
		break;
	case CMD_CFGI_STE_RANGE:
		cmd[1] = (31 << CFGI_1_STE_RANGE_S);
		break;
	case CMD_SYNC:
		cmd[0] |= SYNC_0_MSH_IS | SYNC_0_MSIATTR_OIWB;
		if (entry->sync.msiaddr) {
			cmd[0] |= SYNC_0_CS_SIG_IRQ;
			cmd[1] |= (entry->sync.msiaddr & SYNC_1_MSIADDRESS_M);
		} else
			cmd[0] |= SYNC_0_CS_SIG_SEV;
		break;
	case CMD_PREFETCH_CONFIG:
		cmd[0] |= ((uint64_t)entry->prefetch.sid << PREFETCH_0_SID_S);
		break;
	};
}

static void
smmu_cmdq_enqueue_cmd(struct smmu_softc *sc, struct smmu_cmdq_entry *entry)
{
	uint64_t cmd[CMDQ_ENTRY_DWORDS];
	struct smmu_queue *cmdq;
	void *entry_addr;

	cmdq = &sc->cmdq;

	make_cmd(sc, cmd, entry);

	SMMU_LOCK(sc);

	/* Ensure that a space is available. */
	do {
		cmdq->lc.cons = bus_read_4(sc->res[0], cmdq->cons_off);
	} while (smmu_q_has_space(cmdq) == 0);

	/* Write the command to the current prod entry. */
	entry_addr = (void *)((uint64_t)cmdq->vaddr +
	    Q_IDX(cmdq, cmdq->lc.prod) * CMDQ_ENTRY_DWORDS * 8);
	memcpy(entry_addr, cmd, CMDQ_ENTRY_DWORDS * 8);

	/* Increment prod index. */
	cmdq->lc.prod = smmu_q_inc_prod(cmdq);
	bus_write_4(sc->res[0], cmdq->prod_off, cmdq->lc.prod);

	SMMU_UNLOCK(sc);
}

static void __unused
smmu_poll_until_consumed(struct smmu_softc *sc, struct smmu_queue *q)
{

	while (1) {
		q->lc.val = bus_read_8(sc->res[0], q->prod_off);
		if (smmu_q_empty(q))
			break;
		cpu_spinwait();
	}
}

static int
smmu_sync(struct smmu_softc *sc)
{
	struct smmu_cmdq_entry cmd;
	struct smmu_queue *q;
	uint32_t *base;
	int timeout;
	int prod;

	q = &sc->cmdq;
	prod = q->lc.prod;

	/* Enqueue sync command. */
	cmd.opcode = CMD_SYNC;
	cmd.sync.msiaddr = q->paddr + Q_IDX(q, prod) * CMDQ_ENTRY_DWORDS * 8;
	smmu_cmdq_enqueue_cmd(sc, &cmd);

	/* Wait for the sync completion. */
	base = (void *)((uint64_t)q->vaddr +
	    Q_IDX(q, prod) * CMDQ_ENTRY_DWORDS * 8);

	/*
	 * It takes around 200 loops (6 instructions each)
	 * on Neoverse N1 to complete the sync.
	 */
	timeout = 10000;

	do {
		if (*base == 0) {
			/* MSI write completed. */
			break;
		}
		cpu_spinwait();
	} while (timeout--);

	if (timeout < 0)
		device_printf(sc->dev, "Failed to sync\n");

	return (0);
}

static int
smmu_sync_cd(struct smmu_softc *sc, int sid, int ssid, bool leaf)
{
	struct smmu_cmdq_entry cmd;

	cmd.opcode = CMD_CFGI_CD;
	cmd.cfgi.sid = sid;
	cmd.cfgi.ssid = ssid;
	cmd.cfgi.leaf = leaf;
	smmu_cmdq_enqueue_cmd(sc, &cmd);

	return (0);
}

static void
smmu_invalidate_all_sid(struct smmu_softc *sc)
{
	struct smmu_cmdq_entry cmd;

	/* Invalidate cached config */
	cmd.opcode = CMD_CFGI_STE_RANGE;
	smmu_cmdq_enqueue_cmd(sc, &cmd);
	smmu_sync(sc);
}

static void
smmu_tlbi_all(struct smmu_softc *sc)
{
	struct smmu_cmdq_entry cmd;

	/* Invalidate entire TLB */
	cmd.opcode = CMD_TLBI_NSNH_ALL;
	smmu_cmdq_enqueue_cmd(sc, &cmd);
	smmu_sync(sc);
}

static void
smmu_tlbi_asid(struct smmu_softc *sc, uint16_t asid)
{
	struct smmu_cmdq_entry cmd;

	/* Invalidate TLB for an ASID. */
	cmd.opcode = CMD_TLBI_NH_ASID;
	cmd.tlbi.asid = asid;
	smmu_cmdq_enqueue_cmd(sc, &cmd);
	smmu_sync(sc);
}

static void
smmu_tlbi_va(struct smmu_softc *sc, vm_offset_t va, uint16_t asid)
{
	struct smmu_cmdq_entry cmd;

	/* Invalidate specific range */
	cmd.opcode = CMD_TLBI_NH_VA;
	cmd.tlbi.asid = asid;
	cmd.tlbi.vmid = 0;
	cmd.tlbi.leaf = true; /* We change only L3. */
	cmd.tlbi.addr = va;
	smmu_cmdq_enqueue_cmd(sc, &cmd);
}

static void
smmu_invalidate_sid(struct smmu_softc *sc, uint32_t sid)
{
	struct smmu_cmdq_entry cmd;

	/* Invalidate cached config */
	cmd.opcode = CMD_CFGI_STE;
	cmd.cfgi.sid = sid;
	smmu_cmdq_enqueue_cmd(sc, &cmd);
	smmu_sync(sc);
}

static void
smmu_prefetch_sid(struct smmu_softc *sc, uint32_t sid)
{
	struct smmu_cmdq_entry cmd;

	cmd.opcode = CMD_PREFETCH_CONFIG;
	cmd.prefetch.sid = sid;
	smmu_cmdq_enqueue_cmd(sc, &cmd);
	smmu_sync(sc);
}

/*
 * Init STE in bypass mode. Traffic is not translated for the sid.
 */
static void
smmu_init_ste_bypass(struct smmu_softc *sc, uint32_t sid, uint64_t *ste)
{
	uint64_t val;

	val = STE0_VALID | STE0_CONFIG_BYPASS;

	ste[1] = STE1_SHCFG_INCOMING | STE1_EATS_FULLATS;
	ste[2] = 0;
	ste[3] = 0;
	ste[4] = 0;
	ste[5] = 0;
	ste[6] = 0;
	ste[7] = 0;

	smmu_invalidate_sid(sc, sid);
	ste[0] = val;
	dsb(sy);
	smmu_invalidate_sid(sc, sid);

	smmu_prefetch_sid(sc, sid);
}

/*
 * Enable Stage1 (S1) translation for the sid.
 */
static int
smmu_init_ste_s1(struct smmu_softc *sc, struct smmu_cd *cd,
    uint32_t sid, uint64_t *ste)
{
	uint64_t val;

	val = STE0_VALID;

	/* S1 */
	ste[1] = STE1_EATS_FULLATS	|
		 STE1_S1CSH_IS		|
		 STE1_S1CIR_WBRA	|
		 STE1_S1COR_WBRA	|
		 STE1_STRW_NS_EL1;
	ste[2] = 0;
	ste[3] = 0;
	ste[4] = 0;
	ste[5] = 0;
	ste[6] = 0;
	ste[7] = 0;

	if (sc->features & SMMU_FEATURE_STALL &&
	    ((sc->features & SMMU_FEATURE_STALL_FORCE) == 0))
		ste[1] |= STE1_S1STALLD;

	/* Configure STE */
	val |= (cd->paddr & STE0_S1CONTEXTPTR_M);
	val |= STE0_CONFIG_S1_TRANS;

	smmu_invalidate_sid(sc, sid);

	/* The STE[0] has to be written in a single blast, last of all. */
	ste[0] = val;
	dsb(sy);

	smmu_invalidate_sid(sc, sid);
	smmu_sync_cd(sc, sid, 0, true);
	smmu_invalidate_sid(sc, sid);

	/* The sid will be used soon most likely. */
	smmu_prefetch_sid(sc, sid);

	return (0);
}

static uint64_t *
smmu_get_ste_addr(struct smmu_softc *sc, int sid)
{
	struct smmu_strtab *strtab;
	struct l1_desc *l1_desc;
	uint64_t *addr;

	strtab = &sc->strtab;

	if (sc->features & SMMU_FEATURE_2_LVL_STREAM_TABLE) {
		l1_desc = &strtab->l1[sid >> STRTAB_SPLIT];
		addr = l1_desc->va;
		addr += (sid & ((1 << STRTAB_SPLIT) - 1)) * STRTAB_STE_DWORDS;
	} else {
		addr = (void *)((uint64_t)strtab->vaddr +
		    STRTAB_STE_DWORDS * 8 * sid);
	};

	return (addr);
}

static int
smmu_init_ste(struct smmu_softc *sc, struct smmu_cd *cd, int sid, bool bypass)
{
	uint64_t *addr;

	addr = smmu_get_ste_addr(sc, sid);

	if (bypass)
		smmu_init_ste_bypass(sc, sid, addr);
	else
		smmu_init_ste_s1(sc, cd, sid, addr);

	smmu_sync(sc);

	return (0);
}

static void
smmu_deinit_ste(struct smmu_softc *sc, int sid)
{
	uint64_t *ste;

	ste = smmu_get_ste_addr(sc, sid);
	ste[0] = 0;

	smmu_invalidate_sid(sc, sid);
	smmu_sync_cd(sc, sid, 0, true);
	smmu_invalidate_sid(sc, sid);

	smmu_sync(sc);
}

static int
smmu_init_cd(struct smmu_softc *sc, struct smmu_domain *domain)
{
	vm_paddr_t paddr;
	uint64_t *ptr;
	uint64_t val;
	vm_size_t size;
	struct smmu_cd *cd;
	struct smmu_pmap *p;

	size = 1 * (CD_DWORDS << 3);

	p = &domain->p;
	cd = domain->cd = malloc(sizeof(struct smmu_cd),
	    M_SMMU, M_WAITOK | M_ZERO);

	cd->vaddr = contigmalloc(size, M_SMMU,
	    M_WAITOK | M_ZERO,	/* flags */
	    0,			/* low */
	    (1ul << 40) - 1,	/* high */
	    size,		/* alignment */
	    0);			/* boundary */
	if (cd->vaddr == NULL) {
		device_printf(sc->dev, "Failed to allocate CD\n");
		return (ENXIO);
	}

	cd->size = size;
	cd->paddr = vtophys(cd->vaddr);

	ptr = cd->vaddr;

	val = CD0_VALID;
	val |= CD0_AA64;
	val |= CD0_R;
	val |= CD0_A;
	val |= CD0_ASET;
	val |= (uint64_t)domain->asid << CD0_ASID_S;
	val |= CD0_TG0_4KB;
	val |= CD0_EPD1; /* Disable TT1 */
	val |= ((64 - sc->ias) << CD0_T0SZ_S);
	val |= CD0_IPS_48BITS;

	paddr = p->sp_l0_paddr & CD1_TTB0_M;
	KASSERT(paddr == p->sp_l0_paddr, ("bad allocation 1"));

	ptr[1] = paddr;
	ptr[2] = 0;
	ptr[3] = MAIR_ATTR(MAIR_DEVICE_nGnRnE, VM_MEMATTR_DEVICE)	|
		 MAIR_ATTR(MAIR_NORMAL_NC, VM_MEMATTR_UNCACHEABLE)	|
		 MAIR_ATTR(MAIR_NORMAL_WB, VM_MEMATTR_WRITE_BACK)	|
		 MAIR_ATTR(MAIR_NORMAL_WT, VM_MEMATTR_WRITE_THROUGH);

	/* Install the CD. */
	ptr[0] = val;

	return (0);
}

static int
smmu_init_strtab_linear(struct smmu_softc *sc)
{
	struct smmu_strtab *strtab;
	vm_paddr_t base;
	uint32_t size;
	uint64_t reg;

	strtab = &sc->strtab;
	strtab->num_l1_entries = (1 << sc->sid_bits);

	size = strtab->num_l1_entries * (STRTAB_STE_DWORDS << 3);

	if (bootverbose)
		device_printf(sc->dev,
		    "%s: linear strtab size %d, num_l1_entries %d\n",
		    __func__, size, strtab->num_l1_entries);

	strtab->vaddr = contigmalloc(size, M_SMMU,
	    M_WAITOK | M_ZERO,	/* flags */
	    0,			/* low */
	    (1ul << 48) - 1,	/* high */
	    size,		/* alignment */
	    0);			/* boundary */
	if (strtab->vaddr == NULL) {
		device_printf(sc->dev, "failed to allocate strtab\n");
		return (ENXIO);
	}

	reg = STRTAB_BASE_CFG_FMT_LINEAR;
	reg |= sc->sid_bits << STRTAB_BASE_CFG_LOG2SIZE_S;
	strtab->base_cfg = (uint32_t)reg;

	base = vtophys(strtab->vaddr);

	reg = base & STRTAB_BASE_ADDR_M;
	KASSERT(reg == base, ("bad allocation 2"));
	reg |= STRTAB_BASE_RA;
	strtab->base = reg;

	return (0);
}

static int
smmu_init_strtab_2lvl(struct smmu_softc *sc)
{
	struct smmu_strtab *strtab;
	vm_paddr_t base;
	uint64_t reg_base;
	uint32_t l1size;
	uint32_t size;
	uint32_t reg;
	int sz;

	strtab = &sc->strtab;

	size = STRTAB_L1_SZ_SHIFT - (ilog2(STRTAB_L1_DESC_DWORDS) + 3);
	size = min(size, sc->sid_bits - STRTAB_SPLIT);
	strtab->num_l1_entries = (1 << size);
	size += STRTAB_SPLIT;

	l1size = strtab->num_l1_entries * (STRTAB_L1_DESC_DWORDS << 3);

	if (bootverbose)
		device_printf(sc->dev,
		    "%s: size %d, l1 entries %d, l1size %d\n",
		    __func__, size, strtab->num_l1_entries, l1size);

	strtab->vaddr = contigmalloc(l1size, M_SMMU,
	    M_WAITOK | M_ZERO,	/* flags */
	    0,			/* low */
	    (1ul << 48) - 1,	/* high */
	    l1size,		/* alignment */
	    0);			/* boundary */
	if (strtab->vaddr == NULL) {
		device_printf(sc->dev, "Failed to allocate 2lvl strtab.\n");
		return (ENOMEM);
	}

	sz = strtab->num_l1_entries * sizeof(struct l1_desc);

	strtab->l1 = malloc(sz, M_SMMU, M_WAITOK | M_ZERO);
	if (strtab->l1 == NULL) {
		contigfree(strtab->vaddr, l1size, M_SMMU);
		return (ENOMEM);
	}

	reg = STRTAB_BASE_CFG_FMT_2LVL;
	reg |= size << STRTAB_BASE_CFG_LOG2SIZE_S;
	reg |= STRTAB_SPLIT << STRTAB_BASE_CFG_SPLIT_S;
	strtab->base_cfg = (uint32_t)reg;

	base = vtophys(strtab->vaddr);

	reg_base = base & STRTAB_BASE_ADDR_M;
	KASSERT(reg_base == base, ("bad allocation 3"));
	reg_base |= STRTAB_BASE_RA;
	strtab->base = reg_base;

	return (0);
}

static int
smmu_init_strtab(struct smmu_softc *sc)
{
	int error;

	if (sc->features & SMMU_FEATURE_2_LVL_STREAM_TABLE)
		error = smmu_init_strtab_2lvl(sc);
	else
		error = smmu_init_strtab_linear(sc);

	return (error);
}

static int
smmu_init_l1_entry(struct smmu_softc *sc, int sid)
{
	struct smmu_strtab *strtab;
	struct l1_desc *l1_desc;
	uint64_t *addr;
	uint64_t val;
	size_t size;
	int i;

	strtab = &sc->strtab;
	l1_desc = &strtab->l1[sid >> STRTAB_SPLIT];
	if (l1_desc->va) {
		/* Already allocated. */
		return (0);
	}

	size = 1 << (STRTAB_SPLIT + ilog2(STRTAB_STE_DWORDS) + 3);

	l1_desc->span = STRTAB_SPLIT + 1;
	l1_desc->size = size;
	l1_desc->va = contigmalloc(size, M_SMMU,
	    M_WAITOK | M_ZERO,	/* flags */
	    0,			/* low */
	    (1ul << 48) - 1,	/* high */
	    size,		/* alignment */
	    0);			/* boundary */
	if (l1_desc->va == NULL) {
		device_printf(sc->dev, "failed to allocate l2 entry\n");
		return (ENXIO);
	}

	l1_desc->pa = vtophys(l1_desc->va);

	i = sid >> STRTAB_SPLIT;
	addr = (void *)((uint64_t)strtab->vaddr +
	    STRTAB_L1_DESC_DWORDS * 8 * i);

	/* Install the L1 entry. */
	val = l1_desc->pa & STRTAB_L1_DESC_L2PTR_M;
	KASSERT(val == l1_desc->pa, ("bad allocation 4"));
	val |= l1_desc->span;
	*addr = val;

	return (0);
}

static void __unused
smmu_deinit_l1_entry(struct smmu_softc *sc, int sid)
{
	struct smmu_strtab *strtab;
	struct l1_desc *l1_desc;
	uint64_t *addr;
	int i;

	strtab = &sc->strtab;

	i = sid >> STRTAB_SPLIT;
	addr = (void *)((uint64_t)strtab->vaddr +
	    STRTAB_L1_DESC_DWORDS * 8 * i);
	*addr = 0;

	l1_desc = &strtab->l1[sid >> STRTAB_SPLIT];
	contigfree(l1_desc->va, l1_desc->size, M_SMMU);
}

static int
smmu_disable(struct smmu_softc *sc)
{
	uint32_t reg;
	int error;

	/* Disable SMMU */
	reg = bus_read_4(sc->res[0], SMMU_CR0);
	reg &= ~CR0_SMMUEN;
	error = smmu_write_ack(sc, SMMU_CR0, SMMU_CR0ACK, reg);
	if (error)
		device_printf(sc->dev, "Could not disable SMMU.\n");

	return (0);
}

static int
smmu_event_intr(void *arg)
{
	uint32_t evt[EVTQ_ENTRY_DWORDS * 2];
	struct smmu_softc *sc;

	sc = arg;

	do {
		smmu_evtq_dequeue(sc, evt);
		smmu_print_event(sc, evt);
	} while (!smmu_q_empty(&sc->evtq));

	return (FILTER_HANDLED);
}

static int __unused
smmu_sync_intr(void *arg)
{
	struct smmu_softc *sc;

	sc = arg;

	device_printf(sc->dev, "%s\n", __func__);

	return (FILTER_HANDLED);
}

static int
smmu_gerr_intr(void *arg)
{
	struct smmu_softc *sc;

	sc = arg;

	device_printf(sc->dev, "SMMU Global Error\n");

	return (FILTER_HANDLED);
}

static int
smmu_enable_interrupts(struct smmu_softc *sc)
{
	uint32_t reg;
	int error;

	/* Disable MSI. */
	bus_write_8(sc->res[0], SMMU_GERROR_IRQ_CFG0, 0);
	bus_write_4(sc->res[0], SMMU_GERROR_IRQ_CFG1, 0);
	bus_write_4(sc->res[0], SMMU_GERROR_IRQ_CFG2, 0);

	bus_write_8(sc->res[0], SMMU_EVENTQ_IRQ_CFG0, 0);
	bus_write_4(sc->res[0], SMMU_EVENTQ_IRQ_CFG1, 0);
	bus_write_4(sc->res[0], SMMU_EVENTQ_IRQ_CFG2, 0);

	if (sc->features & CR0_PRIQEN) {
		bus_write_8(sc->res[0], SMMU_PRIQ_IRQ_CFG0, 0);
		bus_write_4(sc->res[0], SMMU_PRIQ_IRQ_CFG1, 0);
		bus_write_4(sc->res[0], SMMU_PRIQ_IRQ_CFG2, 0);
	}

	/* Disable any interrupts. */
	error = smmu_write_ack(sc, SMMU_IRQ_CTRL, SMMU_IRQ_CTRLACK, 0);
	if (error) {
		device_printf(sc->dev, "Could not disable interrupts.\n");
		return (ENXIO);
	}

	/* Enable interrupts. */
	reg = IRQ_CTRL_EVENTQ_IRQEN | IRQ_CTRL_GERROR_IRQEN;
	if (sc->features & SMMU_FEATURE_PRI)
		reg |= IRQ_CTRL_PRIQ_IRQEN;

	error = smmu_write_ack(sc, SMMU_IRQ_CTRL, SMMU_IRQ_CTRLACK, reg);
	if (error) {
		device_printf(sc->dev, "Could not enable interrupts.\n");
		return (ENXIO);
	}

	return (0);
}

#ifdef DEV_ACPI
static void
smmu_configure_intr(struct smmu_softc *sc, struct resource *res)
{
	struct intr_map_data_acpi *ad;
	struct intr_map_data *data;

	data = rman_get_virtual(res);
	KASSERT(data != NULL, ("data is NULL"));

	if (data->type == INTR_MAP_DATA_ACPI) {
		ad = (struct intr_map_data_acpi *)data;
		ad->trig = INTR_TRIGGER_EDGE;
		ad->pol = INTR_POLARITY_HIGH;
	}
}
#endif

static int
smmu_setup_interrupts(struct smmu_softc *sc)
{
	device_t dev;
	int error;

	dev = sc->dev;

#ifdef DEV_ACPI
	/*
	 * Configure SMMU interrupts as EDGE triggered manually
	 * as ACPI tables carries no information for that.
	 */
	smmu_configure_intr(sc, sc->res[1]);
	/* PRIQ is not in use. */
	smmu_configure_intr(sc, sc->res[3]);
	smmu_configure_intr(sc, sc->res[4]);
#endif

	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_MISC,
	    smmu_event_intr, NULL, sc, &sc->intr_cookie[0]);
	if (error) {
		device_printf(dev, "Couldn't setup Event interrupt handler\n");
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->res[4], INTR_TYPE_MISC,
	    smmu_gerr_intr, NULL, sc, &sc->intr_cookie[2]);
	if (error) {
		device_printf(dev, "Couldn't setup Gerr interrupt handler\n");
		return (ENXIO);
	}

	return (0);
}

static int
smmu_reset(struct smmu_softc *sc)
{
	struct smmu_cmdq_entry cmd;
	struct smmu_strtab *strtab;
	int error;
	int reg;

	reg = bus_read_4(sc->res[0], SMMU_CR0);

	if (reg & CR0_SMMUEN)
		device_printf(sc->dev,
		    "%s: Warning: SMMU is enabled\n", __func__);

	error = smmu_disable(sc);
	if (error)
		device_printf(sc->dev,
		    "%s: Could not disable SMMU.\n", __func__);

	if (smmu_enable_interrupts(sc) != 0) {
		device_printf(sc->dev, "Could not enable interrupts.\n");
		return (ENXIO);
	}

	reg = CR1_TABLE_SH_IS	|
	      CR1_TABLE_OC_WBC	|
	      CR1_TABLE_IC_WBC	|
	      CR1_QUEUE_SH_IS	|
	      CR1_QUEUE_OC_WBC	|
	      CR1_QUEUE_IC_WBC;
	bus_write_4(sc->res[0], SMMU_CR1, reg);

	reg = CR2_PTM | CR2_RECINVSID | CR2_E2H;
	bus_write_4(sc->res[0], SMMU_CR2, reg);

	/* Stream table. */
	strtab = &sc->strtab;
	bus_write_8(sc->res[0], SMMU_STRTAB_BASE, strtab->base);
	bus_write_4(sc->res[0], SMMU_STRTAB_BASE_CFG, strtab->base_cfg);

	/* Command queue. */
	bus_write_8(sc->res[0], SMMU_CMDQ_BASE, sc->cmdq.base);
	bus_write_4(sc->res[0], SMMU_CMDQ_PROD, sc->cmdq.lc.prod);
	bus_write_4(sc->res[0], SMMU_CMDQ_CONS, sc->cmdq.lc.cons);

	reg = CR0_CMDQEN;
	error = smmu_write_ack(sc, SMMU_CR0, SMMU_CR0ACK, reg);
	if (error) {
		device_printf(sc->dev, "Could not enable command queue\n");
		return (ENXIO);
	}

	/* Invalidate cached configuration. */
	smmu_invalidate_all_sid(sc);

	if (sc->features & SMMU_FEATURE_HYP) {
		cmd.opcode = CMD_TLBI_EL2_ALL;
		smmu_cmdq_enqueue_cmd(sc, &cmd);
	};

	/* Invalidate TLB. */
	smmu_tlbi_all(sc);

	/* Event queue */
	bus_write_8(sc->res[0], SMMU_EVENTQ_BASE, sc->evtq.base);
	bus_write_4(sc->res[0], SMMU_EVENTQ_PROD, sc->evtq.lc.prod);
	bus_write_4(sc->res[0], SMMU_EVENTQ_CONS, sc->evtq.lc.cons);

	reg |= CR0_EVENTQEN;
	error = smmu_write_ack(sc, SMMU_CR0, SMMU_CR0ACK, reg);
	if (error) {
		device_printf(sc->dev, "Could not enable event queue\n");
		return (ENXIO);
	}

	if (sc->features & SMMU_FEATURE_PRI) {
		/* PRI queue */
		bus_write_8(sc->res[0], SMMU_PRIQ_BASE, sc->priq.base);
		bus_write_4(sc->res[0], SMMU_PRIQ_PROD, sc->priq.lc.prod);
		bus_write_4(sc->res[0], SMMU_PRIQ_CONS, sc->priq.lc.cons);

		reg |= CR0_PRIQEN;
		error = smmu_write_ack(sc, SMMU_CR0, SMMU_CR0ACK, reg);
		if (error) {
			device_printf(sc->dev, "Could not enable PRI queue\n");
			return (ENXIO);
		}
	}

	if (sc->features & SMMU_FEATURE_ATS) {
		reg |= CR0_ATSCHK;
		error = smmu_write_ack(sc, SMMU_CR0, SMMU_CR0ACK, reg);
		if (error) {
			device_printf(sc->dev, "Could not enable ATS check.\n");
			return (ENXIO);
		}
	}

	reg |= CR0_SMMUEN;
	error = smmu_write_ack(sc, SMMU_CR0, SMMU_CR0ACK, reg);
	if (error) {
		device_printf(sc->dev, "Could not enable SMMU.\n");
		return (ENXIO);
	}

	return (0);
}

static int
smmu_check_features(struct smmu_softc *sc)
{
	uint32_t reg;
	uint32_t val;

	sc->features = 0;

	reg = bus_read_4(sc->res[0], SMMU_IDR0);

	if (reg & IDR0_ST_LVL_2) {
		if (bootverbose)
			device_printf(sc->dev,
			    "2-level stream table supported.\n");
		sc->features |= SMMU_FEATURE_2_LVL_STREAM_TABLE;
	}

	if (reg & IDR0_CD2L) {
		if (bootverbose)
			device_printf(sc->dev,
			    "2-level CD table supported.\n");
		sc->features |= SMMU_FEATURE_2_LVL_CD;
	}

	switch (reg & IDR0_TTENDIAN_M) {
	case IDR0_TTENDIAN_MIXED:
		if (bootverbose)
			device_printf(sc->dev, "Mixed endianness supported.\n");
		sc->features |= SMMU_FEATURE_TT_LE;
		sc->features |= SMMU_FEATURE_TT_BE;
		break;
	case IDR0_TTENDIAN_LITTLE:
		if (bootverbose)
			device_printf(sc->dev,
			    "Little endian supported only.\n");
		sc->features |= SMMU_FEATURE_TT_LE;
		break;
	case IDR0_TTENDIAN_BIG:
		if (bootverbose)
			device_printf(sc->dev, "Big endian supported only.\n");
		sc->features |= SMMU_FEATURE_TT_BE;
		break;
	default:
		device_printf(sc->dev, "Unsupported endianness.\n");
		return (ENXIO);
	}

	if (reg & IDR0_SEV)
		sc->features |= SMMU_FEATURE_SEV;

	if (reg & IDR0_MSI) {
		if (bootverbose)
			device_printf(sc->dev, "MSI feature present.\n");
		sc->features |= SMMU_FEATURE_MSI;
	}

	if (reg & IDR0_HYP) {
		if (bootverbose)
			device_printf(sc->dev, "HYP feature present.\n");
		sc->features |= SMMU_FEATURE_HYP;
	}

	if (reg & IDR0_ATS)
		sc->features |= SMMU_FEATURE_ATS;

	if (reg & IDR0_PRI)
		sc->features |= SMMU_FEATURE_PRI;

	switch (reg & IDR0_STALL_MODEL_M) {
	case IDR0_STALL_MODEL_FORCE:
		/* Stall is forced. */
		sc->features |= SMMU_FEATURE_STALL_FORCE;
		/* FALLTHROUGH */
	case IDR0_STALL_MODEL_STALL:
		sc->features |= SMMU_FEATURE_STALL;
		break;
	}

	/* Grab translation stages supported. */
	if (reg & IDR0_S1P) {
		if (bootverbose)
			device_printf(sc->dev,
			    "Stage 1 translation supported.\n");
		sc->features |= SMMU_FEATURE_S1P;
	}
	if (reg & IDR0_S2P) {
		if (bootverbose)
			device_printf(sc->dev,
			    "Stage 2 translation supported.\n");
		sc->features |= SMMU_FEATURE_S2P;
	}

	switch (reg & IDR0_TTF_M) {
	case IDR0_TTF_ALL:
	case IDR0_TTF_AA64:
		sc->ias = 40;
		break;
	default:
		device_printf(sc->dev, "No AArch64 table format support.\n");
		return (ENXIO);
	}

	if (reg & IDR0_ASID16)
		sc->asid_bits = 16;
	else
		sc->asid_bits = 8;

	if (bootverbose)
		device_printf(sc->dev, "ASID bits %d\n", sc->asid_bits);

	if (reg & IDR0_VMID16)
		sc->vmid_bits = 16;
	else
		sc->vmid_bits = 8;

	reg = bus_read_4(sc->res[0], SMMU_IDR1);

	if (reg & (IDR1_TABLES_PRESET | IDR1_QUEUES_PRESET | IDR1_REL)) {
		device_printf(sc->dev,
		    "Embedded implementations not supported by this driver.\n");
		return (ENXIO);
	}

	val = (reg & IDR1_CMDQS_M) >> IDR1_CMDQS_S;
	sc->cmdq.size_log2 = val;
	if (bootverbose)
		device_printf(sc->dev, "CMD queue bits %d\n", val);

	val = (reg & IDR1_EVENTQS_M) >> IDR1_EVENTQS_S;
	sc->evtq.size_log2 = val;
	if (bootverbose)
		device_printf(sc->dev, "EVENT queue bits %d\n", val);

	if (sc->features & SMMU_FEATURE_PRI) {
		val = (reg & IDR1_PRIQS_M) >> IDR1_PRIQS_S;
		sc->priq.size_log2 = val;
		if (bootverbose)
			device_printf(sc->dev, "PRI queue bits %d\n", val);
	}

	sc->ssid_bits = (reg & IDR1_SSIDSIZE_M) >> IDR1_SSIDSIZE_S;
	sc->sid_bits = (reg & IDR1_SIDSIZE_M) >> IDR1_SIDSIZE_S;

	if (sc->sid_bits <= STRTAB_SPLIT)
		sc->features &= ~SMMU_FEATURE_2_LVL_STREAM_TABLE;

	if (bootverbose) {
		device_printf(sc->dev, "SSID bits %d\n", sc->ssid_bits);
		device_printf(sc->dev, "SID bits %d\n", sc->sid_bits);
	}

	/* IDR3 */
	reg = bus_read_4(sc->res[0], SMMU_IDR3);
	if (reg & IDR3_RIL)
		sc->features |= SMMU_FEATURE_RANGE_INV;

	/* IDR5 */
	reg = bus_read_4(sc->res[0], SMMU_IDR5);

	switch (reg & IDR5_OAS_M) {
	case IDR5_OAS_32:
		sc->oas = 32;
		break;
	case IDR5_OAS_36:
		sc->oas = 36;
		break;
	case IDR5_OAS_40:
		sc->oas = 40;
		break;
	case IDR5_OAS_42:
		sc->oas = 42;
		break;
	case IDR5_OAS_44:
		sc->oas = 44;
		break;
	case IDR5_OAS_48:
		sc->oas = 48;
		break;
	case IDR5_OAS_52:
		sc->oas = 52;
		break;
	}

	sc->pgsizes = 0;
	if (reg & IDR5_GRAN64K)
		sc->pgsizes |= 64 * 1024;
	if (reg & IDR5_GRAN16K)
		sc->pgsizes |= 16 * 1024;
	if (reg & IDR5_GRAN4K)
		sc->pgsizes |= 4 * 1024;

	if ((reg & IDR5_VAX_M) == IDR5_VAX_52)
		sc->features |= SMMU_FEATURE_VAX;

	return (0);
}

static void
smmu_init_asids(struct smmu_softc *sc)
{

	sc->asid_set_size = (1 << sc->asid_bits);
	sc->asid_set = bit_alloc(sc->asid_set_size, M_SMMU, M_WAITOK);
	mtx_init(&sc->asid_set_mutex, "asid set", NULL, MTX_SPIN);
}

static int
smmu_asid_alloc(struct smmu_softc *sc, int *new_asid)
{

	mtx_lock_spin(&sc->asid_set_mutex);
	bit_ffc(sc->asid_set, sc->asid_set_size, new_asid);
	if (*new_asid == -1) {
		mtx_unlock_spin(&sc->asid_set_mutex);
		return (ENOMEM);
	}
	bit_set(sc->asid_set, *new_asid);
	mtx_unlock_spin(&sc->asid_set_mutex);

	return (0);
}

static void
smmu_asid_free(struct smmu_softc *sc, int asid)
{

	mtx_lock_spin(&sc->asid_set_mutex);
	bit_clear(sc->asid_set, asid);
	mtx_unlock_spin(&sc->asid_set_mutex);
}

/*
 * Device interface.
 */
int
smmu_attach(device_t dev)
{
	struct smmu_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->dev), "smmu", MTX_DEF);

	error = smmu_setup_interrupts(sc);
	if (error) {
		bus_release_resources(dev, smmu_spec, sc->res);
		return (ENXIO);
	}

	error = smmu_check_features(sc);
	if (error) {
		device_printf(dev, "Some features are required "
		    "but not supported by hardware.\n");
		return (ENXIO);
	}

	smmu_init_asids(sc);

	error = smmu_init_queues(sc);
	if (error) {
		device_printf(dev, "Couldn't allocate queues.\n");
		return (ENXIO);
	}

	error = smmu_init_strtab(sc);
	if (error) {
		device_printf(dev, "Couldn't allocate strtab.\n");
		return (ENXIO);
	}

	error = smmu_reset(sc);
	if (error) {
		device_printf(dev, "Couldn't reset SMMU.\n");
		return (ENXIO);
	}

	return (0);
}

int
smmu_detach(device_t dev)
{
	struct smmu_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resources(dev, smmu_spec, sc->res);

	return (0);
}

static int
smmu_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct smmu_softc *sc;

	sc = device_get_softc(dev);

	device_printf(sc->dev, "%s\n", __func__);

	return (ENOENT);
}

static int
smmu_unmap(device_t dev, struct iommu_domain *iodom,
    vm_offset_t va, bus_size_t size)
{
	struct smmu_domain *domain;
	struct smmu_softc *sc;
	int err;
	int i;

	sc = device_get_softc(dev);

	domain = (struct smmu_domain *)iodom;

	err = 0;

	dprintf("%s: %lx, %ld, domain %d\n", __func__, va, size, domain->asid);

	for (i = 0; i < size; i += PAGE_SIZE) {
		if (smmu_pmap_remove(&domain->p, va) == 0) {
			/* pmap entry removed, invalidate TLB. */
			smmu_tlbi_va(sc, va, domain->asid);
		} else {
			err = ENOENT;
			break;
		}
		va += PAGE_SIZE;
	}

	smmu_sync(sc);

	return (err);
}

static int
smmu_map(device_t dev, struct iommu_domain *iodom,
    vm_offset_t va, vm_page_t *ma, vm_size_t size,
    vm_prot_t prot)
{
	struct smmu_domain *domain;
	struct smmu_softc *sc;
	vm_paddr_t pa;
	int error;
	int i;

	sc = device_get_softc(dev);

	domain = (struct smmu_domain *)iodom;

	dprintf("%s: %lx -> %lx, %ld, domain %d\n", __func__, va, pa, size,
	    domain->asid);

	for (i = 0; size > 0; size -= PAGE_SIZE) {
		pa = VM_PAGE_TO_PHYS(ma[i++]);
		error = smmu_pmap_enter(&domain->p, va, pa, prot, 0);
		if (error)
			return (error);
		smmu_tlbi_va(sc, va, domain->asid);
		va += PAGE_SIZE;
	}

	smmu_sync(sc);

	return (0);
}

static struct iommu_domain *
smmu_domain_alloc(device_t dev, struct iommu_unit *iommu)
{
	struct iommu_domain *iodom;
	struct smmu_domain *domain;
	struct smmu_unit *unit;
	struct smmu_softc *sc;
	int error;
	int new_asid;

	sc = device_get_softc(dev);

	unit = (struct smmu_unit *)iommu;

	domain = malloc(sizeof(*domain), M_SMMU, M_WAITOK | M_ZERO);

	error = smmu_asid_alloc(sc, &new_asid);
	if (error) {
		free(domain, M_SMMU);
		device_printf(sc->dev,
		    "Could not allocate ASID for a new domain.\n");
		return (NULL);
	}

	domain->asid = (uint16_t)new_asid;

	smmu_pmap_pinit(&domain->p);

	error = smmu_init_cd(sc, domain);
	if (error) {
		free(domain, M_SMMU);
		device_printf(sc->dev, "Could not initialize CD\n");
		return (NULL);
	}

	smmu_tlbi_asid(sc, domain->asid);

	LIST_INIT(&domain->ctx_list);

	IOMMU_LOCK(iommu);
	LIST_INSERT_HEAD(&unit->domain_list, domain, next);
	IOMMU_UNLOCK(iommu);

	iodom = &domain->iodom;

	/*
	 * Use 48-bit address space regardless of VAX bit
	 * as we need 64k IOMMU_PAGE_SIZE for 52-bit space.
	 */
	iodom->end = MAXADDR_48BIT;

	return (iodom);
}

static void
smmu_domain_free(device_t dev, struct iommu_domain *iodom)
{
	struct smmu_domain *domain;
	struct smmu_softc *sc;
	struct smmu_cd *cd;

	sc = device_get_softc(dev);

	domain = (struct smmu_domain *)iodom;

	LIST_REMOVE(domain, next);

	cd = domain->cd;

	smmu_pmap_remove_pages(&domain->p);
	smmu_pmap_release(&domain->p);

	smmu_tlbi_asid(sc, domain->asid);
	smmu_asid_free(sc, domain->asid);

	contigfree(cd->vaddr, cd->size, M_SMMU);
	free(cd, M_SMMU);

	free(domain, M_SMMU);
}

static int
smmu_set_buswide(device_t dev, struct smmu_domain *domain,
    struct smmu_ctx *ctx)
{
	struct smmu_softc *sc;
	int i;

	sc = device_get_softc(dev);

	for (i = 0; i < PCI_SLOTMAX; i++)
		smmu_init_ste(sc, domain->cd, (ctx->sid | i), ctx->bypass);

	return (0);
}

static int
smmu_pci_get_sid(device_t child, u_int *xref0, u_int *sid0)
{
	struct pci_id_ofw_iommu pi;
	int err;

	err = pci_get_id(child, PCI_ID_OFW_IOMMU, (uintptr_t *)&pi);
	if (err == 0) {
		if (sid0)
			*sid0 = pi.id;
		if (xref0)
			*xref0 = pi.xref;
	}

	return (err);
}

static struct iommu_ctx *
smmu_ctx_alloc(device_t dev, struct iommu_domain *iodom, device_t child,
    bool disabled)
{
	struct smmu_domain *domain;
	struct smmu_ctx *ctx;

	domain = (struct smmu_domain *)iodom;

	ctx = malloc(sizeof(struct smmu_ctx), M_SMMU, M_WAITOK | M_ZERO);
	ctx->dev = child;
	ctx->domain = domain;
	if (disabled)
		ctx->bypass = true;

	IOMMU_DOMAIN_LOCK(iodom);
	LIST_INSERT_HEAD(&domain->ctx_list, ctx, next);
	IOMMU_DOMAIN_UNLOCK(iodom);

	return (&ctx->ioctx);
}

static int
smmu_ctx_init(device_t dev, struct iommu_ctx *ioctx)
{
	struct smmu_domain *domain;
	struct iommu_domain *iodom;
	struct smmu_softc *sc;
	struct smmu_ctx *ctx;
	devclass_t pci_class;
	u_int sid;
	int err;

	ctx = (struct smmu_ctx *)ioctx;

	sc = device_get_softc(dev);

	domain = ctx->domain;
	iodom = (struct iommu_domain *)domain;

	pci_class = devclass_find("pci");
	if (device_get_devclass(device_get_parent(ctx->dev)) == pci_class) {
		err = smmu_pci_get_sid(ctx->dev, NULL, &sid);
		if (err)
			return (err);

		ioctx->rid = pci_get_rid(dev);
		ctx->sid = sid;
		ctx->vendor = pci_get_vendor(ctx->dev);
		ctx->device = pci_get_device(ctx->dev);
	}

	if (sc->features & SMMU_FEATURE_2_LVL_STREAM_TABLE) {
		err = smmu_init_l1_entry(sc, ctx->sid);
		if (err)
			return (err);
	}

	/*
	 * Neoverse N1 SDP:
	 * 0x800 xhci
	 * 0x700 re
	 * 0x600 sata
	 */

	smmu_init_ste(sc, domain->cd, ctx->sid, ctx->bypass);

	if (device_get_devclass(device_get_parent(ctx->dev)) == pci_class)
		if (iommu_is_buswide_ctx(iodom->iommu, pci_get_bus(ctx->dev)))
			smmu_set_buswide(dev, domain, ctx);

	return (0);
}

static void
smmu_ctx_free(device_t dev, struct iommu_ctx *ioctx)
{
	struct smmu_softc *sc;
	struct smmu_ctx *ctx;

	IOMMU_ASSERT_LOCKED(ioctx->domain->iommu);

	sc = device_get_softc(dev);
	ctx = (struct smmu_ctx *)ioctx;

	smmu_deinit_ste(sc, ctx->sid);

	LIST_REMOVE(ctx, next);

	free(ctx, M_SMMU);
}

struct smmu_ctx *
smmu_ctx_lookup_by_sid(device_t dev, u_int sid)
{
	struct smmu_softc *sc;
	struct smmu_domain *domain;
	struct smmu_unit *unit;
	struct smmu_ctx *ctx;

	sc = device_get_softc(dev);

	unit = &sc->unit;

	LIST_FOREACH(domain, &unit->domain_list, next) {
		LIST_FOREACH(ctx, &domain->ctx_list, next) {
			if (ctx->sid == sid)
				return (ctx);
		}
	}

	return (NULL);
}

static struct iommu_ctx *
smmu_ctx_lookup(device_t dev, device_t child)
{
	struct iommu_unit *iommu __diagused;
	struct smmu_softc *sc;
	struct smmu_domain *domain;
	struct smmu_unit *unit;
	struct smmu_ctx *ctx;

	sc = device_get_softc(dev);

	unit = &sc->unit;
	iommu = &unit->iommu;

	IOMMU_ASSERT_LOCKED(iommu);

	LIST_FOREACH(domain, &unit->domain_list, next) {
		IOMMU_DOMAIN_LOCK(&domain->iodom);
		LIST_FOREACH(ctx, &domain->ctx_list, next) {
			if (ctx->dev == child) {
				IOMMU_DOMAIN_UNLOCK(&domain->iodom);
				return (&ctx->ioctx);
			}
		}
		IOMMU_DOMAIN_UNLOCK(&domain->iodom);
	}

	return (NULL);
}

static int
smmu_find(device_t dev, device_t child)
{
	struct smmu_softc *sc;
	u_int xref;
	int err;

	sc = device_get_softc(dev);

	err = smmu_pci_get_sid(child, &xref, NULL);
	if (err)
		return (ENOENT);

	/* Check if xref is ours. */
	if (xref != sc->xref)
		return (EFAULT);

	return (0);
}

#ifdef FDT
static int
smmu_ofw_md_data(device_t dev, struct iommu_ctx *ioctx, pcell_t *cells,
    int ncells)
{
	struct smmu_ctx *ctx;

	ctx = (struct smmu_ctx *)ioctx;

	if (ncells != 1)
		return (-1);

	ctx->sid = cells[0];

	return (0);
}
#endif

static device_method_t smmu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_detach,	smmu_detach),

	/* SMMU interface */
	DEVMETHOD(iommu_find,		smmu_find),
	DEVMETHOD(iommu_map,		smmu_map),
	DEVMETHOD(iommu_unmap,		smmu_unmap),
	DEVMETHOD(iommu_domain_alloc,	smmu_domain_alloc),
	DEVMETHOD(iommu_domain_free,	smmu_domain_free),
	DEVMETHOD(iommu_ctx_alloc,	smmu_ctx_alloc),
	DEVMETHOD(iommu_ctx_init,	smmu_ctx_init),
	DEVMETHOD(iommu_ctx_free,	smmu_ctx_free),
	DEVMETHOD(iommu_ctx_lookup,	smmu_ctx_lookup),
#ifdef FDT
	DEVMETHOD(iommu_ofw_md_data,	smmu_ofw_md_data),
#endif

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	smmu_read_ivar),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(smmu, smmu_driver, smmu_methods, sizeof(struct smmu_softc));
