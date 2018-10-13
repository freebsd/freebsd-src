/*
 * Wired Ethernet driver interface for QCA MACsec driver
 * Copyright (c) 2005-2009, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2004, Gunter Burchardt <tira@isx.de>
 * Copyright (c) 2013-2014, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <inttypes.h>
#ifdef __linux__
#include <netpacket/packet.h>
#include <net/if_arp.h>
#include <net/if.h>
#endif /* __linux__ */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#include <net/if_dl.h>
#include <net/if_media.h>
#endif /* defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__) */
#ifdef __sun__
#include <sys/sockio.h>
#endif /* __sun__ */

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/defs.h"
#include "common/ieee802_1x_defs.h"
#include "driver.h"

#include "nss_macsec_secy.h"
#include "nss_macsec_secy_rx.h"
#include "nss_macsec_secy_tx.h"

#define MAXSC 16

/* TCI field definition */
#define TCI_ES                0x40
#define TCI_SC                0x20
#define TCI_SCB               0x10
#define TCI_E                 0x08
#define TCI_C                 0x04

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

static const u8 pae_group_addr[ETH_ALEN] =
{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03 };

struct macsec_qca_data {
	char ifname[IFNAMSIZ + 1];
	u32 secy_id;
	void *ctx;

	int sock; /* raw packet socket for driver access */
	int pf_sock;
	int membership, multi, iff_allmulti, iff_up;

	/* shadow */
	Boolean always_include_sci;
	Boolean use_es;
	Boolean use_scb;
	Boolean protect_frames;
	Boolean replay_protect;
	u32 replay_window;
};


static int macsec_qca_multicast_membership(int sock, int ifindex,
					   const u8 *addr, int add)
{
#ifdef __linux__
	struct packet_mreq mreq;

	if (sock < 0)
		return -1;

	os_memset(&mreq, 0, sizeof(mreq));
	mreq.mr_ifindex = ifindex;
	mreq.mr_type = PACKET_MR_MULTICAST;
	mreq.mr_alen = ETH_ALEN;
	os_memcpy(mreq.mr_address, addr, ETH_ALEN);

	if (setsockopt(sock, SOL_PACKET,
		       add ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP,
		       &mreq, sizeof(mreq)) < 0) {
		wpa_printf(MSG_ERROR, "setsockopt: %s", strerror(errno));
		return -1;
	}
	return 0;
#else /* __linux__ */
	return -1;
#endif /* __linux__ */
}


static int macsec_qca_get_ssid(void *priv, u8 *ssid)
{
	ssid[0] = 0;
	return 0;
}


static int macsec_qca_get_bssid(void *priv, u8 *bssid)
{
	/* Report PAE group address as the "BSSID" for macsec connection. */
	os_memcpy(bssid, pae_group_addr, ETH_ALEN);
	return 0;
}


static int macsec_qca_get_capa(void *priv, struct wpa_driver_capa *capa)
{
	os_memset(capa, 0, sizeof(*capa));
	capa->flags = WPA_DRIVER_FLAGS_WIRED;
	return 0;
}


static int macsec_qca_get_ifflags(const char *ifname, int *flags)
{
	struct ifreq ifr;
	int s;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket: %s", strerror(errno));
		return -1;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCGIFFLAGS]: %s",
			   strerror(errno));
		close(s);
		return -1;
	}
	close(s);
	*flags = ifr.ifr_flags & 0xffff;
	return 0;
}


static int macsec_qca_set_ifflags(const char *ifname, int flags)
{
	struct ifreq ifr;
	int s;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket: %s", strerror(errno));
		return -1;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_flags = flags & 0xffff;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCSIFFLAGS]: %s",
			   strerror(errno));
		close(s);
		return -1;
	}
	close(s);
	return 0;
}


#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
static int macsec_qca_get_ifstatus(const char *ifname, int *status)
{
	struct ifmediareq ifmr;
	int s;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_print(MSG_ERROR, "socket: %s", strerror(errno));
		return -1;
	}

	os_memset(&ifmr, 0, sizeof(ifmr));
	os_strlcpy(ifmr.ifm_name, ifname, IFNAMSIZ);
	if (ioctl(s, SIOCGIFMEDIA, (caddr_t) &ifmr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCGIFMEDIA]: %s",
			   strerror(errno));
		close(s);
		return -1;
	}
	close(s);
	*status = (ifmr.ifm_status & (IFM_ACTIVE | IFM_AVALID)) ==
		(IFM_ACTIVE | IFM_AVALID);

	return 0;
}
#endif /* defined(__FreeBSD__) || defined(__DragonFly__) || defined(FreeBSD_kernel__) */


