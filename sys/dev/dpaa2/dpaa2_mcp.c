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
 * DPAA2 MC command portal and helper routines.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/lock.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include "pcib_if.h"
#include "pci_if.h"

#include "dpaa2_mcp.h"
#include "dpaa2_mc.h"
#include "dpaa2_cmd_if.h"

MALLOC_DEFINE(M_DPAA2_MCP, "dpaa2_mcp", "DPAA2 Management Complex Portal");

static struct resource_spec dpaa2_mcp_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE | RF_UNMAPPED },
	RESOURCE_SPEC_END
};

int
dpaa2_mcp_init_portal(struct dpaa2_mcp **mcp, struct resource *res,
    struct resource_map *map, uint16_t flags)
{
	const int mflags = flags & DPAA2_PORTAL_NOWAIT_ALLOC
	    ? (M_NOWAIT | M_ZERO) : (M_WAITOK | M_ZERO);
	struct dpaa2_mcp *p;

	if (!mcp || !res || !map)
		return (DPAA2_CMD_STAT_EINVAL);

	p = malloc(sizeof(struct dpaa2_mcp), M_DPAA2_MCP, mflags);
	if (p == NULL)
		return (DPAA2_CMD_STAT_NO_MEMORY);

	mtx_init(&p->lock, "mcp_sleep_lock", NULL, MTX_DEF);

	p->res = res;
	p->map = map;
	p->flags = flags;
	p->rc_api_major = 0; /* DPRC API version to be cached later. */
	p->rc_api_minor = 0;

	*mcp = p;

	return (0);
}

void
dpaa2_mcp_free_portal(struct dpaa2_mcp *mcp)
{
	uint16_t flags;

	KASSERT(mcp != NULL, ("%s: mcp is NULL", __func__));

	DPAA2_MCP_LOCK(mcp, &flags);
	mcp->flags |= DPAA2_PORTAL_DESTROYED;
	DPAA2_MCP_UNLOCK(mcp);

	/* Let threads stop using this portal. */
	DELAY(DPAA2_PORTAL_TIMEOUT);

	mtx_destroy(&mcp->lock);
	free(mcp, M_DPAA2_MCP);
}

int
dpaa2_mcp_init_command(struct dpaa2_cmd **cmd, uint16_t flags)
{
	const int mflags = flags & DPAA2_CMD_NOWAIT_ALLOC
	    ? (M_NOWAIT | M_ZERO) : (M_WAITOK | M_ZERO);
	struct dpaa2_cmd *c;
	struct dpaa2_cmd_header *hdr;

	if (!cmd)
		return (DPAA2_CMD_STAT_EINVAL);

	c = malloc(sizeof(struct dpaa2_cmd), M_DPAA2_MCP, mflags);
	if (!c)
		return (DPAA2_CMD_STAT_NO_MEMORY);

	hdr = (struct dpaa2_cmd_header *) &c->header;
	hdr->srcid = 0;
	hdr->status = DPAA2_CMD_STAT_OK;
	hdr->token = 0;
	hdr->cmdid = 0;
	hdr->flags_hw = DPAA2_CMD_DEF;
	hdr->flags_sw = DPAA2_CMD_DEF;
	if (flags & DPAA2_CMD_HIGH_PRIO)
		hdr->flags_hw |= DPAA2_HW_FLAG_HIGH_PRIO;
	if (flags & DPAA2_CMD_INTR_DIS)
		hdr->flags_sw |= DPAA2_SW_FLAG_INTR_DIS;
	for (uint32_t i = 0; i < DPAA2_CMD_PARAMS_N; i++)
		c->params[i] = 0;
	*cmd = c;

	return (0);
}

void
dpaa2_mcp_free_command(struct dpaa2_cmd *cmd)
{
	if (cmd != NULL)
		free(cmd, M_DPAA2_MCP);
}

struct dpaa2_cmd *
dpaa2_mcp_tk(struct dpaa2_cmd *cmd, uint16_t token)
{
	struct dpaa2_cmd_header *hdr;
	if (cmd != NULL) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		hdr->token = token;
	}
	return (cmd);
}

struct dpaa2_cmd *
dpaa2_mcp_f(struct dpaa2_cmd *cmd, uint16_t flags)
{
	struct dpaa2_cmd_header *hdr;
	if (cmd) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		hdr->flags_hw = DPAA2_CMD_DEF;
		hdr->flags_sw = DPAA2_CMD_DEF;

		if (flags & DPAA2_CMD_HIGH_PRIO)
			hdr->flags_hw |= DPAA2_HW_FLAG_HIGH_PRIO;
		if (flags & DPAA2_CMD_INTR_DIS)
			hdr->flags_sw |= DPAA2_SW_FLAG_INTR_DIS;
	}
	return (cmd);
}

