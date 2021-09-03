/*
 * hostapd / Driver interaction with Atheros driver
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004, Video54 Technologies
 * Copyright (c) 2005-2007, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2009, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <net/if.h>
#include <sys/ioctl.h>

#include "common.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "l2_packet/l2_packet.h"
#include "p2p/p2p.h"

#include "common.h"
#ifndef _BYTE_ORDER
#ifdef WORDS_BIGENDIAN
#define _BYTE_ORDER _BIG_ENDIAN
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif
#endif /* _BYTE_ORDER */

/*
 * Note, the ATH_WPS_IE setting must match with the driver build.. If the
 * driver does not include this, the IEEE80211_IOCTL_GETWPAIE ioctl will fail.
 */
#define ATH_WPS_IE

#include "ieee80211_external.h"

/* Avoid conflicting definition from the driver header files with
 * common/wpa_common.h */
#undef WPA_OUI_TYPE


#ifdef CONFIG_WPS
#include <netpacket/packet.h>
#endif /* CONFIG_WPS */

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW 0x0019
#endif

#include "linux_wext.h"

#include "driver.h"
#include "eloop.h"
#include "priv_netlink.h"
#include "l2_packet/l2_packet.h"
#include "common/ieee802_11_defs.h"
#include "netlink.h"
#include "linux_ioctl.h"


struct atheros_driver_data {
	struct hostapd_data *hapd;		/* back pointer */

	char	iface[IFNAMSIZ + 1];
	int     ifindex;
	struct l2_packet_data *sock_xmit;	/* raw packet xmit socket */
	struct l2_packet_data *sock_recv;	/* raw packet recv socket */
	int	ioctl_sock;			/* socket for ioctl() use */
	struct netlink_data *netlink;
	int	we_version;
	int fils_en;			/* FILS enable/disable in driver */
	u8	acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;

	struct l2_packet_data *sock_raw; /* raw 802.11 management frames */
	struct wpabuf *wpa_ie;
	struct wpabuf *wps_beacon_ie;
	struct wpabuf *wps_probe_resp_ie;
	u8	own_addr[ETH_ALEN];
};

static int atheros_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
			      u16 reason_code);
static int atheros_set_privacy(void *priv, int enabled);

static const char * athr_get_ioctl_name(int op)
{
	switch (op) {
	case IEEE80211_IOCTL_SETPARAM:
		return "SETPARAM";
	case IEEE80211_IOCTL_GETPARAM:
		return "GETPARAM";
	case IEEE80211_IOCTL_SETKEY:
		return "SETKEY";
	case IEEE80211_IOCTL_SETWMMPARAMS:
		return "SETWMMPARAMS";
	case IEEE80211_IOCTL_DELKEY:
		return "DELKEY";
	case IEEE80211_IOCTL_GETWMMPARAMS:
		return "GETWMMPARAMS";
	case IEEE80211_IOCTL_SETMLME:
		return "SETMLME";
	case IEEE80211_IOCTL_GETCHANINFO:
		return "GETCHANINFO";
	case IEEE80211_IOCTL_SETOPTIE:
		return "SETOPTIE";
	case IEEE80211_IOCTL_GETOPTIE:
		return "GETOPTIE";
	case IEEE80211_IOCTL_ADDMAC:
		return "ADDMAC";
	case IEEE80211_IOCTL_DELMAC:
		return "DELMAC";
	case IEEE80211_IOCTL_GETCHANLIST:
		return "GETCHANLIST";
	case IEEE80211_IOCTL_SETCHANLIST:
		return "SETCHANLIST";
	case IEEE80211_IOCTL_KICKMAC:
		return "KICKMAC";
	case IEEE80211_IOCTL_CHANSWITCH:
		return "CHANSWITCH";
	case IEEE80211_IOCTL_GETMODE:
		return "GETMODE";
	case IEEE80211_IOCTL_SETMODE:
		return "SETMODE";
	case IEEE80211_IOCTL_GET_APPIEBUF:
		return "GET_APPIEBUF";
	case IEEE80211_IOCTL_SET_APPIEBUF:
		return "SET_APPIEBUF";
	case IEEE80211_IOCTL_SET_ACPARAMS:
		return "SET_ACPARAMS";
	case IEEE80211_IOCTL_FILTERFRAME:
		return "FILTERFRAME";
	case IEEE80211_IOCTL_SET_RTPARAMS:
		return "SET_RTPARAMS";
	case IEEE80211_IOCTL_SET_MEDENYENTRY:
		return "SET_MEDENYENTRY";
	case IEEE80211_IOCTL_GET_MACADDR:
		return "GET_MACADDR";
	case IEEE80211_IOCTL_SET_HBRPARAMS:
		return "SET_HBRPARAMS";
	case IEEE80211_IOCTL_SET_RXTIMEOUT:
		return "SET_RXTIMEOUT";
	case IEEE80211_IOCTL_STA_STATS:
		return "STA_STATS";
	case IEEE80211_IOCTL_GETWPAIE:
		return "GETWPAIE";
	default:
		return "??";
	}
}


static const char * athr_get_param_name(int op)
{
	switch (op) {
	case IEEE80211_IOC_MCASTCIPHER:
		return "MCASTCIPHER";
	case IEEE80211_PARAM_MCASTKEYLEN:
		return "MCASTKEYLEN";
	case IEEE80211_PARAM_UCASTCIPHERS:
		return "UCASTCIPHERS";
	case IEEE80211_PARAM_KEYMGTALGS:
		return "KEYMGTALGS";
	case IEEE80211_PARAM_RSNCAPS:
		return "RSNCAPS";
	case IEEE80211_PARAM_WPA:
		return "WPA";
	case IEEE80211_PARAM_AUTHMODE:
		return "AUTHMODE";
	case IEEE80211_PARAM_PRIVACY:
		return "PRIVACY";
	case IEEE80211_PARAM_COUNTERMEASURES:
		return "COUNTERMEASURES";
	default:
		return "??";
	}
}


#ifdef CONFIG_FILS
static int
get80211param(struct atheros_driver_data *drv, int op, int *data)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.mode = op;

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_GETPARAM, &iwr) < 0)
		return -1;

	*data = iwr.u.mode;
	return 0;
}
#endif /* CONFIG_FILS */


static int
set80211priv(struct atheros_driver_data *drv, int op, void *data, int len)
{
	struct iwreq iwr;
	int do_inline = len < IFNAMSIZ;

	/* Certain ioctls must use the non-inlined method */
	if (op == IEEE80211_IOCTL_SET_APPIEBUF ||
	    op == IEEE80211_IOCTL_FILTERFRAME)
		do_inline = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	if (do_inline) {
		/*
		 * Argument data fits inline; put it there.
		 */
		os_memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(drv->ioctl_sock, op, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "atheros: %s: %s: ioctl op=0x%x "
			   "(%s) len=%d failed: %d (%s)",
			   __func__, drv->iface, op,
			   athr_get_ioctl_name(op),
			   len, errno, strerror(errno));
		return -1;
	}
	return 0;
}

static int
set80211param(struct atheros_driver_data *drv, int op, int arg)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.mode = op;
	os_memcpy(iwr.u.name + sizeof(__u32), &arg, sizeof(arg));

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		wpa_printf(MSG_INFO,
			   "%s: %s: Failed to set parameter (op %d (%s) arg %d): ioctl[IEEE80211_IOCTL_SETPARAM]: %s",
			   __func__, drv->iface, op, athr_get_param_name(op),
			   arg, strerror(errno));
		return -1;
	}
	return 0;
}

#ifndef CONFIG_NO_STDOUT_DEBUG
static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		os_snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		os_snprintf(buf, sizeof(buf), MACSTR, 0, 0, 0, 0, 0, 0);
	return buf;
}
#endif /* CONFIG_NO_STDOUT_DEBUG */

/*
 * Configure WPA parameters.
 */
static int
atheros_configure_wpa(struct atheros_driver_data *drv,
		      struct wpa_bss_params *params)
{
	int v;

	switch (params->wpa_group) {
	case WPA_CIPHER_CCMP:
		v = IEEE80211_CIPHER_AES_CCM;
		break;
#ifdef ATH_GCM_SUPPORT
	case WPA_CIPHER_CCMP_256:
		v = IEEE80211_CIPHER_AES_CCM_256;
		break;
	case WPA_CIPHER_GCMP:
		v = IEEE80211_CIPHER_AES_GCM;
		break;
	case WPA_CIPHER_GCMP_256:
		v = IEEE80211_CIPHER_AES_GCM_256;
		break;
#endif /* ATH_GCM_SUPPORT */
	case WPA_CIPHER_TKIP:
		v = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_CIPHER_WEP104:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_WEP40:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_NONE:
		v = IEEE80211_CIPHER_NONE;
		break;
	default:
		wpa_printf(MSG_ERROR, "Unknown group key cipher %u",
			   params->wpa_group);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "%s: group key cipher=%d", __func__, v);
	if (set80211param(drv, IEEE80211_PARAM_MCASTCIPHER, v)) {
		wpa_printf(MSG_INFO, "Unable to set group key cipher to %u", v);
		return -1;
	}
	if (v == IEEE80211_CIPHER_WEP) {
		/* key length is done only for specific ciphers */
		v = (params->wpa_group == WPA_CIPHER_WEP104 ? 13 : 5);
		if (set80211param(drv, IEEE80211_PARAM_MCASTKEYLEN, v)) {
			wpa_printf(MSG_INFO,
				   "Unable to set group key length to %u", v);
			return -1;
		}
	}

