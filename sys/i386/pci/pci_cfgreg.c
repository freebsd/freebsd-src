/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
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
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <i386/isa/pcibus.h>
#include <isa/isavar.h>

#include <machine/segments.h>
#include <machine/pc/bios.h>

#include "pcib_if.h"

static int cfgmech;
static int devmax;
static int usebios;

static int	pcibios_cfgread(int bus, int slot, int func, int reg,
				int bytes);
static void	pcibios_cfgwrite(int bus, int slot, int func, int reg,
				 int data, int bytes);
static int	pcibios_cfgopen(void);
static int	pcireg_cfgread(int bus, int slot, int func, int reg,
			       int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg,
				int data, int bytes);
static int	pcireg_cfgopen(void);

/* read configuration space register */

static int
nexus_pcib_maxslots(device_t dev)
{
	return 31;
}

static u_int32_t
nexus_pcib_read_config(device_t dev, int bus, int slot, int func,
		       int reg, int bytes)
{
	return(usebios ? 
	       pcibios_cfgread(bus, slot, func, reg, bytes) : 
	       pcireg_cfgread(bus, slot, func, reg, bytes));
}

/* write configuration space register */

static void
nexus_pcib_write_config(device_t dev, int bus, int slot, int func,
			int reg, u_int32_t data, int bytes)
{
	return(usebios ? 
	       pcibios_cfgwrite(bus, slot, func, reg, data, bytes) : 
	       pcireg_cfgwrite(bus, slot, func, reg, data, bytes));
}

/* initialise access to PCI configuration space */
static int
pci_cfgopen(void)
{
	if (pcibios_cfgopen() != 0) {
		usebios = 1;
	} else if (pcireg_cfgopen() != 0) {
		usebios = 0;
	} else {
		return(0);
	}
	return(1);
}

/* config space access using BIOS functions */

static int
pcibios_cfgread(int bus, int slot, int func, int reg, int bytes)
{
	struct bios_regs args;
	u_int mask;

	switch(bytes) {
	case 1:
		args.eax = PCIBIOS_READ_CONFIG_BYTE;
		mask = 0xff;
		break;
	case 2:
		args.eax = PCIBIOS_READ_CONFIG_WORD;
		mask = 0xffff;
		break;
	case 4:
		args.eax = PCIBIOS_READ_CONFIG_DWORD;
		mask = 0xffffffff;
		break;
	default:
		return(-1);
	}
	args.ebx = (bus << 8) | (slot << 3) | func;
	args.edi = reg;
	bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL));
	/* check call results? */
	return(args.ecx & mask);
}

static void
pcibios_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes)
{
	struct bios_regs args;

	switch(bytes) {
	case 1:
		args.eax = PCIBIOS_WRITE_CONFIG_BYTE;
		break;
	case 2:
		args.eax = PCIBIOS_WRITE_CONFIG_WORD;
		break;
	case 4:
		args.eax = PCIBIOS_WRITE_CONFIG_DWORD;
		break;
	default:
		return;
	}
	args.ebx = (bus << 8) | (slot << 3) | func;
	args.ecx = data;
	args.edi = reg;
	bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL));
}

/* determine whether there is a PCI BIOS present */

static int
pcibios_cfgopen(void)
{
	/* check for a found entrypoint */
	return(PCIbios.entry != 0);
}

/* configuration space access using direct register operations */

/* enable configuration space accesses and return data port address */

static int
pci_cfgenable(unsigned bus, unsigned slot, unsigned func, int reg, int bytes)
{
	int dataport = 0;

	if (bus <= PCI_BUSMAX
	    && slot < devmax
	    && func <= PCI_FUNCMAX
	    && reg <= PCI_REGMAX
	    && bytes != 3
	    && (unsigned) bytes <= 4
	    && (reg & (bytes -1)) == 0) {
		switch (cfgmech) {
		case 1:
			outl(CONF1_ADDR_PORT, (1 << 31)
			     | (bus << 16) | (slot << 11) 
			     | (func << 8) | (reg & ~0x03));
			dataport = CONF1_DATA_PORT + (reg & 0x03);
			break;
		case 2:
			outb(CONF2_ENABLE_PORT, 0xf0 | (func << 1));
			outb(CONF2_FORWARD_PORT, bus);
			dataport = 0xc000 | (slot << 8) | reg;
			break;
		}
	}
	return (dataport);
}

/* disable configuration space accesses */

