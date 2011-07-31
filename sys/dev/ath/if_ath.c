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
#include <dev/ath/if_ath_tx.h>
#include <dev/ath/if_ath_sysctl.h>
#include <dev/ath/if_ath_keycache.h>
#include <dev/ath/if_athdfs.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif


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
		    const char name[IFNAMSIZ], int unit, int opmode,
		    int flags, const uint8_t bssid[IEEE80211_ADDR_LEN],
		    const uint8_t mac[IEEE80211_ADDR_LEN]);
static void	ath_vap_delete(struct ieee80211vap *);
static void	ath_init(void *);
static void	ath_stop_locked(struct ifnet *);
static void	ath_stop(struct ifnet *);
static void	ath_start(struct ifnet *);
static int	ath_reset_vap(struct ieee80211vap *, u_long);
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
static void	ath_mode_init(struct ath_softc *);
static void	ath_setslottime(struct ath_softc *);
static void	ath_updateslot(struct ifnet *);
static int	ath_beaconq_setup(struct ath_hal *);
static int	ath_beacon_alloc(struct ath_softc *, struct ieee80211_node *);
static void	ath_beacon_update(struct ieee80211vap *, int item);
static void	ath_beacon_setup(struct ath_softc *, struct ath_buf *);
static void	ath_beacon_proc(void *, int);
static struct ath_buf *ath_beacon_generate(struct ath_softc *,
			struct ieee80211vap *);
static void	ath_bstuck_proc(void *, int);
static void	ath_beacon_return(struct ath_softc *, struct ath_buf *);
static void	ath_beacon_free(struct ath_softc *);
static void	ath_beacon_config(struct ath_softc *, struct ieee80211vap *);
static void	ath_descdma_cleanup(struct ath_softc *sc,
			struct ath_descdma *, ath_bufhead *);
static int	ath_desc_alloc(struct ath_softc *);
static void	ath_desc_free(struct ath_softc *);
static struct ieee80211_node *ath_node_alloc(struct ieee80211vap *,
			const uint8_t [IEEE80211_ADDR_LEN]);
static void	ath_node_free(struct ieee80211_node *);
static void	ath_node_getsignal(const struct ieee80211_node *,
			int8_t *, int8_t *);
static int	ath_rxbuf_init(struct ath_softc *, struct ath_buf *);
static void	ath_recv_mgmt(struct ieee80211_node *ni, struct mbuf *m,
			int subtype, int rssi, int nf);
static void	ath_setdefantenna(struct ath_softc *, u_int);
static void	ath_rx_proc(void *, int);
static void	ath_txq_init(struct ath_softc *sc, struct ath_txq *, int);
static struct ath_txq *ath_txq_setup(struct ath_softc*, int qtype, int subtype);
static int	ath_tx_setup(struct ath_softc *, int, int);
static int	ath_wme_update(struct ieee80211com *);
static void	ath_tx_cleanupq(struct ath_softc *, struct ath_txq *);
static void	ath_tx_cleanup(struct ath_softc *);
static void	ath_tx_proc_q0(void *, int);
static void	ath_tx_proc_q0123(void *, int);
static void	ath_tx_proc(void *, int);
static void	ath_tx_draintxq(struct ath_softc *, struct ath_txq *);
static int	ath_chan_set(struct ath_softc *, struct ieee80211_channel *);
static void	ath_draintxq(struct ath_softc *);
static void	ath_stoprecv(struct ath_softc *);
static int	ath_startrecv(struct ath_softc *);
static void	ath_chan_change(struct ath_softc *, struct ieee80211_channel *);
static void	ath_scan_start(struct ieee80211com *);
static void	ath_scan_end(struct ieee80211com *);
static void	ath_set_channel(struct ieee80211com *);
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
static void	ath_led_event(struct ath_softc *, int);

static int	ath_rate_setup(struct ath_softc *, u_int mode);
static void	ath_setcurmode(struct ath_softc *, enum ieee80211_phymode);

static void	ath_announce(struct ath_softc *);

static void	ath_dfs_tasklet(void *, int);

#ifdef IEEE80211_SUPPORT_TDMA
static void	ath_tdma_settimers(struct ath_softc *sc, u_int32_t nexttbtt,
		    u_int32_t bintval);
static void	ath_tdma_bintvalsetup(struct ath_softc *sc,
		    const struct ieee80211_tdma_state *tdma);
static void	ath_tdma_config(struct ath_softc *sc, struct ieee80211vap *vap);
static void	ath_tdma_update(struct ieee80211_node *ni,
		    const struct ieee80211_tdma_param *tdma, int);
static void	ath_tdma_beacon_send(struct ath_softc *sc,
		    struct ieee80211vap *vap);

static __inline void
ath_hal_setcca(struct ath_hal *ah, int ena)
{
	/*
	 * NB: fill me in; this is not provided by default because disabling
	 *     CCA in most locales violates regulatory.
	 */
}

static __inline int
ath_hal_getcca(struct ath_hal *ah)
{
	u_int32_t diag;
	if (ath_hal_getcapability(ah, HAL_CAP_DIAG, 0, &diag) != HAL_OK)
		return 1;
	return ((diag & 0x500000) == 0);
}

#define	TDMA_EP_MULTIPLIER	(1<<10) /* pow2 to optimize out * and / */
#define	TDMA_LPF_LEN		6
#define	TDMA_DUMMY_MARKER	0x127
#define	TDMA_EP_MUL(x, mul)	((x) * (mul))
#define	TDMA_IN(x)		(TDMA_EP_MUL((x), TDMA_EP_MULTIPLIER))
#define	TDMA_LPF(x, y, len) \
    ((x != TDMA_DUMMY_MARKER) ? (((x) * ((len)-1) + (y)) / (len)) : (y))
#define	TDMA_SAMPLE(x, y) do {					\
	x = TDMA_LPF((x), TDMA_IN(y), TDMA_LPF_LEN);		\
} while (0)
#define	TDMA_EP_RND(x,mul) \
	((((x)%(mul)) >= ((mul)/2)) ? ((x) + ((mul) - 1)) / (mul) : (x)/(mul))
#define	TDMA_AVG(x)		TDMA_EP_RND(x, TDMA_EP_MULTIPLIER)
#endif /* IEEE80211_SUPPORT_TDMA */

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

static	int ath_rxbuf = ATH_RXBUF;		/* # rx buffers to allocate */
SYSCTL_INT(_hw_ath, OID_AUTO, rxbuf, CTLFLAG_RW, &ath_rxbuf,
	    0, "rx buffers allocated");
TUNABLE_INT("hw.ath.rxbuf", &ath_rxbuf);
static	int ath_txbuf = ATH_TXBUF;		/* # tx buffers to allocate */
SYSCTL_INT(_hw_ath, OID_AUTO, txbuf, CTLFLAG_RW, &ath_txbuf,
	    0, "tx buffers allocated");
TUNABLE_INT("hw.ath.txbuf", &ath_txbuf);

static	int ath_bstuck_threshold = 4;		/* max missed beacons */
SYSCTL_INT(_hw_ath, OID_AUTO, bstuck, CTLFLAG_RW, &ath_bstuck_threshold,
	    0, "max missed beacon xmits before chip reset");

MALLOC_DEFINE(M_ATHDEV, "athdev", "ath driver dma buffers");

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

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: devid 0x%x\n", __func__, devid);

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto bad;
	}
	ic = ifp->if_l2com;

	/* set these up early for if_printf use */
	if_initname(ifp, device_get_name(sc->sc_dev),
		device_get_unit(sc->sc_dev));

	ah = ath_hal_attach(devid, sc, sc->sc_st, sc->sc_sh, sc->sc_eepromdata, &status);
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
	 * Allocate tx+rx descriptors and populate the lists.
	 */
	error = ath_desc_alloc(sc);
	if (error != 0) {
		if_printf(ifp, "failed to allocate descriptors: %d\n", error);
		goto bad;
	}
	callout_init_mtx(&sc->sc_cal_ch, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->sc_wd_ch, &sc->sc_mtx, 0);

	ATH_TXBUF_LOCK_INIT(sc);

	sc->sc_tq = taskqueue_create("ath_taskq", M_NOWAIT,
		taskqueue_thread_enqueue, &sc->sc_tq);
	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET,
		"%s taskq", ifp->if_xname);

	TASK_INIT(&sc->sc_rxtask, 0, ath_rx_proc, sc);
	TASK_INIT(&sc->sc_bmisstask, 0, ath_bmiss_proc, sc);
	TASK_INIT(&sc->sc_bstucktask,0, ath_bstuck_proc, sc);

	/*
	 * Allocate hardware transmit queues: one queue for
	 * beacon frames and one data queue for each QoS
	 * priority.  Note that the hal handles resetting
	 * these queues at the needed time.
	 *
	 * XXX PS-Poll
	 */
	sc->sc_bhalq = ath_beaconq_setup(ah);
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
		device_printf(sc->sc_dev, "%s: unable to attach DFS\n", __func__);
		error = EIO;
		goto bad2;
	}

	/* Start DFS processing tasklet */
	TASK_INIT(&sc->sc_dfstask, 0, ath_dfs_tasklet, sc);

	sc->sc_blinking = 0;
	sc->sc_ledstate = 1;
	sc->sc_ledon = 0;			/* low true */
	sc->sc_ledidle = (2700*hz)/1000;	/* 2.7sec */
	callout_init(&sc->sc_ledtimer, CALLOUT_MPSAFE);
	/*
	 * Auto-enable soft led processing for IBM cards and for
	 * 5211 minipci cards.  Users can also manually enable/disable
	 * support with a sysctl.
	 */
	sc->sc_softled = (devid == AR5212_DEVID_IBM || devid == AR5211_DEVID);
	if (sc->sc_softled) {
		ath_hal_gpioCfgOutput(ah, sc->sc_ledpin,
		    HAL_GPIO_MUX_MAC_NETWORK_LED);
		ath_hal_gpioset(ah, sc->sc_ledpin, !sc->sc_ledon);
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	ifp->if_start = ath_start;
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
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
		| IEEE80211_C_TXFRAG		/* handle tx frags */
#ifdef	ATH_ENABLE_DFS
		| IEEE80211_C_DFS		/* Enable DFS radar detection */
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
	 * The if_ath 11n support is completely not ready for normal use.
	 * Enabling this option will likely break everything and everything.
	 * Don't think of doing that unless you know what you're doing.
	 */

#ifdef	ATH_ENABLE_11N
	/*
	 * Query HT capabilities
	 */
	if (ath_hal_getcapability(ah, HAL_CAP_HT, 0, NULL) == HAL_OK &&
	    (wmodes & (HAL_MODE_HT20 | HAL_MODE_HT40))) {
		int rxs, txs;

		device_printf(sc->sc_dev, "[HT] enabling HT modes\n");
		ic->ic_htcaps = IEEE80211_HTC_HT		/* HT operation */
			    | IEEE80211_HTC_AMPDU		/* A-MPDU tx/rx */
			    | IEEE80211_HTC_AMSDU		/* A-MSDU tx/rx */
			    | IEEE80211_HTCAP_MAXAMSDU_3839	/* max A-MSDU length */
			    | IEEE80211_HTCAP_SMPS_OFF;		/* SM power save off */
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
		 * rx/tx stream is not currently used anywhere; it needs to be taken
		 * into account when negotiating which MCS rates it'll receive and
		 * what MCS rates are available for TX.
		 */
		(void) ath_hal_getcapability(ah, HAL_CAP_STREAMS, 0, &rxs);
		(void) ath_hal_getcapability(ah, HAL_CAP_STREAMS, 1, &txs);

		ath_hal_getrxchainmask(ah, &sc->sc_rxchainmask);
		ath_hal_gettxchainmask(ah, &sc->sc_txchainmask);

		ic->ic_txstream = txs;
		ic->ic_rxstream = rxs;

		device_printf(sc->sc_dev, "[HT] %d RX streams; %d TX streams\n", rxs, txs);
	}
#endif

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
	ic->ic_node_getsignal = ath_node_getsignal;
	ic->ic_scan_start = ath_scan_start;
	ic->ic_scan_end = ath_scan_end;
	ic->ic_set_channel = ath_set_channel;

	ieee80211_radiotap_attach(ic,
	    &sc->sc_tx_th.wt_ihdr, sizeof(sc->sc_tx_th),
		ATH_TX_RADIOTAP_PRESENT,
	    &sc->sc_rx_th.wr_ihdr, sizeof(sc->sc_rx_th),
		ATH_RX_RADIOTAP_PRESENT);

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
bad:
	if (ah)
		ath_hal_detach(ah);
	if (ifp != NULL)
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

	ath_dfs_detach(sc);
	ath_desc_free(sc);
	ath_tx_cleanup(sc);
	ath_hal_detach(sc->sc_ah);	/* NB: sets chip in full sleep */
	if_free(ifp);

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
ath_vap_create(struct ieee80211com *ic,
	const char name[IFNAMSIZ], int unit, int opmode, int flags,
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t mac0[IEEE80211_ADDR_LEN])
{
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_vap *avp;
	struct ieee80211vap *vap;
	uint8_t mac[IEEE80211_ADDR_LEN];
	int ic_opmode, needbeacon, error;

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
	if (needbeacon & STAILQ_EMPTY(&sc->sc_bbuf)) {
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
		avp->av_bcbuf = STAILQ_FIRST(&sc->sc_bbuf);
		STAILQ_REMOVE_HEAD(&sc->sc_bbuf, bf_list);
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

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/*
		 * Quiesce the hardware while we remove the vap.  In
		 * particular we need to reclaim all references to
		 * the vap state by any frames pending on the tx queues.
		 */
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc);		/* stop xmit side */
		ath_stoprecv(sc);		/* stop recv side */
	}

	ieee80211_vap_detach(vap);
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
		ATH_TXQ_LOCK_DESTROY(&avp->av_mcastq);
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
	ATH_UNLOCK(sc);
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
}

void
ath_suspend(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags %x\n",
		__func__, ifp->if_flags);

	sc->sc_resume_up = (ifp->if_flags & IFF_UP) != 0;
	if (ic->ic_opmode == IEEE80211_M_STA)
		ath_stop(ifp);
	else
		ieee80211_suspend_all(ic);
	/*
	 * NB: don't worry about putting the chip in low power
	 * mode; pci will power off our socket on suspend and
	 * CardBus detaches the device.
	 */
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

