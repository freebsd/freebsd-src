/*
 * WPA Supplicant - driver interaction with BSD net80211 layer
 * Copyright (c) 2004, Sam Leffler <sam@errno.com>
 * Copyright (c) 2004, 2Wire, Inc
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>

#include "common.h"
#include "driver.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef __NetBSD__
#include <net/if_ether.h>
#else
#include <net/ethernet.h>
#endif
#include <net/route.h>

#ifdef __DragonFly__
#include <netproto/802_11/ieee80211_ioctl.h>
#include <netproto/802_11/ieee80211_dragonfly.h>
#else /* __DragonFly__ */
#ifdef __GLIBC__
#include <netinet/ether.h>
#endif /* __GLIBC__ */
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <net80211/ieee80211_crypto.h>
#endif /* __DragonFly__ || __GLIBC__ */
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <net80211/ieee80211_freebsd.h>
#endif
#if __NetBSD__
#include <net80211/ieee80211_netbsd.h>
#endif

#include "l2_packet/l2_packet.h"

struct bsd_driver_global {
	void		*ctx;
	int		sock;			/* socket for 802.11 ioctls */
	int		route;			/* routing socket for events */
	struct dl_list	ifaces;			/* list of interfaces */
};

struct bsd_driver_data {
	struct dl_list	list;
	struct bsd_driver_global *global;
	void	*ctx;

	struct l2_packet_data *sock_xmit;/* raw packet xmit socket */
	char	ifname[IFNAMSIZ+1];	/* interface name */
	int	flags;
	unsigned int ifindex;		/* interface index */
	int	if_removed;		/* has the interface been removed? */
	struct wpa_driver_capa capa;	/* driver capability */
	int	is_ap;			/* Access point mode */
	int	prev_roaming;	/* roaming state to restore on deinit */
	int	prev_privacy;	/* privacy state to restore on deinit */
	int	prev_wpa;	/* wpa state to restore on deinit */
	enum ieee80211_opmode opmode;	/* operation mode */
};

/* Generic functions for hostapd and wpa_supplicant */

static struct bsd_driver_data *
bsd_get_drvindex(void *priv, unsigned int ifindex)
{
	struct bsd_driver_global *global = priv;
	struct bsd_driver_data *drv;

	dl_list_for_each(drv, &global->ifaces, struct bsd_driver_data, list) {
		if (drv->ifindex == ifindex)
			return drv;
	}
	return NULL;
}

static struct bsd_driver_data *
bsd_get_drvname(void *priv, const char *ifname)
{
	struct bsd_driver_global *global = priv;
	struct bsd_driver_data *drv;

	dl_list_for_each(drv, &global->ifaces, struct bsd_driver_data, list) {
		if (os_strcmp(drv->ifname, ifname) == 0)
			return drv;
	}
	return NULL;
}

static int
bsd_set80211(void *priv, int op, int val, const void *arg, int arg_len)
{
	struct bsd_driver_data *drv = priv;
	struct ieee80211req ireq;

	if (drv->ifindex == 0 || drv->if_removed)
		return -1;

	os_memset(&ireq, 0, sizeof(ireq));
	os_strlcpy(ireq.i_name, drv->ifname, sizeof(ireq.i_name));
	ireq.i_type = op;
	ireq.i_val = val;
	ireq.i_data = (void *) arg;
	ireq.i_len = arg_len;

	if (ioctl(drv->global->sock, SIOCS80211, &ireq) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCS80211, op=%u, val=%u, "
			   "arg_len=%u]: %s", op, val, arg_len,
			   strerror(errno));
		return -1;
	}
	return 0;
}

static int
bsd_get80211(void *priv, struct ieee80211req *ireq, int op, void *arg,
	     int arg_len)
{
	struct bsd_driver_data *drv = priv;

	os_memset(ireq, 0, sizeof(*ireq));
	os_strlcpy(ireq->i_name, drv->ifname, sizeof(ireq->i_name));
	ireq->i_type = op;
	ireq->i_len = arg_len;
	ireq->i_data = arg;

	if (ioctl(drv->global->sock, SIOCG80211, ireq) < 0) {
		int level = drv->if_removed ? MSG_DEBUG : MSG_ERROR;

		wpa_printf(level, "ioctl[SIOCG80211, op=%u, "
			   "arg_len=%u]: %s", op, arg_len, strerror(errno));
		return -1;
	}
	return 0;
}

static int
get80211var(struct bsd_driver_data *drv, int op, void *arg, int arg_len)
{
	struct ieee80211req ireq;

	if (bsd_get80211(drv, &ireq, op, arg, arg_len) < 0)
		return -1;
	return ireq.i_len;
}

static int
set80211var(struct bsd_driver_data *drv, int op, const void *arg, int arg_len)
{
	return bsd_set80211(drv, op, 0, arg, arg_len);
}

static int
set80211param(struct bsd_driver_data *drv, int op, int arg)
{
	return bsd_set80211(drv, op, arg, NULL, 0);
}

static int
bsd_get_ssid(void *priv, u8 *ssid, int len)
{
	struct bsd_driver_data *drv = priv;
#ifdef SIOCG80211NWID
	struct ieee80211_nwid nwid;
	struct ifreq ifr;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->ifname, sizeof(ifr.ifr_name));
	ifr.ifr_data = (void *)&nwid;
	if (ioctl(drv->global->sock, SIOCG80211NWID, &ifr) < 0 ||
	    nwid.i_len > IEEE80211_NWID_LEN)
		return -1;
	os_memcpy(ssid, nwid.i_nwid, nwid.i_len);
	return nwid.i_len;
#else
	return get80211var(drv, IEEE80211_IOC_SSID, ssid, IEEE80211_NWID_LEN);
#endif
}

static int
bsd_set_ssid(void *priv, const u8 *ssid, int ssid_len)
{
	struct bsd_driver_data *drv = priv;
#ifdef SIOCS80211NWID
	struct ieee80211_nwid nwid;
	struct ifreq ifr;

	os_memcpy(nwid.i_nwid, ssid, ssid_len);
	nwid.i_len = ssid_len;
	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->ifname, sizeof(ifr.ifr_name));
	ifr.ifr_data = (void *)&nwid;
	return ioctl(drv->global->sock, SIOCS80211NWID, &ifr);
#else
	return set80211var(drv, IEEE80211_IOC_SSID, ssid, ssid_len);
#endif
}

static int
bsd_get_if_media(void *priv)
{
	struct bsd_driver_data *drv = priv;
	struct ifmediareq ifmr;

	os_memset(&ifmr, 0, sizeof(ifmr));
	os_strlcpy(ifmr.ifm_name, drv->ifname, sizeof(ifmr.ifm_name));

	if (ioctl(drv->global->sock, SIOCGIFMEDIA, &ifmr) < 0) {
		wpa_printf(MSG_ERROR, "%s: SIOCGIFMEDIA %s", __func__,
			   strerror(errno));
		return -1;
	}

	return ifmr.ifm_current;
}

static int
bsd_set_if_media(void *priv, int media)
{
	struct bsd_driver_data *drv = priv;
	struct ifreq ifr;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->ifname, sizeof(ifr.ifr_name));
	ifr.ifr_media = media;

	if (ioctl(drv->global->sock, SIOCSIFMEDIA, &ifr) < 0) {
		wpa_printf(MSG_ERROR, "%s: SIOCSIFMEDIA %s", __func__,
			   strerror(errno));
		return -1;
	}

	return 0;
}

static int
bsd_set_mediaopt(void *priv, uint32_t mask, uint32_t mode)
{
	int media = bsd_get_if_media(priv);

	if (media < 0)
		return -1;
	media &= ~mask;
	media |= mode;
	if (bsd_set_if_media(priv, media) < 0)
		return -1;
	return 0;
}

