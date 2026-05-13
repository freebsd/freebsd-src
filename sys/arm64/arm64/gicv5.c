/*-
 * Copyright (c) 2015-2016 The FreeBSD Foundation
 * Copyright (c) 2025 Arm Ltd
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_acpi.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitset.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/intr.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpu_feat.h>
#include <machine/smp.h>

#ifdef FDT
#include <dev/fdt/fdt_intr.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif

#include "pic_if.h"

#include <arm/arm/gic_common.h>
#include "gicv5reg.h"
#include "gicv5var.h"
#include "gic_v3_var.h" /* For GICV3_IVAR_NIRQS */

#define	LPI_IPI_BASE		0
#define	LPI_IPI_LIMIT		(LPI_IPI_BASE + (mp_maxid + 1) * INTR_IPI_COUNT)
#define	LPI_IS_IPI(lpi)		((lpi) < LPI_ITS_BASE)
#define	LPI_IPI_IDX(lpi)	((lpi) - LPI_IPI_BASE)
#define	LPI_TO_IPI(lpi)		(LPI_IPI_IDX(lpi) % INTR_IPI_COUNT)
#define	IPI_TO_LPI(ipi, cpu)	((cpu) * INTR_IPI_COUNT + (ipi))

#define	LPI_ITS_BASE		(LPI_IPI_BASE + LPI_IPI_LIMIT)

/* 2^12 LPIs should be enough for a linear table */
#define	GICV5_LPI_ID_BITS_MAX			12

#define	IRS_CFG_READ_4(_irs, _reg)				\
    bus_read_4((_irs)->irs_cfg, (_reg))
#define	IRS_CFG_WRITE_4(_irs, _reg, _val)			\
    bus_write_4((_irs)->irs_cfg, (_reg), (_val))
#define	IRS_CFG_READ_8(_irs, _reg)				\
    bus_read_8((_irs)->irs_cfg, (_reg))
#define	IRS_CFG_WRITE_8(_irs, _reg, _val)			\
    bus_write_8((_irs)->irs_cfg, (_reg), (_val))

#define NR_VM_IDS (MAXCPU * 2)
BITSET_DEFINE(_vm_ids, NR_VM_IDS);

SLIST_HEAD(vm_data_head, gicv5_vm_data);

struct gicv5_irs {
	cpuset_t		 irs_cpus;
	struct resource		*irs_cfg;
	uint64_t		*ist_base;

	uint64_t		*vmt_base;

	u_int			irs_next_irq_cpu;

	u_int			irs_parange;

	int			irs_cfg_rid;
	u_int			irs_spi_start;
	u_int			irs_spi_count;

	struct mtx		irs_lock;
	size_t			irs_lpi_l2size;
	u_int			irs_lpi_l2bits;
	bool			irs_lpi_2l;

#ifdef INVARIANTS
	bool			irs_ready;
#endif

	bool 			has_virt;

	u_int			vm_id_count;
	u_int			vpe_id_count;

	struct mtx		vm_lock;
	struct _vm_ids		vm_ids;
	struct vm_data_head	vm_data_head;

	size_t			vmd_size;
	size_t			vped_size;
};

struct gicv5_vm_data {
	SLIST_ENTRY(gicv5_vm_data) node;

	int vm_id;

	uint64_t *vpet;
	void *vpete;
	uint32_t *lpi_ist;
	uint32_t *spi_ist;
	void *vmd;
};

struct gicv5_irqsrc {
	struct gicv5_base_irqsrc gi_isrc;
	struct gicv5_irs	*gi_irs;
	enum intr_polarity	 gi_pol;
	enum intr_trigger	 gi_trig;
};

static __read_mostly int *gicv5_iaffids;

static bus_print_child_t gicv5_print_child;
static bus_read_ivar_t gicv5_read_ivar;
static bus_get_cpus_t gicv5_get_cpus;
static bus_get_resource_list_t gicv5_get_resource_list;
static pic_disable_intr_t gicv5_disable_intr;
static pic_enable_intr_t gicv5_enable_intr;
static pic_map_intr_t gicv5_map_intr;
static pic_setup_intr_t gicv5_setup_intr;
static pic_teardown_intr_t gicv5_teardown_intr;
static pic_post_filter_t gicv5_post_filter;
static pic_post_ithread_t gicv5_post_ithread;
static pic_pre_ithread_t gicv5_pre_ithread;
static pic_bind_intr_t gicv5_bind_intr;
#ifdef SMP
static pic_init_secondary_t gicv5_init_secondary;
static pic_ipi_send_t gicv5_ipi_send;
static pic_ipi_setup_t gicv5_ipi_setup;
#endif

static device_method_t gicv5_methods[] = {
	/* Bus interface */
	DEVMETHOD(bus_print_child,	gicv5_print_child),
	DEVMETHOD(bus_read_ivar,	gicv5_read_ivar),
	DEVMETHOD(bus_get_cpus,		gicv5_get_cpus),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_get_resource_list, gicv5_get_resource_list),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_alloc_resource,	bus_generic_rl_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	gicv5_disable_intr),
	DEVMETHOD(pic_enable_intr,	gicv5_enable_intr),
	DEVMETHOD(pic_map_intr,		gicv5_map_intr),
	DEVMETHOD(pic_setup_intr,	gicv5_setup_intr),
	DEVMETHOD(pic_teardown_intr,	gicv5_teardown_intr),
	DEVMETHOD(pic_post_filter,	gicv5_post_filter),
	DEVMETHOD(pic_post_ithread,	gicv5_post_ithread),
	DEVMETHOD(pic_pre_ithread,	gicv5_pre_ithread),
	DEVMETHOD(pic_bind_intr,	gicv5_bind_intr),
#ifdef SMP
	DEVMETHOD(pic_init_secondary,	gicv5_init_secondary),
	DEVMETHOD(pic_ipi_send,		gicv5_ipi_send),
	DEVMETHOD(pic_ipi_setup,	gicv5_ipi_setup),
#endif

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(gic, gicv5_driver, gicv5_methods, sizeof(struct gicv5_softc));

static int
gicv5_wait_for_op(struct gicv5_irs *irs, bus_size_t reg, uint32_t mask,
    uint32_t *valp)
{
	uint32_t val;
	int timeout;

#ifdef INVARIANTS
	if (irs->irs_ready)
		mtx_assert(&irs->irs_lock, MA_OWNED);
#endif

	/* Timeout of ~10ms */
	timeout = 10000;
	do {
		val = IRS_CFG_READ_4(irs, reg);
		if ((val & mask) != 0) {
			if (valp != NULL)
				*valp = val;
			return (0);
		}
		DELAY(1);
	} while (--timeout > 0);

	return (ETIMEDOUT);
}

static int
gicv5_wait_irs_cr0_idle(struct gicv5_irs *irs)
{
	return (gicv5_wait_for_op(irs, IRS_CR0, IRS_CR0_IDLE, NULL));
}

static int
gicv5_wait_irs_spi_status_idle(struct gicv5_irs *irs)
{
	uint32_t val;
	int error;

	error = gicv5_wait_for_op(irs, IRS_SPI_STATUSR, IRS_SPI_STATUSR_IDLE,
	    &val);
	if (error != 0)
		return (error);

	if ((val & IRS_SPI_STATUSR_V) == 0)
		return (EIO);

	return (0);
}

static void
gicv5_irs_init_ist(struct gicv5_softc *sc, struct gicv5_irs *irs,
    uint64_t cfgr)
{
	IRS_CFG_WRITE_4(irs, IRS_IST_CFGR, cfgr);

	KASSERT((vtophys(irs->ist_base) & ~IRS_IST_BASER_ADDR_MASK) == 0,
	    ("%s: Invalid IST base address %lx", __func__,
	    vtophys(irs->ist_base)));
	IRS_CFG_WRITE_8(irs, IRS_IST_BASER, vtophys(irs->ist_base) |
	    IRS_IST_BASER_VALID);

	gicv5_wait_for_op(irs, IRS_IST_STATUSR, IRS_IST_STATUSR_IDLE, NULL);
}

static void
gicv5_irs_alloc_ist(struct gicv5_softc *sc, struct gicv5_irs *irs,
    size_t size)
{
	irs->ist_base = contigmalloc(size, M_DEVBUF, M_WAITOK | M_ZERO, 0,
	    (1ul << irs->irs_parange) - 1, size, 0);
	KASSERT(irs->ist_base != NULL,
	    ("%s: Failed to alloc %zu bytes", __func__, size));
	if (sc->gic_coherent)
		/* Ensure the IRS observed zeroed memory */
		dsb(ishst);
	else
		cpu_dcache_wbinv_range(irs->ist_base, size);

}