void
ath_resume(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags %x\n",
		__func__, ifp->if_flags);

	/*
	 * Must reset the chip before we reload the
	 * keycache as we were powered down on suspend.
	 */
	ath_hal_reset(ah, sc->sc_opmode,
	    sc->sc_curchan != NULL ? sc->sc_curchan : ic->ic_curchan,
	    AH_FALSE, &status);
	ath_reset_keycache(sc);

	/* Let DFS at it in case it's a DFS channel */
	ath_dfs_radar_enable(sc, ic->ic_curchan);

	if (sc->sc_resume_up) {
		if (ic->ic_opmode == IEEE80211_M_STA) {
			ath_init(sc);
			/*
			 * Program the beacon registers using the last rx'd
			 * beacon frame and enable sync on the next beacon
			 * we see.  This should handle the case where we
			 * wakeup and find the same AP and also the case where
			 * we wakeup and need to roam.  For the latter we
			 * should get bmiss events that trigger a roam.
			 */
			ath_beacon_config(sc, NULL);
			sc->sc_syncbeacon = 1;
		} else
			ieee80211_resume_all(ic);
	}
	if (sc->sc_softled) {
		ath_hal_gpioCfgOutput(ah, sc->sc_ledpin,
		    HAL_GPIO_MUX_MAC_NETWORK_LED);
		ath_hal_gpioset(ah, sc->sc_ledpin, !sc->sc_ledon);
	}

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

	if (sc->sc_invalid) {
		/*
		 * The hardware is not ready/present, don't touch anything.
		 * Note this can happen early on if the IRQ is shared.
		 */
		DPRINTF(sc, ATH_DEBUG_ANY, "%s: invalid; ignored\n", __func__);
		return;
	}
	if (!ath_hal_intrpend(ah))		/* shared irq, not for us */
		return;
	if ((ifp->if_flags & IFF_UP) == 0 ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		HAL_INT status;

		DPRINTF(sc, ATH_DEBUG_ANY, "%s: if_flags 0x%x\n",
			__func__, ifp->if_flags);
		ath_hal_getisr(ah, &status);	/* clear ISR */
		ath_hal_intrset(ah, 0);		/* disable further intr's */
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
	status &= sc->sc_imask;			/* discard unasked for bits */

	/* Short-circuit un-handled interrupts */
	if (status == 0x0)
		return;

	if (status & HAL_INT_FATAL) {
		sc->sc_stats.ast_hardware++;
		ath_hal_intrset(ah, 0);		/* disable intr's until reset */
		ath_fatal_proc(sc, 0);
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
				taskqueue_enqueue(sc->sc_tq, &sc->sc_rxtask);
#endif
			}
		}
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
			taskqueue_enqueue(sc->sc_tq, &sc->sc_rxtask);
		if (status & HAL_INT_TX)
			taskqueue_enqueue(sc->sc_tq, &sc->sc_txtask);
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
			ath_hal_intrset(ah, sc->sc_imask);
		}
		if (status & HAL_INT_RXORN) {
			/* NB: hal marks HAL_INT_FATAL when RXORN is fatal */
			sc->sc_stats.ast_rxorn++;
		}
	}
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
	ath_reset(ifp);
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

static int
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

	if (ath_hal_gethangstate(sc->sc_ah, 0xff, &hangs) && hangs != 0) {
		if_printf(ifp, "bb hang detected (0x%x), resetting\n", hangs);
		ath_reset(ifp);
	} else
		ieee80211_beacon_miss(ifp->if_l2com);
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
	if (!ath_hal_reset(ah, sc->sc_opmode, ic->ic_curchan, AH_FALSE, &status)) {
		if_printf(ifp, "unable to reset hardware; hal status %u\n",
			status);
		ATH_UNLOCK(sc);
		return;
	}
	ath_chan_change(sc, ic->ic_curchan);

	/* Let DFS at it in case it's a DFS channel */
	ath_dfs_radar_enable(sc, ic->ic_curchan);

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
		  | HAL_INT_FATAL | HAL_INT_GLOBAL;
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
		ath_draintxq(sc);
		if (!sc->sc_invalid) {
			ath_stoprecv(sc);
			ath_hal_phydisable(ah);
		} else
			sc->sc_rxlink = NULL;
		ath_beacon_free(sc);	/* XXX not needed */
	}
}

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
ath_reset(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;

	ath_hal_intrset(ah, 0);		/* disable interrupts */
	ath_draintxq(sc);		/* stop xmit side */
	ath_stoprecv(sc);		/* stop recv side */
	ath_settkipmic(sc);		/* configure TKIP MIC handling */
	/* NB: indicate channel change so we do a full reset */
	if (!ath_hal_reset(ah, sc->sc_opmode, ic->ic_curchan, AH_TRUE, &status))
		if_printf(ifp, "%s: unable to reset hardware; hal status %u\n",
			__func__, status);
	sc->sc_diversity = ath_hal_getdiversity(ah);

	/* Let DFS at it in case it's a DFS channel */
	ath_dfs_radar_enable(sc, ic->ic_curchan);

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
	ath_hal_intrset(ah, sc->sc_imask);

	ath_start(ifp);			/* restart xmit */
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
	return ath_reset(ifp);
}

struct ath_buf *
_ath_getbuf_locked(struct ath_softc *sc)
{
	struct ath_buf *bf;

	ATH_TXBUF_LOCK_ASSERT(sc);

	bf = STAILQ_FIRST(&sc->sc_txbuf);
	if (bf != NULL && (bf->bf_flags & ATH_BUF_BUSY) == 0)
		STAILQ_REMOVE_HEAD(&sc->sc_txbuf, bf_list);
	else
		bf = NULL;
	if (bf == NULL) {
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: %s\n", __func__,
		    STAILQ_FIRST(&sc->sc_txbuf) == NULL ?
			"out of xmit buffers" : "xmit buffer busy");
	}
	return bf;
}

struct ath_buf *
ath_getbuf(struct ath_softc *sc)
{
	struct ath_buf *bf;

	ATH_TXBUF_LOCK(sc);
	bf = _ath_getbuf_locked(sc);
	if (bf == NULL) {
		struct ifnet *ifp = sc->sc_ifp;

		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: stop queue\n", __func__);
		sc->sc_stats.ast_tx_qstop++;
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	}
	ATH_TXBUF_UNLOCK(sc);
	return bf;
}

static void
ath_start(struct ifnet *ifp)
{
	struct ath_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	struct mbuf *m, *next;
	ath_bufhead frags;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 || sc->sc_invalid)
		return;
	for (;;) {
		/*
		 * Grab a TX buffer and associated resources.
		 */
		bf = ath_getbuf(sc);
		if (bf == NULL)
			break;

		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			ATH_TXBUF_LOCK(sc);
			STAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
			ATH_TXBUF_UNLOCK(sc);
			break;
		}
		ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
		/*
		 * Check for fragmentation.  If this frame
		 * has been broken up verify we have enough
		 * buffers to send all the fragments so all
		 * go out or none...
		 */
		STAILQ_INIT(&frags);
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
			STAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
			ath_txfrag_cleanup(sc, &frags, ni);
			ATH_TXBUF_UNLOCK(sc);
			if (ni != NULL)
				ieee80211_free_node(ni);
			continue;
		}
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
				ath_freetx(next);
				goto reclaim;
			}
			m = next;
			bf = STAILQ_FIRST(&frags);
			KASSERT(bf != NULL, ("no buf for txfrag"));
			STAILQ_REMOVE_HEAD(&frags, bf_list);
			goto nextfrag;
		}

		sc->sc_wd_timer = 5;
	}
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

/*
 * Calculate the receive filter according to the
 * operating mode and state:
 *
 * o always accept unicast, broadcast, and multicast traffic
 * o accept PHY error frames when hardware doesn't have MIB support
 *   to count and we need them for ANI (sta mode only until recently)
 *   and we are not scanning (ANI is disabled)
 *   NB: older hal's add rx filter bits out of sight and we need to
 *	 blindly preserve them
 * o probe request frames are accepted only when operating in
 *   hostap, adhoc, mesh, or monitor modes
 * o enable promiscuous mode
 *   - when in monitor mode
 *   - if interface marked PROMISC (assumes bridge setting is filtered)
 * o accept beacons:
 *   - when operating in station mode for collecting rssi data when
 *     the station is otherwise quiet, or
 *   - when operating in adhoc mode so the 802.11 layer creates
 *     node table entries for peers,
 *   - when scanning
 *   - when doing s/w beacon miss (e.g. for ap+sta)
 *   - when operating in ap mode in 11g to detect overlapping bss that
 *     require protection
 *   - when operating in mesh mode to detect neighbors
 * o accept control frames:
 *   - when in monitor mode
 * XXX HT protection for 11n
 */
static u_int32_t
ath_calcrxfilter(struct ath_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	u_int32_t rfilt;

	rfilt = HAL_RX_FILTER_UCAST | HAL_RX_FILTER_BCAST | HAL_RX_FILTER_MCAST;
	if (!sc->sc_needmib && !sc->sc_scanning)
		rfilt |= HAL_RX_FILTER_PHYERR;
	if (ic->ic_opmode != IEEE80211_M_STA)
		rfilt |= HAL_RX_FILTER_PROBEREQ;
	/* XXX ic->ic_monvaps != 0? */
	if (ic->ic_opmode == IEEE80211_M_MONITOR || (ifp->if_flags & IFF_PROMISC))
		rfilt |= HAL_RX_FILTER_PROM;
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_IBSS ||
	    sc->sc_swbmiss || sc->sc_scanning)
		rfilt |= HAL_RX_FILTER_BEACON;
	/*
	 * NB: We don't recalculate the rx filter when
	 * ic_protmode changes; otherwise we could do
	 * this only when ic_protmode != NONE.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    IEEE80211_IS_CHAN_ANYG(ic->ic_curchan))
		rfilt |= HAL_RX_FILTER_BEACON;

	/*
	 * Enable hardware PS-POLL RX only for hostap mode;
	 * STA mode sends PS-POLL frames but never
	 * receives them.
	 */
	if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_PSPOLL,
	    0, NULL) == HAL_OK &&
	    ic->ic_opmode == IEEE80211_M_HOSTAP)
		rfilt |= HAL_RX_FILTER_PSPOLL;

	if (sc->sc_nmeshvaps) {
		rfilt |= HAL_RX_FILTER_BEACON;
		if (sc->sc_hasbmatch)
			rfilt |= HAL_RX_FILTER_BSSID;
		else
			rfilt |= HAL_RX_FILTER_PROM;
	}
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		rfilt |= HAL_RX_FILTER_CONTROL;

	if (sc->sc_dodfs) {
		rfilt |= HAL_RX_FILTER_PHYRADAR;
	}

	/*
	 * Enable RX of compressed BAR frames only when doing
	 * 802.11n. Required for A-MPDU.
	 */
	if (IEEE80211_IS_CHAN_HT(ic->ic_curchan))
		rfilt |= HAL_RX_FILTER_COMPBAR;

	DPRINTF(sc, ATH_DEBUG_MODE, "%s: RX filter 0x%x, %s if_flags 0x%x\n",
	    __func__, rfilt, ieee80211_opmode_name[ic->ic_opmode], ifp->if_flags);
	return rfilt;
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

static void
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

	/* handle any link-level address change */
	ath_hal_setmac(ah, IF_LLADDR(ifp));

	/* calculate and install multicast filter */
	ath_update_mcast(ifp);
}

/*
 * Set the slot time based on the current setting.
 */
static void
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
 * Setup a h/w transmit queue for beacons.
 */
static int
ath_beaconq_setup(struct ath_hal *ah)
{
	HAL_TXQ_INFO qi;

	memset(&qi, 0, sizeof(qi));
	qi.tqi_aifs = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmin = HAL_TXQ_USEDEFAULT;
	qi.tqi_cwmax = HAL_TXQ_USEDEFAULT;
	/* NB: for dynamic turbo, don't enable any other interrupts */
	qi.tqi_qflags = HAL_TXQ_TXDESCINT_ENABLE;
	return ath_hal_setuptxqueue(ah, HAL_TX_QUEUE_BEACON, &qi);
}

/*
 * Setup the transmit queue parameters for the beacon queue.
 */
static int
ath_beaconq_config(struct ath_softc *sc)
{
#define	ATH_EXPONENT_TO_VALUE(v)	((1<<(v))-1)
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	HAL_TXQ_INFO qi;

	ath_hal_gettxqueueprops(ah, sc->sc_bhalq, &qi);
	if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	    ic->ic_opmode == IEEE80211_M_MBSS) {
		/*
		 * Always burst out beacon and CAB traffic.
		 */
		qi.tqi_aifs = ATH_BEACON_AIFS_DEFAULT;
		qi.tqi_cwmin = ATH_BEACON_CWMIN_DEFAULT;
		qi.tqi_cwmax = ATH_BEACON_CWMAX_DEFAULT;
	} else {
		struct wmeParams *wmep =
			&ic->ic_wme.wme_chanParams.cap_wmeParams[WME_AC_BE];
		/*
		 * Adhoc mode; important thing is to use 2x cwmin.
		 */
		qi.tqi_aifs = wmep->wmep_aifsn;
		qi.tqi_cwmin = 2*ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmin);
		qi.tqi_cwmax = ATH_EXPONENT_TO_VALUE(wmep->wmep_logcwmax);
	}

	if (!ath_hal_settxqueueprops(ah, sc->sc_bhalq, &qi)) {
		device_printf(sc->sc_dev, "unable to update parameters for "
			"beacon hardware queue!\n");
		return 0;
	} else {
		ath_hal_resettxqueue(ah, sc->sc_bhalq); /* push to h/w */
		return 1;
	}
#undef ATH_EXPONENT_TO_VALUE
}

/*
 * Allocate and setup an initial beacon frame.
 */
