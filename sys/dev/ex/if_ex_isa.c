/*-
 * Copyright (c) 2000 Matthew N. Dodd
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
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h> 


#include <isa/isavar.h>
#include <isa/pnpvar.h>

#include <dev/ex/if_exreg.h>
#include <dev/ex/if_exvar.h>

/* Bus Front End Functions */
static void	ex_isa_identify	(driver_t *, device_t);
static int	ex_isa_probe	(device_t);
static int	ex_isa_attach	(device_t);

#if 0
static	void	ex_pnp_wakeup	(void *);

SYSINIT(ex_pnpwakeup, SI_SUB_CPU, SI_ORDER_ANY, ex_pnp_wakeup, NULL);
#endif

static device_method_t ex_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	ex_isa_identify),
	DEVMETHOD(device_probe,		ex_isa_probe),
	DEVMETHOD(device_attach,	ex_isa_attach),

	{ 0, 0 }
};

static driver_t ex_driver = {
	"ex",
	ex_methods,
	sizeof(struct ex_softc),
};

devclass_t ex_devclass;

DRIVER_MODULE(ex, isa, ex_driver, ex_devclass, 0, 0);

static struct isa_pnp_id ex_ids[] = {
	{ 0x3110d425,	NULL },	/* INT1031 */
	{ 0x3010d425,	NULL },	/* INT1030 */
	{ 0,		NULL },
};

#if 0
#define EX_PNP_WAKE		0x279

static u_int8_t ex_pnp_wake_seq[] =
			{ 0x6A, 0xB5, 0xDA, 0xED, 0xF6, 0xFB, 0x7D, 0xBE,
			  0xDF, 0x6F, 0x37, 0x1B, 0x0D, 0x86, 0xC3, 0x61,
			  0xB0, 0x58, 0x2C, 0x16, 0x8B, 0x45, 0xA2, 0xD1,
			  0xE8, 0x74, 0x3A, 0x9D, 0xCE, 0xE7, 0x73, 0x43 };

static void
ex_pnp_wakeup (void * dummy)
{
	int	tmp;

	if (bootverbose)
		printf("ex_pnp_wakeup()\n");

	outb(EX_PNP_WAKE, 0);
	outb(EX_PNP_WAKE, 0);
	for (tmp = 0; tmp < 32; tmp++) {
		outb(EX_PNP_WAKE, ex_pnp_wake_seq[tmp]);
	}
}
#endif

/*
 * Non-destructive identify.
 */
static void
ex_isa_identify (driver_t *driver, device_t parent)
{
	device_t	child;
	u_int32_t	ioport;
	u_char 		enaddr[6];
	u_int		irq;
	int		tmp;
	const char *	desc;

	if (bootverbose)
		printf("ex_isa_identify()\n");

	for (ioport = 0x200; ioport < 0x3a0; ioport += 0x10) {

		/* No board found at address */
		if (!look_for_card(ioport)) {
			continue;
		}

		if (bootverbose)
			printf("ex: Found card at 0x%03x!\n", ioport);

		/* Board in PnP mode */
		if (eeprom_read(ioport, EE_W0) & EE_W0_PNP) {
			/* Reset the card. */
			outb(ioport + CMD_REG, Reset_CMD);
			DELAY(500);
			if (bootverbose)
				printf("ex: card at 0x%03x in PnP mode!\n", ioport);
			continue;
		}

		bzero(enaddr, sizeof(enaddr));

		/* Reset the card. */
		outb(ioport + CMD_REG, Reset_CMD);
		DELAY(400);

		ex_get_address(ioport, enaddr);
		tmp = eeprom_read(ioport, EE_W1) & EE_W1_INT_SEL;

		/* work out which set of irq <-> internal tables to use */
		if (ex_card_type(enaddr) == CARD_TYPE_EX_10_PLUS) {
			irq  = plus_ee2irqmap[tmp];
			desc = "Intel Pro/10+";
		} else {
			irq = ee2irqmap[tmp];
			desc = "Intel Pro/10";
		}

		child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "ex", -1);
		device_set_desc_copy(child, desc);
		device_set_driver(child, driver);
		bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
		bus_set_resource(child, SYS_RES_IOPORT, 0, ioport, EX_IOSIZE);

		if (bootverbose)
			printf("ex: Adding board at 0x%03x, irq %d\n", ioport, irq);
	}

	return;
}

