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

#ifndef	VXGE_HAL_LL_H
#define	VXGE_HAL_LL_H

#include <dev/vxge/include/vxgehal-version.h>
#include <dev/vxge/include/vxge-defs.h>
#include <dev/vxge/include/vxge-os-pal.h>
#include "vxgehal-status.h"
#include <dev/vxge/include/vxgehal-types.h>
#include <dev/vxge/include/vxge-debug.h>
#include <dev/vxge/include/vxge-list.h>
#include <dev/vxge/include/vxge-queue.h>
#include <dev/vxge/include/vxgehal-config.h>
#include <dev/vxge/include/vxgehal-stats.h>
#include <dev/vxge/include/vxgehal-mgmt.h>
#include <dev/vxge/include/vxgehal-mgmtaux.h>

__EXTERN_BEGIN_DECLS

/*
 * Driver
 */

/*
 * enum vxge_hal_xpak_alarm_type_e - XPAK Alarm types
 * @VXGE_HAL_XPAK_ALARM_EXCESS_TEMP: Excess temparature alarm
 * @VXGE_HAL_XPAK_ALARM_EXCESS_BIAS_CURRENT: Excess bias current alarm
 * @VXGE_HAL_XPAK_ALARM_EXCESS_LASER_OUTPUT: Excess laser output alarm
 *
 * XPAK alarm types
 */
typedef enum vxge_hal_xpak_alarm_type_e {
	VXGE_HAL_XPAK_ALARM_EXCESS_TEMP		= 1,
	VXGE_HAL_XPAK_ALARM_EXCESS_BIAS_CURRENT	= 2,
	VXGE_HAL_XPAK_ALARM_EXCESS_LASER_OUTPUT	= 3,
} vxge_hal_xpak_alarm_type_e;

/*
 * function vxge_uld_sched_timer_cb_f - Per-device periodic timer
 * callback.
 * @devh: HAL device handle.
 * @userdata: Per-device user data (a.k.a. context) specified via
 * vxge_hal_device_initialize().
 *
 * Periodic or one-shot timer callback. If specified (that is, not NULL)
 * HAL invokes this callback periodically. The call is performed in the
 * interrupt context, or more exactly, in the context of HAL's ISR
 * vxge_hal_device_continue_irq().
 *
 * See also: vxge_hal_device_initialize {}
 */
typedef void (*vxge_uld_sched_timer_cb_f) (
    vxge_hal_device_h devh,
    void *userdata);

/*
 * function vxge_uld_link_up_f - Link-Up callback provided by upper-layer
 * driver.
 * @devh: HAL device handle.
 * @userdata: Opaque context set by the ULD via
 * vxge_hal_device_private_set()
 * (typically - at HAL device iinitialization time).
 *
 * Link-up notification callback provided by the ULD.
 * This is one of the per-driver callbacks, see vxge_hal_uld_cbs_t {}.
 *
 * See also: vxge_hal_uld_cbs_t {}, vxge_uld_link_down_f {},
 * vxge_hal_driver_initialize(), vxge_hal_device_private_set().
 */
typedef void (*vxge_uld_link_up_f) (
    vxge_hal_device_h devh,
    void *userdata);

/*
 * function vxge_uld_link_down_f - Link-Down callback provided by
 * upper-layer driver.
 * @devh: HAL device handle.
 * @userdata: Opaque context set by the ULD via
 * vxge_hal_device_private_set()
 * (typically - at HAL device iinitialization time).
 *
 * Link-Down notification callback provided by the upper-layer driver.
 * This is one of the per-driver callbacks, see vxge_hal_uld_cbs_t {}.
 *
 * See also: vxge_hal_uld_cbs_t {}, vxge_uld_link_up_f {},
 * vxge_hal_driver_initialize(), vxge_hal_device_private_set().
 */
typedef void (*vxge_uld_link_down_f) (
    vxge_hal_device_h devh,
    void *userdata);

/*
 * function vxge_uld_crit_err_f - Critical Error notification callback.
 * @devh: HAL device handle.
 * @userdata: Opaque context set by the ULD via
 * vxge_hal_device_private_set()
 * (typically - at HAL device iinitialization time).
 * @type: Enumerated hw error, e.g.: double ECC.
 * @serr_data: X3100 status.
 * @ext_data: Extended data. The contents depends on the @type.
 *
 * Critical error notification callback provided by the upper-layer driver.
 * This is one of the per-driver callbacks, see vxge_hal_uld_cbs_t {}.
 *
 * See also: vxge_hal_uld_cbs_t {}, vxge_hal_event_e {},
 * vxge_hal_device_private_set(), vxge_hal_driver_initialize().
 */
typedef void (*vxge_uld_crit_err_f) (
    vxge_hal_device_h devh,
    void *userdata,
    vxge_hal_event_e type,
    u64 ext_data);

/*
 * function vxge_uld_xpak_alarm_log_f - ULD "XPAK alarm log" callback.
 * @devh: HAL device handle.
 * @port: Port number
 * @type: XPAK Alarm type
 *
 * Unless NULL is specified, HAL invokes the callback after checking XPAK
 * counters
 */
typedef void (*vxge_uld_xpak_alarm_log_f) (
    vxge_hal_device_h devh,
    u32 port,
    vxge_hal_xpak_alarm_type_e type);

/*
 * struct vxge_hal_uld_cbs_t - Upper-layer driver "slow-path" callbacks.
 * @link_up: See vxge_uld_link_up_f {}.
 * @link_down: See vxge_uld_link_down_f {}.
 * @crit_err: See vxge_uld_crit_err_f {}.
 * @sched_timer: See vxge_uld_sched_timer_cb_f {}.
 * @xpak_alarm_log:
 *
 * Upper layer driver slow-path (per-driver) callbacks.
 * Implemented by ULD and provided to HAL via
 * vxge_hal_driver_initialize().
 * Note that these callbacks are not mandatory: HAL will not invoke
 * a callback if NULL is specified.
 *
 * See also: vxge_hal_driver_initialize().
 */
typedef struct vxge_hal_uld_cbs_t {
	vxge_uld_link_up_f		link_up;
	vxge_uld_link_down_f		link_down;
	vxge_uld_crit_err_f		crit_err;
	vxge_uld_sched_timer_cb_f	sched_timer;
	vxge_uld_xpak_alarm_log_f	xpak_alarm_log;
} vxge_hal_uld_cbs_t;

/*
 * vxge_hal_driver_initialize - Initialize HAL.
 * @config: HAL configuration, see vxge_hal_driver_config_t {}.
 * @uld_callbacks: Upper-layer driver callbacks, e.g. link-up.
 *
 * HAL initialization entry point. Not to confuse with device initialization
 * (note that HAL "contains" zero or more X3100 devices).
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_ERR_BAD_DRIVER_CONFIG - Driver configuration params invalid.
 *
 */
vxge_hal_status_e
vxge_hal_driver_initialize(
    vxge_hal_driver_config_t *config,
    vxge_hal_uld_cbs_t *uld_callbacks);

/*
 * vxge_hal_driver_debug_set - Set the debug module, level and timestamp
 * @level: Debug level as defined in enum vxge_debug_level_e
 *
 * This routine is used to dynamically change the debug output
 */
void
vxge_hal_driver_debug_set(
    vxge_debug_level_e level);

/*
 * vxge_hal_driver_debug_get - Get the debug level
 *
 * This routine returns the current debug level set
 */
u32
vxge_hal_driver_debug_get(void);

/*
 * vxge_hal_driver_terminate - Terminate HAL.
 *
 * HAL termination entry point.
 */
void
vxge_hal_driver_terminate(void);

void *
vxge_hal_device_get_legacy_reg(pci_dev_h pdev, pci_reg_h regh, u8 *bar0);

/*
 * RX Descriptor
 */
/*
 * enum vxge_hal_rxd_state_e - Descriptor (RXD) state.
 * @VXGE_HAL_RXD_STATE_NONE: Invalid state.
 * @VXGE_HAL_RXD_STATE_AVAIL: Descriptor is available for reservation.
 * @VXGE_HAL_RXD_STATE_POSTED: Descriptor is posted for processing by the
 * device.
 * @VXGE_HAL_RXD_STATE_FREED: Descriptor is free and can be reused for
 * filling-in and posting later.
 *
 * X3100/HAL descriptor states.
 *
 */
typedef enum vxge_hal_rxd_state_e {
	VXGE_HAL_RXD_STATE_NONE		= 0,
	VXGE_HAL_RXD_STATE_AVAIL	= 1,
	VXGE_HAL_RXD_STATE_POSTED	= 2,
	VXGE_HAL_RXD_STATE_FREED	= 3
} vxge_hal_rxd_state_e;

/*
 * Ring
 */

/*
 * struct vxge_hal_ring_rxd_info_t - Extended information associated with a
 *			  completed ring descriptor.
 * @syn_flag: SYN flag
 * @is_icmp: Is ICMP
 * @fast_path_eligible: Fast Path Eligible flag
 * @l3_cksum_valid: in L3 checksum is valid
 * @l3_cksum: Result of IP checksum check (by X3100 hardware).
 *	    This field containing VXGE_HAL_L3_CKSUM_OK would mean that
 *	    the checksum is correct, otherwise - the datagram is
 *	    corrupted.
 * @l4_cksum_valid: in L4 checksum is valid
 * @l4_cksum: Result of TCP/UDP checksum check (by X3100 hardware).
 *	    This field containing VXGE_HAL_L4_CKSUM_OK would mean that
 *	    the checksum is correct. Otherwise - the packet is
 *	    corrupted.
 * @frame: Zero or more of vxge_hal_frame_type_e flags.
 *	    See vxge_hal_frame_type_e {}.
 * @proto: zero or more of vxge_hal_frame_proto_e flags.  Reporting bits for
 *	    various higher-layer protocols, including (but note restricted to)
 *	    TCP and UDP. See vxge_hal_frame_proto_e {}.
 * @is_vlan: If vlan tag is valid
 * @vlan: VLAN tag extracted from the received frame.
 * @rth_bucket: RTH bucket
 * @rth_it_hit: Set, If RTH hash value calculated by the X3100 hardware
 *	    has a matching entry in the Indirection table.
 * @rth_spdm_hit: Set, If RTH hash value calculated by the X3100 hardware
 *	    has a matching entry in the Socket Pair Direct Match table.
 * @rth_hash_type: RTH hash code of the function used to calculate the hash.
 * @rth_value: Receive Traffic Hashing(RTH) hash value. Produced by X3100
 *	    hardware if RTH is enabled.
 */
typedef struct vxge_hal_ring_rxd_info_t {
	u32	syn_flag;
	u32	is_icmp;
	u32	fast_path_eligible;
	u32	l3_cksum_valid;
	u32	l3_cksum;
	u32	l4_cksum_valid;
	u32	l4_cksum;
	u32	frame;
	u32	proto;
	u32	is_vlan;
#define	VXGE_HAL_VLAN_VID_MASK 0xfff
	u32	vlan;
	u32	rth_bucket;
	u32	rth_it_hit;
	u32	rth_spdm_hit;
	u32	rth_hash_type;
	u32	rth_value;
} vxge_hal_ring_rxd_info_t;

/*
 * enum vxge_hal_frame_type_e - Ethernet frame format.
 * @VXGE_HAL_FRAME_TYPE_DIX: DIX (Ethernet II) format.
 * @VXGE_HAL_FRAME_TYPE_LLC: LLC format.
 * @VXGE_HAL_FRAME_TYPE_SNAP: SNAP format.
 * @VXGE_HAL_FRAME_TYPE_IPX: IPX format.
 *
 * Ethernet frame format.
 */
typedef enum vxge_hal_frame_type_e {
	VXGE_HAL_FRAME_TYPE_DIX			= 0x0,
	VXGE_HAL_FRAME_TYPE_LLC			= 0x1,
	VXGE_HAL_FRAME_TYPE_SNAP		= 0x2,
	VXGE_HAL_FRAME_TYPE_IPX			= 0x3,
} vxge_hal_frame_type_e;

typedef enum vxge_hal_tcp_option_e {

	VXGE_HAL_TCPOPT_NOP = 1,	/* Padding */
	VXGE_HAL_TCPOPT_EOL = 0,	/* End of options */
	VXGE_HAL_TCPOPT_MSS = 2,	/* Segment size negotiating */
	VXGE_HAL_TCPOPT_WINDOW = 3,	/* Window scaling */
	VXGE_HAL_TCPOPT_SACK_PERM = 4,	/* SACK Permitted */
	VXGE_HAL_TCPOPT_SACK = 5,	/* SACK Block */
	VXGE_HAL_TCPOPT_TIMESTAMP = 8,	/* Better RTT estimations/PAWS */
	VXGE_HAL_TCPOPT_MD5SIG = 19,	/* MD5 Signature (RFC2385) */
	VXGE_HAL_TCPOLEN_TIMESTAMP = 10,
	VXGE_HAL_TCPOLEN_TSTAMP_ALIGNED = 12

} vxge_hal_tcp_option_e;

/*
 * enum vxge_hal_frame_proto_e - Higher-layer ethernet protocols.
 * @VXGE_HAL_FRAME_PROTO_VLAN_TAGGED: VLAN.
 * @VXGE_HAL_FRAME_PROTO_IPV4: IPv4.
 * @VXGE_HAL_FRAME_PROTO_IPV6: IPv6.
 * @VXGE_HAL_FRAME_PROTO_IP_FRAG: IP fragmented.
 * @VXGE_HAL_FRAME_PROTO_TCP: TCP.
 * @VXGE_HAL_FRAME_PROTO_UDP: UDP.
 * @VXGE_HAL_FRAME_PROTO_TCP_OR_UDP: TCP or UDP.
 *
 * Higher layer ethernet protocols and options.
 */
typedef enum vxge_hal_frame_proto_e {
	VXGE_HAL_FRAME_PROTO_VLAN_TAGGED = 0x80,
	VXGE_HAL_FRAME_PROTO_IPV4		= 0x10,
	VXGE_HAL_FRAME_PROTO_IPV6		= 0x08,
	VXGE_HAL_FRAME_PROTO_IP_FRAG		= 0x04,
	VXGE_HAL_FRAME_PROTO_TCP		= 0x02,
	VXGE_HAL_FRAME_PROTO_UDP		= 0x01,
	VXGE_HAL_FRAME_PROTO_TCP_OR_UDP		= (VXGE_HAL_FRAME_PROTO_TCP | \
						VXGE_HAL_FRAME_PROTO_UDP)
} vxge_hal_frame_proto_e;

/*
 * enum vxge_hal_ring_tcode_e - Transfer codes returned by adapter
 * @VXGE_HAL_RING_T_CODE_OK: Transfer ok.
 * @VXGE_HAL_RING_T_CODE_L3_CKSUM_MISMATCH: Layer 3 checksum presentation
 *		configuration mismatch.
 * @VXGE_HAL_RING_T_CODE_L4_CKSUM_MISMATCH: Layer 4 checksum presentation
 *		configuration mismatch.
 * @VXGE_HAL_RING_T_CODE_L3_L4_CKSUM_MISMATCH: Layer 3 and Layer 4 checksum
 *		presentation configuration mismatch.
 * @VXGE_HAL_RING_T_CODE_L3_PKT_ERR: Layer 3 error¸unparseable packet,
 *		such as unknown IPv6 header.
 * @VXGE_HAL_RING_T_CODE_L2_FRM_ERR: Layer 2 error¸frame integrity
 *		error, such as FCS or ECC).
 * @VXGE_HAL_RING_T_CODE_BUF_SIZE_ERR: Buffer size error¸the RxD buffer(
 *		s) were not appropriately sized and data loss occurred.
 * @VXGE_HAL_RING_T_CODE_INT_ECC_ERR: Internal ECC error¸RxD corrupted.
 * @VXGE_HAL_RING_T_CODE_BENIGN_OVFLOW: Benign overflow¸the contents of
 *		Segment1 exceeded the capacity of Buffer1 and the remainder
 *		was placed in Buffer2. Segment2 now starts in Buffer3.
 *		No data loss or errors occurred.
 * @VXGE_HAL_RING_T_CODE_ZERO_LEN_BUFF: Buffer size 0¸one of the RxDs
 *		assigned buffers has a size of 0 bytes.
 * @VXGE_HAL_RING_T_CODE_FRM_DROP: Frame dropped¸either due to
 *		VPath Reset or because of a VPIN mismatch.
 * @VXGE_HAL_RING_T_CODE_UNUSED: Unused
 * @VXGE_HAL_RING_T_CODE_MULTI_ERR: Multiple errors¸more than one
 *		transfer code condition occurred.
 *
 * Transfer codes returned by adapter.
 */
typedef enum vxge_hal_ring_tcode_e {
	VXGE_HAL_RING_T_CODE_OK				= 0x0,
	VXGE_HAL_RING_T_CODE_L3_CKSUM_MISMATCH		= 0x1,
	VXGE_HAL_RING_T_CODE_L4_CKSUM_MISMATCH		= 0x2,
	VXGE_HAL_RING_T_CODE_L3_L4_CKSUM_MISMATCH	= 0x3,
	VXGE_HAL_RING_T_CODE_L3_PKT_ERR			= 0x5,
	VXGE_HAL_RING_T_CODE_L2_FRM_ERR			= 0x6,
	VXGE_HAL_RING_T_CODE_BUF_SIZE_ERR		= 0x7,
	VXGE_HAL_RING_T_CODE_INT_ECC_ERR		= 0x8,
	VXGE_HAL_RING_T_CODE_BENIGN_OVFLOW		= 0x9,
	VXGE_HAL_RING_T_CODE_ZERO_LEN_BUFF		= 0xA,
	VXGE_HAL_RING_T_CODE_FRM_DROP			= 0xC,
	VXGE_HAL_RING_T_CODE_UNUSED			= 0xE,
	VXGE_HAL_RING_T_CODE_MULTI_ERR			= 0xF
} vxge_hal_ring_tcode_e;

/*
 * enum vxge_hal_ring_hash_type_e - RTH hash types
 * @VXGE_HAL_RING_HASH_TYPE_NONE: No Hash
 * @VXGE_HAL_RING_HASH_TYPE_TCP_IPV4: TCP IPv4
 * @VXGE_HAL_RING_HASH_TYPE_UDP_IPV4: UDP IPv4
 * @VXGE_HAL_RING_HASH_TYPE_IPV4: IPv4
 * @VXGE_HAL_RING_HASH_TYPE_TCP_IPV6: TCP IPv6
 * @VXGE_HAL_RING_HASH_TYPE_UDP_IPV6: UDP IPv6
 * @VXGE_HAL_RING_HASH_TYPE_IPV6: IPv6
 * @VXGE_HAL_RING_HASH_TYPE_TCP_IPV6_EX: TCP IPv6 extension
 * @VXGE_HAL_RING_HASH_TYPE_UDP_IPV6_EX: UDP IPv6 extension
 * @VXGE_HAL_RING_HASH_TYPE_IPV6_EX: IPv6 extension
 *
 * RTH hash types
 */
typedef enum vxge_hal_ring_hash_type_e {
	VXGE_HAL_RING_HASH_TYPE_NONE			= 0x0,
	VXGE_HAL_RING_HASH_TYPE_TCP_IPV4		= 0x1,
	VXGE_HAL_RING_HASH_TYPE_UDP_IPV4		= 0x2,
	VXGE_HAL_RING_HASH_TYPE_IPV4			= 0x3,
	VXGE_HAL_RING_HASH_TYPE_TCP_IPV6		= 0x4,
	VXGE_HAL_RING_HASH_TYPE_UDP_IPV6		= 0x5,
	VXGE_HAL_RING_HASH_TYPE_IPV6			= 0x6,
	VXGE_HAL_RING_HASH_TYPE_TCP_IPV6_EX		= 0x7,
	VXGE_HAL_RING_HASH_TYPE_UDP_IPV6_EX		= 0x8,
	VXGE_HAL_RING_HASH_TYPE_IPV6_EX			= 0x9
} vxge_hal_ring_hash_type_e;

/*
 * struct vxge_hal_ring_rxd_1_t - One buffer mode RxD for ring
 * @host_control: This field is exclusively for host use and is "readonly"
 *	    from the adapter's perspective.
 * @control_0:Bits 0 to 6 - RTH_Bucket get
 *	    Bit 7 - Own Descriptor ownership bit. This bit is set to 1 by the
 *	    host, and is set to 0 by the adapter.
 *		 0 - Host owns RxD and buffer.
 *		 1 - The adapter owns RxD and buffer.
 *	    Bit 8 - Fast_Path_Eligible When set, indicates that the received
 *	   frame meets all of the criteria for fast path processing.
 *	   The required criteria are as follows:
 *	   !SYN &
 *	   (Transfer_Code == "Transfer OK") &
 *	   (!Is_IP_Fragment) &
 *	   ((Is_IPv4 & computed_L3_checksum == 0xFFFF) |
 *	   (Is_IPv6)) &
 *	   ((Is_TCP & computed_L4_checksum == 0xFFFF) |
 *	   (Is_UDP & (computed_L4_checksum == 0xFFFF |
 *	   computed _L4_checksum == 0x0000)))
 *	   (same meaning for all RxD buffer modes)
 *		 Bit 9 - L3 Checksum Correct
 *		 Bit 10 - L4 Checksum Correct
 *		 Bit 11 - Reserved
 *		 Bit 12 to 15 - This field is written by the adapter. It is used
 *		 to report the status of the frame transfer to the host.
 *		 0x0 - Transfer OK
 *		 0x4 - RDA Failure During Transfer
 *		 0x5 - Unparseable Packet, such as unknown IPv6 header.
 *		 0x6 - Frame integrity error (FCS or ECC).
 *		 0x7 - Buffer Size Error. The provided buffer(s) were not
 *		 appropriately sized and data loss occurred.
 *		 0x8 - Internal ECC Error. RxD corrupted.
 *		 0x9 - IPv4 Checksum error
 *		 0xA - TCP/UDP Checksum error
 *		 0xF - Unknown Error or Multiple Error. Indicates an unknown
 *		 problem or that more than one of transfer codes is set.
 *		 Bit 16 - SYN The adapter sets this field to indicate that the
 *		 incoming frame contained a TCP segment with its SYN bit set
 *	   and its ACK bit NOT set. (same meaning for all RxD buffer modes)
 *		 Bit 17 - Is ICMP
 *		 Bit 18 - RTH_SPDM_HIT Set to 1 if there was a match in the
 *		 Socket
 *	   Pair Direct Match Table and the frame was steered based on SPDM.
 *		 Bit 19 - RTH_IT_HIT Set to 1 if there was a match in the
 *	   Indirection Table and the frame was steered based on hash
 *	   indirection.
 *		 Bit 20 to 23 - RTH_HASH_TYPE Indicates the function (hash type)
 *	   that was used to calculate the hash.
 *		 Bit 19 - IS_VLAN Set to '1' if the frame was/is VLAN tagged.
 *		 Bit 25 to 26 - ETHER_ENCAP Reflects the Ethernet encapsulation
 *	   of the received frame.
 *		 0x0 - Ethernet DIX
 *		 0x1 - LLC
 *		 0x2 - SNAP (includes Jumbo-SNAP)
 *		 0x3 - IPX
 *		 Bit 27 - IS_IPV4 Set to '1' if the frame contains IPv4 packet.
 *		 Bit 28 - IS_IPV6 Set to '1' if the frame contains IPv6 packet.
 *		 Bit 29 - IS_IP_FRAG Set to '1' if the frame contains a
 *		 fragmented IP packet.
 *		 Bit 30 - IS_TCP Set to '1' if the frame contains a TCP segment.
 *		 Bit 31 - IS_UDP Set to '1' if the frame contains a UDP message.
 *		 Bit 32 to 47 - L3_Checksum[0:15] The IPv4 checksum value that
 *	   arrived with the frame. If the resulting computed IPv4 header
 *	   checksum for the frame did not produce the expected 0xFFFF value,
 *	   then the transfer code would be set to 0x9.
 *		 Bit 48 to 63 - L4_Checksum[0:15] TCP/UDP checksum value that
 *	   arrived with the frame. If the resulting computed TCP/UDP checksum
 *	   for the frame did not produce the expected 0xFFFF value, then the
 *	   transfer code would be set to 0xA.
 * @control_1:Bits 0 to 1 - Reserved
 *	   Bits 2 to 15 - Buffer0_Size.This field is set by the host and
 *	   eventually overwritten by the adapter. The host writes the
 *	   available buffer size in bytes when it passes the descriptor to
 *	   the adapter. When a frame is delivered the host, the adapter
 *	   populates this field with the number of bytes written into the
 *	   buffer. The largest supported buffer is 16, 383 bytes.
 *		 Bit 16 to 47 - RTH Hash Value 32-bit RTH hash value. Only valid
 *		 if RTH_HASH_TYPE (Control_0, bits 20:23) is nonzero.
 *		 Bit 48 to 63 - VLAN_Tag[0:15] The contents of the variable
 *	   portion of the VLAN tag, if one was detected by the adapter.
 *	   This field is populated even if VLAN-tag stripping is enabled.
 * @buffer0_ptr: Pointer to buffer. This field is populated by the driver.
 *
 * One buffer mode RxD for ring structure
 */
typedef struct vxge_hal_ring_rxd_1_t {
	u64 host_control;
	u64 control_0;
#define	VXGE_HAL_RING_RXD_RTH_BUCKET_GET(ctrl0)		    bVAL7(ctrl0, 0)
#define	VXGE_HAL_RING_RXD_RTH_BUCKET_ADAPTER		    vBIT(val, 0, 7)

#define	VXGE_HAL_RING_RXD_LIST_OWN_GET(ctrl0)		    bVAL1(ctrl0, 7)
#define	VXGE_HAL_RING_RXD_LIST_OWN_ADAPTER		    mBIT(7)

#define	VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE_GET(ctrl0)	    bVAL1(ctrl0, 8)
#define	VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE		    mBIT(8)

#define	VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT_GET(ctrl0)	    bVAL1(ctrl0, 9)
#define	VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT		    mBIT(9)

#define	VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT_GET(ctrl0)	    bVAL1(ctrl0, 10)
#define	VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT		    mBIT(10)

#define	VXGE_HAL_RING_RXD_T_CODE_GET(ctrl0)		    bVAL4(ctrl0, 12)
#define	VXGE_HAL_RING_RXD_T_CODE(val)			    vBIT(val, 12, 4)
#define	VXGE_HAL_RING_RXD_T_CODE_OK			VXGE_HAL_RING_T_CODE_OK
#define	VXGE_HAL_RING_RXD_T_CODE_L3_CKSUM_MISMATCH		\
				    VXGE_HAL_RING_T_CODE_L3_CKSUM_MISMATCH
#define	VXGE_HAL_RING_RXD_T_CODE_L4_CKSUM_MISMATCH		\
				    VXGE_HAL_RING_T_CODE_L4_CKSUM_MISMATCH
#define	VXGE_HAL_RING_RXD_T_CODE_L3_L4_CKSUM_MISMATCH		\
				    VXGE_HAL_RING_T_CODE_L3_L4_CKSUM_MISMATCH
#define	VXGE_HAL_RING_RXD_T_CODE_L3_PKT_ERR	VXGE_HAL_RING_T_CODE_L3_PKT_ERR
#define	VXGE_HAL_RING_RXD_T_CODE_L2_FRM_ERR	VXGE_HAL_RING_T_CODE_L2_FRM_ERR
#define	VXGE_HAL_RING_RXD_T_CODE_BUF_SIZE_ERR			\
				    VXGE_HAL_RING_T_CODE_BUF_SIZE_ERR
#define	VXGE_HAL_RING_RXD_T_CODE_INT_ECC_ERR	VXGE_HAL_RING_T_CODE_INT_ECC_ERR
#define	VXGE_HAL_RING_RXD_T_CODE_BENIGN_OVFLOW			\
				    VXGE_HAL_RING_T_CODE_BENIGN_OVFLOW
#define	VXGE_HAL_RING_RXD_T_CODE_ZERO_LEN_BUFF			\
				    VXGE_HAL_RING_T_CODE_ZERO_LEN_BUFF
#define	VXGE_HAL_RING_RXD_T_CODE_FRM_DROP	VXGE_HAL_RING_T_CODE_FRM_DROP
#define	VXGE_HAL_RING_RXD_T_CODE_UNUSED		VXGE_HAL_RING_T_CODE_UNUSED
#define	VXGE_HAL_RING_RXD_T_CODE_MULTI_ERR	VXGE_HAL_RING_T_CODE_MULTI_ERR

#define	VXGE_HAL_RING_RXD_SYN_GET(ctrl0)		    bVAL1(ctrl0, 16)
#define	VXGE_HAL_RING_RXD_SYN				    mBIT(16)

#define	VXGE_HAL_RING_RXD_IS_ICMP_GET(ctrl0)		    bVAL1(ctrl0, 17)
#define	VXGE_HAL_RING_RXD_IS_ICMP			    mBIT(17)

#define	VXGE_HAL_RING_RXD_RTH_SPDM_HIT_GET(ctrl0)	    bVAL1(ctrl0, 18)
#define	VXGE_HAL_RING_RXD_RTH_SPDM_HIT			    mBIT(18)

#define	VXGE_HAL_RING_RXD_RTH_IT_HIT_GET(ctrl0)		    bVAL1(ctrl0, 19)
#define	VXGE_HAL_RING_RXD_RTH_IT_HIT			    mBIT(19)

#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_GET(ctrl0)	    bVAL4(ctrl0, 20)
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE(val)		    vBIT(val, 20, 4)
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_NONE	VXGE_HAL_RING_HASH_TYPE_NONE
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV4		\
						VXGE_HAL_RING_HASH_TYPE_TCP_IPV4
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV4		\
						VXGE_HAL_RING_HASH_TYPE_UDP_IPV4
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV4	VXGE_HAL_RING_HASH_TYPE_IPV4
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV6		\
						VXGE_HAL_RING_HASH_TYPE_TCP_IPV6
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV6		\
						VXGE_HAL_RING_HASH_TYPE_UDP_IPV6
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV6	VXGE_HAL_RING_HASH_TYPE_IPV6
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV6_EX \
					    VXGE_HAL_RING_HASH_TYPE_TCP_IPV6_EX
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV6_EX \
					    VXGE_HAL_RING_HASH_TYPE_UDP_IPV6_EX
#define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV6_EX	VXGE_HAL_RING_HASH_TYPE_IPV6_EX

#define	VXGE_HAL_RING_RXD_IS_VLAN_GET(ctrl0)		    bVAL1(ctrl0, 24)
#define	VXGE_HAL_RING_RXD_IS_VLAN			    mBIT(24)

#define	VXGE_HAL_RING_RXD_ETHER_ENCAP_GET(ctrl0)	    bVAL2(ctrl0, 25)
#define	VXGE_HAL_RING_RXD_ETHER_ENCAP(val)		    vBIT(val, 25, 2)
#define	VXGE_HAL_RING_RXD_ETHER_ENCAP_DIX		VXGE_HAL_FRAME_TYPE_DIX
#define	VXGE_HAL_RING_RXD_ETHER_ENCAP_LLC		VXGE_HAL_FRAME_TYPE_LLC
#define	VXGE_HAL_RING_RXD_ETHER_ENCAP_SNAP		VXGE_HAL_FRAME_TYPE_SNAP
#define	VXGE_HAL_RING_RXD_ETHER_ENCAP_IPX		VXGE_HAL_FRAME_TYPE_IPX

#define	VXGE_HAL_RING_RXD_IS_IPV4_GET(ctrl0)		    bVAL1(ctrl0, 27)
#define	VXGE_HAL_RING_RXD_IS_IPV4			    mBIT(27)

#define	VXGE_HAL_RING_RXD_IS_IPV6_GET(ctrl0)		    bVAL1(ctrl0, 28)
#define	VXGE_HAL_RING_RXD_IS_IPV6			    mBIT(28)

#define	VXGE_HAL_RING_RXD_IS_IPV_FRAG_GET(ctrl0)	    bVAL1(ctrl0, 29)
#define	VXGE_HAL_RING_RXD_IS_IPV_FRAG			    mBIT(29)

#define	VXGE_HAL_RING_RXD_IS_TCP_GET(ctrl0)		    bVAL1(ctrl0, 30)
#define	VXGE_HAL_RING_RXD_IS_TCP				mBIT(30)

#define	VXGE_HAL_RING_RXD_IS_UDP_GET(ctrl0)		    bVAL1(ctrl0, 31)
#define	VXGE_HAL_RING_RXD_IS_UDP				mBIT(31)

#define	VXGE_HAL_RING_RXD_FRAME_PROTO_GET(ctrl0)	    bVAL5(ctrl0, 27)
#define	VXGE_HAL_RING_RXD_FRAME_PROTO(val)		    vBIT(val, 27, 5)
#define	VXGE_HAL_RING_RXD_FRAME_PROTO_IPV4	    VXGE_HAL_FRAME_PROTO_IPV4
#define	VXGE_HAL_RING_RXD_FRAME_PROTO_IPV6	    VXGE_HAL_FRAME_PROTO_IPV6
#define	VXGE_HAL_RING_RXD_FRAME_PROTO_IP_FRAG	    VXGE_HAL_FRAME_PROTO_IP_FRAG
#define	VXGE_HAL_RING_RXD_FRAME_PROTO_TCP	    VXGE_HAL_FRAME_PROTO_TCP
#define	VXGE_HAL_RING_RXD_FRAME_PROTO_UDP	    VXGE_HAL_FRAME_PROTO_UDP
#define	VXGE_HAL_RING_RXD_FRAME_PROTO_TCP_OR_UDP    (VXGE_HAL_FRAME_PROTO_TCP |\
						    VXGE_HAL_FRAME_PROTO_UDP)

