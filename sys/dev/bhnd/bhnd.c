/*-
 * Copyright (c) 2015-2016 Landon Fuller <landonf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Broadcom Home Networking Division (HND) Bus Driver.
 * 
 * The Broadcom HND family of devices consists of both SoCs and host-connected
 * networking chipsets containing a common family of Broadcom IP cores,
 * including an integrated MIPS and/or ARM cores.
 * 
 * HND devices expose a nearly identical interface whether accessible over a 
 * native SoC interconnect, or when connected via a host interface such as 
 * PCIe. As a result, the majority of hardware support code should be re-usable 
 * across host drivers for HND networking chipsets, as well as FreeBSD support 
 * for Broadcom MIPS/ARM HND SoCs.
 * 
 * Earlier HND models used the siba(4) on-chip interconnect, while later models
 * use bcma(4); the programming model is almost entirely independent
 * of the actual underlying interconect.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/cores/chipc/chipcvar.h>

#include <dev/bhnd/cores/pmu/bhnd_pmu.h>
#include <dev/bhnd/cores/pmu/bhnd_pmureg.h>

#include "bhnd_chipc_if.h"
#include "bhnd_nvram_if.h"

#include "bhnd.h"
#include "bhndvar.h"

MALLOC_DEFINE(M_BHND, "bhnd", "bhnd bus data structures");

/* Bus pass at which all bus-required children must be available, and
 * attachment may be finalized. */
#define	BHND_FINISH_ATTACH_PASS	BUS_PASS_DEFAULT

/**
 * bhnd_generic_probe_nomatch() reporting configuration.
 */
static const struct bhnd_nomatch {
	uint16_t	vendor;		/**< core designer */
	uint16_t	device;		/**< core id */
	bool		if_verbose;	/**< print when bootverbose is set. */
} bhnd_nomatch_table[] = {
	{ BHND_MFGID_ARM,	BHND_COREID_OOB_ROUTER,		true	},
	{ BHND_MFGID_ARM,	BHND_COREID_EROM,		true	},
	{ BHND_MFGID_ARM,	BHND_COREID_PL301,		true	},
	{ BHND_MFGID_ARM,	BHND_COREID_APB_BRIDGE,		true	},
	{ BHND_MFGID_ARM,	BHND_COREID_AXI_UNMAPPED,	false	},

	{ BHND_MFGID_INVALID,	BHND_COREID_INVALID,		false	}
};


static int			 bhnd_delete_children(struct bhnd_softc *sc);

static int			 bhnd_finish_attach(struct bhnd_softc *sc);

static device_t			 bhnd_find_chipc(struct bhnd_softc *sc);
static struct chipc_caps	*bhnd_find_chipc_caps(struct bhnd_softc *sc);
static device_t			 bhnd_find_platform_dev(struct bhnd_softc *sc,
				     const char *classname);
static device_t			 bhnd_find_pmu(struct bhnd_softc *sc);
static device_t			 bhnd_find_nvram(struct bhnd_softc *sc);

static int			 compare_ascending_probe_order(const void *lhs,
				     const void *rhs);
static int			 compare_descending_probe_order(const void *lhs,
				     const void *rhs);

/**
 * Default bhnd(4) bus driver implementation of DEVICE_ATTACH().
 *
 * This implementation calls device_probe_and_attach() for each of the device's
 * children, in bhnd probe order.
 */
int
bhnd_generic_attach(device_t dev)
{
	struct bhnd_softc	*sc;
	device_t		*devs;
	int			 ndevs;
	int			 error;

	if (device_is_attached(dev))
		return (EBUSY);

	sc = device_get_softc(dev);
	sc->dev = dev;

	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	/* Probe and attach all children */
	qsort(devs, ndevs, sizeof(*devs), compare_ascending_probe_order);
	for (int i = 0; i < ndevs; i++) {
		device_t child = devs[i];
		device_probe_and_attach(child);
	}

	/* Try to finalize attachment */
	if (bus_current_pass >= BHND_FINISH_ATTACH_PASS) {
		if ((error = bhnd_finish_attach(sc)))
			goto cleanup;
	}

cleanup:
	free(devs, M_TEMP);

	if (error)
		bhnd_delete_children(sc);

	return (error);
}

/**
 * Detach and delete all children, in reverse of their attach order.
 */
static int
bhnd_delete_children(struct bhnd_softc *sc)
{
	device_t		*devs;
	int			 ndevs;
	int			 error;

	if ((error = device_get_children(sc->dev, &devs, &ndevs)))
		return (error);

	/* Detach in the reverse of attach order */
	qsort(devs, ndevs, sizeof(*devs), compare_descending_probe_order);
	for (int i = 0; i < ndevs; i++) {
		device_t child = devs[i];

		/* Terminate on first error */
		if ((error = device_delete_child(sc->dev, child)))
			goto cleanup;
	}

cleanup:
	free(devs, M_TEMP);
	return (error);
}

/**
 * Default bhnd(4) bus driver implementation of DEVICE_DETACH().
 *
 * This implementation calls device_detach() for each of the device's
 * children, in reverse bhnd probe order, terminating if any call to
 * device_detach() fails.
 */
int
bhnd_generic_detach(device_t dev)
{
	struct bhnd_softc	*sc;

	if (!device_is_attached(dev))
		return (EBUSY);

	sc = device_get_softc(dev);
	return (bhnd_delete_children(sc));
}

