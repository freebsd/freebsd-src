/*
 * WPA Supplicant - Basic mesh peer management
 * Copyright (c) 2013-2014, cozybit, Inc.  All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "ap/hostapd.h"
#include "ap/sta_info.h"
#include "ap/ieee802_11.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "mesh_mpm.h"
#include "mesh_rsn.h"

struct mesh_peer_mgmt_ie {
	const u8 *proto_id;
	const u8 *llid;
	const u8 *plid;
	const u8 *reason;
	const u8 *pmk;
};

static void plink_timer(void *eloop_ctx, void *user_data);


enum plink_event {
	PLINK_UNDEFINED,
	OPN_ACPT,
	OPN_RJCT,
	OPN_IGNR,
	CNF_ACPT,
	CNF_RJCT,
	CNF_IGNR,
	CLS_ACPT,
	CLS_IGNR
};

static const char * const mplstate[] = {
	[PLINK_LISTEN] = "LISTEN",
	[PLINK_OPEN_SENT] = "OPEN_SENT",
	[PLINK_OPEN_RCVD] = "OPEN_RCVD",
	[PLINK_CNF_RCVD] = "CNF_RCVD",
	[PLINK_ESTAB] = "ESTAB",
	[PLINK_HOLDING] = "HOLDING",
	[PLINK_BLOCKED] = "BLOCKED"
};

static const char * const mplevent[] = {
	[PLINK_UNDEFINED] = "UNDEFINED",
	[OPN_ACPT] = "OPN_ACPT",
	[OPN_RJCT] = "OPN_RJCT",
	[OPN_IGNR] = "OPN_IGNR",
	[CNF_ACPT] = "CNF_ACPT",
	[CNF_RJCT] = "CNF_RJCT",
	[CNF_IGNR] = "CNF_IGNR",
	[CLS_ACPT] = "CLS_ACPT",
	[CLS_IGNR] = "CLS_IGNR"
};


static int mesh_mpm_parse_peer_mgmt(struct wpa_supplicant *wpa_s,
				    u8 action_field,
				    const u8 *ie, size_t len,
				    struct mesh_peer_mgmt_ie *mpm_ie)
{
	os_memset(mpm_ie, 0, sizeof(*mpm_ie));

	/* remove optional PMK at end */
	if (len >= 16) {
		len -= 16;
		mpm_ie->pmk = ie + len - 16;
	}

	if ((action_field == PLINK_OPEN && len != 4) ||
	    (action_field == PLINK_CONFIRM && len != 6) ||
	    (action_field == PLINK_CLOSE && len != 6 && len != 8)) {
		wpa_msg(wpa_s, MSG_DEBUG, "MPM: Invalid peer mgmt ie");
		return -1;
	}

	/* required fields */
	if (len < 4)
		return -1;
	mpm_ie->proto_id = ie;
	mpm_ie->llid = ie + 2;
	ie += 4;
	len -= 4;

	/* close reason is always present at end for close */
	if (action_field == PLINK_CLOSE) {
		if (len < 2)
			return -1;
		mpm_ie->reason = ie + len - 2;
		len -= 2;
	}

	/* plid, present for confirm, and possibly close */
	if (len)
		mpm_ie->plid = ie;

	return 0;
}


static int plink_free_count(struct hostapd_data *hapd)
{
	if (hapd->max_plinks > hapd->num_plinks)
		return hapd->max_plinks - hapd->num_plinks;
	return 0;
}


static u16 copy_supp_rates(struct wpa_supplicant *wpa_s,
			   struct sta_info *sta,
			   struct ieee802_11_elems *elems)
{
	if (!elems->supp_rates) {
		wpa_msg(wpa_s, MSG_ERROR, "no supported rates from " MACSTR,
			MAC2STR(sta->addr));
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (elems->supp_rates_len + elems->ext_supp_rates_len >
	    sizeof(sta->supported_rates)) {
		wpa_msg(wpa_s, MSG_ERROR,
			"Invalid supported rates element length " MACSTR
			" %d+%d", MAC2STR(sta->addr), elems->supp_rates_len,
			elems->ext_supp_rates_len);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->supported_rates_len = merge_byte_arrays(
		sta->supported_rates, sizeof(sta->supported_rates),
		elems->supp_rates, elems->supp_rates_len,
		elems->ext_supp_rates, elems->ext_supp_rates_len);

	return WLAN_STATUS_SUCCESS;
}


/* return true if elems from a neighbor match this MBSS */
static Boolean matches_local(struct wpa_supplicant *wpa_s,
			     struct ieee802_11_elems *elems)
{
	struct mesh_conf *mconf = wpa_s->ifmsh->mconf;

	if (elems->mesh_config_len < 5)
		return FALSE;

	return (mconf->meshid_len == elems->mesh_id_len &&
		os_memcmp(mconf->meshid, elems->mesh_id,
			  elems->mesh_id_len) == 0 &&
		mconf->mesh_pp_id == elems->mesh_config[0] &&
		mconf->mesh_pm_id == elems->mesh_config[1] &&
		mconf->mesh_cc_id == elems->mesh_config[2] &&
		mconf->mesh_sp_id == elems->mesh_config[3] &&
		mconf->mesh_auth_id == elems->mesh_config[4]);
}


/* check if local link id is already used with another peer */
static Boolean llid_in_use(struct wpa_supplicant *wpa_s, u16 llid)
{
	struct sta_info *sta;
	struct hostapd_data *hapd = wpa_s->ifmsh->bss[0];

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (sta->my_lid == llid)
			return TRUE;
	}

	return FALSE;
}


/* generate an llid for a link and set to initial state */
static void mesh_mpm_init_link(struct wpa_supplicant *wpa_s,
			       struct sta_info *sta)
{
	u16 llid;

