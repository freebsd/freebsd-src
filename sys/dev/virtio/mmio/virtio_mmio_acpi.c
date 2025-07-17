/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * VirtIO MMIO interface.
 * This driver is heavily based on VirtIO PCI interface driver.
 */

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <dev/virtio/mmio/virtio_mmio.h>

static int	vtmmio_acpi_probe(device_t);

static device_method_t vtmmio_acpi_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		vtmmio_acpi_probe),

	DEVMETHOD_END
};

DEFINE_CLASS_1(virtio_mmio, vtmmio_acpi_driver, vtmmio_acpi_methods,
    sizeof(struct vtmmio_softc), vtmmio_driver);

DRIVER_MODULE(virtio_mmio, acpi, vtmmio_acpi_driver, 0,0);
MODULE_DEPEND(virtio_mmio, acpi, 1, 1, 1);

static int
vtmmio_acpi_probe(device_t dev)
{
	ACPI_HANDLE h;

	if ((h = acpi_get_handle(dev)) == NULL)
		return (ENXIO);

	if (!acpi_MatchHid(h, "LNRO0005"))
		return (ENXIO);

	return (vtmmio_probe(dev));
}