static int
bsd_del_key(void *priv, const u8 *addr, int key_idx)
{
	struct ieee80211req_del_key wk;

	os_memset(&wk, 0, sizeof(wk));
	if (addr == NULL) {
		wpa_printf(MSG_DEBUG, "%s: key_idx=%d", __func__, key_idx);
		wk.idk_keyix = key_idx;
	} else {
		wpa_printf(MSG_DEBUG, "%s: addr=" MACSTR, __func__,
			   MAC2STR(addr));
		os_memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.idk_keyix = (u_int8_t) IEEE80211_KEYIX_NONE;	/* XXX */
	}

	return set80211var(priv, IEEE80211_IOC_DELKEY, &wk, sizeof(wk));
}

static int
bsd_send_mlme_param(void *priv, const u8 op, const u16 reason, const u8 *addr)
{
	struct ieee80211req_mlme mlme;

	os_memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = op;
	mlme.im_reason = reason;
	os_memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	return set80211var(priv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme));
}

static int
bsd_ctrl_iface(void *priv, int enable)
{
	struct bsd_driver_data *drv = priv;
	struct ifreq ifr;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->ifname, sizeof(ifr.ifr_name));

	if (ioctl(drv->global->sock, SIOCGIFFLAGS, &ifr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCGIFFLAGS]: %s",
			   strerror(errno));
		return -1;
	}
	drv->flags = ifr.ifr_flags;


	if (enable) {
		if (ifr.ifr_flags & IFF_UP)
			goto nochange;
		ifr.ifr_flags |= IFF_UP;
	} else {
		if (!(ifr.ifr_flags & IFF_UP))
			goto nochange;
		ifr.ifr_flags &= ~IFF_UP;
	}

	if (ioctl(drv->global->sock, SIOCSIFFLAGS, &ifr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCSIFFLAGS]: %s",
			   strerror(errno));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: if %s (changed) enable %d IFF_UP %d ",
	    __func__, drv->ifname, enable, ((ifr.ifr_flags & IFF_UP) != 0));

	drv->flags = ifr.ifr_flags;
	return 0;

nochange:
	wpa_printf(MSG_DEBUG, "%s: if %s (no change) enable %d IFF_UP %d ",
	    __func__, drv->ifname, enable, ((ifr.ifr_flags & IFF_UP) != 0));
	return 0;
}

static int
bsd_set_key(void *priv, struct wpa_driver_set_key_params *params)
{
	struct ieee80211req_key wk;
#ifdef IEEE80211_KEY_NOREPLAY
	struct bsd_driver_data *drv = priv;
#endif /* IEEE80211_KEY_NOREPLAY */
	enum wpa_alg alg = params->alg;
	const u8 *addr = params->addr;
	int key_idx = params->key_idx;
	int set_tx = params->set_tx;
	const u8 *seq = params->seq;
	size_t seq_len = params->seq_len;
	const u8 *key = params->key;
	size_t key_len = params->key_len;

	wpa_printf(MSG_DEBUG, "%s: alg=%d addr=%p key_idx=%d set_tx=%d "
		   "seq_len=%zu key_len=%zu", __func__, alg, addr, key_idx,
		   set_tx, seq_len, key_len);

	if (alg == WPA_ALG_NONE) {
#ifndef HOSTAPD
		if (addr == NULL || is_broadcast_ether_addr(addr))
			return bsd_del_key(priv, NULL, key_idx);
		else
#endif /* HOSTAPD */
			return bsd_del_key(priv, addr, key_idx);
	}

	os_memset(&wk, 0, sizeof(wk));
	switch (alg) {
	case WPA_ALG_WEP:
		wk.ik_type = IEEE80211_CIPHER_WEP;
		break;
	case WPA_ALG_TKIP:
		wk.ik_type = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_ALG_CCMP:
		wk.ik_type = IEEE80211_CIPHER_AES_CCM;
		break;
	case WPA_ALG_CCMP_256:
		wk.ik_type = IEEE80211_CIPHER_AES_CCM_256;
		break;
	case WPA_ALG_GCMP:
		wk.ik_type = IEEE80211_CIPHER_AES_GCM_128;
		break;
	case WPA_ALG_GCMP_256:
		wk.ik_type = IEEE80211_CIPHER_AES_GCM_256;
		break;
	case WPA_ALG_BIP_CMAC_128:
		wk.ik_type = IEEE80211_CIPHER_BIP_CMAC_128;
		break;
	default:
		wpa_printf(MSG_ERROR, "%s: unknown alg=%d", __func__, alg);
		return -1;
	}

	wk.ik_flags = IEEE80211_KEY_RECV;
	if (set_tx)
		wk.ik_flags |= IEEE80211_KEY_XMIT;

	if (addr == NULL) {
		os_memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wk.ik_keyix = key_idx;
	} else {
		os_memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
		/*
		 * Deduce whether group/global or unicast key by checking
		 * the address (yech).  Note also that we can only mark global
		 * keys default; doing this for a unicast key is an error.
		 */
		if (is_broadcast_ether_addr(addr)) {
			wk.ik_flags |= IEEE80211_KEY_GROUP;
			wk.ik_keyix = key_idx;
		} else {
			wk.ik_keyix = key_idx == 0 ? IEEE80211_KEYIX_NONE :
				key_idx;
		}
	}
	if (wk.ik_keyix != IEEE80211_KEYIX_NONE && set_tx)
		wk.ik_flags |= IEEE80211_KEY_DEFAULT;
#ifndef HOSTAPD
#ifdef IEEE80211_KEY_NOREPLAY
	/*
	 * Ignore replay failures in IBSS and AHDEMO mode.
	 */
	if (drv->opmode == IEEE80211_M_IBSS ||
	    drv->opmode == IEEE80211_M_AHDEMO)
		wk.ik_flags |= IEEE80211_KEY_NOREPLAY;
#endif /* IEEE80211_KEY_NOREPLAY */
#endif /* HOSTAPD */
	wk.ik_keylen = key_len;
	if (seq) {
#ifdef WORDS_BIGENDIAN
		/*
		 * wk.ik_keyrsc is in host byte order (big endian), need to
		 * swap it to match with the byte order used in WPA.
		 */
		int i;
		u8 *keyrsc = (u8 *) &wk.ik_keyrsc;
		for (i = 0; i < seq_len; i++)
			keyrsc[WPA_KEY_RSC_LEN - i - 1] = seq[i];
#else /* WORDS_BIGENDIAN */
		os_memcpy(&wk.ik_keyrsc, seq, seq_len);
#endif /* WORDS_BIGENDIAN */
	}
	os_memcpy(wk.ik_keydata, key, key_len);

	return set80211var(priv, IEEE80211_IOC_WPAKEY, &wk, sizeof(wk));
}

