/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_VPATH_REGS_H
#define	VXGE_HAL_VPATH_REGS_H

__EXTERN_BEGIN_DECLS

typedef struct vxge_hal_vpath_reg_t {

	u8	unused00300[0x00300];

/* 0x00300 */	u64	usdc_vpath;
#define	VXGE_HAL_USDC_VPATH_SGRP_ASSIGN(val)		    vBIT(val, 0, 32)
	u8	unused00a00[0x00a00 - 0x00308];

/* 0x00a00 */	u64	wrdma_alarm_status;
#define	VXGE_HAL_WRDMA_ALARM_STATUS_PRC_ALARM_PRC_INT	    mBIT(1)
/* 0x00a08 */	u64	wrdma_alarm_mask;
	u8	unused00a30[0x00a30 - 0x00a10];

/* 0x00a30 */	u64	prc_alarm_reg;
#define	VXGE_HAL_PRC_ALARM_REG_PRC_RING_BUMP		    mBIT(0)
#define	VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ERR		    mBIT(1)
#define	VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ABORT	    mBIT(2)
#define	VXGE_HAL_PRC_ALARM_REG_PRC_QUANTA_SIZE_ERR	    mBIT(3)
/* 0x00a38 */	u64	prc_alarm_mask;
/* 0x00a40 */	u64	prc_alarm_alarm;
/* 0x00a48 */	u64	prc_cfg1;
#define	VXGE_HAL_PRC_CFG1_RX_TIMER_VAL(val)		    vBIT(val, 3, 29)
#define	VXGE_HAL_PRC_CFG1_TIM_RING_BUMP_INT_ENABLE	    mBIT(34)
#define	VXGE_HAL_PRC_CFG1_RTI_TINT_DISABLE		    mBIT(35)
#define	VXGE_HAL_PRC_CFG1_GREEDY_RETURN			    mBIT(36)
#define	VXGE_HAL_PRC_CFG1_QUICK_SHOT			    mBIT(37)
#define	VXGE_HAL_PRC_CFG1_RX_TIMER_CI			    mBIT(39)
#define	VXGE_HAL_PRC_CFG1_RESET_TIMER_ON_RXD_RET(val)	    vBIT(val, 40, 2)
	u8	unused00a60[0x00a60 - 0x00a50];

/* 0x00a60 */	u64	prc_cfg4;
#define	VXGE_HAL_PRC_CFG4_IN_SVC			    mBIT(7)
#define	VXGE_HAL_PRC_CFG4_RING_MODE(val)		    vBIT(val, 14, 2)
#define	VXGE_HAL_PRC_CFG4_RXD_NO_SNOOP			    mBIT(22)
#define	VXGE_HAL_PRC_CFG4_FRM_NO_SNOOP			    mBIT(23)
#define	VXGE_HAL_PRC_CFG4_RTH_DISABLE			    mBIT(31)
#define	VXGE_HAL_PRC_CFG4_IGNORE_OWNERSHIP		    mBIT(32)
#define	VXGE_HAL_PRC_CFG4_SIGNAL_BENIGN_OVFLW		    mBIT(36)
#define	VXGE_HAL_PRC_CFG4_BIMODAL_INTERRUPT		    mBIT(37)
#define	VXGE_HAL_PRC_CFG4_BACKOFF_INTERVAL(val)		    vBIT(val, 40, 24)
/* 0x00a68 */	u64	prc_cfg5;
#define	VXGE_HAL_PRC_CFG5_RXD0_ADD(val)			    vBIT(val, 0, 61)
/* 0x00a70 */	u64	prc_cfg6;
#define	VXGE_HAL_PRC_CFG6_FRM_PAD_EN			    mBIT(0)
#define	VXGE_HAL_PRC_CFG6_QSIZE_ALIGNED_RXD		    mBIT(2)
#define	VXGE_HAL_PRC_CFG6_DOORBELL_MODE_EN		    mBIT(5)
#define	VXGE_HAL_PRC_CFG6_L3_CPC_TRSFR_CODE_EN		    mBIT(8)
#define	VXGE_HAL_PRC_CFG6_L4_CPC_TRSFR_CODE_EN		    mBIT(9)
#define	VXGE_HAL_PRC_CFG6_RXD_CRXDT(val)		    vBIT(val, 23, 9)
#define	VXGE_HAL_PRC_CFG6_RXD_SPAT(val)			    vBIT(val, 36, 9)
/* 0x00a78 */	u64	prc_cfg7;
#define	VXGE_HAL_PRC_CFG7_SCATTER_MODE(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_PRC_CFG7_SMART_SCAT_EN			    mBIT(11)
#define	VXGE_HAL_PRC_CFG7_RXD_NS_CHG_EN			    mBIT(12)
#define	VXGE_HAL_PRC_CFG7_NO_HDR_SEPARATION		    mBIT(14)
#define	VXGE_HAL_PRC_CFG7_RXD_BUFF_SIZE_MASK(val)	    vBIT(val, 20, 4)
#define	VXGE_HAL_PRC_CFG7_BUFF_SIZE0_MASK(val)		    vBIT(val, 27, 5)
/* 0x00a80 */	u64	tim_dest_addr;
#define	VXGE_HAL_TIM_DEST_ADDR_TIM_DEST_ADDR(val)	    vBIT(val, 0, 64)
/* 0x00a88 */	u64	prc_rxd_doorbell;
#define	VXGE_HAL_PRC_RXD_DOORBELL_NEW_QW_CNT(val)	    vBIT(val, 48, 16)
/* 0x00a90 */	u64	rqa_prty_for_vp;
#define	VXGE_HAL_RQA_PRTY_FOR_VP_RQA_PRTY_FOR_VP(val)	    vBIT(val, 59, 5)
/* 0x00a98 */	u64	rxdmem_size;
#define	VXGE_HAL_RXDMEM_SIZE_PRC_RXDMEM_SIZE(val)	    vBIT(val, 51, 13)
/* 0x00aa0 */	u64	frm_in_progress_cnt;
#define	VXGE_HAL_FRM_IN_PROGRESS_CNT_PRC_FRM_IN_PROGRESS_CNT(val)\
							    vBIT(val, 59, 5)
/* 0x00aa8 */	u64	rx_multi_cast_stats;
#define	VXGE_HAL_RX_MULTI_CAST_STATS_FRAME_DISCARD(val)	    vBIT(val, 48, 16)
/* 0x00ab0 */	u64	rx_frm_transferred;
#define	VXGE_HAL_RX_FRM_TRANSFERRED_RX_FRM_TRANSFERRED(val) vBIT(val, 32, 32)
/* 0x00ab8 */	u64	rxd_returned;
#define	VXGE_HAL_RXD_RETURNED_RXD_RETURNED(val)		    vBIT(val, 48, 16)
	u8	unused00c00[0x00c00 - 0x00ac0];

/* 0x00c00 */	u64	kdfc_fifo_trpl_partition;
#define	VXGE_HAL_KDFC_FIFO_TRPL_PARTITION_LENGTH_0(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_FIFO_TRPL_PARTITION_LENGTH_1(val)	    vBIT(val, 33, 15)
#define	VXGE_HAL_KDFC_FIFO_TRPL_PARTITION_LENGTH_2(val)	    vBIT(val, 49, 15)
/* 0x00c08 */	u64	kdfc_fifo_trpl_ctrl;
#define	VXGE_HAL_KDFC_FIFO_TRPL_CTRL_TRIPLET_ENABLE	    mBIT(7)
/* 0x00c10 */	u64	kdfc_trpl_fifo_0_ctrl;
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_MODE(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_FLIP_EN		    mBIT(22)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_SWAP_EN		    mBIT(23)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_INT_CTRL(val)	    vBIT(val, 26, 2)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_CTRL_STRUC	    mBIT(28)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_ADD_PAD		    mBIT(29)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_NO_SNOOP		    mBIT(30)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_RLX_ORD		    mBIT(31)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_SELECT(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_INT_NO(val)	    vBIT(val, 41, 7)
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_BIT_MAP(val)	    vBIT(val, 48, 16)
/* 0x00c18 */	u64	kdfc_trpl_fifo_1_ctrl;
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_MODE(val)	    vBIT(val, 14, 2)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_FLIP_EN		    mBIT(22)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_SWAP_EN		    mBIT(23)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_INT_CTRL(val)	    vBIT(val, 26, 2)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_CTRL_STRUC	    mBIT(28)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_ADD_PAD		    mBIT(29)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_NO_SNOOP		    mBIT(30)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_RLX_ORD		    mBIT(31)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_SELECT(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_INT_NO(val)	    vBIT(val, 41, 7)
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_BIT_MAP(val)	    vBIT(val, 48, 16)
/* 0x00c20 */	u64	kdfc_trpl_fifo_2_ctrl;
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_FLIP_EN		    mBIT(22)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_SWAP_EN		    mBIT(23)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_INT_CTRL(val)	    vBIT(val, 26, 2)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_CTRL_STRUC	    mBIT(28)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_ADD_PAD		    mBIT(29)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_NO_SNOOP		    mBIT(30)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_RLX_ORD		    mBIT(31)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_SELECT(val)	    vBIT(val, 32, 8)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_INT_NO(val)	    vBIT(val, 41, 7)
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_CTRL_BIT_MAP(val)	    vBIT(val, 48, 16)
/* 0x00c28 */	u64	kdfc_trpl_fifo_0_wb_address;
#define	VXGE_HAL_KDFC_TRPL_FIFO_0_WB_ADDRESS_ADD(val)	    vBIT(val, 0, 64)
/* 0x00c30 */	u64	kdfc_trpl_fifo_1_wb_address;
#define	VXGE_HAL_KDFC_TRPL_FIFO_1_WB_ADDRESS_ADD(val)	    vBIT(val, 0, 64)
/* 0x00c38 */	u64	kdfc_trpl_fifo_2_wb_address;
#define	VXGE_HAL_KDFC_TRPL_FIFO_2_WB_ADDRESS_ADD(val)	    vBIT(val, 0, 64)
/* 0x00c40 */	u64	kdfc_trpl_fifo_offset;
#define	VXGE_HAL_KDFC_TRPL_FIFO_OFFSET_KDFC_RCTR0(val)	    vBIT(val, 1, 15)
#define	VXGE_HAL_KDFC_TRPL_FIFO_OFFSET_KDFC_RCTR1(val)	    vBIT(val, 17, 15)
#define	VXGE_HAL_KDFC_TRPL_FIFO_OFFSET_KDFC_RCTR2(val)	    vBIT(val, 33, 15)
/* 0x00c48 */	u64	kdfc_drbl_triplet_total;
#define	VXGE_HAL_KDFC_DRBL_TRIPLET_TOTAL_KDFC_MAX_SIZE(val) vBIT(val, 17, 15)
	u8	unused00c60[0x00c60 - 0x00c50];

/* 0x00c60 */	u64	usdc_drbl_ctrl;
#define	VXGE_HAL_USDC_DRBL_CTRL_FLIP_EN			    mBIT(22)
#define	VXGE_HAL_USDC_DRBL_CTRL_SWAP_EN			    mBIT(23)
/* 0x00c68 */	u64	usdc_vp_ready;
#define	VXGE_HAL_USDC_VP_READY_USDC_HTN_READY		    mBIT(7)
#define	VXGE_HAL_USDC_VP_READY_USDC_SRQ_READY		    mBIT(15)
#define	VXGE_HAL_USDC_VP_READY_USDC_CQRQ_READY		    mBIT(23)
/* 0x00c70 */	u64	kdfc_status;
#define	VXGE_HAL_KDFC_STATUS_KDFC_WRR_0_READY		    mBIT(0)
#define	VXGE_HAL_KDFC_STATUS_KDFC_WRR_1_READY		    mBIT(1)
#define	VXGE_HAL_KDFC_STATUS_KDFC_WRR_2_READY		    mBIT(2)
	u8	unused00c80[0x00c80 - 0x00c78];

/* 0x00c80 */	u64	xmac_rpa_vcfg;
#define	VXGE_HAL_XMAC_RPA_VCFG_IPV4_TCP_INCL_PH		    mBIT(3)
#define	VXGE_HAL_XMAC_RPA_VCFG_IPV6_TCP_INCL_PH		    mBIT(7)
#define	VXGE_HAL_XMAC_RPA_VCFG_IPV4_UDP_INCL_PH		    mBIT(11)
#define	VXGE_HAL_XMAC_RPA_VCFG_IPV6_UDP_INCL_PH		    mBIT(15)
#define	VXGE_HAL_XMAC_RPA_VCFG_L4_INCL_CF		    mBIT(19)
#define	VXGE_HAL_XMAC_RPA_VCFG_STRIP_VLAN_TAG		    mBIT(23)
/* 0x00c88 */	u64	rxmac_vcfg0;
#define	VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(val)	    vBIT(val, 2, 14)
#define	VXGE_HAL_RXMAC_VCFG0_RTS_USE_MIN_LEN		    mBIT(19)
#define	VXGE_HAL_RXMAC_VCFG0_RTS_MIN_FRM_LEN(val)	    vBIT(val, 26, 14)
#define	VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN		    mBIT(43)
#define	VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN		    mBIT(47)
#define	VXGE_HAL_RXMAC_VCFG0_BCAST_EN			    mBIT(51)
#define	VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN			    mBIT(55)
/* 0x00c90 */	u64	rxmac_vcfg1;
#define	VXGE_HAL_RXMAC_VCFG1_RTS_RTH_MULTI_IT_BD_MODE(val)  vBIT(val, 42, 2)
#define	VXGE_HAL_RXMAC_VCFG1_RTS_RTH_MULTI_IT_EN_MODE	    mBIT(47)
#define	VXGE_HAL_RXMAC_VCFG1_CONTRIB_L2_FLOW		    mBIT(51)
/* 0x00c98 */	u64	rts_access_steer_ctrl;
#define	VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(val)	    vBIT(val, 1, 7)
#define	VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(val) vBIT(val, 8, 4)
#define	VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE		    mBIT(15)
#define	VXGE_HAL_RTS_ACCESS_STEER_CTRL_BEHAV_TBL_SEL	    mBIT(23)
#define	VXGE_HAL_RTS_ACCESS_STEER_CTRL_TABLE_SEL	    mBIT(27)
#define	VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(val)	    vBIT(val, 40, 8)
#define	VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS	    mBIT(0)
/* 0x00ca0 */	u64	rts_access_steer_data0;
#define	VXGE_HAL_RTS_ACCESS_STEER_DATA0_DATA(val)	    vBIT(val, 0, 64)
/* 0x00ca8 */	u64	rts_access_steer_data1;
#define	VXGE_HAL_RTS_ACCESS_STEER_DATA1_DATA(val)	    vBIT(val, 0, 64)
	u8	unused00d00[0x00d00 - 0x00cb0];

/* 0x00d00 */	u64	xmac_vsport_choice;
#define	VXGE_HAL_XMAC_VSPORT_CHOICE_VSPORT_NUMBER(val)	    vBIT(val, 3, 5)
/* 0x00d08 */	u64	xmac_stats_cfg;
/* 0x00d10 */	u64	xmac_stats_access_cmd;
#define	VXGE_HAL_XMAC_STATS_ACCESS_CMD_OP(val)		    vBIT(val, 6, 2)
#define	VXGE_HAL_XMAC_STATS_ACCESS_CMD_STROBE		    mBIT(15)
#define	VXGE_HAL_XMAC_STATS_ACCESS_CMD_OFFSET_SEL(val)	    vBIT(val, 32, 8)
/* 0x00d18 */	u64	xmac_stats_access_data;
#define	VXGE_HAL_XMAC_STATS_ACCESS_DATA_XSMGR_DATA(val)	    vBIT(val, 0, 64)
/* 0x00d20 */	u64	asic_ntwk_vp_ctrl;
#define	VXGE_HAL_ASIC_NTWK_VP_CTRL_REQ_TEST_NTWK	    mBIT(3)
#define	VXGE_HAL_ASIC_NTWK_VP_CTRL_XMACJ_SHOW_PORT_INFO	    mBIT(55)
#define	VXGE_HAL_ASIC_NTWK_VP_CTRL_XMACJ_PORT_NUM	    mBIT(63)
	u8	unused00d30[0x00d30 - 0x00d28];

/* 0x00d30 */	u64	xgmac_vp_int_status;
#define	VXGE_HAL_XGMAC_VP_INT_STATUS_ASIC_NTWK_VP_ERR_INT   mBIT(3)
/* 0x00d38 */	u64	xgmac_vp_int_mask;
/* 0x00d40 */	u64	asic_ntwk_vp_err_reg;
#define	VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT		    mBIT(3)
#define	VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK		    mBIT(7)
#define	VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT_OCCURRED    mBIT(11)
#define	VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK_OCCURRED	    mBIT(15)
#define	VXGE_HAL_ASIC_NTWK_VP_ERR_REG_REAF_FAULT	    mBIT(19)
#define	VXGE_HAL_ASIC_NTWK_VP_ERR_REG_REAF_OK		    mBIT(23)
/* 0x00d48 */	u64	asic_ntwk_vp_err_mask;
/* 0x00d50 */	u64	asic_ntwk_vp_err_alarm;
	u8	unused00d80[0x00d80 - 0x00d58];

/* 0x00d80 */	u64	rtdma_bw_ctrl;
#define	VXGE_HAL_RTDMA_BW_CTRL_BW_CTRL_EN		    mBIT(39)
#define	VXGE_HAL_RTDMA_BW_CTRL_DESIRED_BW(val)		    vBIT(val, 46, 18)
/* 0x00d88 */	u64	rtdma_rd_optimization_ctrl;
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_GEN_INT_AFTER_ABORT	mBIT(3)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_PAD_MODE(val)   vBIT(val, 6, 2)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_PAD_PATTERN(val) vBIT(val, 8, 8)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_FB_WAIT_FOR_SPACE mBIT(19)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_FB_FILL_THRESH(val)\
							    vBIT(val, 21, 3)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_TXD_PYLD_WMARK_EN mBIT(28)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_TXD_PYLD_WMARK(val)\
							    vBIT(val, 29, 3)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_FB_ADDR_BDRY_EN mBIT(35)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_FB_ADDR_BDRY(val) vBIT(val, 37, 3)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_TXD_WAIT_FOR_SPACE mBIT(43)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_TXD_FILL_THRESH(val)\
							    vBIT(val, 51, 5)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_TXD_ADDR_BDRY_EN mBIT(59)
#define	VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_TXD_ADDR_BDRY(val) vBIT(val, 61, 3)
/* 0x00d90 */	u64	pda_pcc_job_monitor;
#define	VXGE_HAL_PDA_PCC_JOB_MONITOR_PDA_PCC_JOB_STATUS	    mBIT(7)
/* 0x00d98 */	u64	tx_protocol_assist_cfg;
#define	VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_LSOV2_EN	    mBIT(6)
#define	VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_IPV6_KEEP_SEARCHING mBIT(7)
	u8	unused01000[0x01000 - 0x00da0];

/* 0x01000 */	u64	tim_cfg1_int_num[4];
#define	VXGE_HAL_TIM_CFG1_INT_NUM_BTIMER_VAL(val)	    vBIT(val, 6, 26)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_BITMP_EN		    mBIT(35)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_TXFRM_CNT_EN		    mBIT(36)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_TXD_CNT_EN		    mBIT(37)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_AC		    mBIT(38)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI		    mBIT(39)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_URNG_A(val)		    vBIT(val, 41, 7)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_URNG_B(val)		    vBIT(val, 49, 7)
#define	VXGE_HAL_TIM_CFG1_INT_NUM_URNG_C(val)		    vBIT(val, 57, 7)
/* 0x01020 */	u64	tim_cfg2_int_num[4];
#define	VXGE_HAL_TIM_CFG2_INT_NUM_UEC_A(val)		    vBIT(val, 0, 16)
#define	VXGE_HAL_TIM_CFG2_INT_NUM_UEC_B(val)		    vBIT(val, 16, 16)
#define	VXGE_HAL_TIM_CFG2_INT_NUM_UEC_C(val)		    vBIT(val, 32, 16)
#define	VXGE_HAL_TIM_CFG2_INT_NUM_UEC_D(val)		    vBIT(val, 48, 16)
/* 0x01040 */	u64	tim_cfg3_int_num[4];
#define	VXGE_HAL_TIM_CFG3_INT_NUM_TIMER_RI		    mBIT(0)
#define	VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_EVENT_SF(val)	    vBIT(val, 1, 4)
#define	VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(val)	    vBIT(val, 6, 26)
#define	VXGE_HAL_TIM_CFG3_INT_NUM_UTIL_SEL(val)		    vBIT(val, 32, 6)
#define	VXGE_HAL_TIM_CFG3_INT_NUM_LTIMER_VAL(val)	    vBIT(val, 38, 26)
/* 0x01060 */	u64	tim_wrkld_clc;
#define	VXGE_HAL_TIM_WRKLD_CLC_WRKLD_EVAL_PRD(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_TIM_WRKLD_CLC_WRKLD_EVAL_DIV(val)	    vBIT(val, 35, 5)
#define	VXGE_HAL_TIM_WRKLD_CLC_CNT_FRM_BYTE		    mBIT(40)
#define	VXGE_HAL_TIM_WRKLD_CLC_CNT_RX_TX(val)		    vBIT(val, 41, 2)
#define	VXGE_HAL_TIM_WRKLD_CLC_CNT_LNK_EN		    mBIT(43)
#define	VXGE_HAL_TIM_WRKLD_CLC_HOST_UTIL(val)		    vBIT(val, 57, 7)
/* 0x01068 */	u64	tim_bitmap;
#define	VXGE_HAL_TIM_BITMAP_MASK(val)			    vBIT(val, 0, 32)
#define	VXGE_HAL_TIM_BITMAP_LLROOT_RXD_EN		    mBIT(32)
#define	VXGE_HAL_TIM_BITMAP_LLROOT_TXD_EN		    mBIT(33)
/* 0x01070 */	u64	tim_ring_assn;
#define	VXGE_HAL_TIM_RING_ASSN_INT_NUM(val)		    vBIT(val, 6, 2)
/* 0x01078 */	u64	tim_remap;
#define	VXGE_HAL_TIM_REMAP_TX_EN			    mBIT(5)
#define	VXGE_HAL_TIM_REMAP_RX_EN			    mBIT(6)
#define	VXGE_HAL_TIM_REMAP_OFFLOAD_EN			    mBIT(7)
#define	VXGE_HAL_TIM_REMAP_TO_VPATH_NUM(val)		    vBIT(val, 11, 5)
/* 0x01080 */	u64	tim_vpath_map;
#define	VXGE_HAL_TIM_VPATH_MAP_BMAP_ROOT(val)		    vBIT(val, 0, 32)
/* 0x01088 */	u64	tim_pci_cfg;
#define	VXGE_HAL_TIM_PCI_CFG_ADD_PAD			    mBIT(7)
#define	VXGE_HAL_TIM_PCI_CFG_NO_SNOOP			    mBIT(15)
#define	VXGE_HAL_TIM_PCI_CFG_RELAXED			    mBIT(23)
#define	VXGE_HAL_TIM_PCI_CFG_CTL_STR			    mBIT(31)
	u8	unused01100[0x01100 - 0x01090];

/* 0x01100 */	u64	sgrp_assign;
#define	VXGE_HAL_SGRP_ASSIGN_SGRP_ASSIGN(val)		    vBIT(val, 0, 64)
/* 0x01108 */	u64	sgrp_aoa_and_result;
#define	VXGE_HAL_SGRP_AOA_AND_RESULT_PET_SGRP_AOA_AND_RESULT(val)\
							    vBIT(val, 0, 64)
/* 0x01110 */	u64	rpe_pci_cfg;
#define	VXGE_HAL_RPE_PCI_CFG_PAD_LRO_DATA_ENABLE	    mBIT(7)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_LRO_HDR_ENABLE		    mBIT(8)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_LRO_CQE_ENABLE		    mBIT(9)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_NONLL_CQE_ENABLE	    mBIT(10)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_BASE_LL_CQE_ENABLE	    mBIT(11)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_LL_CQE_IDATA_ENABLE	    mBIT(12)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_CQRQ_IR_ENABLE		    mBIT(13)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_CQSQ_IR_ENABLE		    mBIT(14)
#define	VXGE_HAL_RPE_PCI_CFG_PAD_CQRR_IR_ENABLE		    mBIT(15)
#define	VXGE_HAL_RPE_PCI_CFG_NOSNOOP_DATA		    mBIT(18)
#define	VXGE_HAL_RPE_PCI_CFG_NOSNOOP_NONLL_CQE		    mBIT(19)
#define	VXGE_HAL_RPE_PCI_CFG_NOSNOOP_LL_CQE		    mBIT(20)
#define	VXGE_HAL_RPE_PCI_CFG_NOSNOOP_CQRQ_IR		    mBIT(21)
#define	VXGE_HAL_RPE_PCI_CFG_NOSNOOP_CQSQ_IR		    mBIT(22)
#define	VXGE_HAL_RPE_PCI_CFG_NOSNOOP_CQRR_IR		    mBIT(23)
#define	VXGE_HAL_RPE_PCI_CFG_RELAXED_DATA		    mBIT(26)
#define	VXGE_HAL_RPE_PCI_CFG_RELAXED_NONLL_CQE		    mBIT(27)
#define	VXGE_HAL_RPE_PCI_CFG_RELAXED_LL_CQE		    mBIT(28)
#define	VXGE_HAL_RPE_PCI_CFG_RELAXED_CQRQ_IR		    mBIT(29)
#define	VXGE_HAL_RPE_PCI_CFG_RELAXED_CQSQ_IR		    mBIT(30)
#define	VXGE_HAL_RPE_PCI_CFG_RELAXED_CQRR_IR		    mBIT(31)
/* 0x01118 */	u64	rpe_lro_cfg;
#define	VXGE_HAL_RPE_LRO_CFG_SUPPRESS_LRO_ETH_TRLR	    mBIT(7)
#define	VXGE_HAL_RPE_LRO_CFG_ALLOW_LRO_SNAP_SNAPJUMBO_MRG   mBIT(11)
#define	VXGE_HAL_RPE_LRO_CFG_ALLOW_LRO_LLC_LLCJUMBO_MRG	    mBIT(15)
#define	VXGE_HAL_RPE_LRO_CFG_INCL_ACK_CNT_IN_CQE	    mBIT(23)
/* 0x01120 */	u64	pe_mr2vp_ack_blk_limit;
#define	VXGE_HAL_PE_MR2VP_ACK_BLK_LIMIT_BLK_LIMIT(val)	    vBIT(val, 32, 32)
/* 0x01128 */	u64	pe_mr2vp_rirr_lirr_blk_limit;
#define	VXGE_HAL_PE_MR2VP_RIRR_LIRR_BLK_LIMIT_RIRR_BLK_LIMIT(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_PE_MR2VP_RIRR_LIRR_BLK_LIMIT_LIRR_BLK_LIMIT(val)\
							    vBIT(val, 32, 32)
/* 0x01130 */	u64	txpe_pci_nce_cfg;
#define	VXGE_HAL_TXPE_PCI_NCE_CFG_NCE_THRESH(val)	    vBIT(val, 0, 32)
#define	VXGE_HAL_TXPE_PCI_NCE_CFG_PAD_TOWI_ENABLE	    mBIT(55)
#define	VXGE_HAL_TXPE_PCI_NCE_CFG_NOSNOOP_TOWI		    mBIT(63)
	u8	unused01180[0x01180 - 0x01138];

/* 0x01180 */	u64	msg_qpad_en_cfg;
#define	VXGE_HAL_MSG_QPAD_EN_CFG_UMQ_BWR_READ		    mBIT(3)
#define	VXGE_HAL_MSG_QPAD_EN_CFG_DMQ_BWR_READ		    mBIT(7)
#define	VXGE_HAL_MSG_QPAD_EN_CFG_MXP_GENDMA_READ	    mBIT(11)
#define	VXGE_HAL_MSG_QPAD_EN_CFG_UXP_GENDMA_READ	    mBIT(15)
#define	VXGE_HAL_MSG_QPAD_EN_CFG_UMQ_MSG_WRITE		    mBIT(19)
#define	VXGE_HAL_MSG_QPAD_EN_CFG_UMQDMQ_IR_WRITE	    mBIT(23)
#define	VXGE_HAL_MSG_QPAD_EN_CFG_MXP_GENDMA_WRITE	    mBIT(27)
#define	VXGE_HAL_MSG_QPAD_EN_CFG_UXP_GENDMA_WRITE	    mBIT(31)
/* 0x01188 */	u64	msg_pci_cfg;
#define	VXGE_HAL_MSG_PCI_CFG_GENDMA_NO_SNOOP		    mBIT(3)
#define	VXGE_HAL_MSG_PCI_CFG_UMQDMQ_IR_NO_SNOOP		    mBIT(7)
#define	VXGE_HAL_MSG_PCI_CFG_UMQ_NO_SNOOP		    mBIT(11)
#define	VXGE_HAL_MSG_PCI_CFG_DMQ_NO_SNOOP		    mBIT(15)
/* 0x01190 */	u64	umqdmq_ir_init;
#define	VXGE_HAL_UMQDMQ_IR_INIT_HOST_WRITE_ADD(val)	    vBIT(val, 0, 64)
/* 0x01198 */	u64	dmq_ir_int;
#define	VXGE_HAL_DMQ_IR_INT_IMMED_ENABLE		    mBIT(6)
#define	VXGE_HAL_DMQ_IR_INT_EVENT_ENABLE		    mBIT(7)
#define	VXGE_HAL_DMQ_IR_INT_NUMBER(val)			    vBIT(val, 9, 7)
#define	VXGE_HAL_DMQ_IR_INT_BITMAP(val)			    vBIT(val, 16, 16)
/* 0x011a0 */	u64	dmq_bwr_init_add;
#define	VXGE_HAL_DMQ_BWR_INIT_ADD_HOST(val)		    vBIT(val, 0, 64)
/* 0x011a8 */	u64	dmq_bwr_init_byte;
#define	VXGE_HAL_DMQ_BWR_INIT_BYTE_COUNT(val)		    vBIT(val, 0, 32)
/* 0x011b0 */	u64	dmq_ir;
#define	VXGE_HAL_DMQ_IR_POLICY(val)			    vBIT(val, 0, 8)
/* 0x011b8 */	u64	umq_int;
#define	VXGE_HAL_UMQ_INT_IMMED_ENABLE			    mBIT(6)
#define	VXGE_HAL_UMQ_INT_EVENT_ENABLE			    mBIT(7)
#define	VXGE_HAL_UMQ_INT_NUMBER(val)			    vBIT(val, 9, 7)
#define	VXGE_HAL_UMQ_INT_BITMAP(val)			    vBIT(val, 16, 16)
/* 0x011c0 */	u64	umq_mr2vp_bwr_pfch_init;
#define	VXGE_HAL_UMQ_MR2VP_BWR_PFCH_INIT_NUMBER(val)	    vBIT(val, 0, 8)
/* 0x011c8 */	u64	umq_bwr_pfch_ctrl;
#define	VXGE_HAL_UMQ_BWR_PFCH_CTRL_POLL_EN		    mBIT(3)
/* 0x011d0 */	u64	umq_mr2vp_bwr_eol;
#define	VXGE_HAL_UMQ_MR2VP_BWR_EOL_POLL_LATENCY(val)	    vBIT(val, 32, 32)
/* 0x011d8 */	u64	umq_bwr_init_add;
#define	VXGE_HAL_UMQ_BWR_INIT_ADD_HOST(val)		    vBIT(val, 0, 64)
/* 0x011e0 */	u64	umq_bwr_init_byte;
#define	VXGE_HAL_UMQ_BWR_INIT_BYTE_COUNT(val)		    vBIT(val, 0, 32)
/* 0x011e8 */	u64	gendma_int;
#define	VXGE_HAL_GENDMA_INT_IMMED_ENABLE		    mBIT(6)
#define	VXGE_HAL_GENDMA_INT_EVENT_ENABLE		    mBIT(7)
#define	VXGE_HAL_GENDMA_INT_NUMBER(val)			    vBIT(val, 9, 7)
#define	VXGE_HAL_GENDMA_INT_BITMAP(val)			    vBIT(val, 16, 16)
/* 0x011f0 */	u64	umqdmq_ir_init_notify;
#define	VXGE_HAL_UMQDMQ_IR_INIT_NOTIFY_PULSE		    mBIT(3)
/* 0x011f8 */	u64	dmq_init_notify;
#define	VXGE_HAL_DMQ_INIT_NOTIFY_PULSE			    mBIT(3)
/* 0x01200 */	u64	umq_init_notify;
#define	VXGE_HAL_UMQ_INIT_NOTIFY_PULSE			    mBIT(3)
	u8	unused01380[0x01380 - 0x01208];

/* 0x01380 */	u64	tpa_cfg;
#define	VXGE_HAL_TPA_CFG_IGNORE_FRAME_ERR		    mBIT(3)
#define	VXGE_HAL_TPA_CFG_IPV6_STOP_SEARCHING		    mBIT(7)
#define	VXGE_HAL_TPA_CFG_L4_PSHDR_PRESENT		    mBIT(11)
#define	VXGE_HAL_TPA_CFG_SUPPORT_MOBILE_IPV6_HDRS	    mBIT(15)
	u8	unused01400[0x01400 - 0x01388];

/* 0x01400 */	u64	tx_vp_reset_discarded_frms;
#define	VXGE_HAL_TX_VP_RESET_DISCARDED_FRMS_TX_VP_RESET_DISCARDED_FRMS(val)\
							    vBIT(val, 48, 16)
	u8	unused01480[0x01480 - 0x01408];

/* 0x01480 */	u64	fau_rpa_vcfg;
#define	VXGE_HAL_FAU_RPA_VCFG_L4_COMP_CSUM		    mBIT(7)
#define	VXGE_HAL_FAU_RPA_VCFG_L3_INCL_CF		    mBIT(11)
#define	VXGE_HAL_FAU_RPA_VCFG_L3_COMP_CSUM		    mBIT(15)
	u8	unused014a8[0x014a8 - 0x01488];

/* 0x014a8 */	u64	fau_adaptive_lro_filter_ctrl;
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_CTRL_IP_FILTER_EN  mBIT(0)
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_CTRL_IP_MODE	    mBIT(1)
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_CTRL_VLAN_FILTER_EN mBIT(2)
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_CTRL_IPV4_ADDRESS_A_EN	mBIT(3)
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_CTRL_IPV4_ADDRESS_B_EN	mBIT(4)
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_CTRL_IPV4_ADDRESS_C_EN	mBIT(5)
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_CTRL_IPV4_ADDRESS_D_EN	mBIT(6)
/* 0x014b0 */	u64	fau_adaptive_lro_filter_ip_data0;
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_IP_DATA0_DATA(val) vBIT(val, 0, 64)
/* 0x014b8 */	u64	fau_adaptive_lro_filter_ip_data1;
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_IP_DATA1_DATA(val) vBIT(val, 0, 64)
/* 0x014c0 */	u64	fau_adaptive_lro_filter_vlan_data;
#define	VXGE_HAL_FAU_ADAPTIVE_LRO_FILTER_VLAN_DATA_VLAN_VID(val)\
							    vBIT(val, 0, 12)
	u8	unused014d0[0x014d0 - 0x014c8];

/* 0x014d0 */	u64	dbg_stats_rx_mpa;
#define	VXGE_HAL_DBG_STATS_RX_MPA_CRC_FAIL_FRMS(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_STATS_RX_MPA_MRK_FAIL_FRMS(val)	    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_STATS_RX_MPA_LEN_FAIL_FRMS(val)	    vBIT(val, 32, 16)
/* 0x014d8 */	u64	dbg_stats_rx_fau;
#define	VXGE_HAL_DBG_STATS_RX_FAU_RX_WOL_FRMS(val)	    vBIT(val, 0, 16)
#define	VXGE_HAL_DBG_STATS_RX_FAU_RX_VP_RESET_DISCARDED_FRMS(val)\
							    vBIT(val, 16, 16)
#define	VXGE_HAL_DBG_STATS_RX_FAU_RX_PERMITTED_FRMS(val)    vBIT(val, 32, 32)
	u8	unused014f0[0x014f0 - 0x014e0];

/* 0x014f0 */	u64	fbmc_vp_rdy;
#define	VXGE_HAL_FBMC_VP_RDY_QUEUE_SPAV_FM		    mBIT(0)
	u8	unused01e00[0x01e00 - 0x014f8];

/* 0x01e00 */	u64	vpath_pcipif_int_status;
#define	VXGE_HAL_VPATH_PCIPIF_INT_STATUS_SRPCIM_MSG_TO_VPATH_INT mBIT(3)
#define	VXGE_HAL_VPATH_PCIPIF_INT_STATUS_VPATH_SPARE_R1_INT mBIT(7)
/* 0x01e08 */	u64	vpath_pcipif_int_mask;
	u8	unused01e20[0x01e20 - 0x01e10];

/* 0x01e20 */	u64	srpcim_msg_to_vpath_reg;
#define	VXGE_HAL_SRPCIM_MSG_TO_VPATH_REG_INT		mBIT(3)
/* 0x01e28 */	u64	srpcim_msg_to_vpath_mask;
/* 0x01e30 */	u64	srpcim_msg_to_vpath_alarm;
	u8	unused01ea0[0x01ea0 - 0x01e38];

/* 0x01ea0 */	u64	vpath_to_srpcim_wmsg;
#define	VXGE_HAL_VPATH_TO_SRPCIM_WMSG_WMSG(val)		vBIT(val, 0, 64)
/* 0x01ea8 */	u64	vpath_to_srpcim_wmsg_trig;
#define	VXGE_HAL_VPATH_TO_SRPCIM_WMSG_TRIG		mBIT(0)
	u8	unused02000[0x02000 - 0x01eb0];

/* 0x02000 */	u64	vpath_general_int_status;
#define	VXGE_HAL_VPATH_GENERAL_INT_STATUS_PIC_INT	mBIT(3)
#define	VXGE_HAL_VPATH_GENERAL_INT_STATUS_PCI_INT	mBIT(7)
#define	VXGE_HAL_VPATH_GENERAL_INT_STATUS_WRDMA_INT	mBIT(15)
#define	VXGE_HAL_VPATH_GENERAL_INT_STATUS_XMAC_INT	mBIT(19)
/* 0x02008 */	u64	vpath_general_int_mask;
#define	VXGE_HAL_VPATH_GENERAL_INT_MASK_PIC_INT		mBIT(3)
#define	VXGE_HAL_VPATH_GENERAL_INT_MASK_PCI_INT		mBIT(7)
#define	VXGE_HAL_VPATH_GENERAL_INT_MASK_WRDMA_INT	mBIT(15)
#define	VXGE_HAL_VPATH_GENERAL_INT_MASK_XMAC_INT	mBIT(19)
/* 0x02010 */	u64	vpath_ppif_int_status;
#define	VXGE_HAL_VPATH_PPIF_INT_STATUS_KDFCCTL_ERRORS_INT   mBIT(3)
#define	VXGE_HAL_VPATH_PPIF_INT_STATUS_GENERAL_ERRORS_INT   mBIT(7)
#define	VXGE_HAL_VPATH_PPIF_INT_STATUS_PCI_CONFIG_ERRORS_INT mBIT(11)
#define	VXGE_HAL_VPATH_PPIF_INT_STATUS_MRPCIM_TO_VPATH_ALARM_INT mBIT(15)
#define	VXGE_HAL_VPATH_PPIF_INT_STATUS_SRPCIM_TO_VPATH_ALARM_INT mBIT(19)
/* 0x02018 */	u64	vpath_ppif_int_mask;
/* 0x02020 */	u64	kdfcctl_errors_reg;
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_OVRWR	    mBIT(3)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_OVRWR	    mBIT(7)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_OVRWR	    mBIT(11)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_POISON    mBIT(15)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_POISON    mBIT(19)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_POISON    mBIT(23)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_DMA_ERR   mBIT(31)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_DMA_ERR   mBIT(35)
#define	VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_DMA_ERR   mBIT(39)
/* 0x02028 */	u64	kdfcctl_errors_mask;
/* 0x02030 */	u64	kdfcctl_errors_alarm;
	u8	unused02040[0x02040 - 0x02038];

/* 0x02040 */	u64	general_errors_reg;
#define	VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO0_OVRFLOW    mBIT(3)
#define	VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO1_OVRFLOW    mBIT(7)
#define	VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO2_OVRFLOW    mBIT(11)
#define	VXGE_HAL_GENERAL_ERRORS_REG_STATSB_PIF_CHAIN_ERR    mBIT(15)
#define	VXGE_HAL_GENERAL_ERRORS_REG_STATSB_DROP_TIMEOUT	    mBIT(19)
#define	VXGE_HAL_GENERAL_ERRORS_REG_TGT_ILLEGAL_ACCESS	    mBIT(27)
#define	VXGE_HAL_GENERAL_ERRORS_REG_INI_SERR_DET	    mBIT(31)
/* 0x02048 */	u64	general_errors_mask;
/* 0x02050 */	u64	general_errors_alarm;
/* 0x02058 */	u64	pci_config_errors_reg;
#define	VXGE_HAL_PCI_CONFIG_ERRORS_REG_STATUS_ERR	    mBIT(3)
#define	VXGE_HAL_PCI_CONFIG_ERRORS_REG_UNCOR_ERR	    mBIT(7)
#define	VXGE_HAL_PCI_CONFIG_ERRORS_REG_COR_ERR		    mBIT(11)
/* 0x02060 */	u64	pci_config_errors_mask;
/* 0x02068 */	u64	pci_config_errors_alarm;
/* 0x02070 */	u64	mrpcim_to_vpath_alarm_reg;
#define	VXGE_HAL_MRPCIM_TO_VPATH_ALARM_REG_ALARM	    mBIT(3)
/* 0x02078 */	u64	mrpcim_to_vpath_alarm_mask;
/* 0x02080 */	u64	mrpcim_to_vpath_alarm_alarm;
/* 0x02088 */	u64	srpcim_to_vpath_alarm_reg;
#define	VXGE_HAL_SRPCIM_TO_VPATH_ALARM_REG_PPIF_ALARM(val)  vBIT(val, 0, 17)
/* 0x02090 */	u64	srpcim_to_vpath_alarm_mask;
/* 0x02098 */	u64	srpcim_to_vpath_alarm_alarm;
	u8	unused02108[0x02108 - 0x020a0];

/* 0x02108 */	u64	kdfcctl_status;
#define	VXGE_HAL_KDFCCTL_STATUS_KDFCCTL_FIFO0_PRES(val)	    vBIT(val, 0, 8)
#define	VXGE_HAL_KDFCCTL_STATUS_KDFCCTL_FIFO1_PRES(val)	    vBIT(val, 8, 8)
#define	VXGE_HAL_KDFCCTL_STATUS_KDFCCTL_FIFO2_PRES(val)	    vBIT(val, 16, 8)
#define	VXGE_HAL_KDFCCTL_STATUS_KDFCCTL_FIFO0_OVRWR(val)    vBIT(val, 24, 8)
#define	VXGE_HAL_KDFCCTL_STATUS_KDFCCTL_FIFO1_OVRWR(val)    vBIT(val, 32, 8)
#define	VXGE_HAL_KDFCCTL_STATUS_KDFCCTL_FIFO2_OVRWR(val)    vBIT(val, 40, 8)
/* 0x02110 */	u64	rsthdlr_status;
#define	VXGE_HAL_RSTHDLR_STATUS_RSTHDLR_CURRENT_RESET	    mBIT(3)
#define	VXGE_HAL_RSTHDLR_STATUS_RSTHDLR_CURRENT_VPIN(val)   vBIT(val, 6, 2)
/* 0x02118 */	u64	fifo0_status;
#define	VXGE_HAL_FIFO0_STATUS_DBLGEN_FIFO0_RDIDX(val)	    vBIT(val, 0, 12)
/* 0x02120 */	u64	fifo1_status;
#define	VXGE_HAL_FIFO1_STATUS_DBLGEN_FIFO1_RDIDX(val)	    vBIT(val, 0, 12)
/* 0x02128 */	u64	fifo2_status;
#define	VXGE_HAL_FIFO2_STATUS_DBLGEN_FIFO2_RDIDX(val)	    vBIT(val, 0, 12)
	u8	unused02158[0x02158 - 0x02130];

/* 0x02158 */	u64	tgt_illegal_access;
#define	VXGE_HAL_TGT_ILLEGAL_ACCESS_SWIF_REGION(val)	    vBIT(val, 1, 7)
	u8	unused02200[0x02200 - 0x02160];

/* 0x02200 */	u64	vpath_general_cfg1;
#define	VXGE_HAL_VPATH_GENERAL_CFG1_TC_VALUE(val)	    vBIT(val, 1, 3)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_DATA_BYTE_SWAPEN	    mBIT(7)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_DATA_FLIPEN		    mBIT(11)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_CTL_BYTE_SWAPEN	    mBIT(15)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_CTL_FLIPEN		    mBIT(23)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_MSIX_ADDR_SWAPEN	    mBIT(51)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_MSIX_ADDR_FLIPEN	    mBIT(55)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_MSIX_DATA_SWAPEN	    mBIT(59)
#define	VXGE_HAL_VPATH_GENERAL_CFG1_MSIX_DATA_FLIPEN	    mBIT(63)
/* 0x02208 */	u64	vpath_general_cfg2;
#define	VXGE_HAL_VPATH_GENERAL_CFG2_SIZE_QUANTUM(val)	    vBIT(val, 1, 3)
/* 0x02210 */	u64	vpath_general_cfg3;
#define	VXGE_HAL_VPATH_GENERAL_CFG3_IGNORE_VPATH_RST_FOR_INTA mBIT(3)
	u8	unused02220[0x02220 - 0x02218];

/* 0x02220 */	u64	kdfcctl_cfg0;
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO0		    mBIT(1)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO1		    mBIT(2)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO2		    mBIT(3)
#define	VXGE_HAL_KDFCCTL_CFG0_BIT_FLIPEN_FIFO0		    mBIT(5)
#define	VXGE_HAL_KDFCCTL_CFG0_BIT_FLIPEN_FIFO1		    mBIT(6)
#define	VXGE_HAL_KDFCCTL_CFG0_BIT_FLIPEN_FIFO2		    mBIT(7)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE0_FIFO0	    mBIT(9)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE0_FIFO1	    mBIT(10)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE0_FIFO2	    mBIT(11)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE1_FIFO0	    mBIT(13)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE1_FIFO1	    mBIT(14)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE1_FIFO2	    mBIT(15)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE2_FIFO0	    mBIT(17)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE2_FIFO1	    mBIT(18)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE2_FIFO2	    mBIT(19)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE3_FIFO0	    mBIT(21)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE3_FIFO1	    mBIT(22)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE3_FIFO2	    mBIT(23)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE4_FIFO0	    mBIT(25)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE4_FIFO1	    mBIT(26)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE4_FIFO2	    mBIT(27)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE5_FIFO0	    mBIT(29)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE5_FIFO1	    mBIT(30)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE5_FIFO2	    mBIT(31)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE6_FIFO0	    mBIT(33)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE6_FIFO1	    mBIT(34)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE6_FIFO2	    mBIT(35)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE7_FIFO0	    mBIT(37)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE7_FIFO1	    mBIT(38)
#define	VXGE_HAL_KDFCCTL_CFG0_BYTE_MASK_BYTE7_FIFO2	    mBIT(39)
/* 0x02228 */	u64	dblgen_cfg0;
#define	VXGE_HAL_DBLGEN_CFG0_NO_OF_QWORDS(val)		    vBIT(val, 0, 12)
#define	VXGE_HAL_DBLGEN_CFG0_EN_DMA			    mBIT(15)
#define	VXGE_HAL_DBLGEN_CFG0_NO_SNOOP			    mBIT(19)
#define	VXGE_HAL_DBLGEN_CFG0_RELAX_ORD			    mBIT(23)
#define	VXGE_HAL_DBLGEN_CFG0_RD_SPLIT_ON_ADDR		    mBIT(27)
/* 0x02230 */	u64	dblgen_cfg1;
#define	VXGE_HAL_DBLGEN_CFG1_FIFO0_BUFFER_START_ADDR(val)   vBIT(val, 0, 64)
/* 0x02238 */	u64	dblgen_cfg2;
#define	VXGE_HAL_DBLGEN_CFG2_FIFO1_BUFFER_START_ADDR(val)   vBIT(val, 0, 64)
/* 0x02240 */	u64	dblgen_cfg3;
#define	VXGE_HAL_DBLGEN_CFG3_FIFO2_BUFFER_START_ADDR(val)   vBIT(val, 0, 64)
/* 0x02248 */	u64	dblgen_cfg4;
#define	VXGE_HAL_DBLGEN_CFG4_TRIPLET_PRIORITY_VP_NUMBER(val) vBIT(val, 3, 5)
/* 0x02250 */	u64	dblgen_cfg5;
#define	VXGE_HAL_DBLGEN_CFG5_FIFO0_WRIDX(val)		    vBIT(val, 0, 12)
/* 0x02258 */	u64	dblgen_cfg6;
#define	VXGE_HAL_DBLGEN_CFG6_FIFO1_WRIDX(val)		    vBIT(val, 0, 12)
/* 0x02260 */	u64	dblgen_cfg7;
#define	VXGE_HAL_DBLGEN_CFG7_FIFO2_WRIDX(val)		    vBIT(val, 0, 12)
/* 0x02268 */	u64	stats_cfg;
#define	VXGE_HAL_STATS_CFG_START_HOST_ADDR(val)		    vBIT(val, 0, 57)
/* 0x02270 */	u64	interrupt_cfg0;
#define	VXGE_HAL_INTERRUPT_CFG0_MSIX_FOR_RXTI(val)	    vBIT(val, 1, 7)
#define	VXGE_HAL_INTERRUPT_CFG0_GROUP0_MSIX_FOR_TXTI(val)   vBIT(val, 9, 7)
#define	VXGE_HAL_INTERRUPT_CFG0_GROUP1_MSIX_FOR_TXTI(val)   vBIT(val, 17, 7)
#define	VXGE_HAL_INTERRUPT_CFG0_GROUP2_MSIX_FOR_TXTI(val)   vBIT(val, 25, 7)
#define	VXGE_HAL_INTERRUPT_CFG0_GROUP3_MSIX_FOR_TXTI(val)   vBIT(val, 33, 7)
	u8	unused02280[0x02280 - 0x02278];

/* 0x02280 */	u64	interrupt_cfg2;
#define	VXGE_HAL_INTERRUPT_CFG2_ALARM_MAP_TO_MSG(val)	    vBIT(val, 1, 7)
/* 0x02288 */	u64	one_shot_vect0_en;
#define	VXGE_HAL_ONE_SHOT_VECT0_EN_ONE_SHOT_VECT0_EN	    mBIT(3)
/* 0x02290 */	u64	one_shot_vect1_en;
#define	VXGE_HAL_ONE_SHOT_VECT1_EN_ONE_SHOT_VECT1_EN	    mBIT(3)
/* 0x02298 */	u64	one_shot_vect2_en;
#define	VXGE_HAL_ONE_SHOT_VECT2_EN_ONE_SHOT_VECT2_EN	    mBIT(3)
/* 0x022a0 */	u64	one_shot_vect3_en;
#define	VXGE_HAL_ONE_SHOT_VECT3_EN_ONE_SHOT_VECT3_EN	    mBIT(3)
	u8	unused022b0[0x022b0 - 0x022a8];

/* 0x022b0 */	u64	pci_config_access_cfg1;
#define	VXGE_HAL_PCI_CONFIG_ACCESS_CFG1_ADDRESS(val)	    vBIT(val, 0, 12)
#define	VXGE_HAL_PCI_CONFIG_ACCESS_CFG1_SEL_FUNC0	    mBIT(15)
/* 0x022b8 */	u64	pci_config_access_cfg2;
#define	VXGE_HAL_PCI_CONFIG_ACCESS_CFG2_REQ		    mBIT(0)
/* 0x022c0 */	u64	pci_config_access_status;
#define	VXGE_HAL_PCI_CONFIG_ACCESS_STATUS_ACCESS_ERR	    mBIT(0)
#define	VXGE_HAL_PCI_CONFIG_ACCESS_STATUS_DATA(val)	    vBIT(val, 32, 32)
	u8	unused02300[0x02300 - 0x022c8];

/* 0x02300 */	u64	vpath_debug_stats0;
#define	VXGE_HAL_VPATH_DEBUG_STATS0_INI_NUM_MWR_SENT(val)   vBIT(val, 0, 32)
/* 0x02308 */	u64	vpath_debug_stats1;
#define	VXGE_HAL_VPATH_DEBUG_STATS1_INI_NUM_MRD_SENT(val)   vBIT(val, 0, 32)
/* 0x02310 */	u64	vpath_debug_stats2;
#define	VXGE_HAL_VPATH_DEBUG_STATS2_INI_NUM_CPL_RCVD(val)   vBIT(val, 0, 32)
/* 0x02318 */	u64	vpath_debug_stats3;
#define	VXGE_HAL_VPATH_DEBUG_STATS3_INI_NUM_MWR_BYTE_SENT(val) vBIT(val, 0, 64)
/* 0x02320 */	u64	vpath_debug_stats4;
#define	VXGE_HAL_VPATH_DEBUG_STATS4_INI_NUM_CPL_BYTE_RCVD(val) vBIT(val, 0, 64)
/* 0x02328 */	u64	vpath_debug_stats5;
#define	VXGE_HAL_VPATH_DEBUG_STATS5_WRCRDTARB_XOFF(val)	    vBIT(val, 32, 32)
/* 0x02330 */	u64	vpath_debug_stats6;
#define	VXGE_HAL_VPATH_DEBUG_STATS6_RDCRDTARB_XOFF(val)	    vBIT(val, 32, 32)
/* 0x02338 */	u64	vpath_genstats_count01;
#define	VXGE_HAL_VPATH_GENSTATS_COUNT01_PPIF_VPATH_GENSTATS_COUNT1(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_VPATH_GENSTATS_COUNT01_PPIF_VPATH_GENSTATS_COUNT0(val)\
							    vBIT(val, 32, 32)
/* 0x02340 */	u64	vpath_genstats_count23;
#define	VXGE_HAL_VPATH_GENSTATS_COUNT23_PPIF_VPATH_GENSTATS_COUNT3(val)\
							    vBIT(val, 0, 32)
#define	VXGE_HAL_VPATH_GENSTATS_COUNT23_PPIF_VPATH_GENSTATS_COUNT2(val)\
							    vBIT(val, 32, 32)
/* 0x02348 */	u64	vpath_genstats_count4;
#define	VXGE_HAL_VPATH_GENSTATS_COUNT4_PPIF_VPATH_GENSTATS_COUNT4(val)\
							    vBIT(val, 32, 32)
/* 0x02350 */	u64	vpath_genstats_count5;
#define	VXGE_HAL_VPATH_GENSTATS_COUNT5_PPIF_VPATH_GENSTATS_COUNT5(val)\
							    vBIT(val, 32, 32)
	u8	unused02540[0x02540 - 0x02358];

/* 0x02540 */	u64	qcc_pci_cfg;
#define	VXGE_HAL_QCC_PCI_CFG_ADD_PAD_CQE_SPACE		    mBIT(5)
#define	VXGE_HAL_QCC_PCI_CFG_ADD_PAD_WQE		    mBIT(6)
#define	VXGE_HAL_QCC_PCI_CFG_ADD_PAD_SRQIR		    mBIT(7)
#define	VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_CQE_SPACE		    mBIT(13)
#define	VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_WQE		    mBIT(14)
#define	VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_SRQIR		    mBIT(15)
#define	VXGE_HAL_QCC_PCI_CFG_RELAXED_SRQIR		    mBIT(23)
#define	VXGE_HAL_QCC_PCI_CFG_CTL_STR_CQE_SPACE		    mBIT(29)
#define	VXGE_HAL_QCC_PCI_CFG_CTL_STR_WQE		    mBIT(30)
#define	VXGE_HAL_QCC_PCI_CFG_CTL_STR_SRQIR		    mBIT(31)
	u8	unused02600[0x02600 - 0x02548];

/* 0x02600 */	u64	h2l_vpath_config;
#define	VXGE_HAL_H2L_VPATH_CONFIG_OD_PAD_ENABLE		    mBIT(7)
#define	VXGE_HAL_H2L_VPATH_CONFIG_OD_NO_SNOOP		    mBIT(15)
/* 0x02608 */	u64	h2l_zero_byte_read_address;
#define	VXGE_HAL_H2L_ZERO_BYTE_READ_ADDRESS_H2L_ZERO_BYTE_READ_ADDRESS(val)\
							    vBIT(val, 0, 64)
	u8	unused02640[0x02640 - 0x02610];

/* 0x02640 */	u64	ph2l_vp_cfg0;
#define	VXGE_HAL_PH2L_VP_CFG0_NOSNOOP_DATA		    mBIT(7)

} vxge_hal_vpath_reg_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_VPATH_REGS_H */
