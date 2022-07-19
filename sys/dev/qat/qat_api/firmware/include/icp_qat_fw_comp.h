/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file icp_qat_fw_comp.h
 * @defgroup icp_qat_fw_comp ICP QAT FW Compression Service
 *           Interface Definitions
 * @ingroup icp_qat_fw
 * @description
 *      This file documents structs used to provide the interface to the
 *      Compression QAT FW service
 *
 *****************************************************************************/

#ifndef _ICP_QAT_FW_COMP_H_
#define _ICP_QAT_FW_COMP_H_

/*
******************************************************************************
* Include local header files
******************************************************************************
*/
#include "icp_qat_fw.h"

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comp
 *        Definition of the Compression command types
 * @description
 *        Enumeration which is used to indicate the ids of functions
 *        that are exposed by the Compression QAT FW service
 *
 *****************************************************************************/

typedef enum {
	ICP_QAT_FW_COMP_CMD_STATIC = 0,
	/*!< Static Compress Request */

	ICP_QAT_FW_COMP_CMD_DYNAMIC = 1,
	/*!< Dynamic Compress Request */

	ICP_QAT_FW_COMP_CMD_DECOMPRESS = 2,
	/*!< Decompress Request */

	ICP_QAT_FW_COMP_CMD_DELIMITER
	/**< Delimiter type */

} icp_qat_fw_comp_cmd_id_t;

/*
 *  REQUEST FLAGS IN COMMON COMPRESSION
 *  In common message it is named as SERVICE SPECIFIC FLAGS.
 *
 *  + ===== + ------ + ------ + --- + -----  + ----- + ----- + -- + ---- + --- +
 *  |  Bit  | 15 - 8 |   7    |  6  |   5    |   4   |   3   |  2 |   1  |  0  |
 *  + ===== + ------ + -----  + --- + -----  + ----- + ----- + -- + ---- + --- +
 *  | Flags |  Rsvd  |  Dis.  |Resvd| Dis.   | Enh.  |Auto   |Sess| Rsvd | Rsvd|
 *  |       |  Bits  | secure |  =0 | Type0  | ASB   |Select |Type| = 0  | = 0 |
 *  |       |  = 0   |RAM use |     | Header |       |Best   |    |      |     |
 *  |       |        |as intmd|     |        |       |       |    |      |     |
 *  |       |        |  buf   |     |        |       |       |    |      |     |
 *  + ===== + ------ + -----  + --- + ------ + ----- + ----- + -- + ---- + --- +
 */

/** Flag usage */

#define ICP_QAT_FW_COMP_STATELESS_SESSION 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that session is stateless */

#define ICP_QAT_FW_COMP_STATEFUL_SESSION 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that session is stateful  */

#define ICP_QAT_FW_COMP_NOT_AUTO_SELECT_BEST 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that autoselectbest is NOT used */

#define ICP_QAT_FW_COMP_AUTO_SELECT_BEST 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that autoselectbest is used */

#define ICP_QAT_FW_COMP_NOT_ENH_AUTO_SELECT_BEST 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is NOT used */

#define ICP_QAT_FW_COMP_ENH_AUTO_SELECT_BEST 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is used */

#define ICP_QAT_FW_COMP_NOT_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is NOT used */

#define ICP_QAT_FW_COMP_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that enhanced autoselectbest is used */

#define ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_USED_AS_INTMD_BUF 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing secure RAM from being used as
 * an intermediate buffer is DISABLED.  */

#define ICP_QAT_FW_COMP_ENABLE_SECURE_RAM_USED_AS_INTMD_BUF 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing secure RAM from being used as
 * an intermediate buffer is ENABLED.  */

/** Flag mask & bit position */

#define ICP_QAT_FW_COMP_SESSION_TYPE_BITPOS 2
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for the session type */

#define ICP_QAT_FW_COMP_SESSION_TYPE_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask used to determine the session type */

#define ICP_QAT_FW_COMP_AUTO_SELECT_BEST_BITPOS 3
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for auto select best */

