/*
 * wlantest control interface
 * Copyright (c) 2010-2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <sys/un.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/defs.h"
#include "common/version.h"
#include "common/ieee802_11_defs.h"
#include "wlantest.h"
#include "wlantest_ctrl.h"


static u8 * attr_get(u8 *buf, size_t buflen, enum wlantest_ctrl_attr attr,
		     size_t *len)
{
	u8 *pos = buf;

	while (pos + 8 <= buf + buflen) {
		enum wlantest_ctrl_attr a;
		size_t alen;
		a = WPA_GET_BE32(pos);
		pos += 4;
		alen = WPA_GET_BE32(pos);
		pos += 4;
		if (pos + alen > buf + buflen) {
			wpa_printf(MSG_DEBUG, "Invalid control message "
				   "attribute");
			return NULL;
		}
		if (a == attr) {
			*len = alen;
			return pos;
		}
		pos += alen;
	}

	return NULL;
}


static u8 * attr_get_macaddr(u8 *buf, size_t buflen,
			     enum wlantest_ctrl_attr attr)
{
	u8 *addr;
	size_t addr_len;
	addr = attr_get(buf, buflen, attr, &addr_len);
	if (addr && addr_len != ETH_ALEN)
		addr = NULL;
	return addr;
}


static int attr_get_int(u8 *buf, size_t buflen, enum wlantest_ctrl_attr attr)
{
	u8 *pos;
	size_t len;
	pos = attr_get(buf, buflen, attr, &len);
	if (pos == NULL || len != 4)
		return -1;
	return WPA_GET_BE32(pos);
}


static u8 * attr_add_str(u8 *pos, u8 *end, enum wlantest_ctrl_attr attr,
			 const char *str)
{
	size_t len = os_strlen(str);

	if (pos == NULL || end - pos < 8 + len)
		return NULL;
	WPA_PUT_BE32(pos, attr);
	pos += 4;
	WPA_PUT_BE32(pos, len);
	pos += 4;
	os_memcpy(pos, str, len);
	pos += len;
	return pos;
}


static u8 * attr_add_be32(u8 *pos, u8 *end, enum wlantest_ctrl_attr attr,
			  u32 val)
{
	if (pos == NULL || end - pos < 12)
		return NULL;
	WPA_PUT_BE32(pos, attr);
	pos += 4;
	WPA_PUT_BE32(pos, 4);
	pos += 4;
	WPA_PUT_BE32(pos, val);
	pos += 4;
	return pos;
}


static void ctrl_disconnect(struct wlantest *wt, int sock)
{
	int i;
	wpa_printf(MSG_DEBUG, "Disconnect control interface connection %d",
		   sock);
	for (i = 0; i < MAX_CTRL_CONNECTIONS; i++) {
		if (wt->ctrl_socks[i] == sock) {
			close(wt->ctrl_socks[i]);
			eloop_unregister_read_sock(wt->ctrl_socks[i]);
			wt->ctrl_socks[i] = -1;
			break;
		}
	}
}


static void ctrl_send(struct wlantest *wt, int sock, const u8 *buf,
		      size_t len)
{
	if (send(sock, buf, len, 0) < 0) {
		wpa_printf(MSG_INFO, "send(ctrl): %s", strerror(errno));
		ctrl_disconnect(wt, sock);
	}
}


static void ctrl_send_simple(struct wlantest *wt, int sock,
			     enum wlantest_ctrl_cmd cmd)
{
	u8 buf[4];
	WPA_PUT_BE32(buf, cmd);
	ctrl_send(wt, sock, buf, sizeof(buf));
}


static struct wlantest_bss * ctrl_get_bss(struct wlantest *wt, int sock,
					  u8 *cmd, size_t clen)
{
	struct wlantest_bss *bss;
	u8 *pos;
	size_t len;

	pos = attr_get(cmd, clen, WLANTEST_ATTR_BSSID, &len);
	if (pos == NULL || len != ETH_ALEN) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return NULL;
	}

	bss = bss_find(wt, pos);
	if (bss == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return NULL;
	}

	return bss;
}


static struct wlantest_sta * ctrl_get_sta(struct wlantest *wt, int sock,
					  u8 *cmd, size_t clen,
					  struct wlantest_bss *bss)
{
	struct wlantest_sta *sta;
	u8 *pos;
	size_t len;

	if (bss == NULL)
		return NULL;

	pos = attr_get(cmd, clen, WLANTEST_ATTR_STA_ADDR, &len);
	if (pos == NULL || len != ETH_ALEN) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return NULL;
	}

	sta = sta_find(bss, pos);
	if (sta == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return NULL;
	}

	return sta;
}


static struct wlantest_sta * ctrl_get_sta2(struct wlantest *wt, int sock,
					   u8 *cmd, size_t clen,
					   struct wlantest_bss *bss)
{
	struct wlantest_sta *sta;
	u8 *pos;
	size_t len;

	if (bss == NULL)
		return NULL;

	pos = attr_get(cmd, clen, WLANTEST_ATTR_STA2_ADDR, &len);
	if (pos == NULL || len != ETH_ALEN) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return NULL;
	}

	sta = sta_find(bss, pos);
	if (sta == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return NULL;
	}

	return sta;
}


static void ctrl_list_bss(struct wlantest *wt, int sock)
{
	u8 buf[WLANTEST_CTRL_MAX_RESP_LEN], *pos, *len;
	struct wlantest_bss *bss;

	pos = buf;
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	WPA_PUT_BE32(pos, WLANTEST_ATTR_BSSID);
	pos += 4;
	len = pos; /* to be filled */
	pos += 4;

	dl_list_for_each(bss, &wt->bss, struct wlantest_bss, list) {
		if (pos + ETH_ALEN > buf + WLANTEST_CTRL_MAX_RESP_LEN)
			break;
		os_memcpy(pos, bss->bssid, ETH_ALEN);
		pos += ETH_ALEN;
	}

	WPA_PUT_BE32(len, pos - len - 4);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_list_sta(struct wlantest *wt, int sock, u8 *cmd, size_t clen)
{
	u8 buf[WLANTEST_CTRL_MAX_RESP_LEN], *pos, *len;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	if (bss == NULL)
		return;

	pos = buf;
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	WPA_PUT_BE32(pos, WLANTEST_ATTR_STA_ADDR);
	pos += 4;
	len = pos; /* to be filled */
	pos += 4;

	dl_list_for_each(sta, &bss->sta, struct wlantest_sta, list) {
		if (pos + ETH_ALEN > buf + WLANTEST_CTRL_MAX_RESP_LEN)
			break;
		os_memcpy(pos, sta->addr, ETH_ALEN);
		pos += ETH_ALEN;
	}

	WPA_PUT_BE32(len, pos - len - 4);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_flush(struct wlantest *wt, int sock)
{
	wpa_printf(MSG_DEBUG, "Drop all collected BSS data");
	bss_flush(wt);
	ctrl_send_simple(wt, sock, WLANTEST_CTRL_SUCCESS);
}


static void ctrl_clear_sta_counters(struct wlantest *wt, int sock, u8 *cmd,
				    size_t clen)
{
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	sta = ctrl_get_sta(wt, sock, cmd, clen, bss);
	if (sta == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	os_memset(sta->counters, 0, sizeof(sta->counters));
	os_memset(sta->tx_tid, 0, sizeof(sta->tx_tid));
	os_memset(sta->rx_tid, 0, sizeof(sta->rx_tid));
	ctrl_send_simple(wt, sock, WLANTEST_CTRL_SUCCESS);
}


static void ctrl_clear_bss_counters(struct wlantest *wt, int sock, u8 *cmd,
				    size_t clen)
{
	struct wlantest_bss *bss;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	if (bss == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	os_memset(bss->counters, 0, sizeof(bss->counters));
	ctrl_send_simple(wt, sock, WLANTEST_CTRL_SUCCESS);
}


static void ctrl_clear_tdls_counters(struct wlantest *wt, int sock, u8 *cmd,
				     size_t clen)
{
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	struct wlantest_sta *sta2;
	struct wlantest_tdls *tdls;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	sta = ctrl_get_sta(wt, sock, cmd, clen, bss);
	sta2 = ctrl_get_sta2(wt, sock, cmd, clen, bss);
	if (sta == NULL || sta2 == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list) {
		if ((tdls->init == sta && tdls->resp == sta2) ||
		    (tdls->init == sta2 && tdls->resp == sta))
			os_memset(tdls->counters, 0, sizeof(tdls->counters));
	}
	ctrl_send_simple(wt, sock, WLANTEST_CTRL_SUCCESS);
}


static void ctrl_get_sta_counter(struct wlantest *wt, int sock, u8 *cmd,
				 size_t clen)
{
	u8 *addr;
	size_t addr_len;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u32 counter;
	u8 buf[4 + 12], *end, *pos;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	sta = ctrl_get_sta(wt, sock, cmd, clen, bss);
	if (sta == NULL)
		return;

	addr = attr_get(cmd, clen, WLANTEST_ATTR_STA_COUNTER, &addr_len);
	if (addr == NULL || addr_len != 4) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}
	counter = WPA_GET_BE32(addr);
	if (counter >= NUM_WLANTEST_STA_COUNTER) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_be32(pos, end, WLANTEST_ATTR_COUNTER,
			    sta->counters[counter]);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_get_bss_counter(struct wlantest *wt, int sock, u8 *cmd,
				 size_t clen)
{
	u8 *addr;
	size_t addr_len;
	struct wlantest_bss *bss;
	u32 counter;
	u8 buf[4 + 12], *end, *pos;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	if (bss == NULL)
		return;

	addr = attr_get(cmd, clen, WLANTEST_ATTR_BSS_COUNTER, &addr_len);
	if (addr == NULL || addr_len != 4) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}
	counter = WPA_GET_BE32(addr);
	if (counter >= NUM_WLANTEST_BSS_COUNTER) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_be32(pos, end, WLANTEST_ATTR_COUNTER,
			    bss->counters[counter]);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_get_tdls_counter(struct wlantest *wt, int sock, u8 *cmd,
				  size_t clen)
{
	u8 *addr;
	size_t addr_len;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	struct wlantest_sta *sta2;
	struct wlantest_tdls *tdls;
	u32 counter;
	u8 buf[4 + 12], *end, *pos;
	int found = 0;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	sta = ctrl_get_sta(wt, sock, cmd, clen, bss);
	sta2 = ctrl_get_sta2(wt, sock, cmd, clen, bss);
	if (sta == NULL || sta2 == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	addr = attr_get(cmd, clen, WLANTEST_ATTR_TDLS_COUNTER, &addr_len);
	if (addr == NULL || addr_len != 4) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}
	counter = WPA_GET_BE32(addr);
	if (counter >= NUM_WLANTEST_TDLS_COUNTER) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	dl_list_for_each(tdls, &bss->tdls, struct wlantest_tdls, list) {
		if (tdls->init == sta && tdls->resp == sta2) {
			found = 1;
			break;
		}
	}

	if (!found) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_be32(pos, end, WLANTEST_ATTR_COUNTER,
			    tdls->counters[counter]);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void build_mgmt_hdr(struct ieee80211_mgmt *mgmt,
			   struct wlantest_bss *bss, struct wlantest_sta *sta,
			   int sender_ap, int stype)
{
	os_memset(mgmt, 0, 24);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT, stype);
	if (sender_ap) {
		if (sta)
			os_memcpy(mgmt->da, sta->addr, ETH_ALEN);
		else
			os_memset(mgmt->da, 0xff, ETH_ALEN);
		os_memcpy(mgmt->sa, bss->bssid, ETH_ALEN);
	} else {
		os_memcpy(mgmt->da, bss->bssid, ETH_ALEN);
		os_memcpy(mgmt->sa, sta->addr, ETH_ALEN);
	}
	os_memcpy(mgmt->bssid, bss->bssid, ETH_ALEN);
}


static int ctrl_inject_auth(struct wlantest *wt, struct wlantest_bss *bss,
			    struct wlantest_sta *sta, int sender_ap,
			    enum wlantest_inject_protection prot)
{
	struct ieee80211_mgmt mgmt;

	if (prot != WLANTEST_INJECT_NORMAL &&
	    prot != WLANTEST_INJECT_UNPROTECTED)
		return -1; /* Authentication frame is never protected */
	if (sta == NULL)
		return -1; /* No broadcast Authentication frames */

	if (sender_ap)
		wpa_printf(MSG_INFO, "INJECT: Auth " MACSTR " -> " MACSTR,
			   MAC2STR(bss->bssid), MAC2STR(sta->addr));
	else
		wpa_printf(MSG_INFO, "INJECT: Auth " MACSTR " -> " MACSTR,
			   MAC2STR(sta->addr), MAC2STR(bss->bssid));
	build_mgmt_hdr(&mgmt, bss, sta, sender_ap, WLAN_FC_STYPE_AUTH);

	mgmt.u.auth.auth_alg = host_to_le16(WLAN_AUTH_OPEN);
	mgmt.u.auth.auth_transaction = host_to_le16(1);
	mgmt.u.auth.status_code = host_to_le16(WLAN_STATUS_SUCCESS);

	return wlantest_inject(wt, bss, sta, (u8 *) &mgmt, 24 + 6,
			       WLANTEST_INJECT_UNPROTECTED);
}


static int ctrl_inject_assocreq(struct wlantest *wt, struct wlantest_bss *bss,
				struct wlantest_sta *sta, int sender_ap,
				enum wlantest_inject_protection prot)
{
	u8 *buf;
	struct ieee80211_mgmt *mgmt;
	int ret;

	if (prot != WLANTEST_INJECT_NORMAL &&
	    prot != WLANTEST_INJECT_UNPROTECTED)
		return -1; /* Association Request frame is never protected */
	if (sta == NULL)
		return -1; /* No broadcast Association Request frames */
	if (sender_ap)
		return -1; /* No Association Request frame sent by AP */
	if (sta->assocreq_ies == NULL) {
		wpa_printf(MSG_INFO, "INJECT: No previous (Re)Association "
			   "Request available for " MACSTR,
			   MAC2STR(sta->addr));
		return -1;
	}

	wpa_printf(MSG_INFO, "INJECT: AssocReq " MACSTR " -> " MACSTR,
		   MAC2STR(sta->addr), MAC2STR(bss->bssid));
	buf = os_malloc(sizeof(*mgmt) + sta->assocreq_ies_len);
	if (buf == NULL)
		return -1;
	mgmt = (struct ieee80211_mgmt *) buf;

	build_mgmt_hdr(mgmt, bss, sta, sender_ap, WLAN_FC_STYPE_ASSOC_REQ);

	mgmt->u.assoc_req.capab_info = host_to_le16(sta->assocreq_capab_info);
	mgmt->u.assoc_req.listen_interval =
		host_to_le16(sta->assocreq_listen_int);
	os_memcpy(mgmt->u.assoc_req.variable, sta->assocreq_ies,
		  sta->assocreq_ies_len);

	ret = wlantest_inject(wt, bss, sta, buf,
			      24 + 4 + sta->assocreq_ies_len,
			      WLANTEST_INJECT_UNPROTECTED);
	os_free(buf);
	return ret;
}


static int ctrl_inject_reassocreq(struct wlantest *wt,
				  struct wlantest_bss *bss,
				  struct wlantest_sta *sta, int sender_ap,
				  enum wlantest_inject_protection prot)
{
	u8 *buf;
	struct ieee80211_mgmt *mgmt;
	int ret;

	if (prot != WLANTEST_INJECT_NORMAL &&
	    prot != WLANTEST_INJECT_UNPROTECTED)
		return -1; /* Reassociation Request frame is never protected */
	if (sta == NULL)
		return -1; /* No broadcast Reassociation Request frames */
	if (sender_ap)
		return -1; /* No Reassociation Request frame sent by AP */
	if (sta->assocreq_ies == NULL) {
		wpa_printf(MSG_INFO, "INJECT: No previous (Re)Association "
			   "Request available for " MACSTR,
			   MAC2STR(sta->addr));
		return -1;
	}

	wpa_printf(MSG_INFO, "INJECT: ReassocReq " MACSTR " -> " MACSTR,
		   MAC2STR(sta->addr), MAC2STR(bss->bssid));
	buf = os_malloc(sizeof(*mgmt) + sta->assocreq_ies_len);
	if (buf == NULL)
		return -1;
	mgmt = (struct ieee80211_mgmt *) buf;

	build_mgmt_hdr(mgmt, bss, sta, sender_ap, WLAN_FC_STYPE_REASSOC_REQ);

	mgmt->u.reassoc_req.capab_info =
		host_to_le16(sta->assocreq_capab_info);
	mgmt->u.reassoc_req.listen_interval =
		host_to_le16(sta->assocreq_listen_int);
	os_memcpy(mgmt->u.reassoc_req.current_ap, bss->bssid, ETH_ALEN);
	os_memcpy(mgmt->u.reassoc_req.variable, sta->assocreq_ies,
		  sta->assocreq_ies_len);

	ret = wlantest_inject(wt, bss, sta, buf,
			      24 + 10 + sta->assocreq_ies_len,
			      WLANTEST_INJECT_UNPROTECTED);
	os_free(buf);
	return ret;
}


static int ctrl_inject_deauth(struct wlantest *wt, struct wlantest_bss *bss,
			      struct wlantest_sta *sta, int sender_ap,
			      enum wlantest_inject_protection prot)
{
	struct ieee80211_mgmt mgmt;

	if (sender_ap) {
		if (sta)
			wpa_printf(MSG_INFO, "INJECT: Deauth " MACSTR " -> "
				   MACSTR,
				   MAC2STR(bss->bssid), MAC2STR(sta->addr));
		else
			wpa_printf(MSG_INFO, "INJECT: Deauth " MACSTR
				   " -> broadcast", MAC2STR(bss->bssid));
	} else
		wpa_printf(MSG_INFO, "INJECT: Deauth " MACSTR " -> " MACSTR,
			   MAC2STR(sta->addr), MAC2STR(bss->bssid));
	build_mgmt_hdr(&mgmt, bss, sta, sender_ap, WLAN_FC_STYPE_DEAUTH);

	mgmt.u.deauth.reason_code = host_to_le16(WLAN_REASON_UNSPECIFIED);

	return wlantest_inject(wt, bss, sta, (u8 *) &mgmt, 24 + 2, prot);
}


static int ctrl_inject_disassoc(struct wlantest *wt, struct wlantest_bss *bss,
				struct wlantest_sta *sta, int sender_ap,
				enum wlantest_inject_protection prot)
{
	struct ieee80211_mgmt mgmt;

	if (sender_ap) {
		if (sta)
			wpa_printf(MSG_INFO, "INJECT: Disassoc " MACSTR " -> "
				   MACSTR,
				   MAC2STR(bss->bssid), MAC2STR(sta->addr));
		else
			wpa_printf(MSG_INFO, "INJECT: Disassoc " MACSTR
				   " -> broadcast", MAC2STR(bss->bssid));
	} else
		wpa_printf(MSG_INFO, "INJECT: Disassoc " MACSTR " -> " MACSTR,
			   MAC2STR(sta->addr), MAC2STR(bss->bssid));
	build_mgmt_hdr(&mgmt, bss, sta, sender_ap, WLAN_FC_STYPE_DISASSOC);

	mgmt.u.disassoc.reason_code = host_to_le16(WLAN_REASON_UNSPECIFIED);

	return wlantest_inject(wt, bss, sta, (u8 *) &mgmt, 24 + 2, prot);
}


static int ctrl_inject_saqueryreq(struct wlantest *wt,
				  struct wlantest_bss *bss,
				  struct wlantest_sta *sta, int sender_ap,
				  enum wlantest_inject_protection prot)
{
	struct ieee80211_mgmt mgmt;

	if (sta == NULL)
		return -1; /* No broadcast SA Query frames */

	if (sender_ap)
		wpa_printf(MSG_INFO, "INJECT: SA Query Request " MACSTR " -> "
			   MACSTR, MAC2STR(bss->bssid), MAC2STR(sta->addr));
	else
		wpa_printf(MSG_INFO, "INJECT: SA Query Request " MACSTR " -> "
			   MACSTR, MAC2STR(sta->addr), MAC2STR(bss->bssid));
	build_mgmt_hdr(&mgmt, bss, sta, sender_ap, WLAN_FC_STYPE_ACTION);

	mgmt.u.action.category = WLAN_ACTION_SA_QUERY;
	mgmt.u.action.u.sa_query_req.action = WLAN_SA_QUERY_REQUEST;
	mgmt.u.action.u.sa_query_req.trans_id[0] = 0x12;
	mgmt.u.action.u.sa_query_req.trans_id[1] = 0x34;
	os_memcpy(sender_ap ? sta->ap_sa_query_tr : sta->sta_sa_query_tr,
		  mgmt.u.action.u.sa_query_req.trans_id,
		  WLAN_SA_QUERY_TR_ID_LEN);
	return wlantest_inject(wt, bss, sta, (u8 *) &mgmt, 24 + 4, prot);
}


static void ctrl_inject(struct wlantest *wt, int sock, u8 *cmd, size_t clen)
{
	u8 *bssid, *sta_addr;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	int frame, sender_ap, prot;
	int ret = 0;

	bssid = attr_get_macaddr(cmd, clen, WLANTEST_ATTR_BSSID);
	sta_addr = attr_get_macaddr(cmd, clen, WLANTEST_ATTR_STA_ADDR);
	frame = attr_get_int(cmd, clen, WLANTEST_ATTR_INJECT_FRAME);
	sender_ap = attr_get_int(cmd, clen, WLANTEST_ATTR_INJECT_SENDER_AP);
	if (sender_ap < 0)
		sender_ap = 0;
	prot = attr_get_int(cmd, clen, WLANTEST_ATTR_INJECT_PROTECTION);
	if (bssid == NULL || sta_addr == NULL || frame < 0 || prot < 0) {
		wpa_printf(MSG_INFO, "Invalid inject command parameters");
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	bss = bss_find(wt, bssid);
	if (bss == NULL) {
		wpa_printf(MSG_INFO, "BSS not found for inject command");
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	if (is_broadcast_ether_addr(sta_addr)) {
		if (!sender_ap) {
			wpa_printf(MSG_INFO, "Invalid broadcast inject "
				   "command without sender_ap set");
			ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
			return;
		} sta = NULL;
	} else {
		sta = sta_find(bss, sta_addr);
		if (sta == NULL) {
			wpa_printf(MSG_INFO, "Station not found for inject "
				   "command");
			ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
			return;
		}
	}

	switch (frame) {
	case WLANTEST_FRAME_AUTH:
		ret = ctrl_inject_auth(wt, bss, sta, sender_ap, prot);
		break;
	case WLANTEST_FRAME_ASSOCREQ:
		ret = ctrl_inject_assocreq(wt, bss, sta, sender_ap, prot);
		break;
	case WLANTEST_FRAME_REASSOCREQ:
		ret = ctrl_inject_reassocreq(wt, bss, sta, sender_ap, prot);
		break;
	case WLANTEST_FRAME_DEAUTH:
		ret = ctrl_inject_deauth(wt, bss, sta, sender_ap, prot);
		break;
	case WLANTEST_FRAME_DISASSOC:
		ret = ctrl_inject_disassoc(wt, bss, sta, sender_ap, prot);
		break;
	case WLANTEST_FRAME_SAQUERYREQ:
		ret = ctrl_inject_saqueryreq(wt, bss, sta, sender_ap, prot);
		break;
	default:
		wpa_printf(MSG_INFO, "Unsupported inject command frame %d",
			   frame);
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	if (ret)
		wpa_printf(MSG_INFO, "Failed to inject frame");
	else
		wpa_printf(MSG_INFO, "Frame injected successfully");
	ctrl_send_simple(wt, sock, ret == 0 ? WLANTEST_CTRL_SUCCESS :
			 WLANTEST_CTRL_FAILURE);
}


static void ctrl_version(struct wlantest *wt, int sock)
{
	u8 buf[WLANTEST_CTRL_MAX_RESP_LEN], *pos;

	pos = buf;
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_str(pos, buf + sizeof(buf), WLANTEST_ATTR_VERSION,
			   VERSION_STR);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_add_passphrase(struct wlantest *wt, int sock, u8 *cmd,
				size_t clen)
{
	u8 *passphrase;
	size_t len;
	struct wlantest_passphrase *p, *pa;
	u8 *bssid;

	passphrase = attr_get(cmd, clen, WLANTEST_ATTR_PASSPHRASE, &len);
	if (passphrase == NULL) {
		u8 *wepkey;
		char *key;
		enum wlantest_ctrl_cmd res;

		wepkey = attr_get(cmd, clen, WLANTEST_ATTR_WEPKEY, &len);
		if (wepkey == NULL) {
			ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
			return;
		}
		key = os_zalloc(len + 1);
		if (key == NULL) {
			ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
			return;
		}
		os_memcpy(key, wepkey, len);
		if (add_wep(wt, key) < 0)
			res = WLANTEST_CTRL_FAILURE;
		else
			res = WLANTEST_CTRL_SUCCESS;
		os_free(key);
		ctrl_send_simple(wt, sock, res);
		return;
	}

	if (len < 8 || len > 63) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	p = os_zalloc(sizeof(*p));
	if (p == NULL) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}
	os_memcpy(p->passphrase, passphrase, len);
	wpa_printf(MSG_INFO, "Add passphrase '%s'", p->passphrase);

	bssid = attr_get_macaddr(cmd, clen, WLANTEST_ATTR_BSSID);
	if (bssid) {
		os_memcpy(p->bssid, bssid, ETH_ALEN);
		wpa_printf(MSG_INFO, "Limit passphrase for BSSID " MACSTR,
			   MAC2STR(p->bssid));
	}

	dl_list_for_each(pa, &wt->passphrase, struct wlantest_passphrase, list)
	{
		if (os_strcmp(p->passphrase, pa->passphrase) == 0 &&
		    os_memcmp(p->bssid, pa->bssid, ETH_ALEN) == 0) {
			wpa_printf(MSG_INFO, "Passphrase was already known");
			os_free(p);
			p = NULL;
			break;
		}
	}

	if (p) {
		struct wlantest_bss *bss;
		dl_list_add(&wt->passphrase, &p->list);
		dl_list_for_each(bss, &wt->bss, struct wlantest_bss, list) {
			if (bssid &&
			    os_memcmp(p->bssid, bss->bssid, ETH_ALEN) != 0)
				continue;
			bss_add_pmk_from_passphrase(bss, p->passphrase);
		}
	}

	ctrl_send_simple(wt, sock, WLANTEST_CTRL_SUCCESS);
}


static void info_print_proto(char *buf, size_t len, int proto)
{
	char *pos, *end;

	if (proto == 0) {
		os_snprintf(buf, len, "OPEN");
		return;
	}

	pos = buf;
	end = buf + len;

	if (proto & WPA_PROTO_WPA)
		pos += os_snprintf(pos, end - pos, "%sWPA",
				   pos == buf ? "" : " ");
	if (proto & WPA_PROTO_RSN)
		pos += os_snprintf(pos, end - pos, "%sWPA2",
				   pos == buf ? "" : " ");
}


static void info_print_cipher(char *buf, size_t len, int cipher)
{
	char *pos, *end;

	if (cipher == 0) {
		os_snprintf(buf, len, "N/A");
		return;
	}

	pos = buf;
	end = buf + len;

	if (cipher & WPA_CIPHER_NONE)
		pos += os_snprintf(pos, end - pos, "%sNONE",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_WEP40)
		pos += os_snprintf(pos, end - pos, "%sWEP40",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_WEP104)
		pos += os_snprintf(pos, end - pos, "%sWEP104",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_TKIP)
		pos += os_snprintf(pos, end - pos, "%sTKIP",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_CCMP)
		pos += os_snprintf(pos, end - pos, "%sCCMP",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_AES_128_CMAC)
		pos += os_snprintf(pos, end - pos, "%sBIP",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_BIP_GMAC_128)
		pos += os_snprintf(pos, end - pos, "%sBIP-GMAC-128",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_BIP_GMAC_256)
		pos += os_snprintf(pos, end - pos, "%sBIP-GMAC-256",
				   pos == buf ? "" : " ");
	if (cipher & WPA_CIPHER_BIP_CMAC_256)
		pos += os_snprintf(pos, end - pos, "%sBIP-CMAC-256",
				   pos == buf ? "" : " ");
}


static void info_print_key_mgmt(char *buf, size_t len, int key_mgmt)
{
	char *pos, *end;

	if (key_mgmt == 0) {
		os_snprintf(buf, len, "N/A");
		return;
	}

	pos = buf;
	end = buf + len;

	if (key_mgmt & WPA_KEY_MGMT_IEEE8021X)
		pos += os_snprintf(pos, end - pos, "%sEAP",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_PSK)
		pos += os_snprintf(pos, end - pos, "%sPSK",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_WPA_NONE)
		pos += os_snprintf(pos, end - pos, "%sWPA-NONE",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X)
		pos += os_snprintf(pos, end - pos, "%sFT-EAP",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_FT_PSK)
		pos += os_snprintf(pos, end - pos, "%sFT-PSK",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256)
		pos += os_snprintf(pos, end - pos, "%sEAP-SHA256",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_PSK_SHA256)
		pos += os_snprintf(pos, end - pos, "%sPSK-SHA256",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B)
		pos += os_snprintf(pos, end - pos, "%sEAP-SUITE-B",
				   pos == buf ? "" : " ");
	if (key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
		pos += os_snprintf(pos, end - pos, "%sEAP-SUITE-B-192",
				   pos == buf ? "" : " ");
}


static void info_print_rsn_capab(char *buf, size_t len, int capab)
{
	char *pos, *end;

	pos = buf;
	end = buf + len;

	if (capab & WPA_CAPABILITY_PREAUTH)
		pos += os_snprintf(pos, end - pos, "%sPREAUTH",
				   pos == buf ? "" : " ");
	if (capab & WPA_CAPABILITY_NO_PAIRWISE)
		pos += os_snprintf(pos, end - pos, "%sNO_PAIRWISE",
				   pos == buf ? "" : " ");
	if (capab & WPA_CAPABILITY_MFPR)
		pos += os_snprintf(pos, end - pos, "%sMFPR",
				   pos == buf ? "" : " ");
	if (capab & WPA_CAPABILITY_MFPC)
		pos += os_snprintf(pos, end - pos, "%sMFPC",
				   pos == buf ? "" : " ");
	if (capab & WPA_CAPABILITY_PEERKEY_ENABLED)
		pos += os_snprintf(pos, end - pos, "%sPEERKEY",
				   pos == buf ? "" : " ");
	if (capab & WPA_CAPABILITY_OCVC)
		pos += os_snprintf(pos, end - pos, "%sOCVC",
				   pos == buf ? "" : " ");
}


static void info_print_state(char *buf, size_t len, int state)
{
	switch (state) {
	case STATE1:
		os_strlcpy(buf, "NOT-AUTH", len);
		break;
	case STATE2:
		os_strlcpy(buf, "AUTH", len);
		break;
	case STATE3:
		os_strlcpy(buf, "AUTH+ASSOC", len);
		break;
	}
}


static void info_print_gtk(char *buf, size_t len, struct wlantest_sta *sta)
{
	size_t pos;

	pos = os_snprintf(buf, len, "IDX=%d,GTK=", sta->gtk_idx);
	wpa_snprintf_hex(buf + pos, len - pos, sta->gtk, sta->gtk_len);
}


static void ctrl_info_sta(struct wlantest *wt, int sock, u8 *cmd, size_t clen)
{
	u8 *addr;
	size_t addr_len;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	enum wlantest_sta_info info;
	u8 buf[4 + 108], *end, *pos;
	char resp[100];

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	sta = ctrl_get_sta(wt, sock, cmd, clen, bss);
	if (sta == NULL)
		return;

	addr = attr_get(cmd, clen, WLANTEST_ATTR_STA_INFO, &addr_len);
	if (addr == NULL || addr_len != 4) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}
	info = WPA_GET_BE32(addr);

	resp[0] = '\0';
	switch (info) {
	case WLANTEST_STA_INFO_PROTO:
		info_print_proto(resp, sizeof(resp), sta->proto);
		break;
	case WLANTEST_STA_INFO_PAIRWISE:
		info_print_cipher(resp, sizeof(resp), sta->pairwise_cipher);
		break;
	case WLANTEST_STA_INFO_KEY_MGMT:
		info_print_key_mgmt(resp, sizeof(resp), sta->key_mgmt);
		break;
	case WLANTEST_STA_INFO_RSN_CAPAB:
		info_print_rsn_capab(resp, sizeof(resp), sta->rsn_capab);
		break;
	case WLANTEST_STA_INFO_STATE:
		info_print_state(resp, sizeof(resp), sta->state);
		break;
	case WLANTEST_STA_INFO_GTK:
		info_print_gtk(resp, sizeof(resp), sta);
		break;
	default:
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_str(pos, end, WLANTEST_ATTR_INFO, resp);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_info_bss(struct wlantest *wt, int sock, u8 *cmd, size_t clen)
{
	u8 *addr;
	size_t addr_len;
	struct wlantest_bss *bss;
	enum wlantest_bss_info info;
	u8 buf[4 + 108], *end, *pos;
	char resp[100];

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	if (bss == NULL)
		return;

	addr = attr_get(cmd, clen, WLANTEST_ATTR_BSS_INFO, &addr_len);
	if (addr == NULL || addr_len != 4) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}
	info = WPA_GET_BE32(addr);

	resp[0] = '\0';
	switch (info) {
	case WLANTEST_BSS_INFO_PROTO:
		info_print_proto(resp, sizeof(resp), bss->proto);
		break;
	case WLANTEST_BSS_INFO_PAIRWISE:
		info_print_cipher(resp, sizeof(resp), bss->pairwise_cipher);
		break;
	case WLANTEST_BSS_INFO_GROUP:
		info_print_cipher(resp, sizeof(resp), bss->group_cipher);
		break;
	case WLANTEST_BSS_INFO_GROUP_MGMT:
		info_print_cipher(resp, sizeof(resp), bss->mgmt_group_cipher);
		break;
	case WLANTEST_BSS_INFO_KEY_MGMT:
		info_print_key_mgmt(resp, sizeof(resp), bss->key_mgmt);
		break;
	case WLANTEST_BSS_INFO_RSN_CAPAB:
		info_print_rsn_capab(resp, sizeof(resp), bss->rsn_capab);
		break;
	default:
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_str(pos, end, WLANTEST_ATTR_INFO, resp);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_send_(struct wlantest *wt, int sock, u8 *cmd, size_t clen)
{
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u8 *bssid, *sta_addr;
	int prot;
	u8 *frame;
	size_t frame_len;
	int ret = 0;
	struct ieee80211_hdr *hdr;
	u16 fc;

	frame = attr_get(cmd, clen, WLANTEST_ATTR_FRAME, &frame_len);
	prot = attr_get_int(cmd, clen, WLANTEST_ATTR_INJECT_PROTECTION);
	if (frame == NULL || frame_len < 24 || prot < 0) {
		wpa_printf(MSG_INFO, "Invalid send command parameters");
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	hdr = (struct ieee80211_hdr *) frame;
	fc = le_to_host16(hdr->frame_control);
	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		bssid = hdr->addr3;
		if (os_memcmp(hdr->addr2, hdr->addr3, ETH_ALEN) == 0)
			sta_addr = hdr->addr1;
		else
			sta_addr = hdr->addr2;
		break;
	case WLAN_FC_TYPE_DATA:
		switch (fc & (WLAN_FC_TODS | WLAN_FC_FROMDS)) {
		case 0:
			bssid = hdr->addr3;
			sta_addr = hdr->addr2;
			break;
		case WLAN_FC_TODS:
			bssid = hdr->addr1;
			sta_addr = hdr->addr2;
			break;
		case WLAN_FC_FROMDS:
			bssid = hdr->addr2;
			sta_addr = hdr->addr1;
			break;
		default:
			wpa_printf(MSG_INFO, "Unsupported inject frame");
			ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
			return;
		}
		break;
	default:
		wpa_printf(MSG_INFO, "Unsupported inject frame");
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	bss = bss_find(wt, bssid);
	if (bss == NULL && prot != WLANTEST_INJECT_UNPROTECTED) {
		wpa_printf(MSG_INFO, "Unknown BSSID");
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	if (bss)
		sta = sta_find(bss, sta_addr);
	else
		sta = NULL;
	if (sta == NULL && prot != WLANTEST_INJECT_UNPROTECTED) {
		wpa_printf(MSG_INFO, "Unknown STA address");
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_FAILURE);
		return;
	}

	ret = wlantest_inject(wt, bss, sta, frame, frame_len, prot);

	if (ret)
		wpa_printf(MSG_INFO, "Failed to inject frame");
	else
		wpa_printf(MSG_INFO, "Frame injected successfully");
	ctrl_send_simple(wt, sock, ret == 0 ? WLANTEST_CTRL_SUCCESS :
			 WLANTEST_CTRL_FAILURE);
}


static void ctrl_relog(struct wlantest *wt, int sock)
{
	int res = wlantest_relog(wt);
	ctrl_send_simple(wt, sock, res ? WLANTEST_CTRL_FAILURE :
			 WLANTEST_CTRL_SUCCESS);
}


static void ctrl_get_tx_tid(struct wlantest *wt, int sock, u8 *cmd, size_t clen)
{
	u8 *addr;
	size_t addr_len;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u32 counter;
	u8 buf[4 + 12], *end, *pos;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	sta = ctrl_get_sta(wt, sock, cmd, clen, bss);
	if (sta == NULL)
		return;

	addr = attr_get(cmd, clen, WLANTEST_ATTR_TID, &addr_len);
	if (addr == NULL || addr_len != 4) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}
	counter = WPA_GET_BE32(addr);
	if (counter >= 16 + 1) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_be32(pos, end, WLANTEST_ATTR_COUNTER,
			    sta->tx_tid[counter]);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_get_rx_tid(struct wlantest *wt, int sock, u8 *cmd, size_t clen)
{
	u8 *addr;
	size_t addr_len;
	struct wlantest_bss *bss;
	struct wlantest_sta *sta;
	u32 counter;
	u8 buf[4 + 12], *end, *pos;

	bss = ctrl_get_bss(wt, sock, cmd, clen);
	sta = ctrl_get_sta(wt, sock, cmd, clen, bss);
	if (sta == NULL)
		return;

	addr = attr_get(cmd, clen, WLANTEST_ATTR_TID, &addr_len);
	if (addr == NULL || addr_len != 4) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}
	counter = WPA_GET_BE32(addr);
	if (counter >= 16 + 1) {
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_INVALID_CMD);
		return;
	}

	pos = buf;
	end = buf + sizeof(buf);
	WPA_PUT_BE32(pos, WLANTEST_CTRL_SUCCESS);
	pos += 4;
	pos = attr_add_be32(pos, end, WLANTEST_ATTR_COUNTER,
			    sta->rx_tid[counter]);
	ctrl_send(wt, sock, buf, pos - buf);
}


static void ctrl_read(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wlantest *wt = eloop_ctx;
	u8 buf[WLANTEST_CTRL_MAX_CMD_LEN];
	int len;
	enum wlantest_ctrl_cmd cmd;

	wpa_printf(MSG_EXCESSIVE, "New control interface message from %d",
		   sock);
	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		wpa_printf(MSG_INFO, "recv(ctrl): %s", strerror(errno));
		ctrl_disconnect(wt, sock);
		return;
	}
	if (len == 0) {
		ctrl_disconnect(wt, sock);
		return;
	}

	if (len < 4) {
		wpa_printf(MSG_INFO, "Too short control interface command "
			   "from %d", sock);
		ctrl_disconnect(wt, sock);
		return;
	}
	cmd = WPA_GET_BE32(buf);
	wpa_printf(MSG_EXCESSIVE, "Control interface command %d from %d",
		   cmd, sock);

	switch (cmd) {
	case WLANTEST_CTRL_PING:
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_SUCCESS);
		break;
	case WLANTEST_CTRL_TERMINATE:
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_SUCCESS);
		eloop_terminate();
		break;
	case WLANTEST_CTRL_LIST_BSS:
		ctrl_list_bss(wt, sock);
		break;
	case WLANTEST_CTRL_LIST_STA:
		ctrl_list_sta(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_FLUSH:
		ctrl_flush(wt, sock);
		break;
	case WLANTEST_CTRL_CLEAR_STA_COUNTERS:
		ctrl_clear_sta_counters(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_CLEAR_BSS_COUNTERS:
		ctrl_clear_bss_counters(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_CLEAR_TDLS_COUNTERS:
		ctrl_clear_tdls_counters(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_GET_STA_COUNTER:
		ctrl_get_sta_counter(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_GET_BSS_COUNTER:
		ctrl_get_bss_counter(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_GET_TDLS_COUNTER:
		ctrl_get_tdls_counter(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_INJECT:
		ctrl_inject(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_VERSION:
		ctrl_version(wt, sock);
		break;
	case WLANTEST_CTRL_ADD_PASSPHRASE:
		ctrl_add_passphrase(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_INFO_STA:
		ctrl_info_sta(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_INFO_BSS:
		ctrl_info_bss(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_SEND:
		ctrl_send_(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_RELOG:
		ctrl_relog(wt, sock);
		break;
	case WLANTEST_CTRL_GET_TX_TID:
		ctrl_get_tx_tid(wt, sock, buf + 4, len - 4);
		break;
	case WLANTEST_CTRL_GET_RX_TID:
		ctrl_get_rx_tid(wt, sock, buf + 4, len - 4);
		break;
	default:
		ctrl_send_simple(wt, sock, WLANTEST_CTRL_UNKNOWN_CMD);
		break;
	}
}


static void ctrl_connect(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wlantest *wt = eloop_ctx;
	int conn, i;

	conn = accept(sock, NULL, NULL);
	if (conn < 0) {
		wpa_printf(MSG_INFO, "accept(ctrl): %s", strerror(errno));
		return;
	}
	wpa_printf(MSG_MSGDUMP, "New control interface connection %d", conn);

	for (i = 0; i < MAX_CTRL_CONNECTIONS; i++) {
		if (wt->ctrl_socks[i] < 0)
			break;
	}

	if (i == MAX_CTRL_CONNECTIONS) {
		wpa_printf(MSG_INFO, "No room for new control connection");
		close(conn);
		return;
	}

	wt->ctrl_socks[i] = conn;
	eloop_register_read_sock(conn, ctrl_read, wt, NULL);
}


int ctrl_init(struct wlantest *wt)
{
	struct sockaddr_un addr;

	wt->ctrl_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (wt->ctrl_sock < 0) {
		wpa_printf(MSG_ERROR, "socket: %s", strerror(errno));
		return -1;
	}

	os_memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	os_strlcpy(addr.sun_path + 1, WLANTEST_SOCK_NAME,
		   sizeof(addr.sun_path) - 1);
	if (bind(wt->ctrl_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_ERROR, "bind: %s", strerror(errno));
		close(wt->ctrl_sock);
		wt->ctrl_sock = -1;
		return -1;
	}

	if (listen(wt->ctrl_sock, 5) < 0) {
		wpa_printf(MSG_ERROR, "listen: %s", strerror(errno));
		close(wt->ctrl_sock);
		wt->ctrl_sock = -1;
		return -1;
	}

	if (eloop_register_read_sock(wt->ctrl_sock, ctrl_connect, wt, NULL)) {
		close(wt->ctrl_sock);
		wt->ctrl_sock = -1;
		return -1;
	}

	return 0;
}


void ctrl_deinit(struct wlantest *wt)
{
	int i;

	if (wt->ctrl_sock < 0)
		return;

	for (i = 0; i < MAX_CTRL_CONNECTIONS; i++) {
		if (wt->ctrl_socks[i] >= 0) {
			close(wt->ctrl_socks[i]);
			eloop_unregister_read_sock(wt->ctrl_socks[i]);
			wt->ctrl_socks[i] = -1;
		}
	}

	eloop_unregister_read_sock(wt->ctrl_sock);
	close(wt->ctrl_sock);
	wt->ctrl_sock = -1;
}
