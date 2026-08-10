/*
 * Wi-Fi Aware - NAN Data Path
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"
#include "nan_i.h"


static const char * nan_ndp_state_str(enum nan_ndp_state state)
{
#define C2S(x) case x: return #x;
	switch (state) {
	C2S(NAN_NDP_STATE_NONE)
	C2S(NAN_NDP_STATE_START)
	C2S(NAN_NDP_STATE_REQ_SENT)
	C2S(NAN_NDP_STATE_REQ_RECV)
	C2S(NAN_NDP_STATE_RES_SENT)
	C2S(NAN_NDP_STATE_RES_RECV)
	C2S(NAN_NDP_STATE_CON_SENT)
	C2S(NAN_NDP_STATE_CON_RECV)
	C2S(NAN_NDP_STATE_DONE)
	default:
		return "Invalid NAN NDP state";
	}
}


static void nan_ndp_set_state(struct nan_data *nan,
			      struct nan_ndp_setup *ndp_setup,
			      enum nan_ndp_state state)
{
	wpa_printf(MSG_DEBUG,
		   "NAN: NDP: state: %s (%u) --> %s (%u)",
		   nan_ndp_state_str(ndp_setup->state),
		   ndp_setup->state, nan_ndp_state_str(state),
		   state);

	ndp_setup->state = state;
}


static struct nan_ndp *
nan_ndp_alloc(struct nan_data *nan, struct nan_peer *peer,
	      u8 initiator, u8 *init_ndi, u8 ndp_id, u8 min_slots,
	      u16 max_latency)
{
	struct nan_ndp *ndp = os_zalloc(sizeof(*ndp));

	if (!ndp)
		return NULL;

	ndp->peer = peer;
	ndp->initiator = initiator;
	ndp->ndp_id = ndp_id;

	os_memcpy(ndp->init_ndi, init_ndi, sizeof(ndp->init_ndi));

	ndp->qos.min_slots = min_slots;
	ndp->qos.max_latency = max_latency;

	return ndp;
}


static int nan_ndp_ssi(struct nan_data *nan, struct nan_ndp_setup *ndp_setup,
		       const u8 *ssi, u16 ssi_len)
{
	os_free(ndp_setup->ssi);
	ndp_setup->ssi = NULL;
	ndp_setup->ssi_len = 0;

	if (!ssi || !ssi_len)
		return 0;

	ndp_setup->ssi = os_memdup(ssi, ssi_len);
	if (!ndp_setup->ssi) {
		wpa_printf(MSG_INFO, "NAN: NDP: Failed to allocate NDP ssi");
		return -1;
	}

	ndp_setup->ssi_len = ssi_len;
	return 0;
}


/**
 * nan_ndp_setup_req - Start handling of NDP setup request
 * @nan: NAN module context from nan_init()
 * @peer: The peer to initiate the NDP setup with
 * @params: NDP setup request parameters
 * Returns: 0 on success, negative on failure
 *
 * On successful request, the data structures would be ready to
 * continue the NDP establishment.
 */
int nan_ndp_setup_req(struct nan_data *nan, struct nan_peer *peer,
		      struct nan_ndp_params *params)
{
	int ret;

	if (peer->ndp_setup.ndp) {
		wpa_printf(MSG_DEBUG, "NAN: NDP: already WIP with peer");
		return -1;
	}

	peer->ndp_setup.ndp = nan_ndp_alloc(nan, peer, 1,
					    params->ndp_id.init_ndi,
					    params->ndp_id.id,
					    params->qos.min_slots,
					    params->qos.max_latency);
	if (!peer->ndp_setup.ndp) {
		wpa_printf(MSG_DEBUG, "NAN: NDP: Failed allocation");
		return -1;
	}

	peer->ndp_setup.dialog_token = nan_get_next_dialog_token(nan);
	peer->ndp_setup.publish_inst_id = params->u.req.publish_inst_id;
	os_memcpy(peer->ndp_setup.service_id, params->u.req.service_id,
		  NAN_SERVICE_ID_LEN);

	/* Require confirmation for all locally initiated NDPs */
	peer->ndp_setup.conf_req = 1;

	/* Store service specific information */
	ret = nan_ndp_ssi(nan, &peer->ndp_setup, params->ssi, params->ssi_len);
	if (ret) {
		os_free(peer->ndp_setup.ndp);
		peer->ndp_setup.ndp = NULL;
		return ret;
	}

	nan_sec_reset(nan, &peer->ndp_setup.sec);

	if (params->sec.csid) {
		peer->ndp_setup.sec.i_csid = params->sec.csid;
		os_memcpy(peer->ndp_setup.sec.pmk, params->sec.pmk, PMK_LEN);

		peer->ndp_setup.sec.present = true;
		peer->ndp_setup.sec.valid = true;

		peer->ndp_setup.sec.i_instance_id =
			peer->ndp_setup.publish_inst_id;

		os_memcpy(&peer->ndp_setup.sec.local_gtk, &params->sec.gtk,
			  sizeof(peer->ndp_setup.sec.local_gtk));
	}

	if (params->interface_id) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP setup request with local interface id");

		os_memcpy(peer->ndp_setup.local_interface_id,
			  params->interface_id,
			  NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN);

		peer->ndp_setup.local_interface_id_valid = true;
	}

	nan_ndp_set_state(nan, &peer->ndp_setup, NAN_NDP_STATE_START);
	peer->ndp_setup.status = NAN_NDP_STATUS_CONTINUED;
	return 0;
}


