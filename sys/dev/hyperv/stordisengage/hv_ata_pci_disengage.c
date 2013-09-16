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

#define HV_X64_MSR_GUEST_OS_ID	0x40000000
#define HV_X64_CPUID_MIN	0x40000005
#define HV_X64_CPUID_MAX	0x4000ffff

/* prototypes */
static int hv_ata_pci_probe(device_t dev);
static int hv_ata_pci_attach(device_t dev);
static int hv_ata_pci_detach(device_t dev);

static int hv_check_for_hyper_v(void);

/*
 * generic PCI ATA device probe
 */
static int
hv_ata_pci_probe(device_t dev)
{
	int ata_disk_enable = 0;
	if(bootverbose)
		device_printf(dev,
		    "hv_ata_pci_probe dev_class/subslcass = %d, %d\n",
			pci_get_class(dev), pci_get_subclass(dev));
			
	/* is this a storage class device ? */
	if (pci_get_class(dev) != PCIC_STORAGE)
		return (ENXIO);

	/* is this an IDE/ATA type device ? */
	if (pci_get_subclass(dev) != PCIS_STORAGE_IDE)
		return (ENXIO);

	if(bootverbose)
		device_printf(dev,
		    "Hyper-V probe for disabling ATA-PCI, emulated driver\n");

	/*
	 * On Hyper-V the default is to use the enlightened driver for
	 * IDE disks. However, if the user wishes to use the native
	 * ATA driver, the environment variable
	 * hw_ata.disk_enable must be explicitly set to 1.
	 */
	if (hv_check_for_hyper_v()) {
		if (getenv_int("hw.ata.disk_enable", &ata_disk_enable)) {
			if(bootverbose)
				device_printf(dev,
				"hw.ata.disk_enable flag is disabling Hyper-V"
				" ATA driver support\n");
			return (ENXIO);
		}

	}

	if(bootverbose)
		device_printf(dev, "Hyper-V ATA storage driver enabled.\n");

	return (BUS_PROBE_VENDOR);
}

static int
hv_ata_pci_attach(device_t dev)
{
	return 0;
}

static int
hv_ata_pci_detach(device_t dev)
{
	return 0;
}

/**
* Detect Hyper-V and enable fast IDE
* via enlighted storage driver
*/
static int
hv_check_for_hyper_v(void)
{
	u_int regs[4];
	int hyper_v_detected = 0;
	do_cpuid(1, regs);
	if (regs[2] & 0x80000000) {
		/* if(a hypervisor is detected) */
		/* make sure this really is Hyper-V */
		/* we look at the CPUID info */
		do_cpuid(HV_X64_MSR_GUEST_OS_ID, regs);
		hyper_v_detected =
			regs[0] >= HV_X64_CPUID_MIN &&
			regs[0] <= HV_X64_CPUID_MAX &&
			!memcmp("Microsoft Hv", &regs[1], 12);
	}
	return (hyper_v_detected);
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
    "pciata-disable",
    hv_ata_pci_methods,
    sizeof(struct ata_pci_controller),
};

DRIVER_MODULE(atapci_dis, pci, hv_ata_pci_disengage_driver,
		hv_ata_pci_devclass, NULL, NULL);
MODULE_VERSION(atapci_dis, 1);
MODULE_DEPEND(atapci_dis, ata, 1, 1, 1);


