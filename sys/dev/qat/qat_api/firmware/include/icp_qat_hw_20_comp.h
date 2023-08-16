/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file icp_qat_hw_2x_comp.h
 * @defgroup ICP QAT HW accessors for using the for 2.x Compression Slice
 * definitions
 * @ingroup icp_qat_hw_2x_comp
 * @description
 *      This file documents definitions for the QAT HW COMP SLICE
 *
 *****************************************************************************/

#ifndef _ICP_QAT_HW_20_COMP_H_
#define _ICP_QAT_HW_20_COMP_H_

#include "icp_qat_hw_20_comp_defs.h" // For HW definitions
#include "icp_qat_fw.h"		     //For Set Field Macros.

#ifdef WIN32
#include <stdlib.h> // built in support for _byteswap_ulong
#define BYTE_SWAP_32 _byteswap_ulong
#else
#define BYTE_SWAP_32 __builtin_bswap32
#endif

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Definition of the hw config csr. This representation has to be further
* processed by the corresponding config build function.
*
*****************************************************************************/
typedef struct icp_qat_hw_comp_20_config_csr_lower_s {
	// Fields programmable directly by the SW.
	icp_qat_hw_comp_20_extended_delay_match_mode_t edmm;
	icp_qat_hw_comp_20_hw_comp_format_t algo;
	icp_qat_hw_comp_20_search_depth_t sd;
	icp_qat_hw_comp_20_hbs_control_t hbs;
	// Fields programmable directly by the FW.
	// Block Drop enable. (Set by FW)
	icp_qat_hw_comp_20_abd_t abd;
	icp_qat_hw_comp_20_lllbd_ctrl_t lllbd;
	// Advanced HW control (Set to default vals)
	icp_qat_hw_comp_20_skip_hash_collision_t hash_col;
	icp_qat_hw_comp_20_skip_hash_update_t hash_update;
	icp_qat_hw_comp_20_byte_skip_t skip_ctrl;

} icp_qat_hw_comp_20_config_csr_lower_t;

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Build the longword as expected by the HW
*
*****************************************************************************/
static inline uint32_t
ICP_QAT_FW_COMP_20_BUILD_CONFIG_LOWER(icp_qat_hw_comp_20_config_csr_lower_t csr)
{
	uint32_t val32 = 0;
	// Programmable values
	QAT_FIELD_SET(val32,
		      csr.algo,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_HW_COMP_FORMAT_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_HW_COMP_FORMAT_MASK);

	QAT_FIELD_SET(val32,
		      csr.sd,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SEARCH_DEPTH_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SEARCH_DEPTH_MASK);

	QAT_FIELD_SET(
	    val32,
	    csr.edmm,
	    ICP_QAT_HW_COMP_20_CONFIG_CSR_EXTENDED_DELAY_MATCH_MODE_BITPOS,
	    ICP_QAT_HW_COMP_20_CONFIG_CSR_EXTENDED_DELAY_MATCH_MODE_MASK);

	QAT_FIELD_SET(val32,
		      csr.hbs,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_HBS_CONTROL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_HBS_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.lllbd,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_MASK);

	QAT_FIELD_SET(val32,
		      csr.hash_col,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_COLLISION_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_COLLISION_MASK);

	QAT_FIELD_SET(val32,
		      csr.hash_update,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_UPDATE_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_UPDATE_MASK);

	QAT_FIELD_SET(val32,
		      csr.skip_ctrl,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_BYTE_SKIP_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_BYTE_SKIP_MASK);
	// Default values.

	QAT_FIELD_SET(val32,
		      csr.abd,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_ABD_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_ABD_MASK);

	QAT_FIELD_SET(val32,
		      csr.lllbd,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_LLLBD_CTRL_MASK);

	return BYTE_SWAP_32(val32);
}

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Definition of the hw config csr. This representation has to be further
* processed by the corresponding config build function.
*
*****************************************************************************/
typedef struct icp_qat_hw_comp_20_config_csr_upper_s {
	icp_qat_hw_comp_20_scb_control_t scb_ctrl;
	icp_qat_hw_comp_20_rmb_control_t rmb_ctrl;
	icp_qat_hw_comp_20_som_control_t som_ctrl;
	icp_qat_hw_comp_20_skip_hash_rd_control_t skip_hash_ctrl;
	icp_qat_hw_comp_20_scb_unload_control_t scb_unload_ctrl;
	icp_qat_hw_comp_20_disable_token_fusion_control_t
	    disable_token_fusion_ctrl;
	icp_qat_hw_comp_20_scb_mode_reset_mask_t scb_mode_reset;
	uint16_t lazy;
	uint16_t nice;
} icp_qat_hw_comp_20_config_csr_upper_t;

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Build the longword as expected by the HW
*
*****************************************************************************/
static inline uint32_t
ICP_QAT_FW_COMP_20_BUILD_CONFIG_UPPER(icp_qat_hw_comp_20_config_csr_upper_t csr)
{
	uint32_t val32 = 0;

	QAT_FIELD_SET(val32,
		      csr.scb_ctrl,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_CONTROL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.rmb_ctrl,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_RMB_CONTROL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_RMB_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.som_ctrl,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SOM_CONTROL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SOM_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.skip_hash_ctrl,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_RD_CONTROL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SKIP_HASH_RD_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.scb_unload_ctrl,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_UNLOAD_CONTROL_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_UNLOAD_CONTROL_MASK);

	QAT_FIELD_SET(
	    val32,
	    csr.disable_token_fusion_ctrl,
	    ICP_QAT_HW_COMP_20_CONFIG_CSR_DISABLE_TOKEN_FUSION_CONTROL_BITPOS,
	    ICP_QAT_HW_COMP_20_CONFIG_CSR_DISABLE_TOKEN_FUSION_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.scb_mode_reset,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_MODE_RESET_MASK_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_SCB_MODE_RESET_MASK_MASK);

	QAT_FIELD_SET(val32,
		      csr.lazy,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_LAZY_PARAM_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_LAZY_PARAM_MASK);

	QAT_FIELD_SET(val32,
		      csr.nice,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_NICE_PARAM_BITPOS,
		      ICP_QAT_HW_COMP_20_CONFIG_CSR_NICE_PARAM_MASK);

	return BYTE_SWAP_32(val32);
}

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Definition of the hw config csr. This representation has to be further
* processed by the corresponding config build function.
*
*****************************************************************************/
typedef struct icp_qat_hw_decomp_20_config_csr_lower_s {
	/* Fields programmable directly by the SW. */
	icp_qat_hw_decomp_20_hbs_control_t hbs;
	/* Advanced HW control (Set to default vals) */
	icp_qat_hw_decomp_20_hw_comp_format_t algo;
} icp_qat_hw_decomp_20_config_csr_lower_t;

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Build the longword as expected by the HW
*
*****************************************************************************/
static inline uint32_t
ICP_QAT_FW_DECOMP_20_BUILD_CONFIG_LOWER(
    icp_qat_hw_decomp_20_config_csr_lower_t csr)
{
	uint32_t val32 = 0;

	QAT_FIELD_SET(val32,
		      csr.hbs,
		      ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HBS_CONTROL_BITPOS,
		      ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HBS_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.algo,
		      ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HW_DECOMP_FORMAT_BITPOS,
		      ICP_QAT_HW_DECOMP_20_CONFIG_CSR_HW_DECOMP_FORMAT_MASK);

	return BYTE_SWAP_32(val32);
}

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Definition of the hw config csr. This representation has to be further
* processed by the corresponding config build function.
*
*****************************************************************************/
typedef struct icp_qat_hw_decomp_20_config_csr_upper_s {
	/* Advanced HW control (Set to default vals) */
	icp_qat_hw_decomp_20_speculative_decoder_control_t sdc;
	icp_qat_hw_decomp_20_mini_cam_control_t mcc;
} icp_qat_hw_decomp_20_config_csr_upper_t;

/**
*****************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*     Build the longword as expected by the HW
*
*****************************************************************************/
static inline uint32_t
ICP_QAT_FW_DECOMP_20_BUILD_CONFIG_UPPER(
    icp_qat_hw_decomp_20_config_csr_upper_t csr)
{
	uint32_t val32 = 0;

	QAT_FIELD_SET(
	    val32,
	    csr.sdc,
	    ICP_QAT_HW_DECOMP_20_CONFIG_CSR_SPECULATIVE_DECODER_CONTROL_BITPOS,
	    ICP_QAT_HW_DECOMP_20_CONFIG_CSR_SPECULATIVE_DECODER_CONTROL_MASK);

	QAT_FIELD_SET(val32,
		      csr.mcc,
		      ICP_QAT_HW_DECOMP_20_CONFIG_CSR_MINI_CAM_CONTROL_BITPOS,
		      ICP_QAT_HW_DECOMP_20_CONFIG_CSR_MINI_CAM_CONTROL_MASK);

	return BYTE_SWAP_32(val32);
}

#endif /* ICP_QAT_HW__2X_COMP_H_ */
