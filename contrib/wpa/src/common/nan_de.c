/*
 * NAN Discovery Engine
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/crc32.h"
#include "utils/list.h"
#include "crypto/crypto.h"
#include "crypto/sha256.h"
#include "ieee802_11_defs.h"
#include "nan/nan.h"
#include "nan_defs.h"
#include "nan_de.h"

const u8 nan_network_id[ETH_ALEN] =
{ 0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00 };

enum nan_de_service_type {
	NAN_DE_PUBLISH,
	NAN_DE_SUBSCRIBE,
};

const u8 p2p_network_id[ETH_ALEN] =
{ 0x51, 0x6f, 0x9a, 0x02, 0x00, 0x00 };

static const u8 wildcard_bssid[ETH_ALEN] =
{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

struct nan_de_service {
	int id;
	enum nan_de_service_type type;
	char *service_name;
	u8 service_id[NAN_SERVICE_ID_LEN];
	struct nan_publish_params publish;
	struct nan_subscribe_params subscribe;
	enum nan_service_protocol_type srv_proto_type;
	struct wpabuf *ssi;
	struct wpabuf *elems;
	struct os_reltime time_started;
	struct os_reltime end_time;
	struct os_reltime last_multicast;
	struct os_reltime first_discovered;
	bool needs_fsd;
	unsigned int freq;
	unsigned int default_freq;
	int *freq_list;
	u8 a3[ETH_ALEN];
	bool a3_set;

	/* Source MAC address for this service (optional) */
	u8 forced_addr[ETH_ALEN];
	bool forced_addr_set;

	/* pauseState information for Publish function */
	struct os_reltime pause_state_end;
	u8 sel_peer_id;
	u8 sel_peer_addr[ETH_ALEN];

	/* Publish state - channel iteration */
	bool in_multi_chan;
	bool first_multi_chan;
	int multi_chan_idx; /* index to freq_list[] */
	struct os_reltime next_publish_state;
	struct os_reltime next_publish_chan;
	unsigned int next_publish_duration;
	bool is_p2p;
	bool is_pr;
	bool listen_stopped;
	bool sync;

	/* Filters */
	struct wpabuf *matching_filter_tx;
	struct wpabuf *matching_filter_rx;

	bool srf_include;
	bool srf_type_bloom_filter;
	u8 srf_bf_idx;
	struct wpabuf *srf;
	bool close_proximity;
	bool gtk_required;
	bool data_path;
	bool security_required;

	/* Bootstrapping methods */
	u16 pbm;

	/* For Publish - int_array of supported cipher suites */
	int *cipher_suites_list;

	/* Bitmap of NAN_CS_INFO_CAPA_* */
	u8 security_capab;

	/* PMKID list for this service */
	struct dl_list pmkid_list;
};

#define NAN_DE_N_MIN 5
#define NAN_DE_N_MAX 10

#define NAN_DE_RSSI_CLOSE_PROXIMITY (-70) /* dBm */

struct nan_de_tracked_tx {
	struct dl_list list;
	u8 dst[ETH_ALEN];
	u32 cookie;
	u8 digest[SHA256_MAC_LEN];
	bool with_wait;
};

enum nan_de_flush_tracked_tx_reason {
	NAN_DE_FLUSH_TRACKED_TX_FLUSH_ALL,
	NAN_DE_FLUSH_TRACKED_TX_WAIT_EXPIRED,
};

struct nan_de {
	u8 nmi[ETH_ALEN];
	u8 cluster_id[ETH_ALEN];
	bool cluster_id_set;
	bool offload;
	bool ap;
	unsigned int max_listen;
	struct nan_callbacks cb;

	struct nan_de_service *service[NAN_DE_MAX_SERVICE];
	unsigned int num_service;

	int next_handle;

	unsigned int ext_listen_freq;
	unsigned int listen_freq;
	unsigned int tx_wait_status_freq;
	unsigned int tx_wait_end_freq;

	struct nan_de_cfg cfg;
	struct os_reltime suspend_cycle_start;

	int dw_freq;

	/* RSSI threshold for close proximity, or zero if not limited */
	int rssi_threshold;

	/*
	 * List of transmit requests (struct nan_de_tracked_tx::list) for which
	 * the caller requested status indicating whether the frame was
	 * acknowledged
	 */
	struct dl_list tracked_tx;

#ifdef CONFIG_TESTING_OPTIONS
	/*
	 * When set, multicast follow-up SDFs will be sent as Protected Dual of
	 * Public Action frames. This can be used to test protection of NAN
	 * multicast Management frames.
	 */
	bool tx_mcast_follow_up_prot;
#endif /* CONFIG_TESTING_OPTIONS */
};


bool nan_de_is_nan_network_id(const u8 *addr)
{
	return ether_addr_equal(addr, nan_network_id);
}


bool nan_de_is_p2p_network_id(const u8 *addr)
{
	return ether_addr_equal(addr, p2p_network_id);
}


struct nan_de * nan_de_init(const u8 *nmi, bool offload, bool ap,
			    unsigned int max_listen,
			    const struct nan_callbacks *cb)
{
	struct nan_de *de;

	de = os_zalloc(sizeof(*de));
	if (!de)
		return NULL;

	os_memcpy(de->nmi, nmi, ETH_ALEN);
	de->offload = offload;
	de->ap = ap;
	de->max_listen = max_listen ? max_listen : 1000;
	os_memcpy(&de->cb, cb, sizeof(*cb));

	de->cfg.n_min = NAN_DE_N_MIN;
	de->cfg.n_max = NAN_DE_N_MAX;

	de->rssi_threshold = NAN_DE_RSSI_CLOSE_PROXIMITY;
	dl_list_init(&de->tracked_tx);

	return de;
}


static void nan_de_service_free(struct nan_de_service *srv)
{
	os_free(srv->service_name);
	wpabuf_free(srv->ssi);
	wpabuf_free(srv->elems);
	wpabuf_free(srv->matching_filter_tx);
	wpabuf_free(srv->matching_filter_rx);
	wpabuf_free(srv->srf);
	os_free(srv->freq_list);
	os_free(srv->cipher_suites_list);
#ifdef CONFIG_NAN
	nan_crypto_clear_pmkid_list(&srv->pmkid_list);
#endif /* CONFIG_NAN */
	os_free(srv);
}


static void nan_de_service_deinit(struct nan_de *de, struct nan_de_service *srv,
				  enum nan_de_reason reason)
{
	if (!srv)
		return;
	if (srv->type == NAN_DE_PUBLISH && de->cb.publish_terminated)
		de->cb.publish_terminated(de->cb.ctx, srv->id, reason);
	if (srv->type == NAN_DE_SUBSCRIBE && de->cb.subscribe_terminated)
		de->cb.subscribe_terminated(de->cb.ctx, srv->id, reason);
	nan_de_service_free(srv);
}


static void nan_de_flush_tracked_tx(struct nan_de *de,
				    enum nan_de_flush_tracked_tx_reason reason)
{
	struct nan_de_tracked_tx *tx, *tmp;

	dl_list_for_each_safe(tx, tmp, &de->tracked_tx,
			      struct nan_de_tracked_tx, list) {
		if (reason == NAN_DE_FLUSH_TRACKED_TX_WAIT_EXPIRED &&
		    !tx->with_wait)
			continue;

		de->cb.transmit_req_status(de->cb.ctx, tx->cookie, false);
		dl_list_del(&tx->list);
		os_free(tx);
	}
}


static void nan_de_clear_pending(struct nan_de *de)
{
	nan_de_flush_tracked_tx(de, NAN_DE_FLUSH_TRACKED_TX_FLUSH_ALL);

	de->listen_freq = 0;
	de->tx_wait_status_freq = 0;
	de->tx_wait_end_freq = 0;
}


static int nan_de_track_tx_digest(const u8 *data, size_t len, u8 *digest)
{
	return sha256_vector(1, &data, &len, digest);
}


static struct nan_de_tracked_tx *
nan_de_add_tracked_tx(struct nan_de *de, const u8 *dst, bool with_wait,
		      u32 cookie, const struct wpabuf *buf)
{
	struct nan_de_tracked_tx *tx;
	u8 digest[SHA256_MAC_LEN];

	if (!de->cb.transmit_req_status) {
		wpa_printf(MSG_DEBUG,
			   "NAN: No tx_status callback, cannot track Tx");
		return NULL;
	}

	if (!cookie) {
		wpa_printf(MSG_DEBUG, "NAN: Invalid cookie for Tx tracking");
		return NULL;
	}

	if (nan_de_track_tx_digest(wpabuf_head(buf), wpabuf_len(buf), digest)) {
		wpa_printf(MSG_INFO, "NAN: Failed to compute Tx digest");
		return NULL;
	}

	dl_list_for_each(tx, &de->tracked_tx, struct nan_de_tracked_tx, list) {
		if (!ether_addr_equal(tx->dst, dst))
			continue;

		if (tx->cookie == cookie) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Already tracking Tx cookie %u to "
				   MACSTR, cookie, MAC2STR(dst));
			return NULL;
		}

		if (os_memcmp(tx->digest, digest, SHA256_MAC_LEN) == 0) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Already tracking identical payload to "
				   MACSTR " (cookie %u)",
				   MAC2STR(dst), tx->cookie);
			return NULL;
		}
	}

	tx = os_zalloc(sizeof(*tx));
	if (!tx)
		return NULL;

	os_memcpy(tx->dst, dst, ETH_ALEN);
	tx->cookie = cookie;
	tx->with_wait = with_wait;
	os_memcpy(tx->digest, digest, SHA256_MAC_LEN);

	dl_list_add(&de->tracked_tx, &tx->list);

	wpa_printf(MSG_DEBUG, "NAN: Track Tx cookie %u", tx->cookie);
	wpa_hexdump(MSG_DEBUG, "NAN: Track Tx digest",
		    tx->digest, SHA256_MAC_LEN);

	return tx;
}


static void nan_de_tx_status_match(struct nan_de *de, const u8 *data,
				   size_t len, u8 acked)
{
	struct nan_de_tracked_tx *tx;
	const struct ieee80211_mgmt *mgmt =
		(const struct ieee80211_mgmt *) data;
	const u8 *pos = (const u8 *) &mgmt->u.action;
	u8 digest[SHA256_MAC_LEN];

	if (len <= offsetof(struct ieee80211_mgmt, u.action))
		return;

	len = data + len - pos;

	if (nan_de_track_tx_digest(pos, len, digest))
		return;

	dl_list_for_each(tx, &de->tracked_tx,
			 struct nan_de_tracked_tx, list) {
		if (!ether_addr_equal(tx->dst, mgmt->da) ||
		    os_memcmp(tx->digest, digest, SHA256_MAC_LEN) != 0)
			continue;

		wpa_printf(MSG_DEBUG, "NAN: Tx status for cookie=%u ack=%u",
			   tx->cookie, acked);

		de->cb.transmit_req_status(de->cb.ctx, tx->cookie, acked);
		dl_list_del(&tx->list);
		os_free(tx);
		return;
	}
}


void nan_de_flush(struct nan_de *de)
{
	unsigned int i;

	if (!de)
		return;

	for (i = 0; i < NAN_DE_MAX_SERVICE; i++) {
		nan_de_service_deinit(de, de->service[i],
				      NAN_DE_REASON_USER_REQUEST);
		de->service[i] = NULL;
	}

	de->num_service = 0;
	nan_de_clear_pending(de);
}