static int
bsd_configure_wpa(void *priv, struct wpa_bss_params *params)
{
#ifndef IEEE80211_IOC_APPIE
	static const char *ciphernames[] =
		{ "WEP", "TKIP", "AES-OCB", "AES-CCM", "CKIP", "NONE",
		  "AES-CCM-256", "BIP-CMAC-128", "BIP-CMAC-256", "BIP-GMAC-128",
		  "BIP-GMAC-256", "AES-GCM-128", "AES-GCM-256" };
	int v;

	switch (params->wpa_group) {
	case WPA_CIPHER_CCMP:
		v = IEEE80211_CIPHER_AES_CCM;
		break;
	case WPA_CIPHER_CCMP_256:
		v = IEEE80211_CIPHER_AES_CCM_256;
		break;
	case WPA_CIPHER_GCMP:
		v = IEEE80211_CIPHER_AES_GCM_128;
		break;
	case WPA_CIPHER_GCMP_256:
		v = IEEE80211_CIPHER_AES_GCM_256;
		break;
	case WPA_CIPHER_BIP_CMAC_128:
		v = IEEE80211_CIPHER_BIP_CMAC_128;
		break;
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
		wpa_printf(MSG_INFO, "Unknown group key cipher %u",
			   params->wpa_group);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "%s: group key cipher=%s (%u)",
		   __func__, ciphernames[v], v);
	if (set80211param(priv, IEEE80211_IOC_MCASTCIPHER, v)) {
		wpa_printf(MSG_INFO,
			   "Unable to set group key cipher to %u (%s)",
			   v, ciphernames[v]);
		return -1;
	}
	if (v == IEEE80211_CIPHER_WEP) {
		/* key length is done only for specific ciphers */
		v = (params->wpa_group == WPA_CIPHER_WEP104 ? 13 : 5);
		if (set80211param(priv, IEEE80211_IOC_MCASTKEYLEN, v)) {
			wpa_printf(MSG_INFO,
				   "Unable to set group key length to %u", v);
			return -1;
		}
	}

	v = 0;
	if (params->wpa_pairwise & WPA_CIPHER_BIP_CMAC_128)
		v |= 1<<IEEE80211_CIPHER_BIP_CMAC_128;
	if (params->wpa_pairwise & WPA_CIPHER_GCMP)
		v |= 1<<IEEE80211_CIPHER_AES_GCM_128;
	if (params->wpa_pairwise & WPA_CIPHER_GCMP_256)
		v |= 1<<IEEE80211_CIPHER_AES_GCM_256;
	if (params->wpa_pairwise & WPA_CIPHER_CCMP)
		v |= 1<<IEEE80211_CIPHER_AES_CCM;
	if (params->wpa_pairwise & WPA_CIPHER_CCMP_256)
		v |= 1<<IEEE80211_CIPHER_AES_CCM_256;
	if (params->wpa_pairwise & WPA_CIPHER_TKIP)
		v |= 1<<IEEE80211_CIPHER_TKIP;
	if (params->wpa_pairwise & WPA_CIPHER_NONE)
		v |= 1<<IEEE80211_CIPHER_NONE;
	wpa_printf(MSG_DEBUG, "%s: pairwise key ciphers=0x%x", __func__, v);
	if (set80211param(priv, IEEE80211_IOC_UCASTCIPHERS, v)) {
		wpa_printf(MSG_INFO,
			   "Unable to set pairwise key ciphers to 0x%x", v);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: key management algorithms=0x%x",
		   __func__, params->wpa_key_mgmt);
	if (set80211param(priv, IEEE80211_IOC_KEYMGTALGS,
			  params->wpa_key_mgmt)) {
		wpa_printf(MSG_INFO,
			   "Unable to set key management algorithms to 0x%x",
			   params->wpa_key_mgmt);
		return -1;
	}

	v = 0;
	if (params->rsn_preauth)
		v |= BIT(0);
	wpa_printf(MSG_DEBUG, "%s: rsn capabilities=0x%x",
		   __func__, params->rsn_preauth);
	if (set80211param(priv, IEEE80211_IOC_RSNCAPS, v)) {
		wpa_printf(MSG_INFO, "Unable to set RSN capabilities to 0x%x",
			   v);
		return -1;
	}
#endif /* IEEE80211_IOC_APPIE */

	wpa_printf(MSG_DEBUG, "%s: enable WPA= 0x%x", __func__, params->wpa);
	if (set80211param(priv, IEEE80211_IOC_WPA, params->wpa)) {
		wpa_printf(MSG_INFO, "Unable to set WPA to %u", params->wpa);
		return -1;
	}
	return 0;
}

static int
bsd_set_ieee8021x(void *priv, struct wpa_bss_params *params)
{
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, params->enabled);

	if (!params->enabled) {
		/* XXX restore state */
		return set80211param(priv, IEEE80211_IOC_AUTHMODE,
				     IEEE80211_AUTH_AUTO);
	}
	if (!params->wpa && !params->ieee802_1x) {
		wpa_printf(MSG_ERROR, "%s: No 802.1X or WPA enabled",
			   __func__);
		return -1;
	}
	if (params->wpa && bsd_configure_wpa(priv, params) != 0) {
		wpa_printf(MSG_ERROR, "%s: Failed to configure WPA state",
			   __func__);
		return -1;
	}
	if (set80211param(priv, IEEE80211_IOC_AUTHMODE,
		(params->wpa ? IEEE80211_AUTH_WPA : IEEE80211_AUTH_8021X))) {
		wpa_printf(MSG_ERROR, "%s: Failed to enable WPA/802.1X",
			   __func__);
		return -1;
	}
	return bsd_ctrl_iface(priv, 1);
}

static void
bsd_new_sta(void *priv, void *ctx, u8 addr[IEEE80211_ADDR_LEN])
{
	struct ieee80211req_wpaie ie;
	int ielen = 0;
	u8 *iebuf = NULL;

	/*
	 * Fetch and validate any negotiated WPA/RSN parameters.
	 */
	memset(&ie, 0, sizeof(ie));
	memcpy(ie.wpa_macaddr, addr, IEEE80211_ADDR_LEN);
	if (get80211var(priv, IEEE80211_IOC_WPAIE, &ie, sizeof(ie)) < 0) {
		wpa_printf(MSG_INFO,
			   "Failed to get WPA/RSN information element");
		goto no_ie;
	}
	iebuf = ie.wpa_ie;
	ielen = ie.wpa_ie[1];
	if (ielen == 0)
		iebuf = NULL;
	else
		ielen += 2;

no_ie:
	drv_event_assoc(ctx, addr, iebuf, ielen, NULL, 0, NULL, -1, 0);
}

static int
bsd_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
	       int encrypt, const u8 *own_addr, u32 flags, int link_id)
{
	struct bsd_driver_data *drv = priv;

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", data, data_len);

	return l2_packet_send(drv->sock_xmit, addr, ETH_P_EAPOL, data,
			      data_len);
}

static int
bsd_set_freq(void *priv, struct hostapd_freq_params *freq)
{
	struct bsd_driver_data *drv = priv;
#ifdef SIOCS80211CHANNEL
	struct ieee80211chanreq creq;
#endif /* SIOCS80211CHANNEL */
	u32 mode;
	int channel = freq->channel;

	if (channel < 14) {
		mode =
			freq->ht_enabled ? IFM_IEEE80211_11NG :
			IFM_IEEE80211_11G;
	} else if (channel == 14) {
		mode = IFM_IEEE80211_11B;
	} else {
		mode =
			freq->vht_enabled ? IFM_IEEE80211_VHT5G :
			freq->ht_enabled ? IFM_IEEE80211_11NA :
			IFM_IEEE80211_11A;
	}
	if (bsd_set_mediaopt(drv, IFM_MMASK, mode) < 0) {
		wpa_printf(MSG_ERROR, "%s: failed to set modulation mode",
			   __func__);
		return -1;
	}

#ifdef SIOCS80211CHANNEL
	os_memset(&creq, 0, sizeof(creq));
	os_strlcpy(creq.i_name, drv->ifname, sizeof(creq.i_name));
	creq.i_channel = (u_int16_t)channel;
	return ioctl(drv->global->sock, SIOCS80211CHANNEL, &creq);
#else /* SIOCS80211CHANNEL */
	return set80211param(priv, IEEE80211_IOC_CHANNEL, channel);
#endif /* SIOCS80211CHANNEL */
}

static int
bsd_set_opt_ie(void *priv, const u8 *ie, size_t ie_len)
{
#ifdef IEEE80211_IOC_APPIE
	wpa_printf(MSG_DEBUG, "%s: set WPA+RSN ie (len %lu)", __func__,
		   (unsigned long)ie_len);
	return bsd_set80211(priv, IEEE80211_IOC_APPIE, IEEE80211_APPIE_WPA,
			    ie, ie_len);
#endif /* IEEE80211_IOC_APPIE */
	return 0;
}

