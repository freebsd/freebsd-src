/*
 * Driver interaction with Linux nl80211/cfg80211
 * Copyright (c) 2002-2012, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <net/if.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/filter.h>
#include <linux/errqueue.h>
#include "nl80211_copy.h"

#include "common.h"
#include "eloop.h"
#include "utils/list.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "l2_packet/l2_packet.h"
#include "netlink.h"
#include "linux_ioctl.h"
#include "radiotap.h"
#include "radiotap_iter.h"
#include "rfkill.h"
#include "driver.h"

#ifndef SO_WIFI_STATUS
# if defined(__sparc__)
#  define SO_WIFI_STATUS	0x0025
# elif defined(__parisc__)
#  define SO_WIFI_STATUS	0x4022
# else
#  define SO_WIFI_STATUS	41
# endif

# define SCM_WIFI_STATUS	SO_WIFI_STATUS
#endif

#ifndef SO_EE_ORIGIN_TXSTATUS
#define SO_EE_ORIGIN_TXSTATUS	4
#endif

#ifndef PACKET_TX_TIMESTAMP
#define PACKET_TX_TIMESTAMP	16
#endif

#ifdef ANDROID
#include "android_drv.h"

/* system/core/libnl_2 in AOSP does not include nla_put_u32() */
int nla_put_u32(struct nl_msg *msg, int attrtype, uint32_t value)
{
	return nla_put(msg, attrtype, sizeof(uint32_t), &value);
}
#endif /* ANDROID */
#ifdef CONFIG_LIBNL20
/* libnl 2.0 compatibility code */
#define nl_handle nl_sock
#define nl80211_handle_alloc nl_socket_alloc_cb
#define nl80211_handle_destroy nl_socket_free
#else
/*
 * libnl 1.1 has a bug, it tries to allocate socket numbers densely
 * but when you free a socket again it will mess up its bitmap and
 * and use the wrong number the next time it needs a socket ID.
 * Therefore, we wrap the handle alloc/destroy and add our own pid
 * accounting.
 */
static uint32_t port_bitmap[32] = { 0 };

static struct nl_handle *nl80211_handle_alloc(void *cb)
{
	struct nl_handle *handle;
	uint32_t pid = getpid() & 0x3FFFFF;
	int i;

	handle = nl_handle_alloc_cb(cb);

	for (i = 0; i < 1024; i++) {
		if (port_bitmap[i / 32] & (1 << (i % 32)))
			continue;
		port_bitmap[i / 32] |= 1 << (i % 32);
		pid += i << 22;
		break;
	}

	nl_socket_set_local_port(handle, pid);

	return handle;
}

static void nl80211_handle_destroy(struct nl_handle *handle)
{
	uint32_t port = nl_socket_get_local_port(handle);

	port >>= 22;
	port_bitmap[port / 32] &= ~(1 << (port % 32));

	nl_handle_destroy(handle);
}
#endif /* CONFIG_LIBNL20 */


static struct nl_handle * nl_create_handle(struct nl_cb *cb, const char *dbg)
{
	struct nl_handle *handle;

	handle = nl80211_handle_alloc(cb);
	if (handle == NULL) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to allocate netlink "
			   "callbacks (%s)", dbg);
		return NULL;
	}

	if (genl_connect(handle)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to connect to generic "
			   "netlink (%s)", dbg);
		nl80211_handle_destroy(handle);
		return NULL;
	}

	return handle;
}


static void nl_destroy_handles(struct nl_handle **handle)
{
	if (*handle == NULL)
		return;
	nl80211_handle_destroy(*handle);
	*handle = NULL;
}


#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP   0x10000         /* driver signals L1 up         */
#endif
#ifndef IFF_DORMANT
#define IFF_DORMANT    0x20000         /* driver signals dormant       */
#endif

#ifndef IF_OPER_DORMANT
#define IF_OPER_DORMANT 5
#endif
#ifndef IF_OPER_UP
#define IF_OPER_UP 6
#endif

struct nl80211_global {
	struct dl_list interfaces;
	int if_add_ifindex;
	struct netlink_data *netlink;
	struct nl_cb *nl_cb;
	struct nl_handle *nl;
	int nl80211_id;
	int ioctl_sock; /* socket for ioctl() use */

	struct nl_handle *nl_event;
};

struct nl80211_wiphy_data {
	struct dl_list list;
	struct dl_list bsss;
	struct dl_list drvs;

	struct nl_handle *nl_beacons;
	struct nl_cb *nl_cb;

	int wiphy_idx;
};

static void nl80211_global_deinit(void *priv);
static void wpa_driver_nl80211_deinit(void *priv);

struct i802_bss {
	struct wpa_driver_nl80211_data *drv;
	struct i802_bss *next;
	int ifindex;
	char ifname[IFNAMSIZ + 1];
	char brname[IFNAMSIZ];
	unsigned int beacon_set:1;
	unsigned int added_if_into_bridge:1;
	unsigned int added_bridge:1;
	unsigned int in_deinit:1;

	u8 addr[ETH_ALEN];

	int freq;

	void *ctx;
	struct nl_handle *nl_preq, *nl_mgmt;
	struct nl_cb *nl_cb;

	struct nl80211_wiphy_data *wiphy_data;
	struct dl_list wiphy_list;
};

struct wpa_driver_nl80211_data {
	struct nl80211_global *global;
	struct dl_list list;
	struct dl_list wiphy_list;
	char phyname[32];
	void *ctx;
	int ifindex;
	int if_removed;
	int if_disabled;
	int ignore_if_down_event;
	struct rfkill_data *rfkill;
	struct wpa_driver_capa capa;
	int has_capability;

	int operstate;

	int scan_complete_events;

	struct nl_cb *nl_cb;

	u8 auth_bssid[ETH_ALEN];
	u8 bssid[ETH_ALEN];
	int associated;
	u8 ssid[32];
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
	unsigned int ignore_next_local_disconnect:1;

	u64 remain_on_chan_cookie;
	u64 send_action_cookie;

	unsigned int last_mgmt_freq;

	struct wpa_driver_scan_filter *filter_ssids;
	size_t num_filter_ssids;

	struct i802_bss first_bss;

	int eapol_tx_sock;

#ifdef HOSTAPD
	int eapol_sock; /* socket for EAPOL frames */

	int default_if_indices[16];
	int *if_indices;
	int num_if_indices;

	int last_freq;
	int last_freq_ht;
#endif /* HOSTAPD */

	/* From failed authentication command */
	int auth_freq;
	u8 auth_bssid_[ETH_ALEN];
	u8 auth_ssid[32];
	size_t auth_ssid_len;
	int auth_alg;
	u8 *auth_ie;
	size_t auth_ie_len;
	u8 auth_wep_key[4][16];
	size_t auth_wep_key_len[4];
	int auth_wep_tx_keyidx;
	int auth_local_state_change;
	int auth_p2p;
};


static void wpa_driver_nl80211_scan_timeout(void *eloop_ctx,
					    void *timeout_ctx);
static int wpa_driver_nl80211_set_mode(struct i802_bss *bss,
				       enum nl80211_iftype nlmode);
static int
wpa_driver_nl80211_finish_drv_init(struct wpa_driver_nl80211_data *drv);
static int wpa_driver_nl80211_mlme(struct wpa_driver_nl80211_data *drv,
				   const u8 *addr, int cmd, u16 reason_code,
				   int local_state_change);
static void nl80211_remove_monitor_interface(
	struct wpa_driver_nl80211_data *drv);
static int nl80211_send_frame_cmd(struct i802_bss *bss,
				  unsigned int freq, unsigned int wait,
				  const u8 *buf, size_t buf_len, u64 *cookie,
				  int no_cck, int no_ack, int offchanok);
static int wpa_driver_nl80211_probe_req_report(void *priv, int report);
#ifdef ANDROID
static int android_pno_start(struct i802_bss *bss,
			     struct wpa_driver_scan_params *params);
static int android_pno_stop(struct i802_bss *bss);
#endif /* ANDROID */

#ifdef HOSTAPD
static void add_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx);
static void del_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx);
static int have_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx);
static int wpa_driver_nl80211_if_remove(void *priv,
					enum wpa_driver_if_type type,
					const char *ifname);
#else /* HOSTAPD */
static inline void add_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx)
{
}

static inline void del_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx)
{
}

static inline int have_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx)
{
	return 0;
}
#endif /* HOSTAPD */

static int i802_set_freq(void *priv, struct hostapd_freq_params *freq);
static int nl80211_disable_11b_rates(struct wpa_driver_nl80211_data *drv,
				     int ifindex, int disabled);

static int nl80211_leave_ibss(struct wpa_driver_nl80211_data *drv);
static int wpa_driver_nl80211_authenticate_retry(
	struct wpa_driver_nl80211_data *drv);


static int is_ap_interface(enum nl80211_iftype nlmode)
{
	return (nlmode == NL80211_IFTYPE_AP ||
		nlmode == NL80211_IFTYPE_P2P_GO);
}


static int is_sta_interface(enum nl80211_iftype nlmode)
{
	return (nlmode == NL80211_IFTYPE_STATION ||
		nlmode == NL80211_IFTYPE_P2P_CLIENT);
}


static int is_p2p_interface(enum nl80211_iftype nlmode)
{
	return (nlmode == NL80211_IFTYPE_P2P_CLIENT ||
		nlmode == NL80211_IFTYPE_P2P_GO);
}


struct nl80211_bss_info_arg {
	struct wpa_driver_nl80211_data *drv;
	struct wpa_scan_results *res;
	unsigned int assoc_freq;
	u8 assoc_bssid[ETH_ALEN];
};

static int bss_info_handler(struct nl_msg *msg, void *arg);


/* nl80211 code */
static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *err = arg;
	*err = 0;
	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *ret = arg;
	*ret = 0;
	return NL_SKIP;
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
			 void *arg)
{
	int *ret = arg;
	*ret = err->error;
	return NL_SKIP;
}


static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}


static int send_and_recv(struct nl80211_global *global,
			 struct nl_handle *nl_handle, struct nl_msg *msg,
			 int (*valid_handler)(struct nl_msg *, void *),
			 void *valid_data)
{
	struct nl_cb *cb;
	int err = -ENOMEM;

	cb = nl_cb_clone(global->nl_cb);
	if (!cb)
		goto out;

	err = nl_send_auto_complete(nl_handle, msg);
	if (err < 0)
		goto out;

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

	if (valid_handler)
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
			  valid_handler, valid_data);

	while (err > 0)
		nl_recvmsgs(nl_handle, cb);
 out:
	nl_cb_put(cb);
	nlmsg_free(msg);
	return err;
}


static int send_and_recv_msgs_global(struct nl80211_global *global,
				     struct nl_msg *msg,
				     int (*valid_handler)(struct nl_msg *, void *),
				     void *valid_data)
{
	return send_and_recv(global, global->nl, msg, valid_handler,
			     valid_data);
}


static int send_and_recv_msgs(struct wpa_driver_nl80211_data *drv,
			      struct nl_msg *msg,
			      int (*valid_handler)(struct nl_msg *, void *),
			      void *valid_data)
{
	return send_and_recv(drv->global, drv->global->nl, msg,
			     valid_handler, valid_data);
}


struct family_data {
	const char *group;
	int id;
};


static int family_handler(struct nl_msg *msg, void *arg)
{
	struct family_data *res = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int i;

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], i) {
		struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];
		nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX, nla_data(mcgrp),
			  nla_len(mcgrp), NULL);
		if (!tb2[CTRL_ATTR_MCAST_GRP_NAME] ||
		    !tb2[CTRL_ATTR_MCAST_GRP_ID] ||
		    os_strncmp(nla_data(tb2[CTRL_ATTR_MCAST_GRP_NAME]),
			       res->group,
			       nla_len(tb2[CTRL_ATTR_MCAST_GRP_NAME])) != 0)
			continue;
		res->id = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	};

	return NL_SKIP;
}


static int nl_get_multicast_id(struct nl80211_global *global,
			       const char *family, const char *group)
{
	struct nl_msg *msg;
	int ret = -1;
	struct family_data res = { group, -ENOENT };

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;
	genlmsg_put(msg, 0, 0, genl_ctrl_resolve(global->nl, "nlctrl"),
		    0, 0, CTRL_CMD_GETFAMILY, 0);
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = send_and_recv_msgs_global(global, msg, family_handler, &res);
	msg = NULL;
	if (ret == 0)
		ret = res.id;

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static void * nl80211_cmd(struct wpa_driver_nl80211_data *drv,
			  struct nl_msg *msg, int flags, uint8_t cmd)
{
	return genlmsg_put(msg, 0, 0, drv->global->nl80211_id,
			   0, flags, cmd, 0);
}


struct wiphy_idx_data {
	int wiphy_idx;
};


static int netdev_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wiphy_idx_data *info = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_WIPHY])
		info->wiphy_idx = nla_get_u32(tb[NL80211_ATTR_WIPHY]);

	return NL_SKIP;
}


static int nl80211_get_wiphy_index(struct i802_bss *bss)
{
	struct nl_msg *msg;
	struct wiphy_idx_data data = {
		.wiphy_idx = -1,
	};

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(bss->drv, msg, 0, NL80211_CMD_GET_INTERFACE);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);

	if (send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data) == 0)
		return data.wiphy_idx;
	msg = NULL;
nla_put_failure:
	nlmsg_free(msg);
	return -1;
}


static int nl80211_register_beacons(struct wpa_driver_nl80211_data *drv,
				    struct nl80211_wiphy_data *w)
{
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_REGISTER_BEACONS);

	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY, w->wiphy_idx);

	ret = send_and_recv(drv->global, w->nl_beacons, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Register beacons command "
			   "failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto nla_put_failure;
	}
	ret = 0;
nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static void nl80211_recv_beacons(int sock, void *eloop_ctx, void *handle)
{
	struct nl80211_wiphy_data *w = eloop_ctx;

	wpa_printf(MSG_EXCESSIVE, "nl80211: Beacon event message available");

	nl_recvmsgs(handle, w->nl_cb);
}


static int process_beacon_event(struct nl_msg *msg, void *arg)
{
	struct nl80211_wiphy_data *w = arg;
	struct wpa_driver_nl80211_data *drv;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	union wpa_event_data event;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (gnlh->cmd != NL80211_CMD_FRAME) {
		wpa_printf(MSG_DEBUG, "nl80211: Unexpected beacon event? (%d)",
			   gnlh->cmd);
		return NL_SKIP;
	}

	if (!tb[NL80211_ATTR_FRAME])
		return NL_SKIP;

	dl_list_for_each(drv, &w->drvs, struct wpa_driver_nl80211_data,
			 wiphy_list) {
		os_memset(&event, 0, sizeof(event));
		event.rx_mgmt.frame = nla_data(tb[NL80211_ATTR_FRAME]);
		event.rx_mgmt.frame_len = nla_len(tb[NL80211_ATTR_FRAME]);
		wpa_supplicant_event(drv->ctx, EVENT_RX_MGMT, &event);
	}

	return NL_SKIP;
}


static struct nl80211_wiphy_data *
nl80211_get_wiphy_data_ap(struct i802_bss *bss)
{
	static DEFINE_DL_LIST(nl80211_wiphys);
	struct nl80211_wiphy_data *w;
	int wiphy_idx, found = 0;
	struct i802_bss *tmp_bss;

	if (bss->wiphy_data != NULL)
		return bss->wiphy_data;

	wiphy_idx = nl80211_get_wiphy_index(bss);

	dl_list_for_each(w, &nl80211_wiphys, struct nl80211_wiphy_data, list) {
		if (w->wiphy_idx == wiphy_idx)
			goto add;
	}

	/* alloc new one */
	w = os_zalloc(sizeof(*w));
	if (w == NULL)
		return NULL;
	w->wiphy_idx = wiphy_idx;
	dl_list_init(&w->bsss);
	dl_list_init(&w->drvs);

	w->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!w->nl_cb) {
		os_free(w);
		return NULL;
	}
	nl_cb_set(w->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_set(w->nl_cb, NL_CB_VALID, NL_CB_CUSTOM, process_beacon_event,
		  w);

	w->nl_beacons = nl_create_handle(bss->drv->global->nl_cb,
					 "wiphy beacons");
	if (w->nl_beacons == NULL) {
		os_free(w);
		return NULL;
	}

	if (nl80211_register_beacons(bss->drv, w)) {
		nl_destroy_handles(&w->nl_beacons);
		os_free(w);
		return NULL;
	}

	eloop_register_read_sock(nl_socket_get_fd(w->nl_beacons),
				 nl80211_recv_beacons, w, w->nl_beacons);

	dl_list_add(&nl80211_wiphys, &w->list);

add:
	/* drv entry for this bss already there? */
	dl_list_for_each(tmp_bss, &w->bsss, struct i802_bss, wiphy_list) {
		if (tmp_bss->drv == bss->drv) {
			found = 1;
			break;
		}
	}
	/* if not add it */
	if (!found)
		dl_list_add(&w->drvs, &bss->drv->wiphy_list);

	dl_list_add(&w->bsss, &bss->wiphy_list);
	bss->wiphy_data = w;
	return w;
}


static void nl80211_put_wiphy_data_ap(struct i802_bss *bss)
{
	struct nl80211_wiphy_data *w = bss->wiphy_data;
	struct i802_bss *tmp_bss;
	int found = 0;

	if (w == NULL)
		return;
	bss->wiphy_data = NULL;
	dl_list_del(&bss->wiphy_list);

	/* still any for this drv present? */
	dl_list_for_each(tmp_bss, &w->bsss, struct i802_bss, wiphy_list) {
		if (tmp_bss->drv == bss->drv) {
			found = 1;
			break;
		}
	}
	/* if not remove it */
	if (!found)
		dl_list_del(&bss->drv->wiphy_list);

	if (!dl_list_empty(&w->bsss))
		return;

	eloop_unregister_read_sock(nl_socket_get_fd(w->nl_beacons));

	nl_cb_put(w->nl_cb);
	nl_destroy_handles(&w->nl_beacons);
	dl_list_del(&w->list);
	os_free(w);
}


static int wpa_driver_nl80211_get_bssid(void *priv, u8 *bssid)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (!drv->associated)
		return -1;
	os_memcpy(bssid, drv->bssid, ETH_ALEN);
	return 0;
}


static int wpa_driver_nl80211_get_ssid(void *priv, u8 *ssid)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (!drv->associated)
		return -1;
	os_memcpy(ssid, drv->ssid, drv->ssid_len);
	return drv->ssid_len;
}


static void wpa_driver_nl80211_event_link(struct wpa_driver_nl80211_data *drv,
					  char *buf, size_t len, int del)
{
	union wpa_event_data event;

	os_memset(&event, 0, sizeof(event));
	if (len > sizeof(event.interface_status.ifname))
		len = sizeof(event.interface_status.ifname) - 1;
	os_memcpy(event.interface_status.ifname, buf, len);
	event.interface_status.ievent = del ? EVENT_INTERFACE_REMOVED :
		EVENT_INTERFACE_ADDED;

	wpa_printf(MSG_DEBUG, "RTM_%sLINK, IFLA_IFNAME: Interface '%s' %s",
		   del ? "DEL" : "NEW",
		   event.interface_status.ifname,
		   del ? "removed" : "added");

	if (os_strcmp(drv->first_bss.ifname, event.interface_status.ifname) == 0) {
		if (del) {
			if (drv->if_removed) {
				wpa_printf(MSG_DEBUG, "nl80211: if_removed "
					   "already set - ignore event");
				return;
			}
			drv->if_removed = 1;
		} else {
			if (if_nametoindex(drv->first_bss.ifname) == 0) {
				wpa_printf(MSG_DEBUG, "nl80211: Interface %s "
					   "does not exist - ignore "
					   "RTM_NEWLINK",
					   drv->first_bss.ifname);
				return;
			}
			if (!drv->if_removed) {
				wpa_printf(MSG_DEBUG, "nl80211: if_removed "
					   "already cleared - ignore event");
				return;
			}
			drv->if_removed = 0;
		}
	}

	wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS, &event);
}


static int wpa_driver_nl80211_own_ifname(struct wpa_driver_nl80211_data *drv,
					 u8 *buf, size_t len)
{
	int attrlen, rta_len;
	struct rtattr *attr;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			if (os_strcmp(((char *) attr) + rta_len, drv->first_bss.ifname)
			    == 0)
				return 1;
			else
				break;
		}
		attr = RTA_NEXT(attr, attrlen);
	}

	return 0;
}


static int wpa_driver_nl80211_own_ifindex(struct wpa_driver_nl80211_data *drv,
					  int ifindex, u8 *buf, size_t len)
{
	if (drv->ifindex == ifindex)
		return 1;

	if (drv->if_removed && wpa_driver_nl80211_own_ifname(drv, buf, len)) {
		drv->first_bss.ifindex = if_nametoindex(drv->first_bss.ifname);
		wpa_printf(MSG_DEBUG, "nl80211: Update ifindex for a removed "
			   "interface");
		wpa_driver_nl80211_finish_drv_init(drv);
		return 1;
	}

	return 0;
}


static struct wpa_driver_nl80211_data *
nl80211_find_drv(struct nl80211_global *global, int idx, u8 *buf, size_t len)
{
	struct wpa_driver_nl80211_data *drv;
	dl_list_for_each(drv, &global->interfaces,
			 struct wpa_driver_nl80211_data, list) {
		if (wpa_driver_nl80211_own_ifindex(drv, idx, buf, len) ||
		    have_ifidx(drv, idx))
			return drv;
	}
	return NULL;
}


static void wpa_driver_nl80211_event_rtm_newlink(void *ctx,
						 struct ifinfomsg *ifi,
						 u8 *buf, size_t len)
{
	struct nl80211_global *global = ctx;
	struct wpa_driver_nl80211_data *drv;
	int attrlen, rta_len;
	struct rtattr *attr;
	u32 brid = 0;
	char namebuf[IFNAMSIZ];

	drv = nl80211_find_drv(global, ifi->ifi_index, buf, len);
	if (!drv) {
		wpa_printf(MSG_DEBUG, "nl80211: Ignore event for foreign "
			   "ifindex %d", ifi->ifi_index);
		return;
	}

	wpa_printf(MSG_DEBUG, "RTM_NEWLINK: operstate=%d ifi_flags=0x%x "
		   "(%s%s%s%s)",
		   drv->operstate, ifi->ifi_flags,
		   (ifi->ifi_flags & IFF_UP) ? "[UP]" : "",
		   (ifi->ifi_flags & IFF_RUNNING) ? "[RUNNING]" : "",
		   (ifi->ifi_flags & IFF_LOWER_UP) ? "[LOWER_UP]" : "",
		   (ifi->ifi_flags & IFF_DORMANT) ? "[DORMANT]" : "");

	if (!drv->if_disabled && !(ifi->ifi_flags & IFF_UP)) {
		if (if_indextoname(ifi->ifi_index, namebuf) &&
		    linux_iface_up(drv->global->ioctl_sock,
				   drv->first_bss.ifname) > 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface down "
				   "event since interface %s is up", namebuf);
			return;
		}
		wpa_printf(MSG_DEBUG, "nl80211: Interface down");
		if (drv->ignore_if_down_event) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface down "
				   "event generated by mode change");
			drv->ignore_if_down_event = 0;
		} else {
			drv->if_disabled = 1;
			wpa_supplicant_event(drv->ctx,
					     EVENT_INTERFACE_DISABLED, NULL);
		}
	}

	if (drv->if_disabled && (ifi->ifi_flags & IFF_UP)) {
		if (if_indextoname(ifi->ifi_index, namebuf) &&
		    linux_iface_up(drv->global->ioctl_sock,
				   drv->first_bss.ifname) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s is down",
				   namebuf);
		} else if (if_nametoindex(drv->first_bss.ifname) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s does not exist",
				   drv->first_bss.ifname);
		} else if (drv->if_removed) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s is marked "
				   "removed", drv->first_bss.ifname);
		} else {
			wpa_printf(MSG_DEBUG, "nl80211: Interface up");
			drv->if_disabled = 0;
			wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_ENABLED,
					     NULL);
		}
	}

	/*
	 * Some drivers send the association event before the operup event--in
	 * this case, lifting operstate in wpa_driver_nl80211_set_operstate()
	 * fails. This will hit us when wpa_supplicant does not need to do
	 * IEEE 802.1X authentication
	 */
	if (drv->operstate == 1 &&
	    (ifi->ifi_flags & (IFF_LOWER_UP | IFF_DORMANT)) == IFF_LOWER_UP &&
	    !(ifi->ifi_flags & IFF_RUNNING))
		netlink_send_oper_ifla(drv->global->netlink, drv->ifindex,
				       -1, IF_OPER_UP);

	attrlen = len;
	attr = (struct rtattr *) buf;
	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			wpa_driver_nl80211_event_link(
				drv,
				((char *) attr) + rta_len,
				attr->rta_len - rta_len, 0);
		} else if (attr->rta_type == IFLA_MASTER)
			brid = nla_get_u32((struct nlattr *) attr);
		attr = RTA_NEXT(attr, attrlen);
	}

	if (ifi->ifi_family == AF_BRIDGE && brid) {
		/* device has been added to bridge */
		if_indextoname(brid, namebuf);
		wpa_printf(MSG_DEBUG, "nl80211: Add ifindex %u for bridge %s",
			   brid, namebuf);
		add_ifidx(drv, brid);
	}
}


static void wpa_driver_nl80211_event_rtm_dellink(void *ctx,
						 struct ifinfomsg *ifi,
						 u8 *buf, size_t len)
{
	struct nl80211_global *global = ctx;
	struct wpa_driver_nl80211_data *drv;
	int attrlen, rta_len;
	struct rtattr *attr;
	u32 brid = 0;

	drv = nl80211_find_drv(global, ifi->ifi_index, buf, len);
	if (!drv) {
		wpa_printf(MSG_DEBUG, "nl80211: Ignore dellink event for "
			   "foreign ifindex %d", ifi->ifi_index);
		return;
	}

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			wpa_driver_nl80211_event_link(
				drv,
				((char *) attr) + rta_len,
				attr->rta_len - rta_len, 1);
		} else if (attr->rta_type == IFLA_MASTER)
			brid = nla_get_u32((struct nlattr *) attr);
		attr = RTA_NEXT(attr, attrlen);
	}

	if (ifi->ifi_family == AF_BRIDGE && brid) {
		/* device has been removed from bridge */
		char namebuf[IFNAMSIZ];
		if_indextoname(brid, namebuf);
		wpa_printf(MSG_DEBUG, "nl80211: Remove ifindex %u for bridge "
			   "%s", brid, namebuf);
		del_ifidx(drv, brid);
	}
}


static void mlme_event_auth(struct wpa_driver_nl80211_data *drv,
			    const u8 *frame, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;

	wpa_printf(MSG_DEBUG, "nl80211: Authenticate event");
	mgmt = (const struct ieee80211_mgmt *) frame;
	if (len < 24 + sizeof(mgmt->u.auth)) {
		wpa_printf(MSG_DEBUG, "nl80211: Too short association event "
			   "frame");
		return;
	}

	os_memcpy(drv->auth_bssid, mgmt->sa, ETH_ALEN);
	os_memset(&event, 0, sizeof(event));
	os_memcpy(event.auth.peer, mgmt->sa, ETH_ALEN);
	event.auth.auth_type = le_to_host16(mgmt->u.auth.auth_alg);
	event.auth.auth_transaction =
		le_to_host16(mgmt->u.auth.auth_transaction);
	event.auth.status_code = le_to_host16(mgmt->u.auth.status_code);
	if (len > 24 + sizeof(mgmt->u.auth)) {
		event.auth.ies = mgmt->u.auth.variable;
		event.auth.ies_len = len - 24 - sizeof(mgmt->u.auth);
	}

	wpa_supplicant_event(drv->ctx, EVENT_AUTH, &event);
}


static unsigned int nl80211_get_assoc_freq(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	int ret;
	struct nl80211_bss_info_arg arg;

	os_memset(&arg, 0, sizeof(arg));
	msg = nlmsg_alloc();
	if (!msg)
		goto nla_put_failure;

	nl80211_cmd(drv, msg, NLM_F_DUMP, NL80211_CMD_GET_SCAN);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	arg.drv = drv;
	ret = send_and_recv_msgs(drv, msg, bss_info_handler, &arg);
	msg = NULL;
	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Operating frequency for the "
			   "associated BSS from scan results: %u MHz",
			   arg.assoc_freq);
		return arg.assoc_freq ? arg.assoc_freq : drv->assoc_freq;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Scan result fetch failed: ret=%d "
		   "(%s)", ret, strerror(-ret));
nla_put_failure:
	nlmsg_free(msg);
	return drv->assoc_freq;
}


static void mlme_event_assoc(struct wpa_driver_nl80211_data *drv,
			    const u8 *frame, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;
	u16 status;

	wpa_printf(MSG_DEBUG, "nl80211: Associate event");
	mgmt = (const struct ieee80211_mgmt *) frame;
	if (len < 24 + sizeof(mgmt->u.assoc_resp)) {
		wpa_printf(MSG_DEBUG, "nl80211: Too short association event "
			   "frame");
		return;
	}

	status = le_to_host16(mgmt->u.assoc_resp.status_code);
	if (status != WLAN_STATUS_SUCCESS) {
		os_memset(&event, 0, sizeof(event));
		event.assoc_reject.bssid = mgmt->bssid;
		if (len > 24 + sizeof(mgmt->u.assoc_resp)) {
			event.assoc_reject.resp_ies =
				(u8 *) mgmt->u.assoc_resp.variable;
			event.assoc_reject.resp_ies_len =
				len - 24 - sizeof(mgmt->u.assoc_resp);
		}
		event.assoc_reject.status_code = status;

		wpa_supplicant_event(drv->ctx, EVENT_ASSOC_REJECT, &event);
		return;
	}

	drv->associated = 1;
	os_memcpy(drv->bssid, mgmt->sa, ETH_ALEN);

	os_memset(&event, 0, sizeof(event));
	if (len > 24 + sizeof(mgmt->u.assoc_resp)) {
		event.assoc_info.resp_ies = (u8 *) mgmt->u.assoc_resp.variable;
		event.assoc_info.resp_ies_len =
			len - 24 - sizeof(mgmt->u.assoc_resp);
	}

	event.assoc_info.freq = drv->assoc_freq;

	wpa_supplicant_event(drv->ctx, EVENT_ASSOC, &event);
}


static void mlme_event_connect(struct wpa_driver_nl80211_data *drv,
			       enum nl80211_commands cmd, struct nlattr *status,
			       struct nlattr *addr, struct nlattr *req_ie,
			       struct nlattr *resp_ie)
{
	union wpa_event_data event;

	if (drv->capa.flags & WPA_DRIVER_FLAGS_SME) {
		/*
		 * Avoid reporting two association events that would confuse
		 * the core code.
		 */
		wpa_printf(MSG_DEBUG, "nl80211: Ignore connect event (cmd=%d) "
			   "when using userspace SME", cmd);
		return;
	}

	if (cmd == NL80211_CMD_CONNECT)
		wpa_printf(MSG_DEBUG, "nl80211: Connect event");
	else if (cmd == NL80211_CMD_ROAM)
		wpa_printf(MSG_DEBUG, "nl80211: Roam event");

	os_memset(&event, 0, sizeof(event));
	if (cmd == NL80211_CMD_CONNECT &&
	    nla_get_u16(status) != WLAN_STATUS_SUCCESS) {
		if (addr)
			event.assoc_reject.bssid = nla_data(addr);
		if (resp_ie) {
			event.assoc_reject.resp_ies = nla_data(resp_ie);
			event.assoc_reject.resp_ies_len = nla_len(resp_ie);
		}
		event.assoc_reject.status_code = nla_get_u16(status);
		wpa_supplicant_event(drv->ctx, EVENT_ASSOC_REJECT, &event);
		return;
	}

	drv->associated = 1;
	if (addr)
		os_memcpy(drv->bssid, nla_data(addr), ETH_ALEN);

	if (req_ie) {
		event.assoc_info.req_ies = nla_data(req_ie);
		event.assoc_info.req_ies_len = nla_len(req_ie);
	}
	if (resp_ie) {
		event.assoc_info.resp_ies = nla_data(resp_ie);
		event.assoc_info.resp_ies_len = nla_len(resp_ie);
	}

	event.assoc_info.freq = nl80211_get_assoc_freq(drv);

	wpa_supplicant_event(drv->ctx, EVENT_ASSOC, &event);
}


static void mlme_event_disconnect(struct wpa_driver_nl80211_data *drv,
				  struct nlattr *reason, struct nlattr *addr,
				  struct nlattr *by_ap)
{
	union wpa_event_data data;
	unsigned int locally_generated = by_ap == NULL;

	if (drv->capa.flags & WPA_DRIVER_FLAGS_SME) {
		/*
		 * Avoid reporting two disassociation events that could
		 * confuse the core code.
		 */
		wpa_printf(MSG_DEBUG, "nl80211: Ignore disconnect "
			   "event when using userspace SME");
		return;
	}

	if (drv->ignore_next_local_disconnect) {
		drv->ignore_next_local_disconnect = 0;
		if (locally_generated) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore disconnect "
				   "event triggered during reassociation");
			return;
		}
		wpa_printf(MSG_WARNING, "nl80211: Was expecting local "
			   "disconnect but got another disconnect "
			   "event first");
	}

	wpa_printf(MSG_DEBUG, "nl80211: Disconnect event");
	drv->associated = 0;
	os_memset(&data, 0, sizeof(data));
	if (reason)
		data.deauth_info.reason_code = nla_get_u16(reason);
	data.deauth_info.locally_generated = by_ap == NULL;
	wpa_supplicant_event(drv->ctx, EVENT_DEAUTH, &data);
}


static void mlme_event_ch_switch(struct wpa_driver_nl80211_data *drv,
				 struct nlattr *freq, struct nlattr *type)
{
	union wpa_event_data data;
	int ht_enabled = 1;
	int chan_offset = 0;

	wpa_printf(MSG_DEBUG, "nl80211: Channel switch event");

	if (!freq || !type)
		return;

	switch (nla_get_u32(type)) {
	case NL80211_CHAN_NO_HT:
		ht_enabled = 0;
		break;
	case NL80211_CHAN_HT20:
		break;
	case NL80211_CHAN_HT40PLUS:
		chan_offset = 1;
		break;
	case NL80211_CHAN_HT40MINUS:
		chan_offset = -1;
		break;
	}

	data.ch_switch.freq = nla_get_u32(freq);
	data.ch_switch.ht_enabled = ht_enabled;
	data.ch_switch.ch_offset = chan_offset;

	wpa_supplicant_event(drv->ctx, EVENT_CH_SWITCH, &data);
}


static void mlme_timeout_event(struct wpa_driver_nl80211_data *drv,
			       enum nl80211_commands cmd, struct nlattr *addr)
{
	union wpa_event_data event;
	enum wpa_event_type ev;

	if (nla_len(addr) != ETH_ALEN)
		return;

	wpa_printf(MSG_DEBUG, "nl80211: MLME event %d; timeout with " MACSTR,
		   cmd, MAC2STR((u8 *) nla_data(addr)));

	if (cmd == NL80211_CMD_AUTHENTICATE)
		ev = EVENT_AUTH_TIMED_OUT;
	else if (cmd == NL80211_CMD_ASSOCIATE)
		ev = EVENT_ASSOC_TIMED_OUT;
	else
		return;

