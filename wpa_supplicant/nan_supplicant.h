/*
 * wpa_supplicant - NAN
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef NAN_SUPPLICANT_H
#define NAN_SUPPLICANT_H

/* NAN synchronization only */
#ifdef CONFIG_NAN

int wpas_nan_init(struct wpa_supplicant *wpa_s);
void wpas_nan_deinit(struct wpa_supplicant *wpa_s);
int wpas_nan_start(struct wpa_supplicant *wpa_s);
int wpas_nan_set(struct wpa_supplicant *wpa_s, char *cmd);
int wpas_nan_update_conf(struct wpa_supplicant *wpa_s);
int wpas_nan_stop(struct wpa_supplicant *wpa_s);
void wpas_nan_flush(struct wpa_supplicant *wpa_s);
void wpas_nan_cluster_join(struct wpa_supplicant *wpa_s,
			   const u8 *cluster_id,
			   bool new_cluster);
void wpas_nan_next_dw(struct wpa_supplicant *wpa_s, u32 freq);
void wpas_nan_sched_update_done(struct wpa_supplicant *wpa_s,
				const union wpa_event_data *data);
void wpas_nan_ulw_update(struct wpa_supplicant *wpa_s,
			 const u8 *ulw, size_t ulw_len);
void wpas_nan_chan_evacuation(struct wpa_supplicant *wpa_s,
			      const struct nan_chan_evacuation_info *info);
int wpas_nan_sched_config_map(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_nan_ndp_request(struct wpa_supplicant *wpa_s, char *cmd);
void wpas_nan_rx_naf(struct wpa_supplicant *wpa_s,
		     const struct ieee80211_mgmt *mgmt, size_t len);
int wpas_nan_ndp_response(struct wpa_supplicant *wpa_s, char *cmd);
int wpas_nan_ndp_terminate(struct wpa_supplicant *wpa_s, char *cmd);
int wpas_nan_peer_info(struct wpa_supplicant *wpa_s, const char *cmd,
		       char *reply, size_t reply_size);
int wpas_nan_status(struct wpa_supplicant *wpa_s, char *reply,
		    size_t reply_size);
int wpas_nan_bootstrap_request(struct wpa_supplicant *wpa_s, char *cmd);
int wpas_nan_bootstrap_reset(struct wpa_supplicant *wpa_s, char *cmd);
bool wpas_nan_is_peer_paired(struct wpa_supplicant *wpa_s, const u8 *peer_addr);
void wpas_nan_data_interface_removed(struct wpa_supplicant *wpa_s);

int wpas_nan_pair(struct wpa_supplicant *wpa_s, const u8 *peer_addr,
		  u8 auth_mode, int cipher, int handle, u8 peer_instance_id,
		  bool responder, const char *password);
int wpas_nan_pairing_start(struct wpa_supplicant *wpa_s, char *cmd);
int wpas_nan_pairing_abort(struct wpa_supplicant *wpa_s, const char *cmd);
int wpas_nan_pasn_auth_tx_status(struct wpa_supplicant *wpa_s, const u8 *data,
				 size_t data_len, bool acked);
int wpas_nan_pasn_auth_rx(struct wpa_supplicant *wpa_s,
			  const struct ieee80211_mgmt *mgmt, size_t len);

#else /* CONFIG_NAN */

static inline int wpas_nan_init(struct wpa_supplicant *wpa_s)
{
	return -1;
}

static inline void wpas_nan_deinit(struct wpa_supplicant *wpa_s)
{}

static inline int wpas_nan_start(struct wpa_supplicant *wpa_s)
{
	return -1;
}

static inline int wpas_nan_set(struct wpa_supplicant *wpa_s, char *cmd)
{
	return -1;
}

static inline int wpas_nan_update_conf(struct wpa_supplicant *wpa_s)
{
	return -1;
}

static inline int wpas_nan_stop(struct wpa_supplicant *wpa_s)
{
	return -1;
}

static inline void wpas_nan_flush(struct wpa_supplicant *wpa_s)
{}

static inline void wpas_nan_cluster_join(struct wpa_supplicant *wpa_s,
					 const u8 *cluster_id,
					 bool new_cluster)
{}

static inline void wpas_nan_next_dw(struct wpa_supplicant *wpa_s, u32 freq)
{}

static inline void wpas_nan_sched_update_done(struct wpa_supplicant *wpa_s,
					      const union wpa_event_data *data)
{}

static inline void wpas_nan_ulw_update(struct wpa_supplicant *wpa_s,
				       const u8 *ulw, size_t ulw_len)
{}

static inline void wpas_nan_rx_naf(struct wpa_supplicant *wpa_s,
				   const struct ieee80211_mgmt *mgmt,
				   size_t len)
{}

static inline bool wpas_nan_is_peer_paired(struct wpa_supplicant *wpa_s,
					  const u8 *peer_addr)
{
	return false;
}

static inline void
wpas_nan_chan_evacuation(struct wpa_supplicant *wpa_s,
			 union wpa_event_data *data)
{}

#endif /* CONFIG_NAN */

struct nan_subscribe_params;
struct nan_publish_params;
enum nan_service_protocol_type;

/* NAN sync and USD common */
#if defined(CONFIG_NAN_USD) || defined(CONFIG_NAN)

int wpas_nan_de_init(struct wpa_supplicant *wpa_s);
void wpas_nan_de_deinit(struct wpa_supplicant *wpa_s);
void wpas_nan_de_rx_sdf(struct wpa_supplicant *wpa_s, const u8 *src,
			const u8 *a3, unsigned int freq,
			const u8 *buf, size_t len, int rssi);
void wpas_nan_de_flush(struct wpa_supplicant *wpa_s);
int wpas_nan_publish(struct wpa_supplicant *wpa_s, const char *service_name,
		     enum nan_service_protocol_type srv_proto_type,
		     const struct wpabuf *ssi,
		     struct nan_publish_params *params, bool p2p);
void wpas_nan_cancel_publish(struct wpa_supplicant *wpa_s, int publish_id);
int wpas_nan_update_publish(struct wpa_supplicant *wpa_s, int publish_id,
			    const struct wpabuf *ssi);
int wpas_nan_subscribe(struct wpa_supplicant *wpa_s,
		       const char *service_name,
		       enum nan_service_protocol_type srv_proto_type,
		       const struct wpabuf *ssi,
		       struct nan_subscribe_params *params, bool p2p);
void wpas_nan_cancel_subscribe(struct wpa_supplicant *wpa_s,
			       int subscribe_id);
int wpas_nan_transmit(struct wpa_supplicant *wpa_s, int handle,
		      const struct wpabuf *ssi, const struct wpabuf *elems,
		      const u8 *peer_addr, u8 req_instance_id, u32 *cookie);
int wpas_nan_tx_status(struct wpa_supplicant *wpa_s,
		       const u8 *data, size_t data_len, int acked);

#else /* CONFIG_NAN_USD || CONFIG_NAN */

static inline int wpas_nan_de_init(struct wpa_supplicant *wpa_s)
{
	return 0;
}

static inline void wpas_nan_de_deinit(struct wpa_supplicant *wpa_s)
{}

static inline
void wpas_nan_de_rx_sdf(struct wpa_supplicant *wpa_s, const u8 *src,
			const u8 *a3, unsigned int freq,
			const u8 *buf, size_t len, int rssi)
{}

static inline void wpas_nan_de_flush(struct wpa_supplicant *wpa_s)
{}

static inline int wpas_nan_tx_status(struct wpa_supplicant *wpa_s,
				     const u8 *data, size_t data_len,
				     u8 acked)
{
	return -1;
}

#endif /* CONFIG_NAN_USD || CONFIG_NAN */

/* NAN USD only */
#ifdef CONFIG_NAN_USD

void wpas_nan_usd_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				       unsigned int freq,
				       unsigned int duration);
