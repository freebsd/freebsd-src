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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <pccard/pcic_pci.h>
#include <pccard/i82365.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#define PRVERB(x)	if (bootverbose) device_printf x

/*
 * Set up the CL-PD6832 to look like a ISA based PCMCIA chip (a
 * PD672X).  This routine is called once per PCMCIA socket.
 */
static void
pd6832_legacy_init(device_t dev)
{
	u_long bcr; 		/* to set interrupts */
	u_short io_port;	/* the io_port to map this slot on */
	static int num6832;	/* The number of 6832s initialized */
	int unit;

	num6832 = 0;
	unit = device_get_unit(dev);

	/*
	 * Some BIOS leave the legacy address uninitialized.  This
	 * insures that the PD6832 puts itself where the driver will
	 * look.  We assume that multiple 6832's should be laid out
	 * sequentially.  We only initialize the first socket's legacy port,
	 * the other is a dummy.
	 */
	io_port = PCIC_INDEX_0 + num6832 * CLPD6832_NUM_REGS;
	if (unit == 0)
		pci_write_config(dev, CLPD6832_LEGACY_16BIT_IOADDR,
		    io_port & ~CLPD6832_LEGACY_16BIT_IOENABLE, 4);

	/*
	 * I think this should be a call to pci_map_port, but that
	 * routine won't map regiaters above 0x28, and the register we
	 * need to map is 0x44.
	 */
	io_port = pci_read_config(dev, CLPD6832_LEGACY_16BIT_IOADDR, 4) &
	    ~CLPD6832_LEGACY_16BIT_IOENABLE;

	/*
	 * Configure the first I/O window to contain CLPD6832_NUM_REGS
	 * words and deactivate the second by setting the limit lower
	 * than the base.
	 */
	pci_write_config(dev, CLPD6832_IO_BASE0, io_port | 1, 4);
	pci_write_config(dev, CLPD6832_IO_LIMIT0,
	    (io_port + CLPD6832_NUM_REGS) | 1, 4);

	pci_write_config(dev, CLPD6832_IO_BASE1, (io_port + 0x20) | 1, 4);
	pci_write_config(dev, CLPD6832_IO_LIMIT1, io_port | 1, 4);

	/*
	 * Set default operating mode (I/O port space) and allocate
	 * this socket to the current unit.
	 */
	pci_write_config(dev, PCIR_COMMAND, CLPD6832_COMMAND_DEFAULTS, 4);
	pci_write_config(dev, CLPD6832_SOCKET, unit, 4);

	/*
	 * Set up the card inserted/card removed interrupts to come
	 * through the isa IRQ.
	 */
	bcr = pci_read_config(dev, CLPD6832_BRIDGE_CONTROL, 4);
	bcr |= (CLPD6832_BCR_ISA_IRQ|CLPD6832_BCR_MGMT_IRQ_ENA);
	pci_write_config(dev, CLPD6832_BRIDGE_CONTROL, bcr, 4);

	/* After initializing 2 sockets, the chip is fully configured */
	if (unit == 1)
		num6832++;

	PRVERB((dev, "CardBus: Legacy PC-card 16bit I/O address [0x%x]\n", 
	    io_port));
}

/*
 * TI1XXX PCI-CardBus Host Adapter specific function code.
 * This function is separated from pcic_pci_attach().
 * Support Device: TI1130,TI1131,TI1250,TI1220.
 * Test Device: TI1221.
 * Takeshi Shibagaki(shiba@jp.freebsd.org).
 */
