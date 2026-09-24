/*
 * Copyright 2026 Justin Hibbits <jhibbits@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "sec_var.h"

/*
 * A job that doesn't complete in 5 seconds (should take microseconds or less)
 * is considered a failure.
 */
#define	SEC_JOB_TIMEOUT	(5 * hz)

/*
 * Job Ring register offsets, relative to the JR's base within SEC's
 * CCSR window.
 */
#define	JR_IRBAR_MS	0x00	/* Input ring base, upper (64-bit reg) */
#define	JR_IRBAR_LS	0x04	/* Input ring base, lower */
#define	JR_IRSR		0x0c	/* Input ring size (ring entries) */
#define	JR_IRSAR	0x14	/* Input ring slots available (add-to) */
#define	JR_IRJAR	0x1c	/* Input ring jobs added (bump on enqueue) */
#define	JR_ORBAR_MS	0x20	/* Output ring base, upper */
#define	JR_ORBAR_LS	0x24	/* Output ring base, lower */
#define	JR_ORSR		0x2c	/* Output ring size */
#define	JR_ORJRR	0x34	/* Output ring jobs removed */
#define	JR_ORSFR	0x3c	/* Output ring slots full */
#define	JR_JRSTAR	0x44	/* Output status (per-job termination) */
#define	JR_JRINTR	0x4c	/* Interrupt status (W1C) */
#define	  JRINTR_JRI	  0x00000001	/* JR interrupt asserted */
#define	  JRINTR_JRE	  0x00000002	/* JR error */
/*
 * HALT tracks a flush requested through JRCR: 01b while SEC
 * is still draining, 10b once every job has reached the output ring.
 * Writing the field's high bit clears it and lets the ring run again.
 */
#define	  JRINTR_HALT_M	  0x0000000c
#define	  JRINTR_HALT_ONGOING 0x00000004
#define	  JRINTR_HALT_DONE  0x00000008
#define	JR_JRCFGR_MS	0x50	/* Configuration, upper */
#define	JR_JRCFGR_LS	0x54	/* Configuration, lower */
#define	  JRCFGR_LS_IMSK  0x00000001	/* Mask interrupts (1=masked) */
#define	  JRCFGR_LS_ICEN  0x00000002	/* Interrupt coalescing enable */
#define	JR_JRCR		0x6c	/* Command: flush/reset */
#define	  JRCR_RESET	  0x00000001	/* Flush, or reset if halted */
#define	JR_IRRIR	0x5c	/* Input ring read index (RO) */
#define	JR_ORWIR	0x64	/* Output ring write index (RO) */

#define	JR_RING_SIZE	16	/* power of 2, small for scaffolding */
#define	JR_RING_MASK	(JR_RING_SIZE - 1)

#define	JR_RD4(sec, jr, off)	bus_read_4(sec->sc_rres, jr->jr_off + off)
#define	JR_WR4(sec, jr, off, v)						\
	bus_write_4(sec->sc_rres, jr->jr_off + off, v)

static void sec_jr_intr(void *arg);

#define	FOREACH_JOB_RING(node) \
	for (phandle_t child = OF_child(node); child != 0; 		\
	    child = OF_peer(child))					\
		if ((ofw_bus_node_is_compatible(child,			\
		    "fsl,sec-v5.0-job-ring") ||				\
		    ofw_bus_node_is_compatible(child,			\
		    "fsl,sec-v4.0-job-ring")) &&			\
		    ofw_bus_node_status_okay(child) &&			\
		    OF_getproplen(child, "reg") == 2 * sizeof(pcell_t))
/*
 * Job Ring helpers.
 */

static int
sec_jr_count(struct sec_softc *sc)
{
	phandle_t node = ofw_bus_get_node(sc->sc_dev);
	int n = 0;

	FOREACH_JOB_RING(node)
		n++;

	return (n);
}

static void
sec_jr_dma_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	vm_paddr_t *pa = arg;
	*pa = error == 0 && nsegs == 1 ? segs[0].ds_addr : 0;
}