/**
 * nan_ndp_setup_resp - Handle higher layer response for an NDP request
 * @nan: NAN module context from nan_init()
 * @peer: Peer that originated the NDP setup request
 * @params: NDP setup parameters
 * Returns: 0 on success, negative on failure
 *
 * The response status can be either ACCEPTED or REJECTED. The internal logic of
 * the NDP state machine adjusts its own state according to the response status
 * and its internal state.
 */
int nan_ndp_setup_resp(struct nan_data *nan, struct nan_peer *peer,
		       struct nan_ndp_params *params)
{
	int ret;

	if (!peer->ndp_setup.ndp ||
	    peer->ndp_setup.ndp->ndp_id != params->ndp_id.id ||
	    !ether_addr_equal(peer->ndp_setup.ndp->init_ndi,
			      params->ndp_id.init_ndi)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: No matching NDP found for NDP response");
		return -1;
	}

	if (peer->ndp_setup.state != NAN_NDP_STATE_REQ_RECV) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Unexpected state for response");
		return -1;
	}

	if (params->u.resp.status != NAN_NDP_STATUS_ACCEPTED &&
	    params->u.resp.status != NAN_NDP_STATUS_REJECTED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Unexpected response status=%u",
			params->u.resp.status);
		return -1;
	}

	peer->ndp_setup.status = params->u.resp.status;
	peer->ndp_setup.reason = params->u.resp.reason_code;

	if (peer->ndp_setup.status == NAN_NDP_STATUS_ACCEPTED) {
		if (peer->ndp_setup.conf_req)
			peer->ndp_setup.status = NAN_NDP_STATUS_CONTINUED;

		os_memcpy(peer->ndp_setup.ndp->resp_ndi,
			  params->u.resp.resp_ndi, ETH_ALEN);

		if (!peer->ndp_setup.sec.present && params->sec.csid) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDP: Security not requested by peer");
			return -1;
		}

		if (peer->ndp_setup.sec.present) {
			if (params->sec.csid != peer->ndp_setup.sec.i_csid) {
				wpa_printf(MSG_DEBUG,
					   "NAN: NDP: Different cipher suite specified.");
				return -1;
			}

			peer->ndp_setup.sec.r_csid = params->sec.csid;
			os_memcpy(peer->ndp_setup.sec.pmk, params->sec.pmk,
				  PMK_LEN);
			os_memcpy(&peer->ndp_setup.sec.local_gtk,
				  &params->sec.gtk,
				  sizeof(peer->ndp_setup.sec.local_gtk));

			ret = nan_sec_init_resp(nan, peer);
			if (ret) {
				wpa_printf(MSG_DEBUG,
					   "NAN: NDP: Failed to init responder security");

				peer->ndp_setup.status =
					NAN_NDP_STATUS_REJECTED;
				peer->ndp_setup.reason =
					NAN_REASON_INVALID_PARAMETERS;
				return 0;
			}

			peer->ndp_setup.status = NAN_NDP_STATUS_CONTINUED;
		}
	}

	/* Store service specific information */
	ret = nan_ndp_ssi(nan, &peer->ndp_setup, params->ssi, params->ssi_len);
	if (ret)
		return ret;

	if (params->interface_id) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP setup response with local interface id");

		os_memcpy(peer->ndp_setup.local_interface_id,
			  params->interface_id,
			  NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN);

		peer->ndp_setup.local_interface_id_valid = true;
	}

	return 0;
}


static int nan_ndp_attr_handle_tlvs(struct nan_data *nan,
				    struct nan_peer *peer,
				    const u8 *tlvs, u16 tlvs_len)
{
	wpa_printf(MSG_DEBUG, "NAN: NDP: Handle NDPE TLVs len=%u", tlvs_len);

	while (tlvs_len > 3) {
		u8 tlv_type = tlvs[0];
		u16 tlv_len = WPA_GET_LE16(tlvs + 1);
		const u8 *tlv_data = tlvs + 3;
		int ret;

		if (tlv_len + 3 > tlvs_len) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDP: Invalid TLV len=%u for type=%u",
				   tlv_len, tlv_type);
			return -1;
		}

		switch (tlv_type) {
		case NAN_NDPE_TLV_IPV6_LINK_LOCAL:
			if (tlv_len != NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN) {
				wpa_printf(MSG_DEBUG,
					   "NAN: NDP: req: Invalid interface ID tlv len=%u",
					   tlv_len);
				break;
			}

			peer->ndp_setup.peer_interface_id_valid = true;
			os_memcpy(peer->ndp_setup.peer_interface_id, tlv_data,
				  NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN);
			break;
		case NAN_NDPE_TLV_SRV_INFO:
			wpa_printf(MSG_DEBUG,
				   "NAN: NDP: Handle NDP service specific information");

			if (tlv_len >= 4 && WPA_GET_BE24(tlv_data) == OUI_WFA &&
			    tlv_data[3] == NAN_SRV_PROTO_GENERIC)
				ret = nan_ndp_ssi(nan, &peer->ndp_setup,
						  tlv_data + 4, tlv_len - 4);
			else
				ret = nan_ndp_ssi(nan, &peer->ndp_setup,
						  tlv_data, tlv_len);
			if (ret)
				wpa_printf(MSG_DEBUG,
					   "NAN: NDP: Failed to save ssi - continue");
			break;
		default:
			wpa_printf(MSG_DEBUG, "NAN: NDP: unknown TLV type=%u",
				   tlv_type);
			break;
		}

		tlvs += 3 + tlv_len;
		tlvs_len -= 3 + tlv_len;
	}

	if (tlvs_len) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: TLV parsing ended with %u left octets",
			   tlvs_len);
		return -1;
	}

	return 0;
}


