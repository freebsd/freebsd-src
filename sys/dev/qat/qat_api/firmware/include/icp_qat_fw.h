/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file icp_qat_fw.h
 * @defgroup icp_qat_fw_comn ICP QAT FW Common Processing Definitions
 * @ingroup icp_qat_fw
 *
 * @description
 *      This file documents the common interfaces that the QAT FW running on
 *      the QAT AE exports. This common layer is used by a number of services
 *      to export content processing services.
 *
 *****************************************************************************/

#ifndef _ICP_QAT_FW_H_
#define _ICP_QAT_FW_H_

/*
* ==============================
* General Notes on the Interface
*/

/*
*
* ==============================
*
* Introduction
*
* Data movement and slice chaining
*
* Endianness
*      - Unless otherwise stated, all structures are defined in LITTLE ENDIAN
*        MODE
*
* Alignment
*      - In general all data structures provided to a request should be aligned
*      on the 64 byte boundary so as to allow optimal memory transfers. At the
*      minimum they must be aligned to the 8 byte boundary
*
* Sizes
*   Quad words = 8 bytes
*
* Terminology
*
* ==============================
*/

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "icp_qat_hw.h"

/* Big assumptions that both bitpos and mask are constants */
#define QAT_FIELD_SET(flags, val, bitpos, mask)                                \
	(flags) = (((flags) & (~((mask) << (bitpos)))) |                       \
		   (((val) & (mask)) << (bitpos)))

#define QAT_FIELD_GET(flags, bitpos, mask) (((flags) >> (bitpos)) & (mask))
#define QAT_FLAG_SET(flags, val, bitpos)                                       \
	((flags) = (((flags) & (~(1 << (bitpos)))) | (((val)&1) << (bitpos))))

#define QAT_FLAG_CLEAR(flags, bitpos) (flags) = ((flags) & (~(1 << (bitpos))))

#define QAT_FLAG_GET(flags, bitpos) (((flags) >> (bitpos)) & 1)

/**< @ingroup icp_qat_fw_comn
 * Default request and response ring size in bytes */
#define ICP_QAT_FW_REQ_DEFAULT_SZ 128
#define ICP_QAT_FW_RESP_DEFAULT_SZ 32

#define ICP_QAT_FW_COMN_ONE_BYTE_SHIFT 8
#define ICP_QAT_FW_COMN_SINGLE_BYTE_MASK 0xFF

/**< @ingroup icp_qat_fw_comn
 * Common Request - Block sizes definitions in multiples of individual long
 * words */
#define ICP_QAT_FW_NUM_LONGWORDS_1 1
#define ICP_QAT_FW_NUM_LONGWORDS_2 2
#define ICP_QAT_FW_NUM_LONGWORDS_3 3
#define ICP_QAT_FW_NUM_LONGWORDS_4 4
#define ICP_QAT_FW_NUM_LONGWORDS_5 5
#define ICP_QAT_FW_NUM_LONGWORDS_6 6
#define ICP_QAT_FW_NUM_LONGWORDS_7 7
#define ICP_QAT_FW_NUM_LONGWORDS_10 10
#define ICP_QAT_FW_NUM_LONGWORDS_13 13

/**< @ingroup icp_qat_fw_comn
 * Definition of the associated service Id for NULL service type.
 * Note: the response is expected to use ICP_QAT_FW_COMN_RESP_SERV_CPM_FW */
#define ICP_QAT_FW_NULL_REQ_SERV_ID 1

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *              Definition of the firmware interface service users, for
 *              responses.
 * @description
 *              Enumeration which is used to indicate the ids of the services
 *              for responses using the external firmware interfaces.
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_FW_COMN_RESP_SERV_NULL,     /**< NULL service id type */
	ICP_QAT_FW_COMN_RESP_SERV_CPM_FW,   /**< CPM FW Service ID */
	ICP_QAT_FW_COMN_RESP_SERV_DELIMITER /**< Delimiter service id type */
} icp_qat_fw_comn_resp_serv_id_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *              Definition of the request types
 * @description
 *              Enumeration which is used to indicate the ids of the request
 *              types used in each of the external firmware interfaces
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_FW_COMN_REQ_NULL = 0,	/**< NULL request type */
	ICP_QAT_FW_COMN_REQ_CPM_FW_PKE = 3,  /**< CPM FW PKE Request */
	ICP_QAT_FW_COMN_REQ_CPM_FW_LA = 4,   /**< CPM FW Lookaside Request */
	ICP_QAT_FW_COMN_REQ_CPM_FW_DMA = 7,  /**< CPM FW DMA Request */
	ICP_QAT_FW_COMN_REQ_CPM_FW_COMP = 9, /**< CPM FW Compression Request */
	ICP_QAT_FW_COMN_REQ_DELIMITER	/**< End delimiter */

} icp_qat_fw_comn_request_id_t;

/* ========================================================================= */
/*                           QAT FW REQUEST STRUCTURES                       */
/* ========================================================================= */

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Common request flags type
 *
 * @description
 *      Definition of the common request flags.
 *
 *****************************************************************************/
typedef uint8_t icp_qat_fw_comn_flags;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Common request - Service specific flags type
 *
 * @description
 *      Definition of the common request service specific flags.
 *
 *****************************************************************************/
typedef uint16_t icp_qat_fw_serv_specif_flags;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Common request - Extended service specific flags type
 *
 * @description
 *      Definition of the common request extended service specific flags.
 *
 *****************************************************************************/
typedef uint8_t icp_qat_fw_ext_serv_specif_flags;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common QAT FW request content descriptor field -
 *      points to the content descriptor parameters or itself contains service-
 *      specific data. Also specifies content descriptor parameter size.
 *      Contains reserved fields.
 * @description
 *      Common section of the request used across all of the services exposed
 *      by the QAT FW. Each of the services inherit these common fields
 *
 *****************************************************************************/
