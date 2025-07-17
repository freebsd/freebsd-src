/*
 * wpa_supplicant - Internal definitions
 * Copyright (c) 2003-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef WPA_SUPPLICANT_I_H
#define WPA_SUPPLICANT_I_H

#include "utils/bitfield.h"
#include "utils/list.h"
#include "common/defs.h"
#include "common/sae.h"
#include "common/wpa_ctrl.h"
#include "common/dpp.h"
#include "crypto/sha384.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "wps/wps_defs.h"
#include "config_ssid.h"
#include "wmm_ac.h"
#include "pasn/pasn_common.h"

extern const char *const wpa_supplicant_version;
extern const char *const wpa_supplicant_license;
#ifndef CONFIG_NO_STDOUT_DEBUG
extern const char *const wpa_supplicant_full_license1;
extern const char *const wpa_supplicant_full_license2;
extern const char *const wpa_supplicant_full_license3;
extern const char *const wpa_supplicant_full_license4;
extern const char *const wpa_supplicant_full_license5;
#endif /* CONFIG_NO_STDOUT_DEBUG */

struct wpa_sm;
struct wpa_supplicant;
struct ibss_rsn;
struct scan_info;
struct wpa_bss;
struct wpa_scan_results;
struct hostapd_hw_modes;
struct wpa_driver_associate_params;
struct wpa_cred;

/*
 * Forward declarations of private structures used within the ctrl_iface
 * backends. Other parts of wpa_supplicant do not have access to data stored in
 * these structures.
 */
struct ctrl_iface_priv;
struct ctrl_iface_global_priv;
struct wpas_dbus_priv;
struct wpas_binder_priv;

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
	 * confanother - Additional configuration name (file or profile) name
	 *
	 * This can also be %NULL when the additional configuration file is not
	 * used.
	 */
	const char *confanother;

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

	/**
	 * p2p_mgmt - Interface used for P2P management (P2P Device operations)
	 *
	 * Indicates whether wpas_p2p_init() must be called for this interface.
	 * This is used only when the driver supports a dedicated P2P Device
	 * interface that is not a network interface.
	 */
	int p2p_mgmt;

#ifdef CONFIG_MATCH_IFACE
	/**
	 * matched - Interface was matched rather than specified
	 *
	 */
	enum {
		WPA_IFACE_NOT_MATCHED,
		WPA_IFACE_MATCHED_NULL,
		WPA_IFACE_MATCHED
	} matched;
#endif /* CONFIG_MATCH_IFACE */
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
	 * ctrl_interface_group - Global ctrl_iface group
	 */
	char *ctrl_interface_group;

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
	 * wpa_debug_tracing - Enable log output through Linux tracing
	 */
	int wpa_debug_tracing;

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

	/**
	 * entropy_file - Optional entropy file
	 *
	 * This parameter can be used to configure wpa_supplicant to maintain
	 * its internal entropy store over restarts.
	 */
	char *entropy_file;

#ifdef CONFIG_P2P
	/**
	 * conf_p2p_dev - Configuration file used to hold the
	 * P2P Device configuration parameters.
	 *
	 * This can also be %NULL. In such a case, if a P2P Device dedicated
	 * interfaces is created, the main configuration file will be used.
	 */
	char *conf_p2p_dev;
#endif /* CONFIG_P2P */

#ifdef CONFIG_MATCH_IFACE
	/**
	 * match_ifaces - Interface descriptions to match
	 */
	struct wpa_interface *match_ifaces;

	/**
	 * match_iface_count - Number of defined matching interfaces
	 */
	int match_iface_count;
#endif /* CONFIG_MATCH_IFACE */
};

struct p2p_srv_bonjour {
	struct dl_list list;
	struct wpabuf *query;
	struct wpabuf *resp;
};

struct p2p_srv_upnp {
	struct dl_list list;
	u8 version;
	char *service;
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
	struct wpas_binder_priv *binder;
	void **drv_priv;
	size_t drv_count;
	struct os_time suspend_time;
	struct p2p_data *p2p;
	struct wpa_supplicant *p2p_init_wpa_s;
	struct wpa_supplicant *p2p_group_formation;
	struct wpa_supplicant *p2p_invite_group;
	u8 p2p_dev_addr[ETH_ALEN];
	struct os_reltime p2p_go_wait_client;
	struct dl_list p2p_srv_bonjour; /* struct p2p_srv_bonjour */
	struct dl_list p2p_srv_upnp; /* struct p2p_srv_upnp */
	int p2p_disabled;
	int cross_connection;
	int p2p_long_listen; /* remaining time in long Listen state in ms */
	struct wpa_freq_range_list p2p_disallow_freq;
	struct wpa_freq_range_list p2p_go_avoid_freq;
	enum wpa_conc_pref {
		WPA_CONC_PREF_NOT_SET,
		WPA_CONC_PREF_STA,
		WPA_CONC_PREF_P2P
	} conc_pref;
	unsigned int p2p_per_sta_psk:1;
	unsigned int p2p_fail_on_wps_complete:1;
	unsigned int p2p_24ghz_social_channels:1;
	unsigned int pending_p2ps_group:1;
	unsigned int pending_group_iface_for_p2ps:1;
	unsigned int pending_p2ps_group_freq;

#ifdef CONFIG_WIFI_DISPLAY
	int wifi_display;
#define MAX_WFD_SUBELEMS 12
	struct wpabuf *wfd_subelem[MAX_WFD_SUBELEMS];
#endif /* CONFIG_WIFI_DISPLAY */

	struct psk_list_entry *add_psk; /* From group formation */
};


/**
 * struct wpa_radio - Internal data for per-radio information
 *
 * This structure is used to share data about configured interfaces
 * (struct wpa_supplicant) that share the same physical radio, e.g., to allow
 * better coordination of offchannel operations.
 */
struct wpa_radio {
	char name[16]; /* from driver_ops get_radio_name() or empty if not
			* available */
	/** NULL if no external scan running. */
	struct wpa_supplicant *external_scan_req_interface;
	unsigned int num_active_works;
	struct dl_list ifaces; /* struct wpa_supplicant::radio_list entries */
	struct dl_list work; /* struct wpa_radio_work::list entries */
};

/**
 * Checks whether an external scan is running on a given radio.
 * @radio: Pointer to radio struct
 * Returns: true if an external scan is running, false otherwise.
 */
static inline bool external_scan_running(struct wpa_radio *radio)
{
	return radio && radio->external_scan_req_interface;
}

#define MAX_ACTIVE_WORKS 2


/**
 * struct wpa_radio_work - Radio work item
 */
struct wpa_radio_work {
	struct dl_list list;
	unsigned int freq; /* known frequency (MHz) or 0 for multiple/unknown */
	const char *type;
	struct wpa_supplicant *wpa_s;
	void (*cb)(struct wpa_radio_work *work, int deinit);
	void *ctx;
	unsigned int started:1;
	struct os_reltime time;
	unsigned int bands;
};

int radio_add_work(struct wpa_supplicant *wpa_s, unsigned int freq,
		   const char *type, int next,
		   void (*cb)(struct wpa_radio_work *work, int deinit),
		   void *ctx);
void radio_work_done(struct wpa_radio_work *work);
void radio_remove_works(struct wpa_supplicant *wpa_s,
			const char *type, int remove_all);
void radio_remove_pending_work(struct wpa_supplicant *wpa_s, void *ctx);
void radio_work_check_next(struct wpa_supplicant *wpa_s);
struct wpa_radio_work *
radio_work_pending(struct wpa_supplicant *wpa_s, const char *type);

struct wpa_connect_work {
	unsigned int sme:1;
	unsigned int bss_removed:1;
	struct wpa_bss *bss;
	struct wpa_ssid *ssid;
};

int wpas_valid_bss_ssid(struct wpa_supplicant *wpa_s, struct wpa_bss *test_bss,
			struct wpa_ssid *test_ssid);
void wpas_connect_work_free(struct wpa_connect_work *cwork);
void wpas_connect_work_done(struct wpa_supplicant *wpa_s);

struct wpa_external_work {
	unsigned int id;
	char type[100];
	unsigned int timeout;
};

enum wpa_radio_work_band wpas_freq_to_band(int freq);
unsigned int wpas_get_bands(struct wpa_supplicant *wpa_s, const int *freqs);

/**
 * offchannel_send_action_result - Result of offchannel send Action frame
 */
enum offchannel_send_action_result {
	OFFCHANNEL_SEND_ACTION_SUCCESS /**< Frame was send and acknowledged */,
	OFFCHANNEL_SEND_ACTION_NO_ACK /**< Frame was sent, but not acknowledged
				       */,
	OFFCHANNEL_SEND_ACTION_FAILED /**< Frame was not sent due to a failure
				       */
};

