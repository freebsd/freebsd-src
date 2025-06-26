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
 * The DPAA2 Resource Container (DPRC) bus driver.
 *
 * DPRC holds all the resources and object information that a software context
 * (kernel, virtual machine, etc.) can access or use.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include "pcib_if.h"
#include "pci_if.h"

#include "dpaa2_mcp.h"
#include "dpaa2_mc.h"
#include "dpaa2_ni.h"
#include "dpaa2_mc_if.h"
#include "dpaa2_cmd_if.h"

/* Timeouts to wait for a command response from MC. */
#define CMD_SPIN_TIMEOUT	100u	/* us */
#define CMD_SPIN_ATTEMPTS	2000u	/* max. 200 ms */

#define TYPE_LEN_MAX		16u
#define LABEL_LEN_MAX		16u

MALLOC_DEFINE(M_DPAA2_RC, "dpaa2_rc", "DPAA2 Resource Container");

/* Discover and add devices to the resource container. */
static int dpaa2_rc_discover(struct dpaa2_rc_softc *);
static int dpaa2_rc_add_child(struct dpaa2_rc_softc *, struct dpaa2_cmd *,
    struct dpaa2_obj *);
static int dpaa2_rc_add_managed_child(struct dpaa2_rc_softc *,
    struct dpaa2_cmd *, struct dpaa2_obj *);

/* Helper routines. */
static int dpaa2_rc_enable_irq(struct dpaa2_mcp *, struct dpaa2_cmd *, uint8_t,
    bool, uint16_t);
static int dpaa2_rc_configure_irq(device_t, device_t, int, uint64_t, uint32_t);
static int dpaa2_rc_add_res(device_t, device_t, enum dpaa2_dev_type, int *, int);
static int dpaa2_rc_print_type(struct resource_list *, enum dpaa2_dev_type);
static struct dpaa2_mcp *dpaa2_rc_select_portal(device_t, device_t);

/* Routines to send commands to MC. */
static int dpaa2_rc_exec_cmd(struct dpaa2_mcp *, struct dpaa2_cmd *, uint16_t);
static int dpaa2_rc_send_cmd(struct dpaa2_mcp *, struct dpaa2_cmd *);
static int dpaa2_rc_wait_for_cmd(struct dpaa2_mcp *, struct dpaa2_cmd *);
static int dpaa2_rc_reset_cmd_params(struct dpaa2_cmd *);

static int
dpaa2_rc_probe(device_t dev)
{
	/* DPRC device will be added by the parent DPRC or MC bus itself. */
	device_set_desc(dev, "DPAA2 Resource Container");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_rc_detach(device_t dev)
{
	struct dpaa2_devinfo *dinfo;
	int error;

	error = bus_generic_detach(dev);
	if (error)
		return (error);

	dinfo = device_get_ivars(dev);

	if (dinfo->portal)
		dpaa2_mcp_free_portal(dinfo->portal);
	if (dinfo)
		free(dinfo, M_DPAA2_RC);

	return (0);
}

static int
dpaa2_rc_attach(device_t dev)
{
	device_t pdev;
	struct dpaa2_mc_softc *mcsc;
	struct dpaa2_rc_softc *sc;
	struct dpaa2_devinfo *dinfo = NULL;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->unit = device_get_unit(dev);

	if (sc->unit == 0) {
		/* Root DPRC should be attached directly to the MC bus. */
		pdev = device_get_parent(dev);
		mcsc = device_get_softc(pdev);

		KASSERT(strcmp(device_get_name(pdev), "dpaa2_mc") == 0,
		    ("root DPRC should be attached to the MC bus"));

		/*
		 * Allocate devinfo to let the parent MC bus access ICID of the
		 * DPRC object.
		 */
		dinfo = malloc(sizeof(struct dpaa2_devinfo), M_DPAA2_RC,
		    M_WAITOK | M_ZERO);
		if (!dinfo) {
			device_printf(dev, "%s: failed to allocate "
			    "dpaa2_devinfo\n", __func__);
			dpaa2_rc_detach(dev);
			return (ENXIO);
		}
		device_set_ivars(dev, dinfo);

		dinfo->pdev = pdev;
		dinfo->dev = dev;
		dinfo->dtype = DPAA2_DEV_RC;
		dinfo->portal = NULL;

		/* Prepare helper portal object to send commands to MC. */
		error = dpaa2_mcp_init_portal(&dinfo->portal, mcsc->res[0],
		    &mcsc->map[0], DPAA2_PORTAL_DEF);
		if (error) {
			device_printf(dev, "%s: failed to initialize dpaa2_mcp: "
			    "error=%d\n", __func__, error);
			dpaa2_rc_detach(dev);
			return (ENXIO);
		}
	} else {
		/* TODO: Child DPRCs aren't supported yet. */
		return (ENXIO);
	}

	/* Create DPAA2 devices for objects in this container. */
	error = dpaa2_rc_discover(sc);
	if (error) {
		device_printf(dev, "%s: failed to discover objects in "
		    "container: error=%d\n", __func__, error);
		dpaa2_rc_detach(dev);
		return (error);
	}

	return (0);
}

/*
 * Bus interface.
 */

static struct resource_list *
dpaa2_rc_get_resource_list(device_t rcdev, device_t child)
{
	struct dpaa2_devinfo *dinfo = device_get_ivars(child);

	return (&dinfo->resources);
}

static void
dpaa2_rc_delete_resource(device_t rcdev, device_t child, int type, int rid)
{
	struct resource_list *rl;
	struct resource_list_entry *rle;
	struct dpaa2_devinfo *dinfo;

	if (device_get_parent(child) != rcdev)
		return;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;
	rle = resource_list_find(rl, type, rid);
	if (rle == NULL)
		return;

	if (rle->res) {
		if (rman_get_flags(rle->res) & RF_ACTIVE ||
		    resource_list_busy(rl, type, rid)) {
			device_printf(rcdev, "%s: resource still owned by "
			    "child: type=%d, rid=%d, start=%jx\n", __func__,
			    type, rid, rman_get_start(rle->res));
			return;
		}
		resource_list_unreserve(rl, rcdev, child, type, rid);
	}
	resource_list_delete(rl, type, rid);
}

static struct resource *
dpaa2_rc_alloc_multi_resource(device_t rcdev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource_list *rl;
	struct dpaa2_devinfo *dinfo;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;

	/*
	 * By default, software portal interrupts are message-based, that is,
	 * they are issued from QMan using a 4 byte write.
	 *
	 * TODO: However this default behavior can be changed by programming one
	 *	 or more software portals to issue their interrupts via a
	 *	 dedicated software portal interrupt wire.
	 *	 See registers SWP_INTW0_CFG to SWP_INTW3_CFG for details.
	 */
	if (type == SYS_RES_IRQ && *rid == 0)
		return (NULL);

	return (resource_list_alloc(rl, rcdev, child, type, rid,
	    start, end, count, flags));
}

static struct resource *
dpaa2_rc_alloc_resource(device_t rcdev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	if (device_get_parent(child) != rcdev)
		return (BUS_ALLOC_RESOURCE(device_get_parent(rcdev), child,
		    type, rid, start, end, count, flags));

	return (dpaa2_rc_alloc_multi_resource(rcdev, child, type, rid, start,
	    end, count, flags));
}

static int
dpaa2_rc_release_resource(device_t rcdev, device_t child, struct resource *r)
{
	struct resource_list *rl;
	struct dpaa2_devinfo *dinfo;

	if (device_get_parent(child) != rcdev)
		return (BUS_RELEASE_RESOURCE(device_get_parent(rcdev), child,
		    r));

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;
	return (resource_list_release(rl, rcdev, child, r));
}

static void
dpaa2_rc_child_deleted(device_t rcdev, device_t child)
{
	struct dpaa2_devinfo *dinfo;
	struct resource_list *rl;
	struct resource_list_entry *rle;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;

	/* Free all allocated resources */
	STAILQ_FOREACH(rle, rl, link) {
		if (rle->res) {
			if (rman_get_flags(rle->res) & RF_ACTIVE ||
			    resource_list_busy(rl, rle->type, rle->rid)) {
				device_printf(child, "%s: resource still owned: "
				    "type=%d, rid=%d, addr=%lx\n", __func__,
				    rle->type, rle->rid,
				    rman_get_start(rle->res));
				bus_release_resource(child, rle->type, rle->rid,
				    rle->res);
			}
			resource_list_unreserve(rl, rcdev, child, rle->type,
			    rle->rid);
		}
	}
	resource_list_free(rl);

	if (dinfo)
		free(dinfo, M_DPAA2_RC);
}

static void
dpaa2_rc_child_detached(device_t rcdev, device_t child)
{
	struct dpaa2_devinfo *dinfo;
	struct resource_list *rl;

	dinfo = device_get_ivars(child);
	rl = &dinfo->resources;

	if (resource_list_release_active(rl, rcdev, child, SYS_RES_IRQ) != 0)
		device_printf(child, "%s: leaked IRQ resources!\n", __func__);
	if (dinfo->msi.msi_alloc != 0) {
		device_printf(child, "%s: leaked %d MSI vectors!\n", __func__,
		    dinfo->msi.msi_alloc);
		PCI_RELEASE_MSI(rcdev, child);
	}
	if (resource_list_release_active(rl, rcdev, child, SYS_RES_MEMORY) != 0)
		device_printf(child, "%s: leaked memory resources!\n", __func__);
}

static int
dpaa2_rc_setup_intr(device_t rcdev, device_t child, struct resource *irq,
    int flags, driver_filter_t *filter, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	struct dpaa2_devinfo *dinfo;
	uint64_t addr;
	uint32_t data;
	void *cookie;
	int error, rid;

	error = bus_generic_setup_intr(rcdev, child, irq, flags, filter, intr,
	    arg, &cookie);
	if (error) {
		device_printf(rcdev, "%s: bus_generic_setup_intr() failed: "
		    "error=%d\n", __func__, error);
		return (error);
	}

	/* If this is not a direct child, just bail out. */
	if (device_get_parent(child) != rcdev) {
		*cookiep = cookie;
		return (0);
	}

	rid = rman_get_rid(irq);
	if (rid == 0) {
		if (bootverbose)
			device_printf(rcdev, "%s: cannot setup interrupt with "
			    "rid=0: INTx are not supported by DPAA2 objects "
			    "yet\n", __func__);
		return (EINVAL);
	} else {
		dinfo = device_get_ivars(child);
		KASSERT(dinfo->msi.msi_alloc > 0,
		    ("No MSI interrupts allocated"));

		/*
		 * Ask our parent to map the MSI and give us the address and
		 * data register values. If we fail for some reason, teardown
		 * the interrupt handler.
		 */
		error = PCIB_MAP_MSI(device_get_parent(rcdev), child,
		    rman_get_start(irq), &addr, &data);
		if (error) {
			device_printf(rcdev, "%s: PCIB_MAP_MSI failed: "
			    "error=%d\n", __func__, error);
			(void)bus_generic_teardown_intr(rcdev, child, irq,
			    cookie);
			return (error);
		}

		/* Configure MSI for this DPAA2 object. */
		error = dpaa2_rc_configure_irq(rcdev, child, rid, addr, data);
		if (error) {
			device_printf(rcdev, "%s: failed to configure IRQ for "
			    "DPAA2 object: rid=%d, type=%s, unit=%d\n", __func__,
			    rid, dpaa2_ttos(dinfo->dtype),
			    device_get_unit(child));
			return (error);
		}
		dinfo->msi.msi_handlers++;
	}
	*cookiep = cookie;
	return (0);
}

static int
dpaa2_rc_teardown_intr(device_t rcdev, device_t child, struct resource *irq,
    void *cookie)
{
	struct resource_list_entry *rle;
	struct dpaa2_devinfo *dinfo;
	int error, rid;

	if (irq == NULL || !(rman_get_flags(irq) & RF_ACTIVE))
		return (EINVAL);

	/* If this isn't a direct child, just bail out */
	if (device_get_parent(child) != rcdev)
		return(bus_generic_teardown_intr(rcdev, child, irq, cookie));

	rid = rman_get_rid(irq);
	if (rid == 0) {
		if (bootverbose)
			device_printf(rcdev, "%s: cannot teardown interrupt "
			    "with rid=0: INTx are not supported by DPAA2 "
			    "objects yet\n", __func__);
		return (EINVAL);
	} else {
		dinfo = device_get_ivars(child);
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, rid);
		if (rle->res != irq)
			return (EINVAL);
		dinfo->msi.msi_handlers--;
	}

	error = bus_generic_teardown_intr(rcdev, child, irq, cookie);
	if (rid > 0)
		KASSERT(error == 0,
		    ("%s: generic teardown failed for MSI", __func__));
	return (error);
}

