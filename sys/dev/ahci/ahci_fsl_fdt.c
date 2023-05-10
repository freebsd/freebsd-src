/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alstom Group
 * Copyright (c) 2020 Semihalf
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* AHCI controller driver for NXP QorIQ Layerscape SoCs. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/rman.h>
#include <sys/unistd.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/ahci/ahci.h>

#include <dev/extres/clk/clk.h>

#define	AHCI_FSL_REG_PHY1	0xa8
#define	AHCI_FSL_REG_PHY2	0xac
#define	AHCI_FSL_REG_PHY3	0xb0
#define	AHCI_FSL_REG_PHY4	0xb4
#define	AHCI_FSL_REG_PHY5	0xb8
#define	AHCI_FSL_REG_AXICC	0xbc
#define	AHCI_FSL_REG_PTC	0xc8

#define AHCI_FSL_LS1021A_AXICC	0xc0

#define	AHCI_FSL_REG_PHY1_TTA_MASK	0x0001ffff
#define	AHCI_FSL_REG_PHY1_SNM		(1 << 17)
#define	AHCI_FSL_REG_PHY1_SNR		(1 << 18)
#define	AHCI_FSL_REG_PHY1_FPR		(1 << 20)
#define	AHCI_FSL_REG_PHY1_PBPS_LBP	0
#define	AHCI_FSL_REG_PHY1_PBPS_LFTP	(0x01 << 21)
#define	AHCI_FSL_REG_PHY1_PBPS_MFTP	(0x02 << 21)
#define	AHCI_FSL_REG_PHY1_PBPS_HFTP	(0x03 << 21)
#define	AHCI_FSL_REG_PHY1_PBPS_PRBS	(0x04 << 21)
#define	AHCI_FSL_REG_PHY1_PBPS_BIST	(0x05 << 21)
#define	AHCI_FSL_REG_PHY1_PBPE		(1 << 24)
#define	AHCI_FSL_REG_PHY1_PBCE		(1 << 25)
#define	AHCI_FSL_REG_PHY1_PBPNA		(1 << 26)
#define	AHCI_FSL_REG_PHY1_STB		(1 << 27)
#define	AHCI_FSL_REG_PHY1_PSSO		(1 << 28)
#define	AHCI_FSL_REG_PHY1_PSS		(1 << 29)
#define	AHCI_FSL_REG_PHY1_ERSN		(1 << 30)
#define	AHCI_FSL_REG_PHY1_ESDF		(1 << 31)

#define	AHCI_FSL_REG_PHY_MASK		0xff

#define	AHCI_FSL_PHY2_CIBGMN_SHIFT	0
#define	AHCI_FSL_PHY2_CIBGMX_SHIFT	8
#define	AHCI_FSL_PHY2_CIBGN_SHIFT	16
#define	AHCI_FSL_PHY2_CINMP_SHIFT	24


#define	AHCI_FSL_PHY3_CWBGMN_SHIFT	0
#define	AHCI_FSL_PHY3_CWBGMX_SHIFT	8
#define	AHCI_FSL_PHY3_CWBGN_SHIFT	16
#define	AHCI_FSL_PHY3_CWNMP_SHIFT	24

/* Only in LS1021A */
#define	AHCI_FSL_PHY4_BMX_SHIFT		0
#define	AHCI_FSL_PHY4_BNM_SHIFT		8
#define	AHCI_FSL_PHY4_SFD_SHIFT		16
#define	AHCI_FSL_PHY4_PTST_SHIFT	24

/* Only in LS1021A */
#define	AHCI_FSL_PHY5_RIT_SHIFT		0
#define	AHCI_FSL_PHY5_RCT_SHIFT		20

#define	AHCI_FSL_REG_PTC_RXWM_MASK	0x0000007f
#define	AHCI_FSL_REG_PTC_ENBD		(1 << 8)
#define	AHCI_FSL_REG_PTC_ITM		(1 << 9)

#define	AHCI_FSL_REG_PHY1_CFG						\
    ((0x1fffe & AHCI_FSL_REG_PHY1_TTA_MASK) |				\
     AHCI_FSL_REG_PHY1_SNM | AHCI_FSL_REG_PHY1_PSS | AHCI_FSL_REG_PHY1_ESDF)

