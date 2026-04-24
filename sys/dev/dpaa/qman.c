/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */
/*-
 * Copyright (c) 2011-2012 Semihalf.
 * All rights reserved.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/rman.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/tlb.h>

#include "dpaa_common.h"
#include "portals.h"
#include "qman.h"
#include "qman_var.h"
#include "qman_portal_if.h"

/* Registers */
#define	QCSP_IO_CFG(n)		(0x004 + (n) * 16)
#define	  IO_CFG_SDEST_M	  0x00ff0000
#define	  IO_CFG_SDEST_S	  16
#define	QMAN_DCP_CFG(n)		(0x300 + (n) * 0x10)
#define	  DCP_CFG_ED		  0x00000100
#define	  DCP_CFG_ED_3		  0x00001000
#define	QMAN_PFDR_FP_LWIT	0x410
#define	QMAN_PFDR_CFG		0x414
#define	QMAN_SFDR_CFG		0x500
#define	QMAN_MCR		0xb00
#define	  MCR_INIT_PFDR		  0x01000000
#define	  MCR_READ_PFDR		  0x02000000
#define	  MCR_READ_SFDR		  0x03000000
#define	  MCR_QUERY_FQD_FILL	  0x10000000
#define	  MCR_QUERY_FQD_TAGS	  0x11000000
#define	  MCR_QUERY_FQD_CACHE	  0x12000000
#define	  MCR_QUERY_WQ		  0x20000000
#define	  MCR_RSLT_OK		  0xf0000000
#define	  MCR_RSLT_OK_DATA	  0xf1000000
#define	  MCR_RSLT_ABRT_INV	  0xf4000000
#define	  MCR_RSLT_ABRT_DIS	  0xf8000000
#define	  MCR_RSLT_ABRT_IDX	  0xff000000
#define	  MCR_RSLT_ABRT_MASK	  0xff000000
#define	QMAN_MCP0		0xb04
#define	QMAN_MCP1		0xb08
#define	QMAN_IP_REV_1		0xbf8
#define	  IP_MJ_M		  0x0000ff00
#define	  IP_MJ_S		  8
#define	  IP_MN_M		  0x000000ff
#define	QMAN_FQD_BARE		0xc00
#define	QMAN_FQD_BAR		0xc04
#define	QMAN_FQD_AR		0xc10
#define	  AR_EN			0x80000000
#define	QMAN_PFDR_BARE		0xc20
#define	QMAN_PFDR_BAR		0xc24
#define	QMAN_PFDR_AR		0xc30
#define	QMAN_QCSP_BARE		0xc80
#define	QMAN_QCSP_BAR		0xc84
#define	QMAN_QCSP_AR		0xc90
#define	QMAN_CI_SCHED_CFG	0xd00
#define	  CI_SCHED_CFG_SW	  0x80000000
#define	  CI_SCHED_CFG_SRCCIV	  0x04000000	/* Recommended */
#define	  CI_SCHED_CFG_SRQ_W_M	  0x00000700
#define	  CI_SCHED_CFG_SRQ_W_S	  8
#define	  CI_SCHED_CFG_RW_W_M	  0x00000070
#define	  CI_SCHED_CFG_RW_W_S	  4
#define	  CI_SCHED_CFG_BMAN_W_M	  0x00000007
#define	QMAN_ERR_ISR		0xe00
#define	QMAN_ERR_IER		0xe04
#define	QCSP_IO_CFG_3(n)	(0x1004 + (n) * 16)

/* Software portals.  Cache-inhibited registers */

#define	QCSP_DQRR_PDQCR		0x05c

/* Software portals.  Cache-enabled registers */

#define	QCSP_VERB_INIT_FQ_PARK		0x40
#define	QCSP_VERB_INIT_FQ_SCHED		0x41
#define	QCSP_VERB_QUERY_FQ		0x44
#define	QCSP_VERB_QUERY_FQ_NP		0x45
#define	QCSP_VERB_ALTER_FQ_SCHED	0x48
#define	QCSP_VERB_ALTER_FQ_FE		0x49
#define	QCSP_VERB_ALTER_FQ_RETIRE	0x4a
#define	QCSP_VERB_ALTER_FQ_TAKE_OUT	0x4b
#define	QCSP_VERB_ALTER_FQ_RETIRE_CTXB	0x4c
#define	QCSP_VERB_ALTER_FQ_XON		0x4d
#define	QCSP_VERB_ALTER_FQ_XOFF		0x4e

