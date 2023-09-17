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
	/* TBD */
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
	struct dpaa2_cmd cmd;
	uint16_t rc_token, con_token;
	int error;

	sc->dev = dev;

	error = bus_alloc_resources(sc->dev, dpaa2_con_spec, sc->res);
	if (error) {
		device_printf(dev, "%s: failed to allocate resources: "
		    "error=%d\n", __func__, error);
		goto err_exit;
	}

	/* Obtain MC portal. */
	mcp_dev = (device_t) rman_get_start(sc->res[MCP_RID(0)]);
	mcp_dinfo = device_get_ivars(mcp_dev);
	dinfo->portal = mcp_dinfo->portal;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPRC: error=%d\n",
		    __func__, error);
		goto err_exit;
	}
	error = DPAA2_CMD_CON_OPEN(dev, child, &cmd, dinfo->id, &con_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPCON: id=%d, error=%d\n",
		    __func__, dinfo->id, error);
		goto close_rc;
	}

	error = DPAA2_CMD_CON_RESET(dev, child, &cmd);
	if (error) {
		device_printf(dev, "%s: failed to reset DPCON: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_con;
	}
	error = DPAA2_CMD_CON_GET_ATTRIBUTES(dev, child, &cmd, &sc->attr);
	if (error) {
		device_printf(dev, "%s: failed to get DPCON attributes: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_con;
	}

	if (bootverbose) {
		device_printf(dev, "chan_id=%d, priorities=%d\n",
		    sc->attr.chan_id, sc->attr.prior_num);
	}

	(void)DPAA2_CMD_CON_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, con_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_con:
	DPAA2_CMD_CON_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, con_token));
close_rc:
	DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
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