static void
ti1xxx_pci_init(device_t dev)
{
	u_long	syscntl,devcntl,cardcntl;
	u_int32_t device_id = pci_get_devid(dev);
	char	buf[128];
	int 	ti113x = (device_id == PCI_DEVICE_ID_PCIC_TI1130)
	    || (device_id == PCI_DEVICE_ID_PCIC_TI1131);

	syscntl  = pci_read_config(dev, TI113X_PCI_SYSTEM_CONTROL, 4);
	devcntl  = pci_read_config(dev, TI113X_PCI_DEVICE_CONTROL, 1);
	cardcntl = pci_read_config(dev, TI113X_PCI_CARD_CONTROL,   1);

	switch(ti113x){
	case 0 :
		strcpy(buf, "TI12XX PCI Config Reg: ");
		break;
	case 1 :
		strcpy(buf, "TI113X PCI Config Reg: ");
		/* 
		 * Default card control register setting is
		 * PCI interrupt.  The method of this code
		 * switches PCI INT and ISA IRQ by bit 7 of
		 * Bridge Control Register(Offset:0x3e,0x13e).
		 * Takeshi Shibagaki(shiba@jp.freebsd.org) 
		 */
		cardcntl |= TI113X_CARDCNTL_PCI_IREQ;
		cardcntl |= TI113X_CARDCNTL_PCI_CSC;
		pci_write_config(dev, TI113X_PCI_CARD_CONTROL,  cardcntl, 1);
		cardcntl = pci_read_config(dev, TI113X_PCI_CARD_CONTROL, 1);
		if (syscntl & TI113X_SYSCNTL_CLKRUN_ENA){
			if (syscntl & TI113X_SYSCNTL_CLKRUN_SEL)
				strcat(buf, "[clkrun irq 12]");
			else
				strcat(buf, "[clkrun irq 10]");
		}
		break;
	}
	if (cardcntl & TI113X_CARDCNTL_RING_ENA)
		strcat(buf, "[ring enable]");
	if (cardcntl & TI113X_CARDCNTL_SPKR_ENA)
		strcat(buf, "[speaker enable]");
	if (syscntl & TI113X_SYSCNTL_PWRSAVINGS)
		strcat(buf, "[pwr save]");
	switch(devcntl & TI113X_DEVCNTL_INTR_MASK){
		case TI113X_DEVCNTL_INTR_ISA :
			strcat(buf, "[CSC parallel isa irq]");
			break;
		case TI113X_DEVCNTL_INTR_SERIAL :
			strcat(buf, "[CSC serial isa irq]");
			break;
		case TI113X_DEVCNTL_INTR_NONE :
			strcat(buf, "[pci only]");
			break;
		case TI12XX_DEVCNTL_INTR_ALLSERIAL :
			strcat(buf, "[FUNC pci int + CSC serial isa irq]");
			break;
	}
	device_printf(dev, "%s\n",buf);
}

static void
generic_cardbus_attach(device_t dev)
{
	u_int16_t	brgcntl;
	u_int32_t	iobase;
	int		unit;

	unit = device_get_unit(dev);

	/* Output ISA IRQ indicated in ExCA register(0x03). */
	brgcntl = pci_read_config(dev, CB_PCI_BRIDGE_CTRL, 2);
	brgcntl |= CB_BCR_INT_EXCA;
	pci_write_config(dev, CB_PCI_BRIDGE_CTRL, brgcntl, 2);

	/* 16bit Legacy Mode Base Address */
	if (unit != 0)
		return;
	
	iobase = pci_read_config(dev, CB_PCI_LEGACY16_IOADDR, 2) &
	    ~CB_PCI_LEGACY16_IOENABLE;
	if (!iobase) {
		iobase = 0x3e0 | CB_PCI_LEGACY16_IOENABLE;
		pci_write_config(dev, CB_PCI_LEGACY16_IOADDR, iobase, 2);
		iobase = pci_read_config(dev, CB_PCI_LEGACY16_IOADDR, 2)
		    & ~CB_PCI_LEGACY16_IOENABLE;
	}
	PRVERB((dev, "Legacy address set to %#x\n", iobase));
	return;
}


/*
 * Return the ID string for the controller if the vendor/product id
 * matches, NULL otherwise.
 */