typedef union icp_qat_fw_comn_req_hdr_cd_pars_s {
	/**< LWs 2-5 */
	struct {
		uint64_t content_desc_addr;
		/**< Address of the content descriptor */

		uint16_t content_desc_resrvd1;
		/**< Content descriptor reserved field */

		uint8_t content_desc_params_sz;
		/**< Size of the content descriptor parameters in quad words.
		 * These
		 * parameters describe the session setup configuration info for
		 * the
		 * slices that this request relies upon i.e. the configuration
		 * word and
		 * cipher key needed by the cipher slice if there is a request
		 * for
		 * cipher processing. */

		uint8_t content_desc_hdr_resrvd2;
		/**< Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/**< Content descriptor reserved field */
	} s;

	struct {
		uint32_t serv_specif_fields[ICP_QAT_FW_NUM_LONGWORDS_4];

	} s1;

} icp_qat_fw_comn_req_hdr_cd_pars_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common QAT FW request middle block.
 * @description
 *      Common section of the request used across all of the services exposed
 *      by the QAT FW. Each of the services inherit these common fields
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comn_req_mid_s {
	/**< LWs 6-13 */
	uint64_t opaque_data;
	/**< Opaque data passed unmodified from the request to response messages
	 * by
	 * firmware (fw) */

	uint64_t src_data_addr;
	/**< Generic definition of the source data supplied to the QAT AE. The
	 * common flags are used to further describe the attributes of this
	 * field */

	uint64_t dest_data_addr;
	/**< Generic definition of the destination data supplied to the QAT AE.
	 * The
	 * common flags are used to further describe the attributes of this
	 * field */

	uint32_t src_length;
	/** < Length of source flat buffer incase src buffer
	 * type is flat */

	uint32_t dst_length;
	/** < Length of source flat buffer incase dst buffer
	 * type is flat */

} icp_qat_fw_comn_req_mid_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common QAT FW request content descriptor control
 *      block.
 *
 * @description
 *      Service specific section of the request used across all of the services
 *      exposed by the QAT FW. Each of the services populates this block
 *      uniquely. Refer to the service-specific header structures e.g.
 *      'icp_qat_fw_cipher_hdr_s' (for Cipher) etc.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comn_req_cd_ctrl_s {
	/**< LWs 27-31 */
	uint32_t content_desc_ctrl_lw[ICP_QAT_FW_NUM_LONGWORDS_5];

} icp_qat_fw_comn_req_cd_ctrl_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common QAT FW request header.
 * @description
 *      Common section of the request used across all of the services exposed
 *      by the QAT FW. Each of the services inherit these common fields. The
 *      reserved field of 7 bits and the service command Id field are all
 *      service-specific fields, along with the service specific flags.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comn_req_hdr_s {
	/**< LW0 */
	uint8_t resrvd1;
	/**< reserved field */

	uint8_t service_cmd_id;
	/**< Service Command Id  - this field is service-specific
	 * Please use service-specific command Id here e.g.Crypto Command Id
	 * or Compression Command Id etc. */

	uint8_t service_type;
	/**< Service type */

	uint8_t hdr_flags;
	/**< This represents a flags field for the Service Request.
	 * The most significant bit is the 'valid' flag and the only
	 * one used. All remaining bit positions are unused and
	 * are therefore reserved and need to be set to 0. */

	/**< LW1 */
	icp_qat_fw_serv_specif_flags serv_specif_flags;
	/**< Common Request service-specific flags
	 * e.g. Symmetric Crypto Command Flags */

	icp_qat_fw_comn_flags comn_req_flags;
	/**< Common Request Flags consisting of
	 * - 6 reserved bits,
	 * - 1 Content Descriptor field type bit and
	 * - 1 Source/destination pointer type bit */

	icp_qat_fw_ext_serv_specif_flags extended_serv_specif_flags;
	/**< An extension of serv_specif_flags
	 */
} icp_qat_fw_comn_req_hdr_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common QAT FW request parameter field.
 *
 * @description
 *      Service specific section of the request used across all of the services
 *      exposed by the QAT FW. Each of the services populates this block
 *      uniquely. Refer to service-specific header structures e.g.
 *      'icp_qat_fw_comn_req_cipher_rqpars_s' (for Cipher) etc.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comn_req_rqpars_s {
	/**< LWs 14-26 */
	uint32_t serv_specif_rqpars_lw[ICP_QAT_FW_NUM_LONGWORDS_13];

} icp_qat_fw_comn_req_rqpars_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common request structure with service specific
 *      fields
 * @description
 *      This is a definition of the full qat request structure used by all
 *      services. Each service is free to use the service fields in its own
 *      way. This struct is useful as a message passing argument before the
 *      service contained within the request is determined.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comn_req_s {
	/**< LWs 0-1 */
	icp_qat_fw_comn_req_hdr_t comn_hdr;
	/**< Common request header */

	/**< LWs 2-5 */
	icp_qat_fw_comn_req_hdr_cd_pars_t cd_pars;
	/**< Common Request content descriptor field which points either to a
	 * content descriptor
	 * parameter block or contains the service-specific data itself. */

	/**< LWs 6-13 */
	icp_qat_fw_comn_req_mid_t comn_mid;
	/**< Common request middle section */

	/**< LWs 14-26 */
	icp_qat_fw_comn_req_rqpars_t serv_specif_rqpars;
	/**< Common request service-specific parameter field */

	/**< LWs 27-31 */
	icp_qat_fw_comn_req_cd_ctrl_t cd_ctrl;
	/**< Common request content descriptor control block -
	 * this field is service-specific */

} icp_qat_fw_comn_req_t;

/* ========================================================================= */
/*                           QAT FW RESPONSE STRUCTURES                      */
/* ========================================================================= */

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Error code field
 *
 * @description
 *      Overloaded field with 8 bit common error field or two
 *      8 bit compression error fields for compression and translator slices
 *
 *****************************************************************************/
