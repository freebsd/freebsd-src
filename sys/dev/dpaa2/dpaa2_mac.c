/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * The DPAA2 MAC driver.
 *
 * For every DPAA2 MAC, there is an MC object named DPMAC, for MDIO and link
 * state updates. The DPMAC virtualizes the MDIO interface, so each PHY driver
 * may see a private interface (removing the need for synchronization in GPP on
 * the multiplexed MDIO hardware).
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>

#include "pcib_if.h"
#include "pci_if.h"

#include "dpaa2_mc.h"
#include "dpaa2_ni.h"
#include "dpaa2_mcp.h"
#include "dpaa2_swp.h"
#include "dpaa2_swp_if.h"
#include "dpaa2_cmd_if.h"

/* Index of the only DPMAC IRQ. */
#define DPMAC_IRQ_INDEX		0

/* DPMAC IRQ statuses. */
#define DPMAC_IRQ_LINK_CFG_REQ	0x00000001 /* change in requested link config. */
#define DPMAC_IRQ_LINK_CHANGED	0x00000002 /* link state changed */
#define DPMAC_IRQ_LINK_UP_REQ	0x00000004 /* link up request */
#define DPMAC_IRQ_LINK_DOWN_REQ	0x00000008 /* link down request */
#define DPMAC_IRQ_EP_CHANGED	0x00000010 /* DPAA2 endpoint dis/connected */

/* DPAA2 MAC resource specification. */
struct resource_spec dpaa2_mac_spec[] = {
	/*
	 * DPMCP resources.
	 *
	 * NOTE: MC command portals (MCPs) are used to send commands to, and
	 *	 receive responses from, the MC firmware. One portal per DPMAC.
	 */
#define MCP_RES_NUM	(1u)
#define MCP_RID_OFF	(0u)
#define MCP_RID(rid)	((rid) + MCP_RID_OFF)
	/* --- */
	{ DPAA2_DEV_MCP, MCP_RID(0), RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	/* --- */
	RESOURCE_SPEC_END
};

/* Interrupt configuration routines. */
static int dpaa2_mac_setup_irq(device_t);
static int dpaa2_mac_setup_msi(struct dpaa2_mac_softc *);

/* Subroutines to get text representation. */
static const char *dpaa2_mac_ethif_to_str(enum dpaa2_mac_eth_if);
static const char *dpaa2_mac_link_type_to_str(enum dpaa2_mac_link_type);

/* Interrupt handlers */
static void dpaa2_mac_intr(void *arg);

static int
dpaa2_mac_probe(device_t dev)
{
	/* DPIO device will be added by a parent resource container itself. */
	device_set_desc(dev, "DPAA2 MAC");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_mac_attach(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	device_t mcp_dev;
	struct dpaa2_mac_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *mcp_dinfo;
	int error;

	sc->dev = dev;

	memset(sc->addr, 0, ETHER_ADDR_LEN);

	error = bus_alloc_resources(sc->dev, dpaa2_mac_spec, sc->res);
	if (error) {
		device_printf(dev, "%s: failed to allocate resources: "
		    "error=%d\n", __func__, error);
		return (ENXIO);
	}

	/* Obtain MC portal. */
	mcp_dev = (device_t) rman_get_start(sc->res[MCP_RID(0)]);
	mcp_dinfo = device_get_ivars(mcp_dev);
	dinfo->portal = mcp_dinfo->portal;

	/* Allocate a command to send to MC hardware. */
	error = dpaa2_mcp_init_command(&sc->cmd, DPAA2_CMD_DEF);
	if (error) {
		device_printf(dev, "Failed to allocate dpaa2_cmd: error=%d\n",
		    error);
		goto err_exit;
	}

	/* Open resource container and DPMAC object. */
	error = DPAA2_CMD_RC_OPEN(dev, child, sc->cmd, rcinfo->id,
	    &sc->rc_token);
	if (error) {
		device_printf(dev, "Failed to open DPRC: error=%d\n", error);
		goto err_free_cmd;
	}
	error = DPAA2_CMD_MAC_OPEN(dev, child, sc->cmd, dinfo->id,
	    &sc->mac_token);
	if (error) {
		device_printf(dev, "Failed to open DPMAC: id=%d, error=%d\n",
		    dinfo->id, error);
		goto err_close_rc;
	}

	error = DPAA2_CMD_MAC_GET_ATTRIBUTES(dev, child, sc->cmd, &sc->attr);
	if (error) {
		device_printf(dev, "Failed to get DPMAC attributes: id=%d, "
		    "error=%d\n", dinfo->id, error);
		goto err_close_mac;
	}
	error = DPAA2_CMD_MAC_GET_ADDR(dev, child, sc->cmd, sc->addr);
	if (error)
		device_printf(dev, "Failed to get physical address: error=%d\n",
		    error);
	/*
	 * TODO: Enable debug output via sysctl.
	 */
	if (bootverbose) {
		device_printf(dev, "ether %6D\n", sc->addr, ":");
		device_printf(dev, "max_rate=%d, eth_if=%s, link_type=%s\n",
		    sc->attr.max_rate,
		    dpaa2_mac_ethif_to_str(sc->attr.eth_if),
		    dpaa2_mac_link_type_to_str(sc->attr.link_type));
	}

	error = dpaa2_mac_setup_irq(dev);
	if (error) {
		device_printf(dev, "Failed to setup IRQs: error=%d\n", error);
		goto err_close_mac;
	}

	return (0);

err_close_mac:
	DPAA2_CMD_MAC_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->mac_token));
err_close_rc:
	DPAA2_CMD_RC_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->rc_token));
