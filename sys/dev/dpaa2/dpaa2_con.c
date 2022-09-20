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
 * The DPAA2 Concentrator (DPCON) driver.
 *
 * Supports configuration of QBMan channels for advanced scheduling of ingress
 * packets from one or more network interfaces.
 *
 * DPCONs are used to distribute Rx or Tx Confirmation traffic to different
 * cores, via affine DPIO objects. The implication is that one DPCON must be
 * available for each core where Rx or Tx Confirmation traffic should be
 * distributed to.
 *
 * QBMan channel contains several work queues. The WQs within a channel have a
 * priority relative to each other. Each channel consists of either eight or two
 * WQs, and thus, there are either eight or two possible priorities in a channel.
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

#include "dpaa2_mcp.h"
#include "dpaa2_swp.h"
#include "dpaa2_mc.h"
#include "dpaa2_cmd_if.h"

/* DPAA2 Concentrator resource specification. */
struct resource_spec dpaa2_con_spec[] = {
	/*
	 * DPMCP resources.
	 *
	 * NOTE: MC command portals (MCPs) are used to send commands to, and
	 *	 receive responses from, the MC firmware. One portal per DPCON.
	 */
#define MCP_RES_NUM	(1u)
#define MCP_RID_OFF	(0u)
#define MCP_RID(rid)	((rid) + MCP_RID_OFF)
	/* --- */
	{ DPAA2_DEV_MCP, MCP_RID(0), RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	/* --- */
	RESOURCE_SPEC_END
};

static int dpaa2_con_detach(device_t dev);

/*
 * Device interface.
 */

static int
dpaa2_con_probe(device_t dev)
{
	/* DPCON device will be added by a parent resource container itself. */
	device_set_desc(dev, "DPAA2 Concentrator");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_con_detach(device_t dev)
{
	device_t child = dev;
	struct dpaa2_con_softc *sc = device_get_softc(dev);

	DPAA2_CMD_CON_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->con_token));
	DPAA2_CMD_RC_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->rc_token));
	dpaa2_mcp_free_command(sc->cmd);

	sc->cmd = NULL;
	sc->con_token = 0;
	sc->rc_token = 0;

	return (0);
}

static int
dpaa2_con_attach(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	device_t mcp_dev;
	struct dpaa2_con_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *mcp_dinfo;
	int error;

	sc->dev = dev;

	error = bus_alloc_resources(sc->dev, dpaa2_con_spec, sc->res);
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

	/* Open resource container and DPCON object. */
	error = DPAA2_CMD_RC_OPEN(dev, child, sc->cmd, rcinfo->id,
	    &sc->rc_token);
	if (error) {
		device_printf(dev, "Failed to open DPRC: error=%d\n", error);
		goto err_free_cmd;
	}
	error = DPAA2_CMD_CON_OPEN(dev, child, sc->cmd, dinfo->id,
	    &sc->con_token);
	if (error) {
		device_printf(dev, "Failed to open DPCON: id=%d, error=%d\n",
		    dinfo->id, error);
		goto err_close_rc;
	}

	/* Prepare DPCON object. */
	error = DPAA2_CMD_CON_RESET(dev, child, sc->cmd);
	if (error) {
		device_printf(dev, "Failed to reset DPCON: id=%d, error=%d\n",
		    dinfo->id, error);
		goto err_close_con;
	}
	error = DPAA2_CMD_CON_GET_ATTRIBUTES(dev, child, sc->cmd, &sc->attr);
	if (error) {
		device_printf(dev, "Failed to get DPCON attributes: id=%d, "
		    "error=%d\n", dinfo->id, error);
		goto err_close_con;
	}

	/* TODO: Enable debug output via sysctl (to reduce output). */
	if (bootverbose)
		device_printf(dev, "chan_id=%d, priorities=%d\n",
		    sc->attr.chan_id, sc->attr.prior_num);

	return (0);

 err_close_con:
	DPAA2_CMD_CON_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->con_token));
 err_close_rc:
	DPAA2_CMD_RC_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->rc_token));
 err_free_cmd:
	dpaa2_mcp_free_command(sc->cmd);
 err_exit:
	return (ENXIO);
}

static device_method_t dpaa2_con_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_con_probe),
	DEVMETHOD(device_attach,	dpaa2_con_attach),
	DEVMETHOD(device_detach,	dpaa2_con_detach),

	DEVMETHOD_END
};

static driver_t dpaa2_con_driver = {
	"dpaa2_con",
	dpaa2_con_methods,
	sizeof(struct dpaa2_con_softc),
};

DRIVER_MODULE(dpaa2_con, dpaa2_rc, dpaa2_con_driver, 0, 0);
