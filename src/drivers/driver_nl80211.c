/*
 * WPA Supplicant - driver interaction with Linux nl80211/cfg80211
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

#include "includes.h"
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include "nl80211_copy.h"
#ifdef CONFIG_CLIENT_MLME
#include <netpacket/packet.h>
#include <linux/if_ether.h>
#include "radiotap.h"
#include "radiotap_iter.h"
#endif /* CONFIG_CLIENT_MLME */

#include "wireless_copy.h"
#include "common.h"
#include "driver.h"
#include "eloop.h"
#include "ieee802_11_defs.h"

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


struct wpa_driver_nl80211_data {
	void *ctx;
	int wext_event_sock;
	int ioctl_sock;
	char ifname[IFNAMSIZ + 1];
	int ifindex;
	int if_removed;
	u8 *assoc_req_ies;
	size_t assoc_req_ies_len;
	u8 *assoc_resp_ies;
	size_t assoc_resp_ies_len;
	struct wpa_driver_capa capa;
	int has_capability;
	int we_version_compiled;

	/* for set_auth_alg fallback */
	int use_crypt;
	int auth_alg_fallback;

	int operstate;

	char mlmedev[IFNAMSIZ + 1];

	int scan_complete_events;

	struct nl_handle *nl_handle;
	struct nl_cache *nl_cache;
	struct nl_cb *nl_cb;
	struct genl_family *nl80211;

#ifdef CONFIG_CLIENT_MLME
	int monitor_sock; /* socket for monitor */
	int monitor_ifidx;
#endif /* CONFIG_CLIENT_MLME */
};


static void wpa_driver_nl80211_scan_timeout(void *eloop_ctx,
					    void *timeout_ctx);
static int wpa_driver_nl80211_set_mode(void *priv, int mode);
static int wpa_driver_nl80211_flush_pmkid(void *priv);
static int wpa_driver_nl80211_get_range(void *priv);
static void
wpa_driver_nl80211_finish_drv_init(struct wpa_driver_nl80211_data *drv);


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

static int send_and_recv_msgs(struct wpa_driver_nl80211_data *drv,
			      struct nl_msg *msg,
			      int (*valid_handler)(struct nl_msg *, void *),
			      void *valid_data)
{
	struct nl_cb *cb;
	int err = -ENOMEM;

	cb = nl_cb_clone(drv->nl_cb);
	if (!cb)
		goto out;

	err = nl_send_auto_complete(drv->nl_handle, msg);
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
		nl_recvmsgs(drv->nl_handle, cb);
 out:
	nl_cb_put(cb);
	nlmsg_free(msg);
	return err;
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


static int nl_get_multicast_id(struct wpa_driver_nl80211_data *drv,
			       const char *family, const char *group)
{
	struct nl_msg *msg;
	int ret = -1;
	struct family_data res = { group, -ENOENT };

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;
	genlmsg_put(msg, 0, 0, genl_ctrl_resolve(drv->nl_handle, "nlctrl"),
		    0, 0, CTRL_CMD_GETFAMILY, 0);
	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	ret = send_and_recv_msgs(drv, msg, family_handler, &res);
	msg = NULL;
	if (ret == 0)
		ret = res.id;

nla_put_failure:
	nlmsg_free(msg);
	return ret;
}


static int wpa_driver_nl80211_send_oper_ifla(
	struct wpa_driver_nl80211_data *drv,
	int linkmode, int operstate)
{
	struct {
		struct nlmsghdr hdr;
		struct ifinfomsg ifinfo;
		char opts[16];
	} req;
	struct rtattr *rta;
	static int nl_seq;
	ssize_t ret;

	os_memset(&req, 0, sizeof(req));

	req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.hdr.nlmsg_type = RTM_SETLINK;
	req.hdr.nlmsg_flags = NLM_F_REQUEST;
	req.hdr.nlmsg_seq = ++nl_seq;
	req.hdr.nlmsg_pid = 0;

	req.ifinfo.ifi_family = AF_UNSPEC;
	req.ifinfo.ifi_type = 0;
	req.ifinfo.ifi_index = drv->ifindex;
	req.ifinfo.ifi_flags = 0;
	req.ifinfo.ifi_change = 0;

	if (linkmode != -1) {
		rta = (struct rtattr *)
			((char *) &req + NLMSG_ALIGN(req.hdr.nlmsg_len));
		rta->rta_type = IFLA_LINKMODE;
		rta->rta_len = RTA_LENGTH(sizeof(char));
		*((char *) RTA_DATA(rta)) = linkmode;
		req.hdr.nlmsg_len = NLMSG_ALIGN(req.hdr.nlmsg_len) +
			RTA_LENGTH(sizeof(char));
	}
	if (operstate != -1) {
		rta = (struct rtattr *)
			((char *) &req + NLMSG_ALIGN(req.hdr.nlmsg_len));
		rta->rta_type = IFLA_OPERSTATE;
		rta->rta_len = RTA_LENGTH(sizeof(char));
		*((char *) RTA_DATA(rta)) = operstate;
		req.hdr.nlmsg_len = NLMSG_ALIGN(req.hdr.nlmsg_len) +
			RTA_LENGTH(sizeof(char));
	}

	wpa_printf(MSG_DEBUG, "WEXT: Operstate: linkmode=%d, operstate=%d",
		   linkmode, operstate);

	ret = send(drv->wext_event_sock, &req, req.hdr.nlmsg_len, 0);
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "WEXT: Sending operstate IFLA failed: "
			   "%s (assume operstate is not supported)",
			   strerror(errno));
	}

	return ret < 0 ? -1 : 0;
}


static int wpa_driver_nl80211_set_auth_param(
	struct wpa_driver_nl80211_data *drv, int idx, u32 value)
{
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.param.flags = idx & IW_AUTH_INDEX;
	iwr.u.param.value = value;

	if (ioctl(drv->ioctl_sock, SIOCSIWAUTH, &iwr) < 0) {
		if (errno != EOPNOTSUPP) {
			wpa_printf(MSG_DEBUG, "WEXT: SIOCSIWAUTH(param %d "
				   "value 0x%x) failed: %s)",
				   idx, value, strerror(errno));
		}
		ret = errno == EOPNOTSUPP ? -2 : -1;
	}

	return ret;
}


static int wpa_driver_nl80211_get_bssid(void *priv, u8 *bssid)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);

	if (ioctl(drv->ioctl_sock, SIOCGIWAP, &iwr) < 0) {
		perror("ioctl[SIOCGIWAP]");
		ret = -1;
	}
	os_memcpy(bssid, iwr.u.ap_addr.sa_data, ETH_ALEN);

	return ret;
}


static int wpa_driver_nl80211_set_bssid(void *priv, const u8 *bssid)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.ap_addr.sa_family = ARPHRD_ETHER;
	if (bssid)
		os_memcpy(iwr.u.ap_addr.sa_data, bssid, ETH_ALEN);
	else
		os_memset(iwr.u.ap_addr.sa_data, 0, ETH_ALEN);

	if (ioctl(drv->ioctl_sock, SIOCSIWAP, &iwr) < 0) {
		perror("ioctl[SIOCSIWAP]");
		ret = -1;
	}

	return ret;
}


static int wpa_driver_nl80211_get_ssid(void *priv, u8 *ssid)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) ssid;
	iwr.u.essid.length = 32;

	if (ioctl(drv->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCGIWESSID]");
		ret = -1;
	} else {
		ret = iwr.u.essid.length;
		if (ret > 32)
			ret = 32;
		/* Some drivers include nul termination in the SSID, so let's
		 * remove it here before further processing. WE-21 changes this
		 * to explicitly require the length _not_ to include nul
		 * termination. */
		if (ret > 0 && ssid[ret - 1] == '\0' &&
		    drv->we_version_compiled < 21)
			ret--;
	}

	return ret;
}


static int wpa_driver_nl80211_set_ssid(void *priv, const u8 *ssid,
				       size_t ssid_len)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;
	char buf[33];

	if (ssid_len > 32)
		return -1;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	/* flags: 1 = ESSID is active, 0 = not (promiscuous) */
	iwr.u.essid.flags = (ssid_len != 0);
	os_memset(buf, 0, sizeof(buf));
	os_memcpy(buf, ssid, ssid_len);
	iwr.u.essid.pointer = (caddr_t) buf;
	if (drv->we_version_compiled < 21) {
		/* For historic reasons, set SSID length to include one extra
		 * character, C string nul termination, even though SSID is
		 * really an octet string that should not be presented as a C
		 * string. Some Linux drivers decrement the length by one and
		 * can thus end up missing the last octet of the SSID if the
		 * length is not incremented here. WE-21 changes this to
		 * explicitly require the length _not_ to include nul
		 * termination. */
		if (ssid_len)
			ssid_len++;
	}
	iwr.u.essid.length = ssid_len;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		ret = -1;
	}

	return ret;
}


static int wpa_driver_nl80211_set_freq(void *priv, int freq)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.freq.m = freq * 100000;
	iwr.u.freq.e = 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWFREQ, &iwr) < 0) {
		perror("ioctl[SIOCSIWFREQ]");
		ret = -1;
	}

	return ret;
}