#define	VXGE_HAL_RING_RXD_L3_CKSUM_GET(ctrl0)		    bVAL16(ctrl0, 32)
#define	VXGE_HAL_RING_RXD_L3_CKSUM(val)			    vBIT(val, 32, 16)

#define	VXGE_HAL_RING_RXD_L4_CKSUM_GET(ctrl0)		    bVAL16(ctrl0, 48)
#define	VXGE_HAL_RING_RXD_L4_CKSUM(val)			    vBIT(val, 48, 16)

	u64 control_1;
#define	VXGE_HAL_RING_RXD_LIST_TAIL_OWN_ADAPTER			mBIT(0)

#define	VXGE_HAL_RING_RXD_1_BUFFER0_SIZE_GET(ctrl1)	    bVAL14(ctrl1, 2)
#define	VXGE_HAL_RING_RXD_1_BUFFER0_SIZE(val)		    vBIT(val, 2, 14)
#define	VXGE_HAL_RING_RXD_1_BUFFER0_SIZE_MASK		    vBIT(0x3FFF, 2, 14)

#define	VXGE_HAL_RING_RXD_1_RTH_HASH_VAL_GET(ctrl1)	    bVAL32(ctrl1, 16)
#define	VXGE_HAL_RING_RXD_1_RTH_HASH_VAL(val)		    vBIT(val, 16, 32)

#define	VXGE_HAL_RING_RXD_VLAN_TAG_GET(ctrl1)		    bVAL16(ctrl1, 48)
#define	VXGE_HAL_RING_RXD_VLAN_TAG(val)			    vBIT(val, 48, 16)

	u64 buffer0_ptr;

} vxge_hal_ring_rxd_1_t;

/*
 * struct vxge_hal_ring_rxd_3_t - Three buffer mode RxD for ring
 * @host_control: This field is exclusively for host use and is "readonly"
 *		from the adapter's perspective.
 * @control_0:Bits 0 to 6 - RTH_Bucket get
 *		 Bit 7 - Own Descriptor ownership bit. This bit is set to 1
 *		 by the host, and is set to 0 by the adapter.
 *		 0 - Host owns RxD and buffer.
 *		 1 - The adapter owns RxD and buffer.
 *		 Bit 8 - Fast_Path_Eligible When set, indicates that the
 *	    received frame meets all of the criteria for fast path processing.
 *	   The required criteria are as follows:
 *	   !SYN &
 *	   (Transfer_Code == "Transfer OK") &
 *	   (!Is_IP_Fragment) &
 *	   ((Is_IPv4 & computed_L3_checksum == 0xFFFF) |
 *	   (Is_IPv6)) &
 *	   ((Is_TCP & computed_L4_checksum == 0xFFFF) |
 *	   (Is_UDP & (computed_L4_checksum == 0xFFFF |
 *	   computed _L4_checksum == 0x0000)))
 *	   (same meaning for all RxD buffer modes)
 *		 Bit 9 - L3 Checksum Correct
 *		 Bit 10 - L4 Checksum Correct
 *		 Bit 11 - Reserved
 *		 Bit 12 to 15 - This field is written by the adapter. It is used
 *		 to report the status of the frame transfer to the host.
 *		 0x0 - Transfer OK
 *		 0x4 - RDA Failure During Transfer
 *		 0x5 - Unparseable Packet, such as unknown IPv6 header.
 *		 0x6 - Frame integrity error (FCS or ECC).
 *		 0x7 - Buffer Size Error. The provided buffer(s) were not
 *		 appropriately sized and data loss occurred.
 *		 0x8 - Internal ECC Error. RxD corrupted.
 *		 0x9 - IPv4 Checksum error
 *		 0xA - TCP/UDP Checksum error
 *		 0xF - Unknown Error or Multiple Error. Indicates an unknown
 *		 problem or that more than one of transfer codes is set.
 *		 Bit 16 - SYN The adapter sets this field to indicate that the
 *		 incoming frame contained a TCP segment with its SYN bit set
 *	   and its ACK bit NOT set. (same meaning for all RxD buffer modes)
 *		 Bit 17 - Is ICMP
 *		 Bit 18 - RTH_SPDM_HIT Set to 1 if there was a match in the
 *	   Socket
 *	   Pair Direct Match Table and the frame was steered based on SPDM.
 *		 Bit 19 - RTH_IT_HIT Set to 1 if there was a match in the
 *	   Indirection Table and the frame was steered based on hash
 *	   indirection.
 *		 Bit 20 to 23 - RTH_HASH_TYPE Indicates the function (hash type)
 *	   that was used to calculate the hash.
 *		 Bit 19 - IS_VLAN Set to '1' if the frame was/is VLAN tagged.
 *		 Bit 25 to 26 - ETHER_ENCAP Reflects the Ethernet encapsulation
 *	   of the received frame.
 *		 0x0 - Ethernet DIX
 *		 0x1 - LLC
 *		 0x2 - SNAP (includes Jumbo-SNAP)
 *		 0x3 - IPX
 *		 Bit 27 - IS_IPV4 Set to '1' if the frame contains IPv4 packet.
 *		 Bit 28 - IS_IPV6 Set to '1' if the frame contains IPv6 packet.
 *		 Bit 29 - IS_IP_FRAG Set to '1' if the frame contains a
 *	   fragmented IP packet.
 *		 Bit 30 - IS_TCP Set to '1' if the frame contains a TCP segment.
 *		 Bit 31 - IS_UDP Set to '1' if the frame contains a UDP message.
 *		 Bit 32 to 47 - L3_Checksum[0:15] The IPv4 checksum value that
 *	   arrived with the frame. If the resulting computed IPv4 header
 *	   checksum for the frame did not produce the expected 0xFFFF value,
 *	   then the transfer code would be set to 0x9.
 *		 Bit 48 to 63 - L4_Checksum[0:15] TCP/UDP checksum value that
 *	   arrived with the frame. If the resulting computed TCP/UDP checksum
 *	   for the frame did not produce the expected 0xFFFF value, then the
 *	   transfer code would be set to 0xA.
 * @control_1:Bit 0 - This field must be used in conjunction with the Ownership
 *	   field (above).
 *	   1 - Set by the host to indicate that the RxD points to fresh
 *	   buffers.
 *	   0 - Cleared by the adapter to indicate that frame data has been
 *	   placed into the assigned buffers, and that the host once again
 *	   owns the descriptor.
 *	   (Note: Please observe the usage guidelines outlined in the
 *	   Ownership field (above)).
 *		 Bit 1  - Unused. Ignored by Adapter on RxD read. Set to 0
 *		 by Adapter on RxD write.
 *	   Bits 2 to 15 - This field is written by the host and by X3100.
 *	   The host writes the available buffer 0 size in bytes when it
 *	   passes the descriptor to the X3100. The X3100 writes the number
 *	   of bytes written to the buffer when it passes the descriptor back
 *	   to the host.
 *		 Bits 16 to 17 - Reserved
 *		 Bits 18 to 31 - This field is set by the host and eventually
 *		 overwritten by the adapter. The host writes the available
 *	   buffer 1size in bytes when it passes the descriptor to the adapter.
 *	   When a frame is delivered the host, the adapter populates this field
 *	   with the number of bytes written into the buffer 1. The largest
 *	   supported buffer is 16, 383 bytes.
 *		 Bits 32 to 33 - Reserved
 *		 Bits 34 to 47 - This field is set by the host and eventually
 *		 overwritten by the adapter. The host writes the available
 *	   buffer 2 size in bytes when it passes the descriptor to the adapter.
 *	   When a frame is delivered the host, the adapter populates this field
 *	   with the number of bytes written into the buffer 2. The largest
 *	   supported buffer is 16, 383 bytes.
 *		 Bit 48 to 63 - VLAN_Tag[0:15] The contents of the variable
 *	   portion of the VLAN tag, if one was detected by the adapter. This
 *	   field is populated even if VLAN-tag stripping is enabled.
 * @buffer0_ptr: Pointer to buffer 0. This field is populated by the driver.
 *	   In 3-buffer mode, when the RxD is returned to the host,
 *	   buffer0_ptr field will be overwritten if the following conditions
 *	   are met:
 *	   1 - RTH_Disable in the PRC_CTRL register is not set.
 *	   2 - RTH is enabled and a valid hash value was calculated for the
 *	   frame. This will be indicated by a non-zero value in the
 *	   RTH_HASH_TYPE field (Control_0, bits 20:23). In the event that the
 *	   pointer is overwritten on return to the host, bits 0:31 will be
 *	   all zeroes while bits 32:63 will contain the calculated hash value.
 * @buffer1_ptr: Pointer to buffer 1. This field is populated by the driver.
 * @buffer2_ptr: Pointer to buffer 2. This field is populated by the driver.
 *
 * Three buffer mode RxD for ring structure
 */
typedef struct vxge_hal_ring_rxd_3_t {
	u64 host_control;
	u64 control_0;
/*
 * The following bit fields are common in all the three buffer modes and are
 * defined in vxge_hal_ring_rxd_1_t
 * #define	VXGE_HAL_RING_RXD_RTH_BUCKET_GET(ctrl0)	    bVAL7(ctrl0, 0)
 * #define	VXGE_HAL_RING_RXD_RTH_BUCKET_ADAPTER	    vBIT(val, 0, 7)
 *
 * #define	VXGE_HAL_RING_RXD_LIST_OWN_GET(ctrl0)	    bVAL1(ctrl0, 7)
 * #define	VXGE_HAL_RING_RXD_LIST_OWN_ADAPTER	    mBIT(7)
 *
 * #define	VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE_GET(ctrl0)		\
 *							    bVAL1(ctrl0, 8)
 * #define	VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE	    mBIT(8)
 *
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT_GET(ctrl0)		\
 *							    bVAL1(ctrl0, 9)
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT	    mBIT(9)
 *
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT_GET(ctrl0)		\
 *							    bVAL1(ctrl0, 10)
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT	    mBIT(10)
 *
 * #define	VXGE_HAL_RING_RXD_T_CODE_GET(ctrl0)	    bVAL4(ctrl0, 12)
 * #define	VXGE_HAL_RING_RXD_T_CODE(val)		    vBIT(val, 12, 4)
 * #define	VXGE_HAL_RING_RXD_T_CODE_OK	    VXGE_HAL_RING_T_CODE_OK
 * #define	VXGE_HAL_RING_RXD_T_CODE_L3_CKSUM_MISMATCH		\
 *				VXGE_HAL_RING_T_CODE_L3_CKSUM_MISMATCH
 * #define	VXGE_HAL_RING_RXD_T_CODE_L4_CKSUM_MISMATCH		\
 *				VXGE_HAL_RING_T_CODE_L4_CKSUM_MISMATCH
 * #define	VXGE_HAL_RING_RXD_T_CODE_L3_L4_CKSUM_MISMATCH		\
 *				VXGE_HAL_RING_T_CODE_L3_L4_CKSUM_MISMATCH
 * #define	VXGE_HAL_RING_RXD_T_CODE_L3_PKT_ERR			\
 *				VXGE_HAL_RING_T_CODE_L3_PKT_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_L2_FRM_ERR			\
 *				VXGE_HAL_RING_T_CODE_L2_FRM_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_BUF_SIZE_ERR			\
 *				VXGE_HAL_RING_T_CODE_BUF_SIZE_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_INT_ECC_ERR			\
 *				VXGE_HAL_RING_T_CODE_INT_ECC_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_BENIGN_OVFLOW			\
 *				VXGE_HAL_RING_T_CODE_BENIGN_OVFLOW
 * #define	VXGE_HAL_RING_RXD_T_CODE_ZERO_LEN_BUFF			\
 *				VXGE_HAL_RING_T_CODE_ZERO_LEN_BUFF
 * #define	VXGE_HAL_RING_RXD_T_CODE_FRM_DROP VXGE_HAL_RING_T_CODE_FRM_DROP
 * #define	VXGE_HAL_RING_RXD_T_CODE_UNUSED	  VXGE_HAL_RING_T_CODE_UNUSED
 * #define	VXGE_HAL_RING_RXD_T_CODE_MULTI_ERR			\
 *				VXGE_HAL_RING_T_CODE_MULTI_ERR
 *
 * #define	VXGE_HAL_RING_RXD_SYN_GET(ctrl0)	    bVAL1(ctrl0, 16)
 * #define	VXGE_HAL_RING_RXD_SYN			    mBIT(16)
 *
 * #define	VXGE_HAL_RING_RXD_IS_ICMP_GET(ctrl0)	    bVAL1(ctrl0, 17)
 * #define	VXGE_HAL_RING_RXD_IS_ICMP		    mBIT(17)
 *
 * #define	VXGE_HAL_RING_RXD_RTH_SPDM_HIT_GET(ctrl0)   bVAL1(ctrl0, 18)
 * #define	VXGE_HAL_RING_RXD_RTH_SPDM_HIT		    mBIT(18)
 *
 * #define	VXGE_HAL_RING_RXD_RTH_IT_HIT_GET(ctrl0)	    bVAL1(ctrl0, 19)
 * #define	VXGE_HAL_RING_RXD_RTH_IT_HIT		    mBIT(19)
 *
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_GET(ctrl0)  bVAL4(ctrl0, 20)
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE(val)	    vBIT(val, 20, 4)
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_NONE			\
 *				VXGE_HAL_RING_HASH_TYPE_NONE
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV4		\
 *				VXGE_HAL_RING_HASH_TYPE_TCP_IPV4
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV4		\
 *				VXGE_HAL_RING_HASH_TYPE_UDP_IPV4
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV4			\
 *				VXGE_HAL_RING_HASH_TYPE_IPV4
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV6		\
 *				VXGE_HAL_RING_HASH_TYPE_TCP_IPV6
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV6		\
 *				VXGE_HAL_RING_HASH_TYPE_UDP_IPV6
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV6			\
 *				VXGE_HAL_RING_HASH_TYPE_IPV6
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV6_EX		\
 *				VXGE_HAL_RING_HASH_TYPE_TCP_IPV6_EX
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV6_EX		\
 *				VXGE_HAL_RING_HASH_TYPE_UDP_IPV6_EX
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV6_EX			\
 *				VXGE_HAL_RING_HASH_TYPE_IPV6_EX
 *
 * #define	VXGE_HAL_RING_RXD_IS_VLAN_GET(ctrl0)	    bVAL1(ctrl0, 24)
 * #define	VXGE_HAL_RING_RXD_IS_VLAN		    mBIT(24)
 *
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_GET(ctrl0)    bVAL2(ctrl0, 25)
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP(val)	    vBIT(val, 25, 2)
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_DIX	VXGE_HAL_FRAME_TYPE_DIX
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_LLC	VXGE_HAL_FRAME_TYPE_LLC
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_SNAP	VXGE_HAL_FRAME_TYPE_SNAP
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_IPX	VXGE_HAL_FRAME_TYPE_IPX
 *
 * #define	VXGE_HAL_RING_RXD_IS_IPV4_GET(ctrl0)	    bVAL1(ctrl0, 27)
 * #define	VXGE_HAL_RING_RXD_IS_IPV4		    mBIT(27)
 *
 * #define	VXGE_HAL_RING_RXD_IS_IPV6_GET(ctrl0)	    bVAL1(ctrl0, 28)
 * #define	VXGE_HAL_RING_RXD_IS_IPV6		    mBIT(28)
 *
 * #define	VXGE_HAL_RING_RXD_IS_IPV_FRAG_GET(ctrl0)    bVAL1(ctrl0, 29)
 * #define	VXGE_HAL_RING_RXD_IS_IPV_FRAG		    mBIT(29)
 *
 * #define	VXGE_HAL_RING_RXD_IS_TCP_GET(ctrl0)	    bVAL1(ctrl0, 30)
 * #define	VXGE_HAL_RING_RXD_IS_TCP		    mBIT(30)
 *
 * #define	VXGE_HAL_RING_RXD_IS_UDP_GET(ctrl0)	    bVAL1(ctrl0, 31)
 * #define	VXGE_HAL_RING_RXD_IS_UDP		    mBIT(31)
 *
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_GET(ctrl0)    bVAL5(ctrl0, 27)
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO(val)	    vBIT(val, 27, 5)
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_IPV4		    \
 *				VXGE_HAL_FRAME_PROTO_IPV4
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_IPV6		    \
 *				VXGE_HAL_FRAME_PROTO_IPV6
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_IP_FRAG		    \
 *				VXGE_HAL_FRAME_PROTO_IP_FRAG
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_TCP		    \
 *				VXGE_HAL_FRAME_PROTO_TCP
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_UDP		    \
 *				VXGE_HAL_FRAME_PROTO_UDP
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_TCP_OR_UDP	    \
 *			(VXGE_HAL_FRAME_PROTO_TCP | VXGE_HAL_FRAME_PROTO_UDP)
 *
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM_GET(ctrl0)	    bVAL16(ctrl0, 32)
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM(val)		    vBIT(val, 32, 16)
 *
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM_GET(ctrl0)	    bVAL16(ctrl0, 48)
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM(val)		    vBIT(val, 48, 16)
 */

	u64 control_1;
#define	VXGE_HAL_RING_RXD_3_BUFFER_EMPTY_GET(ctrl1)	    bVAL1(ctrl1, 0)
#define	VXGE_HAL_RING_RXD_3_BUFFER_EMPTY		    mBIT(0)

#define	VXGE_HAL_RING_RXD_3_BUFFER0_SIZE_GET(ctrl1)	    bVAL14(ctrl1, 2)
#define	VXGE_HAL_RING_RXD_3_BUFFER0_SIZE(val)		    vBIT(val, 2, 14)
#define	VXGE_HAL_RING_RXD_3_BUFFER0_SIZE_MASK		    vBIT(0x3FFc, 2, 14)

#define	VXGE_HAL_RING_RXD_3_BUFFER1_SIZE_GET(ctrl1)	    bVAL14(ctrl1, 18)
#define	VXGE_HAL_RING_RXD_3_BUFFER1_SIZE(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RING_RXD_3_BUFFER1_SIZE_MASK		    vBIT(0x3FFc, 18, 14)

#define	VXGE_HAL_RING_RXD_3_BUFFER2_SIZE_GET(ctrl1)	    bVAL14(ctrl1, 34)
#define	VXGE_HAL_RING_RXD_3_BUFFER2_SIZE(val)		    vBIT(val, 34, 14)
#define	VXGE_HAL_RING_RXD_3_BUFFER2_SIZE_MASK		    vBIT(0x3FFc, 34, 14)

/*
 * The following bit fields are common in all the three buffer modes and are
 * defined in vxge_hal_ring_rxd_1_t
 * #define	VXGE_HAL_RING_RXD_VLAN_TAG_GET(ctrl1)	    bVAL16(ctrl1, 48)
 * #define	VXGE_HAL_RING_RXD_VLAN_TAG(val)		    vBIT(val, 48, 16)
 */
	u64 buffer0_ptr;
#define	VXGE_HAL_RING_RXD_3_RTH_HASH_VALUE_GET(b0_ptr)	    bVAL32(b0_ptr, 32)
#define	VXGE_HAL_RING_RXD_3_RTH_HASH_VALUE(val)		    vBIT(val, 32, 32)

	u64 buffer1_ptr;

	u64 buffer2_ptr;

} vxge_hal_ring_rxd_3_t;

/*
 * struct vxge_hal_ring_rxd_5_t - Five buffer mode RxD for ring
 * @host_control: This 32 bitfield is exclusively for host use and is "readonly"
 *		from the adapter's perspective.
 * @control_2: Bits 0 to 1 - Reserved
 *	       Bits 2 to 15 - This field is set by the host and eventually
 *		overwritten by the adapter. The host writes the available buffer
 *		3 size in bytes when it pas ses the descriptor to the adapter.
 *		When a frame is delivered the host, the adapter populates this
 *		field with the number of bytes written into the buffer. The
 *		largest supported buffer is 16, 383 bytes.
 *	       Bits 16 to 17 - Reserved
 *	       Bits 18 to 31 - This field is set by the host and eventually
 *		overwritten by the adapter. The host writes the available buffer
 *		4 size in bytes when it passes the descriptor to the adapter.
 *		When a frame is delivered the host, the adapter populates this
 *		field with the number of bytes written into the buffer.
 *		The largest supported buffer is 16, 383 bytes.
 * @control_0: Bits 0 to 6 - RTH_Bucket get
 *		 Bit 7 - Own Descriptor ownership bit. This bit is set to 1 by
 *		 the host, and is set to 0 by the adapter.
 *		 0 - Host owns RxD and buffer.
 *		 1 - The adapter owns RxD and buffer.
 *		 Bit 8 - Fast_Path_Eligible When set,indicates that the received
 *	   frame meets all of the criteria for fast path processing.
 *	   The required criteria are as follows:
 *	   !SYN &
 *	   (Transfer_Code == "Transfer OK") &
 *	   (!Is_IP_Fragment) &
 *	   ((Is_IPv4 & computed_L3_checksum == 0xFFFF) |
 *	   (Is_IPv6)) &
 *	   ((Is_TCP & computed_L4_checksum == 0xFFFF) |
 *	   (Is_UDP & (computed_L4_checksum == 0xFFFF |
 *	   computed _L4_checksum == 0x0000)))
 *	   (same meaning for all RxD buffer modes)
 *		 Bit 9 - L3 Checksum Correct
 *		 Bit 10 - L4 Checksum Correct
 *		 Bit 11 - Reserved
 *		 Bit 12 to 15 - This field is written by the adapter. It is used
 *		 to report the status of the frame transfer to the host.
 *		 0x0 - Transfer OK
 *		 0x4 - RDA Failure During Transfer
 *		 0x5 - Unparseable Packet, such as unknown IPv6 header.
 *		 0x6 - Frame integrity error (FCS or ECC).
 *		 0x7 - Buffer Size Error. The provided buffer(s) were not
 *		 appropriately sized and data loss occurred.
 *		 0x8 - Internal ECC Error. RxD corrupted.
 *		 0x9 - IPv4 Checksum error
 *		 0xA - TCP/UDP Checksum error
 *		 0xF - Unknown Error or Multiple Error. Indicates an unknown
 *		 problem or that more than one of transfer codes is set.
 *		 Bit 16 - SYN The adapter sets this field to indicate that the
 *		 incoming frame contained a TCP segment with its SYN bit set
 *	   and its ACK bit NOT set. (same meaning for all RxD buffer modes)
 *		 Bit 17 - Is ICMP
 *		 Bit 18 - RTH_SPDM_HIT Set to 1 if there was a match in the
 *	   Socket Pair Direct Match Table and the frame was steered based on
 *	   SPDM.
 *		 Bit 19 - RTH_IT_HIT Set to 1 if there was a match in the
 *	   Indirection Table and the frame was steered based on hash
 *	   indirection.
 *		 Bit 20 to 23 - RTH_HASH_TYPE Indicates the function (hash type)
 *	   that was used to calculate the hash.
 *		 Bit 19 - IS_VLAN Set to '1' if the frame was/is VLAN tagged.
 *		 Bit 25 to 26 - ETHER_ENCAP Reflects the Ethernet encapsulation
 *	   of the received frame.
 *		 0x0 - Ethernet DIX
 *		 0x1 - LLC
 *		 0x2 - SNAP (includes Jumbo-SNAP)
 *		 0x3 - IPX
 *		 Bit 27 - IS_IPV4 Set to '1' if the frame contains IPv4 packet.
 *		 Bit 28 - IS_IPV6 Set to '1' if the frame contains IPv6 packet.
 *		 Bit 29 - IS_IP_FRAG Set to '1' if the frame contains a
 *	   fragmented IP packet.
 *		 Bit 30 - IS_TCP Set to '1' if the frame contains a TCP segment.
 *		 Bit 31 - IS_UDP Set to '1' if the frame contains a UDP message.
 *		 Bit 32 to 47 - L3_Checksum[0:15] The IPv4 checksum value that
 *	   arrived with the frame. If the resulting computed IPv4 header
 *	   checksum for the frame did not produce the expected 0xFFFF value,
 *	   then the transfer code would be set to 0x9.
 *		 Bit 48 to 63 - L4_Checksum[0:15] TCP/UDP checksum value that
 *	   arrived with the frame. If the resulting computed TCP/UDP checksum
 *	   for the frame did not produce the expected 0xFFFF value, then the
 *	   transfer code would be set to 0xA.
 * @control_1: Bits 0 to 1 - Reserved.
 *	   Bits 2 to 15 - This field is written by the host and by X3100.
 *	   The host writes the available buffer 0 size in bytes when it
 *	   passes the descriptor to the X3100. The X3100 writes the number
 *	   of bytes written to the buffer when it passes the descriptor back
 *	   to the host.
 *		 Bits 16 to 17 - Reserved
 *		 Bits 18 to 31 - This field is set by the host and eventually
 *		 overwritten by the adapter. The host writes the available
 *	   buffer 1 size in bytes when it passes the descriptor to the adapter.
 *	   When a frame is delivered the host, the adapter populates this field
 *	   with the number of bytes written into the buffer 1. The largest
 *	   supported buffer is 16, 383 bytes.
 *		 Bits 32 to 33 - Reserved
 *		 Bits 34 to 47 - This field is set by the host and eventually
 *		 overwritten by the adapter. The host writes the available
 *	   buffer 2 size in bytes when it passes the descriptor to the adapter.
 *	   When a frame is delivered the host, the adapter populates this field
 *	   with the number of bytes written into the buffer 2. The largest
 *	   supported buffer is 16, 383 bytes.
 *		 Bit 48 to 63 - VLAN_Tag[0:15] The contents of the variable
 *	   portion of the VLAN tag, if one was detected by the adapter. This
 *	   field is populated even if VLAN-tag stripping is enabled.
 * @buffer0_ptr: Pointer to buffer 0. This field is populated by the driver.
 *	   In 5-buffer mode, when the RxD is returned to the host,
 *	   buffer0_ptr field will be overwritten if the following conditions
 *	   are met:
 *	   1 - RTH_Disable in the PRC_CTRL register is not set.
 *	   2 - RTH is enabled and a valid hash value was calculated for the
 *	   frame. This will be indicated by a non-zero value in the
 *	   RTH_HASH_TYPE field (Control_0, bits 20:23). In the event that the
 *	   pointer is overwritten on return to the host, bits 0:31 will be
 *	   all zeroes while bits 32:63 will contain the calculated hash value.
 * @buffer1_ptr: Pointer to buffer 1. This field is populated by the driver.
 * @buffer2_ptr: Pointer to buffer 2. This field is populated by the driver.
 * @buffer3_ptr: Pointer to buffer 3. This field is populated by the driver.
 * @buffer4_ptr: Pointer to buffer 4. This field is populated by the driver.
 * @pad: Pad to align at cache line boundary
 *
 * Three buffer mode RxD for ring structure
 */
typedef struct vxge_hal_ring_rxd_5_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u32 host_control;
	u32 control_2;
#else
	u32 control_2;
	u32 host_control;
#endif

#define	VXGE_HAL_RING_RXD_5_BUFFER3_SIZE_GET(ctrl2)	    bVAL14(ctrl2, 34)
#define	VXGE_HAL_RING_RXD_5_BUFFER3_SIZE(val)		    vBIT(val, 34, 14)
#define	VXGE_HAL_RING_RXD_5_BUFFER3_SIZE_MASK		    vBIT(0x3FFF, 34, 14)

#define	VXGE_HAL_RING_RXD_5_BUFFER4_SIZE_GET(ctrl2)	    bVAL14(ctrl2, 50)
#define	VXGE_HAL_RING_RXD_5_BUFFER4_SIZE(val)		    vBIT(val, 50, 14)
#define	VXGE_HAL_RING_RXD_5_BUFFER4_SIZE_MASK		    vBIT(0x3FFF, 50, 14)


	u64 control_0;
