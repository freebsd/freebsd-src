/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file dc_buffers.c
 *
 * @defgroup Dc_DataCompression DC Data Compression
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Implementation of the buffer management operations for
 *      Data Compression service.
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/
#include "cpa.h"
#include "cpa_dc.h"
#include "cpa_dc_bp.h"

#include "sal_types_compression.h"
#include "icp_qat_fw_comp.h"

#define CPA_DC_CEIL_DIV(x, y) (((x) + (y)-1) / (y))
#define DC_DEST_BUFF_EXTRA_DEFLATE_GEN2 (55)

CpaStatus
cpaDcBufferListGetMetaSize(const CpaInstanceHandle instanceHandle,
			   Cpa32U numBuffers,
			   Cpa32U *pSizeInBytes)
{
	CpaInstanceHandle insHandle = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == instanceHandle) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = instanceHandle;
	}

	LAC_CHECK_INSTANCE_HANDLE(insHandle);
	LAC_CHECK_NULL_PARAM(pSizeInBytes);

	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);

	if (0 == numBuffers) {
		QAT_UTILS_LOG("Number of buffers is 0.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	*pSizeInBytes = (sizeof(icp_buffer_list_desc_t) +
			 (sizeof(icp_flat_buffer_desc_t) * (numBuffers + 1)) +
			 ICP_DESCRIPTOR_ALIGNMENT_BYTES);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcBnpBufferListGetMetaSize(const CpaInstanceHandle instanceHandle,
			      Cpa32U numJobs,
			      Cpa32U *pSizeInBytes)
{
	return CPA_STATUS_UNSUPPORTED;
}

static inline CpaStatus
dcDeflateBoundGen2(CpaDcHuffType huffType, Cpa32U inputSize, Cpa32U *outputSize)
{
	/* Formula for GEN2 deflate:
	 * ceil(9 * Total input bytes / 8) + 55 bytes.
	 * 55 bytes is the skid pad value for GEN2 devices.
	 */
	*outputSize =
	    CPA_DC_CEIL_DIV(9 * inputSize, 8) + DC_DEST_BUFF_EXTRA_DEFLATE_GEN2;

	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcDeflateCompressBound(const CpaInstanceHandle dcInstance,
			  CpaDcHuffType huffType,
			  Cpa32U inputSize,
			  Cpa32U *outputSize)
{
	CpaInstanceHandle insHandle = NULL;

	if (CPA_INSTANCE_HANDLE_SINGLE == dcInstance) {
		insHandle = dcGetFirstHandle();
	} else {
		insHandle = dcInstance;
	}

	LAC_CHECK_INSTANCE_HANDLE(insHandle);
	LAC_CHECK_NULL_PARAM(outputSize);
	/* Ensure this is a compression instance */
	SAL_CHECK_INSTANCE_TYPE(insHandle, SAL_SERVICE_TYPE_COMPRESSION);
	if (!inputSize) {
		QAT_UTILS_LOG(
		    "The input size needs to be greater than zero.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if ((CPA_DC_HT_STATIC != huffType) &&
	    (CPA_DC_HT_FULL_DYNAMIC != huffType)) {
		QAT_UTILS_LOG("Invalid huffType value.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	return dcDeflateBoundGen2(huffType, inputSize, outputSize);
}