err_free_cmd:
	dpaa2_mcp_free_command(sc->cmd);
err_exit:
	return (ENXIO);
}

static int
dpaa2_mac_detach(device_t dev)
{
	device_t child = dev;
	struct dpaa2_mac_softc *sc = device_get_softc(dev);

	DPAA2_CMD_MAC_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->mac_token));
	DPAA2_CMD_RC_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->rc_token));
	dpaa2_mcp_free_command(sc->cmd);

	sc->cmd = NULL;
	sc->rc_token = 0;
	sc->mac_token = 0;

	return (0);
}

/**
 * @brief Configure DPMAC object to generate interrupts.
 */
static int
dpaa2_mac_setup_irq(device_t dev)
{
	device_t child = dev;
	struct dpaa2_mac_softc *sc = device_get_softc(dev);
	struct dpaa2_cmd *cmd = sc->cmd;
	uint16_t mac_token = sc->mac_token;
	uint32_t irq_mask;
	int error;

	/* Configure IRQs. */
	error = dpaa2_mac_setup_msi(sc);
	if (error) {
		device_printf(dev, "Failed to allocate MSI\n");
		return (error);
	}
	if ((sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->irq_rid[0], RF_ACTIVE | RF_SHAREABLE)) == NULL) {
		device_printf(dev, "Failed to allocate IRQ resource\n");
		return (ENXIO);
	}
	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, dpaa2_mac_intr, sc, &sc->intr)) {
		device_printf(dev, "Failed to setup IRQ resource\n");
		return (ENXIO);
	}

	/* Configure DPNI to generate interrupts. */
	irq_mask =
	    DPMAC_IRQ_LINK_CFG_REQ |
	    DPMAC_IRQ_LINK_CHANGED |
	    DPMAC_IRQ_LINK_UP_REQ |
	    DPMAC_IRQ_LINK_DOWN_REQ |
	    DPMAC_IRQ_EP_CHANGED;
	error = DPAA2_CMD_MAC_SET_IRQ_MASK(dev, child, dpaa2_mcp_tk(cmd,
	    mac_token), DPMAC_IRQ_INDEX, irq_mask);
	if (error) {
		device_printf(dev, "Failed to set IRQ mask\n");
		return (error);
	}

	/* Enable IRQ. */
	error = DPAA2_CMD_MAC_SET_IRQ_ENABLE(dev, child, cmd, DPMAC_IRQ_INDEX,
	    true);
	if (error) {
		device_printf(dev, "Failed to enable IRQ\n");
		return (error);
	}

	return (0);
}