#define ICP_QAT_FW_COMP_AUTO_SELECT_BEST_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for auto select best */

#define ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_BITPOS 4
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for enhanced auto select best */

#define ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for enhanced auto select best */

#define ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_BITPOS 5
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for disabling type zero header write back
   when Enhanced autoselect best is enabled. If set firmware does
   not return type0 store block header, only copies src to dest.
   (if best output is Type0) */

#define ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for auto select best */

#define ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_BITPOS 7
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for flag used to disable secure ram from
 * being used as an intermediate buffer. */

#define ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for disable secure ram for use as an intermediate
   buffer.  */

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 * Macro used for the generation of the command flags for Compression Request.
 * This should always be used for the generation of the flags. No direct sets or
 * masks should be performed on the flags data
 *
 * @param sesstype         Session Type
 * @param autoselect       AutoSelectBest
 * @enhanced_asb           Enhanced AutoSelectBest
 * @ret_uncomp             RetUnCompressed
 * @secure_ram             Secure Ram usage
 *
 *********************************************************************************/
#define ICP_QAT_FW_COMP_FLAGS_BUILD(                                           \
    sesstype, autoselect, enhanced_asb, ret_uncomp, secure_ram)                \
	(((sesstype & ICP_QAT_FW_COMP_SESSION_TYPE_MASK)                       \
	  << ICP_QAT_FW_COMP_SESSION_TYPE_BITPOS) |                            \
	 ((autoselect & ICP_QAT_FW_COMP_AUTO_SELECT_BEST_MASK)                 \
	  << ICP_QAT_FW_COMP_AUTO_SELECT_BEST_BITPOS) |                        \
	 ((enhanced_asb & ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_MASK)      \
	  << ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_BITPOS) |               \
	 ((ret_uncomp & ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_MASK)    \
	  << ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_BITPOS) |           \
	 ((secure_ram & ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_MASK)  \
	  << ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_BITPOS))

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the session type bit
 *
 * @param flags        Flags to extract the session type bit from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_SESSION_TYPE_GET(flags)                                \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_SESSION_TYPE_BITPOS,                     \
		      ICP_QAT_FW_COMP_SESSION_TYPE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the autoSelectBest bit
 *
 * @param flags        Flags to extract the autoSelectBest bit from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_AUTO_SELECT_BEST_GET(flags)                            \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_AUTO_SELECT_BEST_BITPOS,                 \
		      ICP_QAT_FW_COMP_AUTO_SELECT_BEST_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the enhanced asb bit
 *
 * @param flags        Flags to extract the enhanced asb bit from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_EN_ASB_GET(flags)                                      \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_BITPOS,        \
		      ICP_QAT_FW_COMP_ENHANCED_AUTO_SELECT_BEST_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the RetUncomp bit
 *
 * @param flags        Flags to extract the Ret Uncomp bit from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_RET_UNCOMP_GET(flags)                                  \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_BITPOS,    \
		      ICP_QAT_FW_COMP_RET_DISABLE_TYPE0_HEADER_DATA_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the Secure Ram usage bit
 *
 * @param flags        Flags to extract the Secure Ram usage from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_SECURE_RAM_USE_GET(flags)                              \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_BITPOS,  \
		      ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_AS_INTMD_BUF_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *        Definition of the compression header cd pars block
 * @description
 *      Definition of the compression processing cd pars block.
 *      The structure is a service-specific implementation of the common
 *      structure.
 ******************************************************************************/
typedef union icp_qat_fw_comp_req_hdr_cd_pars_s {
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
		 * cipher
		 * processing. */

		uint8_t content_desc_hdr_resrvd2;
		/**< Content descriptor reserved field */

		uint32_t content_desc_resrvd3;
		/**< Content descriptor reserved field */
	} s;

	struct {
		uint32_t comp_slice_cfg_word[ICP_QAT_FW_NUM_LONGWORDS_2];
		/* Compression Slice Config Word */

		uint32_t content_desc_resrvd4;
		/**< Content descriptor reserved field */
	} sl;

} icp_qat_fw_comp_req_hdr_cd_pars_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *        Definition of the compression request parameters block
 * @description
 *      Definition of the compression processing request parameters block.
 *      The structure below forms part of the Compression + Translation
 *      Parameters block spanning LWs 14-21, thus differing from the common
 *      base Parameters block structure. Unused fields must be set to 0.
 *
 ******************************************************************************/
