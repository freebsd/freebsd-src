/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
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
 *
 *	$FreeBSD$
 */

/*
 * ISA bus enumerator using PnP HIDs from ACPI space.
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include <isa/isavar.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BUS_MANAGER
MODULE_NAME("ISA")

#define PNP_HEXTONUM(c) ((c) >= 'a'		\
			 ? (c) - 'a' + 10	\
			 : ((c) >= 'A'		\
			    ? (c) - 'A' + 10	\
			    : (c) - '0'))
#define PNP_EISAID(s)				\
	((((s[0] - '@') & 0x1f) << 2)		\
	 | (((s[1] - '@') & 0x18) >> 3)		\
	 | (((s[1] - '@') & 0x07) << 13)	\
	 | (((s[2] - '@') & 0x1f) << 8)		\
	 | (PNP_HEXTONUM(s[4]) << 16)		\
	 | (PNP_HEXTONUM(s[3]) << 20)		\
	 | (PNP_HEXTONUM(s[6]) << 24)		\
	 | (PNP_HEXTONUM(s[5]) << 28))

static void	acpi_isa_set_init(device_t dev, void **context);
static void	acpi_isa_set_done(device_t dev, void *context);
static void	acpi_isa_set_ioport(device_t dev, void *context, u_int32_t base, u_int32_t length);
static void	acpi_isa_set_iorange(device_t dev, void *context, u_int32_t low, u_int32_t high, 
				     u_int32_t length, u_int32_t align);
static void	acpi_isa_set_memory(device_t dev, void *context, u_int32_t base, u_int32_t length);
static void	acpi_isa_set_memoryrange(device_t dev, void *context, u_int32_t low, u_int32_t high, 
					 u_int32_t length, u_int32_t align);
static void	acpi_isa_set_irq(device_t dev, void *context, u_int32_t irq);
static void	acpi_isa_set_drq(device_t dev, void *context, u_int32_t drq);
static void	acpi_isa_set_start_dependant(device_t dev, void *context, int preference);
static void	acpi_isa_set_end_dependant(device_t dev, void *context);

static struct acpi_parse_resource_set acpi_isa_parse_set = {
    acpi_isa_set_init,
    acpi_isa_set_done,
    acpi_isa_set_ioport,
    acpi_isa_set_iorange,
    acpi_isa_set_memory,
    acpi_isa_set_memoryrange,
    acpi_isa_set_irq,
    acpi_isa_set_drq,
    acpi_isa_set_start_dependant,
    acpi_isa_set_end_dependant
};

#define MAXDEP	8

struct acpi_isa_context {
    int			ai_config;
    int			ai_nconfigs;
    struct isa_config	ai_configs[MAXDEP + 1];
    int			ai_priorities[MAXDEP + 1];
};

static void		acpi_isa_set_config(void *arg, struct isa_config *config, int enable);
static void		acpi_isa_identify(driver_t *driver, device_t bus);
static ACPI_STATUS	acpi_isa_identify_child(ACPI_HANDLE handle, UINT32 level, 
						void *context, void **status);

static device_method_t acpi_isa_methods[] = {
    DEVMETHOD(device_identify,	acpi_isa_identify),
    {0, 0}
};

static driver_t acpi_isa_driver = {
    "acpi_isa",
    acpi_isa_methods,
    1,
};

static devclass_t acpi_isa_devclass;
DRIVER_MODULE(acpi_isa, isa, acpi_isa_driver, acpi_isa_devclass, 0, 0);

/*
 * This function is called to make the selected configuration 
 * active.
 */
static void
acpi_isa_set_config(void *arg, struct isa_config *config, int enable)
{
}

/*
 * Interrogate ACPI for devices which might be attatched to an ISA
 * bus.
 *
 * Note that it is difficult to determine whether a device in the ACPI
 * namespace is or is not visible to the ISA bus, and thus we are a 
 * little too generous here and just export everything with _HID
 * and _CRS.
 */
