/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
/**
 *****************************************************************************
 * @file lac_buffer_desc.c  Utility functions for setting buffer descriptors
 *
 * @ingroup LacBufferDesc
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include header files
*******************************************************************************
*/
#include "qat_utils.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "icp_adf_init.h"
#include "lac_list.h"
#include "lac_sal_types.h"
#include "lac_buffer_desc.h"
#include "lac_mem.h"
#include "cpa_cy_common.h"

/*
*******************************************************************************
* Define public/global function definitions
*******************************************************************************
*/
/* Invalid physical address value */
#define INVALID_PHYSICAL_ADDRESS 0

/* Indicates what type of buffer writes need to be performed */
typedef enum lac_buff_write_op_e {
	WRITE_NORMAL = 0,
	WRITE_AND_GET_SIZE,
	WRITE_AND_ALLOW_ZERO_BUFFER,
} lac_buff_write_op_t;

/* This function implements the buffer description writes for the traditional
 * APIs */
static CpaStatus
LacBuffDesc_CommonBufferListDescWrite(const CpaBufferList *pUserBufferList,
				      Cpa64U *pBufListAlignedPhyAddr,
				      CpaBoolean isPhysicalAddress,
				      Cpa64U *totalDataLenInBytes,
				      sal_service_t *pService,
				      lac_buff_write_op_t operationType)
{
	Cpa32U numBuffers = 0;
	icp_qat_addr_width_t bufListDescPhyAddr = 0;
	icp_qat_addr_width_t bufListAlignedPhyAddr = 0;
	CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
	icp_buffer_list_desc_t *pBufferListDesc = NULL;
	icp_flat_buffer_desc_t *pCurrFlatBufDesc = NULL;

	if (WRITE_AND_GET_SIZE == operationType) {
		*totalDataLenInBytes = 0;
	}

	numBuffers = pUserBufferList->numBuffers;
	pCurrClientFlatBuffer = pUserBufferList->pBuffers;

	/*
	 * Get the physical address of this descriptor - need to offset by the
	 * alignment restrictions on the buffer descriptors
	 */
	bufListDescPhyAddr = (icp_qat_addr_width_t)LAC_OS_VIRT_TO_PHYS_EXTERNAL(
	    (*pService), pUserBufferList->pPrivateMetaData);

	if (bufListDescPhyAddr == 0) {
		QAT_UTILS_LOG(
		    "Unable to get the physical address of the metadata.\n");
		return CPA_STATUS_FAIL;
	}

	bufListAlignedPhyAddr =
	    LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
				   ICP_DESCRIPTOR_ALIGNMENT_BYTES);

	pBufferListDesc = (icp_buffer_list_desc_t *)(LAC_ARCH_UINT)(
	    (LAC_ARCH_UINT)pUserBufferList->pPrivateMetaData +
	    ((LAC_ARCH_UINT)bufListAlignedPhyAddr -
	     (LAC_ARCH_UINT)bufListDescPhyAddr));

	/* Go past the Buffer List descriptor to the list of buffer descriptors
	 */
	pCurrFlatBufDesc =
	    (icp_flat_buffer_desc_t *)((pBufferListDesc->phyBuffers));

	pBufferListDesc->numBuffers = numBuffers;

	if (WRITE_AND_GET_SIZE != operationType) {
		/* Defining zero buffers is useful for example if running zero
		 * length
		 * hash */
		if (0 == numBuffers) {
			/* In the case where there are zero buffers within the
			 * BufList
			 * it is required by firmware that the number is set to
			 * 1
			 * but the phyBuffer and dataLenInBytes are set to
			 * NULL.*/
			pBufferListDesc->numBuffers = 1;
			pCurrFlatBufDesc->dataLenInBytes = 0;
			pCurrFlatBufDesc->phyBuffer = 0;
		}
	}

	while (0 != numBuffers) {
		pCurrFlatBufDesc->dataLenInBytes =
		    pCurrClientFlatBuffer->dataLenInBytes;

		if (WRITE_AND_GET_SIZE == operationType) {
			/* Calculate the total data length in bytes */
			*totalDataLenInBytes +=
			    pCurrClientFlatBuffer->dataLenInBytes;
		}

		/* Check if providing a physical address in the function. If not
		 * we
		 * need to convert it to a physical one */
		if (CPA_TRUE == isPhysicalAddress) {
			pCurrFlatBufDesc->phyBuffer =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				(LAC_ARCH_UINT)(pCurrClientFlatBuffer->pData));
		} else {
			pCurrFlatBufDesc->phyBuffer =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				LAC_OS_VIRT_TO_PHYS_EXTERNAL(
				    (*pService), pCurrClientFlatBuffer->pData));

			if (WRITE_AND_ALLOW_ZERO_BUFFER != operationType) {
				if (INVALID_PHYSICAL_ADDRESS ==
				    pCurrFlatBufDesc->phyBuffer) {
					QAT_UTILS_LOG(
					    "Unable to get the physical address of the client buffer.\n");
					return CPA_STATUS_FAIL;
				}
			}
		}

		pCurrFlatBufDesc++;
		pCurrClientFlatBuffer++;

		numBuffers--;
	}

	*pBufListAlignedPhyAddr = bufListAlignedPhyAddr;
	return CPA_STATUS_SUCCESS;
}

