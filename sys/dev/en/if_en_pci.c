/*	$NetBSD: if_en_pci.c,v 1.1 1996/06/22 02:00:31 chuck Exp $	*/

/*
 *
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
 * $FreeBSD$
 */

/*
 *
 * i f _ e n _ p c i . c  
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * started: spring, 1996.
 *
 * FreeBSD PCI glue for the eni155p card.
 * thanks to Matt Thomas for figuring out FreeBSD vs NetBSD vs etc.. diffs.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <dev/en/midwayreg.h>
#include <dev/en/midwayvar.h>

/*
 * prototypes
 */

static	int en_pci_probe(device_t);
static	int en_pci_attach(device_t);
static	int en_pci_detach(device_t);
static	int en_pci_shutdown(device_t);

/*
 * local structures
 */

struct en_pci_softc {
  /* bus independent stuff */
  struct en_softc esc;		/* includes "device" structure */

  /* freebsd newbus glue */
  struct resource *res;		/* resource descriptor for registers */
  struct resource *irq;		/* resource descriptor for interrupt */
  void *ih;			/* interrupt handle */
};

#if !defined(MIDWAY_ENIONLY)
static  void eni_get_macaddr(device_t, struct en_pci_softc *);
#endif
#if !defined(MIDWAY_ADPONLY)
static  void adp_get_macaddr(struct en_pci_softc *);
#endif

/*
 * local defines (PCI specific stuff)
 */

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

#if !defined(MIDWAY_ENIONLY)

static void adp_busreset(void *);

/*
 * bus specific reset function [ADP only!]
 */

static void adp_busreset(v)

void *v;

{
  struct en_softc *sc = (struct en_softc *) v;
  u_int32_t dummy;

  bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG, ADP_PCIREG_RESET);
  DELAY(1000);  /* let it reset */
  dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
  bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG, 
		(ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA|ADP_PCIREG_IENABLE));
  dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
  if ((dummy & (ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA)) !=
		(ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA))
    printf("adp_busreset: Adaptec ATM did NOT reset!\n");
}
#endif

/***********************************************************************/

/*
 * autoconfig stuff
 */

static int
en_pci_probe(device_t dev)
{
  switch (pci_get_vendor(dev)) {
#if !defined(MIDWAY_ADPONLY)
  case PCI_VENDOR_EFFICIENTNETS:
    switch (pci_get_device(dev)) {
    case PCI_PRODUCT_EFFICIENTNETS_ENI155PF:
    case PCI_PRODUCT_EFFICIENTNETS_ENI155PA:
      device_set_desc(dev, "Efficient Networks ENI-155p");
      return 0;
    }
    break;
#endif
#if !defined(MIDWAY_ENIONLY)
  case PCI_VENDOR_ADP:
    switch (pci_get_device(dev)) {
    case PCI_PRODUCT_ADP_AIC5900:
    case PCI_PRODUCT_ADP_AIC5905:
      device_set_desc(dev, "Adaptec 155 ATM");
      return 0;
    }
    break;
#endif
  }
  return ENXIO;
}

static int
en_pci_attach(device_t dev)
{
  struct en_softc *sc;
  struct en_pci_softc *scp;
  u_long val;
  int rid, s, unit, error = 0;

  sc = device_get_softc(dev);
  scp = (struct en_pci_softc *)sc;
  bzero(scp, sizeof(*scp));		/* zero */

  s = splimp();

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
  scp->res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				0, ~0, 1, RF_ACTIVE);
  if (!scp->res) {
    device_printf(dev, "could not map memory\n");
    error = ENXIO;
    goto fail;
  }

  sc->en_memt = rman_get_bustag(scp->res);
  sc->en_base = rman_get_bushandle(scp->res);
  
  /*
   * Allocate our interrupt.
   */
  rid = 0;
  scp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				RF_SHAREABLE | RF_ACTIVE);
  if (scp->irq == NULL) {
    device_printf(dev, "could not map interrupt\n");
    bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, scp->res);
    error = ENXIO;
    goto fail;
  }

  error = bus_setup_intr(dev, scp->irq, INTR_TYPE_NET,
			 en_intr, sc, &scp->ih);
  if (error) {
    device_printf(dev, "could not setup irq\n");
    bus_release_resource(dev, SYS_RES_IRQ, 0, scp->irq);
    bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, scp->res);
    goto fail;
  }
  sc->ipl = 1; /* XXX (required to enable interrupt on midway) */

  unit = device_get_unit(dev);
  snprintf(sc->sc_dev.dv_xname, sizeof(sc->sc_dev.dv_xname), "en%d", unit);
  sc->enif.if_unit = unit;
  sc->enif.if_name = "en";

  /* figure out if we are an adaptec card or not */
  sc->is_adaptec = (pci_get_vendor(dev) == PCI_VENDOR_ADP) ? 1 : 0;
  
  /*
   * set up pci bridge
   */
#if !defined(MIDWAY_ENIONLY)
  if (sc->is_adaptec) {
    adp_get_macaddr(scp);
    sc->en_busreset = adp_busreset;
    adp_busreset(sc);
  }
#endif

#if !defined(MIDWAY_ADPONLY)
  if (!sc->is_adaptec) {
    eni_get_macaddr(dev, scp);
    sc->en_busreset = NULL;
    pci_write_config(dev, EN_TONGA, (TONGA_SWAP_DMA|TONGA_SWAP_WORD), 4);
  }