#ifdef SO_RERROR
static void
bsd_route_overflow(int sock, void *ctx, struct bsd_driver_global *global)
{
	char event_buf[2048]; /* max size of a single route(4) msg */
	int n;
	struct ifaddrs *ifaddrs, *ifa;
	struct bsd_driver_data *drv;
	struct sockaddr_dl *sdl;
	union wpa_event_data event;

	/* We need to match the system state, so drain the route
	 * socket to avoid stale messages. */
	do {
		n = read(sock, event_buf, sizeof(event_buf));
	} while (n != -1 || errno == ENOBUFS);

	if (getifaddrs(&ifaddrs) == -1) {
		wpa_printf(MSG_ERROR, "%s getifaddrs() failed: %s",
			   __func__, strerror(errno));
		return;
	}

	/* add or update existing interfaces */
	for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		sdl = (struct sockaddr_dl *) (void *) ifa->ifa_addr;
		drv = bsd_get_drvname(global, ifa->ifa_name);
		if (drv != NULL &&
		    (drv->ifindex != sdl->sdl_index || drv->if_removed)) {
			wpa_printf(MSG_DEBUG,
				   "RTM_IFANNOUNCE: Interface '%s' added",
				   drv->ifname);
			drv->ifindex = sdl->sdl_index;
			drv->if_removed = 0;
			event.interface_status.ievent = EVENT_INTERFACE_ADDED;
			os_strlcpy(event.interface_status.ifname, ifa->ifa_name,
				   sizeof(event.interface_status.ifname));
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS,
					     &event);
		}
		if (!drv &&
		    (drv = bsd_get_drvindex(global, sdl->sdl_index)) != NULL) {
			/* Driver name is invalid */
			wpa_printf(MSG_DEBUG,
				   "RTM_IFANNOUNCE: Interface '%s' removed",
				   drv->ifname);
			drv->if_removed = 1;
			event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
			os_strlcpy(event.interface_status.ifname, drv->ifname,
				   sizeof(event.interface_status.ifname));
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS,
					     &event);
		}
	}

	/* punt missing interfaces and update flags */
	dl_list_for_each(drv, &global->ifaces, struct bsd_driver_data, list) {
		for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL ||
			    ifa->ifa_addr->sa_family != AF_LINK)
				continue;
			sdl = (struct sockaddr_dl *) (void *) ifa->ifa_addr;
			if (os_strcmp(drv->ifname, ifa->ifa_name) == 0)
				break;
		}
		if (ifa == NULL && !drv->if_removed) {
			wpa_printf(MSG_DEBUG,
				   "RTM_IFANNOUNCE: Interface '%s' removed",
				   drv->ifname);
			drv->if_removed = 1;
			event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
			os_strlcpy(event.interface_status.ifname, drv->ifname,
				   sizeof(event.interface_status.ifname));
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS,
					     &event);
		}
		if (!ifa)
			continue;

		if ((ifa->ifa_flags & IFF_UP) == 0 &&
		    (drv->flags & IFF_UP) != 0) {
			wpa_printf(MSG_DEBUG, "RTM_IFINFO: Interface '%s' DOWN",
				   drv->ifname);
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_DISABLED,
					     NULL);
		} else if ((ifa->ifa_flags & IFF_UP) != 0 &&
			   (drv->flags & IFF_UP) == 0) {
			wpa_printf(MSG_DEBUG, "RTM_IFINFO: Interface '%s' UP",
				   drv->ifname);
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_ENABLED,
					     NULL);
		}
		drv->flags = ifa->ifa_flags;
	}

	freeifaddrs(ifaddrs);
}
#endif /* SO_RERROR */

static void
bsd_wireless_event_receive(int sock, void *ctx, void *sock_ctx)
{
	char event_buf[2048]; /* max size of a single route(4) msg */
	struct bsd_driver_global *global = sock_ctx;
	struct bsd_driver_data *drv;
	struct if_announcemsghdr *ifan;
	struct if_msghdr *ifm;
	struct rt_msghdr *rtm;
	union wpa_event_data event;
	struct ieee80211_michael_event *mic;
	struct ieee80211_leave_event *leave;
	struct ieee80211_join_event *join;
	int n;

	n = read(sock, event_buf, sizeof(event_buf));
	if (n < 0) {
		if (errno != EINTR && errno != EAGAIN)
			wpa_printf(MSG_ERROR, "%s read() failed: %s",
				   __func__, strerror(errno));
#ifdef SO_RERROR
		if (errno == ENOBUFS)
			bsd_route_overflow(sock, ctx, sock_ctx);
#endif /* SO_RERROR */
		return;
	}

	rtm = (struct rt_msghdr *) event_buf;
	if (rtm->rtm_version != RTM_VERSION) {
		wpa_printf(MSG_DEBUG, "Invalid routing message version=%d",
			   rtm->rtm_version);
		return;
	}
	os_memset(&event, 0, sizeof(event));
	switch (rtm->rtm_type) {
	case RTM_IEEE80211:
		ifan = (struct if_announcemsghdr *) rtm;
		drv = bsd_get_drvindex(global, ifan->ifan_index);
		if (drv == NULL)
			return;
		switch (ifan->ifan_what) {
		case RTM_IEEE80211_ASSOC:
		case RTM_IEEE80211_REASSOC:
			if (drv->is_ap)
				break;
			wpa_supplicant_event(drv->ctx, EVENT_ASSOC, NULL);
			break;
		case RTM_IEEE80211_DISASSOC:
			if (drv->is_ap)
				break;
			wpa_supplicant_event(drv->ctx, EVENT_DISASSOC, NULL);
			break;
		case RTM_IEEE80211_SCAN:
			if (drv->is_ap)
				break;
			wpa_supplicant_event(drv->ctx, EVENT_SCAN_RESULTS,
					     NULL);
			break;
		case RTM_IEEE80211_LEAVE:
			leave = (struct ieee80211_leave_event *) &ifan[1];
			drv_event_disassoc(drv->ctx, leave->iev_addr);
			break;
		case RTM_IEEE80211_JOIN:
#ifdef RTM_IEEE80211_REJOIN
		case RTM_IEEE80211_REJOIN:
#endif
			join = (struct ieee80211_join_event *) &ifan[1];
			bsd_new_sta(drv, drv->ctx, join->iev_addr);
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
			os_memset(&event, 0, sizeof(event));
			event.michael_mic_failure.unicast =
				!IEEE80211_IS_MULTICAST(mic->iev_dst);
			event.michael_mic_failure.src = mic->iev_src;
			wpa_supplicant_event(drv->ctx,
					     EVENT_MICHAEL_MIC_FAILURE, &event);
			break;
		}
		break;
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *) rtm;
		switch (ifan->ifan_what) {
		case IFAN_DEPARTURE:
			drv = bsd_get_drvindex(global, ifan->ifan_index);
			if (drv)
				drv->if_removed = 1;
			event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
			break;
		case IFAN_ARRIVAL:
			drv = bsd_get_drvname(global, ifan->ifan_name);
			if (drv) {
				drv->ifindex = ifan->ifan_index;
				drv->if_removed = 0;
			}
			event.interface_status.ievent = EVENT_INTERFACE_ADDED;
			break;
		default:
			wpa_printf(MSG_DEBUG, "RTM_IFANNOUNCE: unknown action");
			return;
		}
		wpa_printf(MSG_DEBUG, "RTM_IFANNOUNCE: Interface '%s' %s",
			   ifan->ifan_name,
			   ifan->ifan_what == IFAN_DEPARTURE ?
				"removed" : "added");
		os_strlcpy(event.interface_status.ifname, ifan->ifan_name,
			   sizeof(event.interface_status.ifname));
		if (drv) {
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS,
					     &event);
			/*
			 * Set ifindex to zero after sending the event as the
			 * event might query the driver to ensure a match.
			 */
			if (ifan->ifan_what == IFAN_DEPARTURE)
				drv->ifindex = 0;
		} else {
			wpa_supplicant_event_global(global->ctx,
						    EVENT_INTERFACE_STATUS,
						    &event);
		}
		break;
	case RTM_IFINFO:
		ifm = (struct if_msghdr *) rtm;
		drv = bsd_get_drvindex(global, ifm->ifm_index);
		if (drv == NULL)
			return;
		if (((ifm->ifm_flags & IFF_UP) == 0 ||
		    (ifm->ifm_flags & IFF_RUNNING) == 0) &&
		    (drv->flags & IFF_UP) != 0 &&
		    (drv->flags & IFF_RUNNING) != 0) {
			wpa_printf(MSG_DEBUG, "RTM_IFINFO: Interface '%s' DOWN",
				   drv->ifname);
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_DISABLED,
					     NULL);
		} else if ((ifm->ifm_flags & IFF_UP) != 0 &&
		    (ifm->ifm_flags & IFF_RUNNING) != 0 &&
		    ((drv->flags & IFF_UP) == 0 ||
		    (drv->flags & IFF_RUNNING)  == 0)) {
			wpa_printf(MSG_DEBUG, "RTM_IFINFO: Interface '%s' UP",
				   drv->ifname);
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_ENABLED,
					     NULL);
		}
		drv->flags = ifm->ifm_flags;
		break;
	}
}

