/*-
 * Copyright (c) 1998, 1999 Scott Mitchell
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
 *	$Id: if_xe.c,v 1.13 1999/01/24 22:14:54 root Exp $
 */

/*
 * Portions of this software were derived from Werner Koch's xirc2ps driver
 * for Linux under the terms of the following license (from v1.30 of the
 * xirc2ps driver):
 *
 * Copyright (c) 1997 by Werner Koch (dd9jn)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*		
 * FreeBSD device driver for Xircom CreditCard PCMCIA Ethernet adapters.
 * The following cards and media are supported (Ethernet part only on the
 * multifunction CEM cards):
 *   CE2	10BASE-2, 10BASE-T (I think)
 *   CEM28	ditto
 *   CEM33	ditto
 *   CE3	10BASE-T, 100BASE-TX
 *   CEM56	ditto
 * Certain Intel and Compaq branded cards are also rumoured to work.
 *
 * <Acknowledgements>
 *
 * <Contact details>
 */

#define XE_DEBUG 1

#include "xe.h"
#include "card.h"
#include "apm.h"
#include "bpfilter.h"

#if NXE > 0

#if NCARD > 0

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#if NBPFILTER > 0
#include <net/bpf.h>
#endif /* NBPFILTER > 0 */

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/if_xereg.h>
#include <machine/clock.h>
#if NAPM > 0
#include <machine/apm_bios.h>
#endif /* NAPM > 0 */

#include <pccard/cardinfo.h>
#include <pccard/cis.h>
#include <pccard/driver.h>
#include <pccard/slot.h>



/*
 * One of these structures per allocated device
 */
struct xe_softc {
  struct arpcom arpcom;
  struct ifmedia ifmedia;
  struct isa_device *dev;
  struct pccard_devinfo *crd;
  struct ifmib_iso_8802_3 mibdata;
#if NAPM > 0
  struct apmhook suspend_hook;
  struct apmhook resume_hook;
#endif /* NAPM > 0 */
  char *card_type;	/* Card model name */
  char *vendor;		/* Card manufacturer */
  int srev;     	/* Silicon revision */
  int modem;		/* 1 = Multifunction card with modem */
  int ce3;      	/* 1 = CE3 class (100Mbit) adapter */
  int cem56;    	/* 1 = CEM56 class (CE3 + 56Kbps modem) adapter */
  int gone;		/* 1 = Card bailed out */
  int tx_queued;	/* Packets currently waiting to transmit */
  int tx_ptr;		/* Last value of PTR reg on card */
  int tx_collisions;	/* Collisions since last successful send */
  int tx_timeouts;	/* Count of transmit timeouts */
  int probe;		/* XXX  0 = use value in port, 1 = look for a port */
  int port;		/* XXX  0 = Unknown, 1 = 10Base2, 2 = 10BaseT, 4 = 100BaseTX */
};

static struct xe_softc *sca[MAXSLOT];


/*
 * For accessing card registers
 */
#define XE_INB(r)         inb(scp->dev->id_iobase+(r))
#define XE_INW(r)         inw(scp->dev->id_iobase+(r))
#define XE_OUTB(r, b)     outb(scp->dev->id_iobase+(r), (b))
#define XE_OUTW(r, w)     outw(scp->dev->id_iobase+(r), (w))
#define XE_SELECT_PAGE(p) XE_OUTB(XE_PSR, (p))


/*
 * PC-Card driver routines
 */
static int  xe_card_init	(struct pccard_devinfo *devi);
static void xe_card_unload	(struct pccard_devinfo *devi);
static int  xe_card_intr	(struct pccard_devinfo *devi);

/*
 * isa_driver member functions
 */
static int  xe_probe	(struct isa_device *dev);
static int  xe_attach	(struct isa_device *dev);

/*
 * ifnet member functions
 */
static void xe_init	(void *xscp);
static void xe_start	(struct ifnet *ifp);
static int  xe_ioctl	(struct ifnet *ifp, u_long command, caddr_t data);
static void xe_watchdog	(struct ifnet *ifp);

/*
 * Other random functions
 */
static void xe_stop		(struct xe_softc *scp);
static void xe_reset		(struct xe_softc *scp);
static void xe_setmulti		(struct xe_softc *scp);
static void xe_setaddrs		(struct xe_softc *scp);
static int  xe_pio_write_packet	(struct xe_softc *scp, struct mbuf *mbp);
#ifdef XE_DEBUG
static void xe_reg_dump		(struct xe_softc *scp);
#endif

/*
 * Media selection functions
 */
static int  xe_media_change	(struct ifnet *ifp);
static void xe_media_status	(struct ifnet *ifp, struct ifmediareq *ifm);

/*
 * MII (Medium Independent Interface) functions
 */
static void      xe_mii_clock	(struct xe_softc *scp);
static u_int16_t xe_mii_getbit	(struct xe_softc *scp);
static void      xe_mii_putbit	(struct xe_softc *scp, u_int16_t data);
static void      xe_mii_putbits	(struct xe_softc *scp, u_int16_t data, int len);
static u_int16_t xe_mii_read	(struct xe_softc *scp, u_int8_t phy, u_int8_t reg);
static void      xe_mii_write	(struct xe_softc *scp, u_int8_t phy, u_int8_t reg, u_int16_t data, int len);
static int       xe_mii_init	(struct xe_softc *scp);
#ifdef XE_DEBUG
static void      xe_mii_dump	(struct xe_softc *scp);
#endif

#if NAPM > 0
/*
 * APM hook functions
 */
static int  xe_suspend	(void *xunit);
static int  xe_resume	(void *xunit);
#endif /* NAPM > 0 */


/*
 * PCMCIA driver hooks
 */
static struct pccard_device xe_info = {
	"xe",
	xe_card_init,
	xe_card_unload,
	xe_card_intr,
	0,
	&net_imask
};

DATA_SET(pccarddrv_set, xe_info);


/*
 * ISA driver hooks
 */
struct isa_driver xedriver = {
  xe_probe,
  xe_attach,
  "xe"
};



/*
 * All of the supported devices are PCMCIA cards.  I have no idea if it's even 
 * possible to successfully probe/attach these at boot time (pccardd normally
 * does a lot of setup work) so I don't even bother trying.
 */
static int
xe_probe (struct isa_device *dev) {
  bzero(sca, MAXSLOT * sizeof(sca[0]));
  return 0;
}


/*
 * Attach a device (called when xe_card_init succeeds).  Assume that the probe
 * routine has set up the softc structure correctly and that we can trust the
 * unit number.
 */
static int
xe_attach (struct isa_device *dev)
{
  struct ifnet *ifp;
  struct xe_softc *scp;
  int unit, i;

  unit = dev->id_unit;
  scp = sca[unit];
  ifp = &(scp->arpcom.ac_if);

  /*
   * Power down the interface
   */
  xe_stop(scp);

  /*
   * Initialise the ifnet structure
   */
  if (!ifp->if_name) {
    ifp->if_softc = scp;
    ifp->if_name = "xe";
    ifp->if_unit = unit;
    ifp->if_timer = 0;
    ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
    ifp->if_linkmib = &scp->mibdata;
    ifp->if_linkmiblen = sizeof scp->mibdata;
    ifp->if_output = ether_output;
    ifp->if_start = xe_start;
    ifp->if_ioctl = xe_ioctl;
    ifp->if_watchdog = xe_watchdog;
    ifp->if_init = xe_init;
    ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

    if_attach(ifp);
    ether_ifattach(ifp);
  }

#if NBPFILTER > 0
  /*
   * If BPF is in the kernel, call the attach for it
   */
  bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

  /*
   * Print some useful information
   */
  printf("\nxe%d: %s %s%s%s\n",
	 unit,
	 scp->vendor,
	 scp->card_type,
	 scp->ce3 ?   ", 100Mbps capable" : "",
	 scp->cem56 ? ", with modem"      : "");
  printf("xe%d: Ethernet address %02x", unit, scp->arpcom.ac_enaddr[0]);
  for (i = 1; i < ETHER_ADDR_LEN; i++) {
    printf(":%02x", scp->arpcom.ac_enaddr[i]);
  }
  printf("\n");

  return 1;
}


/*
 * Interrupt service routine.  This shouldn't ever get called, but if it does
 * we just call the card interrupt handler with the appropriate arguments.
 */