static void nan_de_pause_state(struct nan_de_service *srv, const u8 *peer_addr,
			       u8 peer_id)
{
	wpa_printf(MSG_DEBUG, "NAN: Start pauseState");
	os_get_reltime(&srv->pause_state_end);
	srv->pause_state_end.sec += 60;
	if (os_reltime_initialized(&srv->end_time) &&
	    os_reltime_before(&srv->end_time, &srv->pause_state_end))
		srv->pause_state_end = srv->end_time;
	os_memcpy(srv->sel_peer_addr, peer_addr, ETH_ALEN);
	srv->sel_peer_id = peer_id;
}


static void nan_de_unpause_state(struct nan_de_service *srv)
{
	wpa_printf(MSG_DEBUG, "NAN: Stop pauseState");
	srv->pause_state_end.sec = 0;
	srv->pause_state_end.usec = 0;
	os_memset(srv->sel_peer_addr, 0, ETH_ALEN);
	srv->sel_peer_id = 0;
}


static struct wpabuf * nan_de_alloc_sdf(struct nan_de *de, const u8 *dst,
					size_t len,
					enum nan_service_control_type type)
{
	struct wpabuf *buf;
	u8 category = WLAN_ACTION_PUBLIC;

	if (de->cb.is_peer_paired && de->cb.is_peer_paired(de->cb.ctx, dst))
		category = WLAN_ACTION_PROTECTED_DUAL;

#ifdef CONFIG_TESTING_OPTIONS
	if (de->tx_mcast_follow_up_prot &&
	    is_multicast_ether_addr(dst) &&
	    type == NAN_SRV_CTRL_FOLLOW_UP) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Send multicast follow-up as protected");
		category = WLAN_ACTION_PROTECTED_DUAL;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	buf = wpabuf_alloc(2 + 4 + len);
	if (buf) {
		wpabuf_put_u8(buf, category);
		wpabuf_put_u8(buf, WLAN_PA_VENDOR_SPECIFIC);
		wpabuf_put_be32(buf, NAN_SDF_VENDOR_TYPE);
	}

	return buf;
}


static int nan_de_tx(struct nan_de *de, unsigned int freq,
		     unsigned int wait_time,
		     const u8 *dst, const u8 *src, const u8 *bssid,
		     const struct wpabuf *buf,  u32 *cookie)
{
	struct nan_de_tracked_tx *tracked_tx = NULL;
	int res;

	if (!de->cb.tx)
		return -1;

	if (cookie) {
		tracked_tx = nan_de_add_tracked_tx(de, dst, !!wait_time,
						   *cookie, buf);
		if (!tracked_tx)
			return -1;
	}

	res = de->cb.tx(de->cb.ctx, freq, wait_time, dst, src, bssid, buf);
	if (res < 0) {
		if (tracked_tx) {
			dl_list_del(&tracked_tx->list);
			os_free(tracked_tx);
		}
		return res;
	}

	de->tx_wait_status_freq = freq;
	de->tx_wait_end_freq = wait_time ? freq : 0;

	return res;
}


static void nan_buf_add_npba(const struct nan_de *de,
			     const struct nan_de_service *srv,
			     struct wpabuf *buf)
{
	u8 type_and_status = NAN_PBA_TYPE_ADVERTISE |
		(NAN_PBA_STATUS_ACCEPTED << NAN_PBA_STATUS_POS);

	wpa_printf(MSG_DEBUG, "NAN: Add NPBA");

	wpabuf_put_u8(buf, NAN_ATTR_NPBA);
	wpabuf_put_le16(buf, 5);

	/* Dialog token is reserved (0) for advertise */
	wpabuf_put_u8(buf, 0);
	wpabuf_put_u8(buf, type_and_status);
	wpabuf_put_u8(buf, NAN_REASON_RESERVED);
	wpabuf_put_le16(buf, srv->pbm);
}


static void nan_de_tx_sdf(struct nan_de *de, struct nan_de_service *srv,
			  unsigned int wait_time,
			  enum nan_service_control_type type,
			  const u8 *dst, const u8 *a3, u8 req_instance_id,
			  const struct wpabuf *ssi,
			  const struct wpabuf *attrs,  u32 *cookie)
{
	struct wpabuf *buf;
	size_t len = 0, sda_len, sdea_len;
	u8 ctrl = type;
	u16 sdea_ctrl = 0;
	const u8 *forced_addr;
	size_t cs_num = int_array_len(srv->cipher_suites_list);

	/* Service Descriptor attribute */
	sda_len = NAN_SERVICE_ID_LEN + 1 + 1 + 1;
	if (srv->matching_filter_tx && wpabuf_len(srv->matching_filter_tx)) {
		sda_len += wpabuf_len(srv->matching_filter_tx) + 1;
		ctrl |= NAN_SRV_CTRL_MATCHING_FILTER;
	}

	if (srv->srf && wpabuf_len(srv->srf)) {
		/* SRF length + SRF control */
		sda_len += 1 + 1 + wpabuf_len(srv->srf);
		ctrl |= NAN_SRV_CTRL_RESP_FILTER;
	}

	if ((srv->type == NAN_DE_SUBSCRIBE || srv->type == NAN_DE_PUBLISH) &&
	    srv->close_proximity)
		ctrl |= NAN_SRV_CTRL_DISCOVERY_RANGE_LIMITED;

	len += NAN_ATTR_HDR_LEN + sda_len;

	/* Service Descriptor Extension attribute */
	sdea_len = 1 + 2;
	if (ssi)
		sdea_len += 2 + 4 + wpabuf_len(ssi);
	len += NAN_ATTR_HDR_LEN + sdea_len;

	/* Element Container attribute */
	if (srv->elems)
		len += NAN_ATTR_HDR_LEN + 1 + wpabuf_len(srv->elems);

	/* NPBA (dialog token, type and status, reason, pbm) */
	if (srv->pbm && type != NAN_SRV_CTRL_FOLLOW_UP)
		len += NAN_ATTR_HDR_LEN + 1 + 1 + 1 + 2;

	/* Reserve some additional space for extra attributes */
	if (de->cb.add_extra_attrs)
		len += 256;

	len += attrs ? wpabuf_len(attrs) : 0;

	/* Cipher Suite Information Attribute */
	if (srv->type == NAN_DE_PUBLISH && srv->cipher_suites_list) {
		len += NAN_ATTR_HDR_LEN + sizeof(struct nan_cipher_suite_info) +
			cs_num * sizeof(struct nan_cipher_suite);
	}

	/* Security Context Information Attribute */
	if (srv->type == NAN_DE_PUBLISH && !dl_list_empty(&srv->pmkid_list)) {
		unsigned int list_len = dl_list_len(&srv->pmkid_list);

		/* Each entry: sizeof(nan_sec_ctxt) + PMKID_LEN */
		len += NAN_ATTR_HDR_LEN +
			list_len * (sizeof(struct nan_sec_ctxt) + PMKID_LEN);
	}

	buf = nan_de_alloc_sdf(de, dst, len, type);
	if (!buf)
		return;

	/* Service Descriptor attribute */
	wpabuf_put_u8(buf, NAN_ATTR_SDA);
	wpabuf_put_le16(buf, sda_len);
	wpabuf_put_data(buf, srv->service_id, NAN_SERVICE_ID_LEN);
	wpabuf_put_u8(buf, srv->id); /* Instance ID */
	wpabuf_put_u8(buf, req_instance_id); /* Requestor Instance ID */
	wpabuf_put_u8(buf, ctrl);

	if (ctrl & NAN_SRV_CTRL_MATCHING_FILTER) {
		wpabuf_put_u8(buf, wpabuf_len(srv->matching_filter_tx));
		wpabuf_put_buf(buf, srv->matching_filter_tx);
	}

	if (ctrl & NAN_SRV_CTRL_RESP_FILTER) {
		u8 srf_ctrl = 0;

		if (srv->srf_type_bloom_filter)
			srf_ctrl = NAN_SRF_CTRL_BF;

		if (srv->srf_include)
			srf_ctrl |= NAN_SRF_CTRL_INCLUDE;

		srf_ctrl |= (srv->srf_bf_idx & NAN_SRF_CTRL_BF_IDX_MSK) <<
			NAN_SRF_CTRL_BF_IDX_POS;
		wpabuf_put_u8(buf, wpabuf_len(srv->srf) + 1);
		wpabuf_put_u8(buf, srf_ctrl);
		wpabuf_put_buf(buf, srv->srf);
	}

	/* Service Descriptor Extension attribute */
	if (srv->type == NAN_DE_PUBLISH || ssi) {
		if (srv->type == NAN_DE_PUBLISH) {
			if (srv->publish.fsd)
				sdea_ctrl |= NAN_SDEA_CTRL_FSD_REQ;
			if (srv->publish.fsd_gas)
				sdea_ctrl |= NAN_SDEA_CTRL_FSD_GAS;
			if (srv->gtk_required)
				sdea_ctrl |= NAN_SDEA_CTRL_GTK_REQ;
			if (srv->data_path) {
				sdea_ctrl |= NAN_SDEA_CTRL_DATA_PATH_REQ;
				if (srv->cipher_suites_list &&
				    srv->security_required)
					sdea_ctrl |= NAN_SDEA_CTRL_SECURITY_REQ;
			}
		}

		if (sdea_ctrl || ssi) {
			wpabuf_put_u8(buf, NAN_ATTR_SDEA);
			wpabuf_put_le16(buf, sdea_len);
			wpabuf_put_u8(buf, srv->id); /* Instance ID */
			wpabuf_put_le16(buf, sdea_ctrl);
			if (ssi) {
				wpabuf_put_le16(buf, 4 + wpabuf_len(ssi));
				wpabuf_put_be24(buf, OUI_WFA);
				wpabuf_put_u8(buf, srv->srv_proto_type);
				wpabuf_put_buf(buf, ssi);
			}
		}
	}

	/* Element Container attribute */
	if (srv->elems) {
		wpabuf_put_u8(buf, NAN_ATTR_ELEM_CONTAINER);
		wpabuf_put_le16(buf, 1 + wpabuf_len(srv->elems));
		wpabuf_put_u8(buf, 0); /* Map ID */
		wpabuf_put_buf(buf, srv->elems);
	}

	/* Use per-service source address if configured, otherwise use NMI */
	forced_addr = srv->forced_addr_set ? srv->forced_addr : de->nmi;

	if (srv->pbm && type != NAN_SRV_CTRL_FOLLOW_UP)
		nan_buf_add_npba(de, srv, buf);

	if (de->cb.add_extra_attrs)
		de->cb.add_extra_attrs(de->cb.ctx, buf);

	if (attrs) {
		wpa_printf(MSG_DEBUG, "NAN: Add extra NAN attributes");
		wpabuf_put_buf(buf, attrs);
	}

	if (srv->type == NAN_DE_PUBLISH && srv->cipher_suites_list) {
		size_t i;

		wpabuf_put_u8(buf, NAN_ATTR_CSIA);
		wpabuf_put_le16(buf, sizeof(struct nan_cipher_suite_info) +
				cs_num * sizeof(struct nan_cipher_suite));
		wpabuf_put_u8(buf, srv->security_capab);
		for (i = 0; i < cs_num; i++) {
			wpabuf_put_u8(buf, (u8) srv->cipher_suites_list[i]);
			wpabuf_put_u8(buf, srv->id);
		}
	}

	if (srv->type == NAN_DE_PUBLISH && !dl_list_empty(&srv->pmkid_list)) {
		struct nan_de_pmkid *pmkid;
		u8 *len_ptr;

		wpabuf_put_u8(buf, NAN_ATTR_SCIA);
		len_ptr = wpabuf_put(buf, 2); /* length filled later */

		dl_list_for_each(pmkid, &srv->pmkid_list, struct nan_de_pmkid,
				 list) {
			wpabuf_put_le16(buf, PMKID_LEN);
			wpabuf_put_u8(buf, NAN_SEC_CTX_TYPE_ND_PMKID);
			wpabuf_put_u8(buf, srv->id);
			wpabuf_put_data(buf, pmkid->pmkid, PMKID_LEN);
		}

		WPA_PUT_LE16(len_ptr, (u8 *) wpabuf_put(buf, 0) - len_ptr - 2);
	}

	nan_de_tx(de, srv->sync ? 0 : srv->freq, srv->sync ? 0 : wait_time,
		  dst, forced_addr, a3, buf, cookie);
	wpabuf_free(buf);
}