/*
 * The following bit fields are common in all the three buffer modes and are
 * defined in vxge_hal_ring_rxd_1_t
 * #define	VXGE_HAL_RING_RXD_RTH_BUCKET_GET(ctrl0)	    bVAL7(ctrl0, 0)
 * #define	VXGE_HAL_RING_RXD_RTH_BUCKET_ADAPTER	    vBIT(val, 0, 7)
 *
 * #define	VXGE_HAL_RING_RXD_LIST_OWN_GET(ctrl0)	    bVAL1(ctrl0, 7)
 * #define	VXGE_HAL_RING_RXD_LIST_OWN_ADAPTER	    mBIT(7)
 *
 * #define	VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE_GET(ctrl0		\
 *				bVAL1(ctrl0, 8)
 * #define	VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE	    mBIT(8)
 *
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT_GET(ctrl0)		\
 *				bVAL1(ctrl0, 9)
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT	    mBIT(9)
 *
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT_GET(ctrl0)		\
 *				bVAL1(ctrl0, 10)
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT	    mBIT(10)
 *
 * #define	VXGE_HAL_RING_RXD_T_CODE_GET(ctrl0)	    bVAL4(ctrl0, 12)
 * #define	VXGE_HAL_RING_RXD_T_CODE(val)		    vBIT(val, 12, 4)
 * #define	VXGE_HAL_RING_RXD_T_CODE_OK				\
 *				VXGE_HAL_RING_T_CODE_OK
 * #define	VXGE_HAL_RING_RXD_T_CODE_L3_CKSUM_MISMATCH		\
 *				VXGE_HAL_RING_T_CODE_L3_CKSUM_MISMATCH
 * #define	VXGE_HAL_RING_RXD_T_CODE_L4_CKSUM_MISMATCH		\
 *				VXGE_HAL_RING_T_CODE_L4_CKSUM_MISMATCH
 * #define	VXGE_HAL_RING_RXD_T_CODE_L3_L4_CKSUM_MISMATCH		\
 *				VXGE_HAL_RING_T_CODE_L3_L4_CKSUM_MISMATCH
 * #define	VXGE_HAL_RING_RXD_T_CODE_L3_PKT_ERR			\
 *				VXGE_HAL_RING_T_CODE_L3_PKT_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_L2_FRM_ERR			\
 *				VXGE_HAL_RING_T_CODE_L2_FRM_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_BUF_SIZE_ERR			\
 *				VXGE_HAL_RING_T_CODE_BUF_SIZE_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_INT_ECC_ERR			\
 *				VXGE_HAL_RING_T_CODE_INT_ECC_ERR
 * #define	VXGE_HAL_RING_RXD_T_CODE_BENIGN_OVFLOW			\
 *				VXGE_HAL_RING_T_CODE_BENIGN_OVFLOW
 * #define	VXGE_HAL_RING_RXD_T_CODE_ZERO_LEN_BUFF			\
 *				VXGE_HAL_RING_T_CODE_ZERO_LEN_BUFF
 * #define	VXGE_HAL_RING_RXD_T_CODE_FRM_DROP			\
 *				VXGE_HAL_RING_T_CODE_FRM_DROP
 * #define	VXGE_HAL_RING_RXD_T_CODE_UNUSED				\
 *				VXGE_HAL_RING_T_CODE_UNUSED
 * #define	VXGE_HAL_RING_RXD_T_CODE_MULTI_ERR			\
 *				VXGE_HAL_RING_T_CODE_MULTI_ERR
 *
 * #define	VXGE_HAL_RING_RXD_SYN_GET(ctrl0)	    bVAL1(ctrl0, 16)
 * #define	VXGE_HAL_RING_RXD_SYN			    mBIT(16)
 *
 * #define	VXGE_HAL_RING_RXD_IS_ICMP_GET(ctrl0)	    bVAL1(ctrl0, 17)
 * #define	VXGE_HAL_RING_RXD_IS_ICMP		    mBIT(17)
 *
 * #define	VXGE_HAL_RING_RXD_RTH_SPDM_HIT_GET(ctrl0)   bVAL1(ctrl0, 18)
 * #define	VXGE_HAL_RING_RXD_RTH_SPDM_HIT		    mBIT(18)
 *
 * #define	VXGE_HAL_RING_RXD_RTH_IT_HIT_GET(ctrl0)	    bVAL1(ctrl0, 19)
 * #define	VXGE_HAL_RING_RXD_RTH_IT_HIT		    mBIT(19)
 *
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_GET(ctrl0)  bVAL4(ctrl0, 20)
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE(val)	    vBIT(val, 20, 4)
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_NONE		    \
 *				VXGE_HAL_RING_HASH_TYPE_NONE
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV4	    \
 *				VXGE_HAL_RING_HASH_TYPE_TCP_IPV4
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV4	    \
 *				VXGE_HAL_RING_HASH_TYPE_UDP_IPV4
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV4		    \
 *				VXGE_HAL_RING_HASH_TYPE_IPV4
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV6	    \
 *				VXGE_HAL_RING_HASH_TYPE_TCP_IPV6
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV6	    \
 *				VXGE_HAL_RING_HASH_TYPE_UDP_IPV6
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV6		    \
 *				VXGE_HAL_RING_HASH_TYPE_IPV6
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_TCP_IPV6_EX	    \
 *				VXGE_HAL_RING_HASH_TYPE_TCP_IPV6_EX
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_UDP_IPV6_EX	    \
 *				VXGE_HAL_RING_HASH_TYPE_UDP_IPV6_EX
 * #define	VXGE_HAL_RING_RXD_RTH_HASH_TYPE_IPV6_EX		    \
 *				VXGE_HAL_RING_HASH_TYPE_IPV6_EX
 *
 * #define	VXGE_HAL_RING_RXD_IS_VLAN_GET(ctrl0)	    bVAL1(ctrl0, 24)
 * #define	VXGE_HAL_RING_RXD_IS_VLAN		    mBIT(24)
 *
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_GET(ctrl0)    bVAL2(ctrl0, 25)
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP(val)	    vBIT(val, 25, 2)
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_DIX   VXGE_HAL_FRAME_TYPE_DIX
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_LLC   VXGE_HAL_FRAME_TYPE_LLC
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_SNAP  VXGE_HAL_FRAME_TYPE_SNAP
 * #define	VXGE_HAL_RING_RXD_ETHER_ENCAP_IPX   VXGE_HAL_FRAME_TYPE_IPX
 *
 * #define	VXGE_HAL_RING_RXD_IS_IPV4_GET(ctrl0)	    bVAL1(ctrl0, 27)
 * #define	VXGE_HAL_RING_RXD_IS_IPV4		    mBIT(27)
 *
 * #define	VXGE_HAL_RING_RXD_IS_IPV6_GET(ctrl0)	    bVAL1(ctrl0, 28)
 * #define	VXGE_HAL_RING_RXD_IS_IPV6		    mBIT(28)
 *
 * #define	VXGE_HAL_RING_RXD_IS_IPV_FRAG_GET(ctrl0)    bVAL1(ctrl0, 29)
 * #define	VXGE_HAL_RING_RXD_IS_IPV_FRAG		    mBIT(29)
 *
 * #define	VXGE_HAL_RING_RXD_IS_TCP_GET(ctrl0)	    bVAL1(ctrl0, 30)
 * #define	VXGE_HAL_RING_RXD_IS_TCP		    mBIT(30)
 *
 * #define	VXGE_HAL_RING_RXD_IS_UDP_GET(ctrl0)	    bVAL1(ctrl0, 31)
 * #define	VXGE_HAL_RING_RXD_IS_UDP		    mBIT(31)
 *
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_GET(ctrl0)    bVAL5(ctrl0, 27)
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO(val)	    vBIT(val, 27, 5)
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_IPV4  VXGE_HAL_FRAME_PROTO_IPV4
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_IPV6  VXGE_HAL_FRAME_PROTO_IPV6
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_IP_FRAG		\
 *				VXGE_HAL_FRAME_PROTO_IP_FRAG
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_TCP   VXGE_HAL_FRAME_PROTO_TCP
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_UDP   VXGE_HAL_FRAME_PROTO_UDP
 * #define	VXGE_HAL_RING_RXD_FRAME_PROTO_TCP_OR_UDP	\
 *			(VXGE_HAL_FRAME_PROTO_TCP | VXGE_HAL_FRAME_PROTO_UDP)
 *
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM_GET(ctrl0)	    bVAL16(ctrl0, 32)
 * #define	VXGE_HAL_RING_RXD_L3_CKSUM(val)		    vBIT(val, 32, 16)
 *
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM_GET(ctrl0)	    bVAL16(ctrl0, 48)
 * #define	VXGE_HAL_RING_RXD_L4_CKSUM(val)		    vBIT(val, 48, 16)
 */

	u64 control_1;

#define	VXGE_HAL_RING_RXD_5_BUFFER0_SIZE_GET(ctrl1)	    bVAL14(ctrl1, 2)
#define	VXGE_HAL_RING_RXD_5_BUFFER0_SIZE(val)		    vBIT(val, 2, 14)
#define	VXGE_HAL_RING_RXD_5_BUFFER0_SIZE_MASK		    vBIT(0x3FFF, 2, 14)

#define	VXGE_HAL_RING_RXD_5_BUFFER1_SIZE_GET(ctrl1)	    bVAL14(ctrl1, 18)
#define	VXGE_HAL_RING_RXD_5_BUFFER1_SIZE(val)		    vBIT(val, 18, 14)
#define	VXGE_HAL_RING_RXD_5_BUFFER1_SIZE_MASK		    vBIT(0x3FFF, 18, 14)

#define	VXGE_HAL_RING_RXD_5_BUFFER2_SIZE_GET(ctrl1)	    bVAL14(ctrl1, 34)
#define	VXGE_HAL_RING_RXD_5_BUFFER2_SIZE(val)		    vBIT(val, 34, 14)
#define	VXGE_HAL_RING_RXD_5_BUFFER2_SIZE_MASK		    vBIT(0xFFFF, 34, 14)

/*
 * The following bit fields are common in all the three buffer modes and are
 * defined in vxge_hal_ring_rxd_1_t
 * #define	VXGE_HAL_RING_RXD_VLAN_TAG_GET(ctrl1)	    bVAL16(ctrl1, 48)
 * #define	VXGE_HAL_RING_RXD_VLAN_TAG(val)		    vBIT(val, 48, 16)
 */

	u64 buffer0_ptr;
#define	VXGE_HAL_RING_RXD_5_RTH_HASH_VALUE_GET(b0_ptr)	    bVAL32(b0_ptr, 32)
#define	VXGE_HAL_RING_RXD_5_RTH_HASH_VALUE(val)		    vBIT(val, 32, 32)

	u64 buffer1_ptr;
	u64 buffer2_ptr;
	u64 buffer3_ptr;
	u64 buffer4_ptr;
} vxge_hal_ring_rxd_5_t;

/*
 * function vxge_hal_ring_callback_f - Ring callback.
 * @vpath_handle: Virtual Path whose Ring "containing" 1 or more completed
 *		descriptors.
 * @rxdh: First completed descriptor.
 * @rxd_priv: Pointer to per rxd space allocated
 * @t_code: Transfer code, as per X3100 User Guide.
 *	 Returned by HAL.
 * @userdata: Opaque per-ring data specified at ring open
 *	   time, via vxge_hal_vpath_open().
 *
 * ring completion callback (type declaration). A single per-ring
 * callback is specified at virtual path open time, via
 * vxge_hal_vpath_open().
 * Typically gets called as part of the processing of the Interrupt
 * Service Routine.
 *
 * ring callback gets called by HAL if, and only if, there is at least
 * one new completion on a given ring . Upon processing the first @rxdh
 * ULD is _supposed_ to continue consuming completions
 * using - vxge_hal_ring_rxd_next_completed().
 *
 * Note that failure to process new completions in a timely fashion
 * leads to VXGE_HAL_INF_OUT_OF_DESCRIPTORS condition.
 *
 * Non-zero @t_code means failure to process receive descriptor.
 *
 * In the "transmit" case the failure could happen, for instance, when the
 * link is down, in which case X3100 completes the descriptor because it
 * is not able to send the data out.
 *
 * For details please refer to X3100 User Guide.
 *
 * See also: vxge_hal_ring_rxd_next_completed(), vxge_hal_ring_rxd_term_f {}.
 */
typedef vxge_hal_status_e(*vxge_hal_ring_callback_f) (
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    void *rxd_priv,
    u8 t_code,
    void *userdata);

/*
 * function vxge_hal_ring_rxd_init_f - Initialize descriptor callback.
 * @vpath_handle: Virtual path whose ring "containing" the @rxdh descriptor.
 * @rxdh: Descriptor.
 * @rxd_priv: Pointer to per rxd space allocated
 * @index: Index of the descriptor in the ring's set of descriptors.
 * @userdata: Per-ring user data (a.k.a. context) specified at
 * ring open time, via vxge_hal_vpath_open().
 * @reopen: See  vxge_hal_reopen_e {}.
 *
 * Initialize descriptor callback. Unless NULL is specified in the
 * vxge_hal_ring_attr_t {} structure passed to vxge_hal_vpath_open()),
 * HAL invokes the callback as part of the ring create in vxge_hal_vpath_open()
 * implementation.
 * The ULD is expected to fill in this descriptor with buffer(s)
 * and control information.
 *
 * See also: vxge_hal_ring_attr_t {}, vxge_hal_ring_rxd_term_f {}.
 */
typedef vxge_hal_status_e(*vxge_hal_ring_rxd_init_f) (
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    void *rxd_priv,
    u32 index,
    void *userdata,
    vxge_hal_reopen_e reopen);

/*
 * function vxge_hal_ring_rxd_term_f - Terminate descriptor callback.
 * @vpath_handle: Virtual path whose ring "containing" the @rxdh descriptor.
 * @rxdh: First completed descriptor.
 * @rxd_priv: Pointer to per rxd space allocated
 * @state: One of the vxge_hal_rxd_state_e {} enumerated states.
 * @userdata: Per-ring user data (a.k.a. context) specified at
 * ring open time, via vxge_hal_vpath_open().
 * @reopen: See  vxge_hal_reopen_e {}.
 *
 * Terminate descriptor callback. Unless NULL is specified in the
 * vxge_hal_ring_attr_t {} structure passed to vxge_hal_vpath_open()),
 * HAL invokes the callback as part of closing the corresponding
 * ring, prior to de-allocating the ring and associated data
 * structures (including descriptors).
 * ULD should utilize the callback to (for instance) unmap
 * and free DMA data buffers associated with the posted (state =
 * VXGE_HAL_RXD_STATE_POSTED) descriptors,
 * as well as other relevant cleanup functions.
 *
 * See also: vxge_hal_ring_attr_t {}, vxge_hal_ring_rxd_init_f {}.
 */
typedef void (*vxge_hal_ring_rxd_term_f) (
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    void *rxd_priv,
    vxge_hal_rxd_state_e state,
    void *userdata,
    vxge_hal_reopen_e reopen);

/*
 * struct vxge_hal_ring_attr_t - Ring open "template".
 * @callback: Ring completion callback. HAL invokes the callback when there
 *	   are new completions on that ring. In many implementations
 *	   the @callback executes in the hw interrupt context.
 * @rxd_init: Ring's descriptor-initialize callback.
 *	   See vxge_hal_ring_rxd_init_f {}.
 *	   If not NULL, HAL invokes the callback when opening
 *	   the ring.
 * @rxd_term: Ring's descriptor-terminate callback. If not NULL,
 *	   HAL invokes the callback when closing the corresponding ring.
 *	   See also vxge_hal_ring_rxd_term_f {}.
 * @userdata: User-defined "context" of _that_ ring. Passed back to the
 *	   user as one of the @callback, @rxd_init, and @rxd_term arguments.
 * @per_rxd_space: If specified (i.e., greater than zero): extra space
 *	    reserved by HAL per each receive descriptor. Can be used to store,
 *	    and retrieve on completion, information specific
 *	    to the upper-layer.
 *
 * Ring open "template". User fills the structure with ring
 * attributes and passes it to vxge_hal_vpath_open().
 */
typedef struct vxge_hal_ring_attr_t {
	vxge_hal_ring_callback_f		callback;
	vxge_hal_ring_rxd_init_f		rxd_init;
	vxge_hal_ring_rxd_term_f		rxd_term;
	void					*userdata;
	u32					per_rxd_space;
} vxge_hal_ring_attr_t;


/*
 * vxge_hal_ring_rxd_size_get	- Get the size of ring descriptor.
 * @buf_mode: Buffer mode (1, 3 or 5)
 *
 * This function returns the size of RxD for given buffer mode
 */
static inline u32
/* LINTED */
vxge_hal_ring_rxd_size_get(
    u32 buf_mode)
{
	return ((u32) (buf_mode == 1 ? sizeof(vxge_hal_ring_rxd_1_t) : \
	    (buf_mode == 3 ? sizeof(vxge_hal_ring_rxd_3_t) : \
	    sizeof(vxge_hal_ring_rxd_5_t))));

}

/*
 * vxge_hal_ring_rxds_per_block_get - Get the number of rxds per block.
 * @buf_mode: Buffer mode (1, 3 or 5)
 *
 * This function returns the number of RxD for RxD block for given buffer mode
 */
static inline u32
/* LINTED */
vxge_hal_ring_rxds_per_block_get(
    u32 buf_mode)
{
	return ((u32) ((VXGE_OS_HOST_PAGE_SIZE - 16) /
	    ((buf_mode == 1) ? sizeof(vxge_hal_ring_rxd_1_t) :
	    ((buf_mode == 3) ? sizeof(vxge_hal_ring_rxd_3_t) :
	    sizeof(vxge_hal_ring_rxd_5_t)))));
}

/*
 * vxge_hal_ring_rxd_reserve	- Reserve ring descriptor.
 * @vpath_handle: virtual Path handle.
 * @rxdh: Reserved descriptor. On success HAL fills this "out" parameter
 *		 with a valid handle.
 * @rxd_priv: Buffer to return the pointer to per rxd space allocated
 *
 * Reserve Rx descriptor for the subsequent filling-in (by upper layer
 * driver (ULD)) and posting on	the corresponding ring
 * via vxge_hal_ring_rxd_post().
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available.
 *
 */
vxge_hal_status_e
vxge_hal_ring_rxd_reserve(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h *rxdh,
    void **rxd_priv);

/*
 * vxge_hal_ring_rxd_1b_set - Prepare 1-buffer-mode descriptor.
 * @rxdh: Descriptor handle.
 * @dma_pointer: DMA address of	a single receive buffer	this descriptor
 *		should	carry. Note that by the	time
 *		vxge_hal_ring_rxd_1b_set is called, the
 *		receive buffer should be already mapped
 *		to the	corresponding X3100 device.
 * @size: Size of the receive @dma_pointer buffer.
 *
 * Prepare 1-buffer-mode Rx	descriptor for posting
 * (via	vxge_hal_ring_rxd_post()).
 *
 * This	inline helper-function does not	return any parameters and always
 * succeeds.
 *
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_1b_set(
    vxge_hal_rxd_h rxdh,
    dma_addr_t dma_pointer,
    int size)
{
	vxge_hal_ring_rxd_1_t *rxdp = (vxge_hal_ring_rxd_1_t *) rxdh;
	rxdp->buffer0_ptr = dma_pointer;
	rxdp->control_1 &= ~VXGE_HAL_RING_RXD_1_BUFFER0_SIZE_MASK;
	rxdp->control_1 |= VXGE_HAL_RING_RXD_1_BUFFER0_SIZE(size);
}

/*
 * vxge_hal_ring_rxd_3b_set - Prepare 3-buffer-mode descriptor.
 * @rxdh: Descriptor handle.
 * @dma_pointers: Array	of DMA addresses. Contains exactly 3 receive buffers
 *		_this_ descriptor should carry. Note that by the time
 *		vxge_hal_ring_rxd_3b_set is called, the receive	buffers	should
 *		be mapped to the corresponding X3100 device.
 * @sizes: Array of receive buffer sizes. Contains 3 sizes: one size per
 *		buffer from @dma_pointers.
 *
 * Prepare 3-buffer-mode Rx descriptor for posting (via
 * vxge_hal_ring_rxd_post()).
 * This	inline helper-function does not	return any parameters and always
 * succeeds.
 *
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_3b_set(
    vxge_hal_rxd_h rxdh,
    dma_addr_t dma_pointers[],
    u32 sizes[])
{
	vxge_hal_ring_rxd_3_t *rxdp = (vxge_hal_ring_rxd_3_t *) rxdh;
	rxdp->buffer0_ptr = dma_pointers[0];
	rxdp->control_1 &= (~VXGE_HAL_RING_RXD_3_BUFFER0_SIZE_MASK);
	rxdp->control_1 |= VXGE_HAL_RING_RXD_3_BUFFER0_SIZE(sizes[0]);
	rxdp->buffer1_ptr = dma_pointers[1];
	rxdp->control_1 &= (~VXGE_HAL_RING_RXD_3_BUFFER1_SIZE_MASK);
	rxdp->control_1 |= VXGE_HAL_RING_RXD_3_BUFFER1_SIZE(sizes[1]);
	rxdp->buffer2_ptr = dma_pointers[2];
	rxdp->control_1 &= (~VXGE_HAL_RING_RXD_3_BUFFER2_SIZE_MASK);
	rxdp->control_1 |= VXGE_HAL_RING_RXD_3_BUFFER2_SIZE(sizes[2]);
}

/*
 * vxge_hal_ring_rxd_5b_set - Prepare 5-buffer-mode descriptor.
 * @rxdh: Descriptor handle.
 * @dma_pointers: Array	of DMA addresses. Contains exactly 5 receive buffers
 *		_this_ descriptor should carry. Note that by the time
 *		vxge_hal_ring_rxd_5b_set is called, the receive buffers should
 *		be mapped to the corresponding X3100 device.
 * @sizes: Array of receive buffer sizes. Contains 5 sizes: one	size per buffer
 *		from @dma_pointers.
 *
 * Prepare 5-buffer-mode Rx descriptor for posting
 * (via vxge_hal_ring_rxd_post()).
 * This	inline helper-function does not	return any
 * values and always succeeds.
 *
 * See also: vxge_hal_ring_rxd_1b_set(), vxge_hal_ring_rxd_3b_set().
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_5b_set(
    vxge_hal_rxd_h rxdh,
    dma_addr_t dma_pointers[],
    u32 sizes[])
{
	vxge_hal_ring_rxd_5_t *rxdp = (vxge_hal_ring_rxd_5_t *) rxdh;

	rxdp->buffer0_ptr = dma_pointers[0];
	rxdp->control_1 &= (~VXGE_HAL_RING_RXD_5_BUFFER0_SIZE_MASK);
	rxdp->control_1 |= VXGE_HAL_RING_RXD_5_BUFFER0_SIZE(sizes[0]);
	rxdp->buffer1_ptr = dma_pointers[1];
	rxdp->control_1 &= (~VXGE_HAL_RING_RXD_5_BUFFER1_SIZE_MASK);
	rxdp->control_1 |= VXGE_HAL_RING_RXD_5_BUFFER1_SIZE(sizes[1]);
	rxdp->buffer2_ptr = dma_pointers[2];
	rxdp->control_1 &= (~VXGE_HAL_RING_RXD_5_BUFFER2_SIZE_MASK);
	rxdp->control_1 |= VXGE_HAL_RING_RXD_5_BUFFER2_SIZE(sizes[2]);
	rxdp->buffer3_ptr = dma_pointers[3];
	rxdp->control_2 &= (~VXGE_HAL_RING_RXD_5_BUFFER3_SIZE_MASK);
	rxdp->control_2 |= VXGE_HAL_RING_RXD_5_BUFFER3_SIZE(sizes[3]);
	rxdp->buffer4_ptr = dma_pointers[4];
	rxdp->control_2 &= (~VXGE_HAL_RING_RXD_5_BUFFER4_SIZE_MASK);
	rxdp->control_2 |= VXGE_HAL_RING_RXD_5_BUFFER4_SIZE(sizes[4]);
}

/*
 * vxge_hal_ring_rxd_pre_post - Prepare rxd and post
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * This routine prepares a rxd and posts
 */
void
vxge_hal_ring_rxd_pre_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh);

/*
 * vxge_hal_ring_rxd_post_post - Process rxd after post.
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Processes rxd after post
 */
void
vxge_hal_ring_rxd_post_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh);

/*
 * vxge_hal_ring_rxd_post_post_db - Post Doorbell after posting the rxd(s).
 * @vpath_handle: virtual Path handle.
 *
 * Post Doorbell after posting the rxd(s).
 */
void
vxge_hal_ring_rxd_post_post_db(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_ring_rxd_post_post_wmb - Process rxd after post with memory barrier
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Processes rxd after post with memory barrier.
 */
void
vxge_hal_ring_rxd_post_post_wmb(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh);

/*
 * vxge_hal_ring_rxd_post - Post descriptor on the ring.
 * @vpath_handle: virtual Path handle.
 * @rxdh: Descriptor obtained via vxge_hal_ring_rxd_reserve().
 *
 * Post	descriptor on the ring.
 * Prior to posting the	descriptor should be filled in accordance with
 * Host/X3100 interface specification for a given service (LL,	etc.).
 *
 */
void
vxge_hal_ring_rxd_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh);

/*
 * vxge_hal_ring_is_next_rxd_completed - Check if the next rxd is completed
 * @vpath_handle: Virtual Path handle.
 *
 * Checks if the the _next_	completed descriptor is	in host	memory
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed	descriptors
 * are currently available for processing.
 */
