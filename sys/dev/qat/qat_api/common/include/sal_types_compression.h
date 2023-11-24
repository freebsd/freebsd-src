/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 ***************************************************************************
 * @file sal_types_compression.h
 *
 * @ingroup SalCtrl
 *
 * Generic compression instance type definition
 *
 ***************************************************************************/
#ifndef SAL_TYPES_COMPRESSION_H_
#define SAL_TYPES_COMPRESSION_H_

#include "cpa_dc.h"
#include "cpa_dc_dp.h"
#include "lac_sal_types.h"
#include "icp_qat_hw.h"
#include "icp_buffer_desc.h"

#include "lac_mem_pools.h"
#include "icp_adf_transport.h"

#define DC_NUM_RX_RINGS (1)
#define DC_NUM_COMPRESSION_LEVELS (CPA_DC_L12)

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      Compression device specific data
 *
 * @description
 *      Contains device specific information for a compression service.
 *
 *****************************************************************************/
typedef struct sal_compression_device_data {
	/* Device specific minimum output buffer size for static compression */
	Cpa32U minOutputBuffSize;

	/* Device specific minimum output buffer size for dynamic compression */
	Cpa32U minOutputBuffSizeDynamic;

	/* Enable/disable secureRam/acceleratorRam for intermediate buffers*/
	Cpa8U useDevRam;

	/* When set, implies device can decompress interim odd byte length
	 * stateful decompression requests.
	 */
	CpaBoolean oddByteDecompInterim;

	/* When set, implies device can decompress odd byte length
	 * stateful decompression requests when bFinal is absent
	 */
	CpaBoolean oddByteDecompNobFinal;

	/* Flag to indicate if translator slice overflow is supported */
	CpaBoolean translatorOverflow;

	/* Flag to enable/disable delayed match mode */
	icp_qat_hw_compression_delayed_match_t enableDmm;

	Cpa32U inflateContextSize;
	Cpa8U highestHwCompressionDepth;

	/* Mask that reports supported window sizes for comp/decomp */
	Cpa8U windowSizeMask;

	/* List representing compression levels that are the first to have
	   a unique search depth. */
	CpaBoolean uniqueCompressionLevels[DC_NUM_COMPRESSION_LEVELS + 1];
	Cpa8U numCompressionLevels;

	/* Flag to indicate CompressAndVerifyAndRecover feature support */
	CpaBoolean cnvnrSupported;

	/* When set, implies device supports ASB_ENABLE */
	CpaBoolean asbEnableSupport;
} sal_compression_device_data_t;

/**
 *****************************************************************************
 * @ingroup SalCtrl
 *      Compression specific Service Container
 *
 * @description
 *      Contains information required per compression service instance.
 *
 *****************************************************************************/
typedef struct sal_compression_service_s {
	/* An instance of the Generic Service Container */
	sal_service_t generic_service_info;

	/* Memory pool ID used for compression */
	lac_memory_pool_id_t compression_mem_pool;

	/* Pointer to an array of atomic stats for compression */
	QatUtilsAtomic *pCompStatsArr;

	/* Size of the DRAM intermediate buffer in bytes */
	Cpa64U minInterBuffSizeInBytes;

	/* Number of DRAM intermediate buffers */
	Cpa16U numInterBuffs;

	/* Address of the array of DRAM intermediate buffers*/
	icp_qat_addr_width_t *pInterBuffPtrsArray;
	CpaPhysicalAddr pInterBuffPtrsArrayPhyAddr;

	icp_comms_trans_handle trans_handle_compression_tx;
	icp_comms_trans_handle trans_handle_compression_rx;

	/* Maximum number of in flight requests */
	Cpa32U maxNumCompConcurrentReq;

	/* Callback function defined for the DcDp API compression session */
	CpaDcDpCallbackFn pDcDpCb;

	/* Config info */
	Cpa16U acceleratorNum;
	Cpa16U bankNum;
	Cpa16U pkgID;
	Cpa16U isPolled;
	Cpa32U coreAffinity;
	Cpa32U nodeAffinity;

	sal_compression_device_data_t comp_device_data;

	/* Statistics handler */
	debug_file_info_t *debug_file;
} sal_compression_service_t;

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *  This function returns a valid compression instance handle for the system
 *  if it exists.
 *
 *  @performance
 *    To avoid calling this function the user of the QA api should not use
 *    instanceHandle = CPA_INSTANCE_HANDLE_SINGLE.
 *
 * @context
 *    This function is called whenever instanceHandle =
 *    CPA_INSTANCE_HANDLE_SINGLE at the QA Dc api.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @retval   Pointer to first compression instance handle or NULL if no
 *           compression instances in the system.
 *
 *************************************************************************/
CpaInstanceHandle dcGetFirstHandle(void);

#endif /*SAL_TYPES_COMPRESSION_H_*/