static int nan_de_time_to_next_chan_change(struct nan_de_service *srv)
{
	struct os_reltime tmp, diff, now;

	if (os_reltime_before(&srv->next_publish_state,
			      &srv->next_publish_chan))
		tmp = srv->next_publish_state;
	else if (srv->in_multi_chan)
		tmp = srv->next_publish_chan;
	else
		tmp = srv->next_publish_state;

	os_get_reltime(&now);
	os_reltime_sub(&tmp, &now, &diff);
	return os_reltime_in_ms(&diff);
}


static void nan_de_set_publish_times(struct nan_de_service *srv)
{
	os_get_reltime(&srv->next_publish_state);
	srv->next_publish_chan = srv->next_publish_state;
	/* Swap single/multi channel state in N * 100 TU */
	os_reltime_add_ms(&srv->next_publish_state,
			  srv->next_publish_duration * 1024 / 1000);

	/* Swap channel in multi channel state after 150 ms */
	os_reltime_add_ms(&srv->next_publish_chan, 150);
}


static void nan_de_check_chan_change(struct nan_de_service *srv)
{
	if (srv->next_publish_duration) {
		/* Update end times for the first operation of the publish
		 * iteration */
		nan_de_set_publish_times(srv);
		srv->next_publish_duration = 0;
	} else if (srv->in_multi_chan) {
		if (!os_reltime_initialized(&srv->pause_state_end)) {
			srv->multi_chan_idx++;
			if (srv->freq_list[srv->multi_chan_idx] == 0)
				srv->multi_chan_idx = 0;
			srv->freq = srv->freq_list[srv->multi_chan_idx];
			wpa_printf(MSG_DEBUG,
				   "NAN: Publish multi-channel change to %u MHz",
				   srv->freq);
		}
		os_get_reltime(&srv->next_publish_chan);
		os_reltime_add_ms(&srv->next_publish_chan, 150);
	}
}


static void nan_de_tx_multicast(struct nan_de *de, struct nan_de_service *srv,
				u8 req_instance_id)
{
	enum nan_service_control_type type;
	unsigned int wait_time = 100;
	const u8 *network_id;
	const u8 *bssid;

	if (srv->type == NAN_DE_PUBLISH) {
		int ms;

		type = NAN_SRV_CTRL_PUBLISH;

		if (!srv->sync) {
			nan_de_check_chan_change(srv);
			ms = nan_de_time_to_next_chan_change(srv);
			if (ms < 100)
				ms = 100;
			wait_time = ms;
		}
	} else if (srv->type == NAN_DE_SUBSCRIBE) {
		type = NAN_SRV_CTRL_SUBSCRIBE;
	} else {
		return;
	}

	if (srv->is_p2p) {
		network_id = p2p_network_id;
		bssid = wildcard_bssid;
	} else {
		network_id = nan_network_id;
		bssid = nan_network_id;
	}

	if (srv->sync) {
		if (!de->cluster_id_set || !de->dw_freq) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Cluster ID or DW frequency are not set - skip sync TX");
			return;
		}

		wait_time = 0;
		bssid = de->cluster_id;
	}

	nan_de_tx_sdf(de, srv, wait_time, type, network_id, bssid,
		      req_instance_id, srv->ssi, NULL, NULL);
	os_get_reltime(&srv->last_multicast);
}


static void nan_de_add_srv(struct nan_de *de, struct nan_de_service *srv)
{
	int ttl;

	os_get_reltime(&srv->time_started);
	ttl = srv->type == NAN_DE_PUBLISH ? srv->publish.ttl :
		srv->subscribe.ttl;
	if (ttl) {
		srv->end_time = srv->time_started;
		srv->end_time.sec += ttl;
	}

	de->service[srv->id - 1] = srv;
	de->num_service++;
}


static void nan_de_del_srv(struct nan_de *de, struct nan_de_service *srv,
			   enum nan_de_reason reason)
{
	de->service[srv->id - 1] = NULL;
	nan_de_service_deinit(de, srv, reason);
	de->num_service--;
	if (de->num_service == 0)
		nan_de_clear_pending(de);
}


static bool nan_de_srv_expired(struct nan_de_service *srv,
			       struct os_reltime *now)
{
	if (os_reltime_initialized(&srv->end_time))
		return os_reltime_before(&srv->end_time, now);

	if (srv->type == NAN_DE_PUBLISH) {
		/* Time out after one transmission (and wait for FSD) */
		if (!os_reltime_initialized(&srv->last_multicast))
			return false;
		if (!srv->publish.fsd)
			return true;
	}

	if (srv->type == NAN_DE_SUBSCRIBE) {
		/* Time out after first DiscoveryResult event (and wait for
		 * FSD) */
		if (!os_reltime_initialized(&srv->first_discovered))
			return false;
		if (!srv->needs_fsd)
			return true;
	}

	return false;
}


static int nan_de_next_multicast(struct nan_de *de, struct nan_de_service *srv,
				 struct os_reltime *now)
{
	unsigned int period;
	struct os_reltime next, diff;

	if (srv->type == NAN_DE_PUBLISH && !srv->publish.unsolicited)
		return -1;
	if (srv->type == NAN_DE_SUBSCRIBE && !srv->subscribe.active)
		return -1;

	if (!os_reltime_initialized(&srv->last_multicast))
		return 0;

	if (srv->type == NAN_DE_PUBLISH && srv->publish.ttl == 0)
		return -1;

	if (srv->type == NAN_DE_PUBLISH &&
	    os_reltime_initialized(&srv->pause_state_end))
		return -1;

	period = srv->type == NAN_DE_PUBLISH ?
		srv->publish.announcement_period :
		srv->subscribe.query_period;
	if (period == 0)
		period = 100;
	next = srv->last_multicast;
	os_reltime_add_ms(&next, period);

	if (srv->type == NAN_DE_PUBLISH) {
		if (!de->tx_wait_end_freq && srv->publish.unsolicited &&
		    os_reltime_before(&next, now))
			return 0;
		next = srv->next_publish_state;
	}

	if (os_reltime_before(&next, now))
		return 0;

	os_reltime_sub(&next, now, &diff);
	return os_reltime_in_ms(&diff);
}


static int nan_de_srv_time_to_next(struct nan_de *de,
				   struct nan_de_service *srv,
				   struct os_reltime *now)
{
	struct os_reltime diff;
	int next = -1, tmp;

	if (os_reltime_initialized(&srv->end_time)) {
		os_reltime_sub(&srv->end_time, now, &diff);
		tmp = os_reltime_in_ms(&diff);
		if (next == -1 || tmp < next)
			next = tmp;
	}

	if (srv->type == NAN_DE_PUBLISH &&
	    srv->publish.fsd &&
	    os_reltime_initialized(&srv->pause_state_end)) {
		os_reltime_sub(&srv->pause_state_end, now, &diff);
		tmp = os_reltime_in_ms(&diff);
		if (next == -1 || tmp < next)
			next = tmp;
		return next;
	}

	tmp = nan_de_next_multicast(de, srv, now);
	if (tmp >= 0 && (next == -1 || tmp < next))
		next = tmp;

	if (srv->type == NAN_DE_PUBLISH &&
	    os_reltime_initialized(&srv->last_multicast)) {
		/* Time out after one transmission (and wait for FSD) */
		tmp = srv->publish.fsd ? 1000 : 100;
		if (next == -1 || tmp < next)
			next = tmp;
	}

	if (srv->type == NAN_DE_SUBSCRIBE &&
	    os_reltime_initialized(&srv->first_discovered)) {
		/* Time out after first DiscoveryResult event (and wait for
		 * FSD) */
		tmp = srv->needs_fsd ? 1000 : 100;
		if (next == -1 || tmp < next)
			next = tmp;
	}

	if (os_reltime_initialized(&srv->next_publish_state)) {
		os_reltime_sub(&srv->next_publish_state, now, &diff);
		if (diff.sec < 0 || (diff.sec == 0 && diff.usec < 0))
			tmp = 0;
		else
			tmp = os_reltime_in_ms(&diff);
		if (next == -1 || tmp < next)
			next = tmp;
	}

	return next;
}


static void nan_de_start_new_publish_state(struct nan_de *de,
					   struct nan_de_service *srv,
					   bool force_single)
{
	unsigned int n;

	if (srv->sync)
		return;

	if (force_single || !srv->freq_list || srv->freq_list[0] == 0)
		srv->in_multi_chan = false;
	else
		srv->in_multi_chan = !srv->in_multi_chan;

	/* Use same values for N and M. */
	n = de->cfg.n_min + os_random() % (de->cfg.n_max - de->cfg.n_min);
	srv->next_publish_duration = n * 100;

	nan_de_set_publish_times(srv);

	if (os_reltime_initialized(&srv->pause_state_end))
		return;

	if (srv->in_multi_chan && srv->freq_list && srv->freq_list[0]) {
		if (!srv->first_multi_chan)
			srv->multi_chan_idx++;
		if (srv->freq_list[srv->multi_chan_idx] == 0)
			srv->multi_chan_idx = 0;
		srv->first_multi_chan = false;
		srv->freq = srv->freq_list[srv->multi_chan_idx];
	} else {
		srv->freq = srv->default_freq;
	}

	wpa_printf(MSG_DEBUG,
		   "NAN: Publish in %s channel state for %u TU; starting with %u MHz",
		   srv->in_multi_chan ? "multi" : "single", n * 100, srv->freq);
}


static u32 nan_de_listen_duration(struct nan_de *de, struct nan_de_service *srv)
{
	u32 duration = 1000;
	u32 max_duration = de->max_listen;

	/* Limit the listen duration based on the maximal 'N' value */
	if (de->cfg.n_max && de->cfg.n_max * 100 < max_duration)
		max_duration = de->cfg.n_max * 100;

	if (srv->type == NAN_DE_PUBLISH) {
		nan_de_check_chan_change(srv);
		duration = nan_de_time_to_next_chan_change(srv);
		if (duration < 150)
			duration = 150;
	}

	return MIN(duration, max_duration);
}


