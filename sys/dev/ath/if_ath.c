/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/sysctl.h>
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <machine/bus.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

#define	AR_DEBUG
#include <dev/ath/if_athvar.h>
#include <contrib/dev/ath/ah_desc.h>

/* unalligned little endian access */     
#define LE_READ_2(p)							\
	((u_int16_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)							\
	((u_int32_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8) |	\
	  (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24)))

static void	ath_init(void *);
static void	ath_stop(struct ifnet *);
static void	ath_start(struct ifnet *);
static void	ath_reset(struct ath_softc *);
static int	ath_media_change(struct ifnet *);
static void	ath_watchdog(struct ifnet *);
static int	ath_ioctl(struct ifnet *, u_long, caddr_t);
static void	ath_fatal_proc(void *, int);
static void	ath_rxorn_proc(void *, int);
static void	ath_bmiss_proc(void *, int);
static void	ath_initkeytable(struct ath_softc *);
static void	ath_mode_init(struct ath_softc *);
static int	ath_beacon_alloc(struct ath_softc *, struct ieee80211_node *);
static void	ath_beacon_proc(void *, int);
static void	ath_beacon_free(struct ath_softc *);
static void	ath_beacon_config(struct ath_softc *);
static int	ath_desc_alloc(struct ath_softc *);
static void	ath_desc_free(struct ath_softc *);
static struct ieee80211_node *ath_node_alloc(struct ieee80211com *);
static void	ath_node_free(struct ieee80211com *, struct ieee80211_node *);
static void	ath_node_copy(struct ieee80211com *,
			struct ieee80211_node *, const struct ieee80211_node *);
static u_int8_t	ath_node_getrssi(struct ieee80211com *,
			struct ieee80211_node *);
static int	ath_rxbuf_init(struct ath_softc *, struct ath_buf *);
static void	ath_rx_proc(void *, int);
static int	ath_tx_start(struct ath_softc *, struct ieee80211_node *,
			     struct ath_buf *, struct mbuf *);
static void	ath_tx_proc(void *, int);
static int	ath_chan_set(struct ath_softc *, struct ieee80211_channel *);
static void	ath_draintxq(struct ath_softc *);
static void	ath_stoprecv(struct ath_softc *);
static int	ath_startrecv(struct ath_softc *);
static void	ath_next_scan(void *);
static void	ath_calibrate(void *);
static int	ath_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void	ath_newassoc(struct ieee80211com *,
			struct ieee80211_node *, int);
static int	ath_getchannels(struct ath_softc *, u_int cc, HAL_BOOL outdoor);

static int	ath_rate_setup(struct ath_softc *sc, u_int mode);
static void	ath_setcurmode(struct ath_softc *, enum ieee80211_phymode);
static void	ath_rate_ctl_reset(struct ath_softc *, enum ieee80211_state);
static void	ath_rate_ctl(void *, struct ieee80211_node *);

SYSCTL_DECL(_hw_ath);

/* XXX validate sysctl values */
static	int ath_dwelltime = 200;		/* 5 channels/second */
SYSCTL_INT(_hw_ath, OID_AUTO, dwell, CTLFLAG_RW, &ath_dwelltime,
	    0, "channel dwell time (ms) for AP/station scanning");
static	int ath_calinterval = 30;		/* calibrate every 30 secs */
SYSCTL_INT(_hw_ath, OID_AUTO, calibrate, CTLFLAG_RW, &ath_calinterval,
	    0, "chip calibration interval (secs)");
static	int ath_outdoor = AH_TRUE;		/* outdoor operation */
SYSCTL_INT(_hw_ath, OID_AUTO, outdoor, CTLFLAG_RD, &ath_outdoor,
	    0, "enable/disable outdoor operation");
static	int ath_countrycode = CTRY_DEFAULT;	/* country code */
SYSCTL_INT(_hw_ath, OID_AUTO, countrycode, CTLFLAG_RD, &ath_countrycode,
	    0, "country code");
static	int ath_regdomain = 0;			/* regulatory domain */
SYSCTL_INT(_hw_ath, OID_AUTO, regdomain, CTLFLAG_RD, &ath_regdomain,
	    0, "regulatory domain");

#ifdef AR_DEBUG
int	ath_debug = 0;
SYSCTL_INT(_hw_ath, OID_AUTO, debug, CTLFLAG_RW, &ath_debug,
	    0, "control debugging printfs");
#define	IFF_DUMPPKTS(_ifp) \
	(ath_debug || \
	    ((_ifp)->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
static	void ath_printrxbuf(struct ath_buf *bf, int);
static	void ath_printtxbuf(struct ath_buf *bf, int);
#define	DPRINTF(X)	if (ath_debug) printf X
#define	DPRINTF2(X)	if (ath_debug > 1) printf X
#else
#define	IFF_DUMPPKTS(_ifp) \
	(((_ifp)->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define	DPRINTF(X)
#define	DPRINTF2(X)
#endif

int
ath_attach(u_int16_t devid, struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah;
	HAL_STATUS status;
	int error = 0;

	DPRINTF(("ath_attach: devid 0x%x\n", devid));

	/* set these up early for if_printf use */
	if_initname(ifp, device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev));

	ah = ath_hal_attach(devid, sc, sc->sc_st, sc->sc_sh, &status);
	if (ah == NULL) {
		if_printf(ifp, "unable to attach hardware; HAL status %u\n",
			status);
		error = ENXIO;
		goto bad;
	}
	if (ah->ah_abi != HAL_ABI_VERSION) {
		if_printf(ifp, "HAL ABI mismatch detected (0x%x != 0x%x)\n",
			ah->ah_abi, HAL_ABI_VERSION);
		error = ENXIO;
		goto bad;
	}
	if_printf(ifp, "mac %d.%d phy %d.%d",
		ah->ah_macVersion, ah->ah_macRev,
		ah->ah_phyRev >> 4, ah->ah_phyRev & 0xf);
	if (ah->ah_analog5GhzRev)
		printf(" 5ghz radio %d.%d",
			ah->ah_analog5GhzRev >> 4, ah->ah_analog5GhzRev & 0xf);
	if (ah->ah_analog2GhzRev)
		printf(" 2ghz radio %d.%d",
			ah->ah_analog2GhzRev >> 4, ah->ah_analog2GhzRev & 0xf);
	printf("\n");
	sc->sc_ah = ah;
	sc->sc_invalid = 0;	/* ready to go, enable interrupt handling */

	/*
	 * Collect the channel list using the default country
	 * code and including outdoor channels.  The 802.11 layer
	 * is resposible for filtering this list based on settings
	 * like the phy mode.
	 */
	error = ath_getchannels(sc, ath_countrycode, ath_outdoor);
	if (error != 0)
		goto bad;
	/*
	 * Copy these back; they are set as a side effect
	 * of constructing the channel list.
	 */
	ath_regdomain = ath_hal_getregdomain(ah);
	ath_countrycode = ath_hal_getcountrycode(ah);

	/*
	 * Setup rate tables for all potential media types.
	 */
	ath_rate_setup(sc, IEEE80211_MODE_11A);
	ath_rate_setup(sc, IEEE80211_MODE_11B);
	ath_rate_setup(sc, IEEE80211_MODE_11G);
	ath_rate_setup(sc, IEEE80211_MODE_TURBO);

	error = ath_desc_alloc(sc);
	if (error != 0) {
		if_printf(ifp, "failed to allocate descriptors: %d\n", error);
		goto bad;
	}
	callout_init(&sc->sc_scan_ch, CALLOUT_MPSAFE);
	callout_init(&sc->sc_cal_ch, CALLOUT_MPSAFE);

	ATH_TXBUF_LOCK_INIT(sc);
	ATH_TXQ_LOCK_INIT(sc);

	TASK_INIT(&sc->sc_txtask, 0, ath_tx_proc, sc);
	TASK_INIT(&sc->sc_rxtask, 0, ath_rx_proc, sc);
	TASK_INIT(&sc->sc_swbatask, 0, ath_beacon_proc, sc);
	TASK_INIT(&sc->sc_rxorntask, 0, ath_rxorn_proc, sc);
	TASK_INIT(&sc->sc_fataltask, 0, ath_fatal_proc, sc);
	TASK_INIT(&sc->sc_bmisstask, 0, ath_bmiss_proc, sc);

	/*
	 * For now just pre-allocate one data queue and one
	 * beacon queue.  Note that the HAL handles resetting
	 * them at the needed time.  Eventually we'll want to
	 * allocate more tx queues for splitting management
	 * frames and for QOS support.
	 */
	sc->sc_txhalq = ath_hal_setuptxqueue(ah,
		HAL_TX_QUEUE_DATA,
		AH_TRUE			/* enable interrupts */
	);
	if (sc->sc_txhalq == (u_int) -1) {
		if_printf(ifp, "unable to setup a data xmit queue!\n");
		goto bad;
	}
	sc->sc_bhalq = ath_hal_setuptxqueue(ah,
		HAL_TX_QUEUE_BEACON,
		AH_TRUE			/* enable interrupts */
	);
	if (sc->sc_bhalq == (u_int) -1) {
		if_printf(ifp, "unable to setup a beacon xmit queue!\n");
		goto bad;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_start = ath_start;
	ifp->if_watchdog = ath_watchdog;
	ifp->if_ioctl = ath_ioctl;
	ifp->if_init = ath_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	ic->ic_softc = sc;
	ic->ic_newassoc = ath_newassoc;
	/* XXX not right but it's not used anywhere important */
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps = IEEE80211_C_WEP		/* wep supported */
		| IEEE80211_C_IBSS		/* ibss, nee adhoc, mode */
		| IEEE80211_C_HOSTAP		/* hostap mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_RCVMGT;		/* recv management frames */

	/* get mac address from hardware */
	ath_hal_getmac(ah, ic->ic_myaddr);

	/* call MI attach routine. */
	ieee80211_ifattach(ifp);
	/* override default methods */
	ic->ic_node_alloc = ath_node_alloc;
	ic->ic_node_free = ath_node_free;
	ic->ic_node_copy = ath_node_copy;
	ic->ic_node_getrssi = ath_node_getrssi;
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ath_newstate;
	/* complete initialization */
	ieee80211_media_init(ifp, ath_media_change, ieee80211_media_status);

	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
		sizeof(struct ieee80211_frame) + sizeof(sc->sc_tx_th),
		&sc->sc_drvbpf);
	/*
	 * Initialize constant fields.
	 *
	 * NB: the channel is setup each time we transition to the
	 *     RUN state to avoid filling it in for each frame.
	 */
	sc->sc_tx_th.wt_ihdr.it_len = sizeof(sc->sc_tx_th);
	sc->sc_tx_th.wt_ihdr.it_present = ATH_TX_RADIOTAP_PRESENT;

	sc->sc_rx_th.wr_ihdr.it_len = sizeof(sc->sc_rx_th);
	sc->sc_rx_th.wr_ihdr.it_present = ATH_RX_RADIOTAP_PRESENT;

	if_printf(ifp, "802.11 address: %s\n", ether_sprintf(ic->ic_myaddr));

	return 0;
bad:
	if (ah)
		ath_hal_detach(ah);
	sc->sc_invalid = 1;
	return error;
}

int
ath_detach(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	DPRINTF(("ath_detach: if_flags %x\n", ifp->if_flags));

	ath_stop(ifp);
	bpfdetach(ifp);
	ath_desc_free(sc);
	ath_hal_detach(sc->sc_ah);
	ieee80211_ifdetach(ifp);

	ATH_TXBUF_LOCK_DESTROY(sc);
	ATH_TXQ_LOCK_DESTROY(sc);

	return 0;
}

void
ath_suspend(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	DPRINTF(("ath_suspend: if_flags %x\n", ifp->if_flags));

	ath_stop(ifp);
}

void
ath_resume(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	DPRINTF(("ath_resume: if_flags %x\n", ifp->if_flags));

	if (ifp->if_flags & IFF_UP) {
		ath_init(ifp);
		if (ifp->if_flags & IFF_RUNNING)
			ath_start(ifp);
	}
}

void
ath_shutdown(struct ath_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	DPRINTF(("ath_shutdown: if_flags %x\n", ifp->if_flags));

	ath_stop(ifp);
}

void
ath_intr(void *arg)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	HAL_INT status;

	if (sc->sc_invalid) {
		/*
		 * The hardware is not ready/present, don't touch anything.
		 * Note this can happen early on if the IRQ is shared.
		 */
		DPRINTF(("ath_intr: invalid; ignored\n"));
		return;
	}
	if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP)) {
		DPRINTF(("ath_intr: if_flags 0x%x\n", ifp->if_flags));
		ath_hal_getisr(ah, &status);	/* clear ISR */
		ath_hal_intrset(ah, 0);		/* disable further intr's */
		return;
	}
	ath_hal_getisr(ah, &status);		/* NB: clears ISR too */
	DPRINTF2(("ath_intr: status 0x%x\n", status));
#ifdef AR_DEBUG
	if (ath_debug &&
	    (status & (HAL_INT_FATAL|HAL_INT_RXORN|HAL_INT_BMISS))) {
		if_printf(ifp, "ath_intr: status 0x%x\n", status);
		ath_hal_dumpstate(ah);
	}
#endif /* AR_DEBUG */
	status &= sc->sc_imask;			/* discard unasked for bits */
	if (status & HAL_INT_FATAL) {
		sc->sc_stats.ast_hardware++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		taskqueue_enqueue(taskqueue_swi, &sc->sc_fataltask);
	} else if (status & HAL_INT_RXORN) {
		sc->sc_stats.ast_rxorn++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		taskqueue_enqueue(taskqueue_swi, &sc->sc_rxorntask);
	} else {
		if (status & HAL_INT_RXEOL) {
			/*
			 * NB: the hardware should re-read the link when
			 *     RXE bit is written, but it doesn't work at
			 *     least on older hardware revs.
			 */
			sc->sc_stats.ast_rxeol++;
			sc->sc_rxlink = NULL;
		}
		if (status & HAL_INT_TXURN) {
			sc->sc_stats.ast_txurn++;
			/* bump tx trigger level */
			ath_hal_updatetxtriglevel(ah, AH_TRUE);
		}
		if (status & HAL_INT_RX)
			taskqueue_enqueue(taskqueue_swi, &sc->sc_rxtask);
		if (status & HAL_INT_TX)
			taskqueue_enqueue(taskqueue_swi, &sc->sc_txtask);
		if (status & HAL_INT_SWBA)
			taskqueue_enqueue(taskqueue_swi, &sc->sc_swbatask);
		if (status & HAL_INT_BMISS) {
			sc->sc_stats.ast_bmiss++;
			taskqueue_enqueue(taskqueue_swi, &sc->sc_bmisstask);
		}
	}
}