struct wps_ap_info {
	u8 bssid[ETH_ALEN];
	enum wps_ap_info_type {
		WPS_AP_NOT_SEL_REG,
		WPS_AP_SEL_REG,
		WPS_AP_SEL_REG_OUR
	} type;
	unsigned int tries;
	struct os_reltime last_attempt;
	unsigned int pbc_active;
	u8 uuid[WPS_UUID_LEN];
};

#define WPA_FREQ_USED_BY_INFRA_STATION BIT(0)
#define WPA_FREQ_USED_BY_P2P_CLIENT BIT(1)

struct wpa_used_freq_data {
	int freq;
	unsigned int flags;
};

#define RRM_NEIGHBOR_REPORT_TIMEOUT 1 /* 1 second for AP to send a report */

/*
 * struct rrm_data - Data used for managing RRM features
 */
struct rrm_data {
	/* rrm_used - indication regarding the current connection */
	unsigned int rrm_used:1;

	/*
	 * notify_neighbor_rep - Callback for notifying report requester
	 */
	void (*notify_neighbor_rep)(void *ctx, struct wpabuf *neighbor_rep);

	/*
	 * neighbor_rep_cb_ctx - Callback context
	 * Received in the callback registration, and sent to the callback
	 * function as a parameter.
	 */
	void *neighbor_rep_cb_ctx;

	/* next_neighbor_rep_token - Next request's dialog token */
	u8 next_neighbor_rep_token;

	/* token - Dialog token of the current radio measurement */
	u8 token;

	/* destination address of the current radio measurement request */
	u8 dst_addr[ETH_ALEN];
};

enum wpa_supplicant_test_failure {
	WPAS_TEST_FAILURE_NONE,
	WPAS_TEST_FAILURE_SCAN_TRIGGER,
};

struct icon_entry {
	struct dl_list list;
	u8 bssid[ETH_ALEN];
	u8 dialog_token;
	char *file_name;
	u8 *image;
	size_t image_len;
};

struct wpa_bss_tmp_disallowed {
	struct dl_list list;
	u8 bssid[ETH_ALEN];
	int rssi_threshold;
};

struct beacon_rep_data {
	u8 token;
	u8 last_indication;
	struct wpa_driver_scan_params scan_params;
	u8 ssid[SSID_MAX_LEN];
	size_t ssid_len;
	u8 bssid[ETH_ALEN];
	enum beacon_report_detail report_detail;
	struct bitfield *eids;
	struct bitfield *ext_eids;
};


struct external_pmksa_cache {
	struct dl_list list;
	void *pmksa_cache;
};

struct fils_hlp_req {
	struct dl_list list;
	u8 dst[ETH_ALEN];
	struct wpabuf *pkt;
};

struct driver_signal_override {
	struct dl_list list;
	u8 bssid[ETH_ALEN];
	int si_current_signal;
	int si_avg_signal;
	int si_avg_beacon_signal;
	int si_current_noise;
	int scan_level;
};

struct robust_av_data {
	u8 dialog_token;
	enum scs_request_type request_type;
	u8 up_bitmap;
	u8 up_limit;
	u32 stream_timeout;
	u8 frame_classifier[48];
	size_t frame_classifier_len;
	bool valid_config;
};

struct dscp_policy_status {
	u8 id;
	u8 status;
};

struct dscp_resp_data {
	bool more;
	bool reset;
	bool solicited;
	struct dscp_policy_status *policy;
	int num_policies;
};

enum ip_version {
	IPV4 = 4,
	IPV6 = 6,
};


struct ipv4_params {
	struct in_addr src_ip;
	struct in_addr dst_ip;
	u16 src_port;
	u16 dst_port;
	u8 dscp;
	u8 protocol;
};


struct ipv6_params {
	struct in6_addr src_ip;
	struct in6_addr dst_ip;
	u16 src_port;
	u16 dst_port;
	u8 dscp;
	u8 next_header;
	u8 flow_label[3];
};


struct type4_params {
	u8 classifier_mask;
	enum ip_version ip_version;
	union {
		struct ipv4_params v4;
		struct ipv6_params v6;
	} ip_params;
};


struct type10_params {
	u8 prot_instance;
	u8 prot_number;
	u8 *filter_value;
	u8 *filter_mask;
	size_t filter_len;
};


struct tclas_element {
	u8 user_priority;
	u8 classifier_type;
	union {
		struct type4_params type4_param;
		struct type10_params type10_param;
	} frame_classifier;
};


struct qos_characteristics {
	bool available;

	/* Control Info Direction */
	u8 direction;
	/* Presence Bitmap Of Additional Parameters */
	u16 mask;
	/* Minimum Service Interval */
	u32 min_si;
	/* Maximum Service Interval */
	u32 max_si;
	/* Minimum Data Rate */
	u32 min_data_rate;
	/* Delay Bound */
	u32 delay_bound;
	/* Maximum MSDU Size */
	u16 max_msdu_size;
	/* Service Start Time */
	u32 service_start_time;
	/* Service Start Time LinkID */
	u8 service_start_time_link_id;
	/* Mean Data Rate */
	u32 mean_data_rate;
	/* Delayed Bounded Burst Size */
	u32 burst_size;
	/* MSDU Lifetime */
	u16 msdu_lifetime;
	/* MSDU Delivery Info */
	u8 msdu_delivery_info;
	/* Medium Time */
	u16 medium_time;
};


struct scs_desc_elem {
	u8 scs_id;
	enum scs_request_type request_type;
	u8 intra_access_priority;
	bool scs_up_avail;
	struct tclas_element *tclas_elems;
	unsigned int num_tclas_elem;
	u8 tclas_processing;
	struct qos_characteristics qos_char_elem;
};


struct scs_robust_av_data {
	struct scs_desc_elem *scs_desc_elems;
	unsigned int num_scs_desc;
};


enum scs_response_status {
	SCS_DESC_SENT = 0,
	SCS_DESC_SUCCESS = 1,
};


struct active_scs_elem {
	struct dl_list list;
	u8 scs_id;
	enum scs_response_status status;
};


struct ml_sta_link_info {
	u8 link_id;
	u8 bssid[ETH_ALEN];
	u16 status;
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
	struct wpa_radio *radio; /* shared radio context */
	struct dl_list radio_list; /* list head: struct wpa_radio::ifaces */
	struct wpa_supplicant *parent;
	struct wpa_supplicant *p2pdev;
	struct wpa_supplicant *next;
	struct l2_packet_data *l2;
	struct l2_packet_data *l2_br;
	struct os_reltime roam_start;
	struct os_reltime roam_time;
	struct os_reltime session_start;
	struct os_reltime session_length;
	unsigned char own_addr[ETH_ALEN];
	unsigned char perm_addr[ETH_ALEN];
	char ifname[100];
#ifdef CONFIG_MATCH_IFACE
	int matched;
#endif /* CONFIG_MATCH_IFACE */
#ifdef CONFIG_CTRL_IFACE_DBUS_NEW
	char *dbus_new_path;
	char *dbus_groupobj_path;
#ifdef CONFIG_AP
	char *preq_notify_peer;
#endif /* CONFIG_AP */
#endif /* CONFIG_CTRL_IFACE_DBUS_NEW */
#ifdef CONFIG_CTRL_IFACE_BINDER
	const void *binder_object_key;
#endif /* CONFIG_CTRL_IFACE_BINDER */
	char bridge_ifname[16];

	char *confname;
	char *confanother;

	struct wpa_config *conf;
	int countermeasures;
	struct os_reltime last_michael_mic_error;
	u8 bssid[ETH_ALEN];
	u8 pending_bssid[ETH_ALEN]; /* If wpa_state == WPA_ASSOCIATING, this
				     * field contains the target BSSID. */
	int reassociate; /* reassociation requested */
	bool roam_in_progress; /* roam in progress */
	unsigned int reassoc_same_bss:1; /* reassociating to the same BSS */
	unsigned int reassoc_same_ess:1; /* reassociating to the same ESS */
	int disconnected; /* all connections disabled; i.e., do no reassociate
			   * before this has been cleared */
	struct wpa_ssid *current_ssid;
	struct wpa_ssid *last_ssid;
	struct wpa_bss *current_bss;
	int ap_ies_from_associnfo;
	unsigned int assoc_freq;
	u8 ap_mld_addr[ETH_ALEN];
	u8 mlo_assoc_link_id;
	u16 valid_links; /* bitmap of valid MLO link IDs */
	struct {
		u8 addr[ETH_ALEN];
		u8 bssid[ETH_ALEN];
		unsigned int freq;
		struct wpa_bss *bss;
		bool disabled;
	} links[MAX_NUM_MLD_LINKS];
	u8 *last_con_fail_realm;
	size_t last_con_fail_realm_len;

	/* Selected configuration (based on Beacon/ProbeResp WPA IE) */
	int pairwise_cipher;
	int deny_ptk0_rekey;
	int group_cipher;
	int key_mgmt;
	int wpa_proto;
	int mgmt_group_cipher;
	/*
	 * Allowed key management suites for roaming/initial connection
	 * when the driver's SME is in use.
	 */
	int allowed_key_mgmts;

