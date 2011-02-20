/*
 * WPA Supplicant - driver interaction with BSD net80211 layer
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * $FreeBSD$
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "common.h"
#include "driver.h"
#include "eloop.h"
#include "l2_packet.h"
#include "ieee802_11_defs.h"

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_ioctl.h>

struct wpa_driver_bsd_data {
	int	sock;			/* open socket for 802.11 ioctls */
	int	route;			/* routing socket for events */
	char	ifname[IFNAMSIZ+1];	/* interface name */
	unsigned int ifindex;		/* interface index */
	void	*ctx;
	int	prev_roaming;		/* roaming state to restore on deinit */
	int	prev_privacy;		/* privacy state to restore on deinit */
	int	prev_wpa;		/* wpa state to restore on deinit */
	int	prev_scanvalid;		/* scan valid to restore on deinit */
	uint8_t	lastssid[IEEE80211_NWID_LEN];
	int	lastssid_len;
	uint32_t drivercaps;		/* general driver capabilities */
	uint32_t cryptocaps;		/* hardware crypto support */
	enum ieee80211_opmode opmode;	/* operation mode */
};

static enum ieee80211_opmode
get80211opmode(struct wpa_driver_bsd_data *drv)
{
	struct ifmediareq ifmr;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, drv->ifname, sizeof(ifmr.ifm_name));

	if (ioctl(drv->sock, SIOCGIFMEDIA, (caddr_t)&ifmr) >= 0) {
		if (ifmr.ifm_current & IFM_IEEE80211_ADHOC) {
			if (ifmr.ifm_current & IFM_FLAG0)
				return IEEE80211_M_AHDEMO;
			else
				return IEEE80211_M_IBSS;
		}
		if (ifmr.ifm_current & IFM_IEEE80211_HOSTAP)
			return IEEE80211_M_HOSTAP;
		if (ifmr.ifm_current & IFM_IEEE80211_MONITOR)
			return IEEE80211_M_MONITOR;
		if (ifmr.ifm_current & IFM_IEEE80211_MBSS)
			return IEEE80211_M_MBSS;
	}
	return IEEE80211_M_STA;
}

static int
set80211var(struct wpa_driver_bsd_data *drv, int op, const void *arg, int arg_len)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;
	ireq.i_len = arg_len;
	ireq.i_data = (void *) arg;

	if (ioctl(drv->sock, SIOCS80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCS80211, op %u, len %u]: %s\n",
			op, arg_len, strerror(errno));
		return -1;
	}
	return 0;
}

static int
get80211var(struct wpa_driver_bsd_data *drv, int op, void *arg, int arg_len)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;
	ireq.i_len = arg_len;
	ireq.i_data = arg;

	if (ioctl(drv->sock, SIOCG80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCG80211, op %u, len %u]: %s\n",
			op, arg_len, strerror(errno));
		return -1;
	}
	return ireq.i_len;
}

static int
set80211param(struct wpa_driver_bsd_data *drv, int op, int arg)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;
	ireq.i_val = arg;

	if (ioctl(drv->sock, SIOCS80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCS80211, op %u, arg 0x%x]: %s\n",
			op, arg, strerror(errno));
		return -1;
	}
	return 0;
}

static int
get80211param(struct wpa_driver_bsd_data *drv, int op)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = op;

	if (ioctl(drv->sock, SIOCG80211, &ireq) < 0) {
		fprintf(stderr, "ioctl[SIOCG80211, op %u]: %s\n",
			op, strerror(errno));
		return -1;
	}
	return ireq.i_val;
}

static int
getifflags(struct wpa_driver_bsd_data *drv, int *flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, drv->ifname, sizeof (ifr.ifr_name));
	if (ioctl(drv->sock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		perror("SIOCGIFFLAGS");
		return errno;
	}
	*flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);
	return 0;
}

static int
setifflags(struct wpa_driver_bsd_data *drv, int flags)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, drv->ifname, sizeof (ifr.ifr_name));
	ifr.ifr_flags = flags & 0xffff;
	ifr.ifr_flagshigh = flags >> 16;
	if (ioctl(drv->sock, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return errno;
	}
	return 0;
}

static int
wpa_driver_bsd_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_bsd_data *drv = priv;

	return get80211var(drv, IEEE80211_IOC_BSSID,
		bssid, IEEE80211_ADDR_LEN) < 0 ? -1 : 0;
}