static void
ath_fatal_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;

	device_printf(sc->sc_dev, "hardware error; resetting\n");
	ath_reset(sc);
}

static void
ath_rxorn_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;

	device_printf(sc->sc_dev, "rx FIFO overrun; resetting\n");
	ath_reset(sc);
}

static void
ath_bmiss_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(("ath_bmiss_proc: pending %u\n", pending));
	KASSERT(ic->ic_opmode == IEEE80211_M_STA,
		("unexpect operating mode %u", ic->ic_opmode));
	if (ic->ic_state == IEEE80211_S_RUN) {
		/*
		 * Rather than go directly to scan state, try to
		 * reassociate first.  If that fails then the state
		 * machine will drop us into scanning after timing
		 * out waiting for a probe response.
		 */
		ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
	}
}

static u_int
ath_chan2flags(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	static const u_int modeflags[] = {
		0,			/* IEEE80211_MODE_AUTO */
		CHANNEL_A,		/* IEEE80211_MODE_11A */
		CHANNEL_B,		/* IEEE80211_MODE_11B */
		CHANNEL_PUREG,		/* IEEE80211_MODE_11G */
		CHANNEL_T		/* IEEE80211_MODE_TURBO */
	};
	return modeflags[ieee80211_chan2mode(ic, chan)];
}

static void
ath_init(void *arg)
{
	struct ath_softc *sc = (struct ath_softc *) arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_node *ni;
	enum ieee80211_phymode mode;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;
	HAL_CHANNEL hchan;

	DPRINTF(("ath_init: if_flags 0x%x\n", ifp->if_flags));

	ATH_LOCK(sc);
	/*
	 * Stop anything previously setup.  This is safe
	 * whether this is the first time through or not.
	 */
	ath_stop(ifp);

	/*
	 * The basic interface to setting the hardware in a good
	 * state is ``reset''.  On return the hardware is known to
	 * be powered up and with interrupts disabled.  This must
	 * be followed by initialization of the appropriate bits
	 * and then setup of the interrupt mask.
	 */
	hchan.channel = ic->ic_ibss_chan->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, ic->ic_ibss_chan);
	if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_FALSE, &status)) {
		if_printf(ifp, "unable to reset hardware; hal status %u\n",
			status);
		goto done;
	}

	/*
	 * Setup the hardware after reset: the key cache
	 * is filled as needed and the receive engine is
	 * set going.  Frame transmit is handled entirely
	 * in the frame output path; there's nothing to do
	 * here except setup the interrupt mask.
	 */
	if (ic->ic_flags & IEEE80211_F_WEPON)
		ath_initkeytable(sc);
	if (ath_startrecv(sc) != 0) {
		if_printf(ifp, "unable to start recv logic\n");
		goto done;
	}

	/*
	 * Enable interrupts.
	 */
	sc->sc_imask = HAL_INT_RX | HAL_INT_TX
		  | HAL_INT_RXEOL | HAL_INT_RXORN
		  | HAL_INT_FATAL | HAL_INT_GLOBAL;
	ath_hal_intrset(ah, sc->sc_imask);

	ifp->if_flags |= IFF_RUNNING;
	ic->ic_state = IEEE80211_S_INIT;

	/*
	 * The hardware should be ready to go now so it's safe
	 * to kick the 802.11 state machine as it's likely to
	 * immediately call back to us to send mgmt frames.
	 */
	ni = ic->ic_bss;
	ni->ni_chan = ic->ic_ibss_chan;
	mode = ieee80211_chan2mode(ic, ni->ni_chan);
	if (mode != sc->sc_curmode)
		ath_setcurmode(sc, mode);
	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
done:
	ATH_UNLOCK(sc);
}

static void
ath_stop(struct ifnet *ifp)
{
	struct ieee80211com *ic = (struct ieee80211com *) ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(("ath_stop: invalid %u if_flags 0x%x\n",
		sc->sc_invalid, ifp->if_flags));

	ATH_LOCK(sc);
	if (ifp->if_flags & IFF_RUNNING) {
		/*
		 * Shutdown the hardware and driver:
		 *    disable interrupts
		 *    turn off timers
		 *    clear transmit machinery
		 *    clear receive machinery
		 *    drain and release tx queues
		 *    reclaim beacon resources
		 *    reset 802.11 state machine
		 *    power down hardware
		 *
		 * Note that some of this work is not possible if the
		 * hardware is gone (invalid).
		 */
		ifp->if_flags &= ~IFF_RUNNING;
		ifp->if_timer = 0;
		if (!sc->sc_invalid)
			ath_hal_intrset(ah, 0);
		ath_draintxq(sc);
		if (!sc->sc_invalid)
			ath_stoprecv(sc);
		else
			sc->sc_rxlink = NULL;
		IF_DRAIN(&ifp->if_snd);
		ath_beacon_free(sc);
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		if (!sc->sc_invalid)
			ath_hal_setpower(ah, HAL_PM_FULL_SLEEP, 0);
	}
	ATH_UNLOCK(sc);
}

/*
 * Reset the hardware w/o losing operational state.  This is
 * basically a more efficient way of doing ath_stop, ath_init,
 * followed by state transitions to the current 802.11
 * operational state.  Used to recover from errors rx overrun
 * and to reset the hardware when rf gain settings must be reset.
 */
static void
ath_reset(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_channel *c;
	HAL_STATUS status;
	HAL_CHANNEL hchan;

	/*
	 * Convert to a HAL channel description with the flags
	 * constrained to reflect the current operating mode.
	 */
	c = ic->ic_ibss_chan;
	hchan.channel = c->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, c);

	ath_hal_intrset(ah, 0);		/* disable interrupts */
	ath_draintxq(sc);		/* stop xmit side */
	ath_stoprecv(sc);		/* stop recv side */
	/* NB: indicate channel change so we do a full reset */
	if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_TRUE, &status))
		if_printf(ifp, "%s: unable to reset hardware; hal status %u\n",
			__func__, status);
	ath_hal_intrset(ah, sc->sc_imask);
	if (ath_startrecv(sc) != 0)	/* restart recv */
		if_printf(ifp, "%s: unable to start recv logic\n", __func__);
	ath_start(ifp);			/* restart xmit */
	if (ic->ic_state == IEEE80211_S_RUN)
		ath_beacon_config(sc);	/* restart beacons */
}