	v = 0;
	if (params->wpa_pairwise & WPA_CIPHER_CCMP)
		v |= 1<<IEEE80211_CIPHER_AES_CCM;
#ifdef ATH_GCM_SUPPORT
	if (params->wpa_pairwise & WPA_CIPHER_CCMP_256)
		v |= 1<<IEEE80211_CIPHER_AES_CCM_256;
	if (params->wpa_pairwise & WPA_CIPHER_GCMP)
		v |= 1<<IEEE80211_CIPHER_AES_GCM;
	if (params->wpa_pairwise & WPA_CIPHER_GCMP_256)
		v |= 1<<IEEE80211_CIPHER_AES_GCM_256;
#endif /* ATH_GCM_SUPPORT */
	if (params->wpa_pairwise & WPA_CIPHER_TKIP)
		v |= 1<<IEEE80211_CIPHER_TKIP;
	if (params->wpa_pairwise & WPA_CIPHER_NONE)
		v |= 1<<IEEE80211_CIPHER_NONE;
	wpa_printf(MSG_DEBUG, "%s: pairwise key ciphers=0x%x", __func__, v);
	if (set80211param(drv, IEEE80211_PARAM_UCASTCIPHERS, v)) {
		wpa_printf(MSG_INFO,
			   "Unable to set pairwise key ciphers to 0x%x", v);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: key management algorithms=0x%x",
		   __func__, params->wpa_key_mgmt);
	if (set80211param(drv, IEEE80211_PARAM_KEYMGTALGS,
			  params->wpa_key_mgmt)) {
		wpa_printf(MSG_INFO,
			   "Unable to set key management algorithms to 0x%x",
			   params->wpa_key_mgmt);
		return -1;
	}

	v = 0;
	if (params->rsn_preauth)
		v |= BIT(0);
	if (params->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		v |= BIT(7);
		if (params->ieee80211w == MGMT_FRAME_PROTECTION_REQUIRED)
			v |= BIT(6);
	}

	wpa_printf(MSG_DEBUG, "%s: rsn capabilities=0x%x", __func__, v);
	if (set80211param(drv, IEEE80211_PARAM_RSNCAPS, v)) {
		wpa_printf(MSG_INFO, "Unable to set RSN capabilities to 0x%x",
			   v);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: enable WPA=0x%x", __func__, params->wpa);
	if (set80211param(drv, IEEE80211_PARAM_WPA, params->wpa)) {
		wpa_printf(MSG_INFO, "Unable to set WPA to %u", params->wpa);
		return -1;
	}
	return 0;
}

static int
atheros_set_ieee8021x(void *priv, struct wpa_bss_params *params)
{
	struct atheros_driver_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, params->enabled);

	if (!params->enabled) {
		/* XXX restore state */
		if (set80211param(priv, IEEE80211_PARAM_AUTHMODE,
				  IEEE80211_AUTH_AUTO) < 0)
			return -1;
		/* IEEE80211_AUTH_AUTO ends up enabling Privacy; clear that */
		return atheros_set_privacy(drv, 0);
	}
	if (!params->wpa && !params->ieee802_1x) {
		wpa_printf(MSG_WARNING, "No 802.1X or WPA enabled!");
		return -1;
	}
	if (params->wpa && atheros_configure_wpa(drv, params) != 0) {
		wpa_printf(MSG_WARNING, "Error configuring WPA state!");
		return -1;
	}
	if (set80211param(priv, IEEE80211_PARAM_AUTHMODE,
		(params->wpa ? IEEE80211_AUTH_WPA : IEEE80211_AUTH_8021X))) {
		wpa_printf(MSG_WARNING, "Error enabling WPA/802.1X!");
		return -1;
	}

	return 0;
}

static int
atheros_set_privacy(void *priv, int enabled)
{
	struct atheros_driver_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	return set80211param(drv, IEEE80211_PARAM_PRIVACY, enabled);
}

static int
atheros_set_sta_authorized(void *priv, const u8 *addr, int authorized)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s authorized=%d",
		   __func__, ether_sprintf(addr), authorized);

	if (authorized)
		mlme.im_op = IEEE80211_MLME_AUTHORIZE;
	else
		mlme.im_op = IEEE80211_MLME_UNAUTHORIZE;
	mlme.im_reason = 0;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to %sauthorize STA " MACSTR,
			   __func__, authorized ? "" : "un", MAC2STR(addr));
	}

	return ret;
}

static int
atheros_sta_set_flags(void *priv, const u8 *addr,
		      unsigned int total_flags, unsigned int flags_or,
		      unsigned int flags_and)
{
	/* For now, only support setting Authorized flag */
	if (flags_or & WPA_STA_AUTHORIZED)
		return atheros_set_sta_authorized(priv, addr, 1);
	if (!(flags_and & WPA_STA_AUTHORIZED))
		return atheros_set_sta_authorized(priv, addr, 0);
	return 0;
}

static int
atheros_del_key(void *priv, const u8 *addr, int key_idx)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_del_key wk;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s key_idx=%d",
		   __func__, ether_sprintf(addr), key_idx);

	os_memset(&wk, 0, sizeof(wk));
	if (addr != NULL) {
		os_memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.idk_keyix = (u8) IEEE80211_KEYIX_NONE;
	} else {
		wk.idk_keyix = key_idx;
	}

	ret = set80211priv(drv, IEEE80211_IOCTL_DELKEY, &wk, sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to delete key (addr %s"
			   " key_idx %d)", __func__, ether_sprintf(addr),
			   key_idx);
	}

	return ret;
}

static int
atheros_set_key(void *priv, struct wpa_driver_set_key_params *params)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_key wk;
	u_int8_t cipher;
	int ret;
	enum wpa_alg alg = params->alg;
	const u8 *addr = params->addr;
	int key_idx = params->key_idx;
	int set_tx = params->set_tx;
	const u8 *key = params->key;
	size_t key_len = params->key_len;

	if (alg == WPA_ALG_NONE)
		return atheros_del_key(drv, addr, key_idx);

	wpa_printf(MSG_DEBUG, "%s: alg=%d addr=%s key_idx=%d",
		   __func__, alg, ether_sprintf(addr), key_idx);

	switch (alg) {
	case WPA_ALG_WEP:
		cipher = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		cipher = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		cipher = IEEE80211_CIPHER_AES_CCM;
		break;
#ifdef ATH_GCM_SUPPORT
	case WPA_ALG_CCMP_256:
		cipher = IEEE80211_CIPHER_AES_CCM_256;
		break;
	case WPA_ALG_GCMP:
		cipher = IEEE80211_CIPHER_AES_GCM;
		break;
	case WPA_ALG_GCMP_256:
		cipher = IEEE80211_CIPHER_AES_GCM_256;
		break;
#endif /* ATH_GCM_SUPPORT */
	case WPA_ALG_BIP_CMAC_128:
		cipher = IEEE80211_CIPHER_AES_CMAC;
		break;
#ifdef ATH_GCM_SUPPORT
	case WPA_ALG_BIP_CMAC_256:
		cipher = IEEE80211_CIPHER_AES_CMAC_256;
		break;
	case WPA_ALG_BIP_GMAC_128:
		cipher = IEEE80211_CIPHER_AES_GMAC;
		break;
	case WPA_ALG_BIP_GMAC_256:
		cipher = IEEE80211_CIPHER_AES_GMAC_256;
		break;
#endif /* ATH_GCM_SUPPORT */
	default:
		wpa_printf(MSG_INFO, "%s: unknown/unsupported algorithm %d",
			   __func__, alg);
		return -1;
	}

	if (key_len > sizeof(wk.ik_keydata)) {
		wpa_printf(MSG_INFO, "%s: key length %lu too big", __func__,
			   (unsigned long) key_len);
		return -3;
	}

	os_memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT;
	if (addr == NULL || is_broadcast_ether_addr(addr)) {
		os_memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wk.ik_keyix = key_idx;
		if (set_tx)
			wk.ik_flags |= IEEE80211_KEY_DEFAULT;
	} else {
		os_memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.ik_keyix = IEEE80211_KEYIX_NONE;
	}
	wk.ik_keylen = key_len;
	os_memcpy(wk.ik_keydata, key, key_len);

	ret = set80211priv(drv, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to set key (addr %s"
			   " key_idx %d alg %d key_len %lu set_tx %d)",
			   __func__, ether_sprintf(wk.ik_macaddr), key_idx,
			   alg, (unsigned long) key_len, set_tx);
	}

	return ret;
}


static int
atheros_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx,
		   u8 *seq)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_key wk;

	wpa_printf(MSG_DEBUG, "%s: addr=%s idx=%d",
		   __func__, ether_sprintf(addr), idx);

	os_memset(&wk, 0, sizeof(wk));
	if (addr == NULL)
		os_memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
	else
		os_memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	wk.ik_keyix = idx;

	if (set80211priv(drv, IEEE80211_IOCTL_GETKEY, &wk, sizeof(wk))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to get encryption data "
			   "(addr " MACSTR " key_idx %d)",
			   __func__, MAC2STR(wk.ik_macaddr), idx);
		return -1;
	}

