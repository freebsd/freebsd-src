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
 *	$Id: if_xe.c,v 1.20 1999/06/13 19:17:40 scott Exp $
 * $FreeBSD$
 */

/*
 * XXX TODO XXX
 *
 * I've pushed this fairly far, but there are some things that need to be
 * done here.  I'm documenting them here in case I get destracted. -- imp
 *
 * xe_cem56fix -- need to figure out how to map the extra stuff.
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
 * FreeBSD device driver for Xircom CreditCard PCMCIA Ethernet adapters.  The
 * following cards are currently known to work with the driver:
 *   Xircom CreditCard 10/100 (CE3)
 *   Xircom CreditCard Ethernet + Modem 28 (CEM28)
 *   Xircom CreditCard Ethernet 10/100 + Modem 56 (CEM56)
 *   Xircom RealPort Ethernet 10
 *   Xircom RealPort Ethernet 10/100
 *   Xircom RealPort Ethernet 10/100 + Modem 56 (REM56, REM56G)
 *   Intel EtherExpress Pro/100 PC Card Mobile Adapter 16 (Pro/100 M16A)
 *   Compaq Netelligent 10/100 PC Card (CPQ-10/100)
 *
 * Some other cards *should* work, but support for them is either broken or in 
 * an unknown state at the moment.  I'm always interested in hearing from
 * people who own any of these cards:
 *   Xircom CreditCard 10Base-T (PS-CE2-10)
 *   Xircom CreditCard Ethernet + ModemII (CEM2)
 *   Xircom CEM28 and CEM33 Ethernet/Modem cards (may be variants of CEM2?)
 *
 * Thanks to all who assisted with the development and testing of the driver,
 * especially: Werner Koch, Duke Kamstra, Duncan Barclay, Jason George, Dru
 * Nelson, Mike Kephart, Bill Rainey and Douglas Rand.  Apologies if I've left
 * out anyone who deserves a mention here.
 *
 * Special thanks to Ade Lovett for both hosting the mailing list and doing
 * the CEM56/REM56 support code; and the FreeBSD UK Users' Group for hosting
 * the web pages.
 *
 * Contact points:
 *
 * Driver web page: http://ukug.uk.freebsd.org/~scott/xe_drv/
 *
 * Mailing list: http://www.lovett.com/lists/freebsd-xircom/
 * or send "subscribe freebsd-xircom" to <majordomo@lovett.com>
 *
 * Author email: <scott@uk.freebsd.org>
 */


#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
 
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <net/bpf.h>

#include <dev/xe/if_xereg.h>
#include <dev/xe/if_xevar.h>

/*
 * MII command structure
 */
struct xe_mii_frame {
  u_int8_t  mii_stdelim;
  u_int8_t  mii_opcode;
  u_int8_t  mii_phyaddr;
  u_int8_t  mii_regaddr;
  u_int8_t  mii_turnaround;
  u_int16_t mii_data;
};

/*
 * Media autonegotiation progress constants
 */
#define XE_AUTONEG_NONE		0	/* No autonegotiation in progress */
#define XE_AUTONEG_WAITING	1	/* Waiting for transmitter to go idle */
#define XE_AUTONEG_STARTED	2	/* Waiting for autonegotiation to complete */
#define XE_AUTONEG_100TX	3	/* Trying to force 100baseTX link */
#define XE_AUTONEG_FAIL		4	/* Autonegotiation failed */


/*
 * Prototypes start here
 */
static void      xe_init		(void *xscp);
static void      xe_start		(struct ifnet *ifp);
static int       xe_ioctl		(struct ifnet *ifp, u_long command, caddr_t data);
static void      xe_watchdog		(struct ifnet *ifp);
static int       xe_media_change	(struct ifnet *ifp);
static void      xe_media_status	(struct ifnet *ifp, struct ifmediareq *mrp);
static timeout_t xe_setmedia;
static void      xe_hard_reset		(struct xe_softc *scp);
static void      xe_soft_reset		(struct xe_softc *scp);
static void      xe_stop		(struct xe_softc *scp);
static void      xe_enable_intr		(struct xe_softc *scp);
static void      xe_disable_intr	(struct xe_softc *scp);
static void      xe_setmulti		(struct xe_softc *scp);
static void      xe_setaddrs		(struct xe_softc *scp);
static int       xe_pio_write_packet	(struct xe_softc *scp, struct mbuf *mbp);
static u_int32_t xe_compute_crc		(u_int8_t *data, int len) __unused;
static int       xe_compute_hashbit	(u_int32_t crc) __unused;

/*
 * MII functions
 */
static void      xe_mii_sync		(struct xe_softc *scp);
static int       xe_mii_init    	(struct xe_softc *scp);
static void      xe_mii_send		(struct xe_softc *scp, u_int32_t bits, int cnt);
static int       xe_mii_readreg		(struct xe_softc *scp, struct xe_mii_frame *frame);
static int       xe_mii_writereg	(struct xe_softc *scp, struct xe_mii_frame *frame);
static u_int16_t xe_phy_readreg		(struct xe_softc *scp, u_int16_t reg);
static void      xe_phy_writereg	(struct xe_softc *scp, u_int16_t reg, u_int16_t data);

/*
 * Debug functions -- uncomment for VERY verbose dignostic information.
 * Set to 1 for less verbose information
 */
/* #define XE_DEBUG 2 */
#ifdef XE_DEBUG
#define XE_REG_DUMP(scp)		xe_reg_dump((scp))
#define XE_MII_DUMP(scp)		xe_mii_dump((scp))
static void      xe_reg_dump		(struct xe_softc *scp);
static void      xe_mii_dump		(struct xe_softc *scp);
#else
#define XE_REG_DUMP(scp)
#define XE_MII_DUMP(scp)
#endif

/*
 * Attach a device.
 */