static void
wpa_driver_nl80211_event_wireless_custom(void *ctx, char *custom)
{
	union wpa_event_data data;

	wpa_printf(MSG_MSGDUMP, "WEXT: Custom wireless event: '%s'",
		   custom);

	os_memset(&data, 0, sizeof(data));
	/* Host AP driver */
	if (os_strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		data.michael_mic_failure.unicast =
			os_strstr(custom, " unicast ") != NULL;
		/* TODO: parse parameters(?) */
		wpa_supplicant_event(ctx, EVENT_MICHAEL_MIC_FAILURE, &data);
	} else if (os_strncmp(custom, "ASSOCINFO(ReqIEs=", 17) == 0) {
		char *spos;
		int bytes;

		spos = custom + 17;

		bytes = strspn(spos, "0123456789abcdefABCDEF");
		if (!bytes || (bytes & 1))
			return;
		bytes /= 2;

		data.assoc_info.req_ies = os_malloc(bytes);
		if (data.assoc_info.req_ies == NULL)
			return;

		data.assoc_info.req_ies_len = bytes;
		hexstr2bin(spos, data.assoc_info.req_ies, bytes);

		spos += bytes * 2;

		data.assoc_info.resp_ies = NULL;
		data.assoc_info.resp_ies_len = 0;

		if (os_strncmp(spos, " RespIEs=", 9) == 0) {
			spos += 9;

			bytes = strspn(spos, "0123456789abcdefABCDEF");
			if (!bytes || (bytes & 1))
				goto done;
			bytes /= 2;

			data.assoc_info.resp_ies = os_malloc(bytes);
			if (data.assoc_info.resp_ies == NULL)
				goto done;

			data.assoc_info.resp_ies_len = bytes;
			hexstr2bin(spos, data.assoc_info.resp_ies, bytes);
		}

		wpa_supplicant_event(ctx, EVENT_ASSOCINFO, &data);

	done:
		os_free(data.assoc_info.resp_ies);
		os_free(data.assoc_info.req_ies);
#ifdef CONFIG_PEERKEY
	} else if (os_strncmp(custom, "STKSTART.request=", 17) == 0) {
		if (hwaddr_aton(custom + 17, data.stkstart.peer)) {
			wpa_printf(MSG_DEBUG, "WEXT: unrecognized "
				   "STKSTART.request '%s'", custom + 17);
			return;
		}
		wpa_supplicant_event(ctx, EVENT_STKSTART, &data);
#endif /* CONFIG_PEERKEY */
	}
}


static int wpa_driver_nl80211_event_wireless_michaelmicfailure(
	void *ctx, const char *ev, size_t len)
{
	const struct iw_michaelmicfailure *mic;
	union wpa_event_data data;

	if (len < sizeof(*mic))
		return -1;

	mic = (const struct iw_michaelmicfailure *) ev;

	wpa_printf(MSG_DEBUG, "Michael MIC failure wireless event: "
		   "flags=0x%x src_addr=" MACSTR, mic->flags,
		   MAC2STR(mic->src_addr.sa_data));

	os_memset(&data, 0, sizeof(data));
	data.michael_mic_failure.unicast = !(mic->flags & IW_MICFAILURE_GROUP);
	wpa_supplicant_event(ctx, EVENT_MICHAEL_MIC_FAILURE, &data);

	return 0;
}


static int wpa_driver_nl80211_event_wireless_pmkidcand(
	struct wpa_driver_nl80211_data *drv, const char *ev, size_t len)
{
	const struct iw_pmkid_cand *cand;
	union wpa_event_data data;
	const u8 *addr;

	if (len < sizeof(*cand))
		return -1;

	cand = (const struct iw_pmkid_cand *) ev;
	addr = (const u8 *) cand->bssid.sa_data;

	wpa_printf(MSG_DEBUG, "PMKID candidate wireless event: "
		   "flags=0x%x index=%d bssid=" MACSTR, cand->flags,
		   cand->index, MAC2STR(addr));

	os_memset(&data, 0, sizeof(data));
	os_memcpy(data.pmkid_candidate.bssid, addr, ETH_ALEN);
	data.pmkid_candidate.index = cand->index;
	data.pmkid_candidate.preauth = cand->flags & IW_PMKID_CAND_PREAUTH;
	wpa_supplicant_event(drv->ctx, EVENT_PMKID_CANDIDATE, &data);

	return 0;
}


static int wpa_driver_nl80211_event_wireless_assocreqie(
	struct wpa_driver_nl80211_data *drv, const char *ev, int len)
{
	if (len < 0)
		return -1;

	wpa_hexdump(MSG_DEBUG, "AssocReq IE wireless event", (const u8 *) ev,
		    len);
	os_free(drv->assoc_req_ies);
	drv->assoc_req_ies = os_malloc(len);
	if (drv->assoc_req_ies == NULL) {
		drv->assoc_req_ies_len = 0;
		return -1;
	}
	os_memcpy(drv->assoc_req_ies, ev, len);
	drv->assoc_req_ies_len = len;

	return 0;
}


static int wpa_driver_nl80211_event_wireless_assocrespie(
	struct wpa_driver_nl80211_data *drv, const char *ev, int len)
{
	if (len < 0)
		return -1;

	wpa_hexdump(MSG_DEBUG, "AssocResp IE wireless event", (const u8 *) ev,
		    len);
	os_free(drv->assoc_resp_ies);
	drv->assoc_resp_ies = os_malloc(len);
	if (drv->assoc_resp_ies == NULL) {
		drv->assoc_resp_ies_len = 0;
		return -1;
	}
	os_memcpy(drv->assoc_resp_ies, ev, len);
	drv->assoc_resp_ies_len = len;

	return 0;
}


static void wpa_driver_nl80211_event_assoc_ies(struct wpa_driver_nl80211_data *drv)
{
	union wpa_event_data data;

	if (drv->assoc_req_ies == NULL && drv->assoc_resp_ies == NULL)
		return;

	os_memset(&data, 0, sizeof(data));
	if (drv->assoc_req_ies) {
		data.assoc_info.req_ies = drv->assoc_req_ies;
		drv->assoc_req_ies = NULL;
		data.assoc_info.req_ies_len = drv->assoc_req_ies_len;
	}
	if (drv->assoc_resp_ies) {
		data.assoc_info.resp_ies = drv->assoc_resp_ies;
		drv->assoc_resp_ies = NULL;
		data.assoc_info.resp_ies_len = drv->assoc_resp_ies_len;
	}

	wpa_supplicant_event(drv->ctx, EVENT_ASSOCINFO, &data);

	os_free(data.assoc_info.req_ies);
	os_free(data.assoc_info.resp_ies);
}


static void wpa_driver_nl80211_event_wireless(struct wpa_driver_nl80211_data *drv,
					   void *ctx, char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		os_memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		wpa_printf(MSG_DEBUG, "Wireless event: cmd=0x%x len=%d",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (drv->we_version_compiled > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVCUSTOM ||
		     iwe->cmd == IWEVASSOCREQIE ||
		     iwe->cmd == IWEVASSOCRESPIE ||
		     iwe->cmd == IWEVPMKIDCAND)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			os_memcpy(dpos, pos + IW_EV_LCP_LEN,
				  sizeof(struct iw_event) - dlen);
		} else {
			os_memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case SIOCGIWAP:
			wpa_printf(MSG_DEBUG, "Wireless event: new AP: "
				   MACSTR,
				   MAC2STR((u8 *) iwe->u.ap_addr.sa_data));
			if (is_zero_ether_addr(
				    (const u8 *) iwe->u.ap_addr.sa_data) ||
			    os_memcmp(iwe->u.ap_addr.sa_data,
				      "\x44\x44\x44\x44\x44\x44", ETH_ALEN) ==
			    0) {
				os_free(drv->assoc_req_ies);
				drv->assoc_req_ies = NULL;
				os_free(drv->assoc_resp_ies);
				drv->assoc_resp_ies = NULL;
				wpa_supplicant_event(ctx, EVENT_DISASSOC,
						     NULL);
			
			} else {
				wpa_driver_nl80211_event_assoc_ies(drv);
				wpa_supplicant_event(ctx, EVENT_ASSOC, NULL);
			}
			break;
		case IWEVMICHAELMICFAILURE:
			wpa_driver_nl80211_event_wireless_michaelmicfailure(
				ctx, custom, iwe->u.data.length);
			break;
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = os_malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;
			os_memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			wpa_driver_nl80211_event_wireless_custom(ctx, buf);
			os_free(buf);
			break;
		case IWEVASSOCREQIE:
			wpa_driver_nl80211_event_wireless_assocreqie(
				drv, custom, iwe->u.data.length);
			break;
		case IWEVASSOCRESPIE:
			wpa_driver_nl80211_event_wireless_assocrespie(
				drv, custom, iwe->u.data.length);
			break;
		case IWEVPMKIDCAND:
			wpa_driver_nl80211_event_wireless_pmkidcand(
				drv, custom, iwe->u.data.length);
			break;
		}

		pos += iwe->len;
	}
}


static void wpa_driver_nl80211_event_link(struct wpa_driver_nl80211_data *drv,
					  void *ctx, char *buf, size_t len,
					  int del)
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

	if (os_strcmp(drv->ifname, event.interface_status.ifname) == 0) {
		if (del)
			drv->if_removed = 1;
		else
			drv->if_removed = 0;
	}

	wpa_supplicant_event(ctx, EVENT_INTERFACE_STATUS, &event);
}


