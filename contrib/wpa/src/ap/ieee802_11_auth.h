/*
 * hostapd / IEEE 802.11 authentication (ACL)
 * Copyright (c) 2003-2022, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IEEE802_11_AUTH_H
#define IEEE802_11_AUTH_H

enum {
	HOSTAPD_ACL_REJECT = 0,
	HOSTAPD_ACL_ACCEPT = 1,
	HOSTAPD_ACL_PENDING = 2,
	HOSTAPD_ACL_ACCEPT_TIMEOUT = 3
};

struct radius_sta {
	u32 session_timeout;
	u32 acct_interim_interval;
	struct vlan_description vlan_id;
	struct hostapd_sta_wpa_psk_short *psk;
	char *identity;
	char *radius_cui;
};

int hostapd_check_acl(struct hostapd_data *hapd, const u8 *addr,
		      struct vlan_description *vlan_id);
int hostapd_allowed_address(struct hostapd_data *hapd, const u8 *addr,
			    const u8 *msg, size_t len, struct radius_sta *out,
			    int is_probe_req);
int hostapd_acl_init(struct hostapd_data *hapd);
void hostapd_acl_deinit(struct hostapd_data *hapd);
void hostapd_free_psk_list(struct hostapd_sta_wpa_psk_short *psk);
void hostapd_acl_expire(struct hostapd_data *hapd);
void hostapd_copy_psk_list(struct hostapd_sta_wpa_psk_short **psk,
			   struct hostapd_sta_wpa_psk_short *src);
void hostapd_acl_req_radius_psk(struct hostapd_data *hapd, const u8 *addr,
				int key_mgmt, const u8 *anonce,
				const u8 *eapol, size_t eapol_len);

#endif /* IEEE802_11_AUTH_H */
