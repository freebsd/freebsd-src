/*
 * WPA Supplicant / Network configuration structures
 * Copyright (c) 2003-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef CONFIG_SSID_H
#define CONFIG_SSID_H

#include "common/defs.h"
#include "utils/list.h"
#include "eap_peer/eap_config.h"


#define DEFAULT_EAP_WORKAROUND ((unsigned int) -1)
#define DEFAULT_EAPOL_FLAGS (EAPOL_FLAG_REQUIRE_KEY_UNICAST | \
			     EAPOL_FLAG_REQUIRE_KEY_BROADCAST)
#define DEFAULT_PROTO (WPA_PROTO_WPA | WPA_PROTO_RSN)
#define DEFAULT_KEY_MGMT (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_IEEE8021X)
#ifdef CONFIG_NO_TKIP
#define DEFAULT_PAIRWISE (WPA_CIPHER_CCMP)
#define DEFAULT_GROUP (WPA_CIPHER_CCMP)
#else /* CONFIG_NO_TKIP */
#define DEFAULT_PAIRWISE (WPA_CIPHER_CCMP | WPA_CIPHER_TKIP)
#define DEFAULT_GROUP (WPA_CIPHER_CCMP | WPA_CIPHER_TKIP)
#endif /* CONFIG_NO_TKIP */
#define DEFAULT_FRAGMENT_SIZE 1398

#define DEFAULT_BG_SCAN_PERIOD -1
#define DEFAULT_MESH_MAX_RETRIES 2
#define DEFAULT_MESH_RETRY_TIMEOUT 40
#define DEFAULT_MESH_CONFIRM_TIMEOUT 40
#define DEFAULT_MESH_HOLDING_TIMEOUT 40
#define DEFAULT_MESH_RSSI_THRESHOLD 1 /* no change */
#define DEFAULT_DISABLE_HT 0
#define DEFAULT_DISABLE_HT40 0
#define DEFAULT_DISABLE_SGI 0
#define DEFAULT_DISABLE_LDPC 0
#define DEFAULT_TX_STBC -1 /* no change */
#define DEFAULT_RX_STBC -1 /* no change */
#define DEFAULT_DISABLE_MAX_AMSDU -1 /* no change */
#define DEFAULT_AMPDU_FACTOR -1 /* no change */
#define DEFAULT_AMPDU_DENSITY -1 /* no change */
#define DEFAULT_USER_SELECTED_SIM 1
#define DEFAULT_MAX_OPER_CHWIDTH -1

/* Consider global sae_pwe for SAE mechanism for PWE derivation */
#define DEFAULT_SAE_PWE SAE_PWE_NOT_SET

struct psk_list_entry {
	struct dl_list list;
	u8 addr[ETH_ALEN];
	u8 psk[32];
	u8 p2p;
};

enum wpas_mode {
	WPAS_MODE_INFRA = 0,
	WPAS_MODE_IBSS = 1,
	WPAS_MODE_AP = 2,
	WPAS_MODE_P2P_GO = 3,
	WPAS_MODE_P2P_GROUP_FORMATION = 4,
	WPAS_MODE_MESH = 5,
};

enum sae_pk_mode {
	SAE_PK_MODE_AUTOMATIC = 0,
	SAE_PK_MODE_ONLY = 1,
	SAE_PK_MODE_DISABLED = 2,
};

enum wpas_mac_addr_style {
	WPAS_MAC_ADDR_STYLE_NOT_SET = -1,
	WPAS_MAC_ADDR_STYLE_PERMANENT = 0,
	WPAS_MAC_ADDR_STYLE_RANDOM = 1,
	WPAS_MAC_ADDR_STYLE_RANDOM_SAME_OUI = 2,
	WPAS_MAC_ADDR_STYLE_DEDICATED_PER_ESS = 3,
};

/**
 * struct wpa_ssid - Network configuration data
 *
 * This structure includes all the configuration variables for a network. This
 * data is included in the per-interface configuration data as an element of
 * the network list, struct wpa_config::ssid. Each network block in the
 * configuration is mapped to a struct wpa_ssid instance.
 */
struct wpa_ssid {
	/**
	 * next - Next network in global list
	 *
	 * This pointer can be used to iterate over all networks. The head of
	 * this list is stored in the ssid field of struct wpa_config.
	 */
	struct wpa_ssid *next;

	/**
	 * pnext - Next network in per-priority list
	 *
	 * This pointer can be used to iterate over all networks in the same
	 * priority class. The heads of these list are stored in the pssid
	 * fields of struct wpa_config.
	 */
	struct wpa_ssid *pnext;

	/**
	 * id - Unique id for the network
	 *
	 * This identifier is used as a unique identifier for each network
	 * block when using the control interface. Each network is allocated an
	 * id when it is being created, either when reading the configuration
	 * file or when a new network is added through the control interface.
	 */
	int id;

