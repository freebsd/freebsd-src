/*
 * hostapd / Configuration definitions and helpers functions
 * Copyright (c) 2003-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef HOSTAPD_CONFIG_H
#define HOSTAPD_CONFIG_H

#include "common/defs.h"
#include "utils/list.h"
#include "ip_addr.h"
#include "common/wpa_common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "crypto/sha256.h"
#include "wps/wps.h"
#include "fst/fst.h"
#include "vlan.h"

enum macaddr_acl {
	ACCEPT_UNLESS_DENIED = 0,
	DENY_UNLESS_ACCEPTED = 1,
	USE_EXTERNAL_RADIUS_AUTH = 2
};

/**
 * mesh_conf - local MBSS state and settings
 */
struct mesh_conf {
	u8 meshid[32];
	u8 meshid_len;
	/* Active Path Selection Protocol Identifier */
	u8 mesh_pp_id;
	/* Active Path Selection Metric Identifier */
	u8 mesh_pm_id;
	/* Congestion Control Mode Identifier */
	u8 mesh_cc_id;
	/* Synchronization Protocol Identifier */
	u8 mesh_sp_id;
	/* Authentication Protocol Identifier */
	u8 mesh_auth_id;
	u8 *rsn_ie;
	int rsn_ie_len;
#define MESH_CONF_SEC_NONE BIT(0)
#define MESH_CONF_SEC_AUTH BIT(1)
#define MESH_CONF_SEC_AMPE BIT(2)
	unsigned int security;
	enum mfp_options ieee80211w;
	int ocv;
	unsigned int pairwise_cipher;
	unsigned int group_cipher;
	unsigned int mgmt_group_cipher;
	int dot11MeshMaxRetries;
	int dot11MeshRetryTimeout; /* msec */
	int dot11MeshConfirmTimeout; /* msec */
	int dot11MeshHoldingTimeout; /* msec */
	int mesh_fwding;
};

#define MAX_STA_COUNT 2007
#define MAX_VLAN_ID 4094

typedef u8 macaddr[ETH_ALEN];

struct mac_acl_entry {
	macaddr addr;
	struct vlan_description vlan_id;
};

struct hostapd_radius_servers;
struct ft_remote_r0kh;
struct ft_remote_r1kh;

#ifdef CONFIG_WEP
#define NUM_WEP_KEYS 4
struct hostapd_wep_keys {
	u8 idx;
	u8 *key[NUM_WEP_KEYS];
	size_t len[NUM_WEP_KEYS];
	int keys_set;
	size_t default_len; /* key length used for dynamic key generation */
};
#endif /* CONFIG_WEP */

typedef enum hostap_security_policy {
	SECURITY_PLAINTEXT = 0,
#ifdef CONFIG_WEP
	SECURITY_STATIC_WEP = 1,
#endif /* CONFIG_WEP */
	SECURITY_IEEE_802_1X = 2,
	SECURITY_WPA_PSK = 3,
	SECURITY_WPA = 4,
	SECURITY_OSEN = 5
} secpolicy;

struct hostapd_ssid {
	u8 ssid[SSID_MAX_LEN];
	size_t ssid_len;
	u32 short_ssid;
	unsigned int ssid_set:1;
	unsigned int utf8_ssid:1;
	unsigned int wpa_passphrase_set:1;
	unsigned int wpa_psk_set:1;

	char vlan[IFNAMSIZ + 1];
	secpolicy security_policy;

	struct hostapd_wpa_psk *wpa_psk;
	char *wpa_passphrase;
	char *wpa_psk_file;
	struct sae_pt *pt;

#ifdef CONFIG_WEP
	struct hostapd_wep_keys wep;
#endif /* CONFIG_WEP */

#define DYNAMIC_VLAN_DISABLED 0
#define DYNAMIC_VLAN_OPTIONAL 1
#define DYNAMIC_VLAN_REQUIRED 2
	int dynamic_vlan;
#define DYNAMIC_VLAN_NAMING_WITHOUT_DEVICE 0
#define DYNAMIC_VLAN_NAMING_WITH_DEVICE 1
#define DYNAMIC_VLAN_NAMING_END 2
	int vlan_naming;
	int per_sta_vif;
#ifdef CONFIG_FULL_DYNAMIC_VLAN
	char *vlan_tagged_interface;
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
};


#define VLAN_ID_WILDCARD -1

struct hostapd_vlan {
	struct hostapd_vlan *next;
	int vlan_id; /* VLAN ID or -1 (VLAN_ID_WILDCARD) for wildcard entry */
	struct vlan_description vlan_desc;
	char ifname[IFNAMSIZ + 1];
	char bridge[IFNAMSIZ + 1];
	int configured;
	int dynamic_vlan;
#ifdef CONFIG_FULL_DYNAMIC_VLAN

#define DVLAN_CLEAN_WLAN_PORT	0x8
	int clean;
#endif /* CONFIG_FULL_DYNAMIC_VLAN */
};

#define PMK_LEN 32
#define KEYID_LEN 32
#define MIN_PASSPHRASE_LEN 8
#define MAX_PASSPHRASE_LEN 63
struct hostapd_sta_wpa_psk_short {
	struct hostapd_sta_wpa_psk_short *next;
	unsigned int is_passphrase:1;
	u8 psk[PMK_LEN];
	char passphrase[MAX_PASSPHRASE_LEN + 1];
	int ref; /* (number of references held) - 1 */
};

struct hostapd_wpa_psk {
	struct hostapd_wpa_psk *next;
	int group;
	char keyid[KEYID_LEN];
	int wps;
	u8 psk[PMK_LEN];
	u8 addr[ETH_ALEN];
	u8 p2p_dev_addr[ETH_ALEN];
	int vlan_id;
};