static int
dpaa2_rc_print_child(device_t rcdev, device_t child)
{
	struct dpaa2_devinfo *dinfo = device_get_ivars(child);
	struct resource_list *rl = &dinfo->resources;
	int retval = 0;

	retval += bus_print_child_header(rcdev, child);

	retval += resource_list_print_type(rl, "port", SYS_RES_IOPORT, "%#jx");
	retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#jx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%jd");

	/* Print DPAA2-specific resources. */
	retval += dpaa2_rc_print_type(rl, DPAA2_DEV_IO);
	retval += dpaa2_rc_print_type(rl, DPAA2_DEV_BP);
	retval += dpaa2_rc_print_type(rl, DPAA2_DEV_CON);
	retval += dpaa2_rc_print_type(rl, DPAA2_DEV_MCP);

	retval += printf(" at %s (id=%u)", dpaa2_ttos(dinfo->dtype), dinfo->id);

	retval += bus_print_child_domain(rcdev, child);
	retval += bus_print_child_footer(rcdev, child);

	return (retval);
}

/*
 * Pseudo-PCI interface.
 */

/*
 * Attempt to allocate *count MSI messages. The actual number allocated is
 * returned in *count. After this function returns, each message will be
 * available to the driver as SYS_RES_IRQ resources starting at a rid 1.
 *
 * NOTE: Implementation is similar to sys/dev/pci/pci.c.
 */
static int
dpaa2_rc_alloc_msi(device_t rcdev, device_t child, int *count)
{
	struct dpaa2_devinfo *rcinfo = device_get_ivars(rcdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(child);
	int error, actual, i, run, irqs[32];

	/* Don't let count == 0 get us into trouble. */
	if (*count == 0)
		return (EINVAL);

	/* MSI should be allocated by the resource container. */
	if (rcinfo->dtype != DPAA2_DEV_RC)
		return (ENODEV);

	/* Already have allocated messages? */
	if (dinfo->msi.msi_alloc != 0)
		return (ENXIO);

	/* Don't ask for more than the device supports. */
	actual = min(*count, dinfo->msi.msi_msgnum);

	/* Don't ask for more than 32 messages. */
	actual = min(actual, 32);

	/* MSI requires power of 2 number of messages. */
	if (!powerof2(actual))
		return (EINVAL);

	for (;;) {
		/* Try to allocate N messages. */
		error = PCIB_ALLOC_MSI(device_get_parent(rcdev), child, actual,
		    actual, irqs);
		if (error == 0)
			break;
		if (actual == 1)
			return (error);

		/* Try N / 2. */
		actual >>= 1;
	}

	/*
	 * We now have N actual messages mapped onto SYS_RES_IRQ resources in
	 * the irqs[] array, so add new resources starting at rid 1.
	 */
	for (i = 0; i < actual; i++)
		resource_list_add(&dinfo->resources, SYS_RES_IRQ, i + 1,
		    irqs[i], irqs[i], 1);

	if (bootverbose) {
		if (actual == 1) {
			device_printf(child, "using IRQ %d for MSI\n", irqs[0]);
		} else {
			/*
			 * Be fancy and try to print contiguous runs
			 * of IRQ values as ranges.  'run' is true if
			 * we are in a range.
			 */
			device_printf(child, "using IRQs %d", irqs[0]);
			run = 0;
			for (i = 1; i < actual; i++) {
				/* Still in a run? */
				if (irqs[i] == irqs[i - 1] + 1) {
					run = 1;
					continue;
				}

				/* Finish previous range. */
				if (run) {
					printf("-%d", irqs[i - 1]);
					run = 0;
				}

				/* Start new range. */
				printf(",%d", irqs[i]);
			}

			/* Unfinished range? */
			if (run)
				printf("-%d", irqs[actual - 1]);
			printf(" for MSI\n");
		}
	}

	/* Update counts of alloc'd messages. */
	dinfo->msi.msi_alloc = actual;
	dinfo->msi.msi_handlers = 0;
	*count = actual;
	return (0);
}

/*
 * Release the MSI messages associated with this DPAA2 device.
 *
 * NOTE: Implementation is similar to sys/dev/pci/pci.c.
 */
static int
dpaa2_rc_release_msi(device_t rcdev, device_t child)
{
	struct dpaa2_devinfo *rcinfo = device_get_ivars(rcdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(child);
	struct resource_list_entry *rle;
	int i, irqs[32];

	/* MSI should be released by the resource container. */
	if (rcinfo->dtype != DPAA2_DEV_RC)
		return (ENODEV);

	/* Do we have any messages to release? */
	if (dinfo->msi.msi_alloc == 0)
		return (ENODEV);
	KASSERT(dinfo->msi.msi_alloc <= 32,
	    ("more than 32 alloc'd MSI messages"));

	/* Make sure none of the resources are allocated. */
	if (dinfo->msi.msi_handlers > 0)
		return (EBUSY);
	for (i = 0; i < dinfo->msi.msi_alloc; i++) {
		rle = resource_list_find(&dinfo->resources, SYS_RES_IRQ, i + 1);
		KASSERT(rle != NULL, ("missing MSI resource"));
		if (rle->res != NULL)
			return (EBUSY);
		irqs[i] = rle->start;
	}

	/* Release the messages. */
	PCIB_RELEASE_MSI(device_get_parent(rcdev), child, dinfo->msi.msi_alloc,
	    irqs);
	for (i = 0; i < dinfo->msi.msi_alloc; i++)
		resource_list_delete(&dinfo->resources, SYS_RES_IRQ, i + 1);

	/* Update alloc count. */
	dinfo->msi.msi_alloc = 0;
	return (0);
}

/**
 * @brief Return the maximum number of the MSI supported by this DPAA2 device.
 */
static int
dpaa2_rc_msi_count(device_t rcdev, device_t child)
{
	struct dpaa2_devinfo *dinfo = device_get_ivars(child);

	return (dinfo->msi.msi_msgnum);
}

static int
dpaa2_rc_get_id(device_t rcdev, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	struct dpaa2_devinfo *rcinfo = device_get_ivars(rcdev);

	if (rcinfo->dtype != DPAA2_DEV_RC)
		return (ENODEV);

	return (PCIB_GET_ID(device_get_parent(rcdev), child, type, id));
}

/*
 * DPAA2 MC command interface.
 */

static int
dpaa2_rc_mng_get_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t *major, uint32_t *minor, uint32_t *rev)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || major == NULL || minor == NULL ||
	    rev == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MNG_GET_VER);
	if (!error) {
		*major = cmd->params[0] >> 32;
		*minor = cmd->params[1] & 0xFFFFFFFF;
		*rev = cmd->params[0] & 0xFFFFFFFF;
	}

	return (error);
}

static int
dpaa2_rc_mng_get_soc_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t *pvr, uint32_t *svr)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || pvr == NULL || svr == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MNG_GET_SOC_VER);
	if (!error) {
		*pvr = cmd->params[0] >> 32;
		*svr = cmd->params[0] & 0xFFFFFFFF;
	}

	return (error);
}

static int
dpaa2_rc_mng_get_container_id(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, uint32_t *cont_id)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || cont_id == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MNG_GET_CONT_ID);
	if (!error)
		*cont_id = cmd->params[0] & 0xFFFFFFFF;

	return (error);
}

static int
dpaa2_rc_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t cont_id, uint16_t *token)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	struct dpaa2_cmd_header *hdr;
	int error;

	if (portal == NULL || cmd == NULL || token == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = cont_id;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_OPEN);
	if (!error) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		*token = hdr->token;
	}

	return (error);
}

static int
dpaa2_rc_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_CLOSE));
}

static int
dpaa2_rc_get_obj_count(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t *obj_count)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || obj_count == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_GET_OBJ_COUNT);
	if (!error)
		*obj_count = (uint32_t)(cmd->params[0] >> 32);

	return (error);
}

static int
dpaa2_rc_get_obj(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t obj_idx, struct dpaa2_obj *obj)
{
	struct __packed dpaa2_obj_resp {
		uint32_t	_reserved1;
		uint32_t	id;
		uint16_t	vendor;
		uint8_t		irq_count;
		uint8_t		reg_count;
		uint32_t	state;
		uint16_t	ver_major;
		uint16_t	ver_minor;
		uint16_t	flags;
		uint16_t	_reserved2;
		uint8_t		type[16];
		uint8_t		label[16];
	} *pobj;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || obj == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = obj_idx;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_GET_OBJ);
	if (!error) {
		pobj = (struct dpaa2_obj_resp *) &cmd->params[0];
		obj->id = pobj->id;
		obj->vendor = pobj->vendor;
		obj->irq_count = pobj->irq_count;
		obj->reg_count = pobj->reg_count;
		obj->state = pobj->state;
		obj->ver_major = pobj->ver_major;
		obj->ver_minor = pobj->ver_minor;
		obj->flags = pobj->flags;
		obj->type = dpaa2_stot((const char *) pobj->type);
		memcpy(obj->label, pobj->label, sizeof(pobj->label));
	}

	/* Some DPAA2 objects might not be supported by the driver yet. */
	if (obj->type == DPAA2_DEV_NOTYPE)
		error = DPAA2_CMD_STAT_UNKNOWN_OBJ;

	return (error);
}

static int
dpaa2_rc_get_obj_descriptor(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, uint32_t obj_id, enum dpaa2_dev_type dtype,
    struct dpaa2_obj *obj)
{
	struct __packed get_obj_desc_args {
		uint32_t	obj_id;
		uint32_t	_reserved1;
		uint8_t		type[16];
	} *args;
	struct __packed dpaa2_obj_resp {
		uint32_t	_reserved1;
		uint32_t	id;
		uint16_t	vendor;
		uint8_t		irq_count;
		uint8_t		reg_count;
		uint32_t	state;
		uint16_t	ver_major;
		uint16_t	ver_minor;
		uint16_t	flags;
		uint16_t	_reserved2;
		uint8_t		type[16];
		uint8_t		label[16];
	} *pobj;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	const char *type = dpaa2_ttos(dtype);
	int error;

	if (portal == NULL || cmd == NULL || obj == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct get_obj_desc_args *) &cmd->params[0];
	args->obj_id = obj_id;
	memcpy(args->type, type, min(strlen(type) + 1, TYPE_LEN_MAX));

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_GET_OBJ_DESC);
	if (!error) {
		pobj = (struct dpaa2_obj_resp *) &cmd->params[0];
		obj->id = pobj->id;
		obj->vendor = pobj->vendor;
		obj->irq_count = pobj->irq_count;
		obj->reg_count = pobj->reg_count;
		obj->state = pobj->state;
		obj->ver_major = pobj->ver_major;
		obj->ver_minor = pobj->ver_minor;
		obj->flags = pobj->flags;
		obj->type = dpaa2_stot((const char *) pobj->type);
		memcpy(obj->label, pobj->label, sizeof(pobj->label));
	}

	/* Some DPAA2 objects might not be supported by the driver yet. */
	if (obj->type == DPAA2_DEV_NOTYPE)
		error = DPAA2_CMD_STAT_UNKNOWN_OBJ;

	return (error);
}

static int
dpaa2_rc_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_rc_attr *attr)
{
	struct __packed dpaa2_rc_attr {
		uint32_t	cont_id;
		uint32_t	icid;
		uint32_t	options;
		uint32_t	portal_id;
	} *pattr;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || attr == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_GET_ATTR);
	if (!error) {
		pattr = (struct dpaa2_rc_attr *) &cmd->params[0];
		attr->cont_id = pattr->cont_id;
		attr->portal_id = pattr->portal_id;
		attr->options = pattr->options;
		attr->icid = pattr->icid;
	}

	return (error);
}

static int
dpaa2_rc_get_obj_region(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t obj_id, uint8_t reg_idx, enum dpaa2_dev_type dtype,
    struct dpaa2_rc_obj_region *reg)
{
	struct __packed obj_region_args {
		uint32_t	obj_id;
		uint16_t	_reserved1;
		uint8_t		reg_idx;
		uint8_t		_reserved2;
		uint64_t	_reserved3;
		uint64_t	_reserved4;
		uint8_t		type[16];
	} *args;
	struct __packed obj_region {
		uint64_t	_reserved1;
		uint64_t	base_offset;
		uint32_t	size;
		uint32_t	type;
		uint32_t	flags;
		uint32_t	_reserved2;
		uint64_t	base_paddr;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	uint16_t cmdid, api_major, api_minor;
	const char *type = dpaa2_ttos(dtype);
	int error;

