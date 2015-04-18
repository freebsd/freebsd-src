/*
 * Driver interaction with Linux nl80211/cfg80211
 * Copyright (c) 2002-2015, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/types.h>
#include <fcntl.h>
#include <net/if.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#ifdef CONFIG_LIBNL3_ROUTE
#include <netlink/route/neighbour.h>
#endif /* CONFIG_LIBNL3_ROUTE */
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/errqueue.h>

#include "common.h"
#include "eloop.h"
#include "common/qca-vendor.h"
#include "common/qca-vendor-attr.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "l2_packet/l2_packet.h"
#include "netlink.h"
#include "linux_defines.h"
#include "linux_ioctl.h"
#include "radiotap.h"
#include "radiotap_iter.h"
#include "rfkill.h"
#include "driver_nl80211.h"


#ifndef CONFIG_LIBNL20
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


#ifdef ANDROID
/* system/core/libnl_2 does not include nl_socket_set_nonblocking() */
#undef nl_socket_set_nonblocking
#define nl_socket_set_nonblocking(h) android_nl_socket_set_nonblocking(h)

#define genl_ctrl_resolve android_genl_ctrl_resolve
#endif /* ANDROID */


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


#if __WORDSIZE == 64
#define ELOOP_SOCKET_INVALID	(intptr_t) 0x8888888888888889ULL
#else
#define ELOOP_SOCKET_INVALID	(intptr_t) 0x88888889ULL
#endif

static void nl80211_register_eloop_read(struct nl_handle **handle,
					eloop_sock_handler handler,
					void *eloop_data)
{
#ifdef CONFIG_LIBNL20
	/*
	 * libnl uses a pretty small buffer (32 kB that gets converted to 64 kB)
	 * by default. It is possible to hit that limit in some cases where
	 * operations are blocked, e.g., with a burst of Deauthentication frames
	 * to hostapd and STA entry deletion. Try to increase the buffer to make
	 * this less likely to occur.
	 */
	if (nl_socket_set_buffer_size(*handle, 262144, 0) < 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Could not set nl_socket RX buffer size: %s",
			   strerror(errno));
		/* continue anyway with the default (smaller) buffer */
	}
#endif /* CONFIG_LIBNL20 */

	nl_socket_set_nonblocking(*handle);
	eloop_register_read_sock(nl_socket_get_fd(*handle), handler,
				 eloop_data, *handle);
	*handle = (void *) (((intptr_t) *handle) ^ ELOOP_SOCKET_INVALID);
}


static void nl80211_destroy_eloop_handle(struct nl_handle **handle)
{
	*handle = (void *) (((intptr_t) *handle) ^ ELOOP_SOCKET_INVALID);
	eloop_unregister_read_sock(nl_socket_get_fd(*handle));
	nl_destroy_handles(handle);
}


static void nl80211_global_deinit(void *priv);
static void nl80211_check_global(struct nl80211_global *global);

static void wpa_driver_nl80211_deinit(struct i802_bss *bss);
static int wpa_driver_nl80211_set_mode_ibss(struct i802_bss *bss,
					    struct hostapd_freq_params *freq);

static int
wpa_driver_nl80211_finish_drv_init(struct wpa_driver_nl80211_data *drv,
				   const u8 *set_addr, int first,
				   const char *driver_params);
static int nl80211_send_frame_cmd(struct i802_bss *bss,
				  unsigned int freq, unsigned int wait,
				  const u8 *buf, size_t buf_len, u64 *cookie,
				  int no_cck, int no_ack, int offchanok);
static int wpa_driver_nl80211_probe_req_report(struct i802_bss *bss,
					       int report);

static void add_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx);
static void del_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx);
static int have_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx);

static int nl80211_set_channel(struct i802_bss *bss,
			       struct hostapd_freq_params *freq, int set_chan);
static int nl80211_disable_11b_rates(struct wpa_driver_nl80211_data *drv,
				     int ifindex, int disabled);

static int nl80211_leave_ibss(struct wpa_driver_nl80211_data *drv,
			      int reset_mode);

static int i802_set_iface_flags(struct i802_bss *bss, int up);
static int nl80211_set_param(void *priv, const char *param);


/* Converts nl80211_chan_width to a common format */
enum chan_width convert2width(int width)
{
	switch (width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
		return CHAN_WIDTH_20_NOHT;
	case NL80211_CHAN_WIDTH_20:
		return CHAN_WIDTH_20;
	case NL80211_CHAN_WIDTH_40:
		return CHAN_WIDTH_40;
	case NL80211_CHAN_WIDTH_80:
		return CHAN_WIDTH_80;
	case NL80211_CHAN_WIDTH_80P80:
		return CHAN_WIDTH_80P80;
	case NL80211_CHAN_WIDTH_160:
		return CHAN_WIDTH_160;
	}
	return CHAN_WIDTH_UNKNOWN;
}


int is_ap_interface(enum nl80211_iftype nlmode)
{
	return nlmode == NL80211_IFTYPE_AP ||
		nlmode == NL80211_IFTYPE_P2P_GO;
}


int is_sta_interface(enum nl80211_iftype nlmode)
{
	return nlmode == NL80211_IFTYPE_STATION ||
		nlmode == NL80211_IFTYPE_P2P_CLIENT;
}


static int is_p2p_net_interface(enum nl80211_iftype nlmode)
{
	return nlmode == NL80211_IFTYPE_P2P_CLIENT ||
		nlmode == NL80211_IFTYPE_P2P_GO;
}


struct i802_bss * get_bss_ifindex(struct wpa_driver_nl80211_data *drv,
				  int ifindex)
{
	struct i802_bss *bss;

	for (bss = drv->first_bss; bss; bss = bss->next) {
		if (bss->ifindex == ifindex)
			return bss;
	}

	return NULL;
}


static int is_mesh_interface(enum nl80211_iftype nlmode)
{
	return nlmode == NL80211_IFTYPE_MESH_POINT;
}


void nl80211_mark_disconnected(struct wpa_driver_nl80211_data *drv)
{
	if (drv->associated)
		os_memcpy(drv->prev_bssid, drv->bssid, ETH_ALEN);
	drv->associated = 0;
	os_memset(drv->bssid, 0, ETH_ALEN);
}


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


static void nl80211_nlmsg_clear(struct nl_msg *msg)
{
	/*
	 * Clear nlmsg data, e.g., to make sure key material is not left in
	 * heap memory for unnecessarily long time.
	 */
	if (msg) {
		struct nlmsghdr *hdr = nlmsg_hdr(msg);
		void *data = nlmsg_data(hdr);
		/*
		 * This would use nlmsg_datalen() or the older nlmsg_len() if
		 * only libnl were to maintain a stable API.. Neither will work
		 * with all released versions, so just calculate the length
		 * here.
		 */
		int len = hdr->nlmsg_len - NLMSG_HDRLEN;

		os_memset(data, 0, len);
	}
}


static int send_and_recv(struct nl80211_global *global,
			 struct nl_handle *nl_handle, struct nl_msg *msg,
			 int (*valid_handler)(struct nl_msg *, void *),
			 void *valid_data)
{
	struct nl_cb *cb;
	int err = -ENOMEM;

	if (!msg)
		return -ENOMEM;

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

	while (err > 0) {
		int res = nl_recvmsgs(nl_handle, cb);
		if (res < 0) {
			wpa_printf(MSG_INFO,
				   "nl80211: %s->nl_recvmsgs failed: %d",
				   __func__, res);
		}
	}
 out:
	nl_cb_put(cb);
	if (!valid_handler && valid_data == (void *) -1)
		nl80211_nlmsg_clear(msg);
	nlmsg_free(msg);
	return err;
}


int send_and_recv_msgs(struct wpa_driver_nl80211_data *drv,
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
	int ret;
	struct family_data res = { group, -ENOENT };

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;
	if (!genlmsg_put(msg, 0, 0, genl_ctrl_resolve(global->nl, "nlctrl"),
			 0, 0, CTRL_CMD_GETFAMILY, 0) ||
	    nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family)) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv(global, global->nl, msg, family_handler, &res);
	if (ret == 0)
		ret = res.id;
	return ret;
}


void * nl80211_cmd(struct wpa_driver_nl80211_data *drv,
		   struct nl_msg *msg, int flags, uint8_t cmd)
{
	return genlmsg_put(msg, 0, 0, drv->global->nl80211_id,
			   0, flags, cmd, 0);
}


static int nl80211_set_iface_id(struct nl_msg *msg, struct i802_bss *bss)
{
	if (bss->wdev_id_set)
		return nla_put_u64(msg, NL80211_ATTR_WDEV, bss->wdev_id);
	return nla_put_u32(msg, NL80211_ATTR_IFINDEX, bss->ifindex);
}


struct nl_msg * nl80211_cmd_msg(struct i802_bss *bss, int flags, uint8_t cmd)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return NULL;

	if (!nl80211_cmd(bss->drv, msg, flags, cmd) ||
	    nl80211_set_iface_id(msg, bss) < 0) {
		nlmsg_free(msg);
		return NULL;
	}

	return msg;
}


static struct nl_msg *
nl80211_ifindex_msg(struct wpa_driver_nl80211_data *drv, int ifindex,
		    int flags, uint8_t cmd)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return NULL;

	if (!nl80211_cmd(drv, msg, flags, cmd) ||
	    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex)) {
		nlmsg_free(msg);
		return NULL;
	}

	return msg;
}


struct nl_msg * nl80211_drv_msg(struct wpa_driver_nl80211_data *drv, int flags,
				uint8_t cmd)
{
	return nl80211_ifindex_msg(drv, drv->ifindex, flags, cmd);
}


struct nl_msg * nl80211_bss_msg(struct i802_bss *bss, int flags, uint8_t cmd)
{
	return nl80211_ifindex_msg(bss->drv, bss->ifindex, flags, cmd);
}


struct wiphy_idx_data {
	int wiphy_idx;
	enum nl80211_iftype nlmode;
	u8 *macaddr;
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

	if (tb[NL80211_ATTR_IFTYPE])
		info->nlmode = nla_get_u32(tb[NL80211_ATTR_IFTYPE]);

	if (tb[NL80211_ATTR_MAC] && info->macaddr)
		os_memcpy(info->macaddr, nla_data(tb[NL80211_ATTR_MAC]),
			  ETH_ALEN);

	return NL_SKIP;
}


int nl80211_get_wiphy_index(struct i802_bss *bss)
{
	struct nl_msg *msg;
	struct wiphy_idx_data data = {
		.wiphy_idx = -1,
		.macaddr = NULL,
	};

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_GET_INTERFACE)))
		return -1;

	if (send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data) == 0)
		return data.wiphy_idx;
	return -1;
}


static enum nl80211_iftype nl80211_get_ifmode(struct i802_bss *bss)
{
	struct nl_msg *msg;
	struct wiphy_idx_data data = {
		.nlmode = NL80211_IFTYPE_UNSPECIFIED,
		.macaddr = NULL,
	};

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_GET_INTERFACE)))
		return NL80211_IFTYPE_UNSPECIFIED;

	if (send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data) == 0)
		return data.nlmode;
	return NL80211_IFTYPE_UNSPECIFIED;
}


static int nl80211_get_macaddr(struct i802_bss *bss)
{
	struct nl_msg *msg;
	struct wiphy_idx_data data = {
		.macaddr = bss->addr,
	};

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_GET_INTERFACE)))
		return -1;

	return send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data);
}


static int nl80211_register_beacons(struct wpa_driver_nl80211_data *drv,
				    struct nl80211_wiphy_data *w)
{
	struct nl_msg *msg;
	int ret;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	if (!nl80211_cmd(drv, msg, 0, NL80211_CMD_REGISTER_BEACONS) ||
	    nla_put_u32(msg, NL80211_ATTR_WIPHY, w->wiphy_idx)) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv(drv->global, w->nl_beacons, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Register beacons command "
			   "failed: ret=%d (%s)",
			   ret, strerror(-ret));
	}
	return ret;
}


static void nl80211_recv_beacons(int sock, void *eloop_ctx, void *handle)
{
	struct nl80211_wiphy_data *w = eloop_ctx;
	int res;

	wpa_printf(MSG_EXCESSIVE, "nl80211: Beacon event message available");

	res = nl_recvmsgs(handle, w->nl_cb);
	if (res < 0) {
		wpa_printf(MSG_INFO, "nl80211: %s->nl_recvmsgs failed: %d",
			   __func__, res);
	}
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

	nl80211_register_eloop_read(&w->nl_beacons, nl80211_recv_beacons, w);

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

	nl80211_destroy_eloop_handle(&w->nl_beacons);

	nl_cb_put(w->nl_cb);
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


static void wpa_driver_nl80211_event_newlink(
	struct wpa_driver_nl80211_data *drv, const char *ifname)
{
	union wpa_event_data event;

	if (os_strcmp(drv->first_bss->ifname, ifname) == 0) {
		if (if_nametoindex(drv->first_bss->ifname) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Interface %s does not exist - ignore RTM_NEWLINK",
				   drv->first_bss->ifname);
			return;
		}
		if (!drv->if_removed)
			return;
		wpa_printf(MSG_DEBUG, "nl80211: Mark if_removed=0 for %s based on RTM_NEWLINK event",
			   drv->first_bss->ifname);
		drv->if_removed = 0;
	}

	os_memset(&event, 0, sizeof(event));
	os_strlcpy(event.interface_status.ifname, ifname,
		   sizeof(event.interface_status.ifname));
	event.interface_status.ievent = EVENT_INTERFACE_ADDED;
	wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS, &event);
}


static void wpa_driver_nl80211_event_dellink(
	struct wpa_driver_nl80211_data *drv, const char *ifname)
{
	union wpa_event_data event;

	if (os_strcmp(drv->first_bss->ifname, ifname) == 0) {
		if (drv->if_removed) {
			wpa_printf(MSG_DEBUG, "nl80211: if_removed already set - ignore RTM_DELLINK event for %s",
				   ifname);
			return;
		}
		wpa_printf(MSG_DEBUG, "RTM_DELLINK: Interface '%s' removed - mark if_removed=1",
			   ifname);
		drv->if_removed = 1;
	} else {
		wpa_printf(MSG_DEBUG, "RTM_DELLINK: Interface '%s' removed",
			   ifname);
	}

	os_memset(&event, 0, sizeof(event));
	os_strlcpy(event.interface_status.ifname, ifname,
		   sizeof(event.interface_status.ifname));
	event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
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
			if (os_strcmp(((char *) attr) + rta_len,
				      drv->first_bss->ifname) == 0)
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
		nl80211_check_global(drv->global);
		wpa_printf(MSG_DEBUG, "nl80211: Update ifindex for a removed "
			   "interface");
		wpa_driver_nl80211_finish_drv_init(drv, NULL, 0, NULL);
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
	int attrlen;
	struct rtattr *attr;
	u32 brid = 0;
	char namebuf[IFNAMSIZ];
	char ifname[IFNAMSIZ + 1];
	char extra[100], *pos, *end;

	drv = nl80211_find_drv(global, ifi->ifi_index, buf, len);
	if (!drv) {
		wpa_printf(MSG_DEBUG, "nl80211: Ignore RTM_NEWLINK event for foreign ifindex %d",
			   ifi->ifi_index);
		return;
	}

	extra[0] = '\0';
	pos = extra;
	end = pos + sizeof(extra);
	ifname[0] = '\0';

	attrlen = len;
	attr = (struct rtattr *) buf;
	while (RTA_OK(attr, attrlen)) {
		switch (attr->rta_type) {
		case IFLA_IFNAME:
			if (RTA_PAYLOAD(attr) >= IFNAMSIZ)
				break;
			os_memcpy(ifname, RTA_DATA(attr), RTA_PAYLOAD(attr));
			ifname[RTA_PAYLOAD(attr)] = '\0';
			break;
		case IFLA_MASTER:
			brid = nla_get_u32((struct nlattr *) attr);
			pos += os_snprintf(pos, end - pos, " master=%u", brid);
			break;
		case IFLA_WIRELESS:
			pos += os_snprintf(pos, end - pos, " wext");
			break;
		case IFLA_OPERSTATE:
			pos += os_snprintf(pos, end - pos, " operstate=%u",
					   nla_get_u32((struct nlattr *) attr));
			break;
		case IFLA_LINKMODE:
			pos += os_snprintf(pos, end - pos, " linkmode=%u",
					   nla_get_u32((struct nlattr *) attr));
			break;
		}
		attr = RTA_NEXT(attr, attrlen);
	}
	extra[sizeof(extra) - 1] = '\0';

	wpa_printf(MSG_DEBUG, "RTM_NEWLINK: ifi_index=%d ifname=%s%s ifi_family=%d ifi_flags=0x%x (%s%s%s%s)",
		   ifi->ifi_index, ifname, extra, ifi->ifi_family,
		   ifi->ifi_flags,
		   (ifi->ifi_flags & IFF_UP) ? "[UP]" : "",
		   (ifi->ifi_flags & IFF_RUNNING) ? "[RUNNING]" : "",
		   (ifi->ifi_flags & IFF_LOWER_UP) ? "[LOWER_UP]" : "",
		   (ifi->ifi_flags & IFF_DORMANT) ? "[DORMANT]" : "");

	if (!drv->if_disabled && !(ifi->ifi_flags & IFF_UP)) {
		namebuf[0] = '\0';
		if (if_indextoname(ifi->ifi_index, namebuf) &&
		    linux_iface_up(drv->global->ioctl_sock, namebuf) > 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface down "
				   "event since interface %s is up", namebuf);
			drv->ignore_if_down_event = 0;
			return;
		}
		wpa_printf(MSG_DEBUG, "nl80211: Interface down (%s/%s)",
			   namebuf, ifname);
		if (os_strcmp(drv->first_bss->ifname, ifname) != 0) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Not the main interface (%s) - do not indicate interface down",
				   drv->first_bss->ifname);
		} else if (drv->ignore_if_down_event) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface down "
				   "event generated by mode change");
			drv->ignore_if_down_event = 0;
		} else {
			drv->if_disabled = 1;
			wpa_supplicant_event(drv->ctx,
					     EVENT_INTERFACE_DISABLED, NULL);

			/*
			 * Try to get drv again, since it may be removed as
			 * part of the EVENT_INTERFACE_DISABLED handling for
			 * dynamic interfaces
			 */
			drv = nl80211_find_drv(global, ifi->ifi_index,
					       buf, len);
			if (!drv)
				return;
		}
	}

	if (drv->if_disabled && (ifi->ifi_flags & IFF_UP)) {
		if (if_indextoname(ifi->ifi_index, namebuf) &&
		    linux_iface_up(drv->global->ioctl_sock, namebuf) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s is down",
				   namebuf);
		} else if (if_nametoindex(drv->first_bss->ifname) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s does not exist",
				   drv->first_bss->ifname);
		} else if (drv->if_removed) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s is marked "
				   "removed", drv->first_bss->ifname);
		} else {
			struct i802_bss *bss;
			u8 addr[ETH_ALEN];

			/* Re-read MAC address as it may have changed */
			bss = get_bss_ifindex(drv, ifi->ifi_index);
			if (bss &&
			    linux_get_ifhwaddr(drv->global->ioctl_sock,
					       bss->ifname, addr) < 0) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: %s: failed to re-read MAC address",
					   bss->ifname);
			} else if (bss &&
				   os_memcmp(addr, bss->addr, ETH_ALEN) != 0) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Own MAC address on ifindex %d (%s) changed from "
					   MACSTR " to " MACSTR,
					   ifi->ifi_index, bss->ifname,
					   MAC2STR(bss->addr),
					   MAC2STR(addr));
				os_memcpy(bss->addr, addr, ETH_ALEN);
			}

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
	    !(ifi->ifi_flags & IFF_RUNNING)) {
		wpa_printf(MSG_DEBUG, "nl80211: Set IF_OPER_UP again based on ifi_flags and expected operstate");
		netlink_send_oper_ifla(drv->global->netlink, drv->ifindex,
				       -1, IF_OPER_UP);
	}

	if (ifname[0])
		wpa_driver_nl80211_event_newlink(drv, ifname);

	if (ifi->ifi_family == AF_BRIDGE && brid) {
		struct i802_bss *bss;

		/* device has been added to bridge */
		if (!if_indextoname(brid, namebuf)) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Could not find bridge ifname for ifindex %u",
				   brid);
			return;
		}
		wpa_printf(MSG_DEBUG, "nl80211: Add ifindex %u for bridge %s",
			   brid, namebuf);
		add_ifidx(drv, brid);

		for (bss = drv->first_bss; bss; bss = bss->next) {
			if (os_strcmp(ifname, bss->ifname) == 0) {
				os_strlcpy(bss->brname, namebuf, IFNAMSIZ);
				break;
			}
		}
	}
}


static void wpa_driver_nl80211_event_rtm_dellink(void *ctx,
						 struct ifinfomsg *ifi,
						 u8 *buf, size_t len)
{
	struct nl80211_global *global = ctx;
	struct wpa_driver_nl80211_data *drv;
	int attrlen;
	struct rtattr *attr;
	u32 brid = 0;
	char ifname[IFNAMSIZ + 1];
	char extra[100], *pos, *end;

	drv = nl80211_find_drv(global, ifi->ifi_index, buf, len);
	if (!drv) {
		wpa_printf(MSG_DEBUG, "nl80211: Ignore RTM_DELLINK event for foreign ifindex %d",
			   ifi->ifi_index);
		return;
	}

	extra[0] = '\0';
	pos = extra;
	end = pos + sizeof(extra);
	ifname[0] = '\0';

	attrlen = len;
	attr = (struct rtattr *) buf;
	while (RTA_OK(attr, attrlen)) {
		switch (attr->rta_type) {
		case IFLA_IFNAME:
			if (RTA_PAYLOAD(attr) >= IFNAMSIZ)
				break;
			os_memcpy(ifname, RTA_DATA(attr), RTA_PAYLOAD(attr));
			ifname[RTA_PAYLOAD(attr)] = '\0';
			break;
		case IFLA_MASTER:
			brid = nla_get_u32((struct nlattr *) attr);
			pos += os_snprintf(pos, end - pos, " master=%u", brid);
			break;
		case IFLA_OPERSTATE:
			pos += os_snprintf(pos, end - pos, " operstate=%u",
					   nla_get_u32((struct nlattr *) attr));
			break;
		case IFLA_LINKMODE:
			pos += os_snprintf(pos, end - pos, " linkmode=%u",
					   nla_get_u32((struct nlattr *) attr));
			break;
		}
		attr = RTA_NEXT(attr, attrlen);
	}
	extra[sizeof(extra) - 1] = '\0';

	wpa_printf(MSG_DEBUG, "RTM_DELLINK: ifi_index=%d ifname=%s%s ifi_family=%d ifi_flags=0x%x (%s%s%s%s)",
		   ifi->ifi_index, ifname, extra, ifi->ifi_family,
		   ifi->ifi_flags,
		   (ifi->ifi_flags & IFF_UP) ? "[UP]" : "",
		   (ifi->ifi_flags & IFF_RUNNING) ? "[RUNNING]" : "",
		   (ifi->ifi_flags & IFF_LOWER_UP) ? "[LOWER_UP]" : "",
		   (ifi->ifi_flags & IFF_DORMANT) ? "[DORMANT]" : "");

	if (ifname[0] && (ifi->ifi_family != AF_BRIDGE || !brid))
		wpa_driver_nl80211_event_dellink(drv, ifname);

	if (ifi->ifi_family == AF_BRIDGE && brid) {
		/* device has been removed from bridge */
		char namebuf[IFNAMSIZ];

		if (!if_indextoname(brid, namebuf)) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Could not find bridge ifname for ifindex %u",
				   brid);
		} else {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Remove ifindex %u for bridge %s",
				   brid, namebuf);
		}
		del_ifidx(drv, brid);
	}
}


unsigned int nl80211_get_assoc_freq(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	int ret;
	struct nl80211_bss_info_arg arg;

	msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SCAN);
	os_memset(&arg, 0, sizeof(arg));
	arg.drv = drv;
	ret = send_and_recv_msgs(drv, msg, bss_info_handler, &arg);
	if (ret == 0) {
		unsigned int freq = drv->nlmode == NL80211_IFTYPE_ADHOC ?
			arg.ibss_freq : arg.assoc_freq;
		wpa_printf(MSG_DEBUG, "nl80211: Operating frequency for the "
			   "associated BSS from scan results: %u MHz", freq);
		if (freq)
			drv->assoc_freq = freq;
		return drv->assoc_freq;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Scan result fetch failed: ret=%d "
		   "(%s)", ret, strerror(-ret));
	return drv->assoc_freq;
}


static int get_link_signal(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
	static struct nla_policy policy[NL80211_STA_INFO_MAX + 1] = {
		[NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
		[NL80211_STA_INFO_SIGNAL_AVG] = { .type = NLA_U8 },
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

	if (sinfo[NL80211_STA_INFO_SIGNAL_AVG])
		sig_change->avg_signal =
			(s8) nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL_AVG]);
	else
		sig_change->avg_signal = 0;

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


int nl80211_get_link_signal(struct wpa_driver_nl80211_data *drv,
			    struct wpa_signal_info *sig)
{
	struct nl_msg *msg;

	sig->current_signal = -9999;
	sig->current_txrate = 0;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_GET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, drv->bssid)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(drv, msg, get_link_signal, sig);
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


int nl80211_get_link_noise(struct wpa_driver_nl80211_data *drv,
			   struct wpa_signal_info *sig_change)
{
	struct nl_msg *msg;

	sig_change->current_noise = 9999;
	sig_change->frequency = drv->assoc_freq;

	msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);
	return send_and_recv_msgs(drv, msg, get_link_noise, sig_change);
}


