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
 * Lucent WaveLAN/IEEE 802.11 PCMCIA driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
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
 * This driver supports both the PCMCIA and ISA versions of the
 * WaveLAN/IEEE cards. Note however that the ISA card isn't really
 * anything of the sort: it's actually a PCMCIA bridge adapter
 * that fits into an ISA slot, into which a PCMCIA WaveLAN card is
 * inserted. Consequently, you need to use the pccard support for
 * both the ISA and PCMCIA adapters.
 */

#include <sys/param.h>
#include <sys/systm.h>
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
#include <dev/wi/wi_hostap.h>
#include <dev/wi/if_wivar.h>
#include <dev/wi/if_wireg.h>

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

static void wi_intr(void *);
static void wi_reset(struct wi_softc *);
static int wi_ioctl(struct ifnet *, u_long, caddr_t);
static void wi_init(void *);
static void wi_start(struct ifnet *);
static void wi_stop(struct wi_softc *);
static void wi_watchdog(struct ifnet *);
static void wi_rxeof(struct wi_softc *);
static void wi_txeof(struct wi_softc *, int);
static void wi_update_stats(struct wi_softc *);
static void wi_setmulti(struct wi_softc *);

static int wi_cmd(struct wi_softc *, int, int, int, int);
static int wi_read_record(struct wi_softc *, struct wi_ltv_gen *);
static int wi_write_record(struct wi_softc *, struct wi_ltv_gen *);
static int wi_read_data(struct wi_softc *, int, int, caddr_t, int);
static int wi_write_data(struct wi_softc *, int, int, caddr_t, int);
static int wi_seek(struct wi_softc *, int, int, int);
static int wi_alloc_nicmem(struct wi_softc *, int, int *);
static void wi_inquire(void *);
static void wi_setdef(struct wi_softc *, struct wi_req *);

#ifdef WICACHE
static
void wi_cache_store(struct wi_softc *, struct ether_header *,
	struct mbuf *, unsigned short);
#endif

static int wi_get_cur_ssid(struct wi_softc *, char *, int *);
static void wi_get_id(struct wi_softc *);
static int wi_media_change(struct ifnet *);
static void wi_media_status(struct ifnet *, struct ifmediareq *);

static int wi_get_debug(struct wi_softc *, struct wi_req *);
static int wi_set_debug(struct wi_softc *, struct wi_req *);

devclass_t wi_devclass;

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
	{ WI_NIC_3842_PCMCIA_ATM_ID,	WI_NIC_3842_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_3842_MINI_AMD_ID,	WI_NIC_3842_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_MINI_SST_ID,	WI_NIC_3842_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_MINI_ATM_ID,	WI_NIC_3842_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCI_AMD_ID,	WI_NIC_3842_PCI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCI_SST_ID,	WI_NIC_3842_PCI_STR,	WI_INTERSIL },
	{ WI_NIC_3842_PCI_ATM_ID,	WI_NIC_3842_PCI_STR,	WI_INTERSIL },
	{ WI_NIC_P3_PCMCIA_AMD_ID,	WI_NIC_P3_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_P3_PCMCIA_SST_ID,	WI_NIC_P3_PCMCIA_STR,	WI_INTERSIL },
	{ WI_NIC_P3_MINI_AMD_ID,	WI_NIC_P3_MINI_STR,	WI_INTERSIL },
	{ WI_NIC_P3_MINI_SST_ID,	WI_NIC_P3_MINI_STR,	WI_INTERSIL },
	{ 0,	NULL,	0 },
};

int
wi_generic_detach(dev)
	device_t		dev;
{
	struct wi_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	WI_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	if (sc->wi_gone) {
		device_printf(dev, "already unloaded\n");
		WI_UNLOCK(sc);
		return(ENODEV);
	}

	wi_stop(sc);

	/* Delete all remaining media. */
	ifmedia_removeall(&sc->ifmedia);

	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	bus_teardown_intr(dev, sc->irq, sc->wi_intrhand);
	wi_free(dev);
	sc->wi_gone = 1;

	WI_UNLOCK(sc);
	mtx_destroy(&sc->wi_mtx);

	return(0);
}

int
wi_generic_attach(device_t dev)
{
	struct wi_softc		*sc;
	struct wi_ltv_macaddr	mac;
	struct wi_ltv_gen	gen;
	struct ifnet		*ifp;
	int			error;

	/* XXX maybe we need the splimp stuff here XXX */
	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
	    wi_intr, sc, &sc->wi_intrhand);

	if (error) {
		device_printf(dev, "bus_setup_intr() failed! (%d)\n", error);
		wi_free(dev);
		return (error);
	}

	mtx_init(&sc->wi_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	WI_LOCK(sc);

	/* Reset the NIC. */
	wi_reset(sc);

	/*
	 * Read the station address.
	 * And do it twice. I've seen PRISM-based cards that return
	 * an error when trying to read it the first time, which causes
	 * the probe to fail.
	 */
	mac.wi_type = WI_RID_MAC_NODE;
	mac.wi_len = 4;
	wi_read_record(sc, (struct wi_ltv_gen *)&mac);
	if ((error = wi_read_record(sc, (struct wi_ltv_gen *)&mac)) != 0) {
		device_printf(dev, "mac read failed %d\n", error);
		wi_free(dev);
		return (error);
	}
	bcopy((char *)&mac.wi_mac_addr,
	   (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	device_printf(dev, "802.11 address: %6D\n", sc->arpcom.ac_enaddr, ":");

	wi_get_id(sc);

	ifp->if_softc = sc;
	ifp->if_unit = sc->wi_unit;
	ifp->if_name = "wi";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wi_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = wi_start;
	ifp->if_watchdog = wi_watchdog;
	ifp->if_init = wi_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
	bcopy(WI_DEFAULT_NODENAME, sc->wi_node_name,
	    sizeof(WI_DEFAULT_NODENAME) - 1);

	bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
	bcopy(WI_DEFAULT_NETNAME, sc->wi_net_name,
	    sizeof(WI_DEFAULT_NETNAME) - 1);

	bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
	bcopy(WI_DEFAULT_IBSS, sc->wi_ibss_name,
	    sizeof(WI_DEFAULT_IBSS) - 1);

	sc->wi_portnum = WI_DEFAULT_PORT;
	sc->wi_ptype = WI_PORTTYPE_BSS;
	sc->wi_ap_density = WI_DEFAULT_AP_DENSITY;
	sc->wi_rts_thresh = WI_DEFAULT_RTS_THRESH;
	sc->wi_tx_rate = WI_DEFAULT_TX_RATE;
	sc->wi_max_data_len = WI_DEFAULT_DATALEN;
	sc->wi_create_ibss = WI_DEFAULT_CREATE_IBSS;
	sc->wi_pm_enabled = WI_DEFAULT_PM_ENABLED;
	sc->wi_max_sleep = WI_DEFAULT_MAX_SLEEP;
	sc->wi_roaming = WI_DEFAULT_ROAMING;
	sc->wi_authtype = WI_DEFAULT_AUTHTYPE;
	sc->wi_authmode = IEEE80211_AUTH_OPEN;

	/*
	 * Read the default channel from the NIC. This may vary
	 * depending on the country where the NIC was purchased, so
	 * we can't hard-code a default and expect it to work for
	 * everyone.
	 */
	gen.wi_type = WI_RID_OWN_CHNL;
	gen.wi_len = 2;
	wi_read_record(sc, &gen);
	sc->wi_channel = gen.wi_val;

	/*
	 * Find out if we support WEP on this card.
	 */
	gen.wi_type = WI_RID_WEP_AVAIL;
	gen.wi_len = 2;
	wi_read_record(sc, &gen);
	sc->wi_has_wep = gen.wi_val;

	if (bootverbose)
		device_printf(sc->dev, "wi_has_wep = %d\n", sc->wi_has_wep);

	/* 
	 * Find supported rates.
	 */
	gen.wi_type = WI_RID_TX_RATE;
	gen.wi_len = 2;
	wi_read_record(sc, &gen);
	sc->wi_supprates = gen.wi_val;

	bzero((char *)&sc->wi_stats, sizeof(sc->wi_stats));

	wi_init(sc);
	wi_stop(sc);

	ifmedia_init(&sc->ifmedia, 0, wi_media_change, wi_media_status);
	/* XXX: Should read from card capabilities */
#define ADD(m, c)       ifmedia_add(&sc->ifmedia, (m), (c), NULL)
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1,
	    IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1, 0, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2,
	    IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2, 0, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5,
	    IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5, 0, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11,
	    IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11, 0, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 
		IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 0, 0), 0);
	if (sc->sc_firmware_type == WI_INTERSIL) {
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1,
		    IFM_IEEE80211_HOSTAP, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2,
		    IFM_IEEE80211_HOSTAP, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5,
		    IFM_IEEE80211_HOSTAP, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11,
		    IFM_IEEE80211_HOSTAP, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 
		    IFM_IEEE80211_HOSTAP, 0), 0);
	}
#undef	ADD
	ifmedia_set(&sc->ifmedia, IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO,
	    0, 0));


	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	callout_handle_init(&sc->wi_stat_ch);
	WI_UNLOCK(sc);

	return(0);
}

static void
wi_get_id(sc)
	struct wi_softc *sc;
{
	struct wi_ltv_ver       ver;
	struct wi_card_ident	*id;

	/* getting chip identity */
	memset(&ver, 0, sizeof(ver));
	ver.wi_type = WI_RID_CARD_ID;
	ver.wi_len = 5;
	wi_read_record(sc, (struct wi_ltv_gen *)&ver);
	device_printf(sc->dev, "using ");
	sc->sc_firmware_type = WI_NOTYPE;
	for (id = wi_card_ident; id->card_name != NULL; id++) {
		if (le16toh(ver.wi_ver[0]) == id->card_id) {
			printf("%s", id->card_name);
			sc->sc_firmware_type = id->firm_type;
			break;
		}
	}
	if (sc->sc_firmware_type == WI_NOTYPE) {
		if (le16toh(ver.wi_ver[0]) & 0x8000) {
			printf("Unknown PRISM2 chip");
			sc->sc_firmware_type = WI_INTERSIL;
		} else {
			printf("Unknown Lucent chip");
			sc->sc_firmware_type = WI_LUCENT;
		}
	}

