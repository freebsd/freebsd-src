/*	$NetBSD: awi.c,v 1.26 2000/07/21 04:48:55 onoe Exp $	*/
/* $FreeBSD$ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Driver for AMD 802.11 firmware.
 * Uses am79c930 chip driver to talk to firmware running on the am79c930.
 *
 * More-or-less a generic ethernet-like if driver, with 802.11 gorp added.
 */

/*
 * todo:
 *	- flush tx queue on resynch.
 *	- clear oactive on "down".
 *	- rewrite copy-into-mbuf code
 *	- mgmt state machine gets stuck retransmitting assoc requests.
 *	- multicast filter.
 *	- fix device reset so it's more likely to work
 *	- show status goo through ifmedia.
 *
 * more todo:
 *	- deal with more 802.11 frames.
 *		- send reassoc request
 *		- deal with reassoc response
 *		- send/deal with disassociation
 *	- deal with "full" access points (no room for me).
 *	- power save mode
 *
 * later:
 *	- SSID preferences
 *	- need ioctls for poking at the MIBs
 *	- implement ad-hoc mode (including bss creation).
 *	- decide when to do "ad hoc" vs. infrastructure mode (IFF_LINK flags?)
 *		(focus on inf. mode since that will be needed for ietf)
 *	- deal with DH vs. FH versions of the card
 *	- deal with faster cards (2mb/s)
 *	- ?WEP goo (mmm, rc4) (it looks not particularly useful).
 *	- ifmedia revision.
 *	- common 802.11 mibish things.
 *	- common 802.11 media layer.
 */

/*
 * Driver for AMD 802.11 PCnetMobile firmware.
 * Uses am79c930 chip driver to talk to firmware running on the am79c930.
 *
 * The initial version of the driver was written by
 * Bill Sommerfeld <sommerfeld@netbsd.org>.
 * Then the driver module completely rewritten to support cards with DS phy
 * and to support adhoc mode by Atsushi Onoe <onoe@netbsd.org>
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#if defined(__FreeBSD__) && __FreeBSD_version >= 400000
#include <sys/bus.h>
#else
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#ifdef __FreeBSD__
#include <net/ethernet.h>
#else
#include <net/if_ether.h>
#endif
#include <net/if_media.h>
#include <net/if_llc.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#ifdef __NetBSD__
#include <netinet/if_inarp.h>
#else
#include <netinet/if_ether.h>
#endif
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#if defined(__FreeBSD__) && __FreeBSD_version >= 400000
#define	NBPFILTER	1
#elif defined(__FreeBSD__) && __FreeBSD_version >= 300000
#include "bpf.h"
#define	NBPFILTER	NBPF
#else
#include "bpfilter.h"
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#ifdef __NetBSD__
#include <machine/intr.h>
#endif

#ifdef __NetBSD__
#include <dev/ic/am79c930reg.h>
#include <dev/ic/am79c930var.h>
#include <dev/ic/awireg.h>
#include <dev/ic/awivar.h>
#endif
#ifdef __FreeBSD__
#include <dev/awi/am79c930reg.h>
#include <dev/awi/am79c930var.h>
#include <dev/awi/awireg.h>
#include <dev/awi/awivar.h>
#endif

static int awi_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
#ifdef IFM_IEEE80211
static int awi_media_rate2opt(struct awi_softc *sc, int rate);
static int awi_media_opt2rate(struct awi_softc *sc, int opt);
static int awi_media_change(struct ifnet *ifp);
static void awi_media_status(struct ifnet *ifp, struct ifmediareq *imr);
#endif
static void awi_watchdog(struct ifnet *ifp);
static void awi_start(struct ifnet *ifp);
static void awi_txint(struct awi_softc *sc);
static struct mbuf * awi_fix_txhdr(struct awi_softc *sc, struct mbuf *m0);
static struct mbuf * awi_fix_rxhdr(struct awi_softc *sc, struct mbuf *m0);
static void awi_input(struct awi_softc *sc, struct mbuf *m, u_int32_t rxts, u_int8_t rssi);
static void awi_rxint(struct awi_softc *sc);
static struct mbuf * awi_devget(struct awi_softc *sc, u_int32_t off, u_int16_t len);
static int awi_init_hw(struct awi_softc *sc);
static int awi_init_mibs(struct awi_softc *sc);
static int awi_init_txrx(struct awi_softc *sc);
static void awi_stop_txrx(struct awi_softc *sc);
static int awi_start_scan(struct awi_softc *sc);
static int awi_next_scan(struct awi_softc *sc);
static void awi_stop_scan(struct awi_softc *sc);
static void awi_recv_beacon(struct awi_softc *sc, struct mbuf *m0, u_int32_t rxts, u_int8_t rssi);
static int awi_set_ss(struct awi_softc *sc);
static void awi_try_sync(struct awi_softc *sc);
static void awi_sync_done(struct awi_softc *sc);
static void awi_send_deauth(struct awi_softc *sc);
static void awi_send_auth(struct awi_softc *sc, int seq);
static void awi_recv_auth(struct awi_softc *sc, struct mbuf *m0);
static void awi_send_asreq(struct awi_softc *sc, int reassoc);
static void awi_recv_asresp(struct awi_softc *sc, struct mbuf *m0);
static int awi_mib(struct awi_softc *sc, u_int8_t cmd, u_int8_t mib);
static int awi_cmd_scan(struct awi_softc *sc);
static int awi_cmd(struct awi_softc *sc, u_int8_t cmd);
static void awi_cmd_done(struct awi_softc *sc);
static int awi_next_txd(struct awi_softc *sc, int len, u_int32_t *framep, u_int32_t*ntxdp);
static int awi_lock(struct awi_softc *sc);
static void awi_unlock(struct awi_softc *sc);
static int awi_intr_lock(struct awi_softc *sc);
static void awi_intr_unlock(struct awi_softc *sc);
static int awi_cmd_wait(struct awi_softc *sc);
static void awi_print_essid(u_int8_t *essid);

#ifdef AWI_DEBUG
static void awi_dump_pkt(struct awi_softc *sc, struct mbuf *m, int rssi);
int awi_verbose = 0;
int awi_dump = 0;
#define	AWI_DUMP_MASK(fc0)  (1 << (((fc0) & IEEE80211_FC0_SUBTYPE_MASK) >> 4))
int awi_dump_mask = AWI_DUMP_MASK(IEEE80211_FC0_SUBTYPE_BEACON);
int awi_dump_hdr = 0;
int awi_dump_len = 28;
#endif

#if NBPFILTER > 0
#define	AWI_BPF_NORM	0
#define	AWI_BPF_RAW	1
#ifdef __FreeBSD__
#define	AWI_BPF_MTAP(sc, m, raw) do {					\
	if ((sc)->sc_rawbpf == (raw))					\
		BPF_MTAP((sc)->sc_ifp, (m));				\
} while (0);
#else
#define	AWI_BPF_MTAP(sc, m, raw) do {					\
	if ((sc)->sc_ifp->if_bpf && (sc)->sc_rawbpf == (raw))		\
		bpf_mtap((sc)->sc_ifp->if_bpf, (m));			\
} while (0);
#endif
#else
#define	AWI_BPF_MTAP(sc, m, raw)
#endif

#ifndef llc_snap
#define llc_snap              llc_un.type_snap
#endif

#ifdef __FreeBSD__
#if __FreeBSD_version >= 400000
devclass_t awi_devclass;
#endif

#if __FreeBSD_version < 500043
/* NetBSD compatible functions  */
static char * ether_sprintf(u_int8_t *);

static char *
ether_sprintf(enaddr)
	u_int8_t *enaddr;
{
	static char strbuf[18];

	sprintf(strbuf, "%6D", enaddr, ":");
	return strbuf;
}
#endif
#endif

int
awi_attach(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	int s;
	int error;
#ifdef IFM_IEEE80211
	int i;
	u_int8_t *phy_rates;
	int mword;
	struct ifmediareq imr;
#endif

	s = splnet();
	/*
	 * Even if we can sleep in initialization state,
	 * all other processes (e.g. ifconfig) have to wait for
	 * completion of attaching interface.
	 */
	sc->sc_busy = 1;
	sc->sc_status = AWI_ST_INIT;
	TAILQ_INIT(&sc->sc_scan);
	error = awi_init_hw(sc);
	if (error) {
		sc->sc_invalid = 1;
		splx(s);
		return error;
	}
	error = awi_init_mibs(sc);
	splx(s);
	if (error) {
		sc->sc_invalid = 1;
		return error;
	}

	ifp->if_softc = sc;
	ifp->if_start = awi_start;
	ifp->if_ioctl = awi_ioctl;
	ifp->if_watchdog = awi_watchdog;
	ifp->if_mtu = ETHERMTU;
	ifp->if_hdrlen = sizeof(struct ieee80211_frame) +
	    sizeof(struct ether_header);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
#ifdef IFF_NOTRAILERS
	ifp->if_flags |= IFF_NOTRAILERS;
#endif
#ifdef __NetBSD__
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
#endif
#ifdef __FreeBSD__
	ifp->if_output = ether_output;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	memcpy(sc->sc_ec.ac_enaddr, sc->sc_mib_addr.aMAC_Address,
	    ETHER_ADDR_LEN);
#endif

	printf("%s: IEEE802.11 %s %dMbps (firmware %s)\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH ? "FH" : "DS",
	    sc->sc_tx_rate / 10, sc->sc_banner);
	printf("%s: address %s\n",
	    sc->sc_dev.dv_xname,  ether_sprintf(sc->sc_mib_addr.aMAC_Address));
#ifdef __FreeBSD__
	ether_ifattach(ifp, sc->sc_mib_addr.aMAC_Address);
#else
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_mib_addr.aMAC_Address);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
#endif

#ifdef IFM_IEEE80211
	ifmedia_init(&sc->sc_media, 0, awi_media_change, awi_media_status);
	phy_rates = sc->sc_mib_phy.aSuprt_Data_Rates;
	for (i = 0; i < phy_rates[1]; i++) {
		mword = awi_media_rate2opt(sc, AWI_80211_RATE(phy_rates[2 + i]));
		if (mword == 0)
			continue;
		mword |= IFM_IEEE80211;
		ifmedia_add(&sc->sc_media, mword, 0, NULL);
		ifmedia_add(&sc->sc_media,
		    mword | IFM_IEEE80211_ADHOC, 0, NULL);
		if (sc->sc_mib_phy.IEEE_PHY_Type != AWI_PHY_TYPE_FH)
			ifmedia_add(&sc->sc_media,
			    mword | IFM_IEEE80211_ADHOC | IFM_FLAG0, 0, NULL);
	}
	awi_media_status(ifp, &imr);
	ifmedia_set(&sc->sc_media, imr.ifm_active);
#endif

	/* ready to accept ioctl */
	awi_unlock(sc);

	/* Attach is successful. */
	sc->sc_attached = 1;
	return 0;
}

#ifdef __NetBSD__
int
awi_detach(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	int s;

	/* Succeed if there is no work to do. */
	if (!sc->sc_attached)
		return (0);

	s = splnet();
	sc->sc_invalid = 1;
	awi_stop(sc);
	while (sc->sc_sleep_cnt > 0) {
		wakeup(sc);
		(void)tsleep(sc, PWAIT, "awidet", 1);
	}
	if (sc->sc_wep_ctx != NULL)
		free(sc->sc_wep_ctx, M_DEVBUF);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
#ifdef IFM_IEEE80211
	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
#endif
	ether_ifdetach(ifp);
	if_detach(ifp);
	if (sc->sc_enabled) {
		if (sc->sc_disable)
			(*sc->sc_disable)(sc);
		sc->sc_enabled = 0;
	}
	splx(s);
	return 0;
}

int
awi_activate(self, act)
	struct device *self;
	enum devact act;
{
	struct awi_softc *sc = (struct awi_softc *)self;
	int s, error = 0;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		error = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		sc->sc_invalid = 1;
		if (sc->sc_ifp)
			if_deactivate(sc->sc_ifp);
		break;
	}
	splx(s);

	return error;
}

