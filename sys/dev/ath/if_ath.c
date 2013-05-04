/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
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
#include "opt_ath.h"
/*
 * This is needed for register operations which are performed
 * by the driver - eg, calls to ath_hal_gettsf32().
 *
 * It's also required for any AH_DEBUG checks in here, eg the
 * module dependencies.
 */
#include "opt_ah.h"
#include "opt_wlan.h"

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
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>
#include <sys/module.h>
#include <sys/ktr.h>
#include <sys/smp.h>	/* for mp_ncpus */

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#ifdef IEEE80211_SUPPORT_TDMA
#include <net80211/ieee80211_tdma.h>
#endif

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_hal/ah_devid.h>		/* XXX for softled */
#include <dev/ath/ath_hal/ah_diagcodes.h>

#include <dev/ath/if_ath_debug.h>
#include <dev/ath/if_ath_misc.h>
#include <dev/ath/if_ath_tsf.h>
#include <dev/ath/if_ath_tx.h>
#include <dev/ath/if_ath_sysctl.h>
#include <dev/ath/if_ath_led.h>
#include <dev/ath/if_ath_keycache.h>
#include <dev/ath/if_ath_rx.h>
#include <dev/ath/if_ath_rx_edma.h>
#include <dev/ath/if_ath_tx_edma.h>
#include <dev/ath/if_ath_beacon.h>
#include <dev/ath/if_ath_spectral.h>
#include <dev/ath/if_athdfs.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#ifdef	ATH_DEBUG_ALQ
#include <dev/ath/if_ath_alq.h>
#endif

/*
 * Only enable this if you're working on PS-POLL support.
 */
#undef	ATH_SW_PSQ

/*
 * ATH_BCBUF determines the number of vap's that can transmit
 * beacons and also (currently) the number of vap's that can
 * have unique mac addresses/bssid.  When staggering beacons
 * 4 is probably a good max as otherwise the beacons become
 * very closely spaced and there is limited time for cab q traffic
 * to go out.  You can burst beacons instead but that is not good
 * for stations in power save and at some point you really want
 * another radio (and channel).
 *
 * The limit on the number of mac addresses is tied to our use of
 * the U/L bit and tracking addresses in a byte; it would be
 * worthwhile to allow more for applications like proxy sta.
 */
CTASSERT(ATH_BCBUF <= 8);

static struct ieee80211vap *ath_vap_create(struct ieee80211com *,
		    const char [IFNAMSIZ], int, enum ieee80211_opmode, int,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN]);
static void	ath_vap_delete(struct ieee80211vap *);
static void	ath_init(void *);
static void	ath_stop_locked(struct ifnet *);
static void	ath_stop(struct ifnet *);
static int	ath_reset_vap(struct ieee80211vap *, u_long);
static void	ath_start_queue(struct ifnet *ifp);
static int	ath_media_change(struct ifnet *);
static void	ath_watchdog(void *);
static int	ath_ioctl(struct ifnet *, u_long, caddr_t);
static void	ath_fatal_proc(void *, int);
static void	ath_bmiss_vap(struct ieee80211vap *);
static void	ath_bmiss_proc(void *, int);
static void	ath_key_update_begin(struct ieee80211vap *);
static void	ath_key_update_end(struct ieee80211vap *);
static void	ath_update_mcast(struct ifnet *);
static void	ath_update_promisc(struct ifnet *);
static void	ath_updateslot(struct ifnet *);
static void	ath_bstuck_proc(void *, int);
static void	ath_reset_proc(void *, int);
static int	ath_desc_alloc(struct ath_softc *);
static void	ath_desc_free(struct ath_softc *);
static struct ieee80211_node *ath_node_alloc(struct ieee80211vap *,
			const uint8_t [IEEE80211_ADDR_LEN]);
static void	ath_node_cleanup(struct ieee80211_node *);
static void	ath_node_free(struct ieee80211_node *);
static void	ath_node_getsignal(const struct ieee80211_node *,
			int8_t *, int8_t *);
static void	ath_txq_init(struct ath_softc *sc, struct ath_txq *, int);
static struct ath_txq *ath_txq_setup(struct ath_softc*, int qtype, int subtype);
static int	ath_tx_setup(struct ath_softc *, int, int);
static void	ath_tx_cleanupq(struct ath_softc *, struct ath_txq *);
static void	ath_tx_cleanup(struct ath_softc *);
static int	ath_tx_processq(struct ath_softc *sc, struct ath_txq *txq,
		    int dosched);
static void	ath_tx_proc_q0(void *, int);
static void	ath_tx_proc_q0123(void *, int);
static void	ath_tx_proc(void *, int);
static void	ath_txq_sched_tasklet(void *, int);
static int	ath_chan_set(struct ath_softc *, struct ieee80211_channel *);
static void	ath_chan_change(struct ath_softc *, struct ieee80211_channel *);
static void	ath_scan_start(struct ieee80211com *);
static void	ath_scan_end(struct ieee80211com *);
static void	ath_set_channel(struct ieee80211com *);
#ifdef	ATH_ENABLE_11N
static void	ath_update_chw(struct ieee80211com *);
#endif	/* ATH_ENABLE_11N */
static void	ath_calibrate(void *);
static int	ath_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void	ath_setup_stationkey(struct ieee80211_node *);
static void	ath_newassoc(struct ieee80211_node *, int);
static int	ath_setregdomain(struct ieee80211com *,
		    struct ieee80211_regdomain *, int,
		    struct ieee80211_channel []);
static void	ath_getradiocaps(struct ieee80211com *, int, int *,
		    struct ieee80211_channel []);
static int	ath_getchannels(struct ath_softc *);

static int	ath_rate_setup(struct ath_softc *, u_int mode);
static void	ath_setcurmode(struct ath_softc *, enum ieee80211_phymode);

static void	ath_announce(struct ath_softc *);

static void	ath_dfs_tasklet(void *, int);
static void	ath_node_powersave(struct ieee80211_node *, int);
static int	ath_node_set_tim(struct ieee80211_node *, int);

#ifdef IEEE80211_SUPPORT_TDMA
#include <dev/ath/if_ath_tdma.h>
#endif

SYSCTL_DECL(_hw_ath);

/* XXX validate sysctl values */
static	int ath_longcalinterval = 30;		/* long cals every 30 secs */
SYSCTL_INT(_hw_ath, OID_AUTO, longcal, CTLFLAG_RW, &ath_longcalinterval,
	    0, "long chip calibration interval (secs)");
static	int ath_shortcalinterval = 100;		/* short cals every 100 ms */
SYSCTL_INT(_hw_ath, OID_AUTO, shortcal, CTLFLAG_RW, &ath_shortcalinterval,
	    0, "short chip calibration interval (msecs)");
static	int ath_resetcalinterval = 20*60;	/* reset cal state 20 mins */
SYSCTL_INT(_hw_ath, OID_AUTO, resetcal, CTLFLAG_RW, &ath_resetcalinterval,
	    0, "reset chip calibration results (secs)");
static	int ath_anicalinterval = 100;		/* ANI calibration - 100 msec */
SYSCTL_INT(_hw_ath, OID_AUTO, anical, CTLFLAG_RW, &ath_anicalinterval,
	    0, "ANI calibration (msecs)");

int ath_rxbuf = ATH_RXBUF;		/* # rx buffers to allocate */
SYSCTL_INT(_hw_ath, OID_AUTO, rxbuf, CTLFLAG_RW, &ath_rxbuf,
	    0, "rx buffers allocated");
TUNABLE_INT("hw.ath.rxbuf", &ath_rxbuf);
int ath_txbuf = ATH_TXBUF;		/* # tx buffers to allocate */
SYSCTL_INT(_hw_ath, OID_AUTO, txbuf, CTLFLAG_RW, &ath_txbuf,
	    0, "tx buffers allocated");
TUNABLE_INT("hw.ath.txbuf", &ath_txbuf);
int ath_txbuf_mgmt = ATH_MGMT_TXBUF;	/* # mgmt tx buffers to allocate */
SYSCTL_INT(_hw_ath, OID_AUTO, txbuf_mgmt, CTLFLAG_RW, &ath_txbuf_mgmt,
	    0, "tx (mgmt) buffers allocated");
TUNABLE_INT("hw.ath.txbuf_mgmt", &ath_txbuf_mgmt);

int ath_bstuck_threshold = 4;		/* max missed beacons */
SYSCTL_INT(_hw_ath, OID_AUTO, bstuck, CTLFLAG_RW, &ath_bstuck_threshold,
	    0, "max missed beacon xmits before chip reset");

MALLOC_DEFINE(M_ATHDEV, "athdev", "ath driver dma buffers");

void
ath_legacy_attach_comp_func(struct ath_softc *sc)
{

	/*
	 * Special case certain configurations.  Note the
	 * CAB queue is handled by these specially so don't
	 * include them when checking the txq setup mask.
	 */
	switch (sc->sc_txqsetup &~ (1<<sc->sc_cabq->axq_qnum)) {
	case 0x01:
		TASK_INIT(&sc->sc_txtask, 0, ath_tx_proc_q0, sc);
		break;
	case 0x0f:
		TASK_INIT(&sc->sc_txtask, 0, ath_tx_proc_q0123, sc);
		break;
	default:
		TASK_INIT(&sc->sc_txtask, 0, ath_tx_proc, sc);
		break;
	}
}

#define	HAL_MODE_HT20 (HAL_MODE_11NG_HT20 | HAL_MODE_11NA_HT20)
#define	HAL_MODE_HT40 \
	(HAL_MODE_11NG_HT40PLUS | HAL_MODE_11NG_HT40MINUS | \
	HAL_MODE_11NA_HT40PLUS | HAL_MODE_11NA_HT40MINUS)
int
ath_attach(u_int16_t devid, struct ath_softc *sc)
{
	struct ifnet *ifp;
	struct ieee80211com *ic;
	struct ath_hal *ah = NULL;
	HAL_STATUS status;
	int error = 0, i;
	u_int wmodes;
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	int rx_chainmask, tx_chainmask;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: devid 0x%x\n", __func__, devid);

	CURVNET_SET(vnet0);
	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		error = ENOSPC;
		CURVNET_RESTORE();
		goto bad;
	}
	ic = ifp->if_l2com;

	/* set these up early for if_printf use */
	if_initname(ifp, device_get_name(sc->sc_dev),
		device_get_unit(sc->sc_dev));
	CURVNET_RESTORE();

	ah = ath_hal_attach(devid, sc, sc->sc_st, sc->sc_sh,
	    sc->sc_eepromdata, &status);
	if (ah == NULL) {
		if_printf(ifp, "unable to attach hardware; HAL status %u\n",
			status);
		error = ENXIO;
		goto bad;
	}
	sc->sc_ah = ah;
	sc->sc_invalid = 0;	/* ready to go, enable interrupt handling */
#ifdef	ATH_DEBUG
	sc->sc_debug = ath_debug;
#endif

	/*
	 * Setup the DMA/EDMA functions based on the current
	 * hardware support.
	 *
	 * This is required before the descriptors are allocated.
	 */
	if (ath_hal_hasedma(sc->sc_ah)) {
		sc->sc_isedma = 1;
		ath_recv_setup_edma(sc);
		ath_xmit_setup_edma(sc);
	} else {
		ath_recv_setup_legacy(sc);
		ath_xmit_setup_legacy(sc);
	}

	/*
	 * Check if the MAC has multi-rate retry support.
	 * We do this by trying to setup a fake extended
	 * descriptor.  MAC's that don't have support will
	 * return false w/o doing anything.  MAC's that do
	 * support it will return true w/o doing anything.
	 */
	sc->sc_mrretry = ath_hal_setupxtxdesc(ah, NULL, 0,0, 0,0, 0,0);

	/*
	 * Check if the device has hardware counters for PHY
	 * errors.  If so we need to enable the MIB interrupt
	 * so we can act on stat triggers.
	 */
	if (ath_hal_hwphycounters(ah))
		sc->sc_needmib = 1;

	/*
	 * Get the hardware key cache size.
	 */
	sc->sc_keymax = ath_hal_keycachesize(ah);
	if (sc->sc_keymax > ATH_KEYMAX) {
		if_printf(ifp, "Warning, using only %u of %u key cache slots\n",
			ATH_KEYMAX, sc->sc_keymax);
		sc->sc_keymax = ATH_KEYMAX;
	}
	/*
	 * Reset the key cache since some parts do not
	 * reset the contents on initial power up.
	 */
	for (i = 0; i < sc->sc_keymax; i++)
		ath_hal_keyreset(ah, i);

	/*
	 * Collect the default channel list.
	 */
	error = ath_getchannels(sc);
	if (error != 0)
		goto bad;

	/*
	 * Setup rate tables for all potential media types.
	 */
	ath_rate_setup(sc, IEEE80211_MODE_11A);
	ath_rate_setup(sc, IEEE80211_MODE_11B);
	ath_rate_setup(sc, IEEE80211_MODE_11G);
	ath_rate_setup(sc, IEEE80211_MODE_TURBO_A);
	ath_rate_setup(sc, IEEE80211_MODE_TURBO_G);
	ath_rate_setup(sc, IEEE80211_MODE_STURBO_A);
	ath_rate_setup(sc, IEEE80211_MODE_11NA);
	ath_rate_setup(sc, IEEE80211_MODE_11NG);
	ath_rate_setup(sc, IEEE80211_MODE_HALF);
	ath_rate_setup(sc, IEEE80211_MODE_QUARTER);

	/* NB: setup here so ath_rate_update is happy */
	ath_setcurmode(sc, IEEE80211_MODE_11A);

	/*
	 * Allocate TX descriptors and populate the lists.
	 */
	error = ath_desc_alloc(sc);
	if (error != 0) {
		if_printf(ifp, "failed to allocate TX descriptors: %d\n",
		    error);
		goto bad;
	}
	error = ath_txdma_setup(sc);
	if (error != 0) {
		if_printf(ifp, "failed to allocate TX descriptors: %d\n",
		    error);
		goto bad;
	}

	/*
	 * Allocate RX descriptors and populate the lists.
	 */
	error = ath_rxdma_setup(sc);
	if (error != 0) {
		if_printf(ifp, "failed to allocate RX descriptors: %d\n",
		    error);
		goto bad;
	}

	callout_init_mtx(&sc->sc_cal_ch, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->sc_wd_ch, &sc->sc_mtx, 0);

	ATH_TXBUF_LOCK_INIT(sc);

	sc->sc_tq = taskqueue_create("ath_taskq", M_NOWAIT,
		taskqueue_thread_enqueue, &sc->sc_tq);
	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET,
		"%s taskq", ifp->if_xname);

	TASK_INIT(&sc->sc_rxtask, 0, sc->sc_rx.recv_tasklet, sc);
	TASK_INIT(&sc->sc_bmisstask, 0, ath_bmiss_proc, sc);
	TASK_INIT(&sc->sc_bstucktask,0, ath_bstuck_proc, sc);
	TASK_INIT(&sc->sc_resettask,0, ath_reset_proc, sc);
	TASK_INIT(&sc->sc_txqtask, 0, ath_txq_sched_tasklet, sc);
	TASK_INIT(&sc->sc_fataltask, 0, ath_fatal_proc, sc);

	/* XXX make this a higher priority taskqueue? */
	TASK_INIT(&sc->sc_txpkttask, 0, ath_start_task, sc);

	/*
	 * Allocate hardware transmit queues: one queue for
	 * beacon frames and one data queue for each QoS
	 * priority.  Note that the hal handles resetting
	 * these queues at the needed time.
	 *
	 * XXX PS-Poll
	 */
	sc->sc_bhalq = ath_beaconq_setup(sc);
	if (sc->sc_bhalq == (u_int) -1) {
		if_printf(ifp, "unable to setup a beacon xmit queue!\n");
		error = EIO;
		goto bad2;
	}
	sc->sc_cabq = ath_txq_setup(sc, HAL_TX_QUEUE_CAB, 0);
	if (sc->sc_cabq == NULL) {
		if_printf(ifp, "unable to setup CAB xmit queue!\n");
		error = EIO;
		goto bad2;
	}
	/* NB: insure BK queue is the lowest priority h/w queue */
	if (!ath_tx_setup(sc, WME_AC_BK, HAL_WME_AC_BK)) {
		if_printf(ifp, "unable to setup xmit queue for %s traffic!\n",
			ieee80211_wme_acnames[WME_AC_BK]);
		error = EIO;
		goto bad2;
	}
	if (!ath_tx_setup(sc, WME_AC_BE, HAL_WME_AC_BE) ||
	    !ath_tx_setup(sc, WME_AC_VI, HAL_WME_AC_VI) ||
	    !ath_tx_setup(sc, WME_AC_VO, HAL_WME_AC_VO)) {
		/*
		 * Not enough hardware tx queues to properly do WME;
		 * just punt and assign them all to the same h/w queue.
		 * We could do a better job of this if, for example,
		 * we allocate queues when we switch from station to
		 * AP mode.
		 */
		if (sc->sc_ac2q[WME_AC_VI] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_VI]);
		if (sc->sc_ac2q[WME_AC_BE] != NULL)
			ath_tx_cleanupq(sc, sc->sc_ac2q[WME_AC_BE]);
		sc->sc_ac2q[WME_AC_BE] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VI] = sc->sc_ac2q[WME_AC_BK];
		sc->sc_ac2q[WME_AC_VO] = sc->sc_ac2q[WME_AC_BK];
	}

	/*
	 * Attach the TX completion function.
	 *
	 * The non-EDMA chips may have some special case optimisations;
	 * this method gives everyone a chance to attach cleanly.
	 */
	sc->sc_tx.xmit_attach_comp_func(sc);

	/*
	 * Setup rate control.  Some rate control modules
	 * call back to change the anntena state so expose
	 * the necessary entry points.
	 * XXX maybe belongs in struct ath_ratectrl?
	 */
	sc->sc_setdefantenna = ath_setdefantenna;
	sc->sc_rc = ath_rate_attach(sc);
	if (sc->sc_rc == NULL) {
		error = EIO;
		goto bad2;
	}

	/* Attach DFS module */
	if (! ath_dfs_attach(sc)) {
		device_printf(sc->sc_dev,
		    "%s: unable to attach DFS\n", __func__);
		error = EIO;
		goto bad2;
	}

	/* Attach spectral module */
	if (ath_spectral_attach(sc) < 0) {
		device_printf(sc->sc_dev,
		    "%s: unable to attach spectral\n", __func__);
		error = EIO;
		goto bad2;
	}

	/* Start DFS processing tasklet */
	TASK_INIT(&sc->sc_dfstask, 0, ath_dfs_tasklet, sc);

	/* Configure LED state */
	sc->sc_blinking = 0;
	sc->sc_ledstate = 1;
	sc->sc_ledon = 0;			/* low true */
	sc->sc_ledidle = (2700*hz)/1000;	/* 2.7sec */
	callout_init(&sc->sc_ledtimer, CALLOUT_MPSAFE);

	/*
	 * Don't setup hardware-based blinking.
	 *
	 * Although some NICs may have this configured in the
	 * default reset register values, the user may wish
	 * to alter which pins have which function.
	 *
	 * The reference driver attaches the MAC network LED to GPIO1 and
	 * the MAC power LED to GPIO2.  However, the DWA-552 cardbus
	 * NIC has these reversed.
	 */
	sc->sc_hardled = (1 == 0);
	sc->sc_led_net_pin = -1;
	sc->sc_led_pwr_pin = -1;
	/*
	 * Auto-enable soft led processing for IBM cards and for
	 * 5211 minipci cards.  Users can also manually enable/disable
	 * support with a sysctl.
	 */
	sc->sc_softled = (devid == AR5212_DEVID_IBM || devid == AR5211_DEVID);
	ath_led_config(sc);
	ath_hal_setledstate(ah, HAL_LED_INIT);

	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_start = ath_start_queue;
	ifp->if_ioctl = ath_ioctl;
	ifp->if_init = ath_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	/* XXX not right but it's not used anywhere important */
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps =
		  IEEE80211_C_STA		/* station mode */
		| IEEE80211_C_IBSS		/* ibss, nee adhoc, mode */
		| IEEE80211_C_HOSTAP		/* hostap mode */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_AHDEMO		/* adhoc demo mode */
		| IEEE80211_C_WDS		/* 4-address traffic works */
		| IEEE80211_C_MBSS		/* mesh point link mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_WPA		/* capable of WPA1+WPA2 */
#ifndef	ATH_ENABLE_11N
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
#endif
		| IEEE80211_C_TXFRAG		/* handle tx frags */
#ifdef	ATH_ENABLE_DFS
		| IEEE80211_C_DFS		/* Enable radar detection */
#endif
		;
	/*
	 * Query the hal to figure out h/w crypto support.
	 */
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_WEP))
		ic->ic_cryptocaps |= IEEE80211_CRYPTO_WEP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_OCB))
		ic->ic_cryptocaps |= IEEE80211_CRYPTO_AES_OCB;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_AES_CCM))
		ic->ic_cryptocaps |= IEEE80211_CRYPTO_AES_CCM;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_CKIP))
		ic->ic_cryptocaps |= IEEE80211_CRYPTO_CKIP;
	if (ath_hal_ciphersupported(ah, HAL_CIPHER_TKIP)) {
		ic->ic_cryptocaps |= IEEE80211_CRYPTO_TKIP;
		/*
		 * Check if h/w does the MIC and/or whether the
		 * separate key cache entries are required to
		 * handle both tx+rx MIC keys.
		 */
		if (ath_hal_ciphersupported(ah, HAL_CIPHER_MIC))
			ic->ic_cryptocaps |= IEEE80211_CRYPTO_TKIPMIC;
		/*
		 * If the h/w supports storing tx+rx MIC keys
		 * in one cache slot automatically enable use.
		 */
		if (ath_hal_hastkipsplit(ah) ||
		    !ath_hal_settkipsplit(ah, AH_FALSE))
			sc->sc_splitmic = 1;
		/*
		 * If the h/w can do TKIP MIC together with WME then
		 * we use it; otherwise we force the MIC to be done
		 * in software by the net80211 layer.
		 */
		if (ath_hal_haswmetkipmic(ah))
			sc->sc_wmetkipmic = 1;
	}
	sc->sc_hasclrkey = ath_hal_ciphersupported(ah, HAL_CIPHER_CLR);
	/*
	 * Check for multicast key search support.
	 */
	if (ath_hal_hasmcastkeysearch(sc->sc_ah) &&
	    !ath_hal_getmcastkeysearch(sc->sc_ah)) {
		ath_hal_setmcastkeysearch(sc->sc_ah, 1);
	}
	sc->sc_mcastkey = ath_hal_getmcastkeysearch(ah);
	/*
	 * Mark key cache slots associated with global keys
	 * as in use.  If we knew TKIP was not to be used we
	 * could leave the +32, +64, and +32+64 slots free.
	 */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		setbit(sc->sc_keymap, i);
		setbit(sc->sc_keymap, i+64);
		if (sc->sc_splitmic) {
			setbit(sc->sc_keymap, i+32);
			setbit(sc->sc_keymap, i+32+64);
		}
	}
	/*
	 * TPC support can be done either with a global cap or
	 * per-packet support.  The latter is not available on
	 * all parts.  We're a bit pedantic here as all parts
	 * support a global cap.
	 */
	if (ath_hal_hastpc(ah) || ath_hal_hastxpowlimit(ah))
		ic->ic_caps |= IEEE80211_C_TXPMGT;

	/*
	 * Mark WME capability only if we have sufficient
	 * hardware queues to do proper priority scheduling.
	 */
	if (sc->sc_ac2q[WME_AC_BE] != sc->sc_ac2q[WME_AC_BK])
		ic->ic_caps |= IEEE80211_C_WME;
	/*
	 * Check for misc other capabilities.
	 */
	if (ath_hal_hasbursting(ah))
		ic->ic_caps |= IEEE80211_C_BURST;
	sc->sc_hasbmask = ath_hal_hasbssidmask(ah);
	sc->sc_hasbmatch = ath_hal_hasbssidmatch(ah);
	sc->sc_hastsfadd = ath_hal_hastsfadjust(ah);
	sc->sc_rxslink = ath_hal_self_linked_final_rxdesc(ah);
	sc->sc_rxtsf32 = ath_hal_has_long_rxdesc_tsf(ah);
	if (ath_hal_hasfastframes(ah))
		ic->ic_caps |= IEEE80211_C_FF;
	wmodes = ath_hal_getwirelessmodes(ah);
	if (wmodes & (HAL_MODE_108G|HAL_MODE_TURBO))
		ic->ic_caps |= IEEE80211_C_TURBOP;
#ifdef IEEE80211_SUPPORT_TDMA
	if (ath_hal_macversion(ah) > 0x78) {
		ic->ic_caps |= IEEE80211_C_TDMA; /* capable of TDMA */
		ic->ic_tdma_update = ath_tdma_update;
	}