static void wpa_driver_nl80211_event_receive(int sock, void *eloop_ctx,
					     void *handle)
{
	struct nl_cb *cb = eloop_ctx;
	int res;

	wpa_printf(MSG_MSGDUMP, "nl80211: Event message available");

	res = nl_recvmsgs(handle, cb);
	if (res < 0) {
		wpa_printf(MSG_INFO, "nl80211: %s->nl_recvmsgs failed: %d",
			   __func__, res);
	}
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

	if (!nl80211_cmd(drv, msg, 0, NL80211_CMD_REQ_SET_REG) ||
	    nla_put_string(msg, NL80211_ATTR_REG_ALPHA2, alpha2)) {
		nlmsg_free(msg);
		return -EINVAL;
	}
	if (send_and_recv_msgs(drv, msg, NULL, NULL))
		return -EINVAL;
	return 0;
}


static int nl80211_get_country(struct nl_msg *msg, void *arg)
{
	char *alpha2 = arg;
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb_msg[NL80211_ATTR_REG_ALPHA2]) {
		wpa_printf(MSG_DEBUG, "nl80211: No country information available");
		return NL_SKIP;
	}
	os_strlcpy(alpha2, nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]), 3);
	return NL_SKIP;
}


static int wpa_driver_nl80211_get_country(void *priv, char *alpha2)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_REG);
	alpha2[0] = '\0';
	ret = send_and_recv_msgs(drv, msg, nl80211_get_country, alpha2);
	if (!alpha2[0])
		ret = -1;

	return ret;
}


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

	ret = nl_get_multicast_id(global, "nl80211", "vendor");
	if (ret >= 0)
		ret = nl_socket_add_membership(global->nl_event, ret);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Could not add multicast "
			   "membership for vendor events: %d (%s)",
			   ret, strerror(-ret));
		/* Continue without vendor events */
	}

	nl_cb_set(global->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  no_seq_check, NULL);
	nl_cb_set(global->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
		  process_global_event, global);

	nl80211_register_eloop_read(&global->nl_event,
				    wpa_driver_nl80211_event_receive,
				    global->nl_cb);

	return 0;

err:
	nl_destroy_handles(&global->nl_event);
	nl_destroy_handles(&global->nl);
	nl_cb_put(global->nl_cb);
	global->nl_cb = NULL;
	return -1;
}


static void nl80211_check_global(struct nl80211_global *global)
{
	struct nl_handle *handle;
	const char *groups[] = { "scan", "mlme", "regulatory", "vendor", NULL };
	int ret;
	unsigned int i;

	/*
	 * Try to re-add memberships to handle case of cfg80211 getting reloaded
	 * and all registration having been cleared.
	 */
	handle = (void *) (((intptr_t) global->nl_event) ^
			   ELOOP_SOCKET_INVALID);

	for (i = 0; groups[i]; i++) {
		ret = nl_get_multicast_id(global, "nl80211", groups[i]);
		if (ret >= 0)
			ret = nl_socket_add_membership(handle, ret);
		if (ret < 0) {
			wpa_printf(MSG_INFO,
				   "nl80211: Could not re-add multicast membership for %s events: %d (%s)",
				   groups[i], ret, strerror(-ret));
		}
	}
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
	if (i802_set_iface_flags(drv->first_bss, 1)) {
		wpa_printf(MSG_DEBUG, "nl80211: Could not set interface UP "
			   "after rfkill unblock");
		return;
	}
	/* rtnetlink ifup handler will report interface as enabled */
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


static void * wpa_driver_nl80211_drv_init(void *ctx, const char *ifname,
					  void *global_priv, int hostapd,
					  const u8 *set_addr,
					  const char *driver_params)
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
	drv->hostapd = !!hostapd;
	drv->eapol_sock = -1;

	/*
	 * There is no driver capability flag for this, so assume it is
	 * supported and disable this on first attempt to use if the driver
	 * rejects the command due to missing support.
	 */
	drv->set_rekey_offload = 1;

	drv->num_if_indices = sizeof(drv->default_if_indices) / sizeof(int);
	drv->if_indices = drv->default_if_indices;

	drv->first_bss = os_zalloc(sizeof(*drv->first_bss));
	if (!drv->first_bss) {
		os_free(drv);
		return NULL;
	}
	bss = drv->first_bss;
	bss->drv = drv;
	bss->ctx = ctx;

	os_strlcpy(bss->ifname, ifname, sizeof(bss->ifname));
	drv->monitor_ifidx = -1;
	drv->monitor_sock = -1;
	drv->eapol_tx_sock = -1;
	drv->ap_scan_as_station = NL80211_IFTYPE_UNSPECIFIED;

	if (nl80211_init_bss(bss))
		goto failed;

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

	if (linux_iface_up(drv->global->ioctl_sock, ifname) > 0)
		drv->start_iface_up = 1;

	if (wpa_driver_nl80211_finish_drv_init(drv, set_addr, 1, driver_params))
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
		nl80211_check_global(drv->global);
		dl_list_add(&drv->global->interfaces, &drv->list);
		drv->in_interface_list = 1;
	}

	return bss;

failed:
	wpa_driver_nl80211_deinit(bss);
	return NULL;
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
	return wpa_driver_nl80211_drv_init(ctx, ifname, global_priv, 0, NULL,
					   NULL);
}


static int nl80211_register_frame(struct i802_bss *bss,
				  struct nl_handle *nl_handle,
				  u16 type, const u8 *match, size_t match_len)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	char buf[30];

	buf[0] = '\0';
	wpa_snprintf_hex(buf, sizeof(buf), match, match_len);
	wpa_printf(MSG_DEBUG, "nl80211: Register frame type=0x%x (%s) nl_handle=%p match=%s",
		   type, fc2str(type), nl_handle, buf);

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_REGISTER_ACTION)) ||
	    nla_put_u16(msg, NL80211_ATTR_FRAME_TYPE, type) ||
	    nla_put(msg, NL80211_ATTR_FRAME_MATCH, match_len, match)) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv(drv->global, nl_handle, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Register frame command "
			   "failed (type=%u): ret=%d (%s)",
			   type, ret, strerror(-ret));
		wpa_hexdump(MSG_DEBUG, "nl80211: Register frame match",
			    match, match_len);
	}
	return ret;
}


static int nl80211_alloc_mgmt_handle(struct i802_bss *bss)
{
	if (bss->nl_mgmt) {
		wpa_printf(MSG_DEBUG, "nl80211: Mgmt reporting "
			   "already on! (nl_mgmt=%p)", bss->nl_mgmt);
		return -1;
	}

	bss->nl_mgmt = nl_create_handle(bss->nl_cb, "mgmt");
	if (bss->nl_mgmt == NULL)
		return -1;

	return 0;
}


static void nl80211_mgmt_handle_register_eloop(struct i802_bss *bss)
{
	nl80211_register_eloop_read(&bss->nl_mgmt,
				    wpa_driver_nl80211_event_receive,
				    bss->nl_cb);
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
	int ret = 0;

	if (nl80211_alloc_mgmt_handle(bss))
		return -1;
	wpa_printf(MSG_DEBUG, "nl80211: Subscribe to mgmt frames with non-AP "
		   "handle %p", bss->nl_mgmt);

	if (drv->nlmode == NL80211_IFTYPE_ADHOC) {
		u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4);

		/* register for any AUTH message */
		nl80211_register_frame(bss, bss->nl_mgmt, type, NULL, 0);
	}

#ifdef CONFIG_INTERWORKING
	/* QoS Map Configure */
	if (nl80211_register_action_frame(bss, (u8 *) "\x01\x04", 2) < 0)
		ret = -1;
#endif /* CONFIG_INTERWORKING */
#if defined(CONFIG_P2P) || defined(CONFIG_INTERWORKING)
	/* GAS Initial Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0a", 2) < 0)
		ret = -1;
	/* GAS Initial Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0b", 2) < 0)
		ret = -1;
	/* GAS Comeback Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0c", 2) < 0)
		ret = -1;
	/* GAS Comeback Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0d", 2) < 0)
		ret = -1;
	/* Protected GAS Initial Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x09\x0a", 2) < 0)
		ret = -1;
	/* Protected GAS Initial Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x09\x0b", 2) < 0)
		ret = -1;
	/* Protected GAS Comeback Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x09\x0c", 2) < 0)
		ret = -1;
	/* Protected GAS Comeback Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x09\x0d", 2) < 0)
		ret = -1;
#endif /* CONFIG_P2P || CONFIG_INTERWORKING */
#ifdef CONFIG_P2P
	/* P2P Public Action */
	if (nl80211_register_action_frame(bss,
					  (u8 *) "\x04\x09\x50\x6f\x9a\x09",
					  6) < 0)
		ret = -1;
	/* P2P Action */
	if (nl80211_register_action_frame(bss,
					  (u8 *) "\x7f\x50\x6f\x9a\x09",
					  5) < 0)
		ret = -1;
#endif /* CONFIG_P2P */
#ifdef CONFIG_IEEE80211W
	/* SA Query Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x08\x01", 2) < 0)
		ret = -1;
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_TDLS
	if ((drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT)) {
		/* TDLS Discovery Response */
		if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0e", 2) <
		    0)
			ret = -1;
	}
#endif /* CONFIG_TDLS */

	/* FT Action frames */
	if (nl80211_register_action_frame(bss, (u8 *) "\x06", 1) < 0)
		ret = -1;
	else
		drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT |
			WPA_DRIVER_CAPA_KEY_MGMT_FT_PSK;

	/* WNM - BSS Transition Management Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x07", 2) < 0)
		ret = -1;
	/* WNM-Sleep Mode Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x11", 2) < 0)
		ret = -1;

#ifdef CONFIG_HS20
	/* WNM-Notification */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x1a", 2) < 0)
		ret = -1;
#endif /* CONFIG_HS20 */

	/* WMM-AC ADDTS Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x11\x01", 2) < 0)
		ret = -1;

	/* WMM-AC DELTS */
	if (nl80211_register_action_frame(bss, (u8 *) "\x11\x02", 2) < 0)
		ret = -1;

	/* Radio Measurement - Neighbor Report Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x05\x05", 2) < 0)
		ret = -1;

	/* Radio Measurement - Link Measurement Request */
	if ((drv->capa.rrm_flags & WPA_DRIVER_FLAGS_TX_POWER_INSERTION) &&
	    (nl80211_register_action_frame(bss, (u8 *) "\x05\x02", 2) < 0))
		ret = -1;

	nl80211_mgmt_handle_register_eloop(bss);

	return ret;
}


static int nl80211_mgmt_subscribe_mesh(struct i802_bss *bss)
{
	int ret = 0;

	if (nl80211_alloc_mgmt_handle(bss))
		return -1;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Subscribe to mgmt frames with mesh handle %p",
		   bss->nl_mgmt);

	/* Auth frames for mesh SAE */
	if (nl80211_register_frame(bss, bss->nl_mgmt,
				   (WLAN_FC_TYPE_MGMT << 2) |
				   (WLAN_FC_STYPE_AUTH << 4),
				   NULL, 0) < 0)
		ret = -1;

	/* Mesh peering open */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0f\x01", 2) < 0)
		ret = -1;
	/* Mesh peering confirm */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0f\x02", 2) < 0)
		ret = -1;
	/* Mesh peering close */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0f\x03", 2) < 0)
		ret = -1;

	nl80211_mgmt_handle_register_eloop(bss);

	return ret;
}


static int nl80211_register_spurious_class3(struct i802_bss *bss)
{
	struct nl_msg *msg;
	int ret;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_UNEXPECTED_FRAME);
	ret = send_and_recv(bss->drv->global, bss->nl_mgmt, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Register spurious class3 "
			   "failed: ret=%d (%s)",
			   ret, strerror(-ret));
	}
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
		/* WLAN_FC_STYPE_BEACON, */
	};
	unsigned int i;

	if (nl80211_alloc_mgmt_handle(bss))
		return -1;
	wpa_printf(MSG_DEBUG, "nl80211: Subscribe to mgmt frames with AP "
		   "handle %p", bss->nl_mgmt);

	for (i = 0; i < ARRAY_SIZE(stypes); i++) {
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

	nl80211_mgmt_handle_register_eloop(bss);
	return 0;

out_err:
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

	nl80211_mgmt_handle_register_eloop(bss);
	return 0;

out_err:
	nl_destroy_handles(&bss->nl_mgmt);
	return -1;
}


static void nl80211_mgmt_unsubscribe(struct i802_bss *bss, const char *reason)
{
	if (bss->nl_mgmt == NULL)
		return;
	wpa_printf(MSG_DEBUG, "nl80211: Unsubscribe mgmt frames handle %p "
		   "(%s)", bss->nl_mgmt, reason);
	nl80211_destroy_eloop_handle(&bss->nl_mgmt);

	nl80211_put_wiphy_data_ap(bss);
}


static void wpa_driver_nl80211_send_rfkill(void *eloop_ctx, void *timeout_ctx)
{
	wpa_supplicant_event(timeout_ctx, EVENT_INTERFACE_DISABLED, NULL);
}


static void nl80211_del_p2pdev(struct i802_bss *bss)
{
	struct nl_msg *msg;
	int ret;

	msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_DEL_INTERFACE);
	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL);

	wpa_printf(MSG_DEBUG, "nl80211: Delete P2P Device %s (0x%llx): %s",
		   bss->ifname, (long long unsigned int) bss->wdev_id,
		   strerror(-ret));
}


static int nl80211_set_p2pdev(struct i802_bss *bss, int start)
{
	struct nl_msg *msg;
	int ret;

	msg = nl80211_cmd_msg(bss, 0, start ? NL80211_CMD_START_P2P_DEVICE :
			      NL80211_CMD_STOP_P2P_DEVICE);
	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL);

	wpa_printf(MSG_DEBUG, "nl80211: %s P2P Device %s (0x%llx): %s",
		   start ? "Start" : "Stop",
		   bss->ifname, (long long unsigned int) bss->wdev_id,
		   strerror(-ret));
	return ret;
}


static int i802_set_iface_flags(struct i802_bss *bss, int up)
{
	enum nl80211_iftype nlmode;

	nlmode = nl80211_get_ifmode(bss);
	if (nlmode != NL80211_IFTYPE_P2P_DEVICE) {
		return linux_set_iface_flags(bss->drv->global->ioctl_sock,
					     bss->ifname, up);
	}

	/* P2P Device has start/stop which is equivalent */
	return nl80211_set_p2pdev(bss, up);
}


#ifdef CONFIG_TESTING_OPTIONS
static int qca_vendor_test_cmd_handler(struct nl_msg *msg, void *arg)
{
	/* struct wpa_driver_nl80211_data *drv = arg; */
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));


	wpa_printf(MSG_DEBUG,
		   "nl80211: QCA vendor test command response received");

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_VENDOR_DATA]) {
		wpa_printf(MSG_DEBUG, "nl80211: No vendor data attribute");
		return NL_SKIP;
	}

	wpa_hexdump(MSG_DEBUG,
		    "nl80211: Received QCA vendor test command response",
		    nla_data(tb[NL80211_ATTR_VENDOR_DATA]),
		    nla_len(tb[NL80211_ATTR_VENDOR_DATA]));

	return NL_SKIP;
}
#endif /* CONFIG_TESTING_OPTIONS */


static void qca_vendor_test(struct wpa_driver_nl80211_data *drv)
{
#ifdef CONFIG_TESTING_OPTIONS
	struct nl_msg *msg;
	struct nlattr *params;
	int ret;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_TEST) ||
	    !(params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_TEST, 123)) {
		nlmsg_free(msg);
		return;
	}
	nla_nest_end(msg, params);

	ret = send_and_recv_msgs(drv, msg, qca_vendor_test_cmd_handler, drv);
	wpa_printf(MSG_DEBUG,
		   "nl80211: QCA vendor test command returned %d (%s)",
		   ret, strerror(-ret));
#endif /* CONFIG_TESTING_OPTIONS */
}


static int
wpa_driver_nl80211_finish_drv_init(struct wpa_driver_nl80211_data *drv,
				   const u8 *set_addr, int first,
				   const char *driver_params)
{
	struct i802_bss *bss = drv->first_bss;
	int send_rfkill_event = 0;
	enum nl80211_iftype nlmode;

	drv->ifindex = if_nametoindex(bss->ifname);
	bss->ifindex = drv->ifindex;
	bss->wdev_id = drv->global->if_add_wdevid;
	bss->wdev_id_set = drv->global->if_add_wdevid_set;

	bss->if_dynamic = drv->ifindex == drv->global->if_add_ifindex;
	bss->if_dynamic = bss->if_dynamic || drv->global->if_add_wdevid_set;
	drv->global->if_add_wdevid_set = 0;

	if (!bss->if_dynamic && nl80211_get_ifmode(bss) == NL80211_IFTYPE_AP)
		bss->static_ap = 1;

	if (wpa_driver_nl80211_capa(drv))
		return -1;

	if (driver_params && nl80211_set_param(bss, driver_params) < 0)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: interface %s in phy %s",
		   bss->ifname, drv->phyname);

	if (set_addr &&
	    (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 0) ||
	     linux_set_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
				set_addr)))
		return -1;

	if (first && nl80211_get_ifmode(bss) == NL80211_IFTYPE_AP)
		drv->start_mode_ap = 1;

	if (drv->hostapd || bss->static_ap)
		nlmode = NL80211_IFTYPE_AP;
	else if (bss->if_dynamic)
		nlmode = nl80211_get_ifmode(bss);
	else
		nlmode = NL80211_IFTYPE_STATION;

	if (wpa_driver_nl80211_set_mode(bss, nlmode) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not configure driver mode");
		return -1;
	}

	if (nlmode == NL80211_IFTYPE_P2P_DEVICE)
		nl80211_get_macaddr(bss);

	if (!rfkill_is_blocked(drv->rfkill)) {
		int ret = i802_set_iface_flags(bss, 1);
		if (ret) {
			wpa_printf(MSG_ERROR, "nl80211: Could not set "
				   "interface '%s' UP", bss->ifname);
			return ret;
		}
		if (nlmode == NL80211_IFTYPE_P2P_DEVICE)
			return ret;
	} else {
		wpa_printf(MSG_DEBUG, "nl80211: Could not yet enable "
			   "interface '%s' due to rfkill", bss->ifname);
		if (nlmode == NL80211_IFTYPE_P2P_DEVICE)
			return 0;
		drv->if_disabled = 1;
		send_rfkill_event = 1;
	}

	if (!drv->hostapd)
		netlink_send_oper_ifla(drv->global->netlink, drv->ifindex,
				       1, IF_OPER_DORMANT);

	if (linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
			       bss->addr))
		return -1;
	os_memcpy(drv->perm_addr, bss->addr, ETH_ALEN);

	if (send_rfkill_event) {
		eloop_register_timeout(0, 0, wpa_driver_nl80211_send_rfkill,
				       drv, drv->ctx);
	}

	if (drv->vendor_cmd_test_avail)
		qca_vendor_test(drv);

	return 0;
}


static int wpa_driver_nl80211_del_beacon(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;

	wpa_printf(MSG_DEBUG, "nl80211: Remove beacon (ifindex=%d)",
		   drv->ifindex);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_DEL_BEACON);
	return send_and_recv_msgs(drv, msg, NULL, NULL);
}


/**
 * wpa_driver_nl80211_deinit - Deinitialize nl80211 driver interface
 * @bss: Pointer to private nl80211 data from wpa_driver_nl80211_init()
 *
 * Shut down driver interface and processing of driver events. Free
 * private data buffer if one was allocated in wpa_driver_nl80211_init().
 */
static void wpa_driver_nl80211_deinit(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	wpa_printf(MSG_INFO, "nl80211: deinit ifname=%s disabled_11b_rates=%d",
		   bss->ifname, drv->disabled_11b_rates);

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
		if (drv->rtnl_sk)
			nl80211_handle_destroy(drv->rtnl_sk);
	}
	if (bss->added_bridge) {
		if (linux_set_iface_flags(drv->global->ioctl_sock, bss->brname,
					  0) < 0)
			wpa_printf(MSG_INFO,
				   "nl80211: Could not set bridge %s down",
				   bss->brname);
		if (linux_br_del(drv->global->ioctl_sock, bss->brname) < 0)
			wpa_printf(MSG_INFO, "nl80211: Failed to remove "
				   "bridge %s: %s",
				   bss->brname, strerror(errno));
	}

	nl80211_remove_monitor_interface(drv);

	if (is_ap_interface(drv->nlmode))
		wpa_driver_nl80211_del_beacon(drv);

	if (drv->eapol_sock >= 0) {
		eloop_unregister_read_sock(drv->eapol_sock);
		close(drv->eapol_sock);
	}

	if (drv->if_indices != drv->default_if_indices)
		os_free(drv->if_indices);

	if (drv->disabled_11b_rates)
		nl80211_disable_11b_rates(drv, drv->ifindex, 0);

	netlink_send_oper_ifla(drv->global->netlink, drv->ifindex, 0,
			       IF_OPER_UP);
	eloop_cancel_timeout(wpa_driver_nl80211_send_rfkill, drv, drv->ctx);
	rfkill_deinit(drv->rfkill);

	eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv, drv->ctx);

	if (!drv->start_iface_up)
		(void) i802_set_iface_flags(bss, 0);

	if (drv->addr_changed) {
		if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname,
					  0) < 0) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Could not set interface down to restore permanent MAC address");
		}
		if (linux_set_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
				       drv->perm_addr) < 0) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Could not restore permanent MAC address");
		}
	}

	if (drv->nlmode != NL80211_IFTYPE_P2P_DEVICE) {
		if (!drv->hostapd || !drv->start_mode_ap)
			wpa_driver_nl80211_set_mode(bss,
						    NL80211_IFTYPE_STATION);
		nl80211_mgmt_unsubscribe(bss, "deinit");
	} else {
		nl80211_mgmt_unsubscribe(bss, "deinit");
		nl80211_del_p2pdev(bss);
	}

	nl80211_destroy_bss(drv->first_bss);

	os_free(drv->filter_ssids);

	os_free(drv->auth_ie);

	if (drv->in_interface_list)
		dl_list_del(&drv->list);

	os_free(drv->extended_capa);
	os_free(drv->extended_capa_mask);
	os_free(drv->first_bss);
	os_free(drv);
}


static u32 wpa_alg_to_cipher_suite(enum wpa_alg alg, size_t key_len)
{
	switch (alg) {
	case WPA_ALG_WEP:
		if (key_len == 5)
			return WLAN_CIPHER_SUITE_WEP40;
		return WLAN_CIPHER_SUITE_WEP104;
	case WPA_ALG_TKIP:
		return WLAN_CIPHER_SUITE_TKIP;
	case WPA_ALG_CCMP:
		return WLAN_CIPHER_SUITE_CCMP;
	case WPA_ALG_GCMP:
		return WLAN_CIPHER_SUITE_GCMP;
	case WPA_ALG_CCMP_256:
		return WLAN_CIPHER_SUITE_CCMP_256;
	case WPA_ALG_GCMP_256:
		return WLAN_CIPHER_SUITE_GCMP_256;
	case WPA_ALG_IGTK:
		return WLAN_CIPHER_SUITE_AES_CMAC;
	case WPA_ALG_BIP_GMAC_128:
		return WLAN_CIPHER_SUITE_BIP_GMAC_128;
	case WPA_ALG_BIP_GMAC_256:
		return WLAN_CIPHER_SUITE_BIP_GMAC_256;
	case WPA_ALG_BIP_CMAC_256:
		return WLAN_CIPHER_SUITE_BIP_CMAC_256;
	case WPA_ALG_SMS4:
		return WLAN_CIPHER_SUITE_SMS4;
	case WPA_ALG_KRK:
		return WLAN_CIPHER_SUITE_KRK;
	case WPA_ALG_NONE:
	case WPA_ALG_PMK:
		wpa_printf(MSG_ERROR, "nl80211: Unexpected encryption algorithm %d",
			   alg);
		return 0;
	}

	wpa_printf(MSG_ERROR, "nl80211: Unsupported encryption algorithm %d",
		   alg);
	return 0;
}


static u32 wpa_cipher_to_cipher_suite(unsigned int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_CCMP_256:
		return WLAN_CIPHER_SUITE_CCMP_256;
	case WPA_CIPHER_GCMP_256:
		return WLAN_CIPHER_SUITE_GCMP_256;
	case WPA_CIPHER_CCMP:
		return WLAN_CIPHER_SUITE_CCMP;
	case WPA_CIPHER_GCMP:
		return WLAN_CIPHER_SUITE_GCMP;
	case WPA_CIPHER_TKIP:
		return WLAN_CIPHER_SUITE_TKIP;
	case WPA_CIPHER_WEP104:
		return WLAN_CIPHER_SUITE_WEP104;
	case WPA_CIPHER_WEP40:
		return WLAN_CIPHER_SUITE_WEP40;
	case WPA_CIPHER_GTK_NOT_USED:
		return WLAN_CIPHER_SUITE_NO_GROUP_ADDR;
	}

	return 0;
}


static int wpa_cipher_to_cipher_suites(unsigned int ciphers, u32 suites[],
				       int max_suites)
{
	int num_suites = 0;

	if (num_suites < max_suites && ciphers & WPA_CIPHER_CCMP_256)
		suites[num_suites++] = WLAN_CIPHER_SUITE_CCMP_256;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_GCMP_256)
		suites[num_suites++] = WLAN_CIPHER_SUITE_GCMP_256;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_CCMP)
		suites[num_suites++] = WLAN_CIPHER_SUITE_CCMP;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_GCMP)
		suites[num_suites++] = WLAN_CIPHER_SUITE_GCMP;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_TKIP)
		suites[num_suites++] = WLAN_CIPHER_SUITE_TKIP;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_WEP104)
		suites[num_suites++] = WLAN_CIPHER_SUITE_WEP104;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_WEP40)
		suites[num_suites++] = WLAN_CIPHER_SUITE_WEP40;

	return num_suites;
}


