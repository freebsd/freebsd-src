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
#include "common/brcm_vendor.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "netlink.h"
#include "linux_defines.h"
#include "linux_ioctl.h"
#include "radiotap.h"
#include "radiotap_iter.h"
#include "rfkill.h"
#include "driver_nl80211.h"


#ifndef NETLINK_CAP_ACK
#define NETLINK_CAP_ACK 10
#endif /* NETLINK_CAP_ACK */
/* support for extack if compilation headers are too old */
#ifndef NETLINK_EXT_ACK
#define NETLINK_EXT_ACK 11
enum nlmsgerr_attrs {
	NLMSGERR_ATTR_UNUSED,
	NLMSGERR_ATTR_MSG,
	NLMSGERR_ATTR_OFFS,
	NLMSGERR_ATTR_COOKIE,

	__NLMSGERR_ATTR_MAX,
	NLMSGERR_ATTR_MAX = __NLMSGERR_ATTR_MAX - 1
};
#endif
#ifndef NLM_F_CAPPED
#define NLM_F_CAPPED 0x100
#endif
#ifndef NLM_F_ACK_TLVS
#define NLM_F_ACK_TLVS 0x200
#endif
#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif


#ifdef ANDROID
/* system/core/libnl_2 does not include nl_socket_set_nonblocking() */
#undef nl_socket_set_nonblocking
#define nl_socket_set_nonblocking(h) android_nl_socket_set_nonblocking(h)

#endif /* ANDROID */


static struct nl_sock * nl_create_handle(struct nl_cb *cb, const char *dbg)
{
	struct nl_sock *handle;

	handle = nl_socket_alloc_cb(cb);
	if (handle == NULL) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to allocate netlink "
			   "callbacks (%s)", dbg);
		return NULL;
	}

	if (genl_connect(handle)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to connect to generic "
			   "netlink (%s)", dbg);
		nl_socket_free(handle);
		return NULL;
	}

	return handle;
}


static void nl_destroy_handles(struct nl_sock **handle)
{
	if (*handle == NULL)
		return;
	nl_socket_free(*handle);
	*handle = NULL;
}


#if __WORDSIZE == 64
#define ELOOP_SOCKET_INVALID	(intptr_t) 0x8888888888888889ULL
#else
#define ELOOP_SOCKET_INVALID	(intptr_t) 0x88888889ULL
#endif

static void nl80211_register_eloop_read(struct nl_sock **handle,
					eloop_sock_handler handler,
					void *eloop_data, int persist)
{
	/*
	 * libnl uses a pretty small buffer (32 kB that gets converted to 64 kB)
	 * by default. It is possible to hit that limit in some cases where
	 * operations are blocked, e.g., with a burst of Deauthentication frames
	 * to hostapd and STA entry deletion. Try to increase the buffer to make
	 * this less likely to occur.
	 */
	int err;

	err = nl_socket_set_buffer_size(*handle, 262144, 0);
	if (err < 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Could not set nl_socket RX buffer size: %s",
			   nl_geterror(err));
		/* continue anyway with the default (smaller) buffer */
	}

	nl_socket_set_nonblocking(*handle);
	eloop_register_read_sock(nl_socket_get_fd(*handle), handler,
				 eloop_data, *handle);
	if (!persist)
		*handle = (void *) (((intptr_t) *handle) ^
				    ELOOP_SOCKET_INVALID);
}


static void nl80211_destroy_eloop_handle(struct nl_sock **handle, int persist)
{
	if (!persist)
		*handle = (void *) (((intptr_t) *handle) ^
				    ELOOP_SOCKET_INVALID);
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
				  const u8 *buf, size_t buf_len,
				  int save_cookie,
				  int no_cck, int no_ack, int offchanok,
				  const u16 *csa_offs, size_t csa_offs_len);
static int wpa_driver_nl80211_probe_req_report(struct i802_bss *bss,
					       int report);

#define IFIDX_ANY -1

static void add_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx,
		      int ifidx_reason);
static void del_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx,
		      int ifidx_reason);
static int have_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx,
		      int ifidx_reason);

static int nl80211_set_channel(struct i802_bss *bss,
			       struct hostapd_freq_params *freq, int set_chan);
static int nl80211_disable_11b_rates(struct wpa_driver_nl80211_data *drv,
				     int ifindex, int disabled);

static int nl80211_leave_ibss(struct wpa_driver_nl80211_data *drv,
			      int reset_mode);

static int i802_set_iface_flags(struct i802_bss *bss, int up);
static int nl80211_set_param(void *priv, const char *param);
#ifdef CONFIG_MESH
static int nl80211_put_mesh_config(struct nl_msg *msg,
				   struct wpa_driver_mesh_bss_params *params);
#endif /* CONFIG_MESH */
static int i802_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
			     u16 reason);


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
	drv->first_bss->freq = 0;
}


/* nl80211 code */
static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *err = arg;
	*err = 0;
	return NL_STOP;
}


struct nl80211_ack_ext_arg {
	int *err;
	void *ext_data;
};


static int ack_handler_cookie(struct nl_msg *msg, void *arg)
{
	struct nl80211_ack_ext_arg *ext_arg = arg;
	struct nlattr *tb[NLMSGERR_ATTR_MAX + 1];
	u64 *cookie = ext_arg->ext_data;
	struct nlattr *attrs;
	size_t ack_len, attr_len;

	*ext_arg->err = 0;
	ack_len = sizeof(struct nlmsghdr) + sizeof(int) +
		sizeof(struct nlmsghdr);
	attrs = (struct nlattr *)
		((u8 *) nlmsg_data(nlmsg_hdr(msg)) + sizeof(struct nlmsghdr) +
		 sizeof(int));
	if (nlmsg_hdr(msg)->nlmsg_len <= ack_len)
		return NL_STOP;

	attr_len = nlmsg_hdr(msg)->nlmsg_len - ack_len;

	if(!(nlmsg_hdr(msg)->nlmsg_flags & NLM_F_ACK_TLVS))
		return NL_STOP;

	nla_parse(tb, NLMSGERR_ATTR_MAX, attrs, attr_len, NULL);
	if (tb[NLMSGERR_ATTR_COOKIE])
		*cookie = nla_get_u64(tb[NLMSGERR_ATTR_COOKIE]);

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
	struct nlmsghdr *nlh = (struct nlmsghdr *) err - 1;
	int len = nlh->nlmsg_len;
	struct nlattr *attrs;
	struct nlattr *tb[NLMSGERR_ATTR_MAX + 1];
	int *ret = arg;
	int ack_len = sizeof(*nlh) + sizeof(int) + sizeof(*nlh);

	*ret = err->error;

	if (!(nlh->nlmsg_flags & NLM_F_ACK_TLVS))
		return NL_SKIP;

	if (!(nlh->nlmsg_flags & NLM_F_CAPPED))
		ack_len += err->msg.nlmsg_len - sizeof(*nlh);

	if (len <= ack_len)
		return NL_STOP;

	attrs = (void *) ((unsigned char *) nlh + ack_len);
	len -= ack_len;

	nla_parse(tb, NLMSGERR_ATTR_MAX, attrs, len, NULL);
	if (tb[NLMSGERR_ATTR_MSG]) {
		len = strnlen((char *) nla_data(tb[NLMSGERR_ATTR_MSG]),
			      nla_len(tb[NLMSGERR_ATTR_MSG]));
		wpa_printf(MSG_ERROR, "nl80211: kernel reports: %*s",
			   len, (char *) nla_data(tb[NLMSGERR_ATTR_MSG]));
	}

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
			 struct nl_sock *nl_handle, struct nl_msg *msg,
			 int (*valid_handler)(struct nl_msg *, void *),
			 void *valid_data,
			 int (*ack_handler_custom)(struct nl_msg *, void *),
			 void *ack_data)
{
	struct nl_cb *cb;
	int err = -ENOMEM, opt;

	if (!msg)
		return -ENOMEM;

	cb = nl_cb_clone(global->nl_cb);
	if (!cb)
		goto out;

	/* try to set NETLINK_EXT_ACK to 1, ignoring errors */
	opt = 1;
	setsockopt(nl_socket_get_fd(nl_handle), SOL_NETLINK,
		   NETLINK_EXT_ACK, &opt, sizeof(opt));

	/* try to set NETLINK_CAP_ACK to 1, ignoring errors */
	opt = 1;
	setsockopt(nl_socket_get_fd(nl_handle), SOL_NETLINK,
		   NETLINK_CAP_ACK, &opt, sizeof(opt));

	err = nl_send_auto_complete(nl_handle, msg);
	if (err < 0) {
		wpa_printf(MSG_INFO,
			   "nl80211: nl_send_auto_complete() failed: %s",
			   nl_geterror(err));
		/* Need to convert libnl error code to an errno value. For now,
		 * just hardcode this to EBADF; the real error reason is shown
		 * in that error print above. */
		err = -EBADF;
		goto out;
	}

	err = 1;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
	if (ack_handler_custom) {
		struct nl80211_ack_ext_arg *ext_arg = ack_data;

		ext_arg->err = &err;
		nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM,
			  ack_handler_custom, ack_data);
	} else {
		nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
	}

	if (valid_handler)
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
			  valid_handler, valid_data);

	while (err > 0) {
		int res = nl_recvmsgs(nl_handle, cb);

		if (res == -NLE_DUMP_INTR) {
			/* Most likely one of the nl80211 dump routines hit a
			 * case where internal results changed while the dump
			 * was being sent. The most common known case for this
			 * is scan results fetching while associated were every
			 * received Beacon frame from the AP may end up
			 * incrementing bss_generation. This
			 * NL80211_CMD_GET_SCAN case tries again in the caller;
			 * other cases (of which there are no known common ones)
			 * will stop and return an error. */
			wpa_printf(MSG_DEBUG, "nl80211: %s; convert to -EAGAIN",
				   nl_geterror(res));
			err = -EAGAIN;
		} else if (res < 0) {
			wpa_printf(MSG_INFO,
				   "nl80211: %s->nl_recvmsgs failed: %d (%s)",
				   __func__, res, nl_geterror(res));
		}
	}
 out:
	nl_cb_put(cb);
	/* Always clear the message as it can potentially contain keys */
	nl80211_nlmsg_clear(msg);
	nlmsg_free(msg);
	return err;
}


int send_and_recv_msgs(struct wpa_driver_nl80211_data *drv,
		       struct nl_msg *msg,
		       int (*valid_handler)(struct nl_msg *, void *),
		       void *valid_data,
		       int (*ack_handler_custom)(struct nl_msg *, void *),
		       void *ack_data)
{
	return send_and_recv(drv->global, drv->global->nl, msg,
			     valid_handler, valid_data,
			     ack_handler_custom, ack_data);
}


/* Use this method to mark that it is necessary to own the connection/interface
 * for this operation.
 * handle may be set to NULL, to get the same behavior as send_and_recv_msgs().
 * set_owner can be used to mark this socket for receiving control port frames.
 */
static int send_and_recv_msgs_owner(struct wpa_driver_nl80211_data *drv,
				    struct nl_msg *msg,
				    struct nl_sock *handle, int set_owner,
				    int (*valid_handler)(struct nl_msg *,
							 void *),
				    void *valid_data,
				    int (*ack_handler_custom)(struct nl_msg *,
							      void *),
				    void *ack_data)
{
	if (!msg)
		return -ENOMEM;

	/* Control port over nl80211 needs the flags and attributes below.
	 *
	 * The Linux kernel has initial checks for them (in nl80211.c) like:
	 *     validate_pae_over_nl80211(...)
	 * or final checks like:
	 *     dev->ieee80211_ptr->conn_owner_nlportid != info->snd_portid
	 *
	 * Final operations (e.g., disassociate) don't need to set these
	 * attributes, but they have to be performed on the socket, which has
	 * the connection owner property set in the kernel.
	 */
	if ((drv->capa.flags2 & WPA_DRIVER_FLAGS2_CONTROL_PORT_RX) &&
	    handle && set_owner &&
	    (nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT_OVER_NL80211) ||
	     nla_put_flag(msg, NL80211_ATTR_SOCKET_OWNER) ||
	     nla_put_u16(msg, NL80211_ATTR_CONTROL_PORT_ETHERTYPE, ETH_P_PAE) ||
	     nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT_NO_PREAUTH)))
		return -1;

	return send_and_recv(drv->global, handle ? handle : drv->global->nl,
			     msg, valid_handler, valid_data,
			     ack_handler_custom, ack_data);
}


static int
send_and_recv_msgs_connect_handle(struct wpa_driver_nl80211_data *drv,
				  struct nl_msg *msg, struct i802_bss *bss,
				  int set_owner)
{
	struct nl_sock *nl_connect = get_connect_handle(bss);

	if (nl_connect)
		return send_and_recv_msgs_owner(drv, msg, nl_connect, set_owner,
						process_bss_event, bss, NULL,
						NULL);
	else
		return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
}


struct nl_sock * get_connect_handle(struct i802_bss *bss)
{
	if ((bss->drv->capa.flags2 & WPA_DRIVER_FLAGS2_CONTROL_PORT_RX) ||
	    bss->use_nl_connect)
		return bss->nl_connect;

	return NULL;
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
	if (!genlmsg_put(msg, 0, 0, global->nlctrl_id,
			 0, 0, CTRL_CMD_GETFAMILY, 0) ||
	    nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family)) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv(global, global->nl, msg, family_handler, &res,
			    NULL, NULL);
	if (ret == 0)
		ret = res.id;
	return ret;
}


void * nl80211_cmd(struct wpa_driver_nl80211_data *drv,
		   struct nl_msg *msg, int flags, uint8_t cmd)
{
	if (TEST_FAIL())
		return NULL;
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
nl80211_ifindex_msg_build(struct wpa_driver_nl80211_data *drv,
			  struct nl_msg *msg, int ifindex, int flags,
			  uint8_t cmd)
{
	if (!msg)
		return NULL;

	if (!nl80211_cmd(drv, msg, flags, cmd) ||
	    nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifindex)) {
		nlmsg_free(msg);
		return NULL;
	}

	return msg;
}


static struct nl_msg *
nl80211_ifindex_msg(struct wpa_driver_nl80211_data *drv, int ifindex,
		    int flags, uint8_t cmd)
{
	return nl80211_ifindex_msg_build(drv, nlmsg_alloc(), ifindex, flags,
					 cmd);
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
	u8 use_4addr;
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

	if (tb[NL80211_ATTR_4ADDR])
		info->use_4addr = nla_get_u8(tb[NL80211_ATTR_4ADDR]);

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

	if (send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data,
			       NULL, NULL) == 0)
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

	if (send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data,
			       NULL, NULL) == 0)
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

	return send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data,
				  NULL, NULL);
}


static int nl80211_get_4addr(struct i802_bss *bss)
{
	struct nl_msg *msg;
	struct wiphy_idx_data data = {
		.use_4addr = 0,
	};

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_GET_INTERFACE)) ||
	    send_and_recv_msgs(bss->drv, msg, netdev_info_handler, &data,
			       NULL, NULL))
		return -1;
	return data.use_4addr;
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

	ret = send_and_recv(drv->global, w->nl_beacons, msg, NULL, NULL,
			    NULL, NULL);
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
	u8 channel;

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

	/* Beacon frames not supported in IEEE 802.11ad */
	if (ieee80211_freq_to_chan(bss->freq, &channel) !=
	    HOSTAPD_MODE_IEEE80211AD) {
		w->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
		if (!w->nl_cb) {
			os_free(w);
			return NULL;
		}
		nl_cb_set(w->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
			  no_seq_check, NULL);
		nl_cb_set(w->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
			  process_beacon_event, w);

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

		nl80211_register_eloop_read(&w->nl_beacons,
					    nl80211_recv_beacons, w, 0);
	}

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

	if (w->nl_beacons)
		nl80211_destroy_eloop_handle(&w->nl_beacons, 0);

	nl_cb_put(w->nl_cb);
	dl_list_del(&w->list);
	os_free(w);
}


static unsigned int nl80211_get_ifindex(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	return drv->ifindex;
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
	struct nl80211_global *global, struct wpa_driver_nl80211_data *drv,
	int ifindex, const char *ifname)
{
	union wpa_event_data event;

	if (drv && os_strcmp(drv->first_bss->ifname, ifname) == 0) {
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
	event.interface_status.ifindex = ifindex;
	os_strlcpy(event.interface_status.ifname, ifname,
		   sizeof(event.interface_status.ifname));
	event.interface_status.ievent = EVENT_INTERFACE_ADDED;
	if (drv)
		wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS, &event);
	else
		wpa_supplicant_event_global(global->ctx, EVENT_INTERFACE_STATUS,
					    &event);
}


static void wpa_driver_nl80211_event_dellink(
	struct nl80211_global *global, struct wpa_driver_nl80211_data *drv,
	int ifindex, const char *ifname)
{
	union wpa_event_data event;

	if (drv && os_strcmp(drv->first_bss->ifname, ifname) == 0) {
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
	event.interface_status.ifindex = ifindex;
	os_strlcpy(event.interface_status.ifname, ifname,
		   sizeof(event.interface_status.ifname));
	event.interface_status.ievent = EVENT_INTERFACE_REMOVED;
	if (drv)
		wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_STATUS, &event);
	else
		wpa_supplicant_event_global(global->ctx, EVENT_INTERFACE_STATUS,
					    &event);
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
		if (wpa_driver_nl80211_finish_drv_init(drv, NULL, 0, NULL) < 0)
			return -1;
		return 1;
	}

	return 0;
}


static struct wpa_driver_nl80211_data *
nl80211_find_drv(struct nl80211_global *global, int idx, u8 *buf, size_t len,
		 int *init_failed)
{
	struct wpa_driver_nl80211_data *drv;
	int res;

	if (init_failed)
		*init_failed = 0;
	dl_list_for_each(drv, &global->interfaces,
			 struct wpa_driver_nl80211_data, list) {
		res = wpa_driver_nl80211_own_ifindex(drv, idx, buf, len);
		if (res < 0) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Found matching own interface, but failed to complete reinitialization");
			if (init_failed)
				*init_failed = 1;
			return drv;
		}
		if (res > 0 || have_ifidx(drv, idx, IFIDX_ANY))
			return drv;
	}
	return NULL;
}


static void nl80211_refresh_mac(struct wpa_driver_nl80211_data *drv,
				int ifindex, int notify)
{
	struct i802_bss *bss;
	u8 addr[ETH_ALEN];

	bss = get_bss_ifindex(drv, ifindex);
	if (bss &&
	    linux_get_ifhwaddr(drv->global->ioctl_sock,
			       bss->ifname, addr) < 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: %s: failed to re-read MAC address",
			   bss->ifname);
	} else if (bss && os_memcmp(addr, bss->addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Own MAC address on ifindex %d (%s) changed from "
			   MACSTR " to " MACSTR,
			   ifindex, bss->ifname,
			   MAC2STR(bss->addr), MAC2STR(addr));
		os_memcpy(bss->addr, addr, ETH_ALEN);
		if (notify)
			wpa_supplicant_event(drv->ctx,
					     EVENT_INTERFACE_MAC_CHANGED, NULL);
	}
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
	int init_failed;

	extra[0] = '\0';
	pos = extra;
	end = pos + sizeof(extra);
	ifname[0] = '\0';

	attrlen = len;
	attr = (struct rtattr *) buf;
	while (RTA_OK(attr, attrlen)) {
		switch (attr->rta_type) {
		case IFLA_IFNAME:
			if (RTA_PAYLOAD(attr) > IFNAMSIZ)
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

	drv = nl80211_find_drv(global, ifi->ifi_index, buf, len, &init_failed);
	if (!drv)
		goto event_newlink;
	if (init_failed)
		return; /* do not update interface state */

	if (!drv->if_disabled && !(ifi->ifi_flags & IFF_UP)) {
		namebuf[0] = '\0';
		if (if_indextoname(ifi->ifi_index, namebuf) &&
		    linux_iface_up(drv->global->ioctl_sock, namebuf) > 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface down "
				   "event since interface %s is up", namebuf);
			drv->ignore_if_down_event = 0;
			/* Re-read MAC address as it may have changed */
			nl80211_refresh_mac(drv, ifi->ifi_index, 1);
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
					       buf, len, NULL);
			if (!drv)
				return;
		}
	}

	if (drv->if_disabled && (ifi->ifi_flags & IFF_UP)) {
		namebuf[0] = '\0';
		if (if_indextoname(ifi->ifi_index, namebuf) &&
		    linux_iface_up(drv->global->ioctl_sock, namebuf) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s is down",
				   namebuf);
			return;
		}
		wpa_printf(MSG_DEBUG, "nl80211: Interface up (%s/%s)",
			   namebuf, ifname);
		if (os_strcmp(drv->first_bss->ifname, ifname) != 0) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Not the main interface (%s) - do not indicate interface up",
				   drv->first_bss->ifname);
		} else if (if_nametoindex(drv->first_bss->ifname) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s does not exist",
				   drv->first_bss->ifname);
		} else if (drv->if_removed) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignore interface up "
				   "event since interface %s is marked "
				   "removed", drv->first_bss->ifname);
		} else {
			/* Re-read MAC address as it may have changed */
			nl80211_refresh_mac(drv, ifi->ifi_index, 0);

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

event_newlink:
	if (ifname[0])
		wpa_driver_nl80211_event_newlink(global, drv, ifi->ifi_index,
						 ifname);

	if (ifi->ifi_family == AF_BRIDGE && brid && drv) {
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
		add_ifidx(drv, brid, ifi->ifi_index);

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

	extra[0] = '\0';
	pos = extra;
	end = pos + sizeof(extra);
	ifname[0] = '\0';

	attrlen = len;
	attr = (struct rtattr *) buf;
	while (RTA_OK(attr, attrlen)) {
		switch (attr->rta_type) {
		case IFLA_IFNAME:
			if (RTA_PAYLOAD(attr) > IFNAMSIZ)
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

	drv = nl80211_find_drv(global, ifi->ifi_index, buf, len, NULL);

	if (ifi->ifi_family == AF_BRIDGE && brid && drv) {
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
		del_ifidx(drv, brid, ifi->ifi_index);
	}

	if (ifi->ifi_family != AF_BRIDGE || !brid)
		wpa_driver_nl80211_event_dellink(global, drv, ifi->ifi_index,
						 ifname);
}


struct nl80211_get_assoc_freq_arg {
	struct wpa_driver_nl80211_data *drv;
	unsigned int assoc_freq;
	unsigned int ibss_freq;
	u8 assoc_bssid[ETH_ALEN];
	u8 assoc_ssid[SSID_MAX_LEN];
	u8 assoc_ssid_len;
};

static int nl80211_get_assoc_freq_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *bss[NL80211_BSS_MAX + 1];
	static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
		[NL80211_BSS_BSSID] = { .type = NLA_UNSPEC },
		[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
		[NL80211_BSS_INFORMATION_ELEMENTS] = { .type = NLA_UNSPEC },
		[NL80211_BSS_STATUS] = { .type = NLA_U32 },
	};
	struct nl80211_get_assoc_freq_arg *ctx = arg;
	enum nl80211_bss_status status;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_BSS] ||
	    nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS],
			     bss_policy) ||
	    !bss[NL80211_BSS_STATUS])
		return NL_SKIP;

	status = nla_get_u32(bss[NL80211_BSS_STATUS]);
	if (status == NL80211_BSS_STATUS_ASSOCIATED &&
	    bss[NL80211_BSS_FREQUENCY]) {
		ctx->assoc_freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
		wpa_printf(MSG_DEBUG, "nl80211: Associated on %u MHz",
			   ctx->assoc_freq);
	}
	if (status == NL80211_BSS_STATUS_IBSS_JOINED &&
	    bss[NL80211_BSS_FREQUENCY]) {
		ctx->ibss_freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);
		wpa_printf(MSG_DEBUG, "nl80211: IBSS-joined on %u MHz",
			   ctx->ibss_freq);
	}
	if (status == NL80211_BSS_STATUS_ASSOCIATED &&
	    bss[NL80211_BSS_BSSID]) {
		os_memcpy(ctx->assoc_bssid,
			  nla_data(bss[NL80211_BSS_BSSID]), ETH_ALEN);
		wpa_printf(MSG_DEBUG, "nl80211: Associated with "
			   MACSTR, MAC2STR(ctx->assoc_bssid));
	}

	if (status == NL80211_BSS_STATUS_ASSOCIATED &&
	    bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
		const u8 *ie, *ssid;
		size_t ie_len;

		ie = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
		ie_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
		ssid = get_ie(ie, ie_len, WLAN_EID_SSID);
		if (ssid && ssid[1] > 0 && ssid[1] <= SSID_MAX_LEN) {
			ctx->assoc_ssid_len = ssid[1];
			os_memcpy(ctx->assoc_ssid, ssid + 2, ssid[1]);
		}
	}

	return NL_SKIP;
}


int nl80211_get_assoc_ssid(struct wpa_driver_nl80211_data *drv, u8 *ssid)
{
	struct nl_msg *msg;
	int ret;
	struct nl80211_get_assoc_freq_arg arg;
	int count = 0;

try_again:
	msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SCAN);
	os_memset(&arg, 0, sizeof(arg));
	arg.drv = drv;
	ret = send_and_recv_msgs(drv, msg, nl80211_get_assoc_freq_handler,
				 &arg, NULL, NULL);
	if (ret == -EAGAIN) {
		count++;
		if (count >= 10) {
			wpa_printf(MSG_INFO,
				   "nl80211: Failed to receive consistent scan result dump for get_assoc_ssid");
		} else {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Failed to receive consistent scan result dump for get_assoc_ssid - try again");
			goto try_again;
		}
	}
	if (ret == 0) {
		os_memcpy(ssid, arg.assoc_ssid, arg.assoc_ssid_len);
		return arg.assoc_ssid_len;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Scan result fetch failed: ret=%d (%s)",
		   ret, strerror(-ret));
	return ret;
}


unsigned int nl80211_get_assoc_freq(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	int ret;
	struct nl80211_get_assoc_freq_arg arg;
	int count = 0;

try_again:
	msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SCAN);
	os_memset(&arg, 0, sizeof(arg));
	arg.drv = drv;
	ret = send_and_recv_msgs(drv, msg, nl80211_get_assoc_freq_handler,
				 &arg, NULL, NULL);
	if (ret == -EAGAIN) {
		count++;
		if (count >= 10) {
			wpa_printf(MSG_INFO,
				   "nl80211: Failed to receive consistent scan result dump for get_assoc_freq");
		} else {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Failed to receive consistent scan result dump for get_assoc_freq - try again");
			goto try_again;
		}
	}
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
		[NL80211_STA_INFO_BEACON_SIGNAL_AVG] = { .type = NLA_U8 },
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

	if (sinfo[NL80211_STA_INFO_BEACON_SIGNAL_AVG])
		sig_change->avg_beacon_signal =
			(s8)
			nla_get_u8(sinfo[NL80211_STA_INFO_BEACON_SIGNAL_AVG]);
	else
		sig_change->avg_beacon_signal = 0;

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

	sig->current_signal = -WPA_INVALID_NOISE;
	sig->current_txrate = 0;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_GET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, drv->bssid)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(drv, msg, get_link_signal, sig, NULL, NULL);
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

	sig_change->current_noise = WPA_INVALID_NOISE;
	sig_change->frequency = drv->assoc_freq;

	msg = nl80211_drv_msg(drv, NLM_F_DUMP, NL80211_CMD_GET_SURVEY);
	return send_and_recv_msgs(drv, msg, get_link_noise, sig_change,
				  NULL, NULL);
}


