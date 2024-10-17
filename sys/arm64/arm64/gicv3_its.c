/*-
 * Copyright (c) 2015-2016 The FreeBSD Foundation
 * Copyright (c) 2023 Arm Ltd
 *
 * This software was developed by Andrew Turner under
 * the sponsorship of the FreeBSD Foundation.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
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
#include "opt_iommu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/physmem.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/vmem.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/arm/gic_common.h>
#include <arm64/arm64/gic_v3_reg.h>
#include <arm64/arm64/gic_v3_var.h>

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#ifdef IOMMU
#include <dev/iommu/iommu.h>
#include <dev/iommu/iommu_gas.h>
#endif

#include "pcib_if.h"
#include "pic_if.h"
#include "msi_if.h"

MALLOC_DEFINE(M_GICV3_ITS, "GICv3 ITS",
    "ARM GICv3 Interrupt Translation Service");

#define	LPI_NIRQS		(64 * 1024)

/* The size and alignment of the command circular buffer */
#define	ITS_CMDQ_SIZE		(64 * 1024)	/* Must be a multiple of 4K */
#define	ITS_CMDQ_ALIGN		(64 * 1024)

#define	LPI_CONFTAB_SIZE	LPI_NIRQS
#define	LPI_CONFTAB_ALIGN	(64 * 1024)
#define	LPI_CONFTAB_MAX_ADDR	((1ul << 48) - 1) /* We need a 47 bit PA */

/* 1 bit per SPI, PPI, and SGI (8k), and 1 bit per LPI (LPI_CONFTAB_SIZE) */
#define	LPI_PENDTAB_SIZE	((LPI_NIRQS + GIC_FIRST_LPI) / 8)
#define	LPI_PENDTAB_ALIGN	(64 * 1024)
#define	LPI_PENDTAB_MAX_ADDR	((1ul << 48) - 1) /* We need a 47 bit PA */

#define	LPI_INT_TRANS_TAB_ALIGN	256
#define	LPI_INT_TRANS_TAB_MAX_ADDR ((1ul << 48) - 1)

/* ITS commands encoding */
#define	ITS_CMD_MOVI		(0x01)
#define	ITS_CMD_SYNC		(0x05)
#define	ITS_CMD_MAPD		(0x08)
#define	ITS_CMD_MAPC		(0x09)
#define	ITS_CMD_MAPTI		(0x0a)
#define	ITS_CMD_MAPI		(0x0b)
#define	ITS_CMD_INV		(0x0c)
#define	ITS_CMD_INVALL		(0x0d)
/* Command */
#define	CMD_COMMAND_MASK	(0xFFUL)
/* PCI device ID */
#define	CMD_DEVID_SHIFT		(32)
#define	CMD_DEVID_MASK		(0xFFFFFFFFUL << CMD_DEVID_SHIFT)
/* Size of IRQ ID bitfield */
#define	CMD_SIZE_MASK		(0xFFUL)
/* Virtual LPI ID */
#define	CMD_ID_MASK		(0xFFFFFFFFUL)
/* Physical LPI ID */
#define	CMD_PID_SHIFT		(32)
#define	CMD_PID_MASK		(0xFFFFFFFFUL << CMD_PID_SHIFT)
/* Collection */
#define	CMD_COL_MASK		(0xFFFFUL)
/* Target (CPU or Re-Distributor) */
#define	CMD_TARGET_SHIFT	(16)
#define	CMD_TARGET_MASK		(0xFFFFFFFFUL << CMD_TARGET_SHIFT)
/* Interrupt Translation Table address */
#define	CMD_ITT_MASK		(0xFFFFFFFFFF00UL)
/* Valid command bit */
#define	CMD_VALID_SHIFT		(63)
#define	CMD_VALID_MASK		(1UL << CMD_VALID_SHIFT)

#define	ITS_TARGET_NONE		0xFBADBEEF

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
	void			*itt;
};

/*
 * ITS command descriptor.
 * Idea for command description passing taken from Linux.
 */
struct its_cmd_desc {
	uint8_t cmd_type;

	union {
		struct {
			struct its_dev *its_dev;
			struct its_col *col;
			uint32_t id;
		} cmd_desc_movi;

		struct {
			struct its_col *col;
		} cmd_desc_sync;

		struct {
			struct its_col *col;
			uint8_t valid;
		} cmd_desc_mapc;

		struct {
			struct its_dev *its_dev;
			struct its_col *col;
			uint32_t pid;
			uint32_t id;
		} cmd_desc_mapvi;

		struct {
			struct its_dev *its_dev;
			struct its_col *col;
			uint32_t pid;
		} cmd_desc_mapi;

		struct {
			struct its_dev *its_dev;
			uint8_t valid;
		} cmd_desc_mapd;

		struct {
			struct its_dev *its_dev;
			struct its_col *col;
			uint32_t pid;
		} cmd_desc_inv;

		struct {
			struct its_col *col;
		} cmd_desc_invall;
	};
};

/* ITS command. Each command is 32 bytes long */
struct its_cmd {
	uint64_t	cmd_dword[4];	/* ITS command double word */
};

/* An ITS private table */
struct its_ptable {
	void		*ptab_vaddr;
	/* Size of the L1 and L2 tables */
	size_t		ptab_l1_size;
	size_t		ptab_l2_size;
	/* Number of L1 and L2 entries */
	int		ptab_l1_nidents;
	int		ptab_l2_nidents;

	int		ptab_page_size;
	int		ptab_share;
	bool		ptab_indirect;
};

/* ITS collection description. */
struct its_col {
	uint64_t	col_target;	/* Target Re-Distributor */
	uint64_t	col_id;		/* Collection ID */
};

struct gicv3_its_irqsrc {
	struct intr_irqsrc	gi_isrc;
	u_int			gi_id;
	u_int			gi_lpi;
	struct its_dev		*gi_its_dev;
	TAILQ_ENTRY(gicv3_its_irqsrc) gi_link;
};

struct gicv3_its_softc {
	device_t	dev;
	struct intr_pic *sc_pic;
	struct resource *sc_its_res;

	cpuset_t	sc_cpus;
	struct domainset *sc_ds;
	u_int		gic_irq_cpu;
	int		sc_devbits;
	int		sc_dev_table_idx;

	struct its_ptable sc_its_ptab[GITS_BASER_NUM];
	struct its_col *sc_its_cols[MAXCPU];	/* Per-CPU collections */

	/*
	 * TODO: We should get these from the parent as we only want a
	 * single copy of each across the interrupt controller.
	 */
	uint8_t		*sc_conf_base;
	void		*sc_pend_base[MAXCPU];

	/* Command handling */
	struct mtx sc_its_cmd_lock;
	struct its_cmd *sc_its_cmd_base; /* Command circular buffer address */
	size_t sc_its_cmd_next_idx;

	vmem_t *sc_irq_alloc;
	struct gicv3_its_irqsrc	**sc_irqs;
	u_int	sc_irq_base;
	u_int	sc_irq_length;
	u_int	sc_irq_count;

	struct mtx sc_its_dev_lock;
	TAILQ_HEAD(its_dev_list, its_dev) sc_its_dev_list;
	TAILQ_HEAD(free_irqs, gicv3_its_irqsrc) sc_free_irqs;

#define	ITS_FLAGS_CMDQ_FLUSH		0x00000001
#define	ITS_FLAGS_LPI_CONF_FLUSH	0x00000002
#define	ITS_FLAGS_ERRATA_CAVIUM_22375	0x00000004
#define	ITS_FLAGS_LPI_PREALLOC		0x00000008
	u_int sc_its_flags;
	bool	trace_enable;
	vm_page_t ma; /* fake msi page */
};

typedef void (its_quirk_func_t)(device_t);
static its_quirk_func_t its_quirk_cavium_22375;

static const struct {
	const char *desc;
	uint32_t iidr;
	uint32_t iidr_mask;
	its_quirk_func_t *func;
} its_quirks[] = {
	{
		/* Cavium ThunderX Pass 1.x */
		.desc = "Cavium ThunderX errata: 22375, 24313",
		.iidr = GITS_IIDR_RAW(GITS_IIDR_IMPL_CAVIUM,
		    GITS_IIDR_PROD_THUNDER, GITS_IIDR_VAR_THUNDER_1, 0),
		.iidr_mask = ~GITS_IIDR_REVISION_MASK,
		.func = its_quirk_cavium_22375,
	},
};

#define	gic_its_read_4(sc, reg)			\
    bus_read_4((sc)->sc_its_res, (reg))
#define	gic_its_read_8(sc, reg)			\
    bus_read_8((sc)->sc_its_res, (reg))

#define	gic_its_write_4(sc, reg, val)		\
    bus_write_4((sc)->sc_its_res, (reg), (val))
#define	gic_its_write_8(sc, reg, val)		\
    bus_write_8((sc)->sc_its_res, (reg), (val))

static device_attach_t gicv3_its_attach;
static device_detach_t gicv3_its_detach;

static pic_disable_intr_t gicv3_its_disable_intr;
static pic_enable_intr_t gicv3_its_enable_intr;
static pic_map_intr_t gicv3_its_map_intr;
static pic_setup_intr_t gicv3_its_setup_intr;
static pic_post_filter_t gicv3_its_post_filter;
static pic_post_ithread_t gicv3_its_post_ithread;
static pic_pre_ithread_t gicv3_its_pre_ithread;
static pic_bind_intr_t gicv3_its_bind_intr;
#ifdef SMP
static pic_init_secondary_t gicv3_its_init_secondary;
#endif
static msi_alloc_msi_t gicv3_its_alloc_msi;
static msi_release_msi_t gicv3_its_release_msi;
static msi_alloc_msix_t gicv3_its_alloc_msix;
static msi_release_msix_t gicv3_its_release_msix;
static msi_map_msi_t gicv3_its_map_msi;
#ifdef IOMMU
static msi_iommu_init_t gicv3_iommu_init;
static msi_iommu_deinit_t gicv3_iommu_deinit;
#endif

