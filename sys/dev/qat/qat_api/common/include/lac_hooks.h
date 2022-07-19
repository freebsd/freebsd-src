/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *******************************************************************************
 * @file lac_hooks.h
 *
 * @defgroup LacHooks   Hooks
 *
 * @ingroup LacCommon
 *
 * Component Init/Shutdown functions. These are:
 *  - an init function which is called during the intialisation sequence,
 *  - a shutdown function which is called by the overall shutdown function,
 *
 ******************************************************************************/

#ifndef LAC_HOOKS_H
#define LAC_HOOKS_H

/*
********************************************************************************
* Include public/global header files
********************************************************************************
*/

#include "cpa.h"

/*
********************************************************************************
* Include private header files
********************************************************************************
*/

/******************************************************************************/

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function initialises the Large Number (ModExp and ModInv) module
 *
 * @description
 *      This function clears the Large Number statistics
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
CpaStatus LacLn_Init(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function frees statistics array for Large Number module
 *
 * @description
 *      This function frees statistics array for Large Number module
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
void LacLn_StatsFree(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function initialises the Prime module
 *
 * @description
 *      This function clears the Prime statistics
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
CpaStatus LacPrime_Init(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function frees the Prime module statistics array
 *
 * @description
 *      This function frees the Prime module statistics array
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
void LacPrime_StatsFree(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function initialises the DSA module
 *
 * @param[in] instanceHandle
 *
 * @description
 *      This function clears the DSA statistics
 *
 ******************************************************************************/
CpaStatus LacDsa_Init(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function frees the DSA module statistics array
 *
 * @param[in] instanceHandle
 *
 * @description
 *      This function frees the DSA statistics array
 *
 ******************************************************************************/
void LacDsa_StatsFree(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function initialises the Diffie Hellmann module
 *
 * @description
 *      This function initialises the Diffie Hellman statistics
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
CpaStatus LacDh_Init(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function frees the Diffie Hellmann module statistics
 *
 * @description
 *      This function frees the Diffie Hellmann module statistics
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
void LacDh_StatsFree(CpaInstanceHandle instanceHandle);

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      This function registers the callback handlers to SSL/TLS and MGF,
 *      allocates resources that are needed for the component and clears
 *      the stats.
 *
 * @param[in] instanceHandle
 *
 * @retval CPA_STATUS_SUCCESS   Status Success
 * @retval CPA_STATUS_FAIL      General failure
 * @retval CPA_STATUS_RESOURCE  Resource allocation failure
 *
 *****************************************************************************/
CpaStatus LacSymKey_Init(CpaInstanceHandle instanceHandle);

/**
 ******************************************************************************
 * @ingroup LacSymKey
 *      This function frees up resources obtained by the key gen component
 *      and clears the stats
 *
 * @param[in] instanceHandle
 *
 * @retval CPA_STATUS_SUCCESS   Status Success
 *
 *****************************************************************************/
CpaStatus LacSymKey_Shutdown(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function initialises the RSA module
 *
 * @description
 *      This function clears the RSA statistics
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
CpaStatus LacRsa_Init(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function frees the RSA module statistics
 *
 * @description
 *      This function frees the RSA module statistics
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
void LacRsa_StatsFree(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function initialises the EC module
 *
 * @description
 *      This function clears the EC statistics
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
CpaStatus LacEc_Init(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup LacHooks
 *      This function frees the EC module stats array
 *
 * @description
 *      This function frees the EC module stats array
 *
 * @param[in] instanceHandle
 *
 ******************************************************************************/
void LacEc_StatsFree(CpaInstanceHandle instanceHandle);

/**
*******************************************************************************
 * @ingroup LacSymNrbg
 *      Initialise the NRBG module
 *
 * @description
 *     This function registers NRBG callback handlers.
 *
 *
 *****************************************************************************/
void LacSymNrbg_Init(void);

#endif /* LAC_HOOKS_H */