static void nan_de_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct nan_de *de = eloop_ctx;
	unsigned int i;
	int next = -1;
	bool started = false;
	struct os_reltime now;

	os_get_reltime(&now);

	/* Based on the USD specification, the device should always be either on
	 * the default channel or one of the configured channels. However, to
	 * allow operation of other interfaces, suspend the USD functionality
	 * based on the cycle and suspend parameters. This would lower the
	 * probability of service discovery, but would allow functionality of
	 * other interfaces.
	 */
	if (!de->listen_freq && de->cfg.cycle) {
		u32 diff_ms;

		if (os_reltime_initialized(&de->suspend_cycle_start)) {
			struct os_reltime diff;

			os_reltime_sub(&now, &de->suspend_cycle_start, &diff);
			diff_ms = os_reltime_in_ms(&diff);
		} else {
			/* We want to start a new cycle */
			diff_ms = de->cfg.cycle;
		}

		if (diff_ms < de->cfg.suspend) {
			wpa_printf(MSG_DEBUG,
				   "NAN: USD: Suspend in progress: diff_ms=%u",
				   diff_ms);

			/* Set the timer to fire at the end of the suspend */
			diff_ms = de->cfg.suspend - diff_ms;
		} else if (diff_ms >= de->cfg.cycle) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Suspend USD for %u ms (passed=%u ms)",
				   de->cfg.suspend, diff_ms);
			de->suspend_cycle_start = now;

			/* Set the timer to fire at the end of the suspend */
			diff_ms = de->cfg.suspend;
		} else {
			diff_ms = 0;
		}

		if (diff_ms) {
			wpa_printf(MSG_DEBUG, "NAN: diff_ms=%u ms", diff_ms);

			eloop_register_timeout(diff_ms / 1000,
					       (diff_ms % 1000) * 1000,
					       nan_de_timer, de, NULL);
			return;
		}
	}

	for (i = 0; i < NAN_DE_MAX_SERVICE; i++) {
		struct nan_de_service *srv = de->service[i];
		int srv_next;

		if (!srv)
			continue;

		if (nan_de_srv_expired(srv, &now)) {
			wpa_printf(MSG_DEBUG, "NAN: Service id %d expired",
				   srv->id);
			if (srv->type == NAN_DE_PUBLISH &&
			    de->cb.offload_cancel_publish)
				de->cb.offload_cancel_publish(de->cb.ctx,
							      srv->id);
			if (srv->type == NAN_DE_SUBSCRIBE &&
			    de->cb.offload_cancel_subscribe)
				de->cb.offload_cancel_subscribe(de->cb.ctx,
								srv->id);
			nan_de_del_srv(de, srv, NAN_DE_REASON_TIMEOUT);
			continue;
		}

		if (srv->sync)
			continue;

		if (os_reltime_initialized(&srv->next_publish_state) &&
		    os_reltime_before(&srv->next_publish_state, &now))
			nan_de_start_new_publish_state(de, srv, false);

		if (srv->type == NAN_DE_PUBLISH &&
		    os_reltime_initialized(&srv->pause_state_end) &&
		    (os_reltime_before(&srv->pause_state_end, &now)))
			nan_de_unpause_state(srv);

		srv_next = nan_de_srv_time_to_next(de, srv, &now);
		if (srv_next >= 0 && (next == -1 || srv_next < next))
			next = srv_next;

		if (srv->type == NAN_DE_PUBLISH &&
		    srv->publish.fsd &&
		    os_reltime_initialized(&srv->pause_state_end) &&
		    de->tx_wait_end_freq == 0 &&
		    de->listen_freq == 0 && de->ext_listen_freq == 0) {
			struct os_reltime diff;
			int duration;

			os_reltime_sub(&srv->pause_state_end, &now, &diff);
			duration = os_reltime_in_ms(&diff);
			if (duration < 0)
				continue;
			if (srv->listen_stopped) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Publisher listen stopped temporarily - do not start driver listen operation");
				continue;
			}
			if ((unsigned int) duration > de->max_listen)
				duration = de->max_listen;
			if (de->cb.listen(de->cb.ctx, srv->freq, duration,
					  srv->forced_addr_set ?
					  srv->forced_addr : NULL) == 0) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Publisher in pauseState - started listen on %u MHz",
					   srv->freq);
				de->listen_freq = srv->freq;
				return;
			}
		}

		if (srv_next == 0 && !started && !de->offload &&
		    de->listen_freq == 0 && de->ext_listen_freq == 0 &&
		    de->tx_wait_end_freq == 0 &&
		    nan_de_next_multicast(de, srv, &now) == 0) {
			started = true;
			nan_de_tx_multicast(de, srv, 0);
		}

		if (!started && !de->offload && de->cb.listen &&
		    de->listen_freq == 0 && de->ext_listen_freq == 0 &&
		    de->tx_wait_end_freq == 0 &&
		    ((srv->type == NAN_DE_PUBLISH &&
		      !srv->publish.unsolicited && srv->publish.solicited) ||
		     (srv->type == NAN_DE_SUBSCRIBE &&
		      !srv->subscribe.active))) {
			u32 duration;

			if (srv->listen_stopped) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Listen stopped temporarily - do not start driver listen operation");
				continue;
			}

			duration = nan_de_listen_duration(de, srv);

			started = true;
			if (de->cb.listen(de->cb.ctx, srv->freq, duration,
					  srv->forced_addr_set ?
					  srv->forced_addr : NULL) == 0)
				de->listen_freq = srv->freq;
		}

	}

	if (next < 0)
		return;

	if (next == 0)
		next = 1;

	eloop_register_timeout(next / 1000, (next % 1000) * 1000, nan_de_timer,
			       de, NULL);
}


static void nan_de_run_timer(struct nan_de *de)
{
	eloop_cancel_timeout(nan_de_timer, de, NULL);
	eloop_register_timeout(0, 0, nan_de_timer, de, NULL);
}


void nan_de_deinit(struct nan_de *de)
{
	eloop_cancel_timeout(nan_de_timer, de, NULL);
	nan_de_flush(de);
	os_free(de);
}


void nan_de_listen_started(struct nan_de *de, unsigned int freq,
			   unsigned int duration)
{
	if (freq != de->listen_freq)
		de->ext_listen_freq = freq;
}


void nan_de_listen_ended(struct nan_de *de, unsigned int freq)
{
	if (freq == de->ext_listen_freq)
		de->ext_listen_freq = 0;

	if (freq == de->listen_freq) {
		de->listen_freq = 0;
		nan_de_run_timer(de);
	}
}


void nan_de_update_nmi(struct nan_de *de, const u8 *nmi)
{
	if (de)
		os_memcpy(de->nmi, nmi, ETH_ALEN);
}


void nan_de_tx_status(struct nan_de *de, unsigned int freq, const u8 *dst,
		      const u8 *data, size_t data_len, bool ack)
{
	if (freq == de->tx_wait_status_freq)
		de->tx_wait_status_freq = 0;

	nan_de_tx_status_match(de, data, data_len, ack);
}


void nan_de_tx_wait_ended(struct nan_de *de)
{
	if (de->tx_wait_end_freq)
		wpa_printf(MSG_DEBUG,
			   "NAN: TX wait for response ended (freq=%u)",
			   de->tx_wait_end_freq);

	nan_de_flush_tracked_tx(de, NAN_DE_FLUSH_TRACKED_TX_WAIT_EXPIRED);

	de->tx_wait_end_freq = 0;
	nan_de_run_timer(de);
}


static const u8 *
nan_de_get_attr(const u8 *buf, size_t len, enum nan_attr_id id,
		unsigned int skip)
{
	const u8 *pos = buf, *end = buf + len;

	while (end - pos >= NAN_ATTR_HDR_LEN) {
		const u8 *attr = pos;
		u8 attr_id;
		u16 attr_len;

		attr_id = *pos++;
		attr_len = WPA_GET_LE16(pos);
		pos += 2;
		if (attr_len > end - pos) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Truncated attribute %u (len %u; left %zu)",
				   attr_id, attr_len, end - pos);
			break;
		}

		if (attr_id == id) {
			if (skip == 0)
				return attr;
			skip--;
		}

		pos += attr_len;
	}

	return NULL;
}


static void nan_de_get_sdea(const u8 *buf, size_t len, u8 instance_id,
			    u16 *sdea_control,
			    enum nan_service_protocol_type *srv_proto_type,
			    const u8 **ssi, size_t *ssi_len)
{
	unsigned int skip;
	const u8 *sdea, *end;
	u16 sdea_len;

	for (skip = 0; ; skip++) {
		sdea = nan_de_get_attr(buf, len, NAN_ATTR_SDEA, skip);
		if (!sdea)
			break;

		sdea++;
		sdea_len = WPA_GET_LE16(sdea);
		sdea += 2;
		if (sdea_len < 1 + 2)
			continue;
		end = sdea + sdea_len;

		if (instance_id != *sdea++)
			continue; /* Mismatching Instance ID */

		*sdea_control = WPA_GET_LE16(sdea);
		sdea += 2;

		if (*sdea_control & NAN_SDEA_CTRL_RANGE_LIMIT) {
			if (end - sdea < 4)
				continue;
			sdea += 4;
		}

		if (*sdea_control & NAN_SDEA_CTRL_SRV_UPD_INDIC) {
			if (end - sdea < 1)
				continue;
			sdea++;
		}

		if (end - sdea >= 2) {
			u16 srv_info_len;

			srv_info_len = WPA_GET_LE16(sdea);
			sdea += 2;

			if (srv_info_len > end - sdea)
				continue;

			if (srv_info_len >= 4 &&
			    WPA_GET_BE24(sdea) == OUI_WFA) {
				*srv_proto_type = sdea[3];
				*ssi = sdea + 4;
				*ssi_len = srv_info_len - 4;
			}
		}
	}
}


static unsigned int nan_de_parse_csia(const u8 *buf, size_t len, u8 instance_id,
				      u8 *cipher_suites,
				      unsigned int max_cipher_suites,
				      u8 *capabilities)
{
	const u8 *csia, *pos, *end;
	u16 csia_len;
	unsigned int cs_count = 0;
	const struct nan_cipher_suite_info *cs_info;

	csia = nan_de_get_attr(buf, len, NAN_ATTR_CSIA, 0);
	if (!csia)
		return 0;

	csia++;
	csia_len = WPA_GET_LE16(csia);
	csia += 2;

	if (csia_len < 1)
		return 0;

	wpa_printf(MSG_DEBUG,
		   "NAN: Parsing Cipher Suite Information attribute (len=%u)",
		   csia_len);

	cs_info = (const struct nan_cipher_suite_info *) csia;

	if (capabilities)
		*capabilities = cs_info->capab;

	pos = cs_info->cs;
	end = csia + csia_len;

	/* Parse cipher suite list. Each entry is 2 bytes (csid + publish_id) */
	while (end - pos >= 2 && cs_count < max_cipher_suites) {
		u8 csid = *pos++;
		u8 publish_id = *pos++;

		if (csid == NAN_CS_NONE || csid >= NAN_CS_MAX) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Invalid cipher suite ID %u for publish ID %u",
				   csid, publish_id);
			continue;
		}

		wpa_printf(MSG_DEBUG,
			   "NAN: Cipher suite ID %u for publish ID %u",
			   csid, publish_id);

		/* Only include cipher suites for the matching publish ID */
		if (publish_id == instance_id) {
			cipher_suites[cs_count++] = csid;
			wpa_printf(MSG_DEBUG,
				   "NAN: Added cipher suite %u for matching publish ID %u",
				   csid, instance_id);
		}
	}

	wpa_printf(MSG_DEBUG,
		   "NAN: Parsed %u cipher suites from CSIA for publish ID %u",
		   cs_count, instance_id);

	return cs_count;
}


