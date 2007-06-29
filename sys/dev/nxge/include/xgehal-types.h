/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 *  FileName :    xgehal-types.h
 *
 *  Description:  HAL commonly used types and enumerations
 *
 *  Created:      19 May 2004
 */

#ifndef XGE_HAL_TYPES_H
#define XGE_HAL_TYPES_H

#include <dev/nxge/include/xge-os-pal.h>

__EXTERN_BEGIN_DECLS

/*
 * BIT(loc) - set bit at offset
 */
#define BIT(loc)		(0x8000000000000000ULL >> (loc))

/*
 * vBIT(val, loc, sz) - set bits at offset
 */
#define vBIT(val, loc, sz)	(((u64)(val)) << (64-(loc)-(sz)))
#define vBIT32(val, loc, sz)	(((u32)(val)) << (32-(loc)-(sz)))

/*
 * bVALx(bits, loc) - Get the value of x bits at location
 */
#define bVAL1(bits, loc)	((((u64)bits) >> (64-(loc+1))) & 0x1)
#define bVAL2(bits, loc)	((((u64)bits) >> (64-(loc+2))) & 0x3)
#define bVAL3(bits, loc)	((((u64)bits) >> (64-(loc+3))) & 0x7)
#define bVAL4(bits, loc)	((((u64)bits) >> (64-(loc+4))) & 0xF)
#define bVAL5(bits, loc)	((((u64)bits) >> (64-(loc+5))) & 0x1F)
#define bVAL6(bits, loc)	((((u64)bits) >> (64-(loc+6))) & 0x3F)
#define bVAL7(bits, loc)	((((u64)bits) >> (64-(loc+7))) & 0x7F)
#define bVAL8(bits, loc)	((((u64)bits) >> (64-(loc+8))) & 0xFF)
#define bVAL12(bits, loc)	((((u64)bits) >> (64-(loc+12))) & 0xFFF)
#define bVAL14(bits, loc)	((((u64)bits) >> (64-(loc+14))) & 0x3FFF)
#define bVAL16(bits, loc)	((((u64)bits) >> (64-(loc+16))) & 0xFFFF)
#define bVAL20(bits, loc)	((((u64)bits) >> (64-(loc+20))) & 0xFFFFF)
#define bVAL22(bits, loc)	((((u64)bits) >> (64-(loc+22))) & 0x3FFFFF)
#define bVAL24(bits, loc)	((((u64)bits) >> (64-(loc+24))) & 0xFFFFFF)
#define bVAL28(bits, loc)	((((u64)bits) >> (64-(loc+28))) & 0xFFFFFFF)
#define bVAL32(bits, loc)	((((u64)bits) >> (64-(loc+32))) & 0xFFFFFFFF)
#define bVAL36(bits, loc)	((((u64)bits) >> (64-(loc+36))) & 0xFFFFFFFFF)
#define bVAL40(bits, loc)	((((u64)bits) >> (64-(loc+40))) & 0xFFFFFFFFFF)
#define bVAL44(bits, loc)	((((u64)bits) >> (64-(loc+44))) & 0xFFFFFFFFFFF)
#define bVAL48(bits, loc)	((((u64)bits) >> (64-(loc+48))) & 0xFFFFFFFFFFFF)
#define bVAL52(bits, loc)	((((u64)bits) >> (64-(loc+52))) & 0xFFFFFFFFFFFFF)
#define bVAL56(bits, loc)	((((u64)bits) >> (64-(loc+56))) & 0xFFFFFFFFFFFFFF)
#define bVAL60(bits, loc)	((((u64)bits) >> (64-(loc+60))) & 0xFFFFFFFFFFFFFFF)

#define XGE_HAL_BASE_INF		100
#define XGE_HAL_BASE_ERR		200
#define XGE_HAL_BASE_BADCFG	        300

#define XGE_HAL_ALL_FOXES   0xFFFFFFFFFFFFFFFFULL