static void
acpi_isa_identify(driver_t *driver, device_t bus)
{
    ACPI_HANDLE		parent;
    ACPI_STATUS		status;

    FUNCTION_TRACE(__func__);

    if (acpi_disabled("isa"))
	return_VOID;

    /*
     * If this driver is loaded from userland, we can assume that
     * the ISA bus has already been detected, and we should not
     * interfere.
     */
    if (!cold)
	return_VOID;

    /*
     * Look for the _SB_ scope, which will contain all the devices
     * we are likely to support.
     */
    if ((status = AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB_", &parent)) != AE_OK) {
	device_printf(bus, "no ACPI _SB_ scope - %s\n", acpi_strerror(status));
	return_VOID;
    }

    if ((status = AcpiWalkNamespace(ACPI_TYPE_DEVICE, parent, 100, acpi_isa_identify_child, bus, NULL)) != AE_OK)
	device_printf(bus, "AcpiWalkNamespace on _SB_ failed - %s\n", acpi_strerror(status));

    return_VOID;
}

/*
 * Check a device to see whether it makes sense to try attaching it to an
 * ISA bus, and if so, do so.
 *
 * Note that we *must* always return AE_OK, or the namespace walk will terminate.
 *
 * XXX Note also that this is picking up a *lot* of things that are not ISA devices.
 *     Should we consider lazy-binding this so that only the ID is saved and resources
 *     are not parsed until the device is claimed by a driver?
 */
static ACPI_STATUS
acpi_isa_identify_child(ACPI_HANDLE handle, UINT32 level, void *context, void **status)
{
    ACPI_DEVICE_INFO	devinfo;
    ACPI_BUFFER		buf;
    device_t		child, bus = (device_t)context;
    u_int32_t		devid;

    FUNCTION_TRACE(__func__);

    /*
     * Skip this node if it's on the 'avoid' list.
     */
    if (acpi_avoid(handle))
	return_ACPI_STATUS(AE_OK);

    /*
     * Try to get information about the device.
     */
    if (AcpiGetObjectInfo(handle, &devinfo) != AE_OK)
	return_ACPI_STATUS(AE_OK);

    /*
     * Reformat the _HID value into 32 bits.
     */
    if (!(devinfo.Valid & ACPI_VALID_HID))
	return_ACPI_STATUS(AE_OK);

    /*
     * XXX Try to avoid passing stuff to ISA that it just isn't interested
     *     in.  This is the *wrong* solution, and what needs to be done
     *     involves just sending ISA the PnP ID and a handle, and then
     *     lazy-parsing the resources if and only if a driver attaches.
     *     With the way that ISA currently works (using bus_probe_and_attach)
     *     this is very difficult.  Maybe we need a device_configure method?
     */
    if (!(strncmp(devinfo.HardwareId, "PNP0C", 5)))
	return_ACPI_STATUS(AE_OK);

    devid = PNP_EISAID(devinfo.HardwareId);

    /* XXX check _STA here? */
    if (devinfo.Valid & ACPI_VALID_STA) {
    }

    /*
     * Fetch our current settings.
     *
     * XXX Note that we may want to support alternate settings at some
     *     point as well.
     */
    if (acpi_GetIntoBuffer(handle, AcpiGetCurrentResources, &buf) != AE_OK)
	return_ACPI_STATUS(AE_OK);

    /*
     * Add the device and parse our resources
     */
    child = BUS_ADD_CHILD(bus, ISA_ORDER_PNP, NULL, -1);
    isa_set_vendorid(child, devid);
    isa_set_logicalid(child, devid);
    ISA_SET_CONFIG_CALLBACK(bus, child, acpi_isa_set_config, 0);
    acpi_parse_resources(child, handle, &acpi_isa_parse_set);
    AcpiOsFree(buf.Pointer);

    if (!device_get_desc(child))
	device_set_desc_copy(child, devinfo.HardwareId);

    DEBUG_PRINT(TRACE_OBJECTS, ("added ISA PnP info for %s\n", acpi_name(handle)));

    /*
     * XXX Parse configuration data and _CID list to find compatible IDs
     */
    return_ACPI_STATUS(AE_OK);
}

static void
acpi_isa_set_init(device_t dev, void **context)
{
    struct acpi_isa_context	*cp;

    FUNCTION_TRACE(__func__);

    cp = malloc(sizeof(*cp), M_DEVBUF, M_NOWAIT);
    bzero(cp, sizeof(*cp));
    cp->ai_nconfigs = 1;
    *context = cp;

    return_VOID;
}

static void
acpi_isa_set_done(device_t dev, void *context)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;
    struct isa_config		*config, *configs;
    device_t			parent;
    int				i, j;

    FUNCTION_TRACE(__func__);

    if (cp == NULL)
	return_VOID;
    parent = device_get_parent(dev);
    
    /* simple config without dependants */
    if (cp->ai_nconfigs == 1) {
	ISA_ADD_CONFIG(parent, dev, cp->ai_priorities[0], &cp->ai_configs[0]);
	goto done;
    }

    /* Cycle through dependant configs merging primary details */
    configs = &cp->ai_configs[0];
    for(i = 1; i < cp->ai_nconfigs; i++) {
	config = &configs[i];
	for(j = 0; j < configs[0].ic_nmem; j++) {
	    if (config->ic_nmem == ISA_NMEM) {
		device_printf(parent, "too many memory ranges\n");
		free(configs, M_DEVBUF);
		return_VOID;
	    }
	    config->ic_mem[config->ic_nmem] = configs[0].ic_mem[j];
	    config->ic_nmem++;
	}
	for(j = 0; j < configs[0].ic_nport; j++) {
	    if (config->ic_nport == ISA_NPORT) {
		device_printf(parent, "too many port ranges\n");
		free(configs, M_DEVBUF);
		return_VOID;
	    }
	    config->ic_port[config->ic_nport] = configs[0].ic_port[j];
	    config->ic_nport++;
	}
	for(j = 0; j < configs[0].ic_nirq; j++) {
	    if (config->ic_nirq == ISA_NIRQ) {
		device_printf(parent, "too many irq ranges\n");
		free(configs, M_DEVBUF);
		return_VOID;
	    }
	    config->ic_irqmask[config->ic_nirq] = configs[0].ic_irqmask[j];
	    config->ic_nirq++;
	}
	for(j = 0; j < configs[0].ic_ndrq; j++) {
	    if (config->ic_ndrq == ISA_NDRQ) {
		device_printf(parent, "too many drq ranges\n");
		free(configs, M_DEVBUF);
		return_VOID;
	    }
	    config->ic_drqmask[config->ic_ndrq] = configs[0].ic_drqmask[j];
	    config->ic_ndrq++;
	}
	(void)ISA_ADD_CONFIG(parent, dev, cp->ai_priorities[i], &configs[i]);
    }

done:
    free(cp, M_DEVBUF);

    return_VOID;
}