static int issue_key_mgmt_set_key(struct wpa_driver_nl80211_data *drv,
				  const u8 *key, size_t key_len)
{
	struct nl_msg *msg;
	int ret;

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_KEY_MGMT_OFFLOAD))
		return 0;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY) ||
	    nla_put(msg, NL80211_ATTR_VENDOR_DATA, key_len, key)) {
		nl80211_nlmsg_clear(msg);
		nlmsg_free(msg);
		return -1;
	}
	ret = send_and_recv_msgs(drv, msg, NULL, (void *) -1);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Key management set key failed: ret=%d (%s)",
			   ret, strerror(-ret));
	}

	return ret;
}


static int wpa_driver_nl80211_set_key(const char *ifname, struct i802_bss *bss,
				      enum wpa_alg alg, const u8 *addr,
				      int key_idx, int set_tx,
				      const u8 *seq, size_t seq_len,
				      const u8 *key, size_t key_len)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ifindex;
	struct nl_msg *msg;
	int ret;
	int tdls = 0;

	/* Ignore for P2P Device */
	if (drv->nlmode == NL80211_IFTYPE_P2P_DEVICE)
		return 0;

	ifindex = if_nametoindex(ifname);
	wpa_printf(MSG_DEBUG, "%s: ifindex=%d (%s) alg=%d addr=%p key_idx=%d "
		   "set_tx=%d seq_len=%lu key_len=%lu",
		   __func__, ifindex, ifname, alg, addr, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);
#ifdef CONFIG_TDLS
	if (key_idx == -1) {
		key_idx = 0;
		tdls = 1;
	}
#endif /* CONFIG_TDLS */

	if (alg == WPA_ALG_PMK &&
	    (drv->capa.flags & WPA_DRIVER_FLAGS_KEY_MGMT_OFFLOAD)) {
		wpa_printf(MSG_DEBUG, "%s: calling issue_key_mgmt_set_key",
			   __func__);
		ret = issue_key_mgmt_set_key(drv, key, key_len);
		return ret;
	}

	if (alg == WPA_ALG_NONE) {
		msg = nl80211_ifindex_msg(drv, ifindex, 0, NL80211_CMD_DEL_KEY);
		if (!msg)
			return -ENOBUFS;
	} else {
		msg = nl80211_ifindex_msg(drv, ifindex, 0, NL80211_CMD_NEW_KEY);
		if (!msg ||
		    nla_put(msg, NL80211_ATTR_KEY_DATA, key_len, key) ||
		    nla_put_u32(msg, NL80211_ATTR_KEY_CIPHER,
				wpa_alg_to_cipher_suite(alg, key_len)))
			goto fail;
		wpa_hexdump_key(MSG_DEBUG, "nl80211: KEY_DATA", key, key_len);
	}

	if (seq && seq_len) {
		if (nla_put(msg, NL80211_ATTR_KEY_SEQ, seq_len, seq))
			goto fail;
		wpa_hexdump(MSG_DEBUG, "nl80211: KEY_SEQ", seq, seq_len);
	}

	if (addr && !is_broadcast_ether_addr(addr)) {
		wpa_printf(MSG_DEBUG, "   addr=" MACSTR, MAC2STR(addr));
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr))
			goto fail;

		if (alg != WPA_ALG_WEP && key_idx && !set_tx) {
			wpa_printf(MSG_DEBUG, "   RSN IBSS RX GTK");
			if (nla_put_u32(msg, NL80211_ATTR_KEY_TYPE,
					NL80211_KEYTYPE_GROUP))
				goto fail;
		}
	} else if (addr && is_broadcast_ether_addr(addr)) {
		struct nlattr *types;

		wpa_printf(MSG_DEBUG, "   broadcast key");

		types = nla_nest_start(msg, NL80211_ATTR_KEY_DEFAULT_TYPES);
		if (!types ||
		    nla_put_flag(msg, NL80211_KEY_DEFAULT_TYPE_MULTICAST))
			goto fail;
		nla_nest_end(msg, types);
	}
	if (nla_put_u8(msg, NL80211_ATTR_KEY_IDX, key_idx))
		goto fail;

	ret = send_and_recv_msgs(drv, msg, NULL, key ? (void *) -1 : NULL);
	if ((ret == -ENOENT || ret == -ENOLINK) && alg == WPA_ALG_NONE)
		ret = 0;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: set_key failed; err=%d %s)",
			   ret, strerror(-ret));

	/*
	 * If we failed or don't need to set the default TX key (below),
	 * we're done here.
	 */
	if (ret || !set_tx || alg == WPA_ALG_NONE || tdls)
		return ret;
	if (is_ap_interface(drv->nlmode) && addr &&
	    !is_broadcast_ether_addr(addr))
		return ret;

	msg = nl80211_ifindex_msg(drv, ifindex, 0, NL80211_CMD_SET_KEY);
	if (!msg ||
	    nla_put_u8(msg, NL80211_ATTR_KEY_IDX, key_idx) ||
	    nla_put_flag(msg, (alg == WPA_ALG_IGTK ||
			       alg == WPA_ALG_BIP_GMAC_128 ||
			       alg == WPA_ALG_BIP_GMAC_256 ||
			       alg == WPA_ALG_BIP_CMAC_256) ?
			 NL80211_ATTR_KEY_DEFAULT_MGMT :
			 NL80211_ATTR_KEY_DEFAULT))
		goto fail;
	if (addr && is_broadcast_ether_addr(addr)) {
		struct nlattr *types;

		types = nla_nest_start(msg, NL80211_ATTR_KEY_DEFAULT_TYPES);
		if (!types ||
		    nla_put_flag(msg, NL80211_KEY_DEFAULT_TYPE_MULTICAST))
			goto fail;
		nla_nest_end(msg, types);
	} else if (addr) {
		struct nlattr *types;

		types = nla_nest_start(msg, NL80211_ATTR_KEY_DEFAULT_TYPES);
		if (!types ||
		    nla_put_flag(msg, NL80211_KEY_DEFAULT_TYPE_UNICAST))
			goto fail;
		nla_nest_end(msg, types);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret == -ENOENT)
		ret = 0;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: set_key default failed; "
			   "err=%d %s)", ret, strerror(-ret));
	return ret;

fail:
	nl80211_nlmsg_clear(msg);
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

	if (defkey && alg == WPA_ALG_IGTK) {
		if (nla_put_flag(msg, NL80211_KEY_DEFAULT_MGMT))
			return -1;
	} else if (defkey) {
		if (nla_put_flag(msg, NL80211_KEY_DEFAULT))
			return -1;
	}

	if (nla_put_u8(msg, NL80211_KEY_IDX, key_idx) ||
	    nla_put_u32(msg, NL80211_KEY_CIPHER,
			wpa_alg_to_cipher_suite(alg, key_len)) ||
	    (seq && seq_len &&
	     nla_put(msg, NL80211_KEY_SEQ, seq_len, seq)) ||
	    nla_put(msg, NL80211_KEY_DATA, key_len, key))
		return -1;

	nla_nest_end(msg, key_attr);

	return 0;
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

	if (nla_put_flag(msg, NL80211_ATTR_PRIVACY))
		return -ENOBUFS;

	nl_keys = nla_nest_start(msg, NL80211_ATTR_KEYS);
	if (!nl_keys)
		return -ENOBUFS;

	for (i = 0; i < 4; i++) {
		if (!params->wep_key[i])
			continue;

		nl_key = nla_nest_start(msg, i);
		if (!nl_key ||
		    nla_put(msg, NL80211_KEY_DATA, params->wep_key_len[i],
			    params->wep_key[i]) ||
		    nla_put_u32(msg, NL80211_KEY_CIPHER,
				params->wep_key_len[i] == 5 ?
				WLAN_CIPHER_SUITE_WEP40 :
				WLAN_CIPHER_SUITE_WEP104) ||
		    nla_put_u8(msg, NL80211_KEY_IDX, i) ||
		    (i == params->wep_tx_keyidx &&
		     nla_put_flag(msg, NL80211_KEY_DEFAULT)))
			return -ENOBUFS;

		nla_nest_end(msg, nl_key);
	}
	nla_nest_end(msg, nl_keys);

	return 0;
}


int wpa_driver_nl80211_mlme(struct wpa_driver_nl80211_data *drv,
			    const u8 *addr, int cmd, u16 reason_code,
			    int local_state_change)
{
	int ret;
	struct nl_msg *msg;

	if (!(msg = nl80211_drv_msg(drv, 0, cmd)) ||
	    nla_put_u16(msg, NL80211_ATTR_REASON_CODE, reason_code) ||
	    (addr && nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) ||
	    (local_state_change &&
	     nla_put_flag(msg, NL80211_ATTR_LOCAL_STATE_CHANGE))) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: MLME command failed: reason=%u ret=%d (%s)",
			reason_code, ret, strerror(-ret));
	}
	return ret;
}


static int wpa_driver_nl80211_disconnect(struct wpa_driver_nl80211_data *drv,
					 int reason_code)
{
	int ret;

	wpa_printf(MSG_DEBUG, "%s(reason_code=%d)", __func__, reason_code);
	nl80211_mark_disconnected(drv);
	/* Disconnect command doesn't need BSSID - it uses cached value */
	ret = wpa_driver_nl80211_mlme(drv, NULL, NL80211_CMD_DISCONNECT,
				      reason_code, 0);
	/*
	 * For locally generated disconnect, supplicant already generates a
	 * DEAUTH event, so ignore the event from NL80211.
	 */
	drv->ignore_next_local_disconnect = ret == 0;

	return ret;
}


static int wpa_driver_nl80211_deauthenticate(struct i802_bss *bss,
					     const u8 *addr, int reason_code)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret;

	if (drv->nlmode == NL80211_IFTYPE_ADHOC) {
		nl80211_mark_disconnected(drv);
		return nl80211_leave_ibss(drv, 1);
	}
	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_SME))
		return wpa_driver_nl80211_disconnect(drv, reason_code);
	wpa_printf(MSG_DEBUG, "%s(addr=" MACSTR " reason_code=%d)",
		   __func__, MAC2STR(addr), reason_code);
	nl80211_mark_disconnected(drv);
	ret = wpa_driver_nl80211_mlme(drv, addr, NL80211_CMD_DEAUTHENTICATE,
				      reason_code, 0);
	/*
	 * For locally generated deauthenticate, supplicant already generates a
	 * DEAUTH event, so ignore the event from NL80211.
	 */
	drv->ignore_next_local_deauth = ret == 0;
	return ret;
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


static void nl80211_unmask_11b_rates(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (is_p2p_net_interface(drv->nlmode) || !drv->disabled_11b_rates)
		return;

	/*
	 * Looks like we failed to unmask 11b rates previously. This could
	 * happen, e.g., if the interface was down at the point in time when a
	 * P2P group was terminated.
	 */
	wpa_printf(MSG_DEBUG,
		   "nl80211: Interface %s mode is for non-P2P, but 11b rates were disabled - re-enable them",
		   bss->ifname);
	nl80211_disable_11b_rates(drv, drv->ifindex, 0);
}


static int wpa_driver_nl80211_authenticate(
	struct i802_bss *bss, struct wpa_driver_auth_params *params)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1, i;
	struct nl_msg *msg;
	enum nl80211_auth_type type;
	enum nl80211_iftype nlmode;
	int count = 0;
	int is_retry;

	nl80211_unmask_11b_rates(bss);

	is_retry = drv->retry_auth;
	drv->retry_auth = 0;
	drv->ignore_deauth_event = 0;

	nl80211_mark_disconnected(drv);
	os_memset(drv->auth_bssid, 0, ETH_ALEN);
	if (params->bssid)
		os_memcpy(drv->auth_attempt_bssid, params->bssid, ETH_ALEN);
	else
		os_memset(drv->auth_attempt_bssid, 0, ETH_ALEN);
	/* FIX: IBSS mode */
	nlmode = params->p2p ?
		NL80211_IFTYPE_P2P_CLIENT : NL80211_IFTYPE_STATION;
	if (drv->nlmode != nlmode &&
	    wpa_driver_nl80211_set_mode(bss, nlmode) < 0)
		return -1;

retry:
	wpa_printf(MSG_DEBUG, "nl80211: Authenticate (ifindex=%d)",
		   drv->ifindex);

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_AUTHENTICATE);
	if (!msg)
		goto fail;

	for (i = 0; i < 4; i++) {
		if (!params->wep_key[i])
			continue;
		wpa_driver_nl80211_set_key(bss->ifname, bss, WPA_ALG_WEP,
					   NULL, i,
					   i == params->wep_tx_keyidx, NULL, 0,
					   params->wep_key[i],
					   params->wep_key_len[i]);
		if (params->wep_tx_keyidx != i)
			continue;
		if (nl_add_key(msg, WPA_ALG_WEP, i, 1, NULL, 0,
			       params->wep_key[i], params->wep_key_len[i]))
			goto fail;
	}

	if (params->bssid) {
		wpa_printf(MSG_DEBUG, "  * bssid=" MACSTR,
			   MAC2STR(params->bssid));
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid))
			goto fail;
	}
	if (params->freq) {
		wpa_printf(MSG_DEBUG, "  * freq=%d", params->freq);
		if (nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, params->freq))
			goto fail;
	}
	if (params->ssid) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * SSID",
				  params->ssid, params->ssid_len);
		if (nla_put(msg, NL80211_ATTR_SSID, params->ssid_len,
			    params->ssid))
			goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "  * IEs", params->ie, params->ie_len);
	if (params->ie &&
	    nla_put(msg, NL80211_ATTR_IE, params->ie_len, params->ie))
		goto fail;
	if (params->sae_data) {
		wpa_hexdump(MSG_DEBUG, "  * SAE data", params->sae_data,
			    params->sae_data_len);
		if (nla_put(msg, NL80211_ATTR_SAE_DATA, params->sae_data_len,
			    params->sae_data))
			goto fail;
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
		goto fail;
	wpa_printf(MSG_DEBUG, "  * Auth Type %d", type);
	if (nla_put_u32(msg, NL80211_ATTR_AUTH_TYPE, type))
		goto fail;
	if (params->local_state_change) {
		wpa_printf(MSG_DEBUG, "  * Local state change only");
		if (nla_put_flag(msg, NL80211_ATTR_LOCAL_STATE_CHANGE))
			goto fail;
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
			drv->ignore_deauth_event = 1;
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
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Authentication request send successfully");
	}

fail:
	nlmsg_free(msg);
	return ret;
}


int wpa_driver_nl80211_authenticate_retry(struct wpa_driver_nl80211_data *drv)
{
	struct wpa_driver_auth_params params;
	struct i802_bss *bss = drv->first_bss;
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


static int wpa_driver_nl80211_send_frame(struct i802_bss *bss,
					 const void *data, size_t len,
					 int encrypt, int noack,
					 unsigned int freq, int no_cck,
					 int offchanok, unsigned int wait_time)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	u64 cookie;
	int res;

	if (freq == 0 && drv->nlmode == NL80211_IFTYPE_ADHOC) {
		freq = nl80211_get_assoc_freq(drv);
		wpa_printf(MSG_DEBUG,
			   "nl80211: send_frame - Use assoc_freq=%u for IBSS",
			   freq);
	}
	if (freq == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: send_frame - Use bss->freq=%u",
			   bss->freq);
		freq = bss->freq;
	}

	if (drv->use_monitor) {
		wpa_printf(MSG_DEBUG, "nl80211: send_frame(freq=%u bss->freq=%u) -> send_monitor",
			   freq, bss->freq);
		return nl80211_send_monitor(drv, data, len, encrypt, noack);
	}

	wpa_printf(MSG_DEBUG, "nl80211: send_frame -> send_frame_cmd");
	res = nl80211_send_frame_cmd(bss, freq, wait_time, data, len,
				     &cookie, no_cck, noack, offchanok);
	if (res == 0 && !noack) {
		const struct ieee80211_mgmt *mgmt;
		u16 fc;

		mgmt = (const struct ieee80211_mgmt *) data;
		fc = le_to_host16(mgmt->frame_control);
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION) {
			wpa_printf(MSG_MSGDUMP,
				   "nl80211: Update send_action_cookie from 0x%llx to 0x%llx",
				   (long long unsigned int)
				   drv->send_action_cookie,
				   (long long unsigned int) cookie);
			drv->send_action_cookie = cookie;
		}
	}

	return res;
}


static int wpa_driver_nl80211_send_mlme(struct i802_bss *bss, const u8 *data,
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
	wpa_printf(MSG_DEBUG, "nl80211: send_mlme - da= " MACSTR
		   " noack=%d freq=%u no_cck=%d offchanok=%d wait_time=%u fc=0x%x (%s) nlmode=%d",
		   MAC2STR(mgmt->da), noack, freq, no_cck, offchanok, wait_time,
		   fc, fc2str(fc), drv->nlmode);

	if ((is_sta_interface(drv->nlmode) ||
	     drv->nlmode == NL80211_IFTYPE_P2P_DEVICE) &&
	    WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
	    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_PROBE_RESP) {
		/*
		 * The use of last_mgmt_freq is a bit of a hack,
		 * but it works due to the single-threaded nature
		 * of wpa_supplicant.
		 */
		if (freq == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Use last_mgmt_freq=%d",
				   drv->last_mgmt_freq);
			freq = drv->last_mgmt_freq;
		}
		return nl80211_send_frame_cmd(bss, freq, 0,
					      data, data_len, NULL, 1, noack,
					      1);
	}

	if (drv->device_ap_sme && is_ap_interface(drv->nlmode)) {
		if (freq == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Use bss->freq=%d",
				   bss->freq);
			freq = bss->freq;
		}
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

	wpa_printf(MSG_DEBUG, "nl80211: send_mlme -> send_frame");
	return wpa_driver_nl80211_send_frame(bss, data, data_len, encrypt,
					     noack, freq, no_cck, offchanok,
					     wait_time);
}


static int nl80211_put_basic_rates(struct nl_msg *msg, const int *basic_rates)
{
	u8 rates[NL80211_MAX_SUPP_RATES];
	u8 rates_len = 0;
	int i;

	if (!basic_rates)
		return 0;

	for (i = 0; i < NL80211_MAX_SUPP_RATES && basic_rates[i] >= 0; i++)
		rates[rates_len++] = basic_rates[i] / 5;

	return nla_put(msg, NL80211_ATTR_BSS_BASIC_RATES, rates_len, rates);
}


static int nl80211_set_bss(struct i802_bss *bss, int cts, int preamble,
			   int slot, int ht_opmode, int ap_isolate,
			   const int *basic_rates)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_BSS)) ||
	    (cts >= 0 &&
	     nla_put_u8(msg, NL80211_ATTR_BSS_CTS_PROT, cts)) ||
	    (preamble >= 0 &&
	     nla_put_u8(msg, NL80211_ATTR_BSS_SHORT_PREAMBLE, preamble)) ||
	    (slot >= 0 &&
	     nla_put_u8(msg, NL80211_ATTR_BSS_SHORT_SLOT_TIME, slot)) ||
	    (ht_opmode >= 0 &&
	     nla_put_u16(msg, NL80211_ATTR_BSS_HT_OPMODE, ht_opmode)) ||
	    (ap_isolate >= 0 &&
	     nla_put_u8(msg, NL80211_ATTR_AP_ISOLATE, ap_isolate)) ||
	    nl80211_put_basic_rates(msg, basic_rates)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(drv, msg, NULL, NULL);
}


static int wpa_driver_nl80211_set_acl(void *priv,
				      struct hostapd_acl_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *acl;
	unsigned int i;
	int ret;

	if (!(drv->capa.max_acl_mac_addrs))
		return -ENOTSUP;

	if (params->num_mac_acl > drv->capa.max_acl_mac_addrs)
		return -ENOTSUP;

	wpa_printf(MSG_DEBUG, "nl80211: Set %s ACL (num_mac_acl=%u)",
		   params->acl_policy ? "Accept" : "Deny", params->num_mac_acl);

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_MAC_ACL)) ||
	    nla_put_u32(msg, NL80211_ATTR_ACL_POLICY, params->acl_policy ?
			NL80211_ACL_POLICY_DENY_UNLESS_LISTED :
			NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED) ||
	    (acl = nla_nest_start(msg, NL80211_ATTR_MAC_ADDRS)) == NULL) {
		nlmsg_free(msg);
		return -ENOMEM;
	}

	for (i = 0; i < params->num_mac_acl; i++) {
		if (nla_put(msg, i + 1, ETH_ALEN, params->mac_acl[i].addr)) {
			nlmsg_free(msg);
			return -ENOMEM;
		}
	}

	nla_nest_end(msg, acl);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Failed to set MAC ACL: %d (%s)",
			   ret, strerror(-ret));
	}

	return ret;
}


static int nl80211_put_beacon_int(struct nl_msg *msg, int beacon_int)
{
	if (beacon_int > 0) {
		wpa_printf(MSG_DEBUG, "  * beacon_int=%d", beacon_int);
		return nla_put_u32(msg, NL80211_ATTR_BEACON_INTERVAL,
				   beacon_int);
	}

	return 0;
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
	int num_suites;
	int smps_mode;
	u32 suites[10], suite;
	u32 ver;

	beacon_set = params->reenable ? 0 : bss->beacon_set;

	wpa_printf(MSG_DEBUG, "nl80211: Set beacon (beacon_set=%d)",
		   beacon_set);
	if (beacon_set)
		cmd = NL80211_CMD_SET_BEACON;

	wpa_hexdump(MSG_DEBUG, "nl80211: Beacon head",
		    params->head, params->head_len);
	wpa_hexdump(MSG_DEBUG, "nl80211: Beacon tail",
		    params->tail, params->tail_len);
	wpa_printf(MSG_DEBUG, "nl80211: ifindex=%d", bss->ifindex);
	wpa_printf(MSG_DEBUG, "nl80211: beacon_int=%d", params->beacon_int);
	wpa_printf(MSG_DEBUG, "nl80211: dtim_period=%d", params->dtim_period);
	wpa_hexdump_ascii(MSG_DEBUG, "nl80211: ssid",
			  params->ssid, params->ssid_len);
	if (!(msg = nl80211_bss_msg(bss, 0, cmd)) ||
	    nla_put(msg, NL80211_ATTR_BEACON_HEAD, params->head_len,
		    params->head) ||
	    nla_put(msg, NL80211_ATTR_BEACON_TAIL, params->tail_len,
		    params->tail) ||
	    nl80211_put_beacon_int(msg, params->beacon_int) ||
	    nla_put_u32(msg, NL80211_ATTR_DTIM_PERIOD, params->dtim_period) ||
	    nla_put(msg, NL80211_ATTR_SSID, params->ssid_len, params->ssid))
		goto fail;
	if (params->proberesp && params->proberesp_len) {
		wpa_hexdump(MSG_DEBUG, "nl80211: proberesp (offload)",
			    params->proberesp, params->proberesp_len);
		if (nla_put(msg, NL80211_ATTR_PROBE_RESP, params->proberesp_len,
			    params->proberesp))
			goto fail;
	}
	switch (params->hide_ssid) {
	case NO_SSID_HIDING:
		wpa_printf(MSG_DEBUG, "nl80211: hidden SSID not in use");
		if (nla_put_u32(msg, NL80211_ATTR_HIDDEN_SSID,
				NL80211_HIDDEN_SSID_NOT_IN_USE))
			goto fail;
		break;
	case HIDDEN_SSID_ZERO_LEN:
		wpa_printf(MSG_DEBUG, "nl80211: hidden SSID zero len");
		if (nla_put_u32(msg, NL80211_ATTR_HIDDEN_SSID,
				NL80211_HIDDEN_SSID_ZERO_LEN))
			goto fail;
		break;
	case HIDDEN_SSID_ZERO_CONTENTS:
		wpa_printf(MSG_DEBUG, "nl80211: hidden SSID zero contents");
		if (nla_put_u32(msg, NL80211_ATTR_HIDDEN_SSID,
				NL80211_HIDDEN_SSID_ZERO_CONTENTS))
			goto fail;
		break;
	}
	wpa_printf(MSG_DEBUG, "nl80211: privacy=%d", params->privacy);
	if (params->privacy &&
	    nla_put_flag(msg, NL80211_ATTR_PRIVACY))
		goto fail;
	wpa_printf(MSG_DEBUG, "nl80211: auth_algs=0x%x", params->auth_algs);
	if ((params->auth_algs & (WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED)) ==
	    (WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED)) {
		/* Leave out the attribute */
	} else if (params->auth_algs & WPA_AUTH_ALG_SHARED) {
		if (nla_put_u32(msg, NL80211_ATTR_AUTH_TYPE,
				NL80211_AUTHTYPE_SHARED_KEY))
			goto fail;
	} else {
		if (nla_put_u32(msg, NL80211_ATTR_AUTH_TYPE,
				NL80211_AUTHTYPE_OPEN_SYSTEM))
			goto fail;
	}

	wpa_printf(MSG_DEBUG, "nl80211: wpa_version=0x%x", params->wpa_version);
	ver = 0;
	if (params->wpa_version & WPA_PROTO_WPA)
		ver |= NL80211_WPA_VERSION_1;
	if (params->wpa_version & WPA_PROTO_RSN)
		ver |= NL80211_WPA_VERSION_2;
	if (ver &&
	    nla_put_u32(msg, NL80211_ATTR_WPA_VERSIONS, ver))
		goto fail;

	wpa_printf(MSG_DEBUG, "nl80211: key_mgmt_suites=0x%x",
		   params->key_mgmt_suites);
	num_suites = 0;
	if (params->key_mgmt_suites & WPA_KEY_MGMT_IEEE8021X)
		suites[num_suites++] = WLAN_AKM_SUITE_8021X;
	if (params->key_mgmt_suites & WPA_KEY_MGMT_PSK)
		suites[num_suites++] = WLAN_AKM_SUITE_PSK;
	if (num_suites &&
	    nla_put(msg, NL80211_ATTR_AKM_SUITES, num_suites * sizeof(u32),
		    suites))
		goto fail;

	if (params->key_mgmt_suites & WPA_KEY_MGMT_IEEE8021X_NO_WPA &&
	    params->pairwise_ciphers & (WPA_CIPHER_WEP104 | WPA_CIPHER_WEP40) &&
	    nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT))
		goto fail;

	wpa_printf(MSG_DEBUG, "nl80211: pairwise_ciphers=0x%x",
		   params->pairwise_ciphers);
	num_suites = wpa_cipher_to_cipher_suites(params->pairwise_ciphers,
						 suites, ARRAY_SIZE(suites));
	if (num_suites &&
	    nla_put(msg, NL80211_ATTR_CIPHER_SUITES_PAIRWISE,
		    num_suites * sizeof(u32), suites))
		goto fail;

	wpa_printf(MSG_DEBUG, "nl80211: group_cipher=0x%x",
		   params->group_cipher);
	suite = wpa_cipher_to_cipher_suite(params->group_cipher);
	if (suite &&
	    nla_put_u32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP, suite))
		goto fail;

	switch (params->smps_mode) {
	case HT_CAP_INFO_SMPS_DYNAMIC:
		wpa_printf(MSG_DEBUG, "nl80211: SMPS mode - dynamic");
		smps_mode = NL80211_SMPS_DYNAMIC;
		break;
	case HT_CAP_INFO_SMPS_STATIC:
		wpa_printf(MSG_DEBUG, "nl80211: SMPS mode - static");
		smps_mode = NL80211_SMPS_STATIC;
		break;
	default:
		/* invalid - fallback to smps off */
	case HT_CAP_INFO_SMPS_DISABLED:
		wpa_printf(MSG_DEBUG, "nl80211: SMPS mode - off");
		smps_mode = NL80211_SMPS_OFF;
		break;
	}
	if (nla_put_u32(msg, NL80211_ATTR_SMPS_MODE, smps_mode))
		goto fail;

	if (params->beacon_ies) {
		wpa_hexdump_buf(MSG_DEBUG, "nl80211: beacon_ies",
				params->beacon_ies);
		if (nla_put(msg, NL80211_ATTR_IE,
			    wpabuf_len(params->beacon_ies),
			    wpabuf_head(params->beacon_ies)))
			goto fail;
	}
	if (params->proberesp_ies) {
		wpa_hexdump_buf(MSG_DEBUG, "nl80211: proberesp_ies",
				params->proberesp_ies);
		if (nla_put(msg, NL80211_ATTR_IE_PROBE_RESP,
			    wpabuf_len(params->proberesp_ies),
			    wpabuf_head(params->proberesp_ies)))
			goto fail;
	}
	if (params->assocresp_ies) {
		wpa_hexdump_buf(MSG_DEBUG, "nl80211: assocresp_ies",
				params->assocresp_ies);
		if (nla_put(msg, NL80211_ATTR_IE_ASSOC_RESP,
			    wpabuf_len(params->assocresp_ies),
			    wpabuf_head(params->assocresp_ies)))
			goto fail;
	}

	if (drv->capa.flags & WPA_DRIVER_FLAGS_INACTIVITY_TIMER)  {
		wpa_printf(MSG_DEBUG, "nl80211: ap_max_inactivity=%d",
			   params->ap_max_inactivity);
		if (nla_put_u16(msg, NL80211_ATTR_INACTIVITY_TIMEOUT,
				params->ap_max_inactivity))
			goto fail;
	}

