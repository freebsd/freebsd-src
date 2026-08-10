/*
 * Wi-Fi Aware - NAN Data link
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"
#include "utils/bitfield.h"
#include "common/ieee802_11_common.h"
#include "nan_i.h"


struct ndl_attr_params {
	u8 dialog_token;
	u8 type;
	u8 status;
	u8 reason;
	enum nan_ndl_setup_reason setup_reason;

	u16 max_idle_period;
	u8 min_slots;
	u16 max_latency;

	u8 ndc_id[ETH_ALEN];
	const u8 *ndc_sched;
	u16 ndc_sched_len;

	const u8 *immut_sched; u16 immut_sched_len;
};


static const char * nan_ndl_state_str(enum nan_ndl_state state)
{
#define C2S(x) case x: return #x;
	switch (state) {
	C2S(NAN_NDL_STATE_NONE)
	C2S(NAN_NDL_STATE_START)
	C2S(NAN_NDL_STATE_REQ_SENT)
	C2S(NAN_NDL_STATE_REQ_RECV)
	C2S(NAN_NDL_STATE_RES_SENT)
	C2S(NAN_NDL_STATE_RES_RECV)
	C2S(NAN_NDL_STATE_CON_SENT)
	C2S(NAN_NDL_STATE_CON_RECV)
	C2S(NAN_NDL_STATE_DONE)
	default:
		return "Invalid NAN NDL state";
	}
}


static void nan_ndl_set_state(struct nan_data *nan, struct nan_ndl *ndl,
			      enum nan_ndl_state state)
{
	wpa_printf(MSG_DEBUG, "NAN: NDL: State %s (%u) --> %s (%u)",
		   nan_ndl_state_str(ndl->state), ndl->state,
		   nan_ndl_state_str(state), state);

	ndl->state = state;
}


static void nan_ndl_time_bitmap_print(struct nan_data *nan,
				      struct nan_time_bitmap *tbm,
				      const char *type)
{
	if (!tbm->len)
		return;

	wpa_printf(MSG_DEBUG,
		   "channel sched: %s: dur=%u, per=%u, off=%u, len=%u",
		   type, tbm->duration, tbm->period, tbm->offset, tbm->len);

	wpa_printf(MSG_DEBUG, "bitmap[0:3]: 0x%x:0x%x:0x%x:0x%x",
		   tbm->bitmap[0], tbm->bitmap[1],
		   tbm->bitmap[2], tbm->bitmap[3]);
}


static void nan_ndl_chan_sched_print(struct nan_data *nan, size_t idx,
				     struct nan_chan_schedule *cs)
{
	wpa_printf(MSG_DEBUG, "NAN: sched: index=%zu, map_id=%u",
		   idx, cs->map_id);

	wpa_printf(MSG_DEBUG,
		   "channel sched: chan: freq=%u, c1=%d, c2=%d, bandwidth=%d",
		   cs->chan.freq, cs->chan.center_freq1, cs->chan.center_freq2,
		   cs->chan.bandwidth);

	nan_ndl_time_bitmap_print(nan, &cs->committed, "committed");
	nan_ndl_time_bitmap_print(nan, &cs->conditional, "conditional");
}


static void nan_ndl_sched_print(struct nan_data *nan,
				struct nan_schedule *sched)
{
	size_t i;

	wpa_printf(MSG_DEBUG,
		   "NAN: sched: map_ids=0x%x, n_chans=%u, seq_id=%u",
		   sched->map_ids_bitmap,
		   sched->n_chans, sched->sequence_id);

	for (i = 0; i < sched->n_chans; i++)
		nan_ndl_chan_sched_print(nan, i, &sched->chans[i]);
}


static void nan_ndl_clear(struct nan_data *nan, struct nan_peer *peer)
{
	struct nan_ndl *ndl = peer->ndl;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: Clear info for peer=" MACSTR " state=%s (%u)",
		   MAC2STR(peer->nmi_addr),
		   nan_ndl_state_str(peer->ndl->state), peer->ndl->state);

	nan_clear_peer_schedule(nan, peer);

	os_free(ndl->ndc_sched);
	ndl->ndc_sched = NULL;
	ndl->ndc_sched_len = 0;

	os_free(ndl->immut_sched);
	ndl->immut_sched = NULL;
	ndl->immut_sched_len = 0;

	ndl->dialog_token = 0;
	ndl->max_idle_period = 0;

	os_memset(ndl->ndc_id, 0, sizeof(ndl->ndc_id));

	ndl->peer_qos.max_latency = NAN_QOS_MAX_LATENCY_NO_PREF;
	ndl->local_qos.max_latency = NAN_QOS_MAX_LATENCY_NO_PREF;

	ndl->setup_reason = NAN_NDL_SETUP_REASON_NONE;
}


/*
 * nan_ndl_reset - Reset the NDL state
 *
 * @nan: NAN module context from nan_init()
 * @peer: The peer that requires NDL setup reset
 */
void nan_ndl_reset(struct nan_data *nan, struct nan_peer *peer)
{
	wpa_printf(MSG_DEBUG, "NAN: NDL: Reset state for peer=" MACSTR,
		   MAC2STR(peer->nmi_addr));

	if (!peer->ndl) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Reset: no NDL");
		return;
	}

	nan_ndl_clear(nan, peer);

	os_free(peer->ndl);
	peer->ndl = NULL;
}


static struct nan_ndl * nan_ndl_alloc(struct nan_data *nan)
{
	struct nan_ndl *ndl;

	wpa_printf(MSG_DEBUG, "NAN: NDL: Allocating a new NDL");

	ndl = os_zalloc(sizeof(struct nan_ndl));
	if (!ndl) {
		wpa_printf(MSG_INFO, "NAN: NDL: Failed to allocate NDL");
		return NULL;
	}

	ndl->status = NAN_NDL_STATUS_ACCEPTED;
	ndl->reason = NAN_REASON_RESERVED;

	ndl->local_qos.min_slots = NAN_QOS_MIN_SLOTS_NO_PREF;
	ndl->local_qos.max_latency = NAN_QOS_MAX_LATENCY_NO_PREF;

	ndl->peer_qos.min_slots = NAN_QOS_MIN_SLOTS_NO_PREF;
	ndl->peer_qos.max_latency = NAN_QOS_MAX_LATENCY_NO_PREF;

	nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_NONE);

	return ndl;
}


bool nan_ndl_validate_peer_avail(struct nan_data *nan, struct nan_peer *peer)
{
	struct nan_ndl *ndl = peer->ndl;
	bool ret;

	/* First, validate if immutable is covered by the availability map */
	ret = nan_sched_covered_by_avail_entries(nan, &peer->info.avail_entries,
						 ndl->immut_sched,
						 ndl->immut_sched_len);
	if (!ret) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Peer avail: Immutable is not covered by avail");
		peer->ndl->reason = NAN_REASON_IMMUTABLE_UNACCEPTABLE;
		return ret;
	}

	/* Now validate NDC schedule is covered by the availability map */
	ret = nan_sched_covered_by_avail_entries(nan, &peer->info.avail_entries,
						 ndl->ndc_sched,
						 ndl->ndc_sched_len);
	if (!ret) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Peer avail: NDC is not covered by avail");
		peer->ndl->reason = NAN_REASON_NDL_UNACCEPTABLE;
		return ret;
	}

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: Peer NDC and immutable are covered by avail");

	return ret;
}