void
xeintr(int unit) {
  xe_card_intr(sca[unit]->crd);
}


/*
 * Initialize device.  Except for the media selection stuff this is pretty
 * much verbatim from the Linux code.
 */
static void
xe_init(void *xscp) {
  struct xe_softc *scp;
  struct ifnet *ifp;
  int unit, s;

  scp = xscp;
  ifp = &scp->arpcom.ac_if;
  unit = scp->crd->isahd.id_unit;

  if (scp->gone)
    return;

  if (TAILQ_EMPTY(&ifp->if_addrhead))
    return;

  s = splimp();

  /*
   * Reset transmitter flags
   */
  scp->tx_queued = 0;
  scp->tx_ptr = 0;
  scp->tx_collisions = 0;
  ifp->if_timer = 0;

  /*
   * Hard, then soft, reset the card.
   */
  XE_SELECT_PAGE(4);
  DELAY(1);
  XE_OUTB(XE_GPR1, 0);			/* Power off */
  DELAY(40000);
  if (scp->ce3)
    XE_OUTB(XE_GPR1, 1);		/* And back on again */
  else
    XE_OUTB(XE_GPR1, 5);		/* Also set AIC bit, whatever that is */
  DELAY(40000);
  XE_OUTB(XE_CR, XE_CR_SOFT_RESET);	/* Software reset */
  DELAY(40000);
  XE_OUTB(XE_CR, 0);
  DELAY(40000);

  if (scp->ce3) {
    /*
     * set GP1 and GP2 as outputs (bits 2 & 3)
     * set GP1 low to power on the ML6692 (bit 0)
     * set GP2 high to power on the 10Mhz chip (bit 1)
     */
    XE_SELECT_PAGE(4);
    XE_OUTB(XE_GPR0, 0x0e);
  }

  /*
   * Wait for everything to wake up.
   */
  DELAY(500000);

  /*
   * Get silicon revision number
   */
  if (scp->ce3)
    scp->srev = (XE_INB(XE_BOV) & 0x70) >> 4;
  else
    scp->srev = (XE_INB(XE_BOV) & 0x30) >> 4;
#ifdef XE_DEBUG
  printf("xe%d: silicon revision %d\n", unit, scp->srev);
#endif

  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/
  if (scp->probe) {
    if (!scp->ce3) {
      XE_SELECT_PAGE(4);
      XE_OUTB(XE_GPR0, 4);
      scp->probe = 0;
    }
  }
  else if (scp->port == 2) {
    /* select 10BaseT */
#ifdef XE_DEBUG
    printf("xe%d: selecting 10BaseT\n", unit);
#endif
    XE_SELECT_PAGE(0x42);
    XE_OUTB(XE_SWC1, 0x80);
  }
  else if (scp->port == 1) {
    /* select 10Base2 */
#ifdef XE_DEBUG
    printf("xe%d: selecting 10Base2\n", unit);
#endif
    XE_SELECT_PAGE(0x42);
    XE_OUTB(XE_SWC1, 0xc0);
  }
  DELAY(40000);
  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/

  /*
   * Setup the ECR
   */
  XE_SELECT_PAGE(1);
  XE_OUTB(XE_IMR0, 0xff);  /* Allow all interrupts */
  XE_OUTB(XE_IMR1, 0x01);  /* Enable Tx underrun detection */
  XE_SELECT_PAGE(0x42);
  XE_OUTB(XE_SWC0, 0x20);  /* Disable source insertion (WTF is that?) */

  /*
   * Set the 'local memory dividing line' -- splits the 32K card memory into
   * 8K for transmit buffers and 24K for receive.  This is done automatically
   * on newer revision cards.
   */
  if (scp->srev != 1) {
    XE_SELECT_PAGE(2);
    XE_OUTW(XE_RBS, 0x2000);
  }

  /*
   * Set up multicast addresses
   */
  xe_setmulti(scp);

  /*
   * Fix the data offset register -- reset leaves it off-by-one
   */
  XE_SELECT_PAGE(0);
  XE_OUTW(XE_DOR, 0x2000);

  /*
   * Set MAC interrupt masks and clear status regs.  The bit names are direct
   * from the Linux code; I have no idea what most of them do.
   */
  XE_SELECT_PAGE(0x40);		/* Bit 7..0 */
  XE_OUTB(XE_RXM0, 0xff);	/* ROK, RAB, rsv, RO,  CRC, AE,  PTL, MP  */
  XE_OUTB(XE_TXM0, 0xff);	/* TOK, TAB, SQE, LL,  TU,  JAB, EXC, CRS */
  XE_OUTB(XE_TXM1, 0xb0);	/* rsv, rsv, PTD, EXT, rsv, rsv, rsv, rsv */
  XE_OUTB(XE_RXS0, 0x00);	/* ROK, RAB, REN, RO,  CRC, AE,  PTL, MP  */
  XE_OUTB(XE_TXS0, 0x00);	/* TOK, TAB, SQE, LL,  TU,  JAB, EXC, CRS */
  XE_OUTB(XE_TXS1, 0x00);	/* TEN, rsv, PTD, EXT, retry_counter:4    */

  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/
  if (scp->ce3 && xe_mii_init(scp)) {
    u_char val;

#ifdef XE_DEBUG
    printf("xe%d: found an MII\n", unit);
#endif
    if ((scp->port == 4) || scp->cem56) {
      /* use MII */
#ifdef XE_DEBUG
      printf("xe%d: using MII\n", unit);
#endif
      XE_SELECT_PAGE(2);
      val = XE_INB(XE_MSR);
      val |= 0x08;
      XE_OUTB(XE_MSR, val);
      DELAY(20000);
    }
    else {
      XE_SELECT_PAGE(0x42);
      if (scp->port == 2) {
	/* enable 10BaseT */
#ifdef XE_DEBUG
	printf("xe%d: selecting 10BaseT\n", unit);
#endif
	XE_OUTB(XE_SWC1, 0x80);
      }
      else {
	/* enable 10Base2 */
#ifdef XE_DEBUG
	printf("xe%d: selecting 10Base2\n", unit);
#endif
	XE_OUTB(XE_SWC1, 0xc0);
      }
      DELAY(40000);
    }
  }
  else {
    XE_SELECT_PAGE(0);
    scp->port = (XE_INB(XE_ESR) & XE_ESR_MEDIA_SELECT) ? 2 : 1;
  }

  /*
   * Configure the LEDs
   */
  XE_SELECT_PAGE(2);
  if (scp->port > 1) {
    XE_OUTB(XE_LED, 0x3b);		/* For TP: link and activity */
  }
  else {
    XE_OUTB(XE_LED, 0x3a);		/* For BNC: !collision and activity */
  }
  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/
  /***** XXX XXX XXX XXX *****/

  /*
   * Enable receiver, put MAC online
   */
  XE_SELECT_PAGE(0x40);
  XE_OUTB(XE_OCR, XE_OCR_RX_ENABLE|XE_OCR_ONLINE);

  /*
   * Set up IMR, enable interrupts
   */
  XE_SELECT_PAGE(1);
  XE_OUTB(XE_IMR0, 0xff);		/* Enable everything */
  DELAY(1);
  XE_SELECT_PAGE(0);
  XE_OUTB(XE_CR, XE_CR_ENABLE_INTR);
  if (scp->modem && !scp->cem56) {	/* This bit is just magic */
    if (!(XE_INB(0x10) & 0x01)) {
      XE_OUTB(0x10, 0x11);		/* Unmask master int enable bit */
    }
  }

  XE_SELECT_PAGE(0);

  /*
   * Attempt to start output
   */
  ifp->if_flags |= IFF_RUNNING;
  ifp->if_flags &= ~IFF_OACTIVE;
  xe_start(ifp);

  (void)splx(s);
}


