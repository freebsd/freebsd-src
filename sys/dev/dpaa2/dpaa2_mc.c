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
 * The DPAA2 Management Complex (MC) bus driver.
 *
 * MC is a hardware resource manager which can be found in several NXP
 * SoCs (LX2160A, for example) and provides an access to the specialized
 * hardware objects used in network-oriented packet processing applications.
 */

#include "opt_acpi.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#endif

#include "pcib_if.h"
#include "pci_if.h"

#include "dpaa2_mc.h"

/* Macros to read/write MC registers */
#define	mcreg_read_4(_sc, _r)		bus_read_4(&(_sc)->map[1], (_r))
#define	mcreg_write_4(_sc, _r, _v)	bus_write_4(&(_sc)->map[1], (_r), (_v))

#define IORT_DEVICE_NAME		"MCE"

/* MC Registers */
#define MC_REG_GCR1			0x0000u
#define MC_REG_GCR2			0x0004u /* TODO: Does it exist? */
#define MC_REG_GSR			0x0008u
#define MC_REG_FAPR			0x0028u

/* General Control Register 1 (GCR1) */
#define GCR1_P1_STOP			0x80000000u
#define GCR1_P2_STOP			0x40000000u

/* General Status Register (GSR) */
#define GSR_HW_ERR(v)			(((v) & 0x80000000u) >> 31)
#define GSR_CAT_ERR(v)			(((v) & 0x40000000u) >> 30)
#define GSR_DPL_OFFSET(v)		(((v) & 0x3FFFFF00u) >> 8)
#define GSR_MCS(v)			(((v) & 0xFFu) >> 0)

/* Timeouts to wait for the MC status. */
#define MC_STAT_TIMEOUT			1000u	/* us */
#define MC_STAT_ATTEMPTS		100u

/**
 * @brief Structure to describe a DPAA2 device as a managed resource.
 */
struct dpaa2_mc_devinfo {
	STAILQ_ENTRY(dpaa2_mc_devinfo) link;
	device_t	dpaa2_dev;
	uint32_t	flags;
	uint32_t	owners;
};

MALLOC_DEFINE(M_DPAA2_MC, "dpaa2_mc", "DPAA2 Management Complex");

static struct resource_spec dpaa2_mc_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE | RF_UNMAPPED },
	{ SYS_RES_MEMORY, 1, RF_ACTIVE | RF_UNMAPPED | RF_OPTIONAL },
	RESOURCE_SPEC_END
};

static u_int dpaa2_mc_get_xref(device_t, device_t);
static u_int dpaa2_mc_map_id(device_t, device_t, uintptr_t *);

static int dpaa2_mc_alloc_msi_impl(device_t, device_t, int, int, int *);
static int dpaa2_mc_release_msi_impl(device_t, device_t, int, int *);
static int dpaa2_mc_map_msi_impl(device_t, device_t, int, uint64_t *,
    uint32_t *);

/*
 * For device interface.
 */