/**
 * enum nan_ndl_ver - Verdict of comparing local availability to given schedule
 * @NAN_NDL_VER_SCHED_SUBSET_OF_LOCAL: The schedule is a subset of the local one
 * @NAN_NDL_VER_SCHED_SUPERSET_OF_LOCAL: The schedule is a superset of the local
 *     one
 * @NAN_NDL_VER_SCHED_IDENTICAL_OF_LOCAL: The schedule is identical to the local
 * @NAN_NDL_VER_SCHED_NONE: None of the above. This means that there was no
 *     match or an error occurred.
 */
enum nan_ndl_ver {
	NAN_NDL_VER_SCHED_SUBSET_OF_LOCAL,
	NAN_NDL_VER_SCHED_SUPERSET_OF_LOCAL,
	NAN_NDL_VER_SCHED_IDENTICAL_OF_LOCAL,
	NAN_NDL_VER_SCHED_NONE,
};


/*
 * nan_ndl_match_sched_vs_common - Compare the given schedule to the common
 * availability of the local device and the peer device, comparing channel
 * configuration and bitmap.
 *
 * @nan: NAN module context from nan_init()
 * @avail_entries: Peer availability entries
 * @sched: A byte array with 0 or more &struct nan_sched_entry entries
 * @sched_len: Length of the &sched array
 * @common_bf: Bitfield representing the intersection of local and peer
 *     availability
 * @op_class: Local operating class
 * @cbm: Local channel bitmap
 *
 * Returns one of enum nan_ndl_ver. In case of on an error
 * NAN_NDL_VER_SCHED_NONE is returned.
 *
 * The function first verifies that the given schedule is covered by the peer
 * availability. Then, the function verifies that the schedule is covered by
 * the common availability between local and peer.
 *
 * The function can (and should be used) to check the schedule constraints of
 * an NDC schedule and/or immutable schedule.
 *
 * Note: In case that &sched is NULL or &sched_len is 0, returns
 * NAN_NDL_VER_SCHED_IDENTICAL_OF_LOCAL, meaning no constraints.
 */
static enum nan_ndl_ver
nan_ndl_match_sched_vs_common(struct nan_data *nan,
			      struct dl_list *avail_entries,
			      const u8 *sched, size_t sched_len,
			      const struct bitfield *common_bf,
			      u8 op_class, u16 cbm)
{
	struct dl_list sched_entries;
	struct bitfield *sched_bf;
	u8 map_id;
	enum nan_ndl_ver verdict;
	int ret;
	enum nan_reason reason;

	/* No constraints */
	if (!sched || !sched_len)
		return NAN_NDL_VER_SCHED_IDENTICAL_OF_LOCAL;

	/* Convert the schedule entries to availability entries  */
	ret = nan_sched_entries_to_avail_entries(nan, &sched_entries,
						 sched, sched_len);
	if (ret)
		return NAN_NDL_VER_SCHED_NONE;

	/* Convert the schedule availability entries to map ID and bitfield */
	sched_bf = nan_sched_to_bf(nan, &sched_entries, &map_id, &reason);
	nan_flush_avail_entries(&sched_entries);

	/*
	 * Now that the schedule is represented as a bitfield and the map ID is
	 * obtained, compare these against the peer availability entries and
	 * channel configuration. A successful match means that the schedule is
	 * covered by the peer availability entries for the given channel.
	 */
	ret = nan_sched_bf_covered_by_avail_entries_and_chan(
		nan, avail_entries, sched_bf, map_id, op_class, cbm);
	if (ret) {
		int ret2;

		/*
		 * After validating the schedule against the peer availability,
		 * match the peer availability with the local one.
		 */
		ret = bitfield_is_subset(common_bf, sched_bf);
		ret2 = bitfield_is_subset(sched_bf, common_bf);

		if (ret < 0 || ret2 < 0)
			verdict = NAN_NDL_VER_SCHED_NONE;
		else if (ret == 1 && ret2 == 1)
			verdict = NAN_NDL_VER_SCHED_IDENTICAL_OF_LOCAL;
		else if (ret == 1)
			verdict = NAN_NDL_VER_SCHED_SUBSET_OF_LOCAL;
		else
			verdict = NAN_NDL_VER_SCHED_SUPERSET_OF_LOCAL;
	} else {
		verdict = NAN_NDL_VER_SCHED_NONE;
	}

	bitfield_free(sched_bf);
	return verdict;
}


bool nan_ndl_meets_qos(struct nan_data *nan, const struct nan_peer *peer,
		       const struct bitfield *common_bf)
{
	size_t size, max_latency, i;
	u16 crbs;

	/* No QoS requirements */
	if (peer->ndl->peer_qos.min_slots == NAN_QOS_MIN_SLOTS_NO_PREF &&
	    peer->ndl->peer_qos.max_latency == NAN_QOS_MAX_LATENCY_NO_PREF) {
		wpa_printf(MSG_DEBUG, "NAN: No QoS requirements from peer");
		return true;
	}

	size = bitfield_size(common_bf);

	/*
	 * The common map covers an entire 8192 period with 16 TU slots. For
	 * minimal time slots need to only consider the first 32 slots
	 */
	for (i = 0, crbs = 0, max_latency = 0; i < size; i++) {
		if (bitfield_is_set(common_bf, i)) {
			if (i < 32)
				crbs++;

			max_latency = 0;
		} else if (peer->ndl->peer_qos.max_latency !=
			   NAN_QOS_MAX_LATENCY_NO_PREF) {
			max_latency++;
			if (max_latency > peer->ndl->peer_qos.max_latency) {
				wpa_printf(MSG_DEBUG,
					   "NAN: Failed to meet max latency");
				return false;
			}
		}
	}

	if (peer->ndl->peer_qos.min_slots != NAN_QOS_MIN_SLOTS_NO_PREF &&
	    peer->ndl->peer_qos.min_slots >= crbs) {
		wpa_printf(MSG_DEBUG, "NAN: Failed to meet min slots");
		return false;
	}

	return true;
}


static enum nan_ndl_status nan_ndl_determine_status(struct nan_data *nan,
						    struct nan_peer *peer,
						    bool can_counter,
						    enum nan_reason *reason)
{
	struct nan_schedule *sched = &nan->sched;
	struct bitfield *common_bf = NULL, *ndc_bf = NULL, *track_ndc_bf = NULL;
	enum nan_ndl_ver verdict;
	size_t i;
	int ret;

	*reason = NAN_REASON_RESERVED;

	ndc_bf = nan_tbm_to_bf(nan, &sched->ndc);
	if (!ndc_bf) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Failed to build NDC bitfield from schedule");