#ifdef HOSTAPD

/*
 * Avoid conflicts with hostapd definitions by undefining couple of defines
 * from net80211 header files.
 */
#undef RSN_VERSION
#undef WPA_VERSION
#undef WPA_OUI_TYPE

static int bsd_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
			  u16 reason_code, int link_id);

static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		snprintf(buf, sizeof(buf), MACSTR, 0,0,0,0,0,0);
	return buf;
}

static int
bsd_set_privacy(void *priv, int enabled)
{
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	return set80211param(priv, IEEE80211_IOC_PRIVACY, enabled);
}

static int
bsd_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx,
	       int link_id, u8 *seq)
{
	struct ieee80211req_key wk;

	wpa_printf(MSG_DEBUG, "%s: addr=%s idx=%d",
		   __func__, ether_sprintf(addr), idx);

	memset(&wk, 0, sizeof(wk));
	if (addr == NULL)
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
	else
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	wk.ik_keyix = idx;

	if (get80211var(priv, IEEE80211_IOC_WPAKEY, &wk, sizeof(wk)) < 0) {
		wpa_printf(MSG_INFO, "Failed to get encryption");
		return -1;
	}

#ifdef WORDS_BIGENDIAN
	{
		/*
		 * wk.ik_keytsc is in host byte order (big endian), need to
		 * swap it to match with the byte order used in WPA.
		 */
		int i;
		u8 tmp[WPA_KEY_RSC_LEN];
		memcpy(tmp, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
		for (i = 0; i < WPA_KEY_RSC_LEN; i++) {
			seq[i] = tmp[WPA_KEY_RSC_LEN - i - 1];
		}
	}
#else /* WORDS_BIGENDIAN */
	memcpy(seq, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
#endif /* WORDS_BIGENDIAN */
	return 0;
}


static int
bsd_flush(void *priv, int link_id)
{
	u8 allsta[IEEE80211_ADDR_LEN];

	memset(allsta, 0xff, IEEE80211_ADDR_LEN);
	return bsd_sta_deauth(priv, NULL, allsta, IEEE80211_REASON_AUTH_LEAVE,
			      -1);
}


static int
bsd_read_sta_driver_data(void *priv, struct hostap_sta_driver_data *data,
			 const u8 *addr)
{
	struct ieee80211req_sta_stats stats;

	memcpy(stats.is_u.macaddr, addr, IEEE80211_ADDR_LEN);
	if (get80211var(priv, IEEE80211_IOC_STA_STATS, &stats, sizeof(stats))
	    > 0) {
		/* XXX? do packets counts include non-data frames? */
		data->rx_packets = stats.is_stats.ns_rx_data;
		data->rx_bytes = stats.is_stats.ns_rx_bytes;
		data->tx_packets = stats.is_stats.ns_tx_data;
		data->tx_bytes = stats.is_stats.ns_tx_bytes;
	}
	return 0;
}

static int
bsd_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr, u16 reason_code,
	       int link_id)
{
	return bsd_send_mlme_param(priv, IEEE80211_MLME_DEAUTH, reason_code,
				   addr);
}

static int
bsd_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
		 u16 reason_code)
{
	return bsd_send_mlme_param(priv, IEEE80211_MLME_DISASSOC, reason_code,
				   addr);
}

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct bsd_driver_data *drv = ctx;
	drv_event_eapol_rx(drv->ctx, src_addr, buf, len);
}

static void *
bsd_init(struct hostapd_data *hapd, struct wpa_init_params *params)
{
	struct bsd_driver_data *drv;

	drv = os_zalloc(sizeof(struct bsd_driver_data));
	if (drv == NULL) {
		wpa_printf(MSG_ERROR, "Could not allocate memory for bsd driver data");
		return NULL;
	}

	drv->ifindex = if_nametoindex(params->ifname);
	if (drv->ifindex == 0) {
		wpa_printf(MSG_DEBUG, "%s: interface %s does not exist",
			   __func__, params->ifname);
		goto bad;
	}

	drv->ctx = hapd;
	drv->is_ap = 1;
	drv->global = params->global_priv;
	os_strlcpy(drv->ifname, params->ifname, sizeof(drv->ifname));

	drv->sock_xmit = l2_packet_init(drv->ifname, NULL, ETH_P_EAPOL,
					handle_read, drv, 0);
	if (drv->sock_xmit == NULL)
		goto bad;
	if (l2_packet_get_own_addr(drv->sock_xmit, params->own_addr))
		goto bad;

	/* mark down during setup */
	if (bsd_ctrl_iface(drv, 0) < 0)
		goto bad;

	if (bsd_set_mediaopt(drv, IFM_OMASK, IFM_IEEE80211_HOSTAP) < 0) {
		wpa_printf(MSG_ERROR, "%s: failed to set operation mode",
			   __func__);
		goto bad;
	}

	dl_list_add(&drv->global->ifaces, &drv->list);

	return drv;
bad:
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	os_free(drv);
	return NULL;
}


static void
bsd_deinit(void *priv)
{
	struct bsd_driver_data *drv = priv;

	if (drv->ifindex != 0)
		bsd_ctrl_iface(drv, 0);
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	os_free(drv);
}

static int
bsd_set_sta_authorized(void *priv, const u8 *addr,
		       unsigned int total_flags, unsigned int flags_or,
		       unsigned int flags_and)
{
	int authorized = -1;

	/* For now, only support setting Authorized flag */
	if (flags_or & WPA_STA_AUTHORIZED)
		authorized = 1;
	if (!(flags_and & WPA_STA_AUTHORIZED))
		authorized = 0;

	if (authorized < 0)
		return 0;

	return bsd_send_mlme_param(priv, authorized ?
				   IEEE80211_MLME_AUTHORIZE :
				   IEEE80211_MLME_UNAUTHORIZE, 0, addr);
}
#else /* HOSTAPD */

static int
get80211param(struct bsd_driver_data *drv, int op)
{
	struct ieee80211req ireq;

	if (bsd_get80211(drv, &ireq, op, NULL, 0) < 0)
		return -1;
	return ireq.i_val;
}

