/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include "qat_utils.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/time.h>
#include <sys/stdarg.h>
#include <vm/vm.h>
#include <vm/pmap.h>

/**
 *
 * @brief Private data structure
 *
 *  Data struct to store the information on the
 *  memory allocated. This structure is stored at the beginning of
 *  the allocated chunk of memory
 *  size is the no of byte passed to the memory allocation functions
 *  mSize is the real size of the memory required to the OS
 *
 *  +----------------------------+--------------------------------+
 *  | QatUtilsMemAllocInfoStruct | memory returned to user (size) |
 *  +----------------------------+--------------------------------+
 *  ^                            ^
 *  mAllocMemPtr                 Ptr returned to the caller of MemAlloc*
 *
 */

typedef struct _QatUtilsMemAllocInfoStruct {
	void *mAllocMemPtr; /* memory addr returned by the kernel */
	uint32_t mSize;     /* allocated size */
} QatUtilsMemAllocInfoStruct;

/**************************************
 * Memory functions
 *************************************/
void *
qatUtilsMemAllocContiguousNUMA(uint32_t size, uint32_t node, uint32_t alignment)
{
	void *ptr = NULL;
	void *pRet = NULL;
	uint32_t alignment_offset = 0;

	QatUtilsMemAllocInfoStruct memInfo = { 0 };
	if (size == 0 || alignment < 1) {
		QAT_UTILS_LOG(
		    "QatUtilsMemAllocNUMA: size or alignment are zero.\n");
		return NULL;
	}
	if (alignment & (alignment - 1)) {
		QAT_UTILS_LOG(
		    "QatUtilsMemAllocNUMA: Expecting alignment of a power.\n");
		return NULL;
	}

	memInfo.mSize = size + alignment + sizeof(QatUtilsMemAllocInfoStruct);
	ptr = contigmalloc(memInfo.mSize, M_QAT, M_WAITOK, 0, ~1UL, 64, 0);

	memInfo.mAllocMemPtr = ptr;
	pRet =
	    (char *)memInfo.mAllocMemPtr + sizeof(QatUtilsMemAllocInfoStruct);
#ifdef __x86_64__
	alignment_offset = (uint64_t)pRet % alignment;
#else
	alignment_offset = (uint32_t)pRet % alignment;
#endif
	pRet = (char *)pRet + (alignment - alignment_offset);
	memcpy(((char *)pRet) - sizeof(QatUtilsMemAllocInfoStruct),
	       &memInfo,
	       sizeof(QatUtilsMemAllocInfoStruct));

	return pRet;
}

void
qatUtilsMemFreeNUMA(void *ptr)
{
	QatUtilsMemAllocInfoStruct *memInfo = NULL;

	memInfo =
	    (QatUtilsMemAllocInfoStruct *)((int8_t *)ptr -
					   sizeof(QatUtilsMemAllocInfoStruct));
	if (memInfo->mSize == 0 || memInfo->mAllocMemPtr == NULL) {
		QAT_UTILS_LOG(
		    "QatUtilsMemAlignedFree: Detected corrupted data: memory leak!\n");
		return;
	}
	free(memInfo->mAllocMemPtr, M_QAT);
}

CpaStatus
qatUtilsSleep(uint32_t milliseconds)
{
	if (milliseconds != 0) {
		pause("qatUtils sleep", milliseconds * hz / (1000));
	} else {
		sched_relinquish(curthread);
	}
	return CPA_STATUS_SUCCESS;
}

void
qatUtilsYield(void)
{
	sched_relinquish(curthread);
}
