/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _GIC_V3_VAR_H_
#define _GIC_V3_VAR_H_

#define	GIC_V3_DEVSTR	"ARM Generic Interrupt Controller v3.0"

DECLARE_CLASS(gic_v3_driver);

struct gic_v3_irqsrc {
	struct intr_irqsrc	gi_isrc;
	uint32_t		gi_irq;
	enum intr_polarity	gi_pol;
	enum intr_trigger	gi_trig;
};

struct redist_lpis {
	vm_offset_t		conf_base;
	vm_offset_t		pend_base[MAXCPU];
	uint64_t		flags;
};

struct gic_redists {
	/*
	 * Re-Distributor region description.
	 * We will have few of those depending
	 * on the #redistributor-regions property in FDT.
	 */
	struct resource **	regions;
	/* Number of Re-Distributor regions */
	u_int			nregions;
	/* Per-CPU Re-Distributor handler */
	struct resource *	pcpu[MAXCPU];
	/* LPIs data */
	struct redist_lpis	lpis;
};

struct gic_v3_softc {
	device_t		dev;
	struct resource **	gic_res;
	struct mtx		gic_mtx;
	/* Distributor */
	struct resource *	gic_dist;
	/* Re-Distributors */
	struct gic_redists	gic_redists;

	u_int			gic_nirqs;
	u_int			gic_idbits;

	boolean_t		gic_registered;

	int			gic_nchildren;
	device_t		*gic_children;
	struct intr_pic		*gic_pic;
	struct gic_v3_irqsrc	*gic_irqs;
};

#define GIC_INTR_ISRC(sc, irq)	(&sc->gic_irqs[irq].gi_isrc)

MALLOC_DECLARE(M_GIC_V3);

/* ivars */
enum {
	GICV3_IVAR_NIRQS,
	GICV3_IVAR_REDIST_VADDR,
};

__BUS_ACCESSOR(gicv3, nirqs, GICV3, NIRQS, u_int);
__BUS_ACCESSOR(gicv3, redist_vaddr, GICV3, REDIST_VADDR, void *);

/* Device methods */
int gic_v3_attach(device_t dev);
int gic_v3_detach(device_t dev);
int arm_gic_v3_intr(void *);

uint32_t gic_r_read_4(device_t, bus_size_t);
uint64_t gic_r_read_8(device_t, bus_size_t);
void gic_r_write_4(device_t, bus_size_t, uint32_t var);
void gic_r_write_8(device_t, bus_size_t, uint64_t var);

/*
 * GIC Distributor accessors.
 * Notice that only GIC sofc can be passed.
 */
#define	gic_d_read(sc, len, reg)		\
({						\
	bus_read_##len(sc->gic_dist, reg);	\
})

#define	gic_d_write(sc, len, reg, val)		\
({						\
	bus_write_##len(sc->gic_dist, reg, val);\
})

/* GIC Re-Distributor accessors (per-CPU) */
#define	gic_r_read(sc, len, reg)		\
({						\
	u_int cpu = PCPU_GET(cpuid);		\
						\
	bus_read_##len(				\
	    sc->gic_redists.pcpu[cpu],		\
	    reg);				\
})

#define	gic_r_write(sc, len, reg, val)		\
({						\
	u_int cpu = PCPU_GET(cpuid);		\
						\
	bus_write_##len(			\
	    sc->gic_redists.pcpu[cpu],		\
	    reg, val);				\
})

#define	PCI_DEVID_GENERIC(pci_dev)				\
({								\
	((pci_get_domain(pci_dev) << PCI_RID_DOMAIN_SHIFT) |	\
	(pci_get_bus(pci_dev) << PCI_RID_BUS_SHIFT) |		\
	(pci_get_slot(pci_dev) << PCI_RID_SLOT_SHIFT) |		\
	(pci_get_function(pci_dev) << PCI_RID_FUNC_SHIFT));	\
})

/*
 * Request number of maximum MSI-X vectors for this device.
 * Device can ask for less vectors than maximum supported but not more.
 */
#define	PCI_MSIX_NUM(pci_dev)			\
({						\
	struct pci_devinfo *dinfo;		\
	pcicfgregs *cfg;			\
						\
	dinfo = device_get_ivars(pci_dev);	\
	cfg = &dinfo->cfg;			\
						\
	cfg->msix.msix_msgnum;			\
})

#endif /* _GIC_V3_VAR_H_ */