	do {
		if (os_get_random((u8 *) &llid, sizeof(llid)) < 0)
			continue;
	} while (!llid || llid_in_use(wpa_s, llid));

	sta->my_lid = llid;
	sta->peer_lid = 0;

	/*
	 * We do not use wpa_mesh_set_plink_state() here because there is no
	 * entry in kernel yet.
	 */
	sta->plink_state = PLINK_LISTEN;
}


static void mesh_mpm_send_plink_action(struct wpa_supplicant *wpa_s,
				       struct sta_info *sta,
				       enum plink_action_field type,
				       u16 close_reason)
{
	struct wpabuf *buf;
	struct hostapd_iface *ifmsh = wpa_s->ifmsh;
	struct hostapd_data *bss = ifmsh->bss[0];
	struct mesh_conf *conf = ifmsh->mconf;
	u8 supp_rates[2 + 2 + 32];
#ifdef CONFIG_IEEE80211N
	u8 ht_capa_oper[2 + 26 + 2 + 22];
#endif /* CONFIG_IEEE80211N */
	u8 *pos, *cat;
	u8 ie_len, add_plid = 0;
	int ret;
	int ampe = conf->security & MESH_CONF_SEC_AMPE;
	size_t buf_len;

	if (!sta)
		return;

	buf_len = 2 +      /* capability info */
		  2 +      /* AID */
		  2 + 8 +  /* supported rates */
		  2 + (32 - 8) +
		  2 + 32 + /* mesh ID */
		  2 + 7 +  /* mesh config */
		  2 + 23 + /* peering management */
		  2 + 96 + /* AMPE */
		  2 + 16;  /* MIC */
#ifdef CONFIG_IEEE80211N
	if (type != PLINK_CLOSE && wpa_s->mesh_ht_enabled) {
		buf_len += 2 + 26 + /* HT capabilities */
			   2 + 22;  /* HT operation */
	}
#endif /* CONFIG_IEEE80211N */
	if (type != PLINK_CLOSE)
		buf_len += conf->rsn_ie_len; /* RSN IE */

	buf = wpabuf_alloc(buf_len);
	if (!buf)
		return;

	cat = wpabuf_mhead_u8(buf);
	wpabuf_put_u8(buf, WLAN_ACTION_SELF_PROTECTED);
	wpabuf_put_u8(buf, type);

	if (type != PLINK_CLOSE) {
		u8 info;

		/* capability info */
		wpabuf_put_le16(buf, ampe ? IEEE80211_CAP_PRIVACY : 0);

		/* aid */
		if (type == PLINK_CONFIRM)
			wpabuf_put_le16(buf, sta->peer_lid);

		/* IE: supp + ext. supp rates */
		pos = hostapd_eid_supp_rates(bss, supp_rates);
		pos = hostapd_eid_ext_supp_rates(bss, pos);
		wpabuf_put_data(buf, supp_rates, pos - supp_rates);

		/* IE: RSN IE */
		wpabuf_put_data(buf, conf->rsn_ie, conf->rsn_ie_len);

		/* IE: Mesh ID */
		wpabuf_put_u8(buf, WLAN_EID_MESH_ID);
		wpabuf_put_u8(buf, conf->meshid_len);
		wpabuf_put_data(buf, conf->meshid, conf->meshid_len);

		/* IE: mesh conf */
		wpabuf_put_u8(buf, WLAN_EID_MESH_CONFIG);
		wpabuf_put_u8(buf, 7);
		wpabuf_put_u8(buf, conf->mesh_pp_id);
		wpabuf_put_u8(buf, conf->mesh_pm_id);
		wpabuf_put_u8(buf, conf->mesh_cc_id);
		wpabuf_put_u8(buf, conf->mesh_sp_id);
		wpabuf_put_u8(buf, conf->mesh_auth_id);
		info = (bss->num_plinks > 63 ? 63 : bss->num_plinks) << 1;
		/* TODO: Add Connected to Mesh Gate/AS subfields */
		wpabuf_put_u8(buf, info);
		/* always forwarding & accepting plinks for now */
		wpabuf_put_u8(buf, 0x1 | 0x8);
	} else {	/* Peer closing frame */
		/* IE: Mesh ID */
		wpabuf_put_u8(buf, WLAN_EID_MESH_ID);
		wpabuf_put_u8(buf, conf->meshid_len);
		wpabuf_put_data(buf, conf->meshid, conf->meshid_len);
	}

	/* IE: Mesh Peering Management element */
	ie_len = 4;
	if (ampe)
		ie_len += PMKID_LEN;
	switch (type) {
	case PLINK_OPEN:
		break;
	case PLINK_CONFIRM:
		ie_len += 2;
		add_plid = 1;
		break;
	case PLINK_CLOSE:
		ie_len += 2;
		add_plid = 1;
		ie_len += 2; /* reason code */
		break;
	}

	wpabuf_put_u8(buf, WLAN_EID_PEER_MGMT);
	wpabuf_put_u8(buf, ie_len);
	/* peering protocol */
	if (ampe)
		wpabuf_put_le16(buf, 1);
	else
		wpabuf_put_le16(buf, 0);
	wpabuf_put_le16(buf, sta->my_lid);
	if (add_plid)
		wpabuf_put_le16(buf, sta->peer_lid);
	if (type == PLINK_CLOSE)
		wpabuf_put_le16(buf, close_reason);
	if (ampe) {
		if (sta->sae == NULL) {
			wpa_msg(wpa_s, MSG_INFO, "Mesh MPM: no SAE session");
			goto fail;
		}
		mesh_rsn_get_pmkid(wpa_s->mesh_rsn, sta,
				   wpabuf_put(buf, PMKID_LEN));
	}

#ifdef CONFIG_IEEE80211N
	if (type != PLINK_CLOSE && wpa_s->mesh_ht_enabled) {
		pos = hostapd_eid_ht_capabilities(bss, ht_capa_oper);
		pos = hostapd_eid_ht_operation(bss, pos);
		wpabuf_put_data(buf, ht_capa_oper, pos - ht_capa_oper);
	}
#endif /* CONFIG_IEEE80211N */

	if (ampe && mesh_rsn_protect_frame(wpa_s->mesh_rsn, sta, cat, buf)) {
		wpa_msg(wpa_s, MSG_INFO,
			"Mesh MPM: failed to add AMPE and MIC IE");
		goto fail;
	}

	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0,
				  sta->addr, wpa_s->own_addr, wpa_s->own_addr,
				  wpabuf_head(buf), wpabuf_len(buf), 0);
	if (ret < 0)
		wpa_msg(wpa_s, MSG_INFO,
			"Mesh MPM: failed to send peering frame");

fail:
	wpabuf_free(buf);
}