/**
 * @brief Allocate MSI interrupts for DPMAC.
 */
static int
dpaa2_mac_setup_msi(struct dpaa2_mac_softc *sc)
{
	int val;

	val = pci_msi_count(sc->dev);
	if (val < DPAA2_MAC_MSI_COUNT)
		device_printf(sc->dev, "MSI: actual=%d, expected=%d\n", val,
		    DPAA2_MAC_MSI_COUNT);
	val = MIN(val, DPAA2_MAC_MSI_COUNT);

	if (pci_alloc_msi(sc->dev, &val) != 0)
		return (EINVAL);

	for (int i = 0; i < val; i++)
		sc->irq_rid[i] = i + 1;

	return (0);
}

static void
dpaa2_mac_intr(void *arg)
{
	struct dpaa2_mac_softc *sc = (struct dpaa2_mac_softc *) arg;
	device_t child = sc->dev;
	uint32_t status = ~0u; /* clear all IRQ status bits */
	int error;

	error = DPAA2_CMD_MAC_GET_IRQ_STATUS(sc->dev, child,
	    dpaa2_mcp_tk(sc->cmd, sc->mac_token), DPMAC_IRQ_INDEX, &status);
	if (error)
		device_printf(sc->dev, "%s: failed to obtain IRQ status: "
		    "error=%d\n", __func__, error);
}

static const char *
dpaa2_mac_ethif_to_str(enum dpaa2_mac_eth_if eth_if)
{
	switch (eth_if) {
	case DPAA2_MAC_ETH_IF_MII:
		return ("MII");
	case DPAA2_MAC_ETH_IF_RMII:
		return ("RMII");
	case DPAA2_MAC_ETH_IF_SMII:
		return ("SMII");
	case DPAA2_MAC_ETH_IF_GMII:
		return ("GMII");
	case DPAA2_MAC_ETH_IF_RGMII:
		return ("RGMII");
	case DPAA2_MAC_ETH_IF_SGMII:
		return ("SGMII");
	case DPAA2_MAC_ETH_IF_QSGMII:
		return ("QSGMII");
	case DPAA2_MAC_ETH_IF_XAUI:
		return ("XAUI");
	case DPAA2_MAC_ETH_IF_XFI:
		return ("XFI");
	case DPAA2_MAC_ETH_IF_CAUI:
		return ("CAUI");
	case DPAA2_MAC_ETH_IF_1000BASEX:
		return ("1000BASE-X");
	case DPAA2_MAC_ETH_IF_USXGMII:
		return ("USXGMII");
	default:
		return ("unknown");
	}
}

static const char *
dpaa2_mac_link_type_to_str(enum dpaa2_mac_link_type link_type)
{
	switch (link_type) {
	case DPAA2_MAC_LINK_TYPE_NONE:
		return ("NONE");
	case DPAA2_MAC_LINK_TYPE_FIXED:
		return ("FIXED");
	case DPAA2_MAC_LINK_TYPE_PHY:
		return ("PHY");
	case DPAA2_MAC_LINK_TYPE_BACKPLANE:
		return ("BACKPLANE");
	default:
		return ("unknown");
	}
}

static device_method_t dpaa2_mac_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_mac_probe),
	DEVMETHOD(device_attach,	dpaa2_mac_attach),
	DEVMETHOD(device_detach,	dpaa2_mac_detach),

	DEVMETHOD_END
};

static driver_t dpaa2_mac_driver = {
	"dpaa2_mac",
	dpaa2_mac_methods,
	sizeof(struct dpaa2_mac_softc),
};

DRIVER_MODULE(dpaa2_mac, dpaa2_rc, dpaa2_mac_driver, 0, 0);
