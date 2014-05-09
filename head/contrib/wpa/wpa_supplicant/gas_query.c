/*
 * Generic advertisement service (GAS) query
 * Copyright (c) 2009, Atheros Communications
 * Copyright (c) 2011, Qualcomm Atheros
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/gas.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "offchannel.h"
#include "gas_query.h"


/** GAS query timeout in seconds */
#define GAS_QUERY_TIMEOUT_PERIOD 5


/**
 * struct gas_query_pending - Pending GAS query
 */
struct gas_query_pending {
	struct dl_list list;
	u8 addr[ETH_ALEN];
	u8 dialog_token;
	u8 next_frag_id;
	unsigned int wait_comeback:1;
	unsigned int offchannel_tx_started:1;
	int freq;
	u16 status_code;
	struct wpabuf *adv_proto;
	struct wpabuf *resp;
	void (*cb)(void *ctx, const u8 *dst, u8 dialog_token,
		   enum gas_query_result result,
		   const struct wpabuf *adv_proto,
		   const struct wpabuf *resp, u16 status_code);
	void *ctx;
};

/**
 * struct gas_query - Internal GAS query data
 */
struct gas_query {
	struct wpa_supplicant *wpa_s;
	struct dl_list pending; /* struct gas_query_pending */
};


static void gas_query_tx_comeback_timeout(void *eloop_data, void *user_ctx);
static void gas_query_timeout(void *eloop_data, void *user_ctx);


/**
 * gas_query_init - Initialize GAS query component
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: Pointer to GAS query data or %NULL on failure
 */
struct gas_query * gas_query_init(struct wpa_supplicant *wpa_s)
{
	struct gas_query *gas;

	gas = os_zalloc(sizeof(*gas));
	if (gas == NULL)
		return NULL;

	gas->wpa_s = wpa_s;
	dl_list_init(&gas->pending);

	return gas;
}


static void gas_query_done(struct gas_query *gas,
			   struct gas_query_pending *query,
			   enum gas_query_result result)
{
	if (query->offchannel_tx_started)
		offchannel_send_action_done(gas->wpa_s);
	eloop_cancel_timeout(gas_query_tx_comeback_timeout, gas, query);
	eloop_cancel_timeout(gas_query_timeout, gas, query);
	dl_list_del(&query->list);
	query->cb(query->ctx, query->addr, query->dialog_token, result,
		  query->adv_proto, query->resp, query->status_code);
	wpabuf_free(query->adv_proto);
	wpabuf_free(query->resp);
	os_free(query);
}


/**
 * gas_query_deinit - Deinitialize GAS query component
 * @gas: GAS query data from gas_query_init()
 */
void gas_query_deinit(struct gas_query *gas)
{
	struct gas_query_pending *query, *next;

	if (gas == NULL)
		return;

	dl_list_for_each_safe(query, next, &gas->pending,
			      struct gas_query_pending, list)
		gas_query_done(gas, query, GAS_QUERY_DELETED_AT_DEINIT);

	os_free(gas);
}


static struct gas_query_pending *
gas_query_get_pending(struct gas_query *gas, const u8 *addr, u8 dialog_token)
{
	struct gas_query_pending *q;
	dl_list_for_each(q, &gas->pending, struct gas_query_pending, list) {
		if (os_memcmp(q->addr, addr, ETH_ALEN) == 0 &&
		    q->dialog_token == dialog_token)
			return q;
	}
	return NULL;
}


static int gas_query_append(struct gas_query_pending *query, const u8 *data,
			    size_t len)
{
	if (wpabuf_resize(&query->resp, len) < 0) {
		wpa_printf(MSG_DEBUG, "GAS: No memory to store the response");
		return -1;
	}
	wpabuf_put_data(query->resp, data, len);
	return 0;
}


static int gas_query_tx(struct gas_query *gas, struct gas_query_pending *query,
			struct wpabuf *req)
{
	int res;
	wpa_printf(MSG_DEBUG, "GAS: Send action frame to " MACSTR " len=%u "
		   "freq=%d", MAC2STR(query->addr),
		   (unsigned int) wpabuf_len(req), query->freq);
	res = offchannel_send_action(gas->wpa_s, query->freq, query->addr,
				     gas->wpa_s->own_addr, query->addr,
				     wpabuf_head(req), wpabuf_len(req), 1000,
				     NULL, 0);
	if (res == 0)
		query->offchannel_tx_started = 1;
	return res;
}


