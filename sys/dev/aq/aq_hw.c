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

#include <sys/endian.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <machine/cpu.h>
#include <sys/socket.h>
#include <net/if.h>

#include "aq_hw.h"
#include "aq_dbg.h"
#include "aq_hw_llh.h"
#include "aq_fw.h"

#define AQ_HW_FW_SM_RAM        0x2U
#define AQ_CFG_FW_MIN_VER_EXPECTED 0x01050006U


int aq_hw_err_from_flags(struct aq_hw *hw)
{
    return (0);
}

static void aq_hw_chip_features_init(struct aq_hw *hw, u32 *p)
{
    u32 chip_features = 0U;
    u32 val = reg_glb_mif_id_get(hw);
    u32 mif_rev = val & 0xFFU;

    if ((0xFU & mif_rev) == 1U) {
        chip_features |= AQ_HW_CHIP_REVISION_A0 |
                 AQ_HW_CHIP_MPI_AQ |
                 AQ_HW_CHIP_MIPS;
    } else if ((0xFU & mif_rev) == 2U) {
        chip_features |= AQ_HW_CHIP_REVISION_B0 |
                 AQ_HW_CHIP_MPI_AQ |
                 AQ_HW_CHIP_MIPS |
                 AQ_HW_CHIP_TPO2 |
                 AQ_HW_CHIP_RPF2;
    } else if ((0xFU & mif_rev) == 0xAU) {
        chip_features |= AQ_HW_CHIP_REVISION_B1 |
                 AQ_HW_CHIP_MPI_AQ |
                 AQ_HW_CHIP_MIPS |
                 AQ_HW_CHIP_TPO2 |
                 AQ_HW_CHIP_RPF2;
    }

    *p = chip_features;
}

int aq_hw_fw_downld_dwords(struct aq_hw *hw, u32 a, u32 *p, u32 cnt)
{
    int err = 0;

//    AQ_DBG_ENTER();
    AQ_HW_WAIT_FOR(reg_glb_cpu_sem_get(hw,
                       AQ_HW_FW_SM_RAM) == 1U,
                       1U, 10000U);

    if (err < 0) {
        bool is_locked;

        reg_glb_cpu_sem_set(hw, 1U, AQ_HW_FW_SM_RAM);
        is_locked = reg_glb_cpu_sem_get(hw, AQ_HW_FW_SM_RAM);
        if (!is_locked) {
            err = -ETIME;
            goto err_exit;
        }
    }

    mif_mcp_up_mailbox_addr_set(hw, a);

    for (++cnt; --cnt && !err;) {
        mif_mcp_up_mailbox_execute_operation_set(hw, 1);

        if (IS_CHIP_FEATURE(hw, REVISION_B1))
            AQ_HW_WAIT_FOR(a != mif_mcp_up_mailbox_addr_get(hw), 1U, 1000U);
        else
            AQ_HW_WAIT_FOR(!mif_mcp_up_mailbox_busy_get(hw), 1, 1000U);

        *(p++) = mif_mcp_up_mailbox_data_get(hw);
    }

    reg_glb_cpu_sem_set(hw, 1U, AQ_HW_FW_SM_RAM);

err_exit:
//    AQ_DBG_EXIT(err);
    return (err);
}

int aq_hw_ver_match(const aq_hw_fw_version* ver_expected, const aq_hw_fw_version* ver_actual)
{
    AQ_DBG_ENTER();

    if (ver_actual->major_version >= ver_expected->major_version)
        return (true);
    if (ver_actual->minor_version >= ver_expected->minor_version)
        return (true);
    if (ver_actual->build_number >= ver_expected->build_number)
        return (true);

    return (false);
}