static void
ath_start(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct mbuf *m;
	struct ieee80211_frame *wh;

	if ((ifp->if_flags & IFF_RUNNING) == 0 || sc->sc_invalid)
		return;
	for (;;) {
		/*
		 * Grab a TX buffer and associated resources.
		 */
		ATH_TXBUF_LOCK(sc);
		bf = TAILQ_FIRST(&sc->sc_txbuf);
		if (bf != NULL)
			TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
		if (bf == NULL) {
			DPRINTF(("ath_start: out of xmit buffers\n"));
			sc->sc_stats.ast_tx_qstop++;
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		/*
		 * Poll the management queue for frames; they
		 * have priority over normal data frames.
		 */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m == NULL) {
			/*
			 * No data frames go out unless we're associated.
			 */
			if (ic->ic_state != IEEE80211_S_RUN) {
				DPRINTF(("ath_start: ignore data packet, "
					"state %u\n", ic->ic_state));
				sc->sc_stats.ast_tx_discard++;
				ATH_TXBUF_LOCK(sc);
				TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK(sc);
				break;
			}
			IF_DEQUEUE(&ifp->if_snd, m);
			if (m == NULL) {
				ATH_TXBUF_LOCK(sc);
				TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
				ATH_TXBUF_UNLOCK(sc);
				break;
			}
			ifp->if_opackets++;
			BPF_MTAP(ifp, m);
			/*
			 * Encapsulate the packet in prep for transmission.
			 */
			m = ieee80211_encap(ifp, m, &ni);
			if (m == NULL) {
				DPRINTF(("ath_start: encapsulation failure\n"));
				sc->sc_stats.ast_tx_encap++;
				goto bad;
			}
			wh = mtod(m, struct ieee80211_frame *);
			if (ic->ic_flags & IEEE80211_F_WEPON)
				wh->i_fc[1] |= IEEE80211_FC1_WEP;
		} else {
			/*
			 * Hack!  The referenced node pointer is in the
			 * rcvif field of the packet header.  This is
			 * placed there by ieee80211_mgmt_output because
			 * we need to hold the reference with the frame
			 * and there's no other way (other than packet
			 * tags which we consider too expensive to use)
			 * to pass it along.
			 */
			ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
			m->m_pkthdr.rcvif = NULL;

			wh = mtod(m, struct ieee80211_frame *);
			if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
			    IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
				/* fill time stamp */
				u_int64_t tsf;
				u_int32_t *tstamp;

				tsf = ath_hal_gettsf64(ah);
				/* XXX: adjust 100us delay to xmit */
				tsf += 100;
				tstamp = (u_int32_t *)&wh[1];
				tstamp[0] = htole32(tsf & 0xffffffff);
				tstamp[1] = htole32(tsf >> 32);
			}
			sc->sc_stats.ast_tx_mgmt++;
		}
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);

		if (sc->sc_drvbpf) {
			struct mbuf *mb;

			MGETHDR(mb, M_DONTWAIT, m->m_type);
			if (mb != NULL) {
				sc->sc_tx_th.wt_rate =
					ni->ni_rates.rs_rates[ni->ni_txrate];

				mb->m_next = m;
				mb->m_data = (caddr_t)&sc->sc_tx_th;
				mb->m_len = sizeof(sc->sc_tx_th);
				mb->m_pkthdr.len += mb->m_len;
				bpf_mtap(sc->sc_drvbpf, mb);
				m_free(mb);
			}
		}

		if (ath_tx_start(sc, ni, bf, m)) {
	bad:
			ATH_TXBUF_LOCK(sc);
			TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
			ATH_TXBUF_UNLOCK(sc);
			ifp->if_oerrors++;
			if (ni && ni != ic->ic_bss)
				ieee80211_free_node(ic, ni);
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

static int
ath_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) ==
		    (IFF_RUNNING|IFF_UP))
			ath_init(ifp);		/* XXX lose error */
		error = 0;
	}
	return error;
}

static void
ath_watchdog(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	ifp->if_timer = 0;
	if ((ifp->if_flags & IFF_RUNNING) == 0 || sc->sc_invalid)
		return;
	if (sc->sc_tx_timer) {
		if (--sc->sc_tx_timer == 0) {
			if_printf(ifp, "device timeout\n");
#ifdef AR_DEBUG
			if (ath_debug)
				ath_hal_dumpstate(sc->sc_ah);
#endif /* AR_DEBUG */
			ath_init(ifp);		/* XXX ath_reset??? */
			ifp->if_oerrors++;
			sc->sc_stats.ast_watchdog++;
			return;
		}
		ifp->if_timer = 1;
	}
	if (ic->ic_fixed_rate == -1) {
		/*
		 * Run the rate control algorithm if we're not
		 * locked at a fixed rate.
		 */
		if (ic->ic_opmode == IEEE80211_M_STA)
			ath_rate_ctl(sc, ic->ic_bss);
		else
			ieee80211_iterate_nodes(ic, ath_rate_ctl, sc);
	}
	ieee80211_watchdog(ifp);
}

static int
ath_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	ATH_LOCK(sc);
	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING) {
				/*
				 * To avoid rescanning another access point,
				 * do not call ath_init() here.  Instead,
				 * only reflect promisc mode settings.
				 */
				ath_mode_init(sc);
			} else {
				/*
				 * Beware of being called during detach to
				 * reset promiscuous mode.  In that case we
				 * will still be marked UP but not RUNNING.
				 * However trying to re-init the interface
				 * is the wrong thing to do as we've already
				 * torn down much of our state.  There's
				 * probably a better way to deal with this.
				 */
				if (!sc->sc_invalid)
					ath_init(ifp);	/* XXX lose error */
			}
		} else
			ath_stop(ifp);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/*
		 * The upper layer has already installed/removed
		 * the multicast address(es), just recalculate the
		 * multicast filter for the card.
		 */
		if (ifp->if_flags & IFF_RUNNING)
			ath_mode_init(sc);
		break;
	case SIOCGATHSTATS:
		copyout(&sc->sc_stats, ifr->ifr_data, sizeof (sc->sc_stats));
		break;
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) ==
			    (IFF_RUNNING|IFF_UP))
				ath_init(ifp);		/* XXX lose error */
			error = 0;
		}
		break;
	}
	ATH_UNLOCK(sc);
	return error;
}

/*
 * Fill the hardware key cache with key entries.
 */
static void
ath_initkeytable(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	int i;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		struct ieee80211_wepkey *k = &ic->ic_nw_keys[i];
		if (k->wk_len == 0)
			ath_hal_keyreset(ah, i);
		else
			/* XXX return value */
			/* NB: this uses HAL_KEYVAL == ieee80211_wepkey */
			ath_hal_keyset(ah, i, (const HAL_KEYVAL *) k);
	}
}

/*
 * Calculate the receive filter according to the
 * operating mode and state:
 *
 * o always accept unicast, broadcast, and multicast traffic
 * o maintain current state of phy error reception
 * o probe request frames are accepted only when operating in
 *   hostap, adhoc, or monitor modes
 * o enable promiscuous mode according to the interface state
 * o accept beacons:
 *   - when operating in adhoc mode so the 802.11 layer creates
 *     node table entries for peers,
 *   - when operating in station mode for collecting rssi data when
 *     the station is otherwise quiet, or
 *   - when scanning
 */
static u_int32_t
ath_calcrxfilter(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &ic->ic_if;
	u_int32_t rfilt;

	rfilt = (ath_hal_getrxfilter(ah) & HAL_RX_FILTER_PHYERR)
	      | HAL_RX_FILTER_UCAST | HAL_RX_FILTER_BCAST | HAL_RX_FILTER_MCAST;
	if (ic->ic_opmode != IEEE80211_M_STA)
		rfilt |= HAL_RX_FILTER_PROBEREQ;
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    (ifp->if_flags & IFF_PROMISC))
		rfilt |= HAL_RX_FILTER_PROM;
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_IBSS ||
	    ic->ic_state == IEEE80211_S_SCAN)
		rfilt |= HAL_RX_FILTER_BEACON;
	return rfilt;
}

static void
ath_mode_init(struct ath_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &ic->ic_if;
	u_int32_t rfilt, mfilt[2], val;
	u_int8_t pos;
	struct ifmultiaddr *ifma;

	/* configure rx filter */
	rfilt = ath_calcrxfilter(sc);
	ath_hal_setrxfilter(ah, rfilt);

	/* configure operational mode */
	ath_hal_setopmode(ah, ic->ic_opmode);

	/* calculate and install multicast filter */
	if ((ifp->if_flags & IFF_ALLMULTI) == 0) {
		mfilt[0] = mfilt[1] = 0;
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			caddr_t dl;

			/* calculate XOR of eight 6bit values */
			dl = LLADDR((struct sockaddr_dl *) ifma->ifma_addr);
			val = LE_READ_4(dl + 0);
			pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
			val = LE_READ_4(dl + 3);
			pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
			pos &= 0x3f;
			mfilt[pos / 32] |= (1 << (pos % 32));
		}
	} else {
		mfilt[0] = mfilt[1] = ~0;
	}
	ath_hal_setmcastfilter(ah, mfilt[0], mfilt[1]);
	DPRINTF(("ath_mode_init: RX filter 0x%x, MC filter %08x:%08x\n",
		rfilt, mfilt[0], mfilt[1]));
}

static void
ath_mbuf_load_cb(void *arg, bus_dma_segment_t *seg, int nseg, bus_size_t mapsize, int error)
{
	struct ath_buf *bf = arg;

	KASSERT(nseg <= ATH_MAX_SCATTER,
		("ath_mbuf_load_cb: too many DMA segments %u", nseg));
	bf->bf_mapsize = mapsize;
	bf->bf_nseg = nseg;
	bcopy(seg, bf->bf_segs, nseg * sizeof (seg[0]));
}

static int
ath_beacon_alloc(struct ath_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_frame *wh;
	struct ath_buf *bf;
	struct ath_desc *ds;
	struct mbuf *m;
	int error, pktlen;
	u_int8_t *frm, rate;
	u_int16_t capinfo;
	struct ieee80211_rateset *rs;
	const HAL_RATE_TABLE *rt;

	bf = sc->sc_bcbuf;
	if (bf->bf_m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;
	}
	/*
	 * NB: the beacon data buffer must be 32-bit aligned;
	 * we assume the mbuf routines will return us something
	 * with this alignment (perhaps should assert).
	 */
	rs = &ni->ni_rates;
	pktlen = sizeof (struct ieee80211_frame)
	       + 8 + 2 + 2 + 2+ni->ni_esslen + 2+rs->rs_nrates + 3 + 6;
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		pktlen += 2;
	if (pktlen <= MHLEN)
		MGETHDR(m, M_DONTWAIT, MT_DATA);
	else
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		DPRINTF(("ath_beacon_alloc: cannot get mbuf/cluster; size %u\n",
			pktlen));
		sc->sc_stats.ast_be_nombuf++;
		return ENOMEM;
	}

	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_BEACON;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	memcpy(wh->i_addr1, ifp->if_broadcastaddr, IEEE80211_ADDR_LEN);
	memcpy(wh->i_addr2, ic->ic_myaddr, IEEE80211_ADDR_LEN);
	memcpy(wh->i_addr3, ni->ni_bssid, IEEE80211_ADDR_LEN);
	*(u_int16_t *)wh->i_seq = 0;

	/*
	 * beacon frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] parameter set (IBSS)
	 *	[tlv] extended supported rates
	 */
	frm = (u_int8_t *)&wh[1];
	memset(frm, 0, 8);	/* timestamp is set by hardware */
	frm += 8;
	*(u_int16_t *)frm = htole16(ni->ni_intval);
	frm += 2;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = ni->ni_esslen;
	memcpy(frm, ni->ni_essid, ni->ni_esslen);
	frm += ni->ni_esslen;
	frm = ieee80211_add_rates(frm, rs);
	*frm++ = IEEE80211_ELEMID_DSPARMS;
	*frm++ = 1;
	*frm++ = ieee80211_chan2ieee(ic, ni->ni_chan);
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
	} else {
		/* TODO: TIM */
		*frm++ = IEEE80211_ELEMID_TIM;
		*frm++ = 4;	/* length */
		*frm++ = 0;	/* DTIM count */ 
		*frm++ = 1;	/* DTIM period */
		*frm++ = 0;	/* bitmap control */
		*frm++ = 0;	/* Partial Virtual Bitmap (variable length) */
	}
	frm = ieee80211_add_xrates(frm, rs);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
	KASSERT(m->m_pkthdr.len <= pktlen,
		("beacon bigger than expected, len %u calculated %u",
		m->m_pkthdr.len, pktlen));

	DPRINTF2(("ath_beacon_alloc: m %p len %u\n", m, m->m_len));
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m,
				     ath_mbuf_load_cb, bf,
				     BUS_DMA_NOWAIT);
	if (error != 0) {
		m_freem(m);
		return error;
	}
	KASSERT(bf->bf_nseg == 1,
		("ath_beacon_alloc: multi-segment packet; nseg %u",
		bf->bf_nseg));
	bf->bf_m = m;

	/* setup descriptors */
	ds = bf->bf_desc;

	ds->ds_link = 0;
	ds->ds_data = bf->bf_segs[0].ds_addr;
	/*
	 * Calculate rate code.
	 * XXX everything at min xmit rate
	 */
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		rate = rt->info[0].rateCode | rt->info[0].shortPreamble;
	else
		rate = rt->info[0].rateCode;
	ath_hal_setuptxdesc(ah, ds
		, m->m_pkthdr.len + IEEE80211_CRC_LEN	/* packet length */
		, sizeof(struct ieee80211_frame)	/* header length */
		, HAL_PKT_TYPE_BEACON		/* Atheros packet type */
		, 0x20				/* txpower XXX */
		, rate, 1			/* series 0 rate/tries */
		, HAL_TXKEYIX_INVALID		/* no encryption */
		, 0				/* antenna mode */
		, HAL_TXDESC_NOACK		/* no ack for beacons */
		, 0				/* rts/cts rate */
		, 0				/* rts/cts duration */
	);
	/* NB: beacon's BufLen must be a multiple of 4 bytes */
	/* XXX verify mbuf data area covers this roundup */
	ath_hal_filltxdesc(ah, ds
		, roundup(bf->bf_segs[0].ds_len, 4)	/* buffer length */
		, AH_TRUE				/* first segment */
		, AH_TRUE				/* last segment */
	);

	return 0;
}

