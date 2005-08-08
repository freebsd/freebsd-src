/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * IEEE 802.11 ioctl support (FreeBSD-specific)
 */

#include "opt_inet.h"
#include "opt_ipx.h"

#include <sys/endian.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
 
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <dev/wi/if_wavelan_ieee.h>

#define	IS_UP(_ic) \
	(((_ic)->ic_ifp->if_flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define	IS_UP_AUTO(_ic) \
	(IS_UP(_ic) && (_ic)->ic_roaming == IEEE80211_ROAMING_AUTO)

/*
 * XXX
 * Wireless LAN specific configuration interface, which is compatible
 * with wicontrol(8).
 */

struct wi_read_ap_args {
	int	i;		/* result count */
	struct wi_apinfo *ap;	/* current entry in result buffer */
	caddr_t	max;		/* result buffer bound */
};

static void
wi_read_ap_result(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct wi_read_ap_args *sa = arg;
	struct wi_apinfo *ap = sa->ap;
	struct ieee80211_rateset *rs;
	int j;

	if ((caddr_t)(ap + 1) > sa->max)
		return;
	memset(ap, 0, sizeof(struct wi_apinfo));
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		IEEE80211_ADDR_COPY(ap->bssid, ni->ni_macaddr);
		ap->namelen = ic->ic_des_esslen;
		if (ic->ic_des_esslen)
			memcpy(ap->name, ic->ic_des_essid,
			    ic->ic_des_esslen);
	} else {
		IEEE80211_ADDR_COPY(ap->bssid, ni->ni_bssid);
		ap->namelen = ni->ni_esslen;
		if (ni->ni_esslen)
			memcpy(ap->name, ni->ni_essid,
			    ni->ni_esslen);
	}
	ap->channel = ieee80211_chan2ieee(ic, ni->ni_chan);
	ap->signal = ic->ic_node_getrssi(ni);
	ap->capinfo = ni->ni_capinfo;
	ap->interval = ni->ni_intval;
	rs = &ni->ni_rates;
	for (j = 0; j < rs->rs_nrates; j++) {
		if (rs->rs_rates[j] & IEEE80211_RATE_BASIC) {
			ap->rate = (rs->rs_rates[j] &
			    IEEE80211_RATE_VAL) * 5; /* XXX */
		}
	}
	sa->i++;
	sa->ap++;
}

struct wi_read_prism2_args {
	int	i;		/* result count */
	struct wi_scan_res *res;/* current entry in result buffer */
	caddr_t	max;		/* result buffer bound */
};

static void
wi_read_prism2_result(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct wi_read_prism2_args *sa = arg;
	struct wi_scan_res *res = sa->res;

	if ((caddr_t)(res + 1) > sa->max)
		return;
	res->wi_chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	res->wi_noise = 0;
	res->wi_signal = ic->ic_node_getrssi(ni);
	IEEE80211_ADDR_COPY(res->wi_bssid, ni->ni_bssid);
	res->wi_interval = ni->ni_intval;
	res->wi_capinfo = ni->ni_capinfo;
	res->wi_ssid_len = ni->ni_esslen;
	memcpy(res->wi_ssid, ni->ni_essid, IEEE80211_NWID_LEN);
	/* NB: assumes wi_srates holds <= ni->ni_rates */
	memcpy(res->wi_srates, ni->ni_rates.rs_rates,
		sizeof(res->wi_srates));
	if (ni->ni_rates.rs_nrates < 10)
		res->wi_srates[ni->ni_rates.rs_nrates] = 0;
	res->wi_rate = ni->ni_rates.rs_rates[ni->ni_txrate];
	res->wi_rsvd = 0;

	sa->i++;
	sa->res++;
}

struct wi_read_sigcache_args {
	int	i;		/* result count */
	struct wi_sigcache *wsc;/* current entry in result buffer */
	caddr_t	max;		/* result buffer bound */
};

static void
wi_read_sigcache(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct wi_read_sigcache_args *sa = arg;
	struct wi_sigcache *wsc = sa->wsc;

	if ((caddr_t)(wsc + 1) > sa->max)
		return;
	memset(wsc, 0, sizeof(struct wi_sigcache));
	IEEE80211_ADDR_COPY(wsc->macsrc, ni->ni_macaddr);
	wsc->signal = ic->ic_node_getrssi(ni);

	sa->wsc++;
	sa->i++;
}

int
ieee80211_cfgget(struct ieee80211com *ic, u_long cmd, caddr_t data)
{
	struct ifnet *ifp = ic->ic_ifp;
	int i, j, error;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_req wreq;
	struct wi_ltv_keys *keys;

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	wreq.wi_len = 0;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
		/* nothing appropriate */
		break;
	case WI_RID_NODENAME:
		strcpy((char *)&wreq.wi_val[1], hostname);
		wreq.wi_val[0] = htole16(strlen(hostname));
		wreq.wi_len = (1 + strlen(hostname) + 1) / 2;
		break;
	case WI_RID_CURRENT_SSID:
		if (ic->ic_state != IEEE80211_S_RUN) {
			wreq.wi_val[0] = 0;
			wreq.wi_len = 1;
			break;
		}
		wreq.wi_val[0] = htole16(ic->ic_bss->ni_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_bss->ni_essid,
		    ic->ic_bss->ni_esslen);
		wreq.wi_len = (1 + ic->ic_bss->ni_esslen + 1) / 2;
		break;
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		wreq.wi_val[0] = htole16(ic->ic_des_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_des_essid, ic->ic_des_esslen);
		wreq.wi_len = (1 + ic->ic_des_esslen + 1) / 2;
		break;
	case WI_RID_CURRENT_BSSID:
		if (ic->ic_state == IEEE80211_S_RUN)
			IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_bss->ni_bssid);
		else
			memset(wreq.wi_val, 0, IEEE80211_ADDR_LEN);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_CHANNEL_LIST:
		memset(wreq.wi_val, 0, sizeof(wreq.wi_val));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++)
			if (isset(ic->ic_chan_active, i)) {
				setbit((u_int8_t *)wreq.wi_val, j);
				wreq.wi_len = j / 16 + 1;
			}
		break;
	case WI_RID_OWN_CHNL:
		wreq.wi_val[0] = htole16(
			ieee80211_chan2ieee(ic, ic->ic_ibss_chan));
		wreq.wi_len = 1;
		break;
	case WI_RID_CURRENT_CHAN:
		wreq.wi_val[0] = htole16(
			ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan));
		wreq.wi_len = 1;
		break;
	case WI_RID_COMMS_QUALITY:
		wreq.wi_val[0] = 0;				/* quality */
		wreq.wi_val[1] = htole16(ic->ic_node_getrssi(ic->ic_bss));
		wreq.wi_val[2] = 0;				/* noise */
		wreq.wi_len = 3;
		break;
	case WI_RID_PROMISC:
		wreq.wi_val[0] = htole16((ifp->if_flags & IFF_PROMISC) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_PORTTYPE:
		wreq.wi_val[0] = htole16(ic->ic_opmode);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAC_NODE:
		IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_myaddr);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_TX_RATE:
		if (ic->ic_fixed_rate == IEEE80211_FIXED_RATE_NONE)
			wreq.wi_val[0] = 0;	/* auto */
		else
			wreq.wi_val[0] = htole16(
			    (ic->ic_sup_rates[ic->ic_curmode].rs_rates[ic->ic_fixed_rate] &
			    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_TX_RATE:
		wreq.wi_val[0] = htole16(
		    (ic->ic_bss->ni_rates.rs_rates[ic->ic_bss->ni_txrate] &
		    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_RTS_THRESH:
		wreq.wi_val[0] = htole16(ic->ic_rtsthreshold);
		wreq.wi_len = 1;
		break;
	case WI_RID_CREATE_IBSS:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_IBSSON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MICROWAVE_OVEN:
		wreq.wi_val[0] = 0;	/* no ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_ROAMING_MODE:
		wreq.wi_val[0] = htole16(ic->ic_roaming);	/* XXX map */
		wreq.wi_len = 1;
		break;
	case WI_RID_SYSTEM_SCALE:
		wreq.wi_val[0] = htole16(1);	/* low density ... not supp */
		wreq.wi_len = 1;
		break;
	case WI_RID_PM_ENABLED:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAX_SLEEP:
		wreq.wi_val[0] = htole16(ic->ic_lintval);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_BEACON_INT:
		wreq.wi_val[0] = htole16(ic->ic_bss->ni_intval);
		wreq.wi_len = 1;
		break;
	case WI_RID_WEP_AVAIL:
		wreq.wi_val[0] = htole16(1);	/* always available */
		wreq.wi_len = 1;
		break;
	case WI_RID_CNFAUTHMODE:
		wreq.wi_val[0] = htole16(1);	/* TODO: open system only */
		wreq.wi_len = 1;
		break;
	case WI_RID_ENCRYPTION:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_PRIVACY) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_TX_CRYPT_KEY:
		wreq.wi_val[0] = htole16(ic->ic_def_txkey);
		wreq.wi_len = 1;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		keys = (struct wi_ltv_keys *)&wreq;
		/* do not show keys to non-root user */
		error = suser(curthread);
		if (error) {
			memset(keys, 0, sizeof(*keys));
			error = 0;
			break;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			keys->wi_keys[i].wi_keylen =
			    htole16(ic->ic_nw_keys[i].wk_keylen);
			memcpy(keys->wi_keys[i].wi_keydat,
			    ic->ic_nw_keys[i].wk_key,
			    ic->ic_nw_keys[i].wk_keylen);
		}
		wreq.wi_len = sizeof(*keys) / 2;
		break;
	case WI_RID_MAX_DATALEN:
		wreq.wi_val[0] = htole16(ic->ic_fragthreshold);
		wreq.wi_len = 1;
		break;
	case WI_RID_IFACE_STATS:
		/* XXX: should be implemented in lower drivers */
		break;
	case WI_RID_READ_APS:
		/*
		 * Don't return results until active scan completes.
		 */
		if ((ic->ic_flags & (IEEE80211_F_SCAN|IEEE80211_F_ASCAN)) == 0) {
			struct wi_read_ap_args args;

			args.i = 0;
			args.ap = (void *)((char *)wreq.wi_val + sizeof(i));
			args.max = (void *)(&wreq + 1);
			ieee80211_iterate_nodes(&ic->ic_scan,
				wi_read_ap_result, &args);
			memcpy(wreq.wi_val, &args.i, sizeof(args.i));
			wreq.wi_len = (sizeof(int) +
				sizeof(struct wi_apinfo) * args.i) / 2;
		} else
			error = EINPROGRESS;
		break;
	case WI_RID_PRISM2:
		/* NB: we lie so WI_RID_SCAN_RES can include rates */
		wreq.wi_val[0] = 1;
		wreq.wi_len = sizeof(u_int16_t) / 2;
		break;
	case WI_RID_SCAN_RES:			/* compatibility interface */
		if ((ic->ic_flags & (IEEE80211_F_SCAN|IEEE80211_F_ASCAN)) == 0) {
			struct wi_read_prism2_args args;
			struct wi_scan_p2_hdr *p2;

			/* NB: use Prism2 format so we can include rate info */
			p2 = (struct wi_scan_p2_hdr *)wreq.wi_val;
			args.i = 0;
			args.res = (void *)&p2[1];
			args.max = (void *)(&wreq + 1);
			ieee80211_iterate_nodes(&ic->ic_scan,
				wi_read_prism2_result, &args);
			p2->wi_rsvd = 0;
			p2->wi_reason = args.i;
			wreq.wi_len = (sizeof(*p2) +
				sizeof(struct wi_scan_res) * args.i) / 2;
		} else
			error = EINPROGRESS;
		break;
	case WI_RID_READ_CACHE: {
		struct wi_read_sigcache_args args;
		args.i = 0;
		args.wsc = (struct wi_sigcache *) wreq.wi_val;
		args.max = (void *)(&wreq + 1);
		ieee80211_iterate_nodes(&ic->ic_scan, wi_read_sigcache, &args);
		wreq.wi_len = sizeof(struct wi_sigcache) * args.i / 2;
		break;
	}
	default:
		error = EINVAL;
		break;
	}
	if (error == 0) {
		wreq.wi_len++;
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
	}
	return error;
}

static int
findrate(struct ieee80211com *ic, enum ieee80211_phymode mode, int rate)
{
#define	IEEERATE(_ic,_m,_i) \
	((_ic)->ic_sup_rates[_m].rs_rates[_i] & IEEE80211_RATE_VAL)
	int i, nrates = ic->ic_sup_rates[mode].rs_nrates;
	for (i = 0; i < nrates; i++)
		if (IEEERATE(ic, mode, i) == rate)
			return i;
	return -1;
#undef IEEERATE
}

/*
 * Prepare to do a user-initiated scan for AP's.  If no
 * current/default channel is setup or the current channel
 * is invalid then pick the first available channel from
 * the active list as the place to start the scan.
 */
static int
ieee80211_setupscan(struct ieee80211com *ic, const u_int8_t chanlist[])
{
	int i;

	/*
	 * XXX don't permit a scan to be started unless we
	 * know the device is ready.  For the moment this means
	 * the device is marked up as this is the required to
	 * initialize the hardware.  It would be better to permit
	 * scanning prior to being up but that'll require some
	 * changes to the infrastructure.
	 */
	if (!IS_UP(ic))
		return EINVAL;
	if (ic->ic_ibss_chan == NULL ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_ibss_chan))) {
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
			if (isset(chanlist, i)) {
				ic->ic_ibss_chan = &ic->ic_channels[i];
				goto found;
			}
		return EINVAL;			/* no active channels */
found:
		;
	}
	if (ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan)))
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	memcpy(ic->ic_chan_active, chanlist, sizeof(ic->ic_chan_active));
	/*
	 * We force the state to INIT before calling ieee80211_new_state
	 * to get ieee80211_begin_scan called.  We really want to scan w/o
	 * altering the current state but that's not possible right now.
	 */
	/* XXX handle proberequest case */
	ic->ic_state = IEEE80211_S_INIT;	/* XXX bypass state machine */
	return 0;
}