		*reason = NAN_REASON_UNSPECIFIED_REASON;
		return NAN_NDL_STATUS_REJECTED;
	}

	track_ndc_bf = bitfield_alloc(bitfield_size(ndc_bf));
	if (!track_ndc_bf) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Failed to allocate bitfield for tracking NDC");

		bitfield_free(ndc_bf);
		*reason = NAN_REASON_UNSPECIFIED_REASON;
		return NAN_NDL_STATUS_REJECTED;
	}

	/*
	 * Iterate over all the channels included in the local schedule. For
	 * each channel, convert the committed and conditional slots to a
	 * bitfield object and extract the operating class and channel bitmap.
	 *
	 * Using the operating class and channel bitmap find the peer
	 * availability on that channel and intersect it with the local one:
	 *
	 * - Accumulate the intersection of the NDC bitmap with the bitmap of
	 *   all channels with the same map ID as the NDC, so that whether the
	 *   NDC is covered by the local availability can be verified later.
	 * - Accumulate the intersection of local and peer availability for
	 *   all channels, so that the QoS requirements can be verified later.
	 */
	wpa_printf(MSG_DEBUG, "NAN: NDL: n_chans=%u, ndc_map_id=%u",
		   sched->n_chans, sched->ndc_map_id);

	for (i = 0; i < sched->n_chans; i++) {
		struct bitfield *own_chan_bf = NULL, *peer_chan_bf = NULL;
		u16 cbm, pri_cbm;
		u8 map_id, op_class;

		/* Convert the schedule for the current channel to bitfield */
		ret = nan_convert_chan_sched_to_bf(nan, &sched->chans[i],
						   &own_chan_bf, &map_id,
						   &op_class, &cbm, &pri_cbm);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Failed to convert chan sched to bitfield");

			*reason = NAN_REASON_UNSPECIFIED_REASON;
			ret = NAN_NDL_STATUS_REJECTED;
			goto out;
		}

		if (sched->ndc_map_id == map_id) {
			struct bitfield *tmp = bitfield_dup(ndc_bf);

			if (tmp) {
				bitfield_intersect_in_place(tmp, own_chan_bf);
				bitfield_union_in_place(track_ndc_bf, tmp);
				bitfield_free(tmp);
			}
		}

		/* Get the peer availability for the current channel */
		peer_chan_bf = nan_avail_entries_to_bf(
			nan, &peer->info.avail_entries,
			op_class, cbm, pri_cbm);
		if (!peer_chan_bf) {
			bitfield_free(own_chan_bf);
			continue;
		}

		ret = bitfield_intersect_in_place(own_chan_bf, peer_chan_bf);
		bitfield_free(peer_chan_bf);
		peer_chan_bf = NULL;

		if (ret < 0) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Failed to intersect own and peer chan bitfields");

			bitfield_free(own_chan_bf);

			*reason = NAN_REASON_INVALID_AVAILABILITY;
			ret = NAN_NDL_STATUS_REJECTED;
			goto out;
		}

		/*
		 * Accumulate schedule common for the local and peer device, so
		 * it can later be used to verify QoS requirements, etc.
		 */
		if (common_bf) {
			ret = bitfield_union_in_place(common_bf, own_chan_bf);
			if (ret) {
				wpa_printf(MSG_DEBUG,
					   "NAN: NDL: Failed to unify own chan bitfields");

				bitfield_free(own_chan_bf);

				*reason = NAN_REASON_UNSPECIFIED_REASON;
				ret = NAN_NDL_STATUS_REJECTED;
				goto out;
			}
		} else {
			common_bf = bitfield_dup(own_chan_bf);
			if (!common_bf) {
				wpa_printf(MSG_DEBUG,
					   "NAN: NDL: Failed to dup own chan bitfield");

				bitfield_free(own_chan_bf);

				*reason = NAN_REASON_UNSPECIFIED_REASON;
				ret = NAN_NDL_STATUS_REJECTED;
				goto out;
			}
		}

		bitfield_free(own_chan_bf);
		own_chan_bf = NULL;
	}

	if (!common_bf) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: No common availability between local and peer");

		*reason = NAN_REASON_INVALID_AVAILABILITY;
		ret = NAN_NDL_STATUS_CONTINUED;
		goto out;
	}

	/*
	 * Verify that the schedule NDC bitmap is covered by the local
	 * availability for the map used for the NDC.
	 */
	if (!bitfield_is_subset(ndc_bf, track_ndc_bf)) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: NDC bitmap is not covered by local availability on NDC channel");

		*reason = NAN_REASON_UNSPECIFIED_REASON;
		ret = NAN_NDL_STATUS_REJECTED;
		goto out;
	}

	bitfield_free(ndc_bf);
	ndc_bf = track_ndc_bf;
	track_ndc_bf = NULL;

	/*
	 * In case the peer included an immutable schedule, the immutable
	 * schedule must be covered by the common schedule. If not, reject.
	 */
	verdict = nan_ndl_match_sched_vs_common(nan,
						&peer->info.avail_entries,
						peer->ndl->immut_sched,
						peer->ndl->immut_sched_len,
						common_bf, 0, 0);
	if (verdict != NAN_NDL_VER_SCHED_IDENTICAL_OF_LOCAL &&
	    verdict != NAN_NDL_VER_SCHED_SUBSET_OF_LOCAL) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Schedule does not cover immutable. Reject");

		*reason = NAN_REASON_IMMUTABLE_UNACCEPTABLE;
		ret = NAN_NDL_STATUS_REJECTED;
		goto out;
	}

	/*
	 * In case the peer included an NDC, check if it is identical to
	 * the locally generated one.
	 *
	 * Note: The list of peer availability entries needs to be iterated
	 * again, as the NDC map ID must also be matched.
	 */
	verdict = nan_ndl_match_sched_vs_common(nan, &peer->info.avail_entries,
						peer->ndl->ndc_sched,
						peer->ndl->ndc_sched_len,
						ndc_bf, 0, 0);
	if (verdict != NAN_NDL_VER_SCHED_IDENTICAL_OF_LOCAL) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Response NDC does not match req NDC");

		*reason = NAN_REASON_INVALID_AVAILABILITY;
		ret = NAN_NDL_STATUS_CONTINUED;
		goto out;
	}

	if (nan_ndl_meets_qos(nan, peer, common_bf)) {
		wpa_printf(MSG_DEBUG, "NAN: NDL QoS requirements met. Accept");
		ret = NAN_NDL_STATUS_ACCEPTED;
	} else {
		wpa_printf(MSG_DEBUG, "NAN: NDL QoS requirements not met");
		*reason = NAN_REASON_QOS_UNACCEPTABLE;
		ret = NAN_NDL_STATUS_CONTINUED;
	}

out:
	bitfield_free(common_bf);
	bitfield_free(ndc_bf);
	bitfield_free(track_ndc_bf);

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: Response status=%s (%u)",
		   ret == NAN_NDL_STATUS_ACCEPTED ? "ACCEPTED" :
		   ret == NAN_NDL_STATUS_CONTINUED ? "CONTINUED" :
		   "REJECTED",
		   ret);

	if (ret == NAN_NDL_STATUS_CONTINUED && !can_counter) {
		wpa_printf(MSG_DEBUG,
			   "NAN: Cannot counter, change verdict to REJECTED");
		ret = NAN_NDL_STATUS_REJECTED;
	}

	return ret;
}


/*
 * nan_ndl_setup - Handle NDL setup either as an initiator or a responder
 *
 * @nan: NAN module context from nan_init()
 * @peer: The peer for which the NDL is being setup
 * @params: NDP setup request parameters
 * @dialog_token: Dialog token to be used for the NDL setup messages. Should be
 *      used for a new NDL only.
 * Returns: 0 on success, negative on failure.

 * It is possible that an NDL with the peer already exists in which case it
 * would be reused. Otherwise, new NDL establishment will be started.
 */