#if 0
static int
wpa_driver_bsd_set_bssid(void *priv, const char *bssid)
{
	struct wpa_driver_bsd_data *drv = priv;

	return set80211var(drv, IEEE80211_IOC_BSSID,
		bssid, IEEE80211_ADDR_LEN);
}
#endif

static int
wpa_driver_bsd_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_bsd_data *drv = priv;

	return get80211var(drv, IEEE80211_IOC_SSID,
		ssid, IEEE80211_NWID_LEN);
}

static int
wpa_driver_bsd_set_ssid(void *priv, const char *ssid,
			     size_t ssid_len)
{
	struct wpa_driver_bsd_data *drv = priv;

	return set80211var(drv, IEEE80211_IOC_SSID, ssid, ssid_len);
}

static int
wpa_driver_bsd_set_wpa_ie(struct wpa_driver_bsd_data *drv,
	const u8 *wpa_ie, size_t wpa_ie_len)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strncpy(ireq.i_name, drv->ifname, IFNAMSIZ);
	ireq.i_type = IEEE80211_IOC_APPIE;
	ireq.i_val = IEEE80211_APPIE_WPA;
	ireq.i_len = wpa_ie_len;
	ireq.i_data = (void *) wpa_ie;
	if (ioctl(drv->sock, SIOCS80211, &ireq) < 0) {
		fprintf(stderr,
		    "ioctl[IEEE80211_IOC_APPIE:IEEE80211_APPIE_WPA]: %s\n",
		    strerror(errno));
		return -1;
	}
	return 0;
}

static int
wpa_driver_bsd_set_wpa_internal(void *priv, int wpa, int privacy)
{
	struct wpa_driver_bsd_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: wpa=%d privacy=%d",
		__FUNCTION__, wpa, privacy);

	if (!wpa && wpa_driver_bsd_set_wpa_ie(drv, NULL, 0) < 0)
		ret = -1;
	if (set80211param(drv, IEEE80211_IOC_PRIVACY, privacy) < 0)
		ret = -1;
	if (set80211param(drv, IEEE80211_IOC_WPA, wpa) < 0)
		ret = -1;

	return ret;
}

static int
wpa_driver_bsd_del_key(struct wpa_driver_bsd_data *drv, int key_idx,
		       const unsigned char *addr)
{
	struct ieee80211req_del_key wk;

	memset(&wk, 0, sizeof(wk));
	if (addr != NULL &&
	    bcmp(addr, "\xff\xff\xff\xff\xff\xff", IEEE80211_ADDR_LEN) != 0) {
		struct ether_addr ea;

		memcpy(&ea, addr, IEEE80211_ADDR_LEN);
		wpa_printf(MSG_DEBUG, "%s: addr=%s keyidx=%d",
			__func__, ether_ntoa(&ea), key_idx);
		memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.idk_keyix = (uint8_t) IEEE80211_KEYIX_NONE;
	} else {
		wpa_printf(MSG_DEBUG, "%s: keyidx=%d", __func__, key_idx);
		wk.idk_keyix = key_idx;
	}
	return set80211var(drv, IEEE80211_IOC_DELKEY, &wk, sizeof(wk));
}

