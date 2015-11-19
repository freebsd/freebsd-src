/*
 * Driver interaction with Linux nl80211/cfg80211 - Scanning
 * Copyright (c) 2002-2014, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <netlink/genl/genl.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "driver_nl80211.h"


static int get_noise_for_scan_results(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_SURVEY_INFO_MAX + 1];
	static struct nla_policy survey_policy[NL80211_SURVEY_INFO_MAX + 1] = {
		[NL80211_SURVEY_INFO_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_SURVEY_INFO_NOISE] = { .type = NLA_U8 },
	};
	struct wpa_scan_results *scan_results = arg;
	struct wpa_scan_res *scan_res;
	size_t i;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_SURVEY_INFO]) {
		wpa_printf(MSG_DEBUG, "nl80211: Survey data missing");
		return NL_SKIP;
	}

	if (nla_parse_nested(sinfo, NL80211_SURVEY_INFO_MAX,
			     tb[NL80211_ATTR_SURVEY_INFO],
			     survey_policy)) {
		wpa_printf(MSG_DEBUG, "nl80211: Failed to parse nested "
			   "attributes");
		return NL_SKIP;
	}

	if (!sinfo[NL80211_SURVEY_INFO_NOISE])
		return NL_SKIP;

	if (!sinfo[NL80211_SURVEY_INFO_FREQUENCY])
		return NL_SKIP;

	for (i = 0; i < scan_results->num; ++i) {
		scan_res = scan_results->res[i];
		if (!scan_res)
			continue;
		if ((int) nla_get_u32(sinfo[NL80211_SURVEY_INFO_FREQUENCY]) !=
		    scan_res->freq)
			continue;
		if (!(scan_res->flags & WPA_SCAN_NOISE_INVALID))
			continue;
		scan_res->noise = (s8)
			nla_get_u8(sinfo[NL80211_SURVEY_INFO_NOISE]);
		scan_res->flags &= ~WPA_SCAN_NOISE_INVALID;
	}

	return NL_SKIP;
}


static int nl80211_get_noise_for_scan_results(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_scan_results *scan_res)
{
	struct nl_msg *msg;

	msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);
	return send_and_recv_msgs(drv, msg, get_noise_for_scan_results,
				  scan_res);
}


/**
 * wpa_driver_nl80211_scan_timeout - Scan timeout to report scan completion
 * @eloop_ctx: Driver private data
 * @timeout_ctx: ctx argument given to wpa_driver_nl80211_init()
 *
 * This function can be used as registered timeout when starting a scan to
 * generate a scan completed event if the driver does not report this.
 */
void wpa_driver_nl80211_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_driver_nl80211_data *drv = eloop_ctx;
	if (drv->ap_scan_as_station != NL80211_IFTYPE_UNSPECIFIED) {
		wpa_driver_nl80211_set_mode(drv->first_bss,
					    drv->ap_scan_as_station);
		drv->ap_scan_as_station = NL80211_IFTYPE_UNSPECIFIED;
	}
	wpa_printf(MSG_DEBUG, "Scan timeout - try to get results");
	wpa_supplicant_event(timeout_ctx, EVENT_SCAN_RESULTS, NULL);
}