int nan_ndl_setup(struct nan_data *nan, struct nan_peer *peer,
		  const struct nan_ndp_params *params,
		  u8 dialog_token)
{
	struct nan_ndl *ndl;
	enum nan_reason reason;

	if (!peer->ndl) {
		if (params->type != NAN_NDP_ACTION_REQ) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Invalid action; expecting request");
			return -1;
		}

		peer->ndl = nan_ndl_alloc(nan);
		if (!peer->ndl)
			return -1;

		ndl = peer->ndl;
	} else {
		ndl = peer->ndl;

		ndl->send_naf_on_error = 0;

		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: peer=" MACSTR ". state=%s (%u)",
			   MAC2STR(peer->nmi_addr),
			   nan_ndl_state_str(ndl->state),
			   ndl->state);

		if (ndl->state == NAN_NDL_STATE_DONE) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Already established");
			return 0;
		}

		if (!((params->type == NAN_NDP_ACTION_RESP &&
		       ndl->state == NAN_NDL_STATE_REQ_RECV) ||
		      (params->type == NAN_NDP_ACTION_CONF &&
		       ndl->state == NAN_NDL_STATE_RES_RECV))) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Invalid action type=%u, state=%u",
				   params->type, ndl->state);
			return -1;
		}

		if (params->u.resp.status == NAN_NDP_STATUS_REJECTED) {
			reason = params->u.resp.reason_code;
			goto out_fail;
		}
	}

	if (!params->sched_valid) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: no valid schedule");
		reason = NAN_REASON_INVALID_PARAMETERS;
		goto out_fail;
	}

	wpabuf_free(nan->sched.elems);
	os_memcpy(&nan->sched, &params->sched, sizeof(nan->sched));
	nan_ndl_sched_print(nan, &nan->sched);

	/* Copy elems buffer */
	if (params->sched.elems) {
		nan->sched.elems =
			wpabuf_alloc_copy(wpabuf_head(params->sched.elems),
					  wpabuf_len(params->sched.elems));
		if (!nan->sched.elems) {
			reason = NAN_REASON_UNSPECIFIED_REASON;
			goto out_fail;
		}
	}

	nan_ndl_sched_print(nan, &nan->sched);

	if (is_zero_ether_addr(ndl->ndc_id)) {
		if (os_get_random(ndl->ndc_id, ETH_ALEN) < 0) {
			reason = NAN_REASON_UNSPECIFIED_REASON;
			goto out_fail;
		}
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: generated NDC ID " MACSTR,
			   MAC2STR(ndl->ndc_id));
	}

	ndl->local_qos.min_slots = params->qos.min_slots;
	ndl->local_qos.max_latency = params->qos.max_latency;

	if (ndl->state == NAN_NDL_STATE_NONE) {
		ndl->dialog_token = dialog_token;
		nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_START);
		ndl->status = NAN_NDL_STATUS_CONTINUED;
	} else {
		ndl->status = nan_ndl_determine_status(nan, peer,
						       ndl->state ==
						       NAN_NDL_STATE_REQ_RECV,
						       &reason);
		if (ndl->status == NAN_NDL_STATUS_REJECTED)
			goto out_fail;
	}

	ndl->setup_reason = NAN_NDL_SETUP_REASON_NDP;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: success: state=%s (%u). Dialog token=%u",
		   nan_ndl_state_str(ndl->state), ndl->state,
		   ndl->dialog_token);

	return 0;

out_fail:
	wpa_printf(MSG_DEBUG, "NAN: NDL: Failed. reason=%u", reason);
	if (ndl->state == NAN_NDL_STATE_REQ_RECV ||
	    ndl->state == NAN_NDL_STATE_RES_RECV) {
		ndl->status = NAN_NDL_STATUS_REJECTED;
		ndl->reason = reason;
		ndl->send_naf_on_error = 1;

		/*
		 * Do not modify the state. Full cleanup will be done on Tx
		 * status handling.
		 */
	} else {
		nan_ndl_clear(nan, peer);
	}

	return -1;
}


/**
 * nan_ndl_setup_failure - Indicate failure during NDL setup
 * @nan: NAN module context from nan_init()
 * @peer: NAN peer
 * @reason: Failure reason
 * @reset_state: Reset the NDL state if true.
 */
void nan_ndl_setup_failure(struct nan_data *nan, struct nan_peer *peer,
			   enum nan_reason reason, bool reset_state)
{
	struct nan_ndl *ndl = peer->ndl;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: Setup failure: peer " MACSTR
		   ". state=%s (%u). reason=%u",
		   MAC2STR(peer->nmi_addr), nan_ndl_state_str(ndl->state),
		   ndl->state, reason);

	if (reset_state) {
		nan_ndl_reset(nan, peer);
	} else {
		ndl->status = NAN_NDL_STATUS_REJECTED;
		ndl->reason = reason;
	}
}


static int nan_ndl_parse_ndc_attr(struct nan_data *nan,
				  const struct ieee80211_ndc *ndc_attr,
				  u16 ndc_attr_len, u8 *ndc_id,
				  const u8 **ndc_schedule,
				  u16 *ndc_schedule_len)
{
	u16 ext_ndc_len;

	/* Consider only the selected NDC */
	if (!(ndc_attr->ctrl & NAN_NDC_CTRL_SELECTED))
		return -1;

	if (ndc_attr_len < sizeof(*ndc_attr))
		return -1;
	ext_ndc_len = ndc_attr_len - sizeof(*ndc_attr);

	if (ext_ndc_len <= sizeof(struct nan_sched_entry)) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Request with invalid len=%u",
			   ndc_attr_len);
		return -1;
	}

	os_memcpy(ndc_id, ndc_attr->ndc_id, sizeof(ndc_attr->ndc_id));
	*ndc_schedule_len = ext_ndc_len;
	*ndc_schedule = (const u8 *) (ndc_attr + 1);

	wpa_printf(MSG_DEBUG, "NAN: NDL: ndc_id=" MACSTR " schedule_len=%u",
		   MAC2STR(ndc_id), *ndc_schedule_len);
	return 0;
}


static int nan_ndl_parse_qos_attr(struct nan_data *nan,
				  const struct ieee80211_nan_qos *qos_attr,
				  u16 qos_attr_len,
				  u8 *min_slots, u16 *max_latency)
{
	*min_slots = qos_attr->min_slots;
	*max_latency = le_to_host16(qos_attr->max_latency);

	wpa_printf(MSG_DEBUG, "NAN: QoS attr: min_slots=%u, max_latency=%u",
		   *min_slots, *max_latency);
	return 0;
}


static int nan_ndl_attr_handle_req(struct nan_data *nan, struct nan_peer *peer,
				   const struct ndl_attr_params *params)
{
	struct nan_ndl *ndl;
	int ret;

	wpa_printf(MSG_DEBUG, "NAN: NDL: Handle request");

	if (peer->ndl) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Request while another establishment is ongoing");
		return -1;
	}

	if (params->status != NAN_NDL_STATUS_CONTINUED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Request with invalid status=%u",
			   params->status);
		return -1;
	}

	peer->ndl = nan_ndl_alloc(nan);
	if (!peer->ndl)
		return -1;

	ndl = peer->ndl;

	ndl->status = NAN_NDL_STATUS_CONTINUED;

	ndl->dialog_token = params->dialog_token;
	ndl->max_idle_period = params->max_idle_period;
	ndl->setup_reason = params->setup_reason;

	if (!is_zero_ether_addr(params->ndc_id)) {
		os_memcpy(ndl->ndc_id, params->ndc_id, sizeof(ndl->ndc_id));

		if (params->ndc_sched && params->ndc_sched_len) {
			os_free(ndl->ndc_sched);
			ndl->ndc_sched_len = 0;

			ndl->ndc_sched = os_memdup(params->ndc_sched,
						   params->ndc_sched_len);
			if (!ndl->ndc_sched) {
				wpa_printf(MSG_INFO,
					   "NAN: NDL: Failed to copy NDC schedule");
				goto fail;
			}

			ndl->ndc_sched_len = params->ndc_sched_len;
		}
	}

	ndl->peer_qos.min_slots = params->min_slots;
	ndl->peer_qos.max_latency = params->max_latency;

	if (params->immut_sched && params->immut_sched_len) {
		ndl->immut_sched = os_memdup(params->immut_sched,
					     params->immut_sched_len);
		if (!ndl->immut_sched) {
			wpa_printf(MSG_INFO,
				   "NAN: NDL: Failed to copy immutable schedule");
			goto fail;
		}
		ndl->immut_sched_len = params->immut_sched_len;
	}

	ret = nan_ndl_validate_peer_avail(nan, peer);
	if (!ret) {
		ndl->status = NAN_NDL_STATUS_REJECTED;
		ndl->send_naf_on_error = 1;
	}

	nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_REQ_RECV);

	wpa_printf(MSG_DEBUG, "NAN: NDL: Handle request done");
	return 0;