int
dpaa2_mc_attach(device_t dev)
{
	struct dpaa2_mc_softc *sc;
	struct resource_map_request req;
	uint32_t val;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->msi_allocated = false;
	sc->msi_owner = NULL;

	error = bus_alloc_resources(sc->dev, dpaa2_mc_spec, sc->res);
	if (error) {
		device_printf(dev, "%s: failed to allocate resources\n",
		    __func__);
		return (ENXIO);
	}

	if (sc->res[1]) {
		resource_init_map_request(&req);
		req.memattr = VM_MEMATTR_DEVICE;
		error = bus_map_resource(sc->dev, SYS_RES_MEMORY, sc->res[1],
		    &req, &sc->map[1]);
		if (error) {
			device_printf(dev, "%s: failed to map control "
			    "registers\n", __func__);
			dpaa2_mc_detach(dev);
			return (ENXIO);
		}

		if (bootverbose)
			device_printf(dev,
			    "GCR1=0x%x, GCR2=0x%x, GSR=0x%x, FAPR=0x%x\n",
			    mcreg_read_4(sc, MC_REG_GCR1),
			    mcreg_read_4(sc, MC_REG_GCR2),
			    mcreg_read_4(sc, MC_REG_GSR),
			    mcreg_read_4(sc, MC_REG_FAPR));

		/* Reset P1_STOP and P2_STOP bits to resume MC processor. */
		val = mcreg_read_4(sc, MC_REG_GCR1) &
		    ~(GCR1_P1_STOP | GCR1_P2_STOP);
		mcreg_write_4(sc, MC_REG_GCR1, val);

		/* Poll MC status. */
		if (bootverbose)
			device_printf(dev, "polling MC status...\n");
		for (int i = 0; i < MC_STAT_ATTEMPTS; i++) {
			val = mcreg_read_4(sc, MC_REG_GSR);
			if (GSR_MCS(val) != 0u)
				break;
			DELAY(MC_STAT_TIMEOUT);
		}

		if (bootverbose)
			device_printf(dev,
			    "GCR1=0x%x, GCR2=0x%x, GSR=0x%x, FAPR=0x%x\n",
			    mcreg_read_4(sc, MC_REG_GCR1),
			    mcreg_read_4(sc, MC_REG_GCR2),
			    mcreg_read_4(sc, MC_REG_GSR),
			    mcreg_read_4(sc, MC_REG_FAPR));
	}

	/* At least 64 bytes of the command portal should be available. */
	if (rman_get_size(sc->res[0]) < DPAA2_MCP_MEM_WIDTH) {
		device_printf(dev, "%s: MC portal memory region too small: "
		    "%jd\n", __func__, rman_get_size(sc->res[0]));
		dpaa2_mc_detach(dev);
		return (ENXIO);
	}

	/* Map MC portal memory resource. */
	resource_init_map_request(&req);
	req.memattr = VM_MEMATTR_DEVICE;
	error = bus_map_resource(sc->dev, SYS_RES_MEMORY, sc->res[0],
	    &req, &sc->map[0]);
	if (error) {
		device_printf(dev, "Failed to map MC portal memory\n");
		dpaa2_mc_detach(dev);
		return (ENXIO);
	}

	/* Initialize a resource manager for the DPAA2 I/O objects. */
	sc->dpio_rman.rm_type = RMAN_ARRAY;
	sc->dpio_rman.rm_descr = "DPAA2 DPIO objects";
	error = rman_init(&sc->dpio_rman);
	if (error) {
		device_printf(dev, "Failed to initialize a resource manager for "
		    "the DPAA2 I/O objects: error=%d\n", error);
		dpaa2_mc_detach(dev);
		return (ENXIO);
	}

	/* Initialize a resource manager for the DPAA2 buffer pools. */
	sc->dpbp_rman.rm_type = RMAN_ARRAY;
	sc->dpbp_rman.rm_descr = "DPAA2 DPBP objects";
	error = rman_init(&sc->dpbp_rman);
	if (error) {
		device_printf(dev, "Failed to initialize a resource manager for "
		    "the DPAA2 buffer pools: error=%d\n", error);
		dpaa2_mc_detach(dev);
		return (ENXIO);
	}

	/* Initialize a resource manager for the DPAA2 concentrators. */
	sc->dpcon_rman.rm_type = RMAN_ARRAY;
	sc->dpcon_rman.rm_descr = "DPAA2 DPCON objects";
	error = rman_init(&sc->dpcon_rman);
	if (error) {
		device_printf(dev, "Failed to initialize a resource manager for "
		    "the DPAA2 concentrators: error=%d\n", error);
		dpaa2_mc_detach(dev);
		return (ENXIO);
	}

	/* Initialize a resource manager for the DPAA2 MC portals. */
	sc->dpmcp_rman.rm_type = RMAN_ARRAY;
	sc->dpmcp_rman.rm_descr = "DPAA2 DPMCP objects";
	error = rman_init(&sc->dpmcp_rman);
	if (error) {
		device_printf(dev, "Failed to initialize a resource manager for "
		    "the DPAA2 MC portals: error=%d\n", error);
		dpaa2_mc_detach(dev);
		return (ENXIO);
	}

	/* Initialize a list of non-allocatable DPAA2 devices. */
	mtx_init(&sc->mdev_lock, "MC portal mdev lock", NULL, MTX_DEF);
	STAILQ_INIT(&sc->mdev_list);

	mtx_init(&sc->msi_lock, "MC MSI lock", NULL, MTX_DEF);

	/*
	 * Add a root resource container as the only child of the bus. All of
	 * the direct descendant containers will be attached to the root one
	 * instead of the MC device.
	 */
	sc->rcdev = device_add_child(dev, "dpaa2_rc", 0);
	if (sc->rcdev == NULL) {
		dpaa2_mc_detach(dev);
		return (ENXIO);
	}
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

int
dpaa2_mc_detach(device_t dev)
{
	struct dpaa2_mc_softc *sc;
	struct dpaa2_devinfo *dinfo = NULL;
	int error;

	bus_generic_detach(dev);

	sc = device_get_softc(dev);
	if (sc->rcdev)
		device_delete_child(dev, sc->rcdev);
	bus_release_resources(dev, dpaa2_mc_spec, sc->res);

	dinfo = device_get_ivars(dev);
	if (dinfo)
		free(dinfo, M_DPAA2_MC);

	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);

	return (device_delete_children(dev));
}

