/*
 * Wi-Fi Aware - NAN Bootstrap
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"
#include "utils/eloop.h"
#include "nan_i.h"

#define NAN_BOOTSTRAP_RETRY_TIMEOUT_US (1 * 1000 * 1000) /* 1 second */
#define NAN_BOOTSTRAP_COOKIE_LEN 4

static void nan_bootstrap_timeout(void *eloop_data, void *user_ctx);


/**
 * nan_complement_pbm - Get complement pairing bootstrapping method
 * @pbm: Pairing bootstrapping method
 * Returns: Complement pairing bootstrapping method
 */
static u16 nan_complement_pbm(u16 pbm)
{
	if (pbm == NAN_PBA_METHOD_OPPORTUNISTIC)
		return NAN_PBA_METHOD_OPPORTUNISTIC;
	if (pbm == NAN_PBA_METHOD_PIN_DISPLAY)
		return NAN_PBA_METHOD_PIN_KEYPAD;
	if (pbm == NAN_PBA_METHOD_PASSPHRASE_DISPLAY)
		return NAN_PBA_METHOD_PASSPHRASE_KEYPAD;
	if (pbm == NAN_PBA_METHOD_QR_DISPLAY)
		return NAN_PBA_METHOD_QR_SCAN;
	if (pbm == NAN_PBA_METHOD_NFC_TAG)
		return NAN_PBA_METHOD_NFC_READER;
	if (pbm == NAN_PBA_METHOD_PIN_KEYPAD)
		return NAN_PBA_METHOD_PIN_DISPLAY;
	if (pbm == NAN_PBA_METHOD_PASSPHRASE_KEYPAD)
		return NAN_PBA_METHOD_PASSPHRASE_DISPLAY;
	if (pbm == NAN_PBA_METHOD_QR_SCAN)
		return NAN_PBA_METHOD_QR_DISPLAY;
	if (pbm == NAN_PBA_METHOD_NFC_READER)
		return NAN_PBA_METHOD_NFC_TAG;
	if (pbm == NAN_PBA_METHOD_SERVICE_MANAGED)
		return NAN_PBA_METHOD_SERVICE_MANAGED;
	if (pbm == NAN_PBA_METHOD_HANDSHAKE_SKIPPED)
		return NAN_PBA_METHOD_HANDSHAKE_SKIPPED;

	return 0;
}


/**
 * nan_bootstrap_reset - Reset bootstrap state
 * @nan: NAN module context from nan_init()
 * @peer: Peer to reset bootstrap state
 */
void nan_bootstrap_reset(struct nan_data *nan, struct nan_peer *peer)
{
	wpa_printf(MSG_DEBUG, "NAN: Bootstrap: Reset state");

	eloop_cancel_timeout(nan_bootstrap_timeout, nan, peer);
	os_free(peer->bootstrap.cookie);

	/*
	 * Do not use memset to reset all data to preserve the peer's
	 * supported bootstrap methods and the NPBA buffer if present.
	 */
	peer->bootstrap.cookie = NULL;
	peer->bootstrap.cookie_len = 0;
	peer->bootstrap.initiator = false;
	peer->bootstrap.dialog_token = 0;
	peer->bootstrap.requested_pbm = 0;
	peer->bootstrap.authorized = 0;
	peer->bootstrap.status = 0;
	peer->bootstrap.comeback_required = false;
	peer->bootstrap.in_progress = false;
	peer->bootstrap.reason_code = 0;
	peer->bootstrap.handle = -1;
	peer->bootstrap.req_instance_id = 0;
}


/**
 * nan_bootstrap_build_npba - Build NAN Pairing Bootstrap attribute
 * @nan: NAN module context from nan_init()
 * @peer: Peer for which to build the attribute
 * Returns: The constructed NPBA or %NULL on failure
 */
static struct wpabuf * nan_bootstrap_build_npba(struct nan_data *nan,
						struct nan_peer *peer)
{
	struct wpabuf *buf;
	u16 pbm = peer->bootstrap.requested_pbm;
	u8 type_and_status;
	u8 *len;

	/*
	 * Allocate max possible size: header (3) + Dialog Token (1) + Type and
	 * Status (1) + Reason (1) + Comeback (2 + 1 + Comeback Token) + pbm (2)
	 */
	buf = wpabuf_alloc(11 + peer->bootstrap.cookie_len);
	if (!buf)
		return NULL;