static int macsec_qca_multi(const char *ifname, const u8 *addr, int add)
{
	struct ifreq ifr;
	int s;

#ifdef __sun__
	return -1;
#endif /* __sun__ */

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket: %s", strerror(errno));
		return -1;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
#ifdef __linux__
	ifr.ifr_hwaddr.sa_family = AF_UNSPEC;
	os_memcpy(ifr.ifr_hwaddr.sa_data, addr, ETH_ALEN);
#endif /* __linux__ */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
	{
		struct sockaddr_dl *dlp;
		dlp = (struct sockaddr_dl *) &ifr.ifr_addr;
		dlp->sdl_len = sizeof(struct sockaddr_dl);
		dlp->sdl_family = AF_LINK;
		dlp->sdl_index = 0;
		dlp->sdl_nlen = 0;
		dlp->sdl_alen = ETH_ALEN;
		dlp->sdl_slen = 0;
		os_memcpy(LLADDR(dlp), addr, ETH_ALEN);
	}
#endif /* defined(__FreeBSD__) || defined(__DragonFly__) || defined(FreeBSD_kernel__) */
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
	{
		struct sockaddr *sap;
		sap = (struct sockaddr *) &ifr.ifr_addr;
		sap->sa_len = sizeof(struct sockaddr);
		sap->sa_family = AF_UNSPEC;
		os_memcpy(sap->sa_data, addr, ETH_ALEN);
	}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) || defined(__APPLE__) */

	if (ioctl(s, add ? SIOCADDMULTI : SIOCDELMULTI, (caddr_t) &ifr) < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOC{ADD/DEL}MULTI]: %s",
			   strerror(errno));
		close(s);
		return -1;
	}
	close(s);
	return 0;
}


static void __macsec_drv_init(struct macsec_qca_data *drv)
{
	int ret = 0;
	fal_rx_ctl_filt_t rx_ctl_filt;
	fal_tx_ctl_filt_t tx_ctl_filt;

	wpa_printf(MSG_INFO, "%s: secy_id=%d", __func__, drv->secy_id);

	/* Enable Secy and Let EAPoL bypass */
	ret = nss_macsec_secy_en_set(drv->secy_id, TRUE);
	if (ret)
		wpa_printf(MSG_ERROR, "nss_macsec_secy_en_set: FAIL");

	ret = nss_macsec_secy_sc_sa_mapping_mode_set(drv->secy_id,
						     FAL_SC_SA_MAP_1_4);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "nss_macsec_secy_sc_sa_mapping_mode_set: FAIL");

	os_memset(&rx_ctl_filt, 0, sizeof(rx_ctl_filt));
	rx_ctl_filt.bypass = 1;
	rx_ctl_filt.match_type = IG_CTL_COMPARE_ETHER_TYPE;
	rx_ctl_filt.match_mask = 0xffff;
	rx_ctl_filt.ether_type_da_range = 0x888e;
	ret = nss_macsec_secy_rx_ctl_filt_set(drv->secy_id, 0, &rx_ctl_filt);
	if (ret)
		wpa_printf(MSG_ERROR, "nss_macsec_secy_rx_ctl_filt_set: FAIL");

	os_memset(&tx_ctl_filt, 0, sizeof(tx_ctl_filt));
	tx_ctl_filt.bypass = 1;
	tx_ctl_filt.match_type = EG_CTL_COMPARE_ETHER_TYPE;
	tx_ctl_filt.match_mask = 0xffff;
	tx_ctl_filt.ether_type_da_range = 0x888e;
	ret = nss_macsec_secy_tx_ctl_filt_set(drv->secy_id, 0, &tx_ctl_filt);
	if (ret)
		wpa_printf(MSG_ERROR, "nss_macsec_secy_tx_ctl_filt_set: FAIL");
}


static void __macsec_drv_deinit(struct macsec_qca_data *drv)
{
	nss_macsec_secy_en_set(drv->secy_id, FALSE);
	nss_macsec_secy_rx_sc_del_all(drv->secy_id);
	nss_macsec_secy_tx_sc_del_all(drv->secy_id);
}


