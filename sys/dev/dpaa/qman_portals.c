/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/sched.h>
#include <ddb/ddb.h>

#include <machine/bus.h>
#include <machine/tlb.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include "qman.h"
#include "portals.h"
#include "qman_var.h"


/* Cache-enabled registers */
#define	QCSP_EQCR_N(n)		(0x0000 + (n * 64))
#define	QMAN_EQCR_COUNT		8
#define	QCSP_DQRR_N(n)		(0x1000 + (n * 64))
#define	QMAN_DQRR_COUNT		16
#define	QCSP_MR_N(n)		(0x2000 + (n * 64))
#define	QMAN_MR_COUNT		8
#define	QCSP_CR			0x3800
#define	QCSP_RR(n)		(0x3900 + 0x40 * (n))

#define	QCSP_EQCR_PI_CENA	0x0000
#define	  EQCR_PI_VP		  0x00000008
#define	  EQCR_PI_PI_M		  0x00000007
#define	QCSP_EQCR_CI_CENA	0x0004
#define	  EQCR_CI_C		  0x00000008
#define	  EQCR_CI_CI_M		  0x00000007
#define	QCSP_DQRR_PI_CENA	0x0000
#define	  DQRR_PI_VP		  0x00000010
#define	  DQRR_PI_PI_M		  0x0000000f
#define	QCSP_DQRR_CI_CENA	0x0004
#define	  DQRR_CI_C		  0x00000010
#define	  DQRR_CI_CI_M		  0x0000000f

#define	QMAN_MC_VERB_VBIT	0x80


/* Cache-inhibited registers */
#define	QCSP_EQCR_PI_CINH	0x0000
#define	QCSP_EQCR_CI_CINH	0x0004
#define	QCSP_DQRR_PI_CINH	0x0040
#define	QCSP_DQRR_CI_CINH	0x0044
#define	QCSP_EQCR_ITR		0x0008
#define	QCSP_DQRR_ITR		0x0048
#define	QCSP_DQRR_SDQCR		0x0054
#define	  SDQCR_SS		  0x40000000
#define	  SDQCR_FC		  0x20000000
#define	  SDQCR_DP		  0x10000000
#define	  SDQCR_DCT_NUL		  0x00000000
#define	  SDQCR_DCT_PRI_PREC	  0x01000000
#define	  SDQCR_DCT_ACTIVE_WQ	  0x02000000
#define	  SDQCR_DCT_ACTIVE_FQ_O	  0x03000000
#define	  SDQCR_DCT_M		  0x03000000
#define	  SDQCR_TOKEN_M		  0x00ff0000
#define	  SDQCR_TOKEN_S		  16
#define	  DQRR_DQ_SRC_M		  0x0000ffff
#define	  DQRR_DQ_SRC_DCP	  0x00008000
#define	  SDQCR_DQ_SRC_CHAN(n)	  (0x8000 >> (n + 1))
#define	QCSP_DQRR_VDQCR		0x0058
#define	QCSP_DQRR_PDQCR		0x005c
#define	QCSP_MR_ITR		0x0088
#define	QCSP_CFG		0x0100
#define	  CFG_EST_M		  0x70000000
#define	  CFG_EST_S		  28
#define	  CFG_EP		  0x04000000
#define	  CFG_EPM_M		  0x03000000
#define	  CFG_EPM_PI_CI		  0x00000000
#define	  CFG_EPM_PI_CE		  0x01000000
#define	  CFG_EPM_VB1		  0x02000000
#define	  CFG_EPM_VB2		  0x03000000
#define	  CFG_DQRR_MF_M		  0x00f00000
#define	  CFG_DQRR_MF_S		  20
#define	  CFG_DP		  0x00040000
#define	  CFG_DCM_C_M		  0x00030000
#define	  CFG_DCM_CI_CI		  0x00000000
#define	  CFG_DCM_CI_CE		  0x00010000
#define	  CFG_DCM_DCA1		  0x00020000
#define	  CFG_DCM_DCA2		  0x00030000
#define	  CFG_SD		  0x00000200
#define	  CFG_MM		  0x00000100
#define	  CFG_RE		  0x00000080
#define	  CFG_RP		  0x00000040
#define	  CFG_SE		  0x00000020
#define	  CFG_SP		  0x00000010
#define	  CFG_SDEST_M		  0x00000007
#define	QCSP_ISR		0x0e00
#define	  QM_PIRQ_CSCI		  0x00100000
#define	  QM_PIRQ_EQCI		  0x00080000
#define	  QM_PIRQ_EQRI		  0x00040000
#define	  QM_PIRQ_DQRI		  0x00020000
#define	  QM_PIRQ_MRI		  0x00010000
#define	  QM_PIRQ_DQ_AVAIL_M	  0x0000ffff
#define	QCSP_IER		0x0e04
#define	QCSP_ISDR		0x0e08
#define	QCSP_IIR		0xe0c