static int wpa_driver_nl80211_own_ifname(struct wpa_driver_nl80211_data *drv,
					 struct nlmsghdr *h)
{
	struct ifinfomsg *ifi;
	int attrlen, _nlmsg_len, rta_len;
	struct rtattr *attr;

	ifi = NLMSG_DATA(h);

	_nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - _nlmsg_len;
	if (attrlen < 0)
		return 0;

	attr = (struct rtattr *) (((char *) ifi) + _nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			if (os_strcmp(((char *) attr) + rta_len, drv->ifname)
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
					  int ifindex, struct nlmsghdr *h)
{
	if (drv->ifindex == ifindex)
		return 1;

	if (drv->if_removed && wpa_driver_nl80211_own_ifname(drv, h)) {
		drv->ifindex = if_nametoindex(drv->ifname);
		wpa_printf(MSG_DEBUG, "nl80211: Update ifindex for a removed "
			   "interface");
		wpa_driver_nl80211_finish_drv_init(drv);
		return 1;
	}

	return 0;
}


static void wpa_driver_nl80211_event_rtm_newlink(struct wpa_driver_nl80211_data *drv,
					      void *ctx, struct nlmsghdr *h,
					      size_t len)
{
	struct ifinfomsg *ifi;
	int attrlen, _nlmsg_len, rta_len;
	struct rtattr * attr;

	if (len < sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	if (!wpa_driver_nl80211_own_ifindex(drv, ifi->ifi_index, h)) {
		wpa_printf(MSG_DEBUG, "Ignore event for foreign ifindex %d",
			   ifi->ifi_index);
		return;
	}

	wpa_printf(MSG_DEBUG, "RTM_NEWLINK: operstate=%d ifi_flags=0x%x "
		   "(%s%s%s%s)",
		   drv->operstate, ifi->ifi_flags,
		   (ifi->ifi_flags & IFF_UP) ? "[UP]" : "",
		   (ifi->ifi_flags & IFF_RUNNING) ? "[RUNNING]" : "",
		   (ifi->ifi_flags & IFF_LOWER_UP) ? "[LOWER_UP]" : "",
		   (ifi->ifi_flags & IFF_DORMANT) ? "[DORMANT]" : "");
	/*
	 * Some drivers send the association event before the operup event--in
	 * this case, lifting operstate in wpa_driver_nl80211_set_operstate()
	 * fails. This will hit us when wpa_supplicant does not need to do
	 * IEEE 802.1X authentication
	 */
	if (drv->operstate == 1 &&
	    (ifi->ifi_flags & (IFF_LOWER_UP | IFF_DORMANT)) == IFF_LOWER_UP &&
	    !(ifi->ifi_flags & IFF_RUNNING))
		wpa_driver_nl80211_send_oper_ifla(drv, -1, IF_OPER_UP);

	_nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - _nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + _nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			wpa_driver_nl80211_event_wireless(
				drv, ctx, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		} else if (attr->rta_type == IFLA_IFNAME) {
			wpa_driver_nl80211_event_link(
				drv, ctx,
				((char *) attr) + rta_len,
				attr->rta_len - rta_len, 0);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void wpa_driver_nl80211_event_rtm_dellink(struct wpa_driver_nl80211_data *drv,
					      void *ctx, struct nlmsghdr *h,
					      size_t len)
{
	struct ifinfomsg *ifi;
	int attrlen, _nlmsg_len, rta_len;
	struct rtattr * attr;

	if (len < sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	_nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - _nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + _nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_IFNAME) {
			wpa_driver_nl80211_event_link(
				drv, ctx,
				((char *) attr) + rta_len,
				attr->rta_len - rta_len, 1);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void wpa_driver_nl80211_event_receive_wext(int sock, void *eloop_ctx,
						  void *sock_ctx)
{
	char buf[8192];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;
	int max_events = 10;

try_again:
	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *) &from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	h = (struct nlmsghdr *) buf;
	while (left >= (int) sizeof(*h)) {
		int len, plen;

		len = h->nlmsg_len;
		plen = len - sizeof(*h);
		if (len > left || plen < 0) {
			wpa_printf(MSG_DEBUG, "Malformed netlink message: "
				   "len=%d left=%d plen=%d",
				   len, left, plen);
			break;
		}

		switch (h->nlmsg_type) {
		case RTM_NEWLINK:
			wpa_driver_nl80211_event_rtm_newlink(eloop_ctx, sock_ctx,
							  h, plen);
			break;
		case RTM_DELLINK:
			wpa_driver_nl80211_event_rtm_dellink(eloop_ctx, sock_ctx,
							  h, plen);
			break;
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		h = (struct nlmsghdr *) ((char *) h + len);
	}

	if (left > 0) {
		wpa_printf(MSG_DEBUG, "%d extra bytes in the end of netlink "
			   "message", left);
	}

	if (--max_events > 0) {
		/*
		 * Try to receive all events in one eloop call in order to
		 * limit race condition on cases where AssocInfo event, Assoc
		 * event, and EAPOL frames are received more or less at the
		 * same time. We want to process the event messages first
		 * before starting EAPOL processing.
		 */
		goto try_again;
	}
}


static int no_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}


static int process_event(struct nl_msg *msg, void *arg)
{
	struct wpa_driver_nl80211_data *drv = arg;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *tb[NL80211_ATTR_MAX + 1];

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_IFINDEX]) {
		int ifindex = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);
		if (ifindex != drv->ifindex) {
			wpa_printf(MSG_DEBUG, "nl80211: Ignored event (cmd=%d)"
				   " for foreign interface (ifindex %d)",
				   gnlh->cmd, ifindex);
			return NL_SKIP;
		}
	}

	switch (gnlh->cmd) {
	case NL80211_CMD_NEW_SCAN_RESULTS:
		wpa_printf(MSG_DEBUG, "nl80211: New scan results available");
		drv->scan_complete_events = 1;
		eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv,
				     drv->ctx);
		wpa_supplicant_event(drv->ctx, EVENT_SCAN_RESULTS, NULL);
		break;
	case NL80211_CMD_SCAN_ABORTED:
		wpa_printf(MSG_DEBUG, "nl80211: Scan aborted");
		/*
		 * Need to indicate that scan results are available in order
		 * not to make wpa_supplicant stop its scanning.
		 */
		eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv,
				     drv->ctx);
		wpa_supplicant_event(drv->ctx, EVENT_SCAN_RESULTS, NULL);
		break;
	default:
		wpa_printf(MSG_DEBUG, "nl0211: Ignored unknown event (cmd=%d)",
			   gnlh->cmd);
		break;
	}

	return NL_SKIP;
}


static void wpa_driver_nl80211_event_receive(int sock, void *eloop_ctx,
					     void *sock_ctx)
{
	struct nl_cb *cb;
	struct wpa_driver_nl80211_data *drv = eloop_ctx;

	wpa_printf(MSG_DEBUG, "nl80211: Event message available");

	cb = nl_cb_clone(drv->nl_cb);
	if (!cb)
		return;
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, process_event, drv);
	nl_recvmsgs(drv->nl_handle, cb);
	nl_cb_put(cb);
}


static int wpa_driver_nl80211_get_ifflags_ifname(struct wpa_driver_nl80211_data *drv,
					      const char *ifname, int *flags)
{
	struct ifreq ifr;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(drv->ioctl_sock, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		return -1;
	}
	*flags = ifr.ifr_flags & 0xffff;
	return 0;
}


/**
 * wpa_driver_nl80211_get_ifflags - Get interface flags (SIOCGIFFLAGS)
 * @drv: driver_nl80211 private data
 * @flags: Pointer to returned flags value
 * Returns: 0 on success, -1 on failure
 */
static int wpa_driver_nl80211_get_ifflags(struct wpa_driver_nl80211_data *drv,
					  int *flags)
{
	return wpa_driver_nl80211_get_ifflags_ifname(drv, drv->ifname, flags);
}


static int wpa_driver_nl80211_set_ifflags_ifname(
	struct wpa_driver_nl80211_data *drv,
	const char *ifname, int flags)
{
	struct ifreq ifr;

	os_memset(&ifr, 0, sizeof(ifr));
	os_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_flags = flags & 0xffff;
	if (ioctl(drv->ioctl_sock, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return -1;
	}
	return 0;
}


/**
 * wpa_driver_nl80211_set_ifflags - Set interface flags (SIOCSIFFLAGS)
 * @drv: driver_nl80211 private data
 * @flags: New value for flags
 * Returns: 0 on success, -1 on failure
 */
static int wpa_driver_nl80211_set_ifflags(struct wpa_driver_nl80211_data *drv,
					  int flags)
{
	return wpa_driver_nl80211_set_ifflags_ifname(drv, drv->ifname, flags);
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
	struct wpa_driver_nl80211_data *drv = priv;
	char alpha2[3];
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		goto nla_put_failure;

	alpha2[0] = alpha2_arg[0];
	alpha2[1] = alpha2_arg[1];
	alpha2[2] = '\0';

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_REQ_SET_REG, 0);

	NLA_PUT_STRING(msg, NL80211_ATTR_REG_ALPHA2, alpha2);
	if (send_and_recv_msgs(drv, msg, NULL, NULL))
		return -EINVAL;
	return 0;
nla_put_failure:
	return -EINVAL;
}