int
ieee80211_cfgset(struct ieee80211com *ic, u_long cmd, caddr_t data)
{
	struct ifnet *ifp = ic->ic_ifp;
	int i, j, len, error, rate;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_ltv_keys *keys;
	struct wi_req wreq;
	u_char chanlist[roundup(IEEE80211_CHAN_MAX, NBBY)];

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	len = wreq.wi_len ? (wreq.wi_len - 1) * 2 : 0;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
	case WI_RID_NODENAME:
		return EPERM;
	case WI_RID_CURRENT_SSID:
		return EPERM;
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		if (le16toh(wreq.wi_val[0]) * 2 > len ||
		    le16toh(wreq.wi_val[0]) > IEEE80211_NWID_LEN) {
			error = ENOSPC;
			break;
		}
		memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
		ic->ic_des_esslen = le16toh(wreq.wi_val[0]) * 2;
		memcpy(ic->ic_des_essid, &wreq.wi_val[1], ic->ic_des_esslen);
		error = ENETRESET;
		break;
	case WI_RID_CURRENT_BSSID:
		return EPERM;
	case WI_RID_OWN_CHNL:
		if (len != 2)
			return EINVAL;
		i = le16toh(wreq.wi_val[0]);
		if (i < 0 ||
		    i > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, i))
			return EINVAL;
		ic->ic_ibss_chan = &ic->ic_channels[i];
		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		else
			error = ENETRESET;
		break;
	case WI_RID_CURRENT_CHAN:
		return EPERM;
	case WI_RID_COMMS_QUALITY:
		return EPERM;
	case WI_RID_PROMISC:
		if (len != 2)
			return EINVAL;
		if (ifp->if_flags & IFF_PROMISC) {
			if (wreq.wi_val[0] == 0) {
				ifp->if_flags &= ~IFF_PROMISC;
				error = ENETRESET;
			}
		} else {
			if (wreq.wi_val[0] != 0) {
				ifp->if_flags |= IFF_PROMISC;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_PORTTYPE:
		if (len != 2)
			return EINVAL;
		switch (le16toh(wreq.wi_val[0])) {
		case IEEE80211_M_STA:
			break;
		case IEEE80211_M_IBSS:
			if (!(ic->ic_caps & IEEE80211_C_IBSS))
				return EINVAL;
			break;
		case IEEE80211_M_AHDEMO:
			if (ic->ic_phytype != IEEE80211_T_DS ||
			    !(ic->ic_caps & IEEE80211_C_AHDEMO))
				return EINVAL;
			break;
		case IEEE80211_M_HOSTAP:
			if (!(ic->ic_caps & IEEE80211_C_HOSTAP))
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		if (le16toh(wreq.wi_val[0]) != ic->ic_opmode) {
			ic->ic_opmode = le16toh(wreq.wi_val[0]);
			error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		}
		break;
#if 0
	case WI_RID_MAC_NODE:
		if (len != IEEE80211_ADDR_LEN)
			return EINVAL;
		IEEE80211_ADDR_COPY(LLADDR(ifp->if_sadl), wreq.wi_val);
		/* if_init will copy lladdr into ic_myaddr */
		error = ENETRESET;
		break;
#endif
	case WI_RID_TX_RATE:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] == 0) {
			/* auto */
			ic->ic_fixed_rate = IEEE80211_FIXED_RATE_NONE;
			break;
		}
		rate = 2 * le16toh(wreq.wi_val[0]);
		if (ic->ic_curmode == IEEE80211_MODE_AUTO) {
			/*
			 * In autoselect mode search for the rate.  We take
			 * the first instance which may not be right, but we
			 * are limited by the interface.  Note that we also
			 * lock the mode to insure the rate is meaningful
			 * when it is used.
			 */
			for (j = IEEE80211_MODE_11A;
			     j < IEEE80211_MODE_MAX; j++) {
				if ((ic->ic_modecaps & (1<<j)) == 0)
					continue;
				i = findrate(ic, j, rate);
				if (i != -1) {
					/* lock mode too */
					ic->ic_curmode = j;
					goto setrate;
				}
			}
		} else {
			i = findrate(ic, ic->ic_curmode, rate);
			if (i != -1)
				goto setrate;
		}
		return EINVAL;
	setrate:
		ic->ic_fixed_rate = i;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case WI_RID_CUR_TX_RATE:
		return EPERM;
	case WI_RID_RTS_THRESH:
		if (len != 2)
			return EINVAL;
		if (le16toh(wreq.wi_val[0]) != IEEE80211_MAX_LEN)
			return EINVAL;		/* TODO: RTS */
		break;
	case WI_RID_CREATE_IBSS:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_caps & IEEE80211_C_IBSS) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_IBSSON) == 0) {
				ic->ic_flags |= IEEE80211_F_IBSSON;
				if (ic->ic_opmode == IEEE80211_M_IBSS &&
				    ic->ic_state == IEEE80211_S_SCAN)
					error = IS_UP_AUTO(ic) ? ENETRESET : 0;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_IBSSON) {
				ic->ic_flags &= ~IEEE80211_F_IBSSON;
				if (ic->ic_flags & IEEE80211_F_SIBSS) {
					ic->ic_flags &= ~IEEE80211_F_SIBSS;
					error = IS_UP_AUTO(ic) ? ENETRESET : 0;
				}
			}
		}
		break;
	case WI_RID_MICROWAVE_OVEN:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_ROAMING_MODE:
		if (len != 2)
			return EINVAL;
		i = le16toh(wreq.wi_val[0]);
		if (i > IEEE80211_ROAMING_MANUAL)
			return EINVAL;		/* not supported */
		ic->ic_roaming = i;
		break;
	case WI_RID_SYSTEM_SCALE:
		if (len != 2)
			return EINVAL;
		if (le16toh(wreq.wi_val[0]) != 1)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_PM_ENABLED:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
			}
		}
		break;
	case WI_RID_MAX_SLEEP:
		if (len != 2)
			return EINVAL;
		ic->ic_lintval = le16toh(wreq.wi_val[0]);
		if (ic->ic_flags & IEEE80211_F_PMGTON)
			error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case WI_RID_CUR_BEACON_INT:
		return EPERM;
	case WI_RID_WEP_AVAIL:
		return EPERM;
	case WI_RID_CNFAUTHMODE:
		if (len != 2)
			return EINVAL;
		i = le16toh(wreq.wi_val[0]);
		if (i > IEEE80211_AUTH_WPA)
			return EINVAL;
		ic->ic_bss->ni_authmode = i;		/* XXX ENETRESET? */
		error = ENETRESET;
		break;
	case WI_RID_ENCRYPTION:
		if (len != 2)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_caps & IEEE80211_C_WEP) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0) {
				ic->ic_flags |= IEEE80211_F_PRIVACY;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PRIVACY) {
				ic->ic_flags &= ~IEEE80211_F_PRIVACY;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_TX_CRYPT_KEY:
		if (len != 2)
			return EINVAL;
		i = le16toh(wreq.wi_val[0]);
		if (i >= IEEE80211_WEP_NKID)
			return EINVAL;
		ic->ic_def_txkey = i;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		if (len != sizeof(struct wi_ltv_keys))
			return EINVAL;
		keys = (struct wi_ltv_keys *)&wreq;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			len = le16toh(keys->wi_keys[i].wi_keylen);
			if (len != 0 && len < IEEE80211_WEP_KEYLEN)
				return EINVAL;
			if (len > IEEE80211_KEYBUF_SIZE)
				return EINVAL;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			struct ieee80211_key *k = &ic->ic_nw_keys[i];

			len = le16toh(keys->wi_keys[i].wi_keylen);
			k->wk_keylen = len;
			k->wk_flags = IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV;
			memset(k->wk_key, 0, sizeof(k->wk_key));
			memcpy(k->wk_key, keys->wi_keys[i].wi_keydat, len);
#if 0
			k->wk_type = IEEE80211_CIPHER_WEP;
#endif
		}
		error = ENETRESET;
		break;
	case WI_RID_MAX_DATALEN:
		if (len != 2)
			return EINVAL;
		len = le16toh(wreq.wi_val[0]);
		if (len < 350 /* ? */ || len > IEEE80211_MAX_LEN)
			return EINVAL;
		ic->ic_fragthreshold = len;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case WI_RID_IFACE_STATS:
		error = EPERM;
		break;
	case WI_RID_SCAN_REQ:			/* XXX wicontrol */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		error = ieee80211_setupscan(ic, ic->ic_chan_avail);
		if (error == 0)
			error = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		break;
	case WI_RID_SCAN_APS:
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		len--;			/* XXX: tx rate? */
		/* FALLTHRU */
	case WI_RID_CHANNEL_LIST:
		memset(chanlist, 0, sizeof(chanlist));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
			if ((j / 8) >= len)
				break;
			if (isclr((u_int8_t *)wreq.wi_val, j))
				continue;
			if (isclr(ic->ic_chan_active, i)) {
				if (wreq.wi_type != WI_RID_CHANNEL_LIST)
					continue;
				if (isclr(ic->ic_chan_avail, i))
					return EPERM;
			}
			setbit(chanlist, i);
		}
		error = ieee80211_setupscan(ic, chanlist);
		if (wreq.wi_type == WI_RID_CHANNEL_LIST) {
			/* NB: ignore error from ieee80211_setupscan */
			error = ENETRESET;
		} else if (error == 0)
			error = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == ENETRESET && !IS_UP_AUTO(ic))
		error = 0;
	return error;
}