/* configure peering state in ours and driver's station entry */
void wpa_mesh_set_plink_state(struct wpa_supplicant *wpa_s,
			      struct sta_info *sta,
			      enum mesh_plink_state state)
{
	struct hostapd_sta_add_params params;
	int ret;

	sta->plink_state = state;

	os_memset(&params, 0, sizeof(params));
	params.addr = sta->addr;
	params.plink_state = state;
	params.set = 1;

	wpa_msg(wpa_s, MSG_DEBUG, "MPM set " MACSTR " into %s",
		MAC2STR(sta->addr), mplstate[state]);
	ret = wpa_drv_sta_add(wpa_s, &params);
	if (ret) {
		wpa_msg(wpa_s, MSG_ERROR, "Driver failed to set " MACSTR
			": %d", MAC2STR(sta->addr), ret);
	}
}


static void mesh_mpm_fsm_restart(struct wpa_supplicant *wpa_s,
				 struct sta_info *sta)
{
	struct hostapd_data *hapd = wpa_s->ifmsh->bss[0];

	eloop_cancel_timeout(plink_timer, wpa_s, sta);

	ap_free_sta(hapd, sta);
}


static void plink_timer(void *eloop_ctx, void *user_data)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct sta_info *sta = user_data;
	u16 reason = 0;
	struct mesh_conf *conf = wpa_s->ifmsh->mconf;

	switch (sta->plink_state) {
	case PLINK_OPEN_RCVD:
	case PLINK_OPEN_SENT:
		/* retry timer */
		if (sta->mpm_retries < conf->dot11MeshMaxRetries) {
			eloop_register_timeout(
				conf->dot11MeshRetryTimeout / 1000,
				(conf->dot11MeshRetryTimeout % 1000) * 1000,
				plink_timer, wpa_s, sta);
			mesh_mpm_send_plink_action(wpa_s, sta, PLINK_OPEN, 0);
			sta->mpm_retries++;
			break;
		}
		reason = WLAN_REASON_MESH_MAX_RETRIES;
		/* fall through on else */

	case PLINK_CNF_RCVD:
		/* confirm timer */
		if (!reason)
			reason = WLAN_REASON_MESH_CONFIRM_TIMEOUT;
		wpa_mesh_set_plink_state(wpa_s, sta, PLINK_HOLDING);
		eloop_register_timeout(conf->dot11MeshHoldingTimeout / 1000,
			(conf->dot11MeshHoldingTimeout % 1000) * 1000,
			plink_timer, wpa_s, sta);
		mesh_mpm_send_plink_action(wpa_s, sta, PLINK_CLOSE, reason);
		break;
	case PLINK_HOLDING:
		/* holding timer */
		mesh_mpm_fsm_restart(wpa_s, sta);
		break;
	default:
		break;
	}
}