	/**
	 * ro - Whether a network is declared as read-only
	 *
	 * Every network which is defined in a config file that is passed to
	 * wpa_supplicant using the -I option will be marked as read-only
	 * using this flag. It has the effect that it won't be written to
	 * /etc/wpa_supplicant.conf (from -c argument) if, e.g., wpa_gui tells
	 * the daemon to save all changed configs.
	 *
	 * This is necessary because networks from /etc/wpa_supplicant.conf
	 * have a higher priority and changes from an alternative file would be
	 * silently overwritten without this.
	 */
	bool ro;

	/**
	 * priority - Priority group
	 *
	 * By default, all networks will get same priority group (0). If some
	 * of the networks are more desirable, this field can be used to change
	 * the order in which wpa_supplicant goes through the networks when
	 * selecting a BSS. The priority groups will be iterated in decreasing
	 * priority (i.e., the larger the priority value, the sooner the
	 * network is matched against the scan results). Within each priority
	 * group, networks will be selected based on security policy, signal
	 * strength, etc.
	 *
	 * Please note that AP scanning with scan_ssid=1 and ap_scan=2 mode are
	 * not using this priority to select the order for scanning. Instead,
	 * they try the networks in the order that used in the configuration
	 * file.
	 */
	int priority;

	/**
	 * ssid - Service set identifier (network name)
	 *
	 * This is the SSID for the network. For wireless interfaces, this is
	 * used to select which network will be used. If set to %NULL (or
	 * ssid_len=0), any SSID can be used. For wired interfaces, this must
	 * be set to %NULL. Note: SSID may contain any characters, even nul
	 * (ASCII 0) and as such, this should not be assumed to be a nul
	 * terminated string. ssid_len defines how many characters are valid
	 * and the ssid field is not guaranteed to be nul terminated.
	 */
	u8 *ssid;

	/**
	 * ssid_len - Length of the SSID
	 */
	size_t ssid_len;

	/**
	 * bssid - BSSID
	 *
	 * If set, this network block is used only when associating with the AP
	 * using the configured BSSID
	 *
	 * If this is a persistent P2P group (disabled == 2), this is the GO
	 * Device Address.
	 */
	u8 bssid[ETH_ALEN];

	/**
	 * bssid_ignore - List of inacceptable BSSIDs
	 */
	u8 *bssid_ignore;
	size_t num_bssid_ignore;

	/**
	 * bssid_accept - List of acceptable BSSIDs
	 */
	u8 *bssid_accept;
	size_t num_bssid_accept;

	/**
	 * bssid_set - Whether BSSID is configured for this network
	 */
	int bssid_set;

	/**
	 * bssid_hint - BSSID hint
	 *
	 * If set, this is configured to the driver as a preferred initial BSSID
	 * while connecting to this network.
	 */
	u8 bssid_hint[ETH_ALEN];

	/**
	 * bssid_hint_set - Whether BSSID hint is configured for this network
	 */
	int bssid_hint_set;

	/**
	 * go_p2p_dev_addr - GO's P2P Device Address or all zeros if not set
	 */
	u8 go_p2p_dev_addr[ETH_ALEN];

	/**
	 * psk - WPA pre-shared key (256 bits)
	 */
	u8 psk[32];

	/**
	 * psk_set - Whether PSK field is configured
	 */
	int psk_set;

	/**
	 * passphrase - WPA ASCII passphrase
	 *
	 * If this is set, psk will be generated using the SSID and passphrase
	 * configured for the network. ASCII passphrase must be between 8 and
	 * 63 characters (inclusive).
	 */
	char *passphrase;

	/**
	 * sae_password - SAE password
	 *
	 * This parameter can be used to set a password for SAE. By default, the
	 * passphrase value is used if this separate parameter is not used, but
	 * passphrase follows the WPA-PSK constraints (8..63 characters) even
	 * though SAE passwords do not have such constraints.
	 */
	char *sae_password;

	/**
	 * sae_password_id - SAE password identifier
	 *
	 * This parameter can be used to identify a specific SAE password. If
	 * not included, the default SAE password is used instead.
	 */
	char *sae_password_id;

	struct sae_pt *pt;

	/**
	 * ext_psk - PSK/passphrase name in external storage
	 *
	 * If this is set, PSK/passphrase will be fetched from external storage
	 * when requesting association with the network.
	 */
	char *ext_psk;

	/**
	 * mem_only_psk - Whether to keep PSK/passphrase only in memory
	 *
	 * 0 = allow psk/passphrase to be stored to the configuration file
	 * 1 = do not store psk/passphrase to the configuration file
	 */
	int mem_only_psk;

	/**
	 * pairwise_cipher - Bitfield of allowed pairwise ciphers, WPA_CIPHER_*
	 */
	int pairwise_cipher;

	/**
	 * group_cipher - Bitfield of allowed group ciphers, WPA_CIPHER_*
	 */
	int group_cipher;