	if (sc->sc_firmware_type != WI_LUCENT) {
		/* get primary firmware version */
		memset(&ver, 0, sizeof(ver));
		ver.wi_type = WI_RID_PRI_IDENTITY;
		ver.wi_len = 5;
		wi_read_record(sc, (struct wi_ltv_gen *)&ver);
		ver.wi_ver[1] = le16toh(ver.wi_ver[1]);
		ver.wi_ver[2] = le16toh(ver.wi_ver[2]);
		ver.wi_ver[3] = le16toh(ver.wi_ver[3]);
		sc->sc_pri_firmware_ver = ver.wi_ver[2] * 10000 +
		    ver.wi_ver[3] * 100 + ver.wi_ver[1];
	}

	/* get station firmware version */
	memset(&ver, 0, sizeof(ver));
	ver.wi_type = WI_RID_STA_IDENTITY;
	ver.wi_len = 5;
	wi_read_record(sc, (struct wi_ltv_gen *)&ver);
	ver.wi_ver[1] = le16toh(ver.wi_ver[1]);
	ver.wi_ver[2] = le16toh(ver.wi_ver[2]);
	ver.wi_ver[3] = le16toh(ver.wi_ver[3]);
	sc->sc_sta_firmware_ver = ver.wi_ver[2] * 10000 +
	    ver.wi_ver[3] * 100 + ver.wi_ver[1];
	if (sc->sc_firmware_type == WI_INTERSIL &&
	    (sc->sc_sta_firmware_ver == 10102 || 
	     sc->sc_sta_firmware_ver == 20102)) {
		struct wi_ltv_str sver;
		char *p;

		memset(&sver, 0, sizeof(sver));
		sver.wi_type = WI_RID_SYMBOL_IDENTITY;
		sver.wi_len = 7;
		/* value should be "V2.00-11" */
		if (wi_read_record(sc, (struct wi_ltv_gen *)&sver) == 0 &&
		    *(p = (char *)sver.wi_str) == 'V' &&
		    p[2] == '.' && p[5] == '-' && p[8] == '\0') {
			sc->sc_firmware_type = WI_SYMBOL;
			sc->sc_sta_firmware_ver = (p[1] - '0') * 10000 +
			    (p[3] - '0') * 1000 + (p[4] - '0') * 100 +
			    (p[6] - '0') * 10 + (p[7] - '0');
		}
	}
	printf("\n");
	device_printf(sc->dev, "%s Firmware: ",
	     sc->sc_firmware_type == WI_LUCENT ? "Lucent" :
	    (sc->sc_firmware_type == WI_SYMBOL ? "Symbol" : "Intersil"));

	/*
	 * The primary firmware is only valid on Prism based chipsets
	 * (INTERSIL or SYMBOL).
	 */
	if (sc->sc_firmware_type != WI_LUCENT)
	    printf("Primary %u.%02u.%02u, ", sc->sc_pri_firmware_ver / 10000,
		    (sc->sc_pri_firmware_ver % 10000) / 100,
		    sc->sc_pri_firmware_ver % 100);
	printf("Station %u.%02u.%02u\n",
	    sc->sc_sta_firmware_ver / 10000, (sc->sc_sta_firmware_ver % 10000) / 100,
	    sc->sc_sta_firmware_ver % 100);
	return;
}

static void
wi_rxeof(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	struct ether_header	*eh;
	struct mbuf		*m;
	int			id;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_RX_FID);

	/*
	 * if we have the procframe flag set, disregard all this and just
	 * read the data from the device.
	 */
	if (sc->wi_procframe || sc->wi_debug.wi_monitor) {
		struct wi_frame		*rx_frame;
		int			datlen, hdrlen;

		/* first allocate mbuf for packet storage */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			ifp->if_ierrors++;
			return;
		}
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		m->m_pkthdr.rcvif = ifp;

		/* now read wi_frame first so we know how much data to read */
		if (wi_read_data(sc, id, 0, mtod(m, caddr_t),
		    sizeof(struct wi_frame))) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		rx_frame = mtod(m, struct wi_frame *);

		switch ((rx_frame->wi_status & WI_STAT_MAC_PORT) >> 8) {
		case 7:
			switch (rx_frame->wi_frame_ctl & WI_FCTL_FTYPE) {
			case WI_FTYPE_DATA:
				hdrlen = WI_DATA_HDRLEN;
				datlen = rx_frame->wi_dat_len + WI_FCS_LEN;
				break;
			case WI_FTYPE_MGMT:
				hdrlen = WI_MGMT_HDRLEN;
				datlen = rx_frame->wi_dat_len + WI_FCS_LEN;
				break;
			case WI_FTYPE_CTL:
				/*
				 * prism2 cards don't pass control packets
				 * down properly or consistently, so we'll only
				 * pass down the header.
				 */
				hdrlen = WI_CTL_HDRLEN;
				datlen = 0;
				break;
			default:
				device_printf(sc->dev, "received packet of "
				    "unknown type on port 7\n");
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			break;
		case 0:
			hdrlen = WI_DATA_HDRLEN;
			datlen = rx_frame->wi_dat_len + WI_FCS_LEN;
			break;
		default:
			device_printf(sc->dev, "received packet on invalid "
			    "port (wi_status=0x%x)\n", rx_frame->wi_status);
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		if ((hdrlen + datlen + 2) > MCLBYTES) {
			device_printf(sc->dev, "oversized packet received "
			    "(wi_dat_len=%d, wi_status=0x%x)\n",
			    datlen, rx_frame->wi_status);
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		if (wi_read_data(sc, id, hdrlen, mtod(m, caddr_t) + hdrlen,
		    datlen + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		m->m_pkthdr.len = m->m_len = hdrlen + datlen;

		ifp->if_ipackets++;

		/* Handle BPF listeners. */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m);

		m_freem(m);
	} else {
		struct wi_frame		rx_frame;

		/* First read in the frame header */
		if (wi_read_data(sc, id, 0, (caddr_t)&rx_frame,
		    sizeof(rx_frame))) {
			ifp->if_ierrors++;
			return;
		}

		if (rx_frame.wi_status & WI_STAT_ERRSTAT) {
			ifp->if_ierrors++;
			return;
		}

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			ifp->if_ierrors++;
			return;
		}
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}

		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;

		if (rx_frame.wi_status == WI_STAT_MGMT &&
		    sc->wi_ptype == WI_PORTTYPE_AP) {
			if ((WI_802_11_OFFSET_RAW + rx_frame.wi_dat_len + 2) >
			    MCLBYTES) {
				device_printf(sc->dev, "oversized mgmt packet "
				    "received in hostap mode " 
				    "(wi_dat_len=%d, wi_status=0x%x)\n",
				    rx_frame.wi_dat_len, rx_frame.wi_status);
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}

			/* Put the whole header in there. */
			bcopy(&rx_frame, mtod(m, void *),
			    sizeof(struct wi_frame));
			if (wi_read_data(sc, id, WI_802_11_OFFSET_RAW, 
			    mtod(m, caddr_t) + WI_802_11_OFFSET_RAW,
			    rx_frame.wi_dat_len + 2)) {
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			m->m_pkthdr.len = m->m_len =
			    WI_802_11_OFFSET_RAW + rx_frame.wi_dat_len;
			/* XXX: consider giving packet to bhp? */
			wihap_mgmt_input(sc, &rx_frame, m);
			return;
		}

		if (rx_frame.wi_status == WI_STAT_1042 ||
		    rx_frame.wi_status == WI_STAT_TUNNEL ||
		    rx_frame.wi_status == WI_STAT_WMP_MSG) {
			if((rx_frame.wi_dat_len + WI_SNAPHDR_LEN) > MCLBYTES) {
				device_printf(sc->dev,
				    "oversized packet received "
				    "(wi_dat_len=%d, wi_status=0x%x)\n",
				    rx_frame.wi_dat_len, rx_frame.wi_status);
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			m->m_pkthdr.len = m->m_len =
			    rx_frame.wi_dat_len + WI_SNAPHDR_LEN;

#if 0
			bcopy((char *)&rx_frame.wi_addr1,
			    (char *)&eh->ether_dhost, ETHER_ADDR_LEN);
			if (sc->wi_ptype == WI_PORTTYPE_ADHOC) {
				bcopy((char *)&rx_frame.wi_addr2,
				    (char *)&eh->ether_shost, ETHER_ADDR_LEN);
			} else {
				bcopy((char *)&rx_frame.wi_addr3,
				    (char *)&eh->ether_shost, ETHER_ADDR_LEN);
			}
#else
			bcopy((char *)&rx_frame.wi_dst_addr,
				(char *)&eh->ether_dhost, ETHER_ADDR_LEN);
			bcopy((char *)&rx_frame.wi_src_addr,
				(char *)&eh->ether_shost, ETHER_ADDR_LEN);
#endif

			bcopy((char *)&rx_frame.wi_type,
			    (char *)&eh->ether_type, ETHER_TYPE_LEN);

			if (wi_read_data(sc, id, WI_802_11_OFFSET,
			    mtod(m, caddr_t) + sizeof(struct ether_header),
			    m->m_len + 2)) {
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
		} else {
			if((rx_frame.wi_dat_len +
			    sizeof(struct ether_header)) > MCLBYTES) {
				device_printf(sc->dev,
				    "oversized packet received "
				    "(wi_dat_len=%d, wi_status=0x%x)\n",
				    rx_frame.wi_dat_len, rx_frame.wi_status);
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
			m->m_pkthdr.len = m->m_len =
			    rx_frame.wi_dat_len + sizeof(struct ether_header);

			if (wi_read_data(sc, id, WI_802_3_OFFSET,
			    mtod(m, caddr_t), m->m_len + 2)) {
				m_freem(m);
				ifp->if_ierrors++;
				return;
			}
		}

		ifp->if_ipackets++;

		if (sc->wi_ptype == WI_PORTTYPE_AP) {
			/* 
			 * Give host AP code first crack at data
			 * packets.  If it decides to handle it (or
			 * drop it), it will return a non-zero.
			 * Otherwise, it is destined for this host.
			 */
			if (wihap_data_input(sc, &rx_frame, m))
				return;
		}
		/* Receive packet. */
		m_adj(m, sizeof(struct ether_header));
#ifdef WICACHE
		wi_cache_store(sc, eh, m, rx_frame.wi_q_info);
#endif  
		ether_input(ifp, eh, m);
	}
}

static void
wi_txeof(sc, status)
	struct wi_softc		*sc;
	int			status;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status & WI_EV_TX_EXC)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	return;
}

void
wi_inquire(xsc)
	void			*xsc;
{
	struct wi_softc		*sc;
	struct ifnet		*ifp;
	int s;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	sc->wi_stat_ch = timeout(wi_inquire, sc, hz * 60);

	/* Don't do this while we're transmitting */
	if (ifp->if_flags & IFF_OACTIVE)
		return;

	s = splimp();
	wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_COUNTERS, 0, 0);
	splx(s);

	return;
}

void
wi_update_stats(sc)
	struct wi_softc		*sc;
{
	struct wi_ltv_gen	gen;
	u_int16_t		id;
	struct ifnet		*ifp;
	u_int32_t		*ptr;
	int			len, i;
	u_int16_t		t;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_INFO_FID);

	wi_read_data(sc, id, 0, (char *)&gen, 4);

	/*
	 * if we just got our scan results, copy it over into the scan buffer
	 * so we can return it to anyone that asks for it. (add a little
	 * compatibility with the prism2 scanning mechanism)
	 */
	if (gen.wi_type == WI_INFO_SCAN_RESULTS)
	{
		sc->wi_scanbuf_len = gen.wi_len;
		wi_read_data(sc, id, 4, (char *)sc->wi_scanbuf,
		    sc->wi_scanbuf_len * 2);

		return;
	}
	else if (gen.wi_type != WI_INFO_COUNTERS)
		return;

	len = (gen.wi_len - 1 < sizeof(sc->wi_stats) / 4) ?
		gen.wi_len - 1 : sizeof(sc->wi_stats) / 4;
	ptr = (u_int32_t *)&sc->wi_stats;

	for (i = 0; i < len - 1; i++) {
		t = CSR_READ_2(sc, WI_DATA1);
#ifdef WI_HERMES_STATS_WAR
		if (t > 0xF000)
			t = ~t & 0xFFFF;
#endif
		ptr[i] += t;
	}

	ifp->if_collisions = sc->wi_stats.wi_tx_single_retries +
	    sc->wi_stats.wi_tx_multi_retries +
	    sc->wi_stats.wi_tx_retry_limit;

	return;
}

