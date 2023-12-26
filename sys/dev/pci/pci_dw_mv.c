/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

/* Armada 8k DesignWare PCIe driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/devmap.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/phy/phy.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofwpci.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_dw.h>

#include "pcib_if.h"
#include "pci_dw_if.h"

#define MV_GLOBAL_CONTROL_REG		0x8000
#define PCIE_APP_LTSSM_EN		(1 << 2)

#define MV_GLOBAL_STATUS_REG		0x8008
#define	 MV_STATUS_RDLH_LINK_UP			(1 << 1)
#define  MV_STATUS_PHY_LINK_UP			(1 << 9)

#define MV_INT_CAUSE1			0x801C
#define MV_INT_MASK1			0x8020
#define  INT_A_ASSERT_MASK			(1 <<  9)
#define  INT_B_ASSERT_MASK			(1 << 10)
#define  INT_C_ASSERT_MASK			(1 << 11)
#define  INT_D_ASSERT_MASK			(1 << 12)

#define MV_INT_CAUSE2			0x8024
#define MV_INT_MASK2			0x8028
#define MV_ERR_INT_CAUSE		0x802C
#define MV_ERR_INT_MASK			0x8030

#define MV_ARCACHE_TRC_REG		0x8050
#define MV_AWCACHE_TRC_REG		0x8054
#define MV_ARUSER_REG			0x805C
#define MV_AWUSER_REG			0x8060

#define	MV_MAX_LANES	8
struct pci_mv_softc {
	struct pci_dw_softc	dw_sc;
	device_t		dev;
	phandle_t		node;
	struct resource 	*irq_res;
	void			*intr_cookie;
	phy_t			phy[MV_MAX_LANES];
	clk_t			clk_core;
	clk_t			clk_reg;
};

/* Compatible devices. */
static struct ofw_compat_data compat_data[] = {
	{"marvell,armada8k-pcie", 1},
	{NULL,		 	  0},
};

static int
pci_mv_phy_init(struct pci_mv_softc *sc)
{
	int i, rv;

	for (i = 0; i < MV_MAX_LANES; i++) {
		rv =  phy_get_by_ofw_idx(sc->dev, sc->node, i, &(sc->phy[i]));
		if (rv != 0 && rv != ENOENT) {
			device_printf(sc->dev, "Cannot get phy[%d]\n", i);
/* XXX revert when phy driver will be implemented */
#if 0
		goto fail;
#else
		continue;
#endif
		}
		if (sc->phy[i] == NULL)
			continue;
		rv = phy_enable(sc->phy[i]);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot enable phy[%d]\n", i);
			goto fail;
		}
	}
	return (0);

fail:
	for (i = 0; i < MV_MAX_LANES; i++) {
		if (sc->phy[i] == NULL)
			continue;
		phy_release(sc->phy[i]);
	  }

	return (rv);
}

static void
pci_mv_init(struct pci_mv_softc *sc)
{
	uint32_t reg;

	/* Set device configuration to RC */
	reg = pci_dw_dbi_rd4(sc->dev, MV_GLOBAL_CONTROL_REG);
	reg &= ~0x000000F0;
	reg |= 0x000000040;
	pci_dw_dbi_wr4(sc->dev, MV_GLOBAL_CONTROL_REG, reg);

	/* AxCache master transaction attribures */
	pci_dw_dbi_wr4(sc->dev, MV_ARCACHE_TRC_REG, 0x3511);
	pci_dw_dbi_wr4(sc->dev, MV_AWCACHE_TRC_REG, 0x5311);

	/* AxDomain master transaction attribures */
	pci_dw_dbi_wr4(sc->dev, MV_ARUSER_REG, 0x0002);
	pci_dw_dbi_wr4(sc->dev, MV_AWUSER_REG, 0x0002);

	/* Enable all INTx interrupt (virtuual) pins */
	reg = pci_dw_dbi_rd4(sc->dev, MV_INT_MASK1);
	reg |= INT_A_ASSERT_MASK | INT_B_ASSERT_MASK |
	       INT_C_ASSERT_MASK | INT_D_ASSERT_MASK;
	pci_dw_dbi_wr4(sc->dev, MV_INT_MASK1, reg);

	/* Enable local interrupts */
	pci_dw_dbi_wr4(sc->dev, DW_MSI_INTR0_MASK, 0xFFFFFFFF);
	pci_dw_dbi_wr4(sc->dev, MV_INT_MASK1, 0x0001FE00);
	pci_dw_dbi_wr4(sc->dev, MV_INT_MASK2, 0x00000000);
	pci_dw_dbi_wr4(sc->dev, MV_INT_CAUSE1, 0xFFFFFFFF);
	pci_dw_dbi_wr4(sc->dev, MV_INT_CAUSE2, 0xFFFFFFFF);

	/* Errors have own interrupt, not yet populated in DTt */
	pci_dw_dbi_wr4(sc->dev, MV_ERR_INT_MASK, 0);
}