	/**
	 * group_mgmt_cipher - Bitfield of allowed group management ciphers
	 *
	 * This is a bitfield of WPA_CIPHER_AES_128_CMAC and WPA_CIPHER_BIP_*
	 * values. If 0, no constraint is used for the cipher, i.e., whatever
	 * the AP uses is accepted.
	 */
	int group_mgmt_cipher;

	/**
	 * key_mgmt - Bitfield of allowed key management protocols
	 *
	 * WPA_KEY_MGMT_*
	 */
	int key_mgmt;

	/**
	 * bg_scan_period - Background scan period in seconds, 0 to disable, or
	 * -1 to indicate no change to default driver configuration
	 */
	int bg_scan_period;

	/**
	 * proto - Bitfield of allowed protocols, WPA_PROTO_*
	 */
	int proto;

	/**
	 * auth_alg -  Bitfield of allowed authentication algorithms
	 *
	 * WPA_AUTH_ALG_*
	 */
	int auth_alg;

	/**
	 * scan_ssid - Scan this SSID with Probe Requests
	 *
	 * scan_ssid can be used to scan for APs using hidden SSIDs.
	 * Note: Many drivers do not support this. ap_mode=2 can be used with
	 * such drivers to use hidden SSIDs. Note2: Most nl80211-based drivers
	 * do support scan_ssid=1 and that should be used with them instead of
	 * ap_scan=2.
	 */
	int scan_ssid;

#ifdef IEEE8021X_EAPOL
#define EAPOL_FLAG_REQUIRE_KEY_UNICAST BIT(0)
#define EAPOL_FLAG_REQUIRE_KEY_BROADCAST BIT(1)
	/**
	 * eapol_flags - Bit field of IEEE 802.1X/EAPOL options (EAPOL_FLAG_*)
	 */
	int eapol_flags;

	/**
	 * eap - EAP peer configuration for this network
	 */
	struct eap_peer_config eap;
#endif /* IEEE8021X_EAPOL */

#ifdef CONFIG_WEP
#define NUM_WEP_KEYS 4
#define MAX_WEP_KEY_LEN 16
	/**
	 * wep_key - WEP keys
	 */
	u8 wep_key[NUM_WEP_KEYS][MAX_WEP_KEY_LEN];

	/**
	 * wep_key_len - WEP key lengths
	 */
	size_t wep_key_len[NUM_WEP_KEYS];

	/**
	 * wep_tx_keyidx - Default key index for TX frames using WEP
	 */
	int wep_tx_keyidx;
#endif /* CONFIG_WEP */

	/**
	 * proactive_key_caching - Enable proactive key caching
	 *
	 * This field can be used to enable proactive key caching which is also
	 * known as opportunistic PMKSA caching for WPA2. This is disabled (0)
	 * by default unless default value is changed with the global okc=1
	 * parameter. Enable by setting this to 1.
	 *
	 * Proactive key caching is used to make supplicant assume that the APs
	 * are using the same PMK and generate PMKSA cache entries without
	 * doing RSN pre-authentication. This requires support from the AP side
	 * and is normally used with wireless switches that co-locate the
	 * authenticator.
	 *
	 * Internally, special value -1 is used to indicate that the parameter
	 * was not specified in the configuration (i.e., default behavior is
	 * followed).
	 */
	int proactive_key_caching;

	/**
	 * mixed_cell - Whether mixed cells are allowed
	 *
	 * This option can be used to configure whether so called mixed cells,
	 * i.e., networks that use both plaintext and encryption in the same
	 * SSID, are allowed. This is disabled (0) by default. Enable by
	 * setting this to 1.
	 */
	int mixed_cell;

#ifdef IEEE8021X_EAPOL

	/**
	 * leap - Number of EAP methods using LEAP
	 *
	 * This field should be set to 1 if LEAP is enabled. This is used to
	 * select IEEE 802.11 authentication algorithm.
	 */
	int leap;

	/**
	 * non_leap - Number of EAP methods not using LEAP
	 *
	 * This field should be set to >0 if any EAP method other than LEAP is
	 * enabled. This is used to select IEEE 802.11 authentication
	 * algorithm.
	 */
	int non_leap;

	/**
	 * eap_workaround - EAP workarounds enabled
	 *
	 * wpa_supplicant supports number of "EAP workarounds" to work around
	 * interoperability issues with incorrectly behaving authentication
	 * servers. This is recommended to be enabled by default because some
	 * of the issues are present in large number of authentication servers.
	 *
	 * Strict EAP conformance mode can be configured by disabling
	 * workarounds with eap_workaround = 0.
	 */
	unsigned int eap_workaround;

#endif /* IEEE8021X_EAPOL */

