/*
 * Driver interaction with Linux nl80211/cfg80211 - definitions
 * Copyright (c) 2002-2014, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef DRIVER_NL80211_H
#define DRIVER_NL80211_H

#include "nl80211_copy.h"
#include "utils/list.h"
#include "driver.h"

#ifndef NL_CAPABILITY_VERSION_3_5_0
#define nla_nest_start(msg, attrtype) \
	nla_nest_start(msg, NLA_F_NESTED | (attrtype))
#endif

struct nl80211_global {
	void *ctx;
	struct dl_list interfaces;
	int if_add_ifindex;
	u64 if_add_wdevid;
	int if_add_wdevid_set;
	struct netlink_data *netlink;
	struct nl_cb *nl_cb;
	struct nl_sock *nl;
	int nl80211_id;
	unsigned int nl80211_maxattr;
	int nlctrl_id;
	int ioctl_sock; /* socket for ioctl() use */

	struct nl_sock *nl_event;
};

struct nl80211_wiphy_data {
	struct dl_list list;
	struct dl_list bsss;
	struct dl_list drvs;

	struct nl_sock *nl_beacons;
	struct nl_cb *nl_cb;

	int wiphy_idx;
};

#define NL80211_DRV_LINK_ID_NA (-1)

struct i802_link {
	unsigned int beacon_set:1;

	int freq;
	int bandwidth;
	u8 addr[ETH_ALEN];
	void *ctx;
};

struct i802_bss {
	struct wpa_driver_nl80211_data *drv;
	struct i802_bss *next;

	u16 valid_links;
	struct i802_link links[MAX_NUM_MLD_LINKS];
	struct i802_link *flink;

	int ifindex;
	int br_ifindex;
	u64 wdev_id;
	char ifname[IFNAMSIZ + 1];
	char brname[IFNAMSIZ];
	unsigned int added_if_into_bridge:1;
	unsigned int already_in_bridge:1;
	unsigned int added_bridge:1;
	unsigned int in_deinit:1;
	unsigned int wdev_id_set:1;
	unsigned int added_if:1;
	unsigned int static_ap:1;

	u8 addr[ETH_ALEN];
	u8 prev_addr[ETH_ALEN];

	int if_dynamic;

	void *ctx;
	struct nl_sock *nl_preq, *nl_mgmt, *nl_connect;
	struct nl_cb *nl_cb;

	struct nl80211_wiphy_data *wiphy_data;
	struct dl_list wiphy_list;
	u8 rand_addr[ETH_ALEN];
};

struct drv_nl80211_if_info {
	int ifindex;
	/* the AP/AP_VLAN iface that is in this bridge */
	int reason;
};

struct wpa_driver_nl80211_data {
	struct nl80211_global *global;
	struct dl_list list;
	struct dl_list wiphy_list;
	char phyname[32];
	unsigned int wiphy_idx;
	u8 perm_addr[ETH_ALEN];
	void *ctx;
	int ifindex;
	int if_removed;
	int if_disabled;
	int ignore_if_down_event;
	struct rfkill_data *rfkill;
	struct wpa_driver_capa capa;
	u8 *extended_capa, *extended_capa_mask;
	unsigned int extended_capa_len;
	struct drv_nl80211_iface_capa {
		enum nl80211_iftype iftype;
		u8 *ext_capa, *ext_capa_mask;
		unsigned int ext_capa_len;
		u16 eml_capa;
		u16 mld_capa_and_ops;
	} iface_capa[NL80211_IFTYPE_MAX];
	unsigned int num_iface_capa;

	int has_capability;
	int has_driver_key_mgmt;

	int operstate;

	int scan_complete_events;
	enum scan_states {
		NO_SCAN, SCAN_REQUESTED, SCAN_STARTED, SCAN_COMPLETED,
		SCAN_ABORTED, SCHED_SCAN_STARTED, SCHED_SCAN_STOPPED,
		SCHED_SCAN_RESULTS
	} scan_state;

