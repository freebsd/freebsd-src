/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
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

#include "bhnd.h"
#include "bhndvar.h"

#include "bhnd_nvram_if.h"

MALLOC_DEFINE(M_BHND, "bhnd", "bhnd bus data structures");

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

static device_t	find_nvram_child(device_t dev);

static int	compare_ascending_probe_order(const void *lhs,
		    const void *rhs);
static int	compare_descending_probe_order(const void *lhs,
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
	device_t	*devs;
	int		 ndevs;
	int		 error;

	if (device_is_attached(dev))
		return (EBUSY);

	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	qsort(devs, ndevs, sizeof(*devs), compare_ascending_probe_order);
	for (int i = 0; i < ndevs; i++) {
		device_t child = devs[i];
		device_probe_and_attach(child);
	}

	free(devs, M_TEMP);
	return (0);
}

/**
 * Default bhnd(4) bus driver implementation of DEVICE_DETACH().
 *
 * This implementation calls device_detach() for each of the the device's
 * children, in reverse bhnd probe order, terminating if any call to
 * device_detach() fails.
 */
int
bhnd_generic_detach(device_t dev)
{
	device_t	*devs;
	int		 ndevs;
	int		 error;

	if (!device_is_attached(dev))
		return (EBUSY);

	if ((error = device_get_children(dev, &devs, &ndevs)))
		return (error);

	/* Detach in the reverse of attach order */
	qsort(devs, ndevs, sizeof(*devs), compare_descending_probe_order);
	for (int i = 0; i < ndevs; i++) {
		device_t child = devs[i];

		/* Terminate on first error */
		if ((error = device_detach(child)))
			goto cleanup;
	}

cleanup:
	free(devs, M_TEMP);
	return (error);
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
		return (BHND_PROBE_BUS + BHND_PROBE_ORDER_FIRST);

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
		if (bhnd_is_hostb_device(child))
			return (BHND_PROBE_ROOT + BHND_PROBE_ORDER_EARLY);

		return (BHND_PROBE_DEFAULT);
	}
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
 * Find an NVRAM child device on @p dev, if any.
 * 
 * @retval device_t An NVRAM device.
 * @retval NULL If no NVRAM device is found.
 */
static device_t
find_nvram_child(device_t dev)
{
	device_t chipc, nvram;

	/* Look for a directly-attached NVRAM child */
	nvram = device_find_child(dev, devclass_get_name(bhnd_nvram_devclass),
	    -1);
	if (nvram == NULL)
		return (NULL);

	/* Further checks require a bhnd(4) bus */
	if (device_get_devclass(dev) != bhnd_devclass)
		return (NULL);

	/* Look for a ChipCommon-attached OTP device */
	if ((chipc = bhnd_find_child(dev, BHND_DEVCLASS_CC, -1)) != NULL) {
		/* Recursively search the ChipCommon device */
		if ((nvram = find_nvram_child(chipc)) != NULL)
			return (nvram);
	}

	/* Not found */
	return (NULL);
}

/**
 * Default bhnd(4) bus driver implementation of BHND_BUS_READ_NVRAM_VAR().
 * 
 * This implementation searches @p dev for a valid NVRAM device. If no NVRAM
 * child device is found on @p dev, the request is delegated to the
 * BHND_BUS_READ_NVRAM_VAR() method on the parent
 * of @p dev.
 */
static int
bhnd_generic_read_nvram_var(device_t dev, device_t child, const char *name,
    void *buf, size_t *size)
{
	device_t nvram;

	/* Try to find an NVRAM device applicable to @p child */
	if ((nvram = find_nvram_child(dev)) == NULL)
		return (BHND_BUS_READ_NVRAM_VAR(device_get_parent(dev), child,
		    name, buf, size));

	return BHND_NVRAM_GETVAR(nvram, name, buf, size);
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
	if (rl != NULL)
		resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#jx");

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
 * indirect, and these methods should never be called.
 */

static uint8_t
bhnd_read_1(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset)
{
	return (BHND_BUS_READ_1(device_get_parent(dev), child, r, offset));
}

static uint16_t 
bhnd_read_2(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset)
{
	return (BHND_BUS_READ_2(device_get_parent(dev), child, r, offset));
}

static uint32_t 
bhnd_read_4(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset)
{
	return (BHND_BUS_READ_4(device_get_parent(dev), child, r, offset));
}

static void
bhnd_write_1(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset, uint8_t value)
{
	BHND_BUS_WRITE_1(device_get_parent(dev), child, r, offset, value);
}

static void
bhnd_write_2(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset, uint16_t value)
{
	BHND_BUS_WRITE_2(device_get_parent(dev), child, r, offset, value);
}

static void 
bhnd_write_4(device_t dev, device_t child, struct bhnd_resource *r,
    bus_size_t offset, uint32_t value)
{
	BHND_BUS_WRITE_4(device_get_parent(dev), child, r, offset, value);
}

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
	DEVMETHOD(bhnd_bus_get_probe_order,	bhnd_generic_get_probe_order),
	DEVMETHOD(bhnd_bus_is_region_valid,	bhnd_generic_is_region_valid),
	DEVMETHOD(bhnd_bus_is_hostb_device,	bhnd_bus_generic_is_hostb_device),
	DEVMETHOD(bhnd_bus_is_hw_disabled,	bhnd_bus_generic_is_hw_disabled),
	DEVMETHOD(bhnd_bus_read_nvram_var,	bhnd_generic_read_nvram_var),
	DEVMETHOD(bhnd_bus_read_1,		bhnd_read_1),
	DEVMETHOD(bhnd_bus_read_2,		bhnd_read_2),
	DEVMETHOD(bhnd_bus_read_4,		bhnd_read_4),
	DEVMETHOD(bhnd_bus_write_1,		bhnd_write_1),
	DEVMETHOD(bhnd_bus_write_2,		bhnd_write_2),
	DEVMETHOD(bhnd_bus_write_4,		bhnd_write_4),
	DEVMETHOD(bhnd_bus_barrier,		bhnd_barrier),

	DEVMETHOD_END
};

devclass_t bhnd_devclass;	/**< bhnd bus. */
devclass_t bhnd_hostb_devclass;	/**< bhnd bus host bridge. */
devclass_t bhnd_nvram_devclass;	/**< bhnd NVRAM device */

DEFINE_CLASS_0(bhnd, bhnd_driver, bhnd_methods, sizeof(struct bhnd_softc));
MODULE_VERSION(bhnd, 1);