/**
 * Default bhnd(4) bus driver implementation of DEVICE_SHUTDOWN().
 * 
 * This implementation calls device_shutdown() for each of the device's
 * children, in reverse bhnd probe order, terminating if any call to
 * device_shutdown() fails.
 */
int
bhnd_generic_shutdown(device_t dev)
{
	device_t	*devs;
	int		 ndevs;
	int		 error;

	if (!device_is_attached(dev))
		return (EBUSY);

	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	/* Shutdown in the reverse of attach order */
	qsort(devs, ndevs, sizeof(*devs), compare_descending_probe_order);
	for (int i = 0; i < ndevs; i++) {
		device_t child = devs[i];

		/* Terminate on first error */
		if ((error = device_shutdown(child)))
			goto cleanup;
	}

cleanup:
	free(devs, M_TEMP);
	return (error);
}

/**
 * Default bhnd(4) bus driver implementation of DEVICE_RESUME().
 *
 * This implementation calls BUS_RESUME_CHILD() for each of the device's
 * children in bhnd probe order, terminating if any call to BUS_RESUME_CHILD()
 * fails.
 */
int
bhnd_generic_resume(device_t dev)
{
	device_t	*devs;
	int		 ndevs;
	int		 error;

	if (!device_is_attached(dev))
		return (EBUSY);

	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	qsort(devs, ndevs, sizeof(*devs), compare_ascending_probe_order);
	for (int i = 0; i < ndevs; i++) {
		device_t child = devs[i];

		/* Terminate on first error */
		if ((error = BUS_RESUME_CHILD(device_get_parent(child), child)))
			goto cleanup;
	}

cleanup:
	free(devs, M_TEMP);
	return (error);
}

/**
 * Default bhnd(4) bus driver implementation of DEVICE_SUSPEND().
 *
 * This implementation calls BUS_SUSPEND_CHILD() for each of the device's
 * children in reverse bhnd probe order. If any call to BUS_SUSPEND_CHILD()
 * fails, the suspend operation is terminated and any devices that were
 * suspended are resumed immediately by calling their BUS_RESUME_CHILD()
 * methods.
 */
int
bhnd_generic_suspend(device_t dev)
{
	device_t	*devs;
	int		 ndevs;
	int		 error;

	if (!device_is_attached(dev))
		return (EBUSY);

	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	/* Suspend in the reverse of attach order */
	qsort(devs, ndevs, sizeof(*devs), compare_descending_probe_order);
	for (int i = 0; i < ndevs; i++) {
		device_t child = devs[i];
		error = BUS_SUSPEND_CHILD(device_get_parent(child), child);

		/* On error, resume suspended devices and then terminate */
		if (error) {
			for (int j = 0; j < i; j++) {
				BUS_RESUME_CHILD(device_get_parent(devs[j]),
				    devs[j]);
			}

			goto cleanup;
		}
	}

cleanup:
	free(devs, M_TEMP);
	return (error);
}

static void
bhnd_new_pass(device_t dev)
{
	struct bhnd_softc	*sc;
	int			 error;

	sc = device_get_softc(dev);

	/* Attach any permissible children */ 
	bus_generic_new_pass(dev);

	/* Finalize attachment */
	if (!sc->attach_done && bus_current_pass >= BHND_FINISH_ATTACH_PASS) {
		if ((error = bhnd_finish_attach(sc))) {
			panic("bhnd_finish_attach() failed: %d", error);
		}
	}
}

/*
 * Finish any pending bus attachment operations.
 *
 * When attached as a SoC bus (as opposed to a bridged WiFi device), our
 * platform devices may not be attached until later bus passes, necessitating
 * delayed initialization on our part.
 */
static int
bhnd_finish_attach(struct bhnd_softc *sc)
{
	struct chipc_caps	*ccaps;

	GIANT_REQUIRED;	/* for newbus */

	KASSERT(bus_current_pass >= BHND_FINISH_ATTACH_PASS,
	    ("bhnd_finish_attach() called in pass %d", bus_current_pass));

	KASSERT(!sc->attach_done, ("duplicate call to bhnd_finish_attach()"));

	/* Locate chipc device */
	if ((sc->chipc_dev = bhnd_find_chipc(sc)) == NULL) {
		device_printf(sc->dev, "error: ChipCommon device not found\n");
		return (ENXIO);
	}

	ccaps = BHND_CHIPC_GET_CAPS(sc->chipc_dev);

	/* Look for NVRAM device */
	if (ccaps->nvram_src != BHND_NVRAM_SRC_UNKNOWN) {
		if ((sc->nvram_dev = bhnd_find_nvram(sc)) == NULL) {
			device_printf(sc->dev,
			    "warning: NVRAM %s device not found\n",
			    bhnd_nvram_src_name(ccaps->nvram_src));
		}
	}

	/* Look for a PMU  */
	if (ccaps->pmu || ccaps->pwr_ctrl) {
		if ((sc->pmu_dev = bhnd_find_pmu(sc)) == NULL) {
			device_printf(sc->dev,
			    "attach failed: supported PMU not found\n");
			return (ENXIO);
		}
	}

	/* Mark attach as completed */
	sc->attach_done = true;

	return (0);
}

/* Locate the ChipCommon core. */
static device_t
bhnd_find_chipc(struct bhnd_softc *sc)
{
	device_t chipc;

        /* Make sure we're holding Giant for newbus */
	GIANT_REQUIRED;

	/* chipc_dev is initialized during attachment */
	if (sc->attach_done) {
		if ((chipc = sc->chipc_dev) == NULL)
			return (NULL);

		goto found;
	}

	/* Locate chipc core with a core unit of 0 */
	chipc = bhnd_find_child(sc->dev, BHND_DEVCLASS_CC, 0);
	if (chipc == NULL)
		return (NULL);

found:
	if (device_get_state(chipc) < DS_ATTACHING) {
		device_printf(sc->dev, "chipc found, but did not attach\n");
		return (NULL);
	}

	return (chipc);
}

