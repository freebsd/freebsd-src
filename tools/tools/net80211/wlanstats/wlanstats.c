/*-
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
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
 *
 * $FreeBSD$
 */

/*
 * net80211 statistics class.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/ethernet.h>

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <ifaddrs.h>

#include "../../../../sys/net80211/ieee80211_ioctl.h"

#include "wlanstats.h"

#ifndef IEEE80211_ADDR_COPY
#define	IEEE80211_ADDR_COPY(dst, src)	memcpy(dst, src, IEEE80211_ADDR_LEN)
#define	IEEE80211_ADDR_EQ(a1,a2)	(memcmp(a1,a2,IEEE80211_ADDR_LEN) == 0)
#endif

static const struct fmt wlanstats[] = {
#define	S_RX_BADVERSION	0
	{ 5,  "rx_badversion",	"badversion",	"rx frame with bad version" },
#define	S_RX_TOOSHORT	1
	{ 5,  "rx_tooshort",	"tooshort",	"rx frame too short" },
#define	S_RX_WRONGBSS	2
	{ 5,  "rx_wrongbss",	"wrongbss",	"rx from wrong bssid" },
#define	S_RX_DUP	3
	{ 5,  "rx_dup",	"rx_dup",		"rx discard 'cuz dup" },
#define	S_RX_WRONGDIR	4
	{ 5,  "rx_wrongdir",	"wrongdir",	"rx w/ wrong direction" },
#define	S_RX_MCASTECHO	5
	{ 5,  "rx_mcastecho",	"mcastecho",	"rx discard 'cuz mcast echo" },
#define	S_RX_NOTASSOC	6
	{ 5,  "rx_notassoc",	"notassoc",	"rx discard 'cuz sta !assoc" },
#define	S_RX_NOPRIVACY	7
	{ 5,  "rx_noprivacy",	"noprivacy",	"rx w/ wep but privacy off" },
#define	S_RX_UNENCRYPTED	8
	{ 5,  "rx_unencrypted",	"unencrypted",	"rx w/o wep and privacy on" },
#define	S_RX_WEPFAIL	9
	{ 5,  "rx_wepfail",	"wepfail",	"rx wep processing failed" },
#define	S_RX_DECAP	10
	{ 5,  "rx_decap",	"decap",	"rx decapsulation failed" },
#define	S_RX_MGTDISCARD	11
	{ 5,  "rx_mgtdiscard",	"mgtdiscard",	"rx discard mgt frames" },
#define	S_RX_CTL	12
	{ 5,  "rx_ctl",		"ctl",		"rx discard ctrl frames" },
#define	S_RX_BEACON	13
	{ 5,  "rx_beacon",	"beacon",	"rx beacon frames" },
#define	S_RX_RSTOOBIG	14
	{ 5,  "rx_rstoobig",	"rstoobig",	"rx rate set truncated" },
#define	S_RX_ELEM_MISSING	15
	{ 5,  "rx_elem_missing","elem_missing",	"rx required element missing" },
#define	S_RX_ELEM_TOOBIG	16
	{ 5,  "rx_elem_toobig",	"elem_toobig",	"rx element too big" },
#define	S_RX_ELEM_TOOSMALL	17
	{ 5,  "rx_elem_toosmall","elem_toosmall","rx element too small" },
#define	S_RX_ELEM_UNKNOWN	18
	{ 5,  "rx_elem_unknown","elem_unknown",	"rx element unknown" },
#define	S_RX_BADCHAN	19
	{ 5,  "rx_badchan",	"badchan",	"rx frame w/ invalid chan" },
#define	S_RX_CHANMISMATCH	20
	{ 5,  "rx_chanmismatch","chanmismatch",	"rx frame chan mismatch" },
#define	S_RX_NODEALLOC	21
	{ 5,  "rx_nodealloc",	"nodealloc",	"nodes allocated (rx)" },
#define	S_RX_SSIDMISMATCH	22
	{ 5,  "rx_ssidmismatch","ssidmismatch",	"rx frame ssid mismatch" },
#define	S_RX_AUTH_UNSUPPORTED	23
	{ 5,  "rx_auth_unsupported","auth_unsupported",
		"rx w/ unsupported auth alg" },
#define	S_RX_AUTH_FAIL	24
	{ 5,  "rx_auth_fail",	"auth_fail",	"rx sta auth failure" },
#define	S_RX_AUTH_COUNTERMEASURES	25
	{ 5,  "rx_auth_countermeasures",	"auth_countermeasures",
		"rx sta auth failure 'cuz of TKIP countermeasures" },
#define	S_RX_ASSOC_BSS	26
	{ 5,  "rx_assoc_bss",	"assoc_bss",	"rx assoc from wrong bssid" },
#define	S_RX_ASSOC_NOTAUTH	27
	{ 5,  "rx_assoc_notauth","assoc_notauth",	"rx assoc w/o auth" },
#define	S_RX_ASSOC_CAPMISMATCH	28
	{ 5,  "rx_assoc_capmismatch","assoc_capmismatch",
		"rx assoc w/ cap mismatch" },
#define	S_RX_ASSOC_NORATE	29
	{ 5,  "rx_assoc_norate","assoc_norate",	"rx assoc w/ no rate match" },
#define	S_RX_ASSOC_BADWPAIE	30
	{ 5,  "rx_assoc_badwpaie","assoc_badwpaie",
		"rx assoc w/ bad WPA IE" },
#define	S_RX_DEAUTH	31
	{ 5,  "rx_deauth",	"deauth",	"rx deauthentication" },
#define	S_RX_DISASSOC	32
	{ 5,  "rx_disassoc",	"disassoc",	"rx disassociation" },
#define	S_RX_BADSUBTYPE	33
	{ 5,  "rx_badsubtype",	"badsubtype",	"rx frame w/ unknown subtype" },
#define	S_RX_NOBUF	34
	{ 5,  "rx_nobuf",	"nobuf",	"rx failed for lack of mbuf" },
#define	S_RX_DECRYPTCRC	35
	{ 5,  "rx_decryptcrc",	"decryptcrc",	"rx decrypt failed on crc" },
#define	S_RX_AHDEMO_MGT	36
	{ 5,  "rx_ahdemo_mgt",	"ahdemo_mgt",
		"rx discard mgmt frame received in ahdoc demo mode" },
#define	S_RX_BAD_AUTH	37
	{ 5,  "rx_bad_auth",	"bad_auth",	"rx bad authentication request" },
#define	S_RX_UNAUTH	38
	{ 5,  "rx_unauth",	"unauth",
		"rx discard 'cuz port unauthorized" },
#define	S_RX_BADKEYID	39
	{ 5,  "rx_badkeyid",	"badkeyid",	"rx w/ incorrect keyid" },
#define	S_RX_CCMPREPLAY	40
	{ 5,  "rx_ccmpreplay",	"ccmpreplay",	"rx seq# violation (CCMP)" },
#define	S_RX_CCMPFORMAT	41
	{ 5,  "rx_ccmpformat",	"ccmpformat",	"rx format bad (CCMP)" },
#define	S_RX_CCMPMIC	42
	{ 5,  "rx_ccmpmic",	"ccmpmic",	"rx MIC check failed (CCMP)" },
#define	S_RX_TKIPREPLAY	43
	{ 5,  "rx_tkipreplay",	"tkipreplay",	"rx seq# violation (TKIP)" },
#define	S_RX_TKIPFORMAT	44
	{ 5,  "rx_tkipformat",	"tkipformat",	"rx format bad (TKIP)" },
#define	S_RX_TKIPMIC	45
	{ 5,  "rx_tkipmic",	"tkipmic",	"rx MIC check failed (TKIP)" },
#define	S_RX_TKIPICV	46
	{ 5,  "rx_tkipicv",	"tkipicv",	"rx ICV check failed (TKIP)" },
#define	S_RX_BADCIPHER	47
	{ 5,  "rx_badcipher",	"badcipher",	"rx failed 'cuz bad cipher/key type" },
#define	S_RX_NOCIPHERCTX	48
	{ 5,  "rx_nocipherctx",	"nocipherctx",	"rx failed 'cuz key/cipher ctx not setup" },
#define	S_RX_ACL	49
	{ 5,  "rx_acl",		"acl",		"rx discard 'cuz acl policy" },
#define	S_TX_NOBUF	50
	{ 5,  "tx_nobuf",	"nobuf",	"tx failed for lack of mbuf" },
#define	S_TX_NONODE	51
	{ 5,  "tx_nonode",	"nonode",	"tx failed for no node" },
#define	S_TX_UNKNOWNMGT	52
	{ 5,  "tx_unknownmgt",	"unknownmgt",	"tx of unknown mgt frame" },
#define	S_TX_BADCIPHER	53
	{ 5,  "tx_badcipher",	"badcipher",	"tx failed 'cuz bad ciper/key type" },
#define	S_TX_NODEFKEY	54
	{ 5,  "tx_nodefkey",	"nodefkey",	"tx failed 'cuz no defkey" },
#define	S_TX_NOHEADROOM	55
	{ 5,  "tx_noheadroom",	"noheadroom",	"tx failed 'cuz no space for crypto hdrs" },
#define	S_TX_FRAGFRAMES	56
	{ 5,  "tx_fragframes",	"fragframes",	"tx frames fragmented" },
#define	S_TX_FRAGS	57
	{ 5,  "tx_frags",	"frags",		"tx frags generated" },
#define	S_SCAN_ACTIVE	58
	{ 5,  "scan_active",	"scan_active",	"active scans started" },
#define	S_SCAN_PASSIVE	59
	{ 5,  "scan_passive",	"scan_passive",	"passive scans started" },
#define	S_NODE_TIMEOUT	60
	{ 5,  "node_timeout",	"node_timeout",	"nodes timed out inactivity" },
#define	S_CRYPTO_NOMEM	61
	{ 5,  "crypto_nomem",	"crypto_nomem",	"cipher context malloc failed" },
#define	S_CRYPTO_TKIP	62
	{ 5,  "crypto_tkip",	"crypto_tkip",	"tkip crypto done in s/w" },
#define	S_CRYPTO_TKIPENMIC	63
	{ 5,  "crypto_tkipenmic","crypto_tkipenmic",	"tkip tx MIC done in s/w" },
#define	S_CRYPTO_TKIPDEMIC	64
	{ 5,  "crypto_tkipdemic","crypto_tkipdemic",	"tkip rx MIC done in s/w" },
#define	S_CRYPTO_TKIPCM	65
	{ 5,  "crypto_tkipcm",	"crypto_tkipcm",	"tkip dropped frames 'cuz of countermeasures" },
#define	S_CRYPTO_CCMP	66
	{ 5,  "crypto_ccmp",	"crypto_ccmp",	"ccmp crypto done in s/w" },
#define	S_CRYPTO_WEP	67
	{ 5,  "crypto_wep",	"crypto_wep",	"wep crypto done in s/w" },
#define	S_CRYPTO_SETKEY_CIPHER	68
	{ 5,  "crypto_setkey_cipher",	"crypto_setkey_cipher","setkey failed 'cuz cipher rejected data" },
#define	S_CRYPTO_SETKEY_NOKEY	69
	{ 5,  "crypto_setkey_nokey",	"crypto_setkey_nokey","setkey failed 'cuz no key index" },
#define	S_CRYPTO_DELKEY	70
	{ 5,  "crypto_delkey",	"crypto_delkey",	"driver key delete failed" },
#define	S_CRYPTO_BADCIPHER	71
	{ 5,  "crypto_badcipher","crypto_badcipher",	"setkey failed 'cuz unknown cipher" },
#define	S_CRYPTO_NOCIPHER	72
	{ 5,  "crypto_nocipher","crypto_nocipher",	"setkey failed 'cuz cipher module unavailable" },
#define	S_CRYPTO_ATTACHFAIL	73
	{ 5,  "crypto_attachfail","crypto_attachfail",	"setkey failed 'cuz cipher attach failed" },
#define	S_CRYPTO_SWFALLBACK	74
	{ 5,  "crypto_swfallback","crypto_swfallback",	"crypto fell back to s/w implementation" },
#define	S_CRYPTO_KEYFAIL	75
	{ 5,  "crypto_keyfail",	"crypto_keyfail",	"setkey failed 'cuz driver key alloc failed" },
#define	S_CRYPTO_ENMICFAIL	76
	{ 5,  "crypto_enmicfail","crypto_enmicfail",	"enmic failed (may be mbuf exhaustion)" },
#define	S_IBSS_CAPMISMATCH	77
	{ 5,  "ibss_capmismatch","ibss_capmismatch",	"ibss merge faied 'cuz capabilities mismatch" },
#define	S_IBSS_NORATE	78
	{ 5,  "ibss_norate",	"ibss_norate",	"ibss merge faied 'cuz rate set mismatch" },
#define	S_PS_UNASSOC	79
	{ 5,  "ps_unassoc",	"ps_unassoc",	"ps-poll received for unassociated station" },
#define	S_PS_BADAID	80
	{ 5,  "ps_badaid",	"ps_badaid",	"ps-poll received with invalid association id" },
#define	S_PS_QEMPTY	81
	{ 5,  "ps_qempty",	"ps_qempty",	"ps-poll received with nothing to send" },
#define	S_FF_BADHDR	82
	{ 5,  "ff_badhdr",	"ff_badhdr",	"fast frame rx'd w/ bad hdr" },
#define	S_FF_TOOSHORT	83
	{ 5,  "ff_tooshort",	"ff_tooshort",	"fast frame rx decap error" },
#define	S_FF_SPLIT	84
	{ 5,  "ff_split",	"ff_split",	"fast frame rx split error" },
#define	S_FF_DECAP	85
	{ 5,  "ff_decap",	"ff_decap",	"fast frames decap'd" },
#define	S_FF_ENCAP	86
	{ 5,  "ff_encap",	"ff_encap",	"fast frames encap'd for tx" },
#define	S_FF_ENCAPFAIL	87
	{ 5,  "ff_encapfail",	"ff_encapfail",	"fast frames encap failed" },
#define	S_RX_BADBINTVAL	88
	{ 5,  "rx_badbintval",	"rx_badbintval","rx frame with bogus beacon interval" },
#define	S_RX_MGMT	89
	{ 5,  "rx_mgmt",	"rx_mgmt",	"rx management frames" },
#define	S_RX_DEMICFAIL	90
	{ 5,  "rx_demicfail",	"rx_demicfail",	"rx demic failed" },
#define	S_RX_DEFRAG	91
	{ 5,  "rx_defrag",	"rx_defrag",	"rx defragmentation failed" },
#define	S_INPUT		92
	{ 8,	"input",	"input",	"data frames received" },
#define	S_OUTPUT	93
	{ 8,	"output",	"output",	"data frames transmit" },
#define	S_RATE		94
	{ 4,	"rate",		"rate",		"current transmit rate" },
#define	S_RSSI		95
	{ 4,	"rssi",		"rssi",		"current rssi" },
#define	S_NOISE		96
	{ 4,	"noise",	"noise",	"current noise floor (dBm)" },
#define	S_RX_UCAST	97
	{ 8,	"rx_ucast",	"rx_ucast",	"unicast data frames received" },
#define	S_RX_MCAST	98
	{ 8,	"rx_mcast",	"rx_mcast",	"multicast data frames received" },
#define	S_TX_UCAST	99
	{ 8,	"tx_ucast",	"tx_ucast",	"unicast data frames sent" },
#define	S_TX_MCAST	100
	{ 8,	"tx_mcast",	"tx_mcast",	"multicast data frames sent" },
#define	S_SIGNAL	101
	{ 4,	"signal",	"sig",		"current signal (dBm)" },
};
#define	S_LAST	S_RX_DEFRAG
#define	S_MAX	S_LAST+1

struct wlanstatfoo_p {
	struct wlanstatfoo base;
	int s;
	int opmode;
	uint8_t mac[IEEE80211_ADDR_LEN];
	struct ifreq ifr;
	struct ieee80211_stats cur;
	struct ieee80211_stats total;
	struct ieee80211req ireq;
	union {
		struct ieee80211req_sta_req info;
		char buf[1024];
	} u_info;
	struct ieee80211req_sta_stats ncur;
	struct ieee80211req_sta_stats ntotal;
};

static void
wlan_setifname(struct wlanstatfoo *wf0, const char *ifname)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) wf0;

	strncpy(wf->ifr.ifr_name, ifname, sizeof (wf->ifr.ifr_name));
	strncpy(wf->ireq.i_name, ifname, sizeof (wf->ireq.i_name));
}

static const char *
wlan_getifname(struct wlanstatfoo *wf0)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) wf0;

	return wf->ifr.ifr_name;
}

static int
wlan_getopmode(struct wlanstatfoo *wf0)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) wf0;

	if (wf->opmode == -1) {
		struct ifmediareq ifmr;

		memset(&ifmr, 0, sizeof(ifmr));
		strlcpy(ifmr.ifm_name, wf->ifr.ifr_name, sizeof(ifmr.ifm_name));
		if (ioctl(wf->s, SIOCGIFMEDIA, &ifmr) < 0)
			err(1, "%s (SIOCGIFMEDIA)", wf->ifr.ifr_name);
		if (ifmr.ifm_current & IFM_IEEE80211_ADHOC)
			wf->opmode = IEEE80211_M_IBSS;	/* XXX ahdemo */
		else if (ifmr.ifm_current & IFM_IEEE80211_HOSTAP)
			wf->opmode = IEEE80211_M_HOSTAP;
		else if (ifmr.ifm_current & IFM_IEEE80211_MONITOR)
			wf->opmode = IEEE80211_M_MONITOR;
		else
			wf->opmode = IEEE80211_M_STA;
	}
	return wf->opmode;
}