	u8 auth_bssid[ETH_ALEN];
	u8 auth_attempt_bssid[ETH_ALEN];
	u8 bssid[ETH_ALEN];
	u8 prev_bssid[ETH_ALEN];
	int associated;
	struct driver_sta_mlo_info sta_mlo_info;
	u8 ssid[SSID_MAX_LEN];
	size_t ssid_len;
	enum nl80211_iftype nlmode;
	enum nl80211_iftype ap_scan_as_station;
	unsigned int assoc_freq;

	int monitor_sock;
	int monitor_ifidx;
	int monitor_refcount;

	unsigned int disabled_11b_rates:1;
	unsigned int pending_remain_on_chan:1;
	unsigned int in_interface_list:1;
	unsigned int device_ap_sme:1;
	unsigned int poll_command_supported:1;
	unsigned int data_tx_status:1;
	unsigned int scan_for_auth:1;
	unsigned int retry_auth:1;
	unsigned int use_monitor:1;
	unsigned int hostapd:1;
	unsigned int start_mode_sta:1;
	unsigned int start_iface_up:1;
	unsigned int test_use_roc_tx:1;
	unsigned int ignore_deauth_event:1;
	unsigned int vendor_cmd_test_avail:1;
	unsigned int roaming_vendor_cmd_avail:1;
	unsigned int dfs_vendor_cmd_avail:1;
	unsigned int have_low_prio_scan:1;
	unsigned int force_connect_cmd:1;
	unsigned int addr_changed:1;
	unsigned int get_features_vendor_cmd_avail:1;
	unsigned int set_rekey_offload:1;
	unsigned int p2p_go_ctwindow_supported:1;
	unsigned int setband_vendor_cmd_avail:1;
	unsigned int get_pref_freq_list:1;
	unsigned int set_prob_oper_freq:1;
	unsigned int scan_vendor_cmd_avail:1;
	unsigned int connect_reassoc:1;
	unsigned int set_wifi_conf_vendor_cmd_avail:1;
	unsigned int fetch_bss_trans_status:1;
	unsigned int roam_vendor_cmd_avail:1;
	unsigned int add_sta_node_vendor_cmd_avail:1;
	unsigned int control_port_ap:1;
	unsigned int multicast_registrations:1;
	unsigned int no_rrm:1;
	unsigned int get_sta_info_vendor_cmd_avail:1;
	unsigned int fils_discovery:1;
	unsigned int unsol_bcast_probe_resp:1;
	unsigned int qca_do_acs:1;
	unsigned int brcm_do_acs:1;
	unsigned int uses_6ghz:1;
	unsigned int uses_s1g:1;
	unsigned int secure_ranging_ctx_vendor_cmd_avail:1;
	unsigned int puncturing:1;
	unsigned int qca_ap_allowed_freqs:1;

	u32 ignore_next_local_disconnect;
	u32 ignore_next_local_deauth;

	u64 vendor_scan_cookie;
	u64 remain_on_chan_cookie;
	u64 send_frame_cookie;
	int send_frame_link_id;
#define MAX_SEND_FRAME_COOKIES 20
	u64 send_frame_cookies[MAX_SEND_FRAME_COOKIES];
	unsigned int num_send_frame_cookies;
	u64 eapol_tx_cookie;
	int eapol_tx_link_id;

	unsigned int last_mgmt_freq;

	struct wpa_driver_scan_filter *filter_ssids;
	size_t num_filter_ssids;

	struct i802_bss *first_bss;

	int eapol_tx_sock;

	int eapol_sock; /* socket for EAPOL frames */

	struct nl_sock *rtnl_sk; /* nl_sock for NETLINK_ROUTE */

	struct drv_nl80211_if_info default_if_indices[16];
	struct drv_nl80211_if_info *if_indices;
	int num_if_indices;

