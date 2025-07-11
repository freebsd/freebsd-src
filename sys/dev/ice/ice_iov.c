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

static struct ice_vf *ice_iov_get_vf(struct ice_softc *sc, int vf_num);
static void ice_iov_ready_vf(struct ice_softc *sc, struct ice_vf *vf);
static void ice_reset_vf(struct ice_softc *sc, struct ice_vf *vf,
			 bool trigger_vflr);
static void ice_iov_setup_intr_mapping(struct ice_softc *sc, struct ice_vf *vf);

static void ice_vc_version_msg(struct ice_softc *sc, struct ice_vf *vf,
			       u8 *msg_buf);
static void ice_vc_get_vf_res_msg(struct ice_softc *sc, struct ice_vf *vf,
				  u8 *msg_buf);
static void ice_vc_add_eth_addr_msg(struct ice_softc *sc, struct ice_vf *vf,
				    u8 *msg_buf);
static void ice_vc_del_eth_addr_msg(struct ice_softc *sc, struct ice_vf *vf,
				    u8 *msg_buf);
static bool ice_vc_isvalid_ring_len(u16 ring_len);
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
static enum virtchnl_status_code ice_iov_err_to_virt_err(int ice_err);
static int ice_vf_validate_mac(struct ice_vf *vf, const uint8_t *addr);

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
	} else
		ice_set_bit(ICE_FEATURE_SRIOV, sc->feat_en);

	return (error);
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
	for (int i = 0; i < num_vfs; i++)
		sc->vfs[i].vf_num = i;

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
	vf->vf_flags = VF_FLAG_ENABLED;

	/* This VF needs at least one VSI */
	vsi = ice_alloc_vsi(sc, ICE_VSI_VF);
	if (vsi == NULL)
		return (ENOMEM);
	vf->vsi = vsi;
	vsi->vf_num = vfnum;

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

	/* Assign Tx queues from PF space */
	error = ice_resmgr_assign_scattered(&sc->tx_qmgr, vsi->tx_qmap,
					     vsi->num_tx_queues);
	if (error) {
		device_printf(sc->dev, "Unable to assign VF Tx queues: %s\n",
			      ice_err_str(error));
		goto release_vsi;
	}

	/* Assign Rx queues from PF space */
	error = ice_resmgr_assign_scattered(&sc->rx_qmgr, vsi->rx_qmap,
					     vsi->num_rx_queues);
	if (error) {
		device_printf(sc->dev, "Unable to assign VF Rx queues: %s\n",
			      ice_err_str(error));
		goto release_vsi;
	}

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

	/* Assign VF interrupts from PF space */
	if (!(vf->vf_imap =
	      (u16 *)malloc(sizeof(u16) * vf->num_irq_vectors,
	      M_ICE, M_NOWAIT))) {
		device_printf(dev, "Unable to allocate VF-%d imap memory\n", vfnum);
		error = ENOMEM;
		goto free_rxirqvs;
	}
	error = ice_resmgr_assign_contiguous(&sc->dev_imgr, vf->vf_imap, vf->num_irq_vectors);
	if (error) {
		device_printf(dev, "Unable to assign VF-%d interrupt mapping: %s\n",
			      vfnum, ice_err_str(error));
		goto free_imap;
	}

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

	vf->vf_flags |= VF_FLAG_VLAN_CAP;

	/* Create and setup VSI in HW */
	error = ice_initialize_vsi(vsi);
	if (error) {
		device_printf(sc->dev, "Unable to initialize VF %d VSI: %s\n",
			      vfnum, ice_err_str(error));
		goto release_imap;
	}

	/* Add the broadcast address */
	error = ice_add_vsi_mac_filter(vsi, broadcastaddr);
	if (error) {
		device_printf(sc->dev, "Unable to add broadcast filter VF %d VSI: %s\n",
			      vfnum, ice_err_str(error));
		goto release_imap;
	}

	ice_iov_ready_vf(sc, vf);

	return (0);

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
	ice_release_vsi(vsi);
	vf->vsi = NULL;
	return (error);
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
		vsi = vf->vsi;

		/* Free VF interrupt reservation */
		if (vf->vf_imap) {
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

		ice_release_vsi(vsi);
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
	u32 reg, reg_idx, bit_idx;

	for (int i = 0; i < sc->num_vfs; i++) {
		vf = &sc->vfs[i];

		reg_idx = (hw->func_caps.vf_base_id + vf->vf_num) / 32;
		bit_idx = (hw->func_caps.vf_base_id + vf->vf_num) % 32;
		reg = rd32(hw, GLGEN_VFLRSTAT(reg_idx));
		if (reg & BIT(bit_idx))
			ice_reset_vf(sc, vf, false);
	}
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
	struct ice_hw *hw = &sc->hw;
	u32 reg;

	/* Clear the triggering bit */
	reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_num));
	reg &= ~VPGEN_VFRTRIG_VFSWR_M;
	wr32(hw, VPGEN_VFRTRIG(vf->vf_num), reg);

	/* Setup VF interrupt allocation and mapping */
	ice_iov_setup_intr_mapping(sc, vf);

	/* Indicate to the VF that reset is done */
	wr32(hw, VFGEN_RSTAT(vf->vf_num), VIRTCHNL_VFR_VFACTIVE);

	ice_flush(hw);
}

