/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
 *
 * $FreeBSD$
 *
 */

#ifndef _PCI_DW_H_
#define	_PCI_DW_H_

#include "pci_dw_if.h"

/* DesignWare CIe configuration registers */
#define	DW_PORT_LINK_CTRL		0x710
#define	 PORT_LINK_CAPABLE(n)			(((n) & 0x3F) << 16)
#define	 PORT_LINK_CAPABLE_1			0x01
#define	 PORT_LINK_CAPABLE_2			0x03
#define	 PORT_LINK_CAPABLE_4			0x07
#define	 PORT_LINK_CAPABLE_8			0x0F
#define	 PORT_LINK_CAPABLE_16			0x1F
#define	 PORT_LINK_CAPABLE_32			0x3F

#define	DW_GEN2_CTRL			0x80C
#define	 DIRECT_SPEED_CHANGE			(1 << 17)
#define	 GEN2_CTRL_NUM_OF_LANES(n)		(((n) & 0x3F) << 8)
#define	 GEN2_CTRL_NUM_OF_LANES_1		0x01
#define	 GEN2_CTRL_NUM_OF_LANES_2		0x03
#define	 GEN2_CTRL_NUM_OF_LANES_4		0x07
#define	 GEN2_CTRL_NUM_OF_LANES_8		0x0F
#define	 GEN2_CTRL_NUM_OF_LANES_16		0x1F
#define	 GEN2_CTRL_NUM_OF_LANES_32		0x3F

#define DW_MSI_ADDR_LO			0x820
#define DW_MSI_ADDR_HI			0x824
#define DW_MSI_INTR0_ENABLE		0x828
#define DW_MSI_INTR0_MASK		0x82C
#define DW_MSI_INTR0_STATUS		0x830

#define	DW_MISC_CONTROL_1		0x8BC
#define	 DBI_RO_WR_EN				(1 << 0)

/* Legacy (pre-4.80) iATU mode */
#define	DW_IATU_VIEWPORT			0x900
#define	 IATU_REGION_INBOUND			(1U << 31)
#define	 IATU_REGION_INDEX(x)			((x) & 0x7)
#define	DW_IATU_CTRL1			0x904
#define	 IATU_CTRL1_TYPE(x)			((x) & 0x1F)
#define	 IATU_CTRL1_TYPE_MEM			0x0
#define	 IATU_CTRL1_TYPE_IO			0x2
#define	 IATU_CTRL1_TYPE_CFG0			0x4
#define	 IATU_CTRL1_TYPE_CFG1			0x5
#define	DW_IATU_CTRL2			0x908
#define	 IATU_CTRL2_REGION_EN			(1U << 31)
#define	DW_IATU_LWR_BASE_ADDR		0x90C
#define	DW_IATU_UPPER_BASE_ADDR		0x910
#define	DW_IATU_LIMIT_ADDR		0x914
#define	DW_IATU_LWR_TARGET_ADDR		0x918
#define	DW_IATU_UPPER_TARGET_ADDR	0x91C

/* Modern (4.80+) "unroll" iATU mode */
#define	DW_IATU_UR_STEP			0x200
#define	DW_IATU_UR_REG(r, n)		(r) * DW_IATU_UR_STEP + IATU_UR_##n
#define	 IATU_UR_CTRL1				0x00
#define	 IATU_UR_CTRL2				0x04
#define	 IATU_UR_LWR_BASE_ADDR			0x08
#define	 IATU_UR_UPPER_BASE_ADDR		0x0C
#define	 IATU_UR_LIMIT_ADDR			0x10
#define	 IATU_UR_LWR_TARGET_ADDR		0x14
#define	 IATU_UR_UPPER_TARGET_ADDR		0x18

#define	DW_DEFAULT_IATU_UR_DBI_OFFSET	0x300000
#define	DW_DEFAULT_IATU_UR_DBI_SIZE	0x1000

struct pci_dw_softc {
	struct ofw_pci_softc	ofw_pci;	/* Must be first */

	/* Filled by attachement stub */
	struct resource		*dbi_res;

	/* pci_dw variables */
	device_t		dev;
	phandle_t		node;
	struct mtx		mtx;
	struct resource		*cfg_res;

	struct ofw_pci_range	io_range;
	struct ofw_pci_range	*mem_ranges;
	int			num_mem_ranges;

	bool			coherent;
	bus_dma_tag_t		dmat;

	int			num_lanes;
	int			num_out_regions;
	struct resource		*iatu_ur_res;	/* NB: May be dbi_res */
	bus_addr_t		iatu_ur_offset;
	bus_size_t		iatu_ur_size;
	bus_addr_t		cfg_pa;   	/* PA of config memoty */
	bus_size_t		cfg_size; 	/* size of config  region */

	u_int 			bus_start;
	u_int 			bus_end;
	u_int 			root_bus;
	u_int 			sub_bus;
};

DECLARE_CLASS(pci_dw_driver);

static inline void
pci_dw_dbi_wr4(device_t dev, u_int reg, uint32_t val)
{
	PCI_DW_DBI_WRITE(dev, reg, val, 4);
}

static inline void
pci_dw_dbi_wr2(device_t dev, u_int reg, uint16_t val)
{
	PCI_DW_DBI_WRITE(dev, reg, val, 2);
}

static inline void
pci_dw_dbi_wr1(device_t dev, u_int reg, uint8_t val)
{
	PCI_DW_DBI_WRITE(dev, reg, val, 1);
}

static inline uint32_t
pci_dw_dbi_rd4(device_t dev, u_int reg)
{
	return (PCI_DW_DBI_READ(dev, reg, 4));
}

static inline uint16_t
pci_dw_dbi_rd2(device_t dev, u_int reg)
{
	return ((uint16_t)PCI_DW_DBI_READ(dev, reg, 2));
}

static inline uint8_t
pci_dw_dbi_rd1(device_t dev, u_int reg)
{
	return ((uint8_t)PCI_DW_DBI_READ(dev, reg, 1));
}

int pci_dw_init(device_t);

#endif /* __PCI_HOST_GENERIC_H_ */