/* This function implements the buffer description writes for the traditional
 * APIs Zero length buffers are allowed, should be used for CHA-CHA-POLY and
 * GCM algorithms */
CpaStatus
LacBuffDesc_BufferListDescWriteAndAllowZeroBuffer(
    const CpaBufferList *pUserBufferList,
    Cpa64U *pBufListAlignedPhyAddr,
    CpaBoolean isPhysicalAddress,
    sal_service_t *pService)
{
	return LacBuffDesc_CommonBufferListDescWrite(
	    pUserBufferList,
	    pBufListAlignedPhyAddr,
	    isPhysicalAddress,
	    NULL,
	    pService,
	    WRITE_AND_ALLOW_ZERO_BUFFER);
}

/* This function implements the buffer description writes for the traditional
 * APIs */
CpaStatus
LacBuffDesc_BufferListDescWrite(const CpaBufferList *pUserBufferList,
				Cpa64U *pBufListAlignedPhyAddr,
				CpaBoolean isPhysicalAddress,
				sal_service_t *pService)
{
	return LacBuffDesc_CommonBufferListDescWrite(pUserBufferList,
						     pBufListAlignedPhyAddr,
						     isPhysicalAddress,
						     NULL,
						     pService,
						     WRITE_NORMAL);
}

/* This function does the same processing as LacBuffDesc_BufferListDescWrite
 * but calculate as well the total length in bytes of the buffer list. */
CpaStatus
LacBuffDesc_BufferListDescWriteAndGetSize(const CpaBufferList *pUserBufferList,
					  Cpa64U *pBufListAlignedPhyAddr,
					  CpaBoolean isPhysicalAddress,
					  Cpa64U *totalDataLenInBytes,
					  sal_service_t *pService)
{
	Cpa32U numBuffers = 0;
	icp_qat_addr_width_t bufListDescPhyAddr = 0;
	icp_qat_addr_width_t bufListAlignedPhyAddr = 0;
	CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
	icp_buffer_list_desc_t *pBufferListDesc = NULL;
	icp_flat_buffer_desc_t *pCurrFlatBufDesc = NULL;
	*totalDataLenInBytes = 0;

	numBuffers = pUserBufferList->numBuffers;
	pCurrClientFlatBuffer = pUserBufferList->pBuffers;

	/*
	 * Get the physical address of this descriptor - need to offset by the
	 * alignment restrictions on the buffer descriptors
	 */
	bufListDescPhyAddr = (icp_qat_addr_width_t)LAC_OS_VIRT_TO_PHYS_EXTERNAL(
	    (*pService), pUserBufferList->pPrivateMetaData);

	if (INVALID_PHYSICAL_ADDRESS == bufListDescPhyAddr) {
		QAT_UTILS_LOG(
		    "Unable to get the physical address of the metadata.\n");
		return CPA_STATUS_FAIL;
	}

	bufListAlignedPhyAddr =
	    LAC_ALIGN_POW2_ROUNDUP(bufListDescPhyAddr,
				   ICP_DESCRIPTOR_ALIGNMENT_BYTES);

	pBufferListDesc = (icp_buffer_list_desc_t *)(LAC_ARCH_UINT)(
	    (LAC_ARCH_UINT)pUserBufferList->pPrivateMetaData +
	    ((LAC_ARCH_UINT)bufListAlignedPhyAddr -
	     (LAC_ARCH_UINT)bufListDescPhyAddr));

	/* Go past the Buffer List descriptor to the list of buffer descriptors
	 */
	pCurrFlatBufDesc =
	    (icp_flat_buffer_desc_t *)((pBufferListDesc->phyBuffers));

	pBufferListDesc->numBuffers = numBuffers;

	while (0 != numBuffers) {
		pCurrFlatBufDesc->dataLenInBytes =
		    pCurrClientFlatBuffer->dataLenInBytes;

		/* Calculate the total data length in bytes */
		*totalDataLenInBytes += pCurrClientFlatBuffer->dataLenInBytes;

		if (isPhysicalAddress == CPA_TRUE) {
			pCurrFlatBufDesc->phyBuffer =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				(LAC_ARCH_UINT)(pCurrClientFlatBuffer->pData));
		} else {
			pCurrFlatBufDesc->phyBuffer =
			    LAC_MEM_CAST_PTR_TO_UINT64(
				LAC_OS_VIRT_TO_PHYS_EXTERNAL(
				    (*pService), pCurrClientFlatBuffer->pData));

			if (pCurrFlatBufDesc->phyBuffer == 0) {
				QAT_UTILS_LOG(
				    "Unable to get the physical address of the client buffer.\n");
				return CPA_STATUS_FAIL;
			}
		}

		pCurrFlatBufDesc++;
		pCurrClientFlatBuffer++;

		numBuffers--;
	}

	*pBufListAlignedPhyAddr = bufListAlignedPhyAddr;
	return CPA_STATUS_SUCCESS;
}

