/*	$NetBSD: wi.c,v 1.109 2003/01/09 08:52:19 dyoung Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Lucent WaveLAN/IEEE 802.11 PCMCIA driver.
 *
 * Original FreeBSD driver written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The WaveLAN/IEEE adapter is the second generation of the WaveLAN
 * from Lucent. Unlike the older cards, the new ones are programmed
 * entirely via a firmware-driven controller called the Hermes.
 * Unfortunately, Lucent will not release the Hermes programming manual
 * without an NDA (if at all). What they do release is an API library
 * called the HCF (Hardware Control Functions) which is supposed to
 * do the device-specific operations of a device driver for you. The
 * publically available version of the HCF library (the 'HCF Light') is 
 * a) extremely gross, b) lacks certain features, particularly support
 * for 802.11 frames, and c) is contaminated by the GNU Public License.
 *
 * This driver does not use the HCF or HCF Light at all. Instead, it
 * programs the Hermes controller directly, using information gleaned
 * from the HCF Light code and corresponding documentation.
 *
 * This driver supports the ISA, PCMCIA and PCI versions of the Lucent
 * WaveLan cards (based on the Hermes chipset), as well as the newer
 * Prism 2 chipsets with firmware from Intersil and Symbol.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define WI_HERMES_AUTOINC_WAR	/* Work around data write autoinc bug. */
#define WI_HERMES_STATS_WAR	/* Work around stats counter bug. */

#define	NBPFILTER	1

#include <sys/param.h>
#include <sys/systm.h>
#if __FreeBSD_version >= 500033
#include <sys/endian.h>
#endif
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/random.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_ieee80211.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/if_wivar.h>
#include <dev/wi/if_wireg.h>

#define IF_POLL(ifq, m)		((m) = (ifq)->ifq_head)
#define	IFQ_POLL(ifq, m)	IF_POLL((ifq), (m))
#define IFQ_DEQUEUE(ifq, m)	IF_DEQUEUE((ifq), (m))

static void wi_start(struct ifnet *);
static int  wi_reset(struct wi_softc *);
static void wi_watchdog(struct ifnet *);
static int  wi_ioctl(struct ifnet *, u_long, caddr_t);
static int  wi_media_change(struct ifnet *);
static void wi_media_status(struct ifnet *, struct ifmediareq *);

static void wi_rx_intr(struct wi_softc *);
static void wi_tx_intr(struct wi_softc *);
static void wi_tx_ex_intr(struct wi_softc *);
static void wi_info_intr(struct wi_softc *);

static int  wi_get_cfg(struct ifnet *, u_long, caddr_t);
static int  wi_set_cfg(struct ifnet *, u_long, caddr_t);
static int  wi_write_txrate(struct wi_softc *);
static int  wi_write_wep(struct wi_softc *);
static int  wi_write_multi(struct wi_softc *);
static int  wi_alloc_fid(struct wi_softc *, int, int *);
static void wi_read_nicid(struct wi_softc *);
static int  wi_write_ssid(struct wi_softc *, int, u_int8_t *, int);

static int  wi_cmd(struct wi_softc *, int, int, int, int);
static int  wi_seek_bap(struct wi_softc *, int, int);
static int  wi_read_bap(struct wi_softc *, int, int, void *, int);
static int  wi_write_bap(struct wi_softc *, int, int, void *, int);
static int  wi_mwrite_bap(struct wi_softc *, int, int, struct mbuf *, int);
static int  wi_read_rid(struct wi_softc *, int, void *, int *);
static int  wi_write_rid(struct wi_softc *, int, void *, int);

static int  wi_newstate(void *, enum ieee80211_state);

static int  wi_scan_ap(struct wi_softc *, u_int16_t, u_int16_t);
static void wi_scan_result(struct wi_softc *, int, int);

static void wi_dump_pkt(struct wi_frame *, struct ieee80211_node *, int rssi);

static int wi_get_debug(struct wi_softc *, struct wi_req *);
static int wi_set_debug(struct wi_softc *, struct wi_req *);

#if __FreeBSD_version >= 500000
/* support to download firmware for symbol CF card */
static int wi_symbol_write_firm(struct wi_softc *, const void *, int,
		const void *, int);
static int wi_symbol_set_hcr(struct wi_softc *, int);
#endif

static __inline int
wi_write_val(struct wi_softc *sc, int rid, u_int16_t val)
{

	val = htole16(val);
	return wi_write_rid(sc, rid, &val, sizeof(val));
}

SYSCTL_NODE(_hw, OID_AUTO, wi, CTLFLAG_RD, 0, "Wireless driver parameters");

static	struct timeval lasttxerror;	/* time of last tx error msg */
static	int curtxeps;			/* current tx error msgs/sec */
static	int wi_txerate = 0;		/* tx error rate: max msgs/sec */
SYSCTL_INT(_hw_wi, OID_AUTO, txerate, CTLFLAG_RW, &wi_txerate,
	    0, "max tx error msgs/sec; 0 to disable msgs");

#define	WI_DEBUG
#ifdef WI_DEBUG
static	int wi_debug = 0;
SYSCTL_INT(_hw_wi, OID_AUTO, debug, CTLFLAG_RW, &wi_debug,
	    0, "control debugging printfs");

#define	DPRINTF(X)	if (wi_debug) printf X
#define	DPRINTF2(X)	if (wi_debug > 1) printf X
#define	IFF_DUMPPKTS(_ifp) \
	(((_ifp)->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#else
#define	DPRINTF(X)
#define	DPRINTF2(X)
#define	IFF_DUMPPKTS(_ifp)	0
#endif

#define WI_INTRS	(WI_EV_RX | WI_EV_ALLOC | WI_EV_INFO)

struct wi_card_ident wi_card_ident[] = {
	/* CARD_ID			CARD_NAME		FIRM_TYPE */
	{ WI_NIC_LUCENT_ID,		WI_NIC_LUCENT_STR,	WI_LUCENT },
	{ WI_NIC_SONY_ID,		WI_NIC_SONY_STR,	WI_LUCENT },
	{ WI_NIC_LUCENT_EMB_ID,		WI_NIC_LUCENT_EMB_STR,	WI_LUCENT },
	{ WI_NIC_EVB2_ID,		WI_NIC_EVB2_STR,	WI_INTERSIL },
	{ WI_NIC_HWB3763_ID,		WI_NIC_HWB3763_STR,	WI_INTERSIL },
	{ WI_NIC_HWB3163_ID,		WI_NIC_HWB3163_STR,	WI_INTERSIL },
	{ WI_NIC_HWB3163B_ID,		WI_NIC_HWB3163B_STR,	WI_INTERSIL },
	{ WI_NIC_EVB3_ID,		WI_NIC_EVB3_STR,	WI_INTERSIL },
	{ WI_NIC_HWB1153_ID,		WI_NIC_HWB1153_STR,	WI_INTERSIL },
	{ WI_NIC_P2_SST_ID,		WI_NIC_P2_SST_STR,	WI_INTERSIL },
	{ WI_NIC_EVB2_SST_ID,		WI_NIC_EVB2_SST_STR,	WI_INTERSIL },
	{ WI_NIC_3842_EVA_ID,		WI_NIC_3842_EVA_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCMCIA_AMD_ID,	WI_NIC_3842_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCMCIA_SST_ID,	WI_NIC_3842_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCMCIA_ATL_ID,	WI_NIC_3842_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCMCIA_ATS_ID,	WI_NIC_3842_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_3842_MINI_AMD_ID,	WI_NIC_3842_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_MINI_SST_ID,	WI_NIC_3842_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_MINI_ATL_ID,	WI_NIC_3842_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_MINI_ATS_ID,	WI_NIC_3842_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCI_AMD_ID,	WI_NIC_3842_PCI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCI_SST_ID,	WI_NIC_3842_PCI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCI_ATS_ID,	WI_NIC_3842_PCI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCI_ATL_ID,	WI_NIC_3842_PCI_STR,	WI_INTERSIL },
	{ WI_NIC_P3_PCMCIA_AMD_ID,	WI_NIC_P3_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_P3_PCMCIA_SST_ID,	WI_NIC_P3_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_P3_PCMCIA_ATL_ID,	WI_NIC_P3_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_P3_PCMCIA_ATS_ID,	WI_NIC_P3_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_P3_MINI_AMD_ID,	WI_NIC_P3_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_P3_MINI_SST_ID,	WI_NIC_P3_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_P3_MINI_ATL_ID,	WI_NIC_P3_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_P3_MINI_ATS_ID,	WI_NIC_P3_MINI_STR,	WI_INTERSIL },
	{ 0,	NULL,	0 },
};

devclass_t wi_devclass;

int
wi_attach(device_t dev)
{
	struct wi_softc	*sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int i, nrate, mword, buflen;
	u_int8_t r;
	u_int16_t val;
	u_int8_t ratebuf[2 + IEEE80211_RATE_SIZE];
	static const u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	int error;

	/*
	 * NB: no locking is needed here; don't put it here
	 *     unless you can prove it!
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
	    wi_intr, sc, &sc->wi_intrhand);

	if (error) {
		device_printf(dev, "bus_setup_intr() failed! (%d)\n", error);
		wi_free(dev);
		return (error);
	}

#if __FreeBSD_version >= 500000
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
#endif

	/* Reset the NIC. */
	if (wi_reset(sc) != 0)
		return ENXIO;		/* XXX */

	/*
	 * Read the station address.
	 * And do it twice. I've seen PRISM-based cards that return
	 * an error when trying to read it the first time, which causes
	 * the probe to fail.
	 */
	buflen = IEEE80211_ADDR_LEN;
	error = wi_read_rid(sc, WI_RID_MAC_NODE, ic->ic_myaddr, &buflen);
	if (error != 0) {
		buflen = IEEE80211_ADDR_LEN;
		error = wi_read_rid(sc, WI_RID_MAC_NODE, ic->ic_myaddr, &buflen);
	}
	if (error || IEEE80211_ADDR_EQ(ic->ic_myaddr, empty_macaddr)) {
		if (error != 0)
			device_printf(dev, "mac read failed %d\n", error);
		else
			device_printf(dev, "mac read failed (all zeros)\n");
		wi_free(dev);
		return (error);
	}
	device_printf(dev, "802.11 address: %6D\n", ic->ic_myaddr, ":");

	/* Read NIC identification */
	wi_read_nicid(sc);

	ifp->if_softc = sc;
	ifp->if_unit = sc->sc_unit;
	ifp->if_name = "wi";
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wi_ioctl;
	ifp->if_start = wi_start;
	ifp->if_watchdog = wi_watchdog;
	ifp->if_init = wi_init;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_flags = IEEE80211_F_HASPMGT | IEEE80211_F_HASAHDEMO;
	ic->ic_state = IEEE80211_S_INIT;
	ic->ic_newstate = wi_newstate;
	ic->ic_fixed_rate = -1;	/* Auto */

	/* Find available channels */
	buflen = sizeof(val);
	if (wi_read_rid(sc, WI_RID_CHANNEL_LIST, &val, &buflen) != 0)
		val = htole16(0x1fff);	/* assume 1-11 */
	for (i = 0; i < 16; i++) {
		if (isset((u_int8_t*)&val, i))
			setbit(ic->ic_chan_avail, i + 1);
	}
	KASSERT(ic->ic_chan_avail != 0,
		("wi_attach: no available channels listed!"));

	/*
	 * Read the default channel from the NIC. This may vary
	 * depending on the country where the NIC was purchased, so
	 * we can't hard-code a default and expect it to work for
	 * everyone.
	 */
	buflen = sizeof(val);
	if (wi_read_rid(sc, WI_RID_OWN_CHNL, &val, &buflen) == 0)
		ic->ic_ibss_chan = le16toh(val);
	else {
		/* use lowest available channel */
		for (i = 0; i < 16 && !isset(ic->ic_chan_avail, i); i++)
			;
		ic->ic_ibss_chan = i;
	}

	/*
	 * Set flags based on firmware version.
	 */
	switch (sc->sc_firmware_type) {
	case WI_LUCENT:
		sc->sc_ntxbuf = 1;
		sc->sc_flags |= WI_FLAGS_HAS_SYSSCALE;
#ifdef WI_HERMES_AUTOINC_WAR
		/* XXX: not confirmed, but never seen for recent firmware */
		if (sc->sc_sta_firmware_ver <  40000) {
			sc->sc_flags |= WI_FLAGS_BUG_AUTOINC;
		}
#endif
		if (sc->sc_sta_firmware_ver >= 60000)
			sc->sc_flags |= WI_FLAGS_HAS_MOR;
		if (sc->sc_sta_firmware_ver >= 60006)
			ic->ic_flags |= IEEE80211_F_HASIBSS;
		sc->sc_ibss_port = htole16(1);
		break;

	case WI_INTERSIL:
		sc->sc_ntxbuf = WI_NTXBUF;
		sc->sc_flags |= WI_FLAGS_HAS_FRAGTHR;
		sc->sc_flags |= WI_FLAGS_HAS_ROAMING;
		sc->sc_flags |= WI_FLAGS_HAS_SYSSCALE;
		if (sc->sc_sta_firmware_ver > 10101)
			sc->sc_flags |= WI_FLAGS_HAS_DBMADJUST;
		if (sc->sc_sta_firmware_ver >= 800)
			ic->ic_flags |= IEEE80211_F_HASIBSS;
		/*
		 * version 0.8.3 and newer are the only ones that are known
		 * to currently work.  Earlier versions can be made to work,
		 * at least according to the Linux driver.
		 */
		if (sc->sc_sta_firmware_ver >= 803)
			ic->ic_flags |= IEEE80211_F_HASHOSTAP;
		sc->sc_ibss_port = htole16(0);
		break;

	case WI_SYMBOL:
		sc->sc_ntxbuf = 1;
		sc->sc_flags |= WI_FLAGS_HAS_DIVERSITY;
		if (sc->sc_sta_firmware_ver >= 25000)
			ic->ic_flags |= IEEE80211_F_HASIBSS;
		sc->sc_ibss_port = htole16(4);
		break;
	}

	/*
	 * Find out if we support WEP on this card.
	 */
	buflen = sizeof(val);
	if (wi_read_rid(sc, WI_RID_WEP_AVAIL, &val, &buflen) == 0 &&
	    val != htole16(0))
		ic->ic_flags |= IEEE80211_F_HASWEP;

	/* Find supported rates. */
	buflen = sizeof(ratebuf);
	if (wi_read_rid(sc, WI_RID_DATA_RATES, ratebuf, &buflen) == 0) {
		nrate = le16toh(*(u_int16_t *)ratebuf);
		if (nrate > IEEE80211_RATE_SIZE)
			nrate = IEEE80211_RATE_SIZE;
		memcpy(ic->ic_sup_rates, ratebuf + 2, nrate);
	} else {
		/* XXX fallback on error? */
		nrate = 0;
	}

	buflen = sizeof(val);
	if ((sc->sc_flags & WI_FLAGS_HAS_DBMADJUST) &&
	    wi_read_rid(sc, WI_RID_DBM_ADJUST, &val, &buflen) == 0) {
		sc->sc_dbm_adjust = le16toh(val);
	} else
		sc->sc_dbm_adjust = 100;	/* default */

	sc->sc_max_datalen = 2304;
	sc->sc_rts_thresh = 2347;
	sc->sc_frag_thresh = 2346;
	sc->sc_system_scale = 1;
	sc->sc_cnfauthmode = IEEE80211_AUTH_OPEN;
	sc->sc_roaming_mode = 1;

	sc->sc_portnum = WI_DEFAULT_PORT;
	sc->sc_authtype = WI_DEFAULT_AUTHTYPE;

	bzero(sc->sc_nodename, sizeof(sc->sc_nodename));
	sc->sc_nodelen = sizeof(WI_DEFAULT_NODENAME) - 1;
	bcopy(WI_DEFAULT_NODENAME, sc->sc_nodename, sc->sc_nodelen);

	bzero(sc->sc_net_name, sizeof(sc->sc_net_name));
	bcopy(WI_DEFAULT_NETNAME, sc->sc_net_name,
	    sizeof(WI_DEFAULT_NETNAME) - 1);

	ifmedia_init(&sc->sc_media, 0, wi_media_change, wi_media_status);
	if_printf(ifp, "supported rates: ");
#define	ADD(s, o)	ifmedia_add(&sc->sc_media, \
	IFM_MAKEWORD(IFM_IEEE80211, (s), (o), 0), 0, NULL)
	ADD(IFM_AUTO, 0);
	if (ic->ic_flags & IEEE80211_F_HASHOSTAP)
		ADD(IFM_AUTO, IFM_IEEE80211_HOSTAP);
	if (ic->ic_flags & IEEE80211_F_HASIBSS)
		ADD(IFM_AUTO, IFM_IEEE80211_ADHOC);
	ADD(IFM_AUTO, IFM_IEEE80211_ADHOC | IFM_FLAG0);
	for (i = 0; i < nrate; i++) {
		r = ic->ic_sup_rates[i];
		mword = ieee80211_rate2media(r, IEEE80211_T_DS);
		if (mword == 0)
			continue;
		printf("%s%d%sMbps", (i != 0 ? " " : ""),
		    (r & IEEE80211_RATE_VAL) / 2, ((r & 0x1) != 0 ? ".5" : ""));
		ADD(mword, 0);
		if (ic->ic_flags & IEEE80211_F_HASHOSTAP)
			ADD(mword, IFM_IEEE80211_HOSTAP);
		if (ic->ic_flags & IEEE80211_F_HASIBSS)
			ADD(mword, IFM_IEEE80211_ADHOC);
		ADD(mword, IFM_IEEE80211_ADHOC | IFM_FLAG0);
	}
	printf("\n");
	ifmedia_set(&sc->sc_media, IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 0, 0));
#undef ADD

	/*
	 * Call MI attach routine.
	 */
	ieee80211_ifattach(ifp);

	return (0);
}