#ifdef CONFIG_P2P
	if (params->p2p_go_ctwindow > 0) {
		if (drv->p2p_go_ctwindow_supported) {
			wpa_printf(MSG_DEBUG, "nl80211: P2P GO ctwindow=%d",
				   params->p2p_go_ctwindow);
			if (nla_put_u8(msg, NL80211_ATTR_P2P_CTWINDOW,
				       params->p2p_go_ctwindow))
				goto fail;
		} else {
			wpa_printf(MSG_INFO,
				   "nl80211: Driver does not support CTWindow configuration - ignore this parameter");
		}
	}
#endif /* CONFIG_P2P */

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Beacon set failed: %d (%s)",
			   ret, strerror(-ret));
	} else {
		bss->beacon_set = 1;
		nl80211_set_bss(bss, params->cts_protect, params->preamble,
				params->short_slot_time, params->ht_opmode,
				params->isolate, params->basic_rates);
		if (beacon_set && params->freq &&
		    params->freq->bandwidth != bss->bandwidth) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Update BSS %s bandwidth: %d -> %d",
				   bss->ifname, bss->bandwidth,
				   params->freq->bandwidth);
			ret = nl80211_set_channel(bss, params->freq, 1);
			if (ret) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Frequency set failed: %d (%s)",
					   ret, strerror(-ret));
			} else {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Frequency set succeeded for ht2040 coex");
				bss->bandwidth = params->freq->bandwidth;
			}
		} else if (!beacon_set) {
			/*
			 * cfg80211 updates the driver on frequence change in AP
			 * mode only at the point when beaconing is started, so
			 * set the initial value here.
			 */
			bss->bandwidth = params->freq->bandwidth;
		}
	}
	return ret;
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int nl80211_put_freq_params(struct nl_msg *msg,
				   const struct hostapd_freq_params *freq)
{
	wpa_printf(MSG_DEBUG, "  * freq=%d", freq->freq);
	if (nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, freq->freq))
		return -ENOBUFS;

	wpa_printf(MSG_DEBUG, "  * vht_enabled=%d", freq->vht_enabled);
	wpa_printf(MSG_DEBUG, "  * ht_enabled=%d", freq->ht_enabled);

	if (freq->vht_enabled) {
		enum nl80211_chan_width cw;

		wpa_printf(MSG_DEBUG, "  * bandwidth=%d", freq->bandwidth);
		switch (freq->bandwidth) {
		case 20:
			cw = NL80211_CHAN_WIDTH_20;
			break;
		case 40:
			cw = NL80211_CHAN_WIDTH_40;
			break;
		case 80:
			if (freq->center_freq2)
				cw = NL80211_CHAN_WIDTH_80P80;
			else
				cw = NL80211_CHAN_WIDTH_80;
			break;
		case 160:
			cw = NL80211_CHAN_WIDTH_160;
			break;
		default:
			return -EINVAL;
		}

		wpa_printf(MSG_DEBUG, "  * channel_width=%d", cw);
		wpa_printf(MSG_DEBUG, "  * center_freq1=%d",
			   freq->center_freq1);
		wpa_printf(MSG_DEBUG, "  * center_freq2=%d",
			   freq->center_freq2);
		if (nla_put_u32(msg, NL80211_ATTR_CHANNEL_WIDTH, cw) ||
		    nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ1,
				freq->center_freq1) ||
		    (freq->center_freq2 &&
		     nla_put_u32(msg, NL80211_ATTR_CENTER_FREQ2,
				 freq->center_freq2)))
			return -ENOBUFS;
	} else if (freq->ht_enabled) {
		enum nl80211_channel_type ct;

		wpa_printf(MSG_DEBUG, "  * sec_channel_offset=%d",
			   freq->sec_channel_offset);
		switch (freq->sec_channel_offset) {
		case -1:
			ct = NL80211_CHAN_HT40MINUS;
			break;
		case 1:
			ct = NL80211_CHAN_HT40PLUS;
			break;
		default:
			ct = NL80211_CHAN_HT20;
			break;
		}

		wpa_printf(MSG_DEBUG, "  * channel_type=%d", ct);
		if (nla_put_u32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, ct))
			return -ENOBUFS;
	}
	return 0;
}


static int nl80211_set_channel(struct i802_bss *bss,
			       struct hostapd_freq_params *freq, int set_chan)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Set freq %d (ht_enabled=%d, vht_enabled=%d, bandwidth=%d MHz, cf1=%d MHz, cf2=%d MHz)",
		   freq->freq, freq->ht_enabled, freq->vht_enabled,
		   freq->bandwidth, freq->center_freq1, freq->center_freq2);

	msg = nl80211_drv_msg(drv, 0, set_chan ? NL80211_CMD_SET_CHANNEL :
			      NL80211_CMD_SET_WIPHY);
	if (!msg || nl80211_put_freq_params(msg, freq) < 0) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret == 0) {
		bss->freq = freq->freq;
		return 0;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set channel (freq=%d): "
		   "%d (%s)", freq->freq, ret, strerror(-ret));
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
	if (flags & WPA_STA_AUTHENTICATED)
		f |= BIT(NL80211_STA_FLAG_AUTHENTICATED);

	return f;
}


#ifdef CONFIG_MESH
static u32 sta_plink_state_nl80211(enum mesh_plink_state state)
{
	switch (state) {
	case PLINK_LISTEN:
		return NL80211_PLINK_LISTEN;
	case PLINK_OPEN_SENT:
		return NL80211_PLINK_OPN_SNT;
	case PLINK_OPEN_RCVD:
		return NL80211_PLINK_OPN_RCVD;
	case PLINK_CNF_RCVD:
		return NL80211_PLINK_CNF_RCVD;
	case PLINK_ESTAB:
		return NL80211_PLINK_ESTAB;
	case PLINK_HOLDING:
		return NL80211_PLINK_HOLDING;
	case PLINK_BLOCKED:
		return NL80211_PLINK_BLOCKED;
	default:
		wpa_printf(MSG_ERROR, "nl80211: Invalid mesh plink state %d",
			   state);
	}
	return -1;
}
#endif /* CONFIG_MESH */


static int wpa_driver_nl80211_sta_add(void *priv,
				      struct hostapd_sta_add_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nl80211_sta_flag_update upd;
	int ret = -ENOBUFS;

	if ((params->flags & WPA_STA_TDLS_PEER) &&
	    !(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT))
		return -EOPNOTSUPP;

	wpa_printf(MSG_DEBUG, "nl80211: %s STA " MACSTR,
		   params->set ? "Set" : "Add", MAC2STR(params->addr));
	msg = nl80211_bss_msg(bss, 0, params->set ? NL80211_CMD_SET_STATION :
			      NL80211_CMD_NEW_STATION);
	if (!msg || nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, params->addr))
		goto fail;

	if (!params->set || (params->flags & WPA_STA_TDLS_PEER)) {
		wpa_hexdump(MSG_DEBUG, "  * supported rates",
			    params->supp_rates, params->supp_rates_len);
		wpa_printf(MSG_DEBUG, "  * capability=0x%x",
			   params->capability);
		if (nla_put(msg, NL80211_ATTR_STA_SUPPORTED_RATES,
			    params->supp_rates_len, params->supp_rates) ||
		    nla_put_u16(msg, NL80211_ATTR_STA_CAPABILITY,
				params->capability))
			goto fail;

		if (params->ht_capabilities) {
			wpa_hexdump(MSG_DEBUG, "  * ht_capabilities",
				    (u8 *) params->ht_capabilities,
				    sizeof(*params->ht_capabilities));
			if (nla_put(msg, NL80211_ATTR_HT_CAPABILITY,
				    sizeof(*params->ht_capabilities),
				    params->ht_capabilities))
				goto fail;
		}

		if (params->vht_capabilities) {
			wpa_hexdump(MSG_DEBUG, "  * vht_capabilities",
				    (u8 *) params->vht_capabilities,
				    sizeof(*params->vht_capabilities));
			if (nla_put(msg, NL80211_ATTR_VHT_CAPABILITY,
				    sizeof(*params->vht_capabilities),
				    params->vht_capabilities))
				goto fail;
		}

		if (params->ext_capab) {
			wpa_hexdump(MSG_DEBUG, "  * ext_capab",
				    params->ext_capab, params->ext_capab_len);
			if (nla_put(msg, NL80211_ATTR_STA_EXT_CAPABILITY,
				    params->ext_capab_len, params->ext_capab))
				goto fail;
		}
	}
	if (!params->set) {
		if (params->aid) {
			wpa_printf(MSG_DEBUG, "  * aid=%u", params->aid);
			if (nla_put_u16(msg, NL80211_ATTR_STA_AID, params->aid))
				goto fail;
		} else {
			/*
			 * cfg80211 validates that AID is non-zero, so we have
			 * to make this a non-zero value for the TDLS case where
			 * a dummy STA entry is used for now.
			 */
			wpa_printf(MSG_DEBUG, "  * aid=1 (TDLS workaround)");
			if (nla_put_u16(msg, NL80211_ATTR_STA_AID, 1))
				goto fail;
		}
		wpa_printf(MSG_DEBUG, "  * listen_interval=%u",
			   params->listen_interval);
		if (nla_put_u16(msg, NL80211_ATTR_STA_LISTEN_INTERVAL,
				params->listen_interval))
			goto fail;
	} else if (params->aid && (params->flags & WPA_STA_TDLS_PEER)) {
		wpa_printf(MSG_DEBUG, "  * peer_aid=%u", params->aid);
		if (nla_put_u16(msg, NL80211_ATTR_PEER_AID, params->aid))
			goto fail;
	}

	if (params->vht_opmode_enabled) {
		wpa_printf(MSG_DEBUG, "  * opmode=%u", params->vht_opmode);
		if (nla_put_u8(msg, NL80211_ATTR_OPMODE_NOTIF,
			       params->vht_opmode))
			goto fail;
	}

	if (params->supp_channels) {
		wpa_hexdump(MSG_DEBUG, "  * supported channels",
			    params->supp_channels, params->supp_channels_len);
		if (nla_put(msg, NL80211_ATTR_STA_SUPPORTED_CHANNELS,
			    params->supp_channels_len, params->supp_channels))
			goto fail;
	}

	if (params->supp_oper_classes) {
		wpa_hexdump(MSG_DEBUG, "  * supported operating classes",
			    params->supp_oper_classes,
			    params->supp_oper_classes_len);
		if (nla_put(msg, NL80211_ATTR_STA_SUPPORTED_OPER_CLASSES,
			    params->supp_oper_classes_len,
			    params->supp_oper_classes))
			goto fail;
	}

	os_memset(&upd, 0, sizeof(upd));
	upd.set = sta_flags_nl80211(params->flags);
	upd.mask = upd.set | sta_flags_nl80211(params->flags_mask);
	wpa_printf(MSG_DEBUG, "  * flags set=0x%x mask=0x%x",
		   upd.set, upd.mask);
	if (nla_put(msg, NL80211_ATTR_STA_FLAGS2, sizeof(upd), &upd))
		goto fail;

#ifdef CONFIG_MESH
	if (params->plink_state &&
	    nla_put_u8(msg, NL80211_ATTR_STA_PLINK_STATE,
		       sta_plink_state_nl80211(params->plink_state)))
		goto fail;
#endif /* CONFIG_MESH */

	if (params->flags & WPA_STA_WMM) {
		struct nlattr *wme = nla_nest_start(msg, NL80211_ATTR_STA_WME);

		wpa_printf(MSG_DEBUG, "  * qosinfo=0x%x", params->qosinfo);
		if (!wme ||
		    nla_put_u8(msg, NL80211_STA_WME_UAPSD_QUEUES,
			       params->qosinfo & WMM_QOSINFO_STA_AC_MASK) ||
		    nla_put_u8(msg, NL80211_STA_WME_MAX_SP,
			       (params->qosinfo >> WMM_QOSINFO_STA_SP_SHIFT) &
			       WMM_QOSINFO_STA_SP_MASK))
			goto fail;
		nla_nest_end(msg, wme);
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: NL80211_CMD_%s_STATION "
			   "result: %d (%s)", params->set ? "SET" : "NEW", ret,
			   strerror(-ret));
	if (ret == -EEXIST)
		ret = 0;
fail:
	nlmsg_free(msg);
	return ret;
}


static void rtnl_neigh_delete_fdb_entry(struct i802_bss *bss, const u8 *addr)
{
#ifdef CONFIG_LIBNL3_ROUTE
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct rtnl_neigh *rn;
	struct nl_addr *nl_addr;
	int err;

	rn = rtnl_neigh_alloc();
	if (!rn)
		return;

	rtnl_neigh_set_family(rn, AF_BRIDGE);
	rtnl_neigh_set_ifindex(rn, bss->ifindex);
	nl_addr = nl_addr_build(AF_BRIDGE, (void *) addr, ETH_ALEN);
	if (!nl_addr) {
		rtnl_neigh_put(rn);
		return;
	}
	rtnl_neigh_set_lladdr(rn, nl_addr);

	err = rtnl_neigh_delete(drv->rtnl_sk, rn, 0);
	if (err < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: bridge FDB entry delete for "
			   MACSTR " ifindex=%d failed: %s", MAC2STR(addr),
			   bss->ifindex, nl_geterror(err));
	} else {
		wpa_printf(MSG_DEBUG, "nl80211: deleted bridge FDB entry for "
			   MACSTR, MAC2STR(addr));
	}

	nl_addr_put(nl_addr);
	rtnl_neigh_put(rn);
#endif /* CONFIG_LIBNL3_ROUTE */
}


static int wpa_driver_nl80211_sta_remove(struct i802_bss *bss, const u8 *addr,
					 int deauth, u16 reason_code)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_DEL_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
	    (deauth == 0 &&
	     nla_put_u8(msg, NL80211_ATTR_MGMT_SUBTYPE,
			WLAN_FC_STYPE_DISASSOC)) ||
	    (deauth == 1 &&
	     nla_put_u8(msg, NL80211_ATTR_MGMT_SUBTYPE,
			WLAN_FC_STYPE_DEAUTH)) ||
	    (reason_code &&
	     nla_put_u16(msg, NL80211_ATTR_REASON_CODE, reason_code))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	wpa_printf(MSG_DEBUG, "nl80211: sta_remove -> DEL_STATION %s " MACSTR
		   " --> %d (%s)",
		   bss->ifname, MAC2STR(addr), ret, strerror(-ret));

	if (drv->rtnl_sk)
		rtnl_neigh_delete_fdb_entry(bss, addr);

	if (ret == -ENOENT)
		return 0;
	return ret;
}


void nl80211_remove_iface(struct wpa_driver_nl80211_data *drv, int ifidx)
{
	struct nl_msg *msg;
	struct wpa_driver_nl80211_data *drv2;

	wpa_printf(MSG_DEBUG, "nl80211: Remove interface ifindex=%d", ifidx);

	/* stop listening for EAPOL on this interface */
	dl_list_for_each(drv2, &drv->global->interfaces,
			 struct wpa_driver_nl80211_data, list)
		del_ifidx(drv2, ifidx);

	msg = nl80211_ifindex_msg(drv, ifidx, 0, NL80211_CMD_DEL_INTERFACE);
	if (send_and_recv_msgs(drv, msg, NULL, NULL) == 0)
		return;
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
	case NL80211_IFTYPE_AP_VLAN:
		return "AP_VLAN";
	case NL80211_IFTYPE_WDS:
		return "WDS";
	case NL80211_IFTYPE_MONITOR:
		return "MONITOR";
	case NL80211_IFTYPE_MESH_POINT:
		return "MESH_POINT";
	case NL80211_IFTYPE_P2P_CLIENT:
		return "P2P_CLIENT";
	case NL80211_IFTYPE_P2P_GO:
		return "P2P_GO";
	case NL80211_IFTYPE_P2P_DEVICE:
		return "P2P_DEVICE";
	default:
		return "unknown";
	}
}


static int nl80211_create_iface_once(struct wpa_driver_nl80211_data *drv,
				     const char *ifname,
				     enum nl80211_iftype iftype,
				     const u8 *addr, int wds,
				     int (*handler)(struct nl_msg *, void *),
				     void *arg)
{
	struct nl_msg *msg;
	int ifidx;
	int ret = -ENOBUFS;

	wpa_printf(MSG_DEBUG, "nl80211: Create interface iftype %d (%s)",
		   iftype, nl80211_iftype_str(iftype));

	msg = nl80211_cmd_msg(drv->first_bss, 0, NL80211_CMD_NEW_INTERFACE);
	if (!msg ||
	    nla_put_string(msg, NL80211_ATTR_IFNAME, ifname) ||
	    nla_put_u32(msg, NL80211_ATTR_IFTYPE, iftype))
		goto fail;

	if (iftype == NL80211_IFTYPE_MONITOR) {
		struct nlattr *flags;

		flags = nla_nest_start(msg, NL80211_ATTR_MNTR_FLAGS);
		if (!flags ||
		    nla_put_flag(msg, NL80211_MNTR_FLAG_COOK_FRAMES))
			goto fail;

		nla_nest_end(msg, flags);
	} else if (wds) {
		if (nla_put_u8(msg, NL80211_ATTR_4ADDR, wds))
			goto fail;
	}

	/*
	 * Tell cfg80211 that the interface belongs to the socket that created
	 * it, and the interface should be deleted when the socket is closed.
	 */
	if (nla_put_flag(msg, NL80211_ATTR_IFACE_SOCKET_OWNER))
		goto fail;

	ret = send_and_recv_msgs(drv, msg, handler, arg);
	msg = NULL;
	if (ret) {
	fail:
		nlmsg_free(msg);
		wpa_printf(MSG_ERROR, "Failed to create interface %s: %d (%s)",
			   ifname, ret, strerror(-ret));
		return ret;
	}

	if (iftype == NL80211_IFTYPE_P2P_DEVICE)
		return 0;

	ifidx = if_nametoindex(ifname);
	wpa_printf(MSG_DEBUG, "nl80211: New interface %s created: ifindex=%d",
		   ifname, ifidx);

	if (ifidx <= 0)
		return -1;

	/*
	 * Some virtual interfaces need to process EAPOL packets and events on
	 * the parent interface. This is used mainly with hostapd.
	 */
	if (drv->hostapd ||
	    iftype == NL80211_IFTYPE_AP_VLAN ||
	    iftype == NL80211_IFTYPE_WDS ||
	    iftype == NL80211_IFTYPE_MONITOR) {
		/* start listening for EAPOL on this interface */
		add_ifidx(drv, ifidx);
	}

	if (addr && iftype != NL80211_IFTYPE_MONITOR &&
	    linux_set_ifhwaddr(drv->global->ioctl_sock, ifname, addr)) {
		nl80211_remove_iface(drv, ifidx);
		return -1;
	}

	return ifidx;
}


int nl80211_create_iface(struct wpa_driver_nl80211_data *drv,
			 const char *ifname, enum nl80211_iftype iftype,
			 const u8 *addr, int wds,
			 int (*handler)(struct nl_msg *, void *),
			 void *arg, int use_existing)
{
	int ret;

	ret = nl80211_create_iface_once(drv, ifname, iftype, addr, wds, handler,
					arg);

	/* if error occurred and interface exists already */
	if (ret == -ENFILE && if_nametoindex(ifname)) {
		if (use_existing) {
			wpa_printf(MSG_DEBUG, "nl80211: Continue using existing interface %s",
				   ifname);
			if (addr && iftype != NL80211_IFTYPE_MONITOR &&
			    linux_set_ifhwaddr(drv->global->ioctl_sock, ifname,
					       addr) < 0 &&
			    (linux_set_iface_flags(drv->global->ioctl_sock,
						   ifname, 0) < 0 ||
			     linux_set_ifhwaddr(drv->global->ioctl_sock, ifname,
						addr) < 0 ||
			     linux_set_iface_flags(drv->global->ioctl_sock,
						   ifname, 1) < 0))
					return -1;
			return -ENFILE;
		}
		wpa_printf(MSG_INFO, "Try to remove and re-create %s", ifname);

		/* Try to remove the interface that was already there. */
		nl80211_remove_iface(drv, if_nametoindex(ifname));

		/* Try to create the interface again */
		ret = nl80211_create_iface_once(drv, ifname, iftype, addr,
						wds, handler, arg);
	}

	if (ret >= 0 && is_p2p_net_interface(iftype)) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Interface %s created for P2P - disable 11b rates",
			   ifname);
		nl80211_disable_11b_rates(drv, ret, 1);
	}

	return ret;
}


static int nl80211_setup_ap(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	wpa_printf(MSG_DEBUG, "nl80211: Setup AP(%s) - device_ap_sme=%d use_monitor=%d",
		   bss->ifname, drv->device_ap_sme, drv->use_monitor);

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

	wpa_printf(MSG_DEBUG, "nl80211: Teardown AP(%s) - device_ap_sme=%d use_monitor=%d",
		   bss->ifname, drv->device_ap_sme, drv->use_monitor);
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
		wpa_printf(MSG_INFO, "nl80211: Failed to allocate EAPOL buffer(len=%lu)",
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
	struct nl_msg *msg;
	struct nlattr *flags;
	struct nl80211_sta_flag_update upd;

	wpa_printf(MSG_DEBUG, "nl80211: Set STA flags - ifname=%s addr=" MACSTR
		   " total_flags=0x%x flags_or=0x%x flags_and=0x%x authorized=%d",
		   bss->ifname, MAC2STR(addr), total_flags, flags_or, flags_and,
		   !!(total_flags & WPA_STA_AUTHORIZED));

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr))
		goto fail;

	/*
	 * Backwards compatibility version using NL80211_ATTR_STA_FLAGS. This
	 * can be removed eventually.
	 */
	flags = nla_nest_start(msg, NL80211_ATTR_STA_FLAGS);
	if (!flags ||
	    ((total_flags & WPA_STA_AUTHORIZED) &&
	     nla_put_flag(msg, NL80211_STA_FLAG_AUTHORIZED)) ||
	    ((total_flags & WPA_STA_WMM) &&
	     nla_put_flag(msg, NL80211_STA_FLAG_WME)) ||
	    ((total_flags & WPA_STA_SHORT_PREAMBLE) &&
	     nla_put_flag(msg, NL80211_STA_FLAG_SHORT_PREAMBLE)) ||
	    ((total_flags & WPA_STA_MFP) &&
	     nla_put_flag(msg, NL80211_STA_FLAG_MFP)) ||
	    ((total_flags & WPA_STA_TDLS_PEER) &&
	     nla_put_flag(msg, NL80211_STA_FLAG_TDLS_PEER)))
		goto fail;

	nla_nest_end(msg, flags);

	os_memset(&upd, 0, sizeof(upd));
	upd.mask = sta_flags_nl80211(flags_or | ~flags_and);
	upd.set = sta_flags_nl80211(flags_or);
	if (nla_put(msg, NL80211_ATTR_STA_FLAGS2, sizeof(upd), &upd))
		goto fail;

	return send_and_recv_msgs(bss->drv, msg, NULL, NULL);
