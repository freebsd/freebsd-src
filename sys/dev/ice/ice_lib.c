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
/*$FreeBSD$*/

/**
 * @file ice_lib.c
 * @brief Generic device setup and sysctl functions
 *
 * Library of generic device functions not specific to the networking stack.
 *
 * This includes hardware initialization functions, as well as handlers for
 * many of the device sysctls used to probe driver status or tune specific
 * behaviors.
 */

#include "ice_lib.h"
#include "ice_iflib.h"
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <machine/resource.h>
#include <net/if_dl.h>
#include <sys/firmware.h>
#include <sys/priv.h>
#include <sys/limits.h>

/**
 * @var M_ICE
 * @brief main ice driver allocation type
 *
 * malloc(9) allocation type used by the majority of memory allocations in the
 * ice driver.
 */
MALLOC_DEFINE(M_ICE, "ice", "Intel(R) 100Gb Network Driver lib allocations");

/*
 * Helper function prototypes
 */
static int ice_get_next_vsi(struct ice_vsi **all_vsi, int size);
static void ice_set_default_vsi_ctx(struct ice_vsi_ctx *ctx);
static void ice_set_rss_vsi_ctx(struct ice_vsi_ctx *ctx, enum ice_vsi_type type);
static int ice_setup_vsi_qmap(struct ice_vsi *vsi, struct ice_vsi_ctx *ctx);
static int ice_setup_tx_ctx(struct ice_tx_queue *txq,
			    struct ice_tlan_ctx *tlan_ctx, u16 pf_q);
static int ice_setup_rx_ctx(struct ice_rx_queue *rxq);
static int ice_is_rxq_ready(struct ice_hw *hw, int pf_q, u32 *reg);
static void ice_free_fltr_list(struct ice_list_head *list);
static int ice_add_mac_to_list(struct ice_vsi *vsi, struct ice_list_head *list,
			       const u8 *addr, enum ice_sw_fwd_act_type action);
static void ice_check_ctrlq_errors(struct ice_softc *sc, const char *qname,
				   struct ice_ctl_q_info *cq);
static void ice_process_link_event(struct ice_softc *sc, struct ice_rq_event_info *e);
static void ice_process_ctrlq_event(struct ice_softc *sc, const char *qname,
				    struct ice_rq_event_info *event);
static void ice_nvm_version_str(struct ice_hw *hw, struct sbuf *buf);
static void ice_active_pkg_version_str(struct ice_hw *hw, struct sbuf *buf);
static void ice_os_pkg_version_str(struct ice_hw *hw, struct sbuf *buf);
static bool ice_filter_is_mcast(struct ice_vsi *vsi, struct ice_fltr_info *info);
static u_int ice_sync_one_mcast_filter(void *p, struct sockaddr_dl *sdl, u_int errors);
static void ice_add_debug_tunables(struct ice_softc *sc);
static void ice_add_debug_sysctls(struct ice_softc *sc);
static void ice_vsi_set_rss_params(struct ice_vsi *vsi);
static void ice_get_default_rss_key(u8 *seed);
static int  ice_set_rss_key(struct ice_vsi *vsi);
static int  ice_set_rss_lut(struct ice_vsi *vsi);
static void ice_set_rss_flow_flds(struct ice_vsi *vsi);
static void ice_clean_vsi_rss_cfg(struct ice_vsi *vsi);
static const char *ice_aq_speed_to_str(struct ice_port_info *pi);
static const char *ice_requested_fec_mode(struct ice_port_info *pi);
static const char *ice_negotiated_fec_mode(struct ice_port_info *pi);
static const char *ice_autoneg_mode(struct ice_port_info *pi);
static const char *ice_flowcontrol_mode(struct ice_port_info *pi);
static void ice_print_bus_link_data(device_t dev, struct ice_hw *hw);
static void ice_set_pci_link_status_data(struct ice_hw *hw, u16 link_status);
static uint8_t ice_pcie_bandwidth_check(struct ice_softc *sc);
static uint64_t ice_pcie_bus_speed_to_rate(enum ice_pcie_bus_speed speed);
static int ice_pcie_lnk_width_to_int(enum ice_pcie_link_width width);
static uint64_t ice_phy_types_to_max_rate(struct ice_port_info *pi);
static void ice_add_sysctls_sw_stats(struct ice_vsi *vsi,
				     struct sysctl_ctx_list *ctx,
				     struct sysctl_oid *parent);
static void
ice_add_sysctls_mac_pfc_one_stat(struct sysctl_ctx_list *ctx,
				 struct sysctl_oid_list *parent_list,
				 u64* pfc_stat_location,
				 const char *node_name,
				 const char *descr);
static void ice_add_sysctls_mac_pfc_stats(struct sysctl_ctx_list *ctx,
					  struct sysctl_oid *parent,
					  struct ice_hw_port_stats *stats);
static void ice_setup_vsi_common(struct ice_softc *sc, struct ice_vsi *vsi,
				 enum ice_vsi_type type, int idx,
				 bool dynamic);
static void ice_handle_mib_change_event(struct ice_softc *sc,
				 struct ice_rq_event_info *event);
static void
ice_handle_lan_overflow_event(struct ice_softc *sc,
			      struct ice_rq_event_info *event);
static int ice_add_ethertype_to_list(struct ice_vsi *vsi,
				     struct ice_list_head *list,
				     u16 ethertype, u16 direction,
				     enum ice_sw_fwd_act_type action);
static void ice_add_rx_lldp_filter(struct ice_softc *sc);
static void ice_del_rx_lldp_filter(struct ice_softc *sc);
static u16 ice_aq_phy_types_to_link_speeds(u64 phy_type_low,
					   u64 phy_type_high);
struct ice_phy_data;
static int
ice_intersect_phy_types_and_speeds(struct ice_softc *sc,
				   struct ice_phy_data *phy_data);
static int
ice_apply_saved_phy_req_to_cfg(struct ice_softc *sc,
			       struct ice_aqc_set_phy_cfg_data *cfg);
static int
ice_apply_saved_fec_req_to_cfg(struct ice_softc *sc,
			       struct ice_aqc_set_phy_cfg_data *cfg);
static void
ice_apply_saved_fc_req_to_cfg(struct ice_port_info *pi,
			      struct ice_aqc_set_phy_cfg_data *cfg);
static void
ice_print_ldo_tlv(struct ice_softc *sc,
		  struct ice_link_default_override_tlv *tlv);
static void
ice_sysctl_speeds_to_aq_phy_types(u16 sysctl_speeds, u64 *phy_type_low,
				  u64 *phy_type_high);
static u16 ice_apply_supported_speed_filter(u16 report_speeds, u8 mod_type);
static void
ice_handle_health_status_event(struct ice_softc *sc,
			       struct ice_rq_event_info *event);
static void
ice_print_health_status_string(device_t dev,
			       struct ice_aqc_health_status_elem *elem);
static void
ice_debug_print_mib_change_event(struct ice_softc *sc,
				 struct ice_rq_event_info *event);
static bool ice_check_ets_bw(u8 *table);
static u8 ice_dcb_get_num_tc(struct ice_dcbx_cfg *dcbcfg);
static bool
ice_dcb_needs_reconfig(struct ice_softc *sc, struct ice_dcbx_cfg *old_cfg,
		       struct ice_dcbx_cfg *new_cfg);
static void ice_dcb_recfg(struct ice_softc *sc);
static u8 ice_dcb_tc_contig(u8 tc_map);
static int ice_ets_str_to_tbl(const char *str, u8 *table, u8 limit);
static int ice_pf_vsi_cfg_tc(struct ice_softc *sc, u8 tc_map);
static void ice_sbuf_print_ets_cfg(struct sbuf *sbuf, const char *name,
				   struct ice_dcb_ets_cfg *ets);
static void ice_stop_pf_vsi(struct ice_softc *sc);
static void ice_vsi_setup_q_map(struct ice_vsi *vsi, struct ice_vsi_ctx *ctxt);
static void ice_do_dcb_reconfig(struct ice_softc *sc, bool pending_mib);
static int ice_config_pfc(struct ice_softc *sc, u8 new_mode);
void
ice_add_dscp2tc_map_sysctls(struct ice_softc *sc,
			    struct sysctl_ctx_list *ctx,
			    struct sysctl_oid_list *ctx_list);
static void ice_set_default_local_mib_settings(struct ice_softc *sc);
static bool ice_dscp_is_mapped(struct ice_dcbx_cfg *dcbcfg);
static void ice_start_dcbx_agent(struct ice_softc *sc);
static void ice_fw_debug_dump_print_cluster(struct ice_softc *sc,
					    struct sbuf *sbuf, u16 cluster_id);

static int ice_module_init(void);
static int ice_module_exit(void);

/*
 * package version comparison functions
 */
static bool pkg_ver_empty(struct ice_pkg_ver *pkg_ver, u8 *pkg_name);
static int pkg_ver_compatible(struct ice_pkg_ver *pkg_ver);

/*
 * dynamic sysctl handlers
 */
static int ice_sysctl_show_fw(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_pkg_version(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_os_pkg_version(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dump_mac_filters(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dump_vlan_filters(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dump_ethertype_filters(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dump_ethertype_mac_filters(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_current_speed(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_request_reset(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dump_state_flags(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fec_config(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fc_config(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_negotiated_fc(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_negotiated_fec(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_phy_type_low(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_phy_type_high(SYSCTL_HANDLER_ARGS);
static int __ice_sysctl_phy_type_handler(SYSCTL_HANDLER_ARGS,
					 bool is_phy_type_high);
static int ice_sysctl_advertise_speed(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_rx_itr(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_tx_itr(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fw_lldp_agent(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fw_cur_lldp_persist_status(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fw_dflt_lldp_persist_status(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_phy_caps(SYSCTL_HANDLER_ARGS, u8 report_mode);
static int ice_sysctl_phy_sw_caps(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_phy_nvm_caps(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_phy_topo_caps(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_phy_link_status(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_read_i2c_diag_data(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_tx_cso_stat(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_rx_cso_stat(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_pba_number(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_rx_errors_stat(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dump_dcbx_cfg(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dump_vsi_cfg(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_ets_min_rate(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_up2tc_map(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_pfc_config(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_query_port_ets(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_dscp2tc_map(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_pfc_mode(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fw_debug_dump_cluster_setting(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_fw_debug_dump_do_dump(SYSCTL_HANDLER_ARGS);
static int ice_sysctl_allow_no_fec_mod_in_auto(SYSCTL_HANDLER_ARGS);

/**
 * ice_map_bar - Map PCIe BAR memory
 * @dev: the PCIe device
 * @bar: the BAR info structure
 * @bar_num: PCIe BAR number
 *
 * Maps the specified PCIe BAR. Stores the mapping data in struct
 * ice_bar_info.
 */
int
ice_map_bar(device_t dev, struct ice_bar_info *bar, int bar_num)
{
	if (bar->res != NULL) {
		device_printf(dev, "PCI BAR%d already mapped\n", bar_num);
		return (EDOOFUS);
	}

	bar->rid = PCIR_BAR(bar_num);
	bar->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &bar->rid,
					  RF_ACTIVE);
	if (!bar->res) {
		device_printf(dev, "PCI BAR%d mapping failed\n", bar_num);
		return (ENXIO);
	}

	bar->tag = rman_get_bustag(bar->res);
	bar->handle = rman_get_bushandle(bar->res);
	bar->size = rman_get_size(bar->res);

	return (0);
}

/**
 * ice_free_bar - Free PCIe BAR memory
 * @dev: the PCIe device
 * @bar: the BAR info structure
 *
 * Frees the specified PCIe BAR, releasing its resources.
 */
void
ice_free_bar(device_t dev, struct ice_bar_info *bar)
{
	if (bar->res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, bar->rid, bar->res);
	bar->res = NULL;
}

/**
 * ice_set_ctrlq_len - Configure ctrlq lengths for a device
 * @hw: the device hardware structure
 *
 * Configures the control queues for the given device, setting up the
 * specified lengths, prior to initializing hardware.
 */
void
ice_set_ctrlq_len(struct ice_hw *hw)
{
	hw->adminq.num_rq_entries = ICE_AQ_LEN;
	hw->adminq.num_sq_entries = ICE_AQ_LEN;
	hw->adminq.rq_buf_size = ICE_AQ_MAX_BUF_LEN;
	hw->adminq.sq_buf_size = ICE_AQ_MAX_BUF_LEN;

	hw->mailboxq.num_rq_entries = ICE_MBXQ_LEN;
	hw->mailboxq.num_sq_entries = ICE_MBXQ_LEN;
	hw->mailboxq.rq_buf_size = ICE_MBXQ_MAX_BUF_LEN;
	hw->mailboxq.sq_buf_size = ICE_MBXQ_MAX_BUF_LEN;

}

/**
 * ice_get_next_vsi - Get the next available VSI slot
 * @all_vsi: the VSI list
 * @size: the size of the VSI list
 *
 * Returns the index to the first available VSI slot. Will return size (one
 * past the last index) if there are no slots available.
 */
static int
ice_get_next_vsi(struct ice_vsi **all_vsi, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (all_vsi[i] == NULL)
			return i;
	}

	return size;
}

/**
 * ice_setup_vsi_common - Common VSI setup for both dynamic and static VSIs
 * @sc: the device private softc structure
 * @vsi: the VSI to setup
 * @type: the VSI type of the new VSI
 * @idx: the index in the all_vsi array to use
 * @dynamic: whether this VSI memory was dynamically allocated
 *
 * Perform setup for a VSI that is common to both dynamically allocated VSIs
 * and the static PF VSI which is embedded in the softc structure.
 */
static void
ice_setup_vsi_common(struct ice_softc *sc, struct ice_vsi *vsi,
		     enum ice_vsi_type type, int idx, bool dynamic)
{
	/* Store important values in VSI struct */
	vsi->type = type;
	vsi->sc = sc;
	vsi->idx = idx;
	sc->all_vsi[idx] = vsi;
	vsi->dynamic = dynamic;

	/* Setup the VSI tunables now */
	ice_add_vsi_tunables(vsi, sc->vsi_sysctls);
}

/**
 * ice_alloc_vsi - Allocate a dynamic VSI
 * @sc: device softc structure
 * @type: VSI type
 *
 * Allocates a new dynamic VSI structure and inserts it into the VSI list.
 */
struct ice_vsi *
ice_alloc_vsi(struct ice_softc *sc, enum ice_vsi_type type)
{
	struct ice_vsi *vsi;
	int idx;

	/* Find an open index for a new VSI to be allocated. If the returned
	 * index is >= the num_available_vsi then it means no slot is
	 * available.
	 */
	idx = ice_get_next_vsi(sc->all_vsi, sc->num_available_vsi);
	if (idx >= sc->num_available_vsi) {
		device_printf(sc->dev, "No available VSI slots\n");
		return NULL;
	}

	vsi = (struct ice_vsi *)malloc(sizeof(*vsi), M_ICE, M_WAITOK|M_ZERO);
	if (!vsi) {
		device_printf(sc->dev, "Unable to allocate VSI memory\n");
		return NULL;
	}

	ice_setup_vsi_common(sc, vsi, type, idx, true);

	return vsi;
}

/**
 * ice_setup_pf_vsi - Setup the PF VSI
 * @sc: the device private softc
 *
 * Setup the PF VSI structure which is embedded as sc->pf_vsi in the device
 * private softc. Unlike other VSIs, the PF VSI memory is allocated as part of
 * the softc memory, instead of being dynamically allocated at creation.
 */
void
ice_setup_pf_vsi(struct ice_softc *sc)
{
	ice_setup_vsi_common(sc, &sc->pf_vsi, ICE_VSI_PF, 0, false);
}

/**
 * ice_alloc_vsi_qmap
 * @vsi: VSI structure
 * @max_tx_queues: Number of transmit queues to identify
 * @max_rx_queues: Number of receive queues to identify
 *
 * Allocates a max_[t|r]x_queues array of words for the VSI where each
 * word contains the index of the queue it represents.  In here, all
 * words are initialized to an index of ICE_INVALID_RES_IDX, indicating
 * all queues for this VSI are not yet assigned an index and thus,
 * not ready for use.
 *
 * Returns an error code on failure.
 */
int
ice_alloc_vsi_qmap(struct ice_vsi *vsi, const int max_tx_queues,
		   const int max_rx_queues)
{
	struct ice_softc *sc = vsi->sc;
	int i;

	MPASS(max_tx_queues > 0);
	MPASS(max_rx_queues > 0);

	/* Allocate Tx queue mapping memory */
	if (!(vsi->tx_qmap =
	      (u16 *) malloc(sizeof(u16) * max_tx_queues, M_ICE, M_WAITOK))) {
		device_printf(sc->dev, "Unable to allocate Tx qmap memory\n");
		return (ENOMEM);
	}

	/* Allocate Rx queue mapping memory */
	if (!(vsi->rx_qmap =
	      (u16 *) malloc(sizeof(u16) * max_rx_queues, M_ICE, M_WAITOK))) {
		device_printf(sc->dev, "Unable to allocate Rx qmap memory\n");
		goto free_tx_qmap;
	}

	/* Mark every queue map as invalid to start with */
	for (i = 0; i < max_tx_queues; i++) {
		vsi->tx_qmap[i] = ICE_INVALID_RES_IDX;
	}
	for (i = 0; i < max_rx_queues; i++) {
		vsi->rx_qmap[i] = ICE_INVALID_RES_IDX;
	}

	return 0;

free_tx_qmap:
	free(vsi->tx_qmap, M_ICE);
	vsi->tx_qmap = NULL;

	return (ENOMEM);
}

/**
 * ice_free_vsi_qmaps - Free the PF qmaps associated with a VSI
 * @vsi: the VSI private structure
 *
 * Frees the PF qmaps associated with the given VSI. Generally this will be
 * called by ice_release_vsi, but may need to be called during attach cleanup,
 * depending on when the qmaps were allocated.
 */
void
ice_free_vsi_qmaps(struct ice_vsi *vsi)
{
	struct ice_softc *sc = vsi->sc;

	if (vsi->tx_qmap) {
		ice_resmgr_release_map(&sc->tx_qmgr, vsi->tx_qmap,
					   vsi->num_tx_queues);
		free(vsi->tx_qmap, M_ICE);
		vsi->tx_qmap = NULL;
	}

	if (vsi->rx_qmap) {
		ice_resmgr_release_map(&sc->rx_qmgr, vsi->rx_qmap,
					   vsi->num_rx_queues);
		free(vsi->rx_qmap, M_ICE);
		vsi->rx_qmap = NULL;
	}
}

/**
 * ice_set_default_vsi_ctx - Setup default VSI context parameters
 * @ctx: the VSI context to initialize
 *
 * Initialize and prepare a default VSI context for configuring a new VSI.
 */
static void
ice_set_default_vsi_ctx(struct ice_vsi_ctx *ctx)
{
	u32 table = 0;

	memset(&ctx->info, 0, sizeof(ctx->info));
	/* VSI will be allocated from shared pool */
	ctx->alloc_from_pool = true;
	/* Enable source pruning by default */
	ctx->info.sw_flags = ICE_AQ_VSI_SW_FLAG_SRC_PRUNE;
	/* Traffic from VSI can be sent to LAN */
	ctx->info.sw_flags2 = ICE_AQ_VSI_SW_FLAG_LAN_ENA;
	/* Allow all packets untagged/tagged */
	ctx->info.inner_vlan_flags = ((ICE_AQ_VSI_INNER_VLAN_TX_MODE_ALL &
				       ICE_AQ_VSI_INNER_VLAN_TX_MODE_M) >>
				       ICE_AQ_VSI_INNER_VLAN_TX_MODE_S);
	/* Show VLAN/UP from packets in Rx descriptors */
	ctx->info.inner_vlan_flags |= ((ICE_AQ_VSI_INNER_VLAN_EMODE_STR_BOTH &
					ICE_AQ_VSI_INNER_VLAN_EMODE_M) >>
					ICE_AQ_VSI_INNER_VLAN_EMODE_S);
	/* Have 1:1 UP mapping for both ingress/egress tables */
	table |= ICE_UP_TABLE_TRANSLATE(0, 0);
	table |= ICE_UP_TABLE_TRANSLATE(1, 1);
	table |= ICE_UP_TABLE_TRANSLATE(2, 2);
	table |= ICE_UP_TABLE_TRANSLATE(3, 3);
	table |= ICE_UP_TABLE_TRANSLATE(4, 4);
	table |= ICE_UP_TABLE_TRANSLATE(5, 5);
	table |= ICE_UP_TABLE_TRANSLATE(6, 6);
	table |= ICE_UP_TABLE_TRANSLATE(7, 7);
	ctx->info.ingress_table = CPU_TO_LE32(table);
	ctx->info.egress_table = CPU_TO_LE32(table);
	/* Have 1:1 UP mapping for outer to inner UP table */
	ctx->info.outer_up_table = CPU_TO_LE32(table);
	/* No Outer tag support, so outer_vlan_flags remains zero */
}

/**
 * ice_set_rss_vsi_ctx - Setup VSI context parameters for RSS
 * @ctx: the VSI context to configure
 * @type: the VSI type
 *
 * Configures the VSI context for RSS, based on the VSI type.
 */
static void
ice_set_rss_vsi_ctx(struct ice_vsi_ctx *ctx, enum ice_vsi_type type)
{
	u8 lut_type, hash_type;

	switch (type) {
	case ICE_VSI_PF:
		lut_type = ICE_AQ_VSI_Q_OPT_RSS_LUT_PF;
		hash_type = ICE_AQ_VSI_Q_OPT_RSS_TPLZ;
		break;
	case ICE_VSI_VF:
		lut_type = ICE_AQ_VSI_Q_OPT_RSS_LUT_VSI;
		hash_type = ICE_AQ_VSI_Q_OPT_RSS_TPLZ;
		break;
	default:
		/* Other VSI types do not support RSS */
		return;
	}

	ctx->info.q_opt_rss = (((lut_type << ICE_AQ_VSI_Q_OPT_RSS_LUT_S) &
				 ICE_AQ_VSI_Q_OPT_RSS_LUT_M) |
				((hash_type << ICE_AQ_VSI_Q_OPT_RSS_HASH_S) &
				 ICE_AQ_VSI_Q_OPT_RSS_HASH_M));
}

/**
 * ice_setup_vsi_qmap - Setup the queue mapping for a VSI
 * @vsi: the VSI to configure
 * @ctx: the VSI context to configure
 *
 * Configures the context for the given VSI, setting up how the firmware
 * should map the queues for this VSI.
 */
static int
ice_setup_vsi_qmap(struct ice_vsi *vsi, struct ice_vsi_ctx *ctx)
{
	int pow = 0;
	u16 qmap;

	MPASS(vsi->rx_qmap != NULL);

	switch (vsi->qmap_type) {
	case ICE_RESMGR_ALLOC_CONTIGUOUS:
		ctx->info.mapping_flags |= CPU_TO_LE16(ICE_AQ_VSI_Q_MAP_CONTIG);

		ctx->info.q_mapping[0] = CPU_TO_LE16(vsi->rx_qmap[0]);
		ctx->info.q_mapping[1] = CPU_TO_LE16(vsi->num_rx_queues);

		break;
	case ICE_RESMGR_ALLOC_SCATTERED:
		ctx->info.mapping_flags |= CPU_TO_LE16(ICE_AQ_VSI_Q_MAP_NONCONTIG);

		for (int i = 0; i < vsi->num_rx_queues; i++)
			ctx->info.q_mapping[i] = CPU_TO_LE16(vsi->rx_qmap[i]);
		break;
	default:
		return (EOPNOTSUPP);
	}

	/* Calculate the next power-of-2 of number of queues */
	if (vsi->num_rx_queues)
		pow = flsl(vsi->num_rx_queues - 1);

	/* Assign all the queues to traffic class zero */
	qmap = (pow << ICE_AQ_VSI_TC_Q_NUM_S) & ICE_AQ_VSI_TC_Q_NUM_M;
	ctx->info.tc_mapping[0] = CPU_TO_LE16(qmap);

	/* Fill out default driver TC queue info for VSI */
	vsi->tc_info[0].qoffset = 0;
	vsi->tc_info[0].qcount_rx = vsi->num_rx_queues;
	vsi->tc_info[0].qcount_tx = vsi->num_tx_queues;
	for (int i = 1; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		vsi->tc_info[i].qoffset = 0;
		vsi->tc_info[i].qcount_rx = 1;
		vsi->tc_info[i].qcount_tx = 1;
	}
	vsi->tc_map = 0x1;

	return 0;
}

/**
 * ice_initialize_vsi - Initialize a VSI for use
 * @vsi: the vsi to initialize
 *
 * Initialize a VSI over the adminq and prepare it for operation.
 */
int
ice_initialize_vsi(struct ice_vsi *vsi)
{
	struct ice_vsi_ctx ctx = { 0 };
	struct ice_hw *hw = &vsi->sc->hw;
	u16 max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	enum ice_status status;
	int err;

	/* For now, we only have code supporting PF VSIs */
	switch (vsi->type) {
	case ICE_VSI_PF:
		ctx.flags = ICE_AQ_VSI_TYPE_PF;
		break;
	default:
		return (ENODEV);
	}

	ice_set_default_vsi_ctx(&ctx);
	ice_set_rss_vsi_ctx(&ctx, vsi->type);

	/* XXX: VSIs of other types may need different port info? */
	ctx.info.sw_id = hw->port_info->sw_id;

	/* Set some RSS parameters based on the VSI type */
	ice_vsi_set_rss_params(vsi);

	/* Initialize the Rx queue mapping for this VSI */
	err = ice_setup_vsi_qmap(vsi, &ctx);
	if (err) {
		return err;
	}

	/* (Re-)add VSI to HW VSI handle list */
	status = ice_add_vsi(hw, vsi->idx, &ctx, NULL);
	if (status != 0) {
		device_printf(vsi->sc->dev,
		    "Add VSI AQ call failed, err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}
	vsi->info = ctx.info;

	/* Initialize VSI with just 1 TC to start */
	max_txqs[0] = vsi->num_tx_queues;

	status = ice_cfg_vsi_lan(hw->port_info, vsi->idx,
			      ICE_DFLT_TRAFFIC_CLASS, max_txqs);
	if (status) {
		device_printf(vsi->sc->dev,
		    "Failed VSI lan queue config, err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		ice_deinit_vsi(vsi);
		return (ENODEV);
	}

	/* Reset VSI stats */
	ice_reset_vsi_stats(vsi);

	return 0;
}

/**
 * ice_deinit_vsi - Tell firmware to release resources for a VSI
 * @vsi: the VSI to release
 *
 * Helper function which requests the firmware to release the hardware
 * resources associated with a given VSI.
 */
void
ice_deinit_vsi(struct ice_vsi *vsi)
{
	struct ice_vsi_ctx ctx = { 0 };
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;

	/* Assert that the VSI pointer matches in the list */
	MPASS(vsi == sc->all_vsi[vsi->idx]);

	ctx.info = vsi->info;

	status = ice_rm_vsi_lan_cfg(hw->port_info, vsi->idx);
	if (status) {
		/*
		 * This should only fail if the VSI handle is invalid, or if
		 * any of the nodes have leaf nodes which are still in use.
		 */
		device_printf(sc->dev,
			      "Unable to remove scheduler nodes for VSI %d, err %s\n",
			      vsi->idx, ice_status_str(status));
	}

	/* Tell firmware to release the VSI resources */
	status = ice_free_vsi(hw, vsi->idx, &ctx, false, NULL);
	if (status != 0) {
		device_printf(sc->dev,
		    "Free VSI %u AQ call failed, err %s aq_err %s\n",
		    vsi->idx, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
	}
}

/**
 * ice_release_vsi - Release resources associated with a VSI
 * @vsi: the VSI to release
 *
 * Release software and firmware resources associated with a VSI. Release the
 * queue managers associated with this VSI. Also free the VSI structure memory
 * if the VSI was allocated dynamically using ice_alloc_vsi().
 */
void
ice_release_vsi(struct ice_vsi *vsi)
{
	struct ice_softc *sc = vsi->sc;
	int idx = vsi->idx;

	/* Assert that the VSI pointer matches in the list */
	MPASS(vsi == sc->all_vsi[idx]);

	/* Cleanup RSS configuration */
	if (ice_is_bit_set(sc->feat_en, ICE_FEATURE_RSS))
		ice_clean_vsi_rss_cfg(vsi);

	ice_del_vsi_sysctl_ctx(vsi);

	/*
	 * If we unload the driver after a reset fails, we do not need to do
	 * this step.
	 */
	if (!ice_test_state(&sc->state, ICE_STATE_RESET_FAILED))
		ice_deinit_vsi(vsi);

	ice_free_vsi_qmaps(vsi);

	if (vsi->dynamic) {
		free(sc->all_vsi[idx], M_ICE);
	}

	sc->all_vsi[idx] = NULL;
}

/**
 * ice_aq_speed_to_rate - Convert AdminQ speed enum to baudrate
 * @pi: port info data
 *
 * Returns the baudrate value for the current link speed of a given port.
 */
uint64_t
ice_aq_speed_to_rate(struct ice_port_info *pi)
{
	switch (pi->phy.link_info.link_speed) {
	case ICE_AQ_LINK_SPEED_100GB:
		return IF_Gbps(100);
	case ICE_AQ_LINK_SPEED_50GB:
		return IF_Gbps(50);
	case ICE_AQ_LINK_SPEED_40GB:
		return IF_Gbps(40);
	case ICE_AQ_LINK_SPEED_25GB:
		return IF_Gbps(25);
	case ICE_AQ_LINK_SPEED_10GB:
		return IF_Gbps(10);
	case ICE_AQ_LINK_SPEED_5GB:
		return IF_Gbps(5);
	case ICE_AQ_LINK_SPEED_2500MB:
		return IF_Mbps(2500);
	case ICE_AQ_LINK_SPEED_1000MB:
		return IF_Mbps(1000);
	case ICE_AQ_LINK_SPEED_100MB:
		return IF_Mbps(100);
	case ICE_AQ_LINK_SPEED_10MB:
		return IF_Mbps(10);
	case ICE_AQ_LINK_SPEED_UNKNOWN:
	default:
		/* return 0 if we don't know the link speed */
		return 0;
	}
}

/**
 * ice_aq_speed_to_str - Convert AdminQ speed enum to string representation
 * @pi: port info data
 *
 * Returns the string representation of the current link speed for a given
 * port.
 */
static const char *
ice_aq_speed_to_str(struct ice_port_info *pi)
{
	switch (pi->phy.link_info.link_speed) {
	case ICE_AQ_LINK_SPEED_100GB:
		return "100 Gbps";
	case ICE_AQ_LINK_SPEED_50GB:
		return "50 Gbps";
	case ICE_AQ_LINK_SPEED_40GB:
		return "40 Gbps";
	case ICE_AQ_LINK_SPEED_25GB:
		return "25 Gbps";
	case ICE_AQ_LINK_SPEED_20GB:
		return "20 Gbps";
	case ICE_AQ_LINK_SPEED_10GB:
		return "10 Gbps";
	case ICE_AQ_LINK_SPEED_5GB:
		return "5 Gbps";
	case ICE_AQ_LINK_SPEED_2500MB:
		return "2.5 Gbps";
	case ICE_AQ_LINK_SPEED_1000MB:
		return "1 Gbps";
	case ICE_AQ_LINK_SPEED_100MB:
		return "100 Mbps";
	case ICE_AQ_LINK_SPEED_10MB:
		return "10 Mbps";
	case ICE_AQ_LINK_SPEED_UNKNOWN:
	default:
		return "Unknown speed";
	}
}

/**
 * ice_get_phy_type_low - Get media associated with phy_type_low
 * @phy_type_low: the low 64bits of phy_type from the AdminQ
 *
 * Given the lower 64bits of the phy_type from the hardware, return the
 * ifm_active bit associated. Return IFM_UNKNOWN when phy_type_low is unknown.
 * Note that only one of ice_get_phy_type_low or ice_get_phy_type_high should
 * be called. If phy_type_low is zero, call ice_phy_type_high.
 */
int
ice_get_phy_type_low(uint64_t phy_type_low)
{
	switch (phy_type_low) {
	case ICE_PHY_TYPE_LOW_100BASE_TX:
		return IFM_100_TX;
	case ICE_PHY_TYPE_LOW_100M_SGMII:
		return IFM_100_SGMII;
	case ICE_PHY_TYPE_LOW_1000BASE_T:
		return IFM_1000_T;
	case ICE_PHY_TYPE_LOW_1000BASE_SX:
		return IFM_1000_SX;
	case ICE_PHY_TYPE_LOW_1000BASE_LX:
		return IFM_1000_LX;
	case ICE_PHY_TYPE_LOW_1000BASE_KX:
		return IFM_1000_KX;
	case ICE_PHY_TYPE_LOW_1G_SGMII:
		return IFM_1000_SGMII;
	case ICE_PHY_TYPE_LOW_2500BASE_T:
		return IFM_2500_T;
	case ICE_PHY_TYPE_LOW_2500BASE_X:
		return IFM_2500_X;
	case ICE_PHY_TYPE_LOW_2500BASE_KX:
		return IFM_2500_KX;
	case ICE_PHY_TYPE_LOW_5GBASE_T:
		return IFM_5000_T;
	case ICE_PHY_TYPE_LOW_5GBASE_KR:
		return IFM_5000_KR;
	case ICE_PHY_TYPE_LOW_10GBASE_T:
		return IFM_10G_T;
	case ICE_PHY_TYPE_LOW_10G_SFI_DA:
		return IFM_10G_TWINAX;
	case ICE_PHY_TYPE_LOW_10GBASE_SR:
		return IFM_10G_SR;
	case ICE_PHY_TYPE_LOW_10GBASE_LR:
		return IFM_10G_LR;
	case ICE_PHY_TYPE_LOW_10GBASE_KR_CR1:
		return IFM_10G_KR;
	case ICE_PHY_TYPE_LOW_10G_SFI_AOC_ACC:
		return IFM_10G_AOC;
	case ICE_PHY_TYPE_LOW_10G_SFI_C2C:
		return IFM_10G_SFI;
	case ICE_PHY_TYPE_LOW_25GBASE_T:
		return IFM_25G_T;
	case ICE_PHY_TYPE_LOW_25GBASE_CR:
		return IFM_25G_CR;
	case ICE_PHY_TYPE_LOW_25GBASE_CR_S:
		return IFM_25G_CR_S;
	case ICE_PHY_TYPE_LOW_25GBASE_CR1:
		return IFM_25G_CR1;
	case ICE_PHY_TYPE_LOW_25GBASE_SR:
		return IFM_25G_SR;
	case ICE_PHY_TYPE_LOW_25GBASE_LR:
		return IFM_25G_LR;
	case ICE_PHY_TYPE_LOW_25GBASE_KR:
		return IFM_25G_KR;
	case ICE_PHY_TYPE_LOW_25GBASE_KR_S:
		return IFM_25G_KR_S;
	case ICE_PHY_TYPE_LOW_25GBASE_KR1:
		return IFM_25G_KR1;
	case ICE_PHY_TYPE_LOW_25G_AUI_AOC_ACC:
		return IFM_25G_AOC;
	case ICE_PHY_TYPE_LOW_25G_AUI_C2C:
		return IFM_25G_AUI;
	case ICE_PHY_TYPE_LOW_40GBASE_CR4:
		return IFM_40G_CR4;
	case ICE_PHY_TYPE_LOW_40GBASE_SR4:
		return IFM_40G_SR4;
	case ICE_PHY_TYPE_LOW_40GBASE_LR4:
		return IFM_40G_LR4;
	case ICE_PHY_TYPE_LOW_40GBASE_KR4:
		return IFM_40G_KR4;
	case ICE_PHY_TYPE_LOW_40G_XLAUI_AOC_ACC:
		return IFM_40G_XLAUI_AC;
	case ICE_PHY_TYPE_LOW_40G_XLAUI:
		return IFM_40G_XLAUI;
	case ICE_PHY_TYPE_LOW_50GBASE_CR2:
		return IFM_50G_CR2;
	case ICE_PHY_TYPE_LOW_50GBASE_SR2:
		return IFM_50G_SR2;
	case ICE_PHY_TYPE_LOW_50GBASE_LR2:
		return IFM_50G_LR2;
	case ICE_PHY_TYPE_LOW_50GBASE_KR2:
		return IFM_50G_KR2;
	case ICE_PHY_TYPE_LOW_50G_LAUI2_AOC_ACC:
		return IFM_50G_LAUI2_AC;
	case ICE_PHY_TYPE_LOW_50G_LAUI2:
		return IFM_50G_LAUI2;
	case ICE_PHY_TYPE_LOW_50G_AUI2_AOC_ACC:
		return IFM_50G_AUI2_AC;
	case ICE_PHY_TYPE_LOW_50G_AUI2:
		return IFM_50G_AUI2;
	case ICE_PHY_TYPE_LOW_50GBASE_CP:
		return IFM_50G_CP;
	case ICE_PHY_TYPE_LOW_50GBASE_SR:
		return IFM_50G_SR;
	case ICE_PHY_TYPE_LOW_50GBASE_FR:
		return IFM_50G_FR;
	case ICE_PHY_TYPE_LOW_50GBASE_LR:
		return IFM_50G_LR;
	case ICE_PHY_TYPE_LOW_50GBASE_KR_PAM4:
		return IFM_50G_KR_PAM4;
	case ICE_PHY_TYPE_LOW_50G_AUI1_AOC_ACC:
		return IFM_50G_AUI1_AC;
	case ICE_PHY_TYPE_LOW_50G_AUI1:
		return IFM_50G_AUI1;
	case ICE_PHY_TYPE_LOW_100GBASE_CR4:
		return IFM_100G_CR4;
	case ICE_PHY_TYPE_LOW_100GBASE_SR4:
		return IFM_100G_SR4;
	case ICE_PHY_TYPE_LOW_100GBASE_LR4:
		return IFM_100G_LR4;
	case ICE_PHY_TYPE_LOW_100GBASE_KR4:
		return IFM_100G_KR4;
	case ICE_PHY_TYPE_LOW_100G_CAUI4_AOC_ACC:
		return IFM_100G_CAUI4_AC;
	case ICE_PHY_TYPE_LOW_100G_CAUI4:
		return IFM_100G_CAUI4;
	case ICE_PHY_TYPE_LOW_100G_AUI4_AOC_ACC:
		return IFM_100G_AUI4_AC;
	case ICE_PHY_TYPE_LOW_100G_AUI4:
		return IFM_100G_AUI4;
	case ICE_PHY_TYPE_LOW_100GBASE_CR_PAM4:
		return IFM_100G_CR_PAM4;
	case ICE_PHY_TYPE_LOW_100GBASE_KR_PAM4:
		return IFM_100G_KR_PAM4;
	case ICE_PHY_TYPE_LOW_100GBASE_CP2:
		return IFM_100G_CP2;
	case ICE_PHY_TYPE_LOW_100GBASE_SR2:
		return IFM_100G_SR2;
	case ICE_PHY_TYPE_LOW_100GBASE_DR:
		return IFM_100G_DR;
	default:
		return IFM_UNKNOWN;
	}
}

/**
 * ice_get_phy_type_high - Get media associated with phy_type_high
 * @phy_type_high: the upper 64bits of phy_type from the AdminQ
 *
 * Given the upper 64bits of the phy_type from the hardware, return the
 * ifm_active bit associated. Return IFM_UNKNOWN on an unknown value. Note
 * that only one of ice_get_phy_type_low or ice_get_phy_type_high should be
 * called. If phy_type_high is zero, call ice_get_phy_type_low.
 */
int
ice_get_phy_type_high(uint64_t phy_type_high)
{
	switch (phy_type_high) {
	case ICE_PHY_TYPE_HIGH_100GBASE_KR2_PAM4:
		return IFM_100G_KR2_PAM4;
	case ICE_PHY_TYPE_HIGH_100G_CAUI2_AOC_ACC:
		return IFM_100G_CAUI2_AC;
	case ICE_PHY_TYPE_HIGH_100G_CAUI2:
		return IFM_100G_CAUI2;
	case ICE_PHY_TYPE_HIGH_100G_AUI2_AOC_ACC:
		return IFM_100G_AUI2_AC;
	case ICE_PHY_TYPE_HIGH_100G_AUI2:
		return IFM_100G_AUI2;
	default:
		return IFM_UNKNOWN;
	}
}

/**
 * ice_phy_types_to_max_rate - Returns port's max supported baudrate
 * @pi: port info struct
 *
 * ice_aq_get_phy_caps() w/ ICE_AQC_REPORT_TOPO_CAP_MEDIA parameter needs
 * to have been called before this function for it to work.
 */
static uint64_t
ice_phy_types_to_max_rate(struct ice_port_info *pi)
{
	uint64_t phy_low = pi->phy.phy_type_low;
	uint64_t phy_high = pi->phy.phy_type_high;
	uint64_t max_rate = 0;
	int bit;

	/*
	 * These are based on the indices used in the BIT() macros for
	 * ICE_PHY_TYPE_LOW_*
	 */
	static const uint64_t phy_rates[] = {
	    IF_Mbps(100),
	    IF_Mbps(100),
	    IF_Gbps(1ULL),
	    IF_Gbps(1ULL),
	    IF_Gbps(1ULL),
	    IF_Gbps(1ULL),
	    IF_Gbps(1ULL),
	    IF_Mbps(2500ULL),
	    IF_Mbps(2500ULL),
	    IF_Mbps(2500ULL),
	    IF_Gbps(5ULL),
	    IF_Gbps(5ULL),
	    IF_Gbps(10ULL),
	    IF_Gbps(10ULL),
	    IF_Gbps(10ULL),
	    IF_Gbps(10ULL),
	    IF_Gbps(10ULL),
	    IF_Gbps(10ULL),
	    IF_Gbps(10ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(25ULL),
	    IF_Gbps(40ULL),
	    IF_Gbps(40ULL),
	    IF_Gbps(40ULL),
	    IF_Gbps(40ULL),
	    IF_Gbps(40ULL),
	    IF_Gbps(40ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(50ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    /* These rates are for ICE_PHY_TYPE_HIGH_* */
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL),
	    IF_Gbps(100ULL)
	};

	/* coverity[address_of] */
	for_each_set_bit(bit, &phy_high, 64)
		if ((bit + 64) < (int)ARRAY_SIZE(phy_rates))
			max_rate = uqmax(max_rate, phy_rates[(bit + 64)]);

	/* coverity[address_of] */
	for_each_set_bit(bit, &phy_low, 64)
		max_rate = uqmax(max_rate, phy_rates[bit]);

	return (max_rate);
}

/* The if_media type is split over the original 5 bit media variant field,
 * along with extended types using up extra bits in the options section.
 * We want to convert this split number into a bitmap index, so we reverse the
 * calculation of IFM_X here.
 */
#define IFM_IDX(x) (((x) & IFM_TMASK) | \
		    (((x) & IFM_ETH_XTYPE) >> IFM_ETH_XSHIFT))

/**
 * ice_add_media_types - Add supported media types to the media structure
 * @sc: ice private softc structure
 * @media: ifmedia structure to setup
 *
 * Looks up the supported phy types, and initializes the various media types
 * available.
 *
 * @pre this function must be protected from being called while another thread
 * is accessing the ifmedia types.
 */
enum ice_status
ice_add_media_types(struct ice_softc *sc, struct ifmedia *media)
{
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	struct ice_port_info *pi = sc->hw.port_info;
	enum ice_status status;
	uint64_t phy_low, phy_high;
	int bit;

	ASSERT_CFG_LOCKED(sc);

	/* the maximum possible media type index is 511. We probably don't
	 * need most of this space, but this ensures future compatibility when
	 * additional media types are used.
	 */
	ice_declare_bitmap(already_added, 511);

	/* Remove all previous media types */
	ifmedia_removeall(media);

	status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG,
				     &pcaps, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(sc->dev,
		    "%s: ice_aq_get_phy_caps (ACTIVE) failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(sc->hw.adminq.sq_last_status));
		return (status);
	}
	phy_low = le64toh(pcaps.phy_type_low);
	phy_high = le64toh(pcaps.phy_type_high);

	/* make sure the added bitmap is zero'd */
	memset(already_added, 0, sizeof(already_added));

	/* coverity[address_of] */
	for_each_set_bit(bit, &phy_low, 64) {
		uint64_t type = BIT_ULL(bit);
		int ostype;

		/* get the OS media type */
		ostype = ice_get_phy_type_low(type);

		/* don't bother adding the unknown type */
		if (ostype == IFM_UNKNOWN)
			continue;

		/* only add each media type to the list once */
		if (ice_is_bit_set(already_added, IFM_IDX(ostype)))
			continue;

		ifmedia_add(media, IFM_ETHER | ostype, 0, NULL);
		ice_set_bit(IFM_IDX(ostype), already_added);
	}

	/* coverity[address_of] */
	for_each_set_bit(bit, &phy_high, 64) {
		uint64_t type = BIT_ULL(bit);
		int ostype;

		/* get the OS media type */
		ostype = ice_get_phy_type_high(type);

		/* don't bother adding the unknown type */
		if (ostype == IFM_UNKNOWN)
			continue;

		/* only add each media type to the list once */
		if (ice_is_bit_set(already_added, IFM_IDX(ostype)))
			continue;

		ifmedia_add(media, IFM_ETHER | ostype, 0, NULL);
		ice_set_bit(IFM_IDX(ostype), already_added);
	}

	/* Use autoselect media by default */
	ifmedia_add(media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(media, IFM_ETHER | IFM_AUTO);

	return (ICE_SUCCESS);
}

/**
 * ice_configure_rxq_interrupt - Configure HW Rx queue for an MSI-X interrupt
 * @hw: ice hw structure
 * @rxqid: Rx queue index in PF space
 * @vector: MSI-X vector index in PF/VF space
 * @itr_idx: ITR index to use for interrupt
 *
 * @remark ice_flush() may need to be called after this
 */
void
ice_configure_rxq_interrupt(struct ice_hw *hw, u16 rxqid, u16 vector, u8 itr_idx)
{
	u32 val;

	MPASS(itr_idx <= ICE_ITR_NONE);

	val = (QINT_RQCTL_CAUSE_ENA_M |
	       (itr_idx << QINT_RQCTL_ITR_INDX_S) |
	       (vector << QINT_RQCTL_MSIX_INDX_S));
	wr32(hw, QINT_RQCTL(rxqid), val);
}

/**
 * ice_configure_all_rxq_interrupts - Configure HW Rx queues for MSI-X interrupts
 * @vsi: the VSI to configure
 *
 * Called when setting up MSI-X interrupts to configure the Rx hardware queues.
 */
void
ice_configure_all_rxq_interrupts(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->sc->hw;
	int i;

	for (i = 0; i < vsi->num_rx_queues; i++) {
		struct ice_rx_queue *rxq = &vsi->rx_queues[i];

		ice_configure_rxq_interrupt(hw, vsi->rx_qmap[rxq->me],
					    rxq->irqv->me, ICE_RX_ITR);
	}

	ice_flush(hw);
}

/**
 * ice_configure_txq_interrupt - Configure HW Tx queue for an MSI-X interrupt
 * @hw: ice hw structure
 * @txqid: Tx queue index in PF space
 * @vector: MSI-X vector index in PF/VF space
 * @itr_idx: ITR index to use for interrupt
 *
 * @remark ice_flush() may need to be called after this
 */
void
ice_configure_txq_interrupt(struct ice_hw *hw, u16 txqid, u16 vector, u8 itr_idx)
{
	u32 val;

	MPASS(itr_idx <= ICE_ITR_NONE);

	val = (QINT_TQCTL_CAUSE_ENA_M |
	       (itr_idx << QINT_TQCTL_ITR_INDX_S) |
	       (vector << QINT_TQCTL_MSIX_INDX_S));
	wr32(hw, QINT_TQCTL(txqid), val);
}

/**
 * ice_configure_all_txq_interrupts - Configure HW Tx queues for MSI-X interrupts
 * @vsi: the VSI to configure
 *
 * Called when setting up MSI-X interrupts to configure the Tx hardware queues.
 */
void
ice_configure_all_txq_interrupts(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->sc->hw;
	int i;

	for (i = 0; i < vsi->num_tx_queues; i++) {
		struct ice_tx_queue *txq = &vsi->tx_queues[i];

		ice_configure_txq_interrupt(hw, vsi->tx_qmap[txq->me],
					    txq->irqv->me, ICE_TX_ITR);
	}

	ice_flush(hw);
}

/**
 * ice_flush_rxq_interrupts - Unconfigure Hw Rx queues MSI-X interrupt cause
 * @vsi: the VSI to configure
 *
 * Unset the CAUSE_ENA flag of the TQCTL register for each queue, then trigger
 * a software interrupt on that cause. This is required as part of the Rx
 * queue disable logic to dissociate the Rx queue from the interrupt.
 *
 * Note: this function must be called prior to disabling Rx queues with
 * ice_control_all_rx_queues, otherwise the Rx queue may not be disabled properly.
 */
void
ice_flush_rxq_interrupts(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->sc->hw;
	int i;

	for (i = 0; i < vsi->num_rx_queues; i++) {
		struct ice_rx_queue *rxq = &vsi->rx_queues[i];
		u32 reg, val;

		/* Clear the CAUSE_ENA flag */
		reg = vsi->rx_qmap[rxq->me];
		val = rd32(hw, QINT_RQCTL(reg));
		val &= ~QINT_RQCTL_CAUSE_ENA_M;
		wr32(hw, QINT_RQCTL(reg), val);

		ice_flush(hw);

		/* Trigger a software interrupt to complete interrupt
		 * dissociation.
		 */
		wr32(hw, GLINT_DYN_CTL(rxq->irqv->me),
		     GLINT_DYN_CTL_SWINT_TRIG_M | GLINT_DYN_CTL_INTENA_MSK_M);
	}
}

/**
 * ice_flush_txq_interrupts - Unconfigure Hw Tx queues MSI-X interrupt cause
 * @vsi: the VSI to configure
 *
 * Unset the CAUSE_ENA flag of the TQCTL register for each queue, then trigger
 * a software interrupt on that cause. This is required as part of the Tx
 * queue disable logic to dissociate the Tx queue from the interrupt.
 *
 * Note: this function must be called prior to ice_vsi_disable_tx, otherwise
 * the Tx queue disable may not complete properly.
 */
void
ice_flush_txq_interrupts(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->sc->hw;
	int i;

	for (i = 0; i < vsi->num_tx_queues; i++) {
		struct ice_tx_queue *txq = &vsi->tx_queues[i];
		u32 reg, val;

		/* Clear the CAUSE_ENA flag */
		reg = vsi->tx_qmap[txq->me];
		val = rd32(hw, QINT_TQCTL(reg));
		val &= ~QINT_TQCTL_CAUSE_ENA_M;
		wr32(hw, QINT_TQCTL(reg), val);

		ice_flush(hw);

		/* Trigger a software interrupt to complete interrupt
		 * dissociation.
		 */
		wr32(hw, GLINT_DYN_CTL(txq->irqv->me),
		     GLINT_DYN_CTL_SWINT_TRIG_M | GLINT_DYN_CTL_INTENA_MSK_M);
	}
}

/**
 * ice_configure_rx_itr - Configure the Rx ITR settings for this VSI
 * @vsi: the VSI to configure
 *
 * Program the hardware ITR registers with the settings for this VSI.
 */
void
ice_configure_rx_itr(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->sc->hw;
	int i;

	/* TODO: Handle per-queue/per-vector ITR? */

	for (i = 0; i < vsi->num_rx_queues; i++) {
		struct ice_rx_queue *rxq = &vsi->rx_queues[i];

		wr32(hw, GLINT_ITR(ICE_RX_ITR, rxq->irqv->me),
		     ice_itr_to_reg(hw, vsi->rx_itr));
	}

	ice_flush(hw);
}

/**
 * ice_configure_tx_itr - Configure the Tx ITR settings for this VSI
 * @vsi: the VSI to configure
 *
 * Program the hardware ITR registers with the settings for this VSI.
 */
void
ice_configure_tx_itr(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->sc->hw;
	int i;

	/* TODO: Handle per-queue/per-vector ITR? */

	for (i = 0; i < vsi->num_tx_queues; i++) {
		struct ice_tx_queue *txq = &vsi->tx_queues[i];

		wr32(hw, GLINT_ITR(ICE_TX_ITR, txq->irqv->me),
		     ice_itr_to_reg(hw, vsi->tx_itr));
	}

	ice_flush(hw);
}

/**
 * ice_setup_tx_ctx - Setup an ice_tlan_ctx structure for a queue
 * @txq: the Tx queue to configure
 * @tlan_ctx: the Tx LAN queue context structure to initialize
 * @pf_q: real queue number
 */
static int
ice_setup_tx_ctx(struct ice_tx_queue *txq, struct ice_tlan_ctx *tlan_ctx, u16 pf_q)
{
	struct ice_vsi *vsi = txq->vsi;
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;

	tlan_ctx->port_num = hw->port_info->lport;

	/* number of descriptors in the queue */
	tlan_ctx->qlen = txq->desc_count;

	/* set the transmit queue base address, defined in 128 byte units */
	tlan_ctx->base = txq->tx_paddr >> 7;

	tlan_ctx->pf_num = hw->pf_id;

	switch (vsi->type) {
	case ICE_VSI_PF:
		tlan_ctx->vmvf_type = ICE_TLAN_CTX_VMVF_TYPE_PF;
		break;
	default:
		return (ENODEV);
	}

	tlan_ctx->src_vsi = ice_get_hw_vsi_num(hw, vsi->idx);

	/* Enable TSO */
	tlan_ctx->tso_ena = 1;
	tlan_ctx->internal_usage_flag = 1;

	tlan_ctx->tso_qnum = pf_q;

	/*
	 * Stick with the older legacy Tx queue interface, instead of the new
	 * advanced queue interface.
	 */
	tlan_ctx->legacy_int = 1;

	/* Descriptor WB mode */
	tlan_ctx->wb_mode = 0;

	return (0);
}

/**
 * ice_cfg_vsi_for_tx - Configure the hardware for Tx
 * @vsi: the VSI to configure
 *
 * Configure the device Tx queues through firmware AdminQ commands. After
 * this, Tx queues will be ready for transmit.
 */
int
ice_cfg_vsi_for_tx(struct ice_vsi *vsi)
{
	struct ice_aqc_add_tx_qgrp *qg;
	struct ice_hw *hw = &vsi->sc->hw;
	device_t dev = vsi->sc->dev;
	enum ice_status status;
	int i;
	int err = 0;
	u16 qg_size, pf_q;

	qg_size = ice_struct_size(qg, txqs, 1);
	qg = (struct ice_aqc_add_tx_qgrp *)malloc(qg_size, M_ICE, M_NOWAIT|M_ZERO);
	if (!qg)
		return (ENOMEM);

	qg->num_txqs = 1;

	for (i = 0; i < vsi->num_tx_queues; i++) {
		struct ice_tlan_ctx tlan_ctx = { 0 };
		struct ice_tx_queue *txq = &vsi->tx_queues[i];

		pf_q = vsi->tx_qmap[txq->me];
		qg->txqs[0].txq_id = htole16(pf_q);

		err = ice_setup_tx_ctx(txq, &tlan_ctx, pf_q);
		if (err)
			goto free_txqg;

		ice_set_ctx(hw, (u8 *)&tlan_ctx, qg->txqs[0].txq_ctx,
			    ice_tlan_ctx_info);

		status = ice_ena_vsi_txq(hw->port_info, vsi->idx, txq->tc,
					 txq->q_handle, 1, qg, qg_size, NULL);
		if (status) {
			device_printf(dev,
				      "Failed to set LAN Tx queue %d (TC %d, handle %d) context, err %s aq_err %s\n",
				      i, txq->tc, txq->q_handle,
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
			err = ENODEV;
			goto free_txqg;
		}

		/* Keep track of the Tx queue TEID */
		if (pf_q == le16toh(qg->txqs[0].txq_id))
			txq->q_teid = le32toh(qg->txqs[0].q_teid);
	}

free_txqg:
	free(qg, M_ICE);

	return (err);
}

/**
 * ice_setup_rx_ctx - Setup an Rx context structure for a receive queue
 * @rxq: the receive queue to program
 *
 * Setup an Rx queue context structure and program it into the hardware
 * registers. This is a necessary step for enabling the Rx queue.
 *
 * @pre the VSI associated with this queue must have initialized mbuf_sz
 */
static int
ice_setup_rx_ctx(struct ice_rx_queue *rxq)
{
	struct ice_rlan_ctx rlan_ctx = {0};
	struct ice_vsi *vsi = rxq->vsi;
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;
	u32 rxdid = ICE_RXDID_FLEX_NIC;
	u32 regval;
	u16 pf_q;

	pf_q = vsi->rx_qmap[rxq->me];

	/* set the receive queue base address, defined in 128 byte units */
	rlan_ctx.base = rxq->rx_paddr >> 7;

	rlan_ctx.qlen = rxq->desc_count;

	rlan_ctx.dbuf = vsi->mbuf_sz >> ICE_RLAN_CTX_DBUF_S;

	/* use 32 byte descriptors */
	rlan_ctx.dsize = 1;

	/* Strip the Ethernet CRC bytes before the packet is posted to the
	 * host memory.
	 */
	rlan_ctx.crcstrip = 1;

	rlan_ctx.l2tsel = 1;

	/* don't do header splitting */
	rlan_ctx.dtype = ICE_RX_DTYPE_NO_SPLIT;
	rlan_ctx.hsplit_0 = ICE_RLAN_RX_HSPLIT_0_NO_SPLIT;
	rlan_ctx.hsplit_1 = ICE_RLAN_RX_HSPLIT_1_NO_SPLIT;

	/* strip VLAN from inner headers */
	rlan_ctx.showiv = 1;

	rlan_ctx.rxmax = min(vsi->max_frame_size,
			     ICE_MAX_RX_SEGS * vsi->mbuf_sz);

	rlan_ctx.lrxqthresh = 1;

	if (vsi->type != ICE_VSI_VF) {
		regval = rd32(hw, QRXFLXP_CNTXT(pf_q));
		regval &= ~QRXFLXP_CNTXT_RXDID_IDX_M;
		regval |= (rxdid << QRXFLXP_CNTXT_RXDID_IDX_S) &
			QRXFLXP_CNTXT_RXDID_IDX_M;

		regval &= ~QRXFLXP_CNTXT_RXDID_PRIO_M;
		regval |= (0x03 << QRXFLXP_CNTXT_RXDID_PRIO_S) &
			QRXFLXP_CNTXT_RXDID_PRIO_M;

		wr32(hw, QRXFLXP_CNTXT(pf_q), regval);
	}

	status = ice_write_rxq_ctx(hw, &rlan_ctx, pf_q);
	if (status) {
		device_printf(sc->dev,
			      "Failed to set LAN Rx queue context, err %s aq_err %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	wr32(hw, rxq->tail, 0);

	return 0;
}

/**
 * ice_cfg_vsi_for_rx - Configure the hardware for Rx
 * @vsi: the VSI to configure
 *
 * Prepare an Rx context descriptor and configure the device to receive
 * traffic.
 *
 * @pre the VSI must have initialized mbuf_sz
 */
int
ice_cfg_vsi_for_rx(struct ice_vsi *vsi)
{
	int i, err;

	for (i = 0; i < vsi->num_rx_queues; i++) {
		MPASS(vsi->mbuf_sz > 0);
		err = ice_setup_rx_ctx(&vsi->rx_queues[i]);
		if (err)
			return err;
	}

	return (0);
}

/**
 * ice_is_rxq_ready - Check if an Rx queue is ready
 * @hw: ice hw structure
 * @pf_q: absolute PF queue index to check
 * @reg: on successful return, contains qrx_ctrl contents
 *
 * Reads the QRX_CTRL register and verifies if the queue is in a consistent
 * state. That is, QENA_REQ matches QENA_STAT. Used to check before making
 * a request to change the queue, as well as to verify the request has
 * finished. The queue should change status within a few microseconds, so we
 * use a small delay while polling the register.
 *
 * Returns an error code if the queue does not update after a few retries.
 */
static int
ice_is_rxq_ready(struct ice_hw *hw, int pf_q, u32 *reg)
{
	u32 qrx_ctrl, qena_req, qena_stat;
	int i;

	for (i = 0; i < ICE_Q_WAIT_RETRY_LIMIT; i++) {
		qrx_ctrl = rd32(hw, QRX_CTRL(pf_q));
		qena_req = (qrx_ctrl >> QRX_CTRL_QENA_REQ_S) & 1;
		qena_stat = (qrx_ctrl >> QRX_CTRL_QENA_STAT_S) & 1;

		/* if the request and status bits equal, then the queue is
		 * fully disabled or enabled.
		 */
		if (qena_req == qena_stat) {
			*reg = qrx_ctrl;
			return (0);
		}

		/* wait a few microseconds before we check again */
		DELAY(10);
	}

	return (ETIMEDOUT);
}

/**
 * ice_control_rx_queue - Configure hardware to start or stop an Rx queue
 * @vsi: VSI containing queue to enable/disable
 * @qidx: Queue index in VSI space
 * @enable: true to enable queue, false to disable
 *
 * Control the Rx queue through the QRX_CTRL register, enabling or disabling
 * it. Wait for the appropriate time to ensure that the queue has actually
 * reached the expected state.
 */
int
ice_control_rx_queue(struct ice_vsi *vsi, u16 qidx, bool enable)
{
	struct ice_hw *hw = &vsi->sc->hw;
	device_t dev = vsi->sc->dev;
	u32 qrx_ctrl = 0;
	int err;

	struct ice_rx_queue *rxq = &vsi->rx_queues[qidx];
	int pf_q = vsi->rx_qmap[rxq->me];

	err = ice_is_rxq_ready(hw, pf_q, &qrx_ctrl);
	if (err) {
		device_printf(dev,
			      "Rx queue %d is not ready\n",
			      pf_q);
		return err;
	}

	/* Skip if the queue is already in correct state */
	if (enable == !!(qrx_ctrl & QRX_CTRL_QENA_STAT_M))
		return (0);

	if (enable)
		qrx_ctrl |= QRX_CTRL_QENA_REQ_M;
	else
		qrx_ctrl &= ~QRX_CTRL_QENA_REQ_M;
	wr32(hw, QRX_CTRL(pf_q), qrx_ctrl);

	/* wait for the queue to finalize the request */
	err = ice_is_rxq_ready(hw, pf_q, &qrx_ctrl);
	if (err) {
		device_printf(dev,
			      "Rx queue %d %sable timeout\n",
			      pf_q, (enable ? "en" : "dis"));
		return err;
	}

	/* this should never happen */
	if (enable != !!(qrx_ctrl & QRX_CTRL_QENA_STAT_M)) {
		device_printf(dev,
			      "Rx queue %d invalid state\n",
			      pf_q);
		return (EDOOFUS);
	}

	return (0);
}

/**
 * ice_control_all_rx_queues - Configure hardware to start or stop the Rx queues
 * @vsi: VSI to enable/disable queues
 * @enable: true to enable queues, false to disable
 *
 * Control the Rx queues through the QRX_CTRL register, enabling or disabling
 * them. Wait for the appropriate time to ensure that the queues have actually
 * reached the expected state.
 */
int
ice_control_all_rx_queues(struct ice_vsi *vsi, bool enable)
{
	int i, err;

	/* TODO: amortize waits by changing all queues up front and then
	 * checking their status afterwards. This will become more necessary
	 * when we have a large number of queues.
	 */
	for (i = 0; i < vsi->num_rx_queues; i++) {
		err = ice_control_rx_queue(vsi, i, enable);
		if (err)
			break;
	}

	return (0);
}

/**
 * ice_add_mac_to_list - Add MAC filter to a MAC filter list
 * @vsi: the VSI to forward to
 * @list: list which contains MAC filter entries
 * @addr: the MAC address to be added
 * @action: filter action to perform on match
 *
 * Adds a MAC address filter to the list which will be forwarded to firmware
 * to add a series of MAC address filters.
 *
 * Returns 0 on success, and an error code on failure.
 *
 */
static int
ice_add_mac_to_list(struct ice_vsi *vsi, struct ice_list_head *list,
		    const u8 *addr, enum ice_sw_fwd_act_type action)
{
	struct ice_fltr_list_entry *entry;

	entry = (__typeof(entry))malloc(sizeof(*entry), M_ICE, M_NOWAIT|M_ZERO);
	if (!entry)
		return (ENOMEM);

	entry->fltr_info.flag = ICE_FLTR_TX;
	entry->fltr_info.src_id = ICE_SRC_ID_VSI;
	entry->fltr_info.lkup_type = ICE_SW_LKUP_MAC;
	entry->fltr_info.fltr_act = action;
	entry->fltr_info.vsi_handle = vsi->idx;
	bcopy(addr, entry->fltr_info.l_data.mac.mac_addr, ETHER_ADDR_LEN);

	LIST_ADD(&entry->list_entry, list);

	return 0;
}

/**
 * ice_free_fltr_list - Free memory associated with a MAC address list
 * @list: the list to free
 *
 * Free the memory of each entry associated with the list.
 */
static void
ice_free_fltr_list(struct ice_list_head *list)
{
	struct ice_fltr_list_entry *e, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(e, tmp, list, ice_fltr_list_entry, list_entry) {
		LIST_DEL(&e->list_entry);
		free(e, M_ICE);
	}
}

/**
 * ice_add_vsi_mac_filter - Add a MAC address filter for a VSI
 * @vsi: the VSI to add the filter for
 * @addr: MAC address to add a filter for
 *
 * Add a MAC address filter for a given VSI. This is a wrapper around
 * ice_add_mac to simplify the interface. First, it only accepts a single
 * address, so we don't have to mess around with the list setup in other
 * functions. Second, it ignores the ICE_ERR_ALREADY_EXISTS error, so that
 * callers don't need to worry about attempting to add the same filter twice.
 */
int
ice_add_vsi_mac_filter(struct ice_vsi *vsi, const u8 *addr)
{
	struct ice_list_head mac_addr_list;
	struct ice_hw *hw = &vsi->sc->hw;
	device_t dev = vsi->sc->dev;
	enum ice_status status;
	int err = 0;

	INIT_LIST_HEAD(&mac_addr_list);

	err = ice_add_mac_to_list(vsi, &mac_addr_list, addr, ICE_FWD_TO_VSI);
	if (err)
		goto free_mac_list;

	status = ice_add_mac(hw, &mac_addr_list);
	if (status == ICE_ERR_ALREADY_EXISTS) {
		; /* Don't complain if we try to add a filter that already exists */
	} else if (status) {
		device_printf(dev,
			      "Failed to add a filter for MAC %6D, err %s aq_err %s\n",
			      addr, ":",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		err = (EIO);
	}

free_mac_list:
	ice_free_fltr_list(&mac_addr_list);
	return err;
}

/**
 * ice_cfg_pf_default_mac_filters - Setup default unicast and broadcast addrs
 * @sc: device softc structure
 *
 * Program the default unicast and broadcast filters for the PF VSI.
 */
int
ice_cfg_pf_default_mac_filters(struct ice_softc *sc)
{
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;
	int err;

	/* Add the LAN MAC address */
	err = ice_add_vsi_mac_filter(vsi, hw->port_info->mac.lan_addr);
	if (err)
		return err;

	/* Add the broadcast address */
	err = ice_add_vsi_mac_filter(vsi, broadcastaddr);
	if (err)
		return err;

	return (0);
}

/**
 * ice_remove_vsi_mac_filter - Remove a MAC address filter for a VSI
 * @vsi: the VSI to add the filter for
 * @addr: MAC address to remove a filter for
 *
 * Remove a MAC address filter from a given VSI. This is a wrapper around
 * ice_remove_mac to simplify the interface. First, it only accepts a single
 * address, so we don't have to mess around with the list setup in other
 * functions. Second, it ignores the ICE_ERR_DOES_NOT_EXIST error, so that
 * callers don't need to worry about attempting to remove filters which
 * haven't yet been added.
 */
int
ice_remove_vsi_mac_filter(struct ice_vsi *vsi, const u8 *addr)
{
	struct ice_list_head mac_addr_list;
	struct ice_hw *hw = &vsi->sc->hw;
	device_t dev = vsi->sc->dev;
	enum ice_status status;
	int err = 0;

	INIT_LIST_HEAD(&mac_addr_list);

	err = ice_add_mac_to_list(vsi, &mac_addr_list, addr, ICE_FWD_TO_VSI);
	if (err)
		goto free_mac_list;

	status = ice_remove_mac(hw, &mac_addr_list);
	if (status == ICE_ERR_DOES_NOT_EXIST) {
		; /* Don't complain if we try to remove a filter that doesn't exist */
	} else if (status) {
		device_printf(dev,
			      "Failed to remove a filter for MAC %6D, err %s aq_err %s\n",
			      addr, ":",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		err = (EIO);
	}

free_mac_list:
	ice_free_fltr_list(&mac_addr_list);
	return err;
}

/**
 * ice_rm_pf_default_mac_filters - Remove default unicast and broadcast addrs
 * @sc: device softc structure
 *
 * Remove the default unicast and broadcast filters from the PF VSI.
 */
int
ice_rm_pf_default_mac_filters(struct ice_softc *sc)
{
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;
	int err;

	/* Remove the LAN MAC address */
	err = ice_remove_vsi_mac_filter(vsi, hw->port_info->mac.lan_addr);
	if (err)
		return err;

	/* Remove the broadcast address */
	err = ice_remove_vsi_mac_filter(vsi, broadcastaddr);
	if (err)
		return (EIO);

	return (0);
}

/**
 * ice_check_ctrlq_errors - Check for and report controlq errors
 * @sc: device private structure
 * @qname: name of the controlq
 * @cq: the controlq to check
 *
 * Check and report controlq errors. Currently all we do is report them to the
 * kernel message log, but we might want to improve this in the future, such
 * as to keep track of statistics.
 */
static void
ice_check_ctrlq_errors(struct ice_softc *sc, const char *qname,
		       struct ice_ctl_q_info *cq)
{
	struct ice_hw *hw = &sc->hw;
	u32 val;

	/* Check for error indications. Note that all the controlqs use the
	 * same register layout, so we use the PF_FW_AxQLEN defines only.
	 */
	val = rd32(hw, cq->rq.len);
	if (val & (PF_FW_ARQLEN_ARQVFE_M | PF_FW_ARQLEN_ARQOVFL_M |
		   PF_FW_ARQLEN_ARQCRIT_M)) {
		if (val & PF_FW_ARQLEN_ARQVFE_M)
			device_printf(sc->dev,
				"%s Receive Queue VF Error detected\n", qname);
		if (val & PF_FW_ARQLEN_ARQOVFL_M)
			device_printf(sc->dev,
				"%s Receive Queue Overflow Error detected\n",
				qname);
		if (val & PF_FW_ARQLEN_ARQCRIT_M)
			device_printf(sc->dev,
				"%s Receive Queue Critical Error detected\n",
				qname);
		val &= ~(PF_FW_ARQLEN_ARQVFE_M | PF_FW_ARQLEN_ARQOVFL_M |
			 PF_FW_ARQLEN_ARQCRIT_M);
		wr32(hw, cq->rq.len, val);
	}

	val = rd32(hw, cq->sq.len);
	if (val & (PF_FW_ATQLEN_ATQVFE_M | PF_FW_ATQLEN_ATQOVFL_M |
		   PF_FW_ATQLEN_ATQCRIT_M)) {
		if (val & PF_FW_ATQLEN_ATQVFE_M)
			device_printf(sc->dev,
				"%s Send Queue VF Error detected\n", qname);
		if (val & PF_FW_ATQLEN_ATQOVFL_M)
			device_printf(sc->dev,
				"%s Send Queue Overflow Error detected\n",
				qname);
		if (val & PF_FW_ATQLEN_ATQCRIT_M)
			device_printf(sc->dev,
				"%s Send Queue Critical Error detected\n",
				qname);
		val &= ~(PF_FW_ATQLEN_ATQVFE_M | PF_FW_ATQLEN_ATQOVFL_M |
			 PF_FW_ATQLEN_ATQCRIT_M);
		wr32(hw, cq->sq.len, val);
	}
}

/**
 * ice_process_link_event - Process a link event indication from firmware
 * @sc: device softc structure
 * @e: the received event data
 *
 * Gets the current link status from hardware, and may print a message if an
 * unqualified is detected.
 */
static void
ice_process_link_event(struct ice_softc *sc,
		       struct ice_rq_event_info __invariant_only *e)
{
	struct ice_port_info *pi = sc->hw.port_info;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;

	/* Sanity check that the data length isn't too small */
	MPASS(le16toh(e->desc.datalen) >= ICE_GET_LINK_STATUS_DATALEN_V1);

	/*
	 * Even though the adapter gets link status information inside the
	 * event, it needs to send a Get Link Status AQ command in order
	 * to re-enable link events.
	 */
	pi->phy.get_link_info = true;
	ice_get_link_status(pi, &sc->link_up);

	if (pi->phy.link_info.topo_media_conflict &
	   (ICE_AQ_LINK_TOPO_CONFLICT | ICE_AQ_LINK_MEDIA_CONFLICT |
	    ICE_AQ_LINK_TOPO_CORRUPT))
		device_printf(dev,
		    "Possible mis-configuration of the Ethernet port detected; please use the Intel (R) Ethernet Port Configuration Tool utility to address the issue.\n");

	if ((pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE) &&
	    !(pi->phy.link_info.link_info & ICE_AQ_LINK_UP)) {
		if (!(pi->phy.link_info.an_info & ICE_AQ_QUALIFIED_MODULE))
			device_printf(dev,
			    "Link is disabled on this device because an unsupported module type was detected! Refer to the Intel (R) Ethernet Adapters and Devices User Guide for a list of supported modules.\n");
		if (pi->phy.link_info.link_cfg_err & ICE_AQ_LINK_MODULE_POWER_UNSUPPORTED)
			device_printf(dev,
			    "The module's power requirements exceed the device's power supply. Cannot start link.\n");
		if (pi->phy.link_info.link_cfg_err & ICE_AQ_LINK_INVAL_MAX_POWER_LIMIT)
			device_printf(dev,
			    "The installed module is incompatible with the device's NVM image. Cannot start link.\n");
	}

	if (!(pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE)) {
		if (!ice_testandset_state(&sc->state, ICE_STATE_NO_MEDIA)) {
			status = ice_aq_set_link_restart_an(pi, false, NULL);
			if (status != ICE_SUCCESS)
				device_printf(dev,
				    "%s: ice_aq_set_link_restart_an: status %s, aq_err %s\n",
				    __func__, ice_status_str(status),
				    ice_aq_str(hw->adminq.sq_last_status));
		}
	}
	/* ICE_STATE_NO_MEDIA is cleared when polling task detects media */

	/* Indicate that link status must be reported again */
	ice_clear_state(&sc->state, ICE_STATE_LINK_STATUS_REPORTED);

	/* OS link info is updated elsewhere */
}

/**
 * ice_process_ctrlq_event - Respond to a controlq event
 * @sc: device private structure
 * @qname: the name for this controlq
 * @event: the event to process
 *
 * Perform actions in response to various controlq event notifications.
 */
static void
ice_process_ctrlq_event(struct ice_softc *sc, const char *qname,
			struct ice_rq_event_info *event)
{
	u16 opcode;

	opcode = le16toh(event->desc.opcode);

	switch (opcode) {
	case ice_aqc_opc_get_link_status:
		ice_process_link_event(sc, event);
		break;
	case ice_mbx_opc_send_msg_to_pf:
		/* TODO: handle IOV event */
		break;
	case ice_aqc_opc_fw_logs_event:
		ice_handle_fw_log_event(sc, &event->desc, event->msg_buf);
		break;
	case ice_aqc_opc_lldp_set_mib_change:
		ice_handle_mib_change_event(sc, event);
		break;
	case ice_aqc_opc_event_lan_overflow:
		ice_handle_lan_overflow_event(sc, event);
		break;
	case ice_aqc_opc_get_health_status:
		ice_handle_health_status_event(sc, event);
		break;
	default:
		device_printf(sc->dev,
			      "%s Receive Queue unhandled event 0x%04x ignored\n",
			      qname, opcode);
	}
}

/**
 * ice_process_ctrlq - helper function to process controlq rings
 * @sc: device private structure
 * @q_type: specific control queue type
 * @pending: return parameter to track remaining events
 *
 * Process controlq events for a given control queue type. Returns zero on
 * success, and an error code on failure. If successful, pending is the number
 * of remaining events left in the queue.
 */
int
ice_process_ctrlq(struct ice_softc *sc, enum ice_ctl_q q_type, u16 *pending)
{
	struct ice_rq_event_info event = { { 0 } };
	struct ice_hw *hw = &sc->hw;
	struct ice_ctl_q_info *cq;
	enum ice_status status;
	const char *qname;
	int loop = 0;

	switch (q_type) {
	case ICE_CTL_Q_ADMIN:
		cq = &hw->adminq;
		qname = "Admin";
		break;
	case ICE_CTL_Q_MAILBOX:
		cq = &hw->mailboxq;
		qname = "Mailbox";
		break;
	default:
		device_printf(sc->dev,
			      "Unknown control queue type 0x%x\n",
			      q_type);
		return 0;
	}

	ice_check_ctrlq_errors(sc, qname, cq);

	/*
	 * Control queue processing happens during the admin task which may be
	 * holding a non-sleepable lock, so we *must* use M_NOWAIT here.
	 */
	event.buf_len = cq->rq_buf_size;
	event.msg_buf = (u8 *)malloc(event.buf_len, M_ICE, M_ZERO | M_NOWAIT);
	if (!event.msg_buf) {
		device_printf(sc->dev,
			      "Unable to allocate memory for %s Receive Queue event\n",
			      qname);
		return (ENOMEM);
	}

	do {
		status = ice_clean_rq_elem(hw, cq, &event, pending);
		if (status == ICE_ERR_AQ_NO_WORK)
			break;
		if (status) {
			if (q_type == ICE_CTL_Q_ADMIN)
				device_printf(sc->dev,
					      "%s Receive Queue event error %s\n",
					      qname, ice_status_str(status));
			else
				device_printf(sc->dev,
					      "%s Receive Queue event error %s\n",
					      qname, ice_status_str(status));
			free(event.msg_buf, M_ICE);
			return (EIO);
		}
		/* XXX should we separate this handler by controlq type? */
		ice_process_ctrlq_event(sc, qname, &event);
	} while (*pending && (++loop < ICE_CTRLQ_WORK_LIMIT));

	free(event.msg_buf, M_ICE);

	return 0;
}

/**
 * pkg_ver_empty - Check if a package version is empty
 * @pkg_ver: the package version to check
 * @pkg_name: the package name to check
 *
 * Checks if the package version structure is empty. We consider a package
 * version as empty if none of the versions are non-zero and the name string
 * is null as well.
 *
 * This is used to check if the package version was initialized by the driver,
 * as we do not expect an actual DDP package file to have a zero'd version and
 * name.
 *
 * @returns true if the package version is valid, or false otherwise.
 */
static bool
pkg_ver_empty(struct ice_pkg_ver *pkg_ver, u8 *pkg_name)
{
	return (pkg_name[0] == '\0' &&
		pkg_ver->major == 0 &&
		pkg_ver->minor == 0 &&
		pkg_ver->update == 0 &&
		pkg_ver->draft == 0);
}

/**
 * pkg_ver_compatible - Check if the package version is compatible
 * @pkg_ver: the package version to check
 *
 * Compares the package version number to the driver's expected major/minor
 * version. Returns an integer indicating whether the version is older, newer,
 * or compatible with the driver.
 *
 * @returns 0 if the package version is compatible, -1 if the package version
 * is older, and 1 if the package version is newer than the driver version.
 */
static int
pkg_ver_compatible(struct ice_pkg_ver *pkg_ver)
{
	if (pkg_ver->major > ICE_PKG_SUPP_VER_MAJ)
		return (1); /* newer */
	else if ((pkg_ver->major == ICE_PKG_SUPP_VER_MAJ) &&
		 (pkg_ver->minor > ICE_PKG_SUPP_VER_MNR))
		return (1); /* newer */
	else if ((pkg_ver->major == ICE_PKG_SUPP_VER_MAJ) &&
		 (pkg_ver->minor == ICE_PKG_SUPP_VER_MNR))
		return (0); /* compatible */
	else
		return (-1); /* older */
}

/**
 * ice_os_pkg_version_str - Format OS package version info into a sbuf
 * @hw: device hw structure
 * @buf: string buffer to store name/version string
 *
 * Formats the name and version of the OS DDP package as found in the ice_ddp
 * module into a string.
 *
 * @remark This will almost always be the same as the active package, but
 * could be different in some cases. Use ice_active_pkg_version_str to get the
 * version of the active DDP package.
 */
static void
ice_os_pkg_version_str(struct ice_hw *hw, struct sbuf *buf)
{
	char name_buf[ICE_PKG_NAME_SIZE];

	/* If the OS DDP package info is empty, use "None" */
	if (pkg_ver_empty(&hw->pkg_ver, hw->pkg_name)) {
		sbuf_printf(buf, "None");
		return;
	}

	/*
	 * This should already be null-terminated, but since this is a raw
	 * value from an external source, strlcpy() into a new buffer to
	 * make sure.
	 */
	bzero(name_buf, sizeof(name_buf));
	strlcpy(name_buf, (char *)hw->pkg_name, ICE_PKG_NAME_SIZE);

	sbuf_printf(buf, "%s version %u.%u.%u.%u",
	    name_buf,
	    hw->pkg_ver.major,
	    hw->pkg_ver.minor,
	    hw->pkg_ver.update,
	    hw->pkg_ver.draft);
}

/**
 * ice_active_pkg_version_str - Format active package version info into a sbuf
 * @hw: device hw structure
 * @buf: string buffer to store name/version string
 *
 * Formats the name and version of the active DDP package info into a string
 * buffer for use.
 */
static void
ice_active_pkg_version_str(struct ice_hw *hw, struct sbuf *buf)
{
	char name_buf[ICE_PKG_NAME_SIZE];

	/* If the active DDP package info is empty, use "None" */
	if (pkg_ver_empty(&hw->active_pkg_ver, hw->active_pkg_name)) {
		sbuf_printf(buf, "None");
		return;
	}

	/*
	 * This should already be null-terminated, but since this is a raw
	 * value from an external source, strlcpy() into a new buffer to
	 * make sure.
	 */
	bzero(name_buf, sizeof(name_buf));
	strlcpy(name_buf, (char *)hw->active_pkg_name, ICE_PKG_NAME_SIZE);

	sbuf_printf(buf, "%s version %u.%u.%u.%u",
	    name_buf,
	    hw->active_pkg_ver.major,
	    hw->active_pkg_ver.minor,
	    hw->active_pkg_ver.update,
	    hw->active_pkg_ver.draft);

	if (hw->active_track_id != 0)
		sbuf_printf(buf, ", track id 0x%08x", hw->active_track_id);
}

/**
 * ice_nvm_version_str - Format the NVM version information into a sbuf
 * @hw: device hw structure
 * @buf: string buffer to store version string
 *
 * Formats the NVM information including firmware version, API version, NVM
 * version, the EETRACK id, and OEM specific version information into a string
 * buffer.
 */
static void
ice_nvm_version_str(struct ice_hw *hw, struct sbuf *buf)
{
	struct ice_nvm_info *nvm = &hw->flash.nvm;
	struct ice_orom_info *orom = &hw->flash.orom;
	struct ice_netlist_info *netlist = &hw->flash.netlist;

	/* Note that the netlist versions are stored in packed Binary Coded
	 * Decimal format. The use of '%x' will correctly display these as
	 * decimal numbers. This works because every 4 bits will be displayed
	 * as a hexadecimal digit, and the BCD format will only use the values
	 * 0-9.
	 */
	sbuf_printf(buf,
		    "fw %u.%u.%u api %u.%u nvm %x.%02x etid %08x netlist %x.%x.%x-%x.%x.%x.%04x oem %u.%u.%u",
		    hw->fw_maj_ver, hw->fw_min_ver, hw->fw_patch,
		    hw->api_maj_ver, hw->api_min_ver,
		    nvm->major, nvm->minor, nvm->eetrack,
		    netlist->major, netlist->minor,
		    netlist->type >> 16, netlist->type & 0xFFFF,
		    netlist->rev, netlist->cust_ver, netlist->hash,
		    orom->major, orom->build, orom->patch);
}

/**
 * ice_print_nvm_version - Print the NVM info to the kernel message log
 * @sc: the device softc structure
 *
 * Format and print an NVM version string using ice_nvm_version_str().
 */
void
ice_print_nvm_version(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct sbuf *sbuf;

	sbuf = sbuf_new_auto();
	ice_nvm_version_str(hw, sbuf);
	sbuf_finish(sbuf);
	device_printf(dev, "%s\n", sbuf_data(sbuf));
	sbuf_delete(sbuf);
}

/**
 * ice_update_vsi_hw_stats - Update VSI-specific ethernet statistics counters
 * @vsi: the VSI to be updated
 *
 * Reads hardware stats and updates the ice_vsi_hw_stats tracking structure with
 * the updated values.
 */
void
ice_update_vsi_hw_stats(struct ice_vsi *vsi)
{
	struct ice_eth_stats *prev_es, *cur_es;
	struct ice_hw *hw = &vsi->sc->hw;
	u16 vsi_num;

	if (!ice_is_vsi_valid(hw, vsi->idx))
		return;

	vsi_num = ice_get_hw_vsi_num(hw, vsi->idx); /* HW absolute index of a VSI */
	prev_es = &vsi->hw_stats.prev;
	cur_es = &vsi->hw_stats.cur;

#define ICE_VSI_STAT40(name, location) \
	ice_stat_update40(hw, name ## L(vsi_num), \
			  vsi->hw_stats.offsets_loaded, \
			  &prev_es->location, &cur_es->location)

#define ICE_VSI_STAT32(name, location) \
	ice_stat_update32(hw, name(vsi_num), \
			  vsi->hw_stats.offsets_loaded, \
			  &prev_es->location, &cur_es->location)

	ICE_VSI_STAT40(GLV_GORC, rx_bytes);
	ICE_VSI_STAT40(GLV_UPRC, rx_unicast);
	ICE_VSI_STAT40(GLV_MPRC, rx_multicast);
	ICE_VSI_STAT40(GLV_BPRC, rx_broadcast);
	ICE_VSI_STAT32(GLV_RDPC, rx_discards);
	ICE_VSI_STAT40(GLV_GOTC, tx_bytes);
	ICE_VSI_STAT40(GLV_UPTC, tx_unicast);
	ICE_VSI_STAT40(GLV_MPTC, tx_multicast);
	ICE_VSI_STAT40(GLV_BPTC, tx_broadcast);
	ICE_VSI_STAT32(GLV_TEPC, tx_errors);

	ice_stat_update_repc(hw, vsi->idx, vsi->hw_stats.offsets_loaded,
			     cur_es);

#undef ICE_VSI_STAT40
#undef ICE_VSI_STAT32

	vsi->hw_stats.offsets_loaded = true;
}

/**
 * ice_reset_vsi_stats - Reset VSI statistics counters
 * @vsi: VSI structure
 *
 * Resets the software tracking counters for the VSI statistics, and indicate
 * that the offsets haven't been loaded. This is intended to be called
 * post-reset so that VSI statistics count from zero again.
 */
void
ice_reset_vsi_stats(struct ice_vsi *vsi)
{
	/* Reset HW stats */
	memset(&vsi->hw_stats.prev, 0, sizeof(vsi->hw_stats.prev));
	memset(&vsi->hw_stats.cur, 0, sizeof(vsi->hw_stats.cur));
	vsi->hw_stats.offsets_loaded = false;
}

/**
 * ice_update_pf_stats - Update port stats counters
 * @sc: device private softc structure
 *
 * Reads hardware statistics registers and updates the software tracking
 * structure with new values.
 */
void
ice_update_pf_stats(struct ice_softc *sc)
{
	struct ice_hw_port_stats *prev_ps, *cur_ps;
	struct ice_hw *hw = &sc->hw;
	u8 lport;

	MPASS(hw->port_info);

	prev_ps = &sc->stats.prev;
	cur_ps = &sc->stats.cur;
	lport = hw->port_info->lport;

#define ICE_PF_STAT_PFC(name, location, index) \
	ice_stat_update40(hw, name(lport, index), \
			  sc->stats.offsets_loaded, \
			  &prev_ps->location[index], &cur_ps->location[index])

#define ICE_PF_STAT40(name, location) \
	ice_stat_update40(hw, name ## L(lport), \
			  sc->stats.offsets_loaded, \
			  &prev_ps->location, &cur_ps->location)

#define ICE_PF_STAT32(name, location) \
	ice_stat_update32(hw, name(lport), \
			  sc->stats.offsets_loaded, \
			  &prev_ps->location, &cur_ps->location)

	ICE_PF_STAT40(GLPRT_GORC, eth.rx_bytes);
	ICE_PF_STAT40(GLPRT_UPRC, eth.rx_unicast);
	ICE_PF_STAT40(GLPRT_MPRC, eth.rx_multicast);
	ICE_PF_STAT40(GLPRT_BPRC, eth.rx_broadcast);
	ICE_PF_STAT40(GLPRT_GOTC, eth.tx_bytes);
	ICE_PF_STAT40(GLPRT_UPTC, eth.tx_unicast);
	ICE_PF_STAT40(GLPRT_MPTC, eth.tx_multicast);
	ICE_PF_STAT40(GLPRT_BPTC, eth.tx_broadcast);
	/* This stat register doesn't have an lport */
	ice_stat_update32(hw, PRTRPB_RDPC,
			  sc->stats.offsets_loaded,
			  &prev_ps->eth.rx_discards, &cur_ps->eth.rx_discards);

	ICE_PF_STAT32(GLPRT_TDOLD, tx_dropped_link_down);
	ICE_PF_STAT40(GLPRT_PRC64, rx_size_64);
	ICE_PF_STAT40(GLPRT_PRC127, rx_size_127);
	ICE_PF_STAT40(GLPRT_PRC255, rx_size_255);
	ICE_PF_STAT40(GLPRT_PRC511, rx_size_511);
	ICE_PF_STAT40(GLPRT_PRC1023, rx_size_1023);
	ICE_PF_STAT40(GLPRT_PRC1522, rx_size_1522);
	ICE_PF_STAT40(GLPRT_PRC9522, rx_size_big);
	ICE_PF_STAT40(GLPRT_PTC64, tx_size_64);
	ICE_PF_STAT40(GLPRT_PTC127, tx_size_127);
	ICE_PF_STAT40(GLPRT_PTC255, tx_size_255);
	ICE_PF_STAT40(GLPRT_PTC511, tx_size_511);
	ICE_PF_STAT40(GLPRT_PTC1023, tx_size_1023);
	ICE_PF_STAT40(GLPRT_PTC1522, tx_size_1522);
	ICE_PF_STAT40(GLPRT_PTC9522, tx_size_big);

	/* Update Priority Flow Control Stats */
	for (int i = 0; i <= GLPRT_PXOFFRXC_MAX_INDEX; i++) {
		ICE_PF_STAT_PFC(GLPRT_PXONRXC, priority_xon_rx, i);
		ICE_PF_STAT_PFC(GLPRT_PXOFFRXC, priority_xoff_rx, i);
		ICE_PF_STAT_PFC(GLPRT_PXONTXC, priority_xon_tx, i);
		ICE_PF_STAT_PFC(GLPRT_PXOFFTXC, priority_xoff_tx, i);
		ICE_PF_STAT_PFC(GLPRT_RXON2OFFCNT, priority_xon_2_xoff, i);
	}

	ICE_PF_STAT32(GLPRT_LXONRXC, link_xon_rx);
	ICE_PF_STAT32(GLPRT_LXOFFRXC, link_xoff_rx);
	ICE_PF_STAT32(GLPRT_LXONTXC, link_xon_tx);
	ICE_PF_STAT32(GLPRT_LXOFFTXC, link_xoff_tx);
	ICE_PF_STAT32(GLPRT_CRCERRS, crc_errors);
	ICE_PF_STAT32(GLPRT_ILLERRC, illegal_bytes);
	ICE_PF_STAT32(GLPRT_MLFC, mac_local_faults);
	ICE_PF_STAT32(GLPRT_MRFC, mac_remote_faults);
	ICE_PF_STAT32(GLPRT_RLEC, rx_len_errors);
	ICE_PF_STAT32(GLPRT_RUC, rx_undersize);
	ICE_PF_STAT32(GLPRT_RFC, rx_fragments);
	ICE_PF_STAT32(GLPRT_ROC, rx_oversize);
	ICE_PF_STAT32(GLPRT_RJC, rx_jabber);

#undef ICE_PF_STAT40
#undef ICE_PF_STAT32
#undef ICE_PF_STAT_PFC

	sc->stats.offsets_loaded = true;
}

/**
 * ice_reset_pf_stats - Reset port stats counters
 * @sc: Device private softc structure
 *
 * Reset software tracking values for statistics to zero, and indicate that
 * offsets haven't been loaded. Intended to be called after a device reset so
 * that statistics count from zero again.
 */
void
ice_reset_pf_stats(struct ice_softc *sc)
{
	memset(&sc->stats.prev, 0, sizeof(sc->stats.prev));
	memset(&sc->stats.cur, 0, sizeof(sc->stats.cur));
	sc->stats.offsets_loaded = false;
}

/**
 * ice_sysctl_show_fw - sysctl callback to show firmware information
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for the fw_version sysctl, to display the current firmware
 * information found at hardware init time.
 */
static int
ice_sysctl_show_fw(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct sbuf *sbuf;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	ice_nvm_version_str(hw, sbuf);
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_pba_number - sysctl callback to show PBA number
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for the pba_number sysctl, used to read the Product Board Assembly
 * number for this device.
 */
static int
ice_sysctl_pba_number(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u8 pba_string[32] = "";
	enum ice_status status;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	status = ice_read_pba_string(hw, pba_string, sizeof(pba_string));
	if (status) {
		device_printf(dev,
		    "%s: failed to read PBA string from NVM; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	return sysctl_handle_string(oidp, pba_string, sizeof(pba_string), req);
}

/**
 * ice_sysctl_pkg_version - sysctl to show the active package version info
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for the pkg_version sysctl, to display the active DDP package name
 * and version information.
 */
static int
ice_sysctl_pkg_version(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct sbuf *sbuf;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	ice_active_pkg_version_str(hw, sbuf);
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_os_pkg_version - sysctl to show the OS package version info
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for the pkg_version sysctl, to display the OS DDP package name and
 * version info found in the ice_ddp module.
 */
static int
ice_sysctl_os_pkg_version(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct sbuf *sbuf;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	ice_os_pkg_version_str(hw, sbuf);
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_current_speed - sysctl callback to show current link speed
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for the current_speed sysctl, to display the string representing
 * the current link speed.
 */
static int
ice_sysctl_current_speed(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct sbuf *sbuf;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 10, req);
	sbuf_printf(sbuf, "%s", ice_aq_speed_to_str(hw->port_info));
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * @var phy_link_speeds
 * @brief PHY link speed conversion array
 *
 * Array of link speeds to convert ICE_PHY_TYPE_LOW and ICE_PHY_TYPE_HIGH into
 * link speeds used by the link speed sysctls.
 *
 * @remark these are based on the indices used in the BIT() macros for the
 * ICE_PHY_TYPE_LOW_* and ICE_PHY_TYPE_HIGH_* definitions.
 */
static const uint16_t phy_link_speeds[] = {
    ICE_AQ_LINK_SPEED_100MB,
    ICE_AQ_LINK_SPEED_100MB,
    ICE_AQ_LINK_SPEED_1000MB,
    ICE_AQ_LINK_SPEED_1000MB,
    ICE_AQ_LINK_SPEED_1000MB,
    ICE_AQ_LINK_SPEED_1000MB,
    ICE_AQ_LINK_SPEED_1000MB,
    ICE_AQ_LINK_SPEED_2500MB,
    ICE_AQ_LINK_SPEED_2500MB,
    ICE_AQ_LINK_SPEED_2500MB,
    ICE_AQ_LINK_SPEED_5GB,
    ICE_AQ_LINK_SPEED_5GB,
    ICE_AQ_LINK_SPEED_10GB,
    ICE_AQ_LINK_SPEED_10GB,
    ICE_AQ_LINK_SPEED_10GB,
    ICE_AQ_LINK_SPEED_10GB,
    ICE_AQ_LINK_SPEED_10GB,
    ICE_AQ_LINK_SPEED_10GB,
    ICE_AQ_LINK_SPEED_10GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_25GB,
    ICE_AQ_LINK_SPEED_40GB,
    ICE_AQ_LINK_SPEED_40GB,
    ICE_AQ_LINK_SPEED_40GB,
    ICE_AQ_LINK_SPEED_40GB,
    ICE_AQ_LINK_SPEED_40GB,
    ICE_AQ_LINK_SPEED_40GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_50GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    /* These rates are for ICE_PHY_TYPE_HIGH_* */
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB,
    ICE_AQ_LINK_SPEED_100GB
};

#define ICE_SYSCTL_HELP_ADVERTISE_SPEED		\
"\nControl advertised link speed."		\
"\nFlags:"					\
"\n\t   0x0 - Auto"				\
"\n\t   0x1 - 10 Mb"				\
"\n\t   0x2 - 100 Mb"				\
"\n\t   0x4 - 1G"				\
"\n\t   0x8 - 2.5G"				\
"\n\t  0x10 - 5G"				\
"\n\t  0x20 - 10G"				\
"\n\t  0x40 - 20G"				\
"\n\t  0x80 - 25G"				\
"\n\t 0x100 - 40G"				\
"\n\t 0x200 - 50G"				\
"\n\t 0x400 - 100G"				\
"\n\t0x8000 - Unknown"				\
"\n\t"						\
"\nUse \"sysctl -x\" to view flags properly."

#define ICE_PHYS_100MB			\
    (ICE_PHY_TYPE_LOW_100BASE_TX |	\
     ICE_PHY_TYPE_LOW_100M_SGMII)
#define ICE_PHYS_1000MB			\
    (ICE_PHY_TYPE_LOW_1000BASE_T |	\
     ICE_PHY_TYPE_LOW_1000BASE_SX |	\
     ICE_PHY_TYPE_LOW_1000BASE_LX |	\
     ICE_PHY_TYPE_LOW_1000BASE_KX |	\
     ICE_PHY_TYPE_LOW_1G_SGMII)
#define ICE_PHYS_2500MB			\
    (ICE_PHY_TYPE_LOW_2500BASE_T |	\
     ICE_PHY_TYPE_LOW_2500BASE_X |	\
     ICE_PHY_TYPE_LOW_2500BASE_KX)
#define ICE_PHYS_5GB			\
    (ICE_PHY_TYPE_LOW_5GBASE_T |	\
     ICE_PHY_TYPE_LOW_5GBASE_KR)
#define ICE_PHYS_10GB			\
    (ICE_PHY_TYPE_LOW_10GBASE_T |	\
     ICE_PHY_TYPE_LOW_10G_SFI_DA |	\
     ICE_PHY_TYPE_LOW_10GBASE_SR |	\
     ICE_PHY_TYPE_LOW_10GBASE_LR |	\
     ICE_PHY_TYPE_LOW_10GBASE_KR_CR1 |	\
     ICE_PHY_TYPE_LOW_10G_SFI_AOC_ACC |	\
     ICE_PHY_TYPE_LOW_10G_SFI_C2C)
#define ICE_PHYS_25GB			\
    (ICE_PHY_TYPE_LOW_25GBASE_T |	\
     ICE_PHY_TYPE_LOW_25GBASE_CR |	\
     ICE_PHY_TYPE_LOW_25GBASE_CR_S |	\
     ICE_PHY_TYPE_LOW_25GBASE_CR1 |	\
     ICE_PHY_TYPE_LOW_25GBASE_SR |	\
     ICE_PHY_TYPE_LOW_25GBASE_LR |	\
     ICE_PHY_TYPE_LOW_25GBASE_KR |	\
     ICE_PHY_TYPE_LOW_25GBASE_KR_S |	\
     ICE_PHY_TYPE_LOW_25GBASE_KR1 |	\
     ICE_PHY_TYPE_LOW_25G_AUI_AOC_ACC |	\
     ICE_PHY_TYPE_LOW_25G_AUI_C2C)
#define ICE_PHYS_40GB			\
    (ICE_PHY_TYPE_LOW_40GBASE_CR4 |	\
     ICE_PHY_TYPE_LOW_40GBASE_SR4 |	\
     ICE_PHY_TYPE_LOW_40GBASE_LR4 |	\
     ICE_PHY_TYPE_LOW_40GBASE_KR4 |	\
     ICE_PHY_TYPE_LOW_40G_XLAUI_AOC_ACC | \
     ICE_PHY_TYPE_LOW_40G_XLAUI)
#define ICE_PHYS_50GB			\
    (ICE_PHY_TYPE_LOW_50GBASE_CR2 |	\
     ICE_PHY_TYPE_LOW_50GBASE_SR2 |	\
     ICE_PHY_TYPE_LOW_50GBASE_LR2 |	\
     ICE_PHY_TYPE_LOW_50GBASE_KR2 |	\
     ICE_PHY_TYPE_LOW_50G_LAUI2_AOC_ACC | \
     ICE_PHY_TYPE_LOW_50G_LAUI2 |	\
     ICE_PHY_TYPE_LOW_50G_AUI2_AOC_ACC | \
     ICE_PHY_TYPE_LOW_50G_AUI2 |	\
     ICE_PHY_TYPE_LOW_50GBASE_CP |	\
     ICE_PHY_TYPE_LOW_50GBASE_SR |	\
     ICE_PHY_TYPE_LOW_50GBASE_FR |	\
     ICE_PHY_TYPE_LOW_50GBASE_LR |	\
     ICE_PHY_TYPE_LOW_50GBASE_KR_PAM4 |	\
     ICE_PHY_TYPE_LOW_50G_AUI1_AOC_ACC | \
     ICE_PHY_TYPE_LOW_50G_AUI1)
#define ICE_PHYS_100GB_LOW		\
    (ICE_PHY_TYPE_LOW_100GBASE_CR4 |	\
     ICE_PHY_TYPE_LOW_100GBASE_SR4 |	\
     ICE_PHY_TYPE_LOW_100GBASE_LR4 |	\
     ICE_PHY_TYPE_LOW_100GBASE_KR4 |	\
     ICE_PHY_TYPE_LOW_100G_CAUI4_AOC_ACC | \
     ICE_PHY_TYPE_LOW_100G_CAUI4 |	\
     ICE_PHY_TYPE_LOW_100G_AUI4_AOC_ACC | \
     ICE_PHY_TYPE_LOW_100G_AUI4 |	\
     ICE_PHY_TYPE_LOW_100GBASE_CR_PAM4 | \
     ICE_PHY_TYPE_LOW_100GBASE_KR_PAM4 | \
     ICE_PHY_TYPE_LOW_100GBASE_CP2 |	\
     ICE_PHY_TYPE_LOW_100GBASE_SR2 |	\
     ICE_PHY_TYPE_LOW_100GBASE_DR)
#define ICE_PHYS_100GB_HIGH		\
    (ICE_PHY_TYPE_HIGH_100GBASE_KR2_PAM4 | \
     ICE_PHY_TYPE_HIGH_100G_CAUI2_AOC_ACC | \
     ICE_PHY_TYPE_HIGH_100G_CAUI2 |	\
     ICE_PHY_TYPE_HIGH_100G_AUI2_AOC_ACC | \
     ICE_PHY_TYPE_HIGH_100G_AUI2)

/**
 * ice_aq_phy_types_to_link_speeds - Convert the PHY Types to speeds
 * @phy_type_low: lower 64-bit PHY Type bitmask
 * @phy_type_high: upper 64-bit PHY Type bitmask
 *
 * Convert the PHY Type fields from Get PHY Abilities and Set PHY Config into
 * link speed flags. If phy_type_high has an unknown PHY type, then the return
 * value will include the "ICE_AQ_LINK_SPEED_UNKNOWN" flag as well.
 */
static u16
ice_aq_phy_types_to_link_speeds(u64 phy_type_low, u64 phy_type_high)
{
	u16 sysctl_speeds = 0;
	int bit;

	/* coverity[address_of] */
	for_each_set_bit(bit, &phy_type_low, 64)
		sysctl_speeds |= phy_link_speeds[bit];

	/* coverity[address_of] */
	for_each_set_bit(bit, &phy_type_high, 64) {
		if ((bit + 64) < (int)ARRAY_SIZE(phy_link_speeds))
			sysctl_speeds |= phy_link_speeds[bit + 64];
		else
			sysctl_speeds |= ICE_AQ_LINK_SPEED_UNKNOWN;
	}

	return (sysctl_speeds);
}

/**
 * ice_sysctl_speeds_to_aq_phy_types - Convert sysctl speed flags to AQ PHY flags
 * @sysctl_speeds: 16-bit sysctl speeds or AQ_LINK_SPEED flags
 * @phy_type_low: output parameter for lower AQ PHY flags
 * @phy_type_high: output parameter for higher AQ PHY flags
 *
 * Converts the given link speed flags into AQ PHY type flag sets appropriate
 * for use in a Set PHY Config command.
 */
static void
ice_sysctl_speeds_to_aq_phy_types(u16 sysctl_speeds, u64 *phy_type_low,
				  u64 *phy_type_high)
{
	*phy_type_low = 0, *phy_type_high = 0;

	if (sysctl_speeds & ICE_AQ_LINK_SPEED_100MB)
		*phy_type_low |= ICE_PHYS_100MB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_1000MB)
		*phy_type_low |= ICE_PHYS_1000MB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_2500MB)
		*phy_type_low |= ICE_PHYS_2500MB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_5GB)
		*phy_type_low |= ICE_PHYS_5GB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_10GB)
		*phy_type_low |= ICE_PHYS_10GB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_25GB)
		*phy_type_low |= ICE_PHYS_25GB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_40GB)
		*phy_type_low |= ICE_PHYS_40GB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_50GB)
		*phy_type_low |= ICE_PHYS_50GB;
	if (sysctl_speeds & ICE_AQ_LINK_SPEED_100GB) {
		*phy_type_low |= ICE_PHYS_100GB_LOW;
		*phy_type_high |= ICE_PHYS_100GB_HIGH;
	}
}

/**
 * @struct ice_phy_data
 * @brief PHY caps and link speeds
 *
 * Buffer providing report mode and user speeds;
 * returning intersection of PHY types and speeds.
 */
struct ice_phy_data {
	u64 phy_low_orig;     /* PHY low quad from report */
	u64 phy_high_orig;    /* PHY high quad from report */
	u64 phy_low_intr;     /* PHY low quad intersection with user speeds */
	u64 phy_high_intr;    /* PHY high quad intersection with user speeds */
	u16 user_speeds_orig; /* Input from caller - See ICE_AQ_LINK_SPEED_* */
	u16 user_speeds_intr; /* Intersect with report speeds */
	u8 report_mode;       /* See ICE_AQC_REPORT_* */
};

/**
 * ice_intersect_phy_types_and_speeds - Return intersection of link speeds
 * @sc: device private structure
 * @phy_data: device PHY data
 *
 * On read: Displays the currently supported speeds
 * On write: Sets the device's supported speeds
 * Valid input flags: see ICE_SYSCTL_HELP_ADVERTISE_SPEED
 */
static int
ice_intersect_phy_types_and_speeds(struct ice_softc *sc,
				   struct ice_phy_data *phy_data)
{
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	const char *report_types[5] = { "w/o MEDIA",
					"w/MEDIA",
					"ACTIVE",
					"EDOOFUS", /* Not used */
					"DFLT" };
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi = hw->port_info;
	enum ice_status status;
	u16 report_speeds, temp_speeds;
	u8 report_type;
	bool apply_speed_filter = false;

	switch (phy_data->report_mode) {
	case ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA:
	case ICE_AQC_REPORT_TOPO_CAP_MEDIA:
	case ICE_AQC_REPORT_ACTIVE_CFG:
	case ICE_AQC_REPORT_DFLT_CFG:
		report_type = phy_data->report_mode >> 1;
		break;
	default:
		device_printf(sc->dev,
		    "%s: phy_data.report_mode \"%u\" doesn't exist\n",
		    __func__, phy_data->report_mode);
		return (EINVAL);
	}

	/* 0 is treated as "Auto"; the driver will handle selecting the
	 * correct speeds. Including, in some cases, applying an override
	 * if provided.
	 */
	if (phy_data->user_speeds_orig == 0)
		phy_data->user_speeds_orig = USHRT_MAX;
	else if (ice_is_bit_set(sc->feat_en, ICE_FEATURE_LENIENT_LINK_MODE))
		apply_speed_filter = true;

	status = ice_aq_get_phy_caps(pi, false, phy_data->report_mode, &pcaps, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(sc->dev,
		    "%s: ice_aq_get_phy_caps (%s) failed; status %s, aq_err %s\n",
		    __func__, report_types[report_type],
		    ice_status_str(status),
		    ice_aq_str(sc->hw.adminq.sq_last_status));
		return (EIO);
	}

	phy_data->phy_low_orig = le64toh(pcaps.phy_type_low);
	phy_data->phy_high_orig = le64toh(pcaps.phy_type_high);
	report_speeds = ice_aq_phy_types_to_link_speeds(phy_data->phy_low_orig,
	    phy_data->phy_high_orig);
	if (apply_speed_filter) {
		temp_speeds = ice_apply_supported_speed_filter(report_speeds,
		    pcaps.module_type[0]);
		if ((phy_data->user_speeds_orig & temp_speeds) == 0) {
			device_printf(sc->dev,
			    "User-specified speeds (\"0x%04X\") not supported\n",
			    phy_data->user_speeds_orig);
			return (EINVAL);
		}
		report_speeds = temp_speeds;
	}
	ice_sysctl_speeds_to_aq_phy_types(phy_data->user_speeds_orig,
	    &phy_data->phy_low_intr, &phy_data->phy_high_intr);
	phy_data->user_speeds_intr = phy_data->user_speeds_orig & report_speeds;
	phy_data->phy_low_intr &= phy_data->phy_low_orig;
	phy_data->phy_high_intr &= phy_data->phy_high_orig;

	return (0);
 }

/**
 * ice_sysctl_advertise_speed - Display/change link speeds supported by port
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the currently supported speeds
 * On write: Sets the device's supported speeds
 * Valid input flags: see ICE_SYSCTL_HELP_ADVERTISE_SPEED
 */
static int
ice_sysctl_advertise_speed(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_port_info *pi = sc->hw.port_info;
	struct ice_phy_data phy_data = { 0 };
	device_t dev = sc->dev;
	u16 sysctl_speeds;
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Get the current speeds from the adapter's "active" configuration. */
	phy_data.report_mode = ICE_AQC_REPORT_ACTIVE_CFG;
	ret = ice_intersect_phy_types_and_speeds(sc, &phy_data);
	if (ret) {
		/* Error message already printed within function */
		return (ret);
	}

	sysctl_speeds = phy_data.user_speeds_intr;

	ret = sysctl_handle_16(oidp, &sysctl_speeds, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	if (sysctl_speeds > 0x7FF) {
		device_printf(dev,
			      "%s: \"%u\" is outside of the range of acceptable values.\n",
			      __func__, sysctl_speeds);
		return (EINVAL);
	}

	pi->phy.curr_user_speed_req = sysctl_speeds;

	/* Apply settings requested by user */
	return ice_apply_saved_phy_cfg(sc, ICE_APPLY_LS);
}

#define ICE_SYSCTL_HELP_FEC_CONFIG			\
"\nDisplay or set the port's requested FEC mode."	\
"\n\tauto - " ICE_FEC_STRING_AUTO			\
"\n\tfc - " ICE_FEC_STRING_BASER			\
"\n\trs - " ICE_FEC_STRING_RS				\
"\n\tnone - " ICE_FEC_STRING_NONE			\
"\nEither of the left or right strings above can be used to set the requested mode."

/**
 * ice_sysctl_fec_config - Display/change the configured FEC mode
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the configured FEC mode
 * On write: Sets the device's FEC mode to the input string, if it's valid.
 * Valid input strings: see ICE_SYSCTL_HELP_FEC_CONFIG
 */
static int
ice_sysctl_fec_config(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_port_info *pi = sc->hw.port_info;
	enum ice_fec_mode new_mode;
	device_t dev = sc->dev;
	char req_fec[32];
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	bzero(req_fec, sizeof(req_fec));
	strlcpy(req_fec, ice_requested_fec_mode(pi), sizeof(req_fec));

	ret = sysctl_handle_string(oidp, req_fec, sizeof(req_fec), req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	if (strcmp(req_fec, "auto") == 0 ||
	    strcmp(req_fec, ice_fec_str(ICE_FEC_AUTO)) == 0) {
		if (sc->allow_no_fec_mod_in_auto)
			new_mode = ICE_FEC_DIS_AUTO;
		else
			new_mode = ICE_FEC_AUTO;
	} else if (strcmp(req_fec, "fc") == 0 ||
	    strcmp(req_fec, ice_fec_str(ICE_FEC_BASER)) == 0) {
		new_mode = ICE_FEC_BASER;
	} else if (strcmp(req_fec, "rs") == 0 ||
	    strcmp(req_fec, ice_fec_str(ICE_FEC_RS)) == 0) {
		new_mode = ICE_FEC_RS;
	} else if (strcmp(req_fec, "none") == 0 ||
	    strcmp(req_fec, ice_fec_str(ICE_FEC_NONE)) == 0) {
		new_mode = ICE_FEC_NONE;
	} else {
		device_printf(dev,
		    "%s: \"%s\" is not a valid FEC mode\n",
		    __func__, req_fec);
		return (EINVAL);
	}

	/* Cache user FEC mode for later link ups */
	pi->phy.curr_user_fec_req = new_mode;

	/* Apply settings requested by user */
	return ice_apply_saved_phy_cfg(sc, ICE_APPLY_FEC);
}

/**
 * ice_sysctl_negotiated_fec - Display the negotiated FEC mode on the link
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the negotiated FEC mode, in a string
 */
static int
ice_sysctl_negotiated_fec(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	char neg_fec[32];
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Copy const string into a buffer to drop const qualifier */
	bzero(neg_fec, sizeof(neg_fec));
	strlcpy(neg_fec, ice_negotiated_fec_mode(hw->port_info), sizeof(neg_fec));

	ret = sysctl_handle_string(oidp, neg_fec, 0, req);
	if (req->newptr != NULL)
		return (EPERM);

	return (ret);
}

#define ICE_SYSCTL_HELP_FC_CONFIG				\
"\nDisplay or set the port's advertised flow control mode.\n"	\
"\t0 - " ICE_FC_STRING_NONE					\
"\n\t1 - " ICE_FC_STRING_RX					\
"\n\t2 - " ICE_FC_STRING_TX					\
"\n\t3 - " ICE_FC_STRING_FULL					\
"\nEither the numbers or the strings above can be used to set the advertised mode."

/**
 * ice_sysctl_fc_config - Display/change the advertised flow control mode
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the configured flow control mode
 * On write: Sets the device's flow control mode to the input, if it's valid.
 * Valid input strings: see ICE_SYSCTL_HELP_FC_CONFIG
 */
static int
ice_sysctl_fc_config(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_port_info *pi = sc->hw.port_info;
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	enum ice_fc_mode old_mode, new_mode;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	int ret, fc_num;
	bool mode_set = false;
	struct sbuf buf;
	char *fc_str_end;
	char fc_str[32];

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG,
				     &pcaps, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_aq_get_phy_caps failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	/* Convert HW response format to SW enum value */
	if ((pcaps.caps & ICE_AQC_PHY_EN_TX_LINK_PAUSE) &&
	    (pcaps.caps & ICE_AQC_PHY_EN_RX_LINK_PAUSE))
		old_mode = ICE_FC_FULL;
	else if (pcaps.caps & ICE_AQC_PHY_EN_TX_LINK_PAUSE)
		old_mode = ICE_FC_TX_PAUSE;
	else if (pcaps.caps & ICE_AQC_PHY_EN_RX_LINK_PAUSE)
		old_mode = ICE_FC_RX_PAUSE;
	else
		old_mode = ICE_FC_NONE;

	/* Create "old" string for output */
	bzero(fc_str, sizeof(fc_str));
	sbuf_new_for_sysctl(&buf, fc_str, sizeof(fc_str), req);
	sbuf_printf(&buf, "%d<%s>", old_mode, ice_fc_str(old_mode));
	sbuf_finish(&buf);
	sbuf_delete(&buf);

	ret = sysctl_handle_string(oidp, fc_str, sizeof(fc_str), req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	/* Try to parse input as a string, first */
	if (strcasecmp(ice_fc_str(ICE_FC_FULL), fc_str) == 0) {
		new_mode = ICE_FC_FULL;
		mode_set = true;
	}
	else if (strcasecmp(ice_fc_str(ICE_FC_TX_PAUSE), fc_str) == 0) {
		new_mode = ICE_FC_TX_PAUSE;
		mode_set = true;
	}
	else if (strcasecmp(ice_fc_str(ICE_FC_RX_PAUSE), fc_str) == 0) {
		new_mode = ICE_FC_RX_PAUSE;
		mode_set = true;
	}
	else if (strcasecmp(ice_fc_str(ICE_FC_NONE), fc_str) == 0) {
		new_mode = ICE_FC_NONE;
		mode_set = true;
	}

	/*
	 * Then check if it's an integer, for compatibility with the method
	 * used in older drivers.
	 */
	if (!mode_set) {
		fc_num = strtol(fc_str, &fc_str_end, 0);
		if (fc_str_end == fc_str)
			fc_num = -1;
		switch (fc_num) {
		case 3:
			new_mode = ICE_FC_FULL;
			break;
		case 2:
			new_mode = ICE_FC_TX_PAUSE;
			break;
		case 1:
			new_mode = ICE_FC_RX_PAUSE;
			break;
		case 0:
			new_mode = ICE_FC_NONE;
			break;
		default:
			device_printf(dev,
			    "%s: \"%s\" is not a valid flow control mode\n",
			    __func__, fc_str);
			return (EINVAL);
		}
	}

	/* Save flow control mode from user */
	pi->phy.curr_user_fc_req = new_mode;

	/* Turn off Priority Flow Control when Link Flow Control is enabled */
	if ((hw->port_info->qos_cfg.is_sw_lldp) &&
	    (hw->port_info->qos_cfg.local_dcbx_cfg.pfc.pfcena != 0) &&
	    (new_mode != ICE_FC_NONE)) {
		ret = ice_config_pfc(sc, 0x0);
		if (ret)
			return (ret);
	}

	/* Apply settings requested by user */
	return ice_apply_saved_phy_cfg(sc, ICE_APPLY_FC);
}

/**
 * ice_sysctl_negotiated_fc - Display currently negotiated FC mode
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the currently negotiated flow control settings.
 *
 * If link is not established, this will report ICE_FC_NONE, as no flow
 * control is negotiated while link is down.
 */
static int
ice_sysctl_negotiated_fc(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_port_info *pi = sc->hw.port_info;
	const char *negotiated_fc;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	negotiated_fc = ice_flowcontrol_mode(pi);

	return sysctl_handle_string(oidp, __DECONST(char *, negotiated_fc), 0, req);
}

/**
 * __ice_sysctl_phy_type_handler - Display/change supported PHY types/speeds
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 * @is_phy_type_high: if true, handle the high PHY type instead of the low PHY type
 *
 * Private handler for phy_type_high and phy_type_low sysctls.
 */
static int
__ice_sysctl_phy_type_handler(SYSCTL_HANDLER_ARGS, bool is_phy_type_high)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	struct ice_aqc_set_phy_cfg_data cfg = { 0 };
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	uint64_t types;
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	status = ice_aq_get_phy_caps(hw->port_info, false, ICE_AQC_REPORT_ACTIVE_CFG,
				     &pcaps, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_aq_get_phy_caps failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	if (is_phy_type_high)
		types = pcaps.phy_type_high;
	else
		types = pcaps.phy_type_low;

	ret = sysctl_handle_64(oidp, &types, sizeof(types), req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	ice_copy_phy_caps_to_cfg(hw->port_info, &pcaps, &cfg);

	if (is_phy_type_high)
		cfg.phy_type_high = types & hw->port_info->phy.phy_type_high;
	else
		cfg.phy_type_low = types & hw->port_info->phy.phy_type_low;
	cfg.caps |= ICE_AQ_PHY_ENA_AUTO_LINK_UPDT;

	status = ice_aq_set_phy_cfg(hw, hw->port_info, &cfg, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_aq_set_phy_cfg failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	return (0);

}

/**
 * ice_sysctl_phy_type_low - Display/change supported lower PHY types/speeds
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the currently supported lower PHY types
 * On write: Sets the device's supported low PHY types
 */
static int
ice_sysctl_phy_type_low(SYSCTL_HANDLER_ARGS)
{
	return __ice_sysctl_phy_type_handler(oidp, arg1, arg2, req, false);
}

/**
 * ice_sysctl_phy_type_high - Display/change supported higher PHY types/speeds
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the currently supported higher PHY types
 * On write: Sets the device's supported high PHY types
 */
static int
ice_sysctl_phy_type_high(SYSCTL_HANDLER_ARGS)
{
	return __ice_sysctl_phy_type_handler(oidp, arg1, arg2, req, true);
}

/**
 * ice_sysctl_phy_caps - Display response from Get PHY abililties
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 * @report_mode: the mode to report
 *
 * On read: Display the response from Get PHY abillities with the given report
 * mode.
 */
static int
ice_sysctl_phy_caps(SYSCTL_HANDLER_ARGS, u8 report_mode)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi = hw->port_info;
	device_t dev = sc->dev;
	enum ice_status status;
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	ret = priv_check(curthread, PRIV_DRIVER);
	if (ret)
		return (ret);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	status = ice_aq_get_phy_caps(pi, true, report_mode, &pcaps, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_aq_get_phy_caps failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	ret = sysctl_handle_opaque(oidp, &pcaps, sizeof(pcaps), req);
	if (req->newptr != NULL)
		return (EPERM);

	return (ret);
}

/**
 * ice_sysctl_phy_sw_caps - Display response from Get PHY abililties
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Display the response from Get PHY abillities reporting the last
 * software configuration.
 */
static int
ice_sysctl_phy_sw_caps(SYSCTL_HANDLER_ARGS)
{
	return ice_sysctl_phy_caps(oidp, arg1, arg2, req,
				   ICE_AQC_REPORT_ACTIVE_CFG);
}

/**
 * ice_sysctl_phy_nvm_caps - Display response from Get PHY abililties
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Display the response from Get PHY abillities reporting the NVM
 * configuration.
 */
static int
ice_sysctl_phy_nvm_caps(SYSCTL_HANDLER_ARGS)
{
	return ice_sysctl_phy_caps(oidp, arg1, arg2, req,
				   ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA);
}

/**
 * ice_sysctl_phy_topo_caps - Display response from Get PHY abililties
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Display the response from Get PHY abillities reporting the
 * topology configuration.
 */
static int
ice_sysctl_phy_topo_caps(SYSCTL_HANDLER_ARGS)
{
	return ice_sysctl_phy_caps(oidp, arg1, arg2, req,
				   ICE_AQC_REPORT_TOPO_CAP_MEDIA);
}

/**
 * ice_sysctl_phy_link_status - Display response from Get Link Status
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Display the response from firmware for the Get Link Status
 * request.
 */
static int
ice_sysctl_phy_link_status(SYSCTL_HANDLER_ARGS)
{
	struct ice_aqc_get_link_status_data link_data = { 0 };
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi = hw->port_info;
	struct ice_aqc_get_link_status *resp;
	struct ice_aq_desc desc;
	device_t dev = sc->dev;
	enum ice_status status;
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	/*
	 * Ensure that only contexts with driver privilege are allowed to
	 * access this information
	 */
	ret = priv_check(curthread, PRIV_DRIVER);
	if (ret)
		return (ret);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_link_status);
	resp = &desc.params.get_link_status;
	resp->lport_num = pi->lport;

	status = ice_aq_send_cmd(hw, &desc, &link_data, sizeof(link_data), NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_aq_send_cmd failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	ret = sysctl_handle_opaque(oidp, &link_data, sizeof(link_data), req);
	if (req->newptr != NULL)
		return (EPERM);

	return (ret);
}

/**
 * ice_sysctl_fw_cur_lldp_persist_status - Display current FW LLDP status
 * @oidp: sysctl oid structure
 * @arg1: pointer to private softc structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays current persistent LLDP status.
 */
static int
ice_sysctl_fw_cur_lldp_persist_status(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	struct sbuf *sbuf;
	u32 lldp_state;

	UNREFERENCED_PARAMETER(arg2);
	UNREFERENCED_PARAMETER(oidp);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	status = ice_get_cur_lldp_persist_status(hw, &lldp_state);
	if (status) {
		device_printf(dev,
		    "Could not acquire current LLDP persistence status, err %s aq_err %s\n",
		    ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	sbuf_printf(sbuf, "%s", ice_fw_lldp_status(lldp_state));
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_fw_dflt_lldp_persist_status - Display default FW LLDP status
 * @oidp: sysctl oid structure
 * @arg1: pointer to private softc structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays default persistent LLDP status.
 */
static int
ice_sysctl_fw_dflt_lldp_persist_status(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	struct sbuf *sbuf;
	u32 lldp_state;

	UNREFERENCED_PARAMETER(arg2);
	UNREFERENCED_PARAMETER(oidp);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	status = ice_get_dflt_lldp_persist_status(hw, &lldp_state);
	if (status) {
		device_printf(dev,
		    "Could not acquire default LLDP persistence status, err %s aq_err %s\n",
		    ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	sbuf_printf(sbuf, "%s", ice_fw_lldp_status(lldp_state));
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_dscp_is_mapped - Check for non-zero DSCP to TC mappings
 * @dcbcfg: Configuration struct to check for mappings in
 *
 * @return true if there exists a non-zero DSCP to TC mapping
 * inside the input DCB configuration struct.
 */
static bool
ice_dscp_is_mapped(struct ice_dcbx_cfg *dcbcfg)
{
	for (int i = 0; i < ICE_DSCP_NUM_VAL; i++)
		if (dcbcfg->dscp_map[i] != 0)
			return (true);

	return (false);
}

#define ICE_SYSCTL_HELP_FW_LLDP_AGENT	\
"\nDisplay or change FW LLDP agent state:" \
"\n\t0 - disabled"			\
"\n\t1 - enabled"

/**
 * ice_sysctl_fw_lldp_agent - Display or change the FW LLDP agent status
 * @oidp: sysctl oid structure
 * @arg1: pointer to private softc structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays whether the FW LLDP agent is running
 * On write: Persistently enables or disables the FW LLDP agent
 */
static int
ice_sysctl_fw_lldp_agent(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	int ret;
	u32 old_state;
	u8 fw_lldp_enabled;
	bool retried_start_lldp = false;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	status = ice_get_cur_lldp_persist_status(hw, &old_state);
	if (status) {
		device_printf(dev,
		    "Could not acquire current LLDP persistence status, err %s aq_err %s\n",
		    ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	if (old_state > ICE_LLDP_ADMINSTATUS_ENA_RXTX) {
		status = ice_get_dflt_lldp_persist_status(hw, &old_state);
		if (status) {
			device_printf(dev,
			    "Could not acquire default LLDP persistence status, err %s aq_err %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
			return (EIO);
		}
	}
	if (old_state == 0)
		fw_lldp_enabled = false;
	else
		fw_lldp_enabled = true;

	ret = sysctl_handle_bool(oidp, &fw_lldp_enabled, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	if (old_state == 0 && fw_lldp_enabled == false)
		return (0);

	if (old_state != 0 && fw_lldp_enabled == true)
		return (0);

	/* Block transition to FW LLDP if DSCP mode is enabled */
	local_dcbx_cfg = &hw->port_info->qos_cfg.local_dcbx_cfg;
	if ((local_dcbx_cfg->pfc_mode == ICE_QOS_MODE_DSCP) &&
	    ice_dscp_is_mapped(local_dcbx_cfg)) {
		device_printf(dev,
			      "Cannot enable FW-LLDP agent while DSCP QoS is active.\n");
		return (EOPNOTSUPP);
	}

	if (fw_lldp_enabled == false) {
		status = ice_aq_stop_lldp(hw, true, true, NULL);
		/* EPERM is returned if the LLDP agent is already shutdown */
		if (status && hw->adminq.sq_last_status != ICE_AQ_RC_EPERM) {
			device_printf(dev,
			    "%s: ice_aq_stop_lldp failed; status %s, aq_err %s\n",
			    __func__, ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
			return (EIO);
		}
		ice_aq_set_dcb_parameters(hw, true, NULL);
		hw->port_info->qos_cfg.is_sw_lldp = true;
		ice_add_rx_lldp_filter(sc);
	} else {
		ice_del_rx_lldp_filter(sc);
retry_start_lldp:
		status = ice_aq_start_lldp(hw, true, NULL);
		if (status) {
			switch (hw->adminq.sq_last_status) {
			/* EEXIST is returned if the LLDP agent is already started */
			case ICE_AQ_RC_EEXIST:
				break;
			case ICE_AQ_RC_EAGAIN:
				/* Retry command after a 2 second wait */
				if (retried_start_lldp == false) {
					retried_start_lldp = true;
					pause("slldp", ICE_START_LLDP_RETRY_WAIT);
					goto retry_start_lldp;
				}
				/* Fallthrough */
			default:
				device_printf(dev,
				    "%s: ice_aq_start_lldp failed; status %s, aq_err %s\n",
				    __func__, ice_status_str(status),
				    ice_aq_str(hw->adminq.sq_last_status));
				return (EIO);
			}
		}
		ice_start_dcbx_agent(sc);
		hw->port_info->qos_cfg.is_sw_lldp = false;
	}

	return (ret);
}

#define ICE_SYSCTL_HELP_ETS_MIN_RATE \
"\nIn FW DCB mode (fw_lldp_agent=1), displays the current ETS bandwidth table." \
"\nIn SW DCB mode, displays and allows setting the table." \
"\nInput must be in the format e.g. 30,10,10,10,10,10,10,10" \
"\nWhere the bandwidth total must add up to 100"

/**
 * ice_sysctl_ets_min_rate - Report/configure ETS bandwidth
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Returns the current ETS TC bandwidth table
 * cached by the driver.
 *
 * In SW DCB mode this sysctl also accepts a value that will
 * be sent to the firmware for configuration.
 */
static int
ice_sysctl_ets_min_rate(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_port_info *pi;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	struct sbuf *sbuf;
	int ret;

	/* Store input rates from user */
	char ets_user_buf[128] = "";
	u8 new_ets_table[ICE_MAX_TRAFFIC_CLASS] = {};

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	if (req->oldptr == NULL && req->newptr == NULL) {
		ret = SYSCTL_OUT(req, 0, 128);
		return (ret);
	}

	pi = hw->port_info;
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;

	sbuf = sbuf_new(NULL, ets_user_buf, 128, SBUF_FIXEDLEN | SBUF_INCLUDENUL);

	/* Format ETS BW data for output */
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		sbuf_printf(sbuf, "%d", local_dcbx_cfg->etscfg.tcbwtable[i]);
		if (i != ICE_MAX_TRAFFIC_CLASS - 1)
			sbuf_printf(sbuf, ",");
	}

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	/* Read in the new ETS values */
	ret = sysctl_handle_string(oidp, ets_user_buf, sizeof(ets_user_buf), req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	/* Don't allow setting changes in FW DCB mode */
	if (!hw->port_info->qos_cfg.is_sw_lldp)
		return (EPERM);

	ret = ice_ets_str_to_tbl(ets_user_buf, new_ets_table, 100);
	if (ret) {
		device_printf(dev, "%s: Could not parse input BW table: %s\n",
		    __func__, ets_user_buf);
		return (ret);
	}

	if (!ice_check_ets_bw(new_ets_table)) {
		device_printf(dev, "%s: Bandwidth sum does not equal 100: %s\n",
		    __func__, ets_user_buf);
		return (EINVAL);
	}

	memcpy(local_dcbx_cfg->etscfg.tcbwtable, new_ets_table,
	    sizeof(new_ets_table));

	/* If BW > 0, then set TSA entry to 2 */
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		if (new_ets_table[i] > 0)
			local_dcbx_cfg->etscfg.tsatable[i] = 2;
		else
			local_dcbx_cfg->etscfg.tsatable[i] = 0;
	}
	local_dcbx_cfg->etscfg.willing = 0;
	local_dcbx_cfg->etsrec = local_dcbx_cfg->etscfg;
	local_dcbx_cfg->app_mode = ICE_DCBX_APPS_NON_WILLING;

	status = ice_set_dcb_cfg(pi);
	if (status) {
		device_printf(dev,
		    "%s: Failed to set DCB config; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	ice_do_dcb_reconfig(sc, false);

	return (0);
}

#define ICE_SYSCTL_HELP_UP2TC_MAP \
"\nIn FW DCB mode (fw_lldp_agent=1), displays the current ETS priority assignment table." \
"\nIn SW DCB mode, displays and allows setting the table." \
"\nInput must be in this format: 0,1,2,3,4,5,6,7" \
"\nWhere the 1st number is the TC for UP0, 2nd number is the TC for UP1, etc"

/**
 * ice_sysctl_up2tc_map - Report or configure UP2TC mapping
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * In FW DCB mode, returns the current ETS prio table /
 * UP2TC mapping from the local MIB.
 *
 * In SW DCB mode this sysctl also accepts a value that will
 * be sent to the firmware for configuration.
 */
static int
ice_sysctl_up2tc_map(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_port_info *pi;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	struct sbuf *sbuf;
	int ret;

	/* Store input rates from user */
	char up2tc_user_buf[128] = "";
	/* This array is indexed by UP, not TC */
	u8 new_up2tc[ICE_MAX_TRAFFIC_CLASS] = {};

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	if (req->oldptr == NULL && req->newptr == NULL) {
		ret = SYSCTL_OUT(req, 0, 128);
		return (ret);
	}

	pi = hw->port_info;
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;

	sbuf = sbuf_new(NULL, up2tc_user_buf, 128, SBUF_FIXEDLEN | SBUF_INCLUDENUL);

	/* Format ETS Priority Mapping Table for output */
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		sbuf_printf(sbuf, "%d", local_dcbx_cfg->etscfg.prio_table[i]);
		if (i != ICE_MAX_TRAFFIC_CLASS - 1)
			sbuf_printf(sbuf, ",");
	}

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	/* Read in the new ETS priority mapping */
	ret = sysctl_handle_string(oidp, up2tc_user_buf, sizeof(up2tc_user_buf), req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	/* Don't allow setting changes in FW DCB mode */
	if (!hw->port_info->qos_cfg.is_sw_lldp)
		return (EPERM);

	ret = ice_ets_str_to_tbl(up2tc_user_buf, new_up2tc, 7);
	if (ret) {
		device_printf(dev, "%s: Could not parse input priority assignment table: %s\n",
		    __func__, up2tc_user_buf);
		return (ret);
	}

	/* Prepare updated ETS CFG/REC TLVs */
	memcpy(local_dcbx_cfg->etscfg.prio_table, new_up2tc,
	    sizeof(new_up2tc));
	memcpy(local_dcbx_cfg->etsrec.prio_table, new_up2tc,
	    sizeof(new_up2tc));

	status = ice_set_dcb_cfg(pi);
	if (status) {
		device_printf(dev,
		    "%s: Failed to set DCB config; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	ice_do_dcb_reconfig(sc, false);

	return (0);
}

/**
 * ice_config_pfc - helper function to set PFC config in FW
 * @sc: device private structure
 * @new_mode: bit flags indicating PFC status for TCs
 *
 * @pre must be in SW DCB mode
 *
 * Configures the driver's local PFC TLV and sends it to the
 * FW for configuration, then reconfigures the driver/VSI
 * for DCB if needed.
 */
static int
ice_config_pfc(struct ice_softc *sc, u8 new_mode)
{
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi;
	device_t dev = sc->dev;
	enum ice_status status;

	pi = hw->port_info;
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;

	/* Prepare updated PFC TLV */
	local_dcbx_cfg->pfc.pfcena = new_mode;
	local_dcbx_cfg->pfc.pfccap = ICE_MAX_TRAFFIC_CLASS;
	local_dcbx_cfg->pfc.willing = 0;
	local_dcbx_cfg->pfc.mbc = 0;

	/* Warn if PFC is being disabled with RoCE v2 in use */
	if (new_mode == 0 && sc->rdma_entry.attached)
		device_printf(dev,
		    "WARNING: Recommended that Priority Flow Control is enabled when RoCEv2 is in use\n");

	status = ice_set_dcb_cfg(pi);
	if (status) {
		device_printf(dev,
		    "%s: Failed to set DCB config; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	ice_do_dcb_reconfig(sc, false);

	return (0);
}

#define ICE_SYSCTL_HELP_PFC_CONFIG \
"\nIn FW DCB mode (fw_lldp_agent=1), displays the current Priority Flow Control configuration" \
"\nIn SW DCB mode, displays and allows setting the configuration" \
"\nInput/Output is in this format: 0xff" \
"\nWhere bit position # enables/disables PFC for that Traffic Class #"

/**
 * ice_sysctl_pfc_config - Report or configure enabled PFC TCs
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * In FW DCB mode, returns a bitmap containing the current TCs
 * that have PFC enabled on them.
 *
 * In SW DCB mode this sysctl also accepts a value that will
 * be sent to the firmware for configuration.
 */
static int
ice_sysctl_pfc_config(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_port_info *pi;
	struct ice_hw *hw = &sc->hw;
	int ret;

	/* Store input flags from user */
	u8 user_pfc;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	if (req->oldptr == NULL && req->newptr == NULL) {
		ret = SYSCTL_OUT(req, 0, sizeof(u8));
		return (ret);
	}

	pi = hw->port_info;
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;

	/* Format current PFC enable setting for output */
	user_pfc = local_dcbx_cfg->pfc.pfcena;

	/* Read in the new PFC config */
	ret = sysctl_handle_8(oidp, &user_pfc, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	/* Don't allow setting changes in FW DCB mode */
	if (!hw->port_info->qos_cfg.is_sw_lldp)
		return (EPERM);

	/* If LFC is active and PFC is going to be turned on, turn LFC off */
	if (user_pfc != 0 && pi->phy.curr_user_fc_req != ICE_FC_NONE) {
		pi->phy.curr_user_fc_req = ICE_FC_NONE;
		ret = ice_apply_saved_phy_cfg(sc, ICE_APPLY_FC);
		if (ret)
			return (ret);
	}

	return ice_config_pfc(sc, user_pfc);
}

#define ICE_SYSCTL_HELP_PFC_MODE \
"\nDisplay and set the current QoS mode for the firmware" \
"\n\t0: VLAN UP mode" \
"\n\t1: DSCP mode"

/**
 * ice_sysctl_pfc_mode
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Gets and sets whether the port is in DSCP or VLAN PCP-based
 * PFC mode. This is also used to set whether DSCP or VLAN PCP
 * -based settings are configured for DCB.
 */
static int
ice_sysctl_pfc_mode(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_port_info *pi;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	u8 user_pfc_mode, aq_pfc_mode;
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	if (req->oldptr == NULL && req->newptr == NULL) {
		ret = SYSCTL_OUT(req, 0, sizeof(u8));
		return (ret);
	}

	pi = hw->port_info;
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;

	user_pfc_mode = local_dcbx_cfg->pfc_mode;

	/* Read in the new mode */
	ret = sysctl_handle_8(oidp, &user_pfc_mode, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	/* Don't allow setting changes in FW DCB mode */
	if (!hw->port_info->qos_cfg.is_sw_lldp)
		return (EPERM);

	/* Currently, there are only two modes */
	switch (user_pfc_mode) {
	case 0:
		aq_pfc_mode = ICE_AQC_PFC_VLAN_BASED_PFC;
		break;
	case 1:
		aq_pfc_mode = ICE_AQC_PFC_DSCP_BASED_PFC;
		break;
	default:
		device_printf(dev,
		    "%s: Valid input range is 0-1 (input %d)\n",
		    __func__, user_pfc_mode);
		return (EINVAL);
	}

	status = ice_aq_set_pfc_mode(hw, aq_pfc_mode, NULL);
	if (status == ICE_ERR_NOT_SUPPORTED) {
		device_printf(dev,
		    "%s: Failed to set PFC mode; DCB not supported\n",
		    __func__);
		return (ENODEV);
	}
	if (status) {
		device_printf(dev,
		    "%s: Failed to set PFC mode; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	/* Reset settings to default when mode is changed */
	ice_set_default_local_mib_settings(sc);
	/* Cache current settings and reconfigure */
	local_dcbx_cfg->pfc_mode = user_pfc_mode;
	ice_do_dcb_reconfig(sc, false);

	return (0);
}

/**
 * ice_add_device_sysctls - add device specific dynamic sysctls
 * @sc: device private structure
 *
 * Add per-device dynamic sysctls which show device configuration or enable
 * configuring device functionality. For tunable values which can be set prior
 * to load, see ice_add_device_tunables.
 *
 * This function depends on the sysctl layout setup by ice_add_device_tunables,
 * and likely should be called near the end of the attach process.
 */
void
ice_add_device_sysctls(struct ice_softc *sc)
{
	struct sysctl_oid *hw_node;
	device_t dev = sc->dev;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fw_version", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, ice_sysctl_show_fw, "A", "Firmware version");

	if (ice_is_bit_set(sc->feat_en, ICE_FEATURE_HAS_PBA)) {
		SYSCTL_ADD_PROC(ctx, ctx_list,
		    OID_AUTO, "pba_number", CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
		    ice_sysctl_pba_number, "A", "Product Board Assembly Number");
	}

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "ddp_version", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, ice_sysctl_pkg_version, "A", "Active DDP package name and version");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "current_speed", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, ice_sysctl_current_speed, "A", "Current Port Link Speed");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "requested_fec", CTLTYPE_STRING | CTLFLAG_RW,
	    sc, 0, ice_sysctl_fec_config, "A", ICE_SYSCTL_HELP_FEC_CONFIG);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "negotiated_fec", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, ice_sysctl_negotiated_fec, "A", "Current Negotiated FEC mode");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fc", CTLTYPE_STRING | CTLFLAG_RW,
	    sc, 0, ice_sysctl_fc_config, "A", ICE_SYSCTL_HELP_FC_CONFIG);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "advertise_speed", CTLTYPE_U16 | CTLFLAG_RW,
	    sc, 0, ice_sysctl_advertise_speed, "SU", ICE_SYSCTL_HELP_ADVERTISE_SPEED);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fw_lldp_agent", CTLTYPE_U8 | CTLFLAG_RWTUN,
	    sc, 0, ice_sysctl_fw_lldp_agent, "CU", ICE_SYSCTL_HELP_FW_LLDP_AGENT);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "ets_min_rate", CTLTYPE_STRING | CTLFLAG_RW,
	    sc, 0, ice_sysctl_ets_min_rate, "A", ICE_SYSCTL_HELP_ETS_MIN_RATE);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "up2tc_map", CTLTYPE_STRING | CTLFLAG_RW,
	    sc, 0, ice_sysctl_up2tc_map, "A", ICE_SYSCTL_HELP_UP2TC_MAP);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "pfc", CTLTYPE_U8 | CTLFLAG_RW,
	    sc, 0, ice_sysctl_pfc_config, "CU", ICE_SYSCTL_HELP_PFC_CONFIG);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "pfc_mode", CTLTYPE_U8 | CTLFLAG_RWTUN,
	    sc, 0, ice_sysctl_pfc_mode, "CU", ICE_SYSCTL_HELP_PFC_MODE);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "allow_no_fec_modules_in_auto",
	    CTLTYPE_U8 | CTLFLAG_RWTUN | CTLFLAG_MPSAFE,
	    sc, 0, ice_sysctl_allow_no_fec_mod_in_auto, "CU",
	    "Allow \"No FEC\" mode in FEC auto-negotiation");

	ice_add_dscp2tc_map_sysctls(sc, ctx, ctx_list);

	/* Differentiate software and hardware statistics, by keeping hw stats
	 * in their own node. This isn't in ice_add_device_tunables, because
	 * we won't have any CTLFLAG_TUN sysctls under this node.
	 */
	hw_node = SYSCTL_ADD_NODE(ctx, ctx_list, OID_AUTO, "hw", CTLFLAG_RD,
				  NULL, "Port Hardware Statistics");

	ice_add_sysctls_mac_stats(ctx, hw_node, &sc->stats.cur);

	/* Add the main PF VSI stats now. Other VSIs will add their own stats
	 * during creation
	 */
	ice_add_vsi_sysctls(&sc->pf_vsi);

	/* Add sysctls related to debugging the device driver. This includes
	 * sysctls which display additional internal driver state for use in
	 * understanding what is happening within the driver.
	 */
	ice_add_debug_sysctls(sc);
}

/**
 * @enum hmc_error_type
 * @brief enumeration of HMC errors
 *
 * Enumeration defining the possible HMC errors that might occur.
 */
enum hmc_error_type {
	HMC_ERR_PMF_INVALID = 0,
	HMC_ERR_VF_IDX_INVALID = 1,
	HMC_ERR_VF_PARENT_PF_INVALID = 2,
	/* 3 is reserved */
	HMC_ERR_INDEX_TOO_BIG = 4,
	HMC_ERR_ADDRESS_TOO_LARGE = 5,
	HMC_ERR_SEGMENT_DESC_INVALID = 6,
	HMC_ERR_SEGMENT_DESC_TOO_SMALL = 7,
	HMC_ERR_PAGE_DESC_INVALID = 8,
	HMC_ERR_UNSUPPORTED_REQUEST_COMPLETION = 9,
	/* 10 is reserved */
	HMC_ERR_INVALID_OBJECT_TYPE = 11,
	/* 12 is reserved */
};

/**
 * ice_log_hmc_error - Log an HMC error message
 * @hw: device hw structure
 * @dev: the device to pass to device_printf()
 *
 * Log a message when an HMC error interrupt is triggered.
 */
void
ice_log_hmc_error(struct ice_hw *hw, device_t dev)
{
	u32 info, data;
	u8 index, errtype, objtype;
	bool isvf;

	info = rd32(hw, PFHMC_ERRORINFO);
	data = rd32(hw, PFHMC_ERRORDATA);

	index = (u8)(info & PFHMC_ERRORINFO_PMF_INDEX_M);
	errtype = (u8)((info & PFHMC_ERRORINFO_HMC_ERROR_TYPE_M) >>
		       PFHMC_ERRORINFO_HMC_ERROR_TYPE_S);
	objtype = (u8)((info & PFHMC_ERRORINFO_HMC_OBJECT_TYPE_M) >>
		       PFHMC_ERRORINFO_HMC_OBJECT_TYPE_S);

	isvf = info & PFHMC_ERRORINFO_PMF_ISVF_M;

	device_printf(dev, "%s HMC Error detected on PMF index %d:\n",
		      isvf ? "VF" : "PF", index);

	device_printf(dev, "error type %d, object type %d, data 0x%08x\n",
		      errtype, objtype, data);

	switch (errtype) {
	case HMC_ERR_PMF_INVALID:
		device_printf(dev, "Private Memory Function is not valid\n");
		break;
	case HMC_ERR_VF_IDX_INVALID:
		device_printf(dev, "Invalid Private Memory Function index for PE enabled VF\n");
		break;
	case HMC_ERR_VF_PARENT_PF_INVALID:
		device_printf(dev, "Invalid parent PF for PE enabled VF\n");
		break;
	case HMC_ERR_INDEX_TOO_BIG:
		device_printf(dev, "Object index too big\n");
		break;
	case HMC_ERR_ADDRESS_TOO_LARGE:
		device_printf(dev, "Address extends beyond segment descriptor limit\n");
		break;
	case HMC_ERR_SEGMENT_DESC_INVALID:
		device_printf(dev, "Segment descriptor is invalid\n");
		break;
	case HMC_ERR_SEGMENT_DESC_TOO_SMALL:
		device_printf(dev, "Segment descriptor is too small\n");
		break;
	case HMC_ERR_PAGE_DESC_INVALID:
		device_printf(dev, "Page descriptor is invalid\n");
		break;
	case HMC_ERR_UNSUPPORTED_REQUEST_COMPLETION:
		device_printf(dev, "Unsupported Request completion received from PCIe\n");
		break;
	case HMC_ERR_INVALID_OBJECT_TYPE:
		device_printf(dev, "Invalid object type\n");
		break;
	default:
		device_printf(dev, "Unknown HMC error\n");
	}

	/* Clear the error indication */
	wr32(hw, PFHMC_ERRORINFO, 0);
}

/**
 * @struct ice_sysctl_info
 * @brief sysctl information
 *
 * Structure used to simplify the process of defining the many similar
 * statistics sysctls.
 */
struct ice_sysctl_info {
	u64		*stat;
	const char	*name;
	const char	*description;
};

/**
 * ice_add_sysctls_eth_stats - Add sysctls for ethernet statistics
 * @ctx: sysctl ctx to use
 * @parent: the parent node to add sysctls under
 * @stats: the ethernet stats structure to source values from
 *
 * Adds statistics sysctls for the ethernet statistics of the MAC or a VSI.
 * Will add them under the parent node specified.
 *
 * Note that tx_errors is only meaningful for VSIs and not the global MAC/PF
 * statistics, so it is not included here. Similarly, rx_discards has different
 * descriptions for VSIs and MAC/PF stats, so it is also not included here.
 */
void
ice_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
			  struct sysctl_oid *parent,
			  struct ice_eth_stats *stats)
{
	const struct ice_sysctl_info ctls[] = {
		/* Rx Stats */
		{ &stats->rx_bytes, "good_octets_rcvd", "Good Octets Received" },
		{ &stats->rx_unicast, "ucast_pkts_rcvd", "Unicast Packets Received" },
		{ &stats->rx_multicast, "mcast_pkts_rcvd", "Multicast Packets Received" },
		{ &stats->rx_broadcast, "bcast_pkts_rcvd", "Broadcast Packets Received" },
		/* Tx Stats */
		{ &stats->tx_bytes, "good_octets_txd", "Good Octets Transmitted" },
		{ &stats->tx_unicast, "ucast_pkts_txd", "Unicast Packets Transmitted" },
		{ &stats->tx_multicast, "mcast_pkts_txd", "Multicast Packets Transmitted" },
		{ &stats->tx_broadcast, "bcast_pkts_txd", "Broadcast Packets Transmitted" },
		/* End */
		{ 0, 0, 0 }
	};

	struct sysctl_oid_list *parent_list = SYSCTL_CHILDREN(parent);

	const struct ice_sysctl_info *entry = ctls;
	while (entry->stat != 0) {
		SYSCTL_ADD_U64(ctx, parent_list, OID_AUTO, entry->name,
			       CTLFLAG_RD | CTLFLAG_STATS, entry->stat, 0,
			       entry->description);
		entry++;
	}
}

/**
 * ice_sysctl_tx_cso_stat - Display Tx checksum offload statistic
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: Tx CSO stat to read
 * @req: sysctl request pointer
 *
 * On read: Sums the per-queue Tx CSO stat and displays it.
 */
static int
ice_sysctl_tx_cso_stat(SYSCTL_HANDLER_ARGS)
{
	struct ice_vsi *vsi = (struct ice_vsi *)arg1;
	enum ice_tx_cso_stat type = (enum ice_tx_cso_stat)arg2;
	u64 stat = 0;
	int i;

	if (ice_driver_is_detaching(vsi->sc))
		return (ESHUTDOWN);

	/* Check that the type is valid */
	if (type >= ICE_CSO_STAT_TX_COUNT)
		return (EDOOFUS);

	/* Sum the stat for each of the Tx queues */
	for (i = 0; i < vsi->num_tx_queues; i++)
		stat += vsi->tx_queues[i].stats.cso[type];

	return sysctl_handle_64(oidp, NULL, stat, req);
}

/**
 * ice_sysctl_rx_cso_stat - Display Rx checksum offload statistic
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: Rx CSO stat to read
 * @req: sysctl request pointer
 *
 * On read: Sums the per-queue Rx CSO stat and displays it.
 */
static int
ice_sysctl_rx_cso_stat(SYSCTL_HANDLER_ARGS)
{
	struct ice_vsi *vsi = (struct ice_vsi *)arg1;
	enum ice_rx_cso_stat type = (enum ice_rx_cso_stat)arg2;
	u64 stat = 0;
	int i;

	if (ice_driver_is_detaching(vsi->sc))
		return (ESHUTDOWN);

	/* Check that the type is valid */
	if (type >= ICE_CSO_STAT_RX_COUNT)
		return (EDOOFUS);

	/* Sum the stat for each of the Rx queues */
	for (i = 0; i < vsi->num_rx_queues; i++)
		stat += vsi->rx_queues[i].stats.cso[type];

	return sysctl_handle_64(oidp, NULL, stat, req);
}

/**
 * ice_sysctl_rx_errors_stat - Display aggregate of Rx errors
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Sums current values of Rx error statistics and
 * displays it.
 */
static int
ice_sysctl_rx_errors_stat(SYSCTL_HANDLER_ARGS)
{
	struct ice_vsi *vsi = (struct ice_vsi *)arg1;
	struct ice_hw_port_stats *hs = &vsi->sc->stats.cur;
	u64 stat = 0;
	int i, type;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(vsi->sc))
		return (ESHUTDOWN);

	stat += hs->rx_undersize;
	stat += hs->rx_fragments;
	stat += hs->rx_oversize;
	stat += hs->rx_jabber;
	stat += hs->rx_len_errors;
	stat += hs->crc_errors;
	stat += hs->illegal_bytes;

	/* Checksum error stats */
	for (i = 0; i < vsi->num_rx_queues; i++)
		for (type = ICE_CSO_STAT_RX_IP4_ERR;
		     type < ICE_CSO_STAT_RX_COUNT;
		     type++)
			stat += vsi->rx_queues[i].stats.cso[type];

	return sysctl_handle_64(oidp, NULL, stat, req);
}

/**
 * @struct ice_rx_cso_stat_info
 * @brief sysctl information for an Rx checksum offload statistic
 *
 * Structure used to simplify the process of defining the checksum offload
 * statistics.
 */
struct ice_rx_cso_stat_info {
	enum ice_rx_cso_stat	type;
	const char		*name;
	const char		*description;
};

/**
 * @struct ice_tx_cso_stat_info
 * @brief sysctl information for a Tx checksum offload statistic
 *
 * Structure used to simplify the process of defining the checksum offload
 * statistics.
 */
struct ice_tx_cso_stat_info {
	enum ice_tx_cso_stat	type;
	const char		*name;
	const char		*description;
};

/**
 * ice_add_sysctls_sw_stats - Add sysctls for software statistics
 * @vsi: pointer to the VSI to add sysctls for
 * @ctx: sysctl ctx to use
 * @parent: the parent node to add sysctls under
 *
 * Add statistics sysctls for software tracked statistics of a VSI.
 *
 * Currently this only adds checksum offload statistics, but more counters may
 * be added in the future.
 */
static void
ice_add_sysctls_sw_stats(struct ice_vsi *vsi,
			 struct sysctl_ctx_list *ctx,
			 struct sysctl_oid *parent)
{
	struct sysctl_oid *cso_node;
	struct sysctl_oid_list *cso_list;

	/* Tx CSO Stats */
	const struct ice_tx_cso_stat_info tx_ctls[] = {
		{ ICE_CSO_STAT_TX_TCP, "tx_tcp", "Transmit TCP Packets marked for HW checksum" },
		{ ICE_CSO_STAT_TX_UDP, "tx_udp", "Transmit UDP Packets marked for HW checksum" },
		{ ICE_CSO_STAT_TX_SCTP, "tx_sctp", "Transmit SCTP Packets marked for HW checksum" },
		{ ICE_CSO_STAT_TX_IP4, "tx_ip4", "Transmit IPv4 Packets marked for HW checksum" },
		{ ICE_CSO_STAT_TX_IP6, "tx_ip6", "Transmit IPv6 Packets marked for HW checksum" },
		{ ICE_CSO_STAT_TX_L3_ERR, "tx_l3_err", "Transmit packets that driver failed to set L3 HW CSO bits for" },
		{ ICE_CSO_STAT_TX_L4_ERR, "tx_l4_err", "Transmit packets that driver failed to set L4 HW CSO bits for" },
		/* End */
		{ ICE_CSO_STAT_TX_COUNT, 0, 0 }
	};

	/* Rx CSO Stats */
	const struct ice_rx_cso_stat_info rx_ctls[] = {
		{ ICE_CSO_STAT_RX_IP4_ERR, "rx_ip4_err", "Received packets with invalid IPv4 checksum indicated by HW" },
		{ ICE_CSO_STAT_RX_IP6_ERR, "rx_ip6_err", "Received IPv6 packets with extension headers" },
		{ ICE_CSO_STAT_RX_L3_ERR, "rx_l3_err", "Received packets with an unexpected invalid L3 checksum indicated by HW" },
		{ ICE_CSO_STAT_RX_TCP_ERR, "rx_tcp_err", "Received packets with invalid TCP checksum indicated by HW" },
		{ ICE_CSO_STAT_RX_UDP_ERR, "rx_udp_err", "Received packets with invalid UDP checksum indicated by HW" },
		{ ICE_CSO_STAT_RX_SCTP_ERR, "rx_sctp_err", "Received packets with invalid SCTP checksum indicated by HW" },
		{ ICE_CSO_STAT_RX_L4_ERR, "rx_l4_err", "Received packets with an unexpected invalid L4 checksum indicated by HW" },
		/* End */
		{ ICE_CSO_STAT_RX_COUNT, 0, 0 }
	};

	struct sysctl_oid_list *parent_list = SYSCTL_CHILDREN(parent);

	/* Add a node for statistics tracked by software. */
	cso_node = SYSCTL_ADD_NODE(ctx, parent_list, OID_AUTO, "cso", CTLFLAG_RD,
				  NULL, "Checksum offload Statistics");
	cso_list = SYSCTL_CHILDREN(cso_node);

	const struct ice_tx_cso_stat_info *tx_entry = tx_ctls;
	while (tx_entry->name && tx_entry->description) {
		SYSCTL_ADD_PROC(ctx, cso_list, OID_AUTO, tx_entry->name,
				CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS,
				vsi, tx_entry->type, ice_sysctl_tx_cso_stat, "QU",
				tx_entry->description);
		tx_entry++;
	}

	const struct ice_rx_cso_stat_info *rx_entry = rx_ctls;
	while (rx_entry->name && rx_entry->description) {
		SYSCTL_ADD_PROC(ctx, cso_list, OID_AUTO, rx_entry->name,
				CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS,
				vsi, rx_entry->type, ice_sysctl_rx_cso_stat, "QU",
				rx_entry->description);
		rx_entry++;
	}
}

/**
 * ice_add_vsi_sysctls - Add sysctls for a VSI
 * @vsi: pointer to VSI structure
 *
 * Add various sysctls for a given VSI.
 */
void
ice_add_vsi_sysctls(struct ice_vsi *vsi)
{
	struct sysctl_ctx_list *ctx = &vsi->ctx;
	struct sysctl_oid *hw_node, *sw_node;
	struct sysctl_oid_list *vsi_list, *hw_list;

	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	/* Keep hw stats in their own node. */
	hw_node = SYSCTL_ADD_NODE(ctx, vsi_list, OID_AUTO, "hw", CTLFLAG_RD,
				  NULL, "VSI Hardware Statistics");
	hw_list = SYSCTL_CHILDREN(hw_node);

	/* Add the ethernet statistics for this VSI */
	ice_add_sysctls_eth_stats(ctx, hw_node, &vsi->hw_stats.cur);

	SYSCTL_ADD_U64(ctx, hw_list, OID_AUTO, "rx_discards",
			CTLFLAG_RD | CTLFLAG_STATS, &vsi->hw_stats.cur.rx_discards,
			0, "Discarded Rx Packets (see rx_errors or rx_no_desc)");

	SYSCTL_ADD_PROC(ctx, hw_list, OID_AUTO, "rx_errors",
			CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_STATS,
			vsi, 0, ice_sysctl_rx_errors_stat, "QU",
			"Aggregate of all Rx errors");

	SYSCTL_ADD_U64(ctx, hw_list, OID_AUTO, "rx_no_desc",
		       CTLFLAG_RD | CTLFLAG_STATS, &vsi->hw_stats.cur.rx_no_desc,
		       0, "Rx Packets Discarded Due To Lack Of Descriptors");

	SYSCTL_ADD_U64(ctx, hw_list, OID_AUTO, "tx_errors",
			CTLFLAG_RD | CTLFLAG_STATS, &vsi->hw_stats.cur.tx_errors,
			0, "Tx Packets Discarded Due To Error");

	/* Add a node for statistics tracked by software. */
	sw_node = SYSCTL_ADD_NODE(ctx, vsi_list, OID_AUTO, "sw", CTLFLAG_RD,
				  NULL, "VSI Software Statistics");

	ice_add_sysctls_sw_stats(vsi, ctx, sw_node);
}

/**
 * ice_add_sysctls_mac_pfc_one_stat - Add sysctl node for a PFC statistic
 * @ctx: sysctl ctx to use
 * @parent_list: parent sysctl list to add sysctls under
 * @pfc_stat_location: address of statistic for sysctl to display
 * @node_name: Name for statistic node
 * @descr: Description used for nodes added in this function
 *
 * A helper function for ice_add_sysctls_mac_pfc_stats that adds a node
 * for a stat and leaves for each traffic class for that stat.
 */
static void
ice_add_sysctls_mac_pfc_one_stat(struct sysctl_ctx_list *ctx,
				 struct sysctl_oid_list *parent_list,
				 u64* pfc_stat_location,
				 const char *node_name,
				 const char *descr)
{
	struct sysctl_oid_list *node_list;
	struct sysctl_oid *node;
	struct sbuf *namebuf, *descbuf;

	node = SYSCTL_ADD_NODE(ctx, parent_list, OID_AUTO, node_name, CTLFLAG_RD,
				   NULL, descr);
	node_list = SYSCTL_CHILDREN(node);

	namebuf = sbuf_new_auto();
	descbuf = sbuf_new_auto();
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		sbuf_clear(namebuf);
		sbuf_clear(descbuf);

		sbuf_printf(namebuf, "%d", i);
		sbuf_printf(descbuf, "%s for TC %d", descr, i);

		sbuf_finish(namebuf);
		sbuf_finish(descbuf);

		SYSCTL_ADD_U64(ctx, node_list, OID_AUTO, sbuf_data(namebuf),
			CTLFLAG_RD | CTLFLAG_STATS, &pfc_stat_location[i], 0,
			sbuf_data(descbuf));
	}

	sbuf_delete(namebuf);
	sbuf_delete(descbuf);
}

/**
 * ice_add_sysctls_mac_pfc_stats - Add sysctls for MAC PFC statistics
 * @ctx: the sysctl ctx to use
 * @parent: parent node to add the sysctls under
 * @stats: the hw ports stat structure to pull values from
 *
 * Add global Priority Flow Control MAC statistics sysctls. These are
 * structured as a node with the PFC statistic, where there are eight
 * nodes for each traffic class.
 */
static void
ice_add_sysctls_mac_pfc_stats(struct sysctl_ctx_list *ctx,
			      struct sysctl_oid *parent,
			      struct ice_hw_port_stats *stats)
{
	struct sysctl_oid_list *parent_list;

	parent_list = SYSCTL_CHILDREN(parent);

	ice_add_sysctls_mac_pfc_one_stat(ctx, parent_list, stats->priority_xon_rx,
	    "p_xon_recvd", "PFC XON received");
	ice_add_sysctls_mac_pfc_one_stat(ctx, parent_list, stats->priority_xoff_rx,
	    "p_xoff_recvd", "PFC XOFF received");
	ice_add_sysctls_mac_pfc_one_stat(ctx, parent_list, stats->priority_xon_tx,
	    "p_xon_txd", "PFC XON transmitted");
	ice_add_sysctls_mac_pfc_one_stat(ctx, parent_list, stats->priority_xoff_tx,
	    "p_xoff_txd", "PFC XOFF transmitted");
	ice_add_sysctls_mac_pfc_one_stat(ctx, parent_list, stats->priority_xon_2_xoff,
	    "p_xon2xoff", "PFC XON to XOFF transitions");
}

/**
 * ice_add_sysctls_mac_stats - Add sysctls for global MAC statistics
 * @ctx: the sysctl ctx to use
 * @parent: parent node to add the sysctls under
 * @stats: the hw ports stat structure to pull values from
 *
 * Add global MAC statistics sysctls.
 */
void
ice_add_sysctls_mac_stats(struct sysctl_ctx_list *ctx,
			  struct sysctl_oid *parent,
			  struct ice_hw_port_stats *stats)
{
	struct sysctl_oid *mac_node;
	struct sysctl_oid_list *parent_list, *mac_list;

	parent_list = SYSCTL_CHILDREN(parent);

	mac_node = SYSCTL_ADD_NODE(ctx, parent_list, OID_AUTO, "mac", CTLFLAG_RD,
				   NULL, "Mac Hardware Statistics");
	mac_list = SYSCTL_CHILDREN(mac_node);

	/* Add the ethernet statistics common to VSI and MAC */
	ice_add_sysctls_eth_stats(ctx, mac_node, &stats->eth);

	/* Add PFC stats that add per-TC counters */
	ice_add_sysctls_mac_pfc_stats(ctx, mac_node, stats);

	const struct ice_sysctl_info ctls[] = {
		/* Packet Reception Stats */
		{&stats->rx_size_64, "rx_frames_64", "64 byte frames received"},
		{&stats->rx_size_127, "rx_frames_65_127", "65-127 byte frames received"},
		{&stats->rx_size_255, "rx_frames_128_255", "128-255 byte frames received"},
		{&stats->rx_size_511, "rx_frames_256_511", "256-511 byte frames received"},
		{&stats->rx_size_1023, "rx_frames_512_1023", "512-1023 byte frames received"},
		{&stats->rx_size_1522, "rx_frames_1024_1522", "1024-1522 byte frames received"},
		{&stats->rx_size_big, "rx_frames_big", "1523-9522 byte frames received"},
		{&stats->rx_undersize, "rx_undersize", "Undersized packets received"},
		{&stats->rx_fragments, "rx_fragmented", "Fragmented packets received"},
		{&stats->rx_oversize, "rx_oversized", "Oversized packets received"},
		{&stats->rx_jabber, "rx_jabber", "Received Jabber"},
		{&stats->rx_len_errors, "rx_length_errors", "Receive Length Errors"},
		{&stats->eth.rx_discards, "rx_discards",
		    "Discarded Rx Packets by Port (shortage of storage space)"},
		/* Packet Transmission Stats */
		{&stats->tx_size_64, "tx_frames_64", "64 byte frames transmitted"},
		{&stats->tx_size_127, "tx_frames_65_127", "65-127 byte frames transmitted"},
		{&stats->tx_size_255, "tx_frames_128_255", "128-255 byte frames transmitted"},
		{&stats->tx_size_511, "tx_frames_256_511", "256-511 byte frames transmitted"},
		{&stats->tx_size_1023, "tx_frames_512_1023", "512-1023 byte frames transmitted"},
		{&stats->tx_size_1522, "tx_frames_1024_1522", "1024-1522 byte frames transmitted"},
		{&stats->tx_size_big, "tx_frames_big", "1523-9522 byte frames transmitted"},
		{&stats->tx_dropped_link_down, "tx_dropped", "Tx Dropped Due To Link Down"},
		/* Flow control */
		{&stats->link_xon_tx, "xon_txd", "Link XON transmitted"},
		{&stats->link_xon_rx, "xon_recvd", "Link XON received"},
		{&stats->link_xoff_tx, "xoff_txd", "Link XOFF transmitted"},
		{&stats->link_xoff_rx, "xoff_recvd", "Link XOFF received"},
		/* Other */
		{&stats->crc_errors, "crc_errors", "CRC Errors"},
		{&stats->illegal_bytes, "illegal_bytes", "Illegal Byte Errors"},
		{&stats->mac_local_faults, "local_faults", "MAC Local Faults"},
		{&stats->mac_remote_faults, "remote_faults", "MAC Remote Faults"},
		/* End */
		{ 0, 0, 0 }
	};

	const struct ice_sysctl_info *entry = ctls;
	while (entry->stat != 0) {
		SYSCTL_ADD_U64(ctx, mac_list, OID_AUTO, entry->name,
			CTLFLAG_RD | CTLFLAG_STATS, entry->stat, 0,
			entry->description);
		entry++;
	}
}

/**
 * ice_configure_misc_interrupts - enable 'other' interrupt causes
 * @sc: pointer to device private softc
 *
 * Enable various "other" interrupt causes, and associate them to interrupt 0,
 * which is our administrative interrupt.
 */
void
ice_configure_misc_interrupts(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	u32 val;

	/* Read the OICR register to clear it */
	rd32(hw, PFINT_OICR);

	/* Enable useful "other" interrupt causes */
	val = (PFINT_OICR_ECC_ERR_M |
	       PFINT_OICR_MAL_DETECT_M |
	       PFINT_OICR_GRST_M |
	       PFINT_OICR_PCI_EXCEPTION_M |
	       PFINT_OICR_VFLR_M |
	       PFINT_OICR_HMC_ERR_M |
	       PFINT_OICR_PE_CRITERR_M);

	wr32(hw, PFINT_OICR_ENA, val);

	/* Note that since we're using MSI-X index 0, and ITR index 0, we do
	 * not explicitly program them when writing to the PFINT_*_CTL
	 * registers. Nevertheless, these writes are associating the
	 * interrupts with the ITR 0 vector
	 */

	/* Associate the OICR interrupt with ITR 0, and enable it */
	wr32(hw, PFINT_OICR_CTL, PFINT_OICR_CTL_CAUSE_ENA_M);

	/* Associate the Mailbox interrupt with ITR 0, and enable it */
	wr32(hw, PFINT_MBX_CTL, PFINT_MBX_CTL_CAUSE_ENA_M);

	/* Associate the AdminQ interrupt with ITR 0, and enable it */
	wr32(hw, PFINT_FW_CTL, PFINT_FW_CTL_CAUSE_ENA_M);
}

/**
 * ice_filter_is_mcast - Check if info is a multicast filter
 * @vsi: vsi structure addresses are targeted towards
 * @info: filter info
 *
 * @returns true if the provided info is a multicast filter, and false
 * otherwise.
 */
static bool
ice_filter_is_mcast(struct ice_vsi *vsi, struct ice_fltr_info *info)
{
	const u8 *addr = info->l_data.mac.mac_addr;

	/*
	 * Check if this info matches a multicast filter added by
	 * ice_add_mac_to_list
	 */
	if ((info->flag == ICE_FLTR_TX) &&
	    (info->src_id == ICE_SRC_ID_VSI) &&
	    (info->lkup_type == ICE_SW_LKUP_MAC) &&
	    (info->vsi_handle == vsi->idx) &&
	    ETHER_IS_MULTICAST(addr) && !ETHER_IS_BROADCAST(addr))
		return true;

	return false;
}

/**
 * @struct ice_mcast_sync_data
 * @brief data used by ice_sync_one_mcast_filter function
 *
 * Structure used to store data needed for processing by the
 * ice_sync_one_mcast_filter. This structure contains a linked list of filters
 * to be added, an error indication, and a pointer to the device softc.
 */
struct ice_mcast_sync_data {
	struct ice_list_head add_list;
	struct ice_softc *sc;
	int err;
};

/**
 * ice_sync_one_mcast_filter - Check if we need to program the filter
 * @p: void pointer to algorithm data
 * @sdl: link level socket address
 * @count: unused count value
 *
 * Called by if_foreach_llmaddr to operate on each filter in the ifp filter
 * list. For the given address, search our internal list to see if we have
 * found the filter. If not, add it to our list of filters that need to be
 * programmed.
 *
 * @returns (1) if we've actually setup the filter to be added
 */
static u_int
ice_sync_one_mcast_filter(void *p, struct sockaddr_dl *sdl,
			  u_int __unused count)
{
	struct ice_mcast_sync_data *data = (struct ice_mcast_sync_data *)p;
	struct ice_softc *sc = data->sc;
	struct ice_hw *hw = &sc->hw;
	struct ice_switch_info *sw = hw->switch_info;
	const u8 *sdl_addr = (const u8 *)LLADDR(sdl);
	struct ice_fltr_mgmt_list_entry *itr;
	struct ice_list_head *rules;
	int err;

	rules = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rules;

	/*
	 * If a previous filter already indicated an error, there is no need
	 * for us to finish processing the rest of the filters.
	 */
	if (data->err)
		return (0);

	/* See if this filter has already been programmed */
	LIST_FOR_EACH_ENTRY(itr, rules, ice_fltr_mgmt_list_entry, list_entry) {
		struct ice_fltr_info *info = &itr->fltr_info;
		const u8 *addr = info->l_data.mac.mac_addr;

		/* Only check multicast filters */
		if (!ice_filter_is_mcast(&sc->pf_vsi, info))
			continue;

		/*
		 * If this filter matches, mark the internal filter as
		 * "found", and exit.
		 */
		if (bcmp(addr, sdl_addr, ETHER_ADDR_LEN) == 0) {
			itr->marker = ICE_FLTR_FOUND;
			return (1);
		}
	}

	/*
	 * If we failed to locate the filter in our internal list, we need to
	 * place it into our add list.
	 */
	err = ice_add_mac_to_list(&sc->pf_vsi, &data->add_list, sdl_addr,
				  ICE_FWD_TO_VSI);
	if (err) {
		device_printf(sc->dev,
			      "Failed to place MAC %6D onto add list, err %s\n",
			      sdl_addr, ":", ice_err_str(err));
		data->err = err;

		return (0);
	}

	return (1);
}

/**
 * ice_sync_multicast_filters - Synchronize OS and internal filter list
 * @sc: device private structure
 *
 * Called in response to SIOCDELMULTI to synchronize the operating system
 * multicast address list with the internal list of filters programmed to
 * firmware.
 *
 * Works in one phase to find added and deleted filters using a marker bit on
 * the internal list.
 *
 * First, a loop over the internal list clears the marker bit. Second, for
 * each filter in the ifp list is checked. If we find it in the internal list,
 * the marker bit is set. Otherwise, the filter is added to the add list.
 * Third, a loop over the internal list determines if any filters have not
 * been found. Each of these is added to the delete list. Finally, the add and
 * delete lists are programmed to firmware to update the filters.
 *
 * @returns zero on success or an integer error code on failure.
 */
int
ice_sync_multicast_filters(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_fltr_mgmt_list_entry *itr;
	struct ice_mcast_sync_data data = {};
	struct ice_list_head *rules, remove_list;
	enum ice_status status;
	int err = 0;

	INIT_LIST_HEAD(&data.add_list);
	INIT_LIST_HEAD(&remove_list);
	data.sc = sc;
	data.err = 0;

	rules = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rules;

	/* Acquire the lock for the entire duration */
	ice_acquire_lock(&sw->recp_list[ICE_SW_LKUP_MAC].filt_rule_lock);

	/* (1) Reset the marker state for all filters */
	LIST_FOR_EACH_ENTRY(itr, rules, ice_fltr_mgmt_list_entry, list_entry)
		itr->marker = ICE_FLTR_NOT_FOUND;

	/* (2) determine which filters need to be added and removed */
	if_foreach_llmaddr(sc->ifp, ice_sync_one_mcast_filter, (void *)&data);
	if (data.err) {
		/* ice_sync_one_mcast_filter already prints an error */
		err = data.err;
		ice_release_lock(&sw->recp_list[ICE_SW_LKUP_MAC].filt_rule_lock);
		goto free_filter_lists;
	}

	LIST_FOR_EACH_ENTRY(itr, rules, ice_fltr_mgmt_list_entry, list_entry) {
		struct ice_fltr_info *info = &itr->fltr_info;
		const u8 *addr = info->l_data.mac.mac_addr;

		/* Only check multicast filters */
		if (!ice_filter_is_mcast(&sc->pf_vsi, info))
			continue;

		/*
		 * If the filter is not marked as found, then it must no
		 * longer be in the ifp address list, so we need to remove it.
		 */
		if (itr->marker == ICE_FLTR_NOT_FOUND) {
			err = ice_add_mac_to_list(&sc->pf_vsi, &remove_list,
						  addr, ICE_FWD_TO_VSI);
			if (err) {
				device_printf(sc->dev,
					      "Failed to place MAC %6D onto remove list, err %s\n",
					      addr, ":", ice_err_str(err));
				ice_release_lock(&sw->recp_list[ICE_SW_LKUP_MAC].filt_rule_lock);
				goto free_filter_lists;
			}
		}
	}

	ice_release_lock(&sw->recp_list[ICE_SW_LKUP_MAC].filt_rule_lock);

	status = ice_add_mac(hw, &data.add_list);
	if (status) {
		device_printf(sc->dev,
			      "Could not add new MAC filters, err %s aq_err %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		err = (EIO);
		goto free_filter_lists;
	}

	status = ice_remove_mac(hw, &remove_list);
	if (status) {
		device_printf(sc->dev,
			      "Could not remove old MAC filters, err %s aq_err %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		err = (EIO);
		goto free_filter_lists;
	}

free_filter_lists:
	ice_free_fltr_list(&data.add_list);
	ice_free_fltr_list(&remove_list);

	return (err);
}

/**
 * ice_add_vlan_hw_filters - Add multiple VLAN filters for a given VSI
 * @vsi: The VSI to add the filter for
 * @vid: array of VLAN ids to add
 * @length: length of vid array
 *
 * Programs HW filters so that the given VSI will receive the specified VLANs.
 */
enum ice_status
ice_add_vlan_hw_filters(struct ice_vsi *vsi, u16 *vid, u16 length)
{
	struct ice_hw *hw = &vsi->sc->hw;
	struct ice_list_head vlan_list;
	struct ice_fltr_list_entry *vlan_entries;
	enum ice_status status;

	MPASS(length > 0);

	INIT_LIST_HEAD(&vlan_list);

	vlan_entries = (struct ice_fltr_list_entry *)
	    malloc(sizeof(*vlan_entries) * length, M_ICE, M_NOWAIT | M_ZERO);
	if (!vlan_entries)
		return (ICE_ERR_NO_MEMORY);

	for (u16 i = 0; i < length; i++) {
		vlan_entries[i].fltr_info.lkup_type = ICE_SW_LKUP_VLAN;
		vlan_entries[i].fltr_info.fltr_act = ICE_FWD_TO_VSI;
		vlan_entries[i].fltr_info.flag = ICE_FLTR_TX;
		vlan_entries[i].fltr_info.src_id = ICE_SRC_ID_VSI;
		vlan_entries[i].fltr_info.vsi_handle = vsi->idx;
		vlan_entries[i].fltr_info.l_data.vlan.vlan_id = vid[i];

		LIST_ADD(&vlan_entries[i].list_entry, &vlan_list);
	}

	status = ice_add_vlan(hw, &vlan_list);
	if (!status)
		goto done;

	device_printf(vsi->sc->dev, "Failed to add VLAN filters:\n");
	for (u16 i = 0; i < length; i++) {
		device_printf(vsi->sc->dev,
		    "- vlan %d, status %d\n",
		    vlan_entries[i].fltr_info.l_data.vlan.vlan_id,
		    vlan_entries[i].status);
	}
done:
	free(vlan_entries, M_ICE);
	return (status);
}

/**
 * ice_add_vlan_hw_filter - Add a VLAN filter for a given VSI
 * @vsi: The VSI to add the filter for
 * @vid: VLAN to add
 *
 * Programs a HW filter so that the given VSI will receive the specified VLAN.
 */
enum ice_status
ice_add_vlan_hw_filter(struct ice_vsi *vsi, u16 vid)
{
	return ice_add_vlan_hw_filters(vsi, &vid, 1);
}

/**
 * ice_remove_vlan_hw_filters - Remove multiple VLAN filters for a given VSI
 * @vsi: The VSI to remove the filters from
 * @vid: array of VLAN ids to remove
 * @length: length of vid array
 *
 * Removes previously programmed HW filters for the specified VSI.
 */
enum ice_status
ice_remove_vlan_hw_filters(struct ice_vsi *vsi, u16 *vid, u16 length)
{
	struct ice_hw *hw = &vsi->sc->hw;
	struct ice_list_head vlan_list;
	struct ice_fltr_list_entry *vlan_entries;
	enum ice_status status;

	MPASS(length > 0);

	INIT_LIST_HEAD(&vlan_list);

	vlan_entries = (struct ice_fltr_list_entry *)
	    malloc(sizeof(*vlan_entries) * length, M_ICE, M_NOWAIT | M_ZERO);
	if (!vlan_entries)
		return (ICE_ERR_NO_MEMORY);

	for (u16 i = 0; i < length; i++) {
		vlan_entries[i].fltr_info.lkup_type = ICE_SW_LKUP_VLAN;
		vlan_entries[i].fltr_info.fltr_act = ICE_FWD_TO_VSI;
		vlan_entries[i].fltr_info.flag = ICE_FLTR_TX;
		vlan_entries[i].fltr_info.src_id = ICE_SRC_ID_VSI;
		vlan_entries[i].fltr_info.vsi_handle = vsi->idx;
		vlan_entries[i].fltr_info.l_data.vlan.vlan_id = vid[i];

		LIST_ADD(&vlan_entries[i].list_entry, &vlan_list);
	}

	status = ice_remove_vlan(hw, &vlan_list);
	if (!status)
		goto done;

	device_printf(vsi->sc->dev, "Failed to remove VLAN filters:\n");
	for (u16 i = 0; i < length; i++) {
		device_printf(vsi->sc->dev,
		    "- vlan %d, status %d\n",
		    vlan_entries[i].fltr_info.l_data.vlan.vlan_id,
		    vlan_entries[i].status);
	}
done:
	free(vlan_entries, M_ICE);
	return (status);
}

/**
 * ice_remove_vlan_hw_filter - Remove a VLAN filter for a given VSI
 * @vsi: The VSI to remove the filter from
 * @vid: VLAN to remove
 *
 * Removes a previously programmed HW filter for the specified VSI.
 */
enum ice_status
ice_remove_vlan_hw_filter(struct ice_vsi *vsi, u16 vid)
{
	return ice_remove_vlan_hw_filters(vsi, &vid, 1);
}

#define ICE_SYSCTL_HELP_RX_ITR			\
"\nControl Rx interrupt throttle rate."		\
"\n\t0-8160 - sets interrupt rate in usecs"	\
"\n\t    -1 - reset the Rx itr to default"

/**
 * ice_sysctl_rx_itr - Display or change the Rx ITR for a VSI
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the current Rx ITR value
 * on write: Sets the Rx ITR value, reconfiguring device if it is up
 */
static int
ice_sysctl_rx_itr(SYSCTL_HANDLER_ARGS)
{
	struct ice_vsi *vsi = (struct ice_vsi *)arg1;
	struct ice_softc *sc = vsi->sc;
	int increment, ret;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	ret = sysctl_handle_16(oidp, &vsi->rx_itr, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	if (vsi->rx_itr < 0)
		vsi->rx_itr = ICE_DFLT_RX_ITR;
	if (vsi->rx_itr > ICE_ITR_MAX)
		vsi->rx_itr = ICE_ITR_MAX;

	/* Assume 2usec increment if it hasn't been loaded yet */
	increment = sc->hw.itr_gran ? : 2;

	/* We need to round the value to the hardware's ITR granularity */
	vsi->rx_itr = (vsi->rx_itr / increment ) * increment;

	/* If the driver has finished initializing, then we need to reprogram
	 * the ITR registers now. Otherwise, they will be programmed during
	 * driver initialization.
	 */
	if (ice_test_state(&sc->state, ICE_STATE_DRIVER_INITIALIZED))
		ice_configure_rx_itr(vsi);

	return (0);
}

#define ICE_SYSCTL_HELP_TX_ITR			\
"\nControl Tx interrupt throttle rate."		\
"\n\t0-8160 - sets interrupt rate in usecs"	\
"\n\t    -1 - reset the Tx itr to default"

/**
 * ice_sysctl_tx_itr - Display or change the Tx ITR for a VSI
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * On read: Displays the current Tx ITR value
 * on write: Sets the Tx ITR value, reconfiguring device if it is up
 */
static int
ice_sysctl_tx_itr(SYSCTL_HANDLER_ARGS)
{
	struct ice_vsi *vsi = (struct ice_vsi *)arg1;
	struct ice_softc *sc = vsi->sc;
	int increment, ret;

	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	ret = sysctl_handle_16(oidp, &vsi->tx_itr, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	/* Allow configuring a negative value to reset to the default */
	if (vsi->tx_itr < 0)
		vsi->tx_itr = ICE_DFLT_TX_ITR;
	if (vsi->tx_itr > ICE_ITR_MAX)
		vsi->tx_itr = ICE_ITR_MAX;

	/* Assume 2usec increment if it hasn't been loaded yet */
	increment = sc->hw.itr_gran ? : 2;

	/* We need to round the value to the hardware's ITR granularity */
	vsi->tx_itr = (vsi->tx_itr / increment ) * increment;

	/* If the driver has finished initializing, then we need to reprogram
	 * the ITR registers now. Otherwise, they will be programmed during
	 * driver initialization.
	 */
	if (ice_test_state(&sc->state, ICE_STATE_DRIVER_INITIALIZED))
		ice_configure_tx_itr(vsi);

	return (0);
}

/**
 * ice_add_vsi_tunables - Add tunables and nodes for a VSI
 * @vsi: pointer to VSI structure
 * @parent: parent node to add the tunables under
 *
 * Create a sysctl context for the VSI, so that sysctls for the VSI can be
 * dynamically removed upon VSI removal.
 *
 * Add various tunables and set up the basic node structure for the VSI. Must
 * be called *prior* to ice_add_vsi_sysctls. It should be called as soon as
 * possible after the VSI memory is initialized.
 *
 * VSI specific sysctls with CTLFLAG_TUN should be initialized here so that
 * their values can be read from loader.conf prior to their first use in the
 * driver.
 */
void
ice_add_vsi_tunables(struct ice_vsi *vsi, struct sysctl_oid *parent)
{
	struct sysctl_oid_list *vsi_list;
	char vsi_name[32], vsi_desc[32];

	struct sysctl_oid_list *parent_list = SYSCTL_CHILDREN(parent);

	/* Initialize the sysctl context for this VSI */
	sysctl_ctx_init(&vsi->ctx);

	/* Add a node to collect this VSI's statistics together */
	snprintf(vsi_name, sizeof(vsi_name), "%u", vsi->idx);
	snprintf(vsi_desc, sizeof(vsi_desc), "VSI %u", vsi->idx);
	vsi->vsi_node = SYSCTL_ADD_NODE(&vsi->ctx, parent_list, OID_AUTO, vsi_name,
					CTLFLAG_RD, NULL, vsi_desc);
	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	vsi->rx_itr = ICE_DFLT_TX_ITR;
	SYSCTL_ADD_PROC(&vsi->ctx, vsi_list, OID_AUTO, "rx_itr",
			CTLTYPE_S16 | CTLFLAG_RWTUN,
			vsi, 0, ice_sysctl_rx_itr, "S",
			ICE_SYSCTL_HELP_RX_ITR);

	vsi->tx_itr = ICE_DFLT_TX_ITR;
	SYSCTL_ADD_PROC(&vsi->ctx, vsi_list, OID_AUTO, "tx_itr",
			CTLTYPE_S16 | CTLFLAG_RWTUN,
			vsi, 0, ice_sysctl_tx_itr, "S",
			ICE_SYSCTL_HELP_TX_ITR);
}

/**
 * ice_del_vsi_sysctl_ctx - Delete the sysctl context(s) of a VSI
 * @vsi: the VSI to remove contexts for
 *
 * Free the context for the VSI sysctls. This includes the main context, as
 * well as the per-queue sysctls.
 */
void
ice_del_vsi_sysctl_ctx(struct ice_vsi *vsi)
{
	device_t dev = vsi->sc->dev;
	int err;

	if (vsi->vsi_node) {
		err = sysctl_ctx_free(&vsi->ctx);
		if (err)
			device_printf(dev, "failed to free VSI %d sysctl context, err %s\n",
				      vsi->idx, ice_err_str(err));
		vsi->vsi_node = NULL;
	}
}

/**
 * ice_add_dscp2tc_map_sysctls - Add sysctl tree for DSCP to TC mapping
 * @sc: pointer to device private softc
 * @ctx: the sysctl ctx to use
 * @ctx_list: list of sysctl children for device (to add sysctl tree to)
 *
 * Add a sysctl tree for individual dscp2tc_map sysctls. Each child of this
 * node can map 8 DSCPs to TC values; there are 8 of these in turn for a total
 * of 64 DSCP to TC map values that the user can configure.
 */
void
ice_add_dscp2tc_map_sysctls(struct ice_softc *sc,
			    struct sysctl_ctx_list *ctx,
			    struct sysctl_oid_list *ctx_list)
{
	struct sysctl_oid_list *node_list;
	struct sysctl_oid *node;
	struct sbuf *namebuf, *descbuf;
	int first_dscp_val, last_dscp_val;

	node = SYSCTL_ADD_NODE(ctx, ctx_list, OID_AUTO, "dscp2tc_map", CTLFLAG_RD,
			       NULL, "Map of DSCP values to DCB TCs");
	node_list = SYSCTL_CHILDREN(node);

	namebuf = sbuf_new_auto();
	descbuf = sbuf_new_auto();
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		sbuf_clear(namebuf);
		sbuf_clear(descbuf);

		first_dscp_val = i * 8;
		last_dscp_val = first_dscp_val + 7;

		sbuf_printf(namebuf, "%d-%d", first_dscp_val, last_dscp_val);
		sbuf_printf(descbuf, "Map DSCP values %d to %d to TCs",
			    first_dscp_val, last_dscp_val);

		sbuf_finish(namebuf);
		sbuf_finish(descbuf);

		SYSCTL_ADD_PROC(ctx, node_list,
		    OID_AUTO, sbuf_data(namebuf), CTLTYPE_STRING | CTLFLAG_RW,
		    sc, i, ice_sysctl_dscp2tc_map, "A", sbuf_data(descbuf));
	}

	sbuf_delete(namebuf);
	sbuf_delete(descbuf);
}

/**
 * ice_add_device_tunables - Add early tunable sysctls and sysctl nodes
 * @sc: device private structure
 *
 * Add per-device dynamic tunable sysctls, and setup the general sysctl trees
 * for re-use by ice_add_device_sysctls.
 *
 * In order for the sysctl fields to be initialized before use, this function
 * should be called as early as possible during attach activities.
 *
 * Any non-global sysctl marked as CTLFLAG_TUN should likely be initialized
 * here in this function, rather than later in ice_add_device_sysctls.
 *
 * To make things easier, this function is also expected to setup the various
 * sysctl nodes in addition to tunables so that other sysctls which can't be
 * initialized early can hook into the same nodes.
 */
void
ice_add_device_tunables(struct ice_softc *sc)
{
	device_t dev = sc->dev;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
		SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	sc->enable_health_events = ice_enable_health_events;

	SYSCTL_ADD_BOOL(ctx, ctx_list, OID_AUTO, "enable_health_events",
			CTLFLAG_RDTUN, &sc->enable_health_events, 0,
			"Enable FW health event reporting for this PF");

	/* Add a node to track VSI sysctls. Keep track of the node in the
	 * softc so that we can hook other sysctls into it later. This
	 * includes both the VSI statistics, as well as potentially dynamic
	 * VSIs in the future.
	 */

	sc->vsi_sysctls = SYSCTL_ADD_NODE(ctx, ctx_list, OID_AUTO, "vsi",
					  CTLFLAG_RD, NULL, "VSI Configuration and Statistics");

	/* Add debug tunables */
	ice_add_debug_tunables(sc);
}

/**
 * ice_sysctl_dump_mac_filters - Dump a list of all HW MAC Filters
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for "mac_filters" sysctl to dump the programmed MAC filters.
 */
static int
ice_sysctl_dump_mac_filters(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_fltr_mgmt_list_entry *fm_entry;
	struct ice_list_head *rule_head;
	struct ice_lock *rule_lock;
	struct ice_fltr_info *fi;
	struct sbuf *sbuf;
	int ret;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Wire the old buffer so we can take a non-sleepable lock */
	ret = sysctl_wire_old_buffer(req, 0);
	if (ret)
		return (ret);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	rule_lock = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rule_lock;
	rule_head = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rules;

	sbuf_printf(sbuf, "MAC Filter List");

	ice_acquire_lock(rule_lock);

	LIST_FOR_EACH_ENTRY(fm_entry, rule_head, ice_fltr_mgmt_list_entry, list_entry) {
		fi = &fm_entry->fltr_info;

		sbuf_printf(sbuf,
			    "\nmac = %6D, vsi_handle = %3d, fw_act_flag = %5s, lb_en = %1d, lan_en = %1d, fltr_act = %15s, fltr_rule_id = %d",
			    fi->l_data.mac.mac_addr, ":", fi->vsi_handle,
			    ice_fltr_flag_str(fi->flag), fi->lb_en, fi->lan_en,
			    ice_fwd_act_str(fi->fltr_act), fi->fltr_rule_id);

		/* if we have a vsi_list_info, print some information about that */
		if (fm_entry->vsi_list_info) {
			sbuf_printf(sbuf,
				    ", vsi_count = %3d, vsi_list_id = %3d, ref_cnt = %3d",
				    fm_entry->vsi_count,
				    fm_entry->vsi_list_info->vsi_list_id,
				    fm_entry->vsi_list_info->ref_cnt);
		}
	}

	ice_release_lock(rule_lock);

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_dump_vlan_filters - Dump a list of all HW VLAN Filters
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for "vlan_filters" sysctl to dump the programmed VLAN filters.
 */
static int
ice_sysctl_dump_vlan_filters(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_fltr_mgmt_list_entry *fm_entry;
	struct ice_list_head *rule_head;
	struct ice_lock *rule_lock;
	struct ice_fltr_info *fi;
	struct sbuf *sbuf;
	int ret;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Wire the old buffer so we can take a non-sleepable lock */
	ret = sysctl_wire_old_buffer(req, 0);
	if (ret)
		return (ret);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	rule_lock = &sw->recp_list[ICE_SW_LKUP_VLAN].filt_rule_lock;
	rule_head = &sw->recp_list[ICE_SW_LKUP_VLAN].filt_rules;

	sbuf_printf(sbuf, "VLAN Filter List");

	ice_acquire_lock(rule_lock);

	LIST_FOR_EACH_ENTRY(fm_entry, rule_head, ice_fltr_mgmt_list_entry, list_entry) {
		fi = &fm_entry->fltr_info;

		sbuf_printf(sbuf,
			    "\nvlan_id = %4d, vsi_handle = %3d, fw_act_flag = %5s, lb_en = %1d, lan_en = %1d, fltr_act = %15s, fltr_rule_id = %4d",
			    fi->l_data.vlan.vlan_id, fi->vsi_handle,
			    ice_fltr_flag_str(fi->flag), fi->lb_en, fi->lan_en,
			    ice_fwd_act_str(fi->fltr_act), fi->fltr_rule_id);

		/* if we have a vsi_list_info, print some information about that */
		if (fm_entry->vsi_list_info) {
			sbuf_printf(sbuf,
				    ", vsi_count = %3d, vsi_list_id = %3d, ref_cnt = %3d",
				    fm_entry->vsi_count,
				    fm_entry->vsi_list_info->vsi_list_id,
				    fm_entry->vsi_list_info->ref_cnt);
		}
	}

	ice_release_lock(rule_lock);

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_dump_ethertype_filters - Dump a list of all HW Ethertype filters
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for "ethertype_filters" sysctl to dump the programmed Ethertype
 * filters.
 */
static int
ice_sysctl_dump_ethertype_filters(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_fltr_mgmt_list_entry *fm_entry;
	struct ice_list_head *rule_head;
	struct ice_lock *rule_lock;
	struct ice_fltr_info *fi;
	struct sbuf *sbuf;
	int ret;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Wire the old buffer so we can take a non-sleepable lock */
	ret = sysctl_wire_old_buffer(req, 0);
	if (ret)
		return (ret);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	rule_lock = &sw->recp_list[ICE_SW_LKUP_ETHERTYPE].filt_rule_lock;
	rule_head = &sw->recp_list[ICE_SW_LKUP_ETHERTYPE].filt_rules;

	sbuf_printf(sbuf, "Ethertype Filter List");

	ice_acquire_lock(rule_lock);

	LIST_FOR_EACH_ENTRY(fm_entry, rule_head, ice_fltr_mgmt_list_entry, list_entry) {
		fi = &fm_entry->fltr_info;

		sbuf_printf(sbuf,
			    "\nethertype = 0x%04x, vsi_handle = %3d, fw_act_flag = %5s, lb_en = %1d, lan_en = %1d, fltr_act = %15s, fltr_rule_id = %4d",
			fi->l_data.ethertype_mac.ethertype,
			fi->vsi_handle, ice_fltr_flag_str(fi->flag),
			fi->lb_en, fi->lan_en, ice_fwd_act_str(fi->fltr_act),
			fi->fltr_rule_id);

		/* if we have a vsi_list_info, print some information about that */
		if (fm_entry->vsi_list_info) {
			sbuf_printf(sbuf,
				    ", vsi_count = %3d, vsi_list_id = %3d, ref_cnt = %3d",
				    fm_entry->vsi_count,
				    fm_entry->vsi_list_info->vsi_list_id,
				    fm_entry->vsi_list_info->ref_cnt);
		}
	}

	ice_release_lock(rule_lock);

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_dump_ethertype_mac_filters - Dump a list of all HW Ethertype/MAC filters
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for "ethertype_mac_filters" sysctl to dump the programmed
 * Ethertype/MAC filters.
 */
static int
ice_sysctl_dump_ethertype_mac_filters(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_fltr_mgmt_list_entry *fm_entry;
	struct ice_list_head *rule_head;
	struct ice_lock *rule_lock;
	struct ice_fltr_info *fi;
	struct sbuf *sbuf;
	int ret;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Wire the old buffer so we can take a non-sleepable lock */
	ret = sysctl_wire_old_buffer(req, 0);
	if (ret)
		return (ret);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	rule_lock = &sw->recp_list[ICE_SW_LKUP_ETHERTYPE_MAC].filt_rule_lock;
	rule_head = &sw->recp_list[ICE_SW_LKUP_ETHERTYPE_MAC].filt_rules;

	sbuf_printf(sbuf, "Ethertype/MAC Filter List");

	ice_acquire_lock(rule_lock);

	LIST_FOR_EACH_ENTRY(fm_entry, rule_head, ice_fltr_mgmt_list_entry, list_entry) {
		fi = &fm_entry->fltr_info;

		sbuf_printf(sbuf,
			    "\nethertype = 0x%04x, mac = %6D, vsi_handle = %3d, fw_act_flag = %5s, lb_en = %1d, lan_en = %1d, fltr_act = %15s, fltr_rule_id = %4d",
			    fi->l_data.ethertype_mac.ethertype,
			    fi->l_data.ethertype_mac.mac_addr, ":",
			    fi->vsi_handle, ice_fltr_flag_str(fi->flag),
			    fi->lb_en, fi->lan_en, ice_fwd_act_str(fi->fltr_act),
			    fi->fltr_rule_id);

		/* if we have a vsi_list_info, print some information about that */
		if (fm_entry->vsi_list_info) {
			sbuf_printf(sbuf,
				    ", vsi_count = %3d, vsi_list_id = %3d, ref_cnt = %3d",
				    fm_entry->vsi_count,
				    fm_entry->vsi_list_info->vsi_list_id,
				    fm_entry->vsi_list_info->ref_cnt);
		}
	}

	ice_release_lock(rule_lock);

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_dump_state_flags - Dump device driver state flags
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for "state" sysctl to display currently set driver state flags.
 */
static int
ice_sysctl_dump_state_flags(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct sbuf *sbuf;
	u32 copied_state;
	unsigned int i;
	bool at_least_one = false;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Make a copy of the state to ensure we display coherent values */
	copied_state = atomic_load_acq_32(&sc->state);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	/* Add the string for each set state to the sbuf */
	for (i = 0; i < 32; i++) {
		if (copied_state & BIT(i)) {
			const char *str = ice_state_to_str((enum ice_state)i);

			at_least_one = true;

			if (str)
				sbuf_printf(sbuf, "\n%s", str);
			else
				sbuf_printf(sbuf, "\nBIT(%u)", i);
		}
	}

	if (!at_least_one)
		sbuf_printf(sbuf, "Nothing set");

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

#define ICE_SYSCTL_DEBUG_MASK_HELP \
"\nSelect debug statements to print to kernel messages"		\
"\nFlags:"							\
"\n\t        0x1 - Function Tracing"				\
"\n\t        0x2 - Driver Initialization"			\
"\n\t        0x4 - Release"					\
"\n\t        0x8 - FW Logging"					\
"\n\t       0x10 - Link"					\
"\n\t       0x20 - PHY"						\
"\n\t       0x40 - Queue Context"				\
"\n\t       0x80 - NVM"						\
"\n\t      0x100 - LAN"						\
"\n\t      0x200 - Flow"					\
"\n\t      0x400 - DCB"						\
"\n\t      0x800 - Diagnostics"					\
"\n\t     0x1000 - Flow Director"				\
"\n\t     0x2000 - Switch"					\
"\n\t     0x4000 - Scheduler"					\
"\n\t     0x8000 - RDMA"					\
"\n\t    0x10000 - DDP Package"					\
"\n\t    0x20000 - Resources"					\
"\n\t    0x40000 - ACL"						\
"\n\t    0x80000 - PTP"						\
"\n\t   0x100000 - Admin Queue messages"			\
"\n\t   0x200000 - Admin Queue descriptors"			\
"\n\t   0x400000 - Admin Queue descriptor buffers"		\
"\n\t   0x800000 - Admin Queue commands"			\
"\n\t  0x1000000 - Parser"					\
"\n\t  ..."							\
"\n\t  0x8000000 - (Reserved for user)"				\
"\n\t"								\
"\nUse \"sysctl -x\" to view flags properly."

/**
 * ice_add_debug_tunables - Add tunables helpful for debugging the device driver
 * @sc: device private structure
 *
 * Add sysctl tunable values related to debugging the device driver. For now,
 * this means a tunable to set the debug mask early during driver load.
 *
 * The debug node will be marked CTLFLAG_SKIP unless INVARIANTS is defined, so
 * that in normal kernel builds, these will all be hidden, but on a debug
 * kernel they will be more easily visible.
 */
static void
ice_add_debug_tunables(struct ice_softc *sc)
{
	struct sysctl_oid_list *debug_list;
	device_t dev = sc->dev;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	sc->debug_sysctls = SYSCTL_ADD_NODE(ctx, ctx_list, OID_AUTO, "debug",
					    ICE_CTLFLAG_DEBUG | CTLFLAG_RD,
					    NULL, "Debug Sysctls");
	debug_list = SYSCTL_CHILDREN(sc->debug_sysctls);

	SYSCTL_ADD_U64(ctx, debug_list, OID_AUTO, "debug_mask",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RWTUN,
		       &sc->hw.debug_mask, 0,
		       ICE_SYSCTL_DEBUG_MASK_HELP);

	/* Load the default value from the global sysctl first */
	sc->enable_tx_fc_filter = ice_enable_tx_fc_filter;

	SYSCTL_ADD_BOOL(ctx, debug_list, OID_AUTO, "enable_tx_fc_filter",
			ICE_CTLFLAG_DEBUG | CTLFLAG_RDTUN,
			&sc->enable_tx_fc_filter, 0,
			"Drop Ethertype 0x8808 control frames originating from software on this PF");

	sc->tx_balance_en = ice_tx_balance_en;
	SYSCTL_ADD_BOOL(ctx, debug_list, OID_AUTO, "tx_balance",
			ICE_CTLFLAG_DEBUG | CTLFLAG_RWTUN,
			&sc->tx_balance_en, 0,
			"Enable 5-layer scheduler topology");

	/* Load the default value from the global sysctl first */
	sc->enable_tx_lldp_filter = ice_enable_tx_lldp_filter;

	SYSCTL_ADD_BOOL(ctx, debug_list, OID_AUTO, "enable_tx_lldp_filter",
			ICE_CTLFLAG_DEBUG | CTLFLAG_RDTUN,
			&sc->enable_tx_lldp_filter, 0,
			"Drop Ethertype 0x88cc LLDP frames originating from software on this PF");

	ice_add_fw_logging_tunables(sc, sc->debug_sysctls);
}

#define ICE_SYSCTL_HELP_REQUEST_RESET		\
"\nRequest the driver to initiate a reset."	\
"\n\tpfr - Initiate a PF reset"			\
"\n\tcorer - Initiate a CORE reset"		\
"\n\tglobr - Initiate a GLOBAL reset"

/**
 * @var rl_sysctl_ticks
 * @brief timestamp for latest reset request sysctl call
 *
 * Helps rate-limit the call to the sysctl which resets the device
 */
int rl_sysctl_ticks = 0;

/**
 * ice_sysctl_request_reset - Request that the driver initiate a reset
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Callback for "request_reset" sysctl to request that the driver initiate
 * a reset. Expects to be passed one of the following strings
 *
 * "pfr" - Initiate a PF reset
 * "corer" - Initiate a CORE reset
 * "globr" - Initiate a Global reset
 */
static int
ice_sysctl_request_reset(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;
	enum ice_reset_req reset_type = ICE_RESET_INVAL;
	const char *reset_message;
	int ret;

	/* Buffer to store the requested reset string. Must contain enough
	 * space to store the largest expected reset string, which currently
	 * means 6 bytes of space.
	 */
	char reset[6] = "";

	UNREFERENCED_PARAMETER(arg2);

	ret = priv_check(curthread, PRIV_DRIVER);
	if (ret)
		return (ret);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Read in the requested reset type. */
	ret = sysctl_handle_string(oidp, reset, sizeof(reset), req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	if (strcmp(reset, "pfr") == 0) {
		reset_message = "Requesting a PF reset";
		reset_type = ICE_RESET_PFR;
	} else if (strcmp(reset, "corer") == 0) {
		reset_message = "Initiating a CORE reset";
		reset_type = ICE_RESET_CORER;
	} else if (strcmp(reset, "globr") == 0) {
		reset_message = "Initiating a GLOBAL reset";
		reset_type = ICE_RESET_GLOBR;
	} else if (strcmp(reset, "empr") == 0) {
		device_printf(sc->dev, "Triggering an EMP reset via software is not currently supported\n");
		return (EOPNOTSUPP);
	}

	if (reset_type == ICE_RESET_INVAL) {
		device_printf(sc->dev, "%s is not a valid reset request\n", reset);
		return (EINVAL);
	}

	/*
	 * Rate-limit the frequency at which this function is called.
	 * Assuming this is called successfully once, typically,
	 * everything should be handled within the allotted time frame.
	 * However, in the odd setup situations, we've also put in
	 * guards for when the reset has finished, but we're in the
	 * process of rebuilding. And instead of queueing an intent,
	 * simply error out and let the caller retry, if so desired.
	 */
	if (TICKS_2_MSEC(ticks - rl_sysctl_ticks) < 500) {
		device_printf(sc->dev,
		    "Call frequency too high. Operation aborted.\n");
		return (EBUSY);
	}
	rl_sysctl_ticks = ticks;

	if (TICKS_2_MSEC(ticks - sc->rebuild_ticks) < 100) {
		device_printf(sc->dev, "Device rebuilding. Operation aborted.\n");
		return (EBUSY);
	}

	if (rd32(hw, GLGEN_RSTAT) & GLGEN_RSTAT_DEVSTATE_M) {
		device_printf(sc->dev, "Device in reset. Operation aborted.\n");
		return (EBUSY);
	}

	device_printf(sc->dev, "%s\n", reset_message);

	/* Initiate the PF reset during the admin status task */
	if (reset_type == ICE_RESET_PFR) {
		ice_set_state(&sc->state, ICE_STATE_RESET_PFR_REQ);
		return (0);
	}

	/*
	 * Other types of resets including CORE and GLOBAL resets trigger an
	 * interrupt on all PFs. Initiate the reset now. Preparation and
	 * rebuild logic will be handled by the admin status task.
	 */
	status = ice_reset(hw, reset_type);

	/*
	 * Resets can take a long time and we still don't want another call
	 * to this function before we settle down.
	 */
	rl_sysctl_ticks = ticks;

	if (status) {
		device_printf(sc->dev, "failed to initiate device reset, err %s\n",
			      ice_status_str(status));
		ice_set_state(&sc->state, ICE_STATE_RESET_FAILED);
		return (EFAULT);
	}

	return (0);
}

#define ICE_SYSCTL_HELP_FW_DEBUG_DUMP_CLUSTER_SETTING		\
"\nSelect clusters to dump with \"dump\" sysctl"		\
"\nFlags:"							\
"\n\t   0x1 - Switch"						\
"\n\t   0x2 - ACL"						\
"\n\t   0x4 - Tx Scheduler"					\
"\n\t   0x8 - Profile Configuration"				\
"\n\t  0x20 - Link"						\
"\n\t  0x80 - DCB"						\
"\n\t 0x100 - L2P"						\
"\n\t"								\
"\nUse \"sysctl -x\" to view flags properly."

/**
 * ice_sysctl_fw_debug_dump_cluster_setting - Set which clusters to dump
 *     from FW when FW debug dump occurs
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 */
static int
ice_sysctl_fw_debug_dump_cluster_setting(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	device_t dev = sc->dev;
	u16 clusters;
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	ret = priv_check(curthread, PRIV_DRIVER);
	if (ret)
		return (ret);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	clusters = sc->fw_debug_dump_cluster_mask;

	ret = sysctl_handle_16(oidp, &clusters, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	if (!clusters ||
	    (clusters & ~(ICE_FW_DEBUG_DUMP_VALID_CLUSTER_MASK))) {
		device_printf(dev,
		    "%s: ERROR: Incorrect settings requested\n",
		    __func__);
		return (EINVAL);
	}

	sc->fw_debug_dump_cluster_mask = clusters;

	return (0);
}

#define ICE_FW_DUMP_AQ_COUNT_LIMIT	(10000)

/**
 * ice_fw_debug_dump_print_cluster - Print formatted cluster data from FW
 * @sc: the device softc
 * @sbuf: initialized sbuf to print data to
 * @cluster_id: FW cluster ID to print data from
 *
 * Reads debug data from the specified cluster id in the FW and prints it to
 * the input sbuf. This function issues multiple AQ commands to the FW in
 * order to get all of the data in the cluster.
 *
 * @remark Only intended to be used by the sysctl handler
 * ice_sysctl_fw_debug_dump_do_dump
 */
static void
ice_fw_debug_dump_print_cluster(struct ice_softc *sc, struct sbuf *sbuf, u16 cluster_id)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u16 data_buf_size = ICE_AQ_MAX_BUF_LEN;
	const u8 reserved_buf[8] = {};
	enum ice_status status;
	int counter = 0;
	u8 *data_buf;

	/* Other setup */
	data_buf = (u8 *)malloc(data_buf_size, M_ICE, M_NOWAIT | M_ZERO);
	if (!data_buf)
		return;

	/* Input parameters / loop variables */
	u16 table_id = 0;
	u32 offset = 0;

	/* Output from the Get Internal Data AQ command */
	u16 ret_buf_size = 0;
	u16 ret_next_table = 0;
	u32 ret_next_index = 0;

	ice_debug(hw, ICE_DBG_DIAG, "%s: dumping cluster id %d\n", __func__,
	    cluster_id);

	for (;;) {
		/* Do not trust the FW behavior to be completely correct */
		if (counter++ >= ICE_FW_DUMP_AQ_COUNT_LIMIT) {
			device_printf(dev,
			    "%s: Exceeded counter limit for cluster %d\n",
			    __func__, cluster_id);
			break;
		}

		ice_debug(hw, ICE_DBG_DIAG, "---\n");
		ice_debug(hw, ICE_DBG_DIAG,
		    "table_id 0x%04x offset 0x%08x buf_size %d\n",
		    table_id, offset, data_buf_size);

		status = ice_aq_get_internal_data(hw, cluster_id, table_id,
		    offset, data_buf, data_buf_size, &ret_buf_size,
		    &ret_next_table, &ret_next_index, NULL);
		if (status) {
			device_printf(dev,
			    "%s: ice_aq_get_internal_data in cluster %d: err %s aq_err %s\n",
			    __func__, cluster_id, ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
			break;
		}

		ice_debug(hw, ICE_DBG_DIAG,
		    "ret_table_id 0x%04x ret_offset 0x%08x ret_buf_size %d\n",
		    ret_next_table, ret_next_index, ret_buf_size);

		/* Print cluster id */
		u32 print_cluster_id = (u32)cluster_id;
		sbuf_bcat(sbuf, &print_cluster_id, sizeof(print_cluster_id));
		/* Print table id */
		u32 print_table_id = (u32)table_id;
		sbuf_bcat(sbuf, &print_table_id, sizeof(print_table_id));
		/* Print table length */
		u32 print_table_length = (u32)ret_buf_size;
		sbuf_bcat(sbuf, &print_table_length, sizeof(print_table_length));
		/* Print current offset */
		u32 print_curr_offset = offset;
		sbuf_bcat(sbuf, &print_curr_offset, sizeof(print_curr_offset));
		/* Print reserved bytes */
		sbuf_bcat(sbuf, reserved_buf, sizeof(reserved_buf));
		/* Print data */
		sbuf_bcat(sbuf, data_buf, ret_buf_size);

		/* Adjust loop variables */
		memset(data_buf, 0, data_buf_size);
		bool same_table_next = (table_id == ret_next_table);
		bool last_table_next = (ret_next_table == 0xff || ret_next_table == 0xffff);
		bool last_offset_next = (ret_next_index == 0xffffffff || ret_next_index == 0);

		if ((!same_table_next && !last_offset_next) ||
		    (same_table_next && last_table_next)) {
			device_printf(dev,
			    "%s: Unexpected conditions for same_table_next(%d) last_table_next(%d) last_offset_next(%d), ending cluster (%d)\n",
			    __func__, same_table_next, last_table_next, last_offset_next, cluster_id);
			break;
		}

		if (!same_table_next && !last_table_next && last_offset_next) {
			/* We've hit the end of the table */
			table_id = ret_next_table;
			offset = 0;
		}
		else if (!same_table_next && last_table_next && last_offset_next) {
			/* We've hit the end of the cluster */
			break;
		}
		else if (same_table_next && !last_table_next && last_offset_next) {
			if (cluster_id == 0x1 && table_id < 39)
				table_id += 1;
			else
				break;
		}
		else { /* if (same_table_next && !last_table_next && !last_offset_next) */
			/* More data left in the table */
			offset = ret_next_index;
		}
	}

	free(data_buf, M_ICE);
}

#define ICE_SYSCTL_HELP_FW_DEBUG_DUMP_DO_DUMP \
"\nWrite 1 to output a FW debug dump containing the clusters specified by the \"clusters\" sysctl" \
"\nThe \"-b\" flag must be used in order to dump this data as binary data because" \
"\nthis data is opaque and not a string."

#define ICE_FW_DUMP_BASE_TEXT_SIZE	(1024 * 1024)
#define ICE_FW_DUMP_CLUST0_TEXT_SIZE	(2 * 1024 * 1024)
#define ICE_FW_DUMP_CLUST1_TEXT_SIZE	(128 * 1024)
#define ICE_FW_DUMP_CLUST2_TEXT_SIZE	(2 * 1024 * 1024)

/**
 * ice_sysctl_fw_debug_dump_do_dump - Dump data from FW to sysctl output
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Sysctl handler for the debug.dump.dump sysctl. Prints out a specially-
 * formatted dump of some debug FW data intended to be processed by a special
 * Intel tool. Prints out the cluster data specified by the "clusters"
 * sysctl.
 *
 * @remark The actual AQ calls and printing are handled by a helper
 * function above.
 */
static int
ice_sysctl_fw_debug_dump_do_dump(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	device_t dev = sc->dev;
	struct sbuf *sbuf;
	int bit, ret;

	UNREFERENCED_PARAMETER(arg2);

	ret = priv_check(curthread, PRIV_DRIVER);
	if (ret)
		return (ret);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* If the user hasn't written "1" to this sysctl yet: */
	if (!ice_test_state(&sc->state, ICE_STATE_DO_FW_DEBUG_DUMP)) {
		/* Avoid output on the first set of reads to this sysctl in
		 * order to prevent a null byte from being written to the
		 * end result when called via sysctl(8).
		 */
		if (req->oldptr == NULL && req->newptr == NULL) {
			ret = SYSCTL_OUT(req, 0, 0);
			return (ret);
		}

		char input_buf[2] = "";
		ret = sysctl_handle_string(oidp, input_buf, sizeof(input_buf), req);
		if ((ret) || (req->newptr == NULL))
			return (ret);

		/* If we get '1', then indicate we'll do a dump in the next
		 * sysctl read call.
		 */
		if (input_buf[0] == '1') {
			ice_set_state(&sc->state, ICE_STATE_DO_FW_DEBUG_DUMP);
			return (0);
		}

		return (EINVAL);
	}

	/* --- FW debug dump state is set --- */

	if (!sc->fw_debug_dump_cluster_mask) {
		device_printf(dev,
		    "%s: Debug Dump failed because no cluster was specified.\n",
		    __func__);
		ret = EINVAL;
		goto out;
	}

	/* Caller just wants the upper bound for size */
	if (req->oldptr == NULL && req->newptr == NULL) {
		size_t est_output_len = ICE_FW_DUMP_BASE_TEXT_SIZE;
		if (sc->fw_debug_dump_cluster_mask & 0x1)
			est_output_len += ICE_FW_DUMP_CLUST0_TEXT_SIZE;
		if (sc->fw_debug_dump_cluster_mask & 0x2)
			est_output_len += ICE_FW_DUMP_CLUST1_TEXT_SIZE;
		if (sc->fw_debug_dump_cluster_mask & 0x4)
			est_output_len += ICE_FW_DUMP_CLUST2_TEXT_SIZE;

		ret = SYSCTL_OUT(req, 0, est_output_len);
		return (ret);
	}

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	sbuf_clear_flags(sbuf, SBUF_INCLUDENUL);

	ice_debug(&sc->hw, ICE_DBG_DIAG, "%s: Debug Dump running...\n", __func__);

	for_each_set_bit(bit, &sc->fw_debug_dump_cluster_mask,
	    sizeof(sc->fw_debug_dump_cluster_mask) * 8)
		ice_fw_debug_dump_print_cluster(sc, sbuf, bit);

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

out:
	ice_clear_state(&sc->state, ICE_STATE_DO_FW_DEBUG_DUMP);
	return (ret);
}

/**
 * ice_add_debug_sysctls - Add sysctls helpful for debugging the device driver
 * @sc: device private structure
 *
 * Add sysctls related to debugging the device driver. Generally these should
 * simply be sysctls which dump internal driver state, to aid in understanding
 * what the driver is doing.
 */
static void
ice_add_debug_sysctls(struct ice_softc *sc)
{
	struct sysctl_oid *sw_node, *dump_node;
	struct sysctl_oid_list *debug_list, *sw_list, *dump_list;
	device_t dev = sc->dev;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);

	debug_list = SYSCTL_CHILDREN(sc->debug_sysctls);

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "request_reset",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_WR, sc, 0,
			ice_sysctl_request_reset, "A",
			ICE_SYSCTL_HELP_REQUEST_RESET);

	SYSCTL_ADD_U32(ctx, debug_list, OID_AUTO, "pfr_count",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RD,
		       &sc->soft_stats.pfr_count, 0,
		       "# of PF resets handled");

	SYSCTL_ADD_U32(ctx, debug_list, OID_AUTO, "corer_count",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RD,
		       &sc->soft_stats.corer_count, 0,
		       "# of CORE resets handled");

	SYSCTL_ADD_U32(ctx, debug_list, OID_AUTO, "globr_count",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RD,
		       &sc->soft_stats.globr_count, 0,
		       "# of Global resets handled");

	SYSCTL_ADD_U32(ctx, debug_list, OID_AUTO, "empr_count",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RD,
		       &sc->soft_stats.empr_count, 0,
		       "# of EMP resets handled");

	SYSCTL_ADD_U32(ctx, debug_list, OID_AUTO, "tx_mdd_count",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RD,
		       &sc->soft_stats.tx_mdd_count, 0,
		       "# of Tx MDD events detected");

	SYSCTL_ADD_U32(ctx, debug_list, OID_AUTO, "rx_mdd_count",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RD,
		       &sc->soft_stats.rx_mdd_count, 0,
		       "# of Rx MDD events detected");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "state",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_dump_state_flags, "A",
			"Driver State Flags");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "phy_type_low",
			ICE_CTLFLAG_DEBUG | CTLTYPE_U64 | CTLFLAG_RW, sc, 0,
			ice_sysctl_phy_type_low, "QU",
			"PHY type Low from Get PHY Caps/Set PHY Cfg");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "phy_type_high",
			ICE_CTLFLAG_DEBUG | CTLTYPE_U64 | CTLFLAG_RW, sc, 0,
			ice_sysctl_phy_type_high, "QU",
			"PHY type High from Get PHY Caps/Set PHY Cfg");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "phy_sw_caps",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRUCT | CTLFLAG_RD, sc, 0,
			ice_sysctl_phy_sw_caps, "",
			"Get PHY Capabilities (Software configuration)");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "phy_nvm_caps",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRUCT | CTLFLAG_RD, sc, 0,
			ice_sysctl_phy_nvm_caps, "",
			"Get PHY Capabilities (NVM configuration)");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "phy_topo_caps",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRUCT | CTLFLAG_RD, sc, 0,
			ice_sysctl_phy_topo_caps, "",
			"Get PHY Capabilities (Topology configuration)");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "phy_link_status",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRUCT | CTLFLAG_RD, sc, 0,
			ice_sysctl_phy_link_status, "",
			"Get PHY Link Status");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "read_i2c_diag_data",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_read_i2c_diag_data, "A",
			"Dump selected diagnostic data from FW");

	SYSCTL_ADD_U32(ctx, debug_list, OID_AUTO, "fw_build",
		       ICE_CTLFLAG_DEBUG | CTLFLAG_RD, &sc->hw.fw_build, 0,
		       "FW Build ID");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "os_ddp_version",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_os_pkg_version, "A",
			"DDP package name and version found in ice_ddp");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "cur_lldp_persist_status",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_fw_cur_lldp_persist_status, "A",
			"Current LLDP persistent status");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "dflt_lldp_persist_status",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_fw_dflt_lldp_persist_status, "A",
			"Default LLDP persistent status");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "negotiated_fc",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_negotiated_fc, "A",
			"Current Negotiated Flow Control mode");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "local_dcbx_cfg",
			CTLTYPE_STRING | CTLFLAG_RD, sc, ICE_AQ_LLDP_MIB_LOCAL,
			ice_sysctl_dump_dcbx_cfg, "A",
			"Dumps Local MIB information from firmware");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "remote_dcbx_cfg",
			CTLTYPE_STRING | CTLFLAG_RD, sc, ICE_AQ_LLDP_MIB_REMOTE,
			ice_sysctl_dump_dcbx_cfg, "A",
			"Dumps Remote MIB information from firmware");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "pf_vsi_cfg", CTLTYPE_STRING | CTLFLAG_RD,
			sc, 0, ice_sysctl_dump_vsi_cfg, "A",
			"Dumps Selected PF VSI parameters from firmware");

	SYSCTL_ADD_PROC(ctx, debug_list, OID_AUTO, "query_port_ets", CTLTYPE_STRING | CTLFLAG_RD,
			sc, 0, ice_sysctl_query_port_ets, "A",
			"Prints selected output from Query Port ETS AQ command");

	sw_node = SYSCTL_ADD_NODE(ctx, debug_list, OID_AUTO, "switch",
				  ICE_CTLFLAG_DEBUG | CTLFLAG_RD, NULL,
				  "Switch Configuration");
	sw_list = SYSCTL_CHILDREN(sw_node);

	SYSCTL_ADD_PROC(ctx, sw_list, OID_AUTO, "mac_filters",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_dump_mac_filters, "A",
			"MAC Filters");

	SYSCTL_ADD_PROC(ctx, sw_list, OID_AUTO, "vlan_filters",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_dump_vlan_filters, "A",
			"VLAN Filters");

	SYSCTL_ADD_PROC(ctx, sw_list, OID_AUTO, "ethertype_filters",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_dump_ethertype_filters, "A",
			"Ethertype Filters");

	SYSCTL_ADD_PROC(ctx, sw_list, OID_AUTO, "ethertype_mac_filters",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RD, sc, 0,
			ice_sysctl_dump_ethertype_mac_filters, "A",
			"Ethertype/MAC Filters");

	dump_node = SYSCTL_ADD_NODE(ctx, debug_list, OID_AUTO, "dump",
				  ICE_CTLFLAG_DEBUG | CTLFLAG_RD, NULL,
				  "Internal FW Dump");
	dump_list = SYSCTL_CHILDREN(dump_node);

	SYSCTL_ADD_PROC(ctx, dump_list, OID_AUTO, "clusters",
			ICE_CTLFLAG_DEBUG | CTLTYPE_U16 | CTLFLAG_RW, sc, 0,
			ice_sysctl_fw_debug_dump_cluster_setting, "SU",
			ICE_SYSCTL_HELP_FW_DEBUG_DUMP_CLUSTER_SETTING);

	SYSCTL_ADD_PROC(ctx, dump_list, OID_AUTO, "dump",
			ICE_CTLFLAG_DEBUG | CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
			ice_sysctl_fw_debug_dump_do_dump, "",
			ICE_SYSCTL_HELP_FW_DEBUG_DUMP_DO_DUMP);
}

/**
 * ice_vsi_disable_tx - Disable (unconfigure) Tx queues for a VSI
 * @vsi: the VSI to disable
 *
 * Disables the Tx queues associated with this VSI. Essentially the opposite
 * of ice_cfg_vsi_for_tx.
 */
int
ice_vsi_disable_tx(struct ice_vsi *vsi)
{
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;
	u32 *q_teids;
	u16 *q_ids, *q_handles;
	size_t q_teids_size, q_ids_size, q_handles_size;
	int tc, j, buf_idx, err = 0;

	if (vsi->num_tx_queues > 255)
		return (ENOSYS);

	q_teids_size = sizeof(*q_teids) * vsi->num_tx_queues;
	q_teids = (u32 *)malloc(q_teids_size, M_ICE, M_NOWAIT|M_ZERO);
	if (!q_teids)
		return (ENOMEM);

	q_ids_size = sizeof(*q_ids) * vsi->num_tx_queues;
	q_ids = (u16 *)malloc(q_ids_size, M_ICE, M_NOWAIT|M_ZERO);
	if (!q_ids) {
		err = (ENOMEM);
		goto free_q_teids;
	}

	q_handles_size = sizeof(*q_handles) * vsi->num_tx_queues;
	q_handles = (u16 *)malloc(q_handles_size, M_ICE, M_NOWAIT|M_ZERO);
	if (!q_handles) {
		err = (ENOMEM);
		goto free_q_ids;
	}

	ice_for_each_traffic_class(tc) {
		struct ice_tc_info *tc_info = &vsi->tc_info[tc];
		u16 start_idx, end_idx;

		/* Skip rest of disabled TCs once the first
		 * disabled TC is found */
		if (!(vsi->tc_map & BIT(tc)))
			break;

		/* Fill out TX queue information for this TC */
		start_idx = tc_info->qoffset;
		end_idx = start_idx + tc_info->qcount_tx;
		buf_idx = 0;
		for (j = start_idx; j < end_idx; j++) {
			struct ice_tx_queue *txq = &vsi->tx_queues[j];

			q_ids[buf_idx] = vsi->tx_qmap[j];
			q_handles[buf_idx] = txq->q_handle;
			q_teids[buf_idx] = txq->q_teid;
			buf_idx++;
		}

		status = ice_dis_vsi_txq(hw->port_info, vsi->idx, tc, buf_idx,
					 q_handles, q_ids, q_teids, ICE_NO_RESET, 0, NULL);
		if (status == ICE_ERR_DOES_NOT_EXIST) {
			; /* Queues have already been disabled, no need to report this as an error */
		} else if (status == ICE_ERR_RESET_ONGOING) {
			device_printf(sc->dev,
				      "Reset in progress. LAN Tx queues already disabled\n");
			break;
		} else if (status) {
			device_printf(sc->dev,
				      "Failed to disable LAN Tx queues: err %s aq_err %s\n",
				      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
			err = (ENODEV);
			break;
		}

		/* Clear buffers */
		memset(q_teids, 0, q_teids_size);
		memset(q_ids, 0, q_ids_size);
		memset(q_handles, 0, q_handles_size);
	}

/* free_q_handles: */
	free(q_handles, M_ICE);
free_q_ids:
	free(q_ids, M_ICE);
free_q_teids:
	free(q_teids, M_ICE);

	return err;
}

/**
 * ice_vsi_set_rss_params - Set the RSS parameters for the VSI
 * @vsi: the VSI to configure
 *
 * Sets the RSS table size and lookup table type for the VSI based on its
 * VSI type.
 */
static void
ice_vsi_set_rss_params(struct ice_vsi *vsi)
{
	struct ice_softc *sc = vsi->sc;
	struct ice_hw_common_caps *cap;

	cap = &sc->hw.func_caps.common_cap;

	switch (vsi->type) {
	case ICE_VSI_PF:
		/* The PF VSI inherits RSS instance of the PF */
		vsi->rss_table_size = cap->rss_table_size;
		vsi->rss_lut_type = ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_PF;
		break;
	case ICE_VSI_VF:
		vsi->rss_table_size = ICE_VSIQF_HLUT_ARRAY_SIZE;
		vsi->rss_lut_type = ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_VSI;
		break;
	default:
		device_printf(sc->dev,
			      "VSI %d: RSS not supported for VSI type %d\n",
			      vsi->idx, vsi->type);
		break;
	}
}

/**
 * ice_vsi_add_txqs_ctx - Create a sysctl context and node to store txq sysctls
 * @vsi: The VSI to add the context for
 *
 * Creates a sysctl context for storing txq sysctls. Additionally creates
 * a node rooted at the given VSI's main sysctl node. This context will be
 * used to store per-txq sysctls which may need to be released during the
 * driver's lifetime.
 */
void
ice_vsi_add_txqs_ctx(struct ice_vsi *vsi)
{
	struct sysctl_oid_list *vsi_list;

	sysctl_ctx_init(&vsi->txqs_ctx);

	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	vsi->txqs_node = SYSCTL_ADD_NODE(&vsi->txqs_ctx, vsi_list, OID_AUTO, "txqs",
					 CTLFLAG_RD, NULL, "Tx Queues");
}

/**
 * ice_vsi_add_rxqs_ctx - Create a sysctl context and node to store rxq sysctls
 * @vsi: The VSI to add the context for
 *
 * Creates a sysctl context for storing rxq sysctls. Additionally creates
 * a node rooted at the given VSI's main sysctl node. This context will be
 * used to store per-rxq sysctls which may need to be released during the
 * driver's lifetime.
 */
void
ice_vsi_add_rxqs_ctx(struct ice_vsi *vsi)
{
	struct sysctl_oid_list *vsi_list;

	sysctl_ctx_init(&vsi->rxqs_ctx);

	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	vsi->rxqs_node = SYSCTL_ADD_NODE(&vsi->rxqs_ctx, vsi_list, OID_AUTO, "rxqs",
					 CTLFLAG_RD, NULL, "Rx Queues");
}

/**
 * ice_vsi_del_txqs_ctx - Delete the Tx queue sysctl context for this VSI
 * @vsi: The VSI to delete from
 *
 * Frees the txq sysctl context created for storing the per-queue Tx sysctls.
 * Must be called prior to freeing the Tx queue memory, in order to avoid
 * having sysctls point at stale memory.
 */
void
ice_vsi_del_txqs_ctx(struct ice_vsi *vsi)
{
	device_t dev = vsi->sc->dev;
	int err;

	if (vsi->txqs_node) {
		err = sysctl_ctx_free(&vsi->txqs_ctx);
		if (err)
			device_printf(dev, "failed to free VSI %d txqs_ctx, err %s\n",
				      vsi->idx, ice_err_str(err));
		vsi->txqs_node = NULL;
	}
}

/**
 * ice_vsi_del_rxqs_ctx - Delete the Rx queue sysctl context for this VSI
 * @vsi: The VSI to delete from
 *
 * Frees the rxq sysctl context created for storing the per-queue Rx sysctls.
 * Must be called prior to freeing the Rx queue memory, in order to avoid
 * having sysctls point at stale memory.
 */
void
ice_vsi_del_rxqs_ctx(struct ice_vsi *vsi)
{
	device_t dev = vsi->sc->dev;
	int err;

	if (vsi->rxqs_node) {
		err = sysctl_ctx_free(&vsi->rxqs_ctx);
		if (err)
			device_printf(dev, "failed to free VSI %d rxqs_ctx, err %s\n",
				      vsi->idx, ice_err_str(err));
		vsi->rxqs_node = NULL;
	}
}

/**
 * ice_add_txq_sysctls - Add per-queue sysctls for a Tx queue
 * @txq: pointer to the Tx queue
 *
* Add per-queue sysctls for a given Tx queue. Can't be called during
* ice_add_vsi_sysctls, since the queue memory has not yet been setup.
 */
void
ice_add_txq_sysctls(struct ice_tx_queue *txq)
{
	struct ice_vsi *vsi = txq->vsi;
	struct sysctl_ctx_list *ctx = &vsi->txqs_ctx;
	struct sysctl_oid_list *txqs_list, *this_txq_list;
	struct sysctl_oid *txq_node;
	char txq_name[32], txq_desc[32];

	const struct ice_sysctl_info ctls[] = {
		{ &txq->stats.tx_packets, "tx_packets", "Queue Packets Transmitted" },
		{ &txq->stats.tx_bytes, "tx_bytes", "Queue Bytes Transmitted" },
		{ &txq->stats.mss_too_small, "mss_too_small", "TSO sends with an MSS less than 64" },
		{ 0, 0, 0 }
	};

	const struct ice_sysctl_info *entry = ctls;

	txqs_list = SYSCTL_CHILDREN(vsi->txqs_node);

	snprintf(txq_name, sizeof(txq_name), "%u", txq->me);
	snprintf(txq_desc, sizeof(txq_desc), "Tx Queue %u", txq->me);
	txq_node = SYSCTL_ADD_NODE(ctx, txqs_list, OID_AUTO, txq_name,
				   CTLFLAG_RD, NULL, txq_desc);
	this_txq_list = SYSCTL_CHILDREN(txq_node);

	/* Add the Tx queue statistics */
	while (entry->stat != 0) {
		SYSCTL_ADD_U64(ctx, this_txq_list, OID_AUTO, entry->name,
			       CTLFLAG_RD | CTLFLAG_STATS, entry->stat, 0,
			       entry->description);
		entry++;
	}

	SYSCTL_ADD_U8(ctx, this_txq_list, OID_AUTO, "tc",
		       CTLFLAG_RD, &txq->tc, 0,
		       "Traffic Class that Queue belongs to");
}

/**
 * ice_add_rxq_sysctls - Add per-queue sysctls for an Rx queue
 * @rxq: pointer to the Rx queue
 *
 * Add per-queue sysctls for a given Rx queue. Can't be called during
 * ice_add_vsi_sysctls, since the queue memory has not yet been setup.
 */
void
ice_add_rxq_sysctls(struct ice_rx_queue *rxq)
{
	struct ice_vsi *vsi = rxq->vsi;
	struct sysctl_ctx_list *ctx = &vsi->rxqs_ctx;
	struct sysctl_oid_list *rxqs_list, *this_rxq_list;
	struct sysctl_oid *rxq_node;
	char rxq_name[32], rxq_desc[32];

	const struct ice_sysctl_info ctls[] = {
		{ &rxq->stats.rx_packets, "rx_packets", "Queue Packets Received" },
		{ &rxq->stats.rx_bytes, "rx_bytes", "Queue Bytes Received" },
		{ &rxq->stats.desc_errs, "rx_desc_errs", "Queue Rx Descriptor Errors" },
		{ 0, 0, 0 }
	};

	const struct ice_sysctl_info *entry = ctls;

	rxqs_list = SYSCTL_CHILDREN(vsi->rxqs_node);

	snprintf(rxq_name, sizeof(rxq_name), "%u", rxq->me);
	snprintf(rxq_desc, sizeof(rxq_desc), "Rx Queue %u", rxq->me);
	rxq_node = SYSCTL_ADD_NODE(ctx, rxqs_list, OID_AUTO, rxq_name,
				   CTLFLAG_RD, NULL, rxq_desc);
	this_rxq_list = SYSCTL_CHILDREN(rxq_node);

	/* Add the Rx queue statistics */
	while (entry->stat != 0) {
		SYSCTL_ADD_U64(ctx, this_rxq_list, OID_AUTO, entry->name,
			       CTLFLAG_RD | CTLFLAG_STATS, entry->stat, 0,
			       entry->description);
		entry++;
	}

	SYSCTL_ADD_U8(ctx, this_rxq_list, OID_AUTO, "tc",
		       CTLFLAG_RD, &rxq->tc, 0,
		       "Traffic Class that Queue belongs to");
}

/**
 * ice_get_default_rss_key - Obtain a default RSS key
 * @seed: storage for the RSS key data
 *
 * Copies a pre-generated RSS key into the seed memory. The seed pointer must
 * point to a block of memory that is at least 40 bytes in size.
 *
 * The key isn't randomly generated each time this function is called because
 * that makes the RSS key change every time we reconfigure RSS. This does mean
 * that we're hard coding a possibly 'well known' key. We might want to
 * investigate randomly generating this key once during the first call.
 */
static void
ice_get_default_rss_key(u8 *seed)
{
	const u8 default_seed[ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE] = {
		0x39, 0xed, 0xff, 0x4d, 0x43, 0x58, 0x42, 0xc3, 0x5f, 0xb8,
		0xa5, 0x32, 0x95, 0x65, 0x81, 0xcd, 0x36, 0x79, 0x71, 0x97,
		0xde, 0xa4, 0x41, 0x40, 0x6f, 0x27, 0xe9, 0x81, 0x13, 0xa0,
		0x95, 0x93, 0x5b, 0x1e, 0x9d, 0x27, 0x9d, 0x24, 0x84, 0xb5,
	};

	bcopy(default_seed, seed, ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE);
}

/**
 * ice_set_rss_key - Configure a given VSI with the default RSS key
 * @vsi: the VSI to configure
 *
 * Program the hardware RSS key. We use rss_getkey to grab the kernel RSS key.
 * If the kernel RSS interface is not available, this will fall back to our
 * pre-generated hash seed from ice_get_default_rss_key().
 */
static int
ice_set_rss_key(struct ice_vsi *vsi)
{
	struct ice_aqc_get_set_rss_keys keydata = { .standard_rss_key = {0} };
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;

	/*
	 * If the RSS kernel interface is disabled, this will return the
	 * default RSS key above.
	 */
	rss_getkey(keydata.standard_rss_key);

	status = ice_aq_set_rss_key(hw, vsi->idx, &keydata);
	if (status) {
		device_printf(sc->dev,
			      "ice_aq_set_rss_key status %s, error %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	return (0);
}

/**
 * ice_set_rss_flow_flds - Program the RSS hash flows after package init
 * @vsi: the VSI to configure
 *
 * If the package file is initialized, the default RSS flows are reset. We
 * need to reprogram the expected hash configuration. We'll use
 * rss_gethashconfig() to determine which flows to enable. If RSS kernel
 * support is not enabled, this macro will fall back to suitable defaults.
 */
static void
ice_set_rss_flow_flds(struct ice_vsi *vsi)
{
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;
	struct ice_rss_hash_cfg rss_cfg = { 0, 0, ICE_RSS_ANY_HEADERS, false };
	device_t dev = sc->dev;
	enum ice_status status;
	u_int rss_hash_config;

	rss_hash_config = rss_gethashconfig();

	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV4) {
		rss_cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV4;
		rss_cfg.hash_flds = ICE_FLOW_HASH_IPV4;
		status = ice_add_rss_cfg(hw, vsi->idx, &rss_cfg);
		if (status)
			device_printf(dev,
				      "ice_add_rss_cfg on VSI %d failed for ipv4 flow, err %s aq_err %s\n",
				      vsi->idx, ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
	}
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV4) {
		rss_cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_TCP;
		rss_cfg.hash_flds = ICE_HASH_TCP_IPV4;
		status = ice_add_rss_cfg(hw, vsi->idx, &rss_cfg);
		if (status)
			device_printf(dev,
				      "ice_add_rss_cfg on VSI %d failed for tcp4 flow, err %s aq_err %s\n",
				      vsi->idx, ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
	}
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4) {
		rss_cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_UDP;
		rss_cfg.hash_flds = ICE_HASH_UDP_IPV4;
		status = ice_add_rss_cfg(hw, vsi->idx, &rss_cfg);
		if (status)
			device_printf(dev,
				      "ice_add_rss_cfg on VSI %d failed for udp4 flow, err %s aq_err %s\n",
				      vsi->idx, ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
	}
	if (rss_hash_config & (RSS_HASHTYPE_RSS_IPV6 | RSS_HASHTYPE_RSS_IPV6_EX)) {
		rss_cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV6;
		rss_cfg.hash_flds = ICE_FLOW_HASH_IPV6;
		status = ice_add_rss_cfg(hw, vsi->idx, &rss_cfg);
		if (status)
			device_printf(dev,
				      "ice_add_rss_cfg on VSI %d failed for ipv6 flow, err %s aq_err %s\n",
				      vsi->idx, ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
	}
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6) {
		rss_cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_TCP;
		rss_cfg.hash_flds = ICE_HASH_TCP_IPV6;
		status = ice_add_rss_cfg(hw, vsi->idx, &rss_cfg);
		if (status)
			device_printf(dev,
				      "ice_add_rss_cfg on VSI %d failed for tcp6 flow, err %s aq_err %s\n",
				      vsi->idx, ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
	}
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6) {
		rss_cfg.addl_hdrs = ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_UDP;
		rss_cfg.hash_flds = ICE_HASH_UDP_IPV6;
		status = ice_add_rss_cfg(hw, vsi->idx, &rss_cfg);
		if (status)
			device_printf(dev,
				      "ice_add_rss_cfg on VSI %d failed for udp6 flow, err %s aq_err %s\n",
				      vsi->idx, ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
	}

	/* Warn about RSS hash types which are not supported */
	/* coverity[dead_error_condition] */
	if (rss_hash_config & ~ICE_DEFAULT_RSS_HASH_CONFIG) {
		device_printf(dev,
			      "ice_add_rss_cfg on VSI %d could not configure every requested hash type\n",
			      vsi->idx);
	}
}

/**
 * ice_set_rss_lut - Program the RSS lookup table for a VSI
 * @vsi: the VSI to configure
 *
 * Programs the RSS lookup table for a given VSI. We use
 * rss_get_indirection_to_bucket which will use the indirection table provided
 * by the kernel RSS interface when available. If the kernel RSS interface is
 * not available, we will fall back to a simple round-robin fashion queue
 * assignment.
 */
static int
ice_set_rss_lut(struct ice_vsi *vsi)
{
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct ice_aq_get_set_rss_lut_params lut_params;
	enum ice_status status;
	int i, err = 0;
	u8 *lut;

	lut = (u8 *)malloc(vsi->rss_table_size, M_ICE, M_NOWAIT|M_ZERO);
	if (!lut) {
		device_printf(dev, "Failed to allocate RSS lut memory\n");
		return (ENOMEM);
	}

	/* Populate the LUT with max no. of queues. If the RSS kernel
	 * interface is disabled, this will assign the lookup table in
	 * a simple round robin fashion
	 */
	for (i = 0; i < vsi->rss_table_size; i++) {
		/* XXX: this needs to be changed if num_rx_queues ever counts
		 * more than just the RSS queues */
		lut[i] = rss_get_indirection_to_bucket(i) % vsi->num_rx_queues;
	}

	lut_params.vsi_handle = vsi->idx;
	lut_params.lut_size = vsi->rss_table_size;
	lut_params.lut_type = vsi->rss_lut_type;
	lut_params.lut = lut;
	lut_params.global_lut_id = 0;
	status = ice_aq_set_rss_lut(hw, &lut_params);
	if (status) {
		device_printf(dev,
			      "Cannot set RSS lut, err %s aq_err %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		err = (EIO);
	}

	free(lut, M_ICE);
	return err;
}

/**
 * ice_config_rss - Configure RSS for a VSI
 * @vsi: the VSI to configure
 *
 * If FEATURE_RSS is enabled, configures the RSS lookup table and hash key for
 * a given VSI.
 */
int
ice_config_rss(struct ice_vsi *vsi)
{
	int err;

	/* Nothing to do, if RSS is not enabled */
	if (!ice_is_bit_set(vsi->sc->feat_en, ICE_FEATURE_RSS))
		return 0;

	err = ice_set_rss_key(vsi);
	if (err)
		return err;

	ice_set_rss_flow_flds(vsi);

	return ice_set_rss_lut(vsi);
}

/**
 * ice_log_pkg_init - Log a message about status of DDP initialization
 * @sc: the device softc pointer
 * @pkg_status: the status result of ice_copy_and_init_pkg
 *
 * Called by ice_load_pkg after an attempt to download the DDP package
 * contents to the device to log an appropriate message for the system
 * administrator about download status.
 *
 * @post ice_is_init_pkg_successful function is used to determine
 * whether the download was successful and DDP package is compatible
 * with this driver. Otherwise driver will transition to Safe Mode.
 */
void
ice_log_pkg_init(struct ice_softc *sc, enum ice_ddp_state pkg_status)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct sbuf *active_pkg, *os_pkg;

	active_pkg = sbuf_new_auto();
	ice_active_pkg_version_str(hw, active_pkg);
	sbuf_finish(active_pkg);

	os_pkg = sbuf_new_auto();
	ice_os_pkg_version_str(hw, os_pkg);
	sbuf_finish(os_pkg);

	switch (pkg_status) {
	case ICE_DDP_PKG_SUCCESS:
		device_printf(dev,
			      "The DDP package was successfully loaded: %s.\n",
			      sbuf_data(active_pkg));
		break;
	case ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED:
	case ICE_DDP_PKG_ALREADY_LOADED:
		device_printf(dev,
			      "DDP package already present on device: %s.\n",
			      sbuf_data(active_pkg));
		break;
	case ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED:
		device_printf(dev,
			      "The driver could not load the DDP package file because a compatible DDP package is already present on the device.  The device has package %s.  The ice_ddp module has package: %s.\n",
			      sbuf_data(active_pkg),
			      sbuf_data(os_pkg));
		break;
	case ICE_DDP_PKG_FILE_VERSION_TOO_HIGH:
		device_printf(dev,
			      "The device has a DDP package that is higher than the driver supports.  The device has package %s.  The driver requires version %d.%d.x.x.  Entering Safe Mode.\n",
			      sbuf_data(active_pkg),
			      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
		break;
	case ICE_DDP_PKG_FILE_VERSION_TOO_LOW:
		device_printf(dev,
			      "The device has a DDP package that is lower than the driver supports.  The device has package %s.  The driver requires version %d.%d.x.x.  Entering Safe Mode.\n",
			      sbuf_data(active_pkg),
			      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
		break;
	case ICE_DDP_PKG_ALREADY_LOADED_NOT_SUPPORTED:
		/*
		 * This assumes that the active_pkg_ver will not be
		 * initialized if the ice_ddp package version is not
		 * supported.
		 */
		if (pkg_ver_empty(&hw->active_pkg_ver, hw->active_pkg_name)) {
			/* The ice_ddp version is not supported */
			if (pkg_ver_compatible(&hw->pkg_ver) > 0) {
				device_printf(dev,
					      "The DDP package in the ice_ddp module is higher than the driver supports.  The ice_ddp module has package %s.  The driver requires version %d.%d.x.x.  Please use an updated driver.  Entering Safe Mode.\n",
					      sbuf_data(os_pkg),
					      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
			} else if (pkg_ver_compatible(&hw->pkg_ver) < 0) {
				device_printf(dev,
					      "The DDP package in the ice_ddp module is lower than the driver supports.  The ice_ddp module has package %s.  The driver requires version %d.%d.x.x.  Please use an updated ice_ddp module.  Entering Safe Mode.\n",
					      sbuf_data(os_pkg),
					      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
			} else {
				device_printf(dev,
					      "An unknown error occurred when loading the DDP package.  The ice_ddp module has package %s.  The device has package %s.  The driver requires version %d.%d.x.x.  Entering Safe Mode.\n",
					      sbuf_data(os_pkg),
					      sbuf_data(active_pkg),
					      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
			}
		} else {
			if (pkg_ver_compatible(&hw->active_pkg_ver) > 0) {
				device_printf(dev,
					      "The device has a DDP package that is higher than the driver supports.  The device has package %s.  The driver requires version %d.%d.x.x.  Entering Safe Mode.\n",
					      sbuf_data(active_pkg),
					      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
			} else if (pkg_ver_compatible(&hw->active_pkg_ver) < 0) {
				device_printf(dev,
					      "The device has a DDP package that is lower than the driver supports.  The device has package %s.  The driver requires version %d.%d.x.x.  Entering Safe Mode.\n",
					      sbuf_data(active_pkg),
					      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
			} else {
				device_printf(dev,
					      "An unknown error occurred when loading the DDP package.  The ice_ddp module has package %s.  The device has package %s.  The driver requires version %d.%d.x.x.  Entering Safe Mode.\n",
					      sbuf_data(os_pkg),
					      sbuf_data(active_pkg),
					      ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
			}
		}
		break;
	case ICE_DDP_PKG_INVALID_FILE:
		device_printf(dev,
			      "The DDP package in the ice_ddp module is invalid.  Entering Safe Mode\n");
		break;
	case ICE_DDP_PKG_FW_MISMATCH:
		device_printf(dev,
			      "The firmware loaded on the device is not compatible with the DDP package.  Please update the device's NVM.  Entering safe mode.\n");
		break;
	case ICE_DDP_PKG_NO_SEC_MANIFEST:
	case ICE_DDP_PKG_FILE_SIGNATURE_INVALID:
		device_printf(dev,
			      "The DDP package in the ice_ddp module cannot be loaded because its signature is not valid.  Please use a valid ice_ddp module.  Entering Safe Mode.\n");
		break;
	case ICE_DDP_PKG_SECURE_VERSION_NBR_TOO_LOW:
		device_printf(dev,
			      "The DDP package in the ice_ddp module could not be loaded because its security revision is too low.  Please use an updated ice_ddp module.  Entering Safe Mode.\n");
		break;
	case ICE_DDP_PKG_MANIFEST_INVALID:
	case ICE_DDP_PKG_BUFFER_INVALID:
		device_printf(dev,
			      "An error occurred on the device while loading the DDP package.  Entering Safe Mode.\n");
		break;
	default:
		device_printf(dev,
			 "An unknown error occurred when loading the DDP package.  Entering Safe Mode.\n");
		break;
	}

	sbuf_delete(active_pkg);
	sbuf_delete(os_pkg);
}

/**
 * ice_load_pkg_file - Load the DDP package file using firmware_get
 * @sc: device private softc
 *
 * Use firmware_get to load the DDP package memory and then request that
 * firmware download the package contents and program the relevant hardware
 * bits.
 *
 * This function makes a copy of the DDP package memory which is tracked in
 * the ice_hw structure. The copy will be managed and released by
 * ice_deinit_hw(). This allows the firmware reference to be immediately
 * released using firmware_put.
 */
enum ice_status
ice_load_pkg_file(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_ddp_state state;
	const struct firmware *pkg;
	enum ice_status status = ICE_SUCCESS;
	u8 cached_layer_count;
	u8 *buf_copy;

	pkg = firmware_get("ice_ddp");
	if (!pkg) {
		device_printf(dev,
		    "The DDP package module (ice_ddp) failed to load or could not be found. Entering Safe Mode.\n");
		if (cold)
			device_printf(dev,
			    "The DDP package module cannot be automatically loaded while booting. You may want to specify ice_ddp_load=\"YES\" in your loader.conf\n");
		status = ICE_ERR_CFG;
		goto err_load_pkg;
	}

	/* Check for topology change */
	if (ice_is_bit_set(sc->feat_cap, ICE_FEATURE_TX_BALANCE)) {
		cached_layer_count = hw->num_tx_sched_layers;
		buf_copy = (u8 *)malloc(pkg->datasize, M_ICE, M_NOWAIT);
		if (buf_copy == NULL)
			return ICE_ERR_NO_MEMORY;
		memcpy(buf_copy, pkg->data, pkg->datasize);
		status = ice_cfg_tx_topo(&sc->hw, buf_copy, pkg->datasize);
		free(buf_copy, M_ICE);
		/* Success indicates a change was made */
		if (status == ICE_SUCCESS) {
			/* 9 -> 5 */
			if (cached_layer_count == 9)
				device_printf(dev,
				    "Transmit balancing feature enabled\n");
			else
				device_printf(dev,
				    "Transmit balancing feature disabled\n");
			ice_set_bit(ICE_FEATURE_TX_BALANCE, sc->feat_en);
			return (status);
		}
	}

	/* Copy and download the pkg contents */
	state = ice_copy_and_init_pkg(hw, (const u8 *)pkg->data, pkg->datasize);

	/* Release the firmware reference */
	firmware_put(pkg, FIRMWARE_UNLOAD);

	/* Check the active DDP package version and log a message */
	ice_log_pkg_init(sc, state);

	/* Place the driver into safe mode */
	if (ice_is_init_pkg_successful(state))
		return (ICE_ERR_ALREADY_EXISTS);

err_load_pkg:
	ice_zero_bitmap(sc->feat_cap, ICE_FEATURE_COUNT);
	ice_zero_bitmap(sc->feat_en, ICE_FEATURE_COUNT);
	ice_set_bit(ICE_FEATURE_SAFE_MODE, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_SAFE_MODE, sc->feat_en);

	return (status);
}

/**
 * ice_get_ifnet_counter - Retrieve counter value for a given ifnet counter
 * @vsi: the vsi to retrieve the value for
 * @counter: the counter type to retrieve
 *
 * Returns the value for a given ifnet counter. To do so, we calculate the
 * value based on the matching hardware statistics.
 */
uint64_t
ice_get_ifnet_counter(struct ice_vsi *vsi, ift_counter counter)
{
	struct ice_hw_port_stats *hs = &vsi->sc->stats.cur;
	struct ice_eth_stats *es = &vsi->hw_stats.cur;

	/* For some statistics, especially those related to error flows, we do
	 * not have per-VSI counters. In this case, we just report the global
	 * counters.
	 */

	switch (counter) {
	case IFCOUNTER_IPACKETS:
		return (es->rx_unicast + es->rx_multicast + es->rx_broadcast);
	case IFCOUNTER_IERRORS:
		return (hs->crc_errors + hs->illegal_bytes +
			hs->mac_local_faults + hs->mac_remote_faults +
			hs->rx_len_errors + hs->rx_undersize +
			hs->rx_oversize + hs->rx_fragments + hs->rx_jabber);
	case IFCOUNTER_OPACKETS:
		return (es->tx_unicast + es->tx_multicast + es->tx_broadcast);
	case IFCOUNTER_OERRORS:
		return (es->tx_errors);
	case IFCOUNTER_COLLISIONS:
		return (0);
	case IFCOUNTER_IBYTES:
		return (es->rx_bytes);
	case IFCOUNTER_OBYTES:
		return (es->tx_bytes);
	case IFCOUNTER_IMCASTS:
		return (es->rx_multicast);
	case IFCOUNTER_OMCASTS:
		return (es->tx_multicast);
	case IFCOUNTER_IQDROPS:
		return (es->rx_discards);
	case IFCOUNTER_OQDROPS:
		return (hs->tx_dropped_link_down);
	case IFCOUNTER_NOPROTO:
		return (es->rx_unknown_protocol);
	default:
		return if_get_counter_default(vsi->sc->ifp, counter);
	}
}

/**
 * ice_save_pci_info - Save PCI configuration fields in HW struct
 * @hw: the ice_hw struct to save the PCI information in
 * @dev: the device to get the PCI information from
 *
 * This should only be called once, early in the device attach
 * process.
 */
void
ice_save_pci_info(struct ice_hw *hw, device_t dev)
{
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->subsystem_vendor_id = pci_get_subvendor(dev);
	hw->subsystem_device_id = pci_get_subdevice(dev);
	hw->revision_id = pci_get_revid(dev);
	hw->bus.device = pci_get_slot(dev);
	hw->bus.func = pci_get_function(dev);
}

/**
 * ice_replay_all_vsi_cfg - Replace configuration for all VSIs after reset
 * @sc: the device softc
 *
 * Replace the configuration for each VSI, and then cleanup replay
 * information. Called after a hardware reset in order to reconfigure the
 * active VSIs.
 */
int
ice_replay_all_vsi_cfg(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;
	int i;

	for (i = 0 ; i < sc->num_available_vsi; i++) {
		struct ice_vsi *vsi = sc->all_vsi[i];

		if (!vsi)
			continue;

		status = ice_replay_vsi(hw, vsi->idx);
		if (status) {
			device_printf(sc->dev, "Failed to replay VSI %d, err %s aq_err %s\n",
				      vsi->idx, ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
			return (EIO);
		}
	}

	/* Cleanup replay filters after successful reconfiguration */
	ice_replay_post(hw);
	return (0);
}

/**
 * ice_clean_vsi_rss_cfg - Cleanup RSS configuration for a given VSI
 * @vsi: pointer to the VSI structure
 *
 * Cleanup the advanced RSS configuration for a given VSI. This is necessary
 * during driver removal to ensure that all RSS resources are properly
 * released.
 *
 * @remark this function doesn't report an error as it is expected to be
 * called during driver reset and unload, and there isn't much the driver can
 * do if freeing RSS resources fails.
 */
static void
ice_clean_vsi_rss_cfg(struct ice_vsi *vsi)
{
	struct ice_softc *sc = vsi->sc;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;

	status = ice_rem_vsi_rss_cfg(hw, vsi->idx);
	if (status)
		device_printf(dev,
			      "Failed to remove RSS configuration for VSI %d, err %s\n",
			      vsi->idx, ice_status_str(status));

	/* Remove this VSI from the RSS list */
	ice_rem_vsi_rss_list(hw, vsi->idx);
}

/**
 * ice_clean_all_vsi_rss_cfg - Cleanup RSS configuration for all VSIs
 * @sc: the device softc pointer
 *
 * Cleanup the advanced RSS configuration for all VSIs on a given PF
 * interface.
 *
 * @remark This should be called while preparing for a reset, to cleanup stale
 * RSS configuration for all VSIs.
 */
void
ice_clean_all_vsi_rss_cfg(struct ice_softc *sc)
{
	int i;

	/* No need to cleanup if RSS is not enabled */
	if (!ice_is_bit_set(sc->feat_en, ICE_FEATURE_RSS))
		return;

	for (i = 0; i < sc->num_available_vsi; i++) {
		struct ice_vsi *vsi = sc->all_vsi[i];

		if (vsi)
			ice_clean_vsi_rss_cfg(vsi);
	}
}

/**
 * ice_requested_fec_mode - Return the requested FEC mode as a string
 * @pi: The port info structure
 *
 * Return a string representing the requested FEC mode.
 */
static const char *
ice_requested_fec_mode(struct ice_port_info *pi)
{
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	enum ice_status status;

	status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG,
				     &pcaps, NULL);
	if (status)
		/* Just report unknown if we can't get capabilities */
		return "Unknown";

	/* Check if RS-FEC has been requested first */
	if (pcaps.link_fec_options & (ICE_AQC_PHY_FEC_25G_RS_528_REQ |
				      ICE_AQC_PHY_FEC_25G_RS_544_REQ))
		return ice_fec_str(ICE_FEC_RS);

	/* If RS FEC has not been requested, then check BASE-R */
	if (pcaps.link_fec_options & (ICE_AQC_PHY_FEC_10G_KR_40G_KR4_REQ |
				      ICE_AQC_PHY_FEC_25G_KR_REQ))
		return ice_fec_str(ICE_FEC_BASER);

	return ice_fec_str(ICE_FEC_NONE);
}

/**
 * ice_negotiated_fec_mode - Return the negotiated FEC mode as a string
 * @pi: The port info structure
 *
 * Return a string representing the current FEC mode.
 */
static const char *
ice_negotiated_fec_mode(struct ice_port_info *pi)
{
	/* First, check if RS has been requested first */
	if (pi->phy.link_info.fec_info & (ICE_AQ_LINK_25G_RS_528_FEC_EN |
					  ICE_AQ_LINK_25G_RS_544_FEC_EN))
		return ice_fec_str(ICE_FEC_RS);

	/* If RS FEC has not been requested, then check BASE-R */
	if (pi->phy.link_info.fec_info & ICE_AQ_LINK_25G_KR_FEC_EN)
		return ice_fec_str(ICE_FEC_BASER);

	return ice_fec_str(ICE_FEC_NONE);
}

/**
 * ice_autoneg_mode - Return string indicating of autoneg completed
 * @pi: The port info structure
 *
 * Return "True" if autonegotiation is completed, "False" otherwise.
 */
static const char *
ice_autoneg_mode(struct ice_port_info *pi)
{
	if (pi->phy.link_info.an_info & ICE_AQ_AN_COMPLETED)
		return "True";
	else
		return "False";
}

/**
 * ice_flowcontrol_mode - Return string indicating the Flow Control mode
 * @pi: The port info structure
 *
 * Returns the current Flow Control mode as a string.
 */
static const char *
ice_flowcontrol_mode(struct ice_port_info *pi)
{
	return ice_fc_str(pi->fc.current_mode);
}

/**
 * ice_link_up_msg - Log a link up message with associated info
 * @sc: the device private softc
 *
 * Log a link up message with LOG_NOTICE message level. Include information
 * about the duplex, FEC mode, autonegotiation and flow control.
 */
void
ice_link_up_msg(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	struct ifnet *ifp = sc->ifp;
	const char *speed, *req_fec, *neg_fec, *autoneg, *flowcontrol;

	speed = ice_aq_speed_to_str(hw->port_info);
	req_fec = ice_requested_fec_mode(hw->port_info);
	neg_fec = ice_negotiated_fec_mode(hw->port_info);
	autoneg = ice_autoneg_mode(hw->port_info);
	flowcontrol = ice_flowcontrol_mode(hw->port_info);

	log(LOG_NOTICE, "%s: Link is up, %s Full Duplex, Requested FEC: %s, Negotiated FEC: %s, Autoneg: %s, Flow Control: %s\n",
	    if_name(ifp), speed, req_fec, neg_fec, autoneg, flowcontrol);
}

/**
 * ice_update_laa_mac - Update MAC address if Locally Administered
 * @sc: the device softc
 *
 * Update the device MAC address when a Locally Administered Address is
 * assigned.
 *
 * This function does *not* update the MAC filter list itself. Instead, it
 * should be called after ice_rm_pf_default_mac_filters, so that the previous
 * address filter will be removed, and before ice_cfg_pf_default_mac_filters,
 * so that the new address filter will be assigned.
 */
int
ice_update_laa_mac(struct ice_softc *sc)
{
	const u8 *lladdr = (const u8 *)if_getlladdr(sc->ifp);
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;

	/* If the address is the same, then there is nothing to update */
	if (!memcmp(lladdr, hw->port_info->mac.lan_addr, ETHER_ADDR_LEN))
		return (0);

	/* Reject Multicast addresses */
	if (ETHER_IS_MULTICAST(lladdr))
		return (EINVAL);

	status = ice_aq_manage_mac_write(hw, lladdr, ICE_AQC_MAN_MAC_UPDATE_LAA_WOL, NULL);
	if (status) {
		device_printf(sc->dev, "Failed to write mac %6D to firmware, err %s aq_err %s\n",
			      lladdr, ":", ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		return (EFAULT);
	}

	/* Copy the address into place of the LAN address. */
	bcopy(lladdr, hw->port_info->mac.lan_addr, ETHER_ADDR_LEN);

	return (0);
}

/**
 * ice_get_and_print_bus_info - Save (PCI) bus info and print messages
 * @sc: device softc
 *
 * This will potentially print out a warning message if bus bandwidth
 * is insufficient for full-speed operation.
 *
 * This should only be called once, during the attach process, after
 * hw->port_info has been filled out with port link topology information
 * (from the Get PHY Capabilities Admin Queue command).
 */
void
ice_get_and_print_bus_info(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u16 pci_link_status;
	int offset;

	pci_find_cap(dev, PCIY_EXPRESS, &offset);
	pci_link_status = pci_read_config(dev, offset + PCIER_LINK_STA, 2);

	/* Fill out hw struct with PCIE link status info */
	ice_set_pci_link_status_data(hw, pci_link_status);

	/* Use info to print out bandwidth messages */
	ice_print_bus_link_data(dev, hw);

	if (ice_pcie_bandwidth_check(sc)) {
		device_printf(dev,
		    "PCI-Express bandwidth available for this device may be insufficient for optimal performance.\n");
		device_printf(dev,
		    "Please move the device to a different PCI-e link with more lanes and/or higher transfer rate.\n");
	}
}

/**
 * ice_pcie_bus_speed_to_rate - Convert driver bus speed enum value to
 * a 64-bit baudrate.
 * @speed: enum value to convert
 *
 * This only goes up to PCIE Gen 4.
 */
static uint64_t
ice_pcie_bus_speed_to_rate(enum ice_pcie_bus_speed speed)
{
	/* If the PCI-E speed is Gen1 or Gen2, then report
	 * only 80% of bus speed to account for encoding overhead.
	 */
	switch (speed) {
	case ice_pcie_speed_2_5GT:
		return IF_Gbps(2);
	case ice_pcie_speed_5_0GT:
		return IF_Gbps(4);
	case ice_pcie_speed_8_0GT:
		return IF_Gbps(8);
	case ice_pcie_speed_16_0GT:
		return IF_Gbps(16);
	case ice_pcie_speed_unknown:
	default:
		return 0;
	}
}

/**
 * ice_pcie_lnk_width_to_int - Convert driver pci-e width enum value to
 * a 32-bit number.
 * @width: enum value to convert
 */
static int
ice_pcie_lnk_width_to_int(enum ice_pcie_link_width width)
{
	switch (width) {
	case ice_pcie_lnk_x1:
		return (1);
	case ice_pcie_lnk_x2:
		return (2);
	case ice_pcie_lnk_x4:
		return (4);
	case ice_pcie_lnk_x8:
		return (8);
	case ice_pcie_lnk_x12:
		return (12);
	case ice_pcie_lnk_x16:
		return (16);
	case ice_pcie_lnk_x32:
		return (32);
	case ice_pcie_lnk_width_resrv:
	case ice_pcie_lnk_width_unknown:
	default:
		return (0);
	}
}

/**
 * ice_pcie_bandwidth_check - Check if PCI-E bandwidth is sufficient for
 * full-speed device operation.
 * @sc: adapter softc
 *
 * Returns 0 if sufficient; 1 if not.
 */
static uint8_t
ice_pcie_bandwidth_check(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	int num_ports, pcie_width;
	u64 pcie_speed, port_speed;

	MPASS(hw->port_info);

	num_ports = bitcount32(hw->func_caps.common_cap.valid_functions);
	port_speed = ice_phy_types_to_max_rate(hw->port_info);
	pcie_speed = ice_pcie_bus_speed_to_rate(hw->bus.speed);
	pcie_width = ice_pcie_lnk_width_to_int(hw->bus.width);

	/*
	 * If 2x100, clamp ports to 1 -- 2nd port is intended for
	 * failover.
	 */
	if (port_speed == IF_Gbps(100))
		num_ports = 1;

	return !!((num_ports * port_speed) > pcie_speed * pcie_width);
}

/**
 * ice_print_bus_link_data - Print PCI-E bandwidth information
 * @dev: device to print string for
 * @hw: hw struct with PCI-e link information
 */
static void
ice_print_bus_link_data(device_t dev, struct ice_hw *hw)
{
        device_printf(dev, "PCI Express Bus: Speed %s %s\n",
            ((hw->bus.speed == ice_pcie_speed_16_0GT) ? "16.0GT/s" :
            (hw->bus.speed == ice_pcie_speed_8_0GT) ? "8.0GT/s" :
            (hw->bus.speed == ice_pcie_speed_5_0GT) ? "5.0GT/s" :
            (hw->bus.speed == ice_pcie_speed_2_5GT) ? "2.5GT/s" : "Unknown"),
            (hw->bus.width == ice_pcie_lnk_x32) ? "Width x32" :
            (hw->bus.width == ice_pcie_lnk_x16) ? "Width x16" :
            (hw->bus.width == ice_pcie_lnk_x12) ? "Width x12" :
            (hw->bus.width == ice_pcie_lnk_x8) ? "Width x8" :
            (hw->bus.width == ice_pcie_lnk_x4) ? "Width x4" :
            (hw->bus.width == ice_pcie_lnk_x2) ? "Width x2" :
            (hw->bus.width == ice_pcie_lnk_x1) ? "Width x1" : "Width Unknown");
}

/**
 * ice_set_pci_link_status_data - store PCI bus info
 * @hw: pointer to hardware structure
 * @link_status: the link status word from PCI config space
 *
 * Stores the PCI bus info (speed, width, type) within the ice_hw structure
 **/
static void
ice_set_pci_link_status_data(struct ice_hw *hw, u16 link_status)
{
	u16 reg;

	hw->bus.type = ice_bus_pci_express;

	reg = (link_status & PCIEM_LINK_STA_WIDTH) >> 4;

	switch (reg) {
	case ice_pcie_lnk_x1:
	case ice_pcie_lnk_x2:
	case ice_pcie_lnk_x4:
	case ice_pcie_lnk_x8:
	case ice_pcie_lnk_x12:
	case ice_pcie_lnk_x16:
	case ice_pcie_lnk_x32:
		hw->bus.width = (enum ice_pcie_link_width)reg;
		break;
	default:
		hw->bus.width = ice_pcie_lnk_width_unknown;
		break;
	}

	reg = (link_status & PCIEM_LINK_STA_SPEED) + 0x13;

	switch (reg) {
	case ice_pcie_speed_2_5GT:
	case ice_pcie_speed_5_0GT:
	case ice_pcie_speed_8_0GT:
	case ice_pcie_speed_16_0GT:
		hw->bus.speed = (enum ice_pcie_bus_speed)reg;
		break;
	default:
		hw->bus.speed = ice_pcie_speed_unknown;
		break;
	}
}

/**
 * ice_init_link_events - Initialize Link Status Events mask
 * @sc: the device softc
 *
 * Initialize the Link Status Events mask to disable notification of link
 * events we don't care about in software. Also request that link status
 * events be enabled.
 */
int
ice_init_link_events(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	enum ice_status status;
	u16 wanted_events;

	/* Set the bits for the events that we want to be notified by */
	wanted_events = (ICE_AQ_LINK_EVENT_UPDOWN |
			 ICE_AQ_LINK_EVENT_MEDIA_NA |
			 ICE_AQ_LINK_EVENT_MODULE_QUAL_FAIL);

	/* request that every event except the wanted events be masked */
	status = ice_aq_set_event_mask(hw, hw->port_info->lport, ~wanted_events, NULL);
	if (status) {
		device_printf(sc->dev,
			      "Failed to set link status event mask, err %s aq_err %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	/* Request link info with the LSE bit set to enable link status events */
	status = ice_aq_get_link_info(hw->port_info, true, NULL, NULL);
	if (status) {
		device_printf(sc->dev,
			      "Failed to enable link status events, err %s aq_err %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	return (0);
}

/**
 * ice_handle_mdd_event - Handle possibly malicious events
 * @sc: the device softc
 *
 * Called by the admin task if an MDD detection interrupt is triggered.
 * Identifies possibly malicious events coming from VFs. Also triggers for
 * similar incorrect behavior from the PF as well.
 */
void
ice_handle_mdd_event(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	bool mdd_detected = false, request_reinit = false;
	device_t dev = sc->dev;
	u32 reg;

	if (!ice_testandclear_state(&sc->state, ICE_STATE_MDD_PENDING))
		return;

	reg = rd32(hw, GL_MDET_TX_TCLAN);
	if (reg & GL_MDET_TX_TCLAN_VALID_M) {
		u8 pf_num  = (reg & GL_MDET_TX_TCLAN_PF_NUM_M) >> GL_MDET_TX_TCLAN_PF_NUM_S;
		u16 vf_num = (reg & GL_MDET_TX_TCLAN_VF_NUM_M) >> GL_MDET_TX_TCLAN_VF_NUM_S;
		u8 event   = (reg & GL_MDET_TX_TCLAN_MAL_TYPE_M) >> GL_MDET_TX_TCLAN_MAL_TYPE_S;
		u16 queue  = (reg & GL_MDET_TX_TCLAN_QNUM_M) >> GL_MDET_TX_TCLAN_QNUM_S;

		device_printf(dev, "Malicious Driver Detection Tx Descriptor check event '%s' on Tx queue %u PF# %u VF# %u\n",
			      ice_mdd_tx_tclan_str(event), queue, pf_num, vf_num);

		/* Only clear this event if it matches this PF, that way other
		 * PFs can read the event and determine VF and queue number.
		 */
		if (pf_num == hw->pf_id)
			wr32(hw, GL_MDET_TX_TCLAN, 0xffffffff);

		mdd_detected = true;
	}

	/* Determine what triggered the MDD event */
	reg = rd32(hw, GL_MDET_TX_PQM);
	if (reg & GL_MDET_TX_PQM_VALID_M) {
		u8 pf_num  = (reg & GL_MDET_TX_PQM_PF_NUM_M) >> GL_MDET_TX_PQM_PF_NUM_S;
		u16 vf_num = (reg & GL_MDET_TX_PQM_VF_NUM_M) >> GL_MDET_TX_PQM_VF_NUM_S;
		u8 event   = (reg & GL_MDET_TX_PQM_MAL_TYPE_M) >> GL_MDET_TX_PQM_MAL_TYPE_S;
		u16 queue  = (reg & GL_MDET_TX_PQM_QNUM_M) >> GL_MDET_TX_PQM_QNUM_S;

		device_printf(dev, "Malicious Driver Detection Tx Quanta check event '%s' on Tx queue %u PF# %u VF# %u\n",
			      ice_mdd_tx_pqm_str(event), queue, pf_num, vf_num);

		/* Only clear this event if it matches this PF, that way other
		 * PFs can read the event and determine VF and queue number.
		 */
		if (pf_num == hw->pf_id)
			wr32(hw, GL_MDET_TX_PQM, 0xffffffff);

		mdd_detected = true;
	}

	reg = rd32(hw, GL_MDET_RX);
	if (reg & GL_MDET_RX_VALID_M) {
		u8 pf_num  = (reg & GL_MDET_RX_PF_NUM_M) >> GL_MDET_RX_PF_NUM_S;
		u16 vf_num = (reg & GL_MDET_RX_VF_NUM_M) >> GL_MDET_RX_VF_NUM_S;
		u8 event   = (reg & GL_MDET_RX_MAL_TYPE_M) >> GL_MDET_RX_MAL_TYPE_S;
		u16 queue  = (reg & GL_MDET_RX_QNUM_M) >> GL_MDET_RX_QNUM_S;

		device_printf(dev, "Malicious Driver Detection Rx event '%s' on Rx queue %u PF# %u VF# %u\n",
			      ice_mdd_rx_str(event), queue, pf_num, vf_num);

		/* Only clear this event if it matches this PF, that way other
		 * PFs can read the event and determine VF and queue number.
		 */
		if (pf_num == hw->pf_id)
			wr32(hw, GL_MDET_RX, 0xffffffff);

		mdd_detected = true;
	}

	/* Now, confirm that this event actually affects this PF, by checking
	 * the PF registers.
	 */
	if (mdd_detected) {
		reg = rd32(hw, PF_MDET_TX_TCLAN);
		if (reg & PF_MDET_TX_TCLAN_VALID_M) {
			wr32(hw, PF_MDET_TX_TCLAN, 0xffff);
			sc->soft_stats.tx_mdd_count++;
			request_reinit = true;
		}

		reg = rd32(hw, PF_MDET_TX_PQM);
		if (reg & PF_MDET_TX_PQM_VALID_M) {
			wr32(hw, PF_MDET_TX_PQM, 0xffff);
			sc->soft_stats.tx_mdd_count++;
			request_reinit = true;
		}

		reg = rd32(hw, PF_MDET_RX);
		if (reg & PF_MDET_RX_VALID_M) {
			wr32(hw, PF_MDET_RX, 0xffff);
			sc->soft_stats.rx_mdd_count++;
			request_reinit = true;
		}
	}

	/* TODO: Implement logic to detect and handle events caused by VFs. */

	/* request that the upper stack re-initialize the Tx/Rx queues */
	if (request_reinit)
		ice_request_stack_reinit(sc);

	ice_flush(hw);
}

/**
 * ice_start_dcbx_agent - Start DCBX agent in FW via AQ command
 * @sc: the device softc
 *
 * @pre device is DCB capable and the FW LLDP agent has started
 *
 * Checks DCBX status and starts the DCBX agent if it is not in
 * a valid state via an AQ command.
 */
static void
ice_start_dcbx_agent(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	bool dcbx_agent_status;
	enum ice_status status;

	hw->port_info->qos_cfg.dcbx_status = ice_get_dcbx_status(hw);

	if (hw->port_info->qos_cfg.dcbx_status != ICE_DCBX_STATUS_DONE &&
	    hw->port_info->qos_cfg.dcbx_status != ICE_DCBX_STATUS_IN_PROGRESS) {
		/*
		 * Start DCBX agent, but not LLDP. The return value isn't
		 * checked here because a more detailed dcbx agent status is
		 * retrieved and checked in ice_init_dcb() and elsewhere.
		 */
		status = ice_aq_start_stop_dcbx(hw, true, &dcbx_agent_status, NULL);
		if (status && hw->adminq.sq_last_status != ICE_AQ_RC_EPERM)
			device_printf(dev,
			    "start_stop_dcbx failed, err %s aq_err %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
	}
}

/**
 * ice_init_dcb_setup - Initialize DCB settings for HW
 * @sc: the device softc
 *
 * This needs to be called after the fw_lldp_agent sysctl is added, since that
 * can update the device's LLDP agent status if a tunable value is set.
 *
 * Get and store the initial state of DCB settings on driver load. Print out
 * informational messages as well.
 */
void
ice_init_dcb_setup(struct ice_softc *sc)
{
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	u8 pfcmode_ret;

	/* Don't do anything if DCB isn't supported */
	if (!ice_is_bit_set(sc->feat_cap, ICE_FEATURE_DCB)) {
		device_printf(dev, "%s: No DCB support\n", __func__);
		return;
	}

	/* Starts DCBX agent if it needs starting */
	ice_start_dcbx_agent(sc);

	/* This sets hw->port_info->qos_cfg.is_sw_lldp */
	status = ice_init_dcb(hw, true);

	/* If there is an error, then FW LLDP is not in a usable state */
	if (status != 0 && status != ICE_ERR_NOT_READY) {
		/* Don't print an error message if the return code from the AQ
		 * cmd performed in ice_init_dcb() is EPERM; that means the
		 * FW LLDP engine is disabled, and that is a valid state.
		 */
		if (!(status == ICE_ERR_AQ_ERROR &&
		      hw->adminq.sq_last_status == ICE_AQ_RC_EPERM)) {
			device_printf(dev, "DCB init failed, err %s aq_err %s\n",
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
		}
		hw->port_info->qos_cfg.dcbx_status = ICE_DCBX_STATUS_NOT_STARTED;
	}

	switch (hw->port_info->qos_cfg.dcbx_status) {
	case ICE_DCBX_STATUS_DIS:
		ice_debug(hw, ICE_DBG_DCB, "DCBX disabled\n");
		break;
	case ICE_DCBX_STATUS_NOT_STARTED:
		ice_debug(hw, ICE_DBG_DCB, "DCBX not started\n");
		break;
	case ICE_DCBX_STATUS_MULTIPLE_PEERS:
		ice_debug(hw, ICE_DBG_DCB, "DCBX detected multiple peers\n");
		break;
	default:
		break;
	}

	/* LLDP disabled in FW */
	if (hw->port_info->qos_cfg.is_sw_lldp) {
		ice_add_rx_lldp_filter(sc);
		device_printf(dev, "Firmware LLDP agent disabled\n");
	}

	/* Query and cache PFC mode */
	status = ice_aq_query_pfc_mode(hw, &pfcmode_ret, NULL);
	if (status) {
		device_printf(dev, "PFC mode query failed, err %s aq_err %s\n",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
	}
	local_dcbx_cfg = &hw->port_info->qos_cfg.local_dcbx_cfg;
	switch (pfcmode_ret) {
	case ICE_AQC_PFC_VLAN_BASED_PFC:
		local_dcbx_cfg->pfc_mode = ICE_QOS_MODE_VLAN;
		break;
	case ICE_AQC_PFC_DSCP_BASED_PFC:
		local_dcbx_cfg->pfc_mode = ICE_QOS_MODE_DSCP;
		break;
	default:
		/* DCB is disabled, but we shouldn't get here */
		break;
	}

	/* Set default SW MIB for init */
	ice_set_default_local_mib_settings(sc);

	ice_set_bit(ICE_FEATURE_DCB, sc->feat_en);
}

/**
 * ice_dcb_get_tc_map - Scans config to get bitmap of enabled TCs
 * @dcbcfg: DCB configuration to examine
 *
 * Scans a TC mapping table inside dcbcfg to find traffic classes
 * enabled and @returns a bitmask of enabled TCs
 */
u8
ice_dcb_get_tc_map(const struct ice_dcbx_cfg *dcbcfg)
{
	u8 tc_map = 0;
	int i = 0;

	switch (dcbcfg->pfc_mode) {
	case ICE_QOS_MODE_VLAN:
		/* XXX: "i" is actually "User Priority" here, not
		 * Traffic Class, but the max for both is 8, so it works
		 * out here.
		 */
		for (i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++)
			tc_map |= BIT(dcbcfg->etscfg.prio_table[i]);
		break;
	case ICE_QOS_MODE_DSCP:
		for (i = 0; i < ICE_DSCP_NUM_VAL; i++)
			tc_map |= BIT(dcbcfg->dscp_map[i]);
		break;
	default:
		/* Invalid Mode */
		tc_map = ICE_DFLT_TRAFFIC_CLASS;
		break;
	}

	return (tc_map);
}

/**
 * ice_dcb_get_num_tc - Get the number of TCs from DCBX config
 * @dcbcfg: config to retrieve number of TCs from
 *
 * @return number of contiguous TCs found in dcbcfg's ETS Configuration
 * Priority Assignment Table, a value from 1 to 8. If there are
 * non-contiguous TCs used (e.g. assigning 1 and 3 without using 2),
 * then returns 0.
 */
static u8
ice_dcb_get_num_tc(struct ice_dcbx_cfg *dcbcfg)
{
	u8 tc_map;

	tc_map = ice_dcb_get_tc_map(dcbcfg);

	return (ice_dcb_tc_contig(tc_map));
}

/**
 * ice_debug_print_mib_change_event - helper function to log LLDP MIB change events
 * @sc: the device private softc
 * @event: event received on a control queue
 *
 * Prints out the type and contents of an LLDP MIB change event in a DCB debug message.
 */
static void
ice_debug_print_mib_change_event(struct ice_softc *sc, struct ice_rq_event_info *event)
{
	struct ice_aqc_lldp_get_mib *params =
	    (struct ice_aqc_lldp_get_mib *)&event->desc.params.lldp_get_mib;
	u8 mib_type, bridge_type, tx_status;

	static const char* mib_type_strings[] = {
	    "Local MIB",
	    "Remote MIB",
	    "Reserved",
	    "Reserved"
	};
	static const char* bridge_type_strings[] = {
	    "Nearest Bridge",
	    "Non-TPMR Bridge",
	    "Reserved",
	    "Reserved"
	};
	static const char* tx_status_strings[] = {
	    "Port's TX active",
	    "Port's TX suspended and drained",
	    "Reserved",
	    "Port's TX suspended and drained; blocked TC pipe flushed"
	};

	mib_type = (params->type & ICE_AQ_LLDP_MIB_TYPE_M) >>
	    ICE_AQ_LLDP_MIB_TYPE_S;
	bridge_type = (params->type & ICE_AQ_LLDP_BRID_TYPE_M) >>
	    ICE_AQ_LLDP_BRID_TYPE_S;
	tx_status = (params->type & ICE_AQ_LLDP_TX_M) >>
	    ICE_AQ_LLDP_TX_S;

	ice_debug(&sc->hw, ICE_DBG_DCB, "LLDP MIB Change Event (%s, %s, %s)\n",
	    mib_type_strings[mib_type], bridge_type_strings[bridge_type],
	    tx_status_strings[tx_status]);

	/* Nothing else to report */
	if (!event->msg_buf)
		return;

	ice_debug(&sc->hw, ICE_DBG_DCB, "- %s contents:\n", mib_type_strings[mib_type]);
	ice_debug_array(&sc->hw, ICE_DBG_DCB, 16, 1, event->msg_buf,
			event->msg_len);
}

/**
 * ice_dcb_needs_reconfig - Returns true if driver needs to reconfigure
 * @sc: the device private softc
 * @old_cfg: Old DCBX configuration to compare against
 * @new_cfg: New DCBX configuration to check
 *
 * @return true if something changed in new_cfg that requires the driver
 * to do some reconfiguration.
 */
static bool
ice_dcb_needs_reconfig(struct ice_softc *sc, struct ice_dcbx_cfg *old_cfg,
    struct ice_dcbx_cfg *new_cfg)
{
	struct ice_hw *hw = &sc->hw;
	bool needs_reconfig = false;

	/* No change detected in DCBX config */
	if (!memcmp(old_cfg, new_cfg, sizeof(*old_cfg))) {
		ice_debug(hw, ICE_DBG_DCB,
		    "No change detected in local DCBX configuration\n");
		return (false);
	}

	/* Check if ETS config has changed */
	if (memcmp(&new_cfg->etscfg, &old_cfg->etscfg,
		   sizeof(new_cfg->etscfg))) {
		/* If Priority Table has changed, then driver reconfig is needed */
		if (memcmp(&new_cfg->etscfg.prio_table,
			   &old_cfg->etscfg.prio_table,
			   sizeof(new_cfg->etscfg.prio_table))) {
			ice_debug(hw, ICE_DBG_DCB, "ETS UP2TC changed\n");
			needs_reconfig = true;
		}

		/* These are just informational */
		if (memcmp(&new_cfg->etscfg.tcbwtable,
			   &old_cfg->etscfg.tcbwtable,
			   sizeof(new_cfg->etscfg.tcbwtable))) {
			ice_debug(hw, ICE_DBG_DCB, "ETS TCBW table changed\n");
			needs_reconfig = true;
		}

		if (memcmp(&new_cfg->etscfg.tsatable,
			   &old_cfg->etscfg.tsatable,
			   sizeof(new_cfg->etscfg.tsatable))) {
			ice_debug(hw, ICE_DBG_DCB, "ETS TSA table changed\n");
			needs_reconfig = true;
		}
	}

	/* Check if PFC config has changed */
	if (memcmp(&new_cfg->pfc, &old_cfg->pfc, sizeof(new_cfg->pfc))) {
		ice_debug(hw, ICE_DBG_DCB, "PFC config changed\n");
		needs_reconfig = true;
	}

	/* Check if APP table has changed */
	if (memcmp(&new_cfg->app, &old_cfg->app, sizeof(new_cfg->app)))
		ice_debug(hw, ICE_DBG_DCB, "APP Table changed\n");

	ice_debug(hw, ICE_DBG_DCB, "%s result: %d\n", __func__, needs_reconfig);

	return (needs_reconfig);
}

/**
 * ice_stop_pf_vsi - Stop queues for PF LAN VSI
 * @sc: the device private softc
 *
 * Flushes interrupts and stops the queues associated with the PF LAN VSI.
 */
static void
ice_stop_pf_vsi(struct ice_softc *sc)
{
	/* Dissociate the Tx and Rx queues from the interrupts */
	ice_flush_txq_interrupts(&sc->pf_vsi);
	ice_flush_rxq_interrupts(&sc->pf_vsi);

	if (!ice_testandclear_state(&sc->state, ICE_STATE_DRIVER_INITIALIZED))
		return;

	/* Disable the Tx and Rx queues */
	ice_vsi_disable_tx(&sc->pf_vsi);
	ice_control_all_rx_queues(&sc->pf_vsi, false);
}

/**
 * ice_vsi_setup_q_map - Setup a VSI queue map
 * @vsi: the VSI being configured
 * @ctxt: VSI context structure
 */
static void
ice_vsi_setup_q_map(struct ice_vsi *vsi, struct ice_vsi_ctx *ctxt)
{
	u16 qcounts[ICE_MAX_TRAFFIC_CLASS] = {};
	u16 offset = 0, qmap = 0, pow = 0;
	u16 num_q_per_tc, qcount_rx, rem_queues;
	int i, j, k;

	if (vsi->num_tcs == 0) {
		/* at least TC0 should be enabled by default */
		vsi->num_tcs = 1;
		vsi->tc_map = 0x1;
	}

	qcount_rx = vsi->num_rx_queues;
	num_q_per_tc = min(qcount_rx / vsi->num_tcs, ICE_MAX_RXQS_PER_TC);

	if (!num_q_per_tc)
		num_q_per_tc = 1;

	/* Set initial values for # of queues to use for each active TC */
	ice_for_each_traffic_class(i)
		if (i < vsi->num_tcs)
			qcounts[i] = num_q_per_tc;

	/* If any queues are unassigned, add them to TC 0 */
	rem_queues = qcount_rx % vsi->num_tcs;
	if (rem_queues > 0)
		qcounts[0] += rem_queues;

	/* TC mapping is a function of the number of Rx queues assigned to the
	 * VSI for each traffic class and the offset of these queues.
	 * The first 10 bits are for queue offset for TC0, next 4 bits for no:of
	 * queues allocated to TC0. No:of queues is a power-of-2.
	 *
	 * If TC is not enabled, the queue offset is set to 0, and allocate one
	 * queue, this way, traffic for the given TC will be sent to the default
	 * queue.
	 *
	 * Setup number and offset of Rx queues for all TCs for the VSI
	 */
	ice_for_each_traffic_class(i) {
		if (!(vsi->tc_map & BIT(i))) {
			/* TC is not enabled */
			vsi->tc_info[i].qoffset = 0;
			vsi->tc_info[i].qcount_rx = 1;
			vsi->tc_info[i].qcount_tx = 1;

			ctxt->info.tc_mapping[i] = 0;
			continue;
		}

		/* TC is enabled */
		vsi->tc_info[i].qoffset = offset;
		vsi->tc_info[i].qcount_rx = qcounts[i];
		vsi->tc_info[i].qcount_tx = qcounts[i];

		/* find the (rounded up) log-2 of queue count for current TC */
		pow = fls(qcounts[i] - 1);

		qmap = ((offset << ICE_AQ_VSI_TC_Q_OFFSET_S) &
			ICE_AQ_VSI_TC_Q_OFFSET_M) |
			((pow << ICE_AQ_VSI_TC_Q_NUM_S) &
			 ICE_AQ_VSI_TC_Q_NUM_M);
		ctxt->info.tc_mapping[i] = CPU_TO_LE16(qmap);

		/* Store traffic class and handle data in queue structures */
		for (j = offset, k = 0; j < offset + qcounts[i]; j++, k++) {
			vsi->tx_queues[j].q_handle = k;
			vsi->tx_queues[j].tc = i;

			vsi->rx_queues[j].tc = i;
		}
		
		offset += qcounts[i];
	}

	/* Rx queue mapping */
	ctxt->info.mapping_flags |= CPU_TO_LE16(ICE_AQ_VSI_Q_MAP_CONTIG);
	ctxt->info.q_mapping[0] = CPU_TO_LE16(vsi->rx_qmap[0]);
	ctxt->info.q_mapping[1] = CPU_TO_LE16(vsi->num_rx_queues);
}

/**
 * ice_pf_vsi_cfg_tc - Configure PF VSI for a given TC map
 * @sc: the device private softc
 * @tc_map: traffic class bitmap
 *
 * @pre VSI queues are stopped
 *
 * @return 0 if configuration is successful
 * @return EIO if Update VSI AQ cmd fails
 * @return ENODEV if updating Tx Scheduler fails
 */
static int
ice_pf_vsi_cfg_tc(struct ice_softc *sc, u8 tc_map)
{
	u16 max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;
	struct ice_vsi_ctx ctx = { 0 };
	device_t dev = sc->dev;
	enum ice_status status;
	u8 num_tcs = 0;
	int i = 0;

	/* Count the number of enabled Traffic Classes */
	ice_for_each_traffic_class(i)
		if (tc_map & BIT(i))
			num_tcs++;

	vsi->tc_map = tc_map;
	vsi->num_tcs = num_tcs;

	/* Set default parameters for context */
	ctx.vf_num = 0;
	ctx.info = vsi->info;

	/* Setup queue map */
	ice_vsi_setup_q_map(vsi, &ctx);

	/* Update VSI configuration in firmware (RX queues) */
	ctx.info.valid_sections = CPU_TO_LE16(ICE_AQ_VSI_PROP_RXQ_MAP_VALID);
	status = ice_update_vsi(hw, vsi->idx, &ctx, NULL);
	if (status) {
		device_printf(dev,
		    "%s: Update VSI AQ call failed, err %s aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}
	vsi->info = ctx.info;

	/* Use values derived in ice_vsi_setup_q_map() */
	for (i = 0; i < num_tcs; i++)
		max_txqs[i] = vsi->tc_info[i].qcount_tx;

	if (hw->debug_mask & ICE_DBG_DCB) {
		device_printf(dev, "%s: max_txqs:", __func__);
		ice_for_each_traffic_class(i)
			printf(" %d", max_txqs[i]);
		printf("\n");
	}

	/* Update LAN Tx queue info in firmware */
	status = ice_cfg_vsi_lan(hw->port_info, vsi->idx, vsi->tc_map,
				 max_txqs);
	if (status) {
		device_printf(dev,
		    "%s: Failed VSI lan queue config, err %s aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (ENODEV);
	}

	vsi->info.valid_sections = 0;

	return (0);
}

/**
 * ice_dcb_tc_contig - Count TCs if they're contiguous
 * @tc_map: pointer to priority table
 *
 * @return The number of traffic classes in
 * an 8-bit TC bitmap, or if there is a gap, then returns 0.
 */
static u8
ice_dcb_tc_contig(u8 tc_map)
{
	bool tc_unused = false;
	u8 ret = 0;

	/* Scan bitmask for contiguous TCs starting with TC0 */
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		if (tc_map & BIT(i)) {
			if (!tc_unused) {
				ret++;
			} else {
				/* Non-contiguous TCs detected */
				return (0);
			}
		} else
			tc_unused = true;
	}

	return (ret);
}

/**
 * ice_dcb_recfg - Reconfigure VSI with new DCB settings
 * @sc: the device private softc
 *
 * @pre All VSIs have been disabled/stopped
 * 
 * Reconfigures VSI settings based on local_dcbx_cfg.
 */
static void
ice_dcb_recfg(struct ice_softc *sc)
{
	struct ice_dcbx_cfg *dcbcfg =
	    &sc->hw.port_info->qos_cfg.local_dcbx_cfg;
	device_t dev = sc->dev;
	u8 tc_map = 0;
	int ret;

	tc_map = ice_dcb_get_tc_map(dcbcfg);

	/* If non-contiguous TCs are used, then configure
	 * the default TC instead. There's no support for
	 * non-contiguous TCs being used.
	 */
	if (ice_dcb_tc_contig(tc_map) == 0) {
		tc_map = ICE_DFLT_TRAFFIC_CLASS;
		ice_set_default_local_lldp_mib(sc);
	}

	/* Reconfigure VSI queues to add/remove traffic classes */
	ret = ice_pf_vsi_cfg_tc(sc, tc_map);
	if (ret)
		device_printf(dev,
		    "Failed to configure TCs for PF VSI, err %s\n",
		    ice_err_str(ret));

}

/**
 * ice_set_default_local_mib_settings - Set Local LLDP MIB to default settings
 * @sc: device softc structure
 *
 * Overwrites the driver's SW local LLDP MIB with default settings. This
 * ensures the driver has a valid MIB when it next uses the Set Local LLDP MIB
 * admin queue command.
 */
static void
ice_set_default_local_mib_settings(struct ice_softc *sc)
{
	struct ice_dcbx_cfg *dcbcfg;
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi;
	u8 maxtcs, maxtcs_ets, old_pfc_mode;

	pi = hw->port_info;

	dcbcfg = &pi->qos_cfg.local_dcbx_cfg;

	maxtcs = hw->func_caps.common_cap.maxtc;
	/* This value is only 3 bits; 8 TCs maps to 0 */
	maxtcs_ets = maxtcs & ICE_IEEE_ETS_MAXTC_M;

	/* VLAN vs DSCP mode needs to be preserved */
	old_pfc_mode = dcbcfg->pfc_mode;

	/**
	 * Setup the default settings used by the driver for the Set Local
	 * LLDP MIB Admin Queue command (0x0A08). (1TC w/ 100% BW, ETS, no
	 * PFC, TSA=2).
	 */
	memset(dcbcfg, 0, sizeof(*dcbcfg));

	dcbcfg->etscfg.willing = 1;
	dcbcfg->etscfg.tcbwtable[0] = 100;
	dcbcfg->etscfg.maxtcs = maxtcs_ets;
	dcbcfg->etscfg.tsatable[0] = 2;

	dcbcfg->etsrec = dcbcfg->etscfg;
	dcbcfg->etsrec.willing = 0;

	dcbcfg->pfc.willing = 1;
	dcbcfg->pfc.pfccap = maxtcs;

	dcbcfg->pfc_mode = old_pfc_mode;
}

/**
 * ice_do_dcb_reconfig - notify RDMA and reconfigure PF LAN VSI
 * @sc: the device private softc
 * @pending_mib: FW has a pending MIB change to execute
 * 
 * @pre Determined that the DCB configuration requires a change
 *
 * Reconfigures the PF LAN VSI based on updated DCB configuration
 * found in the hw struct's/port_info's/ local dcbx configuration.
 */
static void
ice_do_dcb_reconfig(struct ice_softc *sc, bool pending_mib)
{
	struct ice_aqc_port_ets_elem port_ets = { 0 };
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi;
	device_t dev = sc->dev;
	enum ice_status status;

	pi = sc->hw.port_info;
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;

	ice_rdma_notify_dcb_qos_change(sc);
	/* If there's a pending MIB, tell the FW to execute the MIB change
	 * now.
	 */
	if (pending_mib) {
		status = ice_lldp_execute_pending_mib(hw);
		if ((status == ICE_ERR_AQ_ERROR) &&
		    (hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT)) {
			device_printf(dev,
			    "Execute Pending LLDP MIB AQ call failed, no pending MIB\n");
		} else if (status) {
			device_printf(dev,
			    "Execute Pending LLDP MIB AQ call failed, err %s aq_err %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
			/* This won't break traffic, but QoS will not work as expected */
		}
	}

	/* Set state when there's more than one TC */
	if (ice_dcb_get_num_tc(local_dcbx_cfg) > 1) {
		device_printf(dev, "Multiple traffic classes enabled\n");
		ice_set_state(&sc->state, ICE_STATE_MULTIPLE_TCS);
	} else {
		device_printf(dev, "Multiple traffic classes disabled\n");
		ice_clear_state(&sc->state, ICE_STATE_MULTIPLE_TCS);
	}

	/* Disable PF VSI since it's going to be reconfigured */
	ice_stop_pf_vsi(sc);

	/* Query ETS configuration and update SW Tx scheduler info */
	status = ice_query_port_ets(pi, &port_ets, sizeof(port_ets), NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "Query Port ETS AQ call failed, err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		/* This won't break traffic, but QoS will not work as expected */
	}

	/* Change PF VSI configuration */
	ice_dcb_recfg(sc);

	/* Send new configuration to RDMA client driver */
	ice_rdma_dcb_qos_update(sc, pi);

	ice_request_stack_reinit(sc);
}

/**
 * ice_handle_mib_change_event - helper function to handle LLDP MIB change events
 * @sc: the device private softc
 * @event: event received on a control queue
 *
 * Checks the updated MIB it receives and possibly reconfigures the PF LAN
 * VSI depending on what has changed. This will also print out some debug
 * information about the MIB event if ICE_DBG_DCB is enabled in the debug_mask.
 */
static void
ice_handle_mib_change_event(struct ice_softc *sc, struct ice_rq_event_info *event)
{
	struct ice_aqc_lldp_get_mib *params =
	    (struct ice_aqc_lldp_get_mib *)&event->desc.params.lldp_get_mib;
	struct ice_dcbx_cfg tmp_dcbx_cfg, *local_dcbx_cfg;
	struct ice_port_info *pi;
	device_t dev = sc->dev;
	struct ice_hw *hw = &sc->hw;
	bool needs_reconfig, mib_is_pending;
	enum ice_status status;
	u8 mib_type, bridge_type;

	ASSERT_CFG_LOCKED(sc);

	ice_debug_print_mib_change_event(sc, event);

	pi = sc->hw.port_info;

	mib_type = (params->type & ICE_AQ_LLDP_MIB_TYPE_M) >>
	    ICE_AQ_LLDP_MIB_TYPE_S;
	bridge_type = (params->type & ICE_AQ_LLDP_BRID_TYPE_M) >>
	    ICE_AQ_LLDP_BRID_TYPE_S;
	mib_is_pending = (params->state & ICE_AQ_LLDP_MIB_CHANGE_STATE_M) >>
	    ICE_AQ_LLDP_MIB_CHANGE_STATE_S;

	/* Ignore if event is not for Nearest Bridge */
	if (bridge_type != ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID)
		return;

	/* Check MIB Type and return if event for Remote MIB update */
	if (mib_type == ICE_AQ_LLDP_MIB_REMOTE) {
		/* Update the cached remote MIB and return */
		status = ice_aq_get_dcb_cfg(pi->hw, ICE_AQ_LLDP_MIB_REMOTE,
					 ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID,
					 &pi->qos_cfg.remote_dcbx_cfg);
		if (status)
			device_printf(dev,
			    "%s: Failed to get Remote DCB config; status %s, aq_err %s\n",
			    __func__, ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
		/* Not fatal if this fails */
		return;
	}

	/* Save line length by aliasing the local dcbx cfg */
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;
	/* Save off the old configuration and clear current config */
	tmp_dcbx_cfg = *local_dcbx_cfg;
	memset(local_dcbx_cfg, 0, sizeof(*local_dcbx_cfg));

	/* Update the current local_dcbx_cfg with new data */
	if (mib_is_pending) {
		ice_get_dcb_cfg_from_mib_change(pi, event);
	} else {
		/* Get updated DCBX data from firmware */
		status = ice_get_dcb_cfg(pi);
		if (status) {
			device_printf(dev,
			    "%s: Failed to get Local DCB config; status %s, aq_err %s\n",
			    __func__, ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
			return;
		}
	}

	/* Check to see if DCB needs reconfiguring */
	needs_reconfig = ice_dcb_needs_reconfig(sc, &tmp_dcbx_cfg,
	    local_dcbx_cfg);

	if (!needs_reconfig && !mib_is_pending)
		return;

	/* Reconfigure -- this will also notify FW that configuration is done,
	 * if the FW MIB change is only pending instead of executed.
	 */
	ice_do_dcb_reconfig(sc, mib_is_pending);
}

/**
 * ice_send_version - Send driver version to firmware
 * @sc: the device private softc
 *
 * Send the driver version to the firmware. This must be called as early as
 * possible after ice_init_hw().
 */
int
ice_send_version(struct ice_softc *sc)
{
	struct ice_driver_ver driver_version = {0};
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;

	driver_version.major_ver = ice_major_version;
	driver_version.minor_ver = ice_minor_version;
	driver_version.build_ver = ice_patch_version;
	driver_version.subbuild_ver = ice_rc_version;

	strlcpy((char *)driver_version.driver_string, ice_driver_version,
		sizeof(driver_version.driver_string));

	status = ice_aq_send_driver_ver(hw, &driver_version, NULL);
	if (status) {
		device_printf(dev, "Unable to send driver version to firmware, err %s aq_err %s\n",
			      ice_status_str(status), ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	return (0);
}

/**
 * ice_handle_lan_overflow_event - helper function to log LAN overflow events
 * @sc: device softc
 * @event: event received on a control queue
 *
 * Prints out a message when a LAN overflow event is detected on a receive
 * queue.
 */
static void
ice_handle_lan_overflow_event(struct ice_softc *sc, struct ice_rq_event_info *event)
{
	struct ice_aqc_event_lan_overflow *params =
	    (struct ice_aqc_event_lan_overflow *)&event->desc.params.lan_overflow;
	struct ice_hw *hw = &sc->hw;

	ice_debug(hw, ICE_DBG_DCB, "LAN overflow event detected, prtdcb_ruptq=0x%08x, qtx_ctl=0x%08x\n",
		  LE32_TO_CPU(params->prtdcb_ruptq),
		  LE32_TO_CPU(params->qtx_ctl));
}

/**
 * ice_add_ethertype_to_list - Add an Ethertype filter to a filter list
 * @vsi: the VSI to target packets to
 * @list: the list to add the filter to
 * @ethertype: the Ethertype to filter on
 * @direction: The direction of the filter (Tx or Rx)
 * @action: the action to take
 *
 * Add an Ethertype filter to a filter list. Used to forward a series of
 * filters to the firmware for configuring the switch.
 *
 * Returns 0 on success, and an error code on failure.
 */
static int
ice_add_ethertype_to_list(struct ice_vsi *vsi, struct ice_list_head *list,
			  u16 ethertype, u16 direction,
			  enum ice_sw_fwd_act_type action)
{
	struct ice_fltr_list_entry *entry;

	MPASS((direction == ICE_FLTR_TX) || (direction == ICE_FLTR_RX));

	entry = (__typeof(entry))malloc(sizeof(*entry), M_ICE, M_NOWAIT|M_ZERO);
	if (!entry)
		return (ENOMEM);

	entry->fltr_info.flag = direction;
	entry->fltr_info.src_id = ICE_SRC_ID_VSI;
	entry->fltr_info.lkup_type = ICE_SW_LKUP_ETHERTYPE;
	entry->fltr_info.fltr_act = action;
	entry->fltr_info.vsi_handle = vsi->idx;
	entry->fltr_info.l_data.ethertype_mac.ethertype = ethertype;

	LIST_ADD(&entry->list_entry, list);

	return 0;
}

#define ETHERTYPE_PAUSE_FRAMES 0x8808
#define ETHERTYPE_LLDP_FRAMES 0x88cc

/**
 * ice_cfg_pf_ethertype_filters - Configure switch to drop ethertypes
 * @sc: the device private softc
 *
 * Configure the switch to drop PAUSE frames and LLDP frames transmitted from
 * the host. This prevents malicious VFs from sending these frames and being
 * able to control or configure the network.
 */
int
ice_cfg_pf_ethertype_filters(struct ice_softc *sc)
{
	struct ice_list_head ethertype_list;
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	int err = 0;

	INIT_LIST_HEAD(&ethertype_list);

	/*
	 * Note that the switch filters will ignore the VSI index for the drop
	 * action, so we only need to program drop filters once for the main
	 * VSI.
	 */

	/* Configure switch to drop all Tx pause frames coming from any VSI. */
	if (sc->enable_tx_fc_filter) {
		err = ice_add_ethertype_to_list(vsi, &ethertype_list,
						ETHERTYPE_PAUSE_FRAMES,
						ICE_FLTR_TX, ICE_DROP_PACKET);
		if (err)
			goto free_ethertype_list;
	}

	/* Configure switch to drop LLDP frames coming from any VSI */
	if (sc->enable_tx_lldp_filter) {
		err = ice_add_ethertype_to_list(vsi, &ethertype_list,
						ETHERTYPE_LLDP_FRAMES,
						ICE_FLTR_TX, ICE_DROP_PACKET);
		if (err)
			goto free_ethertype_list;
	}

	status = ice_add_eth_mac(hw, &ethertype_list);
	if (status) {
		device_printf(dev,
			      "Failed to add Tx Ethertype filters, err %s aq_err %s\n",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		err = (EIO);
	}

free_ethertype_list:
	ice_free_fltr_list(&ethertype_list);
	return err;
}

/**
 * ice_add_rx_lldp_filter - add ethertype filter for Rx LLDP frames
 * @sc: the device private structure
 *
 * Add a switch ethertype filter which forwards the LLDP frames to the main PF
 * VSI. Called when the fw_lldp_agent is disabled, to allow the LLDP frames to
 * be forwarded to the stack.
 */
static void
ice_add_rx_lldp_filter(struct ice_softc *sc)
{
	struct ice_list_head ethertype_list;
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	int err;
	u16 vsi_num;

	/*
	 * If FW is new enough, use a direct AQ command to perform the filter
	 * addition.
	 */
	if (ice_fw_supports_lldp_fltr_ctrl(hw)) {
		vsi_num = ice_get_hw_vsi_num(hw, vsi->idx);
		status = ice_lldp_fltr_add_remove(hw, vsi_num, true);
		if (status) {
			device_printf(dev,
			    "Failed to add Rx LLDP filter, err %s aq_err %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
		} else
			ice_set_state(&sc->state,
			    ICE_STATE_LLDP_RX_FLTR_FROM_DRIVER);
		return;
	}

	INIT_LIST_HEAD(&ethertype_list);

	/* Forward Rx LLDP frames to the stack */
	err = ice_add_ethertype_to_list(vsi, &ethertype_list,
					ETHERTYPE_LLDP_FRAMES,
					ICE_FLTR_RX, ICE_FWD_TO_VSI);
	if (err) {
		device_printf(dev,
			      "Failed to add Rx LLDP filter, err %s\n",
			      ice_err_str(err));
		goto free_ethertype_list;
	}

	status = ice_add_eth_mac(hw, &ethertype_list);
	if (status && status != ICE_ERR_ALREADY_EXISTS) {
		device_printf(dev,
			      "Failed to add Rx LLDP filter, err %s aq_err %s\n",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
	} else {
		/*
		 * If status == ICE_ERR_ALREADY_EXISTS, we won't treat an
		 * already existing filter as an error case.
		 */
		ice_set_state(&sc->state, ICE_STATE_LLDP_RX_FLTR_FROM_DRIVER);
	}

free_ethertype_list:
	ice_free_fltr_list(&ethertype_list);
}

/**
 * ice_del_rx_lldp_filter - Remove ethertype filter for Rx LLDP frames
 * @sc: the device private structure
 *
 * Remove the switch filter forwarding LLDP frames to the main PF VSI, called
 * when the firmware LLDP agent is enabled, to stop routing LLDP frames to the
 * stack.
 */
static void
ice_del_rx_lldp_filter(struct ice_softc *sc)
{
	struct ice_list_head ethertype_list;
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	int err;
	u16 vsi_num;

	/*
	 * Only in the scenario where the driver added the filter during
	 * this session (while the driver was loaded) would we be able to
	 * delete this filter.
	 */
	if (!ice_test_state(&sc->state, ICE_STATE_LLDP_RX_FLTR_FROM_DRIVER))
		return;

	/*
	 * If FW is new enough, use a direct AQ command to perform the filter
	 * removal.
	 */
	if (ice_fw_supports_lldp_fltr_ctrl(hw)) {
		vsi_num = ice_get_hw_vsi_num(hw, vsi->idx);
		status = ice_lldp_fltr_add_remove(hw, vsi_num, false);
		if (status) {
			device_printf(dev,
			    "Failed to remove Rx LLDP filter, err %s aq_err %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
		}
		return;
	}

	INIT_LIST_HEAD(&ethertype_list);

	/* Remove filter forwarding Rx LLDP frames to the stack */
	err = ice_add_ethertype_to_list(vsi, &ethertype_list,
					ETHERTYPE_LLDP_FRAMES,
					ICE_FLTR_RX, ICE_FWD_TO_VSI);
	if (err) {
		device_printf(dev,
			      "Failed to remove Rx LLDP filter, err %s\n",
			      ice_err_str(err));
		goto free_ethertype_list;
	}

	status = ice_remove_eth_mac(hw, &ethertype_list);
	if (status == ICE_ERR_DOES_NOT_EXIST) {
		; /* Don't complain if we try to remove a filter that doesn't exist */
	} else if (status) {
		device_printf(dev,
			      "Failed to remove Rx LLDP filter, err %s aq_err %s\n",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
	}

free_ethertype_list:
	ice_free_fltr_list(&ethertype_list);
}

/**
 * ice_init_link_configuration -- Setup link in different ways depending
 * on whether media is available or not.
 * @sc: device private structure
 *
 * Called at the end of the attach process to either set default link
 * parameters if there is media available, or force HW link down and
 * set a state bit if there is no media.
 */
void
ice_init_link_configuration(struct ice_softc *sc)
{
	struct ice_port_info *pi = sc->hw.port_info;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;

	pi->phy.get_link_info = true;
	status = ice_get_link_status(pi, &sc->link_up);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_get_link_status failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return;
	}

	if (pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE) {
		ice_clear_state(&sc->state, ICE_STATE_NO_MEDIA);
		/* Apply default link settings */
		ice_apply_saved_phy_cfg(sc, ICE_APPLY_LS_FEC_FC);
	} else {
		 /* Set link down, and poll for media available in timer. This prevents the
		  * driver from receiving spurious link-related events.
		  */
		ice_set_state(&sc->state, ICE_STATE_NO_MEDIA);
		status = ice_aq_set_link_restart_an(pi, false, NULL);
		if (status != ICE_SUCCESS)
			device_printf(dev,
			    "%s: ice_aq_set_link_restart_an: status %s, aq_err %s\n",
			    __func__, ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
	}
}

/**
 * ice_apply_saved_phy_req_to_cfg -- Write saved user PHY settings to cfg data
 * @sc: device private structure
 * @cfg: new PHY config data to be modified
 *
 * Applies user settings for advertised speeds to the PHY type fields in the
 * supplied PHY config struct. It uses the data from pcaps to check if the
 * saved settings are invalid and uses the pcaps data instead if they are
 * invalid.
 */
static int
ice_apply_saved_phy_req_to_cfg(struct ice_softc *sc,
			       struct ice_aqc_set_phy_cfg_data *cfg)
{
	struct ice_phy_data phy_data = { 0 };
	struct ice_port_info *pi = sc->hw.port_info;
	u64 phy_low = 0, phy_high = 0;
	u16 link_speeds;
	int ret;

	link_speeds = pi->phy.curr_user_speed_req;

	if (ice_is_bit_set(sc->feat_en, ICE_FEATURE_LINK_MGMT_VER_2)) {
		memset(&phy_data, 0, sizeof(phy_data));
		phy_data.report_mode = ICE_AQC_REPORT_DFLT_CFG;
		phy_data.user_speeds_orig = link_speeds;
		ret = ice_intersect_phy_types_and_speeds(sc, &phy_data);
		if (ret != 0) {
			/* Error message already printed within function */
			return (ret);
		}
		phy_low = phy_data.phy_low_intr;
		phy_high = phy_data.phy_high_intr;

		if (link_speeds == 0 || phy_data.user_speeds_intr)
			goto finalize_link_speed;
		if (ice_is_bit_set(sc->feat_en, ICE_FEATURE_LENIENT_LINK_MODE)) {
			memset(&phy_data, 0, sizeof(phy_data));
			phy_data.report_mode = ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA;
			phy_data.user_speeds_orig = link_speeds;
			ret = ice_intersect_phy_types_and_speeds(sc, &phy_data);
			if (ret != 0) {
				/* Error message already printed within function */
				return (ret);
			}
			phy_low = phy_data.phy_low_intr;
			phy_high = phy_data.phy_high_intr;

			if (!phy_data.user_speeds_intr) {
				phy_low = phy_data.phy_low_orig;
				phy_high = phy_data.phy_high_orig;
			}
			goto finalize_link_speed;
		}
		/* If we're here, then it means the benefits of Version 2
		 * link management aren't utilized.  We fall through to
		 * handling Strict Link Mode the same as Version 1 link
		 * management.
		 */
	}

	memset(&phy_data, 0, sizeof(phy_data));
	if ((link_speeds == 0) &&
	    (sc->ldo_tlv.phy_type_low || sc->ldo_tlv.phy_type_high))
		phy_data.report_mode = ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA;
	else
		phy_data.report_mode = ICE_AQC_REPORT_TOPO_CAP_MEDIA;
	phy_data.user_speeds_orig = link_speeds;
	ret = ice_intersect_phy_types_and_speeds(sc, &phy_data);
	if (ret != 0) {
		/* Error message already printed within function */
		return (ret);
	}
	phy_low = phy_data.phy_low_intr;
	phy_high = phy_data.phy_high_intr;

	if (!ice_is_bit_set(sc->feat_en, ICE_FEATURE_LENIENT_LINK_MODE)) {
		if (phy_low == 0 && phy_high == 0) {
			device_printf(sc->dev,
			    "The selected speed is not supported by the current media. Please select a link speed that is supported by the current media.\n");
			return (EINVAL);
		}
	} else {
		if (link_speeds == 0) {
			if (sc->ldo_tlv.phy_type_low & phy_low ||
			    sc->ldo_tlv.phy_type_high & phy_high) {
				phy_low &= sc->ldo_tlv.phy_type_low;
				phy_high &= sc->ldo_tlv.phy_type_high;
			}
		} else if (phy_low == 0 && phy_high == 0) {
			memset(&phy_data, 0, sizeof(phy_data));
			phy_data.report_mode = ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA;
			phy_data.user_speeds_orig = link_speeds;
			ret = ice_intersect_phy_types_and_speeds(sc, &phy_data);
			if (ret != 0) {
				/* Error message already printed within function */
				return (ret);
			}
			phy_low = phy_data.phy_low_intr;
			phy_high = phy_data.phy_high_intr;

			if (!phy_data.user_speeds_intr) {
				phy_low = phy_data.phy_low_orig;
				phy_high = phy_data.phy_high_orig;
			}
		}
	}

finalize_link_speed:

	/* Cache new user settings for speeds */
	pi->phy.curr_user_speed_req = phy_data.user_speeds_intr;
	cfg->phy_type_low = htole64(phy_low);
	cfg->phy_type_high = htole64(phy_high);

	return (ret);
}

/**
 * ice_apply_saved_fec_req_to_cfg -- Write saved user FEC mode to cfg data
 * @sc: device private structure
 * @cfg: new PHY config data to be modified
 *
 * Applies user setting for FEC mode to PHY config struct. It uses the data
 * from pcaps to check if the saved settings are invalid and uses the pcaps
 * data instead if they are invalid.
 */
static int
ice_apply_saved_fec_req_to_cfg(struct ice_softc *sc,
			       struct ice_aqc_set_phy_cfg_data *cfg)
{
	struct ice_port_info *pi = sc->hw.port_info;
	enum ice_status status;

	cfg->caps &= ~ICE_AQC_PHY_EN_AUTO_FEC;
	status = ice_cfg_phy_fec(pi, cfg, pi->phy.curr_user_fec_req);
	if (status)
		return (EIO);

	return (0);
}

/**
 * ice_apply_saved_fc_req_to_cfg -- Write saved user flow control mode to cfg data
 * @pi: port info struct
 * @cfg: new PHY config data to be modified
 *
 * Applies user setting for flow control mode to PHY config struct. There are
 * no invalid flow control mode settings; if there are, then this function
 * treats them like "ICE_FC_NONE".
 */
static void
ice_apply_saved_fc_req_to_cfg(struct ice_port_info *pi,
			      struct ice_aqc_set_phy_cfg_data *cfg)
{
	cfg->caps &= ~(ICE_AQ_PHY_ENA_TX_PAUSE_ABILITY |
		       ICE_AQ_PHY_ENA_RX_PAUSE_ABILITY);

	switch (pi->phy.curr_user_fc_req) {
	case ICE_FC_FULL:
		cfg->caps |= ICE_AQ_PHY_ENA_TX_PAUSE_ABILITY |
			     ICE_AQ_PHY_ENA_RX_PAUSE_ABILITY;
		break;
	case ICE_FC_RX_PAUSE:
		cfg->caps |= ICE_AQ_PHY_ENA_RX_PAUSE_ABILITY;
		break;
	case ICE_FC_TX_PAUSE:
		cfg->caps |= ICE_AQ_PHY_ENA_TX_PAUSE_ABILITY;
		break;
	default:
		/* ICE_FC_NONE */
		break;
	}
}

/**
 * ice_apply_saved_phy_cfg -- Re-apply user PHY config settings
 * @sc: device private structure
 * @settings: which settings to apply
 *
 * Applies user settings for advertised speeds, FEC mode, and flow
 * control mode to a PHY config struct; it uses the data from pcaps
 * to check if the saved settings are invalid and uses the pcaps
 * data instead if they are invalid.
 *
 * For things like sysctls where only one setting needs to be
 * updated, the bitmap allows the caller to specify which setting
 * to update.
 */
int
ice_apply_saved_phy_cfg(struct ice_softc *sc, u8 settings)
{
	struct ice_aqc_set_phy_cfg_data cfg = { 0 };
	struct ice_port_info *pi = sc->hw.port_info;
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u64 phy_low, phy_high;
	enum ice_status status;
	enum ice_fec_mode dflt_fec_mode;
	u16 dflt_user_speed;

	if (!settings || settings > ICE_APPLY_LS_FEC_FC) {
		ice_debug(hw, ICE_DBG_LINK, "Settings out-of-bounds: %u\n",
		    settings);
	}

	status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG,
				     &pcaps, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_aq_get_phy_caps (ACTIVE) failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	phy_low = le64toh(pcaps.phy_type_low);
	phy_high = le64toh(pcaps.phy_type_high);

	/* Save off initial config parameters */
	dflt_user_speed = ice_aq_phy_types_to_link_speeds(phy_low, phy_high);
	dflt_fec_mode = ice_caps_to_fec_mode(pcaps.caps, pcaps.link_fec_options);

	/* Setup new PHY config */
	ice_copy_phy_caps_to_cfg(pi, &pcaps, &cfg);

	/* On error, restore active configuration values */
	if ((settings & ICE_APPLY_LS) &&
	    ice_apply_saved_phy_req_to_cfg(sc, &cfg)) {
		pi->phy.curr_user_speed_req = dflt_user_speed;
		cfg.phy_type_low = pcaps.phy_type_low;
		cfg.phy_type_high = pcaps.phy_type_high;
	}
	if ((settings & ICE_APPLY_FEC) &&
	    ice_apply_saved_fec_req_to_cfg(sc, &cfg)) {
		pi->phy.curr_user_fec_req = dflt_fec_mode;
	}
	if (settings & ICE_APPLY_FC) {
		/* No real error indicators for this process,
		 * so we'll just have to assume it works. */
		ice_apply_saved_fc_req_to_cfg(pi, &cfg);
	}

	/* Enable link and re-negotiate it */
	cfg.caps |= ICE_AQ_PHY_ENA_AUTO_LINK_UPDT | ICE_AQ_PHY_ENA_LINK;

	status = ice_aq_set_phy_cfg(hw, pi, &cfg, NULL);
	if (status != ICE_SUCCESS) {
		/* Don't indicate failure if there's no media in the port.
		 * The settings have been saved and will apply when media
		 * is inserted.
		 */
		if ((status == ICE_ERR_AQ_ERROR) &&
		    (hw->adminq.sq_last_status == ICE_AQ_RC_EBUSY)) {
			device_printf(dev,
			    "%s: Setting will be applied when media is inserted\n",
			    __func__);
			return (0);
		} else {
			device_printf(dev,
			    "%s: ice_aq_set_phy_cfg failed; status %s, aq_err %s\n",
			    __func__, ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
			return (EIO);
		}
	}

	return (0);
}

/**
 * ice_print_ldo_tlv - Print out LDO TLV information
 * @sc: device private structure
 * @tlv: LDO TLV information from the adapter NVM
 *
 * Dump out the information in tlv to the kernel message buffer; intended for
 * debugging purposes.
 */
static void
ice_print_ldo_tlv(struct ice_softc *sc, struct ice_link_default_override_tlv *tlv)
{
	device_t dev = sc->dev;

	device_printf(dev, "TLV: -options     0x%02x\n", tlv->options);
	device_printf(dev, "     -phy_config  0x%02x\n", tlv->phy_config);
	device_printf(dev, "     -fec_options 0x%02x\n", tlv->fec_options);
	device_printf(dev, "     -phy_high    0x%016llx\n",
	    (unsigned long long)tlv->phy_type_high);
	device_printf(dev, "     -phy_low     0x%016llx\n",
	    (unsigned long long)tlv->phy_type_low);
}

/**
 * ice_set_link_management_mode -- Strict or lenient link management
 * @sc: device private structure
 *
 * Some NVMs give the adapter the option to advertise a superset of link
 * configurations.  This checks to see if that option is enabled.
 * Further, the NVM could also provide a specific set of configurations
 * to try; these are cached in the driver's private structure if they
 * are available.
 */
void
ice_set_link_management_mode(struct ice_softc *sc)
{
	struct ice_port_info *pi = sc->hw.port_info;
	device_t dev = sc->dev;
	struct ice_link_default_override_tlv tlv = { 0 };
	enum ice_status status;

	/* Port must be in strict mode if FW version is below a certain
	 * version. (i.e. Don't set lenient mode features)
	 */
	if (!(ice_fw_supports_link_override(&sc->hw)))
		return;

	status = ice_get_link_default_override(&tlv, pi);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_get_link_default_override failed; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(sc->hw.adminq.sq_last_status));
		return;
	}

	if (sc->hw.debug_mask & ICE_DBG_LINK)
		ice_print_ldo_tlv(sc, &tlv);

	/* Set lenient link mode */
	if (ice_is_bit_set(sc->feat_cap, ICE_FEATURE_LENIENT_LINK_MODE) &&
	    (!(tlv.options & ICE_LINK_OVERRIDE_STRICT_MODE)))
		ice_set_bit(ICE_FEATURE_LENIENT_LINK_MODE, sc->feat_en);

	/* FW supports reporting a default configuration */
	if (ice_is_bit_set(sc->feat_cap, ICE_FEATURE_LINK_MGMT_VER_2) &&
	    ice_fw_supports_report_dflt_cfg(&sc->hw)) {
		ice_set_bit(ICE_FEATURE_LINK_MGMT_VER_2, sc->feat_en);
		/* Knowing we're at a high enough firmware revision to
		 * support this link management configuration, we don't
		 * need to check/support earlier versions.
		 */
		return;
	}

	/* Default overrides only work if in lenient link mode */
	if (ice_is_bit_set(sc->feat_cap, ICE_FEATURE_LINK_MGMT_VER_1) &&
	    ice_is_bit_set(sc->feat_en, ICE_FEATURE_LENIENT_LINK_MODE) &&
	    (tlv.options & ICE_LINK_OVERRIDE_EN))
		ice_set_bit(ICE_FEATURE_LINK_MGMT_VER_1, sc->feat_en);

	/* Cache the LDO TLV structure in the driver, since it
	 * won't change during the driver's lifetime.
	 */
	sc->ldo_tlv = tlv;
}

/**
 * ice_init_saved_phy_cfg -- Set cached user PHY cfg settings with NVM defaults
 * @sc: device private structure
 *
 * This should be called before the tunables for these link settings
 * (e.g. advertise_speed) are added -- so that these defaults don't overwrite
 * the cached values that the sysctl handlers will write.
 *
 * This also needs to be called before ice_init_link_configuration, to ensure
 * that there are sane values that can be written if there is media available
 * in the port.
 */
void
ice_init_saved_phy_cfg(struct ice_softc *sc)
{
	struct ice_port_info *pi = sc->hw.port_info;
	struct ice_aqc_get_phy_caps_data pcaps = { 0 };
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	u64 phy_low, phy_high;
	u8 report_mode = ICE_AQC_REPORT_TOPO_CAP_MEDIA;

	if (ice_is_bit_set(sc->feat_en, ICE_FEATURE_LINK_MGMT_VER_2))
		report_mode = ICE_AQC_REPORT_DFLT_CFG;
	status = ice_aq_get_phy_caps(pi, false, report_mode, &pcaps, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "%s: ice_aq_get_phy_caps (%s) failed; status %s, aq_err %s\n",
		    __func__,
		    report_mode == ICE_AQC_REPORT_DFLT_CFG ? "DFLT" : "w/MEDIA",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return;
	}

	phy_low = le64toh(pcaps.phy_type_low);
	phy_high = le64toh(pcaps.phy_type_high);

	/* Save off initial config parameters */
	pi->phy.curr_user_speed_req =
	   ice_aq_phy_types_to_link_speeds(phy_low, phy_high);
	pi->phy.curr_user_fec_req = ice_caps_to_fec_mode(pcaps.caps,
	    pcaps.link_fec_options);
	pi->phy.curr_user_fc_req = ice_caps_to_fc_mode(pcaps.caps);
}

/**
 * ice_module_init - Driver callback to handle module load
 *
 * Callback for handling module load events. This function should initialize
 * any data structures that are used for the life of the device driver.
 */
static int
ice_module_init(void)
{
	ice_rdma_init();
	return (0);
}

/**
 * ice_module_exit - Driver callback to handle module exit
 *
 * Callback for handling module unload events. This function should release
 * any resources initialized during ice_module_init.
 *
 * If this function returns non-zero, the module will not be unloaded. It
 * should only return such a value if the module cannot be unloaded at all,
 * such as due to outstanding memory references that cannot be revoked.
 */
static int
ice_module_exit(void)
{
	ice_rdma_exit();
	return (0);
}

/**
 * ice_module_event_handler - Callback for module events
 * @mod: unused module_t parameter
 * @what: the event requested
 * @arg: unused event argument
 *
 * Callback used to handle module events from the stack. Used to allow the
 * driver to define custom behavior that should happen at module load and
 * unload.
 */
int
ice_module_event_handler(module_t __unused mod, int what, void __unused *arg)
{
	switch (what) {
	case MOD_LOAD:
		return ice_module_init();
	case MOD_UNLOAD:
		return ice_module_exit();
	default:
		/* TODO: do we need to handle MOD_QUIESCE and MOD_SHUTDOWN? */
		return (EOPNOTSUPP);
	}
}

/**
 * ice_handle_nvm_access_ioctl - Handle an NVM access ioctl request
 * @sc: the device private softc
 * @ifd: ifdrv ioctl request pointer
 */
int
ice_handle_nvm_access_ioctl(struct ice_softc *sc, struct ifdrv *ifd)
{
	union ice_nvm_access_data *data;
	struct ice_nvm_access_cmd *cmd;
	size_t ifd_len = ifd->ifd_len, malloc_len;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	u8 *nvm_buffer;
	int err;

	/*
	 * ifioctl forwards SIOCxDRVSPEC to iflib without performing
	 * a privilege check. In turn, iflib forwards the ioctl to the driver
	 * without performing a privilege check. Perform one here to ensure
	 * that non-privileged threads cannot access this interface.
	 */
	err = priv_check(curthread, PRIV_DRIVER);
	if (err)
		return (err);

	if (ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET)) {
		device_printf(dev, "%s: Driver must rebuild data structures after a reset. Operation aborted.\n",
			      __func__);
		return (EBUSY);
	}

	if (ifd_len < sizeof(struct ice_nvm_access_cmd)) {
		device_printf(dev, "%s: ifdrv length is too small. Got %zu, but expected %zu\n",
			      __func__, ifd_len, sizeof(struct ice_nvm_access_cmd));
		return (EINVAL);
	}

	if (ifd->ifd_data == NULL) {
		device_printf(dev, "%s: ifd data buffer not present.\n",
			      __func__);
		return (EINVAL);
	}

	/*
	 * If everything works correctly, ice_handle_nvm_access should not
	 * modify data past the size of the ioctl length. However, it could
	 * lead to memory corruption if it did. Make sure to allocate at least
	 * enough space for the command and data regardless. This
	 * ensures that any access to the data union will not access invalid
	 * memory.
	 */
	malloc_len = max(ifd_len, sizeof(*data) + sizeof(*cmd));

	nvm_buffer = (u8 *)malloc(malloc_len, M_ICE, M_ZERO | M_WAITOK);
	if (!nvm_buffer)
		return (ENOMEM);

	/* Copy the NVM access command and data in from user space */
	/* coverity[tainted_data_argument] */
	err = copyin(ifd->ifd_data, nvm_buffer, ifd_len);
	if (err) {
		device_printf(dev, "%s: Copying request from user space failed, err %s\n",
			      __func__, ice_err_str(err));
		goto cleanup_free_nvm_buffer;
	}

	/*
	 * The NVM command structure is immediately followed by data which
	 * varies in size based on the command.
	 */
	cmd = (struct ice_nvm_access_cmd *)nvm_buffer;
	data = (union ice_nvm_access_data *)(nvm_buffer + sizeof(struct ice_nvm_access_cmd));

	/* Handle the NVM access request */
	status = ice_handle_nvm_access(hw, cmd, data);
	if (status)
		ice_debug(hw, ICE_DBG_NVM,
			  "NVM access request failed, err %s\n",
			  ice_status_str(status));

	/* Copy the possibly modified contents of the handled request out */
	err = copyout(nvm_buffer, ifd->ifd_data, ifd_len);
	if (err) {
		device_printf(dev, "%s: Copying response back to user space failed, err %s\n",
			      __func__, ice_err_str(err));
		goto cleanup_free_nvm_buffer;
	}

	/* Convert private status to an error code for proper ioctl response */
	switch (status) {
	case ICE_SUCCESS:
		err = (0);
		break;
	case ICE_ERR_NO_MEMORY:
		err = (ENOMEM);
		break;
	case ICE_ERR_OUT_OF_RANGE:
		err = (ENOTTY);
		break;
	case ICE_ERR_PARAM:
	default:
		err = (EINVAL);
		break;
	}

cleanup_free_nvm_buffer:
	free(nvm_buffer, M_ICE);
	return err;
}

/**
 * ice_read_sff_eeprom - Read data from SFF eeprom
 * @sc: device softc
 * @dev_addr: I2C device address (typically 0xA0 or 0xA2)
 * @offset: offset into the eeprom
 * @data: pointer to data buffer to store read data in
 * @length: length to read; max length is 16
 *
 * Read from the SFF eeprom in the module for this PF's port. For more details
 * on the contents of an SFF eeprom, refer to SFF-8724 (SFP), SFF-8636 (QSFP),
 * and SFF-8024 (both).
 */
int
ice_read_sff_eeprom(struct ice_softc *sc, u16 dev_addr, u16 offset, u8* data, u16 length)
{
	struct ice_hw *hw = &sc->hw;
	int ret = 0, retries = 0;
	enum ice_status status;

	if (length > 16)
		return (EINVAL);

	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (ENOSYS);

	if (ice_test_state(&sc->state, ICE_STATE_NO_MEDIA))
		return (ENXIO);

	do {
		status = ice_aq_sff_eeprom(hw, 0, dev_addr,
					   offset, 0, 0, data, length,
					   false, NULL);
		if (!status) {
			ret = 0;
			break;
		}
		if (status == ICE_ERR_AQ_ERROR &&
		    hw->adminq.sq_last_status == ICE_AQ_RC_EBUSY) {
			ret = EBUSY;
			continue;
		}
		if (status == ICE_ERR_AQ_ERROR &&
		    hw->adminq.sq_last_status == ICE_AQ_RC_EACCES) {
			/* FW says I2C access isn't supported */
			ret = EACCES;
			break;
		}
		if (status == ICE_ERR_AQ_ERROR &&
		    hw->adminq.sq_last_status == ICE_AQ_RC_EPERM) {
			device_printf(sc->dev,
				  "%s: Module pointer location specified in command does not permit the required operation.\n",
				  __func__);
			ret = EPERM;
			break;
		} else {
			device_printf(sc->dev,
				  "%s: Error reading I2C data: err %s aq_err %s\n",
				  __func__, ice_status_str(status),
				  ice_aq_str(hw->adminq.sq_last_status));
			ret = EIO;
			break;
		}
	} while (retries++ < ICE_I2C_MAX_RETRIES);

	if (ret == EBUSY)
		device_printf(sc->dev,
			  "%s: Error reading I2C data after %d retries\n",
			  __func__, ICE_I2C_MAX_RETRIES);

	return (ret);
}

/**
 * ice_handle_i2c_req - Driver independent I2C request handler
 * @sc: device softc
 * @req: The I2C parameters to use
 *
 * Read from the port's I2C eeprom using the parameters from the ioctl.
 */
int
ice_handle_i2c_req(struct ice_softc *sc, struct ifi2creq *req)
{
	return ice_read_sff_eeprom(sc, req->dev_addr, req->offset, req->data, req->len);
}

/**
 * ice_sysctl_read_i2c_diag_data - Read some module diagnostic data via i2c
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Read 8 bytes of diagnostic data from the SFF eeprom in the (Q)SFP module
 * inserted into the port.
 *
 *             | SFP A2  | QSFP Lower Page
 * ------------|---------|----------------
 * Temperature | 96-97	 | 22-23
 * Vcc         | 98-99   | 26-27
 * TX power    | 102-103 | 34-35..40-41
 * RX power    | 104-105 | 50-51..56-57
 */
static int
ice_sysctl_read_i2c_diag_data(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	device_t dev = sc->dev;
	struct sbuf *sbuf;
	int ret;
	u8 data[16];

	UNREFERENCED_PARAMETER(arg2);
	UNREFERENCED_PARAMETER(oidp);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	if (req->oldptr == NULL) {
		ret = SYSCTL_OUT(req, 0, 128);
		return (ret);
	}

	ret = ice_read_sff_eeprom(sc, 0xA0, 0, data, 1);
	if (ret)
		return (ret);

	/* 0x3 for SFP; 0xD/0x11 for QSFP+/QSFP28 */
	if (data[0] == 0x3) {
		/*
		 * Check for:
		 * - Internally calibrated data
		 * - Diagnostic monitoring is implemented
		 */
		ice_read_sff_eeprom(sc, 0xA0, 92, data, 1);
		if (!(data[0] & 0x60)) {
			device_printf(dev, "Module doesn't support diagnostics: 0xA0[92] = %02X\n", data[0]);
			return (ENODEV);
		}

		sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

		ice_read_sff_eeprom(sc, 0xA2, 96, data, 4);
		for (int i = 0; i < 4; i++)
			sbuf_printf(sbuf, "%02X ", data[i]);

		ice_read_sff_eeprom(sc, 0xA2, 102, data, 4);
		for (int i = 0; i < 4; i++)
			sbuf_printf(sbuf, "%02X ", data[i]);
	} else if (data[0] == 0xD || data[0] == 0x11) {
		/*
		 * QSFP+ modules are always internally calibrated, and must indicate
		 * what types of diagnostic monitoring are implemented
		 */
		sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

		ice_read_sff_eeprom(sc, 0xA0, 22, data, 2);
		for (int i = 0; i < 2; i++)
			sbuf_printf(sbuf, "%02X ", data[i]);

		ice_read_sff_eeprom(sc, 0xA0, 26, data, 2);
		for (int i = 0; i < 2; i++)
			sbuf_printf(sbuf, "%02X ", data[i]);

		ice_read_sff_eeprom(sc, 0xA0, 34, data, 2);
		for (int i = 0; i < 2; i++)
			sbuf_printf(sbuf, "%02X ", data[i]);

		ice_read_sff_eeprom(sc, 0xA0, 50, data, 2);
		for (int i = 0; i < 2; i++)
			sbuf_printf(sbuf, "%02X ", data[i]);
	} else {
		device_printf(dev, "Module is not SFP/SFP+/SFP28/QSFP+ (%02X)\n", data[0]);
		return (ENODEV);
	}

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_alloc_intr_tracking - Setup interrupt tracking structures
 * @sc: device softc structure
 *
 * Sets up the resource manager for keeping track of interrupt allocations,
 * and initializes the tracking maps for the PF's interrupt allocations.
 *
 * Unlike the scheme for queues, this is done in one step since both the
 * manager and the maps both have the same lifetime.
 *
 * @returns 0 on success, or an error code on failure.
 */
int
ice_alloc_intr_tracking(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	int err;

	/* Initialize the interrupt allocation manager */
	err = ice_resmgr_init_contig_only(&sc->imgr,
	    hw->func_caps.common_cap.num_msix_vectors);
	if (err) {
		device_printf(dev, "Unable to initialize PF interrupt manager: %s\n",
			      ice_err_str(err));
		return (err);
	}

	/* Allocate PF interrupt mapping storage */
	if (!(sc->pf_imap =
	      (u16 *)malloc(sizeof(u16) * hw->func_caps.common_cap.num_msix_vectors,
	      M_ICE, M_NOWAIT))) {
		device_printf(dev, "Unable to allocate PF imap memory\n");
		err = ENOMEM;
		goto free_imgr;
	}
	if (!(sc->rdma_imap =
	      (u16 *)malloc(sizeof(u16) * hw->func_caps.common_cap.num_msix_vectors,
	      M_ICE, M_NOWAIT))) {
		device_printf(dev, "Unable to allocate RDMA imap memory\n");
		err = ENOMEM;
		free(sc->pf_imap, M_ICE);
		goto free_imgr;
	}
	for (u32 i = 0; i < hw->func_caps.common_cap.num_msix_vectors; i++) {
		sc->pf_imap[i] = ICE_INVALID_RES_IDX;
		sc->rdma_imap[i] = ICE_INVALID_RES_IDX;
	}

	return (0);

free_imgr:
	ice_resmgr_destroy(&sc->imgr);
	return (err);
}

/**
 * ice_free_intr_tracking - Free PF interrupt tracking structures
 * @sc: device softc structure
 *
 * Frees the interrupt resource allocation manager and the PF's owned maps.
 *
 * VF maps are released when the owning VF's are destroyed, which should always
 * happen before this function is called.
 */
void
ice_free_intr_tracking(struct ice_softc *sc)
{
	if (sc->pf_imap) {
		ice_resmgr_release_map(&sc->imgr, sc->pf_imap,
				       sc->lan_vectors);
		free(sc->pf_imap, M_ICE);
		sc->pf_imap = NULL;
	}
	if (sc->rdma_imap) {
		ice_resmgr_release_map(&sc->imgr, sc->rdma_imap,
				       sc->lan_vectors);
		free(sc->rdma_imap, M_ICE);
		sc->rdma_imap = NULL;
	}

	ice_resmgr_destroy(&sc->imgr);
}

/**
 * ice_apply_supported_speed_filter - Mask off unsupported speeds
 * @report_speeds: bit-field for the desired link speeds
 * @mod_type: type of module/sgmii connection we have
 *
 * Given a bitmap of the desired lenient mode link speeds,
 * this function will mask off the speeds that are not currently
 * supported by the device.
 */
static u16
ice_apply_supported_speed_filter(u16 report_speeds, u8 mod_type)
{
	u16 speed_mask;
	enum { IS_SGMII, IS_SFP, IS_QSFP } module;

	/*
	 * The SFF specification says 0 is unknown, so we'll
	 * treat it like we're connected through SGMII for now.
	 * This may need revisiting if a new type is supported
	 * in the future.
	 */
	switch (mod_type) {
	case 0:
		module = IS_SGMII;
		break;
	case 3:
		module = IS_SFP;
		break;
	default:
		module = IS_QSFP;
		break;
	}

	/* We won't offer anything lower than 100M for any part,
	 * but we'll need to mask off other speeds based on the
	 * device and module type.
	 */
	speed_mask = ~((u16)ICE_AQ_LINK_SPEED_100MB - 1);
	if ((report_speeds & ICE_AQ_LINK_SPEED_10GB) && (module == IS_SFP))
		speed_mask = ~((u16)ICE_AQ_LINK_SPEED_1000MB - 1);
	if (report_speeds & ICE_AQ_LINK_SPEED_25GB)
		speed_mask = ~((u16)ICE_AQ_LINK_SPEED_1000MB - 1);
	if (report_speeds & ICE_AQ_LINK_SPEED_50GB) {
		speed_mask = ~((u16)ICE_AQ_LINK_SPEED_1000MB - 1);
		if (module == IS_QSFP)
			speed_mask = ~((u16)ICE_AQ_LINK_SPEED_10GB - 1);
	}
	if (report_speeds & ICE_AQ_LINK_SPEED_100GB)
		speed_mask = ~((u16)ICE_AQ_LINK_SPEED_25GB - 1);
	return (report_speeds & speed_mask);
}

/**
 * ice_init_health_events - Enable FW health event reporting
 * @sc: device softc
 *
 * Will try to enable firmware health event reporting, but shouldn't
 * cause any grief (to the caller) if this fails.
 */
void
ice_init_health_events(struct ice_softc *sc)
{
	enum ice_status status;
	u8 health_mask;

	if ((!ice_is_bit_set(sc->feat_cap, ICE_FEATURE_HEALTH_STATUS)) ||
		(!sc->enable_health_events))
		return;

	health_mask = ICE_AQC_HEALTH_STATUS_SET_PF_SPECIFIC_MASK |
		      ICE_AQC_HEALTH_STATUS_SET_GLOBAL_MASK;

	status = ice_aq_set_health_status_config(&sc->hw, health_mask, NULL);
	if (status)
		device_printf(sc->dev,
		    "Failed to enable firmware health events, err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(sc->hw.adminq.sq_last_status));
	else
		ice_set_bit(ICE_FEATURE_HEALTH_STATUS, sc->feat_en);
}

/**
 * ice_print_health_status_string - Print message for given FW health event
 * @dev: the PCIe device
 * @elem: health status element containing status code
 *
 * A rather large list of possible health status codes and their associated
 * messages.
 */
static void
ice_print_health_status_string(device_t dev,
			       struct ice_aqc_health_status_elem *elem)
{
	u16 status_code = le16toh(elem->health_status_code);

	switch (status_code) {
	case ICE_AQC_HEALTH_STATUS_INFO_RECOVERY:
		device_printf(dev, "The device is in firmware recovery mode.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_FLASH_ACCESS:
		device_printf(dev, "The flash chip cannot be accessed.\n");
		device_printf(dev, "Possible Solution: If issue persists, call customer support.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_NVM_AUTH:
		device_printf(dev, "NVM authentication failed.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_OROM_AUTH:
		device_printf(dev, "Option ROM authentication failed.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_DDP_AUTH:
		device_printf(dev, "DDP package failed.\n");
		device_printf(dev, "Possible Solution: Update to latest base driver and DDP package.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_NVM_COMPAT:
		device_printf(dev, "NVM image is incompatible.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_OROM_COMPAT:
		device_printf(dev, "Option ROM is incompatible.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_DCB_MIB:
		device_printf(dev, "Supplied MIB file is invalid. DCB reverted to default configuration.\n");
		device_printf(dev, "Possible Solution: Disable FW-LLDP and check DCBx system configuration.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_UNKNOWN_MOD_STRICT:
		device_printf(dev, "An unsupported module was detected.\n");
		device_printf(dev, "Possible Solution 1: Check your cable connection.\n");
		device_printf(dev, "Possible Solution 2: Change or replace the module or cable.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_MOD_TYPE:
		device_printf(dev, "Module type is not supported.\n");
		device_printf(dev, "Possible Solution: Change or replace the module or cable.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_MOD_QUAL:
		device_printf(dev, "Module is not qualified.\n");
		device_printf(dev, "Possible Solution 1: Check your cable connection.\n");
		device_printf(dev, "Possible Solution 2: Change or replace the module or cable.\n");
		device_printf(dev, "Possible Solution 3: Manually set speed and duplex.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_MOD_COMM:
		device_printf(dev, "Device cannot communicate with the module.\n");
		device_printf(dev, "Possible Solution 1: Check your cable connection.\n");
		device_printf(dev, "Possible Solution 2: Change or replace the module or cable.\n");
		device_printf(dev, "Possible Solution 3: Manually set speed and duplex.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_MOD_CONFLICT:
		device_printf(dev, "Unresolved module conflict.\n");
		device_printf(dev, "Possible Solution 1: Manually set speed/duplex or use Intel(R) Ethernet Port Configuration Tool to change the port option.\n");
		device_printf(dev, "Possible Solution 2: If the problem persists, use a cable/module that is found in the supported modules and cables list for this device.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_MOD_NOT_PRESENT:
		device_printf(dev, "Module is not present.\n");
		device_printf(dev, "Possible Solution 1: Check that the module is inserted correctly.\n");
		device_printf(dev, "Possible Solution 2: If the problem persists, use a cable/module that is found in the supported modules and cables list for this device.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_INFO_MOD_UNDERUTILIZED:
		device_printf(dev, "Underutilized module.\n");
		device_printf(dev, "Possible Solution 1: Change or replace the module or cable.\n");
		device_printf(dev, "Possible Solution 2: Use Intel(R) Ethernet Port Configuration Tool to change the port option.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_UNKNOWN_MOD_LENIENT:
		device_printf(dev, "An unsupported module was detected.\n");
		device_printf(dev, "Possible Solution 1: Check your cable connection.\n");
		device_printf(dev, "Possible Solution 2: Change or replace the module or cable.\n");
		device_printf(dev, "Possible Solution 3: Manually set speed and duplex.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_INVALID_LINK_CFG:
		device_printf(dev, "Invalid link configuration.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_PORT_ACCESS:
		device_printf(dev, "Port hardware access error.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_PORT_UNREACHABLE:
		device_printf(dev, "A port is unreachable.\n");
		device_printf(dev, "Possible Solution 1: Use Intel(R) Ethernet Port Configuration Tool to change the port option.\n");
		device_printf(dev, "Possible Solution 2: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_INFO_PORT_SPEED_MOD_LIMITED:
		device_printf(dev, "Port speed is limited due to module.\n");
		device_printf(dev, "Possible Solution: Change the module or use Intel(R) Ethernet Port Configuration Tool to configure the port option to match the current module speed.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_PARALLEL_FAULT:
		device_printf(dev, "A parallel fault was detected.\n");
		device_printf(dev, "Possible Solution: Check link partner connection and configuration.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_INFO_PORT_SPEED_PHY_LIMITED:
		device_printf(dev, "Port speed is limited by PHY capabilities.\n");
		device_printf(dev, "Possible Solution 1: Change the module to align to port option.\n");
		device_printf(dev, "Possible Solution 2: Use Intel(R) Ethernet Port Configuration Tool to change the port option.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_NETLIST_TOPO:
		device_printf(dev, "LOM topology netlist is corrupted.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_NETLIST:
		device_printf(dev, "Unrecoverable netlist error.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_TOPO_CONFLICT:
		device_printf(dev, "Port topology conflict.\n");
		device_printf(dev, "Possible Solution 1: Use Intel(R) Ethernet Port Configuration Tool to change the port option.\n");
		device_printf(dev, "Possible Solution 2: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_LINK_HW_ACCESS:
		device_printf(dev, "Unrecoverable hardware access error.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_LINK_RUNTIME:
		device_printf(dev, "Unrecoverable runtime error.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	case ICE_AQC_HEALTH_STATUS_ERR_DNL_INIT:
		device_printf(dev, "Link management engine failed to initialize.\n");
		device_printf(dev, "Possible Solution: Update to the latest NVM image.\n");
		break;
	default:
		break;
	}
}

/**
 * ice_handle_health_status_event - helper function to output health status
 * @sc: device softc structure
 * @event: event received on a control queue
 *
 * Prints out the appropriate string based on the given Health Status Event
 * code.
 */
static void
ice_handle_health_status_event(struct ice_softc *sc,
			       struct ice_rq_event_info *event)
{
	struct ice_aqc_health_status_elem *health_info;
	u16 status_count;
	int i;

	if (!ice_is_bit_set(sc->feat_en, ICE_FEATURE_HEALTH_STATUS))
		return;

	health_info = (struct ice_aqc_health_status_elem *)event->msg_buf;
	status_count = le16toh(event->desc.params.get_health_status.health_status_count);

	if (status_count > (event->buf_len / sizeof(*health_info))) {
		device_printf(sc->dev, "Received a health status event with invalid event count\n");
		return;
	}

	for (i = 0; i < status_count; i++) {
		ice_print_health_status_string(sc->dev, health_info);
		health_info++;
	}
}

/**
 * ice_set_default_local_lldp_mib - Possibly apply local LLDP MIB to FW
 * @sc: device softc structure
 *
 * This function needs to be called after link up; it makes sure the FW has
 * certain PFC/DCB settings. In certain configurations this will re-apply a
 * default local LLDP MIB configuration; this is intended to workaround a FW
 * behavior where these settings seem to be cleared on link up.
 */
void
ice_set_default_local_lldp_mib(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi;
	device_t dev = sc->dev;
	enum ice_status status;

	/* Set Local MIB can disrupt flow control settings for
	 * non-DCB-supported devices.
	 */
	if (!ice_is_bit_set(sc->feat_en, ICE_FEATURE_DCB))
		return;

	pi = hw->port_info;

	/* Don't overwrite a custom SW configuration */
	if (!pi->qos_cfg.is_sw_lldp &&
	    !ice_test_state(&sc->state, ICE_STATE_MULTIPLE_TCS))
		ice_set_default_local_mib_settings(sc);

	status = ice_set_dcb_cfg(pi);

	if (status)
		device_printf(dev,
		    "Error setting Local LLDP MIB: %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
}

/**
 * ice_sbuf_print_ets_cfg - Helper function to print ETS cfg
 * @sbuf: string buffer to print to
 * @name: prefix string to use
 * @ets: structure to pull values from
 *
 * A helper function for ice_sysctl_dump_dcbx_cfg(), this
 * formats the ETS rec and cfg TLVs into text.
 */
static void
ice_sbuf_print_ets_cfg(struct sbuf *sbuf, const char *name, struct ice_dcb_ets_cfg *ets)
{
	sbuf_printf(sbuf, "%s.willing: %u\n", name, ets->willing);
	sbuf_printf(sbuf, "%s.cbs: %u\n", name, ets->cbs);
	sbuf_printf(sbuf, "%s.maxtcs: %u\n", name, ets->maxtcs);

	sbuf_printf(sbuf, "%s.prio_table:", name);
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++)
		sbuf_printf(sbuf, " %d", ets->prio_table[i]);
	sbuf_printf(sbuf, "\n");

	sbuf_printf(sbuf, "%s.tcbwtable:", name);
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++)
		sbuf_printf(sbuf, " %d", ets->tcbwtable[i]);
	sbuf_printf(sbuf, "\n");

	sbuf_printf(sbuf, "%s.tsatable:", name);
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++)
		sbuf_printf(sbuf, " %d", ets->tsatable[i]);
	sbuf_printf(sbuf, "\n");
}

/**
 * ice_sysctl_dump_dcbx_cfg - Print out DCBX/DCB config info
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: AQ define for either Local or Remote MIB
 * @req: sysctl request pointer
 *
 * Prints out DCB/DCBX configuration, including the contents
 * of either the local or remote MIB, depending on the value
 * used in arg2.
 */
static int
ice_sysctl_dump_dcbx_cfg(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_aqc_get_cee_dcb_cfg_resp cee_cfg = {};
	struct ice_dcbx_cfg dcb_buf = {};
	struct ice_dcbx_cfg *dcbcfg;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct sbuf *sbuf;
	enum ice_status status;
	u8 maxtcs, dcbx_status, is_sw_lldp;

	UNREFERENCED_PARAMETER(oidp);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	is_sw_lldp = hw->port_info->qos_cfg.is_sw_lldp;

	/* The driver doesn't receive a Remote MIB via SW */
	if (is_sw_lldp && arg2 == ICE_AQ_LLDP_MIB_REMOTE)
		return (ENOENT);

	dcbcfg = &hw->port_info->qos_cfg.local_dcbx_cfg;
	if (!is_sw_lldp) {
		/* Collect information from the FW in FW LLDP mode */
		dcbcfg = &dcb_buf;
		status = ice_aq_get_dcb_cfg(hw, (u8)arg2,
		    ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID, dcbcfg);
		if (status && arg2 == ICE_AQ_LLDP_MIB_REMOTE &&
		    hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT) {
			device_printf(dev,
			    "Unable to query Remote MIB; port has not received one yet\n");
			return (ENOENT);
		}
		if (status) {
			device_printf(dev, "Unable to query LLDP MIB, err %s aq_err %s\n",
			    ice_status_str(status),
			    ice_aq_str(hw->adminq.sq_last_status));
			return (EIO);
		}
	}

	status = ice_aq_get_cee_dcb_cfg(hw, &cee_cfg, NULL);
	if (status == ICE_SUCCESS)
		dcbcfg->dcbx_mode = ICE_DCBX_MODE_CEE;
	else if (hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT)
		dcbcfg->dcbx_mode = ICE_DCBX_MODE_IEEE;
	else
		device_printf(dev, "Get CEE DCB Cfg AQ cmd err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));

	maxtcs = hw->func_caps.common_cap.maxtc;
	dcbx_status = ice_get_dcbx_status(hw);

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	/* Do the actual printing */
	sbuf_printf(sbuf, "\n");
	sbuf_printf(sbuf, "SW LLDP mode: %d\n", is_sw_lldp);
	sbuf_printf(sbuf, "Function caps maxtcs: %d\n", maxtcs);
	sbuf_printf(sbuf, "dcbx_status: %d\n", dcbx_status);

	sbuf_printf(sbuf, "numapps: %u\n", dcbcfg->numapps);
	sbuf_printf(sbuf, "CEE TLV status: %u\n", dcbcfg->tlv_status);
	sbuf_printf(sbuf, "pfc_mode: %s\n", (dcbcfg->pfc_mode == ICE_QOS_MODE_DSCP) ?
	    "DSCP" : "VLAN");
	sbuf_printf(sbuf, "dcbx_mode: %s\n",
	    (dcbcfg->dcbx_mode == ICE_DCBX_MODE_IEEE) ? "IEEE" :
	    (dcbcfg->dcbx_mode == ICE_DCBX_MODE_CEE) ? "CEE" :
	    "Unknown");

	ice_sbuf_print_ets_cfg(sbuf, "etscfg", &dcbcfg->etscfg);
	ice_sbuf_print_ets_cfg(sbuf, "etsrec", &dcbcfg->etsrec);

	sbuf_printf(sbuf, "pfc.willing: %u\n", dcbcfg->pfc.willing);
	sbuf_printf(sbuf, "pfc.mbc: %u\n", dcbcfg->pfc.mbc);
	sbuf_printf(sbuf, "pfc.pfccap: 0x%0x\n", dcbcfg->pfc.pfccap);
	sbuf_printf(sbuf, "pfc.pfcena: 0x%0x\n", dcbcfg->pfc.pfcena);

	if (arg2 == ICE_AQ_LLDP_MIB_LOCAL) {
		sbuf_printf(sbuf, "dscp_map:\n");
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++)
				sbuf_printf(sbuf, " %d",
					    dcbcfg->dscp_map[i * 8 + j]);
			sbuf_printf(sbuf, "\n");
		}

		sbuf_printf(sbuf, "\nLocal registers:\n");
		sbuf_printf(sbuf, "PRTDCB_GENC.NUMTC: %d\n",
		    (rd32(hw, PRTDCB_GENC) & PRTDCB_GENC_NUMTC_M)
		        >> PRTDCB_GENC_NUMTC_S);
		sbuf_printf(sbuf, "PRTDCB_TUP2TC: 0x%0x\n",
		    (rd32(hw, PRTDCB_TUP2TC)));
		sbuf_printf(sbuf, "PRTDCB_RUP2TC: 0x%0x\n",
		    (rd32(hw, PRTDCB_RUP2TC)));
		sbuf_printf(sbuf, "GLDCB_TC2PFC: 0x%0x\n",
		    (rd32(hw, GLDCB_TC2PFC)));
	}

	/* Finish */
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_dump_vsi_cfg - print PF LAN VSI configuration
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * XXX: This could be extended to apply to arbitrary PF-owned VSIs,
 * but for simplicity, this only works on the PF's LAN VSI.
 */
static int
ice_sysctl_dump_vsi_cfg(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_vsi_ctx ctx = { 0 };
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct sbuf *sbuf;
	enum ice_status status;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	/* Get HW absolute index of a VSI */
	ctx.vsi_num = ice_get_hw_vsi_num(hw, sc->pf_vsi.idx);

	status = ice_aq_get_vsi_params(hw, &ctx, NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "Get VSI AQ call failed, err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	/* Do the actual printing */
	sbuf_printf(sbuf, "\n");

	sbuf_printf(sbuf, "VSI NUM: %d\n", ctx.vsi_num);
	sbuf_printf(sbuf, "VF  NUM: %d\n", ctx.vf_num);
	sbuf_printf(sbuf, "VSIs allocated: %d\n", ctx.vsis_allocd);
	sbuf_printf(sbuf, "VSIs unallocated: %d\n", ctx.vsis_unallocated);

	sbuf_printf(sbuf, "Rx Queue Map method: %d\n",
	    LE16_TO_CPU(ctx.info.mapping_flags));
	/* The PF VSI is always contiguous, so there's no if-statement here */
	sbuf_printf(sbuf, "Rx Queue base: %d\n",
	    LE16_TO_CPU(ctx.info.q_mapping[0]));
	sbuf_printf(sbuf, "Rx Queue count: %d\n",
	    LE16_TO_CPU(ctx.info.q_mapping[1]));

	sbuf_printf(sbuf, "TC qbases  :");
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		sbuf_printf(sbuf, " %4d",
		    ctx.info.tc_mapping[i] & ICE_AQ_VSI_TC_Q_OFFSET_M);
	}
	sbuf_printf(sbuf, "\n");

	sbuf_printf(sbuf, "TC qcounts :");
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		sbuf_printf(sbuf, " %4d",
		    1 << (ctx.info.tc_mapping[i] >> ICE_AQ_VSI_TC_Q_NUM_S));
	}

	/* Finish */
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_ets_str_to_tbl - Parse string into ETS table
 * @str: input string to parse
 * @table: output eight values used for ETS values
 * @limit: max valid value to accept for ETS values
 *
 * Parses a string and converts the eight values within
 * into a table that can be used in setting ETS settings
 * in a MIB.
 *
 * @return 0 on success, EINVAL if a parsed value is
 * not between 0 and limit.
 */
static int
ice_ets_str_to_tbl(const char *str, u8 *table, u8 limit)
{
	const char *str_start = str;
	char *str_end;
	long token;

	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		token = strtol(str_start, &str_end, 0);
		if (token < 0 || token > limit)
			return (EINVAL);

		table[i] = (u8)token;
		str_start = (str_end + 1);
	}

	return (0);
}

/**
 * ice_check_ets_bw - Check if ETS bw vals are valid
 * @table: eight values used for ETS bandwidth
 *
 * @return true if the sum of all 8 values in table
 * equals 100.
 */
static bool
ice_check_ets_bw(u8 *table)
{
	int sum = 0;
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++)
		sum += (int)table[i];

	return (sum == 100);
}

/**
 * ice_cfg_pba_num - Determine if PBA Number is retrievable
 * @sc: the device private softc structure
 *
 * Sets the feature flag for the existence of a PBA number
 * based on the success of the read command.  This does not
 * cache the result.
 */
void
ice_cfg_pba_num(struct ice_softc *sc)
{
	u8 pba_string[32] = "";

	if ((ice_is_bit_set(sc->feat_cap, ICE_FEATURE_HAS_PBA)) &&
	    (ice_read_pba_string(&sc->hw, pba_string, sizeof(pba_string)) == 0))
		ice_set_bit(ICE_FEATURE_HAS_PBA, sc->feat_en);
}

/**
 * ice_sysctl_query_port_ets - print Port ETS Config from AQ
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 */
static int
ice_sysctl_query_port_ets(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_aqc_port_ets_elem port_ets = { 0 };
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi;
	device_t dev = sc->dev;
	struct sbuf *sbuf;
	enum ice_status status;
	int i = 0;

	UNREFERENCED_PARAMETER(oidp);
	UNREFERENCED_PARAMETER(arg2);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	pi = hw->port_info;

	status = ice_aq_query_port_ets(pi, &port_ets, sizeof(port_ets), NULL);
	if (status != ICE_SUCCESS) {
		device_printf(dev,
		    "Query Port ETS AQ call failed, err %s aq_err %s\n",
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

	/* Do the actual printing */
	sbuf_printf(sbuf, "\n");

	sbuf_printf(sbuf, "Valid TC map: 0x%x\n", port_ets.tc_valid_bits);

	sbuf_printf(sbuf, "TC BW %%:");
	ice_for_each_traffic_class(i) {
		sbuf_printf(sbuf, " %3d", port_ets.tc_bw_share[i]);
	}
	sbuf_printf(sbuf, "\n");

	sbuf_printf(sbuf, "EIR profile ID: %d\n", port_ets.port_eir_prof_id);
	sbuf_printf(sbuf, "CIR profile ID: %d\n", port_ets.port_cir_prof_id);
	sbuf_printf(sbuf, "TC Node prio: 0x%x\n", port_ets.tc_node_prio);

	sbuf_printf(sbuf, "TC Node TEIDs:\n");
	ice_for_each_traffic_class(i) {
		sbuf_printf(sbuf, "%d: %d\n", i, port_ets.tc_node_teid[i]);
	}

	/* Finish */
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/**
 * ice_sysctl_dscp2tc_map - Map DSCP to hardware TCs
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: which eight DSCP to UP mappings to configure (0 - 7)
 * @req: sysctl request pointer
 *
 * Gets or sets the current DSCP to UP table cached by the driver. Since there
 * are 64 possible DSCP values to configure, this sysctl only configures
 * chunks of 8 in that space at a time.
 *
 * This sysctl is only relevant in DSCP mode, and will only function in SW DCB
 * mode.
 */
static int
ice_sysctl_dscp2tc_map(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_port_info *pi;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_status status;
	struct sbuf *sbuf;
	int ret;

	/* Store input rates from user */
	char dscp_user_buf[128] = "";
	u8 new_dscp_table_seg[ICE_MAX_TRAFFIC_CLASS] = {};

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	if (req->oldptr == NULL && req->newptr == NULL) {
		ret = SYSCTL_OUT(req, 0, 128);
		return (ret);
	}

	pi = hw->port_info;
	local_dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;

	sbuf = sbuf_new(NULL, dscp_user_buf, 128, SBUF_FIXEDLEN | SBUF_INCLUDENUL);

	/* Format DSCP-to-UP data for output */
	for (int i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		sbuf_printf(sbuf, "%d", local_dcbx_cfg->dscp_map[arg2 * 8 + i]);
		if (i != ICE_MAX_TRAFFIC_CLASS - 1)
			sbuf_printf(sbuf, ",");
	}

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	/* Read in the new DSCP mapping values */
	ret = sysctl_handle_string(oidp, dscp_user_buf, sizeof(dscp_user_buf), req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	/* Don't allow setting changes in FW DCB mode */
	if (!hw->port_info->qos_cfg.is_sw_lldp) {
		device_printf(dev, "%s: DSCP mapping is not allowed in FW DCBX mode\n",
		    __func__);
		return (EINVAL);
	}

	/* Convert 8 values in a string to a table; this is similar to what
	 * needs to be done for ETS settings, so this function can be re-used
	 * for that purpose.
	 */
	ret = ice_ets_str_to_tbl(dscp_user_buf, new_dscp_table_seg, 8);
	if (ret) {
		device_printf(dev, "%s: Could not parse input DSCP2TC table: %s\n",
		    __func__, dscp_user_buf);
		return (ret);
	}

	memcpy(&local_dcbx_cfg->dscp_map[arg2 * 8], new_dscp_table_seg,
	    sizeof(new_dscp_table_seg));

	local_dcbx_cfg->app_mode = ICE_DCBX_APPS_NON_WILLING;

	status = ice_set_dcb_cfg(pi);
	if (status) {
		device_printf(dev,
		    "%s: Failed to set DCB config; status %s, aq_err %s\n",
		    __func__, ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		return (EIO);
	}

	ice_do_dcb_reconfig(sc, false);

	return (0);
}

/**
 * ice_handle_debug_dump_ioctl - Handle a debug dump ioctl request
 * @sc: the device private softc
 * @ifd: ifdrv ioctl request pointer
 */
int
ice_handle_debug_dump_ioctl(struct ice_softc *sc, struct ifdrv *ifd)
{
	size_t ifd_len = ifd->ifd_len;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	struct ice_debug_dump_cmd *ddc;
	enum ice_status status;
	int err = 0;

	/* Returned arguments from the Admin Queue */
	u16 ret_buf_size = 0;
	u16 ret_next_table = 0;
	u32 ret_next_index = 0;

	/*
	 * ifioctl forwards SIOCxDRVSPEC to iflib without performing
	 * a privilege check. In turn, iflib forwards the ioctl to the driver
	 * without performing a privilege check. Perform one here to ensure
	 * that non-privileged threads cannot access this interface.
	 */
	err = priv_check(curthread, PRIV_DRIVER);
	if (err)
		return (err);

	if (ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET)) {
		device_printf(dev,
		    "%s: Driver must rebuild data structures after a reset. Operation aborted.\n",
		    __func__);
		return (EBUSY);
	}

	if (ifd_len < sizeof(*ddc)) {
		device_printf(dev,
		    "%s: ifdrv length is too small. Got %zu, but expected %zu\n",
		    __func__, ifd_len, sizeof(*ddc));
		return (EINVAL);
	}

	if (ifd->ifd_data == NULL) {
		device_printf(dev, "%s: ifd data buffer not present.\n",
		     __func__);
		return (EINVAL);
	}

	ddc = (struct ice_debug_dump_cmd *)malloc(ifd_len, M_ICE, M_ZERO | M_NOWAIT);
	if (!ddc)
		return (ENOMEM);

	/* Copy the NVM access command and data in from user space */
	/* coverity[tainted_data_argument] */
	err = copyin(ifd->ifd_data, ddc, ifd_len);
	if (err) {
		device_printf(dev, "%s: Copying request from user space failed, err %s\n",
			      __func__, ice_err_str(err));
		goto out;
	}

	/* The data_size arg must be at least 1 for the AQ cmd to work */
	if (ddc->data_size == 0) {
		device_printf(dev,
		    "%s: data_size must be greater than 0\n", __func__);
		err = EINVAL;
		goto out;
	}
	/* ...and it can't be too long */
	if (ddc->data_size > (ifd_len - sizeof(*ddc))) {
		device_printf(dev,
		    "%s: data_size (%d) is larger than ifd_len space (%zu)?\n", __func__,
		    ddc->data_size, ifd_len - sizeof(*ddc));
		err = EINVAL;
		goto out;
	}

	/* Make sure any possible data buffer space is zeroed */
	memset(ddc->data, 0, ifd_len - sizeof(*ddc));

	status = ice_aq_get_internal_data(hw, ddc->cluster_id, ddc->table_id, ddc->offset,
	    (u8 *)ddc->data, ddc->data_size, &ret_buf_size, &ret_next_table, &ret_next_index, NULL);
	ice_debug(hw, ICE_DBG_DIAG, "%s: ret_buf_size %d, ret_next_table %d, ret_next_index %d\n",
	    __func__, ret_buf_size, ret_next_table, ret_next_index);
	if (status) {
		device_printf(dev,
		    "%s: Get Internal Data AQ command failed, err %s aq_err %s\n",
		    __func__,
		    ice_status_str(status),
		    ice_aq_str(hw->adminq.sq_last_status));
		goto aq_error;
	}

	ddc->table_id = ret_next_table;
	ddc->offset = ret_next_index;
	ddc->data_size = ret_buf_size;

	/* Copy the possibly modified contents of the handled request out */
	err = copyout(ddc, ifd->ifd_data, ifd->ifd_len);
	if (err) {
		device_printf(dev, "%s: Copying response back to user space failed, err %s\n",
			      __func__, ice_err_str(err));
		goto out;
	}

aq_error:
	/* Convert private status to an error code for proper ioctl response */
	switch (status) {
	case ICE_SUCCESS:
		err = (0);
		break;
	case ICE_ERR_NO_MEMORY:
		err = (ENOMEM);
		break;
	case ICE_ERR_OUT_OF_RANGE:
		err = (ENOTTY);
		break;
	case ICE_ERR_AQ_ERROR:
		err = (EIO);
		break;
	case ICE_ERR_PARAM:
	default:
		err = (EINVAL);
		break;
	}

out:
	free(ddc, M_ICE);
	return (err);
}

/**
 * ice_sysctl_allow_no_fec_mod_in_auto - Change Auto FEC behavior
 * @oidp: sysctl oid structure
 * @arg1: pointer to private data structure
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Allows user to let "No FEC" mode to be used in "Auto"
 * FEC mode during FEC negotiation. This is only supported
 * on newer firmware versions.
 */
static int
ice_sysctl_allow_no_fec_mod_in_auto(SYSCTL_HANDLER_ARGS)
{
	struct ice_softc *sc = (struct ice_softc *)arg1;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u8 user_flag;
	int ret;

	UNREFERENCED_PARAMETER(arg2);

	ret = priv_check(curthread, PRIV_DRIVER);
	if (ret)
		return (ret);

	if (ice_driver_is_detaching(sc))
		return (ESHUTDOWN);

	user_flag = (u8)sc->allow_no_fec_mod_in_auto;

	ret = sysctl_handle_bool(oidp, &user_flag, 0, req);
	if ((ret) || (req->newptr == NULL))
		return (ret);

	if (!ice_fw_supports_fec_dis_auto(hw)) {
		log(LOG_INFO,
		    "%s: Enabling or disabling of auto configuration of modules that don't support FEC is unsupported by the current firmware\n",
		    device_get_nameunit(dev));
		return (ENODEV);
	}

	if (user_flag == (bool)sc->allow_no_fec_mod_in_auto)
		return (0);

	sc->allow_no_fec_mod_in_auto = (u8)user_flag;

	if (sc->allow_no_fec_mod_in_auto)
		log(LOG_INFO, "%s: Enabled auto configuration of No FEC modules\n",
		    device_get_nameunit(dev));
	else
		log(LOG_INFO,
		    "%s: Auto configuration of No FEC modules reset to NVM defaults\n",
		    device_get_nameunit(dev));

	return (0);
}