static void
pci_cfgdisable(void)
{
	switch (cfgmech) {
	case 1:
		outl(CONF1_ADDR_PORT, 0);
		break;
	case 2:
		outb(CONF2_ENABLE_PORT, 0);
		outb(CONF2_FORWARD_PORT, 0);
		break;
	}
}

static int
pcireg_cfgread(int bus, int slot, int func, int reg, int bytes)
{
	int data = -1;
	int port;

	port = pci_cfgenable(bus, slot, func, reg, bytes);

	if (port != 0) {
		switch (bytes) {
		case 1:
			data = inb(port);
			break;
		case 2:
			data = inw(port);
			break;
		case 4:
			data = inl(port);
			break;
		}
		pci_cfgdisable();
	}
	return (data);
}

static void
pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes)
{
	int port;

	port = pci_cfgenable(bus, slot, func, reg, bytes);
	if (port != 0) {
		switch (bytes) {
		case 1:
			outb(port, data);
			break;
		case 2:
			outw(port, data);
			break;
		case 4:
			outl(port, data);
			break;
		}
		pci_cfgdisable();
	}
}

/* check whether the configuration mechanism has been correct identified */

static int
pci_cfgcheck(int maxdev)
{
	u_char device;

	if (bootverbose) 
		printf("pci_cfgcheck:\tdevice ");

	for (device = 0; device < maxdev; device++) {
		unsigned id, class, header;
		if (bootverbose) 
			printf("%d ", device);

		id = inl(pci_cfgenable(0, device, 0, 0, 4));
		if (id == 0 || id == -1)
			continue;

		class = inl(pci_cfgenable(0, device, 0, 8, 4)) >> 8;
		if (bootverbose)
			printf("[class=%06x] ", class);
		if (class == 0 || (class & 0xf870ff) != 0)
			continue;

		header = inb(pci_cfgenable(0, device, 0, 14, 1));
		if (bootverbose) 
			printf("[hdr=%02x] ", header);
		if ((header & 0x7e) != 0)
			continue;

		if (bootverbose)
			printf("is there (id=%08x)\n", id);

		pci_cfgdisable();
		return (1);
	}
	if (bootverbose) 
		printf("-- nothing found\n");

	pci_cfgdisable();
	return (0);
}

static int
pcireg_cfgopen(void)
{
	unsigned long mode1res,oldval1;
	unsigned char mode2res,oldval2;

	oldval1 = inl(CONF1_ADDR_PORT);

	if (bootverbose) {
		printf("pci_open(1):\tmode 1 addr port (0x0cf8) is 0x%08lx\n",
		       oldval1);
	}

	if ((oldval1 & CONF1_ENABLE_MSK) == 0) {

		cfgmech = 1;
		devmax = 32;

		outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK);
		outb(CONF1_ADDR_PORT +3, 0);
		mode1res = inl(CONF1_ADDR_PORT);
		outl(CONF1_ADDR_PORT, oldval1);

		if (bootverbose)
			printf("pci_open(1a):\tmode1res=0x%08lx (0x%08lx)\n", 
			       mode1res, CONF1_ENABLE_CHK);

		if (mode1res) {
			if (pci_cfgcheck(32)) 
				return (cfgmech);
		}

		outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK1);
		mode1res = inl(CONF1_ADDR_PORT);
		outl(CONF1_ADDR_PORT, oldval1);

		if (bootverbose)
			printf("pci_open(1b):\tmode1res=0x%08lx (0x%08lx)\n", 
			       mode1res, CONF1_ENABLE_CHK1);

		if ((mode1res & CONF1_ENABLE_MSK1) == CONF1_ENABLE_RES1) {
			if (pci_cfgcheck(32)) 
				return (cfgmech);
		}
	}

	oldval2 = inb(CONF2_ENABLE_PORT);

	if (bootverbose) {
		printf("pci_open(2):\tmode 2 enable port (0x0cf8) is 0x%02x\n",
		       oldval2);
	}

	if ((oldval2 & 0xf0) == 0) {

		cfgmech = 2;
		devmax = 16;

		outb(CONF2_ENABLE_PORT, CONF2_ENABLE_CHK);
		mode2res = inb(CONF2_ENABLE_PORT);
		outb(CONF2_ENABLE_PORT, oldval2);

		if (bootverbose)
			printf("pci_open(2a):\tmode2res=0x%02x (0x%02x)\n", 
			       mode2res, CONF2_ENABLE_CHK);

		if (mode2res == CONF2_ENABLE_RES) {
			if (bootverbose)
				printf("pci_open(2a):\tnow trying mechanism 2\n");

			if (pci_cfgcheck(16)) 
				return (cfgmech);
		}
	}

	cfgmech = 0;
	devmax = 0;
	return (cfgmech);
}