static int wpa_driver_nl80211_set_probe_req_ie(void *priv, const u8 *ies,
					       size_t ies_len)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, 0,
		    NL80211_CMD_SET_MGMT_EXTRA_IE, 0);

	NLA_PUT_U8(msg, NL80211_ATTR_MGMT_SUBTYPE, 4 /* ProbeReq */);
	if (ies)
		NLA_PUT(msg, NL80211_ATTR_IE, ies_len, ies);

	ret = 0;

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	return ret;

nla_put_failure:
	return -ENOBUFS;
}


#ifdef CONFIG_CLIENT_MLME

static int nl80211_set_vif(struct wpa_driver_nl80211_data *drv,
			   int drop_unencrypted, int userspace_mlme)
{
#ifdef NL80211_CMD_SET_VIF
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, 0,
		    NL80211_CMD_SET_VIF, 0);

	if (drop_unencrypted >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_VIF_DROP_UNENCRYPTED,
			   drop_unencrypted);
	if (userspace_mlme >= 0)
		NLA_PUT_U8(msg, NL80211_ATTR_VIF_USERSPACE_MLME,
			   userspace_mlme);

	ret = 0;

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	return ret;

nla_put_failure:
	return -ENOBUFS;
#else /* NL80211_CMD_SET_VIF */
	return -1;
#endif /* NL80211_CMD_SET_VIF */
}


static int wpa_driver_nl80211_set_userspace_mlme(
	struct wpa_driver_nl80211_data *drv, int enabled)
{
	return nl80211_set_vif(drv, -1, enabled);
}


static void nl80211_remove_iface(struct wpa_driver_nl80211_data *drv,
				 int ifidx)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		goto nla_put_failure;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_DEL_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, ifidx);
	if (send_and_recv_msgs(drv, msg, NULL, NULL) == 0)
		return;
nla_put_failure:
	wpa_printf(MSG_ERROR, "nl80211: Failed to remove interface.");
}


static int nl80211_create_iface(struct wpa_driver_nl80211_data *drv,
				const char *ifname, enum nl80211_iftype iftype)
{
	struct nl_msg *msg, *flags = NULL;
	int ifidx, err;
	int ret = -ENOBUFS;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_NEW_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, if_nametoindex(drv->ifname));
	NLA_PUT_STRING(msg, NL80211_ATTR_IFNAME, ifname);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE, iftype);

	if (iftype == NL80211_IFTYPE_MONITOR) {
		flags = nlmsg_alloc();
		if (!flags)
			goto nla_put_failure;

		NLA_PUT_FLAG(flags, NL80211_MNTR_FLAG_COOK_FRAMES);

		err = nla_put_nested(msg, NL80211_ATTR_MNTR_FLAGS, flags);

		nlmsg_free(flags);

		if (err)
			goto nla_put_failure;
	}

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (ret) {
	nla_put_failure:
		wpa_printf(MSG_ERROR, "nl80211: Failed to create interface %d",
			   ret);
		return ret;
	}

	ifidx = if_nametoindex(ifname);
	if (ifidx <= 0)
		return -1;

	return ifidx;
}


static void handle_monitor_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wpa_driver_nl80211_data *drv = eloop_ctx;
	int len;
	unsigned char buf[3000];
	struct ieee80211_radiotap_iterator iter;
	int ret;
	int injected = 0, failed = 0, rxflags = 0;
	struct ieee80211_rx_status rx_status;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		perror("recv");
		return;
	}

	if (ieee80211_radiotap_iterator_init(&iter, (void *) buf, len)) {
		wpa_printf(MSG_DEBUG, "nl80211: received invalid radiotap "
			   "frame");
		return;
	}

	os_memset(&rx_status, 0, sizeof(rx_status));

	while (1) {
		ret = ieee80211_radiotap_iterator_next(&iter);
		if (ret == -ENOENT)
			break;
		if (ret) {
			wpa_printf(MSG_DEBUG, "nl80211: received invalid "
				   "radiotap frame (%d)", ret);
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
			failed = le_to_host16((*(u16 *) iter.this_arg)) &
				IEEE80211_RADIOTAP_F_TX_FAIL;
			break;
		case IEEE80211_RADIOTAP_DATA_RETRIES:
			break;
		case IEEE80211_RADIOTAP_CHANNEL:
			/* TODO convert from freq/flags to channel number
			 * rx_status.channel = XXX;
			*/
			break;
		case IEEE80211_RADIOTAP_RATE:
			break;
		case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
			rx_status.ssi = *iter.this_arg;
			break;
		}
	}

	if (rxflags && injected)
		return;

	if (!injected) {
		wpa_supplicant_sta_rx(drv->ctx, buf + iter.max_length,
				      len - iter.max_length, &rx_status);
	} else if (failed) {
		/* TX failure callback */
	} else {
		/* TX success (ACK) callback */
	}
}


static int wpa_driver_nl80211_create_monitor_interface(
	struct wpa_driver_nl80211_data *drv)
{
	char buf[IFNAMSIZ];
	struct sockaddr_ll ll;
	int optval, flags;
	socklen_t optlen;

	os_snprintf(buf, IFNAMSIZ, "mon.%s", drv->ifname);
	buf[IFNAMSIZ - 1] = '\0';

	drv->monitor_ifidx =
		nl80211_create_iface(drv, buf, NL80211_IFTYPE_MONITOR);

	if (drv->monitor_ifidx < 0)
		return -1;

	if (wpa_driver_nl80211_get_ifflags_ifname(drv, buf, &flags) != 0 ||
	    wpa_driver_nl80211_set_ifflags_ifname(drv, buf, flags | IFF_UP) !=
	    0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not set interface '%s' "
			   "UP", buf);
		goto error;
	}

	os_memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = drv->monitor_ifidx;
	drv->monitor_sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (drv->monitor_sock < 0) {
		perror("socket[PF_PACKET,SOCK_RAW]");
		goto error;
	}

	if (bind(drv->monitor_sock, (struct sockaddr *) &ll,
		 sizeof(ll)) < 0) {
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
		wpa_printf(MSG_ERROR, "nl80211: Could not register monitor "
			   "read socket");
		goto error;
	}

	return 0;

 error:
	nl80211_remove_iface(drv, drv->monitor_ifidx);
	return -1;
}

#endif /* CONFIG_CLIENT_MLME */


/**
 * wpa_driver_nl80211_init - Initialize nl80211 driver interface
 * @ctx: context to be used when calling wpa_supplicant functions,
 * e.g., wpa_supplicant_event()
 * @ifname: interface name, e.g., wlan0
 * Returns: Pointer to private data, %NULL on failure
 */
static void * wpa_driver_nl80211_init(void *ctx, const char *ifname)
{
	int s, ret;
	struct sockaddr_nl local;
	struct wpa_driver_nl80211_data *drv;

	drv = os_zalloc(sizeof(*drv));
	if (drv == NULL)
		return NULL;
	drv->ctx = ctx;
	os_strlcpy(drv->ifname, ifname, sizeof(drv->ifname));

	drv->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (drv->nl_cb == NULL) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to allocate netlink "
			   "callbacks");
		goto err1;
	}

	drv->nl_handle = nl_handle_alloc_cb(drv->nl_cb);
	if (drv->nl_handle == NULL) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to allocate netlink "
			   "callbacks");
		goto err2;
	}

	if (genl_connect(drv->nl_handle)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to connect to generic "
			   "netlink");
		goto err3;
	}

	drv->nl_cache = genl_ctrl_alloc_cache(drv->nl_handle);
	if (drv->nl_cache == NULL) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to allocate generic "
			   "netlink cache");
		goto err3;
	}

	drv->nl80211 = genl_ctrl_search_by_name(drv->nl_cache, "nl80211");
	if (drv->nl80211 == NULL) {
		wpa_printf(MSG_ERROR, "nl80211: 'nl80211' generic netlink not "
			   "found");
		goto err4;
	}

	ret = nl_get_multicast_id(drv, "nl80211", "scan");
	if (ret >= 0)
		ret = nl_socket_add_membership(drv->nl_handle, ret);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Could not add multicast "
			   "membership for scan events: %d (%s)",
			   ret, strerror(-ret));
		goto err4;
	}
	eloop_register_read_sock(nl_socket_get_fd(drv->nl_handle),
				 wpa_driver_nl80211_event_receive, drv, ctx);

	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		perror("socket(PF_INET,SOCK_DGRAM)");
		goto err5;
	}

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s < 0) {
		perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
		goto err6;
	}

	os_memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;
	if (bind(s, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind(netlink)");
		close(s);
		goto err6;
	}

	eloop_register_read_sock(s, wpa_driver_nl80211_event_receive_wext, drv,
				 ctx);
	drv->wext_event_sock = s;

	wpa_driver_nl80211_finish_drv_init(drv);

	return drv;

err6:
	close(drv->ioctl_sock);
err5:
	genl_family_put(drv->nl80211);
err4:
	nl_cache_free(drv->nl_cache);
err3:
	nl_handle_destroy(drv->nl_handle);
err2:
	nl_cb_put(drv->nl_cb);
err1:
	os_free(drv);
	return NULL;
}


