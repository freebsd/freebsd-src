/*
 * Copyright (c) 2001 M. Warner Losh.  All Rights Reserved.
 * Copyright (c) 1997 Ted Faber All rights reserved.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <pccard/pcic_pci.h>
#include <pccard/i82365.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#include <pccard/pcicvar.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

#define PRVERB(x)	if (bootverbose) device_printf x

static int pcic_pci_get_memory(device_t dev);

struct pcic_pci_table
{
	u_int32_t	devid;
	const char	*descr;
	int		type;
	u_int32_t	flags;
	int		revision;
} pcic_pci_devs[] = {
	{ PCI_DEVICE_ID_PCIC_CLPD6729,
	  "Cirrus Logic PD6729/6730 PC-Card Controller",
	  PCIC_PD672X, PCIC_PD_POWER },
	{ PCI_DEVICE_ID_PCIC_CLPD6832,
	  "Cirrus Logic PD6832 PCI-CardBus Bridge",
	  PCIC_PD672X, PCIC_PD_POWER },
	{ PCI_DEVICE_ID_PCIC_CLPD6833,
	  "Cirrus Logic PD6833 PCI-CardBus Bridge",
	  PCIC_PD672X, PCIC_PD_POWER },
	{ PCI_DEVICE_ID_PCIC_OZ6729,
	  "O2micro OZ6729 PC-Card Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ PCI_DEVICE_ID_PCIC_OZ6730,
	  "O2micro OZ6730 PC-Card Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ PCI_DEVICE_ID_PCIC_OZ6832,
	  "O2micro 6832/6833 PCI-Cardbus Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ PCI_DEVICE_ID_PCIC_OZ6860,
	  "O2micro 6860/6836 PCI-Cardbus Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ PCI_DEVICE_ID_PCIC_OZ6872,
	  "O2micro 6812/6872 PCI-Cardbus Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ PCI_DEVICE_ID_RICOH_RL5C465,
	  "Ricoh RL5C465 PCI-CardBus Bridge",
	  PCIC_RF5C296, PCIC_RICOH_POWER },
	{ PCI_DEVICE_ID_RICOH_RL5C475,
	  "Ricoh RL5C475 PCI-CardBus Bridge",
	  PCIC_RF5C296, PCIC_RICOH_POWER },
	{ PCI_DEVICE_ID_RICOH_RL5C476,
	  "Ricoh RL5C476 PCI-CardBus Bridge",
	  PCIC_RF5C296, PCIC_RICOH_POWER },
	{ PCI_DEVICE_ID_RICOH_RL5C477,
	  "Ricoh RL5C477 PCI-CardBus Bridge",
	  PCIC_RF5C296, PCIC_RICOH_POWER },
	{ PCI_DEVICE_ID_RICOH_RL5C478,
	  "Ricoh RL5C478 PCI-CardBus Bridge",
	  PCIC_RF5C296, PCIC_RICOH_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1031,
	  "TI PCI-1031 PCI-PCMCIA Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1130,
	  "TI PCI-1130 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1131,
	  "TI PCI-1131 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1211,
	  "TI PCI-1211 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1220,
	  "TI PCI-1220 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1221,
	  "TI PCI-1221 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1225,
	  "TI PCI-1225 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1250,
	  "TI PCI-1250 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1251,
	  "TI PCI-1251 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1251B,
	  "TI PCI-1251B PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1410,
	  "TI PCI-1410 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1420,
	  "TI PCI-1420 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1450,
	  "TI PCI-1450 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI1451,
	  "TI PCI-1451 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_PCIC_TI4451,
	  "TI PCI-4451 PCI-CardBus Bridge",
	  PCIC_I82365SL_DF, PCIC_DF_POWER },
	{ PCI_DEVICE_ID_TOSHIBA_TOPIC95,
	  "Toshiba ToPIC95 PCI-CardBus Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ PCI_DEVICE_ID_TOSHIBA_TOPIC97,
	  "Toshiba ToPIC97 PCI-CardBus Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ PCI_DEVICE_ID_TOSHIBA_TOPIC100,
	  "Toshiba ToPIC100 PCI-CardBus Bridge",
	  PCIC_I82365, PCIC_AB_POWER },
	{ 0, NULL, 0, 0 }
};

/*
 * Read a register from the PCIC.
 */
static unsigned char
pcic_pci_getb2(struct pcic_slot *sp, int reg)
{
	return (bus_space_read_1(sp->bst, sp->bsh, sp->offset + reg));
}