static int nan_ndp_attr_handle_req(struct nan_data *nan, struct nan_peer *peer,
				   struct ieee80211_ndp *ndp_attr, u16 ndp_len,
				   u8 status, bool ndpe)
{
	struct nan_ndp_setup *ndp_setup = &peer->ndp_setup;
	u16 exp_len = sizeof(struct ieee80211_ndp);
	u8 publish_inst_id;

	if (ndp_setup->ndp) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: req: While already WIP with peer");
		return -1;
	}

	if (!(ndp_attr->ndp_ctrl & NAN_NDP_CTRL_PUBLISH_ID_PRESENT)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: req: Without publish ID");
		return -1;
	}

	exp_len++;
	if (ndp_len < exp_len) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: req: Length too short: ndp_len=%u, exp_len=%u",
			   ndp_len, exp_len);
		return -1;
	}

	publish_inst_id = *ndp_attr->optional;
	if (!nan_publish_instance_id_valid(nan, publish_inst_id,
					   ndp_setup->service_id)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: req: Invalid publish instance ID=%u",
			publish_inst_id);
		return -1;
	}

	/* Note: The QoS setting should be set in the response. */
	ndp_setup->ndp = nan_ndp_alloc(nan, peer, 0,
				       ndp_attr->initiator_ndi,
				       ndp_attr->ndp_id,
				       NAN_QOS_MIN_SLOTS_NO_PREF,
				       NAN_QOS_MAX_LATENCY_NO_PREF);
	if (!ndp_setup->ndp)
		return -1;

	nan_sec_reset(nan, &peer->ndp_setup.sec);
	nan_ndp_set_state(nan, ndp_setup, NAN_NDP_STATE_REQ_RECV);

	ndp_setup->status = NAN_NDP_STATUS_CONTINUED;
	ndp_setup->conf_req =
		!!(ndp_attr->ndp_ctrl & NAN_NDP_CTRL_CONFIRM_REQUIRED);
	ndp_setup->dialog_token = ndp_attr->dialog_token;
	ndp_setup->publish_inst_id = publish_inst_id;
	ndp_setup->sec.present =
		!!(ndp_attr->ndp_ctrl & NAN_NDP_CTRL_SECURITY_PRESENT);

	/* Handle service specific information */
	ndp_len -= exp_len;

	if (!ndpe && (ndp_attr->ndp_ctrl & NAN_NDP_CTRL_SPEC_INFO_PRESENT) &&
	    ndp_len) {
		int ret;

		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: req: Handle NDP service specific information");

		ret = nan_ndp_ssi(nan, &peer->ndp_setup,
				  (const u8 *) ndp_attr->optional + 1,
				  ndp_len);
		if (ret)
			wpa_printf(MSG_DEBUG,
				   "NAN: NDP: req: Failed to save ssi. continue");
		return ret;
	}

	if (ndpe)
		return nan_ndp_attr_handle_tlvs(nan, peer,
						ndp_attr->optional + 1,
						ndp_len);

	return 0;
}


static int nan_ndp_attr_handle_res(struct nan_data *nan, struct nan_peer *peer,
				   struct ieee80211_ndp *ndp_attr, u16 ndp_len,
				   u8 status, bool ndpe)
{
	struct nan_ndp_setup *ndp_setup = &peer->ndp_setup;
	u16 opt_len;
	bool sec_present;

	if (!ndp_setup->ndp) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: resp: While no NDP WIP with peer");
		return -1;
	}

	if (ndp_setup->state != NAN_NDP_STATE_REQ_SENT) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: resp: While not expecting one");

		if (ndp_setup->state != NAN_NDP_STATE_START)
			return -1;

		/*
		 * Due to races with the driver, it is possible that the
		 * response is received before an ACK is indicated. Allow the
		 * processing of the attribute, and if all parameters are OK,
		 * fast forward the state machine below.
		 */
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: resp: Received before Tx status.");
	}

	if (ndp_attr->ndp_ctrl & NAN_NDP_CTRL_PUBLISH_ID_PRESENT) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: resp: Unexpected publish ID");
		return -1;
	}

	opt_len = 0;
	if (status == NAN_NDP_STATUS_CONTINUED ||
	    status == NAN_NDP_STATUS_ACCEPTED) {
		if (!(ndp_attr->ndp_ctrl &
		      NAN_NDP_CTRL_RESPONDER_NDI_PRESENT)) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDP: resp: without responder NDI");
			return -1;
		}
		opt_len += ETH_ALEN;
	}

	if (ndp_len < (sizeof(struct ieee80211_ndp) + opt_len)) {
		wpa_printf(MSG_DEBUG, "NAN: NDP: resp: Length too short");
		return -1;
	}

	if (ndp_setup->ndp->ndp_id != ndp_attr->ndp_id ||
	    ndp_setup->dialog_token != ndp_attr->dialog_token ||
	    !ether_addr_equal(ndp_setup->ndp->init_ndi,
			      ndp_attr->initiator_ndi)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: resp: Invalid NDP ID, dialog token or addr");
		return -1;
	}

	if (ndp_setup->state == NAN_NDP_STATE_START) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: resp: Continue though no TX status yet.");
		nan_ndp_set_state(nan, &peer->ndp_setup,
				  NAN_NDP_STATE_REQ_SENT);
	}

	ndp_setup->status = status;

	if (status == NAN_NDP_STATUS_REJECTED) {
		ndp_setup->reason = ndp_attr->reason_code;
		nan_ndp_set_state(nan, &peer->ndp_setup,
				  NAN_NDP_STATE_DONE);
		goto store_ssi;
	}

	sec_present = !!(ndp_attr->ndp_ctrl & NAN_NDP_CTRL_SECURITY_PRESENT);
	if (ndp_setup->sec.present != sec_present) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Security present mismatch");
		return -1;
	}

	if ((sec_present || ndp_setup->conf_req) &&
	    status != NAN_NDP_STATUS_CONTINUED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Security present or confirm required and status != continued");
		return -1;
	}

	if (status == NAN_NDP_STATUS_ACCEPTED)
		nan_ndp_set_state(nan, &peer->ndp_setup, NAN_NDP_STATE_DONE);
	else
		nan_ndp_set_state(nan, &peer->ndp_setup,
				  NAN_NDP_STATE_RES_RECV);

	os_memcpy(ndp_setup->ndp->resp_ndi, &ndp_attr->optional[0],
		  sizeof(ndp_setup->ndp->resp_ndi));

	/*
	 * In case that security is not configured, move the status to accepted.
	 * The state machine would transition to the 'done' state after the
	 * confirm is acked.
	 * TODO: Once security is configured, need to validate the security must
	 * be present.
	 */
	if (!(ndp_attr->ndp_ctrl & NAN_NDP_CTRL_SECURITY_PRESENT))
		ndp_setup->status = NAN_NDP_STATUS_ACCEPTED;