	void *drv_priv; /* private data used by driver_ops */
	void *global_drv_priv;

	u8 *bssid_filter;
	size_t bssid_filter_count;

	u8 *disallow_aps_bssid;
	size_t disallow_aps_bssid_count;
	struct wpa_ssid_value *disallow_aps_ssid;
	size_t disallow_aps_ssid_count;

	u32 setband_mask;

	/* Preferred network for the next connection attempt */
	struct wpa_ssid *next_ssid;

	/* previous scan was wildcard when interleaving between
	 * wildcard scans and specific SSID scan when max_ssids=1 */
	int prev_scan_wildcard;
	struct wpa_ssid *prev_scan_ssid; /* previously scanned SSID;
					  * NULL = not yet initialized (start
					  * with wildcard SSID)
					  * WILDCARD_SSID_SCAN = wildcard
					  * SSID was used in the previous scan
					  */
#define WILDCARD_SSID_SCAN ((struct wpa_ssid *) 1)

	struct wpa_ssid *prev_sched_ssid; /* last SSID used in sched scan */
	int sched_scan_timeout;
	int first_sched_scan;
	int sched_scan_timed_out;
	struct sched_scan_plan *sched_scan_plans;
	size_t sched_scan_plans_num;

	void (*scan_res_handler)(struct wpa_supplicant *wpa_s,
				 struct wpa_scan_results *scan_res);
	void (*scan_res_fail_handler)(struct wpa_supplicant *wpa_s);
	struct dl_list bss; /* struct wpa_bss::list */
	struct dl_list bss_id; /* struct wpa_bss::list_id */
	size_t num_bss;
	unsigned int bss_update_idx;
	unsigned int bss_next_id;

	 /*
	  * Pointers to BSS entries in the order they were in the last scan
	  * results.
	  */
	struct wpa_bss **last_scan_res;
	size_t last_scan_res_used;
	size_t last_scan_res_size;
	struct os_reltime last_scan;

	const struct wpa_driver_ops *driver;
	int interface_removed; /* whether the network interface has been
				* removed */
	struct wpa_sm *wpa;
	struct ptksa_cache *ptksa;

	struct eapol_sm *eapol;

	struct ctrl_iface_priv *ctrl_iface;

	enum wpa_states wpa_state;
	struct wpa_radio_work *scan_work;
	int scanning;
	int sched_scanning;
	unsigned int sched_scan_stop_req:1;
	int new_connection;

	int eapol_received; /* number of EAPOL packets received after the
			     * previous association event */

	u8 rsnxe[20];
	size_t rsnxe_len;

	struct scard_data *scard;
	char imsi[20];
	int mnc_len;

	unsigned char last_eapol_src[ETH_ALEN];

	unsigned int keys_cleared; /* bitfield of key indexes that the driver is
				    * known not to be configured with a key */

	struct wpa_bssid_ignore *bssid_ignore;

	/* Number of connection failures since last successful connection */
	unsigned int consecutive_conn_failures;

	/**
	 * scan_req - Type of the scan request
	 */
	enum scan_req_type {
		/**
		 * NORMAL_SCAN_REQ - Normal scan request
		 *
		 * This is used for scans initiated by wpa_supplicant to find an
		 * AP for a connection.
		 */
		NORMAL_SCAN_REQ,

		/**
		 * INITIAL_SCAN_REQ - Initial scan request
		 *
		 * This is used for the first scan on an interface to force at
		 * least one scan to be run even if the configuration does not
		 * include any enabled networks.
		 */
		INITIAL_SCAN_REQ,

		/**
		 * MANUAL_SCAN_REQ - Manual scan request
		 *
		 * This is used for scans where the user request a scan or
		 * a specific wpa_supplicant operation (e.g., WPS) requires scan
		 * to be run.
		 */
		MANUAL_SCAN_REQ
	} scan_req, last_scan_req;
	enum wpa_states scan_prev_wpa_state;
	struct os_reltime scan_trigger_time, scan_start_time;
	/* Minimum freshness requirement for connection purposes */
	struct os_reltime scan_min_time;
	int scan_runs; /* number of scan runs since WPS was started */
	int *next_scan_freqs;
	int *select_network_scan_freqs;
	int *manual_scan_freqs;
	int *manual_sched_scan_freqs;
	unsigned int manual_scan_passive:1;
	unsigned int manual_scan_use_id:1;
	unsigned int manual_scan_only_new:1;
	unsigned int own_scan_requested:1;
	unsigned int own_scan_running:1;
	unsigned int clear_driver_scan_cache:1;
	unsigned int manual_non_coloc_6ghz:1;
	unsigned int manual_scan_id;
	int scan_interval; /* time in sec between scans to find suitable AP */
	int normal_scans; /* normal scans run before sched_scan */
	int scan_for_connection; /* whether the scan request was triggered for
				  * finding a connection */
	/*
	 * A unique cookie representing the vendor scan request. This cookie is
	 * returned from the driver interface. 0 indicates that there is no
	 * pending vendor scan request.
	 */
	u64 curr_scan_cookie;
#define MAX_SCAN_ID 16
	int scan_id[MAX_SCAN_ID];
	unsigned int scan_id_count;
	u8 next_scan_bssid[ETH_ALEN];
	unsigned int next_scan_bssid_wildcard_ssid:1;

	struct wpa_ssid_value *ssids_from_scan_req;
	unsigned int num_ssids_from_scan_req;
	int *last_scan_freqs;
	unsigned int num_last_scan_freqs;
	unsigned int suitable_network;
	unsigned int no_suitable_network;

	u8 ml_probe_bssid[ETH_ALEN];
	int ml_probe_mld_id;
	u16 ml_probe_links;

	u64 drv_flags;
	u64 drv_flags2;
	unsigned int drv_enc;
	unsigned int drv_rrm_flags;
	unsigned int drv_max_acl_mac_addrs;

	/*
	 * A bitmap of supported protocols for probe response offload. See
	 * struct wpa_driver_capa in driver.h
	 */
	unsigned int probe_resp_offloads;

	/* extended capabilities supported by the driver */
	const u8 *extended_capa, *extended_capa_mask;
	unsigned int extended_capa_len;

	int max_scan_ssids;
	int max_sched_scan_ssids;
	unsigned int max_sched_scan_plans;
	unsigned int max_sched_scan_plan_interval;
	unsigned int max_sched_scan_plan_iterations;
	int sched_scan_supported;
	unsigned int max_match_sets;
	unsigned int max_remain_on_chan;
	unsigned int max_stations;
	unsigned int max_num_akms;

	int pending_mic_error_report;
	int pending_mic_error_pairwise;
	int mic_errors_seen; /* Michael MIC errors with the current PTK */

	struct wps_context *wps;
	int wps_success; /* WPS success event received */
	struct wps_er *wps_er;
	unsigned int wps_run;
	struct os_reltime wps_pin_start_time;
	bool bssid_ignore_cleared;

	struct wpabuf *pending_eapol_rx;
	struct os_reltime pending_eapol_rx_time;
	u8 pending_eapol_rx_src[ETH_ALEN];
	enum frame_encryption pending_eapol_encrypted;
	unsigned int last_eapol_matches_bssid:1;
	unsigned int eapol_failed:1;
	unsigned int eap_expected_failure:1;
	unsigned int reattach:1; /* reassociation to the same BSS requested */
	unsigned int mac_addr_changed:1;
	unsigned int added_vif:1;
	unsigned int wnmsleep_used:1;
	unsigned int owe_transition_select:1;
	unsigned int owe_transition_search:1;
	unsigned int connection_set:1;
	unsigned int connection_ht:1;
	unsigned int connection_vht:1;
	unsigned int connection_he:1;
	unsigned int connection_eht:1;
	unsigned int disable_mbo_oce:1;

	struct os_reltime last_mac_addr_change;
	enum wpas_mac_addr_style last_mac_addr_style;

	struct ibss_rsn *ibss_rsn;

	int set_sta_uapsd;
	int sta_uapsd;
	int set_ap_uapsd;
	int ap_uapsd;
	int auth_alg;
	u16 last_owe_group;

#ifdef CONFIG_SME
	struct {
		u8 ssid[SSID_MAX_LEN];
		size_t ssid_len;
		int freq;
		u8 assoc_req_ie[1500];
		size_t assoc_req_ie_len;
		int mfp;
		int ft_used;
		u8 mobility_domain[2];
		u8 *ft_ies;
		size_t ft_ies_len;
		u8 prev_bssid[ETH_ALEN];
		int prev_bssid_set;
		int auth_alg;
		int proto;