fail:
	nlmsg_free(msg);
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
	if (wpa_driver_nl80211_set_mode(drv->first_bss, nlmode)) {
		nl80211_remove_monitor_interface(drv);
		return -1;
	}

	if (params->freq.freq &&
	    nl80211_set_channel(drv->first_bss, &params->freq, 0)) {
		if (old_mode != nlmode)
			wpa_driver_nl80211_set_mode(drv->first_bss, old_mode);
		nl80211_remove_monitor_interface(drv);
		return -1;
	}

	return 0;
}


static int nl80211_leave_ibss(struct wpa_driver_nl80211_data *drv,
			      int reset_mode)
{
	struct nl_msg *msg;
	int ret;

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_LEAVE_IBSS);
	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Leave IBSS failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Leave IBSS request sent successfully");
	}

	if (reset_mode &&
	    wpa_driver_nl80211_set_mode(drv->first_bss,
					NL80211_IFTYPE_STATION)) {
		wpa_printf(MSG_INFO, "nl80211: Failed to set interface into "
			   "station mode");
	}

	return ret;
}


static int nl80211_ht_vht_overrides(struct nl_msg *msg,
				    struct wpa_driver_associate_params *params)
{
	if (params->disable_ht && nla_put_flag(msg, NL80211_ATTR_DISABLE_HT))
		return -1;

	if (params->htcaps && params->htcaps_mask) {
		int sz = sizeof(struct ieee80211_ht_capabilities);
		wpa_hexdump(MSG_DEBUG, "  * htcaps", params->htcaps, sz);
		wpa_hexdump(MSG_DEBUG, "  * htcaps_mask",
			    params->htcaps_mask, sz);
		if (nla_put(msg, NL80211_ATTR_HT_CAPABILITY, sz,
			    params->htcaps) ||
		    nla_put(msg, NL80211_ATTR_HT_CAPABILITY_MASK, sz,
			    params->htcaps_mask))
			return -1;
	}

#ifdef CONFIG_VHT_OVERRIDES
	if (params->disable_vht) {
		wpa_printf(MSG_DEBUG, "  * VHT disabled");
		if (nla_put_flag(msg, NL80211_ATTR_DISABLE_VHT))
			return -1;
	}

	if (params->vhtcaps && params->vhtcaps_mask) {
		int sz = sizeof(struct ieee80211_vht_capabilities);
		wpa_hexdump(MSG_DEBUG, "  * vhtcaps", params->vhtcaps, sz);
		wpa_hexdump(MSG_DEBUG, "  * vhtcaps_mask",
			    params->vhtcaps_mask, sz);
		if (nla_put(msg, NL80211_ATTR_VHT_CAPABILITY, sz,
			    params->vhtcaps) ||
		    nla_put(msg, NL80211_ATTR_VHT_CAPABILITY_MASK, sz,
			    params->vhtcaps_mask))
			return -1;
	}
#endif /* CONFIG_VHT_OVERRIDES */

	return 0;
}


static int wpa_driver_nl80211_ibss(struct wpa_driver_nl80211_data *drv,
				   struct wpa_driver_associate_params *params)
{
	struct nl_msg *msg;
	int ret = -1;
	int count = 0;

	wpa_printf(MSG_DEBUG, "nl80211: Join IBSS (ifindex=%d)", drv->ifindex);

	if (wpa_driver_nl80211_set_mode_ibss(drv->first_bss, &params->freq)) {
		wpa_printf(MSG_INFO, "nl80211: Failed to set interface into "
			   "IBSS mode");
		return -1;
	}

retry:
	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_JOIN_IBSS)) ||
	    params->ssid == NULL || params->ssid_len > sizeof(drv->ssid))
		goto fail;

	wpa_hexdump_ascii(MSG_DEBUG, "  * SSID",
			  params->ssid, params->ssid_len);
	if (nla_put(msg, NL80211_ATTR_SSID, params->ssid_len, params->ssid))
		goto fail;
	os_memcpy(drv->ssid, params->ssid, params->ssid_len);
	drv->ssid_len = params->ssid_len;

	if (nl80211_put_freq_params(msg, &params->freq) < 0 ||
	    nl80211_put_beacon_int(msg, params->beacon_int))
		goto fail;

	ret = nl80211_set_conn_keys(params, msg);
	if (ret)
		goto fail;

	if (params->bssid && params->fixed_bssid) {
		wpa_printf(MSG_DEBUG, "  * BSSID=" MACSTR,
			   MAC2STR(params->bssid));
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid))
			goto fail;
	}

	if (params->fixed_freq) {
		wpa_printf(MSG_DEBUG, "  * fixed_freq");
		if (nla_put_flag(msg, NL80211_ATTR_FREQ_FIXED))
			goto fail;
	}

	if (params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_PSK ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X_SHA256 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_PSK_SHA256) {
		wpa_printf(MSG_DEBUG, "  * control port");
		if (nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT))
			goto fail;
	}

	if (params->wpa_ie) {
		wpa_hexdump(MSG_DEBUG,
			    "  * Extra IEs for Beacon/Probe Response frames",
			    params->wpa_ie, params->wpa_ie_len);
		if (nla_put(msg, NL80211_ATTR_IE, params->wpa_ie_len,
			    params->wpa_ie))
			goto fail;
	}

	if (nl80211_ht_vht_overrides(msg, params) < 0)
		return -1;

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Join IBSS failed: ret=%d (%s)",
			   ret, strerror(-ret));
		count++;
		if (ret == -EALREADY && count == 1) {
			wpa_printf(MSG_DEBUG, "nl80211: Retry IBSS join after "
				   "forced leave");
			nl80211_leave_ibss(drv, 0);
			nlmsg_free(msg);
			goto retry;
		}
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Join IBSS request sent successfully");
	}

fail:
	nlmsg_free(msg);
	return ret;
}


static int nl80211_connect_common(struct wpa_driver_nl80211_data *drv,
				  struct wpa_driver_associate_params *params,
				  struct nl_msg *msg)
{
	if (params->bssid) {
		wpa_printf(MSG_DEBUG, "  * bssid=" MACSTR,
			   MAC2STR(params->bssid));
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid))
			return -1;
	}

	if (params->bssid_hint) {
		wpa_printf(MSG_DEBUG, "  * bssid_hint=" MACSTR,
			   MAC2STR(params->bssid_hint));
		if (nla_put(msg, NL80211_ATTR_MAC_HINT, ETH_ALEN,
			    params->bssid_hint))
			return -1;
	}

	if (params->freq.freq) {
		wpa_printf(MSG_DEBUG, "  * freq=%d", params->freq.freq);
		if (nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ,
				params->freq.freq))
			return -1;
		drv->assoc_freq = params->freq.freq;
	} else
		drv->assoc_freq = 0;

	if (params->freq_hint) {
		wpa_printf(MSG_DEBUG, "  * freq_hint=%d", params->freq_hint);
		if (nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ_HINT,
				params->freq_hint))
			return -1;
	}

	if (params->bg_scan_period >= 0) {
		wpa_printf(MSG_DEBUG, "  * bg scan period=%d",
			   params->bg_scan_period);
		if (nla_put_u16(msg, NL80211_ATTR_BG_SCAN_PERIOD,
				params->bg_scan_period))
			return -1;
	}

	if (params->ssid) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * SSID",
				  params->ssid, params->ssid_len);
		if (nla_put(msg, NL80211_ATTR_SSID, params->ssid_len,
			    params->ssid))
			return -1;
		if (params->ssid_len > sizeof(drv->ssid))
			return -1;
		os_memcpy(drv->ssid, params->ssid, params->ssid_len);
		drv->ssid_len = params->ssid_len;
	}

	wpa_hexdump(MSG_DEBUG, "  * IEs", params->wpa_ie, params->wpa_ie_len);
	if (params->wpa_ie &&
	    nla_put(msg, NL80211_ATTR_IE, params->wpa_ie_len, params->wpa_ie))
		return -1;

	if (params->wpa_proto) {
		enum nl80211_wpa_versions ver = 0;

		if (params->wpa_proto & WPA_PROTO_WPA)
			ver |= NL80211_WPA_VERSION_1;
		if (params->wpa_proto & WPA_PROTO_RSN)
			ver |= NL80211_WPA_VERSION_2;

		wpa_printf(MSG_DEBUG, "  * WPA Versions 0x%x", ver);
		if (nla_put_u32(msg, NL80211_ATTR_WPA_VERSIONS, ver))
			return -1;
	}

	if (params->pairwise_suite != WPA_CIPHER_NONE) {
		u32 cipher = wpa_cipher_to_cipher_suite(params->pairwise_suite);
		wpa_printf(MSG_DEBUG, "  * pairwise=0x%x", cipher);
		if (nla_put_u32(msg, NL80211_ATTR_CIPHER_SUITES_PAIRWISE,
				cipher))
			return -1;
	}

	if (params->group_suite == WPA_CIPHER_GTK_NOT_USED &&
	    !(drv->capa.enc & WPA_DRIVER_CAPA_ENC_GTK_NOT_USED)) {
		/*
		 * This is likely to work even though many drivers do not
		 * advertise support for operations without GTK.
		 */
		wpa_printf(MSG_DEBUG, "  * skip group cipher configuration for GTK_NOT_USED due to missing driver support advertisement");
	} else if (params->group_suite != WPA_CIPHER_NONE) {
		u32 cipher = wpa_cipher_to_cipher_suite(params->group_suite);
		wpa_printf(MSG_DEBUG, "  * group=0x%x", cipher);
		if (nla_put_u32(msg, NL80211_ATTR_CIPHER_SUITE_GROUP, cipher))
			return -1;
	}

	if (params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_PSK ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FT_IEEE8021X ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FT_PSK ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_CCKM ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_OSEN ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X_SHA256 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_PSK_SHA256 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X_SUITE_B ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) {
		int mgmt = WLAN_AKM_SUITE_PSK;

		switch (params->key_mgmt_suite) {
		case WPA_KEY_MGMT_CCKM:
			mgmt = WLAN_AKM_SUITE_CCKM;
			break;
		case WPA_KEY_MGMT_IEEE8021X:
			mgmt = WLAN_AKM_SUITE_8021X;
			break;
		case WPA_KEY_MGMT_FT_IEEE8021X:
			mgmt = WLAN_AKM_SUITE_FT_8021X;
			break;
		case WPA_KEY_MGMT_FT_PSK:
			mgmt = WLAN_AKM_SUITE_FT_PSK;
			break;
		case WPA_KEY_MGMT_IEEE8021X_SHA256:
			mgmt = WLAN_AKM_SUITE_8021X_SHA256;
			break;
		case WPA_KEY_MGMT_PSK_SHA256:
			mgmt = WLAN_AKM_SUITE_PSK_SHA256;
			break;
		case WPA_KEY_MGMT_OSEN:
			mgmt = WLAN_AKM_SUITE_OSEN;
			break;
		case WPA_KEY_MGMT_IEEE8021X_SUITE_B:
			mgmt = WLAN_AKM_SUITE_8021X_SUITE_B;
			break;
		case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
			mgmt = WLAN_AKM_SUITE_8021X_SUITE_B_192;
			break;
		case WPA_KEY_MGMT_PSK:
		default:
			mgmt = WLAN_AKM_SUITE_PSK;
			break;
		}
		wpa_printf(MSG_DEBUG, "  * akm=0x%x", mgmt);
		if (nla_put_u32(msg, NL80211_ATTR_AKM_SUITES, mgmt))
			return -1;
	}

	if (nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT))
		return -1;

	if (params->mgmt_frame_protection == MGMT_FRAME_PROTECTION_REQUIRED &&
	    nla_put_u32(msg, NL80211_ATTR_USE_MFP, NL80211_MFP_REQUIRED))
		return -1;

	if (params->rrm_used) {
		u32 drv_rrm_flags = drv->capa.rrm_flags;
		if (!(drv_rrm_flags &
		      WPA_DRIVER_FLAGS_DS_PARAM_SET_IE_IN_PROBES) ||
		    !(drv_rrm_flags & WPA_DRIVER_FLAGS_QUIET) ||
		    nla_put_flag(msg, NL80211_ATTR_USE_RRM))
			return -1;
	}

	if (nl80211_ht_vht_overrides(msg, params) < 0)
		return -1;

	if (params->p2p)
		wpa_printf(MSG_DEBUG, "  * P2P group");

	return 0;
}


static int wpa_driver_nl80211_try_connect(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_driver_associate_params *params)
{
	struct nl_msg *msg;
	enum nl80211_auth_type type;
	int ret;
	int algs;

	if (params->req_key_mgmt_offload && params->psk &&
	    (params->key_mgmt_suite == WPA_KEY_MGMT_PSK ||
	     params->key_mgmt_suite == WPA_KEY_MGMT_PSK_SHA256 ||
	     params->key_mgmt_suite == WPA_KEY_MGMT_FT_PSK)) {
		wpa_printf(MSG_DEBUG, "nl80211: Key management set PSK");
		ret = issue_key_mgmt_set_key(drv, params->psk, 32);
		if (ret)
			return ret;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Connect (ifindex=%d)", drv->ifindex);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_CONNECT);
	if (!msg)
		return -1;

	ret = nl80211_connect_common(drv, params, msg);
	if (ret)
		goto fail;

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
		goto fail;

	wpa_printf(MSG_DEBUG, "  * Auth Type %d", type);
	if (nla_put_u32(msg, NL80211_ATTR_AUTH_TYPE, type))
		goto fail;

skip_auth_type:
	ret = nl80211_set_conn_keys(params, msg);
	if (ret)
		goto fail;

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: MLME connect failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Connect request send successfully");
	}

fail:
	nlmsg_free(msg);
	return ret;

}


static int wpa_driver_nl80211_connect(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_driver_associate_params *params)
{
	int ret;

	/* Store the connection attempted bssid for future use */
	if (params->bssid)
		os_memcpy(drv->auth_attempt_bssid, params->bssid, ETH_ALEN);
	else
		os_memset(drv->auth_attempt_bssid, 0, ETH_ALEN);

	ret = wpa_driver_nl80211_try_connect(drv, params);
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

	nl80211_unmask_11b_rates(bss);

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

	nl80211_mark_disconnected(drv);

	wpa_printf(MSG_DEBUG, "nl80211: Associate (ifindex=%d)",
		   drv->ifindex);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_ASSOCIATE);
	if (!msg)
		return -1;

	ret = nl80211_connect_common(drv, params, msg);
	if (ret)
		goto fail;

	if (params->prev_bssid) {
		wpa_printf(MSG_DEBUG, "  * prev_bssid=" MACSTR,
			   MAC2STR(params->prev_bssid));
		if (nla_put(msg, NL80211_ATTR_PREV_BSSID, ETH_ALEN,
			    params->prev_bssid))
			goto fail;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: MLME command failed (assoc): ret=%d (%s)",
			ret, strerror(-ret));
		nl80211_dump_scan(drv);
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Association request send successfully");
	}

fail:
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

	msg = nl80211_cmd_msg(drv->first_bss, 0, NL80211_CMD_SET_INTERFACE);
	if (!msg || nla_put_u32(msg, NL80211_ATTR_IFTYPE, mode))
		goto fail;

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (!ret)
		return 0;
fail:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set interface %d to mode %d:"
		   " %d (%s)", ifindex, mode, ret, strerror(-ret));
	return ret;
}


static int wpa_driver_nl80211_set_mode_impl(
		struct i802_bss *bss,
		enum nl80211_iftype nlmode,
		struct hostapd_freq_params *desired_freq_params)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -1;
	int i;
	int was_ap = is_ap_interface(drv->nlmode);
	int res;
	int mode_switch_res;

	mode_switch_res = nl80211_set_mode(drv, drv->ifindex, nlmode);
	if (mode_switch_res && nlmode == nl80211_get_ifmode(bss))
		mode_switch_res = 0;

	if (mode_switch_res == 0) {
		drv->nlmode = nlmode;
		ret = 0;
		goto done;
	}

	if (mode_switch_res == -ENODEV)
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
		res = i802_set_iface_flags(bss, 0);
		if (res == -EACCES || res == -ENODEV)
			break;
		if (res != 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Failed to set "
				   "interface down");
			os_sleep(0, 100000);
			continue;
		}

		/*
		 * Setting the mode will fail for some drivers if the phy is
		 * on a frequency that the mode is disallowed in.
		 */
		if (desired_freq_params) {
			res = nl80211_set_channel(bss, desired_freq_params, 0);
			if (res) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Failed to set frequency on interface");
			}
		}

		/* Try to set the mode again while the interface is down */
		mode_switch_res = nl80211_set_mode(drv, drv->ifindex, nlmode);
		if (mode_switch_res == -EBUSY) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Delaying mode set while interface going down");
			os_sleep(0, 100000);
			continue;
		}
		ret = mode_switch_res;
		break;
	}

	if (!ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Mode change succeeded while "
			   "interface is down");
		drv->nlmode = nlmode;
		drv->ignore_if_down_event = 1;
	}

	/* Bring the interface back up */
	res = linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1);
	if (res != 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Failed to set interface up after switching mode");
		ret = -1;
	}

done:
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Interface mode change to %d "
			   "from %d failed", nlmode, drv->nlmode);
		return ret;
	}

	if (is_p2p_net_interface(nlmode)) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Interface %s mode change to P2P - disable 11b rates",
			   bss->ifname);
		nl80211_disable_11b_rates(drv, drv->ifindex, 1);
	} else if (drv->disabled_11b_rates) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Interface %s mode changed to non-P2P - re-enable 11b rates",
			   bss->ifname);
		nl80211_disable_11b_rates(drv, drv->ifindex, 0);
	}

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

	if (is_mesh_interface(nlmode) &&
	    nl80211_mgmt_subscribe_mesh(bss))
		return -1;

	if (!bss->in_deinit && !is_ap_interface(nlmode) &&
	    !is_mesh_interface(nlmode) &&
	    nl80211_mgmt_subscribe_non_ap(bss) < 0)
		wpa_printf(MSG_DEBUG, "nl80211: Failed to register Action "
			   "frame processing - ignore for now");

	return 0;
}


int wpa_driver_nl80211_set_mode(struct i802_bss *bss,
				enum nl80211_iftype nlmode)
{
	return wpa_driver_nl80211_set_mode_impl(bss, nlmode, NULL);
}


static int wpa_driver_nl80211_set_mode_ibss(struct i802_bss *bss,
					    struct hostapd_freq_params *freq)
{
	return wpa_driver_nl80211_set_mode_impl(bss, NL80211_IFTYPE_ADHOC,
						freq);
}


static int wpa_driver_nl80211_get_capa(void *priv,
				       struct wpa_driver_capa *capa)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (!drv->has_capability)
		return -1;
	os_memcpy(capa, &drv->capa, sizeof(*capa));
	if (drv->extended_capa && drv->extended_capa_mask) {
		capa->extended_capa = drv->extended_capa;
		capa->extended_capa_mask = drv->extended_capa_mask;
		capa->extended_capa_len = drv->extended_capa_len;
	}

	return 0;
}


static int wpa_driver_nl80211_set_operstate(void *priv, int state)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	wpa_printf(MSG_DEBUG, "nl80211: Set %s operstate %d->%d (%s)",
		   bss->ifname, drv->operstate, state,
		   state ? "UP" : "DORMANT");
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
	int ret;

	if (!drv->associated && is_zero_ether_addr(drv->bssid) && !authorized) {
		wpa_printf(MSG_DEBUG, "nl80211: Skip set_supp_port(unauthorized) while not associated");
		return 0;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Set supplicant port %sauthorized for "
		   MACSTR, authorized ? "" : "un", MAC2STR(drv->bssid));

	os_memset(&upd, 0, sizeof(upd));
	upd.mask = BIT(NL80211_STA_FLAG_AUTHORIZED);
	if (authorized)
		upd.set = BIT(NL80211_STA_FLAG_AUTHORIZED);

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, drv->bssid) ||
	    nla_put(msg, NL80211_ATTR_STA_FLAGS2, sizeof(upd), &upd)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (!ret)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set STA flag: %d (%s)",
		   ret, strerror(-ret));
	return ret;
}


/* Set kernel driver on given frequency (MHz) */
static int i802_set_freq(void *priv, struct hostapd_freq_params *freq)
{
	struct i802_bss *bss = priv;
	return nl80211_set_channel(bss, freq, 0);
}


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

	msg = nl80211_ifindex_msg(drv, if_nametoindex(iface), 0,
				  NL80211_CMD_GET_KEY);
	if (!msg ||
	    (addr && nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) ||
	    nla_put_u8(msg, NL80211_ATTR_KEY_IDX, idx)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	memset(seq, 0, 6);

	return send_and_recv_msgs(drv, msg, get_key_handler, seq);
}


static int i802_set_rts(void *priv, int rts)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	u32 val;

	if (rts >= 2347)
		val = (u32) -1;
	else
		val = rts;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_WIPHY)) ||
	    nla_put_u32(msg, NL80211_ATTR_WIPHY_RTS_THRESHOLD, val)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (!ret)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set RTS threshold %d: "
		   "%d (%s)", rts, ret, strerror(-ret));
	return ret;
}


static int i802_set_frag(void *priv, int frag)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	u32 val;

	if (frag >= 2346)
		val = (u32) -1;
	else
		val = frag;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_WIPHY)) ||
	    nla_put_u32(msg, NL80211_ATTR_WIPHY_FRAG_THRESHOLD, val)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (!ret)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: Failed to set fragmentation threshold "
		   "%d: %d (%s)", frag, ret, strerror(-ret));
	return ret;
}


static int i802_flush(void *priv)
{
	struct i802_bss *bss = priv;
	struct nl_msg *msg;
	int res;

	wpa_printf(MSG_DEBUG, "nl80211: flush -> DEL_STATION %s (all)",
		   bss->ifname);

	/*
	 * XXX: FIX! this needs to flush all VLANs too
	 */
	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_DEL_STATION);
	res = send_and_recv_msgs(bss->drv, msg, NULL, NULL);
	if (res) {
		wpa_printf(MSG_DEBUG, "nl80211: Station flush failed: ret=%d "
			   "(%s)", res, strerror(-res));
	}
	return res;
}


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

static int i802_read_sta_data(struct i802_bss *bss,
			      struct hostap_sta_driver_data *data,
			      const u8 *addr)
{
	struct nl_msg *msg;

	os_memset(data, 0, sizeof(*data));

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_GET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(bss->drv, msg, get_sta_handler, data);
}


static int i802_set_tx_queue_params(void *priv, int queue, int aifs,
				    int cw_min, int cw_max, int burst_time)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *txq, *params;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_WIPHY);
	if (!msg)
		return -1;

	txq = nla_nest_start(msg, NL80211_ATTR_WIPHY_TXQ_PARAMS);
	if (!txq)
		goto fail;

	/* We are only sending parameters for a single TXQ at a time */
	params = nla_nest_start(msg, 1);
	if (!params)
		goto fail;

	switch (queue) {
	case 0:
		if (nla_put_u8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_VO))
			goto fail;
		break;
	case 1:
		if (nla_put_u8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_VI))
			goto fail;
		break;
	case 2:
		if (nla_put_u8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_BE))
			goto fail;
		break;
	case 3:
		if (nla_put_u8(msg, NL80211_TXQ_ATTR_QUEUE, NL80211_TXQ_Q_BK))
			goto fail;
		break;
	}
	/* Burst time is configured in units of 0.1 msec and TXOP parameter in
	 * 32 usec, so need to convert the value here. */
	if (nla_put_u16(msg, NL80211_TXQ_ATTR_TXOP,
			(burst_time * 100 + 16) / 32) ||
	    nla_put_u16(msg, NL80211_TXQ_ATTR_CWMIN, cw_min) ||
	    nla_put_u16(msg, NL80211_TXQ_ATTR_CWMAX, cw_max) ||
	    nla_put_u8(msg, NL80211_TXQ_ATTR_AIFS, aifs))
		goto fail;

	nla_nest_end(msg, params);

	nla_nest_end(msg, txq);

	if (send_and_recv_msgs(drv, msg, NULL, NULL) == 0)
		return 0;
	msg = NULL;
fail:
	nlmsg_free(msg);
	return -1;
}


static int i802_set_sta_vlan(struct i802_bss *bss, const u8 *addr,
			     const char *ifname, int vlan_id)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: %s[%d]: set_sta_vlan(" MACSTR
		   ", ifname=%s[%d], vlan_id=%d)",
		   bss->ifname, if_nametoindex(bss->ifname),
		   MAC2STR(addr), ifname, if_nametoindex(ifname), vlan_id);
	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
	    nla_put_u32(msg, NL80211_ATTR_STA_VLAN, if_nametoindex(ifname))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "nl80211: NL80211_ATTR_STA_VLAN (addr="
			   MACSTR " ifname=%s vlan_id=%d) failed: %d (%s)",
			   MAC2STR(addr), ifname, vlan_id, ret,
			   strerror(-ret));
	}
	return ret;
}


static int i802_get_inact_sec(void *priv, const u8 *addr)
{
	struct hostap_sta_driver_data data;
	int ret;

	data.inactive_msec = (unsigned long) -1;
	ret = i802_read_sta_data(priv, &data, addr);
	if (ret == -ENOENT)
		return -ENOENT;
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

	if (is_mesh_interface(drv->nlmode))
		return -1;

	if (drv->device_ap_sme)
		return wpa_driver_nl80211_sta_remove(bss, addr, 1, reason);

	memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_DEAUTH);
	memcpy(mgmt.da, addr, ETH_ALEN);
	memcpy(mgmt.sa, own_addr, ETH_ALEN);
	memcpy(mgmt.bssid, own_addr, ETH_ALEN);
	mgmt.u.deauth.reason_code = host_to_le16(reason);
	return wpa_driver_nl80211_send_mlme(bss, (u8 *) &mgmt,
					    IEEE80211_HDRLEN +
					    sizeof(mgmt.u.deauth), 0, 0, 0, 0,
					    0);
}