	os_memset(&event, 0, sizeof(event));
	os_memcpy(event.timeout_event.addr, nla_data(addr), ETH_ALEN);
	wpa_supplicant_event(drv->ctx, ev, &event);
}


static void mlme_event_mgmt(struct wpa_driver_nl80211_data *drv,
			    struct nlattr *freq, struct nlattr *sig,
			    const u8 *frame, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;
	u16 fc, stype;
	int ssi_signal = 0;

	wpa_printf(MSG_DEBUG, "nl80211: Frame event");
	mgmt = (const struct ieee80211_mgmt *) frame;
	if (len < 24) {
		wpa_printf(MSG_DEBUG, "nl80211: Too short action frame");
		return;
	}

	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);

	if (sig)
		ssi_signal = (s32) nla_get_u32(sig);

	os_memset(&event, 0, sizeof(event));
	if (freq) {
		event.rx_action.freq = nla_get_u32(freq);
		drv->last_mgmt_freq = event.rx_action.freq;
	}
	if (stype == WLAN_FC_STYPE_ACTION) {
		event.rx_action.da = mgmt->da;
		event.rx_action.sa = mgmt->sa;
		event.rx_action.bssid = mgmt->bssid;
		event.rx_action.category = mgmt->u.action.category;
		event.rx_action.data = &mgmt->u.action.category + 1;
		event.rx_action.len = frame + len - event.rx_action.data;
		wpa_supplicant_event(drv->ctx, EVENT_RX_ACTION, &event);
	} else {
		event.rx_mgmt.frame = frame;
		event.rx_mgmt.frame_len = len;
		event.rx_mgmt.ssi_signal = ssi_signal;
		wpa_supplicant_event(drv->ctx, EVENT_RX_MGMT, &event);
	}
}


static void mlme_event_mgmt_tx_status(struct wpa_driver_nl80211_data *drv,
				      struct nlattr *cookie, const u8 *frame,
				      size_t len, struct nlattr *ack)
{
	union wpa_event_data event;
	const struct ieee80211_hdr *hdr;
	u16 fc;

	wpa_printf(MSG_DEBUG, "nl80211: Frame TX status event");
	if (!is_ap_interface(drv->nlmode)) {
		u64 cookie_val;

		if (!cookie)
			return;

		cookie_val = nla_get_u64(cookie);
		wpa_printf(MSG_DEBUG, "nl80211: Action TX status:"
			   " cookie=0%llx%s (ack=%d)",
			   (long long unsigned int) cookie_val,
			   cookie_val == drv->send_action_cookie ?
			   " (match)" : " (unknown)", ack != NULL);
		if (cookie_val != drv->send_action_cookie)
			return;
	}

	hdr = (const struct ieee80211_hdr *) frame;
	fc = le_to_host16(hdr->frame_control);

	os_memset(&event, 0, sizeof(event));
	event.tx_status.type = WLAN_FC_GET_TYPE(fc);
	event.tx_status.stype = WLAN_FC_GET_STYPE(fc);
	event.tx_status.dst = hdr->addr1;
	event.tx_status.data = frame;
	event.tx_status.data_len = len;
	event.tx_status.ack = ack != NULL;
	wpa_supplicant_event(drv->ctx, EVENT_TX_STATUS, &event);
}


static void mlme_event_deauth_disassoc(struct wpa_driver_nl80211_data *drv,
				       enum wpa_event_type type,
				       const u8 *frame, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;
	const u8 *bssid = NULL;
	u16 reason_code = 0;

	if (type == EVENT_DEAUTH)
		wpa_printf(MSG_DEBUG, "nl80211: Deauthenticate event");
	else
		wpa_printf(MSG_DEBUG, "nl80211: Disassociate event");

	mgmt = (const struct ieee80211_mgmt *) frame;
	if (len >= 24) {
		bssid = mgmt->bssid;

		if (drv->associated != 0 &&
		    os_memcmp(bssid, drv->bssid, ETH_ALEN) != 0 &&
		    os_memcmp(bssid, drv->auth_bssid, ETH_ALEN) != 0) {
			/*
			 * We have presumably received this deauth as a
			 * response to a clear_state_mismatch() outgoing
			 * deauth.  Don't let it take us offline!
			 */
			wpa_printf(MSG_DEBUG, "nl80211: Deauth received "
				   "from Unknown BSSID " MACSTR " -- ignoring",
				   MAC2STR(bssid));
			return;
		}
	}

	drv->associated = 0;
	os_memset(&event, 0, sizeof(event));

	/* Note: Same offset for Reason Code in both frame subtypes */
	if (len >= 24 + sizeof(mgmt->u.deauth))
		reason_code = le_to_host16(mgmt->u.deauth.reason_code);

	if (type == EVENT_DISASSOC) {
		event.disassoc_info.locally_generated =
			!os_memcmp(mgmt->sa, drv->first_bss.addr, ETH_ALEN);
		event.disassoc_info.addr = bssid;
		event.disassoc_info.reason_code = reason_code;
		if (frame + len > mgmt->u.disassoc.variable) {
			event.disassoc_info.ie = mgmt->u.disassoc.variable;
			event.disassoc_info.ie_len = frame + len -
				mgmt->u.disassoc.variable;
		}
	} else {
		event.deauth_info.locally_generated =
			!os_memcmp(mgmt->sa, drv->first_bss.addr, ETH_ALEN);
		event.deauth_info.addr = bssid;
		event.deauth_info.reason_code = reason_code;
		if (frame + len > mgmt->u.deauth.variable) {
			event.deauth_info.ie = mgmt->u.deauth.variable;
			event.deauth_info.ie_len = frame + len -
				mgmt->u.deauth.variable;
		}
	}

	wpa_supplicant_event(drv->ctx, type, &event);
}


static void mlme_event_unprot_disconnect(struct wpa_driver_nl80211_data *drv,
					 enum wpa_event_type type,
					 const u8 *frame, size_t len)
{
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;
	u16 reason_code = 0;

	if (type == EVENT_UNPROT_DEAUTH)
		wpa_printf(MSG_DEBUG, "nl80211: Unprot Deauthenticate event");
	else
		wpa_printf(MSG_DEBUG, "nl80211: Unprot Disassociate event");

	if (len < 24)
		return;

	mgmt = (const struct ieee80211_mgmt *) frame;

	os_memset(&event, 0, sizeof(event));
	/* Note: Same offset for Reason Code in both frame subtypes */
	if (len >= 24 + sizeof(mgmt->u.deauth))
		reason_code = le_to_host16(mgmt->u.deauth.reason_code);

	if (type == EVENT_UNPROT_DISASSOC) {
		event.unprot_disassoc.sa = mgmt->sa;
		event.unprot_disassoc.da = mgmt->da;
		event.unprot_disassoc.reason_code = reason_code;
	} else {
		event.unprot_deauth.sa = mgmt->sa;
		event.unprot_deauth.da = mgmt->da;
		event.unprot_deauth.reason_code = reason_code;
	}

	wpa_supplicant_event(drv->ctx, type, &event);
}


static void mlme_event(struct wpa_driver_nl80211_data *drv,
		       enum nl80211_commands cmd, struct nlattr *frame,
		       struct nlattr *addr, struct nlattr *timed_out,
		       struct nlattr *freq, struct nlattr *ack,
		       struct nlattr *cookie, struct nlattr *sig)
{
	if (timed_out && addr) {
		mlme_timeout_event(drv, cmd, addr);
		return;
	}

	if (frame == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: MLME event %d without frame "
			   "data", cmd);
		return;
	}

	wpa_printf(MSG_DEBUG, "nl80211: MLME event %d", cmd);
	wpa_hexdump(MSG_MSGDUMP, "nl80211: MLME event frame",
		    nla_data(frame), nla_len(frame));

	switch (cmd) {
	case NL80211_CMD_AUTHENTICATE:
		mlme_event_auth(drv, nla_data(frame), nla_len(frame));
		break;
	case NL80211_CMD_ASSOCIATE:
		mlme_event_assoc(drv, nla_data(frame), nla_len(frame));
		break;
	case NL80211_CMD_DEAUTHENTICATE:
		mlme_event_deauth_disassoc(drv, EVENT_DEAUTH,
					   nla_data(frame), nla_len(frame));
		break;
	case NL80211_CMD_DISASSOCIATE:
		mlme_event_deauth_disassoc(drv, EVENT_DISASSOC,
					   nla_data(frame), nla_len(frame));
		break;
	case NL80211_CMD_FRAME:
		mlme_event_mgmt(drv, freq, sig, nla_data(frame),
				nla_len(frame));
		break;
	case NL80211_CMD_FRAME_TX_STATUS:
		mlme_event_mgmt_tx_status(drv, cookie, nla_data(frame),
					  nla_len(frame), ack);
		break;
	case NL80211_CMD_UNPROT_DEAUTHENTICATE:
		mlme_event_unprot_disconnect(drv, EVENT_UNPROT_DEAUTH,
					     nla_data(frame), nla_len(frame));
		break;
	case NL80211_CMD_UNPROT_DISASSOCIATE:
		mlme_event_unprot_disconnect(drv, EVENT_UNPROT_DISASSOC,
					     nla_data(frame), nla_len(frame));
		break;
	default:
		break;
	}
}


static void mlme_event_michael_mic_failure(struct i802_bss *bss,
					   struct nlattr *tb[])
{
	union wpa_event_data data;

	wpa_printf(MSG_DEBUG, "nl80211: MLME event Michael MIC failure");
	os_memset(&data, 0, sizeof(data));
	if (tb[NL80211_ATTR_MAC]) {
		wpa_hexdump(MSG_DEBUG, "nl80211: Source MAC address",
			    nla_data(tb[NL80211_ATTR_MAC]),
			    nla_len(tb[NL80211_ATTR_MAC]));
		data.michael_mic_failure.src = nla_data(tb[NL80211_ATTR_MAC]);
	}
	if (tb[NL80211_ATTR_KEY_SEQ]) {
		wpa_hexdump(MSG_DEBUG, "nl80211: TSC",
			    nla_data(tb[NL80211_ATTR_KEY_SEQ]),
			    nla_len(tb[NL80211_ATTR_KEY_SEQ]));
	}
	if (tb[NL80211_ATTR_KEY_TYPE]) {
		enum nl80211_key_type key_type =
			nla_get_u32(tb[NL80211_ATTR_KEY_TYPE]);
		wpa_printf(MSG_DEBUG, "nl80211: Key Type %d", key_type);
		if (key_type == NL80211_KEYTYPE_PAIRWISE)
			data.michael_mic_failure.unicast = 1;
	} else
		data.michael_mic_failure.unicast = 1;

	if (tb[NL80211_ATTR_KEY_IDX]) {
		u8 key_id = nla_get_u8(tb[NL80211_ATTR_KEY_IDX]);
		wpa_printf(MSG_DEBUG, "nl80211: Key Id %d", key_id);
	}

	wpa_supplicant_event(bss->ctx, EVENT_MICHAEL_MIC_FAILURE, &data);
}


static void mlme_event_join_ibss(struct wpa_driver_nl80211_data *drv,
				 struct nlattr *tb[])
{
	if (tb[NL80211_ATTR_MAC] == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: No address in IBSS joined "
			   "event");
		return;
	}
	os_memcpy(drv->bssid, nla_data(tb[NL80211_ATTR_MAC]), ETH_ALEN);
	drv->associated = 1;
	wpa_printf(MSG_DEBUG, "nl80211: IBSS " MACSTR " joined",
		   MAC2STR(drv->bssid));

	wpa_supplicant_event(drv->ctx, EVENT_ASSOC, NULL);
}


static void mlme_event_remain_on_channel(struct wpa_driver_nl80211_data *drv,
					 int cancel_event, struct nlattr *tb[])
{
	unsigned int freq, chan_type, duration;
	union wpa_event_data data;
	u64 cookie;

	if (tb[NL80211_ATTR_WIPHY_FREQ])
		freq = nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);
	else
		freq = 0;

	if (tb[NL80211_ATTR_WIPHY_CHANNEL_TYPE])
		chan_type = nla_get_u32(tb[NL80211_ATTR_WIPHY_CHANNEL_TYPE]);
	else
		chan_type = 0;

	if (tb[NL80211_ATTR_DURATION])
		duration = nla_get_u32(tb[NL80211_ATTR_DURATION]);
	else
		duration = 0;

	if (tb[NL80211_ATTR_COOKIE])
		cookie = nla_get_u64(tb[NL80211_ATTR_COOKIE]);
	else
		cookie = 0;

	wpa_printf(MSG_DEBUG, "nl80211: Remain-on-channel event (cancel=%d "
		   "freq=%u channel_type=%u duration=%u cookie=0x%llx (%s))",
		   cancel_event, freq, chan_type, duration,
		   (long long unsigned int) cookie,
		   cookie == drv->remain_on_chan_cookie ? "match" : "unknown");

	if (cookie != drv->remain_on_chan_cookie)
		return; /* not for us */

	if (cancel_event)
		drv->pending_remain_on_chan = 0;

	os_memset(&data, 0, sizeof(data));
	data.remain_on_channel.freq = freq;
	data.remain_on_channel.duration = duration;
	wpa_supplicant_event(drv->ctx, cancel_event ?
			     EVENT_CANCEL_REMAIN_ON_CHANNEL :
			     EVENT_REMAIN_ON_CHANNEL, &data);
}


static void send_scan_event(struct wpa_driver_nl80211_data *drv, int aborted,
			    struct nlattr *tb[])
{
	union wpa_event_data event;
	struct nlattr *nl;
	int rem;
	struct scan_info *info;
#define MAX_REPORT_FREQS 50
	int freqs[MAX_REPORT_FREQS];
	int num_freqs = 0;

	if (drv->scan_for_auth) {
		drv->scan_for_auth = 0;
		wpa_printf(MSG_DEBUG, "nl80211: Scan results for missing "
			   "cfg80211 BSS entry");
		wpa_driver_nl80211_authenticate_retry(drv);
		return;
	}

	os_memset(&event, 0, sizeof(event));
	info = &event.scan_info;
	info->aborted = aborted;

	if (tb[NL80211_ATTR_SCAN_SSIDS]) {
		nla_for_each_nested(nl, tb[NL80211_ATTR_SCAN_SSIDS], rem) {
			struct wpa_driver_scan_ssid *s =
				&info->ssids[info->num_ssids];
			s->ssid = nla_data(nl);
			s->ssid_len = nla_len(nl);
			info->num_ssids++;
			if (info->num_ssids == WPAS_MAX_SCAN_SSIDS)
				break;
		}
	}
	if (tb[NL80211_ATTR_SCAN_FREQUENCIES]) {
		nla_for_each_nested(nl, tb[NL80211_ATTR_SCAN_FREQUENCIES], rem)
		{
			freqs[num_freqs] = nla_get_u32(nl);
			num_freqs++;
			if (num_freqs == MAX_REPORT_FREQS - 1)
				break;
		}
		info->freqs = freqs;
		info->num_freqs = num_freqs;
	}
	wpa_supplicant_event(drv->ctx, EVENT_SCAN_RESULTS, &event);
}


static int get_link_signal(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
	static struct nla_policy policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
	};
	struct nlattr *rinfo[NL80211_RATE_INFO_MAX + 1];
	static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
		[NL80211_RATE_INFO_BITRATE] = { .type = NLA_U16 },
		[NL80211_RATE_INFO_MCS] = { .type = NLA_U8 },
		[NL80211_RATE_INFO_40_MHZ_WIDTH] = { .type = NLA_FLAG },
		[NL80211_RATE_INFO_SHORT_GI] = { .type = NLA_FLAG },
	};
	struct wpa_signal_info *sig_change = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_STA_INFO] ||
	    nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
			     tb[NL80211_ATTR_STA_INFO], policy))
		return NL_SKIP;
	if (!sinfo[NL80211_STA_INFO_SIGNAL])
		return NL_SKIP;

	sig_change->current_signal =
		(s8) nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]);

	if (sinfo[NL80211_STA_INFO_TX_BITRATE]) {
		if (nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX,
				     sinfo[NL80211_STA_INFO_TX_BITRATE],
				     rate_policy)) {
			sig_change->current_txrate = 0;
		} else {
			if (rinfo[NL80211_RATE_INFO_BITRATE]) {
				sig_change->current_txrate =
					nla_get_u16(rinfo[
					     NL80211_RATE_INFO_BITRATE]) * 100;
			}
		}
	}

	return NL_SKIP;
}


static int nl80211_get_link_signal(struct wpa_driver_nl80211_data *drv,
				   struct wpa_signal_info *sig)
{
	struct nl_msg *msg;

	sig->current_signal = -9999;
	sig->current_txrate = 0;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_STATION);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, drv->bssid);

	return send_and_recv_msgs(drv, msg, get_link_signal, sig);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int get_link_noise(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_SURVEY_INFO_MAX + 1];
	static struct nla_policy survey_policy[NL80211_SURVEY_INFO_MAX + 1] = {
		[NL80211_SURVEY_INFO_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_SURVEY_INFO_NOISE] = { .type = NLA_U8 },
	};
	struct wpa_signal_info *sig_change = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_SURVEY_INFO]) {
		wpa_printf(MSG_DEBUG, "nl80211: survey data missing!");
		return NL_SKIP;
	}

	if (nla_parse_nested(sinfo, NL80211_SURVEY_INFO_MAX,
			     tb[NL80211_ATTR_SURVEY_INFO],
			     survey_policy)) {
		wpa_printf(MSG_DEBUG, "nl80211: failed to parse nested "
			   "attributes!");
		return NL_SKIP;
	}

	if (!sinfo[NL80211_SURVEY_INFO_FREQUENCY])
		return NL_SKIP;

	if (nla_get_u32(sinfo[NL80211_SURVEY_INFO_FREQUENCY]) !=
	    sig_change->frequency)
		return NL_SKIP;

	if (!sinfo[NL80211_SURVEY_INFO_NOISE])
		return NL_SKIP;

	sig_change->current_noise =
		(s8) nla_get_u8(sinfo[NL80211_SURVEY_INFO_NOISE]);

	return NL_SKIP;
}


static int nl80211_get_link_noise(struct wpa_driver_nl80211_data *drv,
				  struct wpa_signal_info *sig_change)
{
	struct nl_msg *msg;

	sig_change->current_noise = 9999;
	sig_change->frequency = drv->assoc_freq;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	return send_and_recv_msgs(drv, msg, get_link_noise, sig_change);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int get_noise_for_scan_results(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_SURVEY_INFO_MAX + 1];
	static struct nla_policy survey_policy[NL80211_SURVEY_INFO_MAX + 1] = {
		[NL80211_SURVEY_INFO_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_SURVEY_INFO_NOISE] = { .type = NLA_U8 },
	};
	struct wpa_scan_results *scan_results = arg;
	struct wpa_scan_res *scan_res;
	size_t i;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_SURVEY_INFO]) {
		wpa_printf(MSG_DEBUG, "nl80211: Survey data missing");
		return NL_SKIP;
	}

	if (nla_parse_nested(sinfo, NL80211_SURVEY_INFO_MAX,
			     tb[NL80211_ATTR_SURVEY_INFO],
			     survey_policy)) {
		wpa_printf(MSG_DEBUG, "nl80211: Failed to parse nested "
			   "attributes");
		return NL_SKIP;
	}

	if (!sinfo[NL80211_SURVEY_INFO_NOISE])
		return NL_SKIP;

	if (!sinfo[NL80211_SURVEY_INFO_FREQUENCY])
		return NL_SKIP;

	for (i = 0; i < scan_results->num; ++i) {
		scan_res = scan_results->res[i];
		if (!scan_res)
			continue;
		if ((int) nla_get_u32(sinfo[NL80211_SURVEY_INFO_FREQUENCY]) !=
		    scan_res->freq)
			continue;
		if (!(scan_res->flags & WPA_SCAN_NOISE_INVALID))
			continue;
		scan_res->noise = (s8)
			nla_get_u8(sinfo[NL80211_SURVEY_INFO_NOISE]);
		scan_res->flags &= ~WPA_SCAN_NOISE_INVALID;
	}

	return NL_SKIP;
}


static int nl80211_get_noise_for_scan_results(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_scan_results *scan_res)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	return send_and_recv_msgs(drv, msg, get_noise_for_scan_results,
				  scan_res);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static void nl80211_cqm_event(struct wpa_driver_nl80211_data *drv,
			      struct nlattr *tb[])
{
	static struct nla_policy cqm_policy[NL80211_ATTR_CQM_MAX + 1] = {
		[NL80211_ATTR_CQM_RSSI_THOLD] = { .type = NLA_U32 },
		[NL80211_ATTR_CQM_RSSI_HYST] = { .type = NLA_U8 },
		[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT] = { .type = NLA_U32 },
		[NL80211_ATTR_CQM_PKT_LOSS_EVENT] = { .type = NLA_U32 },
	};
	struct nlattr *cqm[NL80211_ATTR_CQM_MAX + 1];
	enum nl80211_cqm_rssi_threshold_event event;
	union wpa_event_data ed;
	struct wpa_signal_info sig;
	int res;

	if (tb[NL80211_ATTR_CQM] == NULL ||
	    nla_parse_nested(cqm, NL80211_ATTR_CQM_MAX, tb[NL80211_ATTR_CQM],
			     cqm_policy)) {
		wpa_printf(MSG_DEBUG, "nl80211: Ignore invalid CQM event");
		return;
	}

	os_memset(&ed, 0, sizeof(ed));

	if (cqm[NL80211_ATTR_CQM_PKT_LOSS_EVENT]) {
		if (!tb[NL80211_ATTR_MAC])
			return;
		os_memcpy(ed.low_ack.addr, nla_data(tb[NL80211_ATTR_MAC]),
			  ETH_ALEN);
		wpa_supplicant_event(drv->ctx, EVENT_STATION_LOW_ACK, &ed);
		return;
	}

	if (cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT] == NULL)
		return;
	event = nla_get_u32(cqm[NL80211_ATTR_CQM_RSSI_THRESHOLD_EVENT]);

	if (event == NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH) {
		wpa_printf(MSG_DEBUG, "nl80211: Connection quality monitor "
			   "event: RSSI high");
		ed.signal_change.above_threshold = 1;
	} else if (event == NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW) {
		wpa_printf(MSG_DEBUG, "nl80211: Connection quality monitor "
			   "event: RSSI low");
		ed.signal_change.above_threshold = 0;
	} else
		return;

	res = nl80211_get_link_signal(drv, &sig);
	if (res == 0) {
		ed.signal_change.current_signal = sig.current_signal;
		ed.signal_change.current_txrate = sig.current_txrate;
		wpa_printf(MSG_DEBUG, "nl80211: Signal: %d dBm  txrate: %d",
			   sig.current_signal, sig.current_txrate);
	}

	res = nl80211_get_link_noise(drv, &sig);
	if (res == 0) {
		ed.signal_change.current_noise = sig.current_noise;
		wpa_printf(MSG_DEBUG, "nl80211: Noise: %d dBm",
			   sig.current_noise);
	}

	wpa_supplicant_event(drv->ctx, EVENT_SIGNAL_CHANGE, &ed);
}


static void nl80211_new_station_event(struct wpa_driver_nl80211_data *drv,
				      struct nlattr **tb)
{
	u8 *addr;
	union wpa_event_data data;

	if (tb[NL80211_ATTR_MAC] == NULL)
		return;
	addr = nla_data(tb[NL80211_ATTR_MAC]);
	wpa_printf(MSG_DEBUG, "nl80211: New station " MACSTR, MAC2STR(addr));

	if (is_ap_interface(drv->nlmode) && drv->device_ap_sme) {
		u8 *ies = NULL;
		size_t ies_len = 0;
		if (tb[NL80211_ATTR_IE]) {
			ies = nla_data(tb[NL80211_ATTR_IE]);
			ies_len = nla_len(tb[NL80211_ATTR_IE]);
		}
		wpa_hexdump(MSG_DEBUG, "nl80211: Assoc Req IEs", ies, ies_len);
		drv_event_assoc(drv->ctx, addr, ies, ies_len, 0);
		return;
	}

	if (drv->nlmode != NL80211_IFTYPE_ADHOC)
		return;

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.ibss_rsn_start.peer, addr, ETH_ALEN);
	wpa_supplicant_event(drv->ctx, EVENT_IBSS_RSN_START, &data);
}


static void nl80211_del_station_event(struct wpa_driver_nl80211_data *drv,
				      struct nlattr **tb)
{
	u8 *addr;
	union wpa_event_data data;

	if (tb[NL80211_ATTR_MAC] == NULL)
		return;
	addr = nla_data(tb[NL80211_ATTR_MAC]);
	wpa_printf(MSG_DEBUG, "nl80211: Delete station " MACSTR,
		   MAC2STR(addr));

	if (is_ap_interface(drv->nlmode) && drv->device_ap_sme) {
		drv_event_disassoc(drv->ctx, addr);
		return;
	}

	if (drv->nlmode != NL80211_IFTYPE_ADHOC)
		return;

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.ibss_peer_lost.peer, addr, ETH_ALEN);
	wpa_supplicant_event(drv->ctx, EVENT_IBSS_PEER_LOST, &data);
}


static void nl80211_rekey_offload_event(struct wpa_driver_nl80211_data *drv,
					struct nlattr **tb)
{
	struct nlattr *rekey_info[NUM_NL80211_REKEY_DATA];
	static struct nla_policy rekey_policy[NUM_NL80211_REKEY_DATA] = {
		[NL80211_REKEY_DATA_KEK] = {
			.minlen = NL80211_KEK_LEN,
			.maxlen = NL80211_KEK_LEN,
		},
		[NL80211_REKEY_DATA_KCK] = {
			.minlen = NL80211_KCK_LEN,
			.maxlen = NL80211_KCK_LEN,
		},
		[NL80211_REKEY_DATA_REPLAY_CTR] = {
			.minlen = NL80211_REPLAY_CTR_LEN,
			.maxlen = NL80211_REPLAY_CTR_LEN,
		},
	};
	union wpa_event_data data;

	if (!tb[NL80211_ATTR_MAC])
		return;
	if (!tb[NL80211_ATTR_REKEY_DATA])
		return;
	if (nla_parse_nested(rekey_info, MAX_NL80211_REKEY_DATA,
			     tb[NL80211_ATTR_REKEY_DATA], rekey_policy))
		return;
	if (!rekey_info[NL80211_REKEY_DATA_REPLAY_CTR])
		return;

	os_memset(&data, 0, sizeof(data));
	data.driver_gtk_rekey.bssid = nla_data(tb[NL80211_ATTR_MAC]);
	wpa_printf(MSG_DEBUG, "nl80211: Rekey offload event for BSSID " MACSTR,
		   MAC2STR(data.driver_gtk_rekey.bssid));
	data.driver_gtk_rekey.replay_ctr =
		nla_data(rekey_info[NL80211_REKEY_DATA_REPLAY_CTR]);
	wpa_hexdump(MSG_DEBUG, "nl80211: Rekey offload - Replay Counter",
		    data.driver_gtk_rekey.replay_ctr, NL80211_REPLAY_CTR_LEN);
	wpa_supplicant_event(drv->ctx, EVENT_DRIVER_GTK_REKEY, &data);
}


static void nl80211_pmksa_candidate_event(struct wpa_driver_nl80211_data *drv,
					  struct nlattr **tb)
{
	struct nlattr *cand[NUM_NL80211_PMKSA_CANDIDATE];
	static struct nla_policy cand_policy[NUM_NL80211_PMKSA_CANDIDATE] = {
		[NL80211_PMKSA_CANDIDATE_INDEX] = { .type = NLA_U32 },
		[NL80211_PMKSA_CANDIDATE_BSSID] = {
			.minlen = ETH_ALEN,
			.maxlen = ETH_ALEN,
		},
		[NL80211_PMKSA_CANDIDATE_PREAUTH] = { .type = NLA_FLAG },
	};
	union wpa_event_data data;

	wpa_printf(MSG_DEBUG, "nl80211: PMKSA candidate event");

	if (!tb[NL80211_ATTR_PMKSA_CANDIDATE])
		return;
	if (nla_parse_nested(cand, MAX_NL80211_PMKSA_CANDIDATE,
			     tb[NL80211_ATTR_PMKSA_CANDIDATE], cand_policy))
		return;
	if (!cand[NL80211_PMKSA_CANDIDATE_INDEX] ||
	    !cand[NL80211_PMKSA_CANDIDATE_BSSID])
		return;

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.pmkid_candidate.bssid,
		  nla_data(cand[NL80211_PMKSA_CANDIDATE_BSSID]), ETH_ALEN);
	data.pmkid_candidate.index =
		nla_get_u32(cand[NL80211_PMKSA_CANDIDATE_INDEX]);
	data.pmkid_candidate.preauth =
		cand[NL80211_PMKSA_CANDIDATE_PREAUTH] != NULL;
	wpa_supplicant_event(drv->ctx, EVENT_PMKID_CANDIDATE, &data);
}


static void nl80211_client_probe_event(struct wpa_driver_nl80211_data *drv,
				       struct nlattr **tb)
{
	union wpa_event_data data;

	wpa_printf(MSG_DEBUG, "nl80211: Probe client event");

	if (!tb[NL80211_ATTR_MAC] || !tb[NL80211_ATTR_ACK])
		return;

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.client_poll.addr,
		  nla_data(tb[NL80211_ATTR_MAC]), ETH_ALEN);

	wpa_supplicant_event(drv->ctx, EVENT_DRIVER_CLIENT_POLL_OK, &data);
}


static void nl80211_tdls_oper_event(struct wpa_driver_nl80211_data *drv,
				    struct nlattr **tb)
{
	union wpa_event_data data;

	wpa_printf(MSG_DEBUG, "nl80211: TDLS operation event");

	if (!tb[NL80211_ATTR_MAC] || !tb[NL80211_ATTR_TDLS_OPERATION])
		return;

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.tdls.peer, nla_data(tb[NL80211_ATTR_MAC]), ETH_ALEN);
	switch (nla_get_u8(tb[NL80211_ATTR_TDLS_OPERATION])) {
	case NL80211_TDLS_SETUP:
		wpa_printf(MSG_DEBUG, "nl80211: TDLS setup request for peer "
			   MACSTR, MAC2STR(data.tdls.peer));
		data.tdls.oper = TDLS_REQUEST_SETUP;
		break;
	case NL80211_TDLS_TEARDOWN:
		wpa_printf(MSG_DEBUG, "nl80211: TDLS teardown request for peer "
			   MACSTR, MAC2STR(data.tdls.peer));
		data.tdls.oper = TDLS_REQUEST_TEARDOWN;
		break;
	default:
		wpa_printf(MSG_DEBUG, "nl80211: Unsupported TDLS operatione "
			   "event");
		return;
	}
	if (tb[NL80211_ATTR_REASON_CODE]) {
		data.tdls.reason_code =
			nla_get_u16(tb[NL80211_ATTR_REASON_CODE]);
	}

	wpa_supplicant_event(drv->ctx, EVENT_TDLS, &data);
}


static void nl80211_spurious_frame(struct i802_bss *bss, struct nlattr **tb,
				   int wds)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	union wpa_event_data event;

	if (!tb[NL80211_ATTR_MAC])
		return;

	os_memset(&event, 0, sizeof(event));
	event.rx_from_unknown.bssid = bss->addr;
	event.rx_from_unknown.addr = nla_data(tb[NL80211_ATTR_MAC]);
	event.rx_from_unknown.wds = wds;

	wpa_supplicant_event(drv->ctx, EVENT_RX_FROM_UNKNOWN, &event);
}


static void do_process_drv_event(struct i802_bss *bss, int cmd,
				 struct nlattr **tb)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (drv->ap_scan_as_station != NL80211_IFTYPE_UNSPECIFIED &&
	    (cmd == NL80211_CMD_NEW_SCAN_RESULTS ||
	     cmd == NL80211_CMD_SCAN_ABORTED)) {
		wpa_driver_nl80211_set_mode(&drv->first_bss,
					    drv->ap_scan_as_station);
		drv->ap_scan_as_station = NL80211_IFTYPE_UNSPECIFIED;
	}

	switch (cmd) {
	case NL80211_CMD_TRIGGER_SCAN:
		wpa_printf(MSG_DEBUG, "nl80211: Scan trigger");
		break;
	case NL80211_CMD_START_SCHED_SCAN:
		wpa_printf(MSG_DEBUG, "nl80211: Sched scan started");
		break;
	case NL80211_CMD_SCHED_SCAN_STOPPED:
		wpa_printf(MSG_DEBUG, "nl80211: Sched scan stopped");
		wpa_supplicant_event(drv->ctx, EVENT_SCHED_SCAN_STOPPED, NULL);
		break;
	case NL80211_CMD_NEW_SCAN_RESULTS:
		wpa_printf(MSG_DEBUG, "nl80211: New scan results available");
		drv->scan_complete_events = 1;
		eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv,
				     drv->ctx);
		send_scan_event(drv, 0, tb);
		break;
	case NL80211_CMD_SCHED_SCAN_RESULTS:
		wpa_printf(MSG_DEBUG,
			   "nl80211: New sched scan results available");
		send_scan_event(drv, 0, tb);
		break;
	case NL80211_CMD_SCAN_ABORTED:
		wpa_printf(MSG_DEBUG, "nl80211: Scan aborted");
		/*
		 * Need to indicate that scan results are available in order
		 * not to make wpa_supplicant stop its scanning.
		 */
		eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv,
				     drv->ctx);
		send_scan_event(drv, 1, tb);
		break;
	case NL80211_CMD_AUTHENTICATE:
	case NL80211_CMD_ASSOCIATE:
	case NL80211_CMD_DEAUTHENTICATE:
	case NL80211_CMD_DISASSOCIATE:
	case NL80211_CMD_FRAME_TX_STATUS:
	case NL80211_CMD_UNPROT_DEAUTHENTICATE:
	case NL80211_CMD_UNPROT_DISASSOCIATE:
		mlme_event(drv, cmd, tb[NL80211_ATTR_FRAME],
			   tb[NL80211_ATTR_MAC], tb[NL80211_ATTR_TIMED_OUT],
			   tb[NL80211_ATTR_WIPHY_FREQ], tb[NL80211_ATTR_ACK],
			   tb[NL80211_ATTR_COOKIE],
			   tb[NL80211_ATTR_RX_SIGNAL_DBM]);
		break;
	case NL80211_CMD_CONNECT:
	case NL80211_CMD_ROAM:
		mlme_event_connect(drv, cmd,
				   tb[NL80211_ATTR_STATUS_CODE],
				   tb[NL80211_ATTR_MAC],
				   tb[NL80211_ATTR_REQ_IE],
				   tb[NL80211_ATTR_RESP_IE]);
		break;
	case NL80211_CMD_CH_SWITCH_NOTIFY:
		mlme_event_ch_switch(drv, tb[NL80211_ATTR_WIPHY_FREQ],
				     tb[NL80211_ATTR_WIPHY_CHANNEL_TYPE]);
		break;
	case NL80211_CMD_DISCONNECT:
		mlme_event_disconnect(drv, tb[NL80211_ATTR_REASON_CODE],
				      tb[NL80211_ATTR_MAC],
				      tb[NL80211_ATTR_DISCONNECTED_BY_AP]);
		break;
	case NL80211_CMD_MICHAEL_MIC_FAILURE:
		mlme_event_michael_mic_failure(bss, tb);
		break;
	case NL80211_CMD_JOIN_IBSS:
		mlme_event_join_ibss(drv, tb);
		break;
	case NL80211_CMD_REMAIN_ON_CHANNEL:
		mlme_event_remain_on_channel(drv, 0, tb);
		break;
	case NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL:
		mlme_event_remain_on_channel(drv, 1, tb);
		break;
	case NL80211_CMD_NOTIFY_CQM:
		nl80211_cqm_event(drv, tb);
		break;
	case NL80211_CMD_REG_CHANGE:
		wpa_printf(MSG_DEBUG, "nl80211: Regulatory domain change");
		wpa_supplicant_event(drv->ctx, EVENT_CHANNEL_LIST_CHANGED,
				     NULL);
		break;
	case NL80211_CMD_REG_BEACON_HINT:
		wpa_printf(MSG_DEBUG, "nl80211: Regulatory beacon hint");
		wpa_supplicant_event(drv->ctx, EVENT_CHANNEL_LIST_CHANGED,
				     NULL);
		break;
	case NL80211_CMD_NEW_STATION:
		nl80211_new_station_event(drv, tb);
		break;
	case NL80211_CMD_DEL_STATION:
		nl80211_del_station_event(drv, tb);
		break;
	case NL80211_CMD_SET_REKEY_OFFLOAD:
		nl80211_rekey_offload_event(drv, tb);
		break;
	case NL80211_CMD_PMKSA_CANDIDATE:
		nl80211_pmksa_candidate_event(drv, tb);
		break;
	case NL80211_CMD_PROBE_CLIENT:
		nl80211_client_probe_event(drv, tb);
		break;
	case NL80211_CMD_TDLS_OPER:
		nl80211_tdls_oper_event(drv, tb);
		break;
	default:
		wpa_printf(MSG_DEBUG, "nl80211: Ignored unknown event "
			   "(cmd=%d)", cmd);
		break;
	}
}