struct hostapd_eap_user {
	struct hostapd_eap_user *next;
	u8 *identity;
	size_t identity_len;
	struct {
		int vendor;
		u32 method;
	} methods[EAP_MAX_METHODS];
	u8 *password;
	size_t password_len;
	u8 *salt;
	size_t salt_len; /* non-zero when password is salted */
	int phase2;
	int force_version;
	unsigned int wildcard_prefix:1;
	unsigned int password_hash:1; /* whether password is hashed with
				       * nt_password_hash() */
	unsigned int remediation:1;
	unsigned int macacl:1;
	int ttls_auth; /* EAP_TTLS_AUTH_* bitfield */
	struct hostapd_radius_attr *accept_attr;
	u32 t_c_timestamp;
};

struct hostapd_radius_attr {
	u8 type;
	struct wpabuf *val;
	struct hostapd_radius_attr *next;
};


#define NUM_TX_QUEUES 4
#define MAX_ROAMING_CONSORTIUM_LEN 15

struct hostapd_roaming_consortium {
	u8 len;
	u8 oi[MAX_ROAMING_CONSORTIUM_LEN];
};

struct hostapd_lang_string {
	u8 lang[3];
	u8 name_len;
	u8 name[252];
};

struct hostapd_venue_url {
	u8 venue_number;
	u8 url_len;
	u8 url[254];
};

#define MAX_NAI_REALMS 10
#define MAX_NAI_REALMLEN 255
#define MAX_NAI_EAP_METHODS 5
#define MAX_NAI_AUTH_TYPES 4
struct hostapd_nai_realm_data {
	u8 encoding;
	char realm_buf[MAX_NAI_REALMLEN + 1];
	char *realm[MAX_NAI_REALMS];
	u8 eap_method_count;
	struct hostapd_nai_realm_eap {
		u8 eap_method;
		u8 num_auths;
		u8 auth_id[MAX_NAI_AUTH_TYPES];
		u8 auth_val[MAX_NAI_AUTH_TYPES];
	} eap_method[MAX_NAI_EAP_METHODS];
};

struct anqp_element {
	struct dl_list list;
	u16 infoid;
	struct wpabuf *payload;
};

struct fils_realm {
	struct dl_list list;
	u8 hash[2];
	char realm[];
};

struct sae_password_entry {
	struct sae_password_entry *next;
	char *password;
	char *identifier;
	u8 peer_addr[ETH_ALEN];
	int vlan_id;
	struct sae_pt *pt;
	struct sae_pk *pk;
};

struct dpp_controller_conf {
	struct dpp_controller_conf *next;
	u8 pkhash[SHA256_MAC_LEN];
	struct hostapd_ip_addr ipaddr;
};

struct airtime_sta_weight {
	struct airtime_sta_weight *next;
	unsigned int weight;
	u8 addr[ETH_ALEN];
};

#define EXT_CAPA_MAX_LEN 15

/**
 * struct hostapd_bss_config - Per-BSS configuration
 */
struct hostapd_bss_config {
	char iface[IFNAMSIZ + 1];
	char bridge[IFNAMSIZ + 1];
	char vlan_bridge[IFNAMSIZ + 1];
	char wds_bridge[IFNAMSIZ + 1];
	int bridge_hairpin; /* hairpin_mode on bridge members */

	enum hostapd_logger_level logger_syslog_level, logger_stdout_level;

	unsigned int logger_syslog; /* module bitfield */
	unsigned int logger_stdout; /* module bitfield */

	int max_num_sta; /* maximum number of STAs in station table */

	int dtim_period;
	unsigned int bss_load_update_period;
	unsigned int chan_util_avg_period;

	int ieee802_1x; /* use IEEE 802.1X */
	int eapol_version;
	int eap_server; /* Use internal EAP server instead of external
			 * RADIUS server */
	struct hostapd_eap_user *eap_user;
	char *eap_user_sqlite;
	char *eap_sim_db;
	unsigned int eap_sim_db_timeout;
	int eap_server_erp; /* Whether ERP is enabled on internal EAP server */
	struct hostapd_ip_addr own_ip_addr;
	char *nas_identifier;
	struct hostapd_radius_servers *radius;
	int radius_require_message_authenticator;
	int acct_interim_interval;
	int radius_request_cui;
	struct hostapd_radius_attr *radius_auth_req_attr;
	struct hostapd_radius_attr *radius_acct_req_attr;
	char *radius_req_attr_sqlite;
	int radius_das_port;
	unsigned int radius_das_time_window;
	int radius_das_require_event_timestamp;
	int radius_das_require_message_authenticator;
	struct hostapd_ip_addr radius_das_client_addr;
	u8 *radius_das_shared_secret;
	size_t radius_das_shared_secret_len;

	struct hostapd_ssid ssid;

	char *eap_req_id_text; /* optional displayable message sent with
				* EAP Request-Identity */
	size_t eap_req_id_text_len;
	int eapol_key_index_workaround;

#ifdef CONFIG_WEP
	size_t default_wep_key_len;
	int individual_wep_key_len;
	int wep_rekeying_period;
	int broadcast_key_idx_min, broadcast_key_idx_max;
#endif /* CONFIG_WEP */
	int eap_reauth_period;
	int erp_send_reauth_start;
	char *erp_domain;
#ifdef CONFIG_TESTING_OPTIONS
	bool eap_skip_prot_success;
#endif /* CONFIG_TESTING_OPTIONS */

	enum macaddr_acl macaddr_acl;
	struct mac_acl_entry *accept_mac;
	int num_accept_mac;
	struct mac_acl_entry *deny_mac;
	int num_deny_mac;
	int wds_sta;
	int isolate;
	int start_disabled;

	int auth_algs; /* bitfield of allowed IEEE 802.11 authentication
			* algorithms, WPA_AUTH_ALG_{OPEN,SHARED,LEAP} */