#endif

  /*
   * done PCI specific stuff
   */

  en_attach(sc);

  splx(s);

  return 0;

 fail:
  splx(s);
  return error;
}

static int
en_pci_detach(device_t dev)
{
	struct en_softc *sc = device_get_softc(dev);
	struct en_pci_softc *scp = (struct en_pci_softc *)sc;
	int s;

	s = splimp();

	/*
	 * Close down routes etc.
	 */
	if_detach(&sc->enif);

	/*
	 * Stop DMA and drop transmit queue.
	 */
	en_reset(sc);

	/*
	 * Deallocate resources.
	 */
	bus_teardown_intr(dev, scp->irq, scp->ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, scp->irq);
	bus_release_resource(dev, SYS_RES_MEMORY, PCI_CBMA, scp->res);

#ifdef notyet
	/*
	 * Free all the driver internal resources
	 */
#endif

	splx(s);

	return 0;
}

static int
en_pci_shutdown(device_t dev)
{
    struct en_pci_softc *psc = (struct en_pci_softc *)device_get_softc(dev);

    en_reset(&psc->esc);
    DELAY(10);	/* is this necessary? */
    return (0);
}

#if !defined(MIDWAY_ENIONLY)

#if defined(sparc)
#define bus_space_read_1(t, h, o) \
  		((void)t, (*(volatile u_int8_t *)((h) + (o))))
#endif

static void 
adp_get_macaddr(scp)
     struct en_pci_softc *scp;
{
  struct en_softc * sc = (struct en_softc *)scp;
  int lcv;

  for (lcv = 0; lcv < sizeof(sc->macaddr); lcv++)
	  sc->macaddr[lcv] = bus_space_read_1(sc->en_memt, sc->en_base,
					      MID_ADPMACOFF + lcv);
}

#endif /* MIDWAY_ENIONLY */

#if !defined(MIDWAY_ADPONLY)

/*
 * Read station (MAC) address from serial EEPROM.
 * derived from linux drivers/atm/eni.c by Werner Almesberger, EPFL LRC.
 */
#define EN_PROM_MAGIC  0x0c
#define EN_PROM_DATA   0x02
#define EN_PROM_CLK    0x01
#define EN_ESI         64

static void 
eni_get_macaddr(device_t dev, struct en_pci_softc *scp)
{
  struct en_softc * sc = (struct en_softc *)scp;
  int i, j, address, status;
  u_int32_t data, t_data;
  u_int8_t tmp;
  
  t_data = pci_read_config(dev, EN_TONGA, 4) & 0xffffff00;

  data =  EN_PROM_MAGIC | EN_PROM_DATA | EN_PROM_CLK;
  pci_write_config(dev, EN_TONGA, data, 4);

  for (i = 0; i < sizeof(sc->macaddr); i ++){
    /* start operation */
    data |= EN_PROM_DATA ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data |= EN_PROM_CLK ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data &= ~EN_PROM_DATA ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data &= ~EN_PROM_CLK ;
    pci_write_config(dev, EN_TONGA, data, 4);
    /* send address with serial line */
    address = ((i + EN_ESI) << 1) + 1;
    for ( j = 7 ; j >= 0 ; j --){
      data = (address >> j) & 1 ? data | EN_PROM_DATA :
      data & ~EN_PROM_DATA;
      pci_write_config(dev, EN_TONGA, data, 4);
      data |= EN_PROM_CLK ;
      pci_write_config(dev, EN_TONGA, data, 4);
      data &= ~EN_PROM_CLK ;
      pci_write_config(dev, EN_TONGA, data, 4);
    }
    /* get ack */
    data |= EN_PROM_DATA ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data |= EN_PROM_CLK ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data = pci_read_config(dev, EN_TONGA, 4);
    status = data & EN_PROM_DATA;
    data &= ~EN_PROM_CLK ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data |= EN_PROM_DATA ;
    pci_write_config(dev, EN_TONGA, data, 4);

    tmp = 0;

    for ( j = 7 ; j >= 0 ; j --){
      tmp <<= 1;
      data |= EN_PROM_DATA ;
      pci_write_config(dev, EN_TONGA, data, 4);
      data |= EN_PROM_CLK ;
      pci_write_config(dev, EN_TONGA, data, 4);
      data = pci_read_config(dev, EN_TONGA, 4);
      if(data & EN_PROM_DATA) tmp |= 1;
      data &= ~EN_PROM_CLK ;
      pci_write_config(dev, EN_TONGA, data, 4);
      data |= EN_PROM_DATA ;
      pci_write_config(dev, EN_TONGA, data, 4);
    }
    /* get ack */
    data |= EN_PROM_DATA ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data |= EN_PROM_CLK ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data = pci_read_config(dev, EN_TONGA, 4);
    status = data & EN_PROM_DATA;
    data &= ~EN_PROM_CLK ;
    pci_write_config(dev, EN_TONGA, data, 4);
    data |= EN_PROM_DATA ;
    pci_write_config(dev, EN_TONGA, data, 4);

    sc->macaddr[i] = tmp;
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

#endif /* !MIDWAY_ADPONLY */

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

DRIVER_MODULE(if_en, pci, en_driver, en_devclass, 0, 0);