	wpa_printf(MSG_DEBUG, "NAN: Bootstrap: Build NPBA");

	if (peer->bootstrap.initiator) {
		type_and_status = NAN_PBA_TYPE_RESPONSE;
		if (peer->bootstrap.status)
			pbm = 0;
	} else {
		type_and_status = NAN_PBA_TYPE_REQUEST;
	}

	type_and_status |= peer->bootstrap.status << NAN_PBA_STATUS_POS;

	wpabuf_put_u8(buf, NAN_ATTR_NPBA);
	len = wpabuf_put(buf, 2);

	wpabuf_put_u8(buf, peer->bootstrap.dialog_token);
	wpabuf_put_u8(buf, type_and_status);
	wpabuf_put_u8(buf, peer->bootstrap.reason_code);

	if (peer->bootstrap.status == NAN_PBA_STATUS_COMEBACK) {
		if (peer->bootstrap.initiator)
			wpabuf_put_le16(buf, peer->bootstrap.comeback_after);

		wpabuf_put_u8(buf, peer->bootstrap.cookie_len);
		if (peer->bootstrap.cookie)
			wpabuf_put_data(buf, peer->bootstrap.cookie,
					peer->bootstrap.cookie_len);
	}

	wpabuf_put_le16(buf, pbm);
	WPA_PUT_LE16(len, wpabuf_len(buf) - 3);

	return buf;
}


/**
 * nan_bootstrap_timeout - Bootstrap timeout handler
 * @eloop_data: NAN module context from nan_init()
 * @user_ctx: Peer for which the timeout occurred
 */
static void nan_bootstrap_timeout(void *eloop_data, void *user_ctx)
{
	struct nan_data *nan = eloop_data;
	struct nan_peer *peer = user_ctx;
	struct wpabuf *attr;

	wpa_printf(MSG_DEBUG, "NAN: Bootstrap: timeout. status=%u",
		   peer->bootstrap.status);

	attr = nan_bootstrap_build_npba(nan, peer);
	if (!attr) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Failed to build attribute");

		nan_bootstrap_reset(nan, peer);

		nan->cfg->bootstrap_completed(nan->cfg->cb_ctx, peer->nmi_addr,
					      0, false,
					      NAN_REASON_UNSPECIFIED_REASON,
					      -1, 0);
		return;
	}

	if (nan->cfg->transmit_followup(nan->cfg->cb_ctx, peer->nmi_addr, attr,
					peer->bootstrap.handle,
					peer->bootstrap.req_instance_id)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Failed to transmit followup");

		nan_bootstrap_reset(nan, peer);

		nan->cfg->bootstrap_completed(nan->cfg->cb_ctx, peer->nmi_addr,
					      0, false,
					      NAN_REASON_UNSPECIFIED_REASON,
					      peer->bootstrap.handle,
					      peer->bootstrap.req_instance_id);
	}

	wpabuf_free(attr);

	/* If the peer didn't reply, try again */
	if (peer->bootstrap.status != NAN_PBA_STATUS_COMEBACK)
		eloop_register_timeout(0, NAN_BOOTSTRAP_RETRY_TIMEOUT_US,
				       nan_bootstrap_timeout, nan, peer);
}


/**
 * nan_bootstrap_handle_rx_request - Process received bootstrap request
 * @nan: NAN module context from nan_init()
 * @peer: Peer from which the request was received
 * @dialog_token: Dialog token from the request
 * @pbm: Pairing bootstrapping method bitmap from the request
 * @cookie: Comeback cookie from the request (if any)
 * @cookie_len: Length of the comeback cookie
 * @status: Status field from the request
 * @handle: Follow-up handle
 * @req_instance_id: Follow-up instance ID
 * @npba: NPBA from the request
 * @npba_len: Length of the NPBA
 */