/*
 * Write a register on the PCIC
 */
static void
pcic_pci_putb2(struct pcic_slot *sp, int reg, unsigned char val)
{
	bus_space_write_1(sp->bst, sp->bsh, sp->offset + reg, val);
}

/*
 * lookup inside the table
 */
static struct pcic_pci_table *
pcic_pci_lookup(u_int32_t devid, struct pcic_pci_table *tbl)
{
	while (tbl->devid) {
		if (tbl->devid == devid)
			return (tbl);
		tbl++;
	}
	return (NULL);
}

/*
 * Set up the CL-PD6832
 */
static void
pcic_pci_pd6832_init(device_t dev)
{
	u_long bcr; 		/* to set interrupts */

        /*
         * CLPD683X management interrupt enable bit is bit 11 in bridge
         * control register(offset 0x3d).
         * When this bit is turned on, card status change interrupt sets
         * on ISA IRQ interrupt.
	 */
	bcr = pci_read_config(dev, CB_PCI_BRIDGE_CTRL, 2);
	bcr |= CLPD6832_BCR_MGMT_IRQ_ENA;
	pci_write_config(dev, CB_PCI_BRIDGE_CTRL, bcr, 2);
}

/*
 * TI PCI-CardBus Host Adapter specific function code.
 * This function is separated from pcic_pci_attach().
 * Takeshi Shibagaki(shiba@jp.freebsd.org).
 */