static void
getlladdr(struct wlanstatfoo_p *wf)
{
	const struct sockaddr_dl *sdl;
	struct ifaddrs *ifp, *p;

	if (getifaddrs(&ifp) != 0)
		err(1, "getifaddrs");
	for (p = ifp; p != NULL; p = p->ifa_next)
		if (strcmp(p->ifa_name, wf->ifr.ifr_name) == 0 &&
		    p->ifa_addr->sa_family == AF_LINK)
			break;
	if (p == NULL)
		errx(1, "did not find link layer address for interface %s",
			wf->ifr.ifr_name);
	sdl = (const struct sockaddr_dl *) p->ifa_addr;
	IEEE80211_ADDR_COPY(wf->mac, LLADDR(sdl));
	freeifaddrs(ifp);
}

static void
wlan_setstamac(struct wlanstatfoo *wf0, const uint8_t *mac)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) wf0;

	if (mac == NULL) {
		switch (wlan_getopmode(wf0)) {
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_MONITOR:
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			getlladdr(wf);
			break;
		case IEEE80211_M_STA:
			wf->ireq.i_type = IEEE80211_IOC_BSSID;
			wf->ireq.i_data = wf->mac;
			wf->ireq.i_len = IEEE80211_ADDR_LEN;
			if (ioctl(wf->s, SIOCG80211, &wf->ireq) <0)
				err(1, wf->ireq.i_name);
			break;
		}
	} else
		IEEE80211_ADDR_COPY(wf->mac, mac);
}

