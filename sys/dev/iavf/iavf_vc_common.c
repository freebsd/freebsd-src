/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

/**
 * @file iavf_vc_common.c
 * @brief Common virtchnl interface functions
 *
 * Contains functions implementing the virtchnl interface for connecting to
 * the PF driver. This file contains the functions which are common between
 * the legacy and iflib driver implementations.
 */
#include "iavf_vc_common.h"

/* busy wait delay in msec */
#define IAVF_BUSY_WAIT_DELAY 10
#define IAVF_BUSY_WAIT_COUNT 50

/* Static function decls */
static void iavf_handle_link_event(struct iavf_sc *sc,
    struct virtchnl_pf_event *vpe);

/**
 * iavf_send_pf_msg - Send virtchnl message to PF device
 * @sc: device softc
 * @op: the op to send
 * @msg: message contents
 * @len: length of the message
 *
 * Send a message to the PF device over the virtchnl connection. Print
 * a status code if the message reports an error.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_send_pf_msg(struct iavf_sc *sc,
	enum virtchnl_ops op, u8 *msg, u16 len)
{
	struct iavf_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum iavf_status status;
	int val_err;

	/* Validating message before sending it to the PF */
	val_err = virtchnl_vc_validate_vf_msg(&sc->version, op, msg, len);
	if (val_err)
		device_printf(dev, "Error validating msg to PF for op %d,"
		    " msglen %d: error %d\n", op, len, val_err);

	if (!iavf_check_asq_alive(hw)) {
		if (op != VIRTCHNL_OP_GET_STATS)
			device_printf(dev, "Unable to send opcode %s to PF, "
			    "ASQ is not alive\n", iavf_vc_opcode_str(op));
		return (0);
	}

	if (op != VIRTCHNL_OP_GET_STATS)
		iavf_dbg_vc(sc,
		    "Sending msg (op=%s[%d]) to PF\n",
		    iavf_vc_opcode_str(op), op);

	status = iavf_aq_send_msg_to_pf(hw, op, IAVF_SUCCESS, msg, len, NULL);
	if (status && op != VIRTCHNL_OP_GET_STATS)
		device_printf(dev, "Unable to send opcode %s to PF, "
		    "status %s, aq error %s\n",
		    iavf_vc_opcode_str(op),
		    iavf_stat_str(hw, status),
		    iavf_aq_str(hw, hw->aq.asq_last_status));

	return (status);
}

/**
 * iavf_send_api_ver - Send the API version we support to the PF
 * @sc: device softc
 *
 * Send API version admin queue message to the PF. The reply is not checked
 * in this function.
 *
 * @returns 0 if the message was successfully sent, or one of the
 * IAVF_ADMIN_QUEUE_ERROR_ statuses if not.
 */
int
iavf_send_api_ver(struct iavf_sc *sc)
{
	struct virtchnl_version_info vvi;

	vvi.major = VIRTCHNL_VERSION_MAJOR;
	vvi.minor = VIRTCHNL_VERSION_MINOR;

	return iavf_send_pf_msg(sc, VIRTCHNL_OP_VERSION,
	    (u8 *)&vvi, sizeof(vvi));
}

/**
 * iavf_verify_api_ver - Verify the PF supports our API version
 * @sc: device softc
 *
 * Compare API versions with the PF. Must be called after admin queue is
 * initialized.
 *
 * @returns 0 if API versions match, EIO if they do not, or
 * IAVF_ERR_ADMIN_QUEUE_NO_WORK if the admin queue is empty.
 */
int
iavf_verify_api_ver(struct iavf_sc *sc)
{
	struct virtchnl_version_info *pf_vvi;
	struct iavf_hw *hw = &sc->hw;
	struct iavf_arq_event_info event;
	enum iavf_status status;
	device_t dev = sc->dev;
	int error = 0;
	int retries = 0;

	event.buf_len = IAVF_AQ_BUF_SZ;
	event.msg_buf = (u8 *)malloc(event.buf_len, M_IAVF, M_WAITOK);

	for (;;) {
		if (++retries > IAVF_AQ_MAX_ERR)
			goto out_alloc;

		/* Initial delay here is necessary */
		iavf_msec_pause(100);
		status = iavf_clean_arq_element(hw, &event, NULL);
		if (status == IAVF_ERR_ADMIN_QUEUE_NO_WORK)
			continue;
		else if (status) {
			error = EIO;
			goto out_alloc;
		}

		if ((enum virtchnl_ops)le32toh(event.desc.cookie_high) !=
		    VIRTCHNL_OP_VERSION) {
			iavf_dbg_vc(sc, "%s: Received unexpected op response: %d\n",
			    __func__, le32toh(event.desc.cookie_high));
			/* Don't stop looking for expected response */
			continue;
		}

		status = (enum iavf_status)le32toh(event.desc.cookie_low);
		if (status) {
			error = EIO;
			goto out_alloc;
		} else
			break;
	}

	pf_vvi = (struct virtchnl_version_info *)event.msg_buf;
	if ((pf_vvi->major > VIRTCHNL_VERSION_MAJOR) ||
	    ((pf_vvi->major == VIRTCHNL_VERSION_MAJOR) &&
	    (pf_vvi->minor > VIRTCHNL_VERSION_MINOR))) {
		device_printf(dev, "Critical PF/VF API version mismatch!\n");
		error = EIO;
	} else {
		sc->version.major = pf_vvi->major;
		sc->version.minor = pf_vvi->minor;
	}

	/* Log PF/VF api versions */
	device_printf(dev, "PF API %d.%d / VF API %d.%d\n",
	    pf_vvi->major, pf_vvi->minor,
	    VIRTCHNL_VERSION_MAJOR, VIRTCHNL_VERSION_MINOR);

out_alloc:
	free(event.msg_buf, M_IAVF);
	return (error);
}

