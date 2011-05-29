/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_STATUS_H
#define	VXGE_HAL_STATUS_H

__EXTERN_BEGIN_DECLS

#define	VXGE_HAL_EVENT_BASE				0
#define	VXGE_LL_EVENT_BASE				100

/*
 * enum vxge_hal_event_e - Enumerates slow-path HAL events.
 * @VXGE_HAL_EVENT_UNKNOWN: Unknown (and invalid) event.
 * @VXGE_HAL_EVENT_SERR: Serious hardware error event.
 * @VXGE_HAL_EVENT_CRITICAL: Critical vpath hardware error event.
 * @VXGE_HAL_EVENT_ECCERR: vpath ECC error event.
 * @VXGE_HAL_EVENT_KDFCCTL: FIFO Doorbell fifo error.
 * @VXGE_HAL_EVENT_SRPCIM_CRITICAL: srpcim hardware error event.
 * @VXGE_HAL_EVENT_MRPCIM_CRITICAL: mrpcim hardware error event.
 * @VXGE_HAL_EVENT_MRPCIM_ECCERR: mrpcim ecc error event.
 * @VXGE_HAL_EVENT_DEVICE_RESET_START: Privileged entity starting device reset
 * @VXGE_HAL_EVENT_DEVICE_RESET_COMPLETE: Device reset has been completed
 * @VXGE_HAL_EVENT_VPATH_RESET_START: A function is starting vpath reset
 * @VXGE_HAL_EVENT_VPATH_RESET_COMPLETE: vpath reset has been completed
 * @VXGE_HAL_EVENT_SLOT_FREEZE: Slot-freeze event. Driver tries to distinguish
 * slot-freeze from the rest critical events (e.g. ECC) when it is
 * impossible to PIO read "through" the bus, i.e. when getting all-foxes.
 *
 * vxge_hal_event_e enumerates slow-path HAL eventis.
 *
 * See also: vxge_hal_uld_cbs_t {}, vxge_uld_link_up_f {},
 * vxge_uld_link_down_f {}.
 */
typedef enum vxge_hal_event_e {
	VXGE_HAL_EVENT_UNKNOWN			= 0,
	/* HAL events */
	VXGE_HAL_EVENT_SERR			= VXGE_HAL_EVENT_BASE + 1,
	VXGE_HAL_EVENT_CRITICAL			= VXGE_HAL_EVENT_BASE + 2,
	VXGE_HAL_EVENT_ECCERR			= VXGE_HAL_EVENT_BASE + 3,
	VXGE_HAL_EVENT_KDFCCTL			= VXGE_HAL_EVENT_BASE + 4,
	VXGE_HAL_EVENT_SRPCIM_CRITICAL		= VXGE_HAL_EVENT_BASE + 5,
	VXGE_HAL_EVENT_MRPCIM_CRITICAL		= VXGE_HAL_EVENT_BASE + 6,
	VXGE_HAL_EVENT_MRPCIM_ECCERR		= VXGE_HAL_EVENT_BASE + 7,
	VXGE_HAL_EVENT_DEVICE_RESET_START	= VXGE_HAL_EVENT_BASE + 8,
	VXGE_HAL_EVENT_DEVICE_RESET_COMPLETE	= VXGE_HAL_EVENT_BASE + 9,
	VXGE_HAL_EVENT_VPATH_RESET_START	= VXGE_HAL_EVENT_BASE + 10,
	VXGE_HAL_EVENT_VPATH_RESET_COMPLETE	= VXGE_HAL_EVENT_BASE + 11,
	VXGE_HAL_EVENT_SLOT_FREEZE		= VXGE_HAL_EVENT_BASE + 12
} vxge_hal_event_e;

#define	VXGE_HAL_BASE_INF	100
#define	VXGE_HAL_BASE_ERR	200
#define	VXGE_HAL_BASE_BADCFG	300