static struct nl_msg *
nl80211_scan_common(struct i802_bss *bss, u8 cmd,
		    struct wpa_driver_scan_params *params)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	size_t i;
	u32 scan_flags = 0;

	msg = nl80211_cmd_msg(bss, 0, cmd);
	if (!msg)
		return NULL;

	if (params->num_ssids) {
		struct nlattr *ssids;

		ssids = nla_nest_start(msg, NL80211_ATTR_SCAN_SSIDS);
		if (ssids == NULL)
			goto fail;
		for (i = 0; i < params->num_ssids; i++) {
			wpa_hexdump_ascii(MSG_MSGDUMP, "nl80211: Scan SSID",
					  params->ssids[i].ssid,
					  params->ssids[i].ssid_len);
			if (nla_put(msg, i + 1, params->ssids[i].ssid_len,
				    params->ssids[i].ssid))
				goto fail;
		}
		nla_nest_end(msg, ssids);
	}

	if (params->extra_ies) {
		wpa_hexdump(MSG_MSGDUMP, "nl80211: Scan extra IEs",
			    params->extra_ies, params->extra_ies_len);
		if (nla_put(msg, NL80211_ATTR_IE, params->extra_ies_len,
			    params->extra_ies))
			goto fail;
	}

	if (params->freqs) {
		struct nlattr *freqs;
		freqs = nla_nest_start(msg, NL80211_ATTR_SCAN_FREQUENCIES);
		if (freqs == NULL)
			goto fail;
		for (i = 0; params->freqs[i]; i++) {
			wpa_printf(MSG_MSGDUMP, "nl80211: Scan frequency %u "
				   "MHz", params->freqs[i]);
			if (nla_put_u32(msg, i + 1, params->freqs[i]))
				goto fail;
		}
		nla_nest_end(msg, freqs);
	}

	os_free(drv->filter_ssids);
	drv->filter_ssids = params->filter_ssids;
	params->filter_ssids = NULL;
	drv->num_filter_ssids = params->num_filter_ssids;

	if (params->only_new_results) {
		wpa_printf(MSG_DEBUG, "nl80211: Add NL80211_SCAN_FLAG_FLUSH");
		scan_flags |= NL80211_SCAN_FLAG_FLUSH;
	}

	if (params->low_priority && drv->have_low_prio_scan) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Add NL80211_SCAN_FLAG_LOW_PRIORITY");
		scan_flags |= NL80211_SCAN_FLAG_LOW_PRIORITY;
	}

	if (params->mac_addr_rand) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Add NL80211_SCAN_FLAG_RANDOM_ADDR");
		scan_flags |= NL80211_SCAN_FLAG_RANDOM_ADDR;

		if (params->mac_addr) {
			wpa_printf(MSG_DEBUG, "nl80211: MAC address: " MACSTR,
				   MAC2STR(params->mac_addr));
			if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN,
				    params->mac_addr))
				goto fail;
		}

		if (params->mac_addr_mask) {
			wpa_printf(MSG_DEBUG, "nl80211: MAC address mask: "
				   MACSTR, MAC2STR(params->mac_addr_mask));
			if (nla_put(msg, NL80211_ATTR_MAC_MASK, ETH_ALEN,
				    params->mac_addr_mask))
				goto fail;
		}
	}

	if (scan_flags &&
	    nla_put_u32(msg, NL80211_ATTR_SCAN_FLAGS, scan_flags))
		goto fail;

	return msg;

fail:
	nlmsg_free(msg);
	return NULL;
}


/**
 * wpa_driver_nl80211_scan - Request the driver to initiate scan
 * @bss: Pointer to private driver data from wpa_driver_nl80211_init()
 * @params: Scan parameters
 * Returns: 0 on success, -1 on failure
 */
int wpa_driver_nl80211_scan(struct i802_bss *bss,
			    struct wpa_driver_scan_params *params)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1, timeout;
	struct nl_msg *msg = NULL;

	wpa_dbg(drv->ctx, MSG_DEBUG, "nl80211: scan request");
	drv->scan_for_auth = 0;

	if (TEST_FAIL())
		return -1;

	msg = nl80211_scan_common(bss, NL80211_CMD_TRIGGER_SCAN, params);
	if (!msg)
		return -1;

	if (params->p2p_probe) {
		struct nlattr *rates;

		wpa_printf(MSG_DEBUG, "nl80211: P2P probe - mask SuppRates");

		rates = nla_nest_start(msg, NL80211_ATTR_SCAN_SUPP_RATES);
		if (rates == NULL)
			goto fail;

		/*
		 * Remove 2.4 GHz rates 1, 2, 5.5, 11 Mbps from supported rates
		 * by masking out everything else apart from the OFDM rates 6,
		 * 9, 12, 18, 24, 36, 48, 54 Mbps from non-MCS rates. All 5 GHz
		 * rates are left enabled.
		 */
		if (nla_put(msg, NL80211_BAND_2GHZ, 8,
			    "\x0c\x12\x18\x24\x30\x48\x60\x6c"))
			goto fail;
		nla_nest_end(msg, rates);

		if (nla_put_flag(msg, NL80211_ATTR_TX_NO_CCK_RATE))
			goto fail;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Scan trigger failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
		if (drv->hostapd && is_ap_interface(drv->nlmode)) {
			enum nl80211_iftype old_mode = drv->nlmode;

			/*
			 * mac80211 does not allow scan requests in AP mode, so
			 * try to do this in station mode.
			 */
			if (wpa_driver_nl80211_set_mode(
				    bss, NL80211_IFTYPE_STATION))
				goto fail;

			if (wpa_driver_nl80211_scan(bss, params)) {
				wpa_driver_nl80211_set_mode(bss, old_mode);
				goto fail;
			}

			/* Restore AP mode when processing scan results */
			drv->ap_scan_as_station = old_mode;
			ret = 0;
		} else
			goto fail;
	}

	drv->scan_state = SCAN_REQUESTED;
	/* Not all drivers generate "scan completed" wireless event, so try to
	 * read results after a timeout. */
	timeout = 10;
	if (drv->scan_complete_events) {
		/*
		 * The driver seems to deliver events to notify when scan is
		 * complete, so use longer timeout to avoid race conditions
		 * with scanning and following association request.
		 */
		timeout = 30;
	}
	wpa_printf(MSG_DEBUG, "Scan requested (ret=%d) - scan timeout %d "
		   "seconds", ret, timeout);
	eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv, drv->ctx);
	eloop_register_timeout(timeout, 0, wpa_driver_nl80211_scan_timeout,
			       drv, drv->ctx);