/**
 * enum xge_hal_status_e - HAL return codes.
 * @XGE_HAL_OK: Success.
 * @XGE_HAL_FAIL: Failure.
 * @XGE_HAL_COMPLETIONS_REMAIN: There are more completions on a channel.
 *      (specific to polling mode completion processing).
 * @XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS: No more completed
 * descriptors. See xge_hal_fifo_dtr_next_completed().
 * @XGE_HAL_INF_OUT_OF_DESCRIPTORS: Out of descriptors. Channel
 * descriptors
 *           are reserved (via xge_hal_fifo_dtr_reserve(),
 *           xge_hal_fifo_dtr_reserve())
 *           and not yet freed (via xge_hal_fifo_dtr_free(),
 *           xge_hal_ring_dtr_free()).
 * @XGE_HAL_INF_CHANNEL_IS_NOT_READY: Channel is not ready for
 * operation.
 * @XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING: Indicates that host needs to
 * poll until PIO is executed.
 * @XGE_HAL_INF_STATS_IS_NOT_READY: Cannot retrieve statistics because
 * HAL and/or device is not yet initialized.
 * @XGE_HAL_INF_NO_MORE_FREED_DESCRIPTORS: No descriptors left to
 * reserve. Internal use only.
 * @XGE_HAL_INF_IRQ_POLLING_CONTINUE: Returned by the ULD channel
 * callback when instructed to exit descriptor processing loop
 * prematurely. Typical usage: polling mode of processing completed
 * descriptors.
 *           Upon getting LRO_ISED, ll driver shall
 *           1) initialise lro struct with mbuf if sg_num == 1.
 *           2) else it will update m_data_ptr_of_mbuf to tcp pointer and
 *           append the new mbuf to the tail of mbuf chain in lro struct.
 *
 * @XGE_HAL_INF_LRO_BEGIN: Returned by ULD LRO module, when new LRO is
 * being initiated.
 * @XGE_HAL_INF_LRO_CONT: Returned by ULD LRO module, when new frame
 * is appended at the end of existing LRO.
 * @XGE_HAL_INF_LRO_UNCAPABLE: Returned by ULD LRO module, when new
 * frame is not LRO capable.
 * @XGE_HAL_INF_LRO_END_1: Returned by ULD LRO module, when new frame
 * triggers LRO flush.
 * @XGE_HAL_INF_LRO_END_2: Returned by ULD LRO module, when new
 * frame triggers LRO flush. Lro frame should be flushed first then
 * new frame should be flushed next.
 * @XGE_HAL_INF_LRO_END_3: Returned by ULD LRO module, when new
 * frame triggers close of current LRO session and opening of new LRO session
 * with the frame.
 * @XGE_HAL_INF_LRO_SESSIONS_XCDED: Returned by ULD LRO module, when no
 * more LRO sessions can be added.
 * @XGE_HAL_INF_NOT_ENOUGH_HW_CQES: TBD
 * @XGE_HAL_ERR_DRIVER_NOT_INITIALIZED: HAL is not initialized.
 * @XGE_HAL_ERR_OUT_OF_MEMORY: Out of memory (example, when and
 * allocating descriptors).
 * @XGE_HAL_ERR_CHANNEL_NOT_FOUND: xge_hal_channel_open will return this
 * error if corresponding channel is not configured.
 * @XGE_HAL_ERR_WRONG_IRQ: Returned by HAL's ISR when the latter is
 * invoked not because of the Xframe-generated interrupt.
 * @XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES: Returned when user tries to
 * configure more than XGE_HAL_MAX_MAC_ADDRESSES  mac addresses.
 * @XGE_HAL_ERR_BAD_DEVICE_ID: Unknown device PCI ID.
 * @XGE_HAL_ERR_OUT_ALIGNED_FRAGS: Too many unaligned fragments
 * in a scatter-gather list.
 * @XGE_HAL_ERR_DEVICE_NOT_INITIALIZED: Device is not initialized.
 * Typically means wrong sequence of API calls.
 * @XGE_HAL_ERR_SWAPPER_CTRL: Error during device initialization: failed
 * to set Xframe byte swapper in accordnace with the host
 * endian-ness.
 * @XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT: Failed to restore the device to
 * a "quiescent" state.
 * @XGE_HAL_ERR_INVALID_MTU_SIZE: Returned when MTU size specified by
 * caller is not in the (64, 9600) range.
 * @XGE_HAL_ERR_OUT_OF_MAPPING: Failed to map DMA-able memory.
 * @XGE_HAL_ERR_BAD_SUBSYSTEM_ID: Bad PCI subsystem ID. (Currently we
 * check for zero/non-zero only.)
 * @XGE_HAL_ERR_INVALID_BAR_ID: Invalid BAR ID. Xframe supports two Base
 * Address Register Spaces: BAR0 (id=0) and BAR1 (id=1).
 * @XGE_HAL_ERR_INVALID_OFFSET: Invalid offset. Example, attempt to read
 * register value (with offset) outside of the BAR0 space.
 * @XGE_HAL_ERR_INVALID_DEVICE: Invalid device. The HAL device handle
 * (passed by ULD) is invalid.
 * @XGE_HAL_ERR_OUT_OF_SPACE: Out-of-provided-buffer-space. Returned by
 * management "get" routines when the retrieved information does
 * not fit into the provided buffer.
 * @XGE_HAL_ERR_INVALID_VALUE_BIT_SIZE: Invalid bit size.
 * @XGE_HAL_ERR_VERSION_CONFLICT: Upper-layer driver and HAL (versions)
 * are not compatible.
 * @XGE_HAL_ERR_INVALID_MAC_ADDRESS: Invalid MAC address.
 * @XGE_HAL_ERR_SPDM_NOT_ENABLED: SPDM support is not enabled.
 * @XGE_HAL_ERR_SPDM_TABLE_FULL: SPDM table is full.
 * @XGE_HAL_ERR_SPDM_INVALID_ENTRY: Invalid SPDM entry.
 * @XGE_HAL_ERR_SPDM_ENTRY_NOT_FOUND: Unable to locate the entry in the
 * SPDM table.
 * @XGE_HAL_ERR_SPDM_TABLE_DATA_INCONSISTENT: Local SPDM table is not in
 * synch ith the actual one.
 * @XGE_HAL_ERR_INVALID_PCI_INFO: Invalid or unrecognized PCI frequency,
 * and or width, and or mode (Xframe-II only, see UG on PCI_INFO register).
 * @XGE_HAL_ERR_CRITICAL: Critical error. Returned by HAL APIs
 * (including xge_hal_device_handle_tcode()) on: ECC, parity, SERR.
 * Also returned when PIO read does not go through ("all-foxes")
 * because of "slot-freeze".
 * @XGE_HAL_ERR_RESET_FAILED: Failed to soft-reset the device.
 * Returned by xge_hal_device_reset(). One circumstance when it could
 * happen: slot freeze by the system (see @XGE_HAL_ERR_CRITICAL).
 * @XGE_HAL_ERR_TOO_MANY: This error is returned if there were laready
 * maximum number of sessions or queues allocated
 * @XGE_HAL_ERR_PKT_DROP: TBD
 * @XGE_HAL_BADCFG_TX_URANGE_A: Invalid Tx link utilization range A. See
 * the structure xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_UFC_A: Invalid frame count for Tx link utilization
 * range A. See the structure xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_URANGE_B: Invalid Tx link utilization range B. See
 * the structure xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_UFC_B: Invalid frame count for Tx link utilization
 * range B. See the strucuture  xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_URANGE_C: Invalid Tx link utilization range C. See
 * the structure  xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_UFC_C: Invalid frame count for Tx link utilization
 * range C. See the structure xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_UFC_D: Invalid frame count for Tx link utilization
 * range D. See the structure  xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_TIMER_VAL: Invalid Tx timer value. See the
 * structure xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_TX_TIMER_CI_EN: Invalid Tx timer continuous interrupt
 * enable. See the structure xge_hal_tti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_URANGE_A: Invalid Rx link utilization range A. See
 * the structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_UFC_A: Invalid frame count for Rx link utilization
 * range A. See the structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_URANGE_B: Invalid Rx link utilization range B. See
 * the structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_UFC_B: Invalid frame count for Rx link utilization
 * range B. See the structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_URANGE_C: Invalid Rx link utilization range C. See
 * the structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_UFC_C: Invalid frame count for Rx link utilization
 * range C. See the structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_UFC_D: Invalid frame count for Rx link utilization
 * range D. See the structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RX_TIMER_VAL:  Invalid Rx timer value. See the
 * structure xge_hal_rti_config_t{} for valid values.
 * @XGE_HAL_BADCFG_FIFO_QUEUE_INITIAL_LENGTH: Invalid initial fifo queue
 * length. See the structure xge_hal_fifo_queue_t for valid values.
 * @XGE_HAL_BADCFG_FIFO_QUEUE_MAX_LENGTH: Invalid fifo queue max length.
 * See the structure xge_hal_fifo_queue_t for valid values.
 * @XGE_HAL_BADCFG_FIFO_QUEUE_INTR: Invalid fifo queue interrupt mode.
 * See the structure xge_hal_fifo_queue_t for valid values.
 * @XGE_HAL_BADCFG_RING_QUEUE_INITIAL_BLOCKS: Invalid Initial number of
 * RxD blocks for the ring. See the structure xge_hal_ring_queue_t for
 * valid values.
 * @XGE_HAL_BADCFG_RING_QUEUE_MAX_BLOCKS: Invalid maximum number of RxD
 * blocks for the ring. See the structure xge_hal_ring_queue_t for
 * valid values.
 * @XGE_HAL_BADCFG_RING_QUEUE_BUFFER_MODE: Invalid ring buffer mode. See
 * the structure xge_hal_ring_queue_t for valid values.
 * @XGE_HAL_BADCFG_RING_QUEUE_SIZE: Invalid ring queue size. See the
 * structure xge_hal_ring_queue_t for valid values.
 * @XGE_HAL_BADCFG_BACKOFF_INTERVAL_US: Invalid backoff timer interval
 * for the ring. See the structure xge_hal_ring_queue_t for valid values.
 * @XGE_HAL_BADCFG_MAX_FRM_LEN: Invalid ring max frame length. See the
 * structure xge_hal_ring_queue_t for valid values.
 * @XGE_HAL_BADCFG_RING_PRIORITY: Invalid ring priority. See the
 * structure xge_hal_ring_queue_t for valid values.
 * @XGE_HAL_BADCFG_TMAC_UTIL_PERIOD: Invalid tmac util period. See the
 * structure xge_hal_mac_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RMAC_UTIL_PERIOD: Invalid rmac util period. See the
 * structure xge_hal_mac_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RMAC_BCAST_EN: Invalid rmac brodcast enable. See the
 * structure xge_hal_mac_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RMAC_HIGH_PTIME: Invalid rmac pause time. See the
 * structure xge_hal_mac_config_t{} for valid values.
 * @XGE_HAL_BADCFG_MC_PAUSE_THRESHOLD_Q0Q3: Invalid threshold for pause
 * frame generation for queues 0 through 3. See the structure
 * xge_hal_mac_config_t{} for valid values.
 * @XGE_HAL_BADCFG_MC_PAUSE_THRESHOLD_Q4Q7:Invalid threshold for pause
 * frame generation for queues 4 through 7. See the structure
 * xge_hal_mac_config_t{} for valid values.
 * @XGE_HAL_BADCFG_FIFO_FRAGS: Invalid fifo max fragments length. See
 * the structure xge_hal_fifo_config_t{} for valid values.
 * @XGE_HAL_BADCFG_FIFO_RESERVE_THRESHOLD: Invalid fifo reserve
 * threshold. See the structure xge_hal_fifo_config_t{} for valid values.
 * @XGE_HAL_BADCFG_FIFO_MEMBLOCK_SIZE: Invalid fifo descriptors memblock
 * size. See the structure xge_hal_fifo_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RING_MEMBLOCK_SIZE: Invalid ring descriptors memblock
 * size. See the structure xge_hal_ring_config_t{} for valid values.
 * @XGE_HAL_BADCFG_MAX_MTU: Invalid max mtu for the device. See the
 * structure xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_ISR_POLLING_CNT: Invalid isr polling count. See the
 * structure xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_LATENCY_TIMER: Invalid Latency timer. See the
 * structure xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_MAX_SPLITS_TRANS: Invalid maximum  number of pci-x
 * split transactions. See the structure xge_hal_device_config_t{} for valid
 * values.
 * @XGE_HAL_BADCFG_MMRB_COUNT: Invalid mmrb count.  See the structure
 * xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_SHARED_SPLITS: Invalid number of outstanding split
 * transactions that is shared by Tx and Rx requests. See the structure
 * xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_STATS_REFRESH_TIME: Invalid time interval for
 * automatic statistics transfer to the host. See the structure
 * xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_PCI_FREQ_MHERZ:  Invalid pci clock frequency. See the
 * structure xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_PCI_MODE: Invalid pci mode. See the structure
 * xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_INTR_MODE: Invalid interrupt mode. See the structure
 * xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_SCHED_TIMER_US: Invalid scheduled timer interval to
 * generate interrupt. See the structure  xge_hal_device_config_t{}
 * for valid values.
 * @XGE_HAL_BADCFG_SCHED_TIMER_ON_SHOT: Invalid scheduled timer one
 * shot. See the structure xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_BADCFG_QUEUE_SIZE_INITIAL: Invalid driver queue initial
 * size. See the structure xge_hal_driver_config_t{} for valid values.
 * @XGE_HAL_BADCFG_QUEUE_SIZE_MAX: Invalid driver queue max size.  See
 * the structure xge_hal_driver_config_t{} for valid values.
 * @XGE_HAL_BADCFG_RING_RTH_EN: Invalid value of RTH-enable. See
 * the structure xge_hal_ring_queue_t for valid values.
 * @XGE_HAL_BADCFG_RING_INDICATE_MAX_PKTS: Invalid value configured for
 * indicate_max_pkts variable.
 * @XGE_HAL_BADCFG_TX_TIMER_AC_EN: Invalid value for Tx timer
 * auto-cancel. See xge_hal_tti_config_t{}.
 * @XGE_HAL_BADCFG_RX_TIMER_AC_EN: Invalid value for Rx timer
 * auto-cancel. See xge_hal_rti_config_t{}.
 * @XGE_HAL_BADCFG_RXUFCA_INTR_THRES: TODO
 * @XGE_HAL_BADCFG_RXUFCA_LO_LIM: TODO
 * @XGE_HAL_BADCFG_RXUFCA_HI_LIM: TODO
 * @XGE_HAL_BADCFG_RXUFCA_LBOLT_PERIOD: TODO
 * @XGE_HAL_BADCFG_TRACEBUF_SIZE: Bad configuration: the size of the circular
 * (in memory) trace buffer either too large or too small. See the
 * the corresponding header file or README for the acceptable range.
 * @XGE_HAL_BADCFG_LINK_VALID_CNT: Bad configuration: the link-valid
 * counter cannot have the specified value. Note that the link-valid
 * counting is done only at device-open time, to determine with the
 * specified certainty that the link is up. See the
 * the corresponding header file or README for the acceptable range.
 * See also @XGE_HAL_BADCFG_LINK_RETRY_CNT.
 * @XGE_HAL_BADCFG_LINK_RETRY_CNT: Bad configuration: the specified
 * link-up retry count is out of the valid range. Note that the link-up
 * retry counting is done only at device-open time.
 * See also xge_hal_device_config_t{}.
 * @XGE_HAL_BADCFG_LINK_STABILITY_PERIOD: Invalid link stability period.
 * @XGE_HAL_BADCFG_DEVICE_POLL_MILLIS: Invalid device poll interval.
 * @XGE_HAL_BADCFG_RMAC_PAUSE_GEN_EN: TBD
 * @XGE_HAL_BADCFG_RMAC_PAUSE_RCV_EN: TBD
 * @XGE_HAL_BADCFG_MEDIA: TBD
 * @XGE_HAL_BADCFG_NO_ISR_EVENTS: TBD
 * See the structure xge_hal_device_config_t{} for valid values.
 * @XGE_HAL_EOF_TRACE_BUF: End of the circular (in memory) trace buffer.
 * Returned by xge_hal_mgmt_trace_read(), when user tries to read the trace
 * past the buffer limits. Used to enable user to load the trace in two
 * or more reads.
 * @XGE_HAL_BADCFG_RING_RTS_MAC_EN: Invalid value of RTS_MAC_EN enable. See
 * the structure xge_hal_ring_queue_t for valid values.
 * @XGE_HAL_BADCFG_LRO_SG_SIZE : Invalid value of LRO scatter gatter size.
 * See the structure xge_hal_device_config_t for valid values.
 * @XGE_HAL_BADCFG_LRO_FRM_LEN : Invalid value of LRO frame length.
 * See the structure xge_hal_device_config_t for valid values.
 * @XGE_HAL_BADCFG_WQE_NUM_ODS: TBD
 * @XGE_HAL_BADCFG_BIMODAL_INTR: Invalid value to configure bimodal interrupts
 * Enumerates status and error codes returned by HAL public
 * API functions.
 * @XGE_HAL_BADCFG_BIMODAL_TIMER_LO_US: TBD
 * @XGE_HAL_BADCFG_BIMODAL_TIMER_HI_US: TBD
 * @XGE_HAL_BADCFG_BIMODAL_XENA_NOT_ALLOWED: TBD
 * @XGE_HAL_BADCFG_RTS_QOS_EN: TBD
 * @XGE_HAL_BADCFG_FIFO_QUEUE_INTR_VECTOR: TBD
 * @XGE_HAL_BADCFG_RING_QUEUE_INTR_VECTOR: TBD
 * @XGE_HAL_BADCFG_RTS_PORT_EN: TBD
 * @XGE_HAL_BADCFG_RING_RTS_PORT_EN: TBD
 *
 */