fail:
	nan_ndl_reset(nan, peer);
	return -1;
}


static int nan_ndl_attr_handle_resp(struct nan_data *nan, struct nan_peer *peer,
				    const struct ndl_attr_params *params)
{
	struct nan_ndl *ndl = peer->ndl;
	int ret;

	wpa_printf(MSG_DEBUG, "NAN: NDL: Handle response");

	if (!ndl) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Unexpected response");
		return -1;
	}

	if (ndl->state != NAN_NDL_STATE_REQ_SENT) {
		if (ndl->state != NAN_NDL_STATE_START) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Response while state=%s (%u)",
				   nan_ndl_state_str(ndl->state), ndl->state);
			return -1;
		}

		/*
		 * Due to races with the driver, it is possible that the
		 * response is received before an ACK is indicated. Allow the
		 * processing of the attribute, and if all parameters are OK,
		 * fast forward the state machine below.
		 */
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Response received before Tx status");
	}

	ndl->send_naf_on_error = 0;
	ndl->status = params->status;
	nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_RES_RECV);

	if (ndl->dialog_token != params->dialog_token) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Resp: Invalid dialog token (%u != %u)",
			   ndl->dialog_token, params->dialog_token);
		return -1;
	}

	if (params->status != NAN_NDL_STATUS_CONTINUED &&
	    params->status != NAN_NDL_STATUS_ACCEPTED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Resp: Rejected. status=%u, reason=%u",
			   params->status, params->reason);

		ndl->reason = params->reason;
		ndl->status = NAN_NDL_STATUS_REJECTED;
		return 0;
	}

	if (is_zero_ether_addr(params->ndc_id)) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Response without NDC");
		return -1;
	}

	if (os_memcmp(ndl->ndc_id, params->ndc_id, sizeof(ndl->ndc_id)) != 0) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Resp: ndc_id changed");
		os_memcpy(ndl->ndc_id, params->ndc_id, sizeof(ndl->ndc_id));
	}

	ndl->max_idle_period = params->max_idle_period;
	ndl->peer_qos.min_slots = params->min_slots;
	ndl->peer_qos.max_latency = params->max_latency;

	/* TODO: validate that in case an ACCEPTED NDL and an NDC, the NDC
	 * schedule is covered by the local device committed availability.
	 */
	if (params->ndc_sched && params->ndc_sched_len) {
		os_free(ndl->ndc_sched);
		ndl->ndc_sched_len = 0;

		ndl->ndc_sched = os_memdup(params->ndc_sched,
					   params->ndc_sched_len);
		if (!ndl->ndc_sched) {
			wpa_printf(MSG_INFO,
				   "NAN: NDL: Resp: Failed to allocate NDC schedule");
			ret = -1;
			goto fail;
		}

		ndl->ndc_sched_len = params->ndc_sched_len;
	}

	if (params->immut_sched && params->immut_sched_len) {
		if (params->status == NAN_NDL_STATUS_ACCEPTED) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Resp: Immutable not allowed with status == accept");
			return -1;
		}

		os_free(ndl->immut_sched);
		ndl->immut_sched_len = 0;
		ndl->immut_sched = os_memdup(params->immut_sched,
					     params->immut_sched_len);
		if (!ndl->immut_sched) {
			wpa_printf(MSG_INFO,
				   "NAN: NDL: Resp: fail allocate immutable schedule");
			ret = -1;
			goto fail;
		}
		ndl->immut_sched_len = params->immut_sched_len;
	}

	ret = nan_ndl_validate_peer_avail(nan, peer);
	if (!ret)
		goto fail;

	wpa_printf(MSG_DEBUG, "NAN: NDL: Resp: status=%u", params->status);

	ndl->status = params->status;
	if (params->status == NAN_NDL_STATUS_ACCEPTED)
		nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_DONE);

	return 0;

fail:
	nan_ndl_clear(nan, peer);
	ndl->status = NAN_NDL_STATUS_REJECTED;
	ndl->reason = NAN_REASON_RESOURCE_LIMITATION;
	ndl->send_naf_on_error = 1;
	return ret;
}


static int nan_ndl_attr_handle_conf(struct nan_data *nan, struct nan_peer *peer,
				    const struct ndl_attr_params *params)
{
	struct nan_ndl *ndl = peer->ndl;
	int ret;

	if (!ndl) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Confirm without an NDL");
		return -1;
	}

	if (ndl->state != NAN_NDL_STATE_RES_SENT) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Confirm while not expecting one");

		if (ndl->state != NAN_NDL_STATE_REQ_RECV ||
		    ndl->status != NAN_NDL_STATUS_CONTINUED)
			return -1;

		/*
		 * Due to races with the driver, it is possible that the
		 * response is received before an ACK is indicated. Allow the
		 * processing of the attribute, and if all parameters are OK,
		 * fast forward the state machine below.
		 */
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Confirm received before Tx status");
	}

	ndl->send_naf_on_error = 0;
	ndl->status = params->status;
	nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_CON_RECV);

	if (params->status != NAN_NDL_STATUS_ACCEPTED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Confirm was not accepted. status=%u, reason=%u",
			   params->status, params->reason);
		ndl->status = NAN_NDL_STATUS_REJECTED;
		ndl->reason = params->reason;
		return 0;
	}

	if (ndl->dialog_token != params->dialog_token) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Confirm with invalid dialog token (%u != %u)",
			   ndl->dialog_token, params->dialog_token);
		return -1;
	}

	if (!is_zero_ether_addr(params->ndc_id) &&
	    os_memcmp(ndl->ndc_id, params->ndc_id, sizeof(ndl->ndc_id)) != 0) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Confirm: ndc_id changed");
		os_memcpy(ndl->ndc_id, params->ndc_id, sizeof(ndl->ndc_id));
	}

	ndl->max_idle_period = params->max_idle_period;
	ndl->peer_qos.min_slots = params->min_slots;
	ndl->peer_qos.max_latency = params->max_latency;

	/* TODO: validate that the NDC schedule is covered by the local device
	 * committed availability.
	 */
	if (params->ndc_sched && params->ndc_sched_len) {
		os_free(ndl->ndc_sched);
		ndl->ndc_sched_len = 0;

		ndl->ndc_sched = os_memdup(params->ndc_sched,
					   params->ndc_sched_len);
		if (!ndl->ndc_sched) {
			wpa_printf(MSG_INFO,
				   "NAN: NDL: Failed to allocate NDC schedule");
			ret = -1;
			goto fail;
		}
		ndl->ndc_sched_len = params->ndc_sched_len;
	}

	/* TODO: validate that the immutable schedule is covered by the local
	 * device committed availability.
	 */
	if (params->immut_sched && params->immut_sched_len) {
		os_free(ndl->immut_sched);
		ndl->immut_sched_len = 0;

		ndl->immut_sched = os_memdup(params->immut_sched,
					     params->immut_sched_len);
		if (!ndl->immut_sched) {
			wpa_printf(MSG_INFO,
				   "NAN: NDL: Failed to allocate immutable schedule");
			ret = -1;
			goto fail;
		}
		ndl->immut_sched_len = params->immut_sched_len;
	}

	ret = nan_ndl_validate_peer_avail(nan, peer);
	if (!ret)
		goto fail;

	nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_DONE);
	return 0;

