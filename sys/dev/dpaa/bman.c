/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
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

#include <machine/bus.h>
#include <machine/tlb.h>

#include "bman.h"
#include "dpaa_common.h"
#include "bman_var.h"

#define	BMAN_POOL_SWDET(n)	(0x000 + 4 * (n))
#define	BMAN_POOL_HWDET(n)	(0x100 + 4 * (n))
#define	BMAN_POOL_SWDXT(n)	(0x200 + 4 * (n))
#define	BMAN_POOL_HWDXT(n)	(0x300 + 4 * (n))
#define	FBPR_FP_LWIT	0x804
#define	BMAN_IP_REV_1	0x0bf8
#define	  IP_MAJ_S	  8
#define	  IP_MAJ_M	  0x0000ff00
#define	  IP_MIN_M	  0x000000ff
#define	BMAN_IP_REV_2	0x0bfc
#define	BMAN_FBPR_BARE	0x0c00
#define	BMAN_FBPR_BAR	0x0c04
#define	BMAN_FBPR_AR	0x0c10
#define	BMAN_LIODNR	0x0d08

#define	BMAN_POOL_CONTENT(n)	(0x0600 + 4 * (n))
#define	BMAN_ECSR	0x0a00
#define	BMAN_ECIR	0x0a04
#define	  ECIR_PORTAL(r)  (((r) >> 24) & 0x0f)
#define	  ECIR_VERB(r)	  (((r) >> 16) & 0x07)
#define	  ECIR_R	  0x00080000
#define	  ECIR_POOL(r)	  ((r) & 0x3f)
#define	BMAN_CECR	0x0a34	/* Corruption Error Capture Register */
#define	BMAN_CEAR	0x0a38	/* Corruption Error Address Register */
#define	BMAN_AECR	0x0a34	/* Acces Error Capture Register */
#define	BMAN_AEAR	0x0a38	/* Acces Error Address Register */
#define	BMAN_ERR_ISR	0x0e00
#define	BMAN_ERR_IER	0x0e04
#define	BMAN_ERR_ISDR	0x0e08
#define	  ERR_EMAI	  0x00000040
#define	  ERR_EMCI	  0x00000020
#define	  ERR_IVCI	  0x00000010
#define	  ERR_FLWI	  0x00000008
#define	  ERR_MBEI	  0x00000004
#define	  ERR_SBEI	  0x00000002
#define	  ERR_BSCN	  0x00000001

static MALLOC_DEFINE(M_BMAN, "bman", "DPAA Buffer Manager structures");

static struct bman_softc *bman_sc;

static void
bman_isr(void *arg)
{
	struct bman_softc *sc = arg;
	uint32_t ier, isr, isr_bit;
	uint32_t reg;

	ier = bus_read_4(sc->sc_rres, BMAN_ERR_IER);
	isr = bus_read_4(sc->sc_rres, BMAN_ERR_ISR);

	isr_bit = (isr & ier);
	if (isr_bit == 0)
		goto end;

	if (isr_bit & ERR_EMAI) {
		device_printf(sc->sc_dev, "External memory access error\n");
		reg = bus_read_4(sc->sc_rres, BMAN_AECR);
		if (reg <= 63)
			device_printf(sc->sc_dev, "  pool %d\n", reg);
		else
			device_printf(sc->sc_dev, "  FBPR free list\n");
		reg = bus_read_4(sc->sc_rres, BMAN_AEAR);
		device_printf(sc->sc_dev, "  offset: %#x\n", reg);
	}

	if (isr_bit & ERR_EMCI) {
		device_printf(sc->sc_dev, "External memory corruption error\n");
		reg = bus_read_4(sc->sc_rres, BMAN_CECR);
		if (reg <= 63)
			device_printf(sc->sc_dev, "  pool %d\n", reg);
		else
			device_printf(sc->sc_dev, "  FBPR free list\n");
		reg = bus_read_4(sc->sc_rres, BMAN_CEAR);
		device_printf(sc->sc_dev, "  offset: %#x\n", reg);
	}
	if (isr_bit & ERR_IVCI) {
		reg = bus_read_4(sc->sc_rres, BMAN_ECIR);
		device_printf(sc->sc_dev, "Invalid verb command\n");
		device_printf(sc->sc_dev, "Portal: %d, ring: %s\n",
		    ECIR_POOL(reg), (reg & ECIR_R) ? "RCR" : "Command");
		device_printf(sc->sc_dev, "verb: 0x%02x, pool: %d\n",
		    ECIR_VERB(reg), ECIR_POOL(reg));
	}
	if (isr_bit & (ERR_MBEI | ERR_SBEI)) {
		if (isr_bit & ERR_MBEI)
			device_printf(sc->sc_dev, "Multi-bit ECC error\n");
		if (isr_bit & ERR_MBEI)
			device_printf(sc->sc_dev, "Single-bit ECC error\n");
		/* TODO: Add more error details for ECC errors. */
	}

end:
	bus_write_4(sc->sc_rres, BMAN_ERR_ISR, isr);
}

