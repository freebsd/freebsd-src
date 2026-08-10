/*
 * Driver interaction with Linux nl80211/cfg80211 - AP monitor interface
 * Copyright (c) 2002-2014, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2003-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <netpacket/packet.h>
#include <linux/filter.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "linux_ioctl.h"
#include "radiotap_iter.h"
#include "driver_nl80211.h"


#ifdef CONFIG_TESTING_OPTIONS

static struct sock_filter msock_filter_insns[] = {
	/* return 0 == "DROP", we don't want RX */
	BPF_STMT(BPF_RET | BPF_K, 0),
};

static const struct sock_fprog msock_filter = {
	.len = ARRAY_SIZE(msock_filter_insns),
	.filter = msock_filter_insns,
};


static int add_monitor_filter(int s)
{
	if (setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER,
		       &msock_filter, sizeof(msock_filter))) {
		wpa_printf(MSG_ERROR, "nl80211: setsockopt(SO_ATTACH_FILTER) failed: %s",
			   strerror(errno));
		return -1;
	}

	return 0;
}


static void
nl80211_remove_monitor_interface(struct wpa_driver_nl80211_data *drv,
				 int ifidx, int sock)
{
	wpa_printf(MSG_DEBUG, "nl80211: Remove monitor interface");

	if (ifidx >= 0)
		nl80211_remove_iface(drv, ifidx);
	if (sock >= 0)
		close(sock);
}


static int nl80211_create_monitor_interface(struct wpa_driver_nl80211_data *drv,
					    int *ifidx, int *sock)
{
	char buf[IFNAMSIZ];
	struct sockaddr_ll ll;
	int optval;
	socklen_t optlen;

	*ifidx = -1;
	*sock = -1;

	if (os_strncmp(drv->first_bss->ifname, "p2p-", 4) == 0) {
		/*
		 * P2P interface name is of the format p2p-%s-%d. For monitor
		 * interface name corresponding to P2P GO, replace "p2p-" with
		 * "mon-" to retain the same interface name length and to
		 * indicate that it is a monitor interface.
		 */
		snprintf(buf, IFNAMSIZ, "mon-%s", drv->first_bss->ifname + 4);
	} else {
		int ret;

		/* Non-P2P interface with AP functionality. */
		ret = os_snprintf(buf, IFNAMSIZ, "mon.%s",
				  drv->first_bss->ifname);
		if (ret >= (int) sizeof(buf))
			wpa_printf(MSG_DEBUG,
				   "nl80211: Monitor interface name has been truncated to %s",
				   buf);
		else if (ret < 0)
			return ret;
	}

	buf[IFNAMSIZ - 1] = '\0';

	*ifidx = nl80211_create_iface(drv, buf, NL80211_IFTYPE_MONITOR, NULL,
				      0, NULL, NULL, 0);

	if (*ifidx < 0)
		return -1;

	if (linux_set_iface_flags(drv->global->ioctl_sock, buf, 1))
		goto error;

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = *ifidx;
	*sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (*sock < 0) {
		wpa_printf(MSG_ERROR, "nl80211: socket[PF_PACKET,SOCK_RAW] failed: %s",
			   strerror(errno));
		goto error;
	}

	if (add_monitor_filter(*sock)) {
		wpa_printf(MSG_INFO, "Failed to set socket filter for monitor "
			   "interface; do filtering in user space");
		/* This works, but will cost in performance. */
	}

	if (bind(*sock, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: monitor socket bind failed: %s",
			   strerror(errno));
		goto error;
	}

	optlen = sizeof(optval);
	optval = 20;
	if (setsockopt(*sock, SOL_SOCKET, SO_PRIORITY, &optval, optlen)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to set socket priority: %s",
			   strerror(errno));
		goto error;
	}

	return 0;
 error:
	nl80211_remove_monitor_interface(drv, *ifidx, *sock);
	return -1;
}


static int _nl80211_send_monitor(int monitor_sock,
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

	if (noack)
		txflags |= IEEE80211_RADIOTAP_F_TX_NOACK;
	WPA_PUT_LE16(&rtap_hdr[12], txflags);

	res = sendmsg(monitor_sock, &msg, 0);
	if (res < 0) {
		wpa_printf(MSG_INFO, "nl80211: sendmsg: %s", strerror(errno));
		return -1;
	}
	return 0;
}


int nl80211_send_monitor(struct wpa_driver_nl80211_data *drv,
			 const void *data, size_t len,
			 int encrypt, int noack)
{
	int res, ifidx, sock;

	res = nl80211_create_monitor_interface(drv, &ifidx, &sock);
	if (res < 0)
		return res;

	res = _nl80211_send_monitor(sock, data, len, encrypt, noack);
	nl80211_remove_monitor_interface(drv, ifidx, sock);
	return res;
}

#endif /* CONFIG_TESTING_OPTIONS */