#ifdef WORDS_BIGENDIAN
	{
		/*
		 * wk.ik_keytsc is in host byte order (big endian), need to
		 * swap it to match with the byte order used in WPA.
		 */
		int i;
#ifndef WPA_KEY_RSC_LEN
#define WPA_KEY_RSC_LEN 8
#endif
		u8 tmp[WPA_KEY_RSC_LEN];
		os_memcpy(tmp, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
		for (i = 0; i < WPA_KEY_RSC_LEN; i++) {
			seq[i] = tmp[WPA_KEY_RSC_LEN - i - 1];
		}
	}
#else /* WORDS_BIGENDIAN */
	os_memcpy(seq, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
#endif /* WORDS_BIGENDIAN */
	return 0;
}


static int
atheros_flush(void *priv)
{
	u8 allsta[IEEE80211_ADDR_LEN];
	os_memset(allsta, 0xff, IEEE80211_ADDR_LEN);
	return atheros_sta_deauth(priv, NULL, allsta,
				  IEEE80211_REASON_AUTH_LEAVE);
}


static int
atheros_read_sta_driver_data(void *priv, struct hostap_sta_driver_data *data,
			     const u8 *addr)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_sta_stats stats;

	os_memset(data, 0, sizeof(*data));

	/*
	 * Fetch statistics for station from the system.
	 */
	os_memset(&stats, 0, sizeof(stats));
	os_memcpy(stats.is_u.macaddr, addr, IEEE80211_ADDR_LEN);
	if (set80211priv(drv, IEEE80211_IOCTL_STA_STATS,
			 &stats, sizeof(stats))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to fetch STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
		if (os_memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
			os_memcpy(data, &drv->acct_data, sizeof(*data));
			return 0;
		}

		wpa_printf(MSG_INFO,
			   "Failed to get station stats information element");
		return -1;
	}

	data->rx_packets = stats.is_stats.ns_rx_data;
	data->rx_bytes = stats.is_stats.ns_rx_bytes;
	data->tx_packets = stats.is_stats.ns_tx_data;
	data->tx_bytes = stats.is_stats.ns_tx_bytes;
	return 0;
}


static int
atheros_sta_clear_stats(void *priv, const u8 *addr)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s", __func__, ether_sprintf(addr));

	mlme.im_op = IEEE80211_MLME_CLEAR_STATS;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme,
			   sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to clear STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
	}

	return ret;
}


static int
atheros_set_opt_ie(void *priv, const u8 *ie, size_t ie_len)
{
	struct atheros_driver_data *drv = priv;
	u8 buf[512];
	struct ieee80211req_getset_appiebuf *app_ie;

	wpa_printf(MSG_DEBUG, "%s buflen = %lu", __func__,
		   (unsigned long) ie_len);
	wpa_hexdump(MSG_DEBUG, "atheros: set_generic_elem", ie, ie_len);

	wpabuf_free(drv->wpa_ie);
	if (ie)
		drv->wpa_ie = wpabuf_alloc_copy(ie, ie_len);
	else
		drv->wpa_ie = NULL;

	app_ie = (struct ieee80211req_getset_appiebuf *) buf;
	if (ie)
		os_memcpy(&(app_ie->app_buf[0]), ie, ie_len);
	app_ie->app_buflen = ie_len;

	app_ie->app_frmtype = IEEE80211_APPIE_FRAME_BEACON;

	/* append WPS IE for Beacon */
	if (drv->wps_beacon_ie != NULL) {
		os_memcpy(&(app_ie->app_buf[ie_len]),
			  wpabuf_head(drv->wps_beacon_ie),
			  wpabuf_len(drv->wps_beacon_ie));
		app_ie->app_buflen = ie_len + wpabuf_len(drv->wps_beacon_ie);
	}
	wpa_hexdump(MSG_DEBUG, "atheros: SET_APPIEBUF(Beacon)",
		    app_ie->app_buf, app_ie->app_buflen);
	set80211priv(drv, IEEE80211_IOCTL_SET_APPIEBUF, app_ie,
		     sizeof(struct ieee80211req_getset_appiebuf) +
		     app_ie->app_buflen);

	/* append WPS IE for Probe Response */
	app_ie->app_frmtype = IEEE80211_APPIE_FRAME_PROBE_RESP;
	if (drv->wps_probe_resp_ie != NULL) {
		os_memcpy(&(app_ie->app_buf[ie_len]),
			  wpabuf_head(drv->wps_probe_resp_ie),
			  wpabuf_len(drv->wps_probe_resp_ie));
		app_ie->app_buflen = ie_len +
			wpabuf_len(drv->wps_probe_resp_ie);
	} else
		app_ie->app_buflen = ie_len;
	wpa_hexdump(MSG_DEBUG, "atheros: SET_APPIEBUF(ProbeResp)",
		    app_ie->app_buf, app_ie->app_buflen);
	set80211priv(drv, IEEE80211_IOCTL_SET_APPIEBUF, app_ie,
		     sizeof(struct ieee80211req_getset_appiebuf) +
		     app_ie->app_buflen);
	return 0;
}

static int
atheros_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
		   u16 reason_code)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = reason_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to deauth STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

static int
atheros_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
		     u16 reason_code)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = reason_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to disassoc STA (addr "
			   MACSTR " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

static int atheros_set_qos_map(void *ctx, const u8 *qos_map_set,
			       u8 qos_map_set_len)
{
#ifdef CONFIG_ATHEROS_QOS_MAP
	struct atheros_driver_data *drv = ctx;
	struct ieee80211req_athdbg req;
	struct ieee80211_qos_map *qos_map = &req.data.qos_map;
	struct iwreq iwr;
	int i, up_start;

	if (qos_map_set_len < 16 || qos_map_set_len > 58 ||
	    qos_map_set_len & 1) {
		wpa_printf(MSG_ERROR, "Invalid QoS Map");
		return -1;
	} else {
		os_memset(&req, 0, sizeof(struct ieee80211req_athdbg));
		req.cmd = IEEE80211_DBGREQ_SETQOSMAPCONF;
		os_memset(&iwr, 0, sizeof(iwr));
		os_strlcpy(iwr.ifr_name, drv->iface, sizeof(iwr.ifr_name));
		iwr.u.data.pointer = (void *) &req;
		iwr.u.data.length = sizeof(struct ieee80211req_athdbg);
	}

	qos_map->valid = 1;
	qos_map->num_dscp_except = (qos_map_set_len - 16) / 2;
	if (qos_map->num_dscp_except) {
		for (i = 0; i < qos_map->num_dscp_except; i++) {
			qos_map->dscp_exception[i].dscp	= qos_map_set[i * 2];
			qos_map->dscp_exception[i].up =	qos_map_set[i * 2 + 1];
		}
	}

	up_start = qos_map_set_len - 16;
	for (i = 0; i < IEEE80211_MAX_QOS_UP_RANGE; i++) {
		qos_map->up[i].low = qos_map_set[up_start + (i * 2)];
		qos_map->up[i].high = qos_map_set[up_start + (i * 2) + 1];
	}

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_DBGREQ, &iwr) < 0) {
		wpa_printf(MSG_ERROR,
			   "%s: %s: Failed to set QoS Map: ioctl[IEEE80211_IOCTL_DBGREQ]: %s",
			   __func__, drv->iface, strerror(errno));
		return -1;
	}
#endif /* CONFIG_ATHEROS_QOS_MAP */

	return 0;
}


static void atheros_raw_receive(void *ctx, const u8 *src_addr, const u8 *buf,
				size_t len)
{
	struct atheros_driver_data *drv = ctx;
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;
	u16 fc, stype;
	int ielen;
	const u8 *iebuf;

	if (len < IEEE80211_HDRLEN)
		return;

	mgmt = (const struct ieee80211_mgmt *) buf;

	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT)
		return;

	stype = WLAN_FC_GET_STYPE(fc);

	wpa_printf(MSG_DEBUG, "%s: subtype 0x%x len %d", __func__, stype,
		   (int) len);

	if (stype == WLAN_FC_STYPE_PROBE_REQ) {
		if (len < IEEE80211_HDRLEN)
			return;

		os_memset(&event, 0, sizeof(event));
		event.rx_probe_req.sa = mgmt->sa;
		event.rx_probe_req.da = mgmt->da;
		event.rx_probe_req.bssid = mgmt->bssid;
		event.rx_probe_req.ie = buf + IEEE80211_HDRLEN;
		event.rx_probe_req.ie_len = len - IEEE80211_HDRLEN;
		wpa_supplicant_event(drv->hapd, EVENT_RX_PROBE_REQ, &event);
		return;
	}

	if (stype == WLAN_FC_STYPE_ACTION &&
	    (os_memcmp(drv->own_addr, mgmt->bssid, ETH_ALEN) == 0 ||
	     is_broadcast_ether_addr(mgmt->bssid))) {
		os_memset(&event, 0, sizeof(event));
		event.rx_mgmt.frame = buf;
		event.rx_mgmt.frame_len = len;
		wpa_supplicant_event(drv->hapd, EVENT_RX_MGMT, &event);
		return;
	}

	if (os_memcmp(drv->own_addr, mgmt->bssid, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "%s: BSSID does not match - ignore",
			   __func__);
		return;
	}

	switch (stype) {
	case WLAN_FC_STYPE_ASSOC_REQ:
		if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req))
			break;
		ielen = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req));
		iebuf = mgmt->u.assoc_req.variable;
		drv_event_assoc(drv->hapd, mgmt->sa, iebuf, ielen, 0);
		break;
	case WLAN_FC_STYPE_REASSOC_REQ:
		if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req))
			break;
		ielen = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req));
		iebuf = mgmt->u.reassoc_req.variable;
		drv_event_assoc(drv->hapd, mgmt->sa, iebuf, ielen, 1);
		break;
	case WLAN_FC_STYPE_AUTH:
		if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth))
			break;
		os_memset(&event, 0, sizeof(event));
		if (le_to_host16(mgmt->u.auth.auth_alg) == WLAN_AUTH_SAE) {
			event.rx_mgmt.frame = buf;
			event.rx_mgmt.frame_len = len;
			wpa_supplicant_event(drv->hapd, EVENT_RX_MGMT, &event);
			break;
		}
		os_memcpy(event.auth.peer, mgmt->sa, ETH_ALEN);
		os_memcpy(event.auth.bssid, mgmt->bssid, ETH_ALEN);
		event.auth.auth_type = le_to_host16(mgmt->u.auth.auth_alg);
		event.auth.status_code =
			le_to_host16(mgmt->u.auth.status_code);
		event.auth.auth_transaction =
			le_to_host16(mgmt->u.auth.auth_transaction);
		event.auth.ies = mgmt->u.auth.variable;
		event.auth.ies_len = len - IEEE80211_HDRLEN -
			sizeof(mgmt->u.auth);
		wpa_supplicant_event(drv->hapd, EVENT_AUTH, &event);
		break;
	default:
		break;
	}
}