static void
wi_intr(xsc)
	void		*xsc;
{
	struct wi_softc		*sc = xsc;
	struct ifnet		*ifp;
	u_int16_t		status;

	WI_LOCK(sc);

	ifp = &sc->arpcom.ac_if;

	if (sc->wi_gone || !(ifp->if_flags & IFF_UP)) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		WI_UNLOCK(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);

	status = CSR_READ_2(sc, WI_EVENT_STAT);
	CSR_WRITE_2(sc, WI_EVENT_ACK, ~WI_INTRS);

	if (status & WI_EV_RX) {
		wi_rxeof(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
	}

	if (status & WI_EV_TX) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX);
	}

	if (status & WI_EV_ALLOC) {
		int			id;

		id = CSR_READ_2(sc, WI_ALLOC_FID);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
		if (id == sc->wi_tx_data_id)
			wi_txeof(sc, status);
	}

	if (status & WI_EV_INFO) {
		wi_update_stats(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO);
	}

	if (status & WI_EV_TX_EXC) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX_EXC);
	}

	if (status & WI_EV_INFO_DROP) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO_DROP);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		wi_start(ifp);
	}

	WI_UNLOCK(sc);

	return;
}

static int
wi_cmd(sc, cmd, val0, val1, val2)
	struct wi_softc		*sc;
	int			cmd;
	int			val0;
	int			val1;
	int			val2;
{
	int			i, s = 0;

	/* wait for the busy bit to clear */
	for (i = 500; i > 0; i--) {	/* 5s */
		if (!(CSR_READ_2(sc, WI_COMMAND) & WI_CMD_BUSY)) {
			break;
		}
		DELAY(10*1000);	/* 10 m sec */
	}
	if (i == 0) {
		device_printf(sc->dev, "wi_cmd: busy bit won't clear.\n" );
		return(ETIMEDOUT);
	}

	CSR_WRITE_2(sc, WI_PARAM0, val0);
	CSR_WRITE_2(sc, WI_PARAM1, val1);
	CSR_WRITE_2(sc, WI_PARAM2, val2);
	CSR_WRITE_2(sc, WI_COMMAND, cmd);

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
#ifdef foo
			if ((s & WI_CMD_CODE_MASK) != (cmd & WI_CMD_CODE_MASK))
				return(EIO);
#endif
			if (s & WI_STAT_CMD_RESULT)
				return(EIO);
			break;
		}
		DELAY(WI_DELAY);
	}

	if (i == WI_TIMEOUT) {
		device_printf(sc->dev,
		    "timeout in wi_cmd 0x%04x; event status 0x%04x\n", cmd, s);
		return(ETIMEDOUT);
	}

	return(0);
}

static void
wi_reset(sc)
	struct wi_softc		*sc;
{
#define WI_INIT_TRIES 3
	int i;
	int tries;
	
	/* Symbol firmware cannot be initialized more than once */
	if (sc->sc_firmware_type == WI_SYMBOL && sc->sc_enabled)
		return;
	if (sc->sc_firmware_type == WI_SYMBOL)
		tries = 1;
	else
		tries = WI_INIT_TRIES;

	for (i = 0; i < tries; i++) {
		if (wi_cmd(sc, WI_CMD_INI, 0, 0, 0) == 0)
			break;
		DELAY(WI_DELAY * 1000);
	}
	sc->sc_enabled = 1;

	if (i == tries) {
		device_printf(sc->dev, "init failed\n");
		return;
	}

	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Calibrate timer. */
	WI_SETVAL(WI_RID_TICK_TIME, 8);

	return;
}

/*
 * Read an LTV record from the NIC.
 */
static int
wi_read_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i, len, code;
	struct wi_ltv_gen	*oltv, p2ltv;

	oltv = ltv;
	if (sc->sc_firmware_type != WI_LUCENT) {
		switch (ltv->wi_type) {
		case WI_RID_ENCRYPTION:
			p2ltv.wi_type = WI_RID_P2_ENCRYPTION;
			p2ltv.wi_len = 2;
			ltv = &p2ltv;
			break;
		case WI_RID_TX_CRYPT_KEY:
			p2ltv.wi_type = WI_RID_P2_TX_CRYPT_KEY;
			p2ltv.wi_len = 2;
			ltv = &p2ltv;
			break;
		}
	}

	/* Tell the NIC to enter record read mode. */
	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_READ, ltv->wi_type, 0, 0))
		return(EIO);

	/* Seek to the record. */
	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	/*
	 * Read the length and record type and make sure they
	 * match what we expect (this verifies that we have enough
	 * room to hold all of the returned data).
	 */
	len = CSR_READ_2(sc, WI_DATA1);
	if (len > ltv->wi_len)
		return(ENOSPC);
	code = CSR_READ_2(sc, WI_DATA1);
	if (code != ltv->wi_type)
		return(EIO);

	ltv->wi_len = len;
	ltv->wi_type = code;

	/* Now read the data. */
	ptr = &ltv->wi_val;
	for (i = 0; i < ltv->wi_len - 1; i++)
		ptr[i] = CSR_READ_2(sc, WI_DATA1);

	if (sc->sc_firmware_type != WI_LUCENT) {
		switch (oltv->wi_type) {
		case WI_RID_TX_RATE:
		case WI_RID_CUR_TX_RATE:
			switch (ltv->wi_val) {
			case 1: oltv->wi_val = 1; break;
			case 2: oltv->wi_val = 2; break;
			case 3:	oltv->wi_val = 6; break;
			case 4: oltv->wi_val = 5; break;
			case 7: oltv->wi_val = 7; break;
			case 8: oltv->wi_val = 11; break;
			case 15: oltv->wi_val = 3; break;
			default: oltv->wi_val = 0x100 + ltv->wi_val; break;
			}
			break;
		case WI_RID_ENCRYPTION:
			oltv->wi_len = 2;
			if (ltv->wi_val & 0x01)
				oltv->wi_val = 1;
			else
				oltv->wi_val = 0;
			break;
		case WI_RID_TX_CRYPT_KEY:
			oltv->wi_len = 2;
			oltv->wi_val = ltv->wi_val;
			break;
		case WI_RID_CNFAUTHMODE:
                        oltv->wi_len = 2;
			if (le16toh(ltv->wi_val) & 0x01)
				oltv->wi_val = htole16(1);
			else if (le16toh(ltv->wi_val) & 0x02)
				oltv->wi_val = htole16(2);
			break;
		}
	}

	return(0);
}

/*
 * Same as read, except we inject data instead of reading it.
 */