/* initiate peering with station */
static void
mesh_mpm_plink_open(struct wpa_supplicant *wpa_s, struct sta_info *sta,
		    enum mesh_plink_state next_state)
{
	struct mesh_conf *conf = wpa_s->ifmsh->mconf;

	eloop_cancel_timeout(plink_timer, wpa_s, sta);
	eloop_register_timeout(conf->dot11MeshRetryTimeout / 1000,
			       (conf->dot11MeshRetryTimeout % 1000) * 1000,
			       plink_timer, wpa_s, sta);
	mesh_mpm_send_plink_action(wpa_s, sta, PLINK_OPEN, 0);
	wpa_mesh_set_plink_state(wpa_s, sta, next_state);
}


int mesh_mpm_plink_close(struct hostapd_data *hapd,
			 struct sta_info *sta, void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	int reason = WLAN_REASON_MESH_PEERING_CANCELLED;

	if (sta) {
		wpa_mesh_set_plink_state(wpa_s, sta, PLINK_HOLDING);
		mesh_mpm_send_plink_action(wpa_s, sta, PLINK_CLOSE, reason);
		wpa_printf(MSG_DEBUG, "MPM closing plink sta=" MACSTR,
			   MAC2STR(sta->addr));
		eloop_cancel_timeout(plink_timer, wpa_s, sta);
		return 0;
	}

	return 1;
}


void mesh_mpm_deinit(struct wpa_supplicant *wpa_s, struct hostapd_iface *ifmsh)
{
	struct hostapd_data *hapd = ifmsh->bss[0];

	/* notify peers we're leaving */
	ap_for_each_sta(hapd, mesh_mpm_plink_close, wpa_s);

	hapd->num_plinks = 0;
	hostapd_free_stas(hapd);
}


/* for mesh_rsn to indicate this peer has completed authentication, and we're
 * ready to start AMPE */
void mesh_mpm_auth_peer(struct wpa_supplicant *wpa_s, const u8 *addr)
{
	struct hostapd_data *data = wpa_s->ifmsh->bss[0];
	struct hostapd_sta_add_params params;
	struct sta_info *sta;
	int ret;

	sta = ap_get_sta(data, addr);
	if (!sta) {
		wpa_msg(wpa_s, MSG_DEBUG, "no such mesh peer");
		return;
	}

	/* TODO: Should do nothing if this STA is already authenticated, but
	 * the AP code already sets this flag. */
	sta->flags |= WLAN_STA_AUTH;

	mesh_rsn_init_ampe_sta(wpa_s, sta);

	os_memset(&params, 0, sizeof(params));
	params.addr = sta->addr;
	params.flags = WPA_STA_AUTHENTICATED | WPA_STA_AUTHORIZED;
	params.set = 1;

	wpa_msg(wpa_s, MSG_DEBUG, "MPM authenticating " MACSTR,
		MAC2STR(sta->addr));
	ret = wpa_drv_sta_add(wpa_s, &params);
	if (ret) {
		wpa_msg(wpa_s, MSG_ERROR,
			"Driver failed to set " MACSTR ": %d",
			MAC2STR(sta->addr), ret);
	}

	if (!sta->my_lid)
		mesh_mpm_init_link(wpa_s, sta);

	mesh_mpm_plink_open(wpa_s, sta, PLINK_OPEN_SENT);
}

/*
 * Initialize a sta_info structure for a peer and upload it into the driver
 * in preparation for beginning authentication or peering. This is done when a
 * Beacon (secure or open mesh) or a peering open frame (for open mesh) is
 * received from the peer for the first time.
 */
static struct sta_info * mesh_mpm_add_peer(struct wpa_supplicant *wpa_s,
					   const u8 *addr,
					   struct ieee802_11_elems *elems)
{
	struct hostapd_sta_add_params params;
	struct mesh_conf *conf = wpa_s->ifmsh->mconf;
	struct hostapd_data *data = wpa_s->ifmsh->bss[0];
	struct sta_info *sta;
	int ret;

