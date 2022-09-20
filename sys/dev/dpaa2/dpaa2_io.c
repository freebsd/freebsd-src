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
 * QBMan command interface and the DPAA2 I/O (DPIO) driver.
 *
 * The DPIO object allows configuration of the QBMan software portal with
 * optional notification capabilities.
 *
 * Software portals are used by the driver to communicate with the QBMan. The
 * DPIO object’s main purpose is to enable the driver to perform I/O – enqueue
 * and dequeue operations, as well as buffer release and acquire operations –
 * using QBMan.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/_cpuset.h>
#include <sys/cpuset.h>
#include <sys/taskqueue.h>
#include <sys/smp.h>

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
#include "dpaa2_io.h"
#include "dpaa2_ni.h"

#define DPIO_IRQ_INDEX		0 /* index of the only DPIO IRQ */
#define DPIO_POLL_MAX		32

/*
 * Memory:
 *	0: cache-enabled part of the QBMan software portal.
 *	1: cache-inhibited part of the QBMan software portal.
 *	2: control registers of the QBMan software portal?
 *
 * Note that MSI should be allocated separately using pseudo-PCI interface.
 */
struct resource_spec dpaa2_io_spec[] = {
	/*
	 * System Memory resources.
	 */
#define MEM_RES_NUM	(3u)
#define MEM_RID_OFF	(0u)
#define MEM_RID(rid)	((rid) + MEM_RID_OFF)
	{ SYS_RES_MEMORY, MEM_RID(0),   RF_ACTIVE | RF_UNMAPPED },
	{ SYS_RES_MEMORY, MEM_RID(1),   RF_ACTIVE | RF_UNMAPPED },
	{ SYS_RES_MEMORY, MEM_RID(2),   RF_ACTIVE | RF_UNMAPPED | RF_OPTIONAL },
	/*
	 * DPMCP resources.
	 *
	 * NOTE: MC command portals (MCPs) are used to send commands to, and
	 *	 receive responses from, the MC firmware. One portal per DPIO.
	 */
#define MCP_RES_NUM	(1u)
#define MCP_RID_OFF	(MEM_RID_OFF + MEM_RES_NUM)
#define MCP_RID(rid)	((rid) + MCP_RID_OFF)
	/* --- */
	{ DPAA2_DEV_MCP,  MCP_RID(0),   RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL },
	/* --- */
	RESOURCE_SPEC_END
};

/* Configuration routines. */
static int dpaa2_io_setup_irqs(device_t dev);
static int dpaa2_io_release_irqs(device_t dev);
static int dpaa2_io_setup_msi(struct dpaa2_io_softc *sc);
static int dpaa2_io_release_msi(struct dpaa2_io_softc *sc);

/* Interrupt handlers */
static void dpaa2_io_intr(void *arg);

static int
dpaa2_io_probe(device_t dev)
{
	/* DPIO device will be added by a parent resource container itself. */
	device_set_desc(dev, "DPAA2 I/O");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_io_detach(device_t dev)
{
	device_t child = dev;
	struct dpaa2_io_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	int error;

	/* Tear down interrupt handler and release IRQ resources. */
	dpaa2_io_release_irqs(dev);

	/* Free software portal helper object. */
	dpaa2_swp_free_portal(sc->swp);

	/* Disable DPIO object. */
	error = DPAA2_CMD_IO_DISABLE(dev, child, dpaa2_mcp_tk(sc->cmd,
	    sc->io_token));
	if (error && bootverbose)
		device_printf(dev, "%s: failed to disable DPIO: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);

	/* Close control sessions with the DPAA2 objects. */
	DPAA2_CMD_IO_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->io_token));
	DPAA2_CMD_RC_CLOSE(dev, child, dpaa2_mcp_tk(sc->cmd, sc->rc_token));

	/* Free pre-allocated MC command. */
	dpaa2_mcp_free_command(sc->cmd);
	sc->cmd = NULL;
	sc->io_token = 0;
	sc->rc_token = 0;

	/* Unmap memory resources of the portal. */
	for (int i = 0; i < MEM_RES_NUM; i++) {
		if (sc->res[MEM_RID(i)] == NULL)
			continue;
		error = bus_unmap_resource(sc->dev, SYS_RES_MEMORY,
		    sc->res[MEM_RID(i)], &sc->map[MEM_RID(i)]);
		if (error && bootverbose)
			device_printf(dev, "%s: failed to unmap memory "
			    "resource: rid=%d, error=%d\n", __func__, MEM_RID(i),
			    error);
	}

	/* Release allocated resources. */
	bus_release_resources(dev, dpaa2_io_spec, sc->res);

	return (0);
}