static void
pcic_pci_ti_init(device_t dev)
{
	u_long	syscntl,devcntl,cardcntl;
	u_int32_t device_id = pci_get_devid(dev);
	char	buf[128];
	struct pcic_softc *sc = device_get_softc(dev);
	int 	ti113x = (device_id == PCI_DEVICE_ID_PCIC_TI1031) ||
	    (device_id == PCI_DEVICE_ID_PCIC_TI1130) ||
	    (device_id == PCI_DEVICE_ID_PCIC_TI1131);

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
		if (sc->func_route == pci_parallel) {
			cardcntl |= TI113X_CARDCNTL_PCI_IRQ_ENA;
			cardcntl &= ~TI113X_CARDCNTL_PCI_IREQ;
		} else {
			cardcntl |= TI113X_CARDCNTL_PCI_IREQ;
		}
		if (sc->csc_route == pci_parallel)
			cardcntl |= TI113X_CARDCNTL_PCI_CSC;
		else
			cardcntl &= ~TI113X_CARDCNTL_PCI_CSC;
		pci_write_config(dev, TI113X_PCI_CARD_CONTROL,  cardcntl, 1);
		cardcntl = pci_read_config(dev, TI113X_PCI_CARD_CONTROL, 1);
		if (syscntl & TI113X_SYSCNTL_CLKRUN_ENA) {
			if (syscntl & TI113X_SYSCNTL_CLKRUN_SEL)
				strcat(buf, "[clkrun irq 12]");
			else
				strcat(buf, "[clkrun irq 10]");
		}
		break;
	}
	if (sc->func_route == pci_parallel) {
		devcntl &= ~TI113X_DEVCNTL_INTR_MASK;
		pci_write_config(dev, TI113X_PCI_DEVICE_CONTROL, devcntl, 1);
		devcntl = pci_read_config(dev, TI113X_PCI_DEVICE_CONTROL, 1);
		syscntl |= TI113X_SYSCNTL_INTRTIE;
		syscntl &= ~TI113X_SYSCNTL_SMIENB;
		pci_write_config(dev, TI113X_PCI_SYSTEM_CONTROL, syscntl, 1);
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
pcic_pci_cardbus_init(device_t dev)
{
	u_int16_t	brgcntl;
	int		unit;
	struct pcic_softc *sc = device_get_softc(dev);

	unit = device_get_unit(dev);

	if (sc->func_route == pci_parallel) {
		/* Use INTA for routing interrupts via pci bus */
		brgcntl = pci_read_config(dev, CB_PCI_BRIDGE_CTRL, 2);
		brgcntl &= ~CB_BCR_INT_EXCA;
		brgcntl |= CB_BCR_WRITE_POST_EN | CB_BCR_MASTER_ABORT;
		pci_write_config(dev, CB_PCI_BRIDGE_CTRL, brgcntl, 2);
	} else {
		/* Output ISA IRQ indicated in ExCA register(0x03). */
		brgcntl = pci_read_config(dev, CB_PCI_BRIDGE_CTRL, 2);
		brgcntl |= CB_BCR_INT_EXCA;
		pci_write_config(dev, CB_PCI_BRIDGE_CTRL, brgcntl, 2);
	}

	/* Turn off legacy address */
	pci_write_config(dev, CB_PCI_LEGACY16_IOADDR, 0, 2);

	/* 
	 * Write zeros into the remaining BARs.  This seems to turn off
	 * the pci configuration of these things and make the cardbus
	 * bridge use the values for memory programmed into the pcic
	 * registers.
	 */
	pci_write_config(dev, CB_PCI_MEMBASE0, 0, 4);
	pci_write_config(dev, CB_PCI_MEMLIMIT0, 0, 4);
	pci_write_config(dev, CB_PCI_MEMBASE1, 0, 4);
	pci_write_config(dev, CB_PCI_MEMLIMIT1, 0, 4);
	pci_write_config(dev, CB_PCI_IOBASE0, 0, 4);
	pci_write_config(dev, CB_PCI_IOLIMIT0, 0, 4);
	pci_write_config(dev, CB_PCI_IOBASE1, 0, 4);
	pci_write_config(dev, CB_PCI_IOLIMIT1, 0, 4);

	return;
}

static void
pcic_pci_ricoh_init(device_t dev, int old)
{
	u_int16_t       brgcntl;

	/*
	 * Ricoh chips have a legacy bridge enable different than most
	 * Code cribbed from NEWBUS's bridge code since I can't find a
	 * datasheet for them that has register definitions.
	 */
	if (old) {
		brgcntl = pci_read_config(dev, CB_PCI_BRIDGE_CTRL, 2);
		brgcntl &= ~(CB_BCR_RL_3E2_EN | CB_BCR_RL_3E0_EN);
		pci_write_config(dev, CB_PCI_BRIDGE_CTRL, brgcntl, 2);
	}
}

static void
pcic_cd_event(void *arg) 
{
	struct pcic_softc *sc = (struct pcic_softc *) arg;
	struct pcic_slot *sp = &sc->slots[0];
	u_int32_t stat;
	
	stat = bus_space_read_4(sp->bst, sp->bsh, CB_SOCKET_STATE);
	device_printf(sc->dev, "debounced state is 0x%x\n", stat);
	if ((stat & CB_SS_CD) == 0) {
		if ((stat & CB_SS_16BIT) == 0)
			device_printf(sp->sc->dev,
			    "Unsupported card type inserted\n");
		else
			pccard_event(sp->slt, card_inserted);
	} else {
		pccard_event(sp->slt, card_removed);
	}
	sc->cd_pending = 0;
}

static void
pcic_pci_intr(void *arg)
{
	struct pcic_softc *sc = (struct pcic_softc *) arg;
	struct pcic_slot *sp = &sc->slots[0];
	u_int32_t event;
	u_int32_t stat;

	event = bus_space_read_4(sp->bst, sp->bsh, CB_SOCKET_EVENT);
	if (event != 0) {
		device_printf(sc->dev, "Event mask 0x%x\n", event);
		if ((event & CB_SE_CD) != 0 && !sc->cd_pending) {
			sc->cd_pending = 1;
			timeout(pcic_cd_event, arg, hz/2);
		}
		/* Ack the interrupt, all of them to be safe */
		bus_space_write_4(sp->bst, sp->bsh, 0, 0xffffffff);
	}
	stat = bus_space_read_4(sp->bst, sp->bsh, CB_SOCKET_STATE);

	/* Now call children interrupts if any */
	if (sp->intr && (stat & CB_SS_CD) == 0)
		sp->intr(sp->argp);
}

/*
 * Return the ID string for the controller if the vendor/product id
 * matches, NULL otherwise.
 */
static int
pcic_pci_probe(device_t dev)
{
	u_int8_t	subclass;
	u_int8_t	progif;
	const char	*desc;
	u_int32_t	device_id;
	struct pcic_pci_table *itm;
	struct resource	*res;
	int		rid;

	device_id = pci_get_devid(dev);
	desc = NULL;
	itm = pcic_pci_lookup(device_id, &pcic_pci_devs[0]);
	if (itm != NULL)
		desc = itm->descr;
	if (desc == NULL && pci_get_class(dev) == PCIC_BRIDGE) {
		subclass = pci_get_subclass(dev);
		progif = pci_get_progif(dev);
		if (subclass == PCIS_BRIDGE_PCMCIA && progif == 0)
			desc = "Generic PCI-PCMCIA Bridge";
		if (subclass == PCIS_BRIDGE_CARDBUS && progif == 0)
			desc = "YENTA PCI-CARDBUS Bridge";
	}
	if (desc == NULL)
		return (ENXIO);
	device_set_desc(dev, desc);

	/*
	 * Take us out of power down mode.
	 */
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		/* Reset the power state. */
		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	}

	/*
	 * Allocated/deallocate interrupt.  This forces the PCI BIOS or
	 * other MD method to route the interrupts to this card.
	 * This so we get the interrupt number in the probe message.
	 */
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_ACTIVE);
	if (res)
		bus_release_resource(dev, SYS_RES_IRQ, rid, res);
	
	return (0);
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
	struct pcic_slot *sp;
	struct pcic_softc *sc;
	u_int32_t sockbase;
	struct pcic_pci_table *itm;
	int rid;
	struct resource *r;
	int error;
	static int num6729;

	/*
	 * In sys/pci/pcireg.h, PCIR_COMMAND must be separated
	 * PCI_COMMAND_REG(0x04) and PCI_STATUS_REG(0x06).
	 * Takeshi Shibagaki(shiba@jp.freebsd.org).
	 */
        command = pci_read_config(dev, PCIR_COMMAND, 4);
        command |= PCIM_CMD_PORTEN | PCIM_CMD_MEMEN;
        pci_write_config(dev, PCIR_COMMAND, command, 4);

	sc = (struct pcic_softc *) device_get_softc(dev);
	sp = &sc->slots[0];
	sp->sc = sc;
	sockbase = pci_read_config(dev, 0x10, 4);
	if (sockbase & 0x1) {
		device_printf(dev, "I/O mapped device, might not work.\n");
		sc->iorid = CB_PCI_SOCKET_BASE;
		sc->iores = bus_alloc_resource(dev, SYS_RES_IOPORT,
		    &sc->iorid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
		if (sc->iores == NULL)
			return (ENOMEM);
		sp->getb = pcic_getb_io;
		sp->putb = pcic_putb_io;
		sc->bst = sp->bst = rman_get_bustag(sc->iores);
		sc->bsh = sp->bsh = rman_get_bushandle(sc->iores);
		sp->offset = (num6729 % 2) * PCIC_SLOT_SIZE;
		sp->controller = PCIC_PD672X;
		sp->revision = 0;
		sc->flags = PCIC_PD_POWER;
		num6729++;
	} else {
		device_printf(dev, "Memory mapped device, will work.\n");
		sc->memrid = CB_PCI_SOCKET_BASE;
		sc->memres = bus_alloc_resource(dev, SYS_RES_MEMORY,
		    &sc->memrid, 0, ~0, 1, RF_ACTIVE);
		if (sc->memres == NULL && pcic_pci_get_memory(dev) != 0)
			return (ENOMEM);
		sp->getb = pcic_pci_getb2;
		sp->putb = pcic_pci_putb2;
		sp->offset = CB_EXCA_OFFSET;
		sc->bst = sp->bst = rman_get_bustag(sc->memres);
		sc->bst = sp->bsh = rman_get_bushandle(sc->memres);
		itm = pcic_pci_lookup(device_id, &pcic_pci_devs[0]);
		if (itm != NULL) {
			sp->controller = itm->type;
			sp->revision = itm->revision;
			sc->flags  = itm->flags;
		} else {
			/* By default, assume we're a D step compatible */
			sp->controller = PCIC_I82365SL_DF;
			sp->revision = 0;
			sc->flags = PCIC_DF_POWER;
		}
		sp->slt = (struct slot *) 1;
	}
	sc->dev = dev;
	sc->csc_route = pci_parallel;
	sc->func_route = pci_parallel;

	switch (device_id) {
	case PCI_DEVICE_ID_RICOH_RL5C465:
	case PCI_DEVICE_ID_RICOH_RL5C466:
		pcic_pci_ricoh_init(dev, 1);
                pcic_pci_cardbus_init(dev);
		break;
	case PCI_DEVICE_ID_RICOH_RL5C475:
	case PCI_DEVICE_ID_RICOH_RL5C476:
	case PCI_DEVICE_ID_RICOH_RL5C477:
	case PCI_DEVICE_ID_RICOH_RL5C478:
		pcic_pci_ricoh_init(dev, 0);
                pcic_pci_cardbus_init(dev);
		break;
	case PCI_DEVICE_ID_PCIC_TI1031:
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
	case PCI_DEVICE_ID_PCIC_TI4451:
                pcic_pci_ti_init(dev);
                pcic_pci_cardbus_init(dev);
		break;
	case PCI_DEVICE_ID_PCIC_CLPD6832:
		pcic_pci_pd6832_init(dev);
		break;
	default:
                pcic_pci_cardbus_init(dev);
                break;
	}

	rid = 0;
	r = NULL;
	r = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, 
	    RF_ACTIVE | RF_SHAREABLE);
	if (r == NULL) {
		device_printf(dev, "Failed to allocate managment irq\n");
		return (EIO);
	}
	sc->irqrid = rid;
	sc->irqres = r;
	sc->irq = rman_get_start(r);
	error = bus_setup_intr(dev, r, INTR_TYPE_AV, pcic_pci_intr,
	    (void *) sc, &sc->ih);
	if (error) {
		pcic_dealloc(dev);
		return (error);
	}

	return (pcic_attach(dev));
}