static int
pcic_pci_probe(device_t dev)
{
	u_int32_t device_id;
	char *desc;

	device_id = pci_get_devid(dev);
	desc = NULL;

	switch (device_id) {
	case PCI_DEVICE_ID_PCIC_CLPD6832:
		desc = "Cirrus Logic PD6832 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1130:
		desc = "TI PCI-1130 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1131:
		desc = "TI PCI-1131 PCI-CardBus Bridge";
		break;
	case PCI_DEVICE_ID_PCIC_TI1211:
		desc = "TI PCI-1211 PCI-CardBus Bridge";
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
	case PCI_DEVICE_ID_PCIC_OZ6832:
		desc = "O2micro 6832 PCI-Cardbus Bridge";
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
	case PCI_DEVICE_ID_PCIC_TI1031:
		desc = "TI PCI-1031 PCI-PCMCIA Bridge";
		break;

	default:
		break;
	}

	if (desc == NULL)
		return (ENXIO);
	
	device_set_desc(dev, desc);
	return (0);	/* exact match */
}

static void
ricoh_init(device_t dev)
{
	u_int16_t       brgcntl;
	/*
	 * Ricoh chips have a legacy bridge enable different than most
	 * Code cribbed from NEWBUS's bridge code since I can't find a
	 * datasheet for them that has register definitions.
	 */
	brgcntl = pci_read_config(dev, CB_PCI_BRIDGE_CTRL, 2);
	brgcntl |= CB_BCR_RL_3E0_EN;
	brgcntl &= ~CB_BCR_RL_3E2_EN;
	pci_write_config(dev, CLPD6832_BRIDGE_CONTROL, brgcntl, 4);
}

/*
 * General PCI based card dispatch routine.  Right now
 * it only understands the Ricoh, CL-PD6832 and TI parts.  It does
 * try to do generic things with other parts.
 */
static int
pcic_pci_attach(device_t dev)
{
	u_int32_t device_id = pci_get_devid(dev);
	u_long command;

	/* Init. CardBus/PC-card controllers as 16-bit PC-card controllers */

	/* Place any per "slot" initialization here */

	/*
	 * In sys/pci/pcireg.h, PCIR_COMMAND must be separated
	 * PCI_COMMAND_REG(0x04) and PCI_STATUS_REG(0x06).
	 * Takeshi Shibagaki(shiba@jp.freebsd.org).
	 */
        command = pci_read_config(dev, PCIR_COMMAND, 4);
        command |= PCIM_CMD_PORTEN | PCIM_CMD_MEMEN;
        pci_write_config(dev, PCIR_COMMAND, command, 4);

	switch (device_id) {
	case PCI_DEVICE_ID_RICOH_RL5C465:
	case PCI_DEVICE_ID_RICOH_RL5C466:
		ricoh_init(dev);
		generic_cardbus_attach(dev);
		break;
	case PCI_DEVICE_ID_PCIC_TI1130:
	case PCI_DEVICE_ID_PCIC_TI1131:
	case PCI_DEVICE_ID_PCIC_TI1211:
	case PCI_DEVICE_ID_PCIC_TI1220:
	case PCI_DEVICE_ID_PCIC_TI1221:
	case PCI_DEVICE_ID_PCIC_TI1225:
	case PCI_DEVICE_ID_PCIC_TI1250:
	case PCI_DEVICE_ID_PCIC_TI1251:
	case PCI_DEVICE_ID_PCIC_TI1251B:
	case PCI_DEVICE_ID_PCIC_TI1410:
	case PCI_DEVICE_ID_PCIC_TI1420:
	case PCI_DEVICE_ID_PCIC_TI1450:
	case PCI_DEVICE_ID_PCIC_TI1451:
                ti1xxx_pci_init(dev);
		/* FALLTHROUGH */
	default:
                generic_cardbus_attach(dev);
                break;
	case PCI_DEVICE_ID_PCIC_CLPD6832:
	case PCI_DEVICE_ID_PCIC_TI1031:		/* 1031 is like 6832 */
		pd6832_legacy_init(dev);
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
				printf(" %08x", pci_read_config(dev, i+j, 4));
			printf("\n");
		}
		p = (u_char *)pmap_mapdev(pci_read_config(dev, 0x10, 4),
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

	return (0);
}

static int
pcic_pci_detach(device_t dev)
{
	return (0);
}

static device_method_t pcic_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcic_pci_probe),
	DEVMETHOD(device_attach,	pcic_pci_attach),
	DEVMETHOD(device_detach,	pcic_pci_detach),
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