typedef struct icp_qat_fw_comp_req_params_s {
	/**< LW 14 */
	uint32_t comp_len;
	/**< Size of input to process in bytes Note:  Only EOP requests can be
	 * odd
	 * for decompression. IA must set LSB to zero for odd sized intermediate
	 * inputs */

	/**< LW 15 */
	uint32_t out_buffer_sz;
	/**< Size of output buffer in bytes */

	/**< LW 16 */
	union {
		struct {
			/** LW 16 */
			uint32_t initial_crc32;
			/**< CRC for processed bytes (input byte count) */

			/** LW 17 */
			uint32_t initial_adler;
			/**< Adler for processed bytes (input byte count) */
		} legacy;

		/** LW 16-17 */
		uint64_t crc_data_addr;
		/**< CRC data structure pointer */
	} crc;

	/** LW 18 */
	uint32_t req_par_flags;

	/** LW 19 */
	uint32_t rsrvd;

} icp_qat_fw_comp_req_params_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 * Macro used for the generation of the request parameter flags.
 * This should always be used for the generation of the flags. No direct sets or
 * masks should be performed on the flags data
 *
 * @param sop                SOP Flag, 0 restore, 1 don't restore
 * @param eop                EOP Flag, 0 restore, 1 don't restore
 * @param bfinal             Set bfinal in this block or not
 * @param cnv                Whether internal CNV check is to be performed
 *                            * ICP_QAT_FW_COMP_NO_CNV
 *                            * ICP_QAT_FW_COMP_CNV
 * @param cnvnr              Whether internal CNV recovery is to be performed
 *                            * ICP_QAT_FW_COMP_NO_CNV_RECOVERY
 *                            * ICP_QAT_FW_COMP_CNV_RECOVERY
 * @param crc                CRC Mode Flag - 0 legacy, 1 crc data struct
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(                                 \
    sop, eop, bfinal, cnv, cnvnr, crc)                                         \
	(((sop & ICP_QAT_FW_COMP_SOP_MASK) << ICP_QAT_FW_COMP_SOP_BITPOS) |    \
	 ((eop & ICP_QAT_FW_COMP_EOP_MASK) << ICP_QAT_FW_COMP_EOP_BITPOS) |    \
	 ((bfinal & ICP_QAT_FW_COMP_BFINAL_MASK)                               \
	  << ICP_QAT_FW_COMP_BFINAL_BITPOS) |                                  \
	 ((cnv & ICP_QAT_FW_COMP_CNV_MASK) << ICP_QAT_FW_COMP_CNV_BITPOS) |    \
	 ((cnvnr & ICP_QAT_FW_COMP_CNV_RECOVERY_MASK)                          \
	  << ICP_QAT_FW_COMP_CNV_RECOVERY_BITPOS) |                            \
	 ((crc & ICP_QAT_FW_COMP_CRC_MODE_MASK)                                \
	  << ICP_QAT_FW_COMP_CRC_MODE_BITPOS))

/*
 *  REQUEST FLAGS IN REQUEST PARAMETERS COMPRESSION
 *
 *  + ===== + ----- + --- +-----+-------+ --- + ---------+ --- + ---- + --- +
 * --- +
 *  |  Bit  | 31-20 |  19 |  18 |   17  | 16  |  15 - 7  |  6  |  5-2 |  1  |  0
 * |
 *  + ===== + ----- + --- +-----+-------+ --- + ---------+ --- | ---- + --- +
 * --- +
 *  | Flags | Resvd | CRC |Resvd| CNVNR | CNV |Resvd Bits|BFin |Resvd | EOP |
 * SOP |
 *  |       | =0    | Mode| =0  |       |     | =0       |     | =0   |     | |
 *  |       |       |     |     |       |     |          |     |      |     | |
 *  + ===== + ----- + --- +-----+-------+ --- + ---------+ --- | ---- + --- +
 * --- +
 */