static int get_channel_info(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1] = { 0 };
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wpa_channel_info *chan_info = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	os_memset(chan_info, 0, sizeof(struct wpa_channel_info));
	chan_info->chanwidth = CHAN_WIDTH_UNKNOWN;

	if (tb[NL80211_ATTR_WIPHY_FREQ])
		chan_info->frequency =
			nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);
	if (tb[NL80211_ATTR_CHANNEL_WIDTH])
		chan_info->chanwidth = convert2width(
			nla_get_u32(tb[NL80211_ATTR_CHANNEL_WIDTH]));
	if (tb[NL80211_ATTR_WIPHY_CHANNEL_TYPE]) {
		enum nl80211_channel_type ct =
			nla_get_u32(tb[NL80211_ATTR_WIPHY_CHANNEL_TYPE]);

		switch (ct) {
		case NL80211_CHAN_HT40MINUS:
			chan_info->sec_channel = -1;
			break;
		case NL80211_CHAN_HT40PLUS:
			chan_info->sec_channel = 1;
			break;
		default:
			chan_info->sec_channel = 0;
			break;
		}
	}
	if (tb[NL80211_ATTR_CENTER_FREQ1])
		chan_info->center_frq1 =
			nla_get_u32(tb[NL80211_ATTR_CENTER_FREQ1]);
	if (tb[NL80211_ATTR_CENTER_FREQ2])
		chan_info->center_frq2 =
			nla_get_u32(tb[NL80211_ATTR_CENTER_FREQ2]);

	if (chan_info->center_frq2) {
		u8 seg1_idx = 0;

		if (ieee80211_freq_to_chan(chan_info->center_frq2, &seg1_idx) !=
		    NUM_HOSTAPD_MODES)
			chan_info->seg1_idx = seg1_idx;
	}

	return NL_SKIP;
}


static int nl80211_channel_info(void *priv, struct wpa_channel_info *ci)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_GET_INTERFACE);
	return send_and_recv_msgs(drv, msg, get_channel_info, ci, NULL, NULL);
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
	if (send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL))
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
	ret = send_and_recv_msgs(drv, msg, nl80211_get_country, alpha2,
				 NULL, NULL);
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

	global->nlctrl_id = genl_ctrl_resolve(global->nl, "nlctrl");
	if (global->nlctrl_id < 0) {
		wpa_printf(MSG_ERROR,
			   "nl80211: 'nlctrl' generic netlink not found");
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
			   ret, nl_geterror(ret));
		goto err;
	}

	ret = nl_get_multicast_id(global, "nl80211", "mlme");
	if (ret >= 0)
		ret = nl_socket_add_membership(global->nl_event, ret);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not add multicast "
			   "membership for mlme events: %d (%s)",
			   ret, nl_geterror(ret));
		goto err;
	}

	ret = nl_get_multicast_id(global, "nl80211", "regulatory");
	if (ret >= 0)
		ret = nl_socket_add_membership(global->nl_event, ret);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Could not add multicast "
			   "membership for regulatory events: %d (%s)",
			   ret, nl_geterror(ret));
		/* Continue without regulatory events */
	}

	ret = nl_get_multicast_id(global, "nl80211", "vendor");
	if (ret >= 0)
		ret = nl_socket_add_membership(global->nl_event, ret);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Could not add multicast "
			   "membership for vendor events: %d (%s)",
			   ret, nl_geterror(ret));
		/* Continue without vendor events */
	}

	nl_cb_set(global->nl_cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
		  no_seq_check, NULL);
	nl_cb_set(global->nl_cb, NL_CB_VALID, NL_CB_CUSTOM,
		  process_global_event, global);

	nl80211_register_eloop_read(&global->nl_event,
				    wpa_driver_nl80211_event_receive,
				    global->nl_cb, 0);

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
	struct nl_sock *handle;
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
				   groups[i], ret, nl_geterror(ret));
		}
	}
}


static void wpa_driver_nl80211_rfkill_blocked(void *ctx)
{
	struct wpa_driver_nl80211_data *drv = ctx;

	wpa_printf(MSG_DEBUG, "nl80211: RFKILL blocked");

	/*
	 * rtnetlink ifdown handler will report interfaces other than the P2P
	 * Device interface as disabled.
	 */
	if (drv->nlmode == NL80211_IFTYPE_P2P_DEVICE)
		wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_DISABLED, NULL);
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

	if (is_p2p_net_interface(drv->nlmode))
		nl80211_disable_11b_rates(drv, drv->ifindex, 1);

	/*
	 * rtnetlink ifup handler will report interfaces other than the P2P
	 * Device interface as enabled.
	 */
	if (drv->nlmode == NL80211_IFTYPE_P2P_DEVICE)
		wpa_supplicant_event(drv->ctx, EVENT_INTERFACE_ENABLED, NULL);
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


static int nl80211_init_connect_handle(struct i802_bss *bss)
{
	if (bss->nl_connect) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Connect handle already created (nl_connect=%p)",
			   bss->nl_connect);
		return -1;
	}

	bss->nl_connect = nl_create_handle(bss->nl_cb, "connect");
	if (!bss->nl_connect)
		return -1;
	nl80211_register_eloop_read(&bss->nl_connect,
				    wpa_driver_nl80211_event_receive,
				    bss->nl_cb, 1);
	return 0;
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

	nl80211_init_connect_handle(bss);

	return 0;
}


static void nl80211_destroy_bss(struct i802_bss *bss)
{
	nl_cb_put(bss->nl_cb);
	bss->nl_cb = NULL;

	if (bss->nl_connect)
		nl80211_destroy_eloop_handle(&bss->nl_connect, 1);
}


static void
wpa_driver_nl80211_drv_init_rfkill(struct wpa_driver_nl80211_data *drv)
{
	struct rfkill_config *rcfg;

	if (drv->rfkill)
		return;

	rcfg = os_zalloc(sizeof(*rcfg));
	if (!rcfg)
		return;

	rcfg->ctx = drv;

	/* rfkill uses netdev sysfs for initialization. However, P2P Device is
	 * not associated with a netdev, so use the name of some other interface
	 * sharing the same wiphy as the P2P Device interface.
	 *
	 * Note: This is valid, as a P2P Device interface is always dynamically
	 * created and is created only once another wpa_s interface was added.
	 */
	if (drv->nlmode == NL80211_IFTYPE_P2P_DEVICE) {
		struct nl80211_global *global = drv->global;
		struct wpa_driver_nl80211_data *tmp1;

		dl_list_for_each(tmp1, &global->interfaces,
				 struct wpa_driver_nl80211_data, list) {
			if (drv == tmp1 || drv->wiphy_idx != tmp1->wiphy_idx ||
			    !tmp1->rfkill)
				continue;

			wpa_printf(MSG_DEBUG,
				   "nl80211: Use (%s) to initialize P2P Device rfkill",
				   tmp1->first_bss->ifname);
			os_strlcpy(rcfg->ifname, tmp1->first_bss->ifname,
				   sizeof(rcfg->ifname));
			break;
		}
	} else {
		os_strlcpy(rcfg->ifname, drv->first_bss->ifname,
			   sizeof(rcfg->ifname));
	}

	rcfg->blocked_cb = wpa_driver_nl80211_rfkill_blocked;
	rcfg->unblocked_cb = wpa_driver_nl80211_rfkill_unblocked;
	drv->rfkill = rfkill_init(rcfg);
	if (!drv->rfkill) {
		wpa_printf(MSG_DEBUG, "nl80211: RFKILL status not available");
		os_free(rcfg);
	}
}


static void * wpa_driver_nl80211_drv_init(void *ctx, const char *ifname,
					  void *global_priv, int hostapd,
					  const u8 *set_addr,
					  const char *driver_params)
{
	struct wpa_driver_nl80211_data *drv;
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

	drv->num_if_indices = ARRAY_SIZE(drv->default_if_indices);
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

	if (wpa_driver_nl80211_finish_drv_init(drv, set_addr, 1, driver_params))
		goto failed;

	if (drv->capa.flags2 & WPA_DRIVER_FLAGS2_CONTROL_PORT_TX_STATUS) {
		drv->control_port_ap = 1;
		goto skip_wifi_status;
	}

	drv->eapol_tx_sock = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (drv->eapol_tx_sock < 0)
		goto failed;

	if (drv->data_tx_status) {
		int enabled = 1;

		if (setsockopt(drv->eapol_tx_sock, SOL_SOCKET, SO_WIFI_STATUS,
			       &enabled, sizeof(enabled)) < 0) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: wifi status sockopt failed: %s",
				   strerror(errno));
			drv->data_tx_status = 0;
			if (!drv->use_monitor)
				drv->capa.flags &=
					~WPA_DRIVER_FLAGS_EAPOL_TX_STATUS;
		} else {
			eloop_register_read_sock(
				drv->eapol_tx_sock,
				wpa_driver_nl80211_handle_eapol_tx_status,
				drv, NULL);
		}
	}
skip_wifi_status:

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
				  struct nl_sock *nl_handle,
				  u16 type, const u8 *match, size_t match_len,
				  bool multicast)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	char buf[30];

	buf[0] = '\0';
	wpa_snprintf_hex(buf, sizeof(buf), match, match_len);
	wpa_printf(MSG_DEBUG,
		   "nl80211: Register frame type=0x%x (%s) nl_handle=%p match=%s multicast=%d",
		   type, fc2str(type), nl_handle, buf, multicast);

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_REGISTER_FRAME)) ||
	    (multicast && nla_put_flag(msg, NL80211_ATTR_RECEIVE_MULTICAST)) ||
	    nla_put_u16(msg, NL80211_ATTR_FRAME_TYPE, type) ||
	    nla_put(msg, NL80211_ATTR_FRAME_MATCH, match_len, match)) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv(drv->global, nl_handle, msg, NULL, NULL,
			    NULL, NULL);
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
				    bss->nl_cb, 0);
}


static int nl80211_register_action_frame(struct i802_bss *bss,
					 const u8 *match, size_t match_len)
{
	u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_ACTION << 4);
	return nl80211_register_frame(bss, bss->nl_mgmt,
				      type, match, match_len, false);
}


static int nl80211_mgmt_subscribe_non_ap(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4);
	int ret = 0;

	if (nl80211_alloc_mgmt_handle(bss))
		return -1;
	wpa_printf(MSG_DEBUG, "nl80211: Subscribe to mgmt frames with non-AP "
		   "handle %p", bss->nl_mgmt);

	if (drv->nlmode == NL80211_IFTYPE_ADHOC) {
		/* register for any AUTH message */
		nl80211_register_frame(bss, bss->nl_mgmt, type, NULL, 0, false);
	} else if ((drv->capa.flags & WPA_DRIVER_FLAGS_SAE) &&
		   !(drv->capa.flags & WPA_DRIVER_FLAGS_SME)) {
		/* register for SAE Authentication frames */
		nl80211_register_frame(bss, bss->nl_mgmt, type,
				       (u8 *) "\x03\x00", 2, false);
	}

#ifdef CONFIG_PASN
	/* register for PASN Authentication frames */
	if ((drv->capa.flags & WPA_DRIVER_FLAGS_SME) &&
	    nl80211_register_frame(bss, bss->nl_mgmt, type,
				   (u8 *) "\x07\x00", 2, false))
		ret = -1;
#endif /* CONFIG_PASN */

#ifdef CONFIG_INTERWORKING
	/* QoS Map Configure */
	if (nl80211_register_action_frame(bss, (u8 *) "\x01\x04", 2) < 0)
		ret = -1;
#endif /* CONFIG_INTERWORKING */
#if defined(CONFIG_P2P) || defined(CONFIG_INTERWORKING) || defined(CONFIG_DPP)
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
#endif /* CONFIG_P2P || CONFIG_INTERWORKING || CONFIG_DPP */
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
#ifdef CONFIG_DPP
	/* DPP Public Action */
	if (nl80211_register_action_frame(bss,
					  (u8 *) "\x04\x09\x50\x6f\x9a\x1a",
					  6) < 0)
		ret = -1;
#endif /* CONFIG_DPP */
#ifdef CONFIG_OCV
	/* SA Query Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x08\x00", 2) < 0)
		ret = -1;
#endif /* CONFIG_OCV */
	/* SA Query Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x08\x01", 2) < 0)
		ret = -1;
#ifdef CONFIG_TDLS
	if ((drv->capa.flags & WPA_DRIVER_FLAGS_TDLS_SUPPORT)) {
		/* TDLS Discovery Response */
		if (nl80211_register_action_frame(bss, (u8 *) "\x04\x0e", 2) <
		    0)
			ret = -1;
	}
#endif /* CONFIG_TDLS */
#ifdef CONFIG_FST
	/* FST Action frames */
	if (nl80211_register_action_frame(bss, (u8 *) "\x12", 1) < 0)
		ret = -1;
#endif /* CONFIG_FST */

	/* FT Action frames */
	if (nl80211_register_action_frame(bss, (u8 *) "\x06", 1) < 0)
		ret = -1;
	else if (!drv->has_driver_key_mgmt) {
		int i;

		/* Update supported AKMs only if the driver doesn't advertize
		 * any AKM capabilities. */
		drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT |
			WPA_DRIVER_CAPA_KEY_MGMT_FT_PSK;

		/* Update per interface supported AKMs */
		for (i = 0; i < WPA_IF_MAX; i++)
			drv->capa.key_mgmt_iftype[i] = drv->capa.key_mgmt;
	}

	/* WNM - BSS Transition Management Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x07", 2) < 0)
		ret = -1;
	/* WNM-Sleep Mode Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x11", 2) < 0)
		ret = -1;
#ifdef CONFIG_WNM
	/* WNM - Collocated Interference Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a\x0b", 2) < 0)
		ret = -1;
#endif /* CONFIG_WNM */

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

	/* Radio Measurement - Radio Measurement Request */
	if  (!drv->no_rrm &&
	     nl80211_register_action_frame(bss, (u8 *) "\x05\x00", 2) < 0)
		ret = -1;

	/* Radio Measurement - Link Measurement Request */
	if ((drv->capa.rrm_flags & WPA_DRIVER_FLAGS_TX_POWER_INSERTION) &&
	    (nl80211_register_action_frame(bss, (u8 *) "\x05\x02", 2) < 0))
		ret = -1;

	/* Robust AV SCS Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x13\x01", 2) < 0)
		ret = -1;

	/* Robust AV MSCS Response */
	if (nl80211_register_action_frame(bss, (u8 *) "\x13\x05", 2) < 0)
		ret = -1;

	/* Protected QoS Management Action frame */
	if (nl80211_register_action_frame(bss, (u8 *) "\x7e\x50\x6f\x9a\x1a",
					  5) < 0)
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
				   NULL, 0, false) < 0)
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
	ret = send_and_recv(bss->drv->global, bss->nl_mgmt, msg, NULL, NULL,
			    NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Register spurious class3 "
			   "failed: ret=%d (%s)",
			   ret, strerror(-ret));
	}
	return ret;
}


static int nl80211_action_subscribe_ap(struct i802_bss *bss)
{
	int ret = 0;

	/* Public Action frames */
	if (nl80211_register_action_frame(bss, (u8 *) "\x04", 1) < 0)
		ret = -1;
	/* RRM Measurement Report */
	if (nl80211_register_action_frame(bss, (u8 *) "\x05\x01", 2) < 0)
		ret = -1;
	/* RRM Link Measurement Report */
	if (nl80211_register_action_frame(bss, (u8 *) "\x05\x03", 2) < 0)
		ret = -1;
	/* RRM Neighbor Report Request */
	if (nl80211_register_action_frame(bss, (u8 *) "\x05\x04", 2) < 0)
		ret = -1;
	/* FT Action frames */
	if (nl80211_register_action_frame(bss, (u8 *) "\x06", 1) < 0)
		ret = -1;
	/* SA Query */
	if (nl80211_register_action_frame(bss, (u8 *) "\x08", 1) < 0)
		ret = -1;
	/* Protected Dual of Public Action */
	if (nl80211_register_action_frame(bss, (u8 *) "\x09", 1) < 0)
		ret = -1;
	/* WNM */
	if (nl80211_register_action_frame(bss, (u8 *) "\x0a", 1) < 0)
		ret = -1;
	/* WMM */
	if (nl80211_register_action_frame(bss, (u8 *) "\x11", 1) < 0)
		ret = -1;
#ifdef CONFIG_FST
	/* FST Action frames */
	if (nl80211_register_action_frame(bss, (u8 *) "\x12", 1) < 0)
		ret = -1;
#endif /* CONFIG_FST */
	/* Vendor-specific */
	if (nl80211_register_action_frame(bss, (u8 *) "\x7f", 1) < 0)
		ret = -1;

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
					   NULL, 0, false) < 0) {
			goto out_err;
		}
	}

	if (nl80211_action_subscribe_ap(bss))
		goto out_err;

	if (nl80211_register_spurious_class3(bss))
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

	if (nl80211_action_subscribe_ap(bss))
		goto out_err;

	if (bss->drv->device_ap_sme) {
		u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4);

		/* Register for all Authentication frames */
		if (nl80211_register_frame(bss, bss->nl_mgmt, type, NULL, 0,
					   false) < 0)
			wpa_printf(MSG_DEBUG,
				   "nl80211: Failed to subscribe to handle Authentication frames - SAE offload may not work");
	}

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
	nl80211_destroy_eloop_handle(&bss->nl_mgmt, 0);

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
	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);

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
	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);

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

	ret = send_and_recv_msgs(drv, msg, qca_vendor_test_cmd_handler, drv,
				 NULL, NULL);
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

	if (first &&
	    nl80211_get_ifmode(bss) != NL80211_IFTYPE_P2P_DEVICE &&
	    linux_iface_up(drv->global->ioctl_sock, bss->ifname) > 0)
		drv->start_iface_up = 1;

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

	if (first && nl80211_get_ifmode(bss) == NL80211_IFTYPE_STATION)
		drv->start_mode_sta = 1;

	if (drv->hostapd || bss->static_ap)
		nlmode = NL80211_IFTYPE_AP;
	else if (bss->if_dynamic ||
		 nl80211_get_ifmode(bss) == NL80211_IFTYPE_MESH_POINT)
		nlmode = nl80211_get_ifmode(bss);
	else
		nlmode = NL80211_IFTYPE_STATION;

	if (wpa_driver_nl80211_set_mode(bss, nlmode) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not configure driver mode");
		return -1;
	}

	if (nlmode == NL80211_IFTYPE_P2P_DEVICE)
		nl80211_get_macaddr(bss);

	wpa_driver_nl80211_drv_init_rfkill(drv);

	if (!rfkill_is_blocked(drv->rfkill)) {
		int ret = i802_set_iface_flags(bss, 1);
		if (ret) {
			wpa_printf(MSG_ERROR, "nl80211: Could not set "
				   "interface '%s' UP", bss->ifname);
			return ret;
		}

		if (is_p2p_net_interface(nlmode))
			nl80211_disable_11b_rates(bss->drv,
						  bss->drv->ifindex, 1);

		if (nlmode == NL80211_IFTYPE_P2P_DEVICE)
			return ret;
	} else {
		wpa_printf(MSG_DEBUG, "nl80211: Could not yet enable "
			   "interface '%s' due to rfkill", bss->ifname);
		if (nlmode != NL80211_IFTYPE_P2P_DEVICE)
			drv->if_disabled = 1;

		send_rfkill_event = 1;
	}

	if (!drv->hostapd && nlmode != NL80211_IFTYPE_P2P_DEVICE)
		netlink_send_oper_ifla(drv->global->netlink, drv->ifindex,
				       1, IF_OPER_DORMANT);

	if (nlmode != NL80211_IFTYPE_P2P_DEVICE) {
		if (linux_get_ifhwaddr(drv->global->ioctl_sock, bss->ifname,
				       bss->addr))
			return -1;
		os_memcpy(drv->perm_addr, bss->addr, ETH_ALEN);
	}

	if (send_rfkill_event) {
		eloop_register_timeout(0, 0, wpa_driver_nl80211_send_rfkill,
				       drv, drv->ctx);
	}

	if (drv->vendor_cmd_test_avail)
		qca_vendor_test(drv);

	return 0;
}


static int wpa_driver_nl80211_del_beacon(struct i802_bss *bss)
{
	struct nl_msg *msg;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	wpa_printf(MSG_DEBUG, "nl80211: Remove beacon (ifindex=%d)",
		   drv->ifindex);
	nl80211_put_wiphy_data_ap(bss);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_DEL_BEACON);
	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
	unsigned int i;

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
	}

	if (drv->rtnl_sk)
		nl_socket_free(drv->rtnl_sk);

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
		wpa_driver_nl80211_del_beacon(bss);

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
		if (drv->start_mode_sta)
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
	os_free(drv->auth_data);

	if (drv->in_interface_list)
		dl_list_del(&drv->list);

	os_free(drv->extended_capa);
	os_free(drv->extended_capa_mask);
	for (i = 0; i < drv->num_iface_ext_capa; i++) {
		os_free(drv->iface_ext_capa[i].ext_capa);
		os_free(drv->iface_ext_capa[i].ext_capa_mask);
	}
	os_free(drv->first_bss);
#ifdef CONFIG_DRIVER_NL80211_QCA
	os_free(drv->pending_roam_data);
#endif /* CONFIG_DRIVER_NL80211_QCA */
	os_free(drv);
}


static u32 wpa_alg_to_cipher_suite(enum wpa_alg alg, size_t key_len)
{
	switch (alg) {
	case WPA_ALG_WEP:
		if (key_len == 5)
			return RSN_CIPHER_SUITE_WEP40;
		return RSN_CIPHER_SUITE_WEP104;
	case WPA_ALG_TKIP:
		return RSN_CIPHER_SUITE_TKIP;
	case WPA_ALG_CCMP:
		return RSN_CIPHER_SUITE_CCMP;
	case WPA_ALG_GCMP:
		return RSN_CIPHER_SUITE_GCMP;
	case WPA_ALG_CCMP_256:
		return RSN_CIPHER_SUITE_CCMP_256;
	case WPA_ALG_GCMP_256:
		return RSN_CIPHER_SUITE_GCMP_256;
	case WPA_ALG_BIP_CMAC_128:
		return RSN_CIPHER_SUITE_AES_128_CMAC;
	case WPA_ALG_BIP_GMAC_128:
		return RSN_CIPHER_SUITE_BIP_GMAC_128;
	case WPA_ALG_BIP_GMAC_256:
		return RSN_CIPHER_SUITE_BIP_GMAC_256;
	case WPA_ALG_BIP_CMAC_256:
		return RSN_CIPHER_SUITE_BIP_CMAC_256;
	case WPA_ALG_SMS4:
		return RSN_CIPHER_SUITE_SMS4;
	case WPA_ALG_KRK:
		return RSN_CIPHER_SUITE_KRK;
	case WPA_ALG_NONE:
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
		return RSN_CIPHER_SUITE_CCMP_256;
	case WPA_CIPHER_GCMP_256:
		return RSN_CIPHER_SUITE_GCMP_256;
	case WPA_CIPHER_CCMP:
		return RSN_CIPHER_SUITE_CCMP;
	case WPA_CIPHER_GCMP:
		return RSN_CIPHER_SUITE_GCMP;
	case WPA_CIPHER_TKIP:
		return RSN_CIPHER_SUITE_TKIP;
	case WPA_CIPHER_WEP104:
		return RSN_CIPHER_SUITE_WEP104;
	case WPA_CIPHER_WEP40:
		return RSN_CIPHER_SUITE_WEP40;
	case WPA_CIPHER_GTK_NOT_USED:
		return RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED;
	}

	return 0;
}


static int wpa_cipher_to_cipher_suites(unsigned int ciphers, u32 suites[],
				       int max_suites)
{
	int num_suites = 0;

	if (num_suites < max_suites && ciphers & WPA_CIPHER_CCMP_256)
		suites[num_suites++] = RSN_CIPHER_SUITE_CCMP_256;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_GCMP_256)
		suites[num_suites++] = RSN_CIPHER_SUITE_GCMP_256;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_CCMP)
		suites[num_suites++] = RSN_CIPHER_SUITE_CCMP;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_GCMP)
		suites[num_suites++] = RSN_CIPHER_SUITE_GCMP;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_TKIP)
		suites[num_suites++] = RSN_CIPHER_SUITE_TKIP;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_WEP104)
		suites[num_suites++] = RSN_CIPHER_SUITE_WEP104;
	if (num_suites < max_suites && ciphers & WPA_CIPHER_WEP40)
		suites[num_suites++] = RSN_CIPHER_SUITE_WEP40;

	return num_suites;
}


static int wpa_key_mgmt_to_suites(unsigned int key_mgmt_suites, u32 suites[],
				  int max_suites)
{
	int num_suites = 0;

#define __AKM(a, b) \
	if (num_suites < max_suites && \
	    (key_mgmt_suites & (WPA_KEY_MGMT_ ## a))) \
		suites[num_suites++] = (RSN_AUTH_KEY_MGMT_ ## b)
	__AKM(IEEE8021X, UNSPEC_802_1X);
	__AKM(PSK, PSK_OVER_802_1X);
	__AKM(FT_IEEE8021X, FT_802_1X);
	__AKM(FT_PSK, FT_PSK);
	__AKM(IEEE8021X_SHA256, 802_1X_SHA256);
	__AKM(PSK_SHA256, PSK_SHA256);
	__AKM(SAE, SAE);
	__AKM(FT_SAE, FT_SAE);
	__AKM(CCKM, CCKM);
	__AKM(OSEN, OSEN);
	__AKM(IEEE8021X_SUITE_B, 802_1X_SUITE_B);
	__AKM(IEEE8021X_SUITE_B_192, 802_1X_SUITE_B_192);
	__AKM(FILS_SHA256, FILS_SHA256);
	__AKM(FILS_SHA384, FILS_SHA384);
	__AKM(FT_FILS_SHA256, FT_FILS_SHA256);
	__AKM(FT_FILS_SHA384, FT_FILS_SHA384);
	__AKM(OWE, OWE);
	__AKM(DPP, DPP);
	__AKM(FT_IEEE8021X_SHA384, FT_802_1X_SHA384);
#undef __AKM

	return num_suites;
}


#ifdef CONFIG_DRIVER_NL80211_QCA
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
	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Key management set key failed: ret=%d (%s)",
			   ret, strerror(-ret));
	}

	return ret;
}
#endif /* CONFIG_DRIVER_NL80211_QCA */


static int nl80211_set_pmk(struct wpa_driver_nl80211_data *drv,
			   const u8 *key, size_t key_len,
			   const u8 *addr)
{
	struct nl_msg *msg = NULL;
	int ret;

	/*
	 * If the authenticator address is not set, assume it is
	 * the current BSSID.
	 */
	if (!addr && drv->associated)
		addr = drv->bssid;
	else if (!addr)
		return -1;

	wpa_printf(MSG_DEBUG, "nl80211: Set PMK to the driver for " MACSTR,
		   MAC2STR(addr));
	wpa_hexdump_key(MSG_DEBUG, "nl80211: PMK", key, key_len);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_PMK);
	if (!msg ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
	    nla_put(msg, NL80211_ATTR_PMK, key_len, key)) {
		nl80211_nlmsg_clear(msg);
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Set PMK failed: ret=%d (%s)",
			   ret, strerror(-ret));
	}

	return ret;
}


