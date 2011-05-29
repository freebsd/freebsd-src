/*
 * wpa_supplicant - Internal definitions
 * Copyright (c) 2003-2010, Jouni Malinen <j@w1.fi>
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

#ifndef WPA_SUPPLICANT_I_H
#define WPA_SUPPLICANT_I_H

#include "utils/list.h"
#include "common/defs.h"

extern const char *wpa_supplicant_version;
extern const char *wpa_supplicant_license;
#ifndef CONFIG_NO_STDOUT_DEBUG
extern const char *wpa_supplicant_full_license1;
extern const char *wpa_supplicant_full_license2;
extern const char *wpa_supplicant_full_license3;
extern const char *wpa_supplicant_full_license4;
extern const char *wpa_supplicant_full_license5;
#endif /* CONFIG_NO_STDOUT_DEBUG */

struct wpa_sm;
struct wpa_supplicant;
struct ibss_rsn;
struct scan_info;
struct wpa_bss;
struct wpa_scan_results;

/*
 * Forward declarations of private structures used within the ctrl_iface
 * backends. Other parts of wpa_supplicant do not have access to data stored in
 * these structures.
 */
struct ctrl_iface_priv;
struct ctrl_iface_global_priv;
struct wpas_dbus_priv;

/**
 * struct wpa_interface - Parameters for wpa_supplicant_add_iface()
 */
struct wpa_interface {
	/**
	 * confname - Configuration name (file or profile) name
	 *
	 * This can also be %NULL when a configuration file is not used. In
	 * that case, ctrl_interface must be set to allow the interface to be
	 * configured.
	 */
	const char *confname;

	/**
	 * ctrl_interface - Control interface parameter
	 *
	 * If a configuration file is not used, this variable can be used to
	 * set the ctrl_interface parameter that would have otherwise been read
	 * from the configuration file. If both confname and ctrl_interface are
	 * set, ctrl_interface is used to override the value from configuration
	 * file.
	 */
	const char *ctrl_interface;

	/**
	 * driver - Driver interface name, or %NULL to use the default driver
	 */
	const char *driver;

	/**
	 * driver_param - Driver interface parameters
	 *
	 * If a configuration file is not used, this variable can be used to
	 * set the driver_param parameters that would have otherwise been read
	 * from the configuration file. If both confname and driver_param are
	 * set, driver_param is used to override the value from configuration
	 * file.
	 */
	const char *driver_param;

	/**
	 * ifname - Interface name
	 */
	const char *ifname;

	/**
	 * bridge_ifname - Optional bridge interface name
	 *
	 * If the driver interface (ifname) is included in a Linux bridge
	 * device, the bridge interface may need to be used for receiving EAPOL
	 * frames. This can be enabled by setting this variable to enable
	 * receiving of EAPOL frames from an additional interface.
	 */
	const char *bridge_ifname;
};

/**
 * struct wpa_params - Parameters for wpa_supplicant_init()
 */
struct wpa_params {
	/**
	 * daemonize - Run %wpa_supplicant in the background
	 */
	int daemonize;

	/**
	 * wait_for_monitor - Wait for a monitor program before starting
	 */
	int wait_for_monitor;

	/**
	 * pid_file - Path to a PID (process ID) file
	 *
	 * If this and daemonize are set, process ID of the background process
	 * will be written to the specified file.
	 */
	char *pid_file;

	/**
	 * wpa_debug_level - Debugging verbosity level (e.g., MSG_INFO)
	 */
	int wpa_debug_level;

	/**
	 * wpa_debug_show_keys - Whether keying material is included in debug
	 *
	 * This parameter can be used to allow keying material to be included
	 * in debug messages. This is a security risk and this option should
	 * not be enabled in normal configuration. If needed during
	 * development or while troubleshooting, this option can provide more
	 * details for figuring out what is happening.
	 */
	int wpa_debug_show_keys;

	/**
	 * wpa_debug_timestamp - Whether to include timestamp in debug messages
	 */
	int wpa_debug_timestamp;

	/**
	 * ctrl_interface - Global ctrl_iface path/parameter
	 */
	char *ctrl_interface;

	/**
	 * dbus_ctrl_interface - Enable the DBus control interface
	 */
	int dbus_ctrl_interface;

