/*
 * NAN unsynchronized service discovery (USD)
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef NAN_USD_AP_H
#define NAN_USD_AP_H

struct nan_subscribe_params;
struct nan_publish_params;
enum nan_service_protocol_type;

int hostapd_nan_usd_init(struct hostapd_data *hapd);
void hostapd_nan_usd_deinit(struct hostapd_data *hapd);
void hostapd_nan_usd_rx_sdf(struct hostapd_data *hapd, const u8 *src,
			    unsigned int freq, const u8 *buf, size_t len);
void hostapd_nan_usd_flush(struct hostapd_data *hapd);
int hostapd_nan_usd_publish(struct hostapd_data *hapd, const char *service_name,
			    enum nan_service_protocol_type srv_proto_type,
			    const struct wpabuf *ssi,
			    struct nan_publish_params *params);
void hostapd_nan_usd_cancel_publish(struct hostapd_data *hapd, int publish_id);
int hostapd_nan_usd_update_publish(struct hostapd_data *hapd, int publish_id,
				   const struct wpabuf *ssi);
int hostapd_nan_usd_subscribe(struct hostapd_data *hapd,
			      const char *service_name,
			      enum nan_service_protocol_type srv_proto_type,
			      const struct wpabuf *ssi,
			      struct nan_subscribe_params *params);
void hostapd_nan_usd_cancel_subscribe(struct hostapd_data *hapd,
				      int subscribe_id);
int hostapd_nan_usd_transmit(struct hostapd_data *hapd, int handle,
			     const struct wpabuf *ssi,
			     const struct wpabuf *elems,
			     const u8 *peer_addr, u8 req_instance_id);
void hostapd_nan_usd_remain_on_channel_cb(struct hostapd_data *hapd,
					  unsigned int freq,
					  unsigned int duration);
void hostapd_nan_usd_cancel_remain_on_channel_cb(struct hostapd_data *hapd,
						 unsigned int freq);
void hostapd_nan_usd_tx_wait_expire(struct hostapd_data *hapd);

#endif /* NAN_USD_AP_H */