static int atheros_receive_pkt(struct atheros_driver_data *drv)
{
	int ret = 0;
	struct ieee80211req_set_filter filt;

	wpa_printf(MSG_DEBUG, "%s Enter", __func__);
	filt.app_filterype = 0;
#ifdef CONFIG_WPS
	filt.app_filterype |= IEEE80211_FILTER_TYPE_PROBE_REQ;
#endif /* CONFIG_WPS */
	filt.app_filterype |= (IEEE80211_FILTER_TYPE_ASSOC_REQ |
			       IEEE80211_FILTER_TYPE_AUTH |
			       IEEE80211_FILTER_TYPE_ACTION);
#ifdef CONFIG_WNM
	filt.app_filterype |= IEEE80211_FILTER_TYPE_ACTION;
#endif /* CONFIG_WNM */
#ifdef CONFIG_HS20
	filt.app_filterype |= IEEE80211_FILTER_TYPE_ACTION;
#endif /* CONFIG_HS20 */
	if (filt.app_filterype) {
		ret = set80211priv(drv, IEEE80211_IOCTL_FILTERFRAME, &filt,
				   sizeof(struct ieee80211req_set_filter));
		if (ret)
			return ret;
	}

#if defined(CONFIG_WPS) || defined(CONFIG_IEEE80211R) || defined(CONFIG_FILS)
	drv->sock_raw = l2_packet_init(drv->iface, NULL, ETH_P_80211_RAW,
				       atheros_raw_receive, drv, 1);
	if (drv->sock_raw == NULL)
		return -1;
#endif /* CONFIG_WPS || CONFIG_IEEE80211R || CONFIG_FILS */
	return ret;
}

static int atheros_reset_appfilter(struct atheros_driver_data *drv)
{
	struct ieee80211req_set_filter filt;
	filt.app_filterype = 0;
	return set80211priv(drv, IEEE80211_IOCTL_FILTERFRAME, &filt,
			    sizeof(struct ieee80211req_set_filter));
}

#ifdef CONFIG_WPS
static int
atheros_set_wps_ie(void *priv, const u8 *ie, size_t len, u32 frametype)
{
	struct atheros_driver_data *drv = priv;
	u8 buf[512];
	struct ieee80211req_getset_appiebuf *beac_ie;

	wpa_printf(MSG_DEBUG, "%s buflen = %lu frametype=%u", __func__,
		   (unsigned long) len, frametype);
	wpa_hexdump(MSG_DEBUG, "atheros: IE", ie, len);

	beac_ie = (struct ieee80211req_getset_appiebuf *) buf;
	beac_ie->app_frmtype = frametype;
	beac_ie->app_buflen = len;
	if (ie)
		os_memcpy(&(beac_ie->app_buf[0]), ie, len);

	/* append the WPA/RSN IE if it is set already */
	if (((frametype == IEEE80211_APPIE_FRAME_BEACON) ||
	     (frametype == IEEE80211_APPIE_FRAME_PROBE_RESP)) &&
	    (drv->wpa_ie != NULL)) {
		wpa_hexdump_buf(MSG_DEBUG, "atheros: Append WPA/RSN IE",
				drv->wpa_ie);
		os_memcpy(&(beac_ie->app_buf[len]), wpabuf_head(drv->wpa_ie),
			  wpabuf_len(drv->wpa_ie));
		beac_ie->app_buflen += wpabuf_len(drv->wpa_ie);
	}

	wpa_hexdump(MSG_DEBUG, "atheros: SET_APPIEBUF",
		    beac_ie->app_buf, beac_ie->app_buflen);
	return set80211priv(drv, IEEE80211_IOCTL_SET_APPIEBUF, beac_ie,
			    sizeof(struct ieee80211req_getset_appiebuf) +
			    beac_ie->app_buflen);
}

static int
atheros_set_ap_wps_ie(void *priv, const struct wpabuf *beacon,
		      const struct wpabuf *proberesp,
		      const struct wpabuf *assocresp)
{
	struct atheros_driver_data *drv = priv;

	wpa_hexdump_buf(MSG_DEBUG, "atheros: set_ap_wps_ie - beacon", beacon);
	wpa_hexdump_buf(MSG_DEBUG, "atheros: set_ap_wps_ie - proberesp",
			proberesp);
	wpa_hexdump_buf(MSG_DEBUG, "atheros: set_ap_wps_ie - assocresp",
			assocresp);
	wpabuf_free(drv->wps_beacon_ie);
	drv->wps_beacon_ie = beacon ? wpabuf_dup(beacon) : NULL;
	wpabuf_free(drv->wps_probe_resp_ie);
	drv->wps_probe_resp_ie = proberesp ? wpabuf_dup(proberesp) : NULL;

	atheros_set_wps_ie(priv, assocresp ? wpabuf_head(assocresp) : NULL,
			   assocresp ? wpabuf_len(assocresp) : 0,
			   IEEE80211_APPIE_FRAME_ASSOC_RESP);
	if (atheros_set_wps_ie(priv, beacon ? wpabuf_head(beacon) : NULL,
			       beacon ? wpabuf_len(beacon) : 0,
			       IEEE80211_APPIE_FRAME_BEACON))
		return -1;
	return atheros_set_wps_ie(priv,
				  proberesp ? wpabuf_head(proberesp) : NULL,
				  proberesp ? wpabuf_len(proberesp): 0,
				  IEEE80211_APPIE_FRAME_PROBE_RESP);
}
#else /* CONFIG_WPS */
#define atheros_set_ap_wps_ie NULL
#endif /* CONFIG_WPS */

static int
atheros_sta_auth(void *priv, struct wpa_driver_sta_auth_params *params)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s status_code=%d",
		   __func__, ether_sprintf(params->addr), params->status);

#ifdef CONFIG_FILS
	/* Copy FILS AAD parameters if the driver supports FILS */
	if (params->fils_auth && drv->fils_en) {
		wpa_printf(MSG_DEBUG, "%s: im_op IEEE80211_MLME_AUTH_FILS",
			   __func__);
		os_memcpy(mlme.fils_aad.ANonce, params->fils_anonce,
			  IEEE80211_FILS_NONCE_LEN);
		os_memcpy(mlme.fils_aad.SNonce, params->fils_snonce,
			  IEEE80211_FILS_NONCE_LEN);
		os_memcpy(mlme.fils_aad.kek, params->fils_kek,
			  IEEE80211_MAX_WPA_KEK_LEN);
		mlme.fils_aad.kek_len = params->fils_kek_len;
		mlme.im_op = IEEE80211_MLME_AUTH_FILS;
		wpa_hexdump(MSG_DEBUG, "FILS: ANonce",
			    mlme.fils_aad.ANonce, FILS_NONCE_LEN);
		wpa_hexdump(MSG_DEBUG, "FILS: SNonce",
			    mlme.fils_aad.SNonce, FILS_NONCE_LEN);
		wpa_hexdump_key(MSG_DEBUG, "FILS: KEK",
				mlme.fils_aad.kek, mlme.fils_aad.kek_len);
	} else {
		mlme.im_op = IEEE80211_MLME_AUTH;
	}
#else /* CONFIG_FILS */
	mlme.im_op = IEEE80211_MLME_AUTH;
#endif /* CONFIG_FILS */

	mlme.im_reason = params->status;
	mlme.im_seq = params->seq;
	os_memcpy(mlme.im_macaddr, params->addr, IEEE80211_ADDR_LEN);
	mlme.im_optie_len = params->len;
	if (params->len) {
		if (params->len < IEEE80211_MAX_OPT_IE) {
			os_memcpy(mlme.im_optie, params->ie, params->len);
		} else {
			wpa_printf(MSG_DEBUG, "%s: Not enough space to copy "
				   "opt_ie STA (addr " MACSTR " reason %d, "
				   "ie_len %d)",
				   __func__, MAC2STR(params->addr),
				   params->status, (int) params->len);
			return -1;
		}
	}
	ret = set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to auth STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(params->addr), params->status);
	}
	return ret;
}

static int
atheros_sta_assoc(void *priv, const u8 *own_addr, const u8 *addr,
		  int reassoc, u16 status_code, const u8 *ie, size_t len)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s status_code=%d reassoc %d",
		   __func__, ether_sprintf(addr), status_code, reassoc);

	if (reassoc)
		mlme.im_op = IEEE80211_MLME_REASSOC;
	else
		mlme.im_op = IEEE80211_MLME_ASSOC;
	mlme.im_reason = status_code;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	mlme.im_optie_len = len;
	if (len) {
		if (len < IEEE80211_MAX_OPT_IE) {
			os_memcpy(mlme.im_optie, ie, len);
		} else {
			wpa_printf(MSG_DEBUG, "%s: Not enough space to copy "
				   "opt_ie STA (addr " MACSTR " reason %d, "
				   "ie_len %d)",
				   __func__, MAC2STR(addr), status_code,
				   (int) len);
			return -1;
		}
	}
	ret = set80211priv(drv, IEEE80211_IOCTL_SETMLME, &mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to assoc STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(addr), status_code);
	}
	return ret;
}