	int wpa; /* bitfield of WPA_PROTO_WPA, WPA_PROTO_RSN */
	int extended_key_id;
	int wpa_key_mgmt;
	enum mfp_options ieee80211w;
	int group_mgmt_cipher;
	int beacon_prot;
	/* dot11AssociationSAQueryMaximumTimeout (in TUs) */
	unsigned int assoc_sa_query_max_timeout;
	/* dot11AssociationSAQueryRetryTimeout (in TUs) */
	int assoc_sa_query_retry_timeout;
#ifdef CONFIG_OCV
	int ocv; /* Operating Channel Validation */
#endif /* CONFIG_OCV */
	enum {
		PSK_RADIUS_IGNORED = 0,
		PSK_RADIUS_ACCEPTED = 1,
		PSK_RADIUS_REQUIRED = 2,
		PSK_RADIUS_DURING_4WAY_HS = 3,
	} wpa_psk_radius;
	int wpa_pairwise;
	int group_cipher; /* wpa_group value override from configuation */
	int wpa_group;
	int wpa_group_rekey;
	int wpa_group_rekey_set;
	int wpa_strict_rekey;
	int wpa_gmk_rekey;
	int wpa_ptk_rekey;
	enum ptk0_rekey_handling wpa_deny_ptk0_rekey;
	u32 wpa_group_update_count;
	u32 wpa_pairwise_update_count;
	int wpa_disable_eapol_key_retries;
	int rsn_pairwise;
	int rsn_preauth;
	char *rsn_preauth_interfaces;

#ifdef CONFIG_IEEE80211R_AP
	/* IEEE 802.11r - Fast BSS Transition */
	u8 mobility_domain[MOBILITY_DOMAIN_ID_LEN];
	u8 r1_key_holder[FT_R1KH_ID_LEN];
	u32 r0_key_lifetime; /* PMK-R0 lifetime seconds */
	int rkh_pos_timeout;
	int rkh_neg_timeout;
	int rkh_pull_timeout; /* ms */
	int rkh_pull_retries;
	u32 reassociation_deadline;
	struct ft_remote_r0kh *r0kh_list;
	struct ft_remote_r1kh *r1kh_list;
	int pmk_r1_push;
	int ft_over_ds;
	int ft_psk_generate_local;
	int r1_max_key_lifetime;
	char *rxkh_file;
#endif /* CONFIG_IEEE80211R_AP */

	char *ctrl_interface; /* directory for UNIX domain sockets */
#ifndef CONFIG_NATIVE_WINDOWS
	gid_t ctrl_interface_gid;
#endif /* CONFIG_NATIVE_WINDOWS */
	int ctrl_interface_gid_set;

	char *ca_cert;
	char *server_cert;
	char *server_cert2;
	char *private_key;
	char *private_key2;
	char *private_key_passwd;
	char *private_key_passwd2;
	char *check_cert_subject;
	int check_crl;
	int check_crl_strict;
	unsigned int crl_reload_interval;
	unsigned int tls_session_lifetime;
	unsigned int tls_flags;
	unsigned int max_auth_rounds;
	unsigned int max_auth_rounds_short;
	char *ocsp_stapling_response;
	char *ocsp_stapling_response_multi;
	char *dh_file;
	char *openssl_ciphers;
	char *openssl_ecdh_curves;
	u8 *pac_opaque_encr_key;
	u8 *eap_fast_a_id;
	size_t eap_fast_a_id_len;
	char *eap_fast_a_id_info;
	int eap_fast_prov;
	int pac_key_lifetime;
	int pac_key_refresh_time;
	int eap_teap_auth;
	int eap_teap_pac_no_inner;
	int eap_teap_separate_result;
	int eap_teap_id;
	int eap_teap_method_sequence;
	int eap_sim_aka_result_ind;
	int eap_sim_id;
	char *imsi_privacy_key;
	int eap_sim_aka_fast_reauth_limit;
	int tnc;
	int fragment_size;
	u16 pwd_group;

	char *radius_server_clients;
	int radius_server_auth_port;
	int radius_server_acct_port;
	int radius_server_ipv6;

	int use_pae_group_addr; /* Whether to send EAPOL frames to PAE group
				 * address instead of individual address
				 * (for driver_wired.c).
				 */

	int ap_max_inactivity;
	int bss_max_idle;
	int max_acceptable_idle_period;
	bool no_disconnect_on_group_keyerror;
	int ignore_broadcast_ssid;
	int no_probe_resp_if_max_sta;

	int wmm_enabled;
	int wmm_uapsd;

	struct hostapd_vlan *vlan;

	macaddr bssid;

	/*
	 * Maximum listen interval that STAs can use when associating with this
	 * BSS. If a STA tries to use larger value, the association will be
	 * denied with status code 51.
	 */
	u16 max_listen_interval;

	int disable_pmksa_caching;
	int okc; /* Opportunistic Key Caching */

	int wps_state;
#ifdef CONFIG_WPS
	int wps_independent;
	int ap_setup_locked;
	u8 uuid[16];
	char *wps_pin_requests;
	char *device_name;
	char *manufacturer;
	char *model_name;
	char *model_number;
	char *serial_number;
	u8 device_type[WPS_DEV_TYPE_LEN];
	char *config_methods;
	u8 os_version[4];
	char *ap_pin;
	int skip_cred_build;
	u8 *extra_cred;
	size_t extra_cred_len;
	int wps_cred_processing;
	int wps_cred_add_sae;
	int force_per_enrollee_psk;
	u8 *ap_settings;
	size_t ap_settings_len;
	struct hostapd_ssid multi_ap_backhaul_ssid;
	char *upnp_iface;
	char *friendly_name;
	char *manufacturer_url;
	char *model_description;
	char *model_url;
	char *upc;
	struct wpabuf *wps_vendor_ext[MAX_WPS_VENDOR_EXTENSIONS];
	struct wpabuf *wps_application_ext;
	int wps_nfc_pw_from_config;
	int wps_nfc_dev_pw_id;
	struct wpabuf *wps_nfc_dh_pubkey;
	struct wpabuf *wps_nfc_dh_privkey;
	struct wpabuf *wps_nfc_dev_pw;
#endif /* CONFIG_WPS */
	int pbc_in_m1;
	char *server_id;

#define P2P_ENABLED BIT(0)
#define P2P_GROUP_OWNER BIT(1)
#define P2P_GROUP_FORMATION BIT(2)
#define P2P_MANAGE BIT(3)
#define P2P_ALLOW_CROSS_CONNECTION BIT(4)
	int p2p;
#ifdef CONFIG_P2P
	u8 ip_addr_go[4];
	u8 ip_addr_mask[4];
	u8 ip_addr_start[4];
	u8 ip_addr_end[4];
#endif /* CONFIG_P2P */