typedef union icp_qat_fw_comn_error_s {
	struct {
		uint8_t resrvd;
		/**< 8 bit reserved field */

		uint8_t comn_err_code;
		/**< 8 bit common error code */

	} s;
	/**< Structure which is used for non-compression responses */

	struct {
		uint8_t xlat_err_code;
		/**< 8 bit translator error field */

		uint8_t cmp_err_code;
		/**< 8 bit compression error field */

	} s1;
	/** Structure which is used for compression responses */

} icp_qat_fw_comn_error_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common QAT FW response header.
 * @description
 *      This section of the response is common across all of the services
 *      that generate a firmware interface response
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comn_resp_hdr_s {
	/**< LW0 */
	uint8_t resrvd1;
	/**< Reserved field - this field is service-specific -
	 * Note: The Response Destination Id has been removed
	 * from first QWord */

	uint8_t service_id;
	/**< Service Id returned by service block */

	uint8_t response_type;
	/**< Response type - copied from the request to
	 * the response message */

	uint8_t hdr_flags;
	/**< This represents a flags field for the Response.
	 * Bit<7> = 'valid' flag
	 * Bit<6> = 'CNV' flag indicating that CNV was executed
	 *          on the current request
	 * Bit<5> = 'CNVNR' flag indicating that a recovery happened
	 *          on the current request following a CNV error
	 * All remaining bits are unused and are therefore reserved.
	 * They must to be set to 0.
	 */

	/**< LW 1 */
	icp_qat_fw_comn_error_t comn_error;
	/**< This field is overloaded to allow for one 8 bit common error field
	 *   or two 8 bit error fields from compression and translator  */

	uint8_t comn_status;
	/**< Status field which specifies which slice(s) report an error */

	uint8_t cmd_id;
	/**< Command Id - passed from the request to the response message */

} icp_qat_fw_comn_resp_hdr_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Definition of the common response structure with service specific
 *      fields
 * @description
 *      This is a definition of the full qat response structure used by all
 *      services.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comn_resp_s {
	/**< LWs 0-1 */
	icp_qat_fw_comn_resp_hdr_t comn_hdr;
	/**< Common header fields */

	/**< LWs 2-3 */
	uint64_t opaque_data;
	/**< Opaque data passed from the request to the response message */

	/**< LWs 4-7 */
	uint32_t resrvd[ICP_QAT_FW_NUM_LONGWORDS_4];
	/**< Reserved */

} icp_qat_fw_comn_resp_t;

/* ========================================================================= */
/*                           MACRO DEFINITIONS                               */
/* ========================================================================= */

/*  Common QAT FW request header - structure of LW0
 *  + ===== + ------- + ----------- + ----------- + ----------- + -------- +
 *  |  Bit  |  31/30  |  29 - 24    |  21 - 16    |  15 - 8     |  7 - 0   |
 *  + ===== + ------- + ----------- + ----------- + ----------- + -------- +
 *  | Flags |  V/Gen  |   Reserved  | Serv Type   | Serv Cmd Id |  Rsv     |
 *  + ===== + ------- + ----------- + ----------- + ----------- + -------- +
 */

/**< @ingroup icp_qat_fw_comn
 *  Definition of the setting of the header's valid flag */
#define ICP_QAT_FW_COMN_REQ_FLAG_SET 1
/**< @ingroup icp_qat_fw_comn
 *  Definition of the setting of the header's valid flag */
#define ICP_QAT_FW_COMN_REQ_FLAG_CLR 0

/**< @ingroup icp_qat_fw_comn
 * Macros defining the bit position and mask of the 'valid' flag, within the
 * hdr_flags field of LW0 (service request and response) */
#define ICP_QAT_FW_COMN_VALID_FLAG_BITPOS 7
#define ICP_QAT_FW_COMN_VALID_FLAG_MASK 0x1

/**< @ingroup icp_qat_fw_comn
 * Macros defining the bit position and mask of the 'generation' flag, within
 * the hdr_flags field of LW0 (service request and response) */
#define ICP_QAT_FW_COMN_GEN_FLAG_BITPOS 6
#define ICP_QAT_FW_COMN_GEN_FLAG_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 *  The request is targeted for QAT2.0 */
#define ICP_QAT_FW_COMN_GEN_2 1
/**< @ingroup icp_qat_fw_comn
*  The request is targeted for QAT1.x. QAT2.0 FW will return
   'unsupported request' if GEN1 request type is sent to QAT2.0 FW */
#define ICP_QAT_FW_COMN_GEN_1 0

#define ICP_QAT_FW_COMN_HDR_RESRVD_FLD_MASK 0x7F

/*  Common QAT FW response header - structure of LW0
 *  + ===== + --- + --- + ----- + ----- + --------- + ----------- + ----- +
 *  |  Bit  | 31  | 30  |   29  | 28-24 |  21 - 16  |  15 - 8     |  7-0  |
 *  + ===== + --- + ----+ ----- + ----- + --------- + ----------- + ----- +
 *  | Flags |  V  | CNV | CNVNR | Rsvd  | Serv Type | Serv Cmd Id |  Rsvd |
 *  + ===== + --- + --- + ----- + ----- + --------- + ----------- + ----- + */
/**< @ingroup icp_qat_fw_comn
 * Macros defining the bit position and mask of 'CNV' flag
 * within the hdr_flags field of LW0 (service response only) */
#define ICP_QAT_FW_COMN_CNV_FLAG_BITPOS 6
#define ICP_QAT_FW_COMN_CNV_FLAG_MASK 0x1

/**< @ingroup icp_qat_fw_comn
 * Macros defining the bit position and mask of CNVNR flag
 * within the hdr_flags field of LW0 (service response only) */
#define ICP_QAT_FW_COMN_CNVNR_FLAG_BITPOS 5
#define ICP_QAT_FW_COMN_CNVNR_FLAG_MASK 0x1

/**< @ingroup icp_qat_fw_comn
 * Macros defining the bit position and mask of Stored Blocks flag
 * within the hdr_flags field of LW0 (service response only)
 */
