/*
 * WPA Supplicant / Network configuration structures
 * Copyright (c) 2003-2008, Jouni Malinen <j@w1.fi>
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

#ifndef CONFIG_SSID_H
#define CONFIG_SSID_H

#include "common/defs.h"
#include "eap_peer/eap_config.h"

#define MAX_SSID_LEN 32


#define DEFAULT_EAP_WORKAROUND ((unsigned int) -1)
#define DEFAULT_EAPOL_FLAGS (EAPOL_FLAG_REQUIRE_KEY_UNICAST | \
			     EAPOL_FLAG_REQUIRE_KEY_BROADCAST)
#define DEFAULT_PROTO (WPA_PROTO_WPA | WPA_PROTO_RSN)
#define DEFAULT_KEY_MGMT (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_IEEE8021X)
#define DEFAULT_PAIRWISE (WPA_CIPHER_CCMP | WPA_CIPHER_TKIP)
#define DEFAULT_GROUP (WPA_CIPHER_CCMP | WPA_CIPHER_TKIP | \
		       WPA_CIPHER_WEP104 | WPA_CIPHER_WEP40)
#define DEFAULT_FRAGMENT_SIZE 1398

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
	 */
	u8 bssid[ETH_ALEN];

	/**
	 * bssid_set - Whether BSSID is configured for this network
	 */
	int bssid_set;

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
	 * pairwise_cipher - Bitfield of allowed pairwise ciphers, WPA_CIPHER_*
	 */
	int pairwise_cipher;

	/**
	 * group_cipher - Bitfield of allowed group ciphers, WPA_CIPHER_*
	 */
	int group_cipher;

	/**
	 * key_mgmt - Bitfield of allowed key management protocols
	 *
	 * WPA_KEY_MGMT_*
	 */
	int key_mgmt;

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
	 * such drivers to use hidden SSIDs.
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

	/**
	 * proactive_key_caching - Enable proactive key caching
	 *
	 * This field can be used to enable proactive key caching which is also
	 * known as opportunistic PMKSA caching for WPA2. This is disabled (0)
	 * by default. Enable by setting this to 1.
	 *
	 * Proactive key caching is used to make supplicant assume that the APs
	 * are using the same PMK and generate PMKSA cache entries without
	 * doing RSN pre-authentication. This requires support from the AP side
	 * and is normally used with wireless switches that co-locate the
	 * authenticator.
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
	 * Note: IBSS can only be used with key_mgmt NONE (plaintext and
	 * static WEP) and key_mgmt=WPA-NONE (fixed group key TKIP/CCMP). In
	 * addition, ap_scan has to be set to 2 for IBSS. WPA-None requires
	 * following network block options: proto=WPA, key_mgmt=WPA-NONE,
	 * pairwise=NONE, group=TKIP (or CCMP, but not both), and psk must also
	 * be set (either directly or using ASCII passphrase).
	 */
	enum wpas_mode {
		WPAS_MODE_INFRA = 0,
		WPAS_MODE_IBSS = 1,
		WPAS_MODE_AP = 2,
	} mode;

	/**
	 * disabled - Whether this network is currently disabled
	 *
	 * 0 = this network can be used (default).
	 * 1 = this network block is disabled (can be enabled through
	 * ctrl_iface, e.g., with wpa_cli or wpa_gui).
	 */
	int disabled;

	/**
	 * peerkey -  Whether PeerKey handshake for direct links is allowed
	 *
	 * This is only used when both RSN/WPA2 and IEEE 802.11e (QoS) are
	 * enabled.
	 *
	 * 0 = disabled (default)
	 * 1 = enabled
	 */
	int peerkey;

	/**
	 * id_str - Network identifier string for external scripts
	 *
	 * This value is passed to external ctrl_iface monitors in
	 * WPA_EVENT_CONNECTED event and wpa_cli sets this as WPA_ID_STR
	 * environment variable for action scripts.
	 */
	char *id_str;

#ifdef CONFIG_IEEE80211W
	/**
	 * ieee80211w - Whether management frame protection is enabled
	 *
	 * This value is used to configure policy for management frame
	 * protection (IEEE 802.11w). 0 = disabled, 1 = optional, 2 = required.
	 */
	enum mfp_options ieee80211w;
#endif /* CONFIG_IEEE80211W */

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
	 * wpa_ptk_rekey - Maximum lifetime for PTK in seconds
	 *
	 * This value can be used to enforce rekeying of PTK to mitigate some
	 * attacks against TKIP deficiencies.
	 */
	int wpa_ptk_rekey;

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
	 * freq_list - Array of allowed frequencies or %NULL for all
	 *
	 * This is an optional zero-terminated array of frequencies in
	 * megahertz (MHz) to allow for selecting the BSS. If set, scan results
	 * that do not match any of the specified frequencies are not
	 * considered when selecting a BSS.
	 */
	int *freq_list;
};

#endif /* CONFIG_SSID_H */