	int disassoc_low_ack;
	int skip_inactivity_poll;

#define TDLS_PROHIBIT BIT(0)
#define TDLS_PROHIBIT_CHAN_SWITCH BIT(1)
	int tdls;
	bool disable_11n;
	bool disable_11ac;
	bool disable_11ax;
	bool disable_11be;

	/* IEEE 802.11v */
	int time_advertisement;
	char *time_zone;
	int wnm_sleep_mode;
	int wnm_sleep_mode_no_keys;
	int bss_transition;

	/* IEEE 802.11u - Interworking */
	int interworking;
	int access_network_type;
	int internet;
	int asra;
	int esr;
	int uesa;
	int venue_info_set;
	u8 venue_group;
	u8 venue_type;
	u8 hessid[ETH_ALEN];

	/* IEEE 802.11u - Roaming Consortium list */
	unsigned int roaming_consortium_count;
	struct hostapd_roaming_consortium *roaming_consortium;

	/* IEEE 802.11u - Venue Name duples */
	unsigned int venue_name_count;
	struct hostapd_lang_string *venue_name;

	/* Venue URL duples */
	unsigned int venue_url_count;
	struct hostapd_venue_url *venue_url;

	/* IEEE 802.11u - Network Authentication Type */
	u8 *network_auth_type;
	size_t network_auth_type_len;

	/* IEEE 802.11u - IP Address Type Availability */
	u8 ipaddr_type_availability;
	u8 ipaddr_type_configured;

	/* IEEE 802.11u - 3GPP Cellular Network */
	u8 *anqp_3gpp_cell_net;
	size_t anqp_3gpp_cell_net_len;

	/* IEEE 802.11u - Domain Name */
	u8 *domain_name;
	size_t domain_name_len;

	unsigned int nai_realm_count;
	struct hostapd_nai_realm_data *nai_realm_data;

	struct dl_list anqp_elem; /* list of struct anqp_element */

	u16 gas_comeback_delay;
	size_t gas_frag_limit;
	int gas_address3;

	u8 qos_map_set[16 + 2 * 21];
	unsigned int qos_map_set_len;

	int osen;
	int proxy_arp;
	int na_mcast_to_ucast;

#ifdef CONFIG_HS20
	int hs20;
	int hs20_release;
	int disable_dgaf;
	u16 anqp_domain_id;
	unsigned int hs20_oper_friendly_name_count;
	struct hostapd_lang_string *hs20_oper_friendly_name;
	u8 *hs20_wan_metrics;
	u8 *hs20_connection_capability;
	size_t hs20_connection_capability_len;
	u8 *hs20_operating_class;
	u8 hs20_operating_class_len;
	struct hs20_icon {
		u16 width;
		u16 height;
		char language[3];
		char type[256];
		char name[256];
		char file[256];
	} *hs20_icons;
	size_t hs20_icons_count;
	u8 osu_ssid[SSID_MAX_LEN];
	size_t osu_ssid_len;
	struct hs20_osu_provider {
		unsigned int friendly_name_count;
		struct hostapd_lang_string *friendly_name;
		char *server_uri;
		int *method_list;
		char **icons;
		size_t icons_count;
		char *osu_nai;
		char *osu_nai2;
		unsigned int service_desc_count;
		struct hostapd_lang_string *service_desc;
	} *hs20_osu_providers, *last_osu;
	size_t hs20_osu_providers_count;
	size_t hs20_osu_providers_nai_count;
	char **hs20_operator_icon;
	size_t hs20_operator_icon_count;
	unsigned int hs20_deauth_req_timeout;
	char *subscr_remediation_url;
	u8 subscr_remediation_method;
	char *hs20_sim_provisioning_url;
	char *t_c_filename;
	u32 t_c_timestamp;
	char *t_c_server_url;
#endif /* CONFIG_HS20 */

	u8 wps_rf_bands; /* RF bands for WPS (WPS_RF_*) */

#ifdef CONFIG_RADIUS_TEST
	char *dump_msk_file;
#endif /* CONFIG_RADIUS_TEST */

	struct wpabuf *vendor_elements;
	struct wpabuf *assocresp_elements;

	unsigned int anti_clogging_threshold;
	unsigned int sae_sync;
	int sae_require_mfp;
	int sae_confirm_immediate;
	enum sae_pwe sae_pwe;
	int *sae_groups;
	struct sae_password_entry *sae_passwords;

	char *wowlan_triggers; /* Wake-on-WLAN triggers */

#ifdef CONFIG_TESTING_OPTIONS
	u8 bss_load_test[5];
	u8 bss_load_test_set;
	struct wpabuf *own_ie_override;
	int sae_reflection_attack;
	int sae_commit_status;
	int sae_pk_omit;
	int sae_pk_password_check_skip;
	struct wpabuf *sae_commit_override;
	struct wpabuf *rsne_override_eapol;
	struct wpabuf *rsnxe_override_eapol;
	struct wpabuf *rsne_override_ft;
	struct wpabuf *rsnxe_override_ft;
	struct wpabuf *gtk_rsc_override;
	struct wpabuf *igtk_rsc_override;
	int no_beacon_rsnxe;
	int skip_prune_assoc;
	int ft_rsnxe_used;
	unsigned int oci_freq_override_eapol_m3;
	unsigned int oci_freq_override_eapol_g1;
	unsigned int oci_freq_override_saquery_req;
	unsigned int oci_freq_override_saquery_resp;
	unsigned int oci_freq_override_ft_assoc;
	unsigned int oci_freq_override_fils_assoc;
	unsigned int oci_freq_override_wnm_sleep;
	struct wpabuf *eapol_m1_elements;
	struct wpabuf *eapol_m3_elements;
	bool eapol_m3_no_encrypt;
	int test_assoc_comeback_type;
	struct wpabuf *presp_elements;

#ifdef CONFIG_IEEE80211BE
	u16 eht_oper_puncturing_override;
#endif /* CONFIG_IEEE80211BE */
#endif /* CONFIG_TESTING_OPTIONS */

#define MESH_ENABLED BIT(0)
	int mesh;
	int mesh_fwding;