static int
sec_jr_irq_setup(struct sec_softc *sc, struct sec_jr *jr, u_int idx)
{
	device_t dev = sc->sc_dev;
	struct resource_list *rl;
	phandle_t iparent;
	pcell_t *cells;
	int ncells, irqnum;

	if (jr->jr_node == 0)
		return (ENXIO);

	if (ofw_bus_intr_by_rid(dev, jr->jr_node, 0, &iparent, &ncells,
	    &cells) != 0)
		return (ENXIO);
	irqnum = ofw_bus_map_intr(dev, iparent, ncells, cells);
	OF_prop_free(cells);
	if (irqnum <= 0)
		return (ENXIO);

	rl = BUS_GET_RESOURCE_LIST(device_get_parent(dev), dev);
	jr->jr_irid = 1 + idx;	/* rid 0 is the SEC top-level error IRQ */
	resource_list_add(rl, SYS_RES_IRQ, jr->jr_irid, irqnum, irqnum, 1);

	jr->jr_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &jr->jr_irid, RF_ACTIVE);
	if (jr->jr_ires == NULL)
		return (ENXIO);

	if (bus_setup_intr(dev, jr->jr_ires, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, sec_jr_intr, jr, &jr->jr_icookie) != 0)
		return (ENXIO);

	/*
	 * Enable JR interrupts (IMSK=0, ICEN=0 = fire on every completion).
	 * Reset default is already IMSK=0, but be explicit.
	 */
	JR_WR4(sc, jr, JR_JRCFGR_LS, 0);

	return (0);
}

/*
 * Watchdog for a wedged ring.  Nothing else reclaims a job that never
 * reaches the output ring, so its caller would wait forever.
 *
 * Writing JRCR[RESET] while RESET reads 0 flushes the ring: jobs already
 * in the holding tanks or DECOs are terminated onto the output ring with
 * an error status, and the ordinary completion path reclaims them.  So
 * this only starts the flush and later clears HALT.  Jobs merely stalled
 * in the input ring resume from there.
 */
static void
sec_jr_watchdog(void *arg)
{
	struct sec_jr *jr = arg;
	struct sec_softc *sc = jr->jr_sc;
	struct sec_job *job;
	uint32_t intr;

	if (jr->jr_dying)
		return;

	if (jr->jr_flushing) {
		intr = JR_RD4(sc, jr, JR_JRINTR);
		if ((intr & JRINTR_HALT_M) == JRINTR_HALT_DONE) {
			JR_WR4(sc, jr, JR_JRINTR, JRINTR_HALT_DONE);
			jr->jr_flushing = false;
			device_printf(sc->sc_dev,
			    "job ring at %#x resumed after flush\n",
			    jr->jr_off);
		}
	} else if ((job = TAILQ_FIRST(&jr->jr_active)) != NULL &&
	    (int)(ticks - job->job_deadline) >= 0) {
		device_printf(sc->sc_dev,
		    "job ring at %#x stalled with %u job%s outstanding, "
		    "flushing\n", jr->jr_off, jr->jr_inflight,
		    jr->jr_inflight != 1 ? "s" : "");
		JR_WR4(sc, jr, JR_JRCR, JRCR_RESET);
		jr->jr_flushing = true;
	}

	callout_reset(&jr->jr_wdog, hz, sec_jr_watchdog, jr);
}