void wpas_nan_usd_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					      unsigned int freq);
void wpas_nan_usd_tx_wait_expire(struct wpa_supplicant *wpa_s);
int * wpas_nan_usd_all_freqs(struct wpa_supplicant *wpa_s);
int wpas_nan_usd_unpause_publish(struct wpa_supplicant *wpa_s, int publish_id,
				 u8 peer_instance_id, const u8 *peer_addr);
int wpas_nan_usd_publish_stop_listen(struct wpa_supplicant *wpa_s,
				     int publish_id);
int wpas_nan_usd_subscribe_stop_listen(struct wpa_supplicant *wpa_s,
				       int subscribe_id);
void wpas_nan_usd_state_change_notif(struct wpa_supplicant *wpa_s);

#else /* CONFIG_NAN_USD */

static inline
void wpas_nan_usd_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				       unsigned int freq,
				       unsigned int duration)
{}

static inline
void wpas_nan_usd_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					      unsigned int freq)
{}

static inline void wpas_nan_usd_tx_wait_expire(struct wpa_supplicant *wpa_s)
{}

static inline
int * wpas_nan_usd_all_freqs(struct wpa_supplicant *wpa_s)
{
	return NULL;
}

static inline
int wpas_nan_usd_unpause_publish(struct wpa_supplicant *wpa_s, int publish_id,
				 u8 peer_instance_id, const u8 *peer_addr)
{
	return -1;
}

static inline
int wpas_nan_usd_publish_stop_listen(struct wpa_supplicant *wpa_s,
				     int publish_id)
{
	return -1;
}

static inline
int wpas_nan_usd_subscribe_stop_listen(struct wpa_supplicant *wpa_s,
				       int subscribe_id)
{
	return -1;
}

static inline void wpas_nan_usd_state_change_notif(struct wpa_supplicant *wpa_s)
{}

#endif /* CONFIG_NAN_USD */

#endif /* NAN_SUPPLICANT_H */