static devclass_t	pcib_devclass;

static const char *
nexus_pcib_is_host_bridge(int bus, int slot, int func,
			  u_int32_t id, u_int8_t class, u_int8_t subclass,
			  u_int8_t *busnum)
{
	const char *s = NULL;
	static u_int8_t pxb[4];	/* hack for 450nx */

	*busnum = 0;

	switch (id) {
	case 0x12258086:
		s = "Intel 824?? host to PCI bridge";
		/* XXX This is a guess */
		/* *busnum = nexus_pcib_read_config(0, bus, slot, func, 0x41, 1); */
		*busnum = bus;
		break;
	case 0x71208086:
		s = "Intel 82810 (i810 GMCH) Host To Hub bridge";
		break;
	case 0x71228086:
		s = "Intel 82810-DC100 (i810-DC100 GMCH) Host To Hub bridge";
		break;
	case 0x71248086:
		s = "Intel 82810E (i810E GMCH) Host To Hub bridge";
		break;
	case 0x71808086:
		s = "Intel 82443LX (440 LX) host to PCI bridge";
		break;
	case 0x71908086:
		s = "Intel 82443BX (440 BX) host to PCI bridge";
		break;
	case 0x71928086:
		s = "Intel 82443BX host to PCI bridge (AGP disabled)";
		break;
	case 0x71948086:
		s = "Intel 82443MX host to PCI bridge";
		break;
	case 0x71a08086:
		s = "Intel 82443GX host to PCI bridge";
		break;
	case 0x71a18086:
		s = "Intel 82443GX host to AGP bridge";
		break;
	case 0x71a28086:
		s = "Intel 82443GX host to PCI bridge (AGP disabled)";
		break;
	case 0x84c48086:
		s = "Intel 82454KX/GX (Orion) host to PCI bridge";
		*busnum = nexus_pcib_read_config(0, bus, slot, func, 0x4a, 1);
		break;
	case 0x84ca8086:
		/*
		 * For the 450nx chipset, there is a whole bundle of
		 * things pretending to be host bridges. The MIOC will 
		 * be seen first and isn't really a pci bridge (the
		 * actual busses are attached to the PXB's). We need to 
		 * read the registers of the MIOC to figure out the
		 * bus numbers for the PXB channels.
		 *
		 * Since the MIOC doesn't have a pci bus attached, we
		 * pretend it wasn't there.
		 */
		pxb[0] = nexus_pcib_read_config(0, bus, slot, func,
						0xd0, 1); /* BUSNO[0] */
		pxb[1] = nexus_pcib_read_config(0, bus, slot, func,
						0xd1, 1) + 1;	/* SUBA[0]+1 */
		pxb[2] = nexus_pcib_read_config(0, bus, slot, func,
						0xd3, 1); /* BUSNO[1] */
		pxb[3] = nexus_pcib_read_config(0, bus, slot, func,
						0xd4, 1) + 1;	/* SUBA[1]+1 */
		return NULL;
	case 0x84cb8086:
		switch (slot) {
		case 0x12:
			s = "Intel 82454NX PXB#0, Bus#A";
			*busnum = pxb[0];
			break;
		case 0x13:
			s = "Intel 82454NX PXB#0, Bus#B";
			*busnum = pxb[1];
			break;
		case 0x14:
			s = "Intel 82454NX PXB#1, Bus#A";
			*busnum = pxb[2];
			break;
		case 0x15:
			s = "Intel 82454NX PXB#1, Bus#B";
			*busnum = pxb[3];
			break;
		}
		break;

		/* AMD -- vendor 0x1022 */
	case 0x70061022:
		s = "AMD-751 host to PCI bridge";
		break;

		/* SiS -- vendor 0x1039 */
	case 0x04961039:
		s = "SiS 85c496";
		break;
	case 0x04061039:
		s = "SiS 85c501";
		break;
	case 0x06011039:
		s = "SiS 85c601";
		break;
	case 0x55911039:
		s = "SiS 5591 host to PCI bridge";
		break;
	case 0x00011039:
		s = "SiS 5591 host to AGP bridge";
		break;

		/* VLSI -- vendor 0x1004 */
	case 0x00051004:
		s = "VLSI 82C592 Host to PCI bridge";
		break;

		/* XXX Here is MVP3, I got the datasheet but NO M/B to test it  */
		/* totally. Please let me know if anything wrong.            -F */
		/* XXX need info on the MVP3 -- any takers? */
	case 0x05981106:
		s = "VIA 82C598MVP (Apollo MVP3) host bridge";
		break;

		/* AcerLabs -- vendor 0x10b9 */
		/* Funny : The datasheet told me vendor id is "10b8",sub-vendor */
		/* id is '10b9" but the register always shows "10b9". -Foxfair  */
	case 0x154110b9:
		s = "AcerLabs M1541 (Aladdin-V) PCI host bridge";
		break;

		/* OPTi -- vendor 0x1045 */
	case 0xc7011045:
		s = "OPTi 82C700 host to PCI bridge";
		break;
	case 0xc8221045:
		s = "OPTi 82C822 host to PCI Bridge";
		break;

		/* RCC -- vendor 0x1166 */
	case 0x00051166:
		s = "RCC HE host to PCI bridge";
		*busnum = nexus_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;
	
	case 0x00061166:
		/* FALLTHROUGH */
	case 0x00081166:
		s = "RCC host to PCI bridge";
		*busnum = nexus_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;

	case 0x00091166:
		s = "RCC LE host to PCI bridge";
		*busnum = nexus_pcib_read_config(0, bus, slot, func, 0x44, 1);
		break;

		/* Integrated Micro Solutions -- vendor 0x10e0 */
	case 0x884910e0:
		s = "Integrated Micro Solutions VL Bridge";
		break;

	default:
		if (class == PCIC_BRIDGE && subclass == PCIS_BRIDGE_HOST)
			s = "Host to PCI bridge";
		break;
	}

	return s;
}

