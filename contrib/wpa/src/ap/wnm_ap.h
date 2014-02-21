/*
 * IEEE 802.11v WNM related functions and structures
 * Copyright (c) 2011-2012, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WNM_AP_H
#define WNM_AP_H

struct rx_action;

int ieee802_11_rx_wnm_action_ap(struct hostapd_data *hapd,
				struct rx_action *action);

#endif /* WNM_AP_H */
