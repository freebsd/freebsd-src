/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
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
 * @file iavf_drv_info.h
 * @brief device IDs and driver version
 *
 * Contains the device IDs tables and the driver version string.
 *
 * It must be included after iavf_legacy.h or iavf_iflib.h, and is expected to
 * be included exactly once in the associated if_iavf file. Thus, it does not
 * have the standard header guard.
 */

/**
 * @var iavf_driver_version
 * @brief driver version string
 *
 * Driver version information, used for display as part of an informational
 * sysctl.
 */
const char iavf_driver_version[] = "3.0.26-k";

#define PVIDV(vendor, devid, name) \
	PVID(vendor, devid, name " - 3.0.26-k")
#define PVIDV_OEM(vendor, devid, svid, sdevid, revid, name) \
	PVID_OEM(vendor, devid, svid, sdevid, revid, name " - 3.0.26-k")

/**
 * @var iavf_vendor_info_array
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
static const pci_vendor_info_t iavf_vendor_info_array[] = {
	PVIDV(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_VF,
	    "Intel(R) Ethernet Virtual Function 700 Series"),
	PVIDV(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_X722_VF,
	    "Intel(R) Ethernet Virtual Function 700 Series (X722)"),
	PVIDV(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_ADAPTIVE_VF,
	    "Intel(R) Ethernet Adaptive Virtual Function"),
	PVID_END
};
