#ifndef CONFIG_H
#define CONFIG_H

#include "config_types.h"

typedef u8 macaddr[ETH_ALEN];

struct hostapd_radius_servers;

#define PMK_LEN 32
struct hostapd_wpa_psk {
	struct hostapd_wpa_psk *next;
	int group;
	u8 psk[PMK_LEN];
	u8 addr[ETH_ALEN];
};

#define EAP_USER_MAX_METHODS 8
struct hostapd_eap_user {
	struct hostapd_eap_user *next;
	u8 *identity;
	size_t identity_len;
	u8 methods[EAP_USER_MAX_METHODS];
	u8 *password;
	size_t password_len;
	int phase2;
	int force_version;
};

struct hostapd_config {
	char iface[IFNAMSIZ + 1];
	char bridge[IFNAMSIZ + 1];

	const struct driver_ops *driver;

	enum {
		HOSTAPD_LEVEL_DEBUG_VERBOSE = 0,
		HOSTAPD_LEVEL_DEBUG = 1,
		HOSTAPD_LEVEL_INFO = 2,
		HOSTAPD_LEVEL_NOTICE = 3,
		HOSTAPD_LEVEL_WARNING = 4
	} logger_syslog_level, logger_stdout_level;

#define HOSTAPD_MODULE_IEEE80211 BIT(0)
#define HOSTAPD_MODULE_IEEE8021X BIT(1)
#define HOSTAPD_MODULE_RADIUS BIT(2)
#define HOSTAPD_MODULE_WPA BIT(3)
#define HOSTAPD_MODULE_DRIVER BIT(4)
#define HOSTAPD_MODULE_IAPP BIT(5)
	unsigned int logger_syslog; /* module bitfield */
	unsigned int logger_stdout; /* module bitfield */

	enum { HOSTAPD_DEBUG_NO = 0, HOSTAPD_DEBUG_MINIMAL = 1,
	       HOSTAPD_DEBUG_VERBOSE = 2,
	       HOSTAPD_DEBUG_MSGDUMPS = 3,
	       HOSTAPD_DEBUG_EXCESSIVE = 4 } debug; /* debug verbosity level */
	char *dump_log_name; /* file name for state dump (SIGUSR1) */

	int ieee802_1x; /* use IEEE 802.1X */
	int eap_server; /* Use internal EAP server instead of external
			 * RADIUS server */
	struct hostapd_eap_user *eap_user;
	char *eap_sim_db;
	struct hostapd_ip_addr own_ip_addr;
	char *nas_identifier;
	struct hostapd_radius_servers *radius;

#define HOSTAPD_SSID_LEN 32
	char ssid[HOSTAPD_SSID_LEN + 1];
	size_t ssid_len;
	int ssid_set;
	char *eap_req_id_text; /* optional displayable message sent with
				* EAP Request-Identity */
	size_t eap_req_id_text_len;
	int eapol_key_index_workaround;
	int eapol_version;

	size_t default_wep_key_len;
	int individual_wep_key_len;
	int wep_rekeying_period;
	int eap_reauth_period;

	int ieee802_11f; /* use IEEE 802.11f (IAPP) */
	char iapp_iface[IFNAMSIZ + 1]; /* interface used with IAPP broadcast
					* frames */

	u8 assoc_ap_addr[ETH_ALEN];
	int assoc_ap; /* whether assoc_ap_addr is set */

	enum {
		ACCEPT_UNLESS_DENIED = 0,
		DENY_UNLESS_ACCEPTED = 1,
		USE_EXTERNAL_RADIUS_AUTH = 2
	} macaddr_acl;
	macaddr *accept_mac;
	int num_accept_mac;
	macaddr *deny_mac;
	int num_deny_mac;

#define HOSTAPD_AUTH_OPEN BIT(0)
#define HOSTAPD_AUTH_SHARED_KEY BIT(1)
	int auth_algs; /* bitfield of allowed IEEE 802.11 authentication
			* algorithms */

#define HOSTAPD_WPA_VERSION_WPA BIT(0)
#define HOSTAPD_WPA_VERSION_WPA2 BIT(1)
	int wpa;
	struct hostapd_wpa_psk *wpa_psk;
	char *wpa_passphrase;
	char *wpa_psk_file;
#define WPA_KEY_MGMT_IEEE8021X BIT(0)
#define WPA_KEY_MGMT_PSK BIT(1)
	int wpa_key_mgmt;
#define WPA_CIPHER_NONE BIT(0)
#define WPA_CIPHER_WEP40 BIT(1)
#define WPA_CIPHER_WEP104 BIT(2)
#define WPA_CIPHER_TKIP BIT(3)
#define WPA_CIPHER_CCMP BIT(4)
	int wpa_pairwise;
	int wpa_group;
	int wpa_group_rekey;
	int wpa_strict_rekey;
	int wpa_gmk_rekey;
	int rsn_preauth;
	char *rsn_preauth_interfaces;

	char *ctrl_interface; /* directory for UNIX domain sockets */
	gid_t ctrl_interface_gid;
	int ctrl_interface_gid_set;

	char *ca_cert;
	char *server_cert;
	char *private_key;
	char *private_key_passwd;
	int check_crl;

	char *radius_server_clients;
	int radius_server_auth_port;
	int radius_server_ipv6;

	char *test_socket; /* UNIX domain socket path for driver_test */

	int use_pae_group_addr; /* Whether to send EAPOL frames to PAE group
				 * address instead of individual address
				 * (for driver_wired.c).
				 */
};


struct hostapd_config * hostapd_config_read(const char *fname);
void hostapd_config_free(struct hostapd_config *conf);
int hostapd_maclist_found(macaddr *list, int num_entries, u8 *addr);
const u8 * hostapd_get_psk(const struct hostapd_config *conf, const u8 *addr,
			   const u8 *prev_psk);
int hostapd_setup_wpa_psk(struct hostapd_config *conf);
const struct hostapd_eap_user *
hostapd_get_eap_user(const struct hostapd_config *conf, const u8 *identity,
		     size_t identity_len, int phase2);

#endif /* CONFIG_H */