static unsigned int nan_de_parse_scia(const u8 *buf, size_t len, u8 instance_id,
				      u8 *pmkid_list, unsigned int max_pmkids)
{
	const u8 *scia, *end;
	u16 scia_len;
	unsigned int pmkid_count = 0;

	scia = nan_de_get_attr(buf, len, NAN_ATTR_SCIA, 0);
	if (!scia)
		return 0;

	scia++;
	scia_len = WPA_GET_LE16(scia);
	scia += 2;

	end = scia + scia_len;

	wpa_printf(MSG_DEBUG,
		   "NAN: Parsing Security Context Information attribute (len=%u)",
		   scia_len);

	/* Parse list of Security Context Identifiers */
	while ((size_t) (end - scia) >= sizeof(struct nan_sec_ctxt)) {
		const struct nan_sec_ctxt *sec_ctx =
			(const struct nan_sec_ctxt *) scia;
		u16 scid_len = le_to_host16(sec_ctx->len);

		if (scid_len + sizeof(*sec_ctx) > (size_t) (end - scia)) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Invalid SCID length %u (remaining %zu)",
				   scid_len, (size_t) (end - scia));
			break;
		}

		/* Check if this is for our instance_id and is a PMKID type */
		if (sec_ctx->scid == NAN_SEC_CTX_TYPE_ND_PMKID &&
		    sec_ctx->instance_id == instance_id) {
			if (scid_len == PMKID_LEN && pmkid_count < max_pmkids) {
				os_memcpy(&pmkid_list[pmkid_count * PMKID_LEN],
					  sec_ctx->ctxt, PMKID_LEN);
				pmkid_count++;
				wpa_hexdump(MSG_DEBUG, "NAN: Parsed PMKID",
					    sec_ctx->ctxt, PMKID_LEN);
			} else {
				wpa_printf(MSG_DEBUG,
					   "NAN: Unexpected SCID length %u or max PMKIDs reached",
					   scid_len);
			}
		}

		scia += scid_len + sizeof(*sec_ctx);
	}

	wpa_printf(MSG_DEBUG, "NAN: Parsed %u PMKIDs from SCIA", pmkid_count);

	return pmkid_count;
}


static void nan_de_process_elem_container(struct nan_de *de, const u8 *buf,
					  size_t len, const u8 *peer_addr,
					  unsigned int freq, bool p2p, bool pr)
{
	const u8 *elem;
	u16 elem_len;

	elem = nan_de_get_attr(buf, len, NAN_ATTR_ELEM_CONTAINER, 0);
	if (!elem)
		return;

	elem++;
	elem_len = WPA_GET_LE16(elem);
	elem += 2;
	/* Skip the attribute if there is not enough froom for an element. */
	if (elem_len < 1 + 2)
		return;

	/* Skip Map ID */
	elem++;
	elem_len--;

	if (p2p && de->cb.process_p2p_usd_elems)
		de->cb.process_p2p_usd_elems(de->cb.ctx, elem, elem_len,
					     peer_addr, freq);
	if (pr && de->cb.process_pr_usd_elems)
		de->cb.process_pr_usd_elems(de->cb.ctx, elem, elem_len,
					     peer_addr, freq);
}


static void nan_de_parse_dcea(const u8 *buf, size_t len, bool *pairing_setup,
			      bool *npk_nik_caching)
{
	const u8 *dcea;
	u16 dcea_len;

	*pairing_setup = false;
	*npk_nik_caching = false;

	dcea = nan_de_get_attr(buf, len, NAN_ATTR_DCEA, 0);
	if (!dcea)
		return;

	dcea_len = WPA_GET_LE16(dcea + 1);
	if (dcea_len < 2) {
		wpa_printf(MSG_DEBUG, "NAN: DCEA length=%u too short",
			   dcea_len);
		return;
	}

	*pairing_setup =  !!(dcea[4] & NAN_DEV_CAPA_EXT_INFO_1_PAIRING_SETUP);
	*npk_nik_caching = !!(dcea[4] &
			      NAN_DEV_CAPA_EXT_INFO_1_NPK_NIK_CACHING);
}


static u16 nan_de_get_advertise_pbm(const u8 *buf, size_t len)
{
	const u8 *npba;
	u16 npba_len;

	npba = nan_de_get_attr(buf, len, NAN_ATTR_NPBA, 0);
	if (!npba)
		return 0;

	npba_len = WPA_GET_LE16(npba + 1);
	if (npba_len < 5) {
		wpa_printf(MSG_DEBUG, "NAN: Invalid NPBA length %u", npba_len);
		return 0;
	}

	/* Skip the attribute ID and length */
	npba += NAN_ATTR_HDR_LEN;
	if ((npba[1] & NAN_PBA_TYPE_MASK) != NAN_PBA_TYPE_ADVERTISE)
		return 0;

	return WPA_GET_LE16(npba + 3);
}


static bool nan_de_filter_match(struct nan_de_service *srv,
				const u8 *matching_filter,
				size_t matching_filter_len)
{
	const u8 *spos, *spos_end, *ppos, *ppos_end;
	const u8 *publish_filter = NULL, *subscribe_filter = NULL;
	u8 publish_filter_len = 0, subscribe_filter_len = 0;

	wpa_printf(MSG_DEBUG,
		   "NAN: Check matching filter for service id %d type %d",
		   srv->id, srv->type);

	if (srv->type == NAN_DE_PUBLISH) {
		if (srv->matching_filter_rx) {
			publish_filter =
				wpabuf_head_u8(srv->matching_filter_rx);
			publish_filter_len =
				wpabuf_len(srv->matching_filter_rx);
		}
		subscribe_filter = matching_filter;
		subscribe_filter_len = matching_filter_len;
	} else if (srv->type == NAN_DE_SUBSCRIBE) {
		if (srv->matching_filter_rx) {
			subscribe_filter =
				wpabuf_head_u8(srv->matching_filter_rx);
			subscribe_filter_len =
				wpabuf_len(srv->matching_filter_rx);
		}
		publish_filter = matching_filter;
		publish_filter_len = matching_filter_len;
	} else {
		wpa_printf(MSG_DEBUG,
			   "NAN: Unsupported service type %d for matching filter",
			   srv->type);
		return false;
	}

	if (!subscribe_filter)
		return true;

	spos = subscribe_filter;
	spos_end = subscribe_filter + subscribe_filter_len;

	ppos = publish_filter;
	ppos_end = publish_filter ? publish_filter + publish_filter_len : NULL;

	wpa_hexdump(MSG_DEBUG, "NAN: subscribe filter",
		    spos, spos_end - spos);
	if (ppos)
		wpa_hexdump(MSG_DEBUG, "NAN: publish filter",
			    ppos, ppos_end - ppos);

	while (spos < spos_end) {
		u8 slen, plen = 0;

		slen = *spos++;

		/* Invalid filter length - do not match */
		if (slen > spos_end - spos)
			return false;

		/* Read publish filter */
		if (ppos && ppos < ppos_end) {
			plen = *ppos++;
			if (plen > ppos_end - ppos)
				return false;
		}

		if (slen > 0) {
			if (!ppos)
				return false;

			/* For non zero filters, compare */
			if (plen &&
			    (plen != slen || os_memcmp(spos, ppos, plen) != 0))
				return false;

			/* Filter matches */
		}

		spos += slen;

		/*
		 * If ppos is NULL we can still have match if the subscribe
		 * filter is <0><0>...
		 */
		if (!ppos)
			continue;

		ppos += plen;

		/* Publish filter is over */
		if (ppos >= ppos_end && spos < spos_end)
			return false;
	}

	return true;
}


static bool nan_de_rx_publish(struct nan_de *de, struct nan_de_service *srv,
			      const u8 *peer_addr, const u8 *a3, u8 instance_id,
			      const u8 *matching_filter,
			      size_t matching_filter_len,
			      u8 req_instance_id, u16 sdea_control,
			      enum nan_service_protocol_type srv_proto_type,
			      const u8 *ssi, size_t ssi_len,
			      bool range_limit, int rssi,
			      const u8 *buf, size_t buf_len)
{
	struct nan_discovery_result res;

	/* The SCIA can potentially contain a PMKID for each cipher suite */
	u8 pmkid_list[(NAN_CS_MAX - 1) * PMKID_LEN];
	unsigned int pmkid_count = 0;
	/* Cipher suites from CSIA */
	u8 cipher_suites[NAN_CS_MAX - 1];
	unsigned int cipher_suite_count = 0;

	if (!nan_de_filter_match(srv, matching_filter, matching_filter_len))
		return false;

	/* Skip USD logic */
	if (srv->sync)
		goto send_event;

	if ((range_limit || srv->close_proximity) &&
	    de->rssi_threshold && rssi) {
		if (rssi < de->rssi_threshold) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Discard SDA with RSSI %d below threshold %d",
				   rssi, de->rssi_threshold);
			return false;
		}
	}

	/* Subscribe function processing of a receive Publish message */
	if (!os_reltime_initialized(&srv->first_discovered)) {
		os_get_reltime(&srv->first_discovered);
		srv->needs_fsd = sdea_control & NAN_SDEA_CTRL_FSD_REQ;
		nan_de_run_timer(de);
	}

	if (!de->offload && srv->subscribe.active && req_instance_id == 0) {
		/* Active subscriber replies with a Subscribe message if it
		 * received a matching unsolicited Publish message. */
		nan_de_tx_multicast(de, srv, instance_id);
	}

	if (!de->offload && !srv->subscribe.active && req_instance_id == 0) {
		/* Passive subscriber replies with a Follow-up message without
		 * Service Specific Info field if it received a matching
		 * unsolicited Publish message. */
		nan_de_transmit(de, srv->id, NULL, NULL, peer_addr,
				instance_id, NULL, NULL);
	}

send_event:
	os_memset(&res, 0, sizeof(res));
	if (buf && buf_len > 0) {
		/* Parse Cipher Suite Information Attribute */
		cipher_suite_count = nan_de_parse_csia(
			buf, buf_len, instance_id, cipher_suites,
			ARRAY_SIZE(cipher_suites), NULL);

		/* Parse Security Context Information attribute */
		pmkid_count = nan_de_parse_scia(buf, buf_len, instance_id,
						pmkid_list,
						sizeof(pmkid_list) / PMKID_LEN);

		/*
		 * Parse Device Capability Extension attribute for pairing
		 * setup and NPK/NIK caching support
		 */
		nan_de_parse_dcea(buf, buf_len,
				  &res.pairing_setup_supp,
				  &res.npk_nik_caching_supp);

		/* Get the bootstrapping methods */
		res.pbm = nan_de_get_advertise_pbm(buf, buf_len);
	}

	res.subscribe_id = srv->id;
	res.srv_proto_type = srv_proto_type;
	res.ssi = ssi;
	res.ssi_len = ssi_len;
	res.peer_publish_id = instance_id;
	res.peer_addr = peer_addr;
	res.fsd = !!(sdea_control & NAN_SDEA_CTRL_FSD_REQ);
	res.fsd_gas = !!(sdea_control & NAN_SDEA_CTRL_FSD_GAS);
	res.data_path = !!(sdea_control & NAN_SDEA_CTRL_DATA_PATH_REQ);
	res.security_required = !!(sdea_control & NAN_SDEA_CTRL_SECURITY_REQ);
	res.cipher_suites = cipher_suite_count > 0 ? cipher_suites : NULL;
	res.n_cipher_suites = cipher_suite_count;
	res.pmkid_list = pmkid_count > 0 ? pmkid_list : NULL;
	res.pmkid_count = pmkid_count;

	if (de->cb.discovery_result)
		de->cb.discovery_result(de->cb.ctx, &res);

	return true;
}


