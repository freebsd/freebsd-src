/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
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

/**
 * @file ice_rdma.c
 * @brief RDMA client driver interface
 *
 * Functions to interface with the RDMA client driver, for enabling RMDA
 * functionality for the ice driver.
 *
 * The RDMA client interface is based on a simple kobject interface which is
 * defined by the rmda_if.m and irdma_di_if.m interfaces.
 *
 * The ice device driver provides the rmda_di_if.m interface methods, while
 * the client RDMA driver provides the irdma_if.m interface methods as an
 * extension ontop of the irdma_di_if kobject.
 *
 * The initial connection between drivers is done via the RDMA client driver
 * calling ice_rdma_register.
 */

#include "ice_iflib.h"
#include "ice_rdma_internal.h"

#include "irdma_if.h"
#include "irdma_di_if.h"

/**
 * @var ice_rdma
 * @brief global RDMA driver state
 *
 * Contains global state the driver uses to connect to a client RDMA interface
 * driver.
 */
static struct ice_rdma_state ice_rdma;

/*
 * Helper function prototypes
 */
static int ice_rdma_pf_attach_locked(struct ice_softc *sc);
static void ice_rdma_pf_detach_locked(struct ice_softc *sc);
static int ice_rdma_check_version(struct ice_rdma_info *info);
static void ice_rdma_cp_qos_info(struct ice_hw *hw,
				 struct ice_dcbx_cfg *dcbx_cfg,
				 struct ice_qos_params *qos_info);

/*
 * RDMA Device Interface prototypes
 */
static int ice_rdma_pf_reset(struct ice_rdma_peer *peer);
static int ice_rdma_pf_msix_init(struct ice_rdma_peer *peer,
				 struct ice_rdma_msix_mapping *msix_info);
static int ice_rdma_qset_register_request(struct ice_rdma_peer *peer,
			     struct ice_rdma_qset_update *res);
static int ice_rdma_update_vsi_filter(struct ice_rdma_peer *peer_dev,
				      bool enable);
static void ice_rdma_request_handler(struct ice_rdma_peer *peer,
				     struct ice_rdma_request *req);


/**
 * @var ice_rdma_di_methods
 * @brief RDMA driver interface methods
 *
 * Kobject methods implementing the driver-side interface for the RDMA peer
 * clients. This method table contains the operations which the client can
 * request from the driver.
 *
 * The client driver will then extend this kobject class with methods that the
 * driver can request from the client.
 */
static kobj_method_t ice_rdma_di_methods[] = {
	KOBJMETHOD(irdma_di_reset, ice_rdma_pf_reset),
	KOBJMETHOD(irdma_di_msix_init, ice_rdma_pf_msix_init),
	KOBJMETHOD(irdma_di_qset_register_request, ice_rdma_qset_register_request),
	KOBJMETHOD(irdma_di_vsi_filter_update, ice_rdma_update_vsi_filter),
	KOBJMETHOD(irdma_di_req_handler, ice_rdma_request_handler),
	KOBJMETHOD_END
};

/* Define ice_rdma_di class which will be extended by the iRDMA driver */
DEFINE_CLASS_0(ice_rdma_di, ice_rdma_di_class, ice_rdma_di_methods, sizeof(struct ice_rdma_peer));

/**
 * ice_rdma_pf_reset - RDMA client interface requested a reset
 * @peer: the RDMA peer client structure
 *
 * Implements IRDMA_DI_RESET, called by the RDMA client driver to request
 * a reset of an ice driver device.
 * @return 0 on success
 */
static int
ice_rdma_pf_reset(struct ice_rdma_peer *peer)
{
	struct ice_softc *sc = ice_rdma_peer_to_sc(peer);

	/* Tell the base driver that RDMA is requesting a PFR */
	ice_set_state(&sc->state, ICE_STATE_RESET_PFR_REQ);

	/* XXX: Base driver will notify RDMA when it's done */

	return (0);
}

/**
 * ice_rdma_pf_msix_init - RDMA client interface request MSI-X initialization
 * @peer: the RDMA peer client structure
 * @msix_info: requested MSI-X mapping
 *
 * Implements IRDMA_DI_MSIX_INIT, called by the RDMA client driver to
 * initialize the MSI-X resources required for RDMA functionality.
 * @returns ENOSYS
 */