static int aq_hw_init_ucp(struct aq_hw *hw)
{
    int err = 0;
    AQ_DBG_ENTER();

    hw->fw_version.raw = 0;

    err = aq_fw_reset(hw);
    if (err != EOK) {
        aq_log_error("aq_hw_init_ucp(): F/W reset failed, err %d", err);
        return (err);
    }

    aq_hw_chip_features_init(hw, &hw->chip_features);
    err = aq_fw_ops_init(hw);
    if (err < 0) {
        aq_log_error("could not initialize F/W ops, err %d", err);
        return (-1);
    }

    if (hw->fw_version.major_version == 1) {
        if (!AQ_READ_REG(hw, 0x370)) {
            unsigned int rnd = 0;
            unsigned int ucp_0x370 = 0;

            rnd = arc4random();

            ucp_0x370 = 0x02020202 | (0xFEFEFEFE & rnd);
            AQ_WRITE_REG(hw, AQ_HW_UCP_0X370_REG, ucp_0x370);
        }

        reg_glb_cpu_scratch_scp_set(hw, 0, 25);
    }

    /* check 10 times by 1ms */
    AQ_HW_WAIT_FOR((hw->mbox_addr = AQ_READ_REG(hw, 0x360)) != 0, 400U, 20);

    aq_hw_fw_version ver_expected = { .raw = AQ_CFG_FW_MIN_VER_EXPECTED };
    if (!aq_hw_ver_match(&ver_expected, &hw->fw_version))
        aq_log_error("atlantic: aq_hw_init_ucp(), wrong FW version: expected:%x actual:%x",
              AQ_CFG_FW_MIN_VER_EXPECTED, hw->fw_version.raw);

    AQ_DBG_EXIT(err);
    return (err);
}