store_ssi:
	/* Handle service specific information */
	ndp_len -= sizeof(struct ieee80211_ndp) + opt_len;
	if (!ndpe && (ndp_attr->ndp_ctrl & NAN_NDP_CTRL_SPEC_INFO_PRESENT) &&
	    ndp_len) {
		int ret;

		wpa_printf(MSG_DEBUG, "NAN: NDP: resp: Handle NDP ssi");
		ret = nan_ndp_ssi(nan, &peer->ndp_setup,
				  ndp_attr->optional + opt_len, ndp_len);
		if (ret)
			wpa_printf(MSG_DEBUG,
				   "NAN: NDP: resp: Failed to save ssi. continue");
		return ret;
	}

	if (ndpe)
		return nan_ndp_attr_handle_tlvs(nan, peer,
						ndp_attr->optional + opt_len,
						ndp_len);

	return 0;
}


static int nan_ndp_attr_handle_confirm(struct nan_data *nan,
				       struct nan_peer *peer,
				       struct ieee80211_ndp *ndp_attr,
				       u8 status)
{
	struct nan_ndp_setup *ndp_setup = &peer->ndp_setup;
	bool sec_present;

	if (!ndp_setup->ndp) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Received confirm while no NDP WIP with peer");
		return -1;
	}

	if (ndp_setup->state != NAN_NDP_STATE_RES_SENT) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: confirm: While not expecting one");

		if (ndp_setup->state != NAN_NDP_STATE_REQ_RECV ||
		    ndp_setup->status != NAN_NDP_STATUS_CONTINUED)
			return -1;

		/*
		 * Due to races with the driver, it is possible that the
		 * confirm is received before an ACK is indicated. Allow the
		 * processing of the attribute, and if all parameters are OK,
		 * fast forward the state machine below.
		 */
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Confirm received before Tx status");
	}

	if (ndp_setup->ndp->ndp_id != ndp_attr->ndp_id ||
	    ndp_setup->dialog_token != ndp_attr->dialog_token ||
	    !ether_addr_equal(ndp_setup->ndp->init_ndi,
			      ndp_attr->initiator_ndi)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: confirm: Invalid NDP ID, dialog token or init ID");
		return -1;
	}

	sec_present = !!(ndp_attr->ndp_ctrl & NAN_NDP_CTRL_SECURITY_PRESENT);
	if (ndp_setup->sec.present != sec_present) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: confirm: Security present mismatch");
		return -1;
	}

	if (sec_present && status != NAN_NDP_STATUS_CONTINUED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: confirm: status != continued with security");
		return -1;
	}

	if (ndp_setup->state == NAN_NDP_STATE_REQ_RECV) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: confirm: Continue though no TX status yet.");
		nan_ndp_set_state(nan, &peer->ndp_setup,
				  NAN_NDP_STATE_RES_SENT);
	}

	ndp_setup->status = status;

	if (sec_present)
		nan_ndp_set_state(nan, &peer->ndp_setup,
				  NAN_NDP_STATE_CON_RECV);
	else
		nan_ndp_set_state(nan, &peer->ndp_setup, NAN_NDP_STATE_DONE);

	return 0;
}


static struct nan_ndp * nan_ndp_find_ndp(struct nan_peer *peer,
					 u8 ndp_id, const u8 *init_ndi)
{
	struct nan_ndp *pndp;

	dl_list_for_each(pndp, &peer->ndps, struct nan_ndp, list) {
		if (pndp->ndp_id == ndp_id &&
		    ether_addr_equal(pndp->init_ndi, init_ndi))
			return pndp;
	}

	return NULL;
}