#endif

	/*
	 * TODO: enforce that at least this many frames are available
	 * in the txbuf list before allowing data frames (raw or
	 * otherwise) to be transmitted.
	 */
	sc->sc_txq_data_minfree = 10;
	/*
	 * Leave this as default to maintain legacy behaviour.
	 * Shortening the cabq/mcastq may end up causing some
	 * undesirable behaviour.
	 */
	sc->sc_txq_mcastq_maxdepth = ath_txbuf;

	/* Enable CABQ by default */
	sc->sc_cabq_enable = 1;

	/*
	 * Allow the TX and RX chainmasks to be overridden by
	 * environment variables and/or device.hints.
	 *
	 * This must be done early - before the hardware is
	 * calibrated or before the 802.11n stream calculation
	 * is done.
	 */
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev), "rx_chainmask",
	    &rx_chainmask) == 0) {
		device_printf(sc->sc_dev, "Setting RX chainmask to 0x%x\n",
		    rx_chainmask);
		(void) ath_hal_setrxchainmask(sc->sc_ah, rx_chainmask);
	}
	if (resource_int_value(device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev), "tx_chainmask",
	    &tx_chainmask) == 0) {
		device_printf(sc->sc_dev, "Setting TX chainmask to 0x%x\n",
		    tx_chainmask);
		(void) ath_hal_settxchainmask(sc->sc_ah, tx_chainmask);
	}

	/*
	 * Query the TX/RX chainmask configuration.
	 *
	 * This is only relevant for 11n devices.
	 */
	ath_hal_getrxchainmask(ah, &sc->sc_rxchainmask);
	ath_hal_gettxchainmask(ah, &sc->sc_txchainmask);

	/*
	 * Disable MRR with protected frames by default.
	 * Only 802.11n series NICs can handle this.
	 */
	sc->sc_mrrprot = 0;	/* XXX should be a capability */

	/*
	 * Query the enterprise mode information the HAL.
	 */
	if (ath_hal_getcapability(ah, HAL_CAP_ENTERPRISE_MODE, 0,
	    &sc->sc_ent_cfg) == HAL_OK)
		sc->sc_use_ent = 1;

#ifdef	ATH_ENABLE_11N
	/*
	 * Query HT capabilities
	 */
	if (ath_hal_getcapability(ah, HAL_CAP_HT, 0, NULL) == HAL_OK &&
	    (wmodes & (HAL_MODE_HT20 | HAL_MODE_HT40))) {
		uint32_t rxs, txs;

		device_printf(sc->sc_dev, "[HT] enabling HT modes\n");

		sc->sc_mrrprot = 1;	/* XXX should be a capability */

		ic->ic_htcaps = IEEE80211_HTC_HT	/* HT operation */
			    | IEEE80211_HTC_AMPDU	/* A-MPDU tx/rx */
			    | IEEE80211_HTC_AMSDU	/* A-MSDU tx/rx */
			    | IEEE80211_HTCAP_MAXAMSDU_3839
			    				/* max A-MSDU length */
			    | IEEE80211_HTCAP_SMPS_OFF;	/* SM power save off */
			;

		/*
		 * Enable short-GI for HT20 only if the hardware
		 * advertises support.
		 * Notably, anything earlier than the AR9287 doesn't.
		 */
		if ((ath_hal_getcapability(ah,
		    HAL_CAP_HT20_SGI, 0, NULL) == HAL_OK) &&
		    (wmodes & HAL_MODE_HT20)) {
			device_printf(sc->sc_dev,
			    "[HT] enabling short-GI in 20MHz mode\n");
			ic->ic_htcaps |= IEEE80211_HTCAP_SHORTGI20;
		}

		if (wmodes & HAL_MODE_HT40)
			ic->ic_htcaps |= IEEE80211_HTCAP_CHWIDTH40
			    |  IEEE80211_HTCAP_SHORTGI40;

		/*
		 * TX/RX streams need to be taken into account when
		 * negotiating which MCS rates it'll receive and
		 * what MCS rates are available for TX.
		 */
		(void) ath_hal_getcapability(ah, HAL_CAP_STREAMS, 0, &txs);
		(void) ath_hal_getcapability(ah, HAL_CAP_STREAMS, 1, &rxs);
		ic->ic_txstream = txs;
		ic->ic_rxstream = rxs;

		/*
		 * Setup TX and RX STBC based on what the HAL allows and
		 * the currently configured chainmask set.
		 * Ie - don't enable STBC TX if only one chain is enabled.
		 * STBC RX is fine on a single RX chain; it just won't
		 * provide any real benefit.
		 */
		if (ath_hal_getcapability(ah, HAL_CAP_RX_STBC, 0,
		    NULL) == HAL_OK) {
			sc->sc_rx_stbc = 1;
			device_printf(sc->sc_dev,
			    "[HT] 1 stream STBC receive enabled\n");
			ic->ic_htcaps |= IEEE80211_HTCAP_RXSTBC_1STREAM;
		}
		if (txs > 1 && ath_hal_getcapability(ah, HAL_CAP_TX_STBC, 0,
		    NULL) == HAL_OK) {
			sc->sc_tx_stbc = 1;
			device_printf(sc->sc_dev,
			    "[HT] 1 stream STBC transmit enabled\n");
			ic->ic_htcaps |= IEEE80211_HTCAP_TXSTBC;
		}

		(void) ath_hal_getcapability(ah, HAL_CAP_RTS_AGGR_LIMIT, 1,
		    &sc->sc_rts_aggr_limit);
		if (sc->sc_rts_aggr_limit != (64 * 1024))
			device_printf(sc->sc_dev,
			    "[HT] RTS aggregates limited to %d KiB\n",
			    sc->sc_rts_aggr_limit / 1024);

		device_printf(sc->sc_dev,
		    "[HT] %d RX streams; %d TX streams\n", rxs, txs);
	}
#endif

	/*
	 * Initial aggregation settings.
	 */
	sc->sc_hwq_limit = ATH_AGGR_MIN_QDEPTH;
	sc->sc_tid_hwq_lo = ATH_AGGR_SCHED_LOW;
	sc->sc_tid_hwq_hi = ATH_AGGR_SCHED_HIGH;
	sc->sc_aggr_limit = ATH_AGGR_MAXSIZE;
	sc->sc_delim_min_pad = 0;

	/*
	 * Check if the hardware requires PCI register serialisation.
	 * Some of the Owl based MACs require this.
	 */
	if (mp_ncpus > 1 &&
	    ath_hal_getcapability(ah, HAL_CAP_SERIALISE_WAR,
	     0, NULL) == HAL_OK) {
		sc->sc_ah->ah_config.ah_serialise_reg_war = 1;
		device_printf(sc->sc_dev,
		    "Enabling register serialisation\n");
	}

	/*
	 * Initialise the deferred completed RX buffer list.
	 */
	TAILQ_INIT(&sc->sc_rx_rxlist[HAL_RX_QUEUE_HP]);
	TAILQ_INIT(&sc->sc_rx_rxlist[HAL_RX_QUEUE_LP]);

	/*
	 * Indicate we need the 802.11 header padded to a
	 * 32-bit boundary for 4-address and QoS frames.
	 */
	ic->ic_flags |= IEEE80211_F_DATAPAD;

	/*
	 * Query the hal about antenna support.
	 */
	sc->sc_defant = ath_hal_getdefantenna(ah);

	/*
	 * Not all chips have the VEOL support we want to
	 * use with IBSS beacons; check here for it.
	 */
	sc->sc_hasveol = ath_hal_hasveol(ah);

	/* get mac address from hardware */
	ath_hal_getmac(ah, macaddr);
	if (sc->sc_hasbmask)
		ath_hal_getbssidmask(ah, sc->sc_hwbssidmask);

	/* NB: used to size node table key mapping array */
	ic->ic_max_keyix = sc->sc_keymax;
	/* call MI attach routine. */
	ieee80211_ifattach(ic, macaddr);
	ic->ic_setregdomain = ath_setregdomain;
	ic->ic_getradiocaps = ath_getradiocaps;
	sc->sc_opmode = HAL_M_STA;

	/* override default methods */
	ic->ic_newassoc = ath_newassoc;
	ic->ic_updateslot = ath_updateslot;
	ic->ic_wme.wme_update = ath_wme_update;
	ic->ic_vap_create = ath_vap_create;
	ic->ic_vap_delete = ath_vap_delete;
	ic->ic_raw_xmit = ath_raw_xmit;
	ic->ic_update_mcast = ath_update_mcast;
	ic->ic_update_promisc = ath_update_promisc;
	ic->ic_node_alloc = ath_node_alloc;
	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = ath_node_free;
	sc->sc_node_cleanup = ic->ic_node_cleanup;
	ic->ic_node_cleanup = ath_node_cleanup;
	ic->ic_node_getsignal = ath_node_getsignal;
	ic->ic_scan_start = ath_scan_start;
	ic->ic_scan_end = ath_scan_end;
	ic->ic_set_channel = ath_set_channel;
#ifdef	ATH_ENABLE_11N
	/* 802.11n specific - but just override anyway */
	sc->sc_addba_request = ic->ic_addba_request;
	sc->sc_addba_response = ic->ic_addba_response;
	sc->sc_addba_stop = ic->ic_addba_stop;
	sc->sc_bar_response = ic->ic_bar_response;
	sc->sc_addba_response_timeout = ic->ic_addba_response_timeout;

	ic->ic_addba_request = ath_addba_request;
	ic->ic_addba_response = ath_addba_response;
	ic->ic_addba_response_timeout = ath_addba_response_timeout;
	ic->ic_addba_stop = ath_addba_stop;
	ic->ic_bar_response = ath_bar_response;

	ic->ic_update_chw = ath_update_chw;
#endif	/* ATH_ENABLE_11N */

#ifdef	ATH_ENABLE_RADIOTAP_VENDOR_EXT
	/*
	 * There's one vendor bitmap entry in the RX radiotap
	 * header; make sure that's taken into account.
	 */
	ieee80211_radiotap_attachv(ic,
	    &sc->sc_tx_th.wt_ihdr, sizeof(sc->sc_tx_th), 0,
		ATH_TX_RADIOTAP_PRESENT,
	    &sc->sc_rx_th.wr_ihdr, sizeof(sc->sc_rx_th), 1,
		ATH_RX_RADIOTAP_PRESENT);
#else
	/*
	 * No vendor bitmap/extensions are present.
	 */
	ieee80211_radiotap_attach(ic,
	    &sc->sc_tx_th.wt_ihdr, sizeof(sc->sc_tx_th),
		ATH_TX_RADIOTAP_PRESENT,
	    &sc->sc_rx_th.wr_ihdr, sizeof(sc->sc_rx_th),
		ATH_RX_RADIOTAP_PRESENT);
#endif	/* ATH_ENABLE_RADIOTAP_VENDOR_EXT */

	/*
	 * Setup the ALQ logging if required
	 */
#ifdef	ATH_DEBUG_ALQ
	if_ath_alq_init(&sc->sc_alq, device_get_nameunit(sc->sc_dev));
	if_ath_alq_setcfg(&sc->sc_alq,
	    sc->sc_ah->ah_macVersion,
	    sc->sc_ah->ah_macRev,
	    sc->sc_ah->ah_phyRev,
	    sc->sc_ah->ah_magic);
#endif

	/*
	 * Setup dynamic sysctl's now that country code and
	 * regdomain are available from the hal.
	 */
	ath_sysctlattach(sc);
	ath_sysctl_stats_attach(sc);
	ath_sysctl_hal_attach(sc);

	if (bootverbose)
		ieee80211_announce(ic);
	ath_announce(sc);
	return 0;
bad2:
	ath_tx_cleanup(sc);
	ath_desc_free(sc);
	ath_txdma_teardown(sc);
	ath_rxdma_teardown(sc);
bad:
	if (ah)
		ath_hal_detach(ah);

	/*
	 * To work around scoping issues with CURVNET_SET/CURVNET_RESTORE..
	 */
	if (ifp != NULL && ifp->if_vnet) {
		CURVNET_SET(ifp->if_vnet);
		if_free(ifp);
		CURVNET_RESTORE();
	} else if (ifp != NULL)
		if_free(ifp);
	sc->sc_invalid = 1;
	return error;
}

int
ath_detach(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags %x\n",
		__func__, ifp->if_flags);

	/*
	 * NB: the order of these is important:
	 * o stop the chip so no more interrupts will fire
	 * o call the 802.11 layer before detaching the hal to
	 *   insure callbacks into the driver to delete global
	 *   key cache entries can be handled
	 * o free the taskqueue which drains any pending tasks
	 * o reclaim the tx queue data structures after calling
	 *   the 802.11 layer as we'll get called back to reclaim
	 *   node state and potentially want to use them
	 * o to cleanup the tx queues the hal is called, so detach
	 *   it last
	 * Other than that, it's straightforward...
	 */
	ath_stop(ifp);
	ieee80211_ifdetach(ifp->if_l2com);
	taskqueue_free(sc->sc_tq);
#ifdef ATH_TX99_DIAG
	if (sc->sc_tx99 != NULL)
		sc->sc_tx99->detach(sc->sc_tx99);
#endif
	ath_rate_detach(sc->sc_rc);
#ifdef	ATH_DEBUG_ALQ
	if_ath_alq_tidyup(&sc->sc_alq);
#endif
	ath_spectral_detach(sc);
	ath_dfs_detach(sc);
	ath_desc_free(sc);
	ath_txdma_teardown(sc);
	ath_rxdma_teardown(sc);
	ath_tx_cleanup(sc);
	ath_hal_detach(sc->sc_ah);	/* NB: sets chip in full sleep */

	CURVNET_SET(ifp->if_vnet);
	if_free(ifp);
	CURVNET_RESTORE();

	return 0;
}

/*
 * MAC address handling for multiple BSS on the same radio.
 * The first vap uses the MAC address from the EEPROM.  For
 * subsequent vap's we set the U/L bit (bit 1) in the MAC
 * address and use the next six bits as an index.
 */
static void
assign_address(struct ath_softc *sc, uint8_t mac[IEEE80211_ADDR_LEN], int clone)
{
	int i;

	if (clone && sc->sc_hasbmask) {
		/* NB: we only do this if h/w supports multiple bssid */
		for (i = 0; i < 8; i++)
			if ((sc->sc_bssidmask & (1<<i)) == 0)
				break;
		if (i != 0)
			mac[0] |= (i << 2)|0x2;
	} else
		i = 0;
	sc->sc_bssidmask |= 1<<i;
	sc->sc_hwbssidmask[0] &= ~mac[0];
	if (i == 0)
		sc->sc_nbssid0++;
}

static void
reclaim_address(struct ath_softc *sc, const uint8_t mac[IEEE80211_ADDR_LEN])
{
	int i = mac[0] >> 2;
	uint8_t mask;

	if (i != 0 || --sc->sc_nbssid0 == 0) {
		sc->sc_bssidmask &= ~(1<<i);
		/* recalculate bssid mask from remaining addresses */
		mask = 0xff;
		for (i = 1; i < 8; i++)
			if (sc->sc_bssidmask & (1<<i))
				mask &= ~((i<<2)|0x2);
		sc->sc_hwbssidmask[0] |= mask;
	}
}

/*
 * Assign a beacon xmit slot.  We try to space out
 * assignments so when beacons are staggered the
 * traffic coming out of the cab q has maximal time
 * to go out before the next beacon is scheduled.
 */
static int
assign_bslot(struct ath_softc *sc)
{
	u_int slot, free;

	free = 0;
	for (slot = 0; slot < ATH_BCBUF; slot++)
		if (sc->sc_bslot[slot] == NULL) {
			if (sc->sc_bslot[(slot+1)%ATH_BCBUF] == NULL &&
			    sc->sc_bslot[(slot-1)%ATH_BCBUF] == NULL)
				return slot;
			free = slot;
			/* NB: keep looking for a double slot */
		}
	return free;
}

static struct ieee80211vap *
ath_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac0[IEEE80211_ADDR_LEN])
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_vap *avp;
	struct ieee80211vap *vap;
	uint8_t mac[IEEE80211_ADDR_LEN];
	int needbeacon, error;
	enum ieee80211_opmode ic_opmode;

	avp = (struct ath_vap *) malloc(sizeof(struct ath_vap),
	    M_80211_VAP, M_WAITOK | M_ZERO);
	needbeacon = 0;
	IEEE80211_ADDR_COPY(mac, mac0);

	ATH_LOCK(sc);
	ic_opmode = opmode;		/* default to opmode of new vap */
	switch (opmode) {
	case IEEE80211_M_STA:
		if (sc->sc_nstavaps != 0) {	/* XXX only 1 for now */
			device_printf(sc->sc_dev, "only 1 sta vap supported\n");
			goto bad;
		}
		if (sc->sc_nvaps) {
			/*
			 * With multiple vaps we must fall back
			 * to s/w beacon miss handling.
			 */
			flags |= IEEE80211_CLONE_NOBEACONS;
		}
		if (flags & IEEE80211_CLONE_NOBEACONS) {
			/*
			 * Station mode w/o beacons are implemented w/ AP mode.
			 */
			ic_opmode = IEEE80211_M_HOSTAP;
		}
		break;
	case IEEE80211_M_IBSS:
		if (sc->sc_nvaps != 0) {	/* XXX only 1 for now */
			device_printf(sc->sc_dev,
			    "only 1 ibss vap supported\n");
			goto bad;
		}
		needbeacon = 1;
		break;
	case IEEE80211_M_AHDEMO:
#ifdef IEEE80211_SUPPORT_TDMA
		if (flags & IEEE80211_CLONE_TDMA) {
			if (sc->sc_nvaps != 0) {
				device_printf(sc->sc_dev,
				    "only 1 tdma vap supported\n");
				goto bad;
			}
			needbeacon = 1;
			flags |= IEEE80211_CLONE_NOBEACONS;
		}
		/* fall thru... */
#endif
	case IEEE80211_M_MONITOR:
		if (sc->sc_nvaps != 0 && ic->ic_opmode != opmode) {
			/*
			 * Adopt existing mode.  Adding a monitor or ahdemo
			 * vap to an existing configuration is of dubious
			 * value but should be ok.
			 */
			/* XXX not right for monitor mode */
			ic_opmode = ic->ic_opmode;
		}
		break;
	case IEEE80211_M_HOSTAP:
	case IEEE80211_M_MBSS:
		needbeacon = 1;
		break;
	case IEEE80211_M_WDS:
		if (sc->sc_nvaps != 0 && ic->ic_opmode == IEEE80211_M_STA) {
			device_printf(sc->sc_dev,
			    "wds not supported in sta mode\n");
			goto bad;
		}
		/*
		 * Silently remove any request for a unique
		 * bssid; WDS vap's always share the local
		 * mac address.
		 */
		flags &= ~IEEE80211_CLONE_BSSID;
		if (sc->sc_nvaps == 0)
			ic_opmode = IEEE80211_M_HOSTAP;
		else
			ic_opmode = ic->ic_opmode;
		break;
	default:
		device_printf(sc->sc_dev, "unknown opmode %d\n", opmode);
		goto bad;
	}
	/*
	 * Check that a beacon buffer is available; the code below assumes it.
	 */
	if (needbeacon & TAILQ_EMPTY(&sc->sc_bbuf)) {
		device_printf(sc->sc_dev, "no beacon buffer available\n");
		goto bad;
	}

	/* STA, AHDEMO? */
	if (opmode == IEEE80211_M_HOSTAP || opmode == IEEE80211_M_MBSS) {
		assign_address(sc, mac, flags & IEEE80211_CLONE_BSSID);
		ath_hal_setbssidmask(sc->sc_ah, sc->sc_hwbssidmask);
	}

	vap = &avp->av_vap;
	/* XXX can't hold mutex across if_alloc */
	ATH_UNLOCK(sc);
	error = ieee80211_vap_setup(ic, vap, name, unit, opmode, flags,
	    bssid, mac);
	ATH_LOCK(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "%s: error %d creating vap\n",
		    __func__, error);
		goto bad2;
	}

	/* h/w crypto support */
	vap->iv_key_alloc = ath_key_alloc;
	vap->iv_key_delete = ath_key_delete;
	vap->iv_key_set = ath_key_set;
	vap->iv_key_update_begin = ath_key_update_begin;
	vap->iv_key_update_end = ath_key_update_end;

	/* override various methods */
	avp->av_recv_mgmt = vap->iv_recv_mgmt;
	vap->iv_recv_mgmt = ath_recv_mgmt;
	vap->iv_reset = ath_reset_vap;
	vap->iv_update_beacon = ath_beacon_update;
	avp->av_newstate = vap->iv_newstate;
	vap->iv_newstate = ath_newstate;
	avp->av_bmiss = vap->iv_bmiss;
	vap->iv_bmiss = ath_bmiss_vap;

	avp->av_node_ps = vap->iv_node_ps;
	vap->iv_node_ps = ath_node_powersave;

	avp->av_set_tim = vap->iv_set_tim;
	vap->iv_set_tim = ath_node_set_tim;

	/* Set default parameters */

	/*
	 * Anything earlier than some AR9300 series MACs don't
	 * support a smaller MPDU density.
	 */
	vap->iv_ampdu_density = IEEE80211_HTCAP_MPDUDENSITY_8;
	/*
	 * All NICs can handle the maximum size, however
	 * AR5416 based MACs can only TX aggregates w/ RTS
	 * protection when the total aggregate size is <= 8k.
	 * However, for now that's enforced by the TX path.
	 */
	vap->iv_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_64K;

	avp->av_bslot = -1;
	if (needbeacon) {
		/*
		 * Allocate beacon state and setup the q for buffered
		 * multicast frames.  We know a beacon buffer is
		 * available because we checked above.
		 */
		avp->av_bcbuf = TAILQ_FIRST(&sc->sc_bbuf);
		TAILQ_REMOVE(&sc->sc_bbuf, avp->av_bcbuf, bf_list);
		if (opmode != IEEE80211_M_IBSS || !sc->sc_hasveol) {
			/*
			 * Assign the vap to a beacon xmit slot.  As above
			 * this cannot fail to find a free one.
			 */
			avp->av_bslot = assign_bslot(sc);
			KASSERT(sc->sc_bslot[avp->av_bslot] == NULL,
			    ("beacon slot %u not empty", avp->av_bslot));
			sc->sc_bslot[avp->av_bslot] = vap;
			sc->sc_nbcnvaps++;
		}
		if (sc->sc_hastsfadd && sc->sc_nbcnvaps > 0) {
			/*
			 * Multple vaps are to transmit beacons and we
			 * have h/w support for TSF adjusting; enable
			 * use of staggered beacons.
			 */
			sc->sc_stagbeacons = 1;
		}
		ath_txq_init(sc, &avp->av_mcastq, ATH_TXQ_SWQ);
	}

	ic->ic_opmode = ic_opmode;
	if (opmode != IEEE80211_M_WDS) {
		sc->sc_nvaps++;
		if (opmode == IEEE80211_M_STA)
			sc->sc_nstavaps++;
		if (opmode == IEEE80211_M_MBSS)
			sc->sc_nmeshvaps++;
	}
	switch (ic_opmode) {
	case IEEE80211_M_IBSS:
		sc->sc_opmode = HAL_M_IBSS;
		break;
	case IEEE80211_M_STA:
		sc->sc_opmode = HAL_M_STA;
		break;
	case IEEE80211_M_AHDEMO:
#ifdef IEEE80211_SUPPORT_TDMA
		if (vap->iv_caps & IEEE80211_C_TDMA) {
			sc->sc_tdma = 1;
			/* NB: disable tsf adjust */
			sc->sc_stagbeacons = 0;
		}
		/*
		 * NB: adhoc demo mode is a pseudo mode; to the hal it's
		 * just ap mode.
		 */
		/* fall thru... */
#endif
	case IEEE80211_M_HOSTAP:
	case IEEE80211_M_MBSS:
		sc->sc_opmode = HAL_M_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		sc->sc_opmode = HAL_M_MONITOR;
		break;
	default:
		/* XXX should not happen */
		break;
	}
	if (sc->sc_hastsfadd) {
		/*
		 * Configure whether or not TSF adjust should be done.
		 */
		ath_hal_settsfadjust(sc->sc_ah, sc->sc_stagbeacons);
	}
	if (flags & IEEE80211_CLONE_NOBEACONS) {
		/*
		 * Enable s/w beacon miss handling.
		 */
		sc->sc_swbmiss = 1;
	}
	ATH_UNLOCK(sc);

	/* complete setup */
	ieee80211_vap_attach(vap, ath_media_change, ieee80211_media_status);
	return vap;
bad2:
	reclaim_address(sc, mac);
	ath_hal_setbssidmask(sc->sc_ah, sc->sc_hwbssidmask);
bad:
	free(avp, M_80211_VAP);
	ATH_UNLOCK(sc);
	return NULL;
}