	/**
	 * wpa_debug_file_path - Path of debug file or %NULL to use stdout
	 */
	const char *wpa_debug_file_path;

	/**
	 * wpa_debug_syslog - Enable log output through syslog
	 */
	int wpa_debug_syslog;

	/**
	 * override_driver - Optional driver parameter override
	 *
	 * This parameter can be used to override the driver parameter in
	 * dynamic interface addition to force a specific driver wrapper to be
	 * used instead.
	 */
	char *override_driver;

	/**
	 * override_ctrl_interface - Optional ctrl_interface override
	 *
	 * This parameter can be used to override the ctrl_interface parameter
	 * in dynamic interface addition to force a control interface to be
	 * created.
	 */
	char *override_ctrl_interface;
};

/**
 * struct wpa_global - Internal, global data for all %wpa_supplicant interfaces
 *
 * This structure is initialized by calling wpa_supplicant_init() when starting
 * %wpa_supplicant.
 */
struct wpa_global {
	struct wpa_supplicant *ifaces;
	struct wpa_params params;
	struct ctrl_iface_global_priv *ctrl_iface;
	struct wpas_dbus_priv *dbus;
	void **drv_priv;
	size_t drv_count;
	struct os_time suspend_time;
};


struct wpa_client_mlme {
#ifdef CONFIG_CLIENT_MLME
	enum {
		IEEE80211_DISABLED, IEEE80211_AUTHENTICATE,
		IEEE80211_ASSOCIATE, IEEE80211_ASSOCIATED,
		IEEE80211_IBSS_SEARCH, IEEE80211_IBSS_JOINED
	} state;
	u8 prev_bssid[ETH_ALEN];
	u8 ssid[32];
	size_t ssid_len;
	u16 aid;
	u16 ap_capab, capab;
	u8 *extra_ie; /* to be added to the end of AssocReq */
	size_t extra_ie_len;
	u8 *extra_probe_ie; /* to be added to the end of ProbeReq */
	size_t extra_probe_ie_len;
	enum wpa_key_mgmt key_mgmt;

	/* The last AssocReq/Resp IEs */
	u8 *assocreq_ies, *assocresp_ies;
	size_t assocreq_ies_len, assocresp_ies_len;

	int auth_tries, assoc_tries;

	unsigned int ssid_set:1;
	unsigned int bssid_set:1;
	unsigned int prev_bssid_set:1;
	unsigned int authenticated:1;
	unsigned int associated:1;
	unsigned int probereq_poll:1;
	unsigned int use_protection:1;
	unsigned int create_ibss:1;
	unsigned int mixed_cell:1;
	unsigned int wmm_enabled:1;

	struct os_time last_probe;

	unsigned int auth_algs; /* bitfield of allowed auth algs
				 * (WPA_AUTH_ALG_*) */
	int auth_alg; /* currently used IEEE 802.11 authentication algorithm */
	int auth_transaction;

	struct os_time ibss_join_req;
	u8 *probe_resp; /* ProbeResp template for IBSS */
	size_t probe_resp_len;
	u32 supp_rates_bits;

	int wmm_last_param_set;

	int sta_scanning;
	int scan_hw_mode_idx;
	int scan_channel_idx;
	enum { SCAN_SET_CHANNEL, SCAN_SEND_PROBE } scan_state;
	struct os_time last_scan_completed;
	int scan_oper_channel;
	int scan_oper_freq;
	int scan_oper_phymode;
	u8 scan_ssid[32];
	size_t scan_ssid_len;
	int scan_skip_11b;
	int *scan_freqs;

	struct ieee80211_sta_bss *sta_bss_list;
#define STA_HASH_SIZE 256
#define STA_HASH(sta) (sta[5])
	struct ieee80211_sta_bss *sta_bss_hash[STA_HASH_SIZE];

	int cts_protect_erp_frames;

	enum hostapd_hw_mode phymode; /* current mode */
	struct hostapd_hw_modes *modes;
	size_t num_modes;
	unsigned int hw_modes; /* bitfield of allowed hardware modes;
				* (1 << HOSTAPD_MODE_*) */
	int num_curr_rates;
	int *curr_rates;
	int freq; /* The current frequency in MHz */
	int channel; /* The current IEEE 802.11 channel number */

#ifdef CONFIG_IEEE80211R
	u8 current_md[6];
	u8 *ft_ies;
	size_t ft_ies_len;
#endif /* CONFIG_IEEE80211R */