static int process_drv_event(struct nl_msg *msg, void *arg)
{
	struct wpa_driver_nl80211_data *drv = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct i802_bss *bss;
	int ifidx = -1;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX])
		ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

	for (bss = &drv->first_bss; bss; bss = bss->next) {
		if (ifidx == -1 || ifidx == bss->ifindex) {
			do_process_drv_event(bss, gnlh->cmd, tb);
			return NL_SKIP;
		}
	}

	wpa_printf(MSG_DEBUG, "nl80211: Ignored event (cmd=%d) for foreign "
		   "interface (ifindex %d)", gnlh->cmd, ifidx);

	return NL_SKIP;
}


static int process_global_event(struct nl_msg *msg, void *arg)
{
	struct nl80211_global *global = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct wpa_driver_nl80211_data *drv, *tmp;
	int ifidx = -1;
	struct i802_bss *bss;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX])
		ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

	dl_list_for_each_safe(drv, tmp, &global->interfaces,
			      struct wpa_driver_nl80211_data, list) {
		for (bss = &drv->first_bss; bss; bss = bss->next) {
			if (ifidx == -1 || ifidx == bss->ifindex) {
				do_process_drv_event(bss, gnlh->cmd, tb);
				return NL_SKIP;
			}
		}
	}

	return NL_SKIP;
}


static int process_bss_event(struct nl_msg *msg, void *arg)
{
	struct i802_bss *bss = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	switch (gnlh->cmd) {
	case NL80211_CMD_FRAME:
	case NL80211_CMD_FRAME_TX_STATUS:
		mlme_event(bss->drv, gnlh->cmd, tb[NL80211_ATTR_FRAME],
			   tb[NL80211_ATTR_MAC], tb[NL80211_ATTR_TIMED_OUT],
			   tb[NL80211_ATTR_WIPHY_FREQ], tb[NL80211_ATTR_ACK],
			   tb[NL80211_ATTR_COOKIE],
			   tb[NL80211_ATTR_RX_SIGNAL_DBM]);
		break;
	case NL80211_CMD_UNEXPECTED_FRAME:
		nl80211_spurious_frame(bss, tb, 0);
		break;
	case NL80211_CMD_UNEXPECTED_4ADDR_FRAME:
		nl80211_spurious_frame(bss, tb, 1);
		break;
	default:
		wpa_printf(MSG_DEBUG, "nl80211: Ignored unknown event "
			   "(cmd=%d)", gnlh->cmd);
		break;
	}

	return NL_SKIP;
}


static void wpa_driver_nl80211_event_receive(int sock, void *eloop_ctx,
					     void *handle)
{
	struct nl_cb *cb = eloop_ctx;

	wpa_printf(MSG_DEBUG, "nl80211: Event message available");

	nl_recvmsgs(handle, cb);
}


/**
 * wpa_driver_nl80211_set_country - ask nl80211 to set the regulatory domain
 * @priv: driver_nl80211 private data
 * @alpha2_arg: country to which to switch to
 * Returns: 0 on success, -1 on failure
 *
 * This asks nl80211 to set the regulatory domain for given
 * country ISO / IEC alpha2.
 */
static int wpa_driver_nl80211_set_country(void *priv, const char *alpha2_arg)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	char alpha2[3];
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	alpha2[0] = alpha2_arg[0];
	alpha2[1] = alpha2_arg[1];
	alpha2[2] = '\0';

	nl80211_cmd(drv, msg, 0, NL80211_CMD_REQ_SET_REG);

	NLA_PUT_STRING(msg, NL80211_ATTR_REG_ALPHA2, alpha2);
	if (send_and_recv_msgs(drv, msg, NULL, NULL))
		return -EINVAL;
	return 0;
nla_put_failure:
	nlmsg_free(msg);
	return -EINVAL;
}


struct wiphy_info_data {
	struct wpa_driver_capa *capa;

	unsigned int error:1;
	unsigned int device_ap_sme:1;
	unsigned int poll_command_supported:1;
	unsigned int data_tx_status:1;
	unsigned int monitor_supported:1;
};


static unsigned int probe_resp_offload_support(int supp_protocols)
{
	unsigned int prot = 0;

	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS;
	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS2;
	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_P2P;
	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_80211U)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_INTERWORKING;

	return prot;
}


static int wiphy_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wiphy_info_data *info = arg;
	int p2p_go_supported = 0, p2p_client_supported = 0;
	int p2p_concurrent = 0, p2p_multichan_concurrent = 0;
	int auth_supported = 0, connect_supported = 0;
	struct wpa_driver_capa *capa = info->capa;
	static struct nla_policy
	iface_combination_policy[NUM_NL80211_IFACE_COMB] = {
		[NL80211_IFACE_COMB_LIMITS] = { .type = NLA_NESTED },
		[NL80211_IFACE_COMB_MAXNUM] = { .type = NLA_U32 },
		[NL80211_IFACE_COMB_STA_AP_BI_MATCH] = { .type = NLA_FLAG },
		[NL80211_IFACE_COMB_NUM_CHANNELS] = { .type = NLA_U32 },
	},
	iface_limit_policy[NUM_NL80211_IFACE_LIMIT] = {
		[NL80211_IFACE_LIMIT_TYPES] = { .type = NLA_NESTED },
		[NL80211_IFACE_LIMIT_MAX] = { .type = NLA_U32 },
	};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS])
		capa->max_scan_ssids =
			nla_get_u8(tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS]);

	if (tb[NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS])
		capa->max_sched_scan_ssids =
			nla_get_u8(tb[NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS]);

	if (tb[NL80211_ATTR_MAX_MATCH_SETS])
		capa->max_match_sets =
			nla_get_u8(tb[NL80211_ATTR_MAX_MATCH_SETS]);

	if (tb[NL80211_ATTR_SUPPORTED_IFTYPES]) {
		struct nlattr *nl_mode;
		int i;
		nla_for_each_nested(nl_mode,
				    tb[NL80211_ATTR_SUPPORTED_IFTYPES], i) {
			switch (nla_type(nl_mode)) {
			case NL80211_IFTYPE_AP:
				capa->flags |= WPA_DRIVER_FLAGS_AP;
				break;
			case NL80211_IFTYPE_P2P_GO:
				p2p_go_supported = 1;
				break;
			case NL80211_IFTYPE_P2P_CLIENT:
				p2p_client_supported = 1;
				break;
			case NL80211_IFTYPE_MONITOR:
				info->monitor_supported = 1;
				break;
			}
		}
	}

	if (tb[NL80211_ATTR_INTERFACE_COMBINATIONS]) {
		struct nlattr *nl_combi;
		int rem_combi;

		nla_for_each_nested(nl_combi,
				    tb[NL80211_ATTR_INTERFACE_COMBINATIONS],
				    rem_combi) {
			struct nlattr *tb_comb[NUM_NL80211_IFACE_COMB];
			struct nlattr *tb_limit[NUM_NL80211_IFACE_LIMIT];
			struct nlattr *nl_limit, *nl_mode;
			int err, rem_limit, rem_mode;
			int combination_has_p2p = 0, combination_has_mgd = 0;

			err = nla_parse_nested(tb_comb, MAX_NL80211_IFACE_COMB,
					       nl_combi,
					       iface_combination_policy);
			if (err || !tb_comb[NL80211_IFACE_COMB_LIMITS] ||
			    !tb_comb[NL80211_IFACE_COMB_MAXNUM] ||
			    !tb_comb[NL80211_IFACE_COMB_NUM_CHANNELS])
				goto broken_combination;

			nla_for_each_nested(nl_limit,
					    tb_comb[NL80211_IFACE_COMB_LIMITS],
					    rem_limit) {
				err = nla_parse_nested(tb_limit,
						       MAX_NL80211_IFACE_LIMIT,
						       nl_limit,
						       iface_limit_policy);
				if (err ||
				    !tb_limit[NL80211_IFACE_LIMIT_TYPES])
					goto broken_combination;

				nla_for_each_nested(
					nl_mode,
					tb_limit[NL80211_IFACE_LIMIT_TYPES],
					rem_mode) {
					int ift = nla_type(nl_mode);
					if (ift == NL80211_IFTYPE_P2P_GO ||
					    ift == NL80211_IFTYPE_P2P_CLIENT)
						combination_has_p2p = 1;
					if (ift == NL80211_IFTYPE_STATION)
						combination_has_mgd = 1;
				}
				if (combination_has_p2p && combination_has_mgd)
					break;
			}

			if (combination_has_p2p && combination_has_mgd) {
				p2p_concurrent = 1;
				if (nla_get_u32(tb_comb[NL80211_IFACE_COMB_NUM_CHANNELS]) > 1)
					p2p_multichan_concurrent = 1;
				break;
			}

broken_combination:
			;
		}
	}

	if (tb[NL80211_ATTR_SUPPORTED_COMMANDS]) {
		struct nlattr *nl_cmd;
		int i;

		nla_for_each_nested(nl_cmd,
				    tb[NL80211_ATTR_SUPPORTED_COMMANDS], i) {
			switch (nla_get_u32(nl_cmd)) {
			case NL80211_CMD_AUTHENTICATE:
				auth_supported = 1;
				break;
			case NL80211_CMD_CONNECT:
				connect_supported = 1;
				break;
			case NL80211_CMD_START_SCHED_SCAN:
				capa->sched_scan_supported = 1;
				break;
			case NL80211_CMD_PROBE_CLIENT:
				info->poll_command_supported = 1;
				break;
			}
		}
	}

	if (tb[NL80211_ATTR_OFFCHANNEL_TX_OK]) {
		wpa_printf(MSG_DEBUG, "nl80211: Using driver-based "
			   "off-channel TX");
		capa->flags |= WPA_DRIVER_FLAGS_OFFCHANNEL_TX;
	}

	if (tb[NL80211_ATTR_ROAM_SUPPORT]) {
		wpa_printf(MSG_DEBUG, "nl80211: Using driver-based roaming");
		capa->flags |= WPA_DRIVER_FLAGS_BSS_SELECTION;
	}

	/* default to 5000 since early versions of mac80211 don't set it */
	capa->max_remain_on_chan = 5000;

	if (tb[NL80211_ATTR_SUPPORT_AP_UAPSD])
		capa->flags |= WPA_DRIVER_FLAGS_AP_UAPSD;

	if (tb[NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION])
		capa->max_remain_on_chan =
			nla_get_u32(tb[NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION]);

	if (auth_supported)
		capa->flags |= WPA_DRIVER_FLAGS_SME;
	else if (!connect_supported) {
		wpa_printf(MSG_INFO, "nl80211: Driver does not support "
			   "authentication/association or connect commands");
		info->error = 1;
	}

	if (p2p_go_supported && p2p_client_supported)
		capa->flags |= WPA_DRIVER_FLAGS_P2P_CAPABLE;
	if (p2p_concurrent) {
		wpa_printf(MSG_DEBUG, "nl80211: Use separate P2P group "
			   "interface (driver advertised support)");
		capa->flags |= WPA_DRIVER_FLAGS_P2P_CONCURRENT;
		capa->flags |= WPA_DRIVER_FLAGS_P2P_MGMT_AND_NON_P2P;

		if (p2p_multichan_concurrent) {
			wpa_printf(MSG_DEBUG, "nl80211: Enable multi-channel "
				   "concurrent (driver advertised support)");
			capa->flags |=
				WPA_DRIVER_FLAGS_MULTI_CHANNEL_CONCURRENT;
		}
	}

	if (tb[NL80211_ATTR_TDLS_SUPPORT]) {
		wpa_printf(MSG_DEBUG, "nl80211: TDLS supported");
		capa->flags |= WPA_DRIVER_FLAGS_TDLS_SUPPORT;

		if (tb[NL80211_ATTR_TDLS_EXTERNAL_SETUP]) {
			wpa_printf(MSG_DEBUG, "nl80211: TDLS external setup");
			capa->flags |=
				WPA_DRIVER_FLAGS_TDLS_EXTERNAL_SETUP;
		}
	}

	if (tb[NL80211_ATTR_DEVICE_AP_SME])
		info->device_ap_sme = 1;

	if (tb[NL80211_ATTR_FEATURE_FLAGS]) {
		u32 flags = nla_get_u32(tb[NL80211_ATTR_FEATURE_FLAGS]);

		if (flags & NL80211_FEATURE_SK_TX_STATUS)
			info->data_tx_status = 1;

		if (flags & NL80211_FEATURE_INACTIVITY_TIMER)
			capa->flags |= WPA_DRIVER_FLAGS_INACTIVITY_TIMER;

		if (flags & NL80211_FEATURE_SAE)
			capa->flags |= WPA_DRIVER_FLAGS_SAE;

		if (flags & NL80211_FEATURE_NEED_OBSS_SCAN)
			capa->flags |= WPA_DRIVER_FLAGS_OBSS_SCAN;
	}

	if (tb[NL80211_ATTR_PROBE_RESP_OFFLOAD]) {
		int protocols =
			nla_get_u32(tb[NL80211_ATTR_PROBE_RESP_OFFLOAD]);
		wpa_printf(MSG_DEBUG, "nl80211: Supports Probe Response "
			   "offload in AP mode");
		capa->flags |= WPA_DRIVER_FLAGS_PROBE_RESP_OFFLOAD;
		capa->probe_resp_offloads =
			probe_resp_offload_support(protocols);
	}

	return NL_SKIP;
}


static int wpa_driver_nl80211_get_info(struct wpa_driver_nl80211_data *drv,
				       struct wiphy_info_data *info)
{
	struct nl_msg *msg;

	os_memset(info, 0, sizeof(*info));
	info->capa = &drv->capa;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_WIPHY);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->first_bss.ifindex);

	if (send_and_recv_msgs(drv, msg, wiphy_info_handler, info) == 0)
		return 0;
	msg = NULL;
nla_put_failure:
	nlmsg_free(msg);
	return -1;
}


static int wpa_driver_nl80211_capa(struct wpa_driver_nl80211_data *drv)
{
	struct wiphy_info_data info;
	if (wpa_driver_nl80211_get_info(drv, &info))
		return -1;

	if (info.error)
		return -1;

	drv->has_capability = 1;
	/* For now, assume TKIP, CCMP, WPA, WPA2 are supported */
	drv->capa.key_mgmt = WPA_DRIVER_CAPA_KEY_MGMT_WPA |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;
	drv->capa.enc = WPA_DRIVER_CAPA_ENC_WEP40 |
		WPA_DRIVER_CAPA_ENC_WEP104 |
		WPA_DRIVER_CAPA_ENC_TKIP |
		WPA_DRIVER_CAPA_ENC_CCMP;
	drv->capa.auth = WPA_DRIVER_AUTH_OPEN |
		WPA_DRIVER_AUTH_SHARED |
		WPA_DRIVER_AUTH_LEAP;

	drv->capa.flags |= WPA_DRIVER_FLAGS_SANE_ERROR_CODES;
	drv->capa.flags |= WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC_DONE;
	drv->capa.flags |= WPA_DRIVER_FLAGS_EAPOL_TX_STATUS;

	if (!info.device_ap_sme) {
		drv->capa.flags |= WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS;

		/*
		 * No AP SME is currently assumed to also indicate no AP MLME
		 * in the driver/firmware.
		 */
		drv->capa.flags |= WPA_DRIVER_FLAGS_AP_MLME;
	}

	drv->device_ap_sme = info.device_ap_sme;
	drv->poll_command_supported = info.poll_command_supported;
	drv->data_tx_status = info.data_tx_status;

	/*
	 * If poll command and tx status are supported, mac80211 is new enough
	 * to have everything we need to not need monitor interfaces.
	 */
	drv->use_monitor = !info.poll_command_supported || !info.data_tx_status;

	if (drv->device_ap_sme && drv->use_monitor) {
		/*
		 * Non-mac80211 drivers may not support monitor interface.
		 * Make sure we do not get stuck with incorrect capability here
		 * by explicitly testing this.
		 */
		if (!info.monitor_supported) {
			wpa_printf(MSG_DEBUG, "nl80211: Disable use_monitor "
				   "with device_ap_sme since no monitor mode "
				   "support detected");
			drv->use_monitor = 0;
		}
	}

	/*
	 * If we aren't going to use monitor interfaces, but the
	 * driver doesn't support data TX status, we won't get TX
	 * status for EAPOL frames.
	 */
	if (!drv->use_monitor && !info.data_tx_status)
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_EAPOL_TX_STATUS;

	return 0;
}


#ifdef ANDROID
static int android_genl_ctrl_resolve(struct nl_handle *handle,
				     const char *name)
{
	/*
	 * Android ICS has very minimal genl_ctrl_resolve() implementation, so
	 * need to work around that.
	 */
	struct nl_cache *cache = NULL;
	struct genl_family *nl80211 = NULL;
	int id = -1;

	if (genl_ctrl_alloc_cache(handle, &cache) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to allocate generic "
			   "netlink cache");
		goto fail;
	}

	nl80211 = genl_ctrl_search_by_name(cache, name);
	if (nl80211 == NULL)
		goto fail;

	id = genl_family_get_id(nl80211);

fail:
	if (nl80211)
		genl_family_put(nl80211);
	if (cache)
		nl_cache_free(cache);

	return id;
}
#define genl_ctrl_resolve android_genl_ctrl_resolve
#endif /* ANDROID */


static int wpa_driver_nl80211_init_nl_global(struct nl80211_global *global)
{
	int ret;

	global->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (global->nl_cb == NULL) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to allocate netlink "
			   "callbacks");
		return -1;
	}

	global->nl = nl_create_handle(global->nl_cb, "nl");
	if (global->nl == NULL)
		goto err;

	global->nl80211_id = genl_ctrl_resolve(global->nl, "nl80211");
	if (global->nl80211_id < 0) {
		wpa_printf(MSG_ERROR, "nl80211: 'nl80211' generic netlink not "
			   "found");
		goto err;
	}

	global->nl_event = nl_create_handle(global->nl_cb, "event");
	if (global->nl_event == NULL)
		goto err;

	ret = nl_get_multicast_id(global, "nl80211", "scan");
	if (ret >= 0)
		ret = nl_socket_add_membership(global->nl_event, ret);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not add multicast "
			   "membership for scan events: %d (%s)",
			   ret, strerror(-ret));
		goto err;
	}

	ret = nl_get_multicast_id(global, "nl80211", "mlme");
	if (ret >= 0)
		ret = nl_socket_add_membership(global->nl_event, ret);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not add multicast "
			   "membership for mlme events: %d (%s)",
			   ret, strerror(-ret));
		goto err;
	}

	ret = nl_get_multicast_id(global, "nl80211", "regulatory");
	if (ret >= 0)
		ret = nl_socket_add_membership(global->nl_event, ret);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Could not add multicast "
			   "membership for regulatory events: %d (%s)",
			   ret, strerror(-ret));
		/* Continue without regulatory events */
	}

	nl_cb_set(global->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  no_seq_check, NULL);
	nl_cb_set(global->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
		  process_global_event, global);

	eloop_register_read_sock(nl_socket_get_fd(global->nl_event),
				 wpa_driver_nl80211_event_receive,
				 global->nl_cb, global->nl_event);

	return 0;

err:
	nl_destroy_handles(&global->nl_event);
	nl_destroy_handles(&global->nl);
	nl_cb_put(global->nl_cb);
	global->nl_cb = NULL;
	return -1;
}


static int wpa_driver_nl80211_init_nl(struct wpa_driver_nl80211_data *drv)
{
	drv->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!drv->nl_cb) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to alloc cb struct");
		return -1;
	}

	nl_cb_set(drv->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  no_seq_check, NULL);
	nl_cb_set(drv->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
		  process_drv_event, drv);

	return 0;
}


static void wpa_driver_nl80211_rfkill_blocked(void *ctx)
{
	wpa_printf(MSG_DEBUG, "nl80211: RFKILL blocked");
	/*
	 * This may be for any interface; use ifdown event to disable
	 * interface.
	 */
}


static void wpa_driver_nl80211_rfkill_unblocked(void *ctx)
{
	struct wpa_driver_nl80211_data *drv = ctx;
	wpa_printf(MSG_DEBUG, "nl80211: RFKILL unblocked");
	if (linux_set_iface_flags(drv->global->ioctl_sock,
				  drv->first_bss.ifname, 1)) {
		wpa_printf(MSG_DEBUG, "nl80211: Could not set interface UP "
			   "after rfkill unblock");
		return;
	}
	/* rtnetlink ifup handler will report interface as enabled */
}


static void nl80211_get_phy_name(struct wpa_driver_nl80211_data *drv)
{
	/* Find phy (radio) to which this interface belongs */
	char buf[90], *pos;
	int f, rv;

	drv->phyname[0] = '\0';
	snprintf(buf, sizeof(buf) - 1, "/sys/class/net/%s/phy80211/name",
		 drv->first_bss.ifname);
	f = open(buf, O_RDONLY);
	if (f < 0) {
		wpa_printf(MSG_DEBUG, "Could not open file %s: %s",
			   buf, strerror(errno));
		return;
	}

	rv = read(f, drv->phyname, sizeof(drv->phyname) - 1);
	close(f);
	if (rv < 0) {
		wpa_printf(MSG_DEBUG, "Could not read file %s: %s",
			   buf, strerror(errno));
		return;
	}

	drv->phyname[rv] = '\0';
	pos = os_strchr(drv->phyname, '\n');
	if (pos)
		*pos = '\0';
	wpa_printf(MSG_DEBUG, "nl80211: interface %s in phy %s",
		   drv->first_bss.ifname, drv->phyname);
}


static void wpa_driver_nl80211_handle_eapol_tx_status(int sock,
						      void *eloop_ctx,
						      void *handle)
{
	struct wpa_driver_nl80211_data *drv = eloop_ctx;
	u8 data[2048];
	struct msghdr msg;
	struct iovec entry;
	u8 control[512];
	struct cmsghdr *cmsg;
	int res, found_ee = 0, found_wifi = 0, acked = 0;
	union wpa_event_data event;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &entry;
	msg.msg_iovlen = 1;
	entry.iov_base = data;
	entry.iov_len = sizeof(data);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	res = recvmsg(sock, &msg, MSG_ERRQUEUE);
	/* if error or not fitting 802.3 header, return */
	if (res < 14)
		return;

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_WIFI_STATUS) {
			int *ack;

			found_wifi = 1;
			ack = (void *)CMSG_DATA(cmsg);
			acked = *ack;
		}

		if (cmsg->cmsg_level == SOL_PACKET &&
		    cmsg->cmsg_type == PACKET_TX_TIMESTAMP) {
			struct sock_extended_err *err =
				(struct sock_extended_err *)CMSG_DATA(cmsg);

			if (err->ee_origin == SO_EE_ORIGIN_TXSTATUS)
				found_ee = 1;
		}
	}

	if (!found_ee || !found_wifi)
		return;

	memset(&event, 0, sizeof(event));
	event.eapol_tx_status.dst = data;
	event.eapol_tx_status.data = data + 14;
	event.eapol_tx_status.data_len = res - 14;
	event.eapol_tx_status.ack = acked;
	wpa_supplicant_event(drv->ctx, EVENT_EAPOL_TX_STATUS, &event);
}


static int nl80211_init_bss(struct i802_bss *bss)
{
	bss->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!bss->nl_cb)
		return -1;

	nl_cb_set(bss->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  no_seq_check, NULL);
	nl_cb_set(bss->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
		  process_bss_event, bss);

	return 0;
}


static void nl80211_destroy_bss(struct i802_bss *bss)
{
	nl_cb_put(bss->nl_cb);
	bss->nl_cb = NULL;
}


/**
 * wpa_driver_nl80211_init - Initialize nl80211 driver interface
 * @ctx: context to be used when calling wpa_supplicant functions,
 * e.g., wpa_supplicant_event()
 * @ifname: interface name, e.g., wlan0
 * @global_priv: private driver global data from global_init()
 * Returns: Pointer to private data, %NULL on failure
 */
static void * wpa_driver_nl80211_init(void *ctx, const char *ifname,
				      void *global_priv)
{
	struct wpa_driver_nl80211_data *drv;
	struct rfkill_config *rcfg;
	struct i802_bss *bss;

	if (global_priv == NULL)
		return NULL;
	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->global = global_priv;
	drv->ctx = ctx;
	bss = &drv->first_bss;
	bss->drv = drv;
	bss->ctx = ctx;

	os_strlcpy(bss->ifname, ifname, sizeof(bss->ifname));
	drv->monitor_ifidx = -1;
	drv->monitor_sock = -1;
	drv->eapol_tx_sock = -1;
	drv->ap_scan_as_station = NL80211_IFTYPE_UNSPECIFIED;

	if (wpa_driver_nl80211_init_nl(drv)) {
		os_free(drv);
		return NULL;
	}

	if (nl80211_init_bss(bss))
		goto failed;

	nl80211_get_phy_name(drv);

	rcfg = os_zalloc(sizeof(*rcfg));
	if (rcfg == NULL)
		goto failed;
	rcfg->ctx = drv;
	os_strlcpy(rcfg->ifname, ifname, sizeof(rcfg->ifname));
	rcfg->blocked_cb = wpa_driver_nl80211_rfkill_blocked;
	rcfg->unblocked_cb = wpa_driver_nl80211_rfkill_unblocked;
	drv->rfkill = rfkill_init(rcfg);
	if (drv->rfkill == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: RFKILL status not available");
		os_free(rcfg);
	}

	if (wpa_driver_nl80211_finish_drv_init(drv))
		goto failed;

	drv->eapol_tx_sock = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (drv->eapol_tx_sock < 0)
		goto failed;

	if (drv->data_tx_status) {
		int enabled = 1;

		if (setsockopt(drv->eapol_tx_sock, SOL_SOCKET, SO_WIFI_STATUS,
			       &enabled, sizeof(enabled)) < 0) {
			wpa_printf(MSG_DEBUG,
				"nl80211: wifi status sockopt failed\n");
			drv->data_tx_status = 0;
			if (!drv->use_monitor)
				drv->capa.flags &=
					~WPA_DRIVER_FLAGS_EAPOL_TX_STATUS;
		} else {
			eloop_register_read_sock(drv->eapol_tx_sock,
				wpa_driver_nl80211_handle_eapol_tx_status,
				drv, NULL);
		}
	}

	if (drv->global) {
		dl_list_add(&drv->global->interfaces, &drv->list);
		drv->in_interface_list = 1;
	}

	return bss;

failed:
	wpa_driver_nl80211_deinit(bss);
	return NULL;
}


static int nl80211_register_frame(struct i802_bss *bss,
				  struct nl_handle *nl_handle,
				  u16 type, const u8 *match, size_t match_len)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: Register frame type=0x%x nl_handle=%p",
		   type, nl_handle);
	wpa_hexdump(MSG_DEBUG, "nl80211: Register frame match",
		    match, match_len);

	nl80211_cmd(drv, msg, 0, NL80211_CMD_REGISTER_ACTION);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);
	NLA_PUT_U16(msg, NL80211_ATTR_FRAME_TYPE, type);
	NLA_PUT(msg, NL80211_ATTR_FRAME_MATCH, match_len, match);

	ret = send_and_recv(drv->global, nl_handle, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Register frame command "
			   "failed (type=%u): ret=%d (%s)",
			   type, ret, strerror(-ret));
		wpa_hexdump(MSG_DEBUG, "nl80211: Register frame match",
			    match, match_len);
		goto nla_put_failure;
	}
	ret = 0;
nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int nl80211_alloc_mgmt_handle(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (bss->nl_mgmt) {
		wpa_printf(MSG_DEBUG, "nl80211: Mgmt reporting "
			   "already on! (nl_mgmt=%p)", bss->nl_mgmt);
		return -1;
	}

	bss->nl_mgmt = nl_create_handle(drv->nl_cb, "mgmt");
	if (bss->nl_mgmt == NULL)
		return -1;

	eloop_register_read_sock(nl_socket_get_fd(bss->nl_mgmt),
				 wpa_driver_nl80211_event_receive, bss->nl_cb,
				 bss->nl_mgmt);

	return 0;
}


static int nl80211_register_action_frame(struct i802_bss *bss,
					 const u8 *match, size_t match_len)
{
	u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_ACTION << 4);
	return nl80211_register_frame(bss, bss->nl_mgmt,
				      type, match, match_len);
}


static int nl80211_mgmt_subscribe_non_ap(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (nl80211_alloc_mgmt_handle(bss))
		return -1;
	wpa_printf(MSG_DEBUG, "nl80211: Subscribe to mgmt frames with non-AP "
		   "handle %p", bss->nl_mgmt);

#if defined(CONFIG_P2P) || defined(CONFIG_INTERWORKING)
	/* GAS Initial Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0a", 2) < 0)
		return -1;
	/* GAS Initial Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0b", 2) < 0)
		return -1;
	/* GAS Comeback Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0c", 2) < 0)
		return -1;
	/* GAS Comeback Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0d", 2) < 0)
		return -1;
#endif /* CONFIG_P2P || CONFIG_INTERWORKING */
#ifdef CONFIG_P2P
	/* P2P Public Action */
	if (nl80211_register_action_frame(bss,
					  (u8 *) "\x04\x09\x50\x6f\x9a\x09",
					  6) < 0)
		return -1;
	/* P2P Action */
	if (nl80211_register_action_frame(bss,
					  (u8 *) "\x7f\x50\x6f\x9a\x09",
					  5) < 0)
		return -1;
#endif /* CONFIG_P2P */
#ifdef CONFIG_IEEE80211W
	/* SA Query Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x08\x01", 2) < 0)
		return -1;
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_TDLS
	if ((drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT)) {
		/* TDLS Discovery Response */
		if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0e", 2) <
		    0)
			return -1;
	}
#endif /* CONFIG_TDLS */

	/* FT Action frames */
	if (nl80211_register_action_frame(bss, (u8 *) "\x06", 1) < 0)
		return -1;
	else
		drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT |
			WPA_DRIVER_CAPA_KEY_MGMT_FT_PSK;

	/* WNM - BSS Transition Management Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x07", 2) < 0)
		return -1;
	/* WNM-Sleep Mode Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x11", 2) < 0)
		return -1;

	return 0;
}


static int nl80211_register_spurious_class3(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_UNEXPECTED_FRAME);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);

	ret = send_and_recv(drv->global, bss->nl_mgmt, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Register spurious class3 "
			   "failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto nla_put_failure;
	}
	ret = 0;
nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int nl80211_mgmt_subscribe_ap(struct i802_bss *bss)
{
	static const int stypes[] = {
		WLAN_FC_STYPE_AUTH,
		WLAN_FC_STYPE_ASSOC_REQ,
		WLAN_FC_STYPE_REASSOC_REQ,
		WLAN_FC_STYPE_DISASSOC,
		WLAN_FC_STYPE_DEAUTH,
		WLAN_FC_STYPE_ACTION,
		WLAN_FC_STYPE_PROBE_REQ,
/* Beacon doesn't work as mac80211 doesn't currently allow
 * it, but it wouldn't really be the right thing anyway as
 * it isn't per interface ... maybe just dump the scan
 * results periodically for OLBC?
 */
//		WLAN_FC_STYPE_BEACON,
	};
	unsigned int i;

	if (nl80211_alloc_mgmt_handle(bss))
		return -1;
	wpa_printf(MSG_DEBUG, "nl80211: Subscribe to mgmt frames with AP "
		   "handle %p", bss->nl_mgmt);

	for (i = 0; i < sizeof(stypes) / sizeof(stypes[0]); i++) {
		if (nl80211_register_frame(bss, bss->nl_mgmt,
					   (WLAN_FC_TYPE_MGMT << 2) |
					   (stypes[i] << 4),
					   NULL, 0) < 0) {
			goto out_err;
		}
	}

	if (nl80211_register_spurious_class3(bss))
		goto out_err;

	if (nl80211_get_wiphy_data_ap(bss) == NULL)
		goto out_err;

	return 0;

out_err:
	eloop_unregister_read_sock(nl_socket_get_fd(bss->nl_mgmt));
	nl_destroy_handles(&bss->nl_mgmt);
	return -1;
}


static int nl80211_mgmt_subscribe_ap_dev_sme(struct i802_bss *bss)
{
	if (nl80211_alloc_mgmt_handle(bss))
		return -1;
	wpa_printf(MSG_DEBUG, "nl80211: Subscribe to mgmt frames with AP "
		   "handle %p (device SME)", bss->nl_mgmt);

	if (nl80211_register_frame(bss, bss->nl_mgmt,
				   (WLAN_FC_TYPE_MGMT << 2) |
				   (WLAN_FC_STYPE_ACTION << 4),
				   NULL, 0) < 0)
		goto out_err;

	return 0;

out_err:
	eloop_unregister_read_sock(nl_socket_get_fd(bss->nl_mgmt));
	nl_destroy_handles(&bss->nl_mgmt);
	return -1;
}


static void nl80211_mgmt_unsubscribe(struct i802_bss *bss, const char *reason)
{
	if (bss->nl_mgmt == NULL)
		return;
	wpa_printf(MSG_DEBUG, "nl80211: Unsubscribe mgmt frames handle %p "
		   "(%s)", bss->nl_mgmt, reason);
	eloop_unregister_read_sock(nl_socket_get_fd(bss->nl_mgmt));
	nl_destroy_handles(&bss->nl_mgmt);

	nl80211_put_wiphy_data_ap(bss);
}


static void wpa_driver_nl80211_send_rfkill(void *eloop_ctx, void *timeout_ctx)
{
	wpa_supplicant_event(timeout_ctx, EVENT_INTERFACE_DISABLED, NULL);
}