static void
bman_get_version(struct bman_softc *sc)
{
	uint32_t reg = bus_read_4(sc->sc_rres, BMAN_IP_REV_1);

	sc->sc_major = (reg & IP_MAJ_M) >> IP_MAJ_S;
	sc->sc_minor = (reg & IP_MIN_M);
}

static int
bman_set_memory(struct bman_softc *sc, vm_paddr_t pa, vm_size_t size)
{
	vm_paddr_t bar_pa;
	if ((pa & (size - 1)) != 0 || (size & (size - 1)) != 0) {
		device_printf(sc->sc_dev,
		    "invalid memory configuration: pa: %#jx, size: %#jx\n",
		    (uintmax_t)pa, (uintmax_t)size);
		return (ENXIO);
	}
	bar_pa = bus_read_4(sc->sc_rres, BMAN_FBPR_BARE);
	bar_pa <<= 32;
	bar_pa |= bus_read_4(sc->sc_rres, BMAN_FBPR_BAR);
	if (bar_pa != 0 && bar_pa != pa) {
		device_printf(sc->sc_dev,
		    "attempted to reinitialize BMan with different BAR\n");
		return (ENOMEM);
	} else if (bar_pa == pa)
		return (0);

	bus_write_4(sc->sc_rres, BMAN_FBPR_BARE, pa >> 32);
	bus_write_4(sc->sc_rres, BMAN_FBPR_BAR, pa & 0xffffffff);
	bus_write_4(sc->sc_rres, BMAN_FBPR_AR, ilog2(size) - 1);

	return (0);
}

int
bman_attach(device_t dev)
{
	struct bman_softc *sc;
	vm_paddr_t bp_pa;
	size_t bp_size;
	int bp_count;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	bman_sc = sc;

	/* Allocate resources */
	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    sc->sc_rrid, RF_ACTIVE);
	if (sc->sc_rres == NULL)
		return (ENXIO);

	sc->sc_irid = 0;
	sc->sc_ires = bus_alloc_resource_any(sc->sc_dev, SYS_RES_IRQ,
	    &sc->sc_irid, RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_ires == NULL)
		goto err;

	bman_get_version(sc);
	if (sc->sc_major == 2 && sc->sc_minor == 0)
		bp_count = BMAN_MAX_POOLS_1023;
	else
		bp_count = BMAN_MAX_POOLS;

	/* TODO: LIODN */
	bus_write_4(sc->sc_rres, BMAN_LIODNR, 0);

	sc->sc_vmem = vmem_create("BMan Pools", 0, bp_count, 1, 0, M_WAITOK);

	/* Pool is reserved memory, so no need to track it ourselves. */
	dpaa_map_private_memory(dev, 0, "fsl,bman-fbpr", &bp_pa, &bp_size);
	bman_set_memory(sc, bp_pa, bp_size);

	/* Warn if FBPR drops below 5% total. */
	bus_write_4(sc->sc_rres, FBPR_FP_LWIT, (bp_size / 8) / 20);

	/* Clear interrupt status, and enable all interrupts. */
	bus_write_4(sc->sc_rres, BMAN_ERR_ISR, 0xffffffff);
	bus_write_4(sc->sc_rres, BMAN_ERR_IER, 0xffffffff);
	bus_write_4(sc->sc_rres, BMAN_ERR_ISDR, 0);

	/* Enable the IRQ line now. */
	if (bus_setup_intr(dev, sc->sc_ires, INTR_TYPE_NET, NULL, bman_isr,
	    sc, &sc->sc_icookie) != 0)
		goto err;

	return (0);