static void
wpa_driver_nl80211_finish_drv_init(struct wpa_driver_nl80211_data *drv)
{
	int flags;

	if (wpa_driver_nl80211_get_ifflags(drv, &flags) != 0)
		printf("Could not get interface '%s' flags\n", drv->ifname);
	else if (!(flags & IFF_UP)) {
		if (wpa_driver_nl80211_set_ifflags(drv, flags | IFF_UP) != 0) {
			printf("Could not set interface '%s' UP\n",
			       drv->ifname);
		}
	}

	/*
	 * Make sure that the driver does not have any obsolete PMKID entries.
	 */
	wpa_driver_nl80211_flush_pmkid(drv);

	if (wpa_driver_nl80211_set_mode(drv, 0) < 0) {
		printf("Could not configure driver to use managed mode\n");
	}

	wpa_driver_nl80211_get_range(drv);

	drv->ifindex = if_nametoindex(drv->ifname);

	wpa_driver_nl80211_send_oper_ifla(drv, 1, IF_OPER_DORMANT);
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
	struct wpa_driver_nl80211_data *drv = priv;
	int flags;

#ifdef CONFIG_CLIENT_MLME
	if (drv->monitor_sock >= 0) {
		eloop_unregister_read_sock(drv->monitor_sock);
		close(drv->monitor_sock);
	}
	if (drv->monitor_ifidx > 0)
		nl80211_remove_iface(drv, drv->monitor_ifidx);
	if (drv->capa.flags & WPA_DRIVER_FLAGS_USER_SPACE_MLME)
		wpa_driver_nl80211_set_userspace_mlme(drv, 0);
#endif /* CONFIG_CLIENT_MLME */

	eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv, drv->ctx);

	/*
	 * Clear possibly configured driver parameters in order to make it
	 * easier to use the driver after wpa_supplicant has been terminated.
	 */
	(void) wpa_driver_nl80211_set_bssid(drv,
					 (u8 *) "\x00\x00\x00\x00\x00\x00");

	wpa_driver_nl80211_send_oper_ifla(priv, 0, IF_OPER_UP);

	eloop_unregister_read_sock(drv->wext_event_sock);

	if (wpa_driver_nl80211_get_ifflags(drv, &flags) == 0)
		(void) wpa_driver_nl80211_set_ifflags(drv, flags & ~IFF_UP);

	close(drv->wext_event_sock);
	close(drv->ioctl_sock);
	os_free(drv->assoc_req_ies);
	os_free(drv->assoc_resp_ies);

	eloop_unregister_read_sock(nl_socket_get_fd(drv->nl_handle));
	genl_family_put(drv->nl80211);
	nl_cache_free(drv->nl_cache);
	nl_handle_destroy(drv->nl_handle);
	nl_cb_put(drv->nl_cb);

	os_free(drv);
}


/**
 * wpa_driver_nl80211_scan_timeout - Scan timeout to report scan completion
 * @eloop_ctx: Unused
 * @timeout_ctx: ctx argument given to wpa_driver_nl80211_init()
 *
 * This function can be used as registered timeout when starting a scan to
 * generate a scan completed event if the driver does not report this.
 */
static void wpa_driver_nl80211_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	wpa_printf(MSG_DEBUG, "Scan timeout - try to get results");
	wpa_supplicant_event(timeout_ctx, EVENT_SCAN_RESULTS, NULL);
}


/**
 * wpa_driver_nl80211_scan - Request the driver to initiate scan
 * @priv: Pointer to private wext data from wpa_driver_nl80211_init()
 * @ssid: Specific SSID to scan for (ProbeReq) or %NULL to scan for
 *	all SSIDs (either active scan with broadcast SSID or passive
 *	scan
 * @ssid_len: Length of the SSID
 * Returns: 0 on success, -1 on failure
 */
static int wpa_driver_nl80211_scan(void *priv, const u8 *ssid, size_t ssid_len)
{
	struct wpa_driver_nl80211_data *drv = priv;
	int ret = 0, timeout;
	struct nl_msg *msg, *ssids;

	msg = nlmsg_alloc();
	ssids = nlmsg_alloc();
	if (!msg || !ssids) {
		nlmsg_free(msg);
		nlmsg_free(ssids);
		return -1;
	}

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, 0,
		    NL80211_CMD_TRIGGER_SCAN, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	if (ssid && ssid_len) {
		/* Request an active scan for a specific SSID */
		NLA_PUT(ssids, 1, ssid_len, ssid);
	} else {
		/* Request an active scan for wildcard SSID */
		NLA_PUT(ssids, 1, 0, "");
	}
	nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	msg = NULL;
	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: Scan trigger failed: ret=%d "
			   "(%s)", ret, strerror(-ret));
		goto nla_put_failure;
	}

	/* Not all drivers generate "scan completed" wireless event, so try to
	 * read results after a timeout. */
	timeout = 10;
	if (drv->scan_complete_events) {
		/*
		 * The driver seems to deliver SIOCGIWSCAN events to notify
		 * when scan is complete, so use longer timeout to avoid race
		 * conditions with scanning and following association request.
		 */
		timeout = 30;
	}
	wpa_printf(MSG_DEBUG, "Scan requested (ret=%d) - scan timeout %d "
		   "seconds", ret, timeout);
	eloop_cancel_timeout(wpa_driver_nl80211_scan_timeout, drv, drv->ctx);
	eloop_register_timeout(timeout, 0, wpa_driver_nl80211_scan_timeout,
			       drv, drv->ctx);

nla_put_failure:
	nlmsg_free(ssids);
	nlmsg_free(msg);
	return ret;
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
	};
	struct wpa_scan_results *res = arg;
	struct wpa_scan_res **tmp;
	struct wpa_scan_res *r;
	const u8 *ie;
	size_t ie_len;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb[NL80211_ATTR_BSS])
		return NL_SKIP;
	if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS],
			     bss_policy))
		return NL_SKIP;
	if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
		ie = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
		ie_len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
	} else {
		ie = NULL;
		ie_len = 0;
	}

	r = os_zalloc(sizeof(*r) + ie_len);
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
	if (bss[NL80211_BSS_SIGNAL_UNSPEC])
		r->qual = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
	if (bss[NL80211_BSS_SIGNAL_MBM])
		r->level = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
	if (bss[NL80211_BSS_TSF])
		r->tsf = nla_get_u64(bss[NL80211_BSS_TSF]);
	r->ie_len = ie_len;
	if (ie)
		os_memcpy(r + 1, ie, ie_len);

	tmp = os_realloc(res->res,
			 (res->num + 1) * sizeof(struct wpa_scan_res *));
	if (tmp == NULL) {
		os_free(r);
		return NL_SKIP;
	}
	tmp[res->num++] = r;
	res->res = tmp;

	return NL_SKIP;
}


/**
 * wpa_driver_nl80211_get_scan_results - Fetch the latest scan results
 * @priv: Pointer to private wext data from wpa_driver_nl80211_init()
 * Returns: Scan results on success, -1 on failure
 */
static struct wpa_scan_results *
wpa_driver_nl80211_get_scan_results(void *priv)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct nl_msg *msg;
	struct wpa_scan_results *res;
	int ret;

	res = os_zalloc(sizeof(*res));
	if (res == NULL)
		return 0;
	msg = nlmsg_alloc();
	if (!msg)
		goto nla_put_failure;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, NLM_F_DUMP,
		    NL80211_CMD_GET_SCAN, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	ret = send_and_recv_msgs(drv, msg, bss_info_handler, res);
	msg = NULL;
	if (ret == 0) {
		wpa_printf(MSG_DEBUG, "Received scan results (%lu BSSes)",
			   (unsigned long) res->num);
		return res;
	}
	wpa_printf(MSG_DEBUG, "nl80211: Scan result fetch failed: ret=%d "
		   "(%s)", ret, strerror(-ret));
nla_put_failure:
	nlmsg_free(msg);
	wpa_scan_results_free(res);
	return NULL;
}


static int wpa_driver_nl80211_get_range(void *priv)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = os_zalloc(buflen);
	if (range == NULL)
		return -1;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		os_free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->has_capability = 1;
		drv->we_version_compiled = range->we_version_compiled;
		if (range->enc_capa & IW_ENC_CAPA_WPA) {
			drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK;
		}
		if (range->enc_capa & IW_ENC_CAPA_WPA2) {
			drv->capa.key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
				WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;
		}
		drv->capa.enc |= WPA_DRIVER_CAPA_ENC_WEP40 |
			WPA_DRIVER_CAPA_ENC_WEP104;
		if (range->enc_capa & IW_ENC_CAPA_CIPHER_TKIP)
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_TKIP;
		if (range->enc_capa & IW_ENC_CAPA_CIPHER_CCMP)
			drv->capa.enc |= WPA_DRIVER_CAPA_ENC_CCMP;
		wpa_printf(MSG_DEBUG, "  capabilities: key_mgmt 0x%x enc 0x%x",
			   drv->capa.key_mgmt, drv->capa.enc);
	} else {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: too old (short) data - "
			   "assuming WPA is not supported");
	}

	os_free(range);
	return 0;
}


static int wpa_driver_nl80211_set_wpa(void *priv, int enabled)
{
	struct wpa_driver_nl80211_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

	return wpa_driver_nl80211_set_auth_param(drv, IW_AUTH_WPA_ENABLED,
					      enabled);
}