int
wi_detach(device_t dev)
{
	struct wi_softc	*sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	WI_LOCK_DECL();

	WI_LOCK(sc);

	/* check if device was removed */
	sc->wi_gone = !bus_child_present(dev);

	wi_stop(ifp, 0);

	/* Delete all remaining media. */
	ifmedia_removeall(&sc->sc_media);

	ieee80211_ifdetach(ifp);
	WI_UNLOCK(sc);
	bus_teardown_intr(dev, sc->irq, sc->wi_intrhand);
	wi_free(dev);
#if __FreeBSD_version >= 500000
	mtx_destroy(&sc->sc_mtx);
#endif
	return (0);
}

#ifdef __NetBSD__
int
wi_activate(struct device *self, enum devact act)
{
	struct wi_softc *sc = (struct wi_softc *)self;
	int rv = 0, s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ic.ic_if);
		break;
	}
	splx(s);
	return rv;
}

void
wi_power(struct wi_softc *sc, int why)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splnet();
	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		wi_stop(ifp, 1);
		break;
	case PWR_RESUME:
		if (ifp->if_flags & IFF_UP) {
			wi_init(ifp);
			(void)wi_intr(sc);
		}
		break;
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
	splx(s);
}
#endif /* __NetBSD__ */

void
wi_shutdown(device_t dev)
{
	struct wi_softc *sc = device_get_softc(dev);

	wi_stop(&sc->sc_if, 1);
}

void
wi_intr(void *arg)
{
	struct wi_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	u_int16_t status;
	WI_LOCK_DECL();

	WI_LOCK(sc);

	if (sc->wi_gone || (ifp->if_flags & IFF_UP) == 0) {
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		CSR_WRITE_2(sc, WI_EVENT_ACK, ~0);
		WI_UNLOCK(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);

	status = CSR_READ_2(sc, WI_EVENT_STAT);
	if (status & WI_EV_RX)
		wi_rx_intr(sc);
	if (status & WI_EV_ALLOC)
		wi_tx_intr(sc);
	if (status & WI_EV_TX_EXC)
		wi_tx_ex_intr(sc);
	if (status & WI_EV_INFO)
		wi_info_intr(sc);
	if ((ifp->if_flags & IFF_OACTIVE) == 0 &&
	    (sc->sc_flags & WI_FLAGS_OUTRANGE) == 0 &&
	    _IF_QLEN(&ifp->if_snd) != 0)
		wi_start(ifp);

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	WI_UNLOCK(sc);

	return;
}

void
wi_init(void *arg)
{
	struct wi_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211com *ic = &sc->sc_ic;
	struct wi_joinreq join;
	int i;
	int error = 0, wasenabled;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	WI_LOCK_DECL();

	WI_LOCK(sc);

	if (sc->wi_gone) {
		WI_UNLOCK(sc);
		return;
	}

	if ((wasenabled = sc->sc_enabled))
		wi_stop(ifp, 0);
	wi_reset(sc);

	/* common 802.11 configuration */
	ic->ic_flags &= ~IEEE80211_F_IBSSON;
	sc->sc_flags &= ~WI_FLAGS_OUTRANGE;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		wi_write_val(sc, WI_RID_PORTTYPE, WI_PORTTYPE_BSS);
		break;
	case IEEE80211_M_IBSS:
		wi_write_val(sc, WI_RID_PORTTYPE, sc->sc_ibss_port);
		ic->ic_flags |= IEEE80211_F_IBSSON;
		break;
	case IEEE80211_M_AHDEMO:
		wi_write_val(sc, WI_RID_PORTTYPE, WI_PORTTYPE_ADHOC);
		break;
	case IEEE80211_M_HOSTAP:
		wi_write_val(sc, WI_RID_PORTTYPE, WI_PORTTYPE_HOSTAP);
		break;
	}

	/* Intersil interprets this RID as joining ESS even in IBSS mode */
	if (sc->sc_firmware_type == WI_LUCENT &&
	    (ic->ic_flags & IEEE80211_F_IBSSON) && ic->ic_des_esslen > 0)
		wi_write_val(sc, WI_RID_CREATE_IBSS, 1);
	else
		wi_write_val(sc, WI_RID_CREATE_IBSS, 0);
	wi_write_val(sc, WI_RID_MAX_SLEEP, ic->ic_lintval);
	wi_write_ssid(sc, WI_RID_DESIRED_SSID, ic->ic_des_essid,
	    ic->ic_des_esslen);
	wi_write_val(sc, WI_RID_OWN_CHNL, ic->ic_ibss_chan);
	wi_write_ssid(sc, WI_RID_OWN_SSID, ic->ic_des_essid, ic->ic_des_esslen);

	ifa = ifaddr_byindex(ifp->if_index);
	sdl = (struct sockaddr_dl *) ifa->ifa_addr;
	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(sdl));
	wi_write_rid(sc, WI_RID_MAC_NODE, ic->ic_myaddr, IEEE80211_ADDR_LEN);

	wi_write_val(sc, WI_RID_PM_ENABLED,
	    (ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0);

	/* not yet common 802.11 configuration */
	wi_write_val(sc, WI_RID_MAX_DATALEN, sc->sc_max_datalen);
	wi_write_val(sc, WI_RID_RTS_THRESH, sc->sc_rts_thresh);
	if (sc->sc_flags & WI_FLAGS_HAS_FRAGTHR)
		wi_write_val(sc, WI_RID_FRAG_THRESH, sc->sc_frag_thresh);

	/* driver specific 802.11 configuration */
	if (sc->sc_flags & WI_FLAGS_HAS_SYSSCALE)
		wi_write_val(sc, WI_RID_SYSTEM_SCALE, sc->sc_system_scale);
	if (sc->sc_flags & WI_FLAGS_HAS_ROAMING)
		wi_write_val(sc, WI_RID_ROAMING_MODE, sc->sc_roaming_mode);
	if (sc->sc_flags & WI_FLAGS_HAS_MOR)
		wi_write_val(sc, WI_RID_MICROWAVE_OVEN, sc->sc_microwave_oven);
	wi_write_txrate(sc);
	wi_write_ssid(sc, WI_RID_NODENAME, sc->sc_nodename, sc->sc_nodelen);

	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    sc->sc_firmware_type == WI_INTERSIL) {
		wi_write_val(sc, WI_RID_OWN_BEACON_INT, ic->ic_lintval);
		wi_write_val(sc, WI_RID_BASIC_RATE, 0x03);   /* 1, 2 */
		wi_write_val(sc, WI_RID_SUPPORT_RATE, 0x0f); /* 1, 2, 5.5, 11 */
		wi_write_val(sc, WI_RID_DTIM_PERIOD, 1);
	}

	/*
	 * Initialize promisc mode.
	 *	Being in the Host-AP mode causes a great
	 *	deal of pain if primisc mode is set.
	 *	Therefore we avoid confusing the firmware
	 *	and always reset promisc mode in Host-AP
	 *	mode.  Host-AP sees all the packets anyway.
	 */
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    (ifp->if_flags & IFF_PROMISC) != 0) {
		wi_write_val(sc, WI_RID_PROMISC, 1);
	} else {
		wi_write_val(sc, WI_RID_PROMISC, 0);
	}

	/* Configure WEP. */
	if (ic->ic_flags & IEEE80211_F_HASWEP)
		wi_write_wep(sc);

	/* Set multicast filter. */
	wi_write_multi(sc);

	/* Allocate fids for the card */
	if (sc->sc_firmware_type != WI_SYMBOL || !wasenabled) {
		sc->sc_buflen = IEEE80211_MAX_LEN + sizeof(struct wi_frame);
		if (sc->sc_firmware_type == WI_SYMBOL)
			sc->sc_buflen = 1585;	/* XXX */
		for (i = 0; i < sc->sc_ntxbuf; i++) {
			error = wi_alloc_fid(sc, sc->sc_buflen,
			    &sc->sc_txd[i].d_fid);
			if (error) {
				device_printf(sc->sc_dev,
				    "tx buffer allocation failed (error %u)\n",
				    error);
				goto out;
			}
			sc->sc_txd[i].d_len = 0;
		}
	}
	sc->sc_txcur = sc->sc_txnext = 0;

	/* Enable desired port */
	wi_cmd(sc, WI_CMD_ENABLE | sc->sc_portnum, 0, 0, 0);

	sc->sc_enabled = 1;
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	if (ic->ic_opmode == IEEE80211_M_AHDEMO ||
	    ic->ic_opmode == IEEE80211_M_HOSTAP)
		wi_newstate(sc, IEEE80211_S_RUN);

	/* Enable interrupts */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	if (!wasenabled &&
	    ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    sc->sc_firmware_type == WI_INTERSIL) {
		/* XXX: some card need to be re-enabled for hostap */
		wi_cmd(sc, WI_CMD_DISABLE | WI_PORT0, 0, 0, 0);
		wi_cmd(sc, WI_CMD_ENABLE | WI_PORT0, 0, 0, 0);
	}

	if (ic->ic_opmode == IEEE80211_M_STA &&
	    ((ic->ic_flags & IEEE80211_F_DESBSSID) ||
	    ic->ic_des_chan != IEEE80211_CHAN_ANY)) {
		memset(&join, 0, sizeof(join));
		if (ic->ic_flags & IEEE80211_F_DESBSSID)
			IEEE80211_ADDR_COPY(&join.wi_bssid, ic->ic_des_bssid);
		if (ic->ic_des_chan != IEEE80211_CHAN_ANY)
			join.wi_chan = htole16(ic->ic_des_chan);
		/* Lucent firmware does not support the JOIN RID. */
		if (sc->sc_firmware_type != WI_LUCENT)
			wi_write_rid(sc, WI_RID_JOIN_REQ, &join, sizeof(join));
	}

	WI_UNLOCK(sc);
	return;
