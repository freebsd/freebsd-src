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

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Fetch a device's resources and associate them with the device.
 *
 * Note that it might be nice to also locate ACPI-specific resource items, such
 * as GPE bits.
 */
ACPI_STATUS
acpi_parse_resources(device_t dev, ACPI_HANDLE handle, struct acpi_parse_resource_set *set)
{
    ACPI_BUFFER		buf;
    RESOURCE		*res;
    char		*curr, *last;
    ACPI_STATUS		status;
    int			i;
    void		*context;

    /*
     * Fetch the device resources
     */
    if (((status = acpi_GetIntoBuffer(handle, AcpiGetPossibleResources, &buf)) != AE_OK) &&
	((status = acpi_GetIntoBuffer(handle, AcpiGetCurrentResources, &buf)) != AE_OK)) {
	device_printf(dev, "can't fetch ACPI resources - %s\n", acpi_strerror(status));
	return(status);
    }

#ifdef ACPI_RES_DEBUG
    device_printf(dev, "got %d bytes of resources\n", buf.Length);
#endif
    set->set_init(dev, &context);

    /*
     * Iterate through the resources
     */
    curr = buf.Pointer;
    last = (char *)buf.Pointer + buf.Length;
    while (curr < last) {
	res = (RESOURCE *)curr;
	curr += res->Length;

	/*
	 * Handle the individual resource types
	 */
	switch(res->Id) {
	case EndTag:
#ifdef ACPI_RES_DEBUG
	    device_printf(dev, "EndTag\n");
#endif
	    curr = last;
	    break;

	case FixedIo:
#ifdef ACPI_RES_DEBUG
	    device_printf(dev, "FixedIo 0x%x/%d\n", res->Data.FixedIo.BaseAddress, res->Data.FixedIo.RangeLength);
#endif
	    set->set_ioport(dev, context, res->Data.FixedIo.BaseAddress, res->Data.FixedIo.RangeLength);
	    break;

	case Io:
	    if (res->Data.Io.MinBaseAddress == res->Data.Io.MaxBaseAddress) {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Io 0x%x/%d\n", res->Data.Io.MinBaseAddress, res->Data.Io.RangeLength);
#endif
		set->set_ioport(dev, context, res->Data.Io.MinBaseAddress, res->Data.Io.RangeLength);
	    } else {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Io 0x%x-0x%x/%d\n", res->Data.Io.MinBaseAddress, res->Data.Io.MaxBaseAddress, 
				 res->Data.Io.RangeLength);
#endif
		set->set_iorange(dev, context, res->Data.Io.MinBaseAddress, res->Data.Io.MaxBaseAddress, 
				 res->Data.Io.RangeLength, res->Data.Io.Alignment);
	    }
	    break;

	case FixedMemory32:
#ifdef ACPI_RES_DEBUG
	    device_printf(dev, "FixedMemory32 0x%x/%d\n", res->Data.FixedMemory32.RangeBaseAddress, 
			    res->Data.FixedMemory32.RangeLength);
#endif
	    set->set_memory(dev, context, res->Data.FixedMemory32.RangeBaseAddress, 
			    res->Data.FixedMemory32.RangeLength);
	    break;

	case Memory32:
	    if (res->Data.Memory32.MinBaseAddress == res->Data.Memory32.MaxBaseAddress) {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Memory32 0x%x/%d\n", res->Data.Memory32.MinBaseAddress, 
			      res->Data.Memory32.RangeLength);
#endif
		set->set_memory(dev, context, res->Data.Memory32.MinBaseAddress, res->Data.Memory32.RangeLength);
	    } else {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Memory32 0x%x-0x%x/%d\n", res->Data.Memory32.MinBaseAddress, 
			      res->Data.Memory32.MaxBaseAddress, res->Data.Memory32.RangeLength);
#endif
		set->set_memoryrange(dev, context, res->Data.Memory32.MinBaseAddress, res->Data.Memory32.MaxBaseAddress,
				     res->Data.Memory32.RangeLength, res->Data.Memory32.Alignment);
	    }
	    break;

	case Memory24:
	    if (res->Data.Memory24.MinBaseAddress == res->Data.Memory24.MaxBaseAddress) {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Memory24 0x%x/%d\n", res->Data.Memory24.MinBaseAddress, 
			      res->Data.Memory24.RangeLength);
#endif
		set->set_memory(dev, context, res->Data.Memory24.MinBaseAddress, res->Data.Memory24.RangeLength);
	    } else {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Memory24 0x%x-0x%x/%d\n", res->Data.Memory24.MinBaseAddress, 
			      res->Data.Memory24.MaxBaseAddress, res->Data.Memory24.RangeLength);