static void
ath_vap_delete(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_vap *avp = ATH_VAP(vap);

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: called\n", __func__);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/*
		 * Quiesce the hardware while we remove the vap.  In
		 * particular we need to reclaim all references to
		 * the vap state by any frames pending on the tx queues.
		 */
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc, ATH_RESET_DEFAULT);		/* stop hw xmit side */
		/* XXX Do all frames from all vaps/nodes need draining here? */
		ath_stoprecv(sc, 1);		/* stop recv side */
	}

	ieee80211_vap_detach(vap);

	/*
	 * XXX Danger Will Robinson! Danger!
	 *
	 * Because ieee80211_vap_detach() can queue a frame (the station
	 * diassociate message?) after we've drained the TXQ and
	 * flushed the software TXQ, we will end up with a frame queued
	 * to a node whose vap is about to be freed.
	 *
	 * To work around this, flush the hardware/software again.
	 * This may be racy - the ath task may be running and the packet
	 * may be being scheduled between sw->hw txq. Tsk.
	 *
	 * TODO: figure out why a new node gets allocated somewhere around
	 * here (after the ath_tx_swq() call; and after an ath_stop_locked()
	 * call!)
	 */

	ath_draintxq(sc, ATH_RESET_DEFAULT);

	ATH_LOCK(sc);
	/*
	 * Reclaim beacon state.  Note this must be done before
	 * the vap instance is reclaimed as we may have a reference
	 * to it in the buffer for the beacon frame.
	 */
	if (avp->av_bcbuf != NULL) {
		if (avp->av_bslot != -1) {
			sc->sc_bslot[avp->av_bslot] = NULL;
			sc->sc_nbcnvaps--;
		}
		ath_beacon_return(sc, avp->av_bcbuf);
		avp->av_bcbuf = NULL;
		if (sc->sc_nbcnvaps == 0) {
			sc->sc_stagbeacons = 0;
			if (sc->sc_hastsfadd)
				ath_hal_settsfadjust(sc->sc_ah, 0);
		}
		/*
		 * Reclaim any pending mcast frames for the vap.
		 */
		ath_tx_draintxq(sc, &avp->av_mcastq);
	}
	/*
	 * Update bookkeeping.
	 */
	if (vap->iv_opmode == IEEE80211_M_STA) {
		sc->sc_nstavaps--;
		if (sc->sc_nstavaps == 0 && sc->sc_swbmiss)
			sc->sc_swbmiss = 0;
	} else if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
	    vap->iv_opmode == IEEE80211_M_MBSS) {
		reclaim_address(sc, vap->iv_myaddr);
		ath_hal_setbssidmask(ah, sc->sc_hwbssidmask);
		if (vap->iv_opmode == IEEE80211_M_MBSS)
			sc->sc_nmeshvaps--;
	}
	if (vap->iv_opmode != IEEE80211_M_WDS)
		sc->sc_nvaps--;
#ifdef IEEE80211_SUPPORT_TDMA
	/* TDMA operation ceases when the last vap is destroyed */
	if (sc->sc_tdma && sc->sc_nvaps == 0) {
		sc->sc_tdma = 0;
		sc->sc_swbmiss = 0;
	}
#endif
	free(avp, M_80211_VAP);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/*
		 * Restart rx+tx machines if still running (RUNNING will
		 * be reset if we just destroyed the last vap).
		 */
		if (ath_startrecv(sc) != 0)
			if_printf(ifp, "%s: unable to restart recv logic\n",
			    __func__);
		if (sc->sc_beacons) {		/* restart beacons */
#ifdef IEEE80211_SUPPORT_TDMA
			if (sc->sc_tdma)
				ath_tdma_config(sc, NULL);
			else
#endif
				ath_beacon_config(sc, NULL);
		}
		ath_hal_intrset(ah, sc->sc_imask);
	}
	ATH_UNLOCK(sc);
}

void
ath_suspend(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags %x\n",
		__func__, ifp->if_flags);

	sc->sc_resume_up = (ifp->if_flags & IFF_UP) != 0;

	ieee80211_suspend_all(ic);
	/*
	 * NB: don't worry about putting the chip in low power
	 * mode; pci will power off our socket on suspend and
	 * CardBus detaches the device.
	 */

	/*
	 * XXX ensure none of the taskqueues are running
	 * XXX ensure sc_invalid is 1
	 * XXX ensure the calibration callout is disabled
	 */

	/* Disable the PCIe PHY, complete with workarounds */
	ath_hal_enablepcie(sc->sc_ah, 1, 1);
}

/*
 * Reset the key cache since some parts do not reset the
 * contents on resume.  First we clear all entries, then
 * re-load keys that the 802.11 layer assumes are setup
 * in h/w.
 */
static void
ath_reset_keycache(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	int i;

	for (i = 0; i < sc->sc_keymax; i++)
		ath_hal_keyreset(ah, i);
	ieee80211_crypto_reload_keys(ic);
}

/*
 * Fetch the current chainmask configuration based on the current
 * operating channel and options.
 */
static void
ath_update_chainmasks(struct ath_softc *sc, struct ieee80211_channel *chan)
{

	/*
	 * Set TX chainmask to the currently configured chainmask;
	 * the TX chainmask depends upon the current operating mode.
	 */
	sc->sc_cur_rxchainmask = sc->sc_rxchainmask;
	if (IEEE80211_IS_CHAN_HT(chan)) {
		sc->sc_cur_txchainmask = sc->sc_txchainmask;
	} else {
		sc->sc_cur_txchainmask = 1;
	}

	DPRINTF(sc, ATH_DEBUG_RESET,
	    "%s: TX chainmask is now 0x%x, RX is now 0x%x\n",
	    __func__,
	    sc->sc_cur_txchainmask,
	    sc->sc_cur_rxchainmask);
}

void
ath_resume(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags %x\n",
		__func__, ifp->if_flags);

	/* Re-enable PCIe, re-enable the PCIe bus */
	ath_hal_enablepcie(ah, 0, 0);

	/*
	 * Must reset the chip before we reload the
	 * keycache as we were powered down on suspend.
	 */
	ath_update_chainmasks(sc,
	    sc->sc_curchan != NULL ? sc->sc_curchan : ic->ic_curchan);
	ath_hal_setchainmasks(sc->sc_ah, sc->sc_cur_txchainmask,
	    sc->sc_cur_rxchainmask);
	ath_hal_reset(ah, sc->sc_opmode,
	    sc->sc_curchan != NULL ? sc->sc_curchan : ic->ic_curchan,
	    AH_FALSE, &status);
	ath_reset_keycache(sc);

	/* Let DFS at it in case it's a DFS channel */
	ath_dfs_radar_enable(sc, ic->ic_curchan);

	/* Let spectral at in case spectral is enabled */
	ath_spectral_enable(sc, ic->ic_curchan);

	/* Restore the LED configuration */
	ath_led_config(sc);
	ath_hal_setledstate(ah, HAL_LED_INIT);

	if (sc->sc_resume_up)
		ieee80211_resume_all(ic);

	/* XXX beacons ? */
}

void
ath_shutdown(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags %x\n",
		__func__, ifp->if_flags);

	ath_stop(ifp);
	/* NB: no point powering down chip as we're about to reboot */
}

/*
 * Interrupt handler.  Most of the actual processing is deferred.
 */
void
ath_intr(void *arg)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ath_hal *ah = sc->sc_ah;
	HAL_INT status = 0;
	uint32_t txqs;

	/*
	 * If we're inside a reset path, just print a warning and
	 * clear the ISR. The reset routine will finish it for us.
	 */
	ATH_PCU_LOCK(sc);
	if (sc->sc_inreset_cnt) {
		HAL_INT status;
		ath_hal_getisr(ah, &status);	/* clear ISR */
		ath_hal_intrset(ah, 0);		/* disable further intr's */
		DPRINTF(sc, ATH_DEBUG_ANY,
		    "%s: in reset, ignoring: status=0x%x\n",
		    __func__, status);
		ATH_PCU_UNLOCK(sc);
		return;
	}

	if (sc->sc_invalid) {
		/*
		 * The hardware is not ready/present, don't touch anything.
		 * Note this can happen early on if the IRQ is shared.
		 */
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid; ignored\n", __func__);
		ATH_PCU_UNLOCK(sc);
		return;
	}
	if (!ath_hal_intrpend(ah)) {		/* shared irq, not for us */
		ATH_PCU_UNLOCK(sc);
		return;
	}

	if ((ifp->if_flags & IFF_UP) == 0 ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		HAL_INT status;

		DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags 0x%x\n",
			__func__, ifp->if_flags);
		ath_hal_getisr(ah, &status);	/* clear ISR */
		ath_hal_intrset(ah, 0);		/* disable further intr's */
		ATH_PCU_UNLOCK(sc);
		return;
	}

	/*
	 * Figure out the reason(s) for the interrupt.  Note
	 * that the hal returns a pseudo-ISR that may include
	 * bits we haven't explicitly enabled so we mask the
	 * value to insure we only process bits we requested.
	 */
	ath_hal_getisr(ah, &status);		/* NB: clears ISR too */
	DPRINTF(sc, ATH_DEBUG_INTR, "%s: status 0x%x\n", __func__, status);
	ATH_KTR(sc, ATH_KTR_INTERRUPTS, 1, "ath_intr: mask=0x%.8x", status);
#ifdef	ATH_DEBUG_ALQ
	if_ath_alq_post_intr(&sc->sc_alq, status, ah->ah_intrstate,
	    ah->ah_syncstate);
#endif	/* ATH_DEBUG_ALQ */
#ifdef	ATH_KTR_INTR_DEBUG
	ATH_KTR(sc, ATH_KTR_INTERRUPTS, 5,
	    "ath_intr: ISR=0x%.8x, ISR_S0=0x%.8x, ISR_S1=0x%.8x, ISR_S2=0x%.8x, ISR_S5=0x%.8x",
	    ah->ah_intrstate[0],
	    ah->ah_intrstate[1],
	    ah->ah_intrstate[2],
	    ah->ah_intrstate[3],
	    ah->ah_intrstate[6]);
#endif

	/* Squirrel away SYNC interrupt debugging */
	if (ah->ah_syncstate != 0) {
		int i;
		for (i = 0; i < 32; i++)
			if (ah->ah_syncstate & (i << i))
				sc->sc_intr_stats.sync_intr[i]++;
	}

	status &= sc->sc_imask;			/* discard unasked for bits */

	/* Short-circuit un-handled interrupts */
	if (status == 0x0) {
		ATH_PCU_UNLOCK(sc);
		return;
	}

	/*
	 * Take a note that we're inside the interrupt handler, so
	 * the reset routines know to wait.
	 */
	sc->sc_intr_cnt++;
	ATH_PCU_UNLOCK(sc);

	/*
	 * Handle the interrupt. We won't run concurrent with the reset
	 * or channel change routines as they'll wait for sc_intr_cnt
	 * to be 0 before continuing.
	 */
	if (status & HAL_INT_FATAL) {
		sc->sc_stats.ast_hardware++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		taskqueue_enqueue(sc->sc_tq, &sc->sc_fataltask);
	} else {
		if (status & HAL_INT_SWBA) {
			/*
			 * Software beacon alert--time to send a beacon.
			 * Handle beacon transmission directly; deferring
			 * this is too slow to meet timing constraints
			 * under load.
			 */
#ifdef IEEE80211_SUPPORT_TDMA
			if (sc->sc_tdma) {
				if (sc->sc_tdmaswba == 0) {
					struct ieee80211com *ic = ifp->if_l2com;
					struct ieee80211vap *vap =
					    TAILQ_FIRST(&ic->ic_vaps);
					ath_tdma_beacon_send(sc, vap);
					sc->sc_tdmaswba =
					    vap->iv_tdma->tdma_bintval;
				} else
					sc->sc_tdmaswba--;
			} else
#endif
			{
				ath_beacon_proc(sc, 0);
#ifdef IEEE80211_SUPPORT_SUPERG
				/*
				 * Schedule the rx taskq in case there's no
				 * traffic so any frames held on the staging
				 * queue are aged and potentially flushed.
				 */
				sc->sc_rx.recv_sched(sc, 1);
#endif
			}
		}
		if (status & HAL_INT_RXEOL) {
			int imask;
			ATH_KTR(sc, ATH_KTR_ERROR, 0, "ath_intr: RXEOL");
			ATH_PCU_LOCK(sc);
			/*
			 * NB: the hardware should re-read the link when
			 *     RXE bit is written, but it doesn't work at
			 *     least on older hardware revs.
			 */
			sc->sc_stats.ast_rxeol++;
			/*
			 * Disable RXEOL/RXORN - prevent an interrupt
			 * storm until the PCU logic can be reset.
			 * In case the interface is reset some other
			 * way before "sc_kickpcu" is called, don't
			 * modify sc_imask - that way if it is reset
			 * by a call to ath_reset() somehow, the
			 * interrupt mask will be correctly reprogrammed.
			 */
			imask = sc->sc_imask;
			imask &= ~(HAL_INT_RXEOL | HAL_INT_RXORN);
			ath_hal_intrset(ah, imask);
			/*
			 * Only blank sc_rxlink if we've not yet kicked
			 * the PCU.
			 *
			 * This isn't entirely correct - the correct solution
			 * would be to have a PCU lock and engage that for
			 * the duration of the PCU fiddling; which would include
			 * running the RX process. Otherwise we could end up
			 * messing up the RX descriptor chain and making the
			 * RX desc list much shorter.
			 */
			if (! sc->sc_kickpcu)
				sc->sc_rxlink = NULL;
			sc->sc_kickpcu = 1;
			ATH_PCU_UNLOCK(sc);
			/*
			 * Enqueue an RX proc, to handled whatever
			 * is in the RX queue.
			 * This will then kick the PCU.
			 */
			sc->sc_rx.recv_sched(sc, 1);
		}
		if (status & HAL_INT_TXURN) {
			sc->sc_stats.ast_txurn++;
			/* bump tx trigger level */
			ath_hal_updatetxtriglevel(ah, AH_TRUE);
		}
		/*
		 * Handle both the legacy and RX EDMA interrupt bits.
		 * Note that HAL_INT_RXLP is also HAL_INT_RXDESC.
		 */
		if (status & (HAL_INT_RX | HAL_INT_RXHP | HAL_INT_RXLP)) {
			sc->sc_stats.ast_rx_intr++;
			sc->sc_rx.recv_sched(sc, 1);
		}
		if (status & HAL_INT_TX) {
			sc->sc_stats.ast_tx_intr++;
			/*
			 * Grab all the currently set bits in the HAL txq bitmap
			 * and blank them. This is the only place we should be
			 * doing this.
			 */
			if (! sc->sc_isedma) {
				ATH_PCU_LOCK(sc);
				txqs = 0xffffffff;
				ath_hal_gettxintrtxqs(sc->sc_ah, &txqs);
				ATH_KTR(sc, ATH_KTR_INTERRUPTS, 3,
				    "ath_intr: TX; txqs=0x%08x, txq_active was 0x%08x, now 0x%08x",
				    txqs,
				    sc->sc_txq_active,
				    sc->sc_txq_active | txqs);
				sc->sc_txq_active |= txqs;
				ATH_PCU_UNLOCK(sc);
			}
			taskqueue_enqueue(sc->sc_tq, &sc->sc_txtask);
		}
		if (status & HAL_INT_BMISS) {
			sc->sc_stats.ast_bmiss++;
			taskqueue_enqueue(sc->sc_tq, &sc->sc_bmisstask);
		}
		if (status & HAL_INT_GTT)
			sc->sc_stats.ast_tx_timeout++;
		if (status & HAL_INT_CST)
			sc->sc_stats.ast_tx_cst++;
		if (status & HAL_INT_MIB) {
			sc->sc_stats.ast_mib++;
			ATH_PCU_LOCK(sc);
			/*
			 * Disable interrupts until we service the MIB
			 * interrupt; otherwise it will continue to fire.
			 */
			ath_hal_intrset(ah, 0);
			/*
			 * Let the hal handle the event.  We assume it will
			 * clear whatever condition caused the interrupt.
			 */
			ath_hal_mibevent(ah, &sc->sc_halstats);
			/*
			 * Don't reset the interrupt if we've just
			 * kicked the PCU, or we may get a nested
			 * RXEOL before the rxproc has had a chance
			 * to run.
			 */
			if (sc->sc_kickpcu == 0)
				ath_hal_intrset(ah, sc->sc_imask);
			ATH_PCU_UNLOCK(sc);
		}
		if (status & HAL_INT_RXORN) {
			/* NB: hal marks HAL_INT_FATAL when RXORN is fatal */
			ATH_KTR(sc, ATH_KTR_ERROR, 0, "ath_intr: RXORN");
			sc->sc_stats.ast_rxorn++;
		}
	}
	ATH_PCU_LOCK(sc);
	sc->sc_intr_cnt--;
	ATH_PCU_UNLOCK(sc);
}

static void
ath_fatal_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	u_int32_t *state;
	u_int32_t len;
	void *sp;

	if_printf(ifp, "hardware error; resetting\n");
	/*
	 * Fatal errors are unrecoverable.  Typically these
	 * are caused by DMA errors.  Collect h/w state from
	 * the hal so we can diagnose what's going on.
	 */
	if (ath_hal_getfatalstate(sc->sc_ah, &sp, &len)) {
		KASSERT(len >= 6*sizeof(u_int32_t), ("len %u bytes", len));
		state = sp;
		if_printf(ifp, "0x%08x 0x%08x 0x%08x, 0x%08x 0x%08x 0x%08x\n",
		    state[0], state[1] , state[2], state[3],
		    state[4], state[5]);
	}
	ath_reset(ifp, ATH_RESET_NOLOSS);
}

static void
ath_bmiss_vap(struct ieee80211vap *vap)
{
	/*
	 * Workaround phantom bmiss interrupts by sanity-checking
	 * the time of our last rx'd frame.  If it is within the
	 * beacon miss interval then ignore the interrupt.  If it's
	 * truly a bmiss we'll get another interrupt soon and that'll
	 * be dispatched up for processing.  Note this applies only
	 * for h/w beacon miss events.
	 */
	if ((vap->iv_flags_ext & IEEE80211_FEXT_SWBMISS) == 0) {
		struct ifnet *ifp = vap->iv_ic->ic_ifp;
		struct ath_softc *sc = ifp->if_softc;
		u_int64_t lastrx = sc->sc_lastrx;
		u_int64_t tsf = ath_hal_gettsf64(sc->sc_ah);
		/* XXX should take a locked ref to iv_bss */
		u_int bmisstimeout =
			vap->iv_bmissthreshold * vap->iv_bss->ni_intval * 1024;

		DPRINTF(sc, ATH_DEBUG_BEACON,
		    "%s: tsf %llu lastrx %lld (%llu) bmiss %u\n",
		    __func__, (unsigned long long) tsf,
		    (unsigned long long)(tsf - lastrx),
		    (unsigned long long) lastrx, bmisstimeout);

		if (tsf - lastrx <= bmisstimeout) {
			sc->sc_stats.ast_bmiss_phantom++;
			return;
		}
	}
	ATH_VAP(vap)->av_bmiss(vap);
}

int
ath_hal_gethangstate(struct ath_hal *ah, uint32_t mask, uint32_t *hangs)
{
	uint32_t rsize;
	void *sp;

	if (!ath_hal_getdiagstate(ah, HAL_DIAG_CHECK_HANGS, &mask, sizeof(mask), &sp, &rsize))
		return 0;
	KASSERT(rsize == sizeof(uint32_t), ("resultsize %u", rsize));
	*hangs = *(uint32_t *)sp;
	return 1;
}

static void
ath_bmiss_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t hangs;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: pending %u\n", __func__, pending);

	/*
	 * Do a reset upon any becaon miss event.
	 *
	 * It may be a non-recognised RX clear hang which needs a reset
	 * to clear.
	 */
	if (ath_hal_gethangstate(sc->sc_ah, 0xff, &hangs) && hangs != 0) {
		ath_reset(ifp, ATH_RESET_NOLOSS);
		if_printf(ifp, "bb hang detected (0x%x), resetting\n", hangs);
	} else {
		ath_reset(ifp, ATH_RESET_NOLOSS);
		ieee80211_beacon_miss(ifp->if_l2com);
	}
}

/*
 * Handle TKIP MIC setup to deal hardware that doesn't do MIC
 * calcs together with WME.  If necessary disable the crypto
 * hardware and mark the 802.11 state so keys will be setup
 * with the MIC work done in software.
 */
static void
ath_settkipmic(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	if ((ic->ic_cryptocaps & IEEE80211_CRYPTO_TKIP) && !sc->sc_wmetkipmic) {
		if (ic->ic_flags & IEEE80211_F_WME) {
			ath_hal_settkipmic(sc->sc_ah, AH_FALSE);
			ic->ic_cryptocaps &= ~IEEE80211_CRYPTO_TKIPMIC;
		} else {
			ath_hal_settkipmic(sc->sc_ah, AH_TRUE);
			ic->ic_cryptocaps |= IEEE80211_CRYPTO_TKIPMIC;
		}
	}
}

static void
ath_init(void *arg)
{
	struct ath_softc *sc = (struct ath_softc *) arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags 0x%x\n",
		__func__, ifp->if_flags);

	ATH_LOCK(sc);
	/*
	 * Stop anything previously setup.  This is safe
	 * whether this is the first time through or not.
	 */
	ath_stop_locked(ifp);

	/*
	 * The basic interface to setting the hardware in a good
	 * state is ``reset''.  On return the hardware is known to
	 * be powered up and with interrupts disabled.  This must
	 * be followed by initialization of the appropriate bits
	 * and then setup of the interrupt mask.
	 */
	ath_settkipmic(sc);
	ath_update_chainmasks(sc, ic->ic_curchan);
	ath_hal_setchainmasks(sc->sc_ah, sc->sc_cur_txchainmask,
	    sc->sc_cur_rxchainmask);
	if (!ath_hal_reset(ah, sc->sc_opmode, ic->ic_curchan, AH_FALSE, &status)) {
		if_printf(ifp, "unable to reset hardware; hal status %u\n",
			status);
		ATH_UNLOCK(sc);
		return;
	}
	ath_chan_change(sc, ic->ic_curchan);

	/* Let DFS at it in case it's a DFS channel */
	ath_dfs_radar_enable(sc, ic->ic_curchan);

	/* Let spectral at in case spectral is enabled */
	ath_spectral_enable(sc, ic->ic_curchan);

	/*
	 * Likewise this is set during reset so update
	 * state cached in the driver.
	 */
	sc->sc_diversity = ath_hal_getdiversity(ah);
	sc->sc_lastlongcal = 0;
	sc->sc_resetcal = 1;
	sc->sc_lastcalreset = 0;
	sc->sc_lastani = 0;
	sc->sc_lastshortcal = 0;
	sc->sc_doresetcal = AH_FALSE;
	/*
	 * Beacon timers were cleared here; give ath_newstate()
	 * a hint that the beacon timers should be poked when
	 * things transition to the RUN state.
	 */
	sc->sc_beacons = 0;

	/*
	 * Setup the hardware after reset: the key cache
	 * is filled as needed and the receive engine is
	 * set going.  Frame transmit is handled entirely
	 * in the frame output path; there's nothing to do
	 * here except setup the interrupt mask.
	 */
	if (ath_startrecv(sc) != 0) {
		if_printf(ifp, "unable to start recv logic\n");
		ATH_UNLOCK(sc);
		return;
	}

	/*
	 * Enable interrupts.
	 */
	sc->sc_imask = HAL_INT_RX | HAL_INT_TX
		  | HAL_INT_RXEOL | HAL_INT_RXORN
		  | HAL_INT_TXURN
		  | HAL_INT_FATAL | HAL_INT_GLOBAL;

	/*
	 * Enable RX EDMA bits.  Note these overlap with
	 * HAL_INT_RX and HAL_INT_RXDESC respectively.
	 */
	if (sc->sc_isedma)
		sc->sc_imask |= (HAL_INT_RXHP | HAL_INT_RXLP);

	/*
	 * Enable MIB interrupts when there are hardware phy counters.
	 * Note we only do this (at the moment) for station mode.
	 */
	if (sc->sc_needmib && ic->ic_opmode == IEEE80211_M_STA)
		sc->sc_imask |= HAL_INT_MIB;

	/* Enable global TX timeout and carrier sense timeout if available */
	if (ath_hal_gtxto_supported(ah))
		sc->sc_imask |= HAL_INT_GTT;

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: imask=0x%x\n",
		__func__, sc->sc_imask);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	callout_reset(&sc->sc_wd_ch, hz, ath_watchdog, sc);
	ath_hal_intrset(ah, sc->sc_imask);

	ATH_UNLOCK(sc);

#ifdef ATH_TX99_DIAG
	if (sc->sc_tx99 != NULL)
		sc->sc_tx99->start(sc->sc_tx99);
	else
#endif
	ieee80211_start_all(ic);		/* start all vap's */
}

static void
ath_stop_locked(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid %u if_flags 0x%x\n",
		__func__, sc->sc_invalid, ifp->if_flags);

	ATH_LOCK_ASSERT(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/*
		 * Shutdown the hardware and driver:
		 *    reset 802.11 state machine
		 *    turn off timers
		 *    disable interrupts
		 *    turn off the radio
		 *    clear transmit machinery
		 *    clear receive machinery
		 *    drain and release tx queues
		 *    reclaim beacon resources
		 *    power down hardware
		 *
		 * Note that some of this work is not possible if the
		 * hardware is gone (invalid).
		 */
#ifdef ATH_TX99_DIAG
		if (sc->sc_tx99 != NULL)
			sc->sc_tx99->stop(sc->sc_tx99);
#endif
		callout_stop(&sc->sc_wd_ch);
		sc->sc_wd_timer = 0;
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		if (!sc->sc_invalid) {
			if (sc->sc_softled) {
				callout_stop(&sc->sc_ledtimer);
				ath_hal_gpioset(ah, sc->sc_ledpin,
					!sc->sc_ledon);
				sc->sc_blinking = 0;
			}
			ath_hal_intrset(ah, 0);
		}
		ath_draintxq(sc, ATH_RESET_DEFAULT);
		if (!sc->sc_invalid) {
			ath_stoprecv(sc, 1);
			ath_hal_phydisable(ah);
		} else
			sc->sc_rxlink = NULL;
		ath_beacon_free(sc);	/* XXX not needed */
	}
}