static void * macsec_qca_init(void *ctx, const char *ifname)
{
	struct macsec_qca_data *drv;
	int flags;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));
	drv->ctx = ctx;

	/* Board specific settings */
	if (os_memcmp("eth2", drv->ifname, 4) == 0)
		drv->secy_id = 1;
	else if (os_memcmp("eth3", drv->ifname, 4) == 0)
		drv->secy_id = 2;
	else
		drv->secy_id = -1;

#ifdef __linux__
	drv->pf_sock = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (drv->pf_sock < 0)
		wpa_printf(MSG_ERROR, "socket(PF_PACKET): %s", strerror(errno));
#else /* __linux__ */
	drv->pf_sock = -1;
#endif /* __linux__ */

	if (macsec_qca_get_ifflags(ifname, &flags) == 0 &&
	    !(flags & IFF_UP) &&
	    macsec_qca_set_ifflags(ifname, flags | IFF_UP) == 0) {
		drv->iff_up = 1;
	}

	if (macsec_qca_multicast_membership(drv->pf_sock,
					    if_nametoindex(drv->ifname),
					    pae_group_addr, 1) == 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: Added multicast membership with packet socket",
			   __func__);
		drv->membership = 1;
	} else if (macsec_qca_multi(ifname, pae_group_addr, 1) == 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: Added multicast membership with SIOCADDMULTI",
			   __func__);
		drv->multi = 1;
	} else if (macsec_qca_get_ifflags(ifname, &flags) < 0) {
		wpa_printf(MSG_INFO, "%s: Could not get interface flags",
			   __func__);
		os_free(drv);
		return NULL;
	} else if (flags & IFF_ALLMULTI) {
		wpa_printf(MSG_DEBUG,
			   "%s: Interface is already configured for multicast",
			   __func__);
	} else if (macsec_qca_set_ifflags(ifname, flags | IFF_ALLMULTI) < 0) {
		wpa_printf(MSG_INFO, "%s: Failed to enable allmulti",
			   __func__);
		os_free(drv);
		return NULL;
	} else {
		wpa_printf(MSG_DEBUG, "%s: Enabled allmulti mode", __func__);
		drv->iff_allmulti = 1;
	}
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
	{
		int status;
		wpa_printf(MSG_DEBUG, "%s: waiting for link to become active",
			   __func__);
		while (macsec_qca_get_ifstatus(ifname, &status) == 0 &&
		       status == 0)
			sleep(1);
	}
#endif /* defined(__FreeBSD__) || defined(__DragonFly__) || defined(FreeBSD_kernel__) */

	return drv;
}


static void macsec_qca_deinit(void *priv)
{
	struct macsec_qca_data *drv = priv;
	int flags;

	if (drv->membership &&
	    macsec_qca_multicast_membership(drv->pf_sock,
					    if_nametoindex(drv->ifname),
					    pae_group_addr, 0) < 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: Failed to remove PAE multicast group (PACKET)",
			   __func__);
	}

	if (drv->multi &&
	    macsec_qca_multi(drv->ifname, pae_group_addr, 0) < 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: Failed to remove PAE multicast group (SIOCDELMULTI)",
			   __func__);
	}

	if (drv->iff_allmulti &&
	    (macsec_qca_get_ifflags(drv->ifname, &flags) < 0 ||
	     macsec_qca_set_ifflags(drv->ifname, flags & ~IFF_ALLMULTI) < 0)) {
		wpa_printf(MSG_DEBUG, "%s: Failed to disable allmulti mode",
			   __func__);
	}

	if (drv->iff_up &&
	    macsec_qca_get_ifflags(drv->ifname, &flags) == 0 &&
	    (flags & IFF_UP) &&
	    macsec_qca_set_ifflags(drv->ifname, flags & ~IFF_UP) < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to set the interface down",
			   __func__);
	}

	if (drv->pf_sock != -1)
		close(drv->pf_sock);

	os_free(drv);
}