static void gas_query_tx_comeback_req(struct gas_query *gas,
				      struct gas_query_pending *query)
{
	struct wpabuf *req;

	req = gas_build_comeback_req(query->dialog_token);
	if (req == NULL) {
		gas_query_done(gas, query, GAS_QUERY_INTERNAL_ERROR);
		return;
	}

	if (gas_query_tx(gas, query, req) < 0) {
		wpa_printf(MSG_DEBUG, "GAS: Failed to send Action frame to "
			   MACSTR, MAC2STR(query->addr));
		gas_query_done(gas, query, GAS_QUERY_INTERNAL_ERROR);
	}

	wpabuf_free(req);
}


static void gas_query_tx_comeback_timeout(void *eloop_data, void *user_ctx)
{
	struct gas_query *gas = eloop_data;
	struct gas_query_pending *query = user_ctx;

	wpa_printf(MSG_DEBUG, "GAS: Comeback timeout for request to " MACSTR,
		   MAC2STR(query->addr));
	gas_query_tx_comeback_req(gas, query);
}


static void gas_query_tx_comeback_req_delay(struct gas_query *gas,
					    struct gas_query_pending *query,
					    u16 comeback_delay)
{
	unsigned int secs, usecs;

	secs = (comeback_delay * 1024) / 1000000;
	usecs = comeback_delay * 1024 - secs * 1000000;
	wpa_printf(MSG_DEBUG, "GAS: Send comeback request to " MACSTR
		   " in %u secs %u usecs", MAC2STR(query->addr), secs, usecs);
	eloop_cancel_timeout(gas_query_tx_comeback_timeout, gas, query);
	eloop_register_timeout(secs, usecs, gas_query_tx_comeback_timeout,
			       gas, query);
}


static void gas_query_rx_initial(struct gas_query *gas,
				 struct gas_query_pending *query,
				 const u8 *adv_proto, const u8 *resp,
				 size_t len, u16 comeback_delay)
{
	wpa_printf(MSG_DEBUG, "GAS: Received initial response from "
		   MACSTR " (dialog_token=%u comeback_delay=%u)",
		   MAC2STR(query->addr), query->dialog_token, comeback_delay);

	query->adv_proto = wpabuf_alloc_copy(adv_proto, 2 + adv_proto[1]);
	if (query->adv_proto == NULL) {
		gas_query_done(gas, query, GAS_QUERY_INTERNAL_ERROR);
		return;
	}

	if (comeback_delay) {
		query->wait_comeback = 1;
		gas_query_tx_comeback_req_delay(gas, query, comeback_delay);
		return;
	}

	/* Query was completed without comeback mechanism */
	if (gas_query_append(query, resp, len) < 0) {
		gas_query_done(gas, query, GAS_QUERY_INTERNAL_ERROR);
		return;
	}

	gas_query_done(gas, query, GAS_QUERY_SUCCESS);
}


static void gas_query_rx_comeback(struct gas_query *gas,
				  struct gas_query_pending *query,
				  const u8 *adv_proto, const u8 *resp,
				  size_t len, u8 frag_id, u8 more_frags,
				  u16 comeback_delay)
{
	wpa_printf(MSG_DEBUG, "GAS: Received comeback response from "
		   MACSTR " (dialog_token=%u frag_id=%u more_frags=%u "
		   "comeback_delay=%u)",
		   MAC2STR(query->addr), query->dialog_token, frag_id,
		   more_frags, comeback_delay);

	if ((size_t) 2 + adv_proto[1] != wpabuf_len(query->adv_proto) ||
	    os_memcmp(adv_proto, wpabuf_head(query->adv_proto),
		      wpabuf_len(query->adv_proto)) != 0) {
		wpa_printf(MSG_DEBUG, "GAS: Advertisement Protocol changed "
			   "between initial and comeback response from "
			   MACSTR, MAC2STR(query->addr));
		gas_query_done(gas, query, GAS_QUERY_PEER_ERROR);
		return;
	}

	if (comeback_delay) {
		if (frag_id) {
			wpa_printf(MSG_DEBUG, "GAS: Invalid comeback response "
				   "with non-zero frag_id and comeback_delay "
				   "from " MACSTR, MAC2STR(query->addr));
			gas_query_done(gas, query, GAS_QUERY_PEER_ERROR);
			return;
		}
		gas_query_tx_comeback_req_delay(gas, query, comeback_delay);
		return;
	}

	if (frag_id != query->next_frag_id) {
		wpa_printf(MSG_DEBUG, "GAS: Unexpected frag_id in response "
			   "from " MACSTR, MAC2STR(query->addr));
		gas_query_done(gas, query, GAS_QUERY_PEER_ERROR);
		return;
	}
	query->next_frag_id++;

	if (gas_query_append(query, resp, len) < 0) {
		gas_query_done(gas, query, GAS_QUERY_INTERNAL_ERROR);
		return;
	}

	if (more_frags) {
		gas_query_tx_comeback_req(gas, query);
		return;
	}

	gas_query_done(gas, query, GAS_QUERY_SUCCESS);
}


