/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
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
#include "sal_hw_gen.h"

#define CPA_DC_CEIL_DIV(x, y) (((x) + (y)-1) / (y))
#define DC_DEST_BUFF_EXTRA_DEFLATE_GEN2 (55)
#define DC_DEST_BUFF_EXTRA_DEFLATE_GEN4_STATIC (1029)
#define DC_DEST_BUFF_EXTRA_DEFLATE_GEN4_DYN (512)
#define DC_DEST_BUFF_MIN_EXTRA_BYTES(x) ((x < 8) ? (8 - x) : 0)
#define DC_BUF_MAX_SIZE (0xFFFFFFFF)

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
	Cpa64U inBufferSize = inputSize;
	Cpa64U outBufferSize = 0;

	/* Formula for GEN2 deflate:
	 * ceil(9 * Total input bytes / 8) + 55 bytes.
	 * 55 bytes is the skid pad value for GEN2 devices.
	 * Adding extra bytes = `DC_DEST_BUFF_MIN_EXTRA_BYTES(inputSize)`
	 * when calculated value from `CPA_DC_CEIL_DIV(9 * inputSize, 8) +
	 * DC_DEST_BUFF_EXTRA_DEFLATE_GEN2` is less than 64 bytes to
	 * achieve a safer output buffer size of 64 bytes.
	 */
	outBufferSize = CPA_DC_CEIL_DIV(9 * inBufferSize, 8) +
	    DC_DEST_BUFF_EXTRA_DEFLATE_GEN2 +
	    DC_DEST_BUFF_MIN_EXTRA_BYTES(inputSize);

	if (outBufferSize > DC_BUF_MAX_SIZE)
		*outputSize = DC_BUF_MAX_SIZE;
	else
		*outputSize = (Cpa32U)outBufferSize;

	return CPA_STATUS_SUCCESS;
}

static inline CpaStatus
dcDeflateBoundGen4(CpaDcHuffType huffType, Cpa32U inputSize, Cpa32U *outputSize)
{
	Cpa64U outputSizeLong;
	Cpa64U inputSizeLong = (Cpa64U)inputSize;

	switch (huffType) {
	case CPA_DC_HT_STATIC:
		/* Formula for GEN4 static deflate:
		 * ceil((9*sourceLen)/8) + 5 + 1024. */
		outputSizeLong = CPA_DC_CEIL_DIV(9 * inputSizeLong, 8) +
		    DC_DEST_BUFF_EXTRA_DEFLATE_GEN4_STATIC;
		break;
	case CPA_DC_HT_FULL_DYNAMIC:
		/* Formula for GEN4 dynamic deflate:
		 * Ceil ((9*sourceLen)/8)▒| +
		 * ((((8/7) * sourceLen)/ 16KB) * (150+5)) + 512
		 */
		outputSizeLong = DC_DEST_BUFF_EXTRA_DEFLATE_GEN4_DYN;
		outputSizeLong += CPA_DC_CEIL_DIV(9 * inputSizeLong, 8);
		outputSizeLong += ((8 * inputSizeLong * 155) / 7) / (16 * 1024);
		break;
	default:
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Avoid output size overflow */
	if (outputSizeLong & 0xffffffff00000000UL)
		return CPA_STATUS_INVALID_PARAM;

	*outputSize = (Cpa32U)outputSizeLong;
	return CPA_STATUS_SUCCESS;
}

CpaStatus
cpaDcDeflateCompressBound(const CpaInstanceHandle dcInstance,
			  CpaDcHuffType huffType,
			  Cpa32U inputSize,
			  Cpa32U *outputSize)
{
	sal_compression_service_t *pService = NULL;
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

	pService = (sal_compression_service_t *)insHandle;
	if (isDcGen4x(pService)) {
		return dcDeflateBoundGen4(huffType, inputSize, outputSize);
	} else {
		return dcDeflateBoundGen2(huffType, inputSize, outputSize);
	}
}

CpaStatus
cpaDcLZ4CompressBound(const CpaInstanceHandle dcInstance,
		      Cpa32U inputSize,
		      Cpa32U *outputSize)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaDcLZ4SCompressBound(const CpaInstanceHandle dcInstance,
		       Cpa32U inputSize,
		       Cpa32U *outputSize)
{
	return CPA_STATUS_UNSUPPORTED;
}
