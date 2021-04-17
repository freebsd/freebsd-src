/*
 * wpa_supplicant - P2P fuzzer
 * Copyright (c) 2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "p2p/p2p.h"
#include "../fuzzer-common.h"


static void debug_print(void *ctx, int level, const char *msg)
{
	wpa_printf(level, "P2P: %s", msg);
}


static void find_stopped(void *ctx)
{
}


static int start_listen(void *ctx, unsigned int freq,
			unsigned int duration,
			const struct wpabuf *probe_resp_ie)
{
	return 0;
}


static void stop_listen(void *ctx)
{
}


static void dev_found(void *ctx, const u8 *addr,
		      const struct p2p_peer_info *info,
		      int new_device)
{
}


static void dev_lost(void *ctx, const u8 *dev_addr)
{
}


static int send_action(void *ctx, unsigned int freq, const u8 *dst,
		       const u8 *src, const u8 *bssid, const u8 *buf,
		       size_t len, unsigned int wait_time, int *scheduled)
{
	*scheduled = 0;
	return 0;
}


static void send_action_done(void *ctx)
{
}


static void go_neg_req_rx(void *ctx, const u8 *src, u16 dev_passwd_id,
			  u8 go_intent)
{
}


static struct p2p_data * init_p2p(void)
{
	struct p2p_config p2p;

	os_memset(&p2p, 0, sizeof(p2p));
	p2p.max_peers = 100;
	p2p.passphrase_len = 8;
	p2p.channels.reg_classes = 1;
	p2p.channels.reg_class[0].reg_class = 81;
	p2p.channels.reg_class[0].channel[0] = 1;
	p2p.channels.reg_class[0].channel[1] = 2;
	p2p.channels.reg_class[0].channels = 2;
	p2p.debug_print = debug_print;
	p2p.find_stopped = find_stopped;
	p2p.start_listen = start_listen;
	p2p.stop_listen = stop_listen;
	p2p.dev_found = dev_found;
	p2p.dev_lost = dev_lost;
	p2p.send_action = send_action;
	p2p.send_action_done = send_action_done;
	p2p.go_neg_req_rx = go_neg_req_rx;

	return p2p_init(&p2p);
}


struct arg_ctx {
	const u8 *data;
	size_t data_len;
	struct p2p_data *p2p;
	int count;
};


static void test_send(void *eloop_data, void *user_ctx)
{
	struct arg_ctx *ctx = eloop_data;
	struct os_reltime rx_time;

	wpa_hexdump(MSG_MSGDUMP, "fuzzer - IEs", ctx->data, ctx->data_len);

	os_memset(&rx_time, 0, sizeof(rx_time));
	p2p_scan_res_handler(ctx->p2p, (u8 *) "\x02\x00\x00\x00\x01\x00", 2412,
			     &rx_time, 0, ctx->data, ctx->data_len);
	p2p_scan_res_handled(ctx->p2p, 0);

	p2p_probe_req_rx(ctx->p2p, (u8 *) "\x02\x00\x00\x00\x01\x00",
			 (u8 *) "\x02\x00\x00\x00\x00\x00",
			 (u8 *) "\x02\x00\x00\x00\x00\x00",
			 ctx->data, ctx->data_len, 2412, 0);

	if (ctx->data_len >= IEEE80211_HDRLEN + 1) {
		struct os_reltime rx_time;
		const struct ieee80211_mgmt *mgmt;

		mgmt = (const struct ieee80211_mgmt *) ctx->data;
		os_memset(&rx_time, 0, sizeof(rx_time));
		p2p_rx_action(ctx->p2p, mgmt->da, mgmt->sa, mgmt->bssid,
			      mgmt->u.action.category,
			      (const u8 *) ctx->data + IEEE80211_HDRLEN + 1,
			      ctx->data_len - IEEE80211_HDRLEN - 1, 2412);
	}

	eloop_terminate();
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct p2p_data *p2p;
	struct arg_ctx ctx;

	wpa_fuzzer_set_debug_level();

	if (os_program_init())
		return -1;

	if (eloop_init()) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		return 0;
	}

	p2p = init_p2p();
	if (!p2p) {
		wpa_printf(MSG_ERROR, "P2P init failed");
		return 0;
	}

	os_memset(&ctx, 0, sizeof(ctx));
	ctx.p2p = p2p;
	ctx.data = data;
	ctx.data_len = size;

	eloop_register_timeout(0, 0, test_send, &ctx, NULL);

	wpa_printf(MSG_DEBUG, "Starting eloop");
	eloop_run();
	wpa_printf(MSG_DEBUG, "eloop done");

	p2p_deinit(p2p);
	eloop_destroy();
	os_program_deinit();

	return 0;
}
