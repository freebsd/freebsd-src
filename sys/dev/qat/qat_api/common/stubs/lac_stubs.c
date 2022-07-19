/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 *
 * @file lac_stubs.c
 *
 * @defgroup kernel stubs
 *
 * All PKE and KPT API won't be supported in kernel API
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

/* API Includes */
#include "cpa.h"
#include "cpa_cy_dh.h"
#include "cpa_cy_dsa.h"
#include "cpa_cy_ecdh.h"
#include "cpa_cy_ecdsa.h"
#include "cpa_cy_ec.h"
#include "cpa_cy_prime.h"
#include "cpa_cy_rsa.h"
#include "cpa_cy_ln.h"
#include "cpa_dc.h"
#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_sal_poll.h"
#include "cpa_cy_sym.h"
#include "cpa_cy_sym_dp.h"
#include "cpa_cy_key.h"
#include "cpa_cy_common.h"
#include "cpa_cy_im.h"
#include "icp_sal_user.h"

/* Diffie Hellman */
CpaStatus
cpaCyDhKeyGenPhase1(const CpaInstanceHandle instanceHandle,
		    const CpaCyGenFlatBufCbFunc pDhPhase1Cb,
		    void *pCallbackTag,
		    const CpaCyDhPhase1KeyGenOpData *pPhase1KeyGenData,
		    CpaFlatBuffer *pLocalOctetStringPV)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDhKeyGenPhase2Secret(
    const CpaInstanceHandle instanceHandle,
    const CpaCyGenFlatBufCbFunc pDhPhase2Cb,
    void *pCallbackTag,
    const CpaCyDhPhase2SecretKeyGenOpData *pPhase2SecretKeyGenData,
    CpaFlatBuffer *pOctetStringSecretKey)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDhQueryStats64(const CpaInstanceHandle instanceHandle,
		    CpaCyDhStats64 *pDhStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDhQueryStats(const CpaInstanceHandle instanceHandle,
		  CpaCyDhStats *pDhStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* DSA */
CpaStatus
cpaCyDsaGenPParam(const CpaInstanceHandle instanceHandle,
		  const CpaCyDsaGenCbFunc pCb,
		  void *pCallbackTag,
		  const CpaCyDsaPParamGenOpData *pOpData,
		  CpaBoolean *pProtocolStatus,
		  CpaFlatBuffer *pP)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaGenGParam(const CpaInstanceHandle instanceHandle,
		  const CpaCyDsaGenCbFunc pCb,
		  void *pCallbackTag,
		  const CpaCyDsaGParamGenOpData *pOpData,
		  CpaBoolean *pProtocolStatus,
		  CpaFlatBuffer *pG)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaGenYParam(const CpaInstanceHandle instanceHandle,
		  const CpaCyDsaGenCbFunc pCb,
		  void *pCallbackTag,
		  const CpaCyDsaYParamGenOpData *pOpData,
		  CpaBoolean *pProtocolStatus,
		  CpaFlatBuffer *pY)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaSignR(const CpaInstanceHandle instanceHandle,
	      const CpaCyDsaGenCbFunc pCb,
	      void *pCallbackTag,
	      const CpaCyDsaRSignOpData *pOpData,
	      CpaBoolean *pProtocolStatus,
	      CpaFlatBuffer *pR)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaSignS(const CpaInstanceHandle instanceHandle,
	      const CpaCyDsaGenCbFunc pCb,
	      void *pCallbackTag,
	      const CpaCyDsaSSignOpData *pOpData,
	      CpaBoolean *pProtocolStatus,
	      CpaFlatBuffer *pS)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaSignRS(const CpaInstanceHandle instanceHandle,
	       const CpaCyDsaRSSignCbFunc pCb,
	       void *pCallbackTag,
	       const CpaCyDsaRSSignOpData *pOpData,
	       CpaBoolean *pProtocolStatus,
	       CpaFlatBuffer *pR,
	       CpaFlatBuffer *pS)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaVerify(const CpaInstanceHandle instanceHandle,
	       const CpaCyDsaVerifyCbFunc pCb,
	       void *pCallbackTag,
	       const CpaCyDsaVerifyOpData *pOpData,
	       CpaBoolean *pVerifyStatus)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaQueryStats(const CpaInstanceHandle instanceHandle,
		   CpaCyDsaStats *pDsaStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyDsaQueryStats64(const CpaInstanceHandle instanceHandle,
		     CpaCyDsaStats64 *pDsaStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* ECDH */
CpaStatus
cpaCyEcdhPointMultiply(const CpaInstanceHandle instanceHandle,
		       const CpaCyEcdhPointMultiplyCbFunc pCb,
		       void *pCallbackTag,
		       const CpaCyEcdhPointMultiplyOpData *pOpData,
		       CpaBoolean *pMultiplyStatus,
		       CpaFlatBuffer *pXk,
		       CpaFlatBuffer *pYk)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcdhQueryStats64(const CpaInstanceHandle instanceHandle,
		      CpaCyEcdhStats64 *pEcdhStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* ECDSA */
CpaStatus
cpaCyEcdsaSignR(const CpaInstanceHandle instanceHandle,
		const CpaCyEcdsaGenSignCbFunc pCb,
		void *pCallbackTag,
		const CpaCyEcdsaSignROpData *pOpData,
		CpaBoolean *pSignStatus,
		CpaFlatBuffer *pR)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcdsaSignS(const CpaInstanceHandle instanceHandle,
		const CpaCyEcdsaGenSignCbFunc pCb,
		void *pCallbackTag,
		const CpaCyEcdsaSignSOpData *pOpData,
		CpaBoolean *pSignStatus,
		CpaFlatBuffer *pS)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcdsaSignRS(const CpaInstanceHandle instanceHandle,
		 const CpaCyEcdsaSignRSCbFunc pCb,
		 void *pCallbackTag,
		 const CpaCyEcdsaSignRSOpData *pOpData,
		 CpaBoolean *pSignStatus,
		 CpaFlatBuffer *pR,
		 CpaFlatBuffer *pS)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcdsaVerify(const CpaInstanceHandle instanceHandle,
		 const CpaCyEcdsaVerifyCbFunc pCb,
		 void *pCallbackTag,
		 const CpaCyEcdsaVerifyOpData *pOpData,
		 CpaBoolean *pVerifyStatus)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcdsaQueryStats64(const CpaInstanceHandle instanceHandle,
		       CpaCyEcdsaStats64 *pEcdsaStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* EC */
CpaStatus
cpaCyEcPointMultiply(const CpaInstanceHandle instanceHandle,
		     const CpaCyEcPointMultiplyCbFunc pCb,
		     void *pCallbackTag,
		     const CpaCyEcPointMultiplyOpData *pOpData,
		     CpaBoolean *pMultiplyStatus,
		     CpaFlatBuffer *pXk,
		     CpaFlatBuffer *pYk)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcPointVerify(const CpaInstanceHandle instanceHandle,
		   const CpaCyEcPointVerifyCbFunc pCb,
		   void *pCallbackTag,
		   const CpaCyEcPointVerifyOpData *pOpData,
		   CpaBoolean *pVerifyStatus)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcQueryStats64(const CpaInstanceHandle instanceHandle,
		    CpaCyEcStats64 *pEcStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyEcMontEdwdsPointMultiply(
    const CpaInstanceHandle instanceHandle,
    const CpaCyEcPointMultiplyCbFunc pCb,
    void *pCallbackTag,
    const CpaCyEcMontEdwdsPointMultiplyOpData *pOpData,
    CpaBoolean *pMultiplyStatus,
    CpaFlatBuffer *pXk,
    CpaFlatBuffer *pYk)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* Prime */
CpaStatus
cpaCyPrimeTest(const CpaInstanceHandle instanceHandle,
	       const CpaCyPrimeTestCbFunc pCb,
	       void *pCallbackTag,
	       const CpaCyPrimeTestOpData *pOpData,
	       CpaBoolean *pTestPassed)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyPrimeQueryStats64(const CpaInstanceHandle instanceHandle,
		       CpaCyPrimeStats64 *pPrimeStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyPrimeQueryStats(const CpaInstanceHandle instanceHandle,
		     CpaCyPrimeStats *pPrimeStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* RSA */
CpaStatus
cpaCyRsaGenKey(const CpaInstanceHandle instanceHandle,
	       const CpaCyRsaKeyGenCbFunc pRsaKeyGenCb,
	       void *pCallbackTag,
	       const CpaCyRsaKeyGenOpData *pKeyGenOpData,
	       CpaCyRsaPrivateKey *pPrivateKey,
	       CpaCyRsaPublicKey *pPublicKey)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyRsaEncrypt(const CpaInstanceHandle instanceHandle,
		const CpaCyGenFlatBufCbFunc pRsaEncryptCb,
		void *pCallbackTag,
		const CpaCyRsaEncryptOpData *pEncryptOpData,
		CpaFlatBuffer *pOutputData)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyRsaDecrypt(const CpaInstanceHandle instanceHandle,
		const CpaCyGenFlatBufCbFunc pRsaDecryptCb,
		void *pCallbackTag,
		const CpaCyRsaDecryptOpData *pDecryptOpData,
		CpaFlatBuffer *pOutputData)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyRsaQueryStats64(const CpaInstanceHandle instanceHandle,
		     CpaCyRsaStats64 *pRsaStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyRsaQueryStats(const CpaInstanceHandle instanceHandle,
		   CpaCyRsaStats *pRsaStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* Large Number */
CpaStatus
cpaCyLnModExp(const CpaInstanceHandle instanceHandle,
	      const CpaCyGenFlatBufCbFunc pLnModExpCb,
	      void *pCallbackTag,
	      const CpaCyLnModExpOpData *pLnModExpOpData,
	      CpaFlatBuffer *pResult)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyLnModInv(const CpaInstanceHandle instanceHandle,
	      const CpaCyGenFlatBufCbFunc pLnModInvCb,
	      void *pCallbackTag,
	      const CpaCyLnModInvOpData *pLnModInvOpData,
	      CpaFlatBuffer *pResult)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
cpaCyLnStatsQuery64(const CpaInstanceHandle instanceHandle,
		    CpaCyLnStats64 *pLnStats)
{
	return CPA_STATUS_UNSUPPORTED;
}

/* Dynamic Instance */
CpaStatus
icp_adf_putDynInstance(icp_accel_dev_t *accel_dev,
		       adf_service_type_t stype,
		       Cpa32U instance_id)
{
	return CPA_STATUS_FAIL;
}

CpaStatus
icp_sal_CyPollAsymRing(CpaInstanceHandle instanceHandle_in,
		       Cpa32U response_quota)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
icp_sal_AsymGetInflightRequests(CpaInstanceHandle instanceHandle,
				Cpa32U *maxInflightRequests,
				Cpa32U *numInflightRequests)
{
	return CPA_STATUS_UNSUPPORTED;
}

CpaStatus
icp_sal_AsymPerformOpNow(CpaInstanceHandle instanceHandle)
{
	return CPA_STATUS_UNSUPPORTED;
}