/*
 * For bus interface.
 */

struct resource *
dpaa2_mc_alloc_resource(device_t mcdev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *res;
	struct rman *rm;
	int error;

	rm = dpaa2_mc_rman(mcdev, type, flags);
	if (rm == NULL)
		return (bus_generic_alloc_resource(mcdev, child, type, rid,
		    start, end, count, flags));

	/*
	 * Skip managing DPAA2-specific resource. It must be provided to MC by
	 * calling DPAA2_MC_MANAGE_DEV() beforehand.
	 */
	if (type <= DPAA2_DEV_MC) {
		error = rman_manage_region(rm, start, end);
		if (error) {
			device_printf(mcdev, "rman_manage_region() failed: "
			    "start=%#jx, end=%#jx, error=%d\n", start, end,
			    error);
			goto fail;
		}
	}

	res = bus_generic_rman_alloc_resource(mcdev, child, type, rid, start,
	    end, count, flags);
	if (res == NULL)
		goto fail;
	return (res);
 fail:
	device_printf(mcdev, "%s() failed: type=%d, rid=%d, start=%#jx, "
	    "end=%#jx, count=%#jx, flags=%x\n", __func__, type, *rid, start, end,
	    count, flags);
	return (NULL);
}

int
dpaa2_mc_adjust_resource(device_t mcdev, device_t child,
    struct resource *r, rman_res_t start, rman_res_t end)
{
	struct rman *rm;

	rm = dpaa2_mc_rman(mcdev, rman_get_type(r), rman_get_flags(r));
	if (rm)
		return (bus_generic_rman_adjust_resource(mcdev, child, r,
		    start, end));
	return (bus_generic_adjust_resource(mcdev, child, r, start, end));
}

int
dpaa2_mc_release_resource(device_t mcdev, device_t child, int type, int rid,
    struct resource *r)
{
	struct rman *rm;

	rm = dpaa2_mc_rman(mcdev, type, rman_get_flags(r));
	if (rm)
		return (bus_generic_rman_release_resource(mcdev, child, type,
		    rid, r));
	return (bus_generic_release_resource(mcdev, child, type, rid, r));
}

int
dpaa2_mc_activate_resource(device_t mcdev, device_t child, struct resource *r)
{
	struct rman *rm;

	rm = dpaa2_mc_rman(mcdev, rman_get_type(r), rman_get_flags(r));
	if (rm)
		return (bus_generic_rman_activate_resource(mcdev, child, r));
	return (bus_generic_activate_resource(mcdev, child, r));
}