/**
 * ice_reset_vf - Perform a hardware reset (VFR) on a VF
 * @sc: device softc structure
 * @vf: driver's VF structure for VF to be reset
 * @trigger_vflr: trigger a reset or only handle already executed reset
 *
 * Performs a VFR for the given VF. This function busy waits until the
 * reset completes in the HW, notifies the VF that the reset is done
 * by setting a bit in a HW register, then returns.
 *
 * @remark This also sets up the PF<->VF interrupt mapping and allocations in
 * the hardware after the hardware reset is finished, via
 * ice_iov_setup_intr_mapping()
 */
static void
ice_reset_vf(struct ice_softc *sc, struct ice_vf *vf, bool trigger_vflr)
{
	u16 global_vf_num, reg_idx, bit_idx;
	struct ice_hw *hw = &sc->hw;
	int status;
	u32 reg;
	int i;

	global_vf_num = vf->vf_num + hw->func_caps.vf_base_id;

	if (trigger_vflr) {
		reg = rd32(hw, VPGEN_VFRTRIG(vf->vf_num));
		reg |= VPGEN_VFRTRIG_VFSWR_M;
		wr32(hw, VPGEN_VFRTRIG(vf->vf_num), reg);
	}

	/* clear the VFLR bit for the VF in a GLGEN_VFLRSTAT register */
	reg_idx = (global_vf_num) / 32;
	bit_idx = (global_vf_num) % 32;
	wr32(hw, GLGEN_VFLRSTAT(reg_idx), BIT(bit_idx));
	ice_flush(hw);

	/* Wait until there are no pending PCI transactions */
	wr32(hw, PF_PCI_CIAA,
	     ICE_PCIE_DEV_STATUS | (global_vf_num << PF_PCI_CIAA_VF_NUM_S));

	for (i = 0; i < ICE_PCI_CIAD_WAIT_COUNT; i++) {
		reg = rd32(hw, PF_PCI_CIAD);
		if (!(reg & PCIEM_STA_TRANSACTION_PND))
			break;

		DELAY(ICE_PCI_CIAD_WAIT_DELAY_US);
	}
	if (i == ICE_PCI_CIAD_WAIT_COUNT)
		device_printf(sc->dev,
			"VF-%d PCI transactions stuck\n", vf->vf_num);

	/* Disable TX queues, which is required during VF reset */
	status = ice_dis_vsi_txq(hw->port_info, vf->vsi->idx, 0, 0, NULL, NULL,
			NULL, ICE_VF_RESET, vf->vf_num, NULL);
	if (status)
		device_printf(sc->dev,
			      "%s: Failed to disable LAN Tx queues: err %s aq_err %s\n",
			      __func__, ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));

	/* Then check for the VF reset to finish in HW */
	for (i = 0; i < ICE_VPGEN_VFRSTAT_WAIT_COUNT; i++) {
		reg = rd32(hw, VPGEN_VFRSTAT(vf->vf_num));
		if ((reg & VPGEN_VFRSTAT_VFRD_M))
			break;

		DELAY(ICE_VPGEN_VFRSTAT_WAIT_DELAY_US);
	}
	if (i == ICE_VPGEN_VFRSTAT_WAIT_COUNT)
		device_printf(sc->dev,
			"VF-%d Reset is stuck\n", vf->vf_num);

	ice_iov_ready_vf(sc, vf);
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

	/* XXX: Only support one VSI per VF, so this size doesn't need adjusting */
	vf_res_len = sizeof(struct virtchnl_vf_resource);
	vf_res = (struct virtchnl_vf_resource *)malloc(vf_res_len, M_ICE,
	    M_WAITOK | M_ZERO);

	vf_res->num_vsis = 1;
	vf_res->num_queue_pairs = vf->vsi->num_tx_queues;
	vf_res->max_vectors = vf_res->num_queue_pairs + 1;

	vf_res->rss_key_size = ICE_GET_SET_RSS_KEY_EXTEND_KEY_SIZE;
	vf_res->rss_lut_size = ICE_VSIQF_HLUT_ARRAY_SIZE;
	vf_res->max_mtu = 0;

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

	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_GET_VF_RESOURCES,
	    VIRTCHNL_STATUS_SUCCESS, (u8 *)vf_res, vf_res_len, NULL);

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
	u16 added_addr_cnt = 0;
	int error = 0;

	addr_list = (struct virtchnl_ether_addr_list *)msg_buf;

	if (addr_list->num_elements >
	    (vf->mac_filter_limit - vf->mac_filter_cnt)) {
		v_status = VIRTCHNL_STATUS_ERR_NO_MEMORY;
		goto done;
	}

	for (int i = 0; i < addr_list->num_elements; i++) {
		u8 *addr = addr_list->list[i].addr;

		/* The type flag is currently ignored; every MAC address is
		 * treated as the LEGACY type
		 */

		error = ice_vf_validate_mac(vf, addr);
		if (error == EPERM) {
			device_printf(sc->dev,
			    "%s: VF-%d: Not permitted to add MAC addr for VSI %d\n",
			    __func__, vf->vf_num, vf->vsi->idx);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			continue;
		} else if (error) {
			device_printf(sc->dev,
			    "%s: VF-%d: Did not add invalid MAC addr for VSI %d\n",
			    __func__, vf->vf_num, vf->vsi->idx);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			continue;
		}

		error = ice_add_vsi_mac_filter(vf->vsi, addr);
		if (error) {
			device_printf(sc->dev,
			    "%s: VF-%d: Error adding MAC addr for VSI %d\n",
			    __func__, vf->vf_num, vf->vsi->idx);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			continue;
		}
		/* Don't count VF's MAC against its MAC filter limit */
		if (memcmp(addr, vf->mac, ETHER_ADDR_LEN))
			added_addr_cnt++;
	}

	vf->mac_filter_cnt += added_addr_cnt;

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
	u16 deleted_addr_cnt = 0;
	int error = 0;

	addr_list = (struct virtchnl_ether_addr_list *)msg_buf;

	for (int i = 0; i < addr_list->num_elements; i++) {
		error = ice_remove_vsi_mac_filter(vf->vsi, addr_list->list[i].addr);
		if (error) {
			device_printf(sc->dev,
			    "%s: VF-%d: Error removing MAC addr for VSI %d\n",
			    __func__, vf->vf_num, vf->vsi->idx);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			continue;
		}
		/* Don't count VF's MAC against its MAC filter limit */
		if (memcmp(addr_list->list[i].addr, vf->mac, ETHER_ADDR_LEN))
			deleted_addr_cnt++;
	}

	if (deleted_addr_cnt >= vf->mac_filter_cnt)
		vf->mac_filter_cnt = 0;
	else
		vf->mac_filter_cnt -= deleted_addr_cnt;

	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_DEL_ETH_ADDR,
	    v_status, NULL, 0, NULL);
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

	if (vlan_list->num_elements > (vf->vlan_limit - vf->vlan_cnt)) {
		v_status = VIRTCHNL_STATUS_ERR_NO_MEMORY;
		goto done;
	}

	status = ice_add_vlan_hw_filters(vsi, vlan_list->vlan_id,
					vlan_list->num_elements);
	if (status) {
		device_printf(sc->dev,
			      "VF-%d: Failure adding VLANs to VSI %d, err %s aq_err %s\n",
			      vf->vf_num, vsi->idx, ice_status_str(status),
			      ice_aq_str(sc->hw.adminq.sq_last_status));
		v_status = ice_iov_err_to_virt_err(status);
		goto done;
	}

	vf->vlan_cnt += vlan_list->num_elements;

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

	status = ice_remove_vlan_hw_filters(vsi, vlan_list->vlan_id,
					vlan_list->num_elements);
	if (status) {
		device_printf(sc->dev,
			      "VF-%d: Failure deleting VLANs from VSI %d, err %s aq_err %s\n",
			      vf->vf_num, vsi->idx, ice_status_str(status),
			      ice_aq_str(sc->hw.adminq.sq_last_status));
		v_status = ice_iov_err_to_virt_err(status);
		goto done;
	}

	if (vlan_list->num_elements >= vf->vlan_cnt)
		vf->vlan_cnt = 0;
	else
		vf->vlan_cnt -= vlan_list->num_elements;

