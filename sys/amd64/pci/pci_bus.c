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
 * $Id: pcibus.c,v 1.39 1997/05/26 21:52:41 se Exp $
 *
 */

#include <sys/types.h>
#include <sys/systm.h>

#include <pci/pcivar.h>
#include <i386/isa/pcibus.h>

#ifdef PCI_COMPAT
/* XXX this is a terrible hack, which keeps the Tekram AMD SCSI driver happy */
#define cfgmech pci_mechanism
int cfgmech;
#else
static int cfgmech;
#endif /* PCI_COMPAT */
static int devmax;

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

/* read configuration space register */

int
pci_cfgread(pcicfgregs *cfg, int reg, int bytes)
{
	int data = -1;
	int port;

	port = pci_cfgenable(cfg->bus, cfg->slot, cfg->func, reg, bytes);

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

/* write configuration space register */

void
pci_cfgwrite(pcicfgregs *cfg, int reg, int data, int bytes)
{
	int port;

	port = pci_cfgenable(cfg->bus, cfg->slot, cfg->func, reg, bytes);
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
		if (class == 0 || (class & 0xf8f0ff) != 0)
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

int
pci_cfgopen(void)
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