out:
	if (error) {
		if_printf(ifp, "interface not running\n");
		wi_stop(ifp, 0);
	}
	WI_UNLOCK(sc);
	DPRINTF(("wi_init: return %d\n", error));
	return;
}

void
wi_stop(struct ifnet *ifp, int disable)
{
	struct wi_softc *sc = ifp->if_softc;
	WI_LOCK_DECL();

	WI_LOCK(sc);

	ieee80211_new_state(ifp, IEEE80211_S_INIT, -1);
	if (sc->sc_enabled && !sc->wi_gone) {
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		wi_cmd(sc, WI_CMD_DISABLE | sc->sc_portnum, 0, 0, 0);
		if (disable) {
#ifdef __NetBSD__
			if (sc->sc_disable)
				(*sc->sc_disable)(sc);
#endif
		}
		sc->sc_enabled = 0;
	}

	sc->sc_tx_timer = 0;
	sc->sc_scan_timer = 0;
	sc->sc_syn_timer = 0;
	sc->sc_false_syns = 0;
	sc->sc_naps = 0;
	ifp->if_flags &= ~(IFF_OACTIVE | IFF_RUNNING);
	ifp->if_timer = 0;

	WI_UNLOCK(sc);
}

static void
wi_start(struct ifnet *ifp)
{
	struct wi_softc	*sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = NULL;
	struct ieee80211_frame *wh;
	struct mbuf *m0;
	struct wi_frame frmhdr;
	int cur, fid, off;
	WI_LOCK_DECL();

	WI_LOCK(sc);

	if (sc->wi_gone) {
		WI_UNLOCK(sc);
		return;
	}
	if (sc->sc_flags & WI_FLAGS_OUTRANGE) {
		WI_UNLOCK(sc);
		return;
	}

	memset(&frmhdr, 0, sizeof(frmhdr));
	cur = sc->sc_txnext;
	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->sc_txd[cur].d_len != 0) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);
			m_copydata(m0, 4, ETHER_ADDR_LEN * 2,
			    (caddr_t)&frmhdr.wi_ehdr);
			frmhdr.wi_ehdr.ether_type = 0;
                        wh = mtod(m0, struct ieee80211_frame *);
		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_POLL(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->sc_txd[cur].d_len != 0) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			ifp->if_opackets++;
			m_copydata(m0, 0, ETHER_HDR_LEN, 
			    (caddr_t)&frmhdr.wi_ehdr);
#if NBPFILTER > 0
			BPF_MTAP(ifp, m0);
#endif

			if ((m0 = ieee80211_encap(ifp, m0)) == NULL) {
				ifp->if_oerrors++;
				continue;
			}
                        wh = mtod(m0, struct ieee80211_frame *);
			if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
			    !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
			    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
			    IEEE80211_FC0_TYPE_DATA &&
			    ((ni = ieee80211_find_node(ic, wh->i_addr1)) ==
			    NULL || ni->ni_associd == 0)) {
				m_freem(m0);
				ifp->if_oerrors++;
				continue;
			}
			if (ic->ic_flags & IEEE80211_F_WEPON)
				wh->i_fc[1] |= IEEE80211_FC1_WEP;

		}
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m0);
#endif
		frmhdr.wi_tx_ctl = htole16(WI_ENC_TX_802_11|WI_TXCNTL_TX_EX);
		if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
		    (wh->i_fc[1] & IEEE80211_FC1_WEP)) {
			if ((m0 = ieee80211_wep_crypt(ifp, m0, 1)) == NULL) {
				ifp->if_oerrors++;
				continue;
			}
			frmhdr.wi_tx_ctl |= htole16(WI_TXCNTL_NOCRYPT);
		}
		m_copydata(m0, 0, sizeof(struct ieee80211_frame),
		    (caddr_t)&frmhdr.wi_whdr);
		m_adj(m0, sizeof(struct ieee80211_frame));
		frmhdr.wi_dat_len = htole16(m0->m_pkthdr.len);
#if NBPFILTER > 0
		if (sc->sc_drvbpf) {
			struct mbuf *mb;

			MGETHDR(mb, M_DONTWAIT, m0->m_type);
			if (mb != NULL) {
				(void) m_dup_pkthdr(mb, m0, M_DONTWAIT);
				mb->m_next = m0;
				mb->m_data = (caddr_t)&frmhdr;
				mb->m_len = sizeof(frmhdr);
				mb->m_pkthdr.len += mb->m_len;
				bpf_mtap(sc->sc_drvbpf, mb);
				m_free(mb);
			}
		}
#endif
		if (IFF_DUMPPKTS(ifp))
			wi_dump_pkt(&frmhdr, ni, -1);
		fid = sc->sc_txd[cur].d_fid;
		off = sizeof(frmhdr);
		if (wi_write_bap(sc, fid, 0, &frmhdr, sizeof(frmhdr)) != 0 ||
		    wi_mwrite_bap(sc, fid, off, m0, m0->m_pkthdr.len) != 0) {
			ifp->if_oerrors++;
			m_freem(m0);
			continue;
		}
		m_freem(m0);
		sc->sc_txd[cur].d_len = off;
		if (sc->sc_txcur == cur) {
			if (wi_cmd(sc, WI_CMD_TX | WI_RECLAIM, fid, 0, 0)) {
				if_printf(ifp, "xmit failed\n");
				sc->sc_txd[cur].d_len = 0;
				continue;
			}
			sc->sc_tx_timer = 5;
			ifp->if_timer = 1;
		}
		sc->sc_txnext = cur = (cur + 1) % sc->sc_ntxbuf;
	}

	WI_UNLOCK(sc);
}

static int
wi_reset(struct wi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
#define WI_INIT_TRIES 3
	int i;
	int error = 0;
	int tries;
	
	/* Symbol firmware cannot be initialized more than once */
	if (sc->sc_firmware_type != WI_INTERSIL && sc->sc_reset)
		return (0);
	if (sc->sc_firmware_type == WI_SYMBOL)
		tries = 1;
	else
		tries = WI_INIT_TRIES;

	for (i = 0; i < tries; i++) {
		if ((error = wi_cmd(sc, WI_CMD_INI, 0, 0, 0)) == 0)
			break;
		DELAY(WI_DELAY * 1000);
	}
	sc->sc_reset = 1;

	if (i == tries) {
		if_printf(ifp, "init failed\n");
		return (error);
	}

	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Calibrate timer. */
	wi_write_val(sc, WI_RID_TICK_TIME, 8);

	return (0);
#undef WI_INIT_TRIES
}

static void
wi_watchdog(struct ifnet *ifp)
{
	struct wi_softc	*sc = ifp->if_softc;

	ifp->if_timer = 0;
	if (!sc->sc_enabled)
		return;

	if (sc->sc_tx_timer) {
		if (--sc->sc_tx_timer == 0) {
			if_printf(ifp, "device timeout\n");
			ifp->if_oerrors++;
			wi_init(ifp->if_softc);
			return;
		}
		ifp->if_timer = 1;
	}

	if (sc->sc_scan_timer) {
		if (--sc->sc_scan_timer <= WI_SCAN_WAIT - WI_SCAN_INQWAIT &&
		    sc->sc_firmware_type == WI_INTERSIL) {
			DPRINTF(("wi_watchdog: inquire scan\n"));
			wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_SCAN_RESULTS, 0, 0);
		}
		if (sc->sc_scan_timer)
			ifp->if_timer = 1;
	}

	if (sc->sc_syn_timer) {
		if (--sc->sc_syn_timer == 0) {
			DPRINTF2(("wi_watchdog: %d false syns\n",
			    sc->sc_false_syns));
			sc->sc_false_syns = 0;
			ieee80211_new_state(ifp, IEEE80211_S_RUN, -1);
			sc->sc_syn_timer = 5;
		}
		ifp->if_timer = 1;
	}

	/* TODO: rate control */
	ieee80211_watchdog(ifp);
}

static int
wi_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ieee80211req *ireq;
	u_int8_t nodename[IEEE80211_NWID_LEN];
	int error = 0;
#if __FreeBSD_version >= 500000
	struct thread *td = curthread;
#else
	struct proc *td = curproc;		/* Little white lie */
#endif
	struct wi_req wreq;
	WI_LOCK_DECL();

	WI_LOCK(sc);

	if (sc->wi_gone) {
		error = ENODEV;
		goto out;
	}

	switch (cmd) {
	case SIOCSIFFLAGS:
		/*
		 * Can't do promisc and hostap at the same time.  If all that's
		 * changing is the promisc flag, try to short-circuit a call to
		 * wi_init() by just setting PROMISC in the hardware.
		 */
		if (ifp->if_flags & IFF_UP) {
			if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
			    ifp->if_flags & IFF_RUNNING) {
				if (ifp->if_flags & IFF_PROMISC &&
				    !(sc->sc_if_flags & IFF_PROMISC)) {
					wi_write_val(sc, WI_RID_PROMISC, 1);
				} else if (!(ifp->if_flags & IFF_PROMISC) &&
				    sc->sc_if_flags & IFF_PROMISC) {
					wi_write_val(sc, WI_RID_PROMISC, 0);
				} else {
					wi_init(sc);
				}
			} else {
				wi_init(sc);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				wi_stop(ifp, 0);
			}
		}
		sc->sc_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = wi_write_multi(sc);
		break;
	case SIOCGIFGENERIC:
		error = wi_get_cfg(ifp, cmd, data);
		break;
	case SIOCSIFGENERIC:
		error = suser(td);
		if (error)
			break;
		error = wi_set_cfg(ifp, cmd, data);
		break;
	case SIOCGPRISM2DEBUG:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (!(ifp->if_flags & IFF_RUNNING) ||
		    sc->sc_firmware_type == WI_LUCENT) {
			error = EIO;
			break;
		}
		error = wi_get_debug(sc, &wreq);
		if (error == 0)
			error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	case SIOCSPRISM2DEBUG:
		if ((error = suser(td)))
			goto out;
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		error = wi_set_debug(sc, &wreq);
		break;
	case SIOCG80211:
		ireq = (struct ieee80211req *) data;
		switch (ireq->i_type) {
		case IEEE80211_IOC_STATIONNAME:
			ireq->i_len = sc->sc_nodelen + 1;
			error = copyout(sc->sc_nodename, ireq->i_data,
					ireq->i_len);
			break;
		default:
			error = ieee80211_ioctl(ifp, cmd, data);
			break;
		}
		break;
	case SIOCS80211:
		error = suser(td);
		if (error)
			break;
		ireq = (struct ieee80211req *) data;
		switch (ireq->i_type) {
		case IEEE80211_IOC_STATIONNAME:
			if (ireq->i_val != 0 ||
			    ireq->i_len > IEEE80211_NWID_LEN) {
				error = EINVAL;
				break;
			}
			memset(nodename, 0, IEEE80211_NWID_LEN);
			error = copyin(ireq->i_data, nodename, ireq->i_len);
			if (error)
				break;
			if (sc->sc_enabled) {
				error = wi_write_ssid(sc, WI_RID_NODENAME,
					nodename, ireq->i_len);
				if (error)
					break;
			}
			memcpy(sc->sc_nodename, nodename, IEEE80211_NWID_LEN);
			sc->sc_nodelen = ireq->i_len;
			break;
		default:
			error = ieee80211_ioctl(ifp, cmd, data);
			break;
		}
		break;
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		break;
	}
	if (error == ENETRESET) {
		if (sc->sc_enabled)
			wi_init(ifp->if_softc);	/* XXX no error return */
		error = 0;
	}
out:
	WI_UNLOCK(sc);

	return (error);
}

static int
wi_media_change(struct ifnet *ifp)
{
	struct wi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifmedia_entry *ime;
	enum ieee80211_opmode newmode;
	int i, rate, error = 0;

	ime = sc->sc_media.ifm_cur;
	if (IFM_SUBTYPE(ime->ifm_media) == IFM_AUTO) {
		i = -1;
	} else {
		rate = ieee80211_media2rate(ime->ifm_media, IEEE80211_T_DS);
		if (rate == 0)
			return EINVAL;
		for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
			if ((ic->ic_sup_rates[i] & IEEE80211_RATE_VAL) == rate)
				break;
		}
		if (i == IEEE80211_RATE_SIZE)
			return EINVAL;
	}
	if (ic->ic_fixed_rate != i) {
		ic->ic_fixed_rate = i;
		error = ENETRESET;
	}

	if ((ime->ifm_media & IFM_IEEE80211_ADHOC) &&
	    (ime->ifm_media & IFM_FLAG0))
		newmode = IEEE80211_M_AHDEMO;
	else if (ime->ifm_media & IFM_IEEE80211_ADHOC)
		newmode = IEEE80211_M_IBSS;
	else if (ime->ifm_media & IFM_IEEE80211_HOSTAP)
		newmode = IEEE80211_M_HOSTAP;
	else
		newmode = IEEE80211_M_STA;
	if (ic->ic_opmode != newmode) {
		ic->ic_opmode = newmode;
		error = ENETRESET;
	}
	if (error == ENETRESET) {
		if (sc->sc_enabled)
			wi_init(ifp->if_softc); /* XXX error code lost */
		error = 0;
	}