typedef enum xge_hal_status_e {
	XGE_HAL_OK				= 0,
	XGE_HAL_FAIL				= 1,
	XGE_HAL_COMPLETIONS_REMAIN		= 2,

	XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS = XGE_HAL_BASE_INF + 1,
	XGE_HAL_INF_OUT_OF_DESCRIPTORS		= XGE_HAL_BASE_INF + 2,
	XGE_HAL_INF_CHANNEL_IS_NOT_READY	= XGE_HAL_BASE_INF + 3,
	XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING	= XGE_HAL_BASE_INF + 4,
	XGE_HAL_INF_STATS_IS_NOT_READY		= XGE_HAL_BASE_INF + 5,
	XGE_HAL_INF_NO_MORE_FREED_DESCRIPTORS	= XGE_HAL_BASE_INF + 6,
	XGE_HAL_INF_IRQ_POLLING_CONTINUE	= XGE_HAL_BASE_INF + 7,
	XGE_HAL_INF_LRO_BEGIN			= XGE_HAL_BASE_INF + 8,
	XGE_HAL_INF_LRO_CONT			= XGE_HAL_BASE_INF + 9,
	XGE_HAL_INF_LRO_UNCAPABLE		= XGE_HAL_BASE_INF + 10,
	XGE_HAL_INF_LRO_END_1			= XGE_HAL_BASE_INF + 11,
	XGE_HAL_INF_LRO_END_2			= XGE_HAL_BASE_INF + 12,
	XGE_HAL_INF_LRO_END_3			= XGE_HAL_BASE_INF + 13,
	XGE_HAL_INF_LRO_SESSIONS_XCDED		= XGE_HAL_BASE_INF + 14,
	XGE_HAL_INF_NOT_ENOUGH_HW_CQES		= XGE_HAL_BASE_INF + 15,
	XGE_HAL_ERR_DRIVER_NOT_INITIALIZED	= XGE_HAL_BASE_ERR + 1,
	XGE_HAL_ERR_OUT_OF_MEMORY		= XGE_HAL_BASE_ERR + 4,
	XGE_HAL_ERR_CHANNEL_NOT_FOUND		= XGE_HAL_BASE_ERR + 5,
	XGE_HAL_ERR_WRONG_IRQ			= XGE_HAL_BASE_ERR + 6,
	XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES	= XGE_HAL_BASE_ERR + 7,
	XGE_HAL_ERR_SWAPPER_CTRL		= XGE_HAL_BASE_ERR + 8,
	XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT	= XGE_HAL_BASE_ERR + 9,
	XGE_HAL_ERR_INVALID_MTU_SIZE		= XGE_HAL_BASE_ERR + 10,
	XGE_HAL_ERR_OUT_OF_MAPPING		= XGE_HAL_BASE_ERR + 11,
	XGE_HAL_ERR_BAD_SUBSYSTEM_ID		= XGE_HAL_BASE_ERR + 12,
	XGE_HAL_ERR_INVALID_BAR_ID		= XGE_HAL_BASE_ERR + 13,
	XGE_HAL_ERR_INVALID_OFFSET		= XGE_HAL_BASE_ERR + 14,
	XGE_HAL_ERR_INVALID_DEVICE		= XGE_HAL_BASE_ERR + 15,
	XGE_HAL_ERR_OUT_OF_SPACE		= XGE_HAL_BASE_ERR + 16,
	XGE_HAL_ERR_INVALID_VALUE_BIT_SIZE	= XGE_HAL_BASE_ERR + 17,
	XGE_HAL_ERR_VERSION_CONFLICT		= XGE_HAL_BASE_ERR + 18,
	XGE_HAL_ERR_INVALID_MAC_ADDRESS		= XGE_HAL_BASE_ERR + 19,
	XGE_HAL_ERR_BAD_DEVICE_ID		= XGE_HAL_BASE_ERR + 20,
        XGE_HAL_ERR_OUT_ALIGNED_FRAGS           = XGE_HAL_BASE_ERR + 21,
	XGE_HAL_ERR_DEVICE_NOT_INITIALIZED	= XGE_HAL_BASE_ERR + 22,
	XGE_HAL_ERR_SPDM_NOT_ENABLED		= XGE_HAL_BASE_ERR + 23,
	XGE_HAL_ERR_SPDM_TABLE_FULL		= XGE_HAL_BASE_ERR + 24,
	XGE_HAL_ERR_SPDM_INVALID_ENTRY		= XGE_HAL_BASE_ERR + 25,
	XGE_HAL_ERR_SPDM_ENTRY_NOT_FOUND	= XGE_HAL_BASE_ERR + 26,
	XGE_HAL_ERR_SPDM_TABLE_DATA_INCONSISTENT= XGE_HAL_BASE_ERR + 27,
	XGE_HAL_ERR_INVALID_PCI_INFO		= XGE_HAL_BASE_ERR + 28,
	XGE_HAL_ERR_CRITICAL		        = XGE_HAL_BASE_ERR + 29,
	XGE_HAL_ERR_RESET_FAILED		= XGE_HAL_BASE_ERR + 30,
	XGE_HAL_ERR_TOO_MANY			= XGE_HAL_BASE_ERR + 32,
	XGE_HAL_ERR_PKT_DROP		        = XGE_HAL_BASE_ERR + 33,

	XGE_HAL_BADCFG_TX_URANGE_A		= XGE_HAL_BASE_BADCFG + 1,
	XGE_HAL_BADCFG_TX_UFC_A			= XGE_HAL_BASE_BADCFG + 2,
	XGE_HAL_BADCFG_TX_URANGE_B		= XGE_HAL_BASE_BADCFG + 3,
	XGE_HAL_BADCFG_TX_UFC_B			= XGE_HAL_BASE_BADCFG + 4,
	XGE_HAL_BADCFG_TX_URANGE_C		= XGE_HAL_BASE_BADCFG + 5,
	XGE_HAL_BADCFG_TX_UFC_C			= XGE_HAL_BASE_BADCFG + 6,
	XGE_HAL_BADCFG_TX_UFC_D			= XGE_HAL_BASE_BADCFG + 8,
	XGE_HAL_BADCFG_TX_TIMER_VAL		= XGE_HAL_BASE_BADCFG + 9,
	XGE_HAL_BADCFG_TX_TIMER_CI_EN		= XGE_HAL_BASE_BADCFG + 10,
	XGE_HAL_BADCFG_RX_URANGE_A		= XGE_HAL_BASE_BADCFG + 11,
	XGE_HAL_BADCFG_RX_UFC_A			= XGE_HAL_BASE_BADCFG + 12,
	XGE_HAL_BADCFG_RX_URANGE_B		= XGE_HAL_BASE_BADCFG + 13,
	XGE_HAL_BADCFG_RX_UFC_B			= XGE_HAL_BASE_BADCFG + 14,
	XGE_HAL_BADCFG_RX_URANGE_C		= XGE_HAL_BASE_BADCFG + 15,
	XGE_HAL_BADCFG_RX_UFC_C			= XGE_HAL_BASE_BADCFG + 16,
	XGE_HAL_BADCFG_RX_UFC_D			= XGE_HAL_BASE_BADCFG + 17,
	XGE_HAL_BADCFG_RX_TIMER_VAL		= XGE_HAL_BASE_BADCFG + 18,
	XGE_HAL_BADCFG_FIFO_QUEUE_INITIAL_LENGTH= XGE_HAL_BASE_BADCFG +	19,
	XGE_HAL_BADCFG_FIFO_QUEUE_MAX_LENGTH    = XGE_HAL_BASE_BADCFG + 20,
	XGE_HAL_BADCFG_FIFO_QUEUE_INTR		= XGE_HAL_BASE_BADCFG + 21,
	XGE_HAL_BADCFG_RING_QUEUE_INITIAL_BLOCKS=XGE_HAL_BASE_BADCFG +	22,
	XGE_HAL_BADCFG_RING_QUEUE_MAX_BLOCKS	= XGE_HAL_BASE_BADCFG +	23,
	XGE_HAL_BADCFG_RING_QUEUE_BUFFER_MODE	= XGE_HAL_BASE_BADCFG +	24,
	XGE_HAL_BADCFG_RING_QUEUE_SIZE		= XGE_HAL_BASE_BADCFG + 25,
	XGE_HAL_BADCFG_BACKOFF_INTERVAL_US	= XGE_HAL_BASE_BADCFG + 26,
	XGE_HAL_BADCFG_MAX_FRM_LEN		= XGE_HAL_BASE_BADCFG + 27,
	XGE_HAL_BADCFG_RING_PRIORITY		= XGE_HAL_BASE_BADCFG + 28,
	XGE_HAL_BADCFG_TMAC_UTIL_PERIOD		= XGE_HAL_BASE_BADCFG + 29,
	XGE_HAL_BADCFG_RMAC_UTIL_PERIOD		= XGE_HAL_BASE_BADCFG + 30,
	XGE_HAL_BADCFG_RMAC_BCAST_EN		= XGE_HAL_BASE_BADCFG + 31,
	XGE_HAL_BADCFG_RMAC_HIGH_PTIME		= XGE_HAL_BASE_BADCFG + 32,
	XGE_HAL_BADCFG_MC_PAUSE_THRESHOLD_Q0Q3	= XGE_HAL_BASE_BADCFG +33,
	XGE_HAL_BADCFG_MC_PAUSE_THRESHOLD_Q4Q7	= XGE_HAL_BASE_BADCFG +	34,
	XGE_HAL_BADCFG_FIFO_FRAGS		= XGE_HAL_BASE_BADCFG + 35,
	XGE_HAL_BADCFG_FIFO_RESERVE_THRESHOLD	= XGE_HAL_BASE_BADCFG +	37,
	XGE_HAL_BADCFG_FIFO_MEMBLOCK_SIZE	= XGE_HAL_BASE_BADCFG + 38,
	XGE_HAL_BADCFG_RING_MEMBLOCK_SIZE	= XGE_HAL_BASE_BADCFG +	39,
	XGE_HAL_BADCFG_MAX_MTU			= XGE_HAL_BASE_BADCFG + 40,
	XGE_HAL_BADCFG_ISR_POLLING_CNT		= XGE_HAL_BASE_BADCFG + 41,
	XGE_HAL_BADCFG_LATENCY_TIMER		= XGE_HAL_BASE_BADCFG + 42,
	XGE_HAL_BADCFG_MAX_SPLITS_TRANS		= XGE_HAL_BASE_BADCFG + 43,
	XGE_HAL_BADCFG_MMRB_COUNT		= XGE_HAL_BASE_BADCFG + 44,
	XGE_HAL_BADCFG_SHARED_SPLITS		= XGE_HAL_BASE_BADCFG + 45,
	XGE_HAL_BADCFG_STATS_REFRESH_TIME	= XGE_HAL_BASE_BADCFG +	46,
	XGE_HAL_BADCFG_PCI_FREQ_MHERZ		= XGE_HAL_BASE_BADCFG + 47,
	XGE_HAL_BADCFG_PCI_MODE			= XGE_HAL_BASE_BADCFG + 48,
	XGE_HAL_BADCFG_INTR_MODE		= XGE_HAL_BASE_BADCFG + 49,
	XGE_HAL_BADCFG_SCHED_TIMER_US		= XGE_HAL_BASE_BADCFG + 50,
	XGE_HAL_BADCFG_SCHED_TIMER_ON_SHOT	= XGE_HAL_BASE_BADCFG + 51,
	XGE_HAL_BADCFG_QUEUE_SIZE_INITIAL	= XGE_HAL_BASE_BADCFG + 52,
	XGE_HAL_BADCFG_QUEUE_SIZE_MAX		= XGE_HAL_BASE_BADCFG + 53,
	XGE_HAL_BADCFG_RING_RTH_EN		= XGE_HAL_BASE_BADCFG + 54,
	XGE_HAL_BADCFG_RING_INDICATE_MAX_PKTS	= XGE_HAL_BASE_BADCFG + 55,
	XGE_HAL_BADCFG_TX_TIMER_AC_EN		= XGE_HAL_BASE_BADCFG +	56,
	XGE_HAL_BADCFG_RX_TIMER_AC_EN		= XGE_HAL_BASE_BADCFG +	57,
	XGE_HAL_BADCFG_RXUFCA_INTR_THRES	= XGE_HAL_BASE_BADCFG + 58,
	XGE_HAL_BADCFG_RXUFCA_LO_LIM		= XGE_HAL_BASE_BADCFG + 59,
	XGE_HAL_BADCFG_RXUFCA_HI_LIM		= XGE_HAL_BASE_BADCFG + 60,
	XGE_HAL_BADCFG_RXUFCA_LBOLT_PERIOD	= XGE_HAL_BASE_BADCFG + 61,
	XGE_HAL_BADCFG_TRACEBUF_SIZE		= XGE_HAL_BASE_BADCFG + 62,
	XGE_HAL_BADCFG_LINK_VALID_CNT		= XGE_HAL_BASE_BADCFG + 63,
	XGE_HAL_BADCFG_LINK_RETRY_CNT		= XGE_HAL_BASE_BADCFG + 64,
	XGE_HAL_BADCFG_LINK_STABILITY_PERIOD	= XGE_HAL_BASE_BADCFG + 65,
	XGE_HAL_BADCFG_DEVICE_POLL_MILLIS       = XGE_HAL_BASE_BADCFG + 66,
	XGE_HAL_BADCFG_RMAC_PAUSE_GEN_EN	= XGE_HAL_BASE_BADCFG + 67,
	XGE_HAL_BADCFG_RMAC_PAUSE_RCV_EN	= XGE_HAL_BASE_BADCFG + 68,
	XGE_HAL_BADCFG_MEDIA			= XGE_HAL_BASE_BADCFG + 69,
	XGE_HAL_BADCFG_NO_ISR_EVENTS		= XGE_HAL_BASE_BADCFG + 70,
	XGE_HAL_BADCFG_RING_RTS_MAC_EN		= XGE_HAL_BASE_BADCFG + 71,
	XGE_HAL_BADCFG_LRO_SG_SIZE		= XGE_HAL_BASE_BADCFG + 72,
	XGE_HAL_BADCFG_LRO_FRM_LEN		= XGE_HAL_BASE_BADCFG + 73,
	XGE_HAL_BADCFG_WQE_NUM_ODS		= XGE_HAL_BASE_BADCFG + 74,
	XGE_HAL_BADCFG_BIMODAL_INTR		= XGE_HAL_BASE_BADCFG + 75,
	XGE_HAL_BADCFG_BIMODAL_TIMER_LO_US	= XGE_HAL_BASE_BADCFG + 76,
	XGE_HAL_BADCFG_BIMODAL_TIMER_HI_US	= XGE_HAL_BASE_BADCFG + 77,
	XGE_HAL_BADCFG_BIMODAL_XENA_NOT_ALLOWED	= XGE_HAL_BASE_BADCFG + 78,
	XGE_HAL_BADCFG_RTS_QOS_EN		= XGE_HAL_BASE_BADCFG + 79,
	XGE_HAL_BADCFG_FIFO_QUEUE_INTR_VECTOR	= XGE_HAL_BASE_BADCFG + 80,
	XGE_HAL_BADCFG_RING_QUEUE_INTR_VECTOR	= XGE_HAL_BASE_BADCFG + 81,
	XGE_HAL_BADCFG_RTS_PORT_EN		= XGE_HAL_BASE_BADCFG + 82,
	XGE_HAL_BADCFG_RING_RTS_PORT_EN		= XGE_HAL_BASE_BADCFG + 83,
	XGE_HAL_BADCFG_TRACEBUF_TIMESTAMP	= XGE_HAL_BASE_BADCFG + 84,
	XGE_HAL_EOF_TRACE_BUF			= -1
} xge_hal_status_e;