#endif
		set->set_memoryrange(dev, context, res->Data.Memory24.MinBaseAddress, res->Data.Memory24.MaxBaseAddress,
				     res->Data.Memory24.RangeLength, res->Data.Memory24.Alignment);
	    }
	    break;

	case Irq:
	    for (i = 0; i < res->Data.Irq.NumberOfInterrupts; i++) {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Irq %d\n", res->Data.Irq.Interrupts[i]);
#endif
		set->set_irq(dev, context, res->Data.Irq.Interrupts[i]);
	    }
	    break;
	    
	case Dma:
	    for (i = 0; i < res->Data.Dma.NumberOfChannels; i++) {
#ifdef ACPI_RES_DEBUG
		device_printf(dev, "Drq %d\n", res->Data.Dma.Channels[i]);
#endif
		set->set_drq(dev, context, res->Data.Dma.Channels[i]);
	    }
	    break;

	case StartDependentFunctions:
#ifdef ACPI_RES_DEBUG
	    device_printf(dev, "start dependant functions");
#endif
	    set->set_start_dependant(dev, context, res->Data.StartDependentFunctions.CompatibilityPriority);
	    break;

	case EndDependentFunctions:
#ifdef ACPI_RES_DEBUG
	    device_printf(dev, "end dependant functions");
#endif
	    set->set_end_dependant(dev, context);
	    break;

	case Address32:
	    device_printf(dev, "unimplemented Address32 resource\n");
	    break;

	case Address16:
	    device_printf(dev, "unimplemented Address16 resource\n");
	    break;

	case ExtendedIrq:
	    device_printf(dev, "unimplemented ExtendedIrq resource\n");
	    break;

	case VendorSpecific:
	    device_printf(dev, "unimplemented VendorSpecific resource\n");
	    break;
	default:
	    break;
	}
    }    
    AcpiOsFree(buf.Pointer);
    set->set_done(dev, context);
    return(AE_OK);
}

static void	acpi_res_set_init(device_t dev, void **context);
static void	acpi_res_set_done(device_t dev, void *context);
static void	acpi_res_set_ioport(device_t dev, void *context, u_int32_t base, u_int32_t length);
static void	acpi_res_set_iorange(device_t dev, void *context, u_int32_t low, u_int32_t high, 
				     u_int32_t length, u_int32_t align);
static void	acpi_res_set_memory(device_t dev, void *context, u_int32_t base, u_int32_t length);
static void	acpi_res_set_memoryrange(device_t dev, void *context, u_int32_t low, u_int32_t high, 
					 u_int32_t length, u_int32_t align);
static void	acpi_res_set_irq(device_t dev, void *context, u_int32_t irq);
static void	acpi_res_set_drq(device_t dev, void *context, u_int32_t drq);
static void	acpi_res_set_start_dependant(device_t dev, void *context, int preference);
static void	acpi_res_set_end_dependant(device_t dev, void *context);

struct acpi_parse_resource_set acpi_res_parse_set = {
    acpi_res_set_init,
    acpi_res_set_done,
    acpi_res_set_ioport,
    acpi_res_set_iorange,
    acpi_res_set_memory,
    acpi_res_set_memoryrange,
    acpi_res_set_irq,
    acpi_res_set_drq,
    acpi_res_set_start_dependant,
    acpi_res_set_end_dependant
};

struct acpi_res_context {
    int		ar_nio;
    int		ar_nmem;
    int		ar_nirq;
};

static void
acpi_res_set_init(device_t dev, void **context)
{
    struct acpi_res_context	*cp;

    if ((cp = AcpiOsAllocate(sizeof(*cp))) != NULL) {
	bzero(cp, sizeof(*cp));
	*context = cp;
    }
}

static void
acpi_res_set_done(device_t dev, void *context)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    AcpiOsFree(cp);
}

static void
acpi_res_set_ioport(device_t dev, void *context, u_int32_t base, u_int32_t length)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    bus_set_resource(dev, SYS_RES_IOPORT, cp->ar_nio++, base, length);
}

static void
acpi_res_set_iorange(device_t dev, void *context, u_int32_t low, u_int32_t high, u_int32_t length, u_int32_t align)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "I/O range not supported\n");
}

static void
acpi_res_set_memory(device_t dev, void *context, u_int32_t base, u_int32_t length)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    bus_set_resource(dev, SYS_RES_MEMORY, cp->ar_nmem++, base, length);
}

static void
acpi_res_set_memoryrange(device_t dev, void *context, u_int32_t low, u_int32_t high, u_int32_t length, u_int32_t align)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "memory range not supported\n");
}

static void
acpi_res_set_irq(device_t dev, void *context, u_int32_t irq)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    bus_set_resource(dev, SYS_RES_IRQ, cp->ar_nirq++, irq, 1);
}

static void
acpi_res_set_drq(device_t dev, void *context, u_int32_t drq)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "DRQ not supported\n");
}

static void
acpi_res_set_start_dependant(device_t dev, void *context, int preference)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "dependant functions not supported");
}

static void
acpi_res_set_end_dependant(device_t dev, void *context)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
}