/**
 * iavf_send_vf_config_msg - Send VF configuration request
 * @sc: device softc
 *
 * Send VF configuration request admin queue message to the PF. The reply
 * is not checked in this function.
 *
 * @returns 0 if the message was successfully sent, or one of the
 * IAVF_ADMIN_QUEUE_ERROR_ statuses if not.
 */
int
iavf_send_vf_config_msg(struct iavf_sc *sc)
{
	u32 caps;

	/* Support the base mode functionality, as well as advanced
	 * speed reporting capability.
	 */
	caps = VF_BASE_MODE_OFFLOADS |
	    VIRTCHNL_VF_CAP_ADV_LINK_SPEED;

	iavf_dbg_info(sc, "Sending offload flags: 0x%b\n",
	    caps, IAVF_PRINTF_VF_OFFLOAD_FLAGS);

	if (sc->version.minor == VIRTCHNL_VERSION_MINOR_NO_VF_CAPS)
		return iavf_send_pf_msg(sc, VIRTCHNL_OP_GET_VF_RESOURCES,
				  NULL, 0);
	else
		return iavf_send_pf_msg(sc, VIRTCHNL_OP_GET_VF_RESOURCES,
				  (u8 *)&caps, sizeof(caps));
}

/**
 * iavf_get_vf_config - Get the VF configuration from the PF
 * @sc: device softc
 *
 * Get VF configuration from PF and populate hw structure. Must be called after
 * admin queue is initialized. Busy waits until response is received from PF,
 * with maximum timeout. Response from PF is returned in the buffer for further
 * processing by the caller.
 *
 * @returns zero on success, or an error code on failure
 */
int
iavf_get_vf_config(struct iavf_sc *sc)
{
	struct iavf_hw	*hw = &sc->hw;
	device_t	dev = sc->dev;
	enum iavf_status status = IAVF_SUCCESS;
	struct iavf_arq_event_info event;
	u16 len;
	u32 retries = 0;
	int error = 0;

	/* Note this assumes a single VSI */
	len = sizeof(struct virtchnl_vf_resource) +
	    sizeof(struct virtchnl_vsi_resource);
	event.buf_len = len;
	event.msg_buf = (u8 *)malloc(event.buf_len, M_IAVF, M_WAITOK);

	for (;;) {
		status = iavf_clean_arq_element(hw, &event, NULL);
		if (status == IAVF_ERR_ADMIN_QUEUE_NO_WORK) {
			if (++retries <= IAVF_AQ_MAX_ERR)
				iavf_msec_pause(10);
		} else if ((enum virtchnl_ops)le32toh(event.desc.cookie_high) !=
		    VIRTCHNL_OP_GET_VF_RESOURCES) {
			iavf_dbg_vc(sc, "%s: Received a response from PF,"
			    " opcode %d, error %d",
			    __func__,
			    le32toh(event.desc.cookie_high),
			    le32toh(event.desc.cookie_low));
			retries++;
			continue;
		} else {
			status = (enum iavf_status)le32toh(event.desc.cookie_low);
			if (status) {
				device_printf(dev, "%s: Error returned from PF,"
				    " opcode %d, error %d\n", __func__,
				    le32toh(event.desc.cookie_high),
				    le32toh(event.desc.cookie_low));
				error = EIO;
				goto out_alloc;
			}
			/* We retrieved the config message, with no errors */
			break;
		}

		if (retries > IAVF_AQ_MAX_ERR) {
			iavf_dbg_vc(sc,
			    "%s: Did not receive response after %d tries.",
			    __func__, retries);
			error = ETIMEDOUT;
			goto out_alloc;
		}
	}

	memcpy(sc->vf_res, event.msg_buf, min(event.msg_len, len));
	iavf_vf_parse_hw_config(hw, sc->vf_res);

out_alloc:
	free(event.msg_buf, M_IAVF);
	return (error);
}

/**
 * iavf_enable_queues - Enable queues
 * @sc: device softc
 *
 * Request that the PF enable all of our queues.
 *
 * @remark the reply from the PF is not checked by this function.
 *
 * @returns zero
 */
int
iavf_enable_queues(struct iavf_sc *sc)
{
	struct virtchnl_queue_select vqs;
	struct iavf_vsi *vsi = &sc->vsi;

	vqs.vsi_id = sc->vsi_res->vsi_id;
	vqs.tx_queues = (1 << IAVF_NTXQS(vsi)) - 1;
	vqs.rx_queues = vqs.tx_queues;
	iavf_send_pf_msg(sc, VIRTCHNL_OP_ENABLE_QUEUES,
			   (u8 *)&vqs, sizeof(vqs));
	return (0);
}

/**
 * iavf_disable_queues - Disable queues
 * @sc: device softc
 *
 * Request that the PF disable all of our queues.
 *
 * @remark the reply from the PF is not checked by this function.
 *
 * @returns zero
 */
int
iavf_disable_queues(struct iavf_sc *sc)
{
	struct virtchnl_queue_select vqs;
	struct iavf_vsi *vsi = &sc->vsi;

	vqs.vsi_id = sc->vsi_res->vsi_id;
	vqs.tx_queues = (1 << IAVF_NTXQS(vsi)) - 1;
	vqs.rx_queues = vqs.tx_queues;
	iavf_send_pf_msg(sc, VIRTCHNL_OP_DISABLE_QUEUES,
			   (u8 *)&vqs, sizeof(vqs));
	return (0);
}