static int
wpa_driver_nl80211_finish_drv_init(struct wpa_driver_nl80211_data *drv)
{
	struct i802_bss *bss = &drv->first_bss;
	int send_rfkill_event = 0;

	drv->ifindex = if_nametoindex(bss->ifname);
	drv->first_bss.ifindex = drv->ifindex;

#ifndef HOSTAPD
	/*
	 * Make sure the interface starts up in station mode unless this is a
	 * dynamically added interface (e.g., P2P) that was already configured
	 * with proper iftype.
	 */
	if (drv->ifindex != drv->global->if_add_ifindex &&
	    wpa_driver_nl80211_set_mode(bss, NL80211_IFTYPE_STATION) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not configure driver to "
			   "use managed mode");
		return -1;
	}

	if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1)) {
		if (rfkill_is_blocked(drv->rfkill)) {
			wpa_printf(MSG_DEBUG, "nl80211: Could not yet enable "
				   "interface '%s' due to rfkill",
				   bss->ifname);
			drv->if_disabled = 1;
			send_rfkill_event = 1;
		} else {
			wpa_printf(MSG_ERROR, "nl80211: Could not set "
				   "interface '%s' UP", bss->ifname);
			return -1;
		}
	}

	netlink_send_oper_ifla(drv->global->netlink, drv->ifindex,
			       1, IF_OPER_DORMANT);
#endif /* HOSTAPD */

	if (wpa_driver_nl80211_capa(drv))
		return -1;

	if (linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
			       bss->addr))
		return -1;

	if (send_rfkill_event) {
		eloop_register_timeout(0, 0, wpa_driver_nl80211_send_rfkill,
				       drv, drv->ctx);
	}

	return 0;
}


static int wpa_driver_nl80211_del_beacon(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_DEL_BEACON);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	return send_and_recv_msgs(drv, msg, NULL, NULL);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


/**
 * wpa_driver_nl80211_deinit - Deinitialize nl80211 driver interface
 * @priv: Pointer to private nl80211 data from wpa_driver_nl80211_init()
 *
 * Shut down driver interface and processing of driver events. Free
 * private data buffer if one was allocated in wpa_driver_nl80211_init().
 */
static void wpa_driver_nl80211_deinit(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	bss->in_deinit = 1;
	if (drv->data_tx_status)
		eloop_unregister_read_sock(drv->eapol_tx_sock);
	if (drv->eapol_tx_sock >= 0)
		close(drv->eapol_tx_sock);

	if (bss->nl_preq)
		wpa_driver_nl80211_probe_req_report(bss, 0);
	if (bss->added_if_into_bridge) {
		if (linux_br_del_if(drv->global->ioctl_sock, bss->brname,
				    bss->ifname) < 0)
			wpa_printf(MSG_INFO, "nl80211: Failed to remove "
				   "interface %s from bridge %s: %s",
				   bss->ifname, bss->brname, strerror(errno));
	}
	if (bss->added_bridge) {
		if (linux_br_del(drv->global->ioctl_sock, bss->brname) < 0)
			wpa_printf(MSG_INFO, "nl80211: Failed to remove "
				   "bridge %s: %s",
				   bss->brname, strerror(errno));
	}

	nl80211_remove_monitor_interface(drv);

	if (is_ap_interface(drv->nlmode))
		wpa_driver_nl80211_del_beacon(drv);

#ifdef HOSTAPD
	if (drv->last_freq_ht) {
		/* Clear HT flags from the driver */
		struct hostapd_freq_params freq;
		os_memset(&freq, 0, sizeof(freq));
		freq.freq = drv->last_freq;
		i802_set_freq(priv, &freq);
	}

	if (drv->eapol_sock >= 0) {
		eloop_unregister_read_sock(drv->eapol_sock);
		close(drv->eapol_sock);
	}

	if (drv->if_indices != drv->default_if_indices)
		os_free(drv->if_indices);
#endif /* HOSTAPD */

	if (drv->disabled_11b_rates)
		nl80211_disable_11b_rates(drv, drv->ifindex, 0);

	netlink_send_oper_ifla(drv->global->netlink, drv->ifindex, 0,
			       IF_OPER_UP);
	rfkill_deinit(drv->rfkill);

	eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv, drv->ctx);

	(void) linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 0);
	wpa_driver_nl80211_set_mode(bss, NL80211_IFTYPE_STATION);
	nl80211_mgmt_unsubscribe(bss, "deinit");

	nl_cb_put(drv->nl_cb);

	nl80211_destroy_bss(&drv->first_bss);

	os_free(drv->filter_ssids);

	os_free(drv->auth_ie);

	if (drv->in_interface_list)
		dl_list_del(&drv->list);

	os_free(drv);
}


/**
 * wpa_driver_nl80211_scan_timeout - Scan timeout to report scan completion
 * @eloop_ctx: Driver private data
 * @timeout_ctx: ctx argument given to wpa_driver_nl80211_init()
 *
 * This function can be used as registered timeout when starting a scan to
 * generate a scan completed event if the driver does not report this.
 */
static void wpa_driver_nl80211_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_driver_nl80211_data *drv = eloop_ctx;
	if (drv->ap_scan_as_station != NL80211_IFTYPE_UNSPECIFIED) {
		wpa_driver_nl80211_set_mode(&drv->first_bss,
					    drv->ap_scan_as_station);
		drv->ap_scan_as_station = NL80211_IFTYPE_UNSPECIFIED;
	}
	wpa_printf(MSG_DEBUG, "Scan timeout - try to get results");
	wpa_supplicant_event(timeout_ctx, EVENT_SCAN_RESULTS, NULL);
}


static struct nl_msg *
nl80211_scan_common(struct wpa_driver_nl80211_data *drv, u8 cmd,
		    struct wpa_driver_scan_params *params)
{
	struct nl_msg *msg;
	int err;
	size_t i;

	msg = nlmsg_alloc();
	if (!msg)
		return NULL;

	nl80211_cmd(drv, msg, 0, cmd);

	if (nla_put_u32(msg, NL80211_ATTR_IFINDEX, drv->ifindex) < 0)
		goto fail;

	if (params->num_ssids) {
		struct nl_msg *ssids = nlmsg_alloc();
		if (ssids == NULL)
			goto fail;
		for (i = 0; i < params->num_ssids; i++) {
			wpa_hexdump_ascii(MSG_MSGDUMP, "nl80211: Scan SSID",
					  params->ssids[i].ssid,
					  params->ssids[i].ssid_len);
			if (nla_put(ssids, i + 1, params->ssids[i].ssid_len,
				    params->ssids[i].ssid) < 0) {
				nlmsg_free(ssids);
				goto fail;
			}
		}
		err = nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids);
		nlmsg_free(ssids);
		if (err < 0)
			goto fail;
	}

	if (params->extra_ies) {
		wpa_hexdump(MSG_MSGDUMP, "nl80211: Scan extra IEs",
			    params->extra_ies, params->extra_ies_len);
		if (nla_put(msg, NL80211_ATTR_IE, params->extra_ies_len,
			    params->extra_ies) < 0)
			goto fail;
	}

	if (params->freqs) {
		struct nl_msg *freqs = nlmsg_alloc();
		if (freqs == NULL)
			goto fail;
		for (i = 0; params->freqs[i]; i++) {
			wpa_printf(MSG_MSGDUMP, "nl80211: Scan frequency %u "
				   "MHz", params->freqs[i]);
			if (nla_put_u32(freqs, i + 1, params->freqs[i]) < 0) {
				nlmsg_free(freqs);
				goto fail;
			}
		}
		err = nla_put_nested(msg, NL80211_ATTR_SCAN_FREQUENCIES,
				     freqs);
		nlmsg_free(freqs);
		if (err < 0)
			goto fail;
	}

	os_free(drv->filter_ssids);
	drv->filter_ssids = params->filter_ssids;
	params->filter_ssids = NULL;
	drv->num_filter_ssids = params->num_filter_ssids;

	return msg;

fail:
	nlmsg_free(msg);
	return NULL;
}


/**
 * wpa_driver_nl80211_scan - Request the driver to initiate scan
 * @priv: Pointer to private driver data from wpa_driver_nl80211_init()
 * @params: Scan parameters
 * Returns: 0 on success, -1 on failure
 */
static int wpa_driver_nl80211_scan(void *priv,
				   struct wpa_driver_scan_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1, timeout;
	struct nl_msg *msg, *rates = NULL;

	drv->scan_for_auth = 0;

	msg = nl80211_scan_common(drv, NL80211_CMD_TRIGGER_SCAN, params);
	if (!msg)
		return -1;

	if (params->p2p_probe) {
		wpa_printf(MSG_DEBUG, "nl80211: P2P probe - mask SuppRates");

		rates = nlmsg_alloc();
		if (rates == NULL)
			goto nla_put_failure;

		/*
		 * Remove 2.4 GHz rates 1, 2, 5.5, 11 Mbps from supported rates
		 * by masking out everything else apart from the OFDM rates 6,
		 * 9, 12, 18, 24, 36, 48, 54 Mbps from non-MCS rates. All 5 GHz
		 * rates are left enabled.
		 */
		NLA_PUT(rates, NL80211_BAND_2GHZ, 8,
			"\x0c\x12\x18\x24\x30\x48\x60\x6c");
		if (nla_put_nested(msg, NL80211_ATTR_SCAN_SUPP_RATES, rates) <
		    0)
			goto nla_put_failure;

		NLA_PUT_FLAG(msg, NL80211_ATTR_TX_NO_CCK_RATE);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Scan trigger failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
#ifdef HOSTAPD
		if (is_ap_interface(drv->nlmode)) {
			/*
			 * mac80211 does not allow scan requests in AP mode, so
			 * try to do this in station mode.
			 */
			if (wpa_driver_nl80211_set_mode(
				    bss, NL80211_IFTYPE_STATION))
				goto nla_put_failure;

			if (wpa_driver_nl80211_scan(drv, params)) {
				wpa_driver_nl80211_set_mode(bss, drv->nlmode);
				goto nla_put_failure;
			}

			/* Restore AP mode when processing scan results */
			drv->ap_scan_as_station = drv->nlmode;
			ret = 0;
		} else
			goto nla_put_failure;
#else /* HOSTAPD */
		goto nla_put_failure;
#endif /* HOSTAPD */
	}

	/* Not all drivers generate "scan completed" wireless event, so try to
	 * read results after a timeout. */
	timeout = 10;
	if (drv->scan_complete_events) {
		/*
		 * The driver seems to deliver events to notify when scan is
		 * complete, so use longer timeout to avoid race conditions
		 * with scanning and following association request.
		 */
		timeout = 30;
	}
	wpa_printf(MSG_DEBUG, "Scan requested (ret=%d) - scan timeout %d "
		   "seconds", ret, timeout);
	eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv, drv->ctx);
	eloop_register_timeout(timeout, 0, wpa_driver_nl80211_scan_timeout,
			       drv, drv->ctx);

nla_put_failure:
	nlmsg_free(msg);
	nlmsg_free(rates);
	return ret;
}


/**
 * wpa_driver_nl80211_sched_scan - Initiate a scheduled scan
 * @priv: Pointer to private driver data from wpa_driver_nl80211_init()
 * @params: Scan parameters
 * @interval: Interval between scan cycles in milliseconds
 * Returns: 0 on success, -1 on failure or if not supported
 */
static int wpa_driver_nl80211_sched_scan(void *priv,
					 struct wpa_driver_scan_params *params,
					 u32 interval)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1;
	struct nl_msg *msg;
	struct nl_msg *match_set_ssid = NULL, *match_sets = NULL;
	struct nl_msg *match_set_rssi = NULL;
	size_t i;

#ifdef ANDROID
	if (!drv->capa.sched_scan_supported)
		return android_pno_start(bss, params);
#endif /* ANDROID */

	msg = nl80211_scan_common(drv, NL80211_CMD_START_SCHED_SCAN, params);
	if (!msg)
		goto nla_put_failure;

	NLA_PUT_U32(msg, NL80211_ATTR_SCHED_SCAN_INTERVAL, interval);

	if ((drv->num_filter_ssids &&
	    (int) drv->num_filter_ssids <= drv->capa.max_match_sets) ||
	    params->filter_rssi) {
		match_sets = nlmsg_alloc();
		if (match_sets == NULL)
			goto nla_put_failure;

		for (i = 0; i < drv->num_filter_ssids; i++) {
			wpa_hexdump_ascii(MSG_MSGDUMP,
					  "nl80211: Sched scan filter SSID",
					  drv->filter_ssids[i].ssid,
					  drv->filter_ssids[i].ssid_len);

			match_set_ssid = nlmsg_alloc();
			if (match_set_ssid == NULL)
				goto nla_put_failure;
			NLA_PUT(match_set_ssid,
				NL80211_ATTR_SCHED_SCAN_MATCH_SSID,
				drv->filter_ssids[i].ssid_len,
				drv->filter_ssids[i].ssid);

			if (nla_put_nested(match_sets, i + 1, match_set_ssid) <
			    0)
				goto nla_put_failure;
		}

		if (params->filter_rssi) {
			match_set_rssi = nlmsg_alloc();
			if (match_set_rssi == NULL)
				goto nla_put_failure;
			NLA_PUT_U32(match_set_rssi,
				    NL80211_SCHED_SCAN_MATCH_ATTR_RSSI,
				    params->filter_rssi);
			wpa_printf(MSG_MSGDUMP,
				   "nl80211: Sched scan RSSI filter %d dBm",
				   params->filter_rssi);
			if (nla_put_nested(match_sets, 0, match_set_rssi) < 0)
				goto nla_put_failure;
		}

		if (nla_put_nested(msg, NL80211_ATTR_SCHED_SCAN_MATCH,
				   match_sets) < 0)
			goto nla_put_failure;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);

	/* TODO: if we get an error here, we should fall back to normal scan */

	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Sched scan start failed: "
			   "ret=%d (%s)", ret, strerror(-ret));
		goto nla_put_failure;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Sched scan requested (ret=%d) - "
		   "scan interval %d msec", ret, interval);

nla_put_failure:
	nlmsg_free(match_set_ssid);
	nlmsg_free(match_sets);
	nlmsg_free(match_set_rssi);
	nlmsg_free(msg);
	return ret;
}


/**
 * wpa_driver_nl80211_stop_sched_scan - Stop a scheduled scan
 * @priv: Pointer to private driver data from wpa_driver_nl80211_init()
 * Returns: 0 on success, -1 on failure or if not supported
 */
static int wpa_driver_nl80211_stop_sched_scan(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = 0;
	struct nl_msg *msg;

#ifdef ANDROID
	if (!drv->capa.sched_scan_supported)
		return android_pno_stop(bss);
#endif /* ANDROID */

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_STOP_SCHED_SCAN);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Sched scan stop failed: "
			   "ret=%d (%s)", ret, strerror(-ret));
		goto nla_put_failure;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Sched scan stop sent (ret=%d)", ret);

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static const u8 * nl80211_get_ie(const u8 *ies, size_t ies_len, u8 ie)
{
	const u8 *end, *pos;

	if (ies == NULL)
		return NULL;

	pos = ies;
	end = ies + ies_len;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == ie)
			return pos;
		pos += 2 + pos[1];
	}

	return NULL;
}


static int nl80211_scan_filtered(struct wpa_driver_nl80211_data *drv,
				 const u8 *ie, size_t ie_len)
{
	const u8 *ssid;
	size_t i;

	if (drv->filter_ssids == NULL)
		return 0;

	ssid = nl80211_get_ie(ie, ie_len, WLAN_EID_SSID);
	if (ssid == NULL)
		return 1;

	for (i = 0; i < drv->num_filter_ssids; i++) {
		if (ssid[1] == drv->filter_ssids[i].ssid_len &&
		    os_memcmp(ssid + 2, drv->filter_ssids[i].ssid, ssid[1]) ==
		    0)
			return 0;
	}

	return 1;
}


static int bss_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_BSSID] = { .type = NLA_UNSPEC },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_TSF] = { .type = NLA_U64 },
		[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
		[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { .type = NLA_UNSPEC },
		[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
		[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
		[NL80211_BSS_STATUS] = { .type = NLA_U32 },
		[NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
		[NL80211_BSS_BEACON_IES] = { .type = NLA_UNSPEC },
	};
	struct nl80211_bss_info_arg *_arg = arg;
	struct wpa_scan_results *res = _arg->res;
	struct wpa_scan_res **tmp;
	struct wpa_scan_res *r;
	const u8 *ie, *beacon_ie;
	size_t ie_len, beacon_ie_len;
	u8 *pos;
	size_t i;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_BSS])
		return NL_SKIP;
	if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS],
			     bss_policy))
		return NL_SKIP;
	if (bss[NL80211_BSS_STATUS]) {
		enum nl80211_bss_status status;
		status = nla_get_u32(bss[NL80211_BSS_STATUS]);
		if (status == NL80211_BSS_STATUS_ASSOCIATED &&
		    bss[NL80211_BSS_FREQUENCY]) {
			_arg->assoc_freq =
				nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
			wpa_printf(MSG_DEBUG, "nl80211: Associated on %u MHz",
				   _arg->assoc_freq);
		}
		if (status == NL80211_BSS_STATUS_ASSOCIATED &&
		    bss[NL80211_BSS_BSSID]) {
			os_memcpy(_arg->assoc_bssid,
				  nla_data(bss[NL80211_BSS_BSSID]), ETH_ALEN);
			wpa_printf(MSG_DEBUG, "nl80211: Associated with "
				   MACSTR, MAC2STR(_arg->assoc_bssid));
		}
	}
	if (!res)
		return NL_SKIP;
	if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
		ie = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
		ie_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
	} else {
		ie = NULL;
		ie_len = 0;
	}
	if (bss[NL80211_BSS_BEACON_IES]) {
		beacon_ie = nla_data(bss[NL80211_BSS_BEACON_IES]);
		beacon_ie_len = nla_len(bss[NL80211_BSS_BEACON_IES]);
	} else {
		beacon_ie = NULL;
		beacon_ie_len = 0;
	}

	if (nl80211_scan_filtered(_arg->drv, ie ? ie : beacon_ie,
				  ie ? ie_len : beacon_ie_len))
		return NL_SKIP;

	r = os_zalloc(sizeof(*r) + ie_len + beacon_ie_len);
	if (r == NULL)
		return NL_SKIP;
	if (bss[NL80211_BSS_BSSID])
		os_memcpy(r->bssid, nla_data(bss[NL80211_BSS_BSSID]),
			  ETH_ALEN);
	if (bss[NL80211_BSS_FREQUENCY])
		r->freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
	if (bss[NL80211_BSS_BEACON_INTERVAL])
		r->beacon_int = nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]);
	if (bss[NL80211_BSS_CAPABILITY])
		r->caps = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
	r->flags |= WPA_SCAN_NOISE_INVALID;
	if (bss[NL80211_BSS_SIGNAL_MBM]) {
		r->level = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
		r->level /= 100; /* mBm to dBm */
		r->flags |= WPA_SCAN_LEVEL_DBM | WPA_SCAN_QUAL_INVALID;
	} else if (bss[NL80211_BSS_SIGNAL_UNSPEC]) {
		r->level = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
		r->flags |= WPA_SCAN_QUAL_INVALID;
	} else
		r->flags |= WPA_SCAN_LEVEL_INVALID | WPA_SCAN_QUAL_INVALID;
	if (bss[NL80211_BSS_TSF])
		r->tsf = nla_get_u64(bss[NL80211_BSS_TSF]);
	if (bss[NL80211_BSS_SEEN_MS_AGO])
		r->age = nla_get_u32(bss[NL80211_BSS_SEEN_MS_AGO]);
	r->ie_len = ie_len;
	pos = (u8 *) (r + 1);
	if (ie) {
		os_memcpy(pos, ie, ie_len);
		pos += ie_len;
	}
	r->beacon_ie_len = beacon_ie_len;
	if (beacon_ie)
		os_memcpy(pos, beacon_ie, beacon_ie_len);

	if (bss[NL80211_BSS_STATUS]) {
		enum nl80211_bss_status status;
		status = nla_get_u32(bss[NL80211_BSS_STATUS]);
		switch (status) {
		case NL80211_BSS_STATUS_AUTHENTICATED:
			r->flags |= WPA_SCAN_AUTHENTICATED;
			break;
		case NL80211_BSS_STATUS_ASSOCIATED:
			r->flags |= WPA_SCAN_ASSOCIATED;
			break;
		default:
			break;
		}
	}

	/*
	 * cfg80211 maintains separate BSS table entries for APs if the same
	 * BSSID,SSID pair is seen on multiple channels. wpa_supplicant does
	 * not use frequency as a separate key in the BSS table, so filter out
	 * duplicated entries. Prefer associated BSS entry in such a case in
	 * order to get the correct frequency into the BSS table.
	 */
	for (i = 0; i < res->num; i++) {
		const u8 *s1, *s2;
		if (os_memcmp(res->res[i]->bssid, r->bssid, ETH_ALEN) != 0)
			continue;

		s1 = nl80211_get_ie((u8 *) (res->res[i] + 1),
				    res->res[i]->ie_len, WLAN_EID_SSID);
		s2 = nl80211_get_ie((u8 *) (r + 1), r->ie_len, WLAN_EID_SSID);
		if (s1 == NULL || s2 == NULL || s1[1] != s2[1] ||
		    os_memcmp(s1, s2, 2 + s1[1]) != 0)
			continue;

		/* Same BSSID,SSID was already included in scan results */
		wpa_printf(MSG_DEBUG, "nl80211: Remove duplicated scan result "
			   "for " MACSTR, MAC2STR(r->bssid));

		if ((r->flags & WPA_SCAN_ASSOCIATED) &&
		    !(res->res[i]->flags & WPA_SCAN_ASSOCIATED)) {
			os_free(res->res[i]);
			res->res[i] = r;
		} else
			os_free(r);
		return NL_SKIP;
	}

	tmp = os_realloc_array(res->res, res->num + 1,
			       sizeof(struct wpa_scan_res *));
	if (tmp == NULL) {
		os_free(r);
		return NL_SKIP;
	}
	tmp[res->num++] = r;
	res->res = tmp;

	return NL_SKIP;
}


static void clear_state_mismatch(struct wpa_driver_nl80211_data *drv,
				 const u8 *addr)
{
	if (drv->capa.flags & WPA_DRIVER_FLAGS_SME) {
		wpa_printf(MSG_DEBUG, "nl80211: Clear possible state "
			   "mismatch (" MACSTR ")", MAC2STR(addr));
		wpa_driver_nl80211_mlme(drv, addr,
					NL80211_CMD_DEAUTHENTICATE,
					WLAN_REASON_PREV_AUTH_NOT_VALID, 1);
	}
}


static void wpa_driver_nl80211_check_bss_status(
	struct wpa_driver_nl80211_data *drv, struct wpa_scan_results *res)
{
	size_t i;

	for (i = 0; i < res->num; i++) {
		struct wpa_scan_res *r = res->res[i];
		if (r->flags & WPA_SCAN_AUTHENTICATED) {
			wpa_printf(MSG_DEBUG, "nl80211: Scan results "
				   "indicates BSS status with " MACSTR
				   " as authenticated",
				   MAC2STR(r->bssid));
			if (is_sta_interface(drv->nlmode) &&
			    os_memcmp(r->bssid, drv->bssid, ETH_ALEN) != 0 &&
			    os_memcmp(r->bssid, drv->auth_bssid, ETH_ALEN) !=
			    0) {
				wpa_printf(MSG_DEBUG, "nl80211: Unknown BSSID"
					   " in local state (auth=" MACSTR
					   " assoc=" MACSTR ")",
					   MAC2STR(drv->auth_bssid),
					   MAC2STR(drv->bssid));
				clear_state_mismatch(drv, r->bssid);
			}
		}

		if (r->flags & WPA_SCAN_ASSOCIATED) {
			wpa_printf(MSG_DEBUG, "nl80211: Scan results "
				   "indicate BSS status with " MACSTR
				   " as associated",
				   MAC2STR(r->bssid));
			if (is_sta_interface(drv->nlmode) &&
			    !drv->associated) {
				wpa_printf(MSG_DEBUG, "nl80211: Local state "
					   "(not associated) does not match "
					   "with BSS state");
				clear_state_mismatch(drv, r->bssid);
			} else if (is_sta_interface(drv->nlmode) &&
				   os_memcmp(drv->bssid, r->bssid, ETH_ALEN) !=
				   0) {
				wpa_printf(MSG_DEBUG, "nl80211: Local state "
					   "(associated with " MACSTR ") does "
					   "not match with BSS state",
					   MAC2STR(drv->bssid));
				clear_state_mismatch(drv, r->bssid);
				clear_state_mismatch(drv, drv->bssid);
			}
		}
	}
}


static struct wpa_scan_results *
nl80211_get_scan_results(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	struct wpa_scan_results *res;
	int ret;
	struct nl80211_bss_info_arg arg;

	res = os_zalloc(sizeof(*res));
	if (res == NULL)
		return NULL;
	msg = nlmsg_alloc();
	if (!msg)
		goto nla_put_failure;

	nl80211_cmd(drv, msg, NLM_F_DUMP, NL80211_CMD_GET_SCAN);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	arg.drv = drv;
	arg.res = res;
	ret = send_and_recv_msgs(drv, msg, bss_info_handler, &arg);
	msg = NULL;
	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Received scan results (%lu "
			   "BSSes)", (unsigned long) res->num);
		nl80211_get_noise_for_scan_results(drv, res);
		return res;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Scan result fetch failed: ret=%d "
		   "(%s)", ret, strerror(-ret));
nla_put_failure:
	nlmsg_free(msg);
	wpa_scan_results_free(res);
	return NULL;
}


/**
 * wpa_driver_nl80211_get_scan_results - Fetch the latest scan results
 * @priv: Pointer to private wext data from wpa_driver_nl80211_init()
 * Returns: Scan results on success, -1 on failure
 */
static struct wpa_scan_results *
wpa_driver_nl80211_get_scan_results(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct wpa_scan_results *res;

	res = nl80211_get_scan_results(drv);
	if (res)
		wpa_driver_nl80211_check_bss_status(drv, res);
	return res;
}


static void nl80211_dump_scan(struct wpa_driver_nl80211_data *drv)
{
	struct wpa_scan_results *res;
	size_t i;

	res = nl80211_get_scan_results(drv);
	if (res == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: Failed to get scan results");
		return;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Scan result dump");
	for (i = 0; i < res->num; i++) {
		struct wpa_scan_res *r = res->res[i];
		wpa_printf(MSG_DEBUG, "nl80211: %d/%d " MACSTR "%s%s",
			   (int) i, (int) res->num, MAC2STR(r->bssid),
			   r->flags & WPA_SCAN_AUTHENTICATED ? " [auth]" : "",
			   r->flags & WPA_SCAN_ASSOCIATED ? " [assoc]" : "");
	}

	wpa_scan_results_free(res);
}


static int wpa_driver_nl80211_set_key(const char *ifname, void *priv,
				      enum wpa_alg alg, const u8 *addr,
				      int key_idx, int set_tx,
				      const u8 *seq, size_t seq_len,
				      const u8 *key, size_t key_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ifindex = if_nametoindex(ifname);
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: ifindex=%d alg=%d addr=%p key_idx=%d "
		   "set_tx=%d seq_len=%lu key_len=%lu",
		   __func__, ifindex, alg, addr, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);
#ifdef CONFIG_TDLS
	if (key_idx == -1)
		key_idx = 0;
#endif /* CONFIG_TDLS */

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	if (alg == WPA_ALG_NONE) {
		nl80211_cmd(drv, msg, 0, NL80211_CMD_DEL_KEY);
	} else {
		nl80211_cmd(drv, msg, 0, NL80211_CMD_NEW_KEY);
		NLA_PUT(msg, NL80211_ATTR_KEY_DATA, key_len, key);
		switch (alg) {
		case WPA_ALG_WEP:
			if (key_len == 5)
				NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
					    WLAN_CIPHER_SUITE_WEP40);
			else
				NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
					    WLAN_CIPHER_SUITE_WEP104);
			break;
		case WPA_ALG_TKIP:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_TKIP);
			break;
		case WPA_ALG_CCMP:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_CCMP);
			break;
		case WPA_ALG_GCMP:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_GCMP);
			break;
		case WPA_ALG_IGTK:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_AES_CMAC);
			break;
		case WPA_ALG_SMS4:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_SMS4);
			break;
		case WPA_ALG_KRK:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_KRK);
			break;
		default:
			wpa_printf(MSG_ERROR, "%s: Unsupported encryption "
				   "algorithm %d", __func__, alg);
			nlmsg_free(msg);
			return -1;
		}
	}

	if (seq && seq_len)
		NLA_PUT(msg, NL80211_ATTR_KEY_SEQ, seq_len, seq);

	if (addr && !is_broadcast_ether_addr(addr)) {
		wpa_printf(MSG_DEBUG, "   addr=" MACSTR, MAC2STR(addr));
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);

		if (alg != WPA_ALG_WEP && key_idx && !set_tx) {
			wpa_printf(MSG_DEBUG, "   RSN IBSS RX GTK");
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_TYPE,
				    NL80211_KEYTYPE_GROUP);
		}
	} else if (addr && is_broadcast_ether_addr(addr)) {
		struct nl_msg *types;
		int err;
		wpa_printf(MSG_DEBUG, "   broadcast key");
		types = nlmsg_alloc();
		if (!types)
			goto nla_put_failure;
		NLA_PUT_FLAG(types, NL80211_KEY_DEFAULT_TYPE_MULTICAST);
		err = nla_put_nested(msg, NL80211_ATTR_KEY_DEFAULT_TYPES,
				     types);
		nlmsg_free(types);
		if (err)
			goto nla_put_failure;
	}
	NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, key_idx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if ((ret == -ENOENT || ret == -ENOLINK) && alg == WPA_ALG_NONE)
		ret = 0;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: set_key failed; err=%d %s)",
			   ret, strerror(-ret));

	/*
	 * If we failed or don't need to set the default TX key (below),
	 * we're done here.
	 */
	if (ret || !set_tx || alg == WPA_ALG_NONE)
		return ret;
	if (is_ap_interface(drv->nlmode) && addr &&
	    !is_broadcast_ether_addr(addr))
		return ret;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_KEY);
	NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, key_idx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);
	if (alg == WPA_ALG_IGTK)
		NLA_PUT_FLAG(msg, NL80211_ATTR_KEY_DEFAULT_MGMT);
	else
		NLA_PUT_FLAG(msg, NL80211_ATTR_KEY_DEFAULT);
	if (addr && is_broadcast_ether_addr(addr)) {
		struct nl_msg *types;
		int err;
		types = nlmsg_alloc();
		if (!types)
			goto nla_put_failure;
		NLA_PUT_FLAG(types, NL80211_KEY_DEFAULT_TYPE_MULTICAST);
		err = nla_put_nested(msg, NL80211_ATTR_KEY_DEFAULT_TYPES,
				     types);
		nlmsg_free(types);
		if (err)
			goto nla_put_failure;
	} else if (addr) {
		struct nl_msg *types;
		int err;
		types = nlmsg_alloc();
		if (!types)
			goto nla_put_failure;
		NLA_PUT_FLAG(types, NL80211_KEY_DEFAULT_TYPE_UNICAST);
		err = nla_put_nested(msg, NL80211_ATTR_KEY_DEFAULT_TYPES,
				     types);
		nlmsg_free(types);
		if (err)
			goto nla_put_failure;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret == -ENOENT)
		ret = 0;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: set_key default failed; "
			   "err=%d %s)", ret, strerror(-ret));
	return ret;

nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int nl_add_key(struct nl_msg *msg, enum wpa_alg alg,
		      int key_idx, int defkey,
		      const u8 *seq, size_t seq_len,
		      const u8 *key, size_t key_len)
{
	struct nlattr *key_attr = nla_nest_start(msg, NL80211_ATTR_KEY);
	if (!key_attr)
		return -1;

	if (defkey && alg == WPA_ALG_IGTK)
		NLA_PUT_FLAG(msg, NL80211_KEY_DEFAULT_MGMT);
	else if (defkey)
		NLA_PUT_FLAG(msg, NL80211_KEY_DEFAULT);

	NLA_PUT_U8(msg, NL80211_KEY_IDX, key_idx);

	switch (alg) {
	case WPA_ALG_WEP:
		if (key_len == 5)
			NLA_PUT_U32(msg, NL80211_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_WEP40);
		else
			NLA_PUT_U32(msg, NL80211_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_WEP104);
		break;
	case WPA_ALG_TKIP:
		NLA_PUT_U32(msg, NL80211_KEY_CIPHER, WLAN_CIPHER_SUITE_TKIP);
		break;
	case WPA_ALG_CCMP:
		NLA_PUT_U32(msg, NL80211_KEY_CIPHER, WLAN_CIPHER_SUITE_CCMP);
		break;
	case WPA_ALG_GCMP:
		NLA_PUT_U32(msg, NL80211_KEY_CIPHER, WLAN_CIPHER_SUITE_GCMP);
		break;
	case WPA_ALG_IGTK:
		NLA_PUT_U32(msg, NL80211_KEY_CIPHER,
			    WLAN_CIPHER_SUITE_AES_CMAC);
		break;
	default:
		wpa_printf(MSG_ERROR, "%s: Unsupported encryption "
			   "algorithm %d", __func__, alg);
		return -1;
	}

	if (seq && seq_len)
		NLA_PUT(msg, NL80211_KEY_SEQ, seq_len, seq);

	NLA_PUT(msg, NL80211_KEY_DATA, key_len, key);

	nla_nest_end(msg, key_attr);

	return 0;
 nla_put_failure:
	return -1;
}


static int nl80211_set_conn_keys(struct wpa_driver_associate_params *params,
				 struct nl_msg *msg)
{
	int i, privacy = 0;
	struct nlattr *nl_keys, *nl_key;

	for (i = 0; i < 4; i++) {
		if (!params->wep_key[i])
			continue;
		privacy = 1;
		break;
	}
	if (params->wps == WPS_MODE_PRIVACY)
		privacy = 1;
	if (params->pairwise_suite &&
	    params->pairwise_suite != WPA_CIPHER_NONE)
		privacy = 1;

	if (!privacy)
		return 0;

	NLA_PUT_FLAG(msg, NL80211_ATTR_PRIVACY);

	nl_keys = nla_nest_start(msg, NL80211_ATTR_KEYS);
	if (!nl_keys)
		goto nla_put_failure;

	for (i = 0; i < 4; i++) {
		if (!params->wep_key[i])
			continue;

		nl_key = nla_nest_start(msg, i);
		if (!nl_key)
			goto nla_put_failure;

		NLA_PUT(msg, NL80211_KEY_DATA, params->wep_key_len[i],
			params->wep_key[i]);
		if (params->wep_key_len[i] == 5)
			NLA_PUT_U32(msg, NL80211_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_WEP40);
		else
			NLA_PUT_U32(msg, NL80211_KEY_CIPHER,
				    WLAN_CIPHER_SUITE_WEP104);

		NLA_PUT_U8(msg, NL80211_KEY_IDX, i);

		if (i == params->wep_tx_keyidx)
			NLA_PUT_FLAG(msg, NL80211_KEY_DEFAULT);

		nla_nest_end(msg, nl_key);
	}
	nla_nest_end(msg, nl_keys);

