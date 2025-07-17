/*
 * Wired Ethernet driver interface for QCA MACsec driver
 * Copyright (c) 2005-2009, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2004, Gunter Burchardt <tira@isx.de>
 * Copyright (c) 2013-2014, Qualcomm Atheros, Inc.
 * Copyright (c) 2019, The Linux Foundation
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
#include "common/eapol_common.h"
#include "pae/ieee802_1x_kay.h"
#include "driver.h"
#include "driver_wired_common.h"

#include "nss_macsec_secy.h"
#include "nss_macsec_secy_rx.h"
#include "nss_macsec_secy_tx.h"

#define MAXSC 16

#define SAK_128_LEN	16
#define SAK_256_LEN	32

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

struct channel_map {
	struct ieee802_1x_mka_sci sci;
};

struct macsec_qca_data {
	struct driver_wired_common_data common;

	int use_pae_group_addr;
	u32 secy_id;

	/* shadow */
	bool always_include_sci;
	bool use_es;
	bool use_scb;
	bool protect_frames;
	bool replay_protect;
	u32 replay_window;

	struct channel_map receive_channel_map[MAXSC];
	struct channel_map transmit_channel_map[MAXSC];
};


static void __macsec_drv_init(struct macsec_qca_data *drv)
{
	int ret = 0;
	fal_rx_ctl_filt_t rx_ctl_filt;
	fal_tx_ctl_filt_t tx_ctl_filt;

	wpa_printf(MSG_INFO, "%s: secy_id=%d", __func__, drv->secy_id);

	/* Enable Secy and Let EAPoL bypass */
	ret = nss_macsec_secy_en_set(drv->secy_id, true);
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
	nss_macsec_secy_en_set(drv->secy_id, false);
	nss_macsec_secy_rx_sc_del_all(drv->secy_id);
	nss_macsec_secy_tx_sc_del_all(drv->secy_id);
}


#ifdef __linux__

static void macsec_qca_handle_data(void *ctx, unsigned char *buf, size_t len)
{
#ifdef HOSTAPD
	struct ieee8023_hdr *hdr;
	u8 *pos, *sa;
	size_t left;
	union wpa_event_data event;

	/* at least 6 bytes src macaddress, 6 bytes dst macaddress
	 * and 2 bytes ethertype
	*/
	if (len < 14) {
		wpa_printf(MSG_MSGDUMP,
			   "macsec_qca_handle_data: too short (%lu)",
			   (unsigned long) len);
		return;
	}
	hdr = (struct ieee8023_hdr *) buf;

	switch (ntohs(hdr->ethertype)) {
	case ETH_P_PAE:
		wpa_printf(MSG_MSGDUMP, "Received EAPOL packet");
		sa = hdr->src;
		os_memset(&event, 0, sizeof(event));
		event.new_sta.addr = sa;
		wpa_supplicant_event(ctx, EVENT_NEW_STA, &event);

		pos = (u8 *) (hdr + 1);
		left = len - sizeof(*hdr);
		drv_event_eapol_rx(ctx, sa, pos, left);
		break;
	default:
		wpa_printf(MSG_DEBUG, "Unknown ethertype 0x%04x in data frame",
			   ntohs(hdr->ethertype));
		break;
	}
#endif /* HOSTAPD */
}


static void macsec_qca_handle_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	int len;
	unsigned char buf[3000];

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca: recv: %s", strerror(errno));
		return;
	}

	macsec_qca_handle_data(eloop_ctx, buf, len);
}

#endif /* __linux__ */