vxge_hal_status_e
vxge_hal_ring_is_next_rxd_completed(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_ring_rxd_next_completed - Get the _next_ completed descriptor.
 * @vpath_handle: Virtual path handle.
 * @rxdh: Descriptor handle. Returned by HAL.
 * @rxd_priv: Buffer to return a pointer to the per rxd space allocated
 * @t_code:	Transfer code, as per X3100 User Guide,
 *			Receive	Descriptor Format. Returned	by HAL.
 *
 * Retrieve the	_next_ completed descriptor.
 * HAL uses ring callback (*vxge_hal_ring_callback_f) to notifiy
 * upper-layer driver (ULD) of new completed descriptors. After that
 * the ULD can use vxge_hal_ring_rxd_next_completed to retrieve the rest
 * completions (the very first completion is passed by HAL via
 * vxge_hal_ring_callback_f).
 *
 * Implementation-wise,	the upper-layer	driver is free to call
 * vxge_hal_ring_rxd_next_completed either immediately from inside the
 * ring callback, or in a deferred fashion and separate (from HAL)
 * context.
 *
 * Non-zero @t_code means failure to fill-in receive buffer(s)
 * of the descriptor.
 * For instance, parity error detected during the data transfer.
 * In this case X3100 will complete the descriptor and indicate
 * for the host that the received data is not to be used.
 * For details please refer to X3100 User Guide.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed descriptors
 * are currently available for processing.
 *
 * See also: vxge_hal_ring_callback_f {},
 * vxge_hal_fifo_txdl_next_completed(), vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_ring_rxd_next_completed(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h *rxdh,
    void **rxd_priv,
    u8 *t_code);

/*
 * vxge_hal_ring_handle_tcode - Handle transfer code.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 * @t_code: One of the enumerated (and documented in the X3100 user guide)
 *	 "transfer codes".
 *
 * Handle descriptor's transfer code. The latter comes with each completed
 * descriptor.
 *
 * Returns: one of the vxge_hal_status_e {} enumerated types.
 * VXGE_HAL_OK			- for success.
 * VXGE_HAL_ERR_CRITICAL	- when encounters critical error.
 */
vxge_hal_status_e
vxge_hal_ring_handle_tcode(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    u8 t_code);

/*
 * vxge_hal_ring_rxd_1b_get - Get data from the completed 1-buf
 * descriptor.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 * @dma_pointer: DMA address of	a single receive buffer	_this_ descriptor
 *				carries. Returned by HAL.
 * @pkt_length:	Length (in bytes) of the data in the buffer pointed	by
 *				@dma_pointer. Returned by HAL.
 *
 * Retrieve protocol data from the completed 1-buffer-mode Rx descriptor.
 * This	inline helper-function uses completed descriptor to populate receive
 * buffer pointer and other "out" parameters. The function always succeeds.
 *
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_1b_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    dma_addr_t *dma_pointer,
    u32 *pkt_length)
{
	vxge_hal_ring_rxd_1_t *rxdp = (vxge_hal_ring_rxd_1_t *) rxdh;

	*pkt_length =
	    (u32) VXGE_HAL_RING_RXD_1_BUFFER0_SIZE_GET(rxdp->control_1);
	*dma_pointer = rxdp->buffer0_ptr;
}

/*
 * vxge_hal_ring_rxd_3b_get - Get data from the completed 3-buf
 * descriptor.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 * @dma_pointers: DMA addresses	of the 3 receive buffers _this_	descriptor
 *			carries. The first two buffers contain ethernet and
 *			(IP + transport) headers. The 3rd buffer contains packet
 *			data.
 * @sizes: Array of receive buffer sizes. Contains 3 sizes: one	size per
 * buffer from @dma_pointers. Returned by HAL.
 *
 * Retrieve	protocol data from the completed 3-buffer-mode Rx descriptor.
 * This	inline helper-function uses completed descriptor to populate receive
 * buffer pointer and other "out" parameters. The function always succeeds.
 *
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_3b_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    dma_addr_t dma_pointers[],
    u32 sizes[])
{
	vxge_hal_ring_rxd_3_t *rxdp = (vxge_hal_ring_rxd_3_t *) rxdh;

	dma_pointers[0] = rxdp->buffer0_ptr;
	sizes[0] = (u32) VXGE_HAL_RING_RXD_3_BUFFER0_SIZE_GET(rxdp->control_1);

	dma_pointers[1] = rxdp->buffer1_ptr;
	sizes[1] = (u32) VXGE_HAL_RING_RXD_3_BUFFER1_SIZE_GET(rxdp->control_1);

	dma_pointers[2] = rxdp->buffer2_ptr;
	sizes[2] = (u32) VXGE_HAL_RING_RXD_3_BUFFER2_SIZE_GET(rxdp->control_1);
}

/*
 * vxge_hal_ring_rxd_5b_get - Get data from the completed 5-buf descriptor.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 * @dma_pointers: DMA addresses	of the 5 receive buffers _this_	descriptor
 *		carries. The first 4 buffers contains L2 (ethernet) through
 *		  L5 headers. The 5th buffer contain received (applicaion)
 *		  data. Returned by HAL.
 * @sizes: Array of receive buffer sizes. Contains 5 sizes: one	size per
 * buffer from @dma_pointers. Returned by HAL.
 *
 * Retrieve	protocol data from the completed 5-buffer-mode Rx descriptor.
 * This	inline helper-function uses completed descriptor to populate receive
 * buffer pointer and other "out" parameters. The function always succeeds.
 *
 * See also: vxge_hal_ring_rxd_3b_get(),	vxge_hal_ring_rxd_5b_get().
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_5b_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    dma_addr_t dma_pointers[],
    int sizes[])
{
	vxge_hal_ring_rxd_5_t *rxdp = (vxge_hal_ring_rxd_5_t *) rxdh;

	dma_pointers[0] = rxdp->buffer0_ptr;
	sizes[0] = (u32) VXGE_HAL_RING_RXD_5_BUFFER0_SIZE_GET(rxdp->control_1);

	dma_pointers[1] = rxdp->buffer1_ptr;
	sizes[1] = (u32) VXGE_HAL_RING_RXD_5_BUFFER1_SIZE_GET(rxdp->control_1);

	dma_pointers[2] = rxdp->buffer2_ptr;
	sizes[2] = (u32) VXGE_HAL_RING_RXD_5_BUFFER2_SIZE_GET(rxdp->control_1);

	dma_pointers[3] = rxdp->buffer3_ptr;
	sizes[3] = (u32) VXGE_HAL_RING_RXD_5_BUFFER3_SIZE_GET(rxdp->control_2);

	dma_pointers[4] = rxdp->buffer4_ptr;
	sizes[4] = (u32) VXGE_HAL_RING_RXD_5_BUFFER3_SIZE_GET(rxdp->control_2);
}

/*
 * vxge_hal_ring_rxd_1b_info_get - Get extended information associated with
 *				  a completed receive descriptor for 1b mode.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 * @rxd_info: Descriptor information
 *
 * Retrieve extended information associated with a completed receive descriptor.
 *
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_1b_info_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    vxge_hal_ring_rxd_info_t *rxd_info)
{
	vxge_hal_ring_rxd_1_t *rxdp = (vxge_hal_ring_rxd_1_t *) rxdh;

	rxd_info->syn_flag =
	    (u32) VXGE_HAL_RING_RXD_SYN_GET(rxdp->control_0);
	rxd_info->is_icmp =
	    (u32) VXGE_HAL_RING_RXD_IS_ICMP_GET(rxdp->control_0);
	rxd_info->fast_path_eligible =
	    (u32) VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE_GET(rxdp->control_0);
	rxd_info->l3_cksum_valid =
	    (u32) VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT_GET(rxdp->control_0);
	rxd_info->l3_cksum =
	    (u32) VXGE_HAL_RING_RXD_L3_CKSUM_GET(rxdp->control_0);
	rxd_info->l4_cksum_valid =
	    (u32) VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT_GET(rxdp->control_0);
	rxd_info->l4_cksum =
	    (u32) VXGE_HAL_RING_RXD_L4_CKSUM_GET(rxdp->control_0);
	rxd_info->frame =
	    (u32) VXGE_HAL_RING_RXD_ETHER_ENCAP_GET(rxdp->control_0);
	rxd_info->proto =
	    (u32) VXGE_HAL_RING_RXD_FRAME_PROTO_GET(rxdp->control_0);
	rxd_info->is_vlan =
	    (u32) VXGE_HAL_RING_RXD_IS_VLAN_GET(rxdp->control_0);
	rxd_info->vlan =
	    (u32) VXGE_HAL_RING_RXD_VLAN_TAG_GET(rxdp->control_1);
	rxd_info->rth_bucket =
	    (u32) VXGE_HAL_RING_RXD_RTH_BUCKET_GET(rxdp->control_0);
	rxd_info->rth_it_hit =
	    (u32) VXGE_HAL_RING_RXD_RTH_IT_HIT_GET(rxdp->control_0);
	rxd_info->rth_spdm_hit =
	    (u32) VXGE_HAL_RING_RXD_RTH_SPDM_HIT_GET(rxdp->control_0);
	rxd_info->rth_hash_type =
	    (u32) VXGE_HAL_RING_RXD_RTH_HASH_TYPE_GET(rxdp->control_0);
	rxd_info->rth_value =
	    (u32) VXGE_HAL_RING_RXD_1_RTH_HASH_VAL_GET(rxdp->control_1);
}

/*
 * vxge_hal_ring_rxd_3b_5b_info_get - Get extended information associated with
 *			    a completed receive descriptor for 3b & 5b mode.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 * @rxd_info: Descriptor information
 *
 * Retrieve extended information associated with a completed receive descriptor.
 *
 */
static	inline
/* LINTED */
void vxge_hal_ring_rxd_3b_5b_info_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh,
    vxge_hal_ring_rxd_info_t *rxd_info)
{
	vxge_hal_ring_rxd_3_t *rxdp = (vxge_hal_ring_rxd_3_t *) rxdh;

	rxd_info->syn_flag =
	    (u32) VXGE_HAL_RING_RXD_SYN_GET(rxdp->control_0);
	rxd_info->is_icmp =
	    (u32) VXGE_HAL_RING_RXD_IS_ICMP_GET(rxdp->control_0);
	rxd_info->fast_path_eligible =
	    (u32) VXGE_HAL_RING_RXD_FAST_PATH_ELIGIBLE_GET(rxdp->control_0);
	rxd_info->l3_cksum_valid =
	    (u32) VXGE_HAL_RING_RXD_L3_CKSUM_CORRECT_GET(rxdp->control_0);
	rxd_info->l3_cksum =
	    (u32) VXGE_HAL_RING_RXD_L3_CKSUM_GET(rxdp->control_0);
	rxd_info->l4_cksum_valid =
	    (u32) VXGE_HAL_RING_RXD_L4_CKSUM_CORRECT_GET(rxdp->control_0);
	rxd_info->l4_cksum =
	    (u32) VXGE_HAL_RING_RXD_L4_CKSUM_GET(rxdp->control_0);
	rxd_info->frame =
	    (u32) VXGE_HAL_RING_RXD_ETHER_ENCAP_GET(rxdp->control_0);
	rxd_info->proto =
	    (u32) VXGE_HAL_RING_RXD_FRAME_PROTO_GET(rxdp->control_0);
	rxd_info->is_vlan =
	    (u32) VXGE_HAL_RING_RXD_IS_VLAN_GET(rxdp->control_0);
	rxd_info->vlan =
	    (u32) VXGE_HAL_RING_RXD_VLAN_TAG_GET(rxdp->control_1);
	rxd_info->rth_bucket =
	    (u32) VXGE_HAL_RING_RXD_RTH_BUCKET_GET(rxdp->control_0);
	rxd_info->rth_it_hit =
	    (u32) VXGE_HAL_RING_RXD_RTH_IT_HIT_GET(rxdp->control_0);
	rxd_info->rth_spdm_hit =
	    (u32) VXGE_HAL_RING_RXD_RTH_SPDM_HIT_GET(rxdp->control_0);
	rxd_info->rth_hash_type =
	    (u32) VXGE_HAL_RING_RXD_RTH_HASH_TYPE_GET(rxdp->control_0);
	rxd_info->rth_value = (u32) VXGE_HAL_RING_RXD_3_RTH_HASH_VALUE_GET(
	    rxdp->buffer0_ptr);
}

/*
 * vxge_hal_device_is_privileged 
 * @host_type: host type.
 * @func_id: function id.
 *
 */
vxge_hal_status_e
vxge_hal_device_is_privileged(
    u32 host_type,
    u32 func_id);

/*
 * vxge_hal_ring_rxd_private_get - Get ULD private per-descriptor data
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Returns: private ULD	info associated	with the descriptor.
 * ULD requests	per-descriptor space via vxge_hal_ring_attr.
 *
 */
void *
vxge_hal_ring_rxd_private_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh);

/*
 * vxge_hal_ring_rxd_free - Free descriptor.
 * @vpath_handle: Virtual Path handle.
 * @rxdh: Descriptor handle.
 *
 * Free	the reserved descriptor. This operation is "symmetrical" to
 * vxge_hal_ring_rxd_reserve. The "free-ing" completes the descriptor's
 * lifecycle.
 *
 * After free-ing (see vxge_hal_ring_rxd_free()) the descriptor again can
 * be:
 *
 * - reserved (vxge_hal_ring_rxd_reserve);
 *
 * - posted	(vxge_hal_ring_rxd_post);
 *
 * - completed (vxge_hal_ring_rxd_next_completed);
 *
 * - and recycled again	(vxge_hal_ring_rxd_free).
 *
 * For alternative state transitions and more details please refer to
 * the design doc.
 *
 */
void
vxge_hal_ring_rxd_free(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rxd_h rxdh);


/*
 * Fifo
 */
/*
 * TX Descriptor
 */
/*
 * enum vxge_hal_txdl_state_e - Descriptor (TXDL) state.
 * @VXGE_HAL_TXDL_STATE_NONE: Invalid state.
 * @VXGE_HAL_TXDL_STATE_AVAIL: Descriptor is available for reservation.
 * @VXGE_HAL_TXDL_STATE_POSTED: Descriptor is posted for processing by the
 * device.
 * @VXGE_HAL_TXDL_STATE_FREED: Descriptor is free and can be reused for
 * filling-in and posting later.
 *
 * X3100/HAL descriptor states.
 *
 */
typedef enum vxge_hal_txdl_state_e {
	VXGE_HAL_TXDL_STATE_NONE	= 0,
	VXGE_HAL_TXDL_STATE_AVAIL	= 1,
	VXGE_HAL_TXDL_STATE_POSTED	= 2,
	VXGE_HAL_TXDL_STATE_FREED	= 3
} vxge_hal_txdl_state_e;

/*
 * enum vxge_hal_fifo_tcode_e - tcodes used in fifo
 * @VXGE_HAL_FIFO_T_CODE_OK: Transfer OK
 * @VXGE_HAL_FIFO_T_CODE_PCI_READ_CORRUPT: PCI read transaction (either TxD or
 *		frame data) returned with corrupt data.
 * @VXGE_HAL_FIFO_T_CODE_PCI_READ_FAIL:PCI read transaction was returned
 *		with no data.
 * @VXGE_HAL_FIFO_T_CODE_INVALID_MSS: The host attempted to send either a
 *		frame or LSO MSS that was too long (>9800B).
 * @VXGE_HAL_FIFO_T_CODE_LSO_ERROR: Error detected during TCP/UDP Large Send
 *		  Offload operation, due to improper header template,
 *		  unsupported protocol, etc.
 * @VXGE_HAL_FIFO_T_CODE_UNUSED: Unused
 * @VXGE_HAL_FIFO_T_CODE_MULTI_ERROR: Set to 1 by the adapter if multiple
 *		data buffer transfer errors are encountered (see below).
 *		Otherwise it is set to 0.
 *
 * These tcodes are returned in various API for TxD status
 */
typedef enum vxge_hal_fifo_tcode_e {
	VXGE_HAL_FIFO_T_CODE_OK			= 0x0,
	VXGE_HAL_FIFO_T_CODE_PCI_READ_CORRUPT	= 0x1,
	VXGE_HAL_FIFO_T_CODE_PCI_READ_FAIL	= 0x2,
	VXGE_HAL_FIFO_T_CODE_INVALID_MSS	= 0x3,
	VXGE_HAL_FIFO_T_CODE_LSO_ERROR		= 0x4,
	VXGE_HAL_FIFO_T_CODE_UNUSED		= 0x7,
	VXGE_HAL_FIFO_T_CODE_MULTI_ERROR	= 0x8
} vxge_hal_fifo_tcode_e;

/*
 * enum vxge_hal_fifo_host_steer_e - Host steer type
 * @VXGE_HAL_FIFO_HOST_STEER_NORMAL: Normal. Use Destination/MAC Address
 *		lookup to determine the transmit porte
 * @VXGE_HAL_FIFO_HOST_STEER_PORT1: Send on physical Port1
 * @VXGE_HAL_FIFO_HOST_STEER_PORT0: Send on physical Port0
 * @VXGE_HAL_FIFO_HOST_STEER_BOTH: Send on both ports.
 *
 * Host steer type
 */
typedef enum vxge_hal_fifo_host_steer_e {
	VXGE_HAL_FIFO_HOST_STEER_NORMAL		= 0x0,
	VXGE_HAL_FIFO_HOST_STEER_PORT1		= 0x1,
	VXGE_HAL_FIFO_HOST_STEER_PORT0		= 0x2,
	VXGE_HAL_FIFO_HOST_STEER_BOTH		= 0x3
} vxge_hal_fifo_host_steer_e;

/*
 * enum vxge_hal_fifo_gather_code_e - Gather codes used in fifo TxD
 * @VXGE_HAL_FIFO_GATHER_CODE_FIRST: First TxDL
 * @VXGE_HAL_FIFO_GATHER_CODE_MIDDLE: Middle TxDL
 * @VXGE_HAL_FIFO_GATHER_CODE_LAST: Last TxDL
 * @VXGE_HAL_FIFO_GATHER_CODE_FIRST_LAST: First and Last TxDL.
 *
 * These gather codes are used to indicate the position of a TxD in a TxD list
 */
typedef enum vxge_hal_fifo_gather_code_e {
	VXGE_HAL_FIFO_GATHER_CODE_FIRST		= 0x2,
	VXGE_HAL_FIFO_GATHER_CODE_MIDDLE	= 0x0,
	VXGE_HAL_FIFO_GATHER_CODE_LAST		= 0x1,
	VXGE_HAL_FIFO_GATHER_CODE_FIRST_LAST	= 0x3
} vxge_hal_fifo_gather_code_e;

/*
 * enum vxge_hal_fifo_lso_frm_encap_e - LSO Frame Encapsulation
 * @VXGE_HAL_FIFO_LSO_FRM_ENCAP_AUTO: auto mode (best guess)
 * @VXGE_HAL_FIFO_LSO_FRM_ENCAP_LLC: LLC
 * @VXGE_HAL_FIFO_LSO_FRM_ENCAP_SNAP: SNAP
 * @VXGE_HAL_FIFO_LSO_FRM_ENCAP_DIX: DIX
 *
 * LSO Frame Encapsulation type
 */
typedef enum vxge_hal_fifo_lso_frm_encap_e {
	VXGE_HAL_FIFO_LSO_FRM_ENCAP_AUTO	= 0x0,
	VXGE_HAL_FIFO_LSO_FRM_ENCAP_LLC		= 0x1,
	VXGE_HAL_FIFO_LSO_FRM_ENCAP_SNAP	= 0x2,
	VXGE_HAL_FIFO_LSO_FRM_ENCAP_DIX		= 0x3
} vxge_hal_fifo_lso_frm_encap_e;

/*
 * struct vxge_hal_fifo_txd_t - Transmit Descriptor
 * @control_0: Bits 0 to 6 - Reserved.
 *	       Bit 7 - List Ownership. This field should be initialized
 *		to '1' by the driver before the transmit list pointer is
 *		written to the adapter. This field will be set to '0' by the
 *		adapter once it has completed transmitting the frame or frames
 *		in the list. Note - This field is only valid in TxD0.
 *		Additionally, for multi-list sequences, the driver should not
 *		release any buffers until the ownership of the last list in the
 *		multi-list sequence has been returned to the host.
 *	       Bits 8 to 11 - Reserved
 *	       Bits 12 to 15 - Transfer_Code. This field is only valid in
 *		TxD0. It is used to describe the status of the transmit data
 *		buffer transfer. This field is always overwritten by the
 *		adapter, so this field may be initialized to any value.
 *	       Bits 16 to 17 - Host steering. This field allows the host to
 *		override the selection of the physical transmit port.
 *		Attention:
 *		Normal sounds as if learned from the switch rather than from
 *		the aggregation algorythms.
 *		00: Normal. Use Destination/MAC Address
 *		lookup to determine the transmit port.
 *		01: Send on physical Port1.
 *		10: Send on physical Port0.
 *		11: Send on both ports.
 *	       Bits 18 to 21 - Reserved
 *	       Bits 22 to 23 - Gather_Code. This field is set by the host and
 *		is used to describe how individual buffers comprise a frame.
 *		10: First descriptor of a frame.
 *		00: Middle of a multi-descriptor frame.
 *		01: Last descriptor of a frame.
 *		11: First and last descriptor of a frame (the entire frame
 *		resides in a single buffer).
 *		For multi-descriptor frames, the only valid gather code sequence
 *		is {10, [00], 01}. In other words,the descriptors must be placed
 *		in the list in the correct order.
 *	       Bits 24 to 27 - Reserved
 *	       Bits 28 to 29 - LSO_Frm_Encap. LSO Frame Encapsulation
 *		definition. Only valid in TxD0. This field allows the host to
 *		indicate the Ethernet encapsulation of an outbound LSO packet.
 *		00 - classic mode (best guess)
 *		01 - LLC
 *		10 - SNAP
 *		11 - DIX
 *		If "classic mode" is selected, the adapter will attempt to
 *		decode the frame's Ethernet encapsulation by examining the L/T
 *		field as follows:
 *		<= 0x05DC LLC/SNAP encoding; must examine DSAP/SSAP to determine
 *		if packet is IPv4 or IPv6.
 *		0x8870 Jumbo-SNAP encoding.
 *		0x0800 IPv4 DIX encoding
 *		0x86DD IPv6 DIX encoding
 *		others illegal encapsulation
 *	       Bits 30 - LSO_ Flag. Large Send Offload (LSO) flag.
 *		Set to 1 to perform segmentation offload for TCP/UDP.
 *		This field is valid only in TxD0.
 *	       Bits 31 to 33 - Reserved.
 *	       Bits 34 to 47 - LSO_MSS. TCP/UDP LSO Maximum Segment Size
 *		This field is meaningful only when LSO_Control is non-zero.
 *		When LSO_Control is set to TCP_LSO, the single (possibly large)
 *		TCP segment described by this TxDL will be sent as a series of
 *		TCP segments each of which contains no more than LSO_MSS
 *		payload bytes.
 *		When LSO_Control is set to UDP_LSO, the single (possibly large)
 *		UDP datagram described by this TxDL will be sent as a series of
 *		UDP datagrams each of which contains no more than LSO_MSS
 *		payload bytes.
 *		All outgoing frames from this TxDL will have LSO_MSS bytes of
 *		UDP or TCP payload, with the exception of the last, which will
 *		have <= LSO_MSS bytes of payload.
 *	       Bits 48 to 63 - Buffer_Size. Number of valid bytes in the
 *		buffer to be read by the adapter. This field is written by the
 *		host. A value of 0 is illegal.
 *	       Bits 32 to 63 - This value is written by the adapter upon
 *		completion of a UDP or TCP LSO operation and indicates the
 *		number of UDP or TCP payload bytes that were transmitted.
 *		0x0000 will bereturned for any non-LSO operation.
 * @control_1: Bits 0 to 4 - Reserved.
 *	       Bit 5 - Tx_CKO_IPv4 Set to a '1' to enable IPv4 header checksum
 *		offload. This field is only valid in the first TxD of a frame.
 *	       Bit 6 - Tx_CKO_TCP Set to a '1' to enable TCP checksum offload.
 *		This field is only valid in the first TxD of a frame (the TxD's
 *		gather code must be 10 or 11). The driver should only set this
 *	       Bit if it can guarantee that TCP is present.
 *	       Bit 7 - Tx_CKO_UDP Set to a '1' to enable UDP checksum offload.
 *		This field is only valid in the first TxD of a frame (the TxD's
 *		gather code must be 10 or 11). The driver should only set this
 *	       Bit if it can guarantee that UDP is present.
 *	       Bits 8 to 14 - Reserved.
 *	       Bit 15 - Tx_VLAN_Enable VLAN tag insertion flag. Set to a '1' to
 *		instruct the adapter to insert the VLAN tag specified by the
 *		Tx_VLAN_Tag field. This field is only valid in the first TxD of
 *		a frame.
 *	       Bits 16 to 31 - Tx_VLAN_Tag. Variable portion of the VLAN tag
 *		to be inserted into the frame by the adapter(the first two bytes
 *		of a VLAN tag are always 0x8100).This field is only valid if the
 *		Tx_VLAN_Enable field is set to '1'.
 *	       Bits 32 to 33 - Reserved.
 *	       Bits 34 to 39 - Tx_Int_Number. Indicates which Tx interrupt
 *		number the frame associated with. This field is written by the
 *		host. It is only valid in the first TxD of a frame.
 *	       Bits 40 to 42 - Reserved.
 *	       Bit 43 - Set to 1 to exclude the frame from bandwidth metering
 *		functions. This field is valid only in the first TxD
 *		of a frame.
 *	       Bits 44 to 45 - Reserved.
 *	       Bit 46 - Tx_Int_Per_List Set to a '1' to instruct the adapter to
 *		generate an interrupt as soon as all of the frames in the list
 *		have been transmitted. In order to have per-frame interrupts,
 *		the driver should place a maximum of one frame per list. This
 *		field is only valid in the first TxD of a frame.
 *	       Bit 47 - Tx_Int_Utilization Set to a '1' to instruct the adapter
 *		to count the frame toward the utilization interrupt specified in
 *		the Tx_Int_Number field. This field is only valid in the first
 *		TxD of a frame.
 *	       Bits 48 to 63 - Reserved.
 * @buffer_pointer: Buffer start address.
 * @host_control: Host_Control.Opaque 64bit data stored by ULD inside the X3100
 *	   descriptor prior to posting the latter on the fifo
 *	   via vxge_hal_fifo_txdl_post().The %host_control is returned as is to
 *	   the ULD with each completed descriptor.
 *
 * Transmit descriptor (TxD).Fifo descriptor contains configured number
 * (list) of TxDs. * For more details please refer to X3100 User Guide,
 * Section 5.4.2 "Transmit Descriptor (TxD) Format".
 */
typedef struct vxge_hal_fifo_txd_t {
	u64 control_0;
#define	VXGE_HAL_FIFO_TXD_LIST_OWN_GET(ctrl0)		    bVAL1(ctrl0, 7)
#define	VXGE_HAL_FIFO_TXD_LIST_OWN_ADAPTER			mBIT(7)

#define	VXGE_HAL_FIFO_TXD_T_CODE_GET(ctrl0)		    bVAL4(ctrl0, 12)
#define	VXGE_HAL_FIFO_TXD_T_CODE(val)			    vBIT(val, 12, 4)
#define	VXGE_HAL_FIFO_TXD_T_CODE_OK		VXGE_HAL_FIFO_T_CODE_OK
#define	VXGE_HAL_FIFO_TXD_T_CODE_PCI_READ_CORRUPT		    \
			VXGE_HAL_FIFO_T_CODE_PCI_READ_CORRUPT
#define	VXGE_HAL_FIFO_TXD_T_CODE_PCI_READ_FAIL			    \
			VXGE_HAL_FIFO_T_CODE_PCI_READ_FAIL
#define	VXGE_HAL_FIFO_TXD_T_CODE_INVALID_MSS	VXGE_HAL_FIFO_T_CODE_INVALID_MSS
#define	VXGE_HAL_FIFO_TXD_T_CODE_LSO_ERROR	VXGE_HAL_FIFO_T_CODE_LSO_ERROR
#define	VXGE_HAL_FIFO_TXD_T_CODE_UNUSED		VXGE_HAL_FIFO_T_CODE_UNUSED
#define	VXGE_HAL_FIFO_TXD_T_CODE_MULTI_ERROR	VXGE_HAL_FIFO_T_CODE_MULTI_ERROR

#define	VXGE_HAL_FIFO_TXD_HOST_STEER_GET(ctrl0)		    bVAL2(ctrl0, 16)
#define	VXGE_HAL_FIFO_TXD_HOST_STEER(val)		    vBIT(val, 16, 2)
#define	VXGE_HAL_FIFO_TXD_HOST_STEER_NORMAL	VXGE_HAL_FIFO_HOST_STEER_NORMAL
#define	VXGE_HAL_FIFO_TXD_HOST_STEER_PORT1	VXGE_HAL_FIFO_HOST_STEER_PORT1
#define	VXGE_HAL_FIFO_TXD_HOST_STEER_PORT0	VXGE_HAL_FIFO_HOST_STEER_PORT0
#define	VXGE_HAL_FIFO_TXD_HOST_STEER_BOTH	VXGE_HAL_FIFO_HOST_STEER_BOTH

#define	VXGE_HAL_FIFO_TXD_GATHER_CODE_GET(ctrl0)	    bVAL2(ctrl0, 22)
#define	VXGE_HAL_FIFO_TXD_GATHER_CODE(val)		    vBIT(val, 22, 2)
#define	VXGE_HAL_FIFO_TXD_GATHER_CODE_FIRST	VXGE_HAL_FIFO_GATHER_CODE_FIRST
#define	VXGE_HAL_FIFO_TXD_GATHER_CODE_MIDDLE	VXGE_HAL_FIFO_GATHER_CODE_MIDDLE
#define	VXGE_HAL_FIFO_TXD_GATHER_CODE_LAST	VXGE_HAL_FIFO_GATHER_CODE_LAST
#define	VXGE_HAL_FIFO_TXD_GATHER_CODE_FIRST_LAST		    \
			VXGE_HAL_FIFO_GATHER_CODE_FIRST_LAST

#define	VXGE_HAL_FIFO_TXD_LSO_FRM_ENCAP_GET(ctrl0)	    bVAL2(ctrl0, 28)
#define	VXGE_HAL_FIFO_TXD_LSO_FRM_ENCAP(val)		    vBIT(val, 28, 2)
#define	VXGE_HAL_FIFO_TXD_LSO_FRM_ENCAP_AUTO	VXGE_HAL_FIFO_LSO_FRM_ENCAP_AUTO
#define	VXGE_HAL_FIFO_TXD_LSO_FRM_ENCAP_LLC	VXGE_HAL_FIFO_LSO_FRM_ENCAP_LLC
#define	VXGE_HAL_FIFO_TXD_LSO_FRM_ENCAP_SNAP	VXGE_HAL_FIFO_LSO_FRM_ENCAP_SNAP
#define	VXGE_HAL_FIFO_TXD_LSO_FRM_ENCAP_DIX	VXGE_HAL_FIFO_LSO_FRM_ENCAP_DIX

#define	VXGE_HAL_FIFO_TXD_LSO_FLAG_GET(ctrl0)		    bVAL1(ctrl0, 30)
#define	VXGE_HAL_FIFO_TXD_LSO_FLAG			    mBIT(30)

#define	VXGE_HAL_FIFO_TXD_LSO_MSS_GET(ctrl0)		    bVAL14(ctrl0, 34)
#define	VXGE_HAL_FIFO_TXD_LSO_MSS(val)			    vBIT(val, 34, 14)

#define	VXGE_HAL_FIFO_TXD_BUFFER_SIZE_GET(ctrl0)	    bVAL16(ctrl0, 48)
#define	VXGE_HAL_FIFO_TXD_BUFFER_SIZE(val)		    vBIT(val, 48, 16)

#define	VXGE_HAL_FIFO_TXD_LSO_BYTES_SENT_GET(ctrl0)	    bVAL32(ctrl0, 32)
#define	VXGE_HAL_FIFO_TXD_LSO_BYTES_SENT(val)		    vBIT(val, 32, 32)

	u64 control_1;
#define	VXGE_HAL_FIFO_TXD_TX_CKO_IPV4_EN_GET(ctrl1)	    bVAL1(ctrl1, 5)
#define	VXGE_HAL_FIFO_TXD_TX_CKO_IPV4_EN		    mBIT(5)

#define	VXGE_HAL_FIFO_TXD_TX_CKO_TCP_EN_GET(ctrl1)	    bVAL1(ctrl1, 6)
#define	VXGE_HAL_FIFO_TXD_TX_CKO_TCP_EN			    mBIT(6)

#define	VXGE_HAL_FIFO_TXD_TX_CKO_UDP_EN_GET(ctrl1)	    bVAL1(ctrl1, 7)
#define	VXGE_HAL_FIFO_TXD_TX_CKO_UDP_EN			    mBIT(7)

#define	VXGE_HAL_FIFO_TXD_TX_CKO_CONTROL	(mBIT(5)|mBIT(6)|mBIT(7))

#define	VXGE_HAL_FIFO_TXD_VLAN_ENABLE_GET(ctrl1)	    bVAL1(ctrl1, 15)
#define	VXGE_HAL_FIFO_TXD_VLAN_ENABLE			    mBIT(15)

#define	VXGE_HAL_FIFO_TXD_VLAN_TAG_GET(ctrl1)		    bVAL16(ctrl1, 16)
#define	VXGE_HAL_FIFO_TXD_VLAN_TAG(val)			    vBIT(val, 16, 16)

#define	VXGE_HAL_FIFO_TXD_INT_NUMBER_GET(ctrl1)		    bVAL6(ctrl1, 34)
#define	VXGE_HAL_FIFO_TXD_INT_NUMBER(val)		    vBIT(val, 34, 6)

#define	VXGE_HAL_FIFO_TXD_NO_BW_LIMIT_GET(ctrl1)	    bVAL1(ctrl1, 43)
#define	VXGE_HAL_FIFO_TXD_NO_BW_LIMIT			    mBIT(43)

#define	VXGE_HAL_FIFO_TXD_INT_TYPE_PER_LIST_GET(ctrl1)	    bVAL1(ctrl1, 46)
#define	VXGE_HAL_FIFO_TXD_INT_TYPE_PER_LIST		    mBIT(46)

#define	VXGE_HAL_FIFO_TXD_INT_TYPE_UTILZ_GET(ctrl1)	    bVAL1(ctrl1, 47)
#define	VXGE_HAL_FIFO_TXD_INT_TYPE_UTILZ		    mBIT(47)

	u64 buffer_pointer;

	u64 host_control;

} vxge_hal_fifo_txd_t;

typedef vxge_hal_fifo_txd_t *vxge_hal_fifo_txdl_t;

/*
 * function vxge_hal_fifo_callback_f - FIFO callback.
 * @vpath_handle: Virtual path whose Fifo "containing" 1 or more completed
 *		descriptors.
 * @txdlh: First completed descriptor.
 * @txdl_priv: Pointer to per txdl space allocated
 * @t_code: Transfer code, as per X3100 User Guide.
 *	 Returned by HAL.
 * @host_control: Opaque 64bit data stored by ULD inside the X3100
 *	   descriptor prior to posting the latter on the fifo
 *	   via vxge_hal_fifo_txdl_post(). The @host_control is returned
 *	   as is to the ULD with each completed descriptor.
 * @userdata: Opaque per-fifo data specified at fifo open
 *	   time, via vxge_hal_vpath_open().
 *
 * Fifo completion callback (type declaration). A single per-fifo
 * callback is specified at fifo open time, via
 * vxge_hal_vpath_open(). Typically gets called as part of the processing
 * of the Interrupt Service Routine.
 *
 * Fifo callback gets called by HAL if, and only if, there is at least
 * one new completion on a given fifo. Upon processing the first @txdlh ULD
 * is _supposed_ to continue consuming completions using:
 *	- vxge_hal_fifo_txdl_next_completed()
 *
 * Note that failure to process new completions in a timely fashion
 * leads to VXGE_HAL_INF_OUT_OF_DESCRIPTORS condition.
 *
 * Non-zero @t_code means failure to process transmit descriptor.
 *
 * In the "transmit" case the failure could happen, for instance, when the
 * link is down, in which case X3100 completes the descriptor because it
 * is not able to send the data out.
 *
 * For details please refer to X3100 User Guide.
 *
 * See also: vxge_hal_fifo_txdl_next_completed(), vxge_hal_fifo_txdl_term_f {}.
 */