#define ICP_QAT_FW_COMN_ST_BLK_FLAG_BITPOS 4
#define ICP_QAT_FW_COMN_ST_BLK_FLAG_MASK 0x1

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of Service Type Field
 *
 * @param icp_qat_fw_comn_req_hdr_t  Structure 'icp_qat_fw_comn_req_hdr_t'
 *                                   to extract the Service Type Field
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_OV_SRV_TYPE_GET(icp_qat_fw_comn_req_hdr_t)             \
	icp_qat_fw_comn_req_hdr_t.service_type

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for setting of Service Type Field
 *
 * @param 'icp_qat_fw_comn_req_hdr_t' structure to set the Service
 *                                    Type Field
 * @param val    Value of the Service Type Field
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_OV_SRV_TYPE_SET(icp_qat_fw_comn_req_hdr_t, val)        \
	icp_qat_fw_comn_req_hdr_t.service_type = val

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of Service Command Id Field
 *
 * @param icp_qat_fw_comn_req_hdr_t  Structure 'icp_qat_fw_comn_req_hdr_t'
 *                                   to extract the Service Command Id Field
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_OV_SRV_CMD_ID_GET(icp_qat_fw_comn_req_hdr_t)           \
	icp_qat_fw_comn_req_hdr_t.service_cmd_id

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for setting of Service Command Id Field
 *
 * @param 'icp_qat_fw_comn_req_hdr_t' structure to set the
 *                                    Service Command Id Field
 * @param val    Value of the Service Command Id Field
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_OV_SRV_CMD_ID_SET(icp_qat_fw_comn_req_hdr_t, val)      \
	icp_qat_fw_comn_req_hdr_t.service_cmd_id = val

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Extract the valid flag from the request or response's header flags.
 *
 * @param hdr_t  Request or Response 'hdr_t' structure to extract the valid bit
 *               from the  'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_VALID_FLAG_GET(hdr_t)                              \
	ICP_QAT_FW_COMN_VALID_FLAG_GET(hdr_t.hdr_flags)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Extract the CNVNR flag from the header flags in the response only.
 *
 * @param hdr_t  Response 'hdr_t' structure to extract the CNVNR bit
 *               from the  'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_CNVNR_FLAG_GET(hdr_flags)                          \
	QAT_FIELD_GET(hdr_flags,                                               \
		      ICP_QAT_FW_COMN_CNVNR_FLAG_BITPOS,                       \
		      ICP_QAT_FW_COMN_CNVNR_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Extract the CNV flag from the header flags in the response only.
 *
 * @param hdr_t  Response 'hdr_t' structure to extract the CNV bit
 *               from the  'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_CNV_FLAG_GET(hdr_flags)                            \
	QAT_FIELD_GET(hdr_flags,                                               \
		      ICP_QAT_FW_COMN_CNV_FLAG_BITPOS,                         \
		      ICP_QAT_FW_COMN_CNV_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Set the valid bit in the request's header flags.
 *
 * @param hdr_t  Request or Response 'hdr_t' structure to set the valid bit
 * @param val    Value of the valid bit flag.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_VALID_FLAG_SET(hdr_t, val)                         \
	ICP_QAT_FW_COMN_VALID_FLAG_SET(hdr_t, val)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Common macro to extract the valid flag from the header flags field
 *      within the header structure (request or response).
 *
 * @param hdr_t  Structure (request or response) to extract the
 *               valid bit from the 'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_VALID_FLAG_GET(hdr_flags)                              \
	QAT_FIELD_GET(hdr_flags,                                               \
		      ICP_QAT_FW_COMN_VALID_FLAG_BITPOS,                       \
		      ICP_QAT_FW_COMN_VALID_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Extract the Stored Block flag from the header flags in the
 *      response only.
 *
 * @param hdr_flags  Response 'hdr' structure to extract the
 *                   Stored Block bit from the 'hdr_flags' field.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_ST_BLK_FLAG_GET(hdr_flags)                         \
	QAT_FIELD_GET(hdr_flags,                                               \
		      ICP_QAT_FW_COMN_ST_BLK_FLAG_BITPOS,                      \
		      ICP_QAT_FW_COMN_ST_BLK_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Set the Stored Block bit in the response's header flags.
 *
 * @param hdr_t  Response 'hdr_t' structure to set the ST_BLK bit
 * @param val    Value of the ST_BLK bit flag.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_ST_BLK_FLAG_SET(hdr_t, val)                        \
	QAT_FIELD_SET((hdr_t.hdr_flags),                                       \
		      (val),                                                   \
		      ICP_QAT_FW_COMN_ST_BLK_FLAG_BITPOS,                      \
		      ICP_QAT_FW_COMN_ST_BLK_FLAG_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Set the generation bit in the request's header flags.
 *
 * @param hdr_t  Request or Response 'hdr_t' structure to set the gen bit
 * @param val    Value of the generation bit flag.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_GENERATION_FLAG_SET(hdr_t, val)                    \
	ICP_QAT_FW_COMN_GENERATION_FLAG_SET(hdr_t, val)

/**
******************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*      Common macro to set the generation bit in the common header
*
* @param hdr_t  Structure (request or response) containing the header
*               flags field, to allow the generation bit to be set.
* @param val    Value of the generation bit flag.
*
*****************************************************************************/
#define ICP_QAT_FW_COMN_GENERATION_FLAG_SET(hdr_t, val)                        \
	QAT_FIELD_SET((hdr_t.hdr_flags),                                       \
		      (val),                                                   \
		      ICP_QAT_FW_COMN_GEN_FLAG_BITPOS,                         \
		      ICP_QAT_FW_COMN_GEN_FLAG_MASK)

/**
******************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*      Common macro to extract the generation flag from the header flags field
*      within the header structure (request or response).
*
* @param hdr_t  Structure (request or response) to extract the
*               generation bit from the 'hdr_flags' field.
*
*****************************************************************************/

#define ICP_QAT_FW_COMN_HDR_GENERATION_FLAG_GET(hdr_flags)                     \
	QAT_FIELD_GET(hdr_flags,                                               \
		      ICP_QAT_FW_COMN_GEN_FLAG_BITPOS,                         \
		      ICP_QAT_FW_COMN_GEN_FLAG_MASK)
/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Common macro to extract the remaining reserved flags from the header
	flags field within the header structure (request or response).
 *
 * @param hdr_t  Structure (request or response) to extract the
 *               remaining bits from the 'hdr_flags' field (excluding the
 *               valid flag).
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_HDR_RESRVD_FLD_GET(hdr_flags)                          \
	(hdr_flags & ICP_QAT_FW_COMN_HDR_RESRVD_FLD_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Common macro to set the valid bit in the header flags field within
 *      the header structure (request or response).
 *
 * @param hdr_t  Structure (request or response) containing the header
 *               flags field, to allow the valid bit to be set.
 * @param val    Value of the valid bit flag.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_VALID_FLAG_SET(hdr_t, val)                             \
	QAT_FIELD_SET((hdr_t.hdr_flags),                                       \
		      (val),                                                   \
		      ICP_QAT_FW_COMN_VALID_FLAG_BITPOS,                       \
		      ICP_QAT_FW_COMN_VALID_FLAG_MASK)

/**
******************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*      Macro that must be used when building the common header flags.
*      Note that all bits reserved field bits 0-6 (LW0) need to be forced to 0.
*
* @param ptr   Value of the valid flag
*****************************************************************************/

#define ICP_QAT_FW_COMN_HDR_FLAGS_BUILD(valid)                                 \
	(((valid)&ICP_QAT_FW_COMN_VALID_FLAG_MASK)                             \
	 << ICP_QAT_FW_COMN_VALID_FLAG_BITPOS)

/*
 *  < @ingroup icp_qat_fw_comn
 *  Common Request Flags Definition
 *  The bit offsets below are within the flags field. These are NOT relative to
 *  the memory word. Unused fields e.g. reserved bits, must be zeroed.
 *
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Bits [15:8]    |  15 |  14 |  13 |  12 |  11 |  10 |  9  |  8  |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Flags[15:8]    | Rsv | Rsv | Rsv | Rsv | Rsv | Rsv | Rsv | Rsv |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Bits  [7:0]    |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 *  | Flags [7:0]    | Rsv | Rsv | Rsv | Rsv | Rsv | BnP | Cdt | Ptr |
 *  + ===== + ------ + --- + --- + --- + --- + --- + --- + --- + --- +
 */

#define QAT_COMN_PTR_TYPE_BITPOS 0
/**< @ingroup icp_qat_fw_comn
 * Common Request Flags - Starting bit position indicating
 * Src&Dst Buffer Pointer type  */

#define QAT_COMN_PTR_TYPE_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * Common Request Flags - One bit mask used to determine
 * Src&Dst Buffer Pointer type */

#define QAT_COMN_CD_FLD_TYPE_BITPOS 1
/**< @ingroup icp_qat_fw_comn
 * Common Request Flags - Starting bit position indicating
 * CD Field type  */

#define QAT_COMN_CD_FLD_TYPE_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * Common Request Flags - One bit mask used to determine
 * CD Field type */

#define QAT_COMN_BNP_ENABLED_BITPOS 2
/**< @ingroup icp_qat_fw_comn
 * Common Request Flags - Starting bit position indicating
 * the source buffer contains batch of requests. if this
 * bit is set, source buffer is type of Batch And Pack OpData List
 * and the Ptr Type Bit only applies to Destination buffer. */

#define QAT_COMN_BNP_ENABLED_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * Batch And Pack Enabled Flag Mask - One bit mask used to determine
 * the source buffer is in Batch and Pack OpData Link List Mode. */

/* ========================================================================= */
/*                                       Pointer Type Flag definitions       */
/* ========================================================================= */
#define QAT_COMN_PTR_TYPE_FLAT 0x0
/**< @ingroup icp_qat_fw_comn
 * Constant value indicating Src&Dst Buffer Pointer type is flat
 * If Batch and Pack mode is enabled, only applies to Destination buffer.*/

#define QAT_COMN_PTR_TYPE_SGL 0x1
/**< @ingroup icp_qat_fw_comn
 * Constant value indicating Src&Dst Buffer Pointer type is SGL type
 * If Batch and Pack mode is enabled, only applies to Destination buffer.*/

#define QAT_COMN_PTR_TYPE_BATCH 0x2
/**< @ingroup icp_qat_fw_comn
 * Constant value indicating Src is a batch request
 * and Dst Buffer Pointer type is SGL type */

/* ========================================================================= */
/*                                       CD Field Flag definitions           */
/* ========================================================================= */
#define QAT_COMN_CD_FLD_TYPE_64BIT_ADR 0x0
/**< @ingroup icp_qat_fw_comn
 * Constant value indicating CD Field contains 64-bit address */

#define QAT_COMN_CD_FLD_TYPE_16BYTE_DATA 0x1
/**< @ingroup icp_qat_fw_comn
 * Constant value indicating CD Field contains 16 bytes of setup data */

/* ========================================================================= */
/*                       Batch And Pack Enable/Disable Definitions           */
/* ========================================================================= */
#define QAT_COMN_BNP_ENABLED 0x1
/**< @ingroup icp_qat_fw_comn
 * Constant value indicating Source buffer will point to Batch And Pack OpData
 * List */

#define QAT_COMN_BNP_DISABLED 0x0
/**< @ingroup icp_qat_fw_comn
 * Constant value indicating Source buffer will point to Batch And Pack OpData
 * List */

/**
******************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*      Macro that must be used when building the common request flags (for all
*      requests but comp BnP).
*      Note that all bits reserved field bits 2-15 (LW1) need to be forced to 0.
*
* @param ptr   Value of the pointer type flag
* @param cdt   Value of the cd field type flag
*****************************************************************************/
#define ICP_QAT_FW_COMN_FLAGS_BUILD(cdt, ptr)                                  \
	((((cdt)&QAT_COMN_CD_FLD_TYPE_MASK) << QAT_COMN_CD_FLD_TYPE_BITPOS) |  \
	 (((ptr)&QAT_COMN_PTR_TYPE_MASK) << QAT_COMN_PTR_TYPE_BITPOS))

/**
******************************************************************************
* @ingroup icp_qat_fw_comn
*
* @description
*      Macro that must be used when building the common request flags for comp
*      BnP service.
*      Note that all bits reserved field bits 3-15 (LW1) need to be forced to 0.
*
* @param ptr   Value of the pointer type flag
* @param cdt   Value of the cd field type flag
* @param bnp   Value of the bnp enabled flag
*****************************************************************************/
#define ICP_QAT_FW_COMN_FLAGS_BUILD_BNP(cdt, ptr, bnp)                         \
	((((cdt)&QAT_COMN_CD_FLD_TYPE_MASK) << QAT_COMN_CD_FLD_TYPE_BITPOS) |  \
	 (((ptr)&QAT_COMN_PTR_TYPE_MASK) << QAT_COMN_PTR_TYPE_BITPOS) |        \
	 (((bnp)&QAT_COMN_BNP_ENABLED_MASK) << QAT_COMN_BNP_ENABLED_BITPOS))

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the pointer type bit from the common flags
 *
 * @param flags      Flags to extract the pointer type bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_PTR_TYPE_GET(flags)                                    \
	QAT_FIELD_GET(flags, QAT_COMN_PTR_TYPE_BITPOS, QAT_COMN_PTR_TYPE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the cd field type bit from the common flags
 *
 * @param flags      Flags to extract the cd field type type bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_CD_FLD_TYPE_GET(flags)                                 \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_COMN_CD_FLD_TYPE_BITPOS,                             \
		      QAT_COMN_CD_FLD_TYPE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the bnp field type bit from the common flags
 *
 * @param flags      Flags to extract the bnp field type type bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_BNP_ENABLED_GET(flags)                                 \
	QAT_FIELD_GET(flags,                                                   \
		      QAT_COMN_BNP_ENABLED_BITPOS,                             \
		      QAT_COMN_BNP_ENABLED_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for setting the pointer type bit in the common flags
 *
 * @param flags      Flags in which Pointer Type bit will be set
 * @param val        Value of the bit to be set in flags
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_PTR_TYPE_SET(flags, val)                               \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_COMN_PTR_TYPE_BITPOS,                                \
		      QAT_COMN_PTR_TYPE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for setting the cd field type bit in the common flags
 *
 * @param flags      Flags in which Cd Field Type bit will be set
 * @param val        Value of the bit to be set in flags
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_CD_FLD_TYPE_SET(flags, val)                            \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_COMN_CD_FLD_TYPE_BITPOS,                             \
		      QAT_COMN_CD_FLD_TYPE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for setting the bnp field type bit in the common flags
 *
 * @param flags      Flags in which Bnp Field Type bit will be set
 * @param val        Value of the bit to be set in flags
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_BNP_ENABLE_SET(flags, val)                             \
	QAT_FIELD_SET(flags,                                                   \
		      val,                                                     \
		      QAT_COMN_BNP_ENABLED_BITPOS,                             \
		      QAT_COMN_BNP_ENABLED_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macros using the bit position and mask to set/extract the next
 *      and current id nibbles within the next_curr_id field of the
 *      content descriptor header block. Note that these are defined
 *      in the common header file, as they are used by compression, cipher
 *      and authentication.
 *
 * @param cd_ctrl_hdr_t      Content descriptor control block header pointer.
 * @param val                Value of the field being set.
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_NEXT_ID_BITPOS 4
#define ICP_QAT_FW_COMN_NEXT_ID_MASK 0xF0
#define ICP_QAT_FW_COMN_CURR_ID_BITPOS 0
#define ICP_QAT_FW_COMN_CURR_ID_MASK 0x0F

#define ICP_QAT_FW_COMN_NEXT_ID_GET(cd_ctrl_hdr_t)                             \
	((((cd_ctrl_hdr_t)->next_curr_id) & ICP_QAT_FW_COMN_NEXT_ID_MASK) >>   \
	 (ICP_QAT_FW_COMN_NEXT_ID_BITPOS))

#define ICP_QAT_FW_COMN_NEXT_ID_SET(cd_ctrl_hdr_t, val)                        \
	((cd_ctrl_hdr_t)->next_curr_id) =                                      \
	    ((((cd_ctrl_hdr_t)->next_curr_id) &                                \
	      ICP_QAT_FW_COMN_CURR_ID_MASK) |                                  \
	     ((val << ICP_QAT_FW_COMN_NEXT_ID_BITPOS) &                        \
	      ICP_QAT_FW_COMN_NEXT_ID_MASK))

#define ICP_QAT_FW_COMN_CURR_ID_GET(cd_ctrl_hdr_t)                             \
	(((cd_ctrl_hdr_t)->next_curr_id) & ICP_QAT_FW_COMN_CURR_ID_MASK)

#define ICP_QAT_FW_COMN_CURR_ID_SET(cd_ctrl_hdr_t, val)                        \
	((cd_ctrl_hdr_t)->next_curr_id) =                                      \
	    ((((cd_ctrl_hdr_t)->next_curr_id) &                                \
	      ICP_QAT_FW_COMN_NEXT_ID_MASK) |                                  \
	     ((val)&ICP_QAT_FW_COMN_CURR_ID_MASK))

/*
 *  < @ingroup icp_qat_fw_comn
 *  Common Status Field Definition  The bit offsets below are within the COMMON
 *  RESPONSE status field, assumed to be 8 bits wide. In the case of the PKE
 *  response (which follows the CPM 1.5 message format), the status field is 16
 *  bits wide.
 *  The status flags are contained within the most significant byte and align
 *  with the diagram below. Please therefore refer to the service-specific PKE
 *  header file for the appropriate macro definition to extract the PKE status
 *  flag from the PKE response, which assumes that a word is passed to the
 *  macro.
 *  + ===== + ------ + --- + --- + ---- + ---- + -------- + ---- + ---------- +
 *  |  Bit  |   7    |  6  |  5  |  4   |  3   |    2     |   1  |      0     |
 *  + ===== + ------ + --- + --- + ---- + ---- + -------- + ---- + ---------- +
 *  | Flags | Crypto | Pke | Cmp | Xlat | EOLB | UnSupReq | Rsvd | XltWaApply |
 *  + ===== + ------ + --- + --- + ---- + ---- + -------- + ---- + ---------- +
 * Note:
 * For the service specific status bit definitions refer to service header files
 * Eg. Crypto Status bit refers to Symmetric Crypto, Key Generation, and NRBG
 * Requests' Status. Unused bits e.g. reserved bits need to have been forced to
 * 0.
 */

#define QAT_COMN_RESP_CRYPTO_STATUS_BITPOS 7
/**< @ingroup icp_qat_fw_comn
 * Starting bit position indicating Response for Crypto service Flag */

#define QAT_COMN_RESP_CRYPTO_STATUS_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * One bit mask used to determine Crypto status mask */

#define QAT_COMN_RESP_PKE_STATUS_BITPOS 6
/**< @ingroup icp_qat_fw_comn
 * Starting bit position indicating Response for PKE service Flag */

#define QAT_COMN_RESP_PKE_STATUS_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * One bit mask used to determine PKE status mask */

#define QAT_COMN_RESP_CMP_STATUS_BITPOS 5
/**< @ingroup icp_qat_fw_comn
 * Starting bit position indicating Response for Compression service Flag */

#define QAT_COMN_RESP_CMP_STATUS_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * One bit mask used to determine Compression status mask */

#define QAT_COMN_RESP_XLAT_STATUS_BITPOS 4
/**< @ingroup icp_qat_fw_comn
 * Starting bit position indicating Response for Xlat service Flag */

#define QAT_COMN_RESP_XLAT_STATUS_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * One bit mask used to determine Translator status mask */

#define QAT_COMN_RESP_CMP_END_OF_LAST_BLK_BITPOS 3
/**< @ingroup icp_qat_fw_comn
 * Starting bit position indicating the last block in a deflate stream for
  the compression service Flag */

#define QAT_COMN_RESP_CMP_END_OF_LAST_BLK_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * One bit mask used to determine the last block in a deflate stream
   status mask */

#define QAT_COMN_RESP_UNSUPPORTED_REQUEST_BITPOS 2
/**< @ingroup icp_qat_fw_comn
 * Starting bit position indicating when an unsupported service request Flag */

#define QAT_COMN_RESP_UNSUPPORTED_REQUEST_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * One bit mask used to determine the unsupported service request status mask */

#define QAT_COMN_RESP_XLT_INV_APPLIED_BITPOS 0
/**< @ingroup icp_qat_fw_comn
 * Bit position indicating that firmware detected an invalid translation during
 * dynamic compression and took measures to overcome this
 *
 */

#define QAT_COMN_RESP_XLT_INV_APPLIED_MASK 0x1
/**< @ingroup icp_qat_fw_comn
 * One bit mask */

/**
 ******************************************************************************
 * @description
 *      Macro that must be used when building the status
 *      for the common response
 *
 * @param crypto   Value of the Crypto Service status flag
 * @param comp     Value of the Compression Service Status flag
 * @param xlat     Value of the Xlator Status flag
 * @param eolb     Value of the Compression End of Last Block Status flag
 * @param unsupp   Value of the Unsupported Request flag
 * @param xlt_inv  Value of the Invalid Translation flag
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_STATUS_BUILD(                                     \
    crypto, pke, comp, xlat, eolb, unsupp, xlt_inv)                            \
	((((crypto)&QAT_COMN_RESP_CRYPTO_STATUS_MASK)                          \
	  << QAT_COMN_RESP_CRYPTO_STATUS_BITPOS) |                             \
	 (((pke)&QAT_COMN_RESP_PKE_STATUS_MASK)                                \
	  << QAT_COMN_RESP_PKE_STATUS_BITPOS) |                                \
	 (((xlt_inv)&QAT_COMN_RESP_XLT_INV_APPLIED_MASK)                       \
	  << QAT_COMN_RESP_XLT_INV_APPLIED_BITPOS) |                           \
	 (((comp)&QAT_COMN_RESP_CMP_STATUS_MASK)                               \
	  << QAT_COMN_RESP_CMP_STATUS_BITPOS) |                                \
	 (((xlat)&QAT_COMN_RESP_XLAT_STATUS_MASK)                              \
	  << QAT_COMN_RESP_XLAT_STATUS_BITPOS) |                               \
	 (((eolb)&QAT_COMN_RESP_CMP_END_OF_LAST_BLK_MASK)                      \
	  << QAT_COMN_RESP_CMP_END_OF_LAST_BLK_BITPOS) |                       \
	 (((unsupp)&QAT_COMN_RESP_UNSUPPORTED_REQUEST_BITPOS)                  \
	  << QAT_COMN_RESP_UNSUPPORTED_REQUEST_MASK))

/* ========================================================================= */
/*                                                                   GETTERS */
/* ========================================================================= */
/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the Crypto bit from the status
 *
 * @param status
 *      Status to extract the status bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_CRYPTO_STAT_GET(status)                           \
	QAT_FIELD_GET(status,                                                  \
		      QAT_COMN_RESP_CRYPTO_STATUS_BITPOS,                      \
		      QAT_COMN_RESP_CRYPTO_STATUS_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the PKE bit from the status
 *
 * @param status
 *      Status to extract the status bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_PKE_STAT_GET(status)                              \
	QAT_FIELD_GET(status,                                                  \
		      QAT_COMN_RESP_PKE_STATUS_BITPOS,                         \
		      QAT_COMN_RESP_PKE_STATUS_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the Compression bit from the status
 *
 * @param status
 *      Status to extract the status bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_CMP_STAT_GET(status)                              \
	QAT_FIELD_GET(status,                                                  \
		      QAT_COMN_RESP_CMP_STATUS_BITPOS,                         \
		      QAT_COMN_RESP_CMP_STATUS_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the Translator bit from the status
 *
 * @param status
 *      Status to extract the status bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_XLAT_STAT_GET(status)                             \
	QAT_FIELD_GET(status,                                                  \
		      QAT_COMN_RESP_XLAT_STATUS_BITPOS,                        \
		      QAT_COMN_RESP_XLAT_STATUS_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the Translation Invalid bit
 *      from the status
 *
 * @param status
 *      Status to extract the status bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_XLT_INV_APPLIED_GET(status)                       \
	QAT_FIELD_GET(status,                                                  \
		      QAT_COMN_RESP_XLT_INV_APPLIED_BITPOS,                    \
		      QAT_COMN_RESP_XLT_INV_APPLIED_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the end of compression block bit from the
 *      status
 *
 * @param status
 *      Status to extract the status bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_CMP_END_OF_LAST_BLK_FLAG_GET(status)              \
	QAT_FIELD_GET(status,                                                  \
		      QAT_COMN_RESP_CMP_END_OF_LAST_BLK_BITPOS,                \
		      QAT_COMN_RESP_CMP_END_OF_LAST_BLK_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comn
 *
 * @description
 *      Macro for extraction of the Unsupported request from the status
 *
 * @param status
 *      Status to extract the status bit from
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMN_RESP_UNSUPPORTED_REQUEST_STAT_GET(status)              \
	QAT_FIELD_GET(status,                                                  \
		      QAT_COMN_RESP_UNSUPPORTED_REQUEST_BITPOS,                \
		      QAT_COMN_RESP_UNSUPPORTED_REQUEST_MASK)

/* ========================================================================= */
/*                                        Status Flag definitions */
/* ========================================================================= */

#define ICP_QAT_FW_COMN_STATUS_FLAG_OK 0
/**< @ingroup icp_qat_fw_comn
 * Definition of successful processing of a request */

#define ICP_QAT_FW_COMN_STATUS_FLAG_ERROR 1
/**< @ingroup icp_qat_fw_comn
 * Definition of erroneous processing of a request */

#define ICP_QAT_FW_COMN_STATUS_CMP_END_OF_LAST_BLK_FLAG_CLR 0
/**< @ingroup icp_qat_fw_comn
 * Final Deflate block of a compression request not completed */

#define ICP_QAT_FW_COMN_STATUS_CMP_END_OF_LAST_BLK_FLAG_SET 1
/**< @ingroup icp_qat_fw_comn
 * Final Deflate block of a compression request completed */

#define ERR_CODE_NO_ERROR 0
/**< Error Code constant value for no error  */

#define ERR_CODE_INVALID_BLOCK_TYPE -1
/* Invalid block type (type == 3)*/

#define ERR_CODE_NO_MATCH_ONES_COMP -2
/* Stored block length does not match one's complement */

#define ERR_CODE_TOO_MANY_LEN_OR_DIS -3
/* Too many length or distance codes */

#define ERR_CODE_INCOMPLETE_LEN -4
/* Code lengths codes incomplete */

#define ERR_CODE_RPT_LEN_NO_FIRST_LEN -5
/* Repeat lengths with no first length */

#define ERR_CODE_RPT_GT_SPEC_LEN -6
/* Repeat more than specified lengths */

#define ERR_CODE_INV_LIT_LEN_CODE_LEN -7
/* Invalid lit/len code lengths */

#define ERR_CODE_INV_DIS_CODE_LEN -8
/* Invalid distance code lengths */

#define ERR_CODE_INV_LIT_LEN_DIS_IN_BLK -9
/* Invalid lit/len or distance code in fixed/dynamic block */

#define ERR_CODE_DIS_TOO_FAR_BACK -10
/* Distance too far back in fixed or dynamic block */

/* Common Error code definitions */
#define ERR_CODE_OVERFLOW_ERROR -11
/**< Error Code constant value for overflow error  */

#define ERR_CODE_SOFT_ERROR -12
/**< Error Code constant value for soft error  */

#define ERR_CODE_FATAL_ERROR -13
/**< Error Code constant value for hard/fatal error  */

#define ERR_CODE_COMP_OUTPUT_CORRUPTION -14
/**< Error Code constant for compression output corruption */

#define ERR_CODE_HW_INCOMPLETE_FILE -15
/**< Error Code constant value for incomplete file hardware error  */

#define ERR_CODE_SSM_ERROR -16
/**< Error Code constant value for error detected by SSM e.g. slice hang  */

#define ERR_CODE_ENDPOINT_ERROR -17
/**< Error Code constant value for error detected by PCIe Endpoint, e.g. push
 * data error  */

#define ERR_CODE_CNV_ERROR -18
/**< Error Code constant value for cnv failure  */

#define ERR_CODE_EMPTY_DYM_BLOCK -19
/**< Error Code constant value for submission of empty dynamic stored block to
 * slice  */

#define ERR_CODE_REGION_OUT_OF_BOUNDS -21
/**< Error returned when decompression ends before the specified partial
 * decompression region was produced */

#define ERR_CODE_MISC_ERROR -50
/**< Error Code constant for error detected but the source
 * of error is not recognized */

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Slice types for building of the processing chain within the content
 *      descriptor
 *
 * @description
 *      Enumeration used to indicate the ids of the slice types through which
 *      data will pass.
 *
 *      A logical slice is not a hardware slice but is a software FSM
 *      performing the actions of a slice
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_FW_SLICE_NULL = 0,    /**< NULL slice type */
	ICP_QAT_FW_SLICE_CIPHER = 1,  /**< CIPHER slice type */
	ICP_QAT_FW_SLICE_AUTH = 2,    /**< AUTH slice type */
	ICP_QAT_FW_SLICE_DRAM_RD = 3, /**< DRAM_RD Logical slice type */
	ICP_QAT_FW_SLICE_DRAM_WR = 4, /**< DRAM_WR Logical slice type */
	ICP_QAT_FW_SLICE_COMP = 5,    /**< Compression slice type */
	ICP_QAT_FW_SLICE_XLAT = 6,    /**< Translator slice type */
	ICP_QAT_FW_SLICE_DELIMITER    /**< End delimiter */

} icp_qat_fw_slice_t;

#endif /* _ICP_QAT_FW_H_ */