static void
gicv5_irs_alloc_linear(struct gicv5_softc *sc, struct gicv5_irs *irs,
    uint32_t *cfgrp, u_int lpi_id_bits, u_int istsz)
{
	size_t size;
	uint32_t cfgr;
	u_int n;

	MPASS(istsz <= IRS_IST_CFGR_ISTSZ_16_VAL);

	/*
	 * This is the alignment calculation from the IRS_IST_BASER
	 * definition. If the size is > 64 bytes then size == align.
	 * For sizes < 64 bytes we can just round up the size.
	 */
	n = MAX(5, istsz + 1 + lpi_id_bits);
	size = 1ul << (n + 1);

	gicv5_irs_alloc_ist(sc, irs, size);

	irs->irs_lpi_2l = false;

	cfgr = IRS_IST_CFGR_STRUCTURE_LINEAR;
	cfgr |= istsz << IRS_IST_CFGR_ISTSZ_SHIFT;
	cfgr |= lpi_id_bits << IRS_IST_CFGR_LPI_ID_BITS_SHIFT;
	*cfgrp = cfgr;
}

static void
gicv5_irs_alloc_2level(struct gicv5_softc *sc, struct gicv5_irs *irs,
    uint32_t *cfgrp, u_int lpi_id_bits, u_int istsz, u_int l2sz)
{
	size_t size;
	uint32_t cfgr;
	u_int n;

	MPASS(istsz <= IRS_IST_CFGR_ISTSZ_16_VAL);

	/*
	 * This is the alignment calculation from the IRS_IST_BASER
	 * definition. If the size is > 64 bytes then size == align.
	 * for sizes < 64 bytes we can just round up the size.
	 */
	n = MAX(5, lpi_id_bits - L2_ISTE_LOG2_ENTRIES(istsz, l2sz) + 2);
	size = 1ul << (n + 1);

	gicv5_irs_alloc_ist(sc, irs, size);

	irs->irs_lpi_l2size = 1ul << (L2_ISTE_LOG2_SIZE(l2sz));
	irs->irs_lpi_l2bits = L2_ISTE_LOG2_ENTRIES(istsz, l2sz);
	irs->irs_lpi_2l = true;

	cfgr = IRS_IST_CFGR_STRUCTURE_2LVL;
	cfgr |= istsz << IRS_IST_CFGR_ISTSZ_SHIFT;
	cfgr |= l2sz << IRS_IST_CFGR_L2SZ_SHIFT;
	cfgr |= lpi_id_bits << IRS_IST_CFGR_LPI_ID_BITS_SHIFT;
	*cfgrp = cfgr;
}

void
gicv5_irs_extend_ist(device_t dev, device_t child, u_int lpi)
{
	struct gicv5_softc *sc;
	struct gicv5_devinfo *di;
	struct gicv5_irs *irs;
	void *l2_ist;
	size_t size;
	u_int index;

	di = device_get_ivars(child);
	irs = di->di_irs;
	MPASS(irs != NULL);

	/*
	 * If we have a linear table then we don't need to extend it, it is
	 * already large enough for all LPIs we could allocate.
	 */
	if (!irs->irs_lpi_2l)
		return;

	sc = device_get_softc(dev);
	index = lpi >> irs->irs_lpi_l2bits;
	size = irs->irs_lpi_l2size;

	/* Check if there the l2 pointer is valid */
	if ((irs->ist_base[index] & L1_ISTE_VALID) != 0) {
		return;
	}

	/* Try allocating the level 2 IST */
	l2_ist = contigmalloc(size, M_DEVBUF, M_WAITOK | M_ZERO, 0,
	    (1ul << irs->irs_parange) - 1, size, 0);

	mtx_lock_spin(&irs->irs_lock);

	/* Check if we won the race */
	if ((irs->ist_base[index] & L1_ISTE_VALID) != 0) {
		mtx_unlock_spin(&irs->irs_lock);
		free(l2_ist, M_DEVBUF);
		return;
	}

	irs->ist_base[index] = vtophys(l2_ist) | L1_ISTE_VALID;
	if (sc->gic_coherent) {
		dsb(ishst);
	} else {
		cpu_dcache_wbinv_range(l2_ist, size);
		cpu_dcache_wb_range(&irs->ist_base[index],
		    sizeof(irs->ist_base[index]));
	}

	IRS_CFG_WRITE_4(irs, IRS_MAP_L2_ISTR, lpi);

	gicv5_wait_for_op(irs, IRS_IST_STATUSR, IRS_IST_STATUSR_IDLE, NULL);

	if (!sc->gic_coherent)
		cpu_dcache_inv_range(&irs->ist_base[index],
		    sizeof(irs->ist_base[index]));
	mtx_unlock_spin(&irs->irs_lock);
}

static void
gicv5_irs_init_vmt(struct gicv5_softc *sc, struct gicv5_irs *irs,
    int vm_id_count)
{
	uint64_t cfgr;

	cfgr = (ilog2_int(vm_id_count) << IRS_VMT_VM_ID_BITS_SHIFT) &
	    IRS_VMT_VM_ID_BITS_MASK;
	IRS_CFG_WRITE_4(irs, IRS_VMT_CFGR, cfgr);

	KASSERT((vtophys(irs->vmt_base) & ~IRS_IST_BASER_ADDR_MASK) == 0,
	    ("%s: Invalid VMT base address %lx", __func__,
	    vtophys(irs->vmt_base)));
	IRS_CFG_WRITE_8(irs, IRS_VMT_BASER, vtophys(irs->vmt_base) |
	    IRS_VMT_BASER_VALID);

	gicv5_wait_for_op(irs, IRS_VMT_STATUSR, IRS_VMT_STATUSR_IDLE, NULL);
}

static void
gicv5_irs_alloc_vmt(struct gicv5_softc *sc, struct gicv5_irs *irs,
    size_t size)
{
	irs->vmt_base = contigmalloc(size, M_DEVBUF, M_WAITOK | M_ZERO, 0,
	    IRS_VMT_BASER_ADDR_LIMIT, size, 0);
	KASSERT(irs->vmt_base != NULL,
	    ("%s: Failed to alloc %zu bytes", __func__, size));
	if (sc->gic_coherent)
		/* Ensure the IRS observed zeroed memory */
		dsb(ishst);
	else
		cpu_dcache_wbinv_range(irs->vmt_base, size);
}

void
gicv5_irs_init(device_t dev, u_int idx, cpuset_t *cpuset)
{
	struct gicv5_softc *sc;

	sc = device_get_softc(dev);

	MPASS(idx < sc->gic_nirs);
	MPASS(sc->gic_irs != NULL);
	MPASS(sc->gic_irs[idx] == NULL);

	sc->gic_irs[idx] = malloc(sizeof(*sc->gic_irs[0]), M_DEVBUF,
	    M_ZERO | M_WAITOK);
	CPU_COPY(cpuset, &sc->gic_irs[idx]->irs_cpus);
	sc->gic_irs[idx]->irs_cfg_rid = idx;

	BIT_FILL(NR_VM_IDS, &sc->gic_irs[idx]->vm_ids);
	SLIST_INIT(&sc->gic_irs[idx]->vm_data_head);
}

