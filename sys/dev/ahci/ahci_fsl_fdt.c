/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#define	AHCI_FSL_REG_PTC	0xc8

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

#define	AHCI_FSL_REG_PTC_RXWM_MASK	0x0000007f
#define	AHCI_FSL_REG_PTC_ENBD		(1 << 8)
#define	AHCI_FSL_REG_PTC_ITM		(1 << 9)

#define	AHCI_FSL_REG_PHY1_CFG	((0x1fffe & AHCI_FSL_REG_PHY1_TTA_MASK)	\
    | AHCI_FSL_REG_PHY1_SNM | AHCI_FSL_REG_PHY1_PSS			\
    | AHCI_FSL_REG_PHY1_ESDF)
#define	AHCI_FSL_REG_PHY2_CFG	((0x1f << AHCI_FSL_PHY2_CIBGMN_SHIFT)	\
    | (0x4d << AHCI_FSL_PHY2_CIBGMX_SHIFT)				\
    | (0x18 << AHCI_FSL_PHY2_CIBGN_SHIFT)				\
    | (0x28 << AHCI_FSL_PHY2_CINMP_SHIFT))
#define	AHCI_FSL_REG_PHY3_CFG	((0x09 << AHCI_FSL_PHY3_CWBGMN_SHIFT)	\
    | (0x15 << AHCI_FSL_PHY3_CWBGMX_SHIFT)				\
    | (0x08 << AHCI_FSL_PHY3_CWBGN_SHIFT)				\
    | (0x0e << AHCI_FSL_PHY3_CWNMP_SHIFT))
/* Bit 27 enabled so value of reserved bits remains as in documentation. */
#define	AHCI_FSL_REG_PTC_CFG	((0x29 & AHCI_FSL_REG_PTC_RXWM_MASK)	\
    | (1 << 27))

#define	AHCI_FSL_REG_ECC	0x0
#define	AHCI_FSL_REG_ECC_DIS	0x80000000

struct ahci_fsl_fdt_soc_data;

struct ahci_fsl_fdt_controller {
	struct ahci_controller	ctlr;	/* Must be the first field. */
	const struct ahci_fsl_fdt_soc_data *soc_data;
	struct resource		*r_ecc;
	int			r_ecc_rid;
};

static void
ahci_fsl_fdt_ls1046a_phy_init(struct ahci_fsl_fdt_controller *ctlr)
{
	struct ahci_controller *ahci;
	uint32_t val;

	ahci = &ctlr->ctlr;
	if (ctlr->r_ecc) {
		val = ATA_INL(ctlr->r_ecc, AHCI_FSL_REG_ECC) |
		    AHCI_FSL_REG_ECC_DIS;
		ATA_OUTL(ctlr->r_ecc, AHCI_FSL_REG_ECC, val);
	}
	ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY1, AHCI_FSL_REG_PHY1_CFG);
	ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY2, AHCI_FSL_REG_PHY2_CFG);
	ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PHY3, AHCI_FSL_REG_PHY3_CFG);
	ATA_OUTL(ahci->r_mem, AHCI_FSL_REG_PTC, AHCI_FSL_REG_PTC_CFG);
}

struct ahci_fsl_fdt_soc_data {
	void (* phy_init)(struct ahci_fsl_fdt_controller *ctlr);
};

static const struct ahci_fsl_fdt_soc_data ahci_fsl_fdt_ls1046a_soc_data = {
	.phy_init = ahci_fsl_fdt_ls1046a_phy_init,
};

static const struct ofw_compat_data ahci_fsl_fdt_compat_data[] = {
	{"fsl,ls1046a-ahci",	(uintptr_t)&ahci_fsl_fdt_ls1046a_soc_data},
	{NULL,			0}
};

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
	uintptr_t ocd_data;
	phandle_t node;
	clk_t clock;
	int ret;

	node = ofw_bus_get_node(dev);
	ctlr = device_get_softc(dev);
	ocd_data = ofw_bus_search_compatible(dev,
	    ahci_fsl_fdt_compat_data)->ocd_data;
	ctlr->soc_data = (struct ahci_fsl_fdt_soc_data *)ocd_data;
	ahci = &ctlr->ctlr;
	ahci->dev = dev;
	ahci->r_rid = 0;
	ahci->quirks = AHCI_Q_NOPMP;

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
		device_printf(dev,
		    "Could not locate \"ahci\" string in the \"reg-names\" property");
		return (ENOENT);
	}

	ahci->r_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &ahci->r_rid, RF_ACTIVE);
	if (!ahci->r_mem) {
		device_printf(dev,
		    "Could not allocate resources for controller\n");
		return (ENOMEM);
	}

	if (!ofw_bus_find_string_index(node, "reg-names", "sata-ecc",
	    &ctlr->r_ecc_rid)) {
		ctlr->r_ecc = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &ctlr->r_ecc_rid, RF_ACTIVE);
		if (!ctlr->r_ecc) {
			device_printf(dev,
			    "Could not allocate resources for controller\n");
			ret = ENOMEM;
			goto err_free_mem;
		}
	}

	/* Setup controller defaults. */
	ahci->numirqs = 1;
	ctlr->soc_data->phy_init(ctlr);

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
	DEVMETHOD(bus_child_location_str,	ahci_child_location_str),
	DEVMETHOD(bus_get_dma_tag,  		ahci_get_dma_tag),
	DEVMETHOD_END
};

static driver_t ahci_fsl_fdt_driver = {
	"ahci",
	ahci_fsl_fdt_methods,
	sizeof(struct ahci_fsl_fdt_controller),
};

static devclass_t ahci_fsl_fdt_devclass;
DRIVER_MODULE(ahci_fsl, simplebus, ahci_fsl_fdt_driver, ahci_fsl_fdt_devclass,
    NULL, NULL);
DRIVER_MODULE(ahci_fsl, ofwbus, ahci_fsl_fdt_driver, ahci_fsl_fdt_devclass,
    NULL, NULL);