typedef vxge_hal_status_e(*vxge_hal_fifo_callback_f) (
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    void *txdl_priv,
    vxge_hal_fifo_tcode_e t_code,
    void *userdata);

/*
 * function vxge_hal_fifo_txdl_init_f - Initialize descriptor callback.
 * @vpath_handle: Virtual path whose Fifo "containing" the @txdlh descriptor.
 * @txdlh: Descriptor.
 * @txdl_priv: Pointer to per txdl space allocated
 * @index: Index of the descriptor in the fifo's set of descriptors.
 * @userdata: Per-fifo user data (a.k.a. context) specified at
 * fifo open time, via vxge_hal_vpath_open().
 * @reopen: See  vxge_hal_reopen_e {}.
 *
 * Initialize descriptor callback. Unless NULL is specified in the
 * vxge_hal_fifo_attr_t {} structure passed to vxge_hal_vpath_open()),
 * HAL invokes the callback as part of the vxge_hal_vpath_open()
 * implementation.
 * The ULD could use the callback to pre-set DMA mappings and/or alignment
 * buffers.
 *
 * See also: vxge_hal_fifo_attr_t {}, vxge_hal_fifo_txdl_term_f {}.
 */
typedef vxge_hal_status_e(*vxge_hal_fifo_txdl_init_f) (
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    void *txdl_priv,
    u32 index,
    void *userdata,
    vxge_hal_reopen_e reopen);

/*
 * function vxge_hal_fifo_txdl_term_f - Terminate descriptor callback.
 * @vpath_handle: Virtual path whose Fifo "containing" the @txdlh descriptor.
 * @txdlh: First completed descriptor.
 * @txdl_priv: Pointer to per txdl space allocated
 * @state: One of the vxge_hal_txdl_state_e {} enumerated states.
 * @userdata: Per-fifo user data (a.k.a. context) specified at
 * fifo open time, via vxge_hal_vpath_open().
 * @reopen: See  vxge_hal_reopen_e {}.
 *
 * Terminate descriptor callback. Unless NULL is specified in the
 * vxge_hal_fifo_attr_t {} structure passed to vxge_hal_vpath_open()),
 * HAL invokes the callback as part of closing fifo, prior to
 * de-allocating the ring and associated data structures
 * (including descriptors).
 * ULD should utilize the callback to (for instance) unmap
 * and free DMA data buffers associated with the posted (state =
 * VXGE_HAL_TXDL_STATE_POSTED) descriptors,
 * as well as other relevant cleanup functions.
 *
 * See also: vxge_hal_fifo_attr_t {}, vxge_hal_fifo_txdl_init_f {}.
 */
typedef void (*vxge_hal_fifo_txdl_term_f) (
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    void *txdl_priv,
    vxge_hal_txdl_state_e state,
    void *userdata,
    vxge_hal_reopen_e reopen);

/*
 * struct vxge_hal_fifo_attr_t - Fifo open "template".
 * @callback: Fifo completion callback. HAL invokes the callback when there
 *	   are new completions on that fifo. In many implementations
 *	   the @callback executes in the hw interrupt context.
 * @txdl_init: Fifo's descriptor-initialize callback.
 *	   See vxge_hal_fifo_txdl_init_f {}.
 *	   If not NULL, HAL invokes the callback when opening
 *	   the fifo via vxge_hal_vpath_open().
 * @txdl_term: Fifo's descriptor-terminate callback. If not NULL,
 *	 HAL invokes the callback when closing the corresponding fifo.
 *	 See also vxge_hal_fifo_txdl_term_f {}.
 * @userdata: User-defined "context" of _that_ fifo. Passed back to the
 *	   user as one of the @callback, @txdl_init, and @txdl_term arguments.
 * @per_txdl_space: If specified (i.e., greater than zero): extra space
 *		 reserved by HAL per each transmit descriptor. Can be used to
 *		 store, and retrieve on completion, information specific
 *		 to the upper-layer.
 *
 * Fifo open "template". User fills the structure with fifo
 * attributes and passes it to vxge_hal_vpath_open().
 */
typedef struct vxge_hal_fifo_attr_t {
	vxge_hal_fifo_callback_f		callback;
	vxge_hal_fifo_txdl_init_f		txdl_init;
	vxge_hal_fifo_txdl_term_f		txdl_term;
	void					*userdata;
	u32					per_txdl_space;
} vxge_hal_fifo_attr_t;

/*
 * vxge_hal_fifo_doorbell_reset - Resets the doorbell fifo
 * @vpath_handle: Vpath Handle
 *
 * This function resets the doorbell fifo during if fifo error occurs
 */
vxge_hal_status_e
vxge_hal_fifo_doorbell_reset(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_fifo_txdl_reserve - Reserve fifo descriptor.
 * @vpath_handle: virtual path handle.
 * @txdlh: Reserved descriptor. On success HAL fills this "out" parameter
 *	with a valid handle.
 * @txdl_priv: Buffer to return the pointer to per txdl space allocated
 *
 * Reserve a single TxDL (that is, fifo descriptor)
 * for the subsequent filling-in by upper layerdriver (ULD))
 * and posting on the corresponding fifo
 * via vxge_hal_fifo_txdl_post().
 *
 * Note: it is the responsibility of ULD to reserve multiple descriptors
 * for lengthy (e.g., LSO) transmit operation. A single fifo descriptor
 * carries up to configured number (fifo.max_frags) of contiguous buffers.
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available
 *
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_reserve(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h *txdlh,
    void **txdl_priv);


/*
 * vxge_hal_fifo_txdl_cksum_set_bits - Offload checksum.
 * @txdlh: Descriptor handle.
 * @cksum_bits: Specifies which checksums are to be offloaded: IPv4,
 *		 and/or TCP and/or UDP.
 *
 * Ask X3100 to calculate IPv4 & transport checksums for _this_ transmit
 * descriptor.
 * This API is part of the preparation of the transmit descriptor for posting
 * (via vxge_hal_fifo_txdl_post()). The related "preparation" APIs include
 * vxge_hal_fifo_txdl_mss_set(), vxge_hal_fifo_txdl_buffer_set_aligned(),
 * and vxge_hal_fifo_txdl_buffer_set().
 * All these APIs fill in the fields of the fifo descriptor,
 * in accordance with the X3100 specification.
 *
 */
static	inline
/* LINTED */
void vxge_hal_fifo_txdl_cksum_set_bits(
    vxge_hal_txdl_h txdlh,
    u64 cksum_bits)
{
	vxge_hal_fifo_txd_t *txdp = (vxge_hal_fifo_txd_t *) txdlh;

	txdp->control_1 |= cksum_bits;

}

/*
 * vxge_hal_fifo_txdl_interrupt_type_set - Set the interrupt type for the txdl
 * @txdlh: Descriptor handle.
 * @interrupt_type: utiliz based interupt or List interrupt
 *
 * vxge_hal_fifo_txdl_interrupt_type_set is used to set the interrupt type for
 * each xmit txdl dynamically
 */
static	inline
/* LINTED */
void vxge_hal_fifo_txdl_interrupt_type_set(
    vxge_hal_txdl_h txdlh,
    u64 interrupt_type)
{
	vxge_hal_fifo_txd_t *txdp = (vxge_hal_fifo_txd_t *) txdlh;

	txdp->control_1 |= interrupt_type;
}

/*
 * vxge_hal_fifo_txdl_lso_set - Set LSO Parameters.
 * @txdlh: Descriptor handle.
 * @encap: LSO Encapsulation
 * @mss: MSS size for LSO.
 *
 * This API is part of the preparation of the transmit descriptor for posting
 * (via vxge_hal_fifo_txdl_post()). The related "preparation" APIs include
 * vxge_hal_fifo_txdl_buffer_set(), vxge_hal_fifo_txdl_buffer_set_aligned(),
 * and vxge_hal_fifo_txdl_cksum_set_bits().
 * All these APIs fill in the fields of the fifo descriptor,
 * in accordance with the X3100 specification.
 *
 */
static	inline
/* LINTED */
void vxge_hal_fifo_txdl_lso_set(
    vxge_hal_txdl_h txdlh,
    u32 encap,
    u32 mss)
{
	vxge_hal_fifo_txd_t *txdp = (vxge_hal_fifo_txd_t *) txdlh;

	txdp->control_0 |= VXGE_HAL_FIFO_TXD_LSO_FRM_ENCAP(encap) |
	    VXGE_HAL_FIFO_TXD_LSO_FLAG | VXGE_HAL_FIFO_TXD_LSO_MSS(mss);
}

/*
 * vxge_hal_fifo_txdl_lso_bytes_sent - Get the lso bytes sent.
 * @txdlh: Descriptor handle.
 *
 * Returns the lso bytes sent
 */
static	inline
/* LINTED */
u32 vxge_hal_fifo_txdl_lso_bytes_sent(
    vxge_hal_txdl_h txdlh)
{
	vxge_hal_fifo_txd_t *txdp = (vxge_hal_fifo_txd_t *) txdlh;

	return (u32) VXGE_HAL_FIFO_TXD_LSO_BYTES_SENT_GET(txdp->control_0);
}

/*
 * vxge_hal_fifo_txdl_vlan_set - Set VLAN tag.
 * @txdlh: Descriptor handle.
 * @vlan_tag: 16bit VLAN tag.
 *
 * Insert VLAN tag into specified transmit descriptor.
 * The actual insertion of the tag into outgoing frame is done by the hardware.
 */
static	inline
/* LINTED */
void vxge_hal_fifo_txdl_vlan_set(
    vxge_hal_txdl_h txdlh,
    u16 vlan_tag)
{
	vxge_hal_fifo_txd_t *txdp = (vxge_hal_fifo_txd_t *) txdlh;

	txdp->control_1 |= VXGE_HAL_FIFO_TXD_VLAN_ENABLE;
	txdp->control_1 |= VXGE_HAL_FIFO_TXD_VLAN_TAG(vlan_tag);
}

/*
 * vxge_hal_fifo_txdl_buffer_set - Set transmit buffer pointer in the
 * descriptor.
 * @vpath_handle: virtual path handle.
 * @txdlh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the caller's scatter-gather list¤
 *	   (of buffers).
 * @dma_pointer: DMA address of the data buffer referenced by @frag_idx.
 * @size: Size of the data buffer (in bytes).
 *
 * This API is part of the preparation of the transmit descriptor for posting
 * (via vxge_hal_fifo_txdl_post()). The related "preparation" APIs include
 * vxge_hal_fifo_txdl_mss_set() and vxge_hal_fifo_txdl_cksum_set_bits().
 * All three APIs fill in the fields of the fifo descriptor,
 * in accordance with the X3100 specification.
 *
 */
void
vxge_hal_fifo_txdl_buffer_set(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 frag_idx,
    dma_addr_t dma_pointer,
    unsigned long size);

/*
 * vxge_hal_fifo_txdl_buffer_set_aligned - Align transmit buffer and fill
 * in fifo descriptor.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the caller's scatter-gather list¤
 *	   (of buffers).
 * @vaddr: Virtual address of the data buffer.
 * @dma_pointer: DMA address of the data buffer referenced by @frag_idx.
 * @size: Size of the data buffer (in bytes).
 * @misaligned_size: Size (in bytes) of the misaligned portion of the
 * data buffer. Calculated by the caller, based on the platform/OS/other
 * specific criteria, which is outside of HAL's domain. See notes below.
 *
 * This API is part of the transmit descriptor preparation for posting
 * (via vxge_hal_fifo_txdl_post()). The related "preparation" APIs include
 * vxge_hal_fifo_txdl_mss_set() and vxge_hal_fifo_txdl_cksum_set_bits().
 * All three APIs fill in the fields of the fifo descriptor,
 * in accordance with the X3100 specification.
 * On the PCI-X based systems aligning transmit data typically provides better
 * transmit performance. The typical alignment granularity: L2 cacheline size.
 * However, HAL does not make assumptions in terms of the alignment granularity;
 * this is specified via additional @misaligned_size parameter described above.
 * Prior to calling vxge_hal_fifo_txdl_buffer_set_aligned(),
 * ULD is supposed to check alignment of a given fragment/buffer. For this HAL
 * provides a separate vxge_hal_check_alignment() API sufficient to cover
 * most (but not all) possible alignment criteria.
 * If the buffer appears to be aligned, the ULD calls
 * vxge_hal_fifo_txdl_buffer_set().
 * Otherwise, ULD calls vxge_hal_fifo_txdl_buffer_set_aligned().
 *
 * Note; This API is a "superset" of vxge_hal_fifo_txdl_buffer_set(). In
 * addition to filling in the specified descriptor it aligns transmit data on
 * the specified boundary.
 * Note: Decision on whether to align or not to align a given contiguous
 * transmit buffer is outside of HAL's domain. To this end ULD can use any
 * programmable criteria, which can help to 1) boost transmit performance,
 * and/or 2) provide a workaround for PCI bridge bugs, if any.
 *
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_buffer_set_aligned(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 frag_idx,
    void *vaddr,
    dma_addr_t dma_pointer,
    u32 size,
    u32 misaligned_size);

/*
 * vxge_hal_fifo_txdl_buffer_append - Append the contents of virtually
 *		contiguous data buffer to a single physically contiguous buffer.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 * @vaddr: Virtual address of the data buffer.
 * @size: Size of the data buffer (in bytes).
 *
 * This API is part of the transmit descriptor preparation for posting
 * (via vxge_hal_fifo_txdl_post()).
 * The main difference of this API wrt to the APIs
 * vxge_hal_fifo_txdl_buffer_set_aligned() is that this API appends the
 * contents of virtually contiguous data buffers received from
 * upper layer into a single physically contiguous data buffer and the
 * device will do a DMA from this buffer.
 *
 * See Also: vxge_hal_fifo_txdl_buffer_finalize(),
 * vxge_hal_fifo_txdl_buffer_set(),
 * vxge_hal_fifo_txdl_buffer_set_aligned().
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_buffer_append(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    void *vaddr,
    u32 size);
/*
 * vxge_hal_fifo_txdl_buffer_finalize - Prepares a descriptor that contains the
 * single physically contiguous buffer.
 *
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the Txdl list.
 *
 * This API in conjuction with vxge_hal_fifo_txdl_buffer_append() prepares
 * a descriptor that consists of a single physically contiguous buffer
 * which inturn contains the contents of one or more virtually contiguous
 * buffers received from the upper layer.
 *
 * See Also: vxge_hal_fifo_txdl_buffer_append().
 */
void
vxge_hal_fifo_txdl_buffer_finalize(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 frag_idx);

/*
 * vxge_hal_fifo_txdl_new_frame_set - Start the new packet by setting TXDL flags
 * @vpath_handle: virtual path handle.
 * @txdlh: Descriptor handle.
 *
 * This API is part of the preparation of the transmit descriptor for posting
 * (via vxge_hal_fifo_txdl_post()). This api is used to mark the end of previous
 * frame and start of a new frame.
 *
 */
void
vxge_hal_fifo_txdl_new_frame_set(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 tagged);

/*
 * vxge_hal_fifo_txdl_post - Post descriptor on the fifo.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor obtained via vxge_hal_fifo_txdl_reserve()
 *
 * Post descriptor on the fifo for transmission.
 * Prior to posting the descriptor should be filled in accordance with
 * Host/X3100 interface specification for a given service (LL, etc.).
 *
 */
void
vxge_hal_fifo_txdl_post(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    u32 tagged);

/*
 * vxge_hal_fifo_is_next_txdl_completed - Checks if the next txdl is completed
 * @vpath_handle: Virtual path handle.
 */
vxge_hal_status_e
vxge_hal_fifo_is_next_txdl_completed(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_fifo_free_txdl_count_get - returns the number of txdls available
 *                   in the fifo
 * @vpath_handle: Virtual path handle.
 */
u32
vxge_hal_fifo_free_txdl_count_get(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_fifo_txdl_next_completed - Retrieve next completed descriptor.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle. Returned by HAL.
 * @txdl_priv: Buffer to return the pointer to per txdl space allocated
 * @t_code: Transfer code, as per X3100 User Guide,
 *	 Transmit Descriptor Format.
 *	 Returned by HAL.
 *
 * Retrieve the _next_ completed descriptor.
 * HAL uses fifo callback (*vxge_hal_fifo_callback_f) to notifiy
 * upper-layer driver (ULD) of new completed descriptors. After that
 * the ULD can use vxge_hal_fifo_txdl_next_completed to retrieve the rest
 * completions (the very first completion is passed by HAL via
 * vxge_hal_fifo_callback_f).
 *
 * Implementation-wise, the upper-layer driver is free to call
 * vxge_hal_fifo_txdl_next_completed either immediately from inside the
 * fifo callback, or in a deferred fashion and separate (from HAL)
 * context.
 *
 * Non-zero @t_code means failure to process the descriptor.
 * The failure could happen, for instance, when the link is
 * down, in which case X3100 completes the descriptor because it
 * is not able to send the data out.
 *
 * For details please refer to X3100 User Guide.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed descriptors
 * are currently available for processing.
 *
 */
vxge_hal_status_e
vxge_hal_fifo_txdl_next_completed(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h *txdlh,
    void **txdl_priv,
    vxge_hal_fifo_tcode_e *t_code);

/*
 * vxge_hal_fifo_handle_tcode - Handle transfer code.
 * @vpath_handle: Virtual Path handle.
 * @txdlh: Descriptor handle.
 * @t_code: One of the enumerated (and documented in the X3100 user guide)
 *	 "transfer codes".
 *
 * Handle descriptor's transfer code. The latter comes with each completed
 * descriptor.
 *
 * Returns: one of the vxge_hal_status_e {} enumerated types.
 * VXGE_HAL_OK			- for success.
 * VXGE_HAL_ERR_CRITICAL	- when encounters critical error.
 */
vxge_hal_status_e
vxge_hal_fifo_handle_tcode(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh,
    vxge_hal_fifo_tcode_e t_code);

/*
 * vxge_hal_fifo_txdl_private_get - Retrieve per-descriptor private data.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 *
 * Retrieve per-descriptor private data.
 * Note that ULD requests per-descriptor space via
 * vxge_hal_fifo_attr_t passed to
 * vxge_hal_vpath_open().
 *
 * Returns: private ULD data associated with the descriptor.
 */
void *
vxge_hal_fifo_txdl_private_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh);

/*
 * vxge_hal_fifo_txdl_free - Free descriptor.
 * @vpath_handle: Virtual path handle.
 * @txdlh: Descriptor handle.
 *
 * Free the reserved descriptor. This operation is "symmetrical" to
 * vxge_hal_fifo_txdl_reserve. The "free-ing" completes the descriptor's
 * lifecycle.
 *
 * After free-ing (see vxge_hal_fifo_txdl_free()) the descriptor again can
 * be:
 *
 * - reserved (vxge_hal_fifo_txdl_reserve);
 *
 * - posted (vxge_hal_fifo_txdl_post);
 *
 * - completed (vxge_hal_fifo_txdl_next_completed);
 *
 * - and recycled again (vxge_hal_fifo_txdl_free).
 *
 * For alternative state transitions and more details please refer to
 * the design doc.
 *
 */
void
vxge_hal_fifo_txdl_free(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_txdl_h txdlh);

/*
 * Device
 */

/*
 * enum vxge_hal_card_e - X3100 adapter type.
 * @VXGE_HAL_CARD_UNKNOWN: Unknown device.
 * @VXGE_HAL_CARD_TITAN: X3100 device.
 *
 * Enumerates X3100 adapter types.
 *
 * See also: vxge_hal_device_check_id().
 */
typedef enum vxge_hal_card_e {
	VXGE_HAL_CARD_UNKNOWN	= 0,
	VXGE_HAL_CARD_TITAN_1	= 1,
	VXGE_HAL_CARD_TITAN_1A	= 2,
	VXGE_HAL_CARD_TITAN_2	= 3
} vxge_hal_card_e;

/*
 * struct vxge_hal_device_attr_t - Device memory spaces.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @regh1: BAR1 mapped memory handle. Same comment as above.
 * @regh2: BAR2 mapped memory handle. Same comment as above.
 * @bar0: BAR0 virtual address.
 * @bar1: BAR1 virtual address.
 * @bar2: BAR2 virtual address.
 * @irqh: IRQ handle (Solaris).
 * @cfgh: Configuration space handle (Solaris), or PCI device @pdev (Linux).
 * @pdev: PCI device object.
 *
 * Device memory spaces. Includes configuration, BAR0, BAR1, etc. per device
 * mapped memories. Also, includes a pointer to OS-specific PCI device object.
 */
typedef struct vxge_hal_device_attr_t {
	pci_reg_h		regh0;
	pci_reg_h		regh1;
	pci_reg_h		regh2;
	u8			*bar0;
	u8			*bar1;
	u8			*bar2;
	pci_irq_h		irqh;
	pci_cfg_h		cfgh;
	pci_dev_h		pdev;
} vxge_hal_device_attr_t;

/*
 * enum vxge_hal_device_link_state_e - Link state enumeration.
 * @VXGE_HAL_LINK_NONE: Invalid link state.
 * @VXGE_HAL_LINK_DOWN: Link is down.
 * @VXGE_HAL_LINK_UP: Link is up.
 *
 */
typedef enum vxge_hal_device_link_state_e {
	VXGE_HAL_LINK_NONE,
	VXGE_HAL_LINK_DOWN,
	VXGE_HAL_LINK_UP
} vxge_hal_device_link_state_e;

/*
 * enum vxge_hal_device_data_rate_e - Data rate enumeration.
 * @VXGE_HAL_DATA_RATE_UNKNOWN: Unknown .
 * @VXGE_HAL_DATA_RATE_1G: 1G.
 * @VXGE_HAL_DATA_RATE_10G: 10G.
 *
 */
typedef enum vxge_hal_device_data_rate_e {
	VXGE_HAL_DATA_RATE_UNKNOWN,
	VXGE_HAL_DATA_RATE_1G,
	VXGE_HAL_DATA_RATE_10G
} vxge_hal_device_data_rate_e;

/*
 * enum vxge_hal_device_lag_mode_e - X3100 adapter lag mode
 * @VXGE_HAL_DEVICE_LAG_MODE_UNKNOWN: Unknown mode.
 * @VXGE_HAL_DEVICE_LAG_MODE_HW_LACP: Hardware Link Aggregation.
 * @VXGE_HAL_DEVICE_LAG_MODE_ACTIVE_PASSIVE: Active Passive.
 * @VXGE_HAL_DEVICE_LAG_MODE_SINGLE_PORT: Single Port.
 * @VXGE_HAL_DEVICE_LAG_MODE_DUAL_PORT: Dual Port.
 * @VXGE_HAL_DEVICE_LAG_MODE_DISABLED: Disabled.
 *
 * Enumerates X3100 adapter lag modes.
 *
 */
typedef enum vxge_hal_device_lag_mode_e {
	VXGE_HAL_DEVICE_LAG_MODE_UNKNOWN = 0,
	VXGE_HAL_DEVICE_LAG_MODE_HW_LACP,
	VXGE_HAL_DEVICE_LAG_MODE_ACTIVE_PASSIVE,
	VXGE_HAL_DEVICE_LAG_MODE_SINGLE_PORT,
	VXGE_HAL_DEVICE_LAG_MODE_DUAL_PORT,
	VXGE_HAL_DEVICE_LAG_MODE_DISABLED
} vxge_hal_device_lag_mode_e;

/*
 * enum vxge_hal_pci_e_signalling_rate_e -  PCI-E Lane signalling rate
 * @VXGE_HAL_PCI_E_SIGNALLING_RATE_2_5GB:   PCI-E signalling rate 2.5 GB
 * @VXGE_HAL_PCI_E_SIGNALLING_RATE_5GB:	    PCI-E signalling rate 5 GB
 * @VXGE_HAL_PCI_E_SIGNALLING_RATE_UNKNOWN: Unrecognized PCI bus frequency
 *
 * PCI-E Lane signalling rate
 */
typedef enum vxge_hal_pci_e_signalling_rate_e {
	VXGE_HAL_PCI_E_SIGNALLING_RATE_2_5GB	= 1,
	VXGE_HAL_PCI_E_SIGNALLING_RATE_5GB	= 2,
	VXGE_HAL_PCI_E_SIGNALLING_RATE_UNKNOWN	= 0
} vxge_hal_pci_e_signalling_rate_e;

/*
 * enum vxge_hal_pci_e_link_width_e - PCI-E Link width enumeration.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_X1:	1 Lane.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_X2:	2 Lane.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_X4:	4 Lane.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_X8:	8 Lane.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_X12:	12 Lane.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_X16:	16 Lane.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_X32:	32 Lane.
 * @VXGE_HAL_PCI_E_LINK_WIDTH_UNKNOWN:	Unknown
 *
 * PCI-E Link width enumeration.
 */
typedef enum vxge_hal_pci_e_link_width_e {
	VXGE_HAL_PCI_E_LINK_WIDTH_X1		= 1,
	VXGE_HAL_PCI_E_LINK_WIDTH_X2		= 2,
	VXGE_HAL_PCI_E_LINK_WIDTH_X4		= 4,
	VXGE_HAL_PCI_E_LINK_WIDTH_X8		= 8,
	VXGE_HAL_PCI_E_LINK_WIDTH_X12		= 12,
	VXGE_HAL_PCI_E_LINK_WIDTH_X16		= 16,
	VXGE_HAL_PCI_E_LINK_WIDTH_X32		= 32,
	VXGE_HAL_PCI_E_LINK_WIDTH_UNKNOWN	= 0
} vxge_hal_pci_e_link_width_e;

#define	VXGE_HAL_DEVICE_STATS_SW_INFO_NOT_TRAFFIC_INTR(hldev)	\
	((vxge_hal_device_t *)hldev)->not_traffic_intr_cnt++

#define	VXGE_HAL_DEVICE_STATS_SW_INFO_TRAFFIC_INTR(hldev)	\
	((vxge_hal_device_t *)hldev)->traffic_intr_cnt++

/*
 * struct vxge_hal_device_t - Hal device object
 * @magic: Magic Number
 * @device_id: PCI Device Id of the adapter
 * @revision: PCI Device major revision
 * @upper_layer_data: Private data set by LL driver
 * @signalling_rate: PCI-E signalling rate
 * @link_width: see vxge_hal_pci_e_link_width_e {}
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @regh1: BAR1 mapped memory handle. Same comment as above.
 * @regh2: BAR2 mapped memory handle. Same comment as above.
 * @bar0: BAR0 virtual address.
 * @bar1: BAR1 virtual address.
 * @bar2: BAR2 virtual address.
 * @irqh: IRQ handle
 * @cfgh: Configuration space handle
 * @pdev: Physical device handle
 * @config: Confguration passed by the LL driver at initialization
 * @is_initialized: Flag to specify if device is initialized
 * @msix_enabled: Flag to indicate if msix is enabled
 * @terminating: Flag to specify if the device is terminating
 * @link_state: Link state
 * @data_rate: Data rate
 * @not_traffic_intr_cnt: Number of times the host was interrupted
 *			without new completions.
 *		    "Non-traffic interrupt counter".
 * @traffic_intr_cnt: Number of traffic interrupts for the device.
 * @debug_module_mask: Debug module mask
 * @debug_level: Debug Level
 *
 * HAL device object. Represents Titan adapter
 */
typedef struct vxge_hal_device_t {
	u32					magic;
#define	VXGE_HAL_DEVICE_MAGIC					0x12345678
#define	VXGE_HAL_DEVICE_DEAD					0xDEADDEAD
	u16					device_id;
	u16					revision;
	void					*upper_layer_data;
	vxge_hal_pci_e_signalling_rate_e	signalling_rate;
	vxge_hal_pci_e_link_width_e		link_width;
	pci_reg_h				regh0;
	pci_reg_h				regh1;
	pci_reg_h				regh2;
	u8					*bar0;
	u8					*bar1;
	u8					*bar2;
	pci_irq_h				irqh;
	pci_cfg_h				cfgh;
	pci_dev_h				pdev;
	vxge_hal_device_config_t		config;
	volatile u32				is_initialized;
	volatile u32				msix_enabled;
	volatile u32				terminating;
	volatile vxge_hal_device_link_state_e	link_state;
	volatile vxge_hal_device_data_rate_e	data_rate;
	volatile u32				not_traffic_intr_cnt;
	volatile u32				traffic_intr_cnt;
	u32					debug_module_mask;
	u32					debug_level;
} vxge_hal_device_t;

/*
 * struct vxge_hal_device_date_t - Date Format
 * @day: Day
 * @month: Month
 * @year: Year
 * @date: Date in string format
 *
 * Structure for returning date
 */
typedef struct vxge_hal_device_date_t {
	u32	day;
	u32	month;
	u32	year;
	char	date[16];
} vxge_hal_device_date_t;

/*
 * struct vxge_hal_device_version_t - Version Format
 * @major: Major Version
 * @minor: Minor Version
 * @build: Build Number
 * @version: Version in string format
 *
 * Structure for returning version
 */
typedef struct vxge_hal_device_version_t {
	u32	major;
	u32	minor;
	u32	build;
	char	version[32];
} vxge_hal_device_version_t;

/*
 * struct vxge_hal_device_pmd_info_t - PMD Information
 * @type: PMD Type
 * @vendor: Vender name
 * @part_num: PMD Part Number
 * @ser_num: PMD Serial Number
 *
 * Structure for returning PMD info
 */
typedef struct vxge_hal_device_pmd_info_t {
	u32	type;
#define	VXGE_HAL_DEVICE_PMD_TYPE_UNKNOWN	0
#define	VXGE_HAL_DEVICE_PMD_TYPE_10G_SR		1
#define	VXGE_HAL_DEVICE_PMD_TYPE_10G_LR		2
#define	VXGE_HAL_DEVICE_PMD_TYPE_10G_LRM	3
#define	VXGE_HAL_DEVICE_PMD_TYPE_10G_DIRECT	4
#define	VXGE_HAL_DEVICE_PMD_TYPE_10G_CX4	5
#define	VXGE_HAL_DEVICE_PMD_TYPE_10G_BASE_T	6
#define	VXGE_HAL_DEVICE_PMD_TYPE_10G_OTHER	7
#define	VXGE_HAL_DEVICE_PMD_TYPE_1G_SX		8
#define	VXGE_HAL_DEVICE_PMD_TYPE_1G_LX		9
#define	VXGE_HAL_DEVICE_PMD_TYPE_1G_CX		10
#define	VXGE_HAL_DEVICE_PMD_TYPE_1G_DIRECT	11
#define	VXGE_HAL_DEVICE_PMD_TYPE_1G_CX4		12
#define	VXGE_HAL_DEVICE_PMD_TYPE_1G_BASE_T	13
#define	VXGE_HAL_DEVICE_PMD_TYPE_1G_OTHER	14
	u32	unused;
	char	vendor[24];
	char	part_num[24];
	char	ser_num[24];
} vxge_hal_device_pmd_info_t;


/*
 * struct vxge_hal_device_hw_info_t - Device information
 * @host_type: Host Type
 * @function_mode: PCI Function Mode
 * @func_id: Function Id
 * @vpath_mask: vpath bit mask
 * @fw_version: Firmware version
 * @fw_date: Firmware Date
 * @flash_version: Firmware version
 * @flash_date: Firmware Date
 * @serial_number: Card Serial Number
 * @part_number: Card Part Number
 * @product_description: Card Product Description
 * @unused: For Solaris alignment purpose
 * @ports: Number of ports present
 * @pmd_port0: Port 0 PMD Info
 * @pmd_port1: Port 1 PMD Info
 * @mac_addrs: Mac addresses for each vpath
 * @mac_addr_masks: Mac address masks for each vpath
 *
 * Returns the vpath mask that has the bits set for each vpath allocated
 * for the driver and teh first mac addresse for each vpath
 */
typedef struct vxge_hal_device_hw_info_t {
	u32			 host_type;
#define	VXGE_HAL_NO_MR_NO_SR_NORMAL_FUNCTION			0
#define	VXGE_HAL_MR_NO_SR_VH0_BASE_FUNCTION			1
#define	VXGE_HAL_NO_MR_SR_VH0_FUNCTION0				2
#define	VXGE_HAL_NO_MR_SR_VH0_VIRTUAL_FUNCTION			3
#define	VXGE_HAL_MR_SR_VH0_INVALID_CONFIG			4
#define	VXGE_HAL_SR_VH_FUNCTION0				5
#define	VXGE_HAL_SR_VH_VIRTUAL_FUNCTION				6
#define	VXGE_HAL_VH_NORMAL_FUNCTION				7
	u64			 function_mode;
	u32			 func_id;
	u64			 vpath_mask;
	vxge_hal_device_version_t fw_version;
	vxge_hal_device_date_t	 fw_date;
	vxge_hal_device_version_t flash_version;
	vxge_hal_device_date_t	 flash_date;
	u8			 serial_number[24];
	u8			 part_number[24];
	u8			 product_description[72];
	u32			 unused;
	u32			 ports;
	vxge_hal_device_pmd_info_t pmd_port0;
	vxge_hal_device_pmd_info_t pmd_port1;
	macaddr_t		 mac_addrs[VXGE_HAL_MAX_VIRTUAL_PATHS];
	macaddr_t		 mac_addr_masks[VXGE_HAL_MAX_VIRTUAL_PATHS];
} vxge_hal_device_hw_info_t;

/*
 * vxge_hal_device_hw_info_get - Get the hw information
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @bar0: Address of BAR0 in PCI config
 * @hw_info: Buffer to return vxge_hal_device_hw_info_t {} structure
 *
 * Returns the vpath mask that has the bits set for each vpath allocated
 * for the driver, FW version information and the first mac addresse for
 * each vpath
 */
vxge_hal_status_e
vxge_hal_device_hw_info_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u8 *bar0,
    vxge_hal_device_hw_info_t *hw_info);