/**
 * iavf_add_vlans - Add VLAN filters
 * @sc: device softc
 *
 * Scan the Filter List looking for vlans that need
 * to be added, then create the data to hand to the AQ
 * for handling.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_add_vlans(struct iavf_sc *sc)
{
	struct virtchnl_vlan_filter_list *v;
	struct iavf_vlan_filter *f, *ftmp;
	device_t dev = sc->dev;
	int i = 0, cnt = 0;
	u32 len;

	/* Get count of VLAN filters to add */
	SLIST_FOREACH(f, sc->vlan_filters, next) {
		if (f->flags & IAVF_FILTER_ADD)
			cnt++;
	}

	if (!cnt) /* no work... */
		return (ENOENT);

	len = sizeof(struct virtchnl_vlan_filter_list) +
	      (cnt * sizeof(u16));

	if (len > IAVF_AQ_BUF_SZ) {
		device_printf(dev, "%s: Exceeded Max AQ Buf size\n",
			__func__);
		return (EFBIG);
	}

	v = (struct virtchnl_vlan_filter_list *)malloc(len, M_IAVF, M_NOWAIT | M_ZERO);
	if (!v) {
		device_printf(dev, "%s: unable to allocate memory\n",
			__func__);
		return (ENOMEM);
	}

	v->vsi_id = sc->vsi_res->vsi_id;
	v->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH_SAFE(f, sc->vlan_filters, next, ftmp) {
                if (f->flags & IAVF_FILTER_ADD) {
                        bcopy(&f->vlan, &v->vlan_id[i], sizeof(u16));
			f->flags = IAVF_FILTER_USED;
                        i++;
                }
                if (i == cnt)
                        break;
	}

	iavf_send_pf_msg(sc, VIRTCHNL_OP_ADD_VLAN, (u8 *)v, len);
	free(v, M_IAVF);
	/* add stats? */
	return (0);
}

/**
 * iavf_del_vlans - Delete VLAN filters
 * @sc: device softc
 *
 * Scan the Filter Table looking for vlans that need
 * to be removed, then create the data to hand to the AQ
 * for handling.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_del_vlans(struct iavf_sc *sc)
{
	struct virtchnl_vlan_filter_list *v;
	struct iavf_vlan_filter *f, *ftmp;
	device_t dev = sc->dev;
	int i = 0, cnt = 0;
	u32 len;

	/* Get count of VLAN filters to delete */
	SLIST_FOREACH(f, sc->vlan_filters, next) {
		if (f->flags & IAVF_FILTER_DEL)
			cnt++;
	}

	if (!cnt) /* no work... */
		return (ENOENT);

	len = sizeof(struct virtchnl_vlan_filter_list) +
	      (cnt * sizeof(u16));

	if (len > IAVF_AQ_BUF_SZ) {
		device_printf(dev, "%s: Exceeded Max AQ Buf size\n",
			__func__);
		return (EFBIG);
	}

	v = (struct virtchnl_vlan_filter_list *)
	    malloc(len, M_IAVF, M_NOWAIT | M_ZERO);
	if (!v) {
		device_printf(dev, "%s: unable to allocate memory\n",
			__func__);
		return (ENOMEM);
	}

	v->vsi_id = sc->vsi_res->vsi_id;
	v->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH_SAFE(f, sc->vlan_filters, next, ftmp) {
                if (f->flags & IAVF_FILTER_DEL) {
                        bcopy(&f->vlan, &v->vlan_id[i], sizeof(u16));
                        i++;
                        SLIST_REMOVE(sc->vlan_filters, f, iavf_vlan_filter, next);
                        free(f, M_IAVF);
                }
                if (i == cnt)
                        break;
	}

	iavf_send_pf_msg(sc, VIRTCHNL_OP_DEL_VLAN, (u8 *)v, len);
	free(v, M_IAVF);
	/* add stats? */
	return (0);
}

/**
 * iavf_add_ether_filters - Add MAC filters
 * @sc: device softc
 *
 * This routine takes additions to the vsi filter
 * table and creates an Admin Queue call to create
 * the filters in the hardware.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_add_ether_filters(struct iavf_sc *sc)
{
	struct virtchnl_ether_addr_list *a;
	struct iavf_mac_filter *f;
	device_t dev = sc->dev;
	int len, j = 0, cnt = 0;
	int error;

	/* Get count of MAC addresses to add */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IAVF_FILTER_ADD)
			cnt++;
	}
	if (cnt == 0) { /* Should not happen... */
		iavf_dbg_vc(sc, "%s: cnt == 0, exiting...\n", __func__);
		return (ENOENT);
	}

	len = sizeof(struct virtchnl_ether_addr_list) +
	    (cnt * sizeof(struct virtchnl_ether_addr));

	a = (struct virtchnl_ether_addr_list *)
	    malloc(len, M_IAVF, M_NOWAIT | M_ZERO);
	if (a == NULL) {
		device_printf(dev, "%s: Failed to get memory for "
		    "virtchnl_ether_addr_list\n", __func__);
		return (ENOMEM);
	}
	a->vsi_id = sc->vsi.id;
	a->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IAVF_FILTER_ADD) {
			bcopy(f->macaddr, a->list[j].addr, ETHER_ADDR_LEN);
			f->flags &= ~IAVF_FILTER_ADD;
			j++;

			iavf_dbg_vc(sc, "%s: ADD: " MAC_FORMAT "\n",
			    __func__, MAC_FORMAT_ARGS(f->macaddr));
		}
		if (j == cnt)
			break;
	}
	iavf_dbg_vc(sc, "%s: len %d, j %d, cnt %d\n", __func__,
	    len, j, cnt);

	error = iavf_send_pf_msg(sc,
	    VIRTCHNL_OP_ADD_ETH_ADDR, (u8 *)a, len);
	/* add stats? */
	free(a, M_IAVF);
	return (error);
}