static struct ieee80211_channel *
getcurchan(struct ieee80211com *ic)
{
	switch (ic->ic_state) {
	case IEEE80211_S_INIT:
	case IEEE80211_S_SCAN:
		return ic->ic_des_chan;
	default:
		return ic->ic_ibss_chan;
	}
}

static int
cap2cipher(int flag)
{
	switch (flag) {
	case IEEE80211_C_WEP:		return IEEE80211_CIPHER_WEP;
	case IEEE80211_C_AES:		return IEEE80211_CIPHER_AES_OCB;
	case IEEE80211_C_AES_CCM:	return IEEE80211_CIPHER_AES_CCM;
	case IEEE80211_C_CKIP:		return IEEE80211_CIPHER_CKIP;
	case IEEE80211_C_TKIP:		return IEEE80211_CIPHER_TKIP;
	}
	return -1;
}

static int
ieee80211_ioctl_getkey(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_node *ni;
	struct ieee80211req_key ik;
	struct ieee80211_key *wk;
	const struct ieee80211_cipher *cip;
	u_int kid;
	int error;

	if (ireq->i_len != sizeof(ik))
		return EINVAL;
	error = copyin(ireq->i_data, &ik, sizeof(ik));
	if (error)
		return error;
	kid = ik.ik_keyix;
	if (kid == IEEE80211_KEYIX_NONE) {
		ni = ieee80211_find_node(&ic->ic_sta, ik.ik_macaddr);
		if (ni == NULL)
			return EINVAL;		/* XXX */
		wk = &ni->ni_ucastkey;
	} else {
		if (kid >= IEEE80211_WEP_NKID)
			return EINVAL;
		wk = &ic->ic_nw_keys[kid];
		IEEE80211_ADDR_COPY(&ik.ik_macaddr, ic->ic_bss->ni_macaddr);
		ni = NULL;
	}
	cip = wk->wk_cipher;
	ik.ik_type = cip->ic_cipher;
	ik.ik_keylen = wk->wk_keylen;
	ik.ik_flags = wk->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV);
	if (wk->wk_keyix == ic->ic_def_txkey)
		ik.ik_flags |= IEEE80211_KEY_DEFAULT;
	if (suser(curthread) == 0) {
		/* NB: only root can read key data */
		ik.ik_keyrsc = wk->wk_keyrsc;
		ik.ik_keytsc = wk->wk_keytsc;
		memcpy(ik.ik_keydata, wk->wk_key, wk->wk_keylen);
		if (cip->ic_cipher == IEEE80211_CIPHER_TKIP) {
			memcpy(ik.ik_keydata+wk->wk_keylen,
				wk->wk_key + IEEE80211_KEYBUF_SIZE,
				IEEE80211_MICBUF_SIZE);
			ik.ik_keylen += IEEE80211_MICBUF_SIZE;
		}
	} else {
		ik.ik_keyrsc = 0;
		ik.ik_keytsc = 0;
		memset(ik.ik_keydata, 0, sizeof(ik.ik_keydata));
	}
	if (ni != NULL)
		ieee80211_free_node(ni);
	return copyout(&ik, ireq->i_data, sizeof(ik));
}

static int
ieee80211_ioctl_getchanlist(struct ieee80211com *ic, struct ieee80211req *ireq)
{

	if (sizeof(ic->ic_chan_active) > ireq->i_len)
		ireq->i_len = sizeof(ic->ic_chan_active);
	return copyout(&ic->ic_chan_active, ireq->i_data, ireq->i_len);
}

static int
ieee80211_ioctl_getchaninfo(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211req_chaninfo chans;	/* XXX off stack? */
	int i, space;

	/*
	 * Since channel 0 is not available for DS, channel 1
	 * is assigned to LSB on WaveLAN.
	 */
	if (ic->ic_phytype == IEEE80211_T_DS)
		i = 1;
	else
		i = 0;
	memset(&chans, 0, sizeof(chans));
	for (; i <= IEEE80211_CHAN_MAX; i++)
		if (isset(ic->ic_chan_avail, i)) {
			struct ieee80211_channel *c = &ic->ic_channels[i];
			chans.ic_chans[chans.ic_nchans].ic_freq = c->ic_freq;
			chans.ic_chans[chans.ic_nchans].ic_flags = c->ic_flags;
			chans.ic_nchans++;
		}
	space = __offsetof(struct ieee80211req_chaninfo,
			ic_chans[chans.ic_nchans]);
	if (space > ireq->i_len)
		space = ireq->i_len;
	return copyout(&chans, ireq->i_data, space);
}

static int
ieee80211_ioctl_getwpaie(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_node *ni;
	struct ieee80211req_wpaie wpaie;
	int error;

	if (ireq->i_len < IEEE80211_ADDR_LEN)
		return EINVAL;
	error = copyin(ireq->i_data, wpaie.wpa_macaddr, IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;
	ni = ieee80211_find_node(&ic->ic_sta, wpaie.wpa_macaddr);
	if (ni == NULL)
		return EINVAL;		/* XXX */
	memset(wpaie.wpa_ie, 0, sizeof(wpaie.wpa_ie));
	if (ni->ni_wpa_ie != NULL) {
		int ielen = ni->ni_wpa_ie[1] + 2;
		if (ielen > sizeof(wpaie.wpa_ie))
			ielen = sizeof(wpaie.wpa_ie);
		memcpy(wpaie.wpa_ie, ni->ni_wpa_ie, ielen);
	}
	ieee80211_free_node(ni);
	if (ireq->i_len > sizeof(wpaie))
		ireq->i_len = sizeof(wpaie);
	return copyout(&wpaie, ireq->i_data, ireq->i_len);
}

static int
ieee80211_ioctl_getstastats(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_node *ni;
	u_int8_t macaddr[IEEE80211_ADDR_LEN];
	const int off = __offsetof(struct ieee80211req_sta_stats, is_stats);
	int error;

	if (ireq->i_len < off)
		return EINVAL;
	error = copyin(ireq->i_data, macaddr, IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;
	ni = ieee80211_find_node(&ic->ic_sta, macaddr);
	if (ni == NULL)
		return EINVAL;		/* XXX */
	if (ireq->i_len > sizeof(struct ieee80211req_sta_stats))
		ireq->i_len = sizeof(struct ieee80211req_sta_stats);
	/* NB: copy out only the statistics */
	error = copyout(&ni->ni_stats, (u_int8_t *) ireq->i_data + off,
			ireq->i_len - off);
	ieee80211_free_node(ni);
	return error;
}

static void
get_scan_result(struct ieee80211req_scan_result *sr,
	const struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	memset(sr, 0, sizeof(*sr));
	sr->isr_ssid_len = ni->ni_esslen;
	if (ni->ni_wpa_ie != NULL)
		sr->isr_ie_len += 2+ni->ni_wpa_ie[1];
	if (ni->ni_wme_ie != NULL)
		sr->isr_ie_len += 2+ni->ni_wme_ie[1];
	sr->isr_len = sizeof(*sr) + sr->isr_ssid_len + sr->isr_ie_len;
	sr->isr_len = roundup(sr->isr_len, sizeof(u_int32_t));
	if (ni->ni_chan != IEEE80211_CHAN_ANYC) {
		sr->isr_freq = ni->ni_chan->ic_freq;
		sr->isr_flags = ni->ni_chan->ic_flags;
	}
	sr->isr_rssi = ic->ic_node_getrssi(ni);
	sr->isr_intval = ni->ni_intval;
	sr->isr_capinfo = ni->ni_capinfo;
	sr->isr_erp = ni->ni_erp;
	IEEE80211_ADDR_COPY(sr->isr_bssid, ni->ni_bssid);
	sr->isr_nrates = ni->ni_rates.rs_nrates;
	if (sr->isr_nrates > 15)
		sr->isr_nrates = 15;
	memcpy(sr->isr_rates, ni->ni_rates.rs_rates, sr->isr_nrates);
}

static int
ieee80211_ioctl_getscanresults(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	union {
		struct ieee80211req_scan_result res;
		char data[512];		/* XXX shrink? */
	} u;
	struct ieee80211req_scan_result *sr = &u.res;
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;
	int error, space;
	u_int8_t *p, *cp;

	p = ireq->i_data;
	space = ireq->i_len;
	error = 0;
	/* XXX locking */
	nt =  &ic->ic_scan;
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		/* NB: skip pre-scan node state */ 
		if (ni->ni_chan == IEEE80211_CHAN_ANYC)
			continue;
		get_scan_result(sr, ni);
		if (sr->isr_len > sizeof(u))
			continue;		/* XXX */
		if (space < sr->isr_len)
			break;
		cp = (u_int8_t *)(sr+1);
		memcpy(cp, ni->ni_essid, ni->ni_esslen);
		cp += ni->ni_esslen;
		if (ni->ni_wpa_ie != NULL) {
			memcpy(cp, ni->ni_wpa_ie, 2+ni->ni_wpa_ie[1]);
			cp += 2+ni->ni_wpa_ie[1];
		}
		if (ni->ni_wme_ie != NULL) {
			memcpy(cp, ni->ni_wme_ie, 2+ni->ni_wme_ie[1]);
			cp += 2+ni->ni_wme_ie[1];
		}
		error = copyout(sr, p, sr->isr_len);
		if (error)
			break;
		p += sr->isr_len;
		space -= sr->isr_len;
	}
	ireq->i_len -= space;
	return error;
}

struct stainforeq {
	struct ieee80211com *ic;
	struct ieee80211req_sta_info *si;
	size_t	space;
};

static size_t
sta_space(const struct ieee80211_node *ni, size_t *ielen)
{
	*ielen = 0;
	if (ni->ni_wpa_ie != NULL)
		*ielen += 2+ni->ni_wpa_ie[1];
	if (ni->ni_wme_ie != NULL)
		*ielen += 2+ni->ni_wme_ie[1];
	return roundup(sizeof(struct ieee80211req_sta_info) + *ielen,
		      sizeof(u_int32_t));
}

static void
get_sta_space(void *arg, struct ieee80211_node *ni)
{
	struct stainforeq *req = arg;
	struct ieee80211com *ic = ni->ni_ic;
	size_t ielen;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    ni->ni_associd == 0)	/* only associated stations */
		return;
	req->space += sta_space(ni, &ielen);
}