	u8 radio_measurements[RRM_CAPABILITIES_IE_LEN];

	int vendor_vht;
	int use_sta_nsts;

	char *no_probe_resp_if_seen_on;
	char *no_auth_if_seen_on;

	int pbss;

#ifdef CONFIG_MBO
	int mbo_enabled;
	/**
	 * oce - Enable OCE in AP and/or STA-CFON mode
	 *  - BIT(0) is Reserved
	 *  - Set BIT(1) to enable OCE in STA-CFON mode
	 *  - Set BIT(2) to enable OCE in AP mode
	 */
	unsigned int oce;
	int mbo_cell_data_conn_pref;
#endif /* CONFIG_MBO */

	int ftm_responder;
	int ftm_initiator;

#ifdef CONFIG_FILS
	u8 fils_cache_id[FILS_CACHE_ID_LEN];
	int fils_cache_id_set;
	struct dl_list fils_realms; /* list of struct fils_realm */
	int fils_dh_group;
	struct hostapd_ip_addr dhcp_server;
	int dhcp_rapid_commit_proxy;
	unsigned int fils_hlp_wait_time;
	u16 dhcp_server_port;
	u16 dhcp_relay_port;
	u32 fils_discovery_min_int;
	u32 fils_discovery_max_int;
#endif /* CONFIG_FILS */

	int multicast_to_unicast;
	int bridge_multicast_to_unicast;

	int broadcast_deauth;

	int notify_mgmt_frames;

#ifdef CONFIG_DPP
	char *dpp_name;
	char *dpp_mud_url;
	char *dpp_extra_conf_req_name;
	char *dpp_extra_conf_req_value;
	char *dpp_connector;
	struct wpabuf *dpp_netaccesskey;
	unsigned int dpp_netaccesskey_expiry;
	struct wpabuf *dpp_csign;
#ifdef CONFIG_DPP2
	struct dpp_controller_conf *dpp_controller;
	int dpp_relay_port;
	int dpp_configurator_connectivity;
	int dpp_pfs;
#endif /* CONFIG_DPP2 */
#endif /* CONFIG_DPP */

#ifdef CONFIG_OWE
	macaddr owe_transition_bssid;
	u8 owe_transition_ssid[SSID_MAX_LEN];
	size_t owe_transition_ssid_len;
	char owe_transition_ifname[IFNAMSIZ + 1];
	int *owe_groups;
	int owe_ptk_workaround;
#endif /* CONFIG_OWE */

	int coloc_intf_reporting;

	u8 send_probe_response;

	u8 transition_disable;

#define BACKHAUL_BSS 1
#define FRONTHAUL_BSS 2
	int multi_ap; /* bitmap of BACKHAUL_BSS, FRONTHAUL_BSS */
	int multi_ap_profile;
	/* Multi-AP Profile-1 clients not allowed to connect */
#define PROFILE1_CLIENT_ASSOC_DISALLOW BIT(0)
	/* Multi-AP Profile-2 clients not allowed to connect */
#define PROFILE2_CLIENT_ASSOC_DISALLOW BIT(1)
	unsigned int multi_ap_client_disallow;
	/* Primary VLAN ID to use in Multi-AP */
	int multi_ap_vlanid;

#ifdef CONFIG_AIRTIME_POLICY
	unsigned int airtime_weight;
	int airtime_limit;
	struct airtime_sta_weight *airtime_weight_list;
#endif /* CONFIG_AIRTIME_POLICY */

#ifdef CONFIG_MACSEC
	/**
	 * macsec_policy - Determines the policy for MACsec secure session
	 *
	 * 0: MACsec not in use (default)
	 * 1: MACsec enabled - Should secure, accept key server's advice to
	 *    determine whether to use a secure session or not.
	 */
	int macsec_policy;

	/**
	 * macsec_integ_only - Determines how MACsec are transmitted
	 *
	 * This setting applies only when MACsec is in use, i.e.,
	 *  - macsec_policy is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0: Encrypt traffic (default)
	 * 1: Integrity only
	 */
	int macsec_integ_only;

	/**
	 * macsec_replay_protect - Enable MACsec replay protection
	 *
	 * This setting applies only when MACsec is in use, i.e.,
	 *  - macsec_policy is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0: Replay protection disabled (default)
	 * 1: Replay protection enabled
	 */
	int macsec_replay_protect;

	/**
	 * macsec_replay_window - MACsec replay protection window
	 *
	 * A window in which replay is tolerated, to allow receipt of frames
	 * that have been misordered by the network.
	 *
	 * This setting applies only when MACsec replay protection active, i.e.,
	 *  - macsec_replay_protect is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0: No replay window, strict check (default)
	 * 1..2^32-1: number of packets that could be misordered
	 */
	u32 macsec_replay_window;

	/**
	 * macsec_offload - Enable MACsec offload
	 *
	 * This setting applies only when MACsec is in use, i.e.,
	 *  - macsec_policy is enabled
	 *  - the key server has decided to enable MACsec
	 *
	 * 0 = MACSEC_OFFLOAD_OFF (default)
	 * 1 = MACSEC_OFFLOAD_PHY
	 * 2 = MACSEC_OFFLOAD_MAC
	 */
	int macsec_offload;

