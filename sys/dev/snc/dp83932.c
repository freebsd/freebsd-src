/*	$FreeBSD$	*/
/*	$NecBSD: dp83932.c,v 1.5 1999/07/29 05:08:44 kmatsuda Exp $	*/
/*	$NetBSD: if_snc.c,v 1.18 1998/04/25 21:27:40 scottr Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999
 *	Kouichi Matsuda.  All rights reserved.
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
 *      This product includes software developed by Kouichi Matsuda for
 *      NetBSD/pc98.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Modified for FreeBSD(98) 4.0 from NetBSD/pc98 1.4.2 by Motomichi Matsuzaki.
 */

/*
 * Modified for NetBSD/pc98 1.2G from NetBSD/mac68k 1.2G by Kouichi Matsuda.
 * Make adapted for NEC PC-9801-83, 84, PC-9801-103, 104, PC-9801N-25 and
 * PC-9801N-J02, J02R, which uses National Semiconductor DP83934AVQB as
 * Ethernet Controller and National Semiconductor NS46C46 as
 * (64 * 16 bits) Microwire Serial EEPROM.
 */

/*-
 * National Semiconductor  DP8393X SONIC Driver
 * Copyright (c) 1991   Algorithmics Ltd (http://www.algor.co.uk)
 * You may use, copy, and modify this program so long as you retain the
 * copyright line.
 *
 * This driver has been substantially modified since Algorithmics donated
 * it.
 *
 *   Denton Gentry <denny1@home.com>
 * and also
 *   Yanagisawa Takeshi <yanagisw@aa.ap.titech.ac.jp>
 * did the work to get this running on the Macintosh.
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/errno.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <dev/snc/dp83932reg.h>
#include <dev/snc/dp83932var.h>

static void	sncwatchdog(void *);
static void	sncinit(void *);
static void	sncinit_locked(struct snc_softc *);
static int	sncstop(struct snc_softc *sc);
static int	sncioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	sncstart(struct ifnet *ifp);
static void	sncstart_locked(struct ifnet *ifp);
static void	sncreset(struct snc_softc *sc);

static void	caminitialise(struct snc_softc *);
static void	camentry(struct snc_softc *, int, u_char *ea);
static void	camprogram(struct snc_softc *);
static void	initialise_tda(struct snc_softc *);
static void	initialise_rda(struct snc_softc *);
static void	initialise_rra(struct snc_softc *);
#ifdef SNCDEBUG
static void	camdump(struct snc_softc *sc);
#endif

static void	sonictxint(struct snc_softc *);
static void	sonicrxint(struct snc_softc *);

static u_int	sonicput(struct snc_softc *sc, struct mbuf *m0, int mtd_next);
static int	sonic_read(struct snc_softc *, u_int32_t, int);
static struct mbuf *sonic_get(struct snc_softc *, u_int32_t, int);

int	snc_enable(struct snc_softc *);
void	snc_disable(struct snc_softc *);

int	snc_mediachange(struct ifnet *);
void	snc_mediastatus(struct ifnet *, struct ifmediareq *);

#undef assert
#undef _assert

#ifdef NDEBUG
#define	assert(e)	((void)0)
#define	_assert(e)	((void)0)
#else
#define	_assert(e)	assert(e)
#ifdef __STDC__
#define	assert(e)	((e) ? (void)0 : __assert("snc ", __FILE__, __LINE__, #e))
#else	/* PCC */
#define	assert(e)	((e) ? (void)0 : __assert("snc "__FILE__, __LINE__, "e"))
#endif
#endif

#ifdef	SNCDEBUG
#define	SNC_SHOWTXHDR	0x01	/* show tx ether_header */
#define	SNC_SHOWRXHDR	0x02	/* show rx ether_header */
#define	SNC_SHOWCAMENT	0x04	/* show CAM entry */
#endif	/* SNCDEBUG */
int sncdebug = 0;


int
sncconfig(struct snc_softc *sc, int *media, int nmedia, int defmedia,
    u_int8_t *myea)
{
	struct ifnet *ifp;
	int i;

#ifdef SNCDEBUG
	if ((sncdebug & SNC_SHOWCAMENT) != 0) {
		camdump(sc);
	}
#endif

	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		return (ENOMEM);
	}

#ifdef SNCDEBUG
	device_printf(sc->sc_dev,
		      "buffers: rra=0x%x cda=0x%x rda=0x%x tda=0x%x\n",
		      sc->v_rra[0], sc->v_cda,
		      sc->v_rda, sc->mtda[0].mtd_vtxp);
