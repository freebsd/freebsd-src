/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_cy_im.h
 *
 * @defgroup cpaCyInstMaint Cryptographic Instance Management API
 *
 * @ingroup cpaCy
 *
 * @description
 *      These functions specify the Instance Management API for available
 *      Cryptographic Instances. It is expected that these functions will only be
 *      called via a single system maintenance entity, rather than individual
 *      clients.
 *
 *****************************************************************************/

#ifndef CPA_CY_IM_H_
#define CPA_CY_IM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cpa_cy_common.h"

/**
 *****************************************************************************
 * @ingroup cpaCyInstMaint
 *      Cryptographic Component Initialization and Start function.
 *
 * @description
 *      This function will initialize and start the Cryptographic component.
 *      It MUST be called before any other crypto function is called. This
 *      function SHOULD be called only once (either for the very first time,
 *      or after an cpaCyStopInstance call which succeeded) per instance.
 *      Subsequent calls will have no effect.
 *
 * @context
 *      This function may sleep, and  MUST NOT be called in interrupt context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 * @param[out] instanceHandle       Handle to an instance of this API to be
 *                                  initialized.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed. Suggested course of action
 *                                  is to shutdown and restart.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      None.
 * @post
 *      None
 * @note
 *      Note that this is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaCyStopInstance()
 *
 *****************************************************************************/
CpaStatus
cpaCyStartInstance(CpaInstanceHandle instanceHandle);

/**
 *****************************************************************************
 * @ingroup cpaCyInstMaint
 *      Cryptographic Component Stop function.
 *
 * @description
 *      This function will stop the Cryptographic component and free
 *      all system resources associated with it. The client MUST ensure that
 *      all outstanding operations have completed before calling this function.
 *      The recommended approach to ensure this is to deregister all session or
 *      callback handles before calling this function. If outstanding
 *      operations still exist when this function is invoked, the callback
 *      function for each of those operations will NOT be invoked and the
 *      shutdown will continue.  If the component is to be restarted, then a
 *      call to cpaCyStartInstance is required.
 *
 * @context
 *      This function may sleep, and so MUST NOT be called in interrupt
 *      context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 * @param[in] instanceHandle        Handle to an instance of this API to be
 *                                  shutdown.
 *
 * @retval CPA_STATUS_SUCCESS       Function executed successfully.
 * @retval CPA_STATUS_FAIL          Function failed. Suggested course of action
 *                                  is to ensure requests are not still being
 *                                  submitted and that all sessions are
 *                                  deregistered. If this does not help, then
 *                                  forcefully remove the component from the
 *                                  system.
 * @retval CPA_STATUS_UNSUPPORTED   Function is not supported.
 *
 * @pre
 *      The component has been initialized via cpaCyStartInstance.
 * @post
 *      None
 * @note
 *      Note that this is a synchronous function and has no completion callback
 *      associated with it.
 *
 * @see
 *      cpaCyStartInstance()
 *
 *****************************************************************************/
CpaStatus
cpaCyStopInstance(CpaInstanceHandle instanceHandle);

/**
 *****************************************************************************
 * @ingroup cpaCyInstMaint
 *      Cryptographic Capabilities Info
 *
 * @description
 *      This structure contains the capabilities that vary across API
 *      implementations. This structure is used in conjunction with
 *      @ref cpaCyQueryCapabilities() to determine the capabilities supported
 *      by a particular API implementation.
 *
 *      The client MUST allocate memory for this structure and any members
 *      that require memory.  When the structure is passed into the function
 *      ownership of the memory passes to the function. Ownership of the
 *      memory returns to the client when the function returns.
 *****************************************************************************/