		int sa_query_count; /* number of pending SA Query requests;
				     * 0 = no SA Query in progress */
		int sa_query_timed_out;
		u8 *sa_query_trans_id; /* buffer of WLAN_SA_QUERY_TR_ID_LEN *
					* sa_query_count octets of pending
					* SA Query transaction identifiers */
		struct os_reltime sa_query_start;
		struct os_reltime last_unprot_disconnect;
		enum { HT_SEC_CHAN_UNKNOWN,
		       HT_SEC_CHAN_ABOVE,
		       HT_SEC_CHAN_BELOW } ht_sec_chan;
		u8 sched_obss_scan;
		u16 obss_scan_int;
		u16 bss_max_idle_period;
#ifdef CONFIG_SAE
		struct sae_data sae;
		struct wpabuf *sae_token;
		int sae_group_index;
		unsigned int sae_pmksa_caching:1;
		u16 seq_num;
		u8 ext_auth_bssid[ETH_ALEN];
		struct wpa_ssid *ext_auth_wpa_ssid;
		u8 ext_auth_ssid[SSID_MAX_LEN];
		size_t ext_auth_ssid_len;
		int ext_auth_key_mgmt;
		u8 ext_auth_ap_mld_addr[ETH_ALEN];
		bool ext_ml_auth;
		int *sae_rejected_groups;
#endif /* CONFIG_SAE */
		u16 assoc_auth_type;
	} sme;
#endif /* CONFIG_SME */

#ifdef CONFIG_AP
	struct hostapd_iface *ap_iface;
	void (*ap_configured_cb)(void *ctx, void *data);
	void *ap_configured_cb_ctx;
	void *ap_configured_cb_data;
#endif /* CONFIG_AP */

	struct hostapd_iface *ifmsh;
#ifdef CONFIG_MESH
	struct mesh_rsn *mesh_rsn;
	int mesh_if_idx;
	unsigned int mesh_if_created:1;
	unsigned int mesh_ht_enabled:1;
	unsigned int mesh_vht_enabled:1;
	unsigned int mesh_he_enabled:1;
	unsigned int mesh_eht_enabled:1;
	struct wpa_driver_mesh_join_params *mesh_params;
#ifdef CONFIG_PMKSA_CACHE_EXTERNAL
	/* struct external_pmksa_cache::list */
	struct dl_list mesh_external_pmksa_cache;
#endif /* CONFIG_PMKSA_CACHE_EXTERNAL */
#endif /* CONFIG_MESH */

	unsigned int off_channel_freq;
	struct wpabuf *pending_action_tx;
	u8 pending_action_src[ETH_ALEN];
	u8 pending_action_dst[ETH_ALEN];
	u8 pending_action_bssid[ETH_ALEN];
	unsigned int pending_action_freq;
	int pending_action_no_cck;
	int pending_action_without_roc;
	unsigned int pending_action_tx_done:1;
	void (*pending_action_tx_status_cb)(struct wpa_supplicant *wpa_s,
					    unsigned int freq, const u8 *dst,
					    const u8 *src, const u8 *bssid,
					    const u8 *data, size_t data_len,
					    enum offchannel_send_action_result
					    result);
	unsigned int roc_waiting_drv_freq;
	int action_tx_wait_time;
	int action_tx_wait_time_used;

	int p2p_mgmt;

#ifdef CONFIG_P2P
	struct p2p_go_neg_results *go_params;
	int create_p2p_iface;
	u8 pending_interface_addr[ETH_ALEN];
	char pending_interface_name[100];
	int pending_interface_type;
	int p2p_group_idx;
	unsigned int pending_listen_freq;
	unsigned int pending_listen_duration;
	enum {
		NOT_P2P_GROUP_INTERFACE,
		P2P_GROUP_INTERFACE_PENDING,
		P2P_GROUP_INTERFACE_GO,
		P2P_GROUP_INTERFACE_CLIENT
	} p2p_group_interface;
	struct p2p_group *p2p_group;
	char p2p_pin[10];
	int p2p_wps_method;
	u8 p2p_auth_invite[ETH_ALEN];
	int p2p_sd_over_ctrl_iface;
	int p2p_in_provisioning;
	int p2p_in_invitation;
	int p2p_retry_limit;
	int p2p_invite_go_freq;
	int pending_invite_ssid_id;
	int show_group_started;
	u8 go_dev_addr[ETH_ALEN];
	int pending_pd_before_join;
	u8 pending_join_iface_addr[ETH_ALEN];
	u8 pending_join_dev_addr[ETH_ALEN];
	int pending_join_wps_method;
	u8 p2p_join_ssid[SSID_MAX_LEN];
	size_t p2p_join_ssid_len;
	int p2p_join_scan_count;
	int auto_pd_scan_retry;
	int force_long_sd;
	u16 pending_pd_config_methods;
	enum {
		NORMAL_PD, AUTO_PD_GO_NEG, AUTO_PD_JOIN, AUTO_PD_ASP
	} pending_pd_use;

	/*
	 * Whether cross connection is disallowed by the AP to which this
	 * interface is associated (only valid if there is an association).
	 */
	int cross_connect_disallowed;

	/*
	 * Whether this P2P group is configured to use cross connection (only
	 * valid if this is P2P GO interface). The actual cross connect packet
	 * forwarding may not be configured depending on the uplink status.
	 */
	int cross_connect_enabled;

	/* Whether cross connection forwarding is in use at the moment. */
	int cross_connect_in_use;

	/*
	 * Uplink interface name for cross connection
	 */
	char cross_connect_uplink[100];

	unsigned int p2p_auto_join:1;
	unsigned int p2p_auto_pd:1;
	unsigned int p2p_go_do_acs:1;
	unsigned int p2p_persistent_group:1;
	unsigned int p2p_fallback_to_go_neg:1;
	unsigned int p2p_pd_before_go_neg:1;
	unsigned int p2p_go_ht40:1;
	unsigned int p2p_go_vht:1;
	unsigned int p2p_go_edmg:1;
	unsigned int p2p_go_he:1;
	unsigned int user_initiated_pd:1;
	unsigned int p2p_go_group_formation_completed:1;
	unsigned int group_formation_reported:1;
	unsigned int p2p_go_no_pri_sec_switch:1;
	unsigned int waiting_presence_resp;
	int p2p_first_connection_timeout;
	unsigned int p2p_nfc_tag_enabled:1;
	unsigned int p2p_peer_oob_pk_hash_known:1;
	unsigned int p2p_disable_ip_addr_req:1;
	unsigned int p2ps_method_config_any:1;
	unsigned int p2p_cli_probe:1;
	unsigned int p2p_go_allow_dfs:1;
	enum hostapd_hw_mode p2p_go_acs_band;
	int p2p_persistent_go_freq;
	int p2p_persistent_id;
	int p2p_go_intent;
	int p2p_connect_freq;
	struct os_reltime p2p_auto_started;
	struct wpa_ssid *p2p_last_4way_hs_fail;
	struct wpa_radio_work *p2p_scan_work;
	struct wpa_radio_work *p2p_listen_work;
	struct wpa_radio_work *p2p_send_action_work;

	u16 p2p_oob_dev_pw_id; /* OOB Device Password Id for group formation */
	struct wpabuf *p2p_oob_dev_pw; /* OOB Device Password for group
					* formation */
	u8 p2p_peer_oob_pubkey_hash[WPS_OOB_PUBKEY_HASH_LEN];
	u8 p2p_ip_addr_info[3 * 4];

	/* group common frequencies */
	int *p2p_group_common_freqs;
	unsigned int p2p_group_common_freqs_num;
	u8 p2ps_join_addr[ETH_ALEN];

	unsigned int p2p_go_max_oper_chwidth;
	unsigned int p2p_go_vht_center_freq2;
	int p2p_lo_started;
#endif /* CONFIG_P2P */

	struct wpa_ssid *bgscan_ssid;
	const struct bgscan_ops *bgscan;
	void *bgscan_priv;
	int signal_threshold;

	const struct autoscan_ops *autoscan;
	struct wpa_driver_scan_params *autoscan_params;
	void *autoscan_priv;

	struct wpa_ssid *connect_without_scan;

	struct wps_ap_info *wps_ap;
	size_t num_wps_ap;
	int wps_ap_iter;

	int after_wps;
	int known_wps_freq;
	unsigned int wps_freq;
	int wps_fragment_size;
	int auto_reconnect_disabled;

	 /* Channel preferences for AP/P2P GO use */
	int best_24_freq;
	int best_5_freq;
	int best_overall_freq;

	struct gas_query *gas;
	struct gas_server *gas_server;

#ifdef CONFIG_INTERWORKING
	unsigned int fetch_anqp_in_progress:1;
	unsigned int network_select:1;
	unsigned int auto_select:1;
	unsigned int auto_network_select:1;
	unsigned int interworking_fast_assoc_tried:1;
	unsigned int fetch_all_anqp:1;
	unsigned int fetch_osu_info:1;
	unsigned int fetch_osu_waiting_scan:1;
	unsigned int fetch_osu_icon_in_progress:1;
	struct wpa_bss *interworking_gas_bss;
	unsigned int osu_icon_id;
	struct dl_list icon_head; /* struct icon_entry */
	struct osu_provider *osu_prov;
	size_t osu_prov_count;
	struct os_reltime osu_icon_fetch_start;
	unsigned int num_osu_scans;
	unsigned int num_prov_found;
#endif /* CONFIG_INTERWORKING */
	unsigned int drv_capa_known;