/*
 * Start output on interface.  We make two assumptions here:
 *  1) that the current priority is set to splimp _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
static void
xe_start(struct ifnet *ifp) {
  struct xe_softc *scp;
  struct mbuf *mbp;
  int unit;

  scp = ifp->if_softc;
  unit = scp->crd->isahd.id_unit;

  if (scp->gone)
    return;

  /*
   * Loop while there are packets to be sent, and space to send them.
   */
  while (1) {
    IF_DEQUEUE(&ifp->if_snd, mbp);	/* Suck a packet off the send queue */

    if (mbp == NULL) {
      /*
       * We are using the !OACTIVE flag to indicate to the outside world that
       * we can accept an additional packet rather than that the transmitter
       * is _actually_ active. Indeed, the transmitter may be active, but if
       * we haven't filled all the buffers with data then we still want to
       * accept more.
       */
      ifp->if_flags &= ~IFF_OACTIVE;
      return;
    }

    if (xe_pio_write_packet(scp, mbp) != 0) {
      IF_PREPEND(&ifp->if_snd, mbp);	/* Push the packet back onto the queue */
      ifp->if_flags |= IFF_OACTIVE;
      return;
    }

#if NBPFILTER > 0
    /*	
     * Tap off here if there is a bpf listener.
     */
    if (ifp->if_bpf) {
      bpf_mtap(ifp, mbp);
    }
#endif /* NBPFILTER > 0 */

    ifp->if_timer = 5;			/* In case we don't hear from the card again */
    scp->tx_queued++;

    m_freem(mbp);
  }
}


/*
 * Process an ioctl request.  Adapted from the ed driver.
 */
static int
xe_ioctl (register struct ifnet *ifp, u_long command, caddr_t data) {
  struct xe_softc *scp = ifp->if_softc;
  int s, error;

  scp = ifp->if_softc;
  error = 0;

  if (scp->gone) {
    return ENXIO;
  }

  s = splimp();

  switch (command) {

   case SIOCSIFADDR:
   case SIOCGIFADDR:
   case SIOCSIFMTU:
    error = ether_ioctl(ifp, command, data);
    break;

   case SIOCSIFFLAGS:
    if (ifp->if_flags & IFF_LINK2) {
      scp->port = 4;	/* 100BASE-TX */
      scp->probe = 0;
    }
    else if (ifp->if_flags & IFF_LINK1) {
      scp->port = 2;	/* 10BASE-T */
      scp->probe = 0;
    }
    else if (ifp->if_flags & IFF_LINK0) {
      scp->port = 1;	/* 10BASE-2 */
      scp->probe = 0;
    }
    else {
      scp->port = 0;	/* Unknown */
      scp->probe = 1;
    }

    /*
     * If the interface is marked up and stopped, then start it.  If it is
     * marked down and running, then stop it.
     */
    if (ifp->if_flags & IFF_UP) {
      if (!(ifp->if_flags & IFF_RUNNING))
	xe_init(scp);
    }
    else {
      if (ifp->if_flags & IFF_RUNNING)
	xe_stop(scp);
    }

   case SIOCADDMULTI:
   case SIOCDELMULTI:
    /*
     * Multicast list has (maybe) changed; set the hardware filter
     * accordingly.  This also serves to deal with promiscuous mode if we have 
     * a BPF listener active.
     */
    xe_setmulti(scp);
    error = 0;
    break;

   default:
    error = EINVAL;
  }

  (void)splx(s);

  return error;
}


/*
 * Device timeout/watchdog routine.  Called automatically if we queue a packet 
 * for transmission but don't get an interrupt within a specified timeout
 * (usually 5 seconds).  When this happens we assume the worst and reset the
 * card.
 */
static void
xe_watchdog(struct ifnet *ifp) {
  struct xe_softc *scp;
  int unit;

  scp  = ifp->if_softc;
  unit = scp->crd->isahd.id_unit;

  if (scp->gone)
    return;

  printf("xe%d: transmit timeout; resetting card\n", unit);
  scp->tx_timeouts++;
  ifp->if_oerrors += scp->tx_queued;
  xe_reset(scp);
}


/*
 * Take interface offline.  This is done by powering down the device, which I
 * assume means just shutting down the transceiver and Ethernet logic.  There
 * is probably a more elegant method that doesn't require a full reset to
 * recover from.
 */
static void
xe_stop(struct xe_softc *scp) {
  struct ifnet *ifp;

  ifp = &scp->arpcom.ac_if;

  if (scp->gone)
    return;

  XE_SELECT_PAGE(0);
  XE_OUTB(XE_CR, 0);   /* Disable interrupts */
  XE_SELECT_PAGE(1);
  XE_OUTB(XE_IMR0, 0); /* Forbid all interrupts */
  XE_SELECT_PAGE(4);
  XE_OUTB(XE_GPR1, 0); /* Power down (clear bit 0) */
  XE_SELECT_PAGE(0);

  ifp->if_flags &= ~IFF_RUNNING;
  ifp->if_flags &= ~IFF_OACTIVE;
  ifp->if_timer = 0;
}


/*
 * Reset the hardware.  Power-down the card then re-initialise it.  The
 * xe_stop() is redundant if xe_init() also does one, but it can't hurt.
 */
static void
xe_reset(struct xe_softc *scp) {
  int s;

  if (scp->gone)
    return;

  s = splimp();

  xe_stop(scp);
  xe_init(scp);

  (void)splx(s);
}


/*
 * Set up multicast filter and promiscuous mode
 */
static void
xe_setmulti(struct xe_softc *scp) {
  struct ifnet *ifp;
  struct ifmultiaddr *maddr;
  int count;

  ifp = &scp->arpcom.ac_if;
  maddr = ifp->if_multiaddrs.lh_first;

  /* Get length of multicast list */
  for (count = 0; maddr != NULL; maddr = maddr->ifma_link.le_next, count++);

  if ((ifp->if_flags & IFF_PROMISC) || (ifp->if_flags & IFF_ALLMULTI) || (count > 9)) {
    /*
     * Go into promiscuous mode if either of the PROMISC or ALLMULTI flags are
     * set, or if we have been asked to deal with more than 9 multicast
     * addresses.  To do this: set MPE and PME in SWC1
     */
    XE_SELECT_PAGE(0x42);
    XE_OUTB(XE_SWC1, 0x06);
  }
  else if ((ifp->if_flags & IFF_MULTICAST) && (count > 0)) {
    /*
     * Program the filters for up to 9 addresses
     */
    XE_SELECT_PAGE(0x42);
    XE_OUTB(XE_SWC1, 0);
    XE_SELECT_PAGE(0x40);
    XE_OUTB(XE_OCR, XE_OCR_OFFLINE);
    xe_setaddrs(scp);
    XE_SELECT_PAGE(0x40);
    XE_OUTB(XE_OCR, XE_OCR_RX_ENABLE|XE_OCR_ONLINE);
  }
  else {
    /*
     * No multicast operation (default)
     */
    XE_SELECT_PAGE(0x42);
    XE_OUTB(XE_SWC1, 0);
  }
  XE_SELECT_PAGE(0);
}


/*
 * Set up all on-chip addresses (for multicast).  AFAICS, there are 10
 * of these things; the first is our MAC address, the other 9 are mcast
 * addresses, padded with the MAC address if there aren't enough.
 * XXX - I think the Linux code gets this wrong and only writes one byte for
 * XXX - each address.  I *think* this code does it right, but it needs more
 * XXX - intensive testing to be sure.
 */
static void
xe_setaddrs(struct xe_softc *scp) {
  struct ifmultiaddr *maddr;
  u_int8_t *addr;
  u_int8_t page, slot, byte, i;

  maddr = scp->arpcom.ac_if.if_multiaddrs.lh_first;

  XE_SELECT_PAGE(page = 0x50);

  for (slot = 0, byte = 8; slot < 10; slot++) {

    if (slot == 0)
      addr = (u_int8_t *)(&scp->arpcom.ac_enaddr);
    else {
      while (maddr != NULL && maddr->ifma_addr->sa_family != AF_LINK)
	maddr = maddr->ifma_link.le_next;
      if (maddr != NULL)
	addr = LLADDR((struct sockaddr_dl *)maddr->ifma_addr);
      else
	addr = (u_int8_t *)(&scp->arpcom.ac_enaddr);
    }

    for (i = 0; i < 6; i++, byte++) {

      if (byte > 15) {
	page++;
	byte = 8;
	XE_SELECT_PAGE(page);
      }

      if (scp->ce3)
	XE_OUTB(byte, addr[5 - i]);
      else
	XE_OUTB(byte, addr[i]);
    }
  }
  XE_SELECT_PAGE(0);
}


/*
 * Write an outgoing packet to the card using programmed I/O.
 */
