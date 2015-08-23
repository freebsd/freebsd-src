/*
 * hostapd / IEEE 802.11 Management: Beacon and Probe Request/Response
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef BEACON_H
#define BEACON_H

struct ieee80211_mgmt;

void handle_probe_req(struct hostapd_data *hapd,
		      const struct ieee80211_mgmt *mgmt, size_t len,
		      int ssi_signal);
int ieee802_11_set_beacon(struct hostapd_data *hapd);
int ieee802_11_set_beacons(struct hostapd_iface *iface);
int ieee802_11_update_beacons(struct hostapd_iface *iface);
int ieee802_11_build_ap_params(struct hostapd_data *hapd,
			       struct wpa_driver_ap_params *params);
void ieee802_11_free_ap_params(struct wpa_driver_ap_params *params);

#endif /* BEACON_H */