fail:
	ndl->reason = NAN_REASON_RESOURCE_LIMITATION;
	ndl->send_naf_on_error = 1;
	return ret;
}


/*
 * nan_ndl_handle_ndl_attr - Handle NDL attribute and update local state
 *
 * @nan: NAN module context from nan_init()
 * @peer: The peer from which the original message was received
 * @msg: Parsed NAN Action frame
 * Returns: 0 on success, negative on failure to parse the attributes etc.
 *
 * As part of the NDL attribute handling, the function also parses NDC and QoS
 * attributes.
 */
int nan_ndl_handle_ndl_attr(struct nan_data *nan, struct nan_peer *peer,
			    struct nan_msg *msg)
{
	const struct ieee80211_ndl *ndl_attr;
	const u8 *ndl_attr_ext;
	struct ndl_attr_params params;
	u16 ndl_attr_len, ndl_attr_ext_len;
	u16 control;
	u8 ctrl_setup_reason;
	u8 ndc_ok;
	int ret;

	os_memset(&params, 0, sizeof(params));

	if (!msg || !peer)
		return -1;

	/*
	 * It is possible that we receive a confirm NAF before the TX status
	 * of the previous NAF was processed. If NDL was accepted, the confirm
	 * would not include the NDL attribute, thus fast forward to state
	 * "done" here.
	 */
	if (msg->oui_subtype == NAN_SUBTYPE_DATA_PATH_CONFIRM &&
	    peer->ndl &&
	    peer->ndl->state == NAN_NDL_STATE_REQ_RECV &&
	    peer->ndl->status == NAN_NDL_STATUS_ACCEPTED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL is accepted - fast forward to state done");
		nan_ndl_set_state(nan, peer->ndl, NAN_NDL_STATE_DONE);
	}

	if (peer->ndl && peer->ndl->state == NAN_NDL_STATE_DONE) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: NDL already done");
		return 0;
	}

	if (!msg->attrs.ndl)
		return 0;

	ndl_attr = (const struct ieee80211_ndl *) msg->attrs.ndl;
	ndl_attr_len = msg->attrs.ndl_len;
	if (ndl_attr_len < sizeof(struct ieee80211_ndl))
		return -1;

	ndl_attr_ext = (const u8 *) (ndl_attr + 1);
	ndl_attr_ext_len = ndl_attr_len - sizeof(struct ieee80211_ndl);

	params.type = BITS(ndl_attr->type_and_status, NAN_NDL_TYPE_MASK,
			   NAN_NDL_TYPE_POS);

	params.status = BITS(ndl_attr->type_and_status, NAN_NDL_STATUS_MASK,
			     NAN_NDL_STATUS_POS);
	params.reason = ndl_attr->reason_code;
	control = le_to_host16(ndl_attr->ctrl);

	if (peer->ndl)
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: curr: state=%s (%d), status=%d",
			   nan_ndl_state_str(peer->ndl->state),
			   peer->ndl->state, peer->ndl->status);
	else
		wpa_printf(MSG_DEBUG, "NAN: NDL: NDL does not exist");

	params.dialog_token = ndl_attr->dialog_token;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: dialog=%u, type=0x%x, status=0x%x, ctrl=0x%x",
		   params.dialog_token, params.type, params.status, control);

	if (control & NAN_NDL_CTRL_PEER_ID_PRESENT) {
		if (ndl_attr_ext_len < 1) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Request with invalid len=%u, control=0x%x",
				   ndl_attr_len, control);
			return -1;
		}

		/*
		 * Peer ID is no longer used. It is considered as reserved in
		 * Wi-Fi Aware Specification v4.0, Table 105 (NDL attribute
		 * format). Just skip it.
		 */
		ndl_attr_ext++;
		ndl_attr_ext_len--;
	}

	if (control & NAN_NDL_CTRL_MAX_IDLE_PERIOD_PRESENT) {
		if (ndl_attr_ext_len < 2) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Request with invalid len=%u, control=0x%x",
				   ndl_attr_len, control);
			return -1;
		}

		params.max_idle_period = WPA_GET_LE16(ndl_attr_ext);
		ndl_attr_ext += 2;
		ndl_attr_ext_len -= 2;

		wpa_printf(MSG_DEBUG, "NAN: NDL: max_idle_period=%u",
			   params.max_idle_period);
	}

	ndc_ok = 1;
	if (control & NAN_NDL_CTRL_NDC_ATTR_PRESENT) {
		struct nan_attrs_entry *n;

		ret = -1;
		dl_list_for_each(n, &msg->attrs.ndc, struct nan_attrs_entry,
				 list) {
			ret = nan_ndl_parse_ndc_attr(
				nan, (const struct ieee80211_ndc *) n->ptr,
				n->len, params.ndc_id,
				&params.ndc_sched, &params.ndc_sched_len);
			if (!ret)
				break;
		}

		if (ret)
			ndc_ok = 0;
	} else if (params.type == NAN_NDL_TYPE_RESPONSE) {
		ndc_ok = 0;
	}

	if (!ndc_ok && params.status != NAN_NDL_STATUS_REJECTED) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Missing valid selected NDC");
		return -1;
	}

	params.min_slots = NAN_QOS_MIN_SLOTS_NO_PREF;
	params.max_latency = NAN_QOS_MAX_LATENCY_NO_PREF;
	if (control & NAN_NDL_CTRL_NDL_QOS_ATTR_PRESENT) {
		if (!msg->attrs.ndl_qos) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL QoS attribute not present but control flag is set");
			return -1;
		}
		ret = nan_ndl_parse_qos_attr(
			nan,
			(const struct ieee80211_nan_qos *) msg->attrs.ndl_qos,
			msg->attrs.ndl_qos_len, &params.min_slots,
			&params.max_latency);
		if (ret)
			return ret;
	}

	if (control & NAN_NDL_CTRL_IMMUT_SCHED_PRESENT) {
		if (ndl_attr_ext_len <= sizeof(struct nan_sched_entry)) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Request with invalid len=%u, control=0x%x",
				ndl_attr_len, control);
			return -1;
		}

		params.immut_sched = ndl_attr_ext;
		params.immut_sched_len = ndl_attr_ext_len;
	}

	ctrl_setup_reason = BITS(control, NAN_NDL_CTRL_NDL_SETUP_REASON_MASK,
				 NAN_NDL_CTRL_NDL_SETUP_REASON_POS);
	if (ctrl_setup_reason == NAN_NDL_CTRL_NDL_SETUP_REASON_NDP) {
		params.setup_reason = NAN_NDL_SETUP_REASON_NDP;
	} else {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Unknown setup reason. Assume NDP");
		params.setup_reason = NAN_NDL_SETUP_REASON_NDP;
	}

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: max_idle_period=%u, immutable len=%u",
		   params.max_idle_period, params.immut_sched_len);

	switch (params.type) {
	case NAN_NDL_TYPE_REQUEST:
		return nan_ndl_attr_handle_req(nan, peer, &params);
	case NAN_NDL_TYPE_RESPONSE:
		return nan_ndl_attr_handle_resp(nan, peer, &params);
	case NAN_NDL_TYPE_CONFIRM:
		return nan_ndl_attr_handle_conf(nan, peer, &params);
	default:
		return -1;
	}
}