static int
dpaa2_mcp_probe(device_t dev)
{
	/* DPMCP device will be added by the parent resource container. */
	device_set_desc(dev, "DPAA2 MC portal");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_mcp_detach(device_t dev)
{
	return (0);
}

static int
dpaa2_mcp_attach(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_mcp_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_cmd *cmd;
	struct dpaa2_mcp *portal;
	struct resource_map_request req;
	uint16_t rc_token, mcp_token;
	int error;

	sc->dev = dev;

	error = bus_alloc_resources(sc->dev, dpaa2_mcp_spec, sc->res);
	if (error) {
		device_printf(dev, "%s: failed to allocate resources\n",
		    __func__);
		goto err_exit;
	}

	/* At least 64 bytes of the command portal should be available. */
	if (rman_get_size(sc->res[0]) < DPAA2_MCP_MEM_WIDTH) {
		device_printf(dev, "%s: MC portal memory region too small: "
		    "%jd\n", __func__, rman_get_size(sc->res[0]));
		goto err_exit;
	}

	/* Map MC portal memory resource. */
	resource_init_map_request(&req);
	req.memattr = VM_MEMATTR_DEVICE;
	error = bus_map_resource(sc->dev, SYS_RES_MEMORY, sc->res[0], &req,
	    &sc->map[0]);
	if (error) {
		device_printf(dev, "%s: failed to map MC portal memory\n",
		    __func__);
		goto err_exit;
	}

	/* Initialize portal to send commands to MC. */
	error = dpaa2_mcp_init_portal(&portal, sc->res[0], &sc->map[0],
	    DPAA2_PORTAL_DEF);
	if (error) {
		device_printf(dev, "%s: failed to initialize dpaa2_mcp: "
		    "error=%d\n", __func__, error);
		goto err_exit;
	}

	/* Allocate a command to send to MC hardware. */
	error = dpaa2_mcp_init_command(&cmd, DPAA2_CMD_DEF);
	if (error) {
		device_printf(dev, "%s: failed to allocate dpaa2_cmd: "
		    "error=%d\n", __func__, error);
		goto err_exit;
	}

	/* Open resource container and DPMCP object. */
	error = DPAA2_CMD_RC_OPEN(dev, child, cmd, rcinfo->id, &rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPRC: error=%d\n",
		    __func__, error);
		goto err_free_cmd;
	}
	error = DPAA2_CMD_MCP_OPEN(dev, child, cmd, dinfo->id, &mcp_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPMCP: id=%d, error=%d\n",
		    __func__, dinfo->id, error);
		goto err_close_rc;
	}

	/* Prepare DPMCP object. */
	error = DPAA2_CMD_MCP_RESET(dev, child, cmd);
	if (error) {
		device_printf(dev, "%s: failed to reset DPMCP: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto err_close_mcp;
	}

	/* Close the DPMCP object and the resource container. */
	error = DPAA2_CMD_MCP_CLOSE(dev, child, cmd);
	if (error) {
		device_printf(dev, "%s: failed to close DPMCP: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto err_close_rc;
	}
	error = DPAA2_CMD_RC_CLOSE(dev, child, dpaa2_mcp_tk(cmd, rc_token));
	if (error) {
		device_printf(dev, "%s: failed to close DPRC: error=%d\n",
		    __func__, error);
		goto err_free_cmd;
	}

	dpaa2_mcp_free_command(cmd);
	dinfo->portal = portal;

	return (0);

err_close_mcp:
	DPAA2_CMD_MCP_CLOSE(dev, child, dpaa2_mcp_tk(cmd, mcp_token));
err_close_rc:
	DPAA2_CMD_RC_CLOSE(dev, child, dpaa2_mcp_tk(cmd, rc_token));
err_free_cmd:
	dpaa2_mcp_free_command(cmd);
err_exit:
	dpaa2_mcp_detach(dev);
	return (ENXIO);
}

static device_method_t dpaa2_mcp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_mcp_probe),
	DEVMETHOD(device_attach,	dpaa2_mcp_attach),
	DEVMETHOD(device_detach,	dpaa2_mcp_detach),

	DEVMETHOD_END
};

static driver_t dpaa2_mcp_driver = {
	"dpaa2_mcp",
	dpaa2_mcp_methods,
	sizeof(struct dpaa2_mcp_softc),
};

DRIVER_MODULE(dpaa2_mcp, dpaa2_rc, dpaa2_mcp_driver, 0, 0);
