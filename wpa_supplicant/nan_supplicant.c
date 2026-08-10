/*
 * wpa_supplicant - NAN
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "utils/eloop.h"
#include "utils/bitfield.h"
#include "common/nan_de.h"
#include "common/ieee802_11_common.h"
#include "ap/hostapd.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "nan/nan.h"
#include "config.h"
#include "offchannel.h"
#include "notify.h"
#include "p2p_supplicant.h"
#include "pr_supplicant.h"
#include "nan_supplicant.h"

#define DEFAULT_NAN_MASTER_PREF 2
#define DEFAULT_NAN_DUAL_BAND   0
#define DEFAULT_NAN_SCAN_PERIOD 60
#define DEFAULT_NAN_SCAN_DWELL_TIME 150
#define DEFAULT_NAN_DISCOVERY_BEACON_INTERVAL 100
#define DEFAULT_NAN_LOW_BAND_FREQUENCY 2437
#define DEFAULT_NAN_HIGH_BAND_FREQUENCY 5745
#define DEFAULT_NAN_RSSI_CLOSE -50
#define DEFAULT_NAN_RSSI_MIDDLE -65

#define NAN_MIN_RSSI_CLOSE  -60
#define NAN_MIN_RSSI_MIDDLE -75

#define NAN_AVAIL_ATTR_MAX_LEN 100

#define DEFAULT_NAN_SUPP_PBM (NAN_PBA_METHOD_OPPORTUNISTIC |      \
			      NAN_PBA_METHOD_PIN_DISPLAY |        \
			      NAN_PBA_METHOD_PASSPHRASE_DISPLAY | \
			      NAN_PBA_METHOD_QR_DISPLAY |         \
			      NAN_PBA_METHOD_NFC_TAG |            \
			      NAN_PBA_METHOD_PIN_KEYPAD |         \
			      NAN_PBA_METHOD_PASSPHRASE_KEYPAD |  \
			      NAN_PBA_METHOD_QR_SCAN |            \
			      NAN_PBA_METHOD_NFC_READER)

#define DEFAULT_NAN_AUTO_ACCEPT_PBM NAN_PBA_METHOD_OPPORTUNISTIC

#define DEFAULT_NAN_BOOTSTRAP_COMEBACK_TIMEOUT 1024

/* Default NAN NIK lifetime in seconds - 12 hours */
#define NAN_NIK_LIFETIME_DEFAULT 43200

/* Default NAN idle period in seconds */
#define DEFAULT_NAN_MAX_NDL_IDLE_PERIOD 25

#ifdef CONFIG_NAN

static int get_center(u8 channel, const u8 *center_channels,
		      unsigned int num_chan, int width)
{
	int span = (width - 20) / 10;
	unsigned int i;

	for (i = 0; i < num_chan; i++) {
		if (channel >= center_channels[i] - span &&
		    channel <= center_channels[i] + span)
			return center_channels[i];
	}

	return 0;
}


static u8 get_center_and_width(int bw, u8 channel, int *width)
{
	static const u8 nan_160mhz_5ghz_chans[] = { 50, 114, 163 };
	static const u8 nan_80mhz_5ghz_chans[] =
		{ 42, 58, 106, 122, 138, 155, 171 };

	switch (bw) {
	case BW20:
		*width = 20;
		return channel;
	case BW40PLUS:
	case BW40MINUS:
		*width = 40;
		return bw == BW40PLUS ? channel + 2 : channel - 2;
	case BW80:
		*width = 80;
		return get_center(channel, nan_80mhz_5ghz_chans,
				  ARRAY_SIZE(nan_80mhz_5ghz_chans), *width);
	case BW160:
		*width = 160;
		return get_center(channel, nan_160mhz_5ghz_chans,
				  ARRAY_SIZE(nan_160mhz_5ghz_chans), *width);
	default:
		return 0;
	}

	return 0;
}


static bool wpas_nan_valid_chan(struct wpa_supplicant *wpa_s,
				enum hostapd_hw_mode mode,
				u8 channel, int bw, u8 op_class, u8 *cf1)
{
	struct hostapd_hw_modes *hw_mode;
	int width, span;
	u8 c, center;

	hw_mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes, mode, false);
	if (!hw_mode)
		return false;

	center = get_center_and_width(bw, channel, &width);
	if (!center)
		return false;

	if (wpa_s->nan_max_bw && width > wpa_s->nan_max_bw)
		return false;

	span = (width - 20) / 10;
	for (c = center - span; c <= center + span; c += 4) {
		int freq = ieee80211_chan_to_freq(NULL, op_class, c);

		if (freq < 0)
			return false;

		if (freq_range_list_includes(&wpa_s->nan_disallowed_freqs,
					     freq))
			return false;

		if (ieee80211_is_dfs(freq, wpa_s->hw.modes,
				     wpa_s->hw.num_modes))
			return false;
	}

	/* Wide channels use center */
	if (width > 40)
		channel = center;

	*cf1 = center;
	return verify_channel(hw_mode, op_class, channel, bw) == ALLOWED;
}


static int wpas_nan_start_cb(void *ctx, const struct nan_cluster_config *config)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpa_drv_nan_start(wpa_s, config);
}


static int wpas_nan_update_config_cb(void *ctx,
				     const struct nan_cluster_config *config)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpa_drv_nan_update_config(wpa_s, config);
}


static void clear_sched_config(struct nan_schedule_config *sched_cfg)
{
	int i;

	for (i = 0; i < sched_cfg->num_channels; i++)
		wpabuf_free(sched_cfg->channels[i].time_bitmap);

	wpabuf_free(sched_cfg->avail_attr);
	os_memset(sched_cfg, 0, sizeof(*sched_cfg));
}


static void wpas_nan_stop_cb(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	int i;

	for (i = 0; i < MAX_NAN_RADIOS; i++) {
		if (wpa_s->nan_sched[i].num_channels) {
			wpa_drv_nan_config_schedule(wpa_s, i + 1, NULL);
			clear_sched_config(&wpa_s->nan_sched[i]);
		}
	}

	wpa_drv_nan_stop(wpa_s);
	nan_de_set_cluster_id(wpa_s->nan_de, NULL);
	wpas_notify_nan_stopped(wpa_s);
}


static int wpas_nan_set_peer_sched_chan(struct wpa_supplicant *wpa_s,
					const struct nan_peer_schedule *sched,
					int i, int j,
					struct nan_schedule_config *sched_cfg)
{
	const struct nan_map_chan *src_chan = &sched->maps[i].chans[j];
	struct nan_chan_entry *chan_entry;
	int ch_idx;

	if (!src_chan->committed)
		return 0;

	wpa_printf(MSG_DEBUG, "    Channel freq=%u, rx_nss=%u",
		   src_chan->chan.freq, src_chan->rx_nss);
	wpa_hexdump(MSG_DEBUG, "      committed_bitmap",
		    src_chan->tbm.bitmap, src_chan->tbm.len);

	ch_idx = sched_cfg->num_channels;
	sched_cfg->channels[ch_idx].freq = src_chan->chan.freq;
	sched_cfg->channels[ch_idx].center_freq1 = src_chan->chan.center_freq1;
	sched_cfg->channels[ch_idx].center_freq2 = src_chan->chan.center_freq2;
	sched_cfg->channels[ch_idx].bandwidth = src_chan->chan.bandwidth;
	sched_cfg->channels[ch_idx].rx_nss = src_chan->rx_nss;
	chan_entry = (struct nan_chan_entry *)
		sched_cfg->channels[ch_idx].chan_entry;

	if (nan_get_chan_entry(wpa_s->nan, &src_chan->chan, chan_entry)) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to get chan entry for freq %d",
			   src_chan->chan.freq);
		return -1;
	}

	/* Copy time bitmap */
	if (src_chan->tbm.len > 0)
		sched_cfg->channels[ch_idx].time_bitmap =
			wpabuf_alloc_copy(src_chan->tbm.bitmap,
					  src_chan->tbm.len);

	sched_cfg->num_channels++;

	return 0;
}


static int wpas_nan_set_peer_schedule_cb(void *ctx, const u8 *nmi_addr,
					 bool new_sta, u16 cdw, u8 sequence_id,
					 u16 max_channel_switch_time,
					 const struct nan_peer_schedule *sched,
					 const struct wpabuf *ulw_elems)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct nan_peer_schedule_config peer_sched;
	int i, j, ret;

	wpa_printf(MSG_DEBUG, "NAN: Set peer schedule - nmi_addr=" MACSTR
		   " new_sta=%d cdw=%u seq_id=%u max_chan_switch_time=%u",
		   MAC2STR(nmi_addr), new_sta, cdw, sequence_id,
		   max_channel_switch_time);

	if (new_sta) {
		struct hostapd_sta_add_params sta_params;

		wpa_printf(MSG_DEBUG, "NAN: New NMI station");
		os_memset(&sta_params, 0, sizeof(sta_params));
		sta_params.addr = nmi_addr;
		sta_params.flags = WPA_STA_AUTHENTICATED | WPA_STA_ASSOCIATED;
		sta_params.flags_mask = sta_params.flags;
		ret = wpa_drv_sta_add(wpa_s, &sta_params);
		if (ret) {
			wpa_printf(MSG_INFO, "NAN: Failed to add NMI station");
			return ret;
		}
	}

	os_memset(&peer_sched, 0, sizeof(peer_sched));
	if (sched) {
		wpa_printf(MSG_DEBUG, "NAN: Peer schedule info:");
		wpa_printf(MSG_DEBUG, "  n_maps=%u", sched->n_maps);

		for (i = 0; i < sched->n_maps && i < MAX_NUM_NAN_MAPS; i++) {
			struct nan_schedule_config *sched_cfg =
				&peer_sched.maps[peer_sched.n_maps].sched;

			wpa_printf(MSG_DEBUG, "  Map %d: map_id=%u",
				   i, sched->maps[i].map_id);

			sched_cfg->num_channels = 0;

			for (j = 0; j < sched->maps[i].n_chans &&
				     sched_cfg->num_channels <
				     MAX_NUM_NAN_SCHEDULE_CHANNELS; j++) {
				if (wpas_nan_set_peer_sched_chan(wpa_s, sched,
								 i, j,
								 sched_cfg) < 0)
					goto out;
			}

			/* Only add map if it has channels after filtering */
			if (sched_cfg->num_channels > 0) {
				peer_sched.maps[peer_sched.n_maps].map_id =
					sched->maps[i].map_id;
				peer_sched.n_maps++;
			}
		}
	}

	ret = wpa_drv_nan_config_peer_schedule(wpa_s, nmi_addr,
					       cdw, sequence_id,
					       max_channel_switch_time,
					       ulw_elems, &peer_sched);

	/* Only print an error without returning, so we attempt to remove
	 * the STA if needed (sched == NULL)
	 */
	if (ret)
		wpa_printf(MSG_INFO, "NAN: Failed to configure peer schedule");

	if (!sched && !new_sta) {
		/* TODO: Should we maybe keep that NMI station? */
		wpa_printf(MSG_DEBUG, "NAN: Unpair NMI station before removal");
		nan_pairing_unpair_peer(wpa_s->nan, nmi_addr);

		wpa_printf(MSG_DEBUG, "NAN: Remove NMI station");
		ret = wpa_drv_sta_remove(wpa_s, nmi_addr);
		if (ret)
			wpa_printf(MSG_INFO,
				   "NAN: Failed to remove NMI station");
	}

out:
	/* Free allocated time bitmaps */
	for (i = 0; i < peer_sched.n_maps; i++) {
		struct nan_schedule_config *sched_cfg =
			&peer_sched.maps[i].sched;

		for (j = 0; j < sched_cfg->num_channels; j++) {
			wpabuf_free(sched_cfg->channels[j].time_bitmap);
			sched_cfg->channels[j].time_bitmap = NULL;
		}
	}

	return ret;
}


static void
wpas_nan_ndp_action_notif_cb(void *ctx,
			     struct nan_ndp_action_notif_params *params)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (params->is_request) {
		wpas_notify_nan_ndp_request(wpa_s, params->ndp_id.peer_nmi,
					    params->ndp_id.init_ndi,
					    params->ndp_id.id,
					    params->publish_inst_id,
					    params->ssi, params->ssi_len,
					    params->csid);
	} else {
		wpas_notify_nan_ndp_counter_request(wpa_s,
						    params->ndp_id.peer_nmi,
						    params->ndp_id.init_ndi,
						    params->ndp_id.id,
						    params->ssi,
						    params->ssi_len);
	}
}


static int wpas_nan_set_ndi_keys(struct wpa_supplicant *wpa_s,
				  const u8 *ndi_addr,
				  enum nan_cipher_suite_id csid,
				  const u8 *tk, size_t tk_len)
{
	enum wpa_alg alg;
	u8 rsc[6];

	os_memset(rsc, 0, sizeof(rsc));
	switch (csid) {
	case NAN_CS_SK_CCM_128:
	case NAN_CS_PK_PASN_128:
		alg = WPA_ALG_CCMP;
		break;
	case NAN_CS_SK_GCM_256:
	case NAN_CS_PK_PASN_256:
		alg = WPA_ALG_GCMP_256;
		break;
	default:
		wpa_printf(MSG_INFO, "NAN: Unsupported CSID %d for NDI keys",
			   csid);
		return -1;
	}

	return wpa_drv_set_key(wpa_s, -1, alg, ndi_addr, 0, 1, rsc, sizeof(rsc),
			       tk, tk_len, KEY_FLAG_PAIRWISE);
}


static int wpas_nan_remove_ndi_keys(struct wpa_supplicant *wpa_s,
				    const u8 *ndi_addr)
{
	return wpa_drv_set_key(wpa_s, -1, WPA_ALG_NONE, ndi_addr, 0, 0,
			       NULL, 0, NULL, 0, KEY_FLAG_PAIRWISE);
}


static int wpas_nan_remove_ndi_gtk(struct wpa_supplicant *wpa_s, int key_id,
				   const u8 *ndi_addr)
{
	return wpa_drv_set_key(wpa_s, -1, WPA_ALG_NONE, ndi_addr, key_id, 0,
			       NULL, 0, NULL, 0, KEY_FLAG_GROUP);
}


static int wpas_nan_remove_ndi_local_gtk(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->ndi_gtk.gtk.gtk_len)
		return 0;

	if (wpa_drv_set_key(wpa_s, -1, WPA_ALG_NONE, broadcast_ether_addr,
			    wpa_s->ndi_gtk.id, 0, NULL, 0, NULL, 0,
			    KEY_FLAG_GROUP_TX_DEFAULT)) {
		wpa_printf(MSG_INFO, "NAN: Failed to remove NDI group TX key");
		return -1;
	}

	wpa_s->ndi_gtk.id = 0;
	os_memset(&wpa_s->ndi_gtk, 0, sizeof(wpa_s->ndi_gtk));
	return 0;
}


static struct wpa_supplicant *
wpas_nan_get_ndi_iface(struct wpa_supplicant *wpa_s, const u8 *ndi_addr)
{
	struct wpa_supplicant *ndi_wpa_s;

	for (ndi_wpa_s = wpa_s->global->ifaces; ndi_wpa_s;
	     ndi_wpa_s = ndi_wpa_s->next) {
		if (ndi_wpa_s->nan_data &&
		    ether_addr_equal(ndi_wpa_s->own_addr, ndi_addr))
			return ndi_wpa_s;
	}

	return NULL;
}


static int wpas_nan_configure_nmi_sta_capa(struct wpa_supplicant *wpa_s,
					  const u8 *nmi_addr)
{
	struct hostapd_sta_add_params sta_params;
	const u8 *ie;
	u8 *elems;
	int elems_len;

	elems_len = nan_get_peer_elems(wpa_s->nan, nmi_addr, &elems);
	if (elems_len < 0) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to get peer elems for NMI station");
		return -1;
	}

	os_memset(&sta_params, 0, sizeof(sta_params));
	sta_params.addr = nmi_addr;
	sta_params.set = 1;
	sta_params.flags = WPA_STA_AUTHORIZED;
	sta_params.flags_mask = sta_params.flags;

	ie = get_ie(elems, elems_len, WLAN_EID_HT_CAP);
	if (!ie) {
		wpa_printf(MSG_INFO,
			   "NAN: No HT capabilities in peer elems for NMI station");
		return -1;
	}

	sta_params.ht_capabilities = (const void *) (ie + 2);
	ie = get_ie(elems, elems_len, WLAN_EID_VHT_CAP);
	sta_params.vht_capabilities = ie ? (const void *) (ie + 2) : NULL;

	return wpa_drv_sta_add(wpa_s, &sta_params);
}


static int wpas_nan_csid_to_wpa_alg(enum nan_cipher_suite_id csid,
				    enum wpa_alg *alg)
{
	switch (csid) {
	case NAN_CS_NONE:
		*alg = WPA_ALG_NONE;
		break;
	case NAN_CS_SK_CCM_128:
	case NAN_CS_GTK_CCMP_128:
		*alg = WPA_ALG_CCMP;
		break;
	case NAN_CS_SK_GCM_256:
	case NAN_CS_GTK_GCMP_256:
		*alg = WPA_ALG_GCMP_256;
		break;
	default:
		wpa_printf(MSG_INFO, "NAN: Unsupported CSID %d", csid);
		return -1;
	}

	return 0;
}


static int wpas_nan_set_ndi_group_keys(struct wpa_supplicant *wpa_s,
				       struct nan_ndp_connection_params *params)
{
	enum wpa_alg alg;

	/* Install the local GTK only if not already installed */
	if (!wpa_s->ndi_gtk.id && params->local_gtk && params->local_gtk->id) {
		u8 rsc[RSN_PN_LEN];

		if (wpas_nan_csid_to_wpa_alg(params->local_gtk->csid, &alg)) {
			wpa_printf(MSG_INFO,
				   "NAN: Unsupported CSID %u for local GTK",
				   params->local_gtk->csid);
			return -1;
		}

		os_memset(rsc, 0, sizeof(rsc));
		if (wpa_drv_set_key(wpa_s, -1, alg, broadcast_ether_addr,
				    params->local_gtk->id, 0, rsc, sizeof(rsc),
				    params->local_gtk->gtk.gtk,
				    params->local_gtk->gtk.gtk_len,
				    KEY_FLAG_GROUP_TX_DEFAULT)) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to set local GTK for NDI");
			return -1;
		}

		os_memcpy(&wpa_s->ndi_gtk, params->local_gtk,
			  sizeof(wpa_s->ndi_gtk));
	}

	if (params->peer_gtk && params->peer_gtk->id) {
		if (wpas_nan_csid_to_wpa_alg(params->peer_gtk->csid, &alg)) {
			wpa_printf(MSG_INFO,
				   "NAN: Unsupported CSID %u for peer GTK",
				   params->peer_gtk->csid);
			return -1;
		}

		if (wpa_drv_set_key(wpa_s, -1, alg, params->peer_ndi,
				    params->peer_gtk->id, 0,
				    params->peer_gtk_rsc, RSN_PN_LEN,
				    params->peer_gtk->gtk.gtk,
				    params->peer_gtk->gtk.gtk_len,
				    KEY_FLAG_GROUP_RX)) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to set peer GTK for NDI");
			return -1;
		}
	}

	return 0;
}


static int wpas_nan_add_ndi_sta(struct wpa_supplicant *wpa_s,
				struct nan_ndp_connection_params *params)
{
	u8 tk[WPA_TK_MAX_LEN];
	size_t tk_len;
	enum nan_cipher_suite_id csid;
	struct wpa_supplicant *ndi_wpa_s;
	struct hostapd_sta_add_params sta_params;
	const u8 *peer_nmi = params->ndp_id.peer_nmi;
	const u8 *peer_ndi = params->peer_ndi;

	ndi_wpa_s = wpas_nan_get_ndi_iface(wpa_s, params->local_ndi);
	if (!ndi_wpa_s) {
		wpa_printf(MSG_INFO,
			   "NAN: No NDI interface found for " MACSTR,
			   MAC2STR(params->local_ndi));
		return -1;
	}