static int macsec_qca_macsec_init(void *priv, struct macsec_init_params *params)
{
	struct macsec_qca_data *drv = priv;

	drv->always_include_sci = params->always_include_sci;
	drv->use_es = params->use_es;
	drv->use_scb = params->use_scb;

	wpa_printf(MSG_DEBUG, "%s: es=%d, scb=%d, sci=%d",
		   __func__, drv->use_es, drv->use_scb,
		   drv->always_include_sci);

	__macsec_drv_init(drv);

	return 0;
}


static int macsec_qca_macsec_deinit(void *priv)
{
	struct macsec_qca_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s", __func__);

	__macsec_drv_deinit(drv);

	return 0;
}


static int macsec_qca_enable_protect_frames(void *priv, Boolean enabled)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	drv->protect_frames = enabled;

	return ret;
}


static int macsec_qca_set_replay_protect(void *priv, Boolean enabled,
					 unsigned int window)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d, win=%u",
		   __func__, enabled, window);

	drv->replay_protect = enabled;
	drv->replay_window = window;

	return ret;
}


static int macsec_qca_set_current_cipher_suite(void *priv, u64 cs)
{
	if (cs != CS_ID_GCM_AES_128) {
		wpa_printf(MSG_ERROR,
			   "%s: NOT supported CipherSuite: %016" PRIx64,
			   __func__, cs);
		return -1;
	}

	/* Support default Cipher Suite 0080020001000001 (GCM-AES-128) */
	wpa_printf(MSG_DEBUG, "%s: default support aes-gcm-128", __func__);

	return 0;
}


static int macsec_qca_enable_controlled_port(void *priv, Boolean enabled)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enable=%d", __func__, enabled);

	ret += nss_macsec_secy_controlled_port_en_set(drv->secy_id, enabled);

	return ret;
}


static int macsec_qca_get_receive_lowest_pn(void *priv, u32 channel, u8 an,
					    u32 *lowest_pn)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 next_pn = 0;
	bool enabled = FALSE;
	u32 win;

	ret += nss_macsec_secy_rx_sa_next_pn_get(drv->secy_id, channel, an,
						 &next_pn);
	ret += nss_macsec_secy_rx_sc_replay_protect_get(drv->secy_id, channel,
							&enabled);
	ret += nss_macsec_secy_rx_sc_anti_replay_window_get(drv->secy_id,
							    channel, &win);

	if (enabled)
		*lowest_pn = (next_pn > win) ? (next_pn - win) : 1;
	else
		*lowest_pn = next_pn;

	wpa_printf(MSG_DEBUG, "%s: lpn=0x%x", __func__, *lowest_pn);

	return ret;
}


static int macsec_qca_get_transmit_next_pn(void *priv, u32 channel, u8 an,
					   u32 *next_pn)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	ret += nss_macsec_secy_tx_sa_next_pn_get(drv->secy_id, channel, an,
						 next_pn);

	wpa_printf(MSG_DEBUG, "%s: npn=0x%x", __func__, *next_pn);

	return ret;
}


int macsec_qca_set_transmit_next_pn(void *priv, u32 channel, u8 an, u32 next_pn)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	ret += nss_macsec_secy_tx_sa_next_pn_set(drv->secy_id, channel, an,
						 next_pn);

	wpa_printf(MSG_INFO, "%s: npn=0x%x", __func__, next_pn);

	return ret;
}


static int macsec_qca_get_available_receive_sc(void *priv, u32 *channel)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 sc_ch = 0;
	bool in_use = FALSE;

	for (sc_ch = 0; sc_ch < MAXSC; sc_ch++) {
		ret = nss_macsec_secy_rx_sc_in_used_get(drv->secy_id, sc_ch,
							&in_use);
		if (ret)
			continue;

		if (!in_use) {
			*channel = sc_ch;
			wpa_printf(MSG_DEBUG, "%s: channel=%d",
				   __func__, *channel);
			return 0;
		}
	}

	wpa_printf(MSG_DEBUG, "%s: no available channel", __func__);

	return -1;
}