int aq_hw_mpi_create(struct aq_hw *hw)
{
    int err = 0;

    AQ_DBG_ENTER();
    err = aq_hw_init_ucp(hw);
    if (err < 0)
        goto err_exit;

err_exit:
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_hw_mpi_read_stats(struct aq_hw *hw, struct aq_hw_fw_mbox *pmbox)
{
    int err = 0;
//    AQ_DBG_ENTER();

    if (hw->fw_ops && hw->fw_ops->get_stats) {
        err = hw->fw_ops->get_stats(hw, &pmbox->stats);
    } else {
        err = -ENOTSUP;
        aq_log_error("get_stats() not supported by F/W");
    }

    if (err == EOK) {
        pmbox->stats.dpc = reg_rx_dma_stat_counter7get(hw);
        pmbox->stats.cprc = stats_rx_lro_coalesced_pkt_count0_get(hw);
    }

//    AQ_DBG_EXIT(err);
    return (err);
}

static int aq_hw_mpi_set(struct aq_hw *hw,
              enum aq_hw_fw_mpi_state_e state, u32 speed)
{
    int err = -ENOTSUP;
    AQ_DBG_ENTERA("speed %d", speed);

    if (hw->fw_ops && hw->fw_ops->set_mode) {
        err = hw->fw_ops->set_mode(hw, state, speed);
    } else {
        aq_log_error("set_mode() not supported by F/W");
    }

    AQ_DBG_EXIT(err);
    return (err);
}

int aq_hw_set_link_speed(struct aq_hw *hw, u32 speed)
{
    return aq_hw_mpi_set(hw, MPI_INIT, speed);
}

int aq_hw_get_link_state(struct aq_hw *hw, u32 *link_speed, struct aq_hw_fc_info *fc_neg)
{
    int err = EOK;

 //   AQ_DBG_ENTER();

    enum aq_hw_fw_mpi_state_e mode;
    aq_fw_link_speed_t speed = aq_fw_none;
    aq_fw_link_fc_t fc;

    if (hw->fw_ops && hw->fw_ops->get_mode) {
        err = hw->fw_ops->get_mode(hw, &mode, &speed, &fc);
    } else {
        aq_log_error("get_mode() not supported by F/W");
		AQ_DBG_EXIT(-ENOTSUP);
        return (-ENOTSUP);
    }

    if (err < 0) {
        aq_log_error("get_mode() failed, err %d", err);
		AQ_DBG_EXIT(err);
        return (err);
    }
	*link_speed = 0;
    if (mode != MPI_INIT)
        return (0);

    switch (speed) {
    case aq_fw_10G:
        *link_speed = 10000U;
        break;

    case aq_fw_5G:
        *link_speed = 5000U;
        break;

    case aq_fw_2G5:
        *link_speed = 2500U;
        break;

    case aq_fw_1G:
        *link_speed = 1000U;
        break;

    case aq_fw_100M:
        *link_speed = 100U;
        break;

    default:
        *link_speed = 0U;
        break;
    }

    fc_neg->fc_rx = !!(fc & aq_fw_fc_ENABLE_RX);
    fc_neg->fc_tx = !!(fc & aq_fw_fc_ENABLE_TX);

 //   AQ_DBG_EXIT(0);
    return (0);
}

int aq_hw_get_mac_permanent(struct aq_hw *hw,  u8 *mac)
{
    int err = -ENOTSUP;
    AQ_DBG_ENTER();

    if (hw->fw_ops && hw->fw_ops->get_mac_addr)
        err = hw->fw_ops->get_mac_addr(hw, mac);

    /* Couldn't get MAC address from HW. Use auto-generated one. */
    if ((mac[0] & 1) || ((mac[0] | mac[1] | mac[2]) == 0)) {
        u16 rnd;
        u32 h = 0;
        u32 l = 0;

        printf("atlantic: HW MAC address %x:%x:%x:%x:%x:%x is multicast or empty MAC", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        printf("atlantic: Use random MAC address");

        rnd = arc4random();

        /* chip revision */
        l = 0xE3000000U
            | (0xFFFFU & rnd)
            | (0x00 << 16);
        h = 0x8001300EU;

        mac[5] = (u8)(0xFFU & l);
        l >>= 8;
        mac[4] = (u8)(0xFFU & l);
        l >>= 8;
        mac[3] = (u8)(0xFFU & l);
        l >>= 8;
        mac[2] = (u8)(0xFFU & l);
        mac[1] = (u8)(0xFFU & h);
        h >>= 8;
        mac[0] = (u8)(0xFFU & h);

        err = EOK;
    }

    AQ_DBG_EXIT(err);
    return (err);
}

int aq_hw_deinit(struct aq_hw *hw)
{
    AQ_DBG_ENTER();
    aq_hw_mpi_set(hw, MPI_DEINIT, 0);
    AQ_DBG_EXIT(0);
    return (0);
}

int aq_hw_set_power(struct aq_hw *hw, unsigned int power_state)
{
    AQ_DBG_ENTER();
    aq_hw_mpi_set(hw, MPI_POWER, 0);
    AQ_DBG_EXIT(0);
    return (0);
}


/* HW NIC functions */

int aq_hw_reset(struct aq_hw *hw)
{
    int err = 0;

    AQ_DBG_ENTER();

    err = aq_fw_reset(hw);
    if (err < 0)
        goto err_exit;

    itr_irq_reg_res_dis_set(hw, 0);
    itr_res_irq_set(hw, 1);

    /* check 10 times by 1ms */
    AQ_HW_WAIT_FOR(itr_res_irq_get(hw) == 0, 1000, 10);
    if (err < 0) {
        printf("atlantic: IRQ reset failed: %d", err);
        goto err_exit;
    }

    if (hw->fw_ops && hw->fw_ops->reset)
        hw->fw_ops->reset(hw);

    err = aq_hw_err_from_flags(hw);

err_exit:
    AQ_DBG_EXIT(err);
    return (err);
}

static int aq_hw_qos_set(struct aq_hw *hw)
{
    u32 tc = 0U;
    u32 buff_size = 0U;
    unsigned int i_priority = 0U;
    int err = 0;

    AQ_DBG_ENTER();
    /* TPS Descriptor rate init */
    tps_tx_pkt_shed_desc_rate_curr_time_res_set(hw, 0x0U);
    tps_tx_pkt_shed_desc_rate_lim_set(hw, 0xA);

    /* TPS VM init */
    tps_tx_pkt_shed_desc_vm_arb_mode_set(hw, 0U);

    /* TPS TC credits init */
    tps_tx_pkt_shed_desc_tc_arb_mode_set(hw, 0U);
    tps_tx_pkt_shed_data_arb_mode_set(hw, 0U);

    tps_tx_pkt_shed_tc_data_max_credit_set(hw, 0xFFF, 0U);
    tps_tx_pkt_shed_tc_data_weight_set(hw, 0x64, 0U);
    tps_tx_pkt_shed_desc_tc_max_credit_set(hw, 0x50, 0U);
    tps_tx_pkt_shed_desc_tc_weight_set(hw, 0x1E, 0U);

    /* Tx buf size */
    buff_size = AQ_HW_TXBUF_MAX;

    tpb_tx_pkt_buff_size_per_tc_set(hw, buff_size, tc);
    tpb_tx_buff_hi_threshold_per_tc_set(hw,
                        (buff_size * (1024 / 32U) * 66U) /
                        100U, tc);
    tpb_tx_buff_lo_threshold_per_tc_set(hw,
                        (buff_size * (1024 / 32U) * 50U) /
                        100U, tc);

    /* QoS Rx buf size per TC */
    tc = 0;
    buff_size = AQ_HW_RXBUF_MAX;

    rpb_rx_pkt_buff_size_per_tc_set(hw, buff_size, tc);
    rpb_rx_buff_hi_threshold_per_tc_set(hw,
                        (buff_size *
                        (1024U / 32U) * 66U) /
                        100U, tc);
    rpb_rx_buff_lo_threshold_per_tc_set(hw,
                        (buff_size *
                        (1024U / 32U) * 50U) /
                        100U, tc);

    /* QoS 802.1p priority -> TC mapping */
    for (i_priority = 8U; i_priority--;)
        rpf_rpb_user_priority_tc_map_set(hw, i_priority, 0U);

    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

static int aq_hw_offload_set(struct aq_hw *hw)
{
    int err = 0;

    AQ_DBG_ENTER();
    /* TX checksums offloads*/
    tpo_ipv4header_crc_offload_en_set(hw, 1);
    tpo_tcp_udp_crc_offload_en_set(hw, 1);
    if (err < 0)
        goto err_exit;

    /* RX checksums offloads*/
    rpo_ipv4header_crc_offload_en_set(hw, 1);
    rpo_tcp_udp_crc_offload_en_set(hw, 1);
    if (err < 0)
        goto err_exit;

    /* LSO offloads*/
    tdm_large_send_offload_en_set(hw, 0xFFFFFFFFU);
    if (err < 0)
        goto err_exit;

/* LRO offloads */
    {
        u32 i = 0;
        u32 val = (8U < HW_ATL_B0_LRO_RXD_MAX) ? 0x3U :
            ((4U < HW_ATL_B0_LRO_RXD_MAX) ? 0x2U :
            ((2U < HW_ATL_B0_LRO_RXD_MAX) ? 0x1U : 0x0));

        for (i = 0; i < HW_ATL_B0_RINGS_MAX; i++)
            rpo_lro_max_num_of_descriptors_set(hw, val, i);

        rpo_lro_time_base_divider_set(hw, 0x61AU);
        rpo_lro_inactive_interval_set(hw, 0);
        /* the LRO timebase divider is 5 uS (0x61a),
         * to get a maximum coalescing interval of 250 uS,
         * we need to multiply by 50(0x32) to get
         * the default value 250 uS
         */
        rpo_lro_max_coalescing_interval_set(hw, 50);

        rpo_lro_qsessions_lim_set(hw, 1U);

        rpo_lro_total_desc_lim_set(hw, 2U);

        rpo_lro_patch_optimization_en_set(hw, 0U);

        rpo_lro_min_pay_of_first_pkt_set(hw, 10U);

        rpo_lro_pkt_lim_set(hw, 1U);

        rpo_lro_en_set(hw, (hw->lro_enabled ? 0xFFFFFFFFU : 0U));
    }


    err = aq_hw_err_from_flags(hw);

err_exit:
    AQ_DBG_EXIT(err);
    return (err);
}

static int aq_hw_init_tx_path(struct aq_hw *hw)
{
    int err = 0;

    AQ_DBG_ENTER();

    /* Tx TC/RSS number config */
    tpb_tx_tc_mode_set(hw, 1U);

    thm_lso_tcp_flag_of_first_pkt_set(hw, 0x0FF6U);
    thm_lso_tcp_flag_of_middle_pkt_set(hw, 0x0FF6U);
    thm_lso_tcp_flag_of_last_pkt_set(hw, 0x0F7FU);

    /* Tx interrupts */
    tdm_tx_desc_wr_wb_irq_en_set(hw, 1U);

    /* misc */
    AQ_WRITE_REG(hw, 0x00007040U, 0x00010000U);//IS_CHIP_FEATURE(TPO2) ? 0x00010000U : 0x00000000U);
    tdm_tx_dca_en_set(hw, 0U);
    tdm_tx_dca_mode_set(hw, 0U);

    tpb_tx_path_scp_ins_en_set(hw, 1U);

    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

static int aq_hw_init_rx_path(struct aq_hw *hw)
{
    //struct aq_nic_cfg_s *cfg = hw->aq_nic_cfg;
    unsigned int control_reg_val = 0U;
    int i;
    int err;

    AQ_DBG_ENTER();
    /* Rx TC/RSS number config */
    rpb_rpf_rx_traf_class_mode_set(hw, 1U);

    /* Rx flow control */
    rpb_rx_flow_ctl_mode_set(hw, 1U);

    /* RSS Ring selection */
    reg_rx_flr_rss_control1set(hw, 0xB3333333U);

    /* Multicast filters */
    for (i = AQ_HW_MAC_MAX; i--;) {
        rpfl2_uc_flr_en_set(hw, (i == 0U) ? 1U : 0U, i);
        rpfl2unicast_flr_act_set(hw, 1U, i);
    }

    reg_rx_flr_mcst_flr_msk_set(hw, 0x00000000U);
    reg_rx_flr_mcst_flr_set(hw, 0x00010FFFU, 0U);

    /* Vlan filters */
    rpf_vlan_outer_etht_set(hw, 0x88A8U);
    rpf_vlan_inner_etht_set(hw, 0x8100U);
	rpf_vlan_accept_untagged_packets_set(hw, true);
	rpf_vlan_untagged_act_set(hw, HW_ATL_RX_HOST);

    rpf_vlan_prom_mode_en_set(hw, 1);
 
    /* Rx Interrupts */
    rdm_rx_desc_wr_wb_irq_en_set(hw, 1U);

    /* misc */
    control_reg_val = 0x000F0000U; //RPF2

    /* RSS hash type set for IP/TCP */
    control_reg_val |= 0x1EU;

    AQ_WRITE_REG(hw, 0x00005040U, control_reg_val);

    rpfl2broadcast_en_set(hw, 1U);
    rpfl2broadcast_flr_act_set(hw, 1U);
    rpfl2broadcast_count_threshold_set(hw, 0xFFFFU & (~0U / 256U));

    rdm_rx_dca_en_set(hw, 0U);
    rdm_rx_dca_mode_set(hw, 0U);

    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_hw_mac_addr_set(struct aq_hw *hw, u8 *mac_addr, u8 index)
{
    int err = 0;
    unsigned int h = 0U;
    unsigned int l = 0U;

    AQ_DBG_ENTER();
    if (!mac_addr) {
        err = -EINVAL;
        goto err_exit;
    }
    h = (mac_addr[0] << 8) | (mac_addr[1]);
    l = (mac_addr[2] << 24) | (mac_addr[3] << 16) |
        (mac_addr[4] << 8) | mac_addr[5];

    rpfl2_uc_flr_en_set(hw, 0U, index);
    rpfl2unicast_dest_addresslsw_set(hw, l, index);
    rpfl2unicast_dest_addressmsw_set(hw, h, index);
    rpfl2_uc_flr_en_set(hw, 1U, index);

    err = aq_hw_err_from_flags(hw);

err_exit:
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_hw_init(struct aq_hw *hw, u8 *mac_addr, u8 adm_irq, bool msix)
{

    int err = 0;
    u32 val = 0;

    AQ_DBG_ENTER();

    /* Force limit MRRS on RDM/TDM to 2K */
    val = AQ_READ_REG(hw, AQ_HW_PCI_REG_CONTROL_6_ADR);
    AQ_WRITE_REG(hw, AQ_HW_PCI_REG_CONTROL_6_ADR, (val & ~0x707) | 0x404);

    /* TX DMA total request limit. B0 hardware is not capable to
    * handle more than (8K-MRRS) incoming DMA data.
    * Value 24 in 256byte units
    */
    AQ_WRITE_REG(hw, AQ_HW_TX_DMA_TOTAL_REQ_LIMIT_ADR, 24);

    aq_hw_init_tx_path(hw);
    aq_hw_init_rx_path(hw);

    aq_hw_mac_addr_set(hw, mac_addr, AQ_HW_MAC);

    aq_hw_mpi_set(hw, MPI_INIT, hw->link_rate);

    aq_hw_qos_set(hw);

    err = aq_hw_err_from_flags(hw);
    if (err < 0)
        goto err_exit;

    /* Interrupts */
    //Enable interrupt
    itr_irq_status_cor_en_set(hw, 0); //Disable clear-on-read for status
    itr_irq_auto_mask_clr_en_set(hw, 1); // Enable auto-mask clear.
	if (msix)
		itr_irq_mode_set(hw, 0x6); //MSIX + multi vector
	else
		itr_irq_mode_set(hw, 0x5); //MSI + multi vector

    reg_gen_irq_map_set(hw, 0x80 | adm_irq, 3);

    aq_hw_offload_set(hw);

err_exit:
    AQ_DBG_EXIT(err);
    return (err);
}


int aq_hw_start(struct aq_hw *hw)
{
    int err;

    AQ_DBG_ENTER();
    tpb_tx_buff_en_set(hw, 1U);
    rpb_rx_buff_en_set(hw, 1U);
    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}


int aq_hw_interrupt_moderation_set(struct aq_hw *hw)
{
    static unsigned int AQ_HW_NIC_timers_table_rx_[][2] = {
        {80, 120},//{0x6U, 0x38U},/* 10Gbit */
        {0xCU, 0x70U},/* 5Gbit */
        {0xCU, 0x70U},/* 5Gbit 5GS */
        {0x18U, 0xE0U},/* 2.5Gbit */
        {0x30U, 0x80U},/* 1Gbit */
        {0x4U, 0x50U},/* 100Mbit */
    };
    static unsigned int AQ_HW_NIC_timers_table_tx_[][2] = {
        {0x4fU, 0x1ff},//{0xffU, 0xffU}, /* 10Gbit */
        {0x4fU, 0xffU}, /* 5Gbit */
        {0x4fU, 0xffU}, /* 5Gbit 5GS */
        {0x4fU, 0xffU}, /* 2.5Gbit */
        {0x4fU, 0xffU}, /* 1Gbit */
        {0x4fU, 0xffU}, /* 100Mbit */
    };
    
    u32 speed_index = 0U; //itr settings for 10 g 
    u32 itr_rx = 2U;
    u32 itr_tx = 2U;
    int custom_itr = hw->itr;
    int active = custom_itr != 0;
    int err;


    AQ_DBG_ENTER();
    
    if (custom_itr == -1) {
	    itr_rx |= AQ_HW_NIC_timers_table_rx_[speed_index][0] << 0x8U; /* set min timer value */
	    itr_rx |= AQ_HW_NIC_timers_table_rx_[speed_index][1] << 0x10U; /* set max timer value */

	    itr_tx |= AQ_HW_NIC_timers_table_tx_[speed_index][0] << 0x8U; /* set min timer value */
	    itr_tx |= AQ_HW_NIC_timers_table_tx_[speed_index][1] << 0x10U; /* set max timer value */
    }else{
	    if (custom_itr > 0x1FF)
		    custom_itr = 0x1FF;

	    itr_rx |= (custom_itr/2) << 0x8U; /* set min timer value */
	    itr_rx |= custom_itr << 0x10U; /* set max timer value */

	    itr_tx |= (custom_itr/2) << 0x8U; /* set min timer value */
	    itr_tx |= custom_itr << 0x10U; /* set max timer value */
    }
    
    tdm_tx_desc_wr_wb_irq_en_set(hw, !active);
    tdm_tdm_intr_moder_en_set(hw, active);
    rdm_rx_desc_wr_wb_irq_en_set(hw, !active);
    rdm_rdm_intr_moder_en_set(hw, active);
    
    for (int i = HW_ATL_B0_RINGS_MAX; i--;) {
        reg_tx_intr_moder_ctrl_set(hw,  itr_tx, i);
        reg_rx_intr_moder_ctrl_set(hw,  itr_rx, i);
    }

    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

/**
 * @brief Set VLAN filter table
 * @details Configure VLAN filter table to accept (and assign the queue) traffic
 *  for the particular vlan ids.
 * Note: use this function under vlan promisc mode not to lost the traffic
 *
 * @param aq_hw_s
 * @param aq_rx_filter_vlan VLAN filter configuration
 * @return 0 - OK, <0 - error
 */
int hw_atl_b0_hw_vlan_set(struct aq_hw_s *self,
				  struct aq_rx_filter_vlan *aq_vlans)
{
	int i;

	for (i = 0; i < AQ_HW_VLAN_MAX_FILTERS; i++) {
		hw_atl_rpf_vlan_flr_en_set(self, 0U, i);
		hw_atl_rpf_vlan_rxq_en_flr_set(self, 0U, i);
		if (aq_vlans[i].enable) {
			hw_atl_rpf_vlan_id_flr_set(self,
						   aq_vlans[i].vlan_id,
						   i);
			hw_atl_rpf_vlan_flr_act_set(self, 1U, i);
			hw_atl_rpf_vlan_flr_en_set(self, 1U, i);
			if (aq_vlans[i].queue != 0xFF) {
				hw_atl_rpf_vlan_rxq_flr_set(self,
							    aq_vlans[i].queue,
							    i);
				hw_atl_rpf_vlan_rxq_en_flr_set(self, 1U, i);
			}
		}
	}

	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_vlan_promisc_set(struct aq_hw_s *self, bool promisc)
{
	hw_atl_rpf_vlan_prom_mode_en_set(self, promisc);
	return aq_hw_err_from_flags(self);
}


void aq_hw_set_promisc(struct aq_hw_s *self, bool l2_promisc, bool vlan_promisc, bool mc_promisc)
{
	AQ_DBG_ENTERA("promisc %d, vlan_promisc %d, allmulti %d", l2_promisc, vlan_promisc, mc_promisc);

	rpfl2promiscuous_mode_en_set(self, l2_promisc);

	hw_atl_b0_hw_vlan_promisc_set(self, l2_promisc | vlan_promisc);

	rpfl2_accept_all_mc_packets_set(self, mc_promisc);
	rpfl2multicast_flr_en_set(self, mc_promisc, 0);

	AQ_DBG_EXIT(0);
}

int aq_hw_rss_hash_set(struct aq_hw_s *self, u8 rss_key[HW_ATL_RSS_HASHKEY_SIZE])
{
	u32 rss_key_dw[HW_ATL_RSS_HASHKEY_SIZE / 4];
	u32 addr = 0U;
	u32 i = 0U;
	int err = 0;

	AQ_DBG_ENTER();

	memcpy(rss_key_dw, rss_key, HW_ATL_RSS_HASHKEY_SIZE);

	for (i = 10, addr = 0U; i--; ++addr) {
		u32 key_data = bswap32(rss_key_dw[i]);
		rpf_rss_key_wr_data_set(self, key_data);
		rpf_rss_key_addr_set(self, addr);
		rpf_rss_key_wr_en_set(self, 1U);
		AQ_HW_WAIT_FOR(rpf_rss_key_wr_en_get(self) == 0,
			       1000U, 10U);
		if (err < 0)
			goto err_exit;
	}

	err = aq_hw_err_from_flags(self);

err_exit:
	AQ_DBG_EXIT(err);
	return (err);
}

int aq_hw_rss_hash_get(struct aq_hw_s *self, u8 rss_key[HW_ATL_RSS_HASHKEY_SIZE])
{
	u32 rss_key_dw[HW_ATL_RSS_HASHKEY_SIZE / 4];
	u32 addr = 0U;
	u32 i = 0U;
	int err = 0;

	AQ_DBG_ENTER();

	for (i = 10, addr = 0U; i--; ++addr) {
		rpf_rss_key_addr_set(self, addr);
		rss_key_dw[i] = bswap32(rpf_rss_key_rd_data_get(self));
	}
	memcpy(rss_key, rss_key_dw, HW_ATL_RSS_HASHKEY_SIZE);

	err = aq_hw_err_from_flags(self);

	AQ_DBG_EXIT(err);
	return (err);
}

int aq_hw_rss_set(struct aq_hw_s *self, u8 rss_table[HW_ATL_RSS_INDIRECTION_TABLE_MAX])
{
	u16 bitary[(HW_ATL_RSS_INDIRECTION_TABLE_MAX *
					3 / 16U)];
	int err = 0;
	u32 i = 0U;

	memset(bitary, 0, sizeof(bitary));

	for (i = HW_ATL_RSS_INDIRECTION_TABLE_MAX; i--;) {
		(*(u32 *)(bitary + ((i * 3U) / 16U))) |=
			((rss_table[i]) << ((i * 3U) & 0xFU));
	}

	for (i = ARRAY_SIZE(bitary); i--;) {
		rpf_rss_redir_tbl_wr_data_set(self, bitary[i]);
		rpf_rss_redir_tbl_addr_set(self, i);
		rpf_rss_redir_wr_en_set(self, 1U);
		AQ_HW_WAIT_FOR(rpf_rss_redir_wr_en_get(self) == 0,
			       1000U, 10U);
		if (err < 0)
			goto err_exit;
	}

	err = aq_hw_err_from_flags(self);

err_exit:
	return (err);
}

int aq_hw_udp_rss_enable(struct aq_hw_s *self, bool enable)
{
	int err = 0;
	if(!enable) {
		/* HW bug workaround:
		 * Disable RSS for UDP using rx flow filter 0.
		 * HW does not track RSS stream for fragmenged UDP,
		 * 0x5040 control reg does not work.
		 */
		hw_atl_rpf_l3_l4_enf_set(self, true, 0);
		hw_atl_rpf_l4_protf_en_set(self, true, 0);
		hw_atl_rpf_l3_l4_rxqf_en_set(self, true, 0);
		hw_atl_rpf_l3_l4_actf_set(self, L2_FILTER_ACTION_HOST, 0);
		hw_atl_rpf_l3_l4_rxqf_set(self, 0, 0);
		hw_atl_rpf_l4_protf_set(self, HW_ATL_RX_UDP, 0);
	} else {
		hw_atl_rpf_l3_l4_enf_set(self, false, 0);
	}

	err = aq_hw_err_from_flags(self);
	return (err);

}