	/* HT/VHT capabilities are configured per NMI station only for
	 * the first NDP. After that it is assumed that capablities are not
	 * changing.
	 */
	if (params->first_ndp &&
	    wpas_nan_configure_nmi_sta_capa(wpa_s, peer_nmi)) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to configure NMI station capabilities");
		return -1;
	}

	if (params->new_ndi_sta) {
		os_memset(&sta_params, 0, sizeof(sta_params));
		sta_params.addr = peer_ndi;
		sta_params.nmi_addr = peer_nmi;
		sta_params.flags = WPA_STA_AUTHENTICATED | WPA_STA_ASSOCIATED;

		/* Set MFP flag early, to prevent races until keys are installed
		 */
		if (params->install_keys)
			sta_params.flags |= WPA_STA_MFP;
		else
			sta_params.flags |= WPA_STA_AUTHORIZED;

		if (wpa_drv_sta_add(ndi_wpa_s, &sta_params)) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to add NDI station for peer "
				   MACSTR, MAC2STR(peer_ndi));
			return -1;
		}
	} else {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDI station already exists for peer " MACSTR,
			   MAC2STR(peer_ndi));
		/* Set MFP flag if keys will be installed (security upgrade) */
		if (params->install_keys &&
		    wpa_drv_sta_set_flags(ndi_wpa_s, peer_ndi, WPA_STA_MFP,
					  WPA_STA_MFP, ~0)) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to set MFP flag for peer "
				   MACSTR, MAC2STR(peer_ndi));
			return -1;
		}
	}

	wpa_printf(MSG_DEBUG, "NAN: NDI station for peer " MACSTR " %s",
		   MAC2STR(peer_ndi),
		   params->new_ndi_sta ? "added" : "already exists");

	if (!params->install_keys) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDI station %s without keys for peer " MACSTR,
			   params->new_ndi_sta ? "added" : "ready",
			   MAC2STR(peer_ndi));
		goto out_success;
	}

	if (nan_peer_get_tk(wpa_s->nan, peer_nmi, peer_ndi, params->local_ndi,
			    tk, &tk_len, &csid)) {
		wpa_printf(MSG_INFO, "NAN: Failed to get TK for NDI station");
		goto remove_sta;
	}

	if (wpas_nan_set_ndi_keys(ndi_wpa_s, peer_ndi, csid, tk, tk_len)) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to set NDI keys for peer " MACSTR,
			   MAC2STR(peer_ndi));
		forced_memzero(tk, tk_len);
		goto remove_sta;
	}
	forced_memzero(tk, tk_len);

	if (wpas_nan_set_ndi_group_keys(ndi_wpa_s, params)) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to set NDI group keys for peer "
			   MACSTR, MAC2STR(peer_ndi));
		wpas_nan_remove_ndi_keys(ndi_wpa_s, peer_ndi);
		goto remove_sta;
	}

	if (wpa_drv_sta_set_flags(ndi_wpa_s, peer_ndi, WPA_STA_AUTHORIZED,
				  WPA_STA_AUTHORIZED, ~0)) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to set authorize for NDI station");
		wpas_nan_remove_ndi_keys(ndi_wpa_s, peer_ndi);
		wpas_nan_remove_ndi_gtk(ndi_wpa_s, params->peer_gtk->id,
					peer_ndi);
		goto remove_sta;
	}

out_success:
	ndi_wpa_s->nan_ndi_ndp_refcount++;
	wpa_printf(MSG_DEBUG,
		   "NAN: NDP refcount incremented to %u (peer_ndi=" MACSTR
		   " peer_nmi=" MACSTR ")",
		   ndi_wpa_s->nan_ndi_ndp_refcount,
		   MAC2STR(peer_ndi), MAC2STR(peer_nmi));

	/* Set operstate UP only when the first NDP is established on this NDI
	 */
	if (ndi_wpa_s->nan_ndi_ndp_refcount == 1)
		wpa_drv_set_operstate(ndi_wpa_s, 1);

	return 0;

remove_sta:
	/*
	 * Clean up the NDI station if it was newly added for this NDP. For
	 * existing stations, we assume that the caller will tear down other
	 * NDPs with this station on failure as it may be now in some
	 * inconsistent state that is too hard to rollback here.
	 */
	if (params->new_ndi_sta)
		wpa_drv_sta_remove(ndi_wpa_s, params->peer_ndi);
	return -2;
}


static void wpas_nan_remove_ndi_sta(struct wpa_supplicant *wpa_s,
				    const u8 *local_ndi,
				    const u8 *peer_ndi,
				    bool remove_sta, int gtk_id)
{
	struct wpa_supplicant *ndi_wpa_s;

	ndi_wpa_s = wpas_nan_get_ndi_iface(wpa_s, local_ndi);
	if (!ndi_wpa_s) {
		wpa_printf(MSG_INFO,
			   "NAN: No NDI interface found for " MACSTR,
			   MAC2STR(local_ndi));
		return;
	}

	if (!ndi_wpa_s->nan_ndi_ndp_refcount)
		return;

	ndi_wpa_s->nan_ndi_ndp_refcount--;
	wpa_printf(MSG_DEBUG, "NAN: NDP refcount decremented to %u (peer_ndi="
		   MACSTR ")", ndi_wpa_s->nan_ndi_ndp_refcount,
		   MAC2STR(peer_ndi));

	/* Only remove the NDI station if no other NDP is using the same
	 * peer NDI address
	 */
	if (remove_sta) {
		if (wpa_drv_sta_set_flags(ndi_wpa_s, peer_ndi,
					  WPA_STA_AUTHORIZED, 0,
					  ~WPA_STA_AUTHORIZED))
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to clear authorized flag for NDI station");

		wpas_nan_remove_ndi_keys(ndi_wpa_s, peer_ndi);
		if (gtk_id)
			wpas_nan_remove_ndi_gtk(ndi_wpa_s, gtk_id, peer_ndi);
		wpa_drv_sta_remove(ndi_wpa_s, peer_ndi);
	}

	/* Remove the local GTK and set operstate DORMANT only when the last NDP
	 * is removed from this NDI
	 */
	if (!ndi_wpa_s->nan_ndi_ndp_refcount) {
		wpas_nan_remove_ndi_local_gtk(ndi_wpa_s);
		wpa_drv_set_operstate(ndi_wpa_s, 0);
	}
}


static int wpas_nan_ndp_connected_cb(void *ctx,
				     struct nan_ndp_connection_params *params)
{
	struct wpa_supplicant *wpa_s = ctx;
	int ret;

	ret = wpas_nan_add_ndi_sta(wpa_s, params);
	if (ret) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to add NDI station for NDP connection");
		return ret;
	}

	wpas_notify_nan_ndp_connected(wpa_s, params->ndp_id.peer_nmi,
				      params->ndp_id.id,
				      params->local_ndi, params->peer_ndi,
				      params->ssi, params->ssi_len,
				      params->interface_id);

       return 0;
}


static void wpas_nan_ndp_disconnected_cb(void *ctx, struct nan_ndp_id *ndp_id,
					 const u8 *local_ndi,
					 const u8 *peer_ndi,
					 enum nan_reason reason,
					 bool locally_generated,
					 bool remove_sta,
					 bool failure, u8 gtk_id)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_nan_remove_ndi_sta(wpa_s, local_ndi, peer_ndi, remove_sta, gtk_id);
	wpas_notify_nan_ndp_disconnected(wpa_s, ndp_id->peer_nmi,
					 ndp_id->id, local_ndi, peer_ndi,
					 reason, locally_generated, failure);
}


static int wpas_nan_send_naf_cb(void *ctx, const u8 *dst, const u8 *src,
				const u8 *cluster_id, struct wpabuf *buf)
{
	struct wpa_supplicant *wpa_s = ctx;
	const u8 *a2;
	int ret;

	a2 = src ? src : wpa_s->own_addr;

	if (src && !ether_addr_equal(src, wpa_s->own_addr)) {
		wpa_printf(MSG_DEBUG, "NAN: Use NDI interface for sending NAF");

		wpa_s = wpas_nan_get_ndi_iface(wpa_s, src);
		if (!wpa_s) {
			wpa_printf(MSG_DEBUG,
				   "NAN: No NDI interface found for address "
				   MACSTR, MAC2STR(src));
			wpa_s = ctx;
		}
	}

	wpa_printf(MSG_DEBUG, "NAN: Send NAF - dst=" MACSTR " src=" MACSTR
		   " cluster_id=" MACSTR, MAC2STR(dst), MAC2STR(a2),
		   MAC2STR(cluster_id));

	ret = wpa_drv_send_action(wpa_s, 0, 0, dst, a2, cluster_id,
				  wpabuf_head(buf), wpabuf_len(buf), 1);
	if (ret)
		wpa_printf(MSG_DEBUG,
			  "NAN: Failed to send sync Action frame (%d)", ret);
	return ret;
}


static int nan_chan_info_cmp(const void *a, const void *b)
{
	const struct nan_channel_info *chan_a = a;
	const struct nan_channel_info *chan_b = b;

	return chan_b->pref - chan_a->pref;
}


static int wpas_nan_get_chans_cb(void *ctx, u8 map_id,
				 struct nan_channels *chans)
{
	struct wpa_supplicant *wpa_s = ctx;
	int *shared_freqs = NULL;
	struct nan_channel_info *chan_list = NULL;
	unsigned int chan_count = 0;
	unsigned int chan_capacity = 0;
	int op;

	wpa_printf(MSG_DEBUG, "NAN: Get channels - map_id=%u", map_id);

	/* Check if override is configured */
	if (wpa_s->nan_override_potential_avail.n_chans > 0) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Using override potential availability (%u channels)",
			   wpa_s->nan_override_potential_avail.n_chans);

		chans->n_chans = wpa_s->nan_override_potential_avail.n_chans;
		chans->chans = os_memdup(
			wpa_s->nan_override_potential_avail.chans,
			wpa_s->nan_override_potential_avail.n_chans *
			sizeof(struct nan_channel_info));
		if (!chans->chans) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to allocate memory for override channels");
			chans->n_chans = 0;
			return -1;
		}

		return 0;
	}

	/* Allocate one extra element so it will be 0 terminated int_array */
	shared_freqs = os_calloc(wpa_s->num_multichan_concurrent + 1,
				 sizeof(int));
	if (!shared_freqs) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to allocate memory for shared freqs");
		goto fail;
	}

	if (get_shared_radio_freqs(wpa_s, shared_freqs,
				   wpa_s->num_multichan_concurrent,
				   false) < 0) {
		wpa_printf(MSG_DEBUG, "NAN: Failed to get shared radio freqs");
		goto fail;
	}

	/* Iterate through global operating classes */
	for (op = 0; global_op_class[op].op_class; op++) {
		const struct oper_class_map *o = &global_op_class[op];
		int c;

		 /* Don't support 80+, 6 GHz, etc. yet */
		if (o->op_class > 129)
			continue;

		/* Iterate through channels in this operating class */
		for (c = o->min_chan; c <= o->max_chan; c += o->inc) {
			int freq;
			u8 center;
			u8 pref;

			/* Don't support 40 MHz channels on 2.4 GHz band */
			if (o->mode == HOSTAPD_MODE_IEEE80211G && o->bw != BW20)
				continue;

			if (!wpas_nan_valid_chan(wpa_s, o->mode, c, o->bw,
						 o->op_class, &center))
				continue;

			freq = ieee80211_chan_to_freq(NULL, o->op_class, c);
			if (freq < 0)
				continue;

			/* Determine preference based on shared frequencies */
			if (int_array_includes(shared_freqs, freq))
				pref = 3;
			else
				pref = 1;

			/* Expand channel list if needed */
			if (chan_count >= chan_capacity) {
				unsigned int new_capacity = chan_capacity ?
					chan_capacity * 2 : 16;
				struct nan_channel_info *new_list;

				new_list = os_realloc_array(
					chan_list, new_capacity,
					sizeof(struct nan_channel_info));
				if (!new_list) {
					wpa_printf(MSG_INFO,
						   "NAN: Failed to expand channel list");
					goto fail;
				}
				chan_list = new_list;
				chan_capacity = new_capacity;
			}

			/* Add channel to list */
			chan_list[chan_count].op_class = o->op_class;
			chan_list[chan_count].channel =
				o->bw == BW80 || o->bw == BW160 ? center : c;
			chan_list[chan_count].pref = pref;
			chan_count++;
		}
	}

	/* Sort channels by preference (higher preference first) */
	if (chan_count > 1)
		qsort(chan_list, chan_count, sizeof(struct nan_channel_info),
		      nan_chan_info_cmp);

	chans->n_chans = chan_count;
	chans->chans = chan_list;

	os_free(shared_freqs);

	wpa_printf(MSG_DEBUG, "NAN: Get channels completed - found %u channels",
		   chan_count);
	return 0;

fail:
	os_free(shared_freqs);
	os_free(chan_list);
	chans->n_chans = 0;
	chans->chans = NULL;
	return -1;
}


static bool wpas_nan_is_valid_publish_id_cb(void *ctx, u8 instance_id,
					    u8 *service_id)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_printf(MSG_DEBUG, "NAN: Check valid publish ID - instance_id=%u",
		   instance_id);

	return nan_de_is_valid_instance_id(wpa_s->nan_de, instance_id, true,
					   service_id);
}


static void wpas_nan_bootstrap_request_cb(void *ctx, const u8 *peer_nmi,
					  u16 pbm, int handle,
					  u8 requestor_instance_id)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_bootstrap_request(wpa_s, peer_nmi, pbm, handle,
					  requestor_instance_id);
}


static void wpas_nan_bootstrap_completed_cb(void *ctx, const u8 *peer_nmi,
					    u16 pbm, bool success,
					    u8 reason_code, int handle,
					    u8 requestor_instance_id)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (success)
		wpas_notify_nan_bootstrap_success(wpa_s, peer_nmi, pbm, handle,
						  requestor_instance_id);
	else
		wpas_notify_nan_bootstrap_failure(wpa_s, peer_nmi, pbm,
						  reason_code, handle,
						  requestor_instance_id);
}


static void wpas_nan_schedule_changed_cb(void *ctx, const u8 *peer_nmi)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_schedule_changed(wpa_s, peer_nmi);
}


static int wpas_nan_transmit_followup_cb(void *ctx, const u8 *peer_nmi,
					 const struct wpabuf *attrs, int handle,
					 u8 req_instance_id)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (!wpa_s->nan_de)
		return -1;

	return nan_de_transmit(wpa_s->nan_de, handle, NULL, NULL,
			       peer_nmi, req_instance_id, attrs, NULL);
}


static u16 wpas_nan_get_service_bootstrap_methods(void *ctx, int handle)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (!wpa_s->nan_de)
		return 0;

	return nan_de_get_service_bootstrap_methods(wpa_s->nan_de, handle);
}


#ifdef CONFIG_PASN

static int wpas_nan_pasn_send_cb(void *ctx, const u8 *data, size_t data_len)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpa_drv_send_mlme(wpa_s, data, data_len, 0, 0, 0);
}


static int wpas_nan_pasn_auth_status_cb(void *ctx, const u8 *peer_addr,
					int akmp, int cipher, u16 status,
					struct wpa_ptk *ptk, const u8 *nd_pmk)
{
	struct wpa_supplicant *wpa_s = ctx;
	enum wpa_alg alg;
	u8 seq[6];

	wpas_notify_nan_pairing_status(wpa_s, peer_addr, akmp, cipher,
				       status, nd_pmk);

	if (status != WLAN_STATUS_SUCCESS)
		return 0;

	if (!ptk) {
		wpa_printf(MSG_DEBUG,
			   "NAN: No PTK provided after pairing with peer "
			   MACSTR, MAC2STR(peer_addr));
		return -1;
	}

	alg = cipher == WPA_CIPHER_CCMP ? WPA_ALG_CCMP : WPA_ALG_GCMP_256;
	os_memset(seq, 0, sizeof(seq));
	if (wpa_drv_set_key(wpa_s, -1, alg, peer_addr, 0, 1, seq, sizeof(seq),
			    ptk->tk, ptk->tk_len, KEY_FLAG_PAIRWISE_RX_TX)) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to install NM-TK for peer " MACSTR,
			   MAC2STR(peer_addr));
		return -1;
	}

	return 0;
}


static int wpas_nan_update_pairing_credentials_cb(void *ctx, const u8 *nik,
						  size_t nik_len,
						  int cipher_ver,
						  int nik_lifetime, int akmp,
						  const u8 *npk, size_t npk_len)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_dev_ik *ik;

	if (!nik || cipher_ver != NAN_NIRA_CIPHER_VER_128 ||
	    nik_len != NAN_NIK_LEN || !npk || !npk_len) {
		wpa_printf(MSG_DEBUG, "NAN: Invalid NIK/NPK parameters");
		return -1;
	}

	wpa_hexdump_key(MSG_DEBUG, "NAN: Received NIK", nik, nik_len);
	wpa_printf(MSG_DEBUG, "NAN: NIK lifetime=%d cipher_ver=%d",
		   nik_lifetime, cipher_ver);

	/* Check if an identity with the same NIK already exists */
	for (ik = wpa_s->conf->identity; ik; ik = ik->next) {
		if (nik_len == wpabuf_len(ik->dik) &&
		    os_memcmp(nik, wpabuf_head(ik->dik), nik_len) == 0) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Remove previous device identity entry for matching NIK");
			wpa_config_remove_identity(wpa_s->conf, ik->id);
			break;
		}
	}

	/* Create a new device identity entry */
	wpa_printf(MSG_DEBUG,
		   "NAN: Create a new device identity entry for NIK");
	ik = wpa_config_add_identity(wpa_s->conf);
	if (!ik) {
		wpa_printf(MSG_INFO, "NAN: Failed to allocate identity");
		return -1;
	}

	/* Store the NIK as the DIK */
	ik->dik = wpabuf_alloc_copy(nik, nik_len);
	if (!ik->dik)
		goto fail;

	/* Store the NPK as the PMK */
	ik->pmk = wpabuf_alloc_copy(npk, npk_len);
	if (!ik->pmk)
		goto fail;

	/* Store cipher version and AKMP */
	ik->dik_cipher = cipher_ver;
	ik->akmp = akmp;

	wpa_printf(MSG_INFO, "NAN: Stored NIK as device identity (id=%d)",
		   ik->id);

	/* Notify control interface about received NIK */
	wpas_notify_nan_nik_received(wpa_s, nik, nik_len, cipher_ver, akmp,
				     npk, npk_len, nik_lifetime, ik->id);

	return ik->id;

fail:
	wpa_printf(MSG_INFO, "NAN: Failed to store NIK as device identity");
	wpa_config_remove_identity(wpa_s->conf, ik->id);
	return -1;
}


static const struct wpa_dev_ik *
wpas_nan_find_ik_by_nonce_tag(struct wpa_supplicant *wpa_s, const u8 *peer_nmi,
			      const u8 *nonce, const u8 *tag)
{
	struct wpa_dev_ik *ik;
	struct wpabuf *derived_tag;

	if (!nonce || !tag) {
		wpa_printf(MSG_DEBUG, "NAN: Invalid nonce or tag");
		return NULL;
	}

	wpa_printf(MSG_DEBUG, "NAN: Looking up device identity");
	wpa_hexdump(MSG_DEBUG, "NAN: NIRA nonce", nonce, NAN_NIRA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "NAN: NIRA tag", tag, NAN_NIRA_TAG_LEN);

	/* Iterate over all saved NIKs (stored as device identities) */
	for (ik = wpa_s->conf->identity; ik; ik = ik->next) {
		/* The device identities saved in the interface configuration
		 * are not checked to match NIK length and to have a PMK.
		 * Although other identities are not expected since this is the
		 * NAN management interface, verify that the DIK matches NIK
		 * length, that a PMK is stored, and the stored AKMP is valid
		 * for NAN pairing.
		 */
		if (!ik->dik || wpabuf_len(ik->dik) != NAN_NIK_LEN ||
		    !ik->pmk ||
		    (ik->akmp != WPA_KEY_MGMT_SAE &&
		     ik->akmp != WPA_KEY_MGMT_PASN))
			continue;

		/* Derive tag from this NIK */
		derived_tag =
			nan_crypto_derive_nira_tag(wpabuf_head_u8(ik->dik),
						   NAN_NIK_LEN, peer_nmi,
						   nonce);
		if (!derived_tag)
			continue;

		/* Compare derived tag with received tag */
		if (os_memcmp(wpabuf_head(derived_tag), tag,
			      NAN_NIRA_TAG_LEN) != 0) {
			wpabuf_free(derived_tag);
			continue;
		}

		wpa_printf(MSG_DEBUG,
			   "NAN: NIRA validation succeeded with NIK id=%d",
			   ik->id);
		wpabuf_free(derived_tag);
		return ik;
	}

	return NULL;
}


static const struct wpabuf * wpas_nan_get_npk_akmp_cb(void *ctx,
						      const u8 *peer_nmi,
						      const u8 *nonce,
						      const u8 *tag, int *akmp)
{
	struct wpa_supplicant *wpa_s = ctx;
	const struct wpa_dev_ik *ik;

	if (!akmp) {
		wpa_printf(MSG_DEBUG, "NAN: Invalid akmp pointer");
		return NULL;
	}

	ik = wpas_nan_find_ik_by_nonce_tag(wpa_s, peer_nmi, nonce, tag);
	if (ik) {
		*akmp = ik->akmp;
		wpa_printf(MSG_DEBUG, "NAN: Found NPK for NIK id=%d, akmp=%d",
			   ik->id, *akmp);
		return ik->pmk;
	}

	wpa_printf(MSG_DEBUG, "NAN: No matching NIK found");
	return NULL;
}


