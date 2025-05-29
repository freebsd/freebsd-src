/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_dev.h
 *
 * @defgroup cpaDev Device API
 *
 * @ingroup cpa
 *
 * @description
 *      These functions specify the API for device level operation.
 *
 * @remarks
 *
 *
 *****************************************************************************/

#ifndef CPA_DEV_H
#define CPA_DEV_H

#ifdef __cplusplus
extern"C" {
#endif


#ifndef CPA_H
#include "cpa.h"
#endif


 /*****************************************************************************
 * @ingroup cpaDev
 *      Returns device information
 *
 * @description
 *      This data structure contains the device information. The device
 *      information are available to both Physical and Virtual Functions.
 *      Depending on the resource partitioning configuration, the services
 *      available may changes. This configuration will impact the size of the
 *      Security Association Database (SADB). Other properties such device SKU
 *      and device ID are also reported.
 *
 *****************************************************************************/
typedef struct _CpaDeviceInfo {
	Cpa32U sku;
	/**< Identifies the SKU of the device. */
	Cpa16U bdf;
	/**< Identifies the Bus Device Function of the device.
	 *   Format is reported as follow:
	 *   - bits<2:0> represent the function number.
	 *   - bits<7:3> represent the device
	 *   - bits<15:8> represent the bus
	 */
	Cpa32U deviceId;
	/**< Returns the device ID. */
	Cpa32U numaNode;
	/**< Return the local NUMA node mapped to the device. */
	CpaBoolean isVf;
	/**< Return whether the device is currently used in a virtual function
	 *   or not. */
	CpaBoolean dcEnabled;
    /**< Compression service enabled */
	CpaBoolean cySymEnabled;
    /**< Symmetric crypto service enabled */
	CpaBoolean cyAsymEnabled;
    /**< Asymmetric crypto service enabled */
	CpaBoolean inlineEnabled;
    /**< Inline service enabled */
	Cpa32U deviceMemorySizeAvailable;
	/**< Return the size of the device memory available. This device memory
	 *   section could be used for the intermediate buffers in the
	 *   compression service.
	 */
} CpaDeviceInfo;


/*****************************************************************************
* @ingroup cpaDev
*      Returns number devices.
*
* @description
*      This API returns the number of devices available to the application.
*      If used on the host, it will return the number of physical devices.
*      If used on the guest, it will return the number of function mapped
*      to the virtual machine.
*
*****************************************************************************/
CpaStatus cpaGetNumDevices (Cpa16U *numDevices);

/*****************************************************************************
* @ingroup cpaDev
*      Returns device information for a given device index.
*
* @description
*      Returns device information for a given device index. This API must
*      be used with cpaGetNumDevices().
*****************************************************************************/
CpaStatus cpaGetDeviceInfo (Cpa16U device, CpaDeviceInfo *deviceInfo);

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_DEV_H */