fail:
	nlmsg_free(msg);
	return ret;
}


/**
 * wpa_driver_nl80211_sched_scan - Initiate a scheduled scan
 * @priv: Pointer to private driver data from wpa_driver_nl80211_init()
 * @params: Scan parameters
 * @interval: Interval between scan cycles in milliseconds
 * Returns: 0 on success, -1 on failure or if not supported
 */
int wpa_driver_nl80211_sched_scan(void *priv,
				  struct wpa_driver_scan_params *params,
				  u32 interval)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1;
	struct nl_msg *msg;
	size_t i;

	wpa_dbg(drv->ctx, MSG_DEBUG, "nl80211: sched_scan request");

#ifdef ANDROID
	if (!drv->capa.sched_scan_supported)
		return android_pno_start(bss, params);
#endif /* ANDROID */

	msg = nl80211_scan_common(bss, NL80211_CMD_START_SCHED_SCAN, params);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_SCHED_SCAN_INTERVAL, interval))
		goto fail;

	if ((drv->num_filter_ssids &&
	    (int) drv->num_filter_ssids <= drv->capa.max_match_sets) ||
	    params->filter_rssi) {
		struct nlattr *match_sets;
		match_sets = nla_nest_start(msg, NL80211_ATTR_SCHED_SCAN_MATCH);
		if (match_sets == NULL)
			goto fail;

		for (i = 0; i < drv->num_filter_ssids; i++) {
			struct nlattr *match_set_ssid;
			wpa_hexdump_ascii(MSG_MSGDUMP,
					  "nl80211: Sched scan filter SSID",
					  drv->filter_ssids[i].ssid,
					  drv->filter_ssids[i].ssid_len);

			match_set_ssid = nla_nest_start(msg, i + 1);
			if (match_set_ssid == NULL ||
			    nla_put(msg, NL80211_ATTR_SCHED_SCAN_MATCH_SSID,
				    drv->filter_ssids[i].ssid_len,
				    drv->filter_ssids[i].ssid) ||
			    (params->filter_rssi &&
			     nla_put_u32(msg,
					 NL80211_SCHED_SCAN_MATCH_ATTR_RSSI,
					 params->filter_rssi)))
				goto fail;

			nla_nest_end(msg, match_set_ssid);
		}

		/*
		 * Due to backward compatibility code, newer kernels treat this
		 * matchset (with only an RSSI filter) as the default for all
		 * other matchsets, unless it's the only one, in which case the
		 * matchset will actually allow all SSIDs above the RSSI.
		 */
		if (params->filter_rssi) {
			struct nlattr *match_set_rssi;
			match_set_rssi = nla_nest_start(msg, 0);
			if (match_set_rssi == NULL ||
			    nla_put_u32(msg, NL80211_SCHED_SCAN_MATCH_ATTR_RSSI,
					params->filter_rssi))
				goto fail;
			wpa_printf(MSG_MSGDUMP,
				   "nl80211: Sched scan RSSI filter %d dBm",
				   params->filter_rssi);
			nla_nest_end(msg, match_set_rssi);
		}

		nla_nest_end(msg, match_sets);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);

	/* TODO: if we get an error here, we should fall back to normal scan */

	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Sched scan start failed: "
			   "ret=%d (%s)", ret, strerror(-ret));
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Sched scan requested (ret=%d) - "
		   "scan interval %d msec", ret, interval);

