/*
 * wpa_supplicant - IBSS RSN
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IBSS_RSN_H
#define IBSS_RSN_H

struct ibss_rsn;

struct ibss_rsn_peer {
	struct ibss_rsn_peer *next;
	struct ibss_rsn *ibss_rsn;

	u8 addr[ETH_ALEN];

	struct wpa_sm *supp;
	enum wpa_states supp_state;
	u8 supp_ie[80];
	size_t supp_ie_len;

	struct wpa_state_machine *auth;
};

struct ibss_rsn {
	struct wpa_supplicant *wpa_s;
	struct wpa_authenticator *auth_group;
	struct ibss_rsn_peer *peers;
	u8 psk[PMK_LEN];
};


struct ibss_rsn * ibss_rsn_init(struct wpa_supplicant *wpa_s);
void ibss_rsn_deinit(struct ibss_rsn *ibss_rsn);
int ibss_rsn_start(struct ibss_rsn *ibss_rsn, const u8 *addr);
void ibss_rsn_stop(struct ibss_rsn *ibss_rsn, const u8 *peermac);
int ibss_rsn_rx_eapol(struct ibss_rsn *ibss_rsn, const u8 *src_addr,
		      const u8 *buf, size_t len);
void ibss_rsn_set_psk(struct ibss_rsn *ibss_rsn, const u8 *psk);

#endif /* IBSS_RSN_H */