static int
ath_beacon_alloc(struct ath_softc *sc, struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ath_vap *avp = ATH_VAP(vap);
	struct ath_buf *bf;
	struct mbuf *m;
	int error;

	bf = avp->av_bcbuf;
	if (bf->bf_m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
	}
	if (bf->bf_node != NULL) {
		ieee80211_free_node(bf->bf_node);
		bf->bf_node = NULL;
	}

	/*
	 * NB: the beacon data buffer must be 32-bit aligned;
	 * we assume the mbuf routines will return us something
	 * with this alignment (perhaps should assert).
	 */
	m = ieee80211_beacon_alloc(ni, &avp->av_boff);
	if (m == NULL) {
		device_printf(sc->sc_dev, "%s: cannot get mbuf\n", __func__);
		sc->sc_stats.ast_be_nombuf++;
		return ENOMEM;
	}
	error = bus_dmamap_load_mbuf_sg(sc->sc_dmat, bf->bf_dmamap, m,
				     bf->bf_segs, &bf->bf_nseg,
				     BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: cannot map mbuf, bus_dmamap_load_mbuf_sg returns %d\n",
		    __func__, error);
		m_freem(m);
		return error;
	}

	/*
	 * Calculate a TSF adjustment factor required for staggered
	 * beacons.  Note that we assume the format of the beacon
	 * frame leaves the tstamp field immediately following the
	 * header.
	 */
	if (sc->sc_stagbeacons && avp->av_bslot > 0) {
		uint64_t tsfadjust;
		struct ieee80211_frame *wh;

		/*
		 * The beacon interval is in TU's; the TSF is in usecs.
		 * We figure out how many TU's to add to align the timestamp
		 * then convert to TSF units and handle byte swapping before
		 * inserting it in the frame.  The hardware will then add this
		 * each time a beacon frame is sent.  Note that we align vap's
		 * 1..N and leave vap 0 untouched.  This means vap 0 has a
		 * timestamp in one beacon interval while the others get a
		 * timstamp aligned to the next interval.
		 */
		tsfadjust = ni->ni_intval *
		    (ATH_BCBUF - avp->av_bslot) / ATH_BCBUF;
		tsfadjust = htole64(tsfadjust << 10);	/* TU -> TSF */

		DPRINTF(sc, ATH_DEBUG_BEACON,
		    "%s: %s beacons bslot %d intval %u tsfadjust %llu\n",
		    __func__, sc->sc_stagbeacons ? "stagger" : "burst",
		    avp->av_bslot, ni->ni_intval,
		    (long long unsigned) le64toh(tsfadjust));

		wh = mtod(m, struct ieee80211_frame *);
		memcpy(&wh[1], &tsfadjust, sizeof(tsfadjust));
	}
	bf->bf_m = m;
	bf->bf_node = ieee80211_ref_node(ni);

	return 0;
}

/*
 * Setup the beacon frame for transmit.
 */
static void
ath_beacon_setup(struct ath_softc *sc, struct ath_buf *bf)
{
#define	USE_SHPREAMBLE(_ic) \
	(((_ic)->ic_flags & (IEEE80211_F_SHPREAMBLE | IEEE80211_F_USEBARKER))\
		== IEEE80211_F_SHPREAMBLE)
	struct ieee80211_node *ni = bf->bf_node;
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m = bf->bf_m;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	int flags, antenna;
	const HAL_RATE_TABLE *rt;
	u_int8_t rix, rate;

	DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: m %p len %u\n",
		__func__, m, m->m_len);

	/* setup descriptors */
	ds = bf->bf_desc;

	flags = HAL_TXDESC_NOACK;
	if (ic->ic_opmode == IEEE80211_M_IBSS && sc->sc_hasveol) {
		ds->ds_link = bf->bf_daddr;	/* self-linked */
		flags |= HAL_TXDESC_VEOL;
		/*
		 * Let hardware handle antenna switching.
		 */
		antenna = sc->sc_txantenna;
	} else {
		ds->ds_link = 0;
		/*
		 * Switch antenna every 4 beacons.
		 * XXX assumes two antenna
		 */
		if (sc->sc_txantenna != 0)
			antenna = sc->sc_txantenna;
		else if (sc->sc_stagbeacons && sc->sc_nbcnvaps != 0)
			antenna = ((sc->sc_stats.ast_be_xmit / sc->sc_nbcnvaps) & 4 ? 2 : 1);
		else
			antenna = (sc->sc_stats.ast_be_xmit & 4 ? 2 : 1);
	}

	KASSERT(bf->bf_nseg == 1,
		("multi-segment beacon frame; nseg %u", bf->bf_nseg));
	ds->ds_data = bf->bf_segs[0].ds_addr;
	/*
	 * Calculate rate code.
	 * XXX everything at min xmit rate
	 */
	rix = 0;
	rt = sc->sc_currates;
	rate = rt->info[rix].rateCode;
	if (USE_SHPREAMBLE(ic))
		rate |= rt->info[rix].shortPreamble;
	ath_hal_setuptxdesc(ah, ds
		, m->m_len + IEEE80211_CRC_LEN	/* frame length */
		, sizeof(struct ieee80211_frame)/* header length */
		, HAL_PKT_TYPE_BEACON		/* Atheros packet type */
		, ni->ni_txpower		/* txpower XXX */
		, rate, 1			/* series 0 rate/tries */
		, HAL_TXKEYIX_INVALID		/* no encryption */
		, antenna			/* antenna mode */
		, flags				/* no ack, veol for beacons */
		, 0				/* rts/cts rate */
		, 0				/* rts/cts duration */
	);
	/* NB: beacon's BufLen must be a multiple of 4 bytes */
	ath_hal_filltxdesc(ah, ds
		, roundup(m->m_len, 4)		/* buffer length */
		, AH_TRUE			/* first segment */
		, AH_TRUE			/* last segment */
		, ds				/* first descriptor */
	);
#if 0
	ath_desc_swap(ds);
#endif
#undef USE_SHPREAMBLE
}

static void
ath_beacon_update(struct ieee80211vap *vap, int item)
{
	struct ieee80211_beacon_offsets *bo = &ATH_VAP(vap)->av_boff;

	setbit(bo->bo_flags, item);
}

/*
 * Append the contents of src to dst; both queues
 * are assumed to be locked.
 */
static void
ath_txqmove(struct ath_txq *dst, struct ath_txq *src)
{
	STAILQ_CONCAT(&dst->axq_q, &src->axq_q);
	dst->axq_link = src->axq_link;
	src->axq_link = NULL;
	dst->axq_depth += src->axq_depth;
	src->axq_depth = 0;
}

/*
 * Transmit a beacon frame at SWBA.  Dynamic updates to the
 * frame contents are done as needed and the slot time is
 * also adjusted based on current state.
 */
static void
ath_beacon_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211vap *vap;
	struct ath_buf *bf;
	int slot, otherant;
	uint32_t bfaddr;

	DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: pending %u\n",
		__func__, pending);
	/*
	 * Check if the previous beacon has gone out.  If
	 * not don't try to post another, skip this period
	 * and wait for the next.  Missed beacons indicate
	 * a problem and should not occur.  If we miss too
	 * many consecutive beacons reset the device.
	 */
	if (ath_hal_numtxpending(ah, sc->sc_bhalq) != 0) {
		sc->sc_bmisscount++;
		sc->sc_stats.ast_be_missed++;
		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: missed %u consecutive beacons\n",
			__func__, sc->sc_bmisscount);
		if (sc->sc_bmisscount >= ath_bstuck_threshold)
			taskqueue_enqueue(sc->sc_tq, &sc->sc_bstucktask);
		return;
	}
	if (sc->sc_bmisscount != 0) {
		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: resume beacon xmit after %u misses\n",
			__func__, sc->sc_bmisscount);
		sc->sc_bmisscount = 0;
	}

	if (sc->sc_stagbeacons) {			/* staggered beacons */
		struct ieee80211com *ic = sc->sc_ifp->if_l2com;
		uint32_t tsftu;

		tsftu = ath_hal_gettsf32(ah) >> 10;
		/* XXX lintval */
		slot = ((tsftu % ic->ic_lintval) * ATH_BCBUF) / ic->ic_lintval;
		vap = sc->sc_bslot[(slot+1) % ATH_BCBUF];
		bfaddr = 0;
		if (vap != NULL && vap->iv_state >= IEEE80211_S_RUN) {
			bf = ath_beacon_generate(sc, vap);
			if (bf != NULL)
				bfaddr = bf->bf_daddr;
		}
	} else {					/* burst'd beacons */
		uint32_t *bflink = &bfaddr;

		for (slot = 0; slot < ATH_BCBUF; slot++) {
			vap = sc->sc_bslot[slot];
			if (vap != NULL && vap->iv_state >= IEEE80211_S_RUN) {
				bf = ath_beacon_generate(sc, vap);
				if (bf != NULL) {
					*bflink = bf->bf_daddr;
					bflink = &bf->bf_desc->ds_link;
				}
			}
		}
		*bflink = 0;				/* terminate list */
	}

	/*
	 * Handle slot time change when a non-ERP station joins/leaves
	 * an 11g network.  The 802.11 layer notifies us via callback,
	 * we mark updateslot, then wait one beacon before effecting
	 * the change.  This gives associated stations at least one
	 * beacon interval to note the state change.
	 */
	/* XXX locking */
	if (sc->sc_updateslot == UPDATE) {
		sc->sc_updateslot = COMMIT;	/* commit next beacon */
		sc->sc_slotupdate = slot;
	} else if (sc->sc_updateslot == COMMIT && sc->sc_slotupdate == slot)
		ath_setslottime(sc);		/* commit change to h/w */

	/*
	 * Check recent per-antenna transmit statistics and flip
	 * the default antenna if noticeably more frames went out
	 * on the non-default antenna.
	 * XXX assumes 2 anntenae
	 */
	if (!sc->sc_diversity && (!sc->sc_stagbeacons || slot == 0)) {
		otherant = sc->sc_defant & 1 ? 2 : 1;
		if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + 2)
			ath_setdefantenna(sc, otherant);
		sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;
	}

	if (bfaddr != 0) {
		/*
		 * Stop any current dma and put the new frame on the queue.
		 * This should never fail since we check above that no frames
		 * are still pending on the queue.
		 */
		if (!ath_hal_stoptxdma(ah, sc->sc_bhalq)) {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: beacon queue %u did not stop?\n",
				__func__, sc->sc_bhalq);
		}
		/* NB: cabq traffic should already be queued and primed */
		ath_hal_puttxbuf(ah, sc->sc_bhalq, bfaddr);
		ath_hal_txstart(ah, sc->sc_bhalq);

		sc->sc_stats.ast_be_xmit++;
	}
}

static struct ath_buf *
ath_beacon_generate(struct ath_softc *sc, struct ieee80211vap *vap)
{
	struct ath_vap *avp = ATH_VAP(vap);
	struct ath_txq *cabq = sc->sc_cabq;
	struct ath_buf *bf;
	struct mbuf *m;
	int nmcastq, error;

	KASSERT(vap->iv_state >= IEEE80211_S_RUN,
	    ("not running, state %d", vap->iv_state));
	KASSERT(avp->av_bcbuf != NULL, ("no beacon buffer"));

	/*
	 * Update dynamic beacon contents.  If this returns
	 * non-zero then we need to remap the memory because
	 * the beacon frame changed size (probably because
	 * of the TIM bitmap).
	 */
	bf = avp->av_bcbuf;
	m = bf->bf_m;
	nmcastq = avp->av_mcastq.axq_depth;
	if (ieee80211_beacon_update(bf->bf_node, &avp->av_boff, m, nmcastq)) {
		/* XXX too conservative? */
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		error = bus_dmamap_load_mbuf_sg(sc->sc_dmat, bf->bf_dmamap, m,
					     bf->bf_segs, &bf->bf_nseg,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			if_printf(vap->iv_ifp,
			    "%s: bus_dmamap_load_mbuf_sg failed, error %u\n",
			    __func__, error);
			return NULL;
		}
	}
	if ((avp->av_boff.bo_tim[4] & 1) && cabq->axq_depth) {
		DPRINTF(sc, ATH_DEBUG_BEACON,
		    "%s: cabq did not drain, mcastq %u cabq %u\n",
		    __func__, nmcastq, cabq->axq_depth);
		sc->sc_stats.ast_cabq_busy++;
		if (sc->sc_nvaps > 1 && sc->sc_stagbeacons) {
			/*
			 * CABQ traffic from a previous vap is still pending.
			 * We must drain the q before this beacon frame goes
			 * out as otherwise this vap's stations will get cab
			 * frames from a different vap.
			 * XXX could be slow causing us to miss DBA
			 */
			ath_tx_draintxq(sc, cabq);
		}
	}
	ath_beacon_setup(sc, bf);
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREWRITE);

	/*
	 * Enable the CAB queue before the beacon queue to
	 * insure cab frames are triggered by this beacon.
	 */
	if (avp->av_boff.bo_tim[4] & 1) {
		struct ath_hal *ah = sc->sc_ah;

		/* NB: only at DTIM */
		ATH_TXQ_LOCK(cabq);
		ATH_TXQ_LOCK(&avp->av_mcastq);
		if (nmcastq) {
			struct ath_buf *bfm;

			/*
			 * Move frames from the s/w mcast q to the h/w cab q.
			 * XXX MORE_DATA bit
			 */
			bfm = STAILQ_FIRST(&avp->av_mcastq.axq_q);
			if (cabq->axq_link != NULL) {
				*cabq->axq_link = bfm->bf_daddr;
			} else
				ath_hal_puttxbuf(ah, cabq->axq_qnum,
					bfm->bf_daddr);
			ath_txqmove(cabq, &avp->av_mcastq);

			sc->sc_stats.ast_cabq_xmit += nmcastq;
		}
		/* NB: gated by beacon so safe to start here */
		ath_hal_txstart(ah, cabq->axq_qnum);
		ATH_TXQ_UNLOCK(cabq);
		ATH_TXQ_UNLOCK(&avp->av_mcastq);
	}
	return bf;
}