static int macsec_qca_create_receive_sc(void *priv, u32 channel,
					const u8 *sci_addr, u16 sci_port,
					unsigned int conf_offset,
					int validation)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	fal_rx_prc_lut_t entry;
	fal_rx_sc_validate_frame_e vf;
	enum validate_frames validate_frames = validation;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* rx prc lut */
	os_memset(&entry, 0, sizeof(entry));

	os_memcpy(entry.sci, sci_addr, ETH_ALEN);
	entry.sci[6] = (sci_port >> 8) & 0xf;
	entry.sci[7] = sci_port & 0xf;
	entry.sci_mask = 0xf;

	entry.valid = 1;
	entry.channel = channel;
	entry.action = FAL_RX_PRC_ACTION_PROCESS;
	entry.offset = conf_offset;

	/* rx validate frame  */
	if (validate_frames == Strict)
		vf = FAL_RX_SC_VALIDATE_FRAME_STRICT;
	else if (validate_frames == Checked)
		vf = FAL_RX_SC_VALIDATE_FRAME_CHECK;
	else
		vf = FAL_RX_SC_VALIDATE_FRAME_DISABLED;

	ret += nss_macsec_secy_rx_prc_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_rx_sc_create(drv->secy_id, channel);
	ret += nss_macsec_secy_rx_sc_validate_frame_set(drv->secy_id, channel,
							vf);
	ret += nss_macsec_secy_rx_sc_replay_protect_set(drv->secy_id, channel,
							drv->replay_protect);
	ret += nss_macsec_secy_rx_sc_anti_replay_window_set(drv->secy_id,
							    channel,
							    drv->replay_window);

	return ret;
}


static int macsec_qca_delete_receive_sc(void *priv, u32 channel)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	fal_rx_prc_lut_t entry;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* rx prc lut */
	os_memset(&entry, 0, sizeof(entry));

	ret += nss_macsec_secy_rx_sc_del(drv->secy_id, channel);
	ret += nss_macsec_secy_rx_prc_lut_set(drv->secy_id, channel, &entry);

	return ret;
}


static int macsec_qca_create_receive_sa(void *priv, u32 channel, u8 an,
					u32 lowest_pn, const u8 *sak)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	fal_rx_sak_t rx_sak;
	int i = 0;

	wpa_printf(MSG_DEBUG, "%s, channel=%d, an=%d, lpn=0x%x",
		   __func__, channel, an, lowest_pn);

	os_memset(&rx_sak, 0, sizeof(rx_sak));
	for (i = 0; i < 16; i++)
		rx_sak.sak[i] = sak[15 - i];

	ret += nss_macsec_secy_rx_sa_create(drv->secy_id, channel, an);
	ret += nss_macsec_secy_rx_sak_set(drv->secy_id, channel, an, &rx_sak);

	return ret;
}


static int macsec_qca_enable_receive_sa(void *priv, u32 channel, u8 an)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel, an);

	ret += nss_macsec_secy_rx_sa_en_set(drv->secy_id, channel, an, TRUE);

	return ret;
}


static int macsec_qca_disable_receive_sa(void *priv, u32 channel, u8 an)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel, an);

	ret += nss_macsec_secy_rx_sa_en_set(drv->secy_id, channel, an, FALSE);

	return ret;
}


static int macsec_qca_get_available_transmit_sc(void *priv, u32 *channel)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 sc_ch = 0;
	bool in_use = FALSE;

	for (sc_ch = 0; sc_ch < MAXSC; sc_ch++) {
		ret = nss_macsec_secy_tx_sc_in_used_get(drv->secy_id, sc_ch,
							&in_use);
		if (ret)
			continue;

		if (!in_use) {
			*channel = sc_ch;
			wpa_printf(MSG_DEBUG, "%s: channel=%d",
				   __func__, *channel);
			return 0;
		}
	}

	wpa_printf(MSG_DEBUG, "%s: no avaiable channel", __func__);

	return -1;
}


static int macsec_qca_create_transmit_sc(void *priv, u32 channel,
					 const u8 *sci_addr, u16 sci_port,
					 unsigned int conf_offset)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	fal_tx_class_lut_t entry;
	u8 psci[ETH_ALEN + 2];

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* class lut */
	os_memset(&entry, 0, sizeof(entry));

	entry.valid = 1;
	entry.action = FAL_TX_CLASS_ACTION_FORWARD;
	entry.channel = channel;

	os_memcpy(psci, sci_addr, ETH_ALEN);
	psci[6] = (sci_port >> 8) & 0xf;
	psci[7] = sci_port & 0xf;

	ret += nss_macsec_secy_tx_class_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_tx_sc_create(drv->secy_id, channel, psci, 8);
	ret += nss_macsec_secy_tx_sc_protect_set(drv->secy_id, channel,
						 drv->protect_frames);
	ret += nss_macsec_secy_tx_sc_confidentiality_offset_set(drv->secy_id,
								channel,
								conf_offset);

	return ret;
}