	struct {
		struct hostapd_hw_modes *modes;
		u16 num_modes;
		u16 flags;
	} hw;
	enum local_hw_capab {
		CAPAB_NO_HT_VHT,
		CAPAB_HT,
		CAPAB_HT40,
		CAPAB_VHT,
	} hw_capab;
#ifdef CONFIG_MACSEC
	struct ieee802_1x_kay *kay;
#endif /* CONFIG_MACSEC */

	int pno;
	int pno_sched_pending;

	/* WLAN_REASON_* reason codes. Negative if locally generated. */
	int disconnect_reason;

	/* WLAN_STATUS_* status codes from last received Authentication frame
	 * from the AP. */
	u16 auth_status_code;

	/* WLAN_STATUS_* status codes from (Re)Association Response frame. */
	u16 assoc_status_code;

	struct ext_password_data *ext_pw;

	struct wpabuf *last_gas_resp, *prev_gas_resp;
	u8 last_gas_addr[ETH_ALEN], prev_gas_addr[ETH_ALEN];
	u8 last_gas_dialog_token, prev_gas_dialog_token;

	unsigned int no_keep_alive:1;
	unsigned int ext_mgmt_frame_handling:1;
	unsigned int ext_eapol_frame_io:1;
	unsigned int wmm_ac_supported:1;
	unsigned int ext_work_in_progress:1;
	unsigned int own_disconnect_req:1;
	unsigned int own_reconnect_req:1;
	unsigned int ignore_post_flush_scan_res:1;

#define MAC_ADDR_RAND_SCAN       BIT(0)
#define MAC_ADDR_RAND_SCHED_SCAN BIT(1)
#define MAC_ADDR_RAND_PNO        BIT(2)
#define MAC_ADDR_RAND_ALL        (MAC_ADDR_RAND_SCAN | \
				  MAC_ADDR_RAND_SCHED_SCAN | \
				  MAC_ADDR_RAND_PNO)
	unsigned int mac_addr_rand_supported;
	unsigned int mac_addr_rand_enable;

	/* MAC Address followed by mask (2 * ETH_ALEN) */
	u8 *mac_addr_scan;
	u8 *mac_addr_sched_scan;
	u8 *mac_addr_pno;

#ifdef CONFIG_WNM
	u8 wnm_dialog_token;
	u8 wnm_reply;
	u8 wnm_num_neighbor_report;
	u8 wnm_mode;
	bool wnm_link_removal;
	u8 wnm_dissoc_addr[ETH_ALEN];
	u16 wnm_dissoc_timer;
	u8 wnm_bss_termination_duration[12];
	struct neighbor_report *wnm_neighbor_report_elements;
	struct os_reltime wnm_cand_valid_until;
	struct wpa_bss *wnm_target_bss;
	enum bss_trans_mgmt_status_code bss_tm_status;
	bool bss_trans_mgmt_in_progress;
	u8 coloc_intf_dialog_token;
	u8 coloc_intf_auto_report;
	u8 coloc_intf_timeout;
#ifdef CONFIG_MBO
	unsigned int wnm_mbo_trans_reason_present:1;
	u8 wnm_mbo_transition_reason;
#endif /* CONFIG_MBO */
#endif /* CONFIG_WNM */

#ifdef CONFIG_TESTING_GET_GTK
	u8 last_gtk[32];
	size_t last_gtk_len;
#endif /* CONFIG_TESTING_GET_GTK */

	unsigned int num_multichan_concurrent;
	struct wpa_radio_work *connect_work;

	unsigned int ext_work_id;

	struct wpabuf *vendor_elem[NUM_VENDOR_ELEM_FRAMES];

#ifdef CONFIG_TESTING_OPTIONS
	struct l2_packet_data *l2_test;
	unsigned int extra_roc_dur;
	enum wpa_supplicant_test_failure test_failure;
	char *get_pref_freq_list_override;
	unsigned int reject_btm_req_reason;
	unsigned int p2p_go_csa_on_inv:1;
	unsigned int ignore_auth_resp:1;
	unsigned int ignore_assoc_disallow:1;
	unsigned int disable_sa_query:1;
	unsigned int testing_resend_assoc:1;
	unsigned int ignore_sae_h2e_only:1;
	int ft_rsnxe_used;
	struct wpabuf *sae_commit_override;
	enum wpa_alg last_tk_alg;
	u8 last_tk_addr[ETH_ALEN];
	int last_tk_key_idx;
	u8 last_tk[WPA_TK_MAX_LEN];
	size_t last_tk_len;
	struct wpabuf *last_assoc_req_wpa_ie;
	int *extra_sae_rejected_groups;
	struct wpabuf *rsne_override_eapol;
	struct wpabuf *rsnxe_override_assoc;
	struct wpabuf *rsnxe_override_eapol;
	struct dl_list drv_signal_override;
	unsigned int oci_freq_override_eapol;
	unsigned int oci_freq_override_saquery_req;
	unsigned int oci_freq_override_saquery_resp;
	unsigned int oci_freq_override_eapol_g2;
	unsigned int oci_freq_override_ft_assoc;
	unsigned int oci_freq_override_fils_assoc;
	unsigned int oci_freq_override_wnm_sleep;
	unsigned int disable_eapol_g2_tx;
	int test_assoc_comeback_type;
#endif /* CONFIG_TESTING_OPTIONS */

	struct wmm_ac_assoc_data *wmm_ac_assoc_info;
	struct wmm_tspec_element *tspecs[WMM_AC_NUM][TS_DIR_IDX_COUNT];
	struct wmm_ac_addts_request *addts_request;
	u8 wmm_ac_last_dialog_token;
	struct wmm_tspec_element *last_tspecs;
	u8 last_tspecs_count;

	struct rrm_data rrm;
	struct beacon_rep_data beacon_rep_data;

#ifdef CONFIG_FST
	struct fst_iface *fst;
	const struct wpabuf *fst_ies;
	struct wpabuf *received_mb_ies;
#endif /* CONFIG_FST */

#ifdef CONFIG_MBO
	/* Multiband operation non-preferred channel */
	struct wpa_mbo_non_pref_channel {
		enum mbo_non_pref_chan_reason reason;
		u8 oper_class;
		u8 chan;
		u8 preference;
	} *non_pref_chan;
	size_t non_pref_chan_num;
	u8 mbo_wnm_token;
	/**
	 * enable_oce - Enable OCE if it is enabled by user and device also
	 *		supports OCE.
	 * User can enable OCE with wpa_config's 'oce' parameter as follows -
	 *  - Set BIT(0) to enable OCE in non-AP STA mode.
	 *  - Set BIT(1) to enable OCE in STA-CFON mode.
	 */
	u8 enable_oce;
#endif /* CONFIG_MBO */

	/*
	 * This should be under CONFIG_MBO, but it is left out to allow using
	 * the bss_temp_disallowed list for other purposes as well.
	 */
	struct dl_list bss_tmp_disallowed;

	/*
	 * Content of a measurement report element with type 8 (LCI),
	 * own location.
	 */
	struct wpabuf *lci;
	struct os_reltime lci_time;

	struct os_reltime beacon_rep_scan;

	/* FILS HLP requests (struct fils_hlp_req) */
	struct dl_list fils_hlp_req;

	struct sched_scan_relative_params {
		/**
		 * relative_rssi_set - Enable relatively preferred BSS reporting
		 *
		 * 0 = Disable reporting relatively preferred BSSs
		 * 1 = Enable reporting relatively preferred BSSs
		 */
		int relative_rssi_set;

		/**
		 * relative_rssi - Relative RSSI for reporting better BSSs
		 *
		 * Amount of RSSI by which a BSS should be better than the
		 * current connected BSS so that the new BSS can be reported
		 * to user space. This applies to sched_scan operations.
		 */
		int relative_rssi;

		/**
		 * relative_adjust_band - Band in which RSSI is to be adjusted
		 */
		enum set_band relative_adjust_band;

		/**
		 * relative_adjust_rssi - RSSI adjustment
		 *
		 * An amount of relative_adjust_rssi should be added to the
		 * BSSs that belong to the relative_adjust_band while comparing
		 * with other bands for BSS reporting.
		 */
		int relative_adjust_rssi;
	} srp;

	/* RIC elements for FT protocol */
	struct wpabuf *ric_ies;

	int last_auth_timeout_sec;