/*
 * enum vxge_hal_status_e - HAL return codes.
 * @VXGE_HAL_OK: Success.
 * @VXGE_HAL_FAIL: Failure.
 * @VXGE_HAL_PENDING: Opearation is pending
 * @VXGE_HAL_CONTINUE: Continue processing
 * @VXGE_HAL_RETURN: Stop processing and return
 * @VXGE_HAL_COMPLETIONS_REMAIN: There are more completions on a channel.
 *	  (specific to polling mode completion processing).
 * @VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS: No more completed
 * descriptors. See vxge_hal_fifo_txdl_next_completed().
 * @VXGE_HAL_INF_OUT_OF_DESCRIPTORS: Out of descriptors. Channel
 * descriptors
 *	  are reserved (via vxge_hal_fifo_txdl_reserve(),
 *	  vxge_hal_ring_rxd_reserve())
 *	  and not yet freed (via vxge_hal_fifo_txdl_free(),
 *	  vxge_hal_ring_rxd_free()).
 * @VXGE_HAL_INF_QUEUE_IS_NOT_READY: A descriptor was reserved and not posted
 * @VXGE_HAL_INF_MEM_STROBE_CMD_EXECUTING: Indicates that host needs to
 * poll until PIO is executed.
 * @VXGE_HAL_INF_STATS_IS_NOT_READY: Queue is not ready for
 * operation.
 * @VXGE_HAL_INF_NO_MORE_FREED_DESCRIPTORS: No descriptors left to
 * reserve. Internal use only.
 * @VXGE_HAL_INF_IRQ_POLLING_CONTINUE: Returned by the ULD channel
 * callback when instructed to exit descriptor processing loop
 * prematurely. Typical usage: polling mode of processing completed
 * descriptors.
 *	  Upon getting LRO_ISED, ll driver shall
 *	  1) initialise lro struct with mbuf if sg_num == 1.
 *	  2) else it will update m_data_ptr_of_mbuf to tcp pointer and
 *	  append the new mbuf to the tail of mbuf chain in lro struct.
 * @VXGE_HAL_INF_SW_LRO_BEGIN: Returned by ULD LRO module, when new LRO is
 * being initiated.
 * @VXGE_HAL_INF_SW_LRO_CONT: Returned by ULD LRO module, when new frame
 * is appended at the end of existing LRO.
 * @VXGE_HAL_INF_SW_LRO_UNCAPABLE: Returned by ULD LRO module, when new
 * frame is not LRO capable.
 * @VXGE_HAL_INF_SW_LRO_FLUSH_SESSION: Returned by ULD LRO module,
 * when new frame triggers LRO flush.
 * @VXGE_HAL_INF_SW_LRO_FLUSH_BOTH: Returned by ULD LRO module, when new
 * frame triggers LRO flush. Lro frame should be flushed first then
 * new frame should be flushed next.
 * @VXGE_HAL_INF_SW_LRO_END_3: Returned by ULD LRO module, when new
 * frame triggers close of current LRO session and opening of new LRO session
 * with the frame.
 * @VXGE_HAL_INF_SW_LRO_SESSIONS_XCDED: Returned by ULD LRO module, when no
 * more LRO sessions can be added.
 * @VXGE_HAL_INF_NOT_ENOUGH_HW_CQES: Enough CQEs are available
 * @VXGE_HAL_INF_LINK_UP_DOWN: Link up down indication received
 * @VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED: HAL is not initialized.
 * @VXGE_HAL_ERR_INVALID_HANDLE: The handle passed is invalid.
 * @VXGE_HAL_ERR_OUT_OF_MEMORY: Out of memory (example, when and
 * allocating descriptors).
 * @VXGE_HAL_ERR_VPATH_NOT_AVAILABLE: Vpath is not allocated to this
 * function
 * @VXGE_HAL_ERR_VPATH_NOT_OPEN: Vpath is not opened i.e put in service.
 * @VXGE_HAL_ERR_WRONG_IRQ: Returned by HAL's ISR when the latter is
 * invoked not because of the X3100-generated interrupt.
 * @VXGE_HAL_ERR_OUT_OF_MAC_ADDRESSES: Returned when user tries to
 * configure more than VXGE_HAL_MAX_MAC_ADDRESSES  mac addresses.
 * @VXGE_HAL_ERR_SWAPPER_CTRL: Error during device initialization: failed
 * to set X3100 byte swapper in accordnace with the host
 * endian-ness.
 * @VXGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT: Failed to restore the device to
 * a "quiescent" state.
 * @VXGE_HAL_ERR_INVALID_MTU_SIZE:Returned when MTU size specified by
 * caller is not in the (64, 9600) range.
 * @VXGE_HAL_ERR_OUT_OF_MAPPING: Failed to map DMA-able memory.
 * @VXGE_HAL_ERR_BAD_SUBSYSTEM_ID: Bad PCI subsystem ID. (Currently we
 * check for zero/non-zero only.)
 * @VXGE_HAL_ERR_INVALID_BAR_ID: Invalid BAR ID. X3100 supports two Base
 * Address Register Spaces: BAR0 (id=0) and BAR1 (id=1).
 * @VXGE_HAL_ERR_INVALID_INDEX: Invalid index. Example, attempt to read
 * register value from the register section that is out of range.
 * @VXGE_HAL_ERR_INVALID_TYPE: Invalid register section.
 * @VXGE_HAL_ERR_INVALID_OFFSET: Invalid offset. Example, attempt to read
 * register value (with offset) outside of the register section range
 * @VXGE_HAL_ERR_INVALID_DEVICE: Invalid device. The HAL device handle
 * (passed by ULD) is invalid.
 * @VXGE_HAL_ERR_OUT_OF_SPACE: Out-of-provided-buffer-space. Returned by
 * management "get" routines when the retrieved information does
 * not fit into the provided buffer.
 * @VXGE_HAL_ERR_INVALID_VALUE_BIT_SIZE: Invalid bit size.
 * @VXGE_HAL_ERR_VERSION_CONFLICT: Upper-layer driver and HAL (versions)
 * are not compatible.
 * @VXGE_HAL_ERR_INVALID_MAC_ADDRESS: Invalid MAC address.
 * @VXGE_HAL_ERR_BAD_DEVICE_ID: Unknown device PCI ID.
 * @VXGE_HAL_ERR_OUT_ALIGNED_FRAGS: Too many unaligned fragments
 * in a scatter-gather list.
 * @VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED: Device is not initialized.
 * Typically means wrong sequence of API calls.
 * @VXGE_HAL_ERR_SPDM_NOT_ENABLED: SPDM support is not enabled.
 * @VXGE_HAL_ERR_SPDM_TABLE_FULL: SPDM table is full.
 * @VXGE_HAL_ERR_SPDM_INVALID_ENTRY: Invalid SPDM entry.
 * @VXGE_HAL_ERR_SPDM_ENTRY_NOT_FOUND: Unable to locate the entry in the
 * SPDM table.
 * @VXGE_HAL_ERR_SPDM_TABLE_DATA_INCONSISTENT: Local SPDM table is not in
 * synch ith the actual one.
 * @VXGE_HAL_ERR_INVALID_PCI_INFO: Invalid or unrecognized PCI  parameters.
 * @VXGE_HAL_ERR_CRITICAL: Critical error. Returned by HAL APIs
 * (including vxge_hal_fifo_handle_tcode() and vxge_hal_ring_handle_tcode())
 * on: ECC, parity, SERR.
 * Also returned when PIO read does not go through ("all-foxes")
 * because of "slot-freeze".
 * @VXGE_HAL_ERR_RESET_FAILED: Failed to soft-reset the device.
 * Returned by vxge_hal_device_reset(). One circumstance when it could
 * happen: slot freeze by the system (see @VXGE_HAL_ERR_CRITICAL).
 * @VXGE_HAL_ERR_TOO_MANY: This error is returned if there were laready
 * maximum number of sessions or queues allocated
 * @VXGE_HAL_ERR_PKT_DROP: Packet got dropped
 * @VXGE_HAL_ERR_INVALID_BLOCK_SIZE: Invalid block size
 * @VXGE_HAL_ERR_INVALID_STATE: Invalid state
 * @VXGE_HAL_ERR_PRIVILAGED_OPEARATION: A previleged operation is attempted
 * @VXGE_HAL_ERR_RESET_IN_PROGRESS: Reset is currently in progress
 * @VXGE_HAL_ERR_MAC_TABLE_FULL: DA table is full
 * @VXGE_HAL_ERR_MAC_TABLE_EMPTY: DA table is empty
 * @VXGE_HAL_ERR_MAC_TABLE_NO_MORE_ENTRIES: There are no more entries in the
 *		DA table
 * @VXGE_HAL_ERR_RTDMA_RTDMA_READY: RTDMA is ready
 * @VXGE_HAL_ERR_WRDMA_WRDMA_READY: WRDMA is ready
 * @VXGE_HAL_ERR_KDFC_KDFC_READY: Kernel mode doorbell controller ready
 * @VXGE_HAL_ERR_TPA_TMAC_BUF_EMPTY: Transmit Protocol Assist TMAC buffer empty
 * @VXGE_HAL_ERR_RDCTL_PIC_QUIESCENT: PIC block is quiescent
 * @VXGE_HAL_ERR_XGMAC_NETWORK_FAULT: Network Fault
 * @VXGE_HAL_ERR_ROCRC_OFFLOAD_QUIESCENT: ROCRC offload quiescent
 * @VXGE_HAL_ERR_G3IF_FB_G3IF_FB_GDDR3_READY: G3DDR Interface FB Ready
 * @VXGE_HAL_ERR_G3IF_CM_G3IF_CM_GDDR3_READY: G3DDR Interface CM Ready
 * @VXGE_HAL_ERR_RIC_RIC_RUNNING: Adapter RIC is still programming flash
 *		settings to device
 * @VXGE_HAL_ERR_CMG_C_PLL_IN_LOCK: CMG C PLL in lock
 * @VXGE_HAL_ERR_XGMAC_X_PLL_IN_LOCK: XGMAC X PLL in Lock
 * @VXGE_HAL_ERR_FBIF_M_PLL_IN_LOCK: FBUF M PLL in Lock
 * @VXGE_HAL_ERR_PCC_PCC_IDLE: PCC is idle
 * @VXGE_HAL_ERR_ROCRC_RC_PRC_QUIESCENT: ROCRC RC PCC quiescent
 * @VXGE_HAL_ERR_SLOT_FREEZE: PCI Slot frozen
 * @VXGE_HAL_ERR_INVALID_TCODE: The t-code returned is invalid
 * @VXGE_HAL_ERR_INVALID_PORT: The port number specified is invalid
 * @VXGE_HAL_ERR_INVALID_PRIORITY: Proiority specified is invalid
 * @VXGE_HAL_ERR_INVALID_MIN_BANDWIDTH: Minimum bandwidth specified is invalid
 * @VXGE_HAL_ERR_INVALID_MAX_BANDWIDTH: Maximum bandwidth specified is invalid
 * @VXGE_HAL_ERR_INVALID_BANDWIDTH_LIMIT: Bandwidth limit specified is invalid
 * @VXGE_HAL_ERR_INVALID_TOTAL_BANDWIDTH: Total bandwidth specified is invalid
 * @VXGE_HAL_ERR_MANAGER_NOT_FOUND: The Function 0 driver or MRPCIM manager is
 *		down
 * @VXGE_HAL_ERR_TIME_OUT: Timeout occurred
 * @VXGE_HAL_ERR_EVENT_UNKNOWN: Unknown alarm
 * @VXGE_HAL_ERR_EVENT_SERR: Serious error on device
 * @VXGE_HAL_ERR_EVENT_CRITICAL: Critical error in the vpath
 * @VXGE_HAL_ERR_EVENT_ECCERR: Ecc Error returned in t-code
 * @VXGE_HAL_ERR_EVENT_KDFCCTL: Kdfcctl error on the device
 * @VXGE_HAL_ERR_EVENT_SRPCIM_CRITICAL: Critical error in SRPCIM
 * @VXGE_HAL_ERR_EVENT_MRPCIM_CRITICAL: Critical error in MRPCIM
 * @VXGE_HAL_ERR_EVENT_MRPCIM_ECCERR: ECC error in MRPCIM
 * @VXGE_HAL_ERR_EVENT_RESET_START: Device is going to be reset
 * @VXGE_HAL_ERR_EVENT_RESET_COMPLETE: Device reset is complete
 * @VXGE_HAL_ERR_EVENT_SLOT_FREEZE: PCI Slot freeze
 * @VXGE_HAL_BADCFG_WIRE_PORT_PORT_ID: Invalid port id in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_MAX_MEDIA: Invalid media type in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_MAX_INITIAL_MTU: Invalid initial MTU size
 *		in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_AUTONEG_MODE: Invalid autonegotiation mode
 *		in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_AUTONEG_RATE: Invalid autonegotiation rate
 *		in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_FIXED_USE_FSM: Invalid fixed use fsm in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_ANTP_USE_FSM: Invalid ANTP use FSM in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_ANBE_USE_FSM: Invalid ANBE use FSM in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_LINK_STABILITY_PERIOD: Invalid link stability
 *		period in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_PORT_STABILITY_PERIOD: Invalid port stability
 *		period in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_TMAC_EN: Invalid Transmit MAC enable setting
 *		in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_EN: Invalid Receive MAC enable setting
 *		in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_TMAC_PAD: Invalid Transmit MAC PAD enable setting
 *		in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_TMAC_PAD_BYTE: Invalid Transmit MAC PAD Byte
 *		setting in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_TMAC_UTIL_PERIOD: Invalid Transmit MAC utilization
 *		period in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_STRIP_FCS: Invalid Receive MAC strip FCS
 *		setting in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PROM_EN: Invalid Receive MAC PROM enable
 *		in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_DISCARD_PFRM: Invalid Receive MAC discard
 *		pfrm setting in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_UTIL_PERIOD: Invalid Receive MAC utilization
 *		period in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_GEN_EN: Invalid Receive MAC pause
 *		generation enable setting in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_RCV_EN: Invalid Receive MAC pause
 *		receive enable setting in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_HIGH_PTIME: Invalid Receive MAC high ptime
 *		setting in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_LIMITER_EN: Invalid Receive MAC pause
 *		limitter enable setting in config
 * @VXGE_HAL_BADCFG_WIRE_PORT_RMAC_MAX_LIMIT: Invalid Receive MAC max limit
 *		setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_MAX_INITIAL_MTU: Invalid initial MTU size
 *		in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_EN: Invalid Transmit MAC enable setting
 *		in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_EN: Invalid Receive MAC enable setting
 *		in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_PAD: Invalid Transmit MAC PAD enable
 *		setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_PAD_BYTE: Invalid Transmit MAC PAD Byte
 *		setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_UTIL_PERIOD: Invalid Transmit MAC
 *		utilization period in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_STRIP_FCS: Invalid Receive MAC strip FCS
 *		setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PROM_EN: Invalid Receive MAC PROM enable
 *		in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_DISCARD_PFRM: Invalid Receive MAC discard
 *		pfrm setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_UTIL_PERIOD: Invalid Receive MAC
 *		utilization period in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_GEN_EN: Invalid Receive MAC
 *		pause generation enable setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_RCV_EN: Invalid Receive MAC
 *		pause receive enable setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_HIGH_PTIME: Invalid Receive MAC
 *		high ptime setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_LIMITER_EN: Invalid Receive MAC pause
 *		limitter enable setting in config
 * @VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_MAX_LIMIT: Invalid Receive MAC
 *		max limit setting in config
 * @VXGE_HAL_BADCFG_MAC_NETWORK_STABILITY_PERIOD: Invalid network
 *		stability period setting in config
 * @VXGE_HAL_BADCFG_MAC_MC_PAUSE_THRESHOLD: Invalid MC pause threshold
 *		setting in config
 * @VXGE_HAL_BADCFG_MAC_PERMA_STOP_EN: Invalid perma stop enable setting
 *		in config
 * @VXGE_HAL_BADCFG_MAC_TMAC_TX_SWITCH_DIS: Invalid Transmit MAC
 *		tx switch disable in config
 * @VXGE_HAL_BADCFG_MAC_TMAC_LOSSY_SWITCH_EN: Invalid Transmit MAC
 *		lossy switch enable in config
 * @VXGE_HAL_BADCFG_MAC_TMAC_LOSSY_WIRE_EN: Invalid Transmit MAC
 *		lossy wire enable in config
 * @VXGE_HAL_BADCFG_MAC_TMAC_BCAST_TO_WIRE_DIS: Invalid Transmit
 *		MAC broadcast to wire disable in config
 * @VXGE_HAL_BADCFG_MAC_TMAC_BCAST_TO_SWITCH_DIS: Invalid Transmit
 *		MAC broadcast to switch disable in config
 * @VXGE_HAL_BADCFG_MAC_TMAC_HOST_APPEND_FCS_EN: Invalid Transmit MAC
 *		host append fcs in config
 * @VXGE_HAL_BADCFG_MAC_TPA_SUPPORT_SNAP_AB_N: Invalid Transmit Protocol
 *		Assist support SNAP AB N setting in config
 * @VXGE_HAL_BADCFG_MAC_TPA_ECC_ENABLE_N: Invalid Transmit Protocol
 *		Assist ecc enable N setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_IGNORE_FRAME_ERR: Invalid Receive Protocol
 *		Assist ignore frame error in config
 * @VXGE_HAL_BADCFG_MAC_RPA_SNAP_AB_N: Invalid Receive Protocol Assist
 *		SNAP AB N in config
 * @VXGE_HAL_BADCFG_MAC_RPA_SEARCH_FOR_HAO: Invalid Receive Protocol
 *		Assist search for HAO in config
 * @VXGE_HAL_BADCFG_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS: Invalid Receive
 *		Protocol support ipv6 mobile headers in config
 * @VXGE_HAL_BADCFG_MAC_RPA_IPV6_STOP_SEARCHING: Invalid Receive Protocol
 *		Assist ipv6 stop searching in config
 * @VXGE_HAL_BADCFG_MAC_RPA_NO_PS_IF_UNKNOWN: Invalid Receive Protocol
 *		Assist no ps if unknown in config
 * @VXGE_HAL_BADCFG_MAC_RPA_SEARCH_FOR_ETYPE: Invalid Receive Protocol
 *		Assist search for etype in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_L4_COMP_CSUM: Invalid Receive Protocol
 *		Assist replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_L3_INCL_CF: Invalid Receive Protocol Assist
 *		replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_L3_COMP_CSUM: Invalid Receive Protocol Assist
 *		replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV4_TCP_INCL_PH: Invalid Receive Protocol
 *		Assist replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV6_TCP_INCL_PH: Invalid Receive Protocol
 *		Assist replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV4_UDP_INCL_PH: Invalid Receive Protocol
 *		Assist replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV6_UDP_INCL_PH: Invalid Receive Protocol
 *		Assist replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_L4_INCL_CF: Invalid Receive Protocol Assist
 *		replication setting in config
 * @VXGE_HAL_BADCFG_MAC_RPA_REPL_STRIP_VLAN_TAG: Invalid Receive Protocol
 *		Assist replication setting in config
 * @VXGE_HAL_BADCFG_LAG_LAG_EN: Invalid option for lag_en in config
 * @VXGE_HAL_BADCFG_LAG_LAG_MODE: Invalid option for lag_mode in config
 * @VXGE_HAL_BADCFG_LAG_TX_DISCARD: Invalid option for tx_discard in config
 * @VXGE_HAL_BADCFG_LAG_TX_AGGR_STATS: Invalid option for incr_tx_aggr_stats
 *		in config
 * @VXGE_HAL_BADCFG_LAG_DISTRIB_ALG_SEL: Invalid option for distrib_alg_sel
 *		in config
 * @VXGE_HAL_BADCFG_LAG_DISTRIB_REMAP_IF_FAIL: Invalid option for
 *		distrib_remap_if_fail in config
 * @VXGE_HAL_BADCFG_LAG_COLL_MAX_DELAY: Invalid Collector Max Delay in config
 * @VXGE_HAL_BADCFG_LAG_RX_DISCARD: Invalid option for rx_discard in config
 * @VXGE_HAL_BADCFG_LAG_PREF_INDIV_PORT: Invalid option for pref_indiv_port
 *		in config
 * @VXGE_HAL_BADCFG_LAG_HOT_STANDBY: Invalid option for hot_standby in config
 * @VXGE_HAL_BADCFG_LAG_LACP_DECIDES: Invalid option for lacp_decides in config
 * @VXGE_HAL_BADCFG_LAG_PREF_ACTIVE_PORT: Invalid option for pref_active_port
 *		in config
 * @VXGE_HAL_BADCFG_LAG_AUTO_FAILBACK: Invalid option for auto_failback
 *		in config
 * @VXGE_HAL_BADCFG_LAG_FAILBACK_EN: Invalid option for failback_en in config
 * @VXGE_HAL_BADCFG_LAG_COLD_FAILOVER_TIMEOUT: Invalid cold_failover_timeout
 *		in config
 * @VXGE_HAL_BADCFG_LAG_LACP_EN: Invalid option for lacp_en in config
 * @VXGE_HAL_BADCFG_LAG_LACP_BEGIN: Invalid option for lacp_begin in config
 * @VXGE_HAL_BADCFG_LAG_DISCARD_LACP: Invalid option for discard_lacp in config
 * @VXGE_HAL_BADCFG_LAG_LIBERAL_LEN_CHK: Invalid option for liberal_len_chk
 *		in config
 * @VXGE_HAL_BADCFG_LAG_MARKER_GEN_RECV_EN: Invalid option for
 *		marker_gen_recv_en in config
 * @VXGE_HAL_BADCFG_LAG_MARKER_RESP_EN: Invalid option for marker_resp_en
 *		in config
 * @VXGE_HAL_BADCFG_LAG_MARKER_RESP_TIMEOUT: Invalid option for
 *		marker_resp_timeout in config
 * @VXGE_HAL_BADCFG_LAG_SLOW_PROTO_MRKR_MIN_INTERVAL: Invalid option for
 *		slow_proto_mrkr_min_interval in config
 * @VXGE_HAL_BADCFG_LAG_THROTTLE_MRKR_RESP: Invalid option for
 *		throttle_mrkr_resp in config
 * @VXGE_HAL_BADCFG_LAG_SYS_PRI: Invalid system priority in config
 * @VXGE_HAL_BADCFG_LAG_USE_PORT_MAC_ADDR: Invalid option for
 *		use_port_mac_addr in config
 * @VXGE_HAL_BADCFG_LAG_MAC_ADDR_SEL: Invalid option for mac_addr_sel in config
 * @VXGE_HAL_BADCFG_LAG_ALT_ADMIN_KEY: Invalid alterneate admin key in config
 * @VXGE_HAL_BADCFG_LAG_ALT_AGGR: Invalid option for alt_aggr in config
 * @VXGE_HAL_BADCFG_LAG_FAST_PER_TIME: Invalid fast periodic time in config
 * @VXGE_HAL_BADCFG_LAG_SLOW_PER_TIME: Invalid slow periodic time in config
 * @VXGE_HAL_BADCFG_LAG_SHORT_TIMEOUT: Invalid short timeout in config
 * @VXGE_HAL_BADCFG_LAG_LONG_TIMEOUT: Invalid long timeout in config
 * @VXGE_HAL_BADCFG_LAG_CHURN_DET_TIME: Invalid churn detection time in config
 * @VXGE_HAL_BADCFG_LAG_AGGR_WAIT_TIME: Invalid Aggregator wait time in config
 * @VXGE_HAL_BADCFG_LAG_SHORT_TIMER_SCALE: Invalid short timer scale in config
 * @VXGE_HAL_BADCFG_LAG_LONG_TIMER_SCALE: Invalid long timer scale in config
 * @VXGE_HAL_BADCFG_LAG_AGGR_AGGR_ID: Invalid Aggregator Id in config
 * @VXGE_HAL_BADCFG_LAG_AGGR_USE_PORT_MAC_ADDR: Invalid option for
 *		use_port_mac_addr in config
 * @VXGE_HAL_BADCFG_LAG_AGGR_MAC_ADDR_SEL: Invalid option for mac_addr_sel
 *		in config
 * @VXGE_HAL_BADCFG_LAG_AGGR_ADMIN_KEY: Invalid admin key in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PORT_ID: Invalid port id in config
 * @VXGE_HAL_BADCFG_LAG_PORT_LAG_EN: Invalid option for lag_en in config
 * @VXGE_HAL_BADCFG_LAG_PORT_DISCARD_SLOW_PROTO: Invalid option for
 *		discard_slow_proto in config
 * @VXGE_HAL_BADCFG_LAG_PORT_HOST_CHOSEN_AGGR: Invalid option for
 *		host_chosen_aggr in config
 * @VXGE_HAL_BADCFG_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO: Invalid option
 *		for discard unknown slow proto in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_PORT_NUM: Invalid Actor port number in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_PORT_PRIORITY: Invalid Actor port priority
 *		in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_KEY_10G: Invalid Actor 10G key in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_KEY_1G: Invalid Actor 1G key in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_LACP_ACTIVITY: Invalid option for
 *		actor_lacp_activity in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_LACP_TIMEOUT: Invalid option for
 *		actor_lacp_timeout in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_AGGREGATION: Invalid option for
 *		actor_aggregation in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_SYNCHRONIZATION: Invalid option
 *		for actor_synchronization in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_COLLECTING: Invalid option for
 *		actor_collecting in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_DISTRIBUTING: Invalid option for
 *		actor_distributing in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_DEFAULTED: Invalid option for
 *		actor_defaulted in config
 * @VXGE_HAL_BADCFG_LAG_PORT_ACTOR_EXPIRED: Invalid option for
 *		actor_expired in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_SYS_PRI: Invalid option for
 *		partner_sys_pri in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_KEY: Invalid option for
 *		partner_key in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_NUM: Invalid option for
 *		partner_port_num in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_PORT_PRIORITY: Invalid option for
 *		partner_port_pri in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_LACP_ACTIVITY: Invalid option for
 *		partner_lacp_activity in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_LACP_TIMEOUT: Invalid option for
 *		partner_lacp_timeout in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_AGGREGATION: Invalid option for
 *		partner_aggregation in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_SYNCHRONIZATION: Invalid option for
 *		partner_synchronization in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_COLLECTING: Invalid option for
 *		partner_collecting in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_DISTRIBUTING: Invalid option for
 *		partner_distributing in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_DEFAULTED: Invalid option for
 *		partner_defaulted in config
 * @VXGE_HAL_BADCFG_LAG_PORT_PARTNER_EXPIRED: Invalid option for
 *		partner_expired in config
 * @VXGE_HAL_BADCFG_VPATH_QOS_PRIORITY: Invalid vpath priority
 * @VXGE_HAL_BADCFG_VPATH_QOS_MIN_BANDWIDTH: Invalid minimum bandwidth
 * @VXGE_HAL_BADCFG_VPATH_QOS_MAX_BANDWIDTH: Invalid maximum bandwidth
 * @VXGE_HAL_BADCFG_LOG_LEVEL: Invalid option for partner_mac_addr in config
 * @VXGE_HAL_BADCFG_RING_ENABLE: Invalid option for ring enable in config
 * @VXGE_HAL_BADCFG_RING_LENGTH: Invalid ring length in config in config
 * @VXGE_HAL_BADCFG_RING_RXD_BUFFER_MODE: Invalid receive buffer mode in config
 * @VXGE_HAL_BADCFG_RING_SCATTER_MODE: Invalid scatter mode setting in config
 * @VXGE_HAL_BADCFG_RING_POST_MODE: Invalid post mode setting in config
 * @VXGE_HAL_BADCFG_RING_MAX_FRM_LEN: Invalid max frame length setting in config
 * @VXGE_HAL_BADCFG_RING_NO_SNOOP_ALL: Invalid no snoop all setting in config
 * @VXGE_HAL_BADCFG_RING_TIMER_VAL: Invalid timer value setting in config
 * @VXGE_HAL_BADCFG_RING_GREEDY_RETURN: Invalid grredy return setting in config
 * @VXGE_HAL_BADCFG_RING_TIMER_CI: Invalid timer ci setting in config
 * @VXGE_HAL_BADCFG_RING_BACKOFF_INTERVAL_US: Invalid backoff interval
 *		in microseconds setting in config
 * @VXGE_HAL_BADCFG_RING_INDICATE_MAX_PKTS: Invalid indicate maximum packets
 *		setting in config
 * @VXGE_HAL_BADCFG_FIFO_ENABLE: Invalid option for FIFO enable in config
 * @VXGE_HAL_BADCFG_FIFO_LENGTH: Invalid FIFO length in config
 * @VXGE_HAL_BADCFG_FIFO_FRAGS: Invalid number of transmit frame fragments
 *		in config
 * @VXGE_HAL_BADCFG_FIFO_ALIGNMENT_SIZE: Invalid alignment size in config
 * @VXGE_HAL_BADCFG_FIFO_MAX_FRAGS: Invalid maximum number of transmit frame
 *		fragments in config
 * @VXGE_HAL_BADCFG_FIFO_QUEUE_INTR: Invalid FIFO queue interrupt setting
 *		in config
 * @VXGE_HAL_BADCFG_FIFO_NO_SNOOP_ALL: Invalid FIFO no snoop all setting
 *		in config
 * @VXGE_HAL_BADCFG_DMQ_LENGTH: Invalid DMQ length setting in config
 * @VXGE_HAL_BADCFG_DMQ_IMMED_EN: Invalid DMQ immediate enable setting in config
 * @VXGE_HAL_BADCFG_DMQ_EVENT_EN: Invalid DMQ event enable setting in config
 * @VXGE_HAL_BADCFG_DMQ_INTR_CTRL: Invalid DMQ interrupt control setting
 *		in config
 * @VXGE_HAL_BADCFG_DMQ_GEN_COMPL: Invalid DMQ general completion setting
 *		in config
 * @VXGE_HAL_BADCFG_UMQ_LENGTH: Invalid UMQ length setting in config
 * @VXGE_HAL_BADCFG_UMQ_IMMED_EN: Invalid UMQ immediate enable setting in config
 * @VXGE_HAL_BADCFG_UMQ_EVENT_EN: Invalid UMQ event enable setting in config
 * @VXGE_HAL_BADCFG_UMQ_INTR_CTRL: Invalid UMQ interrupt control setting
 *		in config
 * @VXGE_HAL_BADCFG_UMQ_GEN_COMPL: Invalid UMQ general completion setting
 *		in config
 * @VXGE_HAL_BADCFG_SW_LRO_SESSIONS: Invalid number of SW LRO sessions
 *		setting in config
 * @VXGE_HAL_BADCFG_SW_LRO_SG_SIZE: Invalid SW LRO Segment size
 * @VXGE_HAL_BADCFG_SW_LRO_FRM_LEN: Invalid SW LRO Frame Length
 * @VXGE_HAL_BADCFG_SW_LRO_MODE: Invalid SW LRO mode setting in config
 * @VXGE_HAL_BADCFG_LRO_SESSIONS_MAX: Invalid maximum number of LRO sessions
 *		setting in config
 * @VXGE_HAL_BADCFG_LRO_SESSIONS_THRESHOLD: Invalid sessions number threshold
 *		setting in config
 * @VXGE_HAL_BADCFG_LRO_SESSIONS_TIMEOUT: Invalid sessions timeout setting
 *		in config
 * @VXGE_HAL_BADCFG_LRO_NO_WQE_THRESHOLD: Invalid lower limit for number
 *		of WQEs in config
 * @VXGE_HAL_BADCFG_LRO_DUPACK_DETECTION: Invalid option for
 *		dupack_detection_enabled in config
 * @VXGE_HAL_BADCFG_LRO_DATA_MERGING: Invalid option for
 *		data_merging_enabled in config
 * @VXGE_HAL_BADCFG_LRO_ACK_MERGING: Invalid option for
 *		ack_merging_enabled in config
 * @VXGE_HAL_BADCFG_LRO_LLC_HDR_MODE: Invalid LLC Header Mode
 * @VXGE_HAL_BADCFG_LRO_SNAP_HDR_MODE: Invalid SNAP Header Mode
 * @VXGE_HAL_BADCFG_LRO_SESSION_ECN: Invalid option for session_ecn_enabled
 * @VXGE_HAL_BADCFG_LRO_SESSION_ECN_NONCE: Invalid option for
 *		session_ecn_enabled_nonce
 * @VXGE_HAL_BADCFG_LRO_RXD_BUFFER_MODE: Invalid buffer mode
 * @VXGE_HAL_BADCFG_LRO_SCATTER_MODE: Invalid scatter mode
 * @VXGE_HAL_BADCFG_LRO_IP_DATAGRAM_SIZE: Invalid IP Datagram size
 * @VXGE_HAL_BADCFG_LRO_FRAME_THRESHOLD: Invalid Frame Threshold
 * @VXGE_HAL_BADCFG_LRO_PSH_THRESHOLD: Invalid push Threshold
 * @VXGE_HAL_BADCFG_LRO_MTU_THRESHOLD: Invalid MTU Threshold
 * @VXGE_HAL_BADCFG_LRO_MSS_THRESHOLD: Invalid MSS Threshold
 * @VXGE_HAL_BADCFG_LRO_TCP_TSVAL_DELTA: Invalid TXP TSVAL DELTA
 * @VXGE_HAL_BADCFG_LRO_ACK_NBR_DELTA: Invalid Acknowledgement delta
 * @VXGE_HAL_BADCFG_LRO_SPARE_WQE_CAPACITY: Invalid Spare WQE Capacity
 * @VXGE_HAL_BADCFG_TIM_INTR_ENABLE: Invalid TIM interrupt enable setting
 *		in config
 * @VXGE_HAL_BADCFG_TIM_BTIMER_VAL: Invalid TIM btimer value setting in config
 * @VXGE_HAL_BADCFG_TIM_TIMER_AC_EN: Invalid TIM timer ac enable setting
 *		in config
 * @VXGE_HAL_BADCFG_TIM_TIMER_CI_EN: Invalid Tx timer continuous interrupt
 * enable. See the structure vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_TIM_TIMER_RI_EN: Invalid TIM timer ri enable setting
 *		in config
 * @VXGE_HAL_BADCFG_TIM_BTIMER_EVENT_SF: Invalid TIM btimer event sf seting
 *		in config
 * @VXGE_HAL_BADCFG_TIM_RTIMER_VAL: Invalid TIM rtimer setting in config
 * @VXGE_HAL_BADCFG_TIM_UTIL_SEL: Invalid TIM utilization setting in config
 * @VXGE_HAL_BADCFG_TIM_LTIMER_VAL: Invalid TIM ltimer value setting in config
 * @VXGE_HAL_BADCFG_TXFRM_CNT_EN: Invalid transmit frame count enable in config
 * @VXGE_HAL_BADCFG_TXD_CNT_EN: Invalid transmit count enable in config
 * @VXGE_HAL_BADCFG_TIM_URANGE_A: Invalid link utilization range A. See
 * the structure vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_TIM_UEC_A: Invalid frame count for link utilization
 * range A. See the structure vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_TIM_URANGE_B: Invalid link utilization range B. See
 * the structure vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_TIM_UEC_B: Invalid frame count for link utilization
 * range B. See the strucuture  vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_TIM_URANGE_C: Invalid link utilization range C. See
 * the structure  vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_TIM_UEC_C: Invalid frame count for link utilization
 * range C. See the structure vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_TIM_UEC_D: Invalid frame count for link utilization
 * range D. See the structure  vxge_hal_tim_intr_config_t {} for valid values.
 * @VXGE_HAL_BADCFG_VPATH_ID: Invalid vpath id in config
 * @VXGE_HAL_BADCFG_VPATH_WIRE_PORT: Invalid wire port to be used
 * @VXGE_HAL_BADCFG_VPATH_NO_SNOOP: Invalid vpath no snoop setting in config
 * @VXGE_HAL_BADCFG_VPATH_MTU: Invalid vpath mtu size setting in config
 * @VXGE_HAL_BADCFG_VPATH_TPA_LSOV2_EN: Invalid vpath transmit protocol assist
 *		lso v2 en setting in config
 * @VXGE_HAL_BADCFG_VPATH_TPA_IGNORE_FRAME_ERROR: Invalid vpath transmit
 *		protocol assist ignore frame error setting in config
 * @VXGE_HAL_BADCFG_VPATH_TPA_IPV6_KEEP_SEARCHING: Invalid vpath transmit
 *		protocol assist ipv6 keep searching setting in config
 * @VXGE_HAL_BADCFG_VPATH_TPA_L4_PSHDR_PRESENT: Invalid vpath transmit protocol
 *		assist L4 pseudo header present setting in config
 * @VXGE_HAL_BADCFG_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS: Invalid vpath transmit
 *		protocol assist support mobile ipv6 headers setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_IPV4_TCP_INCL_PH: Invalid vpath receive protocol
 *		assist ipv4 tcp include pseudo header setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_IPV6_TCP_INCL_PH: Invalid vpath receive protocol
 *		assist ipv6 tcp include pseudo header setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_IPV4_UDP_INCL_PH: Invalid vpath receive protocol
 *		assist ipv4 udp include pseudo header setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_IPV6_UDP_INCL_PH: Invalid vpath receive protocol
 *		assist ipv6 udp include pseudo header setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_L4_INCL_CF: Invalid vpath receive protocol assist
 *		layer 4 include cf setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_STRIP_VLAN_TAG: Invalid vpath receive protocol
 *		assist strip vlan tag setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_L4_COMP_CSUM: Invalid vpath receive protocol
 *		assist layer 4 compute check sum setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_L3_INCL_CF: Invalid vpath receive protocol
 *		assist layer 3 include cf setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_L3_COMP_CSUM: Invalid vpath receive protocol
 *		assist layer 3 compute check sum setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_UCAST_ALL_ADDR_EN: Invalid vpath receive protocol
 *		assist unicast all address enable setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_MCAST_ALL_ADDR_EN: Invalid vpath receive protocol
 *		assist multi-icast all address enable setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_CAST_EN: Invalid vpath receive protocol assist
 *		cast enable setting in config
 * @VXGE_HAL_BADCFG_VPATH_RPA_ALL_VID_EN: Invalid vpath receive protocol
 *		assist all vlan ids enable setting in config
 * @VXGE_HAL_BADCFG_VPATH_VP_Q_L2_FLOW: Invalid Q l2 flow setting in config
 * @VXGE_HAL_BADCFG_VPATH_VP_STATS_READ_METHOD: Invalid Stats read method
 * @VXGE_HAL_BADCFG_VPATH_BANDWIDTH_LIMIT: Invalid bandwidth limit
 * @VXGE_HAL_BADCFG_BLOCKPOOL_MIN: Invalid minimum number of block pool blocks
 *		setting in config
 * @VXGE_HAL_BADCFG_BLOCKPOOL_INITIAL: Invalid initial number of block pool
 *		blocks setting in config
 * @VXGE_HAL_BADCFG_BLOCKPOOL_INCR: Invalid number of block pool blocks
 *		increment setting in config
 * @VXGE_HAL_BADCFG_BLOCKPOOL_MAX: Invalid maximum number of block pool
 *		blocks setting in config
 * @VXGE_HAL_BADCFG_ISR_POLLING_CNT: Invalid isr polling count setting in config
 * @VXGE_HAL_BADCFG_MAX_PAYLOAD_SIZE: Invalid maximum payload size setting
 *		in config
 * @VXGE_HAL_BADCFG_MMRB_COUNT: Invalid mmrb count setting in config
 * @VXGE_HAL_BADCFG_STATS_REFRESH_TIME: Invalid stats refresh time setting
 *		in config
 * @VXGE_HAL_BADCFG_DUMP_ON_SERR: Invalid dump on serr setting in config
 * @VXGE_HAL_BADCFG_DUMP_ON_CRITICAL: Invalid dump on critical error setting
 *		config
 * @VXGE_HAL_BADCFG_DUMP_ON_ECCERR: Invalid dump on ecc error setting config
 * @VXGE_HAL_BADCFG_DUMP_ON_UNKNOWN: Invalid dump on unknown alarm setting
 *		config
 * @VXGE_HAL_BADCFG_INTR_MODE: Invalid interrupt mode setting in config
 * @VXGE_HAL_BADCFG_RTH_EN: Invalid rth enable setting in config
 * @VXGE_HAL_BADCFG_RTH_IT_TYPE: Invalid rth it type setting in config
 * @VXGE_HAL_BADCFG_UFCA_INTR_THRES: Invalid rxufca interrupt threshold
 *		setting in config
 * @VXGE_HAL_BADCFG_UFCA_LO_LIM: Invalid rxufca low limit setting in config
 * @VXGE_HAL_BADCFG_UFCA_HI_LIM: Invalid rxufca high limit setting in config
 * @VXGE_HAL_BADCFG_UFCA_LBOLT_PERIOD: Invalid rxufca lbolt period in config
 * @VXGE_HAL_BADCFG_DEVICE_POLL_MILLIS: Invalid device poll timeout
 *		in milliseconds setting in config
 * @VXGE_HAL_BADCFG_RTS_MAC_EN: Invalid rts mac enable setting in config
 * @VXGE_HAL_BADCFG_RTS_QOS_EN: Invalid rts qos enable setting in config
 * @VXGE_HAL_BADCFG_RTS_PORT_EN: Invalid rts port enable setting in config
 * @VXGE_HAL_BADCFG_MAX_CQE_GROUPS: Invalid maximum number of CQE groups
 *		in config
 * @VXGE_HAL_BADCFG_MAX_NUM_OD_GROUPS: Invalid maximum number of OD groups
 *		in config
 * @VXGE_HAL_BADCFG_NO_WQE_THRESHOLD: Invalid no wqe threshold setting
 *		in config
 * @VXGE_HAL_BADCFG_REFILL_THRESHOLD_HIGH: Invalid refill threshold setting
 *		in config
 * @VXGE_HAL_BADCFG_REFILL_THRESHOLD_LOW: Invalid refill threshold setting
 *		in config
 * @VXGE_HAL_BADCFG_ACK_BLOCK_LIMIT: Invalid acknowledgement block setting
 *		in config
 * @VXGE_HAL_BADCFG_STATS_READ_METHOD: Invalid stats read method
 * @VXGE_HAL_BADCFG_POLL_OR_DOOR_BELL: Invalid poll or doorbell setting
 *		in config
 * @VXGE_HAL_BADCFG_MSIX_ID: Invalid MSIX Id
 * @VXGE_HAL_EOF_TRACE_BUF: Invalid end of trace buffer setting in config
 *
 */
