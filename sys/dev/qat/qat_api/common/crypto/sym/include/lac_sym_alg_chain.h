/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 *****************************************************************************
 * @file lac_sym_alg_chain.h
 *
 * @defgroup  LacAlgChain  Algorithm Chaining
 *
 * @ingroup LacSym
 *
 * Interfaces exposed by the Algorithm Chaining Component
 *
 * @lld_start
 *
 * @lld_overview
 * This is the LAC Algorithm-Chaining feature component.  This component
 * implements session registration and cleanup functions, and a perform
 * function.  Statistics are maintained to track requests issued and completed,
 * errors incurred, and  authentication verification failures.  For each
 * function the parameters  supplied by the client are checked, and then the
 * function proceeds if all the parameters are valid.  This component also
 * incorporates support for Authenticated-Encryption (CCM and GCM) which
 * essentially comprises of a cipher operation and a hash operation combined.
 *
 * This component can combine a cipher operation with a hash operation or just
 * simply create a hash only or cipher only operation and is called from the
 * LAC Symmetric API component. In turn it calls the LAC Cipher, LAC Hash, and
 * LAC Symmetric QAT components.  The goal here is to duplicate as little code
 * as possible from the Cipher and Hash components.
 *
 * The cipher and hash operations can be combined in either order, i.e. cipher
 * first then hash or hash first then cipher.  The client specifies this via
 * the algChainOrder field in the session context.  This ordering choice is
 * stored as part of the session descriptor, so that it is known when a
 * perform request is issued.  In the case of Authenticated-Encryption, the
 * ordering is an implicit part of the CCM or GCM protocol.
 *
 * When building a content descriptor, as part of session registration, this
 * component asks the Cipher and Hash components to build their respective
 * parts of the session descriptor.  The key aspect here is to provide the
 * correct offsets to the Cipher and Hash components for where in the content
 * descriptor to write their Config and Hardware Setup blocks.  Also the
 * Config block in each case must specify the appropriate next slice.
 *
 * When building request parameters, as part of a perform operation, this
 * component asks the Cipher and Hash components to build their respective
 * parts of the request parameters block.  Again the key aspect here is to
 * provide the correct offsets to the Cipher and Hash components for where in
 * the request parameters block to write their parameters.  Also the request
 * parameters block in each case must specify the appropriate next slice.
 *
 * Parameter checking for session registration and for operation perform is
 * mostly delegated to the Cipher and Hash components.  There are a few
 * extra checks that this component must perform: check the algChainOrder
 * parameter, ensure that CCM/GCM are specified for hash/cipher algorithms
 * as appropriate, and ensure that requests are for full packets (partial
 * packets are not supported for Algorithm-Chaining).
 *
 * The perform operation allocates a cookie to capture information required
 * in the request callback.  This cookie is then freed in the callback.
 *
 * @lld_dependencies
 * - \ref LacCipher "Cipher" : For taking care of the cipher aspects of
 *   session registration and operation perform
 * - \ref LacHash "Hash" : For taking care of the hash aspects of session
 *   registration and operation perform
 * - \ref LacSymCommon "Symmetric Common" : statistics.
 * - \ref LacSymQat "Symmetric QAT": To build the QAT request message,
 *   request param structure, and populate the content descriptor.  Also
 *   for registering a callback function to process the QAT response.
 * - \ref QatComms "QAT Comms" : For sending messages to the QAT, and for
 *   setting the response callback
 * - \ref LacMem "Mem" : For memory allocation and freeing, virtual/physical
 *   address translation, and translating between scalar and pointer types
 * - OSAL : For atomics and locking
 *
 * @lld_module_algorithms
 * This component builds up a chain of slices at session init time
 * and stores it in the session descriptor. This is used for building up the
 * content descriptor at session init time and the request parameters structure
 * in the perform operation.
 *
 * The offsets for the first slice are updated so that the second slice adds
 * its configuration information after that of the first slice. The first
 * slice also configures the next slice appropriately.
 *
 * This component is very much hard-coded to just support cipher+hash or
 * hash+cipher.  It should be quite possible to extend this idea to support
 * an arbitrary chain of commands, by building up a command chain that can
 * be traversed in order to build up the appropriate configuration for the
 * QAT.  This notion should be looked at in the future if other forms of
 * Algorithm-Chaining are desired.
 *
 * @lld_process_context
 *
 * @lld_end
 *
 *****************************************************************************/

/*****************************************************************************/

#ifndef LAC_SYM_ALG_CHAIN_H
#define LAC_SYM_ALG_CHAIN_H

/*
******************************************************************************
* Include public/global header files
******************************************************************************
*/

#include "cpa.h"
#include "cpa_cy_sym.h"
#include "lac_session.h"

/*
*******************************************************************************
* Include private header files
*******************************************************************************
*/

/* Macro for checking if zero length buffer are supported
 * only for cipher is AES-GCM and hash are AES-GCM/AES-GMAC */
#define IS_ZERO_LENGTH_BUFFER_SUPPORTED(cipherAlgo, hashAlgo)                  \
	(CPA_CY_SYM_CIPHER_AES_GCM == cipherAlgo &&                            \
	 (CPA_CY_SYM_HASH_AES_GMAC == hashAlgo ||                              \
	  CPA_CY_SYM_HASH_AES_GCM == hashAlgo))