/**
 * gas_query_rx - Indicate reception of a Public Action frame
 * @gas: GAS query data from gas_query_init()
 * @da: Destination MAC address of the Action frame
 * @sa: Source MAC address of the Action frame
 * @bssid: BSSID of the Action frame
 * @data: Payload of the Action frame
 * @len: Length of @data
 * @freq: Frequency (in MHz) on which the frame was received
 * Returns: 0 if the Public Action frame was a GAS frame or -1 if not
 */
int gas_query_rx(struct gas_query *gas, const u8 *da, const u8 *sa,
		 const u8 *bssid, const u8 *data, size_t len, int freq)
{
	struct gas_query_pending *query;
	u8 action, dialog_token, frag_id = 0, more_frags = 0;
	u16 comeback_delay, resp_len;
	const u8 *pos, *adv_proto;

	if (gas == NULL || len < 4)
		return -1;

	pos = data;
	action = *pos++;
	dialog_token = *pos++;

	if (action != WLAN_PA_GAS_INITIAL_RESP &&
	    action != WLAN_PA_GAS_COMEBACK_RESP)
		return -1; /* Not a GAS response */

	query = gas_query_get_pending(gas, sa, dialog_token);
	if (query == NULL) {
		wpa_printf(MSG_DEBUG, "GAS: No pending query found for " MACSTR
			   " dialog token %u", MAC2STR(sa), dialog_token);
		return -1;
	}

	if (query->wait_comeback && action == WLAN_PA_GAS_INITIAL_RESP) {
		wpa_printf(MSG_DEBUG, "GAS: Unexpected initial response from "
			   MACSTR " dialog token %u when waiting for comeback "
			   "response", MAC2STR(sa), dialog_token);
		return 0;
	}

	if (!query->wait_comeback && action == WLAN_PA_GAS_COMEBACK_RESP) {
		wpa_printf(MSG_DEBUG, "GAS: Unexpected comeback response from "
			   MACSTR " dialog token %u when waiting for initial "
			   "response", MAC2STR(sa), dialog_token);
		return 0;
	}

	query->status_code = WPA_GET_LE16(pos);
	pos += 2;

	if (query->status_code != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "GAS: Query to " MACSTR " dialog token "
			   "%u failed - status code %u",
			   MAC2STR(sa), dialog_token, query->status_code);
		gas_query_done(gas, query, GAS_QUERY_FAILURE);
		return 0;
	}

	if (action == WLAN_PA_GAS_COMEBACK_RESP) {
		if (pos + 1 > data + len)
			return 0;
		frag_id = *pos & 0x7f;
		more_frags = (*pos & 0x80) >> 7;
		pos++;
	}

	/* Comeback Delay */
	if (pos + 2 > data + len)
		return 0;
	comeback_delay = WPA_GET_LE16(pos);
	pos += 2;

	/* Advertisement Protocol element */
	if (pos + 2 > data + len || pos + 2 + pos[1] > data + len) {
		wpa_printf(MSG_DEBUG, "GAS: No room for Advertisement "
			   "Protocol element in the response from " MACSTR,
			   MAC2STR(sa));
		return 0;
	}

	if (*pos != WLAN_EID_ADV_PROTO) {
		wpa_printf(MSG_DEBUG, "GAS: Unexpected Advertisement "
			   "Protocol element ID %u in response from " MACSTR,
			   *pos, MAC2STR(sa));
		return 0;
	}

	adv_proto = pos;
	pos += 2 + pos[1];

	/* Query Response Length */
	if (pos + 2 > data + len) {
		wpa_printf(MSG_DEBUG, "GAS: No room for GAS Response Length");
		return 0;
	}
	resp_len = WPA_GET_LE16(pos);
	pos += 2;

	if (pos + resp_len > data + len) {
		wpa_printf(MSG_DEBUG, "GAS: Truncated Query Response in "
			   "response from " MACSTR, MAC2STR(sa));
		return 0;
	}

	if (pos + resp_len < data + len) {
		wpa_printf(MSG_DEBUG, "GAS: Ignore %u octets of extra data "
			   "after Query Response from " MACSTR,
			   (unsigned int) (data + len - pos - resp_len),
			   MAC2STR(sa));
	}

	if (action == WLAN_PA_GAS_COMEBACK_RESP)
		gas_query_rx_comeback(gas, query, adv_proto, pos, resp_len,
				      frag_id, more_frags, comeback_delay);
	else
		gas_query_rx_initial(gas, query, adv_proto, pos, resp_len,
				     comeback_delay);

	return 0;
}