static void
ath_beacon_start_adhoc(struct ath_softc *sc, struct ieee80211vap *vap)
{
	struct ath_vap *avp = ATH_VAP(vap);
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;
	struct mbuf *m;
	int error;

	KASSERT(avp->av_bcbuf != NULL, ("no beacon buffer"));

	/*
	 * Update dynamic beacon contents.  If this returns
	 * non-zero then we need to remap the memory because
	 * the beacon frame changed size (probably because
	 * of the TIM bitmap).
	 */
	bf = avp->av_bcbuf;
	m = bf->bf_m;
	if (ieee80211_beacon_update(bf->bf_node, &avp->av_boff, m, 0)) {
		/* XXX too conservative? */
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		error = bus_dmamap_load_mbuf_sg(sc->sc_dmat, bf->bf_dmamap, m,
					     bf->bf_segs, &bf->bf_nseg,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			if_printf(vap->iv_ifp,
			    "%s: bus_dmamap_load_mbuf_sg failed, error %u\n",
			    __func__, error);
			return;
		}
	}
	ath_beacon_setup(sc, bf);
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREWRITE);

	/* NB: caller is known to have already stopped tx dma */
	ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
	ath_hal_txstart(ah, sc->sc_bhalq);
}

/*
 * Reset the hardware after detecting beacons have stopped.
 */
static void
ath_bstuck_proc(void *arg, int pending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if_printf(ifp, "stuck beacon; resetting (bmiss count %u)\n",
		sc->sc_bmisscount);
	sc->sc_stats.ast_bstuck++;
	ath_reset(ifp);
}

/*
 * Reclaim beacon resources and return buffer to the pool.
 */
static void
ath_beacon_return(struct ath_softc *sc, struct ath_buf *bf)
{

	if (bf->bf_m != NULL) {
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
	}
	if (bf->bf_node != NULL) {
		ieee80211_free_node(bf->bf_node);
		bf->bf_node = NULL;
	}
	STAILQ_INSERT_TAIL(&sc->sc_bbuf, bf, bf_list);
}

/*
 * Reclaim beacon resources.
 */
static void
ath_beacon_free(struct ath_softc *sc)
{
	struct ath_buf *bf;

	STAILQ_FOREACH(bf, &sc->sc_bbuf, bf_list) {
		if (bf->bf_m != NULL) {
			bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
			m_freem(bf->bf_m);
			bf->bf_m = NULL;
		}
		if (bf->bf_node != NULL) {
			ieee80211_free_node(bf->bf_node);
			bf->bf_node = NULL;
		}
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
ath_beacon_config(struct ath_softc *sc, struct ieee80211vap *vap)
{
#define	TSF_TO_TU(_h,_l) \
	((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))
#define	FUDGE	2
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	struct ieee80211_node *ni;
	u_int32_t nexttbtt, intval, tsftu;
	u_int64_t tsf;

	if (vap == NULL)
		vap = TAILQ_FIRST(&ic->ic_vaps);	/* XXX */
	ni = vap->iv_bss;

	/* extract tstamp from last beacon and convert to TU */
	nexttbtt = TSF_TO_TU(LE_READ_4(ni->ni_tstamp.data + 4),
			     LE_READ_4(ni->ni_tstamp.data));
	if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	    ic->ic_opmode == IEEE80211_M_MBSS) {
		/*
		 * For multi-bss ap/mesh support beacons are either staggered
		 * evenly over N slots or burst together.  For the former
		 * arrange for the SWBA to be delivered for each slot.
		 * Slots that are not occupied will generate nothing.
		 */
		/* NB: the beacon interval is kept internally in TU's */
		intval = ni->ni_intval & HAL_BEACON_PERIOD;
		if (sc->sc_stagbeacons)
			intval /= ATH_BCBUF;
	} else {
		/* NB: the beacon interval is kept internally in TU's */
		intval = ni->ni_intval & HAL_BEACON_PERIOD;
	}
	if (nexttbtt == 0)		/* e.g. for ap mode */
		nexttbtt = intval;
	else if (intval)		/* NB: can be 0 for monitor mode */
		nexttbtt = roundup(nexttbtt, intval);
	DPRINTF(sc, ATH_DEBUG_BEACON, "%s: nexttbtt %u intval %u (%u)\n",
		__func__, nexttbtt, intval, ni->ni_intval);
	if (ic->ic_opmode == IEEE80211_M_STA && !sc->sc_swbmiss) {
		HAL_BEACON_STATE bs;
		int dtimperiod, dtimcount;
		int cfpperiod, cfpcount;

		/*
		 * Setup dtim and cfp parameters according to
		 * last beacon we received (which may be none).
		 */
		dtimperiod = ni->ni_dtim_period;
		if (dtimperiod <= 0)		/* NB: 0 if not known */
			dtimperiod = 1;
		dtimcount = ni->ni_dtim_count;
		if (dtimcount >= dtimperiod)	/* NB: sanity check */
			dtimcount = 0;		/* XXX? */
		cfpperiod = 1;			/* NB: no PCF support yet */
		cfpcount = 0;
		/*
		 * Pull nexttbtt forward to reflect the current
		 * TSF and calculate dtim+cfp state for the result.
		 */
		tsf = ath_hal_gettsf64(ah);
		tsftu = TSF_TO_TU(tsf>>32, tsf) + FUDGE;
		do {
			nexttbtt += intval;
			if (--dtimcount < 0) {
				dtimcount = dtimperiod - 1;
				if (--cfpcount < 0)
					cfpcount = cfpperiod - 1;
			}
		} while (nexttbtt < tsftu);
		memset(&bs, 0, sizeof(bs));
		bs.bs_intval = intval;
		bs.bs_nexttbtt = nexttbtt;
		bs.bs_dtimperiod = dtimperiod*intval;
		bs.bs_nextdtim = bs.bs_nexttbtt + dtimcount*intval;
		bs.bs_cfpperiod = cfpperiod*bs.bs_dtimperiod;
		bs.bs_cfpnext = bs.bs_nextdtim + cfpcount*bs.bs_dtimperiod;
		bs.bs_cfpmaxduration = 0;
#if 0
		/*
		 * The 802.11 layer records the offset to the DTIM
		 * bitmap while receiving beacons; use it here to
		 * enable h/w detection of our AID being marked in
		 * the bitmap vector (to indicate frames for us are
		 * pending at the AP).
		 * XXX do DTIM handling in s/w to WAR old h/w bugs
		 * XXX enable based on h/w rev for newer chips
		 */
		bs.bs_timoffset = ni->ni_timoff;
#endif
		/*
		 * Calculate the number of consecutive beacons to miss
		 * before taking a BMISS interrupt.
		 * Note that we clamp the result to at most 10 beacons.
		 */
		bs.bs_bmissthreshold = vap->iv_bmissthreshold;
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
			roundup(IEEE80211_MS_TO_TU(100), bs.bs_intval);
		if (bs.bs_sleepduration > bs.bs_dtimperiod)
			bs.bs_sleepduration = roundup(bs.bs_sleepduration, bs.bs_dtimperiod);

		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: tsf %ju tsf:tu %u intval %u nexttbtt %u dtim %u nextdtim %u bmiss %u sleep %u cfp:period %u maxdur %u next %u timoffset %u\n"
			, __func__
			, tsf, tsftu
			, bs.bs_intval
			, bs.bs_nexttbtt
			, bs.bs_dtimperiod
			, bs.bs_nextdtim
			, bs.bs_bmissthreshold
			, bs.bs_sleepduration
			, bs.bs_cfpperiod
			, bs.bs_cfpmaxduration
			, bs.bs_cfpnext
			, bs.bs_timoffset
		);
		ath_hal_intrset(ah, 0);
		ath_hal_beacontimers(ah, &bs);
		sc->sc_imask |= HAL_INT_BMISS;
		ath_hal_intrset(ah, sc->sc_imask);
	} else {
		ath_hal_intrset(ah, 0);
		if (nexttbtt == intval)
			intval |= HAL_BEACON_RESET_TSF;
		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			/*
			 * In IBSS mode enable the beacon timers but only
			 * enable SWBA interrupts if we need to manually
			 * prepare beacon frames.  Otherwise we use a
			 * self-linked tx descriptor and let the hardware
			 * deal with things.
			 */
			intval |= HAL_BEACON_ENA;
			if (!sc->sc_hasveol)
				sc->sc_imask |= HAL_INT_SWBA;
			if ((intval & HAL_BEACON_RESET_TSF) == 0) {
				/*
				 * Pull nexttbtt forward to reflect
				 * the current TSF.
				 */
				tsf = ath_hal_gettsf64(ah);
				tsftu = TSF_TO_TU(tsf>>32, tsf) + FUDGE;
				do {
					nexttbtt += intval;
				} while (nexttbtt < tsftu);
			}
			ath_beaconq_config(sc);
		} else if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
		    ic->ic_opmode == IEEE80211_M_MBSS) {
			/*
			 * In AP/mesh mode we enable the beacon timers
			 * and SWBA interrupts to prepare beacon frames.
			 */
			intval |= HAL_BEACON_ENA;
			sc->sc_imask |= HAL_INT_SWBA;	/* beacon prepare */
			ath_beaconq_config(sc);
		}
		ath_hal_beaconinit(ah, nexttbtt, intval);
		sc->sc_bmisscount = 0;
		ath_hal_intrset(ah, sc->sc_imask);
		/*
		 * When using a self-linked beacon descriptor in
		 * ibss mode load it once here.
		 */
		if (ic->ic_opmode == IEEE80211_M_IBSS && sc->sc_hasveol)
			ath_beacon_start_adhoc(sc, vap);
	}
	sc->sc_syncbeacon = 0;
#undef FUDGE
#undef TSF_TO_TU
}

static void
ath_load_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *paddr = (bus_addr_t*) arg;
	KASSERT(error == 0, ("error %u on bus_dma callback", error));
	*paddr = segs->ds_addr;
}

static int
ath_descdma_setup(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head,
	const char *name, int nbuf, int ndesc)
{
#define	DS2PHYS(_dd, _ds) \
	((_dd)->dd_desc_paddr + ((caddr_t)(_ds) - (caddr_t)(_dd)->dd_desc))
#define	ATH_DESC_4KB_BOUND_CHECK(_daddr, _len) \
	((((u_int32_t)(_daddr) & 0xFFF) > (0x1000 - (_len))) ? 1 : 0)
	struct ifnet *ifp = sc->sc_ifp;
	uint8_t *ds;
	struct ath_buf *bf;
	int i, bsize, error;
	int desc_len;

	desc_len = sizeof(struct ath_desc);

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA: %u buffers %u desc/buf\n",
	    __func__, name, nbuf, ndesc);

	dd->dd_name = name;
	dd->dd_desc_len = desc_len * nbuf * ndesc;

	device_printf(sc->sc_dev, "desc_len: %d, nbuf=%d, ndesc=%d; dd_desc_len=%d\n",
	    desc_len, nbuf, ndesc, dd->dd_desc_len);

	/*
	 * Merlin work-around:
	 * Descriptors that cross the 4KB boundary can't be used.
	 * Assume one skipped descriptor per 4KB page.
	 */
	if (! ath_hal_split4ktrans(sc->sc_ah)) {
		int numdescpage = 4096 / (desc_len * ndesc);
		dd->dd_desc_len = (nbuf / numdescpage + 1) * 4096;
		device_printf(sc->sc_dev, "numdescpage: %d, new dd_desc_len=%d\n",
		    numdescpage, dd->dd_desc_len);
	}

	/*
	 * Setup DMA descriptor area.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev),	/* parent */
		       PAGE_SIZE, 0,		/* alignment, bounds */
		       BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		       BUS_SPACE_MAXADDR,	/* highaddr */
		       NULL, NULL,		/* filter, filterarg */
		       dd->dd_desc_len,		/* maxsize */
		       1,			/* nsegments */
		       dd->dd_desc_len,		/* maxsegsize */
		       BUS_DMA_ALLOCNOW,	/* flags */
		       NULL,			/* lockfunc */
		       NULL,			/* lockarg */
		       &dd->dd_dmat);
	if (error != 0) {
		if_printf(ifp, "cannot allocate %s DMA tag\n", dd->dd_name);
		return error;
	}

	/* allocate descriptors */
	error = bus_dmamap_create(dd->dd_dmat, BUS_DMA_NOWAIT, &dd->dd_dmamap);
	if (error != 0) {
		if_printf(ifp, "unable to create dmamap for %s descriptors, "
			"error %u\n", dd->dd_name, error);
		goto fail0;
	}

	error = bus_dmamem_alloc(dd->dd_dmat, (void**) &dd->dd_desc,
				 BUS_DMA_NOWAIT | BUS_DMA_COHERENT,
				 &dd->dd_dmamap);
	if (error != 0) {
		if_printf(ifp, "unable to alloc memory for %u %s descriptors, "
			"error %u\n", nbuf * ndesc, dd->dd_name, error);
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

	ds = (uint8_t *) dd->dd_desc;
	DPRINTF(sc, ATH_DEBUG_RESET, "%s: %s DMA map: %p (%lu) -> %p (%lu)\n",
	    __func__, dd->dd_name, ds, (u_long) dd->dd_desc_len,
	    (caddr_t) dd->dd_desc_paddr, /*XXX*/ (u_long) dd->dd_desc_len);

	/* allocate rx buffers */
	bsize = sizeof(struct ath_buf) * nbuf;
	bf = malloc(bsize, M_ATHDEV, M_NOWAIT | M_ZERO);
	if (bf == NULL) {
		if_printf(ifp, "malloc of %s buffers failed, size %u\n",
			dd->dd_name, bsize);
		goto fail3;
	}
	dd->dd_bufptr = bf;

	STAILQ_INIT(head);
	for (i = 0; i < nbuf; i++, bf++, ds += (ndesc * desc_len)) {
		bf->bf_desc = (struct ath_desc *) ds;
		bf->bf_daddr = DS2PHYS(dd, ds);
		if (! ath_hal_split4ktrans(sc->sc_ah)) {
			/*
			 * Merlin WAR: Skip descriptor addresses which
			 * cause 4KB boundary crossing along any point
			 * in the descriptor.
			 */
			 if (ATH_DESC_4KB_BOUND_CHECK(bf->bf_daddr,
			     desc_len * ndesc)) {
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
		STAILQ_INSERT_TAIL(head, bf, bf_list);
	}
	return 0;
fail3:
	bus_dmamap_unload(dd->dd_dmat, dd->dd_dmamap);
fail2:
	bus_dmamem_free(dd->dd_dmat, dd->dd_desc, dd->dd_dmamap);
fail1:
	bus_dmamap_destroy(dd->dd_dmat, dd->dd_dmamap);
fail0:
	bus_dma_tag_destroy(dd->dd_dmat);
	memset(dd, 0, sizeof(*dd));
	return error;
#undef DS2PHYS
#undef ATH_DESC_4KB_BOUND_CHECK
}

static void
ath_descdma_cleanup(struct ath_softc *sc,
	struct ath_descdma *dd, ath_bufhead *head)
{
	struct ath_buf *bf;
	struct ieee80211_node *ni;

	bus_dmamap_unload(dd->dd_dmat, dd->dd_dmamap);
	bus_dmamem_free(dd->dd_dmat, dd->dd_desc, dd->dd_dmamap);
	bus_dmamap_destroy(dd->dd_dmat, dd->dd_dmamap);
	bus_dma_tag_destroy(dd->dd_dmat);

	STAILQ_FOREACH(bf, head, bf_list) {
		if (bf->bf_m) {
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

	STAILQ_INIT(head);
	free(dd->dd_bufptr, M_ATHDEV);
	memset(dd, 0, sizeof(*dd));
}

static int
ath_desc_alloc(struct ath_softc *sc)
{
	int error;

	error = ath_descdma_setup(sc, &sc->sc_rxdma, &sc->sc_rxbuf,
			"rx", ath_rxbuf, 1);
	if (error != 0)
		return error;

	error = ath_descdma_setup(sc, &sc->sc_txdma, &sc->sc_txbuf,
			"tx", ath_txbuf, ATH_TXDESC);
	if (error != 0) {
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);
		return error;
	}

	error = ath_descdma_setup(sc, &sc->sc_bdma, &sc->sc_bbuf,
			"beacon", ATH_BCBUF, 1);
	if (error != 0) {
		ath_descdma_cleanup(sc, &sc->sc_txdma, &sc->sc_txbuf);
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);
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
	if (sc->sc_rxdma.dd_desc_len != 0)
		ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);
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

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: an %p\n", __func__, an);
	return &an->an_node;
}

static void
ath_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
        struct ath_softc *sc = ic->ic_ifp->if_softc;

	DPRINTF(sc, ATH_DEBUG_NODE, "%s: ni %p\n", __func__, ni);

	ath_rate_node_cleanup(sc, ATH_NODE(ni));
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
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: no mbuf/cluster\n", __func__);
			sc->sc_stats.ast_rx_nombuf++;
			return ENOMEM;
		}
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

		error = bus_dmamap_load_mbuf_sg(sc->sc_dmat,
					     bf->bf_dmamap, m,
					     bf->bf_segs, &bf->bf_nseg,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			DPRINTF(sc, ATH_DEBUG_ANY,
			    "%s: bus_dmamap_load_mbuf_sg failed; error %d\n",
			    __func__, error);
			sc->sc_stats.ast_rx_busdma++;
			m_freem(m);
			return error;
		}
		KASSERT(bf->bf_nseg == 1,
			("multi-segment packet; nseg %u", bf->bf_nseg));
		bf->bf_m = m;
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREREAD);

	/*
	 * Setup descriptors.  For receive we always terminate
	 * the descriptor list with a self-linked entry so we'll
	 * not get overrun under high load (as can happen with a
	 * 5212 when ANI processing enables PHY error frames).
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
	/*
	 * 11N: we can no longer afford to self link the last descriptor.
	 * MAC acknowledges BA status as long as it copies frames to host
	 * buffer (or rx fifo). This can incorrectly acknowledge packets
	 * to a sender if last desc is self-linked.
	 */
	ds = bf->bf_desc;
	if (sc->sc_rxslink)
		ds->ds_link = bf->bf_daddr;	/* link to self */
	else
		ds->ds_link = 0;		/* terminate the list */
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