fail:
	nlmsg_free(msg);
	return ret;
}


/**
 * wpa_driver_nl80211_stop_sched_scan - Stop a scheduled scan
 * @priv: Pointer to private driver data from wpa_driver_nl80211_init()
 * Returns: 0 on success, -1 on failure or if not supported
 */
int wpa_driver_nl80211_stop_sched_scan(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret;
	struct nl_msg *msg;

#ifdef ANDROID
	if (!drv->capa.sched_scan_supported)
		return android_pno_stop(bss);
#endif /* ANDROID */

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_STOP_SCHED_SCAN);
	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Sched scan stop failed: ret=%d (%s)",
			   ret, strerror(-ret));
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Sched scan stop sent");
	}

	return ret;
}


const u8 * nl80211_get_ie(const u8 *ies, size_t ies_len, u8 ie)
{
	const u8 *end, *pos;

	if (ies == NULL)
		return NULL;

	pos = ies;
	end = ies + ies_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == ie)
			return pos;
		pos += 2 + pos[1];
	}

	return NULL;
}


static int nl80211_scan_filtered(struct wpa_driver_nl80211_data *drv,
				 const u8 *ie, size_t ie_len)
{
	const u8 *ssid;
	size_t i;

	if (drv->filter_ssids == NULL)
		return 0;

	ssid = nl80211_get_ie(ie, ie_len, WLAN_EID_SSID);
	if (ssid == NULL)
		return 1;

	for (i = 0; i < drv->num_filter_ssids; i++) {
		if (ssid[1] == drv->filter_ssids[i].ssid_len &&
		    os_memcmp(ssid + 2, drv->filter_ssids[i].ssid, ssid[1]) ==
		    0)
			return 0;
	}

	return 1;
}