static int
xe_pio_write_packet(struct xe_softc *scp, struct mbuf *mbp) {
  struct mbuf *mbp2;
  u_int16_t len, pad, free, ok;
  u_int8_t *data;
  u_int8_t savebyte[2], wantbyte;

  /* Get total packet length */
  for (len = 0, mbp2 = mbp; mbp2 != NULL; len += mbp2->m_len, mbp2 = mbp2->m_next);

  /* Packets < minimum length may need to be padded out */
  pad = 0;
  if (len < ETHER_MIN_LEN - ETHER_CRC_LEN) {
    pad = (ETHER_MIN_LEN - ETHER_CRC_LEN - len + 1) >> 1;
    len = ETHER_MIN_LEN - ETHER_CRC_LEN;
  }

  /* Check transmit buffer space */
  XE_SELECT_PAGE(0);
  XE_OUTW(XE_TRS, len+2);
  free = XE_INW(XE_TSO);
  ok = free & 0x8000;
  free &= 0x7fff;
  if (free <= len + 2)
    return 1;

  /* Send packet length to card */
  XE_OUTW(XE_EDP, len);

  /*
   * Write packet to card using PIO (code stolen from the ed driver)
   */
  wantbyte = 0;
  while (mbp != NULL) {
    len = mbp->m_len;
    if (len > 0) {
      data = mtod(mbp, caddr_t);
      if (wantbyte) {		/* Finish the last word */
	savebyte[1] = *data;
	XE_OUTW(XE_EDP, *(u_short *)savebyte);
	data++;
	len--;
	wantbyte = 0;
      }
      if (len > 1) {		/* Output contiguous words */
	outsw(scp->dev->id_iobase+XE_EDP, data, len >> 1);
	data += len & ~1;
	len &= 1;
      }
      if (len == 1) {		/* Save last byte, if necessary */
	savebyte[0] = *data;
	wantbyte = 1;
      }
    }
    mbp = mbp->m_next;
  }
  if (wantbyte)			/* Last byte for odd-length packets */
    XE_OUTW(XE_EDP, *(u_short *)savebyte);

  /*
   * For CE3 cards, just tell 'em to send -- apparently the card will pad out
   * short packets with random cruft.  Otherwise, write nonsense words to fill 
   * out the packet.  I guess it is then sent automatically (?)
   */
  if (scp->ce3)
    XE_OUTB(XE_CR, XE_CR_TX_PACKET|XE_CR_ENABLE_INTR);
  else
    while (pad > 0)
      XE_OUTW(XE_EDP, 0xfeed);

  return 0;
}



/**************************************************************
 *                                                            *
 *                  M I I  F U N C T I O N S                  *
 *                                                            *
 **************************************************************/

#if 0
/*
 * Alternative MII/PHY handling code adapted from the xl driver.  It doesn't
 * seem to work any better than the xirc2_ps stuff, but it's cleaner code.
 * Will probably use this if I can ever get the autoneg to work right :(
 */
struct xe_mii_frame {
	u_int8_t		mii_stdelim;
	u_int8_t		mii_opcode;
	u_int8_t		mii_phyaddr;
	u_int8_t		mii_regaddr;
	u_int8_t		mii_turnaround;
	u_int16_t		mii_data;
};

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
xe_mii_sync(struct xe_softc *scp) {
  register int i;

  XE_SELECT_PAGE(2);
  XE_MII_SET(XE_MII_DIR|XE_MII_WRD);

  for (i = 0; i < 32; i++) {
    XE_MII_SET(XE_MII_CLK);
    DELAY(1);
    XE_MII_CLR(XE_MII_CLK);
    DELAY(1);
  }
}

/*
 * Clock a series of bits through the MII.
 */
static void
xe_mii_send(struct xe_softc *scp, u_int32_t bits, int cnt) {
  int i;

  XE_SELECT_PAGE(2);
  XE_MII_CLR(XE_MII_CLK);
  
  for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
    if (bits & i) {
      XE_MII_SET(XE_MII_WRD);
    } else {
      XE_MII_CLR(XE_MII_WRD);
    }
    DELAY(1);
    XE_MII_CLR(XE_MII_CLK);
    DELAY(1);
    XE_MII_SET(XE_MII_CLK);
  }
}

/*
 * Read an PHY register through the MII.
 */