/* Locate the ChipCommon core and return the device capabilities  */
static struct chipc_caps *
bhnd_find_chipc_caps(struct bhnd_softc *sc)
{
	device_t chipc;

	if ((chipc = bhnd_find_chipc(sc)) == NULL) {
		device_printf(sc->dev, 
		    "chipc unavailable; cannot fetch capabilities\n");
		return (NULL);
	}

	return (BHND_CHIPC_GET_CAPS(chipc));
}

/**
 * Find an attached platform device on @p dev, searching first for cores
 * matching @p classname, and if not found, searching the children of the first
 * bhnd_chipc device on the bus.
 * 
 * @param sc Driver state.
 * @param chipc Attached ChipCommon device.
 * @param classname Device class to search for.
 * 
 * @retval device_t A matching device.
 * @retval NULL If no matching device is found.
 */
static device_t
bhnd_find_platform_dev(struct bhnd_softc *sc, const char *classname)
{
	device_t chipc, child;

        /* Make sure we're holding Giant for newbus */
	GIANT_REQUIRED;

	/* Look for a directly-attached child */
	child = device_find_child(sc->dev, classname, -1);
	if (child != NULL)
		goto found;

	/* Look for the first matching ChipCommon child */
	if ((chipc = bhnd_find_chipc(sc)) == NULL) {
		device_printf(sc->dev, 
		    "chipc unavailable; cannot locate %s\n", classname);
		return (NULL);
	}

	child = device_find_child(chipc, classname, -1);
	if (child != NULL)
		goto found;

	/* Look for a parent-attached device (e.g. nexus0 -> bhnd_nvram) */
	child = device_find_child(device_get_parent(sc->dev), classname, -1);
	if (child == NULL)
		return (NULL);

found:
	if (device_get_state(child) < DS_ATTACHING)
		return (NULL);

	return (child);
}

/* Locate the PMU device, if any */
static device_t
bhnd_find_pmu(struct bhnd_softc *sc)
{
        /* Make sure we're holding Giant for newbus */
	GIANT_REQUIRED;

	/* pmu_dev is initialized during attachment */
	if (sc->attach_done) {
		if (sc->pmu_dev == NULL)
			return (NULL);

		if (device_get_state(sc->pmu_dev) < DS_ATTACHING)
			return (NULL);

		return (sc->pmu_dev);
	}


	return (bhnd_find_platform_dev(sc, "bhnd_pmu"));
}

/* Locate the NVRAM device, if any */
static device_t
bhnd_find_nvram(struct bhnd_softc *sc)
{
	struct chipc_caps *ccaps;

        /* Make sure we're holding Giant for newbus */
	GIANT_REQUIRED;


	/* nvram_dev is initialized during attachment */
	if (sc->attach_done) {
		if (sc->nvram_dev == NULL)
			return (NULL);

		if (device_get_state(sc->nvram_dev) < DS_ATTACHING)
			return (NULL);

		return (sc->nvram_dev);
	}

	if ((ccaps = bhnd_find_chipc_caps(sc)) == NULL)
		return (NULL);

	if (ccaps->nvram_src == BHND_NVRAM_SRC_UNKNOWN)
		return (NULL);

	return (bhnd_find_platform_dev(sc, "bhnd_nvram"));
}

/*
 * Ascending comparison of bhnd device's probe order.
 */
static int
compare_ascending_probe_order(const void *lhs, const void *rhs)
{
	device_t	ldev, rdev;
	int		lorder, rorder;

	ldev = (*(const device_t *) lhs);
	rdev = (*(const device_t *) rhs);

	lorder = BHND_BUS_GET_PROBE_ORDER(device_get_parent(ldev), ldev);
	rorder = BHND_BUS_GET_PROBE_ORDER(device_get_parent(rdev), rdev);

	if (lorder < rorder) {
		return (-1);
	} else if (lorder > rorder) {
		return (1);
	} else {
		return (0);
	}
}

/*
 * Descending comparison of bhnd device's probe order.
 */