static int wpa_driver_nl80211_set_key(struct i802_bss *bss,
				      struct wpa_driver_set_key_params *params)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ifindex;
	struct nl_msg *msg;
	struct nl_msg *key_msg;
	int ret;
	int skip_set_key = 1;
	const char *ifname = params->ifname;
	enum wpa_alg alg = params->alg;
	const u8 *addr = params->addr;
	int key_idx = params->key_idx;
	int set_tx = params->set_tx;
	const u8 *seq = params->seq;
	size_t seq_len = params->seq_len;
	const u8 *key = params->key;
	size_t key_len = params->key_len;
	int vlan_id = params->vlan_id;
	enum key_flag key_flag = params->key_flag;

	/* Ignore for P2P Device */
	if (drv->nlmode == NL80211_IFTYPE_P2P_DEVICE)
		return 0;

	ifindex = if_nametoindex(ifname);
	wpa_printf(MSG_DEBUG, "%s: ifindex=%d (%s) alg=%d addr=%p key_idx=%d "
		   "set_tx=%d seq_len=%lu key_len=%lu key_flag=0x%x",
		   __func__, ifindex, ifname, alg, addr, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len, key_flag);

	if (check_key_flag(key_flag)) {
		wpa_printf(MSG_DEBUG, "%s: invalid key_flag", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_DRIVER_NL80211_QCA
	if ((key_flag & KEY_FLAG_PMK) &&
	    (drv->capa.flags & WPA_DRIVER_FLAGS_KEY_MGMT_OFFLOAD)) {
		wpa_printf(MSG_DEBUG, "%s: calling issue_key_mgmt_set_key",
			   __func__);
		ret = issue_key_mgmt_set_key(drv, key, key_len);
		return ret;
	}
#endif /* CONFIG_DRIVER_NL80211_QCA */

	if (key_flag & KEY_FLAG_PMK) {
		if (drv->capa.flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE_8021X)
			return nl80211_set_pmk(drv, key, key_len, addr);
		/* The driver does not have any offload mechanism for PMK, so
		 * there is no need to configure this key. */
		return 0;
	}

	ret = -ENOBUFS;
	key_msg = nlmsg_alloc();
	if (!key_msg)
		return ret;

	if ((key_flag & KEY_FLAG_PAIRWISE_MASK) ==
	    KEY_FLAG_PAIRWISE_RX_TX_MODIFY) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: SET_KEY (pairwise RX/TX modify)");
		msg = nl80211_ifindex_msg(drv, ifindex, 0, NL80211_CMD_SET_KEY);
		if (!msg)
			goto fail2;
	} else if (alg == WPA_ALG_NONE && (key_flag & KEY_FLAG_RX_TX)) {
		wpa_printf(MSG_DEBUG, "%s: invalid key_flag to delete key",
			   __func__);
		ret = -EINVAL;
		goto fail2;
	} else if (alg == WPA_ALG_NONE) {
		wpa_printf(MSG_DEBUG, "nl80211: DEL_KEY");
		msg = nl80211_ifindex_msg(drv, ifindex, 0, NL80211_CMD_DEL_KEY);
		if (!msg)
			goto fail2;
	} else {
		u32 suite;

		suite = wpa_alg_to_cipher_suite(alg, key_len);
		if (!suite) {
			ret = -EINVAL;
			goto fail2;
		}
		wpa_printf(MSG_DEBUG, "nl80211: NEW_KEY");
		msg = nl80211_ifindex_msg(drv, ifindex, 0, NL80211_CMD_NEW_KEY);
		if (!msg)
			goto fail2;
		if (nla_put(key_msg, NL80211_KEY_DATA, key_len, key) ||
		    nla_put_u32(key_msg, NL80211_KEY_CIPHER, suite))
			goto fail;
		wpa_hexdump_key(MSG_DEBUG, "nl80211: KEY_DATA", key, key_len);

		if (seq && seq_len) {
			if (nla_put(key_msg, NL80211_KEY_SEQ, seq_len, seq))
				goto fail;
			wpa_hexdump(MSG_DEBUG, "nl80211: KEY_SEQ",
				    seq, seq_len);
		}
	}

	if (addr && !is_broadcast_ether_addr(addr)) {
		wpa_printf(MSG_DEBUG, "   addr=" MACSTR, MAC2STR(addr));
		if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr))
			goto fail;

		if ((key_flag & KEY_FLAG_PAIRWISE_MASK) ==
		    KEY_FLAG_PAIRWISE_RX ||
		    (key_flag & KEY_FLAG_PAIRWISE_MASK) ==
		    KEY_FLAG_PAIRWISE_RX_TX_MODIFY) {
			if (nla_put_u8(key_msg, NL80211_KEY_MODE,
				       key_flag == KEY_FLAG_PAIRWISE_RX ?
				       NL80211_KEY_NO_TX : NL80211_KEY_SET_TX))
				goto fail;
		} else if ((key_flag & KEY_FLAG_GROUP_MASK) ==
			   KEY_FLAG_GROUP_RX) {
			wpa_printf(MSG_DEBUG, "   RSN IBSS RX GTK");
			if (nla_put_u32(key_msg, NL80211_KEY_TYPE,
					NL80211_KEYTYPE_GROUP))
				goto fail;
		} else if (!(key_flag & KEY_FLAG_PAIRWISE)) {
			wpa_printf(MSG_DEBUG,
				   "   key_flag missing PAIRWISE when setting a pairwise key");
			ret = -EINVAL;
			goto fail;
		} else if (alg == WPA_ALG_WEP &&
			   (key_flag & KEY_FLAG_RX_TX) == KEY_FLAG_RX_TX) {
			wpa_printf(MSG_DEBUG, "   unicast WEP key");
			skip_set_key = 0;
		} else {
			wpa_printf(MSG_DEBUG, "   pairwise key");
		}
	} else if ((key_flag & KEY_FLAG_PAIRWISE) ||
		   !(key_flag & KEY_FLAG_GROUP)) {
		wpa_printf(MSG_DEBUG,
			   "   invalid key_flag for a broadcast key");
		ret = -EINVAL;
		goto fail;
	} else {
		wpa_printf(MSG_DEBUG, "   broadcast key");
		if (key_flag & KEY_FLAG_DEFAULT)
			skip_set_key = 0;
	}
	if (nla_put_u8(key_msg, NL80211_KEY_IDX, key_idx) ||
	    nla_put_nested(msg, NL80211_ATTR_KEY, key_msg))
		goto fail;
	nl80211_nlmsg_clear(key_msg);
	nlmsg_free(key_msg);
	key_msg = NULL;

	if (vlan_id && (drv->capa.flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD)) {
		wpa_printf(MSG_DEBUG, "nl80211: VLAN ID %d", vlan_id);
		if (nla_put_u16(msg, NL80211_ATTR_VLAN_ID, vlan_id))
			goto fail;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if ((ret == -ENOENT || ret == -ENOLINK) && alg == WPA_ALG_NONE)
		ret = 0;
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: set_key failed; err=%d %s",
			   ret, strerror(-ret));

	/*
	 * If we failed or don't need to set the key as default (below),
	 * we're done here.
	 */
	if (ret || skip_set_key)
		return ret;
	wpa_printf(MSG_DEBUG, "nl80211: NL80211_CMD_SET_KEY - default key");

	ret = -ENOBUFS;
	key_msg = nlmsg_alloc();
	if (!key_msg)
		return ret;

	msg = nl80211_ifindex_msg(drv, ifindex, 0, NL80211_CMD_SET_KEY);
	if (!msg)
		goto fail2;
	if (!key_msg ||
	    nla_put_u8(key_msg, NL80211_KEY_IDX, key_idx) ||
	    nla_put_flag(key_msg, wpa_alg_bip(alg) ?
			 (key_idx == 6 || key_idx == 7 ?
			  NL80211_KEY_DEFAULT_BEACON :
			  NL80211_KEY_DEFAULT_MGMT) :
			 NL80211_KEY_DEFAULT))
		goto fail;
	if (addr && is_broadcast_ether_addr(addr)) {
		struct nlattr *types;

		types = nla_nest_start(key_msg, NL80211_KEY_DEFAULT_TYPES);
		if (!types ||
		    nla_put_flag(key_msg, NL80211_KEY_DEFAULT_TYPE_MULTICAST))
			goto fail;
		nla_nest_end(key_msg, types);
	} else if (addr) {
		struct nlattr *types;

		types = nla_nest_start(key_msg, NL80211_KEY_DEFAULT_TYPES);
		if (!types ||
		    nla_put_flag(key_msg, NL80211_KEY_DEFAULT_TYPE_UNICAST))
			goto fail;
		nla_nest_end(key_msg, types);
	}

	if (nla_put_nested(msg, NL80211_ATTR_KEY, key_msg))
		goto fail;
	nl80211_nlmsg_clear(key_msg);
	nlmsg_free(key_msg);
	key_msg = NULL;

	if (vlan_id && (drv->capa.flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD)) {
		wpa_printf(MSG_DEBUG, "nl80211: set_key default - VLAN ID %d",
			   vlan_id);
		if (nla_put_u16(msg, NL80211_ATTR_VLAN_ID, vlan_id))
			goto fail;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG,
			   "nl80211: set_key default failed; err=%d %s",
			   ret, strerror(-ret));
	return ret;

fail:
	nl80211_nlmsg_clear(msg);
	nlmsg_free(msg);
fail2:
	nl80211_nlmsg_clear(key_msg);
	nlmsg_free(key_msg);
	return ret;
}


static int nl_add_key(struct nl_msg *msg, enum wpa_alg alg,
		      int key_idx, int defkey,
		      const u8 *seq, size_t seq_len,
		      const u8 *key, size_t key_len)
{
	struct nlattr *key_attr = nla_nest_start(msg, NL80211_ATTR_KEY);
	u32 suite;

	if (!key_attr)
		return -1;

	suite = wpa_alg_to_cipher_suite(alg, key_len);
	if (!suite)
		return -1;

	if (defkey && wpa_alg_bip(alg)) {
		if (nla_put_flag(msg, NL80211_KEY_DEFAULT_MGMT))
			return -1;
	} else if (defkey) {
		if (nla_put_flag(msg, NL80211_KEY_DEFAULT))
			return -1;
	}

	if (nla_put_u8(msg, NL80211_KEY_IDX, key_idx) ||
	    nla_put_u32(msg, NL80211_KEY_CIPHER, suite) ||
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
				RSN_CIPHER_SUITE_WEP40 :
				RSN_CIPHER_SUITE_WEP104) ||
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
			    int local_state_change,
			    struct i802_bss *bss)
{
	int ret;
	struct nl_msg *msg;
	struct nl_sock *nl_connect = get_connect_handle(bss);

	if (!(msg = nl80211_drv_msg(drv, 0, cmd)) ||
	    nla_put_u16(msg, NL80211_ATTR_REASON_CODE, reason_code) ||
	    (addr && nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) ||
	    (local_state_change &&
	     nla_put_flag(msg, NL80211_ATTR_LOCAL_STATE_CHANGE))) {
		nlmsg_free(msg);
		return -1;
	}

	if (nl_connect)
		ret = send_and_recv(drv->global, nl_connect, msg,
				    process_bss_event, bss, NULL, NULL);
	else
		ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: MLME command failed: reason=%u ret=%d (%s)",
			reason_code, ret, strerror(-ret));
	}
	return ret;
}


static int wpa_driver_nl80211_disconnect(struct wpa_driver_nl80211_data *drv,
					 u16 reason_code,
					 struct i802_bss *bss)
{
	int ret;
	int drv_associated = drv->associated;

	wpa_printf(MSG_DEBUG, "%s(reason_code=%d)", __func__, reason_code);
	nl80211_mark_disconnected(drv);
	/* Disconnect command doesn't need BSSID - it uses cached value */
	ret = wpa_driver_nl80211_mlme(drv, NULL, NL80211_CMD_DISCONNECT,
				      reason_code, 0, bss);
	/*
	 * For locally generated disconnect, supplicant already generates a
	 * DEAUTH event, so ignore the event from NL80211.
	 */
	drv->ignore_next_local_disconnect = drv_associated && (ret == 0);

	return ret;
}


static int wpa_driver_nl80211_deauthenticate(struct i802_bss *bss,
					     const u8 *addr, u16 reason_code)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret;
	int drv_associated = drv->associated;

	if (drv->nlmode == NL80211_IFTYPE_ADHOC) {
		nl80211_mark_disconnected(drv);
		return nl80211_leave_ibss(drv, 1);
	}
	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_SME)) {
		return wpa_driver_nl80211_disconnect(drv, reason_code, bss);
	}
	wpa_printf(MSG_DEBUG, "%s(addr=" MACSTR " reason_code=%d)",
		   __func__, MAC2STR(addr), reason_code);
	nl80211_mark_disconnected(drv);
	ret = wpa_driver_nl80211_mlme(drv, addr, NL80211_CMD_DEAUTHENTICATE,
				      reason_code, 0, bss);
	/*
	 * For locally generated deauthenticate, supplicant already generates a
	 * DEAUTH event, so ignore the event from NL80211.
	 */
	drv->ignore_next_local_deauth = drv_associated && (ret == 0);

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

	os_free(drv->auth_data);
	drv->auth_data = NULL;
	drv->auth_data_len = 0;
	if (params->auth_data) {
		drv->auth_data = os_memdup(params->auth_data,
					   params->auth_data_len);
		if (drv->auth_data)
			drv->auth_data_len = params->auth_data_len;
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


static enum nl80211_auth_type get_nl_auth_type(int wpa_auth_alg)
{
	if (wpa_auth_alg & WPA_AUTH_ALG_OPEN)
		return NL80211_AUTHTYPE_OPEN_SYSTEM;
	if (wpa_auth_alg & WPA_AUTH_ALG_SHARED)
		return NL80211_AUTHTYPE_SHARED_KEY;
	if (wpa_auth_alg & WPA_AUTH_ALG_LEAP)
		return NL80211_AUTHTYPE_NETWORK_EAP;
	if (wpa_auth_alg & WPA_AUTH_ALG_FT)
		return NL80211_AUTHTYPE_FT;
	if (wpa_auth_alg & WPA_AUTH_ALG_SAE)
		return NL80211_AUTHTYPE_SAE;
	if (wpa_auth_alg & WPA_AUTH_ALG_FILS)
		return NL80211_AUTHTYPE_FILS_SK;
	if (wpa_auth_alg & WPA_AUTH_ALG_FILS_SK_PFS)
		return NL80211_AUTHTYPE_FILS_SK_PFS;

	return NL80211_AUTHTYPE_MAX;
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
	struct wpa_driver_set_key_params p;

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

	os_memset(&p, 0, sizeof(p));
	p.ifname = bss->ifname;
	p.alg = WPA_ALG_WEP;
	for (i = 0; i < 4; i++) {
		if (!params->wep_key[i])
			continue;
		p.key_idx = i;
		p.set_tx = i == params->wep_tx_keyidx;
		p.key = params->wep_key[i];
		p.key_len = params->wep_key_len[i];
		p.key_flag = i == params->wep_tx_keyidx ?
			KEY_FLAG_GROUP_RX_TX_DEFAULT :
			KEY_FLAG_GROUP_RX_TX;
		wpa_driver_nl80211_set_key(bss, &p);
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
		wpa_printf(MSG_DEBUG, "  * SSID=%s",
			   wpa_ssid_txt(params->ssid, params->ssid_len));
		if (nla_put(msg, NL80211_ATTR_SSID, params->ssid_len,
			    params->ssid))
			goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "  * IEs", params->ie, params->ie_len);
	if (params->ie &&
	    nla_put(msg, NL80211_ATTR_IE, params->ie_len, params->ie))
		goto fail;
	if (params->auth_data) {
		wpa_hexdump(MSG_DEBUG, "  * auth_data", params->auth_data,
			    params->auth_data_len);
		if (nla_put(msg, NL80211_ATTR_SAE_DATA, params->auth_data_len,
			    params->auth_data))
			goto fail;
	}
	type = get_nl_auth_type(params->auth_alg);
	wpa_printf(MSG_DEBUG, "  * Auth Type %d", type);
	if (type == NL80211_AUTHTYPE_MAX ||
	    nla_put_u32(msg, NL80211_ATTR_AUTH_TYPE, type))
		goto fail;
	if (params->local_state_change) {
		wpa_printf(MSG_DEBUG, "  * Local state change only");
		if (nla_put_flag(msg, NL80211_ATTR_LOCAL_STATE_CHANGE))
			goto fail;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: MLME command failed (auth): count=%d ret=%d (%s)",
			count, ret, strerror(-ret));
		count++;
		if ((ret == -EALREADY || ret == -EEXIST) && count == 1 &&
		    params->bssid && !params->local_state_change) {
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
	params.auth_data = drv->auth_data;
	params.auth_data_len = drv->auth_data_len;

	for (i = 0; i < 4; i++) {
		if (drv->auth_wep_key_len[i]) {
			params.wep_key[i] = drv->auth_wep_key[i];
			params.wep_key_len[i] = drv->auth_wep_key_len[i];
		}
	}

	drv->retry_auth = 1;
	return wpa_driver_nl80211_authenticate(bss, &params);
}


static int wpa_driver_nl80211_send_mlme(struct i802_bss *bss, const u8 *data,
					size_t data_len, int noack,
					unsigned int freq, int no_cck,
					int offchanok,
					unsigned int wait_time,
					const u16 *csa_offs,
					size_t csa_offs_len, int no_encrypt)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_mgmt *mgmt;
	int encrypt = !no_encrypt;
	u16 fc;
	int use_cookie = 1;
	int res;

	mgmt = (struct ieee80211_mgmt *) data;
	fc = le_to_host16(mgmt->frame_control);
	wpa_printf(MSG_DEBUG, "nl80211: send_mlme - da=" MACSTR
		   " noack=%d freq=%u no_cck=%d offchanok=%d wait_time=%u no_encrypt=%d fc=0x%x (%s) nlmode=%d",
		   MAC2STR(mgmt->da), noack, freq, no_cck, offchanok, wait_time,
		   no_encrypt, fc, fc2str(fc), drv->nlmode);

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
		wait_time = 0;
		use_cookie = 0;
		no_cck = 1;
		offchanok = 1;
		goto send_frame_cmd;
	}

	if (drv->device_ap_sme && is_ap_interface(drv->nlmode)) {
		if (freq == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: Use bss->freq=%d",
				   bss->freq);
			freq = bss->freq;
		}
		if ((int) freq == bss->freq)
			wait_time = 0;
		goto send_frame_cmd;
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

	if (is_sta_interface(drv->nlmode) &&
	    WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
	    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_AUTH) {
		if (freq == 0 &&
		    (drv->capa.flags & WPA_DRIVER_FLAGS_SAE) &&
		    !(drv->capa.flags & WPA_DRIVER_FLAGS_SME)) {
			freq = nl80211_get_assoc_freq(drv);
			wpa_printf(MSG_DEBUG,
				   "nl80211: send_mlme - Use assoc_freq=%u for external auth",
				   freq);
		}

		/* Allow off channel for PASN authentication */
		if (data_len >= IEEE80211_HDRLEN + 2 &&
		    WPA_GET_LE16(data + IEEE80211_HDRLEN) == WLAN_AUTH_PASN &&
		    !offchanok) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: send_mlme: allow off channel for PASN");
			offchanok = 1;
		}
	}

#ifdef CONFIG_PASN
	if (is_sta_interface(drv->nlmode) &&
	    WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
	     WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_DEAUTH) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: send_mlme: allow Deauthentication frame for PASN");

		use_cookie = 0;
		offchanok = 1;
		goto send_frame_cmd;
	}
#endif /* CONFIG_PASN */

	if (freq == 0 && drv->nlmode == NL80211_IFTYPE_ADHOC) {
		freq = nl80211_get_assoc_freq(drv);
		wpa_printf(MSG_DEBUG,
			   "nl80211: send_mlme - Use assoc_freq=%u for IBSS",
			   freq);
	}
	if (freq == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: send_mlme - Use bss->freq=%u",
			   bss->freq);
		freq = bss->freq;
	}

	if (drv->use_monitor && is_ap_interface(drv->nlmode)) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: send_frame(freq=%u bss->freq=%u) -> send_monitor",
			   freq, bss->freq);
		return nl80211_send_monitor(drv, data, data_len, encrypt,
					    noack);
	}

	if (noack || WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
	    WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_ACTION)
		use_cookie = 0;
send_frame_cmd:
#ifdef CONFIG_TESTING_OPTIONS
	if (no_encrypt && !encrypt && !drv->use_monitor) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Request to send an unencrypted frame - use a monitor interface for this");
		if (nl80211_create_monitor_interface(drv) < 0)
			return -1;
		res = nl80211_send_monitor(drv, data, data_len, encrypt,
					   noack);
		nl80211_remove_monitor_interface(drv);
		return res;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_printf(MSG_DEBUG, "nl80211: send_mlme -> send_frame_cmd");
	res = nl80211_send_frame_cmd(bss, freq, wait_time, data, data_len,
				     use_cookie, no_cck, noack, offchanok,
				     csa_offs, csa_offs_len);

	return res;
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

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
}


static int wpa_driver_nl80211_set_acl(void *priv,
				      struct hostapd_acl_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nl_msg *acl;
	unsigned int i;
	int ret;
	size_t acl_nla_sz, acl_nlmsg_sz, nla_sz, nlmsg_sz;

	if (!(drv->capa.max_acl_mac_addrs))
		return -ENOTSUP;

	if (params->num_mac_acl > drv->capa.max_acl_mac_addrs)
		return -ENOTSUP;

	wpa_printf(MSG_DEBUG, "nl80211: Set %s ACL (num_mac_acl=%u)",
		   params->acl_policy ? "Accept" : "Deny", params->num_mac_acl);

	acl_nla_sz = nla_total_size(ETH_ALEN) * params->num_mac_acl;
	acl_nlmsg_sz = nlmsg_total_size(acl_nla_sz);
	acl = nlmsg_alloc_size(acl_nlmsg_sz);
	if (!acl)
		return -ENOMEM;
	for (i = 0; i < params->num_mac_acl; i++) {
		if (nla_put(acl, i + 1, ETH_ALEN, params->mac_acl[i].addr)) {
			nlmsg_free(acl);
			return -ENOMEM;
		}
	}

	/*
	 * genetlink message header (Length of user header is 0) +
	 * u32 attr: NL80211_ATTR_IFINDEX +
	 * u32 attr: NL80211_ATTR_ACL_POLICY +
	 * nested acl attr
	 */
	nla_sz = GENL_HDRLEN +
		nla_total_size(4) * 2 +
		nla_total_size(acl_nla_sz);
	nlmsg_sz = nlmsg_total_size(nla_sz);
	if (!(msg = nl80211_ifindex_msg_build(drv, nlmsg_alloc_size(nlmsg_sz),
					      drv->ifindex, 0,
					      NL80211_CMD_SET_MAC_ACL)) ||
	    nla_put_u32(msg, NL80211_ATTR_ACL_POLICY, params->acl_policy ?
			NL80211_ACL_POLICY_DENY_UNLESS_LISTED :
			NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED) ||
	    nla_put_nested(msg, NL80211_ATTR_MAC_ADDRS, acl)) {
		nlmsg_free(msg);
		nlmsg_free(acl);
		return -ENOMEM;
	}
	nlmsg_free(acl);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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


static int nl80211_put_dtim_period(struct nl_msg *msg, int dtim_period)
{
	if (dtim_period > 0) {
		wpa_printf(MSG_DEBUG, "  * dtim_period=%d", dtim_period);
		return nla_put_u32(msg, NL80211_ATTR_DTIM_PERIOD, dtim_period);
	}

	return 0;
}


#ifdef CONFIG_MESH
static int nl80211_set_mesh_config(void *priv,
				   struct wpa_driver_mesh_bss_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_MESH_CONFIG);
	if (!msg)
		return -1;

	ret = nl80211_put_mesh_config(msg, params);
	if (ret < 0) {
		nlmsg_free(msg);
		return ret;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Mesh config set failed: %d (%s)",
			   ret, strerror(-ret));
		return ret;
	}
	return 0;
}
#endif /* CONFIG_MESH */


static int nl80211_put_beacon_rate(struct nl_msg *msg, u64 flags, u64 flags2,
				   struct wpa_driver_ap_params *params)
{
	struct nlattr *bands, *band;
	struct nl80211_txrate_vht vht_rate;
	struct nl80211_txrate_he he_rate;

	if (!params->freq ||
	    (params->beacon_rate == 0 &&
	     params->rate_type == BEACON_RATE_LEGACY))
		return 0;

	bands = nla_nest_start(msg, NL80211_ATTR_TX_RATES);
	if (!bands)
		return -1;

	switch (params->freq->mode) {
	case HOSTAPD_MODE_IEEE80211B:
	case HOSTAPD_MODE_IEEE80211G:
		band = nla_nest_start(msg, NL80211_BAND_2GHZ);
		break;
	case HOSTAPD_MODE_IEEE80211A:
		if (is_6ghz_freq(params->freq->freq))
			band = nla_nest_start(msg, NL80211_BAND_6GHZ);
		else
			band = nla_nest_start(msg, NL80211_BAND_5GHZ);
		break;
	case HOSTAPD_MODE_IEEE80211AD:
		band = nla_nest_start(msg, NL80211_BAND_60GHZ);
		break;
	default:
		return 0;
	}

	if (!band)
		return -1;

	os_memset(&vht_rate, 0, sizeof(vht_rate));
	os_memset(&he_rate, 0, sizeof(he_rate));