static bool nan_de_rx_subscribe(struct nan_de *de, struct nan_de_service *srv,
				const u8 *peer_addr, const u8 *a3,
				u8 instance_id,
				const u8 *matching_filter,
				size_t matching_filter_len,
				enum nan_service_protocol_type srv_proto_type,
				const u8 *ssi, size_t ssi_len,
				bool range_limit, int rssi)
{
	const u8 *network_id;

	/* Publish function processing of a receive Subscribe message */

	if (!nan_de_filter_match(srv, matching_filter, matching_filter_len))
		return false;

	if ((range_limit || srv->close_proximity) &&
	    de->rssi_threshold && rssi) {
		if (rssi < de->rssi_threshold) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Discard SDA with RSSI %d below threshold %d",
				   rssi, de->rssi_threshold);
			return false;
		}
	}

	if (!srv->publish.solicited)
		return false;

	if (os_reltime_initialized(&srv->pause_state_end) &&
	    (!ether_addr_equal(peer_addr, srv->sel_peer_addr) ||
	     instance_id != srv->sel_peer_id)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: In pauseState - ignore Subscribe message from another subscriber");
		return false;
	}

	if (de->offload)
		goto offload;

	/* Reply with a solicited Publish message */

	if (srv->is_p2p)
		network_id = p2p_network_id;
	else
		network_id = nan_network_id;

	if (srv->sync && de->cluster_id_set)
		a3 = de->cluster_id;
	else if (srv->publish.solicited_multicast || !a3)
		a3 = network_id;
	else if (srv->is_p2p)
		a3 = de->nmi;

	nan_de_tx_sdf(de, srv, 100, NAN_SRV_CTRL_PUBLISH,
		      srv->publish.solicited_multicast ?
		      network_id : peer_addr, a3, instance_id, srv->ssi, NULL,
		      NULL);

	if (!srv->is_p2p && !srv->sync)
		nan_de_pause_state(srv, peer_addr, instance_id);

offload:
	if (!srv->publish.disable_events && de->cb.replied)
		de->cb.replied(de->cb.ctx, srv->id, peer_addr, instance_id,
			       srv_proto_type, ssi, ssi_len);

	return true;
}


static bool nan_de_rx_follow_up(struct nan_de *de, struct nan_de_service *srv,
				const u8 *peer_addr, const u8 *a3,
				u8 instance_id, const u8 *ssi, size_t ssi_len,
				const u8 *buf, size_t len)
{
	/* Follow-up function processing of a receive Follow-up message for a
	 * Subscribe or Publish instance */

	if (srv->type == NAN_DE_PUBLISH &&
	    os_reltime_initialized(&srv->pause_state_end) &&
	    (!ether_addr_equal(peer_addr, srv->sel_peer_addr) ||
	     instance_id != srv->sel_peer_id ||
	     !ssi)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: In pauseState - ignore Follow-up message from another subscriber or without ssi");
		return false;
	}

	if (srv->type == NAN_DE_PUBLISH && !ssi && !srv->sync)
		nan_de_pause_state(srv, peer_addr, instance_id);

	os_memcpy(srv->a3, a3, ETH_ALEN);
	srv->a3_set = true;

	if (de->cb.receive)
		de->cb.receive(de->cb.ctx, srv->id, instance_id, ssi, ssi_len,
			       peer_addr, buf, len);

	return true;
}


static bool nan_check_bloom_filter(const u8 *nmi, const u8 *bf,
				   size_t bf_len, u8 bf_idx)
{
	u8 a_j_x[1 + ETH_ALEN];
	int j;
	u32 crc;

	for (j = 4 * bf_idx; j < 4 * (bf_idx + 1); j++) {
		a_j_x[0] = j;
		os_memcpy(&a_j_x[1], nmi, ETH_ALEN);
		crc = (~ieee80211_crc32(a_j_x, 1 + ETH_ALEN)) & 0xFFFF;
		crc %= bf_len * 8;
		if (!(bf[crc / 8] & BIT(crc % 8)))
			return false;
	}

	return true;
}


static bool nan_srf_match(struct nan_de *de, const u8 *srf, size_t srf_len)
{
	u8 srf_ctrl;
	bool srf_type_bf;
	bool include;
	u8 srf_bf_idx;

	if (srf_len < 1)
		return false;

	srf_ctrl = *srf++;
	srf_len--;

	srf_type_bf = !!(srf_ctrl & NAN_SRF_CTRL_BF);
	include = !!(srf_ctrl & NAN_SRF_CTRL_INCLUDE);
	srf_bf_idx = (srf_ctrl >> NAN_SRF_CTRL_BF_IDX_POS) &
		NAN_SRF_CTRL_BF_IDX_MSK;

	if (srf_type_bf) {
		if (srf_len == 0)
			return false;
		if (nan_check_bloom_filter(de->nmi, srf, srf_len, srf_bf_idx))
			return include;
	} else {
		/* MAC Address filter */
		while (srf_len >= ETH_ALEN) {
			if (ether_addr_equal(srf, de->nmi))
				return include;

			srf += ETH_ALEN;
			srf_len -= ETH_ALEN;
		}
	}

	return !include;
}


static bool nan_de_rx_sda(struct nan_de *de, const u8 *peer_addr, const u8 *a3,
			  unsigned int freq, const u8 *buf, size_t len,
			  const u8 *sda, size_t sda_len, int rssi)
{
	const u8 *service_id;
	u8 instance_id, req_instance_id, ctrl;
	u16 sdea_control = 0;
	unsigned int i;
	enum nan_service_control_type type = 0;
	enum nan_service_protocol_type srv_proto_type = 0;
	const u8 *ssi = NULL;
	size_t ssi_len = 0;
	bool first = true;
	const u8 *end;
	const u8 *matching_filter = NULL;
	size_t matching_filter_len = 0;
	bool ret = false;

	if (sda_len < NAN_SERVICE_ID_LEN + 1 + 1 + 1)
		return false;
	end = sda + sda_len;

	service_id = sda;
	sda += NAN_SERVICE_ID_LEN;
	instance_id = *sda++;
	req_instance_id = *sda++;
	ctrl = *sda++;
	type = ctrl & NAN_SRV_CTRL_TYPE_MASK;
	wpa_printf(MSG_DEBUG,
		   "NAN: SDA - Service ID %02x%02x%02x%02x%02x%02x Instance ID %u Requestor Instance ID %u Service Control 0x%x (Service Control Type %u)",
		   MAC2STR(service_id), instance_id, req_instance_id,
		   ctrl, type);
	if (type != NAN_SRV_CTRL_PUBLISH &&
	    type != NAN_SRV_CTRL_SUBSCRIBE &&
	    type != NAN_SRV_CTRL_FOLLOW_UP) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Discard SDF with unknown Service Control Type %u",
			   type);
		return false;
	}

	if (ctrl & NAN_SRV_CTRL_BINDING_BITMAP) {
		if (end - sda < 2)
			return false;
		sda += 2;
	}

	if (ctrl & NAN_SRV_CTRL_MATCHING_FILTER) {
		u8 flen;

		if (end - sda < 1)
			return false;
		flen = *sda++;
		if (end - sda < flen)
			return false;
		matching_filter = sda;
		matching_filter_len = flen;
		sda += flen;
	}

	if (ctrl & NAN_SRV_CTRL_RESP_FILTER) {
		u8 flen;

		if (end - sda < 1)
			return false;
		flen = *sda++;
		if (end - sda < flen)
			return false;

		if (!nan_srf_match(de, sda, flen)) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Discard SDA with non-matching SRF");
			return false;
		}

		sda += flen;
	}

	if (ctrl & NAN_SRV_CTRL_SRV_INFO) {
		u8 flen;

		if (end - sda < 1)
			return false;
		flen = *sda++;
		if (end - sda < flen)
			return false;

		if (flen) {
			/* This case of SSI in SDA does not have an explicit
			 * indication of a service protocol type unlike the
			 * SDEA case. For now, leave srv_proto_type to 0 for
			 * this SDA case since that is a reserved value for the
			 * SDEA cases. */
			ssi = sda;
			ssi_len = flen;
			wpa_hexdump(MSG_MSGDUMP, "NAN: ssi", ssi, ssi_len);
		}
		sda += flen;
	}

	for (i = 0; i < NAN_DE_MAX_SERVICE; i++) {
		struct nan_de_service *srv = de->service[i];

		if (!srv)
			continue;
		if (os_memcmp(srv->service_id, service_id,
			      NAN_SERVICE_ID_LEN) != 0)
			continue;
		if (type == NAN_SRV_CTRL_PUBLISH) {
			if (srv->type == NAN_DE_PUBLISH)
				continue;
			if (req_instance_id && srv->id != req_instance_id)
				continue;
		}
		if (type == NAN_SRV_CTRL_SUBSCRIBE &&
		    srv->type == NAN_DE_SUBSCRIBE)
			continue;
		wpa_printf(MSG_DEBUG, "NAN: Received SDF matches service ID %u",
			   i + 1);

		if (first) {
			first = false;
			nan_de_get_sdea(buf, len, instance_id, &sdea_control,
					&srv_proto_type, &ssi, &ssi_len);

			if (ssi) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Service Protocol Type %d",
					   srv_proto_type);
				wpa_hexdump(MSG_MSGDUMP, "NAN: ssi",
					    ssi, ssi_len);
			}
			nan_de_process_elem_container(de, buf, len, peer_addr,
						      freq, srv->is_p2p,
						      srv->is_pr);
		}

		switch (type) {
		case NAN_SRV_CTRL_PUBLISH:
			ret |= nan_de_rx_publish(
				de, srv, peer_addr, a3, instance_id,
				matching_filter, matching_filter_len,
				req_instance_id, sdea_control, srv_proto_type,
				ssi, ssi_len,
				ctrl & NAN_SRV_CTRL_DISCOVERY_RANGE_LIMITED,
				rssi, buf, len);
			break;
		case NAN_SRV_CTRL_SUBSCRIBE:
			ret |= nan_de_rx_subscribe(
				de, srv, peer_addr, a3, instance_id,
				matching_filter, matching_filter_len,
				srv_proto_type, ssi, ssi_len,
				ctrl & NAN_SRV_CTRL_DISCOVERY_RANGE_LIMITED,
				rssi);
			break;
		case NAN_SRV_CTRL_FOLLOW_UP:
			ret |= nan_de_rx_follow_up(de, srv, peer_addr, a3,
						   instance_id, ssi, ssi_len,
						   buf, len);
			break;
		}
	}

	return ret;
}


bool nan_de_rx_sdf(struct nan_de *de, const u8 *peer_addr, const u8 *a3,
		   unsigned int freq, const u8 *buf, size_t len, int rssi)
{
	const u8 *sda;
	u16 sda_len;
	unsigned int skip;
	bool ret = false;

	if (!de->num_service)
		return false;

	wpa_printf(MSG_DEBUG, "NAN: RX SDF from " MACSTR
		   " freq=%u len=%zu rssi=%d",
		   MAC2STR(peer_addr), freq, len, rssi);

	wpa_hexdump(MSG_MSGDUMP, "NAN: SDF payload", buf, len);

	for (skip = 0; ; skip++) {
		sda = nan_de_get_attr(buf, len, NAN_ATTR_SDA, skip);
		if (!sda)
			break;

		sda++;
		sda_len = WPA_GET_LE16(sda);
		sda += 2;
		ret |= nan_de_rx_sda(de, peer_addr, a3, freq, buf, len,
				     sda, sda_len, rssi);
	}

	return ret;
}