#define ICP_QAT_FW_COMP_NOT_SOP 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request is NOT Start of Packet */

#define ICP_QAT_FW_COMP_SOP 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request IS Start of Packet */

#define ICP_QAT_FW_COMP_NOT_EOP 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request is NOT Start of Packet */

#define ICP_QAT_FW_COMP_EOP 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing that a request IS End of Packet */

#define ICP_QAT_FW_COMP_NOT_BFINAL 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing to indicate firmware this is not the last block */

#define ICP_QAT_FW_COMP_BFINAL 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing to indicate firmware this is the last block */

#define ICP_QAT_FW_COMP_NO_CNV 0
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that NO cnv check is to be performed on the request */

#define ICP_QAT_FW_COMP_CNV 1
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that a cnv check IS to be performed on the request */

#define ICP_QAT_FW_COMP_NO_CNV_RECOVERY 0
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that NO cnv recovery is to be performed on the request */

#define ICP_QAT_FW_COMP_CNV_RECOVERY 1
/**< @ingroup icp_qat_fw_comp
 * Flag indicating that a cnv recovery is to be performed on the request */

#define ICP_QAT_FW_COMP_CRC_MODE_LEGACY 0
/**< @ingroup icp_qat_fw_comp
 * Flag representing to use the legacy CRC mode */

#define ICP_QAT_FW_COMP_CRC_MODE_E2E 1
/**< @ingroup icp_qat_fw_comp
 * Flag representing to use the external CRC data struct */

#define ICP_QAT_FW_COMP_SOP_BITPOS 0
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for SOP */

#define ICP_QAT_FW_COMP_SOP_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask used to determine SOP */

#define ICP_QAT_FW_COMP_EOP_BITPOS 1
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for EOP */

#define ICP_QAT_FW_COMP_EOP_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask used to determine EOP */

#define ICP_QAT_FW_COMP_BFINAL_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for the bfinal bit */

#define ICP_QAT_FW_COMP_BFINAL_BITPOS 6
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for the bfinal bit */

#define ICP_QAT_FW_COMP_CNV_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for the CNV bit */

#define ICP_QAT_FW_COMP_CNV_BITPOS 16
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for the CNV bit */

#define ICP_QAT_FW_COMP_CNV_RECOVERY_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask for the CNV Recovery bit */

#define ICP_QAT_FW_COMP_CNV_RECOVERY_BITPOS 17
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for the CNV Recovery bit */

#define ICP_QAT_FW_COMP_CRC_MODE_BITPOS 19
/**< @ingroup icp_qat_fw_comp
 * Starting bit position for CRC mode */

#define ICP_QAT_FW_COMP_CRC_MODE_MASK 0x1
/**< @ingroup icp_qat_fw_comp
 * One bit mask used to determine CRC mode */

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the SOP bit
 *
 * @param flags        Flags to extract the SOP bit from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_SOP_GET(flags)                                         \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_SOP_BITPOS,                              \
		      ICP_QAT_FW_COMP_SOP_MASK)

/**
******************************************************************************
* @ingroup icp_qat_fw_comp
*
* @description
*        Macro for extraction of the EOP bit
*
* @param flags        Flags to extract the EOP bit from
*
*****************************************************************************/
#define ICP_QAT_FW_COMP_EOP_GET(flags)                                         \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_EOP_BITPOS,                              \
		      ICP_QAT_FW_COMP_EOP_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the bfinal bit
 *
 * @param flags        Flags to extract the bfinal bit from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_BFINAL_GET(flags)                                      \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_BFINAL_BITPOS,                           \
		      ICP_QAT_FW_COMP_BFINAL_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the CNV bit
 *
 * @param flags        Flag set containing the CNV flag
 *
 *****************************************************************************/