	if (portal == NULL || cmd == NULL || reg == NULL)
		return (DPAA2_CMD_STAT_ERR);

	/*
	 * If the DPRC object version was not yet cached, cache it now.
	 * Otherwise use the already cached value.
	 */
	if (!portal->rc_api_major && !portal->rc_api_minor) {
		error = DPAA2_CMD_RC_GET_API_VERSION(dev, child, cmd,
		    &api_major, &api_minor);
		if (error)
			return (error);
		portal->rc_api_major = api_major;
		portal->rc_api_minor = api_minor;
	} else {
		api_major = portal->rc_api_major;
		api_minor = portal->rc_api_minor;
	}

	/* TODO: Remove magic numbers. */
	if (api_major > 6u || (api_major == 6u && api_minor >= 6u))
		/*
		 * MC API version 6.6 changed the size of the MC portals and
		 * software portals to 64K (as implemented by hardware).
		 */
		cmdid = CMDID_RC_GET_OBJ_REG_V3;
	else if (api_major == 6u && api_minor >= 3u)
		/*
		 * MC API version 6.3 introduced a new field to the region
		 * descriptor: base_address.
		 */
		cmdid = CMDID_RC_GET_OBJ_REG_V2;
	else
		cmdid = CMDID_RC_GET_OBJ_REG;

	args = (struct obj_region_args *) &cmd->params[0];
	args->obj_id = obj_id;
	args->reg_idx = reg_idx;
	memcpy(args->type, type, min(strlen(type) + 1, TYPE_LEN_MAX));

	error = dpaa2_rc_exec_cmd(portal, cmd, cmdid);
	if (!error) {
		resp = (struct obj_region *) &cmd->params[0];
		reg->base_paddr = resp->base_paddr;
		reg->base_offset = resp->base_offset;
		reg->size = resp->size;
		reg->flags = resp->flags;
		reg->type = resp->type & 0xFu;
	}

	return (error);
}

static int
dpaa2_rc_get_api_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint16_t *major, uint16_t *minor)
{
	struct __packed rc_api_version {
		uint16_t	major;
		uint16_t	minor;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || major == NULL || minor == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_GET_API_VERSION);
	if (!error) {
		resp = (struct rc_api_version *) &cmd->params[0];
		*major = resp->major;
		*minor = resp->minor;
	}

	return (error);
}

static int
dpaa2_rc_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint8_t enable)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_enable_irq(portal, cmd, irq_idx, enable,
	    CMDID_RC_SET_IRQ_ENABLE));
}

static int
dpaa2_rc_set_obj_irq(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint64_t addr, uint32_t data, uint32_t irq_usr,
    uint32_t obj_id, enum dpaa2_dev_type dtype)
{
	struct __packed set_obj_irq_args {
		uint32_t	data;
		uint8_t		irq_idx;
		uint8_t		_reserved1[3];
		uint64_t	addr;
		uint32_t	irq_usr;
		uint32_t	obj_id;
		uint8_t		type[16];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	const char *type = dpaa2_ttos(dtype);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct set_obj_irq_args *) &cmd->params[0];
	args->irq_idx = irq_idx;
	args->addr = addr;
	args->data = data;
	args->irq_usr = irq_usr;
	args->obj_id = obj_id;
	memcpy(args->type, type, min(strlen(type) + 1, TYPE_LEN_MAX));

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_SET_OBJ_IRQ));
}

static int
dpaa2_rc_get_conn(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ep_desc *ep1_desc, struct dpaa2_ep_desc *ep2_desc,
    uint32_t *link_stat)
{
	struct __packed get_conn_args {
		uint32_t ep1_id;
		uint32_t ep1_ifid;
		uint8_t  ep1_type[16];
		uint64_t _reserved[4];
	} *args;
	struct __packed get_conn_resp {
		uint64_t _reserved1[3];
		uint32_t ep2_id;
		uint32_t ep2_ifid;
		uint8_t  ep2_type[16];
		uint32_t link_stat;
		uint32_t _reserved2;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || ep1_desc == NULL ||
	    ep2_desc == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct get_conn_args *) &cmd->params[0];
	args->ep1_id = ep1_desc->obj_id;
	args->ep1_ifid = ep1_desc->if_id;
	/* TODO: Remove magic number. */
	strncpy(args->ep1_type, dpaa2_ttos(ep1_desc->type), 16);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_RC_GET_CONN);
	if (!error) {
		resp = (struct get_conn_resp *) &cmd->params[0];
		ep2_desc->obj_id = resp->ep2_id;
		ep2_desc->if_id = resp->ep2_ifid;
		ep2_desc->type = dpaa2_stot((const char *) resp->ep2_type);
		if (link_stat != NULL)
			*link_stat = resp->link_stat;
	}

	return (error);
}

static int
dpaa2_rc_ni_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t dpni_id, uint16_t *token)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	struct dpaa2_cmd_header *hdr;
	int error;

	if (portal == NULL || cmd == NULL || token == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = dpni_id;
	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_OPEN);
 	if (!error) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		*token = hdr->token;
	}

	return (error);
}

static int
dpaa2_rc_ni_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_CLOSE));
}

static int
dpaa2_rc_ni_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_ENABLE));
}

static int
dpaa2_rc_ni_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_DISABLE));
}

static int
dpaa2_rc_ni_get_api_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint16_t *major, uint16_t *minor)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || major == NULL || minor == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_API_VER);
	if (!error) {
		*major = cmd->params[0] & 0xFFFFU;
		*minor = (cmd->params[0] >> 16) & 0xFFFFU;
	}

	return (error);
}

static int
dpaa2_rc_ni_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_RESET));
}

static int
dpaa2_rc_ni_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_attr *attr)
{
	struct __packed ni_attr {
		uint32_t	options;
		uint8_t		num_queues;
		uint8_t		num_rx_tcs;
		uint8_t		mac_entries;
		uint8_t		num_tx_tcs;
		uint8_t		vlan_entries;
		uint8_t		num_channels;
		uint8_t		qos_entries;
		uint8_t		_reserved1;
		uint16_t	fs_entries;
		uint16_t	_reserved2;
		uint8_t		qos_key_size;
		uint8_t		fs_key_size;
		uint16_t	wriop_ver;
		uint8_t		num_cgs;
		uint8_t		_reserved3;
		uint16_t	_reserved4;
		uint64_t	_reserved5[4];
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || attr == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_ATTR);
	if (!error) {
		resp = (struct ni_attr *) &cmd->params[0];

		attr->options =	     resp->options;
		attr->wriop_ver =    resp->wriop_ver;

		attr->entries.fs =   resp->fs_entries;
		attr->entries.mac =  resp->mac_entries;
		attr->entries.vlan = resp->vlan_entries;
		attr->entries.qos =  resp->qos_entries;

		attr->num.queues =   resp->num_queues;
		attr->num.rx_tcs =   resp->num_rx_tcs;
		attr->num.tx_tcs =   resp->num_tx_tcs;
		attr->num.channels = resp->num_channels;
		attr->num.cgs =      resp->num_cgs;

		attr->key_size.fs =  resp->fs_key_size;
		attr->key_size.qos = resp->qos_key_size;
	}

	return (error);
}

static int
dpaa2_rc_ni_set_buf_layout(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_buf_layout *bl)
{
	struct __packed set_buf_layout_args {
		uint8_t		queue_type;
		uint8_t		_reserved1;
		uint16_t	_reserved2;
		uint16_t	options;
		uint8_t		params;
		uint8_t		_reserved3;
		uint16_t	priv_data_size;
		uint16_t	data_align;
		uint16_t	head_room;
		uint16_t	tail_room;
		uint64_t	_reserved4[5];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || bl == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct set_buf_layout_args *) &cmd->params[0];
	args->queue_type = (uint8_t) bl->queue_type;
	args->options = bl->options;
	args->params = 0;
	args->priv_data_size = bl->pd_size;
	args->data_align = bl->fd_align;
	args->head_room = bl->head_size;
	args->tail_room = bl->tail_size;

	args->params |= bl->pass_timestamp	? 1U : 0U;
	args->params |= bl->pass_parser_result	? 2U : 0U;
	args->params |= bl->pass_frame_status	? 4U : 0U;
	args->params |= bl->pass_sw_opaque	? 8U : 0U;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_BUF_LAYOUT));
}

static int
dpaa2_rc_ni_get_tx_data_offset(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, uint16_t *offset)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || offset == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_TX_DATA_OFF);
	if (!error)
		*offset = cmd->params[0] & 0xFFFFU;

	return (error);
}

static int
dpaa2_rc_ni_get_port_mac_addr(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, uint8_t *mac)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || mac == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_PORT_MAC_ADDR);
	if (!error) {
		mac[0] = (cmd->params[0] >> 56) & 0xFFU;
		mac[1] = (cmd->params[0] >> 48) & 0xFFU;
		mac[2] = (cmd->params[0] >> 40) & 0xFFU;
		mac[3] = (cmd->params[0] >> 32) & 0xFFU;
		mac[4] = (cmd->params[0] >> 24) & 0xFFU;
		mac[5] = (cmd->params[0] >> 16) & 0xFFU;
	}

	return (error);
}

static int
dpaa2_rc_ni_set_prim_mac_addr(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, uint8_t *mac)
{
	struct __packed set_prim_mac_args {
		uint8_t		_reserved[2];
		uint8_t		mac[ETHER_ADDR_LEN];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || mac == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	args = (struct set_prim_mac_args *) &cmd->params[0];
	for (int i = 1; i <= ETHER_ADDR_LEN; i++)
		args->mac[i - 1] = mac[ETHER_ADDR_LEN - i];

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_PRIM_MAC_ADDR));
}

static int
dpaa2_rc_ni_get_prim_mac_addr(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, uint8_t *mac)
{
	struct __packed get_prim_mac_resp {
		uint8_t		_reserved[2];
		uint8_t		mac[ETHER_ADDR_LEN];
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || mac == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_PRIM_MAC_ADDR);
	if (!error) {
		resp = (struct get_prim_mac_resp *) &cmd->params[0];
		for (int i = 1; i <= ETHER_ADDR_LEN; i++)
			mac[ETHER_ADDR_LEN - i] = resp->mac[i - 1];
	}

	return (error);
}

static int
dpaa2_rc_ni_set_link_cfg(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_link_cfg *cfg)
{
	struct __packed link_cfg_args {
		uint64_t	_reserved1;
		uint32_t	rate;
		uint32_t	_reserved2;
		uint64_t	options;
		uint64_t	adv_speeds;
		uint64_t	_reserved3[3];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || cfg == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	args = (struct link_cfg_args *) &cmd->params[0];
	args->rate = cfg->rate;
	args->options = cfg->options;
	args->adv_speeds = cfg->adv_speeds;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_LINK_CFG));
}

static int
dpaa2_rc_ni_get_link_cfg(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_link_cfg *cfg)
{
	struct __packed link_cfg_resp {
		uint64_t	_reserved1;
		uint32_t	rate;
		uint32_t	_reserved2;
		uint64_t	options;
		uint64_t	adv_speeds;
		uint64_t	_reserved3[3];
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || cfg == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_LINK_CFG);
	if (!error) {
		resp = (struct link_cfg_resp *) &cmd->params[0];
		cfg->rate = resp->rate;
		cfg->options = resp->options;
		cfg->adv_speeds = resp->adv_speeds;
	}

	return (error);
}

static int
dpaa2_rc_ni_get_link_state(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_link_state *state)
{
	struct __packed link_state_resp {
		uint32_t	_reserved1;
		uint32_t	flags;
		uint32_t	rate;
		uint32_t	_reserved2;
		uint64_t	options;
		uint64_t	supported;
		uint64_t	advert;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || state == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_LINK_STATE);
	if (!error) {
		resp = (struct link_state_resp *) &cmd->params[0];
		state->options = resp->options;
		state->adv_speeds = resp->advert;
		state->sup_speeds = resp->supported;
		state->rate = resp->rate;

		state->link_up = resp->flags & 0x1u ? true : false;
		state->state_valid = resp->flags & 0x2u ? true : false;
	}

	return (error);
}