static int wpa_driver_nl80211_set_key(void *priv, wpa_alg alg,
				      const u8 *addr, int key_idx,
				      int set_tx, const u8 *seq,
				      size_t seq_len,
				      const u8 *key, size_t key_len)
{
	struct wpa_driver_nl80211_data *drv = priv;
	int err;
	struct nl_msg *msg;

	wpa_printf(MSG_DEBUG, "%s: alg=%d addr=%p key_idx=%d set_tx=%d "
		   "seq_len=%lu key_len=%lu",
		   __func__, alg, addr, key_idx, set_tx,
		   (unsigned long) seq_len, (unsigned long) key_len);

	msg = nlmsg_alloc();
	if (msg == NULL)
		return -1;

	if (alg == WPA_ALG_NONE) {
		genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, 0,
			    NL80211_CMD_DEL_KEY, 0);
	} else {
		genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0, 0,
			    NL80211_CMD_NEW_KEY, 0);
		NLA_PUT(msg, NL80211_ATTR_KEY_DATA, key_len, key);
		switch (alg) {
		case WPA_ALG_WEP:
			if (key_len == 5)
				NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
					    0x000FAC01);
			else
				NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER,
					    0x000FAC05);
			break;
		case WPA_ALG_TKIP:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER, 0x000FAC02);
			break;
		case WPA_ALG_CCMP:
			NLA_PUT_U32(msg, NL80211_ATTR_KEY_CIPHER, 0x000FAC04);
			break;
		default:
			nlmsg_free(msg);
			return -1;
		}
	}

	if (addr && os_memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) != 0)
	{
		wpa_printf(MSG_DEBUG, "   addr=" MACSTR, MAC2STR(addr));
		NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	}
	NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, key_idx);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	err = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (err) {
		wpa_printf(MSG_DEBUG, "nl80211: set_key failed; err=%d", err);
		return -1;
	}

	if (set_tx && alg != WPA_ALG_NONE) {
		msg = nlmsg_alloc();
		if (msg == NULL)
			return -1;

		genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
			    0, NL80211_CMD_SET_KEY, 0);
		NLA_PUT_U8(msg, NL80211_ATTR_KEY_IDX, key_idx);
		NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
		NLA_PUT_FLAG(msg, NL80211_ATTR_KEY_DEFAULT);

		err = send_and_recv_msgs(drv, msg, NULL, NULL);
		if (err) {
			wpa_printf(MSG_DEBUG, "nl80211: set default key "
				   "failed; err=%d", err);
			return -1;
		}
	}

	return 0;

nla_put_failure:
	return -ENOBUFS;
}


static int wpa_driver_nl80211_set_countermeasures(void *priv,
					       int enabled)
{
	struct wpa_driver_nl80211_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return wpa_driver_nl80211_set_auth_param(drv,
					      IW_AUTH_TKIP_COUNTERMEASURES,
					      enabled);
}


static int wpa_driver_nl80211_set_drop_unencrypted(void *priv,
						int enabled)
{
	struct wpa_driver_nl80211_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	drv->use_crypt = enabled;
	return wpa_driver_nl80211_set_auth_param(drv, IW_AUTH_DROP_UNENCRYPTED,
					      enabled);
}


static int wpa_driver_nl80211_mlme(struct wpa_driver_nl80211_data *drv,
				const u8 *addr, int cmd, int reason_code)
{
	struct iwreq iwr;
	struct iw_mlme mlme;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	os_memset(&mlme, 0, sizeof(mlme));
	mlme.cmd = cmd;
	mlme.reason_code = reason_code;
	mlme.addr.sa_family = ARPHRD_ETHER;
	os_memcpy(mlme.addr.sa_data, addr, ETH_ALEN);
	iwr.u.data.pointer = (caddr_t) &mlme;
	iwr.u.data.length = sizeof(mlme);

	if (ioctl(drv->ioctl_sock, SIOCSIWMLME, &iwr) < 0) {
		perror("ioctl[SIOCSIWMLME]");
		ret = -1;
	}

	return ret;
}


static int wpa_driver_nl80211_deauthenticate(void *priv, const u8 *addr,
					  int reason_code)
{
	struct wpa_driver_nl80211_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return wpa_driver_nl80211_mlme(drv, addr, IW_MLME_DEAUTH, reason_code);
}


static int wpa_driver_nl80211_disassociate(void *priv, const u8 *addr,
					int reason_code)
{
	struct wpa_driver_nl80211_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return wpa_driver_nl80211_mlme(drv, addr, IW_MLME_DISASSOC,
				    reason_code);
}


static int wpa_driver_nl80211_set_gen_ie(void *priv, const u8 *ie,
				      size_t ie_len)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) ie;
	iwr.u.data.length = ie_len;

	if (ioctl(drv->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
		perror("ioctl[SIOCSIWGENIE]");
		ret = -1;
	}

	return ret;
}


static int wpa_driver_nl80211_cipher2wext(int cipher)
{
	switch (cipher) {
	case CIPHER_NONE:
		return IW_AUTH_CIPHER_NONE;
	case CIPHER_WEP40:
		return IW_AUTH_CIPHER_WEP40;
	case CIPHER_TKIP:
		return IW_AUTH_CIPHER_TKIP;
	case CIPHER_CCMP:
		return IW_AUTH_CIPHER_CCMP;
	case CIPHER_WEP104:
		return IW_AUTH_CIPHER_WEP104;
	default:
		return 0;
	}
}


static int wpa_driver_nl80211_keymgmt2wext(int keymgmt)
{
	switch (keymgmt) {
	case KEY_MGMT_802_1X:
	case KEY_MGMT_802_1X_NO_WPA:
		return IW_AUTH_KEY_MGMT_802_1X;
	case KEY_MGMT_PSK:
		return IW_AUTH_KEY_MGMT_PSK;
	default:
		return 0;
	}
}


static int
wpa_driver_nl80211_auth_alg_fallback(struct wpa_driver_nl80211_data *drv,
				  struct wpa_driver_associate_params *params)
{
	struct iwreq iwr;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "WEXT: Driver did not support "
		   "SIOCSIWAUTH for AUTH_ALG, trying SIOCSIWENCODE");

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	/* Just changing mode, not actual keys */
	iwr.u.encoding.flags = 0;
	iwr.u.encoding.pointer = (caddr_t) NULL;
	iwr.u.encoding.length = 0;

	/*
	 * Note: IW_ENCODE_{OPEN,RESTRICTED} can be interpreted to mean two
	 * different things. Here they are used to indicate Open System vs.
	 * Shared Key authentication algorithm. However, some drivers may use
	 * them to select between open/restricted WEP encrypted (open = allow
	 * both unencrypted and encrypted frames; restricted = only allow
	 * encrypted frames).
	 */

	if (!drv->use_crypt) {
		iwr.u.encoding.flags |= IW_ENCODE_DISABLED;
	} else {
		if (params->auth_alg & AUTH_ALG_OPEN_SYSTEM)
			iwr.u.encoding.flags |= IW_ENCODE_OPEN;
		if (params->auth_alg & AUTH_ALG_SHARED_KEY)
			iwr.u.encoding.flags |= IW_ENCODE_RESTRICTED;
	}

	if (ioctl(drv->ioctl_sock, SIOCSIWENCODE, &iwr) < 0) {
		perror("ioctl[SIOCSIWENCODE]");
		ret = -1;
	}

	return ret;
}


static int wpa_driver_nl80211_associate(
	void *priv, struct wpa_driver_associate_params *params)
{
	struct wpa_driver_nl80211_data *drv = priv;
	int ret = 0;
	int allow_unencrypted_eapol;
	int value;

	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);

	/*
	 * If the driver did not support SIOCSIWAUTH, fallback to
	 * SIOCSIWENCODE here.
	 */
	if (drv->auth_alg_fallback &&
	    wpa_driver_nl80211_auth_alg_fallback(drv, params) < 0)
		ret = -1;

	if (!params->bssid &&
	    wpa_driver_nl80211_set_bssid(drv, NULL) < 0)
		ret = -1;

	/* TODO: should consider getting wpa version and cipher/key_mgmt suites
	 * from configuration, not from here, where only the selected suite is
	 * available */
	if (wpa_driver_nl80211_set_gen_ie(drv, params->wpa_ie, params->wpa_ie_len)
	    < 0)
		ret = -1;
	if (params->wpa_ie == NULL || params->wpa_ie_len == 0)
		value = IW_AUTH_WPA_VERSION_DISABLED;
	else if (params->wpa_ie[0] == WLAN_EID_RSN)
		value = IW_AUTH_WPA_VERSION_WPA2;
	else
		value = IW_AUTH_WPA_VERSION_WPA;
	if (wpa_driver_nl80211_set_auth_param(drv,
					   IW_AUTH_WPA_VERSION, value) < 0)
		ret = -1;
	value = wpa_driver_nl80211_cipher2wext(params->pairwise_suite);
	if (wpa_driver_nl80211_set_auth_param(drv,
					   IW_AUTH_CIPHER_PAIRWISE, value) < 0)
		ret = -1;
	value = wpa_driver_nl80211_cipher2wext(params->group_suite);
	if (wpa_driver_nl80211_set_auth_param(drv,
					   IW_AUTH_CIPHER_GROUP, value) < 0)
		ret = -1;
	value = wpa_driver_nl80211_keymgmt2wext(params->key_mgmt_suite);
	if (wpa_driver_nl80211_set_auth_param(drv,
					   IW_AUTH_KEY_MGMT, value) < 0)
		ret = -1;
	value = params->key_mgmt_suite != KEY_MGMT_NONE ||
		params->pairwise_suite != CIPHER_NONE ||
		params->group_suite != CIPHER_NONE ||
		params->wpa_ie_len;
	if (wpa_driver_nl80211_set_auth_param(drv,
					   IW_AUTH_PRIVACY_INVOKED, value) < 0)
		ret = -1;

	/* Allow unencrypted EAPOL messages even if pairwise keys are set when
	 * not using WPA. IEEE 802.1X specifies that these frames are not
	 * encrypted, but WPA encrypts them when pairwise keys are in use. */
	if (params->key_mgmt_suite == KEY_MGMT_802_1X ||
	    params->key_mgmt_suite == KEY_MGMT_PSK)
		allow_unencrypted_eapol = 0;
	else
		allow_unencrypted_eapol = 1;
	
	if (wpa_driver_nl80211_set_auth_param(drv,
					   IW_AUTH_RX_UNENCRYPTED_EAPOL,
					   allow_unencrypted_eapol) < 0)
		ret = -1;
	if (params->freq && wpa_driver_nl80211_set_freq(drv, params->freq) < 0)
		ret = -1;
	if (wpa_driver_nl80211_set_ssid(drv, params->ssid, params->ssid_len) < 0)
		ret = -1;
	if (params->bssid &&
	    wpa_driver_nl80211_set_bssid(drv, params->bssid) < 0)
		ret = -1;

	return ret;
}