static void
ath_beacon_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_buf *bf = sc->sc_bcbuf;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF2(("%s: pending %u\n", __func__, pending));
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    bf == NULL || bf->bf_m == NULL) {
		DPRINTF(("%s: ic_flags=%x bf=%p bf_m=%p\n",
			__func__, ic->ic_flags, bf, bf ? bf->bf_m : NULL));
		return;
	}
	/* TODO: update beacon to reflect PS poll state */
	if (!ath_hal_stoptxdma(ah, sc->sc_bhalq)) {
		DPRINTF(("%s: beacon queue %u did not stop?",
			__func__, sc->sc_bhalq));
		return;			/* busy, XXX is this right? */
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREWRITE);

	ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
	ath_hal_txstart(ah, sc->sc_bhalq);
	DPRINTF2(("%s: TXDP%u = %p (%p)\n", __func__,
		sc->sc_bhalq, (caddr_t)bf->bf_daddr, bf->bf_desc));
}

static void
ath_beacon_free(struct ath_softc *sc)
{
	struct ath_buf *bf = sc->sc_bcbuf;

	if (bf->bf_m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;
	}
}

/*
 * Configure the beacon and sleep timers.
 *
 * When operating as an AP this resets the TSF and sets
 * up the hardware to notify us when we need to issue beacons.
 *
 * When operating in station mode this sets up the beacon
 * timers according to the timestamp of the last received
 * beacon and the current TSF, configures PCF and DTIM
 * handling, programs the sleep registers so the hardware
 * will wakeup in time to receive beacons, and configures
 * the beacon miss handling so we'll receive a BMISS
 * interrupt when we stop seeing beacons from the AP
 * we've associated with.
 */
static void
ath_beacon_config(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = ic->ic_bss;
	u_int32_t nexttbtt;

	nexttbtt = (LE_READ_4(ni->ni_tstamp + 4) << 22) |
	    (LE_READ_4(ni->ni_tstamp) >> 10);
	DPRINTF(("%s: nexttbtt=%u\n", __func__, nexttbtt));
	nexttbtt += ni->ni_intval;
	if (ic->ic_opmode == IEEE80211_M_STA) {
		HAL_BEACON_STATE bs;
		u_int32_t bmisstime;

		/* NB: no PCF support right now */
		memset(&bs, 0, sizeof(bs));
		bs.bs_intval = ni->ni_intval;
		bs.bs_nexttbtt = nexttbtt;
		bs.bs_dtimperiod = bs.bs_intval;
		bs.bs_nextdtim = nexttbtt;
		/*
		 * Calculate the number of consecutive beacons to miss
		 * before taking a BMISS interrupt.  The configuration
		 * is specified in ms, so we need to convert that to
		 * TU's and then calculate based on the beacon interval.
		 * Note that we clamp the result to at most 10 beacons.
		 */
		bmisstime = (ic->ic_bmisstimeout * 1000) / 1024;
		bs.bs_bmissthreshold = howmany(bmisstime,ni->ni_intval);
		if (bs.bs_bmissthreshold > 10)
			bs.bs_bmissthreshold = 10;
		else if (bs.bs_bmissthreshold <= 0)
			bs.bs_bmissthreshold = 1;

		/*
		 * Calculate sleep duration.  The configuration is
		 * given in ms.  We insure a multiple of the beacon
		 * period is used.  Also, if the sleep duration is
		 * greater than the DTIM period then it makes senses
		 * to make it a multiple of that.
		 *
		 * XXX fixed at 100ms
		 */
		bs.bs_sleepduration =
			roundup((100 * 1000) / 1024, bs.bs_intval);
		if (bs.bs_sleepduration > bs.bs_dtimperiod)
			bs.bs_sleepduration = roundup(bs.bs_sleepduration, bs.bs_dtimperiod);

		DPRINTF(("%s: intval %u nexttbtt %u dtim %u nextdtim %u bmiss %u sleep %u\n"
			, __func__
			, bs.bs_intval
			, bs.bs_nexttbtt
			, bs.bs_dtimperiod
			, bs.bs_nextdtim
			, bs.bs_bmissthreshold
			, bs.bs_sleepduration
		));
		ath_hal_intrset(ah, 0);
		/*
		 * Reset our tsf so the hardware will update the
		 * tsf register to reflect timestamps found in
		 * received beacons.
		 */
		ath_hal_resettsf(ah);
		ath_hal_beacontimers(ah, &bs, 0/*XXX*/, 0, 0);
		sc->sc_imask |= HAL_INT_BMISS;
		ath_hal_intrset(ah, sc->sc_imask);
	} else {
		DPRINTF(("%s: intval %u nexttbtt %u\n",
			__func__, ni->ni_intval, nexttbtt));
		ath_hal_intrset(ah, 0);
		ath_hal_beaconinit(ah, ic->ic_opmode,
			nexttbtt, ni->ni_intval);
		if (ic->ic_opmode != IEEE80211_M_MONITOR)
			sc->sc_imask |= HAL_INT_SWBA;	/* beacon prepare */
		ath_hal_intrset(ah, sc->sc_imask);
	}
}

static void
ath_load_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *paddr = (bus_addr_t*) arg;
	*paddr = segs->ds_addr;
}

static int
ath_desc_alloc(struct ath_softc *sc)
{
	int i, bsize, error;
	struct ath_desc *ds;
	struct ath_buf *bf;

	/* allocate descriptors */
	sc->sc_desc_len = sizeof(struct ath_desc) *
				(ATH_TXBUF * ATH_TXDESC + ATH_RXBUF + 1);
	error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT, &sc->sc_ddmamap);
	if (error != 0)
		return error;

	error = bus_dmamem_alloc(sc->sc_dmat, (void**) &sc->sc_desc,
				 BUS_DMA_NOWAIT, &sc->sc_ddmamap);
	if (error != 0)
		goto fail0;

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_ddmamap,
				sc->sc_desc, sc->sc_desc_len,
				ath_load_cb, &sc->sc_desc_paddr,
				BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail1;

	ds = sc->sc_desc;
	DPRINTF(("ath_desc_alloc: DMA map: %p (%d) -> %p (%lu)\n",
	    ds, sc->sc_desc_len,
	    (caddr_t) sc->sc_desc_paddr, /*XXX*/ (u_long) sc->sc_desc_len));

	/* allocate buffers */
	bsize = sizeof(struct ath_buf) * (ATH_TXBUF + ATH_RXBUF + 1);
	bf = malloc(bsize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (bf == NULL)
		goto fail2;
	sc->sc_bufptr = bf;

	TAILQ_INIT(&sc->sc_rxbuf);
	for (i = 0; i < ATH_RXBUF; i++, bf++, ds++) {
		bf->bf_desc = ds;
		bf->bf_daddr = sc->sc_desc_paddr +
		    ((caddr_t)ds - (caddr_t)sc->sc_desc);
		error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT,
					  &bf->bf_dmamap);
		if (error != 0)
			break;
		TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	}

	TAILQ_INIT(&sc->sc_txbuf);
	for (i = 0; i < ATH_TXBUF; i++, bf++, ds += ATH_TXDESC) {
		bf->bf_desc = ds;
		bf->bf_daddr = sc->sc_desc_paddr +
		    ((caddr_t)ds - (caddr_t)sc->sc_desc);
		error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT,
					  &bf->bf_dmamap);
		if (error != 0)
			break;
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
	}
	TAILQ_INIT(&sc->sc_txq);

	/* beacon buffer */
	bf->bf_desc = ds;
	bf->bf_daddr = sc->sc_desc_paddr + ((caddr_t)ds - (caddr_t)sc->sc_desc);
	error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT, &bf->bf_dmamap);
	if (error != 0)
		return error;
	sc->sc_bcbuf = bf;
	return 0;

fail2:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_ddmamap);
fail1:
	bus_dmamem_free(sc->sc_dmat, sc->sc_desc, sc->sc_ddmamap);
fail0:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);
	sc->sc_ddmamap = NULL;
	return error;
}

static void
ath_desc_free(struct ath_softc *sc)
{
	struct ath_buf *bf;

	bus_dmamap_unload(sc->sc_dmat, sc->sc_ddmamap);
	bus_dmamem_free(sc->sc_dmat, sc->sc_desc, sc->sc_ddmamap);
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_ddmamap);

	TAILQ_FOREACH(bf, &sc->sc_txq, bf_list) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
	}
	TAILQ_FOREACH(bf, &sc->sc_txbuf, bf_list)
		bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
	TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		if (bf->bf_m) {
			bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
			bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
			m_freem(bf->bf_m);
			bf->bf_m = NULL;
		}
	}
	if (sc->sc_bcbuf != NULL) {
		bus_dmamap_unload(sc->sc_dmat, sc->sc_bcbuf->bf_dmamap);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_bcbuf->bf_dmamap);
		sc->sc_bcbuf = NULL;
	}

	TAILQ_INIT(&sc->sc_rxbuf);
	TAILQ_INIT(&sc->sc_txbuf);
	TAILQ_INIT(&sc->sc_txq);
	free(sc->sc_bufptr, M_DEVBUF);
	sc->sc_bufptr = NULL;
}