	/**
	 * macsec_port - MACsec port (in SCI)
	 *
	 * Port component of the SCI.
	 *
	 * Range: 1-65534 (default: 1)
	 */
	int macsec_port;

	/**
	 * mka_priority - Priority of MKA Actor
	 *
	 * Range: 0-255 (default: 255)
	 */
	int mka_priority;

	/**
	 * macsec_csindex - Cipher suite index for MACsec
	 *
	 * Range: 0-1 (default: 0)
	 */
	int macsec_csindex;

	/**
	 * mka_ckn - MKA pre-shared CKN
	 */
#define MACSEC_CKN_MAX_LEN 32
	size_t mka_ckn_len;
	u8 mka_ckn[MACSEC_CKN_MAX_LEN];

	/**
	 * mka_cak - MKA pre-shared CAK
	 */
#define MACSEC_CAK_MAX_LEN 32
	size_t mka_cak_len;
	u8 mka_cak[MACSEC_CAK_MAX_LEN];

#define MKA_PSK_SET_CKN BIT(0)
#define MKA_PSK_SET_CAK BIT(1)
#define MKA_PSK_SET (MKA_PSK_SET_CKN | MKA_PSK_SET_CAK)
	/**
	 * mka_psk_set - Whether mka_ckn and mka_cak are set
	 */
	u8 mka_psk_set;
#endif /* CONFIG_MACSEC */

#ifdef CONFIG_PASN
	/* Whether to allow PASN-UNAUTH */
	int pasn_noauth;

#ifdef CONFIG_TESTING_OPTIONS
	/*
	 * Normally, KDK should be derived if and only if both sides support
	 * secure LTF. Allow forcing KDK derivation for testing purposes.
	 */
	int force_kdk_derivation;

	/* If set, corrupt the MIC in the 2nd Authentication frame of PASN */
	int pasn_corrupt_mic;
#endif /* CONFIG_TESTING_OPTIONS */

	int *pasn_groups;

	/*
	 * The time in TUs after which the non-AP STA is requested to retry the
	 * PASN authentication in case there are too many parallel operations.
	 */
	u16 pasn_comeback_after;
#endif /* CONFIG_PASN */

	unsigned int unsol_bcast_probe_resp_interval;

	u8 ext_capa_mask[EXT_CAPA_MAX_LEN];
	u8 ext_capa[EXT_CAPA_MAX_LEN];

	u8 rnr;
	char *config_id;
	bool xrates_supported;

	bool ssid_protection;

#ifdef CONFIG_IEEE80211BE
	/* The AP is part of an AP MLD */
	u8 mld_ap;

	/* The MLD ID to which the AP MLD is affiliated with */
	u8 mld_id;

	/* The AP's MLD MAC address within the AP MLD */
	u8 mld_addr[ETH_ALEN];

#ifdef CONFIG_TESTING_OPTIONS
	/*
	 * If set indicate the AP as disabled in the RNR element included in the
	 * other APs in the AP MLD.
	 */
	bool mld_indicate_disabled;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_IEEE80211BE */
};

/**
 * struct he_phy_capabilities_info - HE PHY capabilities
 */
struct he_phy_capabilities_info {
	bool he_su_beamformer;
	bool he_su_beamformee;
	bool he_mu_beamformer;
};

/**
 * struct he_operation - HE operation
 */
struct he_operation {
	u8 he_bss_color;
	u8 he_bss_color_disabled;
	u8 he_bss_color_partial;
	u8 he_default_pe_duration;
	u8 he_twt_required;
	u8 he_twt_responder;
	u16 he_rts_threshold;
	u8 he_er_su_disable;
	u16 he_basic_mcs_nss_set;
};

/**
 * struct spatial_reuse - Spatial reuse
 */
struct spatial_reuse {
	u8 sr_control;
	u8 non_srg_obss_pd_max_offset;
	u8 srg_obss_pd_min_offset;
	u8 srg_obss_pd_max_offset;
	u8 srg_bss_color_bitmap[8];
	u8 srg_partial_bssid_bitmap[8];
};

/**
 * struct eht_phy_capabilities_info - EHT PHY capabilities
 */
struct eht_phy_capabilities_info {
	bool su_beamformer;
	bool su_beamformee;
	bool mu_beamformer;
};

/**
 * struct hostapd_config - Per-radio interface configuration
 */
struct hostapd_config {
	struct hostapd_bss_config **bss, *last_bss;
	size_t num_bss;

	u16 beacon_int;
	int rts_threshold;
	int fragm_threshold;
	u8 op_class;
	u8 channel;
	int enable_edmg;
	u8 edmg_channel;
	u8 acs;
	struct wpa_freq_range_list acs_ch_list;
	struct wpa_freq_range_list acs_freq_list;
	u8 acs_freq_list_present;
	int acs_exclude_dfs;
	u8 min_tx_power;
	enum hostapd_hw_mode hw_mode; /* HOSTAPD_MODE_IEEE80211A, .. */
	bool hw_mode_set;
	int acs_exclude_6ghz_non_psc;
	int enable_background_radar;
	enum {
		LONG_PREAMBLE = 0,
		SHORT_PREAMBLE = 1
	} preamble;

	int *supported_rates;
	int *basic_rates;
	unsigned int beacon_rate;
	enum beacon_rate_type rate_type;

	const struct wpa_driver_ops *driver;
	char *driver_params;

	int ap_table_max_size;
	int ap_table_expiration_time;

	unsigned int track_sta_max_num;
	unsigned int track_sta_max_age;

	char country[3]; /* first two octets: country code as described in
			  * ISO/IEC 3166-1. Third octet:
			  * ' ' (ascii 32): all environments
			  * 'O': Outdoor environemnt only
			  * 'I': Indoor environment only
			  * 'X': Used with noncountry entity ("XXX")
			  * 0x00..0x31: identifying IEEE 802.11 standard
			  *	Annex E table (0x04 = global table)
			  */