static int i802_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
			     int reason)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_mgmt mgmt;

	if (is_mesh_interface(drv->nlmode))
		return -1;

	if (drv->device_ap_sme)
		return wpa_driver_nl80211_sta_remove(bss, addr, 0, reason);

	memset(&mgmt, 0, sizeof(mgmt));
	mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					  WLAN_FC_STYPE_DISASSOC);
	memcpy(mgmt.da, addr, ETH_ALEN);
	memcpy(mgmt.sa, own_addr, ETH_ALEN);
	memcpy(mgmt.bssid, own_addr, ETH_ALEN);
	mgmt.u.disassoc.reason_code = host_to_le16(reason);
	return wpa_driver_nl80211_send_mlme(bss, (u8 *) &mgmt,
					    IEEE80211_HDRLEN +
					    sizeof(mgmt.u.disassoc), 0, 0, 0, 0,
					    0);
}


static void dump_ifidx(struct wpa_driver_nl80211_data *drv)
{
	char buf[200], *pos, *end;
	int i, res;

	pos = buf;
	end = pos + sizeof(buf);

	for (i = 0; i < drv->num_if_indices; i++) {
		if (!drv->if_indices[i])
			continue;
		res = os_snprintf(pos, end - pos, " %d", drv->if_indices[i]);
		if (os_snprintf_error(end - pos, res))
			break;
		pos += res;
	}
	*pos = '\0';

	wpa_printf(MSG_DEBUG, "nl80211: if_indices[%d]:%s",
		   drv->num_if_indices, buf);
}


static void add_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx)
{
	int i;
	int *old;

	wpa_printf(MSG_DEBUG, "nl80211: Add own interface ifindex %d",
		   ifidx);
	if (have_ifidx(drv, ifidx)) {
		wpa_printf(MSG_DEBUG, "nl80211: ifindex %d already in the list",
			   ifidx);
		return;
	}
	for (i = 0; i < drv->num_if_indices; i++) {
		if (drv->if_indices[i] == 0) {
			drv->if_indices[i] = ifidx;
			dump_ifidx(drv);
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
	dump_ifidx(drv);
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
	dump_ifidx(drv);
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
			    const char *bridge_ifname, char *ifname_wds)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	char name[IFNAMSIZ + 1];

	os_snprintf(name, sizeof(name), "%s.sta%d", bss->ifname, aid);
	if (ifname_wds)
		os_strlcpy(ifname_wds, name, IFNAMSIZ + 1);

	wpa_printf(MSG_DEBUG, "nl80211: Set WDS STA addr=" MACSTR
		   " aid=%d val=%d name=%s", MAC2STR(addr), aid, val, name);
	if (val) {
		if (!if_nametoindex(name)) {
			if (nl80211_create_iface(drv, name,
						 NL80211_IFTYPE_AP_VLAN,
						 bss->addr, 1, NULL, NULL, 0) <
			    0)
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
		nl80211_remove_iface(drv, if_nametoindex(name));
		return 0;
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
		wpa_printf(MSG_ERROR, "nl80211: EAPOL recv failed: %s",
			   strerror(errno));
		return;
	}

	if (have_ifidx(drv, lladdr.sll_ifindex))
		drv_event_eapol_rx(drv->ctx, lladdr.sll_addr, buf, len);
}


static int i802_check_bridge(struct wpa_driver_nl80211_data *drv,
			     struct i802_bss *bss,
			     const char *brname, const char *ifname)
{
	int br_ifindex;
	char in_br[IFNAMSIZ];

	os_strlcpy(bss->brname, brname, IFNAMSIZ);
	br_ifindex = if_nametoindex(brname);
	if (br_ifindex == 0) {
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
		br_ifindex = if_nametoindex(brname);
		add_ifidx(drv, br_ifindex);
	}
	bss->br_ifindex = br_ifindex;

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

	bss = wpa_driver_nl80211_drv_init(hapd, params->ifname,
					  params->global_priv, 1,
					  params->bssid, params->driver_params);
	if (bss == NULL)
		return NULL;

	drv = bss->drv;

	if (linux_br_get(brname, params->ifname) == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Interface %s is in bridge %s",
			   params->ifname, brname);
		br_ifindex = if_nametoindex(brname);
		os_strlcpy(bss->brname, brname, IFNAMSIZ);
	} else {
		brname[0] = '\0';
		br_ifindex = 0;
	}
	bss->br_ifindex = br_ifindex;

	for (i = 0; i < params->num_bridge; i++) {
		if (params->bridge[i]) {
			ifindex = if_nametoindex(params->bridge[i]);
			if (ifindex)
				add_ifidx(drv, ifindex);
			if (ifindex == br_ifindex)
				br_added = 1;
		}
	}

	/* start listening for EAPOL on the default AP interface */
	add_ifidx(drv, drv->ifindex);

	if (params->num_bridge && params->bridge[0]) {
		if (i802_check_bridge(drv, bss, params->bridge[0],
				      params->ifname) < 0)
			goto failed;
		if (os_strcmp(params->bridge[0], brname) != 0)
			br_added = 1;
	}

	if (!br_added && br_ifindex &&
	    (params->num_bridge == 0 || !params->bridge[0]))
		add_ifidx(drv, br_ifindex);

#ifdef CONFIG_LIBNL3_ROUTE
	if (bss->added_if_into_bridge) {
		drv->rtnl_sk = nl_socket_alloc();
		if (drv->rtnl_sk == NULL) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to allocate nl_sock");
			goto failed;
		}

		if (nl_connect(drv->rtnl_sk, NETLINK_ROUTE)) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to connect nl_sock to NETLINK_ROUTE: %s",
				   strerror(errno));
			goto failed;
		}
	}
#endif /* CONFIG_LIBNL3_ROUTE */

	drv->eapol_sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_PAE));
	if (drv->eapol_sock < 0) {
		wpa_printf(MSG_ERROR, "nl80211: socket(PF_PACKET, SOCK_DGRAM, ETH_P_PAE) failed: %s",
			   strerror(errno));
		goto failed;
	}

	if (eloop_register_read_sock(drv->eapol_sock, handle_eapol, drv, NULL))
	{
		wpa_printf(MSG_INFO, "nl80211: Could not register read socket for eapol");
		goto failed;
	}

	if (linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
			       params->own_addr))
		goto failed;
	os_memcpy(drv->perm_addr, params->own_addr, ETH_ALEN);

	memcpy(bss->addr, params->own_addr, ETH_ALEN);

	return bss;

failed:
	wpa_driver_nl80211_deinit(bss);
	return NULL;
}


static void i802_deinit(void *priv)
{
	struct i802_bss *bss = priv;
	wpa_driver_nl80211_deinit(bss);
}


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
	case WPA_IF_P2P_DEVICE:
		return NL80211_IFTYPE_P2P_DEVICE;
	case WPA_IF_MESH:
		return NL80211_IFTYPE_MESH_POINT;
	}
	return -1;
}


#if defined(CONFIG_P2P) || defined(CONFIG_MESH)

static int nl80211_addr_in_use(struct nl80211_global *global, const u8 *addr)
{
	struct wpa_driver_nl80211_data *drv;
	dl_list_for_each(drv, &global->interfaces,
			 struct wpa_driver_nl80211_data, list) {
		if (os_memcmp(addr, drv->first_bss->addr, ETH_ALEN) == 0)
			return 1;
	}
	return 0;
}


static int nl80211_vif_addr(struct wpa_driver_nl80211_data *drv, u8 *new_addr)
{
	unsigned int idx;

	if (!drv->global)
		return -1;

	os_memcpy(new_addr, drv->first_bss->addr, ETH_ALEN);
	for (idx = 0; idx < 64; idx++) {
		new_addr[0] = drv->first_bss->addr[0] | 0x02;
		new_addr[0] ^= idx << 2;
		if (!nl80211_addr_in_use(drv->global, new_addr))
			break;
	}
	if (idx == 64)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: Assigned new virtual interface address "
		   MACSTR, MAC2STR(new_addr));

	return 0;
}

#endif /* CONFIG_P2P || CONFIG_MESH */


struct wdev_info {
	u64 wdev_id;
	int wdev_id_set;
	u8 macaddr[ETH_ALEN];
};

static int nl80211_wdev_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct wdev_info *wi = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (tb[NL80211_ATTR_WDEV]) {
		wi->wdev_id = nla_get_u64(tb[NL80211_ATTR_WDEV]);
		wi->wdev_id_set = 1;
	}

	if (tb[NL80211_ATTR_MAC])
		os_memcpy(wi->macaddr, nla_data(tb[NL80211_ATTR_MAC]),
			  ETH_ALEN);

	return NL_SKIP;
}


static int wpa_driver_nl80211_if_add(void *priv, enum wpa_driver_if_type type,
				     const char *ifname, const u8 *addr,
				     void *bss_ctx, void **drv_priv,
				     char *force_ifname, u8 *if_addr,
				     const char *bridge, int use_existing)
{
	enum nl80211_iftype nlmode;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ifidx;
	int added = 1;

	if (addr)
		os_memcpy(if_addr, addr, ETH_ALEN);
	nlmode = wpa_driver_nl80211_if_type(type);
	if (nlmode == NL80211_IFTYPE_P2P_DEVICE) {
		struct wdev_info p2pdev_info;

		os_memset(&p2pdev_info, 0, sizeof(p2pdev_info));
		ifidx = nl80211_create_iface(drv, ifname, nlmode, addr,
					     0, nl80211_wdev_handler,
					     &p2pdev_info, use_existing);
		if (!p2pdev_info.wdev_id_set || ifidx != 0) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to create a P2P Device interface %s",
				   ifname);
			return -1;
		}

		drv->global->if_add_wdevid = p2pdev_info.wdev_id;
		drv->global->if_add_wdevid_set = p2pdev_info.wdev_id_set;
		if (!is_zero_ether_addr(p2pdev_info.macaddr))
			os_memcpy(if_addr, p2pdev_info.macaddr, ETH_ALEN);
		wpa_printf(MSG_DEBUG, "nl80211: New P2P Device interface %s (0x%llx) created",
			   ifname,
			   (long long unsigned int) p2pdev_info.wdev_id);
	} else {
		ifidx = nl80211_create_iface(drv, ifname, nlmode, addr,
					     0, NULL, NULL, use_existing);
		if (use_existing && ifidx == -ENFILE) {
			added = 0;
			ifidx = if_nametoindex(ifname);
		} else if (ifidx < 0) {
			return -1;
		}
	}

	if (!addr) {
		if (drv->nlmode == NL80211_IFTYPE_P2P_DEVICE)
			os_memcpy(if_addr, bss->addr, ETH_ALEN);
		else if (linux_get_ifhwaddr(drv->global->ioctl_sock,
					    bss->ifname, if_addr) < 0) {
			if (added)
				nl80211_remove_iface(drv, ifidx);
			return -1;
		}
	}

#if defined(CONFIG_P2P) || defined(CONFIG_MESH)
	if (!addr &&
	    (type == WPA_IF_P2P_CLIENT || type == WPA_IF_P2P_GROUP ||
	     type == WPA_IF_P2P_GO || type == WPA_IF_MESH)) {
		/* Enforce unique P2P Interface Address */
		u8 new_addr[ETH_ALEN];

		if (linux_get_ifhwaddr(drv->global->ioctl_sock, ifname,
				       new_addr) < 0) {
			if (added)
				nl80211_remove_iface(drv, ifidx);
			return -1;
		}
		if (nl80211_addr_in_use(drv->global, new_addr)) {
			wpa_printf(MSG_DEBUG, "nl80211: Allocate new address "
				   "for %s interface", type == WPA_IF_MESH ?
				   "mesh" : "P2P group");
			if (nl80211_vif_addr(drv, new_addr) < 0) {
				if (added)
					nl80211_remove_iface(drv, ifidx);
				return -1;
			}
			if (linux_set_ifhwaddr(drv->global->ioctl_sock, ifname,
					       new_addr) < 0) {
				if (added)
					nl80211_remove_iface(drv, ifidx);
				return -1;
			}
		}
		os_memcpy(if_addr, new_addr, ETH_ALEN);
	}
#endif /* CONFIG_P2P || CONFIG_MESH */

	if (type == WPA_IF_AP_BSS) {
		struct i802_bss *new_bss = os_zalloc(sizeof(*new_bss));
		if (new_bss == NULL) {
			if (added)
				nl80211_remove_iface(drv, ifidx);
			return -1;
		}

		if (bridge &&
		    i802_check_bridge(drv, new_bss, bridge, ifname) < 0) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to add the new "
				   "interface %s to a bridge %s",
				   ifname, bridge);
			if (added)
				nl80211_remove_iface(drv, ifidx);
			os_free(new_bss);
			return -1;
		}

		if (linux_set_iface_flags(drv->global->ioctl_sock, ifname, 1))
		{
			if (added)
				nl80211_remove_iface(drv, ifidx);
			os_free(new_bss);
			return -1;
		}
		os_strlcpy(new_bss->ifname, ifname, IFNAMSIZ);
		os_memcpy(new_bss->addr, if_addr, ETH_ALEN);
		new_bss->ifindex = ifidx;
		new_bss->drv = drv;
		new_bss->next = drv->first_bss->next;
		new_bss->freq = drv->first_bss->freq;
		new_bss->ctx = bss_ctx;
		new_bss->added_if = added;
		drv->first_bss->next = new_bss;
		if (drv_priv)
			*drv_priv = new_bss;
		nl80211_init_bss(new_bss);

		/* Subscribe management frames for this WPA_IF_AP_BSS */
		if (nl80211_setup_ap(new_bss))
			return -1;
	}

	if (drv->global)
		drv->global->if_add_ifindex = ifidx;

	/*
	 * Some virtual interfaces need to process EAPOL packets and events on
	 * the parent interface. This is used mainly with hostapd.
	 */
	if (ifidx > 0 &&
	    (drv->hostapd ||
	     nlmode == NL80211_IFTYPE_AP_VLAN ||
	     nlmode == NL80211_IFTYPE_WDS ||
	     nlmode == NL80211_IFTYPE_MONITOR))
		add_ifidx(drv, ifidx);

	return 0;
}


static int wpa_driver_nl80211_if_remove(struct i802_bss *bss,
					enum wpa_driver_if_type type,
					const char *ifname)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ifindex = if_nametoindex(ifname);

	wpa_printf(MSG_DEBUG, "nl80211: %s(type=%d ifname=%s) ifindex=%d added_if=%d",
		   __func__, type, ifname, ifindex, bss->added_if);
	if (ifindex > 0 && (bss->added_if || bss->ifindex != ifindex))
		nl80211_remove_iface(drv, ifindex);
	else if (ifindex > 0 && !bss->added_if) {
		struct wpa_driver_nl80211_data *drv2;
		dl_list_for_each(drv2, &drv->global->interfaces,
				 struct wpa_driver_nl80211_data, list)
			del_ifidx(drv2, ifindex);
	}

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

	if (bss != drv->first_bss) {
		struct i802_bss *tbss;

		wpa_printf(MSG_DEBUG, "nl80211: Not the first BSS - remove it");
		for (tbss = drv->first_bss; tbss; tbss = tbss->next) {
			if (tbss->next == bss) {
				tbss->next = bss->next;
				/* Unsubscribe management frames */
				nl80211_teardown_ap(bss);
				nl80211_destroy_bss(bss);
				if (!bss->added_if)
					i802_set_iface_flags(bss, 0);
				os_free(bss);
				bss = NULL;
				break;
			}
		}
		if (bss)
			wpa_printf(MSG_INFO, "nl80211: %s - could not find "
				   "BSS %p in the list", __func__, bss);
	} else {
		wpa_printf(MSG_DEBUG, "nl80211: First BSS - reassign context");
		nl80211_teardown_ap(bss);
		if (!bss->added_if && !drv->first_bss->next)
			wpa_driver_nl80211_del_beacon(drv);
		nl80211_destroy_bss(bss);
		if (!bss->added_if)
			i802_set_iface_flags(bss, 0);
		if (drv->first_bss->next) {
			drv->first_bss = drv->first_bss->next;
			drv->ctx = drv->first_bss->ctx;
			os_free(bss);
		} else {
			wpa_printf(MSG_DEBUG, "nl80211: No second BSS to reassign context to");
		}
	}

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

	wpa_printf(MSG_MSGDUMP, "nl80211: CMD_FRAME freq=%u wait=%u no_cck=%d "
		   "no_ack=%d offchanok=%d",
		   freq, wait, no_cck, no_ack, offchanok);
	wpa_hexdump(MSG_MSGDUMP, "CMD_FRAME", buf, buf_len);

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_FRAME)) ||
	    (freq && nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, freq)) ||
	    (wait && nla_put_u32(msg, NL80211_ATTR_DURATION, wait)) ||
	    (offchanok && ((drv->capa.flags & WPA_DRIVER_FLAGS_OFFCHANNEL_TX) ||
			   drv->test_use_roc_tx) &&
	     nla_put_flag(msg, NL80211_ATTR_OFFCHANNEL_TX_OK)) ||
	    (no_cck && nla_put_flag(msg, NL80211_ATTR_TX_NO_CCK_RATE)) ||
	    (no_ack && nla_put_flag(msg, NL80211_ATTR_DONT_WAIT_FOR_ACK)) ||
	    nla_put(msg, NL80211_ATTR_FRAME, buf_len, buf))
		goto fail;

	cookie = 0;
	ret = send_and_recv_msgs(drv, msg, cookie_handler, &cookie);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Frame command failed: ret=%d "
			   "(%s) (freq=%u wait=%u)", ret, strerror(-ret),
			   freq, wait);
	} else {
		wpa_printf(MSG_MSGDUMP, "nl80211: Frame TX command accepted%s; "
			   "cookie 0x%llx", no_ack ? " (no ACK)" : "",
			   (long long unsigned int) cookie);

		if (cookie_out)
			*cookie_out = no_ack ? (u64) -1 : cookie;
	}

fail:
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_send_action(struct i802_bss *bss,
					  unsigned int freq,
					  unsigned int wait_time,
					  const u8 *dst, const u8 *src,
					  const u8 *bssid,
					  const u8 *data, size_t data_len,
					  int no_cck)
{
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

	if (is_ap_interface(drv->nlmode) &&
	    (!(drv->capa.flags & WPA_DRIVER_FLAGS_OFFCHANNEL_TX) ||
	     (int) freq == bss->freq || drv->device_ap_sme ||
	     !drv->use_monitor))
		ret = wpa_driver_nl80211_send_mlme(bss, buf, 24 + data_len,
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

	wpa_printf(MSG_DEBUG, "nl80211: Cancel TX frame wait: cookie=0x%llx",
		   (long long unsigned int) drv->send_action_cookie);
	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_FRAME_WAIT_CANCEL)) ||
	    nla_put_u64(msg, NL80211_ATTR_COOKIE, drv->send_action_cookie)) {
		nlmsg_free(msg);
		return;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: wait cancel failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
}


static int wpa_driver_nl80211_remain_on_channel(void *priv, unsigned int freq,
						unsigned int duration)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	u64 cookie;

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_REMAIN_ON_CHANNEL)) ||
	    nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, freq) ||
	    nla_put_u32(msg, NL80211_ATTR_DURATION, duration)) {
		nlmsg_free(msg);
		return -1;
	}

	cookie = 0;
	ret = send_and_recv_msgs(drv, msg, cookie_handler, &cookie);
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

	msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL);
	if (!msg ||
	    nla_put_u64(msg, NL80211_ATTR_COOKIE, drv->remain_on_chan_cookie)) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret == 0)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: Failed to cancel remain-on-channel: "
		   "%d (%s)", ret, strerror(-ret));
	return -1;
}


static int wpa_driver_nl80211_probe_req_report(struct i802_bss *bss, int report)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (!report) {
		if (bss->nl_preq && drv->device_ap_sme &&
		    is_ap_interface(drv->nlmode) && !bss->in_deinit &&
		    !bss->static_ap) {
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
			nl80211_destroy_eloop_handle(&bss->nl_preq);
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

	nl80211_register_eloop_read(&bss->nl_preq,
				    wpa_driver_nl80211_event_receive,
				    bss->nl_cb);

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

	wpa_printf(MSG_DEBUG,
		   "nl80211: NL80211_CMD_SET_TX_BITRATE_MASK (ifindex=%d %s)",
		   ifindex, disabled ? "NL80211_TXRATE_LEGACY=OFDM-only" :
		   "no NL80211_TXRATE_LEGACY constraint");

	msg = nl80211_ifindex_msg(drv, ifindex, 0,
				  NL80211_CMD_SET_TX_BITRATE_MASK);
	if (!msg)
		return -1;

	bands = nla_nest_start(msg, NL80211_ATTR_TX_RATES);
	if (!bands)
		goto fail;

	/*
	 * Disable 2 GHz rates 1, 2, 5.5, 11 Mbps by masking out everything
	 * else apart from 6, 9, 12, 18, 24, 36, 48, 54 Mbps from non-MCS
	 * rates. All 5 GHz rates are left enabled.
	 */
	band = nla_nest_start(msg, NL80211_BAND_2GHZ);
	if (!band ||
	    (disabled && nla_put(msg, NL80211_TXRATE_LEGACY, 8,
				 "\x0c\x12\x18\x24\x30\x48\x60\x6c")))
		goto fail;
	nla_nest_end(msg, band);

	nla_nest_end(msg, bands);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Set TX rates failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
	} else
		drv->disabled_11b_rates = disabled;

	return ret;

fail:
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
	bss->beacon_set = 0;

	/*
	 * If the P2P GO interface was dynamically added, then it is
	 * possible that the interface change to station is not possible.
	 */
	if (drv->nlmode == NL80211_IFTYPE_P2P_GO && bss->if_dynamic)
		return 0;

	return wpa_driver_nl80211_set_mode(priv, NL80211_IFTYPE_STATION);
}


static int wpa_driver_nl80211_stop_ap(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (!is_ap_interface(drv->nlmode))
		return -1;
	wpa_driver_nl80211_del_beacon(drv);
	bss->beacon_set = 0;
	return 0;
}


static int wpa_driver_nl80211_deinit_p2p_cli(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	if (drv->nlmode != NL80211_IFTYPE_P2P_CLIENT)
		return -1;

	/*
	 * If the P2P Client interface was dynamically added, then it is
	 * possible that the interface change to station is not possible.
	 */
	if (bss->if_dynamic)
		return 0;

	return wpa_driver_nl80211_set_mode(priv, NL80211_IFTYPE_STATION);
}


static void wpa_driver_nl80211_resume(void *priv)
{
	struct i802_bss *bss = priv;

	if (i802_set_iface_flags(bss, 1))
		wpa_printf(MSG_DEBUG, "nl80211: Failed to set interface up on resume event");
}


static int nl80211_signal_monitor(void *priv, int threshold, int hysteresis)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *cqm;

	wpa_printf(MSG_DEBUG, "nl80211: Signal monitor threshold=%d "
		   "hysteresis=%d", threshold, hysteresis);

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_CQM)) ||
	    !(cqm = nla_nest_start(msg, NL80211_ATTR_CQM)) ||
	    nla_put_u32(msg, NL80211_ATTR_CQM_RSSI_THOLD, threshold) ||
	    nla_put_u32(msg, NL80211_ATTR_CQM_RSSI_HYST, hysteresis)) {
		nlmsg_free(msg);
		return -1;
	}
	nla_nest_end(msg, cqm);

	return send_and_recv_msgs(drv, msg, NULL, NULL);
}


static int get_channel_width(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wpa_signal_info *sig_change = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	sig_change->center_frq1 = -1;
	sig_change->center_frq2 = -1;
	sig_change->chanwidth = CHAN_WIDTH_UNKNOWN;

	if (tb[NL80211_ATTR_CHANNEL_WIDTH]) {
		sig_change->chanwidth = convert2width(
			nla_get_u32(tb[NL80211_ATTR_CHANNEL_WIDTH]));
		if (tb[NL80211_ATTR_CENTER_FREQ1])
			sig_change->center_frq1 =
				nla_get_u32(tb[NL80211_ATTR_CENTER_FREQ1]);
		if (tb[NL80211_ATTR_CENTER_FREQ2])
			sig_change->center_frq2 =
				nla_get_u32(tb[NL80211_ATTR_CENTER_FREQ2]);
	}

	return NL_SKIP;
}


static int nl80211_get_channel_width(struct wpa_driver_nl80211_data *drv,
				     struct wpa_signal_info *sig)
{
	struct nl_msg *msg;

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_GET_INTERFACE);
	return send_and_recv_msgs(drv, msg, get_channel_width, sig);
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

	res = nl80211_get_channel_width(drv, si);
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
			   driver->phyname, driver->first_bss->ifname,
			   MAC2STR(driver->first_bss->addr));
		if (is_ap_interface(driver->nlmode))
			freq = driver->first_bss->freq;
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

	if (os_strstr(param, "use_monitor=1")) {
		struct i802_bss *bss = priv;
		struct wpa_driver_nl80211_data *drv = bss->drv;
		drv->use_monitor = 1;
	}

	if (os_strstr(param, "force_connect_cmd=1")) {
		struct i802_bss *bss = priv;
		struct wpa_driver_nl80211_data *drv = bss->drv;
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_SME;
		drv->force_connect_cmd = 1;
	}

	if (os_strstr(param, "no_offchannel_tx=1")) {
		struct i802_bss *bss = priv;
		struct wpa_driver_nl80211_data *drv = bss->drv;
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_OFFCHANNEL_TX;
		drv->test_use_roc_tx = 1;
	}

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
		wpa_printf(MSG_ERROR, "nl80211: socket(PF_INET,SOCK_DGRAM) failed: %s",
			   strerror(errno));
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

	if (global->nl_event)
		nl80211_destroy_eloop_handle(&global->nl_event);

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

	if (!(msg = nl80211_bss_msg(bss, 0, cmd)) ||
	    (pmkid && nla_put(msg, NL80211_ATTR_PMKID, 16, pmkid)) ||
	    (bssid && nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, bssid))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(bss->drv, msg, NULL, NULL);
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