static int
wi_write_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i;
	struct wi_ltv_gen	p2ltv;

	if (sc->sc_firmware_type != WI_LUCENT) {
		switch (ltv->wi_type) {
		case WI_RID_TX_RATE:
			p2ltv.wi_type = WI_RID_TX_RATE;
			p2ltv.wi_len = 2;
			switch (ltv->wi_val) {
			case 1: p2ltv.wi_val = 1; break;
			case 2: p2ltv.wi_val = 2; break;
			case 3:	p2ltv.wi_val = 15; break;
			case 5: p2ltv.wi_val = 4; break;
			case 6: p2ltv.wi_val = 3; break;
			case 7: p2ltv.wi_val = 7; break;
			case 11: p2ltv.wi_val = 8; break;
			default: return EINVAL;
			}
			ltv = &p2ltv;
			break;
		case WI_RID_ENCRYPTION:
			p2ltv.wi_type = WI_RID_P2_ENCRYPTION;
			p2ltv.wi_len = 2;
			if (le16toh(ltv->wi_val)) {
				p2ltv.wi_val =htole16(PRIVACY_INVOKED |
				    EXCLUDE_UNENCRYPTED);
				if (sc->wi_ptype == WI_PORTTYPE_AP)
					/* 
					 * Disable tx encryption...
					 * it's broken.
					 */
					p2ltv.wi_val |= htole16(HOST_ENCRYPT);
			} else
				p2ltv.wi_val =
				    htole16(HOST_ENCRYPT | HOST_DECRYPT);
			ltv = &p2ltv;
			break;
		case WI_RID_TX_CRYPT_KEY:
			p2ltv.wi_type = WI_RID_P2_TX_CRYPT_KEY;
			p2ltv.wi_len = 2;
			p2ltv.wi_val = ltv->wi_val;
			ltv = &p2ltv;
			break;
		case WI_RID_DEFLT_CRYPT_KEYS:
		    {
			int error;
			int keylen;
			struct wi_ltv_str	ws;
			struct wi_ltv_keys	*wk =
			    (struct wi_ltv_keys *)ltv;

			keylen = wk->wi_keys[sc->wi_tx_key].wi_keylen;

			for (i = 0; i < 4; i++) {
				bzero(&ws, sizeof(ws));
				ws.wi_len = (keylen > 5) ? 8 : 4;
				ws.wi_type = WI_RID_P2_CRYPT_KEY0 + i;
				memcpy(ws.wi_str,
				    &wk->wi_keys[i].wi_keydat, keylen);
				error = wi_write_record(sc,
				    (struct wi_ltv_gen *)&ws);
				if (error)
					return error;
			}
			return 0;
		    }
		case WI_RID_CNFAUTHMODE:
			p2ltv.wi_type = WI_RID_CNFAUTHMODE;
			p2ltv.wi_len = 2;
			if (le16toh(ltv->wi_val) == 1)
				p2ltv.wi_val = htole16(0x01);
			else if (le16toh(ltv->wi_val) == 2)
				p2ltv.wi_val = htole16(0x02);
			ltv = &p2ltv;
			break;
		}
	} else {
		/* LUCENT */
		switch (ltv->wi_type) {  
		case WI_RID_TX_RATE:
			switch (ltv->wi_val) {
			case 1: ltv->wi_val = 1; break;  /* 1Mb/s fixed */
			case 2: ltv->wi_val = 2; break;  /* 2Mb/s fixed */
			case 3: ltv->wi_val = 3; break;  /* 11Mb/s auto */
			case 5: ltv->wi_val = 4; break;  /* 5.5Mb/s fixed */
			case 6: ltv->wi_val = 6; break;  /* 2Mb/s auto */
			case 7: ltv->wi_val = 7; break;  /* 5.5Mb/s auto */
			case 11: ltv->wi_val = 5; break; /* 11Mb/s fixed */
			default: return EINVAL;
			}
		}
	}

	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_len);
	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_type);

	ptr = &ltv->wi_val;
	for (i = 0; i < ltv->wi_len - 1; i++)
		CSR_WRITE_2(sc, WI_DATA1, ptr[i]);

	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_WRITE, ltv->wi_type, 0, 0))
		return(EIO);

	return(0);
}

static int
wi_seek(sc, id, off, chan)
	struct wi_softc		*sc;
	int			id, off, chan;
{
	int			i;
	int			selreg, offreg;
	int			status;

	switch (chan) {
	case WI_BAP0:
		selreg = WI_SEL0;
		offreg = WI_OFF0;
		break;
	case WI_BAP1:
		selreg = WI_SEL1;
		offreg = WI_OFF1;
		break;
	default:
		device_printf(sc->dev, "invalid data path: %x\n", chan);
		return(EIO);
	}

	CSR_WRITE_2(sc, selreg, id);
	CSR_WRITE_2(sc, offreg, off);

	for (i = 0; i < WI_TIMEOUT; i++) {
		status = CSR_READ_2(sc, offreg);
		if (!(status & (WI_OFF_BUSY|WI_OFF_ERR)))
			break;
		DELAY(WI_DELAY);
	}

	if (i == WI_TIMEOUT) {
		device_printf(sc->dev, "timeout in wi_seek to %x/%x; last status %x\n",
			id, off, status);
		return(ETIMEDOUT);
	}

	return(0);
}

static int
wi_read_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;

	if (wi_seek(sc, id, off, WI_BAP1))
		return(EIO);

	ptr = (u_int16_t *)buf;
	for (i = 0; i < len / 2; i++)
		ptr[i] = CSR_READ_2(sc, WI_DATA1);

	return(0);
}

/*
 * According to the comments in the HCF Light code, there is a bug in
 * the Hermes (or possibly in certain Hermes firmware revisions) where
 * the chip's internal autoincrement counter gets thrown off during
 * data writes: the autoincrement is missed, causing one data word to
 * be overwritten and subsequent words to be written to the wrong memory
 * locations. The end result is that we could end up transmitting bogus
 * frames without realizing it. The workaround for this is to write a
 * couple of extra guard words after the end of the transfer, then
 * attempt to read then back. If we fail to locate the guard words where
 * we expect them, we preform the transfer over again.
 */
static int
wi_write_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;
#ifdef WI_HERMES_AUTOINC_WAR
	int			retries;

	retries = 512;
again:
#endif

	if (wi_seek(sc, id, off, WI_BAP0))
		return(EIO);

	ptr = (u_int16_t *)buf;
	for (i = 0; i < (len / 2); i++)
		CSR_WRITE_2(sc, WI_DATA0, ptr[i]);

#ifdef WI_HERMES_AUTOINC_WAR
	CSR_WRITE_2(sc, WI_DATA0, 0x1234);
	CSR_WRITE_2(sc, WI_DATA0, 0x5678);

	if (wi_seek(sc, id, off + len, WI_BAP0))
		return(EIO);

	if (CSR_READ_2(sc, WI_DATA0) != 0x1234 ||
	    CSR_READ_2(sc, WI_DATA0) != 0x5678) {
		if (--retries >= 0)
			goto again;
		device_printf(sc->dev, "wi_write_data device timeout\n");
		return (EIO);
	}
#endif

	return(0);
}

/*
 * Allocate a region of memory inside the NIC and zero
 * it out.
 */
static int
wi_alloc_nicmem(sc, len, id)
	struct wi_softc		*sc;
	int			len;
	int			*id;
{
	int			i;

	if (wi_cmd(sc, WI_CMD_ALLOC_MEM, len, 0, 0)) {
		device_printf(sc->dev,
		    "failed to allocate %d bytes on NIC\n", len);
		return(ENOMEM);
	}

	for (i = 0; i < WI_TIMEOUT; i++) {
		if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_ALLOC)
			break;
		DELAY(WI_DELAY);
	}

	if (i == WI_TIMEOUT) {
		device_printf(sc->dev, "time out allocating memory on card\n");
		return(ETIMEDOUT);
	}

	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
	*id = CSR_READ_2(sc, WI_ALLOC_FID);

	if (wi_seek(sc, *id, 0, WI_BAP0)) {
		device_printf(sc->dev, "seek failed while allocating memory on card\n");
		return(EIO);
	}

	for (i = 0; i < len / 2; i++)
		CSR_WRITE_2(sc, WI_DATA0, 0);

	return(0);
}

static void
wi_setmulti(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	int			i = 0;
	struct ifmultiaddr	*ifma;
	struct wi_ltv_mcast	mcast;

	ifp = &sc->arpcom.ac_if;

	bzero((char *)&mcast, sizeof(mcast));

	mcast.wi_type = WI_RID_MCAST_LIST;
	mcast.wi_len = (3 * 16) + 1;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		wi_write_record(sc, (struct wi_ltv_gen *)&mcast);
		return;
	}

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (i < 16) {
			bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
			    (char *)&mcast.wi_mcast[i], ETHER_ADDR_LEN);
			i++;
		} else {
			bzero((char *)&mcast, sizeof(mcast));
			break;
		}
	}

	mcast.wi_len = (i * 3) + 1;
	wi_write_record(sc, (struct wi_ltv_gen *)&mcast);

	return;
}

static void
wi_setdef(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	struct sockaddr_dl	*sdl;
	struct ifaddr		*ifa;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	switch(wreq->wi_type) {
	case WI_RID_MAC_NODE:
		ifa = ifaddr_byindex(ifp->if_index);
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		bcopy((char *)&wreq->wi_val, (char *)&sc->arpcom.ac_enaddr,
		   ETHER_ADDR_LEN);
		bcopy((char *)&wreq->wi_val, LLADDR(sdl), ETHER_ADDR_LEN);
		break;
	case WI_RID_PORTTYPE:
		sc->wi_ptype = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_TX_RATE:
		sc->wi_tx_rate = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_MAX_DATALEN:
		sc->wi_max_data_len = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_RTS_THRESH:
		sc->wi_rts_thresh = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_SYSTEM_SCALE:
		sc->wi_ap_density = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_CREATE_IBSS:
		sc->wi_create_ibss = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_OWN_CHNL:
		sc->wi_channel = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_NODENAME:
		bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_node_name, 30);
		break;
	case WI_RID_DESIRED_SSID:
		bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_net_name, 30);
		break;
	case WI_RID_OWN_SSID:
		bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_ibss_name, 30);
		break;
	case WI_RID_PM_ENABLED:
		sc->wi_pm_enabled = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_MICROWAVE_OVEN:
		sc->wi_mor_enabled = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_MAX_SLEEP:
		sc->wi_max_sleep = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_CNFAUTHMODE:
		sc->wi_authtype = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_ROAMING_MODE:
		sc->wi_roaming = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_ENCRYPTION:
		sc->wi_use_wep = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_TX_CRYPT_KEY:
		sc->wi_tx_key = le16toh(wreq->wi_val[0]);
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		bcopy((char *)wreq, (char *)&sc->wi_keys,
		    sizeof(struct wi_ltv_keys));
		break;
	default:
		break;
	}

	/* Reinitialize WaveLAN. */
	wi_init(sc);

	return;
}