static int
wpa_driver_bsd_set_key(const char *ifname, void *priv, enum wpa_alg alg,
		       const unsigned char *addr, int key_idx, int set_tx,
		       const u8 *seq, size_t seq_len,
		       const u8 *key, size_t key_len)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_key wk;
	struct ether_addr ea;
	char *alg_name;
	u_int8_t cipher;

	if (alg == WPA_ALG_NONE)
		return wpa_driver_bsd_del_key(drv, key_idx, addr);

	switch (alg) {
	case WPA_ALG_WEP:
		alg_name = "WEP";
		cipher = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
	default:
		wpa_printf(MSG_DEBUG, "%s: unknown/unsupported algorithm %d",
			__func__, alg);
		return -1;
	}

	memcpy(&ea, addr, IEEE80211_ADDR_LEN);
	wpa_printf(MSG_DEBUG,
	    "%s: alg=%s addr=%s key_idx=%d set_tx=%d seq_len=%zu key_len=%zu",
	    __func__, alg_name, ether_ntoa(&ea), key_idx, set_tx,
	    seq_len, key_len);

	if (seq_len > sizeof(u_int64_t)) {
		wpa_printf(MSG_DEBUG, "%s: seq_len %zu too big",
			__func__, seq_len);
		return -2;
	}
	if (key_len > sizeof(wk.ik_keydata)) {
		wpa_printf(MSG_DEBUG, "%s: key length %zu too big",
			__func__, key_len);
		return -3;
	}

	memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV;
	if (set_tx)
		wk.ik_flags |= IEEE80211_KEY_XMIT;
	memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	/*
	 * Deduce whether group/global or unicast key by checking
	 * the address (yech).  Note also that we can only mark global
	 * keys default; doing this for a unicast key is an error.
	 */
	if (bcmp(addr, "\xff\xff\xff\xff\xff\xff", IEEE80211_ADDR_LEN) == 0) {
		wk.ik_flags |= IEEE80211_KEY_GROUP;
		wk.ik_keyix = key_idx;
	} else {
		wk.ik_keyix = (key_idx == 0 ? IEEE80211_KEYIX_NONE : key_idx);
	}
	if (wk.ik_keyix != IEEE80211_KEYIX_NONE && set_tx)
		wk.ik_flags |= IEEE80211_KEY_DEFAULT;
	/*
	 * Ignore replay failures in IBSS and AHDEMO mode.
	 */
	if (drv->opmode == IEEE80211_M_IBSS ||
	    drv->opmode == IEEE80211_M_AHDEMO)
		wk.ik_flags |= IEEE80211_KEY_NOREPLAY;
	wk.ik_keylen = key_len;
	memcpy(&wk.ik_keyrsc, seq, seq_len);
	wk.ik_keyrsc = le64toh(wk.ik_keyrsc);
	memcpy(wk.ik_keydata, key, key_len);

	return set80211var(drv, IEEE80211_IOC_WPAKEY, &wk, sizeof(wk));
}

static int
wpa_driver_bsd_set_countermeasures(void *priv, int enabled)
{
	struct wpa_driver_bsd_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
	return set80211param(drv, IEEE80211_IOC_COUNTERMEASURES, enabled);
}


static int
wpa_driver_bsd_set_drop_unencrypted(void *priv, int enabled)
{
	struct wpa_driver_bsd_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
	return set80211param(drv, IEEE80211_IOC_DROPUNENCRYPTED, enabled);
}

static int
wpa_driver_bsd_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_bsd_data *drv = priv;
	int authmode;

	if ((auth_alg & WPA_AUTH_ALG_OPEN) &&
	    (auth_alg & WPA_AUTH_ALG_SHARED))
		authmode = IEEE80211_AUTH_AUTO;
	else if (auth_alg & WPA_AUTH_ALG_SHARED)
		authmode = IEEE80211_AUTH_SHARED;
	else
		authmode = IEEE80211_AUTH_OPEN;

	wpa_printf(MSG_DEBUG, "%s alg 0x%x authmode %u",
		__func__, auth_alg, authmode);

	return set80211param(drv, IEEE80211_IOC_AUTHMODE, authmode);
}

static int
wpa_driver_bsd_deauthenticate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_mlme mlme;

	drv->lastssid_len = 0;

	wpa_printf(MSG_DEBUG, "%s", __func__);
	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211var(drv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme));
}

static int
wpa_driver_bsd_disassociate(void *priv, const u8 *addr, int reason_code)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_mlme mlme;

	drv->lastssid_len = 0;

	wpa_printf(MSG_DEBUG, "%s", __func__);
	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211var(drv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme));
}