static void clean_survey_results(struct survey_results *survey_results)
{
	struct freq_survey *survey, *tmp;

	if (dl_list_empty(&survey_results->survey_list))
		return;

	dl_list_for_each_safe(survey, tmp, &survey_results->survey_list,
			      struct freq_survey, list) {
		dl_list_del(&survey->list);
		os_free(survey);
	}
}


static void add_survey(struct nlattr **sinfo, u32 ifidx,
		       struct dl_list *survey_list)
{
	struct freq_survey *survey;

	survey = os_zalloc(sizeof(struct freq_survey));
	if  (!survey)
		return;

	survey->ifidx = ifidx;
	survey->freq = nla_get_u32(sinfo[NL80211_SURVEY_INFO_FREQUENCY]);
	survey->filled = 0;

	if (sinfo[NL80211_SURVEY_INFO_NOISE]) {
		survey->nf = (int8_t)
			nla_get_u8(sinfo[NL80211_SURVEY_INFO_NOISE]);
		survey->filled |= SURVEY_HAS_NF;
	}

	if (sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME]) {
		survey->channel_time =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME]);
		survey->filled |= SURVEY_HAS_CHAN_TIME;
	}

	if (sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_BUSY]) {
		survey->channel_time_busy =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_BUSY]);
		survey->filled |= SURVEY_HAS_CHAN_TIME_BUSY;
	}

	if (sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_RX]) {
		survey->channel_time_rx =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_RX]);
		survey->filled |= SURVEY_HAS_CHAN_TIME_RX;
	}

	if (sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_TX]) {
		survey->channel_time_tx =
			nla_get_u64(sinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_TX]);
		survey->filled |= SURVEY_HAS_CHAN_TIME_TX;
	}

	wpa_printf(MSG_DEBUG, "nl80211: Freq survey dump event (freq=%d MHz noise=%d channel_time=%ld busy_time=%ld tx_time=%ld rx_time=%ld filled=%04x)",
		   survey->freq,
		   survey->nf,
		   (unsigned long int) survey->channel_time,
		   (unsigned long int) survey->channel_time_busy,
		   (unsigned long int) survey->channel_time_tx,
		   (unsigned long int) survey->channel_time_rx,
		   survey->filled);

	dl_list_add_tail(survey_list, &survey->list);
}


static int check_survey_ok(struct nlattr **sinfo, u32 surveyed_freq,
			   unsigned int freq_filter)
{
	if (!freq_filter)
		return 1;

	return freq_filter == surveyed_freq;
}


static int survey_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *sinfo[NL80211_SURVEY_INFO_MAX + 1];
	struct survey_results *survey_results;
	u32 surveyed_freq = 0;
	u32 ifidx;

	static struct nla_policy survey_policy[NL80211_SURVEY_INFO_MAX + 1] = {
		[NL80211_SURVEY_INFO_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_SURVEY_INFO_NOISE] = { .type = NLA_U8 },
	};

	survey_results = (struct survey_results *) arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_IFINDEX])
		return NL_SKIP;

	ifidx = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

	if (!tb[NL80211_ATTR_SURVEY_INFO])
		return NL_SKIP;

	if (nla_parse_nested(sinfo, NL80211_SURVEY_INFO_MAX,
			     tb[NL80211_ATTR_SURVEY_INFO],
			     survey_policy))
		return NL_SKIP;

	if (!sinfo[NL80211_SURVEY_INFO_FREQUENCY]) {
		wpa_printf(MSG_ERROR, "nl80211: Invalid survey data");
		return NL_SKIP;
	}

	surveyed_freq = nla_get_u32(sinfo[NL80211_SURVEY_INFO_FREQUENCY]);

	if (!check_survey_ok(sinfo, surveyed_freq,
			     survey_results->freq_filter))
		return NL_SKIP;

	if (survey_results->freq_filter &&
	    survey_results->freq_filter != surveyed_freq) {
		wpa_printf(MSG_EXCESSIVE, "nl80211: Ignoring survey data for freq %d MHz",
			   surveyed_freq);
		return NL_SKIP;
	}

	add_survey(sinfo, ifidx, &survey_results->survey_list);

	return NL_SKIP;
}


static int wpa_driver_nl80211_get_survey(void *priv, unsigned int freq)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int err;
	union wpa_event_data data;
	struct survey_results *survey_results;

	os_memset(&data, 0, sizeof(data));
	survey_results = &data.survey_results;

	dl_list_init(&survey_results->survey_list);

	msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);
	if (!msg)
		return -ENOBUFS;

	if (freq)
		data.survey_results.freq_filter = freq;

	do {
		wpa_printf(MSG_DEBUG, "nl80211: Fetch survey data");
		err = send_and_recv_msgs(drv, msg, survey_handler,
					 survey_results);
	} while (err > 0);

	if (err)
		wpa_printf(MSG_ERROR, "nl80211: Failed to process survey data");
	else
		wpa_supplicant_event(drv->ctx, EVENT_SURVEY, &data);

	clean_survey_results(survey_results);
	return err;
}


static void nl80211_set_rekey_info(void *priv, const u8 *kek, size_t kek_len,
				   const u8 *kck, size_t kck_len,
				   const u8 *replay_ctr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nlattr *replay_nested;
	struct nl_msg *msg;
	int ret;

	if (!drv->set_rekey_offload)
		return;

	wpa_printf(MSG_DEBUG, "nl80211: Set rekey offload");
	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_REKEY_OFFLOAD)) ||
	    !(replay_nested = nla_nest_start(msg, NL80211_ATTR_REKEY_DATA)) ||
	    nla_put(msg, NL80211_REKEY_DATA_KEK, kek_len, kek) ||
	    nla_put(msg, NL80211_REKEY_DATA_KCK, kck_len, kck) ||
	    nla_put(msg, NL80211_REKEY_DATA_REPLAY_CTR, NL80211_REPLAY_CTR_LEN,
		    replay_ctr)) {
		nl80211_nlmsg_clear(msg);
		nlmsg_free(msg);
		return;
	}

	nla_nest_end(msg, replay_nested);

	ret = send_and_recv_msgs(drv, msg, NULL, (void *) -1);
	if (ret == -EOPNOTSUPP) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Driver does not support rekey offload");
		drv->set_rekey_offload = 0;
	}
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

	if (wpa_driver_nl80211_send_mlme(bss, (u8 *) &nulldata, size, 0, 0, 0,
					 0, 0) < 0)
		wpa_printf(MSG_DEBUG, "nl80211_send_null_frame: Failed to "
			   "send poll frame");
}

static void nl80211_poll_client(void *priv, const u8 *own_addr, const u8 *addr,
				int qos)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	if (!drv->poll_command_supported) {
		nl80211_send_null_frame(bss, own_addr, addr, qos);
		return;
	}

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_PROBE_CLIENT)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) {
		nlmsg_free(msg);
		return;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Client probe request for "
			   MACSTR " failed: ret=%d (%s)",
			   MAC2STR(addr), ret, strerror(-ret));
	}
}


static int nl80211_set_power_save(struct i802_bss *bss, int enabled)
{
	struct nl_msg *msg;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_POWER_SAVE)) ||
	    nla_put_u32(msg, NL80211_ATTR_PS_STATE,
			enabled ? NL80211_PS_ENABLED : NL80211_PS_DISABLED)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}
	return send_and_recv_msgs(bss->drv, msg, NULL, NULL);
}


static int nl80211_set_p2p_powersave(void *priv, int legacy_ps, int opp_ps,
				     int ctwindow)
{
	struct i802_bss *bss = priv;

	wpa_printf(MSG_DEBUG, "nl80211: set_p2p_powersave (legacy_ps=%d "
		   "opp_ps=%d ctwindow=%d)", legacy_ps, opp_ps, ctwindow);

	if (opp_ps != -1 || ctwindow != -1) {
#ifdef ANDROID_P2P
		wpa_driver_set_p2p_ps(priv, legacy_ps, opp_ps, ctwindow);
#else /* ANDROID_P2P */
		return -1; /* Not yet supported */
#endif /* ANDROID_P2P */
	}

	if (legacy_ps == -1)
		return 0;
	if (legacy_ps != 0 && legacy_ps != 1)
		return -1; /* Not yet supported */

	return nl80211_set_power_save(bss, legacy_ps);
}


static int nl80211_start_radar_detection(void *priv,
					 struct hostapd_freq_params *freq)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: Start radar detection (CAC) %d MHz (ht_enabled=%d, vht_enabled=%d, bandwidth=%d MHz, cf1=%d MHz, cf2=%d MHz)",
		   freq->freq, freq->ht_enabled, freq->vht_enabled,
		   freq->bandwidth, freq->center_freq1, freq->center_freq2);

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_RADAR)) {
		wpa_printf(MSG_DEBUG, "nl80211: Driver does not support radar "
			   "detection");
		return -1;
	}

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_RADAR_DETECT)) ||
	    nl80211_put_freq_params(msg, freq) < 0) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret == 0)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: Failed to start radar detection: "
		   "%d (%s)", ret, strerror(-ret));
	return -1;
}

#ifdef CONFIG_TDLS

static int nl80211_send_tdls_mgmt(void *priv, const u8 *dst, u8 action_code,
				  u8 dialog_token, u16 status_code,
				  u32 peer_capab, int initiator, const u8 *buf,
				  size_t len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT))
		return -EOPNOTSUPP;

	if (!dst)
		return -EINVAL;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_TDLS_MGMT)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, dst) ||
	    nla_put_u8(msg, NL80211_ATTR_TDLS_ACTION, action_code) ||
	    nla_put_u8(msg, NL80211_ATTR_TDLS_DIALOG_TOKEN, dialog_token) ||
	    nla_put_u16(msg, NL80211_ATTR_STATUS_CODE, status_code))
		goto fail;
	if (peer_capab) {
		/*
		 * The internal enum tdls_peer_capability definition is
		 * currently identical with the nl80211 enum
		 * nl80211_tdls_peer_capability, so no conversion is needed
		 * here.
		 */
		if (nla_put_u32(msg, NL80211_ATTR_TDLS_PEER_CAPABILITY,
				peer_capab))
			goto fail;
	}
	if ((initiator &&
	     nla_put_flag(msg, NL80211_ATTR_TDLS_INITIATOR)) ||
	    nla_put(msg, NL80211_ATTR_IE, len, buf))
		goto fail;

	return send_and_recv_msgs(drv, msg, NULL, NULL);

fail:
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

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_TDLS_OPER)) ||
	    nla_put_u8(msg, NL80211_ATTR_TDLS_OPERATION, nl80211_oper) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, peer)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(drv, msg, NULL, NULL);
}


static int
nl80211_tdls_enable_channel_switch(void *priv, const u8 *addr, u8 oper_class,
				   const struct hostapd_freq_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -ENOBUFS;

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT) ||
	    !(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_CHANNEL_SWITCH))
		return -EOPNOTSUPP;

	wpa_printf(MSG_DEBUG, "nl80211: Enable TDLS channel switch " MACSTR
		   " oper_class=%u freq=%u",
		   MAC2STR(addr), oper_class, params->freq);
	msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_TDLS_CHANNEL_SWITCH);
	if (!msg ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
	    nla_put_u8(msg, NL80211_ATTR_OPER_CLASS, oper_class) ||
	    (ret = nl80211_put_freq_params(msg, params))) {
		nlmsg_free(msg);
		wpa_printf(MSG_DEBUG, "nl80211: Could not build TDLS chan switch");
		return ret;
	}

	return send_and_recv_msgs(drv, msg, NULL, NULL);
}


static int
nl80211_tdls_disable_channel_switch(void *priv, const u8 *addr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT) ||
	    !(drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_CHANNEL_SWITCH))
		return -EOPNOTSUPP;

	wpa_printf(MSG_DEBUG, "nl80211: Disable TDLS channel switch " MACSTR,
		   MAC2STR(addr));
	msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_TDLS_CANCEL_CHANNEL_SWITCH);
	if (!msg ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) {
		nlmsg_free(msg);
		wpa_printf(MSG_DEBUG,
			   "nl80211: Could not build TDLS cancel chan switch");
		return -ENOBUFS;
	}

	return send_and_recv_msgs(drv, msg, NULL, NULL);
}

#endif /* CONFIG TDLS */


static int driver_nl80211_set_key(const char *ifname, void *priv,
				  enum wpa_alg alg, const u8 *addr,
				  int key_idx, int set_tx,
				  const u8 *seq, size_t seq_len,
				  const u8 *key, size_t key_len)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_set_key(ifname, bss, alg, addr, key_idx,
					  set_tx, seq, seq_len, key, key_len);
}


static int driver_nl80211_scan2(void *priv,
				struct wpa_driver_scan_params *params)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_scan(bss, params);
}


static int driver_nl80211_deauthenticate(void *priv, const u8 *addr,
					 int reason_code)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_deauthenticate(bss, addr, reason_code);
}


static int driver_nl80211_authenticate(void *priv,
				       struct wpa_driver_auth_params *params)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_authenticate(bss, params);
}


static void driver_nl80211_deinit(void *priv)
{
	struct i802_bss *bss = priv;
	wpa_driver_nl80211_deinit(bss);
}


static int driver_nl80211_if_remove(void *priv, enum wpa_driver_if_type type,
				    const char *ifname)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_if_remove(bss, type, ifname);
}


static int driver_nl80211_send_mlme(void *priv, const u8 *data,
				    size_t data_len, int noack)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_send_mlme(bss, data, data_len, noack,
					    0, 0, 0, 0);
}


static int driver_nl80211_sta_remove(void *priv, const u8 *addr)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_sta_remove(bss, addr, -1, 0);
}


static int driver_nl80211_set_sta_vlan(void *priv, const u8 *addr,
				       const char *ifname, int vlan_id)
{
	struct i802_bss *bss = priv;
	return i802_set_sta_vlan(bss, addr, ifname, vlan_id);
}


static int driver_nl80211_read_sta_data(void *priv,
					struct hostap_sta_driver_data *data,
					const u8 *addr)
{
	struct i802_bss *bss = priv;
	return i802_read_sta_data(bss, data, addr);
}


static int driver_nl80211_send_action(void *priv, unsigned int freq,
				      unsigned int wait_time,
				      const u8 *dst, const u8 *src,
				      const u8 *bssid,
				      const u8 *data, size_t data_len,
				      int no_cck)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_send_action(bss, freq, wait_time, dst, src,
					      bssid, data, data_len, no_cck);
}


static int driver_nl80211_probe_req_report(void *priv, int report)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_probe_req_report(bss, report);
}


static int wpa_driver_nl80211_update_ft_ies(void *priv, const u8 *md,
					    const u8 *ies, size_t ies_len)
{
	int ret;
	struct nl_msg *msg;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	u16 mdid = WPA_GET_LE16(md);

	wpa_printf(MSG_DEBUG, "nl80211: Updating FT IEs");
	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_UPDATE_FT_IES)) ||
	    nla_put(msg, NL80211_ATTR_IE, ies_len, ies) ||
	    nla_put_u16(msg, NL80211_ATTR_MDID, mdid)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: update_ft_ies failed "
			   "err=%d (%s)", ret, strerror(-ret));
	}

	return ret;
}


const u8 * wpa_driver_nl80211_get_macaddr(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (drv->nlmode != NL80211_IFTYPE_P2P_DEVICE)
		return NULL;

	return bss->addr;
}


static const char * scan_state_str(enum scan_states scan_state)
{
	switch (scan_state) {
	case NO_SCAN:
		return "NO_SCAN";
	case SCAN_REQUESTED:
		return "SCAN_REQUESTED";
	case SCAN_STARTED:
		return "SCAN_STARTED";
	case SCAN_COMPLETED:
		return "SCAN_COMPLETED";
	case SCAN_ABORTED:
		return "SCAN_ABORTED";
	case SCHED_SCAN_STARTED:
		return "SCHED_SCAN_STARTED";
	case SCHED_SCAN_STOPPED:
		return "SCHED_SCAN_STOPPED";
	case SCHED_SCAN_RESULTS:
		return "SCHED_SCAN_RESULTS";
	}

	return "??";
}


static int wpa_driver_nl80211_status(void *priv, char *buf, size_t buflen)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int res;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	res = os_snprintf(pos, end - pos,
			  "ifindex=%d\n"
			  "ifname=%s\n"
			  "brname=%s\n"
			  "addr=" MACSTR "\n"
			  "freq=%d\n"
			  "%s%s%s%s%s",
			  bss->ifindex,
			  bss->ifname,
			  bss->brname,
			  MAC2STR(bss->addr),
			  bss->freq,
			  bss->beacon_set ? "beacon_set=1\n" : "",
			  bss->added_if_into_bridge ?
			  "added_if_into_bridge=1\n" : "",
			  bss->added_bridge ? "added_bridge=1\n" : "",
			  bss->in_deinit ? "in_deinit=1\n" : "",
			  bss->if_dynamic ? "if_dynamic=1\n" : "");
	if (os_snprintf_error(end - pos, res))
		return pos - buf;
	pos += res;

	if (bss->wdev_id_set) {
		res = os_snprintf(pos, end - pos, "wdev_id=%llu\n",
				  (unsigned long long) bss->wdev_id);
		if (os_snprintf_error(end - pos, res))
			return pos - buf;
		pos += res;
	}

	res = os_snprintf(pos, end - pos,
			  "phyname=%s\n"
			  "perm_addr=" MACSTR "\n"
			  "drv_ifindex=%d\n"
			  "operstate=%d\n"
			  "scan_state=%s\n"
			  "auth_bssid=" MACSTR "\n"
			  "auth_attempt_bssid=" MACSTR "\n"
			  "bssid=" MACSTR "\n"
			  "prev_bssid=" MACSTR "\n"
			  "associated=%d\n"
			  "assoc_freq=%u\n"
			  "monitor_sock=%d\n"
			  "monitor_ifidx=%d\n"
			  "monitor_refcount=%d\n"
			  "last_mgmt_freq=%u\n"
			  "eapol_tx_sock=%d\n"
			  "%s%s%s%s%s%s%s%s%s%s%s%s%s",
			  drv->phyname,
			  MAC2STR(drv->perm_addr),
			  drv->ifindex,
			  drv->operstate,
			  scan_state_str(drv->scan_state),
			  MAC2STR(drv->auth_bssid),
			  MAC2STR(drv->auth_attempt_bssid),
			  MAC2STR(drv->bssid),
			  MAC2STR(drv->prev_bssid),
			  drv->associated,
			  drv->assoc_freq,
			  drv->monitor_sock,
			  drv->monitor_ifidx,
			  drv->monitor_refcount,
			  drv->last_mgmt_freq,
			  drv->eapol_tx_sock,
			  drv->ignore_if_down_event ?
			  "ignore_if_down_event=1\n" : "",
			  drv->scan_complete_events ?
			  "scan_complete_events=1\n" : "",
			  drv->disabled_11b_rates ?
			  "disabled_11b_rates=1\n" : "",
			  drv->pending_remain_on_chan ?
			  "pending_remain_on_chan=1\n" : "",
			  drv->in_interface_list ? "in_interface_list=1\n" : "",
			  drv->device_ap_sme ? "device_ap_sme=1\n" : "",
			  drv->poll_command_supported ?
			  "poll_command_supported=1\n" : "",
			  drv->data_tx_status ? "data_tx_status=1\n" : "",
			  drv->scan_for_auth ? "scan_for_auth=1\n" : "",
			  drv->retry_auth ? "retry_auth=1\n" : "",
			  drv->use_monitor ? "use_monitor=1\n" : "",
			  drv->ignore_next_local_disconnect ?
			  "ignore_next_local_disconnect=1\n" : "",
			  drv->ignore_next_local_deauth ?
			  "ignore_next_local_deauth=1\n" : "");
	if (os_snprintf_error(end - pos, res))
		return pos - buf;
	pos += res;

	if (drv->has_capability) {
		res = os_snprintf(pos, end - pos,
				  "capa.key_mgmt=0x%x\n"
				  "capa.enc=0x%x\n"
				  "capa.auth=0x%x\n"
				  "capa.flags=0x%llx\n"
				  "capa.rrm_flags=0x%x\n"
				  "capa.max_scan_ssids=%d\n"
				  "capa.max_sched_scan_ssids=%d\n"
				  "capa.sched_scan_supported=%d\n"
				  "capa.max_match_sets=%d\n"
				  "capa.max_remain_on_chan=%u\n"
				  "capa.max_stations=%u\n"
				  "capa.probe_resp_offloads=0x%x\n"
				  "capa.max_acl_mac_addrs=%u\n"
				  "capa.num_multichan_concurrent=%u\n"
				  "capa.mac_addr_rand_sched_scan_supported=%d\n"
				  "capa.mac_addr_rand_scan_supported=%d\n",
				  drv->capa.key_mgmt,
				  drv->capa.enc,
				  drv->capa.auth,
				  (unsigned long long) drv->capa.flags,
				  drv->capa.rrm_flags,
				  drv->capa.max_scan_ssids,
				  drv->capa.max_sched_scan_ssids,
				  drv->capa.sched_scan_supported,
				  drv->capa.max_match_sets,
				  drv->capa.max_remain_on_chan,
				  drv->capa.max_stations,
				  drv->capa.probe_resp_offloads,
				  drv->capa.max_acl_mac_addrs,
				  drv->capa.num_multichan_concurrent,
				  drv->capa.mac_addr_rand_sched_scan_supported,
				  drv->capa.mac_addr_rand_scan_supported);
		if (os_snprintf_error(end - pos, res))
			return pos - buf;
		pos += res;
	}

	return pos - buf;
}


static int set_beacon_data(struct nl_msg *msg, struct beacon_data *settings)
{
	if ((settings->head &&
	     nla_put(msg, NL80211_ATTR_BEACON_HEAD,
		     settings->head_len, settings->head)) ||
	    (settings->tail &&
	     nla_put(msg, NL80211_ATTR_BEACON_TAIL,
		     settings->tail_len, settings->tail)) ||
	    (settings->beacon_ies &&
	     nla_put(msg, NL80211_ATTR_IE,
		     settings->beacon_ies_len, settings->beacon_ies)) ||
	    (settings->proberesp_ies &&
	     nla_put(msg, NL80211_ATTR_IE_PROBE_RESP,
		     settings->proberesp_ies_len, settings->proberesp_ies)) ||
	    (settings->assocresp_ies &&
	     nla_put(msg, NL80211_ATTR_IE_ASSOC_RESP,
		     settings->assocresp_ies_len, settings->assocresp_ies)) ||
	    (settings->probe_resp &&
	     nla_put(msg, NL80211_ATTR_PROBE_RESP,
		     settings->probe_resp_len, settings->probe_resp)))
		return -ENOBUFS;

	return 0;
}


static int nl80211_switch_channel(void *priv, struct csa_settings *settings)
{
	struct nl_msg *msg;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nlattr *beacon_csa;
	int ret = -ENOBUFS;

	wpa_printf(MSG_DEBUG, "nl80211: Channel switch request (cs_count=%u block_tx=%u freq=%d width=%d cf1=%d cf2=%d)",
		   settings->cs_count, settings->block_tx,
		   settings->freq_params.freq, settings->freq_params.bandwidth,
		   settings->freq_params.center_freq1,
		   settings->freq_params.center_freq2);

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_AP_CSA)) {
		wpa_printf(MSG_DEBUG, "nl80211: Driver does not support channel switch command");
		return -EOPNOTSUPP;
	}

	if ((drv->nlmode != NL80211_IFTYPE_AP) &&
	    (drv->nlmode != NL80211_IFTYPE_P2P_GO))
		return -EOPNOTSUPP;

	/* check settings validity */
	if (!settings->beacon_csa.tail ||
	    ((settings->beacon_csa.tail_len <=
	      settings->counter_offset_beacon) ||
	     (settings->beacon_csa.tail[settings->counter_offset_beacon] !=
	      settings->cs_count)))
		return -EINVAL;

	if (settings->beacon_csa.probe_resp &&
	    ((settings->beacon_csa.probe_resp_len <=
	      settings->counter_offset_presp) ||
	     (settings->beacon_csa.probe_resp[settings->counter_offset_presp] !=
	      settings->cs_count)))
		return -EINVAL;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_CHANNEL_SWITCH)) ||
	    nla_put_u32(msg, NL80211_ATTR_CH_SWITCH_COUNT,
			settings->cs_count) ||
	    (ret = nl80211_put_freq_params(msg, &settings->freq_params)) ||
	    (settings->block_tx &&
	     nla_put_flag(msg, NL80211_ATTR_CH_SWITCH_BLOCK_TX)))
		goto error;

	/* beacon_after params */
	ret = set_beacon_data(msg, &settings->beacon_after);
	if (ret)
		goto error;

	/* beacon_csa params */
	beacon_csa = nla_nest_start(msg, NL80211_ATTR_CSA_IES);
	if (!beacon_csa)
		goto fail;

	ret = set_beacon_data(msg, &settings->beacon_csa);
	if (ret)
		goto error;

	if (nla_put_u16(msg, NL80211_ATTR_CSA_C_OFF_BEACON,
			settings->counter_offset_beacon) ||
	    (settings->beacon_csa.probe_resp &&
	     nla_put_u16(msg, NL80211_ATTR_CSA_C_OFF_PRESP,
			 settings->counter_offset_presp)))
		goto fail;

	nla_nest_end(msg, beacon_csa);
	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: switch_channel failed err=%d (%s)",
			   ret, strerror(-ret));
	}
	return ret;

fail:
	ret = -ENOBUFS;
error:
	nlmsg_free(msg);
	wpa_printf(MSG_DEBUG, "nl80211: Could not build channel switch request");
	return ret;
}