#if 0
	ifp->if_baudrate = ifmedia_baudrate(sc->sc_media.ifm_cur->ifm_media);
#endif
	return error;
}

static void
wi_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct wi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	u_int16_t val;
	int rate, len;

	if (sc->wi_gone || !sc->sc_enabled) {
		imr->ifm_active = IFM_IEEE80211 | IFM_NONE;
		imr->ifm_status = 0;
		return;
	}

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN &&
	    (sc->sc_flags & WI_FLAGS_OUTRANGE) == 0)
		imr->ifm_status |= IFM_ACTIVE;
	len = sizeof(val);
	if (wi_read_rid(sc, WI_RID_CUR_TX_RATE, &val, &len) != 0)
		rate = 0;
	else {
		/* convert to 802.11 rate */
		rate = val * 2;
		if (sc->sc_firmware_type == WI_LUCENT) {
			if (rate == 4 * 2)
				rate = 11;	/* 5.5Mbps */
			else if (rate == 5 * 2)
				rate = 22;	/* 11Mbps */
		} else {
			if (rate == 4*2)
				rate = 11;	/* 5.5Mbps */
			else if (rate == 8*2)
				rate = 22;	/* 11Mbps */
		}
	}
	imr->ifm_active |= ieee80211_rate2media(rate, IEEE80211_T_DS);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_AHDEMO:
		imr->ifm_active |= IFM_IEEE80211_ADHOC | IFM_FLAG0;
		break;
	case IEEE80211_M_HOSTAP:
		imr->ifm_active |= IFM_IEEE80211_HOSTAP;
		break;
	}
}

static void
wi_sync_bssid(struct wi_softc *sc, u_int8_t new_bssid[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &ic->ic_bss;
	struct ifnet *ifp = &ic->ic_if;

	if (IEEE80211_ADDR_EQ(new_bssid, ni->ni_bssid))
		return;

	DPRINTF(("wi_sync_bssid: bssid %s -> ", ether_sprintf(ni->ni_bssid)));
	DPRINTF(("%s ?\n", ether_sprintf(new_bssid)));

	/* In promiscuous mode, the BSSID field is not a reliable
	 * indicator of the firmware's BSSID. Damp spurious
	 * change-of-BSSID indications.
	 */
	if ((ifp->if_flags & IFF_PROMISC) != 0 &&
	    sc->sc_false_syns >= WI_MAX_FALSE_SYNS)
		return;

	ieee80211_new_state(ifp, IEEE80211_S_RUN, -1);
}

static void
wi_rx_monitor(struct wi_softc *sc, int fid)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wi_frame *rx_frame;
	struct mbuf *m;
	int datlen, hdrlen;

	/* first allocate mbuf for packet storage */
	m = m_getcl(M_DONTWAIT, MT_DATA, 0);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}

	m->m_pkthdr.rcvif = ifp;

	/* now read wi_frame first so we know how much data to read */
	if (wi_read_bap(sc, fid, 0, mtod(m, caddr_t), sizeof(*rx_frame))) {
		ifp->if_ierrors++;
		goto done;
	}

	rx_frame = mtod(m, struct wi_frame *);

	switch ((rx_frame->wi_status & WI_STAT_MAC_PORT) >> 8) {
	case 7:
		switch (rx_frame->wi_whdr.i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
		case IEEE80211_FC0_TYPE_DATA:
			hdrlen = WI_DATA_HDRLEN;
			datlen = rx_frame->wi_dat_len + WI_FCS_LEN;
			break;
		case IEEE80211_FC0_TYPE_MGT:
			hdrlen = WI_MGMT_HDRLEN;
			datlen = rx_frame->wi_dat_len + WI_FCS_LEN;
			break;
		case IEEE80211_FC0_TYPE_CTL:
			/*
			 * prism2 cards don't pass control packets
			 * down properly or consistently, so we'll only
			 * pass down the header.
			 */
			hdrlen = WI_CTL_HDRLEN;
			datlen = 0;
			break;
		default:
			if_printf(ifp, "received packet of unknown type "
				"on port 7\n");
			ifp->if_ierrors++;
			goto done;
		}
		break;
	case 0:
		hdrlen = WI_DATA_HDRLEN;
		datlen = rx_frame->wi_dat_len + WI_FCS_LEN;
		break;
	default:
		if_printf(ifp, "received packet on invalid "
		    "port (wi_status=0x%x)\n", rx_frame->wi_status);
		ifp->if_ierrors++;
		goto done;
	}

	if (hdrlen + datlen + 2 > MCLBYTES) {
		if_printf(ifp, "oversized packet received "
		    "(wi_dat_len=%d, wi_status=0x%x)\n",
		    datlen, rx_frame->wi_status);
		ifp->if_ierrors++;
		goto done;
	}

	if (wi_read_bap(sc, fid, hdrlen, mtod(m, caddr_t) + hdrlen,
	    datlen + 2) == 0) {
		m->m_pkthdr.len = m->m_len = hdrlen + datlen;
		ifp->if_ipackets++;
		BPF_MTAP(ifp, m);	/* Handle BPF listeners. */
	} else
		ifp->if_ierrors++;
done:
	m_freem(m);
}

static void
wi_rx_intr(struct wi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wi_frame frmhdr;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	int fid, len, off, rssi;
	u_int8_t dir;
	u_int16_t status;
	u_int32_t rstamp;

	fid = CSR_READ_2(sc, WI_RX_FID);

	if (sc->wi_debug.wi_monitor) {
		/*
		 * If we are in monitor mode just
		 * read the data from the device.
		 */
		wi_rx_monitor(sc, fid);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
		return;
	}

	/* First read in the frame header */
	if (wi_read_bap(sc, fid, 0, &frmhdr, sizeof(frmhdr))) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
		ifp->if_ierrors++;
		DPRINTF(("wi_rx_intr: read fid %x failed\n", fid));
		return;
	}

	if (IFF_DUMPPKTS(ifp))
		wi_dump_pkt(&frmhdr, NULL, frmhdr.wi_rx_signal);

	/*
	 * Drop undecryptable or packets with receive errors here
	 */
	status = le16toh(frmhdr.wi_status);
	if (status & WI_STAT_ERRSTAT) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
		ifp->if_ierrors++;
		DPRINTF(("wi_rx_intr: fid %x error status %x\n", fid, status));
		return;
	}
	rssi = frmhdr.wi_rx_signal;
	rstamp = (le16toh(frmhdr.wi_rx_tstamp0) << 16) |
	    le16toh(frmhdr.wi_rx_tstamp1);

	len = le16toh(frmhdr.wi_dat_len);
	off = ALIGN(sizeof(struct ieee80211_frame));

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
		ifp->if_ierrors++;
		DPRINTF(("wi_rx_intr: MGET failed\n"));
		return;
	}
	if (off + len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
			m_freem(m);
			ifp->if_ierrors++;
			DPRINTF(("wi_rx_intr: MCLGET failed\n"));
			return;
		}
	}

	m->m_data += off - sizeof(struct ieee80211_frame);
	memcpy(m->m_data, &frmhdr.wi_whdr, sizeof(struct ieee80211_frame));
	wi_read_bap(sc, fid, sizeof(frmhdr),
	    m->m_data + sizeof(struct ieee80211_frame), len);
	m->m_pkthdr.len = m->m_len = sizeof(struct ieee80211_frame) + len;
	m->m_pkthdr.rcvif = ifp;

	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);

#if NBPFILTER > 0
	if (sc->sc_drvbpf) {
		struct mbuf *mb;

		MGETHDR(mb, M_DONTWAIT, m->m_type);
		if (mb != NULL) {
			(void) m_dup_pkthdr(mb, m, M_DONTWAIT);
			mb->m_next = m;
			mb->m_data = (caddr_t)&frmhdr;
			mb->m_len = sizeof(frmhdr);
			mb->m_pkthdr.len += mb->m_len;
			bpf_mtap(sc->sc_drvbpf, mb);
			m_free(mb);
		}
	}
#endif
	wh = mtod(m, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		/*
		 * WEP is decrypted by hardware. Clear WEP bit
		 * header for ieee80211_input().
		 */
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
	}

	/* synchronize driver's BSSID with firmware's BSSID */
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	if (ic->ic_opmode == IEEE80211_M_IBSS && dir == IEEE80211_FC1_DIR_NODS)
		wi_sync_bssid(sc, wh->i_addr3);

	ieee80211_input(ifp, m, rssi, rstamp);
}

static void
wi_tx_ex_intr(struct wi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct wi_frame frmhdr;
	int fid;

	fid = CSR_READ_2(sc, WI_TX_CMP_FID);
	/* Read in the frame header */
	if (wi_read_bap(sc, fid, 0, &frmhdr, sizeof(frmhdr)) == 0) {
		u_int16_t status = le16toh(frmhdr.wi_status);

		/*
		 * Spontaneous station disconnects appear as xmit
		 * errors.  Don't announce them and/or count them
		 * as an output error.
		 */
		if ((status & WI_TXSTAT_DISCONNECT) == 0) {
			if (ppsratecheck(&lasttxerror, &curtxeps, wi_txerate)) {
				if_printf(ifp, "tx failed");
				if (status & WI_TXSTAT_RET_ERR)
					printf(", retry limit exceeded");
				if (status & WI_TXSTAT_AGED_ERR)
					printf(", max transmit lifetime exceeded");
				if (status & WI_TXSTAT_DISCONNECT)
					printf(", port disconnected");
				if (status & WI_TXSTAT_FORM_ERR)
					printf(", invalid format (data len %u src %6D)",
						le16toh(frmhdr.wi_dat_len),
						frmhdr.wi_ehdr.ether_shost, ":");
				if (status & ~0xf)
					printf(", status=0x%x", status);
				printf("\n");
			}
			ifp->if_oerrors++;
		} else {
			DPRINTF(("port disconnected\n"));
			ifp->if_collisions++;	/* XXX */
		}
	} else
		DPRINTF(("wi_tx_ex_intr: read fid %x failed\n", fid));
	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX_EXC);
}

static void
wi_tx_intr(struct wi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int fid, cur;

	fid = CSR_READ_2(sc, WI_ALLOC_FID);
	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);

	cur = sc->sc_txcur;
	if (sc->sc_txd[cur].d_fid != fid) {
		if_printf(ifp, "bad alloc %x != %x, cur %d nxt %d\n",
		    fid, sc->sc_txd[cur].d_fid, cur, sc->sc_txnext);
		return;
	}
	sc->sc_tx_timer = 0;
	sc->sc_txd[cur].d_len = 0;
	sc->sc_txcur = cur = (cur + 1) % sc->sc_ntxbuf;
	if (sc->sc_txd[cur].d_len == 0)
		ifp->if_flags &= ~IFF_OACTIVE;
	else {
		if (wi_cmd(sc, WI_CMD_TX | WI_RECLAIM, sc->sc_txd[cur].d_fid,
		    0, 0)) {
			if_printf(ifp, "xmit failed\n");
			sc->sc_txd[cur].d_len = 0;
		} else {
			sc->sc_tx_timer = 5;
			ifp->if_timer = 1;
		}
	}
}

static void
wi_info_intr(struct wi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int i, fid, len, off;
	u_int16_t ltbuf[2];
	u_int16_t stat;
	u_int32_t *ptr;

	fid = CSR_READ_2(sc, WI_INFO_FID);
	wi_read_bap(sc, fid, 0, ltbuf, sizeof(ltbuf));

	switch (le16toh(ltbuf[1])) {

	case WI_INFO_LINK_STAT:
		wi_read_bap(sc, fid, sizeof(ltbuf), &stat, sizeof(stat));
		DPRINTF(("wi_info_intr: LINK_STAT 0x%x\n", le16toh(stat)));
		switch (le16toh(stat)) {
		case WI_INFO_LINK_STAT_CONNECTED:
			sc->sc_flags &= ~WI_FLAGS_OUTRANGE;
			if (ic->ic_state == IEEE80211_S_RUN &&
			    ic->ic_opmode != IEEE80211_M_IBSS)
				break;
			/* FALLTHROUGH */
		case WI_INFO_LINK_STAT_AP_CHG:
			ieee80211_new_state(ifp, IEEE80211_S_RUN, -1);
			break;
		case WI_INFO_LINK_STAT_AP_INR:
			sc->sc_flags &= ~WI_FLAGS_OUTRANGE;
			break;
		case WI_INFO_LINK_STAT_AP_OOR:
			if (sc->sc_firmware_type == WI_SYMBOL &&
			    sc->sc_scan_timer > 0) {
				if (wi_cmd(sc, WI_CMD_INQUIRE,
				    WI_INFO_HOST_SCAN_RESULTS, 0, 0) != 0)
					sc->sc_scan_timer = 0;
				break;
			}
			if (ic->ic_opmode == IEEE80211_M_STA)
				sc->sc_flags |= WI_FLAGS_OUTRANGE;
			break;
		case WI_INFO_LINK_STAT_DISCONNECTED:
		case WI_INFO_LINK_STAT_ASSOC_FAILED:
			if (ic->ic_opmode == IEEE80211_M_STA)
				ieee80211_new_state(ifp, IEEE80211_S_INIT, -1);
			break;
		}
		break;

	case WI_INFO_COUNTERS:
		/* some card versions have a larger stats structure */
		len = min(le16toh(ltbuf[0]) - 1, sizeof(sc->sc_stats) / 4);
		ptr = (u_int32_t *)&sc->sc_stats;
		off = sizeof(ltbuf);
		for (i = 0; i < len; i++, off += 2, ptr++) {
			wi_read_bap(sc, fid, off, &stat, sizeof(stat));
#ifdef WI_HERMES_STATS_WAR
			if (stat & 0xf000)
				stat = ~stat;
#endif
			*ptr += stat;
		}
		ifp->if_collisions = sc->sc_stats.wi_tx_single_retries +
		    sc->sc_stats.wi_tx_multi_retries +
		    sc->sc_stats.wi_tx_retry_limit;
		break;

	case WI_INFO_SCAN_RESULTS:
	case WI_INFO_HOST_SCAN_RESULTS:
		wi_scan_result(sc, fid, le16toh(ltbuf[0]));
		break;

	default:
		DPRINTF(("wi_info_intr: got fid %x type %x len %d\n", fid,
		    le16toh(ltbuf[1]), le16toh(ltbuf[0])));
		break;
	}
	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO);
}