static int macsec_qca_init_sockets(struct macsec_qca_data *drv, u8 *own_addr)
{
#ifdef __linux__
	struct ifreq ifr;
	struct sockaddr_ll addr;

	drv->common.sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_PAE));
	if (drv->common.sock < 0) {
		wpa_printf(MSG_ERROR, "socket[PF_PACKET,SOCK_RAW]: %s",
			   strerror(errno));
		return -1;
	}

	if (eloop_register_read_sock(drv->common.sock, macsec_qca_handle_read,
				     drv->common.ctx, NULL)) {
		wpa_printf(MSG_INFO, "Could not register read socket");
		return -1;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->common.ifname, sizeof(ifr.ifr_name));
	if (ioctl(drv->common.sock, SIOCGIFINDEX, &ifr) != 0) {
		wpa_printf(MSG_ERROR, "ioctl(SIOCGIFINDEX): %s",
			   strerror(errno));
		return -1;
	}

	os_memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = ifr.ifr_ifindex;
	wpa_printf(MSG_DEBUG, "Opening raw packet socket for ifindex %d",
		   addr.sll_ifindex);

	if (bind(drv->common.sock, (struct sockaddr *) &addr,
		 sizeof(addr)) < 0) {
		wpa_printf(MSG_ERROR, "macsec_qca: bind: %s", strerror(errno));
		return -1;
	}

	/* filter multicast address */
	if (wired_multicast_membership(drv->common.sock, ifr.ifr_ifindex,
				       pae_group_addr, 1) < 0) {
		wpa_printf(MSG_ERROR,
			"macsec_qca_init_sockets: Failed to add multicast group membership");
		return -1;
	}

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, drv->common.ifname, sizeof(ifr.ifr_name));
	if (ioctl(drv->common.sock, SIOCGIFHWADDR, &ifr) != 0) {
		wpa_printf(MSG_ERROR, "ioctl(SIOCGIFHWADDR): %s",
			   strerror(errno));
		return -1;
	}

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
		wpa_printf(MSG_INFO, "Invalid HW-addr family 0x%04x",
			   ifr.ifr_hwaddr.sa_family);
		return -1;
	}
	os_memcpy(own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	return 0;
#else /* __linux__ */
	return -1;
#endif /* __linux__ */
}


static int macsec_qca_secy_id_get(const char *ifname, u32 *secy_id)
{
#ifdef NSS_MACSEC_SECY_ID_GET_FUNC
	/* Get secy id from nss macsec driver */
	return nss_macsec_secy_id_get((u8 *) ifname, secy_id);
#else /* NSS_MACSEC_SECY_ID_GET_FUNC */
	/* Board specific settings */
	if (os_strcmp(ifname, "eth2") == 0) {
		*secy_id = 1;
	} else if (os_strcmp(ifname, "eth3") == 0) {
		*secy_id = 2;
	} else if (os_strcmp(ifname, "eth4") == 0 ||
		   os_strcmp(ifname, "eth0") == 0) {
		*secy_id = 0;
	} else if (os_strcmp(ifname, "eth5") == 0 ||
		   os_strcmp(ifname, "eth1") == 0) {
		*secy_id = 1;
	} else {
		*secy_id = -1;
		return -1;
	}

	return 0;
#endif /* NSS_MACSEC_SECY_ID_GET_FUNC */
}


static void * macsec_qca_init(void *ctx, const char *ifname)
{
	struct macsec_qca_data *drv;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;

	if (macsec_qca_secy_id_get(ifname, &drv->secy_id)) {
		wpa_printf(MSG_ERROR,
			   "macsec_qca: Failed to get secy_id for %s", ifname);
		os_free(drv);
		return NULL;
	}

	if (driver_wired_init_common(&drv->common, ifname, ctx) < 0) {
		os_free(drv);
		return NULL;
	}

	return drv;
}


static void macsec_qca_deinit(void *priv)
{
	struct macsec_qca_data *drv = priv;

	driver_wired_deinit_common(&drv->common);
	os_free(drv);
}


static void * macsec_qca_hapd_init(struct hostapd_data *hapd,
				   struct wpa_init_params *params)
{
	struct macsec_qca_data *drv;

	drv = os_zalloc(sizeof(struct macsec_qca_data));
	if (!drv) {
		wpa_printf(MSG_INFO,
			   "Could not allocate memory for macsec_qca driver data");
		return NULL;
	}

	if (macsec_qca_secy_id_get(params->ifname, &drv->secy_id)) {
		wpa_printf(MSG_ERROR,
			   "macsec_qca: Failed to get secy_id for %s",
			   params->ifname);
		os_free(drv);
		return NULL;
	}

	drv->common.ctx = hapd;
	os_strlcpy(drv->common.ifname, params->ifname,
		   sizeof(drv->common.ifname));
	drv->use_pae_group_addr = params->use_pae_group_addr;