#define XGE_HAL_ETH_ALEN				6
typedef u8 macaddr_t[XGE_HAL_ETH_ALEN];

#define XGE_HAL_PCI_XFRAME_CONFIG_SPACE_SIZE		0x100

/* frames sizes */
#define XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE		14
#define XGE_HAL_HEADER_802_2_SIZE			3
#define XGE_HAL_HEADER_SNAP_SIZE			5
#define XGE_HAL_HEADER_VLAN_SIZE			4
#define XGE_HAL_MAC_HEADER_MAX_SIZE \
			(XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE + \
			 XGE_HAL_HEADER_802_2_SIZE + \
			 XGE_HAL_HEADER_SNAP_SIZE)

#define XGE_HAL_TCPIP_HEADER_MAX_SIZE			(64 + 64)

/* 32bit alignments */
#define XGE_HAL_HEADER_ETHERNET_II_802_3_ALIGN		2
#define XGE_HAL_HEADER_802_2_SNAP_ALIGN			2
#define XGE_HAL_HEADER_802_2_ALIGN			3
#define XGE_HAL_HEADER_SNAP_ALIGN			1

#define XGE_HAL_L3_CKSUM_OK				0xFFFF
#define XGE_HAL_L4_CKSUM_OK				0xFFFF
#define XGE_HAL_MIN_MTU					46
#define XGE_HAL_MAX_MTU					9600
#define XGE_HAL_DEFAULT_MTU				1500