	sta = ap_get_sta(data, addr);
	if (!sta) {
		sta = ap_sta_add(data, addr);
		if (!sta)
			return NULL;
	}

	/* initialize sta */
	if (copy_supp_rates(wpa_s, sta, elems)) {
		ap_free_sta(data, sta);
		return NULL;
	}

	mesh_mpm_init_link(wpa_s, sta);

#ifdef CONFIG_IEEE80211N
	copy_sta_ht_capab(data, sta, elems->ht_capabilities);
	update_ht_state(data, sta);
#endif /* CONFIG_IEEE80211N */

	/* insert into driver */
	os_memset(&params, 0, sizeof(params));
	params.supp_rates = sta->supported_rates;
	params.supp_rates_len = sta->supported_rates_len;
	params.addr = addr;
	params.plink_state = sta->plink_state;
	params.aid = sta->peer_lid;
	params.listen_interval = 100;
	params.ht_capabilities = sta->ht_capabilities;
	params.flags |= WPA_STA_WMM;
	params.flags_mask |= WPA_STA_AUTHENTICATED;
	if (conf->security == MESH_CONF_SEC_NONE) {
		params.flags |= WPA_STA_AUTHORIZED;
		params.flags |= WPA_STA_AUTHENTICATED;
	} else {
		sta->flags |= WLAN_STA_MFP;
		params.flags |= WPA_STA_MFP;
	}

	ret = wpa_drv_sta_add(wpa_s, &params);
	if (ret) {
		wpa_msg(wpa_s, MSG_ERROR,
			"Driver failed to insert " MACSTR ": %d",
			MAC2STR(addr), ret);
		ap_free_sta(data, sta);
		return NULL;
	}

	return sta;
}


void wpa_mesh_new_mesh_peer(struct wpa_supplicant *wpa_s, const u8 *addr,
			    struct ieee802_11_elems *elems)
{
	struct mesh_conf *conf = wpa_s->ifmsh->mconf;
	struct hostapd_data *data = wpa_s->ifmsh->bss[0];
	struct sta_info *sta;
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	sta = mesh_mpm_add_peer(wpa_s, addr, elems);
	if (!sta)
		return;

	if (ssid && ssid->no_auto_peer) {
		wpa_msg(wpa_s, MSG_INFO, "will not initiate new peer link with "
			MACSTR " because of no_auto_peer", MAC2STR(addr));
		if (data->mesh_pending_auth) {
			struct os_reltime age;
			const struct ieee80211_mgmt *mgmt;
			struct hostapd_frame_info fi;

			mgmt = wpabuf_head(data->mesh_pending_auth);
			os_reltime_age(&data->mesh_pending_auth_time, &age);
			if (age.sec < 2 &&
			    os_memcmp(mgmt->sa, addr, ETH_ALEN) == 0) {
				wpa_printf(MSG_DEBUG,
					   "mesh: Process pending Authentication frame from %u.%06u seconds ago",
					   (unsigned int) age.sec,
					   (unsigned int) age.usec);
				os_memset(&fi, 0, sizeof(fi));
				ieee802_11_mgmt(
					data,
					wpabuf_head(data->mesh_pending_auth),
					wpabuf_len(data->mesh_pending_auth),
					&fi);
			}
			wpabuf_free(data->mesh_pending_auth);
			data->mesh_pending_auth = NULL;
		}
		return;
	}

	if (conf->security == MESH_CONF_SEC_NONE)
		mesh_mpm_plink_open(wpa_s, sta, PLINK_OPEN_SENT);
	else
		mesh_rsn_auth_sae_sta(wpa_s, sta);
}


void mesh_mpm_mgmt_rx(struct wpa_supplicant *wpa_s, struct rx_mgmt *rx_mgmt)
{
	struct hostapd_frame_info fi;

	os_memset(&fi, 0, sizeof(fi));
	fi.datarate = rx_mgmt->datarate;
	fi.ssi_signal = rx_mgmt->ssi_signal;
	ieee802_11_mgmt(wpa_s->ifmsh->bss[0], rx_mgmt->frame,
			rx_mgmt->frame_len, &fi);
}


static void mesh_mpm_plink_estab(struct wpa_supplicant *wpa_s,
				 struct sta_info *sta)
{
	struct hostapd_data *hapd = wpa_s->ifmsh->bss[0];
	struct mesh_conf *conf = wpa_s->ifmsh->mconf;
	u8 seq[6] = {};

	wpa_msg(wpa_s, MSG_INFO, "mesh plink with " MACSTR " established",
		MAC2STR(sta->addr));