/*
 * Extend 15-bit time stamp from rx descriptor to
 * a full 64-bit TSF using the specified TSF.
 */
static __inline u_int64_t
ath_extend_tsf(u_int32_t rstamp, u_int64_t tsf)
{
	if ((tsf & 0x7fff) < rstamp)
		tsf -= 0x8000;
	return ((tsf &~ 0x7fff) | rstamp);
}

/*
 * Intercept management frames to collect beacon rssi data
 * and to do ibss merges.
 */
static void
ath_recv_mgmt(struct ieee80211_node *ni, struct mbuf *m,
	int subtype, int rssi, int nf)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ath_softc *sc = vap->iv_ic->ic_ifp->if_softc;

	/*
	 * Call up first so subsequent work can use information
	 * potentially stored in the node (e.g. for ibss merge).
	 */
	ATH_VAP(vap)->av_recv_mgmt(ni, m, subtype, rssi, nf);
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_BEACON:
		/* update rssi statistics for use by the hal */
		ATH_RSSI_LPF(sc->sc_halstats.ns_avgbrssi, rssi);
		if (sc->sc_syncbeacon &&
		    ni == vap->iv_bss && vap->iv_state == IEEE80211_S_RUN) {
			/*
			 * Resync beacon timers using the tsf of the beacon
			 * frame we just received.
			 */
			ath_beacon_config(sc, vap);
		}
		/* fall thru... */
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		if (vap->iv_opmode == IEEE80211_M_IBSS &&
		    vap->iv_state == IEEE80211_S_RUN) {
			uint32_t rstamp = sc->sc_lastrs->rs_tstamp;
			uint64_t tsf = ath_extend_tsf(rstamp,
				ath_hal_gettsf64(sc->sc_ah));
			/*
			 * Handle ibss merge as needed; check the tsf on the
			 * frame before attempting the merge.  The 802.11 spec
			 * says the station should change it's bssid to match
			 * the oldest station with the same ssid, where oldest
			 * is determined by the tsf.  Note that hardware
			 * reconfiguration happens through callback to
			 * ath_newstate as the state machine will go from
			 * RUN -> RUN when this happens.
			 */
			if (le64toh(ni->ni_tstamp.tsf) >= tsf) {
				DPRINTF(sc, ATH_DEBUG_STATE,
				    "ibss merge, rstamp %u tsf %ju "
				    "tstamp %ju\n", rstamp, (uintmax_t)tsf,
				    (uintmax_t)ni->ni_tstamp.tsf);
				(void) ieee80211_ibss_merge(ni);
			}
		}
		break;
	}
}

/*
 * Set the default antenna.
 */
static void
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
ath_rx_tap(struct ifnet *ifp, struct mbuf *m,
	const struct ath_rx_status *rs, u_int64_t tsf, int16_t nf)
{
#define	CHAN_HT20	htole32(IEEE80211_CHAN_HT20)
#define	CHAN_HT40U	htole32(IEEE80211_CHAN_HT40U)
#define	CHAN_HT40D	htole32(IEEE80211_CHAN_HT40D)
#define	CHAN_HT		(CHAN_HT20|CHAN_HT40U|CHAN_HT40D)
	struct ath_softc *sc = ifp->if_softc;
	const HAL_RATE_TABLE *rt;
	uint8_t rix;

	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	rix = rt->rateCodeToIndex[rs->rs_rate];
	sc->sc_rx_th.wr_rate = sc->sc_hwmap[rix].ieeerate;
	sc->sc_rx_th.wr_flags = sc->sc_hwmap[rix].rxflags;
#ifdef AH_SUPPORT_AR5416
	sc->sc_rx_th.wr_chan_flags &= ~CHAN_HT;
	if (sc->sc_rx_th.wr_rate & IEEE80211_RATE_MCS) {	/* HT rate */
		struct ieee80211com *ic = ifp->if_l2com;

		if ((rs->rs_flags & HAL_RX_2040) == 0)
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT20;
		else if (IEEE80211_IS_CHAN_HT40U(ic->ic_curchan))
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT40U;
		else
			sc->sc_rx_th.wr_chan_flags |= CHAN_HT40D;
		if ((rs->rs_flags & HAL_RX_GI) == 0)
			sc->sc_rx_th.wr_flags |= IEEE80211_RADIOTAP_F_SHORTGI;
	}
#endif
	sc->sc_rx_th.wr_tsf = htole64(ath_extend_tsf(rs->rs_tstamp, tsf));
	if (rs->rs_status & HAL_RXERR_CRC)
		sc->sc_rx_th.wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
	/* XXX propagate other error flags from descriptor */
	sc->sc_rx_th.wr_antnoise = nf;
	sc->sc_rx_th.wr_antsignal = nf + rs->rs_rssi;
	sc->sc_rx_th.wr_antenna = rs->rs_antenna;
#undef CHAN_HT
#undef CHAN_HT20
#undef CHAN_HT40U
#undef CHAN_HT40D
}

static void
ath_handle_micerror(struct ieee80211com *ic,
	struct ieee80211_frame *wh, int keyix)
{
	struct ieee80211_node *ni;

	/* XXX recheck MIC to deal w/ chips that lie */
	/* XXX discard MIC errors on !data frames */
	ni = ieee80211_find_rxnode(ic, (const struct ieee80211_frame_min *) wh);
	if (ni != NULL) {
		ieee80211_notify_michael_failure(ni->ni_vap, wh, keyix);
		ieee80211_free_node(ni);
	}
}

static void
ath_rx_proc(void *arg, int npending)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_rxdma.dd_desc + \
		((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))
	struct ath_softc *sc = arg;
	struct ath_buf *bf;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	struct ath_rx_status *rs;
	struct mbuf *m;
	struct ieee80211_node *ni;
	int len, type, ngood;
	HAL_STATUS status;
	int16_t nf;
	u_int64_t tsf;

	DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: pending %u\n", __func__, npending);
	ngood = 0;
	nf = ath_hal_getchannoise(ah, sc->sc_curchan);
	sc->sc_stats.ast_rx_noise = nf;
	tsf = ath_hal_gettsf64(ah);
	do {
		bf = STAILQ_FIRST(&sc->sc_rxbuf);
		if (sc->sc_rxslink && bf == NULL) {	/* NB: shouldn't happen */
			if_printf(ifp, "%s: no buffer!\n", __func__);
			break;
		} else if (bf == NULL) {
			/*
			 * End of List:
			 * this can happen for non-self-linked RX chains
			 */
			sc->sc_stats.ast_rx_hitqueueend++;
			break;
		}
		m = bf->bf_m;
		if (m == NULL) {		/* NB: shouldn't happen */
			/*
			 * If mbuf allocation failed previously there
			 * will be no mbuf; try again to re-populate it.
			 */
			/* XXX make debug msg */
			if_printf(ifp, "%s: no mbuf!\n", __func__);
			STAILQ_REMOVE_HEAD(&sc->sc_rxbuf, bf_list);
			goto rx_next;
		}
		ds = bf->bf_desc;
		if (ds->ds_link == bf->bf_daddr) {
			/* NB: never process the self-linked entry at the end */
			sc->sc_stats.ast_rx_hitqueueend++;
			break;
		}
		/* XXX sync descriptor memory */
		/*
		 * Must provide the virtual address of the current
		 * descriptor, the physical address, and the virtual
		 * address of the next descriptor in the h/w chain.
		 * This allows the HAL to look ahead to see if the
		 * hardware is done with a descriptor by checking the
		 * done bit in the following descriptor and the address
		 * of the current descriptor the DMA engine is working
		 * on.  All this is necessary because of our use of
		 * a self-linked list to avoid rx overruns.
		 */
		rs = &bf->bf_status.ds_rxstat;
		status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link), rs);
#ifdef ATH_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RECV_DESC)
			ath_printrxbuf(sc, bf, 0, status == HAL_OK);
#endif
		if (status == HAL_EINPROGRESS)
			break;
		STAILQ_REMOVE_HEAD(&sc->sc_rxbuf, bf_list);

		/* These aren't specifically errors */
		if (rs->rs_flags & HAL_RX_GI)
			sc->sc_stats.ast_rx_halfgi++;
		if (rs->rs_flags & HAL_RX_2040)
			sc->sc_stats.ast_rx_2040++;
		if (rs->rs_flags & HAL_RX_DELIM_CRC_PRE)
			sc->sc_stats.ast_rx_pre_crc_err++;
		if (rs->rs_flags & HAL_RX_DELIM_CRC_POST)
			sc->sc_stats.ast_rx_post_crc_err++;
		if (rs->rs_flags & HAL_RX_DECRYPT_BUSY)
			sc->sc_stats.ast_rx_decrypt_busy_err++;
		if (rs->rs_flags & HAL_RX_HI_RX_CHAIN)
			sc->sc_stats.ast_rx_hi_rx_chain++;

		if (rs->rs_status != 0) {
			if (rs->rs_status & HAL_RXERR_CRC)
				sc->sc_stats.ast_rx_crcerr++;
			if (rs->rs_status & HAL_RXERR_FIFO)
				sc->sc_stats.ast_rx_fifoerr++;
			if (rs->rs_status & HAL_RXERR_PHY) {
				sc->sc_stats.ast_rx_phyerr++;
				/* Process DFS radar events */
				if ((rs->rs_phyerr == HAL_PHYERR_RADAR) ||
				    (rs->rs_phyerr == HAL_PHYERR_FALSE_RADAR_EXT)) {
					/* Since we're touching the frame data, sync it */
					bus_dmamap_sync(sc->sc_dmat,
					    bf->bf_dmamap,
					    BUS_DMASYNC_POSTREAD);
					/* Now pass it to the radar processing code */
					ath_dfs_process_phy_err(sc, mtod(m, char *), tsf, rs);
				}

				/* Be suitably paranoid about receiving phy errors out of the stats array bounds */
				if (rs->rs_phyerr < 64)
					sc->sc_stats.ast_rx_phy[rs->rs_phyerr]++;
				goto rx_error;	/* NB: don't count in ierrors */
			}
			if (rs->rs_status & HAL_RXERR_DECRYPT) {
				/*
				 * Decrypt error.  If the error occurred
				 * because there was no hardware key, then
				 * let the frame through so the upper layers
				 * can process it.  This is necessary for 5210
				 * parts which have no way to setup a ``clear''
				 * key cache entry.
				 *
				 * XXX do key cache faulting
				 */
				if (rs->rs_keyix == HAL_RXKEYIX_INVALID)
					goto rx_accept;
				sc->sc_stats.ast_rx_badcrypt++;
			}
			if (rs->rs_status & HAL_RXERR_MIC) {
				sc->sc_stats.ast_rx_badmic++;
				/*
				 * Do minimal work required to hand off
				 * the 802.11 header for notification.
				 */
				/* XXX frag's and qos frames */
				len = rs->rs_datalen;
				if (len >= sizeof (struct ieee80211_frame)) {
					bus_dmamap_sync(sc->sc_dmat,
					    bf->bf_dmamap,
					    BUS_DMASYNC_POSTREAD);
					ath_handle_micerror(ic,
					    mtod(m, struct ieee80211_frame *),
					    sc->sc_splitmic ?
						rs->rs_keyix-32 : rs->rs_keyix);
				}
			}
			ifp->if_ierrors++;
rx_error:
			/*
			 * Cleanup any pending partial frame.
			 */
			if (sc->sc_rxpending != NULL) {
				m_freem(sc->sc_rxpending);
				sc->sc_rxpending = NULL;
			}
			/*
			 * When a tap is present pass error frames
			 * that have been requested.  By default we
			 * pass decrypt+mic errors but others may be
			 * interesting (e.g. crc).
			 */
			if (ieee80211_radiotap_active(ic) &&
			    (rs->rs_status & sc->sc_monpass)) {
				bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
				    BUS_DMASYNC_POSTREAD);
				/* NB: bpf needs the mbuf length setup */
				len = rs->rs_datalen;
				m->m_pkthdr.len = m->m_len = len;
				ath_rx_tap(ifp, m, rs, tsf, nf);
				ieee80211_radiotap_rx_all(ic, m);
			}
			/* XXX pass MIC errors up for s/w reclaculation */
			goto rx_next;
		}