static int
compare_descending_probe_order(const void *lhs, const void *rhs)
{
	return (compare_ascending_probe_order(rhs, lhs));
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_GET_PROBE_ORDER().
 *
 * This implementation determines probe ordering based on the device's class
 * and other properties, including whether the device is serving as a host
 * bridge.
 */
int
bhnd_generic_get_probe_order(device_t dev, device_t child)
{
	switch (bhnd_get_class(child)) {
	case BHND_DEVCLASS_CC:
		/* Must be early enough to provide NVRAM access to the
		 * host bridge */
		return (BHND_PROBE_ROOT + BHND_PROBE_ORDER_FIRST);

	case BHND_DEVCLASS_CC_B:
		/* fall through */
	case BHND_DEVCLASS_PMU:
		return (BHND_PROBE_BUS + BHND_PROBE_ORDER_EARLY);

	case BHND_DEVCLASS_SOC_ROUTER:
		return (BHND_PROBE_BUS + BHND_PROBE_ORDER_LATE);

	case BHND_DEVCLASS_SOC_BRIDGE:
		return (BHND_PROBE_BUS + BHND_PROBE_ORDER_LAST);
		
	case BHND_DEVCLASS_CPU:
		return (BHND_PROBE_CPU + BHND_PROBE_ORDER_FIRST);

	case BHND_DEVCLASS_RAM:
		/* fall through */
	case BHND_DEVCLASS_MEMC:
		return (BHND_PROBE_CPU + BHND_PROBE_ORDER_EARLY);
		
	case BHND_DEVCLASS_NVRAM:
		return (BHND_PROBE_RESOURCE + BHND_PROBE_ORDER_EARLY);

	case BHND_DEVCLASS_PCI:
	case BHND_DEVCLASS_PCIE:
	case BHND_DEVCLASS_PCCARD:
	case BHND_DEVCLASS_ENET:
	case BHND_DEVCLASS_ENET_MAC:
	case BHND_DEVCLASS_ENET_PHY:
	case BHND_DEVCLASS_WLAN:
	case BHND_DEVCLASS_WLAN_MAC:
	case BHND_DEVCLASS_WLAN_PHY:
	case BHND_DEVCLASS_EROM:
	case BHND_DEVCLASS_OTHER:
	case BHND_DEVCLASS_INVALID:
		if (bhnd_find_hostb_device(dev) == child)
			return (BHND_PROBE_ROOT + BHND_PROBE_ORDER_EARLY);

		return (BHND_PROBE_DEFAULT);
	default:
		return (BHND_PROBE_DEFAULT);
	}
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_ALLOC_PMU().
 */
int
bhnd_generic_alloc_pmu(device_t dev, device_t child)
{
	struct bhnd_softc		*sc;
	struct bhnd_resource		*br;
	struct chipc_caps		*ccaps;
	struct bhnd_devinfo		*dinfo;	
	struct bhnd_core_pmu_info	*pm;
	struct resource_list		*rl;
	struct resource_list_entry	*rle;
	device_t			 pmu_dev;
	bhnd_addr_t			 r_addr;
	bhnd_size_t			 r_size;
	bus_size_t			 pmu_regs;
	int				 error;

	GIANT_REQUIRED;	/* for newbus */
	
	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);
	pmu_regs = BHND_CLK_CTL_ST;

	if ((ccaps = bhnd_find_chipc_caps(sc)) == NULL) {
		device_printf(sc->dev, "alloc_pmu failed: chipc "
		    "capabilities unavailable\n");
		return (ENXIO);
	}
	
	if ((pmu_dev = bhnd_find_pmu(sc)) == NULL) {
		device_printf(sc->dev, 
		    "pmu unavailable; cannot allocate request state\n");
		return (ENXIO);
	}

	/* already allocated? */
	if (dinfo->pmu_info != NULL) {
		panic("duplicate PMU allocation for %s",
		    device_get_nameunit(child));
	}

	/* Determine address+size of the core's PMU register block */
	error = bhnd_get_region_addr(child, BHND_PORT_DEVICE, 0, 0, &r_addr,
	    &r_size);
	if (error) {
		device_printf(sc->dev, "error fetching register block info for "
		    "%s: %d\n", device_get_nameunit(child), error);
		return (error);
	}

	if (r_size < (pmu_regs + sizeof(uint32_t))) {
		device_printf(sc->dev, "pmu offset %#jx would overrun %s "
		    "register block\n", (uintmax_t)pmu_regs,
		    device_get_nameunit(child));
		return (ENODEV);
	}

	/* Locate actual resource containing the core's register block */
	if ((rl = BUS_GET_RESOURCE_LIST(dev, child)) == NULL) {
		device_printf(dev, "NULL resource list returned for %s\n",
		    device_get_nameunit(child));
		return (ENXIO);
	}

	if ((rle = resource_list_find(rl, SYS_RES_MEMORY, 0)) == NULL) {
		device_printf(dev, "cannot locate core register resource "
		    "for %s\n", device_get_nameunit(child));
		return (ENXIO);
	}

	if (rle->res == NULL) {
		device_printf(dev, "core register resource unallocated for "
		    "%s\n", device_get_nameunit(child));
		return (ENXIO);
	}

	if (r_addr+pmu_regs < rman_get_start(rle->res) ||
	    r_addr+pmu_regs >= rman_get_end(rle->res))
	{
		device_printf(dev, "core register resource does not map PMU "
		    "registers at %#jx\n for %s\n", r_addr+pmu_regs,
		    device_get_nameunit(child));
		return (ENXIO);
	}

	/* Adjust PMU register offset relative to the actual start address
	 * of the core's register block allocation.
	 * 
	 * XXX: The saved offset will be invalid if bus_adjust_resource is
	 * used to modify the resource's start address.
	 */
	if (rman_get_start(rle->res) > r_addr)
		pmu_regs -= rman_get_start(rle->res) - r_addr;
	else
		pmu_regs -= r_addr - rman_get_start(rle->res);

	/* Allocate and initialize PMU info */
	br = malloc(sizeof(struct bhnd_resource), M_BHND, M_NOWAIT);
	if (br == NULL)
		return (ENOMEM);

	br->res = rle->res;
	br->direct = ((rman_get_flags(rle->res) & RF_ACTIVE) != 0);

	pm = malloc(sizeof(*dinfo->pmu_info), M_BHND, M_NOWAIT);
	if (pm == NULL) {
		free(br, M_BHND);
		return (ENOMEM);
	}
	pm->pm_dev = child;
	pm->pm_pmu = pmu_dev;
	pm->pm_res = br;
	pm->pm_regs = pmu_regs;

	dinfo->pmu_info = pm;
	return (0);
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_RELEASE_PMU().
 */
int
bhnd_generic_release_pmu(device_t dev, device_t child)
{
	struct bhnd_softc		*sc;
	struct bhnd_devinfo		*dinfo;
	device_t			 pmu;
	int				 error;

	GIANT_REQUIRED;	/* for newbus */
	
	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	if ((pmu = bhnd_find_pmu(sc)) == NULL) {
		device_printf(sc->dev, 
		    "pmu unavailable; cannot release request state\n");
		return (ENXIO);
	}

	/* dispatch release request */
	if (dinfo->pmu_info == NULL)
		panic("pmu over-release for %s", device_get_nameunit(child));

	if ((error = BHND_PMU_CORE_RELEASE(pmu, dinfo->pmu_info)))
		return (error);

	/* free PMU info */
	free(dinfo->pmu_info->pm_res, M_BHND);
	free(dinfo->pmu_info, M_BHND);
	dinfo->pmu_info = NULL;

	return (0);
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_REQUEST_CLOCK().
 */
int
bhnd_generic_request_clock(device_t dev, device_t child, bhnd_clock clock)
{
	struct bhnd_softc		*sc;
	struct bhnd_devinfo		*dinfo;
	struct bhnd_core_pmu_info	*pm;

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	if ((pm = dinfo->pmu_info) == NULL)
		panic("no active PMU request state");

	/* dispatch request to PMU */
	return (BHND_PMU_CORE_REQ_CLOCK(pm->pm_pmu, pm, clock));
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_ENABLE_CLOCKS().
 */
int
bhnd_generic_enable_clocks(device_t dev, device_t child, uint32_t clocks)
{
	struct bhnd_softc		*sc;
	struct bhnd_devinfo		*dinfo;
	struct bhnd_core_pmu_info	*pm;

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	if ((pm = dinfo->pmu_info) == NULL)
		panic("no active PMU request state");

	/* dispatch request to PMU */
	return (BHND_PMU_CORE_EN_CLOCKS(pm->pm_pmu, pm, clocks));
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_REQUEST_EXT_RSRC().
 */
int
bhnd_generic_request_ext_rsrc(device_t dev, device_t child, u_int rsrc)
{
	struct bhnd_softc		*sc;
	struct bhnd_devinfo		*dinfo;
	struct bhnd_core_pmu_info	*pm;

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	if ((pm = dinfo->pmu_info) == NULL)
		panic("no active PMU request state");

	/* dispatch request to PMU */
	return (BHND_PMU_CORE_REQ_EXT_RSRC(pm->pm_pmu, pm, rsrc));
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_RELEASE_EXT_RSRC().
 */
int
bhnd_generic_release_ext_rsrc(device_t dev, device_t child, u_int rsrc)
{
	struct bhnd_softc		*sc;
	struct bhnd_devinfo		*dinfo;
	struct bhnd_core_pmu_info	*pm;

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	if ((pm = dinfo->pmu_info) == NULL)
		panic("no active PMU request state");

	/* dispatch request to PMU */
	return (BHND_PMU_CORE_RELEASE_EXT_RSRC(pm->pm_pmu, pm, rsrc));
}


/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_IS_REGION_VALID().
 * 
 * This implementation assumes that port and region numbers are 0-indexed and
 * are allocated non-sparsely, using BHND_BUS_GET_PORT_COUNT() and
 * BHND_BUS_GET_REGION_COUNT() to determine if @p port and @p region fall
 * within the defined range.
 */
static bool
bhnd_generic_is_region_valid(device_t dev, device_t child,
    bhnd_port_type type, u_int port, u_int region)
{
	if (port >= bhnd_get_port_count(child, type))
		return (false);

	if (region >= bhnd_get_region_count(child, type, port))
		return (false);

	return (true);
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_GET_NVRAM_VAR().
 * 
 * This implementation searches @p dev for a usable NVRAM child device.
 * 
 * If no usable child device is found on @p dev, the request is delegated to
 * the BHND_BUS_GET_NVRAM_VAR() method on the parent of @p dev.
 */
int
bhnd_generic_get_nvram_var(device_t dev, device_t child, const char *name,
    void *buf, size_t *size, bhnd_nvram_type type)
{
	struct bhnd_softc	*sc;
	device_t		 nvram, parent;

	sc = device_get_softc(dev);

	/* If a NVRAM device is available, consult it first */
	if ((nvram = bhnd_find_nvram(sc)) != NULL)
		return BHND_NVRAM_GETVAR(nvram, name, buf, size, type);

	/* Otherwise, try to delegate to parent */
	if ((parent = device_get_parent(dev)) == NULL)
		return (ENODEV);

	return (BHND_BUS_GET_NVRAM_VAR(device_get_parent(dev), child,
	    name, buf, size, type));
}

/**
 * Default bhnd(4) bus driver implementation of BUS_PRINT_CHILD().
 * 
 * This implementation requests the device's struct resource_list via
 * BUS_GET_RESOURCE_LIST.
 */
int
bhnd_generic_print_child(device_t dev, device_t child)
{
	struct resource_list	*rl;
	int			retval = 0;

	retval += bus_print_child_header(dev, child);

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	
	
	if (rl != NULL) {
		retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY,
		    "%#jx");

		retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ,
		    "%#jd");
	}

	retval += printf(" at core %u", bhnd_get_core_index(child));

	retval += bus_print_child_domain(dev, child);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

/**
 * Default bhnd(4) bus driver implementation of BUS_PROBE_NOMATCH().
 * 
 * This implementation requests the device's struct resource_list via
 * BUS_GET_RESOURCE_LIST.
 */
void
bhnd_generic_probe_nomatch(device_t dev, device_t child)
{
	struct resource_list		*rl;
	const struct bhnd_nomatch	*nm;
	bool				 report;

	/* Fetch reporting configuration for this device */
	report = true;
	for (nm = bhnd_nomatch_table; nm->device != BHND_COREID_INVALID; nm++) {
		if (nm->vendor != bhnd_get_vendor(child))
			continue;

		if (nm->device != bhnd_get_device(child))
			continue;

		report = false;
		if (bootverbose && nm->if_verbose)
			report = true;
		break;
	}
	
	if (!report)
		return;

	/* Print the non-matched device info */
	device_printf(dev, "<%s %s>", bhnd_get_vendor_name(child),
		bhnd_get_device_name(child));

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (rl != NULL) {
		resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#jx");
		resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%#jd");
	}

	printf(" at core %u (no driver attached)\n",
	    bhnd_get_core_index(child));
}

/**
 * Default implementation of BUS_CHILD_PNPINFO_STR().
 */
static int
bhnd_child_pnpinfo_str(device_t dev, device_t child, char *buf,
    size_t buflen)
{
	if (device_get_parent(child) != dev) {
		return (BUS_CHILD_PNPINFO_STR(device_get_parent(dev), child,
		    buf, buflen));
	}

	snprintf(buf, buflen, "vendor=0x%hx device=0x%hx rev=0x%hhx",
	    bhnd_get_vendor(child), bhnd_get_device(child),
	    bhnd_get_hwrev(child));

	return (0);
}

/**
 * Default implementation of BUS_CHILD_LOCATION_STR().
 */
static int
bhnd_child_location_str(device_t dev, device_t child, char *buf,
    size_t buflen)
{
	bhnd_addr_t	addr;
	bhnd_size_t	size;
	
	if (device_get_parent(child) != dev) {
		return (BUS_CHILD_LOCATION_STR(device_get_parent(dev), child,
		    buf, buflen));
	}


	if (bhnd_get_region_addr(child, BHND_PORT_DEVICE, 0, 0, &addr, &size)) {
		/* No device default port/region */
		if (buflen > 0)
			*buf = '\0';
		return (0);
	}

	snprintf(buf, buflen, "port0.0=0x%llx", (unsigned long long) addr);
	return (0);
}

/**
 * Default bhnd(4) bus driver implementation of BUS_ADD_CHILD().
 * 
 * This implementation manages internal bhnd(4) state, and must be called
 * by subclassing drivers.
 */
device_t
bhnd_generic_add_child(device_t dev, u_int order, const char *name, int unit)
{
	struct bhnd_devinfo	*dinfo;
	device_t		 child;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (NULL);

	if ((dinfo = BHND_BUS_ALLOC_DEVINFO(dev)) == NULL) {
		device_delete_child(dev, child);
		return (NULL);
	}

	device_set_ivars(child, dinfo);

	return (child);
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_CHILD_ADDED().
 * 
 * This implementation manages internal bhnd(4) state, and must be called
 * by subclassing drivers.
 */
void
bhnd_generic_child_added(device_t dev, device_t child)
{
}

/**
 * Default bhnd(4) bus driver implementation of BUS_CHILD_DELETED().
 * 
 * This implementation manages internal bhnd(4) state, and must be called
 * by subclassing drivers.
 */
void
bhnd_generic_child_deleted(device_t dev, device_t child)
{
	struct bhnd_softc	*sc;
	struct bhnd_devinfo	*dinfo;

	sc = device_get_softc(dev);

	/* Free device info */
	if ((dinfo = device_get_ivars(child)) != NULL) {
		if (dinfo->pmu_info != NULL) {
			/* Releasing PMU requests automatically would be nice,
			 * but we can't reference per-core PMU register
			 * resource after driver detach */
			panic("%s leaked device pmu state\n",
			    device_get_nameunit(child));
		}

		BHND_BUS_FREE_DEVINFO(dev, dinfo);
	}

	/* Clean up platform device references */
	if (sc->chipc_dev == child) {
		sc->chipc_dev = NULL;
	} else if (sc->nvram_dev == child) {
		sc->nvram_dev = NULL;
	} else if (sc->pmu_dev == child) {
		sc->pmu_dev = NULL;
	}
}

/**
 * Helper function for implementing BUS_SUSPEND_CHILD().
 *
 * TODO: Power management
 * 
 * If @p child is not a direct child of @p dev, suspension is delegated to
 * the @p dev parent.
 */
int
bhnd_generic_suspend_child(device_t dev, device_t child)
{
	if (device_get_parent(child) != dev)
		BUS_SUSPEND_CHILD(device_get_parent(dev), child);

	return bus_generic_suspend_child(dev, child);
}

/**
 * Helper function for implementing BUS_RESUME_CHILD().
 *
 * TODO: Power management
 * 
 * If @p child is not a direct child of @p dev, suspension is delegated to
 * the @p dev parent.
 */
int
bhnd_generic_resume_child(device_t dev, device_t child)
{
	if (device_get_parent(child) != dev)
		BUS_RESUME_CHILD(device_get_parent(dev), child);

	return bus_generic_resume_child(dev, child);
}

/*
 * Delegate all indirect I/O to the parent device. When inherited by
 * non-bridged bus implementations, resources will never be marked as
 * indirect, and these methods will never be called.
 */
#define	BHND_IO_READ(_type, _name, _method)				\
static _type								\
bhnd_read_ ## _name (device_t dev, device_t child,			\
    struct bhnd_resource *r, bus_size_t offset)				\
{									\
	return (BHND_BUS_READ_ ## _method(				\
		    device_get_parent(dev), child, r, offset));		\
}

#define	BHND_IO_WRITE(_type, _name, _method)				\
static void								\
bhnd_write_ ## _name (device_t dev, device_t child,			\
    struct bhnd_resource *r, bus_size_t offset, _type value)		\
{									\
	return (BHND_BUS_WRITE_ ## _method(				\
		    device_get_parent(dev), child, r, offset,		\
		    value));	\
}

#define	BHND_IO_MISC(_type, _op, _method)				\
static void								\
bhnd_ ## _op (device_t dev, device_t child,				\
    struct bhnd_resource *r, bus_size_t offset, _type datap,		\
    bus_size_t count)							\
{									\
	BHND_BUS_ ## _method(device_get_parent(dev), child, r,		\
	    offset, datap, count);					\
}	

#define	BHND_IO_METHODS(_type, _size)					\
	BHND_IO_READ(_type, _size, _size)				\
	BHND_IO_WRITE(_type, _size, _size)				\
									\
	BHND_IO_READ(_type, stream_ ## _size, STREAM_ ## _size)		\
	BHND_IO_WRITE(_type, stream_ ## _size, STREAM_ ## _size)	\
									\
	BHND_IO_MISC(_type*, read_multi_ ## _size,			\
	    READ_MULTI_ ## _size)					\
	BHND_IO_MISC(_type*, write_multi_ ## _size,			\
	    WRITE_MULTI_ ## _size)					\
									\
	BHND_IO_MISC(_type*, read_multi_stream_ ## _size,		\
	   READ_MULTI_STREAM_ ## _size)					\
	BHND_IO_MISC(_type*, write_multi_stream_ ## _size,		\
	   WRITE_MULTI_STREAM_ ## _size)				\
									\
	BHND_IO_MISC(_type, set_multi_ ## _size, SET_MULTI_ ## _size)	\
	BHND_IO_MISC(_type, set_region_ ## _size, SET_REGION_ ## _size)	\
									\
	BHND_IO_MISC(_type*, read_region_ ## _size,			\
	    READ_REGION_ ## _size)					\
	BHND_IO_MISC(_type*, write_region_ ## _size,			\
	    WRITE_REGION_ ## _size)					\
									\
	BHND_IO_MISC(_type*, read_region_stream_ ## _size,		\
	    READ_REGION_STREAM_ ## _size)				\
	BHND_IO_MISC(_type*, write_region_stream_ ## _size,		\
	    WRITE_REGION_STREAM_ ## _size)				\

BHND_IO_METHODS(uint8_t, 1);
BHND_IO_METHODS(uint16_t, 2);
BHND_IO_METHODS(uint32_t, 4);

static void 
bhnd_barrier(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset, bus_size_t length, int flags)
{
	BHND_BUS_BARRIER(device_get_parent(dev), child, r, offset, length,
	    flags);
}

static device_method_t bhnd_methods[] = {
	/* Device interface */ \
	DEVMETHOD(device_attach,		bhnd_generic_attach),
	DEVMETHOD(device_detach,		bhnd_generic_detach),
	DEVMETHOD(device_shutdown,		bhnd_generic_shutdown),
	DEVMETHOD(device_suspend,		bhnd_generic_suspend),
	DEVMETHOD(device_resume,		bhnd_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_new_pass,			bhnd_new_pass),
	DEVMETHOD(bus_add_child,		bhnd_generic_add_child),
	DEVMETHOD(bus_child_deleted,		bhnd_generic_child_deleted),
	DEVMETHOD(bus_probe_nomatch,		bhnd_generic_probe_nomatch),
	DEVMETHOD(bus_print_child,		bhnd_generic_print_child),
	DEVMETHOD(bus_child_pnpinfo_str,	bhnd_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str,	bhnd_child_location_str),

	DEVMETHOD(bus_suspend_child,		bhnd_generic_suspend_child),
	DEVMETHOD(bus_resume_child,		bhnd_generic_resume_child),

	DEVMETHOD(bus_set_resource,		bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,		bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,		bus_generic_rl_delete_resource),
	DEVMETHOD(bus_alloc_resource,		bus_generic_rl_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		bus_generic_adjust_resource),
	DEVMETHOD(bus_release_resource,		bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),

	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(bus_config_intr,		bus_generic_config_intr),
	DEVMETHOD(bus_bind_intr,		bus_generic_bind_intr),
	DEVMETHOD(bus_describe_intr,		bus_generic_describe_intr),

	DEVMETHOD(bus_get_dma_tag,		bus_generic_get_dma_tag),

	/* BHND interface */
	DEVMETHOD(bhnd_bus_get_chipid,		bhnd_bus_generic_get_chipid),
	DEVMETHOD(bhnd_bus_is_hw_disabled,	bhnd_bus_generic_is_hw_disabled),
	DEVMETHOD(bhnd_bus_read_board_info,	bhnd_bus_generic_read_board_info),

	DEVMETHOD(bhnd_bus_get_probe_order,	bhnd_generic_get_probe_order),

	DEVMETHOD(bhnd_bus_alloc_pmu,		bhnd_generic_alloc_pmu),
	DEVMETHOD(bhnd_bus_release_pmu,		bhnd_generic_release_pmu),
	DEVMETHOD(bhnd_bus_request_clock,	bhnd_generic_request_clock),
	DEVMETHOD(bhnd_bus_enable_clocks,	bhnd_generic_enable_clocks),
	DEVMETHOD(bhnd_bus_request_ext_rsrc,	bhnd_generic_request_ext_rsrc),
	DEVMETHOD(bhnd_bus_release_ext_rsrc,	bhnd_generic_release_ext_rsrc),

	DEVMETHOD(bhnd_bus_child_added,		bhnd_generic_child_added),
	DEVMETHOD(bhnd_bus_is_region_valid,	bhnd_generic_is_region_valid),
	DEVMETHOD(bhnd_bus_get_nvram_var,	bhnd_generic_get_nvram_var),

	/* BHND interface (bus I/O) */
	DEVMETHOD(bhnd_bus_read_1,		bhnd_read_1),
	DEVMETHOD(bhnd_bus_read_2,		bhnd_read_2),
	DEVMETHOD(bhnd_bus_read_4,		bhnd_read_4),
	DEVMETHOD(bhnd_bus_write_1,		bhnd_write_1),
	DEVMETHOD(bhnd_bus_write_2,		bhnd_write_2),
	DEVMETHOD(bhnd_bus_write_4,		bhnd_write_4),

	DEVMETHOD(bhnd_bus_read_stream_1,	bhnd_read_stream_1),
	DEVMETHOD(bhnd_bus_read_stream_2,	bhnd_read_stream_2),
	DEVMETHOD(bhnd_bus_read_stream_4,	bhnd_read_stream_4),
	DEVMETHOD(bhnd_bus_write_stream_1,	bhnd_write_stream_1),
	DEVMETHOD(bhnd_bus_write_stream_2,	bhnd_write_stream_2),
	DEVMETHOD(bhnd_bus_write_stream_4,	bhnd_write_stream_4),

	DEVMETHOD(bhnd_bus_read_multi_1,	bhnd_read_multi_1),
	DEVMETHOD(bhnd_bus_read_multi_2,	bhnd_read_multi_2),
	DEVMETHOD(bhnd_bus_read_multi_4,	bhnd_read_multi_4),
	DEVMETHOD(bhnd_bus_write_multi_1,	bhnd_write_multi_1),
	DEVMETHOD(bhnd_bus_write_multi_2,	bhnd_write_multi_2),
	DEVMETHOD(bhnd_bus_write_multi_4,	bhnd_write_multi_4),
	
	DEVMETHOD(bhnd_bus_read_multi_stream_1,	bhnd_read_multi_stream_1),
	DEVMETHOD(bhnd_bus_read_multi_stream_2,	bhnd_read_multi_stream_2),
	DEVMETHOD(bhnd_bus_read_multi_stream_4,	bhnd_read_multi_stream_4),
	DEVMETHOD(bhnd_bus_write_multi_stream_1,bhnd_write_multi_stream_1),
	DEVMETHOD(bhnd_bus_write_multi_stream_2,bhnd_write_multi_stream_2),
	DEVMETHOD(bhnd_bus_write_multi_stream_4,bhnd_write_multi_stream_4),

	DEVMETHOD(bhnd_bus_set_multi_1,		bhnd_set_multi_1),
	DEVMETHOD(bhnd_bus_set_multi_2,		bhnd_set_multi_2),
	DEVMETHOD(bhnd_bus_set_multi_4,		bhnd_set_multi_4),

	DEVMETHOD(bhnd_bus_set_region_1,	bhnd_set_region_1),
	DEVMETHOD(bhnd_bus_set_region_2,	bhnd_set_region_2),
	DEVMETHOD(bhnd_bus_set_region_4,	bhnd_set_region_4),

	DEVMETHOD(bhnd_bus_read_region_1,	bhnd_read_region_1),
	DEVMETHOD(bhnd_bus_read_region_2,	bhnd_read_region_2),
	DEVMETHOD(bhnd_bus_read_region_4,	bhnd_read_region_4),
	DEVMETHOD(bhnd_bus_write_region_1,	bhnd_write_region_1),
	DEVMETHOD(bhnd_bus_write_region_2,	bhnd_write_region_2),
	DEVMETHOD(bhnd_bus_write_region_4,	bhnd_write_region_4),

	DEVMETHOD(bhnd_bus_read_region_stream_1,bhnd_read_region_stream_1),
	DEVMETHOD(bhnd_bus_read_region_stream_2,bhnd_read_region_stream_2),
	DEVMETHOD(bhnd_bus_read_region_stream_4,bhnd_read_region_stream_4),
	DEVMETHOD(bhnd_bus_write_region_stream_1, bhnd_write_region_stream_1),
	DEVMETHOD(bhnd_bus_write_region_stream_2, bhnd_write_region_stream_2),
	DEVMETHOD(bhnd_bus_write_region_stream_4, bhnd_write_region_stream_4),

	DEVMETHOD(bhnd_bus_barrier,			bhnd_barrier),

	DEVMETHOD_END
};

devclass_t bhnd_devclass;	/**< bhnd bus. */
devclass_t bhnd_hostb_devclass;	/**< bhnd bus host bridge. */
devclass_t bhnd_nvram_devclass;	/**< bhnd NVRAM device */

DEFINE_CLASS_0(bhnd, bhnd_driver, bhnd_methods, sizeof(struct bhnd_softc));
MODULE_VERSION(bhnd, 1);