	if (conf->security & MESH_CONF_SEC_AMPE) {
		wpa_drv_set_key(wpa_s, WPA_ALG_CCMP, sta->addr, 0, 0,
				seq, sizeof(seq), sta->mtk, sizeof(sta->mtk));
		wpa_drv_set_key(wpa_s, WPA_ALG_CCMP, sta->addr, 1, 0,
				seq, sizeof(seq),
				sta->mgtk, sizeof(sta->mgtk));
		wpa_drv_set_key(wpa_s, WPA_ALG_IGTK, sta->addr, 4, 0,
				seq, sizeof(seq),
				sta->mgtk, sizeof(sta->mgtk));

		wpa_hexdump_key(MSG_DEBUG, "mtk:", sta->mtk, sizeof(sta->mtk));
		wpa_hexdump_key(MSG_DEBUG, "mgtk:",
				sta->mgtk, sizeof(sta->mgtk));
	}

	wpa_mesh_set_plink_state(wpa_s, sta, PLINK_ESTAB);
	hapd->num_plinks++;

	sta->flags |= WLAN_STA_ASSOC;

	eloop_cancel_timeout(plink_timer, wpa_s, sta);

	/* Send ctrl event */
	wpa_msg_ctrl(wpa_s, MSG_INFO, MESH_PEER_CONNECTED MACSTR,
		     MAC2STR(sta->addr));
}


static void mesh_mpm_fsm(struct wpa_supplicant *wpa_s, struct sta_info *sta,
			 enum plink_event event)
{
	struct hostapd_data *hapd = wpa_s->ifmsh->bss[0];
	struct mesh_conf *conf = wpa_s->ifmsh->mconf;
	u16 reason = 0;

	wpa_msg(wpa_s, MSG_DEBUG, "MPM " MACSTR " state %s event %s",
		MAC2STR(sta->addr), mplstate[sta->plink_state],
		mplevent[event]);

	switch (sta->plink_state) {
	case PLINK_LISTEN:
		switch (event) {
		case CLS_ACPT:
			mesh_mpm_fsm_restart(wpa_s, sta);
			break;
		case OPN_ACPT:
			mesh_mpm_plink_open(wpa_s, sta, PLINK_OPEN_RCVD);
			mesh_mpm_send_plink_action(wpa_s, sta, PLINK_CONFIRM,
						   0);
			break;
		default:
			break;
		}
		break;
	case PLINK_OPEN_SENT:
		switch (event) {
		case OPN_RJCT:
		case CNF_RJCT:
			reason = WLAN_REASON_MESH_CONFIG_POLICY_VIOLATION;
			/* fall-through */
		case CLS_ACPT:
			wpa_mesh_set_plink_state(wpa_s, sta, PLINK_HOLDING);
			if (!reason)
				reason = WLAN_REASON_MESH_CLOSE_RCVD;
			eloop_register_timeout(
				conf->dot11MeshHoldingTimeout / 1000,
				(conf->dot11MeshHoldingTimeout % 1000) * 1000,
				plink_timer, wpa_s, sta);
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CLOSE, reason);
			break;
		case OPN_ACPT:
			/* retry timer is left untouched */
			wpa_mesh_set_plink_state(wpa_s, sta, PLINK_OPEN_RCVD);
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CONFIRM, 0);
			break;
		case CNF_ACPT:
			wpa_mesh_set_plink_state(wpa_s, sta, PLINK_CNF_RCVD);
			eloop_register_timeout(
				conf->dot11MeshConfirmTimeout / 1000,
				(conf->dot11MeshConfirmTimeout % 1000) * 1000,
				plink_timer, wpa_s, sta);
			break;
		default:
			break;
		}
		break;
	case PLINK_OPEN_RCVD:
		switch (event) {
		case OPN_RJCT:
		case CNF_RJCT:
			reason = WLAN_REASON_MESH_CONFIG_POLICY_VIOLATION;
			/* fall-through */
		case CLS_ACPT:
			wpa_mesh_set_plink_state(wpa_s, sta, PLINK_HOLDING);
			if (!reason)
				reason = WLAN_REASON_MESH_CLOSE_RCVD;
			eloop_register_timeout(
				conf->dot11MeshHoldingTimeout / 1000,
				(conf->dot11MeshHoldingTimeout % 1000) * 1000,
				plink_timer, wpa_s, sta);
			sta->mpm_close_reason = reason;
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CLOSE, reason);
			break;
		case OPN_ACPT:
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CONFIRM, 0);
			break;
		case CNF_ACPT:
			if (conf->security & MESH_CONF_SEC_AMPE)
				mesh_rsn_derive_mtk(wpa_s, sta);
			mesh_mpm_plink_estab(wpa_s, sta);
			break;
		default:
			break;
		}
		break;
	case PLINK_CNF_RCVD:
		switch (event) {
		case OPN_RJCT:
		case CNF_RJCT:
			reason = WLAN_REASON_MESH_CONFIG_POLICY_VIOLATION;
			/* fall-through */
		case CLS_ACPT:
			wpa_mesh_set_plink_state(wpa_s, sta, PLINK_HOLDING);
			if (!reason)
				reason = WLAN_REASON_MESH_CLOSE_RCVD;
			eloop_register_timeout(
				conf->dot11MeshHoldingTimeout / 1000,
				(conf->dot11MeshHoldingTimeout % 1000) * 1000,
				plink_timer, wpa_s, sta);
			sta->mpm_close_reason = reason;
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CLOSE, reason);
			break;
		case OPN_ACPT:
			mesh_mpm_plink_estab(wpa_s, sta);
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CONFIRM, 0);
			break;
		default:
			break;
		}
		break;
	case PLINK_ESTAB:
		switch (event) {
		case CLS_ACPT:
			wpa_mesh_set_plink_state(wpa_s, sta, PLINK_HOLDING);
			reason = WLAN_REASON_MESH_CLOSE_RCVD;

			eloop_register_timeout(
				conf->dot11MeshHoldingTimeout / 1000,
				(conf->dot11MeshHoldingTimeout % 1000) * 1000,
				plink_timer, wpa_s, sta);
			sta->mpm_close_reason = reason;

			wpa_msg(wpa_s, MSG_INFO, "mesh plink with " MACSTR
				" closed with reason %d",
				MAC2STR(sta->addr), reason);

			wpa_msg_ctrl(wpa_s, MSG_INFO,
				     MESH_PEER_DISCONNECTED MACSTR,
				     MAC2STR(sta->addr));

			hapd->num_plinks--;

			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CLOSE, reason);
			break;
		case OPN_ACPT:
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CONFIRM, 0);
			break;
		default:
			break;
		}
		break;
	case PLINK_HOLDING:
		switch (event) {
		case CLS_ACPT:
			mesh_mpm_fsm_restart(wpa_s, sta);
			break;
		case OPN_ACPT:
		case CNF_ACPT:
		case OPN_RJCT:
		case CNF_RJCT:
			reason = sta->mpm_close_reason;
			mesh_mpm_send_plink_action(wpa_s, sta,
						   PLINK_CLOSE, reason);
			break;
		default:
			break;
		}
		break;
	default:
		wpa_msg(wpa_s, MSG_DEBUG,
			"Unsupported MPM event %s for state %s",
			mplevent[event], mplstate[sta->plink_state]);
		break;
	}
}


