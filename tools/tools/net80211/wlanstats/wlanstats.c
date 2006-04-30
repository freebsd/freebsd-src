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
 *
 * $FreeBSD$
 */

/*
 * wlanstats [-i interface]
 * (default interface is ath0).
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/ethernet.h>

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>

#include "../../../../sys/net80211/ieee80211_ioctl.h"

const char *progname;

static void
printstats(FILE *fd, const struct ieee80211_stats *stats)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
#define	STAT(x,fmt) \
	if (stats->is_##x) fprintf(fd, "%u " fmt "\n", stats->is_##x)
	STAT(rx_badversion,	"rx frame with bad version");
	STAT(rx_tooshort,	"rx frame too short");
	STAT(rx_wrongbss,	"rx from wrong bssid");
	STAT(rx_dup,		"rx discard 'cuz dup");
	STAT(rx_wrongdir,	"rx w/ wrong direction");
	STAT(rx_mcastecho,	"rx discard 'cuz mcast echo");
	STAT(rx_notassoc,	"rx discard 'cuz sta !assoc");
	STAT(rx_noprivacy,	"rx w/ wep but privacy off");
	STAT(rx_unencrypted,	"rx w/o wep and privacy on");
	STAT(rx_wepfail,	"rx wep processing failed");
	STAT(rx_decap,		"rx decapsulation failed");
	STAT(rx_mgtdiscard,	"rx discard mgt frames");
	STAT(rx_ctl,		"rx discard ctrl frames");
	STAT(rx_beacon,		"rx beacon frames");
	STAT(rx_rstoobig,	"rx rate set truncated");
	STAT(rx_elem_missing,	"rx required element missing");
	STAT(rx_elem_toobig,	"rx element too big");
	STAT(rx_elem_toosmall,	"rx element too small");
	STAT(rx_elem_unknown,	"rx element unknown");
	STAT(rx_badchan,	"rx frame w/ invalid chan");
	STAT(rx_chanmismatch,	"rx frame chan mismatch");
	STAT(rx_nodealloc,	"nodes allocated (rx)");
	STAT(rx_ssidmismatch,	"rx frame ssid mismatch");
	STAT(rx_auth_unsupported,"rx w/ unsupported auth alg");
	STAT(rx_auth_fail,	"rx sta auth failure");
	STAT(rx_auth_countermeasures,
		"rx sta auth failure 'cuz of TKIP countermeasures");
	STAT(rx_assoc_bss,	"rx assoc from wrong bssid");
	STAT(rx_assoc_notauth,	"rx assoc w/o auth");
	STAT(rx_assoc_capmismatch,"rx assoc w/ cap mismatch");
	STAT(rx_assoc_norate,	"rx assoc w/ no rate match");
	STAT(rx_assoc_badwpaie,	"rx assoc w/ bad WPA IE");
	STAT(rx_deauth,		"rx deauthentication");
	STAT(rx_disassoc,	"rx disassociation");
	STAT(rx_badsubtype,	"rx frame w/ unknown subtype");
	STAT(rx_nobuf,		"rx failed for lack of sk_buffer");
	STAT(rx_decryptcrc,	"rx decrypt failed on crc");
	STAT(rx_ahdemo_mgt,
		"rx discard mgmt frame received in ahdoc demo mode");
	STAT(rx_bad_auth,	"rx bad authentication request");
	STAT(rx_unauth,		"rx discard 'cuz port unauthorized");
	STAT(rx_badkeyid,	"rx w/ incorrect keyid");
	STAT(rx_ccmpreplay,	"rx seq# violation (CCMP)");
	STAT(rx_ccmpformat,	"rx format bad (CCMP)");
	STAT(rx_ccmpmic,	"rx MIC check failed (CCMP)");
	STAT(rx_tkipreplay,	"rx seq# violation (TKIP)");
	STAT(rx_tkipformat,	"rx format bad (TKIP)");
	STAT(rx_tkipmic,	"rx MIC check failed (TKIP)");
	STAT(rx_tkipicv,	"rx ICV check failed (TKIP)");
	STAT(rx_badcipher,	"rx failed 'cuz bad cipher/key type");
	STAT(rx_nocipherctx,	"rx failed 'cuz key/cipher ctx not setup");
	STAT(rx_acl,		"rx discard 'cuz acl policy");
	STAT(tx_nobuf,		"tx failed for lack of sk_buffer");
	STAT(tx_nonode,		"tx failed for no node");
	STAT(tx_unknownmgt,	"tx of unknown mgt frame");
	STAT(tx_badcipher,	"tx failed 'cuz bad ciper/key type");
	STAT(tx_nodefkey,	"tx failed 'cuz no defkey");
	STAT(tx_noheadroom,	"tx failed 'cuz no space for crypto hdrs");
	STAT(tx_fragframes,	"tx frames fragmented");
	STAT(tx_frags,		"tx frags generated");
	STAT(scan_active,	"active scans started");
	STAT(scan_passive,	"passive scans started");
	STAT(node_timeout,	"nodes timed out inactivity");
	STAT(crypto_nomem,	"cipher context malloc failed");
	STAT(crypto_tkip,	"tkip crypto done in s/w");
	STAT(crypto_tkipenmic,	"tkip tx MIC done in s/w");
	STAT(crypto_tkipdemic,	"tkip rx MIC done in s/w");
	STAT(crypto_tkipcm,	"tkip dropped frames 'cuz of countermeasures");
	STAT(crypto_ccmp,	"ccmp crypto done in s/w");
	STAT(crypto_wep,	"wep crypto done in s/w");
	STAT(crypto_setkey_cipher,"setkey failed 'cuz cipher rejected data");
	STAT(crypto_setkey_nokey,"setkey failed 'cuz no key index");
	STAT(crypto_delkey,	"driver key delete failed");
	STAT(crypto_badcipher,	"setkey failed 'cuz unknown cipher");
	STAT(crypto_nocipher,	"setkey failed 'cuz cipher module unavailable");
	STAT(crypto_attachfail,	"setkey failed 'cuz cipher attach failed");
	STAT(crypto_swfallback,	"crypto fell back to s/w implementation");
	STAT(crypto_keyfail,	"setkey failed 'cuz driver key alloc failed");
	STAT(crypto_enmicfail,	"enmic failed (may be mbuf exhaustion)");
	STAT(ibss_capmismatch,	"ibss merge faied 'cuz capabilities mismatch");
	STAT(ibss_norate,	"ibss merge faied 'cuz rate set mismatch");
	STAT(ps_unassoc,	"ps-poll received for unassociated station");
	STAT(ps_badaid,		"ps-poll received with invalid association id");
	STAT(ps_qempty,		"ps-poll received with nothing to send");
	STAT(ff_badhdr,		"fast frame rx'd w/ bad hdr");
	STAT(ff_tooshort,	"fast frame rx decap error");
	STAT(ff_split,		"fast frame rx split error");
	STAT(ff_decap,		"fast frames decap'd");
	STAT(ff_encap,		"fast frames encap'd for tx");
#undef STAT
#undef N
}

struct ifreq ifr;
int	s;

static void
print_sta_stats(FILE *fd, const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
#define	STAT(x,fmt) \
	if (ns->ns_##x) { fprintf(fd, "%s" #x " " fmt, sep, ns->ns_##x); sep = " "; }
	struct ieee80211req ireq;
	struct ieee80211req_sta_stats stats;
	const struct ieee80211_nodestats *ns = &stats.is_stats;
	const char *sep;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, ifr.ifr_name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_STA_STATS;
	ireq.i_data = &stats;
	ireq.i_len = sizeof(stats);
	memcpy(stats.is_u.macaddr, macaddr, IEEE80211_ADDR_LEN);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		err(1, "unable to get station stats for %s",
			ether_ntoa((const struct ether_addr*) macaddr));

	fprintf(fd, "%s:\n", ether_ntoa((const struct ether_addr*) macaddr));

	sep = "\t";
	STAT(rx_data, "%u");
	STAT(rx_mgmt, "%u");
	STAT(rx_ctrl, "%u");
	STAT(rx_beacons, "%u");
	STAT(rx_proberesp, "%u");
	STAT(rx_ucast, "%u");
	STAT(rx_mcast, "%u");
	STAT(rx_bytes, "%llu");
	STAT(rx_dup, "%u");
	STAT(rx_noprivacy, "%u");
	STAT(rx_wepfail, "%u");
	STAT(rx_demicfail, "%u");
	STAT(rx_decap, "%u");
	STAT(rx_defrag, "%u");
	STAT(rx_disassoc, "%u");
	STAT(rx_deauth, "%u");
	STAT(rx_decryptcrc, "%u");
	STAT(rx_unauth, "%u");
	STAT(rx_unencrypted, "%u");
	fprintf(fd, "\n");

	sep = "\t";
	STAT(tx_data, "%u");
	STAT(tx_mgmt, "%u");
	STAT(tx_probereq, "%u");
	STAT(tx_ucast, "%u");
	STAT(tx_mcast, "%u");
	STAT(tx_bytes, "%llu");
	STAT(tx_novlantag, "%u");
	STAT(tx_vlanmismatch, "%u");
	fprintf(fd, "\n");

	sep = "\t";
	STAT(tx_assoc, "%u");
	STAT(tx_assoc_fail, "%u");
	STAT(tx_auth, "%u");
	STAT(tx_auth_fail, "%u");
	STAT(tx_deauth, "%u");
	STAT(tx_deauth_code, "%llu");
	STAT(tx_disassoc, "%u");
	STAT(tx_disassoc_code, "%u");
	fprintf(fd, "\n");

#undef STAT
}

int
main(int argc, char *argv[])
{
	int c, len;
	struct ieee80211req_sta_info *si;
	uint8_t buf[24*1024], *cp;
	struct ieee80211req ireq;
	int allnodes = 0;

	progname = argv[0];

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket");
	strncpy(ifr.ifr_name, "ath0", sizeof (ifr.ifr_name));
	while ((c = getopt(argc, argv, "ai:")) != -1)
		switch (c) {
		case 'a':
			allnodes++;
			break;
		case 'i':
			strncpy(ifr.ifr_name, optarg, sizeof (ifr.ifr_name));
			break;
		default:
			errx(1, "usage: %s [-a] [-i device] [mac...]\n", progname);
			/*NOTREACHED*/
		}

	if (argc == optind && !allnodes) {
		struct ieee80211_stats stats;

		/* no args, just show global stats */
		ifr.ifr_data = (caddr_t) &stats;
		if (ioctl(s, SIOCG80211STATS, &ifr) < 0)
			err(1, ifr.ifr_name);
		printstats(stdout, &stats);
		return 0;
	}
	if (allnodes) {
		/*
		 * Retrieve station/neighbor table and print stats for each.
		 */
		(void) memset(&ireq, 0, sizeof(ireq));
		(void) strncpy(ireq.i_name, ifr.ifr_name, sizeof(ireq.i_name));
		ireq.i_type = IEEE80211_IOC_STA_INFO;
		ireq.i_data = buf;
		ireq.i_len = sizeof(buf);
		if (ioctl(s, SIOCG80211, &ireq) < 0)
			err(1, "unable to get station information");
		len = ireq.i_len;
		if (len >= sizeof(struct ieee80211req_sta_info)) {
			cp = buf;
			do {
				si = (struct ieee80211req_sta_info *) cp;
				print_sta_stats(stdout, si->isi_macaddr);
				cp += si->isi_len, len -= si->isi_len;
			} while (len >= sizeof(struct ieee80211req_sta_info));
		}
	} else {
		/*
		 * Print stats for specified stations.
		 */
		for (c = optind; c < argc; c++) {
			const struct ether_addr *ea = ether_aton(argv[c]);
			if (ea != NULL)
				print_sta_stats(stdout, ea->octet);
		}
	}
}
