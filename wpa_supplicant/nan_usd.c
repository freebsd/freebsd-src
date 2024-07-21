/*
 * NAN unsynchronized service discovery (USD)
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/nan_de.h"
#include "wpa_supplicant_i.h"
#include "offchannel.h"
#include "driver_i.h"
#include "nan_usd.h"


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

	nan_de_tx_status(wpa_s->nan_de, freq, dst);
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

	if (radio_add_work(wpa_s, freq, "nan-usd-tx", 0,
			   wpas_nan_usd_start_tx_cb, twork) < 0) {
		wpas_nan_usd_tx_work_free(twork);
		return -1;
	}

	return 0;
}


struct wpas_nan_usd_listen_work {
	unsigned int freq;
	unsigned int duration;
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
	if (wpa_drv_remain_on_channel(wpa_s, lwork->freq, duration) < 0) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Failed to request the driver to remain on channel (%u MHz) for listen",
			   lwork->freq);
		wpas_nan_usd_listen_work_done(wpa_s);
		return;
	}
}


static int wpas_nan_de_listen(void *ctx, unsigned int freq,
			      unsigned int duration)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpas_nan_usd_listen_work *lwork;

	lwork = os_zalloc(sizeof(*lwork));
	if (!lwork)
		return -1;
	lwork->freq = freq;
	lwork->duration = duration;

	if (radio_add_work(wpa_s, freq, "nan-usd-listen", 0,
			   wpas_nan_usd_start_listen_cb, lwork) < 0) {
		os_free(lwork);
		return -1;
	}

	return 0;
}


static void
wpas_nan_de_discovery_result(void *ctx, int subscribe_id,
			     enum nan_service_protocol_type srv_proto_type,
			     const u8 *ssi, size_t ssi_len, int peer_publish_id,
			     const u8 *peer_addr, bool fsd, bool fsd_gas)
{
	struct wpa_supplicant *wpa_s = ctx;
	char *ssi_hex;

	ssi_hex = os_zalloc(2 * ssi_len + 1);
	if (!ssi_hex)
		return;
	if (ssi)
		wpa_snprintf_hex(ssi_hex, 2 * ssi_len + 1, ssi, ssi_len);
	wpa_msg(wpa_s, MSG_INFO, NAN_DISCOVERY_RESULT
		"subscribe_id=%d publish_id=%d address=" MACSTR
		" fsd=%d fsd_gas=%d srv_proto_type=%u ssi=%s",
		subscribe_id, peer_publish_id, MAC2STR(peer_addr),
		fsd, fsd_gas, srv_proto_type, ssi_hex);
	os_free(ssi_hex);
}


static void wpas_nan_de_replied(void *ctx, int publish_id, const u8 *peer_addr,
				int peer_subscribe_id,
				enum nan_service_protocol_type srv_proto_type,
				const u8 *ssi, size_t ssi_len)
{
	struct wpa_supplicant *wpa_s = ctx;
	char *ssi_hex;

	ssi_hex = os_zalloc(2 * ssi_len + 1);
	if (!ssi_hex)
		return;
	if (ssi)
		wpa_snprintf_hex(ssi_hex, 2 * ssi_len + 1, ssi, ssi_len);
	wpa_msg(wpa_s, MSG_INFO, NAN_REPLIED
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


static void wpas_nan_de_publish_terminated(void *ctx, int publish_id,
					   enum nan_de_reason reason)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_msg(wpa_s, MSG_INFO, NAN_PUBLISH_TERMINATED
		"publish_id=%d reason=%s",
		publish_id, nan_reason_txt(reason));
}


static void wpas_nan_de_subscribe_terminated(void *ctx, int subscribe_id,
					     enum nan_de_reason reason)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_msg(wpa_s, MSG_INFO, NAN_SUBSCRIBE_TERMINATED
		"subscribe_id=%d reason=%s",
		subscribe_id, nan_reason_txt(reason));
}


static void wpas_nan_de_receive(void *ctx, int id, int peer_instance_id,
				const u8 *ssi, size_t ssi_len,
				const u8 *peer_addr)
{
	struct wpa_supplicant *wpa_s = ctx;
	char *ssi_hex;

	ssi_hex = os_zalloc(2 * ssi_len + 1);
	if (!ssi_hex)
		return;
	if (ssi)
		wpa_snprintf_hex(ssi_hex, 2 * ssi_len + 1, ssi, ssi_len);
	wpa_msg(wpa_s, MSG_INFO, NAN_RECEIVE
		"id=%d peer_instance_id=%d address=" MACSTR " ssi=%s",
		id, peer_instance_id, MAC2STR(peer_addr), ssi_hex);
	os_free(ssi_hex);
}


int wpas_nan_usd_init(struct wpa_supplicant *wpa_s)
{
	struct nan_callbacks cb;

	os_memset(&cb, 0, sizeof(cb));
	cb.ctx = wpa_s;
	cb.tx = wpas_nan_de_tx;
	cb.listen = wpas_nan_de_listen;
	cb.discovery_result = wpas_nan_de_discovery_result;
	cb.replied = wpas_nan_de_replied;
	cb.publish_terminated = wpas_nan_de_publish_terminated;
	cb.subscribe_terminated = wpas_nan_de_subscribe_terminated;
	cb.receive = wpas_nan_de_receive;

	wpa_s->nan_de = nan_de_init(wpa_s->own_addr, false, &cb);
	if (!wpa_s->nan_de)
		return -1;
	return 0;
}


void wpas_nan_usd_deinit(struct wpa_supplicant *wpa_s)
{
	nan_de_deinit(wpa_s->nan_de);
	wpa_s->nan_de = NULL;
}


void wpas_nan_usd_rx_sdf(struct wpa_supplicant *wpa_s, const u8 *src,
			 unsigned int freq, const u8 *buf, size_t len)
{
	if (!wpa_s->nan_de)
		return;
	nan_de_rx_sdf(wpa_s->nan_de, src, freq, buf, len);
}


void wpas_nan_usd_flush(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->nan_de)
		return;
	nan_de_flush(wpa_s->nan_de);
}


int wpas_nan_usd_publish(struct wpa_supplicant *wpa_s, const char *service_name,
			 enum nan_service_protocol_type srv_proto_type,
			 const struct wpabuf *ssi,
			 struct nan_publish_params *params)
{
	int publish_id;
	struct wpabuf *elems = NULL;

	if (!wpa_s->nan_de)
		return -1;

	publish_id = nan_de_publish(wpa_s->nan_de, service_name, srv_proto_type,
				    ssi, elems, params);
	wpabuf_free(elems);
	return publish_id;
}


void wpas_nan_usd_cancel_publish(struct wpa_supplicant *wpa_s, int publish_id)
{
	if (!wpa_s->nan_de)
		return;
	nan_de_cancel_publish(wpa_s->nan_de, publish_id);
}


int wpas_nan_usd_update_publish(struct wpa_supplicant *wpa_s, int publish_id,
				const struct wpabuf *ssi)
{
	if (!wpa_s->nan_de)
		return -1;
	return nan_de_update_publish(wpa_s->nan_de, publish_id, ssi);
}


int wpas_nan_usd_subscribe(struct wpa_supplicant *wpa_s,
			   const char *service_name,
			   enum nan_service_protocol_type srv_proto_type,
			   const struct wpabuf *ssi,
			   struct nan_subscribe_params *params)
{
	int subscribe_id;
	struct wpabuf *elems = NULL;

	if (!wpa_s->nan_de)
		return -1;

	subscribe_id = nan_de_subscribe(wpa_s->nan_de, service_name,
					srv_proto_type, ssi, elems, params);
	wpabuf_free(elems);
	return subscribe_id;
}


void wpas_nan_usd_cancel_subscribe(struct wpa_supplicant *wpa_s,
				   int subscribe_id)
{
	if (!wpa_s->nan_de)
		return;
	nan_de_cancel_subscribe(wpa_s->nan_de, subscribe_id);
}


int wpas_nan_usd_transmit(struct wpa_supplicant *wpa_s, int handle,
			  const struct wpabuf *ssi, const struct wpabuf *elems,
			  const u8 *peer_addr, u8 req_instance_id)
{
	if (!wpa_s->nan_de)
		return -1;
	return nan_de_transmit(wpa_s->nan_de, handle, ssi, elems, peer_addr,
			       req_instance_id);
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