typedef struct _CpaCyCapabilitiesInfo
{
  CpaBoolean symSupported;
  /**< CPA_TRUE if instance supports the symmetric cryptography API.
   * See @ref cpaCySym. */
  CpaBoolean symDpSupported;
  /**< CPA_TRUE if instance supports the symmetric cryptography
   * data plane API.
   * See @ref cpaCySymDp. */
  CpaBoolean dhSupported;
  /**< CPA_TRUE if instance supports the Diffie Hellman API.
   * See @ref cpaCyDh. */
  CpaBoolean dsaSupported;
  /**< CPA_TRUE if instance supports the DSA API.
   * See @ref cpaCyDsa. */
  CpaBoolean rsaSupported;
  /**< CPA_TRUE if instance supports the RSA API.
   * See @ref cpaCyRsa. */
  CpaBoolean ecSupported;
  /**< CPA_TRUE if instance supports the Elliptic Curve API.
   * See @ref cpaCyEc. */
  CpaBoolean ecdhSupported;
  /**< CPA_TRUE if instance supports the Elliptic Curve Diffie Hellman API.
   * See @ref cpaCyEcdh. */
  CpaBoolean ecdsaSupported;
  /**< CPA_TRUE if instance supports the Elliptic Curve DSA API.
   * See @ref cpaCyEcdsa. */
  CpaBoolean keySupported;
  /**< CPA_TRUE if instance supports the Key Generation API.
   * See @ref cpaCyKeyGen. */
  CpaBoolean lnSupported;
  /**< CPA_TRUE if instance supports the Large Number API.
   * See @ref cpaCyLn. */
  CpaBoolean primeSupported;
  /**< CPA_TRUE if instance supports the prime number testing API.
   * See @ref cpaCyPrime. */
  CpaBoolean drbgSupported;
  /**< CPA_TRUE if instance supports the DRBG API.
   * See @ref cpaCyDrbg. */
  CpaBoolean nrbgSupported;
  /**< CPA_TRUE if instance supports the NRBG API.
   * See @ref cpaCyNrbg. */
  CpaBoolean randSupported;
  /**< CPA_TRUE if instance supports the random bit/number generation API.
   * See @ref cpaCyRand. */
  CpaBoolean kptSupported;
  /**< CPA_TRUE if instance supports the Intel(R) KPT Cryptographic API.
   * See @ref cpaCyKpt. */
   CpaBoolean hkdfSupported;
  /**< CPA_TRUE if instance supports the HKDF components of the KeyGen API.
   * See @ref cpaCyKeyGen. */
   CpaBoolean extAlgchainSupported;
  /**< CPA_TRUE if instance supports algorithm chaining for certain
   * wireless algorithms. Please refer to implementation for details.
   * See @ref cpaCySym. */
   CpaBoolean ecEdMontSupported;
  /**< CPA_TRUE if instance supports the Edwards and Montgomery elliptic
   * curves of the EC API.
   * See @ref cpaCyEc */
  CpaBoolean ecSm2Supported;
  /**< CPA_TRUE if instance supports the EcSM2 API.
   * See @ref cpaCyEcsm2. */
} CpaCyCapabilitiesInfo;

/**
 *****************************************************************************
 * @ingroup cpaCyInstMaint
 *      Returns capabilities of a Cryptographic API instance
 *
 * @description
 *      This function is used to query the instance capabilities.
 *
 * @context
 *      The function shall not be called in an interrupt context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle        Handle to an instance of this API.
 * @param[out] pCapInfo              Pointer to capabilities info structure.
 *                                   All fields in the structure
 *                                   are populated by the API instance.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      The instance has been initialized via the @ref cpaCyStartInstance
 *      function.
 * @post
 *      None
 *****************************************************************************/
CpaStatus
cpaCyQueryCapabilities(const CpaInstanceHandle instanceHandle,
    CpaCyCapabilitiesInfo * pCapInfo);

/**
 *****************************************************************************
 * @ingroup cpaCyInstMaint
 *      Sets the address translation function
 *
 * @description
 *      This function is used to set the virtual to physical address
 *      translation routine for the instance. The specified routine
 *      is used by the instance to perform any required translation of
 *      a virtual address to a physical address. If the application
 *      does not invoke this function, then the instance will use its
 *      default method, such as virt2phys, for address translation.

 * @context
 *      The function shall not be called in an interrupt context.
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      This function is synchronous and blocking.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle         Handle to an instance of this API.
 * @param[in] virtual2Physical       Routine that performs virtual to
 *                                   physical address translation.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @retval CPA_STATUS_INVALID_PARAM  Invalid parameter passed in.
 * @retval CPA_STATUS_UNSUPPORTED    Function is not supported.
 *
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus
cpaCySetAddressTranslation(const CpaInstanceHandle instanceHandle,
                           CpaVirtualToPhysical virtual2Physical);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /*CPA_CY_IM_H_*/
