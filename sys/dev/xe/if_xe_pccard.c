/*
 * Copyright (c) 2002 Takeshi Shibagaki
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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
 *
 * xe pccard interface driver
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
 
#include <net/ethernet.h>
#include <net/if.h> 
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/if_mib.h>


#include <dev/xe/if_xereg.h>
#include <dev/xe/if_xevar.h>

#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccarddevs.h>
#include "card_if.h"

#define XE_VENDOR_ID_XIRCOM 0x0105
#define XE_VENDOR_ID_COMPAQ_1 0x0138
#define XE_VENDOR_ID_COMPAQ_2 0x0183
#define XE_VENDOR_ID_INTEL 0x0089
#define XE_VENDOR_ID_UNKNOWN 0

struct xe_vendor_table {
	u_int32_t vendor_id;
	char *vendor_desc;
} xe_vendor_devs[] = {
	{ XE_VENDOR_ID_XIRCOM, "Xircom" },
	{ XE_VENDOR_ID_COMPAQ_1, "Compaq" },
	{ XE_VENDOR_ID_COMPAQ_2, "Compaq" },
	{ XE_VENDOR_ID_INTEL, "Intel" },
	{ XE_VENDOR_ID_UNKNOWN, "Unknown" }
};

#define XE_CARD_TYPE_FLAGS_NO 0x0
#define XE_CARD_TYPE_FLAGS_CE2 0x1
#define XE_CARD_TYPE_FLAGS_MOHAWK 0x2
#define XE_CARD_TYPE_FLAGS_DINGO 0x4
#define XE_PROD_UMASK 0x100f
#define XE_PROD_MODEM_UMASK 0x1000
#define XE_PROD_SINGLE_ID1 0x1
#define XE_PROD_SINGLE_ID2 0x2
#define XE_PROD_SINGLE_ID3 0x3
#define XE_PROD_MULTI_ID1 0x1001
#define XE_PROD_MULTI_ID2 0x1002
#define XE_PROD_MULTI_ID3 0x1003
#define XE_PROD_MULTI_ID4 0x1004
#define XE_PROD_MULTI_ID5 0x1005
#define XE_PROD_MULTI_ID6 0x1006 
#define XE_PROD_MULTI_ID7 0x1007  

struct xe_card_type_table {
	u_int32_t prod_type;
	char *card_type_desc;
	u_int32_t flags;
} xe_card_type_devs[] = {
	{ XE_PROD_MULTI_ID1, "CEM", XE_CARD_TYPE_FLAGS_NO },
	{ XE_PROD_MULTI_ID2, "CEM2", XE_CARD_TYPE_FLAGS_CE2 },
	{ XE_PROD_MULTI_ID3, "CEM3", XE_CARD_TYPE_FLAGS_CE2 },
	{ XE_PROD_MULTI_ID4, "CEM33", XE_CARD_TYPE_FLAGS_CE2 },
	{ XE_PROD_MULTI_ID5, "CEM56M", XE_CARD_TYPE_FLAGS_MOHAWK },
	{ XE_PROD_MULTI_ID6, "CEM56", XE_CARD_TYPE_FLAGS_MOHAWK |
					XE_CARD_TYPE_FLAGS_DINGO },
	{ XE_PROD_MULTI_ID7, "CEM56", XE_CARD_TYPE_FLAGS_MOHAWK |
					XE_CARD_TYPE_FLAGS_DINGO },
	{ XE_PROD_SINGLE_ID1, "CE", XE_CARD_TYPE_FLAGS_NO },
	{ XE_PROD_SINGLE_ID2, "CE2", XE_CARD_TYPE_FLAGS_CE2 },
	{ XE_PROD_SINGLE_ID3, "CE3", XE_CARD_TYPE_FLAGS_MOHAWK },
	{ 0, NULL,  -1 }
};

/*
 * Prototypes
 */
static int xe_cem56fix(device_t dev);
static struct xe_vendor_table *xe_vendor_lookup(u_int32_t devid,
					struct xe_vendor_table *tbl);
static struct xe_card_type_table *xe_card_type_lookup(u_int32_t devid,
					struct xe_card_type_table *tbl);

/*
 * Fixing for RealPort cards - they need a little furtling to get the
 * ethernet working. But this codes don't work well in NEWCARD.
 */
