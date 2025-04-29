/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_key.h
 *
 * @defgroup    LacSymKey  Key Generation
 *
 * @ingroup     LacSym
 *
 * @lld_start
 *
 * @lld_overview
 *
 * Key generation component is responsible for SSL, TLS & MGF operations. All
 * memory required for the keygen operations is got from the keygen cookie
 * structure which is carved up as required.
 *
 * For SSL the QAT accelerates the nested hash function with MD5 as the
 * outer hash and SHA1 as the inner hash.
 *
 * Refer to sections in draft-freier-ssl-version3-02.txt:
 *      6.1 Asymmetric cryptographic computations - This refers to converting
 *          the pre-master secret to the master secret.
 *      6.2.2 Converting the master secret into keys and MAC secrets - Using
 *          the master secret to generate the key material.
 *
 * For TLS the QAT accelerates the PRF function as described in
 * rfc4346 - TLS version 1.1 (this obsoletes rfc2246 - TLS version 1.0)
 *      5. HMAC and the pseudorandom function - For the TLS PRF and getting
 *         S1 and S2 from the secret.
 *      6.3. Key calculation - For how the key material is generated
 *      7.4.9. Finished - How the finished message uses the TLS PRF
 *      8.1. Computing the master secret
 *
 *
 * @lld_dependencies
 * \ref LacSymQatHash: for building up hash content descriptor
 * \ref LacMem: for virt to phys conversions
 *
 * @lld_initialisation
 * The response handler is registered with Symmetric. The Maximum SSL is
 * allocated. A structure is allocated containing all the TLS labels that
 * are supported. On shutdown the memory for these structures are freed.
 *
 * @lld_module_algorithms
 * @lld_process_context
 *
 * @lld_end
 *
 *
 *****************************************************************************/
#ifndef LAC_SYM_KEY_H_
#define LAC_SYM_KEY_H_

#include "icp_qat_fw_la.h"
#include "cpa_cy_key.h"

/**< @ingroup LacSymKey
 * Label for SSL. Size is 136 bytes for 16 iterations, which can theroretically
 *  generate up to 256 bytes of output data. QAT will generate a maximum of
 * 255 bytes */

#define LAC_SYM_KEY_TLS_MASTER_SECRET_LABEL ("master secret")
/**< @ingroup LacSymKey
 * Label for TLS Master Secret Key Derivation, as defined in RFC4346 */

#define LAC_SYM_KEY_TLS_KEY_MATERIAL_LABEL ("key expansion")
/**< @ingroup LacSymKey
 * Label for TLS Key Material Generation, as defined in RFC4346. */

#define LAC_SYM_KEY_TLS_CLIENT_FIN_LABEL ("client finished")
/**< @ingroup LacSymKey
 * Label for TLS Client finished Message, as defined in RFC4346. */

#define LAC_SYM_KEY_TLS_SERVER_FIN_LABEL ("server finished")
/**< @ingroup LacSymKey
 * Label for TLS Server finished Message, as defined in RFC4346. */

/*
*******************************************************************************
* Define Constants and Macros for SSL, TLS and MGF
*******************************************************************************
*/

#define LAC_SYM_KEY_NO_HASH_BLK_OFFSET_QW 0
/**< Used to indicate there is no hash block offset in the content descriptor
 */

/*
*******************************************************************************
* Define Constant lengths for HKDF TLS v1.3 sublabels.
*******************************************************************************
*/
#define HKDF_SUB_LABEL_KEY_LENGTH ((Cpa8U)13)
#define HKDF_SUB_LABEL_IV_LENGTH ((Cpa8U)12)
#define HKDF_SUB_LABEL_RESUMPTION_LENGTH ((Cpa8U)20)
#define HKDF_SUB_LABEL_FINISHED_LENGTH ((Cpa8U)18)
#define HKDF_SUB_LABELS_ALL                                                    \
	(CPA_CY_HKDF_SUBLABEL_KEY | CPA_CY_HKDF_SUBLABEL_IV |                  \
	 CPA_CY_HKDF_SUBLABEL_RESUMPTION | CPA_CY_HKDF_SUBLABEL_FINISHED)