static int macsec_qca_delete_transmit_sc(void *priv, u32 channel)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	fal_tx_class_lut_t entry;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* class lut */
	os_memset(&entry, 0, sizeof(entry));

	ret += nss_macsec_secy_tx_class_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_tx_sc_del(drv->secy_id, channel);

	return ret;
}


static int macsec_qca_create_transmit_sa(void *priv, u32 channel, u8 an,
					 u32 next_pn, Boolean confidentiality,
					 const u8 *sak)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u8 tci = 0;
	fal_tx_sak_t tx_sak;
	int i;

	wpa_printf(MSG_DEBUG,
		   "%s: channel=%d, an=%d, next_pn=0x%x, confidentiality=%d",
		   __func__, channel, an, next_pn, confidentiality);

	if (drv->always_include_sci)
		tci |= TCI_SC;
	else if (drv->use_es)
		tci |= TCI_ES;
	else if (drv->use_scb)
		tci |= TCI_SCB;

	if (confidentiality)
		tci |= TCI_E | TCI_C;

	os_memset(&tx_sak, 0, sizeof(tx_sak));
	for (i = 0; i < 16; i++)
		tx_sak.sak[i] = sak[15 - i];

	ret += nss_macsec_secy_tx_sa_next_pn_set(drv->secy_id, channel, an,
						 next_pn);
	ret += nss_macsec_secy_tx_sak_set(drv->secy_id, channel, an, &tx_sak);
	ret += nss_macsec_secy_tx_sc_tci_7_2_set(drv->secy_id, channel,
						 (tci >> 2));
	ret += nss_macsec_secy_tx_sc_an_set(drv->secy_id, channel, an);

	return ret;
}


static int macsec_qca_enable_transmit_sa(void *priv, u32 channel, u8 an)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel, an);

	ret += nss_macsec_secy_tx_sa_en_set(drv->secy_id, channel, an, TRUE);

	return ret;
}


static int macsec_qca_disable_transmit_sa(void *priv, u32 channel, u8 an)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel, an);

	ret += nss_macsec_secy_tx_sa_en_set(drv->secy_id, channel, an, FALSE);

	return ret;
}


const struct wpa_driver_ops wpa_driver_macsec_qca_ops = {
	.name = "macsec_qca",
	.desc = "QCA MACsec Ethernet driver",
	.get_ssid = macsec_qca_get_ssid,
	.get_bssid = macsec_qca_get_bssid,
	.get_capa = macsec_qca_get_capa,
	.init = macsec_qca_init,
	.deinit = macsec_qca_deinit,

	.macsec_init = macsec_qca_macsec_init,
	.macsec_deinit = macsec_qca_macsec_deinit,
	.enable_protect_frames = macsec_qca_enable_protect_frames,
	.set_replay_protect = macsec_qca_set_replay_protect,
	.set_current_cipher_suite = macsec_qca_set_current_cipher_suite,
	.enable_controlled_port = macsec_qca_enable_controlled_port,
	.get_receive_lowest_pn = macsec_qca_get_receive_lowest_pn,
	.get_transmit_next_pn = macsec_qca_get_transmit_next_pn,
	.set_transmit_next_pn = macsec_qca_set_transmit_next_pn,
	.get_available_receive_sc = macsec_qca_get_available_receive_sc,
	.create_receive_sc = macsec_qca_create_receive_sc,
	.delete_receive_sc = macsec_qca_delete_receive_sc,
	.create_receive_sa = macsec_qca_create_receive_sa,
	.enable_receive_sa = macsec_qca_enable_receive_sa,
	.disable_receive_sa = macsec_qca_disable_receive_sa,
	.get_available_transmit_sc = macsec_qca_get_available_transmit_sc,
	.create_transmit_sc = macsec_qca_create_transmit_sc,
	.delete_transmit_sc = macsec_qca_delete_transmit_sc,
	.create_transmit_sa = macsec_qca_create_transmit_sa,
	.enable_transmit_sa = macsec_qca_enable_transmit_sa,
	.disable_transmit_sa = macsec_qca_disable_transmit_sa,
};