/*
 * vxge_hal_device_config_default_get - Initialize device config with defaults.
 * @device_config: Configuration structure to be initialized,
 *		For the X3100 configuration "knobs" please
 *		refer to vxge_hal_device_config_t and X3100
 *		User Guide.
 *
 * Initialize X3100 device config with default values.
 *
 * See also: vxge_hal_device_initialize(), vxge_hal_device_terminate(),
 * vxge_hal_status_e {} vxge_hal_device_attr_t {}.
 */
vxge_hal_status_e
vxge_hal_device_config_default_get(
    vxge_hal_device_config_t *device_config);

/*
 * vxge_hal_device_initialize - Initialize X3100 device.
 * @devh: Buffer to return HAL device handle.
 * @attr: pointer to vxge_hal_device_attr_t structure
 * @device_config: Configuration to be _applied_ to the device,
 * For the X3100 configuration "knobs" please refer to
 * vxge_hal_device_config_t and X3100 User Guide.
 *
 * Initialize X3100 device. Note that all the arguments of this public API
 * are 'IN', except @hldev. Upper-layer driver (ULD) cooperates with
 * OS to find new X3100 device, locate its PCI and memory spaces.
 *
 * When done, the ULD allocates sizeof(vxge_hal_device_t) bytes for HAL
 * to enable the latter to perform X3100 hardware initialization.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED - Driver is not initialized.
 * VXGE_HAL_ERR_BAD_DEVICE_CONFIG - Device configuration params are not
 * valid.
 * VXGE_HAL_ERR_OUT_OF_MEMORY - Memory allocation failed.
 * VXGE_HAL_ERR_BAD_SUBSYSTEM_ID - Device subsystem id is invalid.
 * VXGE_HAL_ERR_INVALID_MAC_ADDRESS - Device mac address in not valid.
 * VXGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to retrieve the mac
 * address within the time(timeout) or TTI/RTI initialization failed.
 * VXGE_HAL_ERR_SWAPPER_CTRL - Failed to configure swapper control.
 *
 * See also: vxge_hal_device_terminate(), vxge_hal_status_e {}
 * vxge_hal_device_attr_t {}.
 */
vxge_hal_status_e
vxge_hal_device_initialize(
    vxge_hal_device_h *devh,
    vxge_hal_device_attr_t *attr,
    vxge_hal_device_config_t *device_config);

/*
 * vxge_hal_device_check_id - Verify device ID.
 * @devh: HAL device handle.
 *
 * Verify device ID.
 * Returns: one of the vxge_hal_card_e {} enumerated types.
 * See also: vxge_hal_card_e {}.
 */
static	inline
/* LINTED */
vxge_hal_card_e vxge_hal_device_check_id(
    vxge_hal_device_h devh)
{
	vxge_hal_device_t *hldev = (vxge_hal_device_t *) devh;
	switch (hldev->device_id) {
	case VXGE_PCI_DEVICE_ID_TITAN_1:
		if (hldev->revision == VXGE_PCI_REVISION_TITAN_1)
			return (VXGE_HAL_CARD_TITAN_1);
		else if (hldev->revision == VXGE_PCI_REVISION_TITAN_1A)
			return (VXGE_HAL_CARD_TITAN_1A);
		else
			break;

	case VXGE_PCI_DEVICE_ID_TITAN_2:
		if (hldev->revision == VXGE_PCI_REVISION_TITAN_2)
			return (VXGE_HAL_CARD_TITAN_2);
		else
			break;
	default:
		break;
	}

	return (VXGE_HAL_CARD_UNKNOWN);
}

/*
 * vxge_hal_device_revision_get - Get Device revision number.
 * @devh: HAL device handle.
 *
 * Returns: Device revision	number
 */
static	inline
/* LINTED */
u32 vxge_hal_device_revision_get(
    vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->revision);
}

/*
 * vxge_hal_device_pciconfig_get - Read the content of given address
 *			 in pci config space.
 * @devh: Device handle.
 * @offset: Configuration address(offset)to read from
 * @length: Length of the data (1, 2 or 4 bytes)
 * @val: Pointer to a buffer to return the content of the address
 *
 * Read from the pci config space.
 *
 */
vxge_hal_status_e
vxge_hal_device_pciconfig_get(
    vxge_hal_device_h devh,
    u32 offset,
    u32 length,
    void *val);

/*
 * vxge_hal_device_bar0_get - Get BAR0 mapped address.
 * @devh: HAL device handle.
 *
 * Returns: BAR0 address of the	specified device.
 */
static	inline
/* LINTED */
u8 *vxge_hal_device_bar0_get(
    vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->bar0);
}

/*
 * vxge_hal_device_bar1_get - Get BAR1 mapped address.
 * @devh: HAL device handle.
 *
 * Returns: BAR1 address of	the	specified device.
 */
static	inline
/* LINTED */
u8 *vxge_hal_device_bar1_get(
    vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->bar1);
}

/*
 * vxge_hal_device_bar2_get - Get BAR2 mapped address.
 * @devh: HAL device handle.
 *
 * Returns: BAR2 address of the	specified device.
 */
static	inline
/* LINTED */
u8 *vxge_hal_device_bar2_get(
    vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->bar2);
}

/*
 * vxge_hal_device_bar0_set - Set BAR0 mapped address.
 * @devh: HAL device handle.
 * @bar0: BAR0 mapped address.
 * * Set BAR0 address in the HAL device	object.
 */
static	inline
/* LINTED */
void
vxge_hal_device_bar0_set(
    vxge_hal_device_h devh,
    u8 *bar0)
{
	((vxge_hal_device_t *) devh)->bar0 = bar0;
}

/*
 * vxge_hal_device_bar1_set - Set BAR1 mapped address.
 * @devh: HAL device handle.
 * @bar1: BAR1 mapped address.
 *
 * Set BAR1 address in	the HAL Device Object.
 */
static	inline
/* LINTED */
void
vxge_hal_device_bar1_set(
    vxge_hal_device_h devh,
    u8 *bar1)
{
	((vxge_hal_device_t *) devh)->bar1 = bar1;
}

/*
 * vxge_hal_device_bar2_set - Set BAR2 mapped address.
 * @devh: HAL device handle.
 * @bar2: BAR2 mapped address.
 *
 * Set BAR2 address in	the HAL Device Object.
 */
static	inline
/* LINTED */
void
vxge_hal_device_bar2_set(
    vxge_hal_device_h devh,
    u8 *bar2)
{
	((vxge_hal_device_t *) devh)->bar2 = bar2;
}

/*
 * vxge_hal_device_enable - Enable device.
 * @devh: HAL device handle.
 *
 * Enable the specified device: bring up the link/interface.
 *
 */
vxge_hal_status_e
vxge_hal_device_enable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_disable - Disable X3100 adapter.
 * @devh: HAL device handle.
 *
 * Disable this device. To gracefully reset the adapter, the host should:
 *
 *	- call vxge_hal_device_disable();
 *
 *	- call vxge_hal_device_intr_disable();
 *
 *	- do some work (error recovery, change mtu, reset, etc);
 *
 *	- call vxge_hal_device_enable();
 *
 *	- call vxge_hal_device_intr_enable().
 *
 * Note: Disabling the device does _not_ include disabling of interrupts.
 * After disabling the device stops receiving new frames but those frames
 * that were already in the pipe will keep coming for some few milliseconds.
 *
 *
 */
vxge_hal_status_e
vxge_hal_device_disable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_pci_info_get - Get PCI bus informations such as width,
 *			frequency, and mode from previously stored values.
 * @devh:		HAL device handle.
 * @signalling_rate:	pointer to a variable of enumerated type
 *			vxge_hal_pci_e_signalling_rate_e {}.
 * @link_width:		pointer to a variable of enumerated type
 *			vxge_hal_pci_e_link_width_e {}.
 *
 * Get pci-e signalling rate and link width.
 *
 * Returns: one of the vxge_hal_status_e {} enumerated types.
 * VXGE_HAL_OK			- for success.
 * VXGE_HAL_ERR_INVALID_DEVICE	- for invalid device handle.
 */