static void
gicv5_irs_attach(device_t dev, struct gicv5_irs *irs, u_int idx)
{
	struct gicv5_softc *sc;
	const char *name;
	uint64_t icc_idr0;
	uint32_t cfgr, cr1, idr0, idr2, idr3, idr4;
	u_int istsz, lpi_id_bits, l2sz, spi_count, spi_end;
	int error, iaffid;
	bool two_levels;
	int vm_id_bits;

	sc = device_get_softc(dev);
	name = device_get_nameunit(dev);

	/* The attachment needs to set this */
	MPASS(!CPU_EMPTY(&irs->irs_cpus));

#ifdef INVARIANTS
	irs->irs_ready = false;
#endif
	mtx_init(&irs->irs_lock, "GICv5 IRS lock", NULL, MTX_SPIN);
	mtx_init(&irs->vm_lock, "GICv5 IRS VM lock", NULL, MTX_DEF);

	irs->irs_cfg = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &irs->irs_cfg_rid, RF_ACTIVE);
	if (irs->irs_cfg == NULL)
		panic("%s: Unable to allocate memory resource",
		    device_get_nameunit(dev));

	switch (bus_read_4(irs->irs_cfg, IRS_IDR0) & IRS_IDR0_PA_RANGE_MASK) {
	default:
	case IRS_IDR0_PA_RANGE_4G:
		irs->irs_parange = 32;
		break;
	case IRS_IDR0_PA_RANGE_64G:
		irs->irs_parange = 36;
		break;
	case IRS_IDR0_PA_RANGE_1T:
		irs->irs_parange = 40;
		break;
	case IRS_IDR0_PA_RANGE_4T:
		irs->irs_parange = 42;
		break;
	case IRS_IDR0_PA_RANGE_16T:
		irs->irs_parange = 44;
		break;
	case IRS_IDR0_PA_RANGE_256T:
		irs->irs_parange = 48;
		break;
	case IRS_IDR0_PA_RANGE_4P:
		irs->irs_parange = 52;
		break;
	case IRS_IDR0_PA_RANGE_64P:
		irs->irs_parange = 56;
		break;
	}

	sc->has_virt = IRS_CFG_READ_4(irs, IRS_IDR0) & IRS_IDR0_VIRT;

	/* Set the control registers */
	if (sc->gic_coherent) {
		cr1 = IRS_CR1_VPED_WA |
		    IRS_CR1_VPED_RA |
		    IRS_CR1_VMD_WA |
		    IRS_CR1_VMD_RA |
		    IRS_CR1_VPET_WA |
		    IRS_CR1_VPET_RA |
		    IRS_CR1_VMT_WA |
		    IRS_CR1_VMT_RA |
		    IRS_CR1_IST_WA |
		    IRS_CR1_IST_RA |
		    IRS_CR1_IC_WB |
		    IRS_CR1_OC_WB |
		    IRS_CR1_SH_IS;
	} else {
		cr1 = IRS_CR1_VPED_NO_WA |
		    IRS_CR1_VPED_NO_RA |
		    IRS_CR1_VMD_NO_WA |
		    IRS_CR1_VMD_NO_RA |
		    IRS_CR1_VPET_NO_WA |
		    IRS_CR1_VPET_NO_RA |
		    IRS_CR1_VMT_NO_WA |
		    IRS_CR1_VMT_NO_RA |
		    IRS_CR1_IST_NO_WA |
		    IRS_CR1_IST_NO_RA |
		    IRS_CR1_IC_NC |
		    IRS_CR1_OC_NC |
		    IRS_CR1_SH_NS;
	}
	IRS_CFG_WRITE_4(irs, IRS_CR1, cr1);
	IRS_CFG_WRITE_4(irs, IRS_CR0, IRS_CR0_IRSEN);
	gicv5_wait_irs_cr0_idle(irs);

	idr2 = IRS_CFG_READ_4(irs, IRS_IDR2);

	two_levels = (idr2 & IRS_IDR2_IST_LEVELS_TWO) != 0;
	lpi_id_bits = IRS_IDR2_ID_BITS(idr2);

	if (!two_levels) {
		/*
		 * Limit the size of the table as we need to entierly allocate
		 * it for the linear table
		 */
		lpi_id_bits = MIN(lpi_id_bits, GICV5_LPI_ID_BITS_MAX);
		/* Ensure lpi_id_bits is at least the mnimum value */
		lpi_id_bits = MAX(lpi_id_bits, IRS_IDR2_MIN_LPI_ID_BITS(idr2));
	}

	icc_idr0 = READ_SPECIALREG(ICC_IDR0_EL1);
	if ((icc_idr0 & ICC_IDR0_GCIE_LEGACY_MASK) != 0)
		device_printf(dev, "Have VGICv3\n");

	switch(icc_idr0 & ICC_IDR0_ID_BITS_MASK) {
	case ICC_IDR0_ID_BITS_16:
		lpi_id_bits = MIN(lpi_id_bits, 16);
		break;
	default:
	case ICC_IDR0_ID_BITS_24:
		lpi_id_bits = MIN(lpi_id_bits, 24);
		break;
	}

	sc->gic_lpi_id_bits = lpi_id_bits;

	/* The IST entries contain metadata so the size will be larger */
	if ((idr2 & IRS_IDR2_ISTMD) != 0) {
		uint64_t istmd_sz;

		istmd_sz = (idr2 & IRS_IDR2_ISTMD_SZ_MASK) >>
		    IRS_IDR2_ISTMD_SZ_SHIFT;
		if (lpi_id_bits < istmd_sz) {
			istsz = IRS_IST_CFGR_ISTSZ_8_VAL;
		} else {
			istsz = IRS_IST_CFGR_ISTSZ_16_VAL;
		}
	} else {
		/* The default ITS entry size is 4 bytes */
		istsz = IRS_IST_CFGR_ISTSZ_4_VAL;
	}

	if (two_levels) {
		if ((idr2 & IRS_IDR2_IST_L2SZ_64K) != 0)
			l2sz = IRS_IST_CFGR_L2SZ_64K_VAL;
		else if ((idr2 & IRS_IDR2_IST_L2SZ_16K) != 0)
			l2sz = IRS_IST_CFGR_L2SZ_16K_VAL;
		else
			l2sz = IRS_IST_CFGR_L2SZ_4K_VAL;
	}

	/*
	 * Use 2 level tables if able, and the size is large enough for them
	 * to be worth it. This is based on the calculation in the GICv5
	 * spec (ARM-AES-0070) 00EAC0 section 10.2.1.14 IRS_IST_CFGR.
	 */
	if (two_levels &&
	    lpi_id_bits > L2_ISTE_LOG2_ENTRIES(istsz, l2sz)) {
		gicv5_irs_alloc_2level(sc, irs, &cfgr, lpi_id_bits, istsz,
		    l2sz);
	} else {
		gicv5_irs_alloc_linear(sc, irs, &cfgr, lpi_id_bits, istsz);
	}

	if (idx == 0)
		sc->gic_nlpis = 1u << lpi_id_bits;
	else
		sc->gic_nlpis = MIN(sc->gic_nlpis, 1u << lpi_id_bits);

	spi_count = IRS_CFG_READ_4(irs, IRS_IDR5) & IRS_IDR5_SPI_RANGE;
	if (sc->gic_irs_irqs == NULL) {
		KASSERT(idx == 0,
		    ("%s: Null IRS table on no-zero index (idx = %d)",
		    __func__, idx));
		sc->gic_spi_count = spi_count;
		sc->gic_irs_irqs = mallocarray(spi_count,
		    sizeof(struct gicv5_irqsrc), M_DEVBUF, M_WAITOK | M_ZERO);
	}

	/* Read and check the IRS SPI details */
	irs->irs_spi_start =
	    IRS_CFG_READ_4(irs, IRS_IDR7) & IRS_IDR7_SPI_BASE;

	irs->irs_spi_count =
	    IRS_CFG_READ_4(irs, IRS_IDR6) & IRS_IDR6_SPI_IRS_RANGE;

	spi_end = irs->irs_spi_start + irs->irs_spi_count;
	if (spi_end > spi_count)
		panic("%s: IRS %u has SPIs past global count (%u > %u)\n",
		    device_get_nameunit(dev), idx, spi_end, spi_count);

	gicv5_irs_init_ist(sc, irs, cfgr);

	/*
	 * Set a valid interrupt affinity ID, even if it's for a CPU not
	 * attached to this IRS.
	 */
	iaffid = gicv5_iaffids[curcpu];
	MPASS(iaffid >= 0);
	for (u_int irq = irs->irs_spi_start; irq < spi_end; irq++) {
		struct gicv5_irqsrc *gi;
		uint64_t cdaff, cdpri;

		gi = &sc->gic_irs_irqs[irq];
		MPASS(gi->gi_irs == NULL);
		gi->gi_isrc.gbi_space = GICv5_SPI;
		gi->gi_isrc.gbi_ipi = false;
		gi->gi_isrc.gbi_irq = irq;
		gi->gi_irs = irs;
		gi->gi_pol = INTR_POLARITY_CONFORM;
		gi->gi_trig = INTR_TRIGGER_CONFORM;
		error = intr_isrc_register(&gi->gi_isrc.gbi_isrc, dev,
		    0, "%s,s%u", name, irq);
		if (error != 0)
			panic("%s: Unable to register SPI irq src",
			    device_get_nameunit(dev));

		/* Set the base priority */
		cdpri = GIC_CDPRI_PRORITY(GICV5_PRI_LOWEST);
		cdpri |= GIC_CDPRI_TYPE_SPI;
		cdpri |= GIC_CDPRI_ID(irq);
		gic_cdpri(cdpri);

		/* Set the affinity */
		cdaff = GIC_CDAFF_IAFFID(iaffid);
		cdaff |= GIC_CDAFF_TYPE_SPI;
		cdaff |= GIC_CDAFF_IRM_TARGETED;
		cdaff |= GIC_CDAFF_ID(irq);
		gic_cdaff(cdaff);
		isb();
	}

	idr0 = IRS_CFG_READ_4(irs, IRS_IDR0);
	if (idr0 & IRS_IDR0_VIRT) {
		idr3 = IRS_CFG_READ_4(irs, IRS_IDR3);
		idr4 = IRS_CFG_READ_4(irs, IRS_IDR4);

		if (idr3 & IRS_IDR3_VMD)
			irs->vmd_size = 1 << ((idr4 & IRS_IDR3_VMD_SZ_MASK) >>
			    IRS_IDR3_VMD_SZ_SHIFT);
		else
			irs->vmd_size = 0;

		vm_id_bits = (idr3 & IRS_IDR3_VM_ID_BITS_MASK)
		    >> IRS_IDR3_VM_ID_BITS_SHIFT;
		irs->vm_id_count = MIN(1 << vm_id_bits, NR_VM_IDS);

		irs->vped_size = 1 << (idr4 & IRS_IDR4_VPED_SZ_MASK);
		irs->vpe_id_count = 2 << ((idr4 & IRS_IDR4_VPE_ID_BITS_MASK)
		    >> IRS_IDR4_VPE_ID_BITS_SHIFT);

		gicv5_irs_alloc_vmt(sc, irs, irs->vm_id_count * L2_VMTE_SIZE);
		gicv5_irs_init_vmt(sc, irs, irs->vm_id_count);
	}

