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
#include <sys/rman.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("RESOURCE")

/*
 * Fetch a device's resources and associate them with the device.
 *
 * Note that it might be nice to also locate ACPI-specific resource items, such
 * as GPE bits.
 *
 * We really need to split the resource-fetching code out from the
 * resource-parsing code, since we may want to use the parsing
 * code for _PRS someday.
 */
ACPI_STATUS
acpi_parse_resources(device_t dev, ACPI_HANDLE handle, struct acpi_parse_resource_set *set)
{
    ACPI_BUFFER		buf;
    ACPI_RESOURCE	*res;
    char		*curr, *last;
    ACPI_STATUS		status;
    void		*context;

    ACPI_FUNCTION_TRACE(__func__);

    /*
     * Special-case some devices that abuse _PRS/_CRS to mean
     * something other than "I consume this resource".
     *
     * XXX do we really need this?  It's only relevant once
     *     we start always-allocating these resources, and even
     *     then, the only special-cased device is likely to be
     *     the PCI interrupt link.
     */

    /*
     * Fetch the device's current resources.
     */
    buf.Length = ACPI_ALLOCATE_BUFFER;
    if (ACPI_FAILURE((status = AcpiGetCurrentResources(handle, &buf)))) {
	if (status != AE_NOT_FOUND)
	    printf("can't fetch resources for %s - %s\n",
		   acpi_name(handle), AcpiFormatException(status));
	return_ACPI_STATUS(status);
    }
    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "%s - got %d bytes of resources\n",
		      acpi_name(handle), buf.Length));
    set->set_init(dev, &context);

    /*
     * Iterate through the resources
     */
    curr = buf.Pointer;
    last = (char *)buf.Pointer + buf.Length;
    while (curr < last) {
	res = (ACPI_RESOURCE *)curr;
	curr += res->Length;

	/*
	 * Handle the individual resource types
	 */
	switch(res->Id) {
	case ACPI_RSTYPE_END_TAG:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "EndTag\n"));
	    curr = last;
	    break;

	case ACPI_RSTYPE_FIXED_IO:
	    if (res->Data.FixedIo.RangeLength <= 0)
		break;
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "FixedIo 0x%x/%d\n",
			      res->Data.FixedIo.BaseAddress,
			      res->Data.FixedIo.RangeLength));
	    set->set_ioport(dev, context,
			    res->Data.FixedIo.BaseAddress,
			    res->Data.FixedIo.RangeLength);
	    break;

	case ACPI_RSTYPE_IO:
	    if (res->Data.Io.RangeLength <= 0)
		break;
	    if (res->Data.Io.MinBaseAddress == res->Data.Io.MaxBaseAddress) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Io 0x%x/%d\n",
				  res->Data.Io.MinBaseAddress,
				  res->Data.Io.RangeLength));
		set->set_ioport(dev, context,
				res->Data.Io.MinBaseAddress,
				res->Data.Io.RangeLength);
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Io 0x%x-0x%x/%d\n",
				  res->Data.Io.MinBaseAddress,
				  res->Data.Io.MaxBaseAddress, 
				  res->Data.Io.RangeLength));
		set->set_iorange(dev, context,
				 res->Data.Io.MinBaseAddress,
				 res->Data.Io.MaxBaseAddress, 
				 res->Data.Io.RangeLength, res->Data.Io.Alignment);
	    }
	    break;

	case ACPI_RSTYPE_FIXED_MEM32:
	    if (res->Data.FixedMemory32.RangeLength <= 0)
		break;
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "FixedMemory32 0x%x/%d\n",
			      res->Data.FixedMemory32.RangeBaseAddress, 
			      res->Data.FixedMemory32.RangeLength));
	    set->set_memory(dev, context, res->Data.FixedMemory32.RangeBaseAddress, 
			    res->Data.FixedMemory32.RangeLength);
	    break;

	case ACPI_RSTYPE_MEM32:
	    if (res->Data.Memory32.RangeLength <= 0)
		break;
	    if (res->Data.Memory32.MinBaseAddress == res->Data.Memory32.MaxBaseAddress) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory32 0x%x/%d\n",
				  res->Data.Memory32.MinBaseAddress, 
				  res->Data.Memory32.RangeLength));
		set->set_memory(dev, context,
				res->Data.Memory32.MinBaseAddress,
				res->Data.Memory32.RangeLength);
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory32 0x%x-0x%x/%d\n",
				  res->Data.Memory32.MinBaseAddress, 
				  res->Data.Memory32.MaxBaseAddress,
				  res->Data.Memory32.RangeLength));
		set->set_memoryrange(dev, context,
				     res->Data.Memory32.MinBaseAddress,
				     res->Data.Memory32.MaxBaseAddress,
				     res->Data.Memory32.RangeLength,
				     res->Data.Memory32.Alignment);
	    }
	    break;

	case ACPI_RSTYPE_MEM24:
	    if (res->Data.Memory24.RangeLength <= 0)
		break;
	    if (res->Data.Memory24.MinBaseAddress == res->Data.Memory24.MaxBaseAddress) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory24 0x%x/%d\n",
				  res->Data.Memory24.MinBaseAddress, 
				  res->Data.Memory24.RangeLength));
		set->set_memory(dev, context, res->Data.Memory24.MinBaseAddress,
				res->Data.Memory24.RangeLength);
	    } else {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Memory24 0x%x-0x%x/%d\n",
				  res->Data.Memory24.MinBaseAddress, 
				  res->Data.Memory24.MaxBaseAddress,
				  res->Data.Memory24.RangeLength));
		set->set_memoryrange(dev, context,
				     res->Data.Memory24.MinBaseAddress,
				     res->Data.Memory24.MaxBaseAddress,
				     res->Data.Memory24.RangeLength,
				     res->Data.Memory24.Alignment);
	    }
	    break;

	case ACPI_RSTYPE_IRQ:
	    /*
	     * from 1.0b 6.4.2 
	     * "This structure is repeated for each separate interrupt
	     * required"
	     */
	    set->set_irq(dev, context, res->Data.Irq.Interrupts,
			 res->Data.Irq.NumberOfInterrupts);
	    break;
	    
	case ACPI_RSTYPE_DMA:
	    /*
	     * from 1.0b 6.4.3 
	     * "This structure is repeated for each separate dma channel
	     * required"
	     */

	    set->set_drq(dev, context, res->Data.Dma.Channels,
			 res->Data.Dma.NumberOfChannels);
	    break;

	case ACPI_RSTYPE_START_DPF:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "start dependant functions\n"));
	    set->set_start_dependant(dev, context,
				     res->Data.StartDpf.CompatibilityPriority);
	    break;

	case ACPI_RSTYPE_END_DPF:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "end dependant functions\n"));
	    set->set_end_dependant(dev, context);
	    break;

	case ACPI_RSTYPE_ADDRESS32:
	    if (res->Data.Address32.AddressLength <= 0)
		break;
	    if (res->Data.Address32.ProducerConsumer != ACPI_CONSUMER) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "ignored Address32 %s producer\n",
				  (res->Data.Address32.ResourceType == ACPI_IO_RANGE) ?
				     "IO" : "Memory"));
		break;
	    }
	    if ((res->Data.Address32.ResourceType != ACPI_MEMORY_RANGE) ||
		(res->Data.Address32.ResourceType != ACPI_IO_RANGE)) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, 
				  "ignored Address32 for non-memory, non-I/O\n"));
		break;
	    }

	    if ((res->Data.Address32.MinAddressFixed == ACPI_ADDRESS_FIXED) &&
		(res->Data.Address32.MaxAddressFixed == ACPI_ADDRESS_FIXED)) {
		if (res->Data.Address32.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address32/Memory 0x%x/%d\n",
				      res->Data.Address32.MinAddressRange,
				      res->Data.Address32.AddressLength));
		    set->set_memory(dev, context,
				    res->Data.Address32.MinAddressRange,
				    res->Data.Address32.AddressLength);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address32/IO 0x%x/%d\n",
				      res->Data.Address32.MinAddressRange,
				      res->Data.Address32.AddressLength));
		    set->set_ioport(dev, context,
				    res->Data.Address32.MinAddressRange,
				    res->Data.Address32.AddressLength);
		}
	    } else {
		if (res->Data.Address32.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address32/Memory 0x%x-0x%x/%d\n",
				      res->Data.Address32.MinAddressRange,
				      res->Data.Address32.MaxAddressRange,
				      res->Data.Address32.AddressLength));
		    set->set_memoryrange(dev, context,
					  res->Data.Address32.MinAddressRange,
					  res->Data.Address32.MaxAddressRange,
					  res->Data.Address32.AddressLength,
					  res->Data.Address32.Granularity);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address32/IO 0x%x-0x%x/%d\n",
				      res->Data.Address32.MinAddressRange,
				      res->Data.Address32.MaxAddressRange,
				      res->Data.Address32.AddressLength));
		    set->set_iorange(dev, context,
				     res->Data.Address32.MinAddressRange,
				     res->Data.Address32.MaxAddressRange,
				     res->Data.Address32.AddressLength,
				     res->Data.Address32.Granularity);
		}
	    }		    
	    break;

	case ACPI_RSTYPE_ADDRESS16:
	    if (res->Data.Address16.AddressLength <= 0)
		break;
	    if (res->Data.Address16.ProducerConsumer != ACPI_CONSUMER) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "ignored Address16 %s producer\n",
				  (res->Data.Address16.ResourceType == ACPI_IO_RANGE) ?
				     "IO" : "Memory"));
		break;
	    }
	    if ((res->Data.Address16.ResourceType != ACPI_MEMORY_RANGE) ||
		(res->Data.Address16.ResourceType != ACPI_IO_RANGE)) {
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, 
				  "ignored Address16 for non-memory, non-I/O\n"));
		break;
	    }

	    if ((res->Data.Address16.MinAddressFixed == ACPI_ADDRESS_FIXED) &&
		(res->Data.Address16.MaxAddressFixed == ACPI_ADDRESS_FIXED)) {
		if (res->Data.Address16.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address16/Memory 0x%x/%d\n",
				      res->Data.Address16.MinAddressRange,
				      res->Data.Address16.AddressLength));
		    set->set_memory(dev, context,
				    res->Data.Address16.MinAddressRange,
				    res->Data.Address16.AddressLength);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address16/IO 0x%x/%d\n",
				      res->Data.Address16.MinAddressRange,
				      res->Data.Address16.AddressLength));
		    set->set_ioport(dev, context,
				    res->Data.Address16.MinAddressRange,
				    res->Data.Address16.AddressLength);
		}
	    } else {
		if (res->Data.Address16.ResourceType == ACPI_MEMORY_RANGE) {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address16/Memory 0x%x-0x%x/%d\n",
				      res->Data.Address16.MinAddressRange,
				      res->Data.Address16.MaxAddressRange,
				      res->Data.Address16.AddressLength));
		    set->set_memoryrange(dev, context,
					  res->Data.Address16.MinAddressRange,
					  res->Data.Address16.MaxAddressRange,
					  res->Data.Address16.AddressLength,
					  res->Data.Address16.Granularity);
		} else {
		    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "Address16/IO 0x%x-0x%x/%d\n",
				      res->Data.Address16.MinAddressRange,
				      res->Data.Address16.MaxAddressRange,
				      res->Data.Address16.AddressLength));
		    set->set_iorange(dev, context,
				     res->Data.Address16.MinAddressRange,
				     res->Data.Address16.MaxAddressRange,
				     res->Data.Address16.AddressLength,
				     res->Data.Address16.Granularity);
		}
	    }		    
	    break;

	case ACPI_RSTYPE_ADDRESS64:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "unimplemented Address64 resource\n"));
	    break;

	case ACPI_RSTYPE_EXT_IRQ:
	    /* XXX special handling? */
	    set->set_irq(dev, context,res->Data.ExtendedIrq.Interrupts,
			 res->Data.ExtendedIrq.NumberOfInterrupts);
	    break;

	case ACPI_RSTYPE_VENDOR:
	    ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "unimplemented VendorSpecific resource\n"));
	    break;
	default:
	    break;
	}
    }    
    AcpiOsFree(buf.Pointer);
    set->set_done(dev, context);
    return_ACPI_STATUS(AE_OK);
}