#define XGE_HAL_SEGEMENT_OFFLOAD_MAX_SIZE	81920

#define XGE_HAL_PCISIZE_XENA			26 /* multiples of dword */
#define XGE_HAL_PCISIZE_HERC			64 /* multiples of dword */

#define XGE_HAL_MAX_MSIX_MESSAGES	64
#define XGE_HAL_MAX_MSIX_MESSAGES_WITH_ADDR XGE_HAL_MAX_MSIX_MESSAGES * 2
/*  Highest level interrupt blocks */
#define XGE_HAL_TX_PIC_INTR     (0x0001<<0)
#define XGE_HAL_TX_DMA_INTR     (0x0001<<1)
#define XGE_HAL_TX_MAC_INTR     (0x0001<<2)
#define XGE_HAL_TX_XGXS_INTR    (0x0001<<3)
#define XGE_HAL_TX_TRAFFIC_INTR (0x0001<<4)
#define XGE_HAL_RX_PIC_INTR     (0x0001<<5)
#define XGE_HAL_RX_DMA_INTR     (0x0001<<6)
#define XGE_HAL_RX_MAC_INTR     (0x0001<<7)
#define XGE_HAL_RX_XGXS_INTR    (0x0001<<8)
#define XGE_HAL_RX_TRAFFIC_INTR (0x0001<<9)
#define XGE_HAL_MC_INTR         (0x0001<<10)
#define XGE_HAL_SCHED_INTR      (0x0001<<11)
#define XGE_HAL_ALL_INTRS       (XGE_HAL_TX_PIC_INTR   | \
                               XGE_HAL_TX_DMA_INTR     | \
                               XGE_HAL_TX_MAC_INTR     | \
                               XGE_HAL_TX_XGXS_INTR    | \
                               XGE_HAL_TX_TRAFFIC_INTR | \
                               XGE_HAL_RX_PIC_INTR     | \
                               XGE_HAL_RX_DMA_INTR     | \
                               XGE_HAL_RX_MAC_INTR     | \
                               XGE_HAL_RX_XGXS_INTR    | \
                               XGE_HAL_RX_TRAFFIC_INTR | \
                               XGE_HAL_MC_INTR	       | \
			       XGE_HAL_SCHED_INTR)