static void
wpas_nan_pasn_pairing_request_cb(void *ctx, const u8 *peer_nmi, u8 csid,
				 u8 instance_id,
				 const struct wpa_ie_data *rsn_data)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_pairing_request(wpa_s, peer_nmi, csid, instance_id,
					rsn_data->key_mgmt,
					!!rsn_data->num_pmkid);
}

#endif /* CONFIG_PASN */


static int wpas_nan_set_group_key_cb(void *ctx, enum wpa_alg alg,
				     const u8 *addr, int key_idx, const u8 *seq,
				     const u8 *key, size_t key_len,
				     enum key_flag key_flags)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpa_drv_set_key(wpa_s, -1, alg, addr, key_idx, 0,
			       seq, RSN_PN_LEN, key, key_len, key_flags);
}


static int wpas_nan_get_seqnum_cb(void *ctx, int key_idx, u8 *seq,
				  const u8 *ndi_addr)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (ndi_addr) {
		wpa_s = wpas_nan_get_ndi_iface(wpa_s, ndi_addr);
		if (!wpa_s) {
			wpa_printf(MSG_DEBUG,
				   "NAN: No NDI interface found for address "
				   MACSTR, MAC2STR(ndi_addr));
			return -1;
		}

		/* If the NDI GTK is not installed yet, RSC is 0 */
		if (!wpa_s->ndi_gtk.id) {
			os_memset(seq, 0, WPA_KEY_RSC_LEN);
			return 0;
		}
	}

	return wpa_drv_get_seqnum(wpa_s, NULL, key_idx, seq);
}


static int wpas_nan_get_peer_inactivity(void *ctx, const u8 *local_ndi,
					const u8 *peer_ndi)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_s = wpas_nan_get_ndi_iface(wpa_s, local_ndi);
	if (!wpa_s) {
		wpa_printf(MSG_DEBUG,
			   "NAN: No NDI interface found for address " MACSTR,
			   MAC2STR(local_ndi));
		return -1;
	}

	return wpa_drv_get_inact_sec(wpa_s, peer_ndi);
}


int wpas_nan_init(struct wpa_supplicant *wpa_s)
{
	struct nan_config nan;

	if (!(wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SUPPORT_NAN) ||
	    !(wpa_s->nan_capa.drv_flags &
	      WPA_DRIVER_FLAGS_NAN_SUPPORT_SYNC_CONFIG)) {
		wpa_printf(MSG_INFO, "NAN: Driver does not support NAN");
		return -1;
	}

	os_memset(&nan, 0, sizeof(nan));
	nan.cb_ctx = wpa_s;
	os_memcpy(nan.nmi_addr, wpa_s->own_addr, ETH_ALEN);

	nan.start = wpas_nan_start_cb;
	nan.stop = wpas_nan_stop_cb;
	nan.update_config = wpas_nan_update_config_cb;

	/* NDP and bootstrapping enabled */
	if (wpa_s->nan_capa.drv_flags & WPA_DRIVER_FLAGS_NAN_SUPPORT_NDP) {
#ifdef CONFIG_PASN
		wpa_printf(MSG_DEBUG, "NAN: Pairing support enabled");
		nan.send_pasn = wpas_nan_pasn_send_cb;
		nan.pairing_result_cb = wpas_nan_pasn_auth_status_cb;
		nan.update_pairing_credentials =
			wpas_nan_update_pairing_credentials_cb;
		nan.get_npk_akmp = wpas_nan_get_npk_akmp_cb;
		nan.pairing_request = wpas_nan_pasn_pairing_request_cb;
		nan.pairing_cfg.pairing_setup = true;
		nan.pairing_cfg.npk_caching = true;
		nan.pairing_cfg.pairing_verification = true;
		nan.pairing_cfg.cipher_suites = NAN_PAIRING_PASN_128 |
			NAN_PAIRING_PASN_256;
#endif /* CONFIG_PASN */

		wpa_printf(MSG_DEBUG, "NAN: NDP support enabled");

		nan.ndp_action_notif = wpas_nan_ndp_action_notif_cb;
		nan.ndp_connected = wpas_nan_ndp_connected_cb;
		nan.ndp_disconnected = wpas_nan_ndp_disconnected_cb;
		nan.send_naf = wpas_nan_send_naf_cb;
		nan.get_chans = wpas_nan_get_chans_cb;
		nan.is_valid_publish_id = wpas_nan_is_valid_publish_id_cb;
		nan.set_peer_schedule = wpas_nan_set_peer_schedule_cb;
		nan.set_group_key = wpas_nan_set_group_key_cb;
		nan.get_seqnum = wpas_nan_get_seqnum_cb;
		nan.schedule_changed = wpas_nan_schedule_changed_cb;

		wpa_printf(MSG_DEBUG, "NAN: Bootstrap support enabled");
		nan.bootstrap_request = wpas_nan_bootstrap_request_cb;
		nan.bootstrap_completed = wpas_nan_bootstrap_completed_cb;
		nan.transmit_followup = wpas_nan_transmit_followup_cb;
		nan.get_supported_bootstrap_methods =
			wpas_nan_get_service_bootstrap_methods;

		if (wpa_s->driver->get_inact_sec)
			nan.get_peer_inactivity = wpas_nan_get_peer_inactivity;
		else
			wpa_printf(MSG_DEBUG,
				   "NAN: Driver does not support getting peer inactivity");

		/*
		 * Set the group security capabilities based on driver support
		 */
		if ((wpa_s->drv_enc & (WPA_DRIVER_CAPA_ENC_CCMP |
				       WPA_DRIVER_CAPA_ENC_GCMP_256)) &&
		    (wpa_s->drv_enc & (WPA_DRIVER_CAPA_ENC_BIP |
				       WPA_DRIVER_CAPA_ENC_BIP_GMAC_256))) {
			/*
			 * By default, use BIP-CMAC-128 cipher suite for
			 * group keys for maximum compatibility.
			 */
			if (!(wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_BIP))
				nan.security_capab |=
					NAN_CS_INFO_CAPA_IGTK_USE_NCS_BIP_GMAC_256;

			/*
			 * By default enable only GTK/IGTK support. Beacon
			 * protection support can be enabled separately
			 */
			nan.security_capab |=
				NAN_CS_INFO_CAPA_GTK_SUPP_NO_BIGTK <<
				NAN_CS_INFO_CAPA_GTK_SUPP_POS;
		}

		wpa_printf(MSG_DEBUG, "NAN: security capabilities=0x%02x",
			   nan.security_capab);
	}

	nan.dev_capa.cdw_info =
		((1 << NAN_CDW_INFO_2G_POS) & NAN_CDW_INFO_2G_MASK) |
		((1 << NAN_CDW_INFO_5G_POS) & NAN_CDW_INFO_5G_MASK);

	/*
	 * Wi-Fi Aware spec v4.0, Table 80 defines the 2.4 GHz and 5 GHz CDW
	 * Override Map ID fields as mandatory without any option to indicate
	 * "applies for all" as in other places in the specification that use
	 * map_ids. At this stage we don't have a local schedule yet, so we will
	 * be referencing non-existent map_id.
	 *
	 * In case of a single radio devices all maps will be using map_id 1,
	 * so we can already configure it. Otherwise, we have no choice but to
	 * leave it unassigned and let this field be updated when the schedule
	 * is configured.
	 *
	 * TODO: Dual radio devices may want to properly query this information
	 * from the driver/device.
	 */
	if (wpa_s->nan_capa.num_radios == 1) {
		nan.dev_capa.cdw_info |= 0x1 << NAN_CDW_INFO_2G_OVERRIDE_POS;
		nan.dev_capa.cdw_info |= 0x1 << NAN_CDW_INFO_5G_OVERRIDE_POS;
	}

	nan.dev_capa.supported_bands = NAN_DEV_CAPA_SBAND_2G;
	if (wpa_s->nan_capa.drv_flags &
	    WPA_DRIVER_FLAGS_NAN_SUPPORT_DUAL_BAND)
		nan.dev_capa.supported_bands |= NAN_DEV_CAPA_SBAND_5G;

	nan.dev_capa.op_mode = wpa_s->nan_capa.op_modes;
	nan.dev_capa.n_antennas = wpa_s->nan_capa.num_antennas;
	nan.dev_capa.channel_switch_time =
		wpa_s->nan_capa.max_channel_switch_time;
	nan.dev_capa.capa = wpa_s->nan_capa.dev_capa;
	nan.dev_capa.capa |= NAN_DEV_CAPA_NDPE_ATTR_SUPP;

	nan.supported_bootstrap_methods = DEFAULT_NAN_SUPP_PBM;
	nan.auto_accept_bootstrap_methods = DEFAULT_NAN_AUTO_ACCEPT_PBM;
	nan.bootstrap_comeback_timeout = DEFAULT_NAN_BOOTSTRAP_COMEBACK_TIMEOUT;

	if (os_get_random(nan.nik, NAN_NIK_LEN) < 0) {
		wpa_printf(MSG_INFO, "NAN: Failed to get random data for NIK");
		return -1;
	}

	nan.nik_lifetime = NAN_NIK_LIFETIME_DEFAULT;
	nan.max_ndl_idle_period = DEFAULT_NAN_MAX_NDL_IDLE_PERIOD;

	wpa_s->nan = nan_init(&nan);
	if (!wpa_s->nan) {
		wpa_printf(MSG_INFO, "NAN: Failed to init");
		return -1;
	}

	/* Set the default configuration */
	os_memset(&wpa_s->nan_cluster_config, 0,
		  sizeof(wpa_s->nan_cluster_config));

	wpa_s->nan_cluster_config.master_pref = DEFAULT_NAN_MASTER_PREF;
	wpa_s->nan_cluster_config.dual_band = DEFAULT_NAN_DUAL_BAND;
	os_memset(wpa_s->nan_cluster_config.cluster_id, 0, ETH_ALEN);
	wpa_s->nan_cluster_config.scan_period = DEFAULT_NAN_SCAN_PERIOD;
	wpa_s->nan_cluster_config.scan_dwell_time = DEFAULT_NAN_SCAN_DWELL_TIME;
	wpa_s->nan_cluster_config.discovery_beacon_interval =
		DEFAULT_NAN_DISCOVERY_BEACON_INTERVAL;

	wpa_s->nan_cluster_config.low_band_cfg.frequency =
		DEFAULT_NAN_LOW_BAND_FREQUENCY;
	wpa_s->nan_cluster_config.low_band_cfg.rssi_close =
		DEFAULT_NAN_RSSI_CLOSE;
	wpa_s->nan_cluster_config.low_band_cfg.rssi_middle =
		DEFAULT_NAN_RSSI_MIDDLE;
	wpa_s->nan_cluster_config.low_band_cfg.awake_dw_interval = true;

	wpa_s->nan_cluster_config.high_band_cfg.frequency =
		DEFAULT_NAN_HIGH_BAND_FREQUENCY;
	wpa_s->nan_cluster_config.high_band_cfg.rssi_close =
		DEFAULT_NAN_RSSI_CLOSE;
	wpa_s->nan_cluster_config.high_band_cfg.rssi_middle =
		DEFAULT_NAN_RSSI_MIDDLE;
	wpa_s->nan_cluster_config.high_band_cfg.awake_dw_interval = true;

	/* TODO: Optimize this, so that the notification are enabled only when
	 * needed, i.e., when the DE is configured with unsolicited publish or
	 * active subscribe
	 */
	wpa_s->nan_cluster_config.enable_dw_notif =
		!!(wpa_s->nan_capa.drv_flags &
		   WPA_DRIVER_FLAGS_NAN_SUPPORT_USERSPACE_DE);

	wpa_s->nan_supported_csids = BIT(NAN_CS_SK_CCM_128) |
		BIT(NAN_CS_SK_GCM_256);
#ifdef CONFIG_PASN
	wpa_s->nan_supported_csids |= BIT(NAN_CS_PK_PASN_128) |
		BIT(NAN_CS_PK_PASN_256);
#endif /* CONFIG_PASN */

	return 0;
}


void wpas_nan_deinit(struct wpa_supplicant *wpa_s)
{
	int i;

	if (!wpa_s || !wpa_s->nan)
		return;

	for (i = 0; i < MAX_NAN_RADIOS; i++)
		clear_sched_config(&wpa_s->nan_sched[i]);

	nan_deinit(wpa_s->nan);
	os_free(wpa_s->nan_disallowed_freqs.range);
	os_memset(&wpa_s->nan_disallowed_freqs, 0,
		  sizeof(wpa_s->nan_disallowed_freqs));
	clear_sched_config(&wpa_s->nan_sched_update.sched);
	wpabuf_free(wpa_s->nan_ulw_attr);
	wpa_s->nan_ulw_attr = NULL;

	os_free(wpa_s->nan_override_potential_avail.chans);
	wpa_s->nan_override_potential_avail.chans = NULL;
	wpa_s->nan_override_potential_avail.n_chans = 0;

	wpa_s->nan = NULL;
}


static bool wpas_nan_ready(struct wpa_supplicant *wpa_s)
{
	return wpa_s->nan_mgmt && wpa_s->nan && wpa_s->nan_de &&
		wpa_s->wpa_state != WPA_INTERFACE_DISABLED;
}


static bool wpas_nan_ndp_allowed(struct wpa_supplicant *wpa_s)
{
	return wpas_nan_ready(wpa_s) &&
		(wpa_s->nan_capa.drv_flags & WPA_DRIVER_FLAGS_NAN_SUPPORT_NDP);
}


/* Join a cluster using current configuration */
int wpas_nan_start(struct wpa_supplicant *wpa_s)
{
	if (!wpas_nan_ready(wpa_s))
		return -1;

	return nan_start(wpa_s->nan, &wpa_s->nan_cluster_config);
}


int wpas_nan_stop(struct wpa_supplicant *wpa_s)
{
	if (!wpas_nan_ready(wpa_s))
		return -1;

	nan_stop(wpa_s->nan);

	return 0;
}


void wpas_nan_flush(struct wpa_supplicant *wpa_s)
{
	if (!wpas_nan_ready(wpa_s))
		return;

	nan_flush(wpa_s->nan);
}


static int wpas_nan_parse_override_potential_avail(struct wpa_supplicant *wpa_s,
						   char *param)
{
	struct nan_channel_info *chans = NULL;
	unsigned int n_chans = 0, capacity = 0;
	char *pos, *end;

	/* Empty string clears the override */
	if (*param == '\0') {
		wpa_printf(MSG_DEBUG,
			   "NAN: Clearing override potential availability");
		goto out;
	}

	/* Parse format: <op_class:0xbitmap:pref>,... */
	pos = param;
	while (pos && *pos) {
		u8 op_class, pref;
		u16 bitmap;
		const struct oper_class_map *o = NULL;
		int op, idx;

		if (sscanf(pos, "%hhu:0x%hx:%hhu", &op_class, &bitmap, &pref) !=
		    3) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid override_potential_availability format at '%s'",
				   pos);
			os_free(chans);
			return -1;
		}

		if (!op_class || op_class > 129 || pref > 3) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid values in override_potential_availability");
			os_free(chans);
			return -1;
		}

		/* Find the operating class in global_op_class */
		for (op = 0; global_op_class[op].op_class; op++) {
			if (global_op_class[op].op_class == op_class) {
				o = &global_op_class[op];
				break;
			}
		}

		if (!o) {
			wpa_printf(MSG_INFO,
				   "NAN: Unknown operating class %d in override_potential_availability",
				   op_class);
			os_free(chans);
			return -1;
		}

		/* Iterate through bitmap bits */
		for (idx = 0; idx < 16 && bitmap; idx++) {
			u8 chan, center;

			if (!(bitmap & BIT(idx)))
				continue;

			chan = op_class_idx_to_chan(o, idx);
			if (!chan) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid channel index %d for op_class %d",
					   idx, op_class);
				os_free(chans);
				return -1;
			}

			/*
			 * Validate the channel. For zero preference only
			 * check the very basic validity, but accept
			 * "NOT ALLOWED" channels, as the user might want
			 * to explicitly mark them as unavailable.
			 */
			if (pref && !wpas_nan_valid_chan(wpa_s, o->mode, chan,
							 o->bw, o->op_class,
							 &center)) {
				wpa_printf(MSG_INFO,
					   "NAN: Channel %d (op_class %d) is not a valid NAN channel",
					   chan, op_class);
				os_free(chans);
				return -1;
			}

			if (!pref) {
				int width;

				center = get_center_and_width(o->bw, chan,
							      &width);
				if (!center) {
					wpa_printf(MSG_INFO,
						   "NAN: Invalid channel %d for op_class %d",
						   chan, op_class);
					os_free(chans);
					return -1;
				}
			}

			/* Expand array if needed */
			if (n_chans >= capacity) {
				struct nan_channel_info *new_chans;

				capacity = capacity ? capacity * 2 : 4;
				new_chans = os_realloc_array(chans, capacity,
							     sizeof(*chans));
				if (!new_chans) {
					wpa_printf(MSG_INFO,
						   "NAN: Memory allocation failed");
					os_free(chans);
					return -1;
				}
				chans = new_chans;
			}

			/* Use center for wide channels */
			chans[n_chans].op_class = op_class;
			chans[n_chans].channel = (o->bw == BW80 ||
						  o->bw == BW160) ?
				center : chan;
			chans[n_chans].pref = pref;
			n_chans++;
		}

		/* Move to next entry */
		end = os_strchr(pos, ',');
		if (end)
			pos = end + 1;
		else
			break;
	}

	/* Sort channels by preference (higher preference first) */
	if (n_chans > 1)
		qsort(chans, n_chans, sizeof(*chans), nan_chan_info_cmp);

out:
	/* Free previous configuration */
	os_free(wpa_s->nan_override_potential_avail.chans);
	wpa_s->nan_override_potential_avail.chans = chans;
	wpa_s->nan_override_potential_avail.n_chans = n_chans;
	wpa_s->schedule_sequence_id++;

	wpa_printf(MSG_DEBUG,
		   "NAN: Configured %u override potential availability channels",
		   n_chans);
	return 0;
}


