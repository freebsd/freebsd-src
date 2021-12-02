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
/*$FreeBSD$*/

/**
 * @file ice_common_sysctls.h
 * @brief driver wide sysctls not related to the iflib stack
 *
 * Contains static sysctl values which are driver wide and configure all
 * devices of the driver at once.
 *
 * Device specific sysctls are setup by functions in ice_lib.c
 */

#ifndef _ICE_COMMON_SYSCTLS_H_
#define _ICE_COMMON_SYSCTLS_H_

#include <sys/sysctl.h>

/**
 * @var ice_enable_irdma
 * @brief boolean indicating if the iRDMA client interface is enabled
 *
 * Global sysctl variable indicating whether the RDMA client interface feature
 * is enabled.
 */
bool ice_enable_irdma = true;

/**
 * @var ice_enable_tx_fc_filter
 * @brief boolean indicating if the Tx Flow Control filter should be enabled
 *
 * Global sysctl variable indicating whether the Tx Flow Control filters
 * should be enabled. If true, Ethertype 0x8808 packets will be dropped if
 * they come from non-HW sources. If false, packets coming from software will
 * not be dropped. Leave this on if unless you must send flow control frames
 * (or other control frames) from software.
 *
 * @remark each PF has a separate sysctl which can override this value.
 */
bool ice_enable_tx_fc_filter = true;

/**
 * @var ice_enable_tx_lldp_filter
 * @brief boolean indicating if the Tx LLDP filter should be enabled
 *
 * Global sysctl variable indicating whether the Tx Flow Control filters
 * should be enabled. If true, Ethertype 0x88cc packets will be dropped if
 * they come from non-HW sources. If false, packets coming from software will
 * not be dropped. Leave this on if unless you must send LLDP frames from
 * software.
 *
 * @remark each PF has a separate sysctl which can override this value.
 */
bool ice_enable_tx_lldp_filter = true;

/**
 * @var ice_enable_health_events
 * @brief boolean indicating if health status events from the FW should be reported
 *
 * Global sysctl variable indicating whether the Health Status events from the
 * FW should be enabled. If true, if an event occurs, the driver will print out
 * a message with a description of the event and possible actions to take.
 *
 * @remark each PF has a separate sysctl which can override this value.
 */
bool ice_enable_health_events = true;

/**
 * @var ice_rdma_max_msix
 * @brief maximum number of MSI-X vectors to reserve for RDMA interface
 *
 * Global sysctl variable indicating the maximum number of MSI-X vectors to
 * reserve for a single RDMA interface.
 */
static uint16_t ice_rdma_max_msix = ICE_RDMA_MAX_MSIX;

/* sysctls marked as tunable, (i.e. with the CTLFLAG_TUN set) will
 * automatically load tunable values, without the need to manually create the
 * TUNABLE definition.
 *
 * This works since at least FreeBSD 11, and was backported into FreeBSD 10
 * before the FreeBSD 10.1-RELEASE.
 *
 * If the tunable needs a custom loader, mark the SYSCTL as CTLFLAG_NOFETCH,
 * and create the tunable manually.
 */

static SYSCTL_NODE(_hw, OID_AUTO, ice, CTLFLAG_RD, 0, "ICE driver parameters");

static SYSCTL_NODE(_hw_ice, OID_AUTO, debug, ICE_CTLFLAG_DEBUG | CTLFLAG_RD, 0,
		   "ICE driver debug parameters");

SYSCTL_BOOL(_hw_ice, OID_AUTO, enable_health_events, CTLFLAG_RDTUN,
	    &ice_enable_health_events, 0,
	    "Enable FW health event reporting globally");

SYSCTL_BOOL(_hw_ice, OID_AUTO, irdma, CTLFLAG_RDTUN, &ice_enable_irdma, 0,
	    "Enable iRDMA client interface");

SYSCTL_U16(_hw_ice, OID_AUTO, rdma_max_msix, CTLFLAG_RDTUN, &ice_rdma_max_msix,
	   0, "Maximum number of MSI-X vectors to reserve per RDMA interface");

SYSCTL_BOOL(_hw_ice_debug, OID_AUTO, enable_tx_fc_filter, CTLFLAG_RDTUN,
	    &ice_enable_tx_fc_filter, 0,
	    "Drop Ethertype 0x8808 control frames originating from non-HW sources");

SYSCTL_BOOL(_hw_ice_debug, OID_AUTO, enable_tx_lldp_filter, CTLFLAG_RDTUN,
	    &ice_enable_tx_lldp_filter, 0,
	    "Drop Ethertype 0x88cc LLDP frames originating from non-HW sources");

#endif /* _ICE_COMMON_SYSCTLS_H_ */