	/**
	 * mode - IEEE 802.11 operation mode (Infrastucture/IBSS)
	 *
	 * 0 = infrastructure (Managed) mode, i.e., associate with an AP.
	 *
	 * 1 = IBSS (ad-hoc, peer-to-peer)
	 *
	 * 2 = AP (access point)
	 *
	 * 3 = P2P Group Owner (can be set in the configuration file)
	 *
	 * 4 = P2P Group Formation (used internally; not in configuration
	 * files)
	 *
	 * 5 = Mesh
	 *
	 * Note: IBSS can only be used with key_mgmt NONE (plaintext and static
	 * WEP) and WPA-PSK (with proto=RSN). In addition, key_mgmt=WPA-NONE
	 * (fixed group key TKIP/CCMP) is available for backwards compatibility,
	 * but its use is deprecated. WPA-None requires following network block
	 * options: proto=WPA, key_mgmt=WPA-NONE, pairwise=NONE, group=TKIP (or
	 * CCMP, but not both), and psk must also be set (either directly or
	 * using ASCII passphrase).
	 */
	enum wpas_mode mode;

	/**
	 * pbss - Whether to use PBSS. Relevant to DMG networks only.
	 * 0 = do not use PBSS
	 * 1 = use PBSS
	 * 2 = don't care (not allowed in AP mode)
	 * Used together with mode configuration. When mode is AP, it
	 * means to start a PCP instead of a regular AP. When mode is INFRA it
	 * means connect to a PCP instead of AP. In this mode you can also
	 * specify 2 (don't care) meaning connect to either AP or PCP.
	 * P2P_GO and P2P_GROUP_FORMATION modes must use PBSS in DMG network.
	 */
	int pbss;

	/**
	 * disabled - Whether this network is currently disabled
	 *
	 * 0 = this network can be used (default).
	 * 1 = this network block is disabled (can be enabled through
	 * ctrl_iface, e.g., with wpa_cli or wpa_gui).
	 * 2 = this network block includes parameters for a persistent P2P
	 * group (can be used with P2P ctrl_iface commands)
	 */
	int disabled;

	/**
	 * disabled_for_connect - Whether this network was temporarily disabled
	 *
	 * This flag is used to reenable all the temporarily disabled networks
	 * after either the success or failure of a WPS connection.
	 */
	int disabled_for_connect;

	/**
	 * id_str - Network identifier string for external scripts
	 *
	 * This value is passed to external ctrl_iface monitors in
	 * WPA_EVENT_CONNECTED event and wpa_cli sets this as WPA_ID_STR
	 * environment variable for action scripts.
	 */
	char *id_str;

	/**
	 * ieee80211w - Whether management frame protection is enabled
	 *
	 * This value is used to configure policy for management frame
	 * protection (IEEE 802.11w). 0 = disabled, 1 = optional, 2 = required.
	 * This is disabled by default unless the default value has been changed
	 * with the global pmf=1/2 parameter.
	 *
	 * Internally, special value 3 is used to indicate that the parameter
	 * was not specified in the configuration (i.e., default behavior is
	 * followed).
	 */
	enum mfp_options ieee80211w;

#ifdef CONFIG_OCV
	/**
	 * ocv - Enable/disable operating channel validation
	 *
	 * If this parameter is set to 1, stations will exchange OCI element
	 * to cryptographically verify the operating channel. Setting this
	 * parameter to 0 disables this option. Default value: 0.
	 */
	int ocv;
#endif /* CONFIG_OCV */

	/**
	 * frequency - Channel frequency in megahertz (MHz) for IBSS
	 *
	 * This value is used to configure the initial channel for IBSS (adhoc)
	 * networks, e.g., 2412 = IEEE 802.11b/g channel 1. It is ignored in
	 * the infrastructure mode. In addition, this value is only used by the
	 * station that creates the IBSS. If an IBSS network with the
	 * configured SSID is already present, the frequency of the network
	 * will be used instead of this configured value.
	 */
	int frequency;

	/**
	 * enable_edmg - Enable EDMG feature in STA/AP mode
	 *
	 * This flag is used for enabling the EDMG capability in STA/AP mode.
	 */
	int enable_edmg;

	/**
	 * edmg_channel - EDMG channel number
	 *
	 * This value is used to configure the EDMG channel bonding feature.
	 * In AP mode it defines the EDMG channel to start the AP on.
	 * in STA mode it defines the EDMG channel to use for connection
	 * (if supported by AP).
	 */
	u8 edmg_channel;

	/**
	 * fixed_freq - Use fixed frequency for IBSS
	 */
	int fixed_freq;

#ifdef CONFIG_ACS
	/**
	 * ACS - Automatic Channel Selection for AP mode
	 *
	 * If present, it will be handled together with frequency.
	 * frequency will be used to determine hardware mode only, when it is
	 * used for both hardware mode and channel when used alone. This will
	 * force the channel to be set to 0, thus enabling ACS.
	 */
	int acs;
#endif /* CONFIG_ACS */

	/**
	 * mesh_basic_rates - BSS Basic rate set for mesh network
	 *
	 */
	int *mesh_basic_rates;