static int nan_ndp_attr_sec_install(struct nan_data *nan, struct nan_peer *peer,
				    const struct ieee80211_ndp *ndp_attr,
				    u8 status)
{
	struct nan_ndp_setup *ndp_setup = &peer->ndp_setup;
	bool sec_present;

	if (!ndp_setup->ndp) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: sec install while no NDP WIP with peer");
		return -1;
	}

	if (ndp_setup->state != NAN_NDP_STATE_CON_SENT) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: sec install while not expecting one");

		if (ndp_setup->state != NAN_NDP_STATE_RES_RECV ||
		    ndp_setup->status != NAN_NDP_STATUS_CONTINUED)
			return -1;

		/* Due to races with the driver, it is possible that the
		 * install is received before an ACK is indicated. Allow the
		 * processing of the attribute, and if all parameters are OK,
		 * fast forward the state machine below.
		 */
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: sec install received before Tx status.");
	}

	if (ndp_setup->ndp->ndp_id != ndp_attr->ndp_id ||
	    ndp_setup->dialog_token != ndp_attr->dialog_token ||
	    !ether_addr_equal(ndp_setup->ndp->init_ndi,
			      ndp_attr->initiator_ndi)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: sec install: Invalid NDP parameters");
		return -1;
	}

	sec_present = !!(ndp_attr->ndp_ctrl & NAN_NDP_CTRL_SECURITY_PRESENT);
	if (ndp_setup->sec.present != sec_present) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: sec install: Security present mismatch");
		return -1;
	}

	if (status != NAN_NDP_STATUS_ACCEPTED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: sec install: status != ACCEPTED");
		return -1;
	}

	nan_ndp_set_state(nan, &peer->ndp_setup, NAN_NDP_STATE_DONE);
	return 0;
}


static int nan_ndp_attr_handle_term(struct nan_data *nan, struct nan_peer *peer,
				    struct ieee80211_ndp *ndp_attr, u8 status)
{
	struct nan_ndp_setup *ndp_setup = &peer->ndp_setup;
	struct nan_ndp_id ndp_id;
	const u8 *local_ndi, *peer_ndi;
	struct nan_ndp *pndp;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDP: Termination peer=" MACSTR " ndp_id=%u, init_ndi="
		   MACSTR,
		   MAC2STR(peer->nmi_addr), ndp_attr->ndp_id,
		   MAC2STR(ndp_attr->initiator_ndi));

	/*
	 * This should not really happen, but just in case, terminate the
	 * establishment. Since the NDP establishment is not yet done, the NDP
	 * is not added to the list of NDPs, so just reject the establishment.
	 */
	if (ndp_setup->ndp && ndp_setup->ndp->ndp_id == ndp_attr->ndp_id &&
	    ether_addr_equal(ndp_setup->ndp->init_ndi,
			     ndp_attr->initiator_ndi)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Termination while NDP is in progress");

		nan_ndp_set_state(nan, &peer->ndp_setup, NAN_NDP_STATE_DONE);
		ndp_setup->status = NAN_NDP_STATUS_REJECTED;
		ndp_setup->reason = NAN_REASON_UNSPECIFIED_REASON;
		return 0;
	}

	/* Find the NDP in the list of active NDPs */
	pndp = nan_ndp_find_ndp(peer, ndp_attr->ndp_id,
				ndp_attr->initiator_ndi);
	if (!pndp) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Termination but NDP does not exist");
		return 1;
	}

	wpa_printf(MSG_DEBUG, "NAN: NDP: Terminating");

	os_memcpy(ndp_id.peer_nmi, peer->nmi_addr, ETH_ALEN);
	os_memcpy(ndp_id.init_ndi, pndp->init_ndi, ETH_ALEN);
	ndp_id.id = pndp->ndp_id;

	if (pndp->initiator) {
		local_ndi = pndp->init_ndi;
		peer_ndi = pndp->resp_ndi;
	} else {
		local_ndi = pndp->resp_ndi;
		peer_ndi = pndp->init_ndi;
	}

	/*
	 * Remove the NDP from the list of active NDPs before calling
	 * nan_ndp_terminated() as the function checks the list of NDPs to
	 * determine if the NDL should be reset as well. Free the NDP only after
	 * the call as the NDI addresses are still referenced.
	 */
	dl_list_del(&pndp->list);

	nan_ndp_terminated(nan, peer, &ndp_id, local_ndi, peer_ndi,
			   ndp_attr->reason_code, pndp->gtk_id);

	os_free(pndp);

	/* Indicate that no further processing is needed */
	return 1;
}


/**
 * nan_ndp_handle_ndp_attr - Handle NDP attribute and update local state
 * @nan: NAN module context from nan_init()
 * @peer: The peer from which the original message was received
 * @msg: Parsed NAN Action frame
 * Returns: 0 on success processing indicating that processing can continue; 1
 * in case of successful processing but no further processing is needed;
 * negative on failure.
 */
int nan_ndp_handle_ndp_attr(struct nan_data *nan, struct nan_peer *peer,
			    struct nan_msg *msg)
{
	struct ieee80211_ndp *ndp_attr = NULL;
	size_t ndp_attr_len = 0;
	bool ndpe_supported;
	u8 type, status;
	int ret;

	if (!msg || !peer || (!msg->attrs.ndp && !msg->attrs.ndpe))
		return -1;

	ndpe_supported = nan_is_ndpe_supported(nan, peer);
	wpa_printf(MSG_DEBUG,
		   "NAN: NDP: Handle NDP attribute. ndpe_supported=%u",
		   ndpe_supported);

