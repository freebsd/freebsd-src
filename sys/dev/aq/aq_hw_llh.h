/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* File hw_atl_llh.h: Declarations of bitfield and register access functions for
 * Atlantic registers.
 */

#ifndef HW_ATL_LLH_H
#define HW_ATL_LLH_H

#include "aq_common.h"

struct aq_hw;

/* global */


void reg_glb_fw_image_id1_set(struct aq_hw* hw, u32 value);
u32 reg_glb_fw_image_id1_get(struct aq_hw* hw);

/* set global microprocessor semaphore */
void reg_glb_cpu_sem_set(struct aq_hw *aq_hw, u32 sem_value, u32 sem_index);

/* get global microprocessor semaphore */
u32 reg_glb_cpu_sem_get(struct aq_hw *aq_hw, u32 sem_index);

/*
*  \brief Get Global Standard Control 1
*  \return GlobalStandardControl1
*/
u32 reg_glb_standard_ctl1_get(struct aq_hw* hw);
/*
*  \brief Set Global Standard Control 1
*/
void reg_glb_standard_ctl1_set(struct aq_hw* hw, u32 glb_standard_ctl1);

/*
*  \brief Set Global Control 2
*/
void reg_global_ctl2_set(struct aq_hw* hw, u32 global_ctl2);
/*
*  \brief Get Global Control 2
*  \return GlobalControl2
*/
u32 reg_global_ctl2_get(struct aq_hw* hw);


/*
*  \brief Set Global Daisy Chain Status 1
*/
void reg_glb_daisy_chain_status1_set(struct aq_hw* hw, u32 glb_daisy_chain_status1);
/*
*  \brief Get Global Daisy Chain Status 1
*  \return glb_daisy_chain_status1
*/
u32 reg_glb_daisy_chain_status1_get(struct aq_hw* hw);


/*
*  \brief Set Global General Provisioning 9
*/
void reg_glb_general_provisioning9_set(struct aq_hw* hw, u32 value);
/*
*  \brief Get Global General Provisioning 9
*  \return GlobalGeneralProvisioning9
*/
u32 reg_glb_general_provisioning9_get(struct aq_hw* hw);

/*
*  \brief Set Global NVR Provisioning 2
*/
void reg_glb_nvr_provisioning2_set(struct aq_hw* hw, u32 value);
/*
*  \brief Get Global NVR Provisioning 2
*  \return GlobalNvrProvisioning2
*/
u32 reg_glb_nvr_provisioning2_get(struct aq_hw* hw);

/*
*  \brief Set Global NVR Interface 1
*/
void reg_glb_nvr_interface1_set(struct aq_hw* hw, u32 value);
/*
*  \brief Get Global NVR Interface 1
*  \return GlobalNvrInterface1
*/
u32 reg_glb_nvr_interface1_get(struct aq_hw* hw);


/* set global register reset disable */
void glb_glb_reg_res_dis_set(struct aq_hw *aq_hw, u32 glb_reg_res_dis);

/* set soft reset */
void glb_soft_res_set(struct aq_hw *aq_hw, u32 soft_res);

/* get soft reset */
u32 glb_soft_res_get(struct aq_hw *aq_hw);

/* stats */

u32 rpb_rx_dma_drop_pkt_cnt_get(struct aq_hw *aq_hw);

/* get rx dma good octet counter lsw */
u32 stats_rx_dma_good_octet_counterlsw_get(struct aq_hw *aq_hw);

/* get rx dma good packet counter lsw */
u32 stats_rx_dma_good_pkt_counterlsw_get(struct aq_hw *aq_hw);

/* get tx dma good octet counter lsw */
u32 stats_tx_dma_good_octet_counterlsw_get(struct aq_hw *aq_hw);

/* get tx dma good packet counter lsw */
u32 stats_tx_dma_good_pkt_counterlsw_get(struct aq_hw *aq_hw);

/* get rx dma good octet counter msw */
u32 stats_rx_dma_good_octet_countermsw_get(struct aq_hw *aq_hw);

/* get rx dma good packet counter msw */
u32 stats_rx_dma_good_pkt_countermsw_get(struct aq_hw *aq_hw);

/* get tx dma good octet counter msw */
u32 stats_tx_dma_good_octet_countermsw_get(struct aq_hw *aq_hw);

/* get tx dma good packet counter msw */
u32 stats_tx_dma_good_pkt_countermsw_get(struct aq_hw *aq_hw);

/* get  rx lro coalesced packet count lsw */
u32 stats_rx_lro_coalesced_pkt_count0_get(struct aq_hw *aq_hw);

/* get msm rx errors counter register */
u32 reg_mac_msm_rx_errs_cnt_get(struct aq_hw *aq_hw);

/* get msm rx unicast frames counter register */
u32 reg_mac_msm_rx_ucst_frm_cnt_get(struct aq_hw *aq_hw);

/* get msm rx multicast frames counter register */
u32 reg_mac_msm_rx_mcst_frm_cnt_get(struct aq_hw *aq_hw);

/* get msm rx broadcast frames counter register */
u32 reg_mac_msm_rx_bcst_frm_cnt_get(struct aq_hw *aq_hw);

/* get msm rx broadcast octets counter register 1 */
u32 reg_mac_msm_rx_bcst_octets_counter1get(struct aq_hw *aq_hw);

/* get msm rx unicast octets counter register 0 */
u32 reg_mac_msm_rx_ucst_octets_counter0get(struct aq_hw *aq_hw);

/* get rx dma statistics counter 7 */
u32 reg_rx_dma_stat_counter7get(struct aq_hw *aq_hw);

/* get msm tx errors counter register */
u32 reg_mac_msm_tx_errs_cnt_get(struct aq_hw *aq_hw);

/* get msm tx unicast frames counter register */
u32 reg_mac_msm_tx_ucst_frm_cnt_get(struct aq_hw *aq_hw);

