/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 ***************************************************************************
 * @file icp_sal_iommu.h
 *
 * @ingroup SalUser
 *
 * Sal iommu wrapper functions.
 *
 ***************************************************************************/

#ifndef ICP_SAL_IOMMU_H
#define ICP_SAL_IOMMU_H

/*************************************************************************
  * @ingroup Sal
  * @description
  *   Function returns page_size rounded size for iommu remapping
  *
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @reentrant
  *      No
  * @threadSafe
  *      No
  *
  * @param[in] size           Minimum required size.
  *
  * @retval    page_size rounded size for iommu remapping.
  *
  *************************************************************************/
size_t icp_sal_iommu_get_remap_size(size_t size);

/*************************************************************************
  * @ingroup Sal
  * @description
  *   Function adds an entry into iommu remapping table
  *
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @reentrant
  *      No
  * @threadSafe
  *      No
  *
  * @param[in] phaddr         Host physical address.
  * @param[in] iova           Guest physical address.
  * @param[in] size           Size of the remapped region.
  *
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  *************************************************************************/
CpaStatus icp_sal_iommu_map(Cpa64U phaddr, Cpa64U iova, size_t size);

/*************************************************************************
  * @ingroup Sal
  * @description
  *   Function removes an entry from iommu remapping table
  *
  * @assumptions
  *      None
  * @sideEffects
  *      None
  * @reentrant
  *      No
  * @threadSafe
  *      No
  *
  * @param[in] iova           Guest physical address to be removed.
  * @param[in] size           Size of the remapped region.
  *
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  *************************************************************************/
CpaStatus icp_sal_iommu_unmap(Cpa64U iova, size_t size);
#endif