static int pci_mv_intr(void *arg)
{
	struct pci_mv_softc *sc = arg;
	uint32_t cause1, cause2;

	/* Ack all interrups */
	cause1 = pci_dw_dbi_rd4(sc->dev, MV_INT_CAUSE1);
	cause2 = pci_dw_dbi_rd4(sc->dev, MV_INT_CAUSE2);

	pci_dw_dbi_wr4(sc->dev, MV_INT_CAUSE1, cause1);
	pci_dw_dbi_wr4(sc->dev, MV_INT_CAUSE2, cause2);
	return (FILTER_HANDLED);
}

static int
pci_mv_get_link(device_t dev, bool *status)
{
	uint32_t reg;

	reg = pci_dw_dbi_rd4(dev, MV_GLOBAL_STATUS_REG);
	if ((reg & (MV_STATUS_RDLH_LINK_UP | MV_STATUS_PHY_LINK_UP)) ==
	    (MV_STATUS_RDLH_LINK_UP | MV_STATUS_PHY_LINK_UP))
		*status = true;
	else
		*status = false;

	return (0);
}

static int
pci_mv_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell Armada8K PCI-E Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
pci_mv_attach(device_t dev)
{
	struct resource_map_request req;
	struct resource_map map;
	struct pci_mv_softc *sc;
	phandle_t node;
	int rv;
	int rid;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->dev = dev;
	sc->node = node;

	rid = 0;
	sc->dw_sc.dbi_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE | RF_UNMAPPED);
	if (sc->dw_sc.dbi_res == NULL) {
		device_printf(dev, "Cannot allocate DBI memory\n");
		rv = ENXIO;
		goto out;
	}

	resource_init_map_request(&req);
	req.memattr = VM_MEMATTR_DEVICE_NP;
	rv = bus_map_resource(dev, SYS_RES_MEMORY, sc->dw_sc.dbi_res, &req,
	    &map);
	if (rv != 0) {
		device_printf(dev, "could not map memory.\n");
		return (rv);
	}
	rman_set_mapping(sc->dw_sc.dbi_res, &map);

	/* PCI interrupt */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resources\n");
		rv = ENXIO;
		goto out;
	}

	/* Clocks */
	rv = clk_get_by_ofw_name(sc->dev, 0, "core", &sc->clk_core);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'core' clock\n");
		rv = ENXIO;
		goto out;
	}

	rv = clk_get_by_ofw_name(sc->dev, 0, "reg", &sc->clk_reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'reg' clock\n");
		rv = ENXIO;
		goto out;
	}

	rv = clk_enable(sc->clk_core);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'core' clock\n");
		rv = ENXIO;
		goto out;
	}

	rv = clk_enable(sc->clk_reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'reg' clock\n");
		rv = ENXIO;
		goto out;
	}

	rv = pci_mv_phy_init(sc);
	if (rv)
		goto out;

	rv = pci_dw_init(dev);
	if (rv != 0)
		goto out;

	pci_mv_init(sc);

	/* Setup interrupt  */
	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
		    pci_mv_intr, NULL, sc, &sc->intr_cookie)) {
		device_printf(dev, "cannot setup interrupt handler\n");
		rv = ENXIO;
		goto out;
	}

	return (bus_generic_attach(dev));
out:
	/* XXX Cleanup */
	return (rv);
}

static device_method_t pci_mv_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			pci_mv_probe),
	DEVMETHOD(device_attach,		pci_mv_attach),

	DEVMETHOD(pci_dw_get_link,		pci_mv_get_link),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, pci_mv_driver, pci_mv_methods,
    sizeof(struct pci_mv_softc), pci_dw_driver);
DRIVER_MODULE( pci_mv, simplebus, pci_mv_driver, NULL, NULL);