#ifdef CONFIG_DPP
	struct dpp_global *dpp;
	struct dpp_authentication *dpp_auth;
	struct wpa_radio_work *dpp_listen_work;
	unsigned int dpp_pending_listen_freq;
	unsigned int dpp_listen_freq;
	struct os_reltime dpp_listen_end;
	u8 dpp_allowed_roles;
	int dpp_qr_mutual;
	int dpp_netrole;
	int dpp_auth_ok_on_ack;
	int dpp_in_response_listen;
	bool dpp_tx_auth_resp_on_roc_stop;
	bool dpp_tx_chan_change;
	bool dpp_listen_on_tx_expire;
	int dpp_gas_client;
	int dpp_gas_server;
	int dpp_gas_dialog_token;
	u8 dpp_intro_bssid[ETH_ALEN];
	void *dpp_intro_network;
	u8 dpp_intro_peer_version;
	struct dpp_pkex *dpp_pkex;
	struct dpp_bootstrap_info *dpp_pkex_bi;
	char *dpp_pkex_code;
	size_t dpp_pkex_code_len;
	char *dpp_pkex_identifier;
	enum dpp_pkex_ver dpp_pkex_ver;
	char *dpp_pkex_auth_cmd;
	char *dpp_configurator_params;
	struct os_reltime dpp_last_init;
	struct os_reltime dpp_init_iter_start;
	unsigned int dpp_init_max_tries;
	unsigned int dpp_init_retry_time;
	unsigned int dpp_resp_wait_time;
	unsigned int dpp_resp_max_tries;
	unsigned int dpp_resp_retry_time;
	u8 dpp_last_ssid[SSID_MAX_LEN];
	size_t dpp_last_ssid_len;
	bool dpp_conf_backup_received;
	bool dpp_pkex_wait_auth_req;
#ifdef CONFIG_DPP2
	struct dpp_pfs *dpp_pfs;
	int dpp_pfs_fallback;
	struct wpabuf *dpp_presence_announcement;
	struct dpp_bootstrap_info *dpp_chirp_bi;
	int dpp_chirp_freq;
	int *dpp_chirp_freqs;
	int dpp_chirp_iter;
	int dpp_chirp_round;
	int dpp_chirp_scan_done;
	int dpp_chirp_listen;
	struct wpa_ssid *dpp_reconfig_ssid;
	int dpp_reconfig_ssid_id;
	struct dpp_reconfig_id *dpp_reconfig_id;
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_DPP3
	struct os_reltime dpp_pb_time;
	bool dpp_pb_configurator;
	int *dpp_pb_freqs;
	unsigned int dpp_pb_freq_idx;
	unsigned int dpp_pb_announce_count;
	struct wpabuf *dpp_pb_announcement;
	struct dpp_bootstrap_info *dpp_pb_bi;
	unsigned int dpp_pb_resp_freq;
	u8 dpp_pb_init_hash[SHA256_MAC_LEN];
	int dpp_pb_stop_iter;
	bool dpp_pb_discovery_done;
	u8 dpp_pb_c_nonce[DPP_MAX_NONCE_LEN];
	size_t dpp_pb_c_nonce_len;
	bool dpp_pb_result_indicated;
	struct os_reltime dpp_pb_announce_time;
	struct dpp_pb_info dpp_pb[DPP_PB_INFO_COUNT];
	u8 dpp_pb_resp_hash[SHA256_MAC_LEN];
	struct os_reltime dpp_pb_last_resp;
	char *dpp_pb_cmd;
#endif /* CONFIG_DPP3 */
#ifdef CONFIG_TESTING_OPTIONS
	char *dpp_config_obj_override;
	char *dpp_discovery_override;
	char *dpp_groups_override;
	unsigned int dpp_ignore_netaccesskey_mismatch:1;
	unsigned int dpp_discard_public_action:1;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_DPP */

#ifdef CONFIG_FILS
	unsigned int disable_fils:1;
#endif /* CONFIG_FILS */
	unsigned int ieee80211ac:1;
	unsigned int enabled_4addr_mode:1;
	unsigned int multi_bss_support:1;
	unsigned int drv_authorized_port:1;
	unsigned int multi_ap_ie:1;
	unsigned int multi_ap_backhaul:1;
	unsigned int multi_ap_fronthaul:1;

#ifndef CONFIG_NO_ROBUST_AV
	struct robust_av_data robust_av;
	bool mscs_setup_done;
	struct scs_robust_av_data scs_robust_av_req;
	u8 scs_dialog_token;
	struct dl_list active_scs_ids;
	bool ongoing_scs_req;
	u8 dscp_req_dialog_token;
	u8 dscp_query_dialog_token;
	unsigned int enable_dscp_policy_capa:1;
	unsigned int connection_dscp:1;
	unsigned int wait_for_dscp_req:1;
#ifdef CONFIG_TESTING_OPTIONS
	unsigned int disable_scs_support:1;
	unsigned int disable_mscs_support:1;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_NO_ROBUST_AV */

	bool wps_scan_done; /* Set upon receiving scan results event */
	bool supp_pbc_active; /* Set for interface when PBC is triggered */
	bool wps_overlap;

#ifdef CONFIG_PASN
	struct pasn_data pasn;
	struct wpa_radio_work *pasn_auth_work;
	unsigned int pasn_count;
	struct pasn_auth *pasn_params;
#endif /* CONFIG_PASN */

	bool is_6ghz_enabled;
	bool crossed_6ghz_dom;
	bool last_scan_all_chan;
	bool last_scan_non_coloc_6ghz;
	bool support_6ghz;

	struct wpa_signal_info last_signal_info;

	struct wpa_ssid *ml_connect_probe_ssid;
	struct wpa_bss *ml_connect_probe_bss;

#ifdef CONFIG_OWE
	/* An array of frequencies to scan for OWE transition mode BSSs when
	 * owe_transition_search == 1 */
	int *owe_trans_scan_freq;
#endif /* CONFIG_OWE */

#ifdef CONFIG_NAN_USD
	struct nan_de *nan_de;
	struct wpa_radio_work *nan_usd_listen_work;
	struct wpa_radio_work *nan_usd_tx_work;
#endif /* CONFIG_NAN_USD */

	bool ssid_verified;
	bool bigtk_set;
	u64 first_beacon_tsf;
	unsigned int beacons_checked;
	unsigned int next_beacon_check;
};


/* wpa_supplicant.c */
void wpa_supplicant_apply_ht_overrides(
	struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
	struct wpa_driver_associate_params *params);
void wpa_supplicant_apply_vht_overrides(
	struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
	struct wpa_driver_associate_params *params);
void wpa_supplicant_apply_he_overrides(
	struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
	struct wpa_driver_associate_params *params);
void wpa_supplicant_apply_eht_overrides(
	struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
	struct wpa_driver_associate_params *params);

int wpa_set_wep_keys(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);
int wpa_supplicant_set_wpa_none_key(struct wpa_supplicant *wpa_s,
				    struct wpa_ssid *ssid);

int wpa_supplicant_reload_configuration(struct wpa_supplicant *wpa_s);

const char * wpa_supplicant_state_txt(enum wpa_states state);
int wpa_supplicant_update_mac_addr(struct wpa_supplicant *wpa_s);
int wpa_supplicant_driver_init(struct wpa_supplicant *wpa_s);
int wpa_supplicant_update_bridge_ifname(struct wpa_supplicant *wpa_s,
					const char *bridge_ifname);
void wpas_set_mgmt_group_cipher(struct wpa_supplicant *wpa_s,
				struct wpa_ssid *ssid, struct wpa_ie_data *ie);
int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *bss, struct wpa_ssid *ssid,
			      u8 *wpa_ie, size_t *wpa_ie_len,
			      bool skip_default_rsne);
int wpas_restore_permanent_mac_addr(struct wpa_supplicant *wpa_s);
void wpa_supplicant_associate(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *bss,
			      struct wpa_ssid *ssid);
void wpa_supplicant_set_non_wpa_policy(struct wpa_supplicant *wpa_s,
				       struct wpa_ssid *ssid);
void wpa_supplicant_initiate_eapol(struct wpa_supplicant *wpa_s);
void wpa_clear_keys(struct wpa_supplicant *wpa_s, const u8 *addr);
void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s,
				     int sec, int usec);
void wpas_auth_timeout_restart(struct wpa_supplicant *wpa_s, int sec_diff);
void wpa_supplicant_reinit_autoscan(struct wpa_supplicant *wpa_s);
void wpa_supplicant_set_state(struct wpa_supplicant *wpa_s,
			      enum wpa_states state);
struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s);
const char * wpa_supplicant_get_eap_mode(struct wpa_supplicant *wpa_s);
void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s);
void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,
				   u16 reason_code);
void wpa_supplicant_reconnect(struct wpa_supplicant *wpa_s);