	switch (params->rate_type) {
	case BEACON_RATE_LEGACY:
		if (!(flags & WPA_DRIVER_FLAGS_BEACON_RATE_LEGACY)) {
			wpa_printf(MSG_INFO,
				   "nl80211: Driver does not support setting Beacon frame rate (legacy)");
			return -1;
		}

		if (nla_put_u8(msg, NL80211_TXRATE_LEGACY,
			       (u8) params->beacon_rate / 5) ||
		    nla_put(msg, NL80211_TXRATE_HT, 0, NULL) ||
		    (params->freq->vht_enabled &&
		     nla_put(msg, NL80211_TXRATE_VHT, sizeof(vht_rate),
			     &vht_rate)))
			return -1;

		wpa_printf(MSG_DEBUG, " * beacon_rate = legacy:%u (* 100 kbps)",
			   params->beacon_rate);
		break;
	case BEACON_RATE_HT:
		if (!(flags & WPA_DRIVER_FLAGS_BEACON_RATE_HT)) {
			wpa_printf(MSG_INFO,
				   "nl80211: Driver does not support setting Beacon frame rate (HT)");
			return -1;
		}
		if (nla_put(msg, NL80211_TXRATE_LEGACY, 0, NULL) ||
		    nla_put_u8(msg, NL80211_TXRATE_HT, params->beacon_rate) ||
		    (params->freq->vht_enabled &&
		     nla_put(msg, NL80211_TXRATE_VHT, sizeof(vht_rate),
			     &vht_rate)))
			return -1;
		wpa_printf(MSG_DEBUG, " * beacon_rate = HT-MCS %u",
			   params->beacon_rate);
		break;
	case BEACON_RATE_VHT:
		if (!(flags & WPA_DRIVER_FLAGS_BEACON_RATE_VHT)) {
			wpa_printf(MSG_INFO,
				   "nl80211: Driver does not support setting Beacon frame rate (VHT)");
			return -1;
		}
		vht_rate.mcs[0] = BIT(params->beacon_rate);
		if (nla_put(msg, NL80211_TXRATE_LEGACY, 0, NULL))
			return -1;
		if (nla_put(msg, NL80211_TXRATE_HT, 0, NULL))
			return -1;
		if (nla_put(msg, NL80211_TXRATE_VHT, sizeof(vht_rate),
			    &vht_rate))
			return -1;
		wpa_printf(MSG_DEBUG, " * beacon_rate = VHT-MCS %u",
			   params->beacon_rate);
		break;
	case BEACON_RATE_HE:
		if (!(flags2 & WPA_DRIVER_FLAGS2_BEACON_RATE_HE)) {
			wpa_printf(MSG_INFO,
				   "nl80211: Driver does not support setting Beacon frame rate (HE)");
			return -1;
		}
		he_rate.mcs[0] = BIT(params->beacon_rate);
		if (nla_put(msg, NL80211_TXRATE_LEGACY, 0, NULL) ||
		    nla_put(msg, NL80211_TXRATE_HT, 0, NULL) ||
		    nla_put(msg, NL80211_TXRATE_VHT, sizeof(vht_rate),
			    &vht_rate) ||
		    nla_put(msg, NL80211_TXRATE_HE, sizeof(he_rate), &he_rate))
			return -1;
		wpa_printf(MSG_DEBUG, " * beacon_rate = HE-MCS %u",
			   params->beacon_rate);
		break;
	}

	nla_nest_end(msg, band);
	nla_nest_end(msg, bands);

	return 0;
}


static int nl80211_set_multicast_to_unicast(struct i802_bss *bss,
					    int multicast_to_unicast)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_MULTICAST_TO_UNICAST);
	if (!msg ||
	    (multicast_to_unicast &&
	     nla_put_flag(msg, NL80211_ATTR_MULTICAST_TO_UNICAST_ENABLED))) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to build NL80211_CMD_SET_MULTICAST_TO_UNICAST msg for %s",
			   bss->ifname);
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);

	switch (ret) {
	case 0:
		wpa_printf(MSG_DEBUG,
			   "nl80211: multicast to unicast %s on interface %s",
			   multicast_to_unicast ? "enabled" : "disabled",
			   bss->ifname);
		break;
	case -EOPNOTSUPP:
		if (!multicast_to_unicast)
			break;
		wpa_printf(MSG_INFO,
			   "nl80211: multicast to unicast not supported on interface %s",
			   bss->ifname);
		break;
	default:
		wpa_printf(MSG_ERROR,
			   "nl80211: %s multicast to unicast failed with %d (%s) on interface %s",
			   multicast_to_unicast ? "enabling" : "disabling",
			   ret, strerror(-ret), bss->ifname);
		break;
	}

	return ret;
}


#ifdef CONFIG_SAE
static int nl80211_put_sae_pwe(struct nl_msg *msg, int pwe)
{
	u8 sae_pwe;

	wpa_printf(MSG_DEBUG, "nl802111: sae_pwe=%d", pwe);
	if (pwe == 0)
		sae_pwe = NL80211_SAE_PWE_HUNT_AND_PECK;
	else if (pwe == 1)
		sae_pwe = NL80211_SAE_PWE_HASH_TO_ELEMENT;
	else if (pwe == 2)
		sae_pwe = NL80211_SAE_PWE_BOTH;
	else if (pwe == 3)
		return 0; /* special test mode */
	else
		return -1;
	if (nla_put_u8(msg, NL80211_ATTR_SAE_PWE, sae_pwe))
		return -1;

	return 0;
}
#endif /* CONFIG_SAE */


#ifdef CONFIG_FILS
static int nl80211_fils_discovery(struct i802_bss *bss, struct nl_msg *msg,
				  struct wpa_driver_ap_params *params)
{
	struct nlattr *attr;

	if (!bss->drv->fils_discovery) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Driver does not support FILS Discovery frame transmission for %s",
			   bss->ifname);
		return -1;
	}

	attr = nla_nest_start(msg, NL80211_ATTR_FILS_DISCOVERY);
	if (!attr ||
	    nla_put_u32(msg, NL80211_FILS_DISCOVERY_ATTR_INT_MIN,
			params->fd_min_int) ||
	    nla_put_u32(msg, NL80211_FILS_DISCOVERY_ATTR_INT_MAX,
			params->fd_max_int) ||
	    (params->fd_frame_tmpl &&
	     nla_put(msg, NL80211_FILS_DISCOVERY_ATTR_TMPL,
		     params->fd_frame_tmpl_len, params->fd_frame_tmpl)))
		return -1;

	nla_nest_end(msg, attr);
	return 0;
}
#endif /* CONFIG_FILS */


#ifdef CONFIG_IEEE80211AX
static int nl80211_unsol_bcast_probe_resp(struct i802_bss *bss,
					  struct nl_msg *msg,
					  struct wpa_driver_ap_params *params)
{
	struct nlattr *attr;

	if (!bss->drv->unsol_bcast_probe_resp) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Driver does not support unsolicited broadcast Probe Response frame transmission for %s",
			   bss->ifname);
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "nl80211: Unsolicited broadcast Probe Response frame interval: %u",
		   params->unsol_bcast_probe_resp_interval);
	attr = nla_nest_start(msg, NL80211_ATTR_UNSOL_BCAST_PROBE_RESP);
	if (!attr ||
	    nla_put_u32(msg, NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_INT,
			params->unsol_bcast_probe_resp_interval) ||
	    (params->unsol_bcast_probe_resp_tmpl &&
	     nla_put(msg, NL80211_UNSOL_BCAST_PROBE_RESP_ATTR_TMPL,
		     params->unsol_bcast_probe_resp_tmpl_len,
		     params->unsol_bcast_probe_resp_tmpl)))
		return -1;

	nla_nest_end(msg, attr);
	return 0;
}
#endif /* CONFIG_IEEE80211AX */


static int wpa_driver_nl80211_set_ap(void *priv,
				     struct wpa_driver_ap_params *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	u8 cmd = NL80211_CMD_NEW_BEACON;
	int ret = -ENOBUFS;
	int beacon_set;
	int num_suites;
	u32 suites[20], suite;
	u32 ver;
#ifdef CONFIG_MESH
	struct wpa_driver_mesh_bss_params mesh_params;
#endif /* CONFIG_MESH */

	beacon_set = params->reenable ? 0 : bss->beacon_set;

	wpa_printf(MSG_DEBUG, "nl80211: Set beacon (beacon_set=%d)",
		   beacon_set);
	if (beacon_set)
		cmd = NL80211_CMD_SET_BEACON;
	else if (!drv->device_ap_sme && !drv->use_monitor &&
		 !nl80211_get_wiphy_data_ap(bss))
		return -ENOBUFS;

	wpa_hexdump(MSG_DEBUG, "nl80211: Beacon head",
		    params->head, params->head_len);
	wpa_hexdump(MSG_DEBUG, "nl80211: Beacon tail",
		    params->tail, params->tail_len);
	wpa_printf(MSG_DEBUG, "nl80211: ifindex=%d", bss->ifindex);
	wpa_printf(MSG_DEBUG, "nl80211: beacon_int=%d", params->beacon_int);
	wpa_printf(MSG_DEBUG, "nl80211: beacon_rate=%u", params->beacon_rate);
	wpa_printf(MSG_DEBUG, "nl80211: rate_type=%d", params->rate_type);
	wpa_printf(MSG_DEBUG, "nl80211: dtim_period=%d", params->dtim_period);
	wpa_printf(MSG_DEBUG, "nl80211: ssid=%s",
		   wpa_ssid_txt(params->ssid, params->ssid_len));
	if (!(msg = nl80211_bss_msg(bss, 0, cmd)) ||
	    nla_put(msg, NL80211_ATTR_BEACON_HEAD, params->head_len,
		    params->head) ||
	    nla_put(msg, NL80211_ATTR_BEACON_TAIL, params->tail_len,
		    params->tail) ||
	    nl80211_put_beacon_int(msg, params->beacon_int) ||
	    nl80211_put_beacon_rate(msg, drv->capa.flags, drv->capa.flags2,
				    params) ||
	    nl80211_put_dtim_period(msg, params->dtim_period) ||
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
	num_suites = wpa_key_mgmt_to_suites(params->key_mgmt_suites,
					    suites, ARRAY_SIZE(suites));
	if (num_suites > NL80211_MAX_NR_AKM_SUITES)
		wpa_printf(MSG_DEBUG,
			   "nl80211: Not enough room for all AKM suites (num_suites=%d > NL80211_MAX_NR_AKM_SUITES)",
			   num_suites);
	else if (num_suites &&
		 nla_put(msg, NL80211_ATTR_AKM_SUITES, num_suites * sizeof(u32),
			 suites))
		goto fail;

	if (params->key_mgmt_suites & WPA_KEY_MGMT_IEEE8021X_NO_WPA &&
	    (!params->pairwise_ciphers ||
	     params->pairwise_ciphers & (WPA_CIPHER_WEP104 | WPA_CIPHER_WEP40)) &&
	    (nla_put_u16(msg, NL80211_ATTR_CONTROL_PORT_ETHERTYPE, ETH_P_PAE) ||
	     nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT)))
		goto fail;

	if (drv->device_ap_sme &&
	    (params->key_mgmt_suites & WPA_KEY_MGMT_SAE) &&
	    nla_put_flag(msg, NL80211_ATTR_EXTERNAL_AUTH_SUPPORT))
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

	if (params->pbss) {
		wpa_printf(MSG_DEBUG, "nl80211: PBSS");
		if (nla_put_flag(msg, NL80211_ATTR_PBSS))
			goto fail;
	}

	if (params->ftm_responder) {
		struct nlattr *ftm;

		if (!(drv->capa.flags & WPA_DRIVER_FLAGS_FTM_RESPONDER)) {
			ret = -ENOTSUP;
			goto fail;
		}

		ftm = nla_nest_start(msg, NL80211_ATTR_FTM_RESPONDER);
		if (!ftm ||
		    nla_put_flag(msg, NL80211_FTM_RESP_ATTR_ENABLED) ||
		    (params->lci &&
		     nla_put(msg, NL80211_FTM_RESP_ATTR_LCI,
			     wpabuf_len(params->lci),
			     wpabuf_head(params->lci))) ||
		    (params->civic &&
		     nla_put(msg, NL80211_FTM_RESP_ATTR_CIVICLOC,
			     wpabuf_len(params->civic),
			     wpabuf_head(params->civic))))
			goto fail;
		nla_nest_end(msg, ftm);
	}

#ifdef CONFIG_IEEE80211AX
	if (params->he_spr_ctrl) {
		struct nlattr *spr;

		spr = nla_nest_start(msg, NL80211_ATTR_HE_OBSS_PD);
		wpa_printf(MSG_DEBUG, "nl80211: he_spr_ctrl=0x%x",
			   params->he_spr_ctrl);

		if (!spr ||
		    nla_put_u8(msg, NL80211_HE_OBSS_PD_ATTR_SR_CTRL,
			       params->he_spr_ctrl) ||
		    ((params->he_spr_ctrl &
		      SPATIAL_REUSE_NON_SRG_OFFSET_PRESENT) &&
		     nla_put_u8(msg, NL80211_HE_OBSS_PD_ATTR_NON_SRG_MAX_OFFSET,
				params->he_spr_non_srg_obss_pd_max_offset)))
			goto fail;

		if ((params->he_spr_ctrl &
		     SPATIAL_REUSE_SRG_INFORMATION_PRESENT) &&
		    (nla_put_u8(msg, NL80211_HE_OBSS_PD_ATTR_MIN_OFFSET,
				params->he_spr_srg_obss_pd_min_offset) ||
		     nla_put_u8(msg, NL80211_HE_OBSS_PD_ATTR_MAX_OFFSET,
				params->he_spr_srg_obss_pd_max_offset) ||
		     nla_put(msg, NL80211_HE_OBSS_PD_ATTR_BSS_COLOR_BITMAP,
			     sizeof(params->he_spr_bss_color_bitmap),
			     params->he_spr_bss_color_bitmap) ||
		     nla_put(msg, NL80211_HE_OBSS_PD_ATTR_PARTIAL_BSSID_BITMAP,
			     sizeof(params->he_spr_partial_bssid_bitmap),
			     params->he_spr_partial_bssid_bitmap)))
			goto fail;

		nla_nest_end(msg, spr);
	}

	if (params->freq && params->freq->he_enabled) {
		struct nlattr *bss_color;

		bss_color = nla_nest_start(msg, NL80211_ATTR_HE_BSS_COLOR);
		if (!bss_color ||
		    (params->he_bss_color_disabled &&
		     nla_put_flag(msg, NL80211_HE_BSS_COLOR_ATTR_DISABLED)) ||
		    (params->he_bss_color_partial &&
		     nla_put_flag(msg, NL80211_HE_BSS_COLOR_ATTR_PARTIAL)) ||
		    nla_put_u8(msg, NL80211_HE_BSS_COLOR_ATTR_COLOR,
			       params->he_bss_color))
			goto fail;
		nla_nest_end(msg, bss_color);
	}

	if (params->twt_responder) {
		wpa_printf(MSG_DEBUG, "nl80211: twt_responder=%d",
			   params->twt_responder);
		if (nla_put_flag(msg, NL80211_ATTR_TWT_RESPONDER))
			goto fail;
	}

	if (params->unsol_bcast_probe_resp_interval &&
	    nl80211_unsol_bcast_probe_resp(bss, msg, params) < 0)
		goto fail;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_SAE
	if (((params->key_mgmt_suites & WPA_KEY_MGMT_SAE) ||
	     (params->key_mgmt_suites & WPA_KEY_MGMT_FT_SAE)) &&
	    nl80211_put_sae_pwe(msg, params->sae_pwe) < 0)
		goto fail;
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
	if (params->fd_max_int && nl80211_fils_discovery(bss, msg, params) < 0)
		goto fail;
#endif /* CONFIG_FILS */

	ret = send_and_recv_msgs_connect_handle(drv, msg, bss, 1);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Beacon set failed: %d (%s)",
			   ret, strerror(-ret));
	} else {
		bss->beacon_set = 1;
		nl80211_set_bss(bss, params->cts_protect, params->preamble,
				params->short_slot_time, params->ht_opmode,
				params->isolate, params->basic_rates);
		nl80211_set_multicast_to_unicast(bss,
						 params->multicast_to_unicast);
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
		} else if (!beacon_set && params->freq) {
			/*
			 * cfg80211 updates the driver on frequence change in AP
			 * mode only at the point when beaconing is started, so
			 * set the initial value here.
			 */
			bss->bandwidth = params->freq->bandwidth;
		}
	}

#ifdef CONFIG_MESH
	if (is_mesh_interface(drv->nlmode) && params->ht_opmode != -1) {
		os_memset(&mesh_params, 0, sizeof(mesh_params));
		mesh_params.flags |= WPA_DRIVER_MESH_CONF_FLAG_HT_OP_MODE;
		mesh_params.ht_opmode = params->ht_opmode;
		ret = nl80211_set_mesh_config(priv, &mesh_params);
		if (ret < 0)
			return ret;
	}
#endif /* CONFIG_MESH */

	return ret;
fail:
	nlmsg_free(msg);
	return ret;
}


static int nl80211_put_freq_params(struct nl_msg *msg,
				   const struct hostapd_freq_params *freq)
{
	enum hostapd_hw_mode hw_mode;
	int is_24ghz;
	u8 channel;

	wpa_printf(MSG_DEBUG, "  * freq=%d", freq->freq);
	if (nla_put_u32(msg, NL80211_ATTR_WIPHY_FREQ, freq->freq))
		return -ENOBUFS;

	wpa_printf(MSG_DEBUG, "  * he_enabled=%d", freq->he_enabled);
	wpa_printf(MSG_DEBUG, "  * vht_enabled=%d", freq->vht_enabled);
	wpa_printf(MSG_DEBUG, "  * ht_enabled=%d", freq->ht_enabled);

	hw_mode = ieee80211_freq_to_chan(freq->freq, &channel);
	is_24ghz = hw_mode == HOSTAPD_MODE_IEEE80211G ||
		hw_mode == HOSTAPD_MODE_IEEE80211B;

	if (freq->vht_enabled || (freq->he_enabled && !is_24ghz)) {
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
	} else if (freq->edmg.channels && freq->edmg.bw_config) {
		wpa_printf(MSG_DEBUG,
			   "  * EDMG configuration: channels=0x%x bw_config=%d",
			   freq->edmg.channels, freq->edmg.bw_config);
		if (nla_put_u8(msg, NL80211_ATTR_WIPHY_EDMG_CHANNELS,
			       freq->edmg.channels) ||
		    nla_put_u8(msg, NL80211_ATTR_WIPHY_EDMG_BW_CONFIG,
			       freq->edmg.bw_config))
			return -1;
	} else {
		wpa_printf(MSG_DEBUG, "  * channel_type=%d",
			   NL80211_CHAN_NO_HT);
		if (nla_put_u32(msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE,
				NL80211_CHAN_NO_HT))
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
		   "nl80211: Set freq %d (ht_enabled=%d, vht_enabled=%d, he_enabled=%d, bandwidth=%d MHz, cf1=%d MHz, cf2=%d MHz)",
		   freq->freq, freq->ht_enabled, freq->vht_enabled, freq->he_enabled,
		   freq->bandwidth, freq->center_freq1, freq->center_freq2);

	msg = nl80211_drv_msg(drv, 0, set_chan ? NL80211_CMD_SET_CHANNEL :
			      NL80211_CMD_SET_WIPHY);
	if (!msg || nl80211_put_freq_params(msg, freq) < 0) {
		nlmsg_free(msg);
		return -1;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
	if (flags & WPA_STA_ASSOCIATED)
		f |= BIT(NL80211_STA_FLAG_ASSOCIATED);

	return f;
}


#ifdef CONFIG_MESH
static u32 sta_plink_state_nl80211(enum mesh_plink_state state)
{
	switch (state) {
	case PLINK_IDLE:
		return NL80211_PLINK_LISTEN;
	case PLINK_OPN_SNT:
		return NL80211_PLINK_OPN_SNT;
	case PLINK_OPN_RCVD:
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

	/*
	 * Set the below properties only in one of the following cases:
	 * 1. New station is added, already associated.
	 * 2. Set WPA_STA_TDLS_PEER station.
	 * 3. Set an already added unassociated station, if driver supports
	 * full AP client state. (Set these properties after station became
	 * associated will be rejected by the driver).
	 */
	if (!params->set || (params->flags & WPA_STA_TDLS_PEER) ||
	    (params->set && FULL_AP_CLIENT_STATE_SUPP(drv->capa.flags) &&
	     (params->flags & WPA_STA_ASSOCIATED))) {
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

		if (params->he_capab) {
			wpa_hexdump(MSG_DEBUG, "  * he_capab",
				    params->he_capab, params->he_capab_len);
			if (nla_put(msg, NL80211_ATTR_HE_CAPABILITY,
				    params->he_capab_len, params->he_capab))
				goto fail;
		}

		if (params->he_6ghz_capab) {
			wpa_hexdump(MSG_DEBUG, "  * he_6ghz_capab",
				    params->he_6ghz_capab,
				    sizeof(*params->he_6ghz_capab));
			if (nla_put(msg, NL80211_ATTR_HE_6GHZ_CAPABILITY,
				    sizeof(*params->he_6ghz_capab),
				    params->he_6ghz_capab))
				goto fail;
		}

		if (params->ext_capab) {
			wpa_hexdump(MSG_DEBUG, "  * ext_capab",
				    params->ext_capab, params->ext_capab_len);
			if (nla_put(msg, NL80211_ATTR_STA_EXT_CAPABILITY,
				    params->ext_capab_len, params->ext_capab))
				goto fail;
		}

		if (is_ap_interface(drv->nlmode) &&
		    nla_put_u8(msg, NL80211_ATTR_STA_SUPPORT_P2P_PS,
			       params->support_p2p_ps ?
			       NL80211_P2P_PS_SUPPORTED :
			       NL80211_P2P_PS_UNSUPPORTED))
			goto fail;
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
			 * a stub STA entry is used for now and for a station
			 * that is still not associated.
			 */
			wpa_printf(MSG_DEBUG, "  * aid=1 (%s workaround)",
				   (params->flags & WPA_STA_TDLS_PEER) ?
				   "TDLS" : "UNASSOC_STA");
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
	} else if (FULL_AP_CLIENT_STATE_SUPP(drv->capa.flags) &&
		   (params->flags & WPA_STA_ASSOCIATED)) {
		wpa_printf(MSG_DEBUG, "  * aid=%u", params->aid);
		wpa_printf(MSG_DEBUG, "  * listen_interval=%u",
			   params->listen_interval);
		if (nla_put_u16(msg, NL80211_ATTR_STA_AID, params->aid) ||
		    nla_put_u16(msg, NL80211_ATTR_STA_LISTEN_INTERVAL,
				params->listen_interval))
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

	/*
	 * If the driver doesn't support full AP client state, ignore ASSOC/AUTH
	 * flags, as nl80211 driver moves a new station, by default, into
	 * associated state.
	 *
	 * On the other hand, if the driver supports that feature and the
	 * station is added in unauthenticated state, set the
	 * authenticated/associated bits in the mask to prevent moving this
	 * station to associated state before it is actually associated.
	 *
	 * This is irrelevant for mesh mode where the station is added to the
	 * driver as authenticated already, and ASSOCIATED isn't part of the
	 * nl80211 API.
	 */
	if (!is_mesh_interface(drv->nlmode)) {
		if (!FULL_AP_CLIENT_STATE_SUPP(drv->capa.flags)) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Ignore ASSOC/AUTH flags since driver doesn't support full AP client state");
			upd.mask &= ~(BIT(NL80211_STA_FLAG_ASSOCIATED) |
				      BIT(NL80211_STA_FLAG_AUTHENTICATED));
		} else if (!params->set &&
			   !(params->flags & WPA_STA_TDLS_PEER)) {
			if (!(params->flags & WPA_STA_AUTHENTICATED))
				upd.mask |= BIT(NL80211_STA_FLAG_AUTHENTICATED);
			if (!(params->flags & WPA_STA_ASSOCIATED))
				upd.mask |= BIT(NL80211_STA_FLAG_ASSOCIATED);
		}
#ifdef CONFIG_MESH
	} else {
		if (params->plink_state == PLINK_ESTAB && params->peer_aid) {
			ret = nla_put_u16(msg, NL80211_ATTR_MESH_PEER_AID,
					  params->peer_aid);
			if (ret)
				goto fail;
		}
#endif /* CONFIG_MESH */
	}

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

	if ((!params->set || (params->flags & WPA_STA_TDLS_PEER) ||
	     FULL_AP_CLIENT_STATE_SUPP(drv->capa.flags)) &&
	     (params->flags & WPA_STA_WMM)) {
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
	{
		del_ifidx(drv2, ifidx, IFIDX_ANY);
		/* Remove all bridges learned for this iface */
		del_ifidx(drv2, IFIDX_ANY, ifidx);
	}

	msg = nl80211_ifindex_msg(drv, ifidx, 0, NL80211_CMD_DEL_INTERFACE);
	if (send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL) == 0)
		return;
	wpa_printf(MSG_ERROR, "Failed to remove interface (ifidx=%d)", ifidx);
}


const char * nl80211_iftype_str(enum nl80211_iftype mode)
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
	case NL80211_IFTYPE_OCB:
		return "OCB";
	case NL80211_IFTYPE_NAN:
		return "NAN";
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

	if ((addr && iftype == NL80211_IFTYPE_P2P_DEVICE) &&
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr))
		goto fail;

	ret = send_and_recv_msgs(drv, msg, handler, arg, NULL, NULL);
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
		add_ifidx(drv, ifidx, IFIDX_ANY);
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
			wpa_printf(MSG_DEBUG,
				   "nl80211: Failed to subscribe for mgmt frames from SME driver - trying to run without it");

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

	nl80211_put_wiphy_data_ap(bss);
	bss->beacon_set = 0;
}


static int nl80211_tx_control_port(void *priv, const u8 *dest,
				   u16 proto, const u8 *buf, size_t len,
				   int no_encrypt)
{
	struct nl80211_ack_ext_arg ext_arg;
	struct i802_bss *bss = priv;
	struct nl_msg *msg;
	u64 cookie = 0;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Send over control port dest=" MACSTR
		   " proto=0x%04x len=%u no_encrypt=%d",
		   MAC2STR(dest), proto, (unsigned int) len, no_encrypt);

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_CONTROL_PORT_FRAME);
	if (!msg ||
	    nla_put_u16(msg, NL80211_ATTR_CONTROL_PORT_ETHERTYPE, proto) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, dest) ||
	    nla_put(msg, NL80211_ATTR_FRAME, len, buf) ||
	    (no_encrypt &&
	     nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	os_memset(&ext_arg, 0, sizeof(struct nl80211_ack_ext_arg));
	ext_arg.ext_data = &cookie;
	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL,
				 ack_handler_cookie, &ext_arg);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: tx_control_port failed: ret=%d (%s)",
			   ret, strerror(-ret));
	} else {
		struct wpa_driver_nl80211_data *drv = bss->drv;

		wpa_printf(MSG_DEBUG,
			   "nl80211: tx_control_port cookie=0x%llx",
			   (long long unsigned int) cookie);
		drv->eapol_tx_cookie = cookie;
	}

	return ret;
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

	/* For now, disable EAPOL TX over control port in AP mode by default
	 * since it does not provide TX status notifications. */
	if (drv->control_port_ap &&
	    (drv->capa.flags & WPA_DRIVER_FLAGS_CONTROL_PORT))
		return nl80211_tx_control_port(bss, addr, ETH_P_EAPOL,
					       data, data_len, !encrypt);

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

	res = nl80211_send_monitor(drv, hdr, len, encrypt, 0);
	if (res < 0) {
		wpa_printf(MSG_ERROR,
			   "hapd_send_eapol - packet len: %lu - failed",
			   (unsigned long) len);
	}
	os_free(hdr);

	return res;
}


static int wpa_driver_nl80211_sta_set_flags(void *priv, const u8 *addr,
					    unsigned int total_flags,
					    unsigned int flags_or,
					    unsigned int flags_and)
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

	return send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);
fail:
	nlmsg_free(msg);
	return -ENOBUFS;
}


static int driver_nl80211_sta_set_airtime_weight(void *priv, const u8 *addr,
						 unsigned int weight)
{
	struct i802_bss *bss = priv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Set STA airtime weight - ifname=%s addr=" MACSTR
		   " weight=%u", bss->ifname, MAC2STR(addr), weight);

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
	    nla_put_u16(msg, NL80211_ATTR_AIRTIME_WEIGHT, weight))
		goto fail;

	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: SET_STATION[AIRTIME_WEIGHT] failed: ret=%d (%s)",
			   ret, strerror(-ret));
	}
	return ret;
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
	ret = send_and_recv_msgs_connect_handle(drv, msg, drv->first_bss, 1);
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