static void its_cmd_movi(device_t, struct gicv3_its_irqsrc *);
static void its_cmd_mapc(device_t, struct its_col *, uint8_t);
static void its_cmd_mapti(device_t, struct gicv3_its_irqsrc *);
static void its_cmd_mapd(device_t, struct its_dev *, uint8_t);
static void its_cmd_inv(device_t, struct its_dev *, struct gicv3_its_irqsrc *);
static void its_cmd_invall(device_t, struct its_col *);

static device_method_t gicv3_its_methods[] = {
	/* Device interface */
	DEVMETHOD(device_detach,	gicv3_its_detach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	gicv3_its_disable_intr),
	DEVMETHOD(pic_enable_intr,	gicv3_its_enable_intr),
	DEVMETHOD(pic_map_intr,		gicv3_its_map_intr),
	DEVMETHOD(pic_setup_intr,	gicv3_its_setup_intr),
	DEVMETHOD(pic_post_filter,	gicv3_its_post_filter),
	DEVMETHOD(pic_post_ithread,	gicv3_its_post_ithread),
	DEVMETHOD(pic_pre_ithread,	gicv3_its_pre_ithread),
#ifdef SMP
	DEVMETHOD(pic_bind_intr,	gicv3_its_bind_intr),
	DEVMETHOD(pic_init_secondary,	gicv3_its_init_secondary),
#endif

	/* MSI/MSI-X */
	DEVMETHOD(msi_alloc_msi,	gicv3_its_alloc_msi),
	DEVMETHOD(msi_release_msi,	gicv3_its_release_msi),
	DEVMETHOD(msi_alloc_msix,	gicv3_its_alloc_msix),
	DEVMETHOD(msi_release_msix,	gicv3_its_release_msix),
	DEVMETHOD(msi_map_msi,		gicv3_its_map_msi),
#ifdef IOMMU
	DEVMETHOD(msi_iommu_init,	gicv3_iommu_init),
	DEVMETHOD(msi_iommu_deinit,	gicv3_iommu_deinit),
#endif

	/* End */
	DEVMETHOD_END
};

static DEFINE_CLASS_0(gic, gicv3_its_driver, gicv3_its_methods,
    sizeof(struct gicv3_its_softc));

static void
gicv3_its_cmdq_init(struct gicv3_its_softc *sc)
{
	vm_paddr_t cmd_paddr;
	uint64_t reg, tmp;

	/* Set up the command circular buffer */
	sc->sc_its_cmd_base = contigmalloc_domainset(ITS_CMDQ_SIZE, M_GICV3_ITS,
	    sc->sc_ds, M_WAITOK | M_ZERO, 0, (1ul << 48) - 1, ITS_CMDQ_ALIGN,
	    0);
	sc->sc_its_cmd_next_idx = 0;

	cmd_paddr = vtophys(sc->sc_its_cmd_base);

	/* Set the base of the command buffer */
	reg = GITS_CBASER_VALID |
	    (GITS_CBASER_CACHE_NIWAWB << GITS_CBASER_CACHE_SHIFT) |
	    cmd_paddr | (GITS_CBASER_SHARE_IS << GITS_CBASER_SHARE_SHIFT) |
	    (ITS_CMDQ_SIZE / 4096 - 1);
	gic_its_write_8(sc, GITS_CBASER, reg);

	/* Read back to check for fixed value fields */
	tmp = gic_its_read_8(sc, GITS_CBASER);

	if ((tmp & GITS_CBASER_SHARE_MASK) !=
	    (GITS_CBASER_SHARE_IS << GITS_CBASER_SHARE_SHIFT)) {
		/* Check if the hardware reported non-shareable */
		if ((tmp & GITS_CBASER_SHARE_MASK) ==
		    (GITS_CBASER_SHARE_NS << GITS_CBASER_SHARE_SHIFT)) {
			/* If so remove the cache attribute */
			reg &= ~GITS_CBASER_CACHE_MASK;
			reg &= ~GITS_CBASER_SHARE_MASK;
			/* Set to Non-cacheable, Non-shareable */
			reg |= GITS_CBASER_CACHE_NIN << GITS_CBASER_CACHE_SHIFT;
			reg |= GITS_CBASER_SHARE_NS << GITS_CBASER_SHARE_SHIFT;

			gic_its_write_8(sc, GITS_CBASER, reg);
		}

		/* The command queue has to be flushed after each command */
		sc->sc_its_flags |= ITS_FLAGS_CMDQ_FLUSH;
	}

	/* Get the next command from the start of the buffer */
	gic_its_write_8(sc, GITS_CWRITER, 0x0);
}

static int
gicv3_its_table_page_size(struct gicv3_its_softc *sc, int table)
{
	uint64_t reg, tmp;
	int page_size;

	page_size = PAGE_SIZE_64K;
	reg = gic_its_read_8(sc, GITS_BASER(table));

	while (1) {
		reg &= GITS_BASER_PSZ_MASK;
		switch (page_size) {
		case PAGE_SIZE_4K:	/* 4KB */
			reg |= GITS_BASER_PSZ_4K << GITS_BASER_PSZ_SHIFT;
			break;
		case PAGE_SIZE_16K:	/* 16KB */
			reg |= GITS_BASER_PSZ_16K << GITS_BASER_PSZ_SHIFT;
			break;
		case PAGE_SIZE_64K:	/* 64KB */
			reg |= GITS_BASER_PSZ_64K << GITS_BASER_PSZ_SHIFT;
			break;
		}

		/* Write the new page size */
		gic_its_write_8(sc, GITS_BASER(table), reg);

		/* Read back to check */
		tmp = gic_its_read_8(sc, GITS_BASER(table));

		/* The page size is correct */
		if ((tmp & GITS_BASER_PSZ_MASK) == (reg & GITS_BASER_PSZ_MASK))
			return (page_size);

		switch (page_size) {
		default:
			return (-1);
		case PAGE_SIZE_16K:
			page_size = PAGE_SIZE_4K;
			break;
		case PAGE_SIZE_64K:
			page_size = PAGE_SIZE_16K;
			break;
		}
	}
}

static bool
gicv3_its_table_supports_indirect(struct gicv3_its_softc *sc, int table)
{
	uint64_t reg;

	reg = gic_its_read_8(sc, GITS_BASER(table));

	/* Try setting the indirect flag */
	reg |= GITS_BASER_INDIRECT;
	gic_its_write_8(sc, GITS_BASER(table), reg);

	/* Read back to check */
	reg = gic_its_read_8(sc, GITS_BASER(table));
	return ((reg & GITS_BASER_INDIRECT) != 0);
}


static int
gicv3_its_table_init(device_t dev, struct gicv3_its_softc *sc)
{
	void *table;
	vm_paddr_t paddr;
	uint64_t cache, reg, share, tmp, type;
	size_t its_tbl_size, nitspages, npages;
	size_t l1_esize, l2_esize, l1_nidents, l2_nidents;
	int i, page_size;
	int devbits;
	bool indirect;

	if ((sc->sc_its_flags & ITS_FLAGS_ERRATA_CAVIUM_22375) != 0) {
		/*
		 * GITS_TYPER[17:13] of ThunderX reports that device IDs
		 * are to be 21 bits in length. The entry size of the ITS
		 * table can be read from GITS_BASERn[52:48] and on ThunderX
		 * is supposed to be 8 bytes in length (for device table).
		 * Finally the page size that is to be used by ITS to access
		 * this table will be set to 64KB.
		 *
		 * This gives 0x200000 entries of size 0x8 bytes covered by
		 * 256 pages each of which 64KB in size. The number of pages
		 * (minus 1) should then be written to GITS_BASERn[7:0]. In
		 * that case this value would be 0xFF but on ThunderX the
		 * maximum value that HW accepts is 0xFD.
		 *
		 * Set an arbitrary number of device ID bits to 20 in order
		 * to limit the number of entries in ITS device table to
		 * 0x100000 and the table size to 8MB.
		 */
		devbits = 20;
		cache = 0;
	} else {
		devbits = GITS_TYPER_DEVB(gic_its_read_8(sc, GITS_TYPER));
		cache = GITS_BASER_CACHE_WAWB;
	}
	sc->sc_devbits = devbits;
	share = GITS_BASER_SHARE_IS;

	for (i = 0; i < GITS_BASER_NUM; i++) {
		reg = gic_its_read_8(sc, GITS_BASER(i));
		/* The type of table */
		type = GITS_BASER_TYPE(reg);
		if (type == GITS_BASER_TYPE_UNIMPL)
			continue;

		/* The table entry size */
		l1_esize = GITS_BASER_ESIZE(reg);

		/* Find the tables page size */
		page_size = gicv3_its_table_page_size(sc, i);
		if (page_size == -1) {
			device_printf(dev, "No valid page size for table %d\n",
			    i);
			return (EINVAL);
		}

		indirect = false;
		l2_nidents = 0;
		l2_esize = 0;
		switch(type) {
		case GITS_BASER_TYPE_DEV:
			if (sc->sc_dev_table_idx != -1)
				device_printf(dev,
				    "Warning: Multiple device tables found\n");

			sc->sc_dev_table_idx = i;
			l1_nidents = (1 << devbits);
			if ((l1_esize * l1_nidents) > (page_size * 2)) {
				indirect =
				    gicv3_its_table_supports_indirect(sc, i);
				if (indirect) {
					/*
					 * Each l1 entry is 8 bytes and points
					 * to an l2 table of size page_size.
					 * Calculate how many entries this is
					 * and use this to find how many
					 * 8 byte l1 idents we need.
					 */
					l2_esize = l1_esize;
					l2_nidents = page_size / l2_esize;
					l1_nidents = l1_nidents / l2_nidents;
					l1_esize = GITS_INDIRECT_L1_ESIZE;
				}
			}
			its_tbl_size = l1_esize * l1_nidents;
			its_tbl_size = roundup2(its_tbl_size, page_size);
			break;
		case GITS_BASER_TYPE_PP: /* Undocumented? */
		case GITS_BASER_TYPE_IC:
			its_tbl_size = page_size;
			break;
		case GITS_BASER_TYPE_VP:
			/*
			 * If GITS_TYPER.SVPET != 0, the pending table is
			 * shared amongst the redistibutors and ther other
			 * ITSes. Requiring sharing across the ITSes when none
			 * of the redistributors have GICR_VPROPBASER.Valid==1
			 * isn't specified in the architecture, but that's how
			 * the GIC-700 behaves. We don't handle vPE tables at
			 * all yet, so just skip this base register.
			 */
		default:
			if (bootverbose)
				device_printf(dev, "Unhandled table type %lx\n",
				    type);
			continue;
		}
		npages = howmany(its_tbl_size, PAGE_SIZE);

		/* Allocate the table */
		table = contigmalloc_domainset(npages * PAGE_SIZE,
		    M_GICV3_ITS, sc->sc_ds, M_WAITOK | M_ZERO, 0,
		    (1ul << 48) - 1, PAGE_SIZE_64K, 0);

		sc->sc_its_ptab[i].ptab_vaddr = table;
		sc->sc_its_ptab[i].ptab_l1_size = its_tbl_size;
		sc->sc_its_ptab[i].ptab_l1_nidents = l1_nidents;
		sc->sc_its_ptab[i].ptab_l2_size = page_size;
		sc->sc_its_ptab[i].ptab_l2_nidents = l2_nidents;

		sc->sc_its_ptab[i].ptab_indirect = indirect;
		sc->sc_its_ptab[i].ptab_page_size = page_size;

		paddr = vtophys(table);

		while (1) {
			nitspages = howmany(its_tbl_size, page_size);

			/* Clear the fields we will be setting */
			reg &= ~(GITS_BASER_VALID | GITS_BASER_INDIRECT |
			    GITS_BASER_CACHE_MASK | GITS_BASER_TYPE_MASK |
			    GITS_BASER_PA_MASK |
			    GITS_BASER_SHARE_MASK | GITS_BASER_PSZ_MASK |
			    GITS_BASER_SIZE_MASK);
			/* Set the new values */
			reg |= GITS_BASER_VALID |
			    (indirect ? GITS_BASER_INDIRECT : 0) |
			    (cache << GITS_BASER_CACHE_SHIFT) |
			    (type << GITS_BASER_TYPE_SHIFT) |
			    paddr | (share << GITS_BASER_SHARE_SHIFT) |
			    (nitspages - 1);

			switch (page_size) {
			case PAGE_SIZE_4K:	/* 4KB */
				reg |=
				    GITS_BASER_PSZ_4K << GITS_BASER_PSZ_SHIFT;
				break;
			case PAGE_SIZE_16K:	/* 16KB */
				reg |=
				    GITS_BASER_PSZ_16K << GITS_BASER_PSZ_SHIFT;
				break;
			case PAGE_SIZE_64K:	/* 64KB */
				reg |=
				    GITS_BASER_PSZ_64K << GITS_BASER_PSZ_SHIFT;
				break;
			}

			gic_its_write_8(sc, GITS_BASER(i), reg);

			/* Read back to check */
			tmp = gic_its_read_8(sc, GITS_BASER(i));

			/* Do the shareability masks line up? */
			if ((tmp & GITS_BASER_SHARE_MASK) !=
			    (reg & GITS_BASER_SHARE_MASK)) {
				share = (tmp & GITS_BASER_SHARE_MASK) >>
				    GITS_BASER_SHARE_SHIFT;
				continue;
			}

			if (tmp != reg) {
				device_printf(dev, "GITS_BASER%d: "
				    "unable to be updated: %lx != %lx\n",
				    i, reg, tmp);
				return (ENXIO);
			}

			sc->sc_its_ptab[i].ptab_share = share;
			/* We should have made all needed changes */
			break;
		}
	}

	return (0);
}