struct wpa_ssid * wpa_supplicant_add_network(struct wpa_supplicant *wpa_s);
int wpa_supplicant_remove_network(struct wpa_supplicant *wpa_s, int id);
int wpa_supplicant_remove_all_networks(struct wpa_supplicant *wpa_s);
void wpa_supplicant_enable_network(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid);
void wpa_supplicant_disable_network(struct wpa_supplicant *wpa_s,
				    struct wpa_ssid *ssid);
void wpa_supplicant_select_network(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid);
int wpas_remove_cred(struct wpa_supplicant *wpa_s, struct wpa_cred *cred);
int wpas_remove_all_creds(struct wpa_supplicant *wpa_s);
int wpas_set_pkcs11_engine_and_module_path(struct wpa_supplicant *wpa_s,
					   const char *pkcs11_engine_path,
					   const char *pkcs11_module_path);
int wpa_supplicant_set_ap_scan(struct wpa_supplicant *wpa_s,
			       int ap_scan);
int wpa_supplicant_set_bss_expiration_age(struct wpa_supplicant *wpa_s,
					  unsigned int expire_age);
int wpa_supplicant_set_bss_expiration_count(struct wpa_supplicant *wpa_s,
					    unsigned int expire_count);
int wpa_supplicant_set_scan_interval(struct wpa_supplicant *wpa_s,
				     int scan_interval);
int wpa_supplicant_set_debug_params(struct wpa_global *global,
				    int debug_level, int debug_timestamp,
				    int debug_show_keys);
void free_hw_features(struct wpa_supplicant *wpa_s);

void wpa_show_license(void);

struct wpa_interface * wpa_supplicant_match_iface(struct wpa_global *global,
						  const char *ifname);
struct wpa_supplicant * wpa_supplicant_add_iface(struct wpa_global *global,
						 struct wpa_interface *iface,
						 struct wpa_supplicant *parent);
int wpa_supplicant_remove_iface(struct wpa_global *global,
				struct wpa_supplicant *wpa_s,
				int terminate);
struct wpa_supplicant * wpa_supplicant_get_iface(struct wpa_global *global,
						 const char *ifname);
struct wpa_global * wpa_supplicant_init(struct wpa_params *params);
int wpa_supplicant_run(struct wpa_global *global);
void wpa_supplicant_deinit(struct wpa_global *global);

int wpa_supplicant_scard_init(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid);
void wpa_supplicant_terminate_proc(struct wpa_global *global);
void wpa_supplicant_rx_eapol(void *ctx, const u8 *own_addr,
			     const u8 *buf, size_t len,
			     enum frame_encryption encrypted);
void wpa_supplicant_update_config(struct wpa_supplicant *wpa_s);
void wpa_supplicant_clear_status(struct wpa_supplicant *wpa_s);
void wpas_connection_failed(struct wpa_supplicant *wpa_s, const u8 *bssid,
			    const u8 **link_bssids);
void fils_connection_failure(struct wpa_supplicant *wpa_s);
void fils_pmksa_cache_flush(struct wpa_supplicant *wpa_s);
int wpas_driver_bss_selection(struct wpa_supplicant *wpa_s);
int wpas_is_p2p_prioritized(struct wpa_supplicant *wpa_s);
void wpas_auth_failed(struct wpa_supplicant *wpa_s, const char *reason,
		      const u8 *bssid);
void wpas_clear_temp_disabled(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid, int clear_failures);
int disallowed_bssid(struct wpa_supplicant *wpa_s, const u8 *bssid);
int disallowed_ssid(struct wpa_supplicant *wpa_s, const u8 *ssid,
		    size_t ssid_len);
void wpas_request_connection(struct wpa_supplicant *wpa_s);
void wpas_request_disconnection(struct wpa_supplicant *wpa_s);
int wpas_build_ext_capab(struct wpa_supplicant *wpa_s, u8 *buf, size_t buflen,
			 struct wpa_bss *bss);
int wpas_update_random_addr(struct wpa_supplicant *wpa_s,
			    enum wpas_mac_addr_style style,
			    struct wpa_ssid *ssid);
int wpas_update_random_addr_disassoc(struct wpa_supplicant *wpa_s);
void add_freq(int *freqs, int *num_freqs, int freq);

int wpas_get_op_chan_phy(int freq, const u8 *ies, size_t ies_len,
			 u8 *op_class, u8 *chan, u8 *phy_type);

int wpas_twt_send_setup(struct wpa_supplicant *wpa_s, u8 dtok, int exponent,
			int mantissa, u8 min_twt, int setup_cmd, u64 twt,
			bool requestor, bool trigger, bool implicit,
			bool flow_type, u8 flow_id, bool protection,
			u8 twt_channel, u8 control);
int wpas_twt_send_teardown(struct wpa_supplicant *wpa_s, u8 flags);

void wpas_rrm_reset(struct wpa_supplicant *wpa_s);
void wpas_rrm_process_neighbor_rep(struct wpa_supplicant *wpa_s,
				   const u8 *report, size_t report_len);
int wpas_rrm_send_neighbor_rep_request(struct wpa_supplicant *wpa_s,
				       const struct wpa_ssid_value *ssid,
				       int lci, int civic,
				       void (*cb)(void *ctx,
						  struct wpabuf *neighbor_rep),
				       void *cb_ctx);
void wpas_rrm_handle_radio_measurement_request(struct wpa_supplicant *wpa_s,
					       const u8 *src, const u8 *dst,
					       const u8 *frame, size_t len);
void wpas_rrm_handle_link_measurement_request(struct wpa_supplicant *wpa_s,
					      const u8 *src,
					      const u8 *frame, size_t len,
					      int rssi);
void wpas_rrm_refuse_request(struct wpa_supplicant *wpa_s);
int wpas_beacon_rep_scan_process(struct wpa_supplicant *wpa_s,
				 struct wpa_scan_results *scan_res,
				 struct scan_info *info);
void wpas_clear_beacon_rep_data(struct wpa_supplicant *wpa_s);
void wpas_flush_fils_hlp_req(struct wpa_supplicant *wpa_s);
void wpas_clear_disabled_interface(void *eloop_ctx, void *timeout_ctx);
void wpa_supplicant_reset_bgscan(struct wpa_supplicant *wpa_s);


/* MBO functions */
int wpas_mbo_ie(struct wpa_supplicant *wpa_s, u8 *buf, size_t len,
		int add_oce_capa);
const u8 * mbo_attr_from_mbo_ie(const u8 *mbo_ie, enum mbo_attr_id attr);
const u8 * wpas_mbo_check_assoc_disallow(struct wpa_bss *bss);
void wpas_mbo_check_pmf(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			struct wpa_ssid *ssid);
const u8 * mbo_get_attr_from_ies(const u8 *ies, size_t ies_len,
				 enum mbo_attr_id attr);
int wpas_mbo_update_non_pref_chan(struct wpa_supplicant *wpa_s,
				  const char *non_pref_chan);
void wpas_mbo_scan_ie(struct wpa_supplicant *wpa_s, struct wpabuf *ie);
void wpas_mbo_ie_trans_req(struct wpa_supplicant *wpa_s, const u8 *ie,
			   size_t len);
size_t wpas_mbo_ie_bss_trans_reject(struct wpa_supplicant *wpa_s, u8 *pos,
				    size_t len,
				    enum mbo_transition_reject_reason reason);
void wpas_mbo_update_cell_capa(struct wpa_supplicant *wpa_s, u8 mbo_cell_capa);
struct wpabuf * mbo_build_anqp_buf(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *bss, u32 mbo_subtypes);
void mbo_parse_rx_anqp_resp(struct wpa_supplicant *wpa_s,
			    struct wpa_bss *bss, const u8 *sa,
			    const u8 *data, size_t slen);
void wpas_update_mbo_connect_params(struct wpa_supplicant *wpa_s);

/* op_classes.c */
enum chan_allowed {
	NOT_ALLOWED, NO_IR, RADAR, ALLOWED
};

enum chan_allowed verify_channel(struct hostapd_hw_modes *mode, u8 op_class,
				 u8 channel, u8 bw);
size_t wpas_supp_op_class_ie(struct wpa_supplicant *wpa_s,
			     struct wpa_ssid *ssid,
			     struct wpa_bss *bss, u8 *pos, size_t len);
int * wpas_supp_op_classes(struct wpa_supplicant *wpa_s);

int wpas_enable_mac_addr_randomization(struct wpa_supplicant *wpa_s,
				       unsigned int type, const u8 *addr,
				       const u8 *mask);
int wpas_disable_mac_addr_randomization(struct wpa_supplicant *wpa_s,
					unsigned int type);

/**
 * wpa_supplicant_ctrl_iface_ctrl_rsp_handle - Handle a control response
 * @wpa_s: Pointer to wpa_supplicant data
 * @ssid: Pointer to the network block the reply is for
 * @field: field the response is a reply for
 * @value: value (ie, password, etc) for @field
 * Returns: 0 on success, non-zero on error
 *
 * Helper function to handle replies to control interface requests.
 */