static struct ieee80211_node *
ath_node_alloc(struct ieee80211com *ic)
{
	struct ath_node *an =
		malloc(sizeof(struct ath_node), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (an) {
		int i;
		for (i = 0; i < ATH_RHIST_SIZE; i++)
			an->an_rx_hist[i].arh_ticks = ATH_RHIST_NOTIME;
		an->an_rx_hist_next = ATH_RHIST_SIZE-1;
		return &an->an_node;
	} else
		return NULL;
}

static void
ath_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
        struct ath_softc *sc = ic->ic_if.if_softc;
	struct ath_buf *bf;

	TAILQ_FOREACH(bf, &sc->sc_txq, bf_list) {
		if (bf->bf_node == ni)
			bf->bf_node = NULL;
	}
	free(ni, M_DEVBUF);
}

static void
ath_node_copy(struct ieee80211com *ic,
	struct ieee80211_node *dst, const struct ieee80211_node *src)
{
	*(struct ath_node *)dst = *(const struct ath_node *)src;
}


static u_int8_t
ath_node_getrssi(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ath_node *an = ATH_NODE(ni);
	int i, now, nsamples, rssi;

	/*
	 * Calculate the average over the last second of sampled data.
	 */
	now = ticks;
	nsamples = 0;
	rssi = 0;
	i = an->an_rx_hist_next;
	do {
		struct ath_recv_hist *rh = &an->an_rx_hist[i];
		if (rh->arh_ticks == ATH_RHIST_NOTIME)
			goto done;
		if (now - rh->arh_ticks > hz)
			goto done;
		rssi += rh->arh_rssi;
		nsamples++;
		if (i == 0)
			i = ATH_RHIST_SIZE-1;
		else
			i--;
	} while (i != an->an_rx_hist_next);
done:
	/*
	 * Return either the average or the last known
	 * value if there is no recent data.
	 */
	return (nsamples ? rssi / nsamples : an->an_rx_hist[i].arh_rssi);
}

static int
ath_rxbuf_init(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;
	int error;
	struct mbuf *m;
	struct ath_desc *ds;

	m = bf->bf_m;
	if (m == NULL) {
		/*
		 * NB: by assigning a page to the rx dma buffer we
		 * implicitly satisfy the Atheros requirement that
		 * this buffer be cache-line-aligned and sized to be
		 * multiple of the cache line size.  Not doing this
		 * causes weird stuff to happen (for the 5210 at least).
		 */
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL) {
			DPRINTF(("ath_rxbuf_init: no mbuf/cluster\n"));
			sc->sc_stats.ast_rx_nombuf++;
			return ENOMEM;
		}
		bf->bf_m = m;
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m,
					     ath_mbuf_load_cb, bf,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			DPRINTF(("ath_rxbuf_init: bus_dmamap_load_mbuf failed;"
				" error %d\n", error));
			sc->sc_stats.ast_rx_busdma++;
			return error;
		}
		KASSERT(bf->bf_nseg == 1,
			("ath_rxbuf_init: multi-segment packet; nseg %u",
			bf->bf_nseg));
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREREAD);

	/*
	 * Setup descriptors.  For receive we always terminate
	 * the descriptor list with a self-linked entry so we'll
	 * not get overrun under high load (as can happen with a
	 * 5212 when ANI processing enables PHY errors).
	 *
	 * To insure the last descriptor is self-linked we create
	 * each descriptor as self-linked and add it to the end.  As
	 * each additional descriptor is added the previous self-linked
	 * entry is ``fixed'' naturally.  This should be safe even
	 * if DMA is happening.  When processing RX interrupts we
	 * never remove/process the last, self-linked, entry on the
	 * descriptor list.  This insures the hardware always has
	 * someplace to write a new frame.
	 */
	ds = bf->bf_desc;
	ds->ds_link = bf->bf_daddr;	/* link to self */
	ds->ds_data = bf->bf_segs[0].ds_addr;
	ath_hal_setuprxdesc(ah, ds
		, m->m_len		/* buffer size */
		, 0
	);

	if (sc->sc_rxlink != NULL)
		*sc->sc_rxlink = bf->bf_daddr;
	sc->sc_rxlink = &ds->ds_link;
	return 0;
}

static void
ath_rx_proc(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ath_buf *bf;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	struct mbuf *m;
	struct ieee80211_frame *wh, whbuf;
	struct ieee80211_node *ni;
	struct ath_node *an;
	struct ath_recv_hist *rh;
	int len;
	u_int phyerr;
	HAL_STATUS status;

	DPRINTF2(("ath_rx_proc: pending %u\n", npending));
	do {
		bf = TAILQ_FIRST(&sc->sc_rxbuf);
		if (bf == NULL) {		/* NB: shouldn't happen */
			if_printf(ifp, "ath_rx_proc: no buffer!\n");
			break;
		}
		ds = bf->bf_desc;
		if (ds->ds_link == bf->bf_daddr) {
			/* NB: never process the self-linked entry at the end */
			break;
		}
		m = bf->bf_m;
		if (m == NULL) {		/* NB: shouldn't happen */
			if_printf(ifp, "ath_rx_proc: no mbuf!\n");
			continue;
		}
		status = ath_hal_rxprocdesc(ah, ds);
#ifdef AR_DEBUG
		if (ath_debug > 1)
			ath_printrxbuf(bf, status == HAL_OK); 
#endif
		if (status == HAL_EINPROGRESS)
			break;
		TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);
		if (ds->ds_rxstat.rs_status != 0) {
			if (ds->ds_rxstat.rs_status & HAL_RXERR_CRC)
				sc->sc_stats.ast_rx_crcerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_FIFO)
				sc->sc_stats.ast_rx_fifoerr++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_DECRYPT)
				sc->sc_stats.ast_rx_badcrypt++;
			if (ds->ds_rxstat.rs_status & HAL_RXERR_PHY) {
				sc->sc_stats.ast_rx_phyerr++;
				phyerr = ds->ds_rxstat.rs_phyerr & 0x1f;
				sc->sc_stats.ast_rx_phy[phyerr]++;
			} else {
				/*
				 * NB: don't count PHY errors as input errors;
				 * we enable them on the 5212 to collect info
				 * about environmental noise and, in that
				 * setting, they don't really reflect tx/rx
				 * errors.
				 */
				ifp->if_ierrors++;
			}
			goto rx_next;
		}

		len = ds->ds_rxstat.rs_datalen;
		if (len < IEEE80211_MIN_LEN) {
			DPRINTF(("ath_rx_proc: short packet %d\n", len));
			sc->sc_stats.ast_rx_tooshort++;
			goto rx_next;
		}

		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, 
		    BUS_DMASYNC_POSTREAD);

		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		bf->bf_m = NULL;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		if (sc->sc_drvbpf) {
			struct mbuf *mb;

			/* XXX pre-allocate space when setting up recv's */
			MGETHDR(mb, M_DONTWAIT, m->m_type);
			if (mb != NULL) {
				sc->sc_rx_th.wr_rate =
					sc->sc_hwmap[ds->ds_rxstat.rs_rate];
				sc->sc_rx_th.wr_antsignal =
					ds->ds_rxstat.rs_rssi;
				sc->sc_rx_th.wr_antenna =
					ds->ds_rxstat.rs_antenna;
				/* XXX TSF */

				(void) m_dup_pkthdr(mb, m, M_DONTWAIT);
				mb->m_next = m;
				mb->m_data = (caddr_t)&sc->sc_rx_th;
				mb->m_len = sizeof(sc->sc_rx_th);
				mb->m_pkthdr.len += mb->m_len;
				bpf_mtap(sc->sc_drvbpf, mb);
				m_free(mb);
			}
		}

		m_adj(m, -IEEE80211_CRC_LEN);
		wh = mtod(m, struct ieee80211_frame *);
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			/*
			 * WEP is decrypted by hardware. Clear WEP bit
			 * and trim WEP header for ieee80211_input().
			 */
			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
			memcpy(&whbuf, wh, sizeof(whbuf));
			m_adj(m, IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN);
			wh = mtod(m, struct ieee80211_frame *);
			memcpy(wh, &whbuf, sizeof(whbuf));
			/*
			 * Also trim WEP ICV from the tail.
			 */
			m_adj(m, -IEEE80211_WEP_CRCLEN);
		}

		/*
		 * Locate the node for sender, track state, and
		 * then pass this node (referenced) up to the 802.11
		 * layer for its use.  We are required to pass
		 * something so we fall back to ic_bss when this frame
		 * is from an unknown sender.
		 */
		if (ic->ic_opmode != IEEE80211_M_STA) {
			ni = ieee80211_find_node(ic, wh->i_addr2);
			if (ni == NULL)
				ni = ieee80211_ref_node(ic->ic_bss);
		} else
			ni = ieee80211_ref_node(ic->ic_bss);

		/*
		 * Record driver-specific state.
		 */
		an = ATH_NODE(ni);
		if (++(an->an_rx_hist_next) == ATH_RHIST_SIZE)
			an->an_rx_hist_next = 0;
		rh = &an->an_rx_hist[an->an_rx_hist_next];
		rh->arh_ticks = ticks;
		rh->arh_rssi = ds->ds_rxstat.rs_rssi;
		rh->arh_antenna = ds->ds_rxstat.rs_antenna;

		/*
		 * Send frame up for processing.
		 */
		ieee80211_input(ifp, m, ni,
			ds->ds_rxstat.rs_rssi, ds->ds_rxstat.rs_tstamp);

		/*
		 * The frame may have caused the node to be marked for
		 * reclamation (e.g. in response to a DEAUTH message)
		 * so use free_node here instead of unref_node.
		 */
		if (ni == ic->ic_bss)
			ieee80211_unref_node(&ni);
		else
			ieee80211_free_node(ic, ni);
  rx_next:
		TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	} while (ath_rxbuf_init(sc, bf) == 0);

	ath_hal_rxmonitor(ah);			/* rx signal state monitoring */
	ath_hal_rxena(ah);			/* in case of RXEOL */
}

/*
 * XXX Size of an ACK control frame in bytes.
 */
#define	IEEE80211_ACK_SIZE	(2+2+IEEE80211_ADDR_LEN+4)