static void
atheros_new_sta(struct atheros_driver_data *drv, u8 addr[IEEE80211_ADDR_LEN])
{
	struct hostapd_data *hapd = drv->hapd;
	struct ieee80211req_wpaie ie;
	int ielen = 0;
	u8 *iebuf = NULL;

	/*
	 * Fetch negotiated WPA/RSN parameters from the system.
	 */
	os_memset(&ie, 0, sizeof(ie));
	os_memcpy(ie.wpa_macaddr, addr, IEEE80211_ADDR_LEN);
	if (set80211priv(drv, IEEE80211_IOCTL_GETWPAIE, &ie, sizeof(ie))) {
		/*
		 * See ATH_WPS_IE comment in the beginning of the file for a
		 * possible cause for the failure..
		 */
		wpa_printf(MSG_DEBUG, "%s: Failed to get WPA/RSN IE: %s",
			   __func__, strerror(errno));
		goto no_ie;
	}
	wpa_hexdump(MSG_MSGDUMP, "atheros req WPA IE",
		    ie.wpa_ie, IEEE80211_MAX_OPT_IE);
	wpa_hexdump(MSG_MSGDUMP, "atheros req RSN IE",
		    ie.rsn_ie, IEEE80211_MAX_OPT_IE);
#ifdef ATH_WPS_IE
	wpa_hexdump(MSG_MSGDUMP, "atheros req WPS IE",
		    ie.wps_ie, IEEE80211_MAX_OPT_IE);
#endif /* ATH_WPS_IE */
	iebuf = ie.wpa_ie;
	/* atheros seems to return some random data if WPA/RSN IE is not set.
	 * Assume the IE was not included if the IE type is unknown. */
	if (iebuf[0] != WLAN_EID_VENDOR_SPECIFIC)
		iebuf[1] = 0;
	if (iebuf[1] == 0 && ie.rsn_ie[1] > 0) {
		/* atheros-ng svn #1453 added rsn_ie. Use it, if wpa_ie was not
		 * set. This is needed for WPA2. */
		iebuf = ie.rsn_ie;
		if (iebuf[0] != WLAN_EID_RSN)
			iebuf[1] = 0;
	}

	ielen = iebuf[1];

#ifdef ATH_WPS_IE
	/* if WPS IE is present, preference is given to WPS */
	if (ie.wps_ie[0] == WLAN_EID_VENDOR_SPECIFIC && ie.wps_ie[1] > 0) {
		iebuf = ie.wps_ie;
		ielen = ie.wps_ie[1];
	}
#endif /* ATH_WPS_IE */

	if (ielen == 0)
		iebuf = NULL;
	else
		ielen += 2;

no_ie:
	drv_event_assoc(hapd, addr, iebuf, ielen, 0);

	if (os_memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
		/* Cached accounting data is not valid anymore. */
		os_memset(drv->acct_mac, 0, ETH_ALEN);
		os_memset(&drv->acct_data, 0, sizeof(drv->acct_data));
	}
}

static void
atheros_wireless_event_wireless_custom(struct atheros_driver_data *drv,
				       char *custom, char *end)
{
#define MGMT_FRAM_TAG_SIZE 30 /* hardcoded in driver */
	wpa_printf(MSG_DEBUG, "Custom wireless event: '%s'", custom);

	if (os_strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		char *pos;
		u8 addr[ETH_ALEN];
		pos = os_strstr(custom, "addr=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "without sender address ignored");
			return;
		}
		pos += 5;
		if (hwaddr_aton(pos, addr) == 0) {
			union wpa_event_data data;
			os_memset(&data, 0, sizeof(data));
			data.michael_mic_failure.unicast = 1;
			data.michael_mic_failure.src = addr;
			wpa_supplicant_event(drv->hapd,
					     EVENT_MICHAEL_MIC_FAILURE, &data);
		} else {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "with invalid MAC address");
		}
	} else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
		char *key, *value;
		u32 val;
		key = custom;
		while ((key = os_strchr(key, '\n')) != NULL) {
			key++;
			value = os_strchr(key, '=');
			if (value == NULL)
				continue;
			*value++ = '\0';
			val = strtoul(value, NULL, 10);
			if (os_strcmp(key, "mac") == 0)
				hwaddr_aton(value, drv->acct_mac);
			else if (os_strcmp(key, "rx_packets") == 0)
				drv->acct_data.rx_packets = val;
			else if (os_strcmp(key, "tx_packets") == 0)
				drv->acct_data.tx_packets = val;
			else if (os_strcmp(key, "rx_bytes") == 0)
				drv->acct_data.rx_bytes = val;
			else if (os_strcmp(key, "tx_bytes") == 0)
				drv->acct_data.tx_bytes = val;
			key = value;
		}
#ifdef CONFIG_WPS
	} else if (os_strncmp(custom, "PUSH-BUTTON.indication", 22) == 0) {
		/* Some atheros kernels send push button as a wireless event */
		/* PROBLEM! this event is received for ALL BSSs ...
		 * so all are enabled for WPS... ugh.
		 */
		wpa_supplicant_event(drv->hapd, EVENT_WPS_BUTTON_PUSHED, NULL);
	} else if (os_strncmp(custom, "Manage.prob_req ", 16) == 0) {
		/*
		 * Atheros driver uses a hack to pass Probe Request frames as a
		 * binary data in the custom wireless event. The old way (using
		 * packet sniffing) didn't work when bridging.
		 * Format: "Manage.prob_req <frame len>" | zero padding | frame
		 */
		int len = atoi(custom + 16);
		if (len < 0 || MGMT_FRAM_TAG_SIZE + len > end - custom) {
			wpa_printf(MSG_DEBUG, "Invalid Manage.prob_req event "
				   "length %d", len);
			return;
		}
		atheros_raw_receive(drv, NULL,
				    (u8 *) custom + MGMT_FRAM_TAG_SIZE, len);
#endif /* CONFIG_WPS */
	} else if (os_strncmp(custom, "Manage.assoc_req ", 17) == 0) {
		/* Format: "Manage.assoc_req <frame len>" | zero padding |
		 * frame */
		int len = atoi(custom + 17);
		if (len < 0 || MGMT_FRAM_TAG_SIZE + len > end - custom) {
			wpa_printf(MSG_DEBUG,
				   "Invalid Manage.assoc_req event length %d",
				   len);
			return;
		}
		atheros_raw_receive(drv, NULL,
				    (u8 *) custom + MGMT_FRAM_TAG_SIZE, len);
	} else if (os_strncmp(custom, "Manage.auth ", 12) == 0) {
		/* Format: "Manage.auth <frame len>" | zero padding | frame */
		int len = atoi(custom + 12);
		if (len < 0 ||
		    MGMT_FRAM_TAG_SIZE + len > end - custom) {
			wpa_printf(MSG_DEBUG,
				   "Invalid Manage.auth event length %d", len);
			return;
		}
		atheros_raw_receive(drv, NULL,
				    (u8 *) custom + MGMT_FRAM_TAG_SIZE, len);
	} else if (os_strncmp(custom, "Manage.action ", 14) == 0) {
		/* Format: "Manage.assoc_req <frame len>" | zero padding | frame
		 */
		int len = atoi(custom + 14);
		if (len < 0 || MGMT_FRAM_TAG_SIZE + len > end - custom) {
			wpa_printf(MSG_DEBUG,
				   "Invalid Manage.action event length %d",
				   len);
			return;
		}
		atheros_raw_receive(drv, NULL,
				    (u8 *) custom + MGMT_FRAM_TAG_SIZE, len);
	}
}


static void send_action_cb_event(struct atheros_driver_data *drv,
				 char *data, size_t data_len)
{
	union wpa_event_data event;
	struct ieee80211_send_action_cb *sa;
	const struct ieee80211_hdr *hdr;
	u16 fc;

	if (data_len < sizeof(*sa) + 24) {
		wpa_printf(MSG_DEBUG,
			   "athr: Too short event message (data_len=%d sizeof(*sa)=%d)",
			   (int) data_len, (int) sizeof(*sa));
		wpa_hexdump(MSG_DEBUG, "athr: Short event message",
			    data, data_len);
		return;
	}

	sa = (struct ieee80211_send_action_cb *) data;

	hdr = (const struct ieee80211_hdr *) (sa + 1);
	fc = le_to_host16(hdr->frame_control);

	os_memset(&event, 0, sizeof(event));
	event.tx_status.type = WLAN_FC_GET_TYPE(fc);
	event.tx_status.stype = WLAN_FC_GET_STYPE(fc);
	event.tx_status.dst = sa->dst_addr;
	event.tx_status.data = (const u8 *) hdr;
	event.tx_status.data_len = data_len - sizeof(*sa);
	event.tx_status.ack = sa->ack;
	wpa_supplicant_event(drv->hapd, EVENT_TX_STATUS, &event);
}


