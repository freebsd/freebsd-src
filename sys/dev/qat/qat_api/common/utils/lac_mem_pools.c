/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 ***************************************************************************
 * @file lac_mem_pools.c
 *
 * @ingroup LacMemPool
 *
 * Memory Pool creation and mgmt function implementations
 *
 ***************************************************************************/

#include "cpa.h"
#include "qat_utils.h"
#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_adf_debug.h"
#include "lac_lock_free_stack.h"
#include "lac_mem_pools.h"
#include "lac_mem.h"
#include "lac_common.h"
#include "cpa_dc.h"
#include "dc_session.h"
#include "dc_datapath.h"
#include "icp_qat_fw_comp.h"
#include "icp_buffer_desc.h"
#include "lac_sym.h"

#define LAC_MEM_POOLS_NUM_SUPPORTED 32000
/**< @ingroup LacMemPool
 * Number of mem pools supported */

#define LAC_MEM_POOLS_NAME_SIZE 17
/**< @ingroup LacMemPool
 * 16 bytes plus '\\0' terminator */

/**< @ingroup LacMemPool
 *     This structure is used to manage each pool created using this utility
 * feature. The client will maintain a pointer (identifier) to the created
 * structure per pool.
 */
typedef struct lac_mem_pool_hdr_s {
	lock_free_stack_t stack;
	char poolName[LAC_MEM_POOLS_NAME_SIZE]; /*16 bytes of a pool name */
	/**< up to 16 bytes of a pool name */
	unsigned int numElementsInPool;
	/**< number of elements in the Pool */
	unsigned int blkSizeInBytes;
	/**< Block size in bytes */
	unsigned int blkAlignmentInBytes;
	/**< block alignment in bytes */
	lac_mem_blk_t **trackBlks;
	/* An array of mem block pointers to track the allocated entries in pool
	 */
	volatile size_t availBlks;
	/* Number of blocks available for allocation in this pool */
} lac_mem_pool_hdr_t;

static lac_mem_pool_hdr_t *lac_mem_pools[LAC_MEM_POOLS_NUM_SUPPORTED] = {
	NULL
};
/**< @ingroup LacMemPool
 * Array of pointers to the mem pool header structure
 */

LAC_DECLARE_HIGHEST_BIT_OF(lac_mem_blk_t);
/**< @ingroup LacMemPool
 * local constant for quickening computation of additional space allocated
 * for holding lac_mem_blk_t container-structure
 */

/**
 *******************************************************************************
 * @ingroup LacMemPool
 * This function cleans up a mem pool.
 ******************************************************************************/
void Lac_MemPoolCleanUpInternal(lac_mem_pool_hdr_t *pPoolID);

static inline Cpa32U
Lac_MemPoolGetElementRealSize(Cpa32U blkSizeInBytes, Cpa32U blkAlignmentInBytes)
{
	Cpa32U addSize = (blkAlignmentInBytes >= sizeof(lac_mem_blk_t) ?
			      blkAlignmentInBytes :
			      1 << (highest_bit_of_lac_mem_blk_t + 1));
	return blkSizeInBytes + addSize;
}