static int
wi_write_multi(struct wi_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int n;
	struct ifmultiaddr *ifma;
	struct wi_mcast mlist;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
allmulti:
		memset(&mlist, 0, sizeof(mlist));
		return wi_write_rid(sc, WI_RID_MCAST_LIST, &mlist,
		    sizeof(mlist));
	}

	n = 0;
#if __FreeBSD_version < 500000
	LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#else
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#endif
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (n >= 16)
			goto allmulti;
		IEEE80211_ADDR_COPY(&mlist.wi_mcast[n],
		    (LLADDR((struct sockaddr_dl *)ifma->ifma_addr)));
		n++;
	}
	return wi_write_rid(sc, WI_RID_MCAST_LIST, &mlist,
	    IEEE80211_ADDR_LEN * n);
}

static void
wi_read_nicid(struct wi_softc *sc)
{
	struct wi_card_ident *id;
	char *p;
	int len;
	u_int16_t ver[4];

	/* getting chip identity */
	memset(ver, 0, sizeof(ver));
	len = sizeof(ver);
	wi_read_rid(sc, WI_RID_CARD_ID, ver, &len);
	device_printf(sc->sc_dev, "using ");

	sc->sc_firmware_type = WI_NOTYPE;
	for (id = wi_card_ident; id->card_name != NULL; id++) {
		if (le16toh(ver[0]) == id->card_id) {
			printf("%s", id->card_name);
			sc->sc_firmware_type = id->firm_type;
			break;
		}
	}
	if (sc->sc_firmware_type == WI_NOTYPE) {
		if (le16toh(ver[0]) & 0x8000) {
			printf("Unknown PRISM2 chip");
			sc->sc_firmware_type = WI_INTERSIL;
		} else {
			printf("Unknown Lucent chip");
			sc->sc_firmware_type = WI_LUCENT;
		}
	}

	/* get primary firmware version (Only Prism chips) */
	if (sc->sc_firmware_type != WI_LUCENT) {
		memset(ver, 0, sizeof(ver));
		len = sizeof(ver);
		wi_read_rid(sc, WI_RID_PRI_IDENTITY, ver, &len);
		sc->sc_pri_firmware_ver = le16toh(ver[2]) * 10000 +
		    le16toh(ver[3]) * 100 + le16toh(ver[1]);
	}

	/* get station firmware version */
	memset(ver, 0, sizeof(ver));
	len = sizeof(ver);
	wi_read_rid(sc, WI_RID_STA_IDENTITY, ver, &len);
	sc->sc_sta_firmware_ver = le16toh(ver[2]) * 10000 +
	    le16toh(ver[3]) * 100 + le16toh(ver[1]);
	if (sc->sc_firmware_type == WI_INTERSIL &&
	    (sc->sc_sta_firmware_ver == 10102 ||
	     sc->sc_sta_firmware_ver == 20102)) {
		char ident[12];
		memset(ident, 0, sizeof(ident));
		len = sizeof(ident);
		/* value should be the format like "V2.00-11" */
		if (wi_read_rid(sc, WI_RID_SYMBOL_IDENTITY, ident, &len) == 0 &&
		    *(p = (char *)ident) >= 'A' &&
		    p[2] == '.' && p[5] == '-' && p[8] == '\0') {
			sc->sc_firmware_type = WI_SYMBOL;
			sc->sc_sta_firmware_ver = (p[1] - '0') * 10000 +
			    (p[3] - '0') * 1000 + (p[4] - '0') * 100 +
			    (p[6] - '0') * 10 + (p[7] - '0');
		}
	}
	printf("\n");
	device_printf(sc->sc_dev, "%s Firmware: ",
	     sc->sc_firmware_type == WI_LUCENT ? "Lucent" :
	    (sc->sc_firmware_type == WI_SYMBOL ? "Symbol" : "Intersil"));
	if (sc->sc_firmware_type != WI_LUCENT)	/* XXX */
		printf("Primary (%u.%u.%u), ",
		    sc->sc_pri_firmware_ver / 10000,
		    (sc->sc_pri_firmware_ver % 10000) / 100,
		    sc->sc_pri_firmware_ver % 100);
	printf("Station (%u.%u.%u)\n",
	    sc->sc_sta_firmware_ver / 10000,
	    (sc->sc_sta_firmware_ver % 10000) / 100,
	    sc->sc_sta_firmware_ver % 100);
}

static int
wi_write_ssid(struct wi_softc *sc, int rid, u_int8_t *buf, int buflen)
{
	struct wi_ssid ssid;

	if (buflen > IEEE80211_NWID_LEN)
		return ENOBUFS;
	memset(&ssid, 0, sizeof(ssid));
	ssid.wi_len = htole16(buflen);
	memcpy(ssid.wi_ssid, buf, buflen);
	return wi_write_rid(sc, rid, &ssid, sizeof(ssid));
}

static int
wi_get_cfg(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_req wreq;
	struct wi_scan_res *res;
	int len, n, error, mif, val, off, i;

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	len = (wreq.wi_len - 1) * 2;
	if (len < sizeof(u_int16_t))
		return ENOSPC;
	if (len > sizeof(wreq.wi_val))
		len = sizeof(wreq.wi_val);

	switch (wreq.wi_type) {

	case WI_RID_IFACE_STATS:
		memcpy(wreq.wi_val, &sc->sc_stats, sizeof(sc->sc_stats));
		if (len < sizeof(sc->sc_stats))
			error = ENOSPC;
		else
			len = sizeof(sc->sc_stats);
		break;

	case WI_RID_ENCRYPTION:
	case WI_RID_TX_CRYPT_KEY:
	case WI_RID_DEFLT_CRYPT_KEYS:
	case WI_RID_TX_RATE:
		return ieee80211_cfgget(ifp, cmd, data);

	case WI_RID_MICROWAVE_OVEN:
		if (sc->sc_enabled && (sc->sc_flags & WI_FLAGS_HAS_MOR)) {
			error = wi_read_rid(sc, wreq.wi_type, wreq.wi_val,
			    &len);
			break;
		}
		wreq.wi_val[0] = htole16(sc->sc_microwave_oven);
		len = sizeof(u_int16_t);
		break;

	case WI_RID_DBM_ADJUST:
		if (sc->sc_enabled && (sc->sc_flags & WI_FLAGS_HAS_DBMADJUST)) {
			error = wi_read_rid(sc, wreq.wi_type, wreq.wi_val,
			    &len);
			break;
		}
		wreq.wi_val[0] = htole16(sc->sc_dbm_adjust);
		len = sizeof(u_int16_t);
		break;

	case WI_RID_ROAMING_MODE:
		if (sc->sc_enabled && (sc->sc_flags & WI_FLAGS_HAS_ROAMING)) {
			error = wi_read_rid(sc, wreq.wi_type, wreq.wi_val,
			    &len);
			break;
		}
		wreq.wi_val[0] = htole16(sc->sc_roaming_mode);
		len = sizeof(u_int16_t);
		break;

	case WI_RID_SYSTEM_SCALE:
		if (sc->sc_enabled && (sc->sc_flags & WI_FLAGS_HAS_SYSSCALE)) {
			error = wi_read_rid(sc, wreq.wi_type, wreq.wi_val,
			    &len);
			break;
		}
		wreq.wi_val[0] = htole16(sc->sc_system_scale);
		len = sizeof(u_int16_t);
		break;

	case WI_RID_FRAG_THRESH:
		if (sc->sc_enabled && (sc->sc_flags & WI_FLAGS_HAS_FRAGTHR)) {
			error = wi_read_rid(sc, wreq.wi_type, wreq.wi_val,
			    &len);
			break;
		}
		wreq.wi_val[0] = htole16(sc->sc_frag_thresh);
		len = sizeof(u_int16_t);
		break;

	case WI_RID_READ_APS:
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			return ieee80211_cfgget(ifp, cmd, data);
		if (sc->sc_scan_timer > 0) {
			error = EINPROGRESS;
			break;
		}
		n = sc->sc_naps;
		if (len < sizeof(n)) {
			error = ENOSPC;
			break;
		}
		if (len < sizeof(n) + sizeof(struct wi_apinfo) * n)
			n = (len - sizeof(n)) / sizeof(struct wi_apinfo);
		len = sizeof(n) + sizeof(struct wi_apinfo) * n;
		memcpy(wreq.wi_val, &n, sizeof(n));
		memcpy((caddr_t)wreq.wi_val + sizeof(n), sc->sc_aps,
		    sizeof(struct wi_apinfo) * n);
		break;

	case WI_RID_PRISM2:
		wreq.wi_val[0] = sc->sc_firmware_type != WI_LUCENT;
		len = sizeof(u_int16_t);
		break;

	case WI_RID_MIF:
		mif = wreq.wi_val[0];
		error = wi_cmd(sc, WI_CMD_READMIF, mif, 0, 0);
		val = CSR_READ_2(sc, WI_RESP0);
		wreq.wi_val[0] = val;
		len = sizeof(u_int16_t);
		break;

	case WI_RID_ZERO_CACHE:
	case WI_RID_PROCFRAME:		/* ignore for compatibility */
		/* XXX ??? */
		break;

	case WI_RID_READ_CACHE:
		return ieee80211_cfgget(ifp, cmd, data);

	case WI_RID_SCAN_RES:		/* compatibility interface */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			return ieee80211_cfgget(ifp, cmd, data);
		if (sc->sc_scan_timer > 0) {
			error = EINPROGRESS;
			break;
		}
		n = sc->sc_naps;
		off = sc->sc_firmware_type != WI_LUCENT ?
			sizeof(struct wi_scan_p2_hdr) : 0;
		if (len < off + sizeof(struct wi_scan_res) * n)
			n = (len - off) / sizeof(struct wi_scan_res);
		len = off + sizeof(struct wi_scan_res) * n;
		if (off != 0) {
			struct wi_scan_p2_hdr *p2 = (struct wi_scan_p2_hdr *)wreq.wi_val;
			/*
			 * Prepend Prism-specific header.
			 */
			if (len < sizeof(struct wi_scan_p2_hdr)) {
				error = ENOSPC;
				break;
			}
			p2 = (struct wi_scan_p2_hdr *)wreq.wi_val;
			p2->wi_rsvd = 0;
			p2->wi_reason = n;	/* XXX */
		}
		for (i = 0; i < n; i++) {
			const struct wi_apinfo *ap = &sc->sc_aps[i];

			res = (struct wi_scan_res *)((char *)wreq.wi_val + off);
			res->wi_chan = ap->channel;
			res->wi_noise = ap->noise;
			res->wi_signal = ap->signal;
			IEEE80211_ADDR_COPY(res->wi_bssid, ap->bssid);
			res->wi_interval = ap->interval;
			res->wi_capinfo = ap->capinfo;
			res->wi_ssid_len = ap->namelen;
			memcpy(res->wi_ssid, ap->name,
				IEEE80211_NWID_LEN);
			if (sc->sc_firmware_type != WI_LUCENT) {
				/* XXX not saved from Prism cards */
				memset(res->wi_srates, 0,
					sizeof(res->wi_srates));
				res->wi_rate = ap->rate;
				res->wi_rsvd = 0;
				off += WI_PRISM2_RES_SIZE;
			} else
				off += WI_WAVELAN_RES_SIZE;
		}
		break;

	default:
		if (sc->sc_enabled) {
			error = wi_read_rid(sc, wreq.wi_type, wreq.wi_val,
			    &len);
			break;
		}
		switch (wreq.wi_type) {
		case WI_RID_MAX_DATALEN:
			wreq.wi_val[0] = htole16(sc->sc_max_datalen);
			len = sizeof(u_int16_t);
			break;
		case WI_RID_RTS_THRESH:
			wreq.wi_val[0] = htole16(sc->sc_rts_thresh);
			len = sizeof(u_int16_t);
			break;
		case WI_RID_CNFAUTHMODE:
			wreq.wi_val[0] = htole16(sc->sc_cnfauthmode);
			len = sizeof(u_int16_t);
			break;
		case WI_RID_NODENAME:
			if (len < sc->sc_nodelen + sizeof(u_int16_t)) {
				error = ENOSPC;
				break;
			}
			len = sc->sc_nodelen + sizeof(u_int16_t);
			wreq.wi_val[0] = htole16((sc->sc_nodelen + 1) / 2);
			memcpy(&wreq.wi_val[1], sc->sc_nodename,
			    sc->sc_nodelen);
			break;
		default:
			return ieee80211_cfgget(ifp, cmd, data);
		}
		break;
	}
	if (error)
		return error;
	wreq.wi_len = (len + 1) / 2 + 1;
	return copyout(&wreq, ifr->ifr_data, (wreq.wi_len + 1) * 2);
}