#define ICP_QAT_FW_COMP_CNV_GET(flags)                                         \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_CNV_BITPOS,                              \
		      ICP_QAT_FW_COMP_CNV_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *        Macro for extraction of the crc mode bit
 *
 * @param flags        Flags to extract the crc mode bit from
 *
 ******************************************************************************/
#define ICP_QAT_FW_COMP_CRC_MODE_GET(flags)                                    \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMP_CRC_MODE_BITPOS,                         \
		      ICP_QAT_FW_COMP_CRC_MODE_MASK)

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *        Definition of the translator request parameters block
 * @description
 *        Definition of the translator processing request parameters block
 *        The structure below forms part of the Compression + Translation
 *        Parameters block spanning LWs 20-21, thus differing from the common
 *        base Parameters block structure. Unused fields must be set to 0.
 *
 ******************************************************************************/
typedef struct icp_qat_fw_xlt_req_params_s {
	/**< LWs 20-21 */
	uint64_t inter_buff_ptr;
	/**< This field specifies the physical address of an intermediate
	  *  buffer SGL array. The array contains a pair of 64-bit
	  *  intermediate buffer pointers to SGL buffer descriptors, one pair
	  *  per CPM. Please refer to the CPM1.6 Firmware Interface HLD
	  *  specification for more details. */
} icp_qat_fw_xlt_req_params_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *      Compression header of the content descriptor block
 * @description
 *      Definition of the service-specific compression control block header
 *      structure. The compression parameters are defined per algorithm
 *      and are located in the icp_qat_hw.h file. This compression
 *      cd block spans LWs 24-29, forming part of the compression + translation
 *      cd block, thus differing from the common base content descriptor
 *      structure.
 *
 ******************************************************************************/
typedef struct icp_qat_fw_comp_cd_hdr_s {
	/**< LW 24 */
	uint16_t ram_bank_flags;
	/**< Flags to show which ram banks to access */

	uint8_t comp_cfg_offset;
	/**< Quad word offset from the content descriptor parameters address to
	 * the
	 * parameters for the compression processing */

	uint8_t next_curr_id;
	/**< This field combines the next and current id (each four bits) -
	  * the next id is the most significant nibble.
	  * Next Id:  Set to the next slice to pass the compressed data through.
	  * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	  * anymore slices after compression
	  * Current Id: Initialised with the compression slice type */

	/**< LW 25 */
	uint32_t resrvd;

	/**< LWs 26-27 */
	uint64_t comp_state_addr;
	/**< Pointer to compression state */

	/**< LWs 28-29 */
	uint64_t ram_banks_addr;
	/**< Pointer to banks */

} icp_qat_fw_comp_cd_hdr_t;

#define COMP_CPR_INITIAL_CRC 0
#define COMP_CPR_INITIAL_ADLER 1

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *      Translator content descriptor header block
 * @description
 *      Definition of the structure used to describe the translation processing
 *      to perform on data. The translator parameters are defined per algorithm
 *      and are located in the icp_qat_hw.h file. This translation cd block
 *      spans LWs 30-31, forming part of the compression + translation cd block,
 *      thus differing from the common base content descriptor structure.
 *
 ******************************************************************************/
typedef struct icp_qat_fw_xlt_cd_hdr_s {
	/**< LW 30 */
	uint16_t resrvd1;
	/**< Reserved field and assumed set to 0 */

	uint8_t resrvd2;
	/**< Reserved field and assumed set to 0 */

	uint8_t next_curr_id;
	/**< This field combines the next and current id (each four bits) -
	  * the next id is the most significant nibble.
	  * Next Id:  Set to the next slice to pass the translated data through.
	  * Set to ICP_QAT_FW_SLICE_DRAM_WR if the data is not to go through
	  * any more slices after compression
	  * Current Id: Initialised with the translation slice type */

	/**< LW 31 */
	uint32_t resrvd3;
	/**< Reserved and should be set to zero, needed for quadword alignment
	 */
} icp_qat_fw_xlt_cd_hdr_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *        Definition of the common Compression QAT FW request
 * @description
 *        This is a definition of the full request structure for
 *        compression and translation.
 *
 ******************************************************************************/