#ifdef INVARIANTS
	irs->irs_ready = true;
#endif
}

int
gicv5_alloc_vm(device_t dev, int nr_vpes)
{
	struct gicv5_vm_data *vm_data;
	struct gicv5_softc *sc;
	struct gicv5_irs *irs;
	size_t lpi_ist_align;
	size_t spi_ist_align;
	uintptr_t vpete_p;
	uint64_t *vmte;
	int i;

	int spi_id_bits = 16;
	int lpi_id_bits;

	sc = device_get_softc(dev);
	irs = sc->gic_irs[0];

	if (nr_vpes > irs->vpe_id_count)
		return (EINVAL);

	lpi_id_bits = sc->gic_lpi_id_bits;

	lpi_ist_align = MAX(L2_ISTE_SIZE * (1 << lpi_id_bits), 64);
	spi_ist_align = MAX(L2_ISTE_SIZE * (1 << spi_id_bits), 64);

	vm_data = malloc(sizeof(*vm_data), M_DEVBUF, M_ZERO | M_WAITOK);

	vm_data->vpet = contigmalloc(VPETE_SIZE * nr_vpes,
	    M_DEVBUF, M_ZERO | M_WAITOK, 0, IRS_VMT_BASER_ADDR_LIMIT,
	    VPETE_SIZE * ilog2_int(nr_vpes), 0);
	vm_data->vpete = contigmalloc(irs->vped_size * nr_vpes,
	    M_DEVBUF, M_ZERO | M_WAITOK, 0, IRS_VMT_BASER_ADDR_LIMIT,
	    irs->vped_size, 0);
	vm_data->lpi_ist = contigmalloc(L2_ISTE_SIZE * (1 << lpi_id_bits),
	    M_DEVBUF, M_ZERO | M_WAITOK, 0, IRS_VMT_BASER_ADDR_LIMIT,
	    lpi_ist_align, 0);
	vm_data->spi_ist = contigmalloc(L2_ISTE_SIZE * (1 << spi_id_bits),
	    M_DEVBUF, M_ZERO | M_WAITOK, 0, IRS_VMT_BASER_ADDR_LIMIT,
	    spi_ist_align, 0);
	if (irs->vmd_size != 0)
		vm_data->vmd = contigmalloc(irs->vmd_size, M_DEVBUF,
		    M_ZERO | M_WAITOK, 0, IRS_VMT_BASER_ADDR_LIMIT,
		    irs->vmd_size, 0);

	if (!vm_data->vpet || !vm_data->vpete || !vm_data->lpi_ist ||
	    !vm_data->spi_ist || (irs->vmd_size != 0 && !vm_data->vmd)) {
		free(vm_data->vmd, M_DEVBUF);
		free(vm_data->spi_ist, M_DEVBUF);
		free(vm_data->lpi_ist, M_DEVBUF);
		free(vm_data->vpete, M_DEVBUF);
		free(vm_data->vpet, M_DEVBUF);
		free(vm_data, M_DEVBUF);

		return (ENOMEM);
	}

	vpete_p = (uintptr_t)vm_data->vpete;

	for (i = 0; i < nr_vpes; i++) {
		vm_data->vpet[i] = (vtophys(vpete_p) & VPETE_VPED_ADDR_MASK) |
		    VPETE_VALID;
		vpete_p += VPE_DESC_SIZE;
	}

	/* Allocate VM ID */
	mtx_lock(&irs->vm_lock);

	vm_data->vm_id = BIT_FFS(irs->vm_id_count, &irs->vm_ids) - 1;
	if (vm_data->vm_id < 0) {
		mtx_unlock(&irs->vm_lock);

		free(vm_data->vmd, M_DEVBUF);
		free(vm_data->spi_ist, M_DEVBUF);
		free(vm_data->lpi_ist, M_DEVBUF);
		free(vm_data->vpete, M_DEVBUF);
		free(vm_data->vpet, M_DEVBUF);
		free(vm_data, M_DEVBUF);

		return (ENOMEM);
	}

	BIT_CLR(irs->vm_id_count, vm_data->vm_id, &irs->vm_ids);
	SLIST_INSERT_HEAD(&irs->vm_data_head, vm_data, node);

	mtx_unlock(&irs->vm_lock);

	/* Fill in VMT entry now we have a VM ID */
	vmte = &irs->vmt_base[vm_data->vm_id * 4];

	vmte[L2_VMTE_VPE_ADDR_IDX] =
	    (vtophys(vm_data->vpet) & L2_VMTE_VPET_ADDR_MASK) |
	    ((uint64_t)ilog2_int(nr_vpes) << L2_VMTE_VPE_ID_BITS_SHIFT);
	vmte[L2_VMTE_SPI_ADDR_IDX] =
	    (vtophys(vm_data->spi_ist) & L2_VMTE_SPI_IST_ADDR_MASK) |
	    ((uint64_t)spi_id_bits << L2_VMTE_SPI_ID_BITS_SHIFT) |
	    L2_VMTE_SPI_IST_VALID;
	vmte[L2_VMTE_LPI_ADDR_IDX] =
	    (vtophys(vm_data->lpi_ist) & L2_VMTE_LPI_IST_ADDR_MASK) |
	    ((uint64_t)lpi_id_bits << L2_VMTE_LPI_ID_BITS_SHIFT) |
	    L2_VMTE_LPI_IST_VALID;
	if (irs->vmd_size)
		vmte[L2_VMTE_VMD_ADDR_IDX] =
		    (vtophys(vm_data->vmd) & L2_VMTE_VMD_ADDR_MASK) |
		    L2_VMTE_VALID;
	else
		vmte[L2_VMTE_VMD_ADDR_IDX] = L2_VMTE_VALID;

	if (sc->gic_coherent) {
		/* Ensure the IRS observed zeroed memory */
		dsb(ishst);
	} else {
		cpu_dcache_wbinv_range(vmte, sizeof(L2_VMTE_SIZE));
		cpu_dcache_wbinv_range(vm_data->vpet,
		    sizeof(VPETE_SIZE) * nr_vpes);
		cpu_dcache_wbinv_range(vm_data->vpete,
		    sizeof(VPE_DESC_SIZE) * nr_vpes);
		cpu_dcache_wbinv_range(vm_data->lpi_ist,
		    sizeof(L2_ISTE_SIZE) * (1 << lpi_id_bits));
		cpu_dcache_wbinv_range(vm_data->spi_ist,
		    sizeof(L2_ISTE_SIZE) * (1 << spi_id_bits));
		if (irs->vmd_size)
			cpu_dcache_wbinv_range(vm_data->vmd, irs->vmd_size);
	}

	IRS_CFG_WRITE_8(irs, IRS_VMAP_VMR,
	    IRS_VMAP_VMR_M | (vm_data->vm_id & IRS_VMAP_VMR_VM_ID_MASK));

	mtx_lock_spin(&irs->irs_lock);
	gicv5_wait_for_op(irs, IRS_VMT_STATUSR, IRS_VMT_STATUSR_IDLE, NULL);
	mtx_unlock_spin(&irs->irs_lock);

	return (0);
}

int
gicv5_free_vm(device_t dev, int vm_id)
{
	struct gicv5_vm_data *vm_data;
	struct gicv5_softc *sc;
	struct gicv5_irs *irs;

	sc = device_get_softc(dev);
	irs = sc->gic_irs[0];

	KASSERT(vm_id < irs->vm_id_count, ("%s: VM ID out of range", __func__));
	KASSERT(!BIT_ISSET(irs->vm_id_count, vm_id, &sc->gic_irs[0]->vm_ids),
	    ("%s: VM ID not allocated", __func__));

	mtx_lock(&irs->vm_lock);

	BIT_SET(irs->vm_id_count, vm_id, &sc->gic_irs[0]->vm_ids);

	/* Make VM invalid */
	IRS_CFG_WRITE_8(irs, IRS_VMAP_VMR,
	    IRS_VMAP_VMR_M | IRS_VMAP_VMR_U |
	    (vm_id & IRS_VMAP_VMR_VM_ID_MASK));

	mtx_lock_spin(&irs->irs_lock);
	gicv5_wait_for_op(irs, IRS_VMT_STATUSR, IRS_VMT_STATUSR_IDLE, NULL);
	mtx_unlock_spin(&irs->irs_lock);

	SLIST_FOREACH(vm_data, &irs->vm_data_head, node) {
		if (vm_data->vm_id == vm_id) {
			SLIST_REMOVE(&irs->vm_data_head, vm_data,
			    gicv5_vm_data, node);

			mtx_unlock(&irs->vm_lock);

			free(vm_data->vpet, M_DEVBUF);
			free(vm_data->vpete, M_DEVBUF);
			free(vm_data->lpi_ist, M_DEVBUF);
			free(vm_data->spi_ist, M_DEVBUF);
			free(vm_data->vmd, M_DEVBUF);

			free(vm_data, M_DEVBUF);

			return (0);
		}
	}

	mtx_unlock(&irs->vm_lock);

	return (ENOENT);
}