static void nan_bootstrap_handle_rx_request(struct nan_data *nan,
					    struct nan_peer *peer,
					    u8 dialog_token, u16 pbm,
					    const u8 *cookie,
					    u8 cookie_len, u8 status,
					    int handle, u8 req_instance_id,
					    const u8 *npba, u16 npba_len)
{
	struct wpabuf *attr = NULL;
	u16 supported_methods;

	wpa_printf(MSG_DEBUG, "NAN: Bootstrap: RX request");

	if (peer->bootstrap.initiator && peer->bootstrap.comeback_required) {
		if (status != NAN_PBA_STATUS_COMEBACK) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Bootstrap: Missing comeback cookie. Ignore");
			return;
		}

		if (cookie_len != peer->bootstrap.cookie_len ||
		    !!cookie ^ !!peer->bootstrap.cookie ||
		    (cookie && os_memcmp(cookie, peer->bootstrap.cookie,
					 cookie_len) != 0)) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Bootstrap: Invalid comeback cookie. Ignore");
			return;
		}

		if (dialog_token != peer->bootstrap.dialog_token) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Bootstrap: Invalid dialog token. Ignore");
			return;
		}

		if (handle != peer->bootstrap.handle ||
		    req_instance_id != peer->bootstrap.req_instance_id) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Bootstrap: Invalid handle or instance ID");
			return;
		}

		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Valid comeback request(pbm=0x%04x) authorized=0x%04x",
			   pbm, peer->bootstrap.authorized);

		if (peer->bootstrap.authorized == nan_complement_pbm(pbm)) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Bootstrap: Peer bootstrap authorized. Accept.");

			peer->bootstrap.status = NAN_PBA_STATUS_ACCEPTED;
			peer->bootstrap.reason_code = 0;
			peer->bootstrap.comeback_required = false;
			peer->bootstrap.requested_pbm = nan_complement_pbm(pbm);
		}

		goto send_response;
	}

	wpa_printf(MSG_DEBUG,
		   "NAN: Bootstrap: New bootstrap request. pbm=0x%04x", pbm);

	nan_bootstrap_reset(nan, peer);

	peer->bootstrap.initiator = true;
	peer->bootstrap.dialog_token = dialog_token;
	peer->bootstrap.requested_pbm = nan_complement_pbm(pbm);
	peer->bootstrap.in_progress = true;
	peer->bootstrap.handle = handle;
	peer->bootstrap.req_instance_id = req_instance_id;
	supported_methods =
		nan->cfg->get_supported_bootstrap_methods(nan->cfg->cb_ctx,
							  handle);

	if (!(supported_methods & peer->bootstrap.requested_pbm)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: No supported bootstrap methods in request");

		peer->bootstrap.status = NAN_PBA_STATUS_REJECTED;
		peer->bootstrap.reason_code =
			NAN_REASON_PAIR_BOOTSTRAP_REJECTED;
		goto send_response;
	}

	if (peer->bootstrap.requested_pbm &
	    nan->cfg->auto_accept_bootstrap_methods) {
		peer->bootstrap.authorized = peer->bootstrap.requested_pbm;
		peer->bootstrap.status = NAN_PBA_STATUS_ACCEPTED;
		goto send_response;
	}

	peer->bootstrap.status = NAN_PBA_STATUS_COMEBACK;
	peer->bootstrap.comeback_required = true;
	peer->bootstrap.cookie_len = NAN_BOOTSTRAP_COOKIE_LEN;
	peer->bootstrap.cookie = os_malloc(peer->bootstrap.cookie_len);

	if (peer->bootstrap.cookie) {
		if (os_get_random(peer->bootstrap.cookie,
				  peer->bootstrap.cookie_len) < 0) {
			nan_bootstrap_reset(nan, peer);
			return;
		}
	} else {
		peer->bootstrap.cookie_len = 0;
	}

	peer->bootstrap.comeback_after = nan->cfg->bootstrap_comeback_timeout;

	if (nan->cfg->bootstrap_request)
		nan->cfg->bootstrap_request(nan->cfg->cb_ctx, peer->nmi_addr,
					    peer->bootstrap.requested_pbm,
					    handle, req_instance_id);