	/*
	 * The NDP attribute and the NDPE attribute are very similar in
	 * structure so handle them in the same way where possible.
	 */
	if (ndpe_supported) {
		ndp_attr = (struct ieee80211_ndp *) msg->attrs.ndpe;
		ndp_attr_len = msg->attrs.ndpe_len;
	}

	if (!ndp_attr || !ndp_attr_len) {
		ndp_attr = (struct ieee80211_ndp *) msg->attrs.ndp;
		ndp_attr_len = msg->attrs.ndp_len;
		ndpe_supported = false;
		if (!ndp_attr || !ndp_attr_len)
			return -1;
	}

	type = BITS(ndp_attr->type_and_status, NAN_NDP_TYPE_MASK,
		    NAN_NDP_TYPE_POS);
	status = BITS(ndp_attr->type_and_status, NAN_NDP_STATUS_MASK,
		      NAN_NDP_STATUS_POS);

	if (peer->ndp_setup.ndp) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: attr: state=%s (%d), status=%d",
			   nan_ndp_state_str(peer->ndp_setup.state),
			   peer->ndp_setup.state,
			   peer->ndp_setup.status);
	} else {
		wpa_printf(MSG_DEBUG, "NAN: NDP: attr: no active NDP setup");
	}

	wpa_printf(MSG_DEBUG, "NAN: NDP: attr: type=0x%x, status=0x%x",
		   type, status);

	switch (type) {
	case NAN_NDP_TYPE_REQUEST:
		ret = nan_ndp_attr_handle_req(nan, peer, ndp_attr,
					      ndp_attr_len, status,
					      ndpe_supported);
		break;
	case NAN_NDP_TYPE_RESPONSE:
		ret = nan_ndp_attr_handle_res(nan, peer, ndp_attr,
					      ndp_attr_len, status,
					      ndpe_supported);
		break;
	case NAN_NDP_TYPE_CONFIRM:
		ret = nan_ndp_attr_handle_confirm(nan, peer, ndp_attr, status);
		break;
	case NAN_NDP_TYPE_SECURITY_INSTALL:
		ret = nan_ndp_attr_sec_install(nan, peer, ndp_attr, status);
		break;
	case NAN_NDP_TYPE_TERMINATE:
		return nan_ndp_attr_handle_term(nan, peer, ndp_attr, status);
	default:
		return -1;
	}

	/* Error or no security.. We are done. */
	if (ret || !peer->ndp_setup.sec.present ||
	    peer->ndp_setup.status == NAN_NDP_STATUS_REJECTED)
		return ret;

	ret = nan_sec_rx(nan, peer, msg);
	if (ret)
		return ret;

	/* Processing of confirm is successful, so to the overall status is
	 * success. */
	if (type == NAN_NDP_TYPE_CONFIRM)
		peer->ndp_setup.status = NAN_NDP_STATUS_ACCEPTED;

	return 0;
}


/**
 * nan_ndp_add_ndp_attr - Add NDP attribute to frame
 * @nan: NAN module context from nan_init()
 * @peer: The peer to which the NAF should be sent
 * @buf: wpabuf to which the attribute would be added
 * Returns: 0 on success, negative on failure
 */
int nan_ndp_add_ndp_attr(struct nan_data *nan, struct nan_peer *peer,
			 struct wpabuf *buf)
{
	struct nan_ndp_setup *ndp_setup;
	u8 type, ndp_ctrl = 0;
	u8 *len_ptr;
	bool ndpe_supported;
	bool add_srv_info = false;

	if (!peer || !peer->ndp_setup.ndp)
		return -1;

	ndpe_supported = nan_is_ndpe_supported(nan, peer);
	ndp_setup = &peer->ndp_setup;

	switch (ndp_setup->state) {
	case NAN_NDP_STATE_START:
		type = NAN_NDP_TYPE_REQUEST;
		ndp_ctrl = NAN_NDP_CTRL_PUBLISH_ID_PRESENT;
		if (ndp_setup->conf_req)
			ndp_ctrl |= NAN_NDP_CTRL_CONFIRM_REQUIRED;
		if (ndp_setup->ssi && ndp_setup->ssi_len) {
			add_srv_info = true;
			if (!ndpe_supported)
				ndp_ctrl |= NAN_NDP_CTRL_SPEC_INFO_PRESENT;
		}
		break;
	case NAN_NDP_STATE_REQ_RECV:
		type = NAN_NDP_TYPE_RESPONSE;
		if (ndp_setup->status != NAN_NDP_STATUS_REJECTED)
			ndp_ctrl |= NAN_NDP_CTRL_RESPONDER_NDI_PRESENT;
		if (ndp_setup->ssi && ndp_setup->ssi_len) {
			add_srv_info = true;
			if (!ndpe_supported)
				ndp_ctrl |= NAN_NDP_CTRL_SPEC_INFO_PRESENT;
		}
		break;
	case NAN_NDP_STATE_RES_RECV:
		type = NAN_NDP_TYPE_CONFIRM;
		break;
	case NAN_NDP_STATE_CON_RECV:
		/* TODO: Integrate NDP security flows */
		type = NAN_NDP_TYPE_SECURITY_INSTALL;
		break;
	case NAN_NDP_STATE_NONE:
	case NAN_NDP_STATE_DONE:
		if (ndp_setup->status == NAN_NDP_STATUS_REJECTED)
			type = NAN_NDP_TYPE_TERMINATE;
		else
			return -1;
		break;
	case NAN_NDP_STATE_REQ_SENT:
	case NAN_NDP_STATE_RES_SENT:
	case NAN_NDP_STATE_CON_SENT:
	default:
		return 0;
	}