	return 0;

nla_put_failure:
	return -ENOBUFS;
}


static int wpa_driver_nl80211_mlme(struct wpa_driver_nl80211_data *drv,
				   const u8 *addr, int cmd, u16 reason_code,
				   int local_state_change)
{
	int ret = -1;
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, cmd);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U16(msg, NL80211_ATTR_REASON_CODE, reason_code);
	if (addr)
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	if (local_state_change)
		NLA_PUT_FLAG(msg, NL80211_ATTR_LOCAL_STATE_CHANGE);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: MLME command failed: reason=%u ret=%d (%s)",
			reason_code, ret, strerror(-ret));
		goto nla_put_failure;
	}
	ret = 0;

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_disconnect(struct wpa_driver_nl80211_data *drv,
					 int reason_code)
{
	wpa_printf(MSG_DEBUG, "%s(reason_code=%d)", __func__, reason_code);
	drv->associated = 0;
	drv->ignore_next_local_disconnect = 0;
	/* Disconnect command doesn't need BSSID - it uses cached value */
	return wpa_driver_nl80211_mlme(drv, NULL, NL80211_CMD_DISCONNECT,
				       reason_code, 0);
}


static int wpa_driver_nl80211_deauthenticate(void *priv, const u8 *addr,
					     int reason_code)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_SME))
		return wpa_driver_nl80211_disconnect(drv, reason_code);
	wpa_printf(MSG_DEBUG, "%s(addr=" MACSTR " reason_code=%d)",
		   __func__, MAC2STR(addr), reason_code);
	drv->associated = 0;
	if (drv->nlmode == NL80211_IFTYPE_ADHOC)
		return nl80211_leave_ibss(drv);
	return wpa_driver_nl80211_mlme(drv, addr, NL80211_CMD_DEAUTHENTICATE,
				       reason_code, 0);
}


static void nl80211_copy_auth_params(struct wpa_driver_nl80211_data *drv,
				     struct wpa_driver_auth_params *params)
{
	int i;

	drv->auth_freq = params->freq;
	drv->auth_alg = params->auth_alg;
	drv->auth_wep_tx_keyidx = params->wep_tx_keyidx;
	drv->auth_local_state_change = params->local_state_change;
	drv->auth_p2p = params->p2p;

	if (params->bssid)
		os_memcpy(drv->auth_bssid_, params->bssid, ETH_ALEN);
	else
		os_memset(drv->auth_bssid_, 0, ETH_ALEN);

	if (params->ssid) {
		os_memcpy(drv->auth_ssid, params->ssid, params->ssid_len);
		drv->auth_ssid_len = params->ssid_len;
	} else
		drv->auth_ssid_len = 0;


	os_free(drv->auth_ie);
	drv->auth_ie = NULL;
	drv->auth_ie_len = 0;
	if (params->ie) {
		drv->auth_ie = os_malloc(params->ie_len);
		if (drv->auth_ie) {
			os_memcpy(drv->auth_ie, params->ie, params->ie_len);
			drv->auth_ie_len = params->ie_len;
		}
	}

	for (i = 0; i < 4; i++) {
		if (params->wep_key[i] && params->wep_key_len[i] &&
		    params->wep_key_len[i] <= 16) {
			os_memcpy(drv->auth_wep_key[i], params->wep_key[i],
				  params->wep_key_len[i]);
			drv->auth_wep_key_len[i] = params->wep_key_len[i];
		} else
			drv->auth_wep_key_len[i] = 0;
	}
}


static int wpa_driver_nl80211_authenticate(
	void *priv, struct wpa_driver_auth_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1, i;
	struct nl_msg *msg;
	enum nl80211_auth_type type;
	enum nl80211_iftype nlmode;
	int count = 0;
	int is_retry;

	is_retry = drv->retry_auth;
	drv->retry_auth = 0;

	drv->associated = 0;
	os_memset(drv->auth_bssid, 0, ETH_ALEN);
	/* FIX: IBSS mode */
	nlmode = params->p2p ?
		NL80211_IFTYPE_P2P_CLIENT : NL80211_IFTYPE_STATION;
	if (drv->nlmode != nlmode &&
	    wpa_driver_nl80211_set_mode(priv, nlmode) < 0)
		return -1;

retry:
	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: Authenticate (ifindex=%d)",
		   drv->ifindex);

	nl80211_cmd(drv, msg, 0, NL80211_CMD_AUTHENTICATE);

	for (i = 0; i < 4; i++) {
		if (!params->wep_key[i])
			continue;
		wpa_driver_nl80211_set_key(bss->ifname, priv, WPA_ALG_WEP,
					   NULL, i,
					   i == params->wep_tx_keyidx, NULL, 0,
					   params->wep_key[i],
					   params->wep_key_len[i]);
		if (params->wep_tx_keyidx != i)
			continue;
		if (nl_add_key(msg, WPA_ALG_WEP, i, 1, NULL, 0,
			       params->wep_key[i], params->wep_key_len[i])) {
			nlmsg_free(msg);
			return -1;
		}
	}

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	if (params->bssid) {
		wpa_printf(MSG_DEBUG, "  * bssid=" MACSTR,
			   MAC2STR(params->bssid));
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid);
	}
	if (params->freq) {
		wpa_printf(MSG_DEBUG, "  * freq=%d", params->freq);
		NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, params->freq);
	}
	if (params->ssid) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * SSID",
				  params->ssid, params->ssid_len);
		NLA_PUT(msg, NL80211_ATTR_SSID, params->ssid_len,
			params->ssid);
	}
	wpa_hexdump(MSG_DEBUG, "  * IEs", params->ie, params->ie_len);
	if (params->ie)
		NLA_PUT(msg, NL80211_ATTR_IE, params->ie_len, params->ie);
	if (params->sae_data) {
		wpa_hexdump(MSG_DEBUG, "  * SAE data", params->sae_data,
			    params->sae_data_len);
		NLA_PUT(msg, NL80211_ATTR_SAE_DATA, params->sae_data_len,
			params->sae_data);
	}
	if (params->auth_alg & WPA_AUTH_ALG_OPEN)
		type = NL80211_AUTHTYPE_OPEN_SYSTEM;
	else if (params->auth_alg & WPA_AUTH_ALG_SHARED)
		type = NL80211_AUTHTYPE_SHARED_KEY;
	else if (params->auth_alg & WPA_AUTH_ALG_LEAP)
		type = NL80211_AUTHTYPE_NETWORK_EAP;
	else if (params->auth_alg & WPA_AUTH_ALG_FT)
		type = NL80211_AUTHTYPE_FT;
	else if (params->auth_alg & WPA_AUTH_ALG_SAE)
		type = NL80211_AUTHTYPE_SAE;
	else
		goto nla_put_failure;
	wpa_printf(MSG_DEBUG, "  * Auth Type %d", type);
	NLA_PUT_U32(msg, NL80211_ATTR_AUTH_TYPE, type);
	if (params->local_state_change) {
		wpa_printf(MSG_DEBUG, "  * Local state change only");
		NLA_PUT_FLAG(msg, NL80211_ATTR_LOCAL_STATE_CHANGE);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: MLME command failed (auth): ret=%d (%s)",
			ret, strerror(-ret));
		count++;
		if (ret == -EALREADY && count == 1 && params->bssid &&
		    !params->local_state_change) {
			/*
			 * mac80211 does not currently accept new
			 * authentication if we are already authenticated. As a
			 * workaround, force deauthentication and try again.
			 */
			wpa_printf(MSG_DEBUG, "nl80211: Retry authentication "
				   "after forced deauthentication");
			wpa_driver_nl80211_deauthenticate(
				bss, params->bssid,
				WLAN_REASON_PREV_AUTH_NOT_VALID);
			nlmsg_free(msg);
			goto retry;
		}

		if (ret == -ENOENT && params->freq && !is_retry) {
			/*
			 * cfg80211 has likely expired the BSS entry even
			 * though it was previously available in our internal
			 * BSS table. To recover quickly, start a single
			 * channel scan on the specified channel.
			 */
			struct wpa_driver_scan_params scan;
			int freqs[2];

			os_memset(&scan, 0, sizeof(scan));
			scan.num_ssids = 1;
			if (params->ssid) {
				scan.ssids[0].ssid = params->ssid;
				scan.ssids[0].ssid_len = params->ssid_len;
			}
			freqs[0] = params->freq;
			freqs[1] = 0;
			scan.freqs = freqs;
			wpa_printf(MSG_DEBUG, "nl80211: Trigger single "
				   "channel scan to refresh cfg80211 BSS "
				   "entry");
			ret = wpa_driver_nl80211_scan(bss, &scan);
			if (ret == 0) {
				nl80211_copy_auth_params(drv, params);
				drv->scan_for_auth = 1;
			}
		} else if (is_retry) {
			/*
			 * Need to indicate this with an event since the return
			 * value from the retry is not delivered to core code.
			 */
			union wpa_event_data event;
			wpa_printf(MSG_DEBUG, "nl80211: Authentication retry "
				   "failed");
			os_memset(&event, 0, sizeof(event));
			os_memcpy(event.timeout_event.addr, drv->auth_bssid_,
				  ETH_ALEN);
			wpa_supplicant_event(drv->ctx, EVENT_AUTH_TIMED_OUT,
					     &event);
		}

		goto nla_put_failure;
	}
	ret = 0;
	wpa_printf(MSG_DEBUG, "nl80211: Authentication request send "
		   "successfully");

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_authenticate_retry(
	struct wpa_driver_nl80211_data *drv)
{
	struct wpa_driver_auth_params params;
	struct i802_bss *bss = &drv->first_bss;
	int i;

	wpa_printf(MSG_DEBUG, "nl80211: Try to authenticate again");

	os_memset(&params, 0, sizeof(params));
	params.freq = drv->auth_freq;
	params.auth_alg = drv->auth_alg;
	params.wep_tx_keyidx = drv->auth_wep_tx_keyidx;
	params.local_state_change = drv->auth_local_state_change;
	params.p2p = drv->auth_p2p;

	if (!is_zero_ether_addr(drv->auth_bssid_))
		params.bssid = drv->auth_bssid_;

	if (drv->auth_ssid_len) {
		params.ssid = drv->auth_ssid;
		params.ssid_len = drv->auth_ssid_len;
	}

	params.ie = drv->auth_ie;
	params.ie_len = drv->auth_ie_len;

	for (i = 0; i < 4; i++) {
		if (drv->auth_wep_key_len[i]) {
			params.wep_key[i] = drv->auth_wep_key[i];
			params.wep_key_len[i] = drv->auth_wep_key_len[i];
		}
	}

	drv->retry_auth = 1;
	return wpa_driver_nl80211_authenticate(bss, &params);
}


struct phy_info_arg {
	u16 *num_modes;
	struct hostapd_hw_modes *modes;
};

static int phy_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct phy_info_arg *phy_info = arg;

	struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];

	struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
	static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
		[NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_MAX_TX_POWER] = { .type = NLA_U32 },
	};

	struct nlattr *tb_rate[NL80211_BITRATE_ATTR_MAX + 1];
	static struct nla_policy rate_policy[NL80211_BITRATE_ATTR_MAX + 1] = {
		[NL80211_BITRATE_ATTR_RATE] = { .type = NLA_U32 },
		[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE] = { .type = NLA_FLAG },
	};

	struct nlattr *nl_band;
	struct nlattr *nl_freq;
	struct nlattr *nl_rate;
	int rem_band, rem_freq, rem_rate;
	struct hostapd_hw_modes *mode;
	int idx, mode_is_set;

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb_msg[NL80211_ATTR_WIPHY_BANDS])
		return NL_SKIP;

	nla_for_each_nested(nl_band, tb_msg[NL80211_ATTR_WIPHY_BANDS], rem_band) {
		mode = os_realloc_array(phy_info->modes,
					*phy_info->num_modes + 1,
					sizeof(*mode));
		if (!mode)
			return NL_SKIP;
		phy_info->modes = mode;

		mode_is_set = 0;

		mode = &phy_info->modes[*(phy_info->num_modes)];
		memset(mode, 0, sizeof(*mode));
		mode->flags = HOSTAPD_MODE_FLAG_HT_INFO_KNOWN;
		*(phy_info->num_modes) += 1;

		nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band),
			  nla_len(nl_band), NULL);

		if (tb_band[NL80211_BAND_ATTR_HT_CAPA]) {
			mode->ht_capab = nla_get_u16(
				tb_band[NL80211_BAND_ATTR_HT_CAPA]);
		}

		if (tb_band[NL80211_BAND_ATTR_HT_AMPDU_FACTOR]) {
			mode->a_mpdu_params |= nla_get_u8(
				tb_band[NL80211_BAND_ATTR_HT_AMPDU_FACTOR]) &
				0x03;
		}

		if (tb_band[NL80211_BAND_ATTR_HT_AMPDU_DENSITY]) {
			mode->a_mpdu_params |= nla_get_u8(
				tb_band[NL80211_BAND_ATTR_HT_AMPDU_DENSITY]) <<
				2;
		}

		if (tb_band[NL80211_BAND_ATTR_HT_MCS_SET] &&
		    nla_len(tb_band[NL80211_BAND_ATTR_HT_MCS_SET])) {
			u8 *mcs;
			mcs = nla_data(tb_band[NL80211_BAND_ATTR_HT_MCS_SET]);
			os_memcpy(mode->mcs_set, mcs, 16);
		}

		if (tb_band[NL80211_BAND_ATTR_VHT_CAPA]) {
			mode->vht_capab = nla_get_u32(
				tb_band[NL80211_BAND_ATTR_VHT_CAPA]);
		}

		if (tb_band[NL80211_BAND_ATTR_VHT_MCS_SET] &&
		    nla_len(tb_band[NL80211_BAND_ATTR_VHT_MCS_SET])) {
			u8 *mcs;
			mcs = nla_data(tb_band[NL80211_BAND_ATTR_VHT_MCS_SET]);
			os_memcpy(mode->vht_mcs_set, mcs, 8);
		}

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq) {
			nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq),
				  nla_len(nl_freq), freq_policy);
			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;
			mode->num_channels++;
		}

		mode->channels = os_calloc(mode->num_channels,
					   sizeof(struct hostapd_channel_data));
		if (!mode->channels)
			return NL_SKIP;

		idx = 0;

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS], rem_freq) {
			nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nl_freq),
				  nla_len(nl_freq), freq_policy);
			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;

			mode->channels[idx].freq = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
			mode->channels[idx].flag = 0;

			if (!mode_is_set) {
				/* crude heuristic */
				if (mode->channels[idx].freq < 4000)
					mode->mode = HOSTAPD_MODE_IEEE80211B;
				else if (mode->channels[idx].freq > 50000)
					mode->mode = HOSTAPD_MODE_IEEE80211AD;
				else
					mode->mode = HOSTAPD_MODE_IEEE80211A;
				mode_is_set = 1;
			}

			switch (mode->mode) {
			case HOSTAPD_MODE_IEEE80211AD:
				mode->channels[idx].chan =
					(mode->channels[idx].freq - 56160) /
					2160;
				break;
			case HOSTAPD_MODE_IEEE80211A:
				mode->channels[idx].chan =
					mode->channels[idx].freq / 5 - 1000;
				break;
			case HOSTAPD_MODE_IEEE80211B:
			case HOSTAPD_MODE_IEEE80211G:
				if (mode->channels[idx].freq == 2484)
					mode->channels[idx].chan = 14;
				else
					mode->channels[idx].chan =
						(mode->channels[idx].freq -
						 2407) / 5;
				break;
			default:
				break;
			}

			if (tb_freq[NL80211_FREQUENCY_ATTR_DISABLED])
				mode->channels[idx].flag |=
					HOSTAPD_CHAN_DISABLED;
			if (tb_freq[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN])
				mode->channels[idx].flag |=
					HOSTAPD_CHAN_PASSIVE_SCAN;
			if (tb_freq[NL80211_FREQUENCY_ATTR_NO_IBSS])
				mode->channels[idx].flag |=
					HOSTAPD_CHAN_NO_IBSS;
			if (tb_freq[NL80211_FREQUENCY_ATTR_RADAR])
				mode->channels[idx].flag |=
					HOSTAPD_CHAN_RADAR;

			if (tb_freq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER] &&
			    !tb_freq[NL80211_FREQUENCY_ATTR_DISABLED])
				mode->channels[idx].max_tx_power =
					nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER]) / 100;

			idx++;
		}

		nla_for_each_nested(nl_rate, tb_band[NL80211_BAND_ATTR_RATES], rem_rate) {
			nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX, nla_data(nl_rate),
				  nla_len(nl_rate), rate_policy);
			if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
				continue;
			mode->num_rates++;
		}

		mode->rates = os_calloc(mode->num_rates, sizeof(int));
		if (!mode->rates)
			return NL_SKIP;

		idx = 0;

		nla_for_each_nested(nl_rate, tb_band[NL80211_BAND_ATTR_RATES], rem_rate) {
			nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX, nla_data(nl_rate),
				  nla_len(nl_rate), rate_policy);
			if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
				continue;
			mode->rates[idx] = nla_get_u32(tb_rate[NL80211_BITRATE_ATTR_RATE]);

			/* crude heuristic */
			if (mode->mode == HOSTAPD_MODE_IEEE80211B &&
			    mode->rates[idx] > 200)
				mode->mode = HOSTAPD_MODE_IEEE80211G;

			idx++;
		}
	}

	return NL_SKIP;
}

static struct hostapd_hw_modes *
wpa_driver_nl80211_add_11b(struct hostapd_hw_modes *modes, u16 *num_modes)
{
	u16 m;
	struct hostapd_hw_modes *mode11g = NULL, *nmodes, *mode;
	int i, mode11g_idx = -1;

	/* If only 802.11g mode is included, use it to construct matching
	 * 802.11b mode data. */

	for (m = 0; m < *num_modes; m++) {
		if (modes[m].mode == HOSTAPD_MODE_IEEE80211B)
			return modes; /* 802.11b already included */
		if (modes[m].mode == HOSTAPD_MODE_IEEE80211G)
			mode11g_idx = m;
	}

	if (mode11g_idx < 0)
		return modes; /* 2.4 GHz band not supported at all */

	nmodes = os_realloc_array(modes, *num_modes + 1, sizeof(*nmodes));
	if (nmodes == NULL)
		return modes; /* Could not add 802.11b mode */

	mode = &nmodes[*num_modes];
	os_memset(mode, 0, sizeof(*mode));
	(*num_modes)++;
	modes = nmodes;

	mode->mode = HOSTAPD_MODE_IEEE80211B;

	mode11g = &modes[mode11g_idx];
	mode->num_channels = mode11g->num_channels;
	mode->channels = os_malloc(mode11g->num_channels *
				   sizeof(struct hostapd_channel_data));
	if (mode->channels == NULL) {
		(*num_modes)--;
		return modes; /* Could not add 802.11b mode */
	}
	os_memcpy(mode->channels, mode11g->channels,
		  mode11g->num_channels * sizeof(struct hostapd_channel_data));

	mode->num_rates = 0;
	mode->rates = os_malloc(4 * sizeof(int));
	if (mode->rates == NULL) {
		os_free(mode->channels);
		(*num_modes)--;
		return modes; /* Could not add 802.11b mode */
	}

	for (i = 0; i < mode11g->num_rates; i++) {
		if (mode11g->rates[i] != 10 && mode11g->rates[i] != 20 &&
		    mode11g->rates[i] != 55 && mode11g->rates[i] != 110)
			continue;
		mode->rates[mode->num_rates] = mode11g->rates[i];
		mode->num_rates++;
		if (mode->num_rates == 4)
			break;
	}

	if (mode->num_rates == 0) {
		os_free(mode->channels);
		os_free(mode->rates);
		(*num_modes)--;
		return modes; /* No 802.11b rates */
	}

	wpa_printf(MSG_DEBUG, "nl80211: Added 802.11b mode based on 802.11g "
		   "information");

	return modes;
}


static void nl80211_set_ht40_mode(struct hostapd_hw_modes *mode, int start,
				  int end)
{
	int c;

	for (c = 0; c < mode->num_channels; c++) {
		struct hostapd_channel_data *chan = &mode->channels[c];
		if (chan->freq - 10 >= start && chan->freq + 10 <= end)
			chan->flag |= HOSTAPD_CHAN_HT40;
	}
}


static void nl80211_set_ht40_mode_sec(struct hostapd_hw_modes *mode, int start,
				      int end)
{
	int c;

	for (c = 0; c < mode->num_channels; c++) {
		struct hostapd_channel_data *chan = &mode->channels[c];
		if (!(chan->flag & HOSTAPD_CHAN_HT40))
			continue;
		if (chan->freq - 30 >= start && chan->freq - 10 <= end)
			chan->flag |= HOSTAPD_CHAN_HT40MINUS;
		if (chan->freq + 10 >= start && chan->freq + 30 <= end)
			chan->flag |= HOSTAPD_CHAN_HT40PLUS;
	}
}


static void nl80211_reg_rule_ht40(struct nlattr *tb[],
				  struct phy_info_arg *results)
{
	u32 start, end, max_bw;
	u16 m;

	if (tb[NL80211_ATTR_FREQ_RANGE_START] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_END] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_MAX_BW] == NULL)
		return;

	start = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_START]) / 1000;
	end = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_END]) / 1000;
	max_bw = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_MAX_BW]) / 1000;

	wpa_printf(MSG_DEBUG, "nl80211: %u-%u @ %u MHz",
		   start, end, max_bw);
	if (max_bw < 40)
		return;

	for (m = 0; m < *results->num_modes; m++) {
		if (!(results->modes[m].ht_capab &
		      HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
			continue;
		nl80211_set_ht40_mode(&results->modes[m], start, end);
	}
}


static void nl80211_reg_rule_sec(struct nlattr *tb[],
				 struct phy_info_arg *results)
{
	u32 start, end, max_bw;
	u16 m;

	if (tb[NL80211_ATTR_FREQ_RANGE_START] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_END] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_MAX_BW] == NULL)
		return;

	start = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_START]) / 1000;
	end = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_END]) / 1000;
	max_bw = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_MAX_BW]) / 1000;

	if (max_bw < 20)
		return;

	for (m = 0; m < *results->num_modes; m++) {
		if (!(results->modes[m].ht_capab &
		      HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
			continue;
		nl80211_set_ht40_mode_sec(&results->modes[m], start, end);
	}
}


static int nl80211_get_reg(struct nl_msg *msg, void *arg)
{
	struct phy_info_arg *results = arg;
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *nl_rule;
	struct nlattr *tb_rule[NL80211_FREQUENCY_ATTR_MAX + 1];
	int rem_rule;
	static struct nla_policy reg_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
		[NL80211_ATTR_REG_RULE_FLAGS] = { .type = NLA_U32 },
		[NL80211_ATTR_FREQ_RANGE_START] = { .type = NLA_U32 },
		[NL80211_ATTR_FREQ_RANGE_END] = { .type = NLA_U32 },
		[NL80211_ATTR_FREQ_RANGE_MAX_BW] = { .type = NLA_U32 },
		[NL80211_ATTR_POWER_RULE_MAX_ANT_GAIN] = { .type = NLA_U32 },
		[NL80211_ATTR_POWER_RULE_MAX_EIRP] = { .type = NLA_U32 },
	};

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb_msg[NL80211_ATTR_REG_ALPHA2] ||
	    !tb_msg[NL80211_ATTR_REG_RULES]) {
		wpa_printf(MSG_DEBUG, "nl80211: No regulatory information "
			   "available");
		return NL_SKIP;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Regulatory information - country=%s",
		   (char *) nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]));

	nla_for_each_nested(nl_rule, tb_msg[NL80211_ATTR_REG_RULES], rem_rule)
	{
		nla_parse(tb_rule, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_rule), nla_len(nl_rule), reg_policy);
		nl80211_reg_rule_ht40(tb_rule, results);
	}

	nla_for_each_nested(nl_rule, tb_msg[NL80211_ATTR_REG_RULES], rem_rule)
	{
		nla_parse(tb_rule, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_rule), nla_len(nl_rule), reg_policy);
		nl80211_reg_rule_sec(tb_rule, results);
	}

	return NL_SKIP;
}


static int nl80211_set_ht40_flags(struct wpa_driver_nl80211_data *drv,
				  struct phy_info_arg *results)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_REG);
	return send_and_recv_msgs(drv, msg, nl80211_get_reg, results);
}


static struct hostapd_hw_modes *
wpa_driver_nl80211_get_hw_feature_data(void *priv, u16 *num_modes, u16 *flags)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct phy_info_arg result = {
		.num_modes = num_modes,
		.modes = NULL,
	};

	*num_modes = 0;
	*flags = 0;

	msg = nlmsg_alloc();
	if (!msg)
		return NULL;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_WIPHY);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	if (send_and_recv_msgs(drv, msg, phy_info_handler, &result) == 0) {
		nl80211_set_ht40_flags(drv, &result);
		return wpa_driver_nl80211_add_11b(result.modes, num_modes);
	}
	msg = NULL;
 nla_put_failure:
	nlmsg_free(msg);
	return NULL;
}


static int wpa_driver_nl80211_send_mntr(struct wpa_driver_nl80211_data *drv,
					const void *data, size_t len,
					int encrypt, int noack)
{
	__u8 rtap_hdr[] = {
		0x00, 0x00, /* radiotap version */
		0x0e, 0x00, /* radiotap length */
		0x02, 0xc0, 0x00, 0x00, /* bmap: flags, tx and rx flags */
		IEEE80211_RADIOTAP_F_FRAG, /* F_FRAG (fragment if required) */
		0x00,       /* padding */
		0x00, 0x00, /* RX and TX flags to indicate that */
		0x00, 0x00, /* this is the injected frame directly */
	};
	struct iovec iov[2] = {
		{
			.iov_base = &rtap_hdr,
			.iov_len = sizeof(rtap_hdr),
		},
		{
			.iov_base = (void *) data,
			.iov_len = len,
		}
	};
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = iov,
		.msg_iovlen = 2,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};
	int res;
	u16 txflags = 0;

	if (encrypt)
		rtap_hdr[8] |= IEEE80211_RADIOTAP_F_WEP;

	if (drv->monitor_sock < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: No monitor socket available "
			   "for %s", __func__);
		return -1;
	}

	if (noack)
		txflags |= IEEE80211_RADIOTAP_F_TX_NOACK;
	WPA_PUT_LE16(&rtap_hdr[12], txflags);

	res = sendmsg(drv->monitor_sock, &msg, 0);
	if (res < 0) {
		wpa_printf(MSG_INFO, "nl80211: sendmsg: %s", strerror(errno));
		return -1;
	}
	return 0;
}


static int wpa_driver_nl80211_send_frame(struct i802_bss *bss,
					 const void *data, size_t len,
					 int encrypt, int noack,
					 unsigned int freq, int no_cck,
					 int offchanok, unsigned int wait_time)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	u64 cookie;

	if (freq == 0)
		freq = bss->freq;

	if (drv->use_monitor)
		return wpa_driver_nl80211_send_mntr(drv, data, len,
						    encrypt, noack);

	return nl80211_send_frame_cmd(bss, freq, wait_time, data, len,
				      &cookie, no_cck, noack, offchanok);
}


static int wpa_driver_nl80211_send_mlme_freq(struct i802_bss *bss,
					     const u8 *data,
					     size_t data_len, int noack,
					     unsigned int freq, int no_cck,
					     int offchanok,
					     unsigned int wait_time)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_mgmt *mgmt;
	int encrypt = 1;
	u16 fc;

	mgmt = (struct ieee80211_mgmt *) data;
	fc = le_to_host16(mgmt->frame_control);

	if (is_sta_interface(drv->nlmode) &&
	    WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
	    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_PROBE_RESP) {
		/*
		 * The use of last_mgmt_freq is a bit of a hack,
		 * but it works due to the single-threaded nature
		 * of wpa_supplicant.
		 */
		if (freq == 0)
			freq = drv->last_mgmt_freq;
		return nl80211_send_frame_cmd(bss, freq, 0,
					      data, data_len, NULL, 1, noack,
					      1);
	}

	if (drv->device_ap_sme && is_ap_interface(drv->nlmode)) {
		if (freq == 0)
			freq = bss->freq;
		return nl80211_send_frame_cmd(bss, freq,
					      (int) freq == bss->freq ? 0 :
					      wait_time,
					      data, data_len,
					      &drv->send_action_cookie,
					      no_cck, noack, offchanok);
	}

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
	    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_AUTH) {
		/*
		 * Only one of the authentication frame types is encrypted.
		 * In order for static WEP encryption to work properly (i.e.,
		 * to not encrypt the frame), we need to tell mac80211 about
		 * the frames that must not be encrypted.
		 */
		u16 auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
		u16 auth_trans = le_to_host16(mgmt->u.auth.auth_transaction);
		if (auth_alg != WLAN_AUTH_SHARED_KEY || auth_trans != 3)
			encrypt = 0;
	}

	return wpa_driver_nl80211_send_frame(bss, data, data_len, encrypt,
					     noack, freq, no_cck, offchanok,
					     wait_time);
}


static int wpa_driver_nl80211_send_mlme(void *priv, const u8 *data,
					size_t data_len, int noack)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_send_mlme_freq(bss, data, data_len, noack,
						 0, 0, 0, 0);
}


static int nl80211_set_bss(struct i802_bss *bss, int cts, int preamble,
			   int slot, int ht_opmode, int ap_isolate,
			   int *basic_rates)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_BSS);

	if (cts >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_BSS_CTS_PROT, cts);
	if (preamble >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_BSS_SHORT_PREAMBLE, preamble);
	if (slot >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_BSS_SHORT_SLOT_TIME, slot);
	if (ht_opmode >= 0)
		NLA_PUT_U16(msg, NL80211_ATTR_BSS_HT_OPMODE, ht_opmode);
	if (ap_isolate >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_AP_ISOLATE, ap_isolate);

	if (basic_rates) {
		u8 rates[NL80211_MAX_SUPP_RATES];
		u8 rates_len = 0;
		int i;

		for (i = 0; i < NL80211_MAX_SUPP_RATES && basic_rates[i] >= 0;
		     i++)
			rates[rates_len++] = basic_rates[i] / 5;

		NLA_PUT(msg, NL80211_ATTR_BSS_BASIC_RATES, rates_len, rates);
	}

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(bss->ifname));

	return send_and_recv_msgs(drv, msg, NULL, NULL);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int wpa_driver_nl80211_set_ap(void *priv,
				     struct wpa_driver_ap_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	u8 cmd = NL80211_CMD_NEW_BEACON;
	int ret;
	int beacon_set;
	int ifindex = if_nametoindex(bss->ifname);
	int num_suites;
	u32 suites[10];
	u32 ver;

	beacon_set = bss->beacon_set;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	wpa_printf(MSG_DEBUG, "nl80211: Set beacon (beacon_set=%d)",
		   beacon_set);
	if (beacon_set)
		cmd = NL80211_CMD_SET_BEACON;

	nl80211_cmd(drv, msg, 0, cmd);
	NLA_PUT(msg, NL80211_ATTR_BEACON_HEAD, params->head_len, params->head);
	NLA_PUT(msg, NL80211_ATTR_BEACON_TAIL, params->tail_len, params->tail);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_BEACON_INTERVAL, params->beacon_int);
	NLA_PUT_U32(msg, NL80211_ATTR_DTIM_PERIOD, params->dtim_period);
	NLA_PUT(msg, NL80211_ATTR_SSID, params->ssid_len,
		params->ssid);
	if (params->proberesp && params->proberesp_len)
		NLA_PUT(msg, NL80211_ATTR_PROBE_RESP, params->proberesp_len,
			params->proberesp);
	switch (params->hide_ssid) {
	case NO_SSID_HIDING:
		NLA_PUT_U32(msg, NL80211_ATTR_HIDDEN_SSID,
			    NL80211_HIDDEN_SSID_NOT_IN_USE);
		break;
	case HIDDEN_SSID_ZERO_LEN:
		NLA_PUT_U32(msg, NL80211_ATTR_HIDDEN_SSID,
			    NL80211_HIDDEN_SSID_ZERO_LEN);
		break;
	case HIDDEN_SSID_ZERO_CONTENTS:
		NLA_PUT_U32(msg, NL80211_ATTR_HIDDEN_SSID,
			    NL80211_HIDDEN_SSID_ZERO_CONTENTS);
		break;
	}
	if (params->privacy)
		NLA_PUT_FLAG(msg, NL80211_ATTR_PRIVACY);
	if ((params->auth_algs & (WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED)) ==
	    (WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED)) {
		/* Leave out the attribute */
	} else if (params->auth_algs & WPA_AUTH_ALG_SHARED)
		NLA_PUT_U32(msg, NL80211_ATTR_AUTH_TYPE,
			    NL80211_AUTHTYPE_SHARED_KEY);
	else
		NLA_PUT_U32(msg, NL80211_ATTR_AUTH_TYPE,
			    NL80211_AUTHTYPE_OPEN_SYSTEM);

	ver = 0;
	if (params->wpa_version & WPA_PROTO_WPA)
		ver |= NL80211_WPA_VERSION_1;
	if (params->wpa_version & WPA_PROTO_RSN)
		ver |= NL80211_WPA_VERSION_2;
	if (ver)
		NLA_PUT_U32(msg, NL80211_ATTR_WPA_VERSIONS, ver);

	num_suites = 0;
	if (params->key_mgmt_suites & WPA_KEY_MGMT_IEEE8021X)
		suites[num_suites++] = WLAN_AKM_SUITE_8021X;
	if (params->key_mgmt_suites & WPA_KEY_MGMT_PSK)
		suites[num_suites++] = WLAN_AKM_SUITE_PSK;
	if (num_suites) {
		NLA_PUT(msg, NL80211_ATTR_AKM_SUITES,
			num_suites * sizeof(u32), suites);
	}

	if (params->key_mgmt_suites & WPA_KEY_MGMT_IEEE8021X &&
	    params->pairwise_ciphers & (WPA_CIPHER_WEP104 | WPA_CIPHER_WEP40))
		NLA_PUT_FLAG(msg, NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT);

	num_suites = 0;
	if (params->pairwise_ciphers & WPA_CIPHER_CCMP)
		suites[num_suites++] = WLAN_CIPHER_SUITE_CCMP;
	if (params->pairwise_ciphers & WPA_CIPHER_GCMP)
		suites[num_suites++] = WLAN_CIPHER_SUITE_GCMP;
	if (params->pairwise_ciphers & WPA_CIPHER_TKIP)
		suites[num_suites++] = WLAN_CIPHER_SUITE_TKIP;
	if (params->pairwise_ciphers & WPA_CIPHER_WEP104)
		suites[num_suites++] = WLAN_CIPHER_SUITE_WEP104;
	if (params->pairwise_ciphers & WPA_CIPHER_WEP40)
		suites[num_suites++] = WLAN_CIPHER_SUITE_WEP40;
	if (num_suites) {
		NLA_PUT(msg, NL80211_ATTR_CIPHER_SUITES_PAIRWISE,
			num_suites * sizeof(u32), suites);
	}

	switch (params->group_cipher) {
	case WPA_CIPHER_CCMP:
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP,
			    WLAN_CIPHER_SUITE_CCMP);
		break;
	case WPA_CIPHER_GCMP:
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP,
			    WLAN_CIPHER_SUITE_GCMP);
		break;
	case WPA_CIPHER_TKIP:
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP,
			    WLAN_CIPHER_SUITE_TKIP);
		break;
	case WPA_CIPHER_WEP104:
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP,
			    WLAN_CIPHER_SUITE_WEP104);
		break;
	case WPA_CIPHER_WEP40:
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP,
			    WLAN_CIPHER_SUITE_WEP40);
		break;
	}

	if (params->beacon_ies) {
		NLA_PUT(msg, NL80211_ATTR_IE, wpabuf_len(params->beacon_ies),
			wpabuf_head(params->beacon_ies));
	}
	if (params->proberesp_ies) {
		NLA_PUT(msg, NL80211_ATTR_IE_PROBE_RESP,
			wpabuf_len(params->proberesp_ies),
			wpabuf_head(params->proberesp_ies));
	}
	if (params->assocresp_ies) {
		NLA_PUT(msg, NL80211_ATTR_IE_ASSOC_RESP,
			wpabuf_len(params->assocresp_ies),
			wpabuf_head(params->assocresp_ies));
	}

	if (drv->capa.flags & WPA_DRIVER_FLAGS_INACTIVITY_TIMER)  {
		NLA_PUT_U16(msg, NL80211_ATTR_INACTIVITY_TIMEOUT,
			    params->ap_max_inactivity);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Beacon set failed: %d (%s)",
			   ret, strerror(-ret));
	} else {
		bss->beacon_set = 1;
		nl80211_set_bss(bss, params->cts_protect, params->preamble,
				params->short_slot_time, params->ht_opmode,
				params->isolate, params->basic_rates);
	}
	return ret;
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int wpa_driver_nl80211_set_freq(struct i802_bss *bss,
				       int freq, int ht_enabled,
				       int sec_channel_offset)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: Set freq %d (ht_enabled=%d "
		   "sec_channel_offset=%d)",
		   freq, ht_enabled, sec_channel_offset);
	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_WIPHY);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	if (ht_enabled) {
		switch (sec_channel_offset) {
		case -1:
			NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
				    NL80211_CHAN_HT40MINUS);
			break;
		case 1:
			NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
				    NL80211_CHAN_HT40PLUS);
			break;
		default:
			NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
				    NL80211_CHAN_HT20);
			break;
		}
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret == 0) {
		bss->freq = freq;
		return 0;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set channel (freq=%d): "
		   "%d (%s)", freq, ret, strerror(-ret));