/**
 * iavf_del_ether_filters - Delete MAC filters
 * @sc: device softc
 *
 * This routine takes filters flagged for deletion in the
 * sc MAC filter list and creates an Admin Queue call
 * to delete those filters in the hardware.
 *
 * @returns zero on success, or an error code on failure.
*/
int
iavf_del_ether_filters(struct iavf_sc *sc)
{
	struct virtchnl_ether_addr_list *d;
	struct iavf_mac_filter *f, *f_temp;
	device_t dev = sc->dev;
	int len, j = 0, cnt = 0;

	/* Get count of MAC addresses to delete */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (f->flags & IAVF_FILTER_DEL)
			cnt++;
	}
	if (cnt == 0) {
		iavf_dbg_vc(sc, "%s: cnt == 0, exiting...\n", __func__);
		return (ENOENT);
	}

	len = sizeof(struct virtchnl_ether_addr_list) +
	    (cnt * sizeof(struct virtchnl_ether_addr));

	d = (struct virtchnl_ether_addr_list *)
	    malloc(len, M_IAVF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		device_printf(dev, "%s: Failed to get memory for "
		    "virtchnl_ether_addr_list\n", __func__);
		return (ENOMEM);
	}
	d->vsi_id = sc->vsi.id;
	d->num_elements = cnt;

	/* Scan the filter array */
	SLIST_FOREACH_SAFE(f, sc->mac_filters, next, f_temp) {
		if (f->flags & IAVF_FILTER_DEL) {
			bcopy(f->macaddr, d->list[j].addr, ETHER_ADDR_LEN);
			iavf_dbg_vc(sc, "DEL: " MAC_FORMAT "\n",
			    MAC_FORMAT_ARGS(f->macaddr));
			j++;
			SLIST_REMOVE(sc->mac_filters, f, iavf_mac_filter, next);
			free(f, M_IAVF);
		}
		if (j == cnt)
			break;
	}
	iavf_send_pf_msg(sc,
	    VIRTCHNL_OP_DEL_ETH_ADDR, (u8 *)d, len);
	/* add stats? */
	free(d, M_IAVF);
	return (0);
}

/**
 * iavf_request_reset - Request a device reset
 * @sc: device softc
 *
 * Request that the PF reset this VF. No response is expected.
 *
 * @returns zero
 */
int
iavf_request_reset(struct iavf_sc *sc)
{
	/*
	** Set the reset status to "in progress" before
	** the request, this avoids any possibility of
	** a mistaken early detection of completion.
	*/
	wr32(&sc->hw, IAVF_VFGEN_RSTAT, VIRTCHNL_VFR_INPROGRESS);
	iavf_send_pf_msg(sc, VIRTCHNL_OP_RESET_VF, NULL, 0);
	return (0);
}

/**
 * iavf_request_stats - Request VF stats
 * @sc: device softc
 *
 * Request the statistics for this VF's VSI from PF.
 *
 * @remark prints an error message on failure to obtain stats, but does not
 * return with an error code.
 *
 * @returns zero
 */
int
iavf_request_stats(struct iavf_sc *sc)
{
	struct virtchnl_queue_select vqs;
	int error = 0;

	vqs.vsi_id = sc->vsi_res->vsi_id;
	/* Low priority, we don't need to error check */
	error = iavf_send_pf_msg(sc, VIRTCHNL_OP_GET_STATS,
	    (u8 *)&vqs, sizeof(vqs));
	if (error)
		device_printf(sc->dev, "Error sending stats request to PF: %d\n", error);

	return (0);
}

/**
 * iavf_update_stats_counters - Update driver statistics
 * @sc: device softc
 * @es: ethernet stats storage
 *
 * Updates driver's stats counters with VSI stats returned from PF.
 */
void
iavf_update_stats_counters(struct iavf_sc *sc, struct iavf_eth_stats *es)
{
	struct iavf_vsi *vsi = &sc->vsi;
	uint64_t tx_discards;

	tx_discards = es->tx_discards;

	/* Update ifnet stats */
	IAVF_SET_IPACKETS(vsi, es->rx_unicast +
	                   es->rx_multicast +
			   es->rx_broadcast);
	IAVF_SET_OPACKETS(vsi, es->tx_unicast +
	                   es->tx_multicast +
			   es->tx_broadcast);
	IAVF_SET_IBYTES(vsi, es->rx_bytes);
	IAVF_SET_OBYTES(vsi, es->tx_bytes);
	IAVF_SET_IMCASTS(vsi, es->rx_multicast);
	IAVF_SET_OMCASTS(vsi, es->tx_multicast);

	IAVF_SET_OERRORS(vsi, es->tx_errors);
	IAVF_SET_IQDROPS(vsi, es->rx_discards);
	IAVF_SET_OQDROPS(vsi, tx_discards);
	IAVF_SET_NOPROTO(vsi, es->rx_unknown_protocol);
	IAVF_SET_COLLISIONS(vsi, 0);

	vsi->eth_stats = *es;
}

/**
 * iavf_config_rss_key - Configure RSS key over virtchnl
 * @sc: device softc
 *
 * Send a message to the PF to configure the RSS key using the virtchnl
 * interface.
 *
 * @remark this does not check the reply from the PF.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_config_rss_key(struct iavf_sc *sc)
{
	struct virtchnl_rss_key *rss_key_msg;
	int msg_len, key_length;
	u8		rss_seed[IAVF_RSS_KEY_SIZE];

#ifdef RSS
	/* Fetch the configured RSS key */
	rss_getkey((uint8_t *) &rss_seed);
#else
	iavf_get_default_rss_key((u32 *)rss_seed);