int
dpaa2_mc_deactivate_resource(device_t mcdev, device_t child, struct resource *r)
{
	struct rman *rm;

	rm = dpaa2_mc_rman(mcdev, rman_get_type(r), rman_get_flags(r));
	if (rm)
		return (bus_generic_rman_deactivate_resource(mcdev, child, r));
	return (bus_generic_deactivate_resource(mcdev, child, r));
}

/*
 * For pseudo-pcib interface.
 */

int
dpaa2_mc_alloc_msi(device_t mcdev, device_t child, int count, int maxcount,
    int *irqs)
{
#if defined(INTRNG)
	return (dpaa2_mc_alloc_msi_impl(mcdev, child, count, maxcount, irqs));
#else
	return (ENXIO);
#endif
}

int
dpaa2_mc_release_msi(device_t mcdev, device_t child, int count, int *irqs)
{
#if defined(INTRNG)
	return (dpaa2_mc_release_msi_impl(mcdev, child, count, irqs));
#else
	return (ENXIO);
#endif
}

int
dpaa2_mc_map_msi(device_t mcdev, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
#if defined(INTRNG)
	return (dpaa2_mc_map_msi_impl(mcdev, child, irq, addr, data));
#else
	return (ENXIO);
#endif
}

int
dpaa2_mc_get_id(device_t mcdev, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	struct dpaa2_devinfo *dinfo;

	dinfo = device_get_ivars(child);

	if (strcmp(device_get_name(mcdev), "dpaa2_mc") != 0)
		return (ENXIO);

	if (type == PCI_ID_MSI)
		return (dpaa2_mc_map_id(mcdev, child, id));

	*id = dinfo->icid;
	return (0);
}

/*
 * For DPAA2 Management Complex bus driver interface.
 */

int
dpaa2_mc_manage_dev(device_t mcdev, device_t dpaa2_dev, uint32_t flags)
{
	struct dpaa2_mc_softc *sc;
	struct dpaa2_devinfo *dinfo;
	struct dpaa2_mc_devinfo *di;
	struct rman *rm;
	int error;

	sc = device_get_softc(mcdev);
	dinfo = device_get_ivars(dpaa2_dev);

	if (!sc || !dinfo || strcmp(device_get_name(mcdev), "dpaa2_mc") != 0)
		return (EINVAL);

	di = malloc(sizeof(*di), M_DPAA2_MC, M_WAITOK | M_ZERO);
	if (!di)
		return (ENOMEM);
	di->dpaa2_dev = dpaa2_dev;
	di->flags = flags;
	di->owners = 0;

	/* Append a new managed DPAA2 device to the queue. */
	mtx_assert(&sc->mdev_lock, MA_NOTOWNED);
	mtx_lock(&sc->mdev_lock);
	STAILQ_INSERT_TAIL(&sc->mdev_list, di, link);
	mtx_unlock(&sc->mdev_lock);

	if (flags & DPAA2_MC_DEV_ALLOCATABLE) {
		/* Select rman based on a type of the DPAA2 device. */
		rm = dpaa2_mc_rman(mcdev, dinfo->dtype, 0);
		if (!rm)
			return (ENOENT);
		/* Manage DPAA2 device as an allocatable resource. */
		error = rman_manage_region(rm, (rman_res_t) dpaa2_dev,
		    (rman_res_t) dpaa2_dev);
		if (error)
			return (error);
	}

	return (0);
}

int
dpaa2_mc_get_free_dev(device_t mcdev, device_t *dpaa2_dev,
    enum dpaa2_dev_type devtype)
{
	struct rman *rm;
	rman_res_t start, end;
	int error;

	if (strcmp(device_get_name(mcdev), "dpaa2_mc") != 0)
		return (EINVAL);

	/* Select resource manager based on a type of the DPAA2 device. */
	rm = dpaa2_mc_rman(mcdev, devtype, 0);
	if (!rm)
		return (ENOENT);
	/* Find first free DPAA2 device of the given type. */
	error = rman_first_free_region(rm, &start, &end);
	if (error)
		return (error);