static void
gicv3_its_conftable_init(struct gicv3_its_softc *sc)
{
	/* note: we assume the ITS children are serialized by the parent */
	static void *conf_table;
	int extra_flags = 0;
	device_t gicv3;
	uint32_t ctlr;
	vm_paddr_t conf_pa;
	vm_offset_t conf_va;

	/*
	 * The PROPBASER is a singleton in our parent. We only set it up the
	 * first time through. conf_table is effectively global to all the units
	 * and we rely on subr_bus to serialize probe/attach.
	 */
	if (conf_table != NULL) {
		sc->sc_conf_base = conf_table;
		return;
	}

	gicv3 = device_get_parent(sc->dev);
	ctlr = gic_r_read_4(gicv3, GICR_CTLR);
	if ((ctlr & GICR_CTLR_LPI_ENABLE) != 0) {
		conf_pa = gic_r_read_8(gicv3, GICR_PROPBASER);
		conf_pa &= GICR_PROPBASER_PA_MASK;
		/*
		 * If there was a pre-existing PROPBASER, then we need to honor
		 * it because implementation defined behavior in gicv3 makes it
		 * impossible to quiesce to change it out. We will only see a
		 * pre-existing one when we've been kexec'd from a Linux kernel,
		 * or from a LinuxBoot environment.
		 *
		 * Linux provides us with a MEMRESERVE table that we put into
		 * the excluded physmem area. If PROPBASER isn't in this tabke,
		 * the system cannot run due to random memory corruption,
		 * so we panic for this case.
		 */
		if (!physmem_excluded(conf_pa, LPI_CONFTAB_SIZE))
			panic("gicv3 PROPBASER needs to reuse %#lx, but not reserved",
			    conf_pa);
		conf_va = PHYS_TO_DMAP(conf_pa);
		if (!pmap_klookup(conf_va, NULL))
			panic("Cannot map prior LPI mapping into KVA");
		conf_table = (void *)conf_va;
		extra_flags = ITS_FLAGS_LPI_PREALLOC | ITS_FLAGS_LPI_CONF_FLUSH;
		if (bootverbose)
			device_printf(sc->dev,
			    "LPI enabled, conf table using pa %#lx va %lx\n",
			    conf_pa, conf_va);
	} else {
		/*
		 * Otherwise just allocate contiguous pages. We'll configure the
		 * PROPBASER register later in its_init_cpu_lpi().
		 */
		conf_table = contigmalloc(LPI_CONFTAB_SIZE,
		    M_GICV3_ITS, M_WAITOK, 0, LPI_CONFTAB_MAX_ADDR,
		    LPI_CONFTAB_ALIGN, 0);
	}
	sc->sc_conf_base = conf_table;
	sc->sc_its_flags |= extra_flags;

	/* Set the default configuration */
	memset(sc->sc_conf_base, GIC_PRIORITY_MAX | LPI_CONF_GROUP1,
	    LPI_CONFTAB_SIZE);

	/* Flush the table to memory */
	cpu_dcache_wb_range(sc->sc_conf_base, LPI_CONFTAB_SIZE);
}

static void
gicv3_its_pendtables_init(struct gicv3_its_softc *sc)
{

	if ((sc->sc_its_flags & ITS_FLAGS_LPI_PREALLOC) == 0) {
		for (int i = 0; i <= mp_maxid; i++) {
			if (CPU_ISSET(i, &sc->sc_cpus) == 0)
				continue;

			sc->sc_pend_base[i] = contigmalloc(
			    LPI_PENDTAB_SIZE, M_GICV3_ITS, M_WAITOK | M_ZERO,
			    0, LPI_PENDTAB_MAX_ADDR, LPI_PENDTAB_ALIGN, 0);

			/* Flush so the ITS can see the memory */
			cpu_dcache_wb_range(sc->sc_pend_base[i],
			    LPI_PENDTAB_SIZE);
		}
	}
}

static void
its_init_cpu_lpi(device_t dev, struct gicv3_its_softc *sc)
{
	device_t gicv3;
	uint64_t xbaser, tmp, size;
	uint32_t ctlr;
	u_int cpuid;

	gicv3 = device_get_parent(dev);
	cpuid = PCPU_GET(cpuid);

	/*
	 * Set the redistributor base. If we're reusing what we found on boot
	 * since the gic was already running, then don't touch it here. We also
	 * don't need to disable / enable LPI if we're not changing PROPBASER,
	 * so only do that if we're not prealloced.
	 */
	if ((sc->sc_its_flags & ITS_FLAGS_LPI_PREALLOC) == 0) {
		/* Disable LPIs */
		ctlr = gic_r_read_4(gicv3, GICR_CTLR);
		ctlr &= ~GICR_CTLR_LPI_ENABLE;
		gic_r_write_4(gicv3, GICR_CTLR, ctlr);

		/* Make sure changes are observable my the GIC */
		dsb(sy);

		size = (flsl(LPI_CONFTAB_SIZE | GIC_FIRST_LPI) - 1);

		xbaser = vtophys(sc->sc_conf_base) |
		    (GICR_PROPBASER_SHARE_IS << GICR_PROPBASER_SHARE_SHIFT) |
		    (GICR_PROPBASER_CACHE_NIWAWB << GICR_PROPBASER_CACHE_SHIFT) |
		    size;

		gic_r_write_8(gicv3, GICR_PROPBASER, xbaser);

		/* Check the cache attributes we set */
		tmp = gic_r_read_8(gicv3, GICR_PROPBASER);

		if ((tmp & GICR_PROPBASER_SHARE_MASK) !=
		    (xbaser & GICR_PROPBASER_SHARE_MASK)) {
			if ((tmp & GICR_PROPBASER_SHARE_MASK) ==
			    (GICR_PROPBASER_SHARE_NS << GICR_PROPBASER_SHARE_SHIFT)) {
				/* We need to mark as non-cacheable */
				xbaser &= ~(GICR_PROPBASER_SHARE_MASK |
				    GICR_PROPBASER_CACHE_MASK);
				/* Non-cacheable */
				xbaser |= GICR_PROPBASER_CACHE_NIN <<
				    GICR_PROPBASER_CACHE_SHIFT;
				/* Non-shareable */
				xbaser |= GICR_PROPBASER_SHARE_NS <<
				    GICR_PROPBASER_SHARE_SHIFT;
				gic_r_write_8(gicv3, GICR_PROPBASER, xbaser);
			}
			sc->sc_its_flags |= ITS_FLAGS_LPI_CONF_FLUSH;
		}

		/*
		 * Set the LPI pending table base
		 */
		xbaser = vtophys(sc->sc_pend_base[cpuid]) |
		    (GICR_PENDBASER_CACHE_NIWAWB << GICR_PENDBASER_CACHE_SHIFT) |
		    (GICR_PENDBASER_SHARE_IS << GICR_PENDBASER_SHARE_SHIFT);

		gic_r_write_8(gicv3, GICR_PENDBASER, xbaser);

		tmp = gic_r_read_8(gicv3, GICR_PENDBASER);

		if ((tmp & GICR_PENDBASER_SHARE_MASK) ==
		    (GICR_PENDBASER_SHARE_NS << GICR_PENDBASER_SHARE_SHIFT)) {
			/* Clear the cahce and shareability bits */
			xbaser &= ~(GICR_PENDBASER_CACHE_MASK |
			    GICR_PENDBASER_SHARE_MASK);
			/* Mark as non-shareable */
			xbaser |= GICR_PENDBASER_SHARE_NS << GICR_PENDBASER_SHARE_SHIFT;
			/* And non-cacheable */
			xbaser |= GICR_PENDBASER_CACHE_NIN <<
			    GICR_PENDBASER_CACHE_SHIFT;
		}

		/* Enable LPIs */
		ctlr = gic_r_read_4(gicv3, GICR_CTLR);
		ctlr |= GICR_CTLR_LPI_ENABLE;
		gic_r_write_4(gicv3, GICR_CTLR, ctlr);

		/* Make sure the GIC has seen everything */
		dsb(sy);
	} else {
		KASSERT(sc->sc_pend_base[cpuid] == NULL,
		    ("PREALLOC too soon cpuid %d", cpuid));
		tmp = gic_r_read_8(gicv3, GICR_PENDBASER);
		tmp &= GICR_PENDBASER_PA_MASK;
		if (!physmem_excluded(tmp, LPI_PENDTAB_SIZE))
			panic("gicv3 PENDBASER on cpu %d needs to reuse 0x%#lx, but not reserved\n",
			    cpuid, tmp);
		sc->sc_pend_base[cpuid] = (void *)PHYS_TO_DMAP(tmp);
	}


	if (bootverbose)
		device_printf(gicv3, "using %sPENDBASE of %#lx on cpu %d\n",
		    (sc->sc_its_flags & ITS_FLAGS_LPI_PREALLOC) ? "pre-existing " : "",
		    vtophys(sc->sc_pend_base[cpuid]), cpuid);
}