static int
pcic_pci_detach(device_t dev)
{
	return (0);
}

/*
 * The PCI bus should do this for us.  However, it doesn't quite yet, so
 * we cope by doing it ourselves.  If it ever does, this code can go quietly
 * into that good night.
 */
static int
pcic_pci_get_memory(device_t dev)
{
	struct pcic_softc *sc;
	u_int32_t sockbase;

	sc = (struct pcic_softc *) device_get_softc(dev);
	sockbase = pci_read_config(dev, sc->memrid, 4);
	if (sockbase >= 0x100000 && sockbase < 0xfffffff0) {
		device_printf(dev, "Could not map register memory\n");
		return (ENOMEM);
	}
	pci_write_config(dev, sc->memrid, 0xffffffff, 4);
	sockbase = pci_read_config(dev, sc->memrid, 4);
	sockbase = (sockbase & 0xfffffff0) & -(sockbase & 0xfffffff0);
#define CARDBUS_SYS_RES_MEMORY_START    0x44000000
#define CARDBUS_SYS_RES_MEMORY_END      0xFFFFFFFF
	sc->memres = bus_generic_alloc_resource(device_get_parent(dev),
	    dev, SYS_RES_MEMORY, &sc->memrid,
	    CARDBUS_SYS_RES_MEMORY_START, CARDBUS_SYS_RES_MEMORY_END,
	    sockbase, RF_ACTIVE | rman_make_alignment_flags(sockbase));
	if (sc->memres == NULL) {
		device_printf(dev, "Could not grab register memory\n");
		return (ENOMEM);
	}
	sockbase = rman_get_start(sc->memres);
	pci_write_config(dev, sc->memrid, sockbase, 4);
	device_printf(dev, "PCI Memory allocated: 0x%08x\n", sockbase);
	return (0);
}

