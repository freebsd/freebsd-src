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
 * @file ice_iflib_recovery_txrx.c
 * @brief iflib Tx/Rx ops for recovery mode
 *
 * Contains the if_txrx structure of operations used when the driver detects
 * that the firmware is in recovery mode. These ops essentially do nothing and
 * exist to prevent any chance that the stack could attempt to transmit or
 * receive when the device is in firmware recovery mode.
 */

#include "ice_iflib.h"

/*
 * iflib txrx methods used when in recovery mode
 */
static int ice_recovery_txd_encap(void *arg, if_pkt_info_t pi);
static int ice_recovery_rxd_pkt_get(void *arg, if_rxd_info_t ri);
static void ice_recovery_txd_flush(void *arg, uint16_t txqid, qidx_t pidx);
static int ice_recovery_txd_credits_update(void *arg, uint16_t txqid, bool clear);
static int ice_recovery_rxd_available(void *arg, uint16_t rxqid, qidx_t pidx, qidx_t budget);
static void ice_recovery_rxd_flush(void *arg, uint16_t rxqid, uint8_t flidx, qidx_t pidx);
static void ice_recovery_rxd_refill(void *arg, if_rxd_update_t iru);

/**
 * @var ice_recovery_txrx
 * @brief Tx/Rx operations for recovery mode
 *
 * Similar to ice_txrx, but contains pointers to functions which are no-ops.
 * Used when the driver is in firmware recovery mode to prevent any attempt to
 * transmit or receive packets while the hardware is not initialized.
 */
struct if_txrx ice_recovery_txrx = {
	.ift_txd_encap = ice_recovery_txd_encap,
	.ift_txd_flush = ice_recovery_txd_flush,
	.ift_txd_credits_update = ice_recovery_txd_credits_update,
	.ift_rxd_available = ice_recovery_rxd_available,
	.ift_rxd_pkt_get = ice_recovery_rxd_pkt_get,
	.ift_rxd_refill = ice_recovery_rxd_refill,
	.ift_rxd_flush = ice_recovery_rxd_flush,
};

/**
 * ice_recovery_txd_encap - prepare Tx descriptors for a packet
 * @arg: the iflib softc structure pointer
 * @pi: packet info
 *
 * Since the Tx queues are not initialized during recovery mode, this function
 * does nothing.
 *
 * @returns ENOSYS
 */
static int
ice_recovery_txd_encap(void __unused *arg, if_pkt_info_t __unused pi)
{
	return (ENOSYS);
}

/**
 * ice_recovery_txd_flush - Flush Tx descriptors to hardware
 * @arg: device specific softc pointer
 * @txqid: the Tx queue to flush
 * @pidx: descriptor index to advance tail to
 *
 * Since the Tx queues are not initialized during recovery mode, this function
 * does nothing.
 */
static void
ice_recovery_txd_flush(void __unused *arg, uint16_t __unused txqid,
		       qidx_t __unused pidx)
{
	;
}

/**
 * ice_recovery_txd_credits_update - cleanup Tx descriptors
 * @arg: device private softc
 * @txqid: the Tx queue to update
 * @clear: if false, only report, do not actually clean
 *
 * Since the Tx queues are not initialized during recovery mode, this function
 * always reports that no descriptors are ready.
 *
 * @returns 0
 */
static int
ice_recovery_txd_credits_update(void __unused *arg, uint16_t __unused txqid,
				bool __unused clear)
{
	return (0);
}

/**
 * ice_recovery_rxd_available - Return number of available Rx packets
 * @arg: device private softc
 * @rxqid: the Rx queue id
 * @pidx: descriptor start point
 * @budget: maximum Rx budget
 *
 * Since the Rx queues are not initialized during recovery mode, this function
 * always reports that no packets are ready.
 *
 * @returns 0
 */
static int
ice_recovery_rxd_available(void __unused *arg, uint16_t __unused rxqid,
			   qidx_t __unused pidx, qidx_t __unused budget)
{
	return (0);
}

/**
 * ice_recovery_rxd_pkt_get - Called by iflib to send data to upper layer
 * @arg: device specific softc
 * @ri: receive packet info
 *
 * Since the Rx queues are not initialized during recovery mode this function
 * always returns an error indicating that nothing could be done.
 *
 * @returns ENOSYS
 */
static int
ice_recovery_rxd_pkt_get(void __unused *arg, if_rxd_info_t __unused ri)
{
	return (ENOSYS);
}

/**
 * ice_recovery_rxd_refill - Prepare Rx descriptors for re-use by hardware
 * @arg: device specific softc structure
 * @iru: the Rx descriptor update structure
 *
 * Since the Rx queues are not initialized during Recovery mode, this function
 * does nothing.
 */
static void
ice_recovery_rxd_refill(void __unused *arg, if_rxd_update_t __unused iru)
{
	;
}

/**
 * ice_recovery_rxd_flush - Flush Rx descriptors to hardware
 * @arg: device specific softc pointer
 * @rxqid: the Rx queue to flush
 * @flidx: unused parameter
 * @pidx: descriptor index to advance tail to
 *
 * Since the Rx queues are not initialized during Recovery mode, this function
 * does nothing.
 */
static void
ice_recovery_rxd_flush(void __unused *arg, uint16_t __unused rxqid,
		       uint8_t flidx __unused, qidx_t __unused pidx)
{
	;
}