static int
its_init_cpu(device_t dev, struct gicv3_its_softc *sc)
{
	device_t gicv3;
	vm_paddr_t target;
	u_int cpuid;
	struct redist_pcpu *rpcpu;

	gicv3 = device_get_parent(dev);
	cpuid = PCPU_GET(cpuid);
	if (!CPU_ISSET(cpuid, &sc->sc_cpus))
		return (0);

	/* Check if the ITS is enabled on this CPU */
	if ((gic_r_read_8(gicv3, GICR_TYPER) & GICR_TYPER_PLPIS) == 0)
		return (ENXIO);

	rpcpu = gicv3_get_redist(dev);

	/* Do per-cpu LPI init once */
	if (!rpcpu->lpi_enabled) {
		its_init_cpu_lpi(dev, sc);
		rpcpu->lpi_enabled = true;
	}

	if ((gic_its_read_8(sc, GITS_TYPER) & GITS_TYPER_PTA) != 0) {
		/* This ITS wants the redistributor physical address */
		target = vtophys((vm_offset_t)rman_get_virtual(rpcpu->res) +
		    rpcpu->offset);
	} else {
		/* This ITS wants the unique processor number */
		target = GICR_TYPER_CPUNUM(gic_r_read_8(gicv3, GICR_TYPER)) <<
		    CMD_TARGET_SHIFT;
	}

	sc->sc_its_cols[cpuid]->col_target = target;
	sc->sc_its_cols[cpuid]->col_id = cpuid;

	its_cmd_mapc(dev, sc->sc_its_cols[cpuid], 1);
	its_cmd_invall(dev, sc->sc_its_cols[cpuid]);

	return (0);
}

static int
gicv3_its_sysctl_trace_enable(SYSCTL_HANDLER_ARGS)
{
	struct gicv3_its_softc *sc;
	int rv;

	sc = arg1;

	rv = sysctl_handle_bool(oidp, &sc->trace_enable, 0, req);
	if (rv != 0 || req->newptr == NULL)
		return (rv);
	if (sc->trace_enable)
		gic_its_write_8(sc, GITS_TRKCTLR, 3);
	else
		gic_its_write_8(sc, GITS_TRKCTLR, 0);

	return (0);
}

static int
gicv3_its_sysctl_trace_regs(SYSCTL_HANDLER_ARGS)
{
	struct gicv3_its_softc *sc;
	struct sbuf *sb;
	int err;

	sc = arg1;
	sb = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (sb == NULL) {
		device_printf(sc->dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}
	sbuf_cat(sb, "\n");
	sbuf_printf(sb, "GITS_TRKCTLR: 0x%08X\n",
	    gic_its_read_4(sc, GITS_TRKCTLR));
	sbuf_printf(sb, "GITS_TRKR:    0x%08X\n",
	    gic_its_read_4(sc, GITS_TRKR));
	sbuf_printf(sb, "GITS_TRKDIDR: 0x%08X\n",
	    gic_its_read_4(sc, GITS_TRKDIDR));
	sbuf_printf(sb, "GITS_TRKPIDR: 0x%08X\n",
	    gic_its_read_4(sc, GITS_TRKPIDR));
	sbuf_printf(sb, "GITS_TRKVIDR: 0x%08X\n",
	    gic_its_read_4(sc, GITS_TRKVIDR));
	sbuf_printf(sb, "GITS_TRKTGTR: 0x%08X\n",
	   gic_its_read_4(sc, GITS_TRKTGTR));

	err = sbuf_finish(sb);
	if (err)
		device_printf(sc->dev, "Error finishing sbuf: %d\n", err);
	sbuf_delete(sb);
	return(err);
}

static int
gicv3_its_init_sysctl(struct gicv3_its_softc *sc)
{
	struct sysctl_oid *oid, *child;
	struct sysctl_ctx_list *ctx_list;

	ctx_list = device_get_sysctl_ctx(sc->dev);
	child = device_get_sysctl_tree(sc->dev);
	oid = SYSCTL_ADD_NODE(ctx_list,
	    SYSCTL_CHILDREN(child), OID_AUTO, "tracing",
	    CTLFLAG_RD| CTLFLAG_MPSAFE, NULL, "Messages tracing");
	if (oid == NULL)
		return (ENXIO);

	/* Add registers */
	SYSCTL_ADD_PROC(ctx_list,
	    SYSCTL_CHILDREN(oid), OID_AUTO, "enable",
	    CTLTYPE_U8 | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    gicv3_its_sysctl_trace_enable, "CU", "Enable tracing");
	SYSCTL_ADD_PROC(ctx_list,
	    SYSCTL_CHILDREN(oid), OID_AUTO, "capture",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    gicv3_its_sysctl_trace_regs, "", "Captured tracing registers.");

	return (0);
}

static int
gicv3_its_attach(device_t dev)
{
	struct gicv3_its_softc *sc;
	int domain, err, i, rid;
	uint64_t phys;
	uint32_t ctlr, iidr;

	sc = device_get_softc(dev);

	sc->sc_dev_table_idx = -1;
	sc->sc_irq_length = gicv3_get_nirqs(dev);
	sc->sc_irq_base = GIC_FIRST_LPI;
	sc->sc_irq_base += device_get_unit(dev) * sc->sc_irq_length;

	rid = 0;
	sc->sc_its_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_its_res == NULL) {
		device_printf(dev, "Could not allocate memory\n");
		return (ENXIO);
	}

	phys = rounddown2(vtophys(rman_get_virtual(sc->sc_its_res)) +
	    GITS_TRANSLATER, PAGE_SIZE);
	sc->ma = malloc(sizeof(struct vm_page), M_DEVBUF, M_WAITOK | M_ZERO);
	vm_page_initfake(sc->ma, phys, VM_MEMATTR_DEFAULT);

	CPU_COPY(&all_cpus, &sc->sc_cpus);
	iidr = gic_its_read_4(sc, GITS_IIDR);
	for (i = 0; i < nitems(its_quirks); i++) {
		if ((iidr & its_quirks[i].iidr_mask) == its_quirks[i].iidr) {
			if (bootverbose) {
				device_printf(dev, "Applying %s\n",
				    its_quirks[i].desc);
			}
			its_quirks[i].func(dev);
			break;
		}
	}

	if (bus_get_domain(dev, &domain) == 0 && domain < MAXMEMDOM) {
		sc->sc_ds = DOMAINSET_PREF(domain);
	} else {
		sc->sc_ds = DOMAINSET_RR();
	}

	/*
	 * GIT_CTLR_EN is mandated to reset to 0 on a Warm reset, but we may be
	 * coming in via, for instance, a kexec/kboot style setup where a
	 * previous kernel has configured then relinquished control.  Clear it
	 * so that we can reconfigure GITS_BASER*.
	 */
	ctlr = gic_its_read_4(sc, GITS_CTLR);
	if ((ctlr & GITS_CTLR_EN) != 0) {
		ctlr &= ~GITS_CTLR_EN;
		gic_its_write_4(sc, GITS_CTLR, ctlr);
	}

	/* Allocate the private tables */
	err = gicv3_its_table_init(dev, sc);
	if (err != 0)
		return (err);

	/* Protects access to the device list */
	mtx_init(&sc->sc_its_dev_lock, "ITS device lock", NULL, MTX_SPIN);

	/* Protects access to the ITS command circular buffer. */
	mtx_init(&sc->sc_its_cmd_lock, "ITS cmd lock", NULL, MTX_SPIN);

	/* Allocate the command circular buffer */
	gicv3_its_cmdq_init(sc);

	/* Allocate the per-CPU collections */
	for (int cpu = 0; cpu <= mp_maxid; cpu++)
		if (CPU_ISSET(cpu, &sc->sc_cpus) != 0)
			sc->sc_its_cols[cpu] = malloc_domainset(
			    sizeof(*sc->sc_its_cols[0]), M_GICV3_ITS,
			    DOMAINSET_PREF(pcpu_find(cpu)->pc_domain),
			    M_WAITOK | M_ZERO);
		else
			sc->sc_its_cols[cpu] = NULL;

	/* Enable the ITS */
	gic_its_write_4(sc, GITS_CTLR, ctlr | GITS_CTLR_EN);

	/* Create the LPI configuration table */
	gicv3_its_conftable_init(sc);

	/* And the pending tebles */
	gicv3_its_pendtables_init(sc);

	/* Enable LPIs on this CPU */
	its_init_cpu(dev, sc);

	TAILQ_INIT(&sc->sc_its_dev_list);
	TAILQ_INIT(&sc->sc_free_irqs);

	/*
	 * Create the vmem object to allocate INTRNG IRQs from. We try to
	 * use all IRQs not already used by the GICv3.
	 * XXX: This assumes there are no other interrupt controllers in the
	 * system.
	 */
	sc->sc_irq_alloc = vmem_create(device_get_nameunit(dev), 0,
	    gicv3_get_nirqs(dev), 1, 0, M_FIRSTFIT | M_WAITOK);

	sc->sc_irqs = malloc(sizeof(*sc->sc_irqs) * sc->sc_irq_length,
	    M_GICV3_ITS, M_WAITOK | M_ZERO);

	/* For GIC-500 install tracking sysctls. */
	if ((iidr & (GITS_IIDR_PRODUCT_MASK | GITS_IIDR_IMPLEMENTOR_MASK)) ==
	    GITS_IIDR_RAW(GITS_IIDR_IMPL_ARM, GITS_IIDR_PROD_GIC500, 0, 0))
		gicv3_its_init_sysctl(sc);

	return (0);
}

