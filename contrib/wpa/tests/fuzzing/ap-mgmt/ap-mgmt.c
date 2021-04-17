/*
 * hostapd - Management frame fuzzer
 * Copyright (c) 2015-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "ap/hostapd.h"
#include "ap/hw_features.h"
#include "ap/ieee802_11.h"
#include "ap/sta_info.h"
#include "ap/ap_list.h"
#include "../fuzzer-common.h"


const struct wpa_driver_ops *const wpa_drivers[] =
{
	NULL
};


struct arg_ctx {
	const u8 *data;
	size_t data_len;
	struct hostapd_iface iface;
	struct hostapd_data hapd;
	struct wpa_driver_ops driver;
	struct hostapd_config iconf;
	struct hostapd_bss_config conf;
};


static void test_send_mgmt(void *eloop_data, void *user_ctx)
{
	struct arg_ctx *ctx = eloop_data;
	struct hostapd_frame_info fi;
	const u8 *pos, *end;

	os_memset(&fi, 0, sizeof(fi));

	pos = ctx->data;
	end = pos + ctx->data_len;

	while (end - pos > 2) {
		u16 flen;

		flen = WPA_GET_BE16(pos);
		pos += 2;
		if (end - pos < flen)
			break;
		wpa_hexdump(MSG_MSGDUMP, "fuzzer - frame", pos, flen);
		ieee802_11_mgmt(&ctx->hapd, pos, flen, &fi);
		pos += flen;
	}

	eloop_terminate();
}


static struct hostapd_hw_modes * gen_modes(void)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;

	mode = os_zalloc(sizeof(struct hostapd_hw_modes));
	if (!mode)
		return NULL;

	mode->mode = HOSTAPD_MODE_IEEE80211G;
	chan = os_zalloc(sizeof(struct hostapd_channel_data));
	if (!chan) {
		os_free(mode);
		return NULL;
	}
	chan->chan = 1;
	chan->freq = 2412;
	mode->channels = chan;
	mode->num_channels = 1;

	mode->rates = os_zalloc(sizeof(int));
	if (!mode->rates) {
		os_free(chan);
		os_free(mode);
		return NULL;
	}
	mode->rates[0] = 10;
	mode->num_rates = 1;

	return mode;
}


static int init_hapd(struct arg_ctx *ctx)
{
	struct hostapd_data *hapd = &ctx->hapd;
	struct sta_info *sta;
	struct hostapd_bss_config *bss;

	hapd->driver = &ctx->driver;
	os_memcpy(hapd->own_addr, "\x02\x00\x00\x00\x03\x00", ETH_ALEN);
	hapd->iface = &ctx->iface;
	hapd->iface->conf = hostapd_config_defaults();
	if (!hapd->iface->conf)
		return -1;
	hapd->iface->hw_features = gen_modes();
	hapd->iface->num_hw_features = 1;
	hapd->iface->current_mode = hapd->iface->hw_features;
	hapd->iconf = hapd->iface->conf;
	hapd->iconf->hw_mode = HOSTAPD_MODE_IEEE80211G;
	hapd->iconf->channel = 1;
	bss = hapd->conf = hapd->iconf->bss[0];
	hostapd_config_defaults_bss(hapd->conf);
	os_memcpy(bss->ssid.ssid, "test", 4);
	bss->ssid.ssid_len = 4;
	bss->ssid.ssid_set = 1;

	sta = ap_sta_add(hapd, (u8 *) "\x02\x00\x00\x00\x00\x00");
	if (sta)
		sta->flags |= WLAN_STA_ASSOC | WLAN_STA_WMM;

	return 0;
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct arg_ctx ctx;

	wpa_fuzzer_set_debug_level();

	if (os_program_init())
		return 0;

	if (eloop_init()) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		return 0;
	}

	os_memset(&ctx, 0, sizeof(ctx));
	ctx.data = data;
	ctx.data_len = size;

	if (init_hapd(&ctx))
		goto fail;

	eloop_register_timeout(0, 0, test_send_mgmt, &ctx, NULL);

	wpa_printf(MSG_DEBUG, "Starting eloop");
	eloop_run();
	wpa_printf(MSG_DEBUG, "eloop done");
	hostapd_free_stas(&ctx.hapd);
	hostapd_free_hw_features(ctx.hapd.iface->hw_features,
				 ctx.hapd.iface->num_hw_features);

fail:
	hostapd_config_free(ctx.hapd.iconf);
	ap_list_deinit(&ctx.iface);
	eloop_destroy();
	os_program_deinit();

	return 0;
}