static int
ath_tx_start(struct ath_softc *sc, struct ieee80211_node *ni, struct ath_buf *bf,
    struct mbuf *m0)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int i, error, iswep, hdrlen, pktlen;
	u_int8_t rix, cix, txrate, ctsrate;
	struct ath_desc *ds;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	u_int32_t iv;
	u_int8_t *ivp;
	u_int8_t hdrbuf[sizeof(struct ieee80211_frame) +
	    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN];
	u_int subtype, flags, ctsduration, antenna;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	HAL_BOOL shortPreamble;
	struct ath_node *an;

	wh = mtod(m0, struct ieee80211_frame *);
	iswep = wh->i_fc[1] & IEEE80211_FC1_WEP;
	hdrlen = sizeof(struct ieee80211_frame);
	pktlen = m0->m_pkthdr.len;

	if (iswep) {
		memcpy(hdrbuf, mtod(m0, caddr_t), hdrlen);
		m_adj(m0, hdrlen);
		M_PREPEND(m0, sizeof(hdrbuf), M_DONTWAIT);
		if (m0 == NULL) {
			sc->sc_stats.ast_tx_nombuf++;
			return ENOMEM;
		}
		ivp = hdrbuf + hdrlen;
		wh = mtod(m0, struct ieee80211_frame *);
		/*
		 * XXX
		 * IV must not duplicate during the lifetime of the key.
		 * But no mechanism to renew keys is defined in IEEE 802.11
		 * WEP.  And IV may be duplicated between other stations
		 * because of the session key itself is shared.
		 * So we use pseudo random IV for now, though it is not the
		 * right way.
		 */
                iv = ic->ic_iv;
		/*
		 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
		 * (B, 255, N) with 3 <= B < 8
		 */
		if (iv >= 0x03ff00 && (iv & 0xf8ff00) == 0x00ff00)
			iv += 0x000100;
		ic->ic_iv = iv + 1;
		for (i = 0; i < IEEE80211_WEP_IVLEN; i++) {
			ivp[i] = iv;
			iv >>= 8;
		}
		ivp[i] = sc->sc_ic.ic_wep_txkey << 6;	/* Key ID and pad */
		memcpy(mtod(m0, caddr_t), hdrbuf, sizeof(hdrbuf));
		/*
		 * The ICV length must be included into hdrlen and pktlen.
		 */
		hdrlen = sizeof(hdrbuf) + IEEE80211_WEP_CRCLEN;
		pktlen = m0->m_pkthdr.len + IEEE80211_WEP_CRCLEN;
	}
	pktlen += IEEE80211_CRC_LEN;

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m0,
				     ath_mbuf_load_cb, bf,
				     BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		/* XXX packet requires too many descriptors */
		bf->bf_nseg = ATH_TXDESC+1;
	} else if (error != 0) {
		sc->sc_stats.ast_tx_busdma++;
		m_freem(m0);
		return error;
	}
	/*
	 * Discard null packets and check for packets that
	 * require too many TX descriptors.  We try to convert
	 * the latter to a cluster.
	 */
	if (bf->bf_nseg > ATH_TXDESC) {		/* too many desc's, linearize */
		sc->sc_stats.ast_tx_linear++;
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			sc->sc_stats.ast_tx_nombuf++;
			m_freem(m0);
			return ENOMEM;
		}
		M_MOVE_PKTHDR(m, m0);
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			sc->sc_stats.ast_tx_nomcl++;
			m_freem(m0);
			m_free(m);
			return ENOMEM;
		}
		m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, caddr_t));
		m_freem(m0);
		m->m_len = m->m_pkthdr.len;
		m0 = m;
		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_dmamap, m0,
					     ath_mbuf_load_cb, bf,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			sc->sc_stats.ast_tx_busdma++;
			m_freem(m0);
			return error;
		}
		KASSERT(bf->bf_nseg == 1,
			("ath_tx_start: packet not one segment; nseg %u",
			bf->bf_nseg));
	} else if (bf->bf_nseg == 0) {		/* null packet, discard */
		sc->sc_stats.ast_tx_nodata++;
		m_freem(m0);
		return EIO;
	}
	DPRINTF2(("ath_tx_start: m %p len %u\n", m0, pktlen));
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREWRITE);
	bf->bf_m = m0;
	bf->bf_node = ni;			/* NB: held reference */

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/*
	 * Calculate Atheros packet type from IEEE80211 packet header
	 * and setup for rate calculations.
	 */
	atype = HAL_PKT_TYPE_NORMAL;			/* default */
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_MGT:
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
			atype = HAL_PKT_TYPE_BEACON;
		else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			atype = HAL_PKT_TYPE_PROBE_RESP;
		else if (subtype == IEEE80211_FC0_SUBTYPE_ATIM)
			atype = HAL_PKT_TYPE_ATIM;
		rix = 0;			/* XXX lowest rate */
		break;
	case IEEE80211_FC0_TYPE_CTL:
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL)
			atype = HAL_PKT_TYPE_PSPOLL;
		rix = 0;			/* XXX lowest rate */
		break;
	default:
		rix = sc->sc_rixmap[ni->ni_rates.rs_rates[ni->ni_txrate] &
				IEEE80211_RATE_VAL];
		if (rix == 0xff) {
			if_printf(ifp, "bogus xmit rate 0x%x\n",
				ni->ni_rates.rs_rates[ni->ni_txrate]);
			sc->sc_stats.ast_tx_badrate++;
			m_freem(m0);
			return EIO;
		}
		break;
	}
	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
		txrate = rt->info[rix].rateCode | rt->info[rix].shortPreamble;
		shortPreamble = AH_TRUE;
		sc->sc_stats.ast_tx_shortpre++;
	} else {
		txrate = rt->info[rix].rateCode;
		shortPreamble = AH_FALSE;
	}

	/*
	 * Calculate miscellaneous flags.
	 */
	flags = HAL_TXDESC_CLRDMASK;		/* XXX needed for wep errors */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
		sc->sc_stats.ast_tx_noack++;
	} else if (pktlen > ic->ic_rtsthreshold) {
		flags |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
		sc->sc_stats.ast_tx_rts++;
	}

	/*
	 * Calculate duration.  This logically belongs in the 802.11
	 * layer but it lacks sufficient information to calculate it.
	 */
	if ((flags & HAL_TXDESC_NOACK) == 0 &&
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL) {
		u_int16_t dur;
		/*
		 * XXX not right with fragmentation.
		 */
		dur = ath_hal_computetxtime(ah, rt, IEEE80211_ACK_SIZE,
				rix, shortPreamble);
		*((u_int16_t*) wh->i_dur) = htole16(dur);
	}

	/*
	 * Calculate RTS/CTS rate and duration if needed.
	 */
	ctsduration = 0;
	if (flags & (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)) {
		/*
		 * CTS transmit rate is derived from the transmit rate
		 * by looking in the h/w rate table.  We must also factor
		 * in whether or not a short preamble is to be used.
		 */
		cix = rt->info[rix].controlRate;
		ctsrate = rt->info[cix].rateCode;
		if (shortPreamble)
			ctsrate |= rt->info[cix].shortPreamble;
		/*
		 * Compute the transmit duration based on the size
		 * of an ACK frame.  We call into the HAL to do the
		 * computation since it depends on the characteristics
		 * of the actual PHY being used.
		 */
		if (flags & HAL_TXDESC_RTSENA) {	/* SIFS + CTS */
			ctsduration += ath_hal_computetxtime(ah,
				rt, IEEE80211_ACK_SIZE, cix, shortPreamble);
		}
		/* SIFS + data */
		ctsduration += ath_hal_computetxtime(ah,
			rt, pktlen, rix, shortPreamble);
		if ((flags & HAL_TXDESC_NOACK) == 0) {	/* SIFS + ACK */
			ctsduration += ath_hal_computetxtime(ah,
				rt, IEEE80211_ACK_SIZE, cix, shortPreamble);
		}
	} else
		ctsrate = 0;

	/*
	 * For now use the antenna on which the last good
	 * frame was received on.  We assume this field is
	 * initialized to 0 which gives us ``auto'' or the
	 * ``default'' antenna.
	 */
	an = (struct ath_node *) ni;
	if (an->an_tx_antenna)
		antenna = an->an_tx_antenna;
	else
		antenna = an->an_rx_hist[an->an_rx_hist_next].arh_antenna;

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, 60			/* txpower XXX */
		, txrate, 1+10		/* series 0 rate/tries */
		, iswep ? sc->sc_ic.ic_wep_txkey : HAL_TXKEYIX_INVALID
		, antenna		/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);
#ifdef notyet
	ath_hal_setupxtxdesc(ah, ds
		, AH_FALSE		/* short preamble */
		, 0, 0			/* series 1 rate/tries */
		, 0, 0			/* series 2 rate/tries */
		, 0, 0			/* series 3 rate/tries */
	);
#endif
	/*
	 * Fillin the remainder of the descriptor info.
	 */
	for (i = 0; i < bf->bf_nseg; i++, ds++) {
		ds->ds_data = bf->bf_segs[i].ds_addr;
		if (i == bf->bf_nseg - 1)
			ds->ds_link = 0;
		else
			ds->ds_link = bf->bf_daddr + sizeof(*ds) * (i + 1);
		ath_hal_filltxdesc(ah, ds
			, bf->bf_segs[i].ds_len	/* segment length */
			, i == 0		/* first segment */
			, i == bf->bf_nseg - 1	/* last segment */
		);
		DPRINTF2(("ath_tx_start: %d: %08x %08x %08x %08x %08x %08x\n",
		    i, ds->ds_link, ds->ds_data, ds->ds_ctl0, ds->ds_ctl1,
		    ds->ds_hw[0], ds->ds_hw[1]));
	}

	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	ATH_TXQ_LOCK(sc);
	TAILQ_INSERT_TAIL(&sc->sc_txq, bf, bf_list);
	if (sc->sc_txlink == NULL) {
		ath_hal_puttxbuf(ah, sc->sc_txhalq, bf->bf_daddr);
		DPRINTF2(("ath_tx_start: TXDP0 = %p (%p)\n",
		    (caddr_t)bf->bf_daddr, bf->bf_desc));
	} else {
		*sc->sc_txlink = bf->bf_daddr;
		DPRINTF2(("ath_tx_start: link(%p)=%p (%p)\n",
		    sc->sc_txlink, (caddr_t)bf->bf_daddr, bf->bf_desc));
	}
	sc->sc_txlink = &bf->bf_desc[bf->bf_nseg - 1].ds_link;
	ATH_TXQ_UNLOCK(sc);

	ath_hal_txstart(ah, sc->sc_txhalq);
	return 0;
}

static void
ath_tx_proc(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_desc *ds;
	struct ieee80211_node *ni;
	struct ath_node *an;
	int sr, lr;
	HAL_STATUS status;

	DPRINTF2(("ath_tx_proc: pending %u tx queue %p, link %p\n",
		npending, (caddr_t) ath_hal_gettxbuf(sc->sc_ah, sc->sc_txhalq),
		sc->sc_txlink));
	for (;;) {
		ATH_TXQ_LOCK(sc);
		bf = TAILQ_FIRST(&sc->sc_txq);
		if (bf == NULL) {
			sc->sc_txlink = NULL;
			ATH_TXQ_UNLOCK(sc);
			break;
		}
		/* only the last descriptor is needed */
		ds = &bf->bf_desc[bf->bf_nseg - 1];
		status = ath_hal_txprocdesc(ah, ds);
#ifdef AR_DEBUG
		if (ath_debug > 1)
			ath_printtxbuf(bf, status == HAL_OK);
#endif
		if (status == HAL_EINPROGRESS) {
			ATH_TXQ_UNLOCK(sc);
			break;
		}
		TAILQ_REMOVE(&sc->sc_txq, bf, bf_list);
		ATH_TXQ_UNLOCK(sc);

		ni = bf->bf_node;
		if (ni != NULL) {
			an = (struct ath_node *) ni;
			if (ds->ds_txstat.ts_status == 0) {
				an->an_tx_ok++;
				an->an_tx_antenna = ds->ds_txstat.ts_antenna;
			} else {
				an->an_tx_err++;
				ifp->if_oerrors++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_XRETRY)
					sc->sc_stats.ast_tx_xretries++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FIFO)
					sc->sc_stats.ast_tx_fifoerr++;
				if (ds->ds_txstat.ts_status & HAL_TXERR_FILT)
					sc->sc_stats.ast_tx_filtered++;
				an->an_tx_antenna = 0;	/* invalidate */
			}
			sr = ds->ds_txstat.ts_shortretry;
			lr = ds->ds_txstat.ts_longretry;
			sc->sc_stats.ast_tx_shortretry += sr;
			sc->sc_stats.ast_tx_longretry += lr;
			if (sr + lr)
				an->an_tx_retr++;
			/*
			 * Reclaim reference to node.
			 *
			 * NB: the node may be reclaimed here if, for example
			 *     this is a DEAUTH message that was sent and the
			 *     node was timed out due to inactivity.
			 */
			if (ni != ic->ic_bss)
				ieee80211_free_node(ic, ni);
		}
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;

		ATH_TXBUF_LOCK(sc);
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
	}
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_tx_timer = 0;

	ath_start(ifp);
}