#endif

	/* Send the fetched key */
	key_length = IAVF_RSS_KEY_SIZE;
	msg_len = sizeof(struct virtchnl_rss_key) + (sizeof(u8) * key_length) - 1;
	rss_key_msg = (struct virtchnl_rss_key *)
	    malloc(msg_len, M_IAVF, M_NOWAIT | M_ZERO);
	if (rss_key_msg == NULL) {
		device_printf(sc->dev, "Unable to allocate msg memory for RSS key msg.\n");
		return (ENOMEM);
	}

	rss_key_msg->vsi_id = sc->vsi_res->vsi_id;
	rss_key_msg->key_len = key_length;
	bcopy(rss_seed, &rss_key_msg->key[0], key_length);

	iavf_dbg_vc(sc, "%s: vsi_id %d, key_len %d\n", __func__,
	    rss_key_msg->vsi_id, rss_key_msg->key_len);

	iavf_send_pf_msg(sc, VIRTCHNL_OP_CONFIG_RSS_KEY,
			  (u8 *)rss_key_msg, msg_len);

	free(rss_key_msg, M_IAVF);
	return (0);
}

/**
 * iavf_set_rss_hena - Configure the RSS HENA
 * @sc: device softc
 *
 * Configure the RSS HENA values by sending a virtchnl message to the PF
 *
 * @remark the reply from the PF is not checked by this function.
 *
 * @returns zero
 */
int
iavf_set_rss_hena(struct iavf_sc *sc)
{
	struct virtchnl_rss_hena hena;
	struct iavf_hw *hw = &sc->hw;

	if (hw->mac.type == IAVF_MAC_VF)
		hena.hena = IAVF_DEFAULT_RSS_HENA_AVF;
	else if (hw->mac.type == IAVF_MAC_X722_VF)
		hena.hena = IAVF_DEFAULT_RSS_HENA_X722;
	else
		hena.hena = IAVF_DEFAULT_RSS_HENA_BASE;

	iavf_send_pf_msg(sc, VIRTCHNL_OP_SET_RSS_HENA,
	    (u8 *)&hena, sizeof(hena));
	return (0);
}

/**
 * iavf_config_rss_lut - Configure RSS lookup table
 * @sc: device softc
 *
 * Configure the RSS lookup table by sending a virtchnl message to the PF.
 *
 * @remark the reply from the PF is not checked in this function.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_config_rss_lut(struct iavf_sc *sc)
{
	struct virtchnl_rss_lut *rss_lut_msg;
	int msg_len;
	u16 lut_length;
	u32 lut;
	int i, que_id;

	lut_length = IAVF_RSS_VSI_LUT_SIZE;
	msg_len = sizeof(struct virtchnl_rss_lut) + (lut_length * sizeof(u8)) - 1;
	rss_lut_msg = (struct virtchnl_rss_lut *)
	    malloc(msg_len, M_IAVF, M_NOWAIT | M_ZERO);
	if (rss_lut_msg == NULL) {
		device_printf(sc->dev, "Unable to allocate msg memory for RSS lut msg.\n");
		return (ENOMEM);
	}

	rss_lut_msg->vsi_id = sc->vsi_res->vsi_id;
	/* Each LUT entry is a max of 1 byte, so this is easy */
	rss_lut_msg->lut_entries = lut_length;

	/* Populate the LUT with max no. of queues in round robin fashion */
	for (i = 0; i < lut_length; i++) {
#ifdef RSS
		/*
		 * Fetch the RSS bucket id for the given indirection entry.
		 * Cap it at the number of configured buckets (which is
		 * num_queues.)
		 */
		que_id = rss_get_indirection_to_bucket(i);
		que_id = que_id % sc->vsi.num_rx_queues;
#else
		que_id = i % sc->vsi.num_rx_queues;
#endif
		lut = que_id & IAVF_RSS_VSI_LUT_ENTRY_MASK;
		rss_lut_msg->lut[i] = lut;
	}

	iavf_send_pf_msg(sc, VIRTCHNL_OP_CONFIG_RSS_LUT,
			  (u8 *)rss_lut_msg, msg_len);

	free(rss_lut_msg, M_IAVF);
	return (0);
}

/**
 * iavf_config_promisc_mode - Configure promiscuous mode
 * @sc: device softc
 *
 * Configure the device into promiscuous mode by sending a virtchnl message to
 * the PF.
 *
 * @remark the reply from the PF is not checked in this function.
 *
 * @returns zero
 */
int
iavf_config_promisc_mode(struct iavf_sc *sc)
{
	struct virtchnl_promisc_info pinfo;

	pinfo.vsi_id = sc->vsi_res->vsi_id;
	pinfo.flags = sc->promisc_flags;

	iavf_send_pf_msg(sc, VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE,
	    (u8 *)&pinfo, sizeof(pinfo));
	return (0);
}

/**
 * iavf_vc_send_cmd - Convert request into virtchnl calls
 * @sc: device softc
 * @request: the requested command to run
 *
 * Send the proper virtchnl call based on the request value.
 *
 * @returns zero on success, or an error code on failure. Note that unknown
 * requests will return zero.
 */
int
iavf_vc_send_cmd(struct iavf_sc *sc, uint32_t request)
{
	switch (request) {
	case IAVF_FLAG_AQ_MAP_VECTORS:
		return iavf_map_queues(sc);

	case IAVF_FLAG_AQ_ADD_MAC_FILTER:
		return iavf_add_ether_filters(sc);

	case IAVF_FLAG_AQ_ADD_VLAN_FILTER:
		return iavf_add_vlans(sc);

	case IAVF_FLAG_AQ_DEL_MAC_FILTER:
		return iavf_del_ether_filters(sc);

	case IAVF_FLAG_AQ_DEL_VLAN_FILTER:
		return iavf_del_vlans(sc);

	case IAVF_FLAG_AQ_CONFIGURE_QUEUES:
		return iavf_configure_queues(sc);

	case IAVF_FLAG_AQ_DISABLE_QUEUES:
		return iavf_disable_queues(sc);

	case IAVF_FLAG_AQ_ENABLE_QUEUES:
		return iavf_enable_queues(sc);

	case IAVF_FLAG_AQ_CONFIG_RSS_KEY:
		return iavf_config_rss_key(sc);

	case IAVF_FLAG_AQ_SET_RSS_HENA:
		return iavf_set_rss_hena(sc);

	case IAVF_FLAG_AQ_CONFIG_RSS_LUT:
		return iavf_config_rss_lut(sc);

	case IAVF_FLAG_AQ_CONFIGURE_PROMISC:
		return iavf_config_promisc_mode(sc);
	}

	return (0);
}

