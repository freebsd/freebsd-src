/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file dc_datapath.h
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Definition of the Data Compression datapath parameters.
 *
 *******************
 * **********************************************************/
#ifndef DC_DATAPATH_H_
#define DC_DATAPATH_H_

#define LAC_QAT_DC_REQ_SZ_LW 32
#define LAC_QAT_DC_RESP_SZ_LW 8

/* Restriction on the source buffer size for compression due to the firmware
 * processing */
#define DC_SRC_BUFFER_MIN_SIZE (15)

/* Restriction on the destination buffer size for compression due to
 * the management of skid buffers in the firmware */
#define DC_DEST_BUFFER_DYN_MIN_SIZE (128)
#define DC_DEST_BUFFER_STA_MIN_SIZE (64)
/* C62x and C3xxx pcie rev0 devices require an additional 32bytes */
#define DC_DEST_BUFFER_STA_ADDITIONAL_SIZE (32)

/* C4xxx device only requires 47 bytes */
#define DC_DEST_BUFFER_MIN_SIZE (47)

/* Minimum destination buffer size for decompression */
#define DC_DEST_BUFFER_DEC_MIN_SIZE (1)

/* Restriction on the source and destination buffer sizes for compression due
 * to the firmware taking 32 bits parameters. The max size is 2^32-1 */
#define DC_BUFFER_MAX_SIZE (0xFFFFFFFF)

/* DC Source & Destination buffer type (FLAT/SGL) */
#define DC_DEFAULT_QAT_PTR_TYPE QAT_COMN_PTR_TYPE_SGL
#define DC_DP_QAT_PTR_TYPE QAT_COMN_PTR_TYPE_FLAT

/* Offset to first byte of Input Byte Counter (IBC) in state register */
#define DC_STATE_IBC_OFFSET (8)
/* Size in bytes of input byte counter (IBC) in state register */
#define DC_IBC_SIZE_IN_BYTES (4)

/* Offset to first byte to CRC32 in state register */
#define DC_STATE_CRC32_OFFSET (40)
/* Offset to first byte to output CRC32 in state register */
#define DC_STATE_OUTPUT_CRC32_OFFSET (48)
/* Offset to first byte to input CRC32 in state register */
#define DC_STATE_INPUT_CRC32_OFFSET (52)

/* Offset to first byte of ADLER32 in state register */
#define DC_STATE_ADLER32_OFFSET (48)

/* 8 bit mask value */
#define DC_8_BIT_MASK (0xff)

/* 8 bit shift position */
#define DC_8_BIT_SHIFT_POS (8)

/* Size in bytes of checksum */
#define DC_CHECKSUM_SIZE_IN_BYTES (4)

/* Mask used to set the most significant bit to zero */
#define DC_STATE_REGISTER_ZERO_MSB_MASK (0x7F)

/* Mask used to keep only the most significant bit and set the others to zero */
#define DC_STATE_REGISTER_KEEP_MSB_MASK (0x80)

/* Compression state register word containing the parity bit */
#define DC_STATE_REGISTER_PARITY_BIT_WORD (5)

/* Location of the parity bit within the compression state register word */
#define DC_STATE_REGISTER_PARITY_BIT (7)

/* size which needs to be reserved before the results field to
 * align the results field with the API struct  */
#define DC_API_ALIGNMENT_OFFSET (offsetof(CpaDcDpOpData, results))

/* Mask used to check the CompressAndVerify capability bit */
#define DC_CNV_EXTENDED_CAPABILITY (0x01)

/* Mask used to check the CompressAndVerifyAndRecover capability bit */
#define DC_CNVNR_EXTENDED_CAPABILITY (0x100)

/* Default values for CNV integrity checks,
 * those are used to inform hardware of specifying CRC parameters to be used
 * when calculating CRCs */
#define DC_CRC_POLY_DEFAULT 0x04c11db7
#define DC_XOR_FLAGS_DEFAULT 0xe0000
#define DC_XOR_OUT_DEFAULT 0xffffffff
#define DC_INVALID_CRC 0x0

/**
*******************************************************************************
* @ingroup cpaDc Data Compression
*      Compression cookie
* @description
*      This cookie stores information for a particular compression perform op.
*      This includes various user-supplied parameters for the operation which
*      will be needed in our callback function.
*      A pointer to this cookie is stored in the opaque data field of the QAT
*      message so that it can be accessed in the asynchronous callback.
* @note
*      The order of the parameters within this structure is important. It needs
*      to match the order of the parameters in CpaDcDpOpData up to the
*      pSessionHandle. This allows the correct processing of the callback.
*****************************************************************************/
typedef struct dc_compression_cookie_s {
	Cpa8U dcReqParamsBuffer[DC_API_ALIGNMENT_OFFSET];
	/**< Memory block  - was previously reserved for request parameters.
	 * Now size maintained so following members align with API struct,
	 * but no longer used for request parameters */
	CpaDcRqResults reserved;
	/**< This is reserved for results to correctly align the structure
	 * to match the one from the data plane API */
	CpaInstanceHandle dcInstance;
	/**< Compression instance handle */
	CpaDcSessionHandle pSessionHandle;
	/**< Pointer to the session handle */
	icp_qat_fw_comp_req_t request;
	/**< Compression request */
	void *callbackTag;
	/**< Opaque data supplied by the client */
	dc_session_desc_t *pSessionDesc;
	/**< Pointer to the session descriptor */
	CpaDcFlush flushFlag;
	/**< Flush flag */
	CpaDcOpData *pDcOpData;
	/**< struct containing flags and CRC related data for this session */
	CpaDcRqResults *pResults;
	/**< Pointer to result buffer holding consumed and produced data */
	Cpa32U srcTotalDataLenInBytes;
	/**< Total length of the source data */
	Cpa32U dstTotalDataLenInBytes;
	/**< Total length of the destination data */
	dc_request_dir_t compDecomp;
	/**< Used to know whether the request is compression or decompression.
	 * Useful when defining the session as combined */
	CpaBufferList *pUserSrcBuff;
	/**< virtual userspace ptr to source SGL */
	CpaBufferList *pUserDestBuff;
	/**< virtual userspace ptr to destination SGL */
} dc_compression_cookie_t;

/**
 *****************************************************************************
 * @ingroup Dc_DataCompression
 *      Callback function called for compression and decompression requests in
 *      asynchronous mode
 *
 * @description
 *      Called to process compression and decompression response messages. This
 *      callback will check for errors, update the statistics and will call the
 *      user callback
 *
 * @param[in]   pRespMsg        Response message
 *
 *****************************************************************************/
void dcCompression_ProcessCallback(void *pRespMsg);

/**
*****************************************************************************
* @ingroup Dc_DataCompression
*      Describes CNV and CNVNR modes
*
* @description
*      This enum is used to indicate the CNV modes.
*
*****************************************************************************/
typedef enum dc_cnv_mode_s {
	DC_NO_CNV = 0,
	/* CNV = FALSE, CNVNR = FALSE */
	DC_CNV,
	/* CNV = TRUE, CNVNR = FALSE */
	DC_CNVNR,
	/* CNV = TRUE, CNVNR = TRUE */
} dc_cnv_mode_t;

#endif /* DC_DATAPATH_H_ */
