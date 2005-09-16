/*	$NetBSD: if_en_pci.c,v 1.1 1996/06/22 02:00:31 chuck Exp $	*/
/*-
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *	Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * i f _ e n _ p c i . c  
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * started: spring, 1996.
 *
 * FreeBSD PCI glue for the eni155p card.
 * thanks to Matt Thomas for figuring out FreeBSD vs NetBSD vs etc.. diffs.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/condvar.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_atm.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/utopia/utopia.h>
#include <dev/en/midwayreg.h>
#include <dev/en/midwayvar.h>

MODULE_DEPEND(en, pci, 1, 1, 1);
MODULE_DEPEND(en, atm, 1, 1, 1);
MODULE_DEPEND(en, utopia, 1, 1, 1);

/*
 * local structures
 */
struct en_pci_softc {
	/* bus independent stuff */
	struct en_softc esc;	/* includes "device" structure */

	/* freebsd newbus glue */
	struct resource *res;	/* resource descriptor for registers */
	struct resource *irq;	/* resource descriptor for interrupt */
	void *ih;		/* interrupt handle */
};

static  void eni_get_macaddr(device_t, struct en_pci_softc *);
static  void adp_get_macaddr(struct en_pci_softc *);

/* 
 * address of config base memory address register in PCI config space
 * (this is card specific)
 */
#define PCI_CBMA        0x10

/*
 * tonga (pci bridge).   ENI cards only!
 */
#define EN_TONGA        0x60            /* PCI config addr of tonga reg */

#define TONGA_SWAP_DMA  0x80            /* endian swap control */
#define TONGA_SWAP_BYTE 0x40
#define TONGA_SWAP_WORD 0x20
#define TONGA_READ_MULT	0x00
#define TONGA_READ_MEM	0x04
#define TONGA_READ_IVAN	0x08
#define TONGA_READ_KEN	0x0C

/*
 * adaptec pci bridge.   ADP cards only!
 */
#define ADP_PCIREG      0x050040        /* PCI control register */

#define ADP_PCIREG_RESET        0x1     /* reset card */
#define ADP_PCIREG_IENABLE	0x2	/* interrupt enable */
#define ADP_PCIREG_SWAP_WORD	0x4	/* swap byte on slave access */
#define ADP_PCIREG_SWAP_DMA	0x8	/* swap byte on DMA */

#define PCI_VENDOR_EFFICIENTNETS 0x111a			/* Efficent Networks */
#define PCI_PRODUCT_EFFICIENTNETS_ENI155PF 0x0000	/* ENI-155P ATM */
#define PCI_PRODUCT_EFFICIENTNETS_ENI155PA 0x0002	/* ENI-155P ATM */
#define PCI_VENDOR_ADP 0x9004				/* adaptec */
#define PCI_PRODUCT_ADP_AIC5900 0x5900
#define PCI_PRODUCT_ADP_AIC5905 0x5905
#define PCI_VENDOR(x)		((x) & 0xFFFF)
#define PCI_CHIPID(x)		(((x) >> 16) & 0xFFFF)

/*
 * bus specific reset function [ADP only!]
 */
static void
adp_busreset(void *v)
{
	struct en_softc *sc = (struct en_softc *)v;
	uint32_t dummy;

	bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG,
	    ADP_PCIREG_RESET);
	DELAY(1000);  			/* let it reset */
	dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
	bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG, 
	    (ADP_PCIREG_SWAP_DMA | ADP_PCIREG_IENABLE));
	dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
	if ((dummy & (ADP_PCIREG_SWAP_WORD | ADP_PCIREG_SWAP_DMA)) !=
	    ADP_PCIREG_SWAP_DMA)
		if_printf(sc->ifp, "adp_busreset: Adaptec ATM did "
		    "NOT reset!\n");
}

/***********************************************************************/

/*
 * autoconfig stuff
 */
static int
en_pci_probe(device_t dev)
{

	switch (pci_get_vendor(dev)) {

	  case PCI_VENDOR_EFFICIENTNETS:
		switch (pci_get_device(dev)) {

		    case PCI_PRODUCT_EFFICIENTNETS_ENI155PF:
		    case PCI_PRODUCT_EFFICIENTNETS_ENI155PA:
			device_set_desc(dev, "Efficient Networks ENI-155p");
			return (BUS_PROBE_DEFAULT);
		}
		break;

	  case PCI_VENDOR_ADP:
		switch (pci_get_device(dev)) {

		  case PCI_PRODUCT_ADP_AIC5900:
		  case PCI_PRODUCT_ADP_AIC5905:
			device_set_desc(dev, "Adaptec 155 ATM");
			return (BUS_PROBE_DEFAULT);
		}
		break;
	}
	return (ENXIO);
}