	/**
	 * Mesh network plink parameters
	 */
	int dot11MeshMaxRetries;
	int dot11MeshRetryTimeout; /* msec */
	int dot11MeshConfirmTimeout; /* msec */
	int dot11MeshHoldingTimeout; /* msec */

	/**
	 * Mesh network layer-2 forwarding (dot11MeshForwarding)
	 */
	int mesh_fwding;

	int ht;
	int ht40;

	int vht;

	int he;

	int eht;

	enum oper_chan_width max_oper_chwidth;

	unsigned int vht_center_freq1;
	unsigned int vht_center_freq2;

	/**
	 * wpa_ptk_rekey - Maximum lifetime for PTK in seconds
	 *
	 * This value can be used to enforce rekeying of PTK to mitigate some
	 * attacks against TKIP deficiencies.
	 */
	int wpa_ptk_rekey;

	/** wpa_deny_ptk0_rekey - Control PTK0 rekeying
	 *
	 * Rekeying a pairwise key using only keyid 0 (PTK0 rekey) has many
	 * broken implementations and should be avoided when using or
	 * interacting with one.
	 *
	 * 0 = always rekey when configured/instructed
	 * 1 = only rekey when the local driver is explicitly indicating it can
	 *	perform this operation without issues
	 * 2 = never allow PTK0 rekeys
	 */
	enum ptk0_rekey_handling wpa_deny_ptk0_rekey;

	/**
	 * group_rekey - Group rekeying time in seconds
	 *
	 * This value, if non-zero, is used as the dot11RSNAConfigGroupRekeyTime
	 * parameter when operating in Authenticator role in IBSS.
	 */
	int group_rekey;

	/**
	 * scan_freq - Array of frequencies to scan or %NULL for all
	 *
	 * This is an optional zero-terminated array of frequencies in
	 * megahertz (MHz) to include in scan requests when searching for this
	 * network. This can be used to speed up scanning when the network is
	 * known to not use all possible channels.
	 */
	int *scan_freq;

	/**
	 * bgscan - Background scan and roaming parameters or %NULL if none
	 *
	 * This is an optional set of parameters for background scanning and
	 * roaming within a network (ESS) in following format:
	 * <bgscan module name>:<module parameters>
	 */
	char *bgscan;

	/**
	 * ignore_broadcast_ssid - Hide SSID in AP mode
	 *
	 * Send empty SSID in beacons and ignore probe request frames that do
	 * not specify full SSID, i.e., require stations to know SSID.
	 * default: disabled (0)
	 * 1 = send empty (length=0) SSID in beacon and ignore probe request
	 * for broadcast SSID
	 * 2 = clear SSID (ASCII 0), but keep the original length (this may be
	 * required with some clients that do not support empty SSID) and
	 * ignore probe requests for broadcast SSID
	 */
	int ignore_broadcast_ssid;

	/**
	 * freq_list - Array of allowed frequencies or %NULL for all
	 *
	 * This is an optional zero-terminated array of frequencies in
	 * megahertz (MHz) to allow for selecting the BSS. If set, scan results
	 * that do not match any of the specified frequencies are not
	 * considered when selecting a BSS.
	 */
	int *freq_list;

	/**
	 * p2p_client_list - List of P2P Clients in a persistent group (GO)
	 *
	 * This is a list of P2P Clients (P2P Device Address) that have joined
	 * the persistent group. This is maintained on the GO for persistent
	 * group entries (disabled == 2).
	 */
	u8 *p2p_client_list;

	/**
	 * num_p2p_clients - Number of entries in p2p_client_list
	 */
	size_t num_p2p_clients;

#ifndef P2P_MAX_STORED_CLIENTS
#define P2P_MAX_STORED_CLIENTS 100
#endif /* P2P_MAX_STORED_CLIENTS */

	/**
	 * psk_list - Per-client PSKs (struct psk_list_entry)
	 */
	struct dl_list psk_list;

	/**
	 * p2p_group - Network generated as a P2P group (used internally)
	 */
	int p2p_group;

	/**
	 * p2p_persistent_group - Whether this is a persistent group
	 */
	int p2p_persistent_group;

	/**
	 * temporary - Whether this network is temporary and not to be saved
	 */
	int temporary;

	/**
	 * export_keys - Whether keys may be exported
	 *
	 * This attribute will be set when keys are determined through
	 * WPS or similar so that they may be exported.
	 */
	int export_keys;

#ifdef CONFIG_HT_OVERRIDES
	/**
	 * disable_ht - Disable HT (IEEE 802.11n) for this network
	 *
	 * By default, use it if it is available, but this can be configured
	 * to 1 to have it disabled.
	 */
	int disable_ht;

	/**
	 * disable_ht40 - Disable HT40 for this network
	 *
	 * By default, use it if it is available, but this can be configured
	 * to 1 to have it disabled.
	 */
	int disable_ht40;