static int
wpa_driver_bsd_associate(void *priv, struct wpa_driver_associate_params *params)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int flags, privacy;

	wpa_printf(MSG_DEBUG,
		"%s: ssid '%.*s' wpa ie len %u pairwise %u group %u key mgmt %u"
		, __func__
		, params->ssid_len, params->ssid
		, params->wpa_ie_len
		, params->pairwise_suite
		, params->group_suite
		, params->key_mgmt_suite
	);

	/* NB: interface must be marked UP to associate */
	if (getifflags(drv, &flags) != 0) {
		wpa_printf(MSG_DEBUG, "%s did not mark interface UP", __func__);
		return -1;
	}
	if ((flags & IFF_UP) == 0 && setifflags(drv, flags | IFF_UP) != 0) {
		wpa_printf(MSG_DEBUG, "%s unable to mark interface UP",
		    __func__);
		return -1;
	}

        if (wpa_driver_bsd_set_drop_unencrypted(drv, params->drop_unencrypted)
            < 0)
                return -1;
	if (wpa_driver_bsd_set_auth_alg(drv, params->auth_alg) < 0)
		return -1;
	/* XXX error handling is wrong but unclear what to do... */
	if (wpa_driver_bsd_set_wpa_ie(drv, params->wpa_ie, params->wpa_ie_len) < 0)
		return -1;

	privacy = !(params->pairwise_suite == CIPHER_NONE &&
	    params->group_suite == CIPHER_NONE &&
	    params->key_mgmt_suite == KEY_MGMT_NONE &&
	    params->wpa_ie_len == 0);
	wpa_printf(MSG_DEBUG, "%s: set PRIVACY %u", __func__, privacy);

	if (set80211param(drv, IEEE80211_IOC_PRIVACY, privacy) < 0)
		return -1;

	if (params->wpa_ie_len &&
	    set80211param(drv, IEEE80211_IOC_WPA,
			  params->wpa_ie[0] == WLAN_EID_RSN ? 2 : 1) < 0)
		return -1;

	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_ASSOC;
	if (params->ssid != NULL)
		memcpy(mlme.im_ssid, params->ssid, params->ssid_len);
	mlme.im_ssid_len = params->ssid_len;
	if (params->bssid != NULL)
		memcpy(mlme.im_macaddr, params->bssid, IEEE80211_ADDR_LEN);
	if (set80211var(drv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme)) < 0)
		return -1;
	memcpy(drv->lastssid, params->ssid, params->ssid_len);
	drv->lastssid_len = params->ssid_len;
	return 0;
}

static int
wpa_driver_bsd_scan(void *priv, struct wpa_driver_scan_params *params)
{
	struct wpa_driver_bsd_data *drv = priv;
	struct ieee80211_scan_req sr;
	int i;
	int flags;

	/* XXX not true but easiest to perpetuate the myth */
	/* NB: interface must be marked UP to do a scan */
	if (getifflags(drv, &flags) != 0) {
		wpa_printf(MSG_DEBUG, "%s did not mark interface UP", __func__);
		return -1;
	}
	if ((flags & IFF_UP) == 0 && setifflags(drv, flags | IFF_UP) != 0) {
		wpa_printf(MSG_DEBUG, "%s unable to mark interface UP",
		    __func__);
		return -1;
	}

	memset(&sr, 0, sizeof(sr));
	sr.sr_flags = IEEE80211_IOC_SCAN_ACTIVE
		    | IEEE80211_IOC_SCAN_ONCE
		    | IEEE80211_IOC_SCAN_NOJOIN
		    ;
	sr.sr_duration = IEEE80211_IOC_SCAN_FOREVER;
	if (params->num_ssids > 0) {
		sr.sr_nssid = params->num_ssids;
#if 0
		/* Boundary check is done by upper layer */
		if (sr.sr_nssid > IEEE80211_IOC_SCAN_MAX_SSID)
			sr.sr_nssid = IEEE80211_IOC_SCAN_MAX_SSID;
#endif
		/* NB: check scan cache first */
		sr.sr_flags |= IEEE80211_IOC_SCAN_CHECK;
}
	for (i = 0; i < sr.sr_nssid; i++) {
		sr.sr_ssid[i].len = params->ssids[i].ssid_len;
		os_memcpy(sr.sr_ssid[i].ssid, params->ssids[i].ssid,
			  sr.sr_ssid[i].len);
	}
	/* NB: net80211 delivers a scan complete event so no need to poll */
	return set80211var(drv, IEEE80211_IOC_SCAN_REQ, &sr, sizeof(sr));
}

#include <net/route.h>
#include <net80211/ieee80211_freebsd.h>