/**
 * iavf_vc_get_op_chan - Get op channel for a request
 * @sc: device softc
 * @request: the request type
 *
 * @returns the op channel for the given request, or NULL if no channel is
 * used.
 */
void *
iavf_vc_get_op_chan(struct iavf_sc *sc, uint32_t request)
{
	switch (request) {
	case IAVF_FLAG_AQ_ENABLE_QUEUES:
		return (&sc->enable_queues_chan);
	case IAVF_FLAG_AQ_DISABLE_QUEUES:
		return (&sc->disable_queues_chan);
	default:
		return (NULL);
	}
}

/**
 * iavf_vc_stat_str - convert virtchnl status err code to a string
 * @hw: pointer to the HW structure
 * @stat_err: the status error code to convert
 *
 * @returns the human readable string representing the specified error code.
 **/
const char *
iavf_vc_stat_str(struct iavf_hw *hw, enum virtchnl_status_code stat_err)
{
	switch (stat_err) {
	case VIRTCHNL_STATUS_SUCCESS:
		return "OK";
	case VIRTCHNL_ERR_PARAM:
		return "VIRTCHNL_ERR_PARAM";
	case VIRTCHNL_STATUS_ERR_NO_MEMORY:
		return "VIRTCHNL_STATUS_ERR_NO_MEMORY";
	case VIRTCHNL_STATUS_ERR_OPCODE_MISMATCH:
		return "VIRTCHNL_STATUS_ERR_OPCODE_MISMATCH";
	case VIRTCHNL_STATUS_ERR_CQP_COMPL_ERROR:
		return "VIRTCHNL_STATUS_ERR_CQP_COMPL_ERROR";
	case VIRTCHNL_STATUS_ERR_INVALID_VF_ID:
		return "VIRTCHNL_STATUS_ERR_INVALID_VF_ID";
	case VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR:
		return "VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR";
	case VIRTCHNL_STATUS_NOT_SUPPORTED:
		return "VIRTCHNL_STATUS_NOT_SUPPORTED";
	}

	snprintf(hw->err_str, sizeof(hw->err_str), "%d", stat_err);
	return hw->err_str;
}

/**
 * iavf_adv_speed_to_ext_speed - Convert numeric speed to iavf speed enum
 * @adv_link_speed: link speed in Mb/s
 *
 * Converts the link speed from the "advanced" link speed virtchnl op into the
 * closest approximation of the internal iavf link speed, rounded down.
 *
 * @returns the link speed as an iavf_ext_link_speed enum value
 */
enum iavf_ext_link_speed
iavf_adv_speed_to_ext_speed(u32 adv_link_speed)
{
	if (adv_link_speed >= 100000)
		return IAVF_EXT_LINK_SPEED_100GB;
	if (adv_link_speed >= 50000)
		return IAVF_EXT_LINK_SPEED_50GB;
	if (adv_link_speed >= 40000)
		return IAVF_EXT_LINK_SPEED_40GB;
	if (adv_link_speed >= 25000)
		return IAVF_EXT_LINK_SPEED_25GB;
	if (adv_link_speed >= 20000)
		return IAVF_EXT_LINK_SPEED_20GB;
	if (adv_link_speed >= 10000)
		return IAVF_EXT_LINK_SPEED_10GB;
	if (adv_link_speed >= 5000)
		return IAVF_EXT_LINK_SPEED_5GB;
	if (adv_link_speed >= 2500)
		return IAVF_EXT_LINK_SPEED_2500MB;
	if (adv_link_speed >= 1000)
		return IAVF_EXT_LINK_SPEED_1000MB;
	if (adv_link_speed >= 100)
		return IAVF_EXT_LINK_SPEED_100MB;
	if (adv_link_speed >= 10)
		return IAVF_EXT_LINK_SPEED_10MB;

	return IAVF_EXT_LINK_SPEED_UNKNOWN;
}

/**
 * iavf_ext_speed_to_ifmedia - Convert internal iavf speed to ifmedia value
 * @link_speed: the link speed
 *
 * @remark this is sort of a hack, because we don't actually know what media
 * type the VF is running on. In an ideal world we might just report the media
 * type as "virtual" and have another mechanism for reporting the link
 * speed.
 *
 * @returns a suitable ifmedia type for the given link speed.
 */
u32
iavf_ext_speed_to_ifmedia(enum iavf_ext_link_speed link_speed)
{
	switch (link_speed) {
	case IAVF_EXT_LINK_SPEED_100GB:
		return IFM_100G_SR4;
	case IAVF_EXT_LINK_SPEED_50GB:
		return IFM_50G_SR2;
	case IAVF_EXT_LINK_SPEED_40GB:
		return IFM_40G_SR4;
	case IAVF_EXT_LINK_SPEED_25GB:
		return IFM_25G_SR;
	case IAVF_EXT_LINK_SPEED_20GB:
		return IFM_20G_KR2;
	case IAVF_EXT_LINK_SPEED_10GB:
		return IFM_10G_SR;
	case IAVF_EXT_LINK_SPEED_5GB:
		return IFM_5000_T;
	case IAVF_EXT_LINK_SPEED_2500MB:
		return IFM_2500_T;
	case IAVF_EXT_LINK_SPEED_1000MB:
		return IFM_1000_T;
	case IAVF_EXT_LINK_SPEED_100MB:
		return IFM_100_TX;
	case IAVF_EXT_LINK_SPEED_10MB:
		return IFM_10_T;
	case IAVF_EXT_LINK_SPEED_UNKNOWN:
	default:
		return IFM_UNKNOWN;
	}
}