static int
dpaa2_rc_ni_set_qos_table(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_qos_table *tbl)
{
	struct __packed qos_table_args {
		uint32_t	_reserved1;
		uint8_t		default_tc;
		uint8_t		options;
		uint16_t	_reserved2;
		uint64_t	_reserved[5];
		uint64_t	kcfg_busaddr;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || tbl == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct qos_table_args *) &cmd->params[0];
	args->default_tc = tbl->default_tc;
	args->kcfg_busaddr = tbl->kcfg_busaddr;

	args->options |= tbl->discard_on_miss	? 1U : 0U;
	args->options |= tbl->keep_entries	? 2U : 0U;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_QOS_TABLE));
}

static int
dpaa2_rc_ni_clear_qos_table(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_CLEAR_QOS_TABLE));
}

static int
dpaa2_rc_ni_set_pools(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_pools_cfg *cfg)
{
	struct __packed set_pools_args {
		uint8_t		pools_num;
		uint8_t		backup_pool_mask;
		uint8_t		_reserved1;
		uint8_t		pool_as; /* assigning: 0 - QPRI, 1 - QDBIN */
		uint32_t	bp_obj_id[DPAA2_NI_MAX_POOLS];
		uint16_t	buf_sz[DPAA2_NI_MAX_POOLS];
		uint32_t	_reserved2;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || cfg == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_pools_args *) &cmd->params[0];
	args->pools_num = cfg->pools_num < DPAA2_NI_MAX_POOLS
	    ? cfg->pools_num : DPAA2_NI_MAX_POOLS;
	for (uint32_t i = 0; i < args->pools_num; i++) {
		args->bp_obj_id[i] = cfg->pools[i].bp_obj_id;
		args->buf_sz[i] = cfg->pools[i].buf_sz;
		args->backup_pool_mask |= (cfg->pools[i].backup_flag & 1) << i;
	}

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_POOLS));
}

static int
dpaa2_rc_ni_set_err_behavior(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_err_cfg *cfg)
{
	struct __packed err_behavior_args {
		uint32_t	err_mask;
		uint8_t		flags;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || cfg == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct err_behavior_args *) &cmd->params[0];
	args->err_mask = cfg->err_mask;

	args->flags |= cfg->set_err_fas ? 0x10u : 0u;
	args->flags |= ((uint8_t) cfg->action) & 0x0Fu;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_ERR_BEHAVIOR));
}

static int
dpaa2_rc_ni_get_queue(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_queue_cfg *cfg)
{
	struct __packed get_queue_args {
		uint8_t		queue_type;
		uint8_t		tc;
		uint8_t		idx;
		uint8_t		chan_id;
	} *args;
	struct __packed get_queue_resp {
		uint64_t	_reserved1;
		uint32_t	dest_id;
		uint16_t	_reserved2;
		uint8_t		priority;
		uint8_t		flags;
		uint64_t	flc;
		uint64_t	user_ctx;
		uint32_t	fqid;
		uint16_t	qdbin;
		uint16_t	_reserved3;
		uint8_t		cgid;
		uint8_t		_reserved[15];
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || cfg == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct get_queue_args *) &cmd->params[0];
	args->queue_type = (uint8_t) cfg->type;
	args->tc = cfg->tc;
	args->idx = cfg->idx;
	args->chan_id = cfg->chan_id;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_QUEUE);
	if (!error) {
		resp = (struct get_queue_resp *) &cmd->params[0];

		cfg->dest_id = resp->dest_id;
		cfg->priority = resp->priority;
		cfg->flow_ctx = resp->flc;
		cfg->user_ctx = resp->user_ctx;
		cfg->fqid = resp->fqid;
		cfg->qdbin = resp->qdbin;
		cfg->cgid = resp->cgid;

		cfg->dest_type = (enum dpaa2_ni_dest_type) resp->flags & 0x0Fu;
		cfg->cgid_valid = (resp->flags & 0x20u) > 0u ? true : false;
		cfg->stash_control = (resp->flags & 0x40u) > 0u ? true : false;
		cfg->hold_active = (resp->flags & 0x80u) > 0u ? true : false;
	}

	return (error);
}

static int
dpaa2_rc_ni_set_queue(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_ni_queue_cfg *cfg)
{
	struct __packed set_queue_args {
		uint8_t		queue_type;
		uint8_t		tc;
		uint8_t		idx;
		uint8_t		options;
		uint32_t	_reserved1;
		uint32_t	dest_id;
		uint16_t	_reserved2;
		uint8_t		priority;
		uint8_t		flags;
		uint64_t	flc;
		uint64_t	user_ctx;
		uint8_t		cgid;
		uint8_t		chan_id;
		uint8_t		_reserved[23];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || cfg == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_queue_args *) &cmd->params[0];
	args->queue_type = (uint8_t) cfg->type;
	args->tc = cfg->tc;
	args->idx = cfg->idx;
	args->options = cfg->options;
	args->dest_id = cfg->dest_id;
	args->priority = cfg->priority;
	args->flc = cfg->flow_ctx;
	args->user_ctx = cfg->user_ctx;
	args->cgid = cfg->cgid;
	args->chan_id = cfg->chan_id;

	args->flags |= (uint8_t)(cfg->dest_type & 0x0Fu);
	args->flags |= cfg->stash_control ? 0x40u : 0u;
	args->flags |= cfg->hold_active ? 0x80u : 0u;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_QUEUE));
}

static int
dpaa2_rc_ni_get_qdid(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    enum dpaa2_ni_queue_type type, uint16_t *qdid)
{
	struct __packed get_qdid_args {
		uint8_t		queue_type;
	} *args;
	struct __packed get_qdid_resp {
		uint16_t	qdid;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || qdid == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct get_qdid_args *) &cmd->params[0];
	args->queue_type = (uint8_t) type;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_QDID);
	if (!error) {
		resp = (struct get_qdid_resp *) &cmd->params[0];
		*qdid = resp->qdid;
	}

	return (error);
}

static int
dpaa2_rc_ni_add_mac_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t *mac)
{
	struct __packed add_mac_args {
		uint8_t		flags;
		uint8_t		_reserved;
		uint8_t		mac[ETHER_ADDR_LEN];
		uint8_t		tc_id;
		uint8_t		fq_id;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || mac == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct add_mac_args *) &cmd->params[0];
	for (int i = 1; i <= ETHER_ADDR_LEN; i++)
		args->mac[i - 1] = mac[ETHER_ADDR_LEN - i];

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_ADD_MAC_ADDR));
}

static int
dpaa2_rc_ni_remove_mac_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t *mac)
{
	struct __packed rem_mac_args {
		uint16_t	_reserved;
		uint8_t		mac[ETHER_ADDR_LEN];
		uint64_t	_reserved1[6];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || mac == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct rem_mac_args *) &cmd->params[0];
	for (int i = 1; i <= ETHER_ADDR_LEN; i++)
		args->mac[i - 1] = mac[ETHER_ADDR_LEN - i];

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_REMOVE_MAC_ADDR));
}

static int
dpaa2_rc_ni_clear_mac_filters(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, bool rm_uni, bool rm_multi)
{
	struct __packed clear_mac_filters_args {
		uint8_t		flags;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct clear_mac_filters_args *) &cmd->params[0];
	args->flags |= rm_uni ? 0x1 : 0x0;
	args->flags |= rm_multi ? 0x2 : 0x0;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_CLEAR_MAC_FILTERS));
}

static int
dpaa2_rc_ni_set_mfl(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint16_t length)
{
	struct __packed set_mfl_args {
		uint16_t length;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_mfl_args *) &cmd->params[0];
	args->length = length;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_MFL));
}

static int
dpaa2_rc_ni_set_offload(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    enum dpaa2_ni_ofl_type ofl_type, bool en)
{
	struct __packed set_ofl_args {
		uint8_t		_reserved[3];
		uint8_t		ofl_type;
		uint32_t	config;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_ofl_args *) &cmd->params[0];
	args->ofl_type = (uint8_t) ofl_type;
	args->config = en ? 1u : 0u;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_OFFLOAD));
}

static int
dpaa2_rc_ni_set_irq_mask(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint32_t mask)
{
	struct __packed set_irq_mask_args {
		uint32_t	mask;
		uint8_t		irq_idx;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_irq_mask_args *) &cmd->params[0];
	args->mask = mask;
	args->irq_idx = irq_idx;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_IRQ_MASK));
}

static int
dpaa2_rc_ni_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, bool en)
{
	struct __packed set_irq_enable_args {
		uint32_t	en;
		uint8_t		irq_idx;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_irq_enable_args *) &cmd->params[0];
	args->en = en ? 1u : 0u;
	args->irq_idx = irq_idx;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_IRQ_ENABLE));
}

static int
dpaa2_rc_ni_get_irq_status(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint32_t *status)
{
	struct __packed get_irq_stat_args {
		uint32_t	status;
		uint8_t		irq_idx;
	} *args;
	struct __packed get_irq_stat_resp {
		uint32_t	status;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || status == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct get_irq_stat_args *) &cmd->params[0];
	args->status = *status;
	args->irq_idx = irq_idx;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_IRQ_STATUS);
	if (!error) {
		resp = (struct get_irq_stat_resp *) &cmd->params[0];
		*status = resp->status;
	}

	return (error);
}

static int
dpaa2_rc_ni_set_uni_promisc(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    bool en)
{
	struct __packed set_uni_promisc_args {
		uint8_t	en;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_uni_promisc_args *) &cmd->params[0];
	args->en = en ? 1u : 0u;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_UNI_PROMISC));
}

static int
dpaa2_rc_ni_set_multi_promisc(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, bool en)
{
	/* TODO: Implementation is the same as for ni_set_uni_promisc(). */
	struct __packed set_multi_promisc_args {
		uint8_t	en;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_multi_promisc_args *) &cmd->params[0];
	args->en = en ? 1u : 0u;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_MULTI_PROMISC));
}

static int
dpaa2_rc_ni_get_statistics(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t page, uint16_t param, uint64_t *cnt)
{
	struct __packed get_statistics_args {
		uint8_t		page;
		uint16_t	param;
	} *args;
	struct __packed get_statistics_resp {
		uint64_t	cnt[7];
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || cnt == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct get_statistics_args *) &cmd->params[0];
	args->page = page;
	args->param = param;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_GET_STATISTICS);
	if (!error) {
		resp = (struct get_statistics_resp *) &cmd->params[0];
		for (int i = 0; i < DPAA2_NI_STAT_COUNTERS; i++)
			cnt[i] = resp->cnt[i];
	}

	return (error);
}

static int
dpaa2_rc_ni_set_rx_tc_dist(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint16_t dist_size, uint8_t tc, enum dpaa2_ni_dist_mode dist_mode,
    bus_addr_t key_cfg_buf)
{
	struct __packed set_rx_tc_dist_args {
		uint16_t	dist_size;
		uint8_t		tc;
		uint8_t		ma_dm; /* miss action + dist. mode */
		uint32_t	_reserved1;
		uint64_t	_reserved2[5];
		uint64_t	key_cfg_iova;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_rx_tc_dist_args *) &cmd->params[0];
	args->dist_size = dist_size;
	args->tc = tc;
	args->ma_dm = ((uint8_t) dist_mode) & 0x0Fu;
	args->key_cfg_iova = key_cfg_buf;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_NI_SET_RX_TC_DIST));
}

static int
dpaa2_rc_io_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t dpio_id, uint16_t *token)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	struct dpaa2_cmd_header *hdr;
	int error;

	if (portal == NULL || cmd == NULL || token == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = dpio_id;
	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_OPEN);
	if (!error) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		*token = hdr->token;
	}

	return (error);
}

static int
dpaa2_rc_io_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_CLOSE));
}

static int
dpaa2_rc_io_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_ENABLE));
}

static int
dpaa2_rc_io_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_DISABLE));
}

static int
dpaa2_rc_io_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_RESET));
}

static int
dpaa2_rc_io_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_io_attr *attr)
{
	struct __packed dpaa2_io_attr {
		uint32_t	id;
		uint16_t	swp_id;
		uint8_t		priors_num;
		uint8_t		chan_mode;
		uint64_t	swp_ce_paddr;
		uint64_t	swp_ci_paddr;
		uint32_t	swp_version;
		uint32_t	_reserved1;
		uint32_t	swp_clk;
		uint32_t	_reserved2[5];
	} *pattr;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || attr == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_GET_ATTR);
	if (!error) {
		pattr = (struct dpaa2_io_attr *) &cmd->params[0];

		attr->swp_ce_paddr = pattr->swp_ce_paddr;
		attr->swp_ci_paddr = pattr->swp_ci_paddr;
		attr->swp_version = pattr->swp_version;
		attr->swp_clk = pattr->swp_clk;
		attr->id = pattr->id;
		attr->swp_id = pattr->swp_id;
		attr->priors_num = pattr->priors_num;
		attr->chan_mode = (enum dpaa2_io_chan_mode)
		    pattr->chan_mode;
	}

	return (error);
}