static int nl80211_add_ts(void *priv, u8 tsid, const u8 *addr,
			  u8 user_priority, u16 admitted_time)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "nl80211: add_ts request: tsid=%u admitted_time=%u up=%d",
		   tsid, admitted_time, user_priority);

	if (!is_sta_interface(drv->nlmode))
		return -ENOTSUP;

	msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_ADD_TX_TS);
	if (!msg ||
	    nla_put_u8(msg, NL80211_ATTR_TSID, tsid) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
	    nla_put_u8(msg, NL80211_ATTR_USER_PRIO, user_priority) ||
	    nla_put_u16(msg, NL80211_ATTR_ADMITTED_TIME, admitted_time)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: add_ts failed err=%d (%s)",
			   ret, strerror(-ret));
	return ret;
}


static int nl80211_del_ts(void *priv, u8 tsid, const u8 *addr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: del_ts request: tsid=%u", tsid);

	if (!is_sta_interface(drv->nlmode))
		return -ENOTSUP;

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_DEL_TX_TS)) ||
	    nla_put_u8(msg, NL80211_ATTR_TSID, tsid) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: del_ts failed err=%d (%s)",
			   ret, strerror(-ret));
	return ret;
}


#ifdef CONFIG_TESTING_OPTIONS
static int cmd_reply_handler(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wpabuf *buf = arg;

	if (!buf)
		return NL_SKIP;

	if ((size_t) genlmsg_attrlen(gnlh, 0) > wpabuf_tailroom(buf)) {
		wpa_printf(MSG_INFO, "nl80211: insufficient buffer space for reply");
		return NL_SKIP;
	}

	wpabuf_put_data(buf, genlmsg_attrdata(gnlh, 0),
			genlmsg_attrlen(gnlh, 0));

	return NL_SKIP;
}
#endif /* CONFIG_TESTING_OPTIONS */


static int vendor_reply_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *nl_vendor_reply, *nl;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wpabuf *buf = arg;
	int rem;

	if (!buf)
		return NL_SKIP;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	nl_vendor_reply = tb[NL80211_ATTR_VENDOR_DATA];

	if (!nl_vendor_reply)
		return NL_SKIP;

	if ((size_t) nla_len(nl_vendor_reply) > wpabuf_tailroom(buf)) {
		wpa_printf(MSG_INFO, "nl80211: Vendor command: insufficient buffer space for reply");
		return NL_SKIP;
	}

	nla_for_each_nested(nl, nl_vendor_reply, rem) {
		wpabuf_put_data(buf, nla_data(nl), nla_len(nl));
	}

	return NL_SKIP;
}


static int nl80211_vendor_cmd(void *priv, unsigned int vendor_id,
			      unsigned int subcmd, const u8 *data,
			      size_t data_len, struct wpabuf *buf)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

#ifdef CONFIG_TESTING_OPTIONS
	if (vendor_id == 0xffffffff) {
		msg = nlmsg_alloc();
		if (!msg)
			return -ENOMEM;

		nl80211_cmd(drv, msg, 0, subcmd);
		if (nlmsg_append(msg, (void *) data, data_len, NLMSG_ALIGNTO) <
		    0)
			goto fail;
		ret = send_and_recv_msgs(drv, msg, cmd_reply_handler, buf);
		if (ret)
			wpa_printf(MSG_DEBUG, "nl80211: command failed err=%d",
				   ret);
		return ret;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, vendor_id) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, subcmd) ||
	    (data &&
	     nla_put(msg, NL80211_ATTR_VENDOR_DATA, data_len, data)))
		goto fail;

	ret = send_and_recv_msgs(drv, msg, vendor_reply_handler, buf);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: vendor command failed err=%d",
			   ret);
	return ret;

fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int nl80211_set_qos_map(void *priv, const u8 *qos_map_set,
			       u8 qos_map_set_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_hexdump(MSG_DEBUG, "nl80211: Setting QoS Map",
		    qos_map_set, qos_map_set_len);

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_QOS_MAP)) ||
	    nla_put(msg, NL80211_ATTR_QOS_MAP, qos_map_set_len, qos_map_set)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: Setting QoS Map failed");

	return ret;
}


static int nl80211_set_wowlan(void *priv,
			      const struct wowlan_triggers *triggers)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *wowlan_triggers;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: Setting wowlan");

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_WOWLAN)) ||
	    !(wowlan_triggers = nla_nest_start(msg,
					       NL80211_ATTR_WOWLAN_TRIGGERS)) ||
	    (triggers->any &&
	     nla_put_flag(msg, NL80211_WOWLAN_TRIG_ANY)) ||
	    (triggers->disconnect &&
	     nla_put_flag(msg, NL80211_WOWLAN_TRIG_DISCONNECT)) ||
	    (triggers->magic_pkt &&
	     nla_put_flag(msg, NL80211_WOWLAN_TRIG_MAGIC_PKT)) ||
	    (triggers->gtk_rekey_failure &&
	     nla_put_flag(msg, NL80211_WOWLAN_TRIG_GTK_REKEY_FAILURE)) ||
	    (triggers->eap_identity_req &&
	     nla_put_flag(msg, NL80211_WOWLAN_TRIG_EAP_IDENT_REQUEST)) ||
	    (triggers->four_way_handshake &&
	     nla_put_flag(msg, NL80211_WOWLAN_TRIG_4WAY_HANDSHAKE)) ||
	    (triggers->rfkill_release &&
	     nla_put_flag(msg, NL80211_WOWLAN_TRIG_RFKILL_RELEASE))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	nla_nest_end(msg, wowlan_triggers);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: Setting wowlan failed");

	return ret;
}


static int nl80211_roaming(void *priv, int allowed, const u8 *bssid)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *params;

	wpa_printf(MSG_DEBUG, "nl80211: Roaming policy: allowed=%d", allowed);

	if (!drv->roaming_vendor_cmd_avail) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Ignore roaming policy change since driver does not provide command for setting it");
		return -1;
	}

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ROAMING) ||
	    !(params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ROAMING_POLICY,
			allowed ? QCA_ROAMING_ALLOWED_WITHIN_ESS :
			QCA_ROAMING_NOT_ALLOWED) ||
	    (bssid &&
	     nla_put(msg, QCA_WLAN_VENDOR_ATTR_MAC_ADDR, ETH_ALEN, bssid))) {
		nlmsg_free(msg);
		return -1;
	}
	nla_nest_end(msg, params);

	return send_and_recv_msgs(drv, msg, NULL, NULL);
}


static int nl80211_set_mac_addr(void *priv, const u8 *addr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int new_addr = addr != NULL;

	if (!addr)
		addr = drv->perm_addr;

	if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 0) < 0)
		return -1;

	if (linux_set_ifhwaddr(drv->global->ioctl_sock, bss->ifname, addr) < 0)
	{
		wpa_printf(MSG_DEBUG,
			   "nl80211: failed to set_mac_addr for %s to " MACSTR,
			   bss->ifname, MAC2STR(addr));
		if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname,
					  1) < 0) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Could not restore interface UP after failed set_mac_addr");
		}
		return -1;
	}

	wpa_printf(MSG_DEBUG, "nl80211: set_mac_addr for %s to " MACSTR,
		   bss->ifname, MAC2STR(addr));
	drv->addr_changed = new_addr;
	os_memcpy(bss->addr, addr, ETH_ALEN);

	if (linux_set_iface_flags(drv->global->ioctl_sock, bss->ifname, 1) < 0)
	{
		wpa_printf(MSG_DEBUG,
			   "nl80211: Could not restore interface UP after set_mac_addr");
	}

	return 0;
}


#ifdef CONFIG_MESH

static int wpa_driver_nl80211_init_mesh(void *priv)
{
	if (wpa_driver_nl80211_set_mode(priv, NL80211_IFTYPE_MESH_POINT)) {
		wpa_printf(MSG_INFO,
			   "nl80211: Failed to set interface into mesh mode");
		return -1;
	}
	return 0;
}


static int nl80211_put_mesh_id(struct nl_msg *msg, const u8 *mesh_id,
			       size_t mesh_id_len)
{
	if (mesh_id) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * Mesh ID (SSID)",
				  mesh_id, mesh_id_len);
		return nla_put(msg, NL80211_ATTR_MESH_ID, mesh_id_len, mesh_id);
	}

	return 0;
}


static int nl80211_join_mesh(struct i802_bss *bss,
			     struct wpa_driver_mesh_join_params *params)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *container;
	int ret = -1;

	wpa_printf(MSG_DEBUG, "nl80211: mesh join (ifindex=%d)", drv->ifindex);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_JOIN_MESH);
	if (!msg ||
	    nl80211_put_freq_params(msg, &params->freq) ||
	    nl80211_put_basic_rates(msg, params->basic_rates) ||
	    nl80211_put_mesh_id(msg, params->meshid, params->meshid_len) ||
	    nl80211_put_beacon_int(msg, params->beacon_int))
		goto fail;

	wpa_printf(MSG_DEBUG, "  * flags=%08X", params->flags);

	container = nla_nest_start(msg, NL80211_ATTR_MESH_SETUP);
	if (!container)
		goto fail;

	if (params->ies) {
		wpa_hexdump(MSG_DEBUG, "  * IEs", params->ies, params->ie_len);
		if (nla_put(msg, NL80211_MESH_SETUP_IE, params->ie_len,
			    params->ies))
			goto fail;
	}
	/* WPA_DRIVER_MESH_FLAG_OPEN_AUTH is treated as default by nl80211 */
	if (params->flags & WPA_DRIVER_MESH_FLAG_SAE_AUTH) {
		if (nla_put_u8(msg, NL80211_MESH_SETUP_AUTH_PROTOCOL, 0x1) ||
		    nla_put_flag(msg, NL80211_MESH_SETUP_USERSPACE_AUTH))
			goto fail;
	}
	if ((params->flags & WPA_DRIVER_MESH_FLAG_AMPE) &&
	    nla_put_flag(msg, NL80211_MESH_SETUP_USERSPACE_AMPE))
		goto fail;
	if ((params->flags & WPA_DRIVER_MESH_FLAG_USER_MPM) &&
	    nla_put_flag(msg, NL80211_MESH_SETUP_USERSPACE_MPM))
		goto fail;
	nla_nest_end(msg, container);

	container = nla_nest_start(msg, NL80211_ATTR_MESH_CONFIG);
	if (!container)
		goto fail;

	if (!(params->conf.flags & WPA_DRIVER_MESH_CONF_FLAG_AUTO_PLINKS) &&
	    nla_put_u32(msg, NL80211_MESHCONF_AUTO_OPEN_PLINKS, 0))
		goto fail;
	if ((params->conf.flags & WPA_DRIVER_MESH_FLAG_DRIVER_MPM) &&
	    nla_put_u16(msg, NL80211_MESHCONF_MAX_PEER_LINKS,
			params->max_peer_links))
		goto fail;

	/*
	 * Set NL80211_MESHCONF_PLINK_TIMEOUT even if user mpm is used because
	 * the timer could disconnect stations even in that case.
	 */
	if (nla_put_u32(msg, NL80211_MESHCONF_PLINK_TIMEOUT,
			params->conf.peer_link_timeout)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to set PLINK_TIMEOUT");
		goto fail;
	}

	nla_nest_end(msg, container);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: mesh join failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto fail;
	}
	ret = 0;
	bss->freq = params->freq.freq;
	wpa_printf(MSG_DEBUG, "nl80211: mesh join request send successfully");

fail:
	nlmsg_free(msg);
	return ret;
}


static int
wpa_driver_nl80211_join_mesh(void *priv,
			     struct wpa_driver_mesh_join_params *params)
{
	struct i802_bss *bss = priv;
	int ret, timeout;

	timeout = params->conf.peer_link_timeout;

	/* Disable kernel inactivity timer */
	if (params->flags & WPA_DRIVER_MESH_FLAG_USER_MPM)
		params->conf.peer_link_timeout = 0;

	ret = nl80211_join_mesh(bss, params);
	if (ret == -EINVAL && params->conf.peer_link_timeout == 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Mesh join retry for peer_link_timeout");
		/*
		 * Old kernel does not support setting
		 * NL80211_MESHCONF_PLINK_TIMEOUT to zero, so set 60 seconds
		 * into future from peer_link_timeout.
		 */
		params->conf.peer_link_timeout = timeout + 60;
		ret = nl80211_join_mesh(priv, params);
	}

	params->conf.peer_link_timeout = timeout;
	return ret;
}


static int wpa_driver_nl80211_leave_mesh(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: mesh leave (ifindex=%d)", drv->ifindex);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_LEAVE_MESH);
	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: mesh leave failed: ret=%d (%s)",
			   ret, strerror(-ret));
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: mesh leave request send successfully");
	}

	if (wpa_driver_nl80211_set_mode(drv->first_bss,
					NL80211_IFTYPE_STATION)) {
		wpa_printf(MSG_INFO,
			   "nl80211: Failed to set interface into station mode");
	}
	return ret;
}

#endif /* CONFIG_MESH */


static int wpa_driver_br_add_ip_neigh(void *priv, u8 version,
				      const u8 *ipaddr, int prefixlen,
				      const u8 *addr)
{
#ifdef CONFIG_LIBNL3_ROUTE
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct rtnl_neigh *rn;
	struct nl_addr *nl_ipaddr = NULL;
	struct nl_addr *nl_lladdr = NULL;
	int family, addrsize;
	int res;

	if (!ipaddr || prefixlen == 0 || !addr)
		return -EINVAL;

	if (bss->br_ifindex == 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: bridge must be set before adding an ip neigh to it");
		return -1;
	}

	if (!drv->rtnl_sk) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: nl_sock for NETLINK_ROUTE is not initialized");
		return -1;
	}

	if (version == 4) {
		family = AF_INET;
		addrsize = 4;
	} else if (version == 6) {
		family = AF_INET6;
		addrsize = 16;
	} else {
		return -EINVAL;
	}

	rn = rtnl_neigh_alloc();
	if (rn == NULL)
		return -ENOMEM;

	/* set the destination ip address for neigh */
	nl_ipaddr = nl_addr_build(family, (void *) ipaddr, addrsize);
	if (nl_ipaddr == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: nl_ipaddr build failed");
		res = -ENOMEM;
		goto errout;
	}
	nl_addr_set_prefixlen(nl_ipaddr, prefixlen);
	res = rtnl_neigh_set_dst(rn, nl_ipaddr);
	if (res) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: neigh set destination addr failed");
		goto errout;
	}

	/* set the corresponding lladdr for neigh */
	nl_lladdr = nl_addr_build(AF_BRIDGE, (u8 *) addr, ETH_ALEN);
	if (nl_lladdr == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: neigh set lladdr failed");
		res = -ENOMEM;
		goto errout;
	}
	rtnl_neigh_set_lladdr(rn, nl_lladdr);

	rtnl_neigh_set_ifindex(rn, bss->br_ifindex);
	rtnl_neigh_set_state(rn, NUD_PERMANENT);

	res = rtnl_neigh_add(drv->rtnl_sk, rn, NLM_F_CREATE);
	if (res) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Adding bridge ip neigh failed: %s",
			   strerror(errno));
	}
errout:
	if (nl_lladdr)
		nl_addr_put(nl_lladdr);
	if (nl_ipaddr)
		nl_addr_put(nl_ipaddr);
	if (rn)
		rtnl_neigh_put(rn);
	return res;
#else /* CONFIG_LIBNL3_ROUTE */
	return -1;
#endif /* CONFIG_LIBNL3_ROUTE */
}


static int wpa_driver_br_delete_ip_neigh(void *priv, u8 version,
					 const u8 *ipaddr)
{
#ifdef CONFIG_LIBNL3_ROUTE
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct rtnl_neigh *rn;
	struct nl_addr *nl_ipaddr;
	int family, addrsize;
	int res;

	if (!ipaddr)
		return -EINVAL;

	if (version == 4) {
		family = AF_INET;
		addrsize = 4;
	} else if (version == 6) {
		family = AF_INET6;
		addrsize = 16;
	} else {
		return -EINVAL;
	}

	if (bss->br_ifindex == 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: bridge must be set to delete an ip neigh");
		return -1;
	}

	if (!drv->rtnl_sk) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: nl_sock for NETLINK_ROUTE is not initialized");
		return -1;
	}

	rn = rtnl_neigh_alloc();
	if (rn == NULL)
		return -ENOMEM;

	/* set the destination ip address for neigh */
	nl_ipaddr = nl_addr_build(family, (void *) ipaddr, addrsize);
	if (nl_ipaddr == NULL) {
		wpa_printf(MSG_DEBUG, "nl80211: nl_ipaddr build failed");
		res = -ENOMEM;
		goto errout;
	}
	res = rtnl_neigh_set_dst(rn, nl_ipaddr);
	if (res) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: neigh set destination addr failed");
		goto errout;
	}

	rtnl_neigh_set_ifindex(rn, bss->br_ifindex);

	res = rtnl_neigh_delete(drv->rtnl_sk, rn, 0);
	if (res) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Deleting bridge ip neigh failed: %s",
			   strerror(errno));
	}
errout:
	if (nl_ipaddr)
		nl_addr_put(nl_ipaddr);
	if (rn)
		rtnl_neigh_put(rn);
	return res;
#else /* CONFIG_LIBNL3_ROUTE */
	return -1;
#endif /* CONFIG_LIBNL3_ROUTE */
}


static int linux_write_system_file(const char *path, unsigned int val)
{
	char buf[50];
	int fd, len;

	len = os_snprintf(buf, sizeof(buf), "%u\n", val);
	if (os_snprintf_error(sizeof(buf), len))
		return -1;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		return -1;

	if (write(fd, buf, len) < 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Failed to write Linux system file: %s with the value of %d",
			   path, val);
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}


static const char * drv_br_port_attr_str(enum drv_br_port_attr attr)
{
	switch (attr) {
	case DRV_BR_PORT_ATTR_PROXYARP:
		return "proxyarp_wifi";
	case DRV_BR_PORT_ATTR_HAIRPIN_MODE:
		return "hairpin_mode";
	}

	return NULL;
}


static int wpa_driver_br_port_set_attr(void *priv, enum drv_br_port_attr attr,
				       unsigned int val)
{
	struct i802_bss *bss = priv;
	char path[128];
	const char *attr_txt;

	attr_txt = drv_br_port_attr_str(attr);
	if (attr_txt == NULL)
		return -EINVAL;

	os_snprintf(path, sizeof(path), "/sys/class/net/%s/brport/%s",
		    bss->ifname, attr_txt);

	if (linux_write_system_file(path, val))
		return -1;

	return 0;
}


static const char * drv_br_net_param_str(enum drv_br_net_param param)
{
	switch (param) {
	case DRV_BR_NET_PARAM_GARP_ACCEPT:
		return "arp_accept";
	}

	return NULL;
}


static int wpa_driver_br_set_net_param(void *priv, enum drv_br_net_param param,
				       unsigned int val)
{
	struct i802_bss *bss = priv;
	char path[128];
	const char *param_txt;
	int ip_version = 4;

	param_txt = drv_br_net_param_str(param);
	if (param_txt == NULL)
		return -EINVAL;

	switch (param) {
		case DRV_BR_NET_PARAM_GARP_ACCEPT:
			ip_version = 4;
			break;
		default:
			return -EINVAL;
	}

	os_snprintf(path, sizeof(path), "/proc/sys/net/ipv%d/conf/%s/%s",
		    ip_version, bss->brname, param_txt);

	if (linux_write_system_file(path, val))
		return -1;

	return 0;
}


static int hw_mode_to_qca_acs(enum hostapd_hw_mode hw_mode)
{
	switch (hw_mode) {
	case HOSTAPD_MODE_IEEE80211B:
		return QCA_ACS_MODE_IEEE80211B;
	case HOSTAPD_MODE_IEEE80211G:
		return QCA_ACS_MODE_IEEE80211G;
	case HOSTAPD_MODE_IEEE80211A:
		return QCA_ACS_MODE_IEEE80211A;
	case HOSTAPD_MODE_IEEE80211AD:
		return QCA_ACS_MODE_IEEE80211AD;
	default:
		return -1;
	}
}


static int wpa_driver_do_acs(void *priv, struct drv_acs_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *data;
	int ret;
	int mode;

	mode = hw_mode_to_qca_acs(params->hw_mode);
	if (mode < 0)
		return -1;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DO_ACS) ||
	    !(data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ACS_HW_MODE, mode) ||
	    (params->ht_enabled &&
	     nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_ACS_HT_ENABLED)) ||
	    (params->ht40_enabled &&
	     nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}
	nla_nest_end(msg, data);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Failed to invoke driver ACS function: %s",
			   strerror(errno));
	}
	return ret;
}


const struct wpa_driver_ops wpa_driver_nl80211_ops = {
	.name = "nl80211",
	.desc = "Linux nl80211/cfg80211",
	.get_bssid = wpa_driver_nl80211_get_bssid,
	.get_ssid = wpa_driver_nl80211_get_ssid,
	.set_key = driver_nl80211_set_key,
	.scan2 = driver_nl80211_scan2,
	.sched_scan = wpa_driver_nl80211_sched_scan,
	.stop_sched_scan = wpa_driver_nl80211_stop_sched_scan,
	.get_scan_results2 = wpa_driver_nl80211_get_scan_results,
	.deauthenticate = driver_nl80211_deauthenticate,
	.authenticate = driver_nl80211_authenticate,
	.associate = wpa_driver_nl80211_associate,
	.global_init = nl80211_global_init,
	.global_deinit = nl80211_global_deinit,
	.init2 = wpa_driver_nl80211_init,
	.deinit = driver_nl80211_deinit,
	.get_capa = wpa_driver_nl80211_get_capa,
	.set_operstate = wpa_driver_nl80211_set_operstate,
	.set_supp_port = wpa_driver_nl80211_set_supp_port,
	.set_country = wpa_driver_nl80211_set_country,
	.get_country = wpa_driver_nl80211_get_country,
	.set_ap = wpa_driver_nl80211_set_ap,
	.set_acl = wpa_driver_nl80211_set_acl,
	.if_add = wpa_driver_nl80211_if_add,
	.if_remove = driver_nl80211_if_remove,
	.send_mlme = driver_nl80211_send_mlme,
	.get_hw_feature_data = nl80211_get_hw_feature_data,
	.sta_add = wpa_driver_nl80211_sta_add,
	.sta_remove = driver_nl80211_sta_remove,
	.hapd_send_eapol = wpa_driver_nl80211_hapd_send_eapol,
	.sta_set_flags = wpa_driver_nl80211_sta_set_flags,
	.hapd_init = i802_init,
	.hapd_deinit = i802_deinit,
	.set_wds_sta = i802_set_wds_sta,
	.get_seqnum = i802_get_seqnum,
	.flush = i802_flush,
	.get_inact_sec = i802_get_inact_sec,
	.sta_clear_stats = i802_sta_clear_stats,
	.set_rts = i802_set_rts,
	.set_frag = i802_set_frag,
	.set_tx_queue_params = i802_set_tx_queue_params,
	.set_sta_vlan = driver_nl80211_set_sta_vlan,
	.sta_deauth = i802_sta_deauth,
	.sta_disassoc = i802_sta_disassoc,
	.read_sta_data = driver_nl80211_read_sta_data,
	.set_freq = i802_set_freq,
	.send_action = driver_nl80211_send_action,
	.send_action_cancel_wait = wpa_driver_nl80211_send_action_cancel_wait,
	.remain_on_channel = wpa_driver_nl80211_remain_on_channel,
	.cancel_remain_on_channel =
	wpa_driver_nl80211_cancel_remain_on_channel,
	.probe_req_report = driver_nl80211_probe_req_report,
	.deinit_ap = wpa_driver_nl80211_deinit_ap,
	.deinit_p2p_cli = wpa_driver_nl80211_deinit_p2p_cli,
	.resume = wpa_driver_nl80211_resume,
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
	.start_dfs_cac = nl80211_start_radar_detection,
	.stop_ap = wpa_driver_nl80211_stop_ap,
#ifdef CONFIG_TDLS
	.send_tdls_mgmt = nl80211_send_tdls_mgmt,
	.tdls_oper = nl80211_tdls_oper,
	.tdls_enable_channel_switch = nl80211_tdls_enable_channel_switch,
	.tdls_disable_channel_switch = nl80211_tdls_disable_channel_switch,
#endif /* CONFIG_TDLS */
	.update_ft_ies = wpa_driver_nl80211_update_ft_ies,
	.get_mac_addr = wpa_driver_nl80211_get_macaddr,
	.get_survey = wpa_driver_nl80211_get_survey,
	.status = wpa_driver_nl80211_status,
	.switch_channel = nl80211_switch_channel,
#ifdef ANDROID_P2P
	.set_noa = wpa_driver_set_p2p_noa,
	.get_noa = wpa_driver_get_p2p_noa,
	.set_ap_wps_ie = wpa_driver_set_ap_wps_p2p_ie,
#endif /* ANDROID_P2P */
#ifdef ANDROID
	.driver_cmd = wpa_driver_nl80211_driver_cmd,
#endif /* ANDROID */
	.vendor_cmd = nl80211_vendor_cmd,
	.set_qos_map = nl80211_set_qos_map,
	.set_wowlan = nl80211_set_wowlan,
	.roaming = nl80211_roaming,
	.set_mac_addr = nl80211_set_mac_addr,
#ifdef CONFIG_MESH
	.init_mesh = wpa_driver_nl80211_init_mesh,
	.join_mesh = wpa_driver_nl80211_join_mesh,
	.leave_mesh = wpa_driver_nl80211_leave_mesh,
#endif /* CONFIG_MESH */
	.br_add_ip_neigh = wpa_driver_br_add_ip_neigh,
	.br_delete_ip_neigh = wpa_driver_br_delete_ip_neigh,
	.br_port_set_attr = wpa_driver_br_port_set_attr,
	.br_set_net_param = wpa_driver_br_set_net_param,
	.add_tx_ts = nl80211_add_ts,
	.del_tx_ts = nl80211_del_ts,
	.do_acs = wpa_driver_do_acs,
};
