/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2025, Intel Corporation
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
 * @file ice_iov.c
 * @brief Virtualization support functions
 *
 * Contains functions for enabling and managing PCIe virtual function devices,
 * including enabling new VFs, and managing VFs over the virtchnl interface.
 */

#include "ice_iov.h"
#include "ice_fault.h"

#include <net/if_vf_status.h>

/* Version 1 driver.ice extension schema; documented in ice(4). */
#define	ICE_VF_STATUS_NAMESPACE			"driver.ice"
#define	ICE_VF_STATUS_VERSION			1
#define	ICE_VF_STATUS_MIRROR_CONFIGURED		"mirror-configured"
#define	ICE_VF_STATUS_MIRROR_SOURCE_VSI		"mirror-source-vsi"
#define	ICE_VF_STATUS_MIRROR_INGRESS_ACTIVE	"mirror-ingress-active"
#define	ICE_VF_STATUS_MIRROR_EGRESS_ACTIVE	"mirror-egress-active"
#define	ICE_VF_STATUS_MDD_BLOCKED		"mdd-blocked"
#define	ICE_VF_STATUS_MDD_TX_EVENTS		"mdd-tx-events"
#define	ICE_VF_STATUS_MDD_RX_EVENTS		"mdd-rx-events"
#define	ICE_VF_STATUS_MBX_BLOCKED		"mailbox-blocked"
#define	ICE_VF_STATUS_MBX_OVERFLOW_EVENTS	"mailbox-overflow-events"
#define	ICE_VF_STATUS_MAC_FILTER_COUNT		"mac-filter-count"
#define	ICE_VF_STATUS_MAC_FILTER_LIMIT		"mac-filter-limit"
#define	ICE_VF_STATUS_RESET_FAILED		"reset-failed"
#define	ICE_VF_STATUS_REBUILD_REQUIRED		"rebuild-required"

/* Optional fields are compacted when absent; values define schema order. */
enum ice_vf_status_field {
	ICE_VF_STATUS_FIELD_MIRROR_CONFIGURED,
	ICE_VF_STATUS_FIELD_MIRROR_SOURCE_VSI,
	ICE_VF_STATUS_FIELD_MIRROR_INGRESS_ACTIVE,
	ICE_VF_STATUS_FIELD_MIRROR_EGRESS_ACTIVE,
	ICE_VF_STATUS_FIELD_MDD_BLOCKED,
	ICE_VF_STATUS_FIELD_MDD_TX_EVENTS,
	ICE_VF_STATUS_FIELD_MDD_RX_EVENTS,
	ICE_VF_STATUS_FIELD_MBX_BLOCKED,
	ICE_VF_STATUS_FIELD_MBX_OVERFLOW_EVENTS,
	ICE_VF_STATUS_FIELD_MAC_FILTER_COUNT,
	ICE_VF_STATUS_FIELD_MAC_FILTER_LIMIT,
	ICE_VF_STATUS_FIELD_RESET_FAILED,
	ICE_VF_STATUS_FIELD_REBUILD_REQUIRED,
	ICE_VF_STATUS_NUM_FIELDS,
};

#define	ICE_VC_MAX_RX_BUFFER			\
	((16 * 1024) - BIT(ICE_RLAN_CTX_DBUF_S))
#define	ICE_VIRTCHNL_QUEUE_MAP_SIZE		16

#ifdef DRIVER_FAILPOINTS
static SYSCTL_NODE(_debug_fail_point_ice, OID_AUTO, iov,
    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "ice SR-IOV fail points");

static int ice_iov_fail_vf = -1;
SYSCTL_INT(_debug_fail_point_ice_iov, OID_AUTO, vf,
    CTLFLAG_RW | CTLFLAG_MPSAFE, &ice_iov_fail_vf, 0,
    "VF eligible for ice SR-IOV fail points (-1 selects every VF)");
#endif /* DRIVER_FAILPOINTS */
static struct ice_vf *ice_iov_get_vf(struct ice_softc *sc, int vf_num);
static int ice_iov_configure_mac_anti_spoof(struct ice_softc *sc,
    struct ice_vf *vf);
static int ice_iov_restore_vf_host_config(struct ice_softc *sc,
    struct ice_vf *vf);
static void ice_iov_clear_vf_queue_state(struct ice_vf *vf);
static void ice_iov_clear_vf_mbx(struct ice_softc *sc, struct ice_vf *vf);
static void ice_iov_complete_vf_reset(struct ice_softc *sc,
    struct ice_vf *vf, bool restore_mapping);
static void ice_iov_ready_vf(struct ice_softc *sc, struct ice_vf *vf);
static int ice_reset_vf(struct ice_softc *sc, struct ice_vf *vf,
			bool trigger_reset, bool release_vf);
static void ice_iov_setup_intr_mapping(struct ice_softc *sc, struct ice_vf *vf);

static void ice_vc_version_msg(struct ice_softc *sc, struct ice_vf *vf,
			       u8 *msg_buf);
static void ice_vc_get_vf_res_msg(struct ice_softc *sc, struct ice_vf *vf,
				  u8 *msg_buf);
static void ice_vc_add_eth_addr_msg(struct ice_softc *sc, struct ice_vf *vf,
				    u8 *msg_buf);
static void ice_vc_del_eth_addr_msg(struct ice_softc *sc, struct ice_vf *vf,
				    u8 *msg_buf);
static bool ice_vc_isvalid_ring_len(u32 ring_len);
static void ice_vc_cfg_vsi_qs_msg(struct ice_softc *sc, struct ice_vf *vf,
				  u8 *msg_buf);
static void ice_vc_cfg_rss_key_msg(struct ice_softc *sc, struct ice_vf *vf,
				   u8 *msg_buf);
static void ice_vc_set_rss_hena_msg(struct ice_softc *sc, struct ice_vf *vf,
				    u8 *msg_buf);
static void ice_vc_enable_queues_msg(struct ice_softc *sc, struct ice_vf *vf,
				     u8 *msg_buf);
static void ice_vc_notify_vf_link_state(struct ice_softc *sc, struct ice_vf *vf);
static void ice_vc_disable_queues_msg(struct ice_softc *sc, struct ice_vf *vf,
				      u8 *msg_buf);
static int ice_vc_disable_queues(struct ice_softc *sc, struct ice_vf *vf,
				 u32 tx_queues, u32 rx_queues);
static void ice_vc_cfg_irq_map_msg(struct ice_softc *sc, struct ice_vf *vf,
				   u8 *msg_buf);
static void ice_vc_get_stats_msg(struct ice_softc *sc, struct ice_vf *vf,
				 u8 *msg_buf);
static void ice_eth_stats_to_virtchnl_eth_stats(struct ice_eth_stats *istats,
     struct virtchnl_eth_stats *vstats);
static void ice_vc_cfg_rss_lut_msg(struct ice_softc *sc, struct ice_vf *vf,
				   u8 *msg_buf);
static void ice_vc_cfg_promisc_mode_msg(struct ice_softc *sc, struct ice_vf *vf,
				        u8 *msg_buf);
static void ice_vc_add_vlan_msg(struct ice_softc *sc, struct ice_vf *vf,
				u8 *msg_buf);
static void ice_vc_del_vlan_msg(struct ice_softc *sc, struct ice_vf *vf,
				u8 *msg_buf);
static int ice_vc_select_vlans(struct ice_vf *vf, u16 *vids, u16 count,
			       bool add, u16 *selected_count);
static enum virtchnl_status_code ice_iov_err_to_virt_err(int ice_err);
static int ice_vf_mac_filter_index(struct ice_vf *vf, const uint8_t *addr);
static int ice_vf_validate_mac(struct ice_vf *vf, const uint8_t *addr);

#ifdef DRIVER_FAILPOINTS
static bool
ice_iov_fail_vf_matches(uint16_t vfnum)
{
	return (ice_iov_fail_vf == -1 || ice_iov_fail_vf == vfnum);
}
#endif

#define	ICE_IOV_FAIL_POINT(_sc, _vfnum, _name, _error, _label) do { \
	ICE_FAIL_POINT_CODE_COND(_sc, _debug_fail_point_ice_iov, _name, \
	    ice_iov_fail_vf_matches((_vfnum)), \
	    FAIL_POINT_NONSLEEPABLE, { \
		(_error) = RETURN_VALUE; \
		if ((_error) <= 0) \
			(_error) = EIO; \
		device_printf((_sc)->dev, \
		    "injecting VF %u failure at %s: %d\n", \
		    (unsigned int)(_vfnum), #_name, (_error)); \
		goto _label; \
	}); \
} while (0)

/**
 * ice_iov_attach - Initialize SR-IOV PF host support
 * @sc: device softc structure
 *
 * Initialize SR-IOV PF host support at the end of the driver attach process.
 *
 * @pre Must be called from sleepable context (calls malloc() w/ M_WAITOK)
 *
 * @returns 0 if successful, or
 * - ENOMEM if there is no memory for the PF/VF schemas or iov device
 * - ENXIO if the device isn't PCI-E or doesn't support the same SR-IOV
 *   version as the kernel
 * - ENOENT if the device doesn't have the SR-IOV capability
 */
int
ice_iov_attach(struct ice_softc *sc)
{
	device_t dev = sc->dev;
	nvlist_t *pf_schema, *vf_schema;
	int error;

	pf_schema = pci_iov_schema_alloc_node();
	vf_schema = pci_iov_schema_alloc_node();

	pci_iov_schema_add_unicast_mac(vf_schema, "mac-addr", 0, NULL);
	pci_iov_schema_add_bool(vf_schema, "mac-anti-spoof",
	    IOV_SCHEMA_HASDEFAULT, TRUE);
	pci_iov_schema_add_bool(vf_schema, "allow-set-mac",
	    IOV_SCHEMA_HASDEFAULT, FALSE);
	pci_iov_schema_add_bool(vf_schema, "allow-promisc",
	    IOV_SCHEMA_HASDEFAULT, FALSE);
	pci_iov_schema_add_uint16(vf_schema, "num-queues",
	    IOV_SCHEMA_HASDEFAULT, ICE_DEFAULT_VF_QUEUES);
	pci_iov_schema_add_uint16(vf_schema, "mirror-src-vsi",
	    IOV_SCHEMA_HASDEFAULT, ICE_INVALID_MIRROR_VSI);
	pci_iov_schema_add_uint16(vf_schema, "max-vlan-allowed",
	    IOV_SCHEMA_HASDEFAULT, ICE_DEFAULT_VF_VLAN_LIMIT);
	pci_iov_schema_add_uint16(vf_schema, "max-mac-filters",
	    IOV_SCHEMA_HASDEFAULT, ICE_DEFAULT_VF_FILTER_LIMIT);

	error = pci_iov_attach(dev, pf_schema, vf_schema);
	if (error != 0) {
		device_printf(dev,
		    "pci_iov_attach failed (error=%s)\n",
		    ice_err_str(error));
		ice_clear_bit(ICE_FEATURE_SRIOV, sc->feat_en);
	} else {
		ice_set_bit(ICE_FEATURE_SRIOV, sc->feat_en);
		if (ice_is_e830(&sc->hw))
			ice_iov_reconfigure_mbx(sc);
		else
			ice_mbx_init_snapshot(&sc->hw);
	}

	return (error);
}

/**
 * ice_iov_reconfigure_mbx - Restore hardware mailbox flood protection
 * @sc: device softc structure
 *
 * E830 limits each VF's outstanding messages in hardware.  The threshold
 * register is reset by a core reset and must be restored during rebuild.
 * Older devices use the software snapshot detector instead.
 */
void
ice_iov_reconfigure_mbx(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;

	if (!ice_is_e830(hw))
		return;

	wr32(hw, E830_MBX_PF_IN_FLIGHT_VF_MSGS_THRESH,
	    ICE_MBX_OVERFLOW_WATERMARK);
	ice_flush(hw);
}

/**
 * ice_iov_detach - Teardown SR-IOV PF host support
 * @sc: device softc structure
 *
 * Teardown SR-IOV PF host support at the start of the driver detach process.
 *
 * @returns 0 if successful or IOV support hasn't been setup, or
 * - EBUSY if VFs still exist
 */
int
ice_iov_detach(struct ice_softc *sc)
{
	device_t dev = sc->dev;
	int error;

	error = pci_iov_detach(dev);
	if (error != 0) {
		device_printf(dev,
		    "pci_iov_detach failed (error=%s)\n",
		    ice_err_str(error));
	}

	return (error);
}

/**
 * ice_iov_init - Called by the OS before the first VF is created.
 * @sc: device softc structure
 * @num_vfs: number of VFs to setup resources for
 * @params: configuration parameters for the PF
 *
 * @returns 0 if successful or an error code on failure
 */
int
ice_iov_init(struct ice_softc *sc, uint16_t num_vfs, const nvlist_t *params __unused)
{
	/* Allocate array of VFs, for tracking */
	sc->vfs = (struct ice_vf *)malloc(sizeof(struct ice_vf) * num_vfs, M_ICE, M_NOWAIT |
	    M_ZERO);
	if (sc->vfs == NULL)
		return (ENOMEM);

	/* Initialize each VF with basic information */
	for (int i = 0; i < num_vfs; i++) {
		sc->vfs[i].vf_num = i;
		if (ice_is_e830(&sc->hw))
			ice_mbx_vf_clear_cnt_e830(&sc->hw, i);
		else
			ice_mbx_init_vf_info(&sc->hw, &sc->vfs[i].mbx_info);
	}

	/* Save off number of configured VFs */
	sc->num_vfs = num_vfs;

	return (0);
}

/**
 * ice_iov_get_vf - Get pointer to VF at given index
 * @sc: device softc structure
 * @vf_num: Index of VF to retrieve
 *
 * @remark will throw an assertion if vf_num is not in the
 * range of allocated VFs
 *
 * @returns a pointer to the VF structure at the given index
 */
static struct ice_vf *
ice_iov_get_vf(struct ice_softc *sc, int vf_num)
{
	MPASS(vf_num < sc->num_vfs);

	return &sc->vfs[vf_num];
}

/**
 * ice_iov_configure_mac_anti_spoof - Apply a VF's source-MAC policy
 * @sc: device softc structure
 * @vf: VF whose VSI security policy should be configured
 *
 * PF and device resets discard the hardware VSI context, so callers must
 * replay this policy after creating or rebuilding the VF's VSI.  Also reapply
 * the PF-owned policy defensively before releasing a VF after VFR.
 */