static int
wi_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	int			error = 0;
	int			len;
	u_int8_t		tmpkey[14];
	char			tmpssid[IEEE80211_NWID_LEN];
	struct wi_softc		*sc;
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct ieee80211req	*ireq;
	struct thread		*td = curthread;

	sc = ifp->if_softc;
	WI_LOCK(sc);
	ifr = (struct ifreq *)data;
	ireq = (struct ieee80211req *)data;

	if (sc->wi_gone) {
		error = ENODEV;
		goto out;
	}

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->wi_if_flags & IFF_PROMISC)) {
				WI_SETVAL(WI_RID_PROMISC, 1);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->wi_if_flags & IFF_PROMISC) {
				WI_SETVAL(WI_RID_PROMISC, 0);
			} else
				wi_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				wi_stop(sc);
			}
		}
		sc->wi_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		wi_setmulti(sc);
		error = 0;
		break;
	case SIOCGWAVELAN:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_len > WI_MAX_DATALEN) {
			error = EINVAL;
			break;
		}
		/* Don't show WEP keys to non-root users. */
		if (wreq.wi_type == WI_RID_DEFLT_CRYPT_KEYS && suser(td))
			break;
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			bcopy((char *)&sc->wi_stats, (char *)&wreq.wi_val,
			    sizeof(sc->wi_stats));
			wreq.wi_len = (sizeof(sc->wi_stats) / 2) + 1;
		} else if (wreq.wi_type == WI_RID_DEFLT_CRYPT_KEYS) {
			bcopy((char *)&sc->wi_keys, (char *)&wreq,
			    sizeof(struct wi_ltv_keys));
		}
#ifdef WICACHE
		else if (wreq.wi_type == WI_RID_ZERO_CACHE) {
			sc->wi_sigitems = sc->wi_nextitem = 0;
		} else if (wreq.wi_type == WI_RID_READ_CACHE) {
			char *pt = (char *)&wreq.wi_val;
			bcopy((char *)&sc->wi_sigitems,
			    (char *)pt, sizeof(int));
			pt += (sizeof (int));
			wreq.wi_len = sizeof(int) / 2;
			bcopy((char *)&sc->wi_sigcache, (char *)pt,
			    sizeof(struct wi_sigcache) * sc->wi_sigitems);
			wreq.wi_len += ((sizeof(struct wi_sigcache) *
			    sc->wi_sigitems) / 2) + 1;
		}
#endif
		else if (wreq.wi_type == WI_RID_PROCFRAME) {
			wreq.wi_len = 2;
			wreq.wi_val[0] = sc->wi_procframe;
		} else if (wreq.wi_type == WI_RID_PRISM2) {
			wreq.wi_len = 2;
			wreq.wi_val[0] = sc->sc_firmware_type != WI_LUCENT;
		} else if (wreq.wi_type == WI_RID_SCAN_RES && 
		    sc->sc_firmware_type == WI_LUCENT) {
			memcpy((char *)wreq.wi_val, (char *)sc->wi_scanbuf,
			    sc->wi_scanbuf_len * 2);
			wreq.wi_len = sc->wi_scanbuf_len;
		} else {
			if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq)) {
				error = EINVAL;
				break;
			}
		}
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	case SIOCSWAVELAN:
		if ((error = suser(td)))
			goto out;
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_len > WI_MAX_DATALEN) {
			error = EINVAL;
			break;
		}
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			error = EINVAL;
			break;
		} else if (wreq.wi_type == WI_RID_MGMT_XMIT) {
			error = wi_mgmt_xmit(sc, (caddr_t)&wreq.wi_val,
			    wreq.wi_len);
		} else if (wreq.wi_type == WI_RID_PROCFRAME) {
			sc->wi_procframe = wreq.wi_val[0];
		/*
		 * if we're getting a scan request from a wavelan card
		 * (non-prism2), send out a cmd_inquire to the card to scan
		 * results for the scan will be received through the info
		 * interrupt handler. otherwise the scan request can be
		 * directly handled by a prism2 card's rid interface.
		 */
		} else if (wreq.wi_type == WI_RID_SCAN_REQ && 
		    sc->sc_firmware_type == WI_LUCENT) {
			wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_SCAN_RESULTS, 0, 0);
		} else {
			error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
			if (!error)
				wi_setdef(sc, &wreq);
		}
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
		switch(ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if(ireq->i_val == -1) {
				bzero(tmpssid, IEEE80211_NWID_LEN);
				error = wi_get_cur_ssid(sc, tmpssid, &len);
				if (error != 0)
					break;
				error = copyout(tmpssid, ireq->i_data,
					IEEE80211_NWID_LEN);
				ireq->i_len = len;
			} else if (ireq->i_val == 0) {
				error = copyout(sc->wi_net_name,
				    ireq->i_data,
				    IEEE80211_NWID_LEN);
				ireq->i_len = IEEE80211_NWID_LEN;
			} else
				error = EINVAL;
			break;
		case IEEE80211_IOC_NUMSSIDS:
			ireq->i_val = 1;
			break;
		case IEEE80211_IOC_WEP:
			if(!sc->wi_has_wep) {
				ireq->i_val = IEEE80211_WEP_NOSUP; 
			} else {
				if(sc->wi_use_wep) {
					ireq->i_val =
					    IEEE80211_WEP_MIXED;
				} else {
					ireq->i_val =
					    IEEE80211_WEP_OFF;
				}
			}
			break;
		case IEEE80211_IOC_WEPKEY:
			if(!sc->wi_has_wep ||
			    ireq->i_val < 0 || ireq->i_val > 3) {
				error = EINVAL;
				break;
			}
			len = sc->wi_keys.wi_keys[ireq->i_val].wi_keylen;
			if (suser(td))
				bcopy(sc->wi_keys.wi_keys[ireq->i_val].wi_keydat,
				    tmpkey, len);
			else
				bzero(tmpkey, len);

			ireq->i_len = len;
			error = copyout(tmpkey, ireq->i_data, len);

			break;
		case IEEE80211_IOC_NUMWEPKEYS:
			if(!sc->wi_has_wep)
				error = EINVAL;
			else
				ireq->i_val = 4;
			break;
		case IEEE80211_IOC_WEPTXKEY:
			if(!sc->wi_has_wep)
				error = EINVAL;
			else
				ireq->i_val = sc->wi_tx_key;
			break;
		case IEEE80211_IOC_AUTHMODE:
			ireq->i_val = sc->wi_authmode;
			break;
		case IEEE80211_IOC_STATIONNAME:
			error = copyout(sc->wi_node_name,
			    ireq->i_data, IEEE80211_NWID_LEN);
			ireq->i_len = IEEE80211_NWID_LEN;
			break;
		case IEEE80211_IOC_CHANNEL:
			wreq.wi_type = WI_RID_CURRENT_CHAN;
			wreq.wi_len = WI_MAX_DATALEN;
			if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq))
				error = EINVAL;
			else {
				ireq->i_val = wreq.wi_val[0];
			}
			break;
		case IEEE80211_IOC_POWERSAVE:
			if(sc->wi_pm_enabled)
				ireq->i_val = IEEE80211_POWERSAVE_ON;
			else
				ireq->i_val = IEEE80211_POWERSAVE_OFF;
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			ireq->i_val = sc->wi_max_sleep;
			break;
		default:
			error = EINVAL;
		}
		break;
	case SIOCS80211:
		if ((error = suser(td)))
			goto out;
		switch(ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_val != 0 ||
			    ireq->i_len > IEEE80211_NWID_LEN) {
				error = EINVAL;
				break;
			}
			/* We set both of them */
			bzero(sc->wi_net_name, IEEE80211_NWID_LEN);
			error = copyin(ireq->i_data,
			    sc->wi_net_name, ireq->i_len);
			bcopy(sc->wi_net_name, sc->wi_ibss_name, IEEE80211_NWID_LEN);
			break;
		case IEEE80211_IOC_WEP:
			/*
			 * These cards only support one mode so
			 * we just turn wep on what ever is
			 * passed in if it's not OFF.
			 */
			if (ireq->i_val == IEEE80211_WEP_OFF) {
				sc->wi_use_wep = 0;
			} else {
				sc->wi_use_wep = 1;
			}
			break;
		case IEEE80211_IOC_WEPKEY:
			if (ireq->i_val < 0 || ireq->i_val > 3 ||
				ireq->i_len > 13) {
				error = EINVAL;
				break;
			} 
			bzero(sc->wi_keys.wi_keys[ireq->i_val].wi_keydat, 13);
			error = copyin(ireq->i_data, 
			    sc->wi_keys.wi_keys[ireq->i_val].wi_keydat,
			    ireq->i_len);
			if(error)
				break;
			sc->wi_keys.wi_keys[ireq->i_val].wi_keylen =
				    ireq->i_len;
			break;
		case IEEE80211_IOC_WEPTXKEY:
			if (ireq->i_val < 0 || ireq->i_val > 3) {
				error = EINVAL;
				break;
			}
			sc->wi_tx_key = ireq->i_val;
			break;
		case IEEE80211_IOC_AUTHMODE:
			sc->wi_authmode = ireq->i_val;
			break;
		case IEEE80211_IOC_STATIONNAME:
			if (ireq->i_len > 32) {
				error = EINVAL;
				break;
			}
			bzero(sc->wi_node_name, 32);
			error = copyin(ireq->i_data,
			    sc->wi_node_name, ireq->i_len);
			break;
		case IEEE80211_IOC_CHANNEL:
			/*
			 * The actual range is 1-14, but if you
			 * set it to 0 you get the default. So
			 * we let that work too.
			 */
			if (ireq->i_val < 0 || ireq->i_val > 14) {
				error = EINVAL;
				break;
			}
			sc->wi_channel = ireq->i_val;
			break;
		case IEEE80211_IOC_POWERSAVE:
			switch (ireq->i_val) {
			case IEEE80211_POWERSAVE_OFF:
				sc->wi_pm_enabled = 0;
				break;
			case IEEE80211_POWERSAVE_ON:
				sc->wi_pm_enabled = 1;
				break;
			default:
				error = EINVAL;
				break;
			}
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			if (ireq->i_val < 0) {
				error = EINVAL;
				break;
			}
			sc->wi_max_sleep = ireq->i_val;
			break;
		default:
			error = EINVAL;
			break;
		}

		/* Reinitialize WaveLAN. */
		wi_init(sc);

	break;
	case SIOCHOSTAP_ADD:
	case SIOCHOSTAP_DEL:
	case SIOCHOSTAP_GET:
	case SIOCHOSTAP_GETALL:
	case SIOCHOSTAP_GFLAGS:
	case SIOCHOSTAP_SFLAGS:
		/* Send all Host AP specific ioctl's to Host AP code. */
		error = wihap_ioctl(sc, command, data);
		break;
	default:
		error = EINVAL;
		break;
	}