#endif

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev));
	ifp->if_ioctl = sncioctl;
	ifp->if_start = sncstart;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
        ifp->if_init = sncinit;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);

	/* Initialize media goo. */
	ifmedia_init(&sc->sc_media, 0, snc_mediachange,
	    snc_mediastatus);
	if (media != NULL) {
		for (i = 0; i < nmedia; i++)
			ifmedia_add(&sc->sc_media, media[i], 0, NULL);
		ifmedia_set(&sc->sc_media, defmedia);
	} else {
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	}

	ether_ifattach(ifp, myea);
	return (0);
}

void
sncshutdown(void *arg)
{
	struct snc_softc *sc = arg;

	SNC_ASSERT_LOCKED(sc);
	sncstop(sc);
}

/*
 * Media change callback.
 */
int
snc_mediachange(struct ifnet *ifp)
{
	struct snc_softc *sc = ifp->if_softc;
	int error;

	SNC_LOCK(sc);
	if (sc->sc_mediachange)
		error = (*sc->sc_mediachange)(sc);
	else
		error = EINVAL;
	SNC_UNLOCK(sc);
	return (error);
}

/*
 * Media status callback.
 */
void
snc_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct snc_softc *sc = ifp->if_softc;

	SNC_LOCK(sc);
	if (sc->sc_enabled == 0) {
		ifmr->ifm_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_status = 0;
		SNC_UNLOCK(sc);
		return;
	}

	if (sc->sc_mediastatus)
		(*sc->sc_mediastatus)(sc, ifmr);
	SNC_UNLOCK(sc);
}


static int
sncioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr;
	struct snc_softc *sc = ifp->if_softc;
	int	err = 0;

	switch (cmd) {

	case SIOCSIFFLAGS:
		SNC_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * then stop it.
			 */
			sncstop(sc);
			snc_disable(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped,
			 * then start it.
			 */
			if ((err = snc_enable(sc)) != 0)
				break;
			sncinit_locked(sc);
		} else if (sc->sc_enabled) {
			/*
			 * reset the interface to pick up any other changes
			 * in flags
			 */
			sncreset(sc);
			sncstart_locked(ifp);
		}
		SNC_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		SNC_LOCK(sc);
		if (sc->sc_enabled == 0) {
			err = EIO;
			SNC_UNLOCK(sc);
			break;
		}
		sncreset(sc);
		SNC_UNLOCK(sc);
		err = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		ifr = (struct ifreq *) data;
		err = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	default:
		err = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (err);
}

/*
 * Encapsulate a packet of type family for the local net.
 */
static void
sncstart(struct ifnet *ifp)
{
	struct snc_softc	*sc = ifp->if_softc;

	SNC_LOCK(sc);
	sncstart_locked(ifp);
	SNC_UNLOCK(sc);
}

static void
sncstart_locked(struct ifnet *ifp)
{
	struct snc_softc	*sc = ifp->if_softc;
	struct mbuf	*m;
	int		mtd_next;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

outloop:
	/* Check for room in the xmit buffer. */
	if ((mtd_next = (sc->mtd_free + 1)) == NTDA)
		mtd_next = 0;

	if (mtd_next == sc->mtd_hw) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0)
		return;

	/* We need the header for m_pkthdr.len. */
	M_ASSERTPKTHDR(m);

	/*
	 * If there is nothing in the o/p queue, and there is room in
	 * the Tx ring, then send the packet directly.  Otherwise append
	 * it to the o/p queue.
	 */
	if ((sonicput(sc, m, mtd_next)) == 0) {
		IF_PREPEND(&ifp->if_snd, m);
		return;
	}

	/*
	 * If bpf is listening on this interface, let it see the packet
	 * before we commit it to the wire, but only if we are really
	 * committed to send it.
	 *
	 * XXX: Locking must protect m against premature m_freem() in
	 * sonictxint().
	 */
	BPF_MTAP(ifp, m);

	sc->mtd_prev = sc->mtd_free;
	sc->mtd_free = mtd_next;

	if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);		/* # of pkts */

	/* Jump back for possibly more punishment. */
	goto outloop;
}

/*
 * reset and restart the SONIC.  Called in case of fatal
 * hardware/software errors.
 */
static void
sncreset(struct snc_softc *sc)
{
	sncstop(sc);
	sncinit_locked(sc);
}

static void
sncinit(void *xsc)
{
	struct snc_softc *sc = xsc;

	SNC_LOCK(sc);
	sncinit_locked(sc);
	SNC_UNLOCK(sc);
}