#define LAC_KEY_HKDF_SUBLABELS_NUM 4
#define LAC_KEY_HKDF_DIGESTS 0
#define LAC_KEY_HKDF_CIPHERS_MAX (CPA_CY_HKDF_TLS_AES_128_CCM_8_SHA256 + 1)
#define LAC_KEY_HKDF_SUBLABELS_MAX (LAC_KEY_HKDF_SUBLABELS_NUM + 1)

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      TLS label struct
 *
 * @description
 *      This structure is used to hold the various TLS labels. Each field is
 *      on an 8 byte boundary provided the structure itself is 8 bytes aligned.
 *****************************************************************************/
typedef struct lac_sym_key_tls_labels_s {
	Cpa8U masterSecret[ICP_QAT_FW_LA_TLS_LABEL_LEN_MAX];
	/**< Master secret label */
	Cpa8U keyMaterial[ICP_QAT_FW_LA_TLS_LABEL_LEN_MAX];
	/**< Key material label */
	Cpa8U clientFinished[ICP_QAT_FW_LA_TLS_LABEL_LEN_MAX];
	/**< client finished label */
	Cpa8U serverFinished[ICP_QAT_FW_LA_TLS_LABEL_LEN_MAX];
	/**< server finished label */
} lac_sym_key_tls_labels_t;

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      TLS HKDF sub label struct
 *
 * @description
 *      This structure is used to hold the various TLS HKDF sub labels.
 *      Each field is on an 8 byte boundary.
 *****************************************************************************/
typedef struct lac_sym_key_tls_hkdf_sub_labels_s {
	CpaCyKeyGenHKDFExpandLabel keySublabel256;
	/**< CPA_CY_HKDF_SUBLABEL_KEY */
	CpaCyKeyGenHKDFExpandLabel ivSublabel256;
	/**< CPA_CY_HKDF_SUBLABEL_IV */
	CpaCyKeyGenHKDFExpandLabel resumptionSublabel256;
	/**< CPA_CY_HKDF_SUBLABEL_RESUMPTION */
	CpaCyKeyGenHKDFExpandLabel finishedSublabel256;
	/**< CPA_CY_HKDF_SUBLABEL_FINISHED */
	CpaCyKeyGenHKDFExpandLabel keySublabel384;
	/**< CPA_CY_HKDF_SUBLABEL_KEY */
	CpaCyKeyGenHKDFExpandLabel ivSublabel384;
	/**< CPA_CY_HKDF_SUBLABEL_IV */
	CpaCyKeyGenHKDFExpandLabel resumptionSublabel384;
	/**< CPA_CY_HKDF_SUBLABEL_RESUMPTION */
	CpaCyKeyGenHKDFExpandLabel finishedSublabel384;
	/**< CPA_CY_HKDF_SUBLABEL_FINISHED */
	CpaCyKeyGenHKDFExpandLabel keySublabelChaChaPoly;
	/**< CPA_CY_HKDF_SUBLABEL_KEY */
	CpaCyKeyGenHKDFExpandLabel ivSublabelChaChaPoly;
	/**< CPA_CY_HKDF_SUBLABEL_IV */
	CpaCyKeyGenHKDFExpandLabel resumptionSublabelChaChaPoly;
	/**< CPA_CY_HKDF_SUBLABEL_RESUMPTION */
	CpaCyKeyGenHKDFExpandLabel finishedSublabelChaChaPoly;
	/**< CPA_CY_HKDF_SUBLABEL_FINISHED */
	Cpa64U sublabelPhysAddr256;
	/**< Physical address of the SHA-256 subLabels */
	Cpa64U sublabelPhysAddr384;
	/**< Physical address of the SHA-384 subLabels */
	Cpa64U sublabelPhysAddrChaChaPoly;
	/**< Physical address of the ChaChaPoly subLabels */
} lac_sym_key_tls_hkdf_sub_labels_t;

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      This function prints the stats to standard out.
 *
 * @retval CPA_STATUS_SUCCESS   Status Success
 * @retval CPA_STATUS_FAIL      General failure
 *
 *****************************************************************************/
void LacKeygen_StatsShow(CpaInstanceHandle instanceHandle);

#endif
