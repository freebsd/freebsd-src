/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/*****************************************************************************
 * @file icp_adf_accel_mgr.h
 *
 * @description
 *      This file contains the function prototype for accel
 *      instances management
 *
 *****************************************************************************/
#ifndef ICP_ADF_ACCEL_MGR_H
#define ICP_ADF_ACCEL_MGR_H

/*
 * Device reset mode type.
 * If device reset is triggered from atomic context
 * it needs to be in ICP_ADF_DEV_RESET_ASYNC mode.
 * Otherwise can be either.
 */
typedef enum icp_adf_dev_reset_mode_e {
	ICP_ADF_DEV_RESET_ASYNC = 0,
	ICP_ADF_DEV_RESET_SYNC
} icp_adf_dev_reset_mode_t;

/*
 * icp_adf_reset_dev
 *
 * Description:
 * Function resets the given device.
 * If device reset is triggered from atomic context
 * it needs to be in ICP_ADF_DEV_RESET_ASYNC mode.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_reset_dev(icp_accel_dev_t *accel_dev,
			    icp_adf_dev_reset_mode_t mode);

/*
 * icp_adf_is_dev_in_reset
 * Check if device is in reset state.
 *
 * Returns:
 *   CPA_TRUE   device is in reset state
 *   CPA_FALSE  device is not in reset state
 */
CpaBoolean icp_adf_is_dev_in_reset(icp_accel_dev_t *accel_dev);

/*
 * icp_adf_is_dev_in_error
 * Check if device is in error state.
 *
 * Returns:
 *   CPA_TRUE   device is in error state
 *   CPA_FALSE  device is not in error state
 */
CpaBoolean icp_adf_is_dev_in_error(icp_accel_dev_t *accel_dev);

/*
 * icp_amgr_getNumInstances
 *
 * Description:
 * Returns number of accel instances in the system.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_amgr_getNumInstances(Cpa16U *pNumInstances);

/*
 * icp_amgr_getInstances
 *
 * Description:
 * Returns table of accel instances in the system.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_amgr_getInstances(Cpa16U numInstances,
				icp_accel_dev_t **pAccel_devs);
/*
 * icp_amgr_getAccelDevByName
 *
 * Description:
 * Returns the accel instance by name.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_amgr_getAccelDevByName(unsigned char *instanceName,
				     icp_accel_dev_t **pAccel_dev);
/*
 * icp_amgr_getAccelDevByCapabilities
 *
 * Description:
 * Returns a started accel device that implements the capabilities
 * specified in capabilitiesMask.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_amgr_getAccelDevByCapabilities(Cpa32U capabilitiesMask,
					     icp_accel_dev_t **pAccel_devs,
					     Cpa16U *pNumInstances);
/*
 * icp_amgr_getAllAccelDevByCapabilities
 *
 * Description:
 * Returns table of accel devices that are started and implement
 * the capabilities specified in capabilitiesMask.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_amgr_getAllAccelDevByCapabilities(Cpa32U capabilitiesMask,
						icp_accel_dev_t **pAccel_devs,
						Cpa16U *pNumInstances);

/*
 * icp_amgr_getAccelDevCapabilities
 * Returns accel devices capabilities specified in capabilitiesMask.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_amgr_getAccelDevCapabilities(icp_accel_dev_t *accel_dev,
					   Cpa32U *pCapabilitiesMask);

/*
 * icp_amgr_getAllAccelDevByEachCapability
 *
 * Description:
 * Returns table of accel devices that are started and implement
 * each of the capability specified in capabilitiesMask.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_amgr_getAllAccelDevByEachCapability(Cpa32U capabilitiesMask,
						  icp_accel_dev_t **pAccel_devs,
						  Cpa16U *pNumInstances);

/*
 * icp_qa_dev_get
 *
 * Description:
 * Function increments the device usage counter.
 *
 * Returns: void
 */
void icp_qa_dev_get(icp_accel_dev_t *pDev);

/*
 * icp_qa_dev_put
 *
 * Description:
 * Function decrements the device usage counter.
 *
 * Returns: void
 */
void icp_qa_dev_put(icp_accel_dev_t *pDev);

/*
 * icp_adf_getAccelDevByAccelId
 *
 * Description:
 * Gets the accel_dev structure based on accelId
 *
 * Returns: a pointer to the accelerator structure or NULL if not found.
 */
icp_accel_dev_t *icp_adf_getAccelDevByAccelId(Cpa32U accelId);

#endif /* ICP_ADF_ACCEL_MGR_H */
