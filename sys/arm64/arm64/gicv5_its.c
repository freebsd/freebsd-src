/*-
 * Copyright (c) 2015-2016 The FreeBSD Foundation
 * Copyright (c) 2023,2025 Arm Ltd
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
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/vmem.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/vmparam.h>

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "gicv5var.h"
#include "gic_v3_var.h" /* For GICV3_IVAR_NIRQS */

#include "pic_if.h"
#include "msi_if.h"

/* ITS Config Frame */
#define	ITS_IDR0			0x0000
#define	 ITS_IDR0_PA_RANGE_SHIFT	2
#define	 ITS_IDR0_PA_RANGE_MASK		(0xfu << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_4G		(0x0u << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_64G		(0x1u << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_1T		(0x2u << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_4T		(0x3u << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_16T		(0x4u << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_256T		(0x5u << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_4P		(0x6u << ITS_IDR0_PA_RANGE_SHIFT)
#define	 ITS_IDR0_PA_RANGE_64P		(0x7u << ITS_IDR0_PA_RANGE_SHIFT)
#define	ITS_IDR1			0x0004
#define	 ITS_IDR1_L2SZ_SHIFT		8
#define	 ITS_IDR1_L2SZ_MASK		(0x7u << ITS_IDR1_L2SZ_SHIFT)
#define	 ITS_IDR1_L2SZ_64K_MASK		(0x4u << ITS_IDR1_L2SZ_SHIFT)
#define	 ITS_IDR1_L2SZ_16K_MASK		(0x2u << ITS_IDR1_L2SZ_SHIFT)
#define	 ITS_IDR1_L2SZ_4K_MASK		(0x1u << ITS_IDR1_L2SZ_SHIFT)
#define	 ITS_IDR1_ITT_LEVELS		(0x1u << 7)
#define	 ITS_IDR1_DT_LEVELS		(0x1u << 6)
#define	 ITS_IDR1_DEVICEID_BITS_SHIFT	0
#define	 ITS_IDR1_DEVICEID_BITS		(0x3fu << ITS_IDR1_DEVICEID_BITS_SHIFT)
#define	 ITS_IDR1_DEVICEID_BITS_VAL(x)	\
    (((x) & ITS_IDR1_DEVICEID_BITS) >> ITS_IDR1_DEVICEID_BITS_SHIFT)
#define	ITS_IDR2			0x0008
#define	 ITS_IDR2_XDMN_EVENTS_SHIFT	5
#define	 ITS_IDR2_XDMN_EVENTS_MASK	(0x3u << ITS_IDR2_XDMN_EVENTS_SHIFT)
#define	 ITS_IDR2_EVENTID_SHIFT		0
#define	 ITS_IDR2_EVENTID_MASK		(0x1fu << ITS_IDR2_EVENTID_SHIFT)
#define	ITS_IIDR			0x0040
#define	ITS_AIDR			0x0044
#define	ITS_CR0				0x0080
#define	 ITS_CR0_IDLE			(0x1u << 1)
#define	 ITS_CR0_ITSEN			(0x1u << 0)
#define	ITS_CR1				0x0084
#define	 ITS_CR1_ITT_RA			(0x1u << 7)
#define	 ITS_CR1_DT_RA			(0x1u << 6)
#define	 ITS_CR1_IC_SHIFT		4
#define	 ITS_CR1_IC_MASK		(0x3u << ITS_CR1_IC_SHIFT)
#define	 ITS_CR1_IC_NC			(0x0u << ITS_CR1_IC_SHIFT)
#define	 ITS_CR1_IC_WB			(0x1u << ITS_CR1_IC_SHIFT)
#define	 ITS_CR1_IC_WT			(0x2u << ITS_CR1_IC_SHIFT)
#define	 ITS_CR1_OC_SHIFT		2
#define	 ITS_CR1_OC_MASK		(0x3u << ITS_CR1_OC_SHIFT)
#define	 ITS_CR1_OC_NC			(0x0u << ITS_CR1_OC_SHIFT)
#define	 ITS_CR1_OC_WB			(0x1u << ITS_CR1_OC_SHIFT)
#define	 ITS_CR1_OC_WT			(0x2u << ITS_CR1_OC_SHIFT)
#define	 ITS_CR1_SH_SHIFT		0
#define	 ITS_CR1_SH_MASK		(0x3u << ITS_CR1_SH_SHIFT)
#define	 ITS_CR1_SH_NS			(0x0u << ITS_CR1_SH_SHIFT)
#define	 ITS_CR1_SH_OS			(0x2u << ITS_CR1_SH_SHIFT)
#define	 ITS_CR1_SH_IS			(0x3u << ITS_CR1_SH_SHIFT)
#define	ITS_DT_BASER			0x00c0
#define	 ITS_DT_BASER_ADDR_MASK		0x00fffffffffffff8ul
#define	 ITS_DT_BASER_ADDR_LIMIT	0x0100000000000000ul
#define	ITS_DT_CFGR			0x00d0
#define	 IRS_DT_CFGR_STRUCTURE_LINEAR	(0x0 << 16)
#define	 IRS_DT_CFGR_STRUCTURE_2LVL	(0x1 << 16)
#define	 ITS_DT_CFGR_L2SZ_SHIFT		6
#define	 ITS_DT_CFGR_L2SZ_64K_VAL	0x2
#define	 ITS_DT_CFGR_L2SZ_64K		(0x2 << ITS_DT_CFGR_L2SZ_SHIFT)
#define	 ITS_DT_CFGR_L2SZ_16K_VAL	0x1
#define	 ITS_DT_CFGR_L2SZ_16K		(0x1 << ITS_DT_CFGR_L2SZ_SHIFT)
#define	 ITS_DT_CFGR_L2SZ_4K_VAL	0x0
#define	 ITS_DT_CFGR_L2SZ_4K		(0x0 << ITS_DT_CFGR_L2SZ_SHIFT)
#define	 IRS_DT_CFGR_DEVICE_ID_BITS_SHIFT 0
#define	ITS_DIDR			0x0100
#define	ITS_EIDR			0x0108
#define	ITS_INV_EVENTR			0x010c
#define	ITS_INV_DEVICER			0x0110
#define	 ITS_INV_DEVICER_I		(0x1u << 31)
#define	 ITS_INV_DEVICER_EVENTID_BITS_SHIFT	1
#define	 ITS_INV_DEVICER_EVENTID_BITS_MASK	\
    (0x1ful << ITS_INV_DEVICER_EVENTID_BITS_SHIFT)
