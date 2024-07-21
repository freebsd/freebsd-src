/*
 * NAN unsynchronized service discovery (USD)
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/wpa_ctrl.h"
#include "common/nan_de.h"
#include "hostapd.h"
#include "ap_drv_ops.h"
#include "nan_usd_ap.h"


static int hostapd_nan_de_tx(void *ctx, unsigned int freq,
			     unsigned int wait_time,
			     const u8 *dst, const u8 *src, const u8 *bssid,
			     const struct wpabuf *buf)
{
	struct hostapd_data *hapd = ctx;

	wpa_printf(MSG_DEBUG, "NAN: TX NAN SDF A1=" MACSTR " A2=" MACSTR
		   " A3=" MACSTR " len=%zu",
		   MAC2STR(dst), MAC2STR(src), MAC2STR(bssid),
		   wpabuf_len(buf));

	/* TODO: Force use of OFDM */
	return hostapd_drv_send_action(hapd, hapd->iface->freq, 0, dst,
				       wpabuf_head(buf), wpabuf_len(buf));
}


static int hostapd_nan_de_listen(void *ctx, unsigned int freq,
			      unsigned int duration)
{
	return 0;
}


static void
hostapd_nan_de_discovery_result(void *ctx, int subscribe_id,
				enum nan_service_protocol_type srv_proto_type,
				const u8 *ssi, size_t ssi_len,
				int peer_publish_id, const u8 *peer_addr,
				bool fsd, bool fsd_gas)
{
	struct hostapd_data *hapd = ctx;
	char *ssi_hex;

	ssi_hex = os_zalloc(2 * ssi_len + 1);
	if (!ssi_hex)
		return;
	if (ssi)
		wpa_snprintf_hex(ssi_hex, 2 * ssi_len + 1, ssi, ssi_len);
	wpa_msg(hapd->msg_ctx, MSG_INFO, NAN_DISCOVERY_RESULT
		"subscribe_id=%d publish_id=%d address=" MACSTR
		" fsd=%d fsd_gas=%d srv_proto_type=%u ssi=%s",
		subscribe_id, peer_publish_id, MAC2STR(peer_addr),
		fsd, fsd_gas, srv_proto_type, ssi_hex);
	os_free(ssi_hex);
}


static void
hostapd_nan_de_replied(void *ctx, int publish_id, const u8 *peer_addr,
		       int peer_subscribe_id,
		       enum nan_service_protocol_type srv_proto_type,
		       const u8 *ssi, size_t ssi_len)
{
	struct hostapd_data *hapd = ctx;
	char *ssi_hex;

	ssi_hex = os_zalloc(2 * ssi_len + 1);
	if (!ssi_hex)
		return;
	if (ssi)
		wpa_snprintf_hex(ssi_hex, 2 * ssi_len + 1, ssi, ssi_len);
	wpa_msg(hapd->msg_ctx, MSG_INFO, NAN_REPLIED
		"publish_id=%d address=" MACSTR
		" subscribe_id=%d srv_proto_type=%u ssi=%s",
		publish_id, MAC2STR(peer_addr), peer_subscribe_id,
		srv_proto_type, ssi_hex);
	os_free(ssi_hex);
}


static const char * nan_reason_txt(enum nan_de_reason reason)
{
	switch (reason) {
	case NAN_DE_REASON_TIMEOUT:
		return "timeout";
	case NAN_DE_REASON_USER_REQUEST:
		return "user-request";
	case NAN_DE_REASON_FAILURE:
		return "failure";
	}

	return "unknown";
}


static void hostapd_nan_de_publish_terminated(void *ctx, int publish_id,
					      enum nan_de_reason reason)
{
	struct hostapd_data *hapd = ctx;

	wpa_msg(hapd->msg_ctx, MSG_INFO, NAN_PUBLISH_TERMINATED
		"publish_id=%d reason=%s",
		publish_id, nan_reason_txt(reason));
}


static void hostapd_nan_de_subscribe_terminated(void *ctx, int subscribe_id,
						enum nan_de_reason reason)
{
	struct hostapd_data *hapd = ctx;

	wpa_msg(hapd->msg_ctx, MSG_INFO, NAN_SUBSCRIBE_TERMINATED
		"subscribe_id=%d reason=%s",
		subscribe_id, nan_reason_txt(reason));
}


