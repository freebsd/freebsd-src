/*
 * Product specific probe and attach routines for:
 * 	3COM 3C579 and 3C509(in eisa config mode) ethernet controllers
 *
 * Copyright (c) 1996 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $FreeBSD$
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

#include <machine/clock.h>

#include <dev/eisa/eisaconf.h>

#include <dev/ep/if_epreg.h>
#include <dev/ep/if_epvar.h>

#define EISA_DEVICE_ID_3COM_3C509_TP	0x506d5090
#define EISA_DEVICE_ID_3COM_3C509_BNC	0x506d5091
#define EISA_DEVICE_ID_3COM_3C579_TP	0x506d5092
#define EISA_DEVICE_ID_3COM_3C579_BNC	0x506d5093
#define EISA_DEVICE_ID_3COM_3C509_COMBO	0x506d5094
#define EISA_DEVICE_ID_3COM_3C509_TPO	0x506d5095

#define	EP_EISA_SLOT_OFFSET		0x0c80
#define	EP_EISA_IOSIZE			0x000a

#define EISA_IOCONF			0x0008
#define		IRQ_CHANNEL		0xf000
#define			INT_3		0x3000
#define			INT_5		0x5000
#define			INT_7		0x7000
#define			INT_9		0x9000
#define			INT_10		0xa000
#define			INT_11		0xb000
#define			INT_12		0xc000
#define			INT_15		0xf000
#define EISA_BPROM_MEDIA_CONF		0x0006
#define		TRANS_TYPE		0xc000
#define			TRANS_TP	0x0000
#define			TRANS_AUI	0x4000
#define			TRANS_BNC	0xc000

static const char *ep_match __P((eisa_id_t type));

static const char*
ep_match(eisa_id_t type)
{
	switch(type) {
		case EISA_DEVICE_ID_3COM_3C509_TP:
			return "3Com 3C509-TP Network Adapter";
			break;
		case EISA_DEVICE_ID_3COM_3C509_BNC:
			return "3Com 3C509-BNC Network Adapter";
			break;
		case EISA_DEVICE_ID_3COM_3C579_TP:
			return "3Com 3C579-TP EISA Network Adapter";
			break;
		case EISA_DEVICE_ID_3COM_3C579_BNC:
			return "3Com 3C579-BNC EISA Network Adapter";
			break;
		case EISA_DEVICE_ID_3COM_3C509_COMBO:
			return "3Com 3C509-Combo Network Adapter";
			break;
		case EISA_DEVICE_ID_3COM_3C509_TPO:
			return "3Com 3C509-TPO Network Adapter";
			break;
		default:
			break;
	}
	return (NULL);
}

static int
ep_eisa_probe(device_t dev)
{
	const char *desc;
	u_long iobase;
	u_short conf;
	u_long port;
	int irq;
	int int_trig;

	desc = ep_match(eisa_get_id(dev));
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	port = (eisa_get_slot(dev) * EISA_SLOT_SIZE);
	iobase = port + EP_EISA_SLOT_OFFSET;

	/* We must be in EISA configuration mode */
	if ((inw(iobase + EP_W0_ADDRESS_CFG) & 0x1f) != 0x1f)
	    return ENXIO;

	eisa_add_iospace(dev, iobase, EP_EISA_IOSIZE, RESVADDR_NONE);
	eisa_add_iospace(dev, port, EP_IOSIZE, RESVADDR_NONE);

	conf = inw(iobase + EISA_IOCONF);
	/* Determine our IRQ */
	switch (conf & IRQ_CHANNEL) {
	case INT_3:
	    irq = 3;
	    break;
	case INT_5:
	    irq = 5;
	    break;
	case INT_7:
	    irq = 7;
	    break;
	case INT_9:
	    irq = 9;
	    break;
	case INT_10:
	    irq = 10;
	    break;
	case INT_11:
	    irq = 11;
	    break;
	case INT_12:
	    irq = 12;
	    break;
	case INT_15:
	    irq = 15;
	    break;
	default:
				/* Disabled */
	    printf("ep: 3COM Network Adapter at "
		   "slot %d has its IRQ disabled. "
		   "Probe failed.\n", 
		   eisa_get_slot(dev));
	    return ENXIO;
	}

	switch(eisa_get_id(dev)) {
		case EISA_DEVICE_ID_3COM_3C579_BNC:
		case EISA_DEVICE_ID_3COM_3C579_TP:
			int_trig = EISA_TRIGGER_LEVEL;
			break;
		default:
			int_trig = EISA_TRIGGER_EDGE;
			break;
	}
			
	eisa_add_intr(dev, irq, int_trig);

	return 0;
}

static int
ep_eisa_attach(device_t dev)
{
	struct ep_softc *	sc = device_get_softc(dev);
	struct resource *	eisa_io = NULL;
	u_int32_t		eisa_iobase;
	int			irq;
	int			error = 0;
	int			rid;

	rid = 1;
	eisa_io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				     0, ~0, 1, RF_ACTIVE);
	if (!eisa_io) {
		device_printf(dev, "No I/O space?!\n");
		error = ENXIO;
		goto bad;
	}
	eisa_iobase = rman_get_start(eisa_io);

	/* Reset and Enable the card */
	outb(eisa_iobase + EP_W0_CONFIG_CTRL, W0_P4_CMD_RESET_ADAPTER);
	DELAY(1000); /* we must wait at least 1 ms */
	outb(eisa_iobase + EP_W0_CONFIG_CTRL, W0_P4_CMD_ENABLE_ADAPTER);
	/* Now the registers are availible through the lower ioport */

	if ((error = ep_alloc(dev))) {
		device_printf(dev, "ep_alloc() failed! (%d)\n", error);
		goto bad;
	}

	switch(eisa_get_id(dev)) {
		case EISA_DEVICE_ID_3COM_3C579_BNC:
		case EISA_DEVICE_ID_3COM_3C579_TP:
			sc->stat = F_ACCESS_32_BITS;
			break;
		default:
			break;
	}

	ep_get_media(sc);

	irq = rman_get_start(sc->irq);
	if (irq == 9)
		irq = 2;

	GO_WINDOW(0);
	SET_IRQ(BASE, irq);

	if ((error = ep_attach(sc))) {
		device_printf(dev, "ep_attach() failed! (%d)\n", error);
		goto bad;
	}

	if ((error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET, ep_intr,
				   sc, &sc->ep_intrhand))) {
		device_printf(dev, "bus_setup_intr() failed! (%d)\n", error);
		goto bad;
	}

	return (0);

 bad:
	if (eisa_io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, eisa_io);

	ep_free(dev);
	return (error);
}

static device_method_t ep_eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ep_eisa_probe),
	DEVMETHOD(device_attach,	ep_eisa_attach),

	{ 0, 0 }
};

static driver_t ep_eisa_driver = {
	"ep",
	ep_eisa_methods,
	sizeof(struct ep_softc),
};

extern devclass_t ep_devclass;

DRIVER_MODULE(ep, eisa, ep_eisa_driver, ep_devclass, 0, 0);