#define	MAX_TXRX_ITERATIONS	1000
static void
ath_txrx_stop_locked(struct ath_softc *sc)
{
	int i = MAX_TXRX_ITERATIONS;

	ATH_UNLOCK_ASSERT(sc);
	ATH_PCU_LOCK_ASSERT(sc);

	/*
	 * Sleep until all the pending operations have completed.
	 *
	 * The caller must ensure that reset has been incremented
	 * or the pending operations may continue being queued.
	 */
	while (sc->sc_rxproc_cnt || sc->sc_txproc_cnt ||
	    sc->sc_txstart_cnt || sc->sc_intr_cnt) {
		if (i <= 0)
			break;
		msleep(sc, &sc->sc_pcu_mtx, 0, "ath_txrx_stop", 1);
		i--;
	}

	if (i <= 0)
		device_printf(sc->sc_dev,
		    "%s: didn't finish after %d iterations\n",
		    __func__, MAX_TXRX_ITERATIONS);
}
#undef	MAX_TXRX_ITERATIONS

#if 0
static void
ath_txrx_stop(struct ath_softc *sc)
{
	ATH_UNLOCK_ASSERT(sc);
	ATH_PCU_UNLOCK_ASSERT(sc);

	ATH_PCU_LOCK(sc);
	ath_txrx_stop_locked(sc);
	ATH_PCU_UNLOCK(sc);
}
#endif

static void
ath_txrx_start(struct ath_softc *sc)
{

	taskqueue_unblock(sc->sc_tq);
}

/*
 * Grab the reset lock, and wait around until noone else
 * is trying to do anything with it.
 *
 * This is totally horrible but we can't hold this lock for
 * long enough to do TX/RX or we end up with net80211/ip stack
 * LORs and eventual deadlock.
 *
 * "dowait" signals whether to spin, waiting for the reset
 * lock count to reach 0. This should (for now) only be used
 * during the reset path, as the rest of the code may not
 * be locking-reentrant enough to behave correctly.
 *
 * Another, cleaner way should be found to serialise all of
 * these operations.
 */
#define	MAX_RESET_ITERATIONS	10
static int
ath_reset_grablock(struct ath_softc *sc, int dowait)
{
	int w = 0;
	int i = MAX_RESET_ITERATIONS;

	ATH_PCU_LOCK_ASSERT(sc);
	do {
		if (sc->sc_inreset_cnt == 0) {
			w = 1;
			break;
		}
		if (dowait == 0) {
			w = 0;
			break;
		}
		ATH_PCU_UNLOCK(sc);
		pause("ath_reset_grablock", 1);
		i--;
		ATH_PCU_LOCK(sc);
	} while (i > 0);

	/*
	 * We always increment the refcounter, regardless
	 * of whether we succeeded to get it in an exclusive
	 * way.
	 */
	sc->sc_inreset_cnt++;

	if (i <= 0)
		device_printf(sc->sc_dev,
		    "%s: didn't finish after %d iterations\n",
		    __func__, MAX_RESET_ITERATIONS);

	if (w == 0)
		device_printf(sc->sc_dev,
		    "%s: warning, recursive reset path!\n",
		    __func__);

	return w;
}
#undef MAX_RESET_ITERATIONS

/*
 * XXX TODO: write ath_reset_releaselock
 */

static void
ath_stop(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;

	ATH_LOCK(sc);
	ath_stop_locked(ifp);
	ATH_UNLOCK(sc);
}

/*
 * Reset the hardware w/o losing operational state.  This is
 * basically a more efficient way of doing ath_stop, ath_init,
 * followed by state transitions to the current 802.11
 * operational state.  Used to recover from various errors and
 * to reset or reload hardware state.
 */
int
ath_reset(struct ifnet *ifp, ATH_RESET_TYPE reset_type)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;
	int i;

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: called\n", __func__);

	/* Ensure ATH_LOCK isn't held; ath_rx_proc can't be locked */
	ATH_PCU_UNLOCK_ASSERT(sc);
	ATH_UNLOCK_ASSERT(sc);

	/* Try to (stop any further TX/RX from occuring */
	taskqueue_block(sc->sc_tq);

	ATH_PCU_LOCK(sc);
	ath_hal_intrset(ah, 0);		/* disable interrupts */
	ath_txrx_stop_locked(sc);	/* Ensure TX/RX is stopped */
	if (ath_reset_grablock(sc, 1) == 0) {
		device_printf(sc->sc_dev, "%s: concurrent reset! Danger!\n",
		    __func__);
	}
	ATH_PCU_UNLOCK(sc);

	/*
	 * Should now wait for pending TX/RX to complete
	 * and block future ones from occuring. This needs to be
	 * done before the TX queue is drained.
	 */
	ath_draintxq(sc, reset_type);	/* stop xmit side */

	/*
	 * Regardless of whether we're doing a no-loss flush or
	 * not, stop the PCU and handle what's in the RX queue.
	 * That way frames aren't dropped which shouldn't be.
	 */
	ath_stoprecv(sc, (reset_type != ATH_RESET_NOLOSS));
	ath_rx_flush(sc);

	ath_settkipmic(sc);		/* configure TKIP MIC handling */
	/* NB: indicate channel change so we do a full reset */
	ath_update_chainmasks(sc, ic->ic_curchan);
	ath_hal_setchainmasks(sc->sc_ah, sc->sc_cur_txchainmask,
	    sc->sc_cur_rxchainmask);
	if (!ath_hal_reset(ah, sc->sc_opmode, ic->ic_curchan, AH_TRUE, &status))
		if_printf(ifp, "%s: unable to reset hardware; hal status %u\n",
			__func__, status);
	sc->sc_diversity = ath_hal_getdiversity(ah);

	/* Let DFS at it in case it's a DFS channel */
	ath_dfs_radar_enable(sc, ic->ic_curchan);

	/* Let spectral at in case spectral is enabled */
	ath_spectral_enable(sc, ic->ic_curchan);

	if (ath_startrecv(sc) != 0)	/* restart recv */
		if_printf(ifp, "%s: unable to start recv logic\n", __func__);
	/*
	 * We may be doing a reset in response to an ioctl
	 * that changes the channel so update any state that
	 * might change as a result.
	 */
	ath_chan_change(sc, ic->ic_curchan);
	if (sc->sc_beacons) {		/* restart beacons */
#ifdef IEEE80211_SUPPORT_TDMA
		if (sc->sc_tdma)
			ath_tdma_config(sc, NULL);
		else
#endif
			ath_beacon_config(sc, NULL);
	}

	/*
	 * Release the reset lock and re-enable interrupts here.
	 * If an interrupt was being processed in ath_intr(),
	 * it would disable interrupts at this point. So we have
	 * to atomically enable interrupts and decrement the
	 * reset counter - this way ath_intr() doesn't end up
	 * disabling interrupts without a corresponding enable
	 * in the rest or channel change path.
	 */
	ATH_PCU_LOCK(sc);
	sc->sc_inreset_cnt--;
	/* XXX only do this if sc_inreset_cnt == 0? */
	ath_hal_intrset(ah, sc->sc_imask);
	ATH_PCU_UNLOCK(sc);

	/*
	 * TX and RX can be started here. If it were started with
	 * sc_inreset_cnt > 0, the TX and RX path would abort.
	 * Thus if this is a nested call through the reset or
	 * channel change code, TX completion will occur but
	 * RX completion and ath_start / ath_tx_start will not
	 * run.
	 */

	/* Restart TX/RX as needed */
	ath_txrx_start(sc);

	/* Restart TX completion and pending TX */
	if (reset_type == ATH_RESET_NOLOSS) {
		for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
			if (ATH_TXQ_SETUP(sc, i)) {
				ATH_TXQ_LOCK(&sc->sc_txq[i]);
				ath_txq_restart_dma(sc, &sc->sc_txq[i]);
				ATH_TXQ_UNLOCK(&sc->sc_txq[i]);

				ATH_TX_LOCK(sc);
				ath_txq_sched(sc, &sc->sc_txq[i]);
				ATH_TX_UNLOCK(sc);
			}
		}
	}

	/*
	 * This may have been set during an ath_start() call which
	 * set this once it detected a concurrent TX was going on.
	 * So, clear it.
	 */
	IF_LOCK(&ifp->if_snd);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	IF_UNLOCK(&ifp->if_snd);

	/* Handle any frames in the TX queue */
	/*
	 * XXX should this be done by the caller, rather than
	 * ath_reset() ?
	 */
	ath_tx_kick(sc);		/* restart xmit */
	return 0;
}

static int
ath_reset_vap(struct ieee80211vap *vap, u_long cmd)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;

	switch (cmd) {
	case IEEE80211_IOC_TXPOWER:
		/*
		 * If per-packet TPC is enabled, then we have nothing
		 * to do; otherwise we need to force the global limit.
		 * All this can happen directly; no need to reset.
		 */
		if (!ath_hal_gettpc(ah))
			ath_hal_settxpowlimit(ah, ic->ic_txpowlimit);
		return 0;
	}
	/* XXX? Full or NOLOSS? */
	return ath_reset(ifp, ATH_RESET_FULL);
}

struct ath_buf *
_ath_getbuf_locked(struct ath_softc *sc, ath_buf_type_t btype)
{
	struct ath_buf *bf;

	ATH_TXBUF_LOCK_ASSERT(sc);

	if (btype == ATH_BUFTYPE_MGMT)
		bf = TAILQ_FIRST(&sc->sc_txbuf_mgmt);
	else
		bf = TAILQ_FIRST(&sc->sc_txbuf);

	if (bf == NULL) {
		sc->sc_stats.ast_tx_getnobuf++;
	} else {
		if (bf->bf_flags & ATH_BUF_BUSY) {
			sc->sc_stats.ast_tx_getbusybuf++;
			bf = NULL;
		}
	}

	if (bf != NULL && (bf->bf_flags & ATH_BUF_BUSY) == 0) {
		if (btype == ATH_BUFTYPE_MGMT)
			TAILQ_REMOVE(&sc->sc_txbuf_mgmt, bf, bf_list);
		else {
			TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);
			sc->sc_txbuf_cnt--;

			/*
			 * This shuldn't happen; however just to be
			 * safe print a warning and fudge the txbuf
			 * count.
			 */
			if (sc->sc_txbuf_cnt < 0) {
				device_printf(sc->sc_dev,
				    "%s: sc_txbuf_cnt < 0?\n",
				    __func__);
				sc->sc_txbuf_cnt = 0;
			}
		}
	} else
		bf = NULL;

	if (bf == NULL) {
		/* XXX should check which list, mgmt or otherwise */
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: %s\n", __func__,
		    TAILQ_FIRST(&sc->sc_txbuf) == NULL ?
			"out of xmit buffers" : "xmit buffer busy");
		return NULL;
	}

	/* XXX TODO: should do this at buffer list initialisation */
	/* XXX (then, ensure the buffer has the right flag set) */
	bf->bf_flags = 0;
	if (btype == ATH_BUFTYPE_MGMT)
		bf->bf_flags |= ATH_BUF_MGMT;
	else
		bf->bf_flags &= (~ATH_BUF_MGMT);

	/* Valid bf here; clear some basic fields */
	bf->bf_next = NULL;	/* XXX just to be sure */
	bf->bf_last = NULL;	/* XXX again, just to be sure */
	bf->bf_comp = NULL;	/* XXX again, just to be sure */
	bzero(&bf->bf_state, sizeof(bf->bf_state));

	/*
	 * Track the descriptor ID only if doing EDMA
	 */
	if (sc->sc_isedma) {
		bf->bf_descid = sc->sc_txbuf_descid;
		sc->sc_txbuf_descid++;
	}

	return bf;
}

/*
 * When retrying a software frame, buffers marked ATH_BUF_BUSY
 * can't be thrown back on the queue as they could still be
 * in use by the hardware.
 *
 * This duplicates the buffer, or returns NULL.
 *
 * The descriptor is also copied but the link pointers and
 * the DMA segments aren't copied; this frame should thus
 * be again passed through the descriptor setup/chain routines
 * so the link is correct.
 *
 * The caller must free the buffer using ath_freebuf().
 */
struct ath_buf *
ath_buf_clone(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_buf *tbf;

	tbf = ath_getbuf(sc,
	    (bf->bf_flags & ATH_BUF_MGMT) ?
	     ATH_BUFTYPE_MGMT : ATH_BUFTYPE_NORMAL);
	if (tbf == NULL)
		return NULL;	/* XXX failure? Why? */

	/* Copy basics */
	tbf->bf_next = NULL;
	tbf->bf_nseg = bf->bf_nseg;
	tbf->bf_flags = bf->bf_flags & ATH_BUF_FLAGS_CLONE;
	tbf->bf_status = bf->bf_status;
	tbf->bf_m = bf->bf_m;
	tbf->bf_node = bf->bf_node;
	/* will be setup by the chain/setup function */
	tbf->bf_lastds = NULL;
	/* for now, last == self */
	tbf->bf_last = tbf;
	tbf->bf_comp = bf->bf_comp;

	/* NOTE: DMA segments will be setup by the setup/chain functions */

	/* The caller has to re-init the descriptor + links */

	/*
	 * Free the DMA mapping here, before we NULL the mbuf.
	 * We must only call bus_dmamap_unload() once per mbuf chain
	 * or behaviour is undefined.
	 */
	if (bf->bf_m != NULL) {
		/*
		 * XXX is this POSTWRITE call required?
		 */
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
	}

	bf->bf_m = NULL;
	bf->bf_node = NULL;

	/* Copy state */
	memcpy(&tbf->bf_state, &bf->bf_state, sizeof(bf->bf_state));

	return tbf;
}

struct ath_buf *
ath_getbuf(struct ath_softc *sc, ath_buf_type_t btype)
{
	struct ath_buf *bf;

	ATH_TXBUF_LOCK(sc);
	bf = _ath_getbuf_locked(sc, btype);
	/*
	 * If a mgmt buffer was requested but we're out of those,
	 * try requesting a normal one.
	 */
	if (bf == NULL && btype == ATH_BUFTYPE_MGMT)
		bf = _ath_getbuf_locked(sc, ATH_BUFTYPE_NORMAL);
	ATH_TXBUF_UNLOCK(sc);
	if (bf == NULL) {
		struct ifnet *ifp = sc->sc_ifp;

		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: stop queue\n", __func__);
		sc->sc_stats.ast_tx_qstop++;
		IF_LOCK(&ifp->if_snd);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		IF_UNLOCK(&ifp->if_snd);
	}
	return bf;
}

static void
ath_start_queue(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;

	ATH_KTR(sc, ATH_KTR_TX, 0, "ath_start_queue: start");
	ath_tx_kick(sc);
	ATH_KTR(sc, ATH_KTR_TX, 0, "ath_start_queue: finished");
}

void
ath_start_task(void *arg, int npending)
{
	struct ath_softc *sc = (struct ath_softc *) arg;
	struct ifnet *ifp = sc->sc_ifp;

	ATH_KTR(sc, ATH_KTR_TX, 0, "ath_start_task: start");

	/* XXX is it ok to hold the ATH_LOCK here? */
	ATH_PCU_LOCK(sc);
	if (sc->sc_inreset_cnt > 0) {
		device_printf(sc->sc_dev,
		    "%s: sc_inreset_cnt > 0; bailing\n", __func__);
		ATH_PCU_UNLOCK(sc);
		IF_LOCK(&ifp->if_snd);
		sc->sc_stats.ast_tx_qstop++;
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		IF_UNLOCK(&ifp->if_snd);
		ATH_KTR(sc, ATH_KTR_TX, 0, "ath_start_task: OACTIVE, finish");
		return;
	}
	sc->sc_txstart_cnt++;
	ATH_PCU_UNLOCK(sc);

	ATH_TX_LOCK(sc);
	ath_start(sc->sc_ifp);
	ATH_TX_UNLOCK(sc);

	ATH_PCU_LOCK(sc);
	sc->sc_txstart_cnt--;
	ATH_PCU_UNLOCK(sc);
	ATH_KTR(sc, ATH_KTR_TX, 0, "ath_start_task: finished");
}

void
ath_start(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct mbuf *m, *next;
	ath_bufhead frags;
	int npkts = 0;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 || sc->sc_invalid)
		return;

	ATH_TX_LOCK_ASSERT(sc);

	ATH_KTR(sc, ATH_KTR_TX, 0, "ath_start: called");

	for (;;) {
		ATH_TXBUF_LOCK(sc);
		if (sc->sc_txbuf_cnt <= sc->sc_txq_data_minfree) {
			/* XXX increment counter? */
			ATH_TXBUF_UNLOCK(sc);
			IF_LOCK(&ifp->if_snd);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IF_UNLOCK(&ifp->if_snd);
			break;
		}
		ATH_TXBUF_UNLOCK(sc);
		
		/*
		 * Grab a TX buffer and associated resources.
		 */
		bf = ath_getbuf(sc, ATH_BUFTYPE_NORMAL);
		if (bf == NULL)
			break;

		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			ATH_TXBUF_LOCK(sc);
			ath_returnbuf_head(sc, bf);
			ATH_TXBUF_UNLOCK(sc);
			break;
		}
		ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
		npkts ++;
		/*
		 * Check for fragmentation.  If this frame
		 * has been broken up verify we have enough
		 * buffers to send all the fragments so all
		 * go out or none...
		 */
		TAILQ_INIT(&frags);
		if ((m->m_flags & M_FRAG) &&
		    !ath_txfrag_setup(sc, &frags, m, ni)) {
			DPRINTF(sc, ATH_DEBUG_XMIT,
			    "%s: out of txfrag buffers\n", __func__);
			sc->sc_stats.ast_tx_nofrag++;
			ifp->if_oerrors++;
			ath_freetx(m);
			goto bad;
		}
		ifp->if_opackets++;
	nextfrag:
		/*
		 * Pass the frame to the h/w for transmission.
		 * Fragmented frames have each frag chained together
		 * with m_nextpkt.  We know there are sufficient ath_buf's
		 * to send all the frags because of work done by
		 * ath_txfrag_setup.  We leave m_nextpkt set while
		 * calling ath_tx_start so it can use it to extend the
		 * the tx duration to cover the subsequent frag and
		 * so it can reclaim all the mbufs in case of an error;
		 * ath_tx_start clears m_nextpkt once it commits to
		 * handing the frame to the hardware.
		 */
		next = m->m_nextpkt;
		if (ath_tx_start(sc, ni, bf, m)) {
	bad:
			ifp->if_oerrors++;
	reclaim:
			bf->bf_m = NULL;
			bf->bf_node = NULL;
			ATH_TXBUF_LOCK(sc);
			ath_returnbuf_head(sc, bf);
			ath_txfrag_cleanup(sc, &frags, ni);
			ATH_TXBUF_UNLOCK(sc);
			/*
			 * XXX todo, free the node outside of
			 * the TX lock context!
			 */
			if (ni != NULL)
				ieee80211_free_node(ni);
			continue;
		}

		/*
		 * Check here if the node is in power save state.
		 */
		ath_tx_update_tim(sc, ni, 1);

		if (next != NULL) {
			/*
			 * Beware of state changing between frags.
			 * XXX check sta power-save state?
			 */
			if (ni->ni_vap->iv_state != IEEE80211_S_RUN) {
				DPRINTF(sc, ATH_DEBUG_XMIT,
				    "%s: flush fragmented packet, state %s\n",
				    __func__,
				    ieee80211_state_name[ni->ni_vap->iv_state]);
				/* XXX dmamap */
				ath_freetx(next);
				goto reclaim;
			}
			m = next;
			bf = TAILQ_FIRST(&frags);
			KASSERT(bf != NULL, ("no buf for txfrag"));
			TAILQ_REMOVE(&frags, bf, bf_list);
			goto nextfrag;
		}

		sc->sc_wd_timer = 5;
	}
	ATH_KTR(sc, ATH_KTR_TX, 1, "ath_start: finished; npkts=%d", npkts);
}
static int
ath_media_change(struct ifnet *ifp)
{
	int error = ieee80211_media_change(ifp);
	/* NB: only the fixed rate can change and that doesn't need a reset */
	return (error == ENETRESET ? 0 : error);
}

/*
 * Block/unblock tx+rx processing while a key change is done.
 * We assume the caller serializes key management operations
 * so we only need to worry about synchronization with other
 * uses that originate in the driver.
 */
static void
ath_key_update_begin(struct ieee80211vap *vap)
{
	struct ifnet *ifp = vap->iv_ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s:\n", __func__);
	taskqueue_block(sc->sc_tq);
	IF_LOCK(&ifp->if_snd);		/* NB: doesn't block mgmt frames */
}

static void
ath_key_update_end(struct ieee80211vap *vap)
{
	struct ifnet *ifp = vap->iv_ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s:\n", __func__);
	IF_UNLOCK(&ifp->if_snd);
	taskqueue_unblock(sc->sc_tq);
}

static void
ath_update_promisc(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	u_int32_t rfilt;

	/* configure rx filter */
	rfilt = ath_calcrxfilter(sc);
	ath_hal_setrxfilter(sc->sc_ah, rfilt);

	DPRINTF(sc, ATH_DEBUG_MODE, "%s: RX filter 0x%x\n", __func__, rfilt);
}

static void
ath_update_mcast(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	u_int32_t mfilt[2];

	/* calculate and install multicast filter */
	if ((ifp->if_flags & IFF_ALLMULTI) == 0) {
		struct ifmultiaddr *ifma;
		/*
		 * Merge multicast addresses to form the hardware filter.
		 */
		mfilt[0] = mfilt[1] = 0;
		if_maddr_rlock(ifp);	/* XXX need some fiddling to remove? */
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			caddr_t dl;
			u_int32_t val;
			u_int8_t pos;

			/* calculate XOR of eight 6bit values */
			dl = LLADDR((struct sockaddr_dl *) ifma->ifma_addr);
			val = LE_READ_4(dl + 0);
			pos = (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
			val = LE_READ_4(dl + 3);
			pos ^= (val >> 18) ^ (val >> 12) ^ (val >> 6) ^ val;
			pos &= 0x3f;
			mfilt[pos / 32] |= (1 << (pos % 32));
		}
		if_maddr_runlock(ifp);
	} else
		mfilt[0] = mfilt[1] = ~0;
	ath_hal_setmcastfilter(sc->sc_ah, mfilt[0], mfilt[1]);
	DPRINTF(sc, ATH_DEBUG_MODE, "%s: MC filter %08x:%08x\n",
		__func__, mfilt[0], mfilt[1]);
}

void
ath_mode_init(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ath_hal *ah = sc->sc_ah;
	u_int32_t rfilt;

	/* configure rx filter */
	rfilt = ath_calcrxfilter(sc);
	ath_hal_setrxfilter(ah, rfilt);

	/* configure operational mode */
	ath_hal_setopmode(ah);

	DPRINTF(sc, ATH_DEBUG_STATE | ATH_DEBUG_MODE,
	    "%s: ah=%p, ifp=%p, if_addr=%p\n",
	    __func__,
	    ah,
	    ifp,
	    (ifp == NULL) ? NULL : ifp->if_addr);

	/* handle any link-level address change */
	ath_hal_setmac(ah, IF_LLADDR(ifp));

	/* calculate and install multicast filter */
	ath_update_mcast(ifp);
}

/*
 * Set the slot time based on the current setting.
 */
void
ath_setslottime(struct ath_softc *sc)
{
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	u_int usec;

	if (IEEE80211_IS_CHAN_HALF(ic->ic_curchan))
		usec = 13;
	else if (IEEE80211_IS_CHAN_QUARTER(ic->ic_curchan))
		usec = 21;
	else if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan)) {
		/* honor short/long slot time only in 11g */
		/* XXX shouldn't honor on pure g or turbo g channel */
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			usec = HAL_SLOT_TIME_9;
		else
			usec = HAL_SLOT_TIME_20;
	} else
		usec = HAL_SLOT_TIME_9;

	DPRINTF(sc, ATH_DEBUG_RESET,
	    "%s: chan %u MHz flags 0x%x %s slot, %u usec\n",
	    __func__, ic->ic_curchan->ic_freq, ic->ic_curchan->ic_flags,
	    ic->ic_flags & IEEE80211_F_SHSLOT ? "short" : "long", usec);

	ath_hal_setslottime(ah, usec);
	sc->sc_updateslot = OK;
}

/*
 * Callback from the 802.11 layer to update the
 * slot time based on the current setting.
 */
static void
ath_updateslot(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;

	/*
	 * When not coordinating the BSS, change the hardware
	 * immediately.  For other operation we defer the change
	 * until beacon updates have propagated to the stations.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	    ic->ic_opmode == IEEE80211_M_MBSS)
		sc->sc_updateslot = UPDATE;
	else
		ath_setslottime(sc);
}

/*
 * Append the contents of src to dst; both queues
 * are assumed to be locked.
 */
void
ath_txqmove(struct ath_txq *dst, struct ath_txq *src)
{

	ATH_TXQ_LOCK_ASSERT(src);
	ATH_TXQ_LOCK_ASSERT(dst);

	TAILQ_CONCAT(&dst->axq_q, &src->axq_q, bf_list);
	dst->axq_link = src->axq_link;
	src->axq_link = NULL;
	dst->axq_depth += src->axq_depth;
	dst->axq_aggr_depth += src->axq_aggr_depth;
	src->axq_depth = 0;
	src->axq_aggr_depth = 0;
}

/*
 * Reset the hardware, with no loss.
 *
 * This can't be used for a general case reset.
 */
static void
ath_reset_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

#if 0
	if_printf(ifp, "%s: resetting\n", __func__);
#endif
	ath_reset(ifp, ATH_RESET_NOLOSS);
}

/*
 * Reset the hardware after detecting beacons have stopped.
 */