	void (*public_action_cb)(void *ctx, const u8 *buf, size_t len,
				 int freq);
	void *public_action_cb_ctx;

#else /* CONFIG_CLIENT_MLME */
	int dummy; /* to keep MSVC happy */
#endif /* CONFIG_CLIENT_MLME */
};

/**
 * struct wpa_supplicant - Internal data for wpa_supplicant interface
 *
 * This structure contains the internal data for core wpa_supplicant code. This
 * should be only used directly from the core code. However, a pointer to this
 * data is used from other files as an arbitrary context pointer in calls to
 * core functions.
 */
struct wpa_supplicant {
	struct wpa_global *global;
	struct wpa_supplicant *next;
	struct l2_packet_data *l2;
	struct l2_packet_data *l2_br;
	unsigned char own_addr[ETH_ALEN];
	char ifname[100];
#ifdef CONFIG_CTRL_IFACE_DBUS
	char *dbus_path;
#endif /* CONFIG_CTRL_IFACE_DBUS */
#ifdef CONFIG_CTRL_IFACE_DBUS_NEW
	char *dbus_new_path;
#endif /* CONFIG_CTRL_IFACE_DBUS_NEW */
	char bridge_ifname[16];

	char *confname;
	struct wpa_config *conf;
	int countermeasures;
	os_time_t last_michael_mic_error;
	u8 bssid[ETH_ALEN];
	u8 pending_bssid[ETH_ALEN]; /* If wpa_state == WPA_ASSOCIATING, this
				     * field contains the targer BSSID. */
	int reassociate; /* reassociation requested */
	int disconnected; /* all connections disabled; i.e., do no reassociate
			   * before this has been cleared */
	struct wpa_ssid *current_ssid;
	struct wpa_bss *current_bss;
	int ap_ies_from_associnfo;
	unsigned int assoc_freq;

	/* Selected configuration (based on Beacon/ProbeResp WPA IE) */
	int pairwise_cipher;
	int group_cipher;
	int key_mgmt;
	int mgmt_group_cipher;

	void *drv_priv; /* private data used by driver_ops */
	void *global_drv_priv;

	struct wpa_ssid *prev_scan_ssid; /* previously scanned SSID;
					  * NULL = not yet initialized (start
					  * with wildcard SSID)
					  * WILDCARD_SSID_SCAN = wildcard
					  * SSID was used in the previous scan
					  */
#define WILDCARD_SSID_SCAN ((struct wpa_ssid *) 1)

	void (*scan_res_handler)(struct wpa_supplicant *wpa_s,
				 struct wpa_scan_results *scan_res);
	struct dl_list bss; /* struct wpa_bss::list */
	struct dl_list bss_id; /* struct wpa_bss::list_id */
	size_t num_bss;
	unsigned int bss_update_idx;
	unsigned int bss_next_id;

	struct wpa_driver_ops *driver;
	int interface_removed; /* whether the network interface has been
				* removed */
	struct wpa_sm *wpa;
	struct eapol_sm *eapol;

	struct ctrl_iface_priv *ctrl_iface;

	enum wpa_states wpa_state;
	int scanning;
	int new_connection;
	int reassociated_connection;

	int eapol_received; /* number of EAPOL packets received after the
			     * previous association event */

	struct scard_data *scard;

	unsigned char last_eapol_src[ETH_ALEN];

	int keys_cleared;

	struct wpa_blacklist *blacklist;

	int scan_req; /* manual scan request; this forces a scan even if there
		       * are no enabled networks in the configuration */
	int scan_runs; /* number of scan runs since WPS was started */

	struct wpa_client_mlme mlme;
	unsigned int drv_flags;
	int max_scan_ssids;
	unsigned int max_remain_on_chan;

	int pending_mic_error_report;
	int pending_mic_error_pairwise;
	int mic_errors_seen; /* Michael MIC errors with the current PTK */

	struct wps_context *wps;
	int wps_success; /* WPS success event received */
	struct wps_er *wps_er;
	int blacklist_cleared;