	if (macsec_qca_init_sockets(drv, params->own_addr)) {
		os_free(drv);
		return NULL;
	}

	return drv;
}


static void macsec_qca_hapd_deinit(void *priv)
{
	struct macsec_qca_data *drv = priv;

	if (drv->common.sock >= 0) {
		eloop_unregister_read_sock(drv->common.sock);
		close(drv->common.sock);
	}

	os_free(drv);
}


static int macsec_qca_send_eapol(void *priv, const u8 *addr,
				 const u8 *data, size_t data_len, int encrypt,
				 const u8 *own_addr, u32 flags, int link_id)
{
	struct macsec_qca_data *drv = priv;
	struct ieee8023_hdr *hdr;
	size_t len;
	u8 *pos;
	int res;

	len = sizeof(*hdr) + data_len;
	hdr = os_zalloc(len);
	if (!hdr) {
		wpa_printf(MSG_INFO,
			   "malloc() failed for macsec_qca_send_eapol(len=%lu)",
			   (unsigned long) len);
		return -1;
	}

	os_memcpy(hdr->dest, drv->use_pae_group_addr ? pae_group_addr : addr,
		  ETH_ALEN);
	os_memcpy(hdr->src, own_addr, ETH_ALEN);
	hdr->ethertype = htons(ETH_P_PAE);

	pos = (u8 *) (hdr + 1);
	os_memcpy(pos, data, data_len);

	res = send(drv->common.sock, (u8 *) hdr, len, 0);
	os_free(hdr);

	if (res < 0) {
		wpa_printf(MSG_ERROR,
			   "macsec_qca_send_eapol - packet len: %lu - failed: send: %s",
			   (unsigned long) len, strerror(errno));
	}

	return res;
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


static int macsec_qca_get_capability(void *priv, enum macsec_cap *cap)
{
	wpa_printf(MSG_DEBUG, "%s", __func__);

	*cap = MACSEC_CAP_INTEG_AND_CONF_0_30_50;

	return 0;
}


static int macsec_qca_enable_protect_frames(void *priv, bool enabled)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	drv->protect_frames = enabled;

	return ret;
}


static int macsec_qca_set_replay_protect(void *priv, bool enabled,
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


static fal_cipher_suite_e macsec_qca_cs_type_get(u64 cs)
{
	if (cs == CS_ID_GCM_AES_128)
		return FAL_CIPHER_SUITE_AES_GCM_128;
	if (cs == CS_ID_GCM_AES_256)
		return FAL_CIPHER_SUITE_AES_GCM_256;
	return FAL_CIPHER_SUITE_MAX;
}


static int macsec_qca_set_current_cipher_suite(void *priv, u64 cs)
{
	struct macsec_qca_data *drv = priv;
	fal_cipher_suite_e cs_type;

	if (cs != CS_ID_GCM_AES_128 && cs != CS_ID_GCM_AES_256) {
		wpa_printf(MSG_ERROR,
			   "%s: NOT supported CipherSuite: %016" PRIx64,
			   __func__, cs);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: CipherSuite: %016" PRIx64, __func__, cs);

	cs_type = macsec_qca_cs_type_get(cs);
	return nss_macsec_secy_cipher_suite_set(drv->secy_id, cs_type);
}


static int macsec_qca_enable_controlled_port(void *priv, bool enabled)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "%s: enable=%d", __func__, enabled);

	ret += nss_macsec_secy_controlled_port_en_set(drv->secy_id, enabled);

	return ret;
}


static int macsec_qca_lookup_channel(struct channel_map *map,
				     struct ieee802_1x_mka_sci *sci,
				     u32 *channel)
{
	u32 i;

	for (i = 0; i < MAXSC; i++) {
		if (os_memcmp(&map[i].sci, sci,
			      sizeof(struct ieee802_1x_mka_sci)) == 0) {
			*channel = i;
			return 0;
		}
	}

	return -1;
}


static void macsec_qca_register_channel(struct channel_map *map,
					struct ieee802_1x_mka_sci *sci,
					u32 channel)
{
	os_memcpy(&map[channel].sci, sci, sizeof(struct ieee802_1x_mka_sci));
}