#define	 ITS_INV_DEVICER_L1		(0x1u << 0)
#define	ITS_READ_EVENTR			0x0114
#define	ITS_READ_EVENT_DATAR		0x0118
#define	ITS_STATUSR			0x0120
#define	 ITS_STATUSR_IDLE		(0x1u << 0)
#define	ITS_SYNCR			0x0140
#define	ITS_SYNC_STATUSR		0x0148
#define	ITS_GEN_EVENT_DIDR		0x0180
#define	ITS_GEN_EVENT_EIDR		0x0188
#define	ITS_GEN_EVENTR			0x018c
#define	ITS_GEN_EVENT_STATUSR		0x0190
#define	ITS_MEC_IDR			0x01c0
#define	ITS_MEC_MECID_R			0x01c4
#define	ITS_MPAM_IDR			0x0200
#define	ITS_MPAM_PARTID_R		0x0204
#define	ITS_SWERR_STATUSR		0x0240
#define	ITS_SWERR_SYNDROMER0		0x0248
#define	ITS_SWERR_SYNDROMER1		0x0250

/* ITS Translate Frame */
#define	ITS_TRANSLATER			0x0000
#define	ITS_RL_TRANSLATER		0x0008

/* L1_DTE - Level 1 device table entry */
#define	L1_DTE_SIZE			8
#define	L1_DTE_SPAN_SHIFT		60
#define	L1_DTE_SPAN_MASK		(0xful << L1_DTE_SPAN_SHIFT)
#define	L1_DTE_L2_ADDR_MASK		0xfffffffffffff8
#define	L1_DTE_VALID			(0x1ul << 0)

/* L2_DTE - Level 2 device table entry */
#define	L2_DTE_SIZE			8
#define	L2_DTE_EVENTID_BITS_SHIFT	59
#define	L2_DTE_EVENTID_BITS_MASK	(0x1ful << L2_DTE_EVENTID_BITS_SHIFT)
#define	L2_DTE_ITT_STRUCTURE_SHIFT	58
#define	L2_DTE_ITT_STRUCTURE_MASK	(0x1ul << L2_DTE_ITT_STRUCTURE_SHIFT)
#define	L2_DTE_ITT_STRUCTURE_LINEAR	(0x0ul << L2_DTE_ITT_STRUCTURE_SHIFT)
#define	L2_DTE_ITT_STRUCTURE_2_LEVEL	(0x1ul << L2_DTE_ITT_STRUCTURE_SHIFT)
#define	L2_DTE_DSWE_SHIFT		57
#define	L2_DTE_DSWE			(0x1 << L2_DTE_DSWE_SHIFT)
#define	L2_DTE_ITT_ADDR_MASK		0x00fffffffffffff8
#define	L2_DTE_ITT_L2SZ_SHIFT		1
#define	L2_DTE_ITT_L2SZ_MASK		(0x3ul << L2_DTE_ITT_L2SZ_SHIFT)
#define	L2_DTE_ITT_L2SZ_4K		(0x0ul << L2_DTE_ITT_L2SZ_SHIFT)
#define	L2_DTE_ITT_L2SZ_16K		(0x1ul << L2_DTE_ITT_L2SZ_SHIFT)
#define	L2_DTE_ITT_L2SZ_64K		(0x2ul << L2_DTE_ITT_L2SZ_SHIFT)
#define	L2_DTE_VALID			(0x1ul << 0)

/* log2(number of entries in l2 table */
#define	L2_DTE_LOG2_ENTRIES(l2sz)	(9 + (2 * l2sz))
#define	L2_DTE_ENTRIES(l2sz)		(1ul << L2_DTE_LOG2_ENTRIES(l2sz))


/* The maximum physical address we can use for the ITT */
/* TODO: Move to use ITS_IDR0.PA_RANGE */
#define	ITT_MAX_ADDR			0x00ffffffffffffff

/* L1_ITTE - Level 1 interrupt translation table entry */
#define	L1_ITTE_SIZE			8
#define	L1_ITTE_SPAN_SHIFT		60
#define	L1_ITTE_SPAN_MASK		(0xful << L1_ITTE_SPAN_SHIFT)
#define	L1_ITTE_L2_ADDR_MASK		0xfffffffffffff8
#define	L1_ITTE_VALID			(0x1ul << 0)

/* L2_ITTE - Level 2 interrupt translation table entry */
#define	L2_ITTE_SIZE			8
#define	L2_ITTE_VM_ID_SHIFT		32
#define	L2_ITTE_VM_ID_MASK		(0xfffful << L2_ITT_VM_ID_SHIFT)
#define	L2_ITTE_VALID			(0x1ul << 31)
#define	L2_ITTE_VIRTUAL			(0x1ul << 30)
#define	L2_ITTE_DAC_SHIFT		28
#define	L2_ITTE_DAC_MASK		(0x3ul << L2_ITT_DAC_SHIFT)
#define	L2_ITTE_LPI_ID_SHIFT		0
#define	L2_ITTE_LPI_ID_MASK		(0xfffffful << L2_ITT_LPI_ID_SHIFT)

/* LPI chunk owned by ITS device */
struct lpi_chunk {
	u_int	lpi_base;
	u_int	lpi_free;	/* First free LPI in set */
	u_int	lpi_num;	/* Total number of LPIs in chunk */
	u_int	lpi_busy;	/* Number of busy LPIs in chink */
};