int wpas_nan_set(struct wpa_supplicant *wpa_s, char *cmd)
{
	struct nan_cluster_config *config = &wpa_s->nan_cluster_config;
	char *param = os_strchr(cmd, ' ');

	if (!param)
		return -1;

	*param++ = '\0';

#define NAN_PARSE_INT(_str, _min, _max)				     \
	if (os_strcmp(#_str, cmd) == 0) {			     \
		int val = atoi(param);                               \
								     \
		if (val < (_min) || val > (_max)) {                  \
			wpa_printf(MSG_INFO,                         \
				   "NAN: Invalid value for " #_str); \
			return -1;                                   \
		}                                                    \
		config->_str = val;                                  \
		return 0;                                            \
	}

#define NAN_PARSE_BAND(_str)						\
	if (os_strcmp(#_str, cmd) == 0) {				\
		int a, b, c, d;						\
									\
		if (sscanf(param, "%d,%d,%d,%d", &a, &b, &c, &d) !=	\
		    4) {						\
			wpa_printf(MSG_DEBUG,				\
				   "NAN: Invalid value for " #_str);	\
			return -1;					\
		}							\
									\
		if (a < NAN_MIN_RSSI_CLOSE ||				\
		    b < NAN_MIN_RSSI_MIDDLE ||				\
		    a <= b) {						\
			wpa_printf(MSG_DEBUG,				\
				   "NAN: Invalid value for " #_str);	\
			return -1;					\
		}							\
		config->_str.rssi_close = a;				\
		config->_str.rssi_middle = b;				\
		config->_str.awake_dw_interval = c;			\
		config->_str.disable_scan = !!d;			\
		return 0;						\
	}

	/* 0 and 255 are reserved */
	NAN_PARSE_INT(master_pref, 1, 254);
	NAN_PARSE_INT(dual_band, 0, 1);
	NAN_PARSE_INT(scan_period, 0, 0xffff);
	NAN_PARSE_INT(scan_dwell_time, 10, 150);
	NAN_PARSE_INT(discovery_beacon_interval, 50, 200);

	NAN_PARSE_BAND(low_band_cfg);
	NAN_PARSE_BAND(high_band_cfg);

	if (os_strcmp("cluster_id", cmd) == 0) {
		u8 cluster_id[ETH_ALEN];

		if (hwaddr_aton(param, cluster_id) < 0) {
			wpa_printf(MSG_INFO, "NAN: Invalid cluster ID");
			return -1;
		}

		if (cluster_id[0] != 0x50 || cluster_id[1] != 0x6f ||
		    cluster_id[2] != 0x9a || cluster_id[3] != 0x01) {
			wpa_printf(MSG_DEBUG, "NAN: Invalid cluster ID format");
			return -1;
		}

		os_memcpy(config->cluster_id, cluster_id, ETH_ALEN);
		return 0;
	}

	if (os_strcmp("max_bw", cmd) == 0) {
		wpa_s->nan_max_bw = atoi(param);
		return 0;
	}

	if (os_strcmp("disallowed_freqs", cmd) == 0) {
		if (freq_range_list_parse(&wpa_s->nan_disallowed_freqs,
					  param)) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid disallowed_freqs value");
			return -1;
		}

		return 0;
	}

	if (os_strcmp("override_potential_availability", cmd) == 0)
		return wpas_nan_parse_override_potential_avail(wpa_s, param);

	if (os_strcmp("bootstrap_config", cmd) == 0) {
		u16 supported_methods, auto_accept_methods, comeback_timeout;

		if (sscanf(param, "%hx,%hx,%hu", &supported_methods,
			   &auto_accept_methods, &comeback_timeout) != 3) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid value for boostrap_config");
			return -1;
		}

		return nan_set_bootstrap_configuration(wpa_s->nan,
						       supported_methods,
						       auto_accept_methods,
						       comeback_timeout);
	}

#undef NAN_PARSE_INT
#undef NAN_PARSE_BAND

#ifdef CONFIG_PASN
#define NAN_PARSE_PAIRING_BOOL(_str)                                 \
	if (os_strcmp(#_str, cmd) == 0) {                            \
		int val = atoi(param);                               \
								     \
		if (val != 0 && val != 1) {                          \
			wpa_printf(MSG_INFO,                         \
				   "NAN: Invalid value for " #_str); \
			return -1;                                   \
		}                                                    \
		return nan_pairing_set_##_str(wpa_s->nan, val);      \
	}

#define NAN_PARSE_PAIRING_INT(_str, _mask)                           \
	if (os_strcmp(#_str, cmd) == 0) {                            \
		unsigned int val = atoi(param);                      \
								     \
		if ((val & (_mask)) != val) {                        \
			wpa_printf(MSG_INFO,                         \
				   "NAN: Invalid value for " #_str); \
			return -1;                                   \
		}                                                    \
		return nan_pairing_set_##_str(wpa_s->nan, val);      \
	}

	NAN_PARSE_PAIRING_BOOL(pairing_setup);
	NAN_PARSE_PAIRING_BOOL(npk_caching);
	NAN_PARSE_PAIRING_BOOL(pairing_verification);
	NAN_PARSE_PAIRING_INT(cipher_suites,
			      NAN_PAIRING_PASN_128 | NAN_PAIRING_PASN_256);
#undef NAN_PARSE_PAIRING_BOOL
#undef NAN_PARSE_PAIRING_INT

	if (os_strcmp("nik", cmd) == 0) {
		u8 nik[NAN_NIK_LEN];
		int res;

		/* Parse NIK value (hex string) */
		if (hexstr2bin(param, nik, NAN_NIK_LEN) < 0) {
			wpa_printf(MSG_INFO, "NAN: Invalid NIK format");
			return -1;
		}

		res = nan_pairing_set_nik(wpa_s->nan, nik, NAN_NIK_LEN);
		forced_memzero(nik, NAN_NIK_LEN);
		return res;
	}

	if (os_strcmp("nik_lifetime", cmd) == 0) {
		u32 lifetime = atoi(param);

		if (lifetime == 0) {
			wpa_printf(MSG_INFO, "NAN: Invalid NIK lifetime");
			return -1;
		}

		return nan_pairing_set_nik_lifetime(wpa_s->nan, lifetime);
	}
#endif /* CONFIG_PASN */

	if (os_strcmp("mgmt_group_cipher", cmd) == 0) {
		int cipher;

		if (os_strcmp(param, "BIP-CMAC-128") == 0) {
			if (!(wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_BIP)) {
				wpa_printf(MSG_INFO,
					   "NAN: BIP-CMAC-128 not supported by the driver");
				return -1;
			}

			cipher = WPA_CIPHER_AES_128_CMAC;
		} else if (os_strcmp(param, "BIP-GMAC-256") == 0) {
			if (!(wpa_s->drv_enc &
			      WPA_DRIVER_CAPA_ENC_BIP_GMAC_256)) {
				wpa_printf(MSG_INFO,
					   "NAN: BIP-CMAC-256 not supported by the driver");
				return -1;
			}

			cipher = WPA_CIPHER_BIP_GMAC_256;
		} else {
			wpa_printf(MSG_INFO,
				   "NAN: Unsupported mgmt_group_cipher value");
			return -1;
		}

		return nan_set_mgmt_group_cipher(wpa_s->nan, cipher);
	}

	if (os_strcmp("beacon_prot", cmd) == 0) {
		bool val = !!atoi(param);

		if (val && !(wpa_s->nan_capa.drv_flags &
			     WPA_DRIVER_FLAGS_NAN_SUPPORT_BEACON_PROT)) {
			wpa_printf(MSG_INFO,
				   "NAN: Beacon protection not supported by the driver");
			return -1;
		}

		if (nan_set_beacon_prot(wpa_s->nan, val) < 0)
			return -1;

		return 0;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (os_strcmp("tx_mcast_follow_up_prot", cmd) == 0) {
		bool val = !!atoi(param);

		nan_de_set_tx_mcast_follow_up_prot(wpa_s->nan_de, val);
		return 0;
	}

	if (os_strcmp("force_conditional_sched", cmd) == 0) {
		wpa_s->nan_force_conditional_sched = !!atoi(param);
		return 0;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (os_strcmp("max_ndl_idle_period", cmd) == 0) {
		u16 max_ndl_idle_period = atoi(param) & 0xffff;

		return nan_set_max_ndl_idle_period(wpa_s->nan,
						   max_ndl_idle_period);
	}

	wpa_printf(MSG_INFO, "NAN: Unknown NAN_SET cmd='%s'", cmd);
	return -1;
}


int wpas_nan_update_conf(struct wpa_supplicant *wpa_s)
{
	if (!wpas_nan_ready(wpa_s))
		return -1;

	wpa_printf(MSG_DEBUG, "NAN: Update NAN configuration");
	return nan_update_config(wpa_s->nan, &wpa_s->nan_cluster_config);
}


static u8 nan_select_40mhz_channel(u8 chan, u8 *op_class, int *bw)
{
	int op;

	for (op = 0; global_op_class[op].op_class; op++) {
		const struct oper_class_map *o = &global_op_class[op];
		int c;

		/* No support for 40 MHz on 2.4 GHz */
		if (o->mode != HOSTAPD_MODE_IEEE80211A)
			continue;

		/* Currently don't support NAN for 80+, 6 GHz, etc. */
		if (o->op_class > 129)
			continue;

		if (o->bw != BW40MINUS && o->bw != BW40PLUS)
			continue;

		for (c = o->min_chan; c <= o->max_chan; c += o->inc) {
			if (c != chan)
				continue;

			*op_class = o->op_class;
			*bw = o->bw;
			if (o->bw == BW40MINUS)
				return chan - 2;
			else
				return chan + 2;
		}
	}

	return 0;
}


static int wpas_nan_select_channel_params(struct wpa_supplicant *wpa_s,
					  int freq, int *center_freq1,
					  int *center_freq2, int *bandwidth)
{
	u8 chan, op_class, center;
	enum hostapd_hw_mode mode;
	int bw;

	mode = ieee80211_freq_to_channel_ext(freq, 0, CONF_OPER_CHWIDTH_USE_HT,
					     &op_class, &chan);
	if (mode == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG, "NAN: Invalid frequency %d", freq);
		return -1;
	}

	if (!wpas_nan_valid_chan(wpa_s, mode, chan, BW20, op_class, &center)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Channel not valid for NAN (freq = %d)",
			   freq);
		return -1;
	}

	/* On 2.4 GHz use 20 MHz channels */
	if (freq >= 2412 && freq <= 2484)
		goto out;

	/* TODO: Add support for NAN on other bands */
	if (freq < 5180 || freq > 5885) {
		wpa_printf(MSG_DEBUG, "NAN: Unsupported frequency %d", freq);
		return -1;
	}

	if (wpas_nan_valid_chan(wpa_s, mode, chan, BW160, 129, &center)) {
		*center_freq1 = ieee80211_chan_to_freq(NULL, op_class, center);
		*center_freq2 = 0;
		*bandwidth = 160;
		return 0;
	}

	if (wpas_nan_valid_chan(wpa_s, mode, chan, BW80, 128, &center)) {
		*center_freq1 = ieee80211_chan_to_freq(NULL, op_class, center);
		*center_freq2 = 0;
		*bandwidth = 80;
		return 0;
	}

	if (nan_select_40mhz_channel(chan, &op_class, &bw) &&
		wpas_nan_valid_chan(wpa_s, mode, center, bw, op_class,
				    &center)) {
		*center_freq1 = ieee80211_chan_to_freq(NULL, op_class,
						       center);
		*center_freq2 = 0;
		*bandwidth = 40;
		return 0;
	}

out:
	/* Fallback to 20 MHz */
	*center_freq1 = freq;
	*center_freq2 = 0;
	*bandwidth = 20;
	return 0;
}


static void nan_dump_sched_config(const char *title,
				  struct nan_schedule_config *sched_cfg)
{
	int i;

	wpa_printf(MSG_DEBUG, "%s: num_channels=%d", title,
		   sched_cfg->num_channels);
	for (i = 0; i < sched_cfg->num_channels; i++) {
		wpa_printf(MSG_DEBUG,
			   "  Channel %d: freq=%d center_freq1=%d center_freq2=%d bandwidth=%d time_bitmap_len=%zu",
			   i + 1,
			   sched_cfg->channels[i].freq,
			   sched_cfg->channels[i].center_freq1,
			   sched_cfg->channels[i].center_freq2,
			   sched_cfg->channels[i].bandwidth,
			   wpabuf_len(sched_cfg->channels[i].time_bitmap));
	}
}


static void wpas_nan_fill_ndp_schedule(struct wpa_supplicant *wpa_s,
				       struct nan_schedule *sched);

static void wpas_nan_update_local_schedule(struct wpa_supplicant *wpa_s)
{
	struct nan_schedule sched;

	wpas_nan_fill_ndp_schedule(wpa_s, &sched);
	nan_local_sched_update(wpa_s->nan, &sched);
}


/* Parse format NAN_SCHED_CONFIG_MAP map_id=<id> [freq:bitmap_hex]..
 * If no bitmaps provided - clear the map */
int wpas_nan_sched_config_map(struct wpa_supplicant *wpa_s, const char *cmd)
{
	struct nan_schedule_config *sched_cfg = &wpa_s->nan_sched_update.sched;
	struct nan_schedule_config old_sched_cfg;
	struct nan_schedule sched;
	char *token, *context = NULL;
	u8 map_id;
	char *pos;
	int *shared_freqs;
	int shared_freqs_count, unused_freqs_count, ret = -1;
	struct bitfield *bf_total;
	unsigned int expected_bitmap_len;
	bool cdw_overwrite_2g = false, cdw_overwrite_5g = false;

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	if (sched_cfg->deferred) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Previous schedule update is still pending");
		return -1;
	}

	if (os_strncmp(cmd, "map_id=", 7) != 0) {
		wpa_printf(MSG_INFO, "NAN: Invalid schedule map format");
		return -1;
	}

	map_id = atoi(cmd + 7);

	if (!map_id || map_id >= MAX_NAN_RADIOS) {
		wpa_printf(MSG_INFO, "NAN: Invalid map_id %d", map_id);
		return -1;
	}

	if (map_id > wpa_s->nan_capa.num_radios) {
		wpa_printf(MSG_INFO,
			   "NAN: map_id %d exceeds number of supported NAN radios %d",
			   map_id, wpa_s->nan_capa.num_radios);
		return -1;
	}

	if (!wpa_s->nan_capa.schedule_period ||
	    !wpa_s->nan_capa.slot_duration) {
		    wpa_printf(MSG_INFO,
			       "NAN: Driver doesn't advertise support for NAN scheduling");
		    return -1;
	}

	expected_bitmap_len = (wpa_s->nan_capa.schedule_period /
			       wpa_s->nan_capa.slot_duration + 7) / 8;

	os_memset(sched_cfg, 0, sizeof(*sched_cfg));

	pos = os_strchr(cmd + 7, ' ');
	if (!pos) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing freq:timebitmap pairs - cleanup schedule");
		ret = wpa_drv_nan_config_schedule(wpa_s, map_id, sched_cfg);
		if (!ret) {
			clear_sched_config(&wpa_s->nan_sched[map_id - 1]);
			wpas_nan_update_local_schedule(wpa_s);
		}

		return ret;
	}

	shared_freqs = os_calloc(wpa_s->num_multichan_concurrent,
				 sizeof(int));
	if (!shared_freqs) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to allocate memory for shared freqs");
		return -1;
	}

	shared_freqs_count =
		get_shared_radio_freqs(wpa_s, shared_freqs,
				       wpa_s->num_multichan_concurrent,
				       false);

	unused_freqs_count = wpa_s->nan_capa.sched_chans - shared_freqs_count;

	bf_total = bitfield_alloc(wpa_s->nan_capa.schedule_period /
				  wpa_s->nan_capa.slot_duration);
	if (!bf_total) {
		wpa_printf(MSG_INFO,
			  "NAN: Failed to allocate bitfield for total schedule");
		goto out;
	}

	/* Parse freq:timebitmap pairs and optional CDW overwrite flags */
	pos++;
	while ((token = str_token(pos, " ", &context))) {
		if (os_strcmp(token, "cdw_overwrite_low_band") == 0) {
			cdw_overwrite_2g = true;
			continue;
		}
		if (os_strcmp(token, "cdw_overwrite_high_band") == 0) {
			cdw_overwrite_5g = true;
			continue;
		}
		int j, i = sched_cfg->num_channels;
		struct bitfield *bf_chan = NULL;
		char *colon = os_strchr(token, ':');
		struct nan_sched_chan chan;
		struct nan_chan_entry *chan_entry;

		if (i >= wpa_s->nan_capa.sched_chans) {
			wpa_printf(MSG_INFO,
				   "NAN: Exceeded max channels per radio %u",
				   wpa_s->nan_capa.sched_chans);
			goto out;
		}

		if (!colon) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid freq:timebitmap format");
			goto out;
		}

		sched_cfg->channels[i].freq = atoi(token);
		if (sched_cfg->channels[i].freq <= 0) {
			wpa_printf(MSG_INFO, "NAN: Invalid frequency %d",
				   sched_cfg->channels[i].freq);
			goto out;
		}

		for (j = 0; j < i; j++) {
			if (sched_cfg->channels[j].freq ==
			    sched_cfg->channels[i].freq) {
				wpa_printf(MSG_INFO,
					   "NAN: Duplicate frequency %d",
					   sched_cfg->channels[i].freq);
				goto out;
			}
		}

		if (wpas_nan_select_channel_params(
			    wpa_s, sched_cfg->channels[i].freq,
			    &sched_cfg->channels[i].center_freq1,
			    &sched_cfg->channels[i].center_freq2,
			    &sched_cfg->channels[i].bandwidth)) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to select channel params for freq %d",
				   sched_cfg->channels[i].freq);
			goto out;
		}

		if (!int_array_includes(shared_freqs,
					sched_cfg->channels[i].freq)) {
			if (!unused_freqs_count) {
				wpa_printf(MSG_INFO,
					   "NAN: No unused radio frequency available for freq %d",
					   sched_cfg->channels[i].freq);
				goto out;
			}

			unused_freqs_count--;
		}

		sched_cfg->channels[i].time_bitmap =
			wpabuf_parse_bin(colon + 1);
		if (!sched_cfg->channels[i].time_bitmap) {
			wpa_printf(MSG_INFO, "NAN: Invalid time bitmap");
			goto out;
		}

		sched_cfg->num_channels++;

		if (wpabuf_len(sched_cfg->channels[i].time_bitmap) !=
		    expected_bitmap_len) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid bitmap length (%zu) for period=%d, slot length=%d",
				   wpabuf_len(sched_cfg->channels[i].time_bitmap),
				   wpa_s->nan_capa.schedule_period,
				   wpa_s->nan_capa.slot_duration);
			goto out;
		}

		bf_chan = bitfield_alloc_data(
			wpabuf_head(sched_cfg->channels[i].time_bitmap),
			wpabuf_len(sched_cfg->channels[i].time_bitmap));
		if (!bf_chan) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to allocate bitfield for channel schedule");
			goto out;
		}

		if (bitfield_intersects(bf_total, bf_chan)) {
			wpa_printf(MSG_INFO,
				   "NAN: Overlapping time bitmap detected for freq %d",
				   sched_cfg->channels[i].freq);
			bitfield_free(bf_chan);
			goto out;
		}

		/* Extract RX NSS from upper nibble of num_antennas */
		sched_cfg->channels[i].rx_nss =
			(wpa_s->nan_capa.num_antennas >> 4) & 0x0f;

		bitfield_union_in_place(bf_total, bf_chan);
		bitfield_free(bf_chan);

		chan.freq = sched_cfg->channels[i].freq;
		chan.center_freq1 = sched_cfg->channels[i].center_freq1;
		chan.center_freq2 = sched_cfg->channels[i].center_freq2;
		chan.bandwidth = sched_cfg->channels[i].bandwidth;
		chan_entry = (struct nan_chan_entry *)
			&sched_cfg->channels[i].chan_entry;
		if (nan_get_chan_entry(wpa_s->nan, &chan, chan_entry)) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to get channel entry for freq %d",
				   sched_cfg->channels[i].freq);
			goto out;
		}
	}

	sched_cfg->avail_attr = wpabuf_alloc(NAN_AVAIL_ATTR_MAX_LEN);
	if (!sched_cfg->avail_attr) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to allocate memory for Availability attribute");
		ret = -1;
		goto out;
	}

	/* Keep previous schedule configuration as we may need to restore it */
	os_memcpy(&old_sched_cfg, &wpa_s->nan_sched[map_id - 1],
		  sizeof(old_sched_cfg));

	os_memcpy(&wpa_s->nan_sched[map_id - 1], sched_cfg, sizeof(*sched_cfg));
	wpas_nan_fill_ndp_schedule(wpa_s, &sched);

	ret = nan_convert_sched_to_avail_attrs(wpa_s->nan,
					       wpa_s->schedule_sequence_id + 1,
					       BIT(map_id),
					       sched.n_chans, sched.chans,
					       sched_cfg->avail_attr,
					       false);

	/* Restore previous schedule configuration */
	os_memcpy(&wpa_s->nan_sched[map_id - 1], &old_sched_cfg,
		  sizeof(old_sched_cfg));
	if (ret < 0) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to convert schedule to Availability Attributes for map_id %d",
			   map_id);
		goto out;
	}

	if (nan_has_active_ndp(wpa_s->nan)) {
		wpa_printf(MSG_DEBUG, "NAN: Set schedule config as deferred");
		sched_cfg->deferred = true;
		wpa_s->nan_sched_update.map_id = map_id;
		nan_set_sched_update_pending(wpa_s->nan, true);
	}

	nan_dump_sched_config("NAN: Set schedule config", sched_cfg);
	ret = wpa_drv_nan_config_schedule(wpa_s, map_id, sched_cfg);
	if (ret < 0) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to configure NAN schedule map_id %d",
			   map_id);
		os_memcpy(&wpa_s->nan_sched[map_id - 1], &old_sched_cfg,
			  sizeof(old_sched_cfg));
		nan_set_sched_update_pending(wpa_s->nan, false);
		goto out;
	}

	if (!sched_cfg->deferred) {
		/* Store the configured schedule */
		wpa_s->schedule_sequence_id++;
		clear_sched_config(&wpa_s->nan_sched[map_id - 1]);
		os_memcpy(&wpa_s->nan_sched[map_id - 1], sched_cfg,
			  sizeof(*sched_cfg));
		os_memset(sched_cfg, 0, sizeof(*sched_cfg));
		wpas_nan_update_local_schedule(wpa_s);
	}

	/* Update CDW overwrite map_id for the specified band */
	if (cdw_overwrite_2g || cdw_overwrite_5g)
		nan_set_cdw_overwrite(wpa_s->nan,
				      cdw_overwrite_2g ? map_id : -1,
				      cdw_overwrite_5g ? map_id : -1);

out:
	os_free(bf_total);
	os_free(shared_freqs);
	if (ret)
		clear_sched_config(sched_cfg);

	return ret;
}


static struct wpabuf * wpas_nan_build_ndp_elems(struct wpa_supplicant *wpa_s)
{
	struct ieee80211_ht_capabilities *ht_cap;
	struct ieee80211_vht_capabilities *vht_cap;
	size_t len;
	struct wpabuf *buf;

	/* Include HT and VHT Capability elements */
	len = 2 + sizeof(struct ieee80211_ht_capabilities);
	if (wpa_s->nan_capa.vht_valid)
		len += 2 + sizeof(struct ieee80211_vht_capabilities);

	buf = wpabuf_alloc(len);
	if (!buf)
		return NULL;

	wpabuf_put_u8(buf, WLAN_EID_HT_CAP);
	wpabuf_put_u8(buf, sizeof(*ht_cap));
	ht_cap = wpabuf_put(buf, sizeof(*ht_cap));
	ht_cap->ht_capabilities_info = host_to_le16(wpa_s->nan_capa.ht_capab);
	ht_cap->a_mpdu_params = wpa_s->nan_capa.ht_ampdu_params;
	os_memcpy(ht_cap->supported_mcs_set, wpa_s->nan_capa.ht_mcs_set,
		  sizeof(ht_cap->supported_mcs_set));