	int ieee80211d;

	int ieee80211h; /* DFS */

	/*
	 * Local power constraint is an octet encoded as an unsigned integer in
	 * units of decibels. Invalid value -1 indicates that Power Constraint
	 * element will not be added.
	 */
	int local_pwr_constraint;

	/* Control Spectrum Management bit */
	int spectrum_mgmt_required;

	struct hostapd_tx_queue_params tx_queue[NUM_TX_QUEUES];

	/*
	 * WMM AC parameters, in same order as 802.1D, i.e.
	 * 0 = BE (best effort)
	 * 1 = BK (background)
	 * 2 = VI (video)
	 * 3 = VO (voice)
	 */
	struct hostapd_wmm_ac_params wmm_ac_params[4];

	int ht_op_mode_fixed;
	u16 ht_capab;
	int ieee80211n;
	int secondary_channel;
	int no_pri_sec_switch;
	int require_ht;
	int obss_interval;
	u32 vht_capab;
	int ieee80211ac;
	int require_vht;
	enum oper_chan_width vht_oper_chwidth;
	u8 vht_oper_centr_freq_seg0_idx;
	u8 vht_oper_centr_freq_seg1_idx;
	u8 ht40_plus_minus_allowed;

	/* Use driver-generated interface addresses when adding multiple BSSs */
	u8 use_driver_iface_addr;

#ifdef CONFIG_FST
	struct fst_iface_cfg fst_cfg;
#endif /* CONFIG_FST */

#ifdef CONFIG_P2P
	u8 p2p_go_ctwindow;
#endif /* CONFIG_P2P */

#ifdef CONFIG_TESTING_OPTIONS
	double ignore_probe_probability;
	double ignore_auth_probability;
	double ignore_assoc_probability;
	double ignore_reassoc_probability;
	double corrupt_gtk_rekey_mic_probability;
	int ecsa_ie_only;
	bool delay_eapol_tx;
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_ACS
	unsigned int acs_num_scans;
	struct acs_bias {
		int channel;
		double bias;
	} *acs_chan_bias;
	unsigned int num_acs_chan_bias;
#endif /* CONFIG_ACS */

	struct wpabuf *lci;
	struct wpabuf *civic;
	int stationary_ap;

	int ieee80211ax;
#ifdef CONFIG_IEEE80211AX
	struct he_phy_capabilities_info he_phy_capab;
	struct he_operation he_op;
	struct ieee80211_he_mu_edca_parameter_set he_mu_edca;
	struct spatial_reuse spr;
	enum oper_chan_width he_oper_chwidth;
	u8 he_oper_centr_freq_seg0_idx;
	u8 he_oper_centr_freq_seg1_idx;
	u8 he_6ghz_max_mpdu;
	u8 he_6ghz_max_ampdu_len_exp;
	u8 he_6ghz_rx_ant_pat;
	u8 he_6ghz_tx_ant_pat;
	u8 he_6ghz_reg_pwr_type;

	int reg_def_cli_eirp_psd;
	int reg_sub_cli_eirp_psd;

	/*
	 * This value should be used when regulatory client EIRP PSD values
	 * advertised by an AP that is an SP AP or an indoor SP AP are
	 * insufficient to ensure that regulatory client limits on total EIRP
	 * are always met for all transmission bandwidths within the bandwidth
	 * of the AP’s BSS.
	 */
	int reg_def_cli_eirp;

	bool require_he;
#endif /* CONFIG_IEEE80211AX */

	/* VHT enable/disable config from CHAN_SWITCH */
#define CH_SWITCH_VHT_ENABLED BIT(0)
#define CH_SWITCH_VHT_DISABLED BIT(1)
	unsigned int ch_switch_vht_config;

	/* HE enable/disable config from CHAN_SWITCH */
#define CH_SWITCH_HE_ENABLED BIT(0)
#define CH_SWITCH_HE_DISABLED BIT(1)
	unsigned int ch_switch_he_config;

	int rssi_reject_assoc_rssi;
	int rssi_reject_assoc_timeout;
	int rssi_ignore_probe_request;

#ifdef CONFIG_AIRTIME_POLICY
	enum {
		AIRTIME_MODE_OFF = 0,
		AIRTIME_MODE_STATIC = 1,
		AIRTIME_MODE_DYNAMIC = 2,
		AIRTIME_MODE_LIMIT = 3,
		__AIRTIME_MODE_MAX,
	} airtime_mode;
	unsigned int airtime_update_interval;
#define AIRTIME_MODE_MAX (__AIRTIME_MODE_MAX - 1)
#endif /* CONFIG_AIRTIME_POLICY */

	int ieee80211be;
#ifdef CONFIG_IEEE80211BE
	enum oper_chan_width eht_oper_chwidth;
	u8 eht_oper_centr_freq_seg0_idx;
	struct eht_phy_capabilities_info eht_phy_capab;
	u16 punct_bitmap; /* a bitmap of disabled 20 MHz channels */
	u8 punct_acs_threshold;
	u8 eht_default_pe_duration;
	u8 eht_bw320_offset;
#endif /* CONFIG_IEEE80211BE */

	/* EHT enable/disable config from CHAN_SWITCH */
#define CH_SWITCH_EHT_ENABLED BIT(0)
#define CH_SWITCH_EHT_DISABLED BIT(1)
	unsigned int ch_switch_eht_config;

	enum mbssid {
		MBSSID_DISABLED = 0,
		MBSSID_ENABLED = 1,
		ENHANCED_MBSSID_ENABLED = 2,
	} mbssid;

	/* Whether to enable TWT responder in HT and VHT modes */
	bool ht_vht_twt_responder;
};


static inline enum oper_chan_width
hostapd_get_oper_chwidth(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		return conf->eht_oper_chwidth;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		return conf->he_oper_chwidth;
#endif /* CONFIG_IEEE80211AX */
	return conf->vht_oper_chwidth;
}