static int wpa_driver_nl80211_set_auth_alg(void *priv, int auth_alg)
{
	struct wpa_driver_nl80211_data *drv = priv;
	int algs = 0, res;

	if (auth_alg & AUTH_ALG_OPEN_SYSTEM)
		algs |= IW_AUTH_ALG_OPEN_SYSTEM;
	if (auth_alg & AUTH_ALG_SHARED_KEY)
		algs |= IW_AUTH_ALG_SHARED_KEY;
	if (auth_alg & AUTH_ALG_LEAP)
		algs |= IW_AUTH_ALG_LEAP;
	if (algs == 0) {
		/* at least one algorithm should be set */
		algs = IW_AUTH_ALG_OPEN_SYSTEM;
	}

	res = wpa_driver_nl80211_set_auth_param(drv, IW_AUTH_80211_AUTH_ALG,
					     algs);
	drv->auth_alg_fallback = res == -2;
	return res;
}


/**
 * wpa_driver_nl80211_set_mode - Set wireless mode (infra/adhoc), SIOCSIWMODE
 * @priv: Pointer to private wext data from wpa_driver_nl80211_init()
 * @mode: 0 = infra/BSS (associate with an AP), 1 = adhoc/IBSS
 * Returns: 0 on success, -1 on failure
 */
static int wpa_driver_nl80211_set_mode(void *priv, int mode)
{
	struct wpa_driver_nl80211_data *drv = priv;
	int ret = -1, flags;
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -1;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_SET_INTERFACE, 0);
	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE,
		    mode ? NL80211_IFTYPE_ADHOC : NL80211_IFTYPE_STATION);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	if (!ret)
		return 0;
	else
		goto try_again;

nla_put_failure:
	wpa_printf(MSG_ERROR, "nl80211: Failed to set interface mode");
	return -1;

try_again:
	/* mac80211 doesn't allow mode changes while the device is up, so
	 * take the device down, try to set the mode again, and bring the
	 * device back up.
	 */
	if (wpa_driver_nl80211_get_ifflags(drv, &flags) == 0) {
		(void) wpa_driver_nl80211_set_ifflags(drv, flags & ~IFF_UP);

		/* Try to set the mode again while the interface is down */
		msg = nlmsg_alloc();
		if (!msg)
			return -1;

		genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
			    0, NL80211_CMD_SET_INTERFACE, 0);
		NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
		NLA_PUT_U32(msg, NL80211_ATTR_IFTYPE,
			    mode ? NL80211_IFTYPE_ADHOC :
			    NL80211_IFTYPE_STATION);
		ret = send_and_recv_msgs(drv, msg, NULL, NULL);
		if (ret) {
			wpa_printf(MSG_ERROR, "Failed to set interface %s "
				   "mode", drv->ifname);
		}

		/* Ignore return value of get_ifflags to ensure that the device
		 * is always up like it was before this function was called.
		 */
		(void) wpa_driver_nl80211_get_ifflags(drv, &flags);
		(void) wpa_driver_nl80211_set_ifflags(drv, flags | IFF_UP);
	}

	return ret;
}


static int wpa_driver_nl80211_pmksa(struct wpa_driver_nl80211_data *drv,
				 u32 cmd, const u8 *bssid, const u8 *pmkid)
{
	struct iwreq iwr;
	struct iw_pmksa pmksa;
	int ret = 0;

	os_memset(&iwr, 0, sizeof(iwr));
	os_strlcpy(iwr.ifr_name, drv->ifname, IFNAMSIZ);
	os_memset(&pmksa, 0, sizeof(pmksa));
	pmksa.cmd = cmd;
	pmksa.bssid.sa_family = ARPHRD_ETHER;
	if (bssid)
		os_memcpy(pmksa.bssid.sa_data, bssid, ETH_ALEN);
	if (pmkid)
		os_memcpy(pmksa.pmkid, pmkid, IW_PMKID_LEN);
	iwr.u.data.pointer = (caddr_t) &pmksa;
	iwr.u.data.length = sizeof(pmksa);

	if (ioctl(drv->ioctl_sock, SIOCSIWPMKSA, &iwr) < 0) {
		if (errno != EOPNOTSUPP)
			perror("ioctl[SIOCSIWPMKSA]");
		ret = -1;
	}

	return ret;
}


static int wpa_driver_nl80211_add_pmkid(void *priv, const u8 *bssid,
				     const u8 *pmkid)
{
	struct wpa_driver_nl80211_data *drv = priv;
	return wpa_driver_nl80211_pmksa(drv, IW_PMKSA_ADD, bssid, pmkid);
}


static int wpa_driver_nl80211_remove_pmkid(void *priv, const u8 *bssid,
		 			const u8 *pmkid)
{
	struct wpa_driver_nl80211_data *drv = priv;
	return wpa_driver_nl80211_pmksa(drv, IW_PMKSA_REMOVE, bssid, pmkid);
}


static int wpa_driver_nl80211_flush_pmkid(void *priv)
{
	struct wpa_driver_nl80211_data *drv = priv;
	return wpa_driver_nl80211_pmksa(drv, IW_PMKSA_FLUSH, NULL, NULL);
}


static int wpa_driver_nl80211_get_capa(void *priv,
				       struct wpa_driver_capa *capa)
{
	struct wpa_driver_nl80211_data *drv = priv;
	if (!drv->has_capability)
		return -1;
	os_memcpy(capa, &drv->capa, sizeof(*capa));
	return 0;
}


static int wpa_driver_nl80211_set_operstate(void *priv, int state)
{
	struct wpa_driver_nl80211_data *drv = priv;

	wpa_printf(MSG_DEBUG, "%s: operstate %d->%d (%s)",
		   __func__, drv->operstate, state, state ? "UP" : "DORMANT");
	drv->operstate = state;
	return wpa_driver_nl80211_send_oper_ifla(
		drv, -1, state ? IF_OPER_UP : IF_OPER_DORMANT);
}


#ifdef CONFIG_CLIENT_MLME
static int wpa_driver_nl80211_open_mlme(struct wpa_driver_nl80211_data *drv)
{
	if (wpa_driver_nl80211_set_userspace_mlme(drv, 1) < 0) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to enable userspace "
			   "MLME");
		return -1;
	}
	if (wpa_driver_nl80211_create_monitor_interface(drv)) {
		wpa_printf(MSG_ERROR, "nl80211: Failed to create monitor "
			   "interface");
		return -1;
	}
	return 0;
}
#endif /* CONFIG_CLIENT_MLME */


static int wpa_driver_nl80211_set_param(void *priv, const char *param)
{
#ifdef CONFIG_CLIENT_MLME
	struct wpa_driver_nl80211_data *drv = priv;

	if (param == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "%s: param='%s'", __func__, param);

	if (os_strstr(param, "use_mlme=1")) {
		wpa_printf(MSG_DEBUG, "nl80211: Using user space MLME");
		drv->capa.flags |= WPA_DRIVER_FLAGS_USER_SPACE_MLME;

		if (wpa_driver_nl80211_open_mlme(drv))
			return -1;
	}
#endif /* CONFIG_CLIENT_MLME */

	return 0;
}


#ifdef CONFIG_CLIENT_MLME

struct phy_info_arg {
	u16 *num_modes;
	struct wpa_hw_modes *modes;
};


