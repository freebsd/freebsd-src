/*
 * Copyright (c) 2001 M. Warner Losh.  All Rights Reserved.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <pccard/i82365.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#include <pccard/pcicvar.h>

/* Get pnp IDs */
#include <isa/isavar.h>
#include <dev/pcic/i82365reg.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

static struct isa_pnp_id pcic_ids[] = {
	{PCIC_PNP_ACTIONTEC,            NULL},          /* AEI0218 */
	{PCIC_PNP_IBM3765,		NULL},		/* IBM3765 */
	{PCIC_PNP_82365,		NULL},		/* PNP0E00 */
	{PCIC_PNP_CL_PD6720,		NULL},		/* PNP0E01 */
	{PCIC_PNP_VLSI_82C146,		NULL},		/* PNP0E02 */
	{PCIC_PNP_82365_CARDBUS,	NULL},		/* PNP0E03 */
	{PCIC_PNP_SCM_SWAPBOX,		NULL},		/* SCM0469 */ 
	{0}
};

static struct {
	const char *name;
	u_int32_t flags;
} bridges[] = {
	{ "Intel i82365SL-A/B",	PCIC_AB_POWER},
	{ "IBM PCIC",		PCIC_AB_POWER},
	{ "VLSI 82C146",	PCIC_AB_POWER},
	{ "Cirrus logic 672x",	PCIC_PD_POWER},
	{ "Cirrus logic 6710", 	PCIC_PD_POWER},
	{ "Vadem 365",		PCIC_VG_POWER},
	{ "Vadem 465",		PCIC_VG_POWER},
	{ "Vadem 468",		PCIC_VG_POWER},
	{ "Vadem 469",		PCIC_VG_POWER},
	{ "Ricoh RF5C296",	PCIC_RICOH_POWER},
	{ "Ricoh RF5C396",	PCIC_RICOH_POWER},
	{ "IBM KING",		PCIC_KING_POWER},
	{ "Intel i82365SL-DF",	PCIC_DF_POWER}
};

/*
 *	Look for an Intel PCIC (or compatible).
 *	For each available slot, allocate a PC-CARD slot.
 */
