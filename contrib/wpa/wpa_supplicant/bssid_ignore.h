/*
 * wpa_supplicant - List of temporarily ignored BSSIDs
 * Copyright (c) 2003-2021, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef BSSID_IGNORE_H
#define BSSID_IGNORE_H

struct wpa_bssid_ignore {
	struct wpa_bssid_ignore *next;
	u8 bssid[ETH_ALEN];
	int count;
	/* Time of the most recent trigger to ignore this BSSID. */
	struct os_reltime start;
	/*
	 * Number of seconds after start that the entey will be considered
	 * valid.
	 */
	int timeout_secs;
};

struct wpa_bssid_ignore * wpa_bssid_ignore_get(struct wpa_supplicant *wpa_s,
					 const u8 *bssid);
int wpa_bssid_ignore_add(struct wpa_supplicant *wpa_s, const u8 *bssid);
int wpa_bssid_ignore_del(struct wpa_supplicant *wpa_s, const u8 *bssid);
int wpa_bssid_ignore_is_listed(struct wpa_supplicant *wpa_s, const u8 *bssid);
void wpa_bssid_ignore_clear(struct wpa_supplicant *wpa_s);
void wpa_bssid_ignore_update(struct wpa_supplicant *wpa_s);

#endif /* BSSID_IGNORE_H */