/*
* Handle size of data problem. WEXT only allows data of 256 bytes for custom
* events, and p2p data can be much bigger. So the athr driver sends a small
* event telling me to collect the big data with an ioctl.
* On the first event, send all pending events to supplicant.
*/
static void fetch_pending_big_events(struct atheros_driver_data *drv)
{
	union wpa_event_data event;
	const struct ieee80211_mgmt *mgmt;
	u8 tbuf[IW_PRIV_SIZE_MASK]; /* max size is 2047 bytes */
	u16 fc, stype;
	struct iwreq iwr;
	size_t data_len;
	u32 freq, frame_type;

	while (1) {
		os_memset(&iwr, 0, sizeof(iwr));
		os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

		iwr.u.data.pointer = (void *) tbuf;
		iwr.u.data.length = sizeof(tbuf);
		iwr.u.data.flags = IEEE80211_IOC_P2P_FETCH_FRAME;

		if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_P2P_BIG_PARAM, &iwr)
		    < 0) {
			if (errno == ENOSPC) {
				wpa_printf(MSG_DEBUG, "%s:%d exit",
					   __func__, __LINE__);
				return;
			}
			wpa_printf(MSG_DEBUG, "athr: %s: P2P_BIG_PARAM["
				   "P2P_FETCH_FRAME] failed: %s",
				   __func__, strerror(errno));
			return;
		}
		data_len = iwr.u.data.length;
		wpa_hexdump(MSG_DEBUG, "athr: P2P_FETCH_FRAME data",
			    (u8 *) tbuf, data_len);
		if (data_len < sizeof(freq) + sizeof(frame_type) + 24) {
			wpa_printf(MSG_DEBUG, "athr: frame too short");
			continue;
		}
		os_memcpy(&freq, tbuf, sizeof(freq));
		os_memcpy(&frame_type, &tbuf[sizeof(freq)],
			  sizeof(frame_type));
		mgmt = (void *) &tbuf[sizeof(freq) + sizeof(frame_type)];
		data_len -= sizeof(freq) + sizeof(frame_type);

		if (frame_type == IEEE80211_EV_RX_MGMT) {
			fc = le_to_host16(mgmt->frame_control);
			stype = WLAN_FC_GET_STYPE(fc);

			wpa_printf(MSG_DEBUG, "athr: EV_RX_MGMT stype=%u "
				"freq=%u len=%u", stype, freq, (int) data_len);

			if (stype == WLAN_FC_STYPE_ACTION) {
				os_memset(&event, 0, sizeof(event));
				event.rx_mgmt.frame = (const u8 *) mgmt;
				event.rx_mgmt.frame_len = data_len;
				wpa_supplicant_event(drv->hapd, EVENT_RX_MGMT,
						     &event);
				continue;
			}
		} else if (frame_type == IEEE80211_EV_P2P_SEND_ACTION_CB) {
			wpa_printf(MSG_DEBUG,
				   "%s: ACTION_CB frame_type=%u len=%zu",
				   __func__, frame_type, data_len);
			send_action_cb_event(drv, (void *) mgmt, data_len);
		} else {
			wpa_printf(MSG_DEBUG, "athr: %s unknown type %d",
				   __func__, frame_type);
			continue;
		}
	}
}

static void
atheros_wireless_event_atheros_custom(struct atheros_driver_data *drv,
				      int opcode, char *buf, int len)
{
	switch (opcode) {
	case IEEE80211_EV_P2P_SEND_ACTION_CB:
		wpa_printf(MSG_DEBUG, "WEXT: EV_P2P_SEND_ACTION_CB");
		fetch_pending_big_events(drv);
		break;
	case IEEE80211_EV_RX_MGMT:
		wpa_printf(MSG_DEBUG, "WEXT: EV_RX_MGMT");
		fetch_pending_big_events(drv);
		break;
	default:
		break;
	}
}

static void
atheros_wireless_event_wireless(struct atheros_driver_data *drv,
				char *data, unsigned int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while ((size_t) (end - pos) >= IW_EV_LCP_LEN) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		os_memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		wpa_printf(MSG_MSGDUMP, "Wireless event: cmd=0x%x len=%d",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN || iwe->len > end - pos)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (drv->we_version > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVASSOCREQIE ||
		     iwe->cmd == IWEVCUSTOM)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			os_memcpy(dpos, pos + IW_EV_LCP_LEN,
				  sizeof(struct iw_event) - dlen);
		} else {
			os_memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVEXPIRED:
			drv_event_disassoc(drv->hapd,
					   (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVREGISTERED:
			atheros_new_sta(drv, (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVASSOCREQIE:
			/* Driver hack.. Use IWEVASSOCREQIE to bypass
			 * IWEVCUSTOM size limitations. Need to handle this
			 * just like IWEVCUSTOM.
			 */
		case IWEVCUSTOM:
			if (iwe->u.data.length > end - custom)
				return;
			buf = os_malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;		/* XXX */
			os_memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';

			if (iwe->u.data.flags != 0) {
				atheros_wireless_event_atheros_custom(
					drv, (int) iwe->u.data.flags,
					buf, len);
			} else {
				atheros_wireless_event_wireless_custom(
					drv, buf, buf + iwe->u.data.length);
			}
			os_free(buf);
			break;
		}

		pos += iwe->len;
	}
}


static void
atheros_wireless_event_rtm_newlink(void *ctx,
				   struct ifinfomsg *ifi, u8 *buf, size_t len)
{
	struct atheros_driver_data *drv = ctx;
	int attrlen, rta_len;
	struct rtattr *attr;

	if (ifi->ifi_index != drv->ifindex)
		return;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			atheros_wireless_event_wireless(
				drv, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static int
atheros_get_we_version(struct atheros_driver_data *drv)
{
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	drv->we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = os_zalloc(buflen);
	if (range == NULL)
		return -1;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCGIWRANGE]: %s",
			   strerror(errno));
		os_free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->we_version = range->we_version_compiled;
	}

	os_free(range);
	return 0;
}


static int
atheros_wireless_event_init(struct atheros_driver_data *drv)
{
	struct netlink_config *cfg;

	atheros_get_we_version(drv);

	cfg = os_zalloc(sizeof(*cfg));
	if (cfg == NULL)
		return -1;
	cfg->ctx = drv;
	cfg->newlink_cb = atheros_wireless_event_rtm_newlink;
	drv->netlink = netlink_init(cfg);
	if (drv->netlink == NULL) {
		os_free(cfg);
		return -1;
	}

	return 0;
}


static int
atheros_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
		   int encrypt, const u8 *own_addr, u32 flags)
{
	struct atheros_driver_data *drv = priv;
	unsigned char buf[3000];
	unsigned char *bp = buf;
	struct l2_ethhdr *eth;
	size_t len;
	int status;

	/*
	 * Prepend the Ethernet header.  If the caller left us
	 * space at the front we could just insert it but since
	 * we don't know we copy to a local buffer.  Given the frequency
	 * and size of frames this probably doesn't matter.
	 */
	len = data_len + sizeof(struct l2_ethhdr);
	if (len > sizeof(buf)) {
		bp = os_malloc(len);
		if (bp == NULL) {
			wpa_printf(MSG_INFO,
				   "EAPOL frame discarded, cannot malloc temp buffer of size %lu!",
				   (unsigned long) len);
			return -1;
		}
	}
	eth = (struct l2_ethhdr *) bp;
	os_memcpy(eth->h_dest, addr, ETH_ALEN);
	os_memcpy(eth->h_source, own_addr, ETH_ALEN);
	eth->h_proto = host_to_be16(ETH_P_EAPOL);
	os_memcpy(eth + 1, data, data_len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", bp, len);

	status = l2_packet_send(drv->sock_xmit, addr, ETH_P_EAPOL, bp, len);

	if (bp != buf)
		os_free(bp);
	return status;
}

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct atheros_driver_data *drv = ctx;
	drv_event_eapol_rx(drv->hapd, src_addr, buf + sizeof(struct l2_ethhdr),
			   len - sizeof(struct l2_ethhdr));
}


static void atheros_read_fils_cap(struct atheros_driver_data *drv)
{
	int fils = 0;

#ifdef CONFIG_FILS
	/* TODO: Would be better to have #ifdef on the IEEE80211_PARAM_* value
	 * to automatically check this against the driver header files. */
	if (get80211param(drv, IEEE80211_PARAM_ENABLE_FILS, &fils) < 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: Failed to get FILS capability from driver",
			   __func__);
		/* Assume driver does not support FILS */
		fils = 0;
	}
#endif /* CONFIG_FILS */
	drv->fils_en = fils;
	wpa_printf(MSG_DEBUG, "atheros: fils_en=%d", drv->fils_en);
}


static void *
atheros_init(struct hostapd_data *hapd, struct wpa_init_params *params)
{
	struct atheros_driver_data *drv;
	struct ifreq ifr;
	struct iwreq iwr;
	char brname[IFNAMSIZ];

	drv = os_zalloc(sizeof(struct atheros_driver_data));
	if (drv == NULL) {
		wpa_printf(MSG_INFO,
			   "Could not allocate memory for atheros driver data");
		return NULL;
	}

	drv->hapd = hapd;
	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		wpa_printf(MSG_ERROR, "socket[PF_INET,SOCK_DGRAM]: %s",
			   strerror(errno));
		goto bad;
	}
	os_memcpy(drv->iface, params->ifname, sizeof(drv->iface));

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->iface, sizeof(ifr.ifr_name));
	if (ioctl(drv->ioctl_sock, SIOCGIFINDEX, &ifr) != 0) {
		wpa_printf(MSG_ERROR, "ioctl(SIOCGIFINDEX): %s",
			   strerror(errno));
		goto bad;
	}
	drv->ifindex = ifr.ifr_ifindex;

	drv->sock_xmit = l2_packet_init(drv->iface, NULL, ETH_P_EAPOL,
					handle_read, drv, 1);
	if (drv->sock_xmit == NULL)
		goto bad;
	if (l2_packet_get_own_addr(drv->sock_xmit, params->own_addr))
		goto bad;
	os_memcpy(drv->own_addr, params->own_addr, ETH_ALEN);
	if (params->bridge[0]) {
		wpa_printf(MSG_DEBUG, "Configure bridge %s for EAPOL traffic.",
			   params->bridge[0]);
		drv->sock_recv = l2_packet_init(params->bridge[0], NULL,
						ETH_P_EAPOL, handle_read, drv,
						1);
		if (drv->sock_recv == NULL)
			goto bad;
	} else if (linux_br_get(brname, drv->iface) == 0) {
		wpa_printf(MSG_DEBUG, "Interface in bridge %s; configure for "
			   "EAPOL receive", brname);
		drv->sock_recv = l2_packet_init(brname, NULL, ETH_P_EAPOL,
						handle_read, drv, 1);
		if (drv->sock_recv == NULL)
			goto bad;
	} else
		drv->sock_recv = drv->sock_xmit;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

	iwr.u.mode = IW_MODE_MASTER;

	if (ioctl(drv->ioctl_sock, SIOCSIWMODE, &iwr) < 0) {
		wpa_printf(MSG_ERROR,
			   "Could not set interface to master mode! ioctl[SIOCSIWMODE]: %s",
			   strerror(errno));
		goto bad;
	}

	/* mark down during setup */
	linux_set_iface_flags(drv->ioctl_sock, drv->iface, 0);
	atheros_set_privacy(drv, 0); /* default to no privacy */

	if (atheros_receive_pkt(drv))
		goto bad;

	if (atheros_wireless_event_init(drv))
		goto bad;

	/* Read FILS capability from the driver */
	atheros_read_fils_cap(drv);

	return drv;