err:
	bman_detach(dev);
	return (ENXIO);
}

int
bman_detach(device_t dev)
{
	struct bman_softc *sc;

	sc = device_get_softc(dev);

	if (sc->sc_vmem != NULL)
		vmem_destroy(sc->sc_vmem);
	if (sc->sc_icookie != NULL)
		bus_teardown_intr(dev, sc->sc_ires, sc->sc_icookie);
	if (sc->sc_ires != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    sc->sc_irid, sc->sc_ires);

	if (sc->sc_rres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_rrid, sc->sc_rres);

	return (0);
}

int
bman_suspend(device_t dev)
{

	return (0);
}

int
bman_resume(device_t dev)
{

	return (0);
}

int
bman_shutdown(device_t dev)
{

	return (0);
}

/*
 * BMAN API
 */

struct bman_pool *
bman_new_pool(void)
{
	struct bman_softc *sc;
	vmem_addr_t bpid;
	struct bman_pool *pool;

	sc = bman_sc;
	pool = NULL;

	if (vmem_alloc(sc->sc_vmem, 1, M_FIRSTFIT | M_NOWAIT, &bpid) != 0)
		return (NULL);

	pool = malloc(sizeof(*pool), M_BMAN, M_WAITOK | M_ZERO);

	pool->bpid = bpid;

	return (pool);
}

struct bman_pool *
bman_pool_create(uint8_t *bpid, uint16_t buffer_size, uint16_t max_buffers,
    uint32_t dep_sw_entry, uint32_t dep_sw_exit,
    uint32_t dep_hw_entry, uint32_t dep_hw_exit,
    bm_depletion_handler dep_cb, void *arg)
{
	struct bman_softc *sc;
	struct bman_pool *bp;

	sc = bman_sc;
	bp = bman_new_pool();
	if (bpid != NULL)
		*bpid = bp->bpid;

	if (dep_cb) {
		bp->dep_cb = dep_cb;
		bus_write_4(sc->sc_rres, BMAN_POOL_SWDET(bp->bpid),
		    dep_sw_entry);
		bus_write_4(sc->sc_rres, BMAN_POOL_SWDXT(bp->bpid),
		    dep_sw_exit);
		bus_write_4(sc->sc_rres, BMAN_POOL_HWDET(bp->bpid),
		    dep_hw_entry);
		bus_write_4(sc->sc_rres, BMAN_POOL_HWDXT(bp->bpid),
		    dep_hw_exit);
		bp->arg = arg;
		bman_portal_enable_scn(DPCPU_GET(bman_affine_portal), bp);
	}

	return (bp);
}

int
bman_pool_destroy(struct bman_pool *pool)
{
	/* Need to error, or print a warning, if the pool isn't empty */
	if (bman_count(pool) != 0)
		return (EBUSY);
	vmem_free(bman_sc->sc_vmem, pool->bpid, 1);
	free(pool, M_BMAN);

	return (0);
}

int
bman_put_buffers(struct bman_pool *pool, struct bman_buffer *buffers, int count)
{
	struct bman_portal_softc *portal;
	int error;

	critical_enter();

	portal = DPCPU_GET(bman_affine_portal);
	if (portal == NULL) {
		critical_exit();
		return (EIO);
	}

	while (count > 0) {
		int c = min(count, 8);
		error = bman_release(pool, buffers, c);
		buffers += c;
		count -= c;
	}

	critical_exit();

	return (error);
}

uint32_t
bman_get_bpid(struct bman_pool *pool)
{
	return (pool->bpid);
}

uint32_t
bman_count(struct bman_pool *pool)
{

	return (bus_read_4(bman_sc->sc_rres, BMAN_POOL_CONTENT(pool->bpid)));
}