/*
 * Drain the transmit queue and reclaim resources.
 */
static void
ath_draintxq(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	struct ath_buf *bf;

	/* XXX return value */
	if (!sc->sc_invalid) {
		/* don't touch the hardware if marked invalid */
		(void) ath_hal_stoptxdma(ah, sc->sc_txhalq);
		DPRINTF(("ath_draintxq: tx queue %p, link %p\n",
		    (caddr_t) ath_hal_gettxbuf(ah, sc->sc_txhalq),
		    sc->sc_txlink));
		(void) ath_hal_stoptxdma(ah, sc->sc_bhalq);
		DPRINTF(("ath_draintxq: beacon queue %p\n",
		    (caddr_t) ath_hal_gettxbuf(ah, sc->sc_bhalq)));
	}
	for (;;) {
		ATH_TXQ_LOCK(sc);
		bf = TAILQ_FIRST(&sc->sc_txq);
		if (bf == NULL) {
			sc->sc_txlink = NULL;
			ATH_TXQ_UNLOCK(sc);
			break;
		}
		TAILQ_REMOVE(&sc->sc_txq, bf, bf_list);
		ATH_TXQ_UNLOCK(sc);
#ifdef AR_DEBUG
		if (ath_debug)
			ath_printtxbuf(bf,
				ath_hal_txprocdesc(ah, bf->bf_desc) == HAL_OK);
#endif /* AR_DEBUG */
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;
		ATH_TXBUF_LOCK(sc);
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
	}
	ifp->if_flags &= ~IFF_OACTIVE;
	sc->sc_tx_timer = 0;
}

/*
 * Disable the receive h/w in preparation for a reset.
 */
static void
ath_stoprecv(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_stoppcurecv(ah);	/* disable PCU */
	ath_hal_setrxfilter(ah, 0);	/* clear recv filter */
	ath_hal_stopdmarecv(ah);	/* disable DMA engine */
	DELAY(3000);			/* long enough for 1 frame */
#ifdef AR_DEBUG
	if (ath_debug) {
		struct ath_buf *bf;

		DPRINTF(("ath_stoprecv: rx queue %p, link %p\n",
		    (caddr_t) ath_hal_getrxbuf(ah), sc->sc_rxlink));
		TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
			if (ath_hal_rxprocdesc(ah, bf->bf_desc) == HAL_OK)
				ath_printrxbuf(bf, 1);
		}
	}
#endif
	sc->sc_rxlink = NULL;		/* just in case */
}

/*
 * Enable the receive h/w following a reset.
 */
static int
ath_startrecv(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;

	sc->sc_rxlink = NULL;
	TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		int error = ath_rxbuf_init(sc, bf);
		if (error != 0) {
			DPRINTF(("ath_startrecv: ath_rxbuf_init failed %d\n",
				error));
			return error;
		}
	}

	bf = TAILQ_FIRST(&sc->sc_rxbuf);
	ath_hal_putrxbuf(ah, bf->bf_daddr);
	ath_hal_rxena(ah);		/* enable recv descriptors */
	ath_mode_init(sc);		/* set filters, etc. */
	ath_hal_startpcurecv(ah);	/* re-enable PCU/DMA engine */
	return 0;
}

/*
 * Set/change channels.  If the channel is really being changed,
 * it's done by resetting the chip.  To accomplish this we must
 * first cleanup any pending DMA, then restart stuff after a la
 * ath_init.
 */
static int
ath_chan_set(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;

	DPRINTF(("ath_chan_set: %u (%u MHz) -> %u (%u MHz)\n",
	    ieee80211_chan2ieee(ic, ic->ic_ibss_chan),
		ic->ic_ibss_chan->ic_freq,
	    ieee80211_chan2ieee(ic, chan), chan->ic_freq));
	if (chan != ic->ic_ibss_chan) {
		HAL_STATUS status;
		HAL_CHANNEL hchan;
		enum ieee80211_phymode mode;

		/*
		 * To switch channels clear any pending DMA operations;
		 * wait long enough for the RX fifo to drain, reset the
		 * hardware at the new frequency, and then re-enable
		 * the relevant bits of the h/w.
		 */
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc);		/* clear pending tx frames */
		ath_stoprecv(sc);		/* turn off frame recv */
		/*
		 * Convert to a HAL channel description with
		 * the flags constrained to reflect the current
		 * operating mode.
		 */
		hchan.channel = chan->ic_freq;
		hchan.channelFlags = ath_chan2flags(ic, chan);
		if (!ath_hal_reset(ah, ic->ic_opmode, &hchan, AH_TRUE, &status)) {
			if_printf(&ic->ic_if, "ath_chan_set: unable to reset "
				"channel %u (%u Mhz)\n",
				ieee80211_chan2ieee(ic, chan), chan->ic_freq);
			return EIO;
		}
		/*
		 * Re-enable rx framework.
		 */
		if (ath_startrecv(sc) != 0) {
			if_printf(&ic->ic_if,
				"ath_chan_set: unable to restart recv logic\n");
			return EIO;
		}

		/*
		 * Update BPF state.
		 */
		sc->sc_tx_th.wt_chan_freq = sc->sc_rx_th.wr_chan_freq =
			htole16(chan->ic_freq);
		sc->sc_tx_th.wt_chan_flags = sc->sc_rx_th.wr_chan_flags =
			htole16(chan->ic_flags);

		/*
		 * Change channels and update the h/w rate map
		 * if we're switching; e.g. 11a to 11b/g.
		 */
		ic->ic_ibss_chan = chan;
		mode = ieee80211_chan2mode(ic, chan);
		if (mode != sc->sc_curmode)
			ath_setcurmode(sc, mode);

		/*
		 * Re-enable interrupts.
		 */
		ath_hal_intrset(ah, sc->sc_imask);
	}
	return 0;
}

static void
ath_next_scan(void *arg)
{
	struct ath_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
}

/*
 * Periodically recalibrate the PHY to account
 * for temperature/environment changes.
 */
static void
ath_calibrate(void *arg)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c;
	HAL_CHANNEL hchan;

	sc->sc_stats.ast_per_cal++;

	/*
	 * Convert to a HAL channel description with the flags
	 * constrained to reflect the current operating mode.
	 */
	c = ic->ic_ibss_chan;
	hchan.channel = c->ic_freq;
	hchan.channelFlags = ath_chan2flags(ic, c);

	DPRINTF(("%s: channel %u/%x\n", __func__, c->ic_freq, c->ic_flags));

	if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE) {
		/*
		 * Rfgain is out of bounds, reset the chip
		 * to load new gain values.
		 */
		sc->sc_stats.ast_per_rfgain++;
		ath_reset(sc);
	}
	if (!ath_hal_calibrate(ah, &hchan)) {
		DPRINTF(("%s: calibration of channel %u failed\n",
			__func__, c->ic_freq));
		sc->sc_stats.ast_per_calfail++;
	}
	callout_reset(&sc->sc_cal_ch, hz * ath_calinterval, ath_calibrate, sc);
}

static int
ath_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni;
	int i, error;
	u_int8_t *bssid;
	u_int32_t rfilt;
	static const HAL_LED_STATE leds[] = {
	    HAL_LED_INIT,	/* IEEE80211_S_INIT */
	    HAL_LED_SCAN,	/* IEEE80211_S_SCAN */
	    HAL_LED_AUTH,	/* IEEE80211_S_AUTH */
	    HAL_LED_ASSOC, 	/* IEEE80211_S_ASSOC */
	    HAL_LED_RUN, 	/* IEEE80211_S_RUN */
	};

	DPRINTF(("%s: %s -> %s\n", __func__,
		ieee80211_state_name[ic->ic_state],
		ieee80211_state_name[nstate]));

	ath_hal_setledstate(ah, leds[nstate]);	/* set LED */

	if (nstate == IEEE80211_S_INIT) {
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		ath_hal_intrset(ah, sc->sc_imask);
		callout_stop(&sc->sc_scan_ch);
		callout_stop(&sc->sc_cal_ch);
		return (*sc->sc_newstate)(ic, nstate, arg);
	}
	ni = ic->ic_bss;
	error = ath_chan_set(sc, ni->ni_chan);
	if (error != 0)
		goto bad;
	rfilt = ath_calcrxfilter(sc);
	if (nstate == IEEE80211_S_SCAN) {
		callout_reset(&sc->sc_scan_ch, (hz * ath_dwelltime) / 1000,
			ath_next_scan, sc);
		bssid = ifp->if_broadcastaddr;
	} else {
		callout_stop(&sc->sc_scan_ch);
		bssid = ni->ni_bssid;
	}
	ath_hal_setrxfilter(ah, rfilt);
	DPRINTF(("%s: RX filter 0x%x bssid %s\n",
		 __func__, rfilt, ether_sprintf(bssid)));

	if (nstate == IEEE80211_S_RUN && ic->ic_opmode == IEEE80211_M_STA)
		ath_hal_setassocid(ah, bssid, ni->ni_associd);
	else
		ath_hal_setassocid(ah, bssid, 0);
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			if (ath_hal_keyisvalid(ah, i))
				ath_hal_keysetmac(ah, i, bssid);
	}

	if (nstate == IEEE80211_S_RUN) {
		DPRINTF(("%s(RUN): ic_flags=0x%08x iv=%d bssid=%s "
			"capinfo=0x%04x chan=%d\n"
			 , __func__
			 , ic->ic_flags
			 , ni->ni_intval
			 , ether_sprintf(ni->ni_bssid)
			 , ni->ni_capinfo
			 , ieee80211_chan2ieee(ic, ni->ni_chan)));

		/*
		 * Allocate and setup the beacon frame for AP or adhoc mode.
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_IBSS) {
			error = ath_beacon_alloc(sc, ni);
			if (error != 0)
				goto bad;
		}

		/*
		 * Configure the beacon and sleep timers.
		 */
		ath_beacon_config(sc);

		/* start periodic recalibration timer */
		callout_reset(&sc->sc_cal_ch, hz * ath_calinterval,
			ath_calibrate, sc);
	} else {
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		ath_hal_intrset(ah, sc->sc_imask);
		callout_stop(&sc->sc_cal_ch);		/* no calibration */
	}
	/*
	 * Reset the rate control state.
	 */
	ath_rate_ctl_reset(sc, nstate);
	/*
	 * Invoke the parent method to complete the work.
	 */
	return (*sc->sc_newstate)(ic, nstate, arg);