static int nan_de_get_handle(struct nan_de *de)
{
	int i = de->next_handle;

	if (de->num_service >= NAN_DE_MAX_SERVICE)
		goto fail;

	do {
		if (!de->service[i]) {
			de->next_handle = (i + 1) % NAN_DE_MAX_SERVICE;
			return i + 1;
		}
		i = (i + 1) % NAN_DE_MAX_SERVICE;
	} while (i != de->next_handle);

fail:
	wpa_printf(MSG_DEBUG, "NAN: No more room for a new service");
	return -1;
}


static int nan_de_derive_service_id(struct nan_de_service *srv)
{
	u8 hash[SHA256_MAC_LEN];
	char *name, *pos;
	int ret;
	const u8 *addr[1];
	size_t len[1];

	name = os_strdup(srv->service_name);
	if (!name)
		return -1;
	pos = name;
	while (*pos) {
		*pos = tolower(*pos);
		pos++;
	}

	addr[0] = (u8 *) name;
	len[0] = os_strlen(name);
	ret = sha256_vector(1, addr, len, hash);
	os_free(name);
	if (ret == 0)
		os_memcpy(srv->service_id, hash, NAN_SERVICE_ID_LEN);

	return ret;
}


const u8 * nan_de_get_service_id(struct nan_de *de, int id)
{
	struct nan_de_service *srv;

	if (id < 1 || id > NAN_DE_MAX_SERVICE)
		return NULL;
	srv = de->service[id - 1];
	if (!srv)
		return NULL;
	return srv->service_id;
}


int nan_de_publish(struct nan_de *de, const char *service_name,
		   enum nan_service_protocol_type srv_proto_type,
		   const struct wpabuf *ssi, const struct wpabuf *elems,
		   struct nan_publish_params *params, bool p2p,
		   const u8 *addr)
{
	int publish_id;
	struct nan_de_service *srv;

	if (!service_name && !params->proximity_ranging) {
		wpa_printf(MSG_DEBUG, "NAN: Publish() - no service_name");
		return -1;
	}

	if (!params->unsolicited && !params->solicited) {
		wpa_printf(MSG_INFO,
			   "NAN: Publish() - both unsolicited and solicited disabled is invalid");
		return -1;
	}

	if (params->proximity_ranging && params->solicited && !elems) {
		wpa_printf(MSG_INFO,
			   "NAN: Unable to fetch proximity ranging params");
		return -1;
	}

	if (params->sync && !de->cluster_id_set) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Publish() - can't publish sync, cluster id is not set");
		return -1;
	}

	if (p2p && params->sync) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Publish() - P2P is not supported with sync");
		return -1;
	}

	publish_id = nan_de_get_handle(de);
	if (publish_id < 1)
		return -1;

	srv = os_zalloc(sizeof(*srv));
	if (!srv)
		return -1;
	srv->type = NAN_DE_PUBLISH;
	srv->freq = srv->default_freq = params->freq;

	if (service_name) {
		srv->service_name = os_strdup(service_name);
		if (!srv->service_name)
			goto fail;
	}

	if (params->proximity_ranging && !service_name)
		os_memset(srv->service_id, 0, NAN_SERVICE_ID_LEN);
	else if (nan_de_derive_service_id(srv) < 0)
		goto fail;

	os_memcpy(&srv->publish, params, sizeof(*params));

	if (params->freq_list) {
		size_t len;

		len = (int_array_len(params->freq_list) + 1) * sizeof(int);
		srv->freq_list = os_memdup(params->freq_list, len);
		if (!srv->freq_list)
			goto fail;
	}
	srv->publish.freq_list = NULL;

	srv->srv_proto_type = srv_proto_type;
	if (ssi) {
		srv->ssi = wpabuf_dup(ssi);
		if (!srv->ssi)
			goto fail;
	}
	if (elems) {
		srv->elems = wpabuf_dup(elems);
		if (!srv->elems)
			goto fail;
	}

	if (params->match_filter_rx) {
		srv->matching_filter_rx =
			wpabuf_parse_bin(params->match_filter_rx);
		if (!srv->matching_filter_rx ||
		    wpabuf_len(srv->matching_filter_rx) > 255) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to parse RX matching filter");
			goto fail;
		}
	}

	if (params->match_filter_tx) {
		srv->matching_filter_tx =
			wpabuf_parse_bin(params->match_filter_tx);
		if (!srv->matching_filter_tx ||
		    wpabuf_len(srv->matching_filter_tx) > 255) {
			wpa_printf(MSG_INFO,
				   "NAN: Failed to parse TX matching filter");
			goto fail;
		}
	}

	srv->sync = params->sync;

	if (addr && params->forced_addr) {
		os_memcpy(srv->forced_addr, addr, ETH_ALEN);
		srv->forced_addr_set = true;
		wpa_printf(MSG_DEBUG, "NAN: Using source address " MACSTR
			   " for publish service", MAC2STR(srv->forced_addr));
	}

	srv->security_capab = params->security_capab;

	if (params->cipher_suites_list) {
		int i = 0;

		while (params->cipher_suites_list[i] && i < NAN_CS_MAX) {
			if (params->cipher_suites_list[i] >= NAN_CS_MAX) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Invalid cipher suite %d in publish",
					   params->cipher_suites_list[i]);
				goto fail;
			}

			i++;
		}

		srv->cipher_suites_list =
			int_array_dup(params->cipher_suites_list);
		if (!srv->cipher_suites_list)
			goto fail;
	}

	dl_list_init(&srv->pmkid_list);
#ifdef CONFIG_NAN
	if (nan_crypto_pmkid_list(&srv->pmkid_list, de->nmi, srv->service_id,
				  srv->cipher_suites_list, params->nd_pmk) < 0)
		goto fail;
#endif /* CONFIG_NAN */

	/* Prepare for single and multi-channel states; starting with
	 * single channel */
	srv->first_multi_chan = true;
	nan_de_start_new_publish_state(de, srv, true);

	wpa_printf(MSG_DEBUG, "NAN: Assigned new publish handle %d for %s",
		   publish_id, service_name ? service_name : "Ranging");
	srv->id = publish_id;
	srv->is_p2p = p2p;
	srv->is_pr = params->proximity_ranging && params->solicited;
	srv->close_proximity = params->close_proximity;
	srv->pbm = params->pbm;
	srv->gtk_required = params->gtk_required;
	srv->data_path = params->data_path;
	srv->security_required = params->security_required;

	nan_de_add_srv(de, srv);
	nan_de_run_timer(de);
	return publish_id;
fail:
	nan_de_service_free(srv);
	return -1;
}


void nan_de_cancel_publish(struct nan_de *de, int publish_id)
{
	struct nan_de_service *srv;

	wpa_printf(MSG_DEBUG, "NAN: CancelPublish(publish_id=%d)", publish_id);

	if (publish_id < 1 || publish_id > NAN_DE_MAX_SERVICE)
		return;
	srv = de->service[publish_id - 1];
	if (!srv || srv->type != NAN_DE_PUBLISH)
		return;
	nan_de_del_srv(de, srv, NAN_DE_REASON_USER_REQUEST);
}


int nan_de_update_publish(struct nan_de *de, int publish_id,
			  const struct wpabuf *ssi)
{
	struct nan_de_service *srv;

	wpa_printf(MSG_DEBUG, "NAN: UpdatePublish(publish_id=%d)", publish_id);

	if (publish_id < 1 || publish_id > NAN_DE_MAX_SERVICE)
		return -1;
	srv = de->service[publish_id - 1];
	if (!srv || srv->type != NAN_DE_PUBLISH)
		return -1;

	wpabuf_free(srv->ssi);
	srv->ssi = NULL;
	if (!ssi)
		return 0;
	srv->ssi = wpabuf_dup(ssi);
	if (!srv->ssi)
		return -1;
	return 0;
}


int nan_de_unpause_publish(struct nan_de *de, int publish_id,
			   u8 peer_instance_id, const u8 *peer_addr)
{
	struct nan_de_service *srv;

	wpa_printf(MSG_DEBUG,
		   "NAN: UnpausePublish(publish_id=%d, peer_instance_id=%d peer_addr="
		   MACSTR ")",
		   publish_id, peer_instance_id, MAC2STR(peer_addr));

	if (publish_id < 1 || publish_id > NAN_DE_MAX_SERVICE)
		return -1;
	srv = de->service[publish_id - 1];
	if (!srv || srv->type != NAN_DE_PUBLISH)
		return -1;

	if (srv->sel_peer_id != peer_instance_id ||
	    !ether_addr_equal(peer_addr, srv->sel_peer_addr) ||
	    !os_reltime_initialized(&srv->pause_state_end))
		return -1;

	nan_de_unpause_state(srv);
	return 0;
}


static void bloom_filter_add(u8 *bf, u8 bf_idx, u8 bf_len, const u8 *mac)
{
	u8 a_j_x[1 + ETH_ALEN];
	int j;
	u32 crc;

	for (j = 4 * bf_idx; j < 4 * (bf_idx + 1); j++) {
		a_j_x[0] = j;
		os_memcpy(&a_j_x[1], mac, ETH_ALEN);
		crc = (~ieee80211_crc32(a_j_x, 1 + ETH_ALEN)) & 0xFFFF;
		crc %= bf_len * 8;
		bf[crc / 8] |= 1 << (crc % 8);
	}
}


static struct wpabuf * nan_build_bloom_filter(const char *srf_mac_list,
					      u8 srf_bf_len, u8 srf_bf_idx)
{
	struct wpabuf *srf;
	int i, n;
	u8 mac[ETH_ALEN];
	u8 *bf;

	if (srf_bf_idx > 3)
		return NULL;

	if (os_strlen(srf_mac_list) % (ETH_ALEN * 2)) {
		wpa_printf(MSG_INFO,
			   "NAN: Invalid SRF MAC list length %zu",
			   os_strlen(srf_mac_list));
		return NULL;
	}

	n = os_strlen(srf_mac_list) / (ETH_ALEN * 2);

	srf = wpabuf_alloc(srf_bf_len);
	if (!srf)
		return NULL;

	bf = wpabuf_put(srf, srf_bf_len);

	for (i = 0; i < n; i++) {
		if (hexstr2bin(srf_mac_list + i * 2 * ETH_ALEN, mac, ETH_ALEN))
		{
			wpa_printf(MSG_INFO,
				   "NAN: Invalid SRF MAC address %s",
				   srf_mac_list + i * 2 * ETH_ALEN);
			goto out;
		}

		bloom_filter_add(bf, srf_bf_idx, srf_bf_len, mac);
	}

	return srf;
out:
	wpabuf_free(srf);
	return NULL;
}