	if (!wpa_s->nan_capa.vht_valid)
		return buf;

	wpabuf_put_u8(buf, WLAN_EID_VHT_CAP);
	wpabuf_put_u8(buf, sizeof(*vht_cap));
	vht_cap = wpabuf_put(buf, sizeof(*vht_cap));
	vht_cap->vht_capabilities_info =
		host_to_le32(wpa_s->nan_capa.vht_capab);
	os_memcpy(&vht_cap->vht_supported_mcs_set,
		  wpa_s->nan_capa.vht_mcs_set,
		  sizeof(vht_cap->vht_supported_mcs_set));

	/* TODO: Add HE capabilities */
	return buf;
}


static int
wpas_nan_fill_ndp_schedule_chan(struct wpa_supplicant *wpa_s,
				struct nan_schedule *sched, int map_id,
				const struct nan_schedule_channel *chan)
{
	struct nan_chan_schedule *chan_sched;
	struct nan_time_bitmap *tbm;
	const u8 *bitmap_data;
	size_t bitmap_len;

	/* None of these should happen */
	if (!chan->time_bitmap) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing time bitmap for map_id %d freq %d",
			   map_id + 1, chan->freq);
		return -1;
	}

	bitmap_len = wpabuf_len(chan->time_bitmap);
	bitmap_data = wpabuf_head(chan->time_bitmap);
	if (bitmap_len > NAN_TIME_BITMAP_MAX_LEN) {
		wpa_printf(MSG_INFO,
			   "NAN: Time bitmap length %zu exceeds maximum %d",
			   bitmap_len, NAN_TIME_BITMAP_MAX_LEN);
		return -1;
	}

	chan_sched = &sched->chans[sched->n_chans++];
	chan_sched->map_id = map_id + 1;
	chan_sched->chan.freq = chan->freq;
	chan_sched->chan.center_freq1 = chan->center_freq1;
	chan_sched->chan.center_freq2 = chan->center_freq2;
	chan_sched->chan.bandwidth = chan->bandwidth;

	tbm = &chan_sched->committed;
#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->nan_force_conditional_sched) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Using conditional TBM for schedule channel");
		tbm = &chan_sched->conditional;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	tbm->duration = wpa_s->nan_capa.slot_duration >> 5;
	tbm->period = ffs(wpa_s->nan_capa.schedule_period) - 7;
	tbm->offset = 0;
	tbm->len = bitmap_len;
	os_memcpy(tbm->bitmap, bitmap_data, bitmap_len);

	wpa_printf(MSG_DEBUG,
		   "NAN: NDP schedule channel added: map_id=%d freq=%d center_freq1=%d center_freq2=%d bandwidth=%d",
		   chan_sched->map_id,
		   chan_sched->chan.freq,
		   chan_sched->chan.center_freq1,
		   chan_sched->chan.center_freq2,
		   chan_sched->chan.bandwidth);

	return 0;
}


static void wpas_nan_fill_ndp_schedule(struct wpa_supplicant *wpa_s,
				       struct nan_schedule *sched)
{
	int map_id;

	os_memset(sched, 0, sizeof(*sched));

	/* Fill the NAN schedule structure from the schedule config */
	for (map_id = 0; map_id < MAX_NAN_RADIOS; map_id++) {
		int i;
		struct nan_schedule_config *sched_cfg =
			&wpa_s->nan_sched[map_id];

		for (i = 0; i < wpa_s->nan_sched[map_id].num_channels; i++) {
			struct nan_schedule_channel *chan;

			chan = &sched_cfg->channels[i];
			if (wpas_nan_fill_ndp_schedule_chan(wpa_s, sched,
							    map_id, chan)
			    < 0)
				return;
		}
	}

	/* Mark all supported radios - for potential availability */
	sched->map_ids_bitmap = (BIT(wpa_s->nan_capa.num_radios) - 1) << 1;
}


static int wpas_nan_get_ndc_map_id(struct wpa_supplicant *wpa_s,
				   const struct nan_peer_schedule *peer_sched,
				   u8 peer_map_id)
{
	int i;
	int freq = nan_get_peer_ndc_freq(wpa_s->nan, peer_sched, peer_map_id);

	if (freq < 0) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Failed to get NDC frequency from peer schedule");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "NAN: Peer NDC frequency is %d MHz", freq);

	for (i = 0; i < MAX_NAN_RADIOS; i++) {
		struct nan_schedule_config *sched_cfg = &wpa_s->nan_sched[i];
		int j;

		for (j = 0; j < sched_cfg->num_channels; j++) {
			if (sched_cfg->channels[j].freq == freq) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Found local NDC map_id %d for peer NDC freq %d",
					   i + 1, freq);
				return i + 1;
			}
		}
	}

	return -1;
}



static int wpas_nan_select_ndc_copy_peers(struct wpa_supplicant *wpa_s,
					  struct nan_ndp_params *ndp)
{
	struct nan_peer_schedule peer_sched;
	int ret;
	u8 map_id;

	wpa_printf(MSG_DEBUG, "NAN: NDP CONF - use the NDC from peer");
	ret = nan_peer_get_schedule_info(wpa_s->nan, ndp->ndp_id.peer_nmi,
					 &peer_sched);
	if (ret) {
		wpa_printf(MSG_DEBUG, "NAN: Failed to get peer schedule info");
		return -1;
	}

	for (map_id = 0; map_id < peer_sched.n_maps; map_id++) {
		if (peer_sched.maps[map_id].ndc.len) {
			ret = wpas_nan_get_ndc_map_id(wpa_s, &peer_sched,
						      map_id);
			if (ret < 0) {
				wpa_printf(MSG_DEBUG,
					   "NAN: No local NDC map_id found for peer NDC");
				return -1;
			}

			ndp->sched.ndc_map_id = ret;
			os_memcpy(&ndp->sched.ndc, &peer_sched.maps[map_id].ndc,
				  sizeof(ndp->sched.ndc));
			return 0;
		}
	}

	wpa_printf(MSG_DEBUG, "NAN: No NDC found in peer schedule");
	return -1;
}


static int wpas_nan_select_ndc(struct wpa_supplicant *wpa_s,
			       struct nan_ndp_params *ndp)
{
	struct nan_time_bitmap *tbm;
	int i;

	/* NDC attribute in request is optional, let the peer decide */
	if (ndp->type == NAN_NDP_ACTION_REQ)
		return 0;

	/* For successfull confirm, copy peer's NDC */
	if (ndp->type == NAN_NDP_ACTION_CONF &&
	    ndp->u.resp.status == NAN_NDP_STATUS_ACCEPTED)
		return wpas_nan_select_ndc_copy_peers(wpa_s, ndp);

	tbm = &ndp->sched.chans[0].committed;
#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->nan_force_conditional_sched) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Using conditional TBM for NDC selection");
		tbm = &ndp->sched.chans[0].conditional;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	os_memcpy(&ndp->sched.ndc, tbm, sizeof(ndp->sched.ndc));
	os_memset(ndp->sched.ndc.bitmap, 0, sizeof(ndp->sched.ndc.bitmap));
	ndp->sched.ndc_map_id = ndp->sched.chans[0].map_id;

	/*
	 * For default NDC channels (6, 149, 44) take the first slot after DW.
	 * Note that if the slot duration is 16 TUs we need to select the next
	 * slot after DW. If the first channel is not one of default NDC
	 * channels, select the first available slot.
	 */
	if (ndp->sched.chans[0].chan.freq == 5745 ||
	    ndp->sched.chans[0].chan.freq == 5220) {
		int dw_bit, byte_idx, bit_in_byte;

		dw_bit = 128 / wpa_s->nan_capa.slot_duration;
		dw_bit += !!(wpa_s->nan_capa.slot_duration == 16);
		byte_idx = dw_bit / 8;
		bit_in_byte = dw_bit % 8;

		if (tbm->bitmap[byte_idx] & BIT(bit_in_byte)) {
			ndp->sched.ndc.bitmap[byte_idx] = BIT(bit_in_byte);
			return 0;
		}
	} else if (ndp->sched.chans[0].chan.freq == 2437 &&
		   wpa_s->nan_capa.slot_duration == 16) {
		if (tbm->bitmap[0] & 0x02) {
			ndp->sched.ndc.bitmap[0] = 0x02;
			return 0;
		}
	}

	/* For other cases, select the first available slot */
	for (i = 0; i < NAN_TIME_BITMAP_MAX_LEN; i++) {
		if (tbm->bitmap[i]) {
			ndp->sched.ndc.bitmap[i] =
				tbm->bitmap[i] & (~tbm->bitmap[i] + 1);
			break;
		}
	}

	return 0;
}


static int wpas_nan_set_ndp_schedule(struct wpa_supplicant *wpa_s,
				     struct nan_ndp_params *ndp)
{
	/* Set schedule for request or successful response */
	if (ndp->type != NAN_NDP_ACTION_REQ &&
	    ndp->u.resp.status == NAN_NDP_STATUS_REJECTED)
		return 0;

	wpas_nan_fill_ndp_schedule(wpa_s, &ndp->sched);

	if (!ndp->sched.n_chans) {
		wpa_printf(MSG_DEBUG,
			   "NAN: No channels configured for NDP schedule");
		return -1;
	}

	/* Set sequence ID */
	ndp->sched.sequence_id = wpa_s->schedule_sequence_id;

	/* Add additional elements */
	ndp->sched.elems = wpas_nan_build_ndp_elems(wpa_s);

	/* Mark schedule as valid */
	ndp->sched_valid = true;

	return wpas_nan_select_ndc(wpa_s, ndp);
}


static char * wpas_nan_parse_password_hex(const char *hexstr)
{
	size_t len = os_strlen(hexstr);
	size_t pwd_len;
	char *pwd;
	size_t i;

	if (!len || len % 2 != 0) {
		wpa_printf(MSG_INFO, "NAN: Invalid password hex length: %zu",
			   len);
		return NULL;
	}

	pwd_len = len / 2;
	pwd = os_malloc(pwd_len + 1);
	if (!pwd)
		return NULL;

	if (hexstr2bin(hexstr, (u8 *) pwd, pwd_len) < 0) {
		wpa_printf(MSG_INFO, "NAN: Invalid password hex data");
		os_free(pwd);
		return NULL;
	}

	/* Reject passwords containing NULL bytes (except the terminator) */
	for (i = 0; i < pwd_len; i++) {
		if (pwd[i] == '\0') {
			wpa_printf(MSG_DEBUG,
				   "NAN: Decoded password contains embedded NUL byte at offset %zu",
				   i);
			os_free(pwd);
			return NULL;
		}
	}

	pwd[pwd_len] = '\0';
	return pwd;
}


static int wpas_nan_fill_nd_pmk(struct wpa_supplicant *wpa_s,
				struct nan_ndp_params *ndp,
				int handle,
				const u8 *publisher_nmi,
				const char *pwd, const char *pmk)
{
	u8 service_id[NAN_SERVICE_ID_LEN];

	if (ndp->sec.csid < NAN_CS_NONE || ndp->sec.csid >= NAN_CS_MAX) {
		wpa_printf(MSG_INFO, "NAN: Invalid CSID value: %d",
			   ndp->sec.csid);
		return -1;
	}

	/*
	 * Get service ID from the local handle (subscribe on
	 * requester and publish on responder)
	 */
	if (!nan_de_is_valid_instance_id(wpa_s->nan_de, handle,
					 ndp->type == NAN_NDP_ACTION_RESP,
					 service_id)) {
		wpa_printf(MSG_INFO,
			   "NAN: Invalid service instance handle: %d",
			   handle);
		return -1;
	}

	/*
	 * For NDP response (publisher side), check if the requested CSID
	 * is supported by the service (including open/NAN_CS_NONE).
	 */
	if (ndp->type == NAN_NDP_ACTION_RESP &&
	    !nan_de_service_supports_csid(wpa_s->nan_de, handle,
					  ndp->sec.csid)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: CSID %d not supported by service",
			   ndp->sec.csid);
		return -1;
	}

	if (ndp->sec.csid == NAN_CS_NONE)
		return 0;

	/* Security parameters are not needed in confirmation */
	if (ndp->type == NAN_NDP_ACTION_CONF)
		return 0;

	if (!(wpa_s->nan_supported_csids & BIT(ndp->sec.csid))) {
			wpa_printf(MSG_INFO,
				   "NAN: Requested CSID %d not supported",
				   ndp->sec.csid);
			return -1;
	}

	if ((!pwd || os_strlen(pwd) == 0) && (!pmk || os_strlen(pmk) == 0)) {
		wpa_printf(MSG_INFO,
			   "NAN: Password/PMK required for CSID %d",
			   ndp->sec.csid);
		return -1;
	}

	if (pmk) {
		if (os_strlen(pmk) != PMK_LEN * 2) {
			wpa_printf(MSG_INFO, "NAN: Invalid PMK length: %zu",
				   os_strlen(pmk));
			return -1;
		}

		if (hexstr2bin(pmk, ndp->sec.pmk, PMK_LEN) < 0) {
			wpa_printf(MSG_INFO, "NAN: Invalid PMK hex data");
			return -1;
		}

		return 0;
	}

	/* Derive PMK from password */
	return nan_crypto_derive_nd_pmk(pwd, service_id, ndp->sec.csid,
					publisher_nmi, ndp->sec.pmk);
}


static int wpas_nan_set_gtk(struct wpa_supplicant *ndi_wpa_s,
			    struct nan_ndp_params *ndp, int gtk_csid)
{
	if (ndi_wpa_s->ndi_gtk.gtk.gtk_len) {
		if (ndi_wpa_s->ndi_gtk.csid != gtk_csid) {
			wpa_printf(MSG_INFO,
				   "NAN: NDI GTK CSID mismatch (expected %d, got %d)",
				   gtk_csid, ndi_wpa_s->ndi_gtk.csid);
			return -1;
		}

		os_memcpy(&ndp->sec.gtk, &ndi_wpa_s->ndi_gtk,
			  sizeof(ndp->sec.gtk));
		return 0;
	}

	ndp->sec.gtk.csid = gtk_csid;
	if (gtk_csid == NAN_CS_GTK_GCMP_256 &&
	    (ndi_wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_GCMP_256)) {
		ndp->sec.gtk.gtk.gtk_len = 32;
	} else if (gtk_csid == NAN_CS_GTK_CCMP_128 &&
		   (ndi_wpa_s->drv_enc & WPA_DRIVER_CAPA_ENC_CCMP)) {
		ndp->sec.gtk.gtk.gtk_len = 16;
	} else {
		wpa_printf(MSG_INFO,
			   "NAN: NDI does not support GTK cipher suites");
		return -1;
	}

	if (os_get_random(ndp->sec.gtk.gtk.gtk, ndp->sec.gtk.gtk.gtk_len) < 0) {
		wpa_printf(MSG_INFO, "NAN: Failed to generate GTK");
		return -1;
	}

	ndp->sec.gtk.id = 1;

	wpa_hexdump_key(MSG_DEBUG, "NAN: Generated new GTK",
			ndp->sec.gtk.gtk.gtk, ndp->sec.gtk.gtk.gtk_len);
	return 0;
}


/* Command format NAN_NDP_REQUEST handle=<id> ndi=<ifname> peer_nmi=<nmi>
   peer_id=<peer_instance_id> ssi=<hexdata> qos=<slots:latency>
   [csid = <cipher_suite> <password=<string>|pwd_hex=<hex>|pmk=<hex>>
   [gtk_csid=<cipher_suite>]] [interface_id=<hex>] */
int wpas_nan_ndp_request(struct wpa_supplicant *wpa_s, char *cmd)
{
	struct nan_ndp_params ndp;
	struct wpabuf *ssi_buf = NULL;
	char *token, *context = NULL;
	char *pos;
	const char *pwd = NULL, *pmk = NULL, *pwd_hex = NULL;
	char *pwd_decoded = NULL;
	int handle = -1;
	int ret = -1;
	u8 *interface_id = NULL;
	struct wpa_supplicant *ndi_wpa_s = NULL;
	int gtk_csid = 0;

	os_memset(&ndp, 0, sizeof(ndp));

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	ndp.type = NAN_NDP_ACTION_REQ;
	ndp.qos.min_slots = NAN_QOS_MIN_SLOTS_NO_PREF;
	ndp.qos.max_latency = NAN_QOS_MAX_LATENCY_NO_PREF;

	/* Parse command parameters */
	while ((token = str_token(cmd, " ", &context))) {
		pos = os_strchr(token, '=');
		if (!pos) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid parameter format: %s",
				   token);
			goto fail;
		}
		*pos++ = '\0';

		if (os_strcmp(token, "handle") == 0) {
			handle = atoi(pos);

			/* Get service ID from the local handle */
			if (!nan_de_is_valid_instance_id(wpa_s->nan_de,
							 handle, false,
							 ndp.u.req.service_id))
			{
				wpa_printf(MSG_INFO,
					   "NAN: Invalid subscribe handle: %d",
					   handle);
				goto fail;
			}
		} else if (os_strcmp(token, "ndi") == 0) {
			ndi_wpa_s = wpa_supplicant_get_iface(wpa_s->global,
							     pos);
			if (!ndi_wpa_s) {
				wpa_printf(MSG_INFO,
					   "NAN: NDI interface not found: %s",
					   pos);
				goto fail;
			}

			if (!ndi_wpa_s->nan_data) {
				wpa_printf(MSG_INFO,
					   "NAN: Interface %s is not a NAN data interface",
					   pos);
				goto fail;
			}

			os_memcpy(ndp.ndp_id.init_ndi, ndi_wpa_s->own_addr,
				  ETH_ALEN);
		} else if (os_strcmp(token, "peer_nmi") == 0) {
			if (hwaddr_aton(pos, ndp.ndp_id.peer_nmi) < 0) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid peer NMI address: %s",
					   pos);
				goto fail;
			}

		} else if (os_strcmp(token, "peer_id") == 0) {
			ndp.u.req.publish_inst_id = atoi(pos);
		} else if (os_strcmp(token, "ssi") == 0) {
			ssi_buf = wpabuf_parse_bin(pos);
			if (!ssi_buf) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid SSI data: %s", pos);
				goto fail;
			}

			ndp.ssi_len = wpabuf_len(ssi_buf);
			ndp.ssi = wpabuf_head(ssi_buf);
		} else if (os_strcmp(token, "qos") == 0) {
			if (sscanf(pos, "%hhu:%hu",
				   &ndp.qos.min_slots,
				   &ndp.qos.max_latency) != 2) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid QoS parameter: %s",
					   pos);
				goto fail;
			}
		} else if (os_strcmp(token, "csid") == 0) {
			ndp.sec.csid = atoi(pos);
		} else if (os_strcmp(token, "password") == 0) {
			pwd = pos;
		} else if (os_strcmp(token, "pwd_hex") == 0) {
			pwd_hex = pos;
		} else if (os_strcmp(token, "pmk") == 0) {
			pmk = pos;
		} else if (os_strcmp(token, "interface_id") == 0) {
			os_free(interface_id);
			interface_id =
				os_malloc(NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN);
			if (!interface_id)
				goto fail;

			if (hexstr2bin(pos, interface_id,
				       NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN) < 0) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Invalid interface_id hex data: %s",
					   pos);
				goto fail;
			}

			ndp.interface_id = interface_id;
		} else if (os_strcmp(token, "gtk_csid") == 0) {
			gtk_csid = atoi(pos);
			if (gtk_csid != NAN_CS_GTK_CCMP_128 &&
			    gtk_csid != NAN_CS_GTK_GCMP_256) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid GTK CSID value: %d",
					   gtk_csid);
				goto fail;
			}
		} else {
			wpa_printf(MSG_INFO, "NAN: Unknown parameter: %s",
				   token);
			goto fail;
		}
	}

	/* Validate required parameters */
	if (handle < 0) {
		wpa_printf(MSG_INFO, "NAN: Missing required parameter: handle");
		goto fail;
	}

	if (!ndp.u.req.publish_inst_id) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: peer_id");
		goto fail;
	}

	if (is_zero_ether_addr(ndp.ndp_id.init_ndi)) {
		wpa_printf(MSG_INFO, "NAN: Missing required parameter: ndi");
		goto fail;
	}

	if (is_zero_ether_addr(ndp.ndp_id.peer_nmi)) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: peer_nmi");
		goto fail;
	}

	if ((pmk && pwd) || (pmk && pwd_hex) || (pwd && pwd_hex)) {
		wpa_printf(MSG_INFO,
			   "NAN: Specify only one of password, pwd_hex, or pmk");
		goto fail;
	}

	if (pwd_hex) {
		pwd_decoded = wpas_nan_parse_password_hex(pwd_hex);
		if (!pwd_decoded)
			goto fail;
	}

	if (wpas_nan_fill_nd_pmk(wpa_s, &ndp, handle, ndp.ndp_id.peer_nmi,
				 pwd_decoded ? pwd_decoded : pwd, pmk) < 0) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to derive NDP PMK");
		goto fail;
	}

	if (wpas_nan_set_ndp_schedule(wpa_s, &ndp)) {
		wpa_printf(MSG_INFO, "NAN: Failed to set NDP schedule");
		goto fail;
	}

	if (gtk_csid) {
		if (ndp.sec.csid == NAN_CS_NONE || !ndi_wpa_s) {
			wpa_printf(MSG_INFO,
				   "NAN: GTK CSID specified without a valid NDP CSID");
			goto fail;
		}

		if (wpas_nan_set_gtk(ndi_wpa_s, &ndp, gtk_csid) < 0) {
			wpa_printf(MSG_DEBUG, "NAN: Failed to set NDP GTK");
			goto fail;
		}
	}

	wpa_printf(MSG_DEBUG, "NAN: Requesting NDP with peer " MACSTR
		   " using handle %d", MAC2STR(ndp.ndp_id.peer_nmi),
		   ndp.u.req.publish_inst_id);
	ret = nan_handle_ndp_setup(wpa_s->nan, &ndp);