CpaStatus
Lac_MemPoolCreate(lac_memory_pool_id_t *pPoolID,
		  char *poolName,
		  unsigned int numElementsInPool,   /*Number of elements*/
		  unsigned int blkSizeInBytes,      /*Block Size in bytes*/
		  unsigned int blkAlignmentInBytes, /*Block alignment (bytes)*/
		  CpaBoolean trackMemory,
		  Cpa32U node)
{
	unsigned int poolSearch = 0;
	unsigned int counter = 0;
	lac_mem_blk_t *pMemBlkCurrent = NULL;

	void *pMemBlk = NULL;

	if (pPoolID == NULL) {
		QAT_UTILS_LOG("Invalid Pool ID param\n");
		return CPA_STATUS_INVALID_PARAM; /*Error*/
	}

	/* Find First available Pool return error otherwise */
	while (lac_mem_pools[poolSearch] != NULL) {
		poolSearch++;
		if (LAC_MEM_POOLS_NUM_SUPPORTED == poolSearch) {
			QAT_UTILS_LOG(
			    "No more memory pools available for allocation.\n");
			return CPA_STATUS_FAIL;
		}
	}

	/* Allocate a Pool header */
	lac_mem_pools[poolSearch] = LAC_OS_MALLOC(sizeof(lac_mem_pool_hdr_t));
	if (NULL == lac_mem_pools[poolSearch]) {
		QAT_UTILS_LOG(
		    "Unable to allocate memory for creation of the pool.\n");
		return CPA_STATUS_RESOURCE; /*Error*/
	}
	memset(lac_mem_pools[poolSearch], 0, sizeof(lac_mem_pool_hdr_t));

	/* Copy in Pool Name */
	if (poolName != NULL) {
		snprintf(lac_mem_pools[poolSearch]->poolName,
			 LAC_MEM_POOLS_NAME_SIZE,
			 "%s",
			 poolName);
	} else {
		LAC_OS_FREE(lac_mem_pools[poolSearch]);
		lac_mem_pools[poolSearch] = NULL;
		QAT_UTILS_LOG("Invalid Pool Name pointer\n");
		return CPA_STATUS_INVALID_PARAM; /*Error*/
	}

	/* Allocate table for tracking memory blocks */
	if (CPA_TRUE == trackMemory) {
		lac_mem_pools[poolSearch]->trackBlks = LAC_OS_MALLOC(
		    (sizeof(lac_mem_blk_t *) * numElementsInPool));
		if (NULL == lac_mem_pools[poolSearch]->trackBlks) {
			LAC_OS_FREE(lac_mem_pools[poolSearch]);
			lac_mem_pools[poolSearch] = NULL;
			QAT_UTILS_LOG(
			    "Unable to allocate memory for tracking memory blocks.\n");
			return CPA_STATUS_RESOURCE; /*Error*/
		}
	} else {
		lac_mem_pools[poolSearch]->trackBlks = NULL;
	}

	lac_mem_pools[poolSearch]->availBlks = 0;
	lac_mem_pools[poolSearch]->stack = _init_stack();

	/* Calculate alignment needed for allocation   */
	for (counter = 0; counter < numElementsInPool; counter++) {
		CpaPhysicalAddr physAddr = 0;
		/* realSize is computed for allocation of  blkSize bytes +
		   additional
		   capacity for lac_mem_blk_t structure storage due to the some
		   OSes
		   (BSD) limitations for memory alignment to be power of 2;
		   sizeof(lac_mem_blk_t) is being round up to the closest power
		   of 2 -
		   optimised towards the least CPU overhead but at additional
		   memory
		   cost
		 */
		Cpa32U realSize =
		    Lac_MemPoolGetElementRealSize(blkSizeInBytes,
						  blkAlignmentInBytes);
		Cpa32U addSize = realSize - blkSizeInBytes;

		if (CPA_STATUS_SUCCESS != LAC_OS_CAMALLOC(&pMemBlk,
							  realSize,
							  blkAlignmentInBytes,
							  node)) {
			Lac_MemPoolCleanUpInternal(lac_mem_pools[poolSearch]);
			lac_mem_pools[poolSearch] = NULL;
			QAT_UTILS_LOG(
			    "Unable to allocate contiguous chunk of memory.\n");
			return CPA_STATUS_RESOURCE;
		}

		/* Calcaulate various offsets */
		physAddr = LAC_OS_VIRT_TO_PHYS_INTERNAL(
		    (void *)((LAC_ARCH_UINT)pMemBlk + addSize));

		/* physAddr is now already aligned to the greater power of 2:
		    blkAlignmentInBytes or sizeof(lac_mem_blk_t) round up
		    We safely put the structure right before the blkSize
		    real data block
		 */
		pMemBlkCurrent =
		    (lac_mem_blk_t *)(((LAC_ARCH_UINT)(pMemBlk)) + addSize -
				      sizeof(lac_mem_blk_t));

		pMemBlkCurrent->physDataPtr = physAddr;
		pMemBlkCurrent->pMemAllocPtr = pMemBlk;
		pMemBlkCurrent->pPoolID = lac_mem_pools[poolSearch];
		pMemBlkCurrent->isInUse = CPA_FALSE;
		pMemBlkCurrent->pNext = NULL;

		push(&lac_mem_pools[poolSearch]->stack, pMemBlkCurrent);

		/* Store allocated memory pointer */
		if (lac_mem_pools[poolSearch]->trackBlks != NULL) {
			(lac_mem_pools[poolSearch]->trackBlks[counter]) =
			    (lac_mem_blk_t *)pMemBlkCurrent;
		}
		__sync_add_and_fetch(&lac_mem_pools[poolSearch]->availBlks, 1);
		(lac_mem_pools[poolSearch])->numElementsInPool = counter + 1;
	}

	/* Set Pool details in the header */
	(lac_mem_pools[poolSearch])->blkSizeInBytes = blkSizeInBytes;
	(lac_mem_pools[poolSearch])->blkAlignmentInBytes = blkAlignmentInBytes;
	/* Set the Pool ID output parameter */
	*pPoolID = (LAC_ARCH_UINT)(lac_mem_pools[poolSearch]);
	/* Success */
	return CPA_STATUS_SUCCESS;
}