out:
	WI_UNLOCK(sc);

	return(error);
}

static void
wi_init(xsc)
	void			*xsc;
{
	struct wi_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct wi_ltv_macaddr	mac;
	int			id = 0;

	WI_LOCK(sc);

	if (sc->wi_gone) {
		WI_UNLOCK(sc);
		return;
	}

	if (ifp->if_flags & IFF_RUNNING)
		wi_stop(sc);

	wi_reset(sc);

	/* Program max data length. */
	WI_SETVAL(WI_RID_MAX_DATALEN, sc->wi_max_data_len);

	/* Enable/disable IBSS creation. */
	WI_SETVAL(WI_RID_CREATE_IBSS, sc->wi_create_ibss);

	/* Set the port type. */
	WI_SETVAL(WI_RID_PORTTYPE, sc->wi_ptype);

	/* Program the RTS/CTS threshold. */
	WI_SETVAL(WI_RID_RTS_THRESH, sc->wi_rts_thresh);

	/* Program the TX rate */
	WI_SETVAL(WI_RID_TX_RATE, sc->wi_tx_rate);

	/* Access point density */
	WI_SETVAL(WI_RID_SYSTEM_SCALE, sc->wi_ap_density);

	/* Power Management Enabled */
	WI_SETVAL(WI_RID_PM_ENABLED, sc->wi_pm_enabled);

	/* Power Managment Max Sleep */
	WI_SETVAL(WI_RID_MAX_SLEEP, sc->wi_max_sleep);

	/* Roaming type */
	WI_SETVAL(WI_RID_ROAMING_MODE, sc->wi_roaming);

	/* Specify the IBSS name */
	WI_SETSTR(WI_RID_OWN_SSID, sc->wi_ibss_name);

	/* Specify the network name */
	WI_SETSTR(WI_RID_DESIRED_SSID, sc->wi_net_name);

	/* Specify the frequency to use */
	WI_SETVAL(WI_RID_OWN_CHNL, sc->wi_channel);

	/* Program the nodename. */
	WI_SETSTR(WI_RID_NODENAME, sc->wi_node_name);

	/* Specify the authentication mode. */
	WI_SETVAL(WI_RID_CNFAUTHMODE, sc->wi_authmode);

	/* Set our MAC address. */
	mac.wi_len = 4;
	mac.wi_type = WI_RID_MAC_NODE;
	bcopy((char *)&sc->arpcom.ac_enaddr,
	   (char *)&mac.wi_mac_addr, ETHER_ADDR_LEN);
	wi_write_record(sc, (struct wi_ltv_gen *)&mac);

	/* Configure WEP. */
	if (sc->wi_has_wep) {
		WI_SETVAL(WI_RID_ENCRYPTION, sc->wi_use_wep);
		WI_SETVAL(WI_RID_TX_CRYPT_KEY, sc->wi_tx_key);
		sc->wi_keys.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
		sc->wi_keys.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
		wi_write_record(sc, (struct wi_ltv_gen *)&sc->wi_keys);
		if (sc->sc_firmware_type != WI_LUCENT && sc->wi_use_wep) {
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
				WI_SETVAL(WI_RID_PROMISC, 1);
			}
			WI_SETVAL(WI_RID_CNFAUTHMODE, sc->wi_authtype);
		}
	}

	/* Initialize promisc mode. */
	if (ifp->if_flags & IFF_PROMISC) {
		WI_SETVAL(WI_RID_PROMISC, 1);
	} else {
		WI_SETVAL(WI_RID_PROMISC, 0);
	}

	/* Set multicast filter. */
	wi_setmulti(sc);

	/* Enable desired port */
	wi_cmd(sc, WI_CMD_ENABLE | sc->wi_portnum, 0, 0, 0);

	if (wi_alloc_nicmem(sc, ETHER_MAX_LEN + sizeof(struct wi_frame) + 8, &id))
		device_printf(sc->dev, "tx buffer allocation failed\n");
	sc->wi_tx_data_id = id;

	if (wi_alloc_nicmem(sc, ETHER_MAX_LEN + sizeof(struct wi_frame) + 8, &id))
		device_printf(sc->dev, "mgmt. buffer allocation failed\n");
	sc->wi_tx_mgmt_id = id;

	/* enable interrupts */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	wihap_init(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->wi_stat_ch = timeout(wi_inquire, sc, hz * 60);
	WI_UNLOCK(sc);

	return;
}

static u_int32_t crc32_tab[] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};

#define RC4STATE 256
#define RC4KEYLEN 16
#define RC4SWAP(x,y) \
    do { u_int8_t t = state[x]; state[x] = state[y]; state[y] = t; } while(0)

static void
wi_do_hostencrypt(struct wi_softc *sc, caddr_t buf, int len)
{
	u_int32_t i, crc, klen;
	u_int8_t state[RC4STATE], key[RC4KEYLEN];
	u_int8_t x, y, *dat;

	if (!sc->wi_icv_flag) {
		sc->wi_icv = arc4random();
		sc->wi_icv_flag++;
        } else
		sc->wi_icv++;
	/*
	 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
	 * (B, 255, N) with 3 <= B < 8
	 */
	if (sc->wi_icv >= 0x03ff00 &&
            (sc->wi_icv & 0xf8ff00) == 0x00ff00)
                sc->wi_icv += 0x000100;

	/* prepend 24bit IV to tx key, byte order does not matter */
	key[0] = sc->wi_icv >> 16;
	key[1] = sc->wi_icv >> 8;
	key[2] = sc->wi_icv;

	klen = sc->wi_keys.wi_keys[sc->wi_tx_key].wi_keylen +
	    IEEE80211_WEP_IVLEN;
	klen = (klen >= RC4KEYLEN) ? RC4KEYLEN : RC4KEYLEN/2;
	bcopy((char *)&sc->wi_keys.wi_keys[sc->wi_tx_key].wi_keydat,
	    (char *)key + IEEE80211_WEP_IVLEN, klen - IEEE80211_WEP_IVLEN);

	/* rc4 keysetup */
	x = y = 0;
	for (i = 0; i < RC4STATE; i++)
		state[i] = i;
	for (i = 0; i < RC4STATE; i++) {
		y = (key[x] + state[i] + y) % RC4STATE;
		RC4SWAP(i, y);
		x = (x + 1) % klen;
	}

	/* output: IV, tx keyid, rc4(data), rc4(crc32(data)) */
	dat = buf;
	dat[0] = key[0];
	dat[1] = key[1];
	dat[2] = key[2];
	dat[3] = sc->wi_tx_key << 6;		/* pad and keyid */
	dat += 4;

	/* compute rc4 over data, crc32 over data */
	crc = ~0;
	x = y = 0;
	for (i = 0; i < len; i++) {
		x = (x + 1) % RC4STATE;
		y = (state[x] + y) % RC4STATE;
		RC4SWAP(x, y);
		crc = crc32_tab[(crc ^ dat[i]) & 0xff] ^ (crc >> 8);
		dat[i] ^= state[(state[x] + state[y]) % RC4STATE];
	}
	crc = ~crc;
	dat += len;

	/* append little-endian crc32 and encrypt */
	dat[0] = crc;
	dat[1] = crc >> 8;
	dat[2] = crc >> 16;
	dat[3] = crc >> 24;
	for (i = 0; i < IEEE80211_WEP_CRCLEN; i++) {
		x = (x + 1) % RC4STATE;
		y = (state[x] + y) % RC4STATE;
		RC4SWAP(x, y);
		dat[i] ^= state[(state[x] + state[y]) % RC4STATE];
	}
}