static int
ice_iov_configure_mac_anti_spoof(struct ice_softc *sc, struct ice_vf *vf)
{
	struct ice_vsi_ctx ctx = { 0 };
	struct ice_vsi *vsi = vf->vsi;
	struct ice_hw *hw = &sc->hw;
	bool enable;
#ifdef DRIVER_FAILPOINTS
	int error;
#endif
	int status;

	enable = (atomic_load_acq_32(&vf->vf_flags) &
	    VF_FLAG_MAC_ANTI_SPOOF) != 0;
	ctx.info.sec_flags = vsi->info.sec_flags;
	ctx.info.valid_sections =
	    CPU_TO_LE16(ICE_AQ_VSI_PROP_SECURITY_VALID);
	if (enable)
		ctx.info.sec_flags |= ICE_AQ_VSI_SEC_FLAG_ENA_MAC_ANTI_SPOOF;
	else
		ctx.info.sec_flags &= ~ICE_AQ_VSI_SEC_FLAG_ENA_MAC_ANTI_SPOOF;

	ICE_IOV_FAIL_POINT(sc, vf->vf_num, mac_anti_spoof_update, error,
	    fail);
	status = ice_update_vsi(hw, vsi->idx, &ctx, NULL);
	if (status != 0) {
		device_printf(sc->dev,
		    "Unable to configure VF %u MAC anti-spoof %s, "
		    "err %s aq_err %s\n", vf->vf_num,
		    enable ? "on" : "off", ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	vsi->info.sec_flags = ctx.info.sec_flags;
	return (0);

#ifdef DRIVER_FAILPOINTS
fail:
	return (error);
#endif
}

/**
 * ice_iov_restore_vf_host_config - Restore PF-owned policy after a VF reset
 * @sc: device softc structure
 * @vf: VF whose host configuration should be restored
 *
 * A VF reset discards the guest's filter configuration. Remove the matching
 * software switch state as well so that replayed guest requests reach
 * firmware instead of being mistaken for filters which still exist. Restore
 * the PF-owned source-MAC policy and base filters before releasing the VF.
 */
static int
ice_iov_restore_vf_host_config(struct ice_softc *sc, struct ice_vf *vf)
{
	struct ice_vsi *vsi = vf->vsi;
	int error;

	ice_remove_vsi_fltr(&sc->hw, vsi->idx);
	vf->mac_filter_cnt = 0;
	vf->vlan_cnt = 0;
	bzero(vf->vlans_map, sizeof(vf->vlans_map));

	error = ice_iov_configure_mac_anti_spoof(sc, vf);
	if (error != 0)
		return (error);

	error = ice_add_vsi_mac_filter(vsi, broadcastaddr);
	if (error != 0)
		return (error);
	if (!ETHER_IS_ZERO(vf->mac)) {
		error = ice_add_vsi_mac_filter(vsi, vf->mac);
		if (error != 0)
			return (error);
	}

	return (0);
}

/**
 * ice_iov_add_vf - Called by the OS for each VF to create
 * @sc: device softc structure
 * @vfnum: index of VF to configure
 * @params: configuration parameters for the VF
 *
 * @returns 0 if successful or an error code on failure
 */
int
ice_iov_add_vf(struct ice_softc *sc, uint16_t vfnum, const nvlist_t *params)
{
	struct ice_tx_queue *txq;
	struct ice_rx_queue *rxq;
	device_t dev = sc->dev;
	struct ice_vsi *vsi;
	struct ice_vf *vf;
	int vf_num_queues;
	const void *mac;
	size_t size;
	int error;
	int i;

	vf = ice_iov_get_vf(sc, vfnum);
	vf->vf_flags = 0;

	/* This VF needs at least one VSI */
	vsi = ice_alloc_vsi(sc, ICE_VSI_VF);
	if (vsi == NULL)
		return (ENOMEM);
	vf->vsi = vsi;
	vsi->vf_num = vfnum;
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_vsi_alloc, error,
	    release_vsi);

	vf_num_queues = nvlist_get_number(params, "num-queues");
	/* Validate and clamp value if invalid */
	if (vf_num_queues < 1 || vf_num_queues > ICE_MAX_SCATTERED_QUEUES)
		device_printf(dev, "Invalid num-queues (%d) for VF %d\n",
		    vf_num_queues, vf->vf_num);
	if (vf_num_queues < 1) {
		device_printf(dev, "Setting VF %d num-queues to 1\n", vf->vf_num);
		vf_num_queues = 1;
	} else if (vf_num_queues > ICE_MAX_SCATTERED_QUEUES) {
		device_printf(dev, "Setting VF %d num-queues to %d\n",
		    vf->vf_num, ICE_MAX_SCATTERED_QUEUES);
		vf_num_queues = ICE_MAX_SCATTERED_QUEUES;
	}
	vsi->qmap_type = ICE_RESMGR_ALLOC_SCATTERED;

	/* Reserve VF queue allocation from PF queues */
	ice_alloc_vsi_qmap(vsi, vf_num_queues, vf_num_queues);
	vsi->num_tx_queues = vsi->num_rx_queues = vf_num_queues;
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_queue_maps, error,
	    release_vsi);

	/* Assign Tx queues from PF space */
	error = ice_resmgr_assign_scattered(&sc->tx_qmgr, vsi->tx_qmap,
					     vsi->num_tx_queues);
	if (error) {
		device_printf(sc->dev, "Unable to assign VF Tx queues: %s\n",
			      ice_err_str(error));
		goto release_vsi;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_tx_reservation, error,
	    release_vsi);

	/* Assign Rx queues from PF space */
	error = ice_resmgr_assign_scattered(&sc->rx_qmgr, vsi->rx_qmap,
					     vsi->num_rx_queues);
	if (error) {
		device_printf(sc->dev, "Unable to assign VF Rx queues: %s\n",
			      ice_err_str(error));
		goto release_vsi;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_rx_reservation, error,
	    release_vsi);

	vsi->max_frame_size = ICE_MAX_FRAME_SIZE;

	/* Allocate queue structure memory */
	vsi->tx_queues = (struct ice_tx_queue *)
	    malloc(sizeof(struct ice_tx_queue) * vsi->num_tx_queues, M_ICE,
		   M_NOWAIT | M_ZERO);
	if (!vsi->tx_queues) {
		device_printf(sc->dev, "VF-%d: Unable to allocate Tx queue memory\n",
			      vfnum);
		error = ENOMEM;
		goto release_vsi;
	}
	for (i = 0, txq = vsi->tx_queues; i < vsi->num_tx_queues; i++, txq++) {
		txq->me = i;
		txq->vsi = vsi;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_tx_queue_memory, error,
	    free_txqs);

	/* Allocate queue structure memory */
	vsi->rx_queues = (struct ice_rx_queue *)
	    malloc(sizeof(struct ice_rx_queue) * vsi->num_rx_queues, M_ICE,
		   M_NOWAIT | M_ZERO);
	if (!vsi->rx_queues) {
		device_printf(sc->dev, "VF-%d: Unable to allocate Rx queue memory\n",
			      vfnum);
		error = ENOMEM;
		goto free_txqs;
	}
	for (i = 0, rxq = vsi->rx_queues; i < vsi->num_rx_queues; i++, rxq++) {
		rxq->me = i;
		rxq->vsi = vsi;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_rx_queue_memory, error,
	    free_rxqs);

	/* Allocate space to store the IRQ vector data */
	vf->num_irq_vectors = vf_num_queues + 1;
	vf->tx_irqvs = (struct ice_irq_vector *)
	    malloc(sizeof(struct ice_irq_vector) * (vf->num_irq_vectors),
		   M_ICE, M_NOWAIT);
	if (!vf->tx_irqvs) {
		device_printf(sc->dev,
			      "Unable to allocate TX irqv memory for VF-%d's %d vectors\n",
			      vfnum, vf->num_irq_vectors);
		error = ENOMEM;
		goto free_rxqs;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_tx_irq_memory, error,
	    free_txirqvs);
	vf->rx_irqvs = (struct ice_irq_vector *)
	    malloc(sizeof(struct ice_irq_vector) * (vf->num_irq_vectors),
		   M_ICE, M_NOWAIT);
	if (!vf->rx_irqvs) {
		device_printf(sc->dev,
			      "Unable to allocate RX irqv memory for VF-%d's %d vectors\n",
			      vfnum, vf->num_irq_vectors);
		error = ENOMEM;
		goto free_txirqvs;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_rx_irq_memory, error,
	    free_rxirqvs);

	/* Assign VF interrupts from PF space */
	if (!(vf->vf_imap =
	      (u16 *)malloc(sizeof(u16) * vf->num_irq_vectors,
	      M_ICE, M_NOWAIT))) {
		device_printf(dev, "Unable to allocate VF-%d imap memory\n", vfnum);
		error = ENOMEM;
		goto free_rxirqvs;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_imap_memory, error,
	    free_imap);
	error = ice_resmgr_assign_contiguous(&sc->dev_imgr, vf->vf_imap, vf->num_irq_vectors);
	if (error) {
		device_printf(dev, "Unable to assign VF-%d interrupt mapping: %s\n",
			      vfnum, ice_err_str(error));
		goto free_imap;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_imap_reservation, error,
	    release_imap);

	if (nvlist_exists_binary(params, "mac-addr")) {
		mac = nvlist_get_binary(params, "mac-addr", &size);
		memcpy(vf->mac, mac, ETHER_ADDR_LEN);

		if (nvlist_get_bool(params, "allow-set-mac"))
			vf->vf_flags |= VF_FLAG_SET_MAC_CAP;
	} else
		/*
		 * If the administrator has not specified a MAC address then
		 * we must allow the VF to choose one.
		 */
		vf->vf_flags |= VF_FLAG_SET_MAC_CAP;

	if (nvlist_get_bool(params, "mac-anti-spoof"))
		vf->vf_flags |= VF_FLAG_MAC_ANTI_SPOOF;

	if (nvlist_get_bool(params, "allow-promisc"))
		vf->vf_flags |= VF_FLAG_PROMISC_CAP;

	vsi->mirror_src_vsi = nvlist_get_number(params, "mirror-src-vsi");

	vf->vlan_limit = nvlist_get_number(params, "max-vlan-allowed");
	vf->mac_filter_limit = nvlist_get_number(params, "max-mac-filters");
	if (vf->mac_filter_limit != 0) {
		vf->mac_filters = mallocarray(vf->mac_filter_limit,
		    sizeof(*vf->mac_filters), M_ICE, M_NOWAIT | M_ZERO);
		if (vf->mac_filters == NULL) {
			device_printf(sc->dev,
			    "Unable to allocate VF-%d MAC filter memory\n",
			    vfnum);
			error = ENOMEM;
			goto release_imap;
		}
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_mac_filter_memory, error,
	    free_mac_filters);

	vf->vf_flags |= VF_FLAG_VLAN_CAP;

	/* Create and setup VSI in HW */
	error = ice_initialize_vsi(vsi);
	if (error) {
		device_printf(sc->dev, "Unable to initialize VF %d VSI: %s\n",
			      vfnum, ice_err_str(error));
		goto free_mac_filters;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_vsi_init, error,
	    free_mac_filters);
	error = ice_iov_configure_mac_anti_spoof(sc, vf);
	if (error != 0)
		goto free_mac_filters;

	/* Add the broadcast address */
	error = ice_add_vsi_mac_filter(vsi, broadcastaddr);
	if (error) {
		device_printf(sc->dev, "Unable to add broadcast filter VF %d VSI: %s\n",
			      vfnum, ice_err_str(error));
		goto free_mac_filters;
	}
	ICE_IOV_FAIL_POINT(sc, vfnum, add_after_broadcast_filter, error,
	    free_mac_filters);

	atomic_set_32(&vf->vf_flags, VF_FLAG_ENABLED);
	ice_iov_ready_vf(sc, vf);

	return (0);

free_mac_filters:
	free(vf->mac_filters, M_ICE);
	vf->mac_filters = NULL;
	vf->mac_filter_cnt = 0;
release_imap:
	ice_resmgr_release_map(&sc->dev_imgr, vf->vf_imap,
			       vf->num_irq_vectors);
free_imap:
	free(vf->vf_imap, M_ICE);
	vf->vf_imap = NULL;
free_rxirqvs:
	free(vf->rx_irqvs, M_ICE);
	vf->rx_irqvs = NULL;
free_txirqvs:
	free(vf->tx_irqvs, M_ICE);
	vf->tx_irqvs = NULL;
free_rxqs:
	free(vsi->rx_queues, M_ICE);
	vsi->rx_queues = NULL;
free_txqs:
	free(vsi->tx_queues, M_ICE);
	vsi->tx_queues = NULL;
release_vsi:
	if (vsi->hw_vsi_created)
		ice_release_vsi(vsi);
	else
		ice_release_vsi_resources(vsi);
	vf->vsi = NULL;
	atomic_store_rel_32(&vf->vf_flags, 0);
	return (error);
}

/**
 * ice_iov_vf_status - report configured VF state
 * @sc: device private structure
 * @statusp: returned status snapshot
 *
 * The iflib context lock protects VF state and VSI lifetime while this
 * method constructs the report.
 */