/* get msm tx multicast frames counter register */
u32 reg_mac_msm_tx_mcst_frm_cnt_get(struct aq_hw *aq_hw);

/* get msm tx broadcast frames counter register */
u32 reg_mac_msm_tx_bcst_frm_cnt_get(struct aq_hw *aq_hw);

/* get msm tx multicast octets counter register 1 */
u32 reg_mac_msm_tx_mcst_octets_counter1get(struct aq_hw *aq_hw);

/* get msm tx broadcast octets counter register 1 */
u32 reg_mac_msm_tx_bcst_octets_counter1get(struct aq_hw *aq_hw);

/* get msm tx unicast octets counter register 0 */
u32 reg_mac_msm_tx_ucst_octets_counter0get(struct aq_hw *aq_hw);

/* get global mif identification */
u32 reg_glb_mif_id_get(struct aq_hw *aq_hw);

/** \brief Set Tx Register Reset Disable
*   \param txRegisterResetDisable 1 = Disable the S/W reset to MAC-PHY registers, 0 = Enable the S/W reset to MAC-PHY registers
*   \note Default value: 0x1
*   \note PORT="pif_mpi_reg_reset_dsbl_i"
*/
void mpi_tx_reg_res_dis_set(struct aq_hw* hw, u32 mpi_tx_reg_res_dis);
/** \brief Get Tx Register Reset Disable
*   \return 1 = Disable the S/W reset to MAC-PHY registers, 0 = Enable the S/W reset to MAC-PHY registers
*   \note Default value: 0x1
*   \note PORT="pif_mpi_reg_reset_dsbl_i"
*/
u32 mpi_tx_reg_res_dis_get(struct aq_hw* hw);


/* interrupt */

/* set interrupt auto mask lsw */
void itr_irq_auto_masklsw_set(struct aq_hw *aq_hw, u32 irq_auto_masklsw);

/* set interrupt mapping enable rx */
void itr_irq_map_en_rx_set(struct aq_hw *aq_hw, u32 irq_map_en_rx, u32 rx);

/* set interrupt mapping enable tx */
void itr_irq_map_en_tx_set(struct aq_hw *aq_hw, u32 irq_map_en_tx, u32 tx);

/* set interrupt mapping rx */
void itr_irq_map_rx_set(struct aq_hw *aq_hw, u32 irq_map_rx, u32 rx);

/* set interrupt mapping tx */
void itr_irq_map_tx_set(struct aq_hw *aq_hw, u32 irq_map_tx, u32 tx);

/* set interrupt mask clear lsw */
void itr_irq_msk_clearlsw_set(struct aq_hw *aq_hw, u32 irq_msk_clearlsw);

/* set interrupt mask set lsw */
void itr_irq_msk_setlsw_set(struct aq_hw *aq_hw, u32 irq_msk_setlsw);

/* set interrupt register reset disable */
void itr_irq_reg_res_dis_set(struct aq_hw *aq_hw, u32 irq_reg_res_dis);

/* set interrupt status clear lsw */
void itr_irq_status_clearlsw_set(struct aq_hw *aq_hw,
                 u32 irq_status_clearlsw);

/* get interrupt status lsw */
u32 itr_irq_statuslsw_get(struct aq_hw *aq_hw);

/* get reset interrupt */
u32 itr_res_irq_get(struct aq_hw *aq_hw);

/* set reset interrupt */
void itr_res_irq_set(struct aq_hw *aq_hw, u32 res_irq);

void itr_irq_mode_set(struct aq_hw *aq_hw, u32 irq_mode);

/* Set Link Interrupt Mapping Enable */
void itr_link_int_map_en_set(struct aq_hw *aq_hw, u32 link_int_en_map_en);

/* Get Link Interrupt Mapping Enable */
u32 itr_link_int_map_en_get(struct aq_hw *aq_hw);

/* Set Link Interrupt Mapping */
void itr_link_int_map_set(struct aq_hw *aq_hw, u32 link_int_map);

/* Get Link Interrupt Mapping */
u32 itr_link_int_map_get(struct aq_hw *aq_hw);


/* Set MIF Interrupt Mapping Enable */
void itr_mif_int_map_en_set(struct aq_hw *aq_hw, u32 mif_int_map_en, u32 mif);

/* Get MIF Interrupt Mapping Enable */
u32 itr_mif_int_map_en_get(struct aq_hw *aq_hw, u32 mif);

/* Set MIF Interrupt Mapping */
void itr_mif_int_map_set(struct aq_hw *aq_hw, u32 mif_int_map, u32 mif);

/* Get MIF Interrupt Mapping */
u32 itr_mif_int_map_get(struct aq_hw *aq_hw, u32 mif);

void itr_irq_status_cor_en_set(struct aq_hw *aq_hw, u32 irq_status_cor_enable);

void itr_irq_auto_mask_clr_en_set(struct aq_hw *aq_hw, u32 irq_auto_mask_clr_en);

/* rdm */

/* set cpu id */
void rdm_cpu_id_set(struct aq_hw *aq_hw, u32 cpuid, u32 dca);

/* set rx dca enable */
void rdm_rx_dca_en_set(struct aq_hw *aq_hw, u32 rx_dca_en);

/* set rx dca mode */
void rdm_rx_dca_mode_set(struct aq_hw *aq_hw, u32 rx_dca_mode);

/* set rx descriptor data buffer size */
void rdm_rx_desc_data_buff_size_set(struct aq_hw *aq_hw,
                    u32 rx_desc_data_buff_size,
                    u32 descriptor);

/* set rx descriptor dca enable */
void rdm_rx_desc_dca_en_set(struct aq_hw *aq_hw, u32 rx_desc_dca_en,
                u32 dca);

/* set rx descriptor enable */
void rdm_rx_desc_en_set(struct aq_hw *aq_hw, u32 rx_desc_en,
            u32 descriptor);

/* set rx descriptor header splitting */
void rdm_rx_desc_head_splitting_set(struct aq_hw *aq_hw,
                    u32 rx_desc_head_splitting,
                    u32 descriptor);