int bss_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_BSSID] = { .type = NLA_UNSPEC },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_TSF] = { .type = NLA_U64 },
		[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { .type = NLA_UNSPEC },
		[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
		[NL80211_BSS_STATUS] = { .type = NLA_U32 },
		[NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
		[NL80211_BSS_BEACON_IES] = { .type = NLA_UNSPEC },
	};
	struct nl80211_bss_info_arg *_arg = arg;
	struct wpa_scan_results *res = _arg->res;
	struct wpa_scan_res **tmp;
	struct wpa_scan_res *r;
	const u8 *ie, *beacon_ie;
	size_t ie_len, beacon_ie_len;
	u8 *pos;
	size_t i;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_BSS])
		return NL_SKIP;
	if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS],
			     bss_policy))
		return NL_SKIP;
	if (bss[NL80211_BSS_STATUS]) {
		enum nl80211_bss_status status;
		status = nla_get_u32(bss[NL80211_BSS_STATUS]);
		if (status == NL80211_BSS_STATUS_ASSOCIATED &&
		    bss[NL80211_BSS_FREQUENCY]) {
			_arg->assoc_freq =
				nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
			wpa_printf(MSG_DEBUG, "nl80211: Associated on %u MHz",
				   _arg->assoc_freq);
		}
		if (status == NL80211_BSS_STATUS_IBSS_JOINED &&
		    bss[NL80211_BSS_FREQUENCY]) {
			_arg->ibss_freq =
				nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
			wpa_printf(MSG_DEBUG, "nl80211: IBSS-joined on %u MHz",
				   _arg->ibss_freq);
		}
		if (status == NL80211_BSS_STATUS_ASSOCIATED &&
		    bss[NL80211_BSS_BSSID]) {
			os_memcpy(_arg->assoc_bssid,
				  nla_data(bss[NL80211_BSS_BSSID]), ETH_ALEN);
			wpa_printf(MSG_DEBUG, "nl80211: Associated with "
				   MACSTR, MAC2STR(_arg->assoc_bssid));
		}
	}
	if (!res)
		return NL_SKIP;
	if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
		ie = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
		ie_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
	} else {
		ie = NULL;
		ie_len = 0;
	}
	if (bss[NL80211_BSS_BEACON_IES]) {
		beacon_ie = nla_data(bss[NL80211_BSS_BEACON_IES]);
		beacon_ie_len = nla_len(bss[NL80211_BSS_BEACON_IES]);
	} else {
		beacon_ie = NULL;
		beacon_ie_len = 0;
	}

	if (nl80211_scan_filtered(_arg->drv, ie ? ie : beacon_ie,
				  ie ? ie_len : beacon_ie_len))
		return NL_SKIP;

	r = os_zalloc(sizeof(*r) + ie_len + beacon_ie_len);
	if (r == NULL)
		return NL_SKIP;
	if (bss[NL80211_BSS_BSSID])
		os_memcpy(r->bssid, nla_data(bss[NL80211_BSS_BSSID]),
			  ETH_ALEN);
	if (bss[NL80211_BSS_FREQUENCY])
		r->freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
	if (bss[NL80211_BSS_BEACON_INTERVAL])
		r->beacon_int = nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]);
	if (bss[NL80211_BSS_CAPABILITY])
		r->caps = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
	r->flags |= WPA_SCAN_NOISE_INVALID;
	if (bss[NL80211_BSS_SIGNAL_MBM]) {
		r->level = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
		r->level /= 100; /* mBm to dBm */
		r->flags |= WPA_SCAN_LEVEL_DBM | WPA_SCAN_QUAL_INVALID;
	} else if (bss[NL80211_BSS_SIGNAL_UNSPEC]) {
		r->level = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
		r->flags |= WPA_SCAN_QUAL_INVALID;
	} else
		r->flags |= WPA_SCAN_LEVEL_INVALID | WPA_SCAN_QUAL_INVALID;
	if (bss[NL80211_BSS_TSF])
		r->tsf = nla_get_u64(bss[NL80211_BSS_TSF]);
	if (bss[NL80211_BSS_BEACON_TSF]) {
		u64 tsf = nla_get_u64(bss[NL80211_BSS_BEACON_TSF]);
		if (tsf > r->tsf)
			r->tsf = tsf;
	}
	if (bss[NL80211_BSS_SEEN_MS_AGO])
		r->age = nla_get_u32(bss[NL80211_BSS_SEEN_MS_AGO]);
	r->ie_len = ie_len;
	pos = (u8 *) (r + 1);
	if (ie) {
		os_memcpy(pos, ie, ie_len);
		pos += ie_len;
	}
	r->beacon_ie_len = beacon_ie_len;
	if (beacon_ie)
		os_memcpy(pos, beacon_ie, beacon_ie_len);

	if (bss[NL80211_BSS_STATUS]) {
		enum nl80211_bss_status status;
		status = nla_get_u32(bss[NL80211_BSS_STATUS]);
		switch (status) {
		case NL80211_BSS_STATUS_ASSOCIATED:
			r->flags |= WPA_SCAN_ASSOCIATED;
			break;
		default:
			break;
		}
	}

	/*
	 * cfg80211 maintains separate BSS table entries for APs if the same
	 * BSSID,SSID pair is seen on multiple channels. wpa_supplicant does
	 * not use frequency as a separate key in the BSS table, so filter out
	 * duplicated entries. Prefer associated BSS entry in such a case in
	 * order to get the correct frequency into the BSS table. Similarly,
	 * prefer newer entries over older.
	 */
	for (i = 0; i < res->num; i++) {
		const u8 *s1, *s2;
		if (os_memcmp(res->res[i]->bssid, r->bssid, ETH_ALEN) != 0)
			continue;

		s1 = nl80211_get_ie((u8 *) (res->res[i] + 1),
				    res->res[i]->ie_len, WLAN_EID_SSID);
		s2 = nl80211_get_ie((u8 *) (r + 1), r->ie_len, WLAN_EID_SSID);
		if (s1 == NULL || s2 == NULL || s1[1] != s2[1] ||
		    os_memcmp(s1, s2, 2 + s1[1]) != 0)
			continue;

		/* Same BSSID,SSID was already included in scan results */
		wpa_printf(MSG_DEBUG, "nl80211: Remove duplicated scan result "
			   "for " MACSTR, MAC2STR(r->bssid));

		if (((r->flags & WPA_SCAN_ASSOCIATED) &&
		     !(res->res[i]->flags & WPA_SCAN_ASSOCIATED)) ||
		    r->age < res->res[i]->age) {
			os_free(res->res[i]);
			res->res[i] = r;
		} else
			os_free(r);
		return NL_SKIP;
	}

	tmp = os_realloc_array(res->res, res->num + 1,
			       sizeof(struct wpa_scan_res *));
	if (tmp == NULL) {
		os_free(r);
		return NL_SKIP;
	}
	tmp[res->num++] = r;
	res->res = tmp;

	return NL_SKIP;
}