static int
gicv3_its_detach(device_t dev)
{

	return (ENXIO);
}

static void
its_quirk_cavium_22375(device_t dev)
{
	struct gicv3_its_softc *sc;
	int domain;

	sc = device_get_softc(dev);
	sc->sc_its_flags |= ITS_FLAGS_ERRATA_CAVIUM_22375;

	/*
	 * We need to limit which CPUs we send these interrupts to on
	 * the original dual socket ThunderX as it is unable to
	 * forward them between the two sockets.
	 */
	if (bus_get_domain(dev, &domain) == 0) {
		if (domain < MAXMEMDOM) {
			CPU_COPY(&cpuset_domain[domain], &sc->sc_cpus);
		} else {
			CPU_ZERO(&sc->sc_cpus);
		}
	}
}

static void
gicv3_its_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv3_its_softc *sc;
	struct gicv3_its_irqsrc *girq;
	uint8_t *conf;

	sc = device_get_softc(dev);
	girq = (struct gicv3_its_irqsrc *)isrc;
	conf = sc->sc_conf_base;

	conf[girq->gi_lpi] &= ~LPI_CONF_ENABLE;

	if ((sc->sc_its_flags & ITS_FLAGS_LPI_CONF_FLUSH) != 0) {
		/* Clean D-cache under command. */
		cpu_dcache_wb_range(&conf[girq->gi_lpi], 1);
	} else {
		/* DSB inner shareable, store */
		dsb(ishst);
	}

	its_cmd_inv(dev, girq->gi_its_dev, girq);
}

static void
gicv3_its_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv3_its_softc *sc;
	struct gicv3_its_irqsrc *girq;
	uint8_t *conf;

	sc = device_get_softc(dev);
	girq = (struct gicv3_its_irqsrc *)isrc;
	conf = sc->sc_conf_base;

	conf[girq->gi_lpi] |= LPI_CONF_ENABLE;

	if ((sc->sc_its_flags & ITS_FLAGS_LPI_CONF_FLUSH) != 0) {
		/* Clean D-cache under command. */
		cpu_dcache_wb_range(&conf[girq->gi_lpi], 1);
	} else {
		/* DSB inner shareable, store */
		dsb(ishst);
	}

	its_cmd_inv(dev, girq->gi_its_dev, girq);
}

static int
gicv3_its_intr(void *arg, uintptr_t irq)
{
	struct gicv3_its_softc *sc = arg;
	struct gicv3_its_irqsrc *girq;
	struct trapframe *tf;

	irq -= sc->sc_irq_base;
	girq = sc->sc_irqs[irq];
	if (girq == NULL)
		panic("gicv3_its_intr: Invalid interrupt %ld",
		    irq + sc->sc_irq_base);

	tf = curthread->td_intr_frame;
	intr_isrc_dispatch(&girq->gi_isrc, tf);
	return (FILTER_HANDLED);
}

static void
gicv3_its_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv3_its_irqsrc *girq;

	girq = (struct gicv3_its_irqsrc *)isrc;
	gic_icc_write(EOIR1, girq->gi_lpi + GIC_FIRST_LPI);
}

static void
gicv3_its_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{

}

static void
gicv3_its_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv3_its_irqsrc *girq;

	girq = (struct gicv3_its_irqsrc *)isrc;
	gic_icc_write(EOIR1, girq->gi_lpi + GIC_FIRST_LPI);
}

static int
gicv3_its_select_cpu(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv3_its_softc *sc;

	sc = device_get_softc(dev);
	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		sc->gic_irq_cpu = intr_irq_next_cpu(sc->gic_irq_cpu,
		    &sc->sc_cpus);
		CPU_SETOF(sc->gic_irq_cpu, &isrc->isrc_cpu);
	}

	return (0);
}

static int
gicv3_its_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gicv3_its_irqsrc *girq;

	gicv3_its_select_cpu(dev, isrc);

	girq = (struct gicv3_its_irqsrc *)isrc;
	its_cmd_movi(dev, girq);
	return (0);
}

static int
gicv3_its_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{

	/*
	 * This should never happen, we only call this function to map
	 * interrupts found before the controller driver is ready.
	 */
	panic("gicv3_its_map_intr: Unable to map a MSI interrupt");
}

static int
gicv3_its_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{

	/* Bind the interrupt to a CPU */
	gicv3_its_bind_intr(dev, isrc);

	return (0);
}

#ifdef SMP
static void
gicv3_its_init_secondary(device_t dev, enum root_type root_type)
{
	struct gicv3_its_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * This is fatal as otherwise we may bind interrupts to this CPU.
	 * We need a way to tell the interrupt framework to only bind to a
	 * subset of given CPUs when it performs the shuffle.
	 */
	if (its_init_cpu(dev, sc) != 0)
		panic("gicv3_its_init_secondary: No usable ITS on CPU%d",
		    PCPU_GET(cpuid));
}
#endif

static uint32_t
its_get_devid(device_t pci_dev)
{
	uintptr_t id;

	if (pci_get_id(pci_dev, PCI_ID_MSI, &id) != 0)
		panic("%s: %s: Unable to get the MSI DeviceID", __func__,
		    device_get_nameunit(pci_dev));

	return (id);
}

static struct its_dev *
its_device_find(device_t dev, device_t child)
{
	struct gicv3_its_softc *sc;
	struct its_dev *its_dev = NULL;

	sc = device_get_softc(dev);

	mtx_lock_spin(&sc->sc_its_dev_lock);
	TAILQ_FOREACH(its_dev, &sc->sc_its_dev_list, entry) {
		if (its_dev->pci_dev == child)
			break;
	}
	mtx_unlock_spin(&sc->sc_its_dev_lock);

	return (its_dev);
}

static bool
its_device_alloc(struct gicv3_its_softc *sc, int devid)
{
	struct its_ptable *ptable;
	void *l2_table;
	uint64_t *table;
	uint32_t index;
	bool shareable;

	/* No device table */
	if (sc->sc_dev_table_idx < 0) {
		if (devid >= (1 << sc->sc_devbits)) {
			if (bootverbose) {
				device_printf(sc->dev,
				    "%s: Device out of range for hardware "
				    "(%x >= %x)\n", __func__, devid,
				    1 << sc->sc_devbits);
			}
			return (false);
		}
		return (true);
	}

	ptable = &sc->sc_its_ptab[sc->sc_dev_table_idx];
	/* Check the devid is within the table limit */
	if (!ptable->ptab_indirect) {
		if (devid >= ptable->ptab_l1_nidents) {
			if (bootverbose) {
				device_printf(sc->dev,
				    "%s: Device out of range for table "
				    "(%x >= %x)\n", __func__, devid,
				    ptable->ptab_l1_nidents);
			}
			return (false);
		}

		return (true);
	}

	/* Check the devid is within the allocated range */
	index = devid / ptable->ptab_l2_nidents;
	if (index >= ptable->ptab_l1_nidents) {
		if (bootverbose) {
			device_printf(sc->dev,
			    "%s: Index out of range for table (%x >= %x)\n",
			    __func__, index, ptable->ptab_l1_nidents);
		}
		return (false);
	}

	table = (uint64_t *)ptable->ptab_vaddr;
	/* We have an second level table */
	if ((table[index] & GITS_BASER_VALID) != 0)
		return (true);

	shareable = true;
	if ((ptable->ptab_share & GITS_BASER_SHARE_MASK) == GITS_BASER_SHARE_NS)
		shareable = false;

	l2_table = contigmalloc_domainset(ptable->ptab_l2_size,
	    M_GICV3_ITS, sc->sc_ds, M_WAITOK | M_ZERO, 0, (1ul << 48) - 1,
	    ptable->ptab_page_size, 0);

	if (!shareable)
		cpu_dcache_wb_range(l2_table, ptable->ptab_l2_size);

	table[index] = vtophys(l2_table) | GITS_BASER_VALID;
	if (!shareable)
		cpu_dcache_wb_range(&table[index], sizeof(table[index]));

	dsb(sy);
	return (true);
}