static inline
/* LINTED */
vxge_hal_status_e vxge_hal_device_pci_info_get(
    vxge_hal_device_h devh,
    vxge_hal_pci_e_signalling_rate_e *signalling_rate,
    vxge_hal_pci_e_link_width_e *link_width)
{
	vxge_hal_device_t *hldev = (vxge_hal_device_t *) devh;

	if (!hldev || !hldev->is_initialized ||
	    (hldev->magic != VXGE_HAL_DEVICE_MAGIC)) {
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}
	*signalling_rate = hldev->signalling_rate;
	*link_width = hldev->link_width;

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_device_link_state_test - Test the link state.
 * @devh: HAL device handle.
 *
 * Test link state.
 * Returns: link state.
 */
vxge_hal_device_link_state_e
vxge_hal_device_link_state_test(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_link_state_get - Get link state.
 * @devh: HAL device handle.
 *
 * Get link state.
 * Returns: link state.
 */
static inline
/* LINTED */
vxge_hal_device_link_state_e vxge_hal_device_link_state_get(
    vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->link_state);
}

/*
 * vxge_hal_device_link_state_poll - Poll for the link state.
 * @devh: HAL device handle.
 *
 * Get link state.
 * Returns: link state.
 */
vxge_hal_device_link_state_e
vxge_hal_device_link_state_poll(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_data_rate_get - Get data rate.
 * @devh: HAL device handle.
 *
 * Get data rate.
 * Returns: data rate(1G or 10G).
 */
static inline
/* LINTED */
vxge_hal_device_data_rate_e vxge_hal_device_data_rate_get(
    vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->data_rate);
}

/*
 * vxge_hal_device_data_rate_poll - Poll for the data rate.
 * @devh: HAL device handle.
 *
 * Get data rate.
 * Returns: data rate.
 */
vxge_hal_device_data_rate_e
vxge_hal_device_data_rate_poll(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_lag_mode_get - Get Current LAG Mode
 * @devh: HAL device handle.
 *
 * Get Current LAG Mode
 */
vxge_hal_device_lag_mode_e
vxge_hal_device_lag_mode_get(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_reset - Reset device.
 * @devh: HAL device handle.
 *
 * Soft-reset the device, reset the device stats except reset_cnt.
 *
 * After reset is done, will try to re-initialize HW.
 *
 * Returns:  VXGE_HAL_PENDING - successfully sent reset to device.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 */
vxge_hal_status_e
vxge_hal_device_reset(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_reset_poll - Poll the device for reset complete.
 * @devh: HAL device handle.
 *
 * Poll the device for reset complete
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_device_reset_poll(vxge_hal_device_h devh);

/*
 * vxge_hal_device_mrpcim_reset_poll - Poll the device for mrpcim reset
 *			 complete.
 * @devh: HAL device handle.
 *
 * Poll the device for mrpcim reset complete
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 * VXGE_HAL_ERR_MANAGER_NOT_FOUND - MRPCIM/SRPCIM manager not found
 * VXGE_HAL_ERR_TIME_OUT - Device Reset timed out
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_device_mrpcim_reset_poll(vxge_hal_device_h devh);

/*
 * vxge_hal_device_terminating - Mark the device as 'terminating'.
 * @devh: HAL device handle.
 *
 * Mark the device as 'terminating', going to terminate. Can be used
 * to serialize termination with other running processes/contexts.
 *
 * See also: vxge_hal_device_terminate().
 */
static inline void
/* LINTED */
vxge_hal_device_terminating(vxge_hal_device_h devh)
{
	((vxge_hal_device_t *) devh)->terminating = 1;
}

/*
 * vxge_hal_device_terminate - Terminate X3100 device.
 * @devh: HAL device handle.
 *
 * Terminate HAL device.
 *
 * See also: vxge_hal_device_initialize().
 */
void
vxge_hal_device_terminate(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_private_set - Set ULD context.
 * @devh: HAL device handle.
 * @data: pointer to ULD context
 *
 * Use HAL device to set upper-layer driver (ULD) context.
 *
 * See also: vxge_hal_device_private_get()
 */
static inline
/* LINTED */
void vxge_hal_device_private_set(
    vxge_hal_device_h devh,
    void *data)
{
	((vxge_hal_device_t *) devh)->upper_layer_data = data;
}

/*
 * vxge_hal_device_private_get - Get ULD context.
 * @devh: HAL device handle.
 *
 * Use HAL device to set upper-layer driver (ULD) context.
 *
 * See also: vxge_hal_device_private_get()
 */
static inline
/* LINTED */
void *vxge_hal_device_private_get(
    vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->upper_layer_data);
}

/*
 * vxge_hal_device_status - Check whether X3100 hardware is ready for
 * operation.
 * @devh: HAL device handle.
 * @hw_status: X3100 status register. Returned by HAL.
 *
 * Check whether X3100 hardware is ready for operation.
 * The checking includes TDMA, RDMA, PFC, PIC, MC_DRAM, and the rest
 * hardware functional blocks.
 *
 * Returns: VXGE_HAL_OK if the device is ready for operation. Otherwise
 * returns VXGE_HAL_FAIL. Also, fills in  adapter status (in @hw_status).
 *
 * See also: vxge_hal_status_e {}.
 * Usage: See ex_open {}.
 */
vxge_hal_status_e
vxge_hal_device_status(vxge_hal_device_h devh, u64 *hw_status);

/*
 * vxge_hal_device_is_slot_freeze
 * @devh: the device
 *
 * Returns non-zero if the slot is freezed.
 * The determination is made based on the adapter_status
 * register which will never give all FFs, unless PCI read
 * cannot go through.
 */
int
vxge_hal_device_is_slot_freeze(vxge_hal_device_h devh);

/*
 * vxge_hal_device_is_traffic_interrupt
 * @reason: The reason returned by the vxge)hal_device_begin_irq
 * @vp_id: Id of vpath for which to check the interrupt
 *
 * Returns non-zero if traffic interrupt raised, 0 otherwise
 */
static inline u64
/* LINTED */
vxge_hal_device_is_traffic_interrupt(u64 reason, u32 vp_id)
{
	return (reason & mBIT(vp_id + 3));
}

/*
 * vxge_hal_device_intr_enable - Enable X3100 interrupts.
 * @devh: HAL device handle.
 * @op: One of the vxge_hal_device_intr_e enumerated values specifying
 *	  the type(s) of interrupts to enable.
 *
 * Enable X3100 interrupts. The function is to be executed the last in
 * X3100 initialization sequence.
 *
 * See also: vxge_hal_device_intr_disable()
 */
void
vxge_hal_device_intr_enable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_intr_disable - Disable X3100 interrupts.
 * @devh: HAL device handle.
 * @op: One of the vxge_hal_device_intr_e enumerated values specifying
 *	  the type(s) of interrupts to disable.
 *
 * Disable X3100 interrupts.
 *
 * See also: vxge_hal_device_intr_enable()
 */
void
vxge_hal_device_intr_disable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_mask_all - Mask all device interrupts.
 * @devh: HAL device handle.
 *
 * Mask	all	device interrupts.
 *
 * See also: vxge_hal_device_unmask_all()
 */
void
vxge_hal_device_mask_all(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_unmask_all - Unmask all device interrupts.
 * @devh: HAL device handle.
 *
 * Unmask all device interrupts.
 *
 * See also: vxge_hal_device_mask_all()
 */
void
vxge_hal_device_unmask_all(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_begin_irq - Begin IRQ processing.
 * @devh: HAL device handle.
 * @skip_alarms: Do not clear the alarms
 * @reason: "Reason" for the interrupt,	the value of X3100's
 *			general_int_status register.
 *
 * The function	performs two actions, It first checks whether (shared IRQ) the
 * interrupt was raised	by the device. Next, it	masks the device interrupts.
 *
 * Note:
 * vxge_hal_device_begin_irq() does not flush MMIO writes through the
 * bridge. Therefore, two back-to-back interrupts are potentially possible.
 * It is the responsibility	of the ULD to make sure	that only one
 * vxge_hal_device_continue_irq() runs at a time.
 *
 * Returns: 0, if the interrupt	is not "ours" (note that in this case the
 * device remain enabled).
 * Otherwise, vxge_hal_device_begin_irq() returns 64bit general adapter
 * status.
 * See also: vxge_hal_device_handle_irq()
 */
vxge_hal_status_e
vxge_hal_device_begin_irq(
    vxge_hal_device_h devh,
    u32 skip_alarms,
    u64 *reason);

/*
 * vxge_hal_device_continue_irq - Continue handling IRQ:	process	all
 *				completed descriptors.
 * @devh: HAL device handle.
 *
 * Process completed descriptors and unmask the	device interrupts.
 *
 * The vxge_hal_device_continue_irq() walks all open virtual paths
 * and calls upper-layer driver	(ULD) via supplied completion
 * callback.
 *
 * Note	that the vxge_hal_device_continue_irq is	part of	the _fast_ path.
 * To optimize the processing, the function does _not_ check for
 * errors and alarms.
 *
 * Returns: VXGE_HAL_OK.
 *
 * See also: vxge_hal_device_handle_irq(),
 * vxge_hal_ring_rxd_next_completed(),
 * vxge_hal_fifo_txdl_next_completed(), vxge_hal_ring_callback_f {},
 * vxge_hal_fifo_callback_f {}.
 */
vxge_hal_status_e
vxge_hal_device_continue_irq(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_handle_irq - Handle device IRQ.
 * @devh: HAL device handle.
 * @skip_alarms: Do not clear the alarms
 *
 * Perform the complete	handling of the	line interrupt.	The function
 * performs two	calls.
 * First it uses vxge_hal_device_begin_irq() to check the reason for
 * the interrupt and mask the device interrupts.
 * Second, it calls vxge_hal_device_continue_irq() to process all
 * completed descriptors and re-enable the interrupts.
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_ERR_WRONG_IRQ - (shared)	IRQ	produced by	other device.
 *
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq().
 */
vxge_hal_status_e
vxge_hal_device_handle_irq(
    vxge_hal_device_h devh,
    u32 skip_alarms);

/*
 * vxge_hal_device_mask_tx - Mask Tx interrupts.
 * @devh: HAL device.
 *
 * Mask	Tx device interrupts.
 *
 * See also: vxge_hal_device_unmask_tx(), vxge_hal_device_mask_rx(),
 * vxge_hal_device_clear_tx().
 */
void
vxge_hal_device_mask_tx(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_clear_tx - Acknowledge (that is, clear) the
 * condition that has caused the TX	interrupt.
 * @devh: HAL device.
 *
 * Acknowledge (that is, clear)	the	condition that has caused
 * the Tx interrupt.
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq(),
 * vxge_hal_device_clear_rx(), vxge_hal_device_mask_tx().
 */
void
vxge_hal_device_clear_tx(
    vxge_hal_device_h devh);


/*
 * vxge_hal_device_unmask_tx - Unmask Tx	interrupts.
 * @devh: HAL device.
 *
 * Unmask Tx device interrupts.
 *
 * See also: vxge_hal_device_mask_tx(), vxge_hal_device_clear_tx().
 */
void
vxge_hal_device_unmask_tx(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_mask_rx - Mask Rx	interrupts.
 * @devh: HAL device.
 *
 * Mask	Rx device interrupts.
 *
 * See also: vxge_hal_device_unmask_rx(), vxge_hal_device_mask_tx(),
 * vxge_hal_device_clear_rx().
 */
void
vxge_hal_device_mask_rx(
    vxge_hal_device_h devh);


/*
 * vxge_hal_device_clear_rx - Acknowledge (that is, clear) the
 * condition that has caused the RX	interrupt.
 * @devh: HAL device.
 *
 * Acknowledge (that is, clear)	the	condition that has caused
 * the Rx interrupt.
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq(),
 * vxge_hal_device_clear_tx(), vxge_hal_device_mask_rx().
 */
void
vxge_hal_device_clear_rx(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_unmask_rx - Unmask Rx	interrupts.
 * @devh: HAL device.
 *
 * Unmask Rx device interrupts.
 *
 * See also: vxge_hal_device_mask_rx(), vxge_hal_device_clear_rx().
 */
void
vxge_hal_device_unmask_rx(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_mask_tx_rx - Mask Tx and Rx interrupts.
 * @devh: HAL device.
 *
 * Mask Tx and Rx device interrupts.
 *
 * See also: vxge_hal_device_unmask_tx_rx(), vxge_hal_device_clear_tx_rx().
 */
void
vxge_hal_device_mask_tx_rx(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_clear_tx_rx - Acknowledge (that is, clear) the
 * condition that has caused the Tx and RX interrupt.
 * @devh: HAL device.
 *
 * Acknowledge (that is, clear)	the	condition that has caused
 * the Tx and Rx interrupt.
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq(),
 * vxge_hal_device_mask_tx_rx(), vxge_hal_device_unmask_tx_rx().
 */
void
vxge_hal_device_clear_tx_rx(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_unmask_tx_rx - Unmask Tx and Rx interrupts.
 * @devh: HAL device.
 *
 * Unmask Tx and Rx device interrupts.
 *
 * See also: vxge_hal_device_mask_tx_rx(), vxge_hal_device_clear_tx_rx().
 */
void
vxge_hal_device_unmask_tx_rx(
    vxge_hal_device_h devh);

/*
 * vxge_hal_device_msix_mode - Is MSIX enabled?
 * @devh: HAL device handle.
 *
 * Returns 0 if MSIX is enabled for the specified device,
 * non-zero otherwise.
 */
static inline int
/* LINTED */
vxge_hal_device_msix_mode(vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->msix_enabled);
}

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)

/*
 * vxge_hal_device_trace_write - Write the trace from the given buffer into
 *				 circular trace buffer
 * @devh: HAL device handle.
 * @trace_buf: Buffer containing the trace.
 * @trace_len: Length of the trace in the buffer
 *
 * Writes the trace from the given buffer into the circular trace buffer
 *
 */
void
vxge_hal_device_trace_write(vxge_hal_device_h devh,
    u8 *trace_buf,
    u32 trace_len);

/*
 * vxge_hal_device_trace_dump - Dump the trace buffer.
 * @devh: HAL device handle.
 *
 * Dump the trace buffer contents.
 */
void
	vxge_hal_device_trace_dump(vxge_hal_device_h devh);

/*
 * vxge_hal_device_trace_read - Read trace buffer contents.
 * @devh: HAL device handle.
 * @buffer: Buffer to store the trace buffer contents.
 * @buf_size: Size of the buffer.
 * @read_length: Size of the valid data in the buffer.
 *
 * Read  HAL trace buffer contents starting from the offset
 * upto the size of the buffer or till EOF is reached.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_EOF_TRACE_BUF - No more data in the trace buffer.
 *
 */
vxge_hal_status_e
vxge_hal_device_trace_read(vxge_hal_device_h devh,
    char *buffer,
    unsigned buf_size,
    unsigned *read_length);

#endif

/*
 * vxge_hal_device_debug_set - Set the debug module, level and timestamp
 * @devh: Hal device object
 * @level: Debug level as defined in enum vxge_debug_level_e
 * @mask: An or value of component masks as defined in vxge_debug.h
 *
 * This routine is used to dynamically change the debug output
 */
void
vxge_hal_device_debug_set(
    vxge_hal_device_h devh,
    vxge_debug_level_e level,
    u32 mask);

/*
 * vxge_hal_device_debug_level_get - Get the debug level
 * @devh: Hal device object
 *
 * This routine returns the current debug level set
 */
static inline u32
/* LINTED */
vxge_hal_device_debug_level_get(vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->debug_level);
}

/*
 * vxge_hal_device_debug_mask_get - Get the debug mask
 * @devh: Hal device object
 *
 * This routine returns the current debug mask set
 */
static inline u32
/* LINTED */
vxge_hal_device_debug_mask_get(vxge_hal_device_h devh)
{
	return (((vxge_hal_device_t *) devh)->debug_module_mask);
}

/*
 * vxge_hal_device_flick_link_led - Flick (blink) link LED.
 * @devh: HAL device handle.
 * @port : Port number 0, or 1
 * @on_off: TRUE if flickering to be on, FALSE to be off
 *
 * Flicker the link LED.
 */
vxge_hal_status_e
vxge_hal_device_flick_link_led(
    vxge_hal_device_h devh,
    u32 port,
    u32 on_off);

/*
 * vxge_hal_device_getpause_data -Pause frame frame generation and reception.
 * @devh: HAL device handle.
 * @port : Port number 0, 1, or 2
 * @tx : A field to return the pause generation capability of the NIC.
 * @rx : A field to return the pause reception capability of the NIC.
 *
 * Returns the Pause frame generation and reception capability of the NIC.
 * Return value:
 * status
 */
vxge_hal_status_e
vxge_hal_device_getpause_data(
    vxge_hal_device_h devh,
    u32 port,
    u32 *tx,
    u32 *rx);

/*
 * Privileged operations
 */

/*
 * enum vxge_hal_pcie_function_mode_e - PCIE Function modes
 * @VXGE_HAL_PCIE_FUNC_MODE_SF1_VP17: Single Function
 *             - 1 function with 17 VPATHs
 * @VXGE_HAL_PCIE_FUNC_MODE_MF8_VP2: Multi Function
 *             - 8 functions with 2 VPATHs per function
 * @VXGE_HAL_PCIE_FUNC_MODE_SR17_VP1: SRIOV
 *             - 17 VFs with 1 VPATH per VF
 * @VXGE_HAL_PCIE_FUNC_MODE_MR17_VP1: MRIOV
 *             - 17 Virtual Hierarchies, 1 Path/Function/Hierarchy
 * @VXGE_HAL_PCIE_FUNC_MODE_MR8_VP2: MRIOV
 *             - 8 Virtual Hierarchies, 2 Path/Function/Hierarchy
 * @VXGE_HAL_PCIE_FUNC_MODE_MF17_VP1: Multi Function
 *             - 17 functions, 1 Path/Function (PCIe ARI)
 * @VXGE_HAL_PCIE_FUNC_MODE_SR8_VP2: SRIOV
 *             - 1 PF, 7 VF, 2 Paths/Function
 * @VXGE_HAL_PCIE_FUNC_MODE_SR4_VP4: SRIOV
 *             - 1 PF, 3 VF, 4 Paths/Function
 * @VXGE_HAL_PCIE_FUNC_MODE_MF2_VP8: Multi Function
 *             - 2 functions, 8 Paths/Function (funcs 2-7 have no resources)
 */
typedef enum vxge_hal_pcie_function_mode_e {
	VXGE_HAL_PCIE_FUNC_MODE_SF1_VP17,
	VXGE_HAL_PCIE_FUNC_MODE_MF8_VP2,
	VXGE_HAL_PCIE_FUNC_MODE_SR17_VP1,
	VXGE_HAL_PCIE_FUNC_MODE_MR17_VP1,
	VXGE_HAL_PCIE_FUNC_MODE_MR8_VP2,
	VXGE_HAL_PCIE_FUNC_MODE_MF17_VP1,
	VXGE_HAL_PCIE_FUNC_MODE_SR8_VP2,
	VXGE_HAL_PCIE_FUNC_MODE_SR4_VP4,
	VXGE_HAL_PCIE_FUNC_MODE_MF2_VP8,
	VXGE_HAL_PCIE_FUNC_MODE_MF4_VP4,
	VXGE_HAL_PCIE_FUNC_MODE_MR4_VP4,
	VXGE_HAL_PCIE_FUNC_MODE_MF8P_VP2

} vxge_hal_pcie_function_mode_e;

/* Behavior on failure */
typedef enum vxge_hal_xmac_nwif_behavior_on_failure {
	VXGE_HAL_XMAC_NWIF_OnFailure_NoMove,
	VXGE_HAL_XMAC_NWIF_OnFailure_OtherPort,
	VXGE_HAL_XMAC_NWIF_OnFailure_OtherPortBackOnRestore
} vxge_hal_xmac_nwif_behavior_on_failure;

/*
 * Network Port configuration cmds
 */
typedef enum vxge_hal_nwif_cmds {
	VXGE_HAL_XMAC_NWIF_Cmd_Version				= 0x0,
	VXGE_HAL_XMAC_NWIF_Cmd_SetMode				= 0x1,
	VXGE_HAL_XMAC_NWIF_Cmd_CfgSnglPort			= 0x4,
	VXGE_HAL_XMAC_NWIF_Cmd_Avail				= 0x6,
	VXGE_HAL_XMAC_NWIF_Cmd_CfgSetActPassPreferredPort	= 0x7,
	VXGE_HAL_XMAC_NWIF_Cmd_CfgSetBehaviourOnFailure		= 0x8,
	VXGE_HAL_XMAC_NWIF_Cmd_CfgDualPort_L2SwitchEnable	= 0x9,
	VXGE_HAL_XMAC_NWIF_Cmd_CfgDualPort_VPathVector		= 0xa,
	VXGE_HAL_XMAC_NWIF_Cmd_Get_Active_Config		= 0xb
} vxge_hal_nwif_cmds;

/* Network port get active config options */
typedef enum vxge_hal_xmac_nwif_actconfig {
	VXGE_HAL_XMAC_NWIF_ActConfig_Avail			= 0,
	VXGE_HAL_XMAC_NWIF_ActConfig_NWPortMode			= 1,
	VXGE_HAL_XMAC_NWIF_ActConfig_PreferredPort		= 2,
	VXGE_HAL_XMAC_NWIF_ActConfig_BehaviourOnFail		= 3,
	VXGE_HAL_XMAC_NWIF_ActConfig_ActivePort			= 4,
	VXGE_HAL_XMAC_NWIF_ActConfig_L2SwitchEnabled		= 5,
	VXGE_HAL_XMAC_NWIF_ActConfig_DualPortPath		= 6
} vxge_hal_xmac_nwif_actconfig;

/* Dual port modes */
typedef enum vxge_hal_xmac_nwif_dp_mode {
	VXGE_HAL_DP_NP_MODE_DEFAULT,
	VXGE_HAL_DP_NP_MODE_LINK_AGGR,
	VXGE_HAL_DP_NP_MODE_ACTIVE_PASSIVE,
	VXGE_HAL_DP_NP_MODE_SINGLE_PORT,
	VXGE_HAL_DP_NP_MODE_DUAL_PORT,
	VXGE_HAL_DP_NP_MODE_DISABLE_PORT_MGMT
} vxge_hal_xmac_nwif_dp_mode;

/* L2 switch status */
typedef enum vxge_hal_xmac_nwif_l2_switch_status {
	VXGE_HAL_XMAC_NWIF_L2_SWITCH_DISABLE,
	VXGE_HAL_XMAC_NWIF_L2_SWITCH_ENABLE
} vxge_hal_xmac_nwif_l2_switch_status;

/*
 * vxge_hal_srpcim_alarm_process - Process srpcim Alarms.
 * @devh: Device Handle.
 * @skip_alarms: Flasg to indicate not to clear alarms
 *
 * Process srpcim alarms.
 *
 */
vxge_hal_status_e
vxge_hal_srpcim_alarm_process(vxge_hal_device_h devh, u32 skip_alarms);

/*
 * vxge_hal_srpcim_intr_enable - Enable srpcim interrupts.
 * @devh: Device Handle.
 *
 * Enable srpcim interrupts.
 *
 * See also: vxge_hal_srpcim_intr_disable()
 */
vxge_hal_status_e
vxge_hal_srpcim_intr_enable(vxge_hal_device_h devh);

/*
 * vxge_hal_srpcim_intr_disable - Disable srpcim interrupts.
 * @devh: Device Handle.
 *
 * Disable srpcim interrupts.
 *
 * See also: vxge_hal_srpcim_intr_enable()
 */
vxge_hal_status_e
vxge_hal_srpcim_intr_disable(vxge_hal_device_h devh);

/*
 * vxge_hal_srpcim_msix_set - Associate MSIX vector with srpcim alarm
 * @devh: Device Handle.
 * @alarm_msix_id: MSIX vector for alarm.
 *
 * This API will associate a given MSIX vector numbers with srpcim alarm
 */
vxge_hal_status_e
vxge_hal_srpcim_msix_set(vxge_hal_device_h devh, int alarm_msix_id);

/*
 * vxge_hal_srpcim_msix_mask - Mask MSIX Vector.
 * @devh: Device Handle.
 *
 * The function masks the srpcim msix interrupt
 *
 */
void
vxge_hal_srpcim_msix_mask(vxge_hal_device_h devh);

/*
 * vxge_hal_srpcim_msix_clear - Clear MSIX Vector.
 * @devh: Device Handle.
 *
 * The function clears the srpcim msix interrupt
 *
 */
void
vxge_hal_srpcim_msix_clear(vxge_hal_device_h devh);


/*
 * vxge_hal_srpcim_msix_unmask - Unmask MSIX Vector.
 * @devh: Device Handle.
 *
 * The function unmasks the srpcim msix interrupt
 *
 */
void
vxge_hal_srpcim_msix_unmask(vxge_hal_device_h devh);

vxge_hal_status_e
vxge_hal_func_mode_count(vxge_hal_device_h devh,
    u32 func_mode, u32 *num_funcs);

vxge_hal_status_e
vxge_hal_send_message(vxge_hal_device_h devh, u64 vp_id, u8 msg_type,
    u8 msg_dst, u32 msg_data, u64 *msg_sent_to_vpaths);

/*
 * vxge_hal_func_mode_set - Set PCI-E function mode
 * @devh: Device Handle.
 * @func_mode: PCI-E func mode. Please see vxge_hal_pcie_function_mode_e{}
 *
 * Set PCI-E function mode.
 *
 */
vxge_hal_status_e
vxge_hal_func_mode_get(vxge_hal_device_h devh, u32 *func_mode);
/*
 * vxge_hal_func_mode_set - Set PCI-E function mode
 * @devh: Device Handle.
 * @func_mode: PCI-E func mode. Please see vxge_hal_pcie_function_mode_e{}
 *
 * Set PCI-E function mode.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_pcie_func_mode_set(vxge_hal_device_h devh,
    vxge_hal_pcie_function_mode_e func_mode);

/*
 * vxge_hal_get_active_config - Get active configuration
 * @devh: Device Handle.
 *
 */
vxge_hal_status_e
vxge_hal_get_active_config(vxge_hal_device_h devh,
    vxge_hal_xmac_nwif_actconfig req_config, u64 *cur_config);

/*
 * vxge_hw_set_port_mode - Set dual port mode
 * override the default dual port mode
 * @devh: Device Handle.
 *
 */

vxge_hal_status_e
vxge_hal_set_port_mode(vxge_hal_device_h devh,
    vxge_hal_xmac_nwif_dp_mode port_mode);

/*
 * vxge_hal_set_behavior_on_failure - Set port behaviour
 * change port behavior on failure
 * @devh: Device Handle.
 */
vxge_hal_status_e
vxge_hal_set_behavior_on_failure(vxge_hal_device_h devh,
    vxge_hal_xmac_nwif_behavior_on_failure behave_on_failure);


/*
 * vxge_hal_set_l2switch_mode - Set port behaviour
 * set l2switch mode
 * @devh: Device Handle.
 */
vxge_hal_status_e
vxge_hal_set_l2switch_mode(vxge_hal_device_h devh,
    enum vxge_hal_xmac_nwif_l2_switch_status l2_switch);
/*
 * vxge_hal_set_fw_api - Setup FW api
 * @devh: Device Handle.
 *
 */
vxge_hal_status_e
vxge_hal_set_fw_api(vxge_hal_device_h devh, u64 vp_id, u32 action,
    u32 offset, u64 data0, u64 data1);

vxge_hal_status_e
vxge_hal_config_vpath_map(vxge_hal_device_h devh, u64 port_map);

vxge_hal_status_e
vxge_hal_get_vpath_mask(vxge_hal_device_h devh, u32 vf_id,
    u32 *no_of_vpath, u64 *vpath_mask);

vxge_hal_status_e
vxge_hal_get_vpath_list(vxge_hal_device_h devh, u32 vf_id,
    u64 *vpath_list, u32 *vpath_count);

vxge_hal_status_e
vxge_hal_rx_bw_priority_set(vxge_hal_device_h devh, u64 vp_id);

vxge_hal_status_e
vxge_hal_tx_bw_priority_set(vxge_hal_device_h devh, u64 vp_id);

vxge_hal_status_e
vxge_hal_bw_priority_get(vxge_hal_device_h devh, u64 vp_id,
    u32 *bandwidth, u32 *priority);

vxge_hal_status_e
vxge_hal_vf_rx_bw_get(vxge_hal_device_h devh, u64 func_id,
    u32 *bandwidth, u32 *priority);
/*
 * vxge_hal_mrpcim_serial_number_get - Returns the serial number
 * @devh: Device Handle.
 *
 * Return the serial number
 */
const u8 *
vxge_hal_mrpcim_serial_number_get(vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_fw_upgrade - Upgrade firmware
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @bar0: Address of BAR0 in PCI config
 * @buffer: Buffer containing F/W image
 * @length: F/W image length
 *
 * Upgrade firmware
 */
vxge_hal_status_e
vxge_hal_mrpcim_fw_upgrade(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u8 *bar0,
    u8 *buffer,
    u32 length);

/*
 * vxge_hal_mrpcim_vpath_map_get - Returns the assigned vpaths map
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @bar0: Address of BAR0 in PCI config
 * @func: Function Number
 *
 * Returns the assigned vpaths map
 */
u64
vxge_hal_mrpcim_vpath_map_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u8 *bar0,
    u32 func);

/*
 * vxge_hal_mrpcim_vpath_qos_set - Set the priority, Guaranteed and maximum
 *				 bandwidth for a vpath.
 * @devh: HAL device handle.
 * @vp_id: Vpath Id.
 * @priority: Priority
 * @min_bandwidth: Minimum Bandwidth
 * @max_bandwidth: Maximum Bandwidth
 *
 * Set the Priority, Guaranteed and maximum bandwidth for a given vpath
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_vpath_qos_set(
    vxge_hal_device_h devh,
    u32 vp_id,
    u32 priority,
    u32 min_bandwidth,
    u32 max_bandwidth);

/*
 * vxge_hal_mrpcim_vpath_qos_get - Get the priority, Guaranteed and maximum
 *				 bandwidth for a vpath.
 * @devh: HAL device handle.
 * @vp_id: Vpath Id.
 * @priority: Buffer to return Priority
 * @min_bandwidth: Buffer to return Minimum Bandwidth
 * @max_bandwidth: Buffer to return Maximum Bandwidth
 *
 * Get the Priority, Guaranteed and maximum bandwidth for a given vpath
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_vpath_qos_get(
    vxge_hal_device_h devh,
    u32 vp_id,
    u32 *priority,
    u32 *min_bandwidth,
    u32 *max_bandwidth);

/*
 * vxge_hal_mrpcim_mac_addr_add - Add the mac address entry
 *		    into MAC address table.
 * @devh: Device handle.
 * @offset: Index into the DA table to add the mac address.
 * @macaddr: MAC address to be added for this vpath into the list
 * @macaddr_mask: MAC address mask for macaddr
 * @vpath_vector: Bit mask specifying the vpaths to which the mac address
 *		applies
 * @duplicate_mode: Duplicate MAC address add mode. Please see
 *		vxge_hal_vpath_mac_addr_add_mode_e {}
 *
 * Adds the given mac address, mac address mask and vpath vector into the list
 *
 * see also: vxge_hal_vpath_mac_addr_delete, vxge_hal_vpath_mac_addr_get and
 * vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_mac_addr_add(
    vxge_hal_device_h devh,
    u32 offset,
    macaddr_t macaddr,
    macaddr_t macaddr_mask,
    u64 vpath_vector,
    u32 duplicate_mode);

/*
 * vxge_hal_mrpcim_mac_addr_get - Read the mac address entry
 *				 into MAC address table.
 * @devh: Device handle.
 * @offset: Index into the DA table to execute the command on.
 * @macaddr: Buffer to return MAC address to be added for this vpath
 * @macaddr_mask: Buffer to return MAC address mask for macaddr
 * @vpath_vector: Buffer to return Bit mask specifying the vpaths to which
 *		the mac address applies
 *
 * Reads the mac address,mac address mask and vpath vector from the given offset
 *
 * see also: vxge_hal_mrpcim_mac_addr_add
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_mac_addr_get(
    vxge_hal_device_h devh,
    u32 offset,
    macaddr_t macaddr,
    macaddr_t macaddr_mask,
    u64 *vpath_vector);

/*
 * vxge_hal_mrpcim_reset - Reset the entire device.
 * @devh: HAL device handle.
 *
 * Soft-reset the device, reset the device stats except reset_cnt.
 *
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_mrpcim_reset(vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_reset_poll - Poll the device for reset complete.
 * @devh: HAL device handle.
 *
 * Soft-reset the device, reset the device stats except reset_cnt.
 *
 * After reset is done, will try to re-initialize HW.
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_mrpcim_reset_poll(vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_strip_repl_vlan_tag_enable - Enable strip Repl vlan tag.
 * @devh: Device handle.
 *
 * Enable X3100 strip Repl vlan tag.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_strip_repl_vlan_tag_enable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_strip_repl_vlan_tag_disable - Disable strip Repl vlan tag.
 * @devh: Device handle.
 *
 * Disable X3100 strip Repl vlan tag.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_strip_repl_vlan_tag_disable(
    vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_intr_enable - Enable the interrupts on mrpcim.
 * @devh: HAL device handle.
 *
 * Enable mrpcim interrupts
 *
 * See also: vxge_hal_mrpcim_intr_disable().
 */
vxge_hal_status_e
vxge_hal_mrpcim_intr_enable(vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_intr_disable - Disable the interrupts on mrpcim.
 * @devh: HAL device handle.
 *
 * Disable mrpcim interrupts
 *
 * See also: vxge_hal_mrpcim_intr_enable().
 */
vxge_hal_status_e
vxge_hal_mrpcim_intr_disable(vxge_hal_device_h devh);

/*
 * vxge_hal_mrpcim_lag_config_get - Get the LAG config.
 * @devh: Device handle.
 * @lconfig: LAG Configuration
 *
 * Returns the current LAG configuration.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_lag_config_get(
    vxge_hal_device_h devh,
    vxge_hal_lag_config_t *lconfig);

/*
 * vxge_hal_mrpcim_lag_config_set - Set the LAG config.
 * @devh: Device handle.
 * @lconfig: LAG Configuration
 *
 * Sets the LAG configuration.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_lag_config_set(
    vxge_hal_device_h devh,
    vxge_hal_lag_config_t *lconfig);

/*
 * vxge_hal_mrpcim_getpause_data -Pause frame frame generation and reception.
 * @devh: HAL device handle.
 * @port : Port number 0, 1, or 2
 * @tx : A field to return the pause generation capability of the NIC.
 * @rx : A field to return the pause reception capability of the NIC.
 *
 * Returns the Pause frame generation and reception capability of the NIC.
 * Return value:
 * status
 */
vxge_hal_status_e
vxge_hal_mrpcim_getpause_data(vxge_hal_device_h devh,
    u32 port,
    u32 *tx,
    u32 *rx);

/*
 * vxge_hal_mrpcim_setpause_data -  set/reset pause frame generation.
 * @devh: HAL device handle.
 * @port : Port number 0, 1, or 2
 * @tx: A field that indicates the pause generation capability to be
 * set on the NIC.
 * @rx: A field that indicates the pause reception capability to be
 * set on the NIC.
 *
 * It can be used to set or reset Pause frame generation or reception
 * support of the NIC.
 * Return value:
 * int, returns 0 on Success
 */

vxge_hal_status_e
vxge_hal_mrpcim_setpause_data(
    vxge_hal_device_h devh,
    u32 port,
    u32 tx,
    u32 rx);

/*
 * vxge_hal_mrpcim_bist_test - invokes the MemBist test of the card .
 * @devh: HAL device handle.
 * @data:variable that returns the result of each of the test conducted by
 * the driver.
 *
 * This invokes the MemBist test of the card. We give around
 * 2 secs time for the Test to complete. If it's still not complete
 * within this peiod, we consider that the test failed.
 * Return value:
 * 0 on success and -1 on failure.
 */
vxge_hal_status_e
vxge_hal_mrpcim_bist_test(vxge_hal_device_h devh, u64 *data);

/*
 * vxge_hal_mrpcim_udp_rth_enable - Enable UDP/RTH.
 * @devh: HAL device handle.
 *
 * enable udp rth
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_udp_rth_enable(
    vxge_hal_device_h devh);

/*
 * Virtual Paths
 */

/*
 * struct vxge_hal_vpath_attr_t - Attributes of virtual path
 * @vp_id: Identifier of Virtual Path
 * @ring_attr: Attributes of ring for non-offload receive
 * @fifo_attr: Attributes of fifo for non-offload transmit
 *
 * Attributes of virtual path.  This structure is passed as parameter
 * to the vxge_hal_vpath_open() routine to set the attributes of DMQ, UMQ,
 * ring and fifo. After virtual path is open, iWARP/RDMA module can attach
 * to virtual path.
 */
typedef struct vxge_hal_vpath_attr_t {
	u32				vp_id;
	vxge_hal_ring_attr_t		ring_attr;
	vxge_hal_fifo_attr_t		fifo_attr;
} vxge_hal_vpath_attr_t;

/*
 * vxge_hal_vpath_open - Open a virtual path on a given adapter
 * @devh: handle to device object
 * @attr: Virtual path attributes
 * @cb_fn: Call back to be called to complete an asynchronous function call
 * @client_handle: handle to be returned in the callback
 * @vpath_handle: Buffer to return a handle to the vpath
 *
 * This function is used to open access to virtual path of an
 * adapter for offload, LRO and SPDM operations. This function returns
 * synchronously.
 */
vxge_hal_status_e
vxge_hal_vpath_open(
    vxge_hal_device_h devh,
    vxge_hal_vpath_attr_t *attr,
    vxge_hal_vpath_callback_f cb_fn,
    vxge_hal_client_h client_handle,
    vxge_hal_vpath_h *vpath_handle);

/*
 * vxge_hal_vpath_enable
 * @vpath_handle: Handle to the vpath object
 *
 * This routine clears the vpath reset and puts vpath in service
 */
vxge_hal_status_e
vxge_hal_vpath_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_id - Get virtual path ID
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function returns virtual path id
 */
u32
vxge_hal_vpath_id(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_close - Close the handle got from previous vpath (vpath) open
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function is used to close access to virtual path opened
 * earlier. This function returns synchronously.
 */
vxge_hal_status_e
vxge_hal_vpath_close(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_reset - Resets vpath
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function is used to request a reset of vpath
 */
vxge_hal_status_e
vxge_hal_vpath_reset(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_reset_poll - Poll for reset complete
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function is used to poll for the vpath reset completion
 */
vxge_hal_status_e
vxge_hal_vpath_reset_poll(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_obj_count_get - Get the Object usage count for a given
 *		 virtual path
 * @vpath_handle: Virtal path handle
 * @obj_counts: Buffer to return object counts
 *
 * This function returns the object counts for virtual path. This function
 * returns synchronously.
 */
vxge_hal_status_e
vxge_hal_vpath_obj_count_get(vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_sw_obj_count_t *obj_counts);

/*
 * vxge_hal_vpath_mtu_check - check MTU value for ranges
 * @vpath_handle: Virtal path handle
 * @new_mtu: new MTU value to check
 *
 * Will do sanity check for new MTU value.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_MTU_SIZE - MTU is invalid.
 *
 * See also: vxge_hal_vpath_mtu_set()
 */
vxge_hal_status_e
vxge_hal_device_mtu_check(vxge_hal_vpath_h vpath_handle,
    unsigned long new_mtu);

/*
 * vxge_hal_vpath_mtu_set - Set MTU.
 * @vpath_handle: Virtal path handle
 * @new_mtu: New MTU size to configure.
 *
 * Set new MTU value. Example, to use jumbo frames:
 * vxge_hal_vpath_mtu_set(my_device, 9600);
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mtu_set(vxge_hal_vpath_h vpath_handle,
    unsigned long new_mtu);

typedef enum vxge_hal_vpath_mac_addr_add_mode_e {
	VXGE_HAL_VPATH_MAC_ADDR_ADD_DUPLICATE = 0,
	VXGE_HAL_VPATH_MAC_ADDR_DISCARD_DUPLICATE = 1,
	VXGE_HAL_VPATH_MAC_ADDR_REPLACE_DUPLICATE = 2
} vxge_hal_vpath_mac_addr_add_mode_e;

/*
 * vxge_hal_vpath_mac_addr_add - Add the mac address entry for this vpath
 *		  to MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: MAC address to be added for this vpath into the list
 * @macaddr_mask: MAC address mask for mac_addr
 * @duplicate_mode: Duplicate MAC address add mode. Please see
 *		vxge_hal_vpath_mac_addr_add_mode_e {}
 *
 * Adds the given mac address and mac address mask into the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_delete, vxge_hal_vpath_mac_addr_get and
 * vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_add(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask,
    vxge_hal_vpath_mac_addr_add_mode_e duplicate_mode);

/*
 * vxge_hal_vpath_mac_addr_get - Get the first mac address entry for this vpath
 *		  from MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: First MAC address entry for this vpath in the list
 * @macaddr_mask: MAC address mask for mac_addr
 *
 * Returns the first mac address and mac address mask in the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_get(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask);

/*
 * vxge_hal_vpath_mac_addr_get_next - Get the next mac address entry for this
 *		   vpath from MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: Next MAC address entry for this vpath in the list
 * @macaddr_mask: MAC address mask for mac_addr
 *
 * Returns the next mac address and mac address mask in the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_get
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_get_next(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask);

/*
 * vxge_hal_vpath_mac_addr_delete - Delete the mac address entry for this vpath
 *		  to MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: MAC address to be added for this vpath into the list
 * @macaddr_mask: MAC address mask for macaddr
 *
 * Delete the given mac address and mac address mask into the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_add, vxge_hal_vpath_mac_addr_get and
 * vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_delete(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask);

/*
 * vxge_hal_vpath_vid_add - Add the vlan id entry for this vpath
 *		  to vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: vlan id to be added for this vpath into the list
 *
 * Adds the given vlan id into the list for this  vpath.
 * see also: vxge_hal_vpath_vid_delete, vxge_hal_vpath_vid_get and
 * vxge_hal_vpath_vid_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_add(
    vxge_hal_vpath_h vpath_handle,
    u64 vid);

/*
 * vxge_hal_vpath_vid_get - Get the first vid entry for this vpath
 *		  from vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: Buffer to return vlan id
 *
 * Returns the first vlan id in the list for this vpath.
 * see also: vxge_hal_vpath_vid_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_get(
    vxge_hal_vpath_h vpath_handle,
    u64 *vid);

/*
 * vxge_hal_vpath_vid_get_next - Get the next vid entry for this vpath
 *		  from vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: Buffer to return vlan id
 *
 * Returns the next vlan id in the list for this vpath.
 * see also: vxge_hal_vpath_vid_get
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_get_next(
    vxge_hal_vpath_h vpath_handle,
    u64 *vid);

/*
 * vxge_hal_vpath_vid_delete - Delete the vlan id entry for this vpath
 *		  to vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: vlan id to be added for this vpath into the list
 *
 * Adds the given vlan id into the list for this  vpath.
 * see also: vxge_hal_vpath_vid_add, vxge_hal_vpath_vid_get and
 * vxge_hal_vpath_vid_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_delete(
    vxge_hal_vpath_h vpath_handle,
    u64 vid);

/*
 * vxge_hal_vpath_etype_add - Add the Ethertype entry for this vpath
 *		  to Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: ethertype to be added for this vpath into the list
 *
 * Adds the given Ethertype into the list for this  vpath.
 * see also: vxge_hal_vpath_etype_delete, vxge_hal_vpath_etype_get and
 * vxge_hal_vpath_etype_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_add(
    vxge_hal_vpath_h vpath_handle,
    u64 etype);

/*
 * vxge_hal_vpath_etype_get - Get the first ethertype entry for this vpath
 *		  from Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: Buffer to return Ethertype
 *
 * Returns the first ethype entry in the list for this vpath.
 * see also: vxge_hal_vpath_etype_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_get(
    vxge_hal_vpath_h vpath_handle,
    u64 *etype);

/*
 * vxge_hal_vpath_etype_get_next - Get the next Ethertype entry for this vpath
 *		  from Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: Buffer to return Ethwrtype
 *
 * Returns the next Ethwrtype in the list for this vpath.
 * see also: vxge_hal_vpath_etype_get
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_get_next(
    vxge_hal_vpath_h vpath_handle,
    u64 *etype);

/*
 * vxge_hal_vpath_etype_delete - Delete the Ethertype entry for this vpath
 *		  to Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: ethertype to be added for this vpath into the list
 *
 * Adds the given Ethertype into the list for this  vpath.
 * see also: vxge_hal_vpath_etype_add, vxge_hal_vpath_etype_get and
 * vxge_hal_vpath_etype_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_delete(
    vxge_hal_vpath_h vpath_handle,
    u64 etype);

/*
 * vxge_hal_vpath_port_add - Add the port entry for this vpath
 *		  to port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: if 0 - Src port or 1 - Dest port
 * @protocol: if 0 - TCP or 1 - UDP
 * @port: port to be added for this vpath into the list
 *
 * Adds the given port into the list for this  vpath.
 * see also: vxge_hal_vpath_port_delete, vxge_hal_vpath_port_get and
 * vxge_hal_vpath_port_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_port_add(
    vxge_hal_vpath_h vpath_handle,
    u32 port_type,
    u32 protocol,
    u32 port);

/*
 * vxge_hal_vpath_port_get - Get the first port number entry for this vpath
 *		  from port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: Buffer to return if 0 - Src port or 1 - Dest port
 * @protocol: Buffer to return if 0 - TCP or 1 - UDP
 * @port: Buffer to return port number
 *
 * Returns the first port number entry in the list for this vpath.
 * see also: vxge_hal_vpath_port_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_port_get(
    vxge_hal_vpath_h vpath_handle,
    u32 *port_type,
    u32 *protocol,
    u32 *port);

/*
 * vxge_hal_vpath_port_get_next - Get the next port number entry for this vpath
 *		  from port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: Buffer to return if 0 - Src port or 1 - Dest port
 * @protocol: Buffer to return if 0 - TCP or 1 - UDP
 * @port: Buffer to return port number
 *
 * Returns the next port number entry in the list for this vpath.
 * see also: vxge_hal_vpath_port_get
 *
 */
vxge_hal_status_e
vxge_hal_vpath_port_get_next(
    vxge_hal_vpath_h vpath_handle,
    u32 *port_type,
    u32 *protocol,
    u32 *port);

/*
 * vxge_hal_vpath_port_delete - Delete the port entry for this vpath
 *		  to port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: if 0 - Src port or 1 - Dest port
 * @protocol: if 0 - TCP or 1 - UDP
 * @port: port to be added for this vpath into the list
 *
 * Adds the given port into the list for this  vpath.
 * see also: vxge_hal_vpath_port_add, vxge_hal_vpath_port_get and
 * vxge_hal_vpath_port_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_port_delete(
    vxge_hal_vpath_h vpath_handle,
    u32 port_type,
    u32 protocol,
    u32 port);

typedef enum vxge_hal_rth_algoritms_t {
	RTH_ALG_NONE	= -1,
	RTH_ALG_JENKINS = 0,
	RTH_ALG_MS_RSS	= 1,
	RTH_ALG_CRC32C	= 2
} vxge_hal_rth_algoritms_t;

/*
 * struct vxge_hal_rth_hash_types_t - RTH hash types.
 * @hash_type_tcpipv4_en: Enables RTH field type HashTypeTcpIPv4
 * @hash_type_ipv4_en: Enables RTH field type HashTypeIPv4
 * @hash_type_tcpipv6_en: Enables RTH field type HashTypeTcpIPv6
 * @hash_type_ipv6_en: Enables RTH field type HashTypeIPv6
 * @hash_type_tcpipv6ex_en: Enables RTH field type HashTypeTcpIPv6Ex
 * @hash_type_ipv6ex_en: Enables RTH field type HashTypeIPv6Ex
 *
 * Used to pass RTH hash types to rts_rts_set.
 *
 * See also: vxge_hal_vpath_rts_rth_set(), vxge_hal_vpath_rts_rth_get().
 */
typedef struct vxge_hal_rth_hash_types_t {
	u8 hash_type_tcpipv4_en;
	u8 hash_type_ipv4_en;
	u8 hash_type_tcpipv6_en;
	u8 hash_type_ipv6_en;
	u8 hash_type_tcpipv6ex_en;
	u8 hash_type_ipv6ex_en;
} vxge_hal_rth_hash_types_t;

/*
 * vxge_hal_vpath_udp_rth_disable - Disable UDP/RTH.
 * @vpath_handle: Vpath handle.
 *
 * Disable udp rth
 *
 */
vxge_hal_status_e
vxge_hal_vpath_udp_rth_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_rts_rth_set - Set/configure RTS hashing.
 * @vpath_handle: Virtual Path handle.
 * @algorithm: Algorithm Select
 * @hash_type: Hash Type
 * @bucket_size: no of least significant bits to be used for hashing.
 * @it_switch: Itable switch required
 *
 * Used to set/configure all RTS hashing related stuff.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_itable_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_set(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rth_algoritms_t algorithm,
    vxge_hal_rth_hash_types_t *hash_type,
    u16 bucket_size,
    u16 it_switch);

/*
 * vxge_hal_vpath_rts_rth_get - Read RTS hashing.
 * @vpath_handle: Virtual Path handle.
 * @algorithm: Buffer to return Algorithm Select
 * @hash_type: Buffer to return Hash Type
 * @table_select: Buffer to return active Table
 * @bucket_size: Buffer to return no of least significant bits used for hashing.
 *
 * Used to read all RTS hashing related stuff.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_itable_set(),
 *		vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_rth_algoritms_t *algorithm,
    vxge_hal_rth_hash_types_t *hash_type,
    u8 *table_select,
    u16 *bucket_size);

/*
 * vxge_hal_vpath_rts_rth_key_set - Configure 40byte secret for hash calc.
 *
 * @vpath_handle: Virtual Path ahandle.
 * @KeySize: Number of 64-bit words
 * @Key: upto 40-byte array of 64-bit values
 * This function configures the 40-byte secret which is used for hash
 * calculation.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_key_set(
    vxge_hal_vpath_h vpath_handle,
    u8 KeySize,
    u64 *Key);

/*
 * vxge_hal_vpath_rts_rth_key_get - Read 40byte secret for hash calc.
 *
 * @vpath_handle: Virtual Path ahandle.
 * @KeySize: Number of 64-bit words
 * @Key: Buffer to return the key
 * This function reads the 40-byte secret which is used for hash
 * calculation.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *		vxge_hal_vpath_rts_rth_key_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_key_get(
    vxge_hal_vpath_h vpath_handle,
    u8 KeySize,
    u64 *Key);

/*
 * vxge_hal_vpath_rts_rth_jhash_cfg_set - Configure JHASH algorithm
 *
 * @vpath_handle: Virtual Path ahandle.
 * @golden_ratio: Golden ratio
 * @init_value: Initial value
 * This function configures JENKIN's HASH algorithm
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_jhash_cfg_set(
    vxge_hal_vpath_h vpath_handle,
    u32 golden_ratio,
    u32 init_value);

/*
 * vxge_hal_vpath_rts_rth_jhash_cfg_get - Read JHASH algorithm
 *
 * @vpath_handle: Virtual Path ahandle.
 * @golden_ratio: Buffer to return Golden ratio
 * @init_value: Buffer to return Initial value
 * This function reads JENKIN's HASH algorithm
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *		vxge_hal_vpath_rts_rth_jhash_cfg_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_jhash_cfg_get(
    vxge_hal_vpath_h vpath_handle,
    u32 *golden_ratio,
    u32 *init_value);

/*
 * vxge_hal_vpath_rts_rth_mask_set - Set/configure JHASH mask.
 * @vpath_handle: Virtual Path ahandle.
 * @table_size: Size of the mask table
 * @hash_mask_ipv6sa: IPv6SA Hash Mask
 * @hash_mask_ipv6da: IPv6DA Hash Mask
 * @hash_mask_ipv4sa: IPv4SA Hash Mask
 * @hash_mask_ipv4da: IPv4DA Hash Mask
 * @hash_mask_l4sp: L4SP Hash Mask
 * @hash_mask_l4dp: L4DP Hash Mask
 *
 * Used to set/configure indirection table masks.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_mask_set(
    vxge_hal_vpath_h vpath_handle,
    u32 table_size,
    u32 *hash_mask_ipv6sa,
    u32 *hash_mask_ipv6da,
    u32 *hash_mask_ipv4sa,
    u32 *hash_mask_ipv4da,
    u32 *hash_mask_l4sp,
    u32 *hash_mask_l4dp);

/*
 * vxge_hal_vpath_rts_rth_mask_get - Read JHASH mask.
 * @vpath_handle: Virtual Path ahandle.
 * @table_size: Size of the mask table
 * @hash_mask_ipv6sa: Buffer to return IPv6SA Hash Mask
 * @hash_mask_ipv6da: Buffer to return IPv6DA Hash Mask
 * @hash_mask_ipv4sa: Buffer to return IPv4SA Hash Mask
 * @hash_mask_ipv4da: Buffer to return IPv4DA Hash Mask
 * @hash_mask_l4sp: Buffer to return L4SP Hash Mask
 * @hash_mask_l4dp: Buffer to return L4DP Hash Mask
 *
 * Used to read rth mask.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *	  vxge_hal_vpath_rts_rth_mask_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_mask_get(
    vxge_hal_vpath_h vpath_handle,
    u32 table_size,
    u32 *hash_mask_ipv6sa,
    u32 *hash_mask_ipv6da,
    u32 *hash_mask_ipv4sa,
    u32 *hash_mask_ipv4da,
    u32 *hash_mask_l4sp,
    u32 *hash_mask_l4dp);

/*
 * vxge_hal_vpath_rts_rth_itable_set - Set/configure indirection table (IT).
 * @vpath_handles: Virtual Path handles.
 * @vpath_count: Number of vpath handles passed in vpath_handles
 * @itable: Pointer to indirection table
 * @itable_size: Number of entries in itable
 *
 * Used to set/configure indirection table.
 * It enables the required no of entries in the IT.
 * It adds entries to the IT.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_itable_set(
    vxge_hal_vpath_h *vpath_handles,
    u32 vpath_count,
    u8 *itable,
    u32 itable_size);

/*
 * vxge_hal_vpath_rts_rth_itable_get - Read indirection table (IT).
 * @vpath_handles: Virtual Path handles.
 * @vpath_count: Number of vpath handles passed in vpath_handles
 * @itable: Pointer to the buffer to return indirection table
 * @itable_size: pointer to buffer to return Number of entries in itable
 *
 * Used to read indirection table.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *		vxge_hal_vpath_rts_rth_itable_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_itable_get(
    vxge_hal_vpath_h *vpath_handles,
    u32 vpath_count,
    u8 *itable,
    u32 itable_size);

/*
 * vxge_hal_vpath_rts_rth_clr - Clear RTS hashing.
 * @vpath_handles: Virtual Path handles.
 * @vpath_count: Number of vpath handles passed in vpath_handles
 *
 * This function is used to clear all RTS hashing related stuff.
 *
 * See also: vxge_hal_vpath_rts_rth_set(), vxge_hal_vpath_rts_rth_itable_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_clr(
    vxge_hal_vpath_h *vpath_handles,
    u32 vpath_count);

/*
 * vxge_hal_vpath_promisc_enable - Enable promiscuous mode.
 * @vpath_handle: Vpath handle.
 *
 * Enable promiscuous mode of X3100 operation.
 *
 * See also: vxge_hal_vpath_promisc_disable().
 */
vxge_hal_status_e
vxge_hal_vpath_promisc_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_promisc_disable - Disable promiscuous mode.
 * @vpath_handle: Vpath handle.
 *
 * Disable promiscuous mode of X3100 operation.
 *
 * See also: vxge_hal_vpath_promisc_enable().
 */
vxge_hal_status_e
vxge_hal_vpath_promisc_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_bcast_enable - Enable broadcast
 * @vpath_handle: Vpath handle.
 *
 * Enable receiving broadcasts.
 */
vxge_hal_status_e
vxge_hal_vpath_bcast_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_bcast_disable - Disable broadcast
 * @vpath_handle: Vpath handle.
 *
 * Disable receiving broadcasts.
 */
vxge_hal_status_e
vxge_hal_vpath_bcast_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_mcast_enable - Enable multicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100 multicast addresses.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mcast_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_mcast_disable - Disable  multicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100 multicast addresses.
 * Returns: VXGE_HAL_OK - success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mcast_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_ucast_enable - Enable unicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100 unicast addresses.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_ucast_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_ucast_disable - Disable  unicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100 unicast addresses.
 * Returns: VXGE_HAL_OK - success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_ucast_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_all_vid_enable - Enable all Vlan Ids.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100 vlan ids.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_all_vid_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_all_vid_disable - Disable all Vlan Ids.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100  vlan ids.
 * Returns: VXGE_HAL_OK - success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_all_vid_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_strip_vlan_tag_enable - Enable strip vlan tag.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100  strip vlan tag.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_strip_vlan_tag_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_strip_vlan_tag_disable - Disable strip vlan tag.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100  strip vlan tag.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_strip_vlan_tag_disable(
    vxge_hal_vpath_h vpath_handle);

void
vxge_hal_vpath_dynamic_tti_rtimer_set(vxge_hal_vpath_h vpath_handle,
    u32 timer_val);

void
vxge_hal_vpath_dynamic_rti_rtimer_set(vxge_hal_vpath_h vpath_handle,
    u32 timer_val);

void
vxge_hal_vpath_tti_ci_set(vxge_hal_vpath_h vpath_handle);

void
vxge_hal_vpath_tti_ci_reset(vxge_hal_vpath_h vpath_handle);

void
vxge_hal_vpath_rti_ci_set(vxge_hal_vpath_h vpath_handle);

void
vxge_hal_vpath_rti_ci_reset(vxge_hal_vpath_h vpath_handle);

/*
 * struct vxge_hal_vpath_tpa_params - Vpath TPA Parameters.
 * @tpa_lsov2_en: LSOv2 Behaviour for IP ID roll-over
 *		1 - enable, 0 - disable,
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @tpa_ignore_frame_error: Ignore Frame Error. TPA may detect frame integrity
 *		errors as it processes each frame. If this bit is set to '0',
 *		the TPA will tag such frames as invalid and they will be dropped
 *		by the transmit MAC. If the bit is set to '1',the frame will not
 *		be tagged as "errored".  Detectable errors include:
 *		1) early end-of-frame error, which occurs when the frame ends
 *		before the number of bytes predicted by the IP "total length"
 *		field have been received;
 *		2) IP version mismatches;
 *		3) IPv6 packets that include routing headers that are not type 0
 *		4) Frames which contain IP packets but have an illegal SNAP-OUI
 *		or LLC-CTRL fields, unless IGNORE_SNAP_OUI or IGNORE_LLC_CTRL
 *		are set (see below).
 *		setting the value of this field to VXGE_HAL_DEFAULT_32 - don't
 *		change current setting
 * @tpa_ipv6_keep_searching: If unknown IPv6 header is found,
 *		 0 - stop searching for TCP
 *		 1 - keep searching for TCP
 *		 VXGE_HAL_DEFAULT_32 - don't change current setting
 * @tpa_l4_pshdr_present: If asserted true, indicates the host has provided a
 *		 valid pseudo header for TCP or UDP running over IPv4 or IPv6
 *		 if set to VXGE_HAL_DEFAULT_32 - don't change current setting
 * @tpa_support_mobile_ipv6_hdrs: This register is somewhat equivalent to
 *		 asserting both Hercules register fields LSO_RT2_EN and
 *		 LSO_IPV6_HAO_EN. Enable/disable support for Type 2 Routing
 *		 Headers, and for Mobile-IPv6 Home Address Option (HAO), as
 *		 defined by mobile-ipv6. if set to VXGE_HAL_DEFAULT_32 -
 *		 don't change current setting
 *
 * See also: vxge_hal_vpath_tpa_set()
 */
typedef struct vxge_hal_vpath_tpa_params {
	u32	tpa_lsov2_en;
	u32	tpa_ignore_frame_error;
	u32	tpa_ipv6_keep_searching;
	u32	tpa_l4_pshdr_present;
	u32	tpa_support_mobile_ipv6_hdrs;
} vxge_hal_vpath_tpa_params;

/*
 * vxge_hal_vpath_tpa_set - Set tpa parameters.
 * @vpath_handle: Virtual Path ahandle.
 * @params: vxge_hal_vpath_tpa_params {} structure with parameters
 *
 * The function	sets the tpa parametrs for the vpath.
 *
 * See also: vxge_hal_vpath_tpa_params {}
 */
vxge_hal_status_e
vxge_hal_vpath_tpa_set(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_tpa_params *params);

/*
 * struct vxge_hal_vpath_rpa_params - Vpath RPA Parameters.
 *
 * @rpa_ipv4_tcp_incl_ph: Determines if the pseudo-header is included in the
 *		calculation of the L4 checksum that is passed to the host. This
 *		field applies to TCP/IPv4 packets only. This field affects both
 *		non-offload and LRO traffic. Note that the RPA always includes
 *		the pseudo-header in the "Checksum Ok" L4 checksum calculation
 *		i.e. the checksum that decides whether a frame is a candidate to
 *		be offloaded.
 *		0 - Do not include the pseudo-header in L4 checksum calculation.
 *		This setting should be used if the adapter is incorrectly
 *		calculating the pseudo-header.
 *		1 - Include the pseudo-header in L4 checksum calculation
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_ipv6_tcp_incl_ph: Determines whether the pseudo-header is included in
 *		the calculation of the L4 checksum that is passed to the host.
 *		This field applies to TCP/IPv6 packets only. This field affects
 *		both non-offload and LRO traffic. Note that the RPA always
 *		includes the pseudo-header in the "Checksum Ok" L4 checksum
 *		calculation. i.e. the checksum that decides whether a frame
 *		is a candidate to be offloaded.
 *		0 - Do not include the pseudo-header in L4 checksum calculation.
 *		This setting should be used if the adapter is incorrectly
 *		calculating the pseudo-header.
 *		1 - Include the pseudo-header in L4 checksum calculation
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_ipv4_udp_incl_ph: Determines whether the pseudo-header is included in
 *		the calculation of the L4 checksum that is passed to the host.
 *		This field applies to UDP/IPv4 packets only. It only affects
 *		non-offload traffic(since UDP frames are not candidates for LRO)
 *		0 - Do not include the pseudo-header in L4 checksum calculation.
 *		This setting should be used if the adapter is incorrectly
 *		calculating the pseudo-header.
 *		1 - Include the pseudo-header in L4 checksum calculation
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_ipv6_udp_incl_ph: Determines if the pseudo-header is included in the
 *		calculation of the L4 checksum that is passed to the host. This
 *		field applies to UDP/IPv6 packets only. It only affects
 *		non-offload traffic(since UDP frames are not candidates for LRO)
 *		0 - Do not include the pseudo-header in L4 checksum calculation.
 *		This setting should be used if the adapter is incorrectly
 *		calculating the pseudo-header.
 *		1 - Include the pseudo-header in L4 checksum calculation
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_l4_incl_cf: Determines whether the checksum field (CF) of the received
 *		frame is included in the calculation of the L4 checksum that is
 *		passed to the host. This field affects both non-offload and LRO
 *		traffic. Note that the RPA always includes the checksum field in
 *		the "Checksum Ok" L4 checksum calculation -- i.e. the checksum
 *		that decides whether a frame is a candidate to be offloaded.
 *		0 - Do not include the checksum field in L4 checksum calculation
 *		1 - Include the checksum field in L4 checksum calculation
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_strip_vlan_tag: Strip VLAN Tag enable/disable. Instructs the device to
 *		remove the VLAN tag from all received tagged frames that are not
 *		replicated at the internal L2 switch.
 *		0 - Do not strip the VLAN tag.
 *		1 - Strip the VLAN tag. Regardless of this setting,VLAN tags are
 *		always placed into the RxDMA descriptor.
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_l4_comp_csum: Determines whether the calculated L4 checksum should be
 *		complemented before it is passed to the host This field affects
 *		both non-offload and LRO traffic.
 *		0 - Do not complement the calculated L4 checksum.
 *		1 - Complement the calculated L4 checksum
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_l3_incl_cf: Determines whether the checksum field (CF) of the received
 *		frame is included in the calculation of the L3 checksum that is
 *		passed to the host. This field affects both non-offload and LRO
 *		traffic. Note that the RPA always includes the checksum field in
 *		the "Checksum Ok" L3 checksum calculation -- i.e. the checksum
 *		that decides whether a frame is a candidate to be offloaded.
 *		0 - Do not include the checksum field in L3 checksum calculation
 *		1 - Include the checksum field in L3 checksum calculation
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_l3_comp_csum: Determines whether the calculated L3 checksum should be
 *		complemented before it is passed to the host This field affects
 *		both non-offload and LRO traffic.
 *		0 - Do not complement the calculated L3 checksum.
 *		1 - Complement the calculated L3 checksum
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 * @rpa_ucast_all_addr_en: Enables frames with any unicast address (as its
 *		destination address) to be passed to the host.
 *		Setting this field to VXGE_HAL_DEFAULT_32 - don't change current
 *		setting
 * @rpa_mcast_all_addr_en: Enables frames with any multicast address (as its
 *		destination address) to be passed to the host.
 *		Setting this field to VXGE_HAL_DEFAULT_32 - don't change current
 *		setting
 * @rpa_bcast_en: Enables frames with any broadicast address (as its
 *		destination address) to be passed to the host.
 *		Setting this field to VXGE_HAL_DEFAULT_32 - don't change current
 *		setting
 * @rpa_all_vid_en: romiscuous mode, it overrides the value held in this field.
 *		0 - Disable;
 *		1 - Enable
 *		VXGE_HAL_DEFAULT_32 - don't change current setting
 *		Note: RXMAC_GLOBAL_CFG.AUTHORIZE_VP_ALL_VID must be set to
 *		allow this.
 *
 * See also: vxge_hal_vpath_rpa_set()
 */
typedef struct vxge_hal_vpath_rpa_params {
	u32	rpa_ipv4_tcp_incl_ph;
	u32	rpa_ipv6_tcp_incl_ph;
	u32	rpa_ipv4_udp_incl_ph;
	u32	rpa_ipv6_udp_incl_ph;
	u32	rpa_l4_incl_cf;
	u32	rpa_strip_vlan_tag;
	u32	rpa_l4_comp_csum;
	u32	rpa_l3_incl_cf;
	u32	rpa_l3_comp_csum;
	u32	rpa_ucast_all_addr_en;
	u32	rpa_mcast_all_addr_en;
	u32	rpa_bcast_en;
	u32	rpa_all_vid_en;
} vxge_hal_vpath_rpa_params;

/*
 * vxge_hal_vpath_rpa_set - Set rpa parameters.
 * @vpath_handle: Virtual Path ahandle.
 * @params: vxge_hal_vpath_rpa_params {} structure with parameters
 *
 * The function	sets the rpa parametrs for the vpath.
 *
 * See also: vxge_hal_vpath_rpa_params {}
 */
vxge_hal_status_e
vxge_hal_vpath_rpa_set(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_rpa_params *params);

/*
 * vxge_hal_vpath_poll_rx - Poll Rx	od Virtual Path	for completed
 *			 descriptors and process the same.
 * @vpath_handle: Virtual Path ahandle.
 * @got_rx: Buffer to return the flag set if receive interrupt is occured
 *
 * The function	polls the Rx for the completed	descriptors and	calls
 * the upper-layer driver (ULD)	via supplied completion	callback.
 *
 * Returns: VXGE_HAL_OK, if the polling is completed successful.
 * VXGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: vxge_hal_vpath_poll_tx()
 */
vxge_hal_status_e
vxge_hal_vpath_poll_rx(
    vxge_hal_vpath_h vpath_handle,
    u32 *got_rx);

/*
 * vxge_hal_vpath_poll_tx - Poll Tx	for completed descriptors and process
 *			 the same.
 * @vpath_handle: Virtual Path ahandle.
 * @got_tx: Buffer to return the flag set if transmit interrupt is occured
 *
 * The function	polls the Tx for the completed	descriptors and	calls
 * the upper-layer driver (ULD)	via supplied completion callback.
 *
 * Returns: VXGE_HAL_OK, if the polling is completed successful.
 * VXGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: vxge_hal_vpath_poll_rx().
 */
vxge_hal_status_e
vxge_hal_vpath_poll_tx(
    vxge_hal_vpath_h vpath_handle,
    u32 *got_tx);


/*
 * vxge_hal_vpath_intr_enable - Enable vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 * @op: One of the vxge_hal_vpath_intr_e enumerated values specifying
 *	  the type(s) of interrupts to enable.
 *
 * Enable vpath interrupts. The function is to be executed the last in
 * vpath initialization sequence.
 *
 * See also: vxge_hal_vpath_intr_disable()
 */
vxge_hal_status_e
vxge_hal_vpath_intr_enable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_intr_disable - Disable vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 * @op: One of the vxge_hal_vpath_intr_e enumerated values specifying
 *	  the type(s) of interrupts to disable.
 *
 * Disable vpath interrupts.
 *
 * See also: vxge_hal_vpath_intr_enable()
 */
vxge_hal_status_e
vxge_hal_vpath_intr_disable(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_mask_all - Mask all vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	all vpath interrupts.
 *
 * See also: vxge_hal_vpath_unmask_all()
 */
void
vxge_hal_vpath_mask_all(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_unmask_all - Unmask all vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask all vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_all()
 */
void
vxge_hal_vpath_unmask_all(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_begin_irq - Begin IRQ processing.
 * @vpath_handle: Virtual Path handle.
 * @skip_alarms: Do not clear the alarms
 * @reason: "Reason" for the interrupt,	the value of vpath's
 *			general_int_status register.
 *
 * The function	performs two actions, It first checks whether (shared IRQ) the
 * interrupt was raised	by the device. Next, it	masks the device interrupts.
 *
 * Note:
 * vxge_hal_vpath_begin_irq() does not flush MMIO writes through the
 * bridge. Therefore, two back-to-back interrupts are potentially possible.
 * It is the responsibility	of the ULD to make sure	that only one
 * vxge_hal_vpath_continue_irq() runs at a time.
 *
 * Returns: 0, if the interrupt	is not "ours" (note that in this case the
 * vpath remain enabled).
 * Otherwise, vxge_hal_vpath_begin_irq() returns 64bit general adapter
 * status.
 * See also: vxge_hal_vpath_handle_irq()
 */
vxge_hal_status_e
vxge_hal_vpath_begin_irq(
    vxge_hal_vpath_h vpath_handle,
    u32 skip_alarms,
    u64 *reason);

/*
 * vxge_hal_vpath_continue_irq - Continue handling IRQ:	process	all
 *				completed descriptors.
 * @vpath_handle: Virtual Path handle.
 *
 * Process completed descriptors and unmask the	vpath interrupts.
 *
 * The vxge_hal_vpath_continue_irq() calls upper-layer driver (ULD)
 * via supplied completion callback.
 *
 * Note	that the vxge_hal_vpath_continue_irq is	part of	the _fast_ path.
 * To optimize the processing, the function does _not_ check for
 * errors and alarms.
 *
 * Returns: VXGE_HAL_OK.
 *
 * See also: vxge_hal_vpath_handle_irq(),
 * vxge_hal_ring_rxd_next_completed(),
 * vxge_hal_fifo_txdl_next_completed(), vxge_hal_ring_callback_f {},
 * vxge_hal_fifo_callback_f {}.
 */
vxge_hal_status_e
vxge_hal_vpath_continue_irq(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpathe_handle_irq - Handle vpath IRQ.
 * @vpath_handle: Virtual Path handle.
 * @skip_alarms: Do not clear the alarms
 *
 * Perform the complete	handling of the	line interrupt.	The function
 * performs two	calls.
 * First it uses vxge_hal_vpath_begin_irq() to check the reason for
 * the interrupt and mask the vpath interrupts.
 * Second, it calls vxge_hal_vpath_continue_irq() to process all
 * completed descriptors and re-enable the interrupts.
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_ERR_WRONG_IRQ - (shared)	IRQ	produced by	other device.
 *
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq().
 */
vxge_hal_status_e
vxge_hal_vpath_handle_irq(
    vxge_hal_vpath_h vpath_handle,
    u32 skip_alarms);

/*
 * vxge_hal_vpath_mask_tx - Mask Tx interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	Tx device interrupts.
 *
 * See also: vxge_hal_vpath_unmask_tx(), vxge_hal_vpath_mask_rx(),
 * vxge_hal_vpath_clear_tx().
 */
void
vxge_hal_vpath_mask_tx(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_clear_tx - Acknowledge (that is, clear) the
 * condition that has caused the TX	interrupt.
 * @vpath_handle: Virtual Path handle.
 *
 * Acknowledge (that is, clear)	the	condition that has caused
 * the Tx interrupt.
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq(),
 * vxge_hal_vpath_clear_rx(), vxge_hal_vpath_mask_tx().
 */
void
vxge_hal_vpath_clear_tx(
    vxge_hal_vpath_h vpath_handle);


/*
 * vxge_hal_vpath_unmask_tx - Unmask Tx	interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask Tx vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_tx(), vxge_hal_vpath_clear_tx().
 */
void
vxge_hal_vpath_unmask_tx(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_mask_rx - Mask Rx	interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_unmask_rx(), vxge_hal_vpath_mask_tx(),
 * vxge_hal_vpath_clear_rx().
 */
void
vxge_hal_vpath_mask_rx(
    vxge_hal_vpath_h vpath_handle);


/*
 * vxge_hal_vpath_clear_rx - Acknowledge (that is, clear) the
 * condition that has caused the RX	interrupt.
 * @vpath_handle: Virtual Path handle.
 *
 * Acknowledge (that is, clear)	the condition that has caused
 * the Rx interrupt.
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq(),
 * vxge_hal_vpath_clear_tx(), vxge_hal_vpath_mask_rx().
 */
void
vxge_hal_vpath_clear_rx(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_unmask_rx - Unmask Rx	interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_rx(), vxge_hal_vpath_clear_rx().
 */
void
vxge_hal_vpath_unmask_rx(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_mask_tx_rx - Mask Tx and Rx	interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	Tx and Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_unmask_tx_rx(), vxge_hal_vpath_clear_tx_rx().
 */
void
vxge_hal_vpath_mask_tx_rx(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_clear_tx_rx - Acknowledge (that is, clear) the
 * condition that has caused the Tx and RX interrupt.
 * @vpath_handle: Virtual Path handle.
 *
 * Acknowledge (that is, clear)	the condition that has caused
 * the Tx and Rx interrupt.
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq(),
 * vxge_hal_vpath_clear_tx_rx(), vxge_hal_vpath_mask_tx_rx().
 */
void
vxge_hal_vpath_clear_tx_rx(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_unmask_tx_rx - Unmask Tx and Rx interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask Tx and Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_tx_rx(), vxge_hal_vpath_clear_tx_rx().
 */
void
vxge_hal_vpath_unmask_tx_rx(
    vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_alarm_process - Process Alarms.
 * @vpath: Virtual Path.
 * @skip_alarms: Do not clear the alarms
 *
 * Process vpath alarms.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_alarm_process(
    vxge_hal_vpath_h vpath_handle,
    u32 skip_alarms);

/* NEWCODE */

vxge_hal_status_e
vxge_hal_vpath_mf_msix_set(vxge_hal_vpath_h vpath_handle,
    int *tim_msix_id, int alarm_msix_id);
void
vxge_hal_vpath_mf_msix_clear(vxge_hal_vpath_h vpath_handle, int msix_id);
void
vxge_hal_vpath_mf_msix_mask(vxge_hal_vpath_h vpath_handle, int msix_id);
void
vxge_hal_vpath_mf_msix_unmask(vxge_hal_vpath_h vpath_handle, int msix_id);

/* NEWCODE */

/*
 * vxge_hal_vpath_msix_mode - Is MSIX enabled?
 * @vpath_handle: Virtual Path handle.
 *
 * Returns 0 if MSI is enabled for the specified device,
 * non-zero otherwise.
 */
u32
vxge_hal_vpath_msix_mode(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_msix_set - Associate MSIX vectors with TIM interrupts and
 *			   alrms
 * @vpath_handle: Virtual Path handle.
 * @tim_msix_id: MSIX vectors associated with VXGE_HAL_VPATH_MSIX_MAX number of
 *		interrupts(Valid numbers 0 to 3).
 * @alarm_msix_id: MSIX vector for alarm (Valid numbers 0 to 3).
 *
 * This API will associate a given MSIX vector numbers with the four TIM
 * interrupts and alarm interrupt.
 */
vxge_hal_status_e
vxge_hal_vpath_msix_set(vxge_hal_vpath_h vpath_handle,
    int *tim_msix_id,
    int alarm_msix_id);

/*
 * vxge_hal_vpath_msix_mask - Mask MSIX Vector.
 * @vpath_handle: Virtual Path handle.
 * @msix_id:  MSIX ID
 *
 * The function masks the msix interrupt for the given msix_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, VXGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_msix_mask(vxge_hal_vpath_h vpath_handle, int msix_id);

/*
 * vxge_hal_vpath_msix_clear - Clear MSIX Vector.
 * @vpath_handle: Virtual Path handle.
 * @msix_id:  MSI ID
 *
 * The function clears the msix interrupt for the given msix_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, VXGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_msix_clear(vxge_hal_vpath_h vpath_handle, int msix_id);

/*
 * vxge_hal_vpath_msix_unmask - Unmask MSIX Vector.
 * @vpath_handle: Virtual Path handle.
 * @msix_id:  MSI ID
 *
 * The function unmasks the msix interrupt for the given msix_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, VXGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_msix_unmask(vxge_hal_vpath_h vpath_handle, int msix_id);

/*
 * vxge_hal_vpath_msix_mask_all - Mask all MSIX vectors for the vpath.
 * @vpath_handle: Virtual Path handle.
 *
 * The function masks the msix interrupt for the given vpath
 *
 */
void
vxge_hal_vpath_msix_mask_all(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_msix_unmask_all - Unmask all MSIX vectors for the vpath.
 * @vpath_handle: Virtual Path handle.
 *
 * The function unmasks the msix interrupt for the given vpath
 *
 */
void
vxge_hal_vpath_msix_unmask_all(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_vpath_is_rxdmem_leak - Check for the rxd memory leak.
 * @vpath_handle: Virtual Path handle.
 *
 * The function checks for the rxd memory leak.
 *
 */
u32
vxge_hal_vpath_is_rxdmem_leak(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_rldram_test - offline test for access to the RldRam chip on
 *			the NIC
 * @devh: HAL device handle.
 * @data: variable that returns the result of each of the test
 * conducted by the driver.
 *
 * This is one of the offline test that tests the read and write
 * access to the RldRam chip on the NIC.
 * Return value:
 * 0 on success.
 */
vxge_hal_status_e
vxge_hal_rldram_test(
    vxge_hal_device_h devh,
    u64 *data);

/*
 * vxge_hal_check_alignment - Check buffer alignment and	calculate the
 *		    "misaligned"	portion.
 * @dma_pointer: DMA address of	the	buffer.
 * @size: Buffer size, in bytes.
 * @alignment: Alignment "granularity" (see	below),	in bytes.
 * @copy_size: Maximum number of bytes to "extract"	from the buffer
 * (in order to	spost it as	a separate scatter-gather entry). See below.
 *
 * Check buffer	alignment and calculate	"misaligned" portion, if exists.
 * The buffer is considered	aligned	if its address is multiple of
 * the specified @alignment. If	this is	the case,
 * vxge_hal_check_alignment() returns zero.
 * Otherwise, vxge_hal_check_alignment() uses the last argument,
 * @copy_size,
 * to calculate	the	size to	"extract" from the buffer. The @copy_size
 * may or may not be equal @alignment. The difference between these two
 * arguments is	that the @alignment is used to make the	decision: aligned
 * or not aligned. While the @copy_size	is used	to calculate the portion
 * of the buffer to "extract", i.e. to post as a separate entry in the
 * transmit descriptor.	For example, the combination
 * @alignment=8	and @copy_size=64 will work okay on AMD Opteron boxes.
 *
 * Note: @copy_size should be a	multiple of @alignment. In many	practical
 * cases @copy_size and	@alignment will	probably be equal.
 *
 * See also: vxge_hal_fifo_txdl_buffer_set_aligned().
 */
u32
vxge_hal_check_alignment(
    dma_addr_t dma_pointer,
    u32 size,
    u32 alignment,
    u32 copy_size);


void
vxge_hw_vpath_set_zero_rx_frm_len(vxge_hal_device_h devh, u32 vp_id);

void
vxge_hw_vpath_wait_receive_idle(vxge_hal_device_h devh, u32 vp_id,
    u32 *count, u32 *total_count);

#define	VXGE_HW_MIN_SUCCESSIVE_IDLE_COUNT	5
#define	VXGE_HW_MAX_POLLING_COUNT		160


__EXTERN_END_DECLS

#include <dev/vxge/include/vxge-os-debug.h>

#endif	/* VXGE_HAL_LL_H */