/* ITS device */
struct its_dev {
	TAILQ_ENTRY(its_dev)	entry;
	/* PCI device */
	device_t		pci_dev;
	/* Device ID (i.e. PCI device ID) */
	uint32_t		devid;
	/* List of assigned LPIs */
	struct lpi_chunk	lpis;
	/* Virtual address of ITT */
	/* XXX: Only a linear ITT for now */
	uint64_t		*itt;
};

/* ITS device list */
struct its_device_list {
	struct mtx		its_dev_lock;
	TAILQ_HEAD(its_dev_list, its_dev) its_dev_list;

	uint64_t		*its_dev_dte_base;
	size_t			 its_dev_dte_l2size;
	u_int			 its_dev_dte_l2bits;
	bool			 its_dev_dte_2l;


	vmem_t			*its_dev_irq_alloc;
	struct gicv5_its_irqsrc	**its_irqs;
};

struct gicv5_its_irqsrc {
	struct gicv5_base_irqsrc gi_isrc;
	u_int			 gi_event_id;
	struct its_dev		*gi_its_dev;
	TAILQ_ENTRY(gicv5_its_irqsrc) gi_link;
};

struct gicv5_its_translate_frame {
	struct intr_pic		*its_pic;
	intptr_t		 its_xref;
	bus_addr_t		 its_frame_paddr;
};

struct gicv5_its_softc {
	struct its_device_list	 its_dl;
	struct resource		*its_cfg;
	struct gicv5_its_translate_frame its_frame;

	struct gicv5_its_irqsrc	**sc_irqs;

	cpuset_t		 its_cpus;
	u_int			 its_irq_cpu;
	TAILQ_HEAD(free_irqs, gicv5_its_irqsrc) sc_free_irqs;

	uint8_t			 its_parange;

	bool			 its_coherent;
};

static uint64_t *gicv5_its_dte_extend(struct gicv5_its_softc *,
    struct its_device_list *, uint32_t);

static device_attach_t gicv5_its_attach;

static pic_disable_intr_t gicv5_its_disable_intr;
static pic_enable_intr_t gicv5_its_enable_intr;
static pic_map_intr_t gicv5_its_map_intr;
static pic_setup_intr_t gicv5_its_setup_intr;
static pic_post_filter_t gicv5_its_post_filter;
static pic_post_ithread_t gicv5_its_post_ithread;
static pic_pre_ithread_t gicv5_its_pre_ithread;
static pic_bind_intr_t gicv5_its_bind_intr;

static msi_alloc_msi_t gicv5_its_alloc_msi;
static msi_release_msi_t gicv5_its_release_msi;
static msi_alloc_msix_t gicv5_its_alloc_msix;
static msi_release_msix_t gicv5_its_release_msix;
static msi_map_msi_t gicv5_its_map_msi;
#ifdef IOMMU
static msi_iommu_init_t gicv5_iommu_init;
static msi_iommu_deinit_t gicv5_iommu_deinit;
#endif

static device_method_t gicv5_its_methods[] = {
	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	gicv5_its_disable_intr),
	DEVMETHOD(pic_enable_intr,	gicv5_its_enable_intr),
	DEVMETHOD(pic_map_intr,		gicv5_its_map_intr),
	DEVMETHOD(pic_setup_intr,	gicv5_its_setup_intr),
	DEVMETHOD(pic_post_filter,	gicv5_its_post_filter),
	DEVMETHOD(pic_post_ithread,	gicv5_its_post_ithread),
	DEVMETHOD(pic_pre_ithread,	gicv5_its_pre_ithread),
#ifdef SMP
	DEVMETHOD(pic_bind_intr,	gicv5_its_bind_intr),
#endif

	/* MSI/MSI-X */
	DEVMETHOD(msi_alloc_msi,	gicv5_its_alloc_msi),
	DEVMETHOD(msi_release_msi,	gicv5_its_release_msi),
	DEVMETHOD(msi_alloc_msix,	gicv5_its_alloc_msix),
	DEVMETHOD(msi_release_msix,	gicv5_its_release_msix),
	DEVMETHOD(msi_map_msi,		gicv5_its_map_msi),
#ifdef IOMMU
	DEVMETHOD(msi_iommu_init,	gicv5_iommu_init),
	DEVMETHOD(msi_iommu_deinit,	gicv5_iommu_deinit),
#endif

	/* End */
	DEVMETHOD_END
};

static DEFINE_CLASS_0(gic, gicv5_its_driver, gicv5_its_methods,
    sizeof(struct gicv5_its_softc));

static void
its_write_cr0(struct gicv5_its_softc *sc, bool en)
{
	uint32_t val;
	int timeout;

	val = en ? ITS_CR0_ITSEN : 0;
	bus_write_4(sc->its_cfg, ITS_CR0, val);

	/* Timeout of ~10ms */
	timeout = 10000;
	do {
		val = bus_read_4(sc->its_cfg, ITS_CR0);
		if ((val & ITS_CR0_IDLE) == ITS_CR0_IDLE)
			return;
		DELAY(1);
	} while (--timeout > 0);

	panic("Timeout waiting for ITS CR0 becoming idle");
}

static void
its_wait_for_statusr(struct gicv5_its_softc *sc)
{
	uint32_t val;
	int timeout;

	/* Timeout of ~10ms */
	timeout = 10000;
	do {
		val = bus_read_4(sc->its_cfg, ITS_STATUSR);
		if ((val & ITS_STATUSR_IDLE) == ITS_STATUSR_IDLE)
			return;
		DELAY(1);
	} while (--timeout > 0);

	panic("Timeout waiting for ITS STATUSR becoming idle");
}

static void
its_dcache_wbinv(struct gicv5_its_softc *sc, void *addr, size_t size)
{
	if (sc->its_coherent)
		dsb(ishst);
	else
		cpu_dcache_wbinv_range(addr, size);
}

