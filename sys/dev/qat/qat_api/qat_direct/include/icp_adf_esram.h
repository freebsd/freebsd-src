/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/******************************************************************************
 * @file icp_adf_esram.h
 *
 * @description
 *      This file contains the ADF interface to retrieve eSRAM information
 *
 *****************************************************************************/
#ifndef ICP_ADF_ESRAM_H
#define ICP_ADF_ESRAM_H

/*
 * icp_adf_esramGetAddress
 *
 * Description:
 * Returns the eSRAM's physical and virtual addresses and its size in bytes.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_esramGetAddress(icp_accel_dev_t *accel_dev,
				  Cpa32U accelNumber,
				  Cpa64U *pPhysAddr,
				  Cpa64U *pVirtAddr,
				  Cpa32U *pSize);

#endif /* ICP_ADF_ESRAM_H */