bad:
	atheros_reset_appfilter(drv);
	if (drv->sock_raw)
		l2_packet_deinit(drv->sock_raw);
	if (drv->sock_recv != NULL && drv->sock_recv != drv->sock_xmit)
		l2_packet_deinit(drv->sock_recv);
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	os_free(drv);
	return NULL;
}


static void
atheros_deinit(void *priv)
{
	struct atheros_driver_data *drv = priv;

	atheros_reset_appfilter(drv);

	if (drv->wpa_ie || drv->wps_beacon_ie || drv->wps_probe_resp_ie) {
		atheros_set_opt_ie(priv, NULL, 0);
		wpabuf_free(drv->wpa_ie);
		wpabuf_free(drv->wps_beacon_ie);
		wpabuf_free(drv->wps_probe_resp_ie);
	}
	netlink_deinit(drv->netlink);
	(void) linux_set_iface_flags(drv->ioctl_sock, drv->iface, 0);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv->sock_recv != NULL && drv->sock_recv != drv->sock_xmit)
		l2_packet_deinit(drv->sock_recv);
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	if (drv->sock_raw)
		l2_packet_deinit(drv->sock_raw);
	os_free(drv);
}

static int
atheros_set_ssid(void *priv, const u8 *buf, int len)
{
	struct atheros_driver_data *drv = priv;
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCSIWESSID,len=%d]: %s",
			   len, strerror(errno));
		return -1;
	}
	return 0;
}

static int
atheros_get_ssid(void *priv, u8 *buf, int len)
{
	struct atheros_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = (len > IW_ESSID_MAX_SIZE) ?
		IW_ESSID_MAX_SIZE : len;

	if (ioctl(drv->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCGIWESSID]: %s",
			   strerror(errno));
		ret = -1;
	} else
		ret = iwr.u.essid.length;

	return ret;
}

static int
atheros_set_countermeasures(void *priv, int enabled)
{
	struct atheros_driver_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return set80211param(drv, IEEE80211_PARAM_COUNTERMEASURES, enabled);
}

static int
atheros_commit(void *priv)
{
	struct atheros_driver_data *drv = priv;
	return linux_set_iface_flags(drv->ioctl_sock, drv->iface, 1);
}

static int atheros_set_authmode(void *priv, int auth_algs)
{
	int authmode;

	if ((auth_algs & WPA_AUTH_ALG_OPEN) &&
	    (auth_algs & WPA_AUTH_ALG_SHARED))
		authmode = IEEE80211_AUTH_AUTO;
	else if (auth_algs & WPA_AUTH_ALG_OPEN)
		authmode = IEEE80211_AUTH_OPEN;
	else if (auth_algs & WPA_AUTH_ALG_SHARED)
		authmode = IEEE80211_AUTH_SHARED;
	else
		return -1;

	return set80211param(priv, IEEE80211_PARAM_AUTHMODE, authmode);
}

static int atheros_set_ap(void *priv, struct wpa_driver_ap_params *params)
{
	/*
	 * TODO: Use this to replace set_authmode, set_privacy, set_ieee8021x,
	 * set_generic_elem, and hapd_set_ssid.
	 */

	wpa_printf(MSG_DEBUG, "atheros: set_ap - pairwise_ciphers=0x%x "
		   "group_cipher=0x%x key_mgmt_suites=0x%x auth_algs=0x%x "
		   "wpa_version=0x%x privacy=%d interworking=%d",
		   params->pairwise_ciphers, params->group_cipher,
		   params->key_mgmt_suites, params->auth_algs,
		   params->wpa_version, params->privacy, params->interworking);
	wpa_hexdump_ascii(MSG_DEBUG, "atheros: SSID",
			  params->ssid, params->ssid_len);
	if (params->hessid)
		wpa_printf(MSG_DEBUG, "atheros: HESSID " MACSTR,
			   MAC2STR(params->hessid));
	wpa_hexdump_buf(MSG_DEBUG, "atheros: beacon_ies",
			params->beacon_ies);
	wpa_hexdump_buf(MSG_DEBUG, "atheros: proberesp_ies",
			params->proberesp_ies);
	wpa_hexdump_buf(MSG_DEBUG, "atheros: assocresp_ies",
			params->assocresp_ies);

#if defined(CONFIG_HS20) && (defined(IEEE80211_PARAM_OSEN) || defined(CONFIG_ATHEROS_OSEN))
	if (params->osen) {
		struct wpa_bss_params bss_params;

		os_memset(&bss_params, 0, sizeof(struct wpa_bss_params));
		bss_params.enabled = 1;
		bss_params.wpa = 2;
		bss_params.wpa_pairwise = WPA_CIPHER_CCMP;
		bss_params.wpa_group = WPA_CIPHER_CCMP;
		bss_params.ieee802_1x = 1;

		if (atheros_set_privacy(priv, 1) ||
		    set80211param(priv, IEEE80211_PARAM_OSEN, 1))
			return -1;

		return atheros_set_ieee8021x(priv, &bss_params);
	}
#endif /* CONFIG_HS20 && IEEE80211_PARAM_OSEN */

	return 0;
}


static int atheros_send_mgmt(void *priv, const u8 *frm, size_t data_len,
			     int noack, unsigned int freq,
			     const u16 *csa_offs, size_t csa_offs_len,
			     int no_encrypt, unsigned int wait)
{
	struct atheros_driver_data *drv = priv;
	u8 buf[1510];
	const struct ieee80211_mgmt *mgmt;
	struct ieee80211req_mgmtbuf *mgmt_frm;

	mgmt = (const struct ieee80211_mgmt *) frm;
	wpa_printf(MSG_DEBUG, "%s frmlen = %lu " MACSTR, __func__,
		   (unsigned long) data_len, MAC2STR(mgmt->da));
	mgmt_frm = (struct ieee80211req_mgmtbuf *) buf;
	os_memcpy(mgmt_frm->macaddr, (u8 *)mgmt->da, IEEE80211_ADDR_LEN);
	mgmt_frm->buflen = data_len;
	if (&mgmt_frm->buf[0] + data_len > buf + sizeof(buf)) {
		wpa_printf(MSG_INFO, "atheros: Too long frame for "
			   "atheros_send_mgmt (%u)", (unsigned int) data_len);
		return -1;
	}
	os_memcpy(&mgmt_frm->buf[0], frm, data_len);
	return set80211priv(drv, IEEE80211_IOCTL_SEND_MGMT, mgmt_frm,
			    sizeof(struct ieee80211req_mgmtbuf) + data_len);
}


#ifdef CONFIG_IEEE80211R

static int atheros_add_tspec(void *priv, const u8 *addr, u8 *tspec_ie,
			     size_t tspec_ielen)
{
	struct atheros_driver_data *drv = priv;
	int retv;
	struct ieee80211req_res req;
	struct ieee80211req_res_addts *addts = &req.u.addts;

	wpa_printf(MSG_DEBUG, "%s", __func__);
	req.type = IEEE80211_RESREQ_ADDTS;
	os_memcpy(&req.macaddr[0], addr, IEEE80211_ADDR_LEN);
	os_memcpy(addts->tspecie, tspec_ie, tspec_ielen);
	retv = set80211priv(drv, IEEE80211_IOCTL_RES_REQ, &req,
			    sizeof(struct ieee80211req_res));
	if (retv < 0) {
		wpa_printf(MSG_DEBUG, "%s IEEE80211_IOCTL_RES_REQ FAILED "
			   "retv = %d", __func__, retv);
		return -1;
	}
	os_memcpy(tspec_ie, addts->tspecie, tspec_ielen);
	return addts->status;
}


static int atheros_add_sta_node(void *priv, const u8 *addr, u16 auth_alg)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211req_res req;
	struct ieee80211req_res_addnode *addnode = &req.u.addnode;

	wpa_printf(MSG_DEBUG, "%s", __func__);
	req.type = IEEE80211_RESREQ_ADDNODE;
	os_memcpy(&req.macaddr[0], addr, IEEE80211_ADDR_LEN);
	addnode->auth_alg = auth_alg;
	return set80211priv(drv, IEEE80211_IOCTL_RES_REQ, &req,
			    sizeof(struct ieee80211req_res));
}

#endif /* CONFIG_IEEE80211R */


/* Use only to set a big param, get will not work. */
static int
set80211big(struct atheros_driver_data *drv, int op, const void *data, int len)
{
	struct iwreq iwr;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

	iwr.u.data.pointer = (void *) data;
	iwr.u.data.length = len;
	iwr.u.data.flags = op;
	wpa_printf(MSG_DEBUG, "%s: op=0x%x=%d (%s) len=0x%x",
		   __func__, op, op, athr_get_param_name(op), len);

	if (ioctl(drv->ioctl_sock, IEEE80211_IOCTL_P2P_BIG_PARAM, &iwr) < 0) {
		wpa_printf(MSG_DEBUG, "%s: op=0x%x (%s) subop=0x%x=%d "
			   "value=0x%x,0x%x failed: %d (%s)",
			   __func__, op, athr_get_ioctl_name(op), iwr.u.mode,
			   iwr.u.mode, iwr.u.data.length,
			   iwr.u.data.flags, errno, strerror(errno));
		return -1;
	}
	return 0;
}