	/* From failed authentication command */
	int auth_freq;
	u8 auth_bssid_[ETH_ALEN];
	u8 auth_ssid[SSID_MAX_LEN];
	size_t auth_ssid_len;
	int auth_alg;
	u8 *auth_ie;
	size_t auth_ie_len;
	u8 *auth_data;
	size_t auth_data_len;
	u8 auth_wep_key[4][16];
	size_t auth_wep_key_len[4];
	int auth_wep_tx_keyidx;
	int auth_local_state_change;
	int auth_p2p;
	bool auth_mld;
	u8 auth_mld_link_id;
	u8 auth_ap_mld_addr[ETH_ALEN];

	/*
	 * Tells whether the last scan issued from wpa_supplicant was a normal
	 * scan (NL80211_CMD_TRIGGER_SCAN) or a vendor scan
	 * (NL80211_CMD_VENDOR). 0 if no pending scan request.
	 */
	int last_scan_cmd;
#ifdef CONFIG_DRIVER_NL80211_QCA
	bool roam_indication_done;
	u8 *pending_roam_data;
	size_t pending_roam_data_len;
	u8 *pending_t2lm_data;
	size_t pending_t2lm_data_len;
	u8 *pending_link_reconfig_data;
	size_t pending_link_reconfig_data_len;
#endif /* CONFIG_DRIVER_NL80211_QCA */
};

struct nl_msg;

struct nl80211_err_info {
	int link_id;
};

void * nl80211_cmd(struct wpa_driver_nl80211_data *drv,
		   struct nl_msg *msg, int flags, uint8_t cmd);
struct nl_msg * nl80211_cmd_msg(struct i802_bss *bss, int flags, uint8_t cmd);
struct nl_msg * nl80211_drv_msg(struct wpa_driver_nl80211_data *drv, int flags,
				uint8_t cmd);
struct nl_msg * nl80211_bss_msg(struct i802_bss *bss, int flags, uint8_t cmd);

int send_and_recv(struct nl80211_global *global,
		  struct nl_sock *nl_handle, struct nl_msg *msg,
		  int (*valid_handler)(struct nl_msg *, void *),
		  void *valid_data,
		  int (*ack_handler_custom)(struct nl_msg *, void *),
		  void *ack_data,
		  struct nl80211_err_info *err_info);

static inline int
send_and_recv_cmd(struct wpa_driver_nl80211_data *drv,
		  struct nl_msg *msg)
{
	return send_and_recv(drv->global, drv->global->nl, msg,
			     NULL, NULL, NULL, NULL, NULL);
}

static inline int
send_and_recv_resp(struct wpa_driver_nl80211_data *drv,
		   struct nl_msg *msg,
		   int (*valid_handler)(struct nl_msg *, void *),
		   void *valid_data)
{
	return send_and_recv(drv->global, drv->global->nl, msg,
			     valid_handler, valid_data, NULL, NULL, NULL);
}

int nl80211_create_iface(struct wpa_driver_nl80211_data *drv,
			 const char *ifname, enum nl80211_iftype iftype,
			 const u8 *addr, int wds,
			 int (*handler)(struct nl_msg *, void *),
			 void *arg, int use_existing);
void nl80211_remove_iface(struct wpa_driver_nl80211_data *drv, int ifidx);
unsigned int nl80211_get_assoc_freq(struct wpa_driver_nl80211_data *drv);
int nl80211_get_assoc_ssid(struct wpa_driver_nl80211_data *drv, u8 *ssid);
enum chan_width convert2width(int width);
void nl80211_mark_disconnected(struct wpa_driver_nl80211_data *drv);
struct i802_bss * get_bss_ifindex(struct wpa_driver_nl80211_data *drv,
				  int ifindex);
int is_ap_interface(enum nl80211_iftype nlmode);
int is_sta_interface(enum nl80211_iftype nlmode);
int wpa_driver_nl80211_authenticate_retry(struct wpa_driver_nl80211_data *drv);
int nl80211_get_link_signal(struct wpa_driver_nl80211_data *drv,
			    const u8 *bssid,
			    struct hostap_sta_driver_data *data);