/* Init FQ */
#define	QCSP_INIT_FQ_WE_OAC		0x0100
#define	QCSP_INIT_FQ_WE_ORPC		0x0080
#define	QCSP_INIT_FQ_WE_CGID		0x0040
#define	QCSP_INIT_FQ_WE_FQ_CTRL		0x0020
#define	QCSP_INIT_FQ_WE_DEST_WQ		0x0010
#define	QCSP_INIT_FQ_WE_ICS_CRED	0x0008
#define	QCSP_INIT_FQ_WE_TD_THRESH	0x0004
#define	QCSP_INIT_FQ_WE_CONTEXT_B	0x0002
#define	QCSP_INIT_FQ_WE_CONTEXT_A	0x0001

#define	QMAN_MC_RES_OK			0xf0

#define	QMAN_MC_AFQS_NE			0x01

/* Init FQ options */
#define	QM_FQCTRL_CGE			0x0400
#define	QM_FQCTRL_TDE			0x0200
#define	QM_FQCTRL_ORP			0x0100
#define	QM_FQCTRL_CTXASTASH		0x0080
#define	QM_FQCTRL_CPCSTASH		0x0040
#define	QM_FQCTRL_FORCESFDR		0x0008
#define	QM_FQCTRL_AVOIDBLOCK		0x0004
#define	QM_FQCTRL_HOLDACTIVE		0x0002
#define	QM_FQCTRL_LIC			0x0001

#define	QMAN_CHANNEL_POOL1_REV1		0x21
#define	QMAN_CHANNEL_POOL1_REV3		0x401

#define	QMAN_PFDR_MAX			0xfffeff

/* P1023 has only 3 pool channels, but we don't support that SoC. */
#define	QMAN_POOL_CHANNELS		15

/* P1023 only supports 64 congestion groups... */
#define	QMAN_CGRS			256

static struct qman_softc *qman_sc;

static MALLOC_DEFINE(M_QMAN, "qman", "DPAA Queue Manager structures");

int qman_channel_base;
int qman_total_fqids;
struct qman_fq **qman_fq_list;

/* Entries sorted right-to-left in bit order of the ISR */
static const char * const qman_errors[] = {
	"Invalid enqueue queue",
	"Invalid enqueue channel!",
	"Invalid enqueue state",
	"Invalid enqueue overflow",
	"Invalid enqueue configuration",
	NULL,
	NULL,
	NULL,
	"Invalid dequeue queue",
	"Invalid dequeue source",
	"Invalid dequeue FQ",
	"Invalid dequeue direct connect portal",
	NULL,
	NULL,
	NULL,
	NULL,
	"Invalid command verb",
	"Invalid FQ flow control state",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"Insufficient free PFDRs",
	"Single-bit ECC error",
	"Multi-bit ECC error",
	"PFDR low watermark",
	"Invalid target transaction",
	"Initiator data error",
	NULL,
	NULL
};

static void
qman_isr(void *arg)
{
	struct qman_softc *sc = arg;
	uint32_t ier, isr, isr_bit;
	int i;

	ier = bus_read_4(sc->sc_rres, QMAN_ERR_IER);
	isr = bus_read_4(sc->sc_rres, QMAN_ERR_ISR);

	if ((ier & isr) == 0)
		return;

	isr_bit = (isr & ier);
	for (i = 0; isr_bit != 0; i++, isr_bit >>= 1) {
		if (isr_bit & 1)
			device_printf(sc->sc_dev, "%s", qman_errors[i]);
	}

	bus_write_4(sc->sc_rres, QMAN_ERR_ISR, isr);
}


/* Set up reserved memory configuration for PFDR and FQD, per `off`. */
static int
qman_set_memory(struct qman_softc *sc, vm_paddr_t pa,
    vm_size_t size, bus_size_t off)
{
	uint32_t bar, bare;
	vm_paddr_t old_bar;

	/*
	 * Register offsets:
	 * 0 - BARE
	 * 4 - BAR
	 * 0x10 - AR
	 */
	bare = bus_read_4(sc->sc_rres, off);
	bar = bus_read_4(sc->sc_rres, off + 4);
	old_bar = (vm_paddr_t)bare << 32 | bar;

	if (old_bar != 0 && old_bar != pa) {
		device_printf(sc->sc_dev, "QMan BAR already initialized!\n");
		return (ENOMEM);
	} else if (old_bar == pa)
		return (EEXIST);

	/*
	 * Zero the memory and flush cache through DMAP. QMan accesses the
	 * memory as non-coherent.
	 */
	memset((void *)PHYS_TO_DMAP(pa), 0, size);
	cpu_flush_dcache((void *)PHYS_TO_DMAP(pa), size);

	bus_write_4(sc->sc_rres, off, pa >> 32);
	bus_write_4(sc->sc_rres, off + 4, (uint32_t)pa);
	bus_write_4(sc->sc_rres, off + 0x10, AR_EN | (ilog2(size) - 1));

	return (0);
}