static void
wi_start(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;
	struct mbuf		*m0;
	struct wi_frame		tx_frame;
	struct ether_header	*eh;
	int			id;

	sc = ifp->if_softc;
	WI_LOCK(sc);

	if (sc->wi_gone) {
		WI_UNLOCK(sc);
		return;
	}

	if (ifp->if_flags & IFF_OACTIVE) {
		WI_UNLOCK(sc);
		return;
	}

nextpkt:
	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL) {
		WI_UNLOCK(sc);
		return;
	}

	bzero((char *)&tx_frame, sizeof(tx_frame));
	tx_frame.wi_frame_ctl = htole16(WI_FTYPE_DATA);
	id = sc->wi_tx_data_id;
	eh = mtod(m0, struct ether_header *);

	if (sc->wi_ptype == WI_PORTTYPE_AP) {
		if (!wihap_check_tx(&sc->wi_hostap_info,
		    eh->ether_dhost, &tx_frame.wi_tx_rate)) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("wi_start: dropping unassoc "
				       "dst %6D\n", eh->ether_dhost, ":");
			m_freem(m0);
			goto nextpkt;
		}
	}
	/*
	 * Use RFC1042 encoding for IP and ARP datagrams,
	 * 802.3 for anything else.
	 */
	if (ntohs(eh->ether_type) > ETHER_MAX_LEN) {
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_addr1, ETHER_ADDR_LEN);
		if (sc->wi_ptype == WI_PORTTYPE_AP) {
			tx_frame.wi_tx_ctl = WI_ENC_TX_MGMT; /* XXX */
			tx_frame.wi_frame_ctl |= WI_FCTL_FROMDS;
			if (sc->wi_use_wep)
				tx_frame.wi_frame_ctl |= WI_FCTL_WEP;
			bcopy((char *)&sc->arpcom.ac_enaddr,
			      (char *)&tx_frame.wi_addr2, ETHER_ADDR_LEN);
			bcopy((char *)&eh->ether_shost,
			      (char *)&tx_frame.wi_addr3, ETHER_ADDR_LEN);
		}
		else
			bcopy((char *)&eh->ether_shost,
			    (char *)&tx_frame.wi_addr2, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_dst_addr, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_src_addr, ETHER_ADDR_LEN);

		tx_frame.wi_dat_len = m0->m_pkthdr.len - WI_SNAPHDR_LEN;
		tx_frame.wi_dat[0] = htons(WI_SNAP_WORD0);
		tx_frame.wi_dat[1] = htons(WI_SNAP_WORD1);
		tx_frame.wi_len = htons(m0->m_pkthdr.len - WI_SNAPHDR_LEN);
		tx_frame.wi_type = eh->ether_type;

		if (sc->wi_ptype == WI_PORTTYPE_AP && sc->wi_use_wep) {
			/* Do host encryption. */
			bcopy(&tx_frame.wi_dat[0], &sc->wi_txbuf[4], 8);
			m_copydata(m0, sizeof(struct ether_header),
			    m0->m_pkthdr.len - sizeof(struct ether_header),
			    (caddr_t)&sc->wi_txbuf[12]);
			wi_do_hostencrypt(sc, &sc->wi_txbuf[0],
			    tx_frame.wi_dat_len);
			tx_frame.wi_dat_len += IEEE80211_WEP_IVLEN +
			    IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN;
			wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
			    sizeof(struct wi_frame));
			wi_write_data(sc, id, WI_802_11_OFFSET_RAW,
			    (caddr_t)&sc->wi_txbuf, (m0->m_pkthdr.len -
			    sizeof(struct ether_header)) + 18);
		} else {
			m_copydata(m0, sizeof(struct ether_header),
			    m0->m_pkthdr.len - sizeof(struct ether_header),
			    (caddr_t)&sc->wi_txbuf);
			wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
			    sizeof(struct wi_frame));
			wi_write_data(sc, id, WI_802_11_OFFSET,
			    (caddr_t)&sc->wi_txbuf, (m0->m_pkthdr.len -
			    sizeof(struct ether_header)) + 2);
		}
	} else {
		tx_frame.wi_dat_len = m0->m_pkthdr.len;

		if (sc->wi_ptype == WI_PORTTYPE_AP && sc->wi_use_wep) {
			/* Do host encryption. */
			printf( "XXX: host encrypt not implemented for 802.3\n" );
		} else {
			eh->ether_type = htons(m0->m_pkthdr.len -
			    WI_SNAPHDR_LEN);
			m_copydata(m0, 0, m0->m_pkthdr.len,
			    (caddr_t)&sc->wi_txbuf);

			wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
			    sizeof(struct wi_frame));
			wi_write_data(sc, id, WI_802_3_OFFSET,
			    (caddr_t)&sc->wi_txbuf, m0->m_pkthdr.len + 2);
		}
	}

	/*
	 * If there's a BPF listner, bounce a copy of
 	 * this frame to him. Also, don't send this to the bpf sniffer
 	 * if we're in procframe or monitor sniffing mode.
	 */
 	if (!(sc->wi_procframe || sc->wi_debug.wi_monitor) && ifp->if_bpf)
		bpf_mtap(ifp, m0);

	m_freem(m0);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id, 0, 0))
		device_printf(sc->dev, "xmit failed\n");

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	WI_UNLOCK(sc);
	return;
}

int
wi_mgmt_xmit(sc, data, len)
	struct wi_softc		*sc;
	caddr_t			data;
	int			len;
{
	struct wi_frame		tx_frame;
	int			id;
	struct wi_80211_hdr	*hdr;
	caddr_t			dptr;

	if (sc->wi_gone)
		return(ENODEV);

	hdr = (struct wi_80211_hdr *)data;
	dptr = data + sizeof(struct wi_80211_hdr);

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_mgmt_id;

	bcopy((char *)hdr, (char *)&tx_frame.wi_frame_ctl,
	   sizeof(struct wi_80211_hdr));

	tx_frame.wi_tx_ctl = WI_ENC_TX_MGMT;
	tx_frame.wi_dat_len = len - sizeof(struct wi_80211_hdr);
	tx_frame.wi_len = htons(tx_frame.wi_dat_len);

	wi_write_data(sc, id, 0, (caddr_t)&tx_frame, sizeof(struct wi_frame));
	wi_write_data(sc, id, WI_802_11_OFFSET_RAW, dptr,
	    len - sizeof(struct wi_80211_hdr) + 2);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id, 0, 0)) {
		device_printf(sc->dev, "xmit failed\n");
		return(EIO);
	}

	return(0);
}

static void
wi_stop(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;

	WI_LOCK(sc);

	if (sc->wi_gone) {
		WI_UNLOCK(sc);
		return;
	}

	wihap_shutdown(sc);

	ifp = &sc->arpcom.ac_if;

	/*
	 * If the card is gone and the memory port isn't mapped, we will
	 * (hopefully) get 0xffff back from the status read, which is not
	 * a valid status value.
	 */
	if (CSR_READ_2(sc, WI_STATUS) != 0xffff) {
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		wi_cmd(sc, WI_CMD_DISABLE|sc->wi_portnum, 0, 0, 0);
	}

	untimeout(wi_inquire, sc, sc->wi_stat_ch);

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	WI_UNLOCK(sc);
	return;
}

static void
wi_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;

	sc = ifp->if_softc;

	device_printf(sc->dev, "watchdog timeout\n");

	wi_init(sc);

	ifp->if_oerrors++;

	return;
}

int
wi_alloc(dev, rid)
	device_t		dev;
	int			rid;
{
	struct wi_softc		*sc = device_get_softc(dev);

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

	sc->dev = dev;
	sc->wi_unit = device_get_unit(dev);

	return (0);
}