/* XXX only fetch what's needed to do reports */
static void
wlan_collect(struct wlanstatfoo_p *wf,
	struct ieee80211_stats *stats, struct ieee80211req_sta_stats *nstats)
{

	IEEE80211_ADDR_COPY(wf->u_info.info.is_u.macaddr, wf->mac);
	wf->ireq.i_type = IEEE80211_IOC_STA_INFO;
	wf->ireq.i_data = (caddr_t) &wf->u_info;
	wf->ireq.i_len = sizeof(wf->u_info);
	if (ioctl(wf->s, SIOCG80211, &wf->ireq) < 0)
		err(1, wf->ireq.i_name);

	IEEE80211_ADDR_COPY(nstats->is_u.macaddr, wf->mac);
	wf->ireq.i_type = IEEE80211_IOC_STA_STATS;
	wf->ireq.i_data = (caddr_t) nstats;
	wf->ireq.i_len = sizeof(*nstats);
	if (ioctl(wf->s, SIOCG80211, &wf->ireq) < 0)
		err(1, wf->ireq.i_name);

	wf->ifr.ifr_data = (caddr_t) stats;
	if (ioctl(wf->s, SIOCG80211STATS, &wf->ifr) < 0)
		err(1, wf->ifr.ifr_name);
}

static void
wlan_collect_cur(struct statfoo *sf)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) sf;

	wlan_collect(wf, &wf->cur, &wf->ncur);
}