static void
get_sta_info(void *arg, struct ieee80211_node *ni)
{
	struct stainforeq *req = arg;
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211req_sta_info *si;
	size_t ielen, len;
	u_int8_t *cp;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    ni->ni_associd == 0)	/* only associated stations */
		return;
	if (ni->ni_chan == IEEE80211_CHAN_ANYC)	/* XXX bogus entry */
		return;
	len = sta_space(ni, &ielen);
	if (len > req->space)
		return;
	si = req->si;
	si->isi_len = len;
	si->isi_ie_len = ielen;
	si->isi_freq = ni->ni_chan->ic_freq;
	si->isi_flags = ni->ni_chan->ic_flags;
	si->isi_state = ni->ni_flags;
	si->isi_authmode = ni->ni_authmode;
	si->isi_rssi = ic->ic_node_getrssi(ni);
	si->isi_capinfo = ni->ni_capinfo;
	si->isi_erp = ni->ni_erp;
	IEEE80211_ADDR_COPY(si->isi_macaddr, ni->ni_macaddr);
	si->isi_nrates = ni->ni_rates.rs_nrates;
	if (si->isi_nrates > 15)
		si->isi_nrates = 15;
	memcpy(si->isi_rates, ni->ni_rates.rs_rates, si->isi_nrates);
	si->isi_txrate = ni->ni_txrate;
	si->isi_associd = ni->ni_associd;
	si->isi_txpower = ni->ni_txpower;
	si->isi_vlan = ni->ni_vlan;
	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		memcpy(si->isi_txseqs, ni->ni_txseqs, sizeof(ni->ni_txseqs));
		memcpy(si->isi_rxseqs, ni->ni_rxseqs, sizeof(ni->ni_rxseqs));
	} else {
		si->isi_txseqs[0] = ni->ni_txseqs[0];
		si->isi_rxseqs[0] = ni->ni_rxseqs[0];
	}
	/* NB: leave all cases in case we relax ni_associd == 0 check */
	if (ieee80211_node_is_authorized(ni))
		si->isi_inact = ic->ic_inact_run;
	else if (ni->ni_associd != 0)
		si->isi_inact = ic->ic_inact_auth;
	else
		si->isi_inact = ic->ic_inact_init;
	si->isi_inact = (si->isi_inact - ni->ni_inact) * IEEE80211_INACT_WAIT;

	cp = (u_int8_t *)(si+1);
	if (ni->ni_wpa_ie != NULL) {
		memcpy(cp, ni->ni_wpa_ie, 2+ni->ni_wpa_ie[1]);
		cp += 2+ni->ni_wpa_ie[1];
	}
	if (ni->ni_wme_ie != NULL) {
		memcpy(cp, ni->ni_wme_ie, 2+ni->ni_wme_ie[1]);
		cp += 2+ni->ni_wme_ie[1];
	}

	req->si = (struct ieee80211req_sta_info *)(((u_int8_t *)si) + len);
	req->space -= len;
}

static int
ieee80211_ioctl_getstainfo(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct stainforeq req;
	int error;

	if (ireq->i_len < sizeof(struct stainforeq))
		return EFAULT;

	error = 0;
	req.space = 0;
	ieee80211_iterate_nodes(&ic->ic_sta, get_sta_space, &req);
	if (req.space > ireq->i_len)
		req.space = ireq->i_len;
	if (req.space > 0) {
		size_t space;
		void *p;

		space = req.space;
		/* XXX M_WAITOK after driver lock released */
		MALLOC(p, void *, space, M_TEMP, M_NOWAIT);
		if (p == NULL)
			return ENOMEM;
		req.si = p;
		ieee80211_iterate_nodes(&ic->ic_sta, get_sta_info, &req);
		ireq->i_len = space - req.space;
		error = copyout(p, ireq->i_data, ireq->i_len);
		FREE(p, M_TEMP);
	} else
		ireq->i_len = 0;

	return error;
}

static int
ieee80211_ioctl_getstatxpow(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_node *ni;
	struct ieee80211req_sta_txpow txpow;
	int error;

	if (ireq->i_len != sizeof(txpow))
		return EINVAL;
	error = copyin(ireq->i_data, &txpow, sizeof(txpow));
	if (error != 0)
		return error;
	ni = ieee80211_find_node(&ic->ic_sta, txpow.it_macaddr);
	if (ni == NULL)
		return EINVAL;		/* XXX */
	txpow.it_txpow = ni->ni_txpower;
	error = copyout(&txpow, ireq->i_data, sizeof(txpow));
	ieee80211_free_node(ni);
	return error;
}

static int
ieee80211_ioctl_getwmeparam(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	struct wmeParams *wmep;
	int ac;

	if ((ic->ic_caps & IEEE80211_C_WME) == 0)
		return EINVAL;

	ac = (ireq->i_len & IEEE80211_WMEPARAM_VAL);
	if (ac >= WME_NUM_AC)
		ac = WME_AC_BE;
	if (ireq->i_len & IEEE80211_WMEPARAM_BSS)
		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[ac];
	else
		wmep = &wme->wme_wmeChanParams.cap_wmeParams[ac];
	switch (ireq->i_type) {
	case IEEE80211_IOC_WME_CWMIN:		/* WME: CWmin */
		ireq->i_val = wmep->wmep_logcwmin;
		break;
	case IEEE80211_IOC_WME_CWMAX:		/* WME: CWmax */
		ireq->i_val = wmep->wmep_logcwmax;
		break;
	case IEEE80211_IOC_WME_AIFS:		/* WME: AIFS */
		ireq->i_val = wmep->wmep_aifsn;
		break;
	case IEEE80211_IOC_WME_TXOPLIMIT:	/* WME: txops limit */
		ireq->i_val = wmep->wmep_txopLimit;
		break;
	case IEEE80211_IOC_WME_ACM:		/* WME: ACM (bss only) */
		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[ac];
		ireq->i_val = wmep->wmep_acm;
		break;
	case IEEE80211_IOC_WME_ACKPOLICY:	/* WME: ACK policy (!bss only)*/
		wmep = &wme->wme_wmeChanParams.cap_wmeParams[ac];
		ireq->i_val = !wmep->wmep_noackPolicy;
		break;
	}
	return 0;
}

/*
 * When building the kernel with -O2 on the i386 architecture, gcc
 * seems to want to inline this function into ieee80211_ioctl()
 * (which is the only routine that calls it). When this happens,
 * ieee80211_ioctl() ends up consuming an additional 2K of stack
 * space. (Exactly why it needs so much is unclear.) The problem
 * is that it's possible for ieee80211_ioctl() to invoke other
 * routines (including driver init functions) which could then find
 * themselves perilously close to exhausting the stack.
 *
 * To avoid this, we deliberately prevent gcc from inlining this
 * routine. Another way to avoid this is to use less agressive
 * optimization when compiling this file (i.e. -O instead of -O2)
 * but special-casing the compilation of this one module in the
 * build system would be awkward.
 */