	if (ndp_setup->sec.present)
		ndp_ctrl |= NAN_NDP_CTRL_SECURITY_PRESENT;

	/*
	 * The NDP attribute and the NDPE attribute are very similar in
	 * structure so handle them in the same way where possible.
	 */
	if (ndpe_supported)
		wpabuf_put_u8(buf, NAN_ATTR_NDP_EXT);
	else
		wpabuf_put_u8(buf, NAN_ATTR_NDP);

	len_ptr = wpabuf_put(buf, 2);

	wpabuf_put_u8(buf, ndp_setup->dialog_token);
	wpabuf_put_u8(buf, type |
		      (ndp_setup->status << NAN_NDP_STATUS_POS));
	wpabuf_put_u8(buf, ndp_setup->reason);
	wpabuf_put_data(buf, ndp_setup->ndp->init_ndi, ETH_ALEN);
	wpabuf_put_u8(buf, ndp_setup->ndp->ndp_id);
	wpabuf_put_u8(buf, ndp_ctrl);

	if (ndp_ctrl & NAN_NDP_CTRL_PUBLISH_ID_PRESENT)
		wpabuf_put_u8(buf, ndp_setup->publish_inst_id);

	if (ndp_ctrl & NAN_NDP_CTRL_RESPONDER_NDI_PRESENT)
		wpabuf_put_data(buf, ndp_setup->ndp->resp_ndi, ETH_ALEN);

	if (add_srv_info) {
		if (!ndpe_supported) {
			wpabuf_put_data(buf, ndp_setup->ssi,
					ndp_setup->ssi_len);
		} else {
			/*
			 * TODO: For the service specific info use the WFA
			 * format. If there is a need to support other vendor
			 * OUIs, this would need to be extended.
			 */
			wpabuf_put_u8(buf, NAN_NDPE_TLV_SRV_INFO);
			wpabuf_put_le16(buf, 4 + ndp_setup->ssi_len);
			wpabuf_put_be24(buf, OUI_WFA);
			wpabuf_put_u8(buf, NAN_SRV_PROTO_GENERIC);
			wpabuf_put_data(buf, ndp_setup->ssi,
					ndp_setup->ssi_len);
		}
	}

	if (ndpe_supported && ndp_setup->local_interface_id_valid) {
		wpabuf_put_u8(buf, NAN_NDPE_TLV_IPV6_LINK_LOCAL);
		wpabuf_put_le16(buf, NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN);
		wpabuf_put_data(buf, ndp_setup->local_interface_id,
				NAN_NDPE_TLV_IPV6_LINK_LOCAL_LEN);
	}

	WPA_PUT_LE16(len_ptr, (u8 *) wpabuf_put(buf, 0) - len_ptr - 2);
	return 0;
}


/**
 * nan_ndp_setup_reset - Reset the ndp_setup state
 * @nan: NAN module context from nan_init()
 * @peer: The peer that requires ndp setup reset
 */
void nan_ndp_setup_reset(struct nan_data *nan, struct nan_peer *peer)
{
	if (!peer)
		return;

	os_free(peer->ndp_setup.ssi);
	os_free(peer->ndp_setup.ndp);

	os_memset(&peer->ndp_setup, 0, sizeof(peer->ndp_setup));
	nan_ndp_set_state(nan, &peer->ndp_setup, NAN_NDP_STATE_NONE);
}


/**
 * nan_ndp_setup_failure - Indicate failure during NDP setup
 * @nan: NAN module context from nan_init()
 * @peer: The peer from which the original message was received
 * @reason: The failure reason
 * @reset_state: Reset the NDP state iff equals to true.
 */
void nan_ndp_setup_failure(struct nan_data *nan, struct nan_peer *peer,
			   enum nan_reason reason, bool reset_state)
{
	struct nan_ndp_setup *ndp_setup = &peer->ndp_setup;

	wpa_printf(MSG_DEBUG, "NAN: NDP: setup failure: peer " MACSTR
		   ". state=%s (%u). reason=%u",
		   MAC2STR(peer->nmi_addr), nan_ndp_state_str(ndp_setup->state),
		   ndp_setup->state, reason);

	if (reset_state) {
		nan_ndp_setup_reset(nan, peer);
	} else {
		ndp_setup->status = NAN_NDP_STATUS_REJECTED;
		ndp_setup->reason = reason;
	}
}


/**
 * nan_ndp_naf_sent - Indicate a NAF has been sent
 * @nan: NAN module context from nan_init()
 * @peer: The peer with whom the NDP is being setup
 * @subtype: The NAN OUI subtype. See &enum nan_subtype
 *
 * Notification, indicating to the NDP SM to that a NAF was sent, so the
 * NDP SM could update its state.
 */
int nan_ndp_naf_sent(struct nan_data *nan, struct nan_peer *peer,
		     enum nan_subtype subtype)
{
	struct nan_ndp_setup *ndp_setup;

	if (!peer || !peer->ndp_setup.ndp)
		return -1;

	ndp_setup = &peer->ndp_setup;

	wpa_printf(MSG_DEBUG, "NAN: NDP: Tx done: state=%s (%d), status=%d",
		   nan_ndp_state_str(ndp_setup->state),
		   ndp_setup->state, ndp_setup->status);

