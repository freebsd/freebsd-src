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
#include "pci.h"
#if (NEN > 0) && (NPCI > 0)

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#ifndef SHUTDOWN_PRE_SYNC
/*
 * device shutdown mechanism has been changed since 2.2-ALPHA.
 * if SHUTDOWN_PRE_SYNC is defined in "sys/systm.h", use new one.
 * otherwise, use old one.
 *	new: 2.2-ALPHA, 2.2-BETA, 2.2-GAMME, 2.2-RELEASE, 3.0
 *	old: 2.1.5, 2.1.6, 2.2-SNAP
 *			-- kjc
 */
#include <sys/devconf.h>
#endif
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <machine/cpufunc.h>		/* for rdtsc proto for clock.h below */
#include <machine/clock.h>		/* for DELAY */

#include <net/if.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <dev/en/midwayreg.h>
#include <dev/en/midwayvar.h>


/*
 * prototypes
 */

static	void en_pci_attach __P((pcici_t, int));
static	char *en_pci_probe __P((pcici_t, pcidi_t));
#ifdef SHUTDOWN_PRE_SYNC
static void en_pci_shutdown __P((int, void *));
#else
static	int en_pci_shutdown __P((struct kern_devconf *, int));
#endif

/*
 * local structures
 */

struct en_pci_softc {
  /* bus independent stuff */
  struct en_softc esc;		/* includes "device" structure */

  /* PCI bus glue */
  void *sc_ih;			/* interrupt handle */
  pci_chipset_tag_t en_pc;	/* for PCI calls */

};

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
#ifdef SHUTDOWN_PRE_SYNC
	NULL,
#else
	en_pci_shutdown,
#endif
};  

DATA_SET (pcidevice_set, endevice);

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

static char *en_pci_probe(config_id, device_id)

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
  sprintf(sc->sc_dev.dv_xname, "en%d", unit);
  sc->enif.if_unit = unit;
  sc->enif.if_name = "en";

  /*
   * figure out if we are an adaptec card or not.
   * XXX: why do we have to re-read PC_ID_REG when en_pci_probe already
   * had that info?
   */

  device_id = pci_conf_read(config_id, PCI_ID_REG);
  sc->is_adaptec = (PCI_VENDOR(device_id) == PCI_VENDOR_ADP) ? 1 : 0;
  
#ifdef SHUTDOWN_PRE_SYNC
  /*
   * Add shutdown hook so that DMA is disabled prior to reboot. Not
   * doing so could allow DMA to corrupt kernel memory during the
   * reboot before the driver initializes.
   */
  at_shutdown(en_pci_shutdown, scp, SHUTDOWN_POST_SYNC);
#endif

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
    sc->en_busreset = adp_busreset;
    adp_busreset(sc);
  }
#endif

#if !defined(MIDWAY_ADPONLY)
  if (!sc->is_adaptec) {
    sc->en_busreset = NULL;
    pci_conf_write(config_id, EN_TONGA, (TONGA_SWAP_DMA|TONGA_SWAP_WORD));
  }
#endif

  /*
   * done PCI specific stuff
   */

  en_attach(sc);

}

#ifdef SHUTDOWN_PRE_SYNC
static void
en_pci_shutdown(
	int howto,
	void *sc)
{
    struct en_pci_softc *psc = (struct en_pci_softc *)sc;
    
    en_reset(&psc->esc);
    DELAY(10);
}
#else  /* !SHUTDOWN_PRE_SYNC */
static int
en_pci_shutdown(kdc, force)

struct kern_devconf *kdc;
int force;

{
  if (kdc->kdc_unit < NEN) {
    struct en_pci_softc *psc = enpcis[kdc->kdc_unit];
    if (psc)			/* can it be null? */
      en_reset(&psc->esc);
    DELAY(10);
  }
  dev_detach(kdc);
  return(0);
}
#endif /* !SHUTDOWN_PRE_SYNC */
#endif /* NEN > 0 && NPCI > 0 */