int
ice_iov_vf_status(struct ice_softc *sc, struct if_vf_status **statusp)
{
	struct ice_vf *vf;
	struct ice_vsi *vsi;
	struct if_vf_extension *extension;
	struct if_vf_info *info;
	struct if_vf_status *status;
	u32 vf_flags;
	bool mirror_configured, software_mbx_limit;
	uint32_t field, num_fields;
	int i;

	if (!ice_is_bit_set(sc->feat_en, ICE_FEATURE_SRIOV))
		return (EOPNOTSUPP);
	status = if_vf_status_alloc(sc->num_vfs);
	if (status == NULL)
		return (ENOMEM);
	for (i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		vsi = vf->vsi;
		vf_flags = atomic_load_acq_32(&vf->vf_flags);
		info = &status->vfs[i];
		info->fields = IFVF_F_CONFIGURED | IFVF_F_INITIALIZED |
		    IFVF_F_TRAFFIC_ALLOWED | IFVF_F_FAULT_BLOCKED |
		    IFVF_F_LINK_STATE_POLICY |
		    IFVF_F_VLAN_MODE | IFVF_F_VLAN_COUNT |
		    IFVF_F_ALLOW_SET_MAC | IFVF_F_ALLOW_SET_VLAN |
		    IFVF_F_MAC_ANTI_SPOOF | IFVF_F_ALLOW_PROMISC;
		info->index = i;
		info->configured =
		    (vf_flags & VF_FLAG_ENABLED) != 0 && vsi != NULL;
		info->initialized = info->configured &&
		    (vf_flags & VF_FLAG_INITIALIZED) != 0;
		info->traffic_allowed = info->configured &&
		    (vf_flags & (VF_FLAG_MDD_BLOCKED |
		    VF_FLAG_MBX_BLOCKED)) == 0;
		info->fault_blocked = (vf_flags & (VF_FLAG_MDD_BLOCKED |
		    VF_FLAG_MBX_BLOCKED)) != 0;
		info->link_state_policy = IFVF_LINK_AUTO;
		if (info->initialized) {
			snprintf(info->api_version, sizeof(info->api_version),
			    "%u.%u", vf->version.major, vf->version.minor);
			info->fields |= IFVF_F_API_VERSION;
		}
		if (!ETHER_IS_ZERO(vf->mac)) {
			memcpy(info->mac, vf->mac, sizeof(info->mac));
			info->fields |= IFVF_F_MAC;
		}
		/* The ICE IOV schema exposes only VF-managed trunk membership. */
		info->vlan_mode = IFVF_VLAN_TRUNK;
		info->vlan_count = vf->vlan_cnt;
		if (info->configured) {
			info->vlan_limit = vf->vlan_limit;
			info->fields |= IFVF_F_VLAN_LIMIT;
		}
		if (vsi != NULL) {
			info->tx_queue_count = vsi->num_tx_queues;
			info->rx_queue_count = vsi->num_rx_queues;
			info->fields |= IFVF_F_NUM_TX_QUEUES |
			    IFVF_F_NUM_RX_QUEUES;
		}

		mirror_configured = vsi != NULL && vsi->mirror_src_vsi !=
		    ICE_INVALID_MIRROR_VSI;
		software_mbx_limit = !ice_is_e830(&sc->hw);
		num_fields = ICE_VF_STATUS_NUM_FIELDS -
		    (mirror_configured ? 0 : 1) -
		    (software_mbx_limit ? 0 : 2) -
		    (info->configured ? 0 : 1);
		extension = if_vf_status_add_extension(info,
		    ICE_VF_STATUS_NAMESPACE, ICE_VF_STATUS_VERSION,
		    num_fields);
		if (extension == NULL) {
			if_vf_status_free(status);
			return (ENOMEM);
		}
		field = ICE_VF_STATUS_FIELD_MIRROR_CONFIGURED;
		if_vf_extension_set_bool(extension, field++,
		    ICE_VF_STATUS_MIRROR_CONFIGURED, mirror_configured);
		if (mirror_configured)
			if_vf_extension_set_number(extension, field++,
			    ICE_VF_STATUS_MIRROR_SOURCE_VSI,
			    vsi->mirror_src_vsi);
		if_vf_extension_set_bool(extension, field++,
		    ICE_VF_STATUS_MIRROR_INGRESS_ACTIVE,
		    vsi != NULL &&
		    vsi->rule_mir_ingress != ICE_INVAL_MIRROR_RULE_ID);
		if_vf_extension_set_bool(extension, field++,
		    ICE_VF_STATUS_MIRROR_EGRESS_ACTIVE,
		    vsi != NULL &&
		    vsi->rule_mir_egress != ICE_INVAL_MIRROR_RULE_ID);
		if_vf_extension_set_bool(extension, field++,
		    ICE_VF_STATUS_MDD_BLOCKED,
		    (vf_flags & VF_FLAG_MDD_BLOCKED) != 0);
		if_vf_extension_set_number(extension, field++,
		    ICE_VF_STATUS_MDD_TX_EVENTS, vf->mdd_tx_events);
		if_vf_extension_set_number(extension, field++,
		    ICE_VF_STATUS_MDD_RX_EVENTS, vf->mdd_rx_events);
		if (software_mbx_limit) {
			if_vf_extension_set_bool(extension, field++,
			    ICE_VF_STATUS_MBX_BLOCKED,
			    (vf_flags & VF_FLAG_MBX_BLOCKED) != 0);
			if_vf_extension_set_number(extension, field++,
			    ICE_VF_STATUS_MBX_OVERFLOW_EVENTS,
			    vf->mbx_overflow_events);
		}
		if_vf_extension_set_number(extension, field++,
		    ICE_VF_STATUS_MAC_FILTER_COUNT, vf->mac_filter_cnt);
		if (info->configured)
			if_vf_extension_set_number(extension, field++,
			    ICE_VF_STATUS_MAC_FILTER_LIMIT, vf->mac_filter_limit);
		if_vf_extension_set_bool(extension, field++,
		    ICE_VF_STATUS_RESET_FAILED,
		    (vf_flags & VF_FLAG_RESET_FAILED) != 0);
		if_vf_extension_set_bool(extension, field++,
		    ICE_VF_STATUS_REBUILD_REQUIRED,
		    (vf_flags & VF_FLAG_REBUILD_REQUIRED) != 0);
		KASSERT(field == num_fields,
		    ("ICE VF status field count %u != %u", field, num_fields));
		info->allow_set_mac = (vf_flags & VF_FLAG_SET_MAC_CAP) != 0;
		info->allow_set_vlan = (vf_flags & VF_FLAG_VLAN_CAP) != 0;
		info->mac_anti_spoof =
		    (vf_flags & VF_FLAG_MAC_ANTI_SPOOF) != 0;
		info->allow_promisc = (vf_flags & VF_FLAG_PROMISC_CAP) != 0;
	}
	*statusp = status;
	return (0);
}

/**
 * ice_iov_uninit - Called by the OS when VFs are destroyed
 * @sc: device softc structure
 */
void
ice_iov_uninit(struct ice_softc *sc)
{
	struct ice_vf *vf;
	struct ice_vsi *vsi;

	/* Release per-VF resources */
	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if (!ice_is_e830(&sc->hw))
			LIST_DEL(&vf->mbx_info.list_entry);
		atomic_store_rel_32(&vf->vf_flags, 0);
		vsi = vf->vsi;
		free(vf->mac_filters, M_ICE);
		vf->mac_filters = NULL;
		vf->mac_filter_cnt = 0;

		/* Free VF interrupt reservation */
		if (vf->vf_imap) {
			ice_resmgr_release_map(&sc->dev_imgr, vf->vf_imap,
			    vf->num_irq_vectors);
			free(vf->vf_imap, M_ICE);
			vf->vf_imap = NULL;
		}

		/* Free queue interrupt mapping trackers */
		if (vf->tx_irqvs) {
			free(vf->tx_irqvs, M_ICE);
			vf->tx_irqvs = NULL;
		}
		if (vf->rx_irqvs) {
			free(vf->rx_irqvs, M_ICE);
			vf->rx_irqvs = NULL;
		}

		if (!vsi)
			continue;

		/* Free VSI queues */
		if (vsi->tx_queues) {
			free(vsi->tx_queues, M_ICE);
			vsi->tx_queues = NULL;
		}
		if (vsi->rx_queues) {
			free(vsi->rx_queues, M_ICE);
			vsi->rx_queues = NULL;
		}

		if (vsi->hw_vsi_created)
			ice_release_vsi(vsi);
		else
			ice_release_vsi_resources(vsi);
		vf->vsi = NULL;
	}

	/* Release memory used for VF tracking */
	if (sc->vfs) {
		free(sc->vfs, M_ICE);
		sc->vfs = NULL;
	}
	sc->num_vfs = 0;
}

/**
 * ice_iov_handle_vflr - Process VFLR event
 * @sc: device softc structure
 *
 * Identifys which VFs have been reset and re-configure
 * them.
 */
void
ice_iov_handle_vflr(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	struct ice_vf *vf;
	u32 reg, reg_idx, bit_idx, vf_flags;

	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];

		reg_idx = (hw->func_caps.vf_base_id + vf->vf_num) / 32;
		bit_idx = (hw->func_caps.vf_base_id + vf->vf_num) % 32;
		reg = rd32(hw, GLGEN_VFLRSTAT(reg_idx));
		if ((reg & BIT(bit_idx)) == 0)
			continue;
		vf_flags = atomic_load_acq_32(&vf->vf_flags);
		if ((vf_flags & VF_FLAG_ENABLED) != 0 && vf->vsi != NULL) {
			if ((vf_flags & VF_FLAG_REBUILD_REQUIRED) != 0) {
				/* Consume the event but leave the invalid VF held. */
				wr32(hw, GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
				ice_flush(hw);
				continue;
			}
			ice_reset_vf(sc, vf, false, true);
			continue;
		}

		/* Consume reset events for inactive or incompletely added VFs. */
		wr32(hw, GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
		ice_flush(hw);
	}
}

/**
 * ice_iov_handle_mdd - Attribute malicious-driver events to VFs
 * @sc: device softc structure
 *
 * Consume every per-VF MDD latch. Block further virtchnl requests and reset a
 * newly blocked VF without restoring its queues, so even event classes which
 * only drop the offending packet cannot continue traffic. An optional policy
 * reconstructs and releases the VF immediately instead.
 *
 * @returns a mask of enum ice_mdd_source_bits attributed to configured or
 * unconfigured VFs of this PF.
 */
u32
ice_iov_handle_mdd(struct ice_softc *sc)
{
	static const struct timeval log_interval = { 2, 0 };
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_pf_event event = {};
	struct ice_vf *vf;
	u32 reg, sources, vf_sources, tx_events, rx_events, vf_flags;
	bool newly_blocked;
	int error;

	event.event = VIRTCHNL_EVENT_RESET_IMPENDING;
	event.severity = PF_EVENT_SEVERITY_CERTAIN_DOOM;
	vf_sources = 0;
	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		sources = 0;
		tx_events = 0;
		rx_events = 0;

		reg = rd32(hw, VP_MDET_TX_PQM(vf->vf_num));
		if ((reg & VP_MDET_TX_PQM_VALID_M) != 0) {
			wr32(hw, VP_MDET_TX_PQM(vf->vf_num), 0xffff);
			sources |= ICE_MDD_TX_PQM;
			tx_events++;
		}
		reg = rd32(hw, VP_MDET_TX_TCLAN(vf->vf_num));
		if ((reg & VP_MDET_TX_TCLAN_VALID_M) != 0) {
			wr32(hw, VP_MDET_TX_TCLAN(vf->vf_num), 0xffff);
			sources |= ICE_MDD_TX_TCLAN;
			tx_events++;
		}
		reg = rd32(hw, VP_MDET_TX_TDPU(vf->vf_num));
		if ((reg & VP_MDET_TX_TDPU_VALID_M) != 0) {
			wr32(hw, VP_MDET_TX_TDPU(vf->vf_num), 0xffff);
			sources |= ICE_MDD_TX_TDPU;
			tx_events++;
		}
		reg = rd32(hw, VP_MDET_RX(vf->vf_num));
		if ((reg & VP_MDET_RX_VALID_M) != 0) {
			wr32(hw, VP_MDET_RX(vf->vf_num), 0xffff);
			sources |= ICE_MDD_RX;
			rx_events++;
		}
		if (tx_events == 0 && rx_events == 0)
			continue;
		vf_sources |= sources;

		vf_flags = atomic_load_acq_32(&vf->vf_flags);
		if ((vf_flags & VF_FLAG_ENABLED) == 0 || vf->vsi == NULL)
			continue;
		vf->mdd_tx_events += tx_events;
		vf->mdd_rx_events += rx_events;
		newly_blocked = (vf_flags & VF_FLAG_MDD_BLOCKED) == 0;
		atomic_set_32(&vf->vf_flags, VF_FLAG_MDD_BLOCKED);

		if (ratecheck(&vf->last_mdd_log, &log_interval)) {
			device_printf(sc->dev,
			    "malicious-driver event from VF-%d "
			    "(tx %ju, rx %ju); %s\n", vf->vf_num,
			    (uintmax_t)vf->mdd_tx_events,
			    (uintmax_t)vf->mdd_rx_events,
			    sc->mdd_auto_reset_vf && newly_blocked ?
			    "resetting VF" : "VF remains blocked");
		}
		if (!newly_blocked)
			continue;

		/* Ignore notification failure; reset does not require VF help. */
		if (sc->mdd_auto_reset_vf &&
		    (vf_flags & VF_FLAG_INITIALIZED) != 0 &&
		    ice_check_sq_alive(hw, &hw->mailboxq)) {
			(void)ice_aq_send_msg_to_vf(hw, vf->vf_num,
			    VIRTCHNL_OP_EVENT, VIRTCHNL_STATUS_SUCCESS,
			    (u8 *)&event, sizeof(event), NULL);
		}
		/*
		 * TDPU MDD drops only the offending packet. Reset the entire VF so
		 * the software blocked state always means that traffic is actually
		 * fenced. The opt-in policy reconstructs its queues immediately.
		 */
		error = ice_reset_vf(sc, vf, true, sc->mdd_auto_reset_vf);
		if (error != 0) {
			device_printf(sc->dev,
			    "failed to quiesce MDD-blocked VF-%d: %d\n",
			    vf->vf_num, error);
		} else if (!sc->mdd_auto_reset_vf) {
			/*
			 * Complete VFR without restoring queues. This leaves the VF
			 * inactive and DMA-fenced, but permits a later physical FLR to
			 * create a new reset edge and recover it.
			 */
			ice_iov_complete_vf_reset(sc, vf, false);
		}
	}
	ice_flush(hw);
	return (vf_sources);
}

/**
 * ice_iov_notify_vfs_reset - Notify initialized VFs of an impending reset
 * @sc: device softc structure
 *
 * Give VF drivers advance notice while the mailbox control queue is still
 * alive. Ignore individual send failures so one VF cannot prevent the PF from
 * notifying its siblings or proceeding with the reset.
 */
void
ice_iov_notify_vfs_reset(struct ice_softc *sc)
{
	struct virtchnl_pf_event event = {};
	struct ice_hw *hw = &sc->hw;
	struct ice_vf *vf;

	if (!ice_check_sq_alive(hw, &hw->mailboxq))
		return;

	event.event = VIRTCHNL_EVENT_RESET_IMPENDING;
	event.severity = PF_EVENT_SEVERITY_CERTAIN_DOOM;
	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		if ((atomic_load_acq_32(&vf->vf_flags) &
		    VF_FLAG_INITIALIZED) == 0)
			continue;
		(void)ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_EVENT,
		    VIRTCHNL_STATUS_SUCCESS, (u8 *)&event, sizeof(event), NULL);
	}
}

/**
 * ice_iov_clear_vf_queue_state - Clear tracked VF queue state
 * @vf: driver's VF structure for the VF to update
 */
static void
ice_iov_clear_vf_queue_state(struct ice_vf *vf)
{
	vf->txq_configured = 0;
	vf->rxq_configured = 0;
	vf->rxq_enabled = 0;
}

/**
 * ice_iov_clear_vf_mdd - Clear hardware MDD latches for a reset VF
 * @sc: device softc structure
 * @vf: driver's VF structure for the VF to update
 *
 * Function reset can generate a spurious anti-spoof MDD indication. Consume
 * all per-VF latches before releasing reset so it cannot re-block a VF which
 * has just been reconstructed successfully.
 */
static void
ice_iov_clear_vf_mdd(struct ice_softc *sc, struct ice_vf *vf)
{
	struct ice_hw *hw = &sc->hw;

	wr32(hw, VP_MDET_TX_PQM(vf->vf_num), 0xffff);
	wr32(hw, VP_MDET_TX_TCLAN(vf->vf_num), 0xffff);
	wr32(hw, VP_MDET_TX_TDPU(vf->vf_num), 0xffff);
	wr32(hw, VP_MDET_RX(vf->vf_num), 0xffff);
	ice_flush(hw);
}

/**
 * ice_iov_complete_vf_reset - Complete a VF reset
 * @sc: device softc structure
 * @vf: driver's VF structure for the VF to update
 * @restore_mapping: restore the VF queue and interrupt mappings
 *
 * Clear VFSWR after the hardware drain, optionally restore the VF mappings,
 * and then publish VFACTIVE. The mapping registers do not retain writes made
 * while VFSWR remains asserted. Callers may instead leave a software-blocked
 * VF with no queue or interrupt mappings.
 */
static void
ice_iov_complete_vf_reset(struct ice_softc *sc, struct ice_vf *vf,
    bool restore_mapping)
{
	struct ice_hw *hw = &sc->hw;
	u32 reg;

	reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_num));
	reg &= ~VPGEN_VFRTRIG_VFSWR_M;
	wr32(hw, VPGEN_VFRTRIG(vf->vf_num), reg);
	if (restore_mapping)
		ice_iov_setup_intr_mapping(sc, vf);
	wr32(hw, VFGEN_RSTAT(vf->vf_num), VIRTCHNL_VFR_VFACTIVE);
	ice_flush(hw);
}

