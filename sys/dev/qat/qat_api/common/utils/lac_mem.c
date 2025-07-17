/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file lac_mem.c  Implementation of Memory Functions
 *
 * @ingroup LacMem
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include header files
*******************************************************************************
*/
#include "qat_utils.h"
#include "cpa.h"

#include "icp_accel_devices.h"
#include "icp_adf_init.h"
#include "icp_adf_transport.h"
#include "icp_adf_debug.h"
#include "icp_sal_iommu.h"

#include "lac_mem.h"
#include "lac_mem_pools.h"
#include "lac_common.h"
#include "lac_list.h"
#include "icp_qat_fw_la.h"
#include "lac_sal_types.h"

/*
********************************************************************************
* Static Variables
********************************************************************************
*/

#define MAX_BUFFER_SIZE (LAC_BITS_TO_BYTES(4096))
/**< @ingroup LacMem
 * Maximum size of the buffers used in the resize function */

/*
*******************************************************************************
* Define public/global function definitions
*******************************************************************************
*/
/**
 * @ingroup LacMem
 */
CpaStatus
icp_LacBufferRestore(Cpa8U *pUserBuffer,
		     Cpa32U userLen,
		     Cpa8U *pWorkingBuffer,
		     Cpa32U workingLen,
		     CpaBoolean copyBuf)
{
	Cpa32U padSize = 0;

	/* NULL is a valid value for working buffer as this function may be
	 * called to clean up in an error case where all the resize operations
	 * were not completed */
	if (NULL == pWorkingBuffer) {
		return CPA_STATUS_SUCCESS;
	}

	if (workingLen < userLen) {
		QAT_UTILS_LOG("Invalid buffer sizes\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	if (pUserBuffer != pWorkingBuffer) {

		if (CPA_TRUE == copyBuf) {
			/* Copy from internal buffer to user buffer */
			padSize = workingLen - userLen;
			memcpy(pUserBuffer, pWorkingBuffer + padSize, userLen);
		}

		Lac_MemPoolEntryFree(pWorkingBuffer);
	}
	return CPA_STATUS_SUCCESS;
}

/**
 * @ingroup LacMem
 */
CpaPhysicalAddr
SalMem_virt2PhysExternal(void *pVirtAddr, void *pServiceGen)
{
	sal_service_t *pService = (sal_service_t *)pServiceGen;

	if (NULL != pService->virt2PhysClient) {
		return pService->virt2PhysClient(pVirtAddr);
	} else {
		/* Use internal QAT Utils virt to phys */
		/* Ok for kernel space probably should not use for user */
		return LAC_OS_VIRT_TO_PHYS_INTERNAL(pVirtAddr);
	}
}

size_t
icp_sal_iommu_get_remap_size(size_t size)
{
	return size;
}

CpaStatus
icp_sal_iommu_map(Cpa64U phaddr, Cpa64U iova, size_t size)
{
	return CPA_STATUS_SUCCESS;
}

CpaStatus
icp_sal_iommu_unmap(Cpa64U iova, size_t size)
{
	return CPA_STATUS_SUCCESS;
}
