/*
 * hostapd / Neighboring APs DB
 * Copyright(c) 2013 - 2016 Intel Mobile Communications GmbH.
 * Copyright(c) 2011 - 2016 Intel Corporation. All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef NEIGHBOR_DB_H
#define NEIGHBOR_DB_H

struct hostapd_neighbor_entry *
hostapd_neighbor_get(struct hostapd_data *hapd, const u8 *bssid,
		     const struct wpa_ssid_value *ssid);
int hostapd_neighbor_show(struct hostapd_data *hapd, char *buf, size_t buflen);
int hostapd_neighbor_set(struct hostapd_data *hapd, const u8 *bssid,
			 const struct wpa_ssid_value *ssid,
			 const struct wpabuf *nr, const struct wpabuf *lci,
			 const struct wpabuf *civic, int stationary,
			 u8 bss_parameters);
void hostapd_neighbor_set_own_report(struct hostapd_data *hapd);
int hostapd_neighbor_sync_own_report(struct hostapd_data *hapd);
int hostapd_neighbor_remove(struct hostapd_data *hapd, const u8 *bssid,
			    const struct wpa_ssid_value *ssid);
void hostapd_free_neighbor_db(struct hostapd_data *hapd);

#endif /* NEIGHBOR_DB_H */