/* get rx descriptor head pointer */
u32 rdm_rx_desc_head_ptr_get(struct aq_hw *aq_hw, u32 descriptor);

/* set rx descriptor length */
void rdm_rx_desc_len_set(struct aq_hw *aq_hw, u32 rx_desc_len,
             u32 descriptor);

/* set rx descriptor write-back interrupt enable */
void rdm_rx_desc_wr_wb_irq_en_set(struct aq_hw *aq_hw,
                  u32 rx_desc_wr_wb_irq_en);

/* set rx header dca enable */
void rdm_rx_head_dca_en_set(struct aq_hw *aq_hw, u32 rx_head_dca_en,
                u32 dca);

/* set rx payload dca enable */
void rdm_rx_pld_dca_en_set(struct aq_hw *aq_hw, u32 rx_pld_dca_en, u32 dca);

/* set rx descriptor header buffer size */
void rdm_rx_desc_head_buff_size_set(struct aq_hw *aq_hw,
                    u32 rx_desc_head_buff_size,
                    u32 descriptor);

/* set rx descriptor reset */
void rdm_rx_desc_res_set(struct aq_hw *aq_hw, u32 rx_desc_res,
             u32 descriptor);

/* Set RDM Interrupt Moderation Enable */
void rdm_rdm_intr_moder_en_set(struct aq_hw *aq_hw, u32 rdm_intr_moder_en);

/* reg */

/* set general interrupt mapping register */
void reg_gen_irq_map_set(struct aq_hw *aq_hw, u32 gen_intr_map, u32 regidx);

/* get general interrupt status register */
u32 reg_gen_irq_status_get(struct aq_hw *aq_hw);

/* set interrupt global control register */
void reg_irq_glb_ctl_set(struct aq_hw *aq_hw, u32 intr_glb_ctl);

/* set interrupt throttle register */
void reg_irq_thr_set(struct aq_hw *aq_hw, u32 intr_thr, u32 throttle);

/* set rx dma descriptor base address lsw */
void reg_rx_dma_desc_base_addresslswset(struct aq_hw *aq_hw,
                    u32 rx_dma_desc_base_addrlsw,
                    u32 descriptor);

/* set rx dma descriptor base address msw */
void reg_rx_dma_desc_base_addressmswset(struct aq_hw *aq_hw,
                    u32 rx_dma_desc_base_addrmsw,
                    u32 descriptor);

/* get rx dma descriptor status register */
u32 reg_rx_dma_desc_status_get(struct aq_hw *aq_hw, u32 descriptor);

/* set rx dma descriptor tail pointer register */
void reg_rx_dma_desc_tail_ptr_set(struct aq_hw *aq_hw,
                  u32 rx_dma_desc_tail_ptr,
                  u32 descriptor);
/* get rx dma descriptor tail pointer register */
u32 reg_rx_dma_desc_tail_ptr_get(struct aq_hw *aq_hw, u32 descriptor);

/* set rx filter multicast filter mask register */
void reg_rx_flr_mcst_flr_msk_set(struct aq_hw *aq_hw,
                 u32 rx_flr_mcst_flr_msk);

/* set rx filter multicast filter register */
void reg_rx_flr_mcst_flr_set(struct aq_hw *aq_hw, u32 rx_flr_mcst_flr,
                 u32 filter);

/* set rx filter rss control register 1 */
void reg_rx_flr_rss_control1set(struct aq_hw *aq_hw,
                u32 rx_flr_rss_control1);

/* Set RX Filter Control Register 2 */
void reg_rx_flr_control2_set(struct aq_hw *aq_hw, u32 rx_flr_control2);

/* Set RX Interrupt Moderation Control Register */
void reg_rx_intr_moder_ctrl_set(struct aq_hw *aq_hw,
                u32 rx_intr_moderation_ctl,
                u32 queue);

/* set tx dma debug control */
void reg_tx_dma_debug_ctl_set(struct aq_hw *aq_hw, u32 tx_dma_debug_ctl);

/* set tx dma descriptor base address lsw */
void reg_tx_dma_desc_base_addresslswset(struct aq_hw *aq_hw,
                    u32 tx_dma_desc_base_addrlsw,
                    u32 descriptor);

/* set tx dma descriptor base address msw */
void reg_tx_dma_desc_base_addressmswset(struct aq_hw *aq_hw,
                    u32 tx_dma_desc_base_addrmsw,
                    u32 descriptor);

/* set tx dma descriptor tail pointer register */
void reg_tx_dma_desc_tail_ptr_set(struct aq_hw *aq_hw,
                  u32 tx_dma_desc_tail_ptr,
                  u32 descriptor);

/* get tx dma descriptor tail pointer register */
u32 reg_tx_dma_desc_tail_ptr_get(struct aq_hw *aq_hw, u32 descriptor);

/* Set TX Interrupt Moderation Control Register */
void reg_tx_intr_moder_ctrl_set(struct aq_hw *aq_hw,
                u32 tx_intr_moderation_ctl,
                u32 queue);

/* get global microprocessor scratch pad */
u32 reg_glb_cpu_scratch_scp_get(struct aq_hw *hw, u32 glb_cpu_scratch_scp_idx);
/* set global microprocessor scratch pad */
void reg_glb_cpu_scratch_scp_set(struct aq_hw *aq_hw,
    u32 glb_cpu_scratch_scp, u32 scratch_scp);

/* get global microprocessor no reset scratch pad */
u32 reg_glb_cpu_no_reset_scratchpad_get(struct aq_hw* hw, u32 index);
/* set global microprocessor no reset scratch pad */
void reg_glb_cpu_no_reset_scratchpad_set(struct aq_hw* aq_hw, u32 value,
    u32 index);

/* rpb */

/* set dma system loopback */
void rpb_dma_sys_lbk_set(struct aq_hw *aq_hw, u32 dma_sys_lbk);

