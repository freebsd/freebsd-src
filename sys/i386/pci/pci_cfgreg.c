/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
 * Copyright (c) 2004, Scott Long <scottl@freebsd.org>
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
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <machine/pci_cfgreg.h>
#include <machine/pc/bios.h>

#define PRVERB(a) do {							\
	if (bootverbose)						\
		printf a ;						\
} while(0)

static int cfgmech;
static int devmax;

static int	pcireg_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);
static int	pcireg_cfgopen(void);

static struct mtx pcicfg_mtx;

/*
 * Some BIOS writers seem to want to ignore the spec and put
 * 0 in the intline rather than 255 to indicate none.  Some use
 * numbers in the range 128-254 to indicate something strange and
 * apparently undocumented anywhere.  Assume these are completely bogus
 * and map them to 255, which means "none".
 */
static __inline int 
pci_i386_map_intline(int line)
{
	if (line == 0 || line >= 128)
		return (PCI_INVALID_IRQ);
	return (line);
}

static u_int16_t
pcibios_get_version(void)
{
	struct bios_regs args;

	if (PCIbios.ventry == 0) {
		PRVERB(("pcibios: No call entry point\n"));
		return (0);
	}
	args.eax = PCIBIOS_BIOS_PRESENT;
	if (bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL))) {
		PRVERB(("pcibios: BIOS_PRESENT call failed\n"));
		return (0);
	}
	if (args.edx != 0x20494350) {
		PRVERB(("pcibios: BIOS_PRESENT didn't return 'PCI ' in edx\n"));
		return (0);
	}
	return (args.ebx & 0xffff);
}

/* 
 * Initialise access to PCI configuration space 
 */
int
pci_cfgregopen(void)
{
	static int		opened = 0;
	u_int16_t		v;

	if (opened)
		return(1);

	if (pcireg_cfgopen() == 0)
		return(0);

	v = pcibios_get_version();
	if (v > 0)
		PRVERB(("pcibios: BIOS version %x.%02x\n", (v & 0xff00) >> 8,
		    v & 0xff));
	mtx_init(&pcicfg_mtx, "pcicfg", NULL, MTX_SPIN);
	opened = 1;

	/* $PIR requires PCI BIOS 2.10 or greater. */
	if (v >= 0x0210)
		pci_pir_open();
	return(1);
}

/* 
 * Read configuration space register
 */
u_int32_t
pci_cfgregread(int bus, int slot, int func, int reg, int bytes)
{
	uint32_t line;

	/*
	 * Some BIOS writers seem to want to ignore the spec and put
	 * 0 in the intline rather than 255 to indicate none.  The rest of
	 * the code uses 255 as an invalid IRQ.
	 */
	if (reg == PCIR_INTLINE && bytes == 1) {
		line = pcireg_cfgread(bus, slot, func, PCIR_INTLINE, 1);
		return (pci_i386_map_intline(line));
	}
	return (pcireg_cfgread(bus, slot, func, reg, bytes));
}

/* 
 * Write configuration space register 
 */
void
pci_cfgregwrite(int bus, int slot, int func, int reg, u_int32_t data, int bytes)
{

	pcireg_cfgwrite(bus, slot, func, reg, data, bytes);
}

/* 
 * Configuration space access using direct register operations
 */

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
	    && (reg & (bytes - 1)) == 0) {
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

	mtx_lock_spin(&pcicfg_mtx);
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
	mtx_unlock_spin(&pcicfg_mtx);
	return (data);
}

static void
pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes)
{
	int port;

	mtx_lock_spin(&pcicfg_mtx);
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
	mtx_unlock_spin(&pcicfg_mtx);
}

/* check whether the configuration mechanism has been correctly identified */
static int
pci_cfgcheck(int maxdev)
{
	uint32_t id, class;
	uint8_t header;
	uint8_t device;
	int port;

	if (bootverbose) 
		printf("pci_cfgcheck:\tdevice ");

	for (device = 0; device < maxdev; device++) {
		if (bootverbose) 
			printf("%d ", device);

		port = pci_cfgenable(0, device, 0, 0, 4);
		id = inl(port);
		if (id == 0 || id == 0xffffffff)
			continue;

		port = pci_cfgenable(0, device, 0, 8, 4);
		class = inl(port) >> 8;
		if (bootverbose)
			printf("[class=%06x] ", class);
		if (class == 0 || (class & 0xf870ff) != 0)
			continue;

		port = pci_cfgenable(0, device, 0, 14, 1);
		header = inb(port);
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
	uint32_t mode1res, oldval1;
	uint8_t mode2res, oldval2;

	oldval1 = inl(CONF1_ADDR_PORT);

	if (bootverbose) {
		printf("pci_open(1):\tmode 1 addr port (0x0cf8) is 0x%08x\n",
		    oldval1);
	}

	if ((oldval1 & CONF1_ENABLE_MSK) == 0) {

		cfgmech = 1;
		devmax = 32;

		outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK);
		DELAY(1);
		mode1res = inl(CONF1_ADDR_PORT);
		outl(CONF1_ADDR_PORT, oldval1);

		if (bootverbose)
			printf("pci_open(1a):\tmode1res=0x%08x (0x%08lx)\n", 
			    mode1res, CONF1_ENABLE_CHK);

		if (mode1res) {
			if (pci_cfgcheck(32)) 
				return (cfgmech);
		}

		outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK1);
		mode1res = inl(CONF1_ADDR_PORT);
		outl(CONF1_ADDR_PORT, oldval1);

		if (bootverbose)
			printf("pci_open(1b):\tmode1res=0x%08x (0x%08lx)\n", 
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