/*
 * Set up PFDR structures.  Some things to keep in mind:
 * - npfdr is the total number of PFDRs in the private memory.  PFDRs are 64
 *   bytes in size, so npfdr is (pfdr_sz/64).
 * - PFDR 0-7 are reserved, so the base starts at 8, not 0, so we adjust
 *   internally.
 * - The second parameter is the last PFDR, not the number of PFDRs, so needs to
 *   be adjusted down one more, so subtract 9.
 */
static int
qman_setup_pfdr(struct qman_softc *sc, int npfdr)
{
	uint32_t res;

	npfdr = min(npfdr, QMAN_PFDR_MAX);
	bus_write_4(sc->sc_rres, QMAN_MCP0, 8);
	bus_write_4(sc->sc_rres, QMAN_MCP1, npfdr - 9);
	bus_write_4(sc->sc_rres, QMAN_MCR, MCR_INIT_PFDR);

	for (int timeout = 100000; timeout > 0; timeout--) {
		DELAY(1);
		res = bus_read_4(sc->sc_rres, QMAN_MCR);
		if (res >= MCR_RSLT_OK)
			break;
	}

	if (res < MCR_RSLT_OK)
		return (EBUSY);
	if (res == MCR_RSLT_OK)
		return (0);

	return (ENXIO);
}

int
qman_attach(device_t dev)
{
	struct qman_softc *sc;
	int error;
	vm_paddr_t fqd_pa, pfdr_pa;
	vm_size_t fqd_sz, pfdr_sz;
	int qman_channel_pool1 = QMAN_CHANNEL_POOL1_REV1;
	uint32_t ver;
	uint32_t nfqd;
	bool qman3 = false;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	qman_sc = sc;

	/* Allocate resources */
	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 0, RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		device_printf(dev, "could not allocate memory.\n");
		goto err;
	}

	sc->sc_irid = 0;
	sc->sc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->sc_irid, RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_ires == NULL) {
		device_printf(dev, "could not allocate error interrupt.\n");
		goto err;
	}
	error = dpaa_map_private_memory(dev, 0, "fsl,qman-fqd",
	    &fqd_pa, &fqd_sz);
	error = dpaa_map_private_memory(dev, 1, "fsl,qman-pfdr",
	    &pfdr_pa, &pfdr_sz);

	bzero((void *)PHYS_TO_DMAP(fqd_pa), fqd_sz);
	cpu_flush_dcache((void *)PHYS_TO_DMAP(fqd_pa), fqd_sz);
	/*
	 * FQDs are 64 bytes in size, with 24 bit pointers, so FQIDs are 24
	 * bits, fits fine in a uint32_t.
	 */
	nfqd = fqd_sz / 64;
	qman_total_fqids = nfqd;
	qman_channel_base = qman_channel_pool1;
	qman_fq_list = malloc(nfqd * sizeof(struct qman_fq *), M_QMAN,
	    M_WAITOK);

	error = qman_set_memory(sc, fqd_pa, fqd_sz, QMAN_FQD_BARE);
	if (error != 0 && error != EEXIST)
		goto err;
	error = qman_set_memory(sc, pfdr_pa, pfdr_sz, QMAN_PFDR_BARE);
	if (error != 0 && error != EEXIST)
		goto err;
	if (error == 0) {
		/* Initialize PFDRs if it hasn't been initialized before */
		error = qman_setup_pfdr(sc, pfdr_sz / 64);
		if (error != 0)
			goto err;
		/* Magic constant from documentation */
		bus_write_4(sc->sc_rres, QMAN_PFDR_CFG, 64);
	}

	bus_write_4(sc->sc_rres, QMAN_ERR_ISR, 0xffffffff);
	bus_write_4(sc->sc_rres, QMAN_ERR_IER, 0xffffffff);

	ver = bus_read_4(sc->sc_rres, QMAN_IP_REV_1);
	sc->sc_qman_major = ((ver & IP_MJ_M) >> IP_MJ_S);
	if (sc->sc_qman_major >= 3)
		qman3 = true;

	if (qman3)
		qman_channel_pool1 = QMAN_CHANNEL_POOL1_REV3;

	sc->sc_qman_base_channel = qman_channel_pool1;

	sc->sc_fqalloc =
	    vmem_create("qman-fqalloc", 1, nfqd - 1, 1, 0, M_WAITOK);
	sc->sc_qpalloc =
	    vmem_create("qman-fqalloc", qman_channel_pool1,
	    QMAN_POOL_CHANNELS, 1, 0, M_WAITOK);
	sc->sc_cgalloc = vmem_create("qman->cgalloc", 0, QMAN_CGRS,
	    1, 0, M_WAITOK);

	if (bus_setup_intr(dev, sc->sc_ires, INTR_TYPE_NET, NULL, qman_isr,
	    sc, &sc->sc_intr_cookie) != 0)
		goto err;

	if (error != 0) {
		device_printf(dev, "could not be initialized\n");
		goto err;
	}
	bus_write_4(sc->sc_rres, QMAN_DCP_CFG(0),
	    qman3 ? DCP_CFG_ED_3 : DCP_CFG_ED);
	bus_write_4(sc->sc_rres, QMAN_DCP_CFG(1),
	    qman3 ? DCP_CFG_ED_3 : DCP_CFG_ED);

	bus_write_4(sc->sc_rres, 0xd00, 0x80000322);

	/* TODO: DO we need a taskqueue?  Allocate here if so */

	return (0);