/**
 * iavf_vc_speed_to_ext_speed - Convert virtchnl speed enum to native iavf
 * driver speed representation.
 * @link_speed: link speed enum value
 *
 * @returns the link speed in the native iavf format.
 */
enum iavf_ext_link_speed
iavf_vc_speed_to_ext_speed(enum virtchnl_link_speed link_speed)
{
	switch (link_speed) {
	case VIRTCHNL_LINK_SPEED_40GB:
		return IAVF_EXT_LINK_SPEED_40GB;
	case VIRTCHNL_LINK_SPEED_25GB:
		return IAVF_EXT_LINK_SPEED_25GB;
	case VIRTCHNL_LINK_SPEED_20GB:
		return IAVF_EXT_LINK_SPEED_20GB;
	case VIRTCHNL_LINK_SPEED_10GB:
		return IAVF_EXT_LINK_SPEED_10GB;
	case VIRTCHNL_LINK_SPEED_1GB:
		return IAVF_EXT_LINK_SPEED_1000MB;
	case VIRTCHNL_LINK_SPEED_100MB:
		return IAVF_EXT_LINK_SPEED_100MB;
	case VIRTCHNL_LINK_SPEED_UNKNOWN:
	default:
		return IAVF_EXT_LINK_SPEED_UNKNOWN;
	}
}

/**
 * iavf_vc_speed_to_string - Convert virtchnl speed to a string
 * @link_speed: the speed to convert
 *
 * @returns string representing the link speed as reported by the virtchnl
 * interface.
 */
const char *
iavf_vc_speed_to_string(enum virtchnl_link_speed link_speed)
{
	return iavf_ext_speed_to_str(iavf_vc_speed_to_ext_speed(link_speed));
}

/**
 * iavf_ext_speed_to_str - Convert iavf speed enum to string representation
 * @link_speed: link speed enum value
 *
 * XXX: This is an iavf-modified copy of ice_aq_speed_to_str()
 *
 * @returns the string representation of the given link speed.
 */
const char *
iavf_ext_speed_to_str(enum iavf_ext_link_speed link_speed)
{
	switch (link_speed) {
	case IAVF_EXT_LINK_SPEED_100GB:
		return "100 Gbps";
	case IAVF_EXT_LINK_SPEED_50GB:
		return "50 Gbps";
	case IAVF_EXT_LINK_SPEED_40GB:
		return "40 Gbps";
	case IAVF_EXT_LINK_SPEED_25GB:
		return "25 Gbps";
	case IAVF_EXT_LINK_SPEED_20GB:
		return "20 Gbps";
	case IAVF_EXT_LINK_SPEED_10GB:
		return "10 Gbps";
	case IAVF_EXT_LINK_SPEED_5GB:
		return "5 Gbps";
	case IAVF_EXT_LINK_SPEED_2500MB:
		return "2.5 Gbps";
	case IAVF_EXT_LINK_SPEED_1000MB:
		return "1 Gbps";
	case IAVF_EXT_LINK_SPEED_100MB:
		return "100 Mbps";
	case IAVF_EXT_LINK_SPEED_10MB:
		return "10 Mbps";
	case IAVF_EXT_LINK_SPEED_UNKNOWN:
	default:
		return "Unknown";
	}
}

/**
 * iavf_vc_opcode_str - Convert virtchnl opcode to string
 * @op: the virtchnl op code
 *
 * @returns the string representation of the given virtchnl op code
 */
const char *
iavf_vc_opcode_str(uint16_t op)
{
	switch (op) {
	case VIRTCHNL_OP_VERSION:
		return ("VERSION");
	case VIRTCHNL_OP_RESET_VF:
		return ("RESET_VF");
	case VIRTCHNL_OP_GET_VF_RESOURCES:
		return ("GET_VF_RESOURCES");
	case VIRTCHNL_OP_CONFIG_TX_QUEUE:
		return ("CONFIG_TX_QUEUE");
	case VIRTCHNL_OP_CONFIG_RX_QUEUE:
		return ("CONFIG_RX_QUEUE");
	case VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		return ("CONFIG_VSI_QUEUES");
	case VIRTCHNL_OP_CONFIG_IRQ_MAP:
		return ("CONFIG_IRQ_MAP");
	case VIRTCHNL_OP_ENABLE_QUEUES:
		return ("ENABLE_QUEUES");
	case VIRTCHNL_OP_DISABLE_QUEUES:
		return ("DISABLE_QUEUES");
	case VIRTCHNL_OP_ADD_ETH_ADDR:
		return ("ADD_ETH_ADDR");
	case VIRTCHNL_OP_DEL_ETH_ADDR:
		return ("DEL_ETH_ADDR");
	case VIRTCHNL_OP_ADD_VLAN:
		return ("ADD_VLAN");
	case VIRTCHNL_OP_DEL_VLAN:
		return ("DEL_VLAN");
	case VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		return ("CONFIG_PROMISCUOUS_MODE");
	case VIRTCHNL_OP_GET_STATS:
		return ("GET_STATS");
	case VIRTCHNL_OP_RSVD:
		return ("RSVD");
	case VIRTCHNL_OP_EVENT:
		return ("EVENT");
	case VIRTCHNL_OP_CONFIG_RSS_KEY:
		return ("CONFIG_RSS_KEY");
	case VIRTCHNL_OP_CONFIG_RSS_LUT:
		return ("CONFIG_RSS_LUT");
	case VIRTCHNL_OP_GET_RSS_HENA_CAPS:
		return ("GET_RSS_HENA_CAPS");
	case VIRTCHNL_OP_SET_RSS_HENA:
		return ("SET_RSS_HENA");
	default:
		return ("UNKNOWN");
	}
}