static int
wpa_driver_bsd_get_bssid(void *priv, u8 *bssid)
{
	struct bsd_driver_data *drv = priv;
#ifdef SIOCG80211BSSID
	struct ieee80211_bssid bs;

	os_strlcpy(bs.i_name, drv->ifname, sizeof(bs.i_name));
	if (ioctl(drv->global->sock, SIOCG80211BSSID, &bs) < 0)
		return -1;
	os_memcpy(bssid, bs.i_bssid, sizeof(bs.i_bssid));
	return 0;
#else
	return get80211var(drv, IEEE80211_IOC_BSSID,
		bssid, IEEE80211_ADDR_LEN) < 0 ? -1 : 0;
#endif
}

static int
wpa_driver_bsd_get_ssid(void *priv, u8 *ssid)
{
	struct bsd_driver_data *drv = priv;
	return bsd_get_ssid(drv, ssid, 0);
}

static int
wpa_driver_bsd_set_wpa_ie(struct bsd_driver_data *drv, const u8 *wpa_ie,
			  size_t wpa_ie_len)
{
#ifdef IEEE80211_IOC_APPIE
	return bsd_set_opt_ie(drv, wpa_ie, wpa_ie_len);
#else /* IEEE80211_IOC_APPIE */
	return set80211var(drv, IEEE80211_IOC_OPTIE, wpa_ie, wpa_ie_len);
#endif /* IEEE80211_IOC_APPIE */
}

static int
wpa_driver_bsd_set_wpa_internal(void *priv, int wpa, int privacy)
{
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: wpa=%d privacy=%d",
		__func__, wpa, privacy);

	if (!wpa && wpa_driver_bsd_set_wpa_ie(priv, NULL, 0) < 0)
		ret = -1;
	if (set80211param(priv, IEEE80211_IOC_PRIVACY, privacy) < 0)
		ret = -1;
	if (set80211param(priv, IEEE80211_IOC_WPA, wpa) < 0)
		ret = -1;

	return ret;
}

static int
wpa_driver_bsd_set_wpa(void *priv, int enabled)
{
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	return wpa_driver_bsd_set_wpa_internal(priv, enabled ? 3 : 0, enabled);
}

static int
wpa_driver_bsd_set_countermeasures(void *priv, int enabled)
{
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
	return set80211param(priv, IEEE80211_IOC_COUNTERMEASURES, enabled);
}


static int
wpa_driver_bsd_set_drop_unencrypted(void *priv, int enabled)
{
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);
	return set80211param(priv, IEEE80211_IOC_DROPUNENCRYPTED, enabled);
}

static int
wpa_driver_bsd_deauthenticate(void *priv, const u8 *addr, u16 reason_code)
{
	return bsd_send_mlme_param(priv, IEEE80211_MLME_DEAUTH, reason_code,
				   addr);
}

static int
wpa_driver_bsd_set_auth_alg(void *priv, int auth_alg)
{
	int authmode;

	if ((auth_alg & WPA_AUTH_ALG_OPEN) &&
	    (auth_alg & WPA_AUTH_ALG_SHARED))
		authmode = IEEE80211_AUTH_AUTO;
	else if (auth_alg & WPA_AUTH_ALG_SHARED)
		authmode = IEEE80211_AUTH_SHARED;
	else
		authmode = IEEE80211_AUTH_OPEN;

	return set80211param(priv, IEEE80211_IOC_AUTHMODE, authmode);
}

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct bsd_driver_data *drv = ctx;

	drv_event_eapol_rx(drv->ctx, src_addr, buf, len);
}

static int
wpa_driver_bsd_set_rsn_wpa_ie(struct bsd_driver_data * drv,
    struct wpa_driver_associate_params *params, const u8 *ie)
{
	int privacy;
	size_t ie_len = ie[1] ? ie[1] + 2 : 0;

	/* XXX error handling is wrong but unclear what to do... */
	if (wpa_driver_bsd_set_wpa_ie(drv, ie, ie_len) < 0)
		return -1;

	privacy = !(params->pairwise_suite == WPA_CIPHER_NONE &&
	    params->group_suite == WPA_CIPHER_NONE &&
	    params->key_mgmt_suite == WPA_KEY_MGMT_NONE);
	wpa_printf(MSG_DEBUG, "%s: set PRIVACY %u", __func__,
	    privacy);

	if (set80211param(drv, IEEE80211_IOC_PRIVACY, privacy) < 0)
		return -1;

	if (ie_len &&
	    set80211param(drv, IEEE80211_IOC_WPA,
	    ie[0] == WLAN_EID_RSN ? 2 : 1) < 0)
		return -1;

	return 0;
}

static int
wpa_driver_bsd_associate(void *priv, struct wpa_driver_associate_params *params)
{
	struct bsd_driver_data *drv = priv;
	struct ieee80211req_mlme mlme;
	u32 mode;
	int ret = 0;
	const u8 *wpa_ie, *rsn_ie;

	wpa_printf(MSG_DEBUG,
		"%s: ssid '%.*s' wpa ie len %u pairwise %u group %u key mgmt %u"
		, __func__
		   , (unsigned int) params->ssid_len, params->ssid
		, (unsigned int) params->wpa_ie_len
		, params->pairwise_suite
		, params->group_suite
		, params->key_mgmt_suite
	);

	switch (params->mode) {
	case IEEE80211_MODE_INFRA:
		mode = 0 /* STA */;
		break;
	case IEEE80211_MODE_IBSS:
#if 0
		mode = IFM_IEEE80211_IBSS;
#endif
		mode = IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_MODE_AP:
		mode = IFM_IEEE80211_HOSTAP;
		break;
	default:
		wpa_printf(MSG_ERROR, "%s: unknown operation mode", __func__);
		return -1;
	}
	if (bsd_set_mediaopt(drv, IFM_OMASK, mode) < 0) {
		wpa_printf(MSG_ERROR, "%s: failed to set operation mode",
			   __func__);
		return -1;
	}

	if (params->mode == IEEE80211_MODE_AP) {
		drv->sock_xmit = l2_packet_init(drv->ifname, NULL, ETH_P_EAPOL,
						handle_read, drv, 0);
		if (drv->sock_xmit == NULL)
			return -1;
		drv->is_ap = 1;
		return 0;
	}

	if (wpa_driver_bsd_set_drop_unencrypted(drv, params->drop_unencrypted)
	    < 0)
		ret = -1;
	if (wpa_driver_bsd_set_auth_alg(drv, params->auth_alg) < 0)
		ret = -1;

	if (params->wpa_ie_len) {
		rsn_ie = get_ie(params->wpa_ie, params->wpa_ie_len,
		    WLAN_EID_RSN);
		if (rsn_ie) {
			if (wpa_driver_bsd_set_rsn_wpa_ie(drv, params,
			    rsn_ie) < 0)
				return -1;
		}
		else {
			wpa_ie = get_vendor_ie(params->wpa_ie,
			    params->wpa_ie_len, WPA_IE_VENDOR_TYPE);
			if (wpa_ie) {
				if (wpa_driver_bsd_set_rsn_wpa_ie(drv, params,
				    wpa_ie) < 0)
					return -1;
			}
		}
	}

	/*
	 * NB: interface must be marked UP for association
	 * or scanning (ap_scan=2)
	 */
	if (bsd_ctrl_iface(drv, 1) < 0)
		return -1;

	os_memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_ASSOC;
	if (params->ssid != NULL)
		os_memcpy(mlme.im_ssid, params->ssid, params->ssid_len);
	mlme.im_ssid_len = params->ssid_len;
	if (params->bssid != NULL)
		os_memcpy(mlme.im_macaddr, params->bssid, IEEE80211_ADDR_LEN);
	if (set80211var(drv, IEEE80211_IOC_MLME, &mlme, sizeof(mlme)) < 0)
		return -1;
	return ret;
}