#ifdef __GNUC__
__attribute__ ((noinline))
#endif
static int
ieee80211_ioctl_get80211(struct ieee80211com *ic, u_long cmd, struct ieee80211req *ireq)
{
	const struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	int error = 0;
	u_int kid, len, m;
	u_int8_t tmpkey[IEEE80211_KEYBUF_SIZE];
	char tmpssid[IEEE80211_NWID_LEN];

	switch (ireq->i_type) {
	case IEEE80211_IOC_SSID:
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			ireq->i_len = ic->ic_des_esslen;
			memcpy(tmpssid, ic->ic_des_essid, ireq->i_len);
			break;
		default:
			ireq->i_len = ic->ic_bss->ni_esslen;
			memcpy(tmpssid, ic->ic_bss->ni_essid,
				ireq->i_len);
			break;
		}
		error = copyout(tmpssid, ireq->i_data, ireq->i_len);
		break;
	case IEEE80211_IOC_NUMSSIDS:
		ireq->i_val = 1;
		break;
	case IEEE80211_IOC_WEP:
		if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0)
			ireq->i_val = IEEE80211_WEP_OFF;
		else if (ic->ic_flags & IEEE80211_F_DROPUNENC)
			ireq->i_val = IEEE80211_WEP_ON;
		else
			ireq->i_val = IEEE80211_WEP_MIXED;
		break;
	case IEEE80211_IOC_WEPKEY:
		kid = (u_int) ireq->i_val;
		if (kid >= IEEE80211_WEP_NKID)
			return EINVAL;
		len = (u_int) ic->ic_nw_keys[kid].wk_keylen;
		/* NB: only root can read WEP keys */
		if (suser(curthread) == 0) {
			bcopy(ic->ic_nw_keys[kid].wk_key, tmpkey, len);
		} else {
			bzero(tmpkey, len);
		}
		ireq->i_len = len;
		error = copyout(tmpkey, ireq->i_data, len);
		break;
	case IEEE80211_IOC_NUMWEPKEYS:
		ireq->i_val = IEEE80211_WEP_NKID;
		break;
	case IEEE80211_IOC_WEPTXKEY:
		ireq->i_val = ic->ic_def_txkey;
		break;
	case IEEE80211_IOC_AUTHMODE:
		if (ic->ic_flags & IEEE80211_F_WPA)
			ireq->i_val = IEEE80211_AUTH_WPA;
		else
			ireq->i_val = ic->ic_bss->ni_authmode;
		break;
	case IEEE80211_IOC_CHANNEL:
		ireq->i_val = ieee80211_chan2ieee(ic, getcurchan(ic));
		break;
	case IEEE80211_IOC_POWERSAVE:
		if (ic->ic_flags & IEEE80211_F_PMGTON)
			ireq->i_val = IEEE80211_POWERSAVE_ON;
		else
			ireq->i_val = IEEE80211_POWERSAVE_OFF;
		break;
	case IEEE80211_IOC_POWERSAVESLEEP:
		ireq->i_val = ic->ic_lintval;
		break;
	case IEEE80211_IOC_RTSTHRESHOLD:
		ireq->i_val = ic->ic_rtsthreshold;
		break;
	case IEEE80211_IOC_PROTMODE:
		ireq->i_val = ic->ic_protmode;
		break;
	case IEEE80211_IOC_TXPOWER:
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
			return EINVAL;
		ireq->i_val = ic->ic_txpowlimit;
		break;
	case IEEE80211_IOC_MCASTCIPHER:
		ireq->i_val = rsn->rsn_mcastcipher;
		break;
	case IEEE80211_IOC_MCASTKEYLEN:
		ireq->i_val = rsn->rsn_mcastkeylen;
		break;
	case IEEE80211_IOC_UCASTCIPHERS:
		ireq->i_val = 0;
		for (m = 0x1; m != 0; m <<= 1)
			if (rsn->rsn_ucastcipherset & m)
				ireq->i_val |= 1<<cap2cipher(m);
		break;
	case IEEE80211_IOC_UCASTCIPHER:
		ireq->i_val = rsn->rsn_ucastcipher;
		break;
	case IEEE80211_IOC_UCASTKEYLEN:
		ireq->i_val = rsn->rsn_ucastkeylen;
		break;
	case IEEE80211_IOC_KEYMGTALGS:
		ireq->i_val = rsn->rsn_keymgmtset;
		break;
	case IEEE80211_IOC_RSNCAPS:
		ireq->i_val = rsn->rsn_caps;
		break;
	case IEEE80211_IOC_WPA:
		switch (ic->ic_flags & IEEE80211_F_WPA) {
		case IEEE80211_F_WPA1:
			ireq->i_val = 1;
			break;
		case IEEE80211_F_WPA2:
			ireq->i_val = 2;
			break;
		case IEEE80211_F_WPA1 | IEEE80211_F_WPA2:
			ireq->i_val = 3;
			break;
		default:
			ireq->i_val = 0;
			break;
		}
		break;
	case IEEE80211_IOC_CHANLIST:
		error = ieee80211_ioctl_getchanlist(ic, ireq);
		break;
	case IEEE80211_IOC_ROAMING:
		ireq->i_val = ic->ic_roaming;
		break;
	case IEEE80211_IOC_PRIVACY:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_PRIVACY) != 0;
		break;
	case IEEE80211_IOC_DROPUNENCRYPTED:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_DROPUNENC) != 0;
		break;
	case IEEE80211_IOC_COUNTERMEASURES:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_COUNTERM) != 0;
		break;
	case IEEE80211_IOC_DRIVER_CAPS:
		ireq->i_val = ic->ic_caps>>16;
		ireq->i_len = ic->ic_caps&0xffff;
		break;
	case IEEE80211_IOC_WME:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_WME) != 0;
		break;
	case IEEE80211_IOC_HIDESSID:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_HIDESSID) != 0;
		break;
	case IEEE80211_IOC_APBRIDGE:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_NOBRIDGE) == 0;
		break;
	case IEEE80211_IOC_OPTIE:
		if (ic->ic_opt_ie == NULL)
			return EINVAL;
		/* NB: truncate, caller can check length */
		if (ireq->i_len > ic->ic_opt_ie_len)
			ireq->i_len = ic->ic_opt_ie_len;
		error = copyout(ic->ic_opt_ie, ireq->i_data, ireq->i_len);
		break;
	case IEEE80211_IOC_WPAKEY:
		error = ieee80211_ioctl_getkey(ic, ireq);
		break;
	case IEEE80211_IOC_CHANINFO:
		error = ieee80211_ioctl_getchaninfo(ic, ireq);
		break;
	case IEEE80211_IOC_BSSID:
		if (ireq->i_len != IEEE80211_ADDR_LEN)
			return EINVAL;
		error = copyout(ic->ic_state == IEEE80211_S_RUN ?
					ic->ic_bss->ni_bssid :
					ic->ic_des_bssid,
				ireq->i_data, ireq->i_len);
		break;
	case IEEE80211_IOC_WPAIE:
		error = ieee80211_ioctl_getwpaie(ic, ireq);
		break;
	case IEEE80211_IOC_SCAN_RESULTS:
		error = ieee80211_ioctl_getscanresults(ic, ireq);
		break;
	case IEEE80211_IOC_STA_STATS:
		error = ieee80211_ioctl_getstastats(ic, ireq);
		break;
	case IEEE80211_IOC_TXPOWMAX:
		ireq->i_val = ic->ic_bss->ni_txpower;
		break;
	case IEEE80211_IOC_STA_TXPOW:
		error = ieee80211_ioctl_getstatxpow(ic, ireq);
		break;
	case IEEE80211_IOC_STA_INFO:
		error = ieee80211_ioctl_getstainfo(ic, ireq);
		break;
	case IEEE80211_IOC_WME_CWMIN:		/* WME: CWmin */
	case IEEE80211_IOC_WME_CWMAX:		/* WME: CWmax */
	case IEEE80211_IOC_WME_AIFS:		/* WME: AIFS */
	case IEEE80211_IOC_WME_TXOPLIMIT:	/* WME: txops limit */
	case IEEE80211_IOC_WME_ACM:		/* WME: ACM (bss only) */
	case IEEE80211_IOC_WME_ACKPOLICY:	/* WME: ACK policy (bss only) */
		error = ieee80211_ioctl_getwmeparam(ic, ireq);
		break;
	case IEEE80211_IOC_DTIM_PERIOD:
		ireq->i_val = ic->ic_dtim_period;
		break;
	case IEEE80211_IOC_BEACON_INTERVAL:
		/* NB: get from ic_bss for station mode */
		ireq->i_val = ic->ic_bss->ni_intval;
		break;
	case IEEE80211_IOC_PUREG:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_PUREG) != 0;
		break;
	case IEEE80211_IOC_FRAGTHRESHOLD:
		ireq->i_val = ic->ic_fragthreshold;
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

static int
ieee80211_ioctl_setoptie(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	int error;
	void *ie;

	/*
	 * NB: Doing this for ap operation could be useful (e.g. for
	 *     WPA and/or WME) except that it typically is worthless
	 *     without being able to intervene when processing
	 *     association response frames--so disallow it for now.
	 */
	if (ic->ic_opmode != IEEE80211_M_STA)
		return EINVAL;
	if (ireq->i_len > IEEE80211_MAX_OPT_IE)
		return EINVAL;
	/* NB: data.length is validated by the wireless extensions code */
	MALLOC(ie, void *, ireq->i_len, M_DEVBUF, M_WAITOK);
	if (ie == NULL)
		return ENOMEM;
	error = copyin(ireq->i_data, ie, ireq->i_len);
	/* XXX sanity check data? */
	if (ic->ic_opt_ie != NULL)
		FREE(ic->ic_opt_ie, M_DEVBUF);
	ic->ic_opt_ie = ie;
	ic->ic_opt_ie_len = ireq->i_len;
	return 0;
}

static int
ieee80211_ioctl_setkey(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211req_key ik;
	struct ieee80211_node *ni;
	struct ieee80211_key *wk;
	u_int16_t kid;
	int error;

	if (ireq->i_len != sizeof(ik))
		return EINVAL;
	error = copyin(ireq->i_data, &ik, sizeof(ik));
	if (error)
		return error;
	/* NB: cipher support is verified by ieee80211_crypt_newkey */
	/* NB: this also checks ik->ik_keylen > sizeof(wk->wk_key) */
	if (ik.ik_keylen > sizeof(ik.ik_keydata))
		return E2BIG;
	kid = ik.ik_keyix;
	if (kid == IEEE80211_KEYIX_NONE) {
		/* XXX unicast keys currently must be tx/rx */
		if (ik.ik_flags != (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))
			return EINVAL;
		if (ic->ic_opmode == IEEE80211_M_STA) {
			ni = ieee80211_ref_node(ic->ic_bss);
			if (!IEEE80211_ADDR_EQ(ik.ik_macaddr, ni->ni_bssid)) {
				ieee80211_free_node(ni);
				return EADDRNOTAVAIL;
			}
		} else {
			ni = ieee80211_find_node(&ic->ic_sta, ik.ik_macaddr);
			if (ni == NULL)
				return ENOENT;
		}
		wk = &ni->ni_ucastkey;
	} else {
		if (kid >= IEEE80211_WEP_NKID)
			return EINVAL;
		wk = &ic->ic_nw_keys[kid];
		ni = NULL;
	}
	error = 0;
	ieee80211_key_update_begin(ic);
	if (ieee80211_crypto_newkey(ic, ik.ik_type, ik.ik_flags, wk)) {
		wk->wk_keylen = ik.ik_keylen;
		/* NB: MIC presence is implied by cipher type */
		if (wk->wk_keylen > IEEE80211_KEYBUF_SIZE)
			wk->wk_keylen = IEEE80211_KEYBUF_SIZE;
		wk->wk_keyrsc = ik.ik_keyrsc;
		wk->wk_keytsc = 0;			/* new key, reset */
		memset(wk->wk_key, 0, sizeof(wk->wk_key));
		memcpy(wk->wk_key, ik.ik_keydata, ik.ik_keylen);
		if (!ieee80211_crypto_setkey(ic, wk,
		    ni != NULL ? ni->ni_macaddr : ik.ik_macaddr))
			error = EIO;
		else if ((ik.ik_flags & IEEE80211_KEY_DEFAULT))
			ic->ic_def_txkey = kid;
	} else
		error = ENXIO;
	ieee80211_key_update_end(ic);
	if (ni != NULL)
		ieee80211_free_node(ni);
	return error;
}

