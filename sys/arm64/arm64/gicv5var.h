/*
 * Copyright (c) 2025 Arm Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ARM64_GICV5VAR_H_
#define	_ARM64_GICV5VAR_H_

DECLARE_CLASS(gicv5_driver);

struct gicv5_irqsrc;
struct gicv5_irs_irqspec;

enum gicv5_irq_space {
	GICv5_INVALID,
	/* GICv5 interrupt types so they are the same values as in the spec */
	GICv5_PPI,
	GICv5_LPI,
	GICv5_SPI,
};

/*
 * Shared between the IRS and ITS
 */
struct gicv5_base_irqsrc {
	struct intr_irqsrc	gbi_isrc;
	enum gicv5_irq_space	gbi_space;
	bool			gbi_ipi;
	uint32_t		gbi_irq;
};

struct gicv5_irs;

struct gicv5_softc {
	device_t		gic_dev;
	struct intr_pic		*gic_pic;

	struct gicv5_irqsrc	*gic_ppi_irqs;
	struct gicv5_irqsrc	*gic_irs_irqs;
	struct gicv5_irqsrc	*gic_ipi_irqs;
	struct gicv5_irs	**gic_irs;

	device_t		*gic_children;

	u_int			gic_bus;
	bool			gic_coherent;
	u_int			gic_nirs;
	u_int			gic_spi_count;
	u_int			gic_nchildren;
	/* Number of LPIs, including IPIs */
	u_int			gic_nlpis;
};

struct gicv5_devinfo {
	struct resource_list	di_rl;
	struct gicv5_irs	*di_irs;
};

#define	GICV5_IVAR_LPI_START	5000

__BUS_ACCESSOR(gicv5, lpi_start, GICV5, LPI_START, u_int);

void gicv5_attach(device_t);
bool gicv5_add_child(device_t, struct gicv5_devinfo *);
int gicv5_intr(void *);

void gicv5_irs_init(device_t, u_int, cpuset_t *);
void gicv5_irs_extend_ist(device_t, device_t, u_int);

#endif /* _ARM64_GICV5VAR_H_ */
