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
#include <sys/cpuset.h>
#include <sys/interrupt.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/sched.h>

#include <machine/bus.h>
#include <machine/tlb.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include "bman.h"
#include "bman_var.h"
#include "portals.h"

#define	BCSP_CFG	0x0100
#define	  CFG_RPM_M	  0x00000003
#define	  CFG_RPM_PI	  0x00000000
#define	  CFG_RPM_PE	  0x00000001
#define	  CFG_RPM_VBM	  0x00000002
#define	BCSP_SCN0	0x0200
#define	BCSP_SCN1	0x0204
#define	BCSP_ISR	0x0e00
#define	BCSP_IER	0x0e04
#define	BCSP_ISDR	0x0e08
#define	  INTR_RCDI	  0x00000004
#define	  INTR_RCRI	  0x00000002
#define	  INTR_BSCN	  0x00000001

#define	BMAN_CE_CR	0x0000
#define	BMAN_CE_RR0	0x0100
#define	BMAN_CE_RR1	0x0140
#define	BMAN_CE_RR(n)	(BMAN_CE_RR0 + 0x40 * (n))
#define	BMAN_CE_RCR	0x1000
#define	BCSP_RCR_PI_CENA	0x3000
#define	BCSP_RCR_CI_CENA	0x3100
#define	BCSP_RCR_PI_CINH	0x000
#define	BCSP_RCR_CI_CINH	0x004

#define	BMAN_MC_VERB_VBIT		0x80
#define	BMAN_MC_VERB_ACQUIRE		0x10
#define	BMAN_MC_VERB_QUERY		0x40
#define	BMAN_RCR_VERB_BPID0		0x20
#define	BMAN_RCR_VERB_BPID_BUF		0x30

struct bman_mc_command {
	uint8_t verb;
	uint8_t cd;
	uint8_t rsvd[62];
};

union bman_mc_result {
	struct {
		uint8_t verb;
		uint8_t cd;
		uint8_t rsvd[62];
	};
	struct {
		uint64_t rsvd_q1[5];
		uint64_t bp_as;
		uint64_t rsvd_q2;
		uint64_t bp_ds;
	};
	struct bman_buffer bufs[8];
};

struct bman_rcr_entry {
	union {
	struct {
		uint8_t verb;
		uint8_t bpid;
		uint8_t rsvd[62];
	};
	struct bman_buffer bufs[8];
	};
};

static void bman_portal_isr(void *arg);

static union bman_mc_result *bman_mc_send(struct bman_portal_softc *p,
    uint8_t verb, uint8_t cd);

DPCPU_DEFINE(struct bman_portal_softc *, bman_affine_portal);

DPAA_RING(bman_rcr, 8, BCSP_RCR_PI_CENA, BCSP_RCR_CI_CENA,
    BCSP_RCR_PI_CINH, BCSP_RCR_CI_CINH);

static uint32_t
bm_ci_read(struct bman_portal_softc *sc, bus_size_t off)
{
	return (bus_read_4(sc->sc_base.sc_mres[1], off));
}

static void
bm_ci_write(struct bman_portal_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->sc_base.sc_mres[1], off, val);
}

int
bman_portal_attach(device_t dev, int cpu)
{
	struct bman_portal_softc *sc = device_get_softc(dev);

	sc->sc_base.sc_cpu = cpu;
	dpaa_portal_alloc_res(dev, cpu);

	bm_ci_write(sc, BCSP_ISDR, 0);
	bm_ci_write(sc, BCSP_IER, INTR_RCRI | INTR_BSCN);
	bus_setup_intr(dev, sc->sc_base.sc_ires, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, bman_portal_isr, sc, &sc->sc_base.sc_intr_cookie);
	bus_bind_intr(dev, sc->sc_base.sc_ires, cpu);

	/* Select valid-bit mode for rings */
	bus_write_4(sc->sc_base.sc_mres[1], BCSP_CFG, CFG_RPM_VBM);
	/* Disable pool depletion notifications. */
	bm_ci_write(sc, BCSP_SCN0, 0);
	bm_ci_write(sc, BCSP_SCN1, 0);

	DPCPU_ID_SET(cpu, bman_affine_portal, sc);

	sc->sc_rcr.ring =
	    (struct bman_rcr_entry *)(sc->sc_base.sc_ce_va + BMAN_CE_RCR);
	bman_rcr_ring_init(&sc->sc_rcr, &sc->sc_base);
	/* Starting MC polarity is always 1 */
	sc->mc.polarity = BMAN_MC_VERB_VBIT;

	return (0);
}