static int
en_pci_attach(device_t dev)
{
	struct en_softc *sc;
	struct en_pci_softc *scp;
	u_long val;
	int rid, error = 0;

	sc = device_get_softc(dev);
	scp = (struct en_pci_softc *)sc;
	sc->ifp = if_alloc(IFT_ATM);
	if (sc->ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}

	if_initname(sc->ifp, device_get_name(dev),
	    device_get_unit(dev));

	/*
	 * Enable bus mastering.
	 */
	val = pci_read_config(dev, PCIR_COMMAND, 2);
	val |= (PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, val, 2);

	/*
	 * Map control/status registers.
	 */
	rid = PCI_CBMA;
	scp->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (scp->res == NULL) {
		device_printf(dev, "could not map memory\n");
		if_free(sc->ifp);
		error = ENXIO;
		goto fail;
	}

	sc->dev = dev;
	sc->en_memt = rman_get_bustag(scp->res);
	sc->en_base = rman_get_bushandle(scp->res);

	/*
	 * Allocate our interrupt.
	 */
	rid = 0;
	scp->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (scp->irq == NULL) {
		device_printf(dev, "could not map interrupt\n");
		bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, scp->res);
		if_free(sc->ifp);
		error = ENXIO;
		goto fail;
	}

	sc->ipl = 1; /* XXX (required to enable interrupt on midway) */

	/* figure out if we are an adaptec card or not */
	sc->is_adaptec = (pci_get_vendor(dev) == PCI_VENDOR_ADP) ? 1 : 0;

	/*
	 * set up pci bridge
	 */
	if (sc->is_adaptec) {
		adp_get_macaddr(scp);
		sc->en_busreset = adp_busreset;
		adp_busreset(sc);
	} else {
		eni_get_macaddr(dev, scp);
		sc->en_busreset = NULL;
		pci_write_config(dev, EN_TONGA, TONGA_SWAP_DMA | TONGA_READ_IVAN, 4);
	}

	/*
	 * Common attach stuff
	 */
	if ((error = en_attach(sc)) != 0) {
		device_printf(dev, "attach failed\n");
		bus_teardown_intr(dev, scp->irq, scp->ih);
		bus_release_resource(dev, SYS_RES_IRQ, 0, scp->irq);
		bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, scp->res);
		if_free(sc->ifp);
		goto fail;
	}

	/*
	 * Do the interrupt SETUP last just before returning
	 */
	error = bus_setup_intr(dev, scp->irq, INTR_TYPE_NET,
	    en_intr, sc, &scp->ih);
	if (error) {
		en_reset(sc);
		atm_ifdetach(sc->ifp);
		if_free(sc->ifp);
		device_printf(dev, "could not setup irq\n");
		bus_release_resource(dev, SYS_RES_IRQ, 0, scp->irq);
		bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, scp->res);
		en_destroy(sc);
		if_free(sc->ifp);
		goto fail;
	}

	return (0);

    fail:
	return (error);
}

/*
 * Detach the adapter
 */
static int
en_pci_detach(device_t dev)
{
	struct en_softc *sc = device_get_softc(dev);
	struct en_pci_softc *scp = (struct en_pci_softc *)sc;

	/*
	 * Stop DMA and drop transmit queue.
	 */
	if ((sc->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		if_printf(sc->ifp, "still running\n");
		sc->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	}

	/*
	 * Close down routes etc.
	 */
	en_reset(sc);
	atm_ifdetach(sc->ifp);
	if_free(sc->ifp);

	/*
	 * Deallocate resources.
	 */
	bus_teardown_intr(dev, scp->irq, scp->ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, scp->irq);
	bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, scp->res);

	/*
	 * Free all the driver internal resources
	 */
	en_destroy(sc);

	return (0);
}

static int
en_pci_shutdown(device_t dev)
{
	struct en_pci_softc *psc = device_get_softc(dev);

	en_reset(&psc->esc);
	DELAY(10);		/* is this necessary? */

	return (0);
}

/*
 * Get the MAC address from an Adaptec board. No idea how to get
 * serial number or other stuff, because I have no documentation for that
 * card.
 */
static void 
adp_get_macaddr(struct en_pci_softc *scp)
{
	struct en_softc * sc = (struct en_softc *)scp;
	int lcv;

	for (lcv = 0; lcv < sizeof(IFP2IFATM(sc->ifp)->mib.esi); lcv++)
		IFP2IFATM(sc->ifp)->mib.esi[lcv] = bus_space_read_1(sc->en_memt,
		    sc->en_base, MID_ADPMACOFF + lcv);
}

/*
 * Read station (MAC) address from serial EEPROM.
 * derived from linux drivers/atm/eni.c by Werner Almesberger, EPFL LRC.
 */