static void
wlan_collect_tot(struct statfoo *sf)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) sf;

	wlan_collect(wf, &wf->total, &wf->ntotal);
}

static void
wlan_update_tot(struct statfoo *sf)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) sf;

	wf->total = wf->cur;
	wf->ntotal = wf->ncur;
}

static int
wlan_getinfo(struct wlanstatfoo_p *wf, int s, char b[], size_t bs)
{
	const struct ieee80211req_sta_info *si = &wf->u_info.info.info[0];
	uint8_t r;

	switch (s) {
	case S_RATE:
		r = si->isi_rates[si->isi_txrate];
		snprintf(b, bs, "%uM", (r &~ 0x80) / 2);
		return 1;
	case S_RSSI:
		snprintf(b, bs, "%d", si->isi_rssi);
		return 1;
	case S_NOISE:
		snprintf(b, bs, "%d", si->isi_noise);
		return 1;
	case S_SIGNAL:
		snprintf(b, bs, "%d", si->isi_rssi + si->isi_noise);
		return 1;
	}
	b[0] = '\0';
	return 0;
}

static int
wlan_get_curstat(struct statfoo *sf, int s, char b[], size_t bs)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) sf;
#define	STAT(x) \
	snprintf(b, bs, "%u", wf->cur.is_##x - wf->total.is_##x); return 1
#define	NSTAT(x) \
	snprintf(b, bs, "%u", \
	    wf->ncur.is_stats.ns_##x - wf->ntotal.is_stats.ns_##x); \
	    return 1

	switch (s) {
	case S_RX_BADVERSION:	STAT(rx_badversion);
	case S_RX_TOOSHORT:	STAT(rx_tooshort);
	case S_RX_WRONGBSS:	STAT(rx_wrongbss);
	case S_RX_DUP:		STAT(rx_dup);
	case S_RX_WRONGDIR:	STAT(rx_wrongdir);
	case S_RX_MCASTECHO:	STAT(rx_mcastecho);
	case S_RX_NOTASSOC:	STAT(rx_notassoc);
	case S_RX_NOPRIVACY:	STAT(rx_noprivacy);
	case S_RX_UNENCRYPTED:	STAT(rx_unencrypted);
	case S_RX_WEPFAIL:	STAT(rx_wepfail);
	case S_RX_DECAP:	STAT(rx_decap);
	case S_RX_MGTDISCARD:	STAT(rx_mgtdiscard);
	case S_RX_CTL:		STAT(rx_ctl);
	case S_RX_BEACON:	STAT(rx_beacon);
	case S_RX_RSTOOBIG:	STAT(rx_rstoobig);
	case S_RX_ELEM_MISSING:	STAT(rx_elem_missing);
	case S_RX_ELEM_TOOBIG:	STAT(rx_elem_toobig);
	case S_RX_ELEM_TOOSMALL:	STAT(rx_elem_toosmall);
	case S_RX_ELEM_UNKNOWN:	STAT(rx_elem_unknown);
	case S_RX_BADCHAN:	STAT(rx_badchan);
	case S_RX_CHANMISMATCH:	STAT(rx_chanmismatch);
	case S_RX_NODEALLOC:	STAT(rx_nodealloc);
	case S_RX_SSIDMISMATCH:	STAT(rx_ssidmismatch);
	case S_RX_AUTH_UNSUPPORTED:	STAT(rx_auth_unsupported);
	case S_RX_AUTH_FAIL:	STAT(rx_auth_fail);
	case S_RX_AUTH_COUNTERMEASURES:	STAT(rx_auth_countermeasures);
	case S_RX_ASSOC_BSS:	STAT(rx_assoc_bss);
	case S_RX_ASSOC_NOTAUTH:	STAT(rx_assoc_notauth);
	case S_RX_ASSOC_CAPMISMATCH:	STAT(rx_assoc_capmismatch);
	case S_RX_ASSOC_NORATE:	STAT(rx_assoc_norate);
	case S_RX_ASSOC_BADWPAIE:	STAT(rx_assoc_badwpaie);
	case S_RX_DEAUTH:	STAT(rx_deauth);
	case S_RX_DISASSOC:	STAT(rx_disassoc);
	case S_RX_BADSUBTYPE:	STAT(rx_badsubtype);
	case S_RX_NOBUF:	STAT(rx_nobuf);
	case S_RX_DECRYPTCRC:	STAT(rx_decryptcrc);
	case S_RX_AHDEMO_MGT:	STAT(rx_ahdemo_mgt);
	case S_RX_BAD_AUTH:	STAT(rx_bad_auth);
	case S_RX_UNAUTH:	STAT(rx_unauth);
	case S_RX_BADKEYID:	STAT(rx_badkeyid);
	case S_RX_CCMPREPLAY:	STAT(rx_ccmpreplay);
	case S_RX_CCMPFORMAT:	STAT(rx_ccmpformat);
	case S_RX_CCMPMIC:	STAT(rx_ccmpmic);
	case S_RX_TKIPREPLAY:	STAT(rx_tkipreplay);
	case S_RX_TKIPFORMAT:	STAT(rx_tkipformat);
	case S_RX_TKIPMIC:	STAT(rx_tkipmic);
	case S_RX_TKIPICV:	STAT(rx_tkipicv);
	case S_RX_BADCIPHER:	STAT(rx_badcipher);
	case S_RX_NOCIPHERCTX:	STAT(rx_nocipherctx);
	case S_RX_ACL:		STAT(rx_acl);
	case S_TX_NOBUF:	STAT(tx_nobuf);
	case S_TX_NONODE:	STAT(tx_nonode);
	case S_TX_UNKNOWNMGT:	STAT(tx_unknownmgt);
	case S_TX_BADCIPHER:	STAT(tx_badcipher);
	case S_TX_NODEFKEY:	STAT(tx_nodefkey);
	case S_TX_NOHEADROOM:	STAT(tx_noheadroom);
	case S_TX_FRAGFRAMES:	STAT(tx_fragframes);
	case S_TX_FRAGS:	STAT(tx_frags);
	case S_SCAN_ACTIVE:	STAT(scan_active);
	case S_SCAN_PASSIVE:	STAT(scan_passive);
	case S_NODE_TIMEOUT:	STAT(node_timeout);
	case S_CRYPTO_NOMEM:	STAT(crypto_nomem);
	case S_CRYPTO_TKIP:	STAT(crypto_tkip);
	case S_CRYPTO_TKIPENMIC:	STAT(crypto_tkipenmic);
	case S_CRYPTO_TKIPDEMIC:	STAT(crypto_tkipdemic);
	case S_CRYPTO_TKIPCM:	STAT(crypto_tkipcm);
	case S_CRYPTO_CCMP:	STAT(crypto_ccmp);
	case S_CRYPTO_WEP:	STAT(crypto_wep);
	case S_CRYPTO_SETKEY_CIPHER:	STAT(crypto_setkey_cipher);
	case S_CRYPTO_SETKEY_NOKEY:	STAT(crypto_setkey_nokey);
	case S_CRYPTO_DELKEY:	STAT(crypto_delkey);
	case S_CRYPTO_BADCIPHER:	STAT(crypto_badcipher);
	case S_CRYPTO_NOCIPHER:	STAT(crypto_nocipher);
	case S_CRYPTO_ATTACHFAIL:	STAT(crypto_attachfail);
	case S_CRYPTO_SWFALLBACK:	STAT(crypto_swfallback);
	case S_CRYPTO_KEYFAIL:	STAT(crypto_keyfail);
	case S_CRYPTO_ENMICFAIL:	STAT(crypto_enmicfail);
	case S_IBSS_CAPMISMATCH:	STAT(ibss_capmismatch);
	case S_IBSS_NORATE:	STAT(ibss_norate);
	case S_PS_UNASSOC:	STAT(ps_unassoc);
	case S_PS_BADAID:	STAT(ps_badaid);
	case S_PS_QEMPTY:	STAT(ps_qempty);
	case S_FF_BADHDR:	STAT(ff_badhdr);
	case S_FF_TOOSHORT:	STAT(ff_tooshort);
	case S_FF_SPLIT:	STAT(ff_split);
	case S_FF_DECAP:	STAT(ff_decap);
	case S_FF_ENCAP:	STAT(ff_encap);
	case S_RX_BADBINTVAL:	STAT(rx_badbintval);
	case S_RX_MGMT:		STAT(rx_mgmt);
	case S_RX_DEMICFAIL:	STAT(rx_demicfail);
	case S_RX_DEFRAG:	STAT(rx_defrag);
	case S_INPUT:		NSTAT(rx_data);
	case S_OUTPUT:		NSTAT(tx_data);
	case S_RX_UCAST:	NSTAT(rx_ucast);
	case S_RX_MCAST:	NSTAT(rx_mcast);
	case S_TX_UCAST:	NSTAT(tx_ucast);
	case S_TX_MCAST:	NSTAT(tx_mcast);
	}
	return wlan_getinfo(wf, s, b, bs);
#undef NSTAT
#undef STAT
}

static int
wlan_get_totstat(struct statfoo *sf, int s, char b[], size_t bs)
{
	struct wlanstatfoo_p *wf = (struct wlanstatfoo_p *) sf;
#define	STAT(x) \
	snprintf(b, bs, "%u", wf->total.is_##x); return 1
#define	NSTAT(x) \
	snprintf(b, bs, "%u", wf->ntotal.is_stats.ns_##x); return 1

	switch (s) {
	case S_RX_BADVERSION:	STAT(rx_badversion);
	case S_RX_TOOSHORT:	STAT(rx_tooshort);
	case S_RX_WRONGBSS:	STAT(rx_wrongbss);
	case S_RX_DUP:	STAT(rx_dup);
	case S_RX_WRONGDIR:	STAT(rx_wrongdir);
	case S_RX_MCASTECHO:	STAT(rx_mcastecho);
	case S_RX_NOTASSOC:	STAT(rx_notassoc);
	case S_RX_NOPRIVACY:	STAT(rx_noprivacy);
	case S_RX_UNENCRYPTED:	STAT(rx_unencrypted);
	case S_RX_WEPFAIL:	STAT(rx_wepfail);
	case S_RX_DECAP:	STAT(rx_decap);
	case S_RX_MGTDISCARD:	STAT(rx_mgtdiscard);
	case S_RX_CTL:		STAT(rx_ctl);
	case S_RX_BEACON:	STAT(rx_beacon);
	case S_RX_RSTOOBIG:	STAT(rx_rstoobig);
	case S_RX_ELEM_MISSING:	STAT(rx_elem_missing);
	case S_RX_ELEM_TOOBIG:	STAT(rx_elem_toobig);
	case S_RX_ELEM_TOOSMALL:	STAT(rx_elem_toosmall);
	case S_RX_ELEM_UNKNOWN:	STAT(rx_elem_unknown);
	case S_RX_BADCHAN:	STAT(rx_badchan);
	case S_RX_CHANMISMATCH:	STAT(rx_chanmismatch);
	case S_RX_NODEALLOC:	STAT(rx_nodealloc);
	case S_RX_SSIDMISMATCH:	STAT(rx_ssidmismatch);
	case S_RX_AUTH_UNSUPPORTED:	STAT(rx_auth_unsupported);
	case S_RX_AUTH_FAIL:	STAT(rx_auth_fail);
	case S_RX_AUTH_COUNTERMEASURES:	STAT(rx_auth_countermeasures);
	case S_RX_ASSOC_BSS:	STAT(rx_assoc_bss);
	case S_RX_ASSOC_NOTAUTH:	STAT(rx_assoc_notauth);
	case S_RX_ASSOC_CAPMISMATCH:	STAT(rx_assoc_capmismatch);
	case S_RX_ASSOC_NORATE:	STAT(rx_assoc_norate);
	case S_RX_ASSOC_BADWPAIE:	STAT(rx_assoc_badwpaie);
	case S_RX_DEAUTH:	STAT(rx_deauth);
	case S_RX_DISASSOC:	STAT(rx_disassoc);
	case S_RX_BADSUBTYPE:	STAT(rx_badsubtype);
	case S_RX_NOBUF:	STAT(rx_nobuf);
	case S_RX_DECRYPTCRC:	STAT(rx_decryptcrc);
	case S_RX_AHDEMO_MGT:	STAT(rx_ahdemo_mgt);
	case S_RX_BAD_AUTH:	STAT(rx_bad_auth);
	case S_RX_UNAUTH:	STAT(rx_unauth);
	case S_RX_BADKEYID:	STAT(rx_badkeyid);
	case S_RX_CCMPREPLAY:	STAT(rx_ccmpreplay);
	case S_RX_CCMPFORMAT:	STAT(rx_ccmpformat);
	case S_RX_CCMPMIC:	STAT(rx_ccmpmic);
	case S_RX_TKIPREPLAY:	STAT(rx_tkipreplay);
	case S_RX_TKIPFORMAT:	STAT(rx_tkipformat);
	case S_RX_TKIPMIC:	STAT(rx_tkipmic);
	case S_RX_TKIPICV:	STAT(rx_tkipicv);
	case S_RX_BADCIPHER:	STAT(rx_badcipher);
	case S_RX_NOCIPHERCTX:	STAT(rx_nocipherctx);
	case S_RX_ACL:		STAT(rx_acl);
	case S_TX_NOBUF:	STAT(tx_nobuf);
	case S_TX_NONODE:	STAT(tx_nonode);
	case S_TX_UNKNOWNMGT:	STAT(tx_unknownmgt);
	case S_TX_BADCIPHER:	STAT(tx_badcipher);
	case S_TX_NODEFKEY:	STAT(tx_nodefkey);
	case S_TX_NOHEADROOM:	STAT(tx_noheadroom);
	case S_TX_FRAGFRAMES:	STAT(tx_fragframes);
	case S_TX_FRAGS:	STAT(tx_frags);
	case S_SCAN_ACTIVE:	STAT(scan_active);
	case S_SCAN_PASSIVE:	STAT(scan_passive);
	case S_NODE_TIMEOUT:	STAT(node_timeout);
	case S_CRYPTO_NOMEM:	STAT(crypto_nomem);
	case S_CRYPTO_TKIP:	STAT(crypto_tkip);
	case S_CRYPTO_TKIPENMIC:	STAT(crypto_tkipenmic);
	case S_CRYPTO_TKIPDEMIC:	STAT(crypto_tkipdemic);
	case S_CRYPTO_TKIPCM:	STAT(crypto_tkipcm);
	case S_CRYPTO_CCMP:	STAT(crypto_ccmp);
	case S_CRYPTO_WEP:	STAT(crypto_wep);
	case S_CRYPTO_SETKEY_CIPHER:	STAT(crypto_setkey_cipher);
	case S_CRYPTO_SETKEY_NOKEY:	STAT(crypto_setkey_nokey);
	case S_CRYPTO_DELKEY:	STAT(crypto_delkey);
	case S_CRYPTO_BADCIPHER:	STAT(crypto_badcipher);
	case S_CRYPTO_NOCIPHER:	STAT(crypto_nocipher);
	case S_CRYPTO_ATTACHFAIL:	STAT(crypto_attachfail);
	case S_CRYPTO_SWFALLBACK:	STAT(crypto_swfallback);
	case S_CRYPTO_KEYFAIL:	STAT(crypto_keyfail);
	case S_CRYPTO_ENMICFAIL:	STAT(crypto_enmicfail);
	case S_IBSS_CAPMISMATCH:	STAT(ibss_capmismatch);
	case S_IBSS_NORATE:	STAT(ibss_norate);
	case S_PS_UNASSOC:	STAT(ps_unassoc);
	case S_PS_BADAID:	STAT(ps_badaid);
	case S_PS_QEMPTY:	STAT(ps_qempty);
	case S_FF_BADHDR:	STAT(ff_badhdr);
	case S_FF_TOOSHORT:	STAT(ff_tooshort);
	case S_FF_SPLIT:	STAT(ff_split);
	case S_FF_DECAP:	STAT(ff_decap);
	case S_FF_ENCAP:	STAT(ff_encap);
	case S_RX_BADBINTVAL:	STAT(rx_badbintval);
	case S_RX_MGMT:		STAT(rx_mgmt);
	case S_RX_DEMICFAIL:	STAT(rx_demicfail);
	case S_RX_DEFRAG:	STAT(rx_defrag);
	case S_INPUT:		NSTAT(rx_data);
	case S_OUTPUT:		NSTAT(tx_data);
	case S_RX_UCAST:	NSTAT(rx_ucast);
	case S_RX_MCAST:	NSTAT(rx_mcast);
	case S_TX_UCAST:	NSTAT(tx_ucast);
	case S_TX_MCAST:	NSTAT(tx_mcast);
	}
	return wlan_getinfo(wf, s, b, bs);
#undef NSTAT
#undef STAT
}

STATFOO_DEFINE_BOUNCE(wlanstatfoo)

struct wlanstatfoo *
wlanstats_new(const char *ifname, const char *fmtstring)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	struct wlanstatfoo_p *wf;

	wf = calloc(1, sizeof(struct wlanstatfoo_p));
	if (wf != NULL) {
		statfoo_init(&wf->base.base, "wlanstats", wlanstats, N(wlanstats));
		/* override base methods */
		wf->base.base.collect_cur = wlan_collect_cur;
		wf->base.base.collect_tot = wlan_collect_tot;
		wf->base.base.get_curstat = wlan_get_curstat;
		wf->base.base.get_totstat = wlan_get_totstat;
		wf->base.base.update_tot = wlan_update_tot;

		/* setup bounce functions for public methods */
		STATFOO_BOUNCE(wf, wlanstatfoo);

		/* setup our public methods */
		wf->base.setifname = wlan_setifname;
		wf->base.getifname = wlan_getifname;
		wf->base.getopmode = wlan_getopmode;
		wf->base.setstamac = wlan_setstamac;
		wf->opmode = -1;

		wf->s = socket(AF_INET, SOCK_DGRAM, 0);
		if (wf->s < 0)
			err(1, "socket");

		wlan_setifname(&wf->base, ifname);
		wf->base.setfmt(&wf->base, fmtstring);
	}
	return &wf->base;
#undef N
}