static int
xe_cem56fix(device_t dev)
{
	struct xe_softc *sc = (struct xe_softc *) device_get_softc(dev);
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	struct resource *r;
	int rid;
	int ioport;

	device_printf(dev, "Realport port 0x%0lx, size 0x%0lx\n",
		bus_get_resource_start(dev, SYS_RES_IOPORT, sc->port_rid),
		bus_get_resource_count(dev, SYS_RES_IOPORT, sc->port_rid));

	rid = 0;
	r = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0,
			       ~0, 4 << 10, RF_ACTIVE);
	if (!r) {
		device_printf(dev, "Can't map in attribute memory\n");
		return (-1);
	}

	bsh = rman_get_bushandle(r);
	bst = rman_get_bustag(r);

	CARD_SET_RES_FLAGS(device_get_parent(dev), dev, SYS_RES_MEMORY, rid,
			   PCCARD_A_MEM_ATTR);

	bus_space_write_1(bst, bsh, DINGO_ECOR, DINGO_ECOR_IRQ_LEVEL |
						DINGO_ECOR_INT_ENABLE |
						DINGO_ECOR_IOB_ENABLE |
						DINGO_ECOR_ETH_ENABLE);
	ioport = bus_get_resource_start(dev, SYS_RES_IOPORT, sc->port_rid);
	bus_space_write_1(bst, bsh, DINGO_EBAR0, ioport & 0xff);
	bus_space_write_1(bst, bsh, DINGO_EBAR1, (ioport >> 8) & 0xff);

	bus_space_write_1(bst, bsh, DINGO_DCOR0, DINGO_DCOR0_SF_INT);
	bus_space_write_1(bst, bsh, DINGO_DCOR1, DINGO_DCOR1_INT_LEVEL |
						 DINGO_DCOR1_EEDIO);
	bus_space_write_1(bst, bsh, DINGO_DCOR2, 0x00);
	bus_space_write_1(bst, bsh, DINGO_DCOR3, 0x00);
	bus_space_write_1(bst, bsh, DINGO_DCOR4, 0x00);

	bus_release_resource(dev, SYS_RES_MEMORY, rid, r);

	/* success! */
	return (0);
}

static struct xe_vendor_table *
xe_vendor_lookup(u_int32_t devid, struct xe_vendor_table *tbl)
{
	while(tbl->vendor_id) {
		if(tbl->vendor_id == devid)
			return (tbl);
		tbl++;
	}       
	return (tbl); /* return Unknown */
}
      
static struct xe_card_type_table *
xe_card_type_lookup(u_int32_t devid, struct xe_card_type_table *tbl)
{
	while(tbl->prod_type) {
		if(tbl->prod_type == (devid & XE_PROD_UMASK))
			return (tbl);
		tbl++;
	}
	return (NULL);
}

/*
 * PCMCIA probe routine.
 * Identify the device.  Called from the bus driver when the card is
 * inserted or otherwise powers up.
 */