send_response:
	attr = nan_bootstrap_build_npba(nan, peer);
	if (!attr) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Failed to build bootstrap attribute");
		goto done;
	}

	if (nan->cfg->transmit_followup(nan->cfg->cb_ctx, peer->nmi_addr, attr,
					handle, req_instance_id)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Failed to transmit bootstrap followup");

		nan->cfg->bootstrap_completed(nan->cfg->cb_ctx, peer->nmi_addr,
					      0, false,
					      NAN_REASON_UNSPECIFIED_REASON,
					      handle, req_instance_id);
		goto done;
	}

	if (peer->bootstrap.status == NAN_PBA_STATUS_COMEBACK) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Wait for comeback bootstrap request");

		wpabuf_free(attr);
		return;
	}

	if (peer->bootstrap.status == NAN_PBA_STATUS_ACCEPTED) {
		wpabuf_free(peer->bootstrap.npba);
		peer->bootstrap.npba = wpabuf_alloc(3 + npba_len);
		if (peer->bootstrap.npba) {
			wpabuf_put_u8(peer->bootstrap.npba, NAN_ATTR_NPBA);
			wpabuf_put_le16(peer->bootstrap.npba, npba_len);
			wpabuf_put_data(peer->bootstrap.npba, npba, npba_len);
		} else {
			wpa_printf(MSG_INFO,
				   "NAN: Bootstrap: Failed to store NPBA");
		}
	}

	nan->cfg->bootstrap_completed(nan->cfg->cb_ctx,
				      peer->nmi_addr,
				      peer->bootstrap.requested_pbm,
				      peer->bootstrap.status ==
				      NAN_PBA_STATUS_ACCEPTED,
				      peer->bootstrap.reason_code,
				      peer->bootstrap.handle,
				      peer->bootstrap.req_instance_id);
done:
	wpabuf_free(attr);
	nan_bootstrap_reset(nan, peer);
}


/**
 * nan_bootstrap_supported - Check if bootstrap operations are supported
 * @nan: NAN module context from nan_init()
 * Returns: Whether all required bootstrap operations are supported
 */
static bool nan_bootstrap_supported(struct nan_data *nan)
{
	return nan->cfg->bootstrap_completed &&
		nan->cfg->bootstrap_request &&
		nan->cfg->transmit_followup &&
		nan->cfg->get_supported_bootstrap_methods;
}


/**
 * nan_bootstrap_handle_rx - Process received bootstrap follow-up
 * @nan: NAN module context from nan_init()
 * @peer_nmi: Peer address from which the follow-up was received
 * @npba: Pointer to the NPBA in the follow-up
 * @npba_len: Length of the NPBA
 * @buf: Complete Follow-up frame
 * @len: Length of the complete Follow-up frame
 * @handle: Follow up handle
 * @req_instance_id: Follow-up instance ID
 * Returns: true if the follow up was processed, false on error
 */