static void clear_state_mismatch(struct wpa_driver_nl80211_data *drv,
				 const u8 *addr)
{
	if (drv->capa.flags & WPA_DRIVER_FLAGS_SME) {
		wpa_printf(MSG_DEBUG, "nl80211: Clear possible state "
			   "mismatch (" MACSTR ")", MAC2STR(addr));
		wpa_driver_nl80211_mlme(drv, addr,
					NL80211_CMD_DEAUTHENTICATE,
					WLAN_REASON_PREV_AUTH_NOT_VALID, 1);
	}
}


static void wpa_driver_nl80211_check_bss_status(
	struct wpa_driver_nl80211_data *drv, struct wpa_scan_results *res)
{
	size_t i;

	for (i = 0; i < res->num; i++) {
		struct wpa_scan_res *r = res->res[i];

		if (r->flags & WPA_SCAN_ASSOCIATED) {
			wpa_printf(MSG_DEBUG, "nl80211: Scan results "
				   "indicate BSS status with " MACSTR
				   " as associated",
				   MAC2STR(r->bssid));
			if (is_sta_interface(drv->nlmode) &&
			    !drv->associated) {
				wpa_printf(MSG_DEBUG, "nl80211: Local state "
					   "(not associated) does not match "
					   "with BSS state");
				clear_state_mismatch(drv, r->bssid);
			} else if (is_sta_interface(drv->nlmode) &&
				   os_memcmp(drv->bssid, r->bssid, ETH_ALEN) !=
				   0) {
				wpa_printf(MSG_DEBUG, "nl80211: Local state "
					   "(associated with " MACSTR ") does "
					   "not match with BSS state",
					   MAC2STR(drv->bssid));
				clear_state_mismatch(drv, r->bssid);
				clear_state_mismatch(drv, drv->bssid);
			}
		}
	}
}


static struct wpa_scan_results *
nl80211_get_scan_results(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	struct wpa_scan_results *res;
	int ret;
	struct nl80211_bss_info_arg arg;

	res = os_zalloc(sizeof(*res));
	if (res == NULL)
		return NULL;
	if (!(msg = nl80211_cmd_msg(drv->first_bss, NLM_F_DUMP,
				    NL80211_CMD_GET_SCAN))) {
		wpa_scan_results_free(res);
		return NULL;
	}

	arg.drv = drv;
	arg.res = res;
	ret = send_and_recv_msgs(drv, msg, bss_info_handler, &arg);
	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Received scan results (%lu "
			   "BSSes)", (unsigned long) res->num);
		nl80211_get_noise_for_scan_results(drv, res);
		return res;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Scan result fetch failed: ret=%d "
		   "(%s)", ret, strerror(-ret));
	wpa_scan_results_free(res);
	return NULL;
}


/**
 * wpa_driver_nl80211_get_scan_results - Fetch the latest scan results
 * @priv: Pointer to private wext data from wpa_driver_nl80211_init()
 * Returns: Scan results on success, -1 on failure
 */
struct wpa_scan_results * wpa_driver_nl80211_get_scan_results(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct wpa_scan_results *res;

	res = nl80211_get_scan_results(drv);
	if (res)
		wpa_driver_nl80211_check_bss_status(drv, res);
	return res;
}


void nl80211_dump_scan(struct wpa_driver_nl80211_data *drv)
{
	struct wpa_scan_results *res;
	size_t i;

	res = nl80211_get_scan_results(drv);
	if (res == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: Failed to get scan results");
		return;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Scan result dump");
	for (i = 0; i < res->num; i++) {
		struct wpa_scan_res *r = res->res[i];
		wpa_printf(MSG_DEBUG, "nl80211: %d/%d " MACSTR "%s",
			   (int) i, (int) res->num, MAC2STR(r->bssid),
			   r->flags & WPA_SCAN_ASSOCIATED ? " [assoc]" : "");
	}

	wpa_scan_results_free(res);
}