/**
 * iavf_vc_completion - Handle PF reply messages
 * @sc: device softc
 * @v_opcode: virtchnl op code
 * @v_retval: virtchnl return value
 * @msg: the message to send
 * @msglen: length of the msg buffer
 *
 * Asynchronous completion function for admin queue messages. Rather than busy
 * wait, we fire off our requests and assume that no errors will be returned.
 * This function handles the reply messages.
 */
void
iavf_vc_completion(struct iavf_sc *sc,
    enum virtchnl_ops v_opcode,
    enum virtchnl_status_code v_retval, u8 *msg, u16 msglen __unused)
{
	device_t	dev = sc->dev;

	if (v_opcode != VIRTCHNL_OP_GET_STATS)
		iavf_dbg_vc(sc, "%s: opcode %s\n", __func__,
		    iavf_vc_opcode_str(v_opcode));

	if (v_opcode == VIRTCHNL_OP_EVENT) {
		struct virtchnl_pf_event *vpe =
			(struct virtchnl_pf_event *)msg;

		switch (vpe->event) {
		case VIRTCHNL_EVENT_LINK_CHANGE:
			iavf_handle_link_event(sc, vpe);
			break;
		case VIRTCHNL_EVENT_RESET_IMPENDING:
			device_printf(dev, "PF initiated reset!\n");
			iavf_set_state(&sc->state, IAVF_STATE_RESET_PENDING);
			break;
		default:
			iavf_dbg_vc(sc, "Unknown event %d from AQ\n",
				vpe->event);
			break;
		}

		return;
	}

	/* Catch-all error response */
	if (v_retval) {
		bool print_error = true;

		switch (v_opcode) {
		case VIRTCHNL_OP_ADD_ETH_ADDR:
			device_printf(dev, "WARNING: Error adding VF mac filter!\n");
			device_printf(dev, "WARNING: Device may not receive traffic!\n");
			break;
		case VIRTCHNL_OP_ENABLE_QUEUES:
			sc->enable_queues_chan = 1;
			wakeup_one(&sc->enable_queues_chan);
			break;
		case VIRTCHNL_OP_DISABLE_QUEUES:
			sc->disable_queues_chan = 1;
			wakeup_one(&sc->disable_queues_chan);
			/* This may fail, but it does not necessarily mean that
			 * something is critically wrong.
			 */
			if (!(sc->dbg_mask & IAVF_DBG_VC))
				print_error = false;
			break;
		default:
			break;
		}

		if (print_error)
			device_printf(dev,
			    "%s: AQ returned error %s to our request %s!\n",
			    __func__, iavf_vc_stat_str(&sc->hw, v_retval),
			    iavf_vc_opcode_str(v_opcode));
		return;
	}

	switch (v_opcode) {
	case VIRTCHNL_OP_GET_STATS:
		iavf_update_stats_counters(sc, (struct iavf_eth_stats *)msg);
		break;
	case VIRTCHNL_OP_ADD_ETH_ADDR:
		break;
	case VIRTCHNL_OP_DEL_ETH_ADDR:
		break;
	case VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		break;
	case VIRTCHNL_OP_ADD_VLAN:
		break;
	case VIRTCHNL_OP_DEL_VLAN:
		break;
	case VIRTCHNL_OP_ENABLE_QUEUES:
		atomic_store_rel_32(&sc->queues_enabled, 1);
		sc->enable_queues_chan = 1;
		wakeup_one(&sc->enable_queues_chan);
		break;
	case VIRTCHNL_OP_DISABLE_QUEUES:
		atomic_store_rel_32(&sc->queues_enabled, 0);
		sc->disable_queues_chan = 1;
		wakeup_one(&sc->disable_queues_chan);
		break;
	case VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		break;
	case VIRTCHNL_OP_CONFIG_IRQ_MAP:
		break;
	case VIRTCHNL_OP_CONFIG_RSS_KEY:
		break;
	case VIRTCHNL_OP_SET_RSS_HENA:
		break;
	case VIRTCHNL_OP_CONFIG_RSS_LUT:
		break;
	default:
		iavf_dbg_vc(sc,
		    "Received unexpected message %s from PF.\n",
		    iavf_vc_opcode_str(v_opcode));
		break;
	}
}

/**
 * iavf_handle_link_event - Handle Link event virtchml message
 * @sc: device softc
 * @vpe: virtchnl PF link event structure
 *
 * Process a virtchnl PF link event and update the driver and stack status of
 * the link event.
 */
static void
iavf_handle_link_event(struct iavf_sc *sc, struct virtchnl_pf_event *vpe)
{
	MPASS(vpe->event == VIRTCHNL_EVENT_LINK_CHANGE);

	if (sc->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_ADV_LINK_SPEED)
	{
		iavf_dbg_vc(sc, "Link change (adv): status %d, speed %u\n",
		    vpe->event_data.link_event_adv.link_status,
		    vpe->event_data.link_event_adv.link_speed);
		sc->link_up =
			vpe->event_data.link_event_adv.link_status;
		sc->link_speed_adv =
			vpe->event_data.link_event_adv.link_speed;

	} else {
		iavf_dbg_vc(sc, "Link change: status %d, speed %x\n",
		    vpe->event_data.link_event.link_status,
		    vpe->event_data.link_event.link_speed);
		sc->link_up =
			vpe->event_data.link_event.link_status;
		sc->link_speed =
			vpe->event_data.link_event.link_speed;
	}

	iavf_update_link_status(sc);
}