int nl80211_get_link_noise(struct wpa_driver_nl80211_data *drv,
			   struct wpa_signal_info *sig_change);
int nl80211_get_wiphy_index(struct i802_bss *bss);
int wpa_driver_nl80211_set_mode(struct i802_bss *bss,
				enum nl80211_iftype nlmode);
int wpa_driver_nl80211_mlme(struct wpa_driver_nl80211_data *drv,
			    const u8 *addr, int cmd, u16 reason_code,
			    int local_state_change,
			    struct i802_bss *bss);

int nl80211_create_monitor_interface(struct wpa_driver_nl80211_data *drv);
void nl80211_remove_monitor_interface(struct wpa_driver_nl80211_data *drv);
int nl80211_send_monitor(struct wpa_driver_nl80211_data *drv,
			 const void *data, size_t len,
			 int encrypt, int noack);

int wpa_driver_nl80211_capa(struct wpa_driver_nl80211_data *drv);
struct hostapd_hw_modes *
nl80211_get_hw_feature_data(void *priv, u16 *num_modes, u16 *flags,
			    u8 *dfs_domain);

int process_global_event(struct nl_msg *msg, void *arg);
int process_bss_event(struct nl_msg *msg, void *arg);

const char * nl80211_iftype_str(enum nl80211_iftype mode);

void nl80211_restore_ap_mode(struct i802_bss *bss);
struct i802_link * nl80211_get_link(struct i802_bss *bss, s8 link_id);

static inline bool nl80211_link_valid(u16 links, s8 link_id)
{
	if (link_id < 0 || link_id >= MAX_NUM_MLD_LINKS)
		return false;

	if (links & BIT(link_id))
		return true;

	return false;
}


static inline bool
nl80211_attr_supported(struct wpa_driver_nl80211_data *drv, unsigned int attr)
{
	return attr <= drv->global->nl80211_maxattr;
}

#ifdef ANDROID
int android_nl_socket_set_nonblocking(struct nl_sock *handle);
int android_pno_start(struct i802_bss *bss,
		      struct wpa_driver_scan_params *params);
int android_pno_stop(struct i802_bss *bss);
extern int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf,
					 size_t buf_len);
extern int wpa_driver_nl80211_driver_event(struct wpa_driver_nl80211_data *drv,
					   u32 vendor_id, u32 subcmd,
					   u8 *data, size_t len);


#ifdef ANDROID_P2P
int wpa_driver_set_p2p_noa(void *priv, u8 count, int start, int duration);
int wpa_driver_get_p2p_noa(void *priv, u8 *buf, size_t len);
int wpa_driver_set_p2p_ps(void *priv, int legacy_ps, int opp_ps, int ctwindow);
int wpa_driver_set_ap_wps_p2p_ie(void *priv, const struct wpabuf *beacon,
				 const struct wpabuf *proberesp,
				 const struct wpabuf *assocresp);
#endif /* ANDROID_P2P */
#endif /* ANDROID */


/* driver_nl80211_scan.c */

void wpa_driver_nl80211_scan_timeout(void *eloop_ctx, void *timeout_ctx);
int wpa_driver_nl80211_scan(struct i802_bss *bss,
			    struct wpa_driver_scan_params *params);
int wpa_driver_nl80211_sched_scan(void *priv,
				  struct wpa_driver_scan_params *params);
int wpa_driver_nl80211_stop_sched_scan(void *priv);
struct wpa_scan_results * wpa_driver_nl80211_get_scan_results(void *priv,
							      const u8 *bssid);
void nl80211_dump_scan(struct wpa_driver_nl80211_data *drv);
int wpa_driver_nl80211_abort_scan(void *priv, u64 scan_cookie);
int wpa_driver_nl80211_vendor_scan(struct i802_bss *bss,
				   struct wpa_driver_scan_params *params);
int nl80211_set_default_scan_ies(void *priv, const u8 *ies, size_t ies_len);

#endif /* DRIVER_NL80211_H */