int nan_de_subscribe(struct nan_de *de, const char *service_name,
		     enum nan_service_protocol_type srv_proto_type,
		     const struct wpabuf *ssi, const struct wpabuf *elems,
		     struct nan_subscribe_params *params, bool p2p,
		     const u8 *addr)
{
	int subscribe_id;
	struct nan_de_service *srv;

	if (!service_name && !params->proximity_ranging) {
		wpa_printf(MSG_DEBUG, "NAN: Subscribe() - no service_name");
		return -1;
	}

	if (params->proximity_ranging && params->active && !elems) {
		wpa_printf(MSG_INFO,
			   "NAN: Unable to fetch proximity ranging params");
		return -1;
	}

	if (params->sync && !de->cluster_id_set) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Subscribe() - can't publish sync, cluster id is not set");
		return -1;
	}

	if (p2p && params->sync) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Subscribe() - P2P is not supported with sync");
		return -1;
	}

	subscribe_id = nan_de_get_handle(de);
	if (subscribe_id < 1)
		return -1;

	srv = os_zalloc(sizeof(*srv));
	if (!srv)
		return -1;
	srv->type = NAN_DE_SUBSCRIBE;
	srv->freq = params->freq;

	if (service_name) {
		srv->service_name = os_strdup(service_name);
		if (!srv->service_name)
			goto fail;
	}

	if (params->proximity_ranging && !service_name)
		os_memset(srv->service_id, 0, NAN_SERVICE_ID_LEN);
	else if (nan_de_derive_service_id(srv) < 0)
		goto fail;

	os_memcpy(&srv->subscribe, params, sizeof(*params));

	if (params->freq_list) {
		size_t len;

		len = (int_array_len(params->freq_list) + 1) * sizeof(int);
		srv->freq_list = os_memdup(params->freq_list, len);
		if (!srv->freq_list)
			goto fail;
	}
	srv->subscribe.freq_list = NULL;

	srv->srv_proto_type = srv_proto_type;
	if (ssi) {
		srv->ssi = wpabuf_dup(ssi);
		if (!srv->ssi)
			goto fail;
	}
	if (elems) {
		srv->elems = wpabuf_dup(elems);
		if (!srv->elems)
			goto fail;
	}

	if (params->match_filter_rx) {
		srv->matching_filter_rx =
			wpabuf_parse_bin(params->match_filter_rx);
		if (!srv->matching_filter_rx ||
		    wpabuf_len(srv->matching_filter_rx) > 255) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to parse RX matching filter");
			goto fail;
		}
	}

	if (params->match_filter_tx) {
		srv->matching_filter_tx =
			wpabuf_parse_bin(params->match_filter_tx);
		if (!srv->matching_filter_tx ||
		    wpabuf_len(srv->matching_filter_tx) > 255) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to parse TX matching filter");
			goto fail;
		}
	}

	if (params->srf_mac_list) {
		if (params->srf_bf_len) {
			srv->srf = nan_build_bloom_filter(params->srf_mac_list,
							  params->srf_bf_len,
							  params->srf_bf_idx);
			srv->srf_type_bloom_filter = true;
			srv->srf_bf_idx = params->srf_bf_idx;
		} else {
			srv->srf = wpabuf_parse_bin(params->srf_mac_list);
			if (wpabuf_len(srv->srf) % ETH_ALEN) {
				wpa_printf(MSG_INFO,
					   "NAN: Invalid SRF MAC list length");
				goto fail;
			}
		}

		if (!srv->srf || wpabuf_len(srv->srf) > 254) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Failed to parse SRF MAC list");
			goto fail;
		}

		srv->srf_include = params->srf_include;
	}

	if (addr && params->forced_addr) {
		os_memcpy(srv->forced_addr, addr, ETH_ALEN);
		srv->forced_addr_set = true;
		wpa_printf(MSG_DEBUG, "NAN: Using source address " MACSTR
			   " for subscribe service", MAC2STR(srv->forced_addr));
	}

	dl_list_init(&srv->pmkid_list);

	wpa_printf(MSG_DEBUG, "NAN: Assigned new subscribe handle %d for %s",
		   subscribe_id, service_name ? service_name : "Ranging");
	srv->id = subscribe_id;
	srv->is_p2p = p2p;
	srv->is_pr = params->proximity_ranging && params->active;
	srv->sync = params->sync;
	srv->close_proximity = params->close_proximity;
	srv->pbm = params->pbm;
	srv->gtk_required = params->gtk_required;

	nan_de_add_srv(de, srv);
	nan_de_run_timer(de);
	return subscribe_id;
fail:
	nan_de_service_free(srv);
	return -1;
}


void nan_de_cancel_subscribe(struct nan_de *de, int subscribe_id)
{
	struct nan_de_service *srv;

	if (subscribe_id < 1 || subscribe_id > NAN_DE_MAX_SERVICE)
		return;
	srv = de->service[subscribe_id - 1];
	if (!srv || srv->type != NAN_DE_SUBSCRIBE)
		return;
	nan_de_del_srv(de, srv, NAN_DE_REASON_USER_REQUEST);
}


int nan_de_transmit(struct nan_de *de, int handle,
		    const struct wpabuf *ssi, const struct wpabuf *elems,
		    const u8 *peer_addr, u8 req_instance_id,
		    const struct wpabuf *nan_attrs, u32 *cookie)
{
	struct nan_de_service *srv;
	const u8 *a3;
	const u8 *network_id;

	if (handle < 1 || handle > NAN_DE_MAX_SERVICE)
		return -1;

	srv = de->service[handle - 1];
	if (!srv)
		return -1;

	if (srv->sync && !de->cluster_id_set) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Cannot transmit Follow-up, cluster ID not set");
		return -1;
	}

	if (srv->is_p2p)
		network_id = p2p_network_id;
	else if (srv->sync)
		network_id = de->cluster_id;
	else
		network_id = nan_network_id;

	if (srv->a3_set)
		a3 = srv->a3;
	else
		a3 = network_id;
	nan_de_tx_sdf(de, srv, 100, NAN_SRV_CTRL_FOLLOW_UP,
		      peer_addr, a3, req_instance_id, ssi, nan_attrs,
		      cookie);

	srv->listen_stopped = false;
	return 0;
}


int nan_de_stop_listen(struct nan_de *de, int handle)
{
	struct nan_de_service *srv;

	if (handle < 1 || handle > NAN_DE_MAX_SERVICE)
		return -1;

	srv = de->service[handle - 1];
	if (!srv)
		return -1;
	srv->listen_stopped = true;
	return 0;
}


int nan_de_config(struct nan_de *de, struct nan_de_cfg *cfg)
{
	if (!de || !cfg)
		return -1;

	 /* No change in configuration */
	if (de->cfg.n_min == cfg->n_min && de->cfg.n_max == cfg->n_max &&
	    de->cfg.cycle == cfg->cycle && de->cfg.suspend == cfg->suspend)
		return 0;

	wpa_printf(MSG_DEBUG,
		   "NAN: Configuring NAN DE: n=(%u, %u), suspend=%u, cycle=%u",
		   cfg->n_min, cfg->n_max, cfg->suspend, cfg->cycle);

	if (!cfg->n_min && !cfg->n_max) {
		cfg->n_min = NAN_DE_N_MIN;
		cfg->n_max = NAN_DE_N_MAX;
	} else if (cfg->n_min < 1 || cfg->n_max < cfg->n_min) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Invalid configuration parameters: N");
		return -1;
	}

	if (((!!cfg->suspend) ^ (!!cfg->cycle)) ||
	    (cfg->cycle && cfg->suspend >= cfg->cycle)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Invalid configuration parameters: cycle");
		return -1;
	}

	de->cfg = *cfg;

	os_memset(&de->suspend_cycle_start, 0, sizeof(de->suspend_cycle_start));

	if (!de->listen_freq)
		nan_de_run_timer(de);

	return 0;
}


void nan_de_dw_trigger(struct nan_de *de, int freq)
{
	int i;
	struct os_reltime now;

	de->dw_freq = freq;

	if (!de->cluster_id_set) {
		wpa_printf(MSG_DEBUG, "NAN: Skip DW, cluster ID not set");
		return;
	}

	os_get_reltime(&now);
	for (i = 0; i < NAN_DE_MAX_SERVICE; i++) {
		struct nan_de_service *srv = de->service[i];

		if (!srv || !srv->sync)
			continue;

		if (nan_de_srv_expired(srv, &now)) {
			nan_de_del_srv(de, srv, NAN_DE_REASON_TIMEOUT);
			continue;
		}

		if ((srv->type == NAN_DE_PUBLISH &&
		     srv->publish.unsolicited) ||
		    (srv->type == NAN_DE_SUBSCRIBE && srv->subscribe.active)) {
			nan_de_tx_multicast(de, srv, 0);
		}
	}
}


void nan_de_set_cluster_id(struct nan_de *de, const u8 *cluster_id)
{
	if (cluster_id) {
		os_memcpy(de->cluster_id, cluster_id, ETH_ALEN);
		de->cluster_id_set = true;
	} else {
		de->cluster_id_set = false;
	}
}


bool nan_de_is_valid_instance_id(struct nan_de *de, int handle,
				 bool publish, u8 *service_id)
{
	struct nan_de_service *srv;

	if (handle < 1 || handle > NAN_DE_MAX_SERVICE)
		return false;

	srv = de->service[handle - 1];
	if (!srv)
		return false;

	if (publish && srv->type != NAN_DE_PUBLISH)
		return false;
	if (!publish && srv->type != NAN_DE_SUBSCRIBE)
		return false;

	os_memcpy(service_id, srv->service_id, NAN_SERVICE_ID_LEN);
	return true;
}


u16 nan_de_get_service_bootstrap_methods(struct nan_de *de, int handle)
{
	struct nan_de_service *srv;

	if (handle < 1 || handle > NAN_DE_MAX_SERVICE)
		return 0;

	srv = de->service[handle - 1];
	if (!srv)
		return 0;

	return srv->pbm;
}


bool nan_de_service_supports_csid(struct nan_de *de, int handle, int csid)
{
	struct nan_de_service *srv;

	if (handle < 1 || handle > NAN_DE_MAX_SERVICE)
		return false;

	srv = de->service[handle - 1];
	if (!srv)
		return false;

	/* If cipher_suites_list is not set, all CSIDs are allowed */
	if (!srv->cipher_suites_list)
		return true;

	/* Open is allowed only if security is not required */
	if (csid == NAN_CS_NONE)
		return !srv->security_required;

	/* Check if the CSID is in the service's cipher suite list */
	return int_array_includes(srv->cipher_suites_list, csid);
}


static const char * nan_de_service_type2str(enum nan_de_service_type type)
{
	switch (type) {
	case NAN_DE_PUBLISH:
		return "publish";
	case NAN_DE_SUBSCRIBE:
		return "subscribe";
	}

	return "unknown";
}


int nan_de_get_status(struct nan_de *de, char *buf, size_t buflen)
{
	char *pos, *end;
	unsigned int i;
	int ret;

	if (!de)
		return -1;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "num_services=%u\n",
			  de->num_service);
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	for (i = 0; i < NAN_DE_MAX_SERVICE; i++) {
		struct nan_de_service *srv = de->service[i];

		if (!srv)
			continue;

		ret = os_snprintf(pos, end - pos,
				  "service=%u type=%s name=%s sync=%d\n",
				  srv->id,
				  nan_de_service_type2str(srv->type),
				  srv->service_name ? srv->service_name : "",
				  srv->sync);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}


#ifdef CONFIG_TESTING_OPTIONS

void nan_de_set_tx_mcast_follow_up_prot(struct nan_de *de, bool prot)
{
	wpa_printf(MSG_DEBUG,
		   "NAN: Set tx_mcast_follow_up_dual_prot: %u->%u",
		   de->tx_mcast_follow_up_prot, prot);

	de->tx_mcast_follow_up_prot = prot;
}

#endif /* CONFIG_TESTING_OPTIONS */