nla_put_failure:
	nlmsg_free(msg);
	return -1;
}


static u32 sta_flags_nl80211(int flags)
{
	u32 f = 0;

	if (flags & WPA_STA_AUTHORIZED)
		f |= BIT(NL80211_STA_FLAG_AUTHORIZED);
	if (flags & WPA_STA_WMM)
		f |= BIT(NL80211_STA_FLAG_WME);
	if (flags & WPA_STA_SHORT_PREAMBLE)
		f |= BIT(NL80211_STA_FLAG_SHORT_PREAMBLE);
	if (flags & WPA_STA_MFP)
		f |= BIT(NL80211_STA_FLAG_MFP);
	if (flags & WPA_STA_TDLS_PEER)
		f |= BIT(NL80211_STA_FLAG_TDLS_PEER);

	return f;
}


static int wpa_driver_nl80211_sta_add(void *priv,
				      struct hostapd_sta_add_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg, *wme = NULL;
	struct nl80211_sta_flag_update upd;
	int ret = -ENOBUFS;

	if ((params->flags & WPA_STA_TDLS_PEER) &&
	    !(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT))
		return -EOPNOTSUPP;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, params->set ? NL80211_CMD_SET_STATION :
		    NL80211_CMD_NEW_STATION);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(bss->ifname));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, params->addr);
	NLA_PUT(msg, NL80211_ATTR_STA_SUPPORTED_RATES, params->supp_rates_len,
		params->supp_rates);
	if (!params->set) {
		NLA_PUT_U16(msg, NL80211_ATTR_STA_AID, params->aid);
		NLA_PUT_U16(msg, NL80211_ATTR_STA_LISTEN_INTERVAL,
			    params->listen_interval);
	}
	if (params->ht_capabilities) {
		NLA_PUT(msg, NL80211_ATTR_HT_CAPABILITY,
			sizeof(*params->ht_capabilities),
			params->ht_capabilities);
	}

	os_memset(&upd, 0, sizeof(upd));
	upd.mask = sta_flags_nl80211(params->flags);
	upd.set = upd.mask;
	NLA_PUT(msg, NL80211_ATTR_STA_FLAGS2, sizeof(upd), &upd);

	if (params->flags & WPA_STA_WMM) {
		wme = nlmsg_alloc();
		if (!wme)
			goto nla_put_failure;

		NLA_PUT_U8(wme, NL80211_STA_WME_UAPSD_QUEUES,
				params->qosinfo & WMM_QOSINFO_STA_AC_MASK);
		NLA_PUT_U8(wme, NL80211_STA_WME_MAX_SP,
				(params->qosinfo > WMM_QOSINFO_STA_SP_SHIFT) &
				WMM_QOSINFO_STA_SP_MASK);
		if (nla_put_nested(msg, NL80211_ATTR_STA_WME, wme) < 0)
			goto nla_put_failure;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: NL80211_CMD_%s_STATION "
			   "result: %d (%s)", params->set ? "SET" : "NEW", ret,
			   strerror(-ret));
	if (ret == -EEXIST)
		ret = 0;
 nla_put_failure:
	nlmsg_free(wme);
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_sta_remove(void *priv, const u8 *addr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_DEL_STATION);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(bss->ifname));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret == -ENOENT)
		return 0;
	return ret;
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static void nl80211_remove_iface(struct wpa_driver_nl80211_data *drv,
				 int ifidx)
{
	struct nl_msg *msg;

	wpa_printf(MSG_DEBUG, "nl80211: Remove interface ifindex=%d", ifidx);

	/* stop listening for EAPOL on this interface */
	del_ifidx(drv, ifidx);

	msg = nlmsg_alloc();
	if (!msg)
		goto nla_put_failure;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_DEL_INTERFACE);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifidx);

	if (send_and_recv_msgs(drv, msg, NULL, NULL) == 0)
		return;
	msg = NULL;
 nla_put_failure:
	nlmsg_free(msg);
	wpa_printf(MSG_ERROR, "Failed to remove interface (ifidx=%d)", ifidx);
}


static const char * nl80211_iftype_str(enum nl80211_iftype mode)
{
	switch (mode) {
	case NL80211_IFTYPE_ADHOC:
		return "ADHOC";
	case NL80211_IFTYPE_STATION:
		return "STATION";
	case NL80211_IFTYPE_AP:
		return "AP";
	case NL80211_IFTYPE_MONITOR:
		return "MONITOR";
	case NL80211_IFTYPE_P2P_CLIENT:
		return "P2P_CLIENT";
	case NL80211_IFTYPE_P2P_GO:
		return "P2P_GO";
	default:
		return "unknown";
	}
}


static int nl80211_create_iface_once(struct wpa_driver_nl80211_data *drv,
				     const char *ifname,
				     enum nl80211_iftype iftype,
				     const u8 *addr, int wds)
{
	struct nl_msg *msg, *flags = NULL;
	int ifidx;
	int ret = -ENOBUFS;

	wpa_printf(MSG_DEBUG, "nl80211: Create interface iftype %d (%s)",
		   iftype, nl80211_iftype_str(iftype));

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_NEW_INTERFACE);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_STRING(msg, NL80211_ATTR_IFNAME, ifname);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, iftype);

	if (iftype == NL80211_IFTYPE_MONITOR) {
		int err;

		flags = nlmsg_alloc();
		if (!flags)
			goto nla_put_failure;

		NLA_PUT_FLAG(flags, NL80211_MNTR_FLAG_COOK_FRAMES);

		err = nla_put_nested(msg, NL80211_ATTR_MNTR_FLAGS, flags);

		nlmsg_free(flags);

		if (err)
			goto nla_put_failure;
	} else if (wds) {
		NLA_PUT_U8(msg, NL80211_ATTR_4ADDR, wds);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
 nla_put_failure:
		nlmsg_free(msg);
		wpa_printf(MSG_ERROR, "Failed to create interface %s: %d (%s)",
			   ifname, ret, strerror(-ret));
		return ret;
	}

	ifidx = if_nametoindex(ifname);
	wpa_printf(MSG_DEBUG, "nl80211: New interface %s created: ifindex=%d",
		   ifname, ifidx);

	if (ifidx <= 0)
		return -1;

	/* start listening for EAPOL on this interface */
	add_ifidx(drv, ifidx);

	if (addr && iftype != NL80211_IFTYPE_MONITOR &&
	    linux_set_ifhwaddr(drv->global->ioctl_sock, ifname, addr)) {
		nl80211_remove_iface(drv, ifidx);
		return -1;
	}

	return ifidx;
}


static int nl80211_create_iface(struct wpa_driver_nl80211_data *drv,
				const char *ifname, enum nl80211_iftype iftype,
				const u8 *addr, int wds)
{
	int ret;

	ret = nl80211_create_iface_once(drv, ifname, iftype, addr, wds);

	/* if error occurred and interface exists already */
	if (ret == -ENFILE && if_nametoindex(ifname)) {
		wpa_printf(MSG_INFO, "Try to remove and re-create %s", ifname);

		/* Try to remove the interface that was already there. */
		nl80211_remove_iface(drv, if_nametoindex(ifname));

		/* Try to create the interface again */
		ret = nl80211_create_iface_once(drv, ifname, iftype, addr,
						wds);
	}

	if (ret >= 0 && is_p2p_interface(iftype))
		nl80211_disable_11b_rates(drv, ret, 1);

	return ret;
}


static void handle_tx_callback(void *ctx, u8 *buf, size_t len, int ok)
{
	struct ieee80211_hdr *hdr;
	u16 fc;
	union wpa_event_data event;

	hdr = (struct ieee80211_hdr *) buf;
	fc = le_to_host16(hdr->frame_control);

	os_memset(&event, 0, sizeof(event));
	event.tx_status.type = WLAN_FC_GET_TYPE(fc);
	event.tx_status.stype = WLAN_FC_GET_STYPE(fc);
	event.tx_status.dst = hdr->addr1;
	event.tx_status.data = buf;
	event.tx_status.data_len = len;
	event.tx_status.ack = ok;
	wpa_supplicant_event(ctx, EVENT_TX_STATUS, &event);
}


static void from_unknown_sta(struct wpa_driver_nl80211_data *drv,
			     u8 *buf, size_t len)
{
	struct ieee80211_hdr *hdr = (void *)buf;
	u16 fc;
	union wpa_event_data event;

	if (len < sizeof(*hdr))
		return;

	fc = le_to_host16(hdr->frame_control);

	os_memset(&event, 0, sizeof(event));
	event.rx_from_unknown.bssid = get_hdr_bssid(hdr, len);
	event.rx_from_unknown.addr = hdr->addr2;
	event.rx_from_unknown.wds = (fc & (WLAN_FC_FROMDS | WLAN_FC_TODS)) ==
		(WLAN_FC_FROMDS | WLAN_FC_TODS);
	wpa_supplicant_event(drv->ctx, EVENT_RX_FROM_UNKNOWN, &event);
}


static void handle_frame(struct wpa_driver_nl80211_data *drv,
			 u8 *buf, size_t len, int datarate, int ssi_signal)
{
	struct ieee80211_hdr *hdr;
	u16 fc;
	union wpa_event_data event;

	hdr = (struct ieee80211_hdr *) buf;
	fc = le_to_host16(hdr->frame_control);

	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		os_memset(&event, 0, sizeof(event));
		event.rx_mgmt.frame = buf;
		event.rx_mgmt.frame_len = len;
		event.rx_mgmt.datarate = datarate;
		event.rx_mgmt.ssi_signal = ssi_signal;
		wpa_supplicant_event(drv->ctx, EVENT_RX_MGMT, &event);
		break;
	case WLAN_FC_TYPE_CTRL:
		/* can only get here with PS-Poll frames */
		wpa_printf(MSG_DEBUG, "CTRL");
		from_unknown_sta(drv, buf, len);
		break;
	case WLAN_FC_TYPE_DATA:
		from_unknown_sta(drv, buf, len);
		break;
	}
}


static void handle_monitor_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wpa_driver_nl80211_data *drv = eloop_ctx;
	int len;
	unsigned char buf[3000];
	struct ieee80211_radiotap_iterator iter;
	int ret;
	int datarate = 0, ssi_signal = 0;
	int injected = 0, failed = 0, rxflags = 0;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		perror("recv");
		return;
	}

	if (ieee80211_radiotap_iterator_init(&iter, (void*)buf, len)) {
		printf("received invalid radiotap frame\n");
		return;
	}

	while (1) {
		ret = ieee80211_radiotap_iterator_next(&iter);
		if (ret == -ENOENT)
			break;
		if (ret) {
			printf("received invalid radiotap frame (%d)\n", ret);
			return;
		}
		switch (iter.this_arg_index) {
		case IEEE80211_RADIOTAP_FLAGS:
			if (*iter.this_arg & IEEE80211_RADIOTAP_F_FCS)
				len -= 4;
			break;
		case IEEE80211_RADIOTAP_RX_FLAGS:
			rxflags = 1;
			break;
		case IEEE80211_RADIOTAP_TX_FLAGS:
			injected = 1;
			failed = le_to_host16((*(uint16_t *) iter.this_arg)) &
					IEEE80211_RADIOTAP_F_TX_FAIL;
			break;
		case IEEE80211_RADIOTAP_DATA_RETRIES:
			break;
		case IEEE80211_RADIOTAP_CHANNEL:
			/* TODO: convert from freq/flags to channel number */
			break;
		case IEEE80211_RADIOTAP_RATE:
			datarate = *iter.this_arg * 5;
			break;
		case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
			ssi_signal = (s8) *iter.this_arg;
			break;
		}
	}

	if (rxflags && injected)
		return;

	if (!injected)
		handle_frame(drv, buf + iter.max_length,
			     len - iter.max_length, datarate, ssi_signal);
	else
		handle_tx_callback(drv->ctx, buf + iter.max_length,
				   len - iter.max_length, !failed);
}


/*
 * we post-process the filter code later and rewrite
 * this to the offset to the last instruction
 */
#define PASS	0xFF
#define FAIL	0xFE

static struct sock_filter msock_filter_insns[] = {
	/*
	 * do a little-endian load of the radiotap length field
	 */
	/* load lower byte into A */
	BPF_STMT(BPF_LD  | BPF_B | BPF_ABS, 2),
	/* put it into X (== index register) */
	BPF_STMT(BPF_MISC| BPF_TAX, 0),
	/* load upper byte into A */
	BPF_STMT(BPF_LD  | BPF_B | BPF_ABS, 3),
	/* left-shift it by 8 */
	BPF_STMT(BPF_ALU | BPF_LSH | BPF_K, 8),
	/* or with X */
	BPF_STMT(BPF_ALU | BPF_OR | BPF_X, 0),
	/* put result into X */
	BPF_STMT(BPF_MISC| BPF_TAX, 0),

	/*
	 * Allow management frames through, this also gives us those
	 * management frames that we sent ourselves with status
	 */
	/* load the lower byte of the IEEE 802.11 frame control field */
	BPF_STMT(BPF_LD  | BPF_B | BPF_IND, 0),
	/* mask off frame type and version */
	BPF_STMT(BPF_ALU | BPF_AND | BPF_K, 0xF),
	/* accept frame if it's both 0, fall through otherwise */
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, PASS, 0),

	/*
	 * TODO: add a bit to radiotap RX flags that indicates
	 * that the sending station is not associated, then
	 * add a filter here that filters on our DA and that flag
	 * to allow us to deauth frames to that bad station.
	 *
	 * For now allow all To DS data frames through.
	 */
	/* load the IEEE 802.11 frame control field */
	BPF_STMT(BPF_LD  | BPF_H | BPF_IND, 0),
	/* mask off frame type, version and DS status */
	BPF_STMT(BPF_ALU | BPF_AND | BPF_K, 0x0F03),
	/* accept frame if version 0, type 2 and To DS, fall through otherwise
	 */
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x0801, PASS, 0),

#if 0
	/*
	 * drop non-data frames
	 */
	/* load the lower byte of the frame control field */
	BPF_STMT(BPF_LD   | BPF_B | BPF_IND, 0),
	/* mask off QoS bit */
	BPF_STMT(BPF_ALU  | BPF_AND | BPF_K, 0x0c),
	/* drop non-data frames */
	BPF_JUMP(BPF_JMP  | BPF_JEQ | BPF_K, 8, 0, FAIL),
#endif
	/* load the upper byte of the frame control field */
	BPF_STMT(BPF_LD   | BPF_B | BPF_IND, 1),
	/* mask off toDS/fromDS */
	BPF_STMT(BPF_ALU  | BPF_AND | BPF_K, 0x03),
	/* accept WDS frames */
	BPF_JUMP(BPF_JMP  | BPF_JEQ | BPF_K, 3, PASS, 0),

	/*
	 * add header length to index
	 */
	/* load the lower byte of the frame control field */
	BPF_STMT(BPF_LD   | BPF_B | BPF_IND, 0),
	/* mask off QoS bit */
	BPF_STMT(BPF_ALU  | BPF_AND | BPF_K, 0x80),
	/* right shift it by 6 to give 0 or 2 */
	BPF_STMT(BPF_ALU  | BPF_RSH | BPF_K, 6),
	/* add data frame header length */
	BPF_STMT(BPF_ALU  | BPF_ADD | BPF_K, 24),
	/* add index, was start of 802.11 header */
	BPF_STMT(BPF_ALU  | BPF_ADD | BPF_X, 0),
	/* move to index, now start of LL header */
	BPF_STMT(BPF_MISC | BPF_TAX, 0),

	/*
	 * Accept empty data frames, we use those for
	 * polling activity.
	 */
	BPF_STMT(BPF_LD  | BPF_W | BPF_LEN, 0),
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_X, 0, PASS, 0),

	/*
	 * Accept EAPOL frames
	 */
	BPF_STMT(BPF_LD  | BPF_W | BPF_IND, 0),
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0xAAAA0300, 0, FAIL),
	BPF_STMT(BPF_LD  | BPF_W | BPF_IND, 4),
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x0000888E, PASS, FAIL),

	/* keep these last two statements or change the code below */
	/* return 0 == "DROP" */
	BPF_STMT(BPF_RET | BPF_K, 0),
	/* return ~0 == "keep all" */
	BPF_STMT(BPF_RET | BPF_K, ~0),
};

static struct sock_fprog msock_filter = {
	.len = sizeof(msock_filter_insns)/sizeof(msock_filter_insns[0]),
	.filter = msock_filter_insns,
};


static int add_monitor_filter(int s)
{
	int idx;

	/* rewrite all PASS/FAIL jump offsets */
	for (idx = 0; idx < msock_filter.len; idx++) {
		struct sock_filter *insn = &msock_filter_insns[idx];

		if (BPF_CLASS(insn->code) == BPF_JMP) {
			if (insn->code == (BPF_JMP|BPF_JA)) {
				if (insn->k == PASS)
					insn->k = msock_filter.len - idx - 2;
				else if (insn->k == FAIL)
					insn->k = msock_filter.len - idx - 3;
			}

			if (insn->jt == PASS)
				insn->jt = msock_filter.len - idx - 2;
			else if (insn->jt == FAIL)
				insn->jt = msock_filter.len - idx - 3;

			if (insn->jf == PASS)
				insn->jf = msock_filter.len - idx - 2;
			else if (insn->jf == FAIL)
				insn->jf = msock_filter.len - idx - 3;
		}
	}

	if (setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER,
		       &msock_filter, sizeof(msock_filter))) {
		perror("SO_ATTACH_FILTER");
		return -1;
	}

	return 0;
}


static void nl80211_remove_monitor_interface(
	struct wpa_driver_nl80211_data *drv)
{
	drv->monitor_refcount--;
	if (drv->monitor_refcount > 0)
		return;

	if (drv->monitor_ifidx >= 0) {
		nl80211_remove_iface(drv, drv->monitor_ifidx);
		drv->monitor_ifidx = -1;
	}
	if (drv->monitor_sock >= 0) {
		eloop_unregister_read_sock(drv->monitor_sock);
		close(drv->monitor_sock);
		drv->monitor_sock = -1;
	}
}


static int
nl80211_create_monitor_interface(struct wpa_driver_nl80211_data *drv)
{
	char buf[IFNAMSIZ];
	struct sockaddr_ll ll;
	int optval;
	socklen_t optlen;

	if (drv->monitor_ifidx >= 0) {
		drv->monitor_refcount++;
		return 0;
	}

	if (os_strncmp(drv->first_bss.ifname, "p2p-", 4) == 0) {
		/*
		 * P2P interface name is of the format p2p-%s-%d. For monitor
		 * interface name corresponding to P2P GO, replace "p2p-" with
		 * "mon-" to retain the same interface name length and to
		 * indicate that it is a monitor interface.
		 */
		snprintf(buf, IFNAMSIZ, "mon-%s", drv->first_bss.ifname + 4);
	} else {
		/* Non-P2P interface with AP functionality. */
		snprintf(buf, IFNAMSIZ, "mon.%s", drv->first_bss.ifname);
	}

	buf[IFNAMSIZ - 1] = '\0';

	drv->monitor_ifidx =
		nl80211_create_iface(drv, buf, NL80211_IFTYPE_MONITOR, NULL,
				     0);

	if (drv->monitor_ifidx == -EOPNOTSUPP) {
		/*
		 * This is backward compatibility for a few versions of
		 * the kernel only that didn't advertise the right
		 * attributes for the only driver that then supported
		 * AP mode w/o monitor -- ath6kl.
		 */
		wpa_printf(MSG_DEBUG, "nl80211: Driver does not support "
			   "monitor interface type - try to run without it");
		drv->device_ap_sme = 1;
	}

	if (drv->monitor_ifidx < 0)
		return -1;

	if (linux_set_iface_flags(drv->global->ioctl_sock, buf, 1))
		goto error;

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = drv->monitor_ifidx;
	drv->monitor_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (drv->monitor_sock < 0) {
		perror("socket[PF_PACKET,SOCK_RAW]");
		goto error;
	}

	if (add_monitor_filter(drv->monitor_sock)) {
		wpa_printf(MSG_INFO, "Failed to set socket filter for monitor "
			   "interface; do filtering in user space");
		/* This works, but will cost in performance. */
	}

	if (bind(drv->monitor_sock, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		perror("monitor socket bind");
		goto error;
	}

	optlen = sizeof(optval);
	optval = 20;
	if (setsockopt
	    (drv->monitor_sock, SOL_SOCKET, SO_PRIORITY, &optval, optlen)) {
		perror("Failed to set socket priority");
		goto error;
	}

	if (eloop_register_read_sock(drv->monitor_sock, handle_monitor_read,
				     drv, NULL)) {
		printf("Could not register monitor read socket\n");
		goto error;
	}

	return 0;
 error:
	nl80211_remove_monitor_interface(drv);
	return -1;
}


static int nl80211_setup_ap(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	wpa_printf(MSG_DEBUG, "nl80211: Setup AP - device_ap_sme=%d "
		   "use_monitor=%d", drv->device_ap_sme, drv->use_monitor);

	/*
	 * Disable Probe Request reporting unless we need it in this way for
	 * devices that include the AP SME, in the other case (unless using
	 * monitor iface) we'll get it through the nl_mgmt socket instead.
	 */
	if (!drv->device_ap_sme)
		wpa_driver_nl80211_probe_req_report(bss, 0);

	if (!drv->device_ap_sme && !drv->use_monitor)
		if (nl80211_mgmt_subscribe_ap(bss))
			return -1;

	if (drv->device_ap_sme && !drv->use_monitor)
		if (nl80211_mgmt_subscribe_ap_dev_sme(bss))
			return -1;

	if (!drv->device_ap_sme && drv->use_monitor &&
	    nl80211_create_monitor_interface(drv) &&
	    !drv->device_ap_sme)
		return -1;

	if (drv->device_ap_sme &&
	    wpa_driver_nl80211_probe_req_report(bss, 1) < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Failed to enable "
			   "Probe Request frame reporting in AP mode");
		/* Try to survive without this */
	}

	return 0;
}


static void nl80211_teardown_ap(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (drv->device_ap_sme) {
		wpa_driver_nl80211_probe_req_report(bss, 0);
		if (!drv->use_monitor)
			nl80211_mgmt_unsubscribe(bss, "AP teardown (dev SME)");
	} else if (drv->use_monitor)
		nl80211_remove_monitor_interface(drv);
	else
		nl80211_mgmt_unsubscribe(bss, "AP teardown");

	bss->beacon_set = 0;
}


static int nl80211_send_eapol_data(struct i802_bss *bss,
				   const u8 *addr, const u8 *data,
				   size_t data_len)
{
	struct sockaddr_ll ll;
	int ret;

	if (bss->drv->eapol_tx_sock < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: No socket to send EAPOL");
		return -1;
	}

	os_memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = bss->ifindex;
	ll.sll_protocol = htons(ETH_P_PAE);
	ll.sll_halen = ETH_ALEN;
	os_memcpy(ll.sll_addr, addr, ETH_ALEN);
	ret = sendto(bss->drv->eapol_tx_sock, data, data_len, 0,
		     (struct sockaddr *) &ll, sizeof(ll));
	if (ret < 0)
		wpa_printf(MSG_ERROR, "nl80211: EAPOL TX: %s",
			   strerror(errno));

	return ret;
}


static const u8 rfc1042_header[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };

static int wpa_driver_nl80211_hapd_send_eapol(
	void *priv, const u8 *addr, const u8 *data,
	size_t data_len, int encrypt, const u8 *own_addr, u32 flags)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_hdr *hdr;
	size_t len;
	u8 *pos;
	int res;
	int qos = flags & WPA_STA_WMM;

	if (drv->device_ap_sme || !drv->use_monitor)
		return nl80211_send_eapol_data(bss, addr, data, data_len);

	len = sizeof(*hdr) + (qos ? 2 : 0) + sizeof(rfc1042_header) + 2 +
		data_len;
	hdr = os_zalloc(len);
	if (hdr == NULL) {
		printf("malloc() failed for i802_send_data(len=%lu)\n",
		       (unsigned long) len);
		return -1;
	}

	hdr->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_DATA, WLAN_FC_STYPE_DATA);
	hdr->frame_control |= host_to_le16(WLAN_FC_FROMDS);
	if (encrypt)
		hdr->frame_control |= host_to_le16(WLAN_FC_ISWEP);
	if (qos) {
		hdr->frame_control |=
			host_to_le16(WLAN_FC_STYPE_QOS_DATA << 4);
	}

	memcpy(hdr->IEEE80211_DA_FROMDS, addr, ETH_ALEN);
	memcpy(hdr->IEEE80211_BSSID_FROMDS, own_addr, ETH_ALEN);
	memcpy(hdr->IEEE80211_SA_FROMDS, own_addr, ETH_ALEN);
	pos = (u8 *) (hdr + 1);

	if (qos) {
		/* Set highest priority in QoS header */
		pos[0] = 7;
		pos[1] = 0;
		pos += 2;
	}

	memcpy(pos, rfc1042_header, sizeof(rfc1042_header));
	pos += sizeof(rfc1042_header);
	WPA_PUT_BE16(pos, ETH_P_PAE);
	pos += 2;
	memcpy(pos, data, data_len);

	res = wpa_driver_nl80211_send_frame(bss, (u8 *) hdr, len, encrypt, 0,
					    0, 0, 0, 0);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "i802_send_eapol - packet len: %lu - "
			   "failed: %d (%s)",
			   (unsigned long) len, errno, strerror(errno));
	}
	os_free(hdr);

	return res;
}


static int wpa_driver_nl80211_sta_set_flags(void *priv, const u8 *addr,
					    int total_flags,
					    int flags_or, int flags_and)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg, *flags = NULL;
	struct nl80211_sta_flag_update upd;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	flags = nlmsg_alloc();
	if (!flags) {
		nlmsg_free(msg);
		return -ENOMEM;
	}

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_STATION);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(bss->ifname));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);

	/*
	 * Backwards compatibility version using NL80211_ATTR_STA_FLAGS. This
	 * can be removed eventually.
	 */
	if (total_flags & WPA_STA_AUTHORIZED)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_AUTHORIZED);

	if (total_flags & WPA_STA_WMM)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_WME);

	if (total_flags & WPA_STA_SHORT_PREAMBLE)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_SHORT_PREAMBLE);

	if (total_flags & WPA_STA_MFP)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_MFP);

	if (total_flags & WPA_STA_TDLS_PEER)
		NLA_PUT_FLAG(flags, NL80211_STA_FLAG_TDLS_PEER);

	if (nla_put_nested(msg, NL80211_ATTR_STA_FLAGS, flags))
		goto nla_put_failure;

	os_memset(&upd, 0, sizeof(upd));
	upd.mask = sta_flags_nl80211(flags_or | ~flags_and);
	upd.set = sta_flags_nl80211(flags_or);
	NLA_PUT(msg, NL80211_ATTR_STA_FLAGS2, sizeof(upd), &upd);

	nlmsg_free(flags);

	return send_and_recv_msgs(drv, msg, NULL, NULL);
 nla_put_failure:
	nlmsg_free(msg);
	nlmsg_free(flags);
	return -ENOBUFS;
}


static int wpa_driver_nl80211_ap(struct wpa_driver_nl80211_data *drv,
				 struct wpa_driver_associate_params *params)
{
	enum nl80211_iftype nlmode, old_mode;

	if (params->p2p) {
		wpa_printf(MSG_DEBUG, "nl80211: Setup AP operations for P2P "
			   "group (GO)");
		nlmode = NL80211_IFTYPE_P2P_GO;
	} else
		nlmode = NL80211_IFTYPE_AP;

	old_mode = drv->nlmode;
	if (wpa_driver_nl80211_set_mode(&drv->first_bss, nlmode)) {
		nl80211_remove_monitor_interface(drv);
		return -1;
	}

	if (wpa_driver_nl80211_set_freq(&drv->first_bss, params->freq, 0, 0)) {
		if (old_mode != nlmode)
			wpa_driver_nl80211_set_mode(&drv->first_bss, old_mode);
		nl80211_remove_monitor_interface(drv);
		return -1;
	}

	return 0;
}


static int nl80211_leave_ibss(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_LEAVE_IBSS);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Leave IBSS failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
		goto nla_put_failure;
	}

	ret = 0;
	wpa_printf(MSG_DEBUG, "nl80211: Leave IBSS request sent successfully");

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_ibss(struct wpa_driver_nl80211_data *drv,
				   struct wpa_driver_associate_params *params)
{
	struct nl_msg *msg;
	int ret = -1;
	int count = 0;

	wpa_printf(MSG_DEBUG, "nl80211: Join IBSS (ifindex=%d)", drv->ifindex);

	if (wpa_driver_nl80211_set_mode(&drv->first_bss,
					NL80211_IFTYPE_ADHOC)) {
		wpa_printf(MSG_INFO, "nl80211: Failed to set interface into "
			   "IBSS mode");
		return -1;
	}

retry:
	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_JOIN_IBSS);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	if (params->ssid == NULL || params->ssid_len > sizeof(drv->ssid))
		goto nla_put_failure;

	wpa_hexdump_ascii(MSG_DEBUG, "  * SSID",
			  params->ssid, params->ssid_len);
	NLA_PUT(msg, NL80211_ATTR_SSID, params->ssid_len,
		params->ssid);
	os_memcpy(drv->ssid, params->ssid, params->ssid_len);
	drv->ssid_len = params->ssid_len;

	wpa_printf(MSG_DEBUG, "  * freq=%d", params->freq);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, params->freq);

	ret = nl80211_set_conn_keys(params, msg);
	if (ret)
		goto nla_put_failure;

	if (params->bssid && params->fixed_bssid) {
		wpa_printf(MSG_DEBUG, "  * BSSID=" MACSTR,
			   MAC2STR(params->bssid));
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid);
	}

	if (params->key_mgmt_suite == KEY_MGMT_802_1X ||
	    params->key_mgmt_suite == KEY_MGMT_PSK ||
	    params->key_mgmt_suite == KEY_MGMT_802_1X_SHA256 ||
	    params->key_mgmt_suite == KEY_MGMT_PSK_SHA256) {
		wpa_printf(MSG_DEBUG, "  * control port");
		NLA_PUT_FLAG(msg, NL80211_ATTR_CONTROL_PORT);
	}

	if (params->wpa_ie) {
		wpa_hexdump(MSG_DEBUG,
			    "  * Extra IEs for Beacon/Probe Response frames",
			    params->wpa_ie, params->wpa_ie_len);
		NLA_PUT(msg, NL80211_ATTR_IE, params->wpa_ie_len,
			params->wpa_ie);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Join IBSS failed: ret=%d (%s)",
			   ret, strerror(-ret));
		count++;
		if (ret == -EALREADY && count == 1) {
			wpa_printf(MSG_DEBUG, "nl80211: Retry IBSS join after "
				   "forced leave");
			nl80211_leave_ibss(drv);
			nlmsg_free(msg);
			goto retry;
		}

		goto nla_put_failure;
	}
	ret = 0;
	wpa_printf(MSG_DEBUG, "nl80211: Join IBSS request sent successfully");

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_try_connect(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_driver_associate_params *params)
{
	struct nl_msg *msg;
	enum nl80211_auth_type type;
	int ret = 0;
	int algs;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: Connect (ifindex=%d)", drv->ifindex);
	nl80211_cmd(drv, msg, 0, NL80211_CMD_CONNECT);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	if (params->bssid) {
		wpa_printf(MSG_DEBUG, "  * bssid=" MACSTR,
			   MAC2STR(params->bssid));
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid);
	}
	if (params->freq) {
		wpa_printf(MSG_DEBUG, "  * freq=%d", params->freq);
		NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, params->freq);
	}
	if (params->bg_scan_period >= 0) {
		wpa_printf(MSG_DEBUG, "  * bg scan period=%d",
			   params->bg_scan_period);
		NLA_PUT_U16(msg, NL80211_ATTR_BG_SCAN_PERIOD,
			    params->bg_scan_period);
	}
	if (params->ssid) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * SSID",
				  params->ssid, params->ssid_len);
		NLA_PUT(msg, NL80211_ATTR_SSID, params->ssid_len,
			params->ssid);
		if (params->ssid_len > sizeof(drv->ssid))
			goto nla_put_failure;
		os_memcpy(drv->ssid, params->ssid, params->ssid_len);
		drv->ssid_len = params->ssid_len;
	}
	wpa_hexdump(MSG_DEBUG, "  * IEs", params->wpa_ie, params->wpa_ie_len);
	if (params->wpa_ie)
		NLA_PUT(msg, NL80211_ATTR_IE, params->wpa_ie_len,
			params->wpa_ie);

	algs = 0;
	if (params->auth_alg & WPA_AUTH_ALG_OPEN)
		algs++;
	if (params->auth_alg & WPA_AUTH_ALG_SHARED)
		algs++;
	if (params->auth_alg & WPA_AUTH_ALG_LEAP)
		algs++;
	if (algs > 1) {
		wpa_printf(MSG_DEBUG, "  * Leave out Auth Type for automatic "
			   "selection");
		goto skip_auth_type;
	}

	if (params->auth_alg & WPA_AUTH_ALG_OPEN)
		type = NL80211_AUTHTYPE_OPEN_SYSTEM;
	else if (params->auth_alg & WPA_AUTH_ALG_SHARED)
		type = NL80211_AUTHTYPE_SHARED_KEY;
	else if (params->auth_alg & WPA_AUTH_ALG_LEAP)
		type = NL80211_AUTHTYPE_NETWORK_EAP;
	else if (params->auth_alg & WPA_AUTH_ALG_FT)
		type = NL80211_AUTHTYPE_FT;
	else
		goto nla_put_failure;

	wpa_printf(MSG_DEBUG, "  * Auth Type %d", type);
	NLA_PUT_U32(msg, NL80211_ATTR_AUTH_TYPE, type);