static void
ath_bstuck_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t hangs = 0;

	if (ath_hal_gethangstate(sc->sc_ah, 0xff, &hangs) && hangs != 0)
		if_printf(ifp, "bb hang detected (0x%x)\n", hangs);

	if_printf(ifp, "stuck beacon; resetting (bmiss count %u)\n",
		sc->sc_bmisscount);
	sc->sc_stats.ast_bstuck++;
	/*
	 * This assumes that there's no simultaneous channel mode change
	 * occuring.
	 */
	ath_reset(ifp, ATH_RESET_NOLOSS);
}

static void
ath_load_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *paddr = (bus_addr_t*) arg;
	KASSERT(error == 0, ("error %u on bus_dma callback", error));
	*paddr = segs->ds_addr;
}

/*
 * Allocate the descriptors and appropriate DMA tag/setup.
 *
 * For some situations (eg EDMA TX completion), there isn't a requirement
 * for the ath_buf entries to be allocated.
 */
int
ath_descdma_alloc_desc(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head,
	const char *name, int ds_size, int ndesc)
{
#define	DS2PHYS(_dd, _ds) \
	((_dd)->dd_desc_paddr + ((caddr_t)(_ds) - (caddr_t)(_dd)->dd_desc))
#define	ATH_DESC_4KB_BOUND_CHECK(_daddr, _len) \
	((((u_int32_t)(_daddr) & 0xFFF) > (0x1000 - (_len))) ? 1 : 0)
	struct ifnet *ifp = sc->sc_ifp;
	int error;

	dd->dd_descsize = ds_size;

	DPRINTF(sc, ATH_DEBUG_RESET,
	    "%s: %s DMA: %u desc, %d bytes per descriptor\n",
	    __func__, name, ndesc, dd->dd_descsize);

	dd->dd_name = name;
	dd->dd_desc_len = dd->dd_descsize * ndesc;

	/*
	 * Merlin work-around:
	 * Descriptors that cross the 4KB boundary can't be used.
	 * Assume one skipped descriptor per 4KB page.
	 */
	if (! ath_hal_split4ktrans(sc->sc_ah)) {
		int numpages = dd->dd_desc_len / 4096;
		dd->dd_desc_len += ds_size * numpages;
	}

	/*
	 * Setup DMA descriptor area.
	 *
	 * BUS_DMA_ALLOCNOW is not used; we never use bounce
	 * buffers for the descriptors themselves.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev),	/* parent */
		       PAGE_SIZE, 0,		/* alignment, bounds */
		       BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		       BUS_SPACE_MAXADDR,	/* highaddr */
		       NULL, NULL,		/* filter, filterarg */
		       dd->dd_desc_len,		/* maxsize */
		       1,			/* nsegments */
		       dd->dd_desc_len,		/* maxsegsize */
		       0,			/* flags */
		       NULL,			/* lockfunc */
		       NULL,			/* lockarg */
		       &dd->dd_dmat);
	if (error != 0) {
		if_printf(ifp, "cannot allocate %s DMA tag\n", dd->dd_name);
		return error;
	}

	/* allocate descriptors */
	error = bus_dmamem_alloc(dd->dd_dmat, (void**) &dd->dd_desc,
				 BUS_DMA_NOWAIT | BUS_DMA_COHERENT,
				 &dd->dd_dmamap);
	if (error != 0) {
		if_printf(ifp, "unable to alloc memory for %u %s descriptors, "
			"error %u\n", ndesc, dd->dd_name, error);
		goto fail1;
	}

	error = bus_dmamap_load(dd->dd_dmat, dd->dd_dmamap,
				dd->dd_desc, dd->dd_desc_len,
				ath_load_cb, &dd->dd_desc_paddr,
				BUS_DMA_NOWAIT);
	if (error != 0) {
		if_printf(ifp, "unable to map %s descriptors, error %u\n",
			dd->dd_name, error);
		goto fail2;
	}

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA map: %p (%lu) -> %p (%lu)\n",
	    __func__, dd->dd_name, (uint8_t *) dd->dd_desc,
	    (u_long) dd->dd_desc_len, (caddr_t) dd->dd_desc_paddr,
	    /*XXX*/ (u_long) dd->dd_desc_len);

	return (0);

fail2:
	bus_dmamem_free(dd->dd_dmat, dd->dd_desc, dd->dd_dmamap);
fail1:
	bus_dma_tag_destroy(dd->dd_dmat);
	memset(dd, 0, sizeof(*dd));
	return error;
#undef DS2PHYS
#undef ATH_DESC_4KB_BOUND_CHECK
}

int
ath_descdma_setup(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head,
	const char *name, int ds_size, int nbuf, int ndesc)
{
#define	DS2PHYS(_dd, _ds) \
	((_dd)->dd_desc_paddr + ((caddr_t)(_ds) - (caddr_t)(_dd)->dd_desc))
#define	ATH_DESC_4KB_BOUND_CHECK(_daddr, _len) \
	((((u_int32_t)(_daddr) & 0xFFF) > (0x1000 - (_len))) ? 1 : 0)
	struct ifnet *ifp = sc->sc_ifp;
	uint8_t *ds;
	struct ath_buf *bf;
	int i, bsize, error;

	/* Allocate descriptors */
	error = ath_descdma_alloc_desc(sc, dd, head, name, ds_size,
	    nbuf * ndesc);

	/* Assume any errors during allocation were dealt with */
	if (error != 0) {
		return (error);
	}

	ds = (uint8_t *) dd->dd_desc;

	/* allocate rx buffers */
	bsize = sizeof(struct ath_buf) * nbuf;
	bf = malloc(bsize, M_ATHDEV, M_NOWAIT | M_ZERO);
	if (bf == NULL) {
		if_printf(ifp, "malloc of %s buffers failed, size %u\n",
			dd->dd_name, bsize);
		goto fail3;
	}
	dd->dd_bufptr = bf;

	TAILQ_INIT(head);
	for (i = 0; i < nbuf; i++, bf++, ds += (ndesc * dd->dd_descsize)) {
		bf->bf_desc = (struct ath_desc *) ds;
		bf->bf_daddr = DS2PHYS(dd, ds);
		if (! ath_hal_split4ktrans(sc->sc_ah)) {
			/*
			 * Merlin WAR: Skip descriptor addresses which
			 * cause 4KB boundary crossing along any point
			 * in the descriptor.
			 */
			 if (ATH_DESC_4KB_BOUND_CHECK(bf->bf_daddr,
			     dd->dd_descsize)) {
				/* Start at the next page */
				ds += 0x1000 - (bf->bf_daddr & 0xFFF);
				bf->bf_desc = (struct ath_desc *) ds;
				bf->bf_daddr = DS2PHYS(dd, ds);
			}
		}
		error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT,
				&bf->bf_dmamap);
		if (error != 0) {
			if_printf(ifp, "unable to create dmamap for %s "
				"buffer %u, error %u\n", dd->dd_name, i, error);
			ath_descdma_cleanup(sc, dd, head);
			return error;
		}
		bf->bf_lastds = bf->bf_desc;	/* Just an initial value */
		TAILQ_INSERT_TAIL(head, bf, bf_list);
	}

	/*
	 * XXX TODO: ensure that ds doesn't overflow the descriptor
	 * allocation otherwise weird stuff will occur and crash your
	 * machine.
	 */
	return 0;
	/* XXX this should likely just call ath_descdma_cleanup() */
fail3:
	bus_dmamap_unload(dd->dd_dmat, dd->dd_dmamap);
	bus_dmamem_free(dd->dd_dmat, dd->dd_desc, dd->dd_dmamap);
	bus_dma_tag_destroy(dd->dd_dmat);
	memset(dd, 0, sizeof(*dd));
	return error;
#undef DS2PHYS
#undef ATH_DESC_4KB_BOUND_CHECK
}

/*
 * Allocate ath_buf entries but no descriptor contents.
 *
 * This is for RX EDMA where the descriptors are the header part of
 * the RX buffer.
 */
int
ath_descdma_setup_rx_edma(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head,
	const char *name, int nbuf, int rx_status_len)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ath_buf *bf;
	int i, bsize, error;

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA: %u buffers\n",
	    __func__, name, nbuf);

	dd->dd_name = name;
	/*
	 * This is (mostly) purely for show.  We're not allocating any actual
	 * descriptors here as EDMA RX has the descriptor be part
	 * of the RX buffer.
	 *
	 * However, dd_desc_len is used by ath_descdma_free() to determine
	 * whether we have already freed this DMA mapping.
	 */
	dd->dd_desc_len = rx_status_len * nbuf;
	dd->dd_descsize = rx_status_len;

	/* allocate rx buffers */
	bsize = sizeof(struct ath_buf) * nbuf;
	bf = malloc(bsize, M_ATHDEV, M_NOWAIT | M_ZERO);
	if (bf == NULL) {
		if_printf(ifp, "malloc of %s buffers failed, size %u\n",
			dd->dd_name, bsize);
		error = ENOMEM;
		goto fail3;
	}
	dd->dd_bufptr = bf;

	TAILQ_INIT(head);
	for (i = 0; i < nbuf; i++, bf++) {
		bf->bf_desc = NULL;
		bf->bf_daddr = 0;
		bf->bf_lastds = NULL;	/* Just an initial value */

		error = bus_dmamap_create(sc->sc_dmat, BUS_DMA_NOWAIT,
				&bf->bf_dmamap);
		if (error != 0) {
			if_printf(ifp, "unable to create dmamap for %s "
				"buffer %u, error %u\n", dd->dd_name, i, error);
			ath_descdma_cleanup(sc, dd, head);
			return error;
		}
		TAILQ_INSERT_TAIL(head, bf, bf_list);
	}
	return 0;
fail3:
	memset(dd, 0, sizeof(*dd));
	return error;
}

void
ath_descdma_cleanup(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head)
{
	struct ath_buf *bf;
	struct ieee80211_node *ni;
	int do_warning = 0;

	if (dd->dd_dmamap != 0) {
		bus_dmamap_unload(dd->dd_dmat, dd->dd_dmamap);
		bus_dmamem_free(dd->dd_dmat, dd->dd_desc, dd->dd_dmamap);
		bus_dma_tag_destroy(dd->dd_dmat);
	}

	if (head != NULL) {
		TAILQ_FOREACH(bf, head, bf_list) {
			if (bf->bf_m) {
				/*
				 * XXX warn if there's buffers here.
				 * XXX it should have been freed by the
				 * owner!
				 */
				
				if (do_warning == 0) {
					do_warning = 1;
					device_printf(sc->sc_dev,
					    "%s: %s: mbuf should've been"
					    " unmapped/freed!\n",
					    __func__,
					    dd->dd_name);
				}
				bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
				m_freem(bf->bf_m);
				bf->bf_m = NULL;
			}
			if (bf->bf_dmamap != NULL) {
				bus_dmamap_destroy(sc->sc_dmat, bf->bf_dmamap);
				bf->bf_dmamap = NULL;
			}
			ni = bf->bf_node;
			bf->bf_node = NULL;
			if (ni != NULL) {
				/*
				 * Reclaim node reference.
				 */
				ieee80211_free_node(ni);
			}
		}
	}

	if (head != NULL)
		TAILQ_INIT(head);

	if (dd->dd_bufptr != NULL)
		free(dd->dd_bufptr, M_ATHDEV);
	memset(dd, 0, sizeof(*dd));
}

static int
ath_desc_alloc(struct ath_softc *sc)
{
	int error;

	error = ath_descdma_setup(sc, &sc->sc_txdma, &sc->sc_txbuf,
		    "tx", sc->sc_tx_desclen, ath_txbuf, ATH_MAX_SCATTER);
	if (error != 0) {
		return error;
	}
	sc->sc_txbuf_cnt = ath_txbuf;

	error = ath_descdma_setup(sc, &sc->sc_txdma_mgmt, &sc->sc_txbuf_mgmt,
		    "tx_mgmt", sc->sc_tx_desclen, ath_txbuf_mgmt,
		    ATH_TXDESC);
	if (error != 0) {
		ath_descdma_cleanup(sc, &sc->sc_txdma, &sc->sc_txbuf);
		return error;
	}

	/*
	 * XXX mark txbuf_mgmt frames with ATH_BUF_MGMT, so the
	 * flag doesn't have to be set in ath_getbuf_locked().
	 */

	error = ath_descdma_setup(sc, &sc->sc_bdma, &sc->sc_bbuf,
			"beacon", sc->sc_tx_desclen, ATH_BCBUF, 1);
	if (error != 0) {
		ath_descdma_cleanup(sc, &sc->sc_txdma, &sc->sc_txbuf);
		ath_descdma_cleanup(sc, &sc->sc_txdma_mgmt,
		    &sc->sc_txbuf_mgmt);
		return error;
	}
	return 0;
}

static void
ath_desc_free(struct ath_softc *sc)
{

	if (sc->sc_bdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_bdma, &sc->sc_bbuf);
	if (sc->sc_txdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_txdma, &sc->sc_txbuf);
	if (sc->sc_txdma_mgmt.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_txdma_mgmt,
		    &sc->sc_txbuf_mgmt);
}

static struct ieee80211_node *
ath_node_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	const size_t space = sizeof(struct ath_node) + sc->sc_rc->arc_space;
	struct ath_node *an;

	an = malloc(space, M_80211_NODE, M_NOWAIT|M_ZERO);
	if (an == NULL) {
		/* XXX stat+msg */
		return NULL;
	}
	ath_rate_node_init(sc, an);

	/* Setup the mutex - there's no associd yet so set the name to NULL */
	snprintf(an->an_name, sizeof(an->an_name), "%s: node %p",
	    device_get_nameunit(sc->sc_dev), an);
	mtx_init(&an->an_mtx, an->an_name, NULL, MTX_DEF);

	/* XXX setup ath_tid */
	ath_tx_tid_init(sc, an);

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: an %p\n", __func__, an);
	return &an->an_node;
}

static void
ath_node_cleanup(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	/* Cleanup ath_tid, free unused bufs, unlink bufs in TXQ */
	ath_tx_node_flush(sc, ATH_NODE(ni));
	ath_rate_node_cleanup(sc, ATH_NODE(ni));
	sc->sc_node_cleanup(ni);
}

static void
ath_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: ni %p\n", __func__, ni);
	mtx_destroy(&ATH_NODE(ni)->an_mtx);
	sc->sc_node_free(ni);
}

static void
ath_node_getsignal(const struct ieee80211_node *ni, int8_t *rssi, int8_t *noise)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;

	*rssi = ic->ic_node_getrssi(ni);
	if (ni->ni_chan != IEEE80211_CHAN_ANYC)
		*noise = ath_hal_getchannoise(ah, ni->ni_chan);
	else
		*noise = -95;		/* nominally correct */
}

/*
 * Set the default antenna.
 */
void
ath_setdefantenna(struct ath_softc *sc, u_int antenna)
{
	struct ath_hal *ah = sc->sc_ah;

	/* XXX block beacon interrupts */
	ath_hal_setdefantenna(ah, antenna);
	if (sc->sc_defant != antenna)
		sc->sc_stats.ast_ant_defswitch++;
	sc->sc_defant = antenna;
	sc->sc_rxotherant = 0;
}

static void
ath_txq_init(struct ath_softc *sc, struct ath_txq *txq, int qnum)
{
	txq->axq_qnum = qnum;
	txq->axq_ac = 0;
	txq->axq_depth = 0;
	txq->axq_aggr_depth = 0;
	txq->axq_intrcnt = 0;
	txq->axq_link = NULL;
	txq->axq_softc = sc;
	TAILQ_INIT(&txq->axq_q);
	TAILQ_INIT(&txq->axq_tidq);
	TAILQ_INIT(&txq->fifo.axq_q);
	ATH_TXQ_LOCK_INIT(sc, txq);
}

/*
 * Setup a h/w transmit queue.
 */
static struct ath_txq *
ath_txq_setup(struct ath_softc *sc, int qtype, int subtype)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;
	int qnum;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_subtype = subtype;
	qi.tqi_aifs = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmin = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmax = HAL_TXQ_USEDEFAULT;
	/*
	 * Enable interrupts only for EOL and DESC conditions.
	 * We mark tx descriptors to receive a DESC interrupt
	 * when a tx queue gets deep; otherwise waiting for the
	 * EOL to reap descriptors.  Note that this is done to
	 * reduce interrupt load and this only defers reaping
	 * descriptors, never transmitting frames.  Aside from
	 * reducing interrupts this also permits more concurrency.
	 * The only potential downside is if the tx queue backs
	 * up in which case the top half of the kernel may backup
	 * due to a lack of tx descriptors.
	 */
	if (sc->sc_isedma)
		qi.tqi_qflags = HAL_TXQ_TXEOLINT_ENABLE |
		    HAL_TXQ_TXOKINT_ENABLE;
	else
		qi.tqi_qflags = HAL_TXQ_TXEOLINT_ENABLE |
		    HAL_TXQ_TXDESCINT_ENABLE;

	qnum = ath_hal_setuptxqueue(ah, qtype, &qi);
	if (qnum == -1) {
		/*
		 * NB: don't print a message, this happens
		 * normally on parts with too few tx queues
		 */
		return NULL;
	}
	if (qnum >= N(sc->sc_txq)) {
		device_printf(sc->sc_dev,
			"hal qnum %u out of range, max %zu!\n",
			qnum, N(sc->sc_txq));
		ath_hal_releasetxqueue(ah, qnum);
		return NULL;
	}
	if (!ATH_TXQ_SETUP(sc, qnum)) {
		ath_txq_init(sc, &sc->sc_txq[qnum], qnum);
		sc->sc_txqsetup |= 1<<qnum;
	}
	return &sc->sc_txq[qnum];
#undef N
}

/*
 * Setup a hardware data transmit queue for the specified
 * access control.  The hal may not support all requested
 * queues in which case it will return a reference to a
 * previously setup queue.  We record the mapping from ac's
 * to h/w queues for use by ath_tx_start and also track
 * the set of h/w queues being used to optimize work in the
 * transmit interrupt handler and related routines.
 */
static int
ath_tx_setup(struct ath_softc *sc, int ac, int haltype)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ath_txq *txq;

	if (ac >= N(sc->sc_ac2q)) {
		device_printf(sc->sc_dev, "AC %u out of range, max %zu!\n",
			ac, N(sc->sc_ac2q));
		return 0;
	}
	txq = ath_txq_setup(sc, HAL_TX_QUEUE_DATA, haltype);
	if (txq != NULL) {
		txq->axq_ac = ac;
		sc->sc_ac2q[ac] = txq;
		return 1;
	} else
		return 0;
#undef N
}

/*
 * Update WME parameters for a transmit queue.
 */
static int
ath_txq_update(struct ath_softc *sc, int ac)
{
#define	ATH_EXPONENT_TO_VALUE(v)	((1<<v)-1)
#define	ATH_TXOP_TO_US(v)		(v<<5)
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_txq *txq = sc->sc_ac2q[ac];
	struct wmeParams *wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;

	ath_hal_gettxqueueprops(ah, txq->axq_qnum, &qi);
#ifdef IEEE80211_SUPPORT_TDMA
	if (sc->sc_tdma) {
		/*
		 * AIFS is zero so there's no pre-transmit wait.  The
		 * burst time defines the slot duration and is configured
		 * through net80211.  The QCU is setup to not do post-xmit
		 * back off, lockout all lower-priority QCU's, and fire
		 * off the DMA beacon alert timer which is setup based
		 * on the slot configuration.
		 */
		qi.tqi_qflags = HAL_TXQ_TXOKINT_ENABLE
			      | HAL_TXQ_TXERRINT_ENABLE
			      | HAL_TXQ_TXURNINT_ENABLE
			      | HAL_TXQ_TXEOLINT_ENABLE
			      | HAL_TXQ_DBA_GATED
			      | HAL_TXQ_BACKOFF_DISABLE
			      | HAL_TXQ_ARB_LOCKOUT_GLOBAL
			      ;
		qi.tqi_aifs = 0;
		/* XXX +dbaprep? */
		qi.tqi_readyTime = sc->sc_tdmaslotlen;
		qi.tqi_burstTime = qi.tqi_readyTime;
	} else {
#endif
		/*
		 * XXX shouldn't this just use the default flags
		 * used in the previous queue setup?
		 */
		qi.tqi_qflags = HAL_TXQ_TXOKINT_ENABLE
			      | HAL_TXQ_TXERRINT_ENABLE
			      | HAL_TXQ_TXDESCINT_ENABLE
			      | HAL_TXQ_TXURNINT_ENABLE
			      | HAL_TXQ_TXEOLINT_ENABLE
			      ;
		qi.tqi_aifs = wmep->wmep_aifsn;
		qi.tqi_cwmin = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
		qi.tqi_cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);
		qi.tqi_readyTime = 0;
		qi.tqi_burstTime = ATH_TXOP_TO_US(wmep->wmep_txopLimit);
#ifdef IEEE80211_SUPPORT_TDMA
	}
#endif

	DPRINTF(sc, ATH_DEBUG_RESET,
	    "%s: Q%u qflags 0x%x aifs %u cwmin %u cwmax %u burstTime %u\n",
	    __func__, txq->axq_qnum, qi.tqi_qflags,
	    qi.tqi_aifs, qi.tqi_cwmin, qi.tqi_cwmax, qi.tqi_burstTime);

	if (!ath_hal_settxqueueprops(ah, txq->axq_qnum, &qi)) {
		if_printf(ifp, "unable to update hardware queue "
			"parameters for %s traffic!\n",
			ieee80211_wme_acnames[ac]);
		return 0;
	} else {
		ath_hal_resettxqueue(ah, txq->axq_qnum); /* push to h/w */
		return 1;
	}
#undef ATH_TXOP_TO_US
#undef ATH_EXPONENT_TO_VALUE
}

/*
 * Callback from the 802.11 layer to update WME parameters.
 */
int
ath_wme_update(struct ieee80211com *ic)
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;

	return !ath_txq_update(sc, WME_AC_BE) ||
	    !ath_txq_update(sc, WME_AC_BK) ||
	    !ath_txq_update(sc, WME_AC_VI) ||
	    !ath_txq_update(sc, WME_AC_VO) ? EIO : 0;
}

/*
 * Reclaim resources for a setup queue.
 */
static void
ath_tx_cleanupq(struct ath_softc *sc, struct ath_txq *txq)
{

	ath_hal_releasetxqueue(sc->sc_ah, txq->axq_qnum);
	sc->sc_txqsetup &= ~(1<<txq->axq_qnum);
	ATH_TXQ_LOCK_DESTROY(txq);
}

/*
 * Reclaim all tx queue resources.
 */
static void
ath_tx_cleanup(struct ath_softc *sc)
{
	int i;

	ATH_TXBUF_LOCK_DESTROY(sc);
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_cleanupq(sc, &sc->sc_txq[i]);
}

/*
 * Return h/w rate index for an IEEE rate (w/o basic rate bit)
 * using the current rates in sc_rixmap.
 */
int
ath_tx_findrix(const struct ath_softc *sc, uint8_t rate)
{
	int rix = sc->sc_rixmap[rate];
	/* NB: return lowest rix for invalid rate */
	return (rix == 0xff ? 0 : rix);
}

static void
ath_tx_update_stats(struct ath_softc *sc, struct ath_tx_status *ts,
    struct ath_buf *bf)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int sr, lr, pri;

	if (ts->ts_status == 0) {
		u_int8_t txant = ts->ts_antenna;
		sc->sc_stats.ast_ant_tx[txant]++;
		sc->sc_ant_tx[txant]++;
		if (ts->ts_finaltsi != 0)
			sc->sc_stats.ast_tx_altrate++;
		pri = M_WME_GETAC(bf->bf_m);
		if (pri >= WME_AC_VO)
			ic->ic_wme.wme_hipri_traffic++;
		if ((bf->bf_state.bfs_txflags & HAL_TXDESC_NOACK) == 0)
			ni->ni_inact = ni->ni_inact_reload;
	} else {
		if (ts->ts_status & HAL_TXERR_XRETRY)
			sc->sc_stats.ast_tx_xretries++;
		if (ts->ts_status & HAL_TXERR_FIFO)
			sc->sc_stats.ast_tx_fifoerr++;
		if (ts->ts_status & HAL_TXERR_FILT)
			sc->sc_stats.ast_tx_filtered++;
		if (ts->ts_status & HAL_TXERR_XTXOP)
			sc->sc_stats.ast_tx_xtxop++;
		if (ts->ts_status & HAL_TXERR_TIMER_EXPIRED)
			sc->sc_stats.ast_tx_timerexpired++;

		if (bf->bf_m->m_flags & M_FF)
			sc->sc_stats.ast_ff_txerr++;
	}
	/* XXX when is this valid? */
	if (ts->ts_flags & HAL_TX_DESC_CFG_ERR)
		sc->sc_stats.ast_tx_desccfgerr++;
	/*
	 * This can be valid for successful frame transmission!
	 * If there's a TX FIFO underrun during aggregate transmission,
	 * the MAC will pad the rest of the aggregate with delimiters.
	 * If a BA is returned, the frame is marked as "OK" and it's up
	 * to the TX completion code to notice which frames weren't
	 * successfully transmitted.
	 */
	if (ts->ts_flags & HAL_TX_DATA_UNDERRUN)
		sc->sc_stats.ast_tx_data_underrun++;
	if (ts->ts_flags & HAL_TX_DELIM_UNDERRUN)
		sc->sc_stats.ast_tx_delim_underrun++;

	sr = ts->ts_shortretry;
	lr = ts->ts_longretry;
	sc->sc_stats.ast_tx_shortretry += sr;
	sc->sc_stats.ast_tx_longretry += lr;

}

