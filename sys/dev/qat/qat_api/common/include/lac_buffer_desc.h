/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 ***************************************************************************
 * @file lac_buffer_desc.h
 *
 * @defgroup LacBufferDesc     Buffer Descriptors
 *
 * @ingroup LacCommon
 *
 * Functions which handle updating a user supplied buffer with the QAT
 * descriptor representation.
 *
 ***************************************************************************/

/***************************************************************************/

#ifndef LAC_BUFFER_DESC_H
#define LAC_BUFFER_DESC_H

/***************************************************************************
 * Include header files
 ***************************************************************************/
#include "cpa.h"
#include "icp_buffer_desc.h"
#include "cpa_cy_sym.h"
#include "lac_common.h"

/**
*******************************************************************************
* @ingroup LacBufferDesc
*      Write the buffer descriptor in QAT friendly format.
*
* @description
*      Updates the Meta Data associated with the pUserBufferList CpaBufferList
*      This function will also return the (aligned) physical address
*      associated with this CpaBufferList.
*
* @param[in]  pUserBufferList           A pointer to the buffer list to
*                                       create the meta data for the QAT.
* @param[out] pBufferListAlignedPhyAddr The pointer to the aligned physical
*                                       address.
* @param[in]  isPhysicalAddress         Type of address
* @param[in]  pService                  Pointer to generic service
*
*****************************************************************************/
CpaStatus LacBuffDesc_BufferListDescWrite(const CpaBufferList *pUserBufferList,
					  Cpa64U *pBufferListAlignedPhyAddr,
					  CpaBoolean isPhysicalAddress,
					  sal_service_t *pService);

/**
*******************************************************************************
* @ingroup LacBufferDesc
*      Write the buffer descriptor in QAT friendly format.
*
* @description
*      Updates the Meta Data associated with the pUserBufferList CpaBufferList
*      This function will also return the (aligned) physical address
*      associated with this CpaBufferList. Zero length buffers are allowed.
*      Should be used for CHA-CHA-POLY and GCM algorithms.
*
* @param[in]  pUserBufferList           A pointer to the buffer list to
*                                       create the meta data for the QAT.
* @param[out] pBufferListAlignedPhyAddr The pointer to the aligned physical
*                                       address.
* @param[in]  isPhysicalAddress         Type of address
* @param[in]  pService                  Pointer to generic service
*
*****************************************************************************/
CpaStatus LacBuffDesc_BufferListDescWriteAndAllowZeroBuffer(
    const CpaBufferList *pUserBufferList,
    Cpa64U *pBufferListAlignedPhyAddr,
    CpaBoolean isPhysicalAddress,
    sal_service_t *pService);

/**
*******************************************************************************
 * @ingroup LacBufferDesc
 *      Write the buffer descriptor in QAT friendly format.
 *
 * @description
 *      Updates the Meta Data associated with the PClientList CpaBufferList
 *      This function will also return the (aligned) physical address
 *      associated with this CpaBufferList and the total data length of the
 *      buffer list.
 *
 * @param[in] pUserBufferList            A pointer to the buffer list to
 *                                       create the meta data for the QAT.
 * @param[out] pBufListAlignedPhyAddr    The pointer to the aligned physical
 *                                       address.
 * @param[in]  isPhysicalAddress         Type of address
 * @param[out] totalDataLenInBytes       The pointer to the total data length
 *                                       of the buffer list
 * @param[in]  pService                  Pointer to generic service
 *
 *****************************************************************************/
CpaStatus
LacBuffDesc_BufferListDescWriteAndGetSize(const CpaBufferList *pUserBufferList,
					  Cpa64U *pBufListAlignedPhyAddr,
					  CpaBoolean isPhysicalAddress,
					  Cpa64U *totalDataLenInBytes,
					  sal_service_t *pService);

/**
*******************************************************************************
 * @ingroup LacBufferDesc
 *      Ensure the CpaFlatBuffer is correctly formatted.
 *
 * @description
 *      Ensures the CpaFlatBuffer is correctly formatted
 *      This function will also return the total size of the buffers
 *      in the scatter gather list.
 *
 * @param[in] pUserFlatBuffer           A pointer to the flat buffer to
 *                                      validate.
 * @param[out] pPktSize                 The total size of the packet.
 * @param[in] alignmentShiftExpected    The expected alignment shift of each
 *                                      of the elements of the scatter gather
 *
 * @retval CPA_STATUS_INVALID_PARAM     BufferList failed checks
 * @retval CPA_STATUS_SUCCESS           Function executed successfully
 *
 *****************************************************************************/
CpaStatus
LacBuffDesc_FlatBufferVerify(const CpaFlatBuffer *pUserFlatBuffer,
			     Cpa64U *pPktSize,
			     lac_aligment_shift_t alignmentShiftExpected);