static int
dpaa2_rc_io_set_irq_mask(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint32_t mask)
{
	/* TODO: Extract similar *_set_irq_mask() into one function. */
	struct __packed set_irq_mask_args {
		uint32_t	mask;
		uint8_t		irq_idx;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_irq_mask_args *) &cmd->params[0];
	args->mask = mask;
	args->irq_idx = irq_idx;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_SET_IRQ_MASK));
}

static int
dpaa2_rc_io_get_irq_status(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint32_t *status)
{
	/* TODO: Extract similar *_get_irq_status() into one function. */
	struct __packed get_irq_stat_args {
		uint32_t	status;
		uint8_t		irq_idx;
	} *args;
	struct __packed get_irq_stat_resp {
		uint32_t	status;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || status == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct get_irq_stat_args *) &cmd->params[0];
	args->status = *status;
	args->irq_idx = irq_idx;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_GET_IRQ_STATUS);
	if (!error) {
		resp = (struct get_irq_stat_resp *) &cmd->params[0];
		*status = resp->status;
	}

	return (error);
}

static int
dpaa2_rc_io_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, bool en)
{
	/* TODO: Extract similar *_set_irq_enable() into one function. */
	struct __packed set_irq_enable_args {
		uint32_t	en;
		uint8_t		irq_idx;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_irq_enable_args *) &cmd->params[0];
	args->en = en ? 1u : 0u;
	args->irq_idx = irq_idx;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_SET_IRQ_ENABLE));
}

static int
dpaa2_rc_io_add_static_dq_chan(device_t dev, device_t child,
    struct dpaa2_cmd *cmd, uint32_t dpcon_id, uint8_t *chan_idx)
{
	struct __packed add_static_dq_chan_args {
		uint32_t	dpcon_id;
	} *args;
	struct __packed add_static_dq_chan_resp {
		uint8_t		chan_idx;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || chan_idx == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct add_static_dq_chan_args *) &cmd->params[0];
	args->dpcon_id = dpcon_id;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_IO_ADD_STATIC_DQ_CHAN);
	if (!error) {
		resp = (struct add_static_dq_chan_resp *) &cmd->params[0];
		*chan_idx = resp->chan_idx;
	}

	return (error);
}

static int
dpaa2_rc_bp_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t dpbp_id, uint16_t *token)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	struct dpaa2_cmd_header *hdr;
	int error;

	if (portal == NULL || cmd == NULL || token == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = dpbp_id;
	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_BP_OPEN);
	if (!error) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		*token = hdr->token;
	}

	return (error);
}

static int
dpaa2_rc_bp_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_BP_CLOSE));
}

static int
dpaa2_rc_bp_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_BP_ENABLE));
}

static int
dpaa2_rc_bp_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_BP_DISABLE));
}

static int
dpaa2_rc_bp_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_BP_RESET));
}

static int
dpaa2_rc_bp_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_bp_attr *attr)
{
	struct __packed dpaa2_bp_attr {
		uint16_t	_reserved1;
		uint16_t	bpid;
		uint32_t	id;
	} *pattr;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || attr == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_BP_GET_ATTR);
	if (!error) {
		pattr = (struct dpaa2_bp_attr *) &cmd->params[0];
		attr->id = pattr->id;
		attr->bpid = pattr->bpid;
	}

	return (error);
}

static int
dpaa2_rc_mac_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t dpmac_id, uint16_t *token)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	struct dpaa2_cmd_header *hdr;
	int error;

	if (portal == NULL || cmd == NULL || token == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = dpmac_id;
	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_OPEN);
	if (!error) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		*token = hdr->token;
	}

	return (error);
}

static int
dpaa2_rc_mac_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_CLOSE));
}

static int
dpaa2_rc_mac_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_RESET));
}

static int
dpaa2_rc_mac_mdio_read(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t phy, uint16_t reg, uint16_t *val)
{
	struct __packed mdio_read_args {
		uint8_t		clause; /* set to 0 by default */
		uint8_t		phy;
		uint16_t	reg;
		uint32_t	_reserved1;
		uint64_t	_reserved2[6];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || val == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct mdio_read_args *) &cmd->params[0];
	args->phy = phy;
	args->reg = reg;
	args->clause = 0;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_MDIO_READ);
	if (!error)
		*val = cmd->params[0] & 0xFFFF;

	return (error);
}

static int
dpaa2_rc_mac_mdio_write(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t phy, uint16_t reg, uint16_t val)
{
	struct __packed mdio_write_args {
		uint8_t		clause; /* set to 0 by default */
		uint8_t		phy;
		uint16_t	reg;
		uint16_t	val;
		uint16_t	_reserved1;
		uint64_t	_reserved2[6];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct mdio_write_args *) &cmd->params[0];
	args->phy = phy;
	args->reg = reg;
	args->val = val;
	args->clause = 0;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_MDIO_WRITE));
}

static int
dpaa2_rc_mac_get_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t *mac)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || mac == NULL)
		return (DPAA2_CMD_STAT_ERR);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_GET_ADDR);
	if (!error) {
		mac[0] = (cmd->params[0] >> 56) & 0xFFU;
		mac[1] = (cmd->params[0] >> 48) & 0xFFU;
		mac[2] = (cmd->params[0] >> 40) & 0xFFU;
		mac[3] = (cmd->params[0] >> 32) & 0xFFU;
		mac[4] = (cmd->params[0] >> 24) & 0xFFU;
		mac[5] = (cmd->params[0] >> 16) & 0xFFU;
	}

	return (error);
}

static int
dpaa2_rc_mac_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_mac_attr *attr)
{
	struct __packed mac_attr_resp {
		uint8_t		eth_if;
		uint8_t		link_type;
		uint16_t	id;
		uint32_t	max_rate;

		uint8_t		fec_mode;
		uint8_t		ifg_mode;
		uint8_t		ifg_len;
		uint8_t		_reserved1;
		uint32_t	_reserved2;

		uint8_t		sgn_post_pre;
		uint8_t		serdes_cfg_mode;
		uint8_t		eq_amp_red;
		uint8_t		eq_post1q;
		uint8_t		eq_preq;
		uint8_t		eq_type;
		uint16_t	_reserved3;

		uint64_t	_reserved[4];
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || attr == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_GET_ATTR);
	if (!error) {
		resp = (struct mac_attr_resp *) &cmd->params[0];
		attr->id = resp->id;
		attr->max_rate = resp->max_rate;
		attr->eth_if = resp->eth_if;
		attr->link_type = resp->link_type;
	}

	return (error);
}

static int
dpaa2_rc_mac_set_link_state(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_mac_link_state *state)
{
	struct __packed mac_set_link_args {
		uint64_t	options;
		uint32_t	rate;
		uint32_t	_reserved1;
		uint32_t	flags;
		uint32_t	_reserved2;
		uint64_t	supported;
		uint64_t	advert;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || state == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct mac_set_link_args *) &cmd->params[0];
	args->options = state->options;
	args->rate = state->rate;
	args->supported = state->supported;
	args->advert = state->advert;

	args->flags |= state->up ? 0x1u : 0u;
	args->flags |= state->state_valid ? 0x2u : 0u;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_SET_LINK_STATE));
}

static int
dpaa2_rc_mac_set_irq_mask(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint32_t mask)
{
	/* TODO: Implementation is the same as for ni_set_irq_mask(). */
	struct __packed set_irq_mask_args {
		uint32_t	mask;
		uint8_t		irq_idx;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_irq_mask_args *) &cmd->params[0];
	args->mask = mask;
	args->irq_idx = irq_idx;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_SET_IRQ_MASK));
}

static int
dpaa2_rc_mac_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, bool en)
{
	/* TODO: Implementation is the same as for ni_set_irq_enable(). */
	struct __packed set_irq_enable_args {
		uint32_t	en;
		uint8_t		irq_idx;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct set_irq_enable_args *) &cmd->params[0];
	args->en = en ? 1u : 0u;
	args->irq_idx = irq_idx;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_SET_IRQ_ENABLE));
}

static int
dpaa2_rc_mac_get_irq_status(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, uint32_t *status)
{
	/* TODO: Implementation is the same as ni_get_irq_status(). */
	struct __packed get_irq_stat_args {
		uint32_t	status;
		uint8_t		irq_idx;
	} *args;
	struct __packed get_irq_stat_resp {
		uint32_t	status;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || status == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	dpaa2_rc_reset_cmd_params(cmd);

	args = (struct get_irq_stat_args *) &cmd->params[0];
	args->status = *status;
	args->irq_idx = irq_idx;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MAC_GET_IRQ_STATUS);
	if (!error) {
		resp = (struct get_irq_stat_resp *) &cmd->params[0];
		*status = resp->status;
	}

	return (error);
}

static int
dpaa2_rc_con_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t dpcon_id, uint16_t *token)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	struct dpaa2_cmd_header *hdr;
	int error;

	if (portal == NULL || cmd == NULL || token == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = dpcon_id;
	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_CON_OPEN);
	if (!error) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		*token = hdr->token;
	}

	return (error);
}


static int
dpaa2_rc_con_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_CON_CLOSE));
}

static int
dpaa2_rc_con_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_CON_RESET));
}

static int
dpaa2_rc_con_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_CON_ENABLE));
}

static int
dpaa2_rc_con_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_CON_DISABLE));
}

static int
dpaa2_rc_con_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_con_attr *attr)
{
	struct __packed con_attr_resp {
		uint32_t	id;
		uint16_t	chan_id;
		uint8_t		prior_num;
		uint8_t		_reserved1;
		uint64_t	_reserved2[6];
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || attr == NULL)
		return (DPAA2_CMD_STAT_EINVAL);

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_CON_GET_ATTR);
	if (!error) {
		resp = (struct con_attr_resp *) &cmd->params[0];
		attr->id = resp->id;
		attr->chan_id = resp->chan_id;
		attr->prior_num = resp->prior_num;
	}

	return (error);
}

static int
dpaa2_rc_con_set_notif(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    struct dpaa2_con_notif_cfg *cfg)
{
	struct __packed set_notif_args {
		uint32_t	dpio_id;
		uint8_t		prior;
		uint8_t		_reserved1;
		uint16_t	_reserved2;
		uint64_t	ctx;
		uint64_t	_reserved3[5];
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL || cfg == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct set_notif_args *) &cmd->params[0];
	args->dpio_id = cfg->dpio_id;
	args->prior = cfg->prior;
	args->ctx = cfg->qman_ctx;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_CON_SET_NOTIF));
}

static int
dpaa2_rc_mcp_create(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t portal_id, uint32_t options, uint32_t *dpmcp_id)
{
	struct __packed mcp_create_args {
		uint32_t	portal_id;
		uint32_t	options;
		uint64_t	_reserved[6];
	} *args;
	struct __packed mcp_create_resp {
		uint32_t	dpmcp_id;
	} *resp;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	int error;

	if (portal == NULL || cmd == NULL || dpmcp_id == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct mcp_create_args *) &cmd->params[0];
	args->portal_id = portal_id;
	args->options = options;

	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MCP_CREATE);
	if (!error) {
		resp = (struct mcp_create_resp *) &cmd->params[0];
		*dpmcp_id = resp->dpmcp_id;
	}

	return (error);
}

static int
dpaa2_rc_mcp_destroy(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t dpmcp_id)
{
	struct __packed mcp_destroy_args {
		uint32_t	dpmcp_id;
	} *args;
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct mcp_destroy_args *) &cmd->params[0];
	args->dpmcp_id = dpmcp_id;

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MCP_DESTROY));
}

static int
dpaa2_rc_mcp_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
    uint32_t dpmcp_id, uint16_t *token)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);
	struct dpaa2_cmd_header *hdr;
	int error;

	if (portal == NULL || cmd == NULL || token == NULL)
		return (DPAA2_CMD_STAT_ERR);

	cmd->params[0] = dpmcp_id;
	error = dpaa2_rc_exec_cmd(portal, cmd, CMDID_MCP_OPEN);
	if (!error) {
		hdr = (struct dpaa2_cmd_header *) &cmd->header;
		*token = hdr->token;
	}

	return (error);
}

static int
dpaa2_rc_mcp_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MCP_CLOSE));
}

