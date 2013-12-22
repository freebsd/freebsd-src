/*-
 * Copyright (c) 1998 - 2008 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2009-2013 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/ata.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>
#include <ata_if.h>

/* prototypes */
static int hv_ata_pci_probe(device_t dev);
static int hv_ata_pci_attach(device_t dev);
static int hv_ata_pci_detach(device_t dev);

/*
 * generic PCI ATA device probe
 */
static int
hv_ata_pci_probe(device_t dev)
{
	device_t parent = device_get_parent(dev);
	int ata_disk_enable;

	ata_disk_enable = 0;

	/*
	 * Don't probe if not running in a Hyper-V environment
	 */
	if (vm_guest != VM_GUEST_HV)
		return (ENXIO);

	if (device_get_unit(parent) != 0 || device_get_ivars(dev) != 0)
		return (ENXIO);

	/*
	 * On Hyper-V the default is to use the enlightened driver for
	 * IDE disks. However, if the user wishes to use the native
	 * ATA driver, the environment variable
	 * hw_ata.disk_enable must be explicitly set to 1.
	 */
	if (getenv_int("hw.ata.disk_enable", &ata_disk_enable)) {
		if (bootverbose)
			device_printf(dev,
			    "hw.ata.disk_enable flag is disabling Hyper-V"
			    " ATA driver support\n");
			return (ENXIO);
	}

	device_set_desc(dev, "Hyper-V ATA storage disengage driver");

	return (BUS_PROBE_VENDOR);
}

static int
hv_ata_pci_attach(device_t dev)
{

	return (0);
}

static int
hv_ata_pci_detach(device_t dev)
{

	return (0);
}

static device_method_t hv_ata_pci_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,	hv_ata_pci_probe),
	DEVMETHOD(device_attach,	hv_ata_pci_attach),
	DEVMETHOD(device_detach,	hv_ata_pci_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	DEVMETHOD_END
};

devclass_t hv_ata_pci_devclass;

static driver_t hv_ata_pci_disengage_driver = {
	"ata",
	hv_ata_pci_methods,
	0,
};

DRIVER_MODULE(atapci_dis, atapci, hv_ata_pci_disengage_driver,
    hv_ata_pci_devclass, NULL, NULL);
MODULE_VERSION(atapci_dis, 1);
MODULE_DEPEND(atapci_dis, ata, 1, 1, 1);