typedef struct icp_qat_fw_comp_req_s {
	/**< LWs 0-1 */
	icp_qat_fw_comn_req_hdr_t comn_hdr;
	/**< Common request header - for Service Command Id,
	 * use service-specific Compression Command Id.
	 * Service Specific Flags - use Compression Command Flags */

	/**< LWs 2-5 */
	icp_qat_fw_comp_req_hdr_cd_pars_t cd_pars;
	/**< Compression service-specific content descriptor field which points
	 * either to a content descriptor parameter block or contains the
	 * compression slice config word. */

	/**< LWs 6-13 */
	icp_qat_fw_comn_req_mid_t comn_mid;
	/**< Common request middle section */

	/**< LWs 14-19 */
	icp_qat_fw_comp_req_params_t comp_pars;
	/**< Compression request Parameters block */

	/**< LWs 20-21 */
	union {
		icp_qat_fw_xlt_req_params_t xlt_pars;
		/**< Translation request Parameters block */

		uint32_t resrvd1[ICP_QAT_FW_NUM_LONGWORDS_2];
		/**< Reserved if not used for translation */
	} u1;

	/**< LWs 22-23 */
	union {
		uint32_t resrvd2[ICP_QAT_FW_NUM_LONGWORDS_2];
		/**< Reserved - not used if Batch and Pack is disabled.*/

		uint64_t bnp_res_table_addr;
		/**< A generic pointer to the unbounded list of
		 * icp_qat_fw_resp_comp_pars_t members. This pointer is only
		 * used when the Batch and Pack is enabled. */
	} u3;

	/**< LWs 24-29 */
	icp_qat_fw_comp_cd_hdr_t comp_cd_ctrl;
	/**< Compression request content descriptor control
	 * block header */

	/**< LWs 30-31 */
	union {
		icp_qat_fw_xlt_cd_hdr_t xlt_cd_ctrl;
		/**< Translation request content descriptor
		 * control block header */

		uint32_t resrvd3[ICP_QAT_FW_NUM_LONGWORDS_2];
		/**< Reserved if not used for translation */
	} u2;

} icp_qat_fw_comp_req_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *        Definition of the compression QAT FW response descriptor
 *        parameters
 * @description
 *        This part of the response is specific to the compression response.
 *
 ******************************************************************************/
typedef struct icp_qat_fw_resp_comp_pars_s {
	/**< LW 4 */
	uint32_t input_byte_counter;
	/**< Input byte counter */

	/**< LW 5 */
	uint32_t output_byte_counter;
	/**< Output byte counter */

	/** LW 6-7 */
	union {
		struct {
			/** LW 6 */
			uint32_t curr_crc32;
			/**< Current CRC32 */

			/** LW 7 */
			uint32_t curr_adler_32;
			/**< Current Adler32 */
		} legacy;

		uint32_t resrvd[ICP_QAT_FW_NUM_LONGWORDS_2];
		/**< Reserved if not in legacy mode */
	} crc;

} icp_qat_fw_resp_comp_pars_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comp
 *       Definition of a single result metadata structure inside Batch and Pack
 *       results table array. It describes the output if single job in the
 *       batch and pack jobs.
 *       Total number of entries in BNP Out table shall be equal to total
 *       number of requests in the 'batch'.
 * @description
 *        This structure is specific to the compression output.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comp_bnp_out_tbl_entry_s {
	/**< LWs 0-3 */
	icp_qat_fw_resp_comp_pars_t comp_out_pars;
	/**< Common output params (checksums and byte counts) */

	/**< LW 4 */
	icp_qat_fw_comn_error_t comn_error;
	/**< This field is overloaded to allow for one 8 bit common error field
	 *   or two 8 bit error fields from compression and translator  */

	uint8_t comn_status;
	/**< Status field which specifies which slice(s) report an error */

	uint8_t reserved0;
	/**< Reserved, shall be set to zero */

	uint32_t reserved1;
	/**< Reserved, shall be set to zero,
	added for aligning entries to quadword boundary */
} icp_qat_fw_comp_bnp_out_tbl_entry_t;

