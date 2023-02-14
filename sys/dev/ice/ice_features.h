/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2022, Intel Corporation
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
/*$FreeBSD$*/

/**
 * @file ice_features.h
 * @brief device feature controls
 *
 * Contains a list of various device features which could be enabled or
 * disabled.
 */

#ifndef _ICE_FEATURES_H_
#define _ICE_FEATURES_H_

/**
 * @enum feat_list
 * @brief driver feature enumeration
 *
 * Enumeration of possible device driver features that can be enabled or
 * disabled. Each possible value represents a different feature which can be
 * enabled or disabled.
 *
 * The driver stores a bitmap of the features that the device and OS are
 * capable of, as well as another bitmap indicating which features are
 * currently enabled for that device.
 */
enum feat_list {
	ICE_FEATURE_SRIOV,
	ICE_FEATURE_RSS,
	ICE_FEATURE_NETMAP,
	ICE_FEATURE_FDIR,
	ICE_FEATURE_MSI,
	ICE_FEATURE_MSIX,
	ICE_FEATURE_RDMA,
	ICE_FEATURE_SAFE_MODE,
	ICE_FEATURE_LENIENT_LINK_MODE,
	ICE_FEATURE_LINK_MGMT_VER_1,
	ICE_FEATURE_LINK_MGMT_VER_2,
	ICE_FEATURE_HEALTH_STATUS,
	ICE_FEATURE_FW_LOGGING,
	ICE_FEATURE_HAS_PBA,
	ICE_FEATURE_DCB,
	ICE_FEATURE_TX_BALANCE,
	/* Must be last entry */
	ICE_FEATURE_COUNT
};

/**
 * ice_disable_unsupported_features - Disable features not enabled by OS
 * @bitmap: the feature bitmap
 *
 * Check for OS support of various driver features. Clear the feature bit for
 * any feature which is not enabled by the OS. This should be called early
 * during driver attach after setting up the feature bitmap.
 *
 * @remark the bitmap parameter is marked as unused in order to avoid an
 * unused parameter warning in case none of the features need to be disabled.
 */
static inline void
ice_disable_unsupported_features(ice_bitmap_t __unused *bitmap)
{
	ice_clear_bit(ICE_FEATURE_SRIOV, bitmap);
#ifndef DEV_NETMAP
	ice_clear_bit(ICE_FEATURE_NETMAP, bitmap);
#endif
}

#endif