void
awi_power(sc, why)
	struct awi_softc *sc;
	int why;
{
	int s;
	int ocansleep;

	if (!sc->sc_enabled)
		return;

	s = splnet();
	ocansleep = sc->sc_cansleep;
	sc->sc_cansleep = 0;
#ifdef needtobefixed	/*ONOE*/
	if (why == PWR_RESUME) {
		sc->sc_enabled = 0;
		awi_init(sc);
		(void)awi_intr(sc);
	} else {
		awi_stop(sc);
		if (sc->sc_disable)
			(*sc->sc_disable)(sc);
	}
#endif
	sc->sc_cansleep = ocansleep;
	splx(s);
}
#endif /* __NetBSD__ */

static int
awi_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct awi_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ieee80211req *ireq = (struct ieee80211req *)data;
	int s, error;
#ifdef SIOCS80211NWID
	struct ieee80211_nwid nwid;
#endif
	u_int8_t *p;
	int len;
	u_int8_t tmpstr[IEEE80211_NWID_LEN*2];
#ifdef __FreeBSD_version
#if __FreeBSD_version < 500028
	struct proc *mythread = curproc;		/* name a white lie */
#else
	struct thread *mythread = curthread;
#endif
#endif

	s = splnet();

	/* serialize ioctl */
	error = awi_lock(sc);
	if (error)
		goto cantlock;
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit((void *)ifp, ifa);
			break;
#endif
		}
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		sc->sc_format_llc = !(ifp->if_flags & IFF_LINK0);
		if (!(ifp->if_flags & IFF_UP)) {
			if (sc->sc_enabled) {
				awi_stop(sc);
				if (sc->sc_disable)
					(*sc->sc_disable)(sc);
				sc->sc_enabled = 0;
			}
			break;
		}
		error = awi_init(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
#ifdef __FreeBSD__
		error = ENETRESET;	/*XXX*/
#else
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ec) :
		    ether_delmulti(ifr, &sc->sc_ec);
#endif
		/*
		 * Do not rescan BSS.  Rather, just reset multicast filter.
		 */
		if (error == ENETRESET) {
			if (sc->sc_enabled)
				error = awi_init(sc);
			else
				error = 0;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
#ifdef SIOCS80211NWID
	case SIOCS80211NWID:
#ifdef __FreeBSD__
		error = suser(mythread);
		if (error)
			break;
#endif
		error = copyin(ifr->ifr_data, &nwid, sizeof(nwid));
		if (error)
			break;
		if (nwid.i_len > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		if (sc->sc_mib_mac.aDesired_ESS_ID[1] == nwid.i_len &&
		    memcmp(&sc->sc_mib_mac.aDesired_ESS_ID[2], nwid.i_nwid,
		    nwid.i_len) == 0)
			break;
		memset(sc->sc_mib_mac.aDesired_ESS_ID, 0, AWI_ESS_ID_SIZE);
		sc->sc_mib_mac.aDesired_ESS_ID[0] = IEEE80211_ELEMID_SSID;
		sc->sc_mib_mac.aDesired_ESS_ID[1] = nwid.i_len;
		memcpy(&sc->sc_mib_mac.aDesired_ESS_ID[2], nwid.i_nwid,
		    nwid.i_len);
		if (sc->sc_enabled) {
			awi_stop(sc);
			error = awi_init(sc);
		}
		break;
	case SIOCG80211NWID:
		if (ifp->if_flags & IFF_RUNNING)
			p = sc->sc_bss.essid;
		else
			p = sc->sc_mib_mac.aDesired_ESS_ID;
		error = copyout(p + 1, ifr->ifr_data, 1 + IEEE80211_NWID_LEN);
		break;
#endif
#ifdef SIOCS80211NWKEY
	case SIOCS80211NWKEY:
#ifdef __FreeBSD__
		error = suser(mythread);
		if (error)
			break;
#endif
		error = awi_wep_setnwkey(sc, (struct ieee80211_nwkey *)data);
		break;
	case SIOCG80211NWKEY:
		error = awi_wep_getnwkey(sc, (struct ieee80211_nwkey *)data);
		break;
#endif
#ifdef IFM_IEEE80211
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
#endif
#ifdef __FreeBSD__
	case SIOCG80211:
		switch(ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_val != -1 && ireq->i_val != 0) {
				error = EINVAL;
				break;
			}
			if (!sc->sc_mib_local.Network_Mode)
				p = sc->sc_ownssid;
			else if (ireq->i_val == -1 &&
			    (ifp->if_flags & IFF_RUNNING))
				p = sc->sc_bss.essid;
			else
				p = sc->sc_mib_mac.aDesired_ESS_ID;
			len = p[1];
			p += 2;
			if (len > IEEE80211_NWID_LEN) {
				error = EINVAL;
				break;
			}
			if (len > 0)
				error = copyout(p, ireq->i_data, len);
			ireq->i_len = len;
			break;
		case IEEE80211_IOC_NUMSSIDS:
			ireq->i_val = 1;
			break;
		case IEEE80211_IOC_WEP:
			if (sc->sc_wep_algo != NULL)
				ireq->i_val = IEEE80211_WEP_MIXED;
			else
				ireq->i_val = IEEE80211_WEP_OFF;
			break;
		case IEEE80211_IOC_WEPKEY:
			if(ireq->i_val < 0 || ireq->i_val > 3) {
				error = EINVAL;
				break;
			}
			len = sizeof(tmpstr);
			error = awi_wep_getkey(sc, ireq->i_val, tmpstr, &len);
			if(error)
				break;
#ifdef __FreeBSD__
			if (!suser(mythread))
				bzero(tmpstr, len);
#endif
			ireq->i_len = len;
			error = copyout(tmpstr, ireq->i_data, len);
			break;
		case IEEE80211_IOC_NUMWEPKEYS:
			ireq->i_val = 4;
			break;
		case IEEE80211_IOC_WEPTXKEY:
			ireq->i_val = sc->sc_wep_defkid;
			break;
		case IEEE80211_IOC_AUTHMODE:
			ireq->i_val = IEEE80211_AUTH_OPEN;
			break;
		case IEEE80211_IOC_STATIONNAME:
			/* not used anywhere */
			error = EINVAL;
			break;
		case IEEE80211_IOC_CHANNEL:
			/* XXX: Handle FH cards */
			ireq->i_val = sc->sc_bss.chanset;
			break;
		case IEEE80211_IOC_POWERSAVE:
			/*
			 * The powersave mode is not supported by the driver.
			 */
			ireq->i_val = IEEE80211_POWERSAVE_NOSUP;
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			error = EINVAL;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case SIOCS80211:
		error = suser(mythread);
		if(error)
			break;
		switch(ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_val != 0 ||
			    ireq->i_len > IEEE80211_NWID_LEN) {
				error = EINVAL;
				break;
			}
			bzero(tmpstr, AWI_ESS_ID_SIZE);
			tmpstr[0] = IEEE80211_ELEMID_SSID;
			tmpstr[1] = ireq->i_len;
			error = copyin(ireq->i_data, tmpstr+2, ireq->i_len);
			if(error)
				break;
			bcopy(tmpstr, sc->sc_mib_mac.aDesired_ESS_ID, 
				AWI_ESS_ID_SIZE);
			bcopy(tmpstr, sc->sc_ownssid, AWI_ESS_ID_SIZE);
			break;
		case IEEE80211_IOC_WEP:
			if(ireq->i_val == IEEE80211_WEP_OFF)
				error = awi_wep_setalgo(sc, 0);
			else
				error = awi_wep_setalgo(sc, 1);
			break;
		case IEEE80211_IOC_WEPKEY:
			if(ireq->i_val < 0 || ireq->i_val > 3 ||
			    ireq->i_len > 13) {
				error = EINVAL;
				break;
			}
			error = copyin(ireq->i_data, tmpstr, ireq->i_len);
			if(error)
				break;
			error = awi_wep_setkey(sc, ireq->i_val, tmpstr,
			    ireq->i_len);
			break;
		case IEEE80211_IOC_WEPTXKEY:
			if(ireq->i_val < 0 || ireq->i_val > 3) {
				error = EINVAL;
				break;
			}
			sc->sc_wep_defkid = ireq->i_val;
			break;
		case IEEE80211_IOC_AUTHMODE:
			if(ireq->i_val != IEEE80211_AUTH_OPEN)
				error = EINVAL;
			break;
		case IEEE80211_IOC_STATIONNAME:
			error = EPERM;
			break;
		case IEEE80211_IOC_CHANNEL:
			if(ireq->i_val < sc->sc_scan_min ||
			    ireq->i_val > sc->sc_scan_max) {
				error = EINVAL;
				break;
			}
			sc->sc_ownch = ireq->i_val;
			break;
		case IEEE80211_IOC_POWERSAVE:
			if(ireq->i_val != IEEE80211_POWERSAVE_OFF)
				error = EINVAL;
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			error = EINVAL;
			break;
		default:
			error = EINVAL;
			break;
		}
		/* Restart the card so the change takes effect */
		if(!error) {
			if(sc->sc_enabled) {
				awi_stop(sc);
				error = awi_init(sc);
			}
		}
		break;
#endif /* __FreeBSD__ */
	default:
		error = awi_wicfg(ifp, cmd, data);
		break;
	}
	awi_unlock(sc);
  cantlock:
	splx(s);
	return error;
}

#ifdef IFM_IEEE80211
static int
awi_media_rate2opt(sc, rate)
	struct awi_softc *sc;
	int rate;
{
	int mword;

	mword = 0;
	switch (rate) {
	case 10:
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
			mword = IFM_IEEE80211_FH1;
		else
			mword = IFM_IEEE80211_DS1;
		break;
	case 20:
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
			mword = IFM_IEEE80211_FH2;
		else
			mword = IFM_IEEE80211_DS2;
		break;
	case 55:
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_DS)
			mword = IFM_IEEE80211_DS5;
		break;
	case 110:
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_DS)
			mword = IFM_IEEE80211_DS11;
		break;
	}
	return mword;
}

static int
awi_media_opt2rate(sc, opt)
	struct awi_softc *sc;
	int opt;
{
	int rate;

	rate = 0;
	switch (IFM_SUBTYPE(opt)) {
	case IFM_IEEE80211_FH1:
	case IFM_IEEE80211_FH2:
		if (sc->sc_mib_phy.IEEE_PHY_Type != AWI_PHY_TYPE_FH)
			return 0;
		break;
	case IFM_IEEE80211_DS1:
	case IFM_IEEE80211_DS2:
	case IFM_IEEE80211_DS5:
	case IFM_IEEE80211_DS11:
		if (sc->sc_mib_phy.IEEE_PHY_Type != AWI_PHY_TYPE_DS)
			return 0;
		break;
	}

	switch (IFM_SUBTYPE(opt)) {
	case IFM_IEEE80211_FH1:
	case IFM_IEEE80211_DS1:
		rate = 10;
		break;
	case IFM_IEEE80211_FH2:
	case IFM_IEEE80211_DS2:
		rate = 20;
		break;
	case IFM_IEEE80211_DS5:
		rate = 55;
		break;
	case IFM_IEEE80211_DS11:
		rate = 110;
		break;
	}
	return rate;
}

/*
 * Called from ifmedia_ioctl via awi_ioctl with lock obtained.
 */
static int
awi_media_change(ifp)
	struct ifnet *ifp;
{
	struct awi_softc *sc = ifp->if_softc;
	struct ifmedia_entry *ime;
	u_int8_t *phy_rates;
	int i, rate, error;