static int
wpa_driver_bsd_scan(void *priv, struct wpa_driver_scan_params *params)
{
	struct bsd_driver_data *drv = priv;
#ifdef IEEE80211_IOC_SCAN_MAX_SSID
	struct ieee80211_scan_req sr;
	int i;
#endif /* IEEE80211_IOC_SCAN_MAX_SSID */

	if (bsd_set_mediaopt(drv, IFM_OMASK, 0 /* STA */) < 0) {
		wpa_printf(MSG_ERROR, "%s: failed to set operation mode",
			   __func__);
		return -1;
	}

	if (set80211param(drv, IEEE80211_IOC_ROAMING,
			  IEEE80211_ROAMING_MANUAL) < 0) {
		wpa_printf(MSG_ERROR, "%s: failed to set "
			   "wpa_supplicant-based roaming: %s", __func__,
			   strerror(errno));
		return -1;
	}

	if (wpa_driver_bsd_set_wpa(drv, 1) < 0) {
		wpa_printf(MSG_ERROR, "%s: failed to set wpa: %s", __func__,
			   strerror(errno));
		return -1;
	}

	/* NB: interface must be marked UP to do a scan */
	if (bsd_ctrl_iface(drv, 1) < 0)
		return -1;

#ifdef IEEE80211_IOC_SCAN_MAX_SSID
	os_memset(&sr, 0, sizeof(sr));
	sr.sr_flags = IEEE80211_IOC_SCAN_ACTIVE | IEEE80211_IOC_SCAN_ONCE |
		IEEE80211_IOC_SCAN_NOJOIN;
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
#else /* IEEE80211_IOC_SCAN_MAX_SSID */
	/* set desired ssid before scan */
	if (bsd_set_ssid(drv, params->ssids[0].ssid,
			 params->ssids[0].ssid_len) < 0)
		return -1;

	/* NB: net80211 delivers a scan complete event so no need to poll */
	return set80211param(drv, IEEE80211_IOC_SCAN_REQ, 0);
#endif /* IEEE80211_IOC_SCAN_MAX_SSID */
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

#ifdef __FreeBSD__
	/*
	 * the rssi value reported by the kernel is in 0.5dB steps relative to
	 * the reported noise floor. see ieee80211_node.h for details.
	 */
	result->level = sr->isr_rssi / 2 + sr->isr_noise;
#else
	result->level = sr->isr_rssi;
#endif

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

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	os_memcpy(pos, (u8 *)(sr + 1) + sr->isr_ssid_len + sr->isr_meshid_len,
		  sr->isr_ie_len);
#else
	os_memcpy(pos, (u8 *)(sr + 1) + sr->isr_ssid_len, sr->isr_ie_len);
#endif
	pos += sr->isr_ie_len;

	result->ie_len = pos - (u8 *)(result + 1);

	tmp = os_realloc_array(res->res, res->num + 1,
			       sizeof(struct wpa_scan_res *));
	if (tmp == NULL) {
		os_free(result);
		return;
	}
	tmp[res->num++] = result;
	res->res = tmp;
}

struct wpa_scan_results *
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

	return res;
}

static int wpa_driver_bsd_capa(struct bsd_driver_data *drv)
{
#ifdef IEEE80211_IOC_DEVCAPS
/* kernel definitions copied from net80211/ieee80211_var.h */
#define IEEE80211_CIPHER_WEP            0
#define IEEE80211_CIPHER_TKIP           1
#define IEEE80211_CIPHER_AES_CCM        3
#define IEEE80211_CRYPTO_WEP            (1<<IEEE80211_CIPHER_WEP)
#define IEEE80211_CRYPTO_TKIP           (1<<IEEE80211_CIPHER_TKIP)
#define IEEE80211_CRYPTO_AES_CCM        (1<<IEEE80211_CIPHER_AES_CCM)
#define IEEE80211_C_HOSTAP      0x00000400      /* CAPABILITY: HOSTAP avail */
#define IEEE80211_C_WPA1        0x00800000      /* CAPABILITY: WPA1 avail */
#define IEEE80211_C_WPA2        0x01000000      /* CAPABILITY: WPA2 avail */
	struct ieee80211_devcaps_req devcaps;

	if (get80211var(drv, IEEE80211_IOC_DEVCAPS, &devcaps,
			sizeof(devcaps)) < 0) {
		wpa_printf(MSG_ERROR, "failed to IEEE80211_IOC_DEVCAPS: %s",
			   strerror(errno));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: drivercaps=0x%08x,cryptocaps=0x%08x",
		   __func__, devcaps.dc_drivercaps, devcaps.dc_cryptocaps);

	if (devcaps.dc_drivercaps & IEEE80211_C_WPA1)
		drv->capa.key_mgmt = WPA_DRIVER_CAPA_KEY_MGMT_WPA |
			WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK;
	if (devcaps.dc_drivercaps & IEEE80211_C_WPA2)
		drv->capa.key_mgmt = WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
			WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;

	if (devcaps.dc_cryptocaps & IEEE80211_CRYPTO_WEP)
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_WEP40 |
			WPA_DRIVER_CAPA_ENC_WEP104;
	if (devcaps.dc_cryptocaps & IEEE80211_CRYPTO_TKIP)
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_TKIP;
	if (devcaps.dc_cryptocaps & IEEE80211_CRYPTO_AES_CCM)
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_CCMP;
	if (devcaps.dc_cryptocaps & IEEE80211_CRYPTO_AES_CCM_256)
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_CCMP_256;
	if (devcaps.dc_cryptocaps & IEEE80211_CRYPTO_AES_GCM_128)
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_GCMP;
	if (devcaps.dc_cryptocaps & IEEE80211_CRYPTO_AES_GCM_256)
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_GCMP_256;
	if (devcaps.dc_cryptocaps & IEEE80211_CRYPTO_BIP_CMAC_128)
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_BIP;

	if (devcaps.dc_drivercaps & IEEE80211_C_HOSTAP)
		drv->capa.flags |= WPA_DRIVER_FLAGS_AP;
#undef IEEE80211_CIPHER_WEP
#undef IEEE80211_CIPHER_TKIP
#undef IEEE80211_CIPHER_AES_CCM
#undef IEEE80211_CRYPTO_WEP
#undef IEEE80211_CRYPTO_TKIP
#undef IEEE80211_CRYPTO_AES_CCM
#undef IEEE80211_C_HOSTAP
#undef IEEE80211_C_WPA1
#undef IEEE80211_C_WPA2
#else /* IEEE80211_IOC_DEVCAPS */
	/* For now, assume TKIP, CCMP, WPA, WPA2 are supported */
	drv->capa.key_mgmt = WPA_DRIVER_CAPA_KEY_MGMT_WPA |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;
	drv->capa.enc = WPA_DRIVER_CAPA_ENC_WEP40 |
		WPA_DRIVER_CAPA_ENC_WEP104 |
		WPA_DRIVER_CAPA_ENC_TKIP |
		WPA_DRIVER_CAPA_ENC_CCMP;
	drv->capa.flags |= WPA_DRIVER_FLAGS_AP;
#endif /* IEEE80211_IOC_DEVCAPS */
#ifdef IEEE80211_IOC_SCAN_MAX_SSID
	drv->capa.max_scan_ssids = IEEE80211_IOC_SCAN_MAX_SSID;
#else /* IEEE80211_IOC_SCAN_MAX_SSID */
	drv->capa.max_scan_ssids = 1;
#endif /* IEEE80211_IOC_SCAN_MAX_SSID */
	drv->capa.auth = WPA_DRIVER_AUTH_OPEN |
		WPA_DRIVER_AUTH_SHARED |
		WPA_DRIVER_AUTH_LEAP;
	return 0;
}