/* set rx traffic class mode */
void rpb_rpf_rx_traf_class_mode_set(struct aq_hw *aq_hw,
                    u32 rx_traf_class_mode);

/* set rx buffer enable */
void rpb_rx_buff_en_set(struct aq_hw *aq_hw, u32 rx_buff_en);

/* set rx buffer high threshold (per tc) */
void rpb_rx_buff_hi_threshold_per_tc_set(struct aq_hw *aq_hw,
                     u32 rx_buff_hi_threshold_per_tc,
                     u32 buffer);

/* set rx buffer low threshold (per tc) */
void rpb_rx_buff_lo_threshold_per_tc_set(struct aq_hw *aq_hw,
                     u32 rx_buff_lo_threshold_per_tc,
                     u32 buffer);

/* set rx flow control mode */
void rpb_rx_flow_ctl_mode_set(struct aq_hw *aq_hw, u32 rx_flow_ctl_mode);

/* set rx packet buffer size (per tc) */
void rpb_rx_pkt_buff_size_per_tc_set(struct aq_hw *aq_hw,
                     u32 rx_pkt_buff_size_per_tc,
                     u32 buffer);

/* set rx xoff enable (per tc) */
void rpb_rx_xoff_en_per_tc_set(struct aq_hw *aq_hw, u32 rx_xoff_en_per_tc,
                   u32 buffer);

/* rpf */

/* set l2 broadcast count threshold */
void rpfl2broadcast_count_threshold_set(struct aq_hw *aq_hw,
                    u32 l2broadcast_count_threshold);

/* set l2 broadcast enable */
void rpfl2broadcast_en_set(struct aq_hw *aq_hw, u32 l2broadcast_en);

/* set l2 broadcast filter action */
void rpfl2broadcast_flr_act_set(struct aq_hw *aq_hw,
                u32 l2broadcast_flr_act);

/* set l2 multicast filter enable */
void rpfl2multicast_flr_en_set(struct aq_hw *aq_hw, u32 l2multicast_flr_en,
                   u32 filter);

/* set l2 promiscuous mode enable */
void rpfl2promiscuous_mode_en_set(struct aq_hw *aq_hw,
                  u32 l2promiscuous_mode_en);

/* set l2 unicast filter action */
void rpfl2unicast_flr_act_set(struct aq_hw *aq_hw, u32 l2unicast_flr_act,
                  u32 filter);

/* set l2 unicast filter enable */
void rpfl2_uc_flr_en_set(struct aq_hw *aq_hw, u32 l2unicast_flr_en,
             u32 filter);

/* set l2 unicast destination address lsw */
void rpfl2unicast_dest_addresslsw_set(struct aq_hw *aq_hw,
                      u32 l2unicast_dest_addresslsw,
                      u32 filter);

/* set l2 unicast destination address msw */
void rpfl2unicast_dest_addressmsw_set(struct aq_hw *aq_hw,
                      u32 l2unicast_dest_addressmsw,
                      u32 filter);

/* Set L2 Accept all Multicast packets */
void rpfl2_accept_all_mc_packets_set(struct aq_hw *aq_hw,
                     u32 l2_accept_all_mc_packets);

/* set user-priority tc mapping */
void rpf_rpb_user_priority_tc_map_set(struct aq_hw *aq_hw,
                      u32 user_priority_tc_map, u32 tc);

/* set rss key address */
void rpf_rss_key_addr_set(struct aq_hw *aq_hw, u32 rss_key_addr);

/* set rss key write data */
void rpf_rss_key_wr_data_set(struct aq_hw *aq_hw, u32 rss_key_wr_data);

/* get rss key read data */
u32 rpf_rss_key_rd_data_get(struct aq_hw *aq_hw);

/* get rss key write enable */
u32 rpf_rss_key_wr_en_get(struct aq_hw *aq_hw);

/* set rss key write enable */
void rpf_rss_key_wr_en_set(struct aq_hw *aq_hw, u32 rss_key_wr_en);

/* set rss redirection table address */
void rpf_rss_redir_tbl_addr_set(struct aq_hw *aq_hw,
                u32 rss_redir_tbl_addr);

/* set rss redirection table write data */
void rpf_rss_redir_tbl_wr_data_set(struct aq_hw *aq_hw,
                   u32 rss_redir_tbl_wr_data);

/* get rss redirection write enable */
u32 rpf_rss_redir_wr_en_get(struct aq_hw *aq_hw);

/* set rss redirection write enable */
void rpf_rss_redir_wr_en_set(struct aq_hw *aq_hw, u32 rss_redir_wr_en);

/* set tpo to rpf system loopback */
void rpf_tpo_to_rpf_sys_lbk_set(struct aq_hw *aq_hw,
                u32 tpo_to_rpf_sys_lbk);

/* set vlan inner ethertype */
void hw_atl_rpf_vlan_inner_etht_set(struct aq_hw *aq_hw, u32 vlan_inner_etht);

/* set vlan outer ethertype */
void hw_atl_rpf_vlan_outer_etht_set(struct aq_hw *aq_hw, u32 vlan_outer_etht);

/* set vlan promiscuous mode enable */
void hw_atl_rpf_vlan_prom_mode_en_set(struct aq_hw *aq_hw,
				      u32 vlan_prom_mode_en);

/* Set VLAN untagged action */
void hw_atl_rpf_vlan_untagged_act_set(struct aq_hw *aq_hw,
				      u32 vlan_untagged_act);

/* Set VLAN accept untagged packets */
void hw_atl_rpf_vlan_accept_untagged_packets_set(struct aq_hw *aq_hw,
						 u32 vlan_acc_untagged_packets);

/* Set VLAN filter enable */
void hw_atl_rpf_vlan_flr_en_set(struct aq_hw *aq_hw, u32 vlan_flr_en,
				u32 filter);

/* Set VLAN Filter Action */
void hw_atl_rpf_vlan_flr_act_set(struct aq_hw *aq_hw, u32 vlan_filter_act,
				 u32 filter);