/* A per-job non-zero status arrives in the OR entry, not through JRE. */
static void
sec_jr_intr(void *arg)
{
	struct sec_jr *jr = arg;
	struct sec_softc *sc = jr->jr_sc;
	struct sec_or_entry done[JR_RING_SIZE];
	uint32_t i, intr, n, tail;
	int blocked;

	mtx_lock(&jr->jr_lock);

	intr = JR_RD4(sc, jr, JR_JRINTR);
	if ((intr & (JRINTR_JRI | JRINTR_JRE)) == 0) {
		mtx_unlock(&jr->jr_lock);
		return;
	}
	if ((intr & JRINTR_JRE) != 0)
		device_printf(sc->sc_dev, "JR error, JRINTR=%#x\n", intr);

	/*
	 * Acknowledge before draining.  A job completing between the ORSFR
	 * read and the W1C would otherwise have its interrupt cleared along
	 * with the one being serviced, and would sit there with nothing
	 * left to raise it again.  Acknowledging first costs at worst a
	 * spurious interrupt that finds the ring empty.
	 */
	JR_WR4(sc, jr, JR_JRINTR, intr & (JRINTR_JRI | JRINTR_JRE));

	/*
	 * Completion has to run with jr_lock dropped, since crypto_done()
	 * can dispatch the next request straight back into sec_process().
	 */
	n = JR_RD4(sc, jr, JR_ORSFR);
	if (n > JR_RING_SIZE)
		n = JR_RING_SIZE;
	for (i = 0; i < n; i++) {
		tail = (jr->jr_or_tail + i) & JR_RING_MASK;
		done[i] = jr->jr_or[tail];
		TAILQ_REMOVE(&jr->jr_active, (struct sec_job *)
		    PHYS_TO_DMAP((vm_paddr_t)done[i].desc_addr), job_link);
	}
	if (n != 0) {
		/* Finish reading the entries before freeing their slots. */
		atomic_thread_fence_rel();
		jr->jr_or_tail += n;
		JR_WR4(sc, jr, JR_ORJRR, n);
		jr->jr_inflight -= n;
	}

	blocked = 0;
	if (jr->jr_blocked != 0 && jr->jr_inflight < JR_RING_SIZE) {
		blocked = jr->jr_blocked;
		jr->jr_blocked = 0;
	}

	mtx_unlock(&jr->jr_lock);

	if (blocked != 0)
		crypto_unblock(sc->sc_cid, blocked);

	for (i = 0; i < n; i++)
		sec_complete_one(sc, done[i].desc_addr, done[i].status);
}

static int
sec_jr_init(struct sec_softc *sc, struct sec_jr *jr)
{
	void *ring_va;
	size_t ir_bytes = JR_RING_SIZE * sizeof(uint64_t);
	size_t or_bytes = JR_RING_SIZE * sizeof(struct sec_or_entry);
	size_t total = ir_bytes + or_bytes;

	mtx_init(&jr->jr_lock, device_get_nameunit(sc->sc_dev), NULL, MTX_DEF);
	TAILQ_INIT(&jr->jr_active);
	callout_init_mtx(&jr->jr_wdog, &jr->jr_lock, 0);

	if (bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 64, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    total, 1, total, BUS_DMA_ALLOCNOW, NULL, NULL,
	    &jr->jr_ring_tag) != 0)
		return (ENOMEM);
	if (bus_dmamem_alloc(jr->jr_ring_tag, &ring_va,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &jr->jr_map) != 0)
		return (ENOMEM);

	jr->jr_ir = ring_va;
	jr->jr_or = (struct sec_or_entry *)((uint8_t *)ring_va + ir_bytes);

	if (bus_dmamap_load(jr->jr_ring_tag, jr->jr_map, ring_va,
	    total, sec_jr_dma_cb, &jr->jr_ir_pa,
	    BUS_DMA_NOWAIT) != 0 || jr->jr_ir_pa == 0)
		return (ENOMEM);
	jr->jr_or_pa = jr->jr_ir_pa + ir_bytes;

	JR_WR4(sc, jr, JR_IRBAR_MS, (uint32_t)(jr->jr_ir_pa >> 32));
	JR_WR4(sc, jr, JR_IRBAR_LS, (uint32_t)jr->jr_ir_pa);
	JR_WR4(sc, jr, JR_IRSR, JR_RING_SIZE);

	JR_WR4(sc, jr, JR_ORBAR_MS, (uint32_t)(jr->jr_or_pa >> 32));
	JR_WR4(sc, jr, JR_ORBAR_LS, (uint32_t)jr->jr_or_pa);
	JR_WR4(sc, jr, JR_ORSR, JR_RING_SIZE);

	/* Enable the ring by writing IRSAR = ring size (all slots free). */
	JR_WR4(sc, jr, JR_IRSAR, JR_RING_SIZE);

	mtx_lock(&jr->jr_lock);
	callout_reset(&jr->jr_wdog, hz, sec_jr_watchdog, jr);
	mtx_unlock(&jr->jr_lock);

	return (0);
}