static enum ieee80211_opmode
get80211opmode(struct bsd_driver_data *drv)
{
	struct ifmediareq ifmr;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) os_strlcpy(ifmr.ifm_name, drv->ifname, sizeof(ifmr.ifm_name));

	if (ioctl(drv->global->sock, SIOCGIFMEDIA, (caddr_t)&ifmr) >= 0) {
		if (ifmr.ifm_current & IFM_IEEE80211_ADHOC) {
			if (ifmr.ifm_current & IFM_FLAG0)
				return IEEE80211_M_AHDEMO;
			else
				return IEEE80211_M_IBSS;
		}
		if (ifmr.ifm_current & IFM_IEEE80211_HOSTAP)
			return IEEE80211_M_HOSTAP;
		if (ifmr.ifm_current & IFM_IEEE80211_IBSS)
			return IEEE80211_M_IBSS;
		if (ifmr.ifm_current & IFM_IEEE80211_MONITOR)
			return IEEE80211_M_MONITOR;
#ifdef IEEE80211_M_MBSS
		if (ifmr.ifm_current & IFM_IEEE80211_MBSS)
			return IEEE80211_M_MBSS;
#endif /* IEEE80211_M_MBSS */
	}
	return IEEE80211_M_STA;
}

static void *
wpa_driver_bsd_init(void *ctx, const char *ifname, void *priv)
{
#define	GETPARAM(drv, param, v) \
	(((v) = get80211param(drv, param)) != -1)
	struct bsd_driver_data *drv;
	int i;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;

	drv->ifindex = if_nametoindex(ifname);
	if (drv->ifindex == 0) {
		wpa_printf(MSG_DEBUG, "%s: interface %s does not exist",
			   __func__, ifname);
		goto fail;
	}

	drv->ctx = ctx;
	drv->global = priv;
	os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));

	/* Set the interface as removed until proven to work. */
	drv->if_removed = 1;

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

	if (wpa_driver_bsd_capa(drv))
		goto fail;

	/* Update per interface supported AKMs */
	for (i = 0; i < WPA_IF_MAX; i++)
		drv->capa.key_mgmt_iftype[i] = drv->capa.key_mgmt;

	/* Down interface during setup. */
	if (bsd_ctrl_iface(drv, 0) < 0)
		goto fail;

	/* Proven to work, lets go! */
	drv->if_removed = 0;

	drv->opmode = get80211opmode(drv);
	dl_list_add(&drv->global->ifaces, &drv->list);

	return drv;
fail:
	os_free(drv);
	return NULL;
#undef GETPARAM
}

static void
wpa_driver_bsd_deinit(void *priv)
{
	struct bsd_driver_data *drv = priv;

	if (drv->ifindex != 0 && !drv->if_removed) {
		wpa_driver_bsd_set_wpa(drv, 0);

		/* NB: mark interface down */
		bsd_ctrl_iface(drv, 0);

		wpa_driver_bsd_set_wpa_internal(drv, drv->prev_wpa,
						drv->prev_privacy);

		if (set80211param(drv, IEEE80211_IOC_ROAMING, drv->prev_roaming)
		    < 0)
			wpa_printf(MSG_DEBUG,
				   "%s: failed to restore roaming state",
				   __func__);
	}

	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	dl_list_del(&drv->list);
	os_free(drv);
}

static int
wpa_driver_bsd_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	struct bsd_driver_data *drv = priv;

	os_memcpy(capa, &drv->capa, sizeof(*capa));
	return 0;
}
#endif /* HOSTAPD */

static void *
bsd_global_init(void *ctx)
{
	struct bsd_driver_global *global;
#if defined(RO_MSGFILTER) || defined(ROUTE_MSGFILTER)
	unsigned char msgfilter[] = {
		RTM_IEEE80211,
		RTM_IFINFO, RTM_IFANNOUNCE,
	};
#endif
#ifdef ROUTE_MSGFILTER
	unsigned int i, msgfilter_mask;
#endif

	global = os_zalloc(sizeof(*global));
	if (global == NULL)
		return NULL;

	global->ctx = ctx;
	dl_list_init(&global->ifaces);

	global->sock = socket(PF_LOCAL, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (global->sock < 0) {
		wpa_printf(MSG_ERROR, "socket[PF_LOCAL,SOCK_DGRAM]: %s",
			   strerror(errno));
		goto fail1;
	}

	global->route = socket(PF_ROUTE,
			       SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (global->route < 0) {
		wpa_printf(MSG_ERROR, "socket[PF_ROUTE,SOCK_RAW]: %s",
			   strerror(errno));
		goto fail;
	}

#if defined(RO_MSGFILTER)
	if (setsockopt(global->route, PF_ROUTE, RO_MSGFILTER,
	    &msgfilter, sizeof(msgfilter)) < 0)
		wpa_printf(MSG_ERROR, "socket[PF_ROUTE,RO_MSGFILTER]: %s",
			   strerror(errno));
#elif defined(ROUTE_MSGFILTER)
	msgfilter_mask = 0;
	for (i = 0; i < (sizeof(msgfilter) / sizeof(msgfilter[0])); i++)
		msgfilter_mask |= ROUTE_FILTER(msgfilter[i]);
	if (setsockopt(global->route, PF_ROUTE, ROUTE_MSGFILTER,
	    &msgfilter_mask, sizeof(msgfilter_mask)) < 0)
		wpa_printf(MSG_ERROR, "socket[PF_ROUTE,ROUTE_MSGFILTER]: %s",
			   strerror(errno));
#endif

	eloop_register_read_sock(global->route, bsd_wireless_event_receive,
				 NULL, global);

	return global;

fail:
	close(global->sock);
fail1:
	os_free(global);
	return NULL;
}

static void
bsd_global_deinit(void *priv)
{
	struct bsd_driver_global *global = priv;

	eloop_unregister_read_sock(global->route);
	(void) close(global->route);
	(void) close(global->sock);
	os_free(global);
}


const struct wpa_driver_ops wpa_driver_bsd_ops = {
	.name			= "bsd",
	.desc			= "BSD 802.11 support",
	.global_init		= bsd_global_init,
	.global_deinit		= bsd_global_deinit,
#ifdef HOSTAPD
	.hapd_init		= bsd_init,
	.hapd_deinit		= bsd_deinit,
	.set_privacy		= bsd_set_privacy,
	.get_seqnum		= bsd_get_seqnum,
	.flush			= bsd_flush,
	.read_sta_data		= bsd_read_sta_driver_data,
	.sta_disassoc		= bsd_sta_disassoc,
	.sta_deauth		= bsd_sta_deauth,
	.sta_set_flags		= bsd_set_sta_authorized,
#else /* HOSTAPD */
	.init2			= wpa_driver_bsd_init,
	.deinit			= wpa_driver_bsd_deinit,
	.get_bssid		= wpa_driver_bsd_get_bssid,
	.get_ssid		= wpa_driver_bsd_get_ssid,
	.set_countermeasures	= wpa_driver_bsd_set_countermeasures,
	.scan2			= wpa_driver_bsd_scan,
	.get_scan_results2	= wpa_driver_bsd_get_scan_results2,
	.deauthenticate		= wpa_driver_bsd_deauthenticate,
	.associate		= wpa_driver_bsd_associate,
	.get_capa		= wpa_driver_bsd_get_capa,
#endif /* HOSTAPD */
	.set_freq		= bsd_set_freq,
	.set_key		= bsd_set_key,
	.set_ieee8021x		= bsd_set_ieee8021x,
	.hapd_set_ssid		= bsd_set_ssid,
	.hapd_get_ssid		= bsd_get_ssid,
	.hapd_send_eapol	= bsd_send_eapol,
	.set_generic_elem	= bsd_set_opt_ie,
};