fail:
	wpabuf_free(ndp.sched.elems);
	wpabuf_free(ssi_buf);
	os_free(interface_id);
	str_clear_free(pwd_decoded);

	return ret;
}


int wpas_nan_ndp_response_set_gtk(struct wpa_supplicant *wpa_s,
				  struct wpa_supplicant *ndi_wpa_s,
				  int handle, struct nan_ndp_params *ndp)
{
	int gtk_csid;

	gtk_csid = nan_ndp_requested_gtk_csid(wpa_s->nan, &ndp->ndp_id);
	if (!gtk_csid) {
		wpa_printf(MSG_DEBUG, "NAN: No GTK requested by peer for NDP");
		return 0;
	}

	if (!nan_de_service_supports_csid(wpa_s->nan_de, handle, gtk_csid)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Cannot set GTK - CSID %d not supported by service",
			   gtk_csid);
		return -1;
	}

	return wpas_nan_set_gtk(ndi_wpa_s, ndp, gtk_csid);
}


/* Command format NAN_NDP_RESPONSE accept|reject peer_nmi=<nmi>
   [reason_code=<reject_reason>]
   [ndi=<ifname> handle=<service_handle> init_ndi=<ndi>
   ndp_id=<id> [ssi=<hexdata>] [qos=<slots:latency>]
   [csid=<csid> <password=<string>|pwd_hex=<hex>|pmk=<hex>>]]
   [interface_id=<hex>] */
int wpas_nan_ndp_response(struct wpa_supplicant *wpa_s, char *cmd)
{
	struct nan_ndp_params ndp;
	struct wpabuf *ssi_buf = NULL;
	char *token, *context = NULL;
	char *pos;
	const char *pwd = NULL, *pmk = NULL, *pwd_hex = NULL;
	char *pwd_decoded = NULL;
	int handle = -1;
	int ret = -1;
	u8 *interface_id = NULL;
	struct wpa_supplicant *ndi_wpa_s = NULL;

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	os_memset(&ndp, 0, sizeof(ndp));

	ndp.type = NAN_NDP_ACTION_RESP;
	ndp.qos.min_slots = NAN_QOS_MIN_SLOTS_NO_PREF;
	ndp.qos.max_latency = NAN_QOS_MAX_LATENCY_NO_PREF;

	/* Parse accept/reject status - the first parameter is mandatory */
	token = str_token(cmd, " ", &context);
	if (!token) {
		wpa_printf(MSG_INFO, "NAN: Missing accept/reject parameter");
		return -1;
	}

	if (os_strcmp(token, "accept") == 0) {
		ndp.u.resp.status = NAN_NDP_STATUS_ACCEPTED;
	} else if (os_strcmp(token, "reject") == 0) {
		ndp.u.resp.status = NAN_NDP_STATUS_REJECTED;
	} else {
		wpa_printf(MSG_INFO, "NAN: Invalid accept/reject parameter: %s",
			   token);
		return -1;
	}

	/* Parse optional parameters */
	while ((token = str_token(cmd, " ", &context))) {
		pos = os_strchr(token, '=');
		if (!pos) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid parameter format: %s", token);
			goto fail;
		}
		*pos++ = '\0';

		if (os_strcmp(token, "reason_code") == 0) {
			ndp.u.resp.reason_code = atoi(pos);
		} else if (os_strcmp(token, "ndi") == 0) {
			ndi_wpa_s = wpa_supplicant_get_iface(wpa_s->global,
							     pos);
			if (!ndi_wpa_s) {
				wpa_printf(MSG_INFO,
					   "NAN: NDI interface not found: %s",
					   pos);
				goto fail;
			}

			if (!ndi_wpa_s->nan_data) {
				wpa_printf(MSG_INFO,
					   "NAN: Interface %s is not a NAN data interface",
					   pos);
				goto fail;
			}

			os_memcpy(ndp.u.resp.resp_ndi, ndi_wpa_s->own_addr,
				  ETH_ALEN);
		} else if (os_strcmp(token, "peer_nmi") == 0) {
			if (hwaddr_aton(pos, ndp.ndp_id.peer_nmi) < 0) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid peer NMI address: %s",
					   pos);
				goto fail;
			}
		} else if (os_strcmp(token, "ndp_id") == 0) {
			ndp.ndp_id.id = atoi(pos);
		} else if (os_strcmp(token, "init_ndi") == 0) {
			if (hwaddr_aton(pos, ndp.ndp_id.init_ndi) < 0) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid initiator NDI address: %s",
					   pos);
				goto fail;
			}
		} else if (os_strcmp(token, "ssi") == 0) {
			ssi_buf = wpabuf_parse_bin(pos);
			if (!ssi_buf) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid SSI data: %s", pos);
				goto fail;
			}

			ndp.ssi_len = wpabuf_len(ssi_buf);
			ndp.ssi = wpabuf_head(ssi_buf);
		} else if (os_strcmp(token, "qos") == 0) {
			if (sscanf(pos, "%hhu:%hu",
				   &ndp.qos.min_slots,
				   &ndp.qos.max_latency) != 2) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid QoS parameter: %s",
					   pos);
				goto fail;
			}
		} else if (os_strcmp(token, "handle") == 0) {
			handle = atoi(pos);
		} else if (os_strcmp(token, "csid") == 0) {
			ndp.sec.csid = atoi(pos);
		} else if (os_strcmp(token, "password") == 0) {
			pwd = pos;
		} else if (os_strcmp(token, "pwd_hex") == 0) {
			pwd_hex = pos;
		} else if (os_strcmp(token, "pmk") == 0) {
			pmk = pos;
		} else if (os_strcmp(token, "interface_id") == 0) {
			os_free(interface_id);
			interface_id =
				os_malloc(NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN);
			if (!interface_id)
				goto fail;

			if (hexstr2bin(pos, interface_id,
				       NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN) < 0) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Invalid interface_id hex data: %s",
					   pos);
				goto fail;
			}

			ndp.interface_id = interface_id;
		} else {
			wpa_printf(MSG_DEBUG, "NAN: Unknown parameter: %s",
				   token);
		}
	}

	/* If we initiated the NDP setup, we are the subscriber */
	if (ether_addr_equal(ndp.u.resp.resp_ndi, ndp.ndp_id.init_ndi))
		ndp.type = NAN_NDP_ACTION_CONF;

	/* Validate required parameters for accept case */
	if (ndp.u.resp.status == NAN_NDP_STATUS_ACCEPTED) {
		const u8 *publisher_nmi;

		if (is_zero_ether_addr(ndp.u.resp.resp_ndi)) {
			wpa_printf(MSG_INFO,
				   "NAN: Missing required parameter for accept: ndi");
			goto fail;
		}

		if (ndp.type == NAN_NDP_ACTION_CONF)
			publisher_nmi = ndp.ndp_id.peer_nmi;
		else
			publisher_nmi = wpa_s->own_addr;

		if (handle < 1) {
			wpa_printf(MSG_INFO,
				   "NAN: Missing required parameter for accept: handle");
			goto fail;
		}

		if ((pmk && pwd) || (pmk && pwd_hex) || (pwd && pwd_hex)) {
			wpa_printf(MSG_INFO,
				   "NAN: Specify only one of password, pwd_hex, or pmk");
			goto fail;
		}

		if (pwd_hex) {
			pwd_decoded = wpas_nan_parse_password_hex(pwd_hex);
			if (!pwd_decoded)
				goto fail;
		}

		if (wpas_nan_fill_nd_pmk(wpa_s, &ndp, handle, publisher_nmi,
					 pwd_decoded ? pwd_decoded : pwd, pmk)
		    < 0) {
			wpa_printf(MSG_INFO, "NAN: Failed to derive NDP PMK");
			goto fail;
		}
	}

	/* Validate common required parameters */
	if (is_zero_ether_addr(ndp.ndp_id.peer_nmi)) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: peer_nmi");
		goto fail;
	}

	if (is_zero_ether_addr(ndp.ndp_id.init_ndi)) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: init_ndi");
		goto fail;
	}

	if (!ndp.ndp_id.id) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: ndp_id");
		goto fail;
	}

	if (ndp.u.resp.status == NAN_NDP_STATUS_ACCEPTED && ndi_wpa_s &&
	    wpas_nan_ndp_response_set_gtk(wpa_s, ndi_wpa_s, handle, &ndp) < 0) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Failed to set GTK for NDP response");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "NAN: %s NDP response for peer " MACSTR
		   " ndp_id=%u",
		   ndp.u.resp.status == NAN_NDP_STATUS_ACCEPTED ?
		   "Accepting" : "Rejecting",
		   MAC2STR(ndp.ndp_id.peer_nmi), ndp.ndp_id.id);

	if (wpas_nan_set_ndp_schedule(wpa_s, &ndp) < 0) {
		wpa_printf(MSG_INFO,
			   "NAN: Failed to set NDP schedule");
		goto fail;
	}

	ret = nan_handle_ndp_setup(wpa_s->nan, &ndp);
	if (ret < 0)
		wpa_printf(MSG_INFO, "NAN: Failed to handle NDP response");

fail:
	wpabuf_free(ndp.sched.elems);
	wpabuf_free(ssi_buf);
	os_free(interface_id);
	str_clear_free(pwd_decoded);

	return ret;
}


/* Format: NAN_NDP_TERMINATE peer_nmi=<nmi> init_ndi=<ndi> ndp_id=<id> */
int wpas_nan_ndp_terminate(struct wpa_supplicant *wpa_s, char *cmd)
{
	struct nan_ndp_params ndp;
	char *token, *context = NULL;
	char *pos;

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	os_memset(&ndp, 0, sizeof(ndp));

	ndp.type = NAN_NDP_ACTION_TERM;

	/* Parse command parameters */
	while ((token = str_token(cmd, " ", &context))) {
		pos = os_strchr(token, '=');
		if (!pos) {
			wpa_printf(MSG_INFO,
				   "NAN: Invalid parameter format: %s",
				   token);
			return -1;
		}
		*pos++ = '\0';

		if (os_strcmp(token, "peer_nmi") == 0) {
			if (hwaddr_aton(pos, ndp.ndp_id.peer_nmi) < 0) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid peer NMI address: %s",
					   pos);
				return -1;
			}
		} else if (os_strcmp(token, "init_ndi") == 0) {
			if (hwaddr_aton(pos, ndp.ndp_id.init_ndi) < 0) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid initiator NDI address: %s",
					   pos);
				return -1;
			}
		} else if (os_strcmp(token, "ndp_id") == 0) {
			ndp.ndp_id.id = atoi(pos);
		} else {
			wpa_printf(MSG_DEBUG, "NAN: Unknown parameter: %s",
				   token);
		}
	}

	/* Validate required parameters */
	if (is_zero_ether_addr(ndp.ndp_id.peer_nmi)) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: peer_nmi");
		return -1;
	}

	if (is_zero_ether_addr(ndp.ndp_id.init_ndi)) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: init_ndi");
		return -1;
	}

	if (!ndp.ndp_id.id) {
		wpa_printf(MSG_INFO,
			   "NAN: Missing required parameter: ndp_id");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "NAN: Terminating NDP with peer " MACSTR
		   " init_ndi=" MACSTR " ndp_id=%u",
		   MAC2STR(ndp.ndp_id.peer_nmi),
		   MAC2STR(ndp.ndp_id.init_ndi), ndp.ndp_id.id);

	return nan_handle_ndp_setup(wpa_s->nan, &ndp);
}


int wpas_nan_status(struct wpa_supplicant *wpa_s, char *reply,
		    size_t reply_size)
{
	char *pos = reply;
	char *end = reply + reply_size;
	int ret;

	if (!wpas_nan_ready(wpa_s))
		return -1;

	ret = nan_get_status(wpa_s->nan, pos, end - pos);
	if (ret > 0)
		pos += ret;

	ret = nan_de_get_status(wpa_s->nan_de, pos, end - pos);
	if (ret > 0)
		pos += ret;
	return pos - reply;
}


#ifdef CONFIG_PASN
static int wpas_nan_append_ik_info(char *reply, size_t reply_size,
				   const struct wpa_dev_ik *ik)
{
	char *pos = reply;
	char *end = reply + reply_size;

	pos += wpa_scnprintf(pos, end - pos, "nik_cipher=%d\n", ik->dik_cipher);
	pos += wpa_scnprintf(pos, end - pos, "nik=");
	pos += wpa_snprintf_hex(pos, end - pos, wpabuf_head(ik->dik),
				wpabuf_len(ik->dik));
	pos += wpa_scnprintf(pos, end - pos, "\n");

	if (ik->pmk) {
		pos += wpa_scnprintf(pos, end - pos, "akmp=%s\n",
				     wpa_key_mgmt_txt(ik->akmp, WPA_PROTO_RSN));
		pos += wpa_scnprintf(pos, end - pos, "npk=");
		pos += wpa_snprintf_hex(pos, end - pos, wpabuf_head(ik->pmk),
					wpabuf_len(ik->pmk));
		pos += wpa_scnprintf(pos, end - pos, "\n");
	}

	return pos - reply;
}
#endif /* CONFIG_PASN */


/* Format: NAN_PEER_INFO <addr>
 * <schedule|potential|capa|bootstrap|pairing> [map_id] */
int wpas_nan_peer_info(struct wpa_supplicant *wpa_s, const char *cmd,
		       char *reply, size_t reply_size)
{
	u8 addr[ETH_ALEN];
	char *pos;
	int ret = 0;

	if (!wpas_nan_ready(wpa_s))
		return -1;

	if (hwaddr_aton(cmd, addr) < 0) {
		wpa_printf(MSG_INFO, "NAN: Invalid peer address: %s", cmd);
		return -1;
	}

	pos = os_strchr(cmd, ' ');
	if (!pos) {
		wpa_printf(MSG_INFO, "NAN: Missing info type parameter");
		return -1;
	}

	if (os_strncmp(pos + 1, "schedule", 8) == 0) {
		struct nan_peer_schedule sched;

		if (nan_peer_get_schedule_info(wpa_s->nan, addr, &sched) < 0) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to get schedule info for peer "
				   MACSTR, MAC2STR(addr));
			return -1;
		}

		ret = nan_peer_dump_sched_to_buf(&sched, reply, reply_size);
	} else if (os_strncmp(pos + 1, "potential", 9) == 0) {
		struct nan_peer_potential_avail pot_avail;

		if (nan_peer_get_pot_avail(wpa_s->nan, addr, &pot_avail) < 0) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to get potential availability for peer "
				   MACSTR, MAC2STR(addr));
			return -1;
		}

		ret = nan_peer_dump_pot_avail_to_buf(&pot_avail, reply,
						     reply_size);
	} else if (os_strncmp(pos + 1, "capa", 4) == 0) {
		int map_id = 0;
		const struct nan_device_capabilities *capa;
		int written = 0;
		char *m;

		m = os_strchr(pos + 1, ' ');
		if (m)
			map_id = atoi(m + 1);

		capa = nan_peer_get_device_capabilities(wpa_s->nan, addr,
							map_id);
		if (!capa) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to get capabilities for peer "
				   MACSTR, MAC2STR(addr));
			return -1;
		}

		written += wpa_scnprintf(reply + written, reply_size - written,
					 "supported_bands=0x%02x\n",
					 capa->supported_bands);
		written += wpa_scnprintf(reply + written, reply_size - written,
					 "op_modes=0x%04x\n", capa->op_mode);
		written += wpa_scnprintf(reply + written, reply_size - written,
					 "cdw_info=0x%04x\n", capa->cdw_info);
		written += wpa_scnprintf(reply + written, reply_size - written,
					 "n_antennas=%d\n", capa->n_antennas);
		written += wpa_scnprintf(reply + written, reply_size - written,
					 "channel_switch_time=%d\n",
					 capa->channel_switch_time);
		written += wpa_scnprintf(reply + written, reply_size - written,
					 "capabilities=0x%02x\n", capa->capa);

		ret = written;
	} else if (os_strncmp(pos + 1, "bootstrap", 9) == 0) {
		u16 supported_methods;

		if (nan_bootstrap_get_supported_methods(wpa_s->nan, addr,
							&supported_methods) <
		    0) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to get bootstrap methods for peer "
				   MACSTR, MAC2STR(addr));
			return -1;
		}

		ret = wpa_scnprintf(reply, reply_size,
				    "supported_methods=0x%04x\n",
				    supported_methods);
#ifdef CONFIG_PASN
	} else if (os_strncmp(pos + 1, "pairing", 7) == 0) {
		const struct nan_pairing_cfg *pairing_cfg;
		const struct wpa_dev_ik *ik = NULL;
		const u8 *nonce = NULL;
		const u8 *tag = NULL;

		pairing_cfg = nan_peer_get_pairing_cfg(wpa_s->nan, addr,
						       &nonce, &tag);
		if (!pairing_cfg) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to get pairing config for peer "
				   MACSTR, MAC2STR(addr));
			return -1;
		}

		ret = wpa_scnprintf(reply, reply_size,
				    "pairing_setup=%d\n"
				    "npk_caching=%d\n"
				    "pairing_verification=%d\n"
				    "cipher_suites=0x%08x\n",
				    pairing_cfg->pairing_setup,
				    pairing_cfg->npk_caching,
				    pairing_cfg->pairing_verification,
				    pairing_cfg->cipher_suites);

		/* Try to find matching NIK if nonce and tag are available */
		if (nonce && tag)
			ik = wpas_nan_find_ik_by_nonce_tag(wpa_s, addr, nonce,
							   tag);

		if (ik)
			ret += wpas_nan_append_ik_info(reply + ret,
						       reply_size - ret, ik);
#endif /* CONFIG_PASN */
	} else if (os_strncmp(pos + 1, "ndps", 4) == 0) {
		ret = nan_peer_dump_ndps_to_buf(wpa_s->nan, addr, reply,
						reply_size);
		if (ret < 0) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to get NDPs for peer " MACSTR,
				   MAC2STR(addr));
			return -1;
		}
	} else {
		wpa_printf(MSG_INFO, "NAN: Unknown info type: %s", pos + 1);
		ret = -1;
	}

	return ret;
}


/*
 * Format: NAN_BOOTSTRAP <peer_nmi> <handle=<id>>
 *     <req_instance_id=<id>> method=<number> [auth]
 */
int wpas_nan_bootstrap_request(struct wpa_supplicant *wpa_s, char *cmd)
{
	char *pos, *token, *context = NULL;
	int handle = 0;
	int req_instance_id = 0;
	u8 peer_nmi[ETH_ALEN];
	u16 bootstrap_method = 0;
	bool auth = false;

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	/* Parse peer address first */
	if (hwaddr_aton(cmd, peer_nmi) < 0)
		return -1;

	/* Move past the peer_mac address */
	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;

	while ((token = str_token(pos, " ", &context))) {
		if (sscanf(token, "handle=%i", &handle) == 1)
			continue;

		if (sscanf(token, "req_instance_id=%i", &req_instance_id) == 1)
			continue;

		if (os_strncmp(token, "method=", 7) == 0) {
			bootstrap_method = atoi(token + 7);
			continue;
		}

		if (os_strcmp(token, "auth") == 0) {
			auth = true;
			continue;
		}

		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_BOOTSTRAP parameter: %s",
			   token);
		return -1;
	}

	if (!bootstrap_method) {
		wpa_printf(MSG_INFO, "CTRL: Missing NAN_BOOTSTRAP method");
		return -1;
	}

	if (handle <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_BOOTSTRAP handle");
		return -1;
	}

	if (is_zero_ether_addr(peer_nmi)) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_BOOTSTRAP address");
		return -1;
	}

	return nan_bootstrap_request(wpa_s->nan, handle, peer_nmi,
				     req_instance_id, bootstrap_method, auth);
}


