/*
 * Common driver-related functions
 * Copyright (c) 2003-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "utils/common.h"
#include "driver.h"

void wpa_scan_results_free(struct wpa_scan_results *res)
{
	size_t i;

	if (res == NULL)
		return;

	for (i = 0; i < res->num; i++)
		os_free(res->res[i]);
	os_free(res->res);
	os_free(res);
}


const char * event_to_string(enum wpa_event_type event)
{
#define E2S(n) case EVENT_ ## n: return #n
	switch (event) {
	E2S(ASSOC);
	E2S(DISASSOC);
	E2S(MICHAEL_MIC_FAILURE);
	E2S(SCAN_RESULTS);
	E2S(ASSOCINFO);
	E2S(INTERFACE_STATUS);
	E2S(PMKID_CANDIDATE);
	E2S(STKSTART);
	E2S(TDLS);
	E2S(FT_RESPONSE);
	E2S(IBSS_RSN_START);
	E2S(AUTH);
	E2S(DEAUTH);
	E2S(ASSOC_REJECT);
	E2S(AUTH_TIMED_OUT);
	E2S(ASSOC_TIMED_OUT);
	E2S(FT_RRB_RX);
	E2S(WPS_BUTTON_PUSHED);
	E2S(TX_STATUS);
	E2S(RX_FROM_UNKNOWN);
	E2S(RX_MGMT);
	E2S(RX_ACTION);
	E2S(REMAIN_ON_CHANNEL);
	E2S(CANCEL_REMAIN_ON_CHANNEL);
	E2S(MLME_RX);
	E2S(RX_PROBE_REQ);
	E2S(NEW_STA);
	E2S(EAPOL_RX);
	E2S(SIGNAL_CHANGE);
	E2S(INTERFACE_ENABLED);
	E2S(INTERFACE_DISABLED);
	E2S(CHANNEL_LIST_CHANGED);
	E2S(INTERFACE_UNAVAILABLE);
	E2S(BEST_CHANNEL);
	E2S(UNPROT_DEAUTH);
	E2S(UNPROT_DISASSOC);
	E2S(STATION_LOW_ACK);
	E2S(P2P_DEV_FOUND);
	E2S(P2P_GO_NEG_REQ_RX);
	E2S(P2P_GO_NEG_COMPLETED);
	E2S(P2P_PROV_DISC_REQUEST);
	E2S(P2P_PROV_DISC_RESPONSE);
	E2S(P2P_SD_REQUEST);
	E2S(P2P_SD_RESPONSE);
	E2S(IBSS_PEER_LOST);
	E2S(DRIVER_GTK_REKEY);
	E2S(SCHED_SCAN_STOPPED);
	E2S(DRIVER_CLIENT_POLL_OK);
	E2S(EAPOL_TX_STATUS);
	E2S(CH_SWITCH);
	E2S(WNM);
	}

	return "UNKNOWN";
#undef E2S
}