static int
xe_mii_readreg(struct xe_softc *scp, struct xe_mii_frame *frame) {
  int i, ack, s;

  s = splimp();

  /*
   * Set up frame for RX.
   */
  frame->mii_stdelim = XE_MII_STARTDELIM;
  frame->mii_opcode = XE_MII_READOP;
  frame->mii_turnaround = 0;
  frame->mii_data = 0;
	
  XE_SELECT_PAGE(2);
  XE_OUTB(XE_GPR2, 0);

  /*
   * Turn on data xmit.
   */
  XE_MII_SET(XE_MII_DIR);

  xe_mii_sync(scp);

  /*	
   * Send command/address info.
   */
  xe_mii_send(scp, frame->mii_stdelim, 2);
  xe_mii_send(scp, frame->mii_opcode, 2);
  xe_mii_send(scp, frame->mii_phyaddr, 5);
  xe_mii_send(scp, frame->mii_regaddr, 5);

  /* Idle bit */
  XE_MII_CLR((XE_MII_CLK|XE_MII_WRD));
  DELAY(1);
  XE_MII_SET(XE_MII_CLK);
  DELAY(1);

  /* Turn off xmit. */
  XE_MII_CLR(XE_MII_DIR);

  /* Check for ack */
  XE_MII_CLR(XE_MII_CLK);
  DELAY(1);
  XE_MII_SET(XE_MII_CLK);
  DELAY(1);
  ack = XE_INB(XE_GPR2) & XE_MII_RDD;

  /*
   * Now try reading data bits. If the ack failed, we still
   * need to clock through 16 cycles to keep the PHY(s) in sync.
   */
  if (ack) {
    for(i = 0; i < 16; i++) {
      XE_MII_CLR(XE_MII_CLK);
      DELAY(1);
      XE_MII_SET(XE_MII_CLK);
      DELAY(1);
    }
    goto fail;
  }

  for (i = 0x8000; i; i >>= 1) {
    XE_MII_CLR(XE_MII_CLK);
    DELAY(1);
    if (!ack) {
      if (XE_INB(XE_GPR2) & XE_MII_RDD)
	frame->mii_data |= i;
      DELAY(1);
    }
    XE_MII_SET(XE_MII_CLK);
    DELAY(1);
  }

fail:

  XE_MII_CLR(XE_MII_CLK);
  DELAY(1);
  XE_MII_SET(XE_MII_CLK);
  DELAY(1);

  splx(s);

  if (ack)
    return(1);
  return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int
xe_mii_writereg(struct xe_softc *scp, struct xe_mii_frame *frame) {
  int s;

  s = splimp();

  /*
   * Set up frame for TX.
   */
  frame->mii_stdelim = XE_MII_STARTDELIM;
  frame->mii_opcode = XE_MII_WRITEOP;
  frame->mii_turnaround = XE_MII_TURNAROUND;
	
  XE_SELECT_PAGE(2);

  /*		
   * Turn on data output.
   */
  XE_MII_SET(XE_MII_DIR);

  xe_mii_sync(scp);

  xe_mii_send(scp, frame->mii_stdelim, 2);
  xe_mii_send(scp, frame->mii_opcode, 2);
  xe_mii_send(scp, frame->mii_phyaddr, 5);
  xe_mii_send(scp, frame->mii_regaddr, 5);
  xe_mii_send(scp, frame->mii_turnaround, 2);
  xe_mii_send(scp, frame->mii_data, 16);

  /* Idle bit. */
  XE_MII_SET(XE_MII_CLK);
  DELAY(1);
  XE_MII_CLR(XE_MII_CLK);
  DELAY(1);

  /*
   * Turn off xmit.
   */
  XE_MII_CLR(XE_MII_DIR);

  splx(s);

  return(0);
}

static u_int16_t
xe_phy_readreg(struct xe_softc *scp, u_int16_t reg) {
  struct xe_mii_frame frame;

  bzero((char *)&frame, sizeof(frame));

  frame.mii_phyaddr = 0;
  frame.mii_regaddr = reg;
  xe_mii_readreg(scp, &frame);

  return(frame.mii_data);
}

static void
xe_phy_writereg(struct xe_softc *scp, u_int16_t reg, u_int16_t data) {
  struct xe_mii_frame frame;

  bzero((char *)&frame, sizeof(frame));

  frame.mii_phyaddr = 0;
  frame.mii_regaddr = reg;
  frame.mii_data = data;
  xe_mii_writereg(scp, &frame);

  return;
}

/*
 * Initiate an autonegotiation session.
 */
static void
xe_autoneg_xmit(struct xe_softc *scp) {
  u_int16_t phy_sts;

  xe_phy_writereg(scp, PHY_BMCR, PHY_BMCR_RESET);
  DELAY(500);
  while(xe_phy_readreg(scp, PHY_BMCR) & PHY_BMCR_RESET);

  phy_sts = xe_phy_readreg(scp, PHY_BMCR);
  phy_sts &= ~PHY_BMCR_AUTONEGENBL;
  xe_phy_writereg(scp, PHY_BMCR, phy_sts);
  DELAY(1000);
  phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
  phy_sts &= ~PHY_BMCR_ISOLATE;
  xe_phy_writereg(scp, PHY_BMCR, phy_sts);
  DELAY(1000);

  return;
}

/*
 * Invoke autonegotiation on a PHY. Also used with the 3Com internal
 * autoneg logic which is mapped onto the MII.
 */
static void
xe_autoneg_mii(struct xe_softc *scp) {
  u_int16_t phy_sts = 0, media, advert, ability;
  int unit = scp->dev->id_unit;
  int i;

  /*
   * First, see if autoneg is supported. If not, there's
   * no point in continuing.
   */
  phy_sts = xe_phy_readreg(scp, PHY_BMSR);
  if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
    printf("xe%d: autonegotiation not supported\n", unit);
    media = xe_phy_readreg(scp, PHY_BMCR);
    media &= ~PHY_BMCR_SPEEDSEL;
    media &= ~PHY_BMCR_DUPLEX;
    xe_phy_writereg(scp, PHY_BMCR, media);
    return;
  }

  xe_autoneg_xmit(scp);
  DELAY(5000000);

  if (xe_phy_readreg(scp, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
    printf("xe%d: autoneg complete, ", unit);
    phy_sts = xe_phy_readreg(scp, PHY_BMSR);
  } else {
    printf("xe%d: autoneg not complete, ", unit);
  }

  media = xe_phy_readreg(scp, PHY_BMCR);

  /* Link is good. Report modes and set duplex mode. */
  if (xe_phy_readreg(scp, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
    printf("link status good ");
    advert = xe_phy_readreg(scp, XL_PHY_ANAR);
    ability = xe_phy_readreg(scp, XL_PHY_LPAR);

    if (advert & PHY_ANAR_100BT4 && ability & PHY_ANAR_100BT4) {
      printf("(100baseT4)\n");
    } else if (advert & PHY_ANAR_100BTXFULL && ability & PHY_ANAR_100BTXFULL) {
      printf("(full-duplex, 100Mbps)\n");
    } else if (advert & PHY_ANAR_100BTXHALF && ability & PHY_ANAR_100BTXHALF) {
      printf("(half-duplex, 100Mbps)\n");
    } else if (advert & PHY_ANAR_10BTFULL && ability & PHY_ANAR_10BTFULL) {
      printf("(full-duplex, 10Mbps)\n");
    } else if (advert & PHY_ANAR_10BTHALF && ability & PHY_ANAR_10BTHALF) {
      printf("(half-duplex, 10Mbps)\n");
    }
  } else {
    printf("no carrier (forcing half-duplex, 10Mbps)\n");
    xe_phy_writereg(scp, PHY_BMCR, 0x0000);
  }
}

static void
xe_mii_new_dump(struct xe_softc *scp) {
  int i, unit = scp->dev->id_unit;

  for (i = 0; i < 2; i++) {
    printf(" %d:%04x", i, xe_phy_readreg(scp, i));
  }
  for (i = 4; i < 7; i++) {
    printf(" %d:%04x", i, xe_phy_readreg(scp, i));
  }
  printf("\n");
}
#endif


#if 1
/*
 * Original MII/PHY code from xirc2_ps driver.  Seems like it should work,
 * according to the ML6692 and DP83840A specs, but the autonegotiation never
 * completes, even on the 10/100 net at work.  It's very weird.
 */
static void
xe_mii_clock(struct xe_softc *scp) {
  XE_OUTB(XE_GPR2, 0x04); /* MDCK low */
  DELAY(1);
  XE_OUTB(XE_GPR2, 0x05); /* MDCK high */
  DELAY(1);
}

static u_int16_t
xe_mii_getbit(struct xe_softc *scp) {
  u_int16_t data;

  XE_OUTB(XE_GPR2, 0x04);
  DELAY(1);
  data = XE_INB(XE_GPR2);
  XE_OUTB(XE_GPR2, 0x05);
  DELAY(1);
  return data & 0x20;
}

static void
xe_mii_putbit(struct xe_softc *scp, u_int16_t data) {
  if (data) {
    XE_OUTB(XE_GPR2, 0x0e);
    DELAY(1);
    XE_OUTB(XE_GPR2, 0x0f);
    DELAY(1);
  }
  else {
    XE_OUTB(XE_GPR2, 0x0c);
    DELAY(1);
    XE_OUTB(XE_GPR2, 0x0d);
    DELAY(1);
  }
}

static void
xe_mii_putbits(struct xe_softc *scp, u_int16_t data, int len) {
  u_int16_t mask;
  for (mask = 1 << (--len); mask != 0; mask >>= 1)
    xe_mii_putbit(scp, data & mask);
}

static u_int16_t
xe_mii_read(struct xe_softc *scp, u_int8_t phy, u_int8_t reg) {
  int i;
  u_int16_t mask, data = 0;

  XE_SELECT_PAGE(2);

  for (i = 0; i < 32; i++)
    xe_mii_putbit(scp, 1);

  xe_mii_putbits(scp, 0x06, 4);
  xe_mii_putbits(scp, phy, 5);
  xe_mii_putbits(scp, reg, 5);
  xe_mii_clock(scp);
  xe_mii_getbit(scp);

  for (mask = 1 << 15; mask != 0; mask >>= 1)
    if (xe_mii_getbit(scp))
      data |= mask;

  xe_mii_clock(scp);

  return data;
}

static void
xe_mii_write(struct xe_softc *scp, u_int8_t phy, u_int8_t reg, u_int16_t data, int len) {
  int i;

  XE_SELECT_PAGE(2);

  for (i = 0; i < 32; i++)
    xe_mii_putbit(scp, 1);

  xe_mii_putbits(scp, 0x05, 4);
  xe_mii_putbits(scp, phy, 5);
  xe_mii_putbits(scp, reg, 5);
  xe_mii_putbit(scp, 1);
  xe_mii_putbit(scp, 0);
  xe_mii_putbits(scp, data, len);
  xe_mii_clock(scp);
}

static int
xe_mii_init(struct xe_softc *scp) {
  u_int16_t control, status, partner;
  int unit = scp->dev->id_unit, i;

  status = xe_mii_read(scp, 0, 1);
  if ((status & 0xff00) != 0x7800) {
#ifdef XE_DEBUG
    printf("xe%d: no MII found, %0x\n", unit, status);
#endif
    return 0;
  }

  /*
   * XXX - do proper media selection here
   */
  if (scp->probe) {
#ifdef XE_DEBUG
    printf("xe%d: trying auto-negotiation\n", unit);
#endif
    control = 0x1000; /* AutoNeg */
  }
  else if (scp->port == 4) {
#ifdef XE_DEBUG
    printf("xe%d: defaulting to 100Mbps\n", unit);
#endif
    control = 0x2000; /* 100Mbps */
  }
  else {
#ifdef XE_DEBUG
    printf("xe%d: defaulting to 10Mbps\n", unit);
#endif
    control = 0x0000; /* 10Mbps */
  }

  xe_mii_write(scp, 0, 0, control, 16);
  DELAY(100);
  control = xe_mii_read(scp, 0, 0);

  if (control & 0x0400) {
#ifdef XE_DEBUG
    printf("xe%d: can't take PHY out of isolation mode\n", unit);
#endif
    return 0;
  }

  if (scp->probe) {
    /* Wait for negotiation to finish */
#ifdef XE_DEBUG
    printf("xe%d: waiting for auto-negotiation to complete...\n", unit);
#endif
    for (i = 0; i < 35; i++) {
      DELAY(100000);
      status = xe_mii_read(scp, 0, 1);
      if ((status & 0x0020) && (status &0x0004))
	break;
    }

    if (!(status & 0x0020)) {
#ifdef XE_DEBUG
      printf("xe%d: auto-negotiation failed\n", unit);
#endif
      control = 0x0000;
      xe_mii_write(scp, 0, 0, control, 16);
      DELAY(100);
      XE_SELECT_PAGE(0);
      scp->port = (XE_INB(XE_ESR) & XE_ESR_MEDIA_SELECT) ? 2 : 1;
    }
    else {
      partner = xe_mii_read(scp, 0, 5);
#ifdef XE_DEBUG
      printf("xe%d: MII link partner = %04x\n", unit, partner);
#endif
      if (partner & 0x0080) {
	scp->port = 4;
      }
      else {
	DELAY(100);
	XE_SELECT_PAGE(0);
	scp->port = (XE_INB(XE_ESR) & XE_ESR_MEDIA_SELECT) ? 2 : 1;
      }
    }
    scp->probe = 0;
  }

#ifdef XE_DEBUG
  printf("xe%d: auto-negotiation result: ", unit);
  switch (scp->port) {
   case 4: printf("100BaseTX\n"); break;
   case 2: printf("10BaseT\n"); break;
   case 1: printf("10Base2\n"); break;
   default: printf("unknown\n");
  }
#endif

  return 1;
}

#ifdef XE_DEBUG
static void
xe_mii_dump(struct xe_softc *scp) {
  int i, unit = scp->dev->id_unit;

  printf("xe%d: MII register dump\n", unit);
  for (i = 0; i < 7; i++) {
    printf(" %04x", xe_mii_read(scp, 0, i));
  }
  printf("\n");
}

static void
xe_reg_dump(struct xe_softc *scp) {
  int unit = scp->dev->id_unit;
  int page, i;

  printf("xe%d: Common registers: ", unit);
  for (i = 0; i < 8; i++) {
    printf(" %2.2x", XE_INB(i));
  }
  printf("\n");

  for (page = 0; page < 8; page++) {
    printf("xe%d: Register page %2.2x: ", unit, page);
    XE_SELECT_PAGE(page);
    for (i = 8; i < 16; i++) {
      printf(" %2.2x", XE_INB(i));
    }
    printf("\n");
  }

  for (page = 0x40; page < 0x5f; page++) {
    if (page==0x43 || (page>=0x46 && page<=0x4f) || (page>=0x51 && page<=0x5e))
      continue;
    printf("xe%d: Register page %2.2x: ", unit, page);
    XE_SELECT_PAGE(page);
    for (i = 8; i < 16; i++) {
      printf(" %2.2x", XE_INB(i));
    }
    printf("\n");
  }
}
#endif
#endif


/**************************************************************
 *                                                            *
 *               P C M C I A  F U N C T I O N S               *
 *                                                            *
 **************************************************************/

#define CARD_MAJOR  50

/*
 * Horrid stuff for accessing CIS tuples
 */
#define CISTPL_BUFSIZE 512
#define CISTPL_TYPE(tpl)     tpl[0]
#define CISTPL_LEN(tpl)      tpl[2]
#define CISTPL_DATA(tpl,pos) tpl[4 + ((pos)<<1)]

	
/*
 * Probe and identify the device.  Called by the slot manager when the card is 
 * inserted or the machine wakes up from suspend mode.  Assmes that the slot
 * structure has been initialised already.
 */
static int
xe_card_init(struct pccard_devinfo *devi)
{
  struct xe_softc *scp;
  struct isa_device *dev;
  struct uio uios;
  struct iovec iov;
  u_char buf[CISTPL_BUFSIZE];
  u_char ver_str[CISTPL_BUFSIZE>>1];
  off_t offs;
  int unit, success, rc, i;

  unit = devi->isahd.id_unit;
  scp = sca[unit];
  dev = &devi->isahd;
  success = 0;

#ifdef XE_DEBUG
  printf("xe: Probing for unit %d\n", unit);
#endif

  /* Check that unit number is OK */
  if (unit > MAXSLOT) {
    printf("xe: bad unit (%d)\n", unit);
    return (ENODEV);
  }

  /* Don't attach an active device */
  if (scp && !scp->gone) {
    printf("xe: unit already attached (%d)\n", unit);
    return (EBUSY);
  }

  /* Allocate per-instance storage */
  if (!scp) {
    if ((scp = malloc(sizeof(*scp), M_DEVBUF, M_NOWAIT)) == NULL) {
      printf("xe%d: failed to allocage driver strorage\n", unit);
      return (ENOMEM);
    }
    bzero(scp, sizeof(*scp));
  }

  /* Re-attach an existing device */
  if (scp->gone) {
    scp->gone = 0;
    xe_stop(scp);
    return 0;
  }

  /* Grep through CIS looking for relevant tuples */
  offs = 0;
  do {
    u_int16_t vendor;
    u_int8_t rev, media, prod;

    iov.iov_base = buf;
    iov.iov_len = CISTPL_BUFSIZE;
    uios.uio_iov = &iov;
    uios.uio_iovcnt = 1;
    uios.uio_offset = offs;
    uios.uio_resid = CISTPL_BUFSIZE;
    uios.uio_segflg = UIO_SYSSPACE;
    uios.uio_rw = UIO_READ;
    uios.uio_procp = 0;

    /*
     * Read tuples one at a time into buf.  Sucks, but it only happens once.
     * XXX - If the stuff we need isn't in attribute memory, or (worse yet)
     * XXX - attribute memory isn't mapped, we're FUBAR.  Maybe need to do an
     * XXX - ioctl on the card device and follow links?
     */
    if ((rc = cdevsw[CARD_MAJOR]->d_read(makedev(CARD_MAJOR, devi->slt->slotnum), &uios, 0)) == 0) {

      switch (CISTPL_TYPE(buf)) {

       case 0x15:	/* Grab version string (needed to ID some weird CE2's) */
#ifdef XE_DEBUG
	printf("xe%d: Got version string (0x15)\n", unit);
#endif
	for (i = 0; i < CISTPL_LEN(buf); ver_str[i] = CISTPL_DATA(buf, i++));
	ver_str[i] = '\0';
	ver_str[CISTPL_BUFSIZE>>1 - 1] = CISTPL_LEN(buf);
	success++;
	break;

       case 0x20:	/* Figure out what type of card we have */
#ifdef XE_DEBUG
	printf("xe%d: Got card ID (0x20)\n", unit);
#endif
	vendor = CISTPL_DATA(buf, 0) + (CISTPL_DATA(buf, 1) << 8);
	rev = CISTPL_DATA(buf, 2);
	media = CISTPL_DATA(buf, 3);
	prod = CISTPL_DATA(buf, 4);

	switch (vendor) {	/* Get vendor ID */
	 case 0x0105:
	  scp->vendor = "Xircom"; break;
	 case 0x0138:
	 case 0x0183:
	  scp->vendor = "Compaq"; break;
	 case 0x0089:
	  scp->vendor = "Intel"; break;
	 default:
	  scp->vendor = "Unknown";
	}

	if (!((prod & 0x40) && (media & 0x01))) {
#ifdef XE_DEBUG
	printf("xe%d: Not a PCMCIA Ethernet card!\n", unit);
#endif
	  rc = ENODEV;		/* Not a PCMCIA Ethernet device */
	}
	else {
	  if (media & 0x10) {	/* Ethernet/modem cards */
#ifdef XE_DEBUG
	printf("xe%d: Card is Ethernet/modem combo\n", unit);
#endif
	    scp->modem = 1;
	    switch (prod & 0x0f) {
	     case 1:
	      scp->card_type = "CEM"; break;
	     case 2:
	      scp->card_type = "CEM2"; break;
	     case 3:
	      scp->card_type = "CEM3"; break;
	     case 4:
	      scp->card_type = "CEM33"; break;
	     case 5:
	      scp->ce3 = 1;
	      scp->card_type = "CEM56M"; break;
	     case 6:
	      scp->ce3 = 1;
	      scp->cem56 = 1;
	      scp->card_type = "CEM56"; break;
	     default:
	      rc = ENODEV;
	    }
	  }
	  else {		/* Ethernet-only cards */
#ifdef XE_DEBUG
	printf("xe%d: Card is Ethernet only\n", unit);
#endif
	    switch (prod & 0x0f) {
	     case 1:
	      scp->card_type = "CE"; break;
	     case 2:
	      scp->card_type = "CE2"; break;
	     case 3:
	      scp->ce3 = 1;
	      scp->card_type = "CE3"; break;
	     default:
	      rc = ENODEV;
	    }
	  }
	}
	success++;
	break;

       case 0x22:	/* Get MAC address */
#ifdef XE_DEBUG
	printf("xe%d: Got MAC address (0x22)\n", unit);
#endif
	if ((CISTPL_LEN(buf) == 8) &&
	    (CISTPL_DATA(buf, 0) == 0x04) &&
	    (CISTPL_DATA(buf, 1) == ETHER_ADDR_LEN)) {
	  for (i = 0; i < ETHER_ADDR_LEN; scp->arpcom.ac_enaddr[i] = CISTPL_DATA(buf, i+2), i++);
	}
	success++;
	break;
       default:
      }
    }

    /* Skip to next tuple */
    offs += ((CISTPL_LEN(buf) + 2) << 1);

  } while ((CISTPL_TYPE(buf) != 0xff) && (CISTPL_LEN(buf) != 0xff) && (rc == 0));


  /* Die now if something went wrong above */
  if ((rc != 0) || (success < 3)) {
    free(scp, M_DEVBUF);
    return rc;
  }

  /* Check for certain strange CE2's that look like CE's */
  if (strcmp(scp->card_type, "CE") == 0) {
    u_char len = ver_str[CISTPL_BUFSIZE>>1 - 1];
#ifdef XE_DEBUG
	printf("xe%d: Checking for weird CE2 string\n", unit);
#endif
    for (i = 0; i < len - 2; i++)
      if (bcmp("CE2", &ver_str[i], 3) == 0)
	scp->card_type = "CE2";
  }

  /* Fill in some private data */
  sca[unit] = scp;
  scp->dev = &devi->isahd;
  scp->crd = devi;
  scp->probe = 1;	/* Do media auto-detect by default */

  /* Attempt to attach the device */
#ifdef XE_DEBUG
	printf("xe%d: Attaching...\n", unit);
#endif
  if (xe_attach(scp->dev) == 0) {
    sca[unit] = 0;
    free(scp, M_DEVBUF);
    return ENXIO;
  }

#if NAPM > 0
  /* Establish APM hooks once device attached */
  scp->suspend_hook.ah_name = "xe_suspend";
  scp->suspend_hook.ah_fun = xe_suspend;
  scp->suspend_hook.ah_arg = (void *)unit;
  scp->suspend_hook.ah_order = APM_MIN_ORDER;
  apm_hook_establish(APM_HOOK_SUSPEND, &scp->suspend_hook);
  scp->resume_hook.ah_name = "xe_resume";
  scp->resume_hook.ah_fun = xe_resume;
  scp->resume_hook.ah_arg = (void *)unit;
  scp->resume_hook.ah_order = APM_MIN_ORDER;
  apm_hook_establish(APM_HOOK_RESUME, &scp->resume_hook);
#endif /* NAPM > 0 */

  /* Success */
  return 0;
}

/*
 * The device entry is being removed, probably because someone ejected the
 * card.  The interface should have been brought down manually before calling
 * this function; if not you may well lose packets.  In any case, I shut down
 * the card and the interface, and hope for the best.  The 'gone' flag is set, 
 * so hopefully no-one else will try to access the missing card.
 */
static void
xe_card_unload(struct pccard_devinfo *devi) {
  struct xe_softc *scp;
  struct ifnet *ifp;
  int unit;

  unit = devi->isahd.id_unit;
  scp = sca[unit];
  ifp = &scp->arpcom.ac_if;

  if (scp->gone) {
    printf("xe%d: already unloaded\n", unit);
    return;
  }

  if_down(ifp);
  ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
  xe_stop(scp);
  scp->gone = 1;
}


/*
 * Card interrupt handler: should return true if the interrupt was for us, in
 * case we are sharing our IRQ line with other devices (this will probably be
 * the case for multifunction cards).
 *
 * This function is probably more complicated than it needs to be, as it
 * attempts to deal with the case where multiple packets get sent between
 * interrupts.  This is especially annoying when working out the collision
 * stats.  Not sure whether this case ever really happens or not (maybe on a
 * slow/heavily loaded machine?) so it's probably best to leave this like it
 * is.
 *
 * Note that the crappy PIO used to get packets on and off the card means that 
 * you will spend a lot of time in this routine -- I can get my P150 to spend
 * 90% of its time servicing interrupts if I really hammer the network.  Could 
 * fix this, but then you'd start dropping/losing packets.  The moral of this
 * story?  If you want good network performance _and_ some cycles left over to 
 * get your work done, don't buy a Xircom card.  Or convince them to tell me
 * how to do memory-mapped I/O :)
 */
static int
xe_card_intr(struct pccard_devinfo *devi) {
  struct xe_softc *scp;
  struct ifnet *ifp;
  int unit, result;
  u_int16_t rx_bytes, rxs, txs;
  u_int8_t psr, isr, esr, rsr;

  unit = devi->isahd.id_unit;
  scp = sca[unit];
  ifp = &scp->arpcom.ac_if;
  rx_bytes = 0;			/* Bytes received on this interrupt */
  result = 0;			/* Set true if the interrupt is for us */

  if (scp->gone)
    return 0;

  if (scp->ce3) {
    XE_OUTB(XE_CR, 0);		/* Disable interrupts */
  }

  psr = XE_INB(XE_PSR);		/* Stash the current register page */

  /*
   * Read ISR to see what caused this interrupt.  Note that this clears the
   * ISR on CE2 type cards.
   */
  if ((isr = XE_INB(XE_ISR)) && isr != 0xff) {

    result = 1;			/* This device did generate an int */
    esr = XE_INB(XE_ESR);	/* Read the other status registers */
    XE_SELECT_PAGE(0x40);
    rxs = XE_INB(XE_RXS0);
    XE_OUTB(XE_RXS0, ~rxs & 0xff);
    txs = XE_INB(XE_TXS0);
    txs |= XE_INB(XE_TXS1) << 8;
    XE_OUTB(XE_TXS0, 0);
    XE_OUTB(XE_TXS1, 0);
    XE_SELECT_PAGE(0);

#if XE_DEBUG > 3
    printf("xe%d: ISR=%#2.2x ESR=%#2.2x RXS=%#2.2x TXS=%#4.4x\n", unit, isr, esr, rxs, txs);
#endif

    /*
     * Handle transmit interrupts
     */
    if (isr & XE_ISR_TX_PACKET) {
      u_int8_t new_ptr, sent;
      
      if ((new_ptr = XE_INB(XE_PTR)) < scp->tx_ptr)	/* Update packet count */
	sent = (0xff - scp->tx_ptr) + new_ptr;		/* PTR rolled over */
      else
	sent = new_ptr - scp->tx_ptr;

      if (sent > 0) {				/* Packets sent since last interrupt */
	scp->tx_ptr = new_ptr;
	scp->tx_queued -= sent;
	ifp->if_opackets += sent;
	ifp->if_collisions += scp->tx_collisions;

	/*
	 * Collision stats are a PITA.  If multiples frames have been sent, we 
	 * distribute any outstanding collision count equally amongst them.
	 * However, if we're missing interrupts we're quite likely to also
	 * miss some collisions; thus the total count will be off anyway.
	 * Likewise, if we miss a frame dropped due to excessive collisions
	 * any outstanding collisions count will be held against the next
	 * frame to be successfully sent.  Hopefully it averages out in the
	 * end!
	 * XXX - This will screw up if tx_collisions/sent > 14.  FIX IT!
	 */
	switch (scp->tx_collisions) {
	 case 0:
	  break;
	 case 1:
	  scp->mibdata.dot3StatsSingleCollisionFrames++;
	  scp->mibdata.dot3StatsCollFrequencies[0]++;
	  break;
	 default:
	  if (sent == 1) {
	    scp->mibdata.dot3StatsMultipleCollisionFrames++;
	    scp->mibdata.dot3StatsCollFrequencies[scp->tx_collisions-1]++;
	  }
	  else {		/* Distribute across multiple frames */
	    scp->mibdata.dot3StatsMultipleCollisionFrames += sent;
	    scp->mibdata.
	      dot3StatsCollFrequencies[scp->tx_collisions/sent] += sent - scp->tx_collisions%sent;
	    scp->mibdata.
	      dot3StatsCollFrequencies[scp->tx_collisions/sent + 1] += scp->tx_collisions%sent;
	  }
	}
	scp->tx_collisions = 0;
      }
      ifp->if_timer = 0;
      ifp->if_flags &= ~IFF_OACTIVE;
    }
    if (txs & 0x0002) {		/* Excessive collisions (packet dropped) */
      ifp->if_collisions += 16;
      ifp->if_oerrors++;
      scp->tx_collisions = 0;
      scp->mibdata.dot3StatsExcessiveCollisions++;
      scp->mibdata.dot3StatsMultipleCollisionFrames++;
      scp->mibdata.dot3StatsCollFrequencies[15]++;
      XE_OUTB(XE_CR, XE_CR_RESTART_TX);
    }
    if (txs & 0x0040)		/* Transmit aborted -- probably collisions */
      scp->tx_collisions++;


    /*
     * Handle receive interrupts 
     */
    while ((esr = XE_INB(XE_ESR)) & XE_ESR_FULL_PKT_RX) {

      if ((rsr = XE_INB(XE_RSR)) & XE_RSR_RX_OK) {
	struct ether_header *ehp;
	struct mbuf *mbp;
	u_int16_t len;

	len = XE_INW(XE_RBC);

	if (len == 0)
	  continue;

#if 0
	/*
	 * Limit the amount of time we spend in this loop, dropping packets if 
	 * necessary.  The Linux code does this with considerably more
	 * finesse, adjusting the threshold dynamically.
	 */
	if ((rx_bytes += len) > 22000) {
	  ifp->if_iqdrops++;
	  scp->mibData.dot3StatsMissedFrames++;
	  XE_OUTW(XE_DOR, 0x8000);
	  continue;
	}
#endif

	if (len & 0x01)
	  len++;

	MGETHDR(mbp, M_DONTWAIT, MT_DATA);	/* Allocate a header mbuf */
	if (mbp != NULL) {
	  mbp->m_pkthdr.rcvif = ifp;
	  mbp->m_pkthdr.len = mbp->m_len = len;

	  /*
	   * If the mbuf header isn't big enough for the packet, attach an
	   * mbuf cluster to hold it.  The +2 is to allow for the nasty little 
	   * alignment hack below.
	   */
	  if (len + 2 > MHLEN) {
	    MCLGET(mbp, M_DONTWAIT);
	    if ((mbp->m_flags & M_EXT) == 0) {
	      m_freem(mbp);
	      mbp = NULL;
	    }
	  }
	}

	if (mbp != NULL) {
	  /*
	   * The Ethernet header is 14 bytes long; thus the actual packet data 
	   * won't be 32-bit aligned when it's dumped into the mbuf.  We
	   * offset everything by 2 bytes to fix this.  Apparently the
	   * alignment is important for NFS, damn its eyes.
	   */
	  mbp->m_data += 2;
	  ehp = mtod(mbp, struct ether_header *);

	  /*
	   * Now get the packet, including the Ethernet header and trailer (?)
	   * We use programmed I/O, because we don't know how to do shared
	   * memory with these cards.  So yes, it's real slow, and heavy on
	   * the interrupts (CPU on my P150 maxed out at ~950KBps incoming).
	   */
	  if (scp->srev == 0) {		/* Workaround a bug in old cards */
	    u_short rhs;

	    XE_SELECT_PAGE(5);
	    rhs = XE_INW(XE_RHS);
	    XE_SELECT_PAGE(0);

	    rhs += 3;			 /* Skip control info */

	    if (rhs >= 0x8000)
	      rhs = 0;

	    if (rhs + len > 0x8000) {
	      int i;

	      /*
	       * XXX - this i-- seems very wrong, but it's what the Linux guys 
	       * XXX - do.  Need someone with an old CE2 to test this for me.
	       */
	      for (i = 0; i < len; i--, rhs++) {
		((char *)ehp)[i] = XE_INB(XE_EDP);
		if (rhs = 0x8000) {
		  rhs = 0;
		  i--;
		}
	      }
	    }
	    else
	      insw(scp->dev->id_iobase+XE_EDP, ehp, len >> 1);
	  }
	  else
	    insw(scp->dev->id_iobase+XE_EDP, ehp, len >> 1);

#if NBPFILTER > 0
	  /*
	   * Check if there's a BPF listener on this interface. If so, hand
	   * off the raw packet to bpf.
	   */
	  if (ifp->if_bpf) {
	    bpf_mtap(ifp, mbp);

	    /*	
	     * Note that the interface cannot be in promiscuous mode if there
	     * are no BPF listeners.  And if we are in promiscuous mode, we
	     * have to check if this packet is really ours.
	     */
	    if ((ifp->if_flags & IFF_PROMISC) &&
		bcmp(ehp->ether_dhost, scp->arpcom.ac_enaddr, sizeof(ehp->ether_dhost)) != 0 &&
		(rsr & XE_RSR_PHYS_PKT)) {
	      m_freem(mbp);
	      mbp = NULL;
	    }
	  }
#endif /* NBPFILTER > 0 */

	  if (mbp != NULL) {
	    mbp->m_pkthdr.len = mbp->m_len = len - ETHER_HDR_LEN;
	    mbp->m_data += ETHER_HDR_LEN;	/* Strip off Ethernet header */
	    ether_input(ifp, ehp, mbp);		/* Send the packet on its way */
	    ifp->if_ipackets++;			/* Success! */
	    XE_OUTW(XE_DOR, 0x8000);		/* skip_rx_packet command */
	  }
	}
      }
      else if (rsr & XE_RSR_LONG_PKT) {		/* Packet length >1518 bytes */
	scp->mibdata.dot3StatsFrameTooLongs++;
	ifp->if_ierrors++;
      }
      else if (rsr & XE_RSR_CRC_ERR) {		/* Bad checksum on packet */
	scp->mibdata.dot3StatsFCSErrors++;
	ifp->if_ierrors++;
      }
      else if (rsr & XE_RSR_ALIGN_ERR) {	/* Packet alignment error */
	scp->mibdata.dot3StatsAlignmentErrors++;
	ifp->if_ierrors++;
      }
    }
    if (rxs & 0x10) {				/* Receiver overrun */
      scp->mibdata.dot3StatsInternalMacReceiveErrors++;
      ifp->if_ierrors++;
      XE_OUTB(XE_CR, XE_CR_CLEAR_OVERRUN);
    }
  }

  XE_SELECT_PAGE(psr);				/* Restore saved page */
  XE_OUTB(XE_CR, XE_CR_ENABLE_INTR);		/* Re-enable interrupts */

  /* XXX - force an int here, instead of dropping packets?     */
  /* XXX - XE_OUTB(XE_CR, XE_CR_ENABLE_INTR|XE_CE_FORCE_INTR); */

  return result;
}



#if NAPM > 0
/**************************************************************
 *                                                            *
 *                  A P M  F U N C T I O N S                  *
 *                                                            *
 **************************************************************/

/*
 * This is called when we go into suspend/standby mode
 */
static int
xe_suspend(void *xunit) {
  struct xe_softc *scp;
  struct ifnet *ifp;
  int unit;

  unit = (int)xunit;
  scp = sca[unit];
  ifp = &scp->arpcom.ac_if;

#ifdef XE_DEBUG
  printf("xe%d: APM suspend\n", unit);
#endif
  if_down(ifp);
  return 0;
}

/*
 * This is called when we wake up again
 */
static int
xe_resume(void *xunit) {
  struct xe_softc *scp;
  int unit;

  unit = (int)xunit;
  scp = sca[unit];

#ifdef XE_DEBUG
  printf("xe%d: APM resume\n", unit);
#endif
  return 0;
}
#endif /* NAPM > 0 */


#endif /* NCARD > 0 */


#endif /* NXE > 0 */
