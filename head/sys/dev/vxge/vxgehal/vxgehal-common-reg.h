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

#ifndef	VXGE_HAL_COMMON_REGS_H
#define	VXGE_HAL_COMMON_REGS_H

__EXTERN_BEGIN_DECLS

typedef struct vxge_hal_common_reg_t {

	u8	unused00a00[0x00a00];

/* 0x00a00 */	u64	prc_status1;
#define	VXGE_HAL_PRC_STATUS1_PRC_VP_QUIESCENT(n)	mBIT(n)
/* 0x00a08 */	u64	rxdcm_reset_in_progress;
#define	VXGE_HAL_RXDCM_RESET_IN_PROGRESS_PRC_VP(n)	mBIT(n)
/* 0x00a10 */	u64	replicq_flush_in_progress;
#define	VXGE_HAL_REPLICQ_FLUSH_IN_PROGRESS_NOA_VP(n)	mBIT(n)
/* 0x00a18 */	u64	rxpe_cmds_reset_in_progress;
#define	VXGE_HAL_RXPE_CMDS_RESET_IN_PROGRESS_NOA_VP(n)	mBIT(n)
/* 0x00a20 */	u64	mxp_cmds_reset_in_progress;
#define	VXGE_HAL_MXP_CMDS_RESET_IN_PROGRESS_NOA_VP(n)	mBIT(n)
/* 0x00a28 */	u64	noffload_reset_in_progress;
#define	VXGE_HAL_NOFFLOAD_RESET_IN_PROGRESS_PRC_VP(n)	mBIT(n)
/* 0x00a30 */	u64	rd_req_in_progress;
#define	VXGE_HAL_RD_REQ_IN_PROGRESS_VP(n)	mBIT(n)
/* 0x00a38 */	u64	rd_req_outstanding;
#define	VXGE_HAL_RD_REQ_OUTSTANDING_VP(n)	mBIT(n)
/* 0x00a40 */	u64	kdfc_reset_in_progress;
#define	VXGE_HAL_KDFC_RESET_IN_PROGRESS_NOA_VP(n)	mBIT(n)
	u8	unused00b00[0x00b00 - 0x00a48];

/* 0x00b00 */	u64	one_cfg_vp;
#define	VXGE_HAL_ONE_CFG_VP_RDY(n)	mBIT(n)
/* 0x00b08 */	u64	one_common;
#define	VXGE_HAL_ONE_COMMON_PET_VPATH_RESET_IN_PROGRESS(n)	mBIT(n)
	u8	unused00b80[0x00b80 - 0x00b10];

/* 0x00b80 */	u64	tim_int_en;
#define	VXGE_HAL_TIM_INT_EN_TIM_VP(n)	mBIT(n)
/* 0x00b88 */	u64	tim_set_int_en;
#define	VXGE_HAL_TIM_SET_INT_EN_VP(n)	mBIT(n)
/* 0x00b90 */	u64	tim_clr_int_en;
#define	VXGE_HAL_TIM_CLR_INT_EN_VP(n)	mBIT(n)
/* 0x00b98 */	u64	tim_mask_int_during_reset;
#define	VXGE_HAL_TIM_MASK_INT_DURING_RESET_VPATH(n)	mBIT(n)
/* 0x00ba0 */	u64	tim_reset_in_progress;
#define	VXGE_HAL_TIM_RESET_IN_PROGRESS_TIM_VPATH(n)	mBIT(n)
/* 0x00ba8 */	u64	tim_outstanding_bmap;
#define	VXGE_HAL_TIM_OUTSTANDING_BMAP_TIM_VPATH(n)	mBIT(n)
	u8	unused00c00[0x00c00 - 0x00bb0];

/* 0x00c00 */	u64	msg_reset_in_progress;
#define	VXGE_HAL_MSG_RESET_IN_PROGRESS_MSG_COMPOSITE(val)	vBIT(val, 0, 17)
/* 0x00c08 */	u64	msg_mxp_mr_ready;
#define	VXGE_HAL_MSG_MXP_MR_READY_MP_BOOTED(n)	mBIT(n)
/* 0x00c10 */	u64	msg_uxp_mr_ready;
#define	VXGE_HAL_MSG_UXP_MR_READY_UP_BOOTED(n)	mBIT(n)
/* 0x00c18 */	u64	msg_dmq_noni_rtl_prefetch;
#define	VXGE_HAL_MSG_DMQ_NONI_RTL_PREFETCH_BYPASS_ENABLE(n)	mBIT(n)
/* 0x00c20 */	u64	msg_umq_rtl_bwr;
#define	VXGE_HAL_MSG_UMQ_RTL_BWR_PREFETCH_DISABLE(n)	mBIT(n)
	u8	unused00d00[0x00d00 - 0x00c28];

/* 0x00d00 */	u64	cmn_rsthdlr_cfg0;
#define	VXGE_HAL_CMN_RSTHDLR_CFG0_SW_RESET_VPATH(val)	vBIT(val, 0, 17)
/* 0x00d08 */	u64	cmn_rsthdlr_cfg1;
#define	VXGE_HAL_CMN_RSTHDLR_CFG1_CLR_VPATH_RESET(val)	vBIT(val, 0, 17)
/* 0x00d10 */	u64	cmn_rsthdlr_cfg2;
#define	VXGE_HAL_CMN_RSTHDLR_CFG2_SW_RESET_FIFO0(val)	vBIT(val, 0, 17)
/* 0x00d18 */	u64	cmn_rsthdlr_cfg3;
#define	VXGE_HAL_CMN_RSTHDLR_CFG3_SW_RESET_FIFO1(val)	vBIT(val, 0, 17)
/* 0x00d20 */	u64	cmn_rsthdlr_cfg4;
#define	VXGE_HAL_CMN_RSTHDLR_CFG4_SW_RESET_FIFO2(val)	vBIT(val, 0, 17)
	u8	unused00d40[0x00d40 - 0x00d28];

/* 0x00d40 */	u64	cmn_rsthdlr_cfg8;
#define	VXGE_HAL_CMN_RSTHDLR_CFG8_INCR_VPATH_INST_NUM(val)	vBIT(val, 0, 17)
/* 0x00d48 */	u64	stats_cfg0;
#define	VXGE_HAL_STATS_CFG0_STATS_ENABLE(val)	vBIT(val, 0, 17)
	u8	unused00da8[0x00da8 - 0x00d50];

/* 0x00da8 */	u64	clear_msix_mask_vect[4];
#define	VXGE_HAL_CLEAR_MSIX_MASK_VECT_CLEAR_MSIX_MASK_VECT(val)	vBIT(val, 0, 17)
/* 0x00dc8 */	u64	set_msix_mask_vect[4];
#define	VXGE_HAL_SET_MSIX_MASK_VECT_SET_MSIX_MASK_VECT(val)	vBIT(val, 0, 17)
/* 0x00de8 */	u64	clear_msix_mask_all_vect;
#define	VXGE_HAL_CLEAR_MSIX_MASK_ALL_VECT_CLEAR_MSIX_MASK_ALL_VECT(val)\
								vBIT(val, 0, 17)
/* 0x00df0 */	u64	set_msix_mask_all_vect;
#define	VXGE_HAL_SET_MSIX_MASK_ALL_VECT_SET_MSIX_MASK_ALL_VECT(val)\
								vBIT(val, 0, 17)
/* 0x00df8 */	u64	mask_vector[4];
#define	VXGE_HAL_MASK_VECTOR_MASK_VECTOR(val)	vBIT(val, 0, 17)
/* 0x00e18 */	u64	msix_pending_vector[4];
#define	VXGE_HAL_MSIX_PENDING_VECTOR_MSIX_PENDING_VECTOR(val)	vBIT(val, 0, 17)
/* 0x00e38 */	u64	clr_msix_one_shot_vec[4];
#define	VXGE_HAL_CLR_MSIX_ONE_SHOT_VEC_CLR_MSIX_ONE_SHOT_VEC(val)\
								vBIT(val, 0, 17)
/* 0x00e58 */	u64	titan_asic_id;
#define	VXGE_HAL_TITAN_ASIC_ID_INITIAL_DEVICE_ID(val)		vBIT(val, 0, 16)
#define	VXGE_HAL_TITAN_ASIC_ID_INITIAL_MAJOR_REVISION(val)	vBIT(val, 48, 8)
#define	VXGE_HAL_TITAN_ASIC_ID_INITIAL_MINOR_REVISION(val)	vBIT(val, 56, 8)
/* 0x00e60 */	u64	titan_general_int_status;
#define	VXGE_HAL_TITAN_GENERAL_INT_STATUS_MRPCIM_ALARM_INT	mBIT(0)
#define	VXGE_HAL_TITAN_GENERAL_INT_STATUS_SRPCIM_ALARM_INT	mBIT(1)
#define	VXGE_HAL_TITAN_GENERAL_INT_STATUS_VPATH_ALARM_INT	mBIT(2)
#define	VXGE_HAL_TITAN_GENERAL_INT_STATUS_VPATH_TRAFFIC_INT(val)\
								vBIT(val, 3, 17)
	u8	unused00e70[0x00e70 - 0x00e68];

/* 0x00e70 */	u64	titan_mask_all_int;
#define	VXGE_HAL_TITAN_MASK_ALL_INT_ALARM	mBIT(7)
#define	VXGE_HAL_TITAN_MASK_ALL_INT_TRAFFIC	mBIT(15)
	u8	unused00e80[0x00e80 - 0x00e78];

/* 0x00e80 */	u64	tim_int_status0;
#define	VXGE_HAL_TIM_INT_STATUS0_TIM_INT_STATUS0(val)	vBIT(val, 0, 64)
/* 0x00e88 */	u64	tim_int_mask0;
#define	VXGE_HAL_TIM_INT_MASK0_TIM_INT_MASK0(val)	vBIT(val, 0, 64)
/* 0x00e90 */	u64	tim_int_status1;
#define	VXGE_HAL_TIM_INT_STATUS1_TIM_INT_STATUS1(val)	vBIT(val, 0, 4)
/* 0x00e98 */	u64	tim_int_mask1;
#define	VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(val)	vBIT(val, 0, 4)
/* 0x00ea0 */	u64	rti_int_status;
#define	VXGE_HAL_RTI_INT_STATUS_RTI_INT_STATUS(val)	vBIT(val, 0, 17)
/* 0x00ea8 */	u64	rti_int_mask;
#define	VXGE_HAL_RTI_INT_MASK_RTI_INT_MASK(val)	vBIT(val, 0, 17)
/* 0x00eb0 */	u64	adapter_status;
#define	VXGE_HAL_ADAPTER_STATUS_RTDMA_RTDMA_READY	mBIT(0)
#define	VXGE_HAL_ADAPTER_STATUS_WRDMA_WRDMA_READY	mBIT(1)
#define	VXGE_HAL_ADAPTER_STATUS_KDFC_KDFC_READY	mBIT(2)
#define	VXGE_HAL_ADAPTER_STATUS_TPA_TMAC_BUF_EMPTY	mBIT(3)
#define	VXGE_HAL_ADAPTER_STATUS_RDCTL_PIC_QUIESCENT	mBIT(4)
#define	VXGE_HAL_ADAPTER_STATUS_XGMAC_NETWORK_FAULT	mBIT(5)
#define	VXGE_HAL_ADAPTER_STATUS_ROCRC_OFFLOAD_QUIESCENT	mBIT(6)
#define	VXGE_HAL_ADAPTER_STATUS_G3IF_FB_G3IF_FB_GDDR3_READY	mBIT(7)
#define	VXGE_HAL_ADAPTER_STATUS_G3IF_CM_G3IF_CM_GDDR3_READY	mBIT(8)
#define	VXGE_HAL_ADAPTER_STATUS_RIC_RIC_RUNNING	mBIT(9)
#define	VXGE_HAL_ADAPTER_STATUS_CMG_C_PLL_IN_LOCK	mBIT(10)
#define	VXGE_HAL_ADAPTER_STATUS_XGMAC_X_PLL_IN_LOCK	mBIT(11)
#define	VXGE_HAL_ADAPTER_STATUS_FBIF_M_PLL_IN_LOCK	mBIT(12)
#define	VXGE_HAL_ADAPTER_STATUS_PCC_PCC_IDLE(val)	vBIT(val, 24, 8)
#define	VXGE_HAL_ADAPTER_STATUS_ROCRC_RC_PRC_QUIESCENT(val)	vBIT(val, 44, 8)
/* 0x00eb8 */	u64	gen_ctrl;
#define	VXGE_HAL_GEN_CTRL_SPI_MRPCIM_WR_DIS	mBIT(0)
#define	VXGE_HAL_GEN_CTRL_SPI_MRPCIM_RD_DIS	mBIT(1)
#define	VXGE_HAL_GEN_CTRL_SPI_SRPCIM_WR_DIS	mBIT(2)
#define	VXGE_HAL_GEN_CTRL_SPI_SRPCIM_RD_DIS	mBIT(3)
#define	VXGE_HAL_GEN_CTRL_SPI_DEBUG_DIS	mBIT(4)
#define	VXGE_HAL_GEN_CTRL_SPI_APP_LTSSM_TIMER_DIS	mBIT(5)
#define	VXGE_HAL_GEN_CTRL_SPI_NOT_USED(val)	vBIT(val, 6, 4)
	u8	unused00ed0[0x00ed0 - 0x00ec0];

/* 0x00ed0 */	u64	adapter_ready;
#define	VXGE_HAL_ADAPTER_READY_ADAPTER_READY	mBIT(63)
/* 0x00ed8 */	u64	outstanding_read;
#define	VXGE_HAL_OUTSTANDING_READ_OUTSTANDING_READ(val)	vBIT(val, 0, 17)
/* 0x00ee0 */	u64	vpath_rst_in_prog;
#define	VXGE_HAL_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG(val)	vBIT(val, 0, 17)
/* 0x00ee8 */	u64	vpath_reg_modified;
#define	VXGE_HAL_VPATH_REG_MODIFIED_VPATH_REG_MODIFIED(val)	vBIT(val, 0, 17)
	u8	unused00f40[0x00f40 - 0x00ef0];

/* 0x00f40 */	u64	qcc_reset_in_progress;
#define	VXGE_HAL_QCC_RESET_IN_PROGRESS_QCC_VPATH(n)	mBIT(n)
	u8	unused00fc0[0x00fc0 - 0x00f48];

/* 0x00fc0 */	u64	cp_reset_in_progress;
#define	VXGE_HAL_CP_RESET_IN_PROGRESS_CP_VPATH(n)	mBIT(n)
	u8	unused01000[0x01000 - 0x00fc8];

/* 0x01000 */	u64	h2l_reset_in_progress;
#define	VXGE_HAL_H2L_RESET_IN_PROGRESS_H2L_VPATH(n)	mBIT(n)
	u8	unused01080[0x01080 - 0x01008];

/* 0x01080 */	u64	xgmac_ready;
#define	VXGE_HAL_XGMAC_READY_XMACJ_READY(val)	vBIT(val, 0, 17)
	u8	unused010c0[0x010c0 - 0x01088];

/* 0x010c0 */	u64	fbif_ready;
#define	VXGE_HAL_FBIF_READY_FAU_READY(val)	vBIT(val, 0, 17)
	u8	unused01100[0x01100 - 0x010c8];

/* 0x01100 */	u64	vplane_assignments;
#define	VXGE_HAL_VPLANE_ASSIGNMENTS_VPLANE_ASSIGNMENTS(val)	vBIT(val, 3, 5)
/* 0x01108 */	u64	vpath_assignments;
#define	VXGE_HAL_VPATH_ASSIGNMENTS_VPATH_ASSIGNMENTS(val)	vBIT(val, 0, 17)
/* 0x01110 */	u64	resource_assignments;
#define	VXGE_HAL_RESOURCE_ASSIGNMENTS_RESOURCE_ASSIGNMENTS(val)	vBIT(val, 0, 17)
/* 0x01118 */	u64	host_type_assignments;
#define	VXGE_HAL_HOST_TYPE_ASSIGNMENTS_HOST_TYPE_ASSIGNMENTS(val)\
								vBIT(val, 5, 3)
/* 0x01120 */	u64	debug_assignments;
#define	VXGE_HAL_DEBUG_ASSIGNMENTS_VHLABEL(val)			vBIT(val, 3, 5)
#define	VXGE_HAL_DEBUG_ASSIGNMENTS_VPLANE(val)			vBIT(val, 11, 5)
#define	VXGE_HAL_DEBUG_ASSIGNMENTS_FUNC(val)			vBIT(val, 19, 5)

/* 0x01128 */	u64	max_resource_assignments;
#define	VXGE_HAL_MAX_RESOURCE_ASSIGNMENTS_PCI_MAX_VPLANE(val)	vBIT(val, 3, 5)
#define	VXGE_HAL_MAX_RESOURCE_ASSIGNMENTS_PCI_MAX_VPATHS(val)	vBIT(val, 11, 5)
/* 0x01130 */	u64	pf_vpath_assignments;
#define	VXGE_HAL_PF_VPATH_ASSIGNMENTS_PF_VPATH_ASSIGNMENTS(val)	vBIT(val, 0, 17)
	u8	unused01200[0x01200 - 0x01138];

/* 0x01200 */	u64	rts_access_icmp;
#define	VXGE_HAL_RTS_ACCESS_ICMP_EN(val)	vBIT(val, 0, 17)
/* 0x01208 */	u64	rts_access_tcpsyn;
#define	VXGE_HAL_RTS_ACCESS_TCPSYN_EN(val)	vBIT(val, 0, 17)
/* 0x01210 */	u64	rts_access_zl4pyld;
#define	VXGE_HAL_RTS_ACCESS_ZL4PYLD_EN(val)	vBIT(val, 0, 17)
/* 0x01218 */	u64	rts_access_l4prtcl_tcp;
#define	VXGE_HAL_RTS_ACCESS_L4PRTCL_TCP_EN(val)	vBIT(val, 0, 17)
/* 0x01220 */	u64	rts_access_l4prtcl_udp;
#define	VXGE_HAL_RTS_ACCESS_L4PRTCL_UDP_EN(val)	vBIT(val, 0, 17)
/* 0x01228 */	u64	rts_access_l4prtcl_flex;
#define	VXGE_HAL_RTS_ACCESS_L4PRTCL_FLEX_EN(val)	vBIT(val, 0, 17)
/* 0x01230 */	u64	rts_access_ipfrag;
#define	VXGE_HAL_RTS_ACCESS_IPFRAG_EN(val)	vBIT(val, 0, 17)

	u8	unused01238[0x01248 - 0x01238];

} vxge_hal_common_reg_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_COMMON_REGS_H */