static int macsec_qca_lookup_receive_channel(struct macsec_qca_data *drv,
					     struct receive_sc *sc,
					     u32 *channel)
{
	return macsec_qca_lookup_channel(drv->receive_channel_map, &sc->sci,
					 channel);
}


static void macsec_qca_register_receive_channel(struct macsec_qca_data *drv,
						struct receive_sc *sc,
						u32 channel)
{
	macsec_qca_register_channel(drv->receive_channel_map, &sc->sci,
				    channel);
}


static int macsec_qca_lookup_transmit_channel(struct macsec_qca_data *drv,
					      struct transmit_sc *sc,
					      u32 *channel)
{
	return macsec_qca_lookup_channel(drv->transmit_channel_map, &sc->sci,
					 channel);
}


static void macsec_qca_register_transmit_channel(struct macsec_qca_data *drv,
						 struct transmit_sc *sc,
						 u32 channel)
{
	macsec_qca_register_channel(drv->transmit_channel_map, &sc->sci,
				    channel);
}


static int macsec_qca_get_receive_lowest_pn(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 next_pn = 0;
	bool enabled = false;
	u32 win;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	ret += nss_macsec_secy_rx_sa_next_pn_get(drv->secy_id, channel, sa->an,
						 &next_pn);
	ret += nss_macsec_secy_rx_sc_replay_protect_get(drv->secy_id, channel,
							&enabled);
	ret += nss_macsec_secy_rx_sc_anti_replay_window_get(drv->secy_id,
							    channel, &win);

	if (enabled)
		sa->lowest_pn = (next_pn > win) ? (next_pn - win) : 1;
	else
		sa->lowest_pn = next_pn;

	wpa_printf(MSG_DEBUG, "%s: lpn=0x%x", __func__, sa->lowest_pn);

	return ret;
}


static int macsec_qca_get_transmit_next_pn(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	ret += nss_macsec_secy_tx_sa_next_pn_get(drv->secy_id, channel, sa->an,
						 &sa->next_pn);

	wpa_printf(MSG_DEBUG, "%s: npn=0x%x", __func__, sa->next_pn);

	return ret;
}


static int macsec_qca_set_transmit_next_pn(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	ret += nss_macsec_secy_tx_sa_next_pn_set(drv->secy_id, channel, sa->an,
						 sa->next_pn);

	wpa_printf(MSG_INFO, "%s: npn=0x%x", __func__, sa->next_pn);

	return ret;
}


static int macsec_qca_get_available_receive_sc(void *priv, u32 *channel)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	u32 sc_ch = 0;
	bool in_use = false;

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


static int macsec_qca_create_receive_sc(void *priv, struct receive_sc *sc,
					unsigned int conf_offset,
					int validation)
{
	struct macsec_qca_data *drv = priv;
	int ret = 0;
	fal_rx_prc_lut_t entry;
	fal_rx_sc_validate_frame_e vf;
	enum validate_frames validate_frames = validation;
	u32 channel;
	const u8 *sci_addr = sc->sci.addr;
	u16 sci_port = be_to_host16(sc->sci.port);

	ret = macsec_qca_get_available_receive_sc(priv, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* rx prc lut */
	os_memset(&entry, 0, sizeof(entry));

	os_memcpy(entry.sci, sci_addr, ETH_ALEN);
	entry.sci[6] = (sci_port >> 8) & 0xff;
	entry.sci[7] = sci_port & 0xff;
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

	macsec_qca_register_receive_channel(drv, sc, channel);

	return ret;
}


static int macsec_qca_delete_receive_sc(void *priv, struct receive_sc *sc)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_rx_prc_lut_t entry;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* rx prc lut */
	os_memset(&entry, 0, sizeof(entry));

	ret += nss_macsec_secy_rx_sc_del(drv->secy_id, channel);
	ret += nss_macsec_secy_rx_prc_lut_set(drv->secy_id, channel, &entry);

	return ret;
}


