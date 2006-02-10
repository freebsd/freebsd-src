/*-
 * Copyright (c) 2006 IronPort Systems Inc. <ambrisko@ironport.com>
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

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/selinfo.h>

#include <sys/bus.h>
#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#ifdef LOCAL_MODULE
#include <ipmivars.h>
#else
#include <dev/ipmi/ipmivars.h>
#endif

static int ipmi_pci_probe(device_t dev);
static int ipmi_pci_attach(device_t dev);
static int ipmi_pci_detach(device_t dev);

static device_method_t ipmi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     ipmi_pci_probe),
	DEVMETHOD(device_attach,    ipmi_pci_attach),
	DEVMETHOD(device_detach,    ipmi_pci_detach),
	{ 0, 0 }
};

struct ipmi_ident
{
	u_int16_t	vendor;
	u_int16_t	device;
	char		*desc;
} ipmi_identifiers[] = {
	{0x1028, 0x000d, "Dell PE2650 SMIC interface"},
	{0, 0, 0}
};

static int
ipmi_pci_probe(device_t dev) {
	struct ipmi_ident *m;

	if (ipmi_attached)
		return ENXIO;

	for (m = ipmi_identifiers; m->vendor != 0; m++) {
		if ((m->vendor == pci_get_vendor(dev)) &&
		    (m->device == pci_get_device(dev))) {
			device_set_desc(dev, m->desc);
			return (BUS_PROBE_DEFAULT);
		}
	}

	return ENXIO;
}

static int
ipmi_pci_attach(device_t dev) {
	struct ipmi_softc *sc = device_get_softc(dev);
	device_t parent, smbios_attach_dev = NULL;
	devclass_t dc;
	int status, flags;
	int error;


	/*
	 * We need to attach to something that can address the BIOS/
	 * SMBIOS memory range.  This is usually the isa bus however
	 * during a static kernel boot the isa bus is not available
	 * so we run up the tree to the nexus bus.  A module load
	 * will use the isa bus attachment.  If neither work bail
	 * since the SMBIOS defines stuff we need to know to attach to
	 * this device.
	 */
	dc = devclass_find("isa");
	if (dc != NULL) {
		smbios_attach_dev = devclass_get_device(dc, 0);
	}

	if (smbios_attach_dev == NULL) {
		smbios_attach_dev = dev;
		for (;;) {
			parent = device_get_parent(smbios_attach_dev);
			if (parent == NULL)
				break;
			if (strcmp(device_get_name(smbios_attach_dev),
			    "nexus") == 0)
				break;
			smbios_attach_dev = parent;
		} 
	}
	
	if (smbios_attach_dev == NULL) {
		device_printf(dev, "Couldn't find isa/nexus device\n");
		goto bad;
	}
	sc->ipmi_smbios_dev = ipmi_smbios_identify(NULL, smbios_attach_dev);
	if (sc->ipmi_smbios_dev == NULL) {
		device_printf(dev, "Couldn't find isa device\n");
		goto bad;
	}
	error = ipmi_smbios_probe(sc->ipmi_smbios_dev);
	if (error != 0) {
		goto bad;
	}
	sc->ipmi_dev = dev;
	error = ipmi_smbios_query(dev);
	device_delete_child(dev, sc->ipmi_smbios_dev);
	if (error != 0)
		goto bad;

	/* Now we know about the IPMI attachment info. */
	if (sc->ipmi_bios_info.kcs_mode) {
		if (sc->ipmi_bios_info.io_mode)
			device_printf(dev, "KCS mode found at io 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));
		else
			device_printf(dev, "KCS mode found at mem 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));

		sc->ipmi_kcs_status_reg   = sc->ipmi_bios_info.offset;
		sc->ipmi_kcs_command_reg  = sc->ipmi_bios_info.offset;
		sc->ipmi_kcs_data_out_reg = 0;
		sc->ipmi_kcs_data_in_reg  = 0;

		if (sc->ipmi_bios_info.io_mode) {
			sc->ipmi_io_rid = PCIR_BAR(0);
			sc->ipmi_io_res = bus_alloc_resource(dev,
			    SYS_RES_IOPORT, &sc->ipmi_io_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
				(sc->ipmi_bios_info.offset * 2),
			    sc->ipmi_bios_info.offset * 2,
			    RF_ACTIVE);
		} else {
			sc->ipmi_mem_rid = PCIR_BAR(0);
			sc->ipmi_mem_res = bus_alloc_resource(dev,
			    SYS_RES_MEMORY, &sc->ipmi_mem_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
			        (sc->ipmi_bios_info.offset * 2),
			    sc->ipmi_bios_info.offset * 2,
			    RF_ACTIVE);
		}

		if (!sc->ipmi_io_res){
			device_printf(dev, "couldn't configure pci io res\n");
			goto bad;
		}

		status = INB(sc, sc->ipmi_kcs_status_reg);
		if (status == 0xff) {
			device_printf(dev, "couldn't find it\n");
			goto bad;
		}
		if(status & KCS_STATUS_OBF){
			ipmi_read(dev, NULL, 0);
		}
	} else if (sc->ipmi_bios_info.smic_mode) {
		if (sc->ipmi_bios_info.io_mode)
			device_printf(dev, "SMIC mode found at io 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));
		else
			device_printf(dev, "SMIC mode found at mem 0x%llx "
			    "alignment 0x%x on %s\n",
			    (long long)sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.offset,
			    device_get_name(device_get_parent(sc->ipmi_dev)));

		sc->ipmi_smic_data    = 0;
		sc->ipmi_smic_ctl_sts = sc->ipmi_bios_info.offset;
		sc->ipmi_smic_flags   = sc->ipmi_bios_info.offset * 2;

		if (sc->ipmi_bios_info.io_mode) {
			sc->ipmi_io_rid = PCIR_BAR(0);
			sc->ipmi_io_res = bus_alloc_resource(dev,
			    SYS_RES_IOPORT, &sc->ipmi_io_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
				(sc->ipmi_bios_info.offset * 3),
			    sc->ipmi_bios_info.offset * 3,
			    RF_ACTIVE);
		} else {
			sc->ipmi_mem_rid = PCIR_BAR(0);
			sc->ipmi_mem_res = bus_alloc_resource(dev,
			    SYS_RES_MEMORY, &sc->ipmi_mem_rid,
			    sc->ipmi_bios_info.address,
			    sc->ipmi_bios_info.address +
			        (sc->ipmi_bios_info.offset * 2),
			    sc->ipmi_bios_info.offset * 2,
			    RF_ACTIVE);
		}

		if (!sc->ipmi_io_res && !sc->ipmi_mem_res){
			device_printf(dev, "couldn't configure pci res\n");
			goto bad;
		}

		flags = INB(sc, sc->ipmi_smic_flags);
		if (flags == 0xff) {
			device_printf(dev, "couldn't find it\n");
			goto bad;
		}
		if ((flags & SMIC_STATUS_SMS_ATN)
		    && (flags & SMIC_STATUS_RX_RDY)){
			ipmi_read(dev, NULL, 0);
		}
	} else {
		device_printf(dev, "No IPMI interface found\n");
		goto bad;
	}
	ipmi_attach(dev);

	sc->ipmi_irq_rid = 0;
        sc->ipmi_irq_res = bus_alloc_resource_any(sc->ipmi_dev, SYS_RES_IRQ,
	    &sc->ipmi_irq_rid, RF_SHAREABLE | RF_ACTIVE);
	if (sc->ipmi_irq_res == NULL) {
                device_printf(sc->ipmi_dev, "can't allocate interrupt\n");
        } else {
		if (bus_setup_intr(sc->ipmi_dev, sc->ipmi_irq_res,
				   INTR_TYPE_MISC, ipmi_intr,
				   sc->ipmi_dev, &sc->ipmi_irq)) {
			device_printf(sc->ipmi_dev,
			    "can't set up interrupt\n");
			return (EINVAL);
		}
	}

	return 0;
bad:
	return ENXIO;
}

static int ipmi_pci_detach(device_t dev) {
	struct ipmi_softc *sc;

	sc = device_get_softc(dev);
	ipmi_detach(dev);
	if (sc->ipmi_ev_tag)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ipmi_ev_tag);

	if (sc->ipmi_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->ipmi_mem_rid,
		    sc->ipmi_mem_res);
	if (sc->ipmi_io_res)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->ipmi_io_rid,
		    sc->ipmi_io_res);
	if (sc->ipmi_irq)
		bus_teardown_intr(sc->ipmi_dev, sc->ipmi_irq_res,
		    sc->ipmi_irq);
	if (sc->ipmi_irq_res)
		bus_release_resource(sc->ipmi_dev, SYS_RES_IRQ,
		    sc->ipmi_irq_rid, sc->ipmi_irq_res);

	return 0;
}

static driver_t ipmi_pci_driver = {
        "ipmi",
        ipmi_methods,
        sizeof(struct ipmi_softc)
};

DRIVER_MODULE(ipmi_foo, pci, ipmi_pci_driver, ipmi_devclass, 0, 0);
