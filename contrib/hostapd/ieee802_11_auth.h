#ifndef IEEE802_11_AUTH_H
#define IEEE802_11_AUTH_H

enum {
	HOSTAPD_ACL_REJECT = 0,
	HOSTAPD_ACL_ACCEPT = 1,
	HOSTAPD_ACL_PENDING = 2,
	HOSTAPD_ACL_ACCEPT_TIMEOUT = 3
};

int hostapd_allowed_address(hostapd *hapd, u8 *addr, u8 *msg, size_t len,
			    u32 *session_timeout, u32 *acct_interim_interval);
int hostapd_acl_init(hostapd *hapd);
void hostapd_acl_deinit(hostapd *hapd);

#endif /* IEEE802_11_AUTH_H */