static int
dpaa2_rc_mcp_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
{
	struct dpaa2_mcp *portal = dpaa2_rc_select_portal(dev, child);

	if (portal == NULL || cmd == NULL)
		return (DPAA2_CMD_STAT_ERR);

	return (dpaa2_rc_exec_cmd(portal, cmd, CMDID_MCP_RESET));
}

/**
 * @brief Create and add devices for DPAA2 objects in this resource container.
 */
static int
dpaa2_rc_discover(struct dpaa2_rc_softc *sc)
{
	device_t rcdev = sc->dev;
	device_t child = sc->dev;
	struct dpaa2_devinfo *rcinfo = device_get_ivars(rcdev);
	struct dpaa2_cmd cmd;
	struct dpaa2_rc_attr dprc_attr;
	struct dpaa2_obj obj;
	uint32_t major, minor, rev, obj_count;
	uint16_t rc_token;
	int rc;

	DPAA2_CMD_INIT(&cmd);

	/* Print MC firmware version. */
	rc = DPAA2_CMD_MNG_GET_VERSION(rcdev, child, &cmd, &major, &minor, &rev);
	if (rc) {
		device_printf(rcdev, "%s: failed to get MC firmware version: "
		    "error=%d\n", __func__, rc);
		return (ENXIO);
	}
	device_printf(rcdev, "MC firmware version: %u.%u.%u\n", major, minor,
	    rev);

	/* Obtain container ID associated with a given MC portal. */
	rc = DPAA2_CMD_MNG_GET_CONTAINER_ID(rcdev, child, &cmd, &sc->cont_id);
	if (rc) {
		device_printf(rcdev, "%s: failed to get container id: "
		    "error=%d\n", __func__, rc);
		return (ENXIO);
	}
	if (bootverbose) {
		device_printf(rcdev, "Resource container ID: %u\n", sc->cont_id);
	}

	/* Open the resource container. */
	rc = DPAA2_CMD_RC_OPEN(rcdev, child, &cmd, sc->cont_id, &rc_token);
	if (rc) {
		device_printf(rcdev, "%s: failed to open container: cont_id=%u, "
		    "error=%d\n", __func__, sc->cont_id, rc);
		return (ENXIO);
	}

	/* Obtain a number of objects in this container. */
	rc = DPAA2_CMD_RC_GET_OBJ_COUNT(rcdev, child, &cmd, &obj_count);
	if (rc) {
		device_printf(rcdev, "%s: failed to count objects in container: "
		    "cont_id=%u, error=%d\n", __func__, sc->cont_id, rc);
		(void)DPAA2_CMD_RC_CLOSE(rcdev, child, &cmd);
		return (ENXIO);
	}
	if (bootverbose) {
		device_printf(rcdev, "Objects in container: %u\n", obj_count);
	}

	rc = DPAA2_CMD_RC_GET_ATTRIBUTES(rcdev, child, &cmd, &dprc_attr);
	if (rc) {
		device_printf(rcdev, "%s: failed to get attributes of the "
		    "container: cont_id=%u, error=%d\n", __func__, sc->cont_id,
		    rc);
		DPAA2_CMD_RC_CLOSE(rcdev, child, &cmd);
		return (ENXIO);
	}
	if (bootverbose) {
		device_printf(rcdev, "Isolation context ID: %u\n",
		    dprc_attr.icid);
	}
	if (rcinfo) {
		rcinfo->id = dprc_attr.cont_id;
		rcinfo->portal_id = dprc_attr.portal_id;
		rcinfo->icid = dprc_attr.icid;
	}

	/*
	 * Add MC portals before everything else.
	 * TODO: Discover DPAA2 objects on-demand.
	 */
	for (uint32_t i = 0; i < obj_count; i++) {
		rc = DPAA2_CMD_RC_GET_OBJ(rcdev, child, &cmd, i, &obj);
		if (rc) {
			continue; /* Skip silently for now. */
		}
		if (obj.type != DPAA2_DEV_MCP) {
			continue;
		}
		dpaa2_rc_add_managed_child(sc, &cmd, &obj);
	}
	/* Probe and attach MC portals. */
	bus_identify_children(rcdev);
	bus_attach_children(rcdev);

	/* Add managed devices (except DPMCPs) to the resource container. */
	for (uint32_t i = 0; i < obj_count; i++) {
		rc = DPAA2_CMD_RC_GET_OBJ(rcdev, child, &cmd, i, &obj);
		if (rc && bootverbose) {
			if (rc == DPAA2_CMD_STAT_UNKNOWN_OBJ) {
				device_printf(rcdev, "%s: skip unsupported "
				    "DPAA2 object: idx=%u\n", __func__, i);
				continue;
			} else {
				device_printf(rcdev, "%s: failed to get "
				    "information about DPAA2 object: idx=%u, "
				    "error=%d\n", __func__, i, rc);
				continue;
			}
		}
		if (obj.type == DPAA2_DEV_MCP) {
			continue; /* Already added. */
		}
		dpaa2_rc_add_managed_child(sc, &cmd, &obj);
	}
	/* Probe and attach managed devices properly. */
	bus_identify_children(rcdev);
	bus_attach_children(rcdev);

	/* Add other devices to the resource container. */
	for (uint32_t i = 0; i < obj_count; i++) {
		rc = DPAA2_CMD_RC_GET_OBJ(rcdev, child, &cmd, i, &obj);
		if (rc == DPAA2_CMD_STAT_UNKNOWN_OBJ && bootverbose) {
			device_printf(rcdev, "%s: skip unsupported DPAA2 "
			    "object: idx=%u\n", __func__, i);
			continue;
		} else if (rc) {
			device_printf(rcdev, "%s: failed to get object: "
			    "idx=%u, error=%d\n", __func__, i, rc);
			continue;
		}
		dpaa2_rc_add_child(sc, &cmd, &obj);
	}

	DPAA2_CMD_RC_CLOSE(rcdev, child, &cmd);

	/* Probe and attach the rest of devices. */
	bus_identify_children(rcdev);
	bus_attach_children(rcdev);
	return (0);
}

/**
 * @brief Add a new DPAA2 device to the resource container bus.
 */
static int
dpaa2_rc_add_child(struct dpaa2_rc_softc *sc, struct dpaa2_cmd *cmd,
    struct dpaa2_obj *obj)
{
	device_t rcdev, dev;
	struct dpaa2_devinfo *rcinfo;
	struct dpaa2_devinfo *dinfo;
	struct resource_spec *res_spec;
	const char *devclass;
	int dpio_n = 0; /* to limit DPIOs by # of CPUs */
	int dpcon_n = 0; /* to limit DPCONs by # of CPUs */
	int rid, error;

	rcdev = sc->dev;
	rcinfo = device_get_ivars(rcdev);

	switch (obj->type) {
	case DPAA2_DEV_NI:
		devclass = "dpaa2_ni";
		res_spec = dpaa2_ni_spec;
		break;
	default:
		return (ENXIO);
	}

	/* Add a device for the DPAA2 object. */
	dev = device_add_child(rcdev, devclass, DEVICE_UNIT_ANY);
	if (dev == NULL) {
		device_printf(rcdev, "%s: failed to add a device for DPAA2 "
		    "object: type=%s, id=%u\n", __func__, dpaa2_ttos(obj->type),
		    obj->id);
		return (ENXIO);
	}

	/* Allocate devinfo for a child. */
	dinfo = malloc(sizeof(struct dpaa2_devinfo), M_DPAA2_RC,
	    M_WAITOK | M_ZERO);
	if (!dinfo) {
		device_printf(rcdev, "%s: failed to allocate dpaa2_devinfo "
		    "for: type=%s, id=%u\n", __func__, dpaa2_ttos(obj->type),
		    obj->id);
		return (ENXIO);
	}
	device_set_ivars(dev, dinfo);

	dinfo->pdev = rcdev;
	dinfo->dev = dev;
	dinfo->id = obj->id;
	dinfo->dtype = obj->type;
	dinfo->portal = NULL;
	/* Children share their parent container's ICID and portal ID. */
	dinfo->icid = rcinfo->icid;
	dinfo->portal_id = rcinfo->portal_id;
	/* MSI configuration */
	dinfo->msi.msi_msgnum = obj->irq_count;
	dinfo->msi.msi_alloc = 0;
	dinfo->msi.msi_handlers = 0;

	/* Initialize a resource list for the child. */
	resource_list_init(&dinfo->resources);

	/* Add DPAA2-specific resources to the resource list. */
	for (; res_spec && res_spec->type != -1; res_spec++) {
		if (res_spec->type < DPAA2_DEV_MC)
			continue; /* Skip non-DPAA2 resource. */
		rid = res_spec->rid;

		/* Limit DPIOs and DPCONs by number of CPUs. */
		if (res_spec->type == DPAA2_DEV_IO && dpio_n >= mp_ncpus) {
			dpio_n++;
			continue;
		}
		if (res_spec->type == DPAA2_DEV_CON && dpcon_n >= mp_ncpus) {
			dpcon_n++;
			continue;
		}

		error = dpaa2_rc_add_res(rcdev, dev, res_spec->type, &rid,
		    res_spec->flags);
		if (error)
			device_printf(rcdev, "%s: dpaa2_rc_add_res() failed: "
			    "error=%d\n", __func__, error);

		if (res_spec->type == DPAA2_DEV_IO)
			dpio_n++;
		if (res_spec->type == DPAA2_DEV_CON)
			dpcon_n++;
	}

	return (0);
}

/**
 * @brief Add a new managed DPAA2 device to the resource container bus.
 *
 * There are DPAA2 objects (DPIO, DPBP) which have their own drivers and can be
 * allocated as resources or associated with the other DPAA2 objects. This
 * function is supposed to discover such managed objects in the resource
 * container and add them as children to perform a proper initialization.
 *
 * NOTE: It must be called together with bus_identify_children() and
 *       bus_attach_children() before dpaa2_rc_add_child().
 */
static int
dpaa2_rc_add_managed_child(struct dpaa2_rc_softc *sc, struct dpaa2_cmd *cmd,
    struct dpaa2_obj *obj)
{
	device_t rcdev, dev, child;
	struct dpaa2_devinfo *rcinfo, *dinfo;
	struct dpaa2_rc_obj_region reg;
	struct resource_spec *res_spec;
	const char *devclass;
	uint64_t start, end, count;
	uint32_t flags = 0;
	int rid, error;

	rcdev = sc->dev;
	child = sc->dev;
	rcinfo = device_get_ivars(rcdev);

	switch (obj->type) {
	case DPAA2_DEV_IO:
		devclass = "dpaa2_io";
		res_spec = dpaa2_io_spec;
		flags = DPAA2_MC_DEV_ALLOCATABLE | DPAA2_MC_DEV_SHAREABLE;
		break;
	case DPAA2_DEV_BP:
		devclass = "dpaa2_bp";
		res_spec = dpaa2_bp_spec;
		flags = DPAA2_MC_DEV_ALLOCATABLE;
		break;
	case DPAA2_DEV_CON:
		devclass = "dpaa2_con";
		res_spec = dpaa2_con_spec;
		flags = DPAA2_MC_DEV_ALLOCATABLE;
		break;
	case DPAA2_DEV_MAC:
		devclass = "dpaa2_mac";
		res_spec = dpaa2_mac_spec;
		flags = DPAA2_MC_DEV_ASSOCIATED;
		break;
	case DPAA2_DEV_MCP:
		devclass = "dpaa2_mcp";
		res_spec = NULL;
		flags = DPAA2_MC_DEV_ALLOCATABLE | DPAA2_MC_DEV_SHAREABLE;
		break;
	default:
		/* Only managed devices above are supported. */
		return (EINVAL);
	}

	/* Add a device for the DPAA2 object. */
	dev = device_add_child(rcdev, devclass, DEVICE_UNIT_ANY);
	if (dev == NULL) {
		device_printf(rcdev, "%s: failed to add a device for DPAA2 "
		    "object: type=%s, id=%u\n", __func__, dpaa2_ttos(obj->type),
		    obj->id);
		return (ENXIO);
	}

	/* Allocate devinfo for the child. */
	dinfo = malloc(sizeof(struct dpaa2_devinfo), M_DPAA2_RC,
	    M_WAITOK | M_ZERO);
	if (!dinfo) {
		device_printf(rcdev, "%s: failed to allocate dpaa2_devinfo "
		    "for: type=%s, id=%u\n", __func__, dpaa2_ttos(obj->type),
		    obj->id);
		return (ENXIO);
	}
	device_set_ivars(dev, dinfo);