	/**
	 * disable_sgi - Disable SGI (Short Guard Interval) for this network
	 *
	 * By default, use it if it is available, but this can be configured
	 * to 1 to have it disabled.
	 */
	int disable_sgi;

	/**
	 * disable_ldpc - Disable LDPC for this network
	 *
	 * By default, use it if it is available, but this can be configured
	 * to 1 to have it disabled.
	 */
	int disable_ldpc;

	/**
	 * ht40_intolerant - Indicate 40 MHz intolerant for this network
	 */
	int ht40_intolerant;

	/**
	 * disable_max_amsdu - Disable MAX A-MSDU
	 *
	 * A-MDSU will be 3839 bytes when disabled, or 7935
	 * when enabled (assuming it is otherwise supported)
	 * -1 (default) means do not apply any settings to the kernel.
	 */
	int disable_max_amsdu;

	/**
	 * ampdu_factor - Maximum A-MPDU Length Exponent
	 *
	 * Value: 0-3, see 7.3.2.56.3 in IEEE Std 802.11n-2009.
	 */
	int ampdu_factor;

	/**
	 * ampdu_density - Minimum A-MPDU Start Spacing
	 *
	 * Value: 0-7, see 7.3.2.56.3 in IEEE Std 802.11n-2009.
	 */
	int ampdu_density;

	/**
	 * ht_mcs - Allowed HT-MCS rates, in ASCII hex: ffff0000...
	 *
	 * By default (empty string): Use whatever the OS has configured.
	 */
	char *ht_mcs;

	/**
	 * tx_stbc - Indicate STBC support for TX streams
	 *
	 * Value: -1..1, by default (-1): use whatever the OS or card has
	 * configured. See IEEE Std 802.11-2016, 9.4.2.56.2.
	 */
	int tx_stbc;

	/**
	 * rx_stbc - Indicate STBC support for RX streams
	 *
	 * Value: -1..3, by default (-1): use whatever the OS or card has
	 * configured. See IEEE Std 802.11-2016, 9.4.2.56.2.
	 */
	int rx_stbc;
#endif /* CONFIG_HT_OVERRIDES */

#ifdef CONFIG_VHT_OVERRIDES
	/**
	 * disable_vht - Disable VHT (IEEE 802.11ac) for this network
	 *
	 * By default, use it if it is available, but this can be configured
	 * to 1 to have it disabled.
	 */
	int disable_vht;

	/**
	 * vht_capa - VHT capabilities to use
	 */
	unsigned int vht_capa;

	/**
	 * vht_capa_mask - mask for VHT capabilities
	 */
	unsigned int vht_capa_mask;

	int vht_rx_mcs_nss_1, vht_rx_mcs_nss_2,
	    vht_rx_mcs_nss_3, vht_rx_mcs_nss_4,
	    vht_rx_mcs_nss_5, vht_rx_mcs_nss_6,
	    vht_rx_mcs_nss_7, vht_rx_mcs_nss_8;
	int vht_tx_mcs_nss_1, vht_tx_mcs_nss_2,
	    vht_tx_mcs_nss_3, vht_tx_mcs_nss_4,
	    vht_tx_mcs_nss_5, vht_tx_mcs_nss_6,
	    vht_tx_mcs_nss_7, vht_tx_mcs_nss_8;
#endif /* CONFIG_VHT_OVERRIDES */

#ifdef CONFIG_HE_OVERRIDES
	/**
	 * disable_he - Disable HE (IEEE 802.11ax) for this network
	 *
	 * By default, use it if it is available, but this can be configured
	 * to 1 to have it disabled.
	 */
	int disable_he;
#endif /* CONFIG_HE_OVERRIDES */

	/**
	 * ap_max_inactivity - Timeout in seconds to detect STA's inactivity
	 *
	 * This timeout value is used in AP mode to clean up inactive stations.
	 * By default: 300 seconds.
	 */
	int ap_max_inactivity;

	/**
	 * dtim_period - DTIM period in Beacon intervals
	 * By default: 2
	 */
	int dtim_period;

	/**
	 * beacon_int - Beacon interval (default: 100 TU)
	 */
	int beacon_int;

	/**
	 * auth_failures - Number of consecutive authentication failures
	 */
	unsigned int auth_failures;

	/**
	 * disabled_until - Network block disabled until this time if non-zero
	 */
	struct os_reltime disabled_until;

	/**
	 * disabled_due_to - BSSID of the disabling failure
	 *
	 * This identifies the BSS that failed the connection attempt that
	 * resulted in the network being temporarily disabled.
	 */
	u8 disabled_due_to[ETH_ALEN];

	/**
	 * parent_cred - Pointer to parent wpa_cred entry
	 *
	 * This pointer can be used to delete temporary networks when a wpa_cred
	 * that was used to create them is removed. This pointer should not be
	 * dereferences since it may not be updated in all cases.
	 */
	void *parent_cred;

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
	 * macsec_offload - Enable MACsec hardware offload
	 *
	 * This setting applies only when MACsec is in use, i.e.,
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

#ifdef CONFIG_HS20
	int update_identifier;

