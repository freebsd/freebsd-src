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

#ifndef	VXGE_HAL_SRPCIM_REGS_H
#define	VXGE_HAL_SRPCIM_REGS_H

__EXTERN_BEGIN_DECLS

typedef struct vxge_hal_srpcim_reg_t {

/* 0x00000 */	u64	tim_mr2sr_resource_assignment_vh;
#define	VXGE_HAL_TIM_MR2SR_RESOURCE_ASSIGNMENT_VH_BMAP_ROOT(val)\
							    vBIT(val, 0, 32)
	u8	unused00100[0x00100 - 0x00008];

/* 0x00100 */	u64	srpcim_pcipif_int_status;
#define	VXGE_HAL_SRPCIM_PCIPIF_INT_STATUS_MRPCIM_MSG_INT mBIT(3)
#define	VXGE_HAL_SRPCIM_PCIPIF_INT_STATUS_VPATH_MSG_VPATH_MSG_INT mBIT(7)
#define	VXGE_HAL_SRPCIM_PCIPIF_INT_STATUS_SRPCIM_SPARE_R1_SRPCIM_SPARE_R1_INT\
							    mBIT(11)
/* 0x00108 */	u64	srpcim_pcipif_int_mask;
/* 0x00110 */	u64	mrpcim_msg_reg;
#define	VXGE_HAL_MRPCIM_MSG_REG_SWIF_MRPCIM_TO_SRPCIM_RMSG_INT	mBIT(3)
/* 0x00118 */	u64	mrpcim_msg_mask;
/* 0x00120 */	u64	mrpcim_msg_alarm;
/* 0x00128 */	u64	vpath_msg_reg;
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH0_TO_SRPCIM_RMSG_INT	mBIT(0)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH1_TO_SRPCIM_RMSG_INT	mBIT(1)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH2_TO_SRPCIM_RMSG_INT	mBIT(2)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH3_TO_SRPCIM_RMSG_INT	mBIT(3)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH4_TO_SRPCIM_RMSG_INT	mBIT(4)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH5_TO_SRPCIM_RMSG_INT	mBIT(5)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH6_TO_SRPCIM_RMSG_INT	mBIT(6)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH7_TO_SRPCIM_RMSG_INT	mBIT(7)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH8_TO_SRPCIM_RMSG_INT	mBIT(8)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH9_TO_SRPCIM_RMSG_INT	mBIT(9)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH10_TO_SRPCIM_RMSG_INT	mBIT(10)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH11_TO_SRPCIM_RMSG_INT	mBIT(11)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH12_TO_SRPCIM_RMSG_INT	mBIT(12)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH13_TO_SRPCIM_RMSG_INT	mBIT(13)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH14_TO_SRPCIM_RMSG_INT	mBIT(14)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH15_TO_SRPCIM_RMSG_INT	mBIT(15)
#define	VXGE_HAL_VPATH_MSG_REG_SWIF_VPATH16_TO_SRPCIM_RMSG_INT	mBIT(16)
/* 0x00130 */	u64	vpath_msg_mask;
/* 0x00138 */	u64	vpath_msg_alarm;
	u8	unused00158[0x00158 - 0x00140];

/* 0x00158 */	u64	vf_bargrp_no;
#define	VXGE_HAL_VF_BARGRP_NO_IDENTIFIER_LSB_FOR_BAR0(val)  vBIT(val, 11, 5)
#define	VXGE_HAL_VF_BARGRP_NO_IDENTIFIER_LSB_FOR_BAR1(val)  vBIT(val, 19, 5)
#define	VXGE_HAL_VF_BARGRP_NO_IDENTIFIER_LSB_FOR_BAR2(val)  vBIT(val, 26, 6)
#define	VXGE_HAL_VF_BARGRP_NO_FIRST_VF_OFFSET(val)	    vBIT(val, 32, 4)
#define	VXGE_HAL_VF_BARGRP_NO_MASK(val)			    vBIT(val, 36, 4)
/* 0x00160 */	u64	srpcim_to_mrpcim_wmsg;
#define	VXGE_HAL_SRPCIM_TO_MRPCIM_WMSG_SRPCIM_TO_MRPCIM_WMSG(val)\
							    vBIT(val, 0, 64)
/* 0x00168 */	u64	srpcim_to_mrpcim_wmsg_trig;
#define	VXGE_HAL_SRPCIM_TO_MRPCIM_WMSG_TRIG_SRPCIM_TO_MRPCIM_WMSG_TRIG mBIT(0)
/* 0x00170 */	u64	mrpcim_to_srpcim_rmsg;
#define	VXGE_HAL_MRPCIM_TO_SRPCIM_RMSG_SWIF_MRPCIM_TO_SRPCIM_RMSG(val)\
							    vBIT(val, 0, 64)
/* 0x00178 */	u64	vpath_to_srpcim_rmsg_sel;
#define	VXGE_HAL_VPATH_TO_SRPCIM_RMSG_SEL_SEL(val)	    vBIT(val, 0, 5)
/* 0x00180 */	u64	vpath_to_srpcim_rmsg;
#define	VXGE_HAL_VPATH_TO_SRPCIM_RMSG_SWIF_VPATH_TO_SRPCIM_RMSG(val)\
							    vBIT(val, 0, 64)
	u8	unused00200[0x00200 - 0x00188];

/* 0x00200 */	u64	srpcim_general_int_status;
#define	VXGE_HAL_SRPCIM_GENERAL_INT_STATUS_PIC_INT	    mBIT(0)
#define	VXGE_HAL_SRPCIM_GENERAL_INT_STATUS_PCI_INT	    mBIT(3)
#define	VXGE_HAL_SRPCIM_GENERAL_INT_STATUS_XMAC_INT	    mBIT(7)
	u8	unused00210[0x00210 - 0x00208];

/* 0x00210 */	u64	srpcim_general_int_mask;
#define	VXGE_HAL_SRPCIM_GENERAL_INT_MASK_PIC_INT	    mBIT(0)
#define	VXGE_HAL_SRPCIM_GENERAL_INT_MASK_PCI_INT	    mBIT(3)
#define	VXGE_HAL_SRPCIM_GENERAL_INT_MASK_XMAC_INT	    mBIT(7)
	u8	unused00220[0x00220 - 0x00218];

/* 0x00220 */	u64	srpcim_ppif_int_status;
#define	VXGE_HAL_SRPCIM_PPIF_INT_STATUS_SRPCIM_GEN_ERRORS_INT\
							    mBIT(3)
#define	VXGE_HAL_SRPCIM_PPIF_INT_STATUS_MRPCIM_TO_SRPCIM_ALARM mBIT(7)
#define	VXGE_HAL_SRPCIM_PPIF_INT_STATUS_VPATH_TO_SRPCIM_ALARM_INT mBIT(11)
/* 0x00228 */	u64	srpcim_ppif_int_mask;
/* 0x00230 */	u64	srpcim_gen_errors_reg;
#define	VXGE_HAL_SRPCIM_GEN_ERRORS_REG_PCICONFIG_PF_STATUS_ERR	mBIT(3)
#define	VXGE_HAL_SRPCIM_GEN_ERRORS_REG_PCICONFIG_PF_UNCOR_ERR	mBIT(7)
#define	VXGE_HAL_SRPCIM_GEN_ERRORS_REG_PCICONFIG_PF_COR_ERR	mBIT(11)
#define	VXGE_HAL_SRPCIM_GEN_ERRORS_REG_INTCTRL_SCHED_INT	mBIT(15)
#define	VXGE_HAL_SRPCIM_GEN_ERRORS_REG_INI_SERR_DET	    mBIT(19)
#define	VXGE_HAL_SRPCIM_GEN_ERRORS_REG_TGT_PF_ILLEGAL_ACCESS	mBIT(23)
/* 0x00238 */	u64	srpcim_gen_errors_mask;
/* 0x00240 */	u64	srpcim_gen_errors_alarm;
/* 0x00248 */	u64	mrpcim_to_srpcim_alarm_reg;
#define	VXGE_HAL_MRPCIM_TO_SRPCIM_ALARM_REG_PPIF_MRPCIM_TO_SRPCIM_ALARM	mBIT(3)
/* 0x00250 */	u64	mrpcim_to_srpcim_alarm_mask;
/* 0x00258 */	u64	mrpcim_to_srpcim_alarm_alarm;
/* 0x00260 */	u64	vpath_to_srpcim_alarm_reg;
#define	VXGE_HAL_VPATH_TO_SRPCIM_ALARM_REG_PPIF_VPATH_TO_SRPCIM_ALARM(val)\
							    vBIT(val, 0, 17)
/* 0x00268 */	u64	vpath_to_srpcim_alarm_mask;
/* 0x00270 */	u64	vpath_to_srpcim_alarm_alarm;
	u8	unused00280[0x00280 - 0x00278];

/* 0x00280 */	u64	pf_sw_reset;
#define	VXGE_HAL_PF_SW_RESET_PF_SW_RESET(val)		    vBIT(val, 0, 8)
/* 0x00288 */	u64	srpcim_general_cfg1;
#define	VXGE_HAL_SRPCIM_GENERAL_CFG1_BOOT_BYTE_SWAPEN	    mBIT(19)
#define	VXGE_HAL_SRPCIM_GENERAL_CFG1_BOOT_BIT_FLIPEN	    mBIT(23)
#define	VXGE_HAL_SRPCIM_GENERAL_CFG1_MSIX_ADDR_SWAPEN	    mBIT(27)
#define	VXGE_HAL_SRPCIM_GENERAL_CFG1_MSIX_ADDR_FLIPEN	    mBIT(31)
#define	VXGE_HAL_SRPCIM_GENERAL_CFG1_MSIX_DATA_SWAPEN	    mBIT(35)
#define	VXGE_HAL_SRPCIM_GENERAL_CFG1_MSIX_DATA_FLIPEN	    mBIT(39)
/* 0x00290 */	u64	srpcim_interrupt_cfg1;
#define	VXGE_HAL_SRPCIM_INTERRUPT_CFG1_ALARM_MAP_TO_MSG(val) vBIT(val, 1, 7)
#define	VXGE_HAL_SRPCIM_INTERRUPT_CFG1_TRAFFIC_CLASS(val)   vBIT(val, 9, 3)
/* 0x00298 */	u64	srpcim_interrupt_cfg2;
#define	VXGE_HAL_SRPCIM_INTERRUPT_CFG2_MSIX_FOR_SCHED_INT(val)\
							    vBIT(val, 1, 7)
#define	VXGE_HAL_SRPCIM_INTERRUPT_CFG2_SCHED_ONE_SHOT	    mBIT(11)
#define	VXGE_HAL_SRPCIM_INTERRUPT_CFG2_SCHED_TIMER_EN	    mBIT(15)
#define	VXGE_HAL_SRPCIM_INTERRUPT_CFG2_SCHED_INT_PERIOD(val) vBIT(val, 32, 32)
	u8	unused002a8[0x002a8 - 0x002a0];

/* 0x002a8 */	u64	srpcim_clear_msix_mask;
#define	VXGE_HAL_SRPCIM_CLEAR_MSIX_MASK_SRPCIM_CLEAR_MSIX_MASK mBIT(0)
/* 0x002b0 */	u64	srpcim_set_msix_mask;
#define	VXGE_HAL_SRPCIM_SET_MSIX_MASK_SRPCIM_SET_MSIX_MASK  mBIT(0)
/* 0x002b8 */	u64	srpcim_clr_msix_one_shot;
#define	VXGE_HAL_SRPCIM_CLR_MSIX_ONE_SHOT_SRPCIM_CLR_MSIX_ONE_SHOT mBIT(0)
/* 0x002c0 */	u64	srpcim_rst_in_prog;
#define	VXGE_HAL_SRPCIM_RST_IN_PROG_SRPCIM_RST_IN_PROG	    mBIT(7)
/* 0x002c8 */	u64	srpcim_reg_modified;
#define	VXGE_HAL_SRPCIM_REG_MODIFIED_SRPCIM_REG_MODIFIED    mBIT(7)
/* 0x002d0 */	u64	tgt_pf_illegal_access;
#define	VXGE_HAL_TGT_PF_ILLEGAL_ACCESS_SWIF_REGION(val)	    vBIT(val, 1, 7)
/* 0x002d8 */	u64	srpcim_msix_status;
#define	VXGE_HAL_SRPCIM_MSIX_STATUS_INTCTL_SRPCIM_MSIX_MASK mBIT(3)
#define	VXGE_HAL_SRPCIM_MSIX_STATUS_INTCTL_SRPCIM_MSIX_PENDING_VECTOR mBIT(7)
	u8	unused00318[0x00318 - 0x002e0];

/* 0x00318 */	u64	usdc_vpl;
#define	VXGE_HAL_USDC_VPL_SGRP_OWN(val)			    vBIT(val, 0, 32)
	u8	unused00600[0x00600 - 0x00320];

/* 0x00600 */	u64	one_cfg_sr_copy;
#define	VXGE_HAL_ONE_CFG_SR_COPY_ONE_CFG_RDY		    mBIT(7)
/* 0x00608 */	u64	sgrp_allocated;
#define	VXGE_HAL_SGRP_ALLOCATED_SGRP_ALLOC(val)		    vBIT(val, 0, 64)
/* 0x00610 */	u64	sgrp_iwarp_lro_allocated;
#define	VXGE_HAL_SGRP_IWARP_LRO_ALLOCATED_ENABLE_IWARP	    mBIT(7)
#define	VXGE_HAL_SGRP_IWARP_LRO_ALLOCATED_LAST_IWARP_SGRP(val) vBIT(val, 11, 5)
	u8	unused00880[0x00880 - 0x00618];

/* 0x00880 */	u64	xgmac_sr_int_status;
#define	VXGE_HAL_XGMAC_SR_INT_STATUS_ASIC_NTWK_SR_ERR_INT   mBIT(3)
/* 0x00888 */	u64	xgmac_sr_int_mask;
/* 0x00890 */	u64	asic_ntwk_sr_err_reg;
#define	VXGE_HAL_ASIC_NTWK_SR_ERR_REG_XMACJ_NTWK_SUSTAINED_FAULT mBIT(3)
#define	VXGE_HAL_ASIC_NTWK_SR_ERR_REG_XMACJ_NTWK_SUSTAINED_OK mBIT(7)
#define	VXGE_HAL_ASIC_NTWK_SR_ERR_REG_XMACJ_NTWK_SUSTAINED_FAULT_OCCURRED\
							    mBIT(11)
#define	VXGE_HAL_ASIC_NTWK_SR_ERR_REG_XMACJ_NTWK_SUSTAINED_OK_OCCURRED mBIT(15)
/* 0x00898 */	u64	asic_ntwk_sr_err_mask;
/* 0x008a0 */	u64	asic_ntwk_sr_err_alarm;
	u8	unused008c0[0x008c0 - 0x008a8];

/* 0x008c0 */	u64	xmac_vsport_choices_sr_clone;
#define	VXGE_HAL_XMAC_VSPORT_CHOICES_SR_CLONE_VSPORT_VECTOR(val)\
							    vBIT(val, 0, 17)
	u8	unused00900[0x00900 - 0x008c8];

/* 0x00900 */	u64	mr_rqa_top_prty_for_vh;
#define	VXGE_HAL_MR_RQA_TOP_PRTY_FOR_VH_RQA_TOP_PRTY_FOR_VH(val)\
							    vBIT(val, 59, 5)
/* 0x00908 */	u64	umq_vh_data_list_empty;
#define	VXGE_HAL_UMQ_VH_DATA_LIST_EMPTY_ROCRC_UMQ_VH_DATA_LIST_EMPTY mBIT(0)
/* 0x00910 */	u64	wde_cfg;
#define	VXGE_HAL_WDE_CFG_NS0_FORCE_MWB_START		    mBIT(0)
#define	VXGE_HAL_WDE_CFG_NS0_FORCE_MWB_END		    mBIT(1)
#define	VXGE_HAL_WDE_CFG_NS0_FORCE_QB_START		    mBIT(2)
#define	VXGE_HAL_WDE_CFG_NS0_FORCE_QB_END		    mBIT(3)
#define	VXGE_HAL_WDE_CFG_NS0_FORCE_MPSB_START		    mBIT(4)
#define	VXGE_HAL_WDE_CFG_NS0_FORCE_MPSB_END		    mBIT(5)
#define	VXGE_HAL_WDE_CFG_NS0_MWB_OPT_EN			    mBIT(6)
#define	VXGE_HAL_WDE_CFG_NS0_QB_OPT_EN			    mBIT(7)
#define	VXGE_HAL_WDE_CFG_NS0_MPSB_OPT_EN		    mBIT(8)
#define	VXGE_HAL_WDE_CFG_NS1_FORCE_MWB_START		    mBIT(9)
#define	VXGE_HAL_WDE_CFG_NS1_FORCE_MWB_END		    mBIT(10)
#define	VXGE_HAL_WDE_CFG_NS1_FORCE_QB_START		    mBIT(11)
#define	VXGE_HAL_WDE_CFG_NS1_FORCE_QB_END		    mBIT(12)
#define	VXGE_HAL_WDE_CFG_NS1_FORCE_MPSB_START		    mBIT(13)
#define	VXGE_HAL_WDE_CFG_NS1_FORCE_MPSB_END		    mBIT(14)
#define	VXGE_HAL_WDE_CFG_NS1_MWB_OPT_EN			    mBIT(15)
#define	VXGE_HAL_WDE_CFG_NS1_QB_OPT_EN			    mBIT(16)
#define	VXGE_HAL_WDE_CFG_NS1_MPSB_OPT_EN		    mBIT(17)
#define	VXGE_HAL_WDE_CFG_DISABLE_QPAD_FOR_UNALIGNED_ADDR    mBIT(19)
#define	VXGE_HAL_WDE_CFG_ALIGNMENT_PREFERENCE(val)	    vBIT(val, 30, 2)
#define	VXGE_HAL_WDE_CFG_MEM_WORD_SIZE(val)		    vBIT(val, 46, 2)

} vxge_hal_srpcim_reg_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_SRPCIM_REGS_H */