/**
*****************************************************************************
* @ingroup icp_qat_fw_comp
*      Supported modes for skipping regions of input or output buffers.
*
* @description
*      This enumeration lists the supported modes for skipping regions of
*      input or output buffers.
*
*****************************************************************************/
typedef enum icp_qat_fw_comp_bnp_skip_mode_s {
	ICP_QAT_FW_SKIP_DISABLED = 0,
	/**< Skip mode is disabled */
	ICP_QAT_FW_SKIP_AT_START = 1,
	/**< Skip region is at the start of the buffer. */
	ICP_QAT_FW_SKIP_AT_END = 2,
	/**< Skip region is at the end of the buffer. */
	ICP_QAT_FW_SKIP_STRIDE = 3
	/**< Skip region occurs at regular intervals within the buffer.
	 specifies the number of bytes between each
	 skip region. */
} icp_qat_fw_comp_bnp_skip_mode_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Flags describing the skip and compression job bahaviour. refer to flag
 *      definitions on skip mode and reset/flush types.
 *      Note: compression behaviour flags are ignored for destination skip info.
 * @description
 *      Definition of the common request flags.
 *
 *****************************************************************************/
typedef uint8_t icp_qat_fw_comp_bnp_flags_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Skip Region Data.
 * @description
 *      This structure contains data relating to configuring skip region
 *      behaviour. A skip region is a region of an input buffer that
 *      should be omitted from processing or a region that should be inserted
 *      into the output buffer.
 *
 *****************************************************************************/
typedef struct icp_qat_fw_comp_bnp_skip_info_s {
	/**< LW 0 */
	uint16_t skip_length;
	/**<Number of bytes to skip when skip mode is enabled */

	/**< LW 1 */
	uint16_t stride_length;
	/**<Size of the stride between skip regions when skip mode is
	 * enabled */

	/**< LW 2 */
	uint16_t firstSkipOffset;
	/**< Number of bytes to skip in a buffer before reading/writing the
	 * input/output data. */

	/**< LWs 3 */
	icp_qat_fw_comp_bnp_flags_t bnp_flags;
	/**< Translation request Parameters block */

	uint8_t resrvd1;
	/**< Reserved if not used for translation */

} icp_qat_fw_comp_bnp_skip_info_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Batch and Pack operation header.
 * @description
 *      This structure contains address of the next bnp op data, and the
 *      length of the compression operation.
 *****************************************************************************/
typedef struct icp_qat_fw_comp_bnp_op_header_s {
	/**< LW 0*/
	uint64_t next_opdata_addr;
	/**< Physical pointer to the Address of the next bnp op data structure.
	*/

	/**< LW 2*/
	uint32_t comp_len;
	/**< Size of input to process in bytes  */

	/**< LW 3*/
	uint32_t resrvd1;
	/**< Reserved - Should be set to zero. */

} icp_qat_fw_comp_bnp_op_header_t;

/**
 *****************************************************************************
 * @ingroup icp_qat_fw_comn
 *      Batch and Pack operation op data structure.
 * @description
 *      This structure contains data relating to describing the skipping and
 *      reset behaviour of source and skipping behaviour of destination buffer
 *      associated with the input job where job has a single sgl vector, and
 *      batch and pack might contain multiple jobs.
 *      The structure also contains a pointer to the next 'job' described by
 *      the next op_data structure.
 *      Corresponding SGL Buffer shall physically follow this structure.
 *****************************************************************************/
