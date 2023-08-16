/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 ***************************************************************************
 * @file sal_hw_gen.h
 *
 * @ingroup SalHwGen
 *
 * @description
 *     Functions which return a value corresponding to qat device generation
 *
 ***************************************************************************/

#ifndef SAL_HW_GEN_H
#define SAL_HW_GEN_H

#include "cpa.h"
#include "sal_types_compression.h"
#include "lac_sal_types_crypto.h"

/**
 ***************************************************************************
 * @ingroup SalHwGen
 *
 * @description This function returns whether qat device is gen 4 or not
 *
 * @param[in] pService     pointer to compression service
 *
 ***************************************************************************/

static inline CpaBoolean
isDcGen4x(const sal_compression_service_t *pService)
{
	return (pService->generic_service_info.gen == GEN4);
}

/**
 ***************************************************************************
 * @ingroup SalHwGen
 *
 * @description This function returns whether qat device is gen 2/3 or not
 *
 * @param[in] pService     pointer to compression service
 *
 ***************************************************************************/

static inline CpaBoolean
isDcGen2x(const sal_compression_service_t *pService)
{
	return ((pService->generic_service_info.gen == GEN2) ||
		(pService->generic_service_info.gen == GEN3));
}

/**
 ***************************************************************************
 * @ingroup SalHwGen
 *
 * @description This function returns whether qat device is gen 4 or not
 *
 * @param[in] pService     pointer to crypto service
 *
 ***************************************************************************/

static inline CpaBoolean
isCyGen4x(const sal_crypto_service_t *pService)
{
	return (pService->generic_service_info.gen == GEN4);
}

/**
 ***************************************************************************
 * @ingroup SalHwGen
 *
 * @description This function returns whether qat device is gen 2/3 or not
 *
 * @param[in] pService     pointer to crypto service
 *
 ***************************************************************************/

static inline CpaBoolean
isCyGen2x(const sal_crypto_service_t *pService)
{
	return ((pService->generic_service_info.gen == GEN2) ||
		(pService->generic_service_info.gen == GEN3));
}

#endif /* SAL_HW_GEN_H */