#define	QM_EQCR_VERB_CMD_ENQUEUE	0x01
#define	QM_EQCR_VERB_BIT_INT		0x04

#define	DEF_SDQCR_TOKEN		0xab

static void qman_portal_loop_rings(struct qman_portal_softc *sc);
static void qman_portal_isr(void *);

DPCPU_DEFINE(device_t, qman_affine_portal);
DPAA_RING(qman_eqcr, QMAN_EQCR_COUNT, QCSP_EQCR_PI_CENA, QCSP_EQCR_CI_CENA,
		QCSP_EQCR_PI_CINH, QCSP_EQCR_CI_CINH);
DPAA_RING(qman_dqrr, QMAN_DQRR_COUNT, QCSP_DQRR_PI_CENA, QCSP_DQRR_CI_CENA,
		QCSP_DQRR_PI_CINH, QCSP_DQRR_CI_CINH);

/*
 * pmode: one of the CFG_EPM constants.
 * stash_prio: 0 or CFG_EP
 * stash_thresh: 0-7
 */
static int
qman_eqcr_init(struct qman_portal_softc *sc, int pmode, u_int stash_thresh,
    u_int stash_prio)
{
	struct resource *regs = sc->sc_base.sc_mres[1];
	uint32_t reg;

	sc->sc_eqcr.ring =
	    (struct qman_eqcr_entry *)(sc->sc_base.sc_ce_va + QCSP_EQCR_N(0));
	qman_eqcr_ring_init(&sc->sc_eqcr, &sc->sc_base);
	reg = bus_read_4(regs, QCSP_CFG);
	reg &= 0x00ffffff;
	reg |= pmode;
	reg |= ((stash_thresh << CFG_EST_S) & CFG_EST_M);
	reg |= stash_prio;

	bus_write_4(regs, QCSP_CFG, reg);
	return (0);
}

static int
qman_dqrr_init(struct qman_portal_softc *sc)
{
	struct resource *regs = sc->sc_base.sc_mres[1];
	uint32_t reg;

	/* Dequeue from the direct-connect channel and pool 0, up to 3 frames */
	bus_write_4(regs, QCSP_DQRR_SDQCR,
	    SDQCR_FC | SDQCR_DP | SDQCR_DCT_PRI_PREC |
	    (DEF_SDQCR_TOKEN << SDQCR_TOKEN_S) |
	    DQRR_DQ_SRC_DCP | SDQCR_DQ_SRC_CHAN(0));
	bus_write_4(regs, QCSP_DQRR_VDQCR, 0);
	bus_write_4(regs, QCSP_DQRR_PDQCR, 0);

	sc->sc_dqrr.ring =
	    (struct qman_dqrr_entry *)(sc->sc_base.sc_ce_va + QCSP_DQRR_N(0));
	qman_dqrr_ring_init(&sc->sc_dqrr, &sc->sc_base);

	/* Set DQRR max fill to 15 */
	reg = bus_read_4(regs, QCSP_CFG);
	reg |= (0xf << CFG_DQRR_MF_S);
	bus_write_4(regs, QCSP_CFG, reg);

	for (int i = 0; i < QMAN_DQRR_COUNT; i++)
		__asm __volatile ("dcbi 0,%0" :: "r"(&sc->sc_dqrr.ring[i]) : "memory");

	return (0);
}

int
qman_portal_attach(device_t dev, int cpu)
{
	struct qman_portal_softc *sc = device_get_softc(dev);
	union qman_mc_command *cr;
	pcell_t cell;
	phandle_t node;

	sc->sc_base.sc_cpu = cpu;
	dpaa_portal_alloc_res(dev, cpu);

	qman_eqcr_init(sc, CFG_EPM_VB1, 0, 0);
	qman_dqrr_init(sc);
	bus_setup_intr(dev, sc->sc_base.sc_ires, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, qman_portal_isr, sc, &sc->sc_base.sc_intr_cookie);
	bus_bind_intr(dev, sc->sc_base.sc_ires, cpu);

	node = ofw_bus_get_node(dev);
	if (OF_getencprop(node, "cell-index", &cell, sizeof(cell)) <= 0) {
		device_printf(dev, "missing 'cell-index' property\n");
		return (ENXIO);
	}
	sc->sc_affine_channel = cell;
	DPCPU_ID_SET(cpu, qman_affine_portal, dev);
	bus_write_4(sc->sc_base.sc_mres[1], QCSP_IER,
	    QM_PIRQ_EQCI | QM_PIRQ_EQRI | QM_PIRQ_MRI | QM_PIRQ_CSCI |
	    QM_PIRQ_DQRI);
	bus_write_4(sc->sc_base.sc_mres[1], QCSP_ISDR, 0);

	/* Initialize the MC polarity bit, it may not be 0. */
	cr = (union qman_mc_command *)(sc->sc_base.sc_ce_va + QCSP_CR);
	sc->sc_mc.polarity =
	    (cr->common.verb & QMAN_MC_VERB_VBIT) ^ QMAN_MC_VERB_VBIT;
	/* TODO: LIODN.  Fake it for now */

	qman_set_sdest(sc->sc_affine_channel, cpu);

	return (0);
}


