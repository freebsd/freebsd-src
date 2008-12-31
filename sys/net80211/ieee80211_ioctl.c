/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_ioctl.c,v 1.57.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_compat.h"

/*
 * IEEE 802.11 ioctl support (FreeBSD-specific)
 */

#include "opt_inet.h"
#include "opt_ipx.h"

#include <sys/endian.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
 
#include <net/if.h>
#include <net/if_dl.h>
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

#define	IS_UP(_ic) \
	(((_ic)->ic_ifp->if_flags & IFF_UP) &&			\
	    ((_ic)->ic_ifp->if_drv_flags & IFF_DRV_RUNNING))
#define	IS_UP_AUTO(_ic) \
	(IS_UP(_ic) && (_ic)->ic_roaming == IEEE80211_ROAMING_AUTO)
#define	RESCAN	1

static struct ieee80211_channel *findchannel(struct ieee80211com *,
		int ieee, int mode);

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
	if (priv_check(curthread, PRIV_NET80211_GETKEY) == 0) {
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

	if (sizeof(ic->ic_chan_active) < ireq->i_len)
		ireq->i_len = sizeof(ic->ic_chan_active);
	return copyout(&ic->ic_chan_active, ireq->i_data, ireq->i_len);
}

static int
ieee80211_ioctl_getchaninfo(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	int space;

	space = __offsetof(struct ieee80211req_chaninfo,
			ic_chans[ic->ic_nchans]);
	if (space > ireq->i_len)
		space = ireq->i_len;
	/* XXX assumes compatible layout */
	return copyout(&ic->ic_nchans, ireq->i_data, space);
}