static int phy_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct phy_info_arg *phy_info = arg;

	struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];

	struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
	static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1]
		= {
		[NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
	};

	struct nlattr *tb_rate[NL80211_BITRATE_ATTR_MAX + 1];
	static struct nla_policy rate_policy[NL80211_BITRATE_ATTR_MAX + 1] = {
		[NL80211_BITRATE_ATTR_RATE] = { .type = NLA_U32 },
		[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE] =
		{ .type = NLA_FLAG },
	};

	struct nlattr *nl_band;
	struct nlattr *nl_freq;
	struct nlattr *nl_rate;
	int rem_band, rem_freq, rem_rate;
	struct wpa_hw_modes *mode;
	int idx, mode_is_set;

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb_msg[NL80211_ATTR_WIPHY_BANDS])
		return NL_SKIP;

	nla_for_each_nested(nl_band, tb_msg[NL80211_ATTR_WIPHY_BANDS],
			    rem_band) {
		mode = os_realloc(phy_info->modes,
				  (*phy_info->num_modes + 1) * sizeof(*mode));
		if (!mode)
			return NL_SKIP;
		phy_info->modes = mode;

		mode_is_set = 0;

		mode = &phy_info->modes[*(phy_info->num_modes)];
		os_memset(mode, 0, sizeof(*mode));
		*(phy_info->num_modes) += 1;

		nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band),
			  nla_len(nl_band), NULL);

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS],
				    rem_freq) {
			nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX,
				  nla_data(nl_freq), nla_len(nl_freq),
				  freq_policy);
			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;
			mode->num_channels++;
		}

		mode->channels = os_zalloc(mode->num_channels *
					   sizeof(struct wpa_channel_data));
		if (!mode->channels)
			return NL_SKIP;

		idx = 0;

		nla_for_each_nested(nl_freq, tb_band[NL80211_BAND_ATTR_FREQS],
				    rem_freq) {
			nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX,
				  nla_data(nl_freq), nla_len(nl_freq),
				  freq_policy);
			if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
				continue;

			mode->channels[idx].freq = nla_get_u32(
				tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
			mode->channels[idx].flag |= WPA_CHAN_W_SCAN |
				WPA_CHAN_W_ACTIVE_SCAN |
				WPA_CHAN_W_IBSS;

			if (!mode_is_set) {
				/* crude heuristic */
				if (mode->channels[idx].freq < 4000)
					mode->mode = WPA_MODE_IEEE80211B;
				else
					mode->mode = WPA_MODE_IEEE80211A;
				mode_is_set = 1;
			}

			/* crude heuristic */
			if (mode->channels[idx].freq < 4000) {
				if (mode->channels[idx].freq == 2848)
					mode->channels[idx].chan = 14;
				else
					mode->channels[idx].chan =
						(mode->channels[idx].freq -
						 2407) / 5;
			} else
				mode->channels[idx].chan =
					mode->channels[idx].freq / 5 - 1000;

			if (tb_freq[NL80211_FREQUENCY_ATTR_DISABLED])
				mode->channels[idx].flag &= ~WPA_CHAN_W_SCAN;
			if (tb_freq[NL80211_FREQUENCY_ATTR_PASSIVE_SCAN])
				mode->channels[idx].flag &=
					~WPA_CHAN_W_ACTIVE_SCAN;
			if (tb_freq[NL80211_FREQUENCY_ATTR_NO_IBSS])
				mode->channels[idx].flag &= ~WPA_CHAN_W_IBSS;
			idx++;
		}

		nla_for_each_nested(nl_rate, tb_band[NL80211_BAND_ATTR_RATES],
				    rem_rate) {
			nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX,
				  nla_data(nl_rate), nla_len(nl_rate),
				  rate_policy);
			if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
				continue;
			mode->num_rates++;
		}

		mode->rates = os_zalloc(mode->num_rates *
					sizeof(struct wpa_rate_data));
		if (!mode->rates)
			return NL_SKIP;

		idx = 0;

		nla_for_each_nested(nl_rate, tb_band[NL80211_BAND_ATTR_RATES],
				    rem_rate) {
			nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX,
				  nla_data(nl_rate), nla_len(nl_rate),
				  rate_policy);
			if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
				continue;
			mode->rates[idx].rate = nla_get_u32(
				tb_rate[NL80211_BITRATE_ATTR_RATE]);

			/* crude heuristic */
			if (mode->mode == WPA_MODE_IEEE80211B &&
			    mode->rates[idx].rate > 200)
				mode->mode = WPA_MODE_IEEE80211G;

			if (tb_rate[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE])
				mode->rates[idx].flags |= WPA_RATE_PREAMBLE2;

			idx++;
		}
	}

	return NL_SKIP;
}


static struct wpa_hw_modes *
wpa_driver_nl80211_get_hw_feature_data(void *priv, u16 *num_modes, u16 *flags)
{
	struct wpa_driver_nl80211_data *drv = priv;
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

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_GET_WIPHY, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);

	if (send_and_recv_msgs(drv, msg, phy_info_handler, &result) == 0)
		return result.modes;
nla_put_failure:
	return NULL;
}


static int wpa_driver_nl80211_set_channel(void *priv, wpa_hw_mode phymode,
					  int chan, int freq)
{
	return wpa_driver_nl80211_set_freq(priv, freq);
}


static int wpa_driver_nl80211_send_mlme(void *priv, const u8 *data,
					size_t data_len)
{
	struct wpa_driver_nl80211_data *drv = priv;
	__u8 rtap_hdr[] = {
		0x00, 0x00, /* radiotap version */
		0x0e, 0x00, /* radiotap length */
		0x02, 0xc0, 0x00, 0x00, /* bmap: flags, tx and rx flags */
		0x0c,       /* F_WEP | F_FRAG (encrypt/fragment if required) */
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
			.iov_len = data_len,
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

	if (sendmsg(drv->monitor_sock, &msg, 0) < 0) {
		perror("send[MLME]");
		return -1;
	}

	return 0;
}


static int wpa_driver_nl80211_mlme_add_sta(void *priv, const u8 *addr,
					   const u8 *supp_rates,
					   size_t supp_rates_len)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_NEW_STATION, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);
	/* TODO: Get proper Association ID and listen interval */
	NLA_PUT_U16(msg, NL80211_ATTR_STA_AID, 1);
	NLA_PUT(msg, NL80211_ATTR_STA_SUPPORTED_RATES, supp_rates_len,
		supp_rates);
	NLA_PUT_U16(msg, NL80211_ATTR_STA_LISTEN_INTERVAL, 1);

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	/* ignore EEXIST, this happens if a STA associates while associated */
	if (ret == -EEXIST || ret >= 0)
		ret = 0;

nla_put_failure:
	return ret;
}


static int wpa_driver_nl80211_mlme_remove_sta(void *priv, const u8 *addr)
{
	struct wpa_driver_nl80211_data *drv = priv;
	struct nl_msg *msg;
	int ret = -1;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	genlmsg_put(msg, 0, 0, genl_family_get_id(drv->nl80211), 0,
		    0, NL80211_CMD_DEL_STATION, 0);

	NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, drv->ifindex);
	NLA_PUT(msg, NL80211_ATTR_MAC, ETH_ALEN, addr);

	ret = 0;

	ret = send_and_recv_msgs(drv, msg, NULL, NULL);
	return ret;

nla_put_failure:
	return -ENOBUFS;
}

#endif /* CONFIG_CLIENT_MLME */


const struct wpa_driver_ops wpa_driver_nl80211_ops = {
	.name = "nl80211",
	.desc = "Linux nl80211/cfg80211",
	.get_bssid = wpa_driver_nl80211_get_bssid,
	.get_ssid = wpa_driver_nl80211_get_ssid,
	.set_wpa = wpa_driver_nl80211_set_wpa,
	.set_key = wpa_driver_nl80211_set_key,
	.set_countermeasures = wpa_driver_nl80211_set_countermeasures,
	.set_drop_unencrypted = wpa_driver_nl80211_set_drop_unencrypted,
	.scan = wpa_driver_nl80211_scan,
	.get_scan_results2 = wpa_driver_nl80211_get_scan_results,
	.deauthenticate = wpa_driver_nl80211_deauthenticate,
	.disassociate = wpa_driver_nl80211_disassociate,
	.set_mode = wpa_driver_nl80211_set_mode,
	.associate = wpa_driver_nl80211_associate,
	.set_auth_alg = wpa_driver_nl80211_set_auth_alg,
	.init = wpa_driver_nl80211_init,
	.deinit = wpa_driver_nl80211_deinit,
	.set_param = wpa_driver_nl80211_set_param,
	.add_pmkid = wpa_driver_nl80211_add_pmkid,
	.remove_pmkid = wpa_driver_nl80211_remove_pmkid,
	.flush_pmkid = wpa_driver_nl80211_flush_pmkid,
	.get_capa = wpa_driver_nl80211_get_capa,
	.set_operstate = wpa_driver_nl80211_set_operstate,
	.set_country = wpa_driver_nl80211_set_country,
	.set_probe_req_ie = wpa_driver_nl80211_set_probe_req_ie,
#ifdef CONFIG_CLIENT_MLME
	.get_hw_feature_data = wpa_driver_nl80211_get_hw_feature_data,
	.set_channel = wpa_driver_nl80211_set_channel,
	.set_ssid = wpa_driver_nl80211_set_ssid,
	.set_bssid = wpa_driver_nl80211_set_bssid,
	.send_mlme = wpa_driver_nl80211_send_mlme,
	.mlme_add_sta = wpa_driver_nl80211_mlme_add_sta,
	.mlme_remove_sta = wpa_driver_nl80211_mlme_remove_sta,
#endif /* CONFIG_CLIENT_MLME */
};