/*
 * Scan the first pci bus for host-pci bridges and add pcib instances
 * to the nexus for each bridge.
 */
static void
nexus_pcib_identify(driver_t *driver, device_t parent)
{
	int bus, slot, func;
	u_int8_t  hdrtype;
	int found = 0;
	int pcifunchigh;
	int found824xx = 0;
	device_t child;
	int *ivar;

	if (pci_cfgopen() == 0)
		return;
	bus = 0;
 retry:
	for (slot = 0; slot <= PCI_SLOTMAX; slot++) {
		func = 0;
		hdrtype = nexus_pcib_read_config(0, bus, slot, func,
						 PCIR_HEADERTYPE, 1);
		if (hdrtype & PCIM_MFDEV)
			pcifunchigh = 7;
		else
			pcifunchigh = 0;
		for (func = 0; func <= pcifunchigh; func++) {
			/*
			 * Read the IDs and class from the device.
			 */
			u_int32_t id;
			u_int8_t class, subclass, busnum;
			const char *s;
			device_t *devs;
			int ndevs, i;

			id = nexus_pcib_read_config(0, bus, slot, func,
						    PCIR_DEVVENDOR, 4);
			if (id == -1)
				continue;
			class = nexus_pcib_read_config(0, bus, slot, func,
						       PCIR_CLASS, 1);
			subclass = nexus_pcib_read_config(0, bus, slot, func,
							  PCIR_SUBCLASS, 1);

			s = nexus_pcib_is_host_bridge(bus, slot, func,
						      id, class, subclass,
						      &busnum);
			if (s == NULL)
				continue;

			/*
			 * Check to see if the physical bus has already
			 * been seen.  Eg: hybrid 32 and 64 bit host
			 * bridges to the same logical bus.
			 */
			if (device_get_children(parent, &devs, &ndevs) == 0) {
				for (i = 0; s != NULL && i < ndevs; i++) {
					if (strcmp(device_get_name(devs[i]),
					    "pcib") != 0)
						continue;
					ivar = device_get_ivars(devs[i]);
					if (ivar == NULL)
						continue;
					if (busnum == *ivar)
						s = NULL;
				}
				free(devs, M_TEMP);
			}

			if (s == NULL)
				continue;
			/*
			 * Add at priority 100 to make sure we
			 * go after any motherboard resources
			 */
			child = BUS_ADD_CHILD(parent, 100,
					      "pcib", busnum);
			device_set_desc(child, s);

			ivar = malloc(sizeof ivar[0], M_DEVBUF,
				      M_NOWAIT);
			if (ivar == NULL)
				panic("out of memory");
			device_set_ivars(child, ivar);
			ivar[0] = busnum;

			found = 1;
			if (id == 0x12258086)
				found824xx = 1;
		}
	}
	if (found824xx && bus == 0) {
		bus++;
		goto retry;
	}

	/*
	 * Make sure we add at least one bridge since some old
	 * hardware doesn't actually have a host-pci bridge device.
	 * Note that pci_cfgopen() thinks we have PCI devices..
	 */
	if (!found) {
		if (bootverbose)
			printf(
	"nexus_pcib_identify: no bridge found, adding pcib0 anyway\n");
		child = BUS_ADD_CHILD(parent, 100, "pcib", 0);
		ivar = malloc(sizeof ivar[0], M_DEVBUF, M_NOWAIT);
		if (ivar == NULL)
			panic("out of memory");
		device_set_ivars(child, ivar);
		ivar[0] = 0;
	}
}