typedef struct icp_qat_fw_comp_bnp_op_data_s {
	icp_qat_fw_comp_bnp_op_header_t bnp_op_header;
	/**< Pointer to next Op data, and general information on the operation
	*/
	icp_qat_fw_comp_bnp_skip_info_t src_bnp_skip_info;
	/**< Optional skip regions in the input buffers */
	icp_qat_fw_comp_bnp_skip_info_t dst_bnp_skip_info;
	/**< Optional skip regions in the output buffers */

} icp_qat_fw_comp_bnp_op_data_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *        Definition of the Compression Eagle Tail Response
 * @description
 *        This is the response delivered to the ET rings by the Compression
 *        QAT FW service for all commands
 *
 ******************************************************************************/
typedef struct icp_qat_fw_comp_resp_s {
	/**< LWs 0-1 */
	icp_qat_fw_comn_resp_hdr_t comn_resp;
	/**< Common interface response format see icp_qat_fw.h */

	/**< LWs 2-3 */
	uint64_t opaque_data;
	/**< Opaque data passed from the request to the response message */

	/**< LWs 4-7 */
	icp_qat_fw_resp_comp_pars_t comp_resp_pars;
	/**< Common response params (checksums and byte counts) */
} icp_qat_fw_comp_resp_t;

/* RAM Bank defines */
#define QAT_FW_COMP_BANK_FLAG_MASK 0x1

#define QAT_FW_COMP_BANK_I_BITPOS 8
#define QAT_FW_COMP_BANK_H_BITPOS 7
#define QAT_FW_COMP_BANK_G_BITPOS 6
#define QAT_FW_COMP_BANK_F_BITPOS 5
#define QAT_FW_COMP_BANK_E_BITPOS 4
#define QAT_FW_COMP_BANK_D_BITPOS 3
#define QAT_FW_COMP_BANK_C_BITPOS 2
#define QAT_FW_COMP_BANK_B_BITPOS 1
#define QAT_FW_COMP_BANK_A_BITPOS 0

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *      Definition of the ram bank enabled values
 * @description
 *      Enumeration used to define whether a ram bank is enabled or not
 *
 ******************************************************************************/
typedef enum {
	ICP_QAT_FW_COMP_BANK_DISABLED = 0, /*!< BANK DISABLED */
	ICP_QAT_FW_COMP_BANK_ENABLED = 1,  /*!< BANK ENABLED */
	ICP_QAT_FW_COMP_BANK_DELIMITER = 2 /**< Delimiter type */
} icp_qat_fw_comp_bank_enabled_t;

/**
 ******************************************************************************
 * @ingroup icp_qat_fw_comp
 *
 * @description
 *      Build the ram bank flags in the compression content descriptor
 *      which specify which banks are used to save history
 *
 * @param bank_i_enable
 * @param bank_h_enable
 * @param bank_g_enable
 * @param bank_f_enable
 * @param bank_e_enable
 * @param bank_d_enable
 * @param bank_c_enable
 * @param bank_b_enable
 * @param bank_a_enable
 ******************************************************************************/
#define ICP_QAT_FW_COMP_RAM_FLAGS_BUILD(bank_i_enable,                         \
					bank_h_enable,                         \
					bank_g_enable,                         \
					bank_f_enable,                         \
					bank_e_enable,                         \
					bank_d_enable,                         \
					bank_c_enable,                         \
					bank_b_enable,                         \
					bank_a_enable)                         \
	((((bank_i_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_I_BITPOS) |                                      \
	 (((bank_h_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_H_BITPOS) |                                      \
	 (((bank_g_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_G_BITPOS) |                                      \
	 (((bank_f_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_F_BITPOS) |                                      \
	 (((bank_e_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_E_BITPOS) |                                      \
	 (((bank_d_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_D_BITPOS) |                                      \
	 (((bank_c_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_C_BITPOS) |                                      \
	 (((bank_b_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_B_BITPOS) |                                      \
	 (((bank_a_enable)&QAT_FW_COMP_BANK_FLAG_MASK)                         \
	  << QAT_FW_COMP_BANK_A_BITPOS))

#endif /* _ICP_QAT_FW_COMP_H_ */