done:
	ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_DEL_VLAN,
	    v_status, NULL, 0, NULL);
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
ice_vc_isvalid_ring_len(u16 ring_len)
{
	return (ring_len >= ICE_MIN_DESC_COUNT &&
		ring_len <= ICE_MAX_DESC_COUNT &&
		!(ring_len % ICE_DESC_COUNT_INCR));
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
	int i, error = 0;

	vqci = (struct virtchnl_vsi_queue_config_info *)msg_buf;

	if (vqci->num_queue_pairs > vf->vsi->num_tx_queues &&
	    vqci->num_queue_pairs > vf->vsi->num_rx_queues) {
		status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	ice_vsi_disable_tx(vf->vsi);
	ice_control_all_rx_queues(vf->vsi, false);

	/*
	 * Clear TX and RX queues config in case VF
	 * requests different number of queues.
	 */
	for (i = 0; i < vsi->num_tx_queues; i++) {
		txq = &vsi->tx_queues[i];

		txq->desc_count = 0;
		txq->tx_paddr = 0;
		txq->tc = 0;
	}

	for (i = 0; i < vsi->num_rx_queues; i++) {
		rxq = &vsi->rx_queues[i];

		rxq->desc_count = 0;
		rxq->rx_paddr = 0;
	}

	vqpi = vqci->qpair;
	for (i = 0; i < vqci->num_queue_pairs; i++, vqpi++) {
		/* Initial parameter validation */
		if (vqpi->txq.vsi_id != vf->vsi->idx ||
		    vqpi->rxq.vsi_id != vf->vsi->idx ||
		    vqpi->txq.queue_id != vqpi->rxq.queue_id ||
		    vqpi->txq.headwb_enabled ||
		    vqpi->rxq.splithdr_enabled ||
		    vqpi->rxq.crc_disable ||
		    !(ice_vc_isvalid_ring_len(vqpi->txq.ring_len)) ||
		    !(ice_vc_isvalid_ring_len(vqpi->rxq.ring_len))) {
			status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}

		/* Copy parameters into VF's queue/VSI structs */
		txq = &vsi->tx_queues[vqpi->txq.queue_id];

		txq->desc_count = vqpi->txq.ring_len;
		txq->tx_paddr = vqpi->txq.dma_ring_addr;
		txq->q_handle = vqpi->txq.queue_id;
		txq->tc = 0;

		rxq = &vsi->rx_queues[vqpi->rxq.queue_id];

		rxq->desc_count = vqpi->rxq.ring_len;
		rxq->rx_paddr = vqpi->rxq.dma_ring_addr;
		vsi->mbuf_sz = vqpi->rxq.databuffer_size;
	}

	/* Configure TX queues in HW */
	error = ice_cfg_vsi_for_tx(vsi);
	if (error) {
		device_printf(dev,
			      "VF-%d: Unable to configure VSI for Tx: %s\n",
			      vf->vf_num, ice_err_str(error));
		status = VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
		goto done;
	}

	/* Configure RX queues in HW */
	error = ice_cfg_vsi_for_rx(vsi);
	if (error) {
		device_printf(dev,
			      "VF-%d: Unable to configure VSI for Rx: %s\n",
			      vf->vf_num, ice_err_str(error));
		status = VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
		ice_vsi_disable_tx(vsi);
		goto done;
	}

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

	if (vrk->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
		    vf->vf_num, vsi->idx, vrk->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	if ((vrk->key_len >
	   (ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE +
	    ICE_AQC_GET_SET_RSS_KEY_DATA_HASH_KEY_SIZE)) ||
	    vrk->key_len == 0) {
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
	int status = 0;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_aq_get_set_rss_lut_params lut_params = {};
	struct ice_vsi *vsi = vf->vsi;

	vrl = (struct virtchnl_rss_lut *)msg_buf;

	if (vrl->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
		    vf->vf_num, vsi->idx, vrl->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	if (vrl->lut_entries > ICE_VSIQF_HLUT_ARRAY_SIZE) {
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	lut_params.vsi_handle = vsi->idx;
	lut_params.lut_size = vsi->rss_table_size;
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
	int bit, error = 0;

	vqs = (struct virtchnl_queue_select *)msg_buf;

	if (vqs->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "%s: VF-%d: Message has invalid VSI ID (expected %d, got %d)\n",
		    __func__, vf->vf_num, vsi->idx, vqs->vsi_id);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	if (!vqs->rx_queues && !vqs->tx_queues) {
		device_printf(sc->dev,
		    "%s: VF-%d: message queue masks are empty\n",
		    __func__, vf->vf_num);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	/* Validate rx_queue mask */
	bit = fls(vqs->rx_queues);
	if (bit > vsi->num_rx_queues) {
		device_printf(sc->dev,
		    "%s: VF-%d: message's rx_queues map (0x%08x) has invalid bit set (%d)\n",
		    __func__, vf->vf_num, vqs->rx_queues, bit);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	/* Tx ring enable is handled in an earlier message. */
	for_each_set_bit(bit, &vqs->rx_queues, 32) {
		error = ice_control_rx_queue(vsi, bit, true);
		if (error) {
			device_printf(sc->dev,
				      "Unable to enable Rx ring %d for receive: %s\n",
				      bit, ice_err_str(error));
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}
	}

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
 * Disables all VF queues for the VF's VSI.
 *
 * @remark Unlike the ENABLE_QUEUES handler, this operates on both
 * Tx and Rx queues
 */
static void
ice_vc_disable_queues_msg(struct ice_softc *sc, struct ice_vf *vf,
			  u8 *msg_buf __unused)
{
	struct ice_hw *hw = &sc->hw;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;
	int error = 0;

	error = ice_control_all_rx_queues(vsi, false);
	if (error) {
		device_printf(sc->dev,
			      "Unable to disable Rx rings for transmit: %s\n",
			      ice_err_str(error));
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	error = ice_vsi_disable_tx(vsi);
	if (error) {
		/* Already prints an error message */
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
	}

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
#define ICE_VIRTCHNL_QUEUE_MAP_SIZE	16
	struct ice_hw *hw = &sc->hw;
	struct virtchnl_irq_map_info *vimi;
	struct virtchnl_vector_map *vvm;
	enum virtchnl_status_code v_status = VIRTCHNL_STATUS_SUCCESS;
	struct ice_vsi *vsi = vf->vsi;
	u16 vector;

	vimi = (struct virtchnl_irq_map_info *)msg_buf;

	if (vimi->num_vectors > vf->num_irq_vectors) {
		device_printf(sc->dev,
		    "%s: VF-%d: message has more vectors (%d) than configured for VF (%d)\n",
		    __func__, vf->vf_num, vimi->num_vectors, vf->num_irq_vectors);
		v_status = VIRTCHNL_STATUS_ERR_PARAM;
		goto done;
	}

	vvm = vimi->vecmap;
	/* Save off information from message */
	for (int i = 0; i < vimi->num_vectors; i++, vvm++) {
		struct ice_tx_queue *txq;
		struct ice_rx_queue *rxq;
		int bit;

		if (vvm->vsi_id != vf->vsi->idx) {
			device_printf(sc->dev,
			    "%s: VF-%d: message's VSI ID (%d) does not match VF's (%d) for vector %d\n",
			    __func__, vf->vf_num, vvm->vsi_id, vf->vsi->idx, i);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}

		/* vvm->vector_id is relative to VF space */
		vector = vvm->vector_id;

		if (vector >= vf->num_irq_vectors) {
			device_printf(sc->dev,
			    "%s: VF-%d: message's vector ID (%d) is greater than VF's max ID (%d)\n",
			    __func__, vf->vf_num, vector, vf->num_irq_vectors - 1);
			v_status = VIRTCHNL_STATUS_ERR_PARAM;
			goto done;
		}

		/* The Misc/Admin Queue vector doesn't need mapping */
		if (vector == 0)
			continue;

		/* coverity[address_of] */
		for_each_set_bit(bit, &vvm->txq_map, ICE_VIRTCHNL_QUEUE_MAP_SIZE) {
			if (bit >= vsi->num_tx_queues) {
				device_printf(sc->dev,
				    "%s: VF-%d: txq map has invalid bit set\n",
				    __func__, vf->vf_num);
				v_status = VIRTCHNL_STATUS_ERR_PARAM;
				goto done;
			}

			vf->tx_irqvs[vector].me = vector;

			txq = &vsi->tx_queues[bit];
			txq->irqv = &vf->tx_irqvs[vector];
			txq->itr_idx = vvm->txitr_idx;
		}
		/* coverity[address_of] */
		for_each_set_bit(bit, &vvm->rxq_map, ICE_VIRTCHNL_QUEUE_MAP_SIZE) {
			if (bit >= vsi->num_rx_queues) {
				device_printf(sc->dev,
				    "%s: VF-%d: rxq map has invalid bit set\n",
				    __func__, vf->vf_num);
				v_status = VIRTCHNL_STATUS_ERR_PARAM;
				goto done;
			}
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

	if (vqs->vsi_id != vsi->idx) {
		device_printf(sc->dev,
		    "%s: VF-%d: message has invalid VSI ID %d (VF has VSI ID %d)\n",
		    __func__, vf->vf_num, vqs->vsi_id, vsi->idx);
		ice_aq_send_msg_to_vf(hw, vf->vf_num, VIRTCHNL_OP_GET_STATS,
		    VIRTCHNL_STATUS_ERR_PARAM, NULL, 0, NULL);
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
 * ice_vc_handle_vf_msg - Handle a message from a VF
 * @sc: device private structure
 * @event: event received from the HW MBX queue
 *
 * Called whenever an event is received from a VF on the HW mailbox queue.
 * Responsible for handling these messages as well as responding to the
 * VF afterwards, depending on the received message type.
 */
void
ice_vc_handle_vf_msg(struct ice_softc *sc, struct ice_rq_event_info *event)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct ice_vf *vf;
	int err = 0;

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

	/* Perform basic checks on the msg */
	err = virtchnl_vc_validate_vf_msg(&vf->version, v_opcode, msg, msglen);
	if (err) {
		device_printf(dev, "%s: Received invalid msg from VF-%d: opcode %d, len %d, error %d\n",
		    __func__, vf->vf_num, v_opcode, msglen, err);
		ice_aq_send_msg_to_vf(hw, v_id, v_opcode, VIRTCHNL_STATUS_ERR_PARAM, NULL, 0, NULL);
		return;
	}

	switch (v_opcode) {
	case VIRTCHNL_OP_VERSION:
		ice_vc_version_msg(sc, vf, msg);
		break;
	case VIRTCHNL_OP_RESET_VF:
		ice_reset_vf(sc, vf, true);
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