	error = 0;
	ime = sc->sc_media.ifm_cur;
	rate = awi_media_opt2rate(sc, ime->ifm_media);
	if (rate == 0)
		return EINVAL;
	if (rate != sc->sc_tx_rate) {
		phy_rates = sc->sc_mib_phy.aSuprt_Data_Rates;
		for (i = 0; i < phy_rates[1]; i++) {
			if (rate == AWI_80211_RATE(phy_rates[2 + i]))
				break;
		}
		if (i == phy_rates[1])
			return EINVAL;
	}
	if (ime->ifm_media & IFM_IEEE80211_ADHOC) {
		sc->sc_mib_local.Network_Mode = 0;
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
			sc->sc_no_bssid = 0;
		else
			sc->sc_no_bssid = (ime->ifm_media & IFM_FLAG0) ? 1 : 0;
	} else {
		sc->sc_mib_local.Network_Mode = 1;
	}
	if (sc->sc_enabled) {
		awi_stop(sc);
		error = awi_init(sc);
	}
	return error;
}

static void
awi_media_status(ifp, imr)
	struct ifnet *ifp;
	struct ifmediareq *imr;
{
	struct awi_softc *sc = ifp->if_softc;

	imr->ifm_status = IFM_AVALID;
	if (ifp->if_flags & IFF_RUNNING)
		imr->ifm_status |= IFM_ACTIVE;
	imr->ifm_active = IFM_IEEE80211;
	imr->ifm_active |= awi_media_rate2opt(sc, sc->sc_tx_rate);
	if (sc->sc_mib_local.Network_Mode == 0) {
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		if (sc->sc_no_bssid)
			imr->ifm_active |= IFM_FLAG0;
	}
}
#endif /* IFM_IEEE80211 */

int
awi_intr(arg)
	void *arg;
{
	struct awi_softc *sc = arg;
	u_int16_t status;
	int error, handled = 0, ocansleep;

	if (!sc->sc_enabled || !sc->sc_enab_intr || sc->sc_invalid)
		return 0;

	am79c930_gcr_setbits(&sc->sc_chip,
	    AM79C930_GCR_DISPWDN | AM79C930_GCR_ECINT);
	awi_write_1(sc, AWI_DIS_PWRDN, 1);
	ocansleep = sc->sc_cansleep;
	sc->sc_cansleep = 0;

	for (;;) {
		error = awi_intr_lock(sc);
		if (error)
			break;
		status = awi_read_1(sc, AWI_INTSTAT);
		awi_write_1(sc, AWI_INTSTAT, 0);
		awi_write_1(sc, AWI_INTSTAT, 0);
		status |= awi_read_1(sc, AWI_INTSTAT2) << 8;
		awi_write_1(sc, AWI_INTSTAT2, 0);
		DELAY(10);
		awi_intr_unlock(sc);
		if (!sc->sc_cmd_inprog)
			status &= ~AWI_INT_CMD;	/* make sure */
		if (status == 0)
			break;
		handled = 1;
		if (status & AWI_INT_RX)
			awi_rxint(sc);
		if (status & AWI_INT_TX)
			awi_txint(sc);
		if (status & AWI_INT_CMD)
			awi_cmd_done(sc);
		if (status & AWI_INT_SCAN_CMPLT) {
			if (sc->sc_status == AWI_ST_SCAN &&
			    sc->sc_mgt_timer > 0)
				(void)awi_next_scan(sc);
		}
	}
	sc->sc_cansleep = ocansleep;
	am79c930_gcr_clearbits(&sc->sc_chip, AM79C930_GCR_DISPWDN);
	awi_write_1(sc, AWI_DIS_PWRDN, 0);
	return handled;
}

int
awi_init(sc)
	struct awi_softc *sc;
{
	int error, ostatus;
	int n;
	struct ifnet *ifp = sc->sc_ifp;
#ifdef __FreeBSD__
	struct ifmultiaddr *ifma;
#else
	struct ether_multi *enm;
	struct ether_multistep step;
#endif

	/* reinitialize muticast filter */
	n = 0;
	ifp->if_flags |= IFF_ALLMULTI;
	sc->sc_mib_local.Accept_All_Multicast_Dis = 0;
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_mib_mac.aPromiscuous_Enable = 1;
		goto set_mib;
	}
	sc->sc_mib_mac.aPromiscuous_Enable = 0;
#ifdef __FreeBSD__
	if (ifp->if_amcount != 0)
		goto set_mib;
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (n == AWI_GROUP_ADDR_SIZE)
			goto set_mib;
		memcpy(sc->sc_mib_addr.aGroup_Addresses[n],
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    ETHER_ADDR_LEN);
		n++;
	}
#else
	ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
	while (enm != NULL) {
		if (n == AWI_GROUP_ADDR_SIZE ||
		    memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)
		    != 0)
			goto set_mib;
		memcpy(sc->sc_mib_addr.aGroup_Addresses[n], enm->enm_addrlo,
		    ETHER_ADDR_LEN);
		n++;
		ETHER_NEXT_MULTI(step, enm);
	}
#endif
	for (; n < AWI_GROUP_ADDR_SIZE; n++)
		memset(sc->sc_mib_addr.aGroup_Addresses[n], 0, ETHER_ADDR_LEN);
	ifp->if_flags &= ~IFF_ALLMULTI;
	sc->sc_mib_local.Accept_All_Multicast_Dis = 1;

  set_mib:
#ifdef notdef	/* allow non-encrypted frame for receiving. */
	sc->sc_mib_mgt.Wep_Required = sc->sc_wep_algo != NULL ? 1 : 0;
#endif
	if (!sc->sc_enabled) {
		sc->sc_enabled = 1;
		if (sc->sc_enable)
			(*sc->sc_enable)(sc);
		sc->sc_status = AWI_ST_INIT;
		error = awi_init_hw(sc);
		if (error)
			return error;
	}
	ostatus = sc->sc_status;
	sc->sc_status = AWI_ST_INIT;
	if ((error = awi_mib(sc, AWI_CMD_SET_MIB, AWI_MIB_LOCAL)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_SET_MIB, AWI_MIB_ADDR)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_SET_MIB, AWI_MIB_MAC)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_SET_MIB, AWI_MIB_MGT)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_SET_MIB, AWI_MIB_PHY)) != 0) {
		awi_stop(sc);
		return error;
	}
	if (ifp->if_flags & IFF_RUNNING)
		sc->sc_status = AWI_ST_RUNNING;
	else {
		if (ostatus == AWI_ST_INIT) {
			error = awi_init_txrx(sc);
			if (error)
				return error;
		}
		error = awi_start_scan(sc);
	}
	return error;
}

void
awi_stop(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct awi_bss *bp;
	struct mbuf *m;

	sc->sc_status = AWI_ST_INIT;
	if (!sc->sc_invalid) {
		(void)awi_cmd_wait(sc);
		if (sc->sc_mib_local.Network_Mode &&
		    sc->sc_status > AWI_ST_AUTH)
			awi_send_deauth(sc);
		awi_stop_txrx(sc);
	}
	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
	ifp->if_timer = 0;
	sc->sc_tx_timer = sc->sc_rx_timer = sc->sc_mgt_timer = 0;
	for (;;) {
		_IF_DEQUEUE(&sc->sc_mgtq, m);
		if (m == NULL)
			break;
		m_freem(m);
	}
	IF_DRAIN(&ifp->if_snd);
	while ((bp = TAILQ_FIRST(&sc->sc_scan)) != NULL) {
		TAILQ_REMOVE(&sc->sc_scan, bp, list);
		free(bp, M_DEVBUF);
	}
}

static void
awi_watchdog(ifp)
	struct ifnet *ifp;
{
	struct awi_softc *sc = ifp->if_softc;
	int ocansleep;

	if (sc->sc_invalid) {
		ifp->if_timer = 0;
		return;
	}

	ocansleep = sc->sc_cansleep;
	sc->sc_cansleep = 0;
	if (sc->sc_tx_timer && --sc->sc_tx_timer == 0) {
		printf("%s: transmit timeout\n", sc->sc_dev.dv_xname);
		awi_txint(sc);
	}
	if (sc->sc_rx_timer && --sc->sc_rx_timer == 0) {
		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: no recent beacons from %s; rescanning\n",
			    sc->sc_dev.dv_xname,
			    ether_sprintf(sc->sc_bss.bssid));
		}
		ifp->if_flags &= ~IFF_RUNNING;
		awi_start_scan(sc);
	}
	if (sc->sc_mgt_timer && --sc->sc_mgt_timer == 0) {
		switch (sc->sc_status) {
		case AWI_ST_SCAN:
			awi_stop_scan(sc);
			break;
		case AWI_ST_AUTH:
		case AWI_ST_ASSOC:
			/* restart scan */
			awi_start_scan(sc);
			break;
		default:
			break;
		}
	}

	if (sc->sc_tx_timer == 0 && sc->sc_rx_timer == 0 &&
	    sc->sc_mgt_timer == 0)
		ifp->if_timer = 0;
	else
		ifp->if_timer = 1;
	sc->sc_cansleep = ocansleep;
}

static void
awi_start(ifp)
	struct ifnet *ifp;
{
	struct awi_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	u_int32_t txd, frame, ntxd;
	u_int8_t rate;
	int len, sent = 0;

