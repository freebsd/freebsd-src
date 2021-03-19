/*
 * WPA Supplicant - Scanning
 * Copyright (c) 2003-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef SCAN_H
#define SCAN_H

/*
 * Noise floor values to use when we have signal strength
 * measurements, but no noise floor measurements. These values were
 * measured in an office environment with many APs.
 */
#define DEFAULT_NOISE_FLOOR_2GHZ (-89)
#define DEFAULT_NOISE_FLOOR_5GHZ (-92)

/*
 * Channels with a great SNR can operate at full rate. What is a great SNR?
 * This doc https://supportforums.cisco.com/docs/DOC-12954 says, "the general
 * rule of thumb is that any SNR above 20 is good." This one
 * http://www.cisco.com/en/US/tech/tk722/tk809/technologies_q_and_a_item09186a00805e9a96.shtml#qa23
 * recommends 25 as a minimum SNR for 54 Mbps data rate. The estimates used in
 * scan_est_throughput() allow even smaller SNR values for the maximum rates
 * (21 for 54 Mbps, 22 for VHT80 MCS9, 24 for HT40 and HT20 MCS7). Use 25 as a
 * somewhat conservative value here.
 */
#define GREAT_SNR 25

#define IS_5GHZ(n) (n > 4000)

int wpa_supplicant_enabled_networks(struct wpa_supplicant *wpa_s);
void wpa_supplicant_req_scan(struct wpa_supplicant *wpa_s, int sec, int usec);
int wpa_supplicant_delayed_sched_scan(struct wpa_supplicant *wpa_s,
				      int sec, int usec);
int wpa_supplicant_req_sched_scan(struct wpa_supplicant *wpa_s);
void wpa_supplicant_cancel_scan(struct wpa_supplicant *wpa_s);
void wpa_supplicant_cancel_delayed_sched_scan(struct wpa_supplicant *wpa_s);
void wpa_supplicant_cancel_sched_scan(struct wpa_supplicant *wpa_s);
void wpa_supplicant_notify_scanning(struct wpa_supplicant *wpa_s,
				    int scanning);
struct wpa_driver_scan_params;
int wpa_supplicant_trigger_scan(struct wpa_supplicant *wpa_s,
				struct wpa_driver_scan_params *params);
struct wpa_scan_results *
wpa_supplicant_get_scan_results(struct wpa_supplicant *wpa_s,
				struct scan_info *info, int new_scan);
int wpa_supplicant_update_scan_results(struct wpa_supplicant *wpa_s);
const u8 * wpa_scan_get_ie(const struct wpa_scan_res *res, u8 ie);
const u8 * wpa_scan_get_vendor_ie(const struct wpa_scan_res *res,
				  u32 vendor_type);
const u8 * wpa_scan_get_vendor_ie_beacon(const struct wpa_scan_res *res,
					 u32 vendor_type);
struct wpabuf * wpa_scan_get_vendor_ie_multi(const struct wpa_scan_res *res,
					     u32 vendor_type);
int wpa_supplicant_filter_bssid_match(struct wpa_supplicant *wpa_s,
				      const u8 *bssid);
void wpa_supplicant_update_scan_int(struct wpa_supplicant *wpa_s, int sec);
void scan_only_handler(struct wpa_supplicant *wpa_s,
		       struct wpa_scan_results *scan_res);
int wpas_scan_scheduled(struct wpa_supplicant *wpa_s);
struct wpa_driver_scan_params *
wpa_scan_clone_params(const struct wpa_driver_scan_params *src);
void wpa_scan_free_params(struct wpa_driver_scan_params *params);
int wpas_start_pno(struct wpa_supplicant *wpa_s);
int wpas_stop_pno(struct wpa_supplicant *wpa_s);
void wpas_scan_reset_sched_scan(struct wpa_supplicant *wpa_s);
void wpas_scan_restart_sched_scan(struct wpa_supplicant *wpa_s);

void wpas_mac_addr_rand_scan_clear(struct wpa_supplicant *wpa_s,
				   unsigned int type);
int wpas_mac_addr_rand_scan_set(struct wpa_supplicant *wpa_s,
				unsigned int type, const u8 *addr,
				const u8 *mask);
int wpas_mac_addr_rand_scan_get_mask(struct wpa_supplicant *wpa_s,
				     unsigned int type, u8 *mask);
int wpas_abort_ongoing_scan(struct wpa_supplicant *wpa_s);
void filter_scan_res(struct wpa_supplicant *wpa_s,
		     struct wpa_scan_results *res);
void scan_snr(struct wpa_scan_res *res);
void scan_est_throughput(struct wpa_supplicant *wpa_s,
			 struct wpa_scan_res *res);
unsigned int wpas_get_est_tpt(const struct wpa_supplicant *wpa_s,
			      const u8 *ies, size_t ies_len, int rate,
			      int snr);
void wpa_supplicant_set_default_scan_ies(struct wpa_supplicant *wpa_s);
int wpa_add_scan_freqs_list(struct wpa_supplicant *wpa_s,
			    enum hostapd_hw_mode band,
			    struct wpa_driver_scan_params *params,
			    bool is_6ghz);

#endif /* SCAN_H */