static int
pcic_pci_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct pcic_softc *sc = (struct pcic_softc *) device_get_softc(dev);
	struct pcic_slot *sp = &sc->slots[0];
	
	if (sp->intr)
		panic("Interrupt already established");
	sp->intr = intr;
	sp->argp = arg;
	*cookiep = sc;
	return (0);
}

static int
pcic_pci_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct pcic_softc *sc = (struct pcic_softc *) device_get_softc(dev);
	struct pcic_slot *sp = &sc->slots[0];

	sp->intr = NULL;
	sp->argp = NULL;
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

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	pcic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, pcic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pcic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	pcic_pci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	pcic_pci_teardown_intr),

	/* Card interface */
	DEVMETHOD(card_set_res_flags,	pcic_set_res_flags),
	DEVMETHOD(card_get_res_flags,	pcic_get_res_flags),
	DEVMETHOD(card_set_memory_offset, pcic_set_memory_offset),
	DEVMETHOD(card_get_memory_offset, pcic_get_memory_offset),

	{0, 0}
};

static driver_t pcic_pci_driver = {
	"pcic",
	pcic_pci_methods,
	sizeof(struct pcic_softc)
};

DRIVER_MODULE(pcic, pci, pcic_pci_driver, pcic_devclass, 0, 0);