	for (;;) {
		txd = sc->sc_txnext;
		_IF_DEQUEUE(&sc->sc_mgtq, m0);
		if (m0 != NULL) {
			if (awi_next_txd(sc, m0->m_pkthdr.len, &frame, &ntxd)) {
				_IF_PREPEND(&sc->sc_mgtq, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
		} else {
			if (!(ifp->if_flags & IFF_RUNNING))
				break;
			IF_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			len = m0->m_pkthdr.len + sizeof(struct ieee80211_frame);
			if (sc->sc_format_llc)
				len += sizeof(struct llc) -
				    sizeof(struct ether_header);
			if (sc->sc_wep_algo != NULL)
				len += IEEE80211_WEP_IVLEN +
				    IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN;
			if (awi_next_txd(sc, len, &frame, &ntxd)) {
				IF_PREPEND(&ifp->if_snd, m0);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			AWI_BPF_MTAP(sc, m0, AWI_BPF_NORM);
			m0 = awi_fix_txhdr(sc, m0);
			if (sc->sc_wep_algo != NULL && m0 != NULL)
				m0 = awi_wep_encrypt(sc, m0, 1);
			if (m0 == NULL) {
				ifp->if_oerrors++;
				continue;
			}
			ifp->if_opackets++;
		}
#ifdef AWI_DEBUG
		if (awi_dump)
			awi_dump_pkt(sc, m0, -1);
#endif
		AWI_BPF_MTAP(sc, m0, AWI_BPF_RAW);
		len = 0;
		for (m = m0; m != NULL; m = m->m_next) {
			awi_write_bytes(sc, frame + len, mtod(m, u_int8_t *),
			    m->m_len);
			len += m->m_len;
		}
		m_freem(m0);
		rate = sc->sc_tx_rate;	/*XXX*/
		awi_write_1(sc, ntxd + AWI_TXD_STATE, 0);
		awi_write_4(sc, txd + AWI_TXD_START, frame);
		awi_write_4(sc, txd + AWI_TXD_NEXT, ntxd);
		awi_write_4(sc, txd + AWI_TXD_LENGTH, len);
		awi_write_1(sc, txd + AWI_TXD_RATE, rate);
		awi_write_4(sc, txd + AWI_TXD_NDA, 0);
		awi_write_4(sc, txd + AWI_TXD_NRA, 0);
		awi_write_1(sc, txd + AWI_TXD_STATE, AWI_TXD_ST_OWN);
		sc->sc_txnext = ntxd;
		sent++;
	}
	if (sent) {
		if (sc->sc_tx_timer == 0)
			sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
#ifdef AWI_DEBUG
		if (awi_verbose)
			printf("awi_start: sent %d txdone %d txnext %d txbase %d txend %d\n", sent, sc->sc_txdone, sc->sc_txnext, sc->sc_txbase, sc->sc_txend);
#endif
	}
}

static void
awi_txint(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	u_int8_t flags;

	while (sc->sc_txdone != sc->sc_txnext) {
		flags = awi_read_1(sc, sc->sc_txdone + AWI_TXD_STATE);
		if ((flags & AWI_TXD_ST_OWN) || !(flags & AWI_TXD_ST_DONE))
			break;
		if (flags & AWI_TXD_ST_ERROR)
			ifp->if_oerrors++;
		sc->sc_txdone = awi_read_4(sc, sc->sc_txdone + AWI_TXD_NEXT) &
		    0x7fff;
	}
	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
#ifdef AWI_DEBUG
	if (awi_verbose)
		printf("awi_txint: txdone %d txnext %d txbase %d txend %d\n",
		    sc->sc_txdone, sc->sc_txnext, sc->sc_txbase, sc->sc_txend);
#endif
	awi_start(ifp);
}

static struct mbuf *
awi_fix_txhdr(sc, m0)
	struct awi_softc *sc;
	struct mbuf *m0;
{
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct llc *llc;

	if (m0->m_len < sizeof(eh)) {
		m0 = m_pullup(m0, sizeof(eh));
		if (m0 == NULL)
			return NULL;
	}
	memcpy(&eh, mtod(m0, caddr_t), sizeof(eh));
	if (sc->sc_format_llc) {
		m_adj(m0, sizeof(struct ether_header) - sizeof(struct llc));
		llc = mtod(m0, struct llc *);
		llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
		llc->llc_control = LLC_UI;
		llc->llc_snap.org_code[0] = llc->llc_snap.org_code[1] = 
		    llc->llc_snap.org_code[2] = 0;
		llc->llc_snap.ether_type = eh.ether_type;
	}
	M_PREPEND(m0, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m0 == NULL)
		return NULL;
	wh = mtod(m0, struct ieee80211_frame *);

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	LE_WRITE_2(wh->i_dur, 0);
	LE_WRITE_2(wh->i_seq, 0);
	if (sc->sc_mib_local.Network_Mode) {
		wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
		memcpy(wh->i_addr1, sc->sc_bss.bssid, ETHER_ADDR_LEN);
		memcpy(wh->i_addr2, eh.ether_shost, ETHER_ADDR_LEN);
		memcpy(wh->i_addr3, eh.ether_dhost, ETHER_ADDR_LEN);
	} else {
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		memcpy(wh->i_addr1, eh.ether_dhost, ETHER_ADDR_LEN);
		memcpy(wh->i_addr2, eh.ether_shost, ETHER_ADDR_LEN);
		memcpy(wh->i_addr3, sc->sc_bss.bssid, ETHER_ADDR_LEN);
	}
	return m0;
}

static struct mbuf *
awi_fix_rxhdr(sc, m0)
	struct awi_softc *sc;
	struct mbuf *m0;
{
	struct ieee80211_frame wh;
	struct ether_header *eh;
	struct llc *llc;

	if (m0->m_len < sizeof(wh)) {
		m_freem(m0);
		return NULL;
	}
	llc = (struct llc *)(mtod(m0, caddr_t) + sizeof(wh));
	if (llc->llc_dsap == LLC_SNAP_LSAP &&
	    llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI &&
	    llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 &&
	    llc->llc_snap.org_code[2] == 0) {
		memcpy(&wh, mtod(m0, caddr_t), sizeof(wh));
		m_adj(m0, sizeof(wh) + sizeof(*llc) - sizeof(*eh));
		eh = mtod(m0, struct ether_header *);
		switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
		case IEEE80211_FC1_DIR_NODS:
			memcpy(eh->ether_dhost, wh.i_addr1, ETHER_ADDR_LEN);
			memcpy(eh->ether_shost, wh.i_addr2, ETHER_ADDR_LEN);
			break;
		case IEEE80211_FC1_DIR_TODS:
			memcpy(eh->ether_dhost, wh.i_addr3, ETHER_ADDR_LEN);
			memcpy(eh->ether_shost, wh.i_addr2, ETHER_ADDR_LEN);
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			memcpy(eh->ether_dhost, wh.i_addr1, ETHER_ADDR_LEN);
			memcpy(eh->ether_shost, wh.i_addr3, ETHER_ADDR_LEN);
			break;
		case IEEE80211_FC1_DIR_DSTODS:
			m_freem(m0);
			return NULL;
		}
	} else {
		/* assuming ethernet encapsulation, just strip 802.11 header */
		m_adj(m0, sizeof(wh));
	}
	if (ALIGN(mtod(m0, caddr_t) + sizeof(struct ether_header)) !=
	    (uintptr_t)(mtod(m0, caddr_t) + sizeof(struct ether_header))) {
		/* XXX: we loose to estimate the type of encapsulation */
		struct mbuf *n, *n0, **np;
		caddr_t newdata;
		int off;

		n0 = NULL;
		np = &n0;
		off = 0;
		while (m0->m_pkthdr.len > off) {
			if (n0 == NULL) {
				MGETHDR(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m0);
					return NULL;
				}
				M_MOVE_PKTHDR(n, m0);
				n->m_len = MHLEN;
			} else {
				MGET(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m0);
					m_freem(n0);
					return NULL;
				}
				n->m_len = MLEN;
			}
			if (m0->m_pkthdr.len - off >= MINCLSIZE) {
				MCLGET(n, M_DONTWAIT);
				if (n->m_flags & M_EXT)
					n->m_len = n->m_ext.ext_size;
			}
			if (n0 == NULL) {
				newdata = (caddr_t)
				    ALIGN(n->m_data
				    + sizeof(struct ether_header))
				    - sizeof(struct ether_header);
				n->m_len -= newdata - n->m_data;
				n->m_data = newdata;
			}
			if (n->m_len > m0->m_pkthdr.len - off)
				n->m_len = m0->m_pkthdr.len - off;
			m_copydata(m0, off, n->m_len, mtod(n, caddr_t));
			off += n->m_len;
			*np = n;
			np = &n->m_next;
		}
		m_freem(m0);
		m0 = n0;
	}
	return m0;
}

static void
awi_input(sc, m, rxts, rssi)
	struct awi_softc *sc;
	struct mbuf *m;
	u_int32_t rxts;
	u_int8_t rssi;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211_frame *wh;

	/* trim CRC here for WEP can find its own CRC at the end of packet. */
	m_adj(m, -ETHER_CRC_LEN);
	AWI_BPF_MTAP(sc, m, AWI_BPF_RAW);
	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		printf("%s; receive packet with wrong version: %x\n",
		    sc->sc_dev.dv_xname, wh->i_fc[0]);
		m_freem(m);
		ifp->if_ierrors++;
		return;
	}
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		m = awi_wep_encrypt(sc, m, 0);
		if (m == NULL) {
			ifp->if_ierrors++;
			return;
		}
		wh = mtod(m, struct ieee80211_frame *);
	}
#ifdef AWI_DEBUG
	if (awi_dump)
		awi_dump_pkt(sc, m, rssi);
#endif

	if ((sc->sc_mib_local.Network_Mode || !sc->sc_no_bssid) &&
	    sc->sc_status == AWI_ST_RUNNING) {
		if (memcmp(wh->i_addr2, sc->sc_bss.bssid, ETHER_ADDR_LEN) == 0) {
			sc->sc_rx_timer = 10;
			sc->sc_bss.rssi = rssi;
		}
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		if (sc->sc_mib_local.Network_Mode) {
			if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=
			    IEEE80211_FC1_DIR_FROMDS) {
				m_freem(m);
				return;
			}
		} else {
			if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=
			    IEEE80211_FC1_DIR_NODS) {
				m_freem(m);
				return;
			}
		}
		m = awi_fix_rxhdr(sc, m);
		if (m == NULL) {
			ifp->if_ierrors++;
			break;
		}
		ifp->if_ipackets++;
#if !(defined(__FreeBSD__) && __FreeBSD_version >= 400000)
		AWI_BPF_MTAP(sc, m, AWI_BPF_NORM);
#endif
		(*ifp->if_input)(ifp, m);
		break;
	case IEEE80211_FC0_TYPE_MGT:
		if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) !=
		   IEEE80211_FC1_DIR_NODS) {
			m_freem(m);
			return;
		}
		switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		case IEEE80211_FC0_SUBTYPE_BEACON:
			awi_recv_beacon(sc, m, rxts, rssi);
			break;
		case IEEE80211_FC0_SUBTYPE_AUTH:
			awi_recv_auth(sc, m);
			break;
		case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
		case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
			awi_recv_asresp(sc, m);
			break;
		case IEEE80211_FC0_SUBTYPE_DEAUTH:
			if (sc->sc_mib_local.Network_Mode)
				awi_send_auth(sc, 1);
			break;
		case IEEE80211_FC0_SUBTYPE_DISASSOC:
			if (sc->sc_mib_local.Network_Mode)
				awi_send_asreq(sc, 1);
			break;
		}
		m_freem(m);
		break;
	case IEEE80211_FC0_TYPE_CTL:
	default:
		/* should not come here */
		m_freem(m);
		break;
	}
}

static void
awi_rxint(sc)
	struct awi_softc *sc;
{
	u_int8_t state, rate, rssi;
	u_int16_t len;
	u_int32_t frame, next, rxts, rxoff;
	struct mbuf *m;

	rxoff = sc->sc_rxdoff;
	for (;;) {
		state = awi_read_1(sc, rxoff + AWI_RXD_HOST_DESC_STATE);
		if (state & AWI_RXD_ST_OWN)
			break;
		if (!(state & AWI_RXD_ST_CONSUMED)) {
			if (state & AWI_RXD_ST_RXERROR)
				sc->sc_ifp->if_ierrors++;
			else {
				len   = awi_read_2(sc, rxoff + AWI_RXD_LEN);
				rate  = awi_read_1(sc, rxoff + AWI_RXD_RATE);
				rssi  = awi_read_1(sc, rxoff + AWI_RXD_RSSI);
				frame = awi_read_4(sc, rxoff + AWI_RXD_START_FRAME) & 0x7fff;
				rxts  = awi_read_4(sc, rxoff + AWI_RXD_LOCALTIME);
				m = awi_devget(sc, frame, len);
				if (state & AWI_RXD_ST_LF)
					awi_input(sc, m, rxts, rssi);
				else
					sc->sc_rxpend = m;
			}
			state |= AWI_RXD_ST_CONSUMED;
			awi_write_1(sc, rxoff + AWI_RXD_HOST_DESC_STATE, state);
		}
		next  = awi_read_4(sc, rxoff + AWI_RXD_NEXT);
		if (next & AWI_RXD_NEXT_LAST)
			break;
		/* make sure the next pointer is correct */
		if (next != awi_read_4(sc, rxoff + AWI_RXD_NEXT))
			break;
		state |= AWI_RXD_ST_OWN;
		awi_write_1(sc, rxoff + AWI_RXD_HOST_DESC_STATE, state);
		rxoff = next & 0x7fff;
	}
	sc->sc_rxdoff = rxoff;
}

static struct mbuf *
awi_devget(sc, off, len)
	struct awi_softc *sc;
	u_int32_t off;
	u_int16_t len;
{
	struct mbuf *m;
	struct mbuf *top, **mp;
	u_int tlen;

	top = sc->sc_rxpend;
	mp = &top;
	if (top != NULL) {
		sc->sc_rxpend = NULL;
		top->m_pkthdr.len += len;
		m = top;
		while (*mp != NULL) {
			m = *mp;
			mp = &m->m_next;
		}
		if (m->m_flags & M_EXT)
			tlen = m->m_ext.ext_size;
		else if (m->m_flags & M_PKTHDR)
			tlen = MHLEN;
		else
			tlen = MLEN;
		tlen -= m->m_len;
		if (tlen > len)
			tlen = len;
		awi_read_bytes(sc, off, mtod(m, u_int8_t *) + m->m_len, tlen);
		off += tlen;
		len -= tlen;
	}

	while (len > 0) {
		if (top == NULL) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				return NULL;
			m->m_pkthdr.rcvif = sc->sc_ifp;
			m->m_pkthdr.len = len;
			m->m_len = MHLEN;
		} else {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(top);
				return NULL;
			}
			m->m_len = MLEN;
		}
		if (len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = m->m_ext.ext_size;
		}
		if (top == NULL) {
			int hdrlen = sizeof(struct ieee80211_frame) +
			    (sc->sc_format_llc ? sizeof(struct llc) :
			    sizeof(struct ether_header));
			caddr_t newdata = (caddr_t)
			    ALIGN(m->m_data + hdrlen) - hdrlen;
			m->m_len -= newdata - m->m_data;
			m->m_data = newdata;
		}
		if (m->m_len > len)
			m->m_len = len;
		awi_read_bytes(sc, off, mtod(m, u_int8_t *), m->m_len);
		off += m->m_len;
		len -= m->m_len;
		*mp = m;
		mp = &m->m_next;
	}
	return top;
}