bad:
	callout_stop(&sc->sc_scan_ch);
	callout_stop(&sc->sc_cal_ch);
	/* NB: do not invoke the parent */
	return error;
}

/*
 * Setup driver-specific state for a newly associated node.
 * Note that we're called also on a re-associate, the isnew
 * param tells us if this is the first time or not.
 */
static void
ath_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	if (isnew) {
		struct ath_node *an = (struct ath_node *) ni;

		an->an_tx_ok = an->an_tx_err =
			an->an_tx_retr = an->an_tx_upper = 0;
		/* start with highest negotiated rate */
		/*
		 * XXX should do otherwise but only when
		 * the rate control algorithm is better.
		 */
		KASSERT(ni->ni_rates.rs_nrates > 0,
			("new association w/ no rates!"));
		ni->ni_txrate = ni->ni_rates.rs_nrates - 1;
	}
}

static int
ath_getchannels(struct ath_softc *sc, u_int cc, HAL_BOOL outdoor)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ath_hal *ah = sc->sc_ah;
	HAL_CHANNEL *chans;
	int i, ix, nchan;

	chans = malloc(IEEE80211_CHAN_MAX * sizeof(HAL_CHANNEL),
			M_TEMP, M_NOWAIT);
	if (chans == NULL) {
		if_printf(ifp, "unable to allocate channel table\n");
		return ENOMEM;
	}
	if (!ath_hal_init_channels(ah, chans, IEEE80211_CHAN_MAX, &nchan,
	    cc, HAL_MODE_ALL, outdoor)) {
		if_printf(ifp, "unable to collect channel list from hal\n");
		free(chans, M_TEMP);
		return EINVAL;
	}

	/*
	 * Convert HAL channels to ieee80211 ones and insert
	 * them in the table according to their channel number.
	 */
	for (i = 0; i < nchan; i++) {
		HAL_CHANNEL *c = &chans[i];
		ix = ath_hal_mhz2ieee(c->channel, c->channelFlags);
		if (ix > IEEE80211_CHAN_MAX) {
			if_printf(ifp, "bad hal channel %u (%u/%x) ignored\n",
				ix, c->channel, c->channelFlags);
			continue;
		}
		/* NB: flags are known to be compatible */
		if (ic->ic_channels[ix].ic_freq == 0) {
			ic->ic_channels[ix].ic_freq = c->channel;
			ic->ic_channels[ix].ic_flags = c->channelFlags;
		} else {
			/* channels overlap; e.g. 11g and 11b */
			ic->ic_channels[ix].ic_flags |= c->channelFlags;
		}
	}
	free(chans, M_TEMP);
	return 0;
}

static int
ath_rate_setup(struct ath_softc *sc, u_int mode)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = &sc->sc_ic;
	const HAL_RATE_TABLE *rt;
	struct ieee80211_rateset *rs;
	int i, maxrates;

	switch (mode) {
	case IEEE80211_MODE_11A:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11A);
		break;
	case IEEE80211_MODE_11B:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11B);
		break;
	case IEEE80211_MODE_11G:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_11G);
		break;
	case IEEE80211_MODE_TURBO:
		sc->sc_rates[mode] = ath_hal_getratetable(ah, HAL_MODE_TURBO);
		break;
	default:
		DPRINTF(("%s: invalid mode %u\n", __func__, mode));
		return 0;
	}
	rt = sc->sc_rates[mode];
	if (rt == NULL)
		return 0;
	if (rt->rateCount > IEEE80211_RATE_MAXSIZE) {
		DPRINTF(("%s: rate table too small (%u > %u)\n",
			__func__, rt->rateCount, IEEE80211_RATE_MAXSIZE));
		maxrates = IEEE80211_RATE_MAXSIZE;
	} else
		maxrates = rt->rateCount;
	rs = &ic->ic_sup_rates[mode];
	for (i = 0; i < maxrates; i++)
		rs->rs_rates[i] = rt->info[i].dot11Rate;
	rs->rs_nrates = maxrates;
	return 1;
}

static void
ath_setcurmode(struct ath_softc *sc, enum ieee80211_phymode mode)
{
	const HAL_RATE_TABLE *rt;
	int i;

	memset(sc->sc_rixmap, 0xff, sizeof(sc->sc_rixmap));
	rt = sc->sc_rates[mode];
	KASSERT(rt != NULL, ("no h/w rate set for phy mode %u", mode));
	for (i = 0; i < rt->rateCount; i++)
		sc->sc_rixmap[rt->info[i].dot11Rate & IEEE80211_RATE_VAL] = i;
	memset(sc->sc_hwmap, 0, sizeof(sc->sc_hwmap));
	for (i = 0; i < 32; i++)
		sc->sc_hwmap[i] = rt->info[rt->rateCodeToIndex[i]].dot11Rate;
	sc->sc_currates = rt;
	sc->sc_curmode = mode;
}

/*
 * Reset the rate control state for each 802.11 state transition.
 */
static void
ath_rate_ctl_reset(struct ath_softc *sc, enum ieee80211_state state)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ath_node *an;

	if (ic->ic_opmode != IEEE80211_M_STA) {
		/*
		 * When operating as a station the node table holds
		 * the AP's that were discovered during scanning.
		 * For any other operating mode we want to reset the
		 * tx rate state of each node.
		 */
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			ni->ni_txrate = 0;		/* use lowest rate */
			an = (struct ath_node *) ni;
			an->an_tx_ok = an->an_tx_err = an->an_tx_retr =
			    an->an_tx_upper = 0;
		}
	}
	/*
	 * Reset local xmit state; this is really only meaningful
	 * when operating in station or adhoc mode.
	 */
	ni = ic->ic_bss;
	an = (struct ath_node *) ni;
	an->an_tx_ok = an->an_tx_err = an->an_tx_retr = an->an_tx_upper = 0;
	if (state == IEEE80211_S_RUN) {
		/* start with highest negotiated rate */
		KASSERT(ni->ni_rates.rs_nrates > 0,
			("transition to RUN state w/ no rates!"));
		ni->ni_txrate = ni->ni_rates.rs_nrates - 1;
	} else {
		/* use lowest rate */
		ni->ni_txrate = 0;
	}
}

/* 
 * Examine and potentially adjust the transmit rate.
 */
static void
ath_rate_ctl(void *arg, struct ieee80211_node *ni)
{
	struct ath_softc *sc = arg;
	struct ath_node *an = (struct ath_node *) ni;
	struct ieee80211_rateset *rs = &ni->ni_rates;
	int mod = 0, orate, enough;

	/*
	 * Rate control
	 * XXX: very primitive version.
	 */
	sc->sc_stats.ast_rate_calls++;

	enough = (an->an_tx_ok + an->an_tx_err >= 10);

	/* no packet reached -> down */
	if (an->an_tx_err > 0 && an->an_tx_ok == 0)
		mod = -1;

	/* all packets needs retry in average -> down */
	if (enough && an->an_tx_ok < an->an_tx_retr)
		mod = -1;

	/* no error and less than 10% of packets needs retry -> up */
	if (enough && an->an_tx_err == 0 && an->an_tx_ok > an->an_tx_retr * 10)
		mod = 1;

	orate = ni->ni_txrate;
	switch (mod) {
	case 0:
		if (enough && an->an_tx_upper > 0)
			an->an_tx_upper--;
		break;
	case -1:
		if (ni->ni_txrate > 0) {
			ni->ni_txrate--;
			sc->sc_stats.ast_rate_drop++;
		}
		an->an_tx_upper = 0;
		break;
	case 1:
		if (++an->an_tx_upper < 2)
			break;
		an->an_tx_upper = 0;
		if (ni->ni_txrate + 1 < rs->rs_nrates) {
			ni->ni_txrate++;
			sc->sc_stats.ast_rate_raise++;
		}
		break;
	}

	if (ni->ni_txrate != orate) {
		printf("%s: %dM -> %dM (%d ok, %d err, %d retr)\n",
		    __func__,
		    (rs->rs_rates[orate] & IEEE80211_RATE_VAL) / 2,
		    (rs->rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL) / 2,
		    an->an_tx_ok, an->an_tx_err, an->an_tx_retr);
	}
	if (ni->ni_txrate != orate || enough)
		an->an_tx_ok = an->an_tx_err = an->an_tx_retr = 0;
}

#ifdef AR_DEBUG
static int
sysctl_hw_ath_dump(SYSCTL_HANDLER_ARGS)
{
	char dmode[64];
	int error;

	strncpy(dmode, "", sizeof(dmode) - 1);
	dmode[sizeof(dmode) - 1] = '\0';
	error = sysctl_handle_string(oidp, &dmode[0], sizeof(dmode), req);

	if (error == 0 && req->newptr != NULL) {
		struct ifnet *ifp;
		struct ath_softc *sc;

		ifp = ifunit("ath0");		/* XXX */
		if (!ifp)
			return EINVAL;
		sc = ifp->if_softc;
		if (strcmp(dmode, "hal") == 0)
			ath_hal_dumpstate(sc->sc_ah);
		else if (strcmp(dmode, "eeprom") == 0)
			ath_hal_dumpeeprom(sc->sc_ah);
		else if (strcmp(dmode, "rfgain") == 0)
			ath_hal_dumprfgain(sc->sc_ah);
		else if (strcmp(dmode, "ani") == 0)
			ath_hal_dumpani(sc->sc_ah);
		else
			return EINVAL;
	}
	return error;
}
SYSCTL_PROC(_hw_ath, OID_AUTO, dump, CTLTYPE_STRING | CTLFLAG_RW,
	0, 0, sysctl_hw_ath_dump, "A", "Dump driver state");

static void
ath_printrxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds;
	int i;

	for (i = 0, ds = bf->bf_desc; i < bf->bf_nseg; i++, ds++) {
		printf("R%d (%p %p) %08x %08x %08x %08x %08x %08x %c\n",
		    i, ds, (struct ath_desc *)bf->bf_daddr + i,
		    ds->ds_link, ds->ds_data,
		    ds->ds_ctl0, ds->ds_ctl1,
		    ds->ds_hw[0], ds->ds_hw[1],
		    !done ? ' ' : (ds->ds_rxstat.rs_status == 0) ? '*' : '!');
	}
}

static void
ath_printtxbuf(struct ath_buf *bf, int done)
{
	struct ath_desc *ds;
	int i;

	for (i = 0, ds = bf->bf_desc; i < bf->bf_nseg; i++, ds++) {
		printf("T%d (%p %p) %08x %08x %08x %08x %08x %08x %08x %08x %c\n",
		    i, ds, (struct ath_desc *)bf->bf_daddr + i,
		    ds->ds_link, ds->ds_data,
		    ds->ds_ctl0, ds->ds_ctl1,
		    ds->ds_hw[0], ds->ds_hw[1], ds->ds_hw[2], ds->ds_hw[3],
		    !done ? ' ' : (ds->ds_txstat.ts_status == 0) ? '*' : '!');
	}
}
#endif /* AR_DEBUG */