static void hostapd_nan_de_receive(void *ctx, int id, int peer_instance_id,
				   const u8 *ssi, size_t ssi_len,
				   const u8 *peer_addr)
{
	struct hostapd_data *hapd = ctx;
	char *ssi_hex;

	ssi_hex = os_zalloc(2 * ssi_len + 1);
	if (!ssi_hex)
		return;
	if (ssi)
		wpa_snprintf_hex(ssi_hex, 2 * ssi_len + 1, ssi, ssi_len);
	wpa_msg(hapd->msg_ctx, MSG_INFO, NAN_RECEIVE
		"id=%d peer_instance_id=%d address=" MACSTR " ssi=%s",
		id, peer_instance_id, MAC2STR(peer_addr), ssi_hex);
	os_free(ssi_hex);
}


int hostapd_nan_usd_init(struct hostapd_data *hapd)
{
	struct nan_callbacks cb;

	os_memset(&cb, 0, sizeof(cb));
	cb.ctx = hapd;
	cb.tx = hostapd_nan_de_tx;
	cb.listen = hostapd_nan_de_listen;
	cb.discovery_result = hostapd_nan_de_discovery_result;
	cb.replied = hostapd_nan_de_replied;
	cb.publish_terminated = hostapd_nan_de_publish_terminated;
	cb.subscribe_terminated = hostapd_nan_de_subscribe_terminated;
	cb.receive = hostapd_nan_de_receive;

	hapd->nan_de = nan_de_init(hapd->own_addr, true, &cb);
	if (!hapd->nan_de)
		return -1;
	return 0;
}


void hostapd_nan_usd_deinit(struct hostapd_data *hapd)
{
	nan_de_deinit(hapd->nan_de);
	hapd->nan_de = NULL;
}


void hostapd_nan_usd_rx_sdf(struct hostapd_data *hapd, const u8 *src,
			    unsigned int freq, const u8 *buf, size_t len)
{
	if (!hapd->nan_de)
		return;
	nan_de_rx_sdf(hapd->nan_de, src, freq, buf, len);
}


void hostapd_nan_usd_flush(struct hostapd_data *hapd)
{
	if (!hapd->nan_de)
		return;
	nan_de_flush(hapd->nan_de);
}


int hostapd_nan_usd_publish(struct hostapd_data *hapd, const char *service_name,
			    enum nan_service_protocol_type srv_proto_type,
			    const struct wpabuf *ssi,
			    struct nan_publish_params *params)
{
	int publish_id;
	struct wpabuf *elems = NULL;

	if (!hapd->nan_de)
		return -1;

	publish_id = nan_de_publish(hapd->nan_de, service_name, srv_proto_type,
				    ssi, elems, params);
	wpabuf_free(elems);
	return publish_id;
}


void hostapd_nan_usd_cancel_publish(struct hostapd_data *hapd, int publish_id)
{
	if (!hapd->nan_de)
		return;
	nan_de_cancel_publish(hapd->nan_de, publish_id);
}


int hostapd_nan_usd_update_publish(struct hostapd_data *hapd, int publish_id,
				   const struct wpabuf *ssi)
{
	int ret;

	if (!hapd->nan_de)
		return -1;
	ret = nan_de_update_publish(hapd->nan_de, publish_id, ssi);
	return ret;
}


int hostapd_nan_usd_subscribe(struct hostapd_data *hapd,
			      const char *service_name,
			      enum nan_service_protocol_type srv_proto_type,
			      const struct wpabuf *ssi,
			      struct nan_subscribe_params *params)
{
	int subscribe_id;
	struct wpabuf *elems = NULL;

	if (!hapd->nan_de)
		return -1;

	subscribe_id = nan_de_subscribe(hapd->nan_de, service_name,
					srv_proto_type, ssi, elems, params);
	wpabuf_free(elems);
	return subscribe_id;
}


void hostapd_nan_usd_cancel_subscribe(struct hostapd_data *hapd,
				      int subscribe_id)
{
	if (!hapd->nan_de)
		return;
	nan_de_cancel_subscribe(hapd->nan_de, subscribe_id);
}


int hostapd_nan_usd_transmit(struct hostapd_data *hapd, int handle,
			     const struct wpabuf *ssi,
			     const struct wpabuf *elems,
			     const u8 *peer_addr, u8 req_instance_id)
{
	if (!hapd->nan_de)
		return -1;
	return nan_de_transmit(hapd->nan_de, handle, ssi, elems, peer_addr,
			       req_instance_id);
}