/*
 * Initialize hardware and start firmware to accept commands.
 * Called everytime after power on firmware.
 */

static int
awi_init_hw(sc)
	struct awi_softc *sc;
{
	u_int8_t status;
	u_int16_t intmask;
	int i, error;

	sc->sc_enab_intr = 0;
	sc->sc_invalid = 0;	/* XXX: really? */
	awi_drvstate(sc, AWI_DRV_RESET);

	/* reset firmware */
	am79c930_gcr_setbits(&sc->sc_chip, AM79C930_GCR_CORESET);
	DELAY(100);
	awi_write_1(sc, AWI_SELFTEST, 0);
	awi_write_1(sc, AWI_CMD, 0);
	awi_write_1(sc, AWI_BANNER, 0);
	am79c930_gcr_clearbits(&sc->sc_chip, AM79C930_GCR_CORESET);
	DELAY(100);

	/* wait for selftest completion */
	for (i = 0; ; i++) {
		if (i >= AWI_SELFTEST_TIMEOUT*hz/1000) {
			printf("%s: failed to complete selftest (timeout)\n",
			    sc->sc_dev.dv_xname);
			return ENXIO;
		}
		status = awi_read_1(sc, AWI_SELFTEST);
		if ((status & 0xf0) == 0xf0)
			break;
		if (sc->sc_cansleep) {
			sc->sc_sleep_cnt++;
			(void)tsleep(sc, PWAIT, "awitst", 1);
			sc->sc_sleep_cnt--;
		} else {
			DELAY(1000*1000/hz);
		}
	}
	if (status != AWI_SELFTEST_PASSED) {
		printf("%s: failed to complete selftest (code %x)\n",
		    sc->sc_dev.dv_xname, status);
		return ENXIO;
	}

	/* check banner to confirm firmware write it */
	awi_read_bytes(sc, AWI_BANNER, sc->sc_banner, AWI_BANNER_LEN);
	if (memcmp(sc->sc_banner, "PCnetMobile:", 12) != 0) {
		printf("%s: failed to complete selftest (bad banner)\n",
		    sc->sc_dev.dv_xname);
		for (i = 0; i < AWI_BANNER_LEN; i++)
			printf("%s%02x", i ? ":" : "\t", sc->sc_banner[i]);
		printf("\n");
		return ENXIO;
	}

	/* initializing interrupt */
	sc->sc_enab_intr = 1;
	error = awi_intr_lock(sc);
	if (error)
		return error;
	intmask = AWI_INT_GROGGY | AWI_INT_SCAN_CMPLT |
	    AWI_INT_TX | AWI_INT_RX | AWI_INT_CMD;
	awi_write_1(sc, AWI_INTMASK, ~intmask & 0xff);
	awi_write_1(sc, AWI_INTMASK2, 0);
	awi_write_1(sc, AWI_INTSTAT, 0);
	awi_write_1(sc, AWI_INTSTAT2, 0);
	awi_intr_unlock(sc);
	am79c930_gcr_setbits(&sc->sc_chip, AM79C930_GCR_ENECINT);

	/* issueing interface test command */
	error = awi_cmd(sc, AWI_CMD_NOP);
	if (error) {
		printf("%s: failed to complete selftest", sc->sc_dev.dv_xname);
		if (error == ENXIO)
			printf(" (no hardware)\n");
		else if (error != EWOULDBLOCK)
			printf(" (error %d)\n", error);
		else if (sc->sc_cansleep)
			printf(" (lost interrupt)\n");
		else
			printf(" (command timeout)\n");
	}
	return error;
}

/*
 * Extract the factory default MIB value from firmware and assign the driver
 * default value.
 * Called once at attaching the interface.
 */

static int
awi_init_mibs(sc)
	struct awi_softc *sc;
{
	int i, error;
	u_int8_t *rate;

	if ((error = awi_mib(sc, AWI_CMD_GET_MIB, AWI_MIB_LOCAL)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_GET_MIB, AWI_MIB_ADDR)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_GET_MIB, AWI_MIB_MAC)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_GET_MIB, AWI_MIB_MGT)) != 0 ||
	    (error = awi_mib(sc, AWI_CMD_GET_MIB, AWI_MIB_PHY)) != 0) {
		printf("%s: failed to get default mib value (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		return error;
	}

	rate = sc->sc_mib_phy.aSuprt_Data_Rates;
	sc->sc_tx_rate = AWI_RATE_1MBIT;
	for (i = 0; i < rate[1]; i++) {
		if (AWI_80211_RATE(rate[2 + i]) > sc->sc_tx_rate)
			sc->sc_tx_rate = AWI_80211_RATE(rate[2 + i]);
	}
	awi_init_region(sc);
	memset(&sc->sc_mib_mac.aDesired_ESS_ID, 0, AWI_ESS_ID_SIZE);
	sc->sc_mib_mac.aDesired_ESS_ID[0] = IEEE80211_ELEMID_SSID;
	sc->sc_mib_local.Fragmentation_Dis = 1;
	sc->sc_mib_local.Accept_All_Multicast_Dis = 1;
	sc->sc_mib_local.Power_Saving_Mode_Dis = 1;

	/* allocate buffers */
	sc->sc_txbase = AWI_BUFFERS;
	sc->sc_txend = sc->sc_txbase +
	    (AWI_TXD_SIZE + sizeof(struct ieee80211_frame) +
	    sizeof(struct ether_header) + ETHERMTU) * AWI_NTXBUFS;
	LE_WRITE_4(&sc->sc_mib_local.Tx_Buffer_Offset, sc->sc_txbase);
	LE_WRITE_4(&sc->sc_mib_local.Tx_Buffer_Size,
	    sc->sc_txend - sc->sc_txbase);
	LE_WRITE_4(&sc->sc_mib_local.Rx_Buffer_Offset, sc->sc_txend);
	LE_WRITE_4(&sc->sc_mib_local.Rx_Buffer_Size,
	    AWI_BUFFERS_END - sc->sc_txend);
	sc->sc_mib_local.Network_Mode = 1;
	sc->sc_mib_local.Acting_as_AP = 0;
	return 0;
}

/*
 * Start transmitter and receiver of firmware
 * Called after awi_init_hw() to start operation.
 */

static int
awi_init_txrx(sc)
	struct awi_softc *sc;
{
	int error;

	/* start transmitter */
	sc->sc_txdone = sc->sc_txnext = sc->sc_txbase;
	awi_write_4(sc, sc->sc_txbase + AWI_TXD_START, 0);
	awi_write_4(sc, sc->sc_txbase + AWI_TXD_NEXT, 0);
	awi_write_4(sc, sc->sc_txbase + AWI_TXD_LENGTH, 0);
	awi_write_1(sc, sc->sc_txbase + AWI_TXD_RATE, 0);
	awi_write_4(sc, sc->sc_txbase + AWI_TXD_NDA, 0);
	awi_write_4(sc, sc->sc_txbase + AWI_TXD_NRA, 0);
	awi_write_1(sc, sc->sc_txbase + AWI_TXD_STATE, 0);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_DATA, sc->sc_txbase);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_MGT, 0);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_BCAST, 0);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_PS, 0);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_TX_CF, 0);
	error = awi_cmd(sc, AWI_CMD_INIT_TX);
	if (error)
		return error;

	/* start receiver */
	if (sc->sc_rxpend) {
		m_freem(sc->sc_rxpend);
		sc->sc_rxpend = NULL;
	}
	error = awi_cmd(sc, AWI_CMD_INIT_RX);
	if (error)
		return error;
	sc->sc_rxdoff = awi_read_4(sc, AWI_CMD_PARAMS+AWI_CA_IRX_DATA_DESC);
	sc->sc_rxmoff = awi_read_4(sc, AWI_CMD_PARAMS+AWI_CA_IRX_PS_DESC);
	return 0;
}

static void
awi_stop_txrx(sc)
	struct awi_softc *sc;
{

	if (sc->sc_cmd_inprog)
		(void)awi_cmd_wait(sc);
	(void)awi_cmd(sc, AWI_CMD_KILL_RX);
	(void)awi_cmd_wait(sc);
	sc->sc_cmd_inprog = AWI_CMD_FLUSH_TX;
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_FTX_DATA, 1);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_FTX_MGT, 0);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_FTX_BCAST, 0);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_FTX_PS, 0);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_FTX_CF, 0);
	(void)awi_cmd(sc, AWI_CMD_FLUSH_TX);
	(void)awi_cmd_wait(sc);
}

int
awi_init_region(sc)
	struct awi_softc *sc;
{

	if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH) {
		switch (sc->sc_mib_phy.aCurrent_Reg_Domain) {
		case AWI_REG_DOMAIN_US:
		case AWI_REG_DOMAIN_CA:
		case AWI_REG_DOMAIN_EU:
			sc->sc_scan_min = 0;
			sc->sc_scan_max = 77;
			break;
		case AWI_REG_DOMAIN_ES:
			sc->sc_scan_min = 0;
			sc->sc_scan_max = 26;
			break;
		case AWI_REG_DOMAIN_FR:
			sc->sc_scan_min = 0;
			sc->sc_scan_max = 32;
			break;
		case AWI_REG_DOMAIN_JP:
			sc->sc_scan_min = 6;
			sc->sc_scan_max = 17;
			break;
		default:
			return EINVAL;
		}
		sc->sc_scan_set = sc->sc_scan_cur % 3 + 1;
	} else {
		switch (sc->sc_mib_phy.aCurrent_Reg_Domain) {
		case AWI_REG_DOMAIN_US:
		case AWI_REG_DOMAIN_CA:
			sc->sc_scan_min = 1;
			sc->sc_scan_max = 11;
			sc->sc_scan_cur = 3;
			break;
		case AWI_REG_DOMAIN_EU:
			sc->sc_scan_min = 1;
			sc->sc_scan_max = 13;
			sc->sc_scan_cur = 3;
			break;
		case AWI_REG_DOMAIN_ES:
			sc->sc_scan_min = 10;
			sc->sc_scan_max = 11;
			sc->sc_scan_cur = 10;
			break;
		case AWI_REG_DOMAIN_FR:
			sc->sc_scan_min = 10;
			sc->sc_scan_max = 13;
			sc->sc_scan_cur = 10;
			break;
		case AWI_REG_DOMAIN_JP:
			sc->sc_scan_min = 14;
			sc->sc_scan_max = 14;
			sc->sc_scan_cur = 14;
			break;
		default:
			return EINVAL;
		}
	}
	sc->sc_ownch = sc->sc_scan_cur;
	return 0;
}

static int
awi_start_scan(sc)
	struct awi_softc *sc;
{
	int error = 0;
	struct awi_bss *bp;