static struct its_dev *
its_device_get(device_t dev, device_t child, u_int nvecs)
{
	struct gicv3_its_softc *sc;
	struct its_dev *its_dev;
	vmem_addr_t irq_base;
	size_t esize, itt_size;

	sc = device_get_softc(dev);

	its_dev = its_device_find(dev, child);
	if (its_dev != NULL)
		return (its_dev);

	its_dev = malloc(sizeof(*its_dev), M_GICV3_ITS, M_NOWAIT | M_ZERO);
	if (its_dev == NULL)
		return (NULL);

	its_dev->pci_dev = child;
	its_dev->devid = its_get_devid(child);

	its_dev->lpis.lpi_busy = 0;
	its_dev->lpis.lpi_num = nvecs;
	its_dev->lpis.lpi_free = nvecs;

	if (!its_device_alloc(sc, its_dev->devid)) {
		free(its_dev, M_GICV3_ITS);
		return (NULL);
	}

	if (vmem_alloc(sc->sc_irq_alloc, nvecs, M_FIRSTFIT | M_NOWAIT,
	    &irq_base) != 0) {
		free(its_dev, M_GICV3_ITS);
		return (NULL);
	}
	its_dev->lpis.lpi_base = irq_base;

	/* Get ITT entry size */
	esize = GITS_TYPER_ITTES(gic_its_read_8(sc, GITS_TYPER));

	/*
	 * Allocate ITT for this device.
	 * PA has to be 256 B aligned. At least two entries for device.
	 */
	itt_size = roundup2(MAX(nvecs, 2) * esize, 256);
	its_dev->itt = contigmalloc_domainset(itt_size,
	    M_GICV3_ITS, sc->sc_ds, M_NOWAIT | M_ZERO, 0,
	    LPI_INT_TRANS_TAB_MAX_ADDR, LPI_INT_TRANS_TAB_ALIGN, 0);
	if (its_dev->itt == NULL) {
		vmem_free(sc->sc_irq_alloc, its_dev->lpis.lpi_base, nvecs);
		free(its_dev, M_GICV3_ITS);
		return (NULL);
	}

	/* Make sure device sees zeroed ITT. */
	if ((sc->sc_its_flags & ITS_FLAGS_CMDQ_FLUSH) != 0)
		cpu_dcache_wb_range(its_dev->itt, itt_size);

	mtx_lock_spin(&sc->sc_its_dev_lock);
	TAILQ_INSERT_TAIL(&sc->sc_its_dev_list, its_dev, entry);
	mtx_unlock_spin(&sc->sc_its_dev_lock);

	/* Map device to its ITT */
	its_cmd_mapd(dev, its_dev, 1);

	return (its_dev);
}

static void
its_device_release(device_t dev, struct its_dev *its_dev)
{
	struct gicv3_its_softc *sc;

	KASSERT(its_dev->lpis.lpi_busy == 0,
	    ("its_device_release: Trying to release an inuse ITS device"));

	/* Unmap device in ITS */
	its_cmd_mapd(dev, its_dev, 0);

	sc = device_get_softc(dev);

	/* Remove the device from the list of devices */
	mtx_lock_spin(&sc->sc_its_dev_lock);
	TAILQ_REMOVE(&sc->sc_its_dev_list, its_dev, entry);
	mtx_unlock_spin(&sc->sc_its_dev_lock);

	/* Free ITT */
	KASSERT(its_dev->itt != NULL, ("Invalid ITT in valid ITS device"));
	free(its_dev->itt, M_GICV3_ITS);

	/* Free the IRQ allocation */
	vmem_free(sc->sc_irq_alloc, its_dev->lpis.lpi_base,
	    its_dev->lpis.lpi_num);

	free(its_dev, M_GICV3_ITS);
}

static struct gicv3_its_irqsrc *
gicv3_its_alloc_irqsrc(device_t dev, struct gicv3_its_softc *sc, u_int irq)
{
	struct gicv3_its_irqsrc *girq = NULL;

	KASSERT(sc->sc_irqs[irq] == NULL,
	    ("%s: Interrupt %u already allocated", __func__, irq));
	mtx_lock_spin(&sc->sc_its_dev_lock);
	if (!TAILQ_EMPTY(&sc->sc_free_irqs)) {
		girq = TAILQ_FIRST(&sc->sc_free_irqs);
		TAILQ_REMOVE(&sc->sc_free_irqs, girq, gi_link);
	}
	mtx_unlock_spin(&sc->sc_its_dev_lock);
	if (girq == NULL) {
		girq = malloc(sizeof(*girq), M_GICV3_ITS,
		    M_NOWAIT | M_ZERO);
		if (girq == NULL)
			return (NULL);
		girq->gi_id = -1;
		if (intr_isrc_register(&girq->gi_isrc, dev, 0,
		    "%s,%u", device_get_nameunit(dev), irq) != 0) {
			free(girq, M_GICV3_ITS);
			return (NULL);
		}
	}
	girq->gi_lpi = irq + sc->sc_irq_base - GIC_FIRST_LPI;
	sc->sc_irqs[irq] = girq;

	return (girq);
}

static void
gicv3_its_release_irqsrc(struct gicv3_its_softc *sc,
    struct gicv3_its_irqsrc *girq)
{
	u_int irq;

	mtx_assert(&sc->sc_its_dev_lock, MA_OWNED);

	irq = girq->gi_lpi + GIC_FIRST_LPI - sc->sc_irq_base;
	sc->sc_irqs[irq] = NULL;

	girq->gi_id = -1;
	girq->gi_its_dev = NULL;
	TAILQ_INSERT_TAIL(&sc->sc_free_irqs, girq, gi_link);
}

static int
gicv3_its_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **srcs)
{
	struct gicv3_its_softc *sc;
	struct gicv3_its_irqsrc *girq;
	struct its_dev *its_dev;
	u_int irq;
	int i;

	its_dev = its_device_get(dev, child, count);
	if (its_dev == NULL)
		return (ENXIO);

	KASSERT(its_dev->lpis.lpi_free >= count,
	    ("gicv3_its_alloc_msi: No free LPIs"));
	sc = device_get_softc(dev);
	irq = its_dev->lpis.lpi_base + its_dev->lpis.lpi_num -
	    its_dev->lpis.lpi_free;

	/* Allocate the irqsrc for each MSI */
	for (i = 0; i < count; i++, irq++) {
		its_dev->lpis.lpi_free--;
		srcs[i] = (struct intr_irqsrc *)gicv3_its_alloc_irqsrc(dev,
		    sc, irq);
		if (srcs[i] == NULL)
			break;
	}

	/* The allocation failed, release them */
	if (i != count) {
		mtx_lock_spin(&sc->sc_its_dev_lock);
		for (i = 0; i < count; i++) {
			girq = (struct gicv3_its_irqsrc *)srcs[i];
			if (girq == NULL)
				break;
			gicv3_its_release_irqsrc(sc, girq);
			srcs[i] = NULL;
		}
		mtx_unlock_spin(&sc->sc_its_dev_lock);
		return (ENXIO);
	}

	/* Finish the allocation now we have all MSI irqsrcs */
	for (i = 0; i < count; i++) {
		girq = (struct gicv3_its_irqsrc *)srcs[i];
		girq->gi_id = i;
		girq->gi_its_dev = its_dev;

		/* Map the message to the given IRQ */
		gicv3_its_select_cpu(dev, (struct intr_irqsrc *)girq);
		its_cmd_mapti(dev, girq);
	}
	its_dev->lpis.lpi_busy += count;
	*pic = dev;

	return (0);
}

static int
gicv3_its_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **isrc)
{
	struct gicv3_its_softc *sc;
	struct gicv3_its_irqsrc *girq;
	struct its_dev *its_dev;
	int i;

	its_dev = its_device_find(dev, child);

	KASSERT(its_dev != NULL,
	    ("gicv3_its_release_msi: Releasing a MSI interrupt with "
	     "no ITS device"));
	KASSERT(its_dev->lpis.lpi_busy >= count,
	    ("gicv3_its_release_msi: Releasing more interrupts than "
	     "were allocated: releasing %d, allocated %d", count,
	     its_dev->lpis.lpi_busy));

	sc = device_get_softc(dev);
	mtx_lock_spin(&sc->sc_its_dev_lock);
	for (i = 0; i < count; i++) {
		girq = (struct gicv3_its_irqsrc *)isrc[i];
		gicv3_its_release_irqsrc(sc, girq);
	}
	mtx_unlock_spin(&sc->sc_its_dev_lock);
	its_dev->lpis.lpi_busy -= count;

	if (its_dev->lpis.lpi_busy == 0)
		its_device_release(dev, its_dev);

	return (0);
}

static int
gicv3_its_alloc_msix(device_t dev, device_t child, device_t *pic,
    struct intr_irqsrc **isrcp)
{
	struct gicv3_its_softc *sc;
	struct gicv3_its_irqsrc *girq;
	struct its_dev *its_dev;
	u_int nvecs, irq;

	nvecs = pci_msix_count(child);
	its_dev = its_device_get(dev, child, nvecs);
	if (its_dev == NULL)
		return (ENXIO);

	KASSERT(its_dev->lpis.lpi_free > 0,
	    ("gicv3_its_alloc_msix: No free LPIs"));
	sc = device_get_softc(dev);
	irq = its_dev->lpis.lpi_base + its_dev->lpis.lpi_num -
	    its_dev->lpis.lpi_free;

	girq = gicv3_its_alloc_irqsrc(dev, sc, irq);
	if (girq == NULL)
		return (ENXIO);
	girq->gi_id = its_dev->lpis.lpi_busy;
	girq->gi_its_dev = its_dev;

	its_dev->lpis.lpi_free--;
	its_dev->lpis.lpi_busy++;

	/* Map the message to the given IRQ */
	gicv3_its_select_cpu(dev, (struct intr_irqsrc *)girq);
	its_cmd_mapti(dev, girq);

	*pic = dev;
	*isrcp = (struct intr_irqsrc *)girq;

	return (0);
}

static int
gicv3_its_release_msix(device_t dev, device_t child, struct intr_irqsrc *isrc)
{
	struct gicv3_its_softc *sc;
	struct gicv3_its_irqsrc *girq;
	struct its_dev *its_dev;

	its_dev = its_device_find(dev, child);

	KASSERT(its_dev != NULL,
	    ("gicv3_its_release_msix: Releasing a MSI-X interrupt with "
	     "no ITS device"));
	KASSERT(its_dev->lpis.lpi_busy > 0,
	    ("gicv3_its_release_msix: Releasing more interrupts than "
	     "were allocated: allocated %d", its_dev->lpis.lpi_busy));

	sc = device_get_softc(dev);
	girq = (struct gicv3_its_irqsrc *)isrc;
	mtx_lock_spin(&sc->sc_its_dev_lock);
	gicv3_its_release_irqsrc(sc, girq);
	mtx_unlock_spin(&sc->sc_its_dev_lock);
	its_dev->lpis.lpi_busy--;

	if (its_dev->lpis.lpi_busy == 0)
		its_device_release(dev, its_dev);

	return (0);
}