static void
wpa_driver_bsd_event_receive(int sock, void *ctx, void *sock_ctx)
{
	struct wpa_driver_bsd_data *drv = sock_ctx;
	char buf[2048];
	struct if_announcemsghdr *ifan;
	struct if_msghdr *ifm;
	struct rt_msghdr *rtm;
	union wpa_event_data event;
	struct ieee80211_michael_event *mic;
	int n;

	n = read(sock, buf, sizeof(buf));
	if (n < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("read(PF_ROUTE)");
		return;
	}

	rtm = (struct rt_msghdr *) buf;
	if (rtm->rtm_version != RTM_VERSION) {
		wpa_printf(MSG_DEBUG, "Routing message version %d not "
			"understood\n", rtm->rtm_version);
		return;
	}
	memset(&event, 0, sizeof(event));
	switch (rtm->rtm_type) {
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *) rtm;
		if (ifan->ifan_index != drv->ifindex)
			break;
		strlcpy(event.interface_status.ifname, drv->ifname,
			sizeof(event.interface_status.ifname));
		switch (ifan->ifan_what) {
		case IFAN_DEPARTURE:
			event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
		default:
			return;
		}
		wpa_printf(MSG_DEBUG, "RTM_IFANNOUNCE: Interface '%s' %s",
			   event.interface_status.ifname,
			   ifan->ifan_what == IFAN_DEPARTURE ?
				"removed" : "added");
		wpa_supplicant_event(ctx, EVENT_INTERFACE_STATUS, &event);
		break;
	case RTM_IEEE80211:
		ifan = (struct if_announcemsghdr *) rtm;
		if (ifan->ifan_index != drv->ifindex)
			break;
		switch (ifan->ifan_what) {
		case RTM_IEEE80211_ASSOC:
		case RTM_IEEE80211_REASSOC:
			wpa_supplicant_event(ctx, EVENT_ASSOC, NULL);
			break;
		case RTM_IEEE80211_DISASSOC:
			wpa_supplicant_event(ctx, EVENT_DISASSOC, NULL);
			break;
		case RTM_IEEE80211_SCAN:
			wpa_supplicant_event(ctx, EVENT_SCAN_RESULTS, NULL);
			break;
		case RTM_IEEE80211_REPLAY:
			/* ignore */
			break;
		case RTM_IEEE80211_MICHAEL:
			mic = (struct ieee80211_michael_event *) &ifan[1];
			wpa_printf(MSG_DEBUG,
				"Michael MIC failure wireless event: "
				"keyix=%u src_addr=" MACSTR, mic->iev_keyix,
				MAC2STR(mic->iev_src));

			memset(&event, 0, sizeof(event));
			event.michael_mic_failure.unicast =
				!IEEE80211_IS_MULTICAST(mic->iev_dst);
			wpa_supplicant_event(ctx, EVENT_MICHAEL_MIC_FAILURE,
				&event);
			break;
		}
		break;
	case RTM_IFINFO:
		ifm = (struct if_msghdr *) rtm;
		if (ifm->ifm_index != drv->ifindex)
			break;
		if ((rtm->rtm_flags & RTF_UP) == 0) {
			strlcpy(event.interface_status.ifname, drv->ifname,
				sizeof(event.interface_status.ifname));
			event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
			wpa_printf(MSG_DEBUG, "RTM_IFINFO: Interface '%s' DOWN",
				   event.interface_status.ifname);
			wpa_supplicant_event(ctx, EVENT_INTERFACE_STATUS, &event);
		}
		break;
	}
}

static int
getmaxrate(const uint8_t rates[15], uint8_t nrates)
{
	int i, maxrate = -1;

	for (i = 0; i < nrates; i++) {
		int rate = rates[i] & IEEE80211_RATE_VAL;
		if (rate > maxrate)
			rate = maxrate;
	}
	return maxrate;
}

/* unalligned little endian access */     
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

static int __inline
iswpaoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}


static void
wpa_driver_bsd_add_scan_entry(struct wpa_scan_results *res,
                              struct ieee80211req_scan_result *sr)
{
        struct wpa_scan_res *result, **tmp;
        size_t extra_len;
        u8 *pos;

        extra_len = 2 + sr->isr_ssid_len;
        extra_len += 2 + sr->isr_nrates;
        extra_len += 3; /* ERP IE */
        extra_len += sr->isr_ie_len;

        result = os_zalloc(sizeof(*result) + extra_len);
        if (result == NULL)
                return;
        os_memcpy(result->bssid, sr->isr_bssid, ETH_ALEN);
        result->freq = sr->isr_freq;
        result->beacon_int = sr->isr_intval;
        result->caps = sr->isr_capinfo;
        result->qual = sr->isr_rssi;
        result->noise = sr->isr_noise;

        pos = (u8 *)(result + 1);