int wpa_supplicant_ctrl_iface_ctrl_rsp_handle(struct wpa_supplicant *wpa_s,
					      struct wpa_ssid *ssid,
					      const char *field,
					      const char *value);

void ibss_mesh_setup_freq(struct wpa_supplicant *wpa_s,
			  const struct wpa_ssid *ssid,
			  struct hostapd_freq_params *freq);

/* events.c */
void wpa_supplicant_mark_disassoc(struct wpa_supplicant *wpa_s);
int wpa_supplicant_connect(struct wpa_supplicant *wpa_s,
			   struct wpa_bss *selected,
			   struct wpa_ssid *ssid);
void wpa_supplicant_stop_countermeasures(void *eloop_ctx, void *sock_ctx);
void wpa_supplicant_delayed_mic_error_report(void *eloop_ctx, void *sock_ctx);
void wnm_bss_keep_alive_deinit(struct wpa_supplicant *wpa_s);
int wpa_supplicant_fast_associate(struct wpa_supplicant *wpa_s);
int wpa_wps_supplicant_fast_associate(struct wpa_supplicant *wpa_s);
struct wpa_bss * wpa_supplicant_pick_network(struct wpa_supplicant *wpa_s,
					     struct wpa_ssid **selected_ssid);
int wpas_temp_disabled(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);
void wpa_supplicant_update_channel_list(struct wpa_supplicant *wpa_s,
					struct channel_list_changed *info);
int wpa_supplicant_need_to_roam_within_ess(struct wpa_supplicant *wpa_s,
					   struct wpa_bss *current_bss,
					   struct wpa_bss *seleceted);
void wpas_reset_mlo_info(struct wpa_supplicant *wpa_s);

/* eap_register.c */
int eap_register_methods(void);

/**
 * Utility method to tell if a given network is for persistent group storage
 * @ssid: Network object
 * Returns: 1 if network is a persistent group, 0 otherwise
 */
static inline int network_is_persistent_group(struct wpa_ssid *ssid)
{
	return ssid->disabled == 2 && ssid->p2p_persistent_group;
}


static inline int wpas_mode_to_ieee80211_mode(enum wpas_mode mode)
{
	switch (mode) {
	default:
	case WPAS_MODE_INFRA:
		return IEEE80211_MODE_INFRA;
	case WPAS_MODE_IBSS:
		return IEEE80211_MODE_IBSS;
	case WPAS_MODE_AP:
	case WPAS_MODE_P2P_GO:
	case WPAS_MODE_P2P_GROUP_FORMATION:
		return IEEE80211_MODE_AP;
	case WPAS_MODE_MESH:
		return IEEE80211_MODE_MESH;
	}
}


int wpas_network_disabled(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);
int wpas_get_ssid_pmf(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid);
int pmf_in_use(struct wpa_supplicant *wpa_s, const u8 *addr);
void wpa_s_setup_sae_pt(struct wpa_config *conf, struct wpa_ssid *ssid,
			bool force);
void wpa_s_clear_sae_rejected(struct wpa_supplicant *wpa_s);

bool wpas_is_sae_avoided(struct wpa_supplicant *wpa_s,
			struct wpa_ssid *ssid,
			const struct wpa_ie_data *ie);

int wpas_init_ext_pw(struct wpa_supplicant *wpa_s);

void dump_freq_data(struct wpa_supplicant *wpa_s, const char *title,
		    struct wpa_used_freq_data *freqs_data,
		    unsigned int len);

int get_shared_radio_freqs_data(struct wpa_supplicant *wpa_s,
				struct wpa_used_freq_data *freqs_data,
				unsigned int len, bool exclude_current);
int get_shared_radio_freqs(struct wpa_supplicant *wpa_s,
			   int *freq_array, unsigned int len,
			   bool exclude_current);
int disabled_freq(struct wpa_supplicant *wpa_s, int freq);

void wpas_network_reenabled(void *eloop_ctx, void *timeout_ctx);

void wpas_vendor_elem_update(struct wpa_supplicant *wpa_s);
struct wpa_supplicant * wpas_vendor_elem(struct wpa_supplicant *wpa_s,
					 enum wpa_vendor_elem_frame frame);
int wpas_vendor_elem_remove(struct wpa_supplicant *wpa_s, int frame,
			    const u8 *elem, size_t len);

#ifdef CONFIG_FST

struct fst_wpa_obj;

void fst_wpa_supplicant_fill_iface_obj(struct wpa_supplicant *wpa_s,
				       struct fst_wpa_obj *iface_obj);

#endif /* CONFIG_FST */

int wpas_sched_scan_plans_set(struct wpa_supplicant *wpa_s, const char *cmd);

struct hostapd_hw_modes * get_mode(struct hostapd_hw_modes *modes,
				   u16 num_modes, enum hostapd_hw_mode mode,
				   bool is_6ghz);
struct hostapd_hw_modes * get_mode_with_freq(struct hostapd_hw_modes *modes,
					     u16 num_modes, int freq);

void wpa_bss_tmp_disallow(struct wpa_supplicant *wpa_s, const u8 *bssid,
			  unsigned int sec, int rssi_threshold);
int wpa_is_bss_tmp_disallowed(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *bss);
void free_bss_tmp_disallowed(struct wpa_supplicant *wpa_s);

struct wpa_ssid * wpa_scan_res_match(struct wpa_supplicant *wpa_s,
				     int i, struct wpa_bss *bss,
				     struct wpa_ssid *group,
				     int only_first_ssid, int debug_print);

int wpas_ctrl_iface_get_pref_freq_list_override(struct wpa_supplicant *wpa_s,
						enum wpa_driver_if_type if_type,
						unsigned int *num,
						struct weighted_pcl *freq_list);

int wpa_is_fils_supported(struct wpa_supplicant *wpa_s);
int wpa_is_fils_sk_pfs_supported(struct wpa_supplicant *wpa_s);

void wpas_clear_driver_signal_override(struct wpa_supplicant *wpa_s);

int wpas_send_mscs_req(struct wpa_supplicant *wpa_s);
void wpas_populate_mscs_descriptor_ie(struct robust_av_data *robust_av,
				      struct wpabuf *buf);
void wpas_handle_robust_av_recv_action(struct wpa_supplicant *wpa_s,
				       const u8 *src, const u8 *buf,
				       size_t len);
void wpas_handle_assoc_resp_mscs(struct wpa_supplicant *wpa_s, const u8 *bssid,
				 const u8 *ies, size_t ies_len);
int wpas_send_scs_req(struct wpa_supplicant *wpa_s);
void free_up_tclas_elem(struct scs_desc_elem *elem);
void free_up_scs_desc(struct scs_robust_av_data *data);
void wpas_handle_robust_av_scs_recv_action(struct wpa_supplicant *wpa_s,
					   const u8 *src, const u8 *buf,
					   size_t len);
void wpas_scs_deinit(struct wpa_supplicant *wpa_s);
void wpas_handle_qos_mgmt_recv_action(struct wpa_supplicant *wpa_s,
				      const u8 *src,
				      const u8 *buf, size_t len);
void wpas_dscp_deinit(struct wpa_supplicant *wpa_s);
int wpas_send_dscp_response(struct wpa_supplicant *wpa_s,
			    struct dscp_resp_data *resp_data);
void wpas_handle_assoc_resp_qos_mgmt(struct wpa_supplicant *wpa_s,
				     const u8 *ies, size_t ies_len);
int wpas_send_dscp_query(struct wpa_supplicant *wpa_s, const char *domain_name,
			 size_t domain_name_length);

int wpas_pasn_auth_start(struct wpa_supplicant *wpa_s, const u8 *own_addr,
			 const u8 *bssid, int akmp, int cipher,
			 u16 group, int network_id,
			 const u8 *comeback, size_t comeback_len);
void wpas_pasn_auth_stop(struct wpa_supplicant *wpa_s);
int wpas_pasn_auth_tx_status(struct wpa_supplicant *wpa_s,
			     const u8 *data, size_t data_len, u8 acked);
int wpas_pasn_auth_rx(struct wpa_supplicant *wpa_s,
		      const struct ieee80211_mgmt *mgmt, size_t len);

int wpas_pasn_deauthenticate(struct wpa_supplicant *wpa_s, const u8 *own_addr,
			     const u8 *bssid);
void wpas_pasn_auth_trigger(struct wpa_supplicant *wpa_s,
			    struct pasn_auth *pasn_auth);
void wpas_pasn_auth_work_done(struct wpa_supplicant *wpa_s, int status);
bool wpas_is_6ghz_supported(struct wpa_supplicant *wpa_s, bool only_enabled);

bool wpa_is_non_eht_scs_traffic_desc_supported(struct wpa_bss *bss);
bool wpas_ap_link_address(struct wpa_supplicant *wpa_s, const u8 *addr);

#endif /* WPA_SUPPLICANT_I_H */