	/*
	 * Note: Due to races between the Tx status and Rx path, it is possible
	 * that the Tx status is received after the peer response was already
	 * processed (which can result with another frame being sent). In such a
	 * case the logic above fast-forwards the state, and the transitions
	 * here need to take this into consideration.
	 */
	switch (ndp_setup->state) {
	case NAN_NDP_STATE_START:
		if (subtype == NAN_SUBTYPE_DATA_PATH_REQUEST)
			nan_ndp_set_state(nan, &peer->ndp_setup,
					  NAN_NDP_STATE_REQ_SENT);
		break;
	case NAN_NDP_STATE_REQ_RECV:
		if (subtype == NAN_SUBTYPE_DATA_PATH_RESPONSE) {
			if (ndp_setup->status == NAN_NDP_STATUS_ACCEPTED)
				nan_ndp_set_state(nan, &peer->ndp_setup,
						  NAN_NDP_STATE_DONE);
			else
				nan_ndp_set_state(nan, &peer->ndp_setup,
						  NAN_NDP_STATE_RES_SENT);
		}
		break;
	case NAN_NDP_STATE_RES_RECV:
		if (subtype == NAN_SUBTYPE_DATA_PATH_CONFIRM) {
			if (ndp_setup->status == NAN_NDP_STATUS_ACCEPTED)
				nan_ndp_set_state(nan, &peer->ndp_setup,
						  NAN_NDP_STATE_DONE);
			else
				nan_ndp_set_state(nan, &peer->ndp_setup,
						  NAN_NDP_STATE_CON_SENT);
		}
		break;
	case NAN_NDP_STATE_CON_RECV:
		if (subtype == NAN_SUBTYPE_DATA_PATH_KEY_INSTALL &&
		    ndp_setup->status == NAN_NDP_STATUS_ACCEPTED)
			nan_ndp_set_state(nan, &peer->ndp_setup,
					  NAN_NDP_STATE_DONE);
		break;
	case NAN_NDP_STATE_REQ_SENT:
	case NAN_NDP_STATE_RES_SENT:
	case NAN_NDP_STATE_CON_SENT:
	case NAN_NDP_STATE_DONE:
	default:
		break;
	}

	return 0;
}


/*
 * nan_ndp_term_req - Handle local NDP termination request
 * @nan: NAN module context from nan_init()
 * @peer: The peer with whom the NDP is being setup
 * @ndp_id: NDP identifier
 * Returns: 0 on success, -1 on failure
 */
int nan_ndp_term_req(struct nan_data *nan, struct nan_peer *peer,
		     struct nan_ndp_id *ndp_id)
{
	struct nan_ndp_setup *ndp_setup = &peer->ndp_setup;
	struct nan_ndp *pndp;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDP: Terminate request with peer=" MACSTR
		   " ndp_id=%u, init_ndi=" MACSTR,
		   MAC2STR(peer->nmi_addr), ndp_id->id,
		   MAC2STR(ndp_id->init_ndi));

	if (ndp_setup->ndp) {
		if (ndp_setup->ndp->ndp_id == ndp_id->id &&
		    ether_addr_equal(ndp_setup->ndp->init_ndi,
				     ndp_id->init_ndi)) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDP: WIP with peer. Terminate");

			nan_ndp_set_state(nan, &peer->ndp_setup,
					  NAN_NDP_STATE_DONE);
			ndp_setup->status = NAN_NDP_STATUS_REJECTED;
			ndp_setup->reason = NAN_REASON_UNSPECIFIED_REASON;
			return 0;
		}

		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Cannot terminate NDP while NDP establishment is WIP");
		return -1;
	}

	/* Find the NDP in the list of active NDPs */
	pndp = nan_ndp_find_ndp(peer, ndp_id->id, ndp_id->init_ndi);
	if (!pndp) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: Termination request for unknown NDP");
		return -1;
	}

	/* Remove the NDP from the list and setup data for the termination */
	dl_list_del(&pndp->list);

	peer->ndp_setup.ndp = pndp;
	peer->ndp_setup.status = NAN_NDP_STATUS_REJECTED;
	peer->ndp_setup.reason = NAN_REASON_UNSPECIFIED_REASON;
	return 0;
}


/**
 * nan_ndp_requested_gtk_csid - Get the GTK CSID requested by peer for NDP setup
 * @nan: NAN module context from nan_init()
 * @ndp_id: NDP identifier
 * Returns: The GTK CSID requested by peer, or NAN_CS_NONE if no matching NDP is
 *	found or GTK is not requested by peer.
 */
int nan_ndp_requested_gtk_csid(struct nan_data *nan,
			       const struct nan_ndp_id *ndp_id)
{
	struct nan_peer *peer;

	peer = nan_get_peer(nan, ndp_id->peer_nmi);
	if (!peer) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: No matching peer found for GTK CSID request");
		return NAN_CS_NONE;
	}

	if (!peer->ndp_setup.ndp ||
	    peer->ndp_setup.ndp->ndp_id != ndp_id->id ||
	    !ether_addr_equal(peer->ndp_setup.ndp->init_ndi,
			      ndp_id->init_ndi)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDP: No matching NDP found for GTK CSID request");
		return NAN_CS_NONE;
	}

	if (peer->ndp_setup.state != NAN_NDP_STATE_REQ_RECV)
		return NAN_CS_NONE;

	return peer->ndp_setup.sec.peer_gtk.csid;
}