static void
sncinit_locked(struct snc_softc *sc)
{
	u_long	s_rcr;

	if (sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING)
		/* already running */
		return;

	NIC_PUT(sc, SNCR_CR, CR_RST);	/* DCR only accessable in reset mode! */

	/* config it */
	NIC_PUT(sc, SNCR_DCR, (sc->sncr_dcr |
		(sc->bitmode ? DCR_DW32 : DCR_DW16)));
	NIC_PUT(sc, SNCR_DCR2, sc->sncr_dcr2);

	s_rcr = RCR_BRD | RCR_LBNONE;
	if (sc->sc_ifp->if_flags & IFF_PROMISC)
		s_rcr |= RCR_PRO;
	if (sc->sc_ifp->if_flags & IFF_ALLMULTI)
		s_rcr |= RCR_AMC;
	NIC_PUT(sc, SNCR_RCR, s_rcr);

	NIC_PUT(sc, SNCR_IMR, (IMR_PRXEN | IMR_PTXEN | IMR_TXEREN | IMR_LCDEN));

	/* clear pending interrupts */
	NIC_PUT(sc, SNCR_ISR, ISR_ALL);

	/* clear tally counters */
	NIC_PUT(sc, SNCR_CRCT, -1);
	NIC_PUT(sc, SNCR_FAET, -1);
	NIC_PUT(sc, SNCR_MPT, -1);

	initialise_tda(sc);
	initialise_rda(sc);
	initialise_rra(sc);

	/* enable the chip */
	NIC_PUT(sc, SNCR_CR, 0);
	wbflush();

	/* program the CAM */
	camprogram(sc);

	/* get it to read resource descriptors */
	NIC_PUT(sc, SNCR_CR, CR_RRRA);
	wbflush();
	while ((NIC_GET(sc, SNCR_CR)) & CR_RRRA)
		continue;

	/* enable rx */
	NIC_PUT(sc, SNCR_CR, CR_RXEN);
	wbflush();

	/* flag interface as "running" */
	sc->sc_ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->sc_ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	callout_reset(&sc->sc_timer, hz, sncwatchdog, sc);

	return;
}

/*
 * close down an interface and free its buffers
 * Called on final close of device, or if sncinit() fails
 * part way through.
 */
static int
sncstop(struct snc_softc *sc)
{
	struct mtd *mtd;

	SNC_ASSERT_LOCKED(sc);

	/* stick chip in reset */
	NIC_PUT(sc, SNCR_CR, CR_RST);
	wbflush();

	/* free all receive buffers (currently static so nothing to do) */

	/* free all pending transmit mbufs */
	while (sc->mtd_hw != sc->mtd_free) {
		mtd = &sc->mtda[sc->mtd_hw];
		if (mtd->mtd_mbuf)
			m_freem(mtd->mtd_mbuf);
		if (++sc->mtd_hw == NTDA) sc->mtd_hw = 0;
	}

	callout_stop(&sc->sc_timer);
	sc->sc_tx_timeout = 0;
	sc->sc_ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	return (0);
}

/*
 * Called if any Tx packets remain unsent after 5 seconds,
 * In all cases we just reset the chip, and any retransmission
 * will be handled by higher level protocol timeouts.
 */
static void
sncwatchdog(void *arg)
{
	struct snc_softc *sc = arg;
	struct mtd *mtd;

	SNC_ASSERT_LOCKED(sc);
	if (sc->sc_tx_timeout && --sc->sc_tx_timeout == 0) {
		if (sc->mtd_hw != sc->mtd_free) {
			/* something still pending for transmit */
			mtd = &sc->mtda[sc->mtd_hw];
			if (SRO(sc, mtd->mtd_vtxp, TXP_STATUS) == 0)
				log(LOG_ERR, "%s: Tx - timeout\n",
				    device_get_nameunit(sc->sc_dev));
			else
				log(LOG_ERR, "%s: Tx - lost interrupt\n",
				    device_get_nameunit(sc->sc_dev));
			sncreset(sc);
		}
	}
	callout_reset(&sc->sc_timer, hz, sncwatchdog, sc);
}

/*
 * stuff packet into sonic
 */