rx_accept:
		/*
		 * Sync and unmap the frame.  At this point we're
		 * committed to passing the mbuf somewhere so clear
		 * bf_m; this means a new mbuf must be allocated
		 * when the rx descriptor is setup again to receive
		 * another frame.
		 */
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		bf->bf_m = NULL;

		len = rs->rs_datalen;
		m->m_len = len;

		if (rs->rs_more) {
			/*
			 * Frame spans multiple descriptors; save
			 * it for the next completed descriptor, it
			 * will be used to construct a jumbogram.
			 */
			if (sc->sc_rxpending != NULL) {
				/* NB: max frame size is currently 2 clusters */
				sc->sc_stats.ast_rx_toobig++;
				m_freem(sc->sc_rxpending);
			}
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = len;
			sc->sc_rxpending = m;
			goto rx_next;
		} else if (sc->sc_rxpending != NULL) {
			/*
			 * This is the second part of a jumbogram,
			 * chain it to the first mbuf, adjust the
			 * frame length, and clear the rxpending state.
			 */
			sc->sc_rxpending->m_next = m;
			sc->sc_rxpending->m_pkthdr.len += len;
			m = sc->sc_rxpending;
			sc->sc_rxpending = NULL;
		} else {
			/*
			 * Normal single-descriptor receive; setup
			 * the rcvif and packet length.
			 */
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = len;
		}

		ifp->if_ipackets++;
		sc->sc_stats.ast_ant_rx[rs->rs_antenna]++;

		/*
		 * Populate the rx status block.  When there are bpf
		 * listeners we do the additional work to provide
		 * complete status.  Otherwise we fill in only the
		 * material required by ieee80211_input.  Note that
		 * noise setting is filled in above.
		 */
		if (ieee80211_radiotap_active(ic))
			ath_rx_tap(ifp, m, rs, tsf, nf);

		/*
		 * From this point on we assume the frame is at least
		 * as large as ieee80211_frame_min; verify that.
		 */
		if (len < IEEE80211_MIN_LEN) {
			if (!ieee80211_radiotap_active(ic)) {
				DPRINTF(sc, ATH_DEBUG_RECV,
				    "%s: short packet %d\n", __func__, len);
				sc->sc_stats.ast_rx_tooshort++;
			} else {
				/* NB: in particular this captures ack's */
				ieee80211_radiotap_rx_all(ic, m);
			}
			m_freem(m);
			goto rx_next;
		}

		if (IFF_DUMPPKTS(sc, ATH_DEBUG_RECV)) {
			const HAL_RATE_TABLE *rt = sc->sc_currates;
			uint8_t rix = rt->rateCodeToIndex[rs->rs_rate];

			ieee80211_dump_pkt(ic, mtod(m, caddr_t), len,
			    sc->sc_hwmap[rix].ieeerate, rs->rs_rssi);
		}

		m_adj(m, -IEEE80211_CRC_LEN);

		/*
		 * Locate the node for sender, track state, and then
		 * pass the (referenced) node up to the 802.11 layer
		 * for its use.
		 */
		ni = ieee80211_find_rxnode_withkey(ic,
			mtod(m, const struct ieee80211_frame_min *),
			rs->rs_keyix == HAL_RXKEYIX_INVALID ?
				IEEE80211_KEYIX_NONE : rs->rs_keyix);
		sc->sc_lastrs = rs;

		if (rs->rs_isaggr)
			sc->sc_stats.ast_rx_agg++;

		if (ni != NULL) {
			/*
 			 * Only punt packets for ampdu reorder processing for
			 * 11n nodes; net80211 enforces that M_AMPDU is only
			 * set for 11n nodes.
 			 */
			if (ni->ni_flags & IEEE80211_NODE_HT)
				m->m_flags |= M_AMPDU;

			/*
			 * Sending station is known, dispatch directly.
			 */
			type = ieee80211_input(ni, m, rs->rs_rssi, nf);
			ieee80211_free_node(ni);
			/*
			 * Arrange to update the last rx timestamp only for
			 * frames from our ap when operating in station mode.
			 * This assumes the rx key is always setup when
			 * associated.
			 */
			if (ic->ic_opmode == IEEE80211_M_STA &&
			    rs->rs_keyix != HAL_RXKEYIX_INVALID)
				ngood++;
		} else {
			type = ieee80211_input_all(ic, m, rs->rs_rssi, nf);
		}
		/*
		 * Track rx rssi and do any rx antenna management.
		 */
		ATH_RSSI_LPF(sc->sc_halstats.ns_avgrssi, rs->rs_rssi);
		if (sc->sc_diversity) {
			/*
			 * When using fast diversity, change the default rx
			 * antenna if diversity chooses the other antenna 3
			 * times in a row.
			 */
			if (sc->sc_defant != rs->rs_antenna) {
				if (++sc->sc_rxotherant >= 3)
					ath_setdefantenna(sc, rs->rs_antenna);
			} else
				sc->sc_rxotherant = 0;
		}

		/* Newer school diversity - kite specific for now */
		/* XXX perhaps migrate the normal diversity code to this? */
		if ((ah)->ah_rxAntCombDiversity)
			(*(ah)->ah_rxAntCombDiversity)(ah, rs, ticks, hz);

		if (sc->sc_softled) {
			/*
			 * Blink for any data frame.  Otherwise do a
			 * heartbeat-style blink when idle.  The latter
			 * is mainly for station mode where we depend on
			 * periodic beacon frames to trigger the poll event.
			 */
			if (type == IEEE80211_FC0_TYPE_DATA) {
				const HAL_RATE_TABLE *rt = sc->sc_currates;
				ath_led_event(sc,
				    rt->rateCodeToIndex[rs->rs_rate]);
			} else if (ticks - sc->sc_ledevent >= sc->sc_ledidle)
				ath_led_event(sc, 0);
		}
rx_next:
		STAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
	} while (ath_rxbuf_init(sc, bf) == 0);

	/* rx signal state monitoring */
	ath_hal_rxmonitor(ah, &sc->sc_halstats, sc->sc_curchan);
	if (ngood)
		sc->sc_lastrx = tsf;

	/* Queue DFS tasklet if needed */
	if (ath_dfs_tasklet_needed(sc, sc->sc_curchan))
		taskqueue_enqueue(sc->sc_tq, &sc->sc_dfstask);

	if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0) {
#ifdef IEEE80211_SUPPORT_SUPERG
		ieee80211_ff_age_all(ic, 100);
#endif
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			ath_start(ifp);
	}
#undef PA2DESC
}

static void
ath_txq_init(struct ath_softc *sc, struct ath_txq *txq, int qnum)
{
	txq->axq_qnum = qnum;
	txq->axq_ac = 0;
	txq->axq_depth = 0;
	txq->axq_intrcnt = 0;
	txq->axq_link = NULL;
	STAILQ_INIT(&txq->axq_q);
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
	qi.tqi_qflags = HAL_TXQ_TXEOLINT_ENABLE | HAL_TXQ_TXDESCINT_ENABLE;
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
		qi.tqi_qflags = HAL_TXQ_TXOKINT_ENABLE
			      | HAL_TXQ_TXERRINT_ENABLE
			      | HAL_TXQ_TXDESCINT_ENABLE
			      | HAL_TXQ_TXURNINT_ENABLE
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
static int
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
	ATH_TXQ_LOCK_DESTROY(txq);
	sc->sc_txqsetup &= ~(1<<txq->axq_qnum);
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

/*
 * Process completed xmit descriptors from the specified queue.
 */
static int
ath_tx_processq(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_buf *bf, *last;
	struct ath_desc *ds, *ds0;
	struct ath_tx_status *ts;
	struct ieee80211_node *ni;
	struct ath_node *an;
	int sr, lr, pri, nacked;
	HAL_STATUS status;

	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: tx queue %u head %p link %p\n",
		__func__, txq->axq_qnum,
		(caddr_t)(uintptr_t) ath_hal_gettxbuf(sc->sc_ah, txq->axq_qnum),
		txq->axq_link);
	nacked = 0;
	for (;;) {
		ATH_TXQ_LOCK(txq);
		txq->axq_intrcnt = 0;	/* reset periodic desc intr count */
		bf = STAILQ_FIRST(&txq->axq_q);
		if (bf == NULL) {
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ds0 = &bf->bf_desc[0];
		ds = &bf->bf_desc[bf->bf_nseg - 1];
		ts = &bf->bf_status.ds_txstat;
		status = ath_hal_txprocdesc(ah, ds, ts);
#ifdef ATH_DEBUG
		if (sc->sc_debug & ATH_DEBUG_XMIT_DESC)
			ath_printtxbuf(sc, bf, txq->axq_qnum, 0,
			    status == HAL_OK);
#endif
		if (status == HAL_EINPROGRESS) {
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ATH_TXQ_REMOVE_HEAD(txq, bf_list);
#ifdef IEEE80211_SUPPORT_TDMA
		if (txq->axq_depth > 0) {
			/*
			 * More frames follow.  Mark the buffer busy
			 * so it's not re-used while the hardware may
			 * still re-read the link field in the descriptor.
			 */
			bf->bf_flags |= ATH_BUF_BUSY;
		} else
#else
		if (txq->axq_depth == 0)
#endif
			txq->axq_link = NULL;
		ATH_TXQ_UNLOCK(txq);

		ni = bf->bf_node;
		if (ni != NULL) {
			an = ATH_NODE(ni);
			if (ts->ts_status == 0) {
				u_int8_t txant = ts->ts_antenna;
				sc->sc_stats.ast_ant_tx[txant]++;
				sc->sc_ant_tx[txant]++;
				if (ts->ts_finaltsi != 0)
					sc->sc_stats.ast_tx_altrate++;
				pri = M_WME_GETAC(bf->bf_m);
				if (pri >= WME_AC_VO)
					ic->ic_wme.wme_hipri_traffic++;
				if ((bf->bf_txflags & HAL_TXDESC_NOACK) == 0)
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

				/* XXX HAL_TX_DATA_UNDERRUN */
				/* XXX HAL_TX_DELIM_UNDERRUN */

				if (bf->bf_m->m_flags & M_FF)
					sc->sc_stats.ast_ff_txerr++;
			}
			/* XXX when is this valid? */
			if (ts->ts_status & HAL_TX_DESC_CFG_ERR)
				sc->sc_stats.ast_tx_desccfgerr++;

			sr = ts->ts_shortretry;
			lr = ts->ts_longretry;
			sc->sc_stats.ast_tx_shortretry += sr;
			sc->sc_stats.ast_tx_longretry += lr;
			/*
			 * Hand the descriptor to the rate control algorithm.
			 */
			if ((ts->ts_status & HAL_TXERR_FILT) == 0 &&
			    (bf->bf_txflags & HAL_TXDESC_NOACK) == 0) {
				/*
				 * If frame was ack'd update statistics,
				 * including the last rx time used to
				 * workaround phantom bmiss interrupts.
				 */
				if (ts->ts_status == 0) {
					nacked++;
					sc->sc_stats.ast_tx_rssi = ts->ts_rssi;
					ATH_RSSI_LPF(sc->sc_halstats.ns_avgtxrssi,
						ts->ts_rssi);
				}
				ath_rate_tx_complete(sc, an, bf);
			}
			/*
			 * Do any tx complete callback.  Note this must
			 * be done before releasing the node reference.
			 */
			if (bf->bf_m->m_flags & M_TXCB)
				ieee80211_process_callback(ni, bf->bf_m,
				    (bf->bf_txflags & HAL_TXDESC_NOACK) == 0 ?
				        ts->ts_status : HAL_TXERR_XRETRY);
			ieee80211_free_node(ni);
		}
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);

		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_node = NULL;

		ATH_TXBUF_LOCK(sc);
		last = STAILQ_LAST(&sc->sc_txbuf, ath_buf, bf_list);
		if (last != NULL)
			last->bf_flags &= ~ATH_BUF_BUSY;
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
	}
#ifdef IEEE80211_SUPPORT_SUPERG
	/*
	 * Flush fast-frame staging queue when traffic slows.
	 */
	if (txq->axq_depth <= 1)
		ieee80211_ff_flush(ic, txq->axq_ac);
#endif
	return nacked;
}

static __inline int
txqactive(struct ath_hal *ah, int qnum)
{
	u_int32_t txqs = 1<<qnum;
	ath_hal_gettxintrtxqs(ah, &txqs);
	return (txqs & (1<<qnum));
}

/*
 * Deferred processing of transmit interrupt; special-cased
 * for a single hardware transmit queue (e.g. 5210 and 5211).
 */
static void
ath_tx_proc_q0(void *arg, int npending)
{
	struct ath_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (txqactive(sc->sc_ah, 0) && ath_tx_processq(sc, &sc->sc_txq[0]))
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);
	if (txqactive(sc->sc_ah, sc->sc_cabq->axq_qnum))
		ath_tx_processq(sc, sc->sc_cabq);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->sc_wd_timer = 0;

	if (sc->sc_softled)
		ath_led_event(sc, sc->sc_txrix);

	ath_start(ifp);
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

	/*
	 * Process each active queue.
	 */
	nacked = 0;
	if (txqactive(sc->sc_ah, 0))
		nacked += ath_tx_processq(sc, &sc->sc_txq[0]);
	if (txqactive(sc->sc_ah, 1))
		nacked += ath_tx_processq(sc, &sc->sc_txq[1]);
	if (txqactive(sc->sc_ah, 2))
		nacked += ath_tx_processq(sc, &sc->sc_txq[2]);
	if (txqactive(sc->sc_ah, 3))
		nacked += ath_tx_processq(sc, &sc->sc_txq[3]);
	if (txqactive(sc->sc_ah, sc->sc_cabq->axq_qnum))
		ath_tx_processq(sc, sc->sc_cabq);
	if (nacked)
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->sc_wd_timer = 0;

	if (sc->sc_softled)
		ath_led_event(sc, sc->sc_txrix);

	ath_start(ifp);
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

	/*
	 * Process each active queue.
	 */
	nacked = 0;
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i) && txqactive(sc->sc_ah, i))
			nacked += ath_tx_processq(sc, &sc->sc_txq[i]);
	if (nacked)
		sc->sc_lastrx = ath_hal_gettsf64(sc->sc_ah);

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->sc_wd_timer = 0;

	if (sc->sc_softled)
		ath_led_event(sc, sc->sc_txrix);

	ath_start(ifp);
}