/**
 * nan_ndl_add_avail_attrs - Add availability attributes
 * @nan: NAN module context from nan_init()
 * @peer: NAN peer for NDL establishment
 * @buf: Frame buffer to which the attribute would be added
 * Returns: 0 on success, negative on failure
 *
 * An availability attribute is added for each map (identified by map ID) in the
 * NDL schedule. Each attribute holds an availability entry for committed slots
 * and an availability entry for conditional slots.
 */
int nan_ndl_add_avail_attrs(struct nan_data *nan, const struct nan_peer *peer,
			    struct wpabuf *buf)
{
	struct nan_schedule *sched;
	u8 type_for_conditional = NAN_AVAIL_ENTRY_CTRL_TYPE_COND;

	if (!peer || !peer->ndl)
		return -1;

	sched = &nan->sched;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: Add Avail attribute. state=%s, status=%u",
		   nan_ndl_state_str(peer->ndl->state), peer->ndl->status);

	if (sched->n_chans < 1) {
		if (peer->ndl->status == NAN_NDL_STATUS_REJECTED) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Rejected. Not adding availability attributes");
			return 0;
		}

		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Cannot build availability without channels");
		return -1;
	}

	/* In case the NDL exchange was complete successfully, consider the
	 * conditional entries as committed, as this is expected by the spec.
	 */
	if (peer->ndl->status == NAN_NDL_STATUS_ACCEPTED) {
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Add conditional entry as committed");
		type_for_conditional = NAN_AVAIL_ENTRY_CTRL_TYPE_COMMITTED;
	}

	return nan_add_avail_attrs(nan, sched->sequence_id,
				   sched->map_ids_bitmap,
				   type_for_conditional,
				   sched->n_chans, sched->chans, buf, true);
}


/**
 * nan_ndl_add_ndl_attr - Add NDL attribute to frame
 * @nan: NAN module context from nan_init()
 * @peer: NAN peer for NDL establishment
 * @buf: Frame buffer to which the attribute would be added
 * Returns: 0 on success, negative on failure.
 */
int nan_ndl_add_ndl_attr(struct nan_data *nan, const struct nan_peer *peer,
			 struct wpabuf *buf)
{
	struct nan_ndl *ndl;
	struct nan_schedule *sched = &nan->sched;
	u16 ndl_ctrl = 0;
	u8 *len_ptr;
	u8 type;

	if (!peer || !peer->ndl)
		return -1;

	ndl = peer->ndl;

	wpa_printf(MSG_DEBUG, "NAN: Add NDL attribute. state=%s, status=%u",
		   nan_ndl_state_str(ndl->state), ndl->status);

	if (nan->cfg->max_ndl_idle_period) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: max idle period=%u",
			   nan->cfg->max_ndl_idle_period);

		ndl_ctrl |= NAN_NDL_CTRL_MAX_IDLE_PERIOD_PRESENT;
	}

	switch (ndl->state) {
	case NAN_NDL_STATE_NONE:
	case NAN_NDL_STATE_REQ_SENT:
	case NAN_NDL_STATE_RES_SENT:
	case NAN_NDL_STATE_CON_SENT:
	default:
		return -1;
	case NAN_NDL_STATE_START:
		type = NAN_NDL_TYPE_REQUEST;
		if (sched->ndc.len)
			ndl_ctrl |= NAN_NDL_CTRL_NDC_ATTR_PRESENT;
		break;
	case NAN_NDL_STATE_REQ_RECV:
		type = NAN_NDL_TYPE_RESPONSE;
		if (sched->ndc.len &&
		    ndl->status != NAN_NDL_STATUS_REJECTED)
			ndl_ctrl |= NAN_NDL_CTRL_NDC_ATTR_PRESENT;
		break;
	case NAN_NDL_STATE_RES_RECV:
		type = NAN_NDL_TYPE_CONFIRM;
		if (sched->ndc.len &&
		    ndl->status != NAN_NDL_STATUS_REJECTED)
			ndl_ctrl |= NAN_NDL_CTRL_NDC_ATTR_PRESENT;
		break;
	case NAN_NDL_STATE_DONE:
		wpa_printf(MSG_DEBUG,
			   "NAN: NDL: Done. Not adding NDL attribute");
		return 0;
	}

	/* QoS attribute is going to be added */
	if (ndl->local_qos.max_latency != NAN_QOS_MAX_LATENCY_NO_PREF ||
	    ndl->local_qos.min_slots != NAN_QOS_MIN_SLOTS_NO_PREF)
		ndl_ctrl |= NAN_NDL_CTRL_NDL_QOS_ATTR_PRESENT;

	wpabuf_put_u8(buf, NAN_ATTR_NDL);
	len_ptr = wpabuf_put(buf, 2);

	wpabuf_put_u8(buf, ndl->dialog_token);
	wpabuf_put_u8(buf, type | (ndl->status << NAN_NDL_STATUS_POS));
	wpabuf_put_u8(buf, ndl->reason);
	wpabuf_put_u8(buf, ndl_ctrl);

	if (nan->cfg->max_ndl_idle_period)
		wpabuf_put_le16(buf, nan->cfg->max_ndl_idle_period);

	WPA_PUT_LE16(len_ptr, (u8 *) wpabuf_put(buf, 0) - len_ptr - 2);

	return 0;
}


/**
 * nan_ndl_add_ndc_attr - Add NDC attribute to frame
 * @nan: NAN module context from nan_init()
 * @peer: NAN peer for NDL establishment
 * @buf: Frame buffer to which the attribute would be added
 * Returns: 0 on success, negative on failure.
 */
int nan_ndl_add_ndc_attr(struct nan_data *nan, const struct nan_peer *peer,
			 struct wpabuf *buf)
{
	struct nan_ndl *ndl;
	struct nan_schedule *sched = &nan->sched;
	u8 ndc_ctrl = NAN_NDC_CTRL_SELECTED;
	u16 sched_entry_ctrl = 0;

	if (!peer || !peer->ndl)
		return -1;

	ndl = peer->ndl;

	if (ndl->state != NAN_NDL_STATE_START &&
	    ndl->state != NAN_NDL_STATE_REQ_RECV &&
	    ndl->state != NAN_NDL_STATE_RES_RECV)
		return 0;

	wpa_printf(MSG_DEBUG, "NAN: Add NDC attribute. state=%s, status=%u",
		   nan_ndl_state_str(ndl->state), ndl->status);

	/* NDC attribute is optional in case of reject */
	if (ndl->status == NAN_NDL_STATUS_REJECTED)
		return 0;

	/*
	 * NDC attribute for NDP Request is optional. In all other cases it is
	 * mandatory
	 */
	if (!sched->ndc.len) {
		if (ndl->state != NAN_NDL_STATE_START) {
			wpa_printf(MSG_DEBUG, "NAN: NDL: No NDC to add");
			return -1;
		}

		return 0;
	}