static int
wi_set_cfg(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_req wreq;
	struct mbuf *m;
	int i, len, error, mif, val;

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	len = wreq.wi_len ? (wreq.wi_len - 1) * 2 : 0;
	switch (wreq.wi_type) {
	case WI_RID_DBM_ADJUST:
		return ENODEV;

	case WI_RID_NODENAME:
		if (le16toh(wreq.wi_val[0]) * 2 > len ||
		    le16toh(wreq.wi_val[0]) > sizeof(sc->sc_nodename)) {
			error = ENOSPC;
			break;
		}
		if (sc->sc_enabled) {
			error = wi_write_rid(sc, wreq.wi_type, wreq.wi_val,
			    len);
			if (error)
				break;
		}
		sc->sc_nodelen = le16toh(wreq.wi_val[0]) * 2;
		memcpy(sc->sc_nodename, &wreq.wi_val[1], sc->sc_nodelen);
		break;

	case WI_RID_MICROWAVE_OVEN:
	case WI_RID_ROAMING_MODE:
	case WI_RID_SYSTEM_SCALE:
	case WI_RID_FRAG_THRESH:
		if (wreq.wi_type == WI_RID_MICROWAVE_OVEN &&
		    (sc->sc_flags & WI_FLAGS_HAS_MOR) == 0)
			break;
		if (wreq.wi_type == WI_RID_ROAMING_MODE &&
		    (sc->sc_flags & WI_FLAGS_HAS_ROAMING) == 0)
			break;
		if (wreq.wi_type == WI_RID_SYSTEM_SCALE &&
		    (sc->sc_flags & WI_FLAGS_HAS_SYSSCALE) == 0)
			break;
		if (wreq.wi_type == WI_RID_FRAG_THRESH &&
		    (sc->sc_flags & WI_FLAGS_HAS_FRAGTHR) == 0)
			break;
		/* FALLTHROUGH */
	case WI_RID_RTS_THRESH:
	case WI_RID_CNFAUTHMODE:
	case WI_RID_MAX_DATALEN:
		if (sc->sc_enabled) {
			error = wi_write_rid(sc, wreq.wi_type, wreq.wi_val,
			    sizeof(u_int16_t));
			if (error)
				break;
		}
		switch (wreq.wi_type) {
		case WI_RID_FRAG_THRESH:
			sc->sc_frag_thresh = le16toh(wreq.wi_val[0]);
			break;
		case WI_RID_RTS_THRESH:
			sc->sc_rts_thresh = le16toh(wreq.wi_val[0]);
			break;
		case WI_RID_MICROWAVE_OVEN:
			sc->sc_microwave_oven = le16toh(wreq.wi_val[0]);
			break;
		case WI_RID_ROAMING_MODE:
			sc->sc_roaming_mode = le16toh(wreq.wi_val[0]);
			break;
		case WI_RID_SYSTEM_SCALE:
			sc->sc_system_scale = le16toh(wreq.wi_val[0]);
			break;
		case WI_RID_CNFAUTHMODE:
			sc->sc_cnfauthmode = le16toh(wreq.wi_val[0]);
			break;
		case WI_RID_MAX_DATALEN:
			sc->sc_max_datalen = le16toh(wreq.wi_val[0]);
			break;
		}
		break;

	case WI_RID_TX_RATE:
		switch (le16toh(wreq.wi_val[0])) {
		case 3:
			ic->ic_fixed_rate = -1;
			break;
		default:
			for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
				if ((ic->ic_sup_rates[i] & IEEE80211_RATE_VAL)
				    / 2 == le16toh(wreq.wi_val[0]))
					break;
			}
			if (i == IEEE80211_RATE_SIZE)
				return EINVAL;
			ic->ic_fixed_rate = i;
		}
		if (sc->sc_enabled)
			error = wi_write_txrate(sc);
		break;

	case WI_RID_SCAN_APS:
		if (sc->sc_enabled && ic->ic_opmode != IEEE80211_M_HOSTAP)
			error = wi_scan_ap(sc, 0x3fff, 0x000f);
		break;

	case WI_RID_SCAN_REQ:		/* compatibility interface */
		if (sc->sc_enabled && ic->ic_opmode != IEEE80211_M_HOSTAP)
			error = wi_scan_ap(sc, wreq.wi_val[0], wreq.wi_val[1]);
		break;

	case WI_RID_MGMT_XMIT:
		if (!sc->sc_enabled) {
			error = ENETDOWN;
			break;
		}
		if (ic->ic_mgtq.ifq_len > 5) {
			error = EAGAIN;
			break;
		}
		/* XXX wi_len looks in u_int8_t, not in u_int16_t */
		m = m_devget((char *)&wreq.wi_val, wreq.wi_len, 0, ifp, NULL);
		if (m == NULL) {
			error = ENOMEM;
			break;
		}
		IF_ENQUEUE(&ic->ic_mgtq, m);
		break;

	case WI_RID_MIF:
		mif = wreq.wi_val[0];
		val = wreq.wi_val[1];
		error = wi_cmd(sc, WI_CMD_WRITEMIF, mif, val, 0);
		break;

	case WI_RID_PROCFRAME:		/* ignore for compatibility */
		break;

	case WI_RID_OWN_SSID:
		if (le16toh(wreq.wi_val[0]) * 2 > len ||
		    le16toh(wreq.wi_val[0]) > IEEE80211_NWID_LEN) {
			error = ENOSPC;
			break;
		}
		memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
		ic->ic_des_esslen = le16toh(wreq.wi_val[0]) * 2;
		memcpy(ic->ic_des_essid, &wreq.wi_val[1], ic->ic_des_esslen);
		error = ENETRESET;
		break;

	default:
		if (sc->sc_enabled) {
			error = wi_write_rid(sc, wreq.wi_type, wreq.wi_val,
			    len);
			if (error)
				break;
		}
		error = ieee80211_cfgset(ifp, cmd, data);
		break;
	}
	return error;
}

static int
wi_write_txrate(struct wi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int i;
	u_int16_t rate;

	if (ic->ic_fixed_rate < 0)
		rate = 0;	/* auto */
	else
		rate = (ic->ic_sup_rates[ic->ic_fixed_rate] &
		    IEEE80211_RATE_VAL) / 2;

	/* rate: 0, 1, 2, 5, 11 */

	switch (sc->sc_firmware_type) {
	case WI_LUCENT:
		switch (rate) {
		case 0:			/* auto == 11mbps auto */
			rate = 3;
			break;
		/* case 1, 2 map to 1, 2*/
		case 5:			/* 5.5Mbps -> 4 */
			rate = 4;
			break;
		case 11:		/* 11mbps -> 5 */
			rate = 5;
			break;
		default:
			break;
		}
		break;
	default:
		/* Choose a bit according to this table.
		 *
		 * bit | data rate
		 * ----+-------------------
		 * 0   | 1Mbps
		 * 1   | 2Mbps
		 * 2   | 5.5Mbps
		 * 3   | 11Mbps
		 */
		for (i = 8; i > 0; i >>= 1) {
			if (rate >= i)
				break;
		}
		if (i == 0)
			rate = 0xf;	/* auto */
		else
			rate = i;
		break;
	}
	return wi_write_val(sc, WI_RID_TX_RATE, rate);
}

static int
wi_write_wep(struct wi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;
	int i, keylen;
	u_int16_t val;
	struct wi_key wkey[IEEE80211_WEP_NKID];

	switch (sc->sc_firmware_type) {
	case WI_LUCENT:
		val = (ic->ic_flags & IEEE80211_F_WEPON) ? 1 : 0;
		error = wi_write_val(sc, WI_RID_ENCRYPTION, val);
		if (error)
			break;
		error = wi_write_val(sc, WI_RID_TX_CRYPT_KEY, ic->ic_wep_txkey);
		if (error)
			break;
		memset(wkey, 0, sizeof(wkey));
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			keylen = ic->ic_nw_keys[i].wk_len;
			wkey[i].wi_keylen = htole16(keylen);
			memcpy(wkey[i].wi_keydat, ic->ic_nw_keys[i].wk_key,
			    keylen);
		}
		error = wi_write_rid(sc, WI_RID_DEFLT_CRYPT_KEYS,
		    wkey, sizeof(wkey));
		break;

	case WI_INTERSIL:
	case WI_SYMBOL:
		if (ic->ic_flags & IEEE80211_F_WEPON) {
			/*
			 * ONLY HWB3163 EVAL-CARD Firmware version
			 * less than 0.8 variant2
			 *
			 *   If promiscuous mode disable, Prism2 chip
			 *  does not work with WEP .
			 * It is under investigation for details.
			 * (ichiro@netbsd.org)
			 */
			if (sc->sc_firmware_type == WI_INTERSIL &&
			    sc->sc_sta_firmware_ver < 802 ) {
				/* firm ver < 0.8 variant 2 */
				wi_write_val(sc, WI_RID_PROMISC, 1);
			}
			wi_write_val(sc, WI_RID_CNFAUTHMODE,
			    sc->sc_cnfauthmode);
			val = PRIVACY_INVOKED | EXCLUDE_UNENCRYPTED;
			/*
			 * Encryption firmware has a bug for HostAP mode.
			 */
			if (sc->sc_firmware_type == WI_INTERSIL &&
			    ic->ic_opmode == IEEE80211_M_HOSTAP)
				val |= HOST_ENCRYPT;
		} else {
			wi_write_val(sc, WI_RID_CNFAUTHMODE,
			    IEEE80211_AUTH_OPEN);
			val = HOST_ENCRYPT | HOST_DECRYPT;
		}
		error = wi_write_val(sc, WI_RID_P2_ENCRYPTION, val);
		if (error)
			break;
		error = wi_write_val(sc, WI_RID_P2_TX_CRYPT_KEY,
		    ic->ic_wep_txkey);
		if (error)
			break;
		/*
		 * It seems that the firmware accept 104bit key only if
		 * all the keys have 104bit length.  We get the length of
		 * the transmit key and use it for all other keys.
		 * Perhaps we should use software WEP for such situation.
		 */
		keylen = ic->ic_nw_keys[ic->ic_wep_txkey].wk_len;
		if (keylen > IEEE80211_WEP_KEYLEN)
			keylen = 13;	/* 104bit keys */
		else
			keylen = IEEE80211_WEP_KEYLEN;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			error = wi_write_rid(sc, WI_RID_P2_CRYPT_KEY0 + i,
			    ic->ic_nw_keys[i].wk_key, keylen);
			if (error)
				break;
		}
		break;
	}
	return error;
}

static int
wi_cmd(struct wi_softc *sc, int cmd, int val0, int val1, int val2)
{
	int			i, s = 0;
	static volatile int count  = 0;
	
	if (count > 0)
		panic("Hey partner, hold on there!");
	count++;

	/* wait for the busy bit to clear */
	for (i = 500; i > 0; i--) {	/* 5s */
		if (!(CSR_READ_2(sc, WI_COMMAND) & WI_CMD_BUSY))
			break;
		DELAY(10*1000);	/* 10 m sec */
	}
	if (i == 0) {
		device_printf(sc->sc_dev, "wi_cmd: busy bit won't clear.\n" );
		count--;
		return(ETIMEDOUT);
	}

	CSR_WRITE_2(sc, WI_PARAM0, val0);
	CSR_WRITE_2(sc, WI_PARAM1, val1);
	CSR_WRITE_2(sc, WI_PARAM2, val2);
	CSR_WRITE_2(sc, WI_COMMAND, cmd);

	if (cmd == WI_CMD_INI) {
		/* XXX: should sleep here. */
		DELAY(100*1000);		/* 100ms delay for init */
	}
	for (i = 0; i < WI_TIMEOUT; i++) {
		/*
		 * Wait for 'command complete' bit to be
		 * set in the event status register.
		 */
		s = CSR_READ_2(sc, WI_EVENT_STAT);
		if (s & WI_EV_CMD) {
			/* Ack the event and read result code. */
			s = CSR_READ_2(sc, WI_STATUS);
			CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_CMD);
			if (s & WI_STAT_CMD_RESULT) {
				count--;
				return(EIO);
			}
			break;
		}
		DELAY(WI_DELAY);
	}

	count--;
	if (i == WI_TIMEOUT) {
		device_printf(sc->sc_dev,
		    "timeout in wi_cmd 0x%04x; event status 0x%04x\n", cmd, s);
		return(ETIMEDOUT);
	}
	return (0);
}

static int
wi_seek_bap(struct wi_softc *sc, int id, int off)
{
	int i, status;

	CSR_WRITE_2(sc, WI_SEL0, id);
	CSR_WRITE_2(sc, WI_OFF0, off);

	for (i = 0; ; i++) {
		status = CSR_READ_2(sc, WI_OFF0);
		if ((status & WI_OFF_BUSY) == 0)
			break;
		if (i == WI_TIMEOUT) {
			device_printf(sc->sc_dev, "timeout in wi_seek to %x/%x\n",
			    id, off);
			sc->sc_bap_off = WI_OFF_ERR;	/* invalidate */
			return ETIMEDOUT;
		}
		DELAY(1);
	}
	if (status & WI_OFF_ERR) {
		device_printf(sc->sc_dev, "failed in wi_seek to %x/%x\n", id, off);
		sc->sc_bap_off = WI_OFF_ERR;	/* invalidate */
		return EIO;
	}
	sc->sc_bap_id = id;
	sc->sc_bap_off = off;
	return 0;
}