#define XGE_HAL_GEN_MASK_INTR    (0x0001<<12)

/* Interrupt masks for the general interrupt mask register */
#define XGE_HAL_ALL_INTRS_DIS   0xFFFFFFFFFFFFFFFFULL

#define XGE_HAL_TXPIC_INT_M     BIT(0)
#define XGE_HAL_TXDMA_INT_M     BIT(1)
#define XGE_HAL_TXMAC_INT_M     BIT(2)
#define XGE_HAL_TXXGXS_INT_M    BIT(3)
#define XGE_HAL_TXTRAFFIC_INT_M BIT(8)
#define XGE_HAL_PIC_RX_INT_M    BIT(32)
#define XGE_HAL_RXDMA_INT_M     BIT(33)
#define XGE_HAL_RXMAC_INT_M     BIT(34)
#define XGE_HAL_MC_INT_M        BIT(35)
#define XGE_HAL_RXXGXS_INT_M    BIT(36)
#define XGE_HAL_RXTRAFFIC_INT_M BIT(40)

/* MSI level Interrupts */
#define XGE_HAL_MAX_MSIX_VECTORS	(16)

typedef struct xge_hal_ipv4 {
	u32 addr;
}xge_hal_ipv4;

typedef struct xge_hal_ipv6 {
	u64 addr[2];
}xge_hal_ipv6;