	wpabuf_put_u8(buf, NAN_ATTR_NDC);
	wpabuf_put_le16(buf, sizeof(struct ieee80211_ndc) +
			sizeof(struct nan_sched_entry) +
			sched->ndc.len);

	wpabuf_put_data(buf, ndl->ndc_id, sizeof(ndl->ndc_id));
	wpabuf_put_u8(buf, ndc_ctrl);

	/* Add the schedule entry */
	wpabuf_put_u8(buf, sched->ndc_map_id);

	sched_entry_ctrl |= sched->ndc.duration <<
		NAN_TIME_BM_CTRL_BIT_DURATION_POS;
	sched_entry_ctrl |= sched->ndc.period <<
		NAN_TIME_BM_CTRL_PERIOD_POS;
	sched_entry_ctrl |= sched->ndc.offset <<
		NAN_TIME_BM_CTRL_START_OFFSET_POS;

	wpabuf_put_le16(buf, sched_entry_ctrl);

	/* Add the time bitmap */
	wpabuf_put_u8(buf, sched->ndc.len);
	wpabuf_put_data(buf, sched->ndc.bitmap, sched->ndc.len);

	return 0;
}


/**
 * nan_ndl_add_qos_attr - Add QOS attribute to frame
 * @nan: NAN module context from nan_init()
 * @peer: NAN peer for NDL establishment
 * @buf: Frame buffer to which the attribute would be added
 * Returns: 0 on success, negative on failure.
 */
int nan_ndl_add_qos_attr(struct nan_data *nan,
			 const struct nan_peer *peer,
			 struct wpabuf *buf)
{
	struct nan_ndl *ndl;

	if (!peer || !peer->ndl)
		return -1;

	ndl = peer->ndl;

	wpa_printf(MSG_DEBUG, "NAN: Add QoS attribute. state=%s, status=%u",
		   nan_ndl_state_str(ndl->state), ndl->status);

	switch (ndl->state) {
	case NAN_NDL_STATE_START:
	case NAN_NDL_STATE_REQ_RECV:
	case NAN_NDL_STATE_RES_RECV:
		break;
	case NAN_NDL_STATE_NONE:
	case NAN_NDL_STATE_REQ_SENT:
	case NAN_NDL_STATE_RES_SENT:
	case NAN_NDL_STATE_CON_SENT:
	case NAN_NDL_STATE_CON_RECV:
	case NAN_NDL_STATE_DONE:
	default:
		return 0;
	}

	if (ndl->local_qos.max_latency == NAN_QOS_MAX_LATENCY_NO_PREF &&
	    ndl->local_qos.min_slots == NAN_QOS_MIN_SLOTS_NO_PREF)
		return 0;

	wpabuf_put_u8(buf, NAN_ATTR_NDL_QOS);
	wpabuf_put_le16(buf, sizeof(struct ieee80211_nan_qos));
	wpabuf_put_u8(buf, ndl->local_qos.min_slots);
	wpabuf_put_le16(buf, ndl->local_qos.max_latency);

	return 0;
}


/**
 * nan_ndl_naf_sent - Indicate a NAF has been sent
 * @nan: NAN module context from nan_init()
 * @peer: The peer with whom the NDL is being setup
 * @subtype: The NAN OUI subtype
 *
 * A notification indicating to the NDL state machine that a NAF was sent, so
 * the NDL state machine can update its state.
 *
 * In case the NDL setup negotiation is successfully done, the final schedule
 * is applied and the NDL is active. If the negotiation is done and failed, the
 * NDL state is reset.
 */
int nan_ndl_naf_sent(struct nan_data *nan, struct nan_peer *peer,
		     enum nan_subtype subtype)
{
	struct nan_ndl *ndl;

	if (!peer || !peer->ndl)
		return -1;

	ndl = peer->ndl;

	if (ndl->state == NAN_NDL_STATE_DONE)
		return 0;

	wpa_printf(MSG_DEBUG,
		   "NAN: NDL: Tx done with peer=" MACSTR " state=%s, status=%u",
		   MAC2STR(peer->nmi_addr), nan_ndl_state_str(ndl->state),
		   ndl->status);

	/* Note: Due to races between the Tx status and Rx path, it is possible
	 * that the Tx status is received after the peer response was already
	 * processed (which can result with another frame being sent). In such a
	 * case the logic above fast-forwards the state, and the transitions
	 * here need to take this into consideration.
	 */
	switch (ndl->state) {
	case NAN_NDL_STATE_START:
		if (subtype != NAN_SUBTYPE_DATA_PATH_REQUEST)
			return 0;
		if (ndl->status != NAN_NDL_STATUS_CONTINUED) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Tx sent: invalid continue status");
			return -1;
		}
		nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_REQ_SENT);
		return 0;
	case NAN_NDL_STATE_REQ_RECV:
		if (subtype != NAN_SUBTYPE_DATA_PATH_RESPONSE)
			return 0;
		if (ndl->status == NAN_NDL_STATUS_CONTINUED) {
			nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_RES_SENT);
			return 0;
		}
		break;
	case NAN_NDL_STATE_RES_RECV:
		if (subtype != NAN_SUBTYPE_DATA_PATH_CONFIRM)
			return 0;
		if (ndl->status == NAN_NDL_STATUS_CONTINUED) {
			wpa_printf(MSG_DEBUG,
				   "NAN: NDL: Tx sent: invalid continue status");
			return -1;
		}
		break;
	case NAN_NDL_STATE_CON_RECV:
	case NAN_NDL_STATE_REQ_SENT:
	case NAN_NDL_STATE_RES_SENT:
	case NAN_NDL_STATE_CON_SENT:
	case NAN_NDL_STATE_DONE:
	default:
		wpa_printf(MSG_DEBUG, "NAN: NDL: Tx sent: unexpected state %d",
			   ndl->state);
		return 0;
	}

	if (ndl->status == NAN_NDL_STATUS_ACCEPTED) {
		wpa_printf(MSG_DEBUG, "NAN: NDL: Schedule setup success");
		nan_ndl_set_state(nan, ndl, NAN_NDL_STATE_DONE);
		return 0;
	}

	/* NDL is rejected and NAF already sent. Higher layer is expected to
	 * handle it.
	 */
	return 0;
}


/*
 * nan_ndl_add_elem_container_attr - Add NAN element container attribute
 *
 * @nan: NAN module context from nan_init()
 * @peer: The peer with whom the NDL is being setup
 * @buf: wpabuf to which the attribute is added
 */
void nan_ndl_add_elem_container_attr(const struct nan_data *nan,
				     const struct nan_peer *peer,
				     struct wpabuf *buf)
{
	const struct nan_ndl *ndl;

	if (!peer || !peer->ndl || !nan->sched.elems)
		return;

	ndl = peer->ndl;

	wpa_printf(MSG_DEBUG, "NAN: Add element container. state=%s, status=%u",
		   nan_ndl_state_str(ndl->state), ndl->status);

	if (peer->ndl->status == NAN_NDL_STATUS_REJECTED)
		return;

	/* Element container is expected only in NDP request/response */
	if (ndl->state != NAN_NDL_STATE_START &&
	    ndl->state != NAN_NDL_STATE_REQ_RECV)
		return;

	wpabuf_put_u8(buf, NAN_ATTR_ELEM_CONTAINER);
	wpabuf_put_le16(buf, 1 + wpabuf_len(nan->sched.elems));
	wpabuf_put_u8(buf, 0);
	wpabuf_put_buf(buf, nan->sched.elems);
}