static int
wi_read_bap(struct wi_softc *sc, int id, int off, void *buf, int buflen)
{
	u_int16_t *ptr;
	int i, error, cnt;

	if (buflen == 0)
		return 0;
	if (id != sc->sc_bap_id || off != sc->sc_bap_off) {
		if ((error = wi_seek_bap(sc, id, off)) != 0)
			return error;
	}
	cnt = (buflen + 1) / 2;
	ptr = (u_int16_t *)buf;
	for (i = 0; i < cnt; i++)
		*ptr++ = CSR_READ_2(sc, WI_DATA0);
	sc->sc_bap_off += cnt * 2;
	return 0;
}

static int
wi_write_bap(struct wi_softc *sc, int id, int off, void *buf, int buflen)
{
	u_int16_t *ptr;
	int i, error, cnt;

	if (buflen == 0)
		return 0;

#ifdef WI_HERMES_AUTOINC_WAR
  again:
#endif
	if (id != sc->sc_bap_id || off != sc->sc_bap_off) {
		if ((error = wi_seek_bap(sc, id, off)) != 0)
			return error;
	}
	cnt = (buflen + 1) / 2;
	ptr = (u_int16_t *)buf;
	for (i = 0; i < cnt; i++)
		CSR_WRITE_2(sc, WI_DATA0, ptr[i]);
	sc->sc_bap_off += cnt * 2;

#ifdef WI_HERMES_AUTOINC_WAR
	/*
	 * According to the comments in the HCF Light code, there is a bug
	 * in the Hermes (or possibly in certain Hermes firmware revisions)
	 * where the chip's internal autoincrement counter gets thrown off
	 * during data writes:  the autoincrement is missed, causing one
	 * data word to be overwritten and subsequent words to be written to
	 * the wrong memory locations. The end result is that we could end
	 * up transmitting bogus frames without realizing it. The workaround
	 * for this is to write a couple of extra guard words after the end
	 * of the transfer, then attempt to read then back. If we fail to
	 * locate the guard words where we expect them, we preform the
	 * transfer over again.
	 */
	if ((sc->sc_flags & WI_FLAGS_BUG_AUTOINC) && (id & 0xf000) == 0) {
		CSR_WRITE_2(sc, WI_DATA0, 0x1234);
		CSR_WRITE_2(sc, WI_DATA0, 0x5678);
		wi_seek_bap(sc, id, sc->sc_bap_off);
		sc->sc_bap_off = WI_OFF_ERR;	/* invalidate */
		if (CSR_READ_2(sc, WI_DATA0) != 0x1234 ||
		    CSR_READ_2(sc, WI_DATA0) != 0x5678) {
			device_printf(sc->sc_dev,
				"detect auto increment bug, try again\n");
			goto again;
		}
	}
#endif
	return 0;
}

static int
wi_mwrite_bap(struct wi_softc *sc, int id, int off, struct mbuf *m0, int totlen)
{
	int error, len;
	struct mbuf *m;

	for (m = m0; m != NULL && totlen > 0; m = m->m_next) {
		if (m->m_len == 0)
			continue;

		len = min(m->m_len, totlen);

		if (((u_long)m->m_data) % 2 != 0 || len % 2 != 0) {
			m_copydata(m, 0, totlen, (caddr_t)&sc->sc_txbuf);
			return wi_write_bap(sc, id, off, (caddr_t)&sc->sc_txbuf,
			    totlen);
		}

		if ((error = wi_write_bap(sc, id, off, m->m_data, len)) != 0)
			return error;

		off += m->m_len;
		totlen -= len;
	}
	return 0;
}

static int
wi_alloc_fid(struct wi_softc *sc, int len, int *idp)
{
	int i;

	if (wi_cmd(sc, WI_CMD_ALLOC_MEM, len, 0, 0)) {
		device_printf(sc->sc_dev, "failed to allocate %d bytes on NIC\n",
		    len);
		return ENOMEM;
	}

	for (i = 0; i < WI_TIMEOUT; i++) {
		if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_ALLOC)
			break;
		if (i == WI_TIMEOUT) {
			device_printf(sc->sc_dev, "timeout in alloc\n");
			return ETIMEDOUT;
		}
		DELAY(1);
	}
	*idp = CSR_READ_2(sc, WI_ALLOC_FID);
	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
	return 0;
}

static int
wi_read_rid(struct wi_softc *sc, int rid, void *buf, int *buflenp)
{
	int error, len;
	u_int16_t ltbuf[2];

	/* Tell the NIC to enter record read mode. */
	error = wi_cmd(sc, WI_CMD_ACCESS | WI_ACCESS_READ, rid, 0, 0);
	if (error)
		return error;

	error = wi_read_bap(sc, rid, 0, ltbuf, sizeof(ltbuf));
	if (error)
		return error;

	if (le16toh(ltbuf[1]) != rid) {
		device_printf(sc->sc_dev, "record read mismatch, rid=%x, got=%x\n",
		    rid, le16toh(ltbuf[1]));
		return EIO;
	}
	len = (le16toh(ltbuf[0]) - 1) * 2;	 /* already got rid */
	if (*buflenp < len) {
		device_printf(sc->sc_dev, "record buffer is too small, "
		    "rid=%x, size=%d, len=%d\n",
		    rid, *buflenp, len);
		return ENOSPC;
	}
	*buflenp = len;
	return wi_read_bap(sc, rid, sizeof(ltbuf), buf, len);
}

static int
wi_write_rid(struct wi_softc *sc, int rid, void *buf, int buflen)
{
	int error;
	u_int16_t ltbuf[2];

	ltbuf[0] = htole16((buflen + 1) / 2 + 1);	 /* includes rid */
	ltbuf[1] = htole16(rid);

	error = wi_write_bap(sc, rid, 0, ltbuf, sizeof(ltbuf));
	if (error)
		return error;
	error = wi_write_bap(sc, rid, sizeof(ltbuf), buf, buflen);
	if (error)
		return error;

	return wi_cmd(sc, WI_CMD_ACCESS | WI_ACCESS_WRITE, rid, 0, 0);
}

static int
wi_newstate(void *arg, enum ieee80211_state nstate)
{
	struct wi_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &ic->ic_bss;
	int i, buflen;
	u_int16_t val;
	struct wi_ssid ssid;
	u_int8_t old_bssid[IEEE80211_ADDR_LEN];
	enum ieee80211_state ostate;
#ifdef WI_DEBUG
	static const char *stname[] =
	    { "INIT", "SCAN", "AUTH", "ASSOC", "RUN" };
#endif /* WI_DEBUG */

	ostate = ic->ic_state;
	DPRINTF(("wi_newstate: %s -> %s\n", stname[ostate], stname[nstate]));

	ic->ic_state = nstate;
	switch (nstate) {
	case IEEE80211_S_INIT:
		ic->ic_flags &= ~IEEE80211_F_SIBSS;
		sc->sc_flags &= ~WI_FLAGS_OUTRANGE;
		return 0;

	case IEEE80211_S_RUN:
		sc->sc_flags &= ~WI_FLAGS_OUTRANGE;
		buflen = IEEE80211_ADDR_LEN;
		wi_read_rid(sc, WI_RID_CURRENT_BSSID, ni->ni_bssid, &buflen);
		IEEE80211_ADDR_COPY(ni->ni_macaddr, ni->ni_bssid);
		buflen = sizeof(val);
		wi_read_rid(sc, WI_RID_CURRENT_CHAN, &val, &buflen);
		ni->ni_chan = le16toh(val);

		if (IEEE80211_ADDR_EQ(old_bssid, ni->ni_bssid))
			sc->sc_false_syns++;
		else
			sc->sc_false_syns = 0;

		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			ni->ni_esslen = ic->ic_des_esslen;
			memcpy(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen);
			ni->ni_nrate = 0;
			for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
				if (ic->ic_sup_rates[i])
					ni->ni_rates[ni->ni_nrate++] =
					    ic->ic_sup_rates[i];
			}
			ni->ni_intval = ic->ic_lintval;
			ni->ni_capinfo = IEEE80211_CAPINFO_ESS;
			if (ic->ic_flags & IEEE80211_F_WEPON)
				ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
		} else {
			/* XXX check return value */
			buflen = sizeof(ssid);
			wi_read_rid(sc, WI_RID_CURRENT_SSID, &ssid, &buflen);
			ni->ni_esslen = le16toh(ssid.wi_len);
			if (ni->ni_esslen > IEEE80211_NWID_LEN)
				ni->ni_esslen = IEEE80211_NWID_LEN;	/*XXX*/
			memcpy(ni->ni_essid, ssid.wi_ssid, ni->ni_esslen);
		}
		break;

	case IEEE80211_S_SCAN:
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		break;
	}

	/* skip standard ieee80211 handling */
	return EINPROGRESS;
}

static int
wi_scan_ap(struct wi_softc *sc, u_int16_t chanmask, u_int16_t txrate)
{
	int error = 0;
	u_int16_t val[2];

	if (!sc->sc_enabled)
		return ENXIO;
	switch (sc->sc_firmware_type) {
	case WI_LUCENT:
		(void)wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_SCAN_RESULTS, 0, 0);
		break;
	case WI_INTERSIL:
		val[0] = chanmask;	/* channel */
		val[1] = txrate;	/* tx rate */
		error = wi_write_rid(sc, WI_RID_SCAN_REQ, val, sizeof(val));
		break;
	case WI_SYMBOL:
		/*
		 * XXX only supported on 3.x ?
		 */
		val[0] = BSCAN_BCAST | BSCAN_ONETIME;
		error = wi_write_rid(sc, WI_RID_BCAST_SCAN_REQ,
		    val, sizeof(val[0]));
		break;
	}
	if (error == 0) {
		sc->sc_scan_timer = WI_SCAN_WAIT;
		sc->sc_ic.ic_if.if_timer = 1;
		DPRINTF(("wi_scan_ap: start scanning, "
			"chamask 0x%x txrate 0x%x\n", chanmask, txrate));
	}
	return error;
}

static void
wi_scan_result(struct wi_softc *sc, int fid, int cnt)
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
	int i, naps, off, szbuf;
	struct wi_scan_header ws_hdr;	/* Prism2 header */
	struct wi_scan_data_p2 ws_dat;	/* Prism2 scantable*/
	struct wi_apinfo *ap;

	off = sizeof(u_int16_t) * 2;
	memset(&ws_hdr, 0, sizeof(ws_hdr));
	switch (sc->sc_firmware_type) {
	case WI_INTERSIL:
		wi_read_bap(sc, fid, off, &ws_hdr, sizeof(ws_hdr));
		off += sizeof(ws_hdr);
		szbuf = sizeof(struct wi_scan_data_p2);
		break;
	case WI_SYMBOL:
		szbuf = sizeof(struct wi_scan_data_p2) + 6;
		break;
	case WI_LUCENT:
		szbuf = sizeof(struct wi_scan_data);
		break;
	default:
		device_printf(sc->sc_dev,
			"wi_scan_result: unknown firmware type %u\n",
			sc->sc_firmware_type);
		naps = 0;
		goto done;
	}
	naps = (cnt * 2 + 2 - off) / szbuf;
	if (naps > N(sc->sc_aps))
		naps = N(sc->sc_aps);
	sc->sc_naps = naps;
	/* Read Data */
	ap = sc->sc_aps;
	memset(&ws_dat, 0, sizeof(ws_dat));
	for (i = 0; i < naps; i++, ap++) {
		wi_read_bap(sc, fid, off, &ws_dat,
		    (sizeof(ws_dat) < szbuf ? sizeof(ws_dat) : szbuf));
		DPRINTF2(("wi_scan_result: #%d: off %d bssid %s\n", i, off,
		    ether_sprintf(ws_dat.wi_bssid)));
		off += szbuf;
		ap->scanreason = le16toh(ws_hdr.wi_reason);
		memcpy(ap->bssid, ws_dat.wi_bssid, sizeof(ap->bssid));
		ap->channel = le16toh(ws_dat.wi_chid);
		ap->signal  = le16toh(ws_dat.wi_signal);
		ap->noise   = le16toh(ws_dat.wi_noise);
		ap->quality = ap->signal - ap->noise;
		ap->capinfo = le16toh(ws_dat.wi_capinfo);
		ap->interval = le16toh(ws_dat.wi_interval);
		ap->rate    = le16toh(ws_dat.wi_rate);
		ap->namelen = le16toh(ws_dat.wi_namelen);
		if (ap->namelen > sizeof(ap->name))
			ap->namelen = sizeof(ap->name);
		memcpy(ap->name, ws_dat.wi_name, ap->namelen);
	}
done:
	/* Done scanning */
	sc->sc_scan_timer = 0;
	DPRINTF(("wi_scan_result: scan complete: ap %d\n", naps));
#undef N
}

static void
wi_dump_pkt(struct wi_frame *wh, struct ieee80211_node *ni, int rssi)
{
	ieee80211_dump_pkt((u_int8_t *) &wh->wi_whdr, sizeof(wh->wi_whdr),
	    ni ? ni->ni_rates[ni->ni_txrate] & IEEE80211_RATE_VAL : -1, rssi);
	printf(" status 0x%x rx_tstamp1 %u rx_tstamp0 0x%u rx_silence %u\n",
		le16toh(wh->wi_status), le16toh(wh->wi_rx_tstamp1),
		le16toh(wh->wi_rx_tstamp0), wh->wi_rx_silence);
	printf(" rx_signal %u rx_rate %u rx_flow %u\n",
		wh->wi_rx_signal, wh->wi_rx_rate, wh->wi_rx_flow);
	printf(" tx_rtry %u tx_rate %u tx_ctl 0x%x dat_len %u\n",
		wh->wi_tx_rtry, wh->wi_tx_rate,
		le16toh(wh->wi_tx_ctl), le16toh(wh->wi_dat_len));
	printf(" ehdr dst %6D src %6D type 0x%x\n",
		wh->wi_ehdr.ether_dhost, ":", wh->wi_ehdr.ether_shost, ":",
		wh->wi_ehdr.ether_type);
}

