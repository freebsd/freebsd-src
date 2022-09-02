/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/******************************************************************************
 * @file icp_adf_cfg.h
 *
 * @defgroup icp_AdfCfg Acceleration Driver Framework Configuration Interface.
 *
 * @ingroup icp_Adf
 *
 * @description
 *      This is the top level header file for the run-time system configuration
 *      parameters. This interface may be used by components of this API to
 *      access the supported run-time configuration parameters.
 *
 *****************************************************************************/

#ifndef ICP_ADF_CFG_H
#define ICP_ADF_CFG_H

#include "cpa.h"
#include "icp_accel_devices.h"

/******************************************************************************
* Section for #define's & typedef's
******************************************************************************/
/* Address of the UOF firmware */
#define ICP_CFG_UOF_ADDRESS_KEY ("Firmware_UofAddress")
/* Size of the UOF firmware */
#define ICP_CFG_UOF_SIZE_BYTES_KEY ("Firmware_UofSizeInBytes")
/* Address of the MMP firmware */
#define ICP_CFG_MMP_ADDRESS_KEY ("Firmware_MmpAddress")
/* Size of the MMP firmware */
#define ICP_CFG_MMP_SIZE_BYTES_KEY ("Firmware_MMpSizeInBytes")
/* MMP firmware version */
#define ICP_CFG_MMP_VER_KEY ("Firmware_MmpVer")
/* UOF firmware version */
#define ICP_CFG_UOF_VER_KEY ("Firmware_UofVer")
/* Tools version */
#define ICP_CFG_TOOLS_VER_KEY ("Firmware_ToolsVer")
/* Hardware rev id */
#define ICP_CFG_HW_REV_ID_KEY ("HW_RevId")
/* Lowest Compatible Driver Version */
#define ICP_CFG_LO_COMPATIBLE_DRV_KEY ("Lowest_Compat_Drv_Ver")
/* Pke Service Disabled flag */
#define ICP_CFG_PKE_DISABLED ("PkeServiceDisabled")
/* SRAM Physical Address Key */
#define ADF_SRAM_PHYSICAL_ADDRESS ("Sram_PhysicalAddress")
/* SRAM Virtual Address Key */
#define ADF_SRAM_VIRTUAL_ADDRESS ("Sram_VirtualAddress")
/* SRAM Size In Bytes Key */
#define ADF_SRAM_SIZE_IN_BYTES ("Sram_SizeInBytes")
/* Device node id, tells to which die the device is
 * connected to */
#define ADF_DEV_NODE_ID "Device_NodeId"
/* Device package id, this is accel_dev id */
#define ADF_DEV_PKG_ID "Device_PkgId"
/* Device bus address, B.D.F (Bus(8bits),Device(5bits),Function(3bits)) */
#define ADF_DEV_BUS_ADDRESS ("Device_BusAddress")
/* Number of Acceleration Engines */
#define ADF_DEV_NUM_AE ("Device_Num_AE")
/* Number of Accelerators */
#define ADF_DEV_NUM_ACCEL ("Device_Num_Accel")
/* Max Number of Acceleration Engines */
#define ADF_DEV_MAX_AE ("Device_Max_AE")
/* Max Number of Accelerators */
#define ADF_DEV_MAX_ACCEL ("Device_Max_Accel")
/* QAT/AE Mask*/
#define ADF_DEV_MAX_RING_PER_QAT ("Device_Max_Num_Rings_per_Accel")
/* Number of Accelerators */
#define ADF_DEV_ACCELAE_MASK_FMT ("Device_Accel_AE_Mask_%d")
/* VF ring offset */
#define ADF_VF_RING_OFFSET_KEY ("VF_RingOffset")
/* Mask of Accelerators */
#define ADF_DEV_AE_MASK ("Device_AE_Mask")
/* Whether or not arbitration is supported */
#define ADF_DEV_ARB_SUPPORTED ("ArbitrationSupported")
/* Slice Watch Dog Timer for CySym+Comp */
#define ADF_DEV_SSM_WDT_BULK "CySymAndDcWatchDogTimer"
/* Slice Watch Dog Timer for CyAsym */
#define ADF_DEV_SSM_WDT_PKE "CyAsymWatchDogTimer"

/* String names for the exposed sections of config file. */
#define GENERAL_SEC "GENERAL"
#define WIRELESS_SEC "WIRELESS_INT_"
#define DYN_SEC "DYN"
#define DEV_LIMIT_CFG_ACCESS_TMPL "_D_L_ACC"
/*#define WIRELESS_ENABLED                "WirelessEnabled"*/

/*
 * icp_adf_cfgGetParamValue
 *
 * Description:
 * This function is used to determine the value for a given parameter name.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_cfgGetParamValue(icp_accel_dev_t *accel_dev,
				   const char *section,
				   const char *param_name,
				   char *param_value);
/*
 * icp_adf_cfgGetRingNumber
 *
 * Description:
 * Function returns ring number configured for the service.
 * NOTE: this function will only be used by QATAL in kernelspace.
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_cfgGetRingNumber(icp_accel_dev_t *accel_dev,
				   const char *section_name,
				   const Cpa32U accel_num,
				   const Cpa32U bank_num,
				   const char *pServiceName,
				   Cpa32U *pRingNum);

/*
 * icp_adf_get_busAddress
 * Gets the B.D.F. of the physical device
 */
Cpa16U icp_adf_get_busAddress(Cpa16U packageId);

#endif /* ICP_ADF_CFG_H */