        *pos++ = WLAN_EID_SSID;
        *pos++ = sr->isr_ssid_len;
        os_memcpy(pos, sr + 1, sr->isr_ssid_len);
        pos += sr->isr_ssid_len;

        /*
         * Deal all rates as supported rate.
         * Because net80211 doesn't report extended supported rate or not.
         */
        *pos++ = WLAN_EID_SUPP_RATES;
        *pos++ = sr->isr_nrates;
        os_memcpy(pos, sr->isr_rates, sr->isr_nrates);
       pos += sr->isr_nrates;

        *pos++ = WLAN_EID_ERP_INFO;
        *pos++ = 1;
        *pos++ = sr->isr_erp;

        os_memcpy(pos, (u8 *)(sr + 1) + sr->isr_ssid_len, sr->isr_ie_len);
        pos += sr->isr_ie_len;

        result->ie_len = pos - (u8 *)(result + 1);

        tmp = os_realloc(res->res,
                         (res->num + 1) * sizeof(struct wpa_scan_res *));
        if (tmp == NULL) {
                os_free(result);
                return;
        }
        tmp[res->num++] = result;
        res->res = tmp;
}

static struct wpa_scan_results *
wpa_driver_bsd_get_scan_results2(void *priv)
{
	struct ieee80211req_scan_result *sr;
	struct wpa_scan_results *res;
	int len, rest;
	uint8_t buf[24*1024], *pos;

	len = get80211var(priv, IEEE80211_IOC_SCAN_RESULTS, buf, 24*1024);
	if (len < 0)
		return NULL;

	res = os_zalloc(sizeof(*res));
	if (res == NULL)
		return NULL;

	pos = buf;
	rest = len;
	while (rest >= sizeof(struct ieee80211req_scan_result)) {
		sr = (struct ieee80211req_scan_result *)pos;
		wpa_driver_bsd_add_scan_entry(res, sr);
		pos += sr->isr_len;
		rest -= sr->isr_len;
	}

	wpa_printf(MSG_DEBUG, "Received %d bytes of scan results (%lu BSSes)",
		len, (unsigned long)res->num);

	return (res);
}


#define	GETPARAM(drv, param, v) \
	(((v) = get80211param(drv, param)) != -1)
#define	IEEE80211_C_BGSCAN	0x20000000

/*
 * Set the scan cache valid threshold to 1.5 x bg scan interval
 * to force all scan requests to consult the cache unless they
 * explicitly bypass it.
 */
static int
setscanvalid(struct wpa_driver_bsd_data *drv)
{
	int bgscan, scanvalid;

	if (!GETPARAM(drv, IEEE80211_IOC_SCANVALID, drv->prev_scanvalid) ||
	    !GETPARAM(drv, IEEE80211_IOC_BGSCAN_INTERVAL, bgscan))
		return -1;
	scanvalid = 3*bgscan/2;
	return (drv->prev_scanvalid < scanvalid) ?
	    set80211param(drv, IEEE80211_IOC_SCANVALID, scanvalid) : 0;
}