	KASSERT(start == end, ("start != end, but should be the same pointer "
	    "to the DPAA2 device: start=%jx, end=%jx", start, end));

	*dpaa2_dev = (device_t) start;

	return (0);
}

int
dpaa2_mc_get_dev(device_t mcdev, device_t *dpaa2_dev,
    enum dpaa2_dev_type devtype, uint32_t obj_id)
{
	struct dpaa2_mc_softc *sc;
	struct dpaa2_devinfo *dinfo;
	struct dpaa2_mc_devinfo *di;
	int error = ENOENT;

	sc = device_get_softc(mcdev);

	if (!sc || strcmp(device_get_name(mcdev), "dpaa2_mc") != 0)
		return (EINVAL);

	mtx_assert(&sc->mdev_lock, MA_NOTOWNED);
	mtx_lock(&sc->mdev_lock);

	STAILQ_FOREACH(di, &sc->mdev_list, link) {
		dinfo = device_get_ivars(di->dpaa2_dev);
		if (dinfo->dtype == devtype && dinfo->id == obj_id) {
			*dpaa2_dev = di->dpaa2_dev;
			error = 0;
			break;
		}
	}

	mtx_unlock(&sc->mdev_lock);

	return (error);
}

int
dpaa2_mc_get_shared_dev(device_t mcdev, device_t *dpaa2_dev,
    enum dpaa2_dev_type devtype)
{
	struct dpaa2_mc_softc *sc;
	struct dpaa2_devinfo *dinfo;
	struct dpaa2_mc_devinfo *di;
	device_t dev = NULL;
	uint32_t owners = UINT32_MAX;
	int error = ENOENT;

	sc = device_get_softc(mcdev);

	if (!sc || strcmp(device_get_name(mcdev), "dpaa2_mc") != 0)
		return (EINVAL);

	mtx_assert(&sc->mdev_lock, MA_NOTOWNED);
	mtx_lock(&sc->mdev_lock);

	STAILQ_FOREACH(di, &sc->mdev_list, link) {
		dinfo = device_get_ivars(di->dpaa2_dev);

		if ((dinfo->dtype == devtype) &&
		    (di->flags & DPAA2_MC_DEV_SHAREABLE) &&
		    (di->owners < owners)) {
			dev = di->dpaa2_dev;
			owners = di->owners;
		}
	}
	if (dev) {
		*dpaa2_dev = dev;
		error = 0;
	}

	mtx_unlock(&sc->mdev_lock);

	return (error);
}

int
dpaa2_mc_reserve_dev(device_t mcdev, device_t dpaa2_dev,
    enum dpaa2_dev_type devtype)
{
	struct dpaa2_mc_softc *sc;
	struct dpaa2_mc_devinfo *di;
	int error = ENOENT;

	sc = device_get_softc(mcdev);

	if (!sc || strcmp(device_get_name(mcdev), "dpaa2_mc") != 0)
		return (EINVAL);

	mtx_assert(&sc->mdev_lock, MA_NOTOWNED);
	mtx_lock(&sc->mdev_lock);

	STAILQ_FOREACH(di, &sc->mdev_list, link) {
		if (di->dpaa2_dev == dpaa2_dev &&
		    (di->flags & DPAA2_MC_DEV_SHAREABLE)) {
			di->owners++;
			error = 0;
			break;
		}
	}

	mtx_unlock(&sc->mdev_lock);

	return (error);
}

int
dpaa2_mc_release_dev(device_t mcdev, device_t dpaa2_dev,
    enum dpaa2_dev_type devtype)
{
	struct dpaa2_mc_softc *sc;
	struct dpaa2_mc_devinfo *di;
	int error = ENOENT;

	sc = device_get_softc(mcdev);

	if (!sc || strcmp(device_get_name(mcdev), "dpaa2_mc") != 0)
		return (EINVAL);