bool nan_bootstrap_handle_rx(struct nan_data *nan, const u8 *peer_nmi,
			     const u8 *npba, u16 npba_len,
			     const u8 *buf, size_t len,
			     int handle, u8 req_instance_id)
{
	const u8 *cookie = NULL;
	u8 dialog_token, type, status, reason_code, cookie_len = 0;
	u16 pbm, comeback_after = 0;
	struct nan_peer *peer;
	const u8 *orig_npba = npba;
	u16 orig_npba_len = npba_len;

	if (!nan_bootstrap_supported(nan)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Not all bootstrap operations are supported. Ignore");
		return false;
	}

	if (npba_len < 3)
		return false;
	dialog_token = *npba++;
	type = *npba & NAN_PBA_TYPE_MASK;
	status = (*npba++ >> NAN_PBA_STATUS_POS) & NAN_PBA_STATUS_MASK;
	reason_code = *npba++;
	npba_len -= 3;

	if (type == NAN_PBA_TYPE_ADVERTISE) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: type == Advertise. Ignore");
		return false;
	}

	if (type != NAN_PBA_TYPE_REQUEST &&
	    type != NAN_PBA_TYPE_RESPONSE) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Unknown NPBA type %u", type);
		return false;
	}

	if (status == NAN_PBA_STATUS_COMEBACK) {
		if (type == NAN_PBA_TYPE_RESPONSE) {
			if (npba_len < 2) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Bootstrap: Response too short for comeback after");
				return false;
			}

			comeback_after = WPA_GET_LE16(npba);
			npba += 2;
			npba_len -= 2;
		}

		if (npba_len < 1) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Bootstrap: Response too short for comeback cookie length");
			return false;
		}

		cookie_len = *npba++;
		npba_len--;
		if (cookie_len > npba_len) {
			wpa_printf(MSG_DEBUG,
				   "NAN: Bootstrap: Comeback field is too long");
			return false;
		}

		cookie = npba;
		npba += cookie_len;
		npba_len -= cookie_len;
	}

	if (npba_len < 2) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Buffer too short for PBM");
		return false;
	}

	pbm = WPA_GET_LE16(npba);
	peer = nan_get_peer(nan, peer_nmi);

	/* Handle NAN bootstrap request */
	if (type == NAN_PBA_TYPE_REQUEST) {
		if (!peer) {
			nan_add_peer(nan, peer_nmi, buf, len);
			peer = nan_get_peer(nan, peer_nmi);
			if (!peer) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Bootstrap: Failed alloc peer from bootstrap request");
				return false;
			}
		}

		nan_bootstrap_handle_rx_request(nan, peer, dialog_token,
						pbm, cookie, cookie_len,
						status, handle,
						req_instance_id,
						orig_npba, orig_npba_len);
		return true;
	}

	/* Handle NAN bootstrap response */
	if (!peer || !peer->bootstrap.in_progress ||
	    peer->bootstrap.initiator) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Unexpected bootstrap response");
		return false;
	}

	if (dialog_token != peer->bootstrap.dialog_token) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Response with invalid dialog token. Ignore");
		return false;
	}

	if (handle != peer->bootstrap.handle ||
	    req_instance_id != peer->bootstrap.req_instance_id) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Response with invalid handle or instance ID. Ignore");
		return false;
	}

	peer->bootstrap.status = status;
	peer->bootstrap.reason_code = reason_code;
	peer->bootstrap.comeback_required = status == NAN_PBA_STATUS_COMEBACK;
	peer->bootstrap.comeback_after = comeback_after;

	if (cookie && cookie_len) {
		os_free(peer->bootstrap.cookie);
		peer->bootstrap.cookie = os_memdup(cookie, cookie_len);
		if (!peer->bootstrap.cookie) {
			nan_bootstrap_reset(nan, peer);
			return false;
		}
		peer->bootstrap.cookie_len = cookie_len;
	}

	if (status == NAN_PBA_STATUS_ACCEPTED &&
	    nan_complement_pbm(pbm) == peer->bootstrap.requested_pbm) {
		wpa_printf(MSG_DEBUG, "NAN: Bootstrap: Accepted. Complete.");

		nan->cfg->bootstrap_completed(nan->cfg->cb_ctx,
					      peer->nmi_addr,
					      peer->bootstrap.requested_pbm,
					      true, 0,
					      peer->bootstrap.handle,
					      peer->bootstrap.req_instance_id);
		nan_bootstrap_reset(nan, peer);
	} else if (status == NAN_PBA_STATUS_REJECTED) {
		wpa_printf(MSG_DEBUG, "NAN: Bootstrap: Rejected. Complete");

		nan->cfg->bootstrap_completed(nan->cfg->cb_ctx, peer->nmi_addr,
					      0, false, reason_code,
					      peer->bootstrap.handle,
					      peer->bootstrap.req_instance_id);
		nan_bootstrap_reset(nan, peer);
	} else if (status == NAN_PBA_STATUS_COMEBACK) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Comeback required. Schedule timeout for %u TU",
			   peer->bootstrap.comeback_after);

		eloop_cancel_timeout(nan_bootstrap_timeout, nan, peer);
		eloop_register_timeout(peer->bootstrap.comeback_after / 1024,
				       (peer->bootstrap.comeback_after % 1024) *
				       1000,
				       nan_bootstrap_timeout, nan, peer);
	} else {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Unknown status=%u. Abort.",
			   status);

		nan->cfg->bootstrap_completed(nan->cfg->cb_ctx, peer->nmi_addr,
					      0, false,
					      NAN_REASON_UNSPECIFIED_REASON,
					      peer->bootstrap.handle,
					      peer->bootstrap.req_instance_id);
		nan_bootstrap_reset(nan, peer);
	}

	return true;
}


/**
 * nan_bootstrap_request - Initiate NAN bootstrap request to a peer
 * @nan: NAN module context from nan_init()
 * @handle: Follow up handle
 * @peer_nmi: Peer address to which to send the bootstrap request
 * @req_instance_id: Follow up instance ID
 * @pbm: Pairing bootstrapping method bitmap to use
 * @auth: If true, authorize the bootstrap request for the peer instead of
 *        sending the request
 * Returns: 0 on success, -1 on failure
 */