err:
	qman_detach(dev);
	return (ENXIO);
}

int
qman_detach(device_t dev)
{
	struct qman_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_fqalloc != NULL)
		vmem_destroy(sc->sc_fqalloc);
	if (sc->sc_qpalloc != NULL)
		vmem_destroy(sc->sc_qpalloc);
	if (sc->sc_cgalloc != NULL)
		vmem_destroy(sc->sc_cgalloc);

	if (sc->sc_intr_cookie != NULL)
		bus_teardown_intr(dev, sc->sc_ires, sc->sc_intr_cookie);

	if (sc->sc_ires != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->sc_irid, sc->sc_ires);

	if (sc->sc_rres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_rrid, sc->sc_rres);

	free(qman_fq_list, M_QMAN);
	qman_fq_list = NULL;

	return (0);
}

int
qman_suspend(device_t dev)
{

	return (0);
}

int
qman_resume(device_t dev)
{

	return (0);
}

int
qman_shutdown(device_t dev)
{

	return (0);
}

int
qman_alloc_channel(void)
{
	struct qman_softc *sc = qman_sc;
	vmem_addr_t channel;

	vmem_alloc(sc->sc_qpalloc, 1, M_BESTFIT | M_WAITOK, &channel);

	return (channel);
}

void
qman_free_channel(int channel)
{
	struct qman_softc *sc = qman_sc;

	vmem_free(sc->sc_qpalloc, channel, 1);
}

/**
 * @group QMan API functions implementation.
 * @{
 */

struct qman_fq *
qman_fq_from_index(uint32_t fqid)
{
	if (fqid > qman_total_fqids)
		return (NULL);
	return (qman_fq_list[fqid]);
}

/* Allocate and initialize an FQ Range */
struct qman_fq *
qman_fq_create(uint32_t fqids_num, int channel, uint8_t wq,
    bool force_fqid, uint32_t fqid_or_align, bool init_parked,
    bool hold_active, bool prefer_in_cache, bool congst_avoid_ena,
    void *congst_group, int8_t overhead_accounting_len,
    uint32_t tail_drop_threshold)
{
	union qman_mc_command cmd;
	struct qman_softc *sc;
	union qman_mc_result *res;
	struct qman_fq *fqh;
	device_t portal;
	vmem_addr_t fqid_base;
	uint8_t rslt;

	sc = qman_sc;

	if (fqids_num != 1) {
		device_printf(sc->sc_dev,
		    "Only one fq allocation allowed currently\n");
		return (NULL);
	}

	bzero(&cmd, sizeof(cmd));
	vmem_alloc(sc->sc_fqalloc, fqids_num, M_BESTFIT | M_WAITOK, &fqid_base);
	cmd.init_fq.fqid = fqid_base;
	cmd.init_fq.count = fqids_num - 1;
	cmd.init_fq.dest_chan = channel;
	cmd.init_fq.dest_wq = wq;
	cmd.init_fq.we_mask = QCSP_INIT_FQ_WE_DEST_WQ | QCSP_INIT_FQ_WE_FQ_CTRL;
	if (init_parked)
		cmd.init_fq.verb = QCSP_VERB_INIT_FQ_PARK;
	else
		cmd.init_fq.verb = QCSP_VERB_INIT_FQ_SCHED;
	cmd.init_fq.fq_ctrl = (prefer_in_cache ? QM_FQCTRL_LIC : 0) |
	    (hold_active ? QM_FQCTRL_HOLDACTIVE : 0) |
	    (congst_avoid_ena ? QM_FQCTRL_AVOIDBLOCK : 0);

	critical_enter();

	/* Ensure we have got QMan port initialized */
	portal = DPCPU_GET(qman_affine_portal);
	res = QMAN_PORTAL_MC_SEND_RAW(portal, &cmd);

	rslt = 0;
	if (res != NULL)
		rslt = res->init_fq.rslt;

	critical_exit();
	if (res == NULL || rslt != QMAN_MC_RES_OK) {
		vmem_free(sc->sc_fqalloc, fqid_base, fqids_num);
		goto err;
	}

	fqh = malloc(sizeof(*fqh), M_QMAN, M_WAITOK | M_ZERO);
	fqh->fqid = fqid_base;

	qman_fq_list[fqid_base] = fqh;

	return (fqh);

err:

	return (NULL);
}