int
xe_attach (device_t dev)
{
  struct xe_softc *scp = device_get_softc(dev);
  int err;

#ifdef XE_DEBUG
  device_printf(dev, "attach\n");
#endif

  /* Fill in some private data */
  scp->ifp = &scp->arpcom.ac_if;
  scp->ifm = &scp->ifmedia;
  scp->autoneg_status = 0;

  /* Hopefully safe to read this here */
  XE_SELECT_PAGE(4);
  scp->version = XE_INB(XE_BOV);

  scp->dev = dev;
  /* Initialise the ifnet structure */
  if (!scp->ifp->if_name) {
    scp->ifp->if_softc = scp;
    scp->ifp->if_name = "xe";
    scp->ifp->if_unit = device_get_unit(dev);
    scp->ifp->if_timer = 0;
    scp->ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
    scp->ifp->if_linkmib = &scp->mibdata;
    scp->ifp->if_linkmiblen = sizeof scp->mibdata;
    scp->ifp->if_output = ether_output;
    scp->ifp->if_start = xe_start;
    scp->ifp->if_ioctl = xe_ioctl;
    scp->ifp->if_watchdog = xe_watchdog;
    scp->ifp->if_init = xe_init;
    scp->ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
  }

  /* Initialise the ifmedia structure */
  ifmedia_init(scp->ifm, 0, xe_media_change, xe_media_status);
  callout_handle_init(&scp->chand);

  /*
   * Fill in supported media types.  Some cards _do_ support full duplex
   * operation, but this driver doesn't, yet.  Therefore we leave those modes
   * out of the list.  We support some form of autoselection in all cases.
   */
  if (scp->mohawk) {
    ifmedia_add(scp->ifm, IFM_ETHER|IFM_100_TX, 0, NULL);
    ifmedia_add(scp->ifm, IFM_ETHER|IFM_10_T, 0, NULL);
  }
  else {
    ifmedia_add(scp->ifm, IFM_ETHER|IFM_10_T, 0, NULL);
    ifmedia_add(scp->ifm, IFM_ETHER|IFM_10_2, 0, NULL);
  }
  ifmedia_add(scp->ifm, IFM_ETHER|IFM_AUTO, 0, NULL);

  /* Default is to autoselect best supported media type */
  ifmedia_set(scp->ifm, IFM_ETHER|IFM_AUTO);

  /* Print some useful information */
  device_printf(dev, "%s %s, bonding version %#x%s%s\n",
	 scp->vendor,
	 scp->card_type,
	 scp->version,
	 scp->mohawk ? ", 100Mbps capable" : "",
	 scp->modem ?  ", with modem"      : "");
  if (scp->mohawk) {
    XE_SELECT_PAGE(0x10);
    device_printf(dev, "DingoID = %#x, RevisionID = %#x, VendorID = %#x\n",
	   XE_INW(XE_DINGOID),
	   XE_INW(XE_RevID),
	   XE_INW(XE_VendorID));
  }
  if (scp->ce2) {
    XE_SELECT_PAGE(0x45);
    device_printf(dev, "CE2 version = %#x\n", XE_INB(XE_REV));
  }

  /* Print MAC address */
  device_printf(dev, "Ethernet address %6D\n", scp->arpcom.ac_enaddr, ":");

  /* Attach the interface */
  ether_ifattach(scp->ifp, ETHER_BPF_SUPPORTED);

  /* Done */
  return 0;
}


/*
 * Initialize device.  Completes the reset procedure on the card and starts
 * output.  If there's an autonegotiation in progress we DON'T do anything;
 * the media selection code will call us again when it's done.
 */