static void *
wpa_driver_bsd_init(void *ctx, const char *ifname)
{
	struct wpa_driver_bsd_data *drv;
	struct ieee80211_devcaps_req devcaps;
	int flags;

	drv = malloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	memset(drv, 0, sizeof(*drv));
	/*
	 * NB: We require the interface name be mappable to an index.
	 *     This implies we do not support having wpa_supplicant
	 *     wait for an interface to appear.  This seems ok; that
	 *     doesn't belong here; it's really the job of devd.
	 */
	drv->ifindex = if_nametoindex(ifname);
	if (drv->ifindex == 0) {
		wpa_printf(MSG_DEBUG, "%s: interface %s does not exist",
			   __func__, ifname);
		goto fail1;
	}
	drv->sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->sock < 0)
		goto fail1;
	drv->ctx = ctx;
	strncpy(drv->ifname, ifname, sizeof(drv->ifname));

	/*
	 * Mark the interface as down to ensure wpa_supplicant has exclusive
	 * access to the net80211 state machine, do this before opening the
	 * route socket to avoid a false event that the interface disappeared.
	 */
	if (getifflags(drv, &flags) == 0)
		(void) setifflags(drv, flags &~ IFF_UP);

	drv->route = socket(PF_ROUTE, SOCK_RAW, 0);
	if (drv->route < 0)
		goto fail;
	eloop_register_read_sock(drv->route,
		wpa_driver_bsd_event_receive, ctx, drv);

	if (get80211var(drv, IEEE80211_IOC_DEVCAPS, &devcaps, sizeof(devcaps)) < 0) {
		wpa_printf(MSG_DEBUG,
		    "%s: failed to get device capabilities: %s",
		    __func__, strerror(errno));
		goto fail;
	}
	drv->drivercaps = devcaps.dc_drivercaps;
	drv->cryptocaps = devcaps.dc_cryptocaps;

	if (!GETPARAM(drv, IEEE80211_IOC_ROAMING, drv->prev_roaming)) {
		wpa_printf(MSG_DEBUG, "%s: failed to get roaming state: %s",
			__func__, strerror(errno));
		goto fail;
	}
	if (!GETPARAM(drv, IEEE80211_IOC_PRIVACY, drv->prev_privacy)) {
		wpa_printf(MSG_DEBUG, "%s: failed to get privacy state: %s",
			__func__, strerror(errno));
		goto fail;
	}
	if (!GETPARAM(drv, IEEE80211_IOC_WPA, drv->prev_wpa)) {
		wpa_printf(MSG_DEBUG, "%s: failed to get wpa state: %s",
			__func__, strerror(errno));
		goto fail;
	}
	if (set80211param(drv, IEEE80211_IOC_ROAMING, IEEE80211_ROAMING_MANUAL) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to set wpa_supplicant-based "
			   "roaming: %s", __func__, strerror(errno));
		goto fail;
	}
	if (drv->drivercaps & IEEE80211_C_BGSCAN) {
		/*
		 * Driver does background scanning; force the scan valid
		 * setting to 1.5 x bg scan interval so the scan cache is
		 * always consulted before we force a foreground scan.
		 */ 
		if (setscanvalid(drv) < 0) {
			wpa_printf(MSG_DEBUG,
			    "%s: warning, failed to set scanvalid, scanning "
			    "may be suboptimal: %s", __func__, strerror(errno));
		}
	}
	if (set80211param(drv, IEEE80211_IOC_WPA, 1+2) < 0) {
		wpa_printf(MSG_DEBUG, "%s: failed to enable WPA support %s",
			   __func__, strerror(errno));
		goto fail;
	}
	drv->opmode = get80211opmode(drv);

	return drv;
fail:
	close(drv->sock);
fail1:
	free(drv);
	return NULL;
}
#undef GETPARAM

static void
wpa_driver_bsd_deinit(void *priv)
{
	struct wpa_driver_bsd_data *drv = priv;
	int flags;

	/* NB: mark interface down */
	if (getifflags(drv, &flags) == 0)
		(void) setifflags(drv, flags &~ IFF_UP);

	wpa_driver_bsd_set_wpa_internal(drv, drv->prev_wpa, drv->prev_privacy);
	if (set80211param(drv, IEEE80211_IOC_ROAMING, drv->prev_roaming) < 0) {
		/* NB: don't whinge if device ejected or equivalent */
		if (errno != ENXIO)
			wpa_printf(MSG_DEBUG, "%s: failed to restore roaming "
			    "state", __func__);
	}
	if (drv->drivercaps & IEEE80211_C_BGSCAN) {
		/* XXX check return value */
		(void) set80211param(drv, IEEE80211_IOC_SCANVALID,
		    drv->prev_scanvalid);
	}

	(void) close(drv->route);		/* ioctl socket */
	(void) close(drv->sock);		/* event socket */
	free(drv);
}


struct wpa_driver_ops wpa_driver_bsd_ops = {
	.name			= "bsd",
	.desc			= "BSD 802.11 support (Atheros, etc.)",
	.init			= wpa_driver_bsd_init,
	.deinit			= wpa_driver_bsd_deinit,
	.get_bssid		= wpa_driver_bsd_get_bssid,
	.get_ssid		= wpa_driver_bsd_get_ssid,
	.set_key		= wpa_driver_bsd_set_key,
	.set_countermeasures	= wpa_driver_bsd_set_countermeasures,
	.scan2			= wpa_driver_bsd_scan,
	.get_scan_results2	= wpa_driver_bsd_get_scan_results2,
	.deauthenticate		= wpa_driver_bsd_deauthenticate,
	.disassociate		= wpa_driver_bsd_disassociate,
	.associate		= wpa_driver_bsd_associate,
};