	while ((bp = TAILQ_FIRST(&sc->sc_scan)) != NULL) {
		TAILQ_REMOVE(&sc->sc_scan, bp, list);
		free(bp, M_DEVBUF);
	}
	if (!sc->sc_mib_local.Network_Mode && sc->sc_no_bssid) {
		memset(&sc->sc_bss, 0, sizeof(sc->sc_bss));
		sc->sc_bss.essid[0] = IEEE80211_ELEMID_SSID;
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH) {
			sc->sc_bss.chanset = sc->sc_ownch % 3 + 1;
			sc->sc_bss.pattern = sc->sc_ownch;
			sc->sc_bss.index = 1;
			sc->sc_bss.dwell_time = 200;	/*XXX*/
		} else
			sc->sc_bss.chanset = sc->sc_ownch;
		sc->sc_status = AWI_ST_SETSS;
		error = awi_set_ss(sc);
	} else {
		if (sc->sc_mib_local.Network_Mode)
			awi_drvstate(sc, AWI_DRV_INFSC);
		else
			awi_drvstate(sc, AWI_DRV_ADHSC);
		sc->sc_start_bss = 0;
		sc->sc_active_scan = 1;
		sc->sc_mgt_timer = AWI_ASCAN_WAIT / 1000;
		sc->sc_ifp->if_timer = 1;
		sc->sc_status = AWI_ST_SCAN;
		error = awi_cmd_scan(sc);
	}
	return error;
}

static int
awi_next_scan(sc)
	struct awi_softc *sc;
{
	int error;

	for (;;) {
		/*
		 * The pattern parameter for FH phy should be incremented
		 * by 3.  But BayStack 650 Access Points apparently always
		 * assign hop pattern set parameter to 1 for any pattern.
		 * So we try all combinations of pattern/set parameters.
		 * Since this causes no error, it may be a bug of
		 * PCnetMobile firmware.
		 */
		sc->sc_scan_cur++;
		if (sc->sc_scan_cur > sc->sc_scan_max) {
			sc->sc_scan_cur = sc->sc_scan_min;
			if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
				sc->sc_scan_set = sc->sc_scan_set % 3 + 1;
		}
		error = awi_cmd_scan(sc);
		if (error != EINVAL)
			break;
	}
	return error;
}

static void
awi_stop_scan(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct awi_bss *bp, *sbp;
	int fail;

	bp = TAILQ_FIRST(&sc->sc_scan);
	if (bp == NULL) {
  notfound:
		if (sc->sc_active_scan) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: entering passive scan mode\n",
				    sc->sc_dev.dv_xname);
			sc->sc_active_scan = 0;
		}
		sc->sc_mgt_timer = AWI_PSCAN_WAIT / 1000;
		ifp->if_timer = 1;
		(void)awi_next_scan(sc);
		return;
	}
	sbp = NULL;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s:\tmacaddr     ch/pat   sig flag  wep  essid\n",
		    sc->sc_dev.dv_xname);
	for (; bp != NULL; bp = TAILQ_NEXT(bp, list)) {
		if (bp->fails) {
			/*
			 * The configuration of the access points may change
			 * during my scan.  So we retries to associate with
			 * it unless there are any suitable AP.
			 */
			if (bp->fails++ < 3)
				continue;
			bp->fails = 0;
		}
		fail = 0;
		/*
		 * Since the firmware apparently scans not only the specified
		 * channel of SCAN command but all available channel within
		 * the region, we should filter out unnecessary responses here.
		 */
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH) {
			if (bp->pattern < sc->sc_scan_min ||
			    bp->pattern > sc->sc_scan_max)
				fail |= 0x01;
		} else {
			if (bp->chanset < sc->sc_scan_min ||
			    bp->chanset > sc->sc_scan_max)
				fail |= 0x01;
		}
		if (sc->sc_mib_local.Network_Mode) {
			if (!(bp->capinfo & IEEE80211_CAPINFO_ESS) ||
			    (bp->capinfo & IEEE80211_CAPINFO_IBSS))
				fail |= 0x02;
		} else {
			if ((bp->capinfo & IEEE80211_CAPINFO_ESS) ||
			    !(bp->capinfo & IEEE80211_CAPINFO_IBSS))
				fail |= 0x02;
		}
		if (sc->sc_wep_algo == NULL) {
			if (bp->capinfo & IEEE80211_CAPINFO_PRIVACY)
				fail |= 0x04;
		} else {
			if (!(bp->capinfo & IEEE80211_CAPINFO_PRIVACY))
				fail |= 0x04;
		}
		if (sc->sc_mib_mac.aDesired_ESS_ID[1] != 0 &&
		    memcmp(&sc->sc_mib_mac.aDesired_ESS_ID, bp->essid,
		    sizeof(bp->essid)) != 0)
			fail |= 0x08;
		if (ifp->if_flags & IFF_DEBUG) {
			printf(" %c %s", fail ? '-' : '+',
			    ether_sprintf(bp->esrc));
			if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
				printf("  %2d/%d%c", bp->pattern, bp->chanset,
				    fail & 0x01 ? '!' : ' ');
			else
				printf("  %4d%c", bp->chanset,
				    fail & 0x01 ? '!' : ' ');
			printf(" %+4d", bp->rssi);
			printf(" %4s%c",
			    (bp->capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
			    (bp->capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
			    "????",
			    fail & 0x02 ? '!' : ' ');
			printf(" %3s%c ",
			    (bp->capinfo & IEEE80211_CAPINFO_PRIVACY) ? "wep" :
			    "no",
			    fail & 0x04 ? '!' : ' ');
			awi_print_essid(bp->essid);
			printf("%s\n", fail & 0x08 ? "!" : "");
		}
		if (!fail) {
			if (sbp == NULL || bp->rssi > sbp->rssi)
				sbp = bp;
		}
	}
	if (sbp == NULL)
		goto notfound;
	sc->sc_bss = *sbp;
	(void)awi_set_ss(sc);
}

static void
awi_recv_beacon(sc, m0, rxts, rssi)
	struct awi_softc *sc;
	struct mbuf *m0;
	u_int32_t rxts;
	u_int8_t rssi;
{
	struct ieee80211_frame *wh;
	struct awi_bss *bp;
	u_int8_t *frame, *eframe;
	u_int8_t *tstamp, *bintval, *capinfo, *ssid, *rates, *parms;

	if (sc->sc_status != AWI_ST_SCAN)
		return;
	wh = mtod(m0, struct ieee80211_frame *);

	frame = (u_int8_t *)&wh[1];
	eframe = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * XXX:
	 *	timestamp [8]
	 *	beacon interval [2]
	 *	capability information [2]
	 *	ssid [tlv]
	 *	supported rates [tlv]
	 *	parameter set [tlv]
	 *	...
	 */
	if (frame + 12 > eframe) {
#ifdef AWI_DEBUG
		if (awi_verbose)
			printf("awi_recv_beacon: frame too short \n");
#endif
		return;
	}
	tstamp = frame;
	frame += 8;
	bintval = frame;
	frame += 2;
	capinfo = frame;
	frame += 2;

	ssid = rates = parms = NULL;
	while (frame < eframe) {
		switch (*frame) {
		case IEEE80211_ELEMID_SSID:
			ssid = frame;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frame;
			break;
		case IEEE80211_ELEMID_FHPARMS:
		case IEEE80211_ELEMID_DSPARMS:
			parms = frame;
			break;
		}
		frame += frame[1] + 2;
	}
	if (ssid == NULL || rates == NULL || parms == NULL) {
#ifdef AWI_DEBUG
		if (awi_verbose)
			printf("awi_recv_beacon: ssid=%p, rates=%p, parms=%p\n",
			    ssid, rates, parms);
#endif
		return;
	}
	if (ssid[1] > IEEE80211_NWID_LEN) {
#ifdef AWI_DEBUG
		if (awi_verbose)
			printf("awi_recv_beacon: bad ssid len: %d from %s\n",
			    ssid[1], ether_sprintf(wh->i_addr2));
#endif
		return;
	}

	TAILQ_FOREACH(bp, &sc->sc_scan, list) {
		if (memcmp(bp->esrc, wh->i_addr2, ETHER_ADDR_LEN) == 0 &&
		    memcmp(bp->bssid, wh->i_addr3, ETHER_ADDR_LEN) == 0)
			break;
	}
	if (bp == NULL) {
		bp = malloc(sizeof(struct awi_bss), M_DEVBUF, M_NOWAIT);
		if (bp == NULL)
			return;
		TAILQ_INSERT_TAIL(&sc->sc_scan, bp, list);
		memcpy(bp->esrc, wh->i_addr2, ETHER_ADDR_LEN);
		memcpy(bp->bssid, wh->i_addr3, ETHER_ADDR_LEN);
		memset(bp->essid, 0, sizeof(bp->essid));
		memcpy(bp->essid, ssid, 2 + ssid[1]);
	}
	bp->rssi = rssi;
	bp->rxtime = rxts;
	memcpy(bp->timestamp, tstamp, sizeof(bp->timestamp));
	bp->interval = LE_READ_2(bintval);
	bp->capinfo = LE_READ_2(capinfo);
	if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH) {
		bp->chanset = parms[4];
		bp->pattern = parms[5];
		bp->index = parms[6];
		bp->dwell_time = LE_READ_2(parms + 2);
	} else {
		bp->chanset = parms[2];
		bp->pattern = 0;
		bp->index = 0;
		bp->dwell_time = 0;
	}
	if (sc->sc_mgt_timer == 0)
		awi_stop_scan(sc);
}

static int
awi_set_ss(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct awi_bss *bp;
	int error;

	sc->sc_status = AWI_ST_SETSS;
	bp = &sc->sc_bss;
	if (ifp->if_flags & IFF_DEBUG) {
		printf("%s: ch %d pat %d id %d dw %d iv %d bss %s ssid ",
		    sc->sc_dev.dv_xname, bp->chanset,
		    bp->pattern, bp->index, bp->dwell_time, bp->interval,
		    ether_sprintf(bp->bssid));
		awi_print_essid(bp->essid);
		printf("\n");
	}
	memcpy(&sc->sc_mib_mgt.aCurrent_BSS_ID, bp->bssid, ETHER_ADDR_LEN);
	memcpy(&sc->sc_mib_mgt.aCurrent_ESS_ID, bp->essid,
	    AWI_ESS_ID_SIZE);
	LE_WRITE_2(&sc->sc_mib_mgt.aBeacon_Period, bp->interval);
	error = awi_mib(sc, AWI_CMD_SET_MIB, AWI_MIB_MGT);
	return error;
}

static void
awi_try_sync(sc)
	struct awi_softc *sc;
{
	struct awi_bss *bp;

	sc->sc_status = AWI_ST_SYNC;
	bp = &sc->sc_bss;

	if (sc->sc_cmd_inprog) {
		if (awi_cmd_wait(sc))
			return;
	}
	sc->sc_cmd_inprog = AWI_CMD_SYNC;
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_SET, bp->chanset);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_PATTERN, bp->pattern);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_IDX, bp->index);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_STARTBSS,
	    sc->sc_start_bss ? 1 : 0); 
	awi_write_2(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_DWELL, bp->dwell_time);
	awi_write_2(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_MBZ, 0);
	awi_write_bytes(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_TIMESTAMP,
	    bp->timestamp, 8);
	awi_write_4(sc, AWI_CMD_PARAMS+AWI_CA_SYNC_REFTIME, bp->rxtime);
	(void)awi_cmd(sc, AWI_CMD_SYNC);
}

static void
awi_sync_done(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;

	if (sc->sc_mib_local.Network_Mode) {
		awi_drvstate(sc, AWI_DRV_INFSY);
		awi_send_auth(sc, 1);
	} else {
		if (ifp->if_flags & IFF_DEBUG) {
			printf("%s: synced with", sc->sc_dev.dv_xname);
			if (sc->sc_no_bssid)
				printf(" no-bssid");
			else {
				printf(" %s ssid ",
				    ether_sprintf(sc->sc_bss.bssid));
				awi_print_essid(sc->sc_bss.essid);
			}
			if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
				printf(" at chanset %d pattern %d\n",
				    sc->sc_bss.chanset, sc->sc_bss.pattern);
			else
				printf(" at channel %d\n", sc->sc_bss.chanset);
		}
		awi_drvstate(sc, AWI_DRV_ADHSY);
		sc->sc_status = AWI_ST_RUNNING;
		ifp->if_flags |= IFF_RUNNING;
		awi_start(ifp);
	}
}