/**
 * ice_iov_ready_vf - Setup VF interrupts and mark it as ready
 * @sc: device softc structure
 * @vf: driver's VF structure for the VF to update
 *
 * Clears VF reset triggering bit, sets up the PF<->VF interrupt
 * mapping and marks the VF as active in the HW so that the VF
 * driver can use it.
 */
static void
ice_iov_ready_vf(struct ice_softc *sc, struct ice_vf *vf)
{
	/* A VF or PF reset discards all queue configuration and state. */
	ice_iov_clear_vf_queue_state(vf);
	ice_iov_clear_vf_mdd(sc, vf);
	atomic_clear_32(&vf->vf_flags, VF_FLAG_MDD_BLOCKED);
	ice_iov_clear_vf_mbx(sc, vf);

	ice_iov_complete_vf_reset(sc, vf, true);
}

/**
 * ice_iov_clear_vf_mbx - Release mailbox isolation after a completed reset
 * @sc: device softc structure
 * @vf: VF whose mailbox state should be cleared
 */
static void
ice_iov_clear_vf_mbx(struct ice_softc *sc, struct ice_vf *vf)
{
	if (ice_is_e830(&sc->hw))
		ice_mbx_vf_clear_cnt_e830(&sc->hw, vf->vf_num);
	else
		ice_mbx_clear_malvf(&vf->mbx_info);
	atomic_clear_32(&vf->vf_flags, VF_FLAG_MBX_BLOCKED);
}

/**
 * ice_iov_rebuild_vf - Rebuild a VF VSI after a PF or device reset
 * @sc: device softc structure
 * @vsi: VF VSI to rebuild
 *
 * PF and device resets discard the hardware VSI and interrupt state for every
 * VF. Re-add the VSI and replay its configuration before reporting the VF as
 * active. A failed rebuild leaves the VF inactive while allowing the PF and
 * other VFs to recover.
 */
int
ice_iov_rebuild_vf(struct ice_softc *sc, struct ice_vsi *vsi)
{
	struct ice_eth_stats accumulated_stats;
	struct ice_hw *hw = &sc->hw;
	struct ice_vf *vf;
	int error, status;

	MPASS(vsi->type == ICE_VSI_VF);
	vf = ice_iov_get_vf(sc, vsi->vf_num);
	atomic_clear_32(&vf->vf_flags, VF_FLAG_INITIALIZED);
	atomic_set_32(&vf->vf_flags, VF_FLAG_REBUILD_REQUIRED);
	ice_iov_clear_vf_queue_state(vf);
	ICE_IOV_FAIL_POINT(sc, vf->vf_num, rebuild_before_initialize, error,
	    fail);

	/* A new hardware VSI starts a new raw statistics epoch. */
	accumulated_stats = vsi->hw_stats.cur;
	error = ice_initialize_vsi(vsi);
	if (error != 0) {
		device_printf(sc->dev,
		    "Unable to re-initialize VF %d VSI, err %s\n",
		    vf->vf_num, ice_err_str(error));
		return (error);
	}
	vsi->hw_stats.cur = accumulated_stats;
	error = ice_iov_configure_mac_anti_spoof(sc, vf);
	if (error != 0)
		return (error);

	status = ice_replay_vsi(hw, vsi->idx);
	if (status != 0) {
		device_printf(sc->dev,
		    "Failed to replay VF %d VSI, err %s aq_err %s\n",
		    vf->vf_num, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	atomic_clear_32(&vf->vf_flags,
	    VF_FLAG_REBUILD_REQUIRED | VF_FLAG_RESET_FAILED);
	ice_iov_ready_vf(sc, vf);
	return (0);

#ifdef DRIVER_FAILPOINTS
fail:
	return (error);
#endif /* DRIVER_FAILPOINTS */
}

/**
 * ice_reset_vf - Perform a hardware reset (VFR) on a VF
 * @sc: device softc structure
 * @vf: driver's VF structure for VF to be reset
 * @trigger_reset: trigger a reset or only handle an already executed reset
 * @release_vf: publish VFACTIVE after reset; otherwise leave the VF held
 *
 * Performs a VFR for the given VF. This function busy waits until the reset
 * completes in the HW and publishes VFACTIVE only after every mandatory
 * reset stage succeeds. In quiesce mode, it returns with VFSWR asserted and
 * without restoring interrupt mappings or publishing VFACTIVE.
 *
 * @remark Release mode also sets up the PF<->VF interrupt mapping and
 * allocations in the hardware after the hardware reset is finished, via
 * ice_iov_setup_intr_mapping()
 */
static int
ice_reset_vf(struct ice_softc *sc, struct ice_vf *vf, bool trigger_reset,
    bool release_vf)
{
	u16 global_vf_num, reg_idx, bit_idx;
	struct ice_hw *hw = &sc->hw;
	bool reset_done;
	int error, status;
	u32 reg;
	int bit, i;

	/* A VFR cannot recover PF-owned VSI state lost during PF rebuild. */
	if (release_vf && (atomic_load_acq_32(&vf->vf_flags) &
	    VF_FLAG_REBUILD_REQUIRED) != 0)
		return (EIO);

	global_vf_num = vf->vf_num + hw->func_caps.vf_base_id;
	atomic_clear_32(&vf->vf_flags, VF_FLAG_INITIALIZED);
	error = 0;

	if (trigger_reset) {
		reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_num));
		reg |= VPGEN_VFRTRIG_VFSWR_M;
		wr32(hw, VPGEN_VFRTRIG(vf->vf_num), reg);
		ice_flush(hw);
	}

	/*
	 * Remove the tracked queue leaves from the software scheduler before
	 * issuing the reset-only AQ command.  That command drains hardware but
	 * does not update the shared scheduler database.  Retain unresolved queue
	 * state if cleanup fails so a later reset can retry it.
	 */
	status = ice_vc_disable_queues(sc, vf, vf->txq_configured,
	    vf->rxq_enabled);
	if (status == 0)
		ice_iov_clear_vf_queue_state(vf);
	else if (error == 0)
		error = status;

	/* This zero-queue command is required to complete every VF reset. */
	status = ice_dis_vsi_txq(hw->port_info, vf->vsi->idx, 0, 0,
	    NULL, NULL, NULL, ICE_VF_RESET, vf->vf_num, NULL);
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    vf_reset_tx_disable, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		status = ICE_ERR_AQ_ERROR;
	});
	if (status) {
		device_printf(sc->dev,
		    "%s: Failed to disable LAN Tx queues: err %s aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		if (error == 0)
			error = EIO;
	}

	/* Then check for the VF reset to finish in HW. */
	reset_done = false;
	for (i = 0; i < ICE_VPGEN_VFRSTAT_WAIT_COUNT; i++) {
		reg = rd32(hw, VPGEN_VFRSTAT(vf->vf_num));
		if ((reg & VPGEN_VFRSTAT_VFRD_M)) {
			reset_done = true;
			break;
		}

		DELAY(ICE_VPGEN_VFRSTAT_WAIT_DELAY_US);
	}
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    vf_reset_vfr_timeout, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		reset_done = false;
	});
	if (!reset_done) {
		device_printf(sc->dev,
			"VF-%d Reset is stuck\n", vf->vf_num);
		if (error == 0)
			error = ETIMEDOUT;
	} else {
		/* VFLR status is W1C only after the hardware drain completes. */
		reg_idx = global_vf_num / 32;
		bit_idx = global_vf_num % 32;
		wr32(hw, GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
		ice_flush(hw);

		/* Hardware resets Tx queues; the PF must disable every Rx. */
		for (bit = 0; bit < vf->vsi->num_rx_queues; bit++) {
			status = ice_control_rx_queue(vf->vsi, bit, false);
			ICE_FAIL_POINT_CODE_COND(sc,
			    _debug_fail_point_ice_iov, vf_reset_rx_disable,
			    ice_iov_fail_vf_matches(vf->vf_num),
			    FAIL_POINT_NONSLEEPABLE, {
				status = EIO;
			});
			if (status != 0) {
				device_printf(sc->dev,
				    "Unable to disable VF-%d Rx queue %d: %s\n",
				    vf->vf_num, bit, ice_err_str(status));
				if (error == 0)
					error = status;
			}
		}
	}

	/* Verify that post-drain cleanup left no outstanding DMA. */
	wr32(hw, PF_PCI_CIAA,
	    ICE_PCIE_DEV_STATUS | (global_vf_num << PF_PCI_CIAA_VF_NUM_S));
	for (i = 0; i < ICE_PCI_CIAD_WAIT_COUNT; i++) {
		reg = rd32(hw, PF_PCI_CIAD);
		if (!(reg & PCIEM_STA_TRANSACTION_PND))
			break;
		DELAY(ICE_PCI_CIAD_WAIT_DELAY_US);
	}
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    vf_reset_pcie_pending, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		i = ICE_PCI_CIAD_WAIT_COUNT;
	});
	if (i == ICE_PCI_CIAD_WAIT_COUNT) {
		device_printf(sc->dev,
		    "VF-%d PCI transactions remain after reset\n", vf->vf_num);
		if (error == 0)
			error = ETIMEDOUT;
	}

	if (error != 0) {
		atomic_set_32(&vf->vf_flags, VF_FLAG_RESET_FAILED);
		return (error);
	}

	if (!release_vf) {
		/* Discard any anti-spoof MDD indication caused by the reset. */
		ice_iov_clear_vf_mdd(sc, vf);
		atomic_clear_32(&vf->vf_flags, VF_FLAG_RESET_FAILED);
		return (0);
	}

	error = ice_iov_restore_vf_host_config(sc, vf);
	if (error != 0) {
		atomic_set_32(&vf->vf_flags, VF_FLAG_RESET_FAILED);
		return (error);
	}

	atomic_clear_32(&vf->vf_flags, VF_FLAG_RESET_FAILED);
	ice_iov_ready_vf(sc, vf);
	return (0);
}

/**
 * ice_iov_quiesce_vfs_for_reset - Hold configured VFs before device reset
 * @sc: device softc structure
 *
 * Gate VF master accesses, drain each VF data path, and leave VFSWR asserted.
 * Process VFs serially to remain below the E810 limit of four concurrent
 * VM/VF reset flows. A successful VSI rebuild releases each VF individually.
 */
int
ice_iov_quiesce_vfs_for_reset(struct ice_softc *sc)
{
	struct virtchnl_pf_event event = {};
	struct ice_hw *hw = &sc->hw;
	struct ice_vf *vf;
	int error, first_error;
	u32 reg, vf_flags;
	bool notify;

	notify = ice_check_sq_alive(hw, &hw->mailboxq);
	event.event = VIRTCHNL_EVENT_RESET_IMPENDING;
	event.severity = PF_EVENT_SEVERITY_CERTAIN_DOOM;

	/* Notify and then gate each VF before it can release its buffers. */
	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		vf_flags = atomic_load_acq_32(&vf->vf_flags);
		if ((vf_flags & VF_FLAG_ENABLED) == 0 || vf->vsi == NULL)
			continue;
		if (notify && (vf_flags & VF_FLAG_INITIALIZED) != 0)
			ice_aq_send_msg_to_vf(hw, vf->vf_num,
			    VIRTCHNL_OP_EVENT, VIRTCHNL_STATUS_SUCCESS,
			    (u8 *)&event, sizeof(event), NULL);

		/* Block mailbox reconfiguration before asserting reset. */
		atomic_clear_32(&vf->vf_flags, VF_FLAG_INITIALIZED);
		atomic_set_32(&vf->vf_flags, VF_FLAG_REBUILD_REQUIRED);
		reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_num));
		reg |= VPGEN_VFRTRIG_VFSWR_M;
		wr32(hw, VPGEN_VFRTRIG(vf->vf_num), reg);
	}
	ice_flush(hw);

	/* Firmware data-path drains remain serialized below its limit. */
	first_error = 0;
	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];
		vf_flags = atomic_load_acq_32(&vf->vf_flags);
		if ((vf_flags & VF_FLAG_ENABLED) == 0 || vf->vsi == NULL)
			continue;
		error = ice_reset_vf(sc, vf, false, false);
		if (error != 0) {
			device_printf(sc->dev,
			    "Failed to quiesce VF-%d for device reset: %d\n",
			    vf->vf_num, error);
			if (first_error == 0)
				first_error = error;
		}
	}

	return (first_error);
}

/**
 * ice_vc_get_vf_res_msg - Handle VIRTCHNL_OP_GET_VF_RESOURCES msg from VF
 * @sc: device private structure
 * @vf: VF tracking structure
 * @msg_buf: raw message buffer from the VF
 *
 * Receives a message from the VF listing its supported capabilities, and
 * replies to the VF with information about what resources the PF has
 * allocated for the VF.
 *
 * @remark This always replies to the VF with a success status; it does not
 * fail. It's up to the VF driver to reject or complain about the PF's response.
 */
static void
ice_vc_get_vf_res_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_vf_resource *vf_res;
	struct virtchnl_vsi_resource *vsi_res;
	u16 vf_res_len;
	u32 vf_caps;
	int status;

	/* XXX: Only support one VSI per VF, so this size doesn't need adjusting */
	vf_res_len = sizeof(struct virtchnl_vf_resource);
	vf_res = (struct virtchnl_vf_resource *)malloc(vf_res_len, M_ICE,
	    M_WAITOK | M_ZERO);

	vf_res->num_vsis = 1;
	vf_res->num_queue_pairs = vf->vsi->num_tx_queues;
	vf_res->max_vectors = vf_res->num_queue_pairs + 1;

	vf_res->rss_key_size = ICE_GET_SET_RSS_KEY_EXTEND_KEY_SIZE;
	vf_res->rss_lut_size = ICE_VSIQF_HLUT_ARRAY_SIZE;
	vf_res->max_mtu = ICE_MAX_FRAME_SIZE;

	vf_res->vf_cap_flags = VF_BASE_MODE_OFFLOADS;
	if (msg_buf != NULL) {
		vf_caps = *((u32 *)(msg_buf));

		if (vf_caps & VIRTCHNL_VF_CAP_ADV_LINK_SPEED)
			vf_res->vf_cap_flags |= VIRTCHNL_VF_CAP_ADV_LINK_SPEED;

		if (vf_caps & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR)
                        vf_res->vf_cap_flags |= VIRTCHNL_VF_OFFLOAD_WB_ON_ITR;
	}

	vsi_res = &vf_res->vsi_res[0];
	vsi_res->vsi_id = vf->vsi->idx;
	vsi_res->num_queue_pairs = vf->vsi->num_tx_queues;
	vsi_res->vsi_type = VIRTCHNL_VSI_SRIOV;
	vsi_res->qset_handle = 0;
	if (!ETHER_IS_ZERO(vf->mac))
		memcpy(vsi_res->default_mac_addr, vf->mac, ETHER_ADDR_LEN);

	status = ice_aq_send_msg_to_vf(hw, vf->vf_num,
	    VIRTCHNL_OP_GET_VF_RESOURCES, VIRTCHNL_STATUS_SUCCESS,
	    (u8 *)vf_res, vf_res_len, NULL);
	if (status == 0)
		atomic_set_32(&vf->vf_flags, VF_FLAG_INITIALIZED);
	else
		device_printf(sc->dev,
		    "Unable to send VF-%u resource response, err %s\n",
		    vf->vf_num, ice_status_str(status));

	free(vf_res, M_ICE);
}