static int
ieee80211_ioctl_delkey(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211req_del_key dk;
	int kid, error;

	if (ireq->i_len != sizeof(dk))
		return EINVAL;
	error = copyin(ireq->i_data, &dk, sizeof(dk));
	if (error)
		return error;
	kid = dk.idk_keyix;
	/* XXX u_int8_t -> u_int16_t */
	if (dk.idk_keyix == (u_int8_t) IEEE80211_KEYIX_NONE) {
		struct ieee80211_node *ni;

		if (ic->ic_opmode == IEEE80211_M_STA) {
			ni = ieee80211_ref_node(ic->ic_bss);
			if (!IEEE80211_ADDR_EQ(dk.idk_macaddr, ni->ni_bssid)) {
				ieee80211_free_node(ni);
				return EADDRNOTAVAIL;
			}
		} else {
			ni = ieee80211_find_node(&ic->ic_sta, dk.idk_macaddr);
			if (ni == NULL)
				return ENOENT;
		}
		/* XXX error return */
		ieee80211_node_delucastkey(ni);
		ieee80211_free_node(ni);
	} else {
		if (kid >= IEEE80211_WEP_NKID)
			return EINVAL;
		/* XXX error return */
		ieee80211_crypto_delkey(ic, &ic->ic_nw_keys[kid]);
	}
	return 0;
}

static void
domlme(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211req_mlme *mlme = arg;

	if (ni->ni_associd != 0) {
		IEEE80211_SEND_MGMT(ic, ni,
			mlme->im_op == IEEE80211_MLME_DEAUTH ?
				IEEE80211_FC0_SUBTYPE_DEAUTH :
				IEEE80211_FC0_SUBTYPE_DISASSOC,
			mlme->im_reason);
	}
	ieee80211_node_leave(ic, ni);
}

static int
ieee80211_ioctl_setmlme(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211req_mlme mlme;
	struct ieee80211_node *ni;
	int error;

	if (ireq->i_len != sizeof(mlme))
		return EINVAL;
	error = copyin(ireq->i_data, &mlme, sizeof(mlme));
	if (error)
		return error;
	switch (mlme.im_op) {
	case IEEE80211_MLME_ASSOC:
		if (ic->ic_opmode != IEEE80211_M_STA)
			return EINVAL;
		/* XXX must be in S_SCAN state? */

		if (mlme.im_ssid_len != 0) {
			/*
			 * Desired ssid specified; must match both bssid and
			 * ssid to distinguish ap advertising multiple ssid's.
			 */
			ni = ieee80211_find_node_with_ssid(&ic->ic_scan,
				mlme.im_macaddr,
				mlme.im_ssid_len, mlme.im_ssid);
		} else {
			/*
			 * Normal case; just match bssid.
			 */
			ni = ieee80211_find_node(&ic->ic_scan, mlme.im_macaddr);
		}
		if (ni == NULL)
			return EINVAL;
		if (!ieee80211_sta_join(ic, ni)) {
			ieee80211_free_node(ni);
			return EINVAL;
		}
		break;
	case IEEE80211_MLME_DISASSOC:
	case IEEE80211_MLME_DEAUTH:
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			/* XXX not quite right */
			ieee80211_new_state(ic, IEEE80211_S_INIT,
				mlme.im_reason);
			break;
		case IEEE80211_M_HOSTAP:
			/* NB: the broadcast address means do 'em all */
			if (!IEEE80211_ADDR_EQ(mlme.im_macaddr, ic->ic_ifp->if_broadcastaddr)) {
				if ((ni = ieee80211_find_node(&ic->ic_sta,
						mlme.im_macaddr)) == NULL)
					return EINVAL;
				domlme(&mlme, ni);
				ieee80211_free_node(ni);
			} else {
				ieee80211_iterate_nodes(&ic->ic_sta,
						domlme, &mlme);
			}
			break;
		default:
			return EINVAL;
		}
		break;
	case IEEE80211_MLME_AUTHORIZE:
	case IEEE80211_MLME_UNAUTHORIZE:
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			return EINVAL;
		ni = ieee80211_find_node(&ic->ic_sta, mlme.im_macaddr);
		if (ni == NULL)
			return EINVAL;
		if (mlme.im_op == IEEE80211_MLME_AUTHORIZE)
			ieee80211_node_authorize(ni);
		else
			ieee80211_node_unauthorize(ni);
		ieee80211_free_node(ni);
		break;
	default:
		return EINVAL;
	}
	return 0;
}

static int
ieee80211_ioctl_macmac(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	u_int8_t mac[IEEE80211_ADDR_LEN];
	const struct ieee80211_aclator *acl = ic->ic_acl;
	int error;

	if (ireq->i_len != sizeof(mac))
		return EINVAL;
	error = copyin(ireq->i_data, mac, ireq->i_len);
	if (error)
		return error;
	if (acl == NULL) {
		acl = ieee80211_aclator_get("mac");
		if (acl == NULL || !acl->iac_attach(ic))
			return EINVAL;
		ic->ic_acl = acl;
	}
	if (ireq->i_type == IEEE80211_IOC_ADDMAC)
		acl->iac_add(ic, mac);
	else
		acl->iac_remove(ic, mac);
	return 0;
}

static int
ieee80211_ioctl_maccmd(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	const struct ieee80211_aclator *acl = ic->ic_acl;

	switch (ireq->i_val) {
	case IEEE80211_MACCMD_POLICY_OPEN:
	case IEEE80211_MACCMD_POLICY_ALLOW:
	case IEEE80211_MACCMD_POLICY_DENY:
		if (acl == NULL) {
			acl = ieee80211_aclator_get("mac");
			if (acl == NULL || !acl->iac_attach(ic))
				return EINVAL;
			ic->ic_acl = acl;
		}
		acl->iac_setpolicy(ic, ireq->i_val);
		break;
	case IEEE80211_MACCMD_FLUSH:
		if (acl != NULL)
			acl->iac_flush(ic);
		/* NB: silently ignore when not in use */
		break;
	case IEEE80211_MACCMD_DETACH:
		if (acl != NULL) {
			ic->ic_acl = NULL;
			acl->iac_detach(ic);
		}
		break;
	default:
		return EINVAL;
	}
	return 0;
}

static int
ieee80211_ioctl_setchanlist(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211req_chanlist list;
	u_char chanlist[IEEE80211_CHAN_BYTES];
	int i, j, error;

	if (ireq->i_len != sizeof(list))
		return EINVAL;
	error = copyin(ireq->i_data, &list, sizeof(list));
	if (error)
		return error;
	memset(chanlist, 0, sizeof(chanlist));
	/*
	 * Since channel 0 is not available for DS, channel 1
	 * is assigned to LSB on WaveLAN.
	 */
	if (ic->ic_phytype == IEEE80211_T_DS)
		i = 1;
	else
		i = 0;
	for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
		/*
		 * NB: silently discard unavailable channels so users
		 *     can specify 1-255 to get all available channels.
		 */
		if (isset(list.ic_channels, j) && isset(ic->ic_chan_avail, i))
			setbit(chanlist, i);
	}
	if (ic->ic_ibss_chan == NULL ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_ibss_chan))) {
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
			if (isset(chanlist, i)) {
				ic->ic_ibss_chan = &ic->ic_channels[i];
				goto found;
			}
		return EINVAL;			/* no active channels */
found:
		;
	}
	memcpy(ic->ic_chan_active, chanlist, sizeof(ic->ic_chan_active));
	if (ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC ||
	    isclr(chanlist, ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan)))
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	return IS_UP_AUTO(ic) ? ENETRESET : 0;
}

static int
ieee80211_ioctl_setstatxpow(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_node *ni;
	struct ieee80211req_sta_txpow txpow;
	int error;

	if (ireq->i_len != sizeof(txpow))
		return EINVAL;
	error = copyin(ireq->i_data, &txpow, sizeof(txpow));
	if (error != 0)
		return error;
	ni = ieee80211_find_node(&ic->ic_sta, txpow.it_macaddr);
	if (ni == NULL)
		return EINVAL;		/* XXX */
	ni->ni_txpower = txpow.it_txpow;
	ieee80211_free_node(ni);
	return error;
}

static int
ieee80211_ioctl_setwmeparam(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	struct wmeParams *wmep, *chanp;
	int isbss, ac;

	if ((ic->ic_caps & IEEE80211_C_WME) == 0)
		return EINVAL;

	isbss = (ireq->i_len & IEEE80211_WMEPARAM_BSS);
	ac = (ireq->i_len & IEEE80211_WMEPARAM_VAL);
	if (ac >= WME_NUM_AC)
		ac = WME_AC_BE;
	if (isbss) {
		chanp = &wme->wme_bssChanParams.cap_wmeParams[ac];
		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[ac];
	} else {
		chanp = &wme->wme_chanParams.cap_wmeParams[ac];
		wmep = &wme->wme_wmeChanParams.cap_wmeParams[ac];
	}
	switch (ireq->i_type) {
	case IEEE80211_IOC_WME_CWMIN:		/* WME: CWmin */
		if (isbss) {
			wmep->wmep_logcwmin = ireq->i_val;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
				chanp->wmep_logcwmin = ireq->i_val;
		} else {
			wmep->wmep_logcwmin = chanp->wmep_logcwmin =
				ireq->i_val;
		}
		break;
	case IEEE80211_IOC_WME_CWMAX:		/* WME: CWmax */
		if (isbss) {
			wmep->wmep_logcwmax = ireq->i_val;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
				chanp->wmep_logcwmax = ireq->i_val;
		} else {
			wmep->wmep_logcwmax = chanp->wmep_logcwmax =
				ireq->i_val;
		}
		break;
	case IEEE80211_IOC_WME_AIFS:		/* WME: AIFS */
		if (isbss) {
			wmep->wmep_aifsn = ireq->i_val;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
				chanp->wmep_aifsn = ireq->i_val;
		} else {
			wmep->wmep_aifsn = chanp->wmep_aifsn = ireq->i_val;
		}
		break;
	case IEEE80211_IOC_WME_TXOPLIMIT:	/* WME: txops limit */
		if (isbss) {
			wmep->wmep_txopLimit = ireq->i_val;
			if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
				chanp->wmep_txopLimit = ireq->i_val;
		} else {
			wmep->wmep_txopLimit = chanp->wmep_txopLimit =
				ireq->i_val;
		}
		break;
	case IEEE80211_IOC_WME_ACM:		/* WME: ACM (bss only) */
		wmep->wmep_acm = ireq->i_val;
		if ((wme->wme_flags & WME_F_AGGRMODE) == 0)
			chanp->wmep_acm = ireq->i_val;
		break;
	case IEEE80211_IOC_WME_ACKPOLICY:	/* WME: ACK policy (!bss only)*/
		wmep->wmep_noackPolicy = chanp->wmep_noackPolicy =
			(ireq->i_val) == 0;
		break;
	}
	ieee80211_wme_updateparams(ic);
	return 0;
}