#ifdef CONFIG_HE_OVERRIDES
	if (params->disable_he) {
		wpa_printf(MSG_DEBUG, "  * HE disabled");
		if (nla_put_flag(msg, NL80211_ATTR_DISABLE_HE))
			return -1;
	}
#endif /* CONFIG_HE_OVERRIDES */

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

	wpa_printf(MSG_DEBUG, "  * SSID=%s",
		   wpa_ssid_txt(params->ssid, params->ssid_len));
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

	ret = nl80211_ht_vht_overrides(msg, params);
	if (ret < 0)
		goto fail;

	ret = send_and_recv_msgs_connect_handle(drv, msg, drv->first_bss, 1);
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


static int nl80211_put_fils_connect_params(struct wpa_driver_nl80211_data *drv,
					   struct wpa_driver_associate_params *params,
					   struct nl_msg *msg)
{
	if (params->fils_erp_username_len) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * FILS ERP EMSKname/username",
				  params->fils_erp_username,
				  params->fils_erp_username_len);
		if (nla_put(msg, NL80211_ATTR_FILS_ERP_USERNAME,
			    params->fils_erp_username_len,
			    params->fils_erp_username))
			return -1;
	}

	if (params->fils_erp_realm_len) {
		wpa_hexdump_ascii(MSG_DEBUG, "  * FILS ERP Realm",
				  params->fils_erp_realm,
				  params->fils_erp_realm_len);
		if (nla_put(msg, NL80211_ATTR_FILS_ERP_REALM,
			    params->fils_erp_realm_len, params->fils_erp_realm))
			return -1;
	}

	if (params->fils_erp_rrk_len) {
		wpa_printf(MSG_DEBUG, "  * FILS ERP next seq %u",
			   params->fils_erp_next_seq_num);
		if (nla_put_u16(msg, NL80211_ATTR_FILS_ERP_NEXT_SEQ_NUM,
				params->fils_erp_next_seq_num))
			return -1;

		wpa_printf(MSG_DEBUG, "  * FILS ERP rRK (len=%lu)",
			   (unsigned long) params->fils_erp_rrk_len);
		if (nla_put(msg, NL80211_ATTR_FILS_ERP_RRK,
			    params->fils_erp_rrk_len, params->fils_erp_rrk))
			return -1;
	}

	return 0;
}


static int nl80211_connect_common(struct wpa_driver_nl80211_data *drv,
				  struct wpa_driver_associate_params *params,
				  struct nl_msg *msg)
{
	if (nla_put_flag(msg, NL80211_ATTR_IFACE_SOCKET_OWNER))
		return -1;

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

	if (params->freq.edmg.channels && params->freq.edmg.bw_config) {
		wpa_printf(MSG_DEBUG,
			   "  * EDMG configuration: channels=0x%x bw_config=%d",
			   params->freq.edmg.channels,
			   params->freq.edmg.bw_config);
		if (nla_put_u8(msg, NL80211_ATTR_WIPHY_EDMG_CHANNELS,
			       params->freq.edmg.channels) ||
		    nla_put_u8(msg, NL80211_ATTR_WIPHY_EDMG_BW_CONFIG,
			       params->freq.edmg.bw_config))
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
		wpa_printf(MSG_DEBUG, "  * SSID=%s",
			   wpa_ssid_txt(params->ssid, params->ssid_len));
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
	    params->key_mgmt_suite == WPA_KEY_MGMT_SAE ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FT_SAE ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X_SUITE_B ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FT_IEEE8021X_SHA384 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FILS_SHA256 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FILS_SHA384 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FT_FILS_SHA256 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_FT_FILS_SHA384 ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_OWE ||
	    params->key_mgmt_suite == WPA_KEY_MGMT_DPP) {
		int mgmt = RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X;

		switch (params->key_mgmt_suite) {
		case WPA_KEY_MGMT_CCKM:
			mgmt = RSN_AUTH_KEY_MGMT_CCKM;
			break;
		case WPA_KEY_MGMT_IEEE8021X:
			mgmt = RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
			break;
		case WPA_KEY_MGMT_FT_IEEE8021X:
			mgmt = RSN_AUTH_KEY_MGMT_FT_802_1X;
			break;
		case WPA_KEY_MGMT_FT_PSK:
			mgmt = RSN_AUTH_KEY_MGMT_FT_PSK;
			break;
		case WPA_KEY_MGMT_IEEE8021X_SHA256:
			mgmt = RSN_AUTH_KEY_MGMT_802_1X_SHA256;
			break;
		case WPA_KEY_MGMT_PSK_SHA256:
			mgmt = RSN_AUTH_KEY_MGMT_PSK_SHA256;
			break;
		case WPA_KEY_MGMT_OSEN:
			mgmt = RSN_AUTH_KEY_MGMT_OSEN;
			break;
		case WPA_KEY_MGMT_SAE:
			mgmt = RSN_AUTH_KEY_MGMT_SAE;
			break;
		case WPA_KEY_MGMT_FT_SAE:
			mgmt = RSN_AUTH_KEY_MGMT_FT_SAE;
			break;
		case WPA_KEY_MGMT_IEEE8021X_SUITE_B:
			mgmt = RSN_AUTH_KEY_MGMT_802_1X_SUITE_B;
			break;
		case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
			mgmt = RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192;
			break;
		case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
			mgmt = RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384;
			break;
		case WPA_KEY_MGMT_FILS_SHA256:
			mgmt = RSN_AUTH_KEY_MGMT_FILS_SHA256;
			break;
		case WPA_KEY_MGMT_FILS_SHA384:
			mgmt = RSN_AUTH_KEY_MGMT_FILS_SHA384;
			break;
		case WPA_KEY_MGMT_FT_FILS_SHA256:
			mgmt = RSN_AUTH_KEY_MGMT_FT_FILS_SHA256;
			break;
		case WPA_KEY_MGMT_FT_FILS_SHA384:
			mgmt = RSN_AUTH_KEY_MGMT_FT_FILS_SHA384;
			break;
		case WPA_KEY_MGMT_OWE:
			mgmt = RSN_AUTH_KEY_MGMT_OWE;
			break;
		case WPA_KEY_MGMT_DPP:
			mgmt = RSN_AUTH_KEY_MGMT_DPP;
			break;
		case WPA_KEY_MGMT_PSK:
		default:
			mgmt = RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X;
			break;
		}
		wpa_printf(MSG_DEBUG, "  * akm=0x%x", mgmt);
		if (nla_put_u32(msg, NL80211_ATTR_AKM_SUITES, mgmt))
			return -1;
	}

	if (params->req_handshake_offload &&
	    (drv->capa.flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE_8021X)) {
		    wpa_printf(MSG_DEBUG, "  * WANT_1X_4WAY_HS");
		    if (nla_put_flag(msg, NL80211_ATTR_WANT_1X_4WAY_HS))
			    return -1;
	    }

	/* Add PSK in case of 4-way handshake offload */
	if (params->psk &&
	    (drv->capa.flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE_PSK)) {
		wpa_hexdump_key(MSG_DEBUG, "  * PSK", params->psk, 32);
		if (nla_put(msg, NL80211_ATTR_PMK, 32, params->psk))
			return -1;
	}

	if (nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT))
		return -1;

	if (params->key_mgmt_suite == WPA_KEY_MGMT_IEEE8021X_NO_WPA &&
	    (params->pairwise_suite == WPA_CIPHER_NONE ||
	     params->pairwise_suite == WPA_CIPHER_WEP104 ||
	     params->pairwise_suite == WPA_CIPHER_WEP40) &&
	    (nla_put_u16(msg, NL80211_ATTR_CONTROL_PORT_ETHERTYPE, ETH_P_PAE) ||
	     nla_put_flag(msg, NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT)))
		return -1;

	if (params->rrm_used) {
		u32 drv_rrm_flags = drv->capa.rrm_flags;
		if ((!((drv_rrm_flags &
			WPA_DRIVER_FLAGS_DS_PARAM_SET_IE_IN_PROBES) &&
		       (drv_rrm_flags & WPA_DRIVER_FLAGS_QUIET)) &&
		     !(drv_rrm_flags & WPA_DRIVER_FLAGS_SUPPORT_RRM)) ||
		    nla_put_flag(msg, NL80211_ATTR_USE_RRM))
			return -1;
	}

	if (nl80211_ht_vht_overrides(msg, params) < 0)
		return -1;

	if (params->p2p)
		wpa_printf(MSG_DEBUG, "  * P2P group");

	if (params->pbss) {
		wpa_printf(MSG_DEBUG, "  * PBSS");
		if (nla_put_flag(msg, NL80211_ATTR_PBSS))
			return -1;
	}

	drv->connect_reassoc = 0;
	if (params->prev_bssid) {
		wpa_printf(MSG_DEBUG, "  * prev_bssid=" MACSTR,
			   MAC2STR(params->prev_bssid));
		if (nla_put(msg, NL80211_ATTR_PREV_BSSID, ETH_ALEN,
			    params->prev_bssid))
			return -1;
		drv->connect_reassoc = 1;
	}

	if ((params->auth_alg & WPA_AUTH_ALG_FILS) &&
	    nl80211_put_fils_connect_params(drv, params, msg) != 0)
		return -1;

	if ((params->key_mgmt_suite == WPA_KEY_MGMT_SAE ||
	     params->key_mgmt_suite == WPA_KEY_MGMT_FT_SAE) &&
	    (!(drv->capa.flags & WPA_DRIVER_FLAGS_SME)) &&
	    nla_put_flag(msg, NL80211_ATTR_EXTERNAL_AUTH_SUPPORT))
		return -1;

	return 0;
}


static int wpa_driver_nl80211_try_connect(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_driver_associate_params *params,
	struct i802_bss *bss)
{
	struct nl_msg *msg;
	enum nl80211_auth_type type;
	int ret;
	int algs;

#ifdef CONFIG_DRIVER_NL80211_QCA
	if (params->req_key_mgmt_offload && params->psk &&
	    (params->key_mgmt_suite == WPA_KEY_MGMT_PSK ||
	     params->key_mgmt_suite == WPA_KEY_MGMT_PSK_SHA256 ||
	     params->key_mgmt_suite == WPA_KEY_MGMT_FT_PSK)) {
		wpa_printf(MSG_DEBUG, "nl80211: Key management set PSK");
		ret = issue_key_mgmt_set_key(drv, params->psk, 32);
		if (ret)
			return ret;
	}
#endif /* CONFIG_DRIVER_NL80211_QCA */

	wpa_printf(MSG_DEBUG, "nl80211: Connect (ifindex=%d)", drv->ifindex);
	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_CONNECT);
	if (!msg)
		return -1;

	ret = nl80211_connect_common(drv, params, msg);
	if (ret)
		goto fail;

	if (params->mgmt_frame_protection == MGMT_FRAME_PROTECTION_REQUIRED &&
	    nla_put_u32(msg, NL80211_ATTR_USE_MFP, NL80211_MFP_REQUIRED))
		goto fail;

	if (params->mgmt_frame_protection == MGMT_FRAME_PROTECTION_OPTIONAL &&
	    (drv->capa.flags & WPA_DRIVER_FLAGS_MFP_OPTIONAL) &&
	    nla_put_u32(msg, NL80211_ATTR_USE_MFP, NL80211_MFP_OPTIONAL))
		goto fail;

#ifdef CONFIG_SAE
	if ((params->key_mgmt_suite == WPA_KEY_MGMT_SAE ||
	     params->key_mgmt_suite == WPA_KEY_MGMT_FT_SAE) &&
	    nl80211_put_sae_pwe(msg, params->sae_pwe) < 0)
		goto fail;
#endif /* CONFIG_SAE */

	algs = 0;
	if (params->auth_alg & WPA_AUTH_ALG_OPEN)
		algs++;
	if (params->auth_alg & WPA_AUTH_ALG_SHARED)
		algs++;
	if (params->auth_alg & WPA_AUTH_ALG_LEAP)
		algs++;
	if (params->auth_alg & WPA_AUTH_ALG_FILS)
		algs++;
	if (params->auth_alg & WPA_AUTH_ALG_FT)
		algs++;
	if (algs > 1) {
		wpa_printf(MSG_DEBUG, "  * Leave out Auth Type for automatic "
			   "selection");
		goto skip_auth_type;
	}

	type = get_nl_auth_type(params->auth_alg);
	wpa_printf(MSG_DEBUG, "  * Auth Type %d", type);
	if (type == NL80211_AUTHTYPE_MAX ||
	    nla_put_u32(msg, NL80211_ATTR_AUTH_TYPE, type))
		goto fail;

skip_auth_type:
	ret = nl80211_set_conn_keys(params, msg);
	if (ret)
		goto fail;

	ret = send_and_recv_msgs_connect_handle(drv, msg, bss, 1);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: MLME connect failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
	} else {
#ifdef CONFIG_DRIVER_NL80211_QCA
		drv->roam_indication_done = false;
#endif /* CONFIG_DRIVER_NL80211_QCA */
		wpa_printf(MSG_DEBUG,
			   "nl80211: Connect request send successfully");
	}

fail:
	nl80211_nlmsg_clear(msg);
	nlmsg_free(msg);
	return ret;

}


static int wpa_driver_nl80211_connect(
	struct wpa_driver_nl80211_data *drv,
	struct wpa_driver_associate_params *params,
	struct i802_bss *bss)
{
	int ret;

	/* Store the connection attempted bssid for future use */
	if (params->bssid)
		os_memcpy(drv->auth_attempt_bssid, params->bssid, ETH_ALEN);
	else
		os_memset(drv->auth_attempt_bssid, 0, ETH_ALEN);

	ret = wpa_driver_nl80211_try_connect(drv, params, bss);
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
			    drv, WLAN_REASON_PREV_AUTH_NOT_VALID, bss))
			return -1;
		ret = wpa_driver_nl80211_try_connect(drv, params, bss);
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
		if (params->key_mgmt_suite == WPA_KEY_MGMT_SAE ||
		    params->key_mgmt_suite == WPA_KEY_MGMT_FT_SAE)
			bss->use_nl_connect = 1;
		else
			bss->use_nl_connect = 0;

		return wpa_driver_nl80211_connect(drv, params, bss);
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

	if (params->mgmt_frame_protection == MGMT_FRAME_PROTECTION_REQUIRED &&
	    nla_put_u32(msg, NL80211_ATTR_USE_MFP, NL80211_MFP_REQUIRED))
		goto fail;

	if (params->fils_kek) {
		wpa_printf(MSG_DEBUG, "  * FILS KEK (len=%u)",
			   (unsigned int) params->fils_kek_len);
		if (nla_put(msg, NL80211_ATTR_FILS_KEK, params->fils_kek_len,
			    params->fils_kek))
			goto fail;
	}
	if (params->fils_nonces) {
		wpa_hexdump(MSG_DEBUG, "  * FILS nonces (for AAD)",
			    params->fils_nonces,
			    params->fils_nonces_len);
		if (nla_put(msg, NL80211_ATTR_FILS_NONCES,
			    params->fils_nonces_len, params->fils_nonces))
			goto fail;
	}

	ret = send_and_recv_msgs_connect_handle(drv, msg, drv->first_bss, 1);
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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

	if (TEST_FAIL())
		return -1;

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

		if (i == 0 && was_ap && !is_ap_interface(nlmode) &&
		    bss->brname[0] &&
		    (bss->added_if_into_bridge || bss->already_in_bridge)) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Remove AP interface %s temporarily from the bridge %s to allow its mode to be set to STATION",
				   bss->ifname, bss->brname);
			if (linux_br_del_if(drv->global->ioctl_sock,
					    bss->brname, bss->ifname) < 0)
				wpa_printf(MSG_INFO,
					   "nl80211: Failed to remove interface %s from bridge %s: %s",
					   bss->ifname, bss->brname,
					   strerror(errno));
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


void nl80211_restore_ap_mode(struct i802_bss *bss)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int was_ap = is_ap_interface(drv->nlmode);

	wpa_driver_nl80211_set_mode(bss, drv->ap_scan_as_station);
	if (!was_ap && is_ap_interface(drv->ap_scan_as_station) &&
	    bss->brname[0] &&
	    (bss->added_if_into_bridge || bss->already_in_bridge)) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Add AP interface %s back into the bridge %s",
			   bss->ifname, bss->brname);
		if (linux_br_add_if(drv->global->ioctl_sock, bss->brname,
				    bss->ifname) < 0) {
			wpa_printf(MSG_WARNING,
				   "nl80211: Failed to add interface %s into bridge %s: %s",
				   bss->ifname, bss->brname, strerror(errno));
		}
	}
	drv->ap_scan_as_station = NL80211_IFTYPE_UNSPECIFIED;
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
	nl80211_nlmsg_clear(msg);
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

	return send_and_recv_msgs(drv, msg, get_key_handler, seq, NULL, NULL);
}


static int i802_set_rts(void *priv, int rts)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	u32 val;

	if (rts >= 2347 || rts == -1)
		val = (u32) -1;
	else
		val = rts;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_WIPHY)) ||
	    nla_put_u32(msg, NL80211_ATTR_WIPHY_RTS_THRESHOLD, val)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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

	if (frag >= 2346 || frag == -1)
		val = (u32) -1;
	else
		val = frag;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_SET_WIPHY)) ||
	    nla_put_u32(msg, NL80211_ATTR_WIPHY_FRAG_THRESHOLD, val)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
	res = send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);
	if (res) {
		wpa_printf(MSG_DEBUG, "nl80211: Station flush failed: ret=%d "
			   "(%s)", res, strerror(-res));
	}
	return res;
}


static void get_sta_tid_stats(struct hostap_sta_driver_data *data,
			      struct nlattr *attr)
{
	struct nlattr *tid_stats[NL80211_TID_STATS_MAX + 1], *tidattr;
	struct nlattr *txq_stats[NL80211_TXQ_STATS_MAX + 1];
	static struct nla_policy txq_stats_policy[NL80211_TXQ_STATS_MAX + 1] = {
		[NL80211_TXQ_STATS_BACKLOG_BYTES] = { .type = NLA_U32 },
		[NL80211_TXQ_STATS_BACKLOG_PACKETS] = { .type = NLA_U32 },
	};
	int rem;

	nla_for_each_nested(tidattr, attr, rem) {
		if (nla_parse_nested(tid_stats, NL80211_TID_STATS_MAX,
				     tidattr, NULL) != 0 ||
		    !tid_stats[NL80211_TID_STATS_TXQ_STATS] ||
		    nla_parse_nested(txq_stats, NL80211_TXQ_STATS_MAX,
				     tid_stats[NL80211_TID_STATS_TXQ_STATS],
				     txq_stats_policy) != 0)
			continue;
		/* sum the backlogs over all TIDs for station */
		if (txq_stats[NL80211_TXQ_STATS_BACKLOG_BYTES])
			data->backlog_bytes += nla_get_u32(
				txq_stats[NL80211_TXQ_STATS_BACKLOG_BYTES]);
		if (txq_stats[NL80211_TXQ_STATS_BACKLOG_PACKETS])
			data->backlog_bytes += nla_get_u32(
				txq_stats[NL80211_TXQ_STATS_BACKLOG_PACKETS]);
	}
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
		[NL80211_STA_INFO_RX_BYTES64] = { .type = NLA_U64 },
		[NL80211_STA_INFO_TX_BYTES64] = { .type = NLA_U64 },
		[NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8 },
		[NL80211_STA_INFO_ACK_SIGNAL] = { .type = NLA_U8 },
		[NL80211_STA_INFO_RX_DURATION] = { .type = NLA_U64 },
		[NL80211_STA_INFO_TX_DURATION] = { .type = NLA_U64 },
		[NL80211_STA_INFO_CONNECTED_TIME] = { .type = NLA_U32 },
	};
	struct nlattr *rate[NL80211_RATE_INFO_MAX + 1];
	static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
		[NL80211_RATE_INFO_BITRATE] = { .type = NLA_U16 },
		[NL80211_RATE_INFO_BITRATE32] = { .type = NLA_U32 },
		[NL80211_RATE_INFO_MCS] = { .type = NLA_U8 },
		[NL80211_RATE_INFO_VHT_MCS] = { .type = NLA_U8 },
		[NL80211_RATE_INFO_SHORT_GI] = { .type = NLA_FLAG },
		[NL80211_RATE_INFO_VHT_NSS] = { .type = NLA_U8 },
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
	/* For backwards compatibility, fetch the 32-bit counters first. */
	if (stats[NL80211_STA_INFO_RX_BYTES])
		data->rx_bytes = nla_get_u32(stats[NL80211_STA_INFO_RX_BYTES]);
	if (stats[NL80211_STA_INFO_TX_BYTES])
		data->tx_bytes = nla_get_u32(stats[NL80211_STA_INFO_TX_BYTES]);
	if (stats[NL80211_STA_INFO_RX_BYTES64] &&
	    stats[NL80211_STA_INFO_TX_BYTES64]) {
		/*
		 * The driver supports 64-bit counters, so use them to override
		 * the 32-bit values.
		 */
		data->rx_bytes =
			nla_get_u64(stats[NL80211_STA_INFO_RX_BYTES64]);
		data->tx_bytes =
			nla_get_u64(stats[NL80211_STA_INFO_TX_BYTES64]);
		data->bytes_64bit = 1;
	}
	if (stats[NL80211_STA_INFO_RX_PACKETS])
		data->rx_packets =
			nla_get_u32(stats[NL80211_STA_INFO_RX_PACKETS]);
	if (stats[NL80211_STA_INFO_TX_PACKETS])
		data->tx_packets =
			nla_get_u32(stats[NL80211_STA_INFO_TX_PACKETS]);
	if (stats[NL80211_STA_INFO_RX_DURATION])
		data->rx_airtime =
			nla_get_u64(stats[NL80211_STA_INFO_RX_DURATION]);
	if (stats[NL80211_STA_INFO_TX_DURATION])
		data->tx_airtime =
			nla_get_u64(stats[NL80211_STA_INFO_TX_DURATION]);
	if (stats[NL80211_STA_INFO_TX_FAILED])
		data->tx_retry_failed =
			nla_get_u32(stats[NL80211_STA_INFO_TX_FAILED]);
	if (stats[NL80211_STA_INFO_SIGNAL])
		data->signal = nla_get_u8(stats[NL80211_STA_INFO_SIGNAL]);
	if (stats[NL80211_STA_INFO_ACK_SIGNAL]) {
		data->last_ack_rssi =
			nla_get_u8(stats[NL80211_STA_INFO_ACK_SIGNAL]);
		data->flags |= STA_DRV_DATA_LAST_ACK_RSSI;
	}

	if (stats[NL80211_STA_INFO_CONNECTED_TIME]) {
		data->connected_sec =
			nla_get_u32(stats[NL80211_STA_INFO_CONNECTED_TIME]);
		data->flags |= STA_DRV_DATA_CONN_TIME;
	}

	if (stats[NL80211_STA_INFO_TX_BITRATE] &&
	    nla_parse_nested(rate, NL80211_RATE_INFO_MAX,
			     stats[NL80211_STA_INFO_TX_BITRATE],
			     rate_policy) == 0) {
		if (rate[NL80211_RATE_INFO_BITRATE32])
			data->current_tx_rate =
				nla_get_u32(rate[NL80211_RATE_INFO_BITRATE32]);
		else if (rate[NL80211_RATE_INFO_BITRATE])
			data->current_tx_rate =
				nla_get_u16(rate[NL80211_RATE_INFO_BITRATE]);

		if (rate[NL80211_RATE_INFO_MCS]) {
			data->tx_mcs = nla_get_u8(rate[NL80211_RATE_INFO_MCS]);
			data->flags |= STA_DRV_DATA_TX_MCS;
		}
		if (rate[NL80211_RATE_INFO_VHT_MCS]) {
			data->tx_vhtmcs =
				nla_get_u8(rate[NL80211_RATE_INFO_VHT_MCS]);
			data->flags |= STA_DRV_DATA_TX_VHT_MCS;
		}
		if (rate[NL80211_RATE_INFO_SHORT_GI])
			data->flags |= STA_DRV_DATA_TX_SHORT_GI;
		if (rate[NL80211_RATE_INFO_VHT_NSS]) {
			data->tx_vht_nss =
				nla_get_u8(rate[NL80211_RATE_INFO_VHT_NSS]);
			data->flags |= STA_DRV_DATA_TX_VHT_NSS;
		}
	}

	if (stats[NL80211_STA_INFO_RX_BITRATE] &&
	    nla_parse_nested(rate, NL80211_RATE_INFO_MAX,
			     stats[NL80211_STA_INFO_RX_BITRATE],
			     rate_policy) == 0) {
		if (rate[NL80211_RATE_INFO_BITRATE32])
			data->current_rx_rate =
				nla_get_u32(rate[NL80211_RATE_INFO_BITRATE32]);
		else if (rate[NL80211_RATE_INFO_BITRATE])
			data->current_rx_rate =
				nla_get_u16(rate[NL80211_RATE_INFO_BITRATE]);

		if (rate[NL80211_RATE_INFO_MCS]) {
			data->rx_mcs =
				nla_get_u8(rate[NL80211_RATE_INFO_MCS]);
			data->flags |= STA_DRV_DATA_RX_MCS;
		}
		if (rate[NL80211_RATE_INFO_VHT_MCS]) {
			data->rx_vhtmcs =
				nla_get_u8(rate[NL80211_RATE_INFO_VHT_MCS]);
			data->flags |= STA_DRV_DATA_RX_VHT_MCS;
		}
		if (rate[NL80211_RATE_INFO_SHORT_GI])
			data->flags |= STA_DRV_DATA_RX_SHORT_GI;
		if (rate[NL80211_RATE_INFO_VHT_NSS]) {
			data->rx_vht_nss =
				nla_get_u8(rate[NL80211_RATE_INFO_VHT_NSS]);
			data->flags |= STA_DRV_DATA_RX_VHT_NSS;
		}
	}

	if (stats[NL80211_STA_INFO_TID_STATS])
		get_sta_tid_stats(data, stats[NL80211_STA_INFO_TID_STATS]);

	return NL_SKIP;
}

static int i802_read_sta_data(struct i802_bss *bss,
			      struct hostap_sta_driver_data *data,
			      const u8 *addr)
{
	struct nl_msg *msg;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_GET_STATION)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(bss->drv, msg, get_sta_handler, data,
				  NULL, NULL);
}


static int i802_set_tx_queue_params(void *priv, int queue, int aifs,
				    int cw_min, int cw_max, int burst_time)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *txq, *params;
	int res;

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

	res = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	wpa_printf(MSG_DEBUG,
		   "nl80211: TX queue param set: queue=%d aifs=%d cw_min=%d cw_max=%d burst_time=%d --> res=%d",
		   queue, aifs, cw_min, cw_max, burst_time, res);
	if (res == 0)
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
	    (vlan_id && (drv->capa.flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD) &&
	     nla_put_u16(msg, NL80211_ATTR_VLAN_ID, vlan_id)) ||
	    nla_put_u32(msg, NL80211_ATTR_STA_VLAN, if_nametoindex(ifname))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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

	os_memset(&data, 0, sizeof(data));
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
			   u16 reason)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct ieee80211_mgmt mgmt;
	u8 channel;

	if (ieee80211_freq_to_chan(bss->freq, &channel) ==
	    HOSTAPD_MODE_IEEE80211AD) {
		/* Deauthentication is not used in DMG/IEEE 802.11ad;
		 * disassociate the STA instead. */
		return i802_sta_disassoc(priv, own_addr, addr, reason);
	}

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
					    0, NULL, 0, 0);
}