	struct wpabuf *pending_eapol_rx;
	struct os_time pending_eapol_rx_time;
	u8 pending_eapol_rx_src[ETH_ALEN];

	struct ibss_rsn *ibss_rsn;

#ifdef CONFIG_SME
	struct {
		u8 ssid[32];
		size_t ssid_len;
		int freq;
		u8 assoc_req_ie[80];
		size_t assoc_req_ie_len;
		int mfp;
		int ft_used;
		u8 mobility_domain[2];
		u8 *ft_ies;
		size_t ft_ies_len;
		u8 prev_bssid[ETH_ALEN];
		int prev_bssid_set;
		int auth_alg;
	} sme;
#endif /* CONFIG_SME */

#ifdef CONFIG_AP
	struct hostapd_iface *ap_iface;
	void (*ap_configured_cb)(void *ctx, void *data);
	void *ap_configured_cb_ctx;
	void *ap_configured_cb_data;
#endif /* CONFIG_AP */

	struct wpa_ssid *bgscan_ssid;
	const struct bgscan_ops *bgscan;
	void *bgscan_priv;

	int connect_without_scan;

	int after_wps;
	unsigned int wps_freq;
};


/* wpa_supplicant.c */
int wpa_set_wep_keys(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);

int wpa_supplicant_reload_configuration(struct wpa_supplicant *wpa_s);

const char * wpa_supplicant_state_txt(enum wpa_states state);
int wpa_supplicant_driver_init(struct wpa_supplicant *wpa_s);
int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *bss, struct wpa_ssid *ssid,
			      u8 *wpa_ie, size_t *wpa_ie_len);
void wpa_supplicant_associate(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *bss,
			      struct wpa_ssid *ssid);
void wpa_supplicant_set_non_wpa_policy(struct wpa_supplicant *wpa_s,
				       struct wpa_ssid *ssid);
void wpa_supplicant_initiate_eapol(struct wpa_supplicant *wpa_s);
void wpa_clear_keys(struct wpa_supplicant *wpa_s, const u8 *addr);
void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s,
				     int sec, int usec);
void wpa_supplicant_set_state(struct wpa_supplicant *wpa_s,
			      enum wpa_states state);
struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s);
void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s);
void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,
				   int reason_code);
void wpa_supplicant_disassociate(struct wpa_supplicant *wpa_s,
				 int reason_code);

void wpa_supplicant_enable_network(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid);
void wpa_supplicant_disable_network(struct wpa_supplicant *wpa_s,
				    struct wpa_ssid *ssid);
void wpa_supplicant_select_network(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid);
int wpa_supplicant_set_ap_scan(struct wpa_supplicant *wpa_s,
			       int ap_scan);
int wpa_supplicant_set_debug_params(struct wpa_global *global,
				    int debug_level, int debug_timestamp,
				    int debug_show_keys);

void wpa_show_license(void);

struct wpa_supplicant * wpa_supplicant_add_iface(struct wpa_global *global,
						 struct wpa_interface *iface);
int wpa_supplicant_remove_iface(struct wpa_global *global,
				struct wpa_supplicant *wpa_s);
struct wpa_supplicant * wpa_supplicant_get_iface(struct wpa_global *global,
						 const char *ifname);
struct wpa_global * wpa_supplicant_init(struct wpa_params *params);
int wpa_supplicant_run(struct wpa_global *global);
void wpa_supplicant_deinit(struct wpa_global *global);

int wpa_supplicant_scard_init(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid);
void wpa_supplicant_terminate_proc(struct wpa_global *global);
void wpa_supplicant_rx_eapol(void *ctx, const u8 *src_addr,
			     const u8 *buf, size_t len);
enum wpa_key_mgmt key_mgmt2driver(int key_mgmt);
enum wpa_cipher cipher_suite2driver(int cipher);

/* events.c */
void wpa_supplicant_mark_disassoc(struct wpa_supplicant *wpa_s);
void wpa_supplicant_connect(struct wpa_supplicant *wpa_s,
			    struct wpa_bss *selected,
			    struct wpa_ssid *ssid);

/* eap_register.c */
int eap_register_methods(void);

#endif /* WPA_SUPPLICANT_I_H */