	dinfo->pdev = rcdev;
	dinfo->dev = dev;
	dinfo->id = obj->id;
	dinfo->dtype = obj->type;
	dinfo->portal = NULL;
	/* Children share their parent container's ICID and portal ID. */
	dinfo->icid = rcinfo->icid;
	dinfo->portal_id = rcinfo->portal_id;
	/* MSI configuration */
	dinfo->msi.msi_msgnum = obj->irq_count;
	dinfo->msi.msi_alloc = 0;
	dinfo->msi.msi_handlers = 0;

	/* Initialize a resource list for the child. */
	resource_list_init(&dinfo->resources);

	/* Add memory regions to the resource list. */
	for (uint8_t i = 0; i < obj->reg_count; i++) {
		error = DPAA2_CMD_RC_GET_OBJ_REGION(rcdev, child, cmd, obj->id,
		    i, obj->type, &reg);
		if (error) {
			device_printf(rcdev, "%s: failed to obtain memory "
			    "region for type=%s, id=%u, reg_idx=%u: error=%d\n",
			    __func__, dpaa2_ttos(obj->type), obj->id, i, error);
			continue;
		}
		count = reg.size;
		start = reg.base_paddr + reg.base_offset;
		end = reg.base_paddr + reg.base_offset + reg.size - 1;

		resource_list_add(&dinfo->resources, SYS_RES_MEMORY, i, start,
		    end, count);
	}

	/* Add DPAA2-specific resources to the resource list. */
	for (; res_spec && res_spec->type != -1; res_spec++) {
		if (res_spec->type < DPAA2_DEV_MC)
			continue; /* Skip non-DPAA2 resource. */
		rid = res_spec->rid;

		error = dpaa2_rc_add_res(rcdev, dev, res_spec->type, &rid,
		    res_spec->flags);
		if (error)
			device_printf(rcdev, "%s: dpaa2_rc_add_res() failed: "
			    "error=%d\n", __func__, error);
	}

	/* Inform MC about a new managed device. */
	error = DPAA2_MC_MANAGE_DEV(rcdev, dev, flags);
	if (error) {
		device_printf(rcdev, "%s: failed to add a managed DPAA2 device: "
		    "type=%s, id=%u, error=%d\n", __func__,
		    dpaa2_ttos(obj->type), obj->id, error);
		return (ENXIO);
	}

	return (0);
}

/**
 * @brief Configure given IRQ using MC command interface.
 */
static int
dpaa2_rc_configure_irq(device_t rcdev, device_t child, int rid, uint64_t addr,
    uint32_t data)
{
	struct dpaa2_devinfo *rcinfo;
	struct dpaa2_devinfo *dinfo;
	struct dpaa2_cmd cmd;
	uint16_t rc_token;
	int rc = EINVAL;

	DPAA2_CMD_INIT(&cmd);

	if (device_get_parent(child) == rcdev && rid >= 1) {
		rcinfo = device_get_ivars(rcdev);
		dinfo = device_get_ivars(child);

		rc = DPAA2_CMD_RC_OPEN(rcdev, child, &cmd, rcinfo->id,
		    &rc_token);
		if (rc) {
			device_printf(rcdev, "%s: failed to open DPRC: "
			    "error=%d\n", __func__, rc);
			return (ENODEV);
		}
		/* Set MSI address and value. */
		rc = DPAA2_CMD_RC_SET_OBJ_IRQ(rcdev, child, &cmd, rid - 1, addr,
		    data, rid, dinfo->id, dinfo->dtype);
		if (rc) {
			device_printf(rcdev, "%s: failed to setup IRQ: "
			    "rid=%d, addr=%jx, data=%x, error=%d\n", __func__,
			    rid, addr, data, rc);
			return (ENODEV);
		}
		rc = DPAA2_CMD_RC_CLOSE(rcdev, child, &cmd);
		if (rc) {
			device_printf(rcdev, "%s: failed to close DPRC: "
			    "error=%d\n", __func__, rc);
			return (ENODEV);
		}
		rc = 0;
	}

	return (rc);
}

/**
 * @brief General implementation of the MC command to enable IRQ.
 */
static int
dpaa2_rc_enable_irq(struct dpaa2_mcp *mcp, struct dpaa2_cmd *cmd,
    uint8_t irq_idx, bool enable, uint16_t cmdid)
{
	struct __packed enable_irq_args {
		uint8_t		enable;
		uint8_t		_reserved1;
		uint16_t	_reserved2;
		uint8_t		irq_idx;
		uint8_t		_reserved3;
		uint16_t	_reserved4;
		uint64_t	_reserved5[6];
	} *args;

	if (!mcp || !cmd)
		return (DPAA2_CMD_STAT_ERR);

	args = (struct enable_irq_args *) &cmd->params[0];
	args->irq_idx = irq_idx;
	args->enable = enable == 0u ? 0u : 1u;

	return (dpaa2_rc_exec_cmd(mcp, cmd, cmdid));
}

/**
 * @brief Sends a command to MC and waits for response.
 */
static int
dpaa2_rc_exec_cmd(struct dpaa2_mcp *mcp, struct dpaa2_cmd *cmd, uint16_t cmdid)
{
	struct dpaa2_cmd_header *hdr;
	uint16_t flags;
	int error;

	if (!mcp || !cmd)
		return (DPAA2_CMD_STAT_ERR);

	/* Prepare a command for the MC hardware. */
	hdr = (struct dpaa2_cmd_header *) &cmd->header;
	hdr->cmdid = cmdid;
	hdr->status = DPAA2_CMD_STAT_READY;

	DPAA2_MCP_LOCK(mcp, &flags);
	if (flags & DPAA2_PORTAL_DESTROYED) {
		/* Terminate operation if portal is destroyed. */
		DPAA2_MCP_UNLOCK(mcp);
		return (DPAA2_CMD_STAT_INVALID_STATE);
	}

	/* Send a command to MC and wait for the result. */
	dpaa2_rc_send_cmd(mcp, cmd);
	error = dpaa2_rc_wait_for_cmd(mcp, cmd);
	if (error) {
		DPAA2_MCP_UNLOCK(mcp);
		return (DPAA2_CMD_STAT_ERR);
	}
	if (hdr->status != DPAA2_CMD_STAT_OK) {
		DPAA2_MCP_UNLOCK(mcp);
		return (int)(hdr->status);
	}

	DPAA2_MCP_UNLOCK(mcp);

	return (DPAA2_CMD_STAT_OK);
}

/**
 * @brief Writes a command to the MC command portal.
 */
static int
dpaa2_rc_send_cmd(struct dpaa2_mcp *mcp, struct dpaa2_cmd *cmd)
{
	/* Write command parameters. */
	for (uint32_t i = 1; i <= DPAA2_CMD_PARAMS_N; i++)
		bus_write_8(mcp->map, sizeof(uint64_t) * i, cmd->params[i-1]);

	bus_barrier(mcp->map, 0, sizeof(struct dpaa2_cmd),
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

	/* Write command header to trigger execution. */
	bus_write_8(mcp->map, 0, cmd->header);

	return (0);
}

/**
 * @brief Polls the MC command portal in order to receive a result of the
 *        command execution.
 */
static int
dpaa2_rc_wait_for_cmd(struct dpaa2_mcp *mcp, struct dpaa2_cmd *cmd)
{
	struct dpaa2_cmd_header *hdr;
	uint64_t val;
	uint32_t i;

	/* Wait for a command execution result from the MC hardware. */
	for (i = 1; i <= CMD_SPIN_ATTEMPTS; i++) {
		val = bus_read_8(mcp->map, 0);
		hdr = (struct dpaa2_cmd_header *) &val;
		if (hdr->status != DPAA2_CMD_STAT_READY) {
			break;
		}
		DELAY(CMD_SPIN_TIMEOUT);
	}

	if (i > CMD_SPIN_ATTEMPTS) {
		/* Return an error on expired timeout. */
		return (DPAA2_CMD_STAT_TIMEOUT);
	} else {
		/* Read command response. */
		cmd->header = val;
		for (i = 1; i <= DPAA2_CMD_PARAMS_N; i++) {
			cmd->params[i-1] =
			    bus_read_8(mcp->map, i * sizeof(uint64_t));
		}
	}

	return (DPAA2_CMD_STAT_OK);
}

/**
 * @brief Reserve a DPAA2-specific device of the given devtype for the child.
 */
static int
dpaa2_rc_add_res(device_t rcdev, device_t child, enum dpaa2_dev_type devtype,
    int *rid, int flags)
{
	device_t dpaa2_dev;
	struct dpaa2_devinfo *dinfo = device_get_ivars(child);
	struct resource *res;
	bool shared = false;
	int error;

	/* Request a free DPAA2 device of the given type from MC. */
	error = DPAA2_MC_GET_FREE_DEV(rcdev, &dpaa2_dev, devtype);
	if (error && !(flags & RF_SHAREABLE)) {
		device_printf(rcdev, "%s: failed to obtain a free %s (rid=%d) "
		    "for: %s (id=%u)\n", __func__, dpaa2_ttos(devtype), *rid,
		    dpaa2_ttos(dinfo->dtype), dinfo->id);
		return (error);
	}

	/* Request a shared DPAA2 device of the given type from MC. */
	if (error) {
		error = DPAA2_MC_GET_SHARED_DEV(rcdev, &dpaa2_dev, devtype);
		if (error) {
			device_printf(rcdev, "%s: failed to obtain a shared "
			    "%s (rid=%d) for: %s (id=%u)\n", __func__,
			    dpaa2_ttos(devtype), *rid, dpaa2_ttos(dinfo->dtype),
			    dinfo->id);
			return (error);
		}
		shared = true;
	}

	/* Add DPAA2 device to the resource list of the child device. */
	resource_list_add(&dinfo->resources, devtype, *rid,
	    (rman_res_t) dpaa2_dev, (rman_res_t) dpaa2_dev, 1);

	/* Reserve a newly added DPAA2 resource. */
	res = resource_list_reserve(&dinfo->resources, rcdev, child, devtype,
	    rid, (rman_res_t) dpaa2_dev, (rman_res_t) dpaa2_dev, 1,
	    flags & ~RF_ACTIVE);
	if (!res) {
		device_printf(rcdev, "%s: failed to reserve %s (rid=%d) for: %s "
		    "(id=%u)\n", __func__, dpaa2_ttos(devtype), *rid,
		    dpaa2_ttos(dinfo->dtype), dinfo->id);
		return (EBUSY);
	}

	/* Reserve a shared DPAA2 device of the given type. */
	if (shared) {
		error = DPAA2_MC_RESERVE_DEV(rcdev, dpaa2_dev, devtype);
		if (error) {
			device_printf(rcdev, "%s: failed to reserve a shared "
			    "%s (rid=%d) for: %s (id=%u)\n", __func__,
			    dpaa2_ttos(devtype), *rid, dpaa2_ttos(dinfo->dtype),
			    dinfo->id);
			return (error);
		}
	}

	return (0);
}

static int
dpaa2_rc_print_type(struct resource_list *rl, enum dpaa2_dev_type type)
{
	struct dpaa2_devinfo *dinfo;
	struct resource_list_entry *rle;
	uint32_t prev_id;
	int printed = 0, series = 0;
	int retval = 0;

	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type == type) {
			dinfo = device_get_ivars((device_t) rle->start);

			if (printed == 0) {
				retval += printf(" %s (id=",
				    dpaa2_ttos(dinfo->dtype));
			} else {
				if (dinfo->id == prev_id + 1) {
					if (series == 0) {
						series = 1;
						retval += printf("-");
					}
				} else {
					if (series == 1) {
						retval += printf("%u", prev_id);
						series = 0;
					}
					retval += printf(",");
				}
			}
			printed++;

			if (series == 0)
				retval += printf("%u", dinfo->id);
			prev_id = dinfo->id;
		}
	}
	if (printed) {
		if (series == 1)
			retval += printf("%u", prev_id);
		retval += printf(")");
	}

	return (retval);
}

static int
dpaa2_rc_reset_cmd_params(struct dpaa2_cmd *cmd)
{
	if (cmd != NULL) {
		memset(cmd->params, 0, sizeof(cmd->params[0]) *
		    DPAA2_CMD_PARAMS_N);
	}
	return (0);
}

static struct dpaa2_mcp *
dpaa2_rc_select_portal(device_t dev, device_t child)
{
	struct dpaa2_devinfo *dinfo = device_get_ivars(dev);
	struct dpaa2_devinfo *cinfo = device_get_ivars(child);

	if (cinfo == NULL || dinfo == NULL || dinfo->dtype != DPAA2_DEV_RC)
		return (NULL);
	return (cinfo->portal != NULL ? cinfo->portal : dinfo->portal);
}

