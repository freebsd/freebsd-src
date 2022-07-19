/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file sal_create_services.c
 *
 * @defgroup SalCtrl Service Access Layer Controller
 *
 * @ingroup SalCtrl
 *
 * @description
 *      This file contains the main function to create a specific service.
 *
 *****************************************************************************/

#include "cpa.h"
#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "qat_utils.h"
#include "lac_list.h"
#include "icp_adf_transport.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"

#include "icp_qat_fw_la.h"
#include "lac_sym_qat.h"
#include "sal_types_compression.h"
#include "lac_sal_types_crypto.h"

#include "icp_adf_init.h"

#include "lac_sal.h"
#include "lac_sal_ctrl.h"

CpaStatus
SalCtrl_ServiceCreate(sal_service_type_t serviceType,
		      Cpa32U instance,
		      sal_service_t **ppInst)
{
	sal_crypto_service_t *pCrypto_service = NULL;
	sal_compression_service_t *pCompression_service = NULL;

	switch ((sal_service_type_t)serviceType) {
	case SAL_SERVICE_TYPE_CRYPTO_ASYM:
	case SAL_SERVICE_TYPE_CRYPTO_SYM:
	case SAL_SERVICE_TYPE_CRYPTO: {
		pCrypto_service =
		    malloc(sizeof(sal_crypto_service_t), M_QAT, M_WAITOK);

		/* Zero memory */
		memset(pCrypto_service, 0, sizeof(sal_crypto_service_t));

		pCrypto_service->generic_service_info.type =
		    (sal_service_type_t)serviceType;
		pCrypto_service->generic_service_info.state =
		    SAL_SERVICE_STATE_UNINITIALIZED;
		pCrypto_service->generic_service_info.instance = instance;

		pCrypto_service->generic_service_info.init = SalCtrl_CryptoInit;
		pCrypto_service->generic_service_info.start =
		    SalCtrl_CryptoStart;
		pCrypto_service->generic_service_info.stop = SalCtrl_CryptoStop;
		pCrypto_service->generic_service_info.shutdown =
		    SalCtrl_CryptoShutdown;

		*(ppInst) = &(pCrypto_service->generic_service_info);

		return CPA_STATUS_SUCCESS;
	}
	case SAL_SERVICE_TYPE_COMPRESSION: {
		pCompression_service =
		    malloc(sizeof(sal_compression_service_t), M_QAT, M_WAITOK);

		/* Zero memory */
		memset(pCompression_service,
		       0,
		       sizeof(sal_compression_service_t));

		pCompression_service->generic_service_info.type =
		    (sal_service_type_t)serviceType;
		pCompression_service->generic_service_info.state =
		    SAL_SERVICE_STATE_UNINITIALIZED;
		pCompression_service->generic_service_info.instance = instance;

		pCompression_service->generic_service_info.init =
		    SalCtrl_CompressionInit;
		pCompression_service->generic_service_info.start =
		    SalCtrl_CompressionStart;
		pCompression_service->generic_service_info.stop =
		    SalCtrl_CompressionStop;
		pCompression_service->generic_service_info.shutdown =
		    SalCtrl_CompressionShutdown;

		*(ppInst) = &(pCompression_service->generic_service_info);
		return CPA_STATUS_SUCCESS;
	}

	default: {
		QAT_UTILS_LOG("Not a valid service type\n");
		(*ppInst) = NULL;
		return CPA_STATUS_FAIL;
	}
	}
}