/**
*******************************************************************************
 * @ingroup LacBufferDesc
 *      Ensure the CpaFlatBuffer is correctly formatted.
 *      This function will allow a size of zero bytes to any of the Flat
 *      buffers.
 *
 * @description
 *      Ensures the CpaFlatBuffer is correctly formatted
 *      This function will also return the total size of the buffers
 *      in the scatter gather list.
 *
 * @param[in] pUserFlatBuffer           A pointer to the flat buffer to
 *                                      validate.
 * @param[out] pPktSize                 The total size of the packet.
 * @param[in] alignmentShiftExpected    The expected alignment shift of each
 *                                      of the elements of the scatter gather
 *
 * @retval CPA_STATUS_INVALID_PARAM     BufferList failed checks
 * @retval CPA_STATUS_SUCCESS           Function executed successfully
 *
 *****************************************************************************/
CpaStatus
LacBuffDesc_FlatBufferVerifyNull(const CpaFlatBuffer *pUserFlatBuffer,
				 Cpa64U *pPktSize,
				 lac_aligment_shift_t alignmentShiftExpected);

/**
*******************************************************************************
 * @ingroup LacBufferDesc
 *      Ensure the CpaBufferList is correctly formatted.
 *
 * @description
 *      Ensures the CpaBufferList pUserBufferList is correctly formatted
 *      including the user supplied metaData.
 *      This function will also return the total size of the buffers
 *      in the scatter gather list.
 *
 * @param[in] pUserBufferList           A pointer to the buffer list to
 *                                      validate.
 * @param[out] pPktSize                 The total size of the buffers in the
 *                                      scatter gather list.
 * @param[in] alignmentShiftExpected    The expected alignment shift of each
 *                                      of the elements of the scatter gather
 *                                      list.
 * @retval CPA_STATUS_INVALID_PARAM     BufferList failed checks
 * @retval CPA_STATUS_SUCCESS           Function executed successfully
 *
 *****************************************************************************/
CpaStatus
LacBuffDesc_BufferListVerify(const CpaBufferList *pUserBufferList,
			     Cpa64U *pPktSize,
			     lac_aligment_shift_t alignmentShiftExpected);

/**
*******************************************************************************
 * @ingroup LacBufferDesc
 *      Ensure the CpaBufferList is correctly formatted.
 *
 * @description
 *      Ensures the CpaBufferList pUserBufferList is correctly formatted
 *      including the user supplied metaData.
 *      This function will also return the total size of the buffers
 *      in the scatter gather list.
 *
 * @param[in] pUserBufferList           A pointer to the buffer list to
 *                                      validate.
 * @param[out] pPktSize                 The total size of the buffers in the
 *                                      scatter gather list.
 * @param[in] alignmentShiftExpected    The expected alignment shift of each
 *                                      of the elements of the scatter gather
 *                                      list.
 * @retval CPA_STATUS_INVALID_PARAM     BufferList failed checks
 * @retval CPA_STATUS_SUCCESS           Function executed successfully
 *
 *****************************************************************************/
CpaStatus
LacBuffDesc_BufferListVerifyNull(const CpaBufferList *pUserBufferList,
				 Cpa64U *pPktSize,
				 lac_aligment_shift_t alignmentShiftExpected);

/**
*******************************************************************************
 * @ingroup LacBufferDesc
 *      Get the total size of a CpaBufferList.
 *
 * @description
 *      This function returns the total size of the buffers
 *      in the scatter gather list.
 *
 * @param[in] pUserBufferList           A pointer to the buffer list to
 *                                      calculate the total size for.
 * @param[out] pPktSize                 The total size of the buffers in the
 *                                      scatter gather list.
 *
 *****************************************************************************/
void LacBuffDesc_BufferListTotalSizeGet(const CpaBufferList *pUserBufferList,
					Cpa64U *pPktSize);

/**
*******************************************************************************
 * @ingroup LacBufferDesc
 *      Zero some of the CpaBufferList.
 *
 * @description
 *      Zero a section of data within the CpaBufferList from an offset for
 *      a specific length.
 *
 * @param[in] pBuffList           A pointer to the buffer list to
 *                                zero an area of.
 * @param[in] offset              Number of bytes from start of buffer to where
 *                                to start zeroing.
 *
 * @param[in] lenToZero           Number of bytes that will be set to zero
 *                                after the call to this function.
 *****************************************************************************/

void LacBuffDesc_BufferListZeroFromOffset(CpaBufferList *pBuffList,
					  Cpa32U offset,
					  Cpa32U lenToZero);

#endif /* LAC_BUFFER_DESC_H */