typedef union xge_hal_ipaddr_t {
	xge_hal_ipv4 ipv4;
	xge_hal_ipv6 ipv6;
}xge_hal_ipaddr_t;

/* DMA level Interrupts */
#define XGE_HAL_TXDMA_PFC_INT_M	BIT(0)

/*  PFC block interrupts */
#define XGE_HAL_PFC_MISC_ERR_1	BIT(0)   /* Interrupt to indicate FIFO
full */

/* basic handles */
typedef void* xge_hal_device_h;
typedef void* xge_hal_dtr_h;
typedef void* xge_hal_channel_h;
#ifdef XGEHAL_RNIC
typedef void* xge_hal_towi_h;
typedef void* xge_hal_hw_wqe_h;
typedef void* xge_hal_hw_cqe_h;
typedef void* xge_hal_lro_wqe_h;
typedef void* xge_hal_lro_cqe_h;
typedef void* xge_hal_up_msg_h;
typedef void* xge_hal_down_msg_h;
typedef void* xge_hal_channel_callback_fh;
typedef void* xge_hal_msg_queueh;
typedef void* xge_hal_pblist_h;
#endif
/*
 * I2C device id. Used in I2C control register for accessing EEPROM device
 * memory.
 */
#define XGE_DEV_ID		5

typedef enum xge_hal_xpak_alarm_type_e {
	XGE_HAL_XPAK_ALARM_EXCESS_TEMP = 1,
	XGE_HAL_XPAK_ALARM_EXCESS_BIAS_CURRENT = 2,
	XGE_HAL_XPAK_ALARM_EXCESS_LASER_OUTPUT = 3,
} xge_hal_xpak_alarm_type_e;


__EXTERN_END_DECLS

#endif /* XGE_HAL_TYPES_H */
