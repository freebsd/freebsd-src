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
 *	$Id: 3c5x9.c,v 1.4 1996/07/19 13:19:47 amurai Exp $
 */

#include "eisa.h"
#if NEISA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/clock.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <i386/isa/if_epreg.h>
#include <i386/eisa/eisaconf.h>

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

static int	ep_eisa_probe __P((void));
static int	ep_eisa_attach __P((struct eisa_device *e_dev));

struct eisa_driver ep_eisa_driver = {
					"ep",
					ep_eisa_probe,
					ep_eisa_attach,
					/*shutdown*/NULL,
					&ep_unit
				      };

DATA_SET (eisadriver_set, ep_eisa_driver);

static char   *ep_match __P((eisa_id_t type));

static  char*
ep_match(type)
	eisa_id_t type;
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
ep_eisa_probe(void)
{
	u_long iobase;
	struct eisa_device *e_dev = NULL;
	int count;

	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, ep_match))) {
		u_short conf;
		u_long port;
		int irq;

		port = (e_dev->ioconf.slot * EISA_SLOT_SIZE);
		iobase = port + EP_EISA_SLOT_OFFSET;

		/* We must be in EISA configuration mode */
		if ((inw(iobase + EP_W0_ADDRESS_CFG) & 0x1f) != 0x1f)
                	continue;

		eisa_add_iospace(e_dev, iobase, EP_EISA_IOSIZE, RESVADDR_NONE);
		eisa_add_iospace(e_dev, port, EP_IOSIZE, RESVADDR_NONE);

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
					e_dev->ioconf.slot);
				continue;
		}
		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &ep_eisa_driver);
		count++;
	}
	return count;
}

static int
ep_eisa_attach(e_dev)
	struct eisa_device *e_dev;
{
	struct ep_softc *sc;
	struct ep_board *epb;
	int unit = e_dev->unit;
	int irq = ffs(e_dev->ioconf.irq) - 1;
	resvaddr_t *ioport;
	resvaddr_t *eisa_ioport;
	u_char level_intr;
	int i;

	/*
	 * The addresses are sorted in increasing order
	 * so we know the port to pass to the core ep
	 * driver comes first.
	 */
	ioport = e_dev->ioconf.ioaddrs.lh_first;

	if(!ioport)
		return -1;

	eisa_ioport = ioport->links.le_next;

	if(!eisa_ioport)
		return -1;
	
	eisa_reg_start(e_dev);
	if(eisa_reg_iospace(e_dev, ioport))
		return -1;

	if(eisa_reg_iospace(e_dev, eisa_ioport))
		return -1;

	epb = &ep_board[ep_boards];

	epb->epb_addr = ioport->addr;
	epb->epb_used = 1;

	if(!(sc = ep_alloc(unit, epb)))
		return -1;

	ep_boards++;

	sc->stat = 0;
	level_intr = FALSE;
	switch(e_dev->id) {
		case EISA_DEVICE_ID_3COM_3C509_TP:
			sc->ep_connectors = UTP|AUI;
			break;
		case EISA_DEVICE_ID_3COM_3C509_BNC:
			sc->ep_connectors = BNC|AUI;
			break;
		case EISA_DEVICE_ID_3COM_3C579_TP:
			sc->ep_connectors = UTP|AUI;
			sc->stat = F_ACCESS_32_BITS;
			level_intr = TRUE;
			break;
		case EISA_DEVICE_ID_3COM_3C579_BNC:
			sc->ep_connectors = BNC|AUI;
			sc->stat = F_ACCESS_32_BITS;
			level_intr = TRUE;
			break;
		case EISA_DEVICE_ID_3COM_3C509_COMBO:
			sc->ep_connectors = UTP|BNC|AUI;
			break;
		case EISA_DEVICE_ID_3COM_3C509_TPO:
			sc->ep_connectors = UTP;
			break;
		default:
			break;
        }
	/*
	 * Set the eisa config selected media type
	 */
	sc->ep_connector = inw(eisa_ioport->addr + EISA_BPROM_MEDIA_CONF)
			   >> ACF_CONNECTOR_BITS;

	if(eisa_reg_intr(e_dev, irq, ep_intr, (void *)sc, &net_imask,
			 /*shared ==*/level_intr)) {
		ep_free(sc);
		return -1;
	}
	eisa_reg_end(e_dev);

	/* Reset and Enable the card */
	outb(eisa_ioport->addr + EP_W0_CONFIG_CTRL, W0_P4_CMD_RESET_ADAPTER);
	DELAY(1000); /* we must wait at least 1 ms */
	outb(eisa_ioport->addr + EP_W0_CONFIG_CTRL, W0_P4_CMD_ENABLE_ADAPTER);

	/* Now the registers are availible through the lower ioport */

	/*
	 * Retrieve our ethernet address
	 */
	 GO_WINDOW(0);
	for(i = 0; i < 3; i++)
		sc->epb->eth_addr[i] = get_e(sc, i);

        /* Even we get irq number from board, we should tell him..
            Otherwise we never get a H/W interrupt anymore...*/
        if ( irq == 9 )
               irq = 2;
        SET_IRQ(eisa_ioport->addr, irq);

	ep_attach(sc);

	if(eisa_enable_intr(e_dev, irq)) {
		ep_free(sc);
		eisa_release_intr(e_dev, irq, ep_intr);
		return -1;
	}

	return 0;
}

#endif /* NEISA > 0 */
