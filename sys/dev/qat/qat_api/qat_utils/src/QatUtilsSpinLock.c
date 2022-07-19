/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_utils.h"
#include <sys/sx.h>

CpaStatus
qatUtilsLockInit(struct mtx *pLock)
{
	if (!pLock)
		return CPA_STATUS_FAIL;
	memset(pLock, 0, sizeof(*pLock));
	mtx_init(pLock, "qat spin", NULL, MTX_DEF | MTX_DUPOK);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsLock(struct mtx *pLock)
{
	if (!pLock)
		return CPA_STATUS_FAIL;
	mtx_lock(pLock);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsUnlock(struct mtx *pLock)
{
	if (!pLock)
		return CPA_STATUS_FAIL;
	mtx_unlock(pLock);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsLockDestroy(struct mtx *pLock)
{
	if (!pLock)
		return CPA_STATUS_FAIL;
	mtx_destroy(pLock);
	return CPA_STATUS_SUCCESS;
}