static int macsec_qca_create_receive_sa(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_rx_sak_t rx_sak;
	int i = 0;
	u32 channel;
	fal_rx_prc_lut_t entry;
	u32 offset;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s, channel=%d, an=%d, lpn=0x%x",
		   __func__, channel, sa->an, sa->lowest_pn);

	os_memset(&rx_sak, 0, sizeof(rx_sak));
	rx_sak.sak_len = sa->pkey->key_len;
	if (sa->pkey->key_len == SAK_128_LEN) {
		for (i = 0; i < 16; i++)
			rx_sak.sak[i] = sa->pkey->key[15 - i];
	} else if (sa->pkey->key_len == SAK_256_LEN) {
		for (i = 0; i < 16; i++) {
			rx_sak.sak1[i] = sa->pkey->key[15 - i];
			rx_sak.sak[i] = sa->pkey->key[31 - i];
		}
	} else {
		return -1;
	}

	if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_0)
		offset = 0;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_30)
		offset = 30;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_50)
		offset = 50;
	else
		return -1;
	ret += nss_macsec_secy_rx_prc_lut_get(drv->secy_id, channel, &entry);
	entry.offset = offset;
	ret += nss_macsec_secy_rx_prc_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_rx_sa_create(drv->secy_id, channel, sa->an);
	ret += nss_macsec_secy_rx_sak_set(drv->secy_id, channel, sa->an,
					  &rx_sak);

	return ret;
}


static int macsec_qca_enable_receive_sa(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_rx_sa_en_set(drv->secy_id, channel, sa->an,
					    true);

	return ret;
}


static int macsec_qca_disable_receive_sa(void *priv, struct receive_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_receive_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_rx_sa_en_set(drv->secy_id, channel, sa->an,
					    false);

	return ret;
}


static int macsec_qca_get_available_transmit_sc(void *priv, u32 *channel)
{
	struct macsec_qca_data *drv = priv;
	u32 sc_ch = 0;
	bool in_use = false;

	for (sc_ch = 0; sc_ch < MAXSC; sc_ch++) {
		if (nss_macsec_secy_tx_sc_in_used_get(drv->secy_id, sc_ch,
						      &in_use))
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


static int macsec_qca_create_transmit_sc(void *priv, struct transmit_sc *sc,
					 unsigned int conf_offset)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_tx_class_lut_t entry;
	u8 psci[ETH_ALEN + 2];
	u32 channel;
	u16 sci_port = be_to_host16(sc->sci.port);

	ret = macsec_qca_get_available_transmit_sc(priv, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* class lut */
	os_memset(&entry, 0, sizeof(entry));

	entry.valid = 1;
	entry.action = FAL_TX_CLASS_ACTION_FORWARD;
	entry.channel = channel;

	os_memcpy(psci, sc->sci.addr, ETH_ALEN);
	psci[6] = (sci_port >> 8) & 0xff;
	psci[7] = sci_port & 0xff;

	ret += nss_macsec_secy_tx_class_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_tx_sc_create(drv->secy_id, channel, psci, 8);
	ret += nss_macsec_secy_tx_sc_protect_set(drv->secy_id, channel,
						 drv->protect_frames);
	ret += nss_macsec_secy_tx_sc_confidentiality_offset_set(drv->secy_id,
								channel,
								conf_offset);

	macsec_qca_register_transmit_channel(drv, sc, channel);

	return ret;
}


static int macsec_qca_delete_transmit_sc(void *priv, struct transmit_sc *sc)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	fal_tx_class_lut_t entry;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d", __func__, channel);

	/* class lut */
	os_memset(&entry, 0, sizeof(entry));

	ret += nss_macsec_secy_tx_class_lut_set(drv->secy_id, channel, &entry);
	ret += nss_macsec_secy_tx_sc_del(drv->secy_id, channel);

	return ret;
}


static int macsec_qca_create_transmit_sa(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u8 tci = 0;
	fal_tx_sak_t tx_sak;
	int i;
	u32 channel;
	u32 offset;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG,
		   "%s: channel=%d, an=%d, next_pn=0x%x, confidentiality=%d",
		   __func__, channel, sa->an, sa->next_pn, sa->confidentiality);

	if (drv->always_include_sci)
		tci |= TCI_SC;
	else if (drv->use_es)
		tci |= TCI_ES;
	else if (drv->use_scb)
		tci |= TCI_SCB;

	if (sa->confidentiality)
		tci |= TCI_E | TCI_C;

	os_memset(&tx_sak, 0, sizeof(tx_sak));
	tx_sak.sak_len = sa->pkey->key_len;
	if (sa->pkey->key_len == SAK_128_LEN) {
		for (i = 0; i < 16; i++)
			tx_sak.sak[i] = sa->pkey->key[15 - i];
	} else if (sa->pkey->key_len == SAK_256_LEN) {
		for (i = 0; i < 16; i++) {
			tx_sak.sak1[i] = sa->pkey->key[15 - i];
			tx_sak.sak[i] = sa->pkey->key[31 - i];
		}
	} else {
		return -1;
	}

	if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_0)
		offset = 0;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_30)
		offset = 30;
	else if (sa->pkey->confidentiality_offset == CONFIDENTIALITY_OFFSET_50)
		offset = 50;
	else
		return -1;
	ret += nss_macsec_secy_tx_sc_confidentiality_offset_set(drv->secy_id,
								channel,
								offset);
	ret += nss_macsec_secy_tx_sa_next_pn_set(drv->secy_id, channel, sa->an,
						 sa->next_pn);
	ret += nss_macsec_secy_tx_sak_set(drv->secy_id, channel, sa->an,
					  &tx_sak);
	ret += nss_macsec_secy_tx_sc_tci_7_2_set(drv->secy_id, channel,
						 (tci >> 2));
	ret += nss_macsec_secy_tx_sc_an_set(drv->secy_id, channel, sa->an);

	return ret;
}


