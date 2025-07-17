/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

/* Layerscape DesignWare PCIe driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
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

#define	PCIE_ABSERR		0x8D0

struct qoriq_dw_pci_cfg {
	uint32_t	pex_pf0_dgb;	/* offset of PEX_PF0_DBG register */
	uint32_t	ltssm_bit;	/* LSB bit of LTSSM state field */
};

struct qorif_dw_pci_softc {
	struct pci_dw_softc	dw_sc;
	device_t		dev;
	phandle_t		node;
	struct resource 	*irq_res;
	void			*intr_cookie;
	struct qoriq_dw_pci_cfg	*soc_cfg;

};

static struct qoriq_dw_pci_cfg ls1043_cfg = {
	.pex_pf0_dgb = 0x10000 + 0x7FC,
	.ltssm_bit = 24,
};

static struct qoriq_dw_pci_cfg ls1012_cfg = {
	.pex_pf0_dgb = 0x80000 + 0x407FC,
	.ltssm_bit = 24,
};

static struct qoriq_dw_pci_cfg ls2080_cfg = {
	.pex_pf0_dgb = 0x80000 + 0x7FC,
	.ltssm_bit = 0,
};

static struct qoriq_dw_pci_cfg ls2028_cfg = {
	.pex_pf0_dgb = 0x80000 + 0x407FC,
	.ltssm_bit = 0,
};


/* Compatible devices. */
static struct ofw_compat_data compat_data[] = {
	{"fsl,ls1012a-pcie", (uintptr_t)&ls1012_cfg},
	{"fsl,ls1028a-pcie", (uintptr_t)&ls2028_cfg},
	{"fsl,ls1043a-pcie", (uintptr_t)&ls1043_cfg},
	{"fsl,ls1046a-pcie", (uintptr_t)&ls1012_cfg},
	{"fsl,ls2080a-pcie", (uintptr_t)&ls2080_cfg},
	{"fsl,ls2085a-pcie", (uintptr_t)&ls2080_cfg},
	{"fsl,ls2088a-pcie", (uintptr_t)&ls2028_cfg},
	{"fsl,ls1088a-pcie", (uintptr_t)&ls2028_cfg},
	{NULL,		 	  0},
};

static void
qorif_dw_pci_dbi_protect(struct qorif_dw_pci_softc *sc, bool protect)
{
	uint32_t reg;

	reg = pci_dw_dbi_rd4(sc->dev, DW_MISC_CONTROL_1);
	if (protect)
		reg &= ~DBI_RO_WR_EN;
	else
		reg |= DBI_RO_WR_EN;
	pci_dw_dbi_wr4(sc->dev, DW_MISC_CONTROL_1, reg);
}

static int qorif_dw_pci_intr(void *arg)
{
#if 0
	struct qorif_dw_pci_softc *sc = arg;
	uint32_t cause1, cause2;

	/* Ack all interrups */
	cause1 = pci_dw_dbi_rd4(sc->dev, MV_INT_CAUSE1);
	cause2 = pci_dw_dbi_rd4(sc->dev, MV_INT_CAUSE2);

	pci_dw_dbi_wr4(sc->dev, MV_INT_CAUSE1, cause1);
	pci_dw_dbi_wr4(sc->dev, MV_INT_CAUSE2, cause2);
#endif
	return (FILTER_HANDLED);
}

static int
qorif_dw_pci_get_link(device_t dev, bool *status)
{
	struct qorif_dw_pci_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	reg = pci_dw_dbi_rd4(sc->dev, sc->soc_cfg->pex_pf0_dgb);
	reg >>=  sc->soc_cfg->ltssm_bit;
	reg &= 0x3F;
	*status = (reg == 0x11) ? true : false;
	return (0);
}

static void
qorif_dw_pci_init(struct qorif_dw_pci_softc *sc)
{

//	ls_pcie_disable_outbound_atus(pcie);

	/* Forward error response */
	pci_dw_dbi_wr4(sc->dev, PCIE_ABSERR,  0x9401);

	qorif_dw_pci_dbi_protect(sc, true);
	pci_dw_dbi_wr1(sc->dev, PCIR_HDRTYPE, 1);
	qorif_dw_pci_dbi_protect(sc, false);

//	ls_pcie_drop_msg_tlp(pcie);

}

static int
qorif_dw_pci_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "NPX Layaerscape PCI-E Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
qorif_dw_pci_attach(device_t dev)
{
	struct resource_map_request req;
	struct resource_map map;
	struct qorif_dw_pci_softc *sc;
	phandle_t node;
	int rv;
	int rid;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->dev = dev;
	sc->node = node;
	sc->soc_cfg = (struct qoriq_dw_pci_cfg *)
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data;

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

	rv = pci_dw_init(dev);
	if (rv != 0)
		goto out;

	qorif_dw_pci_init(sc);

	/* Setup interrupt  */
	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
		    qorif_dw_pci_intr, NULL, sc, &sc->intr_cookie)) {
		device_printf(dev, "cannot setup interrupt handler\n");
		rv = ENXIO;
		goto out;
	}

	bus_attach_children(dev);
	return (0);
out:
	/* XXX Cleanup */
	return (rv);
}

static device_method_t qorif_dw_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			qorif_dw_pci_probe),
	DEVMETHOD(device_attach,		qorif_dw_pci_attach),

	DEVMETHOD(pci_dw_get_link,		qorif_dw_pci_get_link),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, qorif_dw_pci_driver, qorif_dw_pci_methods,
    sizeof(struct qorif_dw_pci_softc), pci_dw_driver);
DRIVER_MODULE( qorif_dw_pci, simplebus, qorif_dw_pci_driver, NULL, NULL);