static inline void
hostapd_set_oper_chwidth(struct hostapd_config *conf,
			 enum oper_chan_width oper_chwidth)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		conf->eht_oper_chwidth = oper_chwidth;
	if (oper_chwidth == CONF_OPER_CHWIDTH_320MHZ)
		oper_chwidth = CONF_OPER_CHWIDTH_160MHZ;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		conf->he_oper_chwidth = oper_chwidth;
#endif /* CONFIG_IEEE80211AX */
	conf->vht_oper_chwidth = oper_chwidth;
}

static inline u8
hostapd_get_oper_centr_freq_seg0_idx(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		return conf->eht_oper_centr_freq_seg0_idx;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		return conf->he_oper_centr_freq_seg0_idx;
#endif /* CONFIG_IEEE80211AX */
	return conf->vht_oper_centr_freq_seg0_idx;
}

static inline void
hostapd_set_oper_centr_freq_seg0_idx(struct hostapd_config *conf,
				     u8 oper_centr_freq_seg0_idx)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be)
		conf->eht_oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
	if (is_6ghz_op_class(conf->op_class) &&
	    center_idx_to_bw_6ghz(oper_centr_freq_seg0_idx) == 4)
		oper_centr_freq_seg0_idx +=
			conf->channel > oper_centr_freq_seg0_idx ? 16 : -16;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		conf->he_oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
#endif /* CONFIG_IEEE80211AX */
	conf->vht_oper_centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
}

static inline u8
hostapd_get_oper_centr_freq_seg1_idx(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		return conf->he_oper_centr_freq_seg1_idx;
#endif /* CONFIG_IEEE80211AX */
	return conf->vht_oper_centr_freq_seg1_idx;
}

static inline void
hostapd_set_oper_centr_freq_seg1_idx(struct hostapd_config *conf,
				     u8 oper_centr_freq_seg1_idx)
{
#ifdef CONFIG_IEEE80211AX
	if (conf->ieee80211ax)
		conf->he_oper_centr_freq_seg1_idx = oper_centr_freq_seg1_idx;
#endif /* CONFIG_IEEE80211AX */
	conf->vht_oper_centr_freq_seg1_idx = oper_centr_freq_seg1_idx;
}

static inline u8
hostapd_get_bw320_offset(struct hostapd_config *conf)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be && is_6ghz_op_class(conf->op_class) &&
	    hostapd_get_oper_chwidth(conf) == CONF_OPER_CHWIDTH_320MHZ)
		return conf->eht_bw320_offset;
#endif /* CONFIG_IEEE80211BE */
	return 0;
}

static inline void
hostapd_set_and_check_bw320_offset(struct hostapd_config *conf,
				   u8 bw320_offset)
{
#ifdef CONFIG_IEEE80211BE
	if (conf->ieee80211be && is_6ghz_op_class(conf->op_class) &&
	    op_class_to_ch_width(conf->op_class) == CONF_OPER_CHWIDTH_320MHZ) {
		if (conf->channel) {
			/* If the channel is set, then calculate bw320_offset
			 * by center frequency segment 0.
			 */
			u8 seg0 = hostapd_get_oper_centr_freq_seg0_idx(conf);

			conf->eht_bw320_offset = (seg0 - 31) % 64 ? 2 : 1;
		} else {
			/* If the channel is not set, bw320_offset indicates
			 * preferred offset of 320 MHz.
			 */
			conf->eht_bw320_offset = bw320_offset;
		}
	} else {
		conf->eht_bw320_offset = 0;
	}
#endif /* CONFIG_IEEE80211BE */
}


int hostapd_mac_comp(const void *a, const void *b);
struct hostapd_config * hostapd_config_defaults(void);
void hostapd_config_defaults_bss(struct hostapd_bss_config *bss);
void hostapd_config_free_radius_attr(struct hostapd_radius_attr *attr);
void hostapd_config_free_eap_user(struct hostapd_eap_user *user);
void hostapd_config_free_eap_users(struct hostapd_eap_user *user);
void hostapd_config_clear_wpa_psk(struct hostapd_wpa_psk **p);
void hostapd_config_clear_rxkhs(struct hostapd_bss_config *conf);
void hostapd_config_free_bss(struct hostapd_bss_config *conf);
void hostapd_config_free(struct hostapd_config *conf);
int hostapd_maclist_found(struct mac_acl_entry *list, int num_entries,
			  const u8 *addr, struct vlan_description *vlan_id);
int hostapd_rate_found(int *list, int rate);
const u8 * hostapd_get_psk(const struct hostapd_bss_config *conf,
			   const u8 *addr, const u8 *p2p_dev_addr,
			   const u8 *prev_psk, int *vlan_id);
int hostapd_setup_wpa_psk(struct hostapd_bss_config *conf);
int hostapd_vlan_valid(struct hostapd_vlan *vlan,
		       struct vlan_description *vlan_desc);
const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan,
					int vlan_id);
struct hostapd_radius_attr *
hostapd_config_get_radius_attr(struct hostapd_radius_attr *attr, u8 type);
struct hostapd_radius_attr * hostapd_parse_radius_attr(const char *value);
int hostapd_config_check(struct hostapd_config *conf, int full_config);
void hostapd_set_security_params(struct hostapd_bss_config *bss,
				 int full_config);
int hostapd_sae_pw_id_in_use(struct hostapd_bss_config *conf);
bool hostapd_sae_pk_in_use(struct hostapd_bss_config *conf);
bool hostapd_sae_pk_exclusively(struct hostapd_bss_config *conf);
int hostapd_setup_sae_pt(struct hostapd_bss_config *conf);
int hostapd_acl_comp(const void *a, const void *b);
int hostapd_add_acl_maclist(struct mac_acl_entry **acl, int *num,
			    int vlan_id, const u8 *addr);
void hostapd_remove_acl_mac(struct mac_acl_entry **acl, int *num,
			    const u8 *addr);

#endif /* HOSTAPD_CONFIG_H */