static int atheros_send_action(void *priv, unsigned int freq,
			       unsigned int wait,
			       const u8 *dst, const u8 *src,
			       const u8 *bssid,
			       const u8 *data, size_t data_len, int no_cck)
{
	struct atheros_driver_data *drv = priv;
	struct ieee80211_p2p_send_action *act;
	int res;

	act = os_zalloc(sizeof(*act) + data_len);
	if (act == NULL)
		return -1;
	act->freq = freq;
	os_memcpy(act->dst_addr, dst, ETH_ALEN);
	os_memcpy(act->src_addr, src, ETH_ALEN);
	os_memcpy(act->bssid, bssid, ETH_ALEN);
	os_memcpy(act + 1, data, data_len);
	wpa_printf(MSG_DEBUG, "%s: freq=%d, wait=%u, dst=" MACSTR ", src="
		   MACSTR ", bssid=" MACSTR,
		   __func__, act->freq, wait, MAC2STR(act->dst_addr),
		   MAC2STR(act->src_addr), MAC2STR(act->bssid));
	wpa_hexdump(MSG_MSGDUMP, "athr: act", (u8 *) act, sizeof(*act));
	wpa_hexdump(MSG_MSGDUMP, "athr: data", data, data_len);

	res = set80211big(drv, IEEE80211_IOC_P2P_SEND_ACTION,
			  act, sizeof(*act) + data_len);
	os_free(act);
	return res;
}


#if defined(CONFIG_WNM) && defined(IEEE80211_APPIE_FRAME_WNM)
static int athr_wnm_tfs(struct atheros_driver_data *drv, const u8* peer,
			u8 *ie, u16 *len, enum wnm_oper oper)
{
#define IEEE80211_APPIE_MAX    1024 /* max appie buffer size */
	u8 buf[IEEE80211_APPIE_MAX];
	struct ieee80211req_getset_appiebuf *tfs_ie;
	u16 val;

	wpa_printf(MSG_DEBUG, "atheros: ifname=%s, WNM TFS IE oper=%d " MACSTR,
		   drv->iface, oper, MAC2STR(peer));

	switch (oper) {
	case WNM_SLEEP_TFS_REQ_IE_SET:
		if (*len > IEEE80211_APPIE_MAX -
		    sizeof(struct ieee80211req_getset_appiebuf)) {
			wpa_printf(MSG_DEBUG, "TFS Req IE(s) too large");
			return -1;
		}
		tfs_ie = (struct ieee80211req_getset_appiebuf *) buf;
		tfs_ie->app_frmtype = IEEE80211_APPIE_FRAME_WNM;
		tfs_ie->app_buflen = ETH_ALEN + 2 + 2 + *len;

		/* Command header for driver */
		os_memcpy(&(tfs_ie->app_buf[0]), peer, ETH_ALEN);
		val = oper;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN, &val, 2);
		val = *len;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN + 2, &val, 2);

		/* copy the ie */
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN + 2 + 2, ie, *len);

		if (set80211priv(drv, IEEE80211_IOCTL_SET_APPIEBUF, tfs_ie,
				 IEEE80211_APPIE_MAX)) {
			wpa_printf(MSG_DEBUG, "%s: Failed to set WNM TFS IE: "
				   "%s", __func__, strerror(errno));
			return -1;
		}
		break;
	case WNM_SLEEP_TFS_RESP_IE_ADD:
		tfs_ie = (struct ieee80211req_getset_appiebuf *) buf;
		tfs_ie->app_frmtype = IEEE80211_APPIE_FRAME_WNM;
		tfs_ie->app_buflen = IEEE80211_APPIE_MAX -
			sizeof(struct ieee80211req_getset_appiebuf);
		/* Command header for driver */
		os_memcpy(&(tfs_ie->app_buf[0]), peer, ETH_ALEN);
		val = oper;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN, &val, 2);
		val = 0;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN + 2, &val, 2);

		if (set80211priv(drv, IEEE80211_IOCTL_GET_APPIEBUF, tfs_ie,
				 IEEE80211_APPIE_MAX)) {
			wpa_printf(MSG_DEBUG, "%s: Failed to get WNM TFS IE: "
				   "%s", __func__, strerror(errno));
			return -1;
		}

		*len = tfs_ie->app_buflen;
		os_memcpy(ie, &(tfs_ie->app_buf[0]), *len);
		wpa_printf(MSG_DEBUG, "atheros: %c len=%d", tfs_ie->app_buf[0],
			   *len);
		break;
	case WNM_SLEEP_TFS_RESP_IE_NONE:
		*len = 0;
		break;
	case WNM_SLEEP_TFS_IE_DEL:
		tfs_ie = (struct ieee80211req_getset_appiebuf *) buf;
		tfs_ie->app_frmtype = IEEE80211_APPIE_FRAME_WNM;
		tfs_ie->app_buflen = IEEE80211_APPIE_MAX -
			sizeof(struct ieee80211req_getset_appiebuf);
		/* Command header for driver */
		os_memcpy(&(tfs_ie->app_buf[0]), peer, ETH_ALEN);
		val = oper;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN, &val, 2);
		val = 0;
		os_memcpy(&(tfs_ie->app_buf[0]) + ETH_ALEN + 2, &val, 2);

		if (set80211priv(drv, IEEE80211_IOCTL_SET_APPIEBUF, tfs_ie,
				 IEEE80211_APPIE_MAX)) {
			wpa_printf(MSG_DEBUG, "%s: Failed to set WNM TFS IE: "
				   "%s", __func__, strerror(errno));
			return -1;
		}
		break;
	default:
		wpa_printf(MSG_DEBUG, "Unsupported TFS oper %d", oper);
		break;
	}

	return 0;
}


static int atheros_wnm_sleep(struct atheros_driver_data *drv,
			     const u8 *peer, enum wnm_oper oper)
{
	u8 *data, *pos;
	size_t dlen;
	int ret;
	u16 val;

	wpa_printf(MSG_DEBUG, "atheros: WNM-Sleep Oper %d, " MACSTR,
		   oper, MAC2STR(peer));

	dlen = ETH_ALEN + 2 + 2;
	data = os_malloc(dlen);
	if (data == NULL)
		return -1;

	/* Command header for driver */
	pos = data;
	os_memcpy(pos, peer, ETH_ALEN);
	pos += ETH_ALEN;

	val = oper;
	os_memcpy(pos, &val, 2);
	pos += 2;

	val = 0;
	os_memcpy(pos, &val, 2);

	ret = atheros_set_wps_ie(drv, data, dlen, IEEE80211_APPIE_FRAME_WNM);

	os_free(data);

	return ret;
}


static int atheros_wnm_oper(void *priv, enum wnm_oper oper, const u8 *peer,
			    u8 *buf, u16 *buf_len)
{
	struct atheros_driver_data *drv = priv;

	switch (oper) {
	case WNM_SLEEP_ENTER_CONFIRM:
	case WNM_SLEEP_ENTER_FAIL:
	case WNM_SLEEP_EXIT_CONFIRM:
	case WNM_SLEEP_EXIT_FAIL:
		return atheros_wnm_sleep(drv, peer, oper);
	case WNM_SLEEP_TFS_REQ_IE_SET:
	case WNM_SLEEP_TFS_RESP_IE_ADD:
	case WNM_SLEEP_TFS_RESP_IE_NONE:
	case WNM_SLEEP_TFS_IE_DEL:
		return athr_wnm_tfs(drv, peer, buf, buf_len, oper);
	default:
		wpa_printf(MSG_DEBUG, "atheros: Unsupported WNM operation %d",
			   oper);
		return -1;
	}
}
#endif /* CONFIG_WNM && IEEE80211_APPIE_FRAME_WNM */


const struct wpa_driver_ops wpa_driver_atheros_ops = {
	.name			= "atheros",
	.hapd_init		= atheros_init,
	.hapd_deinit		= atheros_deinit,
	.set_ieee8021x		= atheros_set_ieee8021x,
	.set_privacy		= atheros_set_privacy,
	.set_key		= atheros_set_key,
	.get_seqnum		= atheros_get_seqnum,
	.flush			= atheros_flush,
	.set_generic_elem	= atheros_set_opt_ie,
	.sta_set_flags		= atheros_sta_set_flags,
	.read_sta_data		= atheros_read_sta_driver_data,
	.hapd_send_eapol	= atheros_send_eapol,
	.sta_disassoc		= atheros_sta_disassoc,
	.sta_deauth		= atheros_sta_deauth,
	.hapd_set_ssid		= atheros_set_ssid,
	.hapd_get_ssid		= atheros_get_ssid,
	.set_countermeasures	= atheros_set_countermeasures,
	.sta_clear_stats	= atheros_sta_clear_stats,
	.commit			= atheros_commit,
	.set_ap_wps_ie		= atheros_set_ap_wps_ie,
	.set_authmode		= atheros_set_authmode,
	.set_ap			= atheros_set_ap,
	.sta_assoc              = atheros_sta_assoc,
	.sta_auth               = atheros_sta_auth,
	.send_mlme       	= atheros_send_mgmt,
#ifdef CONFIG_IEEE80211R
	.add_tspec      	= atheros_add_tspec,
	.add_sta_node    	= atheros_add_sta_node,
#endif /* CONFIG_IEEE80211R */
	.send_action		= atheros_send_action,
#if defined(CONFIG_WNM) && defined(IEEE80211_APPIE_FRAME_WNM)
	.wnm_oper		= atheros_wnm_oper,
#endif /* CONFIG_WNM && IEEE80211_APPIE_FRAME_WNM */
	.set_qos_map		= atheros_set_qos_map,
};