int
bman_portal_detach(device_t dev)
{
	struct bman_portal_softc *sc;
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

static uint64_t
bman_query(struct bman_portal_softc *sc, bool depletion)
{
	union bman_mc_result *mc_res;
	uint64_t res;

	critical_enter();
	mc_res = bman_mc_send(sc, BMAN_MC_VERB_QUERY, 0);
	if (mc_res == NULL)
		goto err;

	if (depletion)
		res = mc_res->bp_ds;
	else
		res = mc_res->bp_as;
	critical_exit();

	return (res);

err:
	critical_exit();
	device_printf(sc->sc_base.sc_dev, "Timeout querying depltetion\n");
	return (0);
}

static void
bman_portal_isr(void *arg)
{
	struct bman_portal_softc *sc = arg;
	uint32_t intrs;

	intrs = bm_ci_read(sc, BCSP_ISR);

	/* Release Command Ring interrupt. */
	if (intrs & INTR_RCRI) {
		bman_rcr_update(&sc->sc_rcr, &sc->sc_base);
	}
	/* Buffer Pool State Change Notification. */
	if (intrs & INTR_BSCN) {
		struct bman_pool *pool;
		uint64_t res = bman_query(sc, true);
		if (__predict_true(res != 0)) {
			int idx = flsll(res);
			pool = sc->sc_pools[64 - idx];
			KASSERT(pool != NULL,
			    ("state change on unassociated bpid %d\n", idx));
			pool->dep_cb(pool->arg, true);
		}
	}

	bm_ci_write(sc, BCSP_ISR, intrs);
}

/* RCR */

int
bman_release(struct bman_pool *pool, const struct bman_buffer *bufs,
    uint8_t count)
{
	struct bman_portal_softc *portal;
	struct bman_rcr_entry *rcr;

	if (count > 8)
		return (EINVAL);

	critical_enter();
	portal = DPCPU_GET(bman_affine_portal);
	rcr = bman_rcr_start(&portal->sc_rcr, &portal->sc_base);
	bzero(rcr, sizeof(*rcr));

	/* This should be safe, because bpid must be less than 256. */
	for (int i = 0; i < count; i++)
		rcr->bufs[i] = bufs[i];
	rcr->bufs[0].bpid = pool->bpid;
	bman_rcr_commit(&portal->sc_rcr, BMAN_RCR_VERB_BPID0 | count);
	critical_exit();

	return (0);
}

/* MC commands */
/* Assumes pinned */
static union bman_mc_result *
bman_mc_send(struct bman_portal_softc *p, uint8_t verb, uint8_t cd)
{
	int res_idx;
	struct bman_mc_command *command;
	union bman_mc_result *rr;
	uintptr_t ce_va = p->sc_base.sc_ce_va;

	command = (struct bman_mc_command *)(ce_va + BMAN_CE_CR);
	dpaa_zero_line(command);
	command->cd = cd;
	dpaa_lw_barrier();
	command->verb = verb | p->mc.polarity;
	res_idx = (p->mc.polarity ? 1 : 0);
	p->mc.polarity ^= BMAN_MC_VERB_VBIT;
	dpaa_flush_line(command);

	rr = (union bman_mc_result *)(ce_va + BMAN_CE_RR(res_idx));
	for (;;) {
		if (rr->verb != 0)
			break;
		dpaa_flush_line(rr);
	}
	return (rr);
}

int
bman_acquire(struct bman_pool *pool, struct bman_buffer *bufs, uint8_t count)
{
	union bman_mc_result *rr;

	if (count > 8 || count == 0)
		return (EINVAL);
	critical_enter();
	rr = bman_mc_send(DPCPU_GET(bman_affine_portal),
	    BMAN_MC_VERB_ACQUIRE | count,
	    pool->bpid);
	critical_exit();

	if (rr == NULL)
		return (ETIMEDOUT);
	if ((rr->verb & ~BMAN_MC_VERB_VBIT) == 0)
		return (ENOMEM);

	memcpy(bufs, rr, count * sizeof(*bufs));

	return (0);
}

/*
 * Enable pool state change notifications on this portal.  This requires the
 * pool to already be configured with the callback to handle state changes.
 */
void
bman_portal_enable_scn(struct bman_portal_softc *sc, struct bman_pool *pool)
{
	uint32_t reg, reg_ptr;

	if (pool->bpid >= 32)
		reg_ptr = BCSP_SCN1;
	else
		reg_ptr = BCSP_SCN0;
	reg = bm_ci_read(sc, reg_ptr);
	reg |= (1 << (31 - pool->bpid));
	bm_ci_write(sc, reg_ptr, reg);
	sc->sc_pools[pool->bpid] = pool;
}