#define	AHCI_FSL_REG_PHY2_CFG						\
    ((0x1f << AHCI_FSL_PHY2_CIBGMN_SHIFT) |				\
     (0x4d << AHCI_FSL_PHY2_CIBGMX_SHIFT) |				\
     (0x18 << AHCI_FSL_PHY2_CIBGN_SHIFT) |				\
     (0x28 << AHCI_FSL_PHY2_CINMP_SHIFT))

#define	AHCI_FSL_REG_PHY2_CFG_LS1021A					\
    ((0x14 << AHCI_FSL_PHY2_CIBGMN_SHIFT) |				\
     (0x34 << AHCI_FSL_PHY2_CIBGMX_SHIFT) |				\
     (0x18 << AHCI_FSL_PHY2_CIBGN_SHIFT) |				\
     (0x28 << AHCI_FSL_PHY2_CINMP_SHIFT))

#define	AHCI_FSL_REG_PHY3_CFG						\
    ((0x09 << AHCI_FSL_PHY3_CWBGMN_SHIFT) |				\
     (0x15 << AHCI_FSL_PHY3_CWBGMX_SHIFT) |				\
     (0x08 << AHCI_FSL_PHY3_CWBGN_SHIFT) |				\
     (0x0e << AHCI_FSL_PHY3_CWNMP_SHIFT))

#define	AHCI_FSL_REG_PHY3_CFG_LS1021A					\
    ((0x06 << AHCI_FSL_PHY3_CWBGMN_SHIFT) |				\
     (0x0e << AHCI_FSL_PHY3_CWBGMX_SHIFT) |				\
     (0x08 << AHCI_FSL_PHY3_CWBGN_SHIFT) |				\
     (0x0e << AHCI_FSL_PHY3_CWNMP_SHIFT))

#define	AHCI_FSL_REG_PHY4_CFG_LS1021A					\
    ((0x0b << AHCI_FSL_PHY4_BMX_SHIFT) |				\
     (0x08 << AHCI_FSL_PHY4_BNM_SHIFT) |				\
     (0x4a << AHCI_FSL_PHY4_SFD_SHIFT) |				\
     (0x06 << AHCI_FSL_PHY4_PTST_SHIFT))

#define	AHCI_FSL_REG_PHY5_CFG_LS1021A					\
    ((0x86470 << AHCI_FSL_PHY5_RIT_SHIFT) |				\
     (0x2aa << AHCI_FSL_PHY5_RCT_SHIFT))

/* Bit 27 enabled so value of reserved bits remains as in documentation. */
#define	AHCI_FSL_REG_PTC_CFG						\
    ((0x29 & AHCI_FSL_REG_PTC_RXWM_MASK) | (1 << 27))

#define	AHCI_FSL_REG_AXICC_CFG	0x3fffffff


#define	AHCI_FSL_REG_ECC	0x0
#define	AHCI_FSL_REG_ECC_LS1021A	0x00020000
#define	AHCI_FSL_REG_ECC_LS1043A	0x80000000
#define	AHCI_FSL_REG_ECC_LS1028A	0x40000000


#define QORIQ_AHCI_LS1021A	1
#define QORIQ_AHCI_LS1028A	2
#define QORIQ_AHCI_LS1043A	3
#define QORIQ_AHCI_LS2080A	4
#define QORIQ_AHCI_LS1046A	5
#define QORIQ_AHCI_LS1088A	6
#define QORIQ_AHCI_LS2088A	7
#define QORIQ_AHCI_LX2160A	8

struct ahci_fsl_fdt_controller {
	struct ahci_controller	ctlr;	/* Must be the first field. */
	int			soc_type;
	struct resource		*r_ecc;
	int			r_ecc_rid;
};

static const struct ofw_compat_data ahci_fsl_fdt_compat_data[] = {
	{"fsl,ls1021a-ahci",	QORIQ_AHCI_LS1021A},
	{"fsl,ls1028a-ahci",	QORIQ_AHCI_LS1028A},
	{"fsl,ls1043a-ahci",	QORIQ_AHCI_LS1043A},
	{"fsl,ls2080a-ahci",	QORIQ_AHCI_LS2080A},
	{"fsl,ls1046a-ahci",	QORIQ_AHCI_LS1046A},
	{"fsl,ls1088a-ahci",	QORIQ_AHCI_LS1088A},
	{"fsl,ls2088a-ahci",	QORIQ_AHCI_LS2088A},
	{"fsl,lx2160a-ahci",	QORIQ_AHCI_LX2160A},
	{NULL,			0}
};