static int i802_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
			     u16 reason)
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
					    0, NULL, 0, 0);
}


static void dump_ifidx(struct wpa_driver_nl80211_data *drv)
{
	char buf[200], *pos, *end;
	int i, res;

	pos = buf;
	end = pos + sizeof(buf);

	for (i = 0; i < drv->num_if_indices; i++) {
		if (!drv->if_indices[i].ifindex)
			continue;
		res = os_snprintf(pos, end - pos, " %d(%d)",
				  drv->if_indices[i].ifindex,
				  drv->if_indices[i].reason);
		if (os_snprintf_error(end - pos, res))
			break;
		pos += res;
	}
	*pos = '\0';

	wpa_printf(MSG_DEBUG, "nl80211: if_indices[%d]:%s",
		   drv->num_if_indices, buf);
}


static void add_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx,
		      int ifidx_reason)
{
	int i;
	struct drv_nl80211_if_info *old;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Add own interface ifindex %d (ifidx_reason %d)",
		   ifidx, ifidx_reason);
	if (have_ifidx(drv, ifidx, ifidx_reason)) {
		wpa_printf(MSG_DEBUG, "nl80211: ifindex %d already in the list",
			   ifidx);
		return;
	}
	for (i = 0; i < drv->num_if_indices; i++) {
		if (drv->if_indices[i].ifindex == 0) {
			drv->if_indices[i].ifindex = ifidx;
			drv->if_indices[i].reason = ifidx_reason;
			dump_ifidx(drv);
			return;
		}
	}

	if (drv->if_indices != drv->default_if_indices)
		old = drv->if_indices;
	else
		old = NULL;

	drv->if_indices = os_realloc_array(old, drv->num_if_indices + 1,
					   sizeof(*old));
	if (!drv->if_indices) {
		if (!old)
			drv->if_indices = drv->default_if_indices;
		else
			drv->if_indices = old;
		wpa_printf(MSG_ERROR, "Failed to reallocate memory for "
			   "interfaces");
		wpa_printf(MSG_ERROR, "Ignoring EAPOL on interface %d", ifidx);
		return;
	}
	if (!old)
		os_memcpy(drv->if_indices, drv->default_if_indices,
			  sizeof(drv->default_if_indices));
	drv->if_indices[drv->num_if_indices].ifindex = ifidx;
	drv->if_indices[drv->num_if_indices].reason = ifidx_reason;
	drv->num_if_indices++;
	dump_ifidx(drv);
}


static void del_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx,
		      int ifidx_reason)
{
	int i;

	for (i = 0; i < drv->num_if_indices; i++) {
		if ((drv->if_indices[i].ifindex == ifidx ||
		     ifidx == IFIDX_ANY) &&
		    (drv->if_indices[i].reason == ifidx_reason ||
		     ifidx_reason == IFIDX_ANY)) {
			drv->if_indices[i].ifindex = 0;
			drv->if_indices[i].reason = 0;
			break;
		}
	}
	dump_ifidx(drv);
}


static int have_ifidx(struct wpa_driver_nl80211_data *drv, int ifidx,
		      int ifidx_reason)
{
	int i;

	for (i = 0; i < drv->num_if_indices; i++)
		if (drv->if_indices[i].ifindex == ifidx &&
		    (drv->if_indices[i].reason == ifidx_reason ||
		     ifidx_reason == IFIDX_ANY))
			return 1;

	return 0;
}


static int i802_set_wds_sta(void *priv, const u8 *addr, int aid, int val,
			    const char *bridge_ifname, char *ifname_wds)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	char name[IFNAMSIZ + 1];
	union wpa_event_data event;
	int ret;

	ret = os_snprintf(name, sizeof(name), "%s.sta%d", bss->ifname, aid);
	if (ret >= (int) sizeof(name))
		wpa_printf(MSG_WARNING,
			   "nl80211: WDS interface name was truncated");
	else if (ret < 0)
		return ret;

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

			os_memset(&event, 0, sizeof(event));
			event.wds_sta_interface.sta_addr = addr;
			event.wds_sta_interface.ifname = name;
			event.wds_sta_interface.istatus = INTERFACE_ADDED;
			wpa_supplicant_event(bss->ctx,
					     EVENT_WDS_STA_INTERFACE_STATUS,
					     &event);
		}
		if (linux_set_iface_flags(drv->global->ioctl_sock, name, 1)) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to set WDS STA "
				   "interface %s up", name);
		}
		return i802_set_sta_vlan(priv, addr, name, 0);
	} else {
		if (bridge_ifname &&
		    linux_br_del_if(drv->global->ioctl_sock, bridge_ifname,
				    name) < 0)
			wpa_printf(MSG_INFO,
				   "nl80211: Failed to remove interface %s from bridge %s: %s",
				   name, bridge_ifname, strerror(errno));

		i802_set_sta_vlan(priv, addr, bss->ifname, 0);
		nl80211_remove_iface(drv, if_nametoindex(name));
		os_memset(&event, 0, sizeof(event));
		event.wds_sta_interface.sta_addr = addr;
		event.wds_sta_interface.ifname = name;
		event.wds_sta_interface.istatus = INTERFACE_REMOVED;
		wpa_supplicant_event(bss->ctx, EVENT_WDS_STA_INTERFACE_STATUS,
				     &event);
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

	if (have_ifidx(drv, lladdr.sll_ifindex, IFIDX_ANY))
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
		add_ifidx(drv, br_ifindex, drv->ifindex);
	}
	bss->br_ifindex = br_ifindex;

	if (linux_br_get(in_br, ifname) == 0) {
		if (os_strcmp(in_br, brname) == 0) {
			bss->already_in_bridge = 1;
			return 0; /* already in the bridge */
		}

		wpa_printf(MSG_DEBUG, "nl80211: Removing interface %s from "
			   "bridge %s", ifname, in_br);
		if (linux_br_del_if(drv->global->ioctl_sock, in_br, ifname) <
		    0) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to "
				   "remove interface %s from bridge "
				   "%s: %s",
				   ifname, in_br, strerror(errno));
			return -1;
		}
	}

	wpa_printf(MSG_DEBUG, "nl80211: Adding interface %s into bridge %s",
		   ifname, brname);
	if (linux_br_add_if(drv->global->ioctl_sock, brname, ifname) < 0) {
		wpa_printf(MSG_WARNING,
			   "nl80211: Failed to add interface %s into bridge %s: %s",
			   ifname, brname, strerror(errno));
		/* Try to continue without the interface being in a bridge. This
		 * may be needed for some cases, e.g., with Open vSwitch, where
		 * an external component will need to handle bridge
		 * configuration. */
		return 0;
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
	char master_ifname[IFNAMSIZ];
	int ifindex, br_ifindex = 0;
	int br_added = 0;

	bss = wpa_driver_nl80211_drv_init(hapd, params->ifname,
					  params->global_priv, 1,
					  params->bssid, params->driver_params);
	if (bss == NULL)
		return NULL;

	drv = bss->drv;

	if (linux_br_get(master_ifname, params->ifname) == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Interface %s is in bridge %s",
			   params->ifname, master_ifname);
		br_ifindex = if_nametoindex(master_ifname);
		os_strlcpy(bss->brname, master_ifname, IFNAMSIZ);
	} else if ((params->num_bridge == 0 || !params->bridge[0]) &&
		   linux_master_get(master_ifname, params->ifname) == 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Interface %s is in master %s",
			params->ifname, master_ifname);
		/* start listening for EAPOL on the master interface */
		add_ifidx(drv, if_nametoindex(master_ifname), drv->ifindex);

		/* check if master itself is under bridge */
		if (linux_br_get(master_ifname, master_ifname) == 0) {
			wpa_printf(MSG_DEBUG, "nl80211: which is in bridge %s",
				   master_ifname);
			br_ifindex = if_nametoindex(master_ifname);
			os_strlcpy(bss->brname, master_ifname, IFNAMSIZ);
		}
	} else {
		master_ifname[0] = '\0';
	}

	bss->br_ifindex = br_ifindex;

	for (i = 0; i < params->num_bridge; i++) {
		if (params->bridge[i]) {
			ifindex = if_nametoindex(params->bridge[i]);
			if (ifindex)
				add_ifidx(drv, ifindex, drv->ifindex);
			if (ifindex == br_ifindex)
				br_added = 1;
		}
	}

	/* start listening for EAPOL on the default AP interface */
	add_ifidx(drv, drv->ifindex, IFIDX_ANY);

	if (params->num_bridge && params->bridge[0]) {
		if (i802_check_bridge(drv, bss, params->bridge[0],
				      params->ifname) < 0)
			goto failed;
		if (os_strcmp(params->bridge[0], master_ifname) != 0)
			br_added = 1;
	}

	if (!br_added && br_ifindex &&
	    (params->num_bridge == 0 || !params->bridge[0]))
		add_ifidx(drv, br_ifindex, drv->ifindex);

#ifdef CONFIG_LIBNL3_ROUTE
	if (bss->added_if_into_bridge || bss->already_in_bridge) {
		int err;

		drv->rtnl_sk = nl_socket_alloc();
		if (drv->rtnl_sk == NULL) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to allocate nl_sock");
			goto failed;
		}

		err = nl_connect(drv->rtnl_sk, NETLINK_ROUTE);
		if (err) {
			wpa_printf(MSG_ERROR, "nl80211: Failed to connect nl_sock to NETLINK_ROUTE: %s",
				   nl_geterror(err));
			goto failed;
		}
	}
#endif /* CONFIG_LIBNL3_ROUTE */

	if (drv->capa.flags2 & WPA_DRIVER_FLAGS2_CONTROL_PORT_RX) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Do not open EAPOL RX socket - using control port for RX");
		goto skip_eapol_sock;
	}

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
skip_eapol_sock:

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
	default:
		return -1;
	}
}


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
				     const char *bridge, int use_existing,
				     int setup_ap)
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
		if (nlmode == NL80211_IFTYPE_P2P_DEVICE)
			os_memcpy(if_addr, bss->addr, ETH_ALEN);
		else if (linux_get_ifhwaddr(drv->global->ioctl_sock,
					    ifname, if_addr) < 0) {
			if (added)
				nl80211_remove_iface(drv, ifidx);
			return -1;
		}
	}

	if (!addr &&
	    (type == WPA_IF_P2P_CLIENT || type == WPA_IF_P2P_GROUP ||
	     type == WPA_IF_P2P_GO || type == WPA_IF_MESH ||
	     type == WPA_IF_STATION)) {
		/* Enforce unique address */
		u8 new_addr[ETH_ALEN];

		if (linux_get_ifhwaddr(drv->global->ioctl_sock, ifname,
				       new_addr) < 0) {
			if (added)
				nl80211_remove_iface(drv, ifidx);
			return -1;
		}
		if (nl80211_addr_in_use(drv->global, new_addr)) {
			wpa_printf(MSG_DEBUG, "nl80211: Allocate new address "
				   "for interface %s type %d", ifname, type);
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

	if (type == WPA_IF_AP_BSS && setup_ap) {
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
		add_ifidx(drv, ifidx, IFIDX_ANY);

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
				 struct wpa_driver_nl80211_data, list) {
			del_ifidx(drv2, ifindex, IFIDX_ANY);
			del_ifidx(drv2, IFIDX_ANY, ifindex);
		}
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
			wpa_driver_nl80211_del_beacon(bss);
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
				  int save_cookie, int no_cck, int no_ack,
				  int offchanok, const u16 *csa_offs,
				  size_t csa_offs_len)
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
	    (csa_offs && nla_put(msg, NL80211_ATTR_CSA_C_OFFSETS_TX,
				 csa_offs_len * sizeof(u16), csa_offs)) ||
	    nla_put(msg, NL80211_ATTR_FRAME, buf_len, buf))
		goto fail;

	cookie = 0;
	ret = send_and_recv_msgs(drv, msg, cookie_handler, &cookie, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Frame command failed: ret=%d "
			   "(%s) (freq=%u wait=%u)", ret, strerror(-ret),
			   freq, wait);
	} else {
		wpa_printf(MSG_MSGDUMP, "nl80211: Frame TX command accepted%s; "
			   "cookie 0x%llx", no_ack ? " (no ACK)" : "",
			   (long long unsigned int) cookie);

		if (save_cookie)
			drv->send_frame_cookie = no_ack ? (u64) -1 : cookie;

		if (drv->num_send_frame_cookies == MAX_SEND_FRAME_COOKIES) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Drop oldest pending send frame cookie 0x%llx",
				   (long long unsigned int)
				   drv->send_frame_cookies[0]);
			os_memmove(&drv->send_frame_cookies[0],
				   &drv->send_frame_cookies[1],
				   (MAX_SEND_FRAME_COOKIES - 1) *
				   sizeof(u64));
			drv->num_send_frame_cookies--;
		}
		drv->send_frame_cookies[drv->num_send_frame_cookies] = cookie;
		drv->num_send_frame_cookies++;
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
	int offchanok = 1;

	if (is_ap_interface(drv->nlmode) && (int) freq == bss->freq &&
	    bss->beacon_set)
		offchanok = 0;

	wpa_printf(MSG_DEBUG, "nl80211: Send Action frame (ifindex=%d, "
		   "freq=%u MHz wait=%d ms no_cck=%d offchanok=%d)",
		   drv->ifindex, freq, wait_time, no_cck, offchanok);

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

	if (os_memcmp(bss->addr, src, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Use random TA " MACSTR,
			   MAC2STR(src));
		os_memcpy(bss->rand_addr, src, ETH_ALEN);
	} else {
		os_memset(bss->rand_addr, 0, ETH_ALEN);
	}

#ifdef CONFIG_MESH
	if (is_mesh_interface(drv->nlmode)) {
		struct hostapd_hw_modes *modes;
		u16 num_modes, flags;
		u8 dfs_domain;
		int i;

		modes = nl80211_get_hw_feature_data(bss, &num_modes,
						    &flags, &dfs_domain);
		if (dfs_domain != HOSTAPD_DFS_REGION_ETSI &&
		    ieee80211_is_dfs(bss->freq, modes, num_modes))
			offchanok = 0;
		if (modes) {
			for (i = 0; i < num_modes; i++) {
				os_free(modes[i].channels);
				os_free(modes[i].rates);
			}
			os_free(modes);
		}
	}
#endif /* CONFIG_MESH */

	if (is_ap_interface(drv->nlmode) &&
	    (!(drv->capa.flags & WPA_DRIVER_FLAGS_OFFCHANNEL_TX) ||
	     (int) freq == bss->freq || drv->device_ap_sme ||
	     !drv->use_monitor))
		ret = wpa_driver_nl80211_send_mlme(bss, buf, 24 + data_len,
						   0, freq, no_cck, offchanok,
						   wait_time, NULL, 0, 0);
	else
		ret = nl80211_send_frame_cmd(bss, freq, wait_time, buf,
					     24 + data_len,
					     1, no_cck, 0, offchanok, NULL, 0);

	os_free(buf);
	return ret;
}


static void nl80211_frame_wait_cancel(struct i802_bss *bss, u64 cookie)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: Cancel TX frame wait: cookie=0x%llx",
		   (long long unsigned int) cookie);
	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_FRAME_WAIT_CANCEL)) ||
	    nla_put_u64(msg, NL80211_ATTR_COOKIE, cookie)) {
		nlmsg_free(msg);
		return;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: wait cancel failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
}


static void wpa_driver_nl80211_send_action_cancel_wait(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	unsigned int i;
	u64 cookie;

	/* Cancel the last pending TX cookie */
	nl80211_frame_wait_cancel(bss, drv->send_frame_cookie);

	/*
	 * Cancel the other pending TX cookies, if any. This is needed since
	 * the driver may keep a list of all pending offchannel TX operations
	 * and free up the radio only once they have expired or cancelled.
	 */
	for (i = drv->num_send_frame_cookies; i > 0; i--) {
		cookie = drv->send_frame_cookies[i - 1];
		if (cookie != drv->send_frame_cookie)
			nl80211_frame_wait_cancel(bss, cookie);
	}
	drv->num_send_frame_cookies = 0;
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
	ret = send_and_recv_msgs(drv, msg, cookie_handler, &cookie, NULL, NULL);
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
			nl80211_destroy_eloop_handle(&bss->nl_preq, 0);
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
				   NULL, 0, false) < 0)
		goto out_err;

	nl80211_register_eloop_read(&bss->nl_preq,
				    wpa_driver_nl80211_event_receive,
				    bss->nl_cb, 0);

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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
	wpa_driver_nl80211_del_beacon(bss);
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
	wpa_driver_nl80211_del_beacon(bss);
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
	enum nl80211_iftype nlmode = nl80211_get_ifmode(bss);

	if (i802_set_iface_flags(bss, 1))
		wpa_printf(MSG_DEBUG, "nl80211: Failed to set interface up on resume event");

	if (is_p2p_net_interface(nlmode))
		nl80211_disable_11b_rates(bss->drv, bss->drv->ifindex, 1);
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

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
	return send_and_recv_msgs(drv, msg, get_channel_width, sig, NULL, NULL);
}


static int nl80211_signal_poll(void *priv, struct wpa_signal_info *si)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int res;

	os_memset(si, 0, sizeof(*si));
	res = nl80211_get_link_signal(drv, si);
	if (res) {
		if (drv->nlmode != NL80211_IFTYPE_ADHOC &&
		    drv->nlmode != NL80211_IFTYPE_MESH_POINT)
			return res;
		si->current_signal = 0;
	}

	res = nl80211_get_channel_width(drv, si);
	if (res != 0)
		return res;

	return nl80211_get_link_noise(drv, si);
}


static int nl80211_set_param(void *priv, const char *param)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	if (param == NULL)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: driver param='%s'", param);

#ifdef CONFIG_P2P
	if (os_strstr(param, "use_p2p_group_interface=1")) {
		wpa_printf(MSG_DEBUG, "nl80211: Use separate P2P group "
			   "interface");
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_CONCURRENT;
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_MGMT_AND_NON_P2P;
	}
#endif /* CONFIG_P2P */

	if (os_strstr(param, "use_monitor=1"))
		drv->use_monitor = 1;

	if (os_strstr(param, "force_connect_cmd=1")) {
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_SME;
		drv->force_connect_cmd = 1;
	}

	if (os_strstr(param, "force_bss_selection=1"))
		drv->capa.flags |= WPA_DRIVER_FLAGS_BSS_SELECTION;

	if (os_strstr(param, "no_offchannel_tx=1")) {
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_OFFCHANNEL_TX;
		drv->test_use_roc_tx = 1;
	}

	if (os_strstr(param, "control_port=0")) {
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_CONTROL_PORT;
		drv->capa.flags2 &= ~(WPA_DRIVER_FLAGS2_CONTROL_PORT_RX |
				      WPA_DRIVER_FLAGS2_CONTROL_PORT_TX_STATUS);
		drv->control_port_ap = 0;
	}

	if (os_strstr(param, "control_port_ap=1"))
		drv->control_port_ap = 1;

	if (os_strstr(param, "control_port_ap=0")) {
		drv->capa.flags2 &= ~WPA_DRIVER_FLAGS2_CONTROL_PORT_TX_STATUS;
		drv->control_port_ap = 0;
	}

	if (os_strstr(param, "full_ap_client_state=0"))
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_FULL_AP_CLIENT_STATE;

	if (os_strstr(param, "no_rrm=1")) {
		drv->no_rrm = 1;

		if (!bss->in_deinit && !is_ap_interface(drv->nlmode) &&
		    !is_mesh_interface(drv->nlmode)) {
			nl80211_mgmt_unsubscribe(bss, "no_rrm=1");
			if (nl80211_mgmt_subscribe_non_ap(bss) < 0)
				wpa_printf(MSG_DEBUG,
					   "nl80211: Failed to re-register Action frame processing - ignore for now");
		}
	}

	return 0;
}


static void * nl80211_global_init(void *ctx)
{
	struct nl80211_global *global;
	struct netlink_config *cfg;

	global = os_zalloc(sizeof(*global));
	if (global == NULL)
		return NULL;
	global->ctx = ctx;
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
		nl80211_destroy_eloop_handle(&global->nl_event, 0);

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


static int nl80211_pmkid(struct i802_bss *bss, int cmd,
			 struct wpa_pmkid_params *params)
{
	struct nl_msg *msg;
	const size_t PMK_MAX_LEN = 48; /* current cfg80211 limit */

	if (!(msg = nl80211_bss_msg(bss, 0, cmd)) ||
	    (params->pmkid &&
	     nla_put(msg, NL80211_ATTR_PMKID, 16, params->pmkid)) ||
	    (params->bssid &&
	     nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, params->bssid)) ||
	    (params->ssid_len &&
	     nla_put(msg, NL80211_ATTR_SSID, params->ssid_len, params->ssid)) ||
	    (params->fils_cache_id &&
	     nla_put(msg, NL80211_ATTR_FILS_CACHE_ID, 2,
		     params->fils_cache_id)) ||
	    (params->pmk_lifetime &&
	     nla_put_u32(msg, NL80211_ATTR_PMK_LIFETIME,
			 params->pmk_lifetime)) ||
	    (params->pmk_reauth_threshold &&
	     nla_put_u8(msg, NL80211_ATTR_PMK_REAUTH_THRESHOLD,
			params->pmk_reauth_threshold)) ||
	    (cmd != NL80211_CMD_DEL_PMKSA &&
	     params->pmk_len && params->pmk_len <= PMK_MAX_LEN &&
	     nla_put(msg, NL80211_ATTR_PMK, params->pmk_len, params->pmk))) {
		nl80211_nlmsg_clear(msg);
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);
}


static int nl80211_add_pmkid(void *priv, struct wpa_pmkid_params *params)
{
	struct i802_bss *bss = priv;
	int ret;

	if (params->bssid)
		wpa_printf(MSG_DEBUG, "nl80211: Add PMKID for " MACSTR,
			   MAC2STR(params->bssid));
	else if (params->fils_cache_id && params->ssid_len) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Add PMKSA for cache id %02x%02x SSID %s",
			   params->fils_cache_id[0], params->fils_cache_id[1],
			   wpa_ssid_txt(params->ssid, params->ssid_len));
	}

	ret = nl80211_pmkid(bss, NL80211_CMD_SET_PMKSA, params);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: NL80211_CMD_SET_PMKSA failed: %d (%s)",
			   ret, strerror(-ret));
	}

	return ret;
}


static int nl80211_remove_pmkid(void *priv, struct wpa_pmkid_params *params)
{
	struct i802_bss *bss = priv;
	int ret;

	if (params->bssid)
		wpa_printf(MSG_DEBUG, "nl80211: Delete PMKID for " MACSTR,
			   MAC2STR(params->bssid));
	else if (params->fils_cache_id && params->ssid_len) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Delete PMKSA for cache id %02x%02x SSID %s",
			   params->fils_cache_id[0], params->fils_cache_id[1],
			   wpa_ssid_txt(params->ssid, params->ssid_len));
	}

	ret = nl80211_pmkid(bss, NL80211_CMD_DEL_PMKSA, params);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: NL80211_CMD_DEL_PMKSA failed: %d (%s)",
			   ret, strerror(-ret));
	}

	return ret;
}


static int nl80211_flush_pmkid(void *priv)
{
	struct i802_bss *bss = priv;
	struct nl_msg *msg;

	wpa_printf(MSG_DEBUG, "nl80211: Flush PMKIDs");
	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_FLUSH_PMKSA);
	if (!msg)
		return -ENOBUFS;
	return send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);
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
					 survey_results, NULL, NULL);
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
	    (kck_len && nla_put(msg, NL80211_REKEY_DATA_KCK, kck_len, kck)) ||
	    nla_put(msg, NL80211_REKEY_DATA_REPLAY_CTR, NL80211_REPLAY_CTR_LEN,
		    replay_ctr)) {
		nl80211_nlmsg_clear(msg);
		nlmsg_free(msg);
		return;
	}

	nla_nest_end(msg, replay_nested);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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
					 0, 0, NULL, 0, 0) < 0)
		wpa_printf(MSG_DEBUG, "nl80211_send_null_frame: Failed to "
			   "send poll frame");
}

static void nl80211_poll_client(void *priv, const u8 *own_addr, const u8 *addr,
				int qos)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	u64 cookie;
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

	ret = send_and_recv_msgs(drv, msg, cookie_handler, &cookie, NULL, NULL);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "nl80211: Client probe request for "
			   MACSTR " failed: ret=%d (%s)",
			   MAC2STR(addr), ret, strerror(-ret));
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Client probe request addr=" MACSTR
			   " cookie=%llu", MAC2STR(addr),
			   (long long unsigned int) cookie);
	}
}


static int nl80211_set_power_save(struct i802_bss *bss, int enabled)
{
	struct nl_msg *msg;
	int ret;

	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_SET_POWER_SAVE)) ||
	    nla_put_u32(msg, NL80211_ATTR_PS_STATE,
			enabled ? NL80211_PS_ENABLED : NL80211_PS_DISABLED)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(bss->drv, msg, NULL, NULL, NULL, NULL);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Setting PS state %s failed: %d (%s)",
			   enabled ? "enabled" : "disabled",
			   ret, strerror(-ret));
	}
	return ret;
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

	wpa_printf(MSG_DEBUG, "nl80211: Start radar detection (CAC) %d MHz (ht_enabled=%d, vht_enabled=%d, he_enabled=%d, bandwidth=%d MHz, cf1=%d MHz, cf2=%d MHz)",
		   freq->freq, freq->ht_enabled, freq->vht_enabled, freq->he_enabled,
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret == 0)
		return 0;
	wpa_printf(MSG_DEBUG, "nl80211: Failed to start radar detection: "
		   "%d (%s)", ret, strerror(-ret));
	return -1;
}

#ifdef CONFIG_TDLS

static int nl80211_add_peer_capab(struct nl_msg *msg,
				  enum tdls_peer_capability capa)
{
	u32 peer_capab = 0;

	if (!capa)
		return 0;

	if (capa & TDLS_PEER_HT)
		peer_capab |= NL80211_TDLS_PEER_HT;
	if (capa & TDLS_PEER_VHT)
		peer_capab |= NL80211_TDLS_PEER_VHT;
	if (capa & TDLS_PEER_WMM)
		peer_capab |= NL80211_TDLS_PEER_WMM;
	if (capa & TDLS_PEER_HE)
		peer_capab |= NL80211_TDLS_PEER_HE;

	return nla_put_u32(msg, NL80211_ATTR_TDLS_PEER_CAPABILITY,
			   peer_capab);
}


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
	    nla_put_u16(msg, NL80211_ATTR_STATUS_CODE, status_code) ||
	    nl80211_add_peer_capab(msg, peer_capab) ||
	    (initiator && nla_put_flag(msg, NL80211_ATTR_TDLS_INITIATOR)) ||
	    nla_put(msg, NL80211_ATTR_IE, len, buf))
		goto fail;

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);

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
	int res;

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

	res = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	wpa_printf(MSG_DEBUG, "nl80211: TDLS_OPER: oper=%d mac=" MACSTR
		   " --> res=%d (%s)", nl80211_oper, MAC2STR(peer), res,
		   strerror(-res));
	return res;
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

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
}