static u_int
sonicput(struct snc_softc *sc, struct mbuf *m0, int mtd_next)
{
	struct mtd *mtdp;
	struct mbuf *m;
	u_int32_t buff;
	u_int32_t txp;
	u_int	len = 0;
	u_int	totlen = 0;

#ifdef whyonearthwouldyoudothis
	if (NIC_GET(sc, SNCR_CR) & CR_TXP)
		return (0);
#endif

	/* grab the replacement mtd */
	mtdp = &sc->mtda[sc->mtd_free];

	buff = mtdp->mtd_vbuf;
	
	/* this packet goes to mtdnext fill in the TDA */
	mtdp->mtd_mbuf = m0;
	txp = mtdp->mtd_vtxp;

	/* Write to the config word. Every (NTDA/2)+1 packets we set an intr */
	if (sc->mtd_pint == 0) {
		sc->mtd_pint = NTDA/2;
		SWO(sc, txp, TXP_CONFIG, TCR_PINT);
	} else {
		sc->mtd_pint--;
		SWO(sc, txp, TXP_CONFIG, 0);
	}

	for (m = m0; m; m = m->m_next) {
		len = m->m_len;
		totlen += len;
		(*sc->sc_copytobuf)(sc, mtod(m, caddr_t), buff, len);
		buff += len;
	}
	if (totlen >= TXBSIZE) {
		panic("%s: sonicput: packet overflow",
		      device_get_nameunit(sc->sc_dev));
	}

	SWO(sc, txp, TXP_FRAGOFF + (0 * TXP_FRAGSIZE) + TXP_FPTRLO,
	    LOWER(mtdp->mtd_vbuf));
	SWO(sc, txp, TXP_FRAGOFF + (0 * TXP_FRAGSIZE) + TXP_FPTRHI,
	    UPPER(mtdp->mtd_vbuf));

	if (totlen < ETHERMIN + sizeof(struct ether_header)) {
		int pad = ETHERMIN + sizeof(struct ether_header) - totlen;
		(*sc->sc_zerobuf)(sc, mtdp->mtd_vbuf + totlen, pad);
		totlen = ETHERMIN + sizeof(struct ether_header);
	}

	SWO(sc, txp, TXP_FRAGOFF + (0 * TXP_FRAGSIZE) + TXP_FSIZE,
	    totlen);
	SWO(sc, txp, TXP_FRAGCNT, 1);
	SWO(sc, txp, TXP_PKTSIZE, totlen);

	/* link onto the next mtd that will be used */
	SWO(sc, txp, TXP_FRAGOFF + (1 * TXP_FRAGSIZE) + TXP_FPTRLO,
	    LOWER(sc->mtda[mtd_next].mtd_vtxp) | EOL);

	/*
	 * The previous txp.tlink currently contains a pointer to
	 * our txp | EOL. Want to clear the EOL, so write our
	 * pointer to the previous txp.
	 */
	SWO(sc, sc->mtda[sc->mtd_prev].mtd_vtxp, sc->mtd_tlinko,
	    LOWER(mtdp->mtd_vtxp));

	/* make sure chip is running */
	wbflush();
	NIC_PUT(sc, SNCR_CR, CR_TXP);
	wbflush();

	/* 5 seconds to watch for failing to transmit */
	sc->sc_tx_timeout = 5;

	return (totlen);
}

/*
 * These are called from sonicioctl() when /etc/ifconfig is run to set
 * the address or switch the i/f on.
 */
/*
 * CAM support
 */
static void
caminitialise(struct snc_softc *sc)
{
	u_int32_t v_cda = sc->v_cda;
	int	i;
	int	camoffset;

	for (i = 0; i < MAXCAM; i++) {
		camoffset = i * CDA_CAMDESC;
		SWO(sc, v_cda, (camoffset + CDA_CAMEP), i);
		SWO(sc, v_cda, (camoffset + CDA_CAMAP2), 0);
		SWO(sc, v_cda, (camoffset + CDA_CAMAP1), 0);
		SWO(sc, v_cda, (camoffset + CDA_CAMAP0), 0);
	}
	SWO(sc, v_cda, CDA_ENABLE, 0);

#ifdef SNCDEBUG
	if ((sncdebug & SNC_SHOWCAMENT) != 0) {
		camdump(sc);
	}
#endif
}

static void
camentry(struct snc_softc *sc, int entry, u_char *ea)
{
	u_int32_t v_cda = sc->v_cda;
	int	camoffset = entry * CDA_CAMDESC;

	SWO(sc, v_cda, camoffset + CDA_CAMEP, entry);
	SWO(sc, v_cda, camoffset + CDA_CAMAP2, (ea[5] << 8) | ea[4]);
	SWO(sc, v_cda, camoffset + CDA_CAMAP1, (ea[3] << 8) | ea[2]);
	SWO(sc, v_cda, camoffset + CDA_CAMAP0, (ea[1] << 8) | ea[0]);
	SWO(sc, v_cda, CDA_ENABLE, 
	    (SRO(sc, v_cda, CDA_ENABLE) | (1 << entry)));
}

