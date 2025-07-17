/*-
 * Copyright (c) 2015 The FreeBSD Foundation
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

#ifndef _GIC_V3_VAR_H_
#define _GIC_V3_VAR_H_

#include <arm/arm/gic_common.h>

#define	GIC_V3_DEVSTR	"ARM Generic Interrupt Controller v3.0"

DECLARE_CLASS(gic_v3_driver);

struct gic_v3_irqsrc;

struct redist_pcpu {
	struct resource		*res;		/* mem resource for redist */
	vm_offset_t		pend_base;
	bus_size_t		offset;
	bool			lpi_enabled;	/* redist LPI configured? */
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
	/*
	 * Whether to treat each region as a single Re-Distributor page or a
	 * series of contiguous pages (i.e. from each ACPI MADT GICC's GICR
	 * Base Address field)
	 */
	bool			single;
	/* Per-CPU Re-Distributor data */
	struct redist_pcpu	*pcpu;
};

struct gic_v3_softc {
	device_t		dev;
	struct resource **	gic_res;
	struct mtx		gic_mtx;
	/* Distributor */
	struct resource *	gic_dist;
	/* Re-Distributors */
	struct gic_redists	gic_redists;

	/* Message Based Interrupts */
	u_int			gic_mbi_start;
	u_int			gic_mbi_end;
	struct mtx		gic_mbi_mtx;

	uint32_t		gic_pidr2;
	u_int			gic_bus;

	u_int			gic_nirqs;
	u_int			gic_idbits;

	boolean_t		gic_registered;

	int			gic_nchildren;
	device_t		*gic_children;
	struct intr_pic		*gic_pic;
	struct gic_v3_irqsrc	*gic_irqs;

	int			nranges;
	struct arm_gic_range *	ranges;
};

struct gic_v3_devinfo {
	int gic_domain;
	int msi_xref;
	int is_vgic;
};

#define GIC_INTR_ISRC(sc, irq)	(&sc->gic_irqs[irq].gi_isrc)

MALLOC_DECLARE(M_GIC_V3);

/* ivars */
#define	GICV3_IVAR_NIRQS	1000
/* 1001 was GICV3_IVAR_REDIST_VADDR */
#define	GICV3_IVAR_REDIST	1002
#define	GICV3_IVAR_SUPPORT_LPIS	1003

__BUS_ACCESSOR(gicv3, nirqs, GICV3, NIRQS, u_int);
__BUS_ACCESSOR(gicv3, redist, GICV3, REDIST, void *);
__BUS_ACCESSOR(gicv3, support_lpis, GICV3, SUPPORT_LPIS, bool);

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
	    (sc)->gic_redists.pcpu[cpu].res,	\
	    (sc)->gic_redists.pcpu[cpu].offset + (reg)); \
})

#define	gic_r_write(sc, len, reg, val)		\
({						\
	u_int cpu = PCPU_GET(cpuid);		\
						\
	bus_write_##len(			\
	    (sc)->gic_redists.pcpu[cpu].res,	\
	    (sc)->gic_redists.pcpu[cpu].offset + (reg), \
	    (val));				\
})

#endif /* _GIC_V3_VAR_H_ */