static void
awi_send_deauth(sc)
	struct awi_softc *sc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	u_int8_t *deauth;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: sending deauth to %s\n", sc->sc_dev.dv_xname,
		    ether_sprintf(sc->sc_bss.bssid));

	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_AUTH;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	LE_WRITE_2(wh->i_dur, 0);
	LE_WRITE_2(wh->i_seq, 0);
	memcpy(wh->i_addr1, sc->sc_bss.bssid, ETHER_ADDR_LEN);
	memcpy(wh->i_addr2, sc->sc_mib_addr.aMAC_Address, ETHER_ADDR_LEN);
	memcpy(wh->i_addr3, sc->sc_bss.bssid, ETHER_ADDR_LEN);

	deauth = (u_int8_t *)&wh[1];
	LE_WRITE_2(deauth, IEEE80211_REASON_AUTH_LEAVE);
	deauth += 2;

	m->m_pkthdr.len = m->m_len = deauth - mtod(m, u_int8_t *);
	_IF_ENQUEUE(&sc->sc_mgtq, m);
	awi_start(ifp);
	awi_drvstate(sc, AWI_DRV_INFTOSS);
}

static void
awi_send_auth(sc, seq)
	struct awi_softc *sc;
	int seq;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	u_int8_t *auth;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	sc->sc_status = AWI_ST_AUTH;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: sending auth to %s\n", sc->sc_dev.dv_xname,
		    ether_sprintf(sc->sc_bss.bssid));

	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_AUTH;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	LE_WRITE_2(wh->i_dur, 0);
	LE_WRITE_2(wh->i_seq, 0);
	memcpy(wh->i_addr1, sc->sc_bss.esrc, ETHER_ADDR_LEN);
	memcpy(wh->i_addr2, sc->sc_mib_addr.aMAC_Address, ETHER_ADDR_LEN);
	memcpy(wh->i_addr3, sc->sc_bss.bssid, ETHER_ADDR_LEN);

	auth = (u_int8_t *)&wh[1];
	/* algorithm number */
	LE_WRITE_2(auth, IEEE80211_AUTH_ALG_OPEN);
	auth += 2;
	/* sequence number */
	LE_WRITE_2(auth, seq);
	auth += 2;
	/* status */
	LE_WRITE_2(auth, 0);
	auth += 2;

	m->m_pkthdr.len = m->m_len = auth - mtod(m, u_int8_t *);
	_IF_ENQUEUE(&sc->sc_mgtq, m);
	awi_start(ifp);

	sc->sc_mgt_timer = AWI_TRANS_TIMEOUT / 1000;
	ifp->if_timer = 1;
}

static void
awi_recv_auth(sc, m0)
	struct awi_softc *sc;
	struct mbuf *m0;
{
	struct ieee80211_frame *wh;
	u_int8_t *auth, *eframe;
	struct awi_bss *bp;
	u_int16_t status;

	wh = mtod(m0, struct ieee80211_frame *);
	auth = (u_int8_t *)&wh[1];
	eframe = mtod(m0, u_int8_t *) + m0->m_len;
	if (sc->sc_ifp->if_flags & IFF_DEBUG)
		printf("%s: receive auth from %s\n", sc->sc_dev.dv_xname,
		    ether_sprintf(wh->i_addr2));

	/* algorithm number */
	if (LE_READ_2(auth) != IEEE80211_AUTH_ALG_OPEN)
		return;
	auth += 2;
	if (!sc->sc_mib_local.Network_Mode) {
		if (sc->sc_status != AWI_ST_RUNNING)
			return;
		if (LE_READ_2(auth) == 1)
			awi_send_auth(sc, 2);
		return;
	}
	if (sc->sc_status != AWI_ST_AUTH)
		return;
	/* sequence number */
	if (LE_READ_2(auth) != 2)
		return;
	auth += 2;
	/* status */
	status = LE_READ_2(auth);
	if (status != 0) {
		printf("%s: authentication failed (reason %d)\n",
		    sc->sc_dev.dv_xname, status);
		TAILQ_FOREACH(bp, &sc->sc_scan, list) {
			if (memcmp(bp->esrc, sc->sc_bss.esrc, ETHER_ADDR_LEN)
			    == 0) {
				bp->fails++;
				break;
			}
		}
		return;
	}
	sc->sc_mgt_timer = 0;
	awi_drvstate(sc, AWI_DRV_INFAUTH);
	awi_send_asreq(sc, 0);
}

static void
awi_send_asreq(sc, reassoc)
	struct awi_softc *sc;
	int reassoc;
{
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	u_int16_t capinfo, lintval;
	u_int8_t *asreq;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;
	sc->sc_status = AWI_ST_ASSOC;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: sending %sassoc req to %s\n", sc->sc_dev.dv_xname,
		    reassoc ? "re" : "",
		    ether_sprintf(sc->sc_bss.bssid));

	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT;
	if (reassoc)
		wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_REASSOC_REQ;
	else
		wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_ASSOC_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	LE_WRITE_2(wh->i_dur, 0);
	LE_WRITE_2(wh->i_seq, 0);
	memcpy(wh->i_addr1, sc->sc_bss.esrc, ETHER_ADDR_LEN);
	memcpy(wh->i_addr2, sc->sc_mib_addr.aMAC_Address, ETHER_ADDR_LEN);
	memcpy(wh->i_addr3, sc->sc_bss.bssid, ETHER_ADDR_LEN);

	asreq = (u_int8_t *)&wh[1];

	/* capability info */
	capinfo = IEEE80211_CAPINFO_CF_POLLABLE;
	if (sc->sc_mib_local.Network_Mode)
		capinfo |= IEEE80211_CAPINFO_ESS;
	else
		capinfo |= IEEE80211_CAPINFO_IBSS;
	if (sc->sc_wep_algo != NULL)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	LE_WRITE_2(asreq, capinfo);
	asreq += 2;

	/* listen interval */
	lintval = LE_READ_2(&sc->sc_mib_mgt.aListen_Interval);
	LE_WRITE_2(asreq, lintval);
	asreq += 2;
	if (reassoc) {
		/* current AP address */
		memcpy(asreq, sc->sc_bss.bssid, ETHER_ADDR_LEN);
		asreq += ETHER_ADDR_LEN;
	}
	/* ssid */
	memcpy(asreq, sc->sc_bss.essid, 2 + sc->sc_bss.essid[1]);
	asreq += 2 + asreq[1];
	/* supported rates */
	memcpy(asreq, &sc->sc_mib_phy.aSuprt_Data_Rates, 4);
	asreq += 2 + asreq[1];

	m->m_pkthdr.len = m->m_len = asreq - mtod(m, u_int8_t *);
	_IF_ENQUEUE(&sc->sc_mgtq, m);
	awi_start(ifp);

	sc->sc_mgt_timer = AWI_TRANS_TIMEOUT / 1000;
	ifp->if_timer = 1;
}

static void
awi_recv_asresp(sc, m0)
	struct awi_softc *sc;
	struct mbuf *m0;
{
	struct ieee80211_frame *wh;
	u_int8_t *asresp, *eframe;
	u_int16_t status;
	u_int8_t rate, *phy_rates;
	struct awi_bss *bp;
	int i, j;

	wh = mtod(m0, struct ieee80211_frame *);
	asresp = (u_int8_t *)&wh[1];
	eframe = mtod(m0, u_int8_t *) + m0->m_len;
	if (sc->sc_ifp->if_flags & IFF_DEBUG)
		printf("%s: receive assoc resp from %s\n", sc->sc_dev.dv_xname,
		    ether_sprintf(wh->i_addr2));

	if (!sc->sc_mib_local.Network_Mode)
		return;

	if (sc->sc_status != AWI_ST_ASSOC)
		return;
	/* capability info */
	asresp += 2;
	/* status */
	status = LE_READ_2(asresp);
	if (status != 0) {
		printf("%s: association failed (reason %d)\n",
		    sc->sc_dev.dv_xname, status);
		TAILQ_FOREACH(bp, &sc->sc_scan, list) {
			if (memcmp(bp->esrc, sc->sc_bss.esrc, ETHER_ADDR_LEN)
			    == 0) {
				bp->fails++;
				break;
			}
		}
		return;
	}
	asresp += 2;
	/* association id */
	asresp += 2;
	/* supported rates */
	rate = AWI_RATE_1MBIT;
	for (i = 0; i < asresp[1]; i++) {
		if (AWI_80211_RATE(asresp[2 + i]) <= rate)
			continue;
		phy_rates = sc->sc_mib_phy.aSuprt_Data_Rates;
		for (j = 0; j < phy_rates[1]; j++) {
			if (AWI_80211_RATE(asresp[2 + i]) ==
			    AWI_80211_RATE(phy_rates[2 + j]))
				rate = AWI_80211_RATE(asresp[2 + i]);
		}
	}
	if (sc->sc_ifp->if_flags & IFF_DEBUG) {
		printf("%s: associated with %s ssid ",
		    sc->sc_dev.dv_xname, ether_sprintf(sc->sc_bss.bssid));
		awi_print_essid(sc->sc_bss.essid);
		if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH)
			printf(" chanset %d pattern %d\n",
			    sc->sc_bss.chanset, sc->sc_bss.pattern);
		else
			printf(" channel %d\n", sc->sc_bss.chanset);
	}
	sc->sc_tx_rate = rate;
	sc->sc_mgt_timer = 0;
	sc->sc_rx_timer = 10;
	sc->sc_ifp->if_timer = 1;
	sc->sc_status = AWI_ST_RUNNING;
	sc->sc_ifp->if_flags |= IFF_RUNNING;
	awi_drvstate(sc, AWI_DRV_INFASSOC);
	awi_start(sc->sc_ifp);
}

static int
awi_mib(sc, cmd, mib)
	struct awi_softc *sc;
	u_int8_t cmd;
	u_int8_t mib;
{
	int error;
	u_int8_t size, *ptr;

	switch (mib) {
	case AWI_MIB_LOCAL:
		ptr = (u_int8_t *)&sc->sc_mib_local;
		size = sizeof(sc->sc_mib_local);
		break;
	case AWI_MIB_ADDR:
		ptr = (u_int8_t *)&sc->sc_mib_addr;
		size = sizeof(sc->sc_mib_addr);
		break;
	case AWI_MIB_MAC:
		ptr = (u_int8_t *)&sc->sc_mib_mac;
		size = sizeof(sc->sc_mib_mac);
		break;
	case AWI_MIB_STAT:
		ptr = (u_int8_t *)&sc->sc_mib_stat;
		size = sizeof(sc->sc_mib_stat);
		break;
	case AWI_MIB_MGT:
		ptr = (u_int8_t *)&sc->sc_mib_mgt;
		size = sizeof(sc->sc_mib_mgt);
		break;
	case AWI_MIB_PHY:
		ptr = (u_int8_t *)&sc->sc_mib_phy;
		size = sizeof(sc->sc_mib_phy);
		break;
	default:
		return EINVAL;
	}
	if (sc->sc_cmd_inprog) {
		error = awi_cmd_wait(sc);
		if (error) {
			if (error == EWOULDBLOCK)
				printf("awi_mib: cmd %d inprog",
				    sc->sc_cmd_inprog);
			return error;
		}
	}
	sc->sc_cmd_inprog = cmd;
	if (cmd == AWI_CMD_SET_MIB)
		awi_write_bytes(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA, ptr, size);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_TYPE, mib);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_SIZE, size);
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_MIB_INDEX, 0);
	error = awi_cmd(sc, cmd);
	if (error)
		return error;
	if (cmd == AWI_CMD_GET_MIB) {
		awi_read_bytes(sc, AWI_CMD_PARAMS+AWI_CA_MIB_DATA, ptr, size);
#ifdef AWI_DEBUG
		if (awi_verbose) {
			int i;

			printf("awi_mib: #%d:", mib);
			for (i = 0; i < size; i++)
				printf(" %02x", ptr[i]);
			printf("\n");
		}
#endif
	}
	return 0;
}