static void
camprogram(struct snc_softc *sc)
{
        struct ifmultiaddr      *ifma;
	struct ifnet *ifp;
	int	timeout;
	int	mcount = 0;

	caminitialise(sc);

	ifp = sc->sc_ifp;

	/* Always load our own address first. */
	camentry (sc, mcount, IF_LLADDR(sc->sc_ifp));
	mcount++;

	/* Assume we won't need allmulti bit. */
	ifp->if_flags &= ~IFF_ALLMULTI;

	/* Loop through multicast addresses */
	if_maddr_rlock(ifp);
        TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
                if (ifma->ifma_addr->sa_family != AF_LINK)
                        continue;
		if (mcount == MAXCAM) {
			 ifp->if_flags |= IFF_ALLMULTI;
			 break;
		}

		/* program the CAM with the specified entry */
		camentry(sc, mcount,
			 LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		mcount++;
	}
	if_maddr_runlock(ifp);

	NIC_PUT(sc, SNCR_CDP, LOWER(sc->v_cda));
	NIC_PUT(sc, SNCR_CDC, MAXCAM);
	NIC_PUT(sc, SNCR_CR, CR_LCAM);
	wbflush();

	timeout = 10000;
	while ((NIC_GET(sc, SNCR_CR) & CR_LCAM) && timeout--)
		continue;
	if (timeout == 0) {
		/* XXX */
		panic("%s: CAM initialisation failed\n",
		      device_get_nameunit(sc->sc_dev));
	}
	timeout = 10000;
	while (((NIC_GET(sc, SNCR_ISR) & ISR_LCD) == 0) && timeout--)
		continue;

	if (NIC_GET(sc, SNCR_ISR) & ISR_LCD)
		NIC_PUT(sc, SNCR_ISR, ISR_LCD);
	else
		device_printf(sc->sc_dev,
			      "CAM initialisation without interrupt\n");
}

#ifdef SNCDEBUG
static void
camdump(struct snc_softc *sc)
{
	int	i;

	printf("CAM entries:\n");
	NIC_PUT(sc, SNCR_CR, CR_RST);
	wbflush();

	for (i = 0; i < 16; i++) {
		u_short  ap2, ap1, ap0;
		NIC_PUT(sc, SNCR_CEP, i);
		wbflush();
		ap2 = NIC_GET(sc, SNCR_CAP2);
		ap1 = NIC_GET(sc, SNCR_CAP1);
		ap0 = NIC_GET(sc, SNCR_CAP0);
		printf("%d: ap2=0x%x ap1=0x%x ap0=0x%x\n", i, ap2, ap1, ap0);
	}
	printf("CAM enable 0x%x\n", NIC_GET(sc, SNCR_CEP));

	NIC_PUT(sc, SNCR_CR, 0);
	wbflush();
}
#endif

static void
initialise_tda(struct snc_softc *sc)
{
	struct mtd *mtd;
	int	i;

	for (i = 0; i < NTDA; i++) {
		mtd = &sc->mtda[i];
		mtd->mtd_mbuf = 0;
	}

	sc->mtd_hw = 0;
	sc->mtd_prev = NTDA - 1;
	sc->mtd_free = 0;
	sc->mtd_tlinko = TXP_FRAGOFF + 1*TXP_FRAGSIZE + TXP_FPTRLO;
	sc->mtd_pint = NTDA/2;

	NIC_PUT(sc, SNCR_UTDA, UPPER(sc->mtda[0].mtd_vtxp));
	NIC_PUT(sc, SNCR_CTDA, LOWER(sc->mtda[0].mtd_vtxp));
}

static void
initialise_rda(struct snc_softc *sc)
{
	int		i;
	u_int32_t	vv_rda = 0;
	u_int32_t	v_rda = 0;

	/* link the RDA's together into a circular list */
	for (i = 0; i < (sc->sc_nrda - 1); i++) {
		v_rda = sc->v_rda + (i * RXPKT_SIZE(sc));
		vv_rda = sc->v_rda + ((i+1) * RXPKT_SIZE(sc));
		SWO(sc, v_rda, RXPKT_RLINK, LOWER(vv_rda));
		SWO(sc, v_rda, RXPKT_INUSE, 1);
	}
	v_rda = sc->v_rda + ((sc->sc_nrda - 1) * RXPKT_SIZE(sc));
	SWO(sc, v_rda, RXPKT_RLINK, LOWER(sc->v_rda) | EOL);
	SWO(sc, v_rda, RXPKT_INUSE, 1);

	/* mark end of receive descriptor list */
	sc->sc_rdamark = sc->sc_nrda - 1;

	sc->sc_rxmark = 0;

	NIC_PUT(sc, SNCR_URDA, UPPER(sc->v_rda));
	NIC_PUT(sc, SNCR_CRDA, LOWER(sc->v_rda));
	wbflush();
}