static void
ath_tx_draintxq(struct ath_softc *sc, struct ath_txq *txq)
{
#ifdef ATH_DEBUG
	struct ath_hal *ah = sc->sc_ah;
#endif
	struct ieee80211_node *ni;
	struct ath_buf *bf;
	u_int ix;

	/*
	 * NB: this assumes output has been stopped and
	 *     we do not need to block ath_tx_proc
	 */
	ATH_TXBUF_LOCK(sc);
	bf = STAILQ_LAST(&sc->sc_txbuf, ath_buf, bf_list);
	if (bf != NULL)
		bf->bf_flags &= ~ATH_BUF_BUSY;
	ATH_TXBUF_UNLOCK(sc);
	for (ix = 0;; ix++) {
		ATH_TXQ_LOCK(txq);
		bf = STAILQ_FIRST(&txq->axq_q);
		if (bf == NULL) {
			txq->axq_link = NULL;
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		ATH_TXQ_REMOVE_HEAD(txq, bf_list);
		ATH_TXQ_UNLOCK(txq);
#ifdef ATH_DEBUG
		if (sc->sc_debug & ATH_DEBUG_RESET) {
			struct ieee80211com *ic = sc->sc_ifp->if_l2com;

			ath_printtxbuf(sc, bf, txq->axq_qnum, ix,
				ath_hal_txprocdesc(ah, bf->bf_desc,
				    &bf->bf_status.ds_txstat) == HAL_OK);
			ieee80211_dump_pkt(ic, mtod(bf->bf_m, const uint8_t *),
			    bf->bf_m->m_len, 0, -1);
		}
#endif /* ATH_DEBUG */
		bus_dmamap_unload(sc->sc_dmat, bf->bf_dmamap);
		ni = bf->bf_node;
		bf->bf_node = NULL;
		if (ni != NULL) {
			/*
			 * Do any callback and reclaim the node reference.
			 */
			if (bf->bf_m->m_flags & M_TXCB)
				ieee80211_process_callback(ni, bf->bf_m, -1);
			ieee80211_free_node(ni);
		}
		m_freem(bf->bf_m);
		bf->bf_m = NULL;
		bf->bf_flags &= ~ATH_BUF_BUSY;

		ATH_TXBUF_LOCK(sc);
		STAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
	}
}

static void
ath_tx_stopdma(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;

	DPRINTF(sc, ATH_DEBUG_RESET, "%s: tx queue [%u] %p, link %p\n",
	    __func__, txq->axq_qnum,
	    (caddr_t)(uintptr_t) ath_hal_gettxbuf(ah, txq->axq_qnum),
	    txq->axq_link);
	(void) ath_hal_stoptxdma(ah, txq->axq_qnum);
}

/*
 * Drain the transmit queues and reclaim resources.
 */
static void
ath_draintxq(struct ath_softc *sc)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = sc->sc_ifp;
	int i;

	/* XXX return value */
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
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
		if (ATH_TXQ_SETUP(sc, i))
			ath_tx_draintxq(sc, &sc->sc_txq[i]);
#ifdef ATH_DEBUG
	if (sc->sc_debug & ATH_DEBUG_RESET) {
		struct ath_buf *bf = STAILQ_FIRST(&sc->sc_bbuf);
		if (bf != NULL && bf->bf_m != NULL) {
			ath_printtxbuf(sc, bf, sc->sc_bhalq, 0,
				ath_hal_txprocdesc(ah, bf->bf_desc,
				    &bf->bf_status.ds_txstat) == HAL_OK);
			ieee80211_dump_pkt(ifp->if_l2com,
			    mtod(bf->bf_m, const uint8_t *), bf->bf_m->m_len,
			    0, -1);
		}
	}
#endif /* ATH_DEBUG */
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->sc_wd_timer = 0;
}

/*
 * Disable the receive h/w in preparation for a reset.
 */