static int macsec_qca_enable_transmit_sa(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_tx_sa_en_set(drv->secy_id, channel, sa->an,
					    true);

	return ret;
}


static int macsec_qca_disable_transmit_sa(void *priv, struct transmit_sa *sa)
{
	struct macsec_qca_data *drv = priv;
	int ret;
	u32 channel;

	ret = macsec_qca_lookup_transmit_channel(priv, sa->sc, &channel);
	if (ret != 0)
		return ret;

	wpa_printf(MSG_DEBUG, "%s: channel=%d, an=%d", __func__, channel,
		   sa->an);

	ret += nss_macsec_secy_tx_sa_en_set(drv->secy_id, channel, sa->an,
					    false);

	return ret;
}


const struct wpa_driver_ops wpa_driver_macsec_qca_ops = {
	.name = "macsec_qca",
	.desc = "QCA MACsec Ethernet driver",
	.get_ssid = driver_wired_get_ssid,
	.get_bssid = driver_wired_get_bssid,
	.get_capa = driver_wired_get_capa,
	.init = macsec_qca_init,
	.deinit = macsec_qca_deinit,
	.hapd_init = macsec_qca_hapd_init,
	.hapd_deinit = macsec_qca_hapd_deinit,
	.hapd_send_eapol = macsec_qca_send_eapol,

	.macsec_init = macsec_qca_macsec_init,
	.macsec_deinit = macsec_qca_macsec_deinit,
	.macsec_get_capability = macsec_qca_get_capability,
	.enable_protect_frames = macsec_qca_enable_protect_frames,
	.set_replay_protect = macsec_qca_set_replay_protect,
	.set_current_cipher_suite = macsec_qca_set_current_cipher_suite,
	.enable_controlled_port = macsec_qca_enable_controlled_port,
	.get_receive_lowest_pn = macsec_qca_get_receive_lowest_pn,
	.get_transmit_next_pn = macsec_qca_get_transmit_next_pn,
	.set_transmit_next_pn = macsec_qca_set_transmit_next_pn,
	.create_receive_sc = macsec_qca_create_receive_sc,
	.delete_receive_sc = macsec_qca_delete_receive_sc,
	.create_receive_sa = macsec_qca_create_receive_sa,
	.enable_receive_sa = macsec_qca_enable_receive_sa,
	.disable_receive_sa = macsec_qca_disable_receive_sa,
	.create_transmit_sc = macsec_qca_create_transmit_sc,
	.delete_transmit_sc = macsec_qca_delete_transmit_sc,
	.create_transmit_sa = macsec_qca_create_transmit_sa,
	.enable_transmit_sa = macsec_qca_enable_transmit_sa,
	.disable_transmit_sa = macsec_qca_disable_transmit_sa,
};