skip_auth_type:
	if (params->wpa_proto) {
		enum nl80211_wpa_versions ver = 0;

		if (params->wpa_proto & WPA_PROTO_WPA)
			ver |= NL80211_WPA_VERSION_1;
		if (params->wpa_proto & WPA_PROTO_RSN)
			ver |= NL80211_WPA_VERSION_2;

		wpa_printf(MSG_DEBUG, "  * WPA Versions 0x%x", ver);
		NLA_PUT_U32(msg, NL80211_ATTR_WPA_VERSIONS, ver);
	}

	if (params->pairwise_suite != CIPHER_NONE) {
		int cipher;

		switch (params->pairwise_suite) {
		case CIPHER_SMS4:
			cipher = WLAN_CIPHER_SUITE_SMS4;
			break;
		case CIPHER_WEP40:
			cipher = WLAN_CIPHER_SUITE_WEP40;
			break;
		case CIPHER_WEP104:
			cipher = WLAN_CIPHER_SUITE_WEP104;
			break;
		case CIPHER_CCMP:
			cipher = WLAN_CIPHER_SUITE_CCMP;
			break;
		case CIPHER_GCMP:
			cipher = WLAN_CIPHER_SUITE_GCMP;
			break;
		case CIPHER_TKIP:
		default:
			cipher = WLAN_CIPHER_SUITE_TKIP;
			break;
		}
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITES_PAIRWISE, cipher);
	}

	if (params->group_suite != CIPHER_NONE) {
		int cipher;

		switch (params->group_suite) {
		case CIPHER_SMS4:
			cipher = WLAN_CIPHER_SUITE_SMS4;
			break;
		case CIPHER_WEP40:
			cipher = WLAN_CIPHER_SUITE_WEP40;
			break;
		case CIPHER_WEP104:
			cipher = WLAN_CIPHER_SUITE_WEP104;
			break;
		case CIPHER_CCMP:
			cipher = WLAN_CIPHER_SUITE_CCMP;
			break;
		case CIPHER_GCMP:
			cipher = WLAN_CIPHER_SUITE_GCMP;
			break;
		case CIPHER_TKIP:
		default:
			cipher = WLAN_CIPHER_SUITE_TKIP;
			break;
		}
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP, cipher);
	}

	if (params->key_mgmt_suite == KEY_MGMT_802_1X ||
	    params->key_mgmt_suite == KEY_MGMT_PSK ||
	    params->key_mgmt_suite == KEY_MGMT_CCKM) {
		int mgmt = WLAN_AKM_SUITE_PSK;

		switch (params->key_mgmt_suite) {
		case KEY_MGMT_CCKM:
			mgmt = WLAN_AKM_SUITE_CCKM;
			break;
		case KEY_MGMT_802_1X:
			mgmt = WLAN_AKM_SUITE_8021X;
			break;
		case KEY_MGMT_PSK:
		default:
			mgmt = WLAN_AKM_SUITE_PSK;
			break;
		}
		NLA_PUT_U32(msg, NL80211_ATTR_AKM_SUITES, mgmt);
	}

	if (params->disable_ht)
		NLA_PUT_FLAG(msg, NL80211_ATTR_DISABLE_HT);

	if (params->htcaps && params->htcaps_mask) {
		int sz = sizeof(struct ieee80211_ht_capabilities);
		NLA_PUT(msg, NL80211_ATTR_HT_CAPABILITY, sz, params->htcaps);
		NLA_PUT(msg, NL80211_ATTR_HT_CAPABILITY_MASK, sz,
			params->htcaps_mask);
	}

	ret = nl80211_set_conn_keys(params, msg);
	if (ret)
		goto nla_put_failure;

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: MLME connect failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
		goto nla_put_failure;
	}
	ret = 0;
	wpa_printf(MSG_DEBUG, "nl80211: Connect request send successfully");

nla_put_failure:
	nlmsg_free(msg);
	return ret;

}


static int wpa_driver_nl80211_connect(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_driver_associate_params *params)
{
	int ret = wpa_driver_nl80211_try_connect(drv, params);
	if (ret == -EALREADY) {
		/*
		 * cfg80211 does not currently accept new connections if
		 * we are already connected. As a workaround, force
		 * disconnection and try again.
		 */
		wpa_printf(MSG_DEBUG, "nl80211: Explicitly "
			   "disconnecting before reassociation "
			   "attempt");
		if (wpa_driver_nl80211_disconnect(
			    drv, WLAN_REASON_PREV_AUTH_NOT_VALID))
			return -1;
		/* Ignore the next local disconnect message. */
		drv->ignore_next_local_disconnect = 1;
		ret = wpa_driver_nl80211_try_connect(drv, params);
	}
	return ret;
}


static int wpa_driver_nl80211_associate(
	void *priv, struct wpa_driver_associate_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1;
	struct nl_msg *msg;

	if (params->mode == IEEE80211_MODE_AP)
		return wpa_driver_nl80211_ap(drv, params);

	if (params->mode == IEEE80211_MODE_IBSS)
		return wpa_driver_nl80211_ibss(drv, params);

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_SME)) {
		enum nl80211_iftype nlmode = params->p2p ?
			NL80211_IFTYPE_P2P_CLIENT : NL80211_IFTYPE_STATION;

		if (wpa_driver_nl80211_set_mode(priv, nlmode) < 0)
			return -1;
		return wpa_driver_nl80211_connect(drv, params);
	}

	drv->associated = 0;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: Associate (ifindex=%d)",
		   drv->ifindex);
	nl80211_cmd(drv, msg, 0, NL80211_CMD_ASSOCIATE);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	if (params->bssid) {
		wpa_printf(MSG_DEBUG, "  * bssid=" MACSTR,
			   MAC2STR(params->bssid));
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid);
	}
	if (params->freq) {
		wpa_printf(MSG_DEBUG, "  * freq=%d", params->freq);
		NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, params->freq);
		drv->assoc_freq = params->freq;
	} else
		drv->assoc_freq = 0;
	if (params->bg_scan_period >= 0) {
		wpa_printf(MSG_DEBUG, "  * bg scan period=%d",
			   params->bg_scan_period);
		NLA_PUT_U16(msg, NL80211_ATTR_BG_SCAN_PERIOD,
			    params->bg_scan_period);
	}
	if (params->ssid) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * SSID",
				  params->ssid, params->ssid_len);
		NLA_PUT(msg, NL80211_ATTR_SSID, params->ssid_len,
			params->ssid);
		if (params->ssid_len > sizeof(drv->ssid))
			goto nla_put_failure;
		os_memcpy(drv->ssid, params->ssid, params->ssid_len);
		drv->ssid_len = params->ssid_len;
	}
	wpa_hexdump(MSG_DEBUG, "  * IEs", params->wpa_ie, params->wpa_ie_len);
	if (params->wpa_ie)
		NLA_PUT(msg, NL80211_ATTR_IE, params->wpa_ie_len,
			params->wpa_ie);

	if (params->pairwise_suite != CIPHER_NONE) {
		int cipher;

		switch (params->pairwise_suite) {
		case CIPHER_WEP40:
			cipher = WLAN_CIPHER_SUITE_WEP40;
			break;
		case CIPHER_WEP104:
			cipher = WLAN_CIPHER_SUITE_WEP104;
			break;
		case CIPHER_CCMP:
			cipher = WLAN_CIPHER_SUITE_CCMP;
			break;
		case CIPHER_GCMP:
			cipher = WLAN_CIPHER_SUITE_GCMP;
			break;
		case CIPHER_TKIP:
		default:
			cipher = WLAN_CIPHER_SUITE_TKIP;
			break;
		}
		wpa_printf(MSG_DEBUG, "  * pairwise=0x%x", cipher);
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITES_PAIRWISE, cipher);
	}

	if (params->group_suite != CIPHER_NONE) {
		int cipher;

		switch (params->group_suite) {
		case CIPHER_WEP40:
			cipher = WLAN_CIPHER_SUITE_WEP40;
			break;
		case CIPHER_WEP104:
			cipher = WLAN_CIPHER_SUITE_WEP104;
			break;
		case CIPHER_CCMP:
			cipher = WLAN_CIPHER_SUITE_CCMP;
			break;
		case CIPHER_GCMP:
			cipher = WLAN_CIPHER_SUITE_GCMP;
			break;
		case CIPHER_TKIP:
		default:
			cipher = WLAN_CIPHER_SUITE_TKIP;
			break;
		}
		wpa_printf(MSG_DEBUG, "  * group=0x%x", cipher);
		NLA_PUT_U32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP, cipher);
	}

#ifdef CONFIG_IEEE80211W
	if (params->mgmt_frame_protection == MGMT_FRAME_PROTECTION_REQUIRED)
		NLA_PUT_U32(msg, NL80211_ATTR_USE_MFP, NL80211_MFP_REQUIRED);
#endif /* CONFIG_IEEE80211W */

	NLA_PUT_FLAG(msg, NL80211_ATTR_CONTROL_PORT);

	if (params->prev_bssid) {
		wpa_printf(MSG_DEBUG, "  * prev_bssid=" MACSTR,
			   MAC2STR(params->prev_bssid));
		NLA_PUT(msg, NL80211_ATTR_PREV_BSSID, ETH_ALEN,
			params->prev_bssid);
	}

	if (params->disable_ht)
		NLA_PUT_FLAG(msg, NL80211_ATTR_DISABLE_HT);

	if (params->htcaps && params->htcaps_mask) {
		int sz = sizeof(struct ieee80211_ht_capabilities);
		NLA_PUT(msg, NL80211_ATTR_HT_CAPABILITY, sz, params->htcaps);
		NLA_PUT(msg, NL80211_ATTR_HT_CAPABILITY_MASK, sz,
			params->htcaps_mask);
	}

	if (params->p2p)
		wpa_printf(MSG_DEBUG, "  * P2P group");

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: MLME command failed (assoc): ret=%d (%s)",
			ret, strerror(-ret));
		nl80211_dump_scan(drv);
		goto nla_put_failure;
	}
	ret = 0;
	wpa_printf(MSG_DEBUG, "nl80211: Association request send "
		   "successfully");

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int nl80211_set_mode(struct wpa_driver_nl80211_data *drv,
			    int ifindex, enum nl80211_iftype mode)
{
	struct nl_msg *msg;
	int ret = -ENOBUFS;

	wpa_printf(MSG_DEBUG, "nl80211: Set mode ifindex %d iftype %d (%s)",
		   ifindex, mode, nl80211_iftype_str(mode));

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_INTERFACE);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, mode);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (!ret)
		return 0;
nla_put_failure:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set interface %d to mode %d:"
		   " %d (%s)", ifindex, mode, ret, strerror(-ret));
	return ret;
}


static int wpa_driver_nl80211_set_mode(struct i802_bss *bss,
				       enum nl80211_iftype nlmode)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1;
	int i;
	int was_ap = is_ap_interface(drv->nlmode);
	int res;

	res = nl80211_set_mode(drv, drv->ifindex, nlmode);
	if (res == 0) {
		drv->nlmode = nlmode;
		ret = 0;
		goto done;
	}

	if (res == -ENODEV)
		return -1;

	if (nlmode == drv->nlmode) {
		wpa_printf(MSG_DEBUG, "nl80211: Interface already in "
			   "requested mode - ignore error");
		ret = 0;
		goto done; /* Already in the requested mode */
	}

	/* mac80211 doesn't allow mode changes while the device is up, so
	 * take the device down, try to set the mode again, and bring the
	 * device back up.
	 */
	wpa_printf(MSG_DEBUG, "nl80211: Try mode change after setting "
		   "interface down");
	for (i = 0; i < 10; i++) {
		res = linux_set_iface_flags(drv->global->ioctl_sock,
					    bss->ifname, 0);
		if (res == -EACCES || res == -ENODEV)
			break;
		if (res == 0) {
			/* Try to set the mode again while the interface is
			 * down */
			ret = nl80211_set_mode(drv, drv->ifindex, nlmode);
			if (ret == -EACCES)
				break;
			res = linux_set_iface_flags(drv->global->ioctl_sock,
						    bss->ifname, 1);
			if (res && !ret)
				ret = -1;
			else if (ret != -EBUSY)
				break;
		} else
			wpa_printf(MSG_DEBUG, "nl80211: Failed to set "
				   "interface down");
		os_sleep(0, 100000);
	}

	if (!ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Mode change succeeded while "
			   "interface is down");
		drv->nlmode = nlmode;
		drv->ignore_if_down_event = 1;
	}

done:
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Interface mode change to %d "
			   "from %d failed", nlmode, drv->nlmode);
		return ret;
	}

	if (is_p2p_interface(nlmode))
		nl80211_disable_11b_rates(drv, drv->ifindex, 1);
	else if (drv->disabled_11b_rates)
		nl80211_disable_11b_rates(drv, drv->ifindex, 0);

	if (is_ap_interface(nlmode)) {
		nl80211_mgmt_unsubscribe(bss, "start AP");
		/* Setup additional AP mode functionality if needed */
		if (nl80211_setup_ap(bss))
			return -1;
	} else if (was_ap) {
		/* Remove additional AP mode functionality */
		nl80211_teardown_ap(bss);
	} else {
		nl80211_mgmt_unsubscribe(bss, "mode change");
	}

	if (!bss->in_deinit && !is_ap_interface(nlmode) &&
	    nl80211_mgmt_subscribe_non_ap(bss) < 0)
		wpa_printf(MSG_DEBUG, "nl80211: Failed to register Action "
			   "frame processing - ignore for now");

	return 0;
}


static int wpa_driver_nl80211_get_capa(void *priv,
				       struct wpa_driver_capa *capa)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (!drv->has_capability)
		return -1;
	os_memcpy(capa, &drv->capa, sizeof(*capa));
	return 0;
}


static int wpa_driver_nl80211_set_operstate(void *priv, int state)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	wpa_printf(MSG_DEBUG, "%s: operstate %d->%d (%s)",
		   __func__, drv->operstate, state, state ? "UP" : "DORMANT");
	drv->operstate = state;
	return netlink_send_oper_ifla(drv->global->netlink, drv->ifindex, -1,
				      state ? IF_OPER_UP : IF_OPER_DORMANT);
}


static int wpa_driver_nl80211_set_supp_port(void *priv, int authorized)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nl80211_sta_flag_update upd;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_STATION);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(bss->ifname));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, drv->bssid);

	os_memset(&upd, 0, sizeof(upd));
	upd.mask = BIT(NL80211_STA_FLAG_AUTHORIZED);
	if (authorized)
		upd.set = BIT(NL80211_STA_FLAG_AUTHORIZED);
	NLA_PUT(msg, NL80211_ATTR_STA_FLAGS2, sizeof(upd), &upd);

	return send_and_recv_msgs(drv, msg, NULL, NULL);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


/* Set kernel driver on given frequency (MHz) */
static int i802_set_freq(void *priv, struct hostapd_freq_params *freq)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_set_freq(bss, freq->freq, freq->ht_enabled,
					   freq->sec_channel_offset);
}


#if defined(HOSTAPD) || defined(CONFIG_AP)

static inline int min_int(int a, int b)
{
	if (a < b)
		return a;
	return b;
}


static int get_key_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	/*
	 * TODO: validate the key index and mac address!
	 * Otherwise, there's a race condition as soon as
	 * the kernel starts sending key notifications.
	 */

	if (tb[NL80211_ATTR_KEY_SEQ])
		memcpy(arg, nla_data(tb[NL80211_ATTR_KEY_SEQ]),
		       min_int(nla_len(tb[NL80211_ATTR_KEY_SEQ]), 6));
	return NL_SKIP;
}


static int i802_get_seqnum(const char *iface, void *priv, const u8 *addr,
			   int idx, u8 *seq)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_KEY);

	if (addr)
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, idx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(iface));

	memset(seq, 0, 6);

	return send_and_recv_msgs(drv, msg, get_key_handler, seq);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int i802_set_rts(void *priv, int rts)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -ENOBUFS;
	u32 val;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	if (rts >= 2347)
		val = (u32) -1;
	else
		val = rts;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_WIPHY);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_RTS_THRESHOLD, val);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (!ret)
		return 0;
nla_put_failure:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set RTS threshold %d: "
		   "%d (%s)", rts, ret, strerror(-ret));
	return ret;
}


static int i802_set_frag(void *priv, int frag)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -ENOBUFS;
	u32 val;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	if (frag >= 2346)
		val = (u32) -1;
	else
		val = frag;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_WIPHY);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FRAG_THRESHOLD, val);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (!ret)
		return 0;
nla_put_failure:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set fragmentation threshold "
		   "%d: %d (%s)", frag, ret, strerror(-ret));
	return ret;
}


static int i802_flush(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int res;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_DEL_STATION);

	/*
	 * XXX: FIX! this needs to flush all VLANs too
	 */
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(bss->ifname));

	res = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (res) {
		wpa_printf(MSG_DEBUG, "nl80211: Station flush failed: ret=%d "
			   "(%s)", res, strerror(-res));
	}
	return res;
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}

#endif /* HOSTAPD || CONFIG_AP */


static int get_sta_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct hostap_sta_driver_data *data = arg;
	struct nlattr *stats[NL80211_STA_INFO_MAX + 1];
	static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
		[NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
		[NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
		[NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
		[NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
		[NL80211_STA_INFO_TX_FAILED] = { .type = NLA_U32 },
	};

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	/*
	 * TODO: validate the interface and mac address!
	 * Otherwise, there's a race condition as soon as
	 * the kernel starts sending station notifications.
	 */

	if (!tb[NL80211_ATTR_STA_INFO]) {
		wpa_printf(MSG_DEBUG, "sta stats missing!");
		return NL_SKIP;
	}
	if (nla_parse_nested(stats, NL80211_STA_INFO_MAX,
			     tb[NL80211_ATTR_STA_INFO],
			     stats_policy)) {
		wpa_printf(MSG_DEBUG, "failed to parse nested attributes!");
		return NL_SKIP;
	}

	if (stats[NL80211_STA_INFO_INACTIVE_TIME])
		data->inactive_msec =
			nla_get_u32(stats[NL80211_STA_INFO_INACTIVE_TIME]);
	if (stats[NL80211_STA_INFO_RX_BYTES])
		data->rx_bytes = nla_get_u32(stats[NL80211_STA_INFO_RX_BYTES]);
	if (stats[NL80211_STA_INFO_TX_BYTES])
		data->tx_bytes = nla_get_u32(stats[NL80211_STA_INFO_TX_BYTES]);
	if (stats[NL80211_STA_INFO_RX_PACKETS])
		data->rx_packets =
			nla_get_u32(stats[NL80211_STA_INFO_RX_PACKETS]);
	if (stats[NL80211_STA_INFO_TX_PACKETS])
		data->tx_packets =
			nla_get_u32(stats[NL80211_STA_INFO_TX_PACKETS]);
	if (stats[NL80211_STA_INFO_TX_FAILED])
		data->tx_retry_failed =
			nla_get_u32(stats[NL80211_STA_INFO_TX_FAILED]);

	return NL_SKIP;
}

static int i802_read_sta_data(void *priv, struct hostap_sta_driver_data *data,
			      const u8 *addr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	os_memset(data, 0, sizeof(*data));
	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_STATION);

	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(bss->ifname));

	return send_and_recv_msgs(drv, msg, get_sta_handler, data);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


#if defined(HOSTAPD) || defined(CONFIG_AP)

static int i802_set_tx_queue_params(void *priv, int queue, int aifs,
				    int cw_min, int cw_max, int burst_time)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *txq, *params;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_WIPHY);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(bss->ifname));

	txq = nla_nest_start(msg, NL80211_ATTR_WIPHY_TXQ_PARAMS);
	if (!txq)
		goto nla_put_failure;

	/* We are only sending parameters for a single TXQ at a time */
	params = nla_nest_start(msg, 1);
	if (!params)
		goto nla_put_failure;

	switch (queue) {
	case 0:
		NLA_PUT_U8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_VO);
		break;
	case 1:
		NLA_PUT_U8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_VI);
		break;
	case 2:
		NLA_PUT_U8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_BE);
		break;
	case 3:
		NLA_PUT_U8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_BK);
		break;
	}
	/* Burst time is configured in units of 0.1 msec and TXOP parameter in
	 * 32 usec, so need to convert the value here. */
	NLA_PUT_U16(msg, NL80211_TXQ_ATTR_TXOP, (burst_time * 100 + 16) / 32);
	NLA_PUT_U16(msg, NL80211_TXQ_ATTR_CWMIN, cw_min);
	NLA_PUT_U16(msg, NL80211_TXQ_ATTR_CWMAX, cw_max);
	NLA_PUT_U8(msg, NL80211_TXQ_ATTR_AIFS, aifs);

	nla_nest_end(msg, params);

	nla_nest_end(msg, txq);

	if (send_and_recv_msgs(drv, msg, NULL, NULL) == 0)
		return 0;
	msg = NULL;
 nla_put_failure:
	nlmsg_free(msg);
	return -1;
}


static int i802_set_sta_vlan(void *priv, const u8 *addr,
			     const char *ifname, int vlan_id)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -ENOBUFS;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_STATION);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX,
		    if_nametoindex(bss->ifname));
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	NLA_PUT_U32(msg, NL80211_ATTR_STA_VLAN,
		    if_nametoindex(ifname));

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "nl80211: NL80211_ATTR_STA_VLAN (addr="
			   MACSTR " ifname=%s vlan_id=%d) failed: %d (%s)",
			   MAC2STR(addr), ifname, vlan_id, ret,
			   strerror(-ret));
	}
 nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int i802_get_inact_sec(void *priv, const u8 *addr)
{
	struct hostap_sta_driver_data data;
	int ret;

	data.inactive_msec = (unsigned long) -1;
	ret = i802_read_sta_data(priv, &data, addr);
	if (ret || data.inactive_msec == (unsigned long) -1)
		return -1;
	return data.inactive_msec / 1000;
}


static int i802_sta_clear_stats(void *priv, const u8 *addr)
{
#if 0
	/* TODO */
#endif
	return 0;
}


static int i802_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
			   int reason)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_mgmt mgmt;

	if (drv->device_ap_sme)
		return wpa_driver_nl80211_sta_remove(bss, addr);

	memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_DEAUTH);
	memcpy(mgmt.da, addr, ETH_ALEN);
	memcpy(mgmt.sa, own_addr, ETH_ALEN);
	memcpy(mgmt.bssid, own_addr, ETH_ALEN);
	mgmt.u.deauth.reason_code = host_to_le16(reason);
	return wpa_driver_nl80211_send_mlme(bss, (u8 *) &mgmt,
					    IEEE80211_HDRLEN +
					    sizeof(mgmt.u.deauth), 0);
}


static int i802_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
			     int reason)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_mgmt mgmt;

	if (drv->device_ap_sme)
		return wpa_driver_nl80211_sta_remove(bss, addr);

	memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_DISASSOC);
	memcpy(mgmt.da, addr, ETH_ALEN);
	memcpy(mgmt.sa, own_addr, ETH_ALEN);
	memcpy(mgmt.bssid, own_addr, ETH_ALEN);
	mgmt.u.disassoc.reason_code = host_to_le16(reason);
	return wpa_driver_nl80211_send_mlme(bss, (u8 *) &mgmt,
					    IEEE80211_HDRLEN +
					    sizeof(mgmt.u.disassoc), 0);
}

#endif /* HOSTAPD || CONFIG_AP */

#ifdef HOSTAPD

static void add_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx)
{
	int i;
	int *old;

	wpa_printf(MSG_DEBUG, "nl80211: Add own interface ifindex %d",
		   ifidx);
	for (i = 0; i < drv->num_if_indices; i++) {
		if (drv->if_indices[i] == 0) {
			drv->if_indices[i] = ifidx;
			return;
		}
	}

	if (drv->if_indices != drv->default_if_indices)
		old = drv->if_indices;
	else
		old = NULL;

	drv->if_indices = os_realloc_array(old, drv->num_if_indices + 1,
					   sizeof(int));
	if (!drv->if_indices) {
		if (!old)
			drv->if_indices = drv->default_if_indices;
		else
			drv->if_indices = old;
		wpa_printf(MSG_ERROR, "Failed to reallocate memory for "
			   "interfaces");
		wpa_printf(MSG_ERROR, "Ignoring EAPOL on interface %d", ifidx);
		return;
	} else if (!old)
		os_memcpy(drv->if_indices, drv->default_if_indices,
			  sizeof(drv->default_if_indices));
	drv->if_indices[drv->num_if_indices] = ifidx;
	drv->num_if_indices++;
}


static void del_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx)
{
	int i;

	for (i = 0; i < drv->num_if_indices; i++) {
		if (drv->if_indices[i] == ifidx) {
			drv->if_indices[i] = 0;
			break;
		}
	}
}


static int have_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx)
{
	int i;

	for (i = 0; i < drv->num_if_indices; i++)
		if (drv->if_indices[i] == ifidx)
			return 1;

	return 0;
}


static int i802_set_wds_sta(void *priv, const u8 *addr, int aid, int val,
                            const char *bridge_ifname)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	char name[IFNAMSIZ + 1];

	os_snprintf(name, sizeof(name), "%s.sta%d", bss->ifname, aid);
	wpa_printf(MSG_DEBUG, "nl80211: Set WDS STA addr=" MACSTR
		   " aid=%d val=%d name=%s", MAC2STR(addr), aid, val, name);
	if (val) {
		if (!if_nametoindex(name)) {
			if (nl80211_create_iface(drv, name,
						 NL80211_IFTYPE_AP_VLAN,
						 NULL, 1) < 0)
				return -1;
			if (bridge_ifname &&
			    linux_br_add_if(drv->global->ioctl_sock,
					    bridge_ifname, name) < 0)
				return -1;
		}
		if (linux_set_iface_flags(drv->global->ioctl_sock, name, 1)) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to set WDS STA "
				   "interface %s up", name);
		}
		return i802_set_sta_vlan(priv, addr, name, 0);
	} else {
		if (bridge_ifname)
			linux_br_del_if(drv->global->ioctl_sock, bridge_ifname,
					name);

		i802_set_sta_vlan(priv, addr, bss->ifname, 0);
		return wpa_driver_nl80211_if_remove(priv, WPA_IF_AP_VLAN,
						    name);
	}
}


static void handle_eapol(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wpa_driver_nl80211_data *drv = eloop_ctx;
	struct sockaddr_ll lladdr;
	unsigned char buf[3000];
	int len;
	socklen_t fromlen = sizeof(lladdr);

	len = recvfrom(sock, buf, sizeof(buf), 0,
		       (struct sockaddr *)&lladdr, &fromlen);
	if (len < 0) {
		perror("recv");
		return;
	}

	if (have_ifidx(drv, lladdr.sll_ifindex))
		drv_event_eapol_rx(drv->ctx, lladdr.sll_addr, buf, len);
}


static int i802_check_bridge(struct wpa_driver_nl80211_data *drv,
			     struct i802_bss *bss,
			     const char *brname, const char *ifname)
{
	int ifindex;
	char in_br[IFNAMSIZ];

	os_strlcpy(bss->brname, brname, IFNAMSIZ);
	ifindex = if_nametoindex(brname);
	if (ifindex == 0) {
		/*
		 * Bridge was configured, but the bridge device does
		 * not exist. Try to add it now.
		 */
		if (linux_br_add(drv->global->ioctl_sock, brname) < 0) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to add the "
				   "bridge interface %s: %s",
				   brname, strerror(errno));
			return -1;
		}
		bss->added_bridge = 1;
		add_ifidx(drv, if_nametoindex(brname));
	}

	if (linux_br_get(in_br, ifname) == 0) {
		if (os_strcmp(in_br, brname) == 0)
			return 0; /* already in the bridge */

		wpa_printf(MSG_DEBUG, "nl80211: Removing interface %s from "
			   "bridge %s", ifname, in_br);
		if (linux_br_del_if(drv->global->ioctl_sock, in_br, ifname) <
		    0) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to "
				   "remove interface %s from bridge "
				   "%s: %s",
				   ifname, brname, strerror(errno));
			return -1;
		}
	}

	wpa_printf(MSG_DEBUG, "nl80211: Adding interface %s into bridge %s",
		   ifname, brname);
	if (linux_br_add_if(drv->global->ioctl_sock, brname, ifname) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to add interface %s "
			   "into bridge %s: %s",
			   ifname, brname, strerror(errno));
		return -1;
	}
	bss->added_if_into_bridge = 1;

	return 0;
}


static void *i802_init(struct hostapd_data *hapd,
		       struct wpa_init_params *params)
{
	struct wpa_driver_nl80211_data *drv;
	struct i802_bss *bss;
	size_t i;
	char brname[IFNAMSIZ];
	int ifindex, br_ifindex;
	int br_added = 0;

	bss = wpa_driver_nl80211_init(hapd, params->ifname,
				      params->global_priv);
	if (bss == NULL)
		return NULL;

	drv = bss->drv;
	drv->nlmode = NL80211_IFTYPE_AP;
	drv->eapol_sock = -1;

	if (linux_br_get(brname, params->ifname) == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Interface %s is in bridge %s",
			   params->ifname, brname);
		br_ifindex = if_nametoindex(brname);
	} else {
		brname[0] = '\0';
		br_ifindex = 0;
	}

	drv->num_if_indices = sizeof(drv->default_if_indices) / sizeof(int);
	drv->if_indices = drv->default_if_indices;
	for (i = 0; i < params->num_bridge; i++) {
		if (params->bridge[i]) {
			ifindex = if_nametoindex(params->bridge[i]);
			if (ifindex)
				add_ifidx(drv, ifindex);
			if (ifindex == br_ifindex)
				br_added = 1;
		}
	}
	if (!br_added && br_ifindex &&
	    (params->num_bridge == 0 || !params->bridge[0]))
		add_ifidx(drv, br_ifindex);

	/* start listening for EAPOL on the default AP interface */
	add_ifidx(drv, drv->ifindex);

	if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 0))
		goto failed;

	if (params->bssid) {
		if (linux_set_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
				       params->bssid))
			goto failed;
	}

	if (wpa_driver_nl80211_set_mode(bss, drv->nlmode)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to set interface %s "
			   "into AP mode", bss->ifname);
		goto failed;
	}

	if (params->num_bridge && params->bridge[0] &&
	    i802_check_bridge(drv, bss, params->bridge[0], params->ifname) < 0)
		goto failed;

	if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1))
		goto failed;

	drv->eapol_sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_PAE));
	if (drv->eapol_sock < 0) {
		perror("socket(PF_PACKET, SOCK_DGRAM, ETH_P_PAE)");
		goto failed;
	}

	if (eloop_register_read_sock(drv->eapol_sock, handle_eapol, drv, NULL))
	{
		printf("Could not register read socket for eapol\n");
		goto failed;
	}

	if (linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
			       params->own_addr))
		goto failed;

	memcpy(bss->addr, params->own_addr, ETH_ALEN);

	return bss;

failed:
	wpa_driver_nl80211_deinit(bss);
	return NULL;
}


static void i802_deinit(void *priv)
{
	wpa_driver_nl80211_deinit(priv);
}

#endif /* HOSTAPD */


static enum nl80211_iftype wpa_driver_nl80211_if_type(
	enum wpa_driver_if_type type)
{
	switch (type) {
	case WPA_IF_STATION:
		return NL80211_IFTYPE_STATION;
	case WPA_IF_P2P_CLIENT:
	case WPA_IF_P2P_GROUP:
		return NL80211_IFTYPE_P2P_CLIENT;
	case WPA_IF_AP_VLAN:
		return NL80211_IFTYPE_AP_VLAN;
	case WPA_IF_AP_BSS:
		return NL80211_IFTYPE_AP;
	case WPA_IF_P2P_GO:
		return NL80211_IFTYPE_P2P_GO;
	}
	return -1;
}


#ifdef CONFIG_P2P

static int nl80211_addr_in_use(struct nl80211_global *global, const u8 *addr)
{
	struct wpa_driver_nl80211_data *drv;
	dl_list_for_each(drv, &global->interfaces,
			 struct wpa_driver_nl80211_data, list) {
		if (os_memcmp(addr, drv->first_bss.addr, ETH_ALEN) == 0)
			return 1;
	}
	return 0;
}


static int nl80211_p2p_interface_addr(struct wpa_driver_nl80211_data *drv,
				      u8 *new_addr)
{
	unsigned int idx;

	if (!drv->global)
		return -1;

	os_memcpy(new_addr, drv->first_bss.addr, ETH_ALEN);
	for (idx = 0; idx < 64; idx++) {
		new_addr[0] = drv->first_bss.addr[0] | 0x02;
		new_addr[0] ^= idx << 2;
		if (!nl80211_addr_in_use(drv->global, new_addr))
			break;
	}
	if (idx == 64)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: Assigned new P2P Interface Address "
		   MACSTR, MAC2STR(new_addr));

	return 0;
}

#endif /* CONFIG_P2P */


static int wpa_driver_nl80211_if_add(void *priv, enum wpa_driver_if_type type,
				     const char *ifname, const u8 *addr,
				     void *bss_ctx, void **drv_priv,
				     char *force_ifname, u8 *if_addr,
				     const char *bridge)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ifidx;
#ifdef HOSTAPD
	struct i802_bss *new_bss = NULL;

	if (type == WPA_IF_AP_BSS) {
		new_bss = os_zalloc(sizeof(*new_bss));
		if (new_bss == NULL)
			return -1;
	}
#endif /* HOSTAPD */

	if (addr)
		os_memcpy(if_addr, addr, ETH_ALEN);
	ifidx = nl80211_create_iface(drv, ifname,
				     wpa_driver_nl80211_if_type(type), addr,
				     0);
	if (ifidx < 0) {
#ifdef HOSTAPD
		os_free(new_bss);
#endif /* HOSTAPD */
		return -1;
	}

	if (!addr &&
	    linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
			       if_addr) < 0) {
		nl80211_remove_iface(drv, ifidx);
		return -1;
	}

#ifdef CONFIG_P2P
	if (!addr &&
	    (type == WPA_IF_P2P_CLIENT || type == WPA_IF_P2P_GROUP ||
	     type == WPA_IF_P2P_GO)) {
		/* Enforce unique P2P Interface Address */
		u8 new_addr[ETH_ALEN], own_addr[ETH_ALEN];

		if (linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
				       own_addr) < 0 ||
		    linux_get_ifhwaddr(drv->global->ioctl_sock, ifname,
				       new_addr) < 0) {
			nl80211_remove_iface(drv, ifidx);
			return -1;
		}
		if (os_memcmp(own_addr, new_addr, ETH_ALEN) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Allocate new address "
				   "for P2P group interface");
			if (nl80211_p2p_interface_addr(drv, new_addr) < 0) {
				nl80211_remove_iface(drv, ifidx);
				return -1;
			}
			if (linux_set_ifhwaddr(drv->global->ioctl_sock, ifname,
					       new_addr) < 0) {
				nl80211_remove_iface(drv, ifidx);
				return -1;
			}
		}
		os_memcpy(if_addr, new_addr, ETH_ALEN);
	}