int
gicv5_get_max_vpe_count(device_t dev)
{
	struct gicv5_softc *sc;
	struct gicv5_irs *irs;

	sc = device_get_softc(dev);
	irs = sc->gic_irs[0];

	return (irs->vpe_id_count);
}

void
gicv5_attach(device_t dev)
{
	struct gicv5_softc *sc;
	const char *name;
	int error;

	sc = device_get_softc(dev);
	sc->gic_dev = dev;
	name = device_get_nameunit(dev);

	MPASS(gicv5_iaffids == NULL);
	gicv5_iaffids = mallocarray(mp_maxid + 1, sizeof(*gicv5_iaffids),
	    M_DEVBUF, M_WAITOK);
	for (int i = 0; i <= mp_maxid; i++)
		gicv5_iaffids[i] = -1;
	gicv5_iaffids[curcpu] =
	    ICC_IAFFIDR_IAFFID_VAL(READ_SPECIALREG(ICC_IAFFIDR_EL1));

	for (u_int i = 0; i < sc->gic_nirs; i++)
		gicv5_irs_attach(dev, sc->gic_irs[i], i);
	if (bootverbose)
		device_printf(dev, "Limited to %u LPIs\n", sc->gic_nlpis);

	sc->gic_ppi_irqs = mallocarray(GICV5_PPI_COUNT,
	    sizeof(struct gicv5_irqsrc), M_DEVBUF, M_ZERO | M_WAITOK);
	for (u_int irq = 0; irq < GICV5_PPI_COUNT; irq++) {
		struct intr_irqsrc *isrc;

		sc->gic_ppi_irqs[irq].gi_irs = NULL;
		sc->gic_ppi_irqs[irq].gi_isrc.gbi_space = GICv5_PPI;
		sc->gic_ppi_irqs[irq].gi_isrc.gbi_ipi = false;
		sc->gic_ppi_irqs[irq].gi_isrc.gbi_irq = irq;
		sc->gic_ppi_irqs[irq].gi_pol = INTR_POLARITY_CONFORM;
		sc->gic_ppi_irqs[irq].gi_trig = INTR_TRIGGER_CONFORM;
		isrc = &sc->gic_ppi_irqs[irq].gi_isrc.gbi_isrc;
		error = intr_isrc_register(isrc, dev, INTR_ISRCF_PPI,
		    "%s,p%u", name, irq);
		if (error != 0)
			panic("%s: Unable to register PPI irq src",
			    device_get_nameunit(dev));
	}

	/* Assign LPIs to be used as IPIs */
	sc->gic_ipi_irqs = mallocarray(INTR_IPI_COUNT,
	    sizeof(*sc->gic_ipi_irqs), M_DEVBUF, M_ZERO | M_WAITOK);
	for (u_int ipi = 0; ipi < INTR_IPI_COUNT; ipi++) {
		struct intr_irqsrc *isrc;

		sc->gic_ipi_irqs[ipi].gi_irs = NULL;
		sc->gic_ipi_irqs[ipi].gi_isrc.gbi_space = GICv5_LPI;
		sc->gic_ipi_irqs[ipi].gi_isrc.gbi_ipi = true;
		sc->gic_ipi_irqs[ipi].gi_isrc.gbi_irq = ipi + LPI_IPI_BASE;
		sc->gic_ipi_irqs[ipi].gi_pol = INTR_POLARITY_HIGH;
		sc->gic_ipi_irqs[ipi].gi_trig = INTR_TRIGGER_EDGE;
		isrc = &sc->gic_ipi_irqs[ipi].gi_isrc.gbi_isrc;
		error = intr_isrc_register(isrc, dev, INTR_ISRCF_IPI,
		    "%s,ipi%u", name, ipi);
		if (error != 0)
			panic("%s: Unable to register LPI irq src",
			    device_get_nameunit(dev));
	}

	WRITE_SPECIALREG(ICC_PPI_PRIORITYR0_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR1_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR2_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR3_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR4_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR5_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR6_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR7_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR8_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR9_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR10_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR11_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR12_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR13_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR14_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR15_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));

	WRITE_SPECIALREG(ICC_PPI_ENABLER0_EL1, ICC_PPI_ENABLER_NONE);
	WRITE_SPECIALREG(ICC_PPI_ENABLER1_EL1, ICC_PPI_ENABLER_NONE);
	isb();

	/* Set the priority to the lowest value */
	WRITE_SPECIALREG(ICC_PCR_EL1, ICC_PCR_PRIORITY_LOWEST);

	/* Enable interrupts */
	WRITE_SPECIALREG(ICC_CR0_EL1, ICC_CR0_EN);
	isb();
}

bool
gicv5_has_virt_support(device_t dev)
{
	struct gicv5_softc *sc;
	uint32_t idr0;

	sc = device_get_softc(dev);
	/*
	 * VIRT field is guaranteed to be identical between all IRS units.
	 * Arm GIC Architecture Specification (AES 0070) rule RJJPQJ.
	 */
	idr0 = IRS_CFG_READ_4(sc->gic_irs[0], IRS_IDR0);

	return (idr0 & IRS_IDR0_VIRT);
}

bool
gicv5_add_child(device_t dev, struct gicv5_devinfo *di)
{
	device_t cdev;
	struct gicv5_softc *sc;

	cdev = device_add_child(dev, NULL, DEVICE_UNIT_ANY);
	if (cdev == NULL)
		return (false);

	sc = device_get_softc(dev);
	sc->gic_nchildren++;
	device_set_ivars(cdev, di);

	return (true);
}

static int
gicv5_print_child(device_t bus, device_t child)
{
	struct resource_list *rl;
	int retval = 0;

	rl = BUS_GET_RESOURCE_LIST(bus, child);
	KASSERT(rl != NULL, ("%s: No resource list", __func__));
	retval += bus_print_child_header(bus, child);
	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#jx");
	retval += bus_print_child_footer(bus, child);

	return (retval);
}

static u_int
gicv5_lpi_count(struct gicv5_softc *sc)
{
	MPASS(sc->gic_nlpis >= LPI_ITS_BASE);
	return (sc->gic_nlpis - LPI_ITS_BASE);
}

static int
gicv5_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct gicv5_softc *sc;
	struct gicv5_devinfo *di;
	u_int nlpis;

	switch (which) {
	case GICV5_IVAR_LPI_START:
		di = device_get_ivars(child);
		if (di == NULL || di->di_irs == NULL)
			return (EINVAL);

		sc = device_get_softc(dev);
		nlpis = gicv5_lpi_count(sc) / sc->gic_nchildren;
		*result = LPI_ITS_BASE + device_get_unit(dev) * nlpis;
		return (0);
	case GICV3_IVAR_NIRQS:
		di = device_get_ivars(child);
		if (di == NULL || di->di_irs == NULL)
			return (EINVAL);

		sc = device_get_softc(dev);
		*result = gicv5_lpi_count(sc) / sc->gic_nchildren;
		return (0);
	case GIC_IVAR_HW_REV:
		*result = 5;
		return (0);
	case GIC_IVAR_BUS:
		sc = device_get_softc(dev);
		KASSERT(sc->gic_bus != GIC_BUS_UNKNOWN,
		    ("%s: Unknown bus type", __func__));
		KASSERT(sc->gic_bus <= GIC_BUS_MAX,
		    ("%s: Invalid bus type %u", __func__, sc->gic_bus));
		*result = sc->gic_bus;
		return (0);
	case GIC_IVAR_VGIC:
		di = device_get_ivars(child);
		if (di == NULL)
			return (EINVAL);
		*result = di->di_is_vgic;
		return (0);
	case GIC_IVAR_SUPPORT_LPIS:
		di = device_get_ivars(child);
		if (di == NULL || di->di_irs == NULL)
			return (EINVAL);

		*result =
		    (IRS_CFG_READ_4(di->di_irs, IRS_IDR2) & IRS_IDR2_LPI) != 0;
		return (0);
	}

	return (ENOENT);
}