static int
dpaa2_io_attach(device_t dev)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	device_t mcp_dev;
	struct dpaa2_io_softc *sc = device_get_softc(dev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *mcp_dinfo;
	struct resource_map_request req;
	struct {
		vm_memattr_t memattr;
		char *label;
	} map_args[MEM_RES_NUM] = {
		{ VM_MEMATTR_WRITE_BACK, "cache-enabled part" },
		{ VM_MEMATTR_DEVICE, "cache-inhibited part" },
		{ VM_MEMATTR_DEVICE, "control registers" }
	};
	int error;

	sc->dev = dev;
	sc->swp = NULL;
	sc->cmd = NULL;
	sc->intr = NULL;
	sc->irq_resource = NULL;

	/* Allocate resources. */
	error = bus_alloc_resources(sc->dev, dpaa2_io_spec, sc->res);
	if (error) {
		device_printf(dev, "%s: failed to allocate resources: "
		    "error=%d\n", __func__, error);
		return (ENXIO);
	}

	/* Set allocated MC portal up. */
	mcp_dev = (device_t) rman_get_start(sc->res[MCP_RID(0)]);
	mcp_dinfo = device_get_ivars(mcp_dev);
	dinfo->portal = mcp_dinfo->portal;

	/* Map memory resources of the portal. */
	for (int i = 0; i < MEM_RES_NUM; i++) {
		if (sc->res[MEM_RID(i)] == NULL)
			continue;

		resource_init_map_request(&req);
		req.memattr = map_args[i].memattr;
		error = bus_map_resource(sc->dev, SYS_RES_MEMORY,
		    sc->res[MEM_RID(i)], &req, &sc->map[MEM_RID(i)]);
		if (error) {
			device_printf(dev, "%s: failed to map %s: error=%d\n",
			    __func__, map_args[i].label, error);
			goto err_exit;
		}
	}

	/* Allocate a command to send to the MC hardware. */
	error = dpaa2_mcp_init_command(&sc->cmd, DPAA2_CMD_DEF);
	if (error) {
		device_printf(dev, "%s: failed to allocate dpaa2_cmd: "
		    "error=%d\n", __func__, error);
		goto err_exit;
	}

	/* Prepare DPIO object. */
	error = DPAA2_CMD_RC_OPEN(dev, child, sc->cmd, rcinfo->id,
	    &sc->rc_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPRC: error=%d\n",
		    __func__, error);
		goto err_exit;
	}
	error = DPAA2_CMD_IO_OPEN(dev, child, sc->cmd, dinfo->id, &sc->io_token);
	if (error) {
		device_printf(dev, "%s: failed to open DPIO: id=%d, error=%d\n",
		    __func__, dinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_IO_RESET(dev, child, sc->cmd);
	if (error) {
		device_printf(dev, "%s: failed to reset DPIO: id=%d, error=%d\n",
		    __func__, dinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_IO_GET_ATTRIBUTES(dev, child, sc->cmd, &sc->attr);
	if (error) {
		device_printf(dev, "%s: failed to get DPIO attributes: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto err_exit;
	}
	error = DPAA2_CMD_IO_ENABLE(dev, child, sc->cmd);
	if (error) {
		device_printf(dev, "%s: failed to enable DPIO: id=%d, "
		    "error=%d\n", __func__, dinfo->id, error);
		goto err_exit;
	}

	/* Prepare descriptor of the QBMan software portal. */
	sc->swp_desc.dpio_dev = dev;
	sc->swp_desc.swp_version = sc->attr.swp_version;
	sc->swp_desc.swp_clk = sc->attr.swp_clk;
	sc->swp_desc.swp_id = sc->attr.swp_id;
	sc->swp_desc.has_notif = sc->attr.priors_num ? true : false;
	sc->swp_desc.has_8prio = sc->attr.priors_num == 8u ? true : false;

	sc->swp_desc.cena_res = sc->res[0];
	sc->swp_desc.cena_map = &sc->map[0];
	sc->swp_desc.cinh_res = sc->res[1];
	sc->swp_desc.cinh_map = &sc->map[1];

	/*
	 * Compute how many 256 QBMAN cycles fit into one ns. This is because
	 * the interrupt timeout period register needs to be specified in QBMAN
	 * clock cycles in increments of 256.
	 */
	sc->swp_desc.swp_cycles_ratio = 256000 /
	    (sc->swp_desc.swp_clk / 1000000);

	/* Initialize QBMan software portal. */
	error = dpaa2_swp_init_portal(&sc->swp, &sc->swp_desc, DPAA2_SWP_DEF);
	if (error) {
		device_printf(dev, "%s: failed to initialize dpaa2_swp: "
		    "error=%d\n", __func__, error);
		goto err_exit;
	}

	error = dpaa2_io_setup_irqs(dev);
	if (error) {
		device_printf(dev, "%s: failed to setup IRQs: error=%d\n",
		    __func__, error);
		goto err_exit;
	}

#if 0
	/* TODO: Enable debug output via sysctl (to reduce output). */
	if (bootverbose)
		device_printf(dev, "dpio_id=%d, swp_id=%d, chan_mode=%s, "
		    "notif_priors=%d, swp_version=0x%x\n",
		    sc->attr.id, sc->attr.swp_id,
		    sc->attr.chan_mode == DPAA2_IO_LOCAL_CHANNEL
		    ? "local_channel" : "no_channel", sc->attr.priors_num,
		    sc->attr.swp_version);
#endif
	return (0);

err_exit:
	dpaa2_io_detach(dev);
	return (ENXIO);
}

/**
 * @brief Enqueue multiple frames to a frame queue using one FQID.
 */
static int
dpaa2_io_enq_multiple_fq(device_t iodev, uint32_t fqid,
    struct dpaa2_fd *fd, int frames_n)
{
	struct dpaa2_io_softc *sc = device_get_softc(iodev);
	struct dpaa2_swp *swp = sc->swp;
	struct dpaa2_eq_desc ed;
	uint32_t flags = 0;

	memset(&ed, 0, sizeof(ed));

	/* Setup enqueue descriptor. */
	dpaa2_swp_set_ed_norp(&ed, false);
	dpaa2_swp_set_ed_fq(&ed, fqid);

	return (dpaa2_swp_enq_mult(swp, &ed, fd, &flags, frames_n));
}

/**
 * @brief Configure the channel data availability notification (CDAN)
 * in a particular WQ channel paired with DPIO.
 */
static int
dpaa2_io_conf_wq_channel(device_t iodev, struct dpaa2_io_notif_ctx *ctx)
{
	struct dpaa2_io_softc *sc = device_get_softc(iodev);

	/* Enable generation of the CDAN notifications. */
	if (ctx->cdan_en)
		return (dpaa2_swp_conf_wq_channel(sc->swp, ctx->fq_chan_id,
		    DPAA2_WQCHAN_WE_EN | DPAA2_WQCHAN_WE_CTX, ctx->cdan_en,
		    ctx->qman_ctx));

	return (0);
}

/**
 * @brief Query current configuration/state of the buffer pool.
 */
static int
dpaa2_io_query_bp(device_t iodev, uint16_t bpid, struct dpaa2_bp_conf *conf)
{
	struct dpaa2_io_softc *sc = device_get_softc(iodev);

	return (dpaa2_swp_query_bp(sc->swp, bpid, conf));
}

/**
 * @brief Release one or more buffer pointers to the QBMan buffer pool.
 */
static int
dpaa2_io_release_bufs(device_t iodev, uint16_t bpid, bus_addr_t *buf,
    uint32_t buf_num)
{
	struct dpaa2_io_softc *sc = device_get_softc(iodev);

	return (dpaa2_swp_release_bufs(sc->swp, bpid, buf, buf_num));
}

/**
 * @brief Configure DPNI object to generate interrupts.
 */
static int
dpaa2_io_setup_irqs(device_t dev)
{
	struct dpaa2_io_softc *sc = device_get_softc(dev);
	int error;

	/*
	 * Setup interrupts generated by the software portal.
	 */
	dpaa2_swp_set_intr_trigger(sc->swp, DPAA2_SWP_INTR_DQRI);
	dpaa2_swp_clear_intr_status(sc->swp, 0xFFFFFFFFu);

	/* Configure IRQs. */
	error = dpaa2_io_setup_msi(sc);
	if (error) {
		device_printf(dev, "%s: failed to allocate MSI: error=%d\n",
		    __func__, error);
		return (error);
	}
	if ((sc->irq_resource = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->irq_rid[0], RF_ACTIVE | RF_SHAREABLE)) == NULL) {
		device_printf(dev, "%s: failed to allocate IRQ resource\n",
		    __func__);
		return (ENXIO);
	}
	if (bus_setup_intr(dev, sc->irq_resource, INTR_TYPE_NET | INTR_MPSAFE |
	    INTR_ENTROPY, NULL, dpaa2_io_intr, sc, &sc->intr)) {
		device_printf(dev, "%s: failed to setup IRQ resource\n",
		    __func__);
		return (ENXIO);
	}

	/* Wrap DPIO ID around number of CPUs. */
	bus_bind_intr(dev, sc->irq_resource, sc->attr.id % mp_ncpus);

	/*
	 * Setup and enable Static Dequeue Command to receive CDANs from
	 * channel 0.
	 */
	if (sc->swp_desc.has_notif)
		dpaa2_swp_set_push_dequeue(sc->swp, 0, true);

	return (0);
}

static int
dpaa2_io_release_irqs(device_t dev)
{
	struct dpaa2_io_softc *sc = device_get_softc(dev);

	/* Disable receiving CDANs from channel 0. */
	if (sc->swp_desc.has_notif)
		dpaa2_swp_set_push_dequeue(sc->swp, 0, false);

	/* Release IRQ resources. */
	if (sc->intr != NULL)
		bus_teardown_intr(dev, sc->irq_resource, &sc->intr);
	if (sc->irq_resource != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid[0],
		    sc->irq_resource);

	(void)dpaa2_io_release_msi(device_get_softc(dev));

	/* Configure software portal to stop generating interrupts. */
	dpaa2_swp_set_intr_trigger(sc->swp, 0);
	dpaa2_swp_clear_intr_status(sc->swp, 0xFFFFFFFFu);

	return (0);
}

/**
 * @brief Allocate MSI interrupts for this DPAA2 I/O object.
 */
static int
dpaa2_io_setup_msi(struct dpaa2_io_softc *sc)
{
	int val;

	val = pci_msi_count(sc->dev);
	if (val < DPAA2_IO_MSI_COUNT)
		device_printf(sc->dev, "MSI: actual=%d, expected=%d\n", val,
		    DPAA2_IO_MSI_COUNT);
	val = MIN(val, DPAA2_IO_MSI_COUNT);

	if (pci_alloc_msi(sc->dev, &val) != 0)
		return (EINVAL);

	for (int i = 0; i < val; i++)
		sc->irq_rid[i] = i + 1;

	return (0);
}

static int
dpaa2_io_release_msi(struct dpaa2_io_softc *sc)
{
	int error;

	error = pci_release_msi(sc->dev);
	if (error) {
		device_printf(sc->dev, "%s: failed to release MSI: error=%d/n",
		    __func__, error);
		return (error);
	}

	return (0);
}

/**
 * @brief DPAA2 I/O interrupt handler.
 */
static void
dpaa2_io_intr(void *arg)
{
	struct dpaa2_io_softc *sc = (struct dpaa2_io_softc *) arg;
	struct dpaa2_io_notif_ctx *ctx[DPIO_POLL_MAX];
	struct dpaa2_dq dq;
	uint32_t idx, status;
	uint16_t flags;
	int rc, cdan_n = 0;

	status = dpaa2_swp_read_intr_status(sc->swp);
	if (status == 0) {
		return;
	}

	DPAA2_SWP_LOCK(sc->swp, &flags);
	if (flags & DPAA2_SWP_DESTROYED) {
		/* Terminate operation if portal is destroyed. */
		DPAA2_SWP_UNLOCK(sc->swp);
		return;
	}

	for (int i = 0; i < DPIO_POLL_MAX; i++) {
		rc = dpaa2_swp_dqrr_next_locked(sc->swp, &dq, &idx);
		if (rc) {
			break;
		}

		if ((dq.common.verb & DPAA2_DQRR_RESULT_MASK) ==
		    DPAA2_DQRR_RESULT_CDAN) {
			ctx[cdan_n++] = (struct dpaa2_io_notif_ctx *) dq.scn.ctx;
		} else {
			/* TODO: Report unknown DQRR entry. */
		}
		dpaa2_swp_write_reg(sc->swp, DPAA2_SWP_CINH_DCAP, idx);
	}
	DPAA2_SWP_UNLOCK(sc->swp);

	for (int i = 0; i < cdan_n; i++) {
		ctx[i]->poll(ctx[i]->channel);
	}

	/* Enable software portal interrupts back */
	dpaa2_swp_clear_intr_status(sc->swp, status);
	dpaa2_swp_write_reg(sc->swp, DPAA2_SWP_CINH_IIR, 0);
}

static device_method_t dpaa2_io_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_io_probe),
	DEVMETHOD(device_attach,	dpaa2_io_attach),
	DEVMETHOD(device_detach,	dpaa2_io_detach),

	/* QBMan software portal interface */
	DEVMETHOD(dpaa2_swp_enq_multiple_fq,	dpaa2_io_enq_multiple_fq),
	DEVMETHOD(dpaa2_swp_conf_wq_channel,	dpaa2_io_conf_wq_channel),
	DEVMETHOD(dpaa2_swp_query_bp,		dpaa2_io_query_bp),
	DEVMETHOD(dpaa2_swp_release_bufs,	dpaa2_io_release_bufs),

	DEVMETHOD_END
};

static driver_t dpaa2_io_driver = {
	"dpaa2_io",
	dpaa2_io_methods,
	sizeof(struct dpaa2_io_softc),
};

DRIVER_MODULE(dpaa2_io, dpaa2_rc, dpaa2_io_driver, 0, 0);