	/**
	 * roaming_consortium_selection - Roaming Consortium Selection
	 *
	 * The matching Roaming Consortium OI that was used to generate this
	 * network profile.
	 */
	u8 *roaming_consortium_selection;

	/**
	 * roaming_consortium_selection_len - roaming_consortium_selection len
	 */
	size_t roaming_consortium_selection_len;
#endif /* CONFIG_HS20 */

	unsigned int wps_run;

	/**
	 * mac_addr - MAC address policy
	 *
	 * 0 = use permanent MAC address
	 * 1 = use random MAC address for each ESS connection
	 * 2 = like 1, but maintain OUI (with local admin bit set)
	 * 3 = use dedicated/pregenerated MAC address (see mac_value)
	 *
	 * Internally, special value -1 is used to indicate that the parameter
	 * was not specified in the configuration (i.e., default behavior is
	 * followed).
	 */
	enum wpas_mac_addr_style mac_addr;

	/**
	 * mac_value - Specific MAC address to be used
	 *
	 * When mac_addr policy is equal to 3 this is the value of the MAC
	 * address that should be used.
	 */
	u8 mac_value[ETH_ALEN];

	/**
	 * no_auto_peer - Do not automatically peer with compatible mesh peers
	 *
	 * When unset, the reception of a beacon from a another mesh peer in
	 * this MBSS will trigger a peering attempt.
	 */
	int no_auto_peer;

	/**
	 * mesh_rssi_threshold - Set mesh parameter mesh_rssi_threshold (dBm)
	 *
	 * -255..-1 = threshold value in dBm
	 * 0 = not using RSSI threshold
	 * 1 = do not change driver default
	 */
	int mesh_rssi_threshold;

	/**
	 * wps_disabled - WPS disabled in AP mode
	 *
	 * 0 = WPS enabled and configured (default)
	 * 1 = WPS disabled
	 */
	int wps_disabled;

	/**
	 * fils_dh_group - FILS DH Group
	 *
	 * 0 = PFS disabled with FILS shared key authentication
	 * 1-65535 DH Group to use for FILS PFS
	 */
	int fils_dh_group;

	/**
	 * dpp_connector - DPP Connector (signedConnector as string)
	 */
	char *dpp_connector;

	/**
	 * dpp_netaccesskey - DPP netAccessKey (own private key)
	 */
	u8 *dpp_netaccesskey;

	/**
	 * dpp_netaccesskey_len - DPP netAccessKey length in octets
	 */
	size_t dpp_netaccesskey_len;

	/**
	 * net_access_key_expiry - DPP netAccessKey expiry in UNIX time stamp
	 *
	 * 0 indicates no expiration.
	 */
	unsigned int dpp_netaccesskey_expiry;

	/**
	 * dpp_csign - C-sign-key (Configurator public key)
	 */
	u8 *dpp_csign;

	/**
	 * dpp_csign_len - C-sign-key length in octets
	 */
	size_t dpp_csign_len;

	/**
	 * dpp_pp_key - ppKey (Configurator privacy protection public key)
	 */
	u8 *dpp_pp_key;

	/**
	 * dpp_pp_key_len - ppKey length in octets
	 */
	size_t dpp_pp_key_len;

	/**
	 * dpp_pfs - DPP PFS
	 * 0: allow PFS to be used or not used
	 * 1: require PFS to be used (note: not compatible with DPP R1)
	 * 2: do not allow PFS to be used
	 */
	int dpp_pfs;

	/**
	 * dpp_pfs_fallback - DPP PFS fallback selection
	 *
	 * This is an internally used variable (i.e., not used in external
	 * configuration) to track state of the DPP PFS fallback mechanism.
	 */
	int dpp_pfs_fallback;

	/**
	 * dpp_connector_privacy - Network introduction type
	 * 0: unprotected variant from DPP R1
	 * 1: privacy protecting (station Connector encrypted) variant from
	 *    DPP R3
	 */
	int dpp_connector_privacy;

	/**
	 * owe_group - OWE DH Group
	 *
	 * 0 = use default (19) first and then try all supported groups one by
	 *	one if AP rejects the selected group
	 * 1-65535 DH Group to use for OWE
	 *
	 * Groups 19 (NIST P-256), 20 (NIST P-384), and 21 (NIST P-521) are
	 * currently supported.
	 */
	int owe_group;

	/**
	 * owe_only - OWE-only mode (disable transition mode)
	 *
	 * 0 = enable transition mode (allow connection to either OWE or open
	 *	BSS)
	 * 1 = disable transition mode (allow connection only with OWE)
	 */
	int owe_only;