/**
*******************************************************************************
* @ingroup LacAlgChain
*      This function registers a session for an Algorithm-Chaining operation.
*
* @description
*      This function is called from the LAC session register API function for
*      Algorithm-Chaining operations. It validates all input parameters. If
*      an invalid parameter is passed, an error is returned to the calling
*      function. If all parameters are valid an Algorithm-Chaining session is
*      registered.
*
* @param[in] instanceHandle    Instance Handle
*
* @param[in] pSessionCtx       Pointer to session context which contains
*                              parameters which are static for a given
*                              cryptographic session such as operation type,
*                              mechanisms, and keys for cipher and/or digest
*                              operations.
* @param[out] pSessionDesc     Pointer to session descriptor
*
* @retval CPA_STATUS_SUCCESS       Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_RESOURCE       Error related to system resources.
*
* @see cpaCySymInitSession()
*
*****************************************************************************/
CpaStatus LacAlgChain_SessionInit(const CpaInstanceHandle instanceHandle,
				  const CpaCySymSessionSetupData *pSessionCtx,
				  lac_session_desc_t *pSessionDesc);

/**
*******************************************************************************
* @ingroup LacAlgChain
*      Data path function for the Algorithm-Chaining component
*
* @description
*      This function gets called from cpaCySymPerformOp() which is the
*      symmetric LAC API function. It is the data path function for the
*      Algorithm-Chaining component. It does the parameter checking on the
*      client supplied parameters and if the parameters are valid, the
*      operation is performed and a request sent to the QAT, otherwise an
*      error is returned to the client.
*
* @param[in] instanceHandle    Instance Handle
*
* @param[in] pSessionDesc  Pointer to session descriptor
* @param[in] pCallbackTag    The application's context for this call
* @param[in] pOpData       Pointer to a structure containing request
*                          parameters. The client code allocates the memory for
*                          this structure. This component takes ownership of
*                          the memory until it is returned in the callback.
*
* @param[in] pSrcBuffer        Source Buffer List
* @param[out] pDstBuffer       Destination Buffer List
* @param[out] pVerifyResult    Verify Result
*
* @retval CPA_STATUS_SUCCESS        Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_RESOURCE       Error related to system resource.
*
* @see cpaCySymPerformOp()
*
*****************************************************************************/
CpaStatus LacAlgChain_Perform(const CpaInstanceHandle instanceHandle,
			      lac_session_desc_t *pSessionDesc,
			      void *pCallbackTag,
			      const CpaCySymOpData *pOpData,
			      const CpaBufferList *pSrcBuffer,
			      CpaBufferList *pDstBuffer,
			      CpaBoolean *pVerifyResult);

/**
*******************************************************************************
* @ingroup LacAlgChain
*      This function is used to update cipher key, as specified in provided
*      input.
*
* @description
*      This function is called from the LAC session register API function for
*      Algorithm-Chaining operations. It validates all input parameters. If
*      an invalid parameter is passed, an error is returned to the calling
*      function. If all parameters are valid an Algorithm-Chaining session is
*      updated.
*
* @threadSafe
*      No
*
* @param[in] pSessionDesc           Pointer to session descriptor
* @param[in] pCipherKey             Pointer to new cipher key.
*
* @retval CPA_STATUS_SUCCESS        Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_RETRY          Resubmit the request.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
*
*****************************************************************************/
CpaStatus LacAlgChain_SessionCipherKeyUpdate(lac_session_desc_t *pSessionDesc,
					     Cpa8U *pCipherKey);

/**
*******************************************************************************
* @ingroup LacAlgChain
*      This function is used to update authentication key, as specified in
*      provided input.
*
* @description
*      This function is called from the LAC session register API function for
*      Algorithm-Chaining operations. It validates all input parameters. If
*      an invalid parameter is passed, an error is returned to the calling
*      function. If all parameters are valid an Algorithm-Chaining session is
*      updated.
*
* @threadSafe
*      No
*
* @param[in] pSessionDesc           Pointer to session descriptor
* @param[in] pCipherKey             Pointer to new authentication key.
*
* @retval CPA_STATUS_SUCCESS        Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_RETRY          Resubmit the request.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
*
*****************************************************************************/
CpaStatus LacAlgChain_SessionAuthKeyUpdate(lac_session_desc_t *pSessionDesc,
					   Cpa8U *pAuthKey);

/**
*******************************************************************************
* @ingroup LacAlgChain
*      This function is used to update AAD length as specified in provided
*      input.
*
* @description
*      This function is called from the LAC session register API function for
*      Algorithm-Chaining operations. It validates all input parameters. If
*      an invalid parameter is passed, an error is returned to the calling
*      function. If all parameters are valid an Algorithm-Chaining session is
*      updated.
*
* @threadSafe
*      No
*
* @param[in] pSessionDesc           Pointer to session descriptor
* @param[in] newAADLength           New AAD length.
*
* @retval CPA_STATUS_SUCCESS        Function executed successfully.
* @retval CPA_STATUS_FAIL           Function failed.
* @retval CPA_STATUS_RETRY          Resubmit the request.
* @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
* @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
*
*****************************************************************************/
CpaStatus LacAlgChain_SessionAADUpdate(lac_session_desc_t *pSessionDesc,
				       Cpa32U newAADLength);

#endif /* LAC_SYM_ALG_CHAIN_H */