#define EN_PROM_MAGIC	0x0c
#define EN_PROM_DATA	0x02
#define EN_PROM_CLK	0x01
#define EN_ESI		64
#define EN_SERIAL	112

/*
 * Read a byte from the given address in the EEPROM
 */
static uint8_t
eni_get_byte(device_t dev, uint32_t *data, u_int address)
{
	int j;
	uint8_t tmp;

	address = (address << 1) + 1;

	/* start operation */
	*data |= EN_PROM_DATA ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data |= EN_PROM_CLK ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data &= ~EN_PROM_DATA ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data &= ~EN_PROM_CLK ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	/* send address with serial line */
	for ( j = 7 ; j >= 0 ; j --) {
		*data = ((address >> j) & 1) ? (*data | EN_PROM_DATA) :
		    (*data & ~EN_PROM_DATA);
		pci_write_config(dev, EN_TONGA, *data, 4);
		*data |= EN_PROM_CLK ;
		pci_write_config(dev, EN_TONGA, *data, 4);
		*data &= ~EN_PROM_CLK ;
		pci_write_config(dev, EN_TONGA, *data, 4);
	}
	/* get ack */
	*data |= EN_PROM_DATA ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data |= EN_PROM_CLK ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data = pci_read_config(dev, EN_TONGA, 4);
	*data &= ~EN_PROM_CLK ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data |= EN_PROM_DATA ;
	pci_write_config(dev, EN_TONGA, *data, 4);

	tmp = 0;

	for ( j = 7 ; j >= 0 ; j --) {
		tmp <<= 1;
		*data |= EN_PROM_DATA ;
		pci_write_config(dev, EN_TONGA, *data, 4);
		*data |= EN_PROM_CLK ;
		pci_write_config(dev, EN_TONGA, *data, 4);
		*data = pci_read_config(dev, EN_TONGA, 4);
		if(*data & EN_PROM_DATA) tmp |= 1;
		*data &= ~EN_PROM_CLK ;
		pci_write_config(dev, EN_TONGA, *data, 4);
		*data |= EN_PROM_DATA ;
		pci_write_config(dev, EN_TONGA, *data, 4);
	}
	/* get ack */
	*data |= EN_PROM_DATA ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data |= EN_PROM_CLK ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data = pci_read_config(dev, EN_TONGA, 4);
	*data &= ~EN_PROM_CLK ;
	pci_write_config(dev, EN_TONGA, *data, 4);
	*data |= EN_PROM_DATA ;
	pci_write_config(dev, EN_TONGA, *data, 4);

	return (tmp);
}

/*
 * Get MAC address and other stuff from the EEPROM
 */
static void 
eni_get_macaddr(device_t dev, struct en_pci_softc *scp)
{
	struct en_softc * sc = (struct en_softc *)scp;
	int i;
	uint32_t data, t_data;

	t_data = pci_read_config(dev, EN_TONGA, 4) & 0xffffff00;

	data =  EN_PROM_MAGIC | EN_PROM_DATA | EN_PROM_CLK;
	pci_write_config(dev, EN_TONGA, data, 4);

	for (i = 0; i < sizeof(IFP2IFATM(sc->ifp)->mib.esi); i ++)
		IFP2IFATM(sc->ifp)->mib.esi[i] = eni_get_byte(dev, &data, i + EN_ESI);

	IFP2IFATM(sc->ifp)->mib.serial = 0;
	for (i = 0; i < 4; i++) {
		IFP2IFATM(sc->ifp)->mib.serial <<= 8;
		IFP2IFATM(sc->ifp)->mib.serial |= eni_get_byte(dev, &data, i + EN_SERIAL);
	}
	/* stop operation */
	data &=  ~EN_PROM_DATA;
	pci_write_config(dev, EN_TONGA, data, 4);
	data |=  EN_PROM_CLK;
	pci_write_config(dev, EN_TONGA, data, 4);
	data |=  EN_PROM_DATA;
	pci_write_config(dev, EN_TONGA, data, 4);
	pci_write_config(dev, EN_TONGA, t_data, 4);
}

/*
 * Driver infrastructure
 */
static device_method_t en_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		en_pci_probe),
	DEVMETHOD(device_attach,	en_pci_attach),
	DEVMETHOD(device_detach,	en_pci_detach),
	DEVMETHOD(device_shutdown,	en_pci_shutdown),

	{ 0, 0 }
};

static driver_t en_driver = {
	"en",
	en_methods,
	sizeof(struct en_pci_softc),
};

static devclass_t en_devclass;

DRIVER_MODULE(en, pci, en_driver, en_devclass, en_modevent, 0);