static bool ecc_inited;

static int
ahci_fsl_fdt_ecc_init(struct ahci_fsl_fdt_controller *ctrl)
{
	uint32_t val;

	switch (ctrl->soc_type) {
	case QORIQ_AHCI_LS2080A:
	case QORIQ_AHCI_LS2088A:
		return (0);

	case QORIQ_AHCI_LS1021A:
		if (!ecc_inited && ctrl->r_ecc == NULL)
			return (ENXIO);
		if (!ecc_inited)
			ATA_OUTL(ctrl->r_ecc, AHCI_FSL_REG_ECC,
			     AHCI_FSL_REG_ECC_LS1021A);
		break;

	case QORIQ_AHCI_LS1043A:
	case QORIQ_AHCI_LS1046A:
		if (!ecc_inited && ctrl->r_ecc == NULL)
			return (ENXIO);
		if (!ecc_inited) {
			val = ATA_INL(ctrl->r_ecc, AHCI_FSL_REG_ECC);
			val = AHCI_FSL_REG_ECC_LS1043A;
			ATA_OUTL(ctrl->r_ecc, AHCI_FSL_REG_ECC, val);
		}
		break;

	case QORIQ_AHCI_LS1028A:
	case QORIQ_AHCI_LS1088A:
	case QORIQ_AHCI_LX2160A:
		if (!ecc_inited && ctrl->r_ecc == NULL)
			return (ENXIO);
		if (!ecc_inited) {

			val = ATA_INL(ctrl->r_ecc, AHCI_FSL_REG_ECC);
			val |= AHCI_FSL_REG_ECC_LS1028A;
			ATA_OUTL(ctrl->r_ecc, AHCI_FSL_REG_ECC, val);
		}
		break;

	default:
		panic("Unimplemented SOC type: %d", ctrl->soc_type);
	}

	ecc_inited = true;
	return (0);
}

static void
ahci_fsl_fdt_phy_init(struct ahci_fsl_fdt_controller *ctrl)
{
	struct ahci_controller *ahci;

	ahci = &ctrl->ctlr;
	if (ctrl->soc_type == QORIQ_AHCI_LS1021A) {
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY1,
		    AHCI_FSL_REG_PHY1_CFG);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY2,
		    AHCI_FSL_REG_PHY2_CFG_LS1021A);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY3,
		    AHCI_FSL_REG_PHY3_CFG_LS1021A);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY4,
		    AHCI_FSL_REG_PHY4_CFG_LS1021A);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY5,
		    AHCI_FSL_REG_PHY5_CFG_LS1021A);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PTC,
		    AHCI_FSL_REG_PTC_CFG);

		if (ctrl->ctlr.dma_coherent)
			ATA_OUTL(ahci->r_mem, AHCI_FSL_LS1021A_AXICC,
			    AHCI_FSL_REG_AXICC_CFG);
	} else {
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY1,
		    AHCI_FSL_REG_PHY1_CFG);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY2,
		    AHCI_FSL_REG_PHY2_CFG);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY3,
		    AHCI_FSL_REG_PHY3_CFG);
		ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PTC,
		    AHCI_FSL_REG_PTC_CFG);

		if (ctrl->ctlr.dma_coherent)
			ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_AXICC,
			    AHCI_FSL_REG_AXICC_CFG);
	}
}