CpaStatus
LacBuffDesc_FlatBufferVerify(const CpaFlatBuffer *pUserFlatBuffer,
			     Cpa64U *pPktSize,
			     lac_aligment_shift_t alignmentShiftExpected)
{
	LAC_CHECK_NULL_PARAM(pUserFlatBuffer);
	LAC_CHECK_NULL_PARAM(pUserFlatBuffer->pData);

	if (0 == pUserFlatBuffer->dataLenInBytes) {
		QAT_UTILS_LOG("FlatBuffer empty\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Expected alignment */
	if (LAC_NO_ALIGNMENT_SHIFT != alignmentShiftExpected) {
		if (!LAC_ADDRESS_ALIGNED(pUserFlatBuffer->pData,
					 alignmentShiftExpected)) {
			QAT_UTILS_LOG(
			    "FlatBuffer not aligned correctly - expected alignment of %u bytes.\n",
			    1 << alignmentShiftExpected);
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	/* Update the total size of the packet. This function being called in a
	 * loop
	 * for an entire buffer list we need to increment the value */
	*pPktSize += pUserFlatBuffer->dataLenInBytes;

	return CPA_STATUS_SUCCESS;
}

CpaStatus
LacBuffDesc_FlatBufferVerifyNull(const CpaFlatBuffer *pUserFlatBuffer,
				 Cpa64U *pPktSize,
				 lac_aligment_shift_t alignmentShiftExpected)
{
	LAC_CHECK_NULL_PARAM(pUserFlatBuffer);

	if (0 != pUserFlatBuffer->dataLenInBytes) {
		LAC_CHECK_NULL_PARAM(pUserFlatBuffer->pData);
	}

	/* Expected alignment */
	if (LAC_NO_ALIGNMENT_SHIFT != alignmentShiftExpected) {
		if (!LAC_ADDRESS_ALIGNED(pUserFlatBuffer->pData,
					 alignmentShiftExpected)) {
			QAT_UTILS_LOG(
			    "FlatBuffer not aligned correctly - expected alignment of %u bytes.\n",
			    1 << alignmentShiftExpected);
			return CPA_STATUS_INVALID_PARAM;
		}
	}

	/* Update the total size of the packet. This function being called in a
	 * loop
	 * for an entire buffer list we need to increment the value */
	*pPktSize += pUserFlatBuffer->dataLenInBytes;

	return CPA_STATUS_SUCCESS;
}

CpaStatus
LacBuffDesc_BufferListVerify(const CpaBufferList *pUserBufferList,
			     Cpa64U *pPktSize,
			     lac_aligment_shift_t alignmentShiftExpected)
{
	CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
	Cpa32U numBuffers = 0;
	CpaStatus status = CPA_STATUS_SUCCESS;

	LAC_CHECK_NULL_PARAM(pUserBufferList);
	LAC_CHECK_NULL_PARAM(pUserBufferList->pBuffers);
	LAC_CHECK_NULL_PARAM(pUserBufferList->pPrivateMetaData);

	numBuffers = pUserBufferList->numBuffers;

	if (0 == pUserBufferList->numBuffers) {
		QAT_UTILS_LOG("Number of buffers is 0.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	pCurrClientFlatBuffer = pUserBufferList->pBuffers;

	*pPktSize = 0;
	while (0 != numBuffers && status == CPA_STATUS_SUCCESS) {
		status = LacBuffDesc_FlatBufferVerify(pCurrClientFlatBuffer,
						      pPktSize,
						      alignmentShiftExpected);

		pCurrClientFlatBuffer++;
		numBuffers--;
	}
	return status;
}

CpaStatus
LacBuffDesc_BufferListVerifyNull(const CpaBufferList *pUserBufferList,
				 Cpa64U *pPktSize,
				 lac_aligment_shift_t alignmentShiftExpected)
{
	CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
	Cpa32U numBuffers = 0;
	CpaStatus status = CPA_STATUS_SUCCESS;

	LAC_CHECK_NULL_PARAM(pUserBufferList);
	LAC_CHECK_NULL_PARAM(pUserBufferList->pBuffers);
	LAC_CHECK_NULL_PARAM(pUserBufferList->pPrivateMetaData);

	numBuffers = pUserBufferList->numBuffers;

	if (0 == pUserBufferList->numBuffers) {
		QAT_UTILS_LOG("Number of buffers is 0.\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	pCurrClientFlatBuffer = pUserBufferList->pBuffers;

	*pPktSize = 0;
	while (0 != numBuffers && status == CPA_STATUS_SUCCESS) {
		status =
		    LacBuffDesc_FlatBufferVerifyNull(pCurrClientFlatBuffer,
						     pPktSize,
						     alignmentShiftExpected);

		pCurrClientFlatBuffer++;
		numBuffers--;
	}
	return status;
}

/**
 ******************************************************************************
 * @ingroup LacBufferDesc
 *****************************************************************************/
void
LacBuffDesc_BufferListTotalSizeGet(const CpaBufferList *pUserBufferList,
				   Cpa64U *pPktSize)
{
	CpaFlatBuffer *pCurrClientFlatBuffer = NULL;
	Cpa32U numBuffers = 0;

	pCurrClientFlatBuffer = pUserBufferList->pBuffers;
	numBuffers = pUserBufferList->numBuffers;

	*pPktSize = 0;
	while (0 != numBuffers) {
		*pPktSize += pCurrClientFlatBuffer->dataLenInBytes;
		pCurrClientFlatBuffer++;
		numBuffers--;
	}
}

void
LacBuffDesc_BufferListZeroFromOffset(CpaBufferList *pBuffList,
				     Cpa32U offset,
				     Cpa32U lenToZero)
{
	Cpa32U zeroLen = 0, sizeLeftToZero = 0;
	Cpa64U currentBufferSize = 0;
	CpaFlatBuffer *pBuffer = NULL;
	Cpa8U *pZero = NULL;
	pBuffer = pBuffList->pBuffers;

	/* Take a copy of total length to zero. */
	sizeLeftToZero = lenToZero;

	while (sizeLeftToZero > 0) {
		currentBufferSize = pBuffer->dataLenInBytes;
		/* check where to start zeroing */
		if (offset >= currentBufferSize) {
			/* Need to get to next buffer and reduce
			 * offset size by data len of buffer */
			offset = offset - pBuffer->dataLenInBytes;
			pBuffer++;
		} else {
			/* Start to Zero from this position */
			pZero = (Cpa8U *)pBuffer->pData + offset;

			/* Need to calculate the correct number of bytes to zero
			 * for this iteration and for this location.
			 */
			if (sizeLeftToZero >= pBuffer->dataLenInBytes) {
				/* The size to zero is spanning buffers, zeroLen
				 * in
				 * this case is from pZero (position) to end of
				 * buffer.
				 */
				zeroLen = pBuffer->dataLenInBytes - offset;
			} else {
				/* zeroLen is set to sizeLeftToZero, then check
				 * if zeroLen and
				 * the offset is greater or equal to the size of
				 * the buffer, if
				 * yes, adjust the zeroLen to zero out the
				 * remainder of this
				 * buffer.
				 */
				zeroLen = sizeLeftToZero;
				if ((zeroLen + offset) >=
				    pBuffer->dataLenInBytes) {
					zeroLen =
					    pBuffer->dataLenInBytes - offset;
				}
			} /* end inner else */
			memset((void *)pZero, 0, zeroLen);
			sizeLeftToZero = sizeLeftToZero - zeroLen;
			/* offset is no longer required as any data left to zero
			 * is now
			 * at the start of the next buffer. set offset to zero
			 * and move on
			 * the buffer pointer to the next buffer.
			 */
			offset = 0;
			pBuffer++;

		} /* end outer else */

	} /* end while */
}