#endif /* CONFIG TDLS */


static int driver_nl80211_set_key(void *priv,
				  struct wpa_driver_set_key_params *params)
{
	struct i802_bss *bss = priv;

	return wpa_driver_nl80211_set_key(bss, params);
}


static int driver_nl80211_scan2(void *priv,
				struct wpa_driver_scan_params *params)
{
	struct i802_bss *bss = priv;
#ifdef CONFIG_DRIVER_NL80211_QCA
	struct wpa_driver_nl80211_data *drv = bss->drv;

	/*
	 * Do a vendor specific scan if possible. If only_new_results is
	 * set, do a normal scan since a kernel (cfg80211) BSS cache flush
	 * cannot be achieved through a vendor scan. The below condition may
	 * need to be modified if new scan flags are added in the future whose
	 * functionality can only be achieved through a normal scan.
	 */
	if (drv->scan_vendor_cmd_avail && !params->only_new_results)
		return wpa_driver_nl80211_vendor_scan(bss, params);
#endif /* CONFIG_DRIVER_NL80211_QCA */
	return wpa_driver_nl80211_scan(bss, params);
}


static int driver_nl80211_deauthenticate(void *priv, const u8 *addr,
					 u16 reason_code)
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
				    size_t data_len, int noack,
				    unsigned int freq,
				    const u16 *csa_offs, size_t csa_offs_len,
				    int no_encrypt, unsigned int wait)
{
	struct i802_bss *bss = priv;
	return wpa_driver_nl80211_send_mlme(bss, data, data_len, noack,
					    freq, 0, 0, wait, csa_offs,
					    csa_offs_len, no_encrypt);
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

	os_memset(data, 0, sizeof(*data));
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: update_ft_ies failed "
			   "err=%d (%s)", ret, strerror(-ret));
	}

	return ret;
}


static int nl80211_update_dh_ie(void *priv, const u8 *peer_mac,
				u16 reason_code, const u8 *ie, size_t ie_len)
{
	int ret;
	struct nl_msg *msg;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;

	wpa_printf(MSG_DEBUG, "nl80211: Updating DH IE peer: " MACSTR
		   " reason %u", MAC2STR(peer_mac), reason_code);
	if (!(msg = nl80211_bss_msg(bss, 0, NL80211_CMD_UPDATE_OWE_INFO)) ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, peer_mac) ||
	    nla_put_u16(msg, NL80211_ATTR_STATUS_CODE, reason_code) ||
	    (ie && nla_put(msg, NL80211_ATTR_IE, ie_len, ie))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: update_dh_ie failed err=%d (%s)",
			   ret, strerror(-ret));
	}

	return ret;
}


static const u8 * wpa_driver_nl80211_get_macaddr(void *priv)
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
	struct nl_msg *msg;
	char alpha2[3] = { 0, 0, 0 };

	pos = buf;
	end = buf + buflen;

	res = os_snprintf(pos, end - pos,
			  "ifindex=%d\n"
			  "ifname=%s\n"
			  "brname=%s\n"
			  "addr=" MACSTR "\n"
			  "freq=%d\n"
			  "%s%s%s%s%s%s",
			  bss->ifindex,
			  bss->ifname,
			  bss->brname,
			  MAC2STR(bss->addr),
			  bss->freq,
			  bss->beacon_set ? "beacon_set=1\n" : "",
			  bss->added_if_into_bridge ?
			  "added_if_into_bridge=1\n" : "",
			  bss->already_in_bridge ? "already_in_bridge=1\n" : "",
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
				  "capa.mac_addr_rand_scan_supported=%d\n"
				  "capa.conc_capab=%u\n"
				  "capa.max_conc_chan_2_4=%u\n"
				  "capa.max_conc_chan_5_0=%u\n"
				  "capa.max_sched_scan_plans=%u\n"
				  "capa.max_sched_scan_plan_interval=%u\n"
				  "capa.max_sched_scan_plan_iterations=%u\n",
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
				  drv->capa.mac_addr_rand_scan_supported,
				  drv->capa.conc_capab,
				  drv->capa.max_conc_chan_2_4,
				  drv->capa.max_conc_chan_5_0,
				  drv->capa.max_sched_scan_plans,
				  drv->capa.max_sched_scan_plan_interval,
				  drv->capa.max_sched_scan_plan_iterations);
		if (os_snprintf_error(end - pos, res))
			return pos - buf;
		pos += res;
	}

	msg = nlmsg_alloc();
	if (msg &&
	    nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_REG) &&
	    nla_put_u32(msg, NL80211_ATTR_WIPHY, drv->wiphy_idx) == 0) {
		if (send_and_recv_msgs(drv, msg, nl80211_get_country,
				       alpha2, NULL, NULL) == 0 &&
		    alpha2[0]) {
			res = os_snprintf(pos, end - pos, "country=%s\n",
					  alpha2);
			if (os_snprintf_error(end - pos, res))
				return pos - buf;
			pos += res;
		}
	} else {
		nlmsg_free(msg);
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
	int csa_off_len = 0;
	int i;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Channel switch request (cs_count=%u block_tx=%u freq=%d channel=%d sec_channel_offset=%d width=%d cf1=%d cf2=%d%s%s%s)",
		   settings->cs_count, settings->block_tx,
		   settings->freq_params.freq,
		   settings->freq_params.channel,
		   settings->freq_params.sec_channel_offset,
		   settings->freq_params.bandwidth,
		   settings->freq_params.center_freq1,
		   settings->freq_params.center_freq2,
		   settings->freq_params.ht_enabled ? " ht" : "",
		   settings->freq_params.vht_enabled ? " vht" : "",
		   settings->freq_params.he_enabled ? " he" : "");

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_AP_CSA)) {
		wpa_printf(MSG_DEBUG, "nl80211: Driver does not support channel switch command");
		return -EOPNOTSUPP;
	}

	if (drv->nlmode != NL80211_IFTYPE_AP &&
	    drv->nlmode != NL80211_IFTYPE_P2P_GO &&
	    drv->nlmode != NL80211_IFTYPE_MESH_POINT)
		return -EOPNOTSUPP;

	/*
	 * Remove empty counters, assuming Probe Response and Beacon frame
	 * counters match. This implementation assumes that there are only two
	 * counters.
	 */
	if (settings->counter_offset_beacon[0] &&
	    !settings->counter_offset_beacon[1]) {
		csa_off_len = 1;
	} else if (settings->counter_offset_beacon[1] &&
		   !settings->counter_offset_beacon[0]) {
		csa_off_len = 1;
		settings->counter_offset_beacon[0] =
			settings->counter_offset_beacon[1];
		settings->counter_offset_presp[0] =
			settings->counter_offset_presp[1];
	} else if (settings->counter_offset_beacon[1] &&
		   settings->counter_offset_beacon[0]) {
		csa_off_len = 2;
	} else {
		wpa_printf(MSG_ERROR, "nl80211: No CSA counters provided");
		return -EINVAL;
	}

	/* Check CSA counters validity */
	if (drv->capa.max_csa_counters &&
	    csa_off_len > drv->capa.max_csa_counters) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Too many CSA counters provided");
		return -EINVAL;
	}

	if (!settings->beacon_csa.tail)
		return -EINVAL;

	for (i = 0; i < csa_off_len; i++) {
		u16 csa_c_off_bcn = settings->counter_offset_beacon[i];
		u16 csa_c_off_presp = settings->counter_offset_presp[i];

		if ((settings->beacon_csa.tail_len <= csa_c_off_bcn) ||
		    (settings->beacon_csa.tail[csa_c_off_bcn] !=
		     settings->cs_count))
			return -EINVAL;

		if (settings->beacon_csa.probe_resp &&
		    ((settings->beacon_csa.probe_resp_len <=
		      csa_c_off_presp) ||
		     (settings->beacon_csa.probe_resp[csa_c_off_presp] !=
		      settings->cs_count)))
			return -EINVAL;
	}

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

	if (nla_put(msg, NL80211_ATTR_CSA_C_OFF_BEACON,
		    csa_off_len * sizeof(u16),
		    settings->counter_offset_beacon) ||
	    (settings->beacon_csa.probe_resp &&
	     nla_put(msg, NL80211_ATTR_CSA_C_OFF_PRESP,
		     csa_off_len * sizeof(u16),
		     settings->counter_offset_presp)))
		goto fail;

	nla_nest_end(msg, beacon_csa);
	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
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


static bool is_cmd_with_nested_attrs(unsigned int vendor_id,
				     unsigned int subcmd)
{
	if (vendor_id != OUI_QCA)
		return true;

	switch (subcmd) {
	case QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY:
	case QCA_NL80211_VENDOR_SUBCMD_STATS_EXT:
	case QCA_NL80211_VENDOR_SUBCMD_SCANNING_MAC_OUI:
	case QCA_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY:
	case QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS:
	case QCA_NL80211_VENDOR_SUBCMD_NAN:
		return false;
	default:
		return true;
	}
}


static int nl80211_vendor_cmd(void *priv, unsigned int vendor_id,
			      unsigned int subcmd, const u8 *data,
			      size_t data_len, enum nested_attr nested_attr,
			      struct wpabuf *buf)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret, nla_flag;

#ifdef CONFIG_TESTING_OPTIONS
	if (vendor_id == 0xffffffff) {
		msg = nlmsg_alloc();
		if (!msg)
			return -ENOMEM;

		nl80211_cmd(drv, msg, 0, subcmd);
		if (nlmsg_append(msg, (void *) data, data_len, NLMSG_ALIGNTO) <
		    0)
			goto fail;
		/* This test vendor_cmd can be used with nl80211 commands that
		 * need the connect nl_sock, so use the owner-setting variant
		 * of send_and_recv_msgs(). */
		ret = send_and_recv_msgs_owner(drv, msg,
					       get_connect_handle(bss), 0,
					       cmd_reply_handler, buf,
					       NULL, NULL);
		if (ret)
			wpa_printf(MSG_DEBUG, "nl80211: command failed err=%d",
				   ret);
		return ret;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (nested_attr == NESTED_ATTR_USED)
		nla_flag = NLA_F_NESTED;
	else if (nested_attr == NESTED_ATTR_UNSPECIFIED &&
		 is_cmd_with_nested_attrs(vendor_id, subcmd))
		nla_flag = NLA_F_NESTED;
	else
		nla_flag = 0;

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, vendor_id) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD, subcmd) ||
	    (data &&
	     nla_put(msg, nla_flag | NL80211_ATTR_VENDOR_DATA,
		     data_len, data)))
		goto fail;

	ret = send_and_recv_msgs(drv, msg, vendor_reply_handler, buf,
				 NULL, NULL);
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: Setting QoS Map failed");

	return ret;
}


static int get_wowlan_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	int *wowlan_enabled = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	*wowlan_enabled = !!tb[NL80211_ATTR_WOWLAN_TRIGGERS];

	return NL_SKIP;
}


static int nl80211_get_wowlan(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int wowlan_enabled;
	int ret;

	wpa_printf(MSG_DEBUG, "nl80211: Getting wowlan status");

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_GET_WOWLAN);

	ret = send_and_recv_msgs(drv, msg, get_wowlan_handler, &wowlan_enabled,
				 NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Getting wowlan status failed");
		return 0;
	}

	wpa_printf(MSG_DEBUG, "nl80211: wowlan is %s",
		   wowlan_enabled ? "enabled" : "disabled");

	return wowlan_enabled;
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

	if (!(msg = nl80211_cmd_msg(bss, 0, NL80211_CMD_SET_WOWLAN)) ||
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

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: Setting wowlan failed");

	return ret;
}


#ifdef CONFIG_DRIVER_NL80211_QCA
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

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
}


static int nl80211_disable_fils(void *priv, int disable)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *params;

	wpa_printf(MSG_DEBUG, "nl80211: Disable FILS=%d", disable);

	if (!drv->set_wifi_conf_vendor_cmd_avail)
		return -1;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION) ||
	    !(params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_DISABLE_FILS,
		       disable)) {
		nlmsg_free(msg);
		return -1;
	}
	nla_nest_end(msg, params);

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
}


/* Reserved QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID value for wpa_supplicant */
#define WPA_SUPPLICANT_CLIENT_ID 1

static int nl80211_set_bssid_tmp_disallow(void *priv, unsigned int num_bssid,
					  const u8 *bssid)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *params, *nlbssids, *attr;
	unsigned int i;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Set temporarily disallowed BSSIDs (num=%u)",
		   num_bssid);

	if (!drv->roam_vendor_cmd_avail)
		return -1;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ROAM) ||
	    !(params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD,
			QCA_WLAN_VENDOR_ROAMING_SUBCMD_SET_BLACKLIST_BSSID) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID,
			WPA_SUPPLICANT_CLIENT_ID) ||
	    nla_put_u32(msg,
			QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID,
			num_bssid))
		goto fail;

	nlbssids = nla_nest_start(
		msg, QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS);
	if (!nlbssids)
		goto fail;

	for (i = 0; i < num_bssid; i++) {
		attr = nla_nest_start(msg, i);
		if (!attr)
			goto fail;
		if (nla_put(msg,
			    QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID,
			    ETH_ALEN, &bssid[i * ETH_ALEN]))
			goto fail;
		wpa_printf(MSG_DEBUG, "nl80211:   BSSID[%u]: " MACSTR, i,
			   MAC2STR(&bssid[i * ETH_ALEN]));
		nla_nest_end(msg, attr);
	}
	nla_nest_end(msg, nlbssids);
	nla_nest_end(msg, params);

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);

fail:
	nlmsg_free(msg);
	return -1;
}


static int nl80211_add_sta_node(void *priv, const u8 *addr, u16 auth_alg)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *params;

	if (!drv->add_sta_node_vendor_cmd_avail)
		return -EOPNOTSUPP;

	wpa_printf(MSG_DEBUG, "nl80211: Add STA node");

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ADD_STA_NODE) ||
	    !(params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    (addr &&
	     nla_put(msg, QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_MAC_ADDR, ETH_ALEN,
		     addr)) ||
	    nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_ADD_STA_NODE_AUTH_ALGO,
			auth_alg)) {
		nlmsg_free(msg);
		wpa_printf(MSG_ERROR,
			   "%s: err in adding vendor_cmd and vendor_data",
			   __func__);
		return -1;
	}
	nla_nest_end(msg, params);

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
}

#endif /* CONFIG_DRIVER_NL80211_QCA */


static int nl80211_set_mac_addr(void *priv, const u8 *addr)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int new_addr = addr != NULL;

	if (TEST_FAIL())
		return -1;

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
		wpa_printf(MSG_DEBUG, "  * Mesh ID (SSID)=%s",
			   wpa_ssid_txt(mesh_id, mesh_id_len));
		return nla_put(msg, NL80211_ATTR_MESH_ID, mesh_id_len, mesh_id);
	}

	return 0;
}


static int nl80211_put_mesh_config(struct nl_msg *msg,
				   struct wpa_driver_mesh_bss_params *params)
{
	struct nlattr *container;

	container = nla_nest_start(msg, NL80211_ATTR_MESH_CONFIG);
	if (!container)
		return -1;

	if (((params->flags & WPA_DRIVER_MESH_CONF_FLAG_AUTO_PLINKS) &&
	     nla_put_u8(msg, NL80211_MESHCONF_AUTO_OPEN_PLINKS,
			params->auto_plinks)) ||
	    ((params->flags & WPA_DRIVER_MESH_CONF_FLAG_FORWARDING) &&
	     nla_put_u8(msg, NL80211_MESHCONF_FORWARDING,
			params->forwarding)) ||
	    ((params->flags & WPA_DRIVER_MESH_CONF_FLAG_MAX_PEER_LINKS) &&
	     nla_put_u16(msg, NL80211_MESHCONF_MAX_PEER_LINKS,
			 params->max_peer_links)) ||
	    ((params->flags & WPA_DRIVER_MESH_CONF_FLAG_RSSI_THRESHOLD) &&
	     nla_put_u32(msg, NL80211_MESHCONF_RSSI_THRESHOLD,
			 params->rssi_threshold)))
		return -1;

	/*
	 * Set NL80211_MESHCONF_PLINK_TIMEOUT even if user mpm is used because
	 * the timer could disconnect stations even in that case.
	 */
	if ((params->flags & WPA_DRIVER_MESH_CONF_FLAG_PEER_LINK_TIMEOUT) &&
	    nla_put_u32(msg, NL80211_MESHCONF_PLINK_TIMEOUT,
			params->peer_link_timeout)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to set PLINK_TIMEOUT");
		return -1;
	}

	if ((params->flags & WPA_DRIVER_MESH_CONF_FLAG_HT_OP_MODE) &&
	    nla_put_u16(msg, NL80211_MESHCONF_HT_OPMODE, params->ht_opmode)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to set HT_OP_MODE");
		return -1;
	}

	nla_nest_end(msg, container);

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
	    nl80211_put_beacon_int(msg, params->beacon_int) ||
	    nl80211_put_dtim_period(msg, params->dtim_period))
		goto fail;

	wpa_printf(MSG_DEBUG, "  * flags=%08X", params->flags);

	if (params->handle_dfs && nla_put_flag(msg, NL80211_ATTR_HANDLE_DFS))
		goto fail;

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

	params->conf.flags |= WPA_DRIVER_MESH_CONF_FLAG_AUTO_PLINKS;
	params->conf.flags |= WPA_DRIVER_MESH_CONF_FLAG_PEER_LINK_TIMEOUT;
	params->conf.flags |= WPA_DRIVER_MESH_CONF_FLAG_MAX_PEER_LINKS;
	if (nl80211_put_mesh_config(msg, &params->conf) < 0)
		goto fail;

	ret = send_and_recv_msgs_connect_handle(drv, msg, bss, 1);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: mesh join failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto fail;
	}
	ret = 0;
	drv->assoc_freq = bss->freq = params->freq.freq;
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
	ret = send_and_recv_msgs_connect_handle(drv, msg, bss, 0);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: mesh leave failed: ret=%d (%s)",
			   ret, strerror(-ret));
	} else {
		wpa_printf(MSG_DEBUG,
			   "nl80211: mesh leave request send successfully");
	}

	if (drv->start_mode_sta &&
	    wpa_driver_nl80211_set_mode(drv->first_bss,
					NL80211_IFTYPE_STATION)) {
		wpa_printf(MSG_INFO,
			   "nl80211: Failed to set interface into station mode");
	}
	return ret;
}


static int nl80211_probe_mesh_link(void *priv, const u8 *addr, const u8 *eth,
				   size_t len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_PROBE_MESH_LINK);
	if (!msg ||
	    nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
	    nla_put(msg, NL80211_ATTR_FRAME, len, eth)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: mesh link probe to " MACSTR
			   " failed: ret=%d (%s)",
			   MAC2STR(addr), ret, strerror(-ret));
	} else {
		wpa_printf(MSG_DEBUG, "nl80211: Mesh link to " MACSTR
			   " probed successfully", MAC2STR(addr));
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
			   nl_geterror(res));
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
			   nl_geterror(res));
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
	default:
		return NULL;
	}
}


static int wpa_driver_br_set_net_param(void *priv, enum drv_br_net_param param,
				       unsigned int val)
{
	struct i802_bss *bss = priv;
	char path[128];
	const char *param_txt;
	int ip_version = 4;

	if (param == DRV_BR_MULTICAST_SNOOPING) {
		os_snprintf(path, sizeof(path),
			    "/sys/devices/virtual/net/%s/bridge/multicast_snooping",
			    bss->brname);
		goto set_val;
	}

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

set_val:
	if (linux_write_system_file(path, val))
		return -1;

	return 0;
}


#ifdef CONFIG_DRIVER_NL80211_QCA

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
	case HOSTAPD_MODE_IEEE80211ANY:
		return QCA_ACS_MODE_IEEE80211ANY;
	default:
		return -1;
	}
}


static int add_acs_ch_list(struct nl_msg *msg, const int *freq_list)
{
	int num_channels = 0, num_freqs;
	u8 *ch_list;
	enum hostapd_hw_mode hw_mode;
	int ret = 0;
	int i;

	if (!freq_list)
		return 0;

	num_freqs = int_array_len(freq_list);
	ch_list = os_malloc(sizeof(u8) * num_freqs);
	if (!ch_list)
		return -1;

	for (i = 0; i < num_freqs; i++) {
		const int freq = freq_list[i];

		if (freq == 0)
			break;
		/* Send 2.4 GHz and 5 GHz channels with
		 * QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST to maintain backwards
		 * compatibility.
		 */
		if (!(freq >= 2412 && freq <= 2484) &&
		    !(freq >= 5180 && freq <= 5900) &&
		    !(freq >= 5945 && freq <= 7115))
			continue;
		hw_mode = ieee80211_freq_to_chan(freq, &ch_list[num_channels]);
		if (hw_mode != NUM_HOSTAPD_MODES)
			num_channels++;
	}

	if (num_channels)
		ret = nla_put(msg, QCA_WLAN_VENDOR_ATTR_ACS_CH_LIST,
			      num_channels, ch_list);

	os_free(ch_list);
	return ret;
}


static int add_acs_freq_list(struct nl_msg *msg, const int *freq_list)
{
	int i, len, ret;
	u32 *freqs;

	if (!freq_list)
		return 0;
	len = int_array_len(freq_list);
	freqs = os_malloc(sizeof(u32) * len);
	if (!freqs)
		return -1;
	for (i = 0; i < len; i++)
		freqs[i] = freq_list[i];
	ret = nla_put(msg, QCA_WLAN_VENDOR_ATTR_ACS_FREQ_LIST,
		      sizeof(u32) * len, freqs);
	os_free(freqs);
	return ret;
}


static int nl80211_qca_do_acs(struct wpa_driver_nl80211_data *drv,
			      struct drv_acs_params *params)
{
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
	     nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_ACS_HT40_ENABLED)) ||
	    (params->vht_enabled &&
	     nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_ACS_VHT_ENABLED)) ||
	    nla_put_u16(msg, QCA_WLAN_VENDOR_ATTR_ACS_CHWIDTH,
			params->ch_width) ||
	    add_acs_ch_list(msg, params->freq_list) ||
	    add_acs_freq_list(msg, params->freq_list) ||
	    (params->edmg_enabled &&
	     nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_ACS_EDMG_ENABLED))) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}
	nla_nest_end(msg, data);

	wpa_printf(MSG_DEBUG,
		   "nl80211: ACS Params: HW_MODE: %d HT: %d HT40: %d VHT: %d BW: %d EDMG: %d",
		   params->hw_mode, params->ht_enabled, params->ht40_enabled,
		   params->vht_enabled, params->ch_width, params->edmg_enabled);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Failed to invoke driver ACS function: %s",
			   strerror(-ret));
	}
	return ret;
}


static int nl80211_set_band(void *priv, u32 band_mask)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *data;
	int ret;
	enum qca_set_band qca_band_value;
	u32 qca_band_mask = QCA_SETBAND_AUTO;

	if (!drv->setband_vendor_cmd_avail ||
	    (band_mask > (WPA_SETBAND_2G | WPA_SETBAND_5G | WPA_SETBAND_6G)))
		return -1;

	if (band_mask & WPA_SETBAND_5G)
		qca_band_mask |= QCA_SETBAND_5G;
	if (band_mask & WPA_SETBAND_2G)
		qca_band_mask |= QCA_SETBAND_2G;
	if (band_mask & WPA_SETBAND_6G)
		qca_band_mask |= QCA_SETBAND_6G;

	/*
	 * QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE is a legacy interface hence make
	 * it suite to its values (AUTO/5G/2G) for backwards compatibility.
	 */
	qca_band_value = ((qca_band_mask & QCA_SETBAND_5G) &&
			  (qca_band_mask & QCA_SETBAND_2G)) ?
				QCA_SETBAND_AUTO :
				qca_band_mask & ~QCA_SETBAND_6G;

	wpa_printf(MSG_DEBUG,
		   "nl80211: QCA_BAND_MASK = 0x%x, QCA_BAND_VALUE = %d",
		   qca_band_mask, qca_band_value);

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SETBAND) ||
	    !(data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE,
			qca_band_value) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_SETBAND_MASK,
			qca_band_mask)) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}
	nla_nest_end(msg, data);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Driver setband function failed: %s",
			   strerror(-ret));
	}
	return ret;
}


struct nl80211_pcl {
	unsigned int num;
	unsigned int *freq_list;
};

static int preferred_freq_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nl80211_pcl *param = arg;
	struct nlattr *nl_vend, *attr;
	enum qca_iface_type iface_type;
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	unsigned int num, max_num;
	u32 *freqs;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	nl_vend = tb[NL80211_ATTR_VENDOR_DATA];
	if (!nl_vend)
		return NL_SKIP;

	nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_MAX,
		  nla_data(nl_vend), nla_len(nl_vend), NULL);

	attr = tb_vendor[
		QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_IFACE_TYPE];
	if (!attr) {
		wpa_printf(MSG_ERROR, "nl80211: iface_type couldn't be found");
		param->num = 0;
		return NL_SKIP;
	}

	iface_type = (enum qca_iface_type) nla_get_u32(attr);
	wpa_printf(MSG_DEBUG, "nl80211: Driver returned iface_type=%d",
		   iface_type);

	attr = tb_vendor[QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST];
	if (!attr) {
		wpa_printf(MSG_ERROR,
			   "nl80211: preferred_freq_list couldn't be found");
		param->num = 0;
		return NL_SKIP;
	}

	/*
	 * param->num has the maximum number of entries for which there
	 * is room in the freq_list provided by the caller.
	 */
	freqs = nla_data(attr);
	max_num = nla_len(attr) / sizeof(u32);
	if (max_num > param->num)
		max_num = param->num;
	for (num = 0; num < max_num; num++)
		param->freq_list[num] = freqs[num];
	param->num = num;

	return NL_SKIP;
}


static int nl80211_get_pref_freq_list(void *priv,
				      enum wpa_driver_if_type if_type,
				      unsigned int *num,
				      unsigned int *freq_list)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	unsigned int i;
	struct nlattr *params;
	struct nl80211_pcl param;
	enum qca_iface_type iface_type;

	if (!drv->get_pref_freq_list)
		return -1;

	switch (if_type) {
	case WPA_IF_STATION:
		iface_type = QCA_IFACE_TYPE_STA;
		break;
	case WPA_IF_AP_BSS:
		iface_type = QCA_IFACE_TYPE_AP;
		break;
	case WPA_IF_P2P_GO:
		iface_type = QCA_IFACE_TYPE_P2P_GO;
		break;
	case WPA_IF_P2P_CLIENT:
		iface_type = QCA_IFACE_TYPE_P2P_CLIENT;
		break;
	case WPA_IF_IBSS:
		iface_type = QCA_IFACE_TYPE_IBSS;
		break;
	case WPA_IF_TDLS:
		iface_type = QCA_IFACE_TYPE_TDLS;
		break;
	default:
		return -1;
	}

	param.num = *num;
	param.freq_list = freq_list;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_IFINDEX, drv->ifindex) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_GET_PREFERRED_FREQ_LIST) ||
	    !(params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u32(msg,
			QCA_WLAN_VENDOR_ATTR_GET_PREFERRED_FREQ_LIST_IFACE_TYPE,
			iface_type)) {
		wpa_printf(MSG_ERROR,
			   "%s: err in adding vendor_cmd and vendor_data",
			   __func__);
		nlmsg_free(msg);
		return -1;
	}
	nla_nest_end(msg, params);

	os_memset(freq_list, 0, *num * sizeof(freq_list[0]));
	ret = send_and_recv_msgs(drv, msg, preferred_freq_info_handler, &param,
				 NULL, NULL);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "%s: err in send_and_recv_msgs", __func__);
		return ret;
	}

	*num = param.num;

	for (i = 0; i < *num; i++) {
		wpa_printf(MSG_DEBUG, "nl80211: preferred_channel_list[%d]=%d",
			   i, freq_list[i]);
	}

	return 0;
}