	mtx_assert(&sc->mdev_lock, MA_NOTOWNED);
	mtx_lock(&sc->mdev_lock);

	STAILQ_FOREACH(di, &sc->mdev_list, link) {
		if (di->dpaa2_dev == dpaa2_dev &&
		    (di->flags & DPAA2_MC_DEV_SHAREABLE)) {
			di->owners -= di->owners > 0 ? 1 : 0;
			error = 0;
			break;
		}
	}

	mtx_unlock(&sc->mdev_lock);

	return (error);
}

/**
 * @internal
 */
static u_int
dpaa2_mc_get_xref(device_t mcdev, device_t child)
{
	struct dpaa2_mc_softc *sc = device_get_softc(mcdev);
	struct dpaa2_devinfo *dinfo = device_get_ivars(child);
#ifdef DEV_ACPI
	u_int xref, devid;
#endif
#ifdef FDT
	phandle_t msi_parent;
#endif
	int error;

	if (sc && dinfo) {
#ifdef DEV_ACPI
		if (sc->acpi_based) {
			/*
			 * NOTE: The first named component from the IORT table
			 * with the given name (as a substring) will be used.
			 */
			error = acpi_iort_map_named_msi(IORT_DEVICE_NAME,
			    dinfo->icid, &xref, &devid);
			if (error)
				return (0);
			return (xref);
		}
#endif
#ifdef FDT
		if (!sc->acpi_based) {
			/* FDT-based driver. */
			error = ofw_bus_msimap(sc->ofw_node, dinfo->icid,
			    &msi_parent, NULL);
			if (error)
				return (0);
			return ((u_int) msi_parent);
		}
#endif
	}
	return (0);
}

/**
 * @internal
 */
static u_int
dpaa2_mc_map_id(device_t mcdev, device_t child, uintptr_t *id)
{
	struct dpaa2_devinfo *dinfo;
#ifdef DEV_ACPI
	u_int xref, devid;
	int error;
#endif

	dinfo = device_get_ivars(child);
	if (dinfo) {
		/*
		 * The first named components from IORT table with the given
		 * name (as a substring) will be used.
		 */
#ifdef DEV_ACPI
		error = acpi_iort_map_named_msi(IORT_DEVICE_NAME, dinfo->icid,
		    &xref, &devid);
		if (error == 0)
			*id = devid;
		else
#endif
			*id = dinfo->icid; /* RID not in IORT, likely FW bug */

		return (0);
	}
	return (ENXIO);
}

/**
 * @internal
 * @brief Obtain a resource manager based on the given type of the resource.
 */
struct rman *
dpaa2_mc_rman(device_t mcdev, int type, u_int flags)
{
	struct dpaa2_mc_softc *sc;

	sc = device_get_softc(mcdev);

	switch (type) {
	case DPAA2_DEV_IO:
		return (&sc->dpio_rman);
	case DPAA2_DEV_BP:
		return (&sc->dpbp_rman);
	case DPAA2_DEV_CON:
		return (&sc->dpcon_rman);
	case DPAA2_DEV_MCP:
		return (&sc->dpmcp_rman);
	default:
		break;
	}

	return (NULL);
}

#if defined(INTRNG) && !defined(IOMMU)

/**
 * @internal
 * @brief Allocates requested number of MSIs.
 *
 * NOTE: This function is a part of fallback solution when IOMMU isn't available.
 *	 Total number of IRQs is limited to 32.
 */