/**
 * ice_vc_version_msg - Handle VIRTCHNL_OP_VERSION msg from VF
 * @sc: device private structure
 * @vf: VF tracking structure
 * @msg_buf: raw message buffer from the VF
 *
 * Receives a version message from the VF, and responds to the VF with
 * the version number that the PF will use.
 *
 * @remark This always replies to the VF with a success status; it does not
 * fail.
 */
static void
ice_vc_version_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct virtchnl_version_info *recv_vf_version;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;

	recv_vf_version = (struct virtchnl_version_info *)msg_buf;

	/* VFs running the 1.0 API expect to get 1.0 back */
	if (VF_IS_V10(recv_vf_version)) {
		vf->version.major = 1;
		vf->version.minor = VIRTCHNL_VERSION_MINOR_NO_VF_CAPS;
	} else {
		vf->version.major = VIRTCHNL_VERSION_MAJOR;
		vf->version.minor = VIRTCHNL_VERSION_MINOR;

		if ((recv_vf_version->major != VIRTCHNL_VERSION_MAJOR) ||
		    (recv_vf_version->minor != VIRTCHNL_VERSION_MINOR))
		    device_printf(dev,
		        "%s: VF-%d requested version (%d.%d) differs from PF version (%d.%d)\n",
			__func__, vf->vf_num,
			recv_vf_version->major, recv_vf_version->minor,
			VIRTCHNL_VERSION_MAJOR, VIRTCHNL_VERSION_MINOR);
	}

	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_VERSION,
	    VIRTCHNL_STATUS_SUCCESS, (u8 *)&vf->version, sizeof(vf->version),
	    NULL);
}

/**
 * ice_vf_validate_mac - Validate MAC address before adding it
 * @vf: VF tracking structure
 * @addr: MAC address to validate
 *
 * Validate a MAC address before adding it to a VF during the handling
 * of a VIRTCHNL_OP_ADD_ETH_ADDR operation. Notably, this also checks if
 * the VF is allowed to set its own arbitrary MAC addresses.
 *
 * Returns 0 if MAC address is valid for the given vf
 */
static int
ice_vf_validate_mac(struct ice_vf *vf, const uint8_t *addr)
{

	if (ETHER_IS_ZERO(addr) || ETHER_IS_BROADCAST(addr))
		return (EINVAL);

	/*
	 * If the VF is not allowed to change its MAC address, don't let it
	 * set a MAC filter for an address that is not a multicast address and
	 * is not its assigned MAC.
	 */
	if (!(vf->vf_flags & VF_FLAG_SET_MAC_CAP) &&
	    !(ETHER_IS_MULTICAST(addr) || !bcmp(addr, vf->mac, ETHER_ADDR_LEN)))
		return (EPERM);

	return (0);
}

/**
 * ice_vf_mac_filter_index - Find a VF-owned MAC filter
 * @vf: VF tracking structure
 * @addr: MAC address to find
 *
 * The administrator-assigned address does not consume the configurable VF
 * filter quota and is therefore not stored in this array.
 */
static int
ice_vf_mac_filter_index(struct ice_vf *vf, const uint8_t *addr)
{

	for (u16 i = 0; i < vf->mac_filter_cnt; i++) {
		if (memcmp(vf->mac_filters[i].addr, addr, ETHER_ADDR_LEN) == 0)
			return (i);
	}
	return (-1);
}

/**
 * ice_vc_add_eth_addr_msg - Handle VIRTCHNL_OP_ADD_ETH_ADDR msg from VF
 * @sc: device private structure
 * @vf: VF tracking structure
 * @msg_buf: raw message buffer from the VF
 *
 * Receives a list of MAC addresses from the VF and adds those addresses
 * to the VSI's filter list.
 */
static void
ice_vc_add_eth_addr_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct virtchnl_ether_addr_list *addr_list;
	struct ice_hw *hw = &sc->hw;
	u16 new_filters;
	int error = 0;

	addr_list = (struct virtchnl_ether_addr_list *)msg_buf;

	/* Validate the entire batch and charge only unique, absent filters. */
	new_filters = 0;
	for (int i = 0; i < addr_list->num_elements; i++) {
		u8 *addr = addr_list->list[i].addr;
		int j;

		error = ice_vf_validate_mac(vf, addr);
		if (error != 0) {
			device_printf(sc->dev,
			    "%s: VF-%d: invalid or unauthorized MAC for VSI %d\n",
			    __func__, vf->vf_num, vf->vsi->idx);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}
		for (j = 0; j < i; j++) {
			if (memcmp(addr_list->list[j].addr, addr,
			    ETHER_ADDR_LEN) == 0)
				break;
		}
		if (j != i || memcmp(addr, vf->mac, ETHER_ADDR_LEN) == 0 ||
		    ice_vf_mac_filter_index(vf, addr) >= 0)
			continue;
		new_filters++;
	}
	if ((u32)vf->mac_filter_cnt + new_filters > vf->mac_filter_limit) {
		v_status = VIRTCHNL_STATUS_ERR_NO_MEMORY;
		goto done;
	}

	for (int i = 0; i < addr_list->num_elements; i++) {
		u8 *addr = addr_list->list[i].addr;
		bool assigned;

		/* The type flag is currently ignored; every MAC address is
		 * treated as the LEGACY type
		 */
		assigned = memcmp(addr, vf->mac, ETHER_ADDR_LEN) == 0;
		if (!assigned && ice_vf_mac_filter_index(vf, addr) >= 0)
			continue;

		error = ice_add_vsi_mac_filter(vf->vsi, addr);
		if (error) {
			device_printf(sc->dev,
			    "%s: VF-%d: Error adding MAC addr for VSI %d\n",
			    __func__, vf->vf_num, vf->vsi->idx);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			continue;
		}
		if (!assigned) {
			MPASS(vf->mac_filter_cnt < vf->mac_filter_limit);
			memcpy(vf->mac_filters[vf->mac_filter_cnt].addr, addr,
			    ETHER_ADDR_LEN);
			vf->mac_filter_cnt++;
		}
	}

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_ADD_ETH_ADDR,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_del_eth_addr_msg - Handle VIRTCHNL_OP_DEL_ETH_ADDR msg from VF
 * @sc: device private structure
 * @vf: VF tracking structure
 * @msg_buf: raw message buffer from the VF
 *
 * Receives a list of MAC addresses from the VF and removes those addresses
 * from the VSI's filter list.
 */
static void
ice_vc_del_eth_addr_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct virtchnl_ether_addr_list *addr_list;
	struct ice_hw *hw = &sc->hw;
	int error = 0;

	addr_list = (struct virtchnl_ether_addr_list *)msg_buf;

	for (int i = 0; i < addr_list->num_elements; i++) {
		u8 *addr = addr_list->list[i].addr;
		bool assigned;
		int index;

		error = ice_vf_validate_mac(vf, addr);
		if (error != 0) {
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			continue;
		}
		assigned = memcmp(addr, vf->mac, ETHER_ADDR_LEN) == 0;
		if (assigned &&
		    (vf->vf_flags & VF_FLAG_SET_MAC_CAP) == 0)
			continue;
		index = assigned ? -1 : ice_vf_mac_filter_index(vf, addr);
		if (!assigned && index < 0)
			continue;

		error = ice_remove_vsi_mac_filter(vf->vsi, addr);
		if (error) {
			device_printf(sc->dev,
			    "%s: VF-%d: Error removing MAC addr for VSI %d\n",
			    __func__, vf->vf_num, vf->vsi->idx);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			continue;
		}
		if (!assigned) {
			if (index + 1 < vf->mac_filter_cnt) {
				memmove(&vf->mac_filters[index],
				    &vf->mac_filters[index + 1],
				    (vf->mac_filter_cnt - index - 1) *
				    sizeof(*vf->mac_filters));
			}
			vf->mac_filter_cnt--;
		}
	}

	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_DEL_ETH_ADDR,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_select_vlans - Compact a VF VLAN request in place
 * @vf: VF tracking structure
 * @vids: VLAN IDs supplied by the VF
 * @count: number of VLAN IDs in the request
 * @add: select absent VLANs for add, or present VLANs for delete
 * @selected_count: returned number of VLAN IDs requiring a hardware change
 *
 * A VF may replay its entire VLAN configuration after a reset or retry a
 * request whose reply was lost.  Select only unique IDs whose membership
 * actually changes so those requests remain idempotent and filter accounting
 * continues to enforce the configured limit.
 */
static int
ice_vc_select_vlans(struct ice_vf *vf, u16 *vids, u16 count, bool add,
    u16 *selected_count)
{
	bitstr_t bit_decl(seen, ICE_VF_VLAN_MAP_LEN);
	u16 selected, vid;

	bzero(seen, sizeof(seen));
	selected = 0;
	for (u16 i = 0; i < count; i++) {
		vid = vids[i];
		if (vid > EVL_VLID_MASK)
			return (EINVAL);
		if (bit_test(seen, vid))
			continue;
		bit_set(seen, vid);
		if (bit_test(vf->vlans_map, vid) == add)
			continue;
		vids[selected++] = vid;
	}
	*selected_count = selected;
	return (0);
}

/**
 * ice_vc_add_vlan_msg - Handle VIRTCHNL_OP_ADD_VLAN msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Adds the VLANs in msg_buf to the VF's VLAN filter list.
 */
