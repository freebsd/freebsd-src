/*-
 * Copyright (c) 2003 Nate Lawson
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/sbuf.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>

/*
 * Package manipulation convenience functions
 */

int
acpi_PkgInt(device_t dev, ACPI_OBJECT *res, int idx, ACPI_INTEGER *dst)
{
    ACPI_OBJECT		*obj;

    obj = &res->Package.Elements[idx];
    if (obj == NULL || obj->Type != ACPI_TYPE_INTEGER) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "PkgInt error, pkg[%d] at %d %p\n",
		    res->Package.Count, idx, obj);
	return (-1);
    }

    *dst = obj->Integer.Value;
    return (0);
}

int
acpi_PkgInt32(device_t dev, ACPI_OBJECT *res, int idx, uint32_t *dst)
{
    ACPI_INTEGER tmp;
    int error;

    error = acpi_PkgInt(dev, res, idx, &tmp);
    if (error == 0)
	*dst = (uint32_t)tmp;
    return (error);
}

int
acpi_PkgStr(device_t dev, ACPI_OBJECT *res, int idx, void *dst, size_t size)
{
    ACPI_OBJECT		*obj;
    void		*ptr;
    size_t		 length;

    obj = &res->Package.Elements[idx];
    if (obj == NULL) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "PkgStr NULL object at %d\n", idx);
	return (-1);
    }
    bzero(dst, sizeof(dst));

    switch (obj->Type) {
    case ACPI_TYPE_STRING:
	ptr = obj->String.Pointer;
	length = obj->String.Length;
	break;
    case ACPI_TYPE_BUFFER:
	ptr = obj->Buffer.Pointer;
	length = obj->Buffer.Length;
	break;
    default:
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "PkgStr: invalid object type %d at %d\n", obj->Type, idx);
	return (-1);
    }

    /* Make sure string will fit, including terminating NUL */
    if (++length > size) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "PkgStr string too long (%zu bytes) at %d\n", length, idx);
	return (-1);
    }

    strlcpy(dst, ptr, length);
    return (0);
}

int
acpi_PkgGas(device_t dev, ACPI_OBJECT *res, int idx, int *rid,
	    struct resource **dst)
{
    ACPI_GENERIC_ADDRESS gas;
    ACPI_OBJECT		*obj;

    obj = &res->Package.Elements[idx];
    if (obj == NULL || obj->Type != ACPI_TYPE_BUFFER ||
	obj->Buffer.Length < sizeof(ACPI_GENERIC_ADDRESS) + 3) {

	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "PkgGas error at %d\n", idx);
	return (-1);
    }

    memcpy(&gas, obj->Buffer.Pointer + 3, sizeof(gas));
    *dst = acpi_bus_alloc_gas(dev, rid, &gas);
    if (*dst == NULL) {
	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "PkgGas error at %d\n", idx);
	return (-1);
    }

    return (0);
}