void *
Lac_MemPoolEntryAlloc(lac_memory_pool_id_t poolID)
{
	lac_mem_pool_hdr_t *pPoolID = (lac_mem_pool_hdr_t *)poolID;
	lac_mem_blk_t *pMemBlkCurrent = NULL;

	/* Explicitly removing NULL PoolID check for speed */
	if (pPoolID == NULL) {
		QAT_UTILS_LOG("Invalid Pool ID");
		return NULL;
	}

	/* Remove block from pool */
	pMemBlkCurrent = pop(&pPoolID->stack);
	if (NULL == pMemBlkCurrent) {
		return (void *)CPA_STATUS_RETRY;
	}
	__sync_sub_and_fetch(&pPoolID->availBlks, 1);
	pMemBlkCurrent->isInUse = CPA_TRUE;
	return (void *)((LAC_ARCH_UINT)(pMemBlkCurrent) +
			sizeof(lac_mem_blk_t));
}

void
Lac_MemPoolEntryFree(void *pEntry)
{
	lac_mem_blk_t *pMemBlk = NULL;

	/* Explicitly NULL pointer check */
	if (pEntry == NULL) {
		QAT_UTILS_LOG("Memory Handle NULL");
		return;
	}

	pMemBlk =
	    (lac_mem_blk_t *)((LAC_ARCH_UINT)pEntry - sizeof(lac_mem_blk_t));
	pMemBlk->isInUse = CPA_FALSE;

	push(&pMemBlk->pPoolID->stack, pMemBlk);
	__sync_add_and_fetch(&pMemBlk->pPoolID->availBlks, 1);
}

void
Lac_MemPoolDestroy(lac_memory_pool_id_t poolID)
{
	unsigned int poolSearch = 0;
	lac_mem_pool_hdr_t *pPoolID = (lac_mem_pool_hdr_t *)poolID;

	if (pPoolID != NULL) {
		/*Remove entry from table*/
		while (lac_mem_pools[poolSearch] != pPoolID) {
			poolSearch++;

			if (LAC_MEM_POOLS_NUM_SUPPORTED == poolSearch) {
				QAT_UTILS_LOG("Invalid Pool ID submitted.\n");
				return;
			}
		}

		lac_mem_pools[poolSearch] = NULL; /*Remove handle from pool*/

		Lac_MemPoolCleanUpInternal(pPoolID);
	}
}

void
Lac_MemPoolCleanUpInternal(lac_mem_pool_hdr_t *pPoolID)
{
	lac_mem_blk_t *pCurrentBlk = NULL;
	void *pFreePtr = NULL;
	Cpa32U count = 0;

	if (pPoolID->trackBlks == NULL) {
		pCurrentBlk = pop(&pPoolID->stack);

		while (pCurrentBlk != NULL) {
			/* Free Data Blocks */
			pFreePtr = pCurrentBlk->pMemAllocPtr;
			pCurrentBlk = pop(&pPoolID->stack);
			LAC_OS_CAFREE(pFreePtr);
		}
	} else {
		for (count = 0; count < pPoolID->numElementsInPool; count++) {
			pFreePtr = (pPoolID->trackBlks[count])->pMemAllocPtr;
			LAC_OS_CAFREE(pFreePtr);
		}
		LAC_OS_FREE(pPoolID->trackBlks);
	}
	LAC_OS_FREE(pPoolID);
}

unsigned int
Lac_MemPoolAvailableEntries(lac_memory_pool_id_t poolID)
{
	lac_mem_pool_hdr_t *pPoolID = (lac_mem_pool_hdr_t *)poolID;
	if (pPoolID == NULL) {
		QAT_UTILS_LOG("Invalid Pool ID\n");
		return 0;
	}
	return pPoolID->availBlks;
}