static int
gicv5_get_cpus(device_t dev, device_t child, enum cpu_sets op, size_t setsize,
    cpuset_t *cpuset)
{
	struct gicv5_devinfo *di;

	if (op != LOCAL_CPUS)
		return (bus_generic_get_cpus(dev, child, op, setsize, cpuset));

	di = device_get_ivars(child);
	if (di == NULL)
		return (bus_generic_get_cpus(dev, child, op, setsize, cpuset));

	if (setsize != sizeof(cpuset_t))
		return (EINVAL);

	*cpuset = di->di_irs->irs_cpus;
	return (0);
}

static struct resource_list *
gicv5_get_resource_list(device_t bus, device_t child)
{
	struct gicv5_devinfo *di;

	di = device_get_ivars(child);
	KASSERT(di != NULL, ("%s: No devinfo", __func__));

	return (&di->di_rl);
}

static void
gicv5_eoi_intr(enum gicv5_irq_space space, uint32_t irq)
{
	uint64_t cddi;

	/* Drop the priority of the specified interrupt */
	cddi = (uint64_t)space << GIC_CDDI_Type_SHIFT;
	cddi |= (uint64_t)irq << GIC_CDDI_ID_SHIFT;
	gic_cddi(cddi);

	/* Drop the running priority of the CPU */
	gic_cdeoi();
}

static void
gicv5_eoi(struct gicv5_base_irqsrc *gbi)
{
	uint32_t irq;
	enum gicv5_irq_space space;

	MPASS(!gbi->gbi_ipi);
	space = gbi->gbi_space;
	irq = gbi->gbi_irq;

	gicv5_eoi_intr(space, irq);
}

int
gicv5_intr(void *arg)
{
	struct gicv5_softc *sc = arg;
	struct gicv5_irqsrc *gi;
	struct trapframe *tf;
	uint64_t hppi;
	u_int irq;

	tf = curthread->td_intr_frame;
	for (;;) {
		hppi = gicr_cdia();
		/* Ensure the interrupt activation has completed */
		gsb_ack();
		/* Ensure the gsb ack instruction has completed */
		isb();

		if ((hppi & ICC_HPPIR_HPPIV) == 0)
			return (FILTER_HANDLED);

		irq = (hppi & ICC_HPPIR_ID_MASK) >> ICC_HPPIR_ID_SHIFT;
		switch (hppi & ICC_HPPIR_TYPE_MASK) {
		case ICC_HPPIR_TYPE_PPI:
			MPASS(irq < GICV5_PPI_COUNT);
			gi = &sc->gic_ppi_irqs[irq];
			if (intr_isrc_dispatch(&gi->gi_isrc.gbi_isrc, tf) != 0){
				if (gi->gi_trig != INTR_TRIGGER_EDGE)
					gicv5_eoi(&gi->gi_isrc);
				gicv5_disable_intr(sc->gic_dev,
				    &gi->gi_isrc.gbi_isrc);
				device_printf(sc->gic_dev,
				    "Stray PPI %u disabled\n", irq);
			}
			break;
		case ICC_HPPIR_TYPE_LPI:
			/* XXX */
			if (LPI_IS_IPI(irq)) {
				u_int ipi;

				KASSERT(LPI_IPI_IDX(irq) < LPI_IPI_LIMIT,
				    ("%s: Invalid IPI LPI %u", __func__, irq));
				ipi = LPI_TO_IPI(irq);
#ifdef SMP
				intr_ipi_dispatch(ipi);
#else
				device_printf(sc->gic_dev,
				    "IPI LPI %u on UP system detected\n", ipi);
#endif
				gicv5_eoi_intr(GICv5_LPI, irq);
			} else {
				intr_child_irq_handler(sc->gic_pic, irq);
			}
			break;
		case ICC_HPPIR_TYPE_SPI:
			MPASS(irq < sc->gic_spi_count);
			gi = &sc->gic_irs_irqs[irq];
			if (intr_isrc_dispatch(&gi->gi_isrc.gbi_isrc, tf) != 0){
				if (gi->gi_trig != INTR_TRIGGER_EDGE)
					gicv5_eoi(&gi->gi_isrc);
				gicv5_disable_intr(sc->gic_dev,
				    &gi->gi_isrc.gbi_isrc);
				device_printf(sc->gic_dev,
				    "Stray SPI %u disabled\n", irq);
			}
			break;
		default:
			panic("%s: Invalid interrupt type %lx", __func__,
			    (hppi & ICC_HPPIR_TYPE_MASK) >>
			    ICC_HPPIR_TYPE_SHIFT);
		}
	}
}

static void
gicv5_disable_intr_action(void *argp)
{
	struct gicv5_base_irqsrc *gbi = argp;
	uint64_t reg;
	uint32_t irq;

	MPASS(!gbi->gbi_ipi);
	irq = gbi->gbi_irq;

	if (irq < GICV5_PPIS_PER_REG)
		reg = READ_SPECIALREG(ICC_PPI_ENABLER0_EL1);
	else
		reg = READ_SPECIALREG(ICC_PPI_ENABLER1_EL1);

	reg &= ~ICC_PPI_ENABLER_MASK(irq);
	reg |= ICC_PPI_ENABLER_DIS(irq);

	if (irq < GICV5_PPIS_PER_REG)
		WRITE_SPECIALREG(ICC_PPI_ENABLER0_EL1, reg);
	else
		WRITE_SPECIALREG(ICC_PPI_ENABLER1_EL1, reg);
	isb();
}

static void
gicv5_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_base_irqsrc *gbi = (struct gicv5_base_irqsrc *)isrc;
	uint32_t irq;

	/* We don't disable IPIs */
	MPASS(!gbi->gbi_ipi);
	irq = gbi->gbi_irq;

	switch (gbi->gbi_space) {
	case GICv5_PPI:
		MPASS((isrc->isrc_flags & INTR_ISRCF_PPI) != 0);

		smp_rendezvous(NULL, gicv5_disable_intr_action, NULL, gbi);
		break;
	case GICv5_SPI:
		gic_cddis(GIC_CDDIS_TYPE_SPI | (irq << GIC_CDDIS_ID_SHIFT));
		isb();
		break;
	case GICv5_LPI:
		gic_cddis(GIC_CDDIS_TYPE_LPI | (irq << GIC_CDDIS_ID_SHIFT));
		isb();
		break;
	default:
		panic("%s: Invalid interrupt space 0x%x", __func__,
		    gbi->gbi_space);
	}
}

static void
gicv5_enable_intr_action(void *argp)
{
	struct gicv5_base_irqsrc *gbi = argp;
	uint64_t reg;
	uint32_t irq;

	MPASS(!gbi->gbi_ipi);
	MPASS(gbi->gbi_space == GICv5_PPI);

	irq = gbi->gbi_irq;

	if (irq < GICV5_PPIS_PER_REG)
		reg = READ_SPECIALREG(ICC_PPI_ENABLER0_EL1);
	else
		reg = READ_SPECIALREG(ICC_PPI_ENABLER1_EL1);

	reg &= ~ICC_PPI_ENABLER_MASK(irq);
	reg |= ICC_PPI_ENABLER_EN(irq);

	if (irq < GICV5_PPIS_PER_REG)
		WRITE_SPECIALREG(ICC_PPI_ENABLER0_EL1, reg);
	else
		WRITE_SPECIALREG(ICC_PPI_ENABLER1_EL1, reg);
	isb();
}

static void
gicv5_enable_ipi(struct gicv5_base_irqsrc *gbi)
{
	uint32_t irq;

	MPASS(gbi->gbi_ipi);
	MPASS(gbi->gbi_space == GICv5_LPI);

	irq = IPI_TO_LPI(gbi->gbi_irq, PCPU_GET(cpuid));
	MPASS(irq < LPI_IPI_LIMIT);

	gic_cden(GIC_CDEN_TYPE_LPI | (irq << GIC_CDEN_ID_SHIFT));
	isb();
}

static void
gicv5_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_base_irqsrc *gbi = (struct gicv5_base_irqsrc *)isrc;
	uint32_t irq;

	if (gbi->gbi_ipi) {
		gicv5_enable_ipi(gbi);
		return;
	}

	irq = gbi->gbi_irq;

	switch (gbi->gbi_space) {
	case GICv5_PPI:
		MPASS((isrc->isrc_flags & INTR_ISRCF_PPI) != 0);

		smp_rendezvous(NULL, gicv5_enable_intr_action, NULL, gbi);
		break;
	case GICv5_SPI:
		gic_cden(GIC_CDEN_TYPE_SPI | (irq << GIC_CDEN_ID_SHIFT));
		isb();
		break;
	case GICv5_LPI:
		gic_cden(GIC_CDEN_TYPE_LPI | (irq << GIC_CDEN_ID_SHIFT));
		isb();
		break;
	default:
		panic("%s: Invalid interrupt space 0x%x", __func__,
		    gbi->gbi_space);
	}
}