static device_method_t dpaa2_rc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			dpaa2_rc_probe),
	DEVMETHOD(device_attach,		dpaa2_rc_attach),
	DEVMETHOD(device_detach,		dpaa2_rc_detach),

	/* Bus interface */
	DEVMETHOD(bus_get_resource_list,	dpaa2_rc_get_resource_list),
	DEVMETHOD(bus_delete_resource,		dpaa2_rc_delete_resource),
	DEVMETHOD(bus_alloc_resource,		dpaa2_rc_alloc_resource),
	DEVMETHOD(bus_release_resource,		dpaa2_rc_release_resource),
	DEVMETHOD(bus_child_deleted,		dpaa2_rc_child_deleted),
	DEVMETHOD(bus_child_detached,		dpaa2_rc_child_detached),
	DEVMETHOD(bus_setup_intr,		dpaa2_rc_setup_intr),
	DEVMETHOD(bus_teardown_intr,		dpaa2_rc_teardown_intr),
	DEVMETHOD(bus_print_child,		dpaa2_rc_print_child),
	DEVMETHOD(bus_add_child,		device_add_child_ordered),
	DEVMETHOD(bus_set_resource,		bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,		bus_generic_rl_get_resource),
	DEVMETHOD(bus_activate_resource, 	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, 	bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,		bus_generic_adjust_resource),

	/* Pseudo-PCI interface */
	DEVMETHOD(pci_alloc_msi,		dpaa2_rc_alloc_msi),
	DEVMETHOD(pci_release_msi,		dpaa2_rc_release_msi),
	DEVMETHOD(pci_msi_count,		dpaa2_rc_msi_count),
	DEVMETHOD(pci_get_id,			dpaa2_rc_get_id),

	/* DPAA2 MC command interface */
	DEVMETHOD(dpaa2_cmd_mng_get_version,	dpaa2_rc_mng_get_version),
	DEVMETHOD(dpaa2_cmd_mng_get_soc_version, dpaa2_rc_mng_get_soc_version),
	DEVMETHOD(dpaa2_cmd_mng_get_container_id, dpaa2_rc_mng_get_container_id),
	/*	DPRC commands */
	DEVMETHOD(dpaa2_cmd_rc_open,		dpaa2_rc_open),
	DEVMETHOD(dpaa2_cmd_rc_close,		dpaa2_rc_close),
	DEVMETHOD(dpaa2_cmd_rc_get_obj_count,	dpaa2_rc_get_obj_count),
	DEVMETHOD(dpaa2_cmd_rc_get_obj,		dpaa2_rc_get_obj),
	DEVMETHOD(dpaa2_cmd_rc_get_obj_descriptor, dpaa2_rc_get_obj_descriptor),
	DEVMETHOD(dpaa2_cmd_rc_get_attributes,	dpaa2_rc_get_attributes),
	DEVMETHOD(dpaa2_cmd_rc_get_obj_region,	dpaa2_rc_get_obj_region),
	DEVMETHOD(dpaa2_cmd_rc_get_api_version, dpaa2_rc_get_api_version),
	DEVMETHOD(dpaa2_cmd_rc_set_irq_enable,	dpaa2_rc_set_irq_enable),
	DEVMETHOD(dpaa2_cmd_rc_set_obj_irq,	dpaa2_rc_set_obj_irq),
	DEVMETHOD(dpaa2_cmd_rc_get_conn,	dpaa2_rc_get_conn),
	/*	DPNI commands */
	DEVMETHOD(dpaa2_cmd_ni_open,		dpaa2_rc_ni_open),
	DEVMETHOD(dpaa2_cmd_ni_close,		dpaa2_rc_ni_close),
	DEVMETHOD(dpaa2_cmd_ni_enable,		dpaa2_rc_ni_enable),
	DEVMETHOD(dpaa2_cmd_ni_disable,		dpaa2_rc_ni_disable),
	DEVMETHOD(dpaa2_cmd_ni_get_api_version,	dpaa2_rc_ni_get_api_version),
	DEVMETHOD(dpaa2_cmd_ni_reset,		dpaa2_rc_ni_reset),
	DEVMETHOD(dpaa2_cmd_ni_get_attributes,	dpaa2_rc_ni_get_attributes),
	DEVMETHOD(dpaa2_cmd_ni_set_buf_layout,	dpaa2_rc_ni_set_buf_layout),
	DEVMETHOD(dpaa2_cmd_ni_get_tx_data_off, dpaa2_rc_ni_get_tx_data_offset),
	DEVMETHOD(dpaa2_cmd_ni_get_port_mac_addr, dpaa2_rc_ni_get_port_mac_addr),
	DEVMETHOD(dpaa2_cmd_ni_set_prim_mac_addr, dpaa2_rc_ni_set_prim_mac_addr),
	DEVMETHOD(dpaa2_cmd_ni_get_prim_mac_addr, dpaa2_rc_ni_get_prim_mac_addr),
	DEVMETHOD(dpaa2_cmd_ni_set_link_cfg,	dpaa2_rc_ni_set_link_cfg),
	DEVMETHOD(dpaa2_cmd_ni_get_link_cfg,	dpaa2_rc_ni_get_link_cfg),
	DEVMETHOD(dpaa2_cmd_ni_get_link_state,	dpaa2_rc_ni_get_link_state),
	DEVMETHOD(dpaa2_cmd_ni_set_qos_table,	dpaa2_rc_ni_set_qos_table),
	DEVMETHOD(dpaa2_cmd_ni_clear_qos_table, dpaa2_rc_ni_clear_qos_table),
	DEVMETHOD(dpaa2_cmd_ni_set_pools,	dpaa2_rc_ni_set_pools),
	DEVMETHOD(dpaa2_cmd_ni_set_err_behavior,dpaa2_rc_ni_set_err_behavior),
	DEVMETHOD(dpaa2_cmd_ni_get_queue,	dpaa2_rc_ni_get_queue),
	DEVMETHOD(dpaa2_cmd_ni_set_queue,	dpaa2_rc_ni_set_queue),
	DEVMETHOD(dpaa2_cmd_ni_get_qdid,	dpaa2_rc_ni_get_qdid),
	DEVMETHOD(dpaa2_cmd_ni_add_mac_addr,	dpaa2_rc_ni_add_mac_addr),
	DEVMETHOD(dpaa2_cmd_ni_remove_mac_addr,	dpaa2_rc_ni_remove_mac_addr),
	DEVMETHOD(dpaa2_cmd_ni_clear_mac_filters, dpaa2_rc_ni_clear_mac_filters),
	DEVMETHOD(dpaa2_cmd_ni_set_mfl,		dpaa2_rc_ni_set_mfl),
	DEVMETHOD(dpaa2_cmd_ni_set_offload,	dpaa2_rc_ni_set_offload),
	DEVMETHOD(dpaa2_cmd_ni_set_irq_mask,	dpaa2_rc_ni_set_irq_mask),
	DEVMETHOD(dpaa2_cmd_ni_set_irq_enable,	dpaa2_rc_ni_set_irq_enable),
	DEVMETHOD(dpaa2_cmd_ni_get_irq_status,	dpaa2_rc_ni_get_irq_status),
	DEVMETHOD(dpaa2_cmd_ni_set_uni_promisc,	dpaa2_rc_ni_set_uni_promisc),
	DEVMETHOD(dpaa2_cmd_ni_set_multi_promisc, dpaa2_rc_ni_set_multi_promisc),
	DEVMETHOD(dpaa2_cmd_ni_get_statistics,	dpaa2_rc_ni_get_statistics),
	DEVMETHOD(dpaa2_cmd_ni_set_rx_tc_dist,	dpaa2_rc_ni_set_rx_tc_dist),
	/*	DPIO commands */
	DEVMETHOD(dpaa2_cmd_io_open,		dpaa2_rc_io_open),
	DEVMETHOD(dpaa2_cmd_io_close,		dpaa2_rc_io_close),
	DEVMETHOD(dpaa2_cmd_io_enable,		dpaa2_rc_io_enable),
	DEVMETHOD(dpaa2_cmd_io_disable,		dpaa2_rc_io_disable),
	DEVMETHOD(dpaa2_cmd_io_reset,		dpaa2_rc_io_reset),
	DEVMETHOD(dpaa2_cmd_io_get_attributes,	dpaa2_rc_io_get_attributes),
	DEVMETHOD(dpaa2_cmd_io_set_irq_mask,	dpaa2_rc_io_set_irq_mask),
	DEVMETHOD(dpaa2_cmd_io_get_irq_status,	dpaa2_rc_io_get_irq_status),
	DEVMETHOD(dpaa2_cmd_io_set_irq_enable,	dpaa2_rc_io_set_irq_enable),
	DEVMETHOD(dpaa2_cmd_io_add_static_dq_chan, dpaa2_rc_io_add_static_dq_chan),
	/*	DPBP commands */
	DEVMETHOD(dpaa2_cmd_bp_open,		dpaa2_rc_bp_open),
	DEVMETHOD(dpaa2_cmd_bp_close,		dpaa2_rc_bp_close),
	DEVMETHOD(dpaa2_cmd_bp_enable,		dpaa2_rc_bp_enable),
	DEVMETHOD(dpaa2_cmd_bp_disable,		dpaa2_rc_bp_disable),
	DEVMETHOD(dpaa2_cmd_bp_reset,		dpaa2_rc_bp_reset),
	DEVMETHOD(dpaa2_cmd_bp_get_attributes,	dpaa2_rc_bp_get_attributes),
	/*	DPMAC commands */
	DEVMETHOD(dpaa2_cmd_mac_open,		dpaa2_rc_mac_open),
	DEVMETHOD(dpaa2_cmd_mac_close,		dpaa2_rc_mac_close),
	DEVMETHOD(dpaa2_cmd_mac_reset,		dpaa2_rc_mac_reset),
	DEVMETHOD(dpaa2_cmd_mac_mdio_read,	dpaa2_rc_mac_mdio_read),
	DEVMETHOD(dpaa2_cmd_mac_mdio_write,	dpaa2_rc_mac_mdio_write),
	DEVMETHOD(dpaa2_cmd_mac_get_addr,	dpaa2_rc_mac_get_addr),
	DEVMETHOD(dpaa2_cmd_mac_get_attributes, dpaa2_rc_mac_get_attributes),
	DEVMETHOD(dpaa2_cmd_mac_set_link_state,	dpaa2_rc_mac_set_link_state),
	DEVMETHOD(dpaa2_cmd_mac_set_irq_mask,	dpaa2_rc_mac_set_irq_mask),
	DEVMETHOD(dpaa2_cmd_mac_set_irq_enable,	dpaa2_rc_mac_set_irq_enable),
	DEVMETHOD(dpaa2_cmd_mac_get_irq_status,	dpaa2_rc_mac_get_irq_status),
	/*	DPCON commands */
	DEVMETHOD(dpaa2_cmd_con_open,		dpaa2_rc_con_open),
	DEVMETHOD(dpaa2_cmd_con_close,		dpaa2_rc_con_close),
	DEVMETHOD(dpaa2_cmd_con_reset,		dpaa2_rc_con_reset),
	DEVMETHOD(dpaa2_cmd_con_enable,		dpaa2_rc_con_enable),
	DEVMETHOD(dpaa2_cmd_con_disable,	dpaa2_rc_con_disable),
	DEVMETHOD(dpaa2_cmd_con_get_attributes,	dpaa2_rc_con_get_attributes),
	DEVMETHOD(dpaa2_cmd_con_set_notif,	dpaa2_rc_con_set_notif),
	/*	DPMCP commands */
	DEVMETHOD(dpaa2_cmd_mcp_create,		dpaa2_rc_mcp_create),
	DEVMETHOD(dpaa2_cmd_mcp_destroy,	dpaa2_rc_mcp_destroy),
	DEVMETHOD(dpaa2_cmd_mcp_open,		dpaa2_rc_mcp_open),
	DEVMETHOD(dpaa2_cmd_mcp_close,		dpaa2_rc_mcp_close),
	DEVMETHOD(dpaa2_cmd_mcp_reset,		dpaa2_rc_mcp_reset),

	DEVMETHOD_END
};

static driver_t dpaa2_rc_driver = {
	"dpaa2_rc",
	dpaa2_rc_methods,
	sizeof(struct dpaa2_rc_softc),
};

/* For root container */
DRIVER_MODULE(dpaa2_rc, dpaa2_mc, dpaa2_rc_driver, 0, 0);
/* For child containers */
DRIVER_MODULE(dpaa2_rc, dpaa2_rc, dpaa2_rc_driver, 0, 0);