	/**
	 * owe_ptk_workaround - OWE PTK derivation workaround
	 *
	 * Initial OWE implementation used SHA256 when deriving the PTK for all
	 * OWE groups. This was supposed to change to SHA384 for group 20 and
	 * SHA512 for group 21. This parameter can be used to enable older
	 * behavior mainly for testing purposes. There is no impact to group 19
	 * behavior, but if enabled, this will make group 20 and 21 cases use
	 * SHA256-based PTK derivation which will not work with the updated
	 * OWE implementation on the AP side.
	 */
	int owe_ptk_workaround;

	/**
	 * owe_transition_bss_select_count - OWE transition BSS select count
	 *
	 * This is an internally used variable (i.e., not used in external
	 * configuration) to track the number of selection attempts done for
	 * OWE BSS in transition mode. This allows fallback to an open BSS if
	 * the selection attempts for OWE BSS exceed the configured threshold.
	 */
	int owe_transition_bss_select_count;

	/**
	 * multi_ap_backhaul_sta - Multi-AP backhaul STA
	 * 0 = normal (non-Multi-AP) station
	 * 1 = Multi-AP backhaul station
	 */
	int multi_ap_backhaul_sta;

	/**
	 * ft_eap_pmksa_caching - Whether FT-EAP PMKSA caching is allowed
	 * 0 = do not try to use PMKSA caching with FT-EAP
	 * 1 = try to use PMKSA caching with FT-EAP
	 *
	 * This controls whether to try to use PMKSA caching with FT-EAP for the
	 * FT initial mobility domain association.
	 */
	int ft_eap_pmksa_caching;

	/**
	 * multi_ap_profile - Supported Multi-AP profile
	 */
	int multi_ap_profile;

	/**
	 * beacon_prot - Whether Beacon protection is enabled
	 *
	 * This depends on management frame protection (ieee80211w) being
	 * enabled.
	 */
	int beacon_prot;

	/**
	 * transition_disable - Transition Disable indication
	 * The AP can notify authenticated stations to disable transition mode
	 * in their network profiles when the network has completed transition
	 * steps, i.e., once sufficiently large number of APs in the ESS have
	 * been updated to support the more secure alternative. When this
	 * indication is used, the stations are expected to automatically
	 * disable transition mode and less secure security options. This
	 * includes use of WEP, TKIP (including use of TKIP as the group
	 * cipher), and connections without PMF.
	 * Bitmap bits:
	 * bit 0 (0x01): WPA3-Personal (i.e., disable WPA2-Personal = WPA-PSK
	 *	and only allow SAE to be used)
	 * bit 1 (0x02): SAE-PK (disable SAE without use of SAE-PK)
	 * bit 2 (0x04): WPA3-Enterprise (move to requiring PMF)
	 * bit 3 (0x08): Enhanced Open (disable use of open network; require
	 *	OWE)
	 */
	u8 transition_disable;

	/**
	 * sae_pk - SAE-PK mode
	 * 0 = automatic SAE/SAE-PK selection based on password; enable
	 * transition mode (allow SAE authentication without SAE-PK)
	 * 1 = SAE-PK only (disable transition mode; allow SAE authentication
	 * only with SAE-PK)
	 * 2 = disable SAE-PK (allow SAE authentication only without SAE-PK)
	 */
	enum sae_pk_mode sae_pk;

	/**
	 * was_recently_reconfigured - Whether this SSID config has been changed
	 * recently
	 *
	 * This is an internally used variable, i.e., not used in external
	 * configuration.
	 */
	bool was_recently_reconfigured;

	/**
	 * sae_pwe - SAE mechanism for PWE derivation
	 *
	 * Internally, special value 4 (DEFAULT_SAE_PWE) is used to indicate
	 * that the parameter is not set and the global sae_pwe value needs to
	 * be considered.
	 *
	 * 0 = hunting-and-pecking loop only
	 * 1 = hash-to-element only
	 * 2 = both hunting-and-pecking loop and hash-to-element enabled
	 */
	enum sae_pwe sae_pwe;

	/**
	 * disable_eht - Disable EHT (IEEE 802.11be) for this network
	 *
	 * By default, use it if it is available, but this can be configured
	 * to 1 to have it disabled.
	 */
	int disable_eht;

	/**
	 * enable_4addr_mode - Set 4addr mode after association
	 * 0 = Do not attempt to set 4addr mode
	 * 1 = Try to set 4addr mode after association
	 *
	 * Linux requires that an interface is set to 4addr mode before it can
	 * be added to a bridge. Set this to 1 for networks where you intent
	 * to use the interface in a bridge.
	 */
	int enable_4addr_mode;

	/**
	 * max_idle - BSS max idle period to request
	 *
	 * If nonzero, request the specified number of 1000 TU (i.e., 1.024 s)
	 * as the maximum idle period for the STA during association.
	 */
	int max_idle;

	/**
	 * ssid_protection - Whether to use SSID protection in 4-way handshake
	 */
	bool ssid_protection;
};

#endif /* CONFIG_SSID_H */