void
Lac_MemPoolStatsShow(void)
{
	unsigned int index = 0;
	QAT_UTILS_LOG(SEPARATOR BORDER
		      "           Memory Pools Stats\n" SEPARATOR);

	while (index < LAC_MEM_POOLS_NUM_SUPPORTED) {
		if (lac_mem_pools[index] != NULL) {
			QAT_UTILS_LOG(
			    BORDER " Pool Name:             %s \n" BORDER
				   " No. Elements in Pool:  %10u \n" BORDER
				   " Element Size in Bytes: %10u \n" BORDER
				   " Alignment in Bytes:    %10u \n" BORDER
				   " No. Available Blocks:  %10zu \n" SEPARATOR,
			    lac_mem_pools[index]->poolName,
			    lac_mem_pools[index]->numElementsInPool,
			    lac_mem_pools[index]->blkSizeInBytes,
			    lac_mem_pools[index]->blkAlignmentInBytes,
			    lac_mem_pools[index]->availBlks);
		}
		index++;
	}
}

static void
Lac_MemPoolInitSymCookies(lac_sym_cookie_t *pSymCookie)
{
	pSymCookie->keyContentDescPhyAddr =
	    LAC_OS_VIRT_TO_PHYS_INTERNAL(pSymCookie->u.keyCookie.contentDesc);
	pSymCookie->keyHashStateBufferPhyAddr = LAC_OS_VIRT_TO_PHYS_INTERNAL(
	    pSymCookie->u.keyCookie.hashStateBuffer);
	pSymCookie->keySslKeyInputPhyAddr = LAC_OS_VIRT_TO_PHYS_INTERNAL(
	    &(pSymCookie->u.keyCookie.u.sslKeyInput));
	pSymCookie->keyTlsKeyInputPhyAddr = LAC_OS_VIRT_TO_PHYS_INTERNAL(
	    &(pSymCookie->u.keyCookie.u.tlsKeyInput));
}

CpaStatus
Lac_MemPoolInitSymCookiesPhyAddr(lac_memory_pool_id_t poolID)
{
	lac_mem_pool_hdr_t *pPoolID = (lac_mem_pool_hdr_t *)poolID;
	lac_sym_cookie_t *pSymCookie = NULL;
	lac_mem_blk_t *pCurrentBlk = NULL;

	if (NULL == pPoolID) {
		QAT_UTILS_LOG("Invalid Pool ID\n");
		return CPA_STATUS_FAIL;
	}

	if (pPoolID->trackBlks == NULL) {
		pCurrentBlk = top(&pPoolID->stack);

		while (pCurrentBlk != NULL) {
			pSymCookie =
			    (lac_sym_cookie_t *)((LAC_ARCH_UINT)(pCurrentBlk) +
						 sizeof(lac_mem_blk_t));
			pCurrentBlk = pCurrentBlk->pNext;
			Lac_MemPoolInitSymCookies(pSymCookie);
		}
	} else {
		Cpa32U count = 0;

		for (count = 0; count < pPoolID->numElementsInPool; count++) {
			pCurrentBlk = pPoolID->trackBlks[count];
			pSymCookie =
			    (lac_sym_cookie_t *)((LAC_ARCH_UINT)(pCurrentBlk) +
						 sizeof(lac_mem_blk_t));
			Lac_MemPoolInitSymCookies(pSymCookie);
		}
	}
	return CPA_STATUS_SUCCESS;
}

CpaStatus
Lac_MemPoolInitDcCookiePhyAddr(lac_memory_pool_id_t poolID)
{
	lac_mem_pool_hdr_t *pPoolID = (lac_mem_pool_hdr_t *)poolID;
	lac_mem_blk_t *pCurrentBlk = NULL;

	if (NULL == pPoolID) {
		QAT_UTILS_LOG("Invalid Pool ID\n");
		return CPA_STATUS_FAIL;
	}

	if (NULL == pPoolID->trackBlks) {
		pCurrentBlk = top(&pPoolID->stack);

		while (pCurrentBlk != NULL) {
			pCurrentBlk = pCurrentBlk->pNext;
		}
	} else {
		Cpa32U count = 0;

		for (count = 0; count < pPoolID->numElementsInPool; count++) {
			pCurrentBlk = pPoolID->trackBlks[count];
		}
	}
	return CPA_STATUS_SUCCESS;
}