/* Set VLAN ID Filter */
void hw_atl_rpf_vlan_id_flr_set(struct aq_hw *aq_hw, u32 vlan_id_flr,
				u32 filter);

/* Set VLAN RX queue assignment enable */
void hw_atl_rpf_vlan_rxq_en_flr_set(struct aq_hw *aq_hw, u32 vlan_rxq_en,
				u32 filter);

/* Set VLAN RX queue */
void hw_atl_rpf_vlan_rxq_flr_set(struct aq_hw *aq_hw, u32 vlan_rxq,
				u32 filter);

/* set ethertype filter enable */
void hw_atl_rpf_etht_flr_en_set(struct aq_hw *aq_hw, u32 etht_flr_en,
				u32 filter);

/* set  ethertype user-priority enable */
void hw_atl_rpf_etht_user_priority_en_set(struct aq_hw *aq_hw,
					  u32 etht_user_priority_en,
					  u32 filter);

/* set  ethertype rx queue enable */
void hw_atl_rpf_etht_rx_queue_en_set(struct aq_hw *aq_hw,
				     u32 etht_rx_queue_en,
				     u32 filter);

/* set ethertype rx queue */
void hw_atl_rpf_etht_rx_queue_set(struct aq_hw *aq_hw, u32 etht_rx_queue,
				  u32 filter);

/* set ethertype user-priority */
void hw_atl_rpf_etht_user_priority_set(struct aq_hw *aq_hw,
				       u32 etht_user_priority,
				       u32 filter);

/* set ethertype management queue */
void hw_atl_rpf_etht_mgt_queue_set(struct aq_hw *aq_hw, u32 etht_mgt_queue,
				   u32 filter);

/* set ethertype filter action */
void hw_atl_rpf_etht_flr_act_set(struct aq_hw *aq_hw, u32 etht_flr_act,
				 u32 filter);

/* set ethertype filter */
void hw_atl_rpf_etht_flr_set(struct aq_hw *aq_hw, u32 etht_flr, u32 filter);

/* set L3/L4 filter enable */
void hw_atl_rpf_l3_l4_enf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 IPv6 enable */
void hw_atl_rpf_l3_v6_enf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 source address enable */
void hw_atl_rpf_l3_saf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 destination address enable */
void hw_atl_rpf_l3_daf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 source port enable */
void hw_atl_rpf_l4_spf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 destination port enable */
void hw_atl_rpf_l4_dpf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 protocol enable */
void hw_atl_rpf_l4_protf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 ARP filter enable */
void hw_atl_rpf_l3_arpf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 rx queue enable */
void hw_atl_rpf_l3_l4_rxqf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 management queue */
void hw_atl_rpf_l3_l4_mng_rxqf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 filter action */
void hw_atl_rpf_l3_l4_actf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 rx queue */
void hw_atl_rpf_l3_l4_rxqf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 protocol value */
void hw_atl_rpf_l4_protf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 source port */
void hw_atl_rpf_l4_spd_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 destination port */
void hw_atl_rpf_l4_dpd_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set vlan inner ethertype */
void rpf_vlan_inner_etht_set(struct aq_hw *aq_hw, u32 vlan_inner_etht);

/* set vlan outer ethertype */
void rpf_vlan_outer_etht_set(struct aq_hw *aq_hw, u32 vlan_outer_etht);

/* set vlan promiscuous mode enable */
void rpf_vlan_prom_mode_en_set(struct aq_hw *aq_hw, u32 vlan_prom_mode_en);

/* Set VLAN untagged action */
void rpf_vlan_untagged_act_set(struct aq_hw *aq_hw, u32 vlan_untagged_act);

/* Set VLAN accept untagged packets */
void rpf_vlan_accept_untagged_packets_set(struct aq_hw *aq_hw,
                      u32 vlan_accept_untagged_packets);

/* Set VLAN filter enable */
void rpf_vlan_flr_en_set(struct aq_hw *aq_hw, u32 vlan_flr_en, u32 filter);

/* Set VLAN Filter Action */
void rpf_vlan_flr_act_set(struct aq_hw *aq_hw, u32 vlan_filter_act,
              u32 filter);

/* Set VLAN ID Filter */
void rpf_vlan_id_flr_set(struct aq_hw *aq_hw, u32 vlan_id_flr, u32 filter);

/* set ethertype filter enable */
void rpf_etht_flr_en_set(struct aq_hw *aq_hw, u32 etht_flr_en, u32 filter);

/* set  ethertype user-priority enable */
void rpf_etht_user_priority_en_set(struct aq_hw *aq_hw,
                   u32 etht_user_priority_en, u32 filter);

/* set  ethertype rx queue enable */
void rpf_etht_rx_queue_en_set(struct aq_hw *aq_hw, u32 etht_rx_queue_en,
                  u32 filter);

/* set ethertype rx queue */
void rpf_etht_rx_queue_set(struct aq_hw *aq_hw, u32 etht_rx_queue,
               u32 filter);

/* set ethertype user-priority */
void rpf_etht_user_priority_set(struct aq_hw *aq_hw, u32 etht_user_priority,
                u32 filter);

/* set ethertype management queue */
void rpf_etht_mgt_queue_set(struct aq_hw *aq_hw, u32 etht_mgt_queue,
                u32 filter);

/* set ethertype filter action */
void rpf_etht_flr_act_set(struct aq_hw *aq_hw, u32 etht_flr_act,
              u32 filter);

/* set ethertype filter */
void rpf_etht_flr_set(struct aq_hw *aq_hw, u32 etht_flr, u32 filter);