static void
ath_stoprecv(struct ath_softc *sc)
{
#define	PA2DESC(_sc, _pa) \
	((struct ath_desc *)((caddr_t)(_sc)->sc_rxdma.dd_desc + \
		((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_stoppcurecv(ah);	/* disable PCU */
	ath_hal_setrxfilter(ah, 0);	/* clear recv filter */
	ath_hal_stopdmarecv(ah);	/* disable DMA engine */
	DELAY(3000);			/* 3ms is long enough for 1 frame */
#ifdef ATH_DEBUG
	if (sc->sc_debug & (ATH_DEBUG_RESET | ATH_DEBUG_FATAL)) {
		struct ath_buf *bf;
		u_int ix;

		printf("%s: rx queue %p, link %p\n", __func__,
			(caddr_t)(uintptr_t) ath_hal_getrxbuf(ah), sc->sc_rxlink);
		ix = 0;
		STAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
			struct ath_desc *ds = bf->bf_desc;
			struct ath_rx_status *rs = &bf->bf_status.ds_rxstat;
			HAL_STATUS status = ath_hal_rxprocdesc(ah, ds,
				bf->bf_daddr, PA2DESC(sc, ds->ds_link), rs);
			if (status == HAL_OK || (sc->sc_debug & ATH_DEBUG_FATAL))
				ath_printrxbuf(sc, bf, ix, status == HAL_OK);
			ix++;
		}
	}
#endif
	if (sc->sc_rxpending != NULL) {
		m_freem(sc->sc_rxpending);
		sc->sc_rxpending = NULL;
	}
	sc->sc_rxlink = NULL;		/* just in case */
#undef PA2DESC
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
	sc->sc_rxpending = NULL;
	STAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
		int error = ath_rxbuf_init(sc, bf);
		if (error != 0) {
			DPRINTF(sc, ATH_DEBUG_RECV,
				"%s: ath_rxbuf_init failed %d\n",
				__func__, error);
			return error;
		}
	}

	bf = STAILQ_FIRST(&sc->sc_rxbuf);
	ath_hal_putrxbuf(ah, bf->bf_daddr);
	ath_hal_rxena(ah);		/* enable recv descriptors */
	ath_mode_init(sc);		/* set filters, etc. */
	ath_hal_startpcurecv(ah);	/* re-enable PCU/DMA engine */
	return 0;
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
		ath_hal_intrset(ah, 0);		/* disable interrupts */
		ath_draintxq(sc);		/* clear pending tx frames */
		ath_stoprecv(sc);		/* turn off frame recv */
		if (!ath_hal_reset(ah, sc->sc_opmode, chan, AH_TRUE, &status)) {
			if_printf(ifp, "%s: unable to reset "
			    "channel %u (%u MHz, flags 0x%x), hal status %u\n",
			    __func__, ieee80211_chan2ieee(ic, chan),
			    chan->ic_freq, chan->ic_flags, status);
			return EIO;
		}
		sc->sc_diversity = ath_hal_getdiversity(ah);

		/* Let DFS at it in case it's a DFS channel */
		ath_dfs_radar_enable(sc, ic->ic_curchan);

		/*
		 * Re-enable rx framework.
		 */
		if (ath_startrecv(sc) != 0) {
			if_printf(ifp, "%s: unable to restart recv logic\n",
			    __func__);
			return EIO;
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
		ath_hal_intrset(ah, sc->sc_imask);
	}
	return 0;
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
	HAL_BOOL longCal, isCalDone;
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
			ath_reset(ifp);
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

	sc->sc_scanning = 1;
	sc->sc_syncbeacon = 0;
	rfilt = ath_calcrxfilter(sc);
	ath_hal_setrxfilter(ah, rfilt);
	ath_hal_setassocid(ah, ifp->if_broadcastaddr, 0);

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

	sc->sc_scanning = 0;
	rfilt = ath_calcrxfilter(sc);
	ath_hal_setrxfilter(ah, rfilt);
	ath_hal_setassocid(ah, sc->sc_curbssid, sc->sc_curaid);

	ath_hal_process_noisefloor(ah);

	DPRINTF(sc, ATH_DEBUG_STATE, "%s: RX filter 0x%x bssid %s aid 0x%x\n",
		 __func__, rfilt, ether_sprintf(sc->sc_curbssid),
		 sc->sc_curaid);
}

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
	if (!sc->sc_scanning && ic->ic_curchan == ic->ic_bsschan)
		sc->sc_syncbeacon = 1;
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

	ni = vap->iv_bss;
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

	if (nstate == IEEE80211_S_RUN) {
		/* NB: collect bss node again, it may have changed */
		ni = vap->iv_bss;

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
			 */
			sc->sc_syncbeacon = 1;
			if (csa_run_transition)
				ath_beacon_config(sc, vap);
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
		ath_keyset(sc, &ni->ni_ucastkey, vap->iv_bss);
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

static void
ath_led_done(void *arg)
{
	struct ath_softc *sc = arg;

	sc->sc_blinking = 0;
}

/*
 * Turn the LED off: flip the pin and then set a timer so no
 * update will happen for the specified duration.
 */
static void
ath_led_off(void *arg)
{
	struct ath_softc *sc = arg;

	ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin, !sc->sc_ledon);
	callout_reset(&sc->sc_ledtimer, sc->sc_ledoff, ath_led_done, sc);
}

/*
 * Blink the LED according to the specified on/off times.
 */
static void
ath_led_blink(struct ath_softc *sc, int on, int off)
{
	DPRINTF(sc, ATH_DEBUG_LED, "%s: on %u off %u\n", __func__, on, off);
	ath_hal_gpioset(sc->sc_ah, sc->sc_ledpin, sc->sc_ledon);
	sc->sc_blinking = 1;
	sc->sc_ledoff = off;
	callout_reset(&sc->sc_ledtimer, on, ath_led_off, sc);
}

static void
ath_led_event(struct ath_softc *sc, int rix)
{
	sc->sc_ledevent = ticks;	/* time of last event */
	if (sc->sc_blinking)		/* don't interrupt active blink */
		return;
	ath_led_blink(sc, sc->sc_hwmap[rix].ledon, sc->sc_hwmap[rix].ledoff);
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

	if (sc->sc_wd_timer != 0 && --sc->sc_wd_timer == 0) {
		struct ifnet *ifp = sc->sc_ifp;
		uint32_t hangs;

		if (ath_hal_gethangstate(sc->sc_ah, 0xffff, &hangs) &&
		    hangs != 0) {
			if_printf(ifp, "%s hang detected (0x%x)\n",
			    hangs & 0xff ? "bb" : "mac", hangs);
		} else
			if_printf(ifp, "device timeout\n");
		ath_reset(ifp);
		ifp->if_oerrors++;
		sc->sc_stats.ast_watchdog++;
	}
	callout_schedule(&sc->sc_wd_ch, hz);
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
	case SIOCZATHSTATS:
		error = priv_check(curthread, PRIV_DRIVER);
		if (error == 0)
			memset(&sc->sc_stats, 0, sizeof(sc->sc_stats));
		break;
#ifdef ATH_DIAGAPI
	case SIOCGATHDIAG:
		error = ath_ioctl_diag(sc, (struct ath_diag *) ifr);
		break;
	case SIOCGATHPHYERR:
		error = ath_ioctl_phyerr(sc,(struct ath_diag*) ifr);
		break;
#endif
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

#ifdef IEEE80211_SUPPORT_TDMA
static __inline uint32_t
ath_hal_getnexttbtt(struct ath_hal *ah)
{
#define	AR_TIMER0	0x8028
	return OS_REG_READ(ah, AR_TIMER0);
}

static __inline void
ath_hal_adjusttsf(struct ath_hal *ah, int32_t tsfdelta)
{
	/* XXX handle wrap/overflow */
	OS_REG_WRITE(ah, AR_TSF_L32, OS_REG_READ(ah, AR_TSF_L32) + tsfdelta);
}

static void
ath_tdma_settimers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t bintval)
{
	struct ath_hal *ah = sc->sc_ah;
	HAL_BEACON_TIMERS bt;

	bt.bt_intval = bintval | HAL_BEACON_ENA;
	bt.bt_nexttbtt = nexttbtt;
	bt.bt_nextdba = (nexttbtt<<3) - sc->sc_tdmadbaprep;
	bt.bt_nextswba = (nexttbtt<<3) - sc->sc_tdmaswbaprep;
	bt.bt_nextatim = nexttbtt+1;
	ath_hal_beaconsettimers(ah, &bt);
}

/*
 * Calculate the beacon interval.  This is periodic in the
 * superframe for the bss.  We assume each station is configured
 * identically wrt transmit rate so the guard time we calculate
 * above will be the same on all stations.  Note we need to
 * factor in the xmit time because the hardware will schedule
 * a frame for transmit if the start of the frame is within
 * the burst time.  When we get hardware that properly kills
 * frames in the PCU we can reduce/eliminate the guard time.
 *
 * Roundup to 1024 is so we have 1 TU buffer in the guard time
 * to deal with the granularity of the nexttbtt timer.  11n MAC's
 * with 1us timer granularity should allow us to reduce/eliminate
 * this.
 */
static void
ath_tdma_bintvalsetup(struct ath_softc *sc,
	const struct ieee80211_tdma_state *tdma)
{
	/* copy from vap state (XXX check all vaps have same value?) */
	sc->sc_tdmaslotlen = tdma->tdma_slotlen;

	sc->sc_tdmabintval = roundup((sc->sc_tdmaslotlen+sc->sc_tdmaguard) *
		tdma->tdma_slotcnt, 1024);
	sc->sc_tdmabintval >>= 10;		/* TSF -> TU */
	if (sc->sc_tdmabintval & 1)
		sc->sc_tdmabintval++;

	if (tdma->tdma_slot == 0) {
		/*
		 * Only slot 0 beacons; other slots respond.
		 */
		sc->sc_imask |= HAL_INT_SWBA;
		sc->sc_tdmaswba = 0;		/* beacon immediately */
	} else {
		/* XXX all vaps must be slot 0 or slot !0 */
		sc->sc_imask &= ~HAL_INT_SWBA;
	}
}

/*
 * Max 802.11 overhead.  This assumes no 4-address frames and
 * the encapsulation done by ieee80211_encap (llc).  We also
 * include potential crypto overhead.
 */
#define	IEEE80211_MAXOVERHEAD \
	(sizeof(struct ieee80211_qosframe) \
	 + sizeof(struct llc) \
	 + IEEE80211_ADDR_LEN \
	 + IEEE80211_WEP_IVLEN \
	 + IEEE80211_WEP_KIDLEN \
	 + IEEE80211_WEP_CRCLEN \
	 + IEEE80211_WEP_MICLEN \
	 + IEEE80211_CRC_LEN)

/*
 * Setup initially for tdma operation.  Start the beacon
 * timers and enable SWBA if we are slot 0.  Otherwise
 * we wait for slot 0 to arrive so we can sync up before
 * starting to transmit.
 */
static void
ath_tdma_config(struct ath_softc *sc, struct ieee80211vap *vap)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	const struct ieee80211_txparam *tp;
	const struct ieee80211_tdma_state *tdma = NULL;
	int rix;

	if (vap == NULL) {
		vap = TAILQ_FIRST(&ic->ic_vaps);   /* XXX */
		if (vap == NULL) {
			if_printf(ifp, "%s: no vaps?\n", __func__);
			return;
		}
	}
	tp = vap->iv_bss->ni_txparms;
	/*
	 * Calculate the guard time for each slot.  This is the
	 * time to send a maximal-size frame according to the
	 * fixed/lowest transmit rate.  Note that the interface
	 * mtu does not include the 802.11 overhead so we must
	 * tack that on (ath_hal_computetxtime includes the
	 * preamble and plcp in it's calculation).
	 */
	tdma = vap->iv_tdma;
	if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
		rix = ath_tx_findrix(sc, tp->ucastrate);
	else
		rix = ath_tx_findrix(sc, tp->mcastrate);
	/* XXX short preamble assumed */
	sc->sc_tdmaguard = ath_hal_computetxtime(ah, sc->sc_currates,
		ifp->if_mtu + IEEE80211_MAXOVERHEAD, rix, AH_TRUE);

	ath_hal_intrset(ah, 0);

	ath_beaconq_config(sc);			/* setup h/w beacon q */
	if (sc->sc_setcca)
		ath_hal_setcca(ah, AH_FALSE);	/* disable CCA */
	ath_tdma_bintvalsetup(sc, tdma);	/* calculate beacon interval */
	ath_tdma_settimers(sc, sc->sc_tdmabintval,
		sc->sc_tdmabintval | HAL_BEACON_RESET_TSF);
	sc->sc_syncbeacon = 0;

	sc->sc_avgtsfdeltap = TDMA_DUMMY_MARKER;
	sc->sc_avgtsfdeltam = TDMA_DUMMY_MARKER;

	ath_hal_intrset(ah, sc->sc_imask);

	DPRINTF(sc, ATH_DEBUG_TDMA, "%s: slot %u len %uus cnt %u "
	    "bsched %u guard %uus bintval %u TU dba prep %u\n", __func__,
	    tdma->tdma_slot, tdma->tdma_slotlen, tdma->tdma_slotcnt,
	    tdma->tdma_bintval, sc->sc_tdmaguard, sc->sc_tdmabintval,
	    sc->sc_tdmadbaprep);
}

/*
 * Update tdma operation.  Called from the 802.11 layer
 * when a beacon is received from the TDMA station operating
 * in the slot immediately preceding us in the bss.  Use
 * the rx timestamp for the beacon frame to update our
 * beacon timers so we follow their schedule.  Note that
 * by using the rx timestamp we implicitly include the
 * propagation delay in our schedule.
 */
static void
ath_tdma_update(struct ieee80211_node *ni,
	const struct ieee80211_tdma_param *tdma, int changed)
{
#define	TSF_TO_TU(_h,_l) \
	((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))
#define	TU_TO_TSF(_tu)	(((u_int64_t)(_tu)) << 10)
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ath_softc *sc = ic->ic_ifp->if_softc;
	struct ath_hal *ah = sc->sc_ah;
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	u_int64_t tsf, rstamp, nextslot;
	u_int32_t txtime, nextslottu, timer0;
	int32_t tudelta, tsfdelta;
	const struct ath_rx_status *rs;
	int rix;

	sc->sc_stats.ast_tdma_update++;

	/*
	 * Check for and adopt configuration changes.
	 */
	if (changed != 0) {
		const struct ieee80211_tdma_state *ts = vap->iv_tdma;

		ath_tdma_bintvalsetup(sc, ts);
		if (changed & TDMA_UPDATE_SLOTLEN)
			ath_wme_update(ic);

		DPRINTF(sc, ATH_DEBUG_TDMA,
		    "%s: adopt slot %u slotcnt %u slotlen %u us "
		    "bintval %u TU\n", __func__,
		    ts->tdma_slot, ts->tdma_slotcnt, ts->tdma_slotlen,
		    sc->sc_tdmabintval);

		/* XXX right? */
		ath_hal_intrset(ah, sc->sc_imask);
		/* NB: beacon timers programmed below */
	}

	/* extend rx timestamp to 64 bits */
	rs = sc->sc_lastrs;
	tsf = ath_hal_gettsf64(ah);
	rstamp = ath_extend_tsf(rs->rs_tstamp, tsf);
	/*
	 * The rx timestamp is set by the hardware on completing
	 * reception (at the point where the rx descriptor is DMA'd
	 * to the host).  To find the start of our next slot we
	 * must adjust this time by the time required to send
	 * the packet just received.
	 */
	rix = rt->rateCodeToIndex[rs->rs_rate];
	txtime = ath_hal_computetxtime(ah, rt, rs->rs_datalen, rix,
	    rt->info[rix].shortPreamble);
	/* NB: << 9 is to cvt to TU and /2 */
	nextslot = (rstamp - txtime) + (sc->sc_tdmabintval << 9);
	nextslottu = TSF_TO_TU(nextslot>>32, nextslot) & HAL_BEACON_PERIOD;

	/*
	 * TIMER0 is the h/w's idea of NextTBTT (in TU's).  Convert
	 * to usecs and calculate the difference between what the
	 * other station thinks and what we have programmed.  This
	 * lets us figure how to adjust our timers to match.  The
	 * adjustments are done by pulling the TSF forward and possibly
	 * rewriting the beacon timers.
	 */
	timer0 = ath_hal_getnexttbtt(ah);
	tsfdelta = (int32_t)((nextslot % TU_TO_TSF(HAL_BEACON_PERIOD+1)) - TU_TO_TSF(timer0));

	DPRINTF(sc, ATH_DEBUG_TDMA_TIMER,
	    "tsfdelta %d avg +%d/-%d\n", tsfdelta,
	    TDMA_AVG(sc->sc_avgtsfdeltap), TDMA_AVG(sc->sc_avgtsfdeltam));

	if (tsfdelta < 0) {
		TDMA_SAMPLE(sc->sc_avgtsfdeltap, 0);
		TDMA_SAMPLE(sc->sc_avgtsfdeltam, -tsfdelta);
		tsfdelta = -tsfdelta % 1024;
		nextslottu++;
	} else if (tsfdelta > 0) {
		TDMA_SAMPLE(sc->sc_avgtsfdeltap, tsfdelta);
		TDMA_SAMPLE(sc->sc_avgtsfdeltam, 0);
		tsfdelta = 1024 - (tsfdelta % 1024);
		nextslottu++;
	} else {
		TDMA_SAMPLE(sc->sc_avgtsfdeltap, 0);
		TDMA_SAMPLE(sc->sc_avgtsfdeltam, 0);
	}
	tudelta = nextslottu - timer0;

	/*
	 * Copy sender's timetstamp into tdma ie so they can
	 * calculate roundtrip time.  We submit a beacon frame
	 * below after any timer adjustment.  The frame goes out
	 * at the next TBTT so the sender can calculate the
	 * roundtrip by inspecting the tdma ie in our beacon frame.
	 *
	 * NB: This tstamp is subtlely preserved when
	 *     IEEE80211_BEACON_TDMA is marked (e.g. when the
	 *     slot position changes) because ieee80211_add_tdma
	 *     skips over the data.
	 */
	memcpy(ATH_VAP(vap)->av_boff.bo_tdma +
		__offsetof(struct ieee80211_tdma_param, tdma_tstamp),
		&ni->ni_tstamp.data, 8);
#if 0
	DPRINTF(sc, ATH_DEBUG_TDMA_TIMER,
	    "tsf %llu nextslot %llu (%d, %d) nextslottu %u timer0 %u (%d)\n",
	    (unsigned long long) tsf, (unsigned long long) nextslot,
	    (int)(nextslot - tsf), tsfdelta,
	    nextslottu, timer0, tudelta);
#endif
	/*
	 * Adjust the beacon timers only when pulling them forward
	 * or when going back by less than the beacon interval.
	 * Negative jumps larger than the beacon interval seem to
	 * cause the timers to stop and generally cause instability.
	 * This basically filters out jumps due to missed beacons.
	 */
	if (tudelta != 0 && (tudelta > 0 || -tudelta < sc->sc_tdmabintval)) {
		ath_tdma_settimers(sc, nextslottu, sc->sc_tdmabintval);
		sc->sc_stats.ast_tdma_timers++;
	}
	if (tsfdelta > 0) {
		ath_hal_adjusttsf(ah, tsfdelta);
		sc->sc_stats.ast_tdma_tsf++;
	}
	ath_tdma_beacon_send(sc, vap);		/* prepare response */
#undef TU_TO_TSF
#undef TSF_TO_TU
}

/*
 * Transmit a beacon frame at SWBA.  Dynamic updates
 * to the frame contents are done as needed.
 */
static void
ath_tdma_beacon_send(struct ath_softc *sc, struct ieee80211vap *vap)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;
	int otherant;

	/*
	 * Check if the previous beacon has gone out.  If
	 * not don't try to post another, skip this period
	 * and wait for the next.  Missed beacons indicate
	 * a problem and should not occur.  If we miss too
	 * many consecutive beacons reset the device.
	 */
	if (ath_hal_numtxpending(ah, sc->sc_bhalq) != 0) {
		sc->sc_bmisscount++;
		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: missed %u consecutive beacons\n",
			__func__, sc->sc_bmisscount);
		if (sc->sc_bmisscount >= ath_bstuck_threshold)
			taskqueue_enqueue(sc->sc_tq, &sc->sc_bstucktask);
		return;
	}
	if (sc->sc_bmisscount != 0) {
		DPRINTF(sc, ATH_DEBUG_BEACON,
			"%s: resume beacon xmit after %u misses\n",
			__func__, sc->sc_bmisscount);
		sc->sc_bmisscount = 0;
	}

	/*
	 * Check recent per-antenna transmit statistics and flip
	 * the default antenna if noticeably more frames went out
	 * on the non-default antenna.
	 * XXX assumes 2 anntenae
	 */
	if (!sc->sc_diversity) {
		otherant = sc->sc_defant & 1 ? 2 : 1;
		if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + 2)
			ath_setdefantenna(sc, otherant);
		sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;
	}

	bf = ath_beacon_generate(sc, vap);
	if (bf != NULL) {
		/*
		 * Stop any current dma and put the new frame on the queue.
		 * This should never fail since we check above that no frames
		 * are still pending on the queue.
		 */
		if (!ath_hal_stoptxdma(ah, sc->sc_bhalq)) {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: beacon queue %u did not stop?\n",
				__func__, sc->sc_bhalq);
			/* NB: the HAL still stops DMA, so proceed */
		}
		ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
		ath_hal_txstart(ah, sc->sc_bhalq);

		sc->sc_stats.ast_be_xmit++;		/* XXX per-vap? */

		/*
		 * Record local TSF for our last send for use
		 * in arbitrating slot collisions.
		 */
		vap->iv_bss->ni_tstamp.tsf = ath_hal_gettsf64(ah);
	}
}
#endif /* IEEE80211_SUPPORT_TDMA */

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
		ieee80211_dfs_notify_radar(ic, sc->sc_curchan);
	}
}

MODULE_VERSION(if_ath, 1);
MODULE_DEPEND(if_ath, wlan, 1, 1, 1);          /* 802.11 media layer */