static int
ieee80211_ioctl_getwpaie(struct ieee80211com *ic, struct ieee80211req *ireq, int req)
{
	struct ieee80211_node *ni;
	struct ieee80211req_wpaie2 wpaie;
	int error;

	if (ireq->i_len < IEEE80211_ADDR_LEN)
		return EINVAL;
	error = copyin(ireq->i_data, wpaie.wpa_macaddr, IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;
	ni = ieee80211_find_node(&ic->ic_sta, wpaie.wpa_macaddr);
	if (ni == NULL)
		return ENOENT;		/* XXX */
	memset(wpaie.wpa_ie, 0, sizeof(wpaie.wpa_ie));
	if (ni->ni_ies.wpa_ie != NULL) {
		int ielen = ni->ni_ies.wpa_ie[1] + 2;
		if (ielen > sizeof(wpaie.wpa_ie))
			ielen = sizeof(wpaie.wpa_ie);
		memcpy(wpaie.wpa_ie, ni->ni_ies.wpa_ie, ielen);
	}
	if (req == IEEE80211_IOC_WPAIE2) {
		memset(wpaie.rsn_ie, 0, sizeof(wpaie.rsn_ie));
		if (ni->ni_ies.rsn_ie != NULL) {
			int ielen = ni->ni_ies.rsn_ie[1] + 2;
			if (ielen > sizeof(wpaie.rsn_ie))
				ielen = sizeof(wpaie.rsn_ie);
			memcpy(wpaie.rsn_ie, ni->ni_ies.rsn_ie, ielen);
		}
		if (ireq->i_len > sizeof(struct ieee80211req_wpaie2))
			ireq->i_len = sizeof(struct ieee80211req_wpaie2);
	} else {
		/* compatibility op, may overwrite wpa ie */
		/* XXX check ic_flags? */
		if (ni->ni_ies.rsn_ie != NULL) {
			int ielen = ni->ni_ies.rsn_ie[1] + 2;
			if (ielen > sizeof(wpaie.wpa_ie))
				ielen = sizeof(wpaie.wpa_ie);
			memcpy(wpaie.wpa_ie, ni->ni_ies.rsn_ie, ielen);
		}
		if (ireq->i_len > sizeof(struct ieee80211req_wpaie))
			ireq->i_len = sizeof(struct ieee80211req_wpaie);
	}
	ieee80211_free_node(ni);
	return copyout(&wpaie, ireq->i_data, ireq->i_len);
}

static int
ieee80211_ioctl_getstastats(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_node *ni;
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	const int off = __offsetof(struct ieee80211req_sta_stats, is_stats);
	int error;

	if (ireq->i_len < off)
		return EINVAL;
	error = copyin(ireq->i_data, macaddr, IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;
	ni = ieee80211_find_node(&ic->ic_sta, macaddr);
	if (ni == NULL)
		return EINVAL;
	if (ireq->i_len > sizeof(struct ieee80211req_sta_stats))
		ireq->i_len = sizeof(struct ieee80211req_sta_stats);
	/* NB: copy out only the statistics */
	error = copyout(&ni->ni_stats, (uint8_t *) ireq->i_data + off,
			ireq->i_len - off);
	ieee80211_free_node(ni);
	return error;
}

static __inline uint8_t *
copyie(uint8_t *cp, const uint8_t *ie)
{
	if (ie != NULL) {
		memcpy(cp, ie, 2+ie[1]);
		cp += 2+ie[1];
	}
	return cp;
}

#ifdef COMPAT_FREEBSD6
#define	IEEE80211_IOC_SCAN_RESULTS_OLD	24

struct scan_result_old {
	uint16_t	isr_len;		/* length (mult of 4) */
	uint16_t	isr_freq;		/* MHz */
	uint16_t	isr_flags;		/* channel flags */
	uint8_t		isr_noise;
	uint8_t		isr_rssi;
	uint8_t		isr_intval;		/* beacon interval */
	uint8_t		isr_capinfo;		/* capabilities */
	uint8_t		isr_erp;		/* ERP element */
	uint8_t		isr_bssid[IEEE80211_ADDR_LEN];
	uint8_t		isr_nrates;
	uint8_t		isr_rates[IEEE80211_RATE_MAXSIZE];
	uint8_t		isr_ssid_len;		/* SSID length */
	uint8_t		isr_ie_len;		/* IE length */
	uint8_t		isr_pad[5];
	/* variable length SSID followed by IE data */
};

struct oscanreq {
	struct scan_result_old *sr;
	size_t space;
};

static size_t
old_scan_space(const struct ieee80211_scan_entry *se, int *ielen)
{
	size_t len;

	*ielen = 0;
	if (se->se_ies.wpa_ie != NULL)
		*ielen += 2+se->se_ies.wpa_ie[1];
	if (se->se_ies.wme_ie != NULL)
		*ielen += 2+se->se_ies.wme_ie[1];
	/*
	 * NB: ie's can be no more than 255 bytes and the max 802.11
	 * packet is <3Kbytes so we are sure this doesn't overflow
	 * 16-bits; if this is a concern we can drop the ie's.
	 */
	len = sizeof(struct scan_result_old) + se->se_ssid[1] + *ielen;
	return roundup(len, sizeof(uint32_t));
}

static void
old_get_scan_space(void *arg, const struct ieee80211_scan_entry *se)
{
	struct oscanreq *req = arg;
	int ielen;

	req->space += old_scan_space(se, &ielen);
}

static void
old_get_scan_result(void *arg, const struct ieee80211_scan_entry *se)
{
	struct oscanreq *req = arg;
	struct scan_result_old *sr;
	int ielen, len, nr, nxr;
	uint8_t *cp;

	len = old_scan_space(se, &ielen);
	if (len > req->space)
		return;

	sr = req->sr;
	memset(sr, 0, sizeof(*sr));
	sr->isr_ssid_len = se->se_ssid[1];
	/* NB: beware of overflow, isr_ie_len is 8 bits */
	sr->isr_ie_len = (ielen > 255 ? 0 : ielen);
	sr->isr_len = len;
	sr->isr_freq = se->se_chan->ic_freq;
	sr->isr_flags = se->se_chan->ic_flags;
	sr->isr_rssi = se->se_rssi;
	sr->isr_noise = se->se_noise;
	sr->isr_intval = se->se_intval;
	sr->isr_capinfo = se->se_capinfo;
	sr->isr_erp = se->se_erp;
	IEEE80211_ADDR_COPY(sr->isr_bssid, se->se_bssid);
	nr = min(se->se_rates[1], IEEE80211_RATE_MAXSIZE);
	memcpy(sr->isr_rates, se->se_rates+2, nr);
	nxr = min(se->se_xrates[1], IEEE80211_RATE_MAXSIZE - nr);
	memcpy(sr->isr_rates+nr, se->se_xrates+2, nxr);
	sr->isr_nrates = nr + nxr;

	cp = (uint8_t *)(sr+1);
	memcpy(cp, se->se_ssid+2, sr->isr_ssid_len);
	cp += sr->isr_ssid_len;
	if (sr->isr_ie_len) {
		cp = copyie(cp, se->se_ies.wpa_ie);
		cp = copyie(cp, se->se_ies.wme_ie);
	}

	req->space -= len;
	req->sr = (struct scan_result_old *)(((uint8_t *)sr) + len);
}

static int
old_getscanresults(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct oscanreq req;
	int error;

	if (ireq->i_len < sizeof(struct scan_result_old))
		return EFAULT;

	error = 0;
	req.space = 0;
	ieee80211_scan_iterate(ic, old_get_scan_space, &req);
	if (req.space > ireq->i_len)
		req.space = ireq->i_len;
	if (req.space > 0) {
		size_t space;
		void *p;

		space = req.space;
		/* XXX M_WAITOK after driver lock released */
		MALLOC(p, void *, space, M_TEMP, M_NOWAIT | M_ZERO);
		if (p == NULL)
			return ENOMEM;
		req.sr = p;
		ieee80211_scan_iterate(ic, old_get_scan_result, &req);
		ireq->i_len = space - req.space;
		error = copyout(p, ireq->i_data, ireq->i_len);
		FREE(p, M_TEMP);
	} else
		ireq->i_len = 0;

	return error;
}
#endif /* COMPAT_FREEBSD6 */

struct scanreq {
	struct ieee80211req_scan_result *sr;
	size_t space;
};

static size_t
scan_space(const struct ieee80211_scan_entry *se, int *ielen)
{
	size_t len;

	*ielen = se->se_ies.len;
	/*
	 * NB: ie's can be no more than 255 bytes and the max 802.11
	 * packet is <3Kbytes so we are sure this doesn't overflow
	 * 16-bits; if this is a concern we can drop the ie's.
	 */
	len = sizeof(struct ieee80211req_scan_result) + se->se_ssid[1] + *ielen;
	return roundup(len, sizeof(uint32_t));
}

static void
get_scan_space(void *arg, const struct ieee80211_scan_entry *se)
{
	struct scanreq *req = arg;
	int ielen;

	req->space += scan_space(se, &ielen);
}

static void
get_scan_result(void *arg, const struct ieee80211_scan_entry *se)
{
	struct scanreq *req = arg;
	struct ieee80211req_scan_result *sr;
	int ielen, len, nr, nxr;
	uint8_t *cp;

	len = scan_space(se, &ielen);
	if (len > req->space)
		return;

	sr = req->sr;
	KASSERT(len <= 65535 && ielen <= 65535,
	    ("len %u ssid %u ie %u", len, se->se_ssid[1], ielen));
	sr->isr_ie_off = sizeof(struct ieee80211req_scan_result);
	sr->isr_ie_len = ielen;
	sr->isr_len = len;
	sr->isr_freq = se->se_chan->ic_freq;
	sr->isr_flags = se->se_chan->ic_flags;
	sr->isr_rssi = se->se_rssi;
	sr->isr_noise = se->se_noise;
	sr->isr_intval = se->se_intval;
	sr->isr_capinfo = se->se_capinfo;
	sr->isr_erp = se->se_erp;
	IEEE80211_ADDR_COPY(sr->isr_bssid, se->se_bssid);
	nr = min(se->se_rates[1], IEEE80211_RATE_MAXSIZE);
	memcpy(sr->isr_rates, se->se_rates+2, nr);
	nxr = min(se->se_xrates[1], IEEE80211_RATE_MAXSIZE - nr);
	memcpy(sr->isr_rates+nr, se->se_xrates+2, nxr);
	sr->isr_nrates = nr + nxr;

	sr->isr_ssid_len = se->se_ssid[1];
	cp = ((uint8_t *)sr) + sr->isr_ie_off;
	memcpy(cp, se->se_ssid+2, sr->isr_ssid_len);

	if (ielen) {
		cp += sr->isr_ssid_len;
		memcpy(cp, se->se_ies.data, ielen);
	}

	req->space -= len;
	req->sr = (struct ieee80211req_scan_result *)(((uint8_t *)sr) + len);
}

static int
ieee80211_ioctl_getscanresults(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct scanreq req;
	int error;

	if (ireq->i_len < sizeof(struct ieee80211req_scan_result))
		return EFAULT;

	error = 0;
	req.space = 0;
	ieee80211_scan_iterate(ic, get_scan_space, &req);
	if (req.space > ireq->i_len)
		req.space = ireq->i_len;
	if (req.space > 0) {
		size_t space;
		void *p;

		space = req.space;
		/* XXX M_WAITOK after driver lock released */
		MALLOC(p, void *, space, M_TEMP, M_NOWAIT | M_ZERO);
		if (p == NULL)
			return ENOMEM;
		req.sr = p;
		ieee80211_scan_iterate(ic, get_scan_result, &req);
		ireq->i_len = space - req.space;
		error = copyout(p, ireq->i_data, ireq->i_len);
		FREE(p, M_TEMP);
	} else
		ireq->i_len = 0;

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
	*ielen = ni->ni_ies.len;
	return roundup(sizeof(struct ieee80211req_sta_info) + *ielen,
		      sizeof(uint32_t));
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
	uint8_t *cp;

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
	si->isi_ie_off = sizeof(struct ieee80211req_sta_info);
	si->isi_ie_len = ielen;
	si->isi_freq = ni->ni_chan->ic_freq;
	si->isi_flags = ni->ni_chan->ic_flags;
	si->isi_state = ni->ni_flags;
	si->isi_authmode = ni->ni_authmode;
	ic->ic_node_getsignal(ni, &si->isi_rssi, &si->isi_noise);
	si->isi_noise = 0;		/* XXX */
	si->isi_capinfo = ni->ni_capinfo;
	si->isi_erp = ni->ni_erp;
	IEEE80211_ADDR_COPY(si->isi_macaddr, ni->ni_macaddr);
	si->isi_nrates = ni->ni_rates.rs_nrates;
	if (si->isi_nrates > 15)
		si->isi_nrates = 15;
	memcpy(si->isi_rates, ni->ni_rates.rs_rates, si->isi_nrates);
	si->isi_txrate = ni->ni_txrate;
	si->isi_ie_len = ielen;
	si->isi_associd = ni->ni_associd;
	si->isi_txpower = ni->ni_txpower;
	si->isi_vlan = ni->ni_vlan;
	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		memcpy(si->isi_txseqs, ni->ni_txseqs, sizeof(ni->ni_txseqs));
		memcpy(si->isi_rxseqs, ni->ni_rxseqs, sizeof(ni->ni_rxseqs));
	} else {
		si->isi_txseqs[0] = ni->ni_txseqs[IEEE80211_NONQOS_TID];
		si->isi_rxseqs[0] = ni->ni_rxseqs[IEEE80211_NONQOS_TID];
	}
	/* NB: leave all cases in case we relax ni_associd == 0 check */
	if (ieee80211_node_is_authorized(ni))
		si->isi_inact = ic->ic_inact_run;
	else if (ni->ni_associd != 0)
		si->isi_inact = ic->ic_inact_auth;
	else
		si->isi_inact = ic->ic_inact_init;
	si->isi_inact = (si->isi_inact - ni->ni_inact) * IEEE80211_INACT_WAIT;

	if (ielen) {
		cp = ((uint8_t *)si) + si->isi_ie_off;
		memcpy(cp, ni->ni_ies.data, ielen);
	}

	req->si = (struct ieee80211req_sta_info *)(((uint8_t *)si) + len);
	req->space -= len;
}

static int
getstainfo_common(struct ieee80211com *ic, struct ieee80211req *ireq,
	struct ieee80211_node *ni, int off)
{
	struct stainforeq req;
	size_t space;
	void *p;
	int error;

	error = 0;
	req.space = 0;
	if (ni == NULL)
		ieee80211_iterate_nodes(&ic->ic_sta, get_sta_space, &req);
	else
		get_sta_space(&req, ni);
	if (req.space > ireq->i_len)
		req.space = ireq->i_len;
	if (req.space > 0) {
		space = req.space;
		/* XXX M_WAITOK after driver lock released */
		MALLOC(p, void *, space, M_TEMP, M_NOWAIT);
		if (p == NULL) {
			error = ENOMEM;
			goto bad;
		}
		req.si = p;
		if (ni == NULL)
			ieee80211_iterate_nodes(&ic->ic_sta, get_sta_info, &req);
		else
			get_sta_info(&req, ni);
		ireq->i_len = space - req.space;
		error = copyout(p, (uint8_t *) ireq->i_data+off, ireq->i_len);
		FREE(p, M_TEMP);
	} else
		ireq->i_len = 0;
bad:
	if (ni != NULL)
		ieee80211_free_node(ni);
	return error;
}

static int
ieee80211_ioctl_getstainfo(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	const int off = __offsetof(struct ieee80211req_sta_req, info);
	struct ieee80211_node *ni;
	int error;

	if (ireq->i_len < sizeof(struct ieee80211req_sta_req))
		return EFAULT;
	error = copyin(ireq->i_data, macaddr, IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;
	if (IEEE80211_ADDR_EQ(macaddr, ic->ic_ifp->if_broadcastaddr)) {
		ni = NULL;
	} else {
		ni = ieee80211_find_node(&ic->ic_sta, macaddr);
		if (ni == NULL)
			return EINVAL;
	}
	return getstainfo_common(ic, ireq, ni, off);
}

#ifdef COMPAT_FREEBSD6
#define	IEEE80211_IOC_STA_INFO_OLD	45

static int
old_getstainfo(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	if (ireq->i_len < sizeof(struct ieee80211req_sta_info))
		return EFAULT;
	return getstainfo_common(ic, ireq, NULL, 0);
}
#endif /* COMPAT_FREEBSD6 */

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

static int
ieee80211_ioctl_getmaccmd(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	const struct ieee80211_aclator *acl = ic->ic_acl;

	return (acl == NULL ? EINVAL : acl->iac_getioctl(ic, ireq));
}

/*
 * Return the current ``state'' of an Atheros capbility.
 * If associated in station mode report the negotiated
 * setting. Otherwise report the current setting.
 */
static int
getathcap(struct ieee80211com *ic, int cap)
{
	if (ic->ic_opmode == IEEE80211_M_STA && ic->ic_state == IEEE80211_S_RUN)
		return IEEE80211_ATH_CAP(ic, ic->ic_bss, cap) != 0;
	else
		return (ic->ic_flags & cap) != 0;
}

static int
ieee80211_ioctl_getcurchan(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	if (ireq->i_len != sizeof(struct ieee80211_channel))
		return EINVAL;
	return copyout(ic->ic_curchan, ireq->i_data, sizeof(*ic->ic_curchan));
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
	uint8_t tmpkey[IEEE80211_KEYBUF_SIZE];
	char tmpssid[IEEE80211_NWID_LEN];

	switch (ireq->i_type) {
	case IEEE80211_IOC_SSID:
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			ireq->i_len = ic->ic_des_ssid[0].len;
			memcpy(tmpssid, ic->ic_des_ssid[0].ssid, ireq->i_len);
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
		if (priv_check(curthread, PRIV_NET80211_GETKEY) == 0) {
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
		ireq->i_val = ieee80211_chan2ieee(ic, ic->ic_curchan);
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
		error = ieee80211_ioctl_getwpaie(ic, ireq, ireq->i_type);
		break;
	case IEEE80211_IOC_WPAIE2:
		error = ieee80211_ioctl_getwpaie(ic, ireq, ireq->i_type);
		break;
#ifdef COMPAT_FREEBSD6
	case IEEE80211_IOC_SCAN_RESULTS_OLD:
		error = old_getscanresults(ic, ireq);
		break;
#endif
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
#ifdef COMPAT_FREEBSD6
	case IEEE80211_IOC_STA_INFO_OLD:
		error = old_getstainfo(ic, ireq);
		break;
#endif
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
	case IEEE80211_IOC_FF:
		ireq->i_val = getathcap(ic, IEEE80211_F_FF);
		break;
	case IEEE80211_IOC_TURBOP:
		ireq->i_val = getathcap(ic, IEEE80211_F_TURBOP);
		break;
	case IEEE80211_IOC_BGSCAN:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_BGSCAN) != 0;
		break;
	case IEEE80211_IOC_BGSCAN_IDLE:
		ireq->i_val = ic->ic_bgscanidle*hz/1000;	/* ms */
		break;
	case IEEE80211_IOC_BGSCAN_INTERVAL:
		ireq->i_val = ic->ic_bgscanintvl/hz;		/* seconds */
		break;
	case IEEE80211_IOC_SCANVALID:
		ireq->i_val = ic->ic_scanvalid/hz;		/* seconds */
		break;
	case IEEE80211_IOC_ROAM_RSSI_11A:
		ireq->i_val = ic->ic_roam.rssi11a;
		break;
	case IEEE80211_IOC_ROAM_RSSI_11B:
		ireq->i_val = ic->ic_roam.rssi11bOnly;
		break;
	case IEEE80211_IOC_ROAM_RSSI_11G:
		ireq->i_val = ic->ic_roam.rssi11b;
		break;
	case IEEE80211_IOC_ROAM_RATE_11A:
		ireq->i_val = ic->ic_roam.rate11a;
		break;
	case IEEE80211_IOC_ROAM_RATE_11B:
		ireq->i_val = ic->ic_roam.rate11bOnly;
		break;
	case IEEE80211_IOC_ROAM_RATE_11G:
		ireq->i_val = ic->ic_roam.rate11b;
		break;
	case IEEE80211_IOC_MCAST_RATE:
		ireq->i_val = ic->ic_mcast_rate;
		break;
	case IEEE80211_IOC_FRAGTHRESHOLD:
		ireq->i_val = ic->ic_fragthreshold;
		break;
	case IEEE80211_IOC_MACCMD:
		error = ieee80211_ioctl_getmaccmd(ic, ireq);
		break;
	case IEEE80211_IOC_BURST:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_BURST) != 0;
		break;
	case IEEE80211_IOC_BMISSTHRESHOLD:
		ireq->i_val = ic->ic_bmissthreshold;
		break;
	case IEEE80211_IOC_CURCHAN:
		error = ieee80211_ioctl_getcurchan(ic, ireq);
		break;
	case IEEE80211_IOC_SHORTGI:
		ireq->i_val = 0;
		if (ic->ic_flags_ext & IEEE80211_FEXT_SHORTGI20)
			ireq->i_val |= IEEE80211_HTCAP_SHORTGI20;
		if (ic->ic_flags_ext & IEEE80211_FEXT_SHORTGI40)
			ireq->i_val |= IEEE80211_HTCAP_SHORTGI40;
		break;
	case IEEE80211_IOC_AMPDU:
		ireq->i_val = 0;
		if (ic->ic_flags_ext & IEEE80211_FEXT_AMPDU_TX)
			ireq->i_val |= 1;
		if (ic->ic_flags_ext & IEEE80211_FEXT_AMPDU_RX)
			ireq->i_val |= 2;
		break;
	case IEEE80211_IOC_AMPDU_LIMIT:
		ireq->i_val = ic->ic_ampdu_limit;	/* XXX truncation? */
		break;
	case IEEE80211_IOC_AMPDU_DENSITY:
		ireq->i_val = ic->ic_ampdu_density;
		break;
	case IEEE80211_IOC_AMSDU:
		ireq->i_val = 0;
		if (ic->ic_flags_ext & IEEE80211_FEXT_AMSDU_TX)
			ireq->i_val |= 1;
		if (ic->ic_flags_ext & IEEE80211_FEXT_AMSDU_RX)
			ireq->i_val |= 2;
		break;
	case IEEE80211_IOC_AMSDU_LIMIT:
		ireq->i_val = ic->ic_amsdu_limit;	/* XXX truncation? */
		break;
	case IEEE80211_IOC_PUREN:
		ireq->i_val = (ic->ic_flags_ext & IEEE80211_FEXT_PUREN) != 0;
		break;
	case IEEE80211_IOC_DOTH:
		ireq->i_val = (ic->ic_flags & IEEE80211_F_DOTH) != 0;
		break;
	case IEEE80211_IOC_HTCOMPAT:
		ireq->i_val = (ic->ic_flags_ext & IEEE80211_FEXT_HTCOMPAT) != 0;
		break;
	case IEEE80211_IOC_INACTIVITY:
		ireq->i_val = (ic->ic_flags_ext & IEEE80211_FEXT_INACT) != 0;
		break;
	case IEEE80211_IOC_HTPROTMODE:
		ireq->i_val = ic->ic_htprotmode;
		break;
	case IEEE80211_IOC_HTCONF:
		if (ic->ic_flags_ext & IEEE80211_FEXT_HT) {
			ireq->i_val = 1;
			if (ic->ic_flags_ext & IEEE80211_FEXT_USEHT40)
				ireq->i_val |= 2;
		} else
			ireq->i_val = 0;
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
	void *ie, *oie;

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
	/* XXX M_WAITOK after driver lock released */
	if (ireq->i_len > 0) {
		MALLOC(ie, void *, ireq->i_len, M_DEVBUF, M_NOWAIT);
		if (ie == NULL)
			return ENOMEM;
		error = copyin(ireq->i_data, ie, ireq->i_len);
		if (error) {
			FREE(ie, M_DEVBUF);
			return error;
		}
	} else {
		ie = NULL;
		ireq->i_len = 0;
	}
	/* XXX sanity check data? */
	oie = ic->ic_opt_ie;
	ic->ic_opt_ie = ie;
	ic->ic_opt_ie_len = ireq->i_len;
	if (oie != NULL)
		FREE(oie, M_DEVBUF);
	return 0;
}

static int
ieee80211_ioctl_setkey(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211req_key ik;
	struct ieee80211_node *ni;
	struct ieee80211_key *wk;
	uint16_t kid;
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
		/*
		 * Global slots start off w/o any assigned key index.
		 * Force one here for consistency with IEEE80211_IOC_WEPKEY.
		 */
		if (wk->wk_keyix == IEEE80211_KEYIX_NONE)
			wk->wk_keyix = kid;
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
	/* XXX uint8_t -> uint16_t */
	if (dk.idk_keyix == (uint8_t) IEEE80211_KEYIX_NONE) {
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

struct scanlookup {
	const uint8_t *mac;
	int esslen;
	const uint8_t *essid;
	const struct ieee80211_scan_entry *se;
};

/*
 * Match mac address and any ssid.
 */
static void
mlmelookup(void *arg, const struct ieee80211_scan_entry *se)
{
	struct scanlookup *look = arg;

	if (!IEEE80211_ADDR_EQ(look->mac, se->se_macaddr))
		return;
	if (look->esslen != 0) {
		if (se->se_ssid[1] != look->esslen)
			return;
		if (memcmp(look->essid, se->se_ssid+2, look->esslen))
			return;
	}
	look->se = se;
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
		/* XXX ibss/ahdemo */
		if (ic->ic_opmode == IEEE80211_M_STA) {
			struct scanlookup lookup;

			lookup.se = NULL;
			lookup.mac = mlme.im_macaddr;
			/* XXX use revised api w/ explicit ssid */
			lookup.esslen = ic->ic_des_ssid[0].len;
			lookup.essid = ic->ic_des_ssid[0].ssid;
			ieee80211_scan_iterate(ic, mlmelookup, &lookup);
			if (lookup.se != NULL &&
			    ieee80211_sta_join(ic, lookup.se))
				return 0;
		}
		return EINVAL;
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
	uint8_t mac[IEEE80211_ADDR_LEN];
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
ieee80211_ioctl_setmaccmd(struct ieee80211com *ic, struct ieee80211req *ireq)
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
		if (acl == NULL)
			return EINVAL;
		else
			return acl->iac_setioctl(ic, ireq);
	}
	return 0;
}

static int
ieee80211_ioctl_setchanlist(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211req_chanlist list;
	u_char chanlist[IEEE80211_CHAN_BYTES];
	int i, j, nchan, error;

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
	nchan = 0;
	for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
		/*
		 * NB: silently discard unavailable channels so users
		 *     can specify 1-255 to get all available channels.
		 */
		if (isset(list.ic_channels, j) && isset(ic->ic_chan_avail, i)) {
			setbit(chanlist, i);
			nchan++;
		}
	}
	if (nchan == 0)
		return EINVAL;
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&	/* XXX */
	    isclr(chanlist, ic->ic_bsschan->ic_ieee))
		ic->ic_bsschan = IEEE80211_CHAN_ANYC;
	memcpy(ic->ic_chan_active, chanlist, sizeof(ic->ic_chan_active));
	return IS_UP_AUTO(ic) ? ieee80211_init(ic, RESCAN) : 0;
}

static int
ieee80211_ioctl_setstastats(struct ieee80211com *ic, struct ieee80211req *ireq)
{
	struct ieee80211_node *ni;
	uint8_t macaddr[IEEE80211_ADDR_LEN];
	int error;

	/*
	 * NB: we could copyin ieee80211req_sta_stats so apps
	 *     could make selective changes but that's overkill;
	 *     just clear all stats for now.
	 */
	if (ireq->i_len < IEEE80211_ADDR_LEN)
		return EINVAL;
	error = copyin(ireq->i_data, macaddr, IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;
	ni = ieee80211_find_node(&ic->ic_sta, macaddr);
	if (ni == NULL)
		return EINVAL;		/* XXX */
	memset(&ni->ni_stats, 0, sizeof(ni->ni_stats));
	ieee80211_free_node(ni);
	return 0;
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
find11gchannel(struct ieee80211com *ic, int start, int freq)
{
	const struct ieee80211_channel *c;
	int i;

	for (i = start+1; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return 1;
	}
	/* NB: should not be needed but in case things are mis-sorted */
	for (i = 0; i < start; i++) {
		c = &ic->ic_channels[i];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return 1;
	}
	return 0;
}

static struct ieee80211_channel *
findchannel(struct ieee80211com *ic, int ieee, int mode)
{
	static const u_int chanflags[IEEE80211_MODE_MAX] = {
		0,			/* IEEE80211_MODE_AUTO */
		IEEE80211_CHAN_A,	/* IEEE80211_MODE_11A */
		IEEE80211_CHAN_B,	/* IEEE80211_MODE_11B */
		IEEE80211_CHAN_G,	/* IEEE80211_MODE_11G */
		IEEE80211_CHAN_FHSS,	/* IEEE80211_MODE_FH */
		IEEE80211_CHAN_108A,	/* IEEE80211_MODE_TURBO_A */
		IEEE80211_CHAN_108G,	/* IEEE80211_MODE_TURBO_G */
		IEEE80211_CHAN_STURBO,	/* IEEE80211_MODE_STURBO_A */
		/* NB: handled specially below */
		IEEE80211_CHAN_A,	/* IEEE80211_MODE_11NA */
		IEEE80211_CHAN_G,	/* IEEE80211_MODE_11NG */
	};
	u_int modeflags;
	int i;

	KASSERT(mode < IEEE80211_MODE_MAX, ("bad mode %u", mode));
	modeflags = chanflags[mode];
	KASSERT(modeflags != 0 || mode == IEEE80211_MODE_AUTO,
	    ("no chanflags for mode %u", mode));
	for (i = 0; i < ic->ic_nchans; i++) {
		struct ieee80211_channel *c = &ic->ic_channels[i];

		if (c->ic_ieee != ieee)
			continue;
		if (mode == IEEE80211_MODE_AUTO) {
			/* ignore turbo channels for autoselect */
			if (IEEE80211_IS_CHAN_TURBO(c))
				continue;
			/*
			 * XXX special-case 11b/g channels so we
			 *     always select the g channel if both
			 *     are present.
			 * XXX prefer HT to non-HT?
			 */
			if (!IEEE80211_IS_CHAN_B(c) ||
			    !find11gchannel(ic, i, c->ic_freq))
				return c;
		} else {
			/* must check HT specially */
			if ((mode == IEEE80211_MODE_11NA ||
			    mode == IEEE80211_MODE_11NG) &&
			    !IEEE80211_IS_CHAN_HT(c))
				continue;
			if ((c->ic_flags & modeflags) == modeflags)
				return c;
		}
	}
	return NULL;
}

/*
 * Check the specified against any desired mode (aka netband).
 * This is only used (presently) when operating in hostap mode
 * to enforce consistency.
 */
static int
check_mode_consistency(const struct ieee80211_channel *c, int mode)
{
	KASSERT(c != IEEE80211_CHAN_ANYC, ("oops, no channel"));

	switch (mode) {
	case IEEE80211_MODE_11B:
		return (IEEE80211_IS_CHAN_B(c));
	case IEEE80211_MODE_11G:
		return (IEEE80211_IS_CHAN_ANYG(c) && !IEEE80211_IS_CHAN_HT(c));
	case IEEE80211_MODE_11A:
		return (IEEE80211_IS_CHAN_A(c) && !IEEE80211_IS_CHAN_HT(c));
	case IEEE80211_MODE_STURBO_A:
		return (IEEE80211_IS_CHAN_STURBO(c));
	case IEEE80211_MODE_11NA:
		return (IEEE80211_IS_CHAN_HTA(c));
	case IEEE80211_MODE_11NG:
		return (IEEE80211_IS_CHAN_HTG(c));
	}
	return 1;

}

/*
 * Common code to set the current channel.  If the device
 * is up and running this may result in an immediate channel
 * change or a kick of the state machine.
 */
static int
setcurchan(struct ieee80211com *ic, struct ieee80211_channel *c)
{
	int error;

	if (c != IEEE80211_CHAN_ANYC) {
		if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
		    !check_mode_consistency(c, ic->ic_des_mode))
			return EINVAL;
		if (ic->ic_state == IEEE80211_S_RUN && c == ic->ic_curchan)
			return 0;	/* NB: nothing to do */
	}
	ic->ic_des_chan = c;

	error = 0;
	if ((ic->ic_opmode == IEEE80211_M_MONITOR ||
	    ic->ic_opmode == IEEE80211_M_WDS) &&
	    ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
		/*
		 * Monitor and wds modes can switch directly.
		 */
		ic->ic_curchan = ic->ic_des_chan;
		if (ic->ic_state == IEEE80211_S_RUN)
			ic->ic_set_channel(ic);
	} else {
		/*
		 * Need to go through the state machine in case we
		 * need to reassociate or the like.  The state machine
		 * will pickup the desired channel and avoid scanning.
		 */
		if (IS_UP_AUTO(ic))
			error = ieee80211_init(ic, RESCAN);
		else if (ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
			/*
			 * When not up+running and a real channel has
			 * been specified fix the current channel so
			 * there is immediate feedback; e.g. via ifconfig.
			 */
			ic->ic_curchan = ic->ic_des_chan;
		}
	}
	return error;
}

/*
 * Old api for setting the current channel; this is
 * deprecated because channel numbers are ambiguous.
 */
static int
ieee80211_ioctl_setchannel(struct ieee80211com *ic,
	const struct ieee80211req *ireq)
{
	struct ieee80211_channel *c;

	/* XXX 0xffff overflows 16-bit signed */
	if (ireq->i_val == 0 ||
	    ireq->i_val == (int16_t) IEEE80211_CHAN_ANY) {
		c = IEEE80211_CHAN_ANYC;
	} else if ((u_int) ireq->i_val > IEEE80211_CHAN_MAX) {
		return EINVAL;
	} else {
		struct ieee80211_channel *c2;

		c = findchannel(ic, ireq->i_val, ic->ic_des_mode);
		if (c == NULL) {
			c = findchannel(ic, ireq->i_val,
				IEEE80211_MODE_AUTO);
			if (c == NULL)
				return EINVAL;
		}
		/*
		 * Fine tune channel selection based on desired mode:
		 *   if 11b is requested, find the 11b version of any
		 *      11g channel returned,
		 *   if static turbo, find the turbo version of any
		 *	11a channel return,
		 *   if 11na is requested, find the ht version of any
		 *      11a channel returned,
		 *   if 11ng is requested, find the ht version of any
		 *      11g channel returned,
		 *   otherwise we should be ok with what we've got.
		 */
		switch (ic->ic_des_mode) {
		case IEEE80211_MODE_11B:
			if (IEEE80211_IS_CHAN_ANYG(c)) {
				c2 = findchannel(ic, ireq->i_val,
					IEEE80211_MODE_11B);
				/* NB: should not happen, =>'s 11g w/o 11b */
				if (c2 != NULL)
					c = c2;
			}
			break;
		case IEEE80211_MODE_TURBO_A:
			if (IEEE80211_IS_CHAN_A(c)) {
				c2 = findchannel(ic, ireq->i_val,
					IEEE80211_MODE_TURBO_A);
				if (c2 != NULL)
					c = c2;
			}
			break;
		case IEEE80211_MODE_11NA:
			if (IEEE80211_IS_CHAN_A(c)) {
				c2 = findchannel(ic, ireq->i_val,
					IEEE80211_MODE_11NA);
				if (c2 != NULL)
					c = c2;
			}
			break;
		case IEEE80211_MODE_11NG:
			if (IEEE80211_IS_CHAN_ANYG(c)) {
				c2 = findchannel(ic, ireq->i_val,
					IEEE80211_MODE_11NG);
				if (c2 != NULL)
					c = c2;
			}
			break;
		default:		/* NB: no static turboG */
			break;
		}
	}
	return setcurchan(ic, c);
}

/*
 * New/current api for setting the current channel; a complete
 * channel description is provide so there is no ambiguity in
 * identifying the channel.
 */
static int
ieee80211_ioctl_setcurchan(struct ieee80211com *ic,
	const struct ieee80211req *ireq)
{
	struct ieee80211_channel chan, *c;
	int error;

	if (ireq->i_len != sizeof(chan))
		return EINVAL;
	error = copyin(ireq->i_data, &chan, sizeof(chan));
	if (error != 0)
		return error;
	/* XXX 0xffff overflows 16-bit signed */
	if (chan.ic_freq == 0 || chan.ic_freq == IEEE80211_CHAN_ANY) {
		c = IEEE80211_CHAN_ANYC;
	} else {
		c = ieee80211_find_channel(ic, chan.ic_freq, chan.ic_flags);
		if (c == NULL)
			return EINVAL;
	}
	return setcurchan(ic, c);
}

static int
ieee80211_ioctl_set80211(struct ieee80211com *ic, u_long cmd, struct ieee80211req *ireq)
{
	static const uint8_t zerobssid[IEEE80211_ADDR_LEN];
	struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	int error;
	const struct ieee80211_authenticator *auth;
	uint8_t tmpkey[IEEE80211_KEYBUF_SIZE];
	char tmpssid[IEEE80211_NWID_LEN];
	uint8_t tmpbssid[IEEE80211_ADDR_LEN];
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
		memset(ic->ic_des_ssid[0].ssid, 0, IEEE80211_NWID_LEN);
		ic->ic_des_ssid[0].len = ireq->i_len;
		memcpy(ic->ic_des_ssid[0].ssid, tmpssid, ireq->i_len);
		ic->ic_des_nssid = (ireq->i_len > 0);
		if (IS_UP_AUTO(ic))
			error = ieee80211_init(ic, RESCAN);
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
		if (IS_UP_AUTO(ic))
			error = ieee80211_init(ic, RESCAN);
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
		break;
	case IEEE80211_IOC_WEPTXKEY:
		kid = (u_int) ireq->i_val;
		if (kid >= IEEE80211_WEP_NKID &&
		    (uint16_t) kid != IEEE80211_KEYIX_NONE)
			return EINVAL;
		ic->ic_def_txkey = kid;
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
		if (IS_UP_AUTO(ic))
			error = ieee80211_init(ic, RESCAN);
		break;
	case IEEE80211_IOC_CHANNEL:
		error = ieee80211_ioctl_setchannel(ic, ireq);
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
		if (error == ENETRESET) {
			/*
			 * Switching in+out of power save mode
			 * should not require a state change.
			 */
			error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
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
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan))
			error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_TXPOWER:
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
			return EINVAL;
		if (!(IEEE80211_TXPOWER_MIN <= ireq->i_val &&
		      ireq->i_val <= IEEE80211_TXPOWER_MAX))
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
		error = ENETRESET;
		break;
	case IEEE80211_IOC_WME:
		if (ireq->i_val) {
			if ((ic->ic_caps & IEEE80211_C_WME) == 0)
				return EINVAL;
			ic->ic_flags |= IEEE80211_F_WME;
		} else
			ic->ic_flags &= ~IEEE80211_F_WME;
		if (IS_UP_AUTO(ic))
			error = ieee80211_init(ic, 0);
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
		ic->ic_caps = (((uint16_t) ireq->i_val) << 16) |
			       ((uint16_t) ireq->i_len);
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
		if (IS_UP_AUTO(ic))
			error = ieee80211_init(ic, RESCAN);
		break;
	case IEEE80211_IOC_CHANLIST:
		error = ieee80211_ioctl_setchanlist(ic, ireq);
		break;
	case IEEE80211_IOC_SCAN_REQ:
		if (!IS_UP(ic))
			return EINVAL;
		(void) ieee80211_start_scan(ic,
			IEEE80211_SCAN_ACTIVE |
			IEEE80211_SCAN_NOPICK |
			IEEE80211_SCAN_ONCE, IEEE80211_SCAN_FOREVER,
			/* XXX use ioctl params */
			ic->ic_des_nssid, ic->ic_des_ssid);
		break;
	case IEEE80211_IOC_ADDMAC:
	case IEEE80211_IOC_DELMAC:
		error = ieee80211_ioctl_macmac(ic, ireq);
		break;
	case IEEE80211_IOC_MACCMD:
		error = ieee80211_ioctl_setmaccmd(ic, ireq);
		break;
	case IEEE80211_IOC_STA_STATS:
		error = ieee80211_ioctl_setstastats(ic, ireq);
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
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan))
			error = ENETRESET;
		break;
	case IEEE80211_IOC_FF:
		if (ireq->i_val) {
			if ((ic->ic_caps & IEEE80211_C_FF) == 0)
				return EINVAL;
			ic->ic_flags |= IEEE80211_F_FF;
		} else
			ic->ic_flags &= ~IEEE80211_F_FF;
		error = ENETRESET;
		break;
	case IEEE80211_IOC_TURBOP:
		if (ireq->i_val) {
			if ((ic->ic_caps & IEEE80211_C_TURBOP) == 0)
				return EINVAL;
			ic->ic_flags |= IEEE80211_F_TURBOP;
		} else
			ic->ic_flags &= ~IEEE80211_F_TURBOP;
		error = ENETRESET;
		break;
	case IEEE80211_IOC_BGSCAN:
		if (ireq->i_val) {
			if ((ic->ic_caps & IEEE80211_C_BGSCAN) == 0)
				return EINVAL;
			ic->ic_flags |= IEEE80211_F_BGSCAN;
		} else
			ic->ic_flags &= ~IEEE80211_F_BGSCAN;
		break;
	case IEEE80211_IOC_BGSCAN_IDLE:
		if (ireq->i_val >= IEEE80211_BGSCAN_IDLE_MIN)
			ic->ic_bgscanidle = ireq->i_val*hz/1000;
		else
			error = EINVAL;
		break;
	case IEEE80211_IOC_BGSCAN_INTERVAL:
		if (ireq->i_val >= IEEE80211_BGSCAN_INTVAL_MIN)
			ic->ic_bgscanintvl = ireq->i_val*hz;
		else
			error = EINVAL;
		break;
	case IEEE80211_IOC_SCANVALID:
		if (ireq->i_val >= IEEE80211_SCAN_VALID_MIN)
			ic->ic_scanvalid = ireq->i_val*hz;
		else
			error = EINVAL;
		break;
	case IEEE80211_IOC_ROAM_RSSI_11A:
		ic->ic_roam.rssi11a = ireq->i_val;
		break;
	case IEEE80211_IOC_ROAM_RSSI_11B:
		ic->ic_roam.rssi11bOnly = ireq->i_val;
		break;
	case IEEE80211_IOC_ROAM_RSSI_11G:
		ic->ic_roam.rssi11b = ireq->i_val;
		break;
	case IEEE80211_IOC_ROAM_RATE_11A:
		ic->ic_roam.rate11a = ireq->i_val & IEEE80211_RATE_VAL;
		break;
	case IEEE80211_IOC_ROAM_RATE_11B:
		ic->ic_roam.rate11bOnly = ireq->i_val & IEEE80211_RATE_VAL;
		break;
	case IEEE80211_IOC_ROAM_RATE_11G:
		ic->ic_roam.rate11b = ireq->i_val & IEEE80211_RATE_VAL;
		break;
	case IEEE80211_IOC_MCAST_RATE:
		ic->ic_mcast_rate = ireq->i_val & IEEE80211_RATE_VAL;
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
	case IEEE80211_IOC_BURST:
		if (ireq->i_val) {
			if ((ic->ic_caps & IEEE80211_C_BURST) == 0)
				return EINVAL;
			ic->ic_flags |= IEEE80211_F_BURST;
		} else
			ic->ic_flags &= ~IEEE80211_F_BURST;
		error = ENETRESET;		/* XXX maybe not for station? */
		break;
	case IEEE80211_IOC_BMISSTHRESHOLD:
		if (!(IEEE80211_HWBMISS_MIN <= ireq->i_val &&
		      ireq->i_val <= IEEE80211_HWBMISS_MAX))
			return EINVAL;
		ic->ic_bmissthreshold = ireq->i_val;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_CURCHAN:
		error = ieee80211_ioctl_setcurchan(ic, ireq);
		break;
	case IEEE80211_IOC_SHORTGI:
		if (ireq->i_val) {
#define	IEEE80211_HTCAP_SHORTGI \
	(IEEE80211_HTCAP_SHORTGI20 | IEEE80211_HTCAP_SHORTGI40)
			if (((ireq->i_val ^ ic->ic_htcaps) & IEEE80211_HTCAP_SHORTGI) != 0)
				return EINVAL;
			if (ireq->i_val & IEEE80211_HTCAP_SHORTGI20)
				ic->ic_flags_ext |= IEEE80211_FEXT_SHORTGI20;
			if (ireq->i_val & IEEE80211_HTCAP_SHORTGI40)
				ic->ic_flags_ext |= IEEE80211_FEXT_SHORTGI40;
#undef IEEE80211_HTCAP_SHORTGI
		} else
			ic->ic_flags_ext &=
			    ~(IEEE80211_FEXT_SHORTGI20 | IEEE80211_FEXT_SHORTGI40);
		/* XXX kick state machine? */
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_AMPDU:
		if (ireq->i_val) {
			if ((ic->ic_htcaps & IEEE80211_HTC_AMPDU) == 0)
				return EINVAL;
			if (ireq->i_val & 1)
				ic->ic_flags_ext |= IEEE80211_FEXT_AMPDU_TX;
			if (ireq->i_val & 2)
				ic->ic_flags_ext |= IEEE80211_FEXT_AMPDU_RX;
		} else
			ic->ic_flags_ext &=
			    ~(IEEE80211_FEXT_AMPDU_TX|IEEE80211_FEXT_AMPDU_RX);
		/* NB: reset only if we're operating on an 11n channel */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_HT(ic->ic_bsschan))
			error = ENETRESET;
		break;
	case IEEE80211_IOC_AMPDU_LIMIT:
		/* XXX validate */
		ic->ic_ampdu_limit = ireq->i_val;
		break;
	case IEEE80211_IOC_AMPDU_DENSITY:
		/* XXX validate */
		ic->ic_ampdu_density = ireq->i_val;
		break;
	case IEEE80211_IOC_AMSDU:
		if (ireq->i_val) {
			if ((ic->ic_htcaps & IEEE80211_HTC_AMSDU) == 0)
				return EINVAL;
			if (ireq->i_val & 1)
				ic->ic_flags_ext |= IEEE80211_FEXT_AMSDU_TX;
			if (ireq->i_val & 2)
				ic->ic_flags_ext |= IEEE80211_FEXT_AMSDU_RX;
		} else
			ic->ic_flags_ext &=
			    ~(IEEE80211_FEXT_AMSDU_TX|IEEE80211_FEXT_AMSDU_RX);
		/* NB: reset only if we're operating on an 11n channel */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_HT(ic->ic_bsschan))
			error = ENETRESET;
		break;
	case IEEE80211_IOC_AMSDU_LIMIT:
		/* XXX validate */
		ic->ic_amsdu_limit = ireq->i_val;	/* XXX truncation? */
		break;
	case IEEE80211_IOC_PUREN:
		if (ireq->i_val) {
			if ((ic->ic_flags_ext & IEEE80211_FEXT_HT) == 0)
				return EINVAL;
			ic->ic_flags_ext |= IEEE80211_FEXT_PUREN;
		} else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_PUREN;
		/* NB: reset only if we're operating on an 11n channel */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_HT(ic->ic_bsschan))
			error = ENETRESET;
		break;
	case IEEE80211_IOC_DOTH:
		if (ireq->i_val) {
#if 0
			/* XXX no capability */
			if ((ic->ic_caps & IEEE80211_C_DOTH) == 0)
				return EINVAL;
#endif
			ic->ic_flags |= IEEE80211_F_DOTH;
		} else
			ic->ic_flags &= ~IEEE80211_F_DOTH;
		error = IS_UP(ic) ? ic->ic_reset(ic->ic_ifp) : 0;
		break;
	case IEEE80211_IOC_HTCOMPAT:
		if (ireq->i_val) {
			if ((ic->ic_flags_ext & IEEE80211_FEXT_HT) == 0)
				return EINVAL;
			ic->ic_flags_ext |= IEEE80211_FEXT_HTCOMPAT;
		} else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_HTCOMPAT;
		/* NB: reset only if we're operating on an 11n channel */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_HT(ic->ic_bsschan))
			error = ENETRESET;
		break;
	case IEEE80211_IOC_INACTIVITY:
		if (ireq->i_val)
			ic->ic_flags_ext |= IEEE80211_FEXT_INACT;
		else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_INACT;
		break;
	case IEEE80211_IOC_HTPROTMODE:
		if (ireq->i_val > IEEE80211_PROT_RTSCTS)
			return EINVAL;
		ic->ic_htprotmode = ireq->i_val ?
		    IEEE80211_PROT_RTSCTS : IEEE80211_PROT_NONE;
		/* NB: if not operating in 11n this can wait */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    IEEE80211_IS_CHAN_HT(ic->ic_bsschan))
			error = ERESTART;
		break;
	case IEEE80211_IOC_HTCONF:
		if (ireq->i_val & 1)
			ic->ic_flags_ext |= IEEE80211_FEXT_HT;
		else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_HT;
		if (ireq->i_val & 2)
			ic->ic_flags_ext |= IEEE80211_FEXT_USEHT40;
		else
			ic->ic_flags_ext &= ~IEEE80211_FEXT_USEHT40;
		error = ENETRESET;
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == ENETRESET)
		error = IS_UP_AUTO(ic) ? ieee80211_init(ic, 0) : 0;
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
		error = priv_check(curthread, PRIV_NET80211_MANAGE);
		if (error == 0)
			error = ieee80211_ioctl_set80211(ic, cmd,
					(struct ieee80211req *) data);
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
				    IF_LLADDR(ifp);
			else
				bcopy((caddr_t) ina->x_host.c_host,
				      (caddr_t) IF_LLADDR(ifp),
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