/* TODO: See if we could merge its device code with GICv3 ITS */
static void
its_device_list_init(struct its_device_list *dev_list)
{
	/* Protects access to the device list */
	mtx_init(&dev_list->its_dev_lock, "ITS device lock", NULL, MTX_SPIN);
	TAILQ_INIT(&dev_list->its_dev_list);
}

static struct its_dev *
its_device_find_locked(struct its_device_list *dev_list, device_t child)
{
	struct its_dev *its_dev;

	mtx_assert(&dev_list->its_dev_lock, MA_OWNED);

	TAILQ_FOREACH(its_dev, &dev_list->its_dev_list, entry) {
		if (its_dev->pci_dev == child)
			return (its_dev);
	}

	return (NULL);
}

static struct its_dev *
its_device_find(struct its_device_list *dev_list, device_t child)
{
	struct its_dev *its_dev;

	mtx_lock_spin(&dev_list->its_dev_lock);
	its_dev = its_device_find_locked(dev_list, child);
	mtx_unlock_spin(&dev_list->its_dev_lock);

	return (its_dev);
}

static uint32_t
its_get_devid(device_t pci_dev)
{
	uintptr_t id;

	if (pci_get_id(pci_dev, PCI_ID_MSI, &id) != 0)
		panic("%s: %s: Unable to get the MSI DeviceID", __func__,
		    device_get_nameunit(pci_dev));

	return (id);
}

static bool
its_device_itt_alloc_linear(struct gicv5_its_softc *sc,
    struct its_dev *its_dev, u_int eventid_bits, uint64_t *dtep)
{
	size_t size;

	size = ((size_t)1 << eventid_bits) * L2_ITTE_SIZE;
	MPASS(size <= PAGE_SIZE_4K);

	its_dev->itt = contigmalloc(size, M_DEVBUF, M_NOWAIT | M_ZERO, 0,
	    (1ul << sc->its_parange) - 1, max(size, PAGE_SIZE), 0);
	if (its_dev->itt == NULL)
		return (false);
	its_dcache_wbinv(sc, its_dev->itt, size);

	*dtep = (uint64_t)eventid_bits << L2_DTE_EVENTID_BITS_SHIFT |
	    L2_DTE_ITT_STRUCTURE_LINEAR | vtophys(its_dev->itt) |
	    L2_DTE_ITT_L2SZ_4K | L2_DTE_VALID;
	return (true);
}

static bool
its_device_dte_update(struct gicv5_its_softc *sc,
    struct its_device_list *dev_list, struct its_dev *its_dev, uint64_t dte)
{
	uint64_t *dtep;

	dtep = gicv5_its_dte_extend(sc, dev_list, its_dev->devid);
	if (dtep == NULL)
		return (false);

	MPASS(atomic_load_64(dtep) == 0);
	atomic_store_64(dtep, dte);
	its_dcache_wbinv(sc, dtep, sizeof(*dtep));
	return (true);
}

static struct its_dev *
its_device_get(device_t dev, struct its_device_list *dev_list, device_t child,
    u_int nvecs)
{
	struct gicv5_its_softc *sc;
	struct its_dev *its_dev, *tmp_dev;
	vmem_addr_t irq_base;
	uint64_t dte;
	u_int eventid_bits;

	its_dev = its_device_find(dev_list, child);
	if (its_dev != NULL)
		return (its_dev);

	its_dev = malloc(sizeof(*its_dev), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (its_dev == NULL)
		return (NULL);

	its_dev->pci_dev = child;
	its_dev->devid = its_get_devid(child);

	its_dev->lpis.lpi_busy = 0;
	its_dev->lpis.lpi_num = nvecs;
	its_dev->lpis.lpi_free = nvecs;

	sc = device_get_softc(dev);
	if (gicv5_its_dte_extend(sc, dev_list, its_dev->devid) == NULL) {
		free(its_dev, M_DEVBUF);
		return (NULL);
	}

	eventid_bits = order_base_2(nvecs);
	if (!its_device_itt_alloc_linear(sc, its_dev, eventid_bits, &dte)) {
		free(its_dev, M_DEVBUF);
		return (NULL);
	}

	if (vmem_alloc(dev_list->its_dev_irq_alloc, nvecs,
	    M_FIRSTFIT | M_NOWAIT, &irq_base) != 0) {
		free(its_dev->itt, M_DEVBUF);
		free(its_dev, M_DEVBUF);
		return (NULL);
	}

	mtx_lock_spin(&dev_list->its_dev_lock);
	/* Recheck the ITS device hasn't been allocated */
	tmp_dev = its_device_find_locked(dev_list, child);
	if (tmp_dev != NULL) {
		mtx_unlock_spin(&dev_list->its_dev_lock);
		/* Clean up the unused device */
		vmem_free(dev_list->its_dev_irq_alloc, its_dev->lpis.lpi_base,
		    nvecs);
		free(its_dev->itt, M_DEVBUF);
		free(its_dev, M_DEVBUF);
		return (tmp_dev);
	}

	/*
	 * Store with an atomic operation to ensure the Valid field is
	 * set with the other fields.
	 */
	if (!its_device_dte_update(sc, dev_list, its_dev, dte)) {
		mtx_unlock_spin(&dev_list->its_dev_lock);
		/* Clean up the unused device */
		vmem_free(dev_list->its_dev_irq_alloc, its_dev->lpis.lpi_base,
		    nvecs);
		free(its_dev->itt, M_DEVBUF);
		free(its_dev, M_DEVBUF);
		return (NULL);
	}

	its_dev->lpis.lpi_base = irq_base;

	TAILQ_INSERT_TAIL(&dev_list->its_dev_list, its_dev, entry);

	bus_write_8(sc->its_cfg, ITS_DIDR, its_dev->devid);
	bus_write_4(sc->its_cfg, ITS_INV_DEVICER, ITS_INV_DEVICER_I |
	    (eventid_bits << ITS_INV_DEVICER_EVENTID_BITS_SHIFT));

	its_wait_for_statusr(sc);
	mtx_unlock_spin(&dev_list->its_dev_lock);

	return (its_dev);
}


static int
gicv5_its_intr(void *arg, uintptr_t irq)
{
	struct gicv5_its_softc *sc = arg;
	struct gicv5_its_irqsrc *gi;
	struct trapframe *tf;

	gi = sc->sc_irqs[irq];
	if (gi == NULL)
		panic("%s: Invalid interrupt %ld", __func__, irq);

	tf = curthread->td_intr_frame;
	intr_isrc_dispatch(&gi->gi_isrc.gbi_isrc, tf);
	return (FILTER_HANDLED);
}

static int
gicv5_its_select_cpu(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv5_its_softc *sc;

	sc = device_get_softc(dev);
	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		sc->its_irq_cpu = intr_irq_next_cpu(sc->its_irq_cpu,
		    &sc->its_cpus);
		CPU_SETOF(sc->its_irq_cpu, &isrc->isrc_cpu);
	}

	return (0);
}