typedef enum vxge_hal_status_e {
	VXGE_HAL_OK				 = 0,
	VXGE_HAL_FAIL				 = 1,
	VXGE_HAL_PENDING			 = 2,
	VXGE_HAL_CONTINUE			 = 3,
	VXGE_HAL_RETURN				 = 4,
	VXGE_HAL_COMPLETIONS_REMAIN		 = 5,
	VXGE_HAL_TRAFFIC_INTERRUPT		 = 6,

	VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS = VXGE_HAL_BASE_INF + 1,
	VXGE_HAL_INF_OUT_OF_DESCRIPTORS		 = VXGE_HAL_BASE_INF + 2,
	VXGE_HAL_INF_QUEUE_IS_NOT_READY		 = VXGE_HAL_BASE_INF + 4,
	VXGE_HAL_INF_MEM_STROBE_CMD_EXECUTING	 = VXGE_HAL_BASE_INF + 5,
	VXGE_HAL_INF_STATS_IS_NOT_READY		 = VXGE_HAL_BASE_INF + 6,
	VXGE_HAL_INF_NO_MORE_FREED_DESCRIPTORS	 = VXGE_HAL_BASE_INF + 7,
	VXGE_HAL_INF_IRQ_POLLING_CONTINUE	 = VXGE_HAL_BASE_INF + 8,
	VXGE_HAL_INF_SW_LRO_BEGIN		 = VXGE_HAL_BASE_INF + 9,
	VXGE_HAL_INF_SW_LRO_CONT		 = VXGE_HAL_BASE_INF + 10,
	VXGE_HAL_INF_SW_LRO_UNCAPABLE		 = VXGE_HAL_BASE_INF + 11,
	VXGE_HAL_INF_SW_LRO_FLUSH_SESSION	 = VXGE_HAL_BASE_INF + 12,
	VXGE_HAL_INF_SW_LRO_FLUSH_BOTH		 = VXGE_HAL_BASE_INF + 13,
	VXGE_HAL_INF_SW_LRO_END_3		 = VXGE_HAL_BASE_INF + 14,
	VXGE_HAL_INF_SW_LRO_SESSIONS_XCDED	 = VXGE_HAL_BASE_INF + 15,
	VXGE_HAL_INF_NOT_ENOUGH_HW_CQES		 = VXGE_HAL_BASE_INF + 16,
	VXGE_HAL_INF_LINK_UP_DOWN		 = VXGE_HAL_BASE_INF + 17,

	VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED	 = VXGE_HAL_BASE_ERR + 1,
	VXGE_HAL_ERR_INVALID_HANDLE		 = VXGE_HAL_BASE_ERR + 2,
	VXGE_HAL_ERR_OUT_OF_MEMORY		 = VXGE_HAL_BASE_ERR + 3,
	VXGE_HAL_ERR_VPATH_NOT_AVAILABLE	 = VXGE_HAL_BASE_ERR + 4,
	VXGE_HAL_ERR_VPATH_NOT_OPEN		 = VXGE_HAL_BASE_ERR + 5,
	VXGE_HAL_ERR_WRONG_IRQ			 = VXGE_HAL_BASE_ERR + 6,
	VXGE_HAL_ERR_OUT_OF_MAC_ADDRESSES	 = VXGE_HAL_BASE_ERR + 7,
	VXGE_HAL_ERR_SWAPPER_CTRL		 = VXGE_HAL_BASE_ERR + 8,
	VXGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT	 = VXGE_HAL_BASE_ERR + 9,
	VXGE_HAL_ERR_INVALID_MTU_SIZE		 = VXGE_HAL_BASE_ERR + 10,
	VXGE_HAL_ERR_OUT_OF_MAPPING		 = VXGE_HAL_BASE_ERR + 11,
	VXGE_HAL_ERR_BAD_SUBSYSTEM_ID		 = VXGE_HAL_BASE_ERR + 12,
	VXGE_HAL_ERR_INVALID_BAR_ID		 = VXGE_HAL_BASE_ERR + 13,
	VXGE_HAL_ERR_INVALID_INDEX		 = VXGE_HAL_BASE_ERR + 14,
	VXGE_HAL_ERR_INVALID_TYPE		 = VXGE_HAL_BASE_ERR + 15,
	VXGE_HAL_ERR_INVALID_OFFSET		 = VXGE_HAL_BASE_ERR + 16,
	VXGE_HAL_ERR_INVALID_DEVICE		 = VXGE_HAL_BASE_ERR + 17,
	VXGE_HAL_ERR_OUT_OF_SPACE		 = VXGE_HAL_BASE_ERR + 18,
	VXGE_HAL_ERR_INVALID_VALUE_BIT_SIZE	 = VXGE_HAL_BASE_ERR + 19,
	VXGE_HAL_ERR_VERSION_CONFLICT		 = VXGE_HAL_BASE_ERR + 20,
	VXGE_HAL_ERR_INVALID_MAC_ADDRESS	 = VXGE_HAL_BASE_ERR + 21,
	VXGE_HAL_ERR_BAD_DEVICE_ID		 = VXGE_HAL_BASE_ERR + 22,
	VXGE_HAL_ERR_OUT_ALIGNED_FRAGS		 = VXGE_HAL_BASE_ERR + 23,
	VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED	 = VXGE_HAL_BASE_ERR + 24,
	VXGE_HAL_ERR_SPDM_NOT_ENABLED		 = VXGE_HAL_BASE_ERR + 25,
	VXGE_HAL_ERR_SPDM_TABLE_FULL		 = VXGE_HAL_BASE_ERR + 26,
	VXGE_HAL_ERR_SPDM_INVALID_ENTRY		 = VXGE_HAL_BASE_ERR + 27,
	VXGE_HAL_ERR_SPDM_ENTRY_NOT_FOUND	 = VXGE_HAL_BASE_ERR + 28,
	VXGE_HAL_ERR_SPDM_TABLE_DATA_INCONSISTENT = VXGE_HAL_BASE_ERR + 29,
	VXGE_HAL_ERR_INVALID_PCI_INFO		 = VXGE_HAL_BASE_ERR + 30,
	VXGE_HAL_ERR_CRITICAL			 = VXGE_HAL_BASE_ERR + 31,
	VXGE_HAL_ERR_RESET_FAILED		 = VXGE_HAL_BASE_ERR + 32,
	VXGE_HAL_ERR_TOO_MANY			 = VXGE_HAL_BASE_ERR + 33,
	VXGE_HAL_ERR_PKT_DROP			 = VXGE_HAL_BASE_ERR + 34,
	VXGE_HAL_ERR_INVALID_BLOCK_SIZE		 = VXGE_HAL_BASE_ERR + 35,
	VXGE_HAL_ERR_INVALID_STATE		 = VXGE_HAL_BASE_ERR + 36,
	VXGE_HAL_ERR_PRIVILAGED_OPEARATION	 = VXGE_HAL_BASE_ERR + 37,
	VXGE_HAL_ERR_RESET_IN_PROGRESS		 = VXGE_HAL_BASE_ERR + 38,
	VXGE_HAL_ERR_MAC_TABLE_FULL		 = VXGE_HAL_BASE_ERR + 39,
	VXGE_HAL_ERR_MAC_TABLE_EMPTY		 = VXGE_HAL_BASE_ERR + 40,
	VXGE_HAL_ERR_MAC_TABLE_NO_MORE_ENTRIES	 = VXGE_HAL_BASE_ERR + 41,
	VXGE_HAL_ERR_RTDMA_RTDMA_READY		 = VXGE_HAL_BASE_ERR + 42,
	VXGE_HAL_ERR_WRDMA_WRDMA_READY		 = VXGE_HAL_BASE_ERR + 43,
	VXGE_HAL_ERR_KDFC_KDFC_READY		 = VXGE_HAL_BASE_ERR + 44,
	VXGE_HAL_ERR_TPA_TMAC_BUF_EMPTY		 = VXGE_HAL_BASE_ERR + 45,
	VXGE_HAL_ERR_RDCTL_PIC_QUIESCENT	 = VXGE_HAL_BASE_ERR + 46,
	VXGE_HAL_ERR_XGMAC_NETWORK_FAULT	 = VXGE_HAL_BASE_ERR + 47,
	VXGE_HAL_ERR_ROCRC_OFFLOAD_QUIESCENT	 = VXGE_HAL_BASE_ERR + 48,
	VXGE_HAL_ERR_G3IF_FB_G3IF_FB_GDDR3_READY  = VXGE_HAL_BASE_ERR + 49,
	VXGE_HAL_ERR_G3IF_CM_G3IF_CM_GDDR3_READY  = VXGE_HAL_BASE_ERR + 50,
	VXGE_HAL_ERR_RIC_RIC_RUNNING		 = VXGE_HAL_BASE_ERR + 51,
	VXGE_HAL_ERR_CMG_C_PLL_IN_LOCK		 = VXGE_HAL_BASE_ERR + 52,
	VXGE_HAL_ERR_XGMAC_X_PLL_IN_LOCK	 = VXGE_HAL_BASE_ERR + 53,
	VXGE_HAL_ERR_FBIF_M_PLL_IN_LOCK		 = VXGE_HAL_BASE_ERR + 54,
	VXGE_HAL_ERR_PCC_PCC_IDLE		 = VXGE_HAL_BASE_ERR + 55,
	VXGE_HAL_ERR_ROCRC_RC_PRC_QUIESCENT	 = VXGE_HAL_BASE_ERR + 56,
	VXGE_HAL_ERR_SLOT_FREEZE		 = VXGE_HAL_BASE_ERR + 57,
	VXGE_HAL_ERR_INVALID_TCODE		 = VXGE_HAL_BASE_ERR + 58,
	VXGE_HAL_ERR_INVALID_PORT		 = VXGE_HAL_BASE_ERR + 59,
	VXGE_HAL_ERR_INVALID_WIRE_PORT		 = VXGE_HAL_BASE_ERR + 60,
	VXGE_HAL_ERR_INVALID_PRIORITY		 = VXGE_HAL_BASE_ERR + 61,
	VXGE_HAL_ERR_INVALID_MIN_BANDWIDTH	 = VXGE_HAL_BASE_ERR + 62,
	VXGE_HAL_ERR_INVALID_MAX_BANDWIDTH	 = VXGE_HAL_BASE_ERR + 63,
	VXGE_HAL_ERR_INVALID_BANDWIDTH_LIMIT	 = VXGE_HAL_BASE_ERR + 64,
	VXGE_HAL_ERR_INVALID_TOTAL_BANDWIDTH	 = VXGE_HAL_BASE_ERR + 65,
	VXGE_HAL_ERR_MANAGER_NOT_FOUND		 = VXGE_HAL_BASE_ERR + 66,
	VXGE_HAL_ERR_TIME_OUT			 = VXGE_HAL_BASE_ERR + 67,
	VXGE_HAL_ERR_EVENT_UNKNOWN		 = VXGE_HAL_BASE_ERR + 68,
	VXGE_HAL_ERR_EVENT_SERR			 = VXGE_HAL_BASE_ERR + 69,
	VXGE_HAL_ERR_EVENT_CRITICAL		 = VXGE_HAL_BASE_ERR + 70,
	VXGE_HAL_ERR_EVENT_ECCERR		 = VXGE_HAL_BASE_ERR + 71,
	VXGE_HAL_ERR_EVENT_KDFCCTL		 = VXGE_HAL_BASE_ERR + 72,
	VXGE_HAL_ERR_EVENT_SRPCIM_CRITICAL	 = VXGE_HAL_BASE_ERR + 73,
	VXGE_HAL_ERR_EVENT_MRPCIM_CRITICAL	 = VXGE_HAL_BASE_ERR + 74,
	VXGE_HAL_ERR_EVENT_MRPCIM_ECCERR	 = VXGE_HAL_BASE_ERR + 75,
	VXGE_HAL_ERR_EVENT_RESET_START		 = VXGE_HAL_BASE_ERR + 76,
	VXGE_HAL_ERR_EVENT_RESET_COMPLETE	 = VXGE_HAL_BASE_ERR + 77,
	VXGE_HAL_ERR_EVENT_SLOT_FREEZE		 = VXGE_HAL_BASE_ERR + 78,
	VXGE_HAL_ERR_INVALID_DP_MODE		 = VXGE_HAL_BASE_ERR + 79,
	VXGE_HAL_ERR_INVALID_L2_SWITCH_STATE	 = VXGE_HAL_BASE_ERR + 79,

	VXGE_HAL_BADCFG_WIRE_PORT_PORT_ID	 = VXGE_HAL_BASE_BADCFG + 1,
	VXGE_HAL_BADCFG_WIRE_PORT_MAX_MEDIA	 = VXGE_HAL_BASE_BADCFG + 2,
	VXGE_HAL_BADCFG_WIRE_PORT_MAX_INITIAL_MTU = VXGE_HAL_BASE_BADCFG + 3,
	VXGE_HAL_BADCFG_WIRE_PORT_AUTONEG_MODE	 = VXGE_HAL_BASE_BADCFG + 4,
	VXGE_HAL_BADCFG_WIRE_PORT_AUTONEG_RATE	 = VXGE_HAL_BASE_BADCFG + 5,
	VXGE_HAL_BADCFG_WIRE_PORT_FIXED_USE_FSM	 = VXGE_HAL_BASE_BADCFG + 6,
	VXGE_HAL_BADCFG_WIRE_PORT_ANTP_USE_FSM	 = VXGE_HAL_BASE_BADCFG + 7,
	VXGE_HAL_BADCFG_WIRE_PORT_ANBE_USE_FSM	 = VXGE_HAL_BASE_BADCFG + 8,
	VXGE_HAL_BADCFG_WIRE_PORT_LINK_STABILITY_PERIOD =
						    VXGE_HAL_BASE_BADCFG + 9,
	VXGE_HAL_BADCFG_WIRE_PORT_PORT_STABILITY_PERIOD =
						    VXGE_HAL_BASE_BADCFG + 10,
	VXGE_HAL_BADCFG_WIRE_PORT_TMAC_EN	 = VXGE_HAL_BASE_BADCFG + 11,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_EN	 = VXGE_HAL_BASE_BADCFG + 12,
	VXGE_HAL_BADCFG_WIRE_PORT_TMAC_PAD	 = VXGE_HAL_BASE_BADCFG + 13,
	VXGE_HAL_BADCFG_WIRE_PORT_TMAC_PAD_BYTE	 = VXGE_HAL_BASE_BADCFG + 14,
	VXGE_HAL_BADCFG_WIRE_PORT_TMAC_UTIL_PERIOD = VXGE_HAL_BASE_BADCFG + 15,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_STRIP_FCS  = VXGE_HAL_BASE_BADCFG + 16,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PROM_EN	 = VXGE_HAL_BASE_BADCFG + 18,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_DISCARD_PFRM = VXGE_HAL_BASE_BADCFG + 19,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_UTIL_PERIOD = VXGE_HAL_BASE_BADCFG + 20,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_GEN_EN = VXGE_HAL_BASE_BADCFG + 21,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_RCV_EN = VXGE_HAL_BASE_BADCFG + 22,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_HIGH_PTIME = VXGE_HAL_BASE_BADCFG + 23,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_LIMITER_EN =
						    VXGE_HAL_BASE_BADCFG + 24,
	VXGE_HAL_BADCFG_WIRE_PORT_RMAC_MAX_LIMIT  = VXGE_HAL_BASE_BADCFG + 25,
	VXGE_HAL_BADCFG_SWITCH_PORT_MAX_INITIAL_MTU = VXGE_HAL_BASE_BADCFG + 26,
	VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_EN	 = VXGE_HAL_BASE_BADCFG + 27,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_EN	 = VXGE_HAL_BASE_BADCFG + 28,
	VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_PAD	 = VXGE_HAL_BASE_BADCFG + 29,
	VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_PAD_BYTE  = VXGE_HAL_BASE_BADCFG + 30,
	VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_UTIL_PERIOD =
						    VXGE_HAL_BASE_BADCFG + 31,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_STRIP_FCS  = VXGE_HAL_BASE_BADCFG + 32,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PROM_EN  = VXGE_HAL_BASE_BADCFG + 33,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_DISCARD_PFRM =
						    VXGE_HAL_BASE_BADCFG + 34,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_UTIL_PERIOD =
						    VXGE_HAL_BASE_BADCFG + 35,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_GEN_EN =
						    VXGE_HAL_BASE_BADCFG + 36,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_RCV_EN =
						    VXGE_HAL_BASE_BADCFG + 37,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_HIGH_PTIME = VXGE_HAL_BASE_BADCFG + 38,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_LIMITER_EN =
						    VXGE_HAL_BASE_BADCFG + 39,
	VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_MAX_LIMIT  = VXGE_HAL_BASE_BADCFG + 40,
	VXGE_HAL_BADCFG_MAC_NETWORK_STABILITY_PERIOD =
						    VXGE_HAL_BASE_BADCFG + 41,
	VXGE_HAL_BADCFG_MAC_MC_PAUSE_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 42,
	VXGE_HAL_BADCFG_MAC_PERMA_STOP_EN	 = VXGE_HAL_BASE_BADCFG + 43,
	VXGE_HAL_BADCFG_MAC_TMAC_TX_SWITCH_DIS	 = VXGE_HAL_BASE_BADCFG + 44,
	VXGE_HAL_BADCFG_MAC_TMAC_LOSSY_SWITCH_EN  = VXGE_HAL_BASE_BADCFG + 45,
	VXGE_HAL_BADCFG_MAC_TMAC_LOSSY_WIRE_EN	 = VXGE_HAL_BASE_BADCFG + 46,
	VXGE_HAL_BADCFG_MAC_TMAC_BCAST_TO_WIRE_DIS = VXGE_HAL_BASE_BADCFG + 47,
	VXGE_HAL_BADCFG_MAC_TMAC_BCAST_TO_SWITCH_DIS =
						    VXGE_HAL_BASE_BADCFG + 48,
	VXGE_HAL_BADCFG_MAC_TMAC_HOST_APPEND_FCS_EN = VXGE_HAL_BASE_BADCFG + 49,
	VXGE_HAL_BADCFG_MAC_TPA_SUPPORT_SNAP_AB_N  = VXGE_HAL_BASE_BADCFG + 50,
	VXGE_HAL_BADCFG_MAC_TPA_ECC_ENABLE_N	 = VXGE_HAL_BASE_BADCFG + 51,
	VXGE_HAL_BADCFG_MAC_RPA_IGNORE_FRAME_ERR  = VXGE_HAL_BASE_BADCFG + 52,
	VXGE_HAL_BADCFG_MAC_RPA_SNAP_AB_N	 = VXGE_HAL_BASE_BADCFG + 53,
	VXGE_HAL_BADCFG_MAC_RPA_SEARCH_FOR_HAO	 = VXGE_HAL_BASE_BADCFG + 54,
	VXGE_HAL_BADCFG_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS =
						    VXGE_HAL_BASE_BADCFG + 55,
	VXGE_HAL_BADCFG_MAC_RPA_IPV6_STOP_SEARCHING = VXGE_HAL_BASE_BADCFG + 56,
	VXGE_HAL_BADCFG_MAC_RPA_NO_PS_IF_UNKNOWN  = VXGE_HAL_BASE_BADCFG + 57,
	VXGE_HAL_BADCFG_MAC_RPA_SEARCH_FOR_ETYPE  = VXGE_HAL_BASE_BADCFG + 58,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_L4_COMP_CSUM  = VXGE_HAL_BASE_BADCFG + 59,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_L3_INCL_CF	 = VXGE_HAL_BASE_BADCFG + 60,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_L3_COMP_CSUM  = VXGE_HAL_BASE_BADCFG + 61,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV4_TCP_INCL_PH =
						    VXGE_HAL_BASE_BADCFG + 62,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV6_TCP_INCL_PH =
						    VXGE_HAL_BASE_BADCFG + 63,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV4_UDP_INCL_PH =
						    VXGE_HAL_BASE_BADCFG + 64,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV6_UDP_INCL_PH =
						    VXGE_HAL_BASE_BADCFG + 65,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_L4_INCL_CF	= VXGE_HAL_BASE_BADCFG + 66,
	VXGE_HAL_BADCFG_MAC_RPA_REPL_STRIP_VLAN_TAG = VXGE_HAL_BASE_BADCFG + 67,

	VXGE_HAL_BADCFG_LAG_LAG_EN		 = VXGE_HAL_BASE_BADCFG + 101,
	VXGE_HAL_BADCFG_LAG_LAG_MODE		 = VXGE_HAL_BASE_BADCFG + 102,
	VXGE_HAL_BADCFG_LAG_TX_DISCARD		 = VXGE_HAL_BASE_BADCFG + 103,
	VXGE_HAL_BADCFG_LAG_TX_AGGR_STATS	 = VXGE_HAL_BASE_BADCFG + 104,
	VXGE_HAL_BADCFG_LAG_DISTRIB_ALG_SEL	 = VXGE_HAL_BASE_BADCFG + 105,
	VXGE_HAL_BADCFG_LAG_DISTRIB_REMAP_IF_FAIL = VXGE_HAL_BASE_BADCFG + 106,
	VXGE_HAL_BADCFG_LAG_COLL_MAX_DELAY	= VXGE_HAL_BASE_BADCFG + 107,
	VXGE_HAL_BADCFG_LAG_RX_DISCARD		 = VXGE_HAL_BASE_BADCFG + 108,
	VXGE_HAL_BADCFG_LAG_PREF_INDIV_PORT	 = VXGE_HAL_BASE_BADCFG + 109,
	VXGE_HAL_BADCFG_LAG_HOT_STANDBY		 = VXGE_HAL_BASE_BADCFG + 110,
	VXGE_HAL_BADCFG_LAG_LACP_DECIDES	 = VXGE_HAL_BASE_BADCFG + 111,
	VXGE_HAL_BADCFG_LAG_PREF_ACTIVE_PORT	 = VXGE_HAL_BASE_BADCFG + 112,
	VXGE_HAL_BADCFG_LAG_AUTO_FAILBACK	 = VXGE_HAL_BASE_BADCFG + 113,
	VXGE_HAL_BADCFG_LAG_FAILBACK_EN		 = VXGE_HAL_BASE_BADCFG + 114,
	VXGE_HAL_BADCFG_LAG_COLD_FAILOVER_TIMEOUT = VXGE_HAL_BASE_BADCFG + 115,
	VXGE_HAL_BADCFG_LAG_LACP_EN		 = VXGE_HAL_BASE_BADCFG + 116,
	VXGE_HAL_BADCFG_LAG_LACP_BEGIN		 = VXGE_HAL_BASE_BADCFG + 117,
	VXGE_HAL_BADCFG_LAG_DISCARD_LACP	 = VXGE_HAL_BASE_BADCFG + 118,
	VXGE_HAL_BADCFG_LAG_LIBERAL_LEN_CHK	 = VXGE_HAL_BASE_BADCFG + 119,
	VXGE_HAL_BADCFG_LAG_MARKER_GEN_RECV_EN	 = VXGE_HAL_BASE_BADCFG + 120,
	VXGE_HAL_BADCFG_LAG_MARKER_RESP_EN	 = VXGE_HAL_BASE_BADCFG + 121,
	VXGE_HAL_BADCFG_LAG_MARKER_RESP_TIMEOUT	 = VXGE_HAL_BASE_BADCFG + 122,
	VXGE_HAL_BADCFG_LAG_SLOW_PROTO_MRKR_MIN_INTERVAL =
						VXGE_HAL_BASE_BADCFG + 123,
	VXGE_HAL_BADCFG_LAG_THROTTLE_MRKR_RESP	 = VXGE_HAL_BASE_BADCFG + 124,
	VXGE_HAL_BADCFG_LAG_SYS_PRI		 = VXGE_HAL_BASE_BADCFG + 125,
	VXGE_HAL_BADCFG_LAG_USE_PORT_MAC_ADDR	 = VXGE_HAL_BASE_BADCFG + 126,
	VXGE_HAL_BADCFG_LAG_MAC_ADDR_SEL	 = VXGE_HAL_BASE_BADCFG + 127,
	VXGE_HAL_BADCFG_LAG_ALT_ADMIN_KEY	 = VXGE_HAL_BASE_BADCFG + 128,
	VXGE_HAL_BADCFG_LAG_ALT_AGGR		 = VXGE_HAL_BASE_BADCFG + 129,
	VXGE_HAL_BADCFG_LAG_FAST_PER_TIME	 = VXGE_HAL_BASE_BADCFG + 130,
	VXGE_HAL_BADCFG_LAG_SLOW_PER_TIME	 = VXGE_HAL_BASE_BADCFG + 131,
	VXGE_HAL_BADCFG_LAG_SHORT_TIMEOUT	 = VXGE_HAL_BASE_BADCFG + 132,
	VXGE_HAL_BADCFG_LAG_LONG_TIMEOUT	 = VXGE_HAL_BASE_BADCFG + 133,
	VXGE_HAL_BADCFG_LAG_CHURN_DET_TIME	 = VXGE_HAL_BASE_BADCFG + 134,
	VXGE_HAL_BADCFG_LAG_AGGR_WAIT_TIME	 = VXGE_HAL_BASE_BADCFG + 135,
	VXGE_HAL_BADCFG_LAG_SHORT_TIMER_SCALE	 = VXGE_HAL_BASE_BADCFG + 136,
	VXGE_HAL_BADCFG_LAG_LONG_TIMER_SCALE	 = VXGE_HAL_BASE_BADCFG + 137,
	VXGE_HAL_BADCFG_LAG_AGGR_AGGR_ID	 = VXGE_HAL_BASE_BADCFG + 138,
	VXGE_HAL_BADCFG_LAG_AGGR_USE_PORT_MAC_ADDR = VXGE_HAL_BASE_BADCFG + 139,
	VXGE_HAL_BADCFG_LAG_AGGR_MAC_ADDR_SEL	 = VXGE_HAL_BASE_BADCFG + 140,
	VXGE_HAL_BADCFG_LAG_AGGR_ADMIN_KEY	 = VXGE_HAL_BASE_BADCFG + 141,
	VXGE_HAL_BADCFG_LAG_PORT_PORT_ID	 = VXGE_HAL_BASE_BADCFG + 142,
	VXGE_HAL_BADCFG_LAG_PORT_LAG_EN		 = VXGE_HAL_BASE_BADCFG + 143,
	VXGE_HAL_BADCFG_LAG_PORT_DISCARD_SLOW_PROTO =
						VXGE_HAL_BASE_BADCFG + 144,
	VXGE_HAL_BADCFG_LAG_PORT_HOST_CHOSEN_AGGR = VXGE_HAL_BASE_BADCFG + 145,
	VXGE_HAL_BADCFG_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO =
						VXGE_HAL_BASE_BADCFG + 146,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_PORT_NUM = VXGE_HAL_BASE_BADCFG + 147,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_PORT_PRIORITY =
						VXGE_HAL_BASE_BADCFG + 148,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_KEY_10G	= VXGE_HAL_BASE_BADCFG + 149,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_KEY_1G	 = VXGE_HAL_BASE_BADCFG + 150,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_LACP_ACTIVITY =
						VXGE_HAL_BASE_BADCFG + 151,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_LACP_TIMEOUT =
						VXGE_HAL_BASE_BADCFG + 152,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_AGGREGATION = VXGE_HAL_BASE_BADCFG + 153,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_SYNCHRONIZATION =
						VXGE_HAL_BASE_BADCFG + 154,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_COLLECTING = VXGE_HAL_BASE_BADCFG + 155,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_DISTRIBUTING =
						VXGE_HAL_BASE_BADCFG + 156,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_DEFAULTED  = VXGE_HAL_BASE_BADCFG + 157,
	VXGE_HAL_BADCFG_LAG_PORT_ACTOR_EXPIRED	= VXGE_HAL_BASE_BADCFG + 158,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_SYS_PRI  = VXGE_HAL_BASE_BADCFG + 159,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_KEY	  = VXGE_HAL_BASE_BADCFG + 160,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_NUM	  = VXGE_HAL_BASE_BADCFG + 161,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_PORT_PRIORITY =
						VXGE_HAL_BASE_BADCFG + 162,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_LACP_ACTIVITY =
						VXGE_HAL_BASE_BADCFG + 163,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_LACP_TIMEOUT =
						VXGE_HAL_BASE_BADCFG + 164,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_AGGREGATION =
						VXGE_HAL_BASE_BADCFG + 165,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_SYNCHRONIZATION =
						VXGE_HAL_BASE_BADCFG + 166,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_COLLECTING =
						VXGE_HAL_BASE_BADCFG + 167,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_DISTRIBUTING =
						VXGE_HAL_BASE_BADCFG + 168,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_DEFAULTED  =
						VXGE_HAL_BASE_BADCFG + 169,
	VXGE_HAL_BADCFG_LAG_PORT_PARTNER_EXPIRED  =
						VXGE_HAL_BASE_BADCFG + 170,
	VXGE_HAL_BADCFG_VPATH_QOS_PRIORITY	 = VXGE_HAL_BASE_BADCFG + 171,
	VXGE_HAL_BADCFG_VPATH_QOS_MIN_BANDWIDTH  = VXGE_HAL_BASE_BADCFG + 172,
	VXGE_HAL_BADCFG_VPATH_QOS_MAX_BANDWIDTH  = VXGE_HAL_BASE_BADCFG + 173,

	VXGE_HAL_BADCFG_LOG_LEVEL		 = VXGE_HAL_BASE_BADCFG + 202,
	VXGE_HAL_BADCFG_RING_ENABLE		 = VXGE_HAL_BASE_BADCFG + 203,
	VXGE_HAL_BADCFG_RING_LENGTH		 = VXGE_HAL_BASE_BADCFG + 204,
	VXGE_HAL_BADCFG_RING_RXD_BUFFER_MODE	 = VXGE_HAL_BASE_BADCFG + 205,
	VXGE_HAL_BADCFG_RING_SCATTER_MODE	 = VXGE_HAL_BASE_BADCFG + 206,
	VXGE_HAL_BADCFG_RING_POST_MODE		 = VXGE_HAL_BASE_BADCFG + 207,
	VXGE_HAL_BADCFG_RING_MAX_FRM_LEN	 = VXGE_HAL_BASE_BADCFG + 208,
	VXGE_HAL_BADCFG_RING_NO_SNOOP_ALL	 = VXGE_HAL_BASE_BADCFG + 209,
	VXGE_HAL_BADCFG_RING_TIMER_VAL		 = VXGE_HAL_BASE_BADCFG + 210,
	VXGE_HAL_BADCFG_RING_GREEDY_RETURN	 = VXGE_HAL_BASE_BADCFG + 211,
	VXGE_HAL_BADCFG_RING_TIMER_CI		 = VXGE_HAL_BASE_BADCFG + 212,
	VXGE_HAL_BADCFG_RING_BACKOFF_INTERVAL_US  = VXGE_HAL_BASE_BADCFG + 213,
	VXGE_HAL_BADCFG_RING_INDICATE_MAX_PKTS	 = VXGE_HAL_BASE_BADCFG + 214,
	VXGE_HAL_BADCFG_FIFO_ENABLE		 = VXGE_HAL_BASE_BADCFG + 215,
	VXGE_HAL_BADCFG_FIFO_LENGTH		 = VXGE_HAL_BASE_BADCFG + 216,
	VXGE_HAL_BADCFG_FIFO_FRAGS		 = VXGE_HAL_BASE_BADCFG + 217,
	VXGE_HAL_BADCFG_FIFO_ALIGNMENT_SIZE	 = VXGE_HAL_BASE_BADCFG + 218,
	VXGE_HAL_BADCFG_FIFO_MAX_FRAGS		 = VXGE_HAL_BASE_BADCFG + 219,
	VXGE_HAL_BADCFG_FIFO_QUEUE_INTR		 = VXGE_HAL_BASE_BADCFG + 220,
	VXGE_HAL_BADCFG_FIFO_NO_SNOOP_ALL	 = VXGE_HAL_BASE_BADCFG + 221,
	VXGE_HAL_BADCFG_DMQ_LENGTH		 = VXGE_HAL_BASE_BADCFG + 222,
	VXGE_HAL_BADCFG_DMQ_IMMED_EN		 = VXGE_HAL_BASE_BADCFG + 223,
	VXGE_HAL_BADCFG_DMQ_EVENT_EN		 = VXGE_HAL_BASE_BADCFG + 224,
	VXGE_HAL_BADCFG_DMQ_INTR_CTRL		 = VXGE_HAL_BASE_BADCFG + 225,
	VXGE_HAL_BADCFG_DMQ_GEN_COMPL		 = VXGE_HAL_BASE_BADCFG + 226,
	VXGE_HAL_BADCFG_UMQ_LENGTH		 = VXGE_HAL_BASE_BADCFG + 227,
	VXGE_HAL_BADCFG_UMQ_IMMED_EN		 = VXGE_HAL_BASE_BADCFG + 228,
	VXGE_HAL_BADCFG_UMQ_EVENT_EN		 = VXGE_HAL_BASE_BADCFG + 229,
	VXGE_HAL_BADCFG_UMQ_INTR_CTRL		 = VXGE_HAL_BASE_BADCFG + 230,
	VXGE_HAL_BADCFG_UMQ_GEN_COMPL		 = VXGE_HAL_BASE_BADCFG + 231,
	VXGE_HAL_BADCFG_SW_LRO_SESSIONS		 = VXGE_HAL_BASE_BADCFG + 232,
	VXGE_HAL_BADCFG_SW_LRO_SG_SIZE		 = VXGE_HAL_BASE_BADCFG + 333,
	VXGE_HAL_BADCFG_SW_LRO_FRM_LEN		 = VXGE_HAL_BASE_BADCFG + 334,
	VXGE_HAL_BADCFG_SW_LRO_MODE		 = VXGE_HAL_BASE_BADCFG + 235,
	VXGE_HAL_BADCFG_LRO_SESSIONS_MAX	 = VXGE_HAL_BASE_BADCFG + 236,
	VXGE_HAL_BADCFG_LRO_SESSIONS_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 237,
	VXGE_HAL_BADCFG_LRO_SESSIONS_TIMEOUT	 = VXGE_HAL_BASE_BADCFG + 238,
	VXGE_HAL_BADCFG_LRO_NO_WQE_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 239,
	VXGE_HAL_BADCFG_LRO_DUPACK_DETECTION	 = VXGE_HAL_BASE_BADCFG + 242,
	VXGE_HAL_BADCFG_LRO_DATA_MERGING	 = VXGE_HAL_BASE_BADCFG + 243,
	VXGE_HAL_BADCFG_LRO_ACK_MERGING		 = VXGE_HAL_BASE_BADCFG + 244,
	VXGE_HAL_BADCFG_LRO_LLC_HDR_MODE	 = VXGE_HAL_BASE_BADCFG + 245,
	VXGE_HAL_BADCFG_LRO_SNAP_HDR_MODE	 = VXGE_HAL_BASE_BADCFG + 246,
	VXGE_HAL_BADCFG_LRO_SESSION_ECN		 = VXGE_HAL_BASE_BADCFG + 247,
	VXGE_HAL_BADCFG_LRO_SESSION_ECN_NONCE	 = VXGE_HAL_BASE_BADCFG + 248,
	VXGE_HAL_BADCFG_LRO_RXD_BUFFER_MODE	 = VXGE_HAL_BASE_BADCFG + 249,
	VXGE_HAL_BADCFG_LRO_SCATTER_MODE	 = VXGE_HAL_BASE_BADCFG + 250,
	VXGE_HAL_BADCFG_LRO_IP_DATAGRAM_SIZE	 = VXGE_HAL_BASE_BADCFG + 251,
	VXGE_HAL_BADCFG_LRO_FRAME_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 252,
	VXGE_HAL_BADCFG_LRO_PSH_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 253,
	VXGE_HAL_BADCFG_LRO_MTU_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 254,
	VXGE_HAL_BADCFG_LRO_MSS_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 255,
	VXGE_HAL_BADCFG_LRO_TCP_TSVAL_DELTA	 = VXGE_HAL_BASE_BADCFG + 256,
	VXGE_HAL_BADCFG_LRO_ACK_NBR_DELTA	 = VXGE_HAL_BASE_BADCFG + 257,
	VXGE_HAL_BADCFG_LRO_SPARE_WQE_CAPACITY	 = VXGE_HAL_BASE_BADCFG + 258,
	VXGE_HAL_BADCFG_TIM_INTR_ENABLE		 = VXGE_HAL_BASE_BADCFG + 259,
	VXGE_HAL_BADCFG_TIM_BTIMER_VAL		 = VXGE_HAL_BASE_BADCFG + 261,
	VXGE_HAL_BADCFG_TIM_TIMER_AC_EN		 = VXGE_HAL_BASE_BADCFG + 262,
	VXGE_HAL_BADCFG_TIM_TIMER_CI_EN		 = VXGE_HAL_BASE_BADCFG + 263,
	VXGE_HAL_BADCFG_TIM_TIMER_RI_EN		 = VXGE_HAL_BASE_BADCFG + 264,
	VXGE_HAL_BADCFG_TIM_BTIMER_EVENT_SF	 = VXGE_HAL_BASE_BADCFG + 265,
	VXGE_HAL_BADCFG_TIM_RTIMER_VAL		 = VXGE_HAL_BASE_BADCFG + 266,
	VXGE_HAL_BADCFG_TIM_UTIL_SEL		 = VXGE_HAL_BASE_BADCFG + 267,
	VXGE_HAL_BADCFG_TIM_LTIMER_VAL		 = VXGE_HAL_BASE_BADCFG + 268,
	VXGE_HAL_BADCFG_TXFRM_CNT_EN		 = VXGE_HAL_BASE_BADCFG + 269,
	VXGE_HAL_BADCFG_TXD_CNT_EN		 = VXGE_HAL_BASE_BADCFG + 270,
	VXGE_HAL_BADCFG_TIM_URANGE_A		 = VXGE_HAL_BASE_BADCFG + 271,
	VXGE_HAL_BADCFG_TIM_UEC_A		 = VXGE_HAL_BASE_BADCFG + 272,
	VXGE_HAL_BADCFG_TIM_URANGE_B		 = VXGE_HAL_BASE_BADCFG + 273,
	VXGE_HAL_BADCFG_TIM_UEC_B		 = VXGE_HAL_BASE_BADCFG + 274,
	VXGE_HAL_BADCFG_TIM_URANGE_C		 = VXGE_HAL_BASE_BADCFG + 275,
	VXGE_HAL_BADCFG_TIM_UEC_C		 = VXGE_HAL_BASE_BADCFG + 276,
	VXGE_HAL_BADCFG_TIM_UEC_D		 = VXGE_HAL_BASE_BADCFG + 277,
	VXGE_HAL_BADCFG_VPATH_ID		 = VXGE_HAL_BASE_BADCFG + 278,
	VXGE_HAL_BADCFG_VPATH_WIRE_PORT		 = VXGE_HAL_BASE_BADCFG + 279,
	VXGE_HAL_BADCFG_VPATH_NO_SNOOP		 = VXGE_HAL_BASE_BADCFG + 281,
	VXGE_HAL_BADCFG_VPATH_MTU		 = VXGE_HAL_BASE_BADCFG + 282,
	VXGE_HAL_BADCFG_VPATH_TPA_LSOV2_EN	 = VXGE_HAL_BASE_BADCFG + 283,
	VXGE_HAL_BADCFG_VPATH_TPA_IGNORE_FRAME_ERROR =
						VXGE_HAL_BASE_BADCFG + 284,
	VXGE_HAL_BADCFG_VPATH_TPA_IPV6_KEEP_SEARCHING =
						VXGE_HAL_BASE_BADCFG + 285,
	VXGE_HAL_BADCFG_VPATH_TPA_L4_PSHDR_PRESENT = VXGE_HAL_BASE_BADCFG + 286,
	VXGE_HAL_BADCFG_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS =
						VXGE_HAL_BASE_BADCFG + 287,
	VXGE_HAL_BADCFG_VPATH_RPA_IPV4_TCP_INCL_PH = VXGE_HAL_BASE_BADCFG + 288,
	VXGE_HAL_BADCFG_VPATH_RPA_IPV6_TCP_INCL_PH = VXGE_HAL_BASE_BADCFG + 289,
	VXGE_HAL_BADCFG_VPATH_RPA_IPV4_UDP_INCL_PH = VXGE_HAL_BASE_BADCFG + 290,
	VXGE_HAL_BADCFG_VPATH_RPA_IPV6_UDP_INCL_PH = VXGE_HAL_BASE_BADCFG + 291,
	VXGE_HAL_BADCFG_VPATH_RPA_L4_INCL_CF	 = VXGE_HAL_BASE_BADCFG + 292,
	VXGE_HAL_BADCFG_VPATH_RPA_STRIP_VLAN_TAG  = VXGE_HAL_BASE_BADCFG + 293,
	VXGE_HAL_BADCFG_VPATH_RPA_L4_COMP_CSUM	 = VXGE_HAL_BASE_BADCFG + 294,
	VXGE_HAL_BADCFG_VPATH_RPA_L3_INCL_CF	 = VXGE_HAL_BASE_BADCFG + 295,
	VXGE_HAL_BADCFG_VPATH_RPA_L3_COMP_CSUM	 = VXGE_HAL_BASE_BADCFG + 296,
	VXGE_HAL_BADCFG_VPATH_RPA_UCAST_ALL_ADDR_EN =
						VXGE_HAL_BASE_BADCFG + 297,
	VXGE_HAL_BADCFG_VPATH_RPA_MCAST_ALL_ADDR_EN =
						VXGE_HAL_BASE_BADCFG + 298,
	VXGE_HAL_BADCFG_VPATH_RPA_CAST_EN	 = VXGE_HAL_BASE_BADCFG + 299,
	VXGE_HAL_BADCFG_VPATH_RPA_ALL_VID_EN	 = VXGE_HAL_BASE_BADCFG + 300,
	VXGE_HAL_BADCFG_VPATH_VP_Q_L2_FLOW	 = VXGE_HAL_BASE_BADCFG + 301,
	VXGE_HAL_BADCFG_VPATH_VP_STATS_READ_METHOD = VXGE_HAL_BASE_BADCFG + 302,
	VXGE_HAL_BADCFG_VPATH_BANDWIDTH_LIMIT	 = VXGE_HAL_BASE_BADCFG + 305,
	VXGE_HAL_BADCFG_BLOCKPOOL_MIN		 = VXGE_HAL_BASE_BADCFG + 306,
	VXGE_HAL_BADCFG_BLOCKPOOL_INITIAL	 = VXGE_HAL_BASE_BADCFG + 307,
	VXGE_HAL_BADCFG_BLOCKPOOL_INCR		 = VXGE_HAL_BASE_BADCFG + 308,
	VXGE_HAL_BADCFG_BLOCKPOOL_MAX		 = VXGE_HAL_BASE_BADCFG + 309,
	VXGE_HAL_BADCFG_ISR_POLLING_CNT		 = VXGE_HAL_BASE_BADCFG + 310,
	VXGE_HAL_BADCFG_MAX_PAYLOAD_SIZE	 = VXGE_HAL_BASE_BADCFG + 312,
	VXGE_HAL_BADCFG_MMRB_COUNT		 = VXGE_HAL_BASE_BADCFG + 313,
	VXGE_HAL_BADCFG_STATS_REFRESH_TIME	 = VXGE_HAL_BASE_BADCFG + 314,
	VXGE_HAL_BADCFG_DUMP_ON_UNKNOWN		 = VXGE_HAL_BASE_BADCFG + 315,
	VXGE_HAL_BADCFG_DUMP_ON_SERR		 = VXGE_HAL_BASE_BADCFG + 316,
	VXGE_HAL_BADCFG_DUMP_ON_CRITICAL	 = VXGE_HAL_BASE_BADCFG + 317,
	VXGE_HAL_BADCFG_DUMP_ON_ECCERR		 = VXGE_HAL_BASE_BADCFG + 318,
	VXGE_HAL_BADCFG_INTR_MODE		 = VXGE_HAL_BASE_BADCFG + 319,
	VXGE_HAL_BADCFG_RTH_EN			 = VXGE_HAL_BASE_BADCFG + 320,
	VXGE_HAL_BADCFG_RTH_IT_TYPE		 = VXGE_HAL_BASE_BADCFG + 321,
	VXGE_HAL_BADCFG_UFCA_INTR_THRES		 = VXGE_HAL_BASE_BADCFG + 323,
	VXGE_HAL_BADCFG_UFCA_LO_LIM		 = VXGE_HAL_BASE_BADCFG + 324,
	VXGE_HAL_BADCFG_UFCA_HI_LIM		 = VXGE_HAL_BASE_BADCFG + 325,
	VXGE_HAL_BADCFG_UFCA_LBOLT_PERIOD	 = VXGE_HAL_BASE_BADCFG + 326,
	VXGE_HAL_BADCFG_DEVICE_POLL_MILLIS	 = VXGE_HAL_BASE_BADCFG + 327,
	VXGE_HAL_BADCFG_RTS_MAC_EN		 = VXGE_HAL_BASE_BADCFG + 330,
	VXGE_HAL_BADCFG_RTS_QOS_EN		 = VXGE_HAL_BASE_BADCFG + 331,
	VXGE_HAL_BADCFG_RTS_PORT_EN		 = VXGE_HAL_BASE_BADCFG + 332,
	VXGE_HAL_BADCFG_MAX_CQE_GROUPS		 = VXGE_HAL_BASE_BADCFG + 333,
	VXGE_HAL_BADCFG_MAX_NUM_OD_GROUPS	 = VXGE_HAL_BASE_BADCFG + 334,
	VXGE_HAL_BADCFG_NO_WQE_THRESHOLD	 = VXGE_HAL_BASE_BADCFG + 335,
	VXGE_HAL_BADCFG_REFILL_THRESHOLD_HIGH	 = VXGE_HAL_BASE_BADCFG + 336,
	VXGE_HAL_BADCFG_REFILL_THRESHOLD_LOW	 = VXGE_HAL_BASE_BADCFG + 337,
	VXGE_HAL_BADCFG_ACK_BLOCK_LIMIT		 = VXGE_HAL_BASE_BADCFG + 338,
	VXGE_HAL_BADCFG_STATS_READ_METHOD	 = VXGE_HAL_BASE_BADCFG + 339,
	VXGE_HAL_BADCFG_POLL_OR_DOOR_BELL	 = VXGE_HAL_BASE_BADCFG + 340,
	VXGE_HAL_BADCFG_MSIX_ID			 = VXGE_HAL_BASE_BADCFG + 341,
	VXGE_HAL_BADCFG_VPATH_PRIORITY		 = VXGE_HAL_BASE_BADCFG + 342,
	VXGE_HAL_EOF_TRACE_BUF			 = -1

} vxge_hal_status_e;

/*
 * enum vxge_hal_result_e - HAL Up Message result codes.
 * @VXGE_HAL_RESULT_OK: Success
 */
typedef enum vxge_hal_result_e {
	VXGE_HAL_RESULT_OK			 = 0
} vxge_hal_result_e;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_STATUS_H */