/* set L3/L4 filter enable */
void hw_atl_rpf_l3_l4_enf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 IPv6 enable */
void hw_atl_rpf_l3_v6_enf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 source address enable */
void hw_atl_rpf_l3_saf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 destination address enable */
void hw_atl_rpf_l3_daf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 source port enable */
void hw_atl_rpf_l4_spf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 destination port enable */
void hw_atl_rpf_l4_dpf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 protocol enable */
void hw_atl_rpf_l4_protf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 ARP filter enable */
void hw_atl_rpf_l3_arpf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 rx queue enable */
void hw_atl_rpf_l3_l4_rxqf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 management queue */
void hw_atl_rpf_l3_l4_mng_rxqf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 filter action */
void hw_atl_rpf_l3_l4_actf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 rx queue */
void hw_atl_rpf_l3_l4_rxqf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 protocol value */
void hw_atl_rpf_l4_protf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 source port */
void hw_atl_rpf_l4_spd_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 destination port */
void hw_atl_rpf_l4_dpd_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* rpo */

/* set ipv4 header checksum offload enable */
void rpo_ipv4header_crc_offload_en_set(struct aq_hw *aq_hw,
                       u32 ipv4header_crc_offload_en);

/* set rx descriptor vlan stripping */
void rpo_rx_desc_vlan_stripping_set(struct aq_hw *aq_hw,
                    u32 rx_desc_vlan_stripping,
                    u32 descriptor);

/* set tcp/udp checksum offload enable */
void rpo_tcp_udp_crc_offload_en_set(struct aq_hw *aq_hw,
                    u32 tcp_udp_crc_offload_en);

/* Set LRO Patch Optimization Enable. */
void rpo_lro_patch_optimization_en_set(struct aq_hw *aq_hw,
                       u32 lro_patch_optimization_en);

/* Set Large Receive Offload Enable */
void rpo_lro_en_set(struct aq_hw *aq_hw, u32 lro_en);

/* Set LRO Q Sessions Limit */
void rpo_lro_qsessions_lim_set(struct aq_hw *aq_hw, u32 lro_qsessions_lim);

/* Set LRO Total Descriptor Limit */
void rpo_lro_total_desc_lim_set(struct aq_hw *aq_hw, u32 lro_total_desc_lim);

/* Set LRO Min Payload of First Packet */
void rpo_lro_min_pay_of_first_pkt_set(struct aq_hw *aq_hw,
                      u32 lro_min_pld_of_first_pkt);

/* Set LRO Packet Limit */
void rpo_lro_pkt_lim_set(struct aq_hw *aq_hw, u32 lro_packet_lim);

/* Set LRO Max Number of Descriptors */
void rpo_lro_max_num_of_descriptors_set(struct aq_hw *aq_hw,
                    u32 lro_max_desc_num, u32 lro);

/* Set LRO Time Base Divider */
void rpo_lro_time_base_divider_set(struct aq_hw *aq_hw,
                   u32 lro_time_base_divider);

/*Set LRO Inactive Interval */
void rpo_lro_inactive_interval_set(struct aq_hw *aq_hw,
                   u32 lro_inactive_interval);

/*Set LRO Max Coalescing Interval */
void rpo_lro_max_coalescing_interval_set(struct aq_hw *aq_hw,
                     u32 lro_max_coalescing_interval);

/* rx */

/* set rx register reset disable */
void rx_rx_reg_res_dis_set(struct aq_hw *aq_hw, u32 rx_reg_res_dis);

/* tdm */

/* set cpu id */
void tdm_cpu_id_set(struct aq_hw *aq_hw, u32 cpuid, u32 dca);

/* set large send offload enable */
void tdm_large_send_offload_en_set(struct aq_hw *aq_hw,
                   u32 large_send_offload_en);

/* set tx descriptor enable */
void tdm_tx_desc_en_set(struct aq_hw *aq_hw, u32 tx_desc_en, u32 descriptor);

/* set tx dca enable */
void tdm_tx_dca_en_set(struct aq_hw *aq_hw, u32 tx_dca_en);

/* set tx dca mode */
void tdm_tx_dca_mode_set(struct aq_hw *aq_hw, u32 tx_dca_mode);

/* set tx descriptor dca enable */
void tdm_tx_desc_dca_en_set(struct aq_hw *aq_hw, u32 tx_desc_dca_en, u32 dca);

/* get tx descriptor head pointer */
u32 tdm_tx_desc_head_ptr_get(struct aq_hw *aq_hw, u32 descriptor);

/* set tx descriptor length */
void tdm_tx_desc_len_set(struct aq_hw *aq_hw, u32 tx_desc_len,
             u32 descriptor);

/* set tx descriptor write-back interrupt enable */
void tdm_tx_desc_wr_wb_irq_en_set(struct aq_hw *aq_hw,
                  u32 tx_desc_wr_wb_irq_en);

/* set tx descriptor write-back threshold */
void tdm_tx_desc_wr_wb_threshold_set(struct aq_hw *aq_hw,
                     u32 tx_desc_wr_wb_threshold,
                     u32 descriptor);

/* Set TDM Interrupt Moderation Enable */
void tdm_tdm_intr_moder_en_set(struct aq_hw *aq_hw,
                   u32 tdm_irq_moderation_en);
/* thm */

/* set lso tcp flag of first packet */
void thm_lso_tcp_flag_of_first_pkt_set(struct aq_hw *aq_hw,
                       u32 lso_tcp_flag_of_first_pkt);

/* set lso tcp flag of last packet */
void thm_lso_tcp_flag_of_last_pkt_set(struct aq_hw *aq_hw,
                      u32 lso_tcp_flag_of_last_pkt);

/* set lso tcp flag of middle packet */
void thm_lso_tcp_flag_of_middle_pkt_set(struct aq_hw *aq_hw,
                    u32 lso_tcp_flag_of_middle_pkt);

/* tpb */

/* set tx buffer enable */
void tpb_tx_buff_en_set(struct aq_hw *aq_hw, u32 tx_buff_en);

/* set tx tc mode */
void tpb_tx_tc_mode_set(struct aq_hw *aq_hw, u32 tc_mode);

/* set tx buffer high threshold (per tc) */
void tpb_tx_buff_hi_threshold_per_tc_set(struct aq_hw *aq_hw,
                     u32 tx_buff_hi_threshold_per_tc,
                     u32 buffer);