static void
initialise_rra(struct snc_softc *sc)
{
	int	i;
	u_int	v;
	int	bitmode = sc->bitmode;

	if (bitmode)
		NIC_PUT(sc, SNCR_EOBC, RBASIZE(sc) / 2 - 2);
	else
		NIC_PUT(sc, SNCR_EOBC, RBASIZE(sc) / 2 - 1);

	NIC_PUT(sc, SNCR_URRA, UPPER(sc->v_rra[0]));
	NIC_PUT(sc, SNCR_RSA, LOWER(sc->v_rra[0]));
	/* rea must point just past the end of the rra space */
	NIC_PUT(sc, SNCR_REA, LOWER(sc->v_rea));
	NIC_PUT(sc, SNCR_RRP, LOWER(sc->v_rra[0]));
	NIC_PUT(sc, SNCR_RSC, 0);

	/* fill up SOME of the rra with buffers */
	for (i = 0; i < NRBA; i++) {
		v = SONIC_GETDMA(sc->rbuf[i]);
		SWO(sc, sc->v_rra[i], RXRSRC_PTRHI, UPPER(v));
		SWO(sc, sc->v_rra[i], RXRSRC_PTRLO, LOWER(v));
		SWO(sc, sc->v_rra[i], RXRSRC_WCHI, UPPER(PAGE_SIZE/2));
		SWO(sc, sc->v_rra[i], RXRSRC_WCLO, LOWER(PAGE_SIZE/2));
	}
	sc->sc_rramark = NRBA;
	NIC_PUT(sc, SNCR_RWP, LOWER(sc->v_rra[sc->sc_rramark]));
	wbflush();
}

void
sncintr(void *arg)
{
	struct snc_softc *sc = (struct snc_softc *)arg;
	int	isr;

	if (sc->sc_enabled == 0)
		return;

	SNC_LOCK(sc);
	while ((isr = (NIC_GET(sc, SNCR_ISR) & ISR_ALL)) != 0) {
		/* scrub the interrupts that we are going to service */
		NIC_PUT(sc, SNCR_ISR, isr);
		wbflush();

		if (isr & (ISR_BR | ISR_LCD | ISR_TC))
			device_printf(sc->sc_dev,
				      "unexpected interrupt status 0x%x\n",
				      isr);

		if (isr & (ISR_TXDN | ISR_TXER | ISR_PINT))
			sonictxint(sc);

		if (isr & ISR_PKTRX)
			sonicrxint(sc);

		if (isr & (ISR_HBL | ISR_RDE | ISR_RBE | ISR_RBAE | ISR_RFO)) {
			if (isr & ISR_HBL)
				/*
				 * The repeater is not providing a heartbeat.
				 * In itself this isn't harmful, lots of the
				 * cheap repeater hubs don't supply a heartbeat.
				 * So ignore the lack of heartbeat. Its only
				 * if we can't detect a carrier that we have a
				 * problem.
				 */
				;
			if (isr & ISR_RDE)
				device_printf(sc->sc_dev, 
					"receive descriptors exhausted\n");
			if (isr & ISR_RBE)
				device_printf(sc->sc_dev, 
					"receive buffers exhausted\n");
			if (isr & ISR_RBAE)
				device_printf(sc->sc_dev, 
					"receive buffer area exhausted\n");
			if (isr & ISR_RFO)
				device_printf(sc->sc_dev, 
					"receive FIFO overrun\n");
		}
		if (isr & (ISR_CRC | ISR_FAE | ISR_MP)) {
#ifdef notdef
			if (isr & ISR_CRC)
				sc->sc_crctally++;
			if (isr & ISR_FAE)
				sc->sc_faetally++;
			if (isr & ISR_MP)
				sc->sc_mptally++;
#endif
		}
		sncstart_locked(sc->sc_ifp);
	}
	SNC_UNLOCK(sc);
	return;
}

/*
 * Transmit interrupt routine
 */
