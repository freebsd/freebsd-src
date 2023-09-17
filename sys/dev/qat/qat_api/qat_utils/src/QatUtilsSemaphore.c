/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_utils.h"

#include <sys/types.h>
#include <sys/lock.h>
#include <sys/sema.h>
#include <sys/mutex.h>

/* Define a 64 bit number */
#define QAT_UTILS_MAX_LONG (0x7FFFFFFF)

/* Max timeout in MS, used to guard against possible overflow */
#define QAT_UTILS_MAX_TIMEOUT_MS (QAT_UTILS_MAX_LONG / hz)

CpaStatus
qatUtilsSemaphoreInit(struct sema **pSid, uint32_t start_value)
{
	if (!pSid)
		return CPA_STATUS_FAIL;

	*pSid = malloc(sizeof(struct sema), M_QAT, M_WAITOK);

	sema_init(*pSid, start_value, "qat sema");

	return CPA_STATUS_SUCCESS;
}

/**
 * DESCRIPTION: If the semaphore is unset, the calling thread is blocked.
 *         If the semaphore is set, it is taken and control is returned
 *         to the caller. If the time indicated in 'timeout' is reached,
 *         the thread will unblock and return an error indication. If the
 *         timeout is set to 'QAT_UTILS_WAIT_NONE', the thread will never block;
 *         if it is set to 'QAT_UTILS_WAIT_FOREVER', the thread will block until
 *         the semaphore is available.
 *
 *
 */

CpaStatus
qatUtilsSemaphoreWait(struct sema **pSid, int32_t timeout)
{

	CpaStatus Status = CPA_STATUS_SUCCESS;
	unsigned long timeoutTime;

	if (!pSid)
		return CPA_STATUS_FAIL;
	/*
	 * Guard against illegal timeout values
	 */
	if ((timeout < 0) && (timeout != QAT_UTILS_WAIT_FOREVER)) {
		QAT_UTILS_LOG(
		    "QatUtilsSemaphoreWait(): illegal timeout value\n");
		return CPA_STATUS_FAIL;
	} else if (timeout > QAT_UTILS_MAX_TIMEOUT_MS) {
		QAT_UTILS_LOG(
		    "QatUtilsSemaphoreWait(): use a smaller timeout value to avoid overflow.\n");
		return CPA_STATUS_FAIL;
	}

	if (timeout == QAT_UTILS_WAIT_FOREVER) {
		sema_wait(*pSid);
	} else if (timeout == QAT_UTILS_WAIT_NONE) {
		if (sema_trywait(*pSid)) {
			Status = CPA_STATUS_FAIL;
		}
	} else {
		/* Convert timeout in milliseconds to HZ */
		timeoutTime = timeout * hz / 1000;
		if (sema_timedwait(*pSid, timeoutTime)) {
			Status = CPA_STATUS_FAIL;
		}
	} /* End of if */

	return Status;
}

CpaStatus
qatUtilsSemaphoreTryWait(struct sema **pSid)
{
	if (!pSid)
		return CPA_STATUS_FAIL;
	if (sema_trywait(*pSid)) {
		return CPA_STATUS_FAIL;
	}
	return CPA_STATUS_SUCCESS;
}

/**
 *
 * DESCRIPTION: This function causes the next available thread in the pend queue
 *              to be unblocked. If no thread is pending on this semaphore, the
 *              semaphore becomes 'full'.
 */
CpaStatus
qatUtilsSemaphorePost(struct sema **pSid)
{
	if (!pSid)
		return CPA_STATUS_FAIL;
	sema_post(*pSid);
	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsSemaphoreDestroy(struct sema **pSid)
{
	if (!pSid)
		return CPA_STATUS_FAIL;

	sema_destroy(*pSid);
	free(*pSid, M_QAT);

	return CPA_STATUS_SUCCESS;
}

/****************************
 *    Mutex
 ****************************/

CpaStatus
qatUtilsMutexInit(struct mtx **pMutex)
{
	if (!pMutex)
		return CPA_STATUS_FAIL;
	*pMutex = malloc(sizeof(struct mtx), M_QAT, M_WAITOK);

	memset(*pMutex, 0, sizeof(struct mtx));

	mtx_init(*pMutex, "qat mtx", NULL, MTX_DEF);
	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsMutexLock(struct mtx **pMutex, int32_t timeout)
{
	if (!pMutex)
		return CPA_STATUS_FAIL;
	if (timeout != QAT_UTILS_WAIT_FOREVER) {
		QAT_UTILS_LOG("QatUtilsMutexLock(): Illegal timeout value\n");
		return CPA_STATUS_FAIL;
	}

	mtx_lock(*pMutex);
	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsMutexUnlock(struct mtx **pMutex)
{
	if (!pMutex || !(*pMutex))
		return CPA_STATUS_FAIL;
	mtx_unlock(*pMutex);
	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsMutexDestroy(struct mtx **pMutex)
{
	if (!pMutex || !(*pMutex))
		return CPA_STATUS_FAIL;
	mtx_destroy(*pMutex);
	free(*pMutex, M_QAT);
	*pMutex = NULL;

	return CPA_STATUS_SUCCESS;
}