#ifdef FDT
static int
gic_map_fdt(device_t dev, u_int ncells, pcell_t *cells, bool *ppi, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	uint64_t reg;
	u_int irq;
	int type;

	if (ncells < 3)
		return (EINVAL);

	/*
	 * The 1st cell is the interrupt type:
	 *	1 = PPI
	 *	2 = LPI
	 *	3 = SPI
	 * The 2nd cell contains the interrupt number
	 * The 3rd cell is the flags, encoded as follows:
	 *   bits[3:0] trigger type and level flags
	 *	1 = edge triggered
	 *      2 = edge triggered (PPI only)
	 *	4 = level-sensitive
	 *	8 = level-sensitive (PPI only)
	 */
	switch (cells[0]) {
	case 1:
		*ppi = true;
		break;
	/* case 2: LPI */
	case 3:
		*ppi = false;
		break;
	default:
		device_printf(dev, "unsupported interrupt type "
		    "configuration:");
		for (u_int i = 0; i < ncells; i++)
			printf(" %x", cells[i]);
		printf("\n");
		return (EINVAL);
	}

	irq = cells[1];

	if (ppi) {
		/* PPIs are hard coded, ignore cells[2] */
		if (irq < GICV5_PPIS_PER_REG)
			reg = READ_SPECIALREG(ICC_PPI_HMR0_EL1);
		else
			reg = READ_SPECIALREG(ICC_PPI_HMR1_EL1);

		if ((reg & ICC_PPI_HMR_MASK(irq)) == ICC_PPI_HMR_EDGE(irq))
			type = FDT_INTR_EDGE_RISING;
		else
			type = FDT_INTR_LEVEL_LOW;
	} else {
		type = cells[2] & FDT_INTR_MASK;
	}

	switch (type) {
	case FDT_INTR_EDGE_RISING:
		*trigp = INTR_TRIGGER_EDGE;
		*polp = INTR_POLARITY_HIGH;
		break;
	case FDT_INTR_EDGE_FALLING:
		*trigp = INTR_TRIGGER_EDGE;
		*polp = INTR_POLARITY_LOW;
		break;
	case FDT_INTR_LEVEL_HIGH:
		*trigp = INTR_TRIGGER_LEVEL;
		*polp = INTR_POLARITY_HIGH;
		break;
	case FDT_INTR_LEVEL_LOW:
		*trigp = INTR_TRIGGER_LEVEL;
		*polp = INTR_POLARITY_LOW;
		break;
	default:
		device_printf(dev, "unsupported trigger/polarity "
		    "configuration 0x%02x\n", cells[2]);
		return (EINVAL);
	}

	*irqp = irq;
	return (0);
}
#endif

#ifdef DEV_ACPI
static int
do_gicv5_map_iwb_intr(struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct intr_map_data_acpi *daa;
	device_t iwb;
	int iwb_id;

	daa = (struct intr_map_data_acpi *)data;
	iwb_id = (daa->irq & GSI_IWB_ID_MASK) >> GSI_IWB_ID_SHIFT;
	iwb = acpi_iort_get_iwb_dev(iwb_id);

	if (iwb == NULL)
		return (ENODEV);

	return (PIC_MAP_INTR(iwb, data, isrcp));
}
#endif

static int
do_gicv5_map_intr(device_t dev, struct intr_map_data *data, bool *ppip,
    u_int *irqp, enum intr_polarity *polp, enum intr_trigger *trigp)
{
	struct gicv5_softc *sc;
	enum intr_polarity pol;
	enum intr_trigger trig;
#ifdef FDT
	struct intr_map_data_fdt *daf;
#endif
#ifdef DEV_ACPI
	struct intr_map_data_acpi *daa;
#endif
	u_int irq;
	bool ppi;

	sc = device_get_softc(dev);

	switch (data->type) {
#ifdef FDT
	case INTR_MAP_DATA_FDT:
		daf = (struct intr_map_data_fdt *)data;
		if (gic_map_fdt(dev, daf->ncells, daf->cells, &ppi, &irq, &pol,
		    &trig) != 0)
			return (EINVAL);
		break;
#endif
#ifdef DEV_ACPI
	case INTR_MAP_DATA_ACPI:
		daa = (struct intr_map_data_acpi *)data;
		if ((daa->irq & GSI_INT_TYPE_MASK) == GSI_INT_TYPE_IWB)
			return (EINVAL);
		irq = daa->irq & GSI_INT_ID_MASK;
		pol = daa->pol;
		trig = daa->trig;
		ppi = ((daa->irq & GSI_INT_TYPE_MASK) == GSI_INT_TYPE_PPI);
		break;
#endif
	default:
		return (EINVAL);
	}

	if (ppi) {
		if (irq >= GICV5_PPI_COUNT)
			return (EINVAL);
	} else {
		if (irq > sc->gic_spi_count)
			return (EINVAL);
	}
	switch (pol) {
	case INTR_POLARITY_CONFORM:
	case INTR_POLARITY_LOW:
	case INTR_POLARITY_HIGH:
		break;
	default:
		return (EINVAL);
	}
	switch (trig) {
	case INTR_TRIGGER_CONFORM:
	case INTR_TRIGGER_EDGE:
	case INTR_TRIGGER_LEVEL:
		break;
	default:
		return (EINVAL);
	}

	*ppip = ppi;
	*irqp = irq;
	if (polp != NULL)
		*polp = pol;
	if (trigp != NULL)
		*trigp = trig;
	return (0);
}

static int
gicv5_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
#ifdef DEV_ACPI
	struct intr_map_data_acpi *daa;
#endif
	struct gicv5_softc *sc;
	u_int irq;
	int error;
	bool ppi;

#ifdef DEV_ACPI
	if (data->type == INTR_MAP_DATA_ACPI) {
		daa = (struct intr_map_data_acpi *)data;
		if ((daa->irq & GSI_INT_TYPE_MASK) == GSI_INT_TYPE_IWB)
			return (do_gicv5_map_iwb_intr(data, isrcp));
	}
#endif

	error = do_gicv5_map_intr(dev, data, &ppi, &irq, NULL, NULL);
	if (error == 0) {
		sc = device_get_softc(dev);
		if (ppi)
			*isrcp = &sc->gic_ppi_irqs[irq].gi_isrc.gbi_isrc;
		else
			*isrcp = &sc->gic_irs_irqs[irq].gi_isrc.gbi_isrc;
	}
	return (error);
}

static int
gicv5_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct gicv5_irqsrc *gi = (struct gicv5_irqsrc *)isrc;
	enum intr_trigger trig;
	enum intr_polarity pol;
	struct gicv5_irs *irs;
	int error;
	u_int irq;
	bool ppi;

	if (data == NULL)
		return (ENOTSUP);

	error = do_gicv5_map_intr(dev, data, &ppi, &irq, &pol, &trig);
	if (error != 0)
		return (error);

	/* This shouldn't return IPIs */
	MPASS(!gi->gi_isrc.gbi_ipi);

	if (gi->gi_isrc.gbi_irq != irq || pol == INTR_POLARITY_CONFORM ||
	    trig == INTR_TRIGGER_CONFORM)
		return (EINVAL);

	if (((isrc->isrc_flags & INTR_ISRCF_PPI) != 0) != ppi)
		return (EINVAL);

	/* Compare config if this is not first setup. */
	if (isrc->isrc_handlers != 0) {
		if (pol != gi->gi_pol || trig != gi->gi_trig)
			return (EINVAL);
		else
			return (0);
	}

	gi->gi_pol = pol;
	gi->gi_trig = trig;

	switch (gi->gi_isrc.gbi_space) {
	default:
		panic("%s: Invalid IRQ space %#x", __func__,
		    gi->gi_isrc.gbi_space);
	case GICv5_PPI:
		MPASS(ppi);
		MPASS(gi->gi_irs == NULL);
		MPASS(irq < GICV5_PPI_COUNT);
		CPU_SET(PCPU_GET(cpuid), &isrc->isrc_cpu);

		break;
	case GICv5_SPI:
		MPASS(!ppi);
		irs = gi->gi_irs;

		mtx_lock_spin(&irs->irs_lock);
		/*
		 * This depends on intr_setup_irq holding the isrc_table_lock
		 * to serialise access to this register
		 */
		IRS_CFG_WRITE_4(irs, IRS_SPI_SELR, irq);
		error = gicv5_wait_irs_spi_status_idle(irs);
		if (error != 0) {
			mtx_unlock_spin(&irs->irs_lock);
			return (error);
		}

		/* Set the trigger mode */
		if (trig == INTR_TRIGGER_EDGE)
			IRS_CFG_WRITE_4(irs, IRS_SPI_CFGR,
			    IRS_SPI_CFGR_TM_EDGE);
		else
			IRS_CFG_WRITE_4(irs, IRS_SPI_CFGR,
			    IRS_SPI_CFGR_TM_LEVEL);

		error = gicv5_wait_irs_spi_status_idle(irs);
		mtx_unlock_spin(&irs->irs_lock);
		return (error);
	}

	return (0);
}

