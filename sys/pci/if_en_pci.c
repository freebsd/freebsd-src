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

#include "en.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#if defined(__FreeBSD__)
#include <sys/eventhandler.h>
#endif
#include <sys/malloc.h>
#include <sys/socket.h>


#include <net/if.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <dev/en/midwayreg.h>
#include <dev/en/midwayvar.h>

#ifndef COMPAT_OLDPCI
#error "The en device requires the old pci compatibility shims"
#endif


/*
 * prototypes
 */

static	void en_pci_attach __P((pcici_t, int));
static	const char *en_pci_probe __P((pcici_t, pcidi_t));
static void en_pci_shutdown __P((void *, int));

/*
 * local structures
 */

struct en_pci_softc {
  /* bus independent stuff */
  struct en_softc esc;		/* includes "device" structure */

  /* PCI bus glue */
  void *sc_ih;			/* interrupt handle */
  pci_chipset_tag_t en_pc;	/* for PCI calls */
  pcici_t en_confid;		/* config id */
};

#if !defined(MIDWAY_ENIONLY)
static  void eni_get_macaddr __P((struct en_pci_softc *));
#endif
#if !defined(MIDWAY_ADPONLY)
static  void adp_get_macaddr __P((struct en_pci_softc *));
#endif

/*
 * pointers to softcs (we alloc)
 */

static struct en_pci_softc *enpcis[NEN] = {0};
extern struct cfdriver en_cd;

/*
 * autoconfig structures
 */

static u_long en_pci_count;

static struct pci_device endevice = {
	"en",
	en_pci_probe,
	en_pci_attach,
	&en_pci_count,
	NULL,
};  

COMPAT_PCI_DRIVER (en, endevice);

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

static void adp_busreset __P((void *));

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

static const char *en_pci_probe(config_id, device_id)

pcici_t config_id;
pcidi_t device_id;

{
#if !defined(MIDWAY_ADPONLY)
  if (PCI_VENDOR(device_id) == PCI_VENDOR_EFFICIENTNETS && 
      (PCI_CHIPID(device_id) == PCI_PRODUCT_EFFICIENTNETS_ENI155PF ||
       PCI_CHIPID(device_id) == PCI_PRODUCT_EFFICIENTNETS_ENI155PA))
    return "Efficient Networks ENI-155p";
#endif

#if !defined(MIDWAY_ENIONLY)
  if (PCI_VENDOR(device_id) == PCI_VENDOR_ADP && 
      (PCI_CHIPID(device_id) == PCI_PRODUCT_ADP_AIC5900 ||
       PCI_CHIPID(device_id) == PCI_PRODUCT_ADP_AIC5905))
    return "Adaptec 155 ATM";
#endif

  return 0;
}

static void en_pci_attach(config_id, unit)

pcici_t config_id;
int unit;

{
  struct en_softc *sc;
  struct en_pci_softc *scp;
  pcidi_t device_id;
  int retval;
  vm_offset_t pa;

  if (unit >= NEN) {
    printf("en%d: not configured; kernel is built for only %d device%s.\n",
	unit, NEN, NEN == 1 ? "" : "s");
    return;
  }

  scp = (struct en_pci_softc *) malloc(sizeof(*scp), M_DEVBUF, M_NOWAIT);
  if (scp == NULL)
    return;
  bzero(scp, sizeof(*scp));		/* zero */
  sc = &scp->esc;

  retval = pci_map_mem(config_id, PCI_CBMA, (vm_offset_t *) &sc->en_base, &pa);

  if (!retval) {
    free((caddr_t) scp, M_DEVBUF);
    return;
  }
  enpcis[unit] = scp;			/* lock it in */
  en_cd.cd_devs[unit] = sc;		/* fake a cfdriver structure */
  en_cd.cd_ndevs = NEN;
  snprintf(sc->sc_dev.dv_xname, sizeof(sc->sc_dev.dv_xname), "en%d", unit);
  sc->enif.if_unit = unit;
  sc->enif.if_name = "en";
  scp->en_confid = config_id;

  /*
   * figure out if we are an adaptec card or not.
   * XXX: why do we have to re-read PC_ID_REG when en_pci_probe already
   * had that info?
   */

  device_id = pci_conf_read(config_id, PCI_ID_REG);
  sc->is_adaptec = (PCI_VENDOR(device_id) == PCI_VENDOR_ADP) ? 1 : 0;
  
  /*
   * Add shutdown hook so that DMA is disabled prior to reboot. Not
   * doing so could allow DMA to corrupt kernel memory during the
   * reboot before the driver initializes.
   */
  EVENTHANDLER_REGISTER(shutdown_post_sync, en_pci_shutdown, scp,
			SHUTDOWN_PRI_DEFAULT);

  if (!pci_map_int(config_id, en_intr, (void *) sc, &net_imask)) {
    printf("%s: couldn't establish interrupt\n", sc->sc_dev.dv_xname);
    return;
  }
  sc->ipl = 1; /* XXX */

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
    eni_get_macaddr(scp);
    sc->en_busreset = NULL;
    pci_conf_write(config_id, EN_TONGA, (TONGA_SWAP_DMA|TONGA_SWAP_WORD));
  }