int
sec_init_rings(struct sec_softc *sc)
{
	uint32_t reg[2];
	int err, i, njrs;

	njrs = sec_jr_count(sc);
	if (njrs == 0)
		return (0);

	sc->sc_jr = mallocarray(njrs, sizeof(struct sec_jr), M_SEC,
	    M_WAITOK | M_ZERO);

	i = 0;
	FOREACH_JOB_RING(ofw_bus_get_node(sc->sc_dev)) {
		struct sec_jr *jr = &sc->sc_jr[i];

		OF_getencprop(child, "reg", reg, sizeof(reg));
		jr->jr_sc = sc;
		jr->jr_node = child;
		/* Offset within SEC's CCSR window. */
		jr->jr_off = reg[0];

		err = sec_jr_init(sc, jr);
		if (err != 0)
			goto fail;

		err = sec_jr_irq_setup(sc, jr, i);
		if (err != 0) {
			device_printf(sc->sc_dev,
			    "could not install JR%u interrupt\n", i);
			goto fail;
		}
		i++;
	}

	sc->sc_njr = njrs;

	return (njrs);

fail:
	/*
	 * Teardown copes with a partly built ring, so running it over the
	 * whole array also cleans up the one that failed.
	 */
	for (i = 0; i < njrs; i++)
		sec_jr_teardown(sc, &sc->sc_jr[i]);
	free(sc->sc_jr, M_SEC);
	sc->sc_jr = NULL;

	return (0);
}

void
sec_jr_teardown(struct sec_softc *sc, struct sec_jr *jr)
{

	if (mtx_initialized(&jr->jr_lock)) {
		mtx_lock(&jr->jr_lock);
		jr->jr_dying = true;
		callout_stop(&jr->jr_wdog);
		mtx_unlock(&jr->jr_lock);
		callout_drain(&jr->jr_wdog);
	}

	if (jr->jr_ring_tag != NULL) {
		/* Halt the JR by writing 0 to input ring size. */
		if (sc->sc_rres != NULL)
			JR_WR4(sc, jr, JR_IRSR, 0);

		if (jr->jr_ir != NULL) {
			bus_dmamap_unload(jr->jr_ring_tag, jr->jr_map);
			bus_dmamem_free(jr->jr_ring_tag, jr->jr_ir,
			    jr->jr_map);
		}
		bus_dma_tag_destroy(jr->jr_ring_tag);
	}

	/* sec_jr_init() can fail after taking the lock but before the tag. */
	if (mtx_initialized(&jr->jr_lock))
		mtx_destroy(&jr->jr_lock);
}

int
sec_jr_submit_job(struct sec_softc *sc, struct sec_jr *jr, struct sec_job *job)
{
	vm_paddr_t job_pa;
	int slot;

	job_pa = pmap_kextract((vm_offset_t)job);
	mtx_lock(&jr->jr_lock);

	if (jr->jr_inflight >= JR_RING_SIZE) {
		jr->jr_blocked = CRYPTO_SYMQ;
		mtx_unlock(&jr->jr_lock);
		return (ERESTART);
	}

	slot = jr->jr_ir_head & JR_RING_MASK;
	jr->jr_ir[slot] = (uint64_t)job_pa;
	jr->jr_ir_head++;
	jr->jr_inflight++;
	job->job_deadline = ticks + SEC_JOB_TIMEOUT;
	TAILQ_INSERT_TAIL(&jr->jr_active, job, job_link);
	/* The ring entry must be visible before the doorbell. */
	atomic_thread_fence_rel();
	JR_WR4(sc, jr, JR_IRJAR, 1);

	mtx_unlock(&jr->jr_lock);

	return (0);
}