static int
gicv5_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	return (0);
}

static void
gicv5_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_base_irqsrc *gbi = (struct gicv5_base_irqsrc *)isrc;

	/* No ithreads for IPIs */
	MPASS(!gbi->gbi_ipi);

	gicv5_eoi(gbi);
}

static void
gicv5_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_base_irqsrc *gbi = (struct gicv5_base_irqsrc *)isrc;

	/* No ithreads for IPIs */
	MPASS(!gbi->gbi_ipi);

	switch (gbi->gbi_space) {
	case GICv5_PPI:
		gicv5_disable_intr_action(isrc);
		break;
	default:
		gicv5_disable_intr(dev, isrc);
		break;
	}
	gicv5_eoi(gbi);
}

static void
gicv5_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_base_irqsrc *gbi = (struct gicv5_base_irqsrc *)isrc;

	/* No ithreads for IPIs */
	MPASS(!gbi->gbi_ipi);

	switch (gbi->gbi_space) {
	case GICv5_PPI:
		gicv5_enable_intr_action(isrc);
		break;
	default:
		gicv5_enable_intr(dev, isrc);
		break;
	}
}

static int
gicv5_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_irqsrc *gi = (struct gicv5_irqsrc *)isrc;
	struct gicv5_base_irqsrc *gbi = &gi->gi_isrc;
	struct gicv5_irs *irs;
	uint64_t cdaff, cdpri;
	uint32_t irq;
	int cpu, iaffid;

	/* IPIs are already bound */
	MPASS(!gbi->gbi_ipi);

	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		irs = gi->gi_irs;
		cpu = irs->irs_next_irq_cpu =
		    intr_irq_next_cpu(irs->irs_next_irq_cpu, &irs->irs_cpus);
	} else {
		cpu = CPU_FFS(&isrc->isrc_cpu) - 1;
	}

	MPASS(cpu <= mp_maxid);
	iaffid = gicv5_iaffids[cpu];
	MPASS(iaffid >= 0);

	irq = gbi->gbi_irq;

	/* TODO: Where should priority be set? */
	cdpri = GIC_CDPRI_PRORITY(GICV5_PRI_LOWEST);
	cdpri |= GIC_CDPRI_ID(irq);

	cdaff = GIC_CDAFF_IAFFID(iaffid);
	cdaff |= GIC_CDAFF_IRM_TARGETED;
	cdaff |= GIC_CDAFF_ID(irq);

	switch (gbi->gbi_space) {
	default:
		panic("%s: Invalid space %x", __func__, gbi->gbi_space);
	case GICv5_SPI:
		cdpri |= GIC_CDPRI_TYPE_SPI;
		cdaff |= GIC_CDAFF_TYPE_SPI;
		break;
	case GICv5_LPI:
		cdpri |= GIC_CDPRI_TYPE_LPI;
		cdaff |= GIC_CDAFF_TYPE_LPI;
		break;
	}

	gic_cdpri(cdpri);
	gic_cdaff(cdaff);
	isb();

	return (0);
}

#ifdef SMP
static void
gicv5_init_secondary(device_t dev, uint32_t rootnum)
{
	struct gicv5_softc *sc;
	u_int cpu;

	sc = device_get_softc(dev);
	cpu = curcpu;

	WRITE_SPECIALREG(ICC_PPI_PRIORITYR0_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR1_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR2_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR3_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR4_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR5_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR6_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR7_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR8_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR9_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR10_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR11_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR12_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR13_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR14_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));
	WRITE_SPECIALREG(ICC_PPI_PRIORITYR15_EL1,
	    ICC_PPI_PRIORITYR_PRIORITY_ALL(GICV5_PRI_LOWEST));

	/* Disable all PPIs, then enable as needed */
	WRITE_SPECIALREG(ICC_PPI_ENABLER0_EL1, ICC_PPI_ENABLER_NONE);
	WRITE_SPECIALREG(ICC_PPI_ENABLER1_EL1, ICC_PPI_ENABLER_NONE);
	isb();

	/* Set the priority to the lowest value */
	WRITE_SPECIALREG(ICC_PCR_EL1, ICC_PCR_PRIORITY_LOWEST);

	/* Enable interrupts */
	WRITE_SPECIALREG(ICC_CR0_EL1, ICC_CR0_EN);
	isb();

	/* Enable IPIs */
	for (u_int irq = 0; irq < INTR_IPI_COUNT; irq++) {
		struct gicv5_base_irqsrc *gbi;

		gbi = &sc->gic_ipi_irqs[irq].gi_isrc;
		if (intr_isrc_init_on_cpu(&gbi->gbi_isrc, cpu)) {
			gicv5_enable_ipi(gbi);
		}
	}

	/* Enable PPIs */
	for (u_int irq = 0; irq < GICV5_PPI_COUNT; irq++) {
		struct intr_irqsrc *isrc;

		isrc = &sc->gic_ppi_irqs[irq].gi_isrc.gbi_isrc;
		if (intr_isrc_init_on_cpu(isrc, cpu)) {
			gicv5_enable_intr_action(isrc);
		}
	}

	/* Set the priority to the lowest value */
	WRITE_SPECIALREG(ICC_PCR_EL1, ICC_PCR_PRIORITY_LOWEST);

	/* Enable interrupts */
	WRITE_SPECIALREG(ICC_CR0_EL1, ICC_CR0_EN);
	isb();
}

static void
gicv5_ipi_send(device_t dev, struct intr_irqsrc *isrc, cpuset_t cpus,
    u_int ipi)
{
	struct gicv5_irqsrc *gi = (struct gicv5_irqsrc *)isrc;
	uint64_t val;
	u_int cpu, irq;

	MPASS(gi->gi_isrc.gbi_ipi);

	val = 0;
	switch (gi->gi_isrc.gbi_space) {
	default:
		panic("%s: Invalid space: %x", __func__, gi->gi_isrc.gbi_space);
	case GICv5_LPI:
		val |= GIC_CDPEND_TYPE_LPI;
		break;
	}
	val |= GIC_CDPEND_PENDING_SET;
	CPU_FOREACH_ISSET(cpu, &cpus) {
		irq = IPI_TO_LPI(gi->gi_isrc.gbi_irq, cpu);
		gic_cdpend(val | GIC_CDPEND_ID(irq));
		isb();
	}
}

static int
gicv5_ipi_setup(device_t dev, u_int ipi, struct intr_irqsrc **isrcp)
{
	struct gicv5_softc *sc;
	struct gicv5_irqsrc *gi;
	u_int irq;
	int iaffid;

	sc = device_get_softc(dev);
	gi = &sc->gic_ipi_irqs[ipi];
	MPASS(gi->gi_isrc.gbi_ipi);

	for (u_int cpu = 0; cpu <= mp_maxid; cpu++) {
		if (CPU_ABSENT(cpu))
			continue;
		iaffid = gicv5_iaffids[cpu];
		KASSERT(iaffid >= 0,
		    ("%s: No iaffid for cpu %u", __func__, cpu));
		CPU_SET(cpu, &gi->gi_isrc.gbi_isrc.isrc_cpu);
		irq = IPI_TO_LPI(ipi, cpu);

		gic_cdpri(GIC_CDPRI_PRORITY(GICV5_PRI_LOWEST) |
		    GIC_CDPRI_TYPE_LPI | GIC_CDPRI_ID(irq));
		gic_cdaff(GIC_CDAFF_IAFFID(iaffid) | GIC_CDAFF_TYPE_LPI |
		    GIC_CDAFF_ID(irq));
		isb();
	}

	*isrcp = &gi->gi_isrc.gbi_isrc;
	return (0);
}

static cpu_feat_en
gicv5_feat_check(const struct cpu_feat *feat __unused, u_int midr __unused)
{
	if (gicv5_iaffids == NULL)
		return (FEAT_ALWAYS_DISABLE);

	return (FEAT_ALWAYS_ENABLE);
}

static bool
gicv5_feat_enable(const struct cpu_feat *feat __unused,
    cpu_feat_errata errata_status __unused, u_int *errata_list __unused,
    u_int errata_count __unused)
{
	u_int cpu;

	/* This is handled by attach */
	cpu = curcpu;
	if (cpu == 0)
		return (true);

	MPASS(cpu <= mp_maxid);
	MPASS(gicv5_iaffids[cpu] == -1);
	gicv5_iaffids[cpu] =
	    ICC_IAFFIDR_IAFFID_VAL(READ_SPECIALREG(ICC_IAFFIDR_EL1));

	return (true);
}

CPU_FEAT(gicv5, "GICv5",
    gicv5_feat_check, NULL, gicv5_feat_enable, NULL,
    CPU_FEAT_AFTER_DEV | CPU_FEAT_PER_CPU);
#endif