static int
pcic_isa_probe(device_t dev)
{
	int slotnum, validslots = 0;
	struct pcic_slot *sp;
	struct pcic_slot *sp0;
	struct pcic_slot *sp1;
	struct pcic_slot spsave;
	unsigned char c;
	struct resource *r;
	int rid;
	struct pcic_softc *sc;
	int error;

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, pcic_ids);
	if (error == ENXIO)
		return (ENXIO);

	if (bus_get_resource_start(dev, SYS_RES_IOPORT, 0) == 0)
		bus_set_resource(dev, SYS_RES_IOPORT, 0, PCIC_INDEX0, 2);
	rid = 0;
	r = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!r) {
		if (bootverbose)
			device_printf(dev, "Cannot get I/O range\n");
		return (ENOMEM);
	}

	sc = (struct pcic_softc *) device_get_softc(dev);
	sc->unit = device_get_unit(dev);
	sp = &sc->slots[0];
	for (slotnum = 0; slotnum < PCIC_CARD_SLOTS; slotnum++, sp++) {
		/*
		 *	Initialise the PCIC slot table.
		 */
		sp->getb = pcic_getb_io;
		sp->putb = pcic_putb_io;
		sp->index = rman_get_start(r);
		sp->data = sp->index + 1;
		sp->offset = slotnum * PCIC_SLOT_SIZE;
		sp->controller = -1;
	}

	/*
	 * Prescan for the broken VLSI chips.
	 *
	 * According to the Linux PCMCIA code from David Hinds,
	 * working chipsets return 0x84 from their (correct) ID ports,
	 * while the broken ones would need to be probed at the new
	 * offset we set after we assume it's broken.
	 *
	 * Note: because of this, we may incorrectly detect a single
	 * slot vlsi chip as a i82365sl step D.  I cannot find a
	 * datasheet for the affected chip, so that's the best we can
	 * do for now.
	 */
	sp0 = &sc->slots[0];
	sp1 = &sc->slots[1];
	if (sp0->getb(sp0, PCIC_ID_REV) == PCIC_VLSI82C146 &&
	    sp1->getb(sp1, PCIC_ID_REV) != PCIC_VLSI82C146) {
		spsave = *sp1;
		sp1->index += 4;
		sp1->data += 4;
		sp1->offset = PCIC_SLOT_SIZE << 1;
		if (sp1->getb(sp1, PCIC_ID_REV) != PCIC_VLSI82C146) {
			*sp1 = spsave;
		} else {
			sp0->controller = PCIC_VLSI;
			sp1->controller = PCIC_VLSI;
		}
	}
	
	/*
	 * Look for normal chipsets here.
	 */
	sp = &sc->slots[0];
	for (slotnum = 0; slotnum < PCIC_CARD_SLOTS; slotnum++, sp++) {
		/*
		 * see if there's a PCMCIA controller here
		 * Intel PCMCIA controllers use 0x82 and 0x83
		 * IBM clone chips use 0x88 and 0x89, apparently
		 */
		c = sp->getb(sp, PCIC_ID_REV);
		sp->revision = -1;
		switch(c) {
		/*
		 *	82365 or clones.
		 */
		case PCIC_INTEL0:
		case PCIC_INTEL1:
			sp->controller = PCIC_I82365;
			sp->revision = c & 1;
			/*
			 *	Now check for VADEM chips.
			 */
			outb(sp->index, 0x0E);	/* Unlock VADEM's extra regs */
			outb(sp->index, 0x37);
			pcic_setb(sp, PCIC_VMISC, PCIC_VADEMREV);
			c = sp->getb(sp, PCIC_ID_REV);
			if (c & 0x08) {
				switch (sp->revision = c & 7) {
				case 1:
					sp->controller = PCIC_VG365;
					break;
				case 2:
					sp->controller = PCIC_VG465;
					break;
				case 3:
					sp->controller = PCIC_VG468;
					break;
				default:
					sp->controller = PCIC_VG469;
					break;
				}
				pcic_clrb(sp, PCIC_VMISC, PCIC_VADEMREV);
			}

			/*
			 * Check for RICOH RF5C[23]96 PCMCIA Controller
			 */
			c = sp->getb(sp, PCIC_RICOH_ID);
			if (c == PCIC_RID_396)
				sp->controller = PCIC_RF5C396;
			else if (c == PCIC_RID_296)
				sp->controller = PCIC_RF5C296;

			break;
		/*
		 *	Intel i82365sl-DF step or maybe a vlsi 82c146
		 * we detected the vlsi case earlier, so if the controller
		 * isn't set, we know it is a i82365sl step D.
		 */
		case PCIC_INTEL2:
			if (sp->controller == -1)
				sp->controller = PCIC_I82365SL_DF;
			break;
		case PCIC_IBM1:
		case PCIC_IBM2:
			sp->controller = PCIC_IBM;
			sp->revision = c & 1;
			break;
		case PCIC_IBM3:
			sp->controller = PCIC_IBM_KING;
			sp->revision = c & 1;
			break;
		default:
			continue;
		}
		/*
		 *	Check for Cirrus logic chips.
		 */
		sp->putb(sp, PCIC_CLCHIP, 0);
		c = sp->getb(sp, PCIC_CLCHIP);
		if ((c & PCIC_CLC_TOGGLE) == PCIC_CLC_TOGGLE) {
			c = sp->getb(sp, PCIC_CLCHIP);
			if ((c & PCIC_CLC_TOGGLE) == 0) {
				if (c & PCIC_CLC_DUAL)
					sp->controller = PCIC_PD672X;
				else
					sp->controller = PCIC_PD6710;
				sp->revision = 8 - ((c & 0x1F) >> 2);
			}
		}
		device_set_desc(dev, bridges[(int) sp->controller].name);
		sc->flags = bridges[(int) sp->controller].flags;
		/*
		 *	OK it seems we have a PCIC or lookalike.
		 *	Allocate a slot and initialise the data structures.
		 */
		validslots++;
		sp->slt = (struct slot *) 1;
		/*
		 * Modem cards send the speaker audio (dialing noises)
		 * to the host's speaker.  Cirrus Logic PCIC chips must
		 * enable this.  There is also a Low Power Dynamic Mode bit
		 * that claims to reduce power consumption by 30%, so
		 * enable it and hope for the best.
		 */
		if (sp->controller == PCIC_PD672X) {
			pcic_setb(sp, PCIC_MISC1, PCIC_MISC1_SPEAKER);
			pcic_setb(sp, PCIC_MISC2, PCIC_LPDM_EN);
		}
	}
	bus_release_resource(dev, SYS_RES_IOPORT, rid, r);
	return (validslots ? 0 : ENXIO);
}

static int
pcic_isa_attach(device_t dev)
{
	struct pcic_softc *sc;
	int rid;
	struct resource *r;

	sc = device_get_softc(dev);
	rid = 0;
	r = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!r) {
		pcic_dealloc(dev);
		return (ENXIO);
	}
	sc->iorid = rid;
	sc->iores = r;
	return (pcic_attach(dev));
}

static device_method_t pcic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pcic_isa_probe),
	DEVMETHOD(device_attach,	pcic_isa_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, pcic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, pcic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	pcic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	pcic_teardown_intr),

	/* Card interface */
	DEVMETHOD(card_set_res_flags,	pcic_set_res_flags),
	DEVMETHOD(card_get_res_flags,	pcic_get_res_flags),
	DEVMETHOD(card_set_memory_offset, pcic_set_memory_offset),
	DEVMETHOD(card_get_memory_offset, pcic_get_memory_offset),

	{ 0, 0 }
};

static driver_t pcic_driver = {
	"pcic",
	pcic_methods,
	sizeof(struct pcic_softc)
};

DRIVER_MODULE(pcic, isa, pcic_driver, pcic_devclass, 0, 0);