static void
gicv5_its_dte_alloc(struct gicv5_its_softc *sc, size_t size)
{
	sc->its_dl.its_dev_dte_base = contigmalloc(size, M_DEVBUF,
	    M_WAITOK | M_ZERO, 0, (1ul << sc->its_parange) - 1,
	    size, 0);
	its_dcache_wbinv(sc, sc->its_dl.its_dev_dte_base, size);
}

static void
gicv5_its_dte_alloc_linear(struct gicv5_its_softc *sc, uint32_t *cfgrp,
    u_int devid_bits)
{
	size_t size;
	uint32_t cfgr;
	u_int n;

	/*
	 * This is the alignment calculation from the ITS_DT_BASER definition.
	 */
	n = 2 + devid_bits;
	size = 1ul << (n + 1);

	gicv5_its_dte_alloc(sc, size);

	sc->its_dl.its_dev_dte_2l = false;

	cfgr = IRS_DT_CFGR_STRUCTURE_LINEAR;
	cfgr |= devid_bits << IRS_DT_CFGR_DEVICE_ID_BITS_SHIFT;
	*cfgrp = cfgr;
}

static void
gicv5_its_dte_alloc_2level(struct gicv5_its_softc *sc, uint32_t *cfgrp,
    u_int devid_bits, u_int l2sz)
{
	size_t size;
	uint32_t cfgr;
	u_int n;

	/*
	 * This is the alignment calculation from the ITS_DT_BASER
	 * definition.
	 */
	n = MAX(2, devid_bits - L2_DTE_LOG2_ENTRIES(l2sz) + 2);
	size = 1ul << (n + 1);

	gicv5_its_dte_alloc(sc, size);

	sc->its_dl.its_dev_dte_l2bits = L2_DTE_LOG2_ENTRIES(l2sz);
	sc->its_dl.its_dev_dte_l2size = L2_DTE_ENTRIES(l2sz) * L2_DTE_SIZE;
	sc->its_dl.its_dev_dte_2l = true;

	cfgr = IRS_DT_CFGR_STRUCTURE_2LVL;
	cfgr |= l2sz << ITS_DT_CFGR_L2SZ_SHIFT;
	cfgr |= devid_bits << IRS_DT_CFGR_DEVICE_ID_BITS_SHIFT;
	*cfgrp = cfgr;
}

static uint64_t *
gicv5_its_dte_extend(struct gicv5_its_softc *sc,
    struct its_device_list *dev_list, uint32_t devid)
{
	uint64_t *l2_dtep;
	size_t size;
	uint64_t dte, new_dte;
	u_int index;

	if (!dev_list->its_dev_dte_2l)
		return (&dev_list->its_dev_dte_base[devid]);

	index = devid >> dev_list->its_dev_dte_l2bits;
	size = dev_list->its_dev_dte_l2size;

	/* Check if there the l2 pointer is valid */
	dte = atomic_load_64(&dev_list->its_dev_dte_base[index]);
	if ((dte & L1_DTE_VALID) != 0) {
		l2_dtep = (uint64_t *)PHYS_TO_DMAP(dte & L1_DTE_L2_ADDR_MASK);
		goto out;
	}

	l2_dtep = contigmalloc(size, M_DEVBUF, M_NOWAIT | M_ZERO, 0,
	    (1ul << sc->its_parange) - 1, size, 0);
	if (l2_dtep == NULL)
		return (NULL);

	its_dcache_wbinv(sc, l2_dtep, size);

	new_dte = (uint64_t)dev_list->its_dev_dte_l2bits << L1_DTE_SPAN_SHIFT;
	new_dte |= vtophys(l2_dtep);
	new_dte |= L1_DTE_VALID;
	while (!atomic_fcmpset_64(&dev_list->its_dev_dte_base[index], &dte,
	    new_dte)) {
		if ((dte & L1_DTE_VALID) != 0) {
			free(l2_dtep, M_DEVBUF);
			l2_dtep =
			    (uint64_t *)PHYS_TO_DMAP(dte & L1_DTE_L2_ADDR_MASK);
			goto out;
		}
	}

out:
	return (&l2_dtep[devid % (1 << dev_list->its_dev_dte_l2bits)]);
}