int nan_bootstrap_request(struct nan_data *nan, int handle,
			  const u8 *peer_nmi, u8 req_instance_id, u16 pbm,
			  bool auth)
{
	struct nan_peer *peer;
	int ret;

	if (!nan || !nan->nan_started)
		return -1;

	if (!nan_bootstrap_supported(nan)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Not all bootstrap operations are supported. Ignore");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "NAN: Bootstrap: Request bootstrap to peer " MACSTR
		   " pbm=0x%x auth=%d",
		   MAC2STR(peer_nmi), pbm, auth);

	peer = nan_get_peer(nan, peer_nmi);
	if (!peer) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Request for unknown peer");
		return -1;
	}

	if (!(pbm & nan->cfg->supported_bootstrap_methods)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Unsupported method=0x%x (local support=0x%x)",
			   pbm, nan->cfg->supported_bootstrap_methods);
		return -1;
	}

	if (pbm & (pbm - 1)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Multiple bootstrap methods=0x%x in request",
			   pbm);
		return -1;
	}

	if (auth) {
		wpa_printf(MSG_DEBUG, "NAN: Bootstrap: Authorize request");

		peer->bootstrap.authorized = pbm;
		return 0;
	}

	if (peer->bootstrap.in_progress) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: already in progress with the peer");
		return -1;
	}

	if (!(nan_complement_pbm(pbm) & peer->bootstrap.supported_methods)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Peer doesn't support method=0x%x",
			   pbm);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "NAN: Prepare bootstrap request");

	nan_bootstrap_reset(nan, peer);

	peer->bootstrap.dialog_token = nan_get_next_dialog_token(nan);
	peer->bootstrap.requested_pbm = pbm;
	peer->bootstrap.in_progress = true;
	peer->bootstrap.handle = handle;
	peer->bootstrap.req_instance_id = req_instance_id;

	peer->bootstrap.npba = nan_bootstrap_build_npba(nan, peer);
	if (!peer->bootstrap.npba) {
		nan_bootstrap_reset(nan, peer);
		return -1;
	}

	ret = nan->cfg->transmit_followup(nan->cfg->cb_ctx, peer->nmi_addr,
					  peer->bootstrap.npba, handle,
					  req_instance_id);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Failed to transmit follow-up");

		nan_bootstrap_reset(nan, peer);
		return -1;
	}

	eloop_register_timeout(0, NAN_BOOTSTRAP_RETRY_TIMEOUT_US,
			       nan_bootstrap_timeout, nan, peer);
	return ret;
}


/**
 * nan_bootstrap_peer_reset - Reset bootstrap state for a peer
 * @nan: NAN module context from nan_init()
 * @peer_nmi: Peer address for which to reset the bootstrap state
 * Returns: 0 on success, -1 on failure
 */
int nan_bootstrap_peer_reset(struct nan_data *nan, const u8 *peer_nmi)
{
	struct nan_peer *peer;

	if (!nan || !nan->nan_started)
		return -1;

	peer = nan_get_peer(nan, peer_nmi);
	if (!peer) {
		wpa_printf(MSG_DEBUG, "NAN: Bootstrap: Reset for unknown peer");
		return -1;
	}

	nan_bootstrap_reset(nan, peer);
	return 0;
}


/**
 * nan_bootstrap_get_supported_methods - Get supported bootstrap methods for a peer
 *
 * @nan: NAN module context from nan_init()
 * @peer_nmi: Peer address
 * @supported_methods: Pointer to store the supported methods bitmap
 * Returns: 0 on success, -1 on failure
 */
int nan_bootstrap_get_supported_methods(struct nan_data *nan,
					const u8 *peer_nmi,
					u16 *supported_methods)
{
	struct nan_peer *peer;

	if (!nan || !nan->nan_started || !supported_methods)
		return -1;

	peer = nan_get_peer(nan, peer_nmi);
	if (!peer) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Bootstrap: Get supported methods for unknown peer");
		return -1;
	}

	*supported_methods = peer->bootstrap.supported_methods;
	return 0;
}