static int
ice_rdma_pf_msix_init(struct ice_rdma_peer *peer,
		      struct ice_rdma_msix_mapping __unused *msix_info)
{
	struct ice_softc *sc = ice_rdma_peer_to_sc(peer);

	MPASS(msix_info != NULL);

	device_printf(sc->dev, "%s: iRDMA MSI-X initialization request is not yet implemented\n", __func__);

	/* TODO: implement MSI-X initialization for RDMA */
	return (ENOSYS);
}

/**
 * ice_rdma_register_request - RDMA client interface request qset
 *                             registration or unregistration
 * @peer: the RDMA peer client structure
 * @res: resources to be registered or unregistered
 * @returns 0 on success, EINVAL on argument issues, ENOMEM on memory
 * allocation failure, EXDEV on vsi device mismatch
 */
static int
ice_rdma_qset_register_request(struct ice_rdma_peer *peer, struct ice_rdma_qset_update *res)
{
	struct ice_softc *sc = ice_rdma_peer_to_sc(peer);
	struct ice_vsi *vsi = NULL;
	struct ice_dcbx_cfg *dcbx_cfg;
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;
	int count, i, ret = 0;
	uint32_t *qset_teid;
	uint16_t *qs_handle;
	uint16_t max_rdmaqs[ICE_MAX_TRAFFIC_CLASS];
	uint16_t vsi_id;
	uint8_t ena_tc = 0;

	if (!res)
		return -EINVAL;

	if (res->cnt_req > ICE_MAX_TXQ_PER_TXQG)
		return -EINVAL;

	switch(res->res_type) {
	case ICE_RDMA_QSET_ALLOC:
		count = res->cnt_req;
		vsi_id = peer->pf_vsi_num;
		break;
	case ICE_RDMA_QSET_FREE:
		count = res->res_allocated;
		vsi_id = res->qsets.vsi_id;
		break;
	default:
		return -EINVAL;
	}
	qset_teid = (uint32_t *)ice_calloc(hw, count, sizeof(*qset_teid));
	if (!qset_teid)
		return -ENOMEM;

	qs_handle = (uint16_t *)ice_calloc(hw, count, sizeof(*qs_handle));
	if (!qs_handle) {
		ice_free(hw, qset_teid);
		return -ENOMEM;
	}

	ice_for_each_traffic_class(i)
		max_rdmaqs[i] = 0;
	for (i = 0; i < sc->num_available_vsi; i++) {
		if (sc->all_vsi[i] &&
		    ice_get_hw_vsi_num(hw, sc->all_vsi[i]->idx) == vsi_id) {
			vsi = sc->all_vsi[i];
			break;
		}
	}

	if (!vsi) {
		ice_debug(hw, ICE_DBG_RDMA, "RDMA QSet invalid VSI\n");
		ret = -EINVAL;
		goto out;
	}
	if (sc != vsi->sc) {
		ice_debug(hw, ICE_DBG_RDMA, "VSI is tied to unexpected device\n");
		ret = -EXDEV;
		goto out;
	}

	for (i = 0; i < count; i++) {
		struct ice_rdma_qset_params *qset;

		qset = &res->qsets;
		if (qset->vsi_id != peer->pf_vsi_num) {
			ice_debug(hw, ICE_DBG_RDMA, "RDMA QSet invalid VSI requested %d %d\n",
				  qset->vsi_id, peer->pf_vsi_num);
			ret = -EINVAL;
			goto out;
		}
		max_rdmaqs[qset->tc]++;
		qs_handle[i] = qset->qs_handle;
		qset_teid[i] = qset->teid;
	}

	switch(res->res_type) {
	case ICE_RDMA_QSET_ALLOC:
		dcbx_cfg = &hw->port_info->qos_cfg.local_dcbx_cfg;
		ena_tc = ice_dcb_get_tc_map(dcbx_cfg);

		ice_debug(hw, ICE_DBG_RDMA, "%s:%d ena_tc=%x\n", __func__, __LINE__, ena_tc);
		status = ice_cfg_vsi_rdma(hw->port_info, vsi->idx, ena_tc,
					  max_rdmaqs);
		if (status) {
			ice_debug(hw, ICE_DBG_RDMA, "Failed VSI RDMA qset config\n");
			ret = -EINVAL;
			goto out;
		}

		for (i = 0; i < count; i++) {
			struct ice_rdma_qset_params *qset;

			qset = &res->qsets;
			status = ice_ena_vsi_rdma_qset(hw->port_info, vsi->idx,
						       qset->tc, &qs_handle[i], 1,
						       &qset_teid[i]);
			if (status) {
				ice_debug(hw, ICE_DBG_RDMA, "Failed VSI RDMA qset enable\n");
				ret = -EINVAL;
				goto out;
			}
			qset->teid = qset_teid[i];
		}
		break;
	case ICE_RDMA_QSET_FREE:
		status = ice_dis_vsi_rdma_qset(hw->port_info, count, qset_teid, qs_handle);
		if (status)
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

out:
	ice_free(hw, qs_handle);
	ice_free(hw, qset_teid);

	return ret;
}

/**
 *  ice_rdma_update_vsi_filter - configure vsi information
 *                               when opening or closing rdma driver
 *  @peer: the RDMA peer client structure
 *  @enable: enable or disable the rdma filter
 *  @return 0 on success, EINVAL on wrong vsi
 */
static int
ice_rdma_update_vsi_filter(struct ice_rdma_peer *peer,
			   bool enable)
{
	struct ice_softc *sc = ice_rdma_peer_to_sc(peer);
	struct ice_vsi *vsi;
	int ret;

	vsi = &sc->pf_vsi;
	if (!vsi)
		return -EINVAL;

	ret = ice_cfg_iwarp_fltr(&sc->hw, vsi->idx, enable);
	if (ret) {
		device_printf(sc->dev, "Failed to  %sable iWARP filtering\n",
				enable ? "en" : "dis");
	} else {
		if (enable)
			vsi->info.q_opt_flags |= ICE_AQ_VSI_Q_OPT_PE_FLTR_EN;
		else
			vsi->info.q_opt_flags &= ~ICE_AQ_VSI_Q_OPT_PE_FLTR_EN;
	}

	return ret;
}

/**
 * ice_rdma_request_handler - handle requests incoming from RDMA driver
 * @peer: the RDMA peer client structure
 * @req: structure containing request
 */
static void
ice_rdma_request_handler(struct ice_rdma_peer *peer,
			 struct ice_rdma_request *req)
{
	if (!req || !peer) {
		log(LOG_WARNING, "%s: peer or req are not valid\n", __func__);
		return;
	}

	switch(req->type) {
	case ICE_RDMA_EVENT_RESET:
		ice_rdma_pf_reset(peer);
		break;
	case ICE_RDMA_EVENT_QSET_REGISTER:
		ice_rdma_qset_register_request(peer, &req->res);
		break;
	case ICE_RDMA_EVENT_VSI_FILTER_UPDATE:
		ice_rdma_update_vsi_filter(peer, req->enable_filter);
		break;
	default:
		log(LOG_WARNING, "%s: Event %d not supported\n", __func__, req->type);
		break;
	}
}

/**
 * ice_rdma_cp_qos_info - gather current QOS/DCB settings in LAN to pass
 *                        to RDMA driver
 * @hw: ice hw structure
 * @dcbx_cfg: current DCB settings in ice driver
 * @qos_info: destination of the DCB settings
 */
static void
ice_rdma_cp_qos_info(struct ice_hw *hw, struct ice_dcbx_cfg *dcbx_cfg,
		     struct ice_qos_params *qos_info)
{
	u32 up2tc;
	u8 j;
	u8 num_tc = 0;
	u8 val_tc = 0;  /* number of TC for validation */
	u8 cnt_tc = 0;

	/* setup qos_info fields with defaults */
	qos_info->num_apps = 0;
	qos_info->num_tc = 1;

	for (j = 0; j < ICE_TC_MAX_USER_PRIORITY; j++)
		qos_info->up2tc[j] = 0;

	qos_info->tc_info[0].rel_bw = 100;
	for (j = 1; j < IEEE_8021QAZ_MAX_TCS; j++)
		qos_info->tc_info[j].rel_bw = 0;

	/* gather current values */
	up2tc = rd32(hw, PRTDCB_TUP2TC);
	qos_info->num_apps = dcbx_cfg->numapps;

	for (j = 0; j < ICE_MAX_TRAFFIC_CLASS; j++) {
		num_tc |= BIT(dcbx_cfg->etscfg.prio_table[j]);
	}
	for (j = 0; j < ICE_MAX_TRAFFIC_CLASS; j++) {
		if (num_tc & BIT(j)) {
			cnt_tc++;
			val_tc |= BIT(j);
		} else {
			break;
		}
	}
	qos_info->num_tc = (val_tc == num_tc && num_tc != 0) ? cnt_tc : 1;
	for (j = 0; j < ICE_TC_MAX_USER_PRIORITY; j++)
		qos_info->up2tc[j] = (up2tc >> (j * 3)) & 0x7;

	for (j = 0; j < IEEE_8021QAZ_MAX_TCS; j++)
		qos_info->tc_info[j].rel_bw = dcbx_cfg->etscfg.tcbwtable[j];
	for (j = 0; j < qos_info->num_apps; j++) {
		qos_info->apps[j].priority = dcbx_cfg->app[j].priority;
		qos_info->apps[j].prot_id = dcbx_cfg->app[j].prot_id;
		qos_info->apps[j].selector = dcbx_cfg->app[j].selector;
	}

	/* Gather DSCP-to-TC mapping and QoS/PFC mode */
	memcpy(qos_info->dscp_map, dcbx_cfg->dscp_map, sizeof(qos_info->dscp_map));
	qos_info->pfc_mode = dcbx_cfg->pfc_mode;
}

/**
 * ice_rdma_check_version - Check that the provided RDMA version is compatible
 * @info: the RDMA client information structure
 *
 * Verify that the client RDMA driver provided a version that is compatible
 * with the driver interface.
 * @return 0 on success, ENOTSUP when LAN-RDMA interface version doesn't match,
 * EINVAL on kobject interface fail.
 */
static int
ice_rdma_check_version(struct ice_rdma_info *info)
{
	/* Make sure the MAJOR version matches */
	if (info->major_version != ICE_RDMA_MAJOR_VERSION) {
		log(LOG_WARNING, "%s: the iRDMA driver requested version %d.%d.%d, but this driver only supports major version %d.x.x\n",
		    __func__,
		    info->major_version, info->minor_version, info->patch_version,
		    ICE_RDMA_MAJOR_VERSION);
		return (ENOTSUP);
	}

	/*
	 * Make sure that the MINOR version is compatible.
	 *
	 * This means that the RDMA client driver version MUST not be greater
	 * than the version provided by the driver, as it would indicate that
	 * the RDMA client expects features which are not supported by the
	 * main driver.
	 */
	if (info->minor_version > ICE_RDMA_MINOR_VERSION) {
		log(LOG_WARNING, "%s: the iRDMA driver requested version %d.%d.%d, but this driver only supports up to minor version %d.%d.x\n",
		__func__,
		info->major_version, info->minor_version, info->patch_version,
		ICE_RDMA_MAJOR_VERSION, ICE_RDMA_MINOR_VERSION);
		return (ENOTSUP);
	}

	/*
	 * Make sure that the PATCH version is compatible.
	 *
	 * This means that the RDMA client version MUST not be greater than
	 * the version provided by the driver, as it may indicate that the
	 * RDMA client expects certain backwards compatible bug fixes which
	 * are not implemented by this version of the main driver.
	 */
	if ((info->minor_version == ICE_RDMA_MINOR_VERSION) &&
	    (info->patch_version > ICE_RDMA_PATCH_VERSION)) {
		log(LOG_WARNING, "%s: the iRDMA driver requested version %d.%d.%d, but this driver only supports up to patch version %d.%d.%d\n",
		__func__,
		info->major_version, info->minor_version, info->patch_version,
		ICE_RDMA_MAJOR_VERSION, ICE_RDMA_MINOR_VERSION, ICE_RDMA_PATCH_VERSION);
		return (ENOTSUP);
	}

	/* Make sure that the kobject class is initialized */
	if (info->rdma_class == NULL) {
		log(LOG_WARNING, "%s: the iRDMA driver did not specify a kobject interface\n",
		    __func__);
		return (EINVAL);
	}

	return (0);
}

/**
 * ice_rdma_register - Register an RDMA client driver
 * @info: the RDMA client information structure
 *
 * Called by the RDMA client driver on load. Used to initialize the RDMA
 * client driver interface and enable interop between the ice driver and the
 * RDMA client driver.
 *
 * The RDMA client driver must provide the version number it expects, along
 * with a pointer to a kobject class that extends the irdma_di_if class, and
 * implements the irdma_if class interface.
 * @return 0 on success, ECONNREFUSED when RDMA is turned off, EBUSY when irdma
 * already registered, ENOTSUP when LAN-RDMA interface version doesn't match,
 * EINVAL on kobject interface fail.
 */
int
ice_rdma_register(struct ice_rdma_info *info)
{
	struct ice_rdma_entry *entry;
	struct ice_softc *sc;
	int err = 0;

	sx_xlock(&ice_rdma.mtx);

	if (!ice_enable_irdma) {
		log(LOG_INFO, "%s: The iRDMA driver interface has been disabled\n", __func__);
		err = (ECONNREFUSED);
		goto return_unlock;
	}

	if (ice_rdma.registered) {
		log(LOG_WARNING, "%s: iRDMA driver already registered\n", __func__);
		err = (EBUSY);
		goto return_unlock;
	}

	/* Make sure the iRDMA version is compatible */
	err = ice_rdma_check_version(info);
	if (err)
		goto return_unlock;

	log(LOG_INFO, "%s: iRDMA driver registered using version %d.%d.%d\n",
	    __func__, info->major_version, info->minor_version, info->patch_version);

	ice_rdma.peer_class = info->rdma_class;

	/*
	 * Initialize the kobject interface and notify the RDMA client of each
	 * existing PF interface.
	 */
	LIST_FOREACH(entry, &ice_rdma.peers, node) {
		kobj_init((kobj_t)&entry->peer, ice_rdma.peer_class);
		/* Gather DCB/QOS info into peer */
		sc = __containerof(entry, struct ice_softc, rdma_entry);
		memset(&entry->peer.initial_qos_info, 0, sizeof(entry->peer.initial_qos_info));
		ice_rdma_cp_qos_info(&sc->hw, &sc->hw.port_info->qos_cfg.local_dcbx_cfg,
				     &entry->peer.initial_qos_info);

		IRDMA_PROBE(&entry->peer);
		if (entry->initiated)
			IRDMA_OPEN(&entry->peer);
	}
	ice_rdma.registered = true;

return_unlock:
	sx_xunlock(&ice_rdma.mtx);

	return (err);
}

/**
 * ice_rdma_unregister - Unregister an RDMA client driver
 *
 * Called by the RDMA client driver on unload. Used to de-initialize the RDMA
 * client driver interface and shut down communication between the ice driver
 * and the RDMA client driver.
 * @return 0 on success, ENOENT when irdma driver wasn't registered
 */
int
ice_rdma_unregister(void)
{
	struct ice_rdma_entry *entry;

	sx_xlock(&ice_rdma.mtx);

	if (!ice_rdma.registered) {
		log(LOG_WARNING, "%s: iRDMA driver was not previously registered\n",
		       __func__);
		sx_xunlock(&ice_rdma.mtx);
		return (ENOENT);
	}

	log(LOG_INFO, "%s: iRDMA driver unregistered\n", __func__);
	ice_rdma.registered = false;
	ice_rdma.peer_class = NULL;

	/*
	 * Release the kobject interface for each of the existing PF
	 * interfaces. Note that we do not notify the client about removing
	 * each PF, as it is assumed that the client will have already cleaned
	 * up any associated resources when it is unregistered.
	 */
	LIST_FOREACH(entry, &ice_rdma.peers, node)
		kobj_delete((kobj_t)&entry->peer, NULL);

	sx_xunlock(&ice_rdma.mtx);

	return (0);
}

/**
 * ice_rdma_init - RDMA driver init routine
 *
 * Called during ice driver module initialization to setup the RDMA client
 * interface mutex and RDMA peer structure list.
 */
void
ice_rdma_init(void)
{
	LIST_INIT(&ice_rdma.peers);
	sx_init_flags(&ice_rdma.mtx, "ice rdma interface", SX_DUPOK);

	ice_rdma.registered = false;
	ice_rdma.peer_class = NULL;
}

/**
 * ice_rdma_exit - RDMA driver exit routine
 *
 * Called during ice driver module exit to shutdown the RDMA client interface
 * mutex.
 */
void
ice_rdma_exit(void)
{
	MPASS(LIST_EMPTY(&ice_rdma.peers));
	sx_destroy(&ice_rdma.mtx);
}

/**
 * ice_rdma_pf_attach_locked - Prepare a PF for RDMA connections
 * @sc: the ice driver softc
 *
 * Initialize a peer entry for this PF and add it to the RDMA interface list.
 * Notify the client RDMA driver of a new PF device.
 *
 * @pre must be called while holding the ice_rdma mutex.
 * @return 0 on success and when RDMA feature is not available, EEXIST when
 * irdma is already attached
 */
static int
ice_rdma_pf_attach_locked(struct ice_softc *sc)
{
	struct ice_rdma_entry *entry;

	/* Do not attach the PF unless RDMA is supported */
	if (!ice_is_bit_set(sc->feat_cap, ICE_FEATURE_RDMA))
		return (0);

	entry = &sc->rdma_entry;
	if (entry->attached) {
		device_printf(sc->dev, "iRDMA peer entry already exists\n");
		return (EEXIST);
	}

	entry->attached = true;
	entry->peer.dev = sc->dev;
	entry->peer.ifp = sc->ifp;
	entry->peer.pf_id = sc->hw.pf_id;
	entry->peer.pci_mem = sc->bar0.res;
	entry->peer.pf_vsi_num = ice_get_hw_vsi_num(&sc->hw, sc->pf_vsi.idx);
	if (sc->rdma_imap && sc->rdma_imap[0] != ICE_INVALID_RES_IDX &&
	    sc->irdma_vectors > 0) {
		entry->peer.msix.base = sc->rdma_imap[0];
		entry->peer.msix.count = sc->irdma_vectors;
	}

	/* Gather DCB/QOS info into peer */
	memset(&entry->peer.initial_qos_info, 0, sizeof(entry->peer.initial_qos_info));
	ice_rdma_cp_qos_info(&sc->hw, &sc->hw.port_info->qos_cfg.local_dcbx_cfg,
			     &entry->peer.initial_qos_info);

	/*
	 * If the RDMA client driver has already registered, initialize the
	 * kobject and notify the client of a new PF
	 */
	if (ice_rdma.registered) {
		kobj_init((kobj_t)&entry->peer, ice_rdma.peer_class);
		IRDMA_PROBE(&entry->peer);
	}

	LIST_INSERT_HEAD(&ice_rdma.peers, entry, node);

	ice_set_bit(ICE_FEATURE_RDMA, sc->feat_en);

	return (0);
}

/**
 * ice_rdma_pf_attach - Notify the RDMA client of a new PF
 * @sc: the ice driver softc
 *
 * Called during PF attach to notify the RDMA client of a new PF.
 * @return 0 or EEXIST if irdma was already attached
 */
int
ice_rdma_pf_attach(struct ice_softc *sc)
{
	int err;

	sx_xlock(&ice_rdma.mtx);
	err = ice_rdma_pf_attach_locked(sc);
	sx_xunlock(&ice_rdma.mtx);

	return (err);
}

/**
 * ice_rdma_pf_detach_locked - Notify the RDMA client on PF detach
 * @sc: the ice driver softc
 *
 * Notify the RDMA peer client driver of removal of a PF, and release any
 * RDMA-specific resources associated with that PF. Remove the PF from the
 * list of available RDMA entries.
 *
 * @pre must be called while holding the ice_rdma mutex.
 */
static void
ice_rdma_pf_detach_locked(struct ice_softc *sc)
{
	struct ice_rdma_entry *entry;

	/* No need to detach the PF if RDMA is not enabled */
	if (!ice_is_bit_set(sc->feat_en, ICE_FEATURE_RDMA))
		return;

	entry = &sc->rdma_entry;
	if (!entry->attached) {
		device_printf(sc->dev, "iRDMA peer entry was not attached\n");
		return;
	}

	/*
	 * If the RDMA client driver is registered, notify the client that
	 * a PF has been removed, and release the kobject reference.
	 */
	if (ice_rdma.registered) {
		IRDMA_REMOVE(&entry->peer);
		kobj_delete((kobj_t)&entry->peer, NULL);
	}

	LIST_REMOVE(entry, node);
	entry->attached = false;

	ice_clear_bit(ICE_FEATURE_RDMA, sc->feat_en);
}

/**
 * ice_rdma_pf_detach - Notify the RDMA client of a PF detaching
 * @sc: the ice driver softc
 *
 * Take the ice_rdma mutex and then notify the RDMA client that a PF has been
 * removed.
 */
void
ice_rdma_pf_detach(struct ice_softc *sc)
{
	sx_xlock(&ice_rdma.mtx);
	ice_rdma_pf_detach_locked(sc);
	sx_xunlock(&ice_rdma.mtx);
}

/**
 * ice_rdma_pf_init - Notify the RDMA client that a PF has initialized
 * @sc: the ice driver softc
 *
 * Called by the ice driver when a PF has been initialized. Notifies the RDMA
 * client that a PF is up and ready to operate.
 * @return 0 on success, propagates IRDMA_OPEN return value
 */
int
ice_rdma_pf_init(struct ice_softc *sc)
{
	struct ice_rdma_peer *peer = &sc->rdma_entry.peer;

	sx_xlock(&ice_rdma.mtx);

	/* Update the MTU */
	peer->mtu = if_getmtu(sc->ifp);
	sc->rdma_entry.initiated = true;

	if (sc->rdma_entry.attached && ice_rdma.registered) {
		sx_xunlock(&ice_rdma.mtx);
		return IRDMA_OPEN(peer);
	}

	sx_xunlock(&ice_rdma.mtx);

	return (0);
}

/**
 * ice_rdma_pf_stop - Notify the RDMA client of a stopped PF device
 * @sc: the ice driver softc
 *
 * Called by the ice driver when a PF is stopped. Notifies the RDMA client
 * driver that the PF has stopped and is not ready to operate.
 * @return 0 on success
 */
int
ice_rdma_pf_stop(struct ice_softc *sc)
{
	sx_xlock(&ice_rdma.mtx);

	sc->rdma_entry.initiated = false;
	if (sc->rdma_entry.attached && ice_rdma.registered) {
		sx_xunlock(&ice_rdma.mtx);
		return IRDMA_CLOSE(&sc->rdma_entry.peer);
	}

	sx_xunlock(&ice_rdma.mtx);

	return (0);
}

/**
 * ice_rdma_link_change - Notify RDMA client of a change in link status
 * @sc: the ice driver softc
 * @linkstate: the link status
 * @baudrate: the link rate in bits per second
 *
 * Notify the RDMA client of a link status change, by sending it the new link
 * state and baudrate.
 *
 * The link state is represented the same was as in the ifnet structure. It
 * should be LINK_STATE_UNKNOWN, LINK_STATE_DOWN, or LINK_STATE_UP.
 */
void
ice_rdma_link_change(struct ice_softc *sc, int linkstate, uint64_t baudrate)
{
	struct ice_rdma_peer *peer = &sc->rdma_entry.peer;
	struct ice_rdma_event event;

	memset(&event, 0, sizeof(struct ice_rdma_event));
	event.type = ICE_RDMA_EVENT_LINK_CHANGE;
	event.linkstate = linkstate;
	event.baudrate = baudrate;

	sx_xlock(&ice_rdma.mtx);

	if (sc->rdma_entry.attached && ice_rdma.registered)
		IRDMA_EVENT_HANDLER(peer, &event);

	sx_xunlock(&ice_rdma.mtx);
}

/**
 *  ice_rdma_notify_dcb_qos_change - notify RDMA driver to pause traffic
 *  @sc: the ice driver softc
 *
 *  Notify the RDMA driver that QOS/DCB settings are about to change.
 *  Once the function return, all the QPs should be suspended.
 */
void
ice_rdma_notify_dcb_qos_change(struct ice_softc *sc)
{
	struct ice_rdma_peer *peer = &sc->rdma_entry.peer;
	struct ice_rdma_event event;

	memset(&event, 0, sizeof(struct ice_rdma_event));
	event.type = ICE_RDMA_EVENT_TC_CHANGE;
	/* pre-event */
	event.prep = true;

	sx_xlock(&ice_rdma.mtx);
	if (sc->rdma_entry.attached && ice_rdma.registered)
		IRDMA_EVENT_HANDLER(peer, &event);
	sx_xunlock(&ice_rdma.mtx);
}

/**
 *  ice_rdma_dcb_qos_update - pass the changed dcb settings to RDMA driver
 *  @sc: the ice driver softc
 *  @pi: the port info structure
 *
 *  Pass the changed DCB settings to RDMA traffic. This function should be
 *  called only after ice_rdma_notify_dcb_qos_change has been called and
 *  returned before. After the function returns, all the RDMA traffic
 *  should be resumed.
 */
void
ice_rdma_dcb_qos_update(struct ice_softc *sc, struct ice_port_info *pi)
{
	struct ice_rdma_peer *peer = &sc->rdma_entry.peer;
	struct ice_rdma_event event;

	memset(&event, 0, sizeof(struct ice_rdma_event));
	event.type = ICE_RDMA_EVENT_TC_CHANGE;
	/* post-event */
	event.prep = false;

	/* gather current configuration */
	ice_rdma_cp_qos_info(&sc->hw, &pi->qos_cfg.local_dcbx_cfg, &event.port_qos);
	sx_xlock(&ice_rdma.mtx);
	if (sc->rdma_entry.attached && ice_rdma.registered)
		IRDMA_EVENT_HANDLER(peer, &event);
	sx_xunlock(&ice_rdma.mtx);
}

/**
 *  ice_rdma_notify_pe_intr - notify irdma on incoming interrupts regarding PE
 *  @sc: the ice driver softc
 *  @oicr: interrupt cause
 *
 *  Pass the information about received interrupt to RDMA driver if it was
 *  relating to PE. Specifically PE_CRITERR and HMC_ERR.
 *  The irdma driver shall decide what should be done upon these interrupts.
 */
void
ice_rdma_notify_pe_intr(struct ice_softc *sc, uint32_t oicr)
{
	struct ice_rdma_peer *peer = &sc->rdma_entry.peer;
	struct ice_rdma_event event;

	memset(&event, 0, sizeof(struct ice_rdma_event));
	event.type = ICE_RDMA_EVENT_CRIT_ERR;
	event.oicr_reg = oicr;

	sx_xlock(&ice_rdma.mtx);
	if (sc->rdma_entry.attached && ice_rdma.registered)
		IRDMA_EVENT_HANDLER(peer, &event);
	sx_xunlock(&ice_rdma.mtx);
}

/**
 *  ice_rdma_notify_reset - notify irdma on incoming pf-reset
 *  @sc: the ice driver softc
 *
 *  Inform irdma driver of an incoming PF reset.
 *  The irdma driver shall set its state to reset, and avoid using CQP
 *  anymore. Next step should be to call ice_rdma_pf_stop in order to
 *  remove resources.
 */
void
ice_rdma_notify_reset(struct ice_softc *sc)
{
	struct ice_rdma_peer *peer = &sc->rdma_entry.peer;
	struct ice_rdma_event event;

	memset(&event, 0, sizeof(struct ice_rdma_event));
	event.type = ICE_RDMA_EVENT_RESET;

	sx_xlock(&ice_rdma.mtx);
	if (sc->rdma_entry.attached && ice_rdma.registered)
	        IRDMA_EVENT_HANDLER(peer, &event);
	sx_xunlock(&ice_rdma.mtx);
}