static int
cipher2cap(int cipher)
{
	switch (cipher) {
	case IEEE80211_CIPHER_WEP:	return IEEE80211_C_WEP;
	case IEEE80211_CIPHER_AES_OCB:	return IEEE80211_C_AES;
	case IEEE80211_CIPHER_AES_CCM:	return IEEE80211_C_AES_CCM;
	case IEEE80211_CIPHER_CKIP:	return IEEE80211_C_CKIP;
	case IEEE80211_CIPHER_TKIP:	return IEEE80211_C_TKIP;
	}
	return 0;
}

static int
ieee80211_ioctl_set80211(struct ieee80211com *ic, u_long cmd, struct ieee80211req *ireq)
{
	static const u_int8_t zerobssid[IEEE80211_ADDR_LEN];
	struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	int error;
	const struct ieee80211_authenticator *auth;
	u_int8_t tmpkey[IEEE80211_KEYBUF_SIZE];
	char tmpssid[IEEE80211_NWID_LEN];
	u_int8_t tmpbssid[IEEE80211_ADDR_LEN];
	struct ieee80211_key *k;
	int j, caps;
	u_int kid;

	error = 0;
	switch (ireq->i_type) {
	case IEEE80211_IOC_SSID:
		if (ireq->i_val != 0 ||
		    ireq->i_len > IEEE80211_NWID_LEN)
			return EINVAL;
		error = copyin(ireq->i_data, tmpssid, ireq->i_len);
		if (error)
			break;
		memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
		ic->ic_des_esslen = ireq->i_len;
		memcpy(ic->ic_des_essid, tmpssid, ireq->i_len);
		error = ENETRESET;
		break;
	case IEEE80211_IOC_WEP:
		switch (ireq->i_val) {
		case IEEE80211_WEP_OFF:
			ic->ic_flags &= ~IEEE80211_F_PRIVACY;
			ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
			break;
		case IEEE80211_WEP_ON:
			ic->ic_flags |= IEEE80211_F_PRIVACY;
			ic->ic_flags |= IEEE80211_F_DROPUNENC;
			break;
		case IEEE80211_WEP_MIXED:
			ic->ic_flags |= IEEE80211_F_PRIVACY;
			ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
			break;
		}
		error = ENETRESET;
		break;
	case IEEE80211_IOC_WEPKEY:
		kid = (u_int) ireq->i_val;
		if (kid >= IEEE80211_WEP_NKID)
			return EINVAL;
		k = &ic->ic_nw_keys[kid];
		if (ireq->i_len == 0) {
			/* zero-len =>'s delete any existing key */
			(void) ieee80211_crypto_delkey(ic, k);
			break;
		}
		if (ireq->i_len > sizeof(tmpkey))
			return EINVAL;
		memset(tmpkey, 0, sizeof(tmpkey));
		error = copyin(ireq->i_data, tmpkey, ireq->i_len);
		if (error)
			break;
		ieee80211_key_update_begin(ic);
		k->wk_keyix = kid;	/* NB: force fixed key id */
		if (ieee80211_crypto_newkey(ic, IEEE80211_CIPHER_WEP,
		    IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV, k)) {
			k->wk_keylen = ireq->i_len;
			memcpy(k->wk_key, tmpkey, sizeof(tmpkey));
			if  (!ieee80211_crypto_setkey(ic, k, ic->ic_myaddr))
				error = EINVAL;
		} else
			error = EINVAL;
		ieee80211_key_update_end(ic);
		if (!error)			/* NB: for compatibility */
			error = ENETRESET;
		break;
	case IEEE80211_IOC_WEPTXKEY:
		kid = (u_int) ireq->i_val;
		if (kid >= IEEE80211_WEP_NKID &&
		    (u_int16_t) kid != IEEE80211_KEYIX_NONE)
			return EINVAL;
		ic->ic_def_txkey = kid;
		error = ENETRESET;	/* push to hardware */
		break;
	case IEEE80211_IOC_AUTHMODE:
		switch (ireq->i_val) {
		case IEEE80211_AUTH_WPA:
		case IEEE80211_AUTH_8021X:	/* 802.1x */
		case IEEE80211_AUTH_OPEN:	/* open */
		case IEEE80211_AUTH_SHARED:	/* shared-key */
		case IEEE80211_AUTH_AUTO:	/* auto */
			auth = ieee80211_authenticator_get(ireq->i_val);
			if (auth == NULL)
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		switch (ireq->i_val) {
		case IEEE80211_AUTH_WPA:	/* WPA w/ 802.1x */
			ic->ic_flags |= IEEE80211_F_PRIVACY;
			ireq->i_val = IEEE80211_AUTH_8021X;
			break;
		case IEEE80211_AUTH_OPEN:	/* open */
			ic->ic_flags &= ~(IEEE80211_F_WPA|IEEE80211_F_PRIVACY);
			break;
		case IEEE80211_AUTH_SHARED:	/* shared-key */
		case IEEE80211_AUTH_8021X:	/* 802.1x */
			ic->ic_flags &= ~IEEE80211_F_WPA;
			/* both require a key so mark the PRIVACY capability */
			ic->ic_flags |= IEEE80211_F_PRIVACY;
			break;
		case IEEE80211_AUTH_AUTO:	/* auto */
			ic->ic_flags &= ~IEEE80211_F_WPA;
			/* XXX PRIVACY handling? */
			/* XXX what's the right way to do this? */
			break;
		}
		/* NB: authenticator attach/detach happens on state change */
		ic->ic_bss->ni_authmode = ireq->i_val;
		/* XXX mixed/mode/usage? */
		ic->ic_auth = auth;
		error = ENETRESET;
		break;
	case IEEE80211_IOC_CHANNEL:
		/* XXX 0xffff overflows 16-bit signed */
		if (ireq->i_val == 0 ||
		    ireq->i_val == (int16_t) IEEE80211_CHAN_ANY)
			ic->ic_des_chan = IEEE80211_CHAN_ANYC;
		else if ((u_int) ireq->i_val > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, ireq->i_val)) {
			return EINVAL;
		} else
			ic->ic_ibss_chan = ic->ic_des_chan =
				&ic->ic_channels[ireq->i_val];
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			error = ENETRESET;
			break;
		default:
			/*
			 * If the desired channel has changed (to something
			 * other than any) and we're not already scanning,
			 * then kick the state machine.
			 */
			if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
			    ic->ic_bss->ni_chan != ic->ic_des_chan &&
			    (ic->ic_flags & IEEE80211_F_SCAN) == 0)
				error = ENETRESET;
			break;
		}
		if (error == ENETRESET && ic->ic_opmode == IEEE80211_M_MONITOR)
			error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_POWERSAVE:
		switch (ireq->i_val) {
		case IEEE80211_POWERSAVE_OFF:
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
			break;
		case IEEE80211_POWERSAVE_ON:
			if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
				error = EINVAL;
			else if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case IEEE80211_IOC_POWERSAVESLEEP:
		if (ireq->i_val < 0)
			return EINVAL;
		ic->ic_lintval = ireq->i_val;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_RTSTHRESHOLD:
		if (!(IEEE80211_RTS_MIN <= ireq->i_val &&
		      ireq->i_val <= IEEE80211_RTS_MAX))
			return EINVAL;
		ic->ic_rtsthreshold = ireq->i_val;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_PROTMODE:
		if (ireq->i_val > IEEE80211_PROT_RTSCTS)
			return EINVAL;
		ic->ic_protmode = ireq->i_val;
		/* NB: if not operating in 11g this can wait */
		if (ic->ic_curmode == IEEE80211_MODE_11G)
			error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_TXPOWER:
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
			return EINVAL;
		if (!(IEEE80211_TXPOWER_MIN < ireq->i_val &&
		      ireq->i_val < IEEE80211_TXPOWER_MAX))
			return EINVAL;
		ic->ic_txpowlimit = ireq->i_val;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_ROAMING:
		if (!(IEEE80211_ROAMING_DEVICE <= ireq->i_val &&
		    ireq->i_val <= IEEE80211_ROAMING_MANUAL))
			return EINVAL;
		ic->ic_roaming = ireq->i_val;
		/* XXXX reset? */
		break;
	case IEEE80211_IOC_PRIVACY:
		if (ireq->i_val) {
			/* XXX check for key state? */
			ic->ic_flags |= IEEE80211_F_PRIVACY;
		} else
			ic->ic_flags &= ~IEEE80211_F_PRIVACY;
		break;
	case IEEE80211_IOC_DROPUNENCRYPTED:
		if (ireq->i_val)
			ic->ic_flags |= IEEE80211_F_DROPUNENC;
		else
			ic->ic_flags &= ~IEEE80211_F_DROPUNENC;
		break;
	case IEEE80211_IOC_WPAKEY:
		error = ieee80211_ioctl_setkey(ic, ireq);
		break;
	case IEEE80211_IOC_DELKEY:
		error = ieee80211_ioctl_delkey(ic, ireq);
		break;
	case IEEE80211_IOC_MLME:
		error = ieee80211_ioctl_setmlme(ic, ireq);
		break;
	case IEEE80211_IOC_OPTIE:
		error = ieee80211_ioctl_setoptie(ic, ireq);
		break;
	case IEEE80211_IOC_COUNTERMEASURES:
		if (ireq->i_val) {
			if ((ic->ic_flags & IEEE80211_F_WPA) == 0)
				return EINVAL;
			ic->ic_flags |= IEEE80211_F_COUNTERM;
		} else
			ic->ic_flags &= ~IEEE80211_F_COUNTERM;
		break;
	case IEEE80211_IOC_WPA:
		if (ireq->i_val > 3)
			return EINVAL;
		/* XXX verify ciphers available */
		ic->ic_flags &= ~IEEE80211_F_WPA;
		switch (ireq->i_val) {
		case 1:
			ic->ic_flags |= IEEE80211_F_WPA1;
			break;
		case 2:
			ic->ic_flags |= IEEE80211_F_WPA2;
			break;
		case 3:
			ic->ic_flags |= IEEE80211_F_WPA1 | IEEE80211_F_WPA2;
			break;
		}
		error = ENETRESET;		/* XXX? */
		break;
	case IEEE80211_IOC_WME:
		if (ireq->i_val) {
			if ((ic->ic_caps & IEEE80211_C_WME) == 0)
				return EINVAL;
			ic->ic_flags |= IEEE80211_F_WME;
		} else
			ic->ic_flags &= ~IEEE80211_F_WME;
		error = ENETRESET;		/* XXX maybe not for station? */
		break;
	case IEEE80211_IOC_HIDESSID:
		if (ireq->i_val)
			ic->ic_flags |= IEEE80211_F_HIDESSID;
		else
			ic->ic_flags &= ~IEEE80211_F_HIDESSID;
		error = ENETRESET;
		break;
	case IEEE80211_IOC_APBRIDGE:
		if (ireq->i_val == 0)
			ic->ic_flags |= IEEE80211_F_NOBRIDGE;
		else
			ic->ic_flags &= ~IEEE80211_F_NOBRIDGE;
		break;
	case IEEE80211_IOC_MCASTCIPHER:
		if ((ic->ic_caps & cipher2cap(ireq->i_val)) == 0 &&
		    !ieee80211_crypto_available(ireq->i_val))
			return EINVAL;
		rsn->rsn_mcastcipher = ireq->i_val;
		error = (ic->ic_flags & IEEE80211_F_WPA) ? ENETRESET : 0;
		break;
	case IEEE80211_IOC_MCASTKEYLEN:
		if (!(0 < ireq->i_val && ireq->i_val < IEEE80211_KEYBUF_SIZE))
			return EINVAL;
		/* XXX no way to verify driver capability */
		rsn->rsn_mcastkeylen = ireq->i_val;
		error = (ic->ic_flags & IEEE80211_F_WPA) ? ENETRESET : 0;
		break;
	case IEEE80211_IOC_UCASTCIPHERS:
		/*
		 * Convert user-specified cipher set to the set
		 * we can support (via hardware or software).
		 * NB: this logic intentionally ignores unknown and
		 * unsupported ciphers so folks can specify 0xff or
		 * similar and get all available ciphers.
		 */
		caps = 0;
		for (j = 1; j < 32; j++)	/* NB: skip WEP */
			if ((ireq->i_val & (1<<j)) &&
			    ((ic->ic_caps & cipher2cap(j)) ||
			     ieee80211_crypto_available(j)))
				caps |= 1<<j;
		if (caps == 0)			/* nothing available */
			return EINVAL;
		/* XXX verify ciphers ok for unicast use? */
		/* XXX disallow if running as it'll have no effect */
		rsn->rsn_ucastcipherset = caps;
		error = (ic->ic_flags & IEEE80211_F_WPA) ? ENETRESET : 0;
		break;
	case IEEE80211_IOC_UCASTCIPHER:
		if ((rsn->rsn_ucastcipherset & cipher2cap(ireq->i_val)) == 0)
			return EINVAL;
		rsn->rsn_ucastcipher = ireq->i_val;
		break;
	case IEEE80211_IOC_UCASTKEYLEN:
		if (!(0 < ireq->i_val && ireq->i_val < IEEE80211_KEYBUF_SIZE))
			return EINVAL;
		/* XXX no way to verify driver capability */
		rsn->rsn_ucastkeylen = ireq->i_val;
		break;
	case IEEE80211_IOC_DRIVER_CAPS:
		/* NB: for testing */
		ic->ic_caps = (((u_int16_t) ireq->i_val) << 16) |
			       ((u_int16_t) ireq->i_len);
		break;
	case IEEE80211_IOC_KEYMGTALGS:
		/* XXX check */
		rsn->rsn_keymgmtset = ireq->i_val;
		error = (ic->ic_flags & IEEE80211_F_WPA) ? ENETRESET : 0;
		break;
	case IEEE80211_IOC_RSNCAPS:
		/* XXX check */
		rsn->rsn_caps = ireq->i_val;
		error = (ic->ic_flags & IEEE80211_F_WPA) ? ENETRESET : 0;
		break;
	case IEEE80211_IOC_BSSID:
		/* NB: should only be set when in STA mode */
		if (ic->ic_opmode != IEEE80211_M_STA)
			return EINVAL;
		if (ireq->i_len != sizeof(tmpbssid))
			return EINVAL;
		error = copyin(ireq->i_data, tmpbssid, ireq->i_len);
		if (error)
			break;
		IEEE80211_ADDR_COPY(ic->ic_des_bssid, tmpbssid);
		if (IEEE80211_ADDR_EQ(ic->ic_des_bssid, zerobssid))
			ic->ic_flags &= ~IEEE80211_F_DESBSSID;
		else
			ic->ic_flags |= IEEE80211_F_DESBSSID;
		error = ENETRESET;
		break;
	case IEEE80211_IOC_CHANLIST:
		error = ieee80211_ioctl_setchanlist(ic, ireq);
		break;
	case IEEE80211_IOC_SCAN_REQ:
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)	/* XXX ignore */
			break;
		error = ieee80211_setupscan(ic, ic->ic_chan_avail);
		if (error == 0)		/* XXX background scan */
			error = ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		break;
	case IEEE80211_IOC_ADDMAC:
	case IEEE80211_IOC_DELMAC:
		error = ieee80211_ioctl_macmac(ic, ireq);
		break;
	case IEEE80211_IOC_MACCMD:
		error = ieee80211_ioctl_maccmd(ic, ireq);
		break;
	case IEEE80211_IOC_STA_TXPOW:
		error = ieee80211_ioctl_setstatxpow(ic, ireq);
		break;
	case IEEE80211_IOC_WME_CWMIN:		/* WME: CWmin */
	case IEEE80211_IOC_WME_CWMAX:		/* WME: CWmax */
	case IEEE80211_IOC_WME_AIFS:		/* WME: AIFS */
	case IEEE80211_IOC_WME_TXOPLIMIT:	/* WME: txops limit */
	case IEEE80211_IOC_WME_ACM:		/* WME: ACM (bss only) */
	case IEEE80211_IOC_WME_ACKPOLICY:	/* WME: ACK policy (bss only) */
		error = ieee80211_ioctl_setwmeparam(ic, ireq);
		break;
	case IEEE80211_IOC_DTIM_PERIOD:
		if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
		    ic->ic_opmode != IEEE80211_M_IBSS)
			return EINVAL;
		if (IEEE80211_DTIM_MIN <= ireq->i_val &&
		    ireq->i_val <= IEEE80211_DTIM_MAX) {
			ic->ic_dtim_period = ireq->i_val;
			error = ENETRESET;		/* requires restart */
		} else
			error = EINVAL;
		break;
	case IEEE80211_IOC_BEACON_INTERVAL:
		if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
		    ic->ic_opmode != IEEE80211_M_IBSS)
			return EINVAL;
		if (IEEE80211_BINTVAL_MIN <= ireq->i_val &&
		    ireq->i_val <= IEEE80211_BINTVAL_MAX) {
			ic->ic_bintval = ireq->i_val;
			error = ENETRESET;		/* requires restart */
		} else
			error = EINVAL;
		break;
	case IEEE80211_IOC_PUREG:
		if (ireq->i_val)
			ic->ic_flags |= IEEE80211_F_PUREG;
		else
			ic->ic_flags &= ~IEEE80211_F_PUREG;
		/* NB: reset only if we're operating on an 11g channel */
		if (ic->ic_curmode == IEEE80211_MODE_11G)
			error = ENETRESET;
		break;
	case IEEE80211_IOC_FRAGTHRESHOLD:
		if ((ic->ic_caps & IEEE80211_C_TXFRAG) == 0 &&
		    ireq->i_val != IEEE80211_FRAG_MAX)
			return EINVAL;
		if (!(IEEE80211_FRAG_MIN <= ireq->i_val &&
		      ireq->i_val <= IEEE80211_FRAG_MAX))
			return EINVAL;
		ic->ic_fragthreshold = ireq->i_val;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == ENETRESET && !IS_UP_AUTO(ic))
		error = 0;
	return error;
}