static int
ahci_fsl_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, ahci_fsl_fdt_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "NXP QorIQ Layerscape AHCI controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ahci_fsl_fdt_attach(device_t dev)
{
	struct ahci_fsl_fdt_controller *ctlr;
	struct ahci_controller *ahci;
	phandle_t node;
	clk_t clock;
	int ret;

	node = ofw_bus_get_node(dev);
	ctlr = device_get_softc(dev);
	ctlr->soc_type =
	    ofw_bus_search_compatible(dev, ahci_fsl_fdt_compat_data)->ocd_data;
	ahci = &ctlr->ctlr;
	ahci->dev = dev;
	ahci->r_rid = 0;
	ahci->quirks = AHCI_Q_NOPMP;

	ahci->dma_coherent = OF_hasprop(node, "dma-coherent");

	ret = clk_get_by_ofw_index(dev, node, 0, &clock);
	if (ret != 0) {
		device_printf(dev, "No clock found.\n");
		return (ENXIO);
	}

	ret = clk_enable(clock);
	if (ret !=0) {
		device_printf(dev, "Could not enable clock.\n");
		return (ENXIO);
	}

	if (OF_hasprop(node, "reg-names") && ofw_bus_find_string_index(node,
	    "reg-names", "ahci", &ahci->r_rid)) {
		device_printf(dev, "Could not locate 'ahci' string in the "
		    "'reg-names' property");
		return (ENOENT);
	}

	ahci->r_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &ahci->r_rid, RF_ACTIVE);
	if (!ahci->r_mem) {
		device_printf(dev,
		    "Could not allocate resources for controller\n");
		return (ENOMEM);
	}

	ret = ofw_bus_find_string_index(node, "reg-names", "sata-ecc",
	    &ctlr->r_ecc_rid);
	if (ret == 0) {
		ctlr->r_ecc = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &ctlr->r_ecc_rid, RF_ACTIVE| RF_SHAREABLE);
		if (!ctlr->r_ecc) {
			device_printf(dev,
			    "Could not allocate resources for controller\n");
			ret = ENOMEM;
			goto err_free_mem;
		}
	} else if (ret != ENOENT) {
		device_printf(dev, "Could not locate 'sata-ecc' string in "
		"the 'reg-names' property");
		goto err_free_mem;
	}

	ret = ahci_fsl_fdt_ecc_init(ctlr);
	if (ret != 0) {
		device_printf(dev, "Could not initialize 'ecc' registers");
		goto err_free_mem;
	}

	/* Setup controller defaults. */
	ahci->numirqs = 1;
	ahci_fsl_fdt_phy_init(ctlr);

	/* Reset controller. */
	ret = ahci_ctlr_reset(dev);
	if (ret)
		goto err_free_mem;

	ret = ahci_attach(dev);
	if (ret) {
		device_printf(dev,
		    "Could not initialize AHCI, with error: %d\n", ret);
		goto err_free_ecc;
	}
	return (0);

err_free_mem:
	bus_free_resource(dev, SYS_RES_MEMORY, ahci->r_mem);
err_free_ecc:
	if (ctlr->r_ecc)
		bus_free_resource(dev, SYS_RES_MEMORY, ctlr->r_ecc);
	return (ret);
}

static int
ahci_fsl_fdt_detach(device_t dev)
{
	struct ahci_fsl_fdt_controller *ctlr;

	ctlr = device_get_softc(dev);
	if (ctlr->r_ecc)
		bus_free_resource(dev, SYS_RES_MEMORY, ctlr->r_ecc);
	return ahci_detach(dev);
}

static const device_method_t ahci_fsl_fdt_methods[] = {
	DEVMETHOD(device_probe,			ahci_fsl_fdt_probe),
	DEVMETHOD(device_attach,		ahci_fsl_fdt_attach),
	DEVMETHOD(device_detach,		ahci_fsl_fdt_detach),
	DEVMETHOD(bus_alloc_resource,		ahci_alloc_resource),
	DEVMETHOD(bus_release_resource,		ahci_release_resource),
	DEVMETHOD(bus_setup_intr,   		ahci_setup_intr),
	DEVMETHOD(bus_teardown_intr,		ahci_teardown_intr),
	DEVMETHOD(bus_print_child,		ahci_print_child),
	DEVMETHOD(bus_child_location,		ahci_child_location),
	DEVMETHOD(bus_get_dma_tag,  		ahci_get_dma_tag),
	DEVMETHOD_END
};

static driver_t ahci_fsl_fdt_driver = {
	"ahci",
	ahci_fsl_fdt_methods,
	sizeof(struct ahci_fsl_fdt_controller),
};

DRIVER_MODULE(ahci_fsl, simplebus, ahci_fsl_fdt_driver, NULL, NULL);
DRIVER_MODULE(ahci_fsl, ofwbus, ahci_fsl_fdt_driver, NULL, NULL);