static int
awi_cmd_scan(sc)
	struct awi_softc *sc;
{
	int error;
	u_int8_t scan_mode;

	if (sc->sc_active_scan)
		scan_mode = AWI_SCAN_ACTIVE;
	else
		scan_mode = AWI_SCAN_PASSIVE;
	if (sc->sc_mib_mgt.aScan_Mode != scan_mode) {
		sc->sc_mib_mgt.aScan_Mode = scan_mode;
		error = awi_mib(sc, AWI_CMD_SET_MIB, AWI_MIB_MGT);
		return error;
	}

	if (sc->sc_cmd_inprog) {
		error = awi_cmd_wait(sc);
		if (error)
			return error;
	}
	sc->sc_cmd_inprog = AWI_CMD_SCAN;
	awi_write_2(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_DURATION,
	    sc->sc_active_scan ? AWI_ASCAN_DURATION : AWI_PSCAN_DURATION);
	if (sc->sc_mib_phy.IEEE_PHY_Type == AWI_PHY_TYPE_FH) {
		awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_SET,
		    sc->sc_scan_set);
		awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_PATTERN,
		    sc->sc_scan_cur);
		awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_IDX, 1);
	} else {
		awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_SET,
		    sc->sc_scan_cur);
		awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_PATTERN, 0);
		awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_IDX, 0);
	}
	awi_write_1(sc, AWI_CMD_PARAMS+AWI_CA_SCAN_SUSP, 0);
	return awi_cmd(sc, AWI_CMD_SCAN);
}

static int
awi_cmd(sc, cmd)
	struct awi_softc *sc;
	u_int8_t cmd;
{
	u_int8_t status;
	int error = 0;

	sc->sc_cmd_inprog = cmd;
	awi_write_1(sc, AWI_CMD_STATUS, AWI_STAT_IDLE);
	awi_write_1(sc, AWI_CMD, cmd);
	if (sc->sc_status != AWI_ST_INIT)
		return 0;
	error = awi_cmd_wait(sc);
	if (error)
		return error;
	status = awi_read_1(sc, AWI_CMD_STATUS);
	awi_write_1(sc, AWI_CMD, 0);
	switch (status) {
	case AWI_STAT_OK:
		break;
	case AWI_STAT_BADPARM:
		return EINVAL;
	default:
		printf("%s: command %d failed %x\n",
		    sc->sc_dev.dv_xname, cmd, status);
		return ENXIO;
	}
	return 0;
}

static void
awi_cmd_done(sc)
	struct awi_softc *sc;
{
	u_int8_t cmd, status;

	status = awi_read_1(sc, AWI_CMD_STATUS);
	if (status == AWI_STAT_IDLE)
		return;		/* stray interrupt */

	cmd = sc->sc_cmd_inprog;
	sc->sc_cmd_inprog = 0;
	if (sc->sc_status == AWI_ST_INIT) {
		wakeup(sc);
		return;
	}
	awi_write_1(sc, AWI_CMD, 0);

	if (status != AWI_STAT_OK) {
		printf("%s: command %d failed %x\n",
		    sc->sc_dev.dv_xname, cmd, status);
		return;
	}
	switch (sc->sc_status) {
	case AWI_ST_SCAN:
		if (cmd == AWI_CMD_SET_MIB)
			awi_cmd_scan(sc);	/* retry */
		break;
	case AWI_ST_SETSS:
		awi_try_sync(sc);
		break;
	case AWI_ST_SYNC:
		awi_sync_done(sc);
		break;
	default:
		break;
	}
}

static int
awi_next_txd(sc, len, framep, ntxdp)
	struct awi_softc *sc;
	int len;
	u_int32_t *framep, *ntxdp;
{
	u_int32_t txd, ntxd, frame;

	txd = sc->sc_txnext;
	frame = txd + AWI_TXD_SIZE;
	if (frame + len > sc->sc_txend)
		frame = sc->sc_txbase;
	ntxd = frame + len;
	if (ntxd + AWI_TXD_SIZE > sc->sc_txend)
		ntxd = sc->sc_txbase;
	*framep = frame;
	*ntxdp = ntxd;
	/*
	 * Determine if there are any room in ring buffer.
	 *		--- send wait,  === new data,  +++ conflict (ENOBUFS)
	 *   base........................end
	 *	   done----txd=====ntxd		OK
	 *	 --txd=====done++++ntxd--	full
	 *	 --txd=====ntxd    done--	OK
	 *	 ==ntxd    done----txd===	OK
	 *	 ==done++++ntxd----txd===	full
	 *	 ++ntxd    txd=====done++	full
	 */
	if (txd < ntxd) {
		if (txd < sc->sc_txdone && ntxd + AWI_TXD_SIZE > sc->sc_txdone)
			return ENOBUFS;
	} else {
		if (txd < sc->sc_txdone || ntxd + AWI_TXD_SIZE > sc->sc_txdone)
			return ENOBUFS;
	}
	return 0;
}

static int
awi_lock(sc)
	struct awi_softc *sc;
{
	int error = 0;

	if (curproc == NULL) {
		/*
		 * XXX
		 * Though driver ioctl should be called with context,
		 * KAME ipv6 stack calls ioctl in interrupt for now.
		 * We simply abort the request if there are other
		 * ioctl requests in progress.
		 */
		if (sc->sc_busy) {
			return EWOULDBLOCK;
			if (sc->sc_invalid)
				return ENXIO;
		}
		sc->sc_busy = 1;
		sc->sc_cansleep = 0;
		return 0;
	}
	while (sc->sc_busy) {
		if (sc->sc_invalid)
			return ENXIO;
		sc->sc_sleep_cnt++;
		error = tsleep(sc, PWAIT | PCATCH, "awilck", 0);
		sc->sc_sleep_cnt--;
		if (error)
			return error;
	}
	sc->sc_busy = 1;
	sc->sc_cansleep = 1;
	return 0;
}

static void
awi_unlock(sc)
	struct awi_softc *sc;
{
	sc->sc_busy = 0;
	sc->sc_cansleep = 0;
	if (sc->sc_sleep_cnt)
		wakeup(sc);
}

static int
awi_intr_lock(sc)
	struct awi_softc *sc;
{
	u_int8_t status;
	int i, retry;

	status = 1;
	for (retry = 0; retry < 10; retry++) {
		for (i = 0; i < AWI_LOCKOUT_TIMEOUT*1000/5; i++) {
			status = awi_read_1(sc, AWI_LOCKOUT_HOST);
			if (status == 0)
				break;
			DELAY(5);
		}
		if (status != 0)
			break;
		awi_write_1(sc, AWI_LOCKOUT_MAC, 1);
		status = awi_read_1(sc, AWI_LOCKOUT_HOST);
		if (status == 0)
			break;
		awi_write_1(sc, AWI_LOCKOUT_MAC, 0);
	}
	if (status != 0) {
		printf("%s: failed to lock interrupt\n",
		    sc->sc_dev.dv_xname);
		return ENXIO;
	}
	return 0;
}

static void
awi_intr_unlock(sc)
	struct awi_softc *sc;
{

	awi_write_1(sc, AWI_LOCKOUT_MAC, 0);
}

static int
awi_cmd_wait(sc)
	struct awi_softc *sc;
{
	int i, error = 0;

	i = 0;
	while (sc->sc_cmd_inprog) {
		if (sc->sc_invalid)
			return ENXIO;
		if (awi_read_1(sc, AWI_CMD) != sc->sc_cmd_inprog) {
			printf("%s: failed to access hardware\n",
			    sc->sc_dev.dv_xname);
			sc->sc_invalid = 1;
			return ENXIO;
		}
		if (sc->sc_cansleep) {
			sc->sc_sleep_cnt++;
			error = tsleep(sc, PWAIT, "awicmd",
			    AWI_CMD_TIMEOUT*hz/1000);
			sc->sc_sleep_cnt--;
		} else {
			if (awi_read_1(sc, AWI_CMD_STATUS) != AWI_STAT_IDLE) {
				awi_cmd_done(sc);
				break;
			}
			if (i++ >= AWI_CMD_TIMEOUT*1000/10)
				error = EWOULDBLOCK;
			else
				DELAY(10);
		}
		if (error)
			break;
	}
	return error;
}

static void
awi_print_essid(essid)
	u_int8_t *essid;
{
	int i, len;
	u_int8_t *p;

	len = essid[1];
	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;	/*XXX*/
	/* determine printable or not */
	for (i = 0, p = essid + 2; i < len; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i == len) {
		printf("\"");
		for (i = 0, p = essid + 2; i < len; i++, p++)
			printf("%c", *p);
		printf("\"");
	} else {
		printf("0x");
		for (i = 0, p = essid + 2; i < len; i++, p++)
			printf("%02x", *p);
	}
}

#ifdef AWI_DEBUG
static void
awi_dump_pkt(sc, m, rssi)
	struct awi_softc *sc;
	struct mbuf *m;
	int rssi;
{
	struct ieee80211_frame *wh;
	int i, l;

	wh = mtod(m, struct ieee80211_frame *);

	if (awi_dump_mask != 0 &&
	    ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK)==IEEE80211_FC1_DIR_NODS) &&
	    ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK)==IEEE80211_FC0_TYPE_MGT)) {
		if ((AWI_DUMP_MASK(wh->i_fc[0]) & awi_dump_mask) != 0)
			return;
	}
	if (awi_dump_mask < 0 &&
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK)==IEEE80211_FC0_TYPE_DATA)
		return;

	if (rssi < 0)
		printf("tx: ");
	else
		printf("rx: ");
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		printf("NODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr3));
		break;
	case IEEE80211_FC1_DIR_TODS:
		printf("TODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s)", ether_sprintf(wh->i_addr1));
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		printf("FRDS %s", ether_sprintf(wh->i_addr3));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr2));
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		printf("DSDS %s", ether_sprintf((u_int8_t *)&wh[1]));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s", ether_sprintf(wh->i_addr2));
		printf("->%s)", ether_sprintf(wh->i_addr1));
		break;
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		printf(" data");
		break;
	case IEEE80211_FC0_TYPE_MGT:
		switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
		case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
			printf(" probe_req");
			break;
		case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
			printf(" probe_resp");
			break;
		case IEEE80211_FC0_SUBTYPE_BEACON:
			printf(" beacon");
			break;
		case IEEE80211_FC0_SUBTYPE_AUTH:
			printf(" auth");
			break;
		case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
			printf(" assoc_req");
			break;
		case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
			printf(" assoc_resp");
			break;
		case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
			printf(" reassoc_req");
			break;
		case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
			printf(" reassoc_resp");
			break;
		case IEEE80211_FC0_SUBTYPE_DEAUTH:
			printf(" deauth");
			break;
		case IEEE80211_FC0_SUBTYPE_DISASSOC:
			printf(" disassoc");
			break;
		default:
			printf(" mgt#%d",
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		}
		break;
	default:
		printf(" type#%d",
		    wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
		break;
	}
	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
		printf(" WEP");
	if (rssi >= 0)
		printf(" +%d", rssi);
	printf("\n");
	if (awi_dump_len > 0) {
		l = m->m_len;
		if (l > awi_dump_len + sizeof(*wh))
			l = awi_dump_len + sizeof(*wh);
		i = sizeof(*wh);
		if (awi_dump_hdr)
			i = 0;
		for (; i < l; i++) {
			if ((i & 1) == 0)
				printf(" ");
			printf("%02x", mtod(m, u_int8_t *)[i]);
		}
		printf("\n");
	}
}
#endif