int
qman_portal_detach(device_t dev)
{
	struct qman_portal_softc *sc;
	int i;

	sc = device_get_softc(dev);

	/* TODO: Unmap TLB regions */
	thread_lock(curthread);
	sched_bind(curthread, sc->sc_base.sc_cpu);
	thread_unlock(curthread);

	if (sc->sc_base.sc_ires != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_base.sc_ires);

	for (i = 0; i < nitems(sc->sc_base.sc_mres); i++) {
		if (sc->sc_base.sc_mres[i] != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY,
			    i, sc->sc_base.sc_mres[i]);
	}
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);


	return (0);
}

static void
qman_portal_isr(void *arg)
{
	struct qman_portal_softc *sc = arg;

	qman_portal_loop_rings(sc);
}

int
qman_portal_fq_enqueue(device_t dev, struct qman_fq *fq, struct dpaa_fd *frame)
{
	struct qman_portal_softc *sc = device_get_softc(dev);
	struct qman_eqcr_entry *eqcr;

	/* Get available... */
	eqcr = qman_eqcr_start(&sc->sc_eqcr, &sc->sc_base);
	if (eqcr == NULL)
		return (EBUSY);
	eqcr->fd = *frame;
	eqcr->fqid = fq->fqid;
	qman_eqcr_commit(&sc->sc_eqcr, QM_EQCR_VERB_CMD_ENQUEUE);

	return (0);
}

static int
qman_portal_loop_dqrr(struct qman_portal_softc *sc)
{
	struct qman_dqrr_entry *dqrr;
	struct qman_dqrr_entry *base;
	struct qman_fq *fq;
	int ci = bus_read_4(sc->sc_base.sc_mres[1], QCSP_DQRR_CI_CINH) &
	    DQRR_CI_CI_M;
	int pi = bus_read_4(sc->sc_base.sc_mres[1], QCSP_DQRR_PI_CINH) &
	    DQRR_PI_PI_M;

	base = sc->sc_dqrr.ring;
	do {
		dqrr = &base[ci];
		dpaa_flush_line(dqrr);
		dpaa_touch_line(dqrr);
		if ((dqrr->stat & QMAN_DQRR_STAT_HAS_FRAME)) {
			fq = qman_fq_from_index(dqrr->fqid);
			if (fq != NULL && fq->cb.dqrr != NULL) {
				fq->cb.dqrr(sc->sc_base.sc_dev, fq,
				    &dqrr->fd, fq->cb.ctx);
			}
		} else
			break;
		ci = (ci + 1) & DQRR_CI_CI_M;
		bus_write_4(sc->sc_base.sc_mres[1], QCSP_DQRR_CI_CINH, ci);
	} while (ci != pi);

	return (0);
}

static void
qman_portal_loop_rings(struct qman_portal_softc *sc)
{
	uint32_t isr;

	isr = bus_read_4(sc->sc_base.sc_mres[1], QCSP_ISR);

	/* Handle DQRR first. */
	if ((isr & QM_PIRQ_DQRI)) {
		qman_portal_loop_dqrr(sc);
	}
	if ((isr & QM_PIRQ_CSCI)) {
	}
	if ((isr & QM_PIRQ_EQRI)) {
		qman_eqcr_update(&sc->sc_eqcr, &sc->sc_base);
	}
	bus_write_4(sc->sc_base.sc_mres[1], QCSP_ISR, isr);
}

/* MC commands */