static int
gicv5_its_attach(device_t dev)
{
	struct gicv5_its_softc *sc;
	uint32_t cfgr, idr;
	u_int devid_bits, l2sz, lpi_start, nlpis;
	int error, rid;
	bool two_levels;

	sc = device_get_softc(dev);

	lpi_start = gicv5_get_lpi_start(dev);
	nlpis = gicv3_get_nirqs(dev);

	rid = 0;
	sc->its_cfg = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->its_cfg == NULL) {
		device_printf(dev, "Unable to map config frame\n");
		return (ENXIO);
	}

	switch (bus_read_4(sc->its_cfg, ITS_IDR0) & ITS_IDR0_PA_RANGE_MASK) {
	default:
	case ITS_IDR0_PA_RANGE_4G:
		sc->its_parange = 32;
		break;
	case ITS_IDR0_PA_RANGE_64G:
		sc->its_parange = 36;
		break;
	case ITS_IDR0_PA_RANGE_1T:
		sc->its_parange = 40;
		break;
	case ITS_IDR0_PA_RANGE_4T:
		sc->its_parange = 42;
		break;
	case ITS_IDR0_PA_RANGE_16T:
		sc->its_parange = 44;
		break;
	case ITS_IDR0_PA_RANGE_256T:
		sc->its_parange = 48;
		break;
	case ITS_IDR0_PA_RANGE_4P:
		sc->its_parange = 52;
		break;
	case ITS_IDR0_PA_RANGE_64P:
		sc->its_parange = 56;
		break;
	}

	idr = bus_read_4(sc->its_cfg, ITS_IDR1);
	if ((idr & ITS_IDR1_ITT_LEVELS) != 0)
		device_printf(dev, "2 level itt\n");
	if ((idr & ITS_IDR1_DT_LEVELS) != 0)
		device_printf(dev, "2 level device table\n");
	if ((idr & (ITS_IDR1_ITT_LEVELS | ITS_IDR1_DT_LEVELS)) != 0) {
		if ((idr & ITS_IDR1_L2SZ_64K_MASK) != 0)
			device_printf(dev, "64K l2 size\n");
		if ((idr & ITS_IDR1_L2SZ_16K_MASK) != 0)
			device_printf(dev, "16K l2 size\n");
		if ((idr & ITS_IDR1_L2SZ_4K_MASK) != 0)
			device_printf(dev, "4K l2 size\n");
		if ((idr & ITS_IDR1_L2SZ_MASK) == 0)
			device_printf(dev, "2 level tables, but no l2 size\n");
	}

	its_device_list_init(&sc->its_dl);
	TAILQ_INIT(&sc->sc_free_irqs);

	error = bus_get_cpus(dev, LOCAL_CPUS, sizeof(sc->its_cpus),
	    &sc->its_cpus);
	if (error != 0) {
		device_printf(dev, "Failed to read CPU list\n");
		goto exit;
	}

	if (sc->its_coherent) {
		bus_write_4(sc->its_cfg, ITS_CR1, ITS_CR1_ITT_RA |
		    ITS_CR1_DT_RA | ITS_CR1_IC_WB | ITS_CR1_OC_WB |
		    ITS_CR1_SH_IS);
	} else {
		bus_write_4(sc->its_cfg, ITS_CR1, ITS_CR1_IC_NC |
		    ITS_CR1_OC_NC | ITS_CR1_SH_NS);
	}

	two_levels = (idr & ITS_IDR1_DT_LEVELS) != 0;

	if (two_levels) {
		if ((idr & ITS_IDR1_L2SZ_64K_MASK) != 0)
			l2sz = ITS_DT_CFGR_L2SZ_64K_VAL;
		else if ((idr & ITS_IDR1_L2SZ_16K_MASK) != 0)
			l2sz = ITS_DT_CFGR_L2SZ_16K_VAL;
		else
			l2sz = ITS_DT_CFGR_L2SZ_4K_VAL;
	}

	devid_bits = ITS_IDR1_DEVICEID_BITS_VAL(idr);

	/*
	 * Use 2 level tables if able, and the size is large enough for them
	 * to be worth it. This is based on the calculation in the GICv5
	 * spec (ARM-AES-0070) 00EAC0 section 10.3.1.6 ITS_DT_CFGR.
	 */
	if (two_levels && devid_bits > (9 + (2 * l2sz)))
		gicv5_its_dte_alloc_2level(sc, &cfgr, devid_bits, l2sz);
	else
		gicv5_its_dte_alloc_linear(sc, &cfgr, devid_bits);

	bus_write_4(sc->its_cfg, ITS_DT_CFGR, cfgr);

	bus_write_8(sc->its_cfg, ITS_DT_BASER,
	    vtophys(sc->its_dl.its_dev_dte_base));
	sc->its_dl.its_dev_irq_alloc = vmem_create(device_get_nameunit(dev),
	    lpi_start, nlpis, 1, 0, M_FIRSTFIT | M_WAITOK);
	sc->sc_irqs = mallocarray(nlpis, sizeof(*sc->sc_irqs), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	its_write_cr0(sc, true);

	/* Register this device as a interrupt controller */
	sc->its_frame.its_pic = intr_pic_register(dev, sc->its_frame.its_xref);
	error = intr_pic_add_handler(device_get_parent(dev),
	    sc->its_frame.its_pic, gicv5_its_intr, sc, lpi_start, nlpis);
	if (error != 0) {
		device_printf(dev, "Failed to add PIC handler\n");
		goto exit;
	}

	/* Register this device to handle MSI interrupts */
	error = intr_msi_register(dev, sc->its_frame.its_xref);
	if (error != 0) {
		device_printf(dev, "Failed to register for MSIs\n");
		goto exit;
	}

	return (0);

exit:
	if (sc->its_frame.its_pic != NULL)
		intr_pic_deregister(dev, sc->its_frame.its_xref);
	free(sc->sc_irqs, M_DEVBUF);
	if (sc->its_dl.its_dev_irq_alloc != NULL)
		vmem_destroy(sc->its_dl.its_dev_irq_alloc);
	mtx_destroy(&sc->its_dl.its_dev_lock);
	bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->its_cfg);
	return (error);
}

static void
gicv5_its_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	return (PIC_DISABLE_INTR(device_get_parent(dev), isrc));
}

static void
gicv5_its_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	return (PIC_ENABLE_INTR(device_get_parent(dev), isrc));
}