/* Format: NAN_BOOTSTRAP_RESET <peer_nmi> */
int wpas_nan_bootstrap_reset(struct wpa_supplicant *wpa_s, char *cmd)
{
	u8 peer_nmi[ETH_ALEN];

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	if (hwaddr_aton(cmd, peer_nmi) < 0)
		return -1;

	return nan_bootstrap_peer_reset(wpa_s->nan, peer_nmi);
}


static void wpas_nan_de_add_extra_attrs(void *ctx, struct wpabuf *buf)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct nan_schedule sched;
	u32 map_ids = (BIT(wpa_s->nan_capa.num_radios) - 1) << 1;
	int i;

	if (!wpas_nan_ndp_allowed(wpa_s) || !map_ids)
		return;

	wpas_nan_fill_ndp_schedule(wpa_s, &sched);
	nan_add_dev_capa_attr(wpa_s->nan, buf);
	nan_convert_sched_to_avail_attrs(wpa_s->nan,
					 wpa_s->schedule_sequence_id,
					 map_ids, sched.n_chans,
					 sched.chans, buf, true);
	nan_pairing_add_attrs(wpa_s->nan, buf);

	if (!wpa_s->nan_ulw_attr)
		return;

	/* Add ULW attribute only if there are committed availability entries */
	for (i = 0; i < sched.n_chans; i++) {
		if (sched.chans[i].committed.len) {
			wpabuf_put_buf(buf, wpa_s->nan_ulw_attr);
			break;
		}
	}
}


void wpas_nan_cluster_join(struct wpa_supplicant *wpa_s,
			   const u8 *cluster_id,
			   bool new_cluster)
{
	if (!wpas_nan_ready(wpa_s))
		return;

	wpas_notify_nan_cluster_join(wpa_s, cluster_id, new_cluster);

	nan_de_set_cluster_id(wpa_s->nan_de, cluster_id);
	nan_set_cluster_id(wpa_s->nan, cluster_id);
}


void wpas_nan_next_dw(struct wpa_supplicant *wpa_s, u32 freq)
{
	if (!wpas_nan_ready(wpa_s))
		return;

	wpa_printf(MSG_DEBUG, "NAN: Next DW notification freq=%d", freq);
	nan_de_dw_trigger(wpa_s->nan_de, freq);
}


void wpas_nan_sched_update_done(struct wpa_supplicant *wpa_s,
				const union wpa_event_data *data)
{
	u8 map_id = wpa_s->nan_sched_update.map_id;
	bool success = data->nan_sched_update_done_info.success;

	if (!wpas_nan_ready(wpa_s))
		return;

	if (!wpa_s->nan_sched_update.sched.deferred) {
		wpa_printf(MSG_DEBUG, "NAN: Schedule update not in progress");
		return;
	}

	nan_set_sched_update_pending(wpa_s->nan, false);
	wpas_notify_nan_sched_update_done(wpa_s, success);

	if (!success) {
		clear_sched_config(&wpa_s->nan_sched_update.sched);
		wpa_printf(MSG_DEBUG, "NAN: Schedule update failed");
		return;
	}

	clear_sched_config(&wpa_s->nan_sched[map_id - 1]);
	os_memcpy(&wpa_s->nan_sched[map_id - 1],
		  &wpa_s->nan_sched_update.sched,
		  sizeof(wpa_s->nan_sched_update.sched));
	os_memset(&wpa_s->nan_sched_update.sched, 0,
		  sizeof(wpa_s->nan_sched_update.sched));
	wpa_s->schedule_sequence_id++;

	wpas_nan_update_local_schedule(wpa_s);
}


void wpas_nan_ulw_update(struct wpa_supplicant *wpa_s,
			 const u8 *ulw, size_t ulw_len)
{
	if (!wpas_nan_ready(wpa_s))
		return;

	wpabuf_free(wpa_s->nan_ulw_attr);
	if (ulw && ulw_len) {
		wpa_s->nan_ulw_attr = wpabuf_alloc_copy(ulw, ulw_len);
		if (!wpa_s->nan_ulw_attr) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to allocate ULW attribute buffer");
			return;
		}

		wpa_hexdump(MSG_DEBUG, "NAN: ULW update", ulw, ulw_len);
	} else {
		wpa_printf(MSG_DEBUG, "NAN: ULW update cleared");
		wpa_s->nan_ulw_attr = NULL;
	}
}


void wpas_nan_chan_evacuation(struct wpa_supplicant *wpa_s,
			      const struct nan_chan_evacuation_info *info)
{
	size_t map_id, i;
	int freq = info->freq;

	if (!wpas_nan_ready(wpa_s))
		return;

	wpa_printf(MSG_DEBUG, "NAN: Channel evacuation notification freq=%d",
		   freq);

	for (map_id = 0; map_id < MAX_NAN_RADIOS; map_id++) {
		struct nan_schedule_config *sched =
			&wpa_s->nan_sched[map_id];

		for (i = 0; i < sched->num_channels; i++) {
			if (sched->channels[i].freq != freq)
				continue;

			wpas_notify_nan_chan_evacuation(wpa_s, map_id, freq);
			break;
		}
	}
}


#ifdef CONFIG_PASN

static int wpas_nan_pasn_update_station(struct wpa_supplicant *wpa_s,
					const u8 *nmi_addr)
{
	struct hostapd_sta_add_params params;

	os_memset(&params, 0, sizeof(params));
	params.addr = nmi_addr;
	params.flags = WPA_STA_MFP;
	params.set = 1;

	if (wpa_drv_sta_add(wpa_s, &params) < 0) {
		wpa_printf(MSG_INFO, "NAN PASN: Failed to update PASN station "
			   MACSTR, MAC2STR(nmi_addr));
		return -1;
	}

	return 0;
}


/**
 * wpas_nan_pair - Initiate NAN pairing with a peer device
 * @wpa_s: Pointer to wpa_supplicant data structure
 * @peer_addr: MAC address of the peer device to pair with
 * @auth_mode: Authentication mode to use for pairing
 * @cipher: Cipher suite to use for the pairing session
 * @handle: Handle of the service for which pairing is requested
 * @peer_instance_id: Instance ID of the peer service
 * @responder: True if the local device is the responder, false if initiator
 * @password: Password for PASN authentication
 * Returns: 0 on success, -1 on failure
 */
int wpas_nan_pair(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		  u8 auth_mode, int cipher, int handle, u8 peer_instance_id,
		  bool responder, const char *password)
{
	int ret;
	struct nan_schedule sched;

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	wpas_nan_fill_ndp_schedule(wpa_s, &sched);
	ret = nan_pairing_initiate_pasn_auth(wpa_s->nan, peer_addr, auth_mode,
					     cipher, handle, peer_instance_id,
					     responder, password, &sched);
	if (!ret)
		ret = wpas_nan_pasn_update_station(wpa_s, peer_addr);
	else
		wpa_printf(MSG_INFO,
			   "NAN PASN: Failed to start PASN authentication");

	return ret;
}


/*
 * Format: NAN_PAIR <peer_nmi> <handle=<id>>
 *	<peer_instance_id=<id>> <auth=<0|1|2>> <cipher=<CCMP|GCMP-256>>
 *	[responder] [password=<password>|pwd_hex=<hex>]
 */
int wpas_nan_pairing_start(struct wpa_supplicant *wpa_s, char *cmd)
{
	char *token, *context = NULL;
	u8 addr[ETH_ALEN];
	u8 auth_mode = 0;
	u8 peer_instance_id = 0;
	int handle = 0;
	int cipher = WPA_CIPHER_NONE;
	char *password = NULL, *password_hex = NULL;
	char *password_decoded = NULL;
	bool responder = false;
	char *pos;

	/* Parse peer address first */
	if (hwaddr_aton(cmd, addr) < 0)
		return -1;

	/* Move past the peer_mac address */
	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;

	while ((token = str_token(pos, " ", &context))) {
		if (os_strncmp(token, "auth=", 5) == 0) {
			auth_mode = atoi(token + 5);
			if (auth_mode > 2) {
				wpa_printf(MSG_INFO,
					   "NAN_PAIR: Invalid auth mode: %u",
					   auth_mode);
				return -1;
			}
		} else if (os_strncmp(token, "handle=", 7) == 0) {
			handle = atoi(token + 7);
		} else if (os_strncmp(token, "peer_instance_id=", 17) == 0) {
			peer_instance_id = atoi(token + 17);
		} else if (os_strncmp(token, "cipher=", 7) == 0) {
			if (os_strcmp(token + 7, "CCMP") == 0) {
				cipher = WPA_CIPHER_CCMP;
			} else if (os_strcmp(token + 7, "GCMP-256") == 0) {
				cipher = WPA_CIPHER_GCMP_256;
			} else {
				wpa_printf(MSG_INFO,
					   "NAN_PAIR: Invalid cipher: '%s'",
					   token + 7);
				return -1;
			}
		} else if (os_strncmp(token, "responder", 9) == 0) {
			responder = true;
		} else if (os_strncmp(token, "password=", 9) == 0) {
			password = token + 9;
		} else if (os_strncmp(token, "pwd_hex=", 8) == 0) {
			password_hex = token + 8;
		} else {
			wpa_printf(MSG_INFO,
				   "NAN_PAIR: Invalid parameter: '%s'",
				   token);
			return -1;
		}
	}

	if (handle <= 0) {
		wpa_printf(MSG_INFO, "NAN_PAIR: missing or invalid handle");
		return -1;
	}

	if (!peer_instance_id) {
		wpa_printf(MSG_INFO,
			   "NAN_PAIR: missing or invalid peer_instance_id");
		return -1;
	}

	if (cipher == WPA_CIPHER_NONE) {
		wpa_printf(MSG_INFO, "NAN_PAIR: missing cipher");
		return -1;
	}

	if (password && password_hex) {
		wpa_printf(MSG_DEBUG,
			   "NAN_PAIR: Specify only one of password or pwd_hex");
		return -1;
	}

	if (password_hex) {
		password_decoded = wpas_nan_parse_password_hex(password_hex);
		if (!password_decoded)
			return -1;
	}

	if (wpas_nan_pair(wpa_s, addr, auth_mode, cipher, handle,
			  peer_instance_id, responder,
			  password_decoded ? password_decoded : password) < 0) {
		str_clear_free(password_decoded);
		wpa_printf(MSG_INFO, "NAN_PAIR: Pairing initiation failed");
		return -1;
	}

	str_clear_free(password_decoded);

	return 0;
}


int wpas_nan_pasn_auth_tx_status(struct wpa_supplicant *wpa_s, const u8 *data,
				 size_t data_len, bool acked)
{
	struct nan_data *nan = wpa_s->nan;

	return nan_pairing_pasn_auth_tx_status(nan, data, data_len, acked);
}


int wpas_nan_pairing_abort(struct wpa_supplicant *wpa_s, const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct nan_data *nan = wpa_s->nan;

	if (!nan) {
		wpa_printf(MSG_INFO, "NAN_PAIR_ABORT: NAN not initialized");
		return -1;
	}

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_INFO,
			   "NAN_PAIR_ABORT: Invalid peer address: '%s'", cmd);
		return -1;
	}

	if (nan_pairing_abort(nan, addr) < 0) {
		wpa_printf(MSG_INFO,
			   "NAN_PAIR_ABORT: Abort failed for peer " MACSTR,
			   MAC2STR(addr));
		return -1;
	}

	return 0;
}


int wpas_nan_pasn_auth_rx(struct wpa_supplicant *wpa_s,
			  const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct nan_data *nan = wpa_s->nan;

	if (!nan || !wpas_nan_ndp_allowed(wpa_s))
		return -1;

	return nan_pairing_auth_rx(nan, mgmt, len);
}

#endif /* CONFIG_PASN */


bool wpas_nan_is_peer_paired(struct wpa_supplicant *wpa_s, const u8 *peer_addr)
{
	if (!wpa_s->nan)
		return false;

	return nan_pairing_is_peer_paired(wpa_s->nan, peer_addr);
}

#endif /* CONFIG_NAN */


static const char *
tx_status_result_txt(enum offchannel_send_action_result result)
{
	switch (result) {
	case OFFCHANNEL_SEND_ACTION_SUCCESS:
		return "success";
	case OFFCHANNEL_SEND_ACTION_NO_ACK:
		return "no-ack";
	case OFFCHANNEL_SEND_ACTION_FAILED:
		return "failed";
	}

	return "?";
}


static void wpas_nan_de_tx_status(struct wpa_supplicant *wpa_s,
				  unsigned int freq, const u8 *dst,
				  const u8 *src, const u8 *bssid,
				  const u8 *data, size_t data_len,
				  enum offchannel_send_action_result result)
{
	if (!wpa_s->nan_de)
		return;

	wpa_printf(MSG_DEBUG, "NAN: TX status A1=" MACSTR " A2=" MACSTR
		   " A3=" MACSTR " freq=%d len=%zu result=%s",
		   MAC2STR(dst), MAC2STR(src), MAC2STR(bssid), freq,
		   data_len, tx_status_result_txt(result));

	nan_de_tx_status(wpa_s->nan_de, freq, dst, data, data_len,
			 result == OFFCHANNEL_SEND_ACTION_SUCCESS);
}


struct wpas_nan_usd_tx_work {
	unsigned int freq;
	unsigned int wait_time;
	u8 dst[ETH_ALEN];
	u8 src[ETH_ALEN];
	u8 bssid[ETH_ALEN];
	struct wpabuf *buf;
};


static void wpas_nan_usd_tx_work_free(struct wpas_nan_usd_tx_work *twork)
{
	if (!twork)
		return;
	wpabuf_free(twork->buf);
	os_free(twork);
}


static void wpas_nan_usd_tx_work_done(struct wpa_supplicant *wpa_s)
{
	struct wpas_nan_usd_tx_work *twork;

	if (!wpa_s->nan_usd_tx_work)
		return;

	twork = wpa_s->nan_usd_tx_work->ctx;
	wpas_nan_usd_tx_work_free(twork);
	radio_work_done(wpa_s->nan_usd_tx_work);
	wpa_s->nan_usd_tx_work = NULL;
}


static int wpas_nan_de_tx_send(struct wpa_supplicant *wpa_s, unsigned int freq,
			       unsigned int wait_time, const u8 *dst,
			       const u8 *src, const u8 *bssid,
			       const struct wpabuf *buf)
{
	wpa_printf(MSG_DEBUG, "NAN: TX NAN SDF A1=" MACSTR " A2=" MACSTR
		   " A3=" MACSTR " freq=%d len=%zu",
		   MAC2STR(dst), MAC2STR(src), MAC2STR(bssid), freq,
		   wpabuf_len(buf));

	return offchannel_send_action(wpa_s, freq, dst, src, bssid,
				      wpabuf_head(buf), wpabuf_len(buf),
				      wait_time, wpas_nan_de_tx_status, 1);
}


static void wpas_nan_usd_start_tx_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpas_nan_usd_tx_work *twork = work->ctx;

	if (deinit) {
		if (work->started) {
			wpa_s->nan_usd_tx_work = NULL;
			offchannel_send_action_done(wpa_s);
		}
		wpas_nan_usd_tx_work_free(twork);
		return;
	}

	wpa_s->nan_usd_tx_work = work;

	if (wpas_nan_de_tx_send(wpa_s, twork->freq, twork->wait_time,
				twork->dst, twork->src, twork->bssid,
				twork->buf) < 0)
		wpas_nan_usd_tx_work_done(wpa_s);
}


static int wpas_nan_de_tx(void *ctx, unsigned int freq, unsigned int wait_time,
			  const u8 *dst, const u8 *src, const u8 *bssid,
			  const struct wpabuf *buf)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpas_nan_usd_tx_work *twork;

	if (!freq && !wait_time) {
		int ret;

		wpa_printf(MSG_DEBUG, "NAN: SYNC TX NAN SDF A1=" MACSTR " A2="
			   MACSTR " A3=" MACSTR " len=%zu",
			   MAC2STR(dst), MAC2STR(src), MAC2STR(bssid),
			   wpabuf_len(buf));
		ret = wpa_drv_send_action(wpa_s, 0, 0, dst, src, bssid,
					  wpabuf_head(buf), wpabuf_len(buf),
					  1);
		if (ret)
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to send sync action frame (%d)",
				   ret);
		return ret;
	}

	if (wpa_s->nan_usd_tx_work || wpa_s->nan_usd_listen_work) {
		/* Reuse ongoing radio work */
		return wpas_nan_de_tx_send(wpa_s, freq, wait_time, dst, src,
					   bssid, buf);
	}

	twork = os_zalloc(sizeof(*twork));
	if (!twork)
		return -1;
	twork->freq = freq;
	twork->wait_time = wait_time;
	os_memcpy(twork->dst, dst, ETH_ALEN);
	os_memcpy(twork->src, src, ETH_ALEN);
	os_memcpy(twork->bssid, bssid, ETH_ALEN);
	twork->buf = wpabuf_dup(buf);
	if (!twork->buf) {
		wpas_nan_usd_tx_work_free(twork);
		return -1;
	}

	if (!radio_add_work(wpa_s, freq, "nan-usd-tx", 0,
			    wpas_nan_usd_start_tx_cb, twork)) {
		wpas_nan_usd_tx_work_free(twork);
		return -1;
	}

	return 0;
}


struct wpas_nan_usd_listen_work {
	unsigned int freq;
	unsigned int duration;
	u8 forced_addr[ETH_ALEN];
	bool forced_addr_set;
};


static void wpas_nan_usd_listen_work_done(struct wpa_supplicant *wpa_s)
{
	struct wpas_nan_usd_listen_work *lwork;

	if (!wpa_s->nan_usd_listen_work)
		return;

	lwork = wpa_s->nan_usd_listen_work->ctx;
	os_free(lwork);
	radio_work_done(wpa_s->nan_usd_listen_work);
	wpa_s->nan_usd_listen_work = NULL;
}


static void wpas_nan_usd_remain_on_channel_timeout(void *eloop_ctx,
						   void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpas_nan_usd_listen_work *lwork = timeout_ctx;

	wpas_nan_usd_cancel_remain_on_channel_cb(wpa_s, lwork->freq);
}


static void wpas_nan_usd_start_listen_cb(struct wpa_radio_work *work,
					 int deinit)
{
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpas_nan_usd_listen_work *lwork = work->ctx;
	unsigned int duration;

	if (deinit) {
		if (work->started) {
			wpa_s->nan_usd_listen_work = NULL;
			wpa_drv_cancel_remain_on_channel(wpa_s);
		}
		os_free(lwork);
		return;
	}

	wpa_s->nan_usd_listen_work = work;

	duration = lwork->duration;
	if (duration > wpa_s->max_remain_on_chan)
		duration = wpa_s->max_remain_on_chan;
	wpa_printf(MSG_DEBUG, "NAN: Start listen on %u MHz for %u ms",
		   lwork->freq, duration);
	if (wpa_drv_remain_on_channel(wpa_s, lwork->freq, duration,
				      (lwork->forced_addr_set &&
				       (wpa_s->drv_flags2 &
					WPA_DRIVER_FLAGS2_ROC_ADDR_FILTER)) ?
				      lwork->forced_addr : NULL) < 0) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Failed to request the driver to remain on channel (%u MHz) for listen",
			   lwork->freq);
		eloop_cancel_timeout(wpas_nan_usd_remain_on_channel_timeout,
				     wpa_s, ELOOP_ALL_CTX);
		/* Restart the listen state after a delay */
		eloop_register_timeout(0, 500,
				       wpas_nan_usd_remain_on_channel_timeout,
				       wpa_s, lwork);
		wpas_nan_usd_listen_work_done(wpa_s);
		return;
	}
}


static int wpas_nan_de_listen(void *ctx, unsigned int freq,
			      unsigned int duration, const u8 *forced_addr)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpas_nan_usd_listen_work *lwork;

	lwork = os_zalloc(sizeof(*lwork));
	if (!lwork)
		return -1;
	lwork->freq = freq;
	lwork->duration = duration;
	if (forced_addr) {
		os_memcpy(lwork->forced_addr, forced_addr, ETH_ALEN);
		lwork->forced_addr_set = true;
	}

	if (!radio_add_work(wpa_s, freq, "nan-usd-listen", 0,
			    wpas_nan_usd_start_listen_cb, lwork)) {
		os_free(lwork);
		return -1;
	}

	return 0;
}


static void
wpas_nan_de_discovery_result(void *ctx, struct nan_discovery_result *res)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_discovery_result(wpa_s, res);
}


static void wpas_nan_de_replied(void *ctx, int publish_id, const u8 *peer_addr,
				int peer_subscribe_id,
				enum nan_service_protocol_type srv_proto_type,
				const u8 *ssi, size_t ssi_len)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_replied(wpa_s, srv_proto_type, publish_id,
				peer_subscribe_id, peer_addr, ssi, ssi_len);
}


