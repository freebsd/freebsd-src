/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ice_drv_info.h
 * @brief device IDs and driver version
 *
 * Contains the device IDs tables and the driver version string.
 *
 * This file contains static or constant definitions intended to be included
 * exactly once in the main driver interface file. It implicitly depends on
 * the main driver header file.
 *
 * These definitions could be placed directly in the interface file, but are
 * kept separate for organizational purposes.
 */

/**
 * @var ice_driver_version
 * @brief driver version string
 *
 * Driver version information, used for display as part of an informational
 * sysctl, and as part of the driver information sent to the firmware at load.
 *
 * @var ice_major_version
 * @brief driver major version number
 *
 * @var ice_minor_version
 * @brief driver minor version number
 *
 * @var ice_patch_version
 * @brief driver patch version number
 *
 * @var ice_rc_version
 * @brief driver release candidate version number
 */
const char ice_driver_version[] = "1.37.11-k";
const uint8_t ice_major_version = 1;
const uint8_t ice_minor_version = 37;
const uint8_t ice_patch_version = 11;
const uint8_t ice_rc_version = 0;

#define PVIDV(vendor, devid, name) \
	PVID(vendor, devid, name " - 1.37.11-k")
#define PVIDV_OEM(vendor, devid, svid, sdevid, revid, name) \
	PVID_OEM(vendor, devid, svid, sdevid, revid, name " - 1.37.11-k")

/**
 * @var ice_vendor_info_array
 * @brief array of PCI devices supported by this driver
 *
 * Array of PCI devices which are supported by this driver. Used to determine
 * whether a given device should be loaded by this driver. This information is
 * also exported as part of the module information for other tools to analyze.
 *
 * @remark Each type of device ID needs to be listed from most-specific entry
 * to most-generic entry; e.g. PVIDV_OEM()s for a device ID must come before
 * the PVIDV() for it.
 */
static const pci_vendor_info_t ice_vendor_info_array[] = {
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_BACKPLANE,
		"Intel(R) Ethernet Controller E810-C for backplane"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0001, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q1"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0002, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q2"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0003, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q1"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0004, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q2"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0005, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q1 for OCP3.0"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0006, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q2 for OCP3.0"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0007, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q1 for OCP3.0"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x0008, 0,
		"Intel(R) Ethernet Network Adapter E810-C-Q2 for OCP3.0"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x000D, 0,
		"Intel(R) Ethernet Network Adapter E810-L-Q2 for OCP3.0"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		ICE_INTEL_VENDOR_ID, 0x000E, 0,
		"Intel(R) Ethernet Network Adapter E810-2C-Q2"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP,
		"Intel(R) Ethernet Controller E810-C for QSFP"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_SFP,
		ICE_INTEL_VENDOR_ID, 0x0005, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-4"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_SFP,
		ICE_INTEL_VENDOR_ID, 0x0006, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-4"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_SFP,
		ICE_INTEL_VENDOR_ID, 0x0007, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-4"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_SFP,
		ICE_INTEL_VENDOR_ID, 0x000C, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-4 for OCP 3.0"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_SFP,
		"Intel(R) Ethernet Controller E810-C for SFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_BACKPLANE,
	      "Intel(R) Ethernet Connection E822-C for backplane"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_QSFP,
	      "Intel(R) Ethernet Connection E822-C for QSFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_SFP,
	      "Intel(R) Ethernet Connection E822-C for SFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_10G_BASE_T,
	      "Intel(R) Ethernet Connection E822-C/X557-AT 10GBASE-T"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822C_SGMII,
	      "Intel(R) Ethernet Connection E822-C 1GbE"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_BACKPLANE,
	      "Intel(R) Ethernet Connection E822-L for backplane"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_SFP,
	      "Intel(R) Ethernet Connection E822-L for SFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_10G_BASE_T,
	      "Intel(R) Ethernet Connection E822-L/X557-AT 10GBASE-T"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E822L_SGMII,
	      "Intel(R) Ethernet Connection E822-L 1GbE"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_BACKPLANE,
	      "Intel(R) Ethernet Connection E823-L for backplane"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_SFP,
	      "Intel(R) Ethernet Connection E823-L for SFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_QSFP,
	      "Intel(R) Ethernet Connection E823-L for QSFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_10G_BASE_T,
	      "Intel(R) Ethernet Connection E823-L/X557-AT 10GBASE-T"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823L_1GBE,
	      "Intel(R) Ethernet Connection E823-L 1GbE"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823C_BACKPLANE,
	      "Intel(R) Ethernet Connection E823-C for backplane"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823C_QSFP,
	      "Intel(R) Ethernet Connection E823-C for QSFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823C_SFP,
	      "Intel(R) Ethernet Connection E823-C for SFP"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823C_10G_BASE_T,
	      "Intel(R) Ethernet Connection E823-C/X557-AT 10GBASE-T"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E823C_SGMII,
	      "Intel(R) Ethernet Connection E823-C 1GbE"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_BACKPLANE,
	      "Intel(R) Ethernet Controller E810-XXV for backplane"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_QSFP,
		"Intel(R) Ethernet Controller E810-XXV for QSFP"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_SFP,
		ICE_INTEL_VENDOR_ID, 0x0003, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-2"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_SFP,
		ICE_INTEL_VENDOR_ID, 0x0004, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-2"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_SFP,
		ICE_INTEL_VENDOR_ID, 0x0005, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-2 for OCP 3.0"),
	PVIDV_OEM(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_SFP,
		ICE_INTEL_VENDOR_ID, 0x0006, 0,
		"Intel(R) Ethernet Network Adapter E810-XXV-2 for OCP 3.0"),
	PVIDV(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810_XXV_SFP,
		"Intel(R) Ethernet Controller E810-XXV for SFP"),
	PVID_END
};