static int
ex_isa_probe(device_t dev)
{
	u_int		iobase;
	u_int		irq;
	char *		irq2ee;
	u_char *	ee2irq;
	u_char 		enaddr[6];
	int		tmp;
	int		error;

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, ex_ids);

	/* If the card had a PnP ID that didn't match any we know about */
	if (error == ENXIO) {
		return(error);
	}

	/* If we had some other problem. */
	if (!(error == 0 || error == ENOENT)) {
		return(error);
	}

	iobase = bus_get_resource_start(dev, SYS_RES_IOPORT, 0);
	if (!iobase) {
		printf("ex: no iobase?\n");
		return(ENXIO);
	}

	if (!look_for_card(iobase)) {
		printf("ex: no card found at 0x%03x\n", iobase);
		return(ENXIO);
	}

	if (bootverbose)
		printf("ex: ex_isa_probe() found card at 0x%03x\n", iobase);

	/*
	 * Reset the card.
	 */
	outb(iobase + CMD_REG, Reset_CMD);
	DELAY(800);

	ex_get_address(iobase, enaddr);

	/* work out which set of irq <-> internal tables to use */
	if (ex_card_type(enaddr) == CARD_TYPE_EX_10_PLUS) {
		irq2ee = plus_irq2eemap;
		ee2irq = plus_ee2irqmap;
	} else {
		irq2ee = irq2eemap;
		ee2irq = ee2irqmap;
	}

	tmp = eeprom_read(iobase, EE_W1) & EE_W1_INT_SEL;
	irq = bus_get_resource_start(dev, SYS_RES_IRQ, 0);

	if (irq > 0) {
		/* This will happen if board is in PnP mode. */
		if (ee2irq[tmp] != irq) {
			printf("ex: WARNING: board's EEPROM is configured"
				" for IRQ %d, using %d\n",
				ee2irq[tmp], irq);
		}
	} else {
		irq = ee2irq[tmp];
		bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	}

	if (irq == 0) {
		printf("ex: invalid IRQ.\n");
		return(ENXIO);
	}

	return(0);
}

static int
ex_isa_attach(device_t dev)
{
	struct ex_softc *	sc = device_get_softc(dev);
	int			error = 0;
	u_int16_t		temp;

	sc->dev = dev;
	sc->ioport_rid = 0;
	sc->irq_rid = 0;

	if ((error = ex_alloc_resources(dev)) != 0) {
		device_printf(dev, "ex_alloc_resources() failed!\n");
		goto bad;
	}

	/*
	 * Fill in several fields of the softc structure:
	 *	- I/O base address.
	 *	- Hardware Ethernet address.
	 *	- IRQ number (if not supplied in config file, read it from EEPROM).
	 *	- Connector type.
	 */
	sc->iobase = rman_get_start(sc->ioport);
	sc->irq_no = rman_get_start(sc->irq);

	ex_get_address(sc->iobase, sc->arpcom.ac_enaddr);

	temp = eeprom_read(sc->iobase, EE_W0);
	device_printf(sc->dev, "%s config, %s bus, ",
		(temp & EE_W0_PNP) ? "PnP" : "Manual",
		(temp & EE_W0_BUS16) ? "16-bit" : "8-bit");

	temp = eeprom_read(sc->iobase, EE_W6);
	printf("board id 0x%03x, stepping 0x%01x\n",
		(temp & EE_W6_BOARD_MASK) >> EE_W6_BOARD_SHIFT,
		temp & EE_W6_STEP_MASK);

	if ((error = ex_attach(dev)) != 0) {
		device_printf(dev, "ex_attach() failed!\n");
		goto bad;
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
				ex_intr, (void *)sc, &sc->ih);
	if (error) {
		device_printf(dev, "bus_setup_intr() failed!\n");
		goto bad;
	}

	return(0);
bad:
	ex_release_resources(dev);
	return (error);
}