/*
 * Resource-set vectors used to attach _CRS-derived resources 
 * to an ACPI device.
 */
static void	acpi_res_set_init(device_t dev, void **context);
static void	acpi_res_set_done(device_t dev, void *context);
static void	acpi_res_set_ioport(device_t dev, void *context, u_int32_t base, u_int32_t length);
static void	acpi_res_set_iorange(device_t dev, void *context, u_int32_t low, u_int32_t high, 
				     u_int32_t length, u_int32_t align);
static void	acpi_res_set_memory(device_t dev, void *context, u_int32_t base, u_int32_t length);
static void	acpi_res_set_memoryrange(device_t dev, void *context, u_int32_t low, u_int32_t high, 
					 u_int32_t length, u_int32_t align);
static void	acpi_res_set_irq(device_t dev, void *context, u_int32_t *irq,
				 int count);
static void	acpi_res_set_drq(device_t dev, void *context, u_int32_t *drq,
				 int count);
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
    int		ar_ndrq;
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
acpi_res_set_irq(device_t dev, void *context, u_int32_t *irq, int count)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;
    
    if (cp == NULL)
	return;
    if (irq == NULL)
	return;

    /*This implements no resource relocation.*/
    if(count != 1)
	return;

    bus_set_resource(dev, SYS_RES_IRQ, cp->ar_nirq++, *irq, 1);
}