/*
 * The default completion. If fail is 1, this means
 * "please don't retry the frame, and just return -1 status
 * to the net80211 stack.
 */
void
ath_tx_default_comp(struct ath_softc *sc, struct ath_buf *bf, int fail)
{
	struct ath_tx_status *ts = &bf->bf_status.ds_txstat;
	int st;

	if (fail == 1)
		st = -1;
	else
		st = ((bf->bf_state.bfs_txflags & HAL_TXDESC_NOACK) == 0) ?
		    ts->ts_status : HAL_TXERR_XRETRY;

#if 0
	if (bf->bf_state.bfs_dobaw)
		device_printf(sc->sc_dev,
		    "%s: bf %p: seqno %d: dobaw should've been cleared!\n",
		    __func__,
		    bf,
		    SEQNO(bf->bf_state.bfs_seqno));
#endif
	if (bf->bf_next != NULL)
		device_printf(sc->sc_dev,
		    "%s: bf %p: seqno %d: bf_next not NULL!\n",
		    __func__,
		    bf,
		    SEQNO(bf->bf_state.bfs_seqno));

	/*
	 * Check if the node software queue is empty; if so
	 * then clear the TIM.
	 *
	 * This needs to be done before the buffer is freed as
	 * otherwise the node reference will have been released
	 * and the node may not actually exist any longer.
	 *
	 * XXX I don't like this belonging here, but it's cleaner
	 * to do it here right now then all the other places
	 * where ath_tx_default_comp() is called.
	 *
	 * XXX TODO: during drain, ensure that the callback is
	 * being called so we get a chance to update the TIM.
	 */
	if (bf->bf_node)
		ath_tx_update_tim(sc, bf->bf_node, 0);

	/*
	 * Do any tx complete callback.  Note this must
	 * be done before releasing the node reference.
	 * This will free the mbuf, release the net80211
	 * node and recycle the ath_buf.
	 */
	ath_tx_freebuf(sc, bf, st);
}

/*
 * Update rate control with the given completion status.
 */
void
ath_tx_update_ratectrl(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_rc_series *rc, struct ath_tx_status *ts, int frmlen,
    int nframes, int nbad)
{
	struct ath_node *an;

	/* Only for unicast frames */
	if (ni == NULL)
		return;

	an = ATH_NODE(ni);
	ATH_NODE_UNLOCK_ASSERT(an);

	if ((ts->ts_status & HAL_TXERR_FILT) == 0) {
		ATH_NODE_LOCK(an);
		ath_rate_tx_complete(sc, an, rc, ts, frmlen, nframes, nbad);
		ATH_NODE_UNLOCK(an);
	}
}

/*
 * Process the completion of the given buffer.
 *
 * This calls the rate control update and then the buffer completion.
 * This will either free the buffer or requeue it.  In any case, the
 * bf pointer should be treated as invalid after this function is called.
 */
void
ath_tx_process_buf_completion(struct ath_softc *sc, struct ath_txq *txq,
    struct ath_tx_status *ts, struct ath_buf *bf)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ath_node *an = NULL;

	ATH_TX_UNLOCK_ASSERT(sc);

	/* If unicast frame, update general statistics */
	if (ni != NULL) {
		an = ATH_NODE(ni);
		/* update statistics */
		ath_tx_update_stats(sc, ts, bf);
	}

	/*
	 * Call the completion handler.
	 * The completion handler is responsible for
	 * calling the rate control code.
	 *
	 * Frames with no completion handler get the
	 * rate control code called here.
	 */
	if (bf->bf_comp == NULL) {
		if ((ts->ts_status & HAL_TXERR_FILT) == 0 &&
		    (bf->bf_state.bfs_txflags & HAL_TXDESC_NOACK) == 0) {
			/*
			 * XXX assume this isn't an aggregate
			 * frame.
			 */
			ath_tx_update_ratectrl(sc, ni,
			     bf->bf_state.bfs_rc, ts,
			    bf->bf_state.bfs_pktlen, 1,
			    (ts->ts_status == 0 ? 0 : 1));
		}
		ath_tx_default_comp(sc, bf, 0);
	} else
		bf->bf_comp(sc, bf, 0);
}



/*
 * Process completed xmit descriptors from the specified queue.
 * Kick the packet scheduler if needed. This can occur from this
 * particular task.
 */
static int
ath_tx_processq(struct ath_softc *sc, struct ath_txq *txq, int dosched)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;
	struct ath_desc *ds;
	struct ath_tx_status *ts;
	struct ieee80211_node *ni;
#ifdef	IEEE80211_SUPPORT_SUPERG
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
#endif	/* IEEE80211_SUPPORT_SUPERG */
	int nacked;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: tx queue %u head %p link %p\n",
		__func__, txq->axq_qnum,
		(caddr_t)(uintptr_t) ath_hal_gettxbuf(sc->sc_ah, txq->axq_qnum),
		txq->axq_link);

	ATH_KTR(sc, ATH_KTR_TXCOMP, 4,
	    "ath_tx_processq: txq=%u head %p link %p depth %p",
	    txq->axq_qnum,
	    (caddr_t)(uintptr_t) ath_hal_gettxbuf(sc->sc_ah, txq->axq_qnum),
	    txq->axq_link,
	    txq->axq_depth);

	nacked = 0;
	for (;;) {
		ATH_TXQ_LOCK(txq);
		txq->axq_intrcnt = 0;	/* reset periodic desc intr count */
		bf = TAILQ_FIRST(&txq->axq_q);
		if (bf == NULL) {
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ds = bf->bf_lastds;	/* XXX must be setup correctly! */
		ts = &bf->bf_status.ds_txstat;

		status = ath_hal_txprocdesc(ah, ds, ts);
#ifdef ATH_DEBUG
		if (sc->sc_debug & ATH_DEBUG_XMIT_DESC)
			ath_printtxbuf(sc, bf, txq->axq_qnum, 0,
			    status == HAL_OK);
		else if ((sc->sc_debug & ATH_DEBUG_RESET) && (dosched == 0))
			ath_printtxbuf(sc, bf, txq->axq_qnum, 0,
			    status == HAL_OK);
#endif
#ifdef	ATH_DEBUG_ALQ
		if (if_ath_alq_checkdebug(&sc->sc_alq,
		    ATH_ALQ_EDMA_TXSTATUS)) {
			if_ath_alq_post(&sc->sc_alq, ATH_ALQ_EDMA_TXSTATUS,
			sc->sc_tx_statuslen,
			(char *) ds);
		}
#endif

		if (status == HAL_EINPROGRESS) {
			ATH_KTR(sc, ATH_KTR_TXCOMP, 3,
			    "ath_tx_processq: txq=%u, bf=%p ds=%p, HAL_EINPROGRESS",
			    txq->axq_qnum, bf, ds);
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ATH_TXQ_REMOVE(txq, bf, bf_list);

		/*
		 * Always mark the last buffer in this list as busy.
		 *
		 * The hardware may re-read the holding descriptor
		 * even if we hit the end of the list and try writing
		 * a new TxDP.
		 *
		 * If there's no holding descriptor then this is the
		 * last buffer in the list of buffers after a fresh
		 * reset; it'll soon become the holding buffer.
		 */
		bf->bf_last->bf_flags |= ATH_BUF_BUSY;

		if (bf->bf_state.bfs_aggr)
			txq->axq_aggr_depth--;

		ni = bf->bf_node;

		ATH_KTR(sc, ATH_KTR_TXCOMP, 5,
		    "ath_tx_processq: txq=%u, bf=%p, ds=%p, ni=%p, ts_status=0x%08x",
		    txq->axq_qnum, bf, ds, ni, ts->ts_status);
		/*
		 * If unicast frame was ack'd update RSSI,
		 * including the last rx time used to
		 * workaround phantom bmiss interrupts.
		 */
		if (ni != NULL && ts->ts_status == 0 &&
		    ((bf->bf_state.bfs_txflags & HAL_TXDESC_NOACK) == 0)) {
			nacked++;
			sc->sc_stats.ast_tx_rssi = ts->ts_rssi;
			ATH_RSSI_LPF(sc->sc_halstats.ns_avgtxrssi,
				ts->ts_rssi);
		}
		ATH_TXQ_UNLOCK(txq);

		/*
		 * Update statistics and call completion
		 */
		ath_tx_process_buf_completion(sc, txq, ts, bf);

		/* XXX at this point, bf and ni may be totally invalid */
	}
#ifdef IEEE80211_SUPPORT_SUPERG
	/*
	 * Flush fast-frame staging queue when traffic slows.
	 */
	if (txq->axq_depth <= 1)
		ieee80211_ff_flush(ic, txq->axq_ac);
#endif

	/* Kick the software TXQ scheduler */
	if (dosched) {
		ATH_TX_LOCK(sc);
		ath_txq_sched(sc, txq);
		ATH_TX_UNLOCK(sc);
	}

	ATH_KTR(sc, ATH_KTR_TXCOMP, 1,
	    "ath_tx_processq: txq=%u: done",
	    txq->axq_qnum);

	return nacked;
}

#define	TXQACTIVE(t, q)		( (t) & (1 << (q)))

/*
 * Deferred processing of transmit interrupt; special-cased
 * for a single hardware transmit queue (e.g. 5210 and 5211).
 */
static void
ath_tx_proc_q0(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t txqs;

	ATH_PCU_LOCK(sc);
	sc->sc_txproc_cnt++;
	txqs = sc->sc_txq_active;
	sc->sc_txq_active &= ~txqs;
	ATH_PCU_UNLOCK(sc);

	ATH_KTR(sc, ATH_KTR_TXCOMP, 1,
	    "ath_tx_proc_q0: txqs=0x%08x", txqs);

	if (TXQACTIVE(txqs, 0) && ath_tx_processq(sc, &sc->sc_txq[0], 1))
		/* XXX why is lastrx updated in tx code? */
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);
	if (TXQACTIVE(txqs, sc->sc_cabq->axq_qnum))
		ath_tx_processq(sc, sc->sc_cabq, 1);
	IF_LOCK(&ifp->if_snd);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	IF_UNLOCK(&ifp->if_snd);
	sc->sc_wd_timer = 0;

	if (sc->sc_softled)
		ath_led_event(sc, sc->sc_txrix);

	ATH_PCU_LOCK(sc);
	sc->sc_txproc_cnt--;
	ATH_PCU_UNLOCK(sc);

	ath_tx_kick(sc);
}

/*
 * Deferred processing of transmit interrupt; special-cased
 * for four hardware queues, 0-3 (e.g. 5212 w/ WME support).
 */
static void
ath_tx_proc_q0123(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	int nacked;
	uint32_t txqs;

	ATH_PCU_LOCK(sc);
	sc->sc_txproc_cnt++;
	txqs = sc->sc_txq_active;
	sc->sc_txq_active &= ~txqs;
	ATH_PCU_UNLOCK(sc);

	ATH_KTR(sc, ATH_KTR_TXCOMP, 1,
	    "ath_tx_proc_q0123: txqs=0x%08x", txqs);

	/*
	 * Process each active queue.
	 */
	nacked = 0;
	if (TXQACTIVE(txqs, 0))
		nacked += ath_tx_processq(sc, &sc->sc_txq[0], 1);
	if (TXQACTIVE(txqs, 1))
		nacked += ath_tx_processq(sc, &sc->sc_txq[1], 1);
	if (TXQACTIVE(txqs, 2))
		nacked += ath_tx_processq(sc, &sc->sc_txq[2], 1);
	if (TXQACTIVE(txqs, 3))
		nacked += ath_tx_processq(sc, &sc->sc_txq[3], 1);
	if (TXQACTIVE(txqs, sc->sc_cabq->axq_qnum))
		ath_tx_processq(sc, sc->sc_cabq, 1);
	if (nacked)
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);

	IF_LOCK(&ifp->if_snd);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	IF_UNLOCK(&ifp->if_snd);
	sc->sc_wd_timer = 0;

	if (sc->sc_softled)
		ath_led_event(sc, sc->sc_txrix);

	ATH_PCU_LOCK(sc);
	sc->sc_txproc_cnt--;
	ATH_PCU_UNLOCK(sc);

	ath_tx_kick(sc);
}

/*
 * Deferred processing of transmit interrupt.
 */
static void
ath_tx_proc(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	int i, nacked;
	uint32_t txqs;

	ATH_PCU_LOCK(sc);
	sc->sc_txproc_cnt++;
	txqs = sc->sc_txq_active;
	sc->sc_txq_active &= ~txqs;
	ATH_PCU_UNLOCK(sc);

	ATH_KTR(sc, ATH_KTR_TXCOMP, 1, "ath_tx_proc: txqs=0x%08x", txqs);

	/*
	 * Process each active queue.
	 */
	nacked = 0;
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i) && TXQACTIVE(txqs, i))
			nacked += ath_tx_processq(sc, &sc->sc_txq[i], 1);
	if (nacked)
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);

	/* XXX check this inside of IF_LOCK? */
	IF_LOCK(&ifp->if_snd);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	IF_UNLOCK(&ifp->if_snd);
	sc->sc_wd_timer = 0;

	if (sc->sc_softled)
		ath_led_event(sc, sc->sc_txrix);

	ATH_PCU_LOCK(sc);
	sc->sc_txproc_cnt--;
	ATH_PCU_UNLOCK(sc);

	ath_tx_kick(sc);
}
#undef	TXQACTIVE

/*
 * Deferred processing of TXQ rescheduling.
 */
static void
ath_txq_sched_tasklet(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	int i;

	/* XXX is skipping ok? */
	ATH_PCU_LOCK(sc);
#if 0
	if (sc->sc_inreset_cnt > 0) {
		device_printf(sc->sc_dev,
		    "%s: sc_inreset_cnt > 0; skipping\n", __func__);
		ATH_PCU_UNLOCK(sc);
		return;
	}
#endif
	sc->sc_txproc_cnt++;
	ATH_PCU_UNLOCK(sc);

	ATH_TX_LOCK(sc);
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		if (ATH_TXQ_SETUP(sc, i)) {
			ath_txq_sched(sc, &sc->sc_txq[i]);
		}
	}
	ATH_TX_UNLOCK(sc);

	ATH_PCU_LOCK(sc);
	sc->sc_txproc_cnt--;
	ATH_PCU_UNLOCK(sc);
}

void
ath_returnbuf_tail(struct ath_softc *sc, struct ath_buf *bf)
{

	ATH_TXBUF_LOCK_ASSERT(sc);

	if (bf->bf_flags & ATH_BUF_MGMT)
		TAILQ_INSERT_TAIL(&sc->sc_txbuf_mgmt, bf, bf_list);
	else {
		TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		sc->sc_txbuf_cnt++;
		if (sc->sc_txbuf_cnt > ath_txbuf) {
			device_printf(sc->sc_dev,
			    "%s: sc_txbuf_cnt > %d?\n",
			    __func__,
			    ath_txbuf);
			sc->sc_txbuf_cnt = ath_txbuf;
		}
	}
}

void
ath_returnbuf_head(struct ath_softc *sc, struct ath_buf *bf)
{

	ATH_TXBUF_LOCK_ASSERT(sc);

	if (bf->bf_flags & ATH_BUF_MGMT)
		TAILQ_INSERT_HEAD(&sc->sc_txbuf_mgmt, bf, bf_list);
	else {
		TAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
		sc->sc_txbuf_cnt++;
		if (sc->sc_txbuf_cnt > ATH_TXBUF) {
			device_printf(sc->sc_dev,
			    "%s: sc_txbuf_cnt > %d?\n",
			    __func__,
			    ATH_TXBUF);
			sc->sc_txbuf_cnt = ATH_TXBUF;
		}
	}
}

/*
 * Free the holding buffer if it exists
 */
void
ath_txq_freeholdingbuf(struct ath_softc *sc, struct ath_txq *txq)
{
	ATH_TXBUF_LOCK_ASSERT(sc);

	if (txq->axq_holdingbf == NULL)
		return;

	txq->axq_holdingbf->bf_flags &= ~ATH_BUF_BUSY;
	ath_returnbuf_tail(sc, txq->axq_holdingbf);
	txq->axq_holdingbf = NULL;
}

/*
 * Add this buffer to the holding queue, freeing the previous
 * one if it exists.
 */
static void
ath_txq_addholdingbuf(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_txq *txq;

	ATH_TXBUF_LOCK_ASSERT(sc);

	/* XXX assert ATH_BUF_BUSY is set */

	/* XXX assert the tx queue is under the max number */
	if (bf->bf_state.bfs_tx_queue > HAL_NUM_TX_QUEUES) {
		device_printf(sc->sc_dev, "%s: bf=%p: invalid tx queue (%d)\n",
		    __func__,
		    bf,
		    bf->bf_state.bfs_tx_queue);
		bf->bf_flags &= ~ATH_BUF_BUSY;
		ath_returnbuf_tail(sc, bf);
		return;
	}
	txq = &sc->sc_txq[bf->bf_state.bfs_tx_queue];
	ath_txq_freeholdingbuf(sc, txq);
	txq->axq_holdingbf = bf;
}

/*
 * Return a buffer to the pool and update the 'busy' flag on the
 * previous 'tail' entry.
 *
 * This _must_ only be called when the buffer is involved in a completed
 * TX. The logic is that if it was part of an active TX, the previous
 * buffer on the list is now not involved in a halted TX DMA queue, waiting
 * for restart (eg for TDMA.)
 *
 * The caller must free the mbuf and recycle the node reference.
 */
void
ath_freebuf(struct ath_softc *sc, struct ath_buf *bf)
{
	KASSERT((bf->bf_node == NULL), ("%s: bf->bf_node != NULL\n", __func__));
	KASSERT((bf->bf_m == NULL), ("%s: bf->bf_m != NULL\n", __func__));

	/*
	 * If this buffer is busy, push it onto the holding queue
	 */
	if (bf->bf_flags & ATH_BUF_BUSY) {
		ATH_TXBUF_LOCK(sc);
		ath_txq_addholdingbuf(sc, bf);
		ATH_TXBUF_UNLOCK(sc);
		return;
	}

	/*
	 * Not a busy buffer, so free normally
	 */
	ATH_TXBUF_LOCK(sc);
	ath_returnbuf_tail(sc, bf);
	ATH_TXBUF_UNLOCK(sc);
}

/*
 * This is currently used by ath_tx_draintxq() and
 * ath_tx_tid_free_pkts().
 *
 * It recycles a single ath_buf.
 */
void
ath_tx_freebuf(struct ath_softc *sc, struct ath_buf *bf, int status)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct mbuf *m0 = bf->bf_m;

	/*
	 * Make sure that we only sync/unload if there's an mbuf.
	 * If not (eg we cloned a buffer), the unload will have already
	 * occured.
	 */
	if (bf->bf_m != NULL) {
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
	}

	bf->bf_node = NULL;
	bf->bf_m = NULL;

	/* Free the buffer, it's not needed any longer */
	ath_freebuf(sc, bf);

	if (ni != NULL) {
		/*
		 * Do any callback and reclaim the node reference.
		 */
		if (m0->m_flags & M_TXCB)
			ieee80211_process_callback(ni, m0, status);
		ieee80211_free_node(ni);
	}

	/* Finally, we don't need this mbuf any longer */
	m_freem(m0);
}

static struct ath_buf *
ath_tx_draintxq_get_one(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_buf *bf;

	ATH_TXQ_LOCK_ASSERT(txq);

	/*
	 * Drain the FIFO queue first, then if it's
	 * empty, move to the normal frame queue.
	 */
	bf = TAILQ_FIRST(&txq->fifo.axq_q);
	if (bf != NULL) {
		/*
		 * Is it the last buffer in this set?
		 * Decrement the FIFO counter.
		 */
		if (bf->bf_flags & ATH_BUF_FIFOEND) {
			if (txq->axq_fifo_depth == 0) {
				device_printf(sc->sc_dev,
				    "%s: Q%d: fifo_depth=0, fifo.axq_depth=%d?\n",
				    __func__,
				    txq->axq_qnum,
				    txq->fifo.axq_depth);
			} else
				txq->axq_fifo_depth--;
		}
		ATH_TXQ_REMOVE(&txq->fifo, bf, bf_list);
		return (bf);
	}

	/*
	 * Debugging!
	 */
	if (txq->axq_fifo_depth != 0 || txq->fifo.axq_depth != 0) {
		device_printf(sc->sc_dev,
		    "%s: Q%d: fifo_depth=%d, fifo.axq_depth=%d\n",
		    __func__,
		    txq->axq_qnum,
		    txq->axq_fifo_depth,
		    txq->fifo.axq_depth);
	}

	/*
	 * Now drain the pending queue.
	 */
	bf = TAILQ_FIRST(&txq->axq_q);
	if (bf == NULL) {
		txq->axq_link = NULL;
		return (NULL);
	}
	ATH_TXQ_REMOVE(txq, bf, bf_list);
	return (bf);
}

void
ath_tx_draintxq(struct ath_softc *sc, struct ath_txq *txq)
{
#ifdef ATH_DEBUG
	struct ath_hal *ah = sc->sc_ah;
#endif
	struct ath_buf *bf;
	u_int ix;

	/*
	 * NB: this assumes output has been stopped and
	 *     we do not need to block ath_tx_proc
	 */
	for (ix = 0;; ix++) {
		ATH_TXQ_LOCK(txq);
		bf = ath_tx_draintxq_get_one(sc, txq);
		if (bf == NULL) {
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		if (bf->bf_state.bfs_aggr)
			txq->axq_aggr_depth--;
#ifdef ATH_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RESET) {
			struct ieee80211com *ic = sc->sc_ifp->if_l2com;
			int status = 0;

			/*
			 * EDMA operation has a TX completion FIFO
			 * separate from the TX descriptor, so this
			 * method of checking the "completion" status
			 * is wrong.
			 */
			if (! sc->sc_isedma) {
				status = (ath_hal_txprocdesc(ah,
				    bf->bf_lastds,
				    &bf->bf_status.ds_txstat) == HAL_OK);
			}
			ath_printtxbuf(sc, bf, txq->axq_qnum, ix, status);
			ieee80211_dump_pkt(ic, mtod(bf->bf_m, const uint8_t *),
			    bf->bf_m->m_len, 0, -1);
		}
#endif /* ATH_DEBUG */
		/*
		 * Since we're now doing magic in the completion
		 * functions, we -must- call it for aggregation
		 * destinations or BAW tracking will get upset.
		 */
		/*
		 * Clear ATH_BUF_BUSY; the completion handler
		 * will free the buffer.
		 */
		ATH_TXQ_UNLOCK(txq);
		bf->bf_flags &= ~ATH_BUF_BUSY;
		if (bf->bf_comp)
			bf->bf_comp(sc, bf, 1);
		else
			ath_tx_default_comp(sc, bf, 1);
	}

	/*
	 * Free the holding buffer if it exists
	 */
	ATH_TXBUF_LOCK(sc);
	ath_txq_freeholdingbuf(sc, txq);
	ATH_TXBUF_UNLOCK(sc);

	/*
	 * Drain software queued frames which are on
	 * active TIDs.
	 */
	ath_tx_txq_drain(sc, txq);
}

static void
ath_tx_stopdma(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_RESET,
	    "%s: tx queue [%u] %p, active=%d, hwpending=%d, flags 0x%08x, link %p\n",
	    __func__,
	    txq->axq_qnum,
	    (caddr_t)(uintptr_t) ath_hal_gettxbuf(ah, txq->axq_qnum),
	    (int) (!! ath_hal_txqenabled(ah, txq->axq_qnum)),
	    (int) ath_hal_numtxpending(ah, txq->axq_qnum),
	    txq->axq_flags,
	    txq->axq_link);
	(void) ath_hal_stoptxdma(ah, txq->axq_qnum);
}

int
ath_stoptxdma(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	int i;

	/* XXX return value */
	if (sc->sc_invalid)
		return 0;

	if (!sc->sc_invalid) {
		/* don't touch the hardware if marked invalid */
		DPRINTF(sc, ATH_DEBUG_RESET, "%s: tx queue [%u] %p, link %p\n",
		    __func__, sc->sc_bhalq,
		    (caddr_t)(uintptr_t) ath_hal_gettxbuf(ah, sc->sc_bhalq),
		    NULL);
		(void) ath_hal_stoptxdma(ah, sc->sc_bhalq);
		for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
			if (ATH_TXQ_SETUP(sc, i))
				ath_tx_stopdma(sc, &sc->sc_txq[i]);
	}

	return 1;
}

#ifdef	ATH_DEBUG
static void
ath_tx_dump(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;
	int i = 0;

	if (! (sc->sc_debug & ATH_DEBUG_RESET))
		return;

	device_printf(sc->sc_dev, "%s: Q%d: begin\n",
	    __func__, txq->axq_qnum);
	TAILQ_FOREACH(bf, &txq->axq_q, bf_list) {
		ath_printtxbuf(sc, bf, txq->axq_qnum, i,
			ath_hal_txprocdesc(ah, bf->bf_lastds,
			    &bf->bf_status.ds_txstat) == HAL_OK);
		i++;
	}
	device_printf(sc->sc_dev, "%s: Q%d: end\n",
	    __func__, txq->axq_qnum);
}
#endif /* ATH_DEBUG */

/*
 * Drain the transmit queues and reclaim resources.
 */