static void
sonictxint(struct snc_softc *sc)
{
	struct mtd	*mtd;
	u_int32_t	txp;
	unsigned short	txp_status;
	int		mtd_hw;
	struct ifnet	*ifp = sc->sc_ifp;

	mtd_hw = sc->mtd_hw;

	if (mtd_hw == sc->mtd_free)
		return;

	while (mtd_hw != sc->mtd_free) {
		mtd = &sc->mtda[mtd_hw];

		txp = mtd->mtd_vtxp;

		if (SRO(sc, txp, TXP_STATUS) == 0) {
			break; /* it hasn't really gone yet */
		}

#ifdef SNCDEBUG
		if ((sncdebug & SNC_SHOWTXHDR) != 0)
		{
			struct ether_header eh;

			(*sc->sc_copyfrombuf)(sc, &eh, mtd->mtd_vbuf, sizeof(eh));
			device_printf(sc->sc_dev,
			    "xmit status=0x%x len=%d type=0x%x from %6D",
			    SRO(sc, txp, TXP_STATUS),
			    SRO(sc, txp, TXP_PKTSIZE),
			    htons(eh.ether_type),
			    eh.ether_shost, ":");
			printf(" (to %6D)\n", eh.ether_dhost, ":");
		}
#endif /* SNCDEBUG */

		sc->sc_tx_timeout = 0;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		if (mtd->mtd_mbuf != 0) {
			m_freem(mtd->mtd_mbuf);
			mtd->mtd_mbuf = 0;
		}
		if (++mtd_hw == NTDA) mtd_hw = 0;

		txp_status = SRO(sc, txp, TXP_STATUS);

		if_inc_counter(ifp, IFCOUNTER_COLLISIONS,
		    (txp_status & TCR_EXC) ? 16 :
		    ((txp_status & TCR_NC) >> 12));

		if ((txp_status & TCR_PTX) == 0) {
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			device_printf(sc->sc_dev, "Tx packet status=0x%x\n",
				      txp_status);
			
			/* XXX - DG This looks bogus */
			if (mtd_hw != sc->mtd_free) {
				printf("resubmitting remaining packets\n");
				mtd = &sc->mtda[mtd_hw];
				NIC_PUT(sc, SNCR_CTDA, LOWER(mtd->mtd_vtxp));
				NIC_PUT(sc, SNCR_CR, CR_TXP);
				wbflush();
				break;
			}
		}
	}

	sc->mtd_hw = mtd_hw;
	return;
}

/*
 * Receive interrupt routine
 */
static void
sonicrxint(struct snc_softc *sc)
{
	u_int32_t rda;
	int	orra;
	int	len;
	int	rramark;
	int	rdamark;
	u_int16_t rxpkt_ptr;

	rda = sc->v_rda + (sc->sc_rxmark * RXPKT_SIZE(sc));

	while (SRO(sc, rda, RXPKT_INUSE) == 0) {
		u_int status = SRO(sc, rda, RXPKT_STATUS);

		orra = RBASEQ(SRO(sc, rda, RXPKT_SEQNO)) & RRAMASK;
		rxpkt_ptr = SRO(sc, rda, RXPKT_PTRLO);
		/*
		 * Do not trunc ether_header length.
		 * Our sonic_read() and sonic_get() require it.
		 */
		len = SRO(sc, rda, RXPKT_BYTEC) - FCSSIZE;
		if (status & RCR_PRX) {
			/* XXX: Does PAGE_MASK require? */
			u_int32_t pkt =
			    sc->rbuf[orra & RBAMASK] + (rxpkt_ptr & PAGE_MASK);
			if (sonic_read(sc, pkt, len))
				if_inc_counter(sc->sc_ifp, IFCOUNTER_IPACKETS, 1);
			else
				if_inc_counter(sc->sc_ifp, IFCOUNTER_IERRORS, 1);
		} else
			if_inc_counter(sc->sc_ifp, IFCOUNTER_IERRORS, 1);

		/*
		 * give receive buffer area back to chip.
		 *
		 * If this was the last packet in the RRA, give the RRA to
		 * the chip again.
		 * If sonic read didnt copy it out then we would have to
		 * wait !!
		 * (dont bother add it back in again straight away)
		 *
		 * Really, we're doing v_rra[rramark] = v_rra[orra] but
		 * we have to use the macros because SONIC might be in
		 * 16 or 32 bit mode.
		 */
		if (status & RCR_LPKT) {
			u_int32_t tmp1, tmp2;

			rramark = sc->sc_rramark;
			tmp1 = sc->v_rra[rramark];
			tmp2 = sc->v_rra[orra];
			SWO(sc, tmp1, RXRSRC_PTRLO,
				SRO(sc, tmp2, RXRSRC_PTRLO));
			SWO(sc, tmp1, RXRSRC_PTRHI,
				SRO(sc, tmp2, RXRSRC_PTRHI));
			SWO(sc, tmp1, RXRSRC_WCLO,
				SRO(sc, tmp2, RXRSRC_WCLO));
			SWO(sc, tmp1, RXRSRC_WCHI,
				SRO(sc, tmp2, RXRSRC_WCHI));

			/* zap old rra for fun */
			SWO(sc, tmp2, RXRSRC_WCHI, 0);
			SWO(sc, tmp2, RXRSRC_WCLO, 0);

			sc->sc_rramark = (++rramark) & RRAMASK;
			NIC_PUT(sc, SNCR_RWP, LOWER(sc->v_rra[rramark]));
			wbflush();
		}

		/*
		 * give receive descriptor back to chip simple
		 * list is circular
		 */
		rdamark = sc->sc_rdamark;
		SWO(sc, rda, RXPKT_INUSE, 1);
		SWO(sc, rda, RXPKT_RLINK,
			SRO(sc, rda, RXPKT_RLINK) | EOL);
		SWO(sc, (sc->v_rda + (rdamark * RXPKT_SIZE(sc))), RXPKT_RLINK,
			SRO(sc, (sc->v_rda + (rdamark * RXPKT_SIZE(sc))),
			RXPKT_RLINK) & ~EOL);
		sc->sc_rdamark = sc->sc_rxmark;

		if (++sc->sc_rxmark >= sc->sc_nrda)
			sc->sc_rxmark = 0;
		rda = sc->v_rda + (sc->sc_rxmark * RXPKT_SIZE(sc));
	}
}

