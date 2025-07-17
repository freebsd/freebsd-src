/*
 * NAN unsynchronized service discovery (USD)
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef NAN_USD_H
#define NAN_USD_H

struct nan_subscribe_params;
struct nan_publish_params;
enum nan_service_protocol_type;

int wpas_nan_usd_init(struct wpa_supplicant *wpa_s);
void wpas_nan_usd_deinit(struct wpa_supplicant *wpa_s);
void wpas_nan_usd_rx_sdf(struct wpa_supplicant *wpa_s, const u8 *src,
			 unsigned int freq, const u8 *buf, size_t len);
void wpas_nan_usd_flush(struct wpa_supplicant *wpa_s);
int wpas_nan_usd_publish(struct wpa_supplicant *wpa_s, const char *service_name,
			 enum nan_service_protocol_type srv_proto_type,
			 const struct wpabuf *ssi,
			 struct nan_publish_params *params);
void wpas_nan_usd_cancel_publish(struct wpa_supplicant *wpa_s, int publish_id);
int wpas_nan_usd_update_publish(struct wpa_supplicant *wpa_s, int publish_id,
				const struct wpabuf *ssi);
int wpas_nan_usd_subscribe(struct wpa_supplicant *wpa_s,
			   const char *service_name,
			   enum nan_service_protocol_type srv_proto_type,
			   const struct wpabuf *ssi,
			   struct nan_subscribe_params *params);
void wpas_nan_usd_cancel_subscribe(struct wpa_supplicant *wpa_s,
				   int subscribe_id);
int wpas_nan_usd_transmit(struct wpa_supplicant *wpa_s, int handle,
			  const struct wpabuf *ssi, const struct wpabuf *elems,
			  const u8 *peer_addr, u8 req_instance_id);
void wpas_nan_usd_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
				       unsigned int freq,
				       unsigned int duration);
void wpas_nan_usd_cancel_remain_on_channel_cb(struct wpa_supplicant *wpa_s,
					      unsigned int freq);
void wpas_nan_usd_tx_wait_expire(struct wpa_supplicant *wpa_s);
int * wpas_nan_usd_all_freqs(struct wpa_supplicant *wpa_s);

#endif /* NAN_USD_H */