int
wi_alloc(device_t dev, int rid)
{
	struct wi_softc	*sc = device_get_softc(dev);

	if (sc->wi_bus_type != WI_BUS_PCI_NATIVE) {
		sc->iobase_rid = rid;
		sc->iobase = bus_alloc_resource(dev, SYS_RES_IOPORT,
		    &sc->iobase_rid, 0, ~0, (1 << 6),
		    rman_make_alignment_flags(1 << 6) | RF_ACTIVE);
		if (!sc->iobase) {
			device_printf(dev, "No I/O space?!\n");
			return (ENXIO);
		}

		sc->wi_io_addr = rman_get_start(sc->iobase);
		sc->wi_btag = rman_get_bustag(sc->iobase);
		sc->wi_bhandle = rman_get_bushandle(sc->iobase);
	} else {
		sc->mem_rid = rid;
		sc->mem = bus_alloc_resource(dev, SYS_RES_MEMORY,
		    &sc->mem_rid, 0, ~0, 1, RF_ACTIVE);

		if (!sc->mem) {
			device_printf(dev, "No Mem space on prism2.5?\n");
			return (ENXIO);
		}

		sc->wi_btag = rman_get_bustag(sc->mem);
		sc->wi_bhandle = rman_get_bushandle(sc->mem);
	}


	sc->irq_rid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
	    0, ~0, 1, RF_ACTIVE |
	    ((sc->wi_bus_type == WI_BUS_PCCARD) ? 0 : RF_SHAREABLE));

	if (!sc->irq) {
		wi_free(dev);
		device_printf(dev, "No irq?!\n");
		return (ENXIO);
	}

	sc->sc_dev = dev;
	sc->sc_unit = device_get_unit(dev);

	return (0);
}

void
wi_free(device_t dev)
{
	struct wi_softc	*sc = device_get_softc(dev);

	if (sc->iobase != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iobase_rid, sc->iobase);
		sc->iobase = NULL;
	}
	if (sc->irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
		sc->irq = NULL;
	}
	if (sc->mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem);
		sc->mem = NULL;
	}

	return;
}

static int
wi_get_debug(struct wi_softc *sc, struct wi_req *wreq)
{
	int error = 0;

	wreq->wi_len = 1;

	switch (wreq->wi_type) {
	case WI_DEBUG_SLEEP:
		wreq->wi_len++;
		wreq->wi_val[0] = sc->wi_debug.wi_sleep;
		break;
	case WI_DEBUG_DELAYSUPP:
		wreq->wi_len++;
		wreq->wi_val[0] = sc->wi_debug.wi_delaysupp;
		break;
	case WI_DEBUG_TXSUPP:
		wreq->wi_len++;
		wreq->wi_val[0] = sc->wi_debug.wi_txsupp;
		break;
	case WI_DEBUG_MONITOR:
		wreq->wi_len++;
		wreq->wi_val[0] = sc->wi_debug.wi_monitor;
		break;
	case WI_DEBUG_LEDTEST:
		wreq->wi_len += 3;
		wreq->wi_val[0] = sc->wi_debug.wi_ledtest;
		wreq->wi_val[1] = sc->wi_debug.wi_ledtest_param0;
		wreq->wi_val[2] = sc->wi_debug.wi_ledtest_param1;
		break;
	case WI_DEBUG_CONTTX:
		wreq->wi_len += 2;
		wreq->wi_val[0] = sc->wi_debug.wi_conttx;
		wreq->wi_val[1] = sc->wi_debug.wi_conttx_param0;
		break;
	case WI_DEBUG_CONTRX:
		wreq->wi_len++;
		wreq->wi_val[0] = sc->wi_debug.wi_contrx;
		break;
	case WI_DEBUG_SIGSTATE:
		wreq->wi_len += 2;
		wreq->wi_val[0] = sc->wi_debug.wi_sigstate;
		wreq->wi_val[1] = sc->wi_debug.wi_sigstate_param0;
		break;
	case WI_DEBUG_CONFBITS:
		wreq->wi_len += 2;
		wreq->wi_val[0] = sc->wi_debug.wi_confbits;
		wreq->wi_val[1] = sc->wi_debug.wi_confbits_param0;
		break;
	default:
		error = EIO;
		break;
	}

	return (error);
}

static int
wi_set_debug(struct wi_softc *sc, struct wi_req *wreq)
{
	int error = 0;
	u_int16_t		cmd, param0 = 0, param1 = 0;

	switch (wreq->wi_type) {
	case WI_DEBUG_RESET:
	case WI_DEBUG_INIT:
	case WI_DEBUG_CALENABLE:
		break;
	case WI_DEBUG_SLEEP:
		sc->wi_debug.wi_sleep = 1;
		break;
	case WI_DEBUG_WAKE:
		sc->wi_debug.wi_sleep = 0;
		break;
	case WI_DEBUG_CHAN:
		param0 = wreq->wi_val[0];
		break;
	case WI_DEBUG_DELAYSUPP:
		sc->wi_debug.wi_delaysupp = 1;
		break;
	case WI_DEBUG_TXSUPP:
		sc->wi_debug.wi_txsupp = 1;
		break;
	case WI_DEBUG_MONITOR:
		sc->wi_debug.wi_monitor = 1;
		break;
	case WI_DEBUG_LEDTEST:
		param0 = wreq->wi_val[0];
		param1 = wreq->wi_val[1];
		sc->wi_debug.wi_ledtest = 1;
		sc->wi_debug.wi_ledtest_param0 = param0;
		sc->wi_debug.wi_ledtest_param1 = param1;
		break;
	case WI_DEBUG_CONTTX:
		param0 = wreq->wi_val[0];
		sc->wi_debug.wi_conttx = 1;
		sc->wi_debug.wi_conttx_param0 = param0;
		break;
	case WI_DEBUG_STOPTEST:
		sc->wi_debug.wi_delaysupp = 0;
		sc->wi_debug.wi_txsupp = 0;
		sc->wi_debug.wi_monitor = 0;
		sc->wi_debug.wi_ledtest = 0;
		sc->wi_debug.wi_ledtest_param0 = 0;
		sc->wi_debug.wi_ledtest_param1 = 0;
		sc->wi_debug.wi_conttx = 0;
		sc->wi_debug.wi_conttx_param0 = 0;
		sc->wi_debug.wi_contrx = 0;
		sc->wi_debug.wi_sigstate = 0;
		sc->wi_debug.wi_sigstate_param0 = 0;
		break;
	case WI_DEBUG_CONTRX:
		sc->wi_debug.wi_contrx = 1;
		break;
	case WI_DEBUG_SIGSTATE:
		param0 = wreq->wi_val[0];
		sc->wi_debug.wi_sigstate = 1;
		sc->wi_debug.wi_sigstate_param0 = param0;
		break;
	case WI_DEBUG_CONFBITS:
		param0 = wreq->wi_val[0];
		param1 = wreq->wi_val[1];
		sc->wi_debug.wi_confbits = param0;
		sc->wi_debug.wi_confbits_param0 = param1;
		break;
	default:
		error = EIO;
		break;
	}

	if (error)
		return (error);

	cmd = WI_CMD_DEBUG | (wreq->wi_type << 8);
	error = wi_cmd(sc, cmd, param0, param1, 0);

	return (error);
}

#if __FreeBSD_version >= 500000
/*
 * Special routines to download firmware for Symbol CF card.
 * XXX: This should be modified generic into any PRISM-2 based card.
 */

#define	WI_SBCF_PDIADDR		0x3100

/* unaligned load little endian */
#define	GETLE32(p)	((p)[0] | ((p)[1]<<8) | ((p)[2]<<16) | ((p)[3]<<24))
#define	GETLE16(p)	((p)[0] | ((p)[1]<<8))

int
wi_symbol_load_firm(struct wi_softc *sc, const void *primsym, int primlen,
    const void *secsym, int seclen)
{
	uint8_t ebuf[256];
	int i;

	/* load primary code and run it */
	wi_symbol_set_hcr(sc, WI_HCR_EEHOLD);
	if (wi_symbol_write_firm(sc, primsym, primlen, NULL, 0))
		return EIO;
	wi_symbol_set_hcr(sc, WI_HCR_RUN);
	for (i = 0; ; i++) {
		if (i == 10)
			return ETIMEDOUT;
		tsleep(sc, PWAIT, "wiinit", 1);
		if (CSR_READ_2(sc, WI_CNTL) == WI_CNTL_AUX_ENA_STAT)
			break;
		/* write the magic key value to unlock aux port */
		CSR_WRITE_2(sc, WI_PARAM0, WI_AUX_KEY0);
		CSR_WRITE_2(sc, WI_PARAM1, WI_AUX_KEY1);
		CSR_WRITE_2(sc, WI_PARAM2, WI_AUX_KEY2);
		CSR_WRITE_2(sc, WI_CNTL, WI_CNTL_AUX_ENA_CNTL);
	}

	/* issue read EEPROM command: XXX copied from wi_cmd() */
	CSR_WRITE_2(sc, WI_PARAM0, 0);
	CSR_WRITE_2(sc, WI_PARAM1, 0);
	CSR_WRITE_2(sc, WI_PARAM2, 0);
	CSR_WRITE_2(sc, WI_COMMAND, WI_CMD_READEE);
        for (i = 0; i < WI_TIMEOUT; i++) {
                if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_CMD)
                        break;
                DELAY(1);
        }
        CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_CMD);

	CSR_WRITE_2(sc, WI_AUX_PAGE, WI_SBCF_PDIADDR / WI_AUX_PGSZ);
	CSR_WRITE_2(sc, WI_AUX_OFFSET, WI_SBCF_PDIADDR % WI_AUX_PGSZ);
	CSR_READ_MULTI_STREAM_2(sc, WI_AUX_DATA,
	    (uint16_t *)ebuf, sizeof(ebuf) / 2);
	if (GETLE16(ebuf) > sizeof(ebuf))
		return EIO;
	if (wi_symbol_write_firm(sc, secsym, seclen, ebuf + 4, GETLE16(ebuf)))
		return EIO;
	return 0;
}

static int
wi_symbol_write_firm(struct wi_softc *sc, const void *buf, int buflen,
    const void *ebuf, int ebuflen)
{
	const uint8_t *p, *ep, *q, *eq;
	char *tp;
	uint32_t addr, id, eid;
	int i, len, elen, nblk, pdrlen;

	/*
	 * Parse the header of the firmware image.
	 */
	p = buf;
	ep = p + buflen;
	while (p < ep && *p++ != ' ');	/* FILE: */
	while (p < ep && *p++ != ' ');	/* filename */
	while (p < ep && *p++ != ' ');	/* type of the firmware */
	nblk = strtoul(p, &tp, 10);
	p = tp;
	pdrlen = strtoul(p + 1, &tp, 10);
	p = tp;
	while (p < ep && *p++ != 0x1a);	/* skip rest of header */

	/*
	 * Block records: address[4], length[2], data[length];
	 */
	for (i = 0; i < nblk; i++) {
		addr = GETLE32(p);	p += 4;
		len  = GETLE16(p);	p += 2;
		CSR_WRITE_2(sc, WI_AUX_PAGE, addr / WI_AUX_PGSZ);
		CSR_WRITE_2(sc, WI_AUX_OFFSET, addr % WI_AUX_PGSZ);
		CSR_WRITE_MULTI_STREAM_2(sc, WI_AUX_DATA,
		    (const uint16_t *)p, len / 2);
		p += len;
	}
	
	/*
	 * PDR: id[4], address[4], length[4];
	 */
	for (i = 0; i < pdrlen; ) {
		id   = GETLE32(p);	p += 4; i += 4;
		addr = GETLE32(p);	p += 4; i += 4;
		len  = GETLE32(p);	p += 4; i += 4;
		/* replace PDR entry with the values from EEPROM, if any */
		for (q = ebuf, eq = q + ebuflen; q < eq; q += elen * 2) {
			elen = GETLE16(q);	q += 2;
			eid  = GETLE16(q);	q += 2;
			elen--;		/* elen includes eid */
			if (eid == 0)
				break;
			if (eid != id)
				continue;
			CSR_WRITE_2(sc, WI_AUX_PAGE, addr / WI_AUX_PGSZ);
			CSR_WRITE_2(sc, WI_AUX_OFFSET, addr % WI_AUX_PGSZ);
			CSR_WRITE_MULTI_STREAM_2(sc, WI_AUX_DATA,
			    (const uint16_t *)q, len / 2);
			break;
		}
	}
	return 0;
}

static int
wi_symbol_set_hcr(struct wi_softc *sc, int mode)
{
	uint16_t hcr;

	CSR_WRITE_2(sc, WI_COR, WI_COR_RESET);
	tsleep(sc, PWAIT, "wiinit", 1);
	hcr = CSR_READ_2(sc, WI_HCR);
	hcr = (hcr & WI_HCR_4WIRE) | (mode & ~WI_HCR_4WIRE);
	CSR_WRITE_2(sc, WI_HCR, hcr);
	tsleep(sc, PWAIT, "wiinit", 1);
	CSR_WRITE_2(sc, WI_COR, WI_COR_IOMODE);
	tsleep(sc, PWAIT, "wiinit", 1);
	return 0;
}
#endif