static int
gicv3_its_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct gicv3_its_softc *sc;
	struct gicv3_its_irqsrc *girq;

	sc = device_get_softc(dev);
	girq = (struct gicv3_its_irqsrc *)isrc;

	*addr = vtophys(rman_get_virtual(sc->sc_its_res)) + GITS_TRANSLATER;
	*data = girq->gi_id;

	return (0);
}

#ifdef IOMMU
static int
gicv3_iommu_init(device_t dev, device_t child, struct iommu_domain **domain)
{
	struct gicv3_its_softc *sc;
	struct iommu_ctx *ctx;
	int error;

	sc = device_get_softc(dev);
	ctx = iommu_get_dev_ctx(child);
	if (ctx == NULL)
		return (ENXIO);
	/* Map the page containing the GITS_TRANSLATER register. */
	error = iommu_map_msi(ctx, PAGE_SIZE, 0,
	    IOMMU_MAP_ENTRY_WRITE, IOMMU_MF_CANWAIT, &sc->ma);
	*domain = iommu_get_ctx_domain(ctx);

	return (error);
}

static void
gicv3_iommu_deinit(device_t dev, device_t child)
{
	struct iommu_ctx *ctx;

	ctx = iommu_get_dev_ctx(child);
	if (ctx == NULL)
		return;

	iommu_unmap_msi(ctx);
}
#endif

/*
 * Commands handling.
 */

static __inline void
cmd_format_command(struct its_cmd *cmd, uint8_t cmd_type)
{
	/* Command field: DW0 [7:0] */
	cmd->cmd_dword[0] &= htole64(~CMD_COMMAND_MASK);
	cmd->cmd_dword[0] |= htole64(cmd_type);
}

static __inline void
cmd_format_devid(struct its_cmd *cmd, uint32_t devid)
{
	/* Device ID field: DW0 [63:32] */
	cmd->cmd_dword[0] &= htole64(~CMD_DEVID_MASK);
	cmd->cmd_dword[0] |= htole64((uint64_t)devid << CMD_DEVID_SHIFT);
}

static __inline void
cmd_format_size(struct its_cmd *cmd, uint16_t size)
{
	/* Size field: DW1 [4:0] */
	cmd->cmd_dword[1] &= htole64(~CMD_SIZE_MASK);
	cmd->cmd_dword[1] |= htole64((size & CMD_SIZE_MASK));
}

static __inline void
cmd_format_id(struct its_cmd *cmd, uint32_t id)
{
	/* ID field: DW1 [31:0] */
	cmd->cmd_dword[1] &= htole64(~CMD_ID_MASK);
	cmd->cmd_dword[1] |= htole64(id);
}

static __inline void
cmd_format_pid(struct its_cmd *cmd, uint32_t pid)
{
	/* Physical ID field: DW1 [63:32] */
	cmd->cmd_dword[1] &= htole64(~CMD_PID_MASK);
	cmd->cmd_dword[1] |= htole64((uint64_t)pid << CMD_PID_SHIFT);
}

static __inline void
cmd_format_col(struct its_cmd *cmd, uint16_t col_id)
{
	/* Collection field: DW2 [16:0] */
	cmd->cmd_dword[2] &= htole64(~CMD_COL_MASK);
	cmd->cmd_dword[2] |= htole64(col_id);
}

static __inline void
cmd_format_target(struct its_cmd *cmd, uint64_t target)
{
	/* Target Address field: DW2 [47:16] */
	cmd->cmd_dword[2] &= htole64(~CMD_TARGET_MASK);
	cmd->cmd_dword[2] |= htole64(target & CMD_TARGET_MASK);
}

static __inline void
cmd_format_itt(struct its_cmd *cmd, uint64_t itt)
{
	/* ITT Address field: DW2 [47:8] */
	cmd->cmd_dword[2] &= htole64(~CMD_ITT_MASK);
	cmd->cmd_dword[2] |= htole64(itt & CMD_ITT_MASK);
}

static __inline void
cmd_format_valid(struct its_cmd *cmd, uint8_t valid)
{
	/* Valid field: DW2 [63] */
	cmd->cmd_dword[2] &= htole64(~CMD_VALID_MASK);
	cmd->cmd_dword[2] |= htole64((uint64_t)valid << CMD_VALID_SHIFT);
}

static inline bool
its_cmd_queue_full(struct gicv3_its_softc *sc)
{
	size_t read_idx, next_write_idx;

	/* Get the index of the next command */
	next_write_idx = (sc->sc_its_cmd_next_idx + 1) %
	    (ITS_CMDQ_SIZE / sizeof(struct its_cmd));
	/* And the index of the current command being read */
	read_idx = gic_its_read_4(sc, GITS_CREADR) / sizeof(struct its_cmd);

	/*
	 * The queue is full when the write offset points
	 * at the command before the current read offset.
	 */
	return (next_write_idx == read_idx);
}

static inline void
its_cmd_sync(struct gicv3_its_softc *sc, struct its_cmd *cmd)
{

	if ((sc->sc_its_flags & ITS_FLAGS_CMDQ_FLUSH) != 0) {
		/* Clean D-cache under command. */
		cpu_dcache_wb_range(cmd, sizeof(*cmd));
	} else {
		/* DSB inner shareable, store */
		dsb(ishst);
	}

}

static inline uint64_t
its_cmd_cwriter_offset(struct gicv3_its_softc *sc, struct its_cmd *cmd)
{
	uint64_t off;

	off = (cmd - sc->sc_its_cmd_base) * sizeof(*cmd);

	return (off);
}

static void
its_cmd_wait_completion(device_t dev, struct its_cmd *cmd_first,
    struct its_cmd *cmd_last)
{
	struct gicv3_its_softc *sc;
	uint64_t first, last, read;
	size_t us_left;

	sc = device_get_softc(dev);

	/*
	 * XXX ARM64TODO: This is obviously a significant delay.
	 * The reason for that is that currently the time frames for
	 * the command to complete are not known.
	 */
	us_left = 1000000;

	first = its_cmd_cwriter_offset(sc, cmd_first);
	last = its_cmd_cwriter_offset(sc, cmd_last);

	for (;;) {
		read = gic_its_read_8(sc, GITS_CREADR);
		if (first < last) {
			if (read < first || read >= last)
				break;
		} else if (read < first && read >= last)
			break;

		if (us_left-- == 0) {
			/* This means timeout */
			device_printf(dev,
			    "Timeout while waiting for CMD completion.\n");
			return;
		}
		DELAY(1);
	}
}

static struct its_cmd *
its_cmd_alloc_locked(device_t dev)
{
	struct gicv3_its_softc *sc;
	struct its_cmd *cmd;
	size_t us_left;

	sc = device_get_softc(dev);

	/*
	 * XXX ARM64TODO: This is obviously a significant delay.
	 * The reason for that is that currently the time frames for
	 * the command to complete (and therefore free the descriptor)
	 * are not known.
	 */
	us_left = 1000000;

	mtx_assert(&sc->sc_its_cmd_lock, MA_OWNED);
	while (its_cmd_queue_full(sc)) {
		if (us_left-- == 0) {
			/* Timeout while waiting for free command */
			device_printf(dev,
			    "Timeout while waiting for free command\n");
			return (NULL);
		}
		DELAY(1);
	}

	cmd = &sc->sc_its_cmd_base[sc->sc_its_cmd_next_idx];
	sc->sc_its_cmd_next_idx++;
	sc->sc_its_cmd_next_idx %= ITS_CMDQ_SIZE / sizeof(struct its_cmd);

	return (cmd);
}

static uint64_t
its_cmd_prepare(struct its_cmd *cmd, struct its_cmd_desc *desc)
{
	uint64_t target;
	uint8_t cmd_type;
	u_int size;

	cmd_type = desc->cmd_type;
	target = ITS_TARGET_NONE;

	switch (cmd_type) {
	case ITS_CMD_MOVI:	/* Move interrupt ID to another collection */
		target = desc->cmd_desc_movi.col->col_target;
		cmd_format_command(cmd, ITS_CMD_MOVI);
		cmd_format_id(cmd, desc->cmd_desc_movi.id);
		cmd_format_col(cmd, desc->cmd_desc_movi.col->col_id);
		cmd_format_devid(cmd, desc->cmd_desc_movi.its_dev->devid);
		break;
	case ITS_CMD_SYNC:	/* Wait for previous commands completion */
		target = desc->cmd_desc_sync.col->col_target;
		cmd_format_command(cmd, ITS_CMD_SYNC);
		cmd_format_target(cmd, target);
		break;
	case ITS_CMD_MAPD:	/* Assign ITT to device */
		cmd_format_command(cmd, ITS_CMD_MAPD);
		cmd_format_itt(cmd, vtophys(desc->cmd_desc_mapd.its_dev->itt));
		/*
		 * Size describes number of bits to encode interrupt IDs
		 * supported by the device minus one.
		 * When V (valid) bit is zero, this field should be written
		 * as zero.
		 */
		if (desc->cmd_desc_mapd.valid != 0) {
			size = fls(desc->cmd_desc_mapd.its_dev->lpis.lpi_num);
			size = MAX(1, size) - 1;
		} else
			size = 0;

		cmd_format_size(cmd, size);
		cmd_format_devid(cmd, desc->cmd_desc_mapd.its_dev->devid);
		cmd_format_valid(cmd, desc->cmd_desc_mapd.valid);
		break;
	case ITS_CMD_MAPC:	/* Map collection to Re-Distributor */
		target = desc->cmd_desc_mapc.col->col_target;
		cmd_format_command(cmd, ITS_CMD_MAPC);
		cmd_format_col(cmd, desc->cmd_desc_mapc.col->col_id);
		cmd_format_valid(cmd, desc->cmd_desc_mapc.valid);
		cmd_format_target(cmd, target);
		break;
	case ITS_CMD_MAPTI:
		target = desc->cmd_desc_mapvi.col->col_target;
		cmd_format_command(cmd, ITS_CMD_MAPTI);
		cmd_format_devid(cmd, desc->cmd_desc_mapvi.its_dev->devid);
		cmd_format_id(cmd, desc->cmd_desc_mapvi.id);
		cmd_format_pid(cmd, desc->cmd_desc_mapvi.pid);
		cmd_format_col(cmd, desc->cmd_desc_mapvi.col->col_id);
		break;
	case ITS_CMD_MAPI:
		target = desc->cmd_desc_mapi.col->col_target;
		cmd_format_command(cmd, ITS_CMD_MAPI);
		cmd_format_devid(cmd, desc->cmd_desc_mapi.its_dev->devid);
		cmd_format_id(cmd, desc->cmd_desc_mapi.pid);
		cmd_format_col(cmd, desc->cmd_desc_mapi.col->col_id);
		break;
	case ITS_CMD_INV:
		target = desc->cmd_desc_inv.col->col_target;
		cmd_format_command(cmd, ITS_CMD_INV);
		cmd_format_devid(cmd, desc->cmd_desc_inv.its_dev->devid);
		cmd_format_id(cmd, desc->cmd_desc_inv.pid);
		break;
	case ITS_CMD_INVALL:
		cmd_format_command(cmd, ITS_CMD_INVALL);
		cmd_format_col(cmd, desc->cmd_desc_invall.col->col_id);
		break;
	default:
		panic("its_cmd_prepare: Invalid command: %x", cmd_type);
	}

	return (target);
}