/* set tx buffer low threshold (per tc) */
void tpb_tx_buff_lo_threshold_per_tc_set(struct aq_hw *aq_hw,
                     u32 tx_buff_lo_threshold_per_tc,
                     u32 buffer);

/* set tx dma system loopback enable */
void tpb_tx_dma_sys_lbk_en_set(struct aq_hw *aq_hw, u32 tx_dma_sys_lbk_en);

/* set tx packet buffer size (per tc) */
void tpb_tx_pkt_buff_size_per_tc_set(struct aq_hw *aq_hw,
                     u32 tx_pkt_buff_size_per_tc, u32 buffer);

/* toggle rdm rx dma descriptor cache init */
void rdm_rx_dma_desc_cache_init_tgl(struct aq_hw *aq_hw);

/* set tx path pad insert enable */
void tpb_tx_path_scp_ins_en_set(struct aq_hw *aq_hw, u32 tx_path_scp_ins_en);

/* tpo */

/* set ipv4 header checksum offload enable */
void tpo_ipv4header_crc_offload_en_set(struct aq_hw *aq_hw,
                       u32 ipv4header_crc_offload_en);

/* set tcp/udp checksum offload enable */
void tpo_tcp_udp_crc_offload_en_set(struct aq_hw *aq_hw,
                    u32 tcp_udp_crc_offload_en);

/* set tx pkt system loopback enable */
void tpo_tx_pkt_sys_lbk_en_set(struct aq_hw *aq_hw, u32 tx_pkt_sys_lbk_en);

/* tps */

/* set tx packet scheduler data arbitration mode */
void tps_tx_pkt_shed_data_arb_mode_set(struct aq_hw *aq_hw,
                       u32 tx_pkt_shed_data_arb_mode);

/* set tx packet scheduler descriptor rate current time reset */
void tps_tx_pkt_shed_desc_rate_curr_time_res_set(struct aq_hw *aq_hw,
                         u32 curr_time_res);

/* set tx packet scheduler descriptor rate limit */
void tps_tx_pkt_shed_desc_rate_lim_set(struct aq_hw *aq_hw,
                       u32 tx_pkt_shed_desc_rate_lim);

/* set tx packet scheduler descriptor tc arbitration mode */
void tps_tx_pkt_shed_desc_tc_arb_mode_set(struct aq_hw *aq_hw,
                      u32 tx_pkt_shed_desc_tc_arb_mode);

/* set tx packet scheduler descriptor tc max credit */
void tps_tx_pkt_shed_desc_tc_max_credit_set(struct aq_hw *aq_hw,
                        u32 tx_pkt_shed_desc_tc_max_credit,
                        u32 tc);

/* set tx packet scheduler descriptor tc weight */
void tps_tx_pkt_shed_desc_tc_weight_set(struct aq_hw *aq_hw,
                    u32 tx_pkt_shed_desc_tc_weight,
                    u32 tc);

/* set tx packet scheduler descriptor vm arbitration mode */
void tps_tx_pkt_shed_desc_vm_arb_mode_set(struct aq_hw *aq_hw,
                      u32 tx_pkt_shed_desc_vm_arb_mode);

/* set tx packet scheduler tc data max credit */
void tps_tx_pkt_shed_tc_data_max_credit_set(struct aq_hw *aq_hw,
                        u32 tx_pkt_shed_tc_data_max_credit,
                        u32 tc);

/* set tx packet scheduler tc data weight */
void tps_tx_pkt_shed_tc_data_weight_set(struct aq_hw *aq_hw,
                    u32 tx_pkt_shed_tc_data_weight,
                    u32 tc);

/* tx */

/* set tx register reset disable */
void tx_tx_reg_res_dis_set(struct aq_hw *aq_hw, u32 tx_reg_res_dis);

/* msm */

/* get register access status */
u32 msm_reg_access_status_get(struct aq_hw *aq_hw);

/* set  register address for indirect address */
void msm_reg_addr_for_indirect_addr_set(struct aq_hw *aq_hw,
                    u32 reg_addr_for_indirect_addr);

/* set register read strobe */
void msm_reg_rd_strobe_set(struct aq_hw *aq_hw, u32 reg_rd_strobe);

/* get  register read data */
u32 msm_reg_rd_data_get(struct aq_hw *aq_hw);

/* set  register write data */
void msm_reg_wr_data_set(struct aq_hw *aq_hw, u32 reg_wr_data);

/* set register write strobe */
void msm_reg_wr_strobe_set(struct aq_hw *aq_hw, u32 reg_wr_strobe);

/* pci */

/* set pci register reset disable */
void pci_pci_reg_res_dis_set(struct aq_hw *aq_hw, u32 pci_reg_res_dis);


/*
*  \brief Set MIF Power Gating Enable Control
*/
void reg_mif_power_gating_enable_control_set(struct aq_hw* hw, u32 value);
/*
*  \brief Get MIF Power Gating Enable Control
*  \return MifPowerGatingEnableControl
*/
u32 reg_mif_power_gating_enable_control_get(struct aq_hw* hw);

/* get mif up mailbox busy */
u32 mif_mcp_up_mailbox_busy_get(struct aq_hw *aq_hw);

/* set mif up mailbox execute operation */
void mif_mcp_up_mailbox_execute_operation_set(struct aq_hw* hw, u32 value);

/* get mif uP mailbox address */
u32 mif_mcp_up_mailbox_addr_get(struct aq_hw *aq_hw);
/* set mif uP mailbox address */
void mif_mcp_up_mailbox_addr_set(struct aq_hw *hw, u32 value);

/* get mif uP mailbox data */
u32 mif_mcp_up_mailbox_data_get(struct aq_hw *aq_hw);

/* clear ipv4 filter destination address */
void hw_atl_rpfl3l4_ipv4_dest_addr_clear(struct aq_hw *aq_hw, u8 location);