static void
gicv5_its_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	PIC_PRE_ITHREAD(device_get_parent(dev), isrc);
}

static void
gicv5_its_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	PIC_POST_ITHREAD(device_get_parent(dev), isrc);
}

static void
gicv5_its_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	PIC_POST_FILTER(device_get_parent(dev), isrc);
}

static int
gicv5_its_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	gicv5_its_select_cpu(dev, isrc);

	return (PIC_BIND_INTR(device_get_parent(dev), isrc));
}

static int
gicv5_its_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	/*
	 * This should never happen, we only call this function to map
	 * interrupts found before the controller driver is ready.
	 */
	panic("%s: Unable to map a MSI interrupt", __func__);
}

static int
gicv5_its_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	/* Bind the interrupt to a CPU */
	gicv5_its_bind_intr(dev, isrc);

	return (0);
}

static struct gicv5_its_irqsrc *
gicv5_its_alloc_irqsrc(device_t dev, struct gicv5_its_softc *sc,
    u_int event_id, u_int lpi)
{
	struct gicv5_its_irqsrc *girq = NULL;

	KASSERT(sc->sc_irqs[lpi] == NULL,
	    ("%s: LPI %u already allocated", __func__, lpi));
	mtx_lock_spin(&sc->its_dl.its_dev_lock);
	if (!TAILQ_EMPTY(&sc->sc_free_irqs)) {
		girq = TAILQ_FIRST(&sc->sc_free_irqs);
		TAILQ_REMOVE(&sc->sc_free_irqs, girq, gi_link);
	}
	mtx_unlock_spin(&sc->its_dl.its_dev_lock);
	if (girq == NULL) {
		girq = malloc(sizeof(*girq), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (girq == NULL)
			return (NULL);
		girq->gi_isrc.gbi_space = GICv5_LPI;
		girq->gi_isrc.gbi_ipi = false;
		if (intr_isrc_register(&girq->gi_isrc.gbi_isrc, dev, 0,
		    "%s,%u", device_get_nameunit(dev), lpi) != 0) {
			free(girq, M_DEVBUF);
			return (NULL);
		}
	}

	gicv5_irs_extend_ist(device_get_parent(dev), dev, lpi);

	girq->gi_isrc.gbi_irq = lpi;
	girq->gi_event_id = event_id;
	sc->sc_irqs[lpi] = girq;

	return (girq);
}

static void
gicv5_its_release_irqsrc(struct gicv5_its_softc *sc,
    struct gicv5_its_irqsrc *girq)
{
	int error;

	error = intr_isrc_deregister(&girq->gi_isrc.gbi_isrc);
	if (error != 0)
		panic("Failed to deregister ITS irqsrc");

	girq->gi_isrc.gbi_irq = -1;
	girq->gi_event_id = -1;
	girq->gi_its_dev = NULL;

	mtx_lock_spin(&sc->its_dl.its_dev_lock);
	TAILQ_INSERT_TAIL(&sc->sc_free_irqs, girq, gi_link);
	mtx_unlock_spin(&sc->its_dl.its_dev_lock);
}

static void
gicv5_its_update_itt(struct gicv5_its_softc *sc, struct its_dev *its_dev,
    struct gicv5_its_irqsrc *girq, u_int event_id)
{
	/*
	 * Update the ITT entry. Use an atomic operation to ensure the
	 * hardware sees the full value in a single operation.
	 */
	atomic_store_64(&its_dev->itt[event_id],
	    L2_ITTE_VALID | girq->gi_isrc.gbi_irq);
	its_dcache_wbinv(sc, &its_dev->itt[event_id],
	    sizeof(its_dev->itt[event_id]));

	/* Invalidate the event */
	bus_write_8(sc->its_cfg, ITS_DIDR, its_dev->devid);
	bus_write_4(sc->its_cfg, ITS_EIDR, event_id);
	bus_write_4(sc->its_cfg, ITS_INV_EVENTR, 0x1ul << 31);

	its_wait_for_statusr(sc);
}

static int
gicv5_its_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **srcs)
{
	struct gicv5_its_softc *sc;
	struct gicv5_its_irqsrc *girq;
	struct its_dev *its_dev;
	u_int event_id, lpi;
	int i;

	sc = device_get_softc(dev);
	its_dev = its_device_get(dev, &sc->its_dl, child, count);
	if (its_dev == NULL)
		return (ENXIO);

	KASSERT(its_dev->lpis.lpi_free > 0, ("%s: No free LPIs", __func__));

	event_id = its_dev->lpis.lpi_num - its_dev->lpis.lpi_free;
	lpi = its_dev->lpis.lpi_base + event_id;

	/* Allocate the irqsrc for each MSI */
	for (i = 0; i < count; i++, lpi++) {
		its_dev->lpis.lpi_free--;
		srcs[i] = (struct intr_irqsrc *)gicv5_its_alloc_irqsrc(dev, sc,
		    event_id, lpi);
		if (srcs[i] == NULL)
			break;
	}

	/* The allocation failed, release them */
	if (i != count) {
		for (int j = 0; j < i; j++)
			gicv5_its_release_irqsrc(sc,
			    (struct gicv5_its_irqsrc *)srcs[j]);
		return (ENXIO);
	}

	/* Finish the allocation now we have all MSI irqsrcs */
	for (i = 0; i < count; i++) {
		girq = (struct gicv5_its_irqsrc *)srcs[i];
		girq->gi_its_dev = its_dev;

		/* Map the message to the given IRQ */
		gicv5_its_select_cpu(dev, (struct intr_irqsrc *)girq);

		gicv5_its_update_itt(sc, its_dev, girq, event_id + i);
	}
	its_dev->lpis.lpi_busy += count;
	*pic = dev;

	return (0);
}