static int
its_cmd_send(device_t dev, struct its_cmd_desc *desc)
{
	struct gicv3_its_softc *sc;
	struct its_cmd *cmd, *cmd_sync, *cmd_write;
	struct its_col col_sync;
	struct its_cmd_desc desc_sync;
	uint64_t target, cwriter;

	sc = device_get_softc(dev);
	mtx_lock_spin(&sc->sc_its_cmd_lock);
	cmd = its_cmd_alloc_locked(dev);
	if (cmd == NULL) {
		device_printf(dev, "could not allocate ITS command\n");
		mtx_unlock_spin(&sc->sc_its_cmd_lock);
		return (EBUSY);
	}

	target = its_cmd_prepare(cmd, desc);
	its_cmd_sync(sc, cmd);

	if (target != ITS_TARGET_NONE) {
		cmd_sync = its_cmd_alloc_locked(dev);
		if (cmd_sync != NULL) {
			desc_sync.cmd_type = ITS_CMD_SYNC;
			col_sync.col_target = target;
			desc_sync.cmd_desc_sync.col = &col_sync;
			its_cmd_prepare(cmd_sync, &desc_sync);
			its_cmd_sync(sc, cmd_sync);
		}
	}

	/* Update GITS_CWRITER */
	cwriter = sc->sc_its_cmd_next_idx * sizeof(struct its_cmd);
	gic_its_write_8(sc, GITS_CWRITER, cwriter);
	cmd_write = &sc->sc_its_cmd_base[sc->sc_its_cmd_next_idx];
	mtx_unlock_spin(&sc->sc_its_cmd_lock);

	its_cmd_wait_completion(dev, cmd, cmd_write);

	return (0);
}

/* Handlers to send commands */
static void
its_cmd_movi(device_t dev, struct gicv3_its_irqsrc *girq)
{
	struct gicv3_its_softc *sc;
	struct its_cmd_desc desc;
	struct its_col *col;

	sc = device_get_softc(dev);
	col = sc->sc_its_cols[CPU_FFS(&girq->gi_isrc.isrc_cpu) - 1];

	desc.cmd_type = ITS_CMD_MOVI;
	desc.cmd_desc_movi.its_dev = girq->gi_its_dev;
	desc.cmd_desc_movi.col = col;
	desc.cmd_desc_movi.id = girq->gi_id;

	its_cmd_send(dev, &desc);
}

static void
its_cmd_mapc(device_t dev, struct its_col *col, uint8_t valid)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_MAPC;
	desc.cmd_desc_mapc.col = col;
	/*
	 * Valid bit set - map the collection.
	 * Valid bit cleared - unmap the collection.
	 */
	desc.cmd_desc_mapc.valid = valid;

	its_cmd_send(dev, &desc);
}

static void
its_cmd_mapti(device_t dev, struct gicv3_its_irqsrc *girq)
{
	struct gicv3_its_softc *sc;
	struct its_cmd_desc desc;
	struct its_col *col;
	u_int col_id;

	sc = device_get_softc(dev);

	col_id = CPU_FFS(&girq->gi_isrc.isrc_cpu) - 1;
	col = sc->sc_its_cols[col_id];

	desc.cmd_type = ITS_CMD_MAPTI;
	desc.cmd_desc_mapvi.its_dev = girq->gi_its_dev;
	desc.cmd_desc_mapvi.col = col;
	/* The EventID sent to the device */
	desc.cmd_desc_mapvi.id = girq->gi_id;
	/* The physical interrupt presented to softeware */
	desc.cmd_desc_mapvi.pid = girq->gi_lpi + GIC_FIRST_LPI;

	its_cmd_send(dev, &desc);
}

static void
its_cmd_mapd(device_t dev, struct its_dev *its_dev, uint8_t valid)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_MAPD;
	desc.cmd_desc_mapd.its_dev = its_dev;
	desc.cmd_desc_mapd.valid = valid;

	its_cmd_send(dev, &desc);
}

static void
its_cmd_inv(device_t dev, struct its_dev *its_dev,
    struct gicv3_its_irqsrc *girq)
{
	struct gicv3_its_softc *sc;
	struct its_cmd_desc desc;
	struct its_col *col;

	sc = device_get_softc(dev);
	col = sc->sc_its_cols[CPU_FFS(&girq->gi_isrc.isrc_cpu) - 1];

	desc.cmd_type = ITS_CMD_INV;
	/* The EventID sent to the device */
	desc.cmd_desc_inv.pid = girq->gi_id;
	desc.cmd_desc_inv.its_dev = its_dev;
	desc.cmd_desc_inv.col = col;

	its_cmd_send(dev, &desc);
}

static void
its_cmd_invall(device_t dev, struct its_col *col)
{
	struct its_cmd_desc desc;

	desc.cmd_type = ITS_CMD_INVALL;
	desc.cmd_desc_invall.col = col;

	its_cmd_send(dev, &desc);
}

#ifdef FDT
static device_probe_t gicv3_its_fdt_probe;
static device_attach_t gicv3_its_fdt_attach;

static device_method_t gicv3_its_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gicv3_its_fdt_probe),
	DEVMETHOD(device_attach,	gicv3_its_fdt_attach),

	/* End */
	DEVMETHOD_END
};

#define its_baseclasses its_fdt_baseclasses
DEFINE_CLASS_1(its, gicv3_its_fdt_driver, gicv3_its_fdt_methods,
    sizeof(struct gicv3_its_softc), gicv3_its_driver);
#undef its_baseclasses

EARLY_DRIVER_MODULE(its_fdt, gic, gicv3_its_fdt_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

static int
gicv3_its_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "arm,gic-v3-its"))
		return (ENXIO);

	if (!gicv3_get_support_lpis(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM GIC Interrupt Translation Service");
	return (BUS_PROBE_DEFAULT);
}

static int
gicv3_its_fdt_attach(device_t dev)
{
	struct gicv3_its_softc *sc;
	phandle_t xref;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	err = gicv3_its_attach(dev);
	if (err != 0)
		return (err);

	/* Register this device as a interrupt controller */
	xref = OF_xref_from_node(ofw_bus_get_node(dev));
	sc->sc_pic = intr_pic_register(dev, xref);
	err = intr_pic_add_handler(device_get_parent(dev), sc->sc_pic,
	    gicv3_its_intr, sc, sc->sc_irq_base, sc->sc_irq_length);
	if (err != 0) {
		device_printf(dev, "Failed to add PIC handler: %d\n", err);
		return (err);
	}

	/* Register this device to handle MSI interrupts */
	err = intr_msi_register(dev, xref);
	if (err != 0) {
		device_printf(dev, "Failed to register for MSIs: %d\n", err);
		return (err);
	}

	return (0);
}
#endif

#ifdef DEV_ACPI
static device_probe_t gicv3_its_acpi_probe;
static device_attach_t gicv3_its_acpi_attach;

static device_method_t gicv3_its_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gicv3_its_acpi_probe),
	DEVMETHOD(device_attach,	gicv3_its_acpi_attach),

	/* End */
	DEVMETHOD_END
};

#define its_baseclasses its_acpi_baseclasses
DEFINE_CLASS_1(its, gicv3_its_acpi_driver, gicv3_its_acpi_methods,
    sizeof(struct gicv3_its_softc), gicv3_its_driver);
#undef its_baseclasses

EARLY_DRIVER_MODULE(its_acpi, gic, gicv3_its_acpi_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

static int
gicv3_its_acpi_probe(device_t dev)
{

	if (gic_get_bus(dev) != GIC_BUS_ACPI)
		return (EINVAL);

	if (gic_get_hw_rev(dev) < 3)
		return (EINVAL);

	if (!gicv3_get_support_lpis(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM GIC Interrupt Translation Service");
	return (BUS_PROBE_DEFAULT);
}

static int
gicv3_its_acpi_attach(device_t dev)
{
	struct gicv3_its_softc *sc;
	struct gic_v3_devinfo *di;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	err = gicv3_its_attach(dev);
	if (err != 0)
		return (err);

	di = device_get_ivars(dev);
	sc->sc_pic = intr_pic_register(dev, di->msi_xref);
	err = intr_pic_add_handler(device_get_parent(dev), sc->sc_pic,
	    gicv3_its_intr, sc, sc->sc_irq_base, sc->sc_irq_length);
	if (err != 0) {
		device_printf(dev, "Failed to add PIC handler: %d\n", err);
		return (err);
	}

	/* Register this device to handle MSI interrupts */
	err = intr_msi_register(dev, di->msi_xref);
	if (err != 0) {
		device_printf(dev, "Failed to register for MSIs: %d\n", err);
		return (err);
	}

	return (0);
}
#endif
