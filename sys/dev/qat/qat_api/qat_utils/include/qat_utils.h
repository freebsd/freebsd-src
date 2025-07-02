/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef QAT_UTILS_H
#define QAT_UTILS_H

#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/endian.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/limits.h>
#include <sys/unistd.h>
#include <sys/libkern.h>
#ifdef __x86_64__
#include <asm/atomic64.h>
#else
#include <asm/atomic.h>
#endif
#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2/sha256.h>
#include <crypto/sha2/sha224.h>
#include <crypto/sha2/sha384.h>
#include <crypto/sha2/sha512.h>
#include <crypto/rijndael/rijndael-api-fst.h>

#include <opencrypto/cryptodev.h>

#include "cpa.h"

#define QAT_UTILS_LOG(...) printf("QAT: "__VA_ARGS__)

#define QAT_UTILS_WAIT_FOREVER (-1)
#define QAT_UTILS_WAIT_NONE 0

#define QAT_UTILS_HOST_TO_NW_16(uData) QAT_UTILS_OS_HOST_TO_NW_16(uData)
#define QAT_UTILS_HOST_TO_NW_32(uData) QAT_UTILS_OS_HOST_TO_NW_32(uData)
#define QAT_UTILS_HOST_TO_NW_64(uData) QAT_UTILS_OS_HOST_TO_NW_64(uData)

#define QAT_UTILS_NW_TO_HOST_16(uData) QAT_UTILS_OS_NW_TO_HOST_16(uData)
#define QAT_UTILS_NW_TO_HOST_32(uData) QAT_UTILS_OS_NW_TO_HOST_32(uData)
#define QAT_UTILS_NW_TO_HOST_64(uData) QAT_UTILS_OS_NW_TO_HOST_64(uData)

#define QAT_UTILS_UDIV64_32(dividend, divisor)                                 \
	QAT_UTILS_OS_UDIV64_32(dividend, divisor)

#define QAT_UTILS_UMOD64_32(dividend, divisor)                                 \
	QAT_UTILS_OS_UMOD64_32(dividend, divisor)