void
wi_free(dev)
	device_t		dev;
{
	struct wi_softc		*sc = device_get_softc(dev);

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

void
wi_shutdown(dev)
	device_t		dev;
{
	struct wi_softc		*sc;

	sc = device_get_softc(dev);
	wi_stop(sc);

	return;
}

#ifdef WICACHE
/* wavelan signal strength cache code.
 * store signal/noise/quality on per MAC src basis in
 * a small fixed cache.  The cache wraps if > MAX slots
 * used.  The cache may be zeroed out to start over.
 * Two simple filters exist to reduce computation:
 * 1. ip only (literally 0x800) which may be used
 * to ignore some packets.  It defaults to ip only.
 * it could be used to focus on broadcast, non-IP 802.11 beacons.
 * 2. multicast/broadcast only.  This may be used to
 * ignore unicast packets and only cache signal strength
 * for multicast/broadcast packets (beacons); e.g., Mobile-IP
 * beacons and not unicast traffic.
 *
 * The cache stores (MAC src(index), IP src (major clue), signal,
 *	quality, noise)
 *
 * No apologies for storing IP src here.  It's easy and saves much
 * trouble elsewhere.  The cache is assumed to be INET dependent, 
 * although it need not be.
 */

#ifdef documentation

int wi_sigitems;                                /* number of cached entries */
struct wi_sigcache wi_sigcache[MAXWICACHE];  /*  array of cache entries */
int wi_nextitem;                                /*  index/# of entries */


#endif

/* control variables for cache filtering.  Basic idea is
 * to reduce cost (e.g., to only Mobile-IP agent beacons
 * which are broadcast or multicast).  Still you might
 * want to measure signal strength with unicast ping packets
 * on a pt. to pt. ant. setup.
 */
/* set true if you want to limit cache items to broadcast/mcast 
 * only packets (not unicast).  Useful for mobile-ip beacons which
 * are broadcast/multicast at network layer.  Default is all packets
 * so ping/unicast will work say with pt. to pt. antennae setup.
 */
static int wi_cache_mcastonly = 0;
SYSCTL_INT(_machdep, OID_AUTO, wi_cache_mcastonly, CTLFLAG_RW, 
	&wi_cache_mcastonly, 0, "");

/* set true if you want to limit cache items to IP packets only
*/
static int wi_cache_iponly = 1;
SYSCTL_INT(_machdep, OID_AUTO, wi_cache_iponly, CTLFLAG_RW, 
	&wi_cache_iponly, 0, "");

/*
 * Original comments:
 * -----------------
 * wi_cache_store, per rx packet store signal
 * strength in MAC (src) indexed cache.
 *
 * follows linux driver in how signal strength is computed.
 * In ad hoc mode, we use the rx_quality field. 
 * signal and noise are trimmed to fit in the range from 47..138.
 * rx_quality field MSB is signal strength.
 * rx_quality field LSB is noise.
 * "quality" is (signal - noise) as is log value.
 * note: quality CAN be negative.
 * 
 * In BSS mode, we use the RID for communication quality.
 * TBD:  BSS mode is currently untested.
 *
 * Bill's comments:
 * ---------------
 * Actually, we use the rx_quality field all the time for both "ad-hoc"
 * and BSS modes. Why? Because reading an RID is really, really expensive:
 * there's a bunch of PIO operations that have to be done to read a record
 * from the NIC, and reading the comms quality RID each time a packet is
 * received can really hurt performance. We don't have to do this anyway:
 * the comms quality field only reflects the values in the rx_quality field
 * anyway. The comms quality RID is only meaningful in infrastructure mode,
 * but the values it contains are updated based on the rx_quality from
 * frames received from the access point.
 *
 * Also, according to Lucent, the signal strength and noise level values
 * can be converted to dBms by subtracting 149, so I've modified the code
 * to do that instead of the scaling it did originally.
 */
static void
wi_cache_store(struct wi_softc *sc, struct ether_header *eh,
                     struct mbuf *m, unsigned short rx_quality)
{
	struct ip *ip = 0; 
	int i;
	static int cache_slot = 0; 	/* use this cache entry */
	static int wrapindex = 0;       /* next "free" cache entry */
	int sig, noise;
	int sawip=0;

	/* 
	 * filters:
	 * 1. ip only
	 * 2. configurable filter to throw out unicast packets,
	 * keep multicast only.
	 */
 
	if ((ntohs(eh->ether_type) == ETHERTYPE_IP)) {
		sawip = 1;
	}

	/* 
	 * filter for ip packets only 
	*/
	if (wi_cache_iponly && !sawip) {
		return;
	}

	/*
	 *  filter for broadcast/multicast only
	 */
	if (wi_cache_mcastonly && ((eh->ether_dhost[0] & 1) == 0)) {
		return;
	}

#ifdef SIGDEBUG
	printf("wi%d: q value %x (MSB=0x%x, LSB=0x%x) \n", sc->wi_unit,
	    rx_quality & 0xffff, rx_quality >> 8, rx_quality & 0xff);
#endif

	/*
	 *  find the ip header.  we want to store the ip_src
	 * address.  
	 */
	if (sawip)
		ip = mtod(m, struct ip *);
        
	/*
	 * do a linear search for a matching MAC address 
	 * in the cache table
	 * . MAC address is 6 bytes,
	 * . var w_nextitem holds total number of entries already cached
	 */
	for(i = 0; i < sc->wi_nextitem; i++) {
		if (! bcmp(eh->ether_shost , sc->wi_sigcache[i].macsrc,  6 )) {
			/* 
			 * Match!,
			 * so we already have this entry,
			 * update the data
			 */
			break;	
		}
	}

	/*
	 *  did we find a matching mac address?
	 * if yes, then overwrite a previously existing cache entry
	 */
	if (i < sc->wi_nextitem )   {
		cache_slot = i; 
	}
	/*
	 * else, have a new address entry,so
	 * add this new entry,
	 * if table full, then we need to replace LRU entry
	 */
	else    {                          

		/* 
		 * check for space in cache table 
		 * note: wi_nextitem also holds number of entries
		 * added in the cache table 
		 */
		if ( sc->wi_nextitem < MAXWICACHE ) {
			cache_slot = sc->wi_nextitem;
			sc->wi_nextitem++;                 
			sc->wi_sigitems = sc->wi_nextitem;
		}
        	/* no space found, so simply wrap with wrap index
		 * and "zap" the next entry
		 */
		else {
			if (wrapindex == MAXWICACHE) {
				wrapindex = 0;
			}
			cache_slot = wrapindex++;
		}
	}

	/* 
	 * invariant: cache_slot now points at some slot
	 * in cache.
	 */
	if (cache_slot < 0 || cache_slot >= MAXWICACHE) {
		log(LOG_ERR, "wi_cache_store, bad index: %d of "
		    "[0..%d], gross cache error\n",
		    cache_slot, MAXWICACHE);
		return;
	}

	/*
	 *  store items in cache
	 *  .ip source address
	 *  .mac src
	 *  .signal, etc.
	 */
	if (sawip)
		sc->wi_sigcache[cache_slot].ipsrc = ip->ip_src.s_addr;
	bcopy( eh->ether_shost, sc->wi_sigcache[cache_slot].macsrc,  6);

	sig = (rx_quality >> 8) & 0xFF;
	noise = rx_quality & 0xFF;
	sc->wi_sigcache[cache_slot].signal = sig - 149;
	sc->wi_sigcache[cache_slot].noise = noise - 149;
	sc->wi_sigcache[cache_slot].quality = sig - noise;

	return;
}
#endif

static int
wi_get_cur_ssid(sc, ssid, len)
	struct wi_softc		*sc;
	char			*ssid;
	int			*len;
{
	int			error = 0;
	struct wi_req		wreq;

	wreq.wi_len = WI_MAX_DATALEN;
	switch (sc->wi_ptype) {
	case WI_PORTTYPE_AP:
		*len = IEEE80211_NWID_LEN;
		bcopy(sc->wi_net_name, ssid, IEEE80211_NWID_LEN);
		break;
	case WI_PORTTYPE_ADHOC:
		wreq.wi_type = WI_RID_CURRENT_SSID;
		error = wi_read_record(sc, (struct wi_ltv_gen *)&wreq);
		if (error != 0)
			break;
		if (wreq.wi_val[0] > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		*len = wreq.wi_val[0];
		bcopy(&wreq.wi_val[1], ssid, IEEE80211_NWID_LEN);
		break;
	case WI_PORTTYPE_BSS:
		wreq.wi_type = WI_RID_COMMQUAL;
		error = wi_read_record(sc, (struct wi_ltv_gen *)&wreq);
		if (error != 0)
			break;
		if (wreq.wi_val[0] != 0) /* associated */ {
			wreq.wi_type = WI_RID_CURRENT_SSID;
			wreq.wi_len = WI_MAX_DATALEN;
			error = wi_read_record(sc, (struct wi_ltv_gen *)&wreq);
			if (error != 0)
				break;
			if (wreq.wi_val[0] > IEEE80211_NWID_LEN) {
				error = EINVAL;
				break;
			}
			*len = wreq.wi_val[0];
			bcopy(&wreq.wi_val[1], ssid, IEEE80211_NWID_LEN);
		} else {
			*len = IEEE80211_NWID_LEN;
			bcopy(sc->wi_net_name, ssid, IEEE80211_NWID_LEN);
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static int
wi_media_change(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc = ifp->if_softc;
	int			otype = sc->wi_ptype;
	int			orate = sc->wi_tx_rate;

	if ((sc->ifmedia.ifm_cur->ifm_media & IFM_IEEE80211_ADHOC) != 0)
		sc->wi_ptype = WI_PORTTYPE_ADHOC;
	else if ((sc->ifmedia.ifm_cur->ifm_media & IFM_IEEE80211_HOSTAP) != 0)
		sc->wi_ptype = WI_PORTTYPE_AP;
	else
		sc->wi_ptype = WI_PORTTYPE_BSS;

	switch (IFM_SUBTYPE(sc->ifmedia.ifm_cur->ifm_media)) {
	case IFM_IEEE80211_DS1:
		sc->wi_tx_rate = 1;
		break;
	case IFM_IEEE80211_DS2:
		sc->wi_tx_rate = 2;
		break;
	case IFM_IEEE80211_DS5:
		sc->wi_tx_rate = 5;
		break;
	case IFM_IEEE80211_DS11:
		sc->wi_tx_rate = 11;
		break;
	case IFM_AUTO:
		sc->wi_tx_rate = 3;
		break;
	}

	if (otype != sc->wi_ptype ||
	    orate != sc->wi_tx_rate)
		wi_init(sc);

	return(0);
}

static void
wi_media_status(ifp, imr)
	struct ifnet		*ifp;
	struct ifmediareq	*imr;
{
	struct wi_req		wreq;
	struct wi_softc		*sc = ifp->if_softc;

	if (sc->wi_tx_rate == 3) {
		imr->ifm_active = IFM_IEEE80211|IFM_AUTO;
		if (sc->wi_ptype == WI_PORTTYPE_ADHOC)
			imr->ifm_active |= IFM_IEEE80211_ADHOC;
		else if (sc->wi_ptype == WI_PORTTYPE_AP)
			imr->ifm_active |= IFM_IEEE80211_HOSTAP;
		wreq.wi_type = WI_RID_CUR_TX_RATE;
		wreq.wi_len = WI_MAX_DATALEN;
		if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq) == 0) {
			switch(wreq.wi_val[0]) {
			case 1:
				imr->ifm_active |= IFM_IEEE80211_DS1;
				break;
			case 2:
				imr->ifm_active |= IFM_IEEE80211_DS2;
				break;
			case 6:
				imr->ifm_active |= IFM_IEEE80211_DS5;
				break;
			case 11:
				imr->ifm_active |= IFM_IEEE80211_DS11;
				break;
				}
		}
	} else {
		imr->ifm_active = sc->ifmedia.ifm_cur->ifm_media;
	}

	imr->ifm_status = IFM_AVALID;
	if (sc->wi_ptype == WI_PORTTYPE_ADHOC)
		/*
		 * XXX: It would be nice if we could give some actually
		 * useful status like whether we joined another IBSS or
		 * created one ourselves.
		 */
		imr->ifm_status |= IFM_ACTIVE;
	else if (sc->wi_ptype == WI_PORTTYPE_AP)
		imr->ifm_status |= IFM_ACTIVE;
	else {
		wreq.wi_type = WI_RID_COMMQUAL;
		wreq.wi_len = WI_MAX_DATALEN;
		if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq) == 0 &&
		    wreq.wi_val[0] != 0)
			imr->ifm_status |= IFM_ACTIVE;
	}
}

static int
wi_get_debug(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	int			error = 0;

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
wi_set_debug(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	int			error = 0;
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
