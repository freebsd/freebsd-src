/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2006, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2007-2008, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef IEEE802_11_H
#define IEEE802_11_H

#include "ieee802_11_defs.h"
#include "ieee802_11_common.h"

struct hostapd_frame_info {
	u32 phytype;
	u32 channel;
	u32 datarate;
	u32 ssi_signal;

	unsigned int passive_scan:1;
};

struct hostapd_iface;
struct hostapd_data;
struct sta_info;

void ieee802_11_send_deauth(struct hostapd_data *hapd, u8 *addr, u16 reason);
void ieee802_11_mgmt(struct hostapd_data *hapd, u8 *buf, size_t len,
		     u16 stype, struct hostapd_frame_info *fi);
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, u8 *buf, size_t len,
			u16 stype, int ok);
void ieee802_11_print_ssid(char *buf, const u8 *ssid, u8 len);
void ieee80211_michael_mic_failure(struct hostapd_data *hapd, const u8 *addr,
				   int local);
int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen);
int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen);
u16 hostapd_own_capab_info(struct hostapd_data *hapd, struct sta_info *sta,
			   int probe);
u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ht_capabilities_info(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ht_operation(struct hostapd_data *hapd, u8 *eid);
int hostapd_ht_operation_update(struct hostapd_iface *iface);
void ieee802_11_send_sa_query_req(struct hostapd_data *hapd,
				  const u8 *addr, const u8 *trans_id);

#endif /* IEEE802_11_H */
