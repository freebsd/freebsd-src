/*
 * Copyright (c) 1997 Ted Faber
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Ted Faber.
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
 * $FreeBSD: src/sys/pci/pcic_p.c,v 1.20 2000/02/02 16:49:21 peter Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <pci/pcic_p.h>
#include <pccard/i82365.h>
#include <vm/vm.h>
#include <vm/pmap.h>

/*
 * Set up the CL-PD6832 to look like a ISA based PCMCIA chip (a
 * PD672X).  This routine is called once per PCMCIA socket.
 */
static void
pd6832_legacy_init(device_t self)
{
	u_long bcr; 		/* to set interrupts */
	u_short io_port;	/* the io_port to map this slot on */
	static int num6832;	/* The number of 6832s initialized */
	int unit;

	num6832 = 0;
	unit = device_get_unit(self);
	/*
	 * Some BIOS leave the legacy address uninitialized.  This
	 * insures that the PD6832 puts itself where the driver will
	 * look.  We assume that multiple 6832's should be laid out
	 * sequentially.  We only initialize the first socket's legacy port,
	 * the other is a dummy.
	 */
	io_port = PCIC_INDEX_0 + num6832 * CLPD6832_NUM_REGS;
	if (unit == 0)
		pci_write_config(self, CLPD6832_LEGACY_16BIT_IOADDR,
				 io_port & ~PCI_MAP_IO, 4);

	/*
	 * I think this should be a call to pci_map_port, but that
	 * routine won't map regiaters above 0x28, and the register we
	 * need to map is 0x44.
	 */
	io_port = pci_read_config(self, CLPD6832_LEGACY_16BIT_IOADDR, 4) &
	    ~PCI_MAP_IO;

	/*
	 * Configure the first I/O window to contain CLPD6832_NUM_REGS
	 * words and deactivate the second by setting the limit lower
	 * than the base.
	 */
	pci_write_config(self, CLPD6832_IO_BASE0, io_port | 1, 4);
	pci_write_config(self, CLPD6832_IO_LIMIT0,
			 (io_port + CLPD6832_NUM_REGS) | 1, 4);

	pci_write_config(self, CLPD6832_IO_BASE1, (io_port + 0x20) | 1, 4);
	pci_write_config(self, CLPD6832_IO_LIMIT1, io_port | 1, 4);

	/*
	 * Set default operating mode (I/O port space) and allocate
	 * this socket to the current unit.
	 */
	pci_write_config(self, PCI_COMMAND_STATUS_REG,
			 CLPD6832_COMMAND_DEFAULTS, 4);
	pci_write_config(self, CLPD6832_SOCKET, unit, 4);

	/*
	 * Set up the card inserted/card removed interrupts to come
	 * through the isa IRQ.
	 */
	bcr = pci_read_config(self, CLPD6832_BRIDGE_CONTROL, 4);
	bcr |= (CLPD6832_BCR_ISA_IRQ|CLPD6832_BCR_MGMT_IRQ_ENA);
	pci_write_config(self, CLPD6832_BRIDGE_CONTROL, bcr, 4);

	/* After initializing 2 sockets, the chip is fully configured */
	if (unit == 1)
		num6832++;

	if (bootverbose)
		printf("CardBus: Legacy PC-card 16bit I/O address [0x%x]\n",
		       io_port);
}

/*
 * Return the ID string for the controller if the vendor/product id
 * matches, NULL otherwise.
 */
static int
pcic_pci_probe(device_t self)
{
	u_int32_t device_id;
	char *desc;

	device_id = pci_get_devid(self);
	desc = NULL;

	switch (device_id) {
	case PCI_DEVICE_ID_PCIC_CLPD6832:
		desc = "Cirrus Logic PD6832 PCI/CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1130:
		desc = "TI PCI-1130 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1131:
		desc = "TI PCI-1131 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1220:
		desc = "TI PCI-1220 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1221:
		desc = "TI PCI-1221 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1225:
		desc = "TI PCI-1225 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1250:
		desc = "TI PCI-1250 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1251:
		desc = "TI PCI-1251 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1251B:
		desc = "TI PCI-1251B PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1410:
		desc = "TI PCI-1410 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1420:
		desc = "TI PCI-1420 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1450:
		desc = "TI PCI-1450 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1451:
		desc = "TI PCI-1451 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_TOSHIBA_TOPIC95:
		desc = "Toshiba ToPIC95 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_TOSHIBA_TOPIC97:
		desc = "Toshiba ToPIC97 PCI-CardBus Bridge";
		break;
 	case PCI_DEVICE_ID_RICOH_RL5C465:
		desc = "Ricoh RL5C465 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_RICOH_RL5C475:
		desc = "Ricoh RL5C475 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_RICOH_RL5C476:
		desc = "Ricoh RL5C476 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_RICOH_RL5C478:
		desc = "Ricoh RL5C478 PCI-CardBus Bridge";
		break;
	/* 16bit PC-card bridges */
	case PCI_DEVICE_ID_PCIC_CLPD6729:
		desc = "Cirrus Logic PD6729/6730 PC-Card Controller";
		break;
	case PCI_DEVICE_ID_PCIC_OZ6729:
		desc = "O2micro OZ6729 PC-Card Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_OZ6730:
		desc = "O2micro OZ6730 PC-Card Bridge";
		break;

	default:
		break;
	}

	if (desc == NULL)
		return (ENXIO);
	
	device_set_desc(self, desc);
	return 0;	/* exact match */
}


/*
 * General PCI based card dispatch routine.  Right now
 * it only understands the CL-PD6832.
 */
static int
pcic_pci_attach(device_t self)
{
	u_int32_t device_id = pci_get_devid(self);

	switch (device_id) {
	case PCI_DEVICE_ID_PCIC_CLPD6832:
		pd6832_legacy_init(self);
		break;
	}

	if (bootverbose) { 		
		int i, j;
		u_char *p;
		u_long *pl;

		printf("PCI Config space:\n");
		for (j = 0; j < 0x98; j += 16) {
			printf("%02x: ", j);
			for (i = 0; i < 16; i += 4)
				printf(" %08x", pci_read_config(self, i+j, 4));
			printf("\n");
		}
		p = (u_char *)pmap_mapdev(pci_read_config(self, 0x10, 4),
					  0x1000);
		pl = (u_long *)p;
		printf("Cardbus Socket registers:\n");
		printf("00: ");
		for (i = 0; i < 4; i += 1)
			printf(" %08lx:", pl[i]);
		printf("\n10: ");
		for (i = 4; i < 8; i += 1)
			printf(" %08lx:", pl[i]);
		printf("\nExCa registers:\n");
		for (i = 0; i < 0x40; i += 16)
			printf("%02x: %16D\n", i, p + 0x800 + i, " ");
	}

	return 0;	/* no error */
}


static device_method_t pcic_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcic_pci_probe),
	DEVMETHOD(device_attach,	pcic_pci_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	{0, 0}
};

static driver_t pcic_pci_driver = {
	"pcic-pci",
	pcic_pci_methods,
	0	/* no softc */
};

static devclass_t pcic_pci_devclass;

DRIVER_MODULE(pcic_pci, pci, pcic_pci_driver, pcic_pci_devclass, 0, 0);