int
ieee80211_ioctl(struct ieee80211com *ic, u_long cmd, caddr_t data)
{
	struct ifnet *ifp = ic->ic_ifp;
	int error = 0;
	struct ifreq *ifr;
	struct ifaddr *ifa;			/* XXX */

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, (struct ifreq *) data,
				&ic->ic_media, cmd);
		break;
	case SIOCG80211:
		error = ieee80211_ioctl_get80211(ic, cmd,
				(struct ieee80211req *) data);
		break;
	case SIOCS80211:
		error = suser(curthread);
		if (error == 0)
			error = ieee80211_ioctl_set80211(ic, cmd,
					(struct ieee80211req *) data);
		break;
	case SIOCGIFGENERIC:
		error = ieee80211_cfgget(ic, cmd, data);
		break;
	case SIOCSIFGENERIC:
		error = suser(curthread);
		if (error)
			break;
		error = ieee80211_cfgset(ic, cmd, data);
		break;
	case SIOCG80211STATS:
		ifr = (struct ifreq *)data;
		copyout(&ic->ic_stats, ifr->ifr_data, sizeof (ic->ic_stats));
		break;
	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;
		if (!(IEEE80211_MTU_MIN <= ifr->ifr_mtu &&
		    ifr->ifr_mtu <= IEEE80211_MTU_MAX))
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFADDR:
		/*
		 * XXX Handle this directly so we can supress if_init calls.
		 * XXX This should be done in ether_ioctl but for the moment
		 * XXX there are too many other parts of the system that
		 * XXX set IFF_UP and so supress if_init being called when
		 * XXX it should be.
		 */
		ifa = (struct ifaddr *) data;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			if ((ifp->if_flags & IFF_UP) == 0) {
				ifp->if_flags |= IFF_UP;
				ifp->if_init(ifp->if_softc);
			}
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef IPX
		/*
		 * XXX - This code is probably wrong,
		 *	 but has been copied many times.
		 */
		case AF_IPX: {
			struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);

			if (ipx_nullhost(*ina))
				ina->x_host = *(union ipx_host *)
				    IFP2ENADDR(ifp);
			else
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) IFP2ENADDR(ifp),
				      ETHER_ADDR_LEN);
			/* fall thru... */
		}
#endif
		default:
			if ((ifp->if_flags & IFF_UP) == 0) {
				ifp->if_flags |= IFF_UP;
				ifp->if_init(ifp->if_softc);
			}
			break;
		}
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return error;
}