/* clear ipv4 filter source address */
void hw_atl_rpfl3l4_ipv4_src_addr_clear(struct aq_hw *aq_hw, u8 location);

/* clear command for filter l3-l4 */
void hw_atl_rpfl3l4_cmd_clear(struct aq_hw *aq_hw, u8 location);

/* clear ipv6 filter destination address */
void hw_atl_rpfl3l4_ipv6_dest_addr_clear(struct aq_hw *aq_hw, u8 location);

/* clear ipv6 filter source address */
void hw_atl_rpfl3l4_ipv6_src_addr_clear(struct aq_hw *aq_hw, u8 location);

/* set ipv4 filter destination address */
void hw_atl_rpfl3l4_ipv4_dest_addr_set(struct aq_hw *aq_hw, u8 location,
				       u32 ipv4_dest);

/* set ipv4 filter source address */
void hw_atl_rpfl3l4_ipv4_src_addr_set(struct aq_hw *aq_hw, u8 location,
				      u32 ipv4_src);

/* set command for filter l3-l4 */
void hw_atl_rpfl3l4_cmd_set(struct aq_hw *aq_hw, u8 location, u32 cmd);

/* set ipv6 filter source address */
void hw_atl_rpfl3l4_ipv6_src_addr_set(struct aq_hw *aq_hw, u8 location,
				      u32 *ipv6_src);

/* set ipv6 filter destination address */
void hw_atl_rpfl3l4_ipv6_dest_addr_set(struct aq_hw *aq_hw, u8 location,
				       u32 *ipv6_dest);

/* set vlan inner ethertype */
void hw_atl_rpf_vlan_inner_etht_set(struct aq_hw *aq_hw, u32 vlan_inner_etht);

/* set vlan outer ethertype */
void hw_atl_rpf_vlan_outer_etht_set(struct aq_hw *aq_hw, u32 vlan_outer_etht);

/* set vlan promiscuous mode enable */
void hw_atl_rpf_vlan_prom_mode_en_set(struct aq_hw *aq_hw,
				      u32 vlan_prom_mode_en);

/* Set VLAN untagged action */
void hw_atl_rpf_vlan_untagged_act_set(struct aq_hw *aq_hw,
				      u32 vlan_untagged_act);

/* Set VLAN accept untagged packets */
void hw_atl_rpf_vlan_accept_untagged_packets_set(struct aq_hw *aq_hw,
						 u32 vlan_acc_untagged_packets);

/* Set VLAN filter enable */
void hw_atl_rpf_vlan_flr_en_set(struct aq_hw *aq_hw, u32 vlan_flr_en,
				u32 filter);

/* Set VLAN Filter Action */
void hw_atl_rpf_vlan_flr_act_set(struct aq_hw *aq_hw, u32 vlan_filter_act,
				 u32 filter);

/* Set VLAN ID Filter */
void hw_atl_rpf_vlan_id_flr_set(struct aq_hw *aq_hw, u32 vlan_id_flr,
				u32 filter);

/* Set VLAN RX queue assignment enable */
void hw_atl_rpf_vlan_rxq_en_flr_set(struct aq_hw *aq_hw, u32 vlan_rxq_en,
				u32 filter);

/* Set VLAN RX queue */
void hw_atl_rpf_vlan_rxq_flr_set(struct aq_hw *aq_hw, u32 vlan_rxq,
				u32 filter);

/* set ethertype filter enable */
void hw_atl_rpf_etht_flr_en_set(struct aq_hw *aq_hw, u32 etht_flr_en,
				u32 filter);

/* set  ethertype user-priority enable */
void hw_atl_rpf_etht_user_priority_en_set(struct aq_hw *aq_hw,
					  u32 etht_user_priority_en,
					  u32 filter);

/* set  ethertype rx queue enable */
void hw_atl_rpf_etht_rx_queue_en_set(struct aq_hw *aq_hw,
				     u32 etht_rx_queue_en,
				     u32 filter);

/* set ethertype rx queue */
void hw_atl_rpf_etht_rx_queue_set(struct aq_hw *aq_hw, u32 etht_rx_queue,
				  u32 filter);

/* set ethertype user-priority */
void hw_atl_rpf_etht_user_priority_set(struct aq_hw *aq_hw,
				       u32 etht_user_priority,
				       u32 filter);

/* set ethertype management queue */
void hw_atl_rpf_etht_mgt_queue_set(struct aq_hw *aq_hw, u32 etht_mgt_queue,
				   u32 filter);

/* set ethertype filter action */
void hw_atl_rpf_etht_flr_act_set(struct aq_hw *aq_hw, u32 etht_flr_act,
				 u32 filter);

/* set ethertype filter */
void hw_atl_rpf_etht_flr_set(struct aq_hw *aq_hw, u32 etht_flr, u32 filter);

/* set L3/L4 filter enable */
void hw_atl_rpf_l3_l4_enf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 IPv6 enable */
void hw_atl_rpf_l3_v6_enf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 source address enable */
void hw_atl_rpf_l3_saf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 destination address enable */
void hw_atl_rpf_l3_daf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 source port enable */
void hw_atl_rpf_l4_spf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 destination port enable */
void hw_atl_rpf_l4_dpf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 protocol enable */
void hw_atl_rpf_l4_protf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3 ARP filter enable */
void hw_atl_rpf_l3_arpf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 rx queue enable */
void hw_atl_rpf_l3_l4_rxqf_en_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 management queue */
void hw_atl_rpf_l3_l4_mng_rxqf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 filter action */
void hw_atl_rpf_l3_l4_actf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L3/L4 rx queue */
void hw_atl_rpf_l3_l4_rxqf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 protocol value */
void hw_atl_rpf_l4_protf_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 source port */
void hw_atl_rpf_l4_spd_set(struct aq_hw *aq_hw, u32 val, u32 filter);

/* set L4 destination port */
void hw_atl_rpf_l4_dpd_set(struct aq_hw *aq_hw, u32 val, u32 filter);

#endif /* HW_ATL_LLH_H */