#endif /* CONFIG_P2P */

#ifdef HOSTAPD
	if (bridge &&
	    i802_check_bridge(drv, new_bss, bridge, ifname) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to add the new "
			   "interface %s to a bridge %s", ifname, bridge);
		nl80211_remove_iface(drv, ifidx);
		os_free(new_bss);
		return -1;
	}

	if (type == WPA_IF_AP_BSS) {
		if (linux_set_iface_flags(drv->global->ioctl_sock, ifname, 1))
		{
			nl80211_remove_iface(drv, ifidx);
			os_free(new_bss);
			return -1;
		}
		os_strlcpy(new_bss->ifname, ifname, IFNAMSIZ);
		os_memcpy(new_bss->addr, if_addr, ETH_ALEN);
		new_bss->ifindex = ifidx;
		new_bss->drv = drv;
		new_bss->next = drv->first_bss.next;
		new_bss->freq = drv->first_bss.freq;
		new_bss->ctx = bss_ctx;
		drv->first_bss.next = new_bss;
		if (drv_priv)
			*drv_priv = new_bss;
		nl80211_init_bss(new_bss);

		/* Subscribe management frames for this WPA_IF_AP_BSS */
		if (nl80211_setup_ap(new_bss))
			return -1;
	}
#endif /* HOSTAPD */

	if (drv->global)
		drv->global->if_add_ifindex = ifidx;

	return 0;
}


static int wpa_driver_nl80211_if_remove(void *priv,
					enum wpa_driver_if_type type,
					const char *ifname)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ifindex = if_nametoindex(ifname);

	wpa_printf(MSG_DEBUG, "nl80211: %s(type=%d ifname=%s) ifindex=%d",
		   __func__, type, ifname, ifindex);
	if (ifindex <= 0)
		return -1;

	nl80211_remove_iface(drv, ifindex);

#ifdef HOSTAPD
	if (type != WPA_IF_AP_BSS)
		return 0;

	if (bss->added_if_into_bridge) {
		if (linux_br_del_if(drv->global->ioctl_sock, bss->brname,
				    bss->ifname) < 0)
			wpa_printf(MSG_INFO, "nl80211: Failed to remove "
				   "interface %s from bridge %s: %s",
				   bss->ifname, bss->brname, strerror(errno));
	}
	if (bss->added_bridge) {
		if (linux_br_del(drv->global->ioctl_sock, bss->brname) < 0)
			wpa_printf(MSG_INFO, "nl80211: Failed to remove "
				   "bridge %s: %s",
				   bss->brname, strerror(errno));
	}

	if (bss != &drv->first_bss) {
		struct i802_bss *tbss;

		for (tbss = &drv->first_bss; tbss; tbss = tbss->next) {
			if (tbss->next == bss) {
				tbss->next = bss->next;
				/* Unsubscribe management frames */
				nl80211_teardown_ap(bss);
				nl80211_destroy_bss(bss);
				os_free(bss);
				bss = NULL;
				break;
			}
		}
		if (bss)
			wpa_printf(MSG_INFO, "nl80211: %s - could not find "
				   "BSS %p in the list", __func__, bss);
	}
#endif /* HOSTAPD */

	return 0;
}


static int cookie_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	u64 *cookie = arg;
	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (tb[NL80211_ATTR_COOKIE])
		*cookie = nla_get_u64(tb[NL80211_ATTR_COOKIE]);
	return NL_SKIP;
}


static int nl80211_send_frame_cmd(struct i802_bss *bss,
				  unsigned int freq, unsigned int wait,
				  const u8 *buf, size_t buf_len,
				  u64 *cookie_out, int no_cck, int no_ack,
				  int offchanok)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	u64 cookie;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: CMD_FRAME freq=%u wait=%u no_cck=%d "
		   "no_ack=%d offchanok=%d",
		   freq, wait, no_cck, no_ack, offchanok);
	nl80211_cmd(drv, msg, 0, NL80211_CMD_FRAME);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	if (wait)
		NLA_PUT_U32(msg, NL80211_ATTR_DURATION, wait);
	if (offchanok && (drv->capa.flags & WPA_DRIVER_FLAGS_OFFCHANNEL_TX))
		NLA_PUT_FLAG(msg, NL80211_ATTR_OFFCHANNEL_TX_OK);
	if (no_cck)
		NLA_PUT_FLAG(msg, NL80211_ATTR_TX_NO_CCK_RATE);
	if (no_ack)
		NLA_PUT_FLAG(msg, NL80211_ATTR_DONT_WAIT_FOR_ACK);

	NLA_PUT(msg, NL80211_ATTR_FRAME, buf_len, buf);

	cookie = 0;
	ret = send_and_recv_msgs(drv, msg, cookie_handler, &cookie);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Frame command failed: ret=%d "
			   "(%s) (freq=%u wait=%u)", ret, strerror(-ret),
			   freq, wait);
		goto nla_put_failure;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Frame TX command accepted%s; "
		   "cookie 0x%llx", no_ack ? " (no ACK)" : "",
		   (long long unsigned int) cookie);

	if (cookie_out)
		*cookie_out = no_ack ? (u64) -1 : cookie;

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_send_action(void *priv, unsigned int freq,
					  unsigned int wait_time,
					  const u8 *dst, const u8 *src,
					  const u8 *bssid,
					  const u8 *data, size_t data_len,
					  int no_cck)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1;
	u8 *buf;
	struct ieee80211_hdr *hdr;

	wpa_printf(MSG_DEBUG, "nl80211: Send Action frame (ifindex=%d, "
		   "freq=%u MHz wait=%d ms no_cck=%d)",
		   drv->ifindex, freq, wait_time, no_cck);

	buf = os_zalloc(24 + data_len);
	if (buf == NULL)
		return ret;
	os_memcpy(buf + 24, data, data_len);
	hdr = (struct ieee80211_hdr *) buf;
	hdr->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_ACTION);
	os_memcpy(hdr->addr1, dst, ETH_ALEN);
	os_memcpy(hdr->addr2, src, ETH_ALEN);
	os_memcpy(hdr->addr3, bssid, ETH_ALEN);

	if (is_ap_interface(drv->nlmode))
		ret = wpa_driver_nl80211_send_mlme_freq(priv, buf,
							24 + data_len,
							0, freq, no_cck, 1,
							wait_time);
	else
		ret = nl80211_send_frame_cmd(bss, freq, wait_time, buf,
					     24 + data_len,
					     &drv->send_action_cookie,
					     no_cck, 0, 1);

	os_free(buf);
	return ret;
}


static void wpa_driver_nl80211_send_action_cancel_wait(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	msg = nlmsg_alloc();
	if (!msg)
		return;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_FRAME_WAIT_CANCEL);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U64(msg, NL80211_ATTR_COOKIE, drv->send_action_cookie);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: wait cancel failed: ret=%d "
			   "(%s)", ret, strerror(-ret));

 nla_put_failure:
	nlmsg_free(msg);
}


static int wpa_driver_nl80211_remain_on_channel(void *priv, unsigned int freq,
						unsigned int duration)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	u64 cookie;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_REMAIN_ON_CHANNEL);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_WIPHY_FREQ, freq);
	NLA_PUT_U32(msg, NL80211_ATTR_DURATION, duration);

	cookie = 0;
	ret = send_and_recv_msgs(drv, msg, cookie_handler, &cookie);
	msg = NULL;
	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Remain-on-channel cookie "
			   "0x%llx for freq=%u MHz duration=%u",
			   (long long unsigned int) cookie, freq, duration);
		drv->remain_on_chan_cookie = cookie;
		drv->pending_remain_on_chan = 1;
		return 0;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Failed to request remain-on-channel "
		   "(freq=%d duration=%u): %d (%s)",
		   freq, duration, ret, strerror(-ret));
nla_put_failure:
	nlmsg_free(msg);
	return -1;
}


static int wpa_driver_nl80211_cancel_remain_on_channel(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	if (!drv->pending_remain_on_chan) {
		wpa_printf(MSG_DEBUG, "nl80211: No pending remain-on-channel "
			   "to cancel");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Cancel remain-on-channel with cookie "
		   "0x%llx",
		   (long long unsigned int) drv->remain_on_chan_cookie);

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U64(msg, NL80211_ATTR_COOKIE, drv->remain_on_chan_cookie);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret == 0)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: Failed to cancel remain-on-channel: "
		   "%d (%s)", ret, strerror(-ret));
nla_put_failure:
	nlmsg_free(msg);
	return -1;
}


static int wpa_driver_nl80211_probe_req_report(void *priv, int report)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (!report) {
		if (bss->nl_preq && drv->device_ap_sme &&
		    is_ap_interface(drv->nlmode)) {
			/*
			 * Do not disable Probe Request reporting that was
			 * enabled in nl80211_setup_ap().
			 */
			wpa_printf(MSG_DEBUG, "nl80211: Skip disabling of "
				   "Probe Request reporting nl_preq=%p while "
				   "in AP mode", bss->nl_preq);
		} else if (bss->nl_preq) {
			wpa_printf(MSG_DEBUG, "nl80211: Disable Probe Request "
				   "reporting nl_preq=%p", bss->nl_preq);
			eloop_unregister_read_sock(
				nl_socket_get_fd(bss->nl_preq));
			nl_destroy_handles(&bss->nl_preq);
		}
		return 0;
	}

	if (bss->nl_preq) {
		wpa_printf(MSG_DEBUG, "nl80211: Probe Request reporting "
			   "already on! nl_preq=%p", bss->nl_preq);
		return 0;
	}

	bss->nl_preq = nl_create_handle(drv->global->nl_cb, "preq");
	if (bss->nl_preq == NULL)
		return -1;
	wpa_printf(MSG_DEBUG, "nl80211: Enable Probe Request "
		   "reporting nl_preq=%p", bss->nl_preq);

	if (nl80211_register_frame(bss, bss->nl_preq,
				   (WLAN_FC_TYPE_MGMT << 2) |
				   (WLAN_FC_STYPE_PROBE_REQ << 4),
				   NULL, 0) < 0)
		goto out_err;

	eloop_register_read_sock(nl_socket_get_fd(bss->nl_preq),
				 wpa_driver_nl80211_event_receive, bss->nl_cb,
				 bss->nl_preq);

	return 0;

 out_err:
	nl_destroy_handles(&bss->nl_preq);
	return -1;
}


static int nl80211_disable_11b_rates(struct wpa_driver_nl80211_data *drv,
				     int ifindex, int disabled)
{
	struct nl_msg *msg;
	struct nlattr *bands, *band;
	int ret;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_TX_BITRATE_MASK);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifindex);

	bands = nla_nest_start(msg, NL80211_ATTR_TX_RATES);
	if (!bands)
		goto nla_put_failure;

	/*
	 * Disable 2 GHz rates 1, 2, 5.5, 11 Mbps by masking out everything
	 * else apart from 6, 9, 12, 18, 24, 36, 48, 54 Mbps from non-MCS
	 * rates. All 5 GHz rates are left enabled.
	 */
	band = nla_nest_start(msg, NL80211_BAND_2GHZ);
	if (!band)
		goto nla_put_failure;
	if (disabled) {
		NLA_PUT(msg, NL80211_TXRATE_LEGACY, 8,
			"\x0c\x12\x18\x24\x30\x48\x60\x6c");
	}
	nla_nest_end(msg, band);

	nla_nest_end(msg, bands);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Set TX rates failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
	} else
		drv->disabled_11b_rates = disabled;

	return ret;

nla_put_failure:
	nlmsg_free(msg);
	return -1;
}


static int wpa_driver_nl80211_deinit_ap(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (!is_ap_interface(drv->nlmode))
		return -1;
	wpa_driver_nl80211_del_beacon(drv);
	return wpa_driver_nl80211_set_mode(priv, NL80211_IFTYPE_STATION);
}


static int wpa_driver_nl80211_deinit_p2p_cli(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (drv->nlmode != NL80211_IFTYPE_P2P_CLIENT)
		return -1;
	return wpa_driver_nl80211_set_mode(priv, NL80211_IFTYPE_STATION);
}


static void wpa_driver_nl80211_resume(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1)) {
		wpa_printf(MSG_DEBUG, "nl80211: Failed to set interface up on "
			   "resume event");
	}
}


static int nl80211_send_ft_action(void *priv, u8 action, const u8 *target_ap,
				  const u8 *ies, size_t ies_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret;
	u8 *data, *pos;
	size_t data_len;
	const u8 *own_addr = bss->addr;

	if (action != 1) {
		wpa_printf(MSG_ERROR, "nl80211: Unsupported send_ft_action "
			   "action %d", action);
		return -1;
	}

	/*
	 * Action frame payload:
	 * Category[1] = 6 (Fast BSS Transition)
	 * Action[1] = 1 (Fast BSS Transition Request)
	 * STA Address
	 * Target AP Address
	 * FT IEs
	 */

	data_len = 2 + 2 * ETH_ALEN + ies_len;
	data = os_malloc(data_len);
	if (data == NULL)
		return -1;
	pos = data;
	*pos++ = 0x06; /* FT Action category */
	*pos++ = action;
	os_memcpy(pos, own_addr, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, target_ap, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, ies, ies_len);

	ret = wpa_driver_nl80211_send_action(bss, drv->assoc_freq, 0,
					     drv->bssid, own_addr, drv->bssid,
					     data, data_len, 0);
	os_free(data);

	return ret;
}


static int nl80211_signal_monitor(void *priv, int threshold, int hysteresis)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg, *cqm = NULL;
	int ret = -1;

	wpa_printf(MSG_DEBUG, "nl80211: Signal monitor threshold=%d "
		   "hysteresis=%d", threshold, hysteresis);

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_CQM);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);

	cqm = nlmsg_alloc();
	if (cqm == NULL)
		goto nla_put_failure;

	NLA_PUT_U32(cqm, NL80211_ATTR_CQM_RSSI_THOLD, threshold);
	NLA_PUT_U32(cqm, NL80211_ATTR_CQM_RSSI_HYST, hysteresis);
	if (nla_put_nested(msg, NL80211_ATTR_CQM, cqm) < 0)
		goto nla_put_failure;

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;

nla_put_failure:
	nlmsg_free(cqm);
	nlmsg_free(msg);
	return ret;
}


static int nl80211_signal_poll(void *priv, struct wpa_signal_info *si)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int res;

	os_memset(si, 0, sizeof(*si));
	res = nl80211_get_link_signal(drv, si);
	if (res != 0)
		return res;

	return nl80211_get_link_noise(drv, si);
}


static int wpa_driver_nl80211_shared_freq(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct wpa_driver_nl80211_data *driver;
	int freq = 0;

	/*
	 * If the same PHY is in connected state with some other interface,
	 * then retrieve the assoc freq.
	 */
	wpa_printf(MSG_DEBUG, "nl80211: Get shared freq for PHY %s",
		   drv->phyname);

	dl_list_for_each(driver, &drv->global->interfaces,
			 struct wpa_driver_nl80211_data, list) {
		if (drv == driver ||
		    os_strcmp(drv->phyname, driver->phyname) != 0 ||
		    !driver->associated)
			continue;

		wpa_printf(MSG_DEBUG, "nl80211: Found a match for PHY %s - %s "
			   MACSTR,
			   driver->phyname, driver->first_bss.ifname,
			   MAC2STR(driver->first_bss.addr));
		if (is_ap_interface(driver->nlmode))
			freq = driver->first_bss.freq;
		else
			freq = nl80211_get_assoc_freq(driver);
		wpa_printf(MSG_DEBUG, "nl80211: Shared freq for PHY %s: %d",
			   drv->phyname, freq);
	}

	if (!freq)
		wpa_printf(MSG_DEBUG, "nl80211: No shared interface for "
			   "PHY (%s) in associated state", drv->phyname);

	return freq;
}


static int nl80211_send_frame(void *priv, const u8 *data, size_t data_len,
			      int encrypt)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_send_frame(bss, data, data_len, encrypt, 0,
					     0, 0, 0, 0);
}


static int nl80211_set_param(void *priv, const char *param)
{
	wpa_printf(MSG_DEBUG, "nl80211: driver param='%s'", param);
	if (param == NULL)
		return 0;

#ifdef CONFIG_P2P
	if (os_strstr(param, "use_p2p_group_interface=1")) {
		struct i802_bss *bss = priv;
		struct wpa_driver_nl80211_data *drv = bss->drv;

		wpa_printf(MSG_DEBUG, "nl80211: Use separate P2P group "
			   "interface");
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_CONCURRENT;
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_MGMT_AND_NON_P2P;
	}
#endif /* CONFIG_P2P */

	return 0;
}


static void * nl80211_global_init(void)
{
	struct nl80211_global *global;
	struct netlink_config *cfg;

	global = os_zalloc(sizeof(*global));
	if (global == NULL)
		return NULL;
	global->ioctl_sock = -1;
	dl_list_init(&global->interfaces);
	global->if_add_ifindex = -1;

	cfg = os_zalloc(sizeof(*cfg));
	if (cfg == NULL)
		goto err;

	cfg->ctx = global;
	cfg->newlink_cb = wpa_driver_nl80211_event_rtm_newlink;
	cfg->dellink_cb = wpa_driver_nl80211_event_rtm_dellink;
	global->netlink = netlink_init(cfg);
	if (global->netlink == NULL) {
		os_free(cfg);
		goto err;
	}

	if (wpa_driver_nl80211_init_nl_global(global) < 0)
		goto err;

	global->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (global->ioctl_sock < 0) {
		perror("socket(PF_INET,SOCK_DGRAM)");
		goto err;
	}

	return global;

err:
	nl80211_global_deinit(global);
	return NULL;
}


static void nl80211_global_deinit(void *priv)
{
	struct nl80211_global *global = priv;
	if (global == NULL)
		return;
	if (!dl_list_empty(&global->interfaces)) {
		wpa_printf(MSG_ERROR, "nl80211: %u interface(s) remain at "
			   "nl80211_global_deinit",
			   dl_list_len(&global->interfaces));
	}

	if (global->netlink)
		netlink_deinit(global->netlink);

	nl_destroy_handles(&global->nl);

	if (global->nl_event) {
		eloop_unregister_read_sock(
			nl_socket_get_fd(global->nl_event));
		nl_destroy_handles(&global->nl_event);
	}

	nl_cb_put(global->nl_cb);

	if (global->ioctl_sock >= 0)
		close(global->ioctl_sock);

	os_free(global);
}


static const char * nl80211_get_radio_name(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	return drv->phyname;
}


static int nl80211_pmkid(struct i802_bss *bss, int cmd, const u8 *bssid,
			 const u8 *pmkid)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(bss->drv, msg, 0, cmd);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(bss->ifname));
	if (pmkid)
		NLA_PUT(msg, NL80211_ATTR_PMKID, 16, pmkid);
	if (bssid)
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, bssid);

	return send_and_recv_msgs(bss->drv, msg, NULL, NULL);
 nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int nl80211_add_pmkid(void *priv, const u8 *bssid, const u8 *pmkid)
{
	struct i802_bss *bss = priv;
	wpa_printf(MSG_DEBUG, "nl80211: Add PMKID for " MACSTR, MAC2STR(bssid));
	return nl80211_pmkid(bss, NL80211_CMD_SET_PMKSA, bssid, pmkid);
}


static int nl80211_remove_pmkid(void *priv, const u8 *bssid, const u8 *pmkid)
{
	struct i802_bss *bss = priv;
	wpa_printf(MSG_DEBUG, "nl80211: Delete PMKID for " MACSTR,
		   MAC2STR(bssid));
	return nl80211_pmkid(bss, NL80211_CMD_DEL_PMKSA, bssid, pmkid);
}


static int nl80211_flush_pmkid(void *priv)
{
	struct i802_bss *bss = priv;
	wpa_printf(MSG_DEBUG, "nl80211: Flush PMKIDs");
	return nl80211_pmkid(bss, NL80211_CMD_FLUSH_PMKSA, NULL, NULL);
}


static void nl80211_set_rekey_info(void *priv, const u8 *kek, const u8 *kck,
				   const u8 *replay_ctr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nlattr *replay_nested;
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_SET_REKEY_OFFLOAD);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);

	replay_nested = nla_nest_start(msg, NL80211_ATTR_REKEY_DATA);
	if (!replay_nested)
		goto nla_put_failure;

	NLA_PUT(msg, NL80211_REKEY_DATA_KEK, NL80211_KEK_LEN, kek);
	NLA_PUT(msg, NL80211_REKEY_DATA_KCK, NL80211_KCK_LEN, kck);
	NLA_PUT(msg, NL80211_REKEY_DATA_REPLAY_CTR, NL80211_REPLAY_CTR_LEN,
		replay_ctr);

	nla_nest_end(msg, replay_nested);

	send_and_recv_msgs(drv, msg, NULL, NULL);
	return;
 nla_put_failure:
	nlmsg_free(msg);
}


static void nl80211_send_null_frame(struct i802_bss *bss, const u8 *own_addr,
				    const u8 *addr, int qos)
{
	/* send data frame to poll STA and check whether
	 * this frame is ACKed */
	struct {
		struct ieee80211_hdr hdr;
		u16 qos_ctl;
	} STRUCT_PACKED nulldata;
	size_t size;

	/* Send data frame to poll STA and check whether this frame is ACKed */

	os_memset(&nulldata, 0, sizeof(nulldata));

	if (qos) {
		nulldata.hdr.frame_control =
			IEEE80211_FC(WLAN_FC_TYPE_DATA,
				     WLAN_FC_STYPE_QOS_NULL);
		size = sizeof(nulldata);
	} else {
		nulldata.hdr.frame_control =
			IEEE80211_FC(WLAN_FC_TYPE_DATA,
				     WLAN_FC_STYPE_NULLFUNC);
		size = sizeof(struct ieee80211_hdr);
	}

	nulldata.hdr.frame_control |= host_to_le16(WLAN_FC_FROMDS);
	os_memcpy(nulldata.hdr.IEEE80211_DA_FROMDS, addr, ETH_ALEN);
	os_memcpy(nulldata.hdr.IEEE80211_BSSID_FROMDS, own_addr, ETH_ALEN);
	os_memcpy(nulldata.hdr.IEEE80211_SA_FROMDS, own_addr, ETH_ALEN);

	if (wpa_driver_nl80211_send_mlme(bss, (u8 *) &nulldata, size, 0) < 0)
		wpa_printf(MSG_DEBUG, "nl80211_send_null_frame: Failed to "
			   "send poll frame");
}

static void nl80211_poll_client(void *priv, const u8 *own_addr, const u8 *addr,
				int qos)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	if (!drv->poll_command_supported) {
		nl80211_send_null_frame(bss, own_addr, addr, qos);
		return;
	}

	msg = nlmsg_alloc();
	if (!msg)
		return;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_PROBE_CLIENT);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);

	send_and_recv_msgs(drv, msg, NULL, NULL);
	return;
 nla_put_failure:
	nlmsg_free(msg);
}


static int nl80211_set_power_save(struct i802_bss *bss, int enabled)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(bss->drv, msg, 0, NL80211_CMD_SET_POWER_SAVE);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_PS_STATE,
		    enabled ? NL80211_PS_ENABLED : NL80211_PS_DISABLED);
	return send_and_recv_msgs(bss->drv, msg, NULL, NULL);
nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int nl80211_set_p2p_powersave(void *priv, int legacy_ps, int opp_ps,
				     int ctwindow)
{
	struct i802_bss *bss = priv;

	wpa_printf(MSG_DEBUG, "nl80211: set_p2p_powersave (legacy_ps=%d "
		   "opp_ps=%d ctwindow=%d)", legacy_ps, opp_ps, ctwindow);

	if (opp_ps != -1 || ctwindow != -1)
		return -1; /* Not yet supported */

	if (legacy_ps == -1)
		return 0;
	if (legacy_ps != 0 && legacy_ps != 1)
		return -1; /* Not yet supported */

	return nl80211_set_power_save(bss, legacy_ps);
}


#ifdef CONFIG_TDLS

static int nl80211_send_tdls_mgmt(void *priv, const u8 *dst, u8 action_code,
				  u8 dialog_token, u16 status_code,
				  const u8 *buf, size_t len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT))
		return -EOPNOTSUPP;

	if (!dst)
		return -EINVAL;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_TDLS_MGMT);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, dst);
	NLA_PUT_U8(msg, NL80211_ATTR_TDLS_ACTION, action_code);
	NLA_PUT_U8(msg, NL80211_ATTR_TDLS_DIALOG_TOKEN, dialog_token);
	NLA_PUT_U16(msg, NL80211_ATTR_STATUS_CODE, status_code);
	NLA_PUT(msg, NL80211_ATTR_IE, len, buf);

	return send_and_recv_msgs(drv, msg, NULL, NULL);

nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int nl80211_tdls_oper(void *priv, enum tdls_oper oper, const u8 *peer)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	enum nl80211_tdls_operation nl80211_oper;

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT))
		return -EOPNOTSUPP;

	switch (oper) {
	case TDLS_DISCOVERY_REQ:
		nl80211_oper = NL80211_TDLS_DISCOVERY_REQ;
		break;
	case TDLS_SETUP:
		nl80211_oper = NL80211_TDLS_SETUP;
		break;
	case TDLS_TEARDOWN:
		nl80211_oper = NL80211_TDLS_TEARDOWN;
		break;
	case TDLS_ENABLE_LINK:
		nl80211_oper = NL80211_TDLS_ENABLE_LINK;
		break;
	case TDLS_DISABLE_LINK:
		nl80211_oper = NL80211_TDLS_DISABLE_LINK;
		break;
	case TDLS_ENABLE:
		return 0;
	case TDLS_DISABLE:
		return 0;
	default:
		return -EINVAL;
	}

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_TDLS_OPER);
	NLA_PUT_U8(msg, NL80211_ATTR_TDLS_OPERATION, nl80211_oper);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, peer);

	return send_and_recv_msgs(drv, msg, NULL, NULL);

nla_put_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}

#endif /* CONFIG TDLS */


#ifdef ANDROID

typedef struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;

static int drv_errors = 0;

static void wpa_driver_send_hang_msg(struct wpa_driver_nl80211_data *drv)
{
	drv_errors++;
	if (drv_errors > DRV_NUMBER_SEQUENTIAL_ERRORS) {
		drv_errors = 0;
		wpa_msg(drv->ctx, MSG_INFO, WPA_EVENT_DRIVER_STATE "HANGED");
	}
}


static int android_priv_cmd(struct i802_bss *bss, const char *cmd)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ifreq ifr;
	android_wifi_priv_cmd priv_cmd;
	char buf[MAX_DRV_CMD_SIZE];
	int ret;

	os_memset(&ifr, 0, sizeof(ifr));
	os_memset(&priv_cmd, 0, sizeof(priv_cmd));
	os_strlcpy(ifr.ifr_name, bss->ifname, IFNAMSIZ);

	os_memset(buf, 0, sizeof(buf));
	os_strlcpy(buf, cmd, sizeof(buf));

	priv_cmd.buf = buf;
	priv_cmd.used_len = sizeof(buf);
	priv_cmd.total_len = sizeof(buf);
	ifr.ifr_data = &priv_cmd;

	ret = ioctl(drv->global->ioctl_sock, SIOCDEVPRIVATE + 1, &ifr);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "%s: failed to issue private commands",
			   __func__);
		wpa_driver_send_hang_msg(drv);
		return ret;
	}

	drv_errors = 0;
	return 0;
}


static int android_pno_start(struct i802_bss *bss,
			     struct wpa_driver_scan_params *params)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ifreq ifr;
	android_wifi_priv_cmd priv_cmd;
	int ret = 0, i = 0, bp;
	char buf[WEXT_PNO_MAX_COMMAND_SIZE];

	bp = WEXT_PNOSETUP_HEADER_SIZE;
	os_memcpy(buf, WEXT_PNOSETUP_HEADER, bp);
	buf[bp++] = WEXT_PNO_TLV_PREFIX;
	buf[bp++] = WEXT_PNO_TLV_VERSION;
	buf[bp++] = WEXT_PNO_TLV_SUBVERSION;
	buf[bp++] = WEXT_PNO_TLV_RESERVED;

	while (i < WEXT_PNO_AMOUNT && (size_t) i < params->num_ssids) {
		/* Check that there is enough space needed for 1 more SSID, the
		 * other sections and null termination */
		if ((bp + WEXT_PNO_SSID_HEADER_SIZE + MAX_SSID_LEN +
		     WEXT_PNO_NONSSID_SECTIONS_SIZE + 1) >= (int) sizeof(buf))
			break;
		wpa_hexdump_ascii(MSG_DEBUG, "For PNO Scan",
				  params->ssids[i].ssid,
				  params->ssids[i].ssid_len);
		buf[bp++] = WEXT_PNO_SSID_SECTION;
		buf[bp++] = params->ssids[i].ssid_len;
		os_memcpy(&buf[bp], params->ssids[i].ssid,
			  params->ssids[i].ssid_len);
		bp += params->ssids[i].ssid_len;
		i++;
	}

	buf[bp++] = WEXT_PNO_SCAN_INTERVAL_SECTION;
	os_snprintf(&buf[bp], WEXT_PNO_SCAN_INTERVAL_LENGTH + 1, "%x",
		    WEXT_PNO_SCAN_INTERVAL);
	bp += WEXT_PNO_SCAN_INTERVAL_LENGTH;

	buf[bp++] = WEXT_PNO_REPEAT_SECTION;
	os_snprintf(&buf[bp], WEXT_PNO_REPEAT_LENGTH + 1, "%x",
		    WEXT_PNO_REPEAT);
	bp += WEXT_PNO_REPEAT_LENGTH;

	buf[bp++] = WEXT_PNO_MAX_REPEAT_SECTION;
	os_snprintf(&buf[bp], WEXT_PNO_MAX_REPEAT_LENGTH + 1, "%x",
		    WEXT_PNO_MAX_REPEAT);
	bp += WEXT_PNO_MAX_REPEAT_LENGTH + 1;

	memset(&ifr, 0, sizeof(ifr));
	memset(&priv_cmd, 0, sizeof(priv_cmd));
	os_strncpy(ifr.ifr_name, bss->ifname, IFNAMSIZ);

	priv_cmd.buf = buf;
	priv_cmd.used_len = bp;
	priv_cmd.total_len = bp;
	ifr.ifr_data = &priv_cmd;

	ret = ioctl(drv->global->ioctl_sock, SIOCDEVPRIVATE + 1, &ifr);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "ioctl[SIOCSIWPRIV] (pnosetup): %d",
			   ret);
		wpa_driver_send_hang_msg(drv);
		return ret;
	}

	drv_errors = 0;

	return android_priv_cmd(bss, "PNOFORCE 1");
}


static int android_pno_stop(struct i802_bss *bss)
{
	return android_priv_cmd(bss, "PNOFORCE 0");
}

#endif /* ANDROID */


const struct wpa_driver_ops wpa_driver_nl80211_ops = {
	.name = "nl80211",
	.desc = "Linux nl80211/cfg80211",
	.get_bssid = wpa_driver_nl80211_get_bssid,
	.get_ssid = wpa_driver_nl80211_get_ssid,
	.set_key = wpa_driver_nl80211_set_key,
	.scan2 = wpa_driver_nl80211_scan,
	.sched_scan = wpa_driver_nl80211_sched_scan,
	.stop_sched_scan = wpa_driver_nl80211_stop_sched_scan,
	.get_scan_results2 = wpa_driver_nl80211_get_scan_results,
	.deauthenticate = wpa_driver_nl80211_deauthenticate,
	.authenticate = wpa_driver_nl80211_authenticate,
	.associate = wpa_driver_nl80211_associate,
	.global_init = nl80211_global_init,
	.global_deinit = nl80211_global_deinit,
	.init2 = wpa_driver_nl80211_init,
	.deinit = wpa_driver_nl80211_deinit,
	.get_capa = wpa_driver_nl80211_get_capa,
	.set_operstate = wpa_driver_nl80211_set_operstate,
	.set_supp_port = wpa_driver_nl80211_set_supp_port,
	.set_country = wpa_driver_nl80211_set_country,
	.set_ap = wpa_driver_nl80211_set_ap,
	.if_add = wpa_driver_nl80211_if_add,
	.if_remove = wpa_driver_nl80211_if_remove,
	.send_mlme = wpa_driver_nl80211_send_mlme,
	.get_hw_feature_data = wpa_driver_nl80211_get_hw_feature_data,
	.sta_add = wpa_driver_nl80211_sta_add,
	.sta_remove = wpa_driver_nl80211_sta_remove,
	.hapd_send_eapol = wpa_driver_nl80211_hapd_send_eapol,
	.sta_set_flags = wpa_driver_nl80211_sta_set_flags,
#ifdef HOSTAPD
	.hapd_init = i802_init,
	.hapd_deinit = i802_deinit,
	.set_wds_sta = i802_set_wds_sta,
#endif /* HOSTAPD */
#if defined(HOSTAPD) || defined(CONFIG_AP)
	.get_seqnum = i802_get_seqnum,
	.flush = i802_flush,
	.get_inact_sec = i802_get_inact_sec,
	.sta_clear_stats = i802_sta_clear_stats,
	.set_rts = i802_set_rts,
	.set_frag = i802_set_frag,
	.set_tx_queue_params = i802_set_tx_queue_params,
	.set_sta_vlan = i802_set_sta_vlan,
	.sta_deauth = i802_sta_deauth,
	.sta_disassoc = i802_sta_disassoc,
#endif /* HOSTAPD || CONFIG_AP */
	.read_sta_data = i802_read_sta_data,
	.set_freq = i802_set_freq,
	.send_action = wpa_driver_nl80211_send_action,
	.send_action_cancel_wait = wpa_driver_nl80211_send_action_cancel_wait,
	.remain_on_channel = wpa_driver_nl80211_remain_on_channel,
	.cancel_remain_on_channel =
	wpa_driver_nl80211_cancel_remain_on_channel,
	.probe_req_report = wpa_driver_nl80211_probe_req_report,
	.deinit_ap = wpa_driver_nl80211_deinit_ap,
	.deinit_p2p_cli = wpa_driver_nl80211_deinit_p2p_cli,
	.resume = wpa_driver_nl80211_resume,
	.send_ft_action = nl80211_send_ft_action,
	.signal_monitor = nl80211_signal_monitor,
	.signal_poll = nl80211_signal_poll,
	.send_frame = nl80211_send_frame,
	.shared_freq = wpa_driver_nl80211_shared_freq,
	.set_param = nl80211_set_param,
	.get_radio_name = nl80211_get_radio_name,
	.add_pmkid = nl80211_add_pmkid,
	.remove_pmkid = nl80211_remove_pmkid,
	.flush_pmkid = nl80211_flush_pmkid,
	.set_rekey_info = nl80211_set_rekey_info,
	.poll_client = nl80211_poll_client,
	.set_p2p_powersave = nl80211_set_p2p_powersave,
#ifdef CONFIG_TDLS
	.send_tdls_mgmt = nl80211_send_tdls_mgmt,
	.tdls_oper = nl80211_tdls_oper,
#endif /* CONFIG_TDLS */
};