static void gas_query_timeout(void *eloop_data, void *user_ctx)
{
	struct gas_query *gas = eloop_data;
	struct gas_query_pending *query = user_ctx;

	wpa_printf(MSG_DEBUG, "GAS: No response received for query to " MACSTR,
		   MAC2STR(query->addr));
	gas_query_done(gas, query, GAS_QUERY_TIMEOUT);
}


static int gas_query_dialog_token_available(struct gas_query *gas,
					    const u8 *dst, u8 dialog_token)
{
	struct gas_query_pending *q;
	dl_list_for_each(q, &gas->pending, struct gas_query_pending, list) {
		if (os_memcmp(dst, q->addr, ETH_ALEN) == 0 &&
		    dialog_token == q->dialog_token)
			return 0;
	}

	return 1;
}


/**
 * gas_query_req - Request a GAS query
 * @gas: GAS query data from gas_query_init()
 * @dst: Destination MAC address for the query
 * @freq: Frequency (in MHz) for the channel on which to send the query
 * @req: GAS query payload
 * @cb: Callback function for reporting GAS query result and response
 * @ctx: Context pointer to use with the @cb call
 * Returns: dialog token (>= 0) on success or -1 on failure
 */
int gas_query_req(struct gas_query *gas, const u8 *dst, int freq,
		  struct wpabuf *req,
		  void (*cb)(void *ctx, const u8 *dst, u8 dialog_token,
			     enum gas_query_result result,
			     const struct wpabuf *adv_proto,
			     const struct wpabuf *resp, u16 status_code),
		  void *ctx)
{
	struct gas_query_pending *query;
	int dialog_token;

	if (wpabuf_len(req) < 3)
		return -1;

	for (dialog_token = 0; dialog_token < 256; dialog_token++) {
		if (gas_query_dialog_token_available(gas, dst, dialog_token))
			break;
	}
	if (dialog_token == 256)
		return -1; /* Too many pending queries */

	query = os_zalloc(sizeof(*query));
	if (query == NULL)
		return -1;

	os_memcpy(query->addr, dst, ETH_ALEN);
	query->dialog_token = dialog_token;
	query->freq = freq;
	query->cb = cb;
	query->ctx = ctx;
	dl_list_add(&gas->pending, &query->list);

	*(wpabuf_mhead_u8(req) + 2) = dialog_token;

	wpa_printf(MSG_DEBUG, "GAS: Starting request for " MACSTR
		   " dialog_token %u", MAC2STR(dst), dialog_token);
	if (gas_query_tx(gas, query, req) < 0) {
		wpa_printf(MSG_DEBUG, "GAS: Failed to send Action frame to "
			   MACSTR, MAC2STR(query->addr));
		dl_list_del(&query->list);
		os_free(query);
		return -1;
	}

	eloop_register_timeout(GAS_QUERY_TIMEOUT_PERIOD, 0, gas_query_timeout,
			       gas, query);

	return dialog_token;
}


/**
 * gas_query_cancel - Cancel a pending GAS query
 * @gas: GAS query data from gas_query_init()
 * @dst: Destination MAC address for the query
 * @dialog_token: Dialog token from gas_query_req()
 */
void gas_query_cancel(struct gas_query *gas, const u8 *dst, u8 dialog_token)
{
	struct gas_query_pending *query;

	query = gas_query_get_pending(gas, dst, dialog_token);
	if (query)
		gas_query_done(gas, query, GAS_QUERY_CANCELLED);

}