static void
ice_vc_add_vlan_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_vlan_filter_list *vlan_list;
	u16 selected;
	int status = 0;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;

	vlan_list = (struct virtchnl_vlan_filter_list *)msg_buf;

	if (vlan_list->vsi_id != vsi->idx) {
		device_printf(sc->dev,
			      "VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
			      vf->vf_num, vsi->idx, vlan_list->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	status = ice_vc_select_vlans(vf, vlan_list->vlan_id,
	    vlan_list->num_elements, true, &selected);
	if (status != 0) {
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	if ((u32)vf->vlan_cnt + selected > vf->vlan_limit) {
		v_status = VIRTCHNL_STATUS_ERR_NO_MEMORY;
		goto done;
	}
	if (selected == 0)
		goto done;

	for (u16 i = 0; i < selected; i++) {
		status = ice_add_vlan_hw_filter(vsi, vlan_list->vlan_id[i]);
		if (status != 0 && status != ICE_ERR_ALREADY_EXISTS) {
			device_printf(sc->dev,
			    "VF-%d: Failure adding VLAN %d to VSI %d, err %s aq_err %s\n",
			    vf->vf_num, vlan_list->vlan_id[i], vsi->idx,
			    ice_status_str(status),
			    ice_aq_str(sc->hw.adminq.sq_last_status));
			v_status = ice_iov_err_to_virt_err(status);
			goto done;
		}
		bit_set(vf->vlans_map, vlan_list->vlan_id[i]);
		vf->vlan_cnt++;
	}

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_ADD_VLAN,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_del_vlan_msg - Handle VIRTCHNL_OP_DEL_VLAN msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Removes the VLANs in msg_buf from the VF's VLAN filter list.
 */
static void
ice_vc_del_vlan_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_vlan_filter_list *vlan_list;
	u16 selected;
	int status = 0;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;

	vlan_list = (struct virtchnl_vlan_filter_list *)msg_buf;

	if (vlan_list->vsi_id != vsi->idx) {
		device_printf(sc->dev,
			      "VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
			      vf->vf_num, vsi->idx, vlan_list->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	status = ice_vc_select_vlans(vf, vlan_list->vlan_id,
	    vlan_list->num_elements, false, &selected);
	if (status != 0) {
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}
	if (selected == 0)
		goto done;

	for (u16 i = 0; i < selected; i++) {
		status = ice_remove_vlan_hw_filter(vsi, vlan_list->vlan_id[i]);
		if (status != 0 && status != ICE_ERR_DOES_NOT_EXIST) {
			device_printf(sc->dev,
			    "VF-%d: Failure deleting VLAN %d from VSI %d, err %s aq_err %s\n",
			    vf->vf_num, vlan_list->vlan_id[i], vsi->idx,
			    ice_status_str(status),
			    ice_aq_str(sc->hw.adminq.sq_last_status));
			v_status = ice_iov_err_to_virt_err(status);
			goto done;
		}
		bit_clear(vf->vlans_map, vlan_list->vlan_id[i]);
		MPASS(vf->vlan_cnt > 0);
		vf->vlan_cnt--;
	}

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_DEL_VLAN,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_validate_queue_select - Validate a VF queue selection
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @vqs: queue selection from the VF
 *
 * Return true when the VSI ID and both queue masks are valid for the VF.
 */
static bool
ice_vc_validate_queue_select(struct ice_softc *sc, struct ice_vf *vf,
    const struct virtchnl_queue_select *vqs)
{
	struct ice_vsi *vsi = vf->vsi;
	int bit;

	if (vqs->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "%s: VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
		    __func__, vf->vf_num, vsi->idx, vqs->vsi_id);
		return (false);
	}
	if (vqs->rx_queues == 0 && vqs->tx_queues == 0) {
		device_printf(sc->dev,
		    "%s: VF-%d: message queue masks are empty\n",
		    __func__, vf->vf_num);
		return (false);
	}

	bit = fls(vqs->rx_queues);
	if (bit > vsi->num_rx_queues) {
		device_printf(sc->dev,
		    "%s: VF-%d: message's Rx queue map (0x%08x) has invalid bit set (%d)\n",
		    __func__, vf->vf_num, vqs->rx_queues, bit);
		return (false);
	}
	bit = fls(vqs->tx_queues);
	if (bit > vsi->num_tx_queues) {
		device_printf(sc->dev,
		    "%s: VF-%d: message's Tx queue map (0x%08x) has invalid bit set (%d)\n",
		    __func__, vf->vf_num, vqs->tx_queues, bit);
		return (false);
	}

	return (true);
}

/**
 * ice_vc_disable_tx_queue - Disable one configured VF Tx queue
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @qid: VF-relative queue ID
 */
static int
ice_vc_disable_tx_queue(struct ice_softc *sc, struct ice_vf *vf, u16 qid)
{
	struct ice_vsi *vsi = vf->vsi;
	struct ice_tx_queue *txq = &vsi->tx_queues[qid];
	struct ice_hw *hw = &sc->hw;
	u16 q_handle, q_id;
	u32 q_teid;
	int status;

	q_handle = txq->q_handle;
	q_id = vsi->tx_qmap[qid];
	q_teid = txq->q_teid;
	status = ice_dis_vsi_txq(hw->port_info, vsi->idx, txq->tc, 1,
	    &q_handle, &q_id, &q_teid, ICE_NO_RESET, 0, NULL);
	if (status != ICE_SUCCESS && status != ICE_ERR_DOES_NOT_EXIST &&
	    status != ICE_ERR_RESET_ONGOING) {
		device_printf(sc->dev,
		    "Failed to disable VF-%d Tx queue %u, err %s aq_err %s\n",
		    vf->vf_num, qid, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}
	txq->q_handle = 0;
	txq->q_teid = 0;

	return (0);
}

/**
 * ice_vc_disable_queues - Disable selected configured VF queues
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @tx_queues: VF-relative Tx queue bitmap
 * @rx_queues: VF-relative Rx queue bitmap
 *
 * Queue disable is idempotent. Queues which are not configured or enabled
 * have no hardware work to perform and are treated as successfully disabled.
 * Since CONFIG_VSI_QUEUES also enables Tx, disabling a Tx queue removes its
 * tracked configuration and the VF must configure it before enabling it again.
 */
static int
ice_vc_disable_queues(struct ice_softc *sc, struct ice_vf *vf,
    u32 tx_queues, u32 rx_queues)
{
	struct ice_vsi *vsi = vf->vsi;
	u32 queues;
	int bit, error;

	queues = rx_queues & vf->rxq_enabled;
	while (queues != 0) {
		bit = ffs(queues) - 1;
		error = ice_control_rx_queue(vsi, bit, false);
		if (error != 0) {
			device_printf(sc->dev,
			    "Unable to disable VF-%d Rx queue %d: %s\n",
			    vf->vf_num, bit, ice_err_str(error));
			return (error);
		}
		vf->rxq_enabled &= ~BIT(bit);
		queues &= ~BIT(bit);
	}

	queues = tx_queues & vf->txq_configured;
	if (queues == vf->txq_configured && queues != 0) {
		error = ice_vsi_disable_tx(vsi);
		if (error != 0)
			return (error);
		vf->txq_configured = 0;
		return (0);
	}
	while (queues != 0) {
		bit = ffs(queues) - 1;
		error = ice_vc_disable_tx_queue(sc, vf, bit);
		if (error != 0)
			return (error);
		vf->txq_configured &= ~BIT(bit);
		queues &= ~BIT(bit);
	}

	return (0);
}

/**
 * ice_vc_validate_ring_len - Check to see if a descriptor ring length is valid
 * @ring_len: length of ring
 *
 * Check whether a ring size value is valid.
 *
 * @returns true if given ring size is valid
 */
static bool
ice_vc_isvalid_ring_len(u32 ring_len)
{
	return (ring_len >= ICE_MIN_DESC_COUNT &&
		ring_len <= ICE_MAX_DESC_COUNT &&
		!(ring_len % ICE_DESC_COUNT_INCR));
}

/**
 * ice_vc_isvalid_txq - Validate a VF transmit queue description
 * @txq: VF-supplied transmit queue description
 *
 * Queue base addresses are encoded in the hardware context in 128-byte
 * units. Reject values which would be truncated while building the context.
 */
static bool
ice_vc_isvalid_txq(const struct virtchnl_txq_info *txq)
{
	u64 align;

	align = BIT_ULL(ICE_TLAN_CTX_BASE_S);
	return (ice_vc_isvalid_ring_len(txq->ring_len) &&
	    txq->dma_ring_addr != 0 &&
	    (txq->dma_ring_addr & (align - 1)) == 0 &&
	    txq->headwb_enabled == 0);
}

/**
 * ice_vc_isvalid_rxq - Validate a VF receive queue description
 * @rxq: VF-supplied receive queue description
 *
 * The receive queue context stores its ring base and data buffer size in
 * 128-byte units. It can represent data buffers from 128 through 16256
 * bytes. The current driver supports neither header splitting nor retaining
 * the Ethernet CRC for VFs. Some older iavf drivers request the maximum PF
 * frame size with a buffer too small to hold it in five segments. Accept that
 * advisory mismatch; ice_setup_rx_ctx() safely limits the hardware RXMAX to
 * five data buffers.
 */
static bool
ice_vc_isvalid_rxq(const struct virtchnl_rxq_info *rxq)
{
	u64 ring_align;
	u32 buffer_align;

	ring_align = BIT_ULL(ICE_RLAN_BASE_S);
	buffer_align = BIT(ICE_RLAN_CTX_DBUF_S);

	return (ice_vc_isvalid_ring_len(rxq->ring_len) &&
	    rxq->dma_ring_addr != 0 &&
	    (rxq->dma_ring_addr & (ring_align - 1)) == 0 &&
	    rxq->databuffer_size >= buffer_align &&
	    rxq->databuffer_size <= ICE_VC_MAX_RX_BUFFER &&
	    (rxq->databuffer_size & (buffer_align - 1)) == 0 &&
	    rxq->max_pkt_size >= ETHER_MIN_LEN &&
	    rxq->max_pkt_size <= ICE_MAX_FRAME_SIZE &&
	    rxq->splithdr_enabled == 0 && rxq->crc_disable == 0);
}

/**
 * ice_vc_isvalid_itr_idx - Validate a virtchnl interrupt throttle index
 * @itr_idx: VF-supplied ITR index
 */
static bool
ice_vc_isvalid_itr_idx(u16 itr_idx)
{

	return (itr_idx == VIRTCHNL_ITR_IDX_0 ||
	    itr_idx == VIRTCHNL_ITR_IDX_1 ||
	    itr_idx == VIRTCHNL_ITR_IDX_NO_ITR);
}

/**
 * ice_vc_cfg_vsi_qs_msg - Handle VIRTCHNL_OP_CONFIG_VSI_QUEUES msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 */
static void
ice_vc_cfg_vsi_qs_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	device_t dev = sc->dev;
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_vsi_queue_config_info *vqci;
	struct virtchnl_queue_pair_info *vqpi;
	enum virtchnl_status_code status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;
	struct ice_tx_queue *txq;
	struct ice_rx_queue *rxq;
	u32 expected_map, max_pkt_size, queue_map, rx_buffer_size;
	int i, error = 0;

	vqci = (struct virtchnl_vsi_queue_config_info *)msg_buf;
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    malformed_queues, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		switch (RETURN_VALUE) {
		case 1:
			vqci->qpair[0].txq.dma_ring_addr |= 1;
			break;
		case 2:
			vqci->qpair[0].rxq.dma_ring_addr |= 1;
			break;
		case 3:
			vqci->qpair[0].rxq.databuffer_size++;
			break;
		case 4:
			vqci->qpair[0].rxq.max_pkt_size = 0;
			break;
		case 5:
			if (vqci->num_queue_pairs > 1) {
				vqci->qpair[1].txq.queue_id =
				    vqci->qpair[0].txq.queue_id;
				vqci->qpair[1].rxq.queue_id =
				    vqci->qpair[0].rxq.queue_id;
			} else {
				vqci->qpair[0].txq.queue_id++;
			}
			break;
		case 6:
			vqci->qpair[0].rxq.databuffer_size =
			    ICE_VC_MAX_RX_BUFFER + BIT(ICE_RLAN_CTX_DBUF_S);
			break;
		default:
			vqci->vsi_id++;
			break;
		}
	});

	if (vqci->vsi_id != vsi->idx || vqci->num_queue_pairs == 0 ||
	    vqci->num_queue_pairs > sizeof(queue_map) * NBBY ||
	    vqci->num_queue_pairs > vsi->num_tx_queues ||
	    vqci->num_queue_pairs > vsi->num_rx_queues) {
		status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	queue_map = 0;
	rx_buffer_size = 0;
	max_pkt_size = 0;
	vqpi = vqci->qpair;
	for (i = 0; i < vqci->num_queue_pairs; i++, vqpi++) {
		if (vqpi->txq.vsi_id != vsi->idx ||
		    vqpi->rxq.vsi_id != vsi->idx ||
		    vqpi->txq.queue_id != vqpi->rxq.queue_id ||
		    vqpi->txq.queue_id >= vsi->num_tx_queues ||
		    vqpi->rxq.queue_id >= vsi->num_rx_queues ||
		    (queue_map & BIT(vqpi->txq.queue_id)) != 0 ||
		    !ice_vc_isvalid_txq(&vqpi->txq) ||
		    !ice_vc_isvalid_rxq(&vqpi->rxq)) {
			status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}
		if (i == 0) {
			rx_buffer_size = vqpi->rxq.databuffer_size;
			max_pkt_size = vqpi->rxq.max_pkt_size;
		} else if (vqpi->rxq.databuffer_size != rx_buffer_size ||
		    vqpi->rxq.max_pkt_size != max_pkt_size) {
			status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}
		queue_map |= BIT(vqpi->txq.queue_id);
	}
	if (vqci->num_queue_pairs == sizeof(queue_map) * NBBY)
		expected_map = ~0U;
	else
		expected_map = BIT(vqci->num_queue_pairs) - 1;
	if (queue_map != expected_map) {
		status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	error = ice_vc_disable_queues(sc, vf, vf->txq_configured,
	    vf->rxq_enabled);
	if (error != 0) {
		status = VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
		goto done;
	}
	vf->txq_configured = 0;
	vf->rxq_configured = 0;
	vf->rxq_enabled = 0;

	/*
	 * Clear TX and RX queues config in case VF
	 * requests different number of queues.
	 */
	for (i = 0; i < vsi->num_tx_queues; i++) {
		txq = &vsi->tx_queues[i];

		txq->desc_count = 0;
		txq->tx_paddr = 0;
		txq->q_teid = 0;
		txq->q_handle = 0;
		txq->tc = 0;
	}

	for (i = 0; i < vsi->num_rx_queues; i++) {
		rxq = &vsi->rx_queues[i];

		rxq->desc_count = 0;
		rxq->rx_paddr = 0;
	}

	vqpi = vqci->qpair;
	for (i = 0; i < vqci->num_queue_pairs; i++, vqpi++) {
		/* Copy parameters into VF's queue/VSI structs */
		txq = &vsi->tx_queues[vqpi->txq.queue_id];

		txq->desc_count = vqpi->txq.ring_len;
		txq->tx_paddr = vqpi->txq.dma_ring_addr;
		txq->q_handle = vqpi->txq.queue_id;
		txq->tc = 0;

		rxq = &vsi->rx_queues[vqpi->rxq.queue_id];

		rxq->desc_count = vqpi->rxq.ring_len;
		rxq->rx_paddr = vqpi->rxq.dma_ring_addr;
	}
	vsi->mbuf_sz = rx_buffer_size;
	vsi->max_frame_size = max_pkt_size;

	/* Configure TX queues in HW */
	/*
	 * Record the intended map before programming hardware so a partial
	 * firmware failure remains discoverable and can be cleaned up by the
	 * next configuration attempt.
	 */
	vf->txq_configured = queue_map;
	error = ice_cfg_vsi_for_tx(vsi);
	if (error) {
		device_printf(dev,
			      "VF-%d: Unable to configure VSI for Tx: %s\n",
			      vf->vf_num, ice_err_str(error));
		status = VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
		if (ice_vsi_disable_tx(vsi) == 0)
			vf->txq_configured = 0;
		goto done;
	}

	/* Configure RX queues in HW */
	error = ice_cfg_vsi_for_rx(vsi);
	if (error) {
		device_printf(dev,
			      "VF-%d: Unable to configure VSI for Rx: %s\n",
			      vf->vf_num, ice_err_str(error));
		status = VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
		(void)ice_vc_disable_queues(sc, vf, vf->txq_configured, 0);
		goto done;
	}
	vf->rxq_configured = queue_map;

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_CONFIG_VSI_QUEUES,
	    status, NULL, 0, NULL);
}

/**
 * ice_vc_cfg_rss_key_msg - Handle VIRTCHNL_OP_CONFIG_RSS_KEY msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Sets the RSS key for the given VF, using the contents of msg_buf.
 */
static void
ice_vc_cfg_rss_key_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_aqc_get_set_rss_keys keydata =
	    { .standard_rss_key = {0}, .extended_hash_key = {0} };
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_rss_key *vrk;
	int status = 0;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;

	vrk = (struct virtchnl_rss_key *)msg_buf;
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    malformed_rss_key, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		vrk->key_len--;
	});

	if (vrk->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
		    vf->vf_num, vsi->idx, vrk->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	/* The VF must use the exact key size advertised by this PF. */
	if (vrk->key_len != ICE_GET_SET_RSS_KEY_EXTEND_KEY_SIZE) {
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	memcpy(&keydata, vrk->key, vrk->key_len);

	status = ice_aq_set_rss_key(hw, vsi->idx, &keydata);
	if (status) {
		device_printf(sc->dev,
			      "ice_aq_set_rss_key status %s, error %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		v_status = ice_iov_err_to_virt_err(status);
		goto done;
	}

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_CONFIG_RSS_KEY,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_cfg_rss_lut_msg - Handle VIRTCHNL_OP_CONFIG_RSS_LUT msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Adds the LUT from the VF in msg_buf to the PF via an admin queue call.
 */
static void
ice_vc_cfg_rss_lut_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_rss_lut *vrl;
	int i, status = 0;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_aq_get_set_rss_lut_params lut_params = {};
	struct ice_vsi *vsi = vf->vsi;

	vrl = (struct virtchnl_rss_lut *)msg_buf;
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    malformed_rss_lut, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		if (RETURN_VALUE == 1)
			vrl->lut_entries--;
		else
			vrl->lut[0] = vsi->num_rx_queues;
	});

	if (vrl->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
		    vf->vf_num, vsi->idx, vrl->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	/* The VF must use the exact LUT size advertised by this PF. */
	if (vrl->lut_entries != vsi->rss_table_size) {
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}
	for (i = 0; i < vrl->lut_entries; i++) {
		if (vrl->lut[i] >= vsi->num_rx_queues) {
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}
	}

	lut_params.vsi_handle = vsi->idx;
	lut_params.lut_size = vrl->lut_entries;
	lut_params.lut_type = vsi->rss_lut_type;
	lut_params.lut = vrl->lut;
	lut_params.global_lut_id = 0;

	status = ice_aq_set_rss_lut(hw, &lut_params);
	if (status) {
		device_printf(sc->dev,
			      "VF-%d: Cannot set RSS lut, err %s aq_err %s\n",
			      vf->vf_num, ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		v_status = ice_iov_err_to_virt_err(status);
	}

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_CONFIG_RSS_LUT,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_set_rss_hena_msg - Handle VIRTCHNL_OP_SET_RSS_HENA msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Adds the VF's hena (hash enable) bits as flow types to the PF's RSS flow
 * type list.
 */
static void
ice_vc_set_rss_hena_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_rss_hena *vrh;
	int status = 0;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;

	MPASS(vsi != NULL);

	vrh = (struct virtchnl_rss_hena *)msg_buf;

	/*
	 * Remove existing configuration to make sure only requested
	 * config is applied and allow VFs to disable RSS completly.
	 */
	status = ice_rem_vsi_rss_cfg(hw, vsi->idx);
	if (vrh->hena) {
		/*
		 * Problem with removing config is not fatal, when new one
		 * is requested. Warn about it but try to apply new config
		 * anyway.
		 */
		if (status)
			device_printf(sc->dev,
			    "ice_rem_vsi_rss_cfg status %s, error %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
		status = ice_add_avf_rss_cfg(hw, vsi->idx, vrh->hena);
		if (status)
			device_printf(sc->dev,
			    "ice_add_avf_rss_cfg status %s, error %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
	}
	v_status = ice_iov_err_to_virt_err(status);
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_SET_RSS_HENA,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_enable_queues_msg - Handle VIRTCHNL_OP_ENABLE_QUEUES msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Enables VF queues selected in msg_buf for Tx/Rx traffic.
 *
 * @remark Only actually operates on Rx queues; Tx queues are enabled in
 * CONFIG_VSI_QUEUES message handler.
 */
static void
ice_vc_enable_queues_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_queue_select *vqs;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;
	u32 queues;
	int bit, error;

	vqs = (struct virtchnl_queue_select *)msg_buf;

	if (!ice_vc_validate_queue_select(sc, vf, vqs)) {
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}
	if ((vqs->tx_queues & ~vf->txq_configured) != 0 ||
	    (vqs->rx_queues & ~vf->rxq_configured) != 0) {
		device_printf(sc->dev,
		    "%s: VF-%d: cannot enable unconfigured queues "
		    "(Tx 0x%08x, Rx 0x%08x)\n",
		    __func__, vf->vf_num, vqs->tx_queues,
		    vqs->rx_queues);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	queues = vqs->rx_queues & ~vf->rxq_enabled;
	while (queues != 0) {
		bit = ffs(queues) - 1;
		error = ice_control_rx_queue(vsi, bit, true);
		if (error) {
			device_printf(sc->dev,
			    "Unable to enable VF-%d Rx queue %d: %s\n",
			    vf->vf_num, bit, ice_err_str(error));
			v_status = VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
			goto done;
		}
		vf->rxq_enabled |= BIT(bit);
		queues &= ~BIT(bit);
	}
	/* Tx queues were enabled when their contexts were configured. */

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_ENABLE_QUEUES,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_disable_queues_msg - Handle VIRTCHNL_OP_DISABLE_QUEUES msg
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Disables the selected VF Tx and Rx queues. Repeated requests for queues
 * which are already disabled complete successfully without touching hardware.
 */
static void
ice_vc_disable_queues_msg(struct ice_softc *sc, struct ice_vf *vf,
			  u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_queue_select *vqs;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	int error;

	vqs = (struct virtchnl_queue_select *)msg_buf;
	if (!ice_vc_validate_queue_select(sc, vf, vqs)) {
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}
	error = ice_vc_disable_queues(sc, vf, vqs->tx_queues,
	    vqs->rx_queues);
	if (error != 0)
		v_status = VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_DISABLE_QUEUES,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_cfg_irq_map_msg - Handle VIRTCHNL_OP_CFG_IRQ_MAP msg from VF
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Configures the interrupt vectors described in the message in msg_buf. The
 * VF needs to send this message during init, so that queues can be allowed
 * to generate interrupts.
 */
static void
ice_vc_cfg_irq_map_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_irq_map_info *vimi;
	struct virtchnl_vector_map *vvm;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;
	u32 vectors_seen;
	u16 rxqs_seen, txqs_seen, valid_rxqs, valid_txqs, vector;

	vimi = (struct virtchnl_irq_map_info *)msg_buf;
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    malformed_irq_map, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		switch (RETURN_VALUE) {
		case 1:
			vimi->vecmap[0].rxitr_idx = VIRTCHNL_ITR_IDX_NO_ITR + 1;
			break;
		case 2:
			vimi->vecmap[0].vector_id = 0;
			vimi->vecmap[0].rxq_map = 1;
			break;
		case 3:
			if (vimi->num_vectors > 1) {
				vimi->vecmap[1].vector_id =
				    vimi->vecmap[0].vector_id;
			} else {
				vimi->vecmap[0].vsi_id++;
			}
			break;
		default:
			vimi->vecmap[0].vsi_id++;
			break;
		}
	});

	if (vimi->num_vectors == 0 ||
	    vimi->num_vectors > vf->num_irq_vectors ||
	    vimi->num_vectors > sizeof(vectors_seen) * NBBY ||
	    vsi->num_tx_queues < 1 ||
	    vsi->num_tx_queues > ICE_VIRTCHNL_QUEUE_MAP_SIZE ||
	    vsi->num_rx_queues < 1 ||
	    vsi->num_rx_queues > ICE_VIRTCHNL_QUEUE_MAP_SIZE) {
		device_printf(sc->dev,
		    "%s: VF-%d: invalid vector count %d (VF has %d)\n",
		    __func__, vf->vf_num, vimi->num_vectors, vf->num_irq_vectors);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	valid_txqs = vsi->num_tx_queues == ICE_VIRTCHNL_QUEUE_MAP_SIZE ?
	    (u16)~0U : (u16)(BIT(vsi->num_tx_queues) - 1);
	valid_rxqs = vsi->num_rx_queues == ICE_VIRTCHNL_QUEUE_MAP_SIZE ?
	    (u16)~0U : (u16)(BIT(vsi->num_rx_queues) - 1);
	vectors_seen = 0;
	txqs_seen = 0;
	rxqs_seen = 0;

	/* Validate the complete request before changing any queue state. */
	vvm = vimi->vecmap;
	for (int i = 0; i < vimi->num_vectors; i++, vvm++) {
		/* vvm->vector_id is relative to VF space */
		vector = vvm->vector_id;
		if (vvm->vsi_id != vsi->idx ||
		    vector >= vf->num_irq_vectors ||
		    vector >= sizeof(vectors_seen) * NBBY ||
		    (vectors_seen & BIT(vector)) != 0 ||
		    !ice_vc_isvalid_itr_idx(vvm->txitr_idx) ||
		    !ice_vc_isvalid_itr_idx(vvm->rxitr_idx) ||
		    (vvm->txq_map & ~valid_txqs) != 0 ||
		    (vvm->rxq_map & ~valid_rxqs) != 0 ||
		    (txqs_seen & vvm->txq_map) != 0 ||
		    (rxqs_seen & vvm->rxq_map) != 0 ||
		    (vector == 0 &&
		    (vvm->txq_map != 0 || vvm->rxq_map != 0))) {
			device_printf(sc->dev,
			    "%s: VF-%d: invalid queue mapping for vector %u\n",
			    __func__, vf->vf_num, vector);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}
		vectors_seen |= BIT(vector);
		txqs_seen |= vvm->txq_map;
		rxqs_seen |= vvm->rxq_map;
	}

	/* Save the validated queue-to-vector mappings. */
	vvm = vimi->vecmap;
	for (int i = 0; i < vimi->num_vectors; i++, vvm++) {
		struct ice_tx_queue *txq;
		struct ice_rx_queue *rxq;
		int bit;

		vector = vvm->vector_id;

		/* The Misc/Admin Queue vector doesn't need mapping */
		if (vector == 0)
			continue;

		for (bit = 0; bit < ICE_VIRTCHNL_QUEUE_MAP_SIZE; bit++) {
			if ((vvm->txq_map & BIT(bit)) == 0)
				continue;
			vf->tx_irqvs[vector].me = vector;

			txq = &vsi->tx_queues[bit];
			txq->irqv = &vf->tx_irqvs[vector];
			txq->itr_idx = vvm->txitr_idx;
		}
		for (bit = 0; bit < ICE_VIRTCHNL_QUEUE_MAP_SIZE; bit++) {
			if ((vvm->rxq_map & BIT(bit)) == 0)
				continue;
			vf->rx_irqvs[vector].me = vector;

			rxq = &vsi->rx_queues[bit];
			rxq->irqv = &vf->rx_irqvs[vector];
			rxq->itr_idx = vvm->rxitr_idx;
		}
	}

	/* Write to T/RQCTL registers to actually map vectors to queues */
	for (int i = 0; i < vf->vsi->num_rx_queues; i++)
		if (vsi->rx_queues[i].irqv != NULL)
			ice_configure_rxq_interrupt(hw, vsi->rx_qmap[i],
			    vsi->rx_queues[i].irqv->me, vsi->rx_queues[i].itr_idx);

	for (int i = 0; i < vf->vsi->num_tx_queues; i++)
		if (vsi->tx_queues[i].irqv != NULL)
			ice_configure_txq_interrupt(hw, vsi->tx_qmap[i],
			    vsi->tx_queues[i].irqv->me, vsi->tx_queues[i].itr_idx);

	ice_flush(hw);

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_CONFIG_IRQ_MAP,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_eth_stats_to_virtchnl_eth_stats - Convert stats for virtchnl
 * @istats: VSI stats from HW to convert
 * @vstats: stats struct to copy to
 *
 * This function copies all known stats in struct virtchnl_eth_stats from the
 * input struct ice_eth_stats to an output struct virtchnl_eth_stats.
 *
 * @remark These two structure types currently have the same definition up to
 * the size of struct virtchnl_eth_stats (on FreeBSD), but that could change
 * in the future.
 */
static void
ice_eth_stats_to_virtchnl_eth_stats(struct ice_eth_stats *istats,
				    struct virtchnl_eth_stats *vstats)
{
	vstats->rx_bytes = istats->rx_bytes;
	vstats->rx_unicast = istats->rx_unicast;
	vstats->rx_multicast = istats->rx_multicast;
	vstats->rx_broadcast = istats->rx_broadcast;
	vstats->rx_discards = istats->rx_discards;
	vstats->rx_unknown_protocol = istats->rx_unknown_protocol;
	vstats->tx_bytes = istats->tx_bytes;
	vstats->tx_unicast = istats->tx_unicast;
	vstats->tx_multicast = istats->tx_multicast;
	vstats->tx_broadcast = istats->tx_broadcast;
	vstats->tx_discards = istats->tx_discards;
	vstats->tx_errors = istats->tx_errors;
}

/**
 * ice_vc_get_stats_msg - Handle VIRTCHNL_OP_GET_STATS msg
 * @sc: device private structure
 * @vf: VF tracking structure
 * @msg_buf: raw message buffer from the VF
 *
 * Updates the VF's VSI stats and sends those stats back to the VF.
 */
static void
ice_vc_get_stats_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct virtchnl_queue_select *vqs;
	struct virtchnl_eth_stats stats;
	struct ice_vsi *vsi = vf->vsi;
	struct ice_hw *hw = &sc->hw;

	vqs = (struct virtchnl_queue_select *)msg_buf;
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    get_stats_bad_vsi, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		vqs->vsi_id = vsi->idx + 1;
		device_printf(sc->dev,
		    "injecting invalid GET_STATS VSI ID for VF %u\n",
		    (unsigned int)vf->vf_num);
	});

	if (vqs->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "%s: VF-%d: message has invalid VSI ID %d (VF has VSI ID %d)\n",
		    __func__, vf->vf_num, vqs->vsi_id, vsi->idx);
		ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_GET_STATS,
		    VIRTCHNL_STATUS_ERR_PARAM, NULL, 0, NULL);
		return;
	}

	ice_update_vsi_hw_stats(vf->vsi);
	ice_eth_stats_to_virtchnl_eth_stats(&vsi->hw_stats.cur, &stats);

	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_GET_STATS,
	    VIRTCHNL_STATUS_SUCCESS, (u8 *)&stats,
	    sizeof(struct virtchnl_eth_stats), NULL);
}

/**
 * ice_vc_cfg_promisc_mode_msg - Handle VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE
 * @sc: PF's softc structure
 * @vf: VF tracking structure
 * @msg_buf: message buffer from VF
 *
 * Configures the promiscuous modes for the given VSI in msg_buf.
 */
static void
ice_vc_cfg_promisc_mode_msg(struct ice_softc *sc, struct ice_vf *vf, u8 *msg_buf)
{
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_promisc_info *vpi;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	int status = 0;
	struct ice_vsi *vsi = vf->vsi;
	ice_declare_bitmap(old_promisc_mask, ICE_PROMISC_MAX);
	ice_declare_bitmap(req_promisc_mask, ICE_PROMISC_MAX);
	ice_declare_bitmap(clear_promisc_mask, ICE_PROMISC_MAX);
	ice_declare_bitmap(set_promisc_mask, ICE_PROMISC_MAX);
	ice_declare_bitmap(old_req_xor_mask, ICE_PROMISC_MAX);
	u16 vid;

	vpi = (struct virtchnl_promisc_info *)msg_buf;

	/* Check to see if VF has permission to configure promiscuous mode */
	if (!(vf->vf_flags & VF_FLAG_PROMISC_CAP)) {
		device_printf(sc->dev,
			      "VF-%d: attempted to configure promiscuous mode\n",
			      vf->vf_num);
		/* Don't reply to VF with an error */
		goto done;
	}

	if (vpi->vsi_id != vsi->idx) {
		device_printf(sc->dev,
			      "VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
			      vf->vf_num, vsi->idx, vpi->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	if (vpi->flags & ~ICE_VIRTCHNL_VALID_PROMISC_FLAGS) {
		device_printf(sc->dev,
			      "VF-%d: Message has invalid promiscuous flags set (valid 0x%02x, got 0x%02x)\n",
			      vf->vf_num, ICE_VIRTCHNL_VALID_PROMISC_FLAGS,
			      vpi->flags);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;

	}

	ice_zero_bitmap(req_promisc_mask, ICE_PROMISC_MAX);
	/* Convert virtchnl flags to ice AQ promiscuous mode flags */
	if (vpi->flags & FLAG_VF_UNICAST_PROMISC) {
		ice_set_bit(ICE_PROMISC_UCAST_TX, req_promisc_mask);
		ice_set_bit(ICE_PROMISC_UCAST_RX, req_promisc_mask);
	}
	if (vpi->flags & FLAG_VF_MULTICAST_PROMISC) {
		ice_set_bit(ICE_PROMISC_MCAST_TX, req_promisc_mask);
		ice_set_bit(ICE_PROMISC_MCAST_RX, req_promisc_mask);
	}

	status = ice_get_vsi_promisc(hw, vsi->idx, old_promisc_mask, &vid);
	if (status) {
		device_printf(sc->dev,
			      "VF-%d: Failed to get promiscuous mode mask for VSI %d, err %s aq_err %s\n",
			      vf->vf_num, vsi->idx,
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		v_status = ice_iov_err_to_virt_err(status);
		goto done;
	}

	/* Figure out what got added and what got removed */
	ice_zero_bitmap(old_req_xor_mask, ICE_PROMISC_MAX);
	ice_xor_bitmap(old_req_xor_mask, old_promisc_mask, req_promisc_mask, ICE_PROMISC_MAX);
	ice_and_bitmap(clear_promisc_mask, old_req_xor_mask, old_promisc_mask, ICE_PROMISC_MAX);
	ice_and_bitmap(set_promisc_mask, old_req_xor_mask, req_promisc_mask, ICE_PROMISC_MAX);

	if (ice_is_any_bit_set(clear_promisc_mask, ICE_PROMISC_MAX)) {
		status = ice_clear_vsi_promisc(hw, vsi->idx,
					       clear_promisc_mask, 0);
		if (status) {
			device_printf(sc->dev,
				      "VF-%d: Failed to clear promiscuous mode for VSI %d, err %s aq_err %s\n",
				      vf->vf_num, vsi->idx,
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
			v_status = ice_iov_err_to_virt_err(status);
			goto done;
		}
	}

	if (ice_is_any_bit_set(set_promisc_mask, ICE_PROMISC_MAX)) {
		status = ice_set_vsi_promisc(hw, vsi->idx, set_promisc_mask, 0);
		if (status) {
			device_printf(sc->dev,
				      "VF-%d: Failed to set promiscuous mode for VSI %d, err %s aq_err %s\n",
				      vf->vf_num, vsi->idx,
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
			v_status = ice_iov_err_to_virt_err(status);
			goto done;
		}
	}

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE,
	    v_status, NULL, 0, NULL);
}

/**
 * ice_vc_notify_all_vfs_link_state - Notify all VFs of PF link state
 * @sc: device private structure
 *
 * Sends a message to all VFs about the status of the PF's link
 * state. For more details, @see ice_vc_notify_vf_link_state.
 */
void
ice_vc_notify_all_vfs_link_state(struct ice_softc *sc)
{
	for (int i = 0; i < sc->num_vfs; i++)
		ice_vc_notify_vf_link_state(sc, &sc->vfs[i]);
}

/**
 * ice_vc_notify_vf_link_state - Notify VF of PF link state
 * @sc: device private structure
 * @vf: VF tracking structure
 *
 * Sends an event message to the specified VF with information about
 * the current link state from the PF's port. This includes whether
 * link is up or down, and the link speed in 100Mbps units.
 */
static void
ice_vc_notify_vf_link_state(struct ice_softc *sc, struct ice_vf *vf)
{
	struct virtchnl_pf_event event = {};
	struct ice_hw *hw = &sc->hw;

	event.event = VIRTCHNL_EVENT_LINK_CHANGE;
	event.severity = PF_EVENT_SEVERITY_INFO;
	event.event_data.link_event_adv.link_status = sc->link_up;
	event.event_data.link_event_adv.link_speed =
		(u32)ice_conv_link_speed_to_virtchnl(true,
		    hw->port_info->phy.link_info.link_speed);

	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_EVENT,
	    VIRTCHNL_STATUS_SUCCESS, (u8 *)&event, sizeof(event), NULL);
}

/**
 * ice_iov_mbx_overflow - Detect and isolate a VF flooding the PF mailbox
 * @sc: device private structure
 * @vf: VF which sent the current message
 * @mbx_data: software mailbox snapshot data, or NULL on E830
 *
 * E830 enforces the per-VF watermark in hardware.  On older devices, reset
 * and block a VF after the Intel snapshot detector first attributes an
 * overflow.  A later external VF reset, PF reset, or SR-IOV recreation
 * releases it.
 *
 * @returns true if the current message must be discarded.
 */
static bool
ice_iov_mbx_overflow(struct ice_softc *sc, struct ice_vf *vf,
    struct ice_mbx_data *mbx_data)
{
	struct ice_hw *hw = &sc->hw;
	bool report_malvf;
	u32 reg, vf_flags;
	int error, status;

	if (mbx_data == NULL)
		return (false);

	/* Every message advances the snapshot, including a blocked VF's. */
	report_malvf = false;
	status = ice_mbx_vf_state_handler(hw, mbx_data, &vf->mbx_info,
	    &report_malvf);
	if ((atomic_load_acq_32(&vf->vf_flags) & VF_FLAG_MBX_BLOCKED) != 0)
		return (true);
	ICE_FAIL_POINT_CODE_COND(sc, _debug_fail_point_ice_iov,
	    mailbox_overflow, ice_iov_fail_vf_matches(vf->vf_num),
	    FAIL_POINT_NONSLEEPABLE, {
		status = 0;
		vf->mbx_info.malicious = 1;
		report_malvf = true;
	});
	if (status != 0) {
		device_printf(sc->dev,
		    "Unable to check VF %u mailbox overflow, err %s\n",
		    vf->vf_num, ice_status_str(status));
		return (false);
	}
	if (!report_malvf)
		return (vf->mbx_info.malicious != 0);

	vf->mbx_overflow_events++;
	atomic_set_32(&vf->vf_flags, VF_FLAG_MBX_BLOCKED);
	device_printf(sc->dev,
	    "VF %u exceeded the mailbox message limit; resetting and blocking it\n",
	    vf->vf_num);

	vf_flags = atomic_load_acq_32(&vf->vf_flags);
	if ((vf_flags & VF_FLAG_ENABLED) != 0 && vf->vsi != NULL) {
		error = ice_reset_vf(sc, vf, true, false);
		if (error != 0) {
			device_printf(sc->dev,
			    "Unable to isolate VF %u after mailbox overflow: %s\n",
			    vf->vf_num, ice_err_str(error));
		} else {
			/*
			 * Leave queues and mailbox requests blocked, but complete VFR
			 * so a later physical FLR can create a new reset edge and
			 * recover the VF.
			 */
			ice_iov_complete_vf_reset(sc, vf, false);
		}
	} else {
		/* An incompletely configured VF has no queues to drain. */
		reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_num));
		reg |= VPGEN_VFRTRIG_VFSWR_M;
		wr32(hw, VPGEN_VFRTRIG(vf->vf_num), reg);
		ice_flush(hw);
	}

	return (true);
}

/**
 * ice_vc_handle_vf_msg - Handle a message from a VF
 * @sc: device private structure
 * @event: event received from the HW MBX queue
 * @mbx_data: software overflow-detection data, or NULL on E830
 *
 * Called whenever an event is received from a VF on the HW mailbox queue.
 * Responsible for handling these messages as well as responding to the
 * VF afterwards, depending on the received message type.
 */
void
ice_vc_handle_vf_msg(struct ice_softc *sc, struct ice_rq_event_info *event,
    struct ice_mbx_data *mbx_data)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct ice_vf *vf;
	int err = 0;
	u32 vf_flags;

	u32 v_opcode = event->desc.cookie_high;
	u16 v_id = event->desc.retval;
	u8 *msg = event->msg_buf;
	u16 msglen = event->msg_len;

	if (v_id >= sc->num_vfs) {
		device_printf(dev, "%s: Received msg from invalid VF-%d: opcode %d, len %d\n",
		    __func__, v_id, v_opcode, msglen);
		return;
	}

	vf = &sc->vfs[v_id];
	if (ice_iov_mbx_overflow(sc, vf, mbx_data))
		return;

	/* Perform basic checks on the msg */
	err = virtchnl_vc_validate_vf_msg(&vf->version, v_opcode, msg, msglen);
	if (err) {
		device_printf(dev, "%s: Received invalid msg from VF-%d: opcode %d, len %d, error %d\n",
		    __func__, vf->vf_num, v_opcode, msglen, err);
		ice_aq_send_msg_to_vf(hw, v_id, v_opcode, VIRTCHNL_STATUS_ERR_PARAM, NULL, 0, NULL);
		return;
	}

	vf_flags = atomic_load_acq_32(&vf->vf_flags);
	if ((vf_flags & VF_FLAG_ENABLED) == 0 || vf->vsi == NULL)
		return;
	/* Only a reset outside this dispatcher may release an isolated VF. */
	if ((vf_flags & (VF_FLAG_MDD_BLOCKED | VF_FLAG_MBX_BLOCKED)) != 0)
		return;

	/*
	 * Permit only reset negotiation while VF hardware state is unsafe.
	 * A VFR can retry RESET_FAILED; REBUILD_REQUIRED needs a PF rebuild.
	 */
	if ((vf_flags & (VF_FLAG_REBUILD_REQUIRED | VF_FLAG_RESET_FAILED)) != 0 &&
	    v_opcode != VIRTCHNL_OP_VERSION &&
	    v_opcode != VIRTCHNL_OP_RESET_VF) {
		ice_aq_send_msg_to_vf(hw, v_id, v_opcode,
		    VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR, NULL, 0, NULL);
		return;
	}

	switch (v_opcode) {
	case VIRTCHNL_OP_VERSION:
		ice_vc_version_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_RESET_VF:
		ice_reset_vf(sc, vf, true, true);
		break;
	case VIRTCHNL_OP_GET_VF_RESOURCES:
		ice_vc_get_vf_res_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_ADD_ETH_ADDR:
		ice_vc_add_eth_addr_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_DEL_ETH_ADDR:
		ice_vc_del_eth_addr_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_ADD_VLAN:
		ice_vc_add_vlan_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_DEL_VLAN:
		ice_vc_del_vlan_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		ice_vc_cfg_vsi_qs_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_RSS_KEY:
		ice_vc_cfg_rss_key_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_RSS_LUT:
		ice_vc_cfg_rss_lut_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_SET_RSS_HENA:
		ice_vc_set_rss_hena_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_ENABLE_QUEUES:
		ice_vc_enable_queues_msg(sc, vf, msg);
		ice_vc_notify_vf_link_state(sc, vf);
		break;
	case VIRTCHNL_OP_DISABLE_QUEUES:
		ice_vc_disable_queues_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_IRQ_MAP:
		ice_vc_cfg_irq_map_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_GET_STATS:
		ice_vc_get_stats_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		ice_vc_cfg_promisc_mode_msg(sc, vf, msg);
		break;
	default:
		device_printf(dev, "%s: Received unknown msg from VF-%d: opcode %d, len %d\n",
		    __func__, vf->vf_num, v_opcode, msglen);
		ice_aq_send_msg_to_vf(hw, v_id, v_opcode,
		    VIRTCHNL_STATUS_ERR_NOT_SUPPORTED, NULL, 0, NULL);
		break;
	}
}

/**
 * ice_iov_setup_intr_mapping - Setup interrupt config for a VF
 * @sc: device softc structure
 * @vf: driver's VF structure for VF to be configured
 *
 * Before a VF can be used, and after a VF reset, the PF must configure
 * the VF's interrupt allocation registers. This includes allocating
 * interrupts from the PF's interrupt pool to the VF using the
 * VPINT_ALLOC(_PCI) registers, and setting up a mapping from PF vectors
 * to VF vectors in GLINT_VECT2FUNC.
 *
 * As well, this sets up queue allocation registers and maps the mailbox
 * interrupt for the VF.
 */
static void
ice_iov_setup_intr_mapping(struct ice_softc *sc, struct ice_vf *vf)
{
	struct ice_hw *hw = &sc->hw;
	struct ice_vsi *vsi = vf->vsi;
	u16 v;

	/* Calculate indices for register ops below */
	u16 vf_first_irq_idx = vf->vf_imap[0];
	u16 vf_last_irq_idx = (vf_first_irq_idx + vf->num_irq_vectors) - 1;
	u16 abs_vf_first_irq_idx = hw->func_caps.common_cap.msix_vector_first_id +
	    vf_first_irq_idx;
	u16 abs_vf_last_irq_idx = (abs_vf_first_irq_idx + vf->num_irq_vectors) - 1;
	u16 abs_vf_num = vf->vf_num + hw->func_caps.vf_base_id;

	/* Map out VF interrupt allocation in global device space. Both
	 * VPINT_ALLOC and VPINT_ALLOC_PCI use the same values.
	 */
	wr32(hw, VPINT_ALLOC(vf->vf_num),
	    (((abs_vf_first_irq_idx << VPINT_ALLOC_FIRST_S) & VPINT_ALLOC_FIRST_M) |
	    ((abs_vf_last_irq_idx << VPINT_ALLOC_LAST_S) & VPINT_ALLOC_LAST_M) |
	    VPINT_ALLOC_VALID_M));
	wr32(hw, VPINT_ALLOC_PCI(vf->vf_num),
	    (((abs_vf_first_irq_idx << VPINT_ALLOC_PCI_FIRST_S) & VPINT_ALLOC_PCI_FIRST_M) |
	    ((abs_vf_last_irq_idx << VPINT_ALLOC_PCI_LAST_S) & VPINT_ALLOC_PCI_LAST_M) |
	    VPINT_ALLOC_PCI_VALID_M));

	/* Create inverse mapping of vectors to PF/VF combinations */
	for (v = vf_first_irq_idx; v <= vf_last_irq_idx; v++)
	{
		wr32(hw, GLINT_VECT2FUNC(v),
		    (((abs_vf_num << GLINT_VECT2FUNC_VF_NUM_S) & GLINT_VECT2FUNC_VF_NUM_M) |
		     ((hw->pf_id << GLINT_VECT2FUNC_PF_NUM_S) & GLINT_VECT2FUNC_PF_NUM_M)));
	}

	/* Map mailbox interrupt to MSI-X index 0. Disable ITR for it, too. */
	wr32(hw, VPINT_MBX_CTL(abs_vf_num),
	    ((0 << VPINT_MBX_CTL_MSIX_INDX_S) & VPINT_MBX_CTL_MSIX_INDX_M) |
	    ((0x3 << VPINT_MBX_CTL_ITR_INDX_S) & VPINT_MBX_CTL_ITR_INDX_M) |
	    VPINT_MBX_CTL_CAUSE_ENA_M);

	/* Mark the TX queue mapping registers as valid */
	wr32(hw, VPLAN_TXQ_MAPENA(vf->vf_num), VPLAN_TXQ_MAPENA_TX_ENA_M);

	/* Indicate to HW that VF has scattered queue allocation */
	wr32(hw, VPLAN_TX_QBASE(vf->vf_num), VPLAN_TX_QBASE_VFQTABLE_ENA_M);
	for (int i = 0; i < vsi->num_tx_queues; i++) {
		wr32(hw, VPLAN_TX_QTABLE(i, vf->vf_num),
		    (vsi->tx_qmap[i] << VPLAN_TX_QTABLE_QINDEX_S) & VPLAN_TX_QTABLE_QINDEX_M);
	}

	/* Mark the RX queue mapping registers as valid */
	wr32(hw, VPLAN_RXQ_MAPENA(vf->vf_num), VPLAN_RXQ_MAPENA_RX_ENA_M);
	wr32(hw, VPLAN_RX_QBASE(vf->vf_num), VPLAN_RX_QBASE_VFQTABLE_ENA_M);
	for (int i = 0; i < vsi->num_rx_queues; i++) {
		wr32(hw, VPLAN_RX_QTABLE(i, vf->vf_num),
		    (vsi->rx_qmap[i] << VPLAN_RX_QTABLE_QINDEX_S) & VPLAN_RX_QTABLE_QINDEX_M);
	}
}

/**
 * ice_err_to_virt err - translate ice errors into virtchnl errors
 * @ice_err: status returned from ice function
 */
static enum virtchnl_status_code
ice_iov_err_to_virt_err(int ice_err)
{
	switch (ice_err) {
	case 0:
		return VIRTCHNL_STATUS_SUCCESS;
	case ICE_ERR_BAD_PTR:
	case ICE_ERR_INVAL_SIZE:
	case ICE_ERR_DEVICE_NOT_SUPPORTED:
	case ICE_ERR_PARAM:
	case ICE_ERR_CFG:
		return VIRTCHNL_STATUS_ERR_PARAM;
	case ICE_ERR_NO_MEMORY:
		return VIRTCHNL_STATUS_ERR_NO_MEMORY;
	case ICE_ERR_NOT_READY:
	case ICE_ERR_RESET_FAILED:
	case ICE_ERR_FW_API_VER:
	case ICE_ERR_AQ_ERROR:
	case ICE_ERR_AQ_TIMEOUT:
	case ICE_ERR_AQ_FULL:
	case ICE_ERR_AQ_NO_WORK:
	case ICE_ERR_AQ_EMPTY:
		return VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
	default:
		return VIRTCHNL_STATUS_ERR_NOT_SUPPORTED;
	}
}
