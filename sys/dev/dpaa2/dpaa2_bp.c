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
 * The DPAA2 Buffer Pool (DPBP) driver.
 *
 * The DPBP configures a buffer pool that can be associated with DPAA2 network
 * and accelerator interfaces.
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
#include "dpaa2_mcp.h"
#include "dpaa2_swp.h"
#include "dpaa2_swp_if.h"
#include "dpaa2_cmd_if.h"

/* DPAA2 Buffer Pool resource specification. */
struct resource_spec dpaa2_bp_spec[] = {
	/*
	 * DPMCP resources.
	 *
	 * NOTE: MC command portals (MCPs) are used to send commands to, and
	 *	 receive responses from, the MC firmware. One portal per DPBP.
	 */
#define MCP_RES_NUM	(1u)
#define MCP_RID_OFF	(0u)
#define MCP_RID(rid)	((rid) + MCP_RID_OFF)
	/* --- */
	{ DPAA2_DEV_MCP, MCP_RID(0), RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	/* --- */
	RESOURCE_SPEC_END
};

static int
dpaa2_bp_probe(device_t dev)
{
	/* DPBP device will be added by the parent resource container. */
	device_set_desc(dev, "DPAA2 Buffer Pool");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_bp_detach(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_bp_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd cmd;
	uint16_t rc_token, bp_token;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPRC: error=%d\n",
		    __func__, error);
		goto err_exit;
	}
	error = DPAA2_CMD_BP_OPEN(dev, child, &cmd, dinfo->id, &bp_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPBP: id=%d, error=%d\n",
		    __func__, dinfo->id, error);
		goto close_rc;
	}
	(void)DPAA2_CMD_BP_DISABLE(dev, child, &cmd);
	(void)DPAA2_CMD_BP_CLOSE(dev, child, &cmd);
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));

	dinfo->portal = NULL;
	bus_release_resources(sc->dev, dpaa2_bp_spec, sc->res);

	return (0);

close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
err_exit:
	return (ENXIO);
}

static int
dpaa2_bp_attach(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	device_t mcp_dev;
	struct dpaa2_bp_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *mcp_dinfo;
	struct dpaa2_cmd cmd;
	uint16_t rc_token, bp_token;
	int error;

	sc->dev = dev;

	error = bus_alloc_resources(sc->dev, dpaa2_bp_spec, sc->res);
	if (error) {
		device_printf(dev, "%s: failed to allocate resources: "
		    "error=%d\n", __func__, error);
		goto err_exit;
	}

	/* Send commands to MC via allocated portal. */
	mcp_dev = (device_t) rman_get_start(sc->res[MCP_RID(0)]);
	mcp_dinfo = device_get_ivars(mcp_dev);
	dinfo->portal = mcp_dinfo->portal;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPRC: error=%d\n",
		    __func__, error);
		goto detach;
	}
	error = DPAA2_CMD_BP_OPEN(dev, child, &cmd, dinfo->id, &bp_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPBP: id=%d, error=%d\n",
		    __func__, dinfo->id, error);
		goto close_rc;
	}

	error = DPAA2_CMD_BP_RESET(dev, child, &cmd);
	if (error) {
		device_printf(dev, "%s: failed to reset DPBP: id=%d, error=%d\n",
		    __func__, dinfo->id, error);
		goto close_bp;
	}
	error = DPAA2_CMD_BP_ENABLE(dev, child, &cmd);
	if (error) {
		device_printf(dev, "%s: failed to enable DPBP: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_bp;
	}
	error = DPAA2_CMD_BP_GET_ATTRIBUTES(dev, child, &cmd, &sc->attr);
	if (error) {
		device_printf(dev, "%s: failed to get DPBP attributes: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto close_bp;
	}

	(void)DPAA2_CMD_BP_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, bp_token));
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
	return (0);

close_bp:
	(void)DPAA2_CMD_BP_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, bp_token));
close_rc:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rc_token));
detach:
	dpaa2_bp_detach(dev);
err_exit:
	return (ENXIO);
}

static device_method_t dpaa2_bp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_bp_probe),
	DEVMETHOD(device_attach,	dpaa2_bp_attach),
	DEVMETHOD(device_detach,	dpaa2_bp_detach),

	DEVMETHOD_END
};

static driver_t dpaa2_bp_driver = {
	"dpaa2_bp",
	dpaa2_bp_methods,
	sizeof(struct dpaa2_bp_softc),
};

DRIVER_MODULE(dpaa2_bp, dpaa2_rc, dpaa2_bp_driver, 0, 0);