static void
acpi_isa_set_ioport(device_t dev, void *context, u_int32_t base, u_int32_t length)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;
    struct isa_config		*ic = &cp->ai_configs[cp->ai_config];

    if (cp == NULL)
	return;
    if (ic->ic_nport == ISA_NPORT) {
	printf("too many ports\n");
	return;
    }
    ic->ic_port[ic->ic_nport].ir_start = base;
    ic->ic_port[ic->ic_nport].ir_end = base + length - 1;
    ic->ic_port[ic->ic_nport].ir_size = length;
    ic->ic_port[ic->ic_nport].ir_align = 1;
    ic->ic_nport++;
}

static void
acpi_isa_set_iorange(device_t dev, void *context, u_int32_t low, u_int32_t high, u_int32_t length, u_int32_t align)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;
    struct isa_config		*ic = &cp->ai_configs[cp->ai_config];

    if (cp == NULL)
	return;
    if (ic->ic_nport == ISA_NPORT) {
	printf("too many ports\n");
	return;
    }
    ic->ic_port[ic->ic_nport].ir_start = low;
    ic->ic_port[ic->ic_nport].ir_end = high + length - 1;
    ic->ic_port[ic->ic_nport].ir_size = length;
    ic->ic_port[ic->ic_nport].ir_align = imin(1, align);
    ic->ic_nport++;
}

static void
acpi_isa_set_memory(device_t dev, void *context, u_int32_t base, u_int32_t length)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;
    struct isa_config		*ic = &cp->ai_configs[cp->ai_config];

    if (cp == NULL)
	return;
    if (ic->ic_nmem == ISA_NMEM) {
	printf("too many memory ranges\n");
	return;
    }
    ic->ic_mem[ic->ic_nmem].ir_start = base;
    ic->ic_mem[ic->ic_nmem].ir_end = base + length - 1;
    ic->ic_mem[ic->ic_nmem].ir_size = length;
    ic->ic_mem[ic->ic_nmem].ir_align = 1;
    ic->ic_nmem++;
}

static void
acpi_isa_set_memoryrange(device_t dev, void *context, u_int32_t low, u_int32_t high, u_int32_t length, u_int32_t align)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;
    struct isa_config		*ic = &cp->ai_configs[cp->ai_config];

    if (cp == NULL)
	return;
    if (ic->ic_nmem == ISA_NMEM) {
	printf("too many memory ranges\n");
	return;
    }
    ic->ic_mem[ic->ic_nmem].ir_start = low;
    ic->ic_mem[ic->ic_nmem].ir_end = high + length - 1;
    ic->ic_mem[ic->ic_nmem].ir_size = length;
    ic->ic_mem[ic->ic_nmem].ir_align = imin(1, align);
    ic->ic_nmem++;
}

static void
acpi_isa_set_irq(device_t dev, void *context, u_int32_t irq)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;
    struct isa_config		*ic = &cp->ai_configs[cp->ai_config];

    if (cp == NULL)
	return;
    if (ic->ic_nirq == ISA_NIRQ) {
	printf("too many IRQs\n");
	return;
    }
    ic->ic_irqmask[ic->ic_nirq] = 1 << irq;
    ic->ic_nirq++;
}

static void
acpi_isa_set_drq(device_t dev, void *context, u_int32_t drq)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;
    struct isa_config		*ic = &cp->ai_configs[cp->ai_config];

    if (cp == NULL)
	return;
    if (ic->ic_nirq == ISA_NDRQ) {
	printf("too many DRQs\n");
	return;
    }
    ic->ic_drqmask[ic->ic_ndrq] = drq;
    ic->ic_ndrq++;
}

/*
 * XXX the "too many dependant configs" logic here is wrong, and
 * will spam the last dependant config.
 */
static void
acpi_isa_set_start_dependant(device_t dev, void *context, int preference)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;

    if (cp == NULL)
	return;

    if (cp->ai_nconfigs > MAXDEP) {
	printf("too many dependant configs\n");
	return;
    }
    cp->ai_config = cp->ai_nconfigs;
    cp->ai_priorities[cp->ai_config] = preference;
    cp->ai_nconfigs++;
}

static void
acpi_isa_set_end_dependant(device_t dev, void *context)
{
    struct acpi_isa_context	*cp = (struct acpi_isa_context *)context;

    if (cp == NULL)
	return;
    cp->ai_config = 0;
}