static int
nexus_pcib_probe(device_t dev)
{

	if (pci_cfgopen() != 0)
		return 0;

	return ENXIO;
}

static int
nexus_pcib_attach(device_t dev)
{
	device_t child;

	child = device_add_child(dev, "pci", device_get_unit(dev));

	return bus_generic_attach(dev);
}

static int
nexus_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{

	switch (which) {
	case  PCIB_IVAR_BUS:
		*result = *(int*) device_get_ivars(dev);
		return 0;
	}
	return ENOENT;
}

static int
nexus_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{

	switch (which) {
	case  PCIB_IVAR_BUS:
		*(int*) device_get_ivars(dev) = value;
		return 0;
	}
	return ENOENT;
}


static device_method_t nexus_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	nexus_pcib_identify),
	DEVMETHOD(device_probe,		nexus_pcib_probe),
	DEVMETHOD(device_attach,	nexus_pcib_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	nexus_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	nexus_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	nexus_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	nexus_pcib_read_config),
	DEVMETHOD(pcib_write_config,	nexus_pcib_write_config),

	{ 0, 0 }
};

static driver_t nexus_pcib_driver = {
	"pcib",
	nexus_pcib_methods,
	1,
};

DRIVER_MODULE(pcib, nexus, nexus_pcib_driver, pcib_devclass, 0, 0);


/*
 * Provide a device to "eat" the host->pci bridges that we dug up above
 * and stop them showing up twice on the probes.  This also stops them
 * showing up as 'none' in pciconf -l.
 */
static int
pci_hostb_probe(device_t dev)
{

	if (pci_get_class(dev) == PCIC_BRIDGE &&
	    pci_get_subclass(dev) == PCIS_BRIDGE_HOST) {
		device_set_desc(dev, "Host to PCI bridge");
		device_quiet(dev);
		return 0;
	}
	return ENXIO;
}

static int
pci_hostb_attach(device_t dev)
{

	return 0;
}

static device_method_t pci_hostb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pci_hostb_probe),
	DEVMETHOD(device_attach,	pci_hostb_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	{ 0, 0 }
};
static driver_t pci_hostb_driver = {
	"hostb",
	pci_hostb_methods,
	1,
};
static devclass_t pci_hostb_devclass;

DRIVER_MODULE(hostb, pci, pci_hostb_driver, pci_hostb_devclass, 0, 0);


/*
 * Install placeholder to claim the resources owned by the
 * PCI bus interface.  This could be used to extract the 
 * config space registers in the extreme case where the PnP
 * ID is available and the PCI BIOS isn't, but for now we just
 * eat the PnP ID and do nothing else.
 *
 * XXX we should silence this probe, as it will generally confuse 
 * people.
 */
static struct isa_pnp_id pcibus_pnp_ids[] = {
	{ 0x030ad041 /* PNP030A */, "PCI Bus" },
	{ 0 }
};

static int
pcibus_pnp_probe(device_t dev)
{
	int result;
	
	if ((result = ISA_PNP_PROBE(device_get_parent(dev), dev, pcibus_pnp_ids)) <= 0)
		device_quiet(dev);
	return(result);
}

static int
pcibus_pnp_attach(device_t dev)
{
	return(0);
}

static device_method_t pcibus_pnp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcibus_pnp_probe),
	DEVMETHOD(device_attach,	pcibus_pnp_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	{ 0, 0 }
};

static driver_t pcibus_pnp_driver = {
	"pcibus_pnp",
	pcibus_pnp_methods,
	1,		/* no softc */
};

static devclass_t pcibus_pnp_devclass;

DRIVER_MODULE(pcibus_pnp, isa, pcibus_pnp_driver, pcibus_pnp_devclass, 0, 0);