void
ath_legacy_tx_drain(struct ath_softc *sc, ATH_RESET_TYPE reset_type)
{
#ifdef	ATH_DEBUG
	struct ath_hal *ah = sc->sc_ah;
#endif
	struct ifnet *ifp = sc->sc_ifp;
	int i;

	(void) ath_stoptxdma(sc);

	/*
	 * Dump the queue contents
	 */
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		/*
		 * XXX TODO: should we just handle the completed TX frames
		 * here, whether or not the reset is a full one or not?
		 */
		if (ATH_TXQ_SETUP(sc, i)) {
#ifdef	ATH_DEBUG
			if (sc->sc_debug & ATH_DEBUG_RESET)
				ath_tx_dump(sc, &sc->sc_txq[i]);
#endif	/* ATH_DEBUG */
			if (reset_type == ATH_RESET_NOLOSS)
				ath_tx_processq(sc, &sc->sc_txq[i], 0);
			else
				ath_tx_draintxq(sc, &sc->sc_txq[i]);
		}
	}
#ifdef ATH_DEBUG
	if (sc->sc_debug & ATH_DEBUG_RESET) {
		struct ath_buf *bf = TAILQ_FIRST(&sc->sc_bbuf);
		if (bf != NULL && bf->bf_m != NULL) {
			ath_printtxbuf(sc, bf, sc->sc_bhalq, 0,
				ath_hal_txprocdesc(ah, bf->bf_lastds,
				    &bf->bf_status.ds_txstat) == HAL_OK);
			ieee80211_dump_pkt(ifp->if_l2com,
			    mtod(bf->bf_m, const uint8_t *), bf->bf_m->m_len,
			    0, -1);
		}
	}
#endif /* ATH_DEBUG */
	IF_LOCK(&ifp->if_snd);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	IF_UNLOCK(&ifp->if_snd);
	sc->sc_wd_timer = 0;
}

/*
 * Update internal state after a channel change.
 */
static void
ath_chan_change(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	enum ieee80211_phymode mode;

	/*
	 * Change channels and update the h/w rate map
	 * if we're switching; e.g. 11a to 11b/g.
	 */
	mode = ieee80211_chan2mode(chan);
	if (mode != sc->sc_curmode)
		ath_setcurmode(sc, mode);
	sc->sc_curchan = chan;
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
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	int ret = 0;

	/* Treat this as an interface reset */
	ATH_PCU_UNLOCK_ASSERT(sc);
	ATH_UNLOCK_ASSERT(sc);

	/* (Try to) stop TX/RX from occuring */
	taskqueue_block(sc->sc_tq);

	ATH_PCU_LOCK(sc);
	ath_hal_intrset(ah, 0);		/* Stop new RX/TX completion */
	ath_txrx_stop_locked(sc);	/* Stop pending RX/TX completion */
	if (ath_reset_grablock(sc, 1) == 0) {
		device_printf(sc->sc_dev, "%s: concurrent reset! Danger!\n",
		    __func__);
	}
	ATH_PCU_UNLOCK(sc);

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %u (%u MHz, flags 0x%x)\n",
	    __func__, ieee80211_chan2ieee(ic, chan),
	    chan->ic_freq, chan->ic_flags);
	if (chan != sc->sc_curchan) {
		HAL_STATUS status;
		/*
		 * To switch channels clear any pending DMA operations;
		 * wait long enough for the RX fifo to drain, reset the
		 * hardware at the new frequency, and then re-enable
		 * the relevant bits of the h/w.
		 */
#if 0
		ath_hal_intrset(ah, 0);		/* disable interrupts */
#endif
		ath_stoprecv(sc, 1);		/* turn off frame recv */
		/*
		 * First, handle completed TX/RX frames.
		 */
		ath_rx_flush(sc);
		ath_draintxq(sc, ATH_RESET_NOLOSS);
		/*
		 * Next, flush the non-scheduled frames.
		 */
		ath_draintxq(sc, ATH_RESET_FULL);	/* clear pending tx frames */

		ath_update_chainmasks(sc, chan);
		ath_hal_setchainmasks(sc->sc_ah, sc->sc_cur_txchainmask,
		    sc->sc_cur_rxchainmask);
		if (!ath_hal_reset(ah, sc->sc_opmode, chan, AH_TRUE, &status)) {
			if_printf(ifp, "%s: unable to reset "
			    "channel %u (%u MHz, flags 0x%x), hal status %u\n",
			    __func__, ieee80211_chan2ieee(ic, chan),
			    chan->ic_freq, chan->ic_flags, status);
			ret = EIO;
			goto finish;
		}
		sc->sc_diversity = ath_hal_getdiversity(ah);

		/* Let DFS at it in case it's a DFS channel */
		ath_dfs_radar_enable(sc, chan);

		/* Let spectral at in case spectral is enabled */
		ath_spectral_enable(sc, chan);

		/*
		 * Re-enable rx framework.
		 */
		if (ath_startrecv(sc) != 0) {
			if_printf(ifp, "%s: unable to restart recv logic\n",
			    __func__);
			ret = EIO;
			goto finish;
		}

		/*
		 * Change channels and update the h/w rate map
		 * if we're switching; e.g. 11a to 11b/g.
		 */
		ath_chan_change(sc, chan);

		/*
		 * Reset clears the beacon timers; reset them
		 * here if needed.
		 */
		if (sc->sc_beacons) {		/* restart beacons */
#ifdef IEEE80211_SUPPORT_TDMA
			if (sc->sc_tdma)
				ath_tdma_config(sc, NULL);
			else
#endif
			ath_beacon_config(sc, NULL);
		}

		/*
		 * Re-enable interrupts.
		 */
#if 0
		ath_hal_intrset(ah, sc->sc_imask);
#endif
	}

finish:
	ATH_PCU_LOCK(sc);
	sc->sc_inreset_cnt--;
	/* XXX only do this if sc_inreset_cnt == 0? */
	ath_hal_intrset(ah, sc->sc_imask);
	ATH_PCU_UNLOCK(sc);

	IF_LOCK(&ifp->if_snd);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	IF_UNLOCK(&ifp->if_snd);
	ath_txrx_start(sc);
	/* XXX ath_start? */

	return ret;
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
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	HAL_BOOL longCal, isCalDone = AH_TRUE;
	HAL_BOOL aniCal, shortCal = AH_FALSE;
	int nextcal;

	if (ic->ic_flags & IEEE80211_F_SCAN)	/* defer, off channel */
		goto restart;
	longCal = (ticks - sc->sc_lastlongcal >= ath_longcalinterval*hz);
	aniCal = (ticks - sc->sc_lastani >= ath_anicalinterval*hz/1000);
	if (sc->sc_doresetcal)
		shortCal = (ticks - sc->sc_lastshortcal >= ath_shortcalinterval*hz/1000);

	DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: shortCal=%d; longCal=%d; aniCal=%d\n", __func__, shortCal, longCal, aniCal);
	if (aniCal) {
		sc->sc_stats.ast_ani_cal++;
		sc->sc_lastani = ticks;
		ath_hal_ani_poll(ah, sc->sc_curchan);
	}

	if (longCal) {
		sc->sc_stats.ast_per_cal++;
		sc->sc_lastlongcal = ticks;
		if (ath_hal_getrfgain(ah) == HAL_RFGAIN_NEED_CHANGE) {
			/*
			 * Rfgain is out of bounds, reset the chip
			 * to load new gain values.
			 */
			DPRINTF(sc, ATH_DEBUG_CALIBRATE,
				"%s: rfgain change\n", __func__);
			sc->sc_stats.ast_per_rfgain++;
			sc->sc_resetcal = 0;
			sc->sc_doresetcal = AH_TRUE;
			taskqueue_enqueue(sc->sc_tq, &sc->sc_resettask);
			callout_reset(&sc->sc_cal_ch, 1, ath_calibrate, sc);
			return;
		}
		/*
		 * If this long cal is after an idle period, then
		 * reset the data collection state so we start fresh.
		 */
		if (sc->sc_resetcal) {
			(void) ath_hal_calreset(ah, sc->sc_curchan);
			sc->sc_lastcalreset = ticks;
			sc->sc_lastshortcal = ticks;
			sc->sc_resetcal = 0;
			sc->sc_doresetcal = AH_TRUE;
		}
	}

	/* Only call if we're doing a short/long cal, not for ANI calibration */
	if (shortCal || longCal) {
		isCalDone = AH_FALSE;
		if (ath_hal_calibrateN(ah, sc->sc_curchan, longCal, &isCalDone)) {
			if (longCal) {
				/*
				 * Calibrate noise floor data again in case of change.
				 */
				ath_hal_process_noisefloor(ah);
			}
		} else {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: calibration of channel %u failed\n",
				__func__, sc->sc_curchan->ic_freq);
			sc->sc_stats.ast_per_calfail++;
		}
		if (shortCal)
			sc->sc_lastshortcal = ticks;
	}
	if (!isCalDone) {
restart:
		/*
		 * Use a shorter interval to potentially collect multiple
		 * data samples required to complete calibration.  Once
		 * we're told the work is done we drop back to a longer
		 * interval between requests.  We're more aggressive doing
		 * work when operating as an AP to improve operation right
		 * after startup.
		 */
		sc->sc_lastshortcal = ticks;
		nextcal = ath_shortcalinterval*hz/1000;
		if (sc->sc_opmode != HAL_M_HOSTAP)
			nextcal *= 10;
		sc->sc_doresetcal = AH_TRUE;
	} else {
		/* nextcal should be the shortest time for next event */
		nextcal = ath_longcalinterval*hz;
		if (sc->sc_lastcalreset == 0)
			sc->sc_lastcalreset = sc->sc_lastlongcal;
		else if (ticks - sc->sc_lastcalreset >= ath_resetcalinterval*hz)
			sc->sc_resetcal = 1;	/* setup reset next trip */
		sc->sc_doresetcal = AH_FALSE;
	}
	/* ANI calibration may occur more often than short/long/resetcal */
	if (ath_anicalinterval > 0)
		nextcal = MIN(nextcal, ath_anicalinterval*hz/1000);

	if (nextcal != 0) {
		DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: next +%u (%sisCalDone)\n",
		    __func__, nextcal, isCalDone ? "" : "!");
		callout_reset(&sc->sc_cal_ch, nextcal, ath_calibrate, sc);
	} else {
		DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s: calibration disabled\n",
		    __func__);
		/* NB: don't rearm timer */
	}
}

static void
ath_scan_start(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	u_int32_t rfilt;

	/* XXX calibration timer? */

	ATH_LOCK(sc);
	sc->sc_scanning = 1;
	sc->sc_syncbeacon = 0;
	rfilt = ath_calcrxfilter(sc);
	ATH_UNLOCK(sc);

	ATH_PCU_LOCK(sc);
	ath_hal_setrxfilter(ah, rfilt);
	ath_hal_setassocid(ah, ifp->if_broadcastaddr, 0);
	ATH_PCU_UNLOCK(sc);

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %s aid 0\n",
		 __func__, rfilt, ether_sprintf(ifp->if_broadcastaddr));
}

static void
ath_scan_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	u_int32_t rfilt;

	ATH_LOCK(sc);
	sc->sc_scanning = 0;
	rfilt = ath_calcrxfilter(sc);
	ATH_UNLOCK(sc);

	ATH_PCU_LOCK(sc);
	ath_hal_setrxfilter(ah, rfilt);
	ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);

	ath_hal_process_noisefloor(ah);
	ATH_PCU_UNLOCK(sc);

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %s aid 0x%x\n",
		 __func__, rfilt, ether_sprintf(sc->sc_curbssid),
		 sc->sc_curaid);
}

#ifdef	ATH_ENABLE_11N
/*
 * For now, just do a channel change.
 *
 * Later, we'll go through the hard slog of suspending tx/rx, changing rate
 * control state and resetting the hardware without dropping frames out
 * of the queue.
 *
 * The unfortunate trouble here is making absolutely sure that the
 * channel width change has propagated enough so the hardware
 * absolutely isn't handed bogus frames for it's current operating
 * mode. (Eg, 40MHz frames in 20MHz mode.) Since TX and RX can and
 * does occur in parallel, we need to make certain we've blocked
 * any further ongoing TX (and RX, that can cause raw TX)
 * before we do this.
 */
static void
ath_update_chw(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: called\n", __func__);
	ath_set_channel(ic);
}
#endif	/* ATH_ENABLE_11N */

static void
ath_set_channel(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;

	(void) ath_chan_set(sc, ic->ic_curchan);
	/*
	 * If we are returning to our bss channel then mark state
	 * so the next recv'd beacon's tsf will be used to sync the
	 * beacon timers.  Note that since we only hear beacons in
	 * sta/ibss mode this has no effect in other operating modes.
	 */
	ATH_LOCK(sc);
	if (!sc->sc_scanning && ic->ic_curchan == ic->ic_bsschan)
		sc->sc_syncbeacon = 1;
	ATH_UNLOCK(sc);
}

/*
 * Walk the vap list and check if there any vap's in RUN state.
 */
static int
ath_isanyrunningvaps(struct ieee80211vap *this)
{
	struct ieee80211com *ic = this->iv_ic;
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap != this && vap->iv_state >= IEEE80211_S_RUN)
			return 1;
	}
	return 0;
}

static int
ath_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_vap *avp = ATH_VAP(vap);
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211_node *ni = NULL;
	int i, error, stamode;
	u_int32_t rfilt;
	int csa_run_transition = 0;

	static const HAL_LED_STATE leds[] = {
	    HAL_LED_INIT,	/* IEEE80211_S_INIT */
	    HAL_LED_SCAN,	/* IEEE80211_S_SCAN */
	    HAL_LED_AUTH,	/* IEEE80211_S_AUTH */
	    HAL_LED_ASSOC, 	/* IEEE80211_S_ASSOC */
	    HAL_LED_RUN, 	/* IEEE80211_S_CAC */
	    HAL_LED_RUN, 	/* IEEE80211_S_RUN */
	    HAL_LED_RUN, 	/* IEEE80211_S_CSA */
	    HAL_LED_RUN, 	/* IEEE80211_S_SLEEP */
	};

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[vap->iv_state],
		ieee80211_state_name[nstate]);

	/*
	 * net80211 _should_ have the comlock asserted at this point.
	 * There are some comments around the calls to vap->iv_newstate
	 * which indicate that it (newstate) may end up dropping the
	 * lock.  This and the subsequent lock assert check after newstate
	 * are an attempt to catch these and figure out how/why.
	 */
	IEEE80211_LOCK_ASSERT(ic);

	if (vap->iv_state == IEEE80211_S_CSA && nstate == IEEE80211_S_RUN)
		csa_run_transition = 1;

	callout_drain(&sc->sc_cal_ch);
	ath_hal_setledstate(ah, leds[nstate]);	/* set LED */

	if (nstate == IEEE80211_S_SCAN) {
		/*
		 * Scanning: turn off beacon miss and don't beacon.
		 * Mark beacon state so when we reach RUN state we'll
		 * [re]setup beacons.  Unblock the task q thread so
		 * deferred interrupt processing is done.
		 */
		ath_hal_intrset(ah,
		    sc->sc_imask &~ (HAL_INT_SWBA | HAL_INT_BMISS));
		sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
		sc->sc_beacons = 0;
		taskqueue_unblock(sc->sc_tq);
	}

	ni = ieee80211_ref_node(vap->iv_bss);
	rfilt = ath_calcrxfilter(sc);
	stamode = (vap->iv_opmode == IEEE80211_M_STA ||
		   vap->iv_opmode == IEEE80211_M_AHDEMO ||
		   vap->iv_opmode == IEEE80211_M_IBSS);
	if (stamode && nstate == IEEE80211_S_RUN) {
		sc->sc_curaid = ni->ni_associd;
		IEEE80211_ADDR_COPY(sc->sc_curbssid, ni->ni_bssid);
		ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);
	}
	DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %s aid 0x%x\n",
	   __func__, rfilt, ether_sprintf(sc->sc_curbssid), sc->sc_curaid);
	ath_hal_setrxfilter(ah, rfilt);

	/* XXX is this to restore keycache on resume? */
	if (vap->iv_opmode != IEEE80211_M_STA &&
	    (vap->iv_flags & IEEE80211_F_PRIVACY)) {
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			if (ath_hal_keyisvalid(ah, i))
				ath_hal_keysetmac(ah, i, ni->ni_bssid);
	}

	/*
	 * Invoke the parent method to do net80211 work.
	 */
	error = avp->av_newstate(vap, nstate, arg);
	if (error != 0)
		goto bad;

	/*
	 * See above: ensure av_newstate() doesn't drop the lock
	 * on us.
	 */
	IEEE80211_LOCK_ASSERT(ic);

	if (nstate == IEEE80211_S_RUN) {
		/* NB: collect bss node again, it may have changed */
		ieee80211_free_node(ni);
		ni = ieee80211_ref_node(vap->iv_bss);

		DPRINTF(sc, ATH_DEBUG_STATE,
		    "%s(RUN): iv_flags 0x%08x bintvl %d bssid %s "
		    "capinfo 0x%04x chan %d\n", __func__,
		    vap->iv_flags, ni->ni_intval, ether_sprintf(ni->ni_bssid),
		    ni->ni_capinfo, ieee80211_chan2ieee(ic, ic->ic_curchan));

		switch (vap->iv_opmode) {
#ifdef IEEE80211_SUPPORT_TDMA
		case IEEE80211_M_AHDEMO:
			if ((vap->iv_caps & IEEE80211_C_TDMA) == 0)
				break;
			/* fall thru... */
#endif
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_IBSS:
		case IEEE80211_M_MBSS:
			/*
			 * Allocate and setup the beacon frame.
			 *
			 * Stop any previous beacon DMA.  This may be
			 * necessary, for example, when an ibss merge
			 * causes reconfiguration; there will be a state
			 * transition from RUN->RUN that means we may
			 * be called with beacon transmission active.
			 */
			ath_hal_stoptxdma(ah, sc->sc_bhalq);

			error = ath_beacon_alloc(sc, ni);
			if (error != 0)
				goto bad;
			/*
			 * If joining an adhoc network defer beacon timer
			 * configuration to the next beacon frame so we
			 * have a current TSF to use.  Otherwise we're
			 * starting an ibss/bss so there's no need to delay;
			 * if this is the first vap moving to RUN state, then
			 * beacon state needs to be [re]configured.
			 */
			if (vap->iv_opmode == IEEE80211_M_IBSS &&
			    ni->ni_tstamp.tsf != 0) {
				sc->sc_syncbeacon = 1;
			} else if (!sc->sc_beacons) {
#ifdef IEEE80211_SUPPORT_TDMA
				if (vap->iv_caps & IEEE80211_C_TDMA)
					ath_tdma_config(sc, vap);
				else
#endif
					ath_beacon_config(sc, vap);
				sc->sc_beacons = 1;
			}
			break;
		case IEEE80211_M_STA:
			/*
			 * Defer beacon timer configuration to the next
			 * beacon frame so we have a current TSF to use
			 * (any TSF collected when scanning is likely old).
			 * However if it's due to a CSA -> RUN transition,
			 * force a beacon update so we pick up a lack of
			 * beacons from an AP in CAC and thus force a
			 * scan.
			 *
			 * And, there's also corner cases here where
			 * after a scan, the AP may have disappeared.
			 * In that case, we may not receive an actual
			 * beacon to update the beacon timer and thus we
			 * won't get notified of the missing beacons.
			 */
			sc->sc_syncbeacon = 1;
#if 0
			if (csa_run_transition)
#endif
				ath_beacon_config(sc, vap);

			/*
			 * PR: kern/175227
			 *
			 * Reconfigure beacons during reset; as otherwise
			 * we won't get the beacon timers reprogrammed
			 * after a reset and thus we won't pick up a
			 * beacon miss interrupt.
			 *
			 * Hopefully we'll see a beacon before the BMISS
			 * timer fires (too often), leading to a STA
			 * disassociation.
			 */
			sc->sc_beacons = 1;
			break;
		case IEEE80211_M_MONITOR:
			/*
			 * Monitor mode vaps have only INIT->RUN and RUN->RUN
			 * transitions so we must re-enable interrupts here to
			 * handle the case of a single monitor mode vap.
			 */
			ath_hal_intrset(ah, sc->sc_imask);
			break;
		case IEEE80211_M_WDS:
			break;
		default:
			break;
		}
		/*
		 * Let the hal process statistics collected during a
		 * scan so it can provide calibrated noise floor data.
		 */
		ath_hal_process_noisefloor(ah);
		/*
		 * Reset rssi stats; maybe not the best place...
		 */
		sc->sc_halstats.ns_avgbrssi = ATH_RSSI_DUMMY_MARKER;
		sc->sc_halstats.ns_avgrssi = ATH_RSSI_DUMMY_MARKER;
		sc->sc_halstats.ns_avgtxrssi = ATH_RSSI_DUMMY_MARKER;
		/*
		 * Finally, start any timers and the task q thread
		 * (in case we didn't go through SCAN state).
		 */
		if (ath_longcalinterval != 0) {
			/* start periodic recalibration timer */
			callout_reset(&sc->sc_cal_ch, 1, ath_calibrate, sc);
		} else {
			DPRINTF(sc, ATH_DEBUG_CALIBRATE,
			    "%s: calibration disabled\n", __func__);
		}
		taskqueue_unblock(sc->sc_tq);
	} else if (nstate == IEEE80211_S_INIT) {
		/*
		 * If there are no vaps left in RUN state then
		 * shutdown host/driver operation:
		 * o disable interrupts
		 * o disable the task queue thread
		 * o mark beacon processing as stopped
		 */
		if (!ath_isanyrunningvaps(vap)) {
			sc->sc_imask &= ~(HAL_INT_SWBA | HAL_INT_BMISS);
			/* disable interrupts  */
			ath_hal_intrset(ah, sc->sc_imask &~ HAL_INT_GLOBAL);
			taskqueue_block(sc->sc_tq);
			sc->sc_beacons = 0;
		}
#ifdef IEEE80211_SUPPORT_TDMA
		ath_hal_setcca(ah, AH_TRUE);
#endif
	}
bad:
	ieee80211_free_node(ni);
	return error;
}

/*
 * Allocate a key cache slot to the station so we can
 * setup a mapping from key index to node. The key cache
 * slot is needed for managing antenna state and for
 * compression when stations do not use crypto.  We do
 * it uniliaterally here; if crypto is employed this slot
 * will be reassigned.
 */
static void
ath_setup_stationkey(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ath_softc *sc = vap->iv_ic->ic_ifp->if_softc;
	ieee80211_keyix keyix, rxkeyix;

	/* XXX should take a locked ref to vap->iv_bss */
	if (!ath_key_alloc(vap, &ni->ni_ucastkey, &keyix, &rxkeyix)) {
		/*
		 * Key cache is full; we'll fall back to doing
		 * the more expensive lookup in software.  Note
		 * this also means no h/w compression.
		 */
		/* XXX msg+statistic */
	} else {
		/* XXX locking? */
		ni->ni_ucastkey.wk_keyix = keyix;
		ni->ni_ucastkey.wk_rxkeyix = rxkeyix;
		/* NB: must mark device key to get called back on delete */
		ni->ni_ucastkey.wk_flags |= IEEE80211_KEY_DEVKEY;
		IEEE80211_ADDR_COPY(ni->ni_ucastkey.wk_macaddr, ni->ni_macaddr);
		/* NB: this will create a pass-thru key entry */
		ath_keyset(sc, vap, &ni->ni_ucastkey, vap->iv_bss);
	}
}

/*
 * Setup driver-specific state for a newly associated node.
 * Note that we're called also on a re-associate, the isnew
 * param tells us if this is the first time or not.
 */
static void
ath_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ath_node *an = ATH_NODE(ni);
	struct ieee80211vap *vap = ni->ni_vap;
	struct ath_softc *sc = vap->iv_ic->ic_ifp->if_softc;
	const struct ieee80211_txparam *tp = ni->ni_txparms;

	an->an_mcastrix = ath_tx_findrix(sc, tp->mcastrate);
	an->an_mgmtrix = ath_tx_findrix(sc, tp->mgmtrate);

	ath_rate_newassoc(sc, an, isnew);
	if (isnew &&
	    (vap->iv_flags & IEEE80211_F_PRIVACY) == 0 && sc->sc_hasclrkey &&
	    ni->ni_ucastkey.wk_keyix == IEEE80211_KEYIX_NONE)
		ath_setup_stationkey(ni);
}

static int
ath_setregdomain(struct ieee80211com *ic, struct ieee80211_regdomain *reg,
	int nchans, struct ieee80211_channel chans[])
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_REGDOMAIN,
	    "%s: rd %u cc %u location %c%s\n",
	    __func__, reg->regdomain, reg->country, reg->location,
	    reg->ecm ? " ecm" : "");

	status = ath_hal_set_channels(ah, chans, nchans,
	    reg->country, reg->regdomain);
	if (status != HAL_OK) {
		DPRINTF(sc, ATH_DEBUG_REGDOMAIN, "%s: failed, status %u\n",
		    __func__, status);
		return EINVAL;		/* XXX */
	}

	return 0;
}

static void
ath_getradiocaps(struct ieee80211com *ic,
	int maxchans, int *nchans, struct ieee80211_channel chans[])
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_REGDOMAIN, "%s: use rd %u cc %d\n",
	    __func__, SKU_DEBUG, CTRY_DEFAULT);

	/* XXX check return */
	(void) ath_hal_getchannels(ah, chans, maxchans, nchans,
	    HAL_MODE_ALL, CTRY_DEFAULT, SKU_DEBUG, AH_TRUE);

}