static int
xe_pccard_probe(device_t dev)
{
	struct xe_softc *scp = (struct xe_softc *) device_get_softc(dev);
	u_int32_t vendor,prodid,prod;
	u_int16_t prodext;
	char *cis3_str=NULL;
	struct xe_vendor_table *vendor_itm;
	struct xe_card_type_table *card_itm;
	int i;

	/*
	 * PCCARD_CISTPL_MANFID = 0x20
	 */
	pccard_get_vendor(dev, &vendor);
	vendor_itm = xe_vendor_lookup(vendor, &xe_vendor_devs[0]);
	if (vendor_itm == NULL)
		return (ENODEV);
	scp->vendor = vendor_itm->vendor_desc;
	pccard_get_product(dev, &prodid);
	pccard_get_prodext(dev, &prodext);
	/*
	 * prod(new) =  rev, media, prod(old)
	 * prod(new) =  (don't care), (care 0x10 bit), (care 0x0f bit)
	 */
	prod = (prodid << 8) | prodext;
	card_itm = xe_card_type_lookup(prod, &xe_card_type_devs[0]);
	if (card_itm == NULL)
		return (ENODEV);
	scp->card_type = card_itm->card_type_desc;
	if (card_itm->prod_type & XE_PROD_MODEM_UMASK)
		scp->modem = 1;
	for(i=1; i!=XE_CARD_TYPE_FLAGS_DINGO; i=i<<1) {
		switch(i & card_itm->flags) {
		case XE_CARD_TYPE_FLAGS_CE2:
			scp->ce2 = 1; break;
		case XE_CARD_TYPE_FLAGS_MOHAWK:
			scp->mohawk = 1; break;
		case XE_CARD_TYPE_FLAGS_DINGO:
			scp->dingo = 1; break;
		}
	}
	/*
	 * PCCARD_CISTPL_VERS_1 = 0x15
	 */
	pccard_get_cis3_str(dev, &cis3_str);
	if (strcmp(scp->card_type, "CE") == 0)
		if (strcmp(cis3_str, "CE2") ==0)
			scp->card_type = "CE2"; /* Look for "CE2" string */

	/*
	 * PCCARD_CISTPL_FUNCE = 0x22
	 */
	pccard_get_ether(dev, scp->arpcom.ac_enaddr);

	/* Reject unsupported cards */
	if(strcmp(scp->card_type, "CE") == 0
	|| strcmp(scp->card_type, "CEM") == 0) {
		device_printf(dev, "Sorry, your %s card is not supported :(\n",
				scp->card_type);
		return (ENODEV);
	}

	/* Success */
	return (0);
}

/*
 * Attach a device.
 */
static int
xe_pccard_attach(device_t dev)
{
	struct xe_softc *scp = device_get_softc(dev);
	int err;

	if ((err = xe_activate(dev)) != 0)
		return (err);
         
	/* Hack RealPorts into submission */
	if (scp->dingo && xe_cem56fix(dev) < 0) {
		device_printf(dev, "Unable to fix your RealPort\n");
		xe_deactivate(dev);
		return (ENODEV);
	}
	if ((err = xe_attach(dev))) {
		device_printf(dev, "xe_attach() failed! (%d)\n", err);
		return (err);
	}
	return (0);
}

/*
 * The device entry is being removed, probably because someone ejected the
 * card.  The interface should have been brought down manually before calling
 * this function; if not you may well lose packets.  In any case, I shut down
 * the card and the interface, and hope for the best.
 */
static int
xe_pccard_detach(device_t dev)
{
	struct xe_softc *sc = device_get_softc(dev);

	sc->arpcom.ac_if.if_flags &= ~IFF_RUNNING;
	ether_ifdetach(&sc->arpcom.ac_if, ETHER_BPF_SUPPORTED);
	xe_deactivate(dev);
	return (0);
}

static const struct pccard_product xe_pccard_products[] = {
	PCMCIA_CARD(COMPAQ2, CPQ_10_100, 0),
	PCMCIA_CARD(XIRCOM, CE, 0),
	PCMCIA_CARD(XIRCOM, CE2, 0),
	PCMCIA_CARD(XIRCOM, CE3, 0),
	PCMCIA_CARD(XIRCOM, CEM, 0),
	PCMCIA_CARD(XIRCOM, CEM28, 0),
	PCMCIA_CARD(XIRCOM, CEM33, 0),
	PCMCIA_CARD(XIRCOM, CEM56, 0),
	PCMCIA_CARD(XIRCOM, REM56, 0),
        { NULL }
};

static int
xe_pccard_match(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, xe_pccard_products,
	     sizeof(xe_pccard_products[0]), NULL)) != NULL) {
		device_set_desc(dev, pp->pp_name);
		return (0);
	}
	return (EIO);
}

static device_method_t xe_pccard_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         pccard_compat_probe),
        DEVMETHOD(device_attach,        pccard_compat_attach),
        DEVMETHOD(device_detach,        xe_pccard_detach),

        /* Card interface */
        DEVMETHOD(card_compat_match,    xe_pccard_match),
        DEVMETHOD(card_compat_probe,    xe_pccard_probe),
        DEVMETHOD(card_compat_attach,   xe_pccard_attach),

        { 0, 0 }
};

static driver_t xe_pccard_driver = {
        "xe",
        xe_pccard_methods,
        sizeof(struct xe_softc),
};

devclass_t xe_devclass;

DRIVER_MODULE(xe, pccard, xe_pccard_driver, xe_devclass, 0, 0);