#define ICP_CHECK_FOR_NULL_PARAM(param)                                        \
	do {                                                                   \
		if (NULL == param) {                                           \
			QAT_UTILS_LOG("%s(): invalid param: %s\n",             \
				      __FUNCTION__,                            \
				      #param);                                 \
			return CPA_STATUS_INVALID_PARAM;                       \
		}                                                              \
	} while (0)

#define ICP_CHECK_FOR_NULL_PARAM_VOID(param)                                   \
	do {                                                                   \
		if (NULL == param) {                                           \
			QAT_UTILS_LOG("%s(): invalid param: %s\n",             \
				      __FUNCTION__,                            \
				      #param);                                 \
			return;                                                \
		}                                                              \
	} while (0)

/*Macro for adding an element to the tail of a doubly linked list*/
/*The currentptr tracks the tail, and the headptr tracks the head.*/
#define ICP_ADD_ELEMENT_TO_END_OF_LIST(elementtoadd, currentptr, headptr)      \
	do {                                                                   \
		if (NULL == currentptr) {                                      \
			currentptr = elementtoadd;                             \
			elementtoadd->pNext = NULL;                            \
			elementtoadd->pPrev = NULL;                            \
			headptr = currentptr;                                  \
		} else {                                                       \
			elementtoadd->pPrev = currentptr;                      \
			currentptr->pNext = elementtoadd;                      \
			elementtoadd->pNext = NULL;                            \
			currentptr = elementtoadd;                             \
		}                                                              \
	} while (0)

/*currentptr is not used in this case since we don't track the tail. */
#define ICP_ADD_ELEMENT_TO_HEAD_OF_LIST(elementtoadd, currentptr, headptr)     \
	do {                                                                   \
		if (NULL == headptr) {                                         \
			elementtoadd->pNext = NULL;                            \
			elementtoadd->pPrev = NULL;                            \
			headptr = elementtoadd;                                \
		} else {                                                       \
			elementtoadd->pPrev = NULL;                            \
			elementtoadd->pNext = headptr;                         \
			headptr->pPrev = elementtoadd;                         \
			headptr = elementtoadd;                                \
		}                                                              \
	} while (0)

#define ICP_REMOVE_ELEMENT_FROM_LIST(elementtoremove, currentptr, headptr)     \
	do {                                                                   \
		/*If the previous pointer is not NULL*/                        \
		if (NULL != elementtoremove->pPrev) {                          \
			elementtoremove->pPrev->pNext =                        \
			    elementtoremove->pNext;                            \
			if (elementtoremove->pNext) {                          \
				elementtoremove->pNext->pPrev =                \
				    elementtoremove->pPrev;                    \
			} else {                                               \
				/* Move the tail pointer backwards */          \
				currentptr = elementtoremove->pPrev;           \
			}                                                      \
		} else if (NULL != elementtoremove->pNext) {                   \
			/*Remove the head pointer.*/                           \
			elementtoremove->pNext->pPrev = NULL;                  \
			/*Hence move the head forward.*/                       \
			headptr = elementtoremove->pNext;                      \
		} else {                                                       \
			/*Remove the final entry in the list. */               \
			currentptr = NULL;                                     \
			headptr = NULL;                                        \
		}                                                              \
	} while (0)

MALLOC_DECLARE(M_QAT);

#ifdef __x86_64__
typedef atomic64_t QatUtilsAtomic;
#else
typedef atomic_t QatUtilsAtomic;
#endif

#define QAT_UTILS_OS_NW_TO_HOST_16(uData) be16toh(uData)
#define QAT_UTILS_OS_NW_TO_HOST_32(uData) be32toh(uData)
#define QAT_UTILS_OS_NW_TO_HOST_64(uData) be64toh(uData)

#define QAT_UTILS_OS_HOST_TO_NW_16(uData) htobe16(uData)
#define QAT_UTILS_OS_HOST_TO_NW_32(uData) htobe32(uData)
#define QAT_UTILS_OS_HOST_TO_NW_64(uData) htobe64(uData)

/**
 * @ingroup QatUtils
 *
 * @brief Atomically read the value of atomic variable
 *
 * @param  pAtomicVar  IN   - atomic variable
 *
 * Atomically reads the value of pAtomicVar to the outValue
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return  pAtomicVar value
 */
int64_t qatUtilsAtomicGet(QatUtilsAtomic *pAtomicVar);

/**
 * @ingroup QatUtils
 *
 * @brief Atomically set the value of atomic variable
 *
 * @param  inValue    IN   -  atomic variable to be set equal to inValue
 *
 * @param  pAtomicVar  OUT  - atomic variable
 *
 * Atomically sets the value of pAtomicVar to the value given
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return none
 */
void qatUtilsAtomicSet(int64_t inValue, QatUtilsAtomic *pAtomicVar);

/**
 * @ingroup QatUtils
 *
 * @brief add the value to atomic variable
 *
 * @param  inValue (in)   -  value to be added to the atomic variable
 *
 * @param  pAtomicVar (in & out)   - atomic variable
 *
 * Atomically adds the value of inValue to the pAtomicVar
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return pAtomicVar value after the addition
 */
int64_t qatUtilsAtomicAdd(int64_t inValue, QatUtilsAtomic *pAtomicVar);

/**
 * @ingroup QatUtils
 *
 * @brief subtract the value from atomic variable
 *
 * @param  inValue   IN     -  atomic variable value to be subtracted by value
 *
 * @param  pAtomicVar IN/OUT - atomic variable
 *
 * Atomically subtracts the value of pAtomicVar by inValue
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return pAtomicVar value after the subtraction
 */
int64_t qatUtilsAtomicSub(int64_t inValue, QatUtilsAtomic *pAtomicVar);

/**
 * @ingroup QatUtils
 *
 * @brief increment value of atomic variable by 1
 *
 * @param  pAtomicVar IN/OUT   - atomic variable
 *
 * Atomically increments the value of pAtomicVar by 1.
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return pAtomicVar value after the increment
 */
int64_t qatUtilsAtomicInc(QatUtilsAtomic *pAtomicVar);

/**
 * @ingroup QatUtils
 *
 * @brief decrement value of atomic variable by 1
 *
 * @param  pAtomicVar IN/OUT  - atomic variable
 *
 * Atomically decrements the value of pAtomicVar by 1.
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return pAtomic value after the decrement
 */
int64_t qatUtilsAtomicDec(QatUtilsAtomic *pAtomicVar);

/**
 * @ingroup QatUtils
 *
 * @brief NUMA aware memory allocation; available on Linux OS only.
 *
 * @param size - memory size to allocate, in bytes
 * @param node - node
 * @param alignment - memory boundary alignment (alignment can not be 0)
 *
 * Allocates a memory zone of a given size on the specified node
 * The returned memory is guaraunteed to be physically contiguous if the
 * given size is less than 128KB and belonging to the node specified
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return Pointer to the allocated zone or NULL if the allocation failed
 */
void *qatUtilsMemAllocContiguousNUMA(uint32_t size,
				     uint32_t node,
				     uint32_t alignment);

/**
 * @ingroup QatUtils
 *
 * @brief Frees memory allocated by qatUtilsMemAllocContigousNUMA.
 *
 * @param ptr - pointer to the memory zone
 * @param size - size of the pointer previously allocated
 *
 * Frees a previously allocated memory zone
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - none
 */
void qatUtilsMemFreeNUMA(void *ptr);

/**
 * @ingroup QatUtils
 *
 * @brief virtual to physical address translation
 *
 * @param virtAddr - virtual address
 *
 * Converts a virtual address into its equivalent MMU-mapped physical address
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return Corresponding physical address
 */
#define QAT_UTILS_MMU_VIRT_TO_PHYS(virtAddr)                                   \
	((uint64_t)((virtAddr) ? vtophys(virtAddr) : 0))

/**
 * @ingroup QatUtils
 *
 * @brief Initializes the SpinLock object
 *
 * @param pLock - Spinlock handle
 *
 * Initializes the SpinLock object.
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsLockInit(struct mtx *pLock);

/**
 * @ingroup QatUtils
 *
 * @brief Acquires a spin lock
 *
 * @param pLock - Spinlock handle
 *
 * This routine acquires a spin lock so the
 * caller can synchronize access to shared data in a
 * multiprocessor-safe way by raising IRQL.
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - Returns CPA_STATUS_SUCCESS if the spinlock is acquired. Returns
 * CPA_STATUS_FAIL
 * if
 *           spinlock handle is NULL. If spinlock is already acquired by any
 *           other thread of execution then it tries in busy loop/spins till it
 *           gets spinlock.
 */
CpaStatus qatUtilsLock(struct mtx *pLock);

/**
 * @ingroup QatUtils
 *
 * @brief Releases the spin lock
 *
 * @param pLock - Spinlock handle
 *
 * This routine releases the spin lock which the thread had acquired
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - return CPA_STATUS_SUCCESS if the spinlock is released. Returns
 * CPA_STATUS_FAIL
 * if
 *           spinlockhandle passed is NULL.
 */
CpaStatus qatUtilsUnlock(struct mtx *pLock);

/**
 * @ingroup QatUtils
 *
 * @brief Destroy the spin lock object
 *
 * @param pLock - Spinlock handle
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - returns CPA_STATUS_SUCCESS if plock is destroyed.
 *           returns CPA_STATUS_FAIL if plock is NULL.
 */
CpaStatus qatUtilsLockDestroy(struct mtx *pLock);

/**
 * @ingroup QatUtils
 *
 * @brief Initializes a semaphore
 *
 * @param pSid - semaphore handle
 * @param start_value - initial semaphore value
 *
 * Initializes a semaphore object
 * Note: Semaphore initialization qatUtilsSemaphoreInit API must be called
 * first before using any QAT Utils Semaphore APIs
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsSemaphoreInit(struct sema **pSid, uint32_t start_value);

/**
 * @ingroup QatUtils
 *
 * @brief Destroys a semaphore object
 *
 * @param pSid - semaphore handle
 *
 * Destroys a semaphore object; the caller should ensure that no thread is
 * blocked on this semaphore. If call made when thread blocked on semaphore the
 * behaviour is unpredictable
 *
 * @li Reentrant: yes
] * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsSemaphoreDestroy(struct sema **pSid);

/**
 * @ingroup QatUtils
 *
 * @brief Waits on (decrements) a semaphore
 *
 * @param pSid - semaphore handle
 * @param timeout - timeout, in ms; QAT_UTILS_WAIT_FOREVER (-1) if the thread
 * is to block indefinitely or QAT_UTILS_WAIT_NONE (0) if the thread is to
 * return immediately even if the call fails
 *
 * Decrements a semaphore, blocking if the semaphore is
 * unavailable (value is 0).
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsSemaphoreWait(struct sema **pSid, int32_t timeout);

/**
 * @ingroup QatUtils
 *
 * @brief Non-blocking wait on semaphore
 *
 * @param semaphore - semaphore handle
 *
 * Decrements a semaphore, not blocking the calling thread if the semaphore
 * is unavailable
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsSemaphoreTryWait(struct sema **semaphore);

/**
 * @ingroup QatUtils
 *
 * @brief Posts to (increments) a semaphore
 *
 * @param pSid - semaphore handle
 *
 * Increments a semaphore object
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsSemaphorePost(struct sema **pSid);

/**
 * @ingroup QatUtils
 *
 * @brief initializes a pMutex
 *
 * @param pMutex - pMutex handle
 *
 * Initializes a pMutex object
 * @note Mutex initialization qatUtilsMutexInit API must be called
 * first before using any QAT Utils Mutex APIs
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsMutexInit(struct mtx **pMutex);

/**
 * @ingroup QatUtils
 *
 * @brief locks a pMutex
 *
 * @param pMutex - pMutex handle
 * @param timeout - timeout in ms; QAT_UTILS_WAIT_FOREVER (-1) to wait forever
 *                  or QAT_UTILS_WAIT_NONE to return immediately
 *
 * Locks a pMutex object
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsMutexLock(struct mtx **pMutex, int32_t timeout);

/**
 * @ingroup QatUtils
 *
 * @brief Unlocks a pMutex
 *
 * @param pMutex - pMutex handle
 *
 * Unlocks a pMutex object
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsMutexUnlock(struct mtx **pMutex);

/**
 * @ingroup QatUtils
 *
 * @brief Destroys a pMutex object
 *
 * @param pMutex - pMutex handle
 *
 * Destroys a pMutex object; the caller should ensure that no thread is
 * blocked on this pMutex. If call made when thread blocked on pMutex the
 * behaviour is unpredictable
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsMutexDestroy(struct mtx **pMutex);

/**
 * @ingroup QatUtils
 *
 * @brief Non-blocking attempt to lock a pMutex
 *
 * @param pMutex - pMutex handle
 *
 * Attempts to lock a pMutex object, returning immediately with
 * CPA_STATUS_SUCCESS if
 * the lock was successful or CPA_STATUS_FAIL if the lock failed
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsMutexTryLock(struct mtx **pMutex);

/**
 * @ingroup QatUtils
 *
 * @brief Yielding sleep for a number of milliseconds
 *
 * @param milliseconds - number of milliseconds to sleep
 *
 * The calling thread will sleep for the specified number of milliseconds.
 * This sleep is yielding, hence other tasks will be scheduled by the
 * operating system during the sleep period. Calling this function with an
 * argument of 0 will place the thread at the end of the current scheduling
 * loop.
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 */
CpaStatus qatUtilsSleep(uint32_t milliseconds);

/**
 * @ingroup QatUtils
 *
 * @brief Yields execution of current thread
 *
 * Yields the execution of the current thread
 *
 * @li Reentrant: yes
 * @li IRQ safe:  no
 *
 * @return - none
 */
void qatUtilsYield(void);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate MD5 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least md5 block size long as defined in
 *         rfc1321 (64 bytes)
 *         out - output pointer for state data after single md5 transform
 *         operation.
 *         The buffer needs to be at least md5 state size long as defined in
 *         rfc1321 (16 bytes)
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashMD5(uint8_t *in, uint8_t *out);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate MD5 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least md5 block size long as defined in
 *         rfc1321 (64 bytes)
 *         out - output pointer for state data after single md5 transform
 *         operation.
 *         The buffer needs to be at least md5 state size long as defined in
 *         rfc1321 (16 bytes)
 *         len - Length on the input to be processed.
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashMD5Full(uint8_t *in, uint8_t *out, uint32_t len);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA1 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha1 block size long as defined in
 *         rfc3174 (64 bytes)
 *         out - output pointer for state data after single sha1 transform
 *         operation.
 *         The buffer needs to be at least sha1 state size long as defined in
 *         rfc3174 (20 bytes)
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA1(uint8_t *in, uint8_t *out);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA1 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha1 block size long as defined in
 *         rfc3174 (64 bytes)
 *         out - output pointer for state data after single sha1 transform
 *         operation.
 *         The buffer needs to be at least sha1 state size long as defined in
 *         rfc3174 (20 bytes)
 *         len - Length on the input to be processed.
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA1Full(uint8_t *in, uint8_t *out, uint32_t len);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA224 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha224 block size long as defined in
 *         rfc3874 and rfc4868 (64 bytes)
 *         out - output pointer for state data after single sha224 transform
 *         operation.
 *         The buffer needs to be at least sha224 state size long as defined in
 *         rfc3874 and rfc4868 (32 bytes)
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA224(uint8_t *in, uint8_t *out);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA256 transform operation
 *
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha256 block size long as defined in
 *         rfc4868 (64 bytes)
 *         out - output pointer for state data after single sha256 transform
 *         operation.
 *         The buffer needs to be at least sha256 state size long as defined in
 *         rfc4868 (32 bytes)
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA256(uint8_t *in, uint8_t *out);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA256 transform operation
 *
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha256 block size long as defined in
 *         rfc4868 (64 bytes)
 *         out - output pointer for state data after single sha256 transform
 *         operation.
 *         The buffer needs to be at least sha256 state size long as defined in
 *         rfc4868 (32 bytes)
 *         len - Length on the input to be processed.
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA256Full(uint8_t *in, uint8_t *out, uint32_t len);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA384 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha384 block size long as defined in
 *         rfc4868 (128 bytes)
 *         out - output pointer for state data after single sha384 transform
 *         operation.
 *         The buffer needs to be at least sha384 state size long as defined in
 *         rfc4868 (64 bytes)
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA384(uint8_t *in, uint8_t *out);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA384 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha384 block size long as defined in
 *         rfc4868 (128 bytes)
 *         out - output pointer for state data after single sha384 transform
 *         operation.
 *         The buffer needs to be at least sha384 state size long as defined in
 *         rfc4868 (64 bytes)
 *         len - Length on the input to be processed.
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA384Full(uint8_t *in, uint8_t *out, uint32_t len);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA512 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha512 block size long as defined in
 *         rfc4868 (128 bytes)
 *         out - output pointer for state data after single sha512 transform
 *         operation.
 *         The buffer needs to be at least sha512 state size long as defined in
 *         rfc4868 (64 bytes)
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA512(uint8_t *in, uint8_t *out);

/**
 * @ingroup QatUtils
 *
 * @brief  Calculate SHA512 transform operation
 *
 * @param  in - pointer to data to be processed.
 *         The buffer needs to be at least sha512 block size long as defined in
 *         rfc4868 (128 bytes)
 *         out - output pointer for state data after single sha512 transform
 *         operation.
 *         The buffer needs to be at least sha512 state size long as defined in
 *         rfc4868 (64 bytes)
 *         len - Length on the input to be processed.
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsHashSHA512Full(uint8_t *in, uint8_t *out, uint32_t len);

/**
 * @ingroup QatUtils
 *
 * @brief  Single block AES encrypt
 *
 * @param  key - pointer to symmetric key.
 *         keyLenInBytes - key length
 *         in - pointer to data to encrypt
 *         out - pointer to output buffer for encrypted text
 *         The in and out buffers need to be at least AES block size long
 *         as defined in rfc3686 (16 bytes)
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsAESEncrypt(uint8_t *key,
			     uint32_t keyLenInBytes,
			     uint8_t *in,
			     uint8_t *out);

/**
 * @ingroup QatUtils
 *
 * @brief  Converts AES forward key to reverse key
 *
 * @param  key - pointer to symmetric key.
 *         keyLenInBytes - key length
 *         out - pointer to output buffer for reversed key
 *         The in and out buffers need to be at least AES block size long
 *         as defined in rfc3686 (16 bytes)
 *
 * @li Reentrant: yes
 * @li IRQ safe:  yes
 *
 * @return - CPA_STATUS_SUCCESS/CPA_STATUS_FAIL
 *
 */
CpaStatus qatUtilsAESKeyExpansionForward(uint8_t *key,
					 uint32_t keyLenInBytes,
					 uint32_t *out);
#endif