static int nl80211_set_prob_oper_freq(void *priv, unsigned int freq)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	struct nlattr *params;

	if (!drv->set_prob_oper_freq)
		return -1;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Set P2P probable operating freq %u for ifindex %d",
		   freq, bss->ifindex);

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_PROBABLE_OPER_CHANNEL) ||
	    !(params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u32(msg,
			QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_IFACE_TYPE,
			QCA_IFACE_TYPE_P2P_CLIENT) ||
	    nla_put_u32(msg,
			QCA_WLAN_VENDOR_ATTR_PROBABLE_OPER_CHANNEL_FREQ,
			freq)) {
		wpa_printf(MSG_ERROR,
			   "%s: err in adding vendor_cmd and vendor_data",
			   __func__);
		nlmsg_free(msg);
		return -1;
	}
	nla_nest_end(msg, params);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_ERROR, "%s: err in send_and_recv_msgs",
			   __func__);
		return ret;
	}
	nlmsg_free(msg);
	return 0;
}


static int nl80211_p2p_lo_start(void *priv, unsigned int freq,
				unsigned int period, unsigned int interval,
				unsigned int count, const u8 *device_types,
				size_t dev_types_len,
				const u8 *ies, size_t ies_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *container;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Start P2P Listen offload: freq=%u, period=%u, interval=%u, count=%u",
		   freq, period, interval, count);

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_P2P_LISTEN_OFFLOAD))
		return -1;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_START))
		goto fail;

	container = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!container)
		goto fail;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CHANNEL,
			freq) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_PERIOD,
			period) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_INTERVAL,
			interval) ||
	    nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_COUNT,
			count) ||
	    nla_put(msg, QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_DEVICE_TYPES,
		    dev_types_len, device_types) ||
	    nla_put(msg, QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_VENDOR_IE,
		    ies_len, ies))
		goto fail;

	nla_nest_end(msg, container);
	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: Failed to send P2P Listen offload vendor command");
		goto fail;
	}

	return 0;

fail:
	nlmsg_free(msg);
	return -1;
}


static int nl80211_p2p_lo_stop(void *priv)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;

	wpa_printf(MSG_DEBUG, "nl80211: Stop P2P Listen offload");

	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_P2P_LISTEN_OFFLOAD))
		return -1;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_STOP)) {
		nlmsg_free(msg);
		return -1;
	}

	return send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
}


static int nl80211_set_tdls_mode(void *priv, int tdls_external_control)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *params;
	int ret;
	u32 tdls_mode;

	wpa_printf(MSG_DEBUG,
		   "nl80211: Set TDKS mode: tdls_external_control=%d",
		   tdls_external_control);

	if (tdls_external_control == 1)
		tdls_mode = QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT |
			QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL;
	else
		tdls_mode = QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_CONFIGURE_TDLS))
		goto fail;

	params = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!params)
		goto fail;

	if (nla_put_u32(msg, QCA_WLAN_VENDOR_ATTR_TDLS_CONFIG_TRIGGER_MODE,
			tdls_mode))
		goto fail;

	nla_nest_end(msg, params);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Set TDLS mode failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto fail;
	}
	return 0;
fail:
	nlmsg_free(msg);
	return -1;
}


#ifdef CONFIG_MBO

static enum mbo_transition_reject_reason
nl80211_mbo_reject_reason_mapping(enum qca_wlan_btm_candidate_status status)
{
	switch (status) {
	case QCA_STATUS_REJECT_EXCESSIVE_FRAME_LOSS_EXPECTED:
		return MBO_TRANSITION_REJECT_REASON_FRAME_LOSS;
	case QCA_STATUS_REJECT_EXCESSIVE_DELAY_EXPECTED:
		return MBO_TRANSITION_REJECT_REASON_DELAY;
	case QCA_STATUS_REJECT_INSUFFICIENT_QOS_CAPACITY:
		return MBO_TRANSITION_REJECT_REASON_QOS_CAPACITY;
	case QCA_STATUS_REJECT_LOW_RSSI:
		return MBO_TRANSITION_REJECT_REASON_RSSI;
	case QCA_STATUS_REJECT_HIGH_INTERFERENCE:
		return MBO_TRANSITION_REJECT_REASON_INTERFERENCE;
	case QCA_STATUS_REJECT_UNKNOWN:
	default:
		return MBO_TRANSITION_REJECT_REASON_UNSPECIFIED;
	}
}


static void nl80211_parse_btm_candidate_info(struct candidate_list *candidate,
					     struct nlattr *tb[], int num)
{
	enum qca_wlan_btm_candidate_status status;
	char buf[50];

	os_memcpy(candidate->bssid,
		  nla_data(tb[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID]),
		  ETH_ALEN);

	status = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_STATUS]);
	candidate->is_accept = status == QCA_STATUS_ACCEPT;
	candidate->reject_reason = nl80211_mbo_reject_reason_mapping(status);

	if (candidate->is_accept)
		os_snprintf(buf, sizeof(buf), "Accepted");
	else
		os_snprintf(buf, sizeof(buf),
			    "Rejected, Reject_reason: %d",
			    candidate->reject_reason);
	wpa_printf(MSG_DEBUG, "nl80211:   BSSID[%d]: " MACSTR " %s",
		   num, MAC2STR(candidate->bssid), buf);
}


static int
nl80211_get_bss_transition_status_handler(struct nl_msg *msg, void *arg)
{
	struct wpa_bss_candidate_info *info = arg;
	struct candidate_list *candidate = info->candidates;
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX + 1];
	static struct nla_policy policy[
		QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX + 1] = {
		[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID] = {
			.minlen = ETH_ALEN
		},
		[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_STATUS] = {
			.type = NLA_U32,
		},
	};
	struct nlattr *attr;
	int rem;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	u8 num;

	num = info->num; /* number of candidates sent to driver */
	info->num = 0;
	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb_msg[NL80211_ATTR_VENDOR_DATA] ||
	    nla_parse_nested(tb_vendor, QCA_WLAN_VENDOR_ATTR_MAX,
			     tb_msg[NL80211_ATTR_VENDOR_DATA], NULL) ||
	    !tb_vendor[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO])
		return NL_SKIP;

	wpa_printf(MSG_DEBUG,
		   "nl80211: WNM Candidate list received from driver");
	nla_for_each_nested(attr,
			    tb_vendor[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO],
			    rem) {
		if (info->num >= num ||
		    nla_parse_nested(
			    tb, QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX,
			    attr, policy) ||
		    !tb[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID] ||
		    !tb[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_STATUS])
			break;

		nl80211_parse_btm_candidate_info(candidate, tb, info->num);

		candidate++;
		info->num++;
	}

	return NL_SKIP;
}


static struct wpa_bss_candidate_info *
nl80211_get_bss_transition_status(void *priv, struct wpa_bss_trans_info *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *attr, *attr1, *attr2;
	struct wpa_bss_candidate_info *info;
	u8 i;
	int ret;
	u8 *pos;

	if (!drv->fetch_bss_trans_status)
		return NULL;

	info = os_zalloc(sizeof(*info));
	if (!info)
		return NULL;
	/* Allocate memory for number of candidates sent to driver */
	info->candidates = os_calloc(params->n_candidates,
				     sizeof(*info->candidates));
	if (!info->candidates) {
		os_free(info);
		return NULL;
	}

	/* Copy the number of candidates being sent to driver. This is used in
	 * nl80211_get_bss_transition_status_handler() to limit the number of
	 * candidates that can be populated in info->candidates and will be
	 * later overwritten with the actual number of candidates received from
	 * the driver.
	 */
	info->num = params->n_candidates;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_FETCH_BSS_TRANSITION_STATUS))
		goto fail;

	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_BTM_MBO_TRANSITION_REASON,
		       params->mbo_transition_reason))
		goto fail;

	attr1 = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO);
	if (!attr1)
		goto fail;

	wpa_printf(MSG_DEBUG,
		   "nl80211: WNM Candidate list info sending to driver: mbo_transition_reason: %d n_candidates: %d",
		   params->mbo_transition_reason, params->n_candidates);
	pos = params->bssid;
	for (i = 0; i < params->n_candidates; i++) {
		wpa_printf(MSG_DEBUG, "nl80211:   BSSID[%d]: " MACSTR, i,
			   MAC2STR(pos));
		attr2 = nla_nest_start(msg, i);
		if (!attr2 ||
		    nla_put(msg, QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID,
			    ETH_ALEN, pos))
			goto fail;
		pos += ETH_ALEN;
		nla_nest_end(msg, attr2);
	}

	nla_nest_end(msg, attr1);
	nla_nest_end(msg, attr);

	ret = send_and_recv_msgs(drv, msg,
				 nl80211_get_bss_transition_status_handler,
				 info, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: WNM Get BSS transition status failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto fail;
	}
	return info;

fail:
	nlmsg_free(msg);
	os_free(info->candidates);
	os_free(info);
	return NULL;
}


/**
 * nl80211_ignore_assoc_disallow - Configure driver to ignore assoc_disallow
 * @priv: Pointer to private driver data from wpa_driver_nl80211_init()
 * @ignore_assoc_disallow: 0 to not ignore, 1 to ignore
 * Returns: 0 on success, -1 on failure
 */
static int nl80211_ignore_assoc_disallow(void *priv, int ignore_disallow)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *attr;
	int ret = -1;

	if (!drv->set_wifi_conf_vendor_cmd_avail)
		return -1;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION))
		goto fail;

	attr = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!attr)
		goto fail;

	wpa_printf(MSG_DEBUG, "nl80211: Set ignore_assoc_disallow %d",
		   ignore_disallow);
	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_CONFIG_IGNORE_ASSOC_DISALLOWED,
		       ignore_disallow))
		goto fail;

	nla_nest_end(msg, attr);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Set ignore_assoc_disallow failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto fail;
	}

fail:
	nlmsg_free(msg);
	return ret;
}

#endif /* CONFIG_MBO */

#endif /* CONFIG_DRIVER_NL80211_QCA */


#ifdef CONFIG_DRIVER_NL80211_BRCM
static int wpa_driver_do_broadcom_acs(struct wpa_driver_nl80211_data *drv,
				      struct drv_acs_params *params)
{
	struct nl_msg *msg;
	struct nlattr *data;
	int freq_list_len;
	int ret = -1;

	freq_list_len = int_array_len(params->freq_list);
	wpa_printf(MSG_DEBUG, "%s: freq_list_len=%d",
		   __func__, freq_list_len);

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR);
	if (!msg ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_BRCM) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			BRCM_VENDOR_SCMD_ACS) ||
	    !(data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA)) ||
	    nla_put_u8(msg, BRCM_VENDOR_ATTR_ACS_HW_MODE, params->hw_mode) ||
	    nla_put_u8(msg, BRCM_VENDOR_ATTR_ACS_HT_ENABLED,
		       params->ht_enabled) ||
	    nla_put_u8(msg, BRCM_VENDOR_ATTR_ACS_HT40_ENABLED,
		       params->ht40_enabled) ||
	    nla_put_u8(msg, BRCM_VENDOR_ATTR_ACS_VHT_ENABLED,
		       params->vht_enabled) ||
	    nla_put_u16(msg, BRCM_VENDOR_ATTR_ACS_CHWIDTH, params->ch_width) ||
	    (freq_list_len > 0 &&
	     nla_put(msg, BRCM_VENDOR_ATTR_ACS_FREQ_LIST,
		     sizeof(int) * freq_list_len, params->freq_list)))
		goto fail;
	nla_nest_end(msg, data);

	wpa_printf(MSG_DEBUG,
		   "nl80211: ACS Params: HW_MODE: %d HT: %d HT40: %d VHT: %d BW: %d",
		   params->hw_mode, params->ht_enabled, params->ht40_enabled,
		   params->vht_enabled, params->ch_width);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: BRCM Failed to invoke driver ACS function: %s",
			   strerror(errno));
	}

	msg = NULL;
fail:
	nlmsg_free(msg);
	return ret;
}
#endif /* CONFIG_DRIVER_NL80211_BRCM */


static int nl80211_do_acs(void *priv, struct drv_acs_params *params)
{
#if defined(CONFIG_DRIVER_NL80211_QCA) || defined(CONFIG_DRIVER_NL80211_BRCM)
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
#endif /* CONFIG_DRIVER_NL80211_QCA || CONFIG_DRIVER_NL80211_BRCM */

#ifdef CONFIG_DRIVER_NL80211_QCA
	if (drv->qca_do_acs)
		return nl80211_qca_do_acs(drv, params);
#endif /* CONFIG_DRIVER_NL80211_QCA */

#ifdef CONFIG_DRIVER_NL80211_BRCM
	if (drv->brcm_do_acs)
		return wpa_driver_do_broadcom_acs(drv, params);
#endif /* CONFIG_DRIVER_NL80211_BRCM */

	return -1;
}


static int nl80211_write_to_file(const char *name, unsigned int val)
{
	int fd, len;
	char tmp[128];
	int ret = 0;

	fd = open(name, O_RDWR);
	if (fd < 0) {
		int level;
		/*
		 * Flags may not exist on older kernels, or while we're tearing
		 * down a disappearing device.
		 */
		if (errno == ENOENT) {
			ret = 0;
			level = MSG_DEBUG;
		} else {
			ret = -1;
			level = MSG_ERROR;
		}
		wpa_printf(level, "nl80211: Failed to open %s: %s",
			   name, strerror(errno));
		return ret;
	}

	len = os_snprintf(tmp, sizeof(tmp), "%u\n", val);
	len = write(fd, tmp, len);
	if (len < 0) {
		ret = -1;
		wpa_printf(MSG_ERROR, "nl80211: Failed to write to %s: %s",
			   name, strerror(errno));
	}
	close(fd);

	return ret;
}


static int nl80211_configure_data_frame_filters(void *priv, u32 filter_flags)
{
	struct i802_bss *bss = priv;
	char path[128];
	int ret;

	/* P2P-Device has no netdev that can (or should) be configured here */
	if (nl80211_get_ifmode(bss) == NL80211_IFTYPE_P2P_DEVICE)
		return 0;

	wpa_printf(MSG_DEBUG, "nl80211: Data frame filter flags=0x%x",
		   filter_flags);

	/* Configure filtering of unicast frame encrypted using GTK */
	ret = os_snprintf(path, sizeof(path),
			  "/proc/sys/net/ipv4/conf/%s/drop_unicast_in_l2_multicast",
			  bss->ifname);
	if (os_snprintf_error(sizeof(path), ret))
		return -1;

	ret = nl80211_write_to_file(path,
				    !!(filter_flags &
				       WPA_DATA_FRAME_FILTER_FLAG_GTK));
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to set IPv4 unicast in multicast filter");
		return ret;
	}

	os_snprintf(path, sizeof(path),
		    "/proc/sys/net/ipv6/conf/%s/drop_unicast_in_l2_multicast",
		    bss->ifname);
	ret = nl80211_write_to_file(path,
				    !!(filter_flags &
				       WPA_DATA_FRAME_FILTER_FLAG_GTK));

	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to set IPv6 unicast in multicast filter");
		return ret;
	}

	/* Configure filtering of unicast frame encrypted using GTK */
	os_snprintf(path, sizeof(path),
		    "/proc/sys/net/ipv4/conf/%s/drop_gratuitous_arp",
		    bss->ifname);
	ret = nl80211_write_to_file(path,
				    !!(filter_flags &
				       WPA_DATA_FRAME_FILTER_FLAG_ARP));
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed set gratuitous ARP filter");
		return ret;
	}

	/* Configure filtering of IPv6 NA frames */
	os_snprintf(path, sizeof(path),
		    "/proc/sys/net/ipv6/conf/%s/drop_unsolicited_na",
		    bss->ifname);
	ret = nl80211_write_to_file(path,
				    !!(filter_flags &
				       WPA_DATA_FRAME_FILTER_FLAG_NA));
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "nl80211: Failed to set unsolicited NA filter");
		return ret;
	}

	return 0;
}


static int nl80211_get_ext_capab(void *priv, enum wpa_driver_if_type type,
				 const u8 **ext_capa, const u8 **ext_capa_mask,
				 unsigned int *ext_capa_len)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	enum nl80211_iftype nlmode;
	unsigned int i;

	if (!ext_capa || !ext_capa_mask || !ext_capa_len)
		return -1;

	nlmode = wpa_driver_nl80211_if_type(type);

	/* By default, use the per-radio values */
	*ext_capa = drv->extended_capa;
	*ext_capa_mask = drv->extended_capa_mask;
	*ext_capa_len = drv->extended_capa_len;

	/* Replace the default value if a per-interface type value exists */
	for (i = 0; i < drv->num_iface_ext_capa; i++) {
		if (nlmode == drv->iface_ext_capa[i].iftype) {
			*ext_capa = drv->iface_ext_capa[i].ext_capa;
			*ext_capa_mask = drv->iface_ext_capa[i].ext_capa_mask;
			*ext_capa_len = drv->iface_ext_capa[i].ext_capa_len;
			break;
		}
	}

	return 0;
}


static int nl80211_update_connection_params(
	void *priv, struct wpa_driver_associate_params *params,
	enum wpa_drv_update_connect_params_mask mask)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -1;
	enum nl80211_auth_type type;

	/* Update Connection Params is intended for drivers that implement
	 * internal SME and expect these updated connection params from
	 * wpa_supplicant. Do not send this request for the drivers using
	 * SME from wpa_supplicant.
	 */
	if (drv->capa.flags & WPA_DRIVER_FLAGS_SME)
		return 0;

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_UPDATE_CONNECT_PARAMS);
	if (!msg)
		goto fail;

	wpa_printf(MSG_DEBUG, "nl80211: Update connection params (ifindex=%d)",
		   drv->ifindex);

	if ((mask & WPA_DRV_UPDATE_ASSOC_IES) && params->wpa_ie) {
		if (nla_put(msg, NL80211_ATTR_IE, params->wpa_ie_len,
			    params->wpa_ie))
			goto fail;
		wpa_hexdump(MSG_DEBUG, "  * IEs", params->wpa_ie,
			    params->wpa_ie_len);
	}

	if (mask & WPA_DRV_UPDATE_AUTH_TYPE) {
		type = get_nl_auth_type(params->auth_alg);
		if (type == NL80211_AUTHTYPE_MAX ||
		    nla_put_u32(msg, NL80211_ATTR_AUTH_TYPE, type))
			goto fail;
		wpa_printf(MSG_DEBUG, "  * Auth Type %d", type);
	}

	if ((mask & WPA_DRV_UPDATE_FILS_ERP_INFO) &&
	    nl80211_put_fils_connect_params(drv, params, msg))
		goto fail;

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret)
		wpa_dbg(drv->ctx, MSG_DEBUG,
			"nl80211: Update connect params command failed: ret=%d (%s)",
			ret, strerror(-ret));

fail:
	nlmsg_free(msg);
	return ret;
}


static int nl80211_send_external_auth_status(void *priv,
					     struct external_auth *params)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg = NULL;
	int ret = -1;

	/* External auth command/status is intended for drivers that implement
	 * internal SME but want to offload authentication processing (e.g.,
	 * SAE) to hostapd/wpa_supplicant. Do not send the status to drivers
	 * which do not support AP SME or use wpa_supplicant/hostapd SME.
	 */
	if ((is_ap_interface(drv->nlmode) && !bss->drv->device_ap_sme) ||
	    (drv->capa.flags & WPA_DRIVER_FLAGS_SME))
		return -1;

	wpa_dbg(drv->ctx, MSG_DEBUG,
		"nl80211: External auth status: %u", params->status);

	msg = nl80211_drv_msg(drv, 0, NL80211_CMD_EXTERNAL_AUTH);
	if (!msg ||
	    nla_put_u16(msg, NL80211_ATTR_STATUS_CODE, params->status) ||
	    (params->ssid && params->ssid_len &&
	     nla_put(msg, NL80211_ATTR_SSID, params->ssid_len, params->ssid)) ||
	    (params->pmkid &&
	     nla_put(msg, NL80211_ATTR_PMKID, PMKID_LEN, params->pmkid)) ||
	    (params->bssid &&
	     nla_put(msg, NL80211_ATTR_BSSID, ETH_ALEN, params->bssid)))
		goto fail;
	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: External Auth status update failed: ret=%d (%s)",
			   ret, strerror(-ret));
		goto fail;
	}
fail:
	nlmsg_free(msg);
	return ret;
}


static int nl80211_set_4addr_mode(void *priv, const char *bridge_ifname,
				  int val)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret = -ENOBUFS;

	wpa_printf(MSG_DEBUG, "nl80211: %s 4addr mode (bridge_ifname: %s)",
		   val ? "Enable" : "Disable", bridge_ifname);

	msg = nl80211_cmd_msg(drv->first_bss, 0, NL80211_CMD_SET_INTERFACE);
	if (!msg || nla_put_u8(msg, NL80211_ATTR_4ADDR, val))
		goto fail;

	if (bridge_ifname[0] && bss->added_if_into_bridge && !val) {
		if (linux_br_del_if(drv->global->ioctl_sock,
				    bridge_ifname, bss->ifname)) {
			wpa_printf(MSG_ERROR,
				   "nl80211: Failed to remove interface %s from bridge %s",
				   bss->ifname, bridge_ifname);
			return -1;
		}
		bss->added_if_into_bridge = 0;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL, NULL, NULL);
	msg = NULL;
	if (ret && val && nl80211_get_4addr(bss) == 1) {
		wpa_printf(MSG_DEBUG,
			   "nl80211: 4addr mode was already enabled");
		ret = 0;
	}
	if (!ret) {
		if (bridge_ifname[0] && val &&
		    i802_check_bridge(drv, bss, bridge_ifname, bss->ifname) < 0)
			return -1;
		return 0;
	}

fail:
	nlmsg_free(msg);
	wpa_printf(MSG_ERROR, "nl80211: Failed to enable/disable 4addr");

	return ret;
}


#ifdef CONFIG_DPP
static int nl80211_dpp_listen(void *priv, bool enable)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_ACTION << 4);
	struct nl_sock *handle;

	if (!drv->multicast_registrations || !bss->nl_mgmt)
		return 0; /* cannot do more than hope broadcast RX works */

	wpa_printf(MSG_DEBUG,
		   "nl80211: Update DPP Public Action frame registration (%s multicast RX)",
		   enable ? "enable" : "disable");
	handle = (void *) (((intptr_t) bss->nl_mgmt) ^ ELOOP_SOCKET_INVALID);
	return nl80211_register_frame(bss, handle, type,
				      (u8 *) "\x04\x09\x50\x6f\x9a\x1a", 6,
				      enable);
}
#endif /* CONFIG_DPP */


#ifdef CONFIG_TESTING_OPTIONS
static int testing_nl80211_register_frame(void *priv, u16 type,
					  const u8 *match, size_t match_len,
					  bool multicast)
{
	struct i802_bss *bss = priv;
	struct nl_sock *handle;

	if (!bss->nl_mgmt)
		return -1;
	handle = (void *) (((intptr_t) bss->nl_mgmt) ^ ELOOP_SOCKET_INVALID);
	return nl80211_register_frame(bss, handle, type, match, match_len,
				      multicast);
}
#endif /* CONFIG_TESTING_OPTIONS */


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
	.abort_scan = wpa_driver_nl80211_abort_scan,
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
	.tx_control_port = nl80211_tx_control_port,
	.hapd_send_eapol = wpa_driver_nl80211_hapd_send_eapol,
	.sta_set_flags = wpa_driver_nl80211_sta_set_flags,
	.sta_set_airtime_weight = driver_nl80211_sta_set_airtime_weight,
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
	.channel_info = nl80211_channel_info,
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
	.update_dh_ie = nl80211_update_dh_ie,
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
#ifndef ANDROID_LIB_STUB
	.driver_cmd = wpa_driver_nl80211_driver_cmd,
#endif /* !ANDROID_LIB_STUB */
#endif /* ANDROID */
	.vendor_cmd = nl80211_vendor_cmd,
	.set_qos_map = nl80211_set_qos_map,
	.get_wowlan = nl80211_get_wowlan,
	.set_wowlan = nl80211_set_wowlan,
	.set_mac_addr = nl80211_set_mac_addr,
#ifdef CONFIG_MESH
	.init_mesh = wpa_driver_nl80211_init_mesh,
	.join_mesh = wpa_driver_nl80211_join_mesh,
	.leave_mesh = wpa_driver_nl80211_leave_mesh,
	.probe_mesh_link = nl80211_probe_mesh_link,
#endif /* CONFIG_MESH */
	.br_add_ip_neigh = wpa_driver_br_add_ip_neigh,
	.br_delete_ip_neigh = wpa_driver_br_delete_ip_neigh,
	.br_port_set_attr = wpa_driver_br_port_set_attr,
	.br_set_net_param = wpa_driver_br_set_net_param,
	.add_tx_ts = nl80211_add_ts,
	.del_tx_ts = nl80211_del_ts,
	.get_ifindex = nl80211_get_ifindex,
#ifdef CONFIG_DRIVER_NL80211_QCA
	.roaming = nl80211_roaming,
	.disable_fils = nl80211_disable_fils,
	.set_band = nl80211_set_band,
	.get_pref_freq_list = nl80211_get_pref_freq_list,
	.set_prob_oper_freq = nl80211_set_prob_oper_freq,
	.p2p_lo_start = nl80211_p2p_lo_start,
	.p2p_lo_stop = nl80211_p2p_lo_stop,
	.set_default_scan_ies = nl80211_set_default_scan_ies,
	.set_tdls_mode = nl80211_set_tdls_mode,
#ifdef CONFIG_MBO
	.get_bss_transition_status = nl80211_get_bss_transition_status,
	.ignore_assoc_disallow = nl80211_ignore_assoc_disallow,
#endif /* CONFIG_MBO */
	.set_bssid_tmp_disallow = nl80211_set_bssid_tmp_disallow,
	.add_sta_node = nl80211_add_sta_node,
#endif /* CONFIG_DRIVER_NL80211_QCA */
	.do_acs = nl80211_do_acs,
	.configure_data_frame_filters = nl80211_configure_data_frame_filters,
	.get_ext_capab = nl80211_get_ext_capab,
	.update_connect_params = nl80211_update_connection_params,
	.send_external_auth_status = nl80211_send_external_auth_status,
	.set_4addr_mode = nl80211_set_4addr_mode,
#ifdef CONFIG_DPP
	.dpp_listen = nl80211_dpp_listen,
#endif /* CONFIG_DPP */
#ifdef CONFIG_TESTING_OPTIONS
	.register_frame = testing_nl80211_register_frame,
#endif /* CONFIG_TESTING_OPTIONS */
};