static void
acpi_res_set_drq(device_t dev, void *context, u_int32_t *drq, int count)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    if (drq == NULL)
	return;
    
    /*This implements no resource relocation.*/
    if(count != 1)
	return;

    bus_set_resource(dev, SYS_RES_DRQ, cp->ar_ndrq++, *drq, 1);
}

static void
acpi_res_set_start_dependant(device_t dev, void *context, int preference)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
    device_printf(dev, "dependant functions not supported\n");
}

static void
acpi_res_set_end_dependant(device_t dev, void *context)
{
    struct acpi_res_context	*cp = (struct acpi_res_context *)context;

    if (cp == NULL)
	return;
}

/*
 * Resource-owning placeholders.
 *
 * This code "owns" system resource objects that aren't
 * otherwise useful to devices, and which shouldn't be
 * considered "free".
 *
 * Note that some systems claim *all* of the physical address space
 * with a PNP0C01 device, so we cannot correctly "own" system memory
 * here (must be done in the SMAP handler on x86 systems, for
 * example).
 */

static int	acpi_sysresource_probe(device_t dev);
static int	acpi_sysresource_attach(device_t dev);

static device_method_t acpi_sysresource_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_sysresource_probe),
    DEVMETHOD(device_attach,	acpi_sysresource_attach),

    {0, 0}
};

static driver_t acpi_sysresource_driver = {
    "acpi_sysresource",
    acpi_sysresource_methods,
    0,
};

static devclass_t acpi_sysresource_devclass;
DRIVER_MODULE(acpi_sysresource, acpi, acpi_sysresource_driver, acpi_sysresource_devclass, 0, 0);

static int
acpi_sysresource_probe(device_t dev)
{
    if (acpi_disabled("sysresource"))
	return(ENXIO);
    if (acpi_MatchHid(dev, "PNP0C02")) {
	device_set_desc(dev, "system resource");
    } else {
	return(ENXIO);
    }
    device_quiet(dev);
    return(-100);
}

static int
acpi_sysresource_attach(device_t dev)
{
    struct resource	*res;
    int			i, rid;

    /*
     * Suck up all the resources that might have been assigned to us.
     * Note that it's impossible to tell the difference between a
     * resource that someone else has claimed, and one that doesn't
     * exist.
     */
    for (i = 0; i < 100; i++) {
	rid = i;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, 0);
	rid = i;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1, 0);
	rid = i;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_SHAREABLE);
    }
    return(0);
}