static int
ath_getchannels(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;

	/*
	 * Collect channel set based on EEPROM contents.
	 */
	status = ath_hal_init_channels(ah, ic->ic_channels, IEEE80211_CHAN_MAX,
	    &ic->ic_nchans, HAL_MODE_ALL, CTRY_DEFAULT, SKU_NONE, AH_TRUE);
	if (status != HAL_OK) {
		if_printf(ifp, "%s: unable to collect channel list from hal, "
		    "status %d\n", __func__, status);
		return EINVAL;
	}
	(void) ath_hal_getregdomain(ah, &sc->sc_eerd);
	ath_hal_getcountrycode(ah, &sc->sc_eecc);	/* NB: cannot fail */
	/* XXX map Atheros sku's to net80211 SKU's */
	/* XXX net80211 types too small */
	ic->ic_regdomain.regdomain = (uint16_t) sc->sc_eerd;
	ic->ic_regdomain.country = (uint16_t) sc->sc_eecc;
	ic->ic_regdomain.isocc[0] = ' ';	/* XXX don't know */
	ic->ic_regdomain.isocc[1] = ' ';

	ic->ic_regdomain.ecm = 1;
	ic->ic_regdomain.location = 'I';

	DPRINTF(sc, ATH_DEBUG_REGDOMAIN,
	    "%s: eeprom rd %u cc %u (mapped rd %u cc %u) location %c%s\n",
	    __func__, sc->sc_eerd, sc->sc_eecc,
	    ic->ic_regdomain.regdomain, ic->ic_regdomain.country,
	    ic->ic_regdomain.location, ic->ic_regdomain.ecm ? " ecm" : "");
	return 0;
}

static int
ath_rate_setup(struct ath_softc *sc, u_int mode)
{
	struct ath_hal *ah = sc->sc_ah;
	const HAL_RATE_TABLE *rt;

	switch (mode) {
	case IEEE80211_MODE_11A:
		rt = ath_hal_getratetable(ah, HAL_MODE_11A);
		break;
	case IEEE80211_MODE_HALF:
		rt = ath_hal_getratetable(ah, HAL_MODE_11A_HALF_RATE);
		break;
	case IEEE80211_MODE_QUARTER:
		rt = ath_hal_getratetable(ah, HAL_MODE_11A_QUARTER_RATE);
		break;
	case IEEE80211_MODE_11B:
		rt = ath_hal_getratetable(ah, HAL_MODE_11B);
		break;
	case IEEE80211_MODE_11G:
		rt = ath_hal_getratetable(ah, HAL_MODE_11G);
		break;
	case IEEE80211_MODE_TURBO_A:
		rt = ath_hal_getratetable(ah, HAL_MODE_108A);
		break;
	case IEEE80211_MODE_TURBO_G:
		rt = ath_hal_getratetable(ah, HAL_MODE_108G);
		break;
	case IEEE80211_MODE_STURBO_A:
		rt = ath_hal_getratetable(ah, HAL_MODE_TURBO);
		break;
	case IEEE80211_MODE_11NA:
		rt = ath_hal_getratetable(ah, HAL_MODE_11NA_HT20);
		break;
	case IEEE80211_MODE_11NG:
		rt = ath_hal_getratetable(ah, HAL_MODE_11NG_HT20);
		break;
	default:
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid mode %u\n",
			__func__, mode);
		return 0;
	}
	sc->sc_rates[mode] = rt;
	return (rt != NULL);
}

static void
ath_setcurmode(struct ath_softc *sc, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	/* NB: on/off times from the Atheros NDIS driver, w/ permission */
	static const struct {
		u_int		rate;		/* tx/rx 802.11 rate */
		u_int16_t	timeOn;		/* LED on time (ms) */
		u_int16_t	timeOff;	/* LED off time (ms) */
	} blinkrates[] = {
		{ 108,  40,  10 },
		{  96,  44,  11 },
		{  72,  50,  13 },
		{  48,  57,  14 },
		{  36,  67,  16 },
		{  24,  80,  20 },
		{  22, 100,  25 },
		{  18, 133,  34 },
		{  12, 160,  40 },
		{  10, 200,  50 },
		{   6, 240,  58 },
		{   4, 267,  66 },
		{   2, 400, 100 },
		{   0, 500, 130 },
		/* XXX half/quarter rates */
	};
	const HAL_RATE_TABLE *rt;
	int i, j;

	memset(sc->sc_rixmap, 0xff, sizeof(sc->sc_rixmap));
	rt = sc->sc_rates[mode];
	KASSERT(rt != NULL, ("no h/w rate set for phy mode %u", mode));
	for (i = 0; i < rt->rateCount; i++) {
		uint8_t ieeerate = rt->info[i].dot11Rate & IEEE80211_RATE_VAL;
		if (rt->info[i].phy != IEEE80211_T_HT)
			sc->sc_rixmap[ieeerate] = i;
		else
			sc->sc_rixmap[ieeerate | IEEE80211_RATE_MCS] = i;
	}
	memset(sc->sc_hwmap, 0, sizeof(sc->sc_hwmap));
	for (i = 0; i < N(sc->sc_hwmap); i++) {
		if (i >= rt->rateCount) {
			sc->sc_hwmap[i].ledon = (500 * hz) / 1000;
			sc->sc_hwmap[i].ledoff = (130 * hz) / 1000;
			continue;
		}
		sc->sc_hwmap[i].ieeerate =
			rt->info[i].dot11Rate & IEEE80211_RATE_VAL;
		if (rt->info[i].phy == IEEE80211_T_HT)
			sc->sc_hwmap[i].ieeerate |= IEEE80211_RATE_MCS;
		sc->sc_hwmap[i].txflags = IEEE80211_RADIOTAP_F_DATAPAD;
		if (rt->info[i].shortPreamble ||
		    rt->info[i].phy == IEEE80211_T_OFDM)
			sc->sc_hwmap[i].txflags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		sc->sc_hwmap[i].rxflags = sc->sc_hwmap[i].txflags;
		for (j = 0; j < N(blinkrates)-1; j++)
			if (blinkrates[j].rate == sc->sc_hwmap[i].ieeerate)
				break;
		/* NB: this uses the last entry if the rate isn't found */
		/* XXX beware of overlow */
		sc->sc_hwmap[i].ledon = (blinkrates[j].timeOn * hz) / 1000;
		sc->sc_hwmap[i].ledoff = (blinkrates[j].timeOff * hz) / 1000;
	}
	sc->sc_currates = rt;
	sc->sc_curmode = mode;
	/*
	 * All protection frames are transmited at 2Mb/s for
	 * 11g, otherwise at 1Mb/s.
	 */
	if (mode == IEEE80211_MODE_11G)
		sc->sc_protrix = ath_tx_findrix(sc, 2*2);
	else
		sc->sc_protrix = ath_tx_findrix(sc, 2*1);
	/* NB: caller is responsible for resetting rate control state */
#undef N
}

static void
ath_watchdog(void *arg)
{
	struct ath_softc *sc = arg;
	int do_reset = 0;

	if (sc->sc_wd_timer != 0 && --sc->sc_wd_timer == 0) {
		struct ifnet *ifp = sc->sc_ifp;
		uint32_t hangs;

		if (ath_hal_gethangstate(sc->sc_ah, 0xffff, &hangs) &&
		    hangs != 0) {
			if_printf(ifp, "%s hang detected (0x%x)\n",
			    hangs & 0xff ? "bb" : "mac", hangs);
		} else
			if_printf(ifp, "device timeout\n");
		do_reset = 1;
		ifp->if_oerrors++;
		sc->sc_stats.ast_watchdog++;
	}

	/*
	 * We can't hold the lock across the ath_reset() call.
	 *
	 * And since this routine can't hold a lock and sleep,
	 * do the reset deferred.
	 */
	if (do_reset) {
		taskqueue_enqueue(sc->sc_tq, &sc->sc_resettask);
	}

	callout_schedule(&sc->sc_wd_ch, hz);
}

/*
 * Fetch the rate control statistics for the given node.
 */
static int
ath_ioctl_ratestats(struct ath_softc *sc, struct ath_rateioctl *rs)
{
	struct ath_node *an;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211_node *ni;
	int error = 0;

	/* Perform a lookup on the given node */
	ni = ieee80211_find_node(&ic->ic_sta, rs->is_u.macaddr);
	if (ni == NULL) {
		error = EINVAL;
		goto bad;
	}

	/* Lock the ath_node */
	an = ATH_NODE(ni);
	ATH_NODE_LOCK(an);

	/* Fetch the rate control stats for this node */
	error = ath_rate_fetch_node_stats(sc, an, rs);

	/* No matter what happens here, just drop through */

	/* Unlock the ath_node */
	ATH_NODE_UNLOCK(an);

	/* Unref the node */
	ieee80211_node_decref(ni);

bad:
	return (error);
}

#ifdef ATH_DIAGAPI
/*
 * Diagnostic interface to the HAL.  This is used by various
 * tools to do things like retrieve register contents for
 * debugging.  The mechanism is intentionally opaque so that
 * it can change frequently w/o concern for compatiblity.
 */
static int
ath_ioctl_diag(struct ath_softc *sc, struct ath_diag *ad)
{
	struct ath_hal *ah = sc->sc_ah;
	u_int id = ad->ad_id & ATH_DIAG_ID;
	void *indata = NULL;
	void *outdata = NULL;
	u_int32_t insize = ad->ad_in_size;
	u_int32_t outsize = ad->ad_out_size;
	int error = 0;

	if (ad->ad_id & ATH_DIAG_IN) {
		/*
		 * Copy in data.
		 */
		indata = malloc(insize, M_TEMP, M_NOWAIT);
		if (indata == NULL) {
			error = ENOMEM;
			goto bad;
		}
		error = copyin(ad->ad_in_data, indata, insize);
		if (error)
			goto bad;
	}
	if (ad->ad_id & ATH_DIAG_DYN) {
		/*
		 * Allocate a buffer for the results (otherwise the HAL
		 * returns a pointer to a buffer where we can read the
		 * results).  Note that we depend on the HAL leaving this
		 * pointer for us to use below in reclaiming the buffer;
		 * may want to be more defensive.
		 */
		outdata = malloc(outsize, M_TEMP, M_NOWAIT);
		if (outdata == NULL) {
			error = ENOMEM;
			goto bad;
		}
	}
	if (ath_hal_getdiagstate(ah, id, indata, insize, &outdata, &outsize)) {
		if (outsize < ad->ad_out_size)
			ad->ad_out_size = outsize;
		if (outdata != NULL)
			error = copyout(outdata, ad->ad_out_data,
					ad->ad_out_size);
	} else {
		error = EINVAL;
	}
bad:
	if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
		free(indata, M_TEMP);
	if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
		free(outdata, M_TEMP);
	return error;
}
#endif /* ATH_DIAGAPI */

static int
ath_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
#define	IS_RUNNING(ifp) \
	((ifp->if_flags & IFF_UP) && (ifp->if_drv_flags & IFF_DRV_RUNNING))
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *)data;
	const HAL_RATE_TABLE *rt;
	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		ATH_LOCK(sc);
		if (IS_RUNNING(ifp)) {
			/*
			 * To avoid rescanning another access point,
			 * do not call ath_init() here.  Instead,
			 * only reflect promisc mode settings.
			 */
			ath_mode_init(sc);
		} else if (ifp->if_flags & IFF_UP) {
			/*
			 * Beware of being called during attach/detach
			 * to reset promiscuous mode.  In that case we
			 * will still be marked UP but not RUNNING.
			 * However trying to re-init the interface
			 * is the wrong thing to do as we've already
			 * torn down much of our state.  There's
			 * probably a better way to deal with this.
			 */
			if (!sc->sc_invalid)
				ath_init(sc);	/* XXX lose error */
		} else {
			ath_stop_locked(ifp);
#ifdef notyet
			/* XXX must wakeup in places like ath_vap_delete */
			if (!sc->sc_invalid)
				ath_hal_setpower(sc->sc_ah, HAL_PM_FULL_SLEEP);
#endif
		}
		ATH_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	case SIOCGATHSTATS:
		/* NB: embed these numbers to get a consistent view */
		sc->sc_stats.ast_tx_packets = ifp->if_opackets;
		sc->sc_stats.ast_rx_packets = ifp->if_ipackets;
		sc->sc_stats.ast_tx_rssi = ATH_RSSI(sc->sc_halstats.ns_avgtxrssi);
		sc->sc_stats.ast_rx_rssi = ATH_RSSI(sc->sc_halstats.ns_avgrssi);
#ifdef IEEE80211_SUPPORT_TDMA
		sc->sc_stats.ast_tdma_tsfadjp = TDMA_AVG(sc->sc_avgtsfdeltap);
		sc->sc_stats.ast_tdma_tsfadjm = TDMA_AVG(sc->sc_avgtsfdeltam);
#endif
		rt = sc->sc_currates;
		sc->sc_stats.ast_tx_rate =
		    rt->info[sc->sc_txrix].dot11Rate &~ IEEE80211_RATE_BASIC;
		if (rt->info[sc->sc_txrix].phy & IEEE80211_T_HT)
			sc->sc_stats.ast_tx_rate |= IEEE80211_RATE_MCS;
		return copyout(&sc->sc_stats,
		    ifr->ifr_data, sizeof (sc->sc_stats));
	case SIOCGATHAGSTATS:
		return copyout(&sc->sc_aggr_stats,
		    ifr->ifr_data, sizeof (sc->sc_aggr_stats));
	case SIOCZATHSTATS:
		error = priv_check(curthread, PRIV_DRIVER);
		if (error == 0) {
			memset(&sc->sc_stats, 0, sizeof(sc->sc_stats));
			memset(&sc->sc_aggr_stats, 0,
			    sizeof(sc->sc_aggr_stats));
			memset(&sc->sc_intr_stats, 0,
			    sizeof(sc->sc_intr_stats));
		}
		break;
#ifdef ATH_DIAGAPI
	case SIOCGATHDIAG:
		error = ath_ioctl_diag(sc, (struct ath_diag *) ifr);
		break;
	case SIOCGATHPHYERR:
		error = ath_ioctl_phyerr(sc,(struct ath_diag*) ifr);
		break;
#endif
	case SIOCGATHSPECTRAL:
		error = ath_ioctl_spectral(sc,(struct ath_diag*) ifr);
		break;
	case SIOCGATHNODERATESTATS:
		error = ath_ioctl_ratestats(sc, (struct ath_rateioctl *) ifr);
		break;
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, cmd, data);
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
#undef IS_RUNNING
}

/*
 * Announce various information on device/driver attach.
 */
static void
ath_announce(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ath_hal *ah = sc->sc_ah;

	if_printf(ifp, "AR%s mac %d.%d RF%s phy %d.%d\n",
		ath_hal_mac_name(ah), ah->ah_macVersion, ah->ah_macRev,
		ath_hal_rf_name(ah), ah->ah_phyRev >> 4, ah->ah_phyRev & 0xf);
	if_printf(ifp, "2GHz radio: 0x%.4x; 5GHz radio: 0x%.4x\n",
		ah->ah_analog2GhzRev, ah->ah_analog5GhzRev);
	if (bootverbose) {
		int i;
		for (i = 0; i <= WME_AC_VO; i++) {
			struct ath_txq *txq = sc->sc_ac2q[i];
			if_printf(ifp, "Use hw queue %u for %s traffic\n",
				txq->axq_qnum, ieee80211_wme_acnames[i]);
		}
		if_printf(ifp, "Use hw queue %u for CAB traffic\n",
			sc->sc_cabq->axq_qnum);
		if_printf(ifp, "Use hw queue %u for beacons\n", sc->sc_bhalq);
	}
	if (ath_rxbuf != ATH_RXBUF)
		if_printf(ifp, "using %u rx buffers\n", ath_rxbuf);
	if (ath_txbuf != ATH_TXBUF)
		if_printf(ifp, "using %u tx buffers\n", ath_txbuf);
	if (sc->sc_mcastkey && bootverbose)
		if_printf(ifp, "using multicast key search\n");
}

static void
ath_dfs_tasklet(void *p, int npending)
{
	struct ath_softc *sc = (struct ath_softc *) p;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	/*
	 * If previous processing has found a radar event,
	 * signal this to the net80211 layer to begin DFS
	 * processing.
	 */
	if (ath_dfs_process_radar_event(sc, sc->sc_curchan)) {
		/* DFS event found, initiate channel change */
		/*
		 * XXX doesn't currently tell us whether the event
		 * XXX was found in the primary or extension
		 * XXX channel!
		 */
		IEEE80211_LOCK(ic);
		ieee80211_dfs_notify_radar(ic, sc->sc_curchan);
		IEEE80211_UNLOCK(ic);
	}
}

/*
 * Enable/disable power save.  This must be called with
 * no TX driver locks currently held, so it should only
 * be called from the RX path (which doesn't hold any
 * TX driver locks.)
 */
static void
ath_node_powersave(struct ieee80211_node *ni, int enable)
{
#ifdef	ATH_SW_PSQ
	struct ath_node *an = ATH_NODE(ni);
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_vap *avp = ATH_VAP(ni->ni_vap);

	ATH_NODE_UNLOCK_ASSERT(an);
	/* XXX and no TXQ locks should be held here */

	DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE, "%s: ni=%p, enable=%d\n",
	    __func__, ni, enable);

	/* Suspend or resume software queue handling */
	if (enable)
		ath_tx_node_sleep(sc, an);
	else
		ath_tx_node_wakeup(sc, an);

	/* Update net80211 state */
	avp->av_node_ps(ni, enable);
#else
	struct ath_vap *avp = ATH_VAP(ni->ni_vap);

	/* Update net80211 state */
	avp->av_node_ps(ni, enable);
#endif/* ATH_SW_PSQ */
}

/*
 * Notification from net80211 that the powersave queue state has
 * changed.
 *
 * Since the software queue also may have some frames:
 *
 * + if the node software queue has frames and the TID state
 *   is 0, we set the TIM;
 * + if the node and the stack are both empty, we clear the TIM bit.
 * + If the stack tries to set the bit, always set it.
 * + If the stack tries to clear the bit, only clear it if the
 *   software queue in question is also cleared.
 *
 * TODO: this is called during node teardown; so let's ensure this
 * is all correctly handled and that the TIM bit is cleared.
 * It may be that the node flush is called _AFTER_ the net80211
 * stack clears the TIM.
 *
 * Here is the racy part.  Since it's possible >1 concurrent,
 * overlapping TXes will appear complete with a TX completion in
 * another thread, it's possible that the concurrent TIM calls will
 * clash.  We can't hold the node lock here because setting the
 * TIM grabs the net80211 comlock and this may cause a LOR.
 * The solution is either to totally serialise _everything_ at
 * this point (ie, all TX, completion and any reset/flush go into
 * one taskqueue) or a new "ath TIM lock" needs to be created that
 * just wraps the driver state change and this call to avp->av_set_tim().
 *
 * The same race exists in the net80211 power save queue handling
 * as well.  Since multiple transmitting threads may queue frames
 * into the driver, as well as ps-poll and the driver transmitting
 * frames (and thus clearing the psq), it's quite possible that
 * a packet entering the PSQ and a ps-poll being handled will
 * race, causing the TIM to be cleared and not re-set.
 */
static int
ath_node_set_tim(struct ieee80211_node *ni, int enable)
{
#ifdef	ATH_SW_PSQ
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_node *an = ATH_NODE(ni);
	struct ath_vap *avp = ATH_VAP(ni->ni_vap);
	int changed = 0;

	ATH_NODE_UNLOCK_ASSERT(an);

	/*
	 * For now, just track and then update the TIM.
	 */
	ATH_NODE_LOCK(an);
	an->an_stack_psq = enable;

	/*
	 * This will get called for all operating modes,
	 * even if avp->av_set_tim is unset.
	 * It's currently set for hostap/ibss modes; but
	 * the same infrastructure is used for both STA
	 * and AP/IBSS node power save.
	 */
	if (avp->av_set_tim == NULL) {
		ATH_NODE_UNLOCK(an);
		return (0);
	}

	/*
	 * If setting the bit, always set it here.
	 * If clearing the bit, only clear it if the
	 * software queue is also empty.
	 *
	 * If the node has left power save, just clear the TIM
	 * bit regardless of the state of the power save queue.
	 *
	 * XXX TODO: although atomics are used, it's quite possible
	 * that a race will occur between this and setting/clearing
	 * in another thread.  TX completion will occur always in
	 * one thread, however setting/clearing the TIM bit can come
	 * from a variety of different process contexts!
	 */
	if (enable && an->an_tim_set == 1) {
		DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE,
		    "%s: an=%p, enable=%d, tim_set=1, ignoring\n",
		    __func__, an, enable);
		ATH_NODE_UNLOCK(an);
	} else if (enable) {
		DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE,
		    "%s: an=%p, enable=%d, enabling TIM\n",
		    __func__, an, enable);
		an->an_tim_set = 1;
		ATH_NODE_UNLOCK(an);
		changed = avp->av_set_tim(ni, enable);
	} else if (atomic_load_acq_int(&an->an_swq_depth) == 0) {
		/* disable */
		DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE,
		    "%s: an=%p, enable=%d, an_swq_depth == 0, disabling\n",
		    __func__, an, enable);
		an->an_tim_set = 0;
		ATH_NODE_UNLOCK(an);
		changed = avp->av_set_tim(ni, enable);
	} else if (! an->an_is_powersave) {
		/*
		 * disable regardless; the node isn't in powersave now
		 */
		DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE,
		    "%s: an=%p, enable=%d, an_pwrsave=0, disabling\n",
		    __func__, an, enable);
		an->an_tim_set = 0;
		ATH_NODE_UNLOCK(an);
		changed = avp->av_set_tim(ni, enable);
	} else {
		/*
		 * psq disable, node is currently in powersave, node
		 * software queue isn't empty, so don't clear the TIM bit
		 * for now.
		 */
		ATH_NODE_UNLOCK(an);
		DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE,
		    "%s: enable=%d, an_swq_depth > 0, ignoring\n",
		    __func__, enable);
		changed = 0;
	}

	return (changed);
#else
	struct ath_vap *avp = ATH_VAP(ni->ni_vap);

	/*
	 * Some operating modes don't set av_set_tim(), so don't
	 * update it here.
	 */
	if (avp->av_set_tim == NULL)
		return (0);

	return (avp->av_set_tim(ni, enable));
#endif /* ATH_SW_PSQ */
}

/*
 * Set or update the TIM from the software queue.
 *
 * Check the software queue depth before attempting to do lock
 * anything; that avoids trying to obtain the lock.  Then,
 * re-check afterwards to ensure nothing has changed in the
 * meantime.
 *
 * set:   This is designed to be called from the TX path, after
 *        a frame has been queued; to see if the swq > 0.
 *
 * clear: This is designed to be called from the buffer completion point
 *        (right now it's ath_tx_default_comp()) where the state of
 *        a software queue has changed.
 *
 * It makes sense to place it at buffer free / completion rather
 * than after each software queue operation, as there's no real
 * point in churning the TIM bit as the last frames in the software
 * queue are transmitted.  If they fail and we retry them, we'd
 * just be setting the TIM bit again anyway.
 */
void
ath_tx_update_tim(struct ath_softc *sc, struct ieee80211_node *ni,
     int enable)
{
#ifdef	ATH_SW_PSQ
	struct ath_node *an;
	struct ath_vap *avp;

	/* Don't do this for broadcast/etc frames */
	if (ni == NULL)
		return;

	an = ATH_NODE(ni);
	avp = ATH_VAP(ni->ni_vap);

	/*
	 * And for operating modes without the TIM handler set, let's
	 * just skip those.
	 */
	if (avp->av_set_tim == NULL)
		return;

	ATH_NODE_UNLOCK_ASSERT(an);

	if (enable) {
		/*
		 * Don't bother grabbing the lock unless the queue is not
		 * empty.
		 */
		if (atomic_load_acq_int(&an->an_swq_depth) == 0)
			return;

		ATH_NODE_LOCK(an);
		if (an->an_is_powersave &&
		    an->an_tim_set == 0 &&
		    atomic_load_acq_int(&an->an_swq_depth) != 0) {
			DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE,
			    "%s: an=%p, swq_depth>0, tim_set=0, set!\n",
			    __func__, an);
			an->an_tim_set = 1;
			ATH_NODE_UNLOCK(an);
			(void) avp->av_set_tim(ni, 1);
		} else {
			ATH_NODE_UNLOCK(an);
		}
	} else {
		/*
		 * Don't bother grabbing the lock unless the queue is empty.
		 */
		if (atomic_load_acq_int(&an->an_swq_depth) != 0)
			return;

		ATH_NODE_LOCK(an);
		if (an->an_is_powersave &&
		    an->an_stack_psq == 0 &&
		    an->an_tim_set == 1 &&
		    atomic_load_acq_int(&an->an_swq_depth) == 0) {
			DPRINTF(sc, ATH_DEBUG_NODE_PWRSAVE,
			    "%s: an=%p, swq_depth=0, tim_set=1, psq_set=0,"
			    " clear!\n",
			    __func__, an);
			an->an_tim_set = 0;
			ATH_NODE_UNLOCK(an);
			(void) avp->av_set_tim(ni, 0);
		} else {
			ATH_NODE_UNLOCK(an);
		}
	}
#else
	return;
#endif	/* ATH_SW_PSQ */
}

MODULE_VERSION(if_ath, 1);
MODULE_DEPEND(if_ath, wlan, 1, 1, 1);          /* 802.11 media layer */
#if	defined(IEEE80211_ALQ) || defined(AH_DEBUG_ALQ)
MODULE_DEPEND(if_ath, alq, 1, 1, 1);
#endif