static int
gicv5_its_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **isrc)
{
	struct gicv5_its_softc *sc;
	struct gicv5_its_irqsrc *girq;
	struct its_dev *its_dev;

	sc = device_get_softc(dev);
	its_dev = its_device_find(&sc->its_dl, child);

	KASSERT(its_dev != NULL,
	    ("%s: Releasing a MSI interrupt with no ITS device", __func__));
	KASSERT(its_dev->lpis.lpi_busy >= count,
	    ("%s: Releasing more interrupts than were allocated: "
	     "releasing %d, allocated %d", __func__, count,
	     its_dev->lpis.lpi_busy));

	for (int i = 0; i < count; i++) {
		girq = (struct gicv5_its_irqsrc *)isrc[i];
		gicv5_its_release_irqsrc(sc, girq);
	}
	its_dev->lpis.lpi_busy -= count;

	return (0);
}

static int
gicv5_its_alloc_msix(device_t dev, device_t child, device_t *pic,
    struct intr_irqsrc **isrcp)
{
	struct gicv5_its_softc *sc;
	struct gicv5_its_irqsrc *girq;
	struct its_dev *its_dev;
	u_int nvecs, event_id, lpi;

	sc = device_get_softc(dev);
	nvecs = pci_msix_count(child);
	its_dev = its_device_get(dev, &sc->its_dl, child, nvecs);
	if (its_dev == NULL)
		return (ENXIO);

	KASSERT(its_dev->lpis.lpi_free > 0, ("%s: No free LPIs", __func__));
	event_id = its_dev->lpis.lpi_num - its_dev->lpis.lpi_free;
	lpi = its_dev->lpis.lpi_base + event_id;

	girq = gicv5_its_alloc_irqsrc(dev, sc, event_id, lpi);
	if (girq == NULL)
		return (ENXIO);
	girq->gi_its_dev = its_dev;

	its_dev->lpis.lpi_free--;
	its_dev->lpis.lpi_busy++;

	/* Map the message to the given IRQ */
	gicv5_its_select_cpu(dev, (struct intr_irqsrc *)girq);

	gicv5_its_update_itt(sc, its_dev, girq, event_id);

	*pic = dev;
	*isrcp = (struct intr_irqsrc *)girq;

	return (0);
}

static int
gicv5_its_release_msix(device_t dev, device_t child, struct intr_irqsrc *isrc)
{
	struct gicv5_its_softc *sc;
	struct gicv5_its_irqsrc *girq;
	struct its_dev *its_dev;

	sc = device_get_softc(dev);
	its_dev = its_device_find(&sc->its_dl, child);

	KASSERT(its_dev != NULL,
	    ("%s: Releasing a MSI interrupt with no ITS device", __func__));
	KASSERT(its_dev->lpis.lpi_busy > 0,
	    ("%s: Releasing more interrupts than were allocated: "
	     "releasing 1, allocated %d", __func__, its_dev->lpis.lpi_busy));

	girq = (struct gicv5_its_irqsrc *)isrc;
	gicv5_its_release_irqsrc(sc, girq);
	its_dev->lpis.lpi_busy--;

	return (0);
}

static int
gicv5_its_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct gicv5_its_softc *sc;
	struct gicv5_its_irqsrc *gi;
	struct gicv5_its_translate_frame *frame;

	sc = device_get_softc(dev);
	gi = (struct gicv5_its_irqsrc *)isrc;

	frame = &sc->its_frame;
	*addr = frame->its_frame_paddr + ITS_TRANSLATER;
	*data = gi->gi_event_id;

	return (0);
}

#ifdef IOMMU
static int
gicv5_iommu_init(device_t dev, device_t child, struct iommu_domain **domain)
{
	/* TODO */
	panic("%s", __func__);
}

static void
gicv5_iommu_deinit(device_t dev, device_t child)
{
	/* TODO */
	panic("%s", __func__);
}
#endif

#ifdef FDT
static device_probe_t gicv5_its_fdt_probe;
static device_attach_t gicv5_its_fdt_attach;

static device_method_t gicv5_its_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gicv5_its_fdt_probe),
	DEVMETHOD(device_attach,	gicv5_its_fdt_attach),

	/* End */
	DEVMETHOD_END
};

#define its_baseclasses itsv5_fdt_baseclasses
DEFINE_CLASS_1(its, gicv5_its_fdt_driver, gicv5_its_fdt_methods,
    sizeof(struct gicv5_its_softc), gicv5_its_driver);
#undef its_baseclasses

EARLY_DRIVER_MODULE(itsv5_fdt, gic, gicv5_its_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

static int
gicv5_its_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "arm,gic-v5-its"))
		return (ENXIO);

	if (!gic_get_support_lpis(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM GICv5 Interrupt Translation Service");
	return (BUS_PROBE_DEFAULT);
}

static int
gicv5_its_fdt_attach(device_t dev)
{
	struct gicv5_its_softc *sc;
	phandle_t node, child;
	bool found;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	found = false;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if (OF_hasprop(child, "msi-controller")) {
			bus_size_t size;
			int idx;

			/*
			 * Find the index for the expected register. If it's
			 * missing we can skip this frame.
			 */
			if (ofw_bus_find_string_index(child, "reg-names",
			    "ns-translate", &idx) != 0)
				continue;

			if (found) {
				device_printf(dev,
				    "Too many ITS frames found\n");
				return (EINVAL);
			}

			if (ofw_reg_to_paddr(child, idx,
			    &sc->its_frame.its_frame_paddr, &size, NULL) != 0) {
				device_printf(dev,
				    "Unable to read frame physical address\n");
				return (EINVAL);
			}
			sc->its_frame.its_pic = NULL;
			sc->its_frame.its_xref = OF_xref_from_node(child);
			found = true;
		}
	}
	if (!found) {
		device_printf(dev, "No valid ITS frame found\n");
		return (EINVAL);
	}

	sc->its_coherent = !OF_hasprop(node, "dma-noncoherent");

	return (gicv5_its_attach(dev));
}
#endif /* FDT */