/*
 * sonic_read -- pull packet off interface and forward to
 * appropriate protocol handler
 */
static int
sonic_read(struct snc_softc *sc, u_int32_t pkt, int len)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ether_header *et;
	struct mbuf *m;

	if (len <= sizeof(struct ether_header) ||
	    len > ETHERMTU + sizeof(struct ether_header)) {
		device_printf(sc->sc_dev,
			      "invalid packet length %d bytes\n", len);
		return (0);
	}

	/* Pull packet off interface. */
	m = sonic_get(sc, pkt, len);
	if (m == 0) {
		return (0);
	}

	/* We assume that the header fit entirely in one mbuf. */
	et = mtod(m, struct ether_header *);

#ifdef SNCDEBUG
	if ((sncdebug & SNC_SHOWRXHDR) != 0)
	{
		device_printf(sc->sc_dev, "rcvd 0x%x len=%d type=0x%x from %6D",
		    pkt, len, htons(et->ether_type),
		    et->ether_shost, ":");
		printf(" (to %6D)\n", et->ether_dhost, ":");
	}
#endif /* SNCDEBUG */

	/* Pass the packet up. */
	SNC_UNLOCK(sc);
	(*ifp->if_input)(ifp, m);
	SNC_LOCK(sc);
	return (1);
}


/*
 * munge the received packet into an mbuf chain
 */
static struct mbuf *
sonic_get(struct snc_softc *sc, u_int32_t pkt, int datalen)
{
	struct	mbuf *m, *top, **mp;
	int	len;
	/*
	 * Do not trunc ether_header length.
	 * Our sonic_read() and sonic_get() require it.
	 */

	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = sc->sc_ifp;
	m->m_pkthdr.len = datalen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (datalen > 0) {
		if (top) {
			MGET(m, M_NOWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			len = MLEN;
		}
		if (datalen >= MINCLSIZE) {
			MCLGET(m, M_NOWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				if (top) m_freem(top);
				return (0);
			}
			len = MCLBYTES;
		}
#if 0
		/* XXX: Require? */
		if (!top) {
			register int pad =
			    ALIGN(sizeof(struct ether_header)) -
			        sizeof(struct ether_header);
			m->m_data += pad;
			len -= pad;
		}
#endif
		m->m_len = len = min(datalen, len);

		(*sc->sc_copyfrombuf)(sc, mtod(m, caddr_t), pkt, len);
		pkt += len;
		datalen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}
/*
 * Enable power on the interface.
 */
int
snc_enable(struct snc_softc *sc)
{

#ifdef	SNCDEBUG
	device_printf(sc->sc_dev, "snc_enable()\n");
#endif	/* SNCDEBUG */

	if (sc->sc_enabled == 0 && sc->sc_enable != NULL) {
		if ((*sc->sc_enable)(sc) != 0) {
			device_printf(sc->sc_dev, "device enable failed\n");
			return (EIO);
		}
	}

	sc->sc_enabled = 1;
	return (0);
}

/*
 * Disable power on the interface.
 */
void
snc_disable(struct snc_softc *sc)
{

#ifdef	SNCDEBUG
	device_printf(sc->sc_dev, "snc_disable()\n");
#endif	/* SNCDEBUG */

	if (sc->sc_enabled != 0 && sc->sc_disable != NULL) {
		(*sc->sc_disable)(sc);
		sc->sc_enabled = 0;
	}
}