static void
xe_init(void *xscp) {
  struct xe_softc *scp = xscp;
  int s;

#ifdef XE_DEBUG
  device_printf(scp->dev, "init\n");
#endif

  if (TAILQ_EMPTY(&scp->ifp->if_addrhead)) return;

  /* Reset transmitter flags */
  scp->tx_queued = 0;
  scp->tx_tpr = 0;
  scp->tx_collisions = 0;
  scp->ifp->if_timer = 0;

  s = splimp();

  XE_SELECT_PAGE(0x42);
  XE_OUTB(XE_SWC0, 0x20);	/* Disable source insertion (WTF is that?) */

  /*
   * Set the 'local memory dividing line' -- splits the 32K card memory into
   * 8K for transmit buffers and 24K for receive.  This is done automatically
   * on newer revision cards.
   */
  if (scp->srev != 1) {
    XE_SELECT_PAGE(2);
    XE_OUTW(XE_RBS, 0x2000);
  }

  /* Set up multicast addresses */
  xe_setmulti(scp);

  /* Fix the data offset register -- reset leaves it off-by-one */
  XE_SELECT_PAGE(0);
  XE_OUTW(XE_DO, 0x2000);

  /*
   * Set MAC interrupt masks and clear status regs.  The bit names are direct
   * from the Linux code; I have no idea what most of them do.
   */
  XE_SELECT_PAGE(0x40);		/* Bit 7..0 */
  XE_OUTB(XE_RX0Msk, 0xff);	/* ROK, RAB, rsv, RO,  CRC, AE,  PTL, MP  */
  XE_OUTB(XE_TX0Msk, 0xff);	/* TOK, TAB, SQE, LL,  TU,  JAB, EXC, CRS */
  XE_OUTB(XE_TX0Msk+1, 0xb0);	/* rsv, rsv, PTD, EXT, rsv, rsv, rsv, rsv */
  XE_OUTB(XE_RST0, 0x00);	/* ROK, RAB, REN, RO,  CRC, AE,  PTL, MP  */
  XE_OUTB(XE_TXST0, 0x00);	/* TOK, TAB, SQE, LL,  TU,  JAB, EXC, CRS */
  XE_OUTB(XE_TXST1, 0x00);	/* TEN, rsv, PTD, EXT, retry_counter:4    */

  /*
   * Check for an in-progress autonegotiation.  If one is active, just set
   * IFF_RUNNING and return.  The media selection code will call us again when 
   * it's done.
   */
  if (scp->autoneg_status) {
    scp->ifp->if_flags |= IFF_RUNNING;
  }
  else {
    /* Enable receiver, put MAC online */
    XE_SELECT_PAGE(0x40);
    XE_OUTB(XE_CMD0, XE_CMD0_RX_ENABLE|XE_CMD0_ONLINE);

    /* Set up IMR, enable interrupts */
    xe_enable_intr(scp);

    /* Attempt to start output */
    scp->ifp->if_flags |= IFF_RUNNING;
    scp->ifp->if_flags &= ~IFF_OACTIVE;
    xe_start(scp->ifp);
  }

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
  struct xe_softc *scp = ifp->if_softc;
  struct mbuf *mbp;

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

    /* Tap off here if there is a bpf listener */
    if (ifp->if_bpf) {
#if XE_DEBUG > 1
      device_printf(scp->dev, "sending output packet to BPF\n");
#endif
      bpf_mtap(ifp, mbp);
    }

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
  struct xe_softc *scp;
  int s, error;

  scp = ifp->if_softc;
  error = 0;

  s = splimp();

  switch (command) {

   case SIOCSIFADDR:
   case SIOCGIFADDR:
   case SIOCSIFMTU:
    error = ether_ioctl(ifp, command, data);
    break;

   case SIOCSIFFLAGS:
    /*
     * If the interface is marked up and stopped, then start it.  If it is
     * marked down and running, then stop it.
     */
    if (ifp->if_flags & IFF_UP) {
      if (!(ifp->if_flags & IFF_RUNNING)) {
	xe_hard_reset(scp);
	xe_setmedia(scp);
	xe_init(scp);
      }
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

   case SIOCSIFMEDIA:
   case SIOCGIFMEDIA:
    /*
     * Someone wants to get/set media options.
     */
    error = ifmedia_ioctl(ifp, (struct ifreq *)data, &scp->ifmedia, command);
    break;

   default:
    error = EINVAL;
  }

  (void)splx(s);

  return error;
}


/*
 * Card interrupt handler.
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
static void
xe_intr(void *xscp) 
{
  struct xe_softc *scp = (struct xe_softc *) xscp;
  struct ifnet *ifp;
  int result;
  u_int16_t rx_bytes, rxs, txs;
  u_int8_t psr, isr, esr, rsr;

  ifp = &scp->arpcom.ac_if;
  rx_bytes = 0;			/* Bytes received on this interrupt */
  result = 0;			/* Set true if the interrupt is for us */

  if (scp->mohawk) {
    XE_OUTB(XE_CR, 0);		/* Disable interrupts */
  }

  psr = XE_INB(XE_PR);		/* Stash the current register page */

  /*
   * Read ISR to see what caused this interrupt.  Note that this clears the
   * ISR on CE2 type cards.
   */
  if ((isr = XE_INB(XE_ISR)) && isr != 0xff) {

    result = 1;			/* This device did generate an int */
    esr = XE_INB(XE_ESR);	/* Read the other status registers */
    XE_SELECT_PAGE(0x40);
    rxs = XE_INB(XE_RST0);
    XE_OUTB(XE_RST0, ~rxs & 0xff);
    txs = XE_INB(XE_TXST0);
    txs |= XE_INB(XE_TXST1) << 8;
    XE_OUTB(XE_TXST0, 0);
    XE_OUTB(XE_TXST1, 0);
    XE_SELECT_PAGE(0);

#if XE_DEBUG > 2
    printf("xe%d: ISR=%#2.2x ESR=%#2.2x RST=%#2.2x TXST=%#4.4x\n", unit, isr, esr, rxs, txs);
#endif

    /*
     * Handle transmit interrupts
     */
    if (isr & XE_ISR_TX_PACKET) {
      u_int8_t new_tpr, sent;
      
      if ((new_tpr = XE_INB(XE_TPR)) < scp->tx_tpr)	/* Update packet count */
	sent = (0xff - scp->tx_tpr) + new_tpr;		/* TPR rolled over */
      else
	sent = new_tpr - scp->tx_tpr;

      if (sent > 0) {				/* Packets sent since last interrupt */
	scp->tx_tpr = new_tpr;
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
	 * XXX - This will screw up if tx_collisions/sent > 14. FIX IT!
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
    while ((esr = XE_INB(XE_ESR)) & XE_ESR_FULL_PACKET_RX) {

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
	  XE_OUTW(XE_DO, 0x8000);
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
	    rhs = XE_INW(XE_RHSA);
	    XE_SELECT_PAGE(0);

	    rhs += 3;			 /* Skip control info */

	    if (rhs >= 0x8000)
	      rhs = 0;

	    if (rhs + len > 0x8000) {
	      int i;

	      /*
	       * XXX - This i-- seems very wrong, but it's what the Linux guys 
	       * XXX - do.  Need someone with an old CE2 to test this for me.
	       * XXX - 99/3/28: Changed the first i-- to an i++, maybe that'll
	       * XXX - fix it?  It seems as though the previous version would
	       * XXX - have caused an infinite loop (what, another one?).
	       */
	      for (i = 0; i < len; i++, rhs++) {
		((char *)ehp)[i] = XE_INB(XE_EDP);
		if (rhs == 0x8000) {
		  rhs = 0;
		  i--;
		}
	      }
	    }
	    else
	      bus_space_read_multi_2(scp->bst, scp->bsh, XE_EDP, 
	       (u_int16_t *) ehp, len >> 1);
	  }
	  else
	    bus_space_read_multi_2(scp->bst, scp->bsh, XE_EDP, 
	     (u_int16_t *) ehp, len >> 1);

	  /* Deliver packet to upper layers */
	  if (mbp != NULL) {
	    mbp->m_pkthdr.len = mbp->m_len = len - ETHER_HDR_LEN;
	    mbp->m_data += ETHER_HDR_LEN;	/* Strip off Ethernet header */
	    ether_input(ifp, ehp, mbp);		/* Send the packet on its way */
	    ifp->if_ipackets++;			/* Success! */
	  }
	  XE_OUTW(XE_DO, 0x8000);		/* skip_rx_packet command */
	}
      }
      else if (rsr & XE_RSR_LONG_PACKET) {	/* Packet length >1518 bytes */
	scp->mibdata.dot3StatsFrameTooLongs++;
	ifp->if_ierrors++;
      }
      else if (rsr & XE_RSR_CRC_ERROR) {	/* Bad checksum on packet */
	scp->mibdata.dot3StatsFCSErrors++;
	ifp->if_ierrors++;
      }
      else if (rsr & XE_RSR_ALIGN_ERROR) {	/* Packet alignment error */
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

  /* Could force an int here, instead of dropping packets? */
  /* XE_OUTB(XE_CR, XE_CR_ENABLE_INTR|XE_CE_FORCE_INTR); */

  return;
}


/*
 * Device timeout/watchdog routine.  Called automatically if we queue a packet 
 * for transmission but don't get an interrupt within a specified timeout
 * (usually 5 seconds).  When this happens we assume the worst and reset the
 * card.
 */
static void
xe_watchdog(struct ifnet *ifp) {
  struct xe_softc *scp = ifp->if_softc;

  device_printf(scp->dev, "watchdog timeout; resetting card\n");
  scp->tx_timeouts++;
  ifp->if_oerrors += scp->tx_queued;
  xe_stop(scp);
  xe_hard_reset(scp);
  xe_setmedia(scp);
  xe_init(scp);
}


/*
 * Change media selection.
 */
static int
xe_media_change(struct ifnet *ifp) {
  struct xe_softc *scp = ifp->if_softc;

#ifdef XE_DEBUG
  printf("xe%d: media_change\n", ifp->if_unit);
#endif

  if (IFM_TYPE(scp->ifm->ifm_media) != IFM_ETHER)
    return(EINVAL);

  /*
   * Some card/media combos aren't always possible -- filter those out here.
   */
  if ((IFM_SUBTYPE(scp->ifm->ifm_media) == IFM_AUTO ||
       IFM_SUBTYPE(scp->ifm->ifm_media) == IFM_100_TX) && !scp->phy_ok)
    return (EINVAL);

  xe_setmedia(scp);

  return 0;
}


/*
 * Return current media selection.
 */
static void
xe_media_status(struct ifnet *ifp, struct ifmediareq *mrp) {

#ifdef XE_DEBUG
  printf("xe%d: media_status\n", ifp->if_unit);
#endif

  mrp->ifm_active = ((struct xe_softc *)ifp->if_softc)->media;

  return;
}


/*
 * Select active media.
 */
static void xe_setmedia(void *xscp) {
  struct xe_softc *scp = xscp;
  u_int16_t bmcr, bmsr, anar, lpar;

#ifdef XE_DEBUG
  device_printf(scp->dev, "setmedia\n");
#endif

  /* Cancel any pending timeout */
  untimeout(xe_setmedia, scp, scp->chand);
  xe_disable_intr(scp);

  /* Select media */
  scp->media = IFM_ETHER;
  switch (IFM_SUBTYPE(scp->ifm->ifm_media)) {

   case IFM_AUTO:	/* Autoselect media */
    scp->media = IFM_ETHER|IFM_AUTO;

    /*
     * Autoselection is really awful.  It goes something like this:
     *
     * Wait until the transmitter goes idle (2sec timeout).
     * Reset card
     *   IF a 100Mbit PHY exists
     *     Start NWAY autonegotiation (3.5sec timeout)
     *     IF that succeeds
     *       Select 100baseTX or 10baseT, whichever was detected
     *     ELSE
     *       Reset card
     *       IF a 100Mbit PHY exists
     *         Try to force a 100baseTX link (3sec timeout)
     *         IF that succeeds
     *           Select 100baseTX
     *         ELSE
     *           Disable the PHY
     *         ENDIF
     *       ENDIF
     *     ENDIF
     *   ENDIF
     * IF nothing selected so far
     *   IF a 100Mbit PHY exists
     *     Select 10baseT
     *   ELSE
     *     Select 10baseT or 10base2, whichever is connected
     *   ENDIF
     * ENDIF
     */
    switch (scp->autoneg_status) {

     case XE_AUTONEG_NONE:
#if XE_DEBUG > 1
      device_printf(scp->dev, "Waiting for idle transmitter\n");
#endif
      scp->arpcom.ac_if.if_flags |= IFF_OACTIVE;
      scp->autoneg_status = XE_AUTONEG_WAITING;
      scp->chand = timeout(xe_setmedia, scp, hz * 2);
      return;

     case XE_AUTONEG_WAITING:
      xe_soft_reset(scp);
      if (scp->phy_ok) {
#if XE_DEBUG > 1
	device_printf(scp->dev, "Starting autonegotiation\n");
#endif
	bmcr = xe_phy_readreg(scp, PHY_BMCR);
	bmcr &= ~(PHY_BMCR_AUTONEGENBL);
	xe_phy_writereg(scp, PHY_BMCR, bmcr);
	anar = xe_phy_readreg(scp, PHY_ANAR);
	anar &= ~(PHY_ANAR_100BT4|PHY_ANAR_100BTXFULL|PHY_ANAR_10BTFULL);
	anar |= PHY_ANAR_100BTXHALF|PHY_ANAR_10BTHALF;
	xe_phy_writereg(scp, PHY_ANAR, anar);
	bmcr |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	xe_phy_writereg(scp, PHY_BMCR, bmcr);
	scp->autoneg_status = XE_AUTONEG_STARTED;
	scp->chand = timeout(xe_setmedia, scp, hz * 7/2);
	return;
      }
      else {
	scp->autoneg_status = XE_AUTONEG_FAIL;
      }
      break;

     case XE_AUTONEG_STARTED:
      bmsr = xe_phy_readreg(scp, PHY_BMSR);
      lpar = xe_phy_readreg(scp, PHY_LPAR);
      if (bmsr & (PHY_BMSR_AUTONEGCOMP|PHY_BMSR_LINKSTAT)) {
#if XE_DEBUG > 1
	device_printf(scp->dev, "Autonegotiation complete!\n");
#endif
	/*
	 * XXX - Shouldn't have to do this, but (on my hub at least) the
	 * XXX - transmitter won't work after a successful autoneg.  So we see 
	 * XXX - what the negotiation result was and force that mode.  I'm
	 * XXX - sure there is an easy fix for this.
	 */
	if (lpar & PHY_LPAR_100BTXHALF) {
	  xe_phy_writereg(scp, PHY_BMCR, PHY_BMCR_SPEEDSEL);
	  XE_MII_DUMP(scp);
	  XE_SELECT_PAGE(2);
	  XE_OUTB(XE_MSR, XE_INB(XE_MSR) | 0x08);
	  scp->media = IFM_ETHER|IFM_100_TX;
	  scp->autoneg_status = XE_AUTONEG_NONE;
	}
	else {
	  /*
	   * XXX - Bit of a hack going on in here.
	   * XXX - This is derived from Ken Hughes patch to the Linux driver
	   * XXX - to make it work with 10Mbit _autonegotiated_ links on CE3B
	   * XXX - cards.  What's a CE3B and how's it differ from a plain CE3?
	   * XXX - these are the things we need to find out.
	   */
	  xe_phy_writereg(scp, PHY_BMCR, 0x0000);
	  XE_SELECT_PAGE(2);
	  /* BEGIN HACK */
	  XE_OUTB(XE_MSR, XE_INB(XE_MSR) | 0x08);
	  XE_SELECT_PAGE(0x42);
	  XE_OUTB(XE_SWC1, 0x80);
	  scp->media = IFM_ETHER|IFM_10_T;
	  scp->autoneg_status = XE_AUTONEG_NONE;
	  /* END HACK */
	  /*XE_OUTB(XE_MSR, XE_INB(XE_MSR) & ~0x08);*/	/* Disable PHY? */
	  /*scp->autoneg_status = XE_AUTONEG_FAIL;*/
	}
      }
      else {
#if XE_DEBUG > 1
	device_printf(scp->dev, "Autonegotiation failed; trying 100baseTX\n");
#endif
	XE_MII_DUMP(scp);
	xe_soft_reset(scp);
	if (scp->phy_ok) {
	  xe_phy_writereg(scp, PHY_BMCR, PHY_BMCR_SPEEDSEL);
	  scp->autoneg_status = XE_AUTONEG_100TX;
	  scp->chand = timeout(xe_setmedia, scp, hz * 3);
	  return;
	}
	else {
	  scp->autoneg_status = XE_AUTONEG_FAIL;
	}
      }
      break;

     case XE_AUTONEG_100TX:
      (void)xe_phy_readreg(scp, PHY_BMSR);
      bmsr = xe_phy_readreg(scp, PHY_BMSR);
      if (bmsr & PHY_BMSR_LINKSTAT) {
#if XE_DEBUG > 1
	device_printf(scp->dev, "Got 100baseTX link!\n");
#endif
	XE_MII_DUMP(scp);
	XE_SELECT_PAGE(2);
	XE_OUTB(XE_MSR, XE_INB(XE_MSR) | 0x08);
	scp->media = IFM_ETHER|IFM_100_TX;
	scp->autoneg_status = XE_AUTONEG_NONE;
      }
      else {
#if XE_DEBUG > 1
	device_printf(scp->dev, "Autonegotiation failed; disabling PHY\n");
#endif
	XE_MII_DUMP(scp);
	xe_phy_writereg(scp, PHY_BMCR, 0x0000);
	XE_SELECT_PAGE(2);
	XE_OUTB(XE_MSR, XE_INB(XE_MSR) & ~0x08);	/* Disable PHY? */
	scp->autoneg_status = XE_AUTONEG_FAIL;
      }
      break;
    }

    /*
     * If we got down here _and_ autoneg_status is XE_AUTONEG_FAIL, then
     * either autonegotiation failed, or never got started to begin with.  In
     * either case, select a suitable 10Mbit media and hope it works.  We
     * don't need to reset the card again, since it will have been done
     * already by the big switch above.
     */
    if (scp->autoneg_status == XE_AUTONEG_FAIL) {
#if XE_DEBUG > 1
      device_printf(scp->dev, "Selecting 10baseX\n");
#endif
      if (scp->mohawk) {
	XE_SELECT_PAGE(0x42);
	XE_OUTB(XE_SWC1, 0x80);
	scp->media = IFM_ETHER|IFM_10_T;
	scp->autoneg_status = XE_AUTONEG_NONE;
      }
      else {
	XE_SELECT_PAGE(4);
	XE_OUTB(XE_GPR0, 4);
	DELAY(50000);
	XE_SELECT_PAGE(0x42);
	XE_OUTB(XE_SWC1, (XE_INB(XE_ESR) & XE_ESR_MEDIA_SELECT) ? 0x80 : 0xc0);
	scp->media = IFM_ETHER|((XE_INB(XE_ESR) & XE_ESR_MEDIA_SELECT) ? IFM_10_T : IFM_10_2);
	scp->autoneg_status = XE_AUTONEG_NONE;
      }
    }
    break;


    /*
     * If a specific media has been requested, we just reset the card and
     * select it (one small exception -- if 100baseTX is requested by there is 
     * no PHY, we fall back to 10baseT operation).
     */
   case IFM_100_TX:	/* Force 100baseTX */
    xe_soft_reset(scp);
    if (scp->phy_ok) {
#if XE_DEBUG > 1
      device_printf(scp->dev, "Selecting 100baseTX\n");
#endif
      XE_SELECT_PAGE(0x42);
      XE_OUTB(XE_SWC1, 0);
      xe_phy_writereg(scp, PHY_BMCR, PHY_BMCR_SPEEDSEL);
      XE_SELECT_PAGE(2);
      XE_OUTB(XE_MSR, XE_INB(XE_MSR) | 0x08);
      scp->media |= IFM_100_TX;
      break;
    }
    /* FALLTHROUGH */

   case IFM_10_T:	/* Force 10baseT */
    xe_soft_reset(scp);
#if XE_DEBUG > 1
    device_printf(scp->dev, "Selecting 10baseT\n");
#endif
    if (scp->phy_ok) {
      xe_phy_writereg(scp, PHY_BMCR, 0x0000);
      XE_SELECT_PAGE(2);
      XE_OUTB(XE_MSR, XE_INB(XE_MSR) & ~0x08);	/* Disable PHY */
    }
    XE_SELECT_PAGE(0x42);
    XE_OUTB(XE_SWC1, 0x80);
    scp->media |= IFM_10_T;
    break;

   case IFM_10_2:
    xe_soft_reset(scp);
#if XE_DEBUG > 1
    device_printf(scp->dev, "Selecting 10base2\n");
#endif
    XE_SELECT_PAGE(0x42);
    XE_OUTB(XE_SWC1, 0xc0);
    scp->media |= IFM_10_2;
    break;
  }


  /*
   * Finally, the LEDs are set to match whatever media was chosen and the
   * transmitter is unblocked. 
   */
#if XE_DEBUG > 1
  device_printf(scp->dev, "Setting LEDs\n");
#endif
  XE_SELECT_PAGE(2);
  switch (IFM_SUBTYPE(scp->media)) {
   case IFM_100_TX:
   case IFM_10_T:
    XE_OUTB(XE_LED, 0x3b);
    if (scp->dingo)
      XE_OUTB(0x0b, 0x04);	/* 100Mbit LED */
    break;

   case IFM_10_2:
    XE_OUTB(XE_LED, 0x3a);
    break;
  }

  /* Restart output? */
  scp->ifp->if_flags &= ~IFF_OACTIVE;
  xe_init(scp);
}


/*
 * Hard reset (power cycle) the card.
 */
static void
xe_hard_reset(struct xe_softc *scp) {
  int s;

#ifdef XE_DEBUG
  device_printf(scp->dev, "hard_reset\n");
#endif

  s = splimp();

  /*
   * Power cycle the card.
   */
  XE_SELECT_PAGE(4);
  XE_OUTB(XE_GPR1, 0);		/* Power off */
  DELAY(40000);

  if (scp->mohawk)
    XE_OUTB(XE_GPR1, 1);	/* And back on again */
  else
    XE_OUTB(XE_GPR1, 5);	/* Also set AIC bit, whatever that is */
  DELAY(40000);
  XE_SELECT_PAGE(0);

  (void)splx(s);
}


/*
 * Soft reset the card.  Also makes sure that the ML6692 and 10Mbit controller 
 * are powered up, sets the silicon revision number in softc, disables
 * interrupts and checks for the prescence of a 100Mbit PHY.  This should
 * leave us in a position where we can access the PHY and do media
 * selection. The function imposes a 0.5s delay while the hardware powers up.
 */
static void
xe_soft_reset(struct xe_softc *scp) {
  int s;

#ifdef XE_DEBUG
  device_printf(scp->dev, "soft_reset\n");
#endif

  s = splimp();

  /*
   * Reset the card, (again).
   */
  XE_SELECT_PAGE(0);
  XE_OUTB(XE_CR, XE_CR_SOFT_RESET);
  DELAY(40000);
  XE_OUTB(XE_CR, 0);
  DELAY(40000);

  if (scp->mohawk) {
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
   * Get silicon revision number.
   */
  XE_SELECT_PAGE(4);
  if (scp->mohawk)
    scp->srev = (XE_INB(XE_BOV) & 0x70) >> 4;
  else
    scp->srev = (XE_INB(XE_BOV) & 0x30) >> 4;
#ifdef XE_DEBUG
  device_printf(scp->dev, "silicon revision = %d\n", scp->srev);
#endif
  
  /*
   * Shut off interrupts.
   */
  xe_disable_intr(scp);

  /*
   * Check for PHY.
   */
  if (scp->mohawk) {
    scp->phy_ok = xe_mii_init(scp);
  }

  XE_SELECT_PAGE(0);

  (void)splx(s);
}


/*
 * Take interface offline.  This is done by powering down the device, which I
 * assume means just shutting down the transceiver and Ethernet logic.  This
 * requires a _hard_ reset to recover from, as we need to power up again.
 */
static void
xe_stop(struct xe_softc *scp) {
  int s;

#ifdef XE_DEBUG
  device_printf(scp->dev, "stop\n");
#endif

  s = splimp();

  /*
   * Shut off interrupts.
   */
  xe_disable_intr(scp);

  /*
   * Power down.
   */
  XE_SELECT_PAGE(4);
  XE_OUTB(XE_GPR1, 0);
  XE_SELECT_PAGE(0);

  /*
   * ~IFF_RUNNING == interface down.
   */
  scp->ifp->if_flags &= ~IFF_RUNNING;
  scp->ifp->if_flags &= ~IFF_OACTIVE;
  scp->ifp->if_timer = 0;

  (void)splx(s);
}


/*
 * Enable Ethernet interrupts from the card.
 */
static void
xe_enable_intr(struct xe_softc *scp) {
#ifdef XE_DEBUG
  device_printf(scp->dev, "enable_intr\n");
#endif

  XE_SELECT_PAGE(1);
  XE_OUTB(XE_IMR0, 0xff);		/* Unmask everything */
  XE_OUTB(XE_IMR1, 0x01);		/* Unmask TX underrun detection */
  DELAY(1);

  XE_SELECT_PAGE(0);
  XE_OUTB(XE_CR, XE_CR_ENABLE_INTR);	/* Enable interrupts */
  if (scp->modem && !scp->dingo) {	/* This bit is just magic */
    if (!(XE_INB(0x10) & 0x01)) {
      XE_OUTB(0x10, 0x11);		/* Unmask master int enable bit */
    }
  }
}


/*
 * Disable all Ethernet interrupts from the card.
 */
static void
xe_disable_intr(struct xe_softc *scp) {
#ifdef XE_DEBUG
  device_printf(scp->dev, "disable_intr\n");
#endif

  XE_SELECT_PAGE(0);
  XE_OUTB(XE_CR, 0);			/* Disable interrupts */
  if (scp->modem && !scp->dingo) {	/* More magic (does this work?) */
    XE_OUTB(0x10, 0x10);		/* Mask the master int enable bit */
  }

  XE_SELECT_PAGE(1);
  XE_OUTB(XE_IMR0, 0);			/* Forbid all interrupts */
  XE_OUTB(XE_IMR1, 0);
  XE_SELECT_PAGE(0);
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
  maddr = TAILQ_FIRST(&ifp->if_multiaddrs);

  /* Get length of multicast list */
  for (count = 0; maddr != NULL; maddr = TAILQ_NEXT(maddr, ifma_link), count++);

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
    XE_OUTB(XE_SWC1, 0x01);
    XE_SELECT_PAGE(0x40);
    XE_OUTB(XE_CMD0, XE_CMD0_OFFLINE);
    /*xe_reg_dump(scp);*/
    xe_setaddrs(scp);
    /*xe_reg_dump(scp);*/
    XE_SELECT_PAGE(0x40);
    XE_OUTB(XE_CMD0, XE_CMD0_RX_ENABLE|XE_CMD0_ONLINE);
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
 * XXX - This doesn't work right, but I'm not sure why yet.  We seem to be
 * XXX - doing much the same as the Linux code, which is weird enough that
 * XXX - it's probably right (despite my earlier comments to the contrary).
 */
static void
xe_setaddrs(struct xe_softc *scp) {
  struct ifmultiaddr *maddr;
  u_int8_t *addr;
  u_int8_t page, slot, byte, i;

  maddr = TAILQ_FIRST(&scp->arpcom.ac_if.if_multiaddrs);

  XE_SELECT_PAGE(page = 0x50);

  for (slot = 0, byte = 8; slot < 10; slot++) {

    if (slot == 0)
      addr = (u_int8_t *)(&scp->arpcom.ac_enaddr);
    else {
      while (maddr != NULL && maddr->ifma_addr->sa_family != AF_LINK)
	maddr = TAILQ_NEXT(maddr, ifma_link);
      if (maddr != NULL)
	addr = LLADDR((struct sockaddr_dl *)maddr->ifma_addr);
      else
	addr = (u_int8_t *)(&scp->arpcom.ac_enaddr);
    }

    for (i = 0; i < 6; i++, byte++) {
#if XE_DEBUG > 2
      if (i)
	printf(":%x", addr[i]);
      else
	device_printf(scp->dev, "individual addresses %d: %x", slot, addr[0]);
#endif

      if (byte > 15) {
	page++;
	byte = 8;
	XE_SELECT_PAGE(page);
      }

      if (scp->mohawk)
	XE_OUTB(byte, addr[5 - i]);
      else
	XE_OUTB(byte, addr[i]);
    }
#if XE_DEBUG > 2
    printf("\n");
#endif
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
	bus_space_write_multi_2(scp->bst, scp->bsh, XE_EDP, (u_int16_t *) data,
	 len >> 1);
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
  if (scp->mohawk)
    XE_OUTB(XE_CR, XE_CR_TX_PACKET|XE_CR_ENABLE_INTR);
  else
    while (pad > 0) {
      XE_OUTW(XE_EDP, 0xdead);
      pad--;
    }

  return 0;
}

/*
 * Compute the 32-bit Ethernet CRC for the given buffer.
 */
static u_int32_t
xe_compute_crc(u_int8_t *data, int len) {
  u_int32_t crc = 0xffffffff;
  u_int32_t poly = 0x04c11db6;
  u_int8_t current, crc31, bit;
  int i, k;

  for (i = 0; i < len; i++) {
    current = data[i];
    for (k = 1; k <= 8; k++) {
      if (crc & 0x80000000) {
	crc31 = 0x01;
      }
      else {
	crc31 = 0;
      }
      bit = crc31 ^ (current & 0x01);
      crc <<= 1;
      current >>= 1;
      if (bit) {
	crc = (crc ^ poly)|1;
      }
    }
  }
  return crc;
}


/*
 * Convert a CRC into an index into the multicast hash table.  What we do is
 * take the most-significant 6 bits of the CRC, reverse them, and use that as
 * the bit number in the hash table.  Bits 5:3 of the result give the byte
 * within the table (0-7); bits 2:0 give the bit number within that byte (also 
 * 0-7), ie. the number of shifts needed to get it into the lsb position.
 */
static int
xe_compute_hashbit(u_int32_t crc) {
  u_int8_t hashbit = 0;
  int i;

  for (i = 0; i < 6; i++) {
    hashbit >>= 1;
    if (crc & 0x80000000) {
      hashbit &= 0x80;
    }
    crc <<= 1;
  }
  return (hashbit >> 2);
}



/**************************************************************
 *                                                            *
 *                  M I I  F U N C T I O N S                  *
 *                                                            *
 **************************************************************/

/*
 * Alternative MII/PHY handling code adapted from the xl driver.  It doesn't
 * seem to work any better than the xirc2_ps stuff, but it's cleaner code.
 * XXX - this stuff shouldn't be here.  It should all be abstracted off to
 * XXX - some kind of common MII-handling code, shared by all drivers.  But
 * XXX - that's a whole other mission.
 */
#define XE_MII_SET(x)	XE_OUTB(XE_GPR2, (XE_INB(XE_GPR2) | 0x04) | (x))
#define XE_MII_CLR(x)	XE_OUTB(XE_GPR2, (XE_INB(XE_GPR2) | 0x04) & ~(x))


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
 * Look for a MII-compliant PHY.  If we find one, reset it.
 */
static int
xe_mii_init(struct xe_softc *scp) {
  u_int16_t status;

  status = xe_phy_readreg(scp, PHY_BMSR);
  if ((status & 0xff00) != 0x7800) {
#if XE_DEBUG > 1
    device_printf(scp->dev, "no PHY found, %0x\n", status);
#endif
    return 0;
  }
  else {
#if XE_DEBUG > 1
    device_printf(scp->dev, "PHY OK!\n");
#endif

    /* Reset the PHY */
    xe_phy_writereg(scp, PHY_BMCR, PHY_BMCR_RESET);
    DELAY(500);
    while(xe_phy_readreg(scp, PHY_BMCR) & PHY_BMCR_RESET);
    XE_MII_DUMP(scp);
    return 1;
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


/*
 * Read a register from the PHY.
 */
static u_int16_t
xe_phy_readreg(struct xe_softc *scp, u_int16_t reg) {
  struct xe_mii_frame frame;

  bzero((char *)&frame, sizeof(frame));

  frame.mii_phyaddr = 0;
  frame.mii_regaddr = reg;
  xe_mii_readreg(scp, &frame);

  return(frame.mii_data);
}


/*
 * Write to a PHY register.
 */
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


#ifdef XE_DEBUG
/*
 * A bit of debugging code.
 */
static void
xe_mii_dump(struct xe_softc *scp) {
  int i, s;

  s = splimp();

  device_printf(scp->dev, "MII registers: ");
  for (i = 0; i < 2; i++) {
    printf(" %d:%04x", i, xe_phy_readreg(scp, i));
  }
  for (i = 4; i < 7; i++) {
    printf(" %d:%04x", i, xe_phy_readreg(scp, i));
  }
  printf("\n");

  (void)splx(s);
}

static void
xe_reg_dump(struct xe_softc *scp) {
  int page, i, s;

  s = splimp();

  device_printf(scp->dev, "Common registers: ");
  for (i = 0; i < 8; i++) {
    printf(" %2.2x", XE_INB(i));
  }
  printf("\n");

  for (page = 0; page <= 8; page++) {
    device_printf(scp->dev, "Register page %2.2x: ", page);
    XE_SELECT_PAGE(page);
    for (i = 8; i < 16; i++) {
      printf(" %2.2x", XE_INB(i));
    }
    printf("\n");
  }

  for (page = 0x10; page < 0x5f; page++) {
    if ((page >= 0x11 && page <= 0x3f) ||
	(page == 0x41) ||
	(page >= 0x43 && page <= 0x4f) ||
	(page >= 0x59))
      continue;
    device_printf(scp->dev, "Register page %2.2x: ", page);
    XE_SELECT_PAGE(page);
    for (i = 8; i < 16; i++) {
      printf(" %2.2x", XE_INB(i));
    }
    printf("\n");
  }

  (void)splx(s);
}
#endif

int
xe_activate(device_t dev)
{
	struct xe_softc *sc = device_get_softc(dev);
	int start, err;

	if (!sc->dingo) {
		sc->port_rid = 0;	/* 0 is managed by pccard */
		sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT,
		    &sc->port_rid, 0, ~0, 16, RF_ACTIVE);
	} else {
		/*
		 * Find a 16 byte aligned ioport for the card.
		 */
#if XE_DEBUG > 0
		device_printf(dev, "Finding an aligned port for RealPort\n");
#endif /* XE_DEBUG */
		sc->port_rid = 1;	/* 0 is managed by pccard */
		start = 0x100;
		do {
			sc->port_res = bus_alloc_resource(dev,
			    SYS_RES_IOPORT, &sc->port_rid, start, 0x3ff, 16,
			    RF_ACTIVE);
			if (sc->port_res == 0)
				break;		/* we failed */
			if ((rman_get_start(sc->port_res) & 0xf) == 0)
				break;		/* good */
			bus_release_resource(dev, SYS_RES_IOPORT, sc->port_rid, 
			    sc->port_res);
			start = (rman_get_start(sc->port_res) + 15) & ~0xf;
		} while (1);
#if XE_DEBUG > 2
		device_printf(dev, "port 0x%0lx, size 0x%0lx\n",
		    bus_get_resource_start(dev, SYS_RES_IOPORT, sc->port_rid),
		    bus_get_resource_count(dev, SYS_RES_IOPORT, sc->port_rid));
#endif /* XE_DEBUG */
	}
	if (!sc->port_res) {
#if XE_DEBUG > 0
		device_printf(dev, "Cannot allocate ioport\n");
#endif		
		return ENOMEM;
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid, 
	    0, ~0, 1, RF_ACTIVE);
	if (!sc->irq_res) {
#if XE_DEBUG > 0
		device_printf(dev, "Cannot allocate irq\n");
#endif
		xe_deactivate(dev);
		return ENOMEM;
	}
	if ((err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET, xe_intr, sc,
	    &sc->intrhand)) != 0) {
		xe_deactivate(dev);
		return err;
	}

	sc->bst = rman_get_bustag(sc->port_res);
	sc->bsh = rman_get_bushandle(sc->port_res);
	return (0);
}

void
xe_deactivate(device_t dev)
{
	struct xe_softc *sc = device_get_softc(dev);
	
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = 0;
	if (sc->port_res)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->port_rid, 
		    sc->port_res);
	sc->port_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, 
		    sc->irq_res);
	sc->irq_res = 0;
	return;
}