static int
qman_fq_retire(device_t portal, struct qman_fq *fq)
{
	union qman_mc_command cmd;
	union qman_mc_result *rr;

	bzero(&cmd, sizeof(cmd));

	cmd.alter_fqs.verb = QCSP_VERB_ALTER_FQ_RETIRE;
	cmd.alter_fqs.fqid = fq->fqid;
	rr = QMAN_PORTAL_MC_SEND_RAW(portal, &cmd);
	if (rr == NULL)
		return (ETIMEDOUT);

	if (rr->alter_fqs.rslt == QMAN_MC_RES_OK) {
		if (rr->alter_fqs.fqs & QMAN_MC_AFQS_NE) {
			/* TODO: Drain.... */
		}
		return (0);
	}

	return (0);
}

int
qman_fq_free(struct qman_fq *fq)
{
	struct qman_softc *sc;
	int error;

	sc = qman_sc;

	critical_enter();
	error = qman_fq_retire(DPCPU_GET(qman_affine_portal), fq);
	/* TODO: Take FQ out of service. */
	critical_exit();
	if (error != 0)
		return (error);
	vmem_free(sc->sc_fqalloc, fq->fqid, 1);
	qman_fq_list[fq->fqid] = NULL;
	free(fq, M_QMAN);

	return (0);
}

int
qman_fq_register_cb(struct qman_fq *fq, qman_cb_dqrr callback,
    void *ctx)
{
	fq->cb.dqrr = callback;
	fq->cb.ctx = ctx;

	return (0);
}

int
qman_fq_enqueue(struct qman_fq *fq, struct dpaa_fd *frame)
{
	struct qman_softc *sc;
	int error;
	void *portal;

	sc = qman_sc;
	critical_enter();

	/* Ensure we have got QMan port initialized */
	portal = DPCPU_GET(qman_affine_portal);
	if (portal == NULL) {
		device_printf(sc->sc_dev, "could not setup QMan portal\n");
		critical_exit();
		return (ENXIO);
	}

	error = QMAN_PORTAL_ENQUEUE(portal, fq, frame);

	critical_exit();

	return (error);
}

uint32_t
qman_fq_get_fqid(struct qman_fq *fq)
{
	return (fq->fqid);
}


uint32_t
qman_fq_get_counter(struct qman_fq *fq, int counter)
{
	union qman_mc_result *cmd_res;
	union qman_mc_command command;
	device_t portal;
	u_int ret = 0;

	bzero(&command, sizeof(command));
	command.query_fq_np.verb = QCSP_VERB_QUERY_FQ_NP;
	command.query_fq_np.fqid = fq->fqid;
	critical_enter();
	portal = DPCPU_GET(qman_affine_portal);
	cmd_res = QMAN_PORTAL_MC_SEND_RAW(portal, &command);
	if (counter == QMAN_COUNTER_FRAME)
		ret = cmd_res->query_fq_np.frm_cnt;
	else if (counter == QMAN_COUNTER_BYTES)
		ret = cmd_res->query_fq_np.byte_cnt;

	critical_exit();

	return (ret);
}

void
qman_set_sdest(uint16_t channel, int cpu)
{
	struct qman_softc *sc = qman_sc;
	uint32_t reg;

	if (sc->sc_qman_major >= 3) {
		reg = bus_read_4(sc->sc_rres, QCSP_IO_CFG_3(channel));
		reg &= IO_CFG_SDEST_M;
		reg |= (cpu << IO_CFG_SDEST_S);
		bus_write_4(sc->sc_rres, QCSP_IO_CFG_3(channel), reg);
	} else {
		reg = bus_read_4(sc->sc_rres, QCSP_IO_CFG(channel));
		reg &= IO_CFG_SDEST_M;
		reg |= (cpu << IO_CFG_SDEST_S);
		bus_write_4(sc->sc_rres, QCSP_IO_CFG(channel), reg);
	}
}

/*
 * TODO: add polling and/or congestion support.
 */

/** @} */