#endif

  /*
   * done PCI specific stuff
   */

  en_attach(sc);

}

static void
en_pci_shutdown(
	void *sc,
	int howto)
{
    struct en_pci_softc *psc = (struct en_pci_softc *)sc;
    
    en_reset(&psc->esc);
    DELAY(10);
}

#if !defined(MIDWAY_ENIONLY)

#if defined(sparc) || defined(__FreeBSD__)
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
eni_get_macaddr(scp)
     struct en_pci_softc *scp;
{
  struct en_softc * sc = (struct en_softc *)scp;
  pcici_t id = scp->en_confid;
  int i, j, address, status;
  u_int32_t data, t_data;
  u_int8_t tmp;
  
  t_data = pci_conf_read(id, EN_TONGA) & 0xffffff00;

  data =  EN_PROM_MAGIC | EN_PROM_DATA | EN_PROM_CLK;
  pci_conf_write(id, EN_TONGA, data);

  for (i = 0; i < sizeof(sc->macaddr); i ++){
    /* start operation */
    data |= EN_PROM_DATA ;
    pci_conf_write(id, EN_TONGA, data);
    data |= EN_PROM_CLK ;
    pci_conf_write(id, EN_TONGA, data);
    data &= ~EN_PROM_DATA ;
    pci_conf_write(id, EN_TONGA, data);
    data &= ~EN_PROM_CLK ;
    pci_conf_write(id, EN_TONGA, data);
    /* send address with serial line */
    address = ((i + EN_ESI) << 1) + 1;
    for ( j = 7 ; j >= 0 ; j --){
      data = (address >> j) & 1 ? data | EN_PROM_DATA :
      data & ~EN_PROM_DATA;
      pci_conf_write(id, EN_TONGA, data);
      data |= EN_PROM_CLK ;
      pci_conf_write(id, EN_TONGA, data);
      data &= ~EN_PROM_CLK ;
      pci_conf_write(id, EN_TONGA, data);
    }
    /* get ack */
    data |= EN_PROM_DATA ;
    pci_conf_write(id, EN_TONGA, data);
    data |= EN_PROM_CLK ;
    pci_conf_write(id, EN_TONGA, data);
    data = pci_conf_read(id, EN_TONGA);
    status = data & EN_PROM_DATA;
    data &= ~EN_PROM_CLK ;
    pci_conf_write(id, EN_TONGA, data);
    data |= EN_PROM_DATA ;
    pci_conf_write(id, EN_TONGA, data);

    tmp = 0;

    for ( j = 7 ; j >= 0 ; j --){
      tmp <<= 1;
      data |= EN_PROM_DATA ;
      pci_conf_write(id, EN_TONGA, data);
      data |= EN_PROM_CLK ;
      pci_conf_write(id, EN_TONGA, data);
      data = pci_conf_read(id, EN_TONGA);
      if(data & EN_PROM_DATA) tmp |= 1;
      data &= ~EN_PROM_CLK ;
      pci_conf_write(id, EN_TONGA, data);
      data |= EN_PROM_DATA ;
      pci_conf_write(id, EN_TONGA, data);
    }
    /* get ack */
    data |= EN_PROM_DATA ;
    pci_conf_write(id, EN_TONGA, data);
    data |= EN_PROM_CLK ;
    pci_conf_write(id, EN_TONGA, data);
    data = pci_conf_read(id, EN_TONGA);
    status = data & EN_PROM_DATA;
    data &= ~EN_PROM_CLK ;
    pci_conf_write(id, EN_TONGA, data);
    data |= EN_PROM_DATA ;
    pci_conf_write(id, EN_TONGA, data);

    sc->macaddr[i] = tmp;
  }
  /* stop operation */
  data &=  ~EN_PROM_DATA;
  pci_conf_write(id, EN_TONGA, data);
  data |=  EN_PROM_CLK;
  pci_conf_write(id, EN_TONGA, data);
  data |=  EN_PROM_DATA;
  pci_conf_write(id, EN_TONGA, data);
  pci_conf_write(id, EN_TONGA, t_data);
}

#endif /* !MIDWAY_ADPONLY */
