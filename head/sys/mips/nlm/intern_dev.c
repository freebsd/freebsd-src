/*-
 * Copyright (c) 2011 Netlogic Microsystems Inc.
 *
 * (based on pci/ignore_pci.c)
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * 'Ignore' driver - eats devices that show up errnoeously on PCI
 * but shouldn't ever be listed or handled by a driver.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>

#include <mips/nlm/hal/haldefs.h>
#include <mips/nlm/hal/iomap.h>

static int	nlm_soc_pci_probe(device_t dev);

static device_method_t nlm_soc_pci_methods[] = {
	DEVMETHOD(device_probe,		nlm_soc_pci_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	{ 0, 0 }
};

static driver_t nlm_soc_pci_driver = {
    "nlm_soc_pci",
    nlm_soc_pci_methods,
    0,
};

static devclass_t nlm_soc_pci_devclass;
DRIVER_MODULE(nlm_soc_pci, pci, nlm_soc_pci_driver, nlm_soc_pci_devclass, 0, 0);

static int
nlm_soc_pci_probe(device_t dev)
{
	if (pci_get_vendor(dev) != PCI_VENDOR_NETLOGIC)
		return(ENXIO);

	/* Ignore SoC internal devices */
	switch (pci_get_device(dev)) {
	case PCI_DEVICE_ID_NLM_ICI: 
	case PCI_DEVICE_ID_NLM_PIC: 
	case PCI_DEVICE_ID_NLM_FMN: 
		device_set_desc(dev, "Netlogic Internal");
		device_quiet(dev);
		return(-10000);

	default:
		return(ENXIO);
	}
}