void mesh_mpm_action_rx(struct wpa_supplicant *wpa_s,
			const struct ieee80211_mgmt *mgmt, size_t len)
{
	u8 action_field;
	struct hostapd_data *hapd = wpa_s->ifmsh->bss[0];
	struct mesh_conf *mconf = wpa_s->ifmsh->mconf;
	struct sta_info *sta;
	u16 plid = 0, llid = 0;
	enum plink_event event;
	struct ieee802_11_elems elems;
	struct mesh_peer_mgmt_ie peer_mgmt_ie;
	const u8 *ies;
	size_t ie_len;
	int ret;

	if (mgmt->u.action.category != WLAN_ACTION_SELF_PROTECTED)
		return;

	action_field = mgmt->u.action.u.slf_prot_action.action;
	if (action_field != PLINK_OPEN &&
	    action_field != PLINK_CONFIRM &&
	    action_field != PLINK_CLOSE)
		return;

	ies = mgmt->u.action.u.slf_prot_action.variable;
	ie_len = (const u8 *) mgmt + len -
		mgmt->u.action.u.slf_prot_action.variable;

	/* at least expect mesh id and peering mgmt */
	if (ie_len < 2 + 2) {
		wpa_printf(MSG_DEBUG,
			   "MPM: Ignore too short action frame %u ie_len %u",
			   action_field, (unsigned int) ie_len);
		return;
	}
	wpa_printf(MSG_DEBUG, "MPM: Received PLINK action %u", action_field);

	if (action_field == PLINK_OPEN || action_field == PLINK_CONFIRM) {
		wpa_printf(MSG_DEBUG, "MPM: Capability 0x%x",
			   WPA_GET_LE16(ies));
		ies += 2;	/* capability */
		ie_len -= 2;
	}
	if (action_field == PLINK_CONFIRM) {
		wpa_printf(MSG_DEBUG, "MPM: AID 0x%x", WPA_GET_LE16(ies));
		ies += 2;	/* aid */
		ie_len -= 2;
	}

	/* check for mesh peering, mesh id and mesh config IEs */
	if (ieee802_11_parse_elems(ies, ie_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "MPM: Failed to parse PLINK IEs");
		return;
	}
	if (!elems.peer_mgmt) {
		wpa_printf(MSG_DEBUG,
			   "MPM: No Mesh Peering Management element");
		return;
	}
	if (action_field != PLINK_CLOSE) {
		if (!elems.mesh_id || !elems.mesh_config) {
			wpa_printf(MSG_DEBUG,
				   "MPM: No Mesh ID or Mesh Configuration element");
			return;
		}

		if (!matches_local(wpa_s, &elems)) {
			wpa_printf(MSG_DEBUG,
				   "MPM: Mesh ID or Mesh Configuration element do not match local MBSS");
			return;
		}
	}

	ret = mesh_mpm_parse_peer_mgmt(wpa_s, action_field,
				       elems.peer_mgmt,
				       elems.peer_mgmt_len,
				       &peer_mgmt_ie);
	if (ret) {
		wpa_printf(MSG_DEBUG, "MPM: Mesh parsing rejected frame");
		return;
	}

	/* the sender's llid is our plid and vice-versa */
	plid = WPA_GET_LE16(peer_mgmt_ie.llid);
	if (peer_mgmt_ie.plid)
		llid = WPA_GET_LE16(peer_mgmt_ie.plid);
	wpa_printf(MSG_DEBUG, "MPM: plid=0x%x llid=0x%x", plid, llid);

	sta = ap_get_sta(hapd, mgmt->sa);

	/*
	 * If this is an open frame from an unknown STA, and this is an
	 * open mesh, then go ahead and add the peer before proceeding.
	 */
	if (!sta && action_field == PLINK_OPEN &&
	    !(mconf->security & MESH_CONF_SEC_AMPE))
		sta = mesh_mpm_add_peer(wpa_s, mgmt->sa, &elems);

	if (!sta) {
		wpa_printf(MSG_DEBUG, "MPM: No STA entry for peer");
		return;
	}

#ifdef CONFIG_SAE
	/* peer is in sae_accepted? */
	if (sta->sae && sta->sae->state != SAE_ACCEPTED) {
		wpa_printf(MSG_DEBUG, "MPM: SAE not yet accepted for peer");
		return;
	}
#endif /* CONFIG_SAE */

	if (!sta->my_lid)
		mesh_mpm_init_link(wpa_s, sta);

	if ((mconf->security & MESH_CONF_SEC_AMPE) &&
	    mesh_rsn_process_ampe(wpa_s, sta, &elems,
				  &mgmt->u.action.category,
				  ies, ie_len)) {
		wpa_printf(MSG_DEBUG, "MPM: RSN process rejected frame");
		return;
	}

	if (sta->plink_state == PLINK_BLOCKED) {
		wpa_printf(MSG_DEBUG, "MPM: PLINK_BLOCKED");
		return;
	}

	/* Now we will figure out the appropriate event... */
	switch (action_field) {
	case PLINK_OPEN:
		if (plink_free_count(hapd) == 0) {
			event = OPN_IGNR;
			wpa_printf(MSG_INFO,
				   "MPM: Peer link num over quota(%d)",
				   hapd->max_plinks);
		} else if (sta->peer_lid && sta->peer_lid != plid) {
			event = OPN_IGNR;
		} else {
			sta->peer_lid = plid;
			event = OPN_ACPT;
		}
		break;
	case PLINK_CONFIRM:
		if (plink_free_count(hapd) == 0) {
			event = CNF_IGNR;
			wpa_printf(MSG_INFO,
				   "MPM: Peer link num over quota(%d)",
				   hapd->max_plinks);
		} else if (sta->my_lid != llid ||
			   (sta->peer_lid && sta->peer_lid != plid)) {
			event = CNF_IGNR;
		} else {
			if (!sta->peer_lid)
				sta->peer_lid = plid;
			event = CNF_ACPT;
		}
		break;
	case PLINK_CLOSE:
		if (sta->plink_state == PLINK_ESTAB)
			/* Do not check for llid or plid. This does not
			 * follow the standard but since multiple plinks
			 * per cand are not supported, it is necessary in
			 * order to avoid a livelock when MP A sees an
			 * establish peer link to MP B but MP B does not
			 * see it. This can be caused by a timeout in
			 * B's peer link establishment or B being
			 * restarted.
			 */
			event = CLS_ACPT;
		else if (sta->peer_lid != plid)
			event = CLS_IGNR;
		else if (peer_mgmt_ie.plid && sta->my_lid != llid)
			event = CLS_IGNR;
		else
			event = CLS_ACPT;
		break;
	default:
		/*
		 * This cannot be hit due to the action_field check above, but
		 * compilers may not be able to figure that out and can warn
		 * about uninitialized event below.
		 */
		return;
	}
	mesh_mpm_fsm(wpa_s, sta, event);
}


/* called by ap_free_sta */
void mesh_mpm_free_sta(struct sta_info *sta)
{
	eloop_cancel_timeout(plink_timer, ELOOP_ALL_CTX, sta);
	eloop_cancel_timeout(mesh_auth_timer, ELOOP_ALL_CTX, sta);
}