/* Assumes pinned */
union qman_mc_result *
qman_portal_mc_send_raw(device_t dev, union qman_mc_command *c)
{
	struct qman_portal_softc *sc;
	int res_idx;
	union qman_mc_result *rr;
	union qman_mc_command *cr;
	int timeout = 10000;
	uint8_t verb;

	sc = device_get_softc(dev);

	verb = c->common.verb;
	c->common.verb = 0;
	cr = (union qman_mc_command *)(sc->sc_base.sc_ce_va + QCSP_CR);
	dpaa_zero_line(cr);
	*cr = *c;
	dpaa_lw_barrier();
	cr->common.verb = verb | sc->sc_mc.polarity;
	res_idx = (sc->sc_mc.polarity ? 1 : 0);
	sc->sc_mc.polarity ^= QMAN_MC_VERB_VBIT;
	dpaa_flush_line(cr);
	dpaa_touch_line(cr);

	rr = (union qman_mc_result *)(sc->sc_base.sc_ce_va + QCSP_RR(res_idx));
	for (; timeout > 0; --timeout) {
		dpaa_flush_line(rr);
		if (rr->common.verb != 0)
			break;
	}
	if (timeout == 0)
		return (NULL);
	return (rr);
}

void
qman_portal_static_dequeue_channel(device_t dev, int channel)
{
	struct qman_portal_softc *sc = device_get_softc(dev);
	uint32_t reg;

	reg = bus_read_4(sc->sc_base.sc_mres[1], QCSP_DQRR_SDQCR);
	reg |= (1 << (15 - (channel - qman_channel_base)));
	bus_write_4(sc->sc_base.sc_mres[1], QCSP_DQRR_SDQCR, reg);
}

void
qman_portal_static_dequeue_rm_channel(device_t dev, int channel)
{
	struct qman_portal_softc *sc = device_get_softc(dev);
	uint32_t reg;

	reg = bus_read_4(sc->sc_base.sc_mres[1], QCSP_DQRR_SDQCR);
	reg &= ~(1 << (15 - (channel - qman_channel_base)));
	bus_write_4(sc->sc_base.sc_mres[1], QCSP_DQRR_SDQCR, reg);
}

DB_SHOW_COMMAND(fqid, qman_show_fqid)
{
	union qman_mc_command cmd;
	union qman_mc_result *res;
	union qman_mc_result save_res;
	device_t portal;

	if (!have_addr)
		return;

	bzero(&cmd, sizeof(cmd));
	cmd.query_fq_np.fqid = addr;

	/* Ensure we have got QMan port initialized */
	portal = DPCPU_GET(qman_affine_portal);
	res = qman_portal_mc_send_raw(portal, &cmd);

	if (res != NULL)
		save_res = *res;

	/* Dump all NP fields */
	if (res != NULL && save_res.query_fq_np.rslt == 0xf0) {
		db_printf("FQID: %d\n", (int)addr);
		db_printf("  State: %x\n", save_res.query_fq_np.state);
		db_printf("  Link: %x\n", save_res.query_fq_np.fqd_link);
		db_printf("  ODP_SEQ: %x\n", save_res.query_fq_np.odp_seq);
		db_printf("  ORP_NESN: %x\n", save_res.query_fq_np.orp_nesn);
		db_printf("  ORP_EA_HSEQ: %x\n",
		    save_res.query_fq_np.orp_ea_hseq);
		db_printf("  ORP_EA_TSEQ: %x\n",
		    save_res.query_fq_np.orp_ea_tseq);
		db_printf("  ORP_EA_HPTR: %x\n",
		    save_res.query_fq_np.orp_ea_hptr);
		db_printf("  ORP_EA_TPTR: %x\n",
		    save_res.query_fq_np.orp_ea_tptr);
		db_printf("  pfdr_hptr: %x\n", save_res.query_fq_np.pfdr_hptr);
		db_printf("  pfdr_tptr: %x\n", save_res.query_fq_np.pfdr_tptr);
		db_printf("  IS: %x\n", save_res.query_fq_np.is);
		db_printf("  ICS_SURP: %x\n", save_res.query_fq_np.ics_surp);
		db_printf("  byte_cnt: %x\n", save_res.query_fq_np.byte_cnt);
		db_printf("  frm_cnt: %x\n", save_res.query_fq_np.frm_cnt);
		db_printf("  ra1_sfdr: %x\n", save_res.query_fq_np.ra1_sfdr);
		db_printf("  ra2_sfdr: %x\n", save_res.query_fq_np.ra2_sfdr);
		db_printf("  od1_sfdr: %x\n", save_res.query_fq_np.od1_sfdr);
		db_printf("  od2_sfdr: %x\n", save_res.query_fq_np.od2_sfdr);
		db_printf("  od3_sfdr: %x\n", save_res.query_fq_np.od3_sfdr);
	}
}