static void wpas_nan_de_publish_terminated(void *ctx, int publish_id,
					   enum nan_de_reason reason)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_publish_terminated(wpa_s, publish_id, reason);
}


static void wpas_nan_usd_offload_cancel_publish(void *ctx, int publish_id)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD)
		wpas_drv_nan_cancel_publish(wpa_s, publish_id);
}


static void wpas_nan_de_subscribe_terminated(void *ctx, int subscribe_id,
					     enum nan_de_reason reason)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_subscribe_terminated(wpa_s, subscribe_id, reason);
}


static void wpas_nan_usd_offload_cancel_subscribe(void *ctx, int subscribe_id)
{
	struct wpa_supplicant *wpa_s = ctx;

	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD)
		wpas_drv_nan_cancel_subscribe(wpa_s, subscribe_id);
}


static void wpas_nan_de_receive(void *ctx, int id, int peer_instance_id,
				const u8 *ssi, size_t ssi_len,
				const u8 *peer_addr,
				const u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;

#ifdef CONFIG_NAN
	if (nan_process_followup(wpa_s->nan, peer_addr, buf, len,
				 peer_instance_id, id))
		return;
#endif /* CONFIG_NAN */

	wpas_notify_nan_receive(wpa_s, id, peer_instance_id, peer_addr,
				ssi, ssi_len);
}


static void wpas_nan_de_transmit_req_status(void *ctx, u32 cookie, bool ack)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_notify_nan_transmit_req_status(wpa_s, cookie, ack);
}


#ifdef CONFIG_P2P
static void wpas_nan_process_p2p_usd_elems(void *ctx, const u8 *buf,
					   u16 buf_len, const u8 *peer_addr,
					   unsigned int freq)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_p2p_process_usd_elems(wpa_s, buf, buf_len, peer_addr, freq);
}
#endif /* CONFIG_P2P */


#ifdef CONFIG_PR
static void wpas_nan_process_pr_usd_elems(void *ctx, const u8 *buf, u16 buf_len,
					  const u8 *peer_addr,
					  unsigned int freq)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpas_pr_process_usd_elems(wpa_s, buf, buf_len, peer_addr, freq);
}
#endif /* CONFIG_PR */


#if defined(CONFIG_NAN) && defined(CONFIG_PASN)
static bool wpas_nan_is_peer_paired_cb(void *ctx, const u8 *peer_addr)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpas_nan_is_peer_paired(wpa_s, peer_addr);
}
#endif /* CONFIG_NAN && CONFIG_PASN */


int wpas_nan_de_init(struct wpa_supplicant *wpa_s)
{
	struct nan_callbacks cb;
	bool offload = !!(wpa_s->drv_flags2 &
			  WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD);

	os_memset(&cb, 0, sizeof(cb));
	cb.ctx = wpa_s;
	cb.tx = wpas_nan_de_tx;
	cb.listen = wpas_nan_de_listen;
	cb.discovery_result = wpas_nan_de_discovery_result;
	cb.replied = wpas_nan_de_replied;
	cb.publish_terminated = wpas_nan_de_publish_terminated;
	cb.subscribe_terminated = wpas_nan_de_subscribe_terminated;
	cb.offload_cancel_publish = wpas_nan_usd_offload_cancel_publish;
	cb.offload_cancel_subscribe = wpas_nan_usd_offload_cancel_subscribe;
	cb.receive = wpas_nan_de_receive;
	cb.transmit_req_status = wpas_nan_de_transmit_req_status;
#ifdef CONFIG_P2P
	cb.process_p2p_usd_elems = wpas_nan_process_p2p_usd_elems;
#endif /* CONFIG_P2P */
#ifdef CONFIG_PR
	cb.process_pr_usd_elems = wpas_nan_process_pr_usd_elems;
#endif /* CONFIG_PR */
#ifdef CONFIG_NAN
	cb.add_extra_attrs = wpas_nan_de_add_extra_attrs;
#ifdef CONFIG_PASN
	cb.is_peer_paired = wpas_nan_is_peer_paired_cb;
#endif /* CONFIG_PASN */
#endif /* CONFIG_NAN */

	wpa_s->nan_de = nan_de_init(wpa_s->own_addr, offload, false,
				    wpa_s->max_remain_on_chan, &cb);
	if (!wpa_s->nan_de)
		return -1;
	return 0;
}


void wpas_nan_de_deinit(struct wpa_supplicant *wpa_s)
{
	eloop_cancel_timeout(wpas_nan_usd_remain_on_channel_timeout,
			     wpa_s, ELOOP_ALL_CTX);
	nan_de_deinit(wpa_s->nan_de);
	wpa_s->nan_de = NULL;
}


void wpas_nan_de_rx_sdf(struct wpa_supplicant *wpa_s, const u8 *src,
			const u8 *a3, unsigned int freq,
			const u8 *buf, size_t len, int rssi)
{
	bool store_peer;

	if (!wpa_s->nan_de)
		return;

	store_peer = nan_de_rx_sdf(wpa_s->nan_de, src, a3, freq, buf,
				   len, rssi);

	if (!store_peer)
		return;

#ifdef CONFIG_NAN
	if (!wpas_nan_ready(wpa_s))
		return;

	nan_add_peer(wpa_s->nan, src, buf, len);
#endif /* CONFIG_NAN */
}


void wpas_nan_de_flush(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->nan_de)
		return;
	nan_de_flush(wpa_s->nan_de);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD)
		wpas_drv_nan_flush(wpa_s);
}


int wpas_nan_publish(struct wpa_supplicant *wpa_s, const char *service_name,
		     enum nan_service_protocol_type srv_proto_type,
		     const struct wpabuf *ssi,
		     struct nan_publish_params *params, bool p2p)
{
	int publish_id;
	struct wpabuf *elems = NULL;
	const u8 *addr;

	if (!wpa_s->nan_de)
		return -1;

	if (params->proximity_ranging && !params->solicited) {
		wpa_printf(MSG_INFO,
			   "PR unsolicited publish service discovery not allowed");
		return -1;
	}

	addr = wpa_s->own_addr;

#ifdef CONFIG_NAN
	if (params->sync) {
		if (!(wpa_s->nan_capa.drv_flags &
		      WPA_DRIVER_FLAGS_NAN_SUPPORT_USERSPACE_DE)) {
			wpa_printf(MSG_INFO,
				   "NAN: Cannot advertise sync service, driver does not support user space DE");
			return -1;
		}

		if (!wpas_nan_ready(wpa_s)) {
			wpa_printf(MSG_INFO,
				   "NAN: Synchronized support is not enabled");
			return -1;
		}

		if (p2p) {
			wpa_printf(MSG_INFO,
				   "NAN: Sync discovery is not supported for P2P");
			return -1;
		}

		if (params->proximity_ranging) {
			wpa_printf(MSG_INFO,
				   "NAN: Sync discovery is not supported for PR");
			return -1;
		}
	}
#endif /* CONFIG_NAN */

	if (p2p) {
		elems = wpas_p2p_usd_elems(wpa_s, service_name);
		addr = wpa_s->global->p2p_dev_addr;
	} else if (params->proximity_ranging) {
		const u8 *src = params->forced_addr ?
			params->forced_addr : wpa_s->own_addr;

		elems = wpas_pr_usd_elems(wpa_s, src);
	}

	if (params->forced_addr) {
		if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_MGMT_TX_RANDOM_TA)) {
			wpa_printf(MSG_INFO, "NAN: Random TA not allowed");
			return -1;
		}
		addr = params->forced_addr;
	}

	publish_id = nan_de_publish(wpa_s->nan_de, service_name, srv_proto_type,
				    ssi, elems, params, p2p, addr);
	if (publish_id >= 1 && !params->sync &&
	    (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD) &&
	    wpas_drv_nan_publish(wpa_s, addr, publish_id, service_name,
				 nan_de_get_service_id(wpa_s->nan_de,
						       publish_id),
				 srv_proto_type, ssi, elems, params,
				 p2p ? p2p_network_id : nan_network_id) < 0) {
		nan_de_cancel_publish(wpa_s->nan_de, publish_id);
		publish_id = -1;
	}
#ifdef CONFIG_AP
	if (publish_id >= 1 && wpa_s->ap_iface && wpa_s->ap_iface->bss[0]) {
		wpa_printf(MSG_DEBUG, "NAN: Linking nan_de for AP interface");
		wpa_s->ap_iface->bss[0]->nan_de = wpa_s->nan_de;
	}
#endif /* CONFIG_AP */

	wpabuf_free(elems);
	return publish_id;
}


void wpas_nan_cancel_publish(struct wpa_supplicant *wpa_s, int publish_id)
{
	if (!wpa_s->nan_de)
		return;
	nan_de_cancel_publish(wpa_s->nan_de, publish_id);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD)
		wpas_drv_nan_cancel_publish(wpa_s, publish_id);
}


int wpas_nan_update_publish(struct wpa_supplicant *wpa_s, int publish_id,
			    const struct wpabuf *ssi)
{
	int ret;

	if (!wpa_s->nan_de)
		return -1;
	ret = nan_de_update_publish(wpa_s->nan_de, publish_id, ssi);
	if (ret == 0 && (wpa_s->drv_flags2 &
			 WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD) &&
	    wpas_drv_nan_update_publish(wpa_s, publish_id, ssi) < 0)
		return -1;
	return ret;
}


int wpas_nan_usd_unpause_publish(struct wpa_supplicant *wpa_s, int publish_id,
				 u8 peer_instance_id, const u8 *peer_addr)
{
	if (!wpa_s->nan_de)
		return -1;
	return nan_de_unpause_publish(wpa_s->nan_de, publish_id,
				      peer_instance_id, peer_addr);
}


static int wpas_nan_stop_listen(struct wpa_supplicant *wpa_s, int id)
{
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD)
		return 0;

	if (nan_de_stop_listen(wpa_s->nan_de, id) < 0)
		return -1;

	if (wpa_s->nan_usd_listen_work) {
		wpa_printf(MSG_DEBUG, "NAN: Stop listen operation");
		wpa_drv_cancel_remain_on_channel(wpa_s);
		wpas_nan_usd_listen_work_done(wpa_s);
	}

	if (wpa_s->nan_usd_tx_work) {
		wpa_printf(MSG_DEBUG, "NAN: Stop TX wait operation");
		offchannel_send_action_done(wpa_s);
		wpas_nan_usd_tx_work_done(wpa_s);
	}

	return 0;
}


int wpas_nan_usd_publish_stop_listen(struct wpa_supplicant *wpa_s,
				     int publish_id)
{
	if (!wpa_s->nan_de)
		return -1;

	wpa_printf(MSG_DEBUG, "NAN: Request to stop listen for publish_id=%d",
		   publish_id);
	return wpas_nan_stop_listen(wpa_s, publish_id);
}


int wpas_nan_subscribe(struct wpa_supplicant *wpa_s,
		       const char *service_name,
		       enum nan_service_protocol_type srv_proto_type,
		       const struct wpabuf *ssi,
		       struct nan_subscribe_params *params, bool p2p)
{
	int subscribe_id;
	struct wpabuf *elems = NULL;
	const u8 *addr;

	if (!wpa_s->nan_de)
		return -1;

	if (params->proximity_ranging && !params->active) {
		wpa_printf(MSG_INFO,
			   "PR passive subscriber service discovery not allowed");
		return -1;
	}

	addr = wpa_s->own_addr;

#ifdef CONFIG_NAN
	if (params->sync) {
		if (!(wpa_s->nan_capa.drv_flags &
		      WPA_DRIVER_FLAGS_NAN_SUPPORT_USERSPACE_DE)) {
			wpa_printf(MSG_INFO,
				   "NAN: Cannot subscribe sync, user space DE is not supported");
			return -1;
		}

		if (!wpas_nan_ready(wpa_s)) {
			wpa_printf(MSG_INFO, "NAN: Not ready (subscribe)");
			return -1;
		}

		if (p2p) {
			wpa_printf(MSG_INFO,
				   "NAN: Sync discovery is not supported for P2P (subscribe)");
			return -1;
		}

		if (params->proximity_ranging) {
			wpa_printf(MSG_INFO,
				   "NAN: Sync discovery is not supported for PR (subscribe)");
			return -1;
		}
	}
#endif /* CONFIG_NAN */

	if (p2p) {
		elems = wpas_p2p_usd_elems(wpa_s, service_name);
		addr = wpa_s->global->p2p_dev_addr;
	} else if (params->proximity_ranging) {
		const u8 *src = params->forced_addr ?
			params->forced_addr : wpa_s->own_addr;

		elems = wpas_pr_usd_elems(wpa_s, src);
	}

	if (params->forced_addr) {
		if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_MGMT_TX_RANDOM_TA)) {
			wpa_printf(MSG_INFO, "NAN: Random TA not allowed");
			return -1;
		}
		addr = params->forced_addr;
	}

	subscribe_id = nan_de_subscribe(wpa_s->nan_de, service_name,
					srv_proto_type, ssi, elems, params,
					p2p, addr);
	if (subscribe_id >= 1 && !params->sync &&
	    (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD) &&
	    wpas_drv_nan_subscribe(wpa_s, addr, subscribe_id, service_name,
				   nan_de_get_service_id(wpa_s->nan_de,
							 subscribe_id),
				   srv_proto_type, ssi, elems, params,
				   p2p ? p2p_network_id : nan_network_id) < 0) {
		nan_de_cancel_subscribe(wpa_s->nan_de, subscribe_id);
		subscribe_id = -1;
	}
#ifdef CONFIG_AP
	if (subscribe_id >= 1 && wpa_s->ap_iface && wpa_s->ap_iface->bss[0]) {
		wpa_printf(MSG_DEBUG, "NAN: Linking nan_de for AP interface");
		wpa_s->ap_iface->bss[0]->nan_de = wpa_s->nan_de;
	}
#endif /* CONFIG_AP */

	wpabuf_free(elems);
	return subscribe_id;
}


void wpas_nan_cancel_subscribe(struct wpa_supplicant *wpa_s,
			       int subscribe_id)
{
	if (!wpa_s->nan_de)
		return;
	nan_de_cancel_subscribe(wpa_s->nan_de, subscribe_id);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_NAN_USD_OFFLOAD)
		wpas_drv_nan_cancel_subscribe(wpa_s, subscribe_id);
}


int wpas_nan_usd_subscribe_stop_listen(struct wpa_supplicant *wpa_s,
				       int subscribe_id)
{
	if (!wpa_s->nan_de)
		return -1;

	wpa_printf(MSG_DEBUG, "NAN: Request to stop listen for subscribe_id=%d",
		   subscribe_id);
	return wpas_nan_stop_listen(wpa_s, subscribe_id);
}


int wpas_nan_transmit(struct wpa_supplicant *wpa_s, int handle,
		      const struct wpabuf *ssi, const struct wpabuf *elems,
		      const u8 *peer_addr, u8 req_instance_id, u32 *cookie)
{
	if (!wpa_s->nan_de)
		return -1;
	return nan_de_transmit(wpa_s->nan_de, handle, ssi, elems, peer_addr,
			       req_instance_id, NULL, cookie);
}


void wpas_nan_usd_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				       unsigned int freq, unsigned int duration)
{
	wpas_nan_usd_listen_work_done(wpa_s);

	if (wpa_s->nan_de)
		nan_de_listen_started(wpa_s->nan_de, freq, duration);
}


void wpas_nan_usd_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					      unsigned int freq)
{
	if (wpa_s->nan_de)
		nan_de_listen_ended(wpa_s->nan_de, freq);
}


void wpas_nan_usd_tx_wait_expire(struct wpa_supplicant *wpa_s)
{
	wpas_nan_usd_tx_work_done(wpa_s);

	if (wpa_s->nan_de)
		nan_de_tx_wait_ended(wpa_s->nan_de);
}


int * wpas_nan_usd_all_freqs(struct wpa_supplicant *wpa_s)
{
	int i, j;
	int *freqs = NULL;

	if (!wpa_s->hw.modes)
		return NULL;

	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		struct hostapd_hw_modes *mode = &wpa_s->hw.modes[i];

		for (j = 0; j < mode->num_channels; j++) {
			struct hostapd_channel_data *chan = &mode->channels[j];

			/* All 20 MHz channels on 2.4 and 5 GHz band */
			if (chan->freq < 2412 || chan->freq > 5900)
				continue;

			/* that allow frames to be transmitted */
			if (chan->flag & (HOSTAPD_CHAN_DISABLED |
					  HOSTAPD_CHAN_NO_IR |
					  HOSTAPD_CHAN_RADAR))
				continue;

			int_array_add_unique(&freqs, chan->freq);
		}
	}

	return freqs;
}


void wpas_nan_usd_state_change_notif(struct wpa_supplicant *wpa_s)
{
	struct wpa_supplicant *ifs;
	unsigned int n_active = 0;
	struct nan_de_cfg cfg;

	if (!wpa_s->radio)
		return;

	os_memset(&cfg, 0, sizeof(cfg));

	dl_list_for_each(ifs, &wpa_s->radio->ifaces, struct wpa_supplicant,
			 radio_list) {
		if (ifs->wpa_state >= WPA_AUTHENTICATING)
			n_active++;
	}

	wpa_printf(MSG_DEBUG,
		   "NAN: state change notif: n_active=%u, p2p_in_progress=%u",
		   n_active, wpas_p2p_in_progress(wpa_s));

	if (n_active) {
		cfg.n_max = 3;

		if (!wpas_p2p_in_progress(wpa_s)) {
			/* Limit the USD operation on channel to 100 - 300 TUs
			 * to allow more time for other interfaces.
			 */
			cfg.n_min = 1;
		} else {
			/* Limit the USD operation on channel to 200 - 300 TUs
			 * to allow P2P operation to complete.
			 */
			cfg.n_min = 2;
		}

		/* Each 500 ms suspend USD operation for 300 ms */
		cfg.cycle = 500;
		cfg.suspend = 300;
	}

	dl_list_for_each(ifs, &wpa_s->radio->ifaces, struct wpa_supplicant,
			 radio_list) {
		if (ifs->nan_de)
			nan_de_config(ifs->nan_de, &cfg);
	}
}


#ifdef CONFIG_NAN
static struct wpa_supplicant *
wpas_nan_get_mgmt_iface(struct wpa_supplicant *wpa_s)
{
	struct wpa_supplicant *nmi_wpa_s;

	for (nmi_wpa_s = wpa_s->global->ifaces; nmi_wpa_s;
	     nmi_wpa_s = nmi_wpa_s->next) {
		if (nmi_wpa_s->nan_mgmt)
			return nmi_wpa_s;
	}

	return wpa_s;
}
#endif /* CONFIG_NAN */


int wpas_nan_tx_status(struct wpa_supplicant *wpa_s,
			const u8 *data, size_t data_len, int acked)
{
#ifdef CONFIG_NAN
	const struct ieee80211_mgmt *mgmt =
		(const struct ieee80211_mgmt *) data;

	wpa_s = wpas_nan_get_mgmt_iface(wpa_s);

	if (wpa_s->nan_de)
		nan_de_tx_status(wpa_s->nan_de, 0, mgmt->da, data, data_len,
				 acked);

	if (!wpas_nan_ndp_allowed(wpa_s))
		return -1;

	wpa_printf(MSG_DEBUG, "NAN: TX status for frame len=%zu acked=%u",
		   data_len, acked);

	if (!nan_tx_status(wpa_s->nan, mgmt->da, data, data_len, acked)) {
		wpa_printf(MSG_DEBUG, "NAN: Processed NAF TX status");
		return 0;
	}
#endif /* CONFIG_NAN */

	return -1;
}


#ifdef CONFIG_NAN
void wpas_nan_rx_naf(struct wpa_supplicant *wpa_s,
		     const struct ieee80211_mgmt *mgmt, size_t len)
{
	if (mgmt->u.action.category == WLAN_ACTION_PROTECTED_DUAL) {
		wpa_printf(MSG_DEBUG, "NAN: RX NAF: ifname=%s: protected",
			   wpa_s->ifname);

		wpa_s = wpas_nan_get_mgmt_iface(wpa_s);

		wpa_printf(MSG_DEBUG, "NAN: RX NAF: Continue processing on %s",
			   wpa_s->ifname);
	}

	if (!wpas_nan_ndp_allowed(wpa_s))
		return;

	nan_action_rx(wpa_s->nan, mgmt, len);
}


void wpas_nan_data_interface_removed(struct wpa_supplicant *wpa_s)
{
	struct wpa_supplicant *nan_dev_wpas = wpas_nan_get_mgmt_iface(wpa_s);

	wpa_printf(MSG_DEBUG,
		   "NAN: Data interface removed (%s) - terminate NDPs on "
		   MACSTR, wpa_s->ifname, MAC2STR(wpa_s->own_addr));

	if (nan_dev_wpas)
		nan_terminate_ndi_ndps(nan_dev_wpas->nan, wpa_s->own_addr);
}
#endif /* CONFIG_NAN */