static int
dpaa2_mc_alloc_msi_impl(device_t mcdev, device_t child, int count, int maxcount,
    int *irqs)
{
	struct dpaa2_mc_softc *sc = device_get_softc(mcdev);
	int msi_irqs[DPAA2_MC_MSI_COUNT];
	int error;

	/* Pre-allocate a bunch of MSIs for MC to be used by its children. */
	if (!sc->msi_allocated) {
		error = intr_alloc_msi(mcdev, child, dpaa2_mc_get_xref(mcdev,
		    child), DPAA2_MC_MSI_COUNT, DPAA2_MC_MSI_COUNT, msi_irqs);
		if (error) {
			device_printf(mcdev, "failed to pre-allocate %d MSIs: "
			    "error=%d\n", DPAA2_MC_MSI_COUNT, error);
			return (error);
		}

		mtx_assert(&sc->msi_lock, MA_NOTOWNED);
		mtx_lock(&sc->msi_lock);
		for (int i = 0; i < DPAA2_MC_MSI_COUNT; i++) {
			sc->msi[i].child = NULL;
			sc->msi[i].irq = msi_irqs[i];
		}
		sc->msi_owner = child;
		sc->msi_allocated = true;
		mtx_unlock(&sc->msi_lock);
	}

	error = ENOENT;

	/* Find the first free MSIs from the pre-allocated pool. */
	mtx_assert(&sc->msi_lock, MA_NOTOWNED);
	mtx_lock(&sc->msi_lock);
	for (int i = 0; i < DPAA2_MC_MSI_COUNT; i++) {
		if (sc->msi[i].child != NULL)
			continue;
		error = 0;
		for (int j = 0; j < count; j++) {
			if (i + j >= DPAA2_MC_MSI_COUNT) {
				device_printf(mcdev, "requested %d MSIs exceed "
				    "limit of %d available\n", count,
				    DPAA2_MC_MSI_COUNT);
				error = E2BIG;
				break;
			}
			sc->msi[i + j].child = child;
			irqs[j] = sc->msi[i + j].irq;
		}
		break;
	}
	mtx_unlock(&sc->msi_lock);

	return (error);
}

/**
 * @internal
 * @brief Marks IRQs as free in the pre-allocated pool of MSIs.
 *
 * NOTE: This function is a part of fallback solution when IOMMU isn't available.
 *	 Total number of IRQs is limited to 32.
 * NOTE: MSIs are kept allocated in the kernel as a part of the pool.
 */
static int
dpaa2_mc_release_msi_impl(device_t mcdev, device_t child, int count, int *irqs)
{
	struct dpaa2_mc_softc *sc = device_get_softc(mcdev);

	mtx_assert(&sc->msi_lock, MA_NOTOWNED);
	mtx_lock(&sc->msi_lock);
	for (int i = 0; i < DPAA2_MC_MSI_COUNT; i++) {
		if (sc->msi[i].child != child)
			continue;
		for (int j = 0; j < count; j++) {
			if (sc->msi[i].irq == irqs[j]) {
				sc->msi[i].child = NULL;
				break;
			}
		}
	}
	mtx_unlock(&sc->msi_lock);

	return (0);
}

/**
 * @internal
 * @brief Provides address to write to and data according to the given MSI from
 * the pre-allocated pool.
 *
 * NOTE: This function is a part of fallback solution when IOMMU isn't available.
 *	 Total number of IRQs is limited to 32.
 */
static int
dpaa2_mc_map_msi_impl(device_t mcdev, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
	struct dpaa2_mc_softc *sc = device_get_softc(mcdev);
	int error = EINVAL;

	mtx_assert(&sc->msi_lock, MA_NOTOWNED);
	mtx_lock(&sc->msi_lock);
	for (int i = 0; i < DPAA2_MC_MSI_COUNT; i++) {
		if (sc->msi[i].child == child && sc->msi[i].irq == irq) {
			error = 0;
			break;
		}
	}
	mtx_unlock(&sc->msi_lock);
	if (error)
		return (error);

	return (intr_map_msi(mcdev, sc->msi_owner, dpaa2_mc_get_xref(mcdev,
	    sc->msi_owner), irq, addr, data));
}

#endif /* defined(INTRNG) && !defined(IOMMU) */

static device_method_t dpaa2_mc_methods[] = {
	DEVMETHOD_END
};

DEFINE_CLASS_0(dpaa2_mc, dpaa2_mc_driver, dpaa2_mc_methods,
    sizeof(struct dpaa2_mc_softc));
