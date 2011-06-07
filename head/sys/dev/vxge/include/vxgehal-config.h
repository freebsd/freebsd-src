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

#ifndef	VXGE_HAL_CONFIG_H
#define	VXGE_HAL_CONFIG_H

__EXTERN_BEGIN_DECLS

#define	VXGE_HAL_USE_FLASH_DEFAULT			VXGE_HAL_DEFAULT_32

#define	VXGE_HAL_MAX_INTR_PER_VP			4

#define	VXGE_HAL_VPATH_MSIX_MAX				4

#define	VXGE_HAL_VPATH_INTR_TX				0

#define	VXGE_HAL_VPATH_INTR_RX				1

#define	VXGE_HAL_VPATH_INTR_EINTA			2

#define	VXGE_HAL_VPATH_INTR_BMAP			3

#define	WAIT_FACTOR					1
/*
 * struct vxge_hal_driver_config_t - HAL driver object configuration.
 *
 * @level: Debug Level. See vxge_debug_level_e {}
 *
 * Currently this structure contains just a few basic values.
 */
typedef struct vxge_hal_driver_config_t {
	vxge_debug_level_e level;
} vxge_hal_driver_config_t;

/*
 * struct vxge_hal_wire_port_config_t - Wire Port configuration (Physical ports)
 * @port_id: Port number
 * @media: Transponder type.
 * @mtu: mtu size used on this port.
 * @autoneg_mode: The device supports several mechanisms to auto-negotiate the
 *		port data rate. The Fixed mode essentially means no
 *		auto-negotiation and the data rate is determined by the RATE
 *		field of this register. The MDIO-based mode determines the data
 *		rate by reading MDIO registers in the external PHY chip. The
 *		Backplane Ethernet mode using parallel detect and/or
 *		DME-signaled exchange of page information with the PHY chip in
 *		order to figure out the proper rate.
 *		00 - Fixed
 *		01 - MDIO-based
 *		10 - Backplane Ethernet
 *		11 - Reserved
 * @autoneg_rate: When MODE is set to Fixed, then this field determines the data
 *		rate of the port.
 *		0 - 1G
 *		1 - 10G
 * @fixed_use_fsm: When MODE is set to Fixed, then this field determines whether
 *		a processor (i.e. F/W or host driver) or hardware-based state
 *		machine is used to run the auto-negotiation.
 *		0 - Use processor. Either on-chip F/W or host-based driver used.
 *		1 - Use H/W state machine
 * @antp_use_fsm: When MODE is set to ANTP (Auto-Negotiation for Twisted Pair),
 *		this field determines whether a processor (F/W or host driver)
 *		or hardware-based state machine is used to talk to the PHY chip
 *		via the MDIO interface.
 *		0 - Use processor. Either on-chip F/W or host-based driver used.
 *		1 - Use H/W state machine
 * @anbe_use_fsm: When MODE is set to ANBE-based, then this field determines
 *		whether a processor (i.e. F/W or host driver) or hardware-based
 *		state machine is used to talk to the Backplane Ethernet logic
 *		inside the device
 *		0 - Use processor. Either on-chip F/W or host-based driver used.
 *		1 - Use H/W state machine
 * @link_stability_period: Timeout for the link stability
 * @port_stability_period: Timeout for the port stability
 * @tmac_en: TMAC enable. 0 - Disable; 1 - Enable
 * @rmac_en: RMAC enable. 0 - Disable; 1 - Enable
 * @tmac_pad: Determines whether padding is appended to transmitted frames.
 *		0 - No padding appended
 *		1 - Pad to 64 bytes (not including preamble/SFD)
 * @tmac_pad_byte: The byte that is used to pad
 * @tmac_util_period: The sampling period over which the transmit utilization
 *		   is calculated.
 * @rmac_strip_fcs: Determines whether FCS of received frames is removed by the
 *		MAC or sent to the host.
 *		0 - Send FCS to host.
 *		1 - FCS removed by MAC.
 * @rmac_prom_en: Enable/Disable promiscuous mode. In promiscuous mode all
 *		received frames are passed to the host. PROM_EN overrules the
 *		configuration determined by the UCAST_ALL_ADDR_EN,
 *		MCAST_ALL_ADDR_EN and ALL_VID_EN fields of RXMAC_VCFG, as well
 *		as the configurable discard fields in RMAC_ERR_CFG_PORTn.
 *		Note: PROM_EN does not overrule DISCARD_PFRM (i.e. discard of
 *		pause frames by receive MAC is controlled solely by
 *		DISCARD_PFRM).
 *		0 - Disable
 *		1 - Enable
 * @rmac_discard_pfrm: Determines whether received pause frames are discarded at
 *		the receive MAC or passed to the host.
 *		Note: Other MAC control frames are always passed to the host.
 *		0 - Send to host.
 *		1 - Pause frames discarded by MAC.
 * @rmac_util_period: The sampling period over which the receive utilization
 *		   is calculated.
 * @rmac_strip_pad: Determines whether padding of received frames is removed by
 *		 the MAC or sent to the host.
 * @rmac_bcast_en: Enable frames containing broadcast address to be
 *		passed to the host.
 * @rmac_pause_gen_en: Received pause generation enable.
 * @rmac_pause_rcv_en: Receive pause enable.
 * @rmac_pause_time: The value to be inserted in outgoing pause frames.
 *		Has units of pause quanta (one pause quanta = 512 bit times).
 * @limiter_en: Enables logic that limits the contribution that any one receive
 *		queue can have on the transmission of pause frames. This avoids
 *		a situation where the adapter will permanently send pause frames
 *		due to a receive VPATH that is either undergoing a long reset or
 *		is in a dead state.
 *		0 - Don't limit the contribution of any queue. If any queue's
 *		fill level sits above the high threshold, then a pause frame
 *		is sent.
 *		1 - Place a cap on the number of pause frames that are sent
 *		because any one queue has crossed its high threshold.
 *		See MAX_LIMIT for more details.
 * @max_limit: Contains the value that is loaded into the per-queue limiting
 *		counters that exist in the flow control logic. Essentially,
 *		this represents the maximum number of pause frames that are sent
 *		due to any one particular queue having crossed its high
 *		threshold. Each counter is set to this max limit the first time
 *		the corresponding queue's high threshold is crossed. The counter
 *		decrements each time the queue remains above the high threshold
 *		and the adapter requests pause frame transmission. Once the
 *		counter expires that queue no longer contributes to pause frame
 *		transmission requests. The queue's fill level must drop below
 *		the low pause threshold before it is once again allowed to
 *		contribute. Note: This field is only used when LIMITER_EN is set
 *		to 1.
 *
 * Wire Port Configuration
 */
typedef struct vxge_hal_wire_port_config_t {

		u32				port_id;
#define	VXGE_HAL_WIRE_PORT_PORT0				0
#define	VXGE_HAL_WIRE_PORT_PORT1				1
#define	VXGE_HAL_WIRE_PORT_MAX_PORTS		    VXGE_HAL_MAC_MAX_WIRE_PORTS

		u32				media;
#define	VXGE_HAL_WIRE_PORT_MIN_MEDIA				0
#define	VXGE_HAL_WIRE_PORT_MEDIA_SR				0
#define	VXGE_HAL_WIRE_PORT_MEDIA_SW				1
#define	VXGE_HAL_WIRE_PORT_MEDIA_LR				2
#define	VXGE_HAL_WIRE_PORT_MEDIA_LW				3
#define	VXGE_HAL_WIRE_PORT_MEDIA_ER				4
#define	VXGE_HAL_WIRE_PORT_MEDIA_EW				5
#define	VXGE_HAL_WIRE_PORT_MAX_MEDIA				5
#define	VXGE_HAL_WIRE_PORT_MEDIA_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				mtu;
#define	VXGE_HAL_WIRE_PORT_MIN_INITIAL_MTU	    VXGE_HAL_MIN_MTU
#define	VXGE_HAL_WIRE_PORT_MAX_INITIAL_MTU	    VXGE_HAL_MAX_MTU
#define	VXGE_HAL_WIRE_PORT_DEF_INITIAL_MTU	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				autoneg_mode;
#define	VXGE_HAL_WIRE_PORT_AUTONEG_MODE_FIXED			0
#define	VXGE_HAL_WIRE_PORT_AUTONEG_MODE_ANTP			1
#define	VXGE_HAL_WIRE_PORT_AUTONEG_MODE_ANBE			2
#define	VXGE_HAL_WIRE_PORT_AUTONEG_MODE_RESERVED		3
#define	VXGE_HAL_WIRE_PORT_AUTONEG_MODE_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				autoneg_rate;
#define	VXGE_HAL_WIRE_PORT_AUTONEG_RATE_1G			0
#define	VXGE_HAL_WIRE_PORT_AUTONEG_RATE_10G			1
#define	VXGE_HAL_WIRE_PORT_AUTONEG_RATE_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				fixed_use_fsm;
#define	VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_PROCESSOR		0
#define	VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_HW			1
#define	VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

		u32				antp_use_fsm;
#define	VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_PROCESSOR		0
#define	VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_HW			1
#define	VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				anbe_use_fsm;
#define	VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_PROCESSOR		0
#define	VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_HW			1
#define	VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				link_stability_period;
#define	VXGE_HAL_WIRE_PORT_MIN_LINK_STABILITY_PERIOD		0x0 /* 0s */
#define	VXGE_HAL_WIRE_PORT_MAX_LINK_STABILITY_PERIOD		0xF /* 2s */
#define	VXGE_HAL_WIRE_PORT_DEF_LINK_STABILITY_PERIOD		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				port_stability_period;
#define	VXGE_HAL_WIRE_PORT_MIN_PORT_STABILITY_PERIOD		0x0 /* 0s */
#define	VXGE_HAL_WIRE_PORT_MAX_PORT_STABILITY_PERIOD		0xF /* 2s */
#define	VXGE_HAL_WIRE_PORT_DEF_PORT_STABILITY_PERIOD		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_en;
#define	VXGE_HAL_WIRE_PORT_TMAC_ENABLE				1
#define	VXGE_HAL_WIRE_PORT_TMAC_DISABLE				0
#define	VXGE_HAL_WIRE_PORT_TMAC_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_en;
#define	VXGE_HAL_WIRE_PORT_RMAC_ENABLE				1
#define	VXGE_HAL_WIRE_PORT_RMAC_DISABLE				0
#define	VXGE_HAL_WIRE_PORT_RMAC_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_pad;
#define	VXGE_HAL_WIRE_PORT_TMAC_NO_PAD				0
#define	VXGE_HAL_WIRE_PORT_TMAC_64B_PAD				1
#define	VXGE_HAL_WIRE_PORT_TMAC_PAD_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_pad_byte;
#define	VXGE_HAL_WIRE_PORT_MIN_TMAC_PAD_BYTE			0
#define	VXGE_HAL_WIRE_PORT_MAX_TMAC_PAD_BYTE			255
#define	VXGE_HAL_WIRE_PORT_DEF_TMAC_PAD_BYTE	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_util_period;
#define	VXGE_HAL_WIRE_PORT_MIN_TMAC_UTIL_PERIOD			0
#define	VXGE_HAL_WIRE_PORT_MAX_TMAC_UTIL_PERIOD			15
#define	VXGE_HAL_WIRE_PORT_DEF_TMAC_UTIL_PERIOD	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_strip_fcs;
#define	VXGE_HAL_WIRE_PORT_RMAC_STRIP_FCS			1
#define	VXGE_HAL_WIRE_PORT_RMAC_SEND_FCS_TO_HOST		0
#define	VXGE_HAL_WIRE_PORT_RMAC_STRIP_FCS_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_prom_en;
#define	VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_ENABLE			1
#define	VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_DISABLE			0
#define	VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_discard_pfrm;
#define	VXGE_HAL_WIRE_PORT_RMAC_DISCARD_PFRM			1
#define	VXGE_HAL_WIRE_PORT_RMAC_SEND_PFRM_TO_HOST		0
#define	VXGE_HAL_WIRE_PORT_RMAC_DISCARD_PFRM_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_util_period;
#define	VXGE_HAL_WIRE_PORT_MIN_RMAC_UTIL_PERIOD			0
#define	VXGE_HAL_WIRE_PORT_MAX_RMAC_UTIL_PERIOD			15
#define	VXGE_HAL_WIRE_PORT_DEF_RMAC_UTIL_PERIOD	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_pause_gen_en;
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_ENABLE		1
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_DISABLE		0
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_pause_rcv_en;
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_ENABLE		1
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_DISABLE		0
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_pause_time;
#define	VXGE_HAL_WIRE_PORT_MIN_RMAC_HIGH_PTIME			16
#define	VXGE_HAL_WIRE_PORT_MAX_RMAC_HIGH_PTIME			65535
#define	VXGE_HAL_WIRE_PORT_DEF_RMAC_HIGH_PTIME	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				limiter_en;
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_ENABLE		1
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_DISABLE		0
#define	VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				max_limit;
#define	VXGE_HAL_WIRE_PORT_MIN_RMAC_MAX_LIMIT			0
#define	VXGE_HAL_WIRE_PORT_MAX_RMAC_MAX_LIMIT			255
#define	VXGE_HAL_WIRE_PORT_DEF_RMAC_MAX_LIMIT	    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_wire_port_config_t;

/*
 * struct vxge_hal_switch_port_config_t - Switch Port configuration(vm-vm port)
 * @mtu: mtu size used on this port.
 * @tmac_en: TMAC enable. 0 - Disable; 1 - Enable
 * @rmac_en: RMAC enable. 0 - Disable; 1 - Enable
 * @tmac_pad: Determines whether padding is appended to transmitted frames.
 *		0 - No padding appended
 *		1 - Pad to 64 bytes (not including preamble/SFD)
 * @tmac_pad_byte: The byte that is used to pad
 * @tmac_util_period: The sampling period over which the transmit utilization
 *		   is calculated.
 * @rmac_strip_fcs: Determines whether FCS of received frames is removed by the
 *		MAC or sent to the host.
 *		0 - Send FCS to host.
 *		1 - FCS removed by MAC.
 * @rmac_prom_en: Enable/Disable promiscuous mode. In promiscuous mode all
 *		received frames are passed to the host. PROM_EN overrules the
 *		configuration determined by the UCAST_ALL_ADDR_EN,
 *		MCAST_ALL_ADDR_EN and ALL_VID_EN fields of RXMAC_VCFG, as well
 *		as the configurable discard fields in RMAC_ERR_CFG_PORTn.
 *		Note: PROM_EN does not overrule DISCARD_PFRM (i.e. discard of
 *		pause frames by receive MAC is controlled solely by
 *              DISCARD_PFRM).
 *		0 - Disable
 *		1 - Enable
 * @rmac_discard_pfrm: Determines whether received pause frames are discarded at
 *		the receive MAC or passed to the host.
 *		Note: Other MAC control frames are always passed to the host.
 *		0 - Send to host.
 *		1 - Pause frames discarded by MAC.
 * @rmac_util_period: The sampling period over which the receive utilization
 *		   is calculated.
 * @rmac_strip_pad: Determines whether padding of received frames is removed by
 *		 the MAC or sent to the host.
 * @rmac_bcast_en: Enable frames containing broadcast address to be
 *		passed to the host.
 * @rmac_pause_gen_en: Received pause generation enable.
 * @rmac_pause_rcv_en: Receive pause enable.
 * @rmac_pause_time: The value to be inserted in outgoing pause frames.
 *		Has units of pause quanta (one pause quanta = 512 bit times).
 * @limiter_en: Enables logic that limits the contribution that any one receive
 *		queue can have on the transmission of pause frames. This avoids
 *		a situation where the adapter will permanently send pause frames
 *		due to a receive VPATH that is either undergoing a long reset or
 *		is in a dead state.
 *		0 - Don't limit the contribution of any queue. If any queue's
 *		fill level sits above the high threshold, then a pause frame
 *		is sent.
 *		1 - Place a cap on the number of pause frames that are sent
 *		because any one queue has crossed its high threshold.
 *		See MAX_LIMIT for more details.
 * @max_limit: Contains the value that is loaded into the per-queue limiting
 *		counters that exist in the flow control logic. Essentially,
 *		this represents the maximum number of pause frames that are sent
 *		due to any one particular queue having crossed its high
 *		threshold. Each counter is set to this max limit the first time
 *		the corresponding queue's high threshold is crossed. The counter
 *		decrements each time the queue remains above the high threshold
 *		and the adapter requests pause frame transmission. Once the
 *		counter expires that queue no longer contributes to pause frame
 *		transmission requests. The queue's fill level must drop below
 *		the low pause threshold before it is once again allowed to
 *		contribute. Note: This field is only used when LIMITER_EN is set
 *		to 1.
 *
 * Switch Port Configuration
 */
typedef struct vxge_hal_switch_port_config_t {

		u32				mtu;
#define	VXGE_HAL_SWITCH_PORT_MIN_INITIAL_MTU		    VXGE_HAL_MIN_MTU
#define	VXGE_HAL_SWITCH_PORT_MAX_INITIAL_MTU		    VXGE_HAL_MAX_MTU
#define	VXGE_HAL_SWITCH_PORT_DEF_INITIAL_MTU	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_en;
#define	VXGE_HAL_SWITCH_PORT_TMAC_ENABLE				1
#define	VXGE_HAL_SWITCH_PORT_TMAC_DISABLE				0
#define	VXGE_HAL_SWITCH_PORT_TMAC_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_en;
#define	VXGE_HAL_SWITCH_PORT_RMAC_ENABLE				1
#define	VXGE_HAL_SWITCH_PORT_RMAC_DISABLE				0
#define	VXGE_HAL_SWITCH_PORT_RMAC_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_pad;
#define	VXGE_HAL_SWITCH_PORT_TMAC_NO_PAD				0
#define	VXGE_HAL_SWITCH_PORT_TMAC_64B_PAD				1
#define	VXGE_HAL_SWITCH_PORT_TMAC_PAD_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_pad_byte;
#define	VXGE_HAL_SWITCH_PORT_MIN_TMAC_PAD_BYTE			0
#define	VXGE_HAL_SWITCH_PORT_MAX_TMAC_PAD_BYTE			255
#define	VXGE_HAL_SWITCH_PORT_DEF_TMAC_PAD_BYTE	    VXGE_HAL_USE_FLASH_DEFAULT

		u32				tmac_util_period;
#define	VXGE_HAL_SWITCH_PORT_MIN_TMAC_UTIL_PERIOD			0
#define	VXGE_HAL_SWITCH_PORT_MAX_TMAC_UTIL_PERIOD			15
#define	VXGE_HAL_SWITCH_PORT_DEF_TMAC_UTIL_PERIOD   VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_strip_fcs;
#define	VXGE_HAL_SWITCH_PORT_RMAC_STRIP_FCS			1
#define	VXGE_HAL_SWITCH_PORT_RMAC_SEND_FCS_TO_HOST		0
#define	VXGE_HAL_SWITCH_PORT_RMAC_STRIP_FCS_DEFAULT VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_prom_en;
#define	VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_ENABLE			1
#define	VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_DISABLE			0
#define	VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_DEFAULT   VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_discard_pfrm;
#define	VXGE_HAL_SWITCH_PORT_RMAC_DISCARD_PFRM			1
#define	VXGE_HAL_SWITCH_PORT_RMAC_SEND_PFRM_TO_HOST		0
#define	VXGE_HAL_SWITCH_PORT_RMAC_DISCARD_PFRM_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_util_period;
#define	VXGE_HAL_SWITCH_PORT_MIN_RMAC_UTIL_PERIOD			0
#define	VXGE_HAL_SWITCH_PORT_MAX_RMAC_UTIL_PERIOD			15
#define	VXGE_HAL_SWITCH_PORT_DEF_RMAC_UTIL_PERIOD   VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_pause_gen_en;
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_ENABLE		1
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_DISABLE		0
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_pause_rcv_en;
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_ENABLE		1
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_DISABLE		0
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				rmac_pause_time;
#define	VXGE_HAL_SWITCH_PORT_MIN_RMAC_HIGH_PTIME			16
#define	VXGE_HAL_SWITCH_PORT_MAX_RMAC_HIGH_PTIME			65535
#define	VXGE_HAL_SWITCH_PORT_DEF_RMAC_HIGH_PTIME    VXGE_HAL_USE_FLASH_DEFAULT

		u32				limiter_en;
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_ENABLE		1
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_DISABLE		0
#define	VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

		u32				max_limit;
#define	VXGE_HAL_SWITCH_PORT_MIN_RMAC_MAX_LIMIT			0
#define	VXGE_HAL_SWITCH_PORT_MAX_RMAC_MAX_LIMIT			255
#define	VXGE_HAL_SWITCH_PORT_DEF_RMAC_MAX_LIMIT	    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_switch_port_config_t;

/*
 * struct vxge_hal_mac_config_t - MAC configuration (Physical ports).
 * @wire_port_config: Wire Port configuration
 * @switch_port_config: Switch Port configuration
 * @network_stability_period: The wait period for network stability
 * @mc_pause_threshold: Contains thresholds for pause frame generation
 *		  for queues 0 through 15. The threshold value indicates portion
 *		of the individual receive buffer queue size. Thresholds have a
 *		range of 0 to 255, allowing 256 possible watermarks in a queue.
 * @tmac_perma_stop_en: Controls TMAC reaction to double ECC errors on its
 *		internal SRAMs.
 *		0 - Disable TMAC permanent stop
 *		1 - Enable TMAC permanent stop whenever double ECC errors are
 *		detected on the internal transmit SRAMs.
 * @tmac_tx_switch_dis: Allows the user to disable the switching of transmit
 *		frames back to the receive path.
 *		0 - Tx frames are switched based on the result of the DA lookup
 *		1 - The DA lookup result is ignored and all traffic is sent to
 *		the wire.
 *		Note that this register field does not impact the multicast
 *		replication on the receive side.
 * @tmac_lossy_switch_en: Controls the behaviour of the internal Tx to Rx switch
 *		in the event of back-pressure on the receive path due to
 *		priority given to traffic arriving off the wire or simply due to
 *		a full receive external buffer. br>
 *		0 - No frames are dropped on the switch path. Instead, in the
 *		event of back-pressure, the transmit path is backed up.
 *		1 - Whenever back-pressure is present and the next frame is
 *		bound for the switch path, then the frame is dropped.
 *		If it is also destined for the transmit wire, it is still
 *		sent there.
 *		Note: HIP traffic that is bound for the switch path is never
 *		dropped - the transmit path is forced to backup.
 * @tmac_lossy_wire_en: Controls the behaviour of the TMAC when the wire path
 *		is unavailable. This occurs when the target wire port is down.
 *		0 - No frames are dropped on the wire path. Instead,in the event
 *		of port failure, the transmit path is backed up.
 *		1 - Whenever a wire port is down and the next frame is bound for
 *		that port, then the frame is dropped. If it is also destined
 *		for the switch path, it is still sent there.
 * @tmac_bcast_to_wire_dis: Suppresses the transmission of broadcast frames to
 *		the wire.
 *		0 - Transmit broadcast frames are sent out the wire and also
 *		sent along the switch path.
 *		1 - Transmit broadcast frame are only sent along the switch path
 * @tmac_bcast_to_switch_dis: Suppresses the transmission of broadcast frames
 *		along the switch path.
 *		0 - Transmit broadcast frames are sent out the wire and also
 *		sent along the switch path.
 *		1 - Transmit broadcast frame are only sent out the wire.
 * @tmac_host_append_fcs_en: Suppresses the H/W from appending the FCS to the
 *		end of transmitted frames. The host is responsible for tacking
 *		on the 4-byte FCS at the end of the frame.
 *		0 - Normal operation. H/W appends FCS to all transmitted frames.
 *		1 - Host appends FCS to frame. Transmit MAC passes it through
 * @tpa_support_snap_ab_n: When set to 0, the TPA will accept LLC-SAP values of
 *		0xAB as valid. If set to 1, the TPA rejects LLC-SAP values of
 *		0xAB (only 0xAA is accepted).
 * @tpa_ecc_enable_n: Allows ECC protection of TPA internal memories to be
 *		disabled without having to disable ECC protection for entire
 *		chip.
 *		0 - Disable TPA ECC protection
 *		1 - Enable TPA ECC protection.
 *		Note: If chip-wide ECC protection is disabled, then so is TPA
 *		ECC protection.
 * @rpa_ignore_frame_err: Ignore Frame Error. The RPA may detect frame integrity
 *		errors as it processes each received frame. If this bit is set
 *		to '0', the RPA will tag such frames as "errored" in the RxDMA
 *		descriptor. If the bit is set to '1', the frame will not be
 *		tagged as "errored".
 *		Detectable errors include:
 *		1) early end-of-frame error, which occurs when the frame ends
 *		before the number of bytes predicted by the IP "total length"
 *		field have been received;
 *		2) IP version mismatches;
 *		3) IPv6 packets that include routing headers that are not type 0
 *		4) Frames which contain IP packets but have an illegal SNAP-OUI
 *		or LLC-CTRL fields, unless IGNORE_SNAP_OUI or IGNORE_LLC_CTRL
 *		are set
 * @rpa_support_snap_ab_n: When set to 0, the RPA will accept LLC-SAP values of
 *		0xAB as valid. If set to 1, the RPA rejects LLC-SAP values of
 *		0xAB (only 0xAA is accepted).
 * @rpa_search_for_hao: Enable searching for the Home Address Option.If this bit
 *		is set, the RPA will parse through Destination Address Headers
 *		searching for the H.A.O. If the bit is not set, the RPA will not
 *		perform a search and these headers will effectively be ignored.
 * @rpa_support_ipv6_mobile_hdrs: Enable/disable support for the mobile-ipv6
 *		Home Address Option (HAO) and Route 2 Routing Headers,as defined
 *		in RFC 3775.
 *		0 - Do not support mobile IPv6.
 *		1 - Support mobile IPv6
 * @rpa_ipv6_stop_searching: Enable/disable unknown IPv6 extension header
 *		parsing. If the adapter discovers an unknown extension header,
 *		it can either continue to search for a L4 protocol, or stop
 *		searching.
 *		0 - do not stop searching for L4 when an unknown header is
 *		encountered.
 *		1 - stop searching when an unknown header is encountered.
 * @rpa_no_ps_if_unknown: Enable/disable pseudo-header inclusion if an unknown
 *		IPv6 extension header is encountered.
 *		If this bit is set to '1' and an unknown routing header or IPv6
 *		extension header is discovered, the L4 checksum will not include
 *		a pseudo-header.
 *		If it is set to '0', the adapter will use the addresses found
 *		in the IPv6 base header, and/or the addresses found in a Routing
 *		Header or Home Address Option (if it is enabled).
 *		This applies to frames not on LRO sessions only. For frames on
 *		LRO sessions, the pseudo-header is always included in the L4
 *		checksum
 * @rpa_search_for_etype: For receive traffic steering purposes, indicates
 *		whether the RPA should parse through the LLC header to find the
 *		Ethertype of the packet.
 *		0 - RPA presents the 802.3 length/type field, which for an
 *		LLC-encoded frame is interpreted as a length.
 *		1 - RPA parses the LLC-header and presents the Ethertype to the
 *		traffic steering logic. When SEARCH_FOR_ETYPE is set and a jumbo
 *		snap frame is received then GLOBAL_PA_CFG.EN_JS determines the
 *		value that is presented to the traffic steering logic. If EN_JS
 *		is set, then the RPA parses inside the header to find the
 *		Ethertype, while if EN_JS is not set the RPA presents 0x8870.
 * @rpa_repl_l4_comp_csum: Controls whether or not to complement the L4 checksum
 *		after the final calculation.
 *		0: Do not complement the L4 checksum.
 *		1: Complement the L4 checksum.
 *		For the behaviour on non-replicated frames see FAU_RPA_VCFG.
 * @rpa_repl_l3_incl_cf: Controls whether or not to include the L3 checksum
 *		field in the checksum calculation.
 *		0: Do not include the L3 checksum field in checksum calculation
 *		1: Include the L4 checksum field in the checksum calculation.
 *		For the behaviour on non-replicated frames see FAU_RPA_VCFG.
 * @rpa_repl_l3_comp_csum: Controls whether or not to complement the L3 checksum
 *		after the final calculation.
 *		0: Do not complement the L3 checksum.
 *		1: Complement the L3 checksum.
 *		For the behaviour on non-replicated frames see FAU_RPA_VCFG.
 * @rpa_repl_ipv4_tcp_incl_ph: For received frames that are replicated at the
 *		internal L2 switch, determines whether the pseudo-header is
 *		included in the calculation of the L4 checksum that is passed to
 *		the host.
 * @rpa_repl_ipv6_tcp_incl_ph: For received frames that are replicated at the
 *		internal L2 switch, determines whether the pseudo-header is
 *		included in the calculation of the L4 checksum that is passed to
 *		the host.
 * @rpa_repl_ipv4_udp_incl_ph: For received frames that are replicated at the
 *		internal L2 switch, determines whether the pseudo-header is
 *		included in the calculation of the L4 checksum that is passed to
 *		the host.
 * @rpa_repl_ipv6_udp_incl_ph: For received frames that are replicated at the
 *		internal L2 switch, determines whether the pseudo-header is
 *		included in the calculation of the L4 checksum that is passed to
 *		the host.
 * @rpa_repl_l4_incl_cf: For received frames that are replicated at the internal
 *		L2 switch, determines whether the checksum field (CF) of the
 *		received frame is included in the calculation of the L4
 *		checksum that is passed to the host.
 * @rpa_repl_strip_vlan_tag: Strip VLAN Tag enable/disable. Instructs the device
 *		to remove the VLAN tag from all received tagged frames that
 *		are replicated at the internal L2 switch (i.e. multicast frames
 *		that are placed in the replication queue).
 *		0 - Do not strip the VLAN tag.
 *		1 - Strip the VLAN tag.
 *		Regardless of this setting, VLAN tags are always placed into
 *		the RxDMA descriptor.
 *
 * MAC configuration. This includes various aspects of configuration, including:
 * - Pause frame threshold;
 * - sampling rate to calculate link utilization;
 * - enabling/disabling broadcasts.
 *
 * See X3100 ER User Guide for more details.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the vxge_hal_mac_config_t {} structure. Please refer to the
 * corresponding include file.
 */
typedef struct vxge_hal_mac_config_t {

	vxge_hal_wire_port_config_t	wire_port_config[	\
						VXGE_HAL_MAC_MAX_WIRE_PORTS];
	vxge_hal_switch_port_config_t	switch_port_config;

	u32				network_stability_period;
#define	VXGE_HAL_MAC_MIN_NETWORK_STABILITY_PERIOD		0x0 /* 0s */
#define	VXGE_HAL_MAC_MAX_NETWORK_STABILITY_PERIOD		0x7 /* 2s */
#define	VXGE_HAL_MAC_DEF_NETWORK_STABILITY_PERIOD   VXGE_HAL_USE_FLASH_DEFAULT

	u32				mc_pause_threshold[16];
#define	VXGE_HAL_MAC_MIN_MC_PAUSE_THRESHOLD			0
#define	VXGE_HAL_MAC_MAX_MC_PAUSE_THRESHOLD			254
#define	VXGE_HAL_MAC_DEF_MC_PAUSE_THRESHOLD	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tmac_perma_stop_en;
#define	VXGE_HAL_MAC_TMAC_PERMA_STOP_ENABLE			1
#define	VXGE_HAL_MAC_TMAC_PERMA_STOP_DISABLE			0
#define	VXGE_HAL_MAC_TMAC_PERMA_STOP_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tmac_tx_switch_dis;
#define	VXGE_HAL_MAC_TMAC_TX_SWITCH_ENABLE			0
#define	VXGE_HAL_MAC_TMAC_TX_SWITCH_DISABLE			1
#define	VXGE_HAL_MAC_TMAC_TX_SWITCH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tmac_lossy_switch_en;
#define	VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_ENABLE			1
#define	VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_DISABLE			0
#define	VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tmac_lossy_wire_en;
#define	VXGE_HAL_MAC_TMAC_LOSSY_WIRE_ENABLE			1
#define	VXGE_HAL_MAC_TMAC_LOSSY_WIRE_DISABLE			0
#define	VXGE_HAL_MAC_TMAC_LOSSY_WIRE_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tmac_bcast_to_wire_dis;
#define	VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_DISABLE			1
#define	VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_ENABLE			0
#define	VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tmac_bcast_to_switch_dis;
#define	VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_DISABLE		1
#define	VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_ENABLE		0
#define	VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tmac_host_append_fcs_en;
#define	VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_ENABLE		1
#define	VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_DISABLE		0
#define	VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tpa_support_snap_ab_n;
#define	VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_LLC_SAP_AB		0
#define	VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_LLC_SAP_AA		1
#define	VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_DEFAULT  VXGE_HAL_USE_FLASH_DEFAULT

	u32				tpa_ecc_enable_n;
#define	VXGE_HAL_MAC_TPA_ECC_ENABLE_N_ENABLE			1
#define	VXGE_HAL_MAC_TPA_ECC_ENABLE_N_DISABLE			0
#define	VXGE_HAL_MAC_TPA_ECC_ENABLE_N_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_ignore_frame_err;
#define	VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_ENABLE		1
#define	VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_DISABLE		0
#define	VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_support_snap_ab_n;
#define	VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_ENABLE		1
#define	VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_DISABLE		0
#define	VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_DEFAULT  VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_search_for_hao;
#define	VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_ENABLE			1
#define	VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_DISABLE			0
#define	VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_support_ipv6_mobile_hdrs;
#define	VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_ENABLE	1
#define	VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_DISABLE	0
#define	VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_DEFAULT	\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_ipv6_stop_searching;
#define	VXGE_HAL_MAC_RPA_IPV6_STOP_SEARCHING			1
#define	VXGE_HAL_MAC_RPA_IPV6_DONT_STOP_SEARCHING		0
#define	VXGE_HAL_MAC_RPA_IPV6_STOP_SEARCHING_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_no_ps_if_unknown;
#define	VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_ENABLE		1
#define	VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_DISABLE		0
#define	VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_DEFAULT   VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_search_for_etype;
#define	VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_ENABLE		1
#define	VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_DISABLE		0
#define	VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_DEFAULT   VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_l4_comp_csum;
#define	VXGE_HAL_MAC_RPA_REPL_L4_COMP_CSUM_ENABLE		1
#define	VXGE_HAL_MAC_RPA_REPL_L4_COMP_CSUM_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_l4_COMP_CSUM_DEFAULT  VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_l3_incl_cf;
#define	VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_ENABLE			1
#define	VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_l3_comp_csum;
#define	VXGE_HAL_MAC_RPA_REPL_L3_COMP_CSUM_ENABLE		1
#define	VXGE_HAL_MAC_RPA_REPL_L3_COMP_CSUM_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_l3_COMP_CSUM_DEFAULT  VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_ipv4_tcp_incl_ph;
#define	VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_ENABLE		1
#define	VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_ipv6_tcp_incl_ph;
#define	VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_ENABLE		1
#define	VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_ipv4_udp_incl_ph;
#define	VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_ENABLE		1
#define	VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_ipv6_udp_incl_ph;
#define	VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_ENABLE		1
#define	VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_l4_incl_cf;
#define	VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_ENABLE			1
#define	VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_repl_strip_vlan_tag;
#define	VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_ENABLE		1
#define	VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DISABLE		0
#define	VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_mac_config_t;

/*
 * struct vxge_hal_lag_port_config_t - LAG Port configuration(For privileged
 *				  mode driver only)
 *
 * @port_id: Port Id
 * @lag_en: Enables or disables the port from joining a link aggregation group.
 *		If link aggregation is enabled and this port is disabled, then
 *		this port does not carry traffic (it is not associated with an
 *		Aggregator). Both this bit and port_enabled from the physical
 *		layer logic must be asserted to permit the Receive machine to
 *		move beyond the PORT_DISABLED state.
 *		0 - Disable;
 *		1 - Enable;
 * @discard_slow_proto: Discard received frames that contain the Slow Protocols
 *		Multicast address (IEEE 802.3-2005 Clause 43B) -- Such frames
 *		are used for link aggregation Marker Protocol and for LACP.
 *		0 - Pass to host;
 *		1 - Discard;
 * @host_chosen_aggr: When the host is running the Link Aggregation Control
 *		algorithm, this field determines which aggregator is attached
 *		to this port. This field is only valid when LAG_LACP_CFG.EN is 0
 *		0 - Aggregator 0 is attached to this port.
 *		1 - Aggregator 1 is attached to this port.
 * @discard_unknown_slow_proto: Discard received frames that contain the Slow
 *		Protocols Multicast address (IEEE 802.3-2005 Clause 43B),
 *		but have an unknown Slow Protocols PDU.
 *		0 - Pass to host
 *		1 - Discard
 *		Note: This field is only relevant when DISCARD_SLOW_PROTO
 *		is set to 0.
 * @actor_port_num: The port number assigned to the port. Port Number 0 is
 *		reserved and must not be assigned to any port.
 * @actor_port_priority: The priority value assigned to the port.
 * @actor_key_10g: The port's administrative Key when auto-negotiated to 10Gbps
 *		The null (all zeros) Key value is not available for local use.
 * @actor_key_1g: The port's administrative Key when auto-negotiated to 1Gbps.
 *		The null (all zeros) Key value is not available for local use.
 * @actor_lacp_activity: Indicates the Activity control value for this port.
 *		0 - Passive LACP
 *		1 - Active LACP
 * @actor_lacp_timeout: Indicates the Timeout control value for this port.
 *		0 - Long Timeout
 *		1 - Short Timeout
 * @actor_aggregation: Indicates if the port is a potential candidate for
 *		aggregation.
 *		0 - Link is Individual
 *		1 - Link is Aggregateable
 * @actor_synchronization: Indicates if the port is in sync.
 *		0 - Link is out of sync; it is in the wrong Aggregation
 *		1 - Link is in sync (allocated to the correct Link Aggregation
 *		Group, the group is associated with a compatible Aggregator,
 *		and the identity of the Link Aggregation Group is consistent
 *		with the System ID and operational Key information transmitted)
 * @actor_collecting: Indicates whether collecting of incoming frames is enabled
 *		on this port.
 *		0 - Not collecting
 *		1 - Collection is enabled
 * @actor_distributing: Indicates whether distribution of outgoing frames is
 *		enabled on this port.
 *		0 - Not distributing
 *		1 - Distribution is enabled
 * @actor_defaulted: Indicates whether the Actor's Receive state machine is
 *		using administratively configured information for the Partner.
 *		0 - The operational Partner info has been received in a LACPDU
 *		1 - The operation Partner info is using administrative defaults
 * @actor_expired: Indicates whether the Actor's Receive state machine is in the
 *		EXPIRED state.
 *		0 - Not in the EXPIRED state
 *		1 - Is in the EXPIRED state
 * @partner_sys_pri: The administrative default for the System Priority
 *		component of the System Identifier of the Partner.
 * @partner_key: The administrative default for the Partner's Key. The null
 *		(all zeros) Key value is not available for local use.
 * @partner_port_num: The administrative default for the Port Number component
 *		of the Partner's Port Identifier.
 * @partner_port_priority: The administrative default for the Port Identifier
 *		component of the Partner's Port Identifier.
 * @partner_lacp_activity: Indicates the Activity control value for this port.
 *		0 - Passive LACP
 *		1 - Active LACP
 * @partner_lacp_timeout: Indicates the Timeout control value for this port.
 *		0 - Long Timeout
 *		1 - Short Timeout
 * @partner_aggregation: Indicates if the port is a potential candidate for
 *		aggregation.
 *		0 - Link is Individual
 *		1 - Link is Aggregateable
 * @partner_synchronization: Indicates if the port is in sync.
 *		0 - Link is out of sync; it is in the wrong Aggregation
 *		1 - Link is in sync (allocated to the correct Link Aggregation
 *		Group, the group is associated with a compatible Aggregator,
 *		and the identity of the Link Aggregation Group is consistent
 *		with the System ID and operational Key information transmitted)
 * @partner_collecting: Indicates whether collecting of incoming frames is
 *		enabled on this port.
 *		0 - Not collecting
 *		1 - Collection is enabled.
 *		Note: According to IEEE 802.3-2005, the value of the
 *		partner_collecting field of this register must be the same as
 *		the value of the partner_synchronization field of this register
 * @partner_distributing: Indicates whether distribution of outgoing frames is
 *		enabled on this port.
 *		0 - Not distributing
 *		1 - Distribution is enabled
 * @partner_defaulted: Indicates whether the Actor's Receive state machine is
 *		using administratively configured information for the Partner.
 *		0 - The operational Partner information has been received in
 *		    a LACPDU
 *		1 - The operation Partner information is using administrative
 *		    defaults
 * @partner_expired: Indicates whether the Actor's Receive state machine is in
 *		the expired state.
 *		0 - Not in the EXPIRED state
 *		1 - Is in the EXPIRED state
 * @partner_mac_addr: Default value for the MAC address of the Partner.
 *
 * This structure is configuration for LAG Port of device
 */
typedef struct vxge_hal_lag_port_config_t {
	u32	port_id;
#define	VXGE_HAL_LAG_PORT_PORT_ID_0				1
#define	VXGE_HAL_LAG_PORT_PORT_ID_1				2
#define	VXGE_HAL_LAG_PORT_MAX_PORTS		    VXGE_HAL_MAC_MAX_WIRE_PORTS

	u32	lag_en;
#define	VXGE_HAL_LAG_PORT_LAG_EN_DISABLE			0
#define	VXGE_HAL_LAG_PORT_LAG_EN_ENABLE				1
#define	VXGE_HAL_LAG_PORT_LAG_EN_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	discard_slow_proto;
#define	VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_DISABLE		0
#define	VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_ENABLE		1
#define	VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	host_chosen_aggr;
#define	VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_0			0
#define	VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_1			1
#define	VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_DEFAULT  VXGE_HAL_USE_FLASH_DEFAULT

	u32	discard_unknown_slow_proto;
#define	VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_DISABLE	0
#define	VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_ENABLE	1
#define	VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_DEFAULT	\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_port_num;
#define	VXGE_HAL_LAG_PORT_MIN_ACTOR_PORT_NUM			0
#define	VXGE_HAL_LAG_PORT_MAX_ACTOR_PORT_NUM			65535
#define	VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_NUM	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_port_priority;
#define	VXGE_HAL_LAG_PORT_MIN_ACTOR_PORT_PRIORITY		0
#define	VXGE_HAL_LAG_PORT_MAX_ACTOR_PORT_PRIORITY		65535
#define	VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_PRIORITY    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_key_10g;
#define	VXGE_HAL_LAG_PORT_MIN_ACTOR_KEY_10G			0
#define	VXGE_HAL_LAG_PORT_MAX_ACTOR_KEY_10G			65535
#define	VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_10G	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_key_1g;
#define	VXGE_HAL_LAG_PORT_MIN_ACTOR_KEY_1G			0
#define	VXGE_HAL_LAG_PORT_MAX_ACTOR_KEY_1G			65535
#define	VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_1G	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_lacp_activity;
#define	VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_PASSIVE		0
#define	VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_ACTIVE		1
#define	VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_lacp_timeout;
#define	VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_LONG		0
#define	VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_SHORT		1
#define	VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_aggregation;
#define	VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_INDIVIDUAL		0
#define	VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_AGGREGATEABLE	1
#define	VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_DEFAULT VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_synchronization;
#define	VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_OUT_OF_SYNC	0
#define	VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_IN_SYNC		1
#define	VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_collecting;
#define	VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_DISABLE		0
#define	VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_ENABLE		1
#define	VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_DEFAULT  VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_distributing;
#define	VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DISABLE		0
#define	VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_ENABLE		1
#define	VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_defaulted;
#define	VXGE_HAL_LAG_PORT_ACTOR_DEFAULTED			0
#define	VXGE_HAL_LAG_PORT_ACTOR_NOT_DEFAULTED			1
#define	VXGE_HAL_LAG_PORT_ACTOR_DEFAULTED_DEFAULT   VXGE_HAL_USE_FLASH_DEFAULT

	u32	actor_expired;
#define	VXGE_HAL_LAG_PORT_ACTOR_EXPIRED			0
#define	VXGE_HAL_LAG_PORT_ACTOR_NOT_EXPIRED			1
#define	VXGE_HAL_LAG_PORT_ACTOR_EXPIRED_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_sys_pri;
#define	VXGE_HAL_LAG_PORT_MIN_PARTNER_SYS_PRI			0
#define	VXGE_HAL_LAG_PORT_MAX_PARTNER_SYS_PRI			65535
#define	VXGE_HAL_LAG_PORT_DEF_PARTNER_SYS_PRI	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_key;
#define	VXGE_HAL_LAG_PORT_MIN_PARTNER_KEY			0
#define	VXGE_HAL_LAG_PORT_MAX_PARTNER_KEY			65535
#define	VXGE_HAL_LAG_PORT_DEF_PARTNER_KEY	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_port_num;
#define	VXGE_HAL_LAG_PORT_MIN_PARTNER_PORT_NUM			0
#define	VXGE_HAL_LAG_PORT_MAX_PARTNER_PORT_NUM			65535
#define	VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_NUM	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_port_priority;
#define	VXGE_HAL_LAG_PORT_MIN_PARTNER_PORT_PRIORITY		0
#define	VXGE_HAL_LAG_PORT_MAX_PARTNER_PORT_PRIORITY		65535
#define	VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_PRIORITY VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_lacp_activity;
#define	VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_PASSIVE		0
#define	VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_ACTIVE		1
#define	VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_lacp_timeout;
#define	VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_LONG		0
#define	VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_SHORT		1
#define	VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_aggregation;
#define	VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_INDIVIDUAL	0
#define	VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_AGGREGATEABLE	1
#define	VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_synchronization;
#define	VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_OUT_OF_SYNC	0
#define	VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_IN_SYNC	1
#define	VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_DEFAULT	\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_collecting;
#define	VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_DISABLE		0
#define	VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_ENABLE		1
#define	VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_distributing;
#define	VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DISABLE		0
#define	VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_ENABLE		1
#define	VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_defaulted;
#define	VXGE_HAL_LAG_PORT_PARTNER_DEFAULTED			0
#define	VXGE_HAL_LAG_PORT_PARTNER_NOT_DEFAULTED			1
#define	VXGE_HAL_LAG_PORT_PARTNER_DEFAULTED_DEFAULT VXGE_HAL_USE_FLASH_DEFAULT

	u32	partner_expired;
#define	VXGE_HAL_LAG_PORT_PARTNER_EXPIRED			0
#define	VXGE_HAL_LAG_PORT_PARTNER_NOT_EXPIRED			1
#define	VXGE_HAL_LAG_PORT_PARTNER_EXPIRED_DEFAULT   VXGE_HAL_USE_FLASH_DEFAULT

	macaddr_t partner_mac_addr;

} vxge_hal_lag_port_config_t;

/*
 * struct vxge_hal_lag_aggr_config_t - LAG Aggregator configuration
 *				  (For privileged mode driver only)
 *
 * @aggr_id: Aggregator Id
 * @mac_addr: The MAC address assigned to the Aggregator.
 * @use_port_mac_addr: Indicates whether the Aggregator should use:
 *		0 - the address specified in this register
 *		1 - the station address of one of the ports to which
 *		    it is attached
 * @mac_addr_sel: Indicates which port address to use, if use_port_mac_addr
 *		is set and two ports are attached to the aggregator:
 *		0 - the station address of port 0
 *		1 - the station address of port 1.
 * @admin_key: The Aggregator's administrative Key under most circumstances
 *		(see alt_admin_key for exceptions). The null (all zeros) Key
 *		value is not available for local use.
 * This structure is configuration for LAG Aggregators of device
 */
typedef struct vxge_hal_lag_aggr_config_t {
	u32	aggr_id;
#define	VXGE_HAL_LAG_AGGR_AGGR_ID_1				1
#define	VXGE_HAL_LAG_AGGR_AGGR_ID_2				2
#define	VXGE_HAL_LAG_AGGR_MAX_PORTS		    VXGE_HAL_MAC_MAX_AGGR_PORTS

	macaddr_t mac_addr;

	u32	use_port_mac_addr;
#define	VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_DISBALE		0
#define	VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_ENABLE		1
#define	VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_DEFAULT VXGE_HAL_USE_FLASH_DEFAULT

	u32	mac_addr_sel;
#define	VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_PORT_0			0
#define	VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_PORT_1			1
#define	VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	admin_key;
#define	VXGE_HAL_LAG_AGGR_MIN_ADMIN_KEY				0
#define	VXGE_HAL_LAG_AGGR_MAX_ADMIN_KEY				65535
#define	VXGE_HAL_LAG_AGGR_DEF_ADMIN_KEY		    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_lag_aggr_config_t;

/*
 * struct vxge_hal_lag_la_config_t - LAG Link Aggregator mode configuration(
 *				For privileged mode driver only)
 *
 * @tx_discard: When the state of the port state attached to the Tx Aggregator
 *		is not Distributing, this field determines whether frames from
 *		the Frame Distributor are discarded by the Aggregator Mux
 * @distrib_alg_sel: Configures the link aggregation distribution algorithm,
 *		which determines the destination port of each wire-bound frame.
 *		0x0 - The source VPATH determines the target port and the
 *		   mapping is controlled by the MAP_VPATHn fields of this
 *		   register.
 *		0x1 - Even parity over the frame's MAC destination address
 *		0x2 - Even parity over the frame's MAC source address
 *		0x3 - Even parity over the frame's MAC destination address and
 *		  MAC source address
 *		Note: If the host changes this mapping while traffic is flowing,
 *		then (to avoid mis-ordering at the receiver) host must either
 *		enable the Marker protocol or assume responsibility for ensuring
 *		that no frames pertaining to the conversations (that are moving
 *		to a new port) are in flight.
 * @distrib_dest: When LAG_TX_CFG.DISTRIB_ALG_SEL is set to use the source
 *              VPATH, then this field indicates the target adapter port for
 *              frames that come from a particular VPATH.
 *              0 - Send frames from this VPATH to port 0
 *              1 - Send frames from this VPATH to port 1
 *              Note: If the host updates this mapping while traffic is flowing,
 *              then (to avoid mis-ordering at the receiver) the host must
 *              either enable the Marker protocol or assume responsibility for
 *              ensuring that no frames pertaining to the conversations (that
 *              are moving to a new port) are in flight.
 * @distrib_remap_if_fail: When lag_mode is Link Aggregated, this field controls
 *		whether frames are re-distributed to the working port if one
 *		port goes down.
 *		0 - Don't remap. Enforce frames destined for port 'x' to remain
 *		destined for it and let LAG_CFG.TX_DISCARD_BEHAV determine
 *		what happens to the frames.
 *		1 - Remap the frames to the working port, essentially ignoring
 *		the mapping table.
 * @coll_max_delay: Collector Max Delay - the maximum amount of time (measured
 *		in units of tens of microseconds) that the Frame Collector is
 *		allowed to delay delivery of frames to the host. The contents
 *		of this field are placed into the transmitted LACPDU.
 * @rx_discard: When the state of the port state attached to the Rx Aggregator
 *		is not Collecting, this field determines whether frames to the
 *		Frame Collector are discarded by the Aggregator Parser
 *
 * Link Aggregation Link Aggregator Mode Configuration
 */
typedef struct vxge_hal_lag_la_config_t {
	u32	tx_discard;
#define	VXGE_HAL_LAG_TX_DISCARD_DISBALE				0
#define	VXGE_HAL_LAG_TX_DISCARD_ENABLE				1
#define	VXGE_HAL_LAG_TX_DISCARD_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	distrib_alg_sel;
#define	VXGE_HAL_LAG_DISTRIB_ALG_SEL_SRC_VPATH			0
#define	VXGE_HAL_LAG_DISTRIB_ALG_SEL_DEST_MAC_ADDR		1
#define	VXGE_HAL_LAG_DISTRIB_ALG_SEL_SRC_MAC_ADDR		2
#define	VXGE_HAL_LAG_DISTRIB_ALG_SEL_BOTH_MAC_ADDR		3
#define	VXGE_HAL_LAG_DISTRIB_ALG_SEL_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u64	distrib_dest;
#define	VXGE_HAL_LAG_DISTRIB_DEST_VPATH_TO_PORT_PORT0(vpid)	0
#define	VXGE_HAL_LAG_DISTRIB_DEST_VPATH_TO_PORT_PORT1(vpid)	mBIT(vpid)
#define	VXGE_HAL_LAG_DISTRIB_DEST_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	distrib_remap_if_fail;
#define	VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_DISBALE		0
#define	VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_ENABLE		1
#define	VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

	u32	coll_max_delay;
#define	VXGE_HAL_LAG_MIN_COLL_MAX_DELAY				0
#define	VXGE_HAL_LAG_MAX_COLL_MAX_DELAY				65535
#define	VXGE_HAL_LAG_DEF_COLL_MAX_DELAY		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	rx_discard;
#define	VXGE_HAL_LAG_RX_DISCARD_DISBALE				0
#define	VXGE_HAL_LAG_RX_DISCARD_ENABLE				1
#define	VXGE_HAL_LAG_RX_DISCARD_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT


} vxge_hal_lag_la_config_t;

/*
 * struct vxge_hal_lag_ap_config_t - LAG Active Passive Failover mode
 *			    configuration(For privileged mode driver only)
 *
 * @hot_standby: Keep the standby port alive even when it is not carrying
 *		traffic
 *		0 - Standby port disabled until needed. The hardware behaves as
 *		if XGMAC_CFG_PORTn.PORT_EN has disabled the port.
 *		1 - Standby port kept up
 * @lacp_decides: This field determines whether or not the LACP Selection logic
 *		handles hot standby port interaction. This field is only used
 *		when hot_standby is 1.
 *		0 - LACP Selection logic does not explicitly determine standby
 *		port, instead internal logic changes the aggregator's key
 *		using information found in the alt_admin_key
 *		field. Note that this does not disable LACP.
 *		1 - LACP Selection logic explicitly determines standby port by
 *		enforcing a rule that if one port is already attached to any
 *		aggregator, then the other port is put into STANDBY. Assuming
 *		both ports have the same Key, at startup (or anytime both
 *		ports have become UNSELECTED) the Selection logic uses
 *		pref_active_port to choose the active (and consequently
 *		standby) port. After that it only selects a new port when
 *		the active port goes down.
 * @pref_active_port: Indicates the preferred active port number.
 *		If hot_standby is disabled (i.e. "cold standby"), then
 *		pref_active_port determines which port remains powered up
 *		(and consequently which one is powered down). If hot_standby is
 *		enabled, then pref_active_port is used by the Selection logic
 *		whenever both ports have become UNSELECTED and the Selection
 *		logic must decide which to make SELECTED and which to make
 *		STANDBY.
 *		0 - Link0 is preferred (Link1 becomes the standby port).
 *		1 - Link1 is preferred
 * @auto_failback: When LACP Selection logic is not handling standby port
 *		interaction, this register provides additional user flexibility
 *		for standby port handling. The AUTO_FAILBACK field controls
 *		whether the device automatically fails back to the preferred
 *		(i.e. non-alternate) Aggregator+Port pair in the event that the
 *		preferred port comes back up after a failure. Only used when
 *		hot_standby is set to 1 and lacp_decides is set to 0.
 *		0 - After a failure on the preferred port, stay on alternate
 *		port even if the preferred port comes back up. Return to
 *		preferred port only when host indicates to return
 *		(via FAILBACK_EN)
 *		1 - After a failure on the preferred port,automatically failback
 *		to preferred port whenever it comes back up.
 * @failback_en: This field is used when hot_standby is set to 1,lacp_decides is
 *		set to 0, and AUTO_FAILBACK is set to 0. The field is also used
 *		when hot_standby is set to 0. The failback_en field allow the
 *		host to control when the adapter is allowed to fail back to the
 *		preferred port. The driver sets this field to indicate to the
 *		adapter that it okay to fail back to the preferred port (i.e.
 *		attempt to acquire a good port on the preferred port). This
 *		field is self-clearing -- the adapter clears it immediately.
 *		Note that the host can use waiting_to_fallback to tell if the
 *		adapter is waiting for host intervention.
 *		0 - Adapter has acknowledged the request to fail back.
 *		1 - Host requests that the adapter fail back to preferred port.
 * @cold_failover_timeout: When cold standby mode is entered, this field
 *		controls how long (in msec) the adapter waits for the preferred
 *		port to come alive (assuming it isn't alreay alive. It the
 *		preferred port does not come up, then the adapter fails over
 *		to the standby port when the timer expires. At the time of
 *		standby port initialization, the timer is started again and
 *		if the standby port does not come up after the timer expires,
 *		then both ports are shut down.
 * @alt_admin_key: The Aggregator's administrative Key whenever the device is in
 *		active-passive failover mode and both ports are up. This
 *		prevents both ports from becoming active in this case.
 *		The H/W is responsible for choosing the proper key to use in
 *		this case. The null (all zeros) Key value is not available for
 *		local use.
 * @alt_aggr: Identifies which Aggregator is designated as the alternate
 *		(i.e. unused) Aggregator, when both ports are up.
 *		0 - Aggregator0 is the alternate
 *		1 - Aggregator1 is the alternate
 *
 * Link Aggregation Active Passive failover mode Configuration
 */
typedef struct vxge_hal_lag_ap_config_t {
	u32	hot_standby;
#define	VXGE_HAL_LAG_HOT_STANDBY_DISBALE_PORT			0
#define	VXGE_HAL_LAG_HOT_STANDBY_KEEP_UP_PORT			1
#define	VXGE_HAL_LAG_HOT_STANDBY_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	lacp_decides;
#define	VXGE_HAL_LAG_LACP_DECIDES_DISBALE			0
#define	VXGE_HAL_LAG_LACP_DECIDES_ENBALE			1
#define	VXGE_HAL_LAG_LACP_DECIDES_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	pref_active_port;
#define	VXGE_HAL_LAG_PREF_ACTIVE_PORT_0				0
#define	VXGE_HAL_LAG_PREF_ACTIVE_PORT_1				1
#define	VXGE_HAL_LAG_PREF_ACTIVE_PORT_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	auto_failback;
#define	VXGE_HAL_LAG_AUTO_FAILBACK_DISBALE			0
#define	VXGE_HAL_LAG_AUTO_FAILBACK_ENBALE			1
#define	VXGE_HAL_LAG_AUTO_FAILBACK_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	failback_en;
#define	VXGE_HAL_LAG_FAILBACK_EN_DISBALE			0
#define	VXGE_HAL_LAG_FAILBACK_EN_ENBALE				1
#define	VXGE_HAL_LAG_FAILBACK_EN_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	cold_failover_timeout;
#define	VXGE_HAL_LAG_MIN_COLD_FAILOVER_TIMEOUT			0
#define	VXGE_HAL_LAG_MAX_COLD_FAILOVER_TIMEOUT			65535
#define	VXGE_HAL_LAG_DEF_COLD_FAILOVER_TIMEOUT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	alt_admin_key;
#define	VXGE_HAL_LAG_MIN_ALT_ADMIN_KEY				0
#define	VXGE_HAL_LAG_MAX_ALT_ADMIN_KEY				65535
#define	VXGE_HAL_LAG_DEF_ALT_ADMIN_KEY		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	alt_aggr;
#define	VXGE_HAL_LAG_ALT_AGGR_0					0
#define	VXGE_HAL_LAG_ALT_AGGR_1					1
#define	VXGE_HAL_LAG_ALT_AGGR_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_lag_ap_config_t;

/*
 * struct vxge_hal_lag_sl_config_t - LAG Single Link configuration(For
 *		privileged mode driver only)
 *
 * @pref_indiv_port: For Single Link mode, this field indicates the preferred
 *		active port number. It is used by the Selection logic whenever
 *		both ports have become UNSELECTED and the Selection logic must
 *		decide which to make SELECTED and which to keep UNSELECTED.
 *		This field is only valid when the MODE field is set to
 *		'Single Link'.
 *
 * Link Aggregation Single Link Configuration
 */
typedef struct vxge_hal_lag_sl_config_t {
	u32	pref_indiv_port;
#define	VXGE_HAL_LAG_PREF_INDIV_PORT_0				0
#define	VXGE_HAL_LAG_PREF_INDIV_PORT_1				1
#define	VXGE_HAL_LAG_PREF_INDIV_PORT_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT
} vxge_hal_lag_sl_config_t;

/*
 * struct vxge_hal_lag_lacp_config_t - LAG LACP configuration(For privileged
 *				  mode driver only)
 *
 * @lacp_en: Enables use of the on-chip LACP implementation.
 * @lacp_begin: Re-initializes the LACP protocol state machines.
 * @discard_lacp: If LACP is not enabled on the device, then all LACP frames
 *		are passed to the host. However, when LACP is enabled,this field
 *		determines whether the LACP frames are still passed to the host.
 * @liberal_len_chk: Controls the length checks that are performed on the
 *		received LACPDU by the RX FSM. Normally, the received value of
 *		the following length fields is a known constant and(as suggested
 *		by IEEE 802.3-2005 43.4.12) the hardware validates them:
 *		Actor_Information_Length, Partner_Information_Length,
 *		Collector_Information_Length, and Terminator_Information_Length.
 * @marker_gen_recv_en: Enables marker generator/receiver. If this functionality
 *		is disabled, then the host must assume responsibility for
 *		ensuring that no frames pertaining to the conversations (that
 *		are moving to a new port) are in flight, whenever the transmit
 *		distribution algorithm is updated.
 * @marker_resp_en: Enables the transmission of Marker Response PDUs. Adapter
 *		sends a Marker Response PDU in response to a received Marker PDU
 * @marker_resp_timeout: Timeout value for response to Marker frame - number
 *		of milliseconds that the frame distribution logic will wait
 *		before assuming that all frames transmitted on a particular
 *		conversation have been successfully received. If a Marker
 *		Response PDU comes back before the timer expires, then the
 *		same assumption is made.
 * @slow_proto_mrkr_min_interval: Minimum interval (in milliseconds) between
 *		Marker PDU transmissions. Includes both Marker PDUs and Marker
 *		Response PDUs. According to IEEE 802.3-2005 Annex 43B.2, the
 *		device should send no more than 10 frames in any one-second
 *		period. Thus, waiting 100ms between successive transmission
 *		of Slow Protocol frames for the Marker Protocol (i.e. those
 *		that are sourced by our Marker Generator), guarantee that we
 *		satisfy this requirement. To be overly conservative the default
 *		value of this register allows for 200ms between frames.
 * @throttle_mrkr_resp: Permits the adapter to throttle the tranmission of
 *		Marker Response PDUs to satisfy the Slow Protocols transmission
 *		rate (see IEEE 802.3-2005 Annex 43B).
 *		0 - Transmission of Marker Response PDUs is not moderated.
 *		A Marker Response PDU is sent in response to every received
 *		Marker frame, regardless of whether the Marker frames are
 *		being received at a rate below the Slow Protocols rate.
 *		1 - Limit the transmission of Marker Response PDUs to the Slow
 *		Protocols transmission rate. If a remote host is generating
 *		Marker frames too quickly, then some of these frames will
 *		have no corresponding Marker Response PDU generated by the
 *		adapter.
 *
 * Link Aggregation LACP Configuration
 */
typedef struct vxge_hal_lag_lacp_config_t {
	u32	lacp_en;
#define	VXGE_HAL_LAG_LACP_EN_DISBALE				0
#define	VXGE_HAL_LAG_LACP_EN_ENABLE				1
#define	VXGE_HAL_LAG_LACP_EN_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	lacp_begin;
#define	VXGE_HAL_LAG_LACP_BEGIN_NORMAL				0
#define	VXGE_HAL_LAG_LACP_BEGIN_RESET				1
#define	VXGE_HAL_LAG_LACP_BEGIN_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	discard_lacp;
#define	VXGE_HAL_LAG_DISCARD_LACP_DISBALE			0
#define	VXGE_HAL_LAG_DISCARD_LACP_ENABLE			1
#define	VXGE_HAL_LAG_DISCARD_LACP_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	liberal_len_chk;
#define	VXGE_HAL_LAG_LIBERAL_LEN_CHK_DISBALE			0
#define	VXGE_HAL_LAG_LIBERAL_LEN_CHK_ENABLE			1
#define	VXGE_HAL_LAG_LIBERAL_LEN_CHK_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	marker_gen_recv_en;
#define	VXGE_HAL_LAG_MARKER_GEN_RECV_EN_DISBALE			0
#define	VXGE_HAL_LAG_MARKER_GEN_RECV_EN_ENABLE			1
#define	VXGE_HAL_LAG_MARKER_GEN_RECV_EN_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	marker_resp_en;
#define	VXGE_HAL_LAG_MARKER_RESP_EN_DISBALE			0
#define	VXGE_HAL_LAG_MARKER_RESP_EN_ENABLE			1
#define	VXGE_HAL_LAG_MARKER_RESP_EN_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	marker_resp_timeout;
#define	VXGE_HAL_LAG_MIN_MARKER_RESP_TIMEOUT			0
#define	VXGE_HAL_LAG_MAX_MARKER_RESP_TIMEOUT			65535
#define	VXGE_HAL_LAG_DEF_MARKER_RESP_TIMEOUT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	slow_proto_mrkr_min_interval;
#define	VXGE_HAL_LAG_MIN_SLOW_PROTO_MRKR_MIN_INTERVAL		0
#define	VXGE_HAL_LAG_MAX_SLOW_PROTO_MRKR_MIN_INTERVAL		65535
#define	VXGE_HAL_LAG_DEF_SLOW_PROTO_MRKR_MIN_INTERVAL		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32	throttle_mrkr_resp;
#define	VXGE_HAL_LAG_THROTTLE_MRKR_RESP_DISBALE			0
#define	VXGE_HAL_LAG_THROTTLE_MRKR_RESP_ENABLE			1
#define	VXGE_HAL_LAG_THROTTLE_MRKR_RESP_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_lag_lacp_config_t;

/*
 * struct vxge_hal_lag_config_t - LAG configuration(For privileged
 *				  mode driver only)
 *
 * @lag_en: Enables link aggregation
 * @lag_mode: Select the mode of operation for link aggregation. The options:
 *		00 - Link Aggregated
 *		01 - Active/Passive Failover
 *		10 - Single Link
 * @la_mode_config: LAG Link Aggregator mode config
 * @ap_mode_config: LAG Active Passive Failover mode config
 * @sl_mode_config: LAG Single Link mode config
 * @lacp_config: LAG LACP config
 * @incr_tx_aggr_stats: Controls whether Tx aggregator stats are incremented
 *		when Link Aggregation is disabled.
 *		0 - Don't increment
 *		1 - Increment
 *		Note: When LAG is enabled, aggregator stats are always
 *		incremented.
 * @port_config: Lag Port configuration. See vxge_hal_lag_port_config_t {}
 * @aggr_config: Lag Aggregator configuration. See vxge_hal_lag_aggr_config_t {}
 * @sys_pri: The System Priority of the System. Numerically lower values have
 *		higher priority.
 * @mac_addr: The MAC address assigned to the System. Should be non-zero.
 * @use_port_mac_addr: Indicates whether the Aggregator should use:
 *		0 - the address specified in this register.
 *		1 - the station address of one of the ports in the System
 * @mac_addr_sel: Indicates which port address to use, if USE_PORT_ADDR is set:
 *		0 - the station address of port 0
 *		1 - the station address of port 1.
 * @fast_per_time: Fast Periodic Time - number of seconds between periodic
 *		transmissions of Short Timeouts.
 * @slow_per_time: Slow Periodic Time - number of seconds between periodic
 *		transmissions of Long Timeouts.
 * @short_timeout: Short Timeout Time - number of seconds before
 *		invalidating received LACPDU information using Short
 *		Timeouts (3 x Fast Periodic Time).
 * @long_timeout: Long Timeout Time - number of seconds before invalidating
 *		received LACPDU information using Long Timeouts
 *		(3 x Slow Periodic Time).
 * @churn_det_time: Churn Detection Time - number of seconds that the
 *		Actor and Partner Churn state machines wait for the Actor
 *		or Partner Sync state to stabilize.
 * @aggr_wait_time: Aggregate Wait Time - number of seconds to delay
 *		aggregation,to allow multiple links to aggregate simultaneously
 * @short_timer_scale: For simulation purposes, this field allows scaling of
 *		link aggregation timers. Specifically, the included timers are
 *		short (programmed with units of msec) and include 'Emptied Link
 *		Timer', 'Slow Proto Timer for Marker PDU', 'Slow Proto Timer for
 *		Marker Response PDU', and 'Cold Failover Timer'.
 *		0x0 - No scaling
 *		0x1 - Scale by 10X (counter expires 10 times faster)
 *		0x2 - Scale by 100X
 *		0x3 - Scale by 1000X
 * @long_timer_scale: For simulation purposes, this field allows scaling of link
 *		aggregation timers. Specifically, the included timers are long
 *		(programmed with units of seconds) and include 'Current While
 *		Timer', 'Periodic Timer', 'Wait While Timer', 'Transmit LACP
 *		Timer', 'Actor Churn Timer', 'Partner Churn Timer',
 *		0x0 - No scaling
 *		0x1 - Scale by 10X (counter expires 10 times faster)
 *		0x2 - Scale by 100X
 *		0x3 - Scale by 1000X
 *		0x4 - Scale by 10000X
 *		0x5 - Scale by 100000X
 *		0x6 - Scale by 1000000X
 *
 * Link Aggregation Configuration
 */
typedef struct vxge_hal_lag_config_t {
	u32	lag_en;
#define	VXGE_HAL_LAG_LAG_EN_DISABLE				0
#define	VXGE_HAL_LAG_LAG_EN_ENABLE				1
#define	VXGE_HAL_LAG_LAG_EN_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	lag_mode;
#define	VXGE_HAL_LAG_LAG_MODE_LAG				0
#define	VXGE_HAL_LAG_LAG_MODE_ACTIVE_PASSIVE_FAILOVER		1
#define	VXGE_HAL_LAG_LAG_MODE_SINGLE_LINK			2
#define	VXGE_HAL_LAG_LAG_MODE_DEFAULT		    VXGE_HAL_USE_FLASH_DEFAULT

	vxge_hal_lag_la_config_t	la_mode_config;
	vxge_hal_lag_ap_config_t	ap_mode_config;
	vxge_hal_lag_sl_config_t	sl_mode_config;
	vxge_hal_lag_lacp_config_t	lacp_config;

	u32	incr_tx_aggr_stats;
#define	VXGE_HAL_LAG_INCR_TX_AGGR_STATS_DISBALE			0
#define	VXGE_HAL_LAG_INCR_TX_AGGR_STATS_ENABLE			1
#define	VXGE_HAL_LAG_INCR_TX_AGGR_STATS_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	vxge_hal_lag_port_config_t port_config[VXGE_HAL_LAG_PORT_MAX_PORTS];
	vxge_hal_lag_aggr_config_t aggr_config[VXGE_HAL_LAG_AGGR_MAX_PORTS];

	u32	sys_pri;
#define	VXGE_HAL_LAG_MIN_SYS_PRI				0
#define	VXGE_HAL_LAG_MAX_SYS_PRI				65535
#define	VXGE_HAL_LAG_DEF_SYS_PRI		    VXGE_HAL_USE_FLASH_DEFAULT

	macaddr_t mac_addr;

	u32	use_port_mac_addr;
#define	VXGE_HAL_LAG_USE_PORT_MAC_ADDR_DISBALE			0
#define	VXGE_HAL_LAG_USE_PORT_MAC_ADDR_ENABLE			1
#define	VXGE_HAL_LAG_USE_PORT_MAC_ADDR_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	mac_addr_sel;
#define	VXGE_HAL_LAG_MAC_ADDR_SEL_PORT_0			0
#define	VXGE_HAL_LAG_MAC_ADDR_SEL_PORT_1			1
#define	VXGE_HAL_LAG_MAC_ADDR_SEL_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	fast_per_time;
#define	VXGE_HAL_LAG_MIN_FAST_PER_TIME				0
#define	VXGE_HAL_LAG_MAX_FAST_PER_TIME				65535
#define	VXGE_HAL_LAG_DEF_FAST_PER_TIME		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	slow_per_time;
#define	VXGE_HAL_LAG_MIN_SLOW_PER_TIME				0
#define	VXGE_HAL_LAG_MAX_SLOW_PER_TIME				65535
#define	VXGE_HAL_LAG_DEF_SLOW_PER_TIME		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	short_timeout;
#define	VXGE_HAL_LAG_MIN_SHORT_TIMEOUT				0
#define	VXGE_HAL_LAG_MAX_SHORT_TIMEOUT				65535
#define	VXGE_HAL_LAG_DEF_SHORT_TIMEOUT		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	long_timeout;
#define	VXGE_HAL_LAG_MIN_LONG_TIMEOUT				0
#define	VXGE_HAL_LAG_MAX_LONG_TIMEOUT				65535
#define	VXGE_HAL_LAG_DEF_LONG_TIMEOUT		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	churn_det_time;
#define	VXGE_HAL_LAG_MIN_CHURN_DET_TIME				0
#define	VXGE_HAL_LAG_MAX_CHURN_DET_TIME				65535
#define	VXGE_HAL_LAG_DEF_CHURN_DET_TIME		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	aggr_wait_time;
#define	VXGE_HAL_LAG_MIN_AGGR_WAIT_TIME				0
#define	VXGE_HAL_LAG_MAX_AGGR_WAIT_TIME				65535
#define	VXGE_HAL_LAG_DEF_AGGR_WAIT_TIME		    VXGE_HAL_USE_FLASH_DEFAULT

	u32	short_timer_scale;
#define	VXGE_HAL_LAG_SHORT_TIMER_SCALE_1X			0
#define	VXGE_HAL_LAG_SHORT_TIMER_SCALE_10X			1
#define	VXGE_HAL_LAG_SHORT_TIMER_SCALE_100X			2
#define	VXGE_HAL_LAG_SHORT_TIMER_SCALE_1000X			3
#define	VXGE_HAL_LAG_SHORT_TIMER_SCALE_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32	long_timer_scale;
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_1X			0
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_10X			1
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_100X			2
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_1000X			3
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_10000X			4
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_100000X			5
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_1000000X			6
#define	VXGE_HAL_LAG_LONG_TIMER_SCALE_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_lag_config_t;

/*
 * struct vxge_hal_vpath_qos_config_t - Vpath qos(For privileged
 *				  mode driver only)
 * @priority: The priority of vpath
 * @min_bandwidth: Minimum Guaranteed bandwidth
 * @max_bandwidth: Maximum allowed bandwidth
 *
 * This structure is vpath qos configuration for MRPCIM section of device
 */
typedef struct vxge_hal_vpath_qos_config_t {
	u32				priority;
#define	VXGE_HAL_VPATH_QOS_PRIORITY_MIN				0
#define	VXGE_HAL_VPATH_QOS_PRIORITY_MAX				16
#define	VXGE_HAL_VPATH_QOS_PRIORITY_DEFAULT			0

	u32				min_bandwidth;
#define	VXGE_HAL_VPATH_QOS_MIN_BANDWIDTH_MIN			0
#define	VXGE_HAL_VPATH_QOS_MIN_BANDWIDTH_MAX			100
#define	VXGE_HAL_VPATH_QOS_MIN_BANDWIDTH_DEFAULT		0

	u32				max_bandwidth;
#define	VXGE_HAL_VPATH_QOS_MAX_BANDWIDTH_MIN			0
#define	VXGE_HAL_VPATH_QOS_MAX_BANDWIDTH_MAX			100
#define	VXGE_HAL_VPATH_QOS_MAX_BANDWIDTH_DEFAULT		0

} vxge_hal_vpath_qos_config_t;

/*
 * struct vxge_hal_mrpcim_config_t - MRPCIM secion configuration(For privileged
 *				  mode driver only)
 *
 * @mac_config: MAC Port Config. See vxge_hal_mac_config_t {}
 * @lag_config: MAC Port Config. See vxge_hal_lag_config_t {}
 * @vp_qos: Vpath QOS
 * @vpath_to_wire_port_map_en: Mask to enable vpath to wire port mapping.
 * @vpath_to_wire_port_map: If LAG is not enabled or lag_distrib_dest is not set
 *		then vpath_to_wire_port_map is used to assign independent ports
 *		to vpath
 *
 * This structure is configuration for MRPCIM section of device
 */
typedef struct vxge_hal_mrpcim_config_t {
	vxge_hal_mac_config_t mac_config;
	vxge_hal_lag_config_t lag_config;
	u64	vpath_to_wire_port_map_en;
#define	VXGE_HAL_VPATH_TO_WIRE_PORT_MAP_EN_DISABLE(vpid)	0
#define	VXGE_HAL_VPATH_TO_WIRE_PORT_MAP_EN_ENABLE(vpid)		mBIT(vpid)
#define	VXGE_HAL_VPATH_WIRE_PORTS_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT
	u64	vpath_to_wire_port_map;
#define	VXGE_HAL_VPATH_TO_WIRE_PORT_MAP_PORT0(vpid)		0
#define	VXGE_HAL_VPATH_TO_WIRE_PORT_MAP_PORT1(vpid)		mBIT(vpid)
#define	VXGE_HAL_VPATH_TO_WIRE_PORT_MAP_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT
	vxge_hal_vpath_qos_config_t vp_qos[VXGE_HAL_MAX_VIRTUAL_PATHS];
} vxge_hal_mrpcim_config_t;

/*
 * struct vxge_hal_tim_intr_config_t - X3100 Tim interrupt configuration.
 * @intr_enable: Set to 1, if interrupt is enabled.
 * @btimer_val: Boundary Timer Initialization value in units of 272 ns.
 * @timer_ac_en: Timer Automatic Cancel. 1 : Automatic Canceling Enable: when
 *		asserted, other interrupt-generating entities will cancel the
 *		scheduled timer interrupt.
 * @timer_ci_en: Timer Continuous Interrupt. 1 : Continuous Interrupting Enable:
 *		When asserted, an interrupt will be generated every time the
 *		boundary timer expires, even if no traffic has been transmitted
 *		on this interrupt.
 * @timer_ri_en: Timer Consecutive (Re-) Interrupt 1 : Consecutive
 *		(Re-) Interrupt Enable: When asserted, an interrupt will be
 *		generated the next time the timer expires,even if no traffic has
 *		been transmitted on this interrupt. (This will only happen once
 *		each time that this value is written to the TIM.) This bit is
 *		cleared by H/W at the end of the current-timer-interval when
 *		the interrupt is triggered.
 * @rtimer_event_sf: Restriction Timer Event Scale Factor. A scale factor that
 *		is to be applied to the current event count before it is added
 *		to the restriction timer value when the the restriction timer
 *		is started.
 *		The scale factor is applied as a right or left shift to multiply
 *		or divide by the event count. The programmable values are as
 *		follows:
 *		0-disable restriction timer and use the base timer value.
 *		1-Multiply the event count by 2, shift left by 1.
 *		2-Multiply the event count by 4, shift left by 2.
 *		3-Multiply the event count by 8, shift left by 3.
 *		4-Multiply the event count by 16, shift left by 4.
 *		5-Multiply the event count by 32, shift left by 5.
 *		6-Multiply the event count by 64, shift  left by 6.
 *		7-Multiply the event count by 128, shift left by 7.
 *		8-add the event count, no shifting.
 *		9-Divide the event count by 128, shift right by 7.
 *		10-Divide the event count by 64, shift right by 6.
 *		11-Divide the event count by 32, shift right by 5.
 *		12-Divide the event count by 16, shift right by 4.
 *		13-Divide the event count by 8, shift right by 3.
 *		14-Divide the event count by 4, shift right by 2.
 *		15-Divide the event count by 2, shift right by 1.
 * @rtimer_val: Restriction Timer Initialization value in units of 272 ns.
 * @util_sel: Utilization Selector. Selects which of the workload approximations
 *		to use (e.g. legacy Tx utilization, Tx/Rx utilization, host
 *		specified utilization etc.),selects one of the 17 host
 *		configured values.
 *		0-Virtual Path 0
 *		1-Virtual Path 1
 *		...
 *		16-Virtual Path 17
 *		17-Legacy Tx network utilization, provided by TPA
 *		18-Legacy Rx network utilization, provided by FAU
 *		19-Average of legacy Rx and Tx utilization calculated from link
 *		utilization values.
 *		20-31-Invalid configurations
 *		32-Host utilization for Virtual Path 0
 *		33-Host utilization for Virtual Path 1
 *		...
 *		48-Host utilization for Virtual Path 17
 *		49-Legacy Tx network utilization, provided by TPA
 *		50-Legacy Rx network utilization, provided by FAU
 *		51-Average of legacy Rx and Tx utilization calculated from
 *		link utilization values.
 *		52-63-Invalid configurations
 * @ltimer_val: Latency Timer Initialization Value in units of 272 ns.
 * @txfrm_cnt_en: Transmit Frame Event Count Enable. This configuration bit
 *		when set to 1 enables counting of transmit frame's(signalled by
 *		SM), towards utilization event count values.
 * @txd_cnt_en: TxD Return Event Count Enable. This configuration bit when set
 *		to 1 enables counting of TxD0 returns (signalled by PCC's),
 *		towards utilization event count values.
 * @urange_a: Defines the upper limit (in percent) for this utilization range
 *		to be active. This range is considered active
 *		 if 0 = UTIL = URNG_A and the UEC_A field (below) is non-zero.
 * @uec_a: Utilization Event Count A. If this range is active, the adapter will
 *		wait until UEC_A events have occurred on the interrupt before
 *		generating an interrupt.
 * @urange_b: Link utilization range B.
 * @uec_b: Utilization Event Count B.
 * @urange_c: Link utilization range C.
 * @uec_c: Utilization Event Count C.
 * @urange_d: Link utilization range D.
 * @uec_d: Utilization Event Count D.
 * @ufca_intr_thres
 * @ufca_lo_lim
 * @ufca_hi_lim
 * @ufca_lbolt_period:
 *
 * Traffic Interrupt Controller Module interrupt configuration.
 */
typedef struct vxge_hal_tim_intr_config_t {

	u32				intr_enable;
#define	VXGE_HAL_TIM_INTR_ENABLE				1
#define	VXGE_HAL_TIM_INTR_DISABLE				0
#define	VXGE_HAL_TIM_INTR_DEFAULT				0

	u32				btimer_val;
#define	VXGE_HAL_MIN_TIM_BTIMER_VAL				0
#define	VXGE_HAL_MAX_TIM_BTIMER_VAL				67108864
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_BTIMER_VAL    VXGE_HAL_USE_FLASH_DEFAULT

	u32				timer_ac_en;
#define	VXGE_HAL_TIM_TIMER_AC_ENABLE				1
#define	VXGE_HAL_TIM_TIMER_AC_DISABLE				0
#define	VXGE_HAL_TIM_TIMER_AC_USE_FLASH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				timer_ci_en;
#define	VXGE_HAL_TIM_TIMER_CI_ENABLE				1
#define	VXGE_HAL_TIM_TIMER_CI_DISABLE				0
#define	VXGE_HAL_TIM_TIMER_CI_USE_FLASH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				timer_ri_en;
#define	VXGE_HAL_TIM_TIMER_RI_ENABLE				1
#define	VXGE_HAL_TIM_TIMER_RI_DISABLE				0
#define	VXGE_HAL_TIM_TIMER_RI_USE_FLASH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rtimer_event_sf;
#define	VXGE_HAL_MIN_TIM_RTIMER_EVENT_SF			0
#define	VXGE_HAL_MAX_TIM_RTIMER_EVENT_SF			15
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_EVENT_SF		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rtimer_val;
#define	VXGE_HAL_MIN_TIM_RTIMER_VAL				0
#define	VXGE_HAL_MAX_TIM_RTIMER_VAL				67108864
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_VAL    VXGE_HAL_USE_FLASH_DEFAULT

	u32				util_sel;
#define	VXGE_HAL_TIM_UTIL_SEL_VPATH(n)				n
#define	VXGE_HAL_TIM_UTIL_SEL_LEGACY_TX_NET_UTIL		17
#define	VXGE_HAL_TIM_UTIL_SEL_LEGACY_RX_NET_UTIL		18
#define	VXGE_HAL_TIM_UTIL_SEL_LEGACY_TX_RX_AVE_NET_UTIL		19
#define	VXGE_HAL_TIM_UTIL_SEL_VPATH(n)				n
#define	VXGE_HAL_TIM_UTIL_SEL_VPATH(n)				n
#define	VXGE_HAL_TIM_UTIL_SEL_HOST_UTIL_VPATH(n)		(32+n)
#define	VXGE_HAL_TIM_UTIL_SEL_TIM_UTIL_VPATH(n)			63
#define	VXGE_HAL_TIM_UTIL_SEL_USE_FLASH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				ltimer_val;
#define	VXGE_HAL_MIN_TIM_LTIMER_VAL				0
#define	VXGE_HAL_MAX_TIM_LTIMER_VAL				67108864
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_LTIMER_VAL    VXGE_HAL_USE_FLASH_DEFAULT

	/* Line utilization interrupts */
	u32				txfrm_cnt_en;
#define	VXGE_HAL_TXFRM_CNT_EN_ENABLE				1
#define	VXGE_HAL_TXFRM_CNT_EN_DISABLE				0
#define	VXGE_HAL_TXFRM_CNT_EN_USE_FLASH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				txd_cnt_en;
#define	VXGE_HAL_TXD_CNT_EN_ENABLE				1
#define	VXGE_HAL_TXD_CNT_EN_DISABLE				0
#define	VXGE_HAL_TXD_CNT_EN_USE_FLASH_DEFAULT	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				urange_a;
#define	VXGE_HAL_MIN_TIM_URANGE_A				0
#define	VXGE_HAL_MAX_TIM_URANGE_A				100
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_A	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				uec_a;
#define	VXGE_HAL_MIN_TIM_UEC_A					0
#define	VXGE_HAL_MAX_TIM_UEC_A					65535
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_A	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				urange_b;
#define	VXGE_HAL_MIN_TIM_URANGE_B				0
#define	VXGE_HAL_MAX_TIM_URANGE_B				100
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_B	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				uec_b;
#define	VXGE_HAL_MIN_TIM_UEC_B					0
#define	VXGE_HAL_MAX_TIM_UEC_B					65535
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_B	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				urange_c;
#define	VXGE_HAL_MIN_TIM_URANGE_C				0
#define	VXGE_HAL_MAX_TIM_URANGE_C				100
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_C	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				uec_c;
#define	VXGE_HAL_MIN_TIM_UEC_C					0
#define	VXGE_HAL_MAX_TIM_UEC_C					65535
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_C	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				uec_d;
#define	VXGE_HAL_MIN_TIM_UEC_D					0
#define	VXGE_HAL_MAX_TIM_UEC_D					65535
#define	VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_D	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				ufca_intr_thres;
#define	VXGE_HAL_MIN_UFCA_INTR_THRES				1
#define	VXGE_HAL_MAX_UFCA_INTR_THRES				4096
#define	VXGE_HAL_USE_FLASH_DEFAULT_UFCA_INTR_THRES    VXGE_HAL_USE_FLASH_DEFAULT

	u32				ufca_lo_lim;
#define	VXGE_HAL_MIN_UFCA_LO_LIM				1
#define	VXGE_HAL_MAX_UFCA_LO_LIM				16
#define	VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LO_LIM	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				ufca_hi_lim;
#define	VXGE_HAL_MIN_UFCA_HI_LIM				1
#define	VXGE_HAL_MAX_UFCA_HI_LIM				256
#define	VXGE_HAL_USE_FLASH_DEFAULT_UFCA_HI_LIM	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				ufca_lbolt_period;
#define	VXGE_HAL_MIN_UFCA_LBOLT_PERIOD				1
#define	VXGE_HAL_MAX_UFCA_LBOLT_PERIOD				1024
#define	VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LBOLT_PERIOD		\
						    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_tim_intr_config_t;

/*
 * struct vxge_hal_fifo_config_t - Configuration of fifo.
 * @enable: Is this fifo to be commissioned
 * @fifo_length: Numbers of TxDLs (that is, lists of Tx descriptors)per queue.
 * @max_frags: Max number of Tx buffers per TxDL (that is, per single
 *		transmit operation).
 *		No more than 256 transmit buffers can be specified.
 * @alignment_size: per Tx fragment DMA-able memory used to align transmit data
 *		(e.g., to align on a cache line).
 * @max_aligned_frags: Number of fragments to be aligned out of
 *		maximum fragments (see @max_frags).
 * @intr: Boolean. Use 1 to generate interrupt for each completed TxDL.
 *		Use 0 otherwise.
 * @no_snoop_bits: If non-zero, specifies no-snoop PCI operation,
 *		which generally improves latency of the host bridge operation
 *		(see PCI specification). For valid values please refer
 *		to vxge_hal_fifo_config_t {} in the driver sources.
 * Configuration of all X3100 fifos.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the vxge_hal_fifo_config_t {} structure.
 */
typedef struct vxge_hal_fifo_config_t {
	u32				enable;
#define	VXGE_HAL_FIFO_ENABLE					1
#define	VXGE_HAL_FIFO_DISABLE					0
#define	VXGE_HAL_FIFO_DEFAULT					1

	u32				fifo_length;
#define	VXGE_HAL_MIN_FIFO_LENGTH				1
#define	VXGE_HAL_MAX_FIFO_LENGTH				12*1024
#define	VXGE_HAL_DEF_FIFO_LENGTH				512

	u32				max_frags;
#define	VXGE_HAL_MIN_FIFO_FRAGS					1
#define	VXGE_HAL_MAX_FIFO_FRAGS					256
#define	VXGE_HAL_DEF_FIFO_FRAGS					256

	u32				alignment_size;
#define	VXGE_HAL_MIN_FIFO_ALIGNMENT_SIZE			0
#define	VXGE_HAL_MAX_FIFO_ALIGNMENT_SIZE			65536
#define	VXGE_HAL_DEF_FIFO_ALIGNMENT_SIZE	    __vxge_os_cacheline_size

	u32				max_aligned_frags;
	/* range: (1, @max_frags) */

	u32				intr;
#define	VXGE_HAL_FIFO_QUEUE_INTR_ENABLE				1
#define	VXGE_HAL_FIFO_QUEUE_INTR_DISABLE			0
#define	VXGE_HAL_FIFO_QUEUE_INTR_DEFAULT			0

	u32				no_snoop_bits;
#define	VXGE_HAL_FIFO_NO_SNOOP_DISABLED				0
#define	VXGE_HAL_FIFO_NO_SNOOP_TXD				1
#define	VXGE_HAL_FIFO_NO_SNOOP_FRM				2
#define	VXGE_HAL_FIFO_NO_SNOOP_ALL				3
#define	VXGE_HAL_FIFO_NO_SNOOP_DEFAULT				0

} vxge_hal_fifo_config_t;

/*
 * struct vxge_hal_ring_config_t - Ring configurations.
 * @enable: Is this ring to be commissioned
 * @ring_length: Numbers of RxDs in the ring
 * @buffer_mode: Receive buffer mode (1, 2, 3, or 5); for details please refer
 *		to X3100 User Guide.
 * @scatter_mode: X3100 supports two receive scatter modes: A and B.
 *		For details please refer to X3100 User Guide.
 * @post_mode: The RxD post mode.
 * @max_frm_len: Maximum frame length that can be received on _that_ ring.
 *		Setting this field to VXGE_HAL_USE_FLASH_DEFAULT ensures that
 *		the ring will "accept"
 *		MTU-size frames (note that MTU can be changed at runtime).
 *		Any value other than (VXGE_HAL_USE_FLASH_DEFAULT) specifies a
 *		certain "hard" limit on the receive frame sizes. The field can
 *		be used to activate receive frame-length based steering.
 * @no_snoop_bits: If non-zero, specifies no-snoop PCI operation,
 *		which generally improves latency of the host bridge operation
 *		(see PCI specification). For valid values please refer
 *		to vxge_hal_ring_config_t {} in the driver sources.
 * @rx_timer_val: The number of 32ns periods that would be counted between two
 *		timer interrupts.
 * @greedy_return: If Set it forces the device to return absolutely all RxD
 *		that are consumed and still on board when a timer interrupt
 *		triggers. If Clear, then if the device has already returned
 *		RxD before current timer interrupt trigerred and after the
 *		previous timer interrupt triggered, then the device is not
 *		forced to returned the rest of the consumed RxD that it has
 *		on board which account for a byte count less than the one
 *		programmed into PRC_CFG6.RXD_CRXDT field
 * @rx_timer_ci: TBD
 * @backoff_interval_us: Time (in microseconds), after which X3100
 *		tries to download RxDs posted by the host.
 *		Note that the "backoff" does not happen if host posts receive
 *		descriptors in the timely fashion.
 * @indicate_max_pkts: Sets maximum number of received frames to be processed
 *		within single interrupt.
 * @sw_lro_sessions: Number of LRO Sessions
 * @sw_lro_sg_size: Size of LROable segment
 * @sw_lro_frm_len: Length of LROable frame
 *
 * Ring configuration.
 */
typedef struct vxge_hal_ring_config_t {
	u32				enable;
#define	VXGE_HAL_RING_ENABLE					1
#define	VXGE_HAL_RING_DISABLE					0
#define	VXGE_HAL_RING_DEFAULT					1

	u32				ring_length;
#define	VXGE_HAL_MIN_RING_LENGTH				1
#define	VXGE_HAL_MAX_RING_LENGTH				8096
#define	VXGE_HAL_DEF_RING_LENGTH				512

	u32				buffer_mode;
#define	VXGE_HAL_RING_RXD_BUFFER_MODE_1				1
#define	VXGE_HAL_RING_RXD_BUFFER_MODE_3				3
#define	VXGE_HAL_RING_RXD_BUFFER_MODE_5				5
#define	VXGE_HAL_RING_RXD_BUFFER_MODE_DEFAULT			1

	u32				scatter_mode;
#define	VXGE_HAL_RING_SCATTER_MODE_A				0
#define	VXGE_HAL_RING_SCATTER_MODE_B				1
#define	VXGE_HAL_RING_SCATTER_MODE_C				2
#define	VXGE_HAL_RING_SCATTER_MODE_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				post_mode;
#define	VXGE_HAL_RING_POST_MODE_LEGACY				0
#define	VXGE_HAL_RING_POST_MODE_DOORBELL			1
#define	VXGE_HAL_RING_POST_MODE_USE_FLASH_DEFAULT   VXGE_HAL_USE_FLASH_DEFAULT

	u32				max_frm_len;
#define	VXGE_HAL_MIN_RING_MAX_FRM_LEN			    VXGE_HAL_MIN_MTU
#define	VXGE_HAL_MAX_RING_MAX_FRM_LEN			    VXGE_HAL_MAX_MTU
#define	VXGE_HAL_MAX_RING_FRM_LEN_USE_MTU	    VXGE_HAL_USE_FLASH_DEFAULT

	u32				no_snoop_bits;
#define	VXGE_HAL_RING_NO_SNOOP_DISABLED				0
#define	VXGE_HAL_RING_NO_SNOOP_RXD				1
#define	VXGE_HAL_RING_NO_SNOOP_FRM				2
#define	VXGE_HAL_RING_NO_SNOOP_ALL				3
#define	VXGE_HAL_RING_NO_SNOOP_USE_FLASH_DEFAULT    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rx_timer_val;
#define	VXGE_HAL_RING_MIN_RX_TIMER_VAL				0
#define	VXGE_HAL_RING_MAX_RX_TIMER_VAL				536870912
#define	VXGE_HAL_RING_USE_FLASH_DEFAULT_RX_TIMER_VAL		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				greedy_return;
#define	VXGE_HAL_RING_GREEDY_RETURN_ENABLE			1
#define	VXGE_HAL_RING_GREEDY_RETURN_DISABLE			0
#define	VXGE_HAL_RING_GREEDY_RETURN_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rx_timer_ci;
#define	VXGE_HAL_RING_RX_TIMER_CI_ENABLE			1
#define	VXGE_HAL_RING_RX_TIMER_CI_DISABLE			0
#define	VXGE_HAL_RING_RX_TIMER_CI_USE_FLASH_DEFAULT VXGE_HAL_USE_FLASH_DEFAULT

	u32				backoff_interval_us;
#define	VXGE_HAL_MIN_BACKOFF_INTERVAL_US			1
#define	VXGE_HAL_MAX_BACKOFF_INTERVAL_US			125000
#define	VXGE_HAL_USE_FLASH_DEFAULT_BACKOFF_INTERVAL_US		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				indicate_max_pkts;
#define	VXGE_HAL_MIN_RING_INDICATE_MAX_PKTS			1
#define	VXGE_HAL_MAX_RING_INDICATE_MAX_PKTS			65536
#define	VXGE_HAL_DEF_RING_INDICATE_MAX_PKTS			65536


} vxge_hal_ring_config_t;


/*
 * struct vxge_hal_vp_config_t - Configuration of virtual path
 * @vp_id: Virtual Path Id
 * @wire_port: Wire port to be associated with the vpath
 * @bandwidth_limit: Desired bandwidth limit for this vpath.
 *		0 = Disable limit, 1 = 8192 kBps, 2 = 16384 kBps, ... ,
 *		>152588 = 1 GBps
 * @no_snoop: Enable or disable no snoop for vpath
 * @ring: See vxge_hal_ring_config_t {}.
 * @fifo: See vxge_hal_fifo_config_t {}.
 * @dmq: See vxge_hal_dmq_config_t {};
 * @umq: See vxge_hal_umq_config_t {};
 * @lro: See vxge_hal_lro_config_t {};
 * @tti: Configuration of interrupt associated with Transmit.
 *		see vxge_hal_tim_intr_config_t();
 * @rti: Configuration of interrupt associated with Receive.
 *		 see vxge_hal_tim_intr_config_t();
 * @mtu: mtu size used on this port.
 * @tpa_lsov2_en: LSOv2 Behaviour for IP ID roll-over
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
 * @tpa_ipv6_keep_searching: If unknown IPv6 header is found,
 *		0 - stop searching for TCP
 *		1 - keep searching for TCP
 * @tpa_l4_pshdr_present: If asserted true, indicates the host has provided a
 *		valid pseudo header for TCP or UDP running over IPv4 or IPv6
 * @tpa_support_mobile_ipv6_hdrs: This register is somewhat equivalent to
 *		asserting both Hercules register fields LSO_RT2_EN and
 *		LSO_IPV6_HAO_EN. Enable/disable support for Type 2 Routing
 *		Headers, and for Mobile-IPv6 Home Address Option (HAO),
 *		as defined by mobile-ipv6.
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
 * @rpa_ipv4_udp_incl_ph: Determines whether the pseudo-header is included in
 *		the calculation of the L4 checksum that is passed to the host.
 *		This field applies to UDP/IPv4 packets only. It only affects
 *		non-offload traffic(since UDP frames are not candidates for LRO)
 *		0 - Do not include the pseudo-header in L4 checksum calculation.
 *		This setting should be used if the adapter is incorrectly
 *		calculating the pseudo-header.
 *		1 - Include the pseudo-header in L4 checksum calculation
 * @rpa_ipv6_udp_incl_ph: Determines if the pseudo-header is included in the
 *		calculation of the L4 checksum that is passed to the host. This
 *		field applies to UDP/IPv6 packets only. It only affects
 *		non-offload traffic(since UDP frames are not candidates for LRO)
 *		0 - Do not include the pseudo-header in L4 checksum calculation.
 *		This setting should be used if the adapter is incorrectly
 *		calculating the pseudo-header.
 *		1 - Include the pseudo-header in L4 checksum calculation
 * @rpa_l4_incl_cf: Determines whether the checksum field (CF) of the received
 *		frame is included in the calculation of the L4 checksum that is
 *		passed to the host. This field affects both non-offload and LRO
 *		traffic. Note that the RPA always includes the checksum field in
 *		the "Checksum Ok" L4 checksum calculation -- i.e. the checksum
 *		that decides whether a frame is a candidate to be offloaded.
 *		0 - Do not include the checksum field in L4 checksum calculation
 *		1 - Include the checksum field in L4 checksum calculation
 * @rpa_strip_vlan_tag: Strip VLAN Tag enable/disable. Instructs the device to
 *		remove the VLAN tag from all received tagged frames that are not
 *		replicated at the internal L2 switch.
 *		0 - Do not strip the VLAN tag.
 *		1 - Strip the VLAN tag. Regardless of this setting,VLAN tags are
 *		always placed into the RxDMA descriptor.
 * @rpa_l4_comp_csum: Determines whether the calculated L4 checksum should be
 *		complemented before it is passed to the host This field affects
 *		both non-offload and LRO traffic.
 *		0 - Do not complement the calculated L4 checksum.
 *		1 - Complement the calculated L4 checksum
 * @rpa_l3_incl_cf: Determines whether the checksum field (CF) of the received
 *		frame is included in the calculation of the L3 checksum that is
 *		passed to the host. This field affects both non-offload and LRO
 *		traffic. Note that the RPA always includes the checksum field in
 *		the "Checksum Ok" L3 checksum calculation--i.e. the checksum
 *		that decides whether a frame is a candidate to be offloaded.
 *		0 - Do not include the checksum field in L3 checksum calculation
 *		1 - Include the checksum field in L3 checksum calculation
 * @rpa_l3_comp_csum: Determines whether the calculated L3 checksum should be
 *		complemented before it is passed to the host This field affects
 *		both non-offload and LRO traffic.
 *		0 - Do not complement the calculated L3 checksum.
 *		1 - Complement the calculated L3 checksum
 * @rpa_ucast_all_addr_en: Enables frames with any unicast address (as its
 *		destination address) to be passed to the host.
 * @rpa_mcast_all_addr_en: Enables frames with any multicast address (as its
 *		destination address) to be passed to the host.
 * @rpa_bcast_en: Enables frames with any broadicast address (as its
 *		destination address) to be passed to the host.
 * @rpa_all_vid_en: romiscuous mode, it overrides the value held in this field.
 *		0 - Disable;
 *		1 - Enable
 *		Note: RXMAC_GLOBAL_CFG.AUTHORIZE_VP_ALL_VID must be set to
 *		allow this.
 * @vp_queue_l2_flow: Allows per-VPATH receive queue from
 *		contributing to L2 flow control. Has precedence over
 *		RMAC_PAUSE_CFG_PORTn.LIMITER_EN.
 *		0 - Queue is not allowed to contribute to L2 flow control.
 *		1 - Queue is allowed to contribute to L2 flow control.
 *
 * This structure is used by the driver to pass the configuration parameters to
 * configure Virtual Path.
 */
typedef struct vxge_hal_vp_config_t {
	u32				vp_id;

	u32				wire_port;
#define	VXGE_HAL_VPATH_USE_DEFAULT_PORT		VXGE_HAL_FIFO_HOST_STEER_NORMAL
#define	VXGE_HAL_VPATH_USE_PORT0		VXGE_HAL_FIFO_HOST_STEER_PORT0
#define	VXGE_HAL_VPATH_USE_PORT1		VXGE_HAL_FIFO_HOST_STEER_PORT1
#define	VXGE_HAL_VPATH_USE_BOTH			VXGE_HAL_FIFO_HOST_STEER_BOTH

	u32				bandwidth;
#define	VXGE_HAL_VPATH_BW_LIMIT_MAX			10000
#define	VXGE_HAL_VPATH_BW_LIMIT_MIN			100
#define	VXGE_HAL_VPATH_BW_LIMIT_DEFAULT			0XFFFFFFFF
#define	VXGE_HAL_TX_BW_VPATH_LIMIT			8

	u32				priority;
#define	VXGE_HAL_VPATH_PRIORITY_MIN			0
#define	VXGE_HAL_VPATH_PRIORITY_MAX			3
#define	VXGE_HAL_VPATH_PRIORITY_DEFAULT			0XFFFFFFFF

	u32				no_snoop;
#define	VXGE_HAL_VPATH_NO_SNOOP_ENABLE			1
#define	VXGE_HAL_VPATH_NO_SNOOP_DISABLE			0
#define	VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT			\
						VXGE_HAL_USE_FLASH_DEFAULT

	vxge_hal_ring_config_t		ring;
	vxge_hal_fifo_config_t		fifo;

	vxge_hal_tim_intr_config_t	tti;
	vxge_hal_tim_intr_config_t	rti;

	u32				mtu;
#define	VXGE_HAL_VPATH_MIN_INITIAL_MTU			VXGE_HAL_MIN_MTU
#define	VXGE_HAL_VPATH_MAX_INITIAL_MTU			VXGE_HAL_MAX_MTU
#define	VXGE_HAL_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU			\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tpa_lsov2_en;
#define	VXGE_HAL_VPATH_TPA_LSOV2_EN_ENABLE				1
#define	VXGE_HAL_VPATH_TPA_LSOV2_EN_DISABLE				0
#define	VXGE_HAL_VPATH_TPA_LSOV2_EN_USE_FLASH_DEFAULT			\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tpa_ignore_frame_error;
#define	VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_ENABLE			1
#define	VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_DISABLE			0
#define	VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tpa_ipv6_keep_searching;
#define	VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_ENABLE			1
#define	VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_DISABLE			0
#define	VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_USE_FLASH_DEFAULT	\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tpa_l4_pshdr_present;
#define	VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_ENABLE			1
#define	VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_DISABLE			0
#define	VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				tpa_support_mobile_ipv6_hdrs;
#define	VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_ENABLE		1
#define	VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_DISABLE		0
#define	VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_USE_FLASH_DEFAULT	\
						    VXGE_HAL_USE_FLASH_DEFAULT
#define	VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_DEFAULT		\
		VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_USE_FLASH_DEFAULT

	u32				rpa_ipv4_tcp_incl_ph;
#define	VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_ENABLE			1
#define	VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_DISABLE			0
#define	VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_ipv6_tcp_incl_ph;
#define	VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_ENABLE			1
#define	VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_DISABLE			0
#define	VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_ipv4_udp_incl_ph;
#define	VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_ENABLE			1
#define	VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_DISABLE			0
#define	VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_ipv6_udp_incl_ph;
#define	VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_ENABLE			1
#define	VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_DISABLE			0
#define	VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_l4_incl_cf;
#define	VXGE_HAL_VPATH_RPA_L4_INCL_CF_ENABLE				1
#define	VXGE_HAL_VPATH_RPA_L4_INCL_CF_DISABLE				0
#define	VXGE_HAL_VPATH_RPA_L4_INCL_CF_USE_FLASH_DEFAULT			\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_strip_vlan_tag;
#define	VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_ENABLE			1
#define	VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_DISABLE			0
#define	VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_l4_comp_csum;
#define	VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_ENABLE				1
#define	VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_DISABLE				0
#define	VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_l3_incl_cf;
#define	VXGE_HAL_VPATH_RPA_L3_INCL_CF_ENABLE				1
#define	VXGE_HAL_VPATH_RPA_L3_INCL_CF_DISABLE				0
#define	VXGE_HAL_VPATH_RPA_L3_INCL_CF_USE_FLASH_DEFAULT			\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_l3_comp_csum;
#define	VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_ENABLE				1
#define	VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_DISABLE				0
#define	VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_ucast_all_addr_en;
#define	VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_ENABLE			1
#define	VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_DISABLE			0
#define	VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_mcast_all_addr_en;
#define	VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_ENABLE			1
#define	VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_DISABLE			0
#define	VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_USE_FLASH_DEFAULT		\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_bcast_en;
#define	VXGE_HAL_VPATH_RPA_BCAST_ENABLE					1
#define	VXGE_HAL_VPATH_RPA_BCAST_DISABLE				0
#define	VXGE_HAL_VPATH_RPA_BCAST_USE_FLASH_DEFAULT  VXGE_HAL_USE_FLASH_DEFAULT

	u32				rpa_all_vid_en;
#define	VXGE_HAL_VPATH_RPA_ALL_VID_ENABLE				1
#define	VXGE_HAL_VPATH_RPA_ALL_VID_DISABLE				0
#define	VXGE_HAL_VPATH_RPA_ALL_VID_USE_FLASH_DEFAULT			\
						    VXGE_HAL_USE_FLASH_DEFAULT

	u32				vp_queue_l2_flow;
#define	VXGE_HAL_VPATH_VP_Q_L2_FLOW_ENABLE				1
#define	VXGE_HAL_VPATH_VP_Q_L2_FLOW_DISABLE				0
#define	VXGE_HAL_VPATH_VP_Q_L2_FLOW_USE_FLASH_DEFAULT			\
						    VXGE_HAL_USE_FLASH_DEFAULT

} vxge_hal_vp_config_t;

/*
 * struct vxge_hal_device_config_t - Device configuration.
 * @dma_blockpool_min: Minimum blocks in the DMA pool
 * @dma_blockpool_initial: Initial size of DMA Pool
 * @dma_blockpool_incr: Number of blocks to request each time number of blocks
 *		in the pool reaches dma_pool_min
 * @dma_blockpool_max: Maximum blocks in DMA pool
 * @mrpcim_config: MRPCIM section config. Used only for the privileged mode ULD
 *		instance.
 * @isr_polling_cnt: Maximum number of times to "poll" for Tx and Rx
 *		completions. Used in vxge_hal_device_handle_irq().
 * @max_payload_size: Maximum TLP payload size for the device/fFunction.
 *		As a Receiver, the Function/device must handle TLPs as large
 *		as the set value; as . As a Transmitter, the Function/device
 *		must not generate TLPs exceeding the set value. Permissible
 *		values that can be programmed are indicated by the
 *		Max_Payload_Size Supported in the Device Capabilities register
 * @mmrb_count: Maximum Memory Read Byte Count. Use (VXGE_HAL_USE_FLASH_DEFAULT)
 *		to use default BIOS value.
 * @stats_refresh_time_sec: Sets the default interval for automatic stats
 *		transfer to the host. This includes MAC stats as well as
 *		PCI stats.
 * @intr_mode: Line, or MSI-X interrupt.
 *
 * @dump_on_unknown: Dump adapter state ("about", statistics, registers)
 *		on UNKNWON#.
 * @dump_on_serr: Dump adapter state ("about", statistics, registers) on SERR#.
 * @dump_on_critical: Dump adapter state ("about", statistics, registers)
 *		on CRITICAL#.
 * @dump_on_eccerr: Dump adapter state ("about", statistics, registers) on
 *		 ECC error.
 * @rth_en: Enable Receive Traffic Hashing(RTH) using IT(Indirection Table).
 * @rth_it_type: RTH IT table programming type
 * @rts_mac_en: Enable Receive Traffic Steering using MAC destination address
 * @rts_qos_en: TBD
 * @rts_port_en: TBD
 * @vp_config: Configuration for virtual paths
 * @max_cqe_groups:  The maximum number of adapter CQE group blocks a CQRQ
 *		can own at any one time.
 * @max_num_wqe_od_groups: The maximum number of WQE Headers/OD Groups that
 *		this S-RQ can own at any one time.
 * @no_wqe_threshold: Maximum number of times adapter polls WQE Hdr blocks for
 *		WQEs before generating a message or interrupt.
 * @refill_threshold_high:This field provides a hysteresis upper bound for
 *		automatic adapter refill operations.
 * @refill_threshold_low:This field provides a hysteresis lower bound for
 *		automatic adapter refill operations.
 * @eol_policy: This field sets the policy for handling the end of list
 *		condition.
 *		2'b00 - When EOL is reached, poll until last block wrapper
 *			size is no longer 0.
 *		2'b01 - Send UMQ message when EOL is reached.
 *		2'b1x - Poll until the poll_count_max is reached and
 *			if still EOL, send UMQ message
 * @eol_poll_count_max:sets the maximum number of times the queue manager will
 *		poll fora non-zero block wrapper before giving up and sending
 *		a UMQ message
 * @ack_blk_limit: Limit on the maximum number of ACK list blocks that can be
 *		held by a session at any one time.
 * @poll_or_doorbell: TBD
 * @stats_read_method: Stats read method.(DMA or PIO)
 * @device_poll_millis: Specify the interval (in mulliseconds) to wait for
 *		register reads
 * @debug_level: Debug logging level. see vxge_debug_level_e {}
 * @debug_mask: Module mask for debug logging level. for masks see vxge_debug.h
 * @lro_enable: SW LRO enable mask
 * @tracebuf_size: Size of the trace buffer. Set it to '0' to disable.
 *
 * X3100 configuration.
 * Contains per-device configuration parameters, including:
 * - latency timer (settable via PCI configuration space);
 * - maximum number of split transactions;
 * - maximum number of shared splits;
 * - stats sampling interval, etc.
 *
 * In addition, vxge_hal_device_config_t {} includes "subordinate"
 * configurations, including:
 * - fifos and rings;
 * - MAC (done at firmware level).
 *
 * See X3100 User Guide for more details.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the vxge_hal_device_config_t {} structure. Please refer to the
 * corresponding include file.
 * See also: vxge_hal_tim_intr_config_t {}.
 */
typedef struct vxge_hal_device_config_t {
	u32				dma_blockpool_min;
	u32				dma_blockpool_initial;
	u32				dma_blockpool_incr;
	u32				dma_blockpool_max;
#define	VXGE_HAL_MIN_DMA_BLOCK_POOL_SIZE		0
#define	VXGE_HAL_INITIAL_DMA_BLOCK_POOL_SIZE		0
#define	VXGE_HAL_INCR_DMA_BLOCK_POOL_SIZE		4
#define	VXGE_HAL_MAX_DMA_BLOCK_POOL_SIZE		4096

	vxge_hal_mrpcim_config_t	mrpcim_config;

	u32				isr_polling_cnt;
#define	VXGE_HAL_MIN_ISR_POLLING_CNT			0
#define	VXGE_HAL_MAX_ISR_POLLING_CNT			65536
#define	VXGE_HAL_DEF_ISR_POLLING_CNT			1

	u32				max_payload_size;
#define	VXGE_HAL_USE_BIOS_DEFAULT_PAYLOAD_SIZE	    VXGE_HAL_USE_FLASH_DEFAULT
#define	VXGE_HAL_MAX_PAYLOAD_SIZE_128			0
#define	VXGE_HAL_MAX_PAYLOAD_SIZE_256			1
#define	VXGE_HAL_MAX_PAYLOAD_SIZE_512			2
#define	VXGE_HAL_MAX_PAYLOAD_SIZE_1024			3
#define	VXGE_HAL_MAX_PAYLOAD_SIZE_2048			4
#define	VXGE_HAL_MAX_PAYLOAD_SIZE_4096			5

	u32				mmrb_count;
#define	VXGE_HAL_USE_BIOS_DEFAULT_MMRB_COUNT	    VXGE_HAL_USE_FLASH_DEFAULT
#define	VXGE_HAL_MMRB_COUNT_128				0
#define	VXGE_HAL_MMRB_COUNT_256				1
#define	VXGE_HAL_MMRB_COUNT_512				2
#define	VXGE_HAL_MMRB_COUNT_1024			3
#define	VXGE_HAL_MMRB_COUNT_2048			4
#define	VXGE_HAL_MMRB_COUNT_4096			5

	u32				stats_refresh_time_sec;
#define	VXGE_HAL_STATS_REFRESH_DISABLE			0
#define	VXGE_HAL_MIN_STATS_REFRESH_TIME			1
#define	VXGE_HAL_MAX_STATS_REFRESH_TIME			300
#define	VXGE_HAL_USE_FLASH_DEFAULT_STATS_REFRESH_TIME	30

	u32				intr_mode;
#define	VXGE_HAL_INTR_MODE_IRQLINE			0
#define	VXGE_HAL_INTR_MODE_MSIX				1
#define	VXGE_HAL_INTR_MODE_MSIX_ONE_SHOT		2
#define	VXGE_HAL_INTR_MODE_EMULATED_INTA		3
#define	VXGE_HAL_INTR_MODE_DEF				0

	u32				dump_on_unknown;
#define	VXGE_HAL_DUMP_ON_UNKNOWN_DISABLE		0
#define	VXGE_HAL_DUMP_ON_UNKNOWN_ENABLE			1
#define	VXGE_HAL_DUMP_ON_UNKNOWN_DEFAULT		0

	u32				dump_on_serr;
#define	VXGE_HAL_DUMP_ON_SERR_DISABLE			0
#define	VXGE_HAL_DUMP_ON_SERR_ENABLE			1
#define	VXGE_HAL_DUMP_ON_SERR_DEFAULT			0

	u32				dump_on_critical;
#define	VXGE_HAL_DUMP_ON_CRITICAL_DISABLE		0
#define	VXGE_HAL_DUMP_ON_CRITICAL_ENABLE		1
#define	VXGE_HAL_DUMP_ON_CRITICAL_DEFAULT		0

	u32				dump_on_eccerr;
#define	VXGE_HAL_DUMP_ON_ECCERR_DISABLE			0
#define	VXGE_HAL_DUMP_ON_ECCERR_ENABLE			1
#define	VXGE_HAL_DUMP_ON_ECCERR_DEFAULT			0

	u32				rth_en;
#define	VXGE_HAL_RTH_DISABLE				0
#define	VXGE_HAL_RTH_ENABLE				1
#define	VXGE_HAL_RTH_DEFAULT				0

	u32				rth_it_type;
#define	VXGE_HAL_RTH_IT_TYPE_SOLO_IT			0
#define	VXGE_HAL_RTH_IT_TYPE_MULTI_IT			1
#define	VXGE_HAL_RTH_IT_TYPE_DEFAULT			0

	u32				rts_mac_en;
#define	VXGE_HAL_RTS_MAC_DISABLE			0
#define	VXGE_HAL_RTS_MAC_ENABLE				1
#define	VXGE_HAL_RTS_MAC_DEFAULT			0

	u32				rts_qos_en;
#define	VXGE_HAL_RTS_QOS_DISABLE			0
#define	VXGE_HAL_RTS_QOS_ENABLE				1
#define	VXGE_HAL_RTS_QOS_DEFAULT			0

	u32				rts_port_en;
#define	VXGE_HAL_RTS_PORT_DISABLE			0
#define	VXGE_HAL_RTS_PORT_ENABLE			1
#define	VXGE_HAL_RTS_PORT_DEFAULT			0

	vxge_hal_vp_config_t		vp_config[VXGE_HAL_MAX_VIRTUAL_PATHS];

	u32				max_cqe_groups;
#define	VXGE_HAL_MIN_MAX_CQE_GROUPS			1
#define	VXGE_HAL_MAX_MAX_CQE_GROUPS			16
#define	VXGE_HAL_DEF_MAX_CQE_GROUPS			16

	u32				max_num_wqe_od_groups;
#define	VXGE_HAL_MIN_MAX_NUM_OD_GROUPS			1
#define	VXGE_HAL_MAX_MAX_NUM_OD_GROUPS			16
#define	VXGE_HAL_DEF_MAX_NUM_OD_GROUPS			16

	u32				no_wqe_threshold;
#define	VXGE_HAL_MIN_NO_WQE_THRESHOLD			1
#define	VXGE_HAL_MAX_NO_WQE_THRESHOLD			16
#define	VXGE_HAL_DEF_NO_WQE_THRESHOLD			16

	u32				refill_threshold_high;
#define	VXGE_HAL_MIN_REFILL_THRESHOLD_HIGH		1
#define	VXGE_HAL_MAX_REFILL_THRESHOLD_HIGH		16
#define	VXGE_HAL_DEF_REFILL_THRESHOLD_HIGH		16

	u32				refill_threshold_low;
#define	VXGE_HAL_MIN_REFILL_THRESHOLD_LOW		1
#define	VXGE_HAL_MAX_REFILL_THRESHOLD_LOW		16
#define	VXGE_HAL_DEF_REFILL_THRESHOLD_LOW		16

	u32				ack_blk_limit;
#define	VXGE_HAL_MIN_ACK_BLOCK_LIMIT			1
#define	VXGE_HAL_MAX_ACK_BLOCK_LIMIT			16
#define	VXGE_HAL_DEF_ACK_BLOCK_LIMIT			16

	u32				poll_or_doorbell;
#define	VXGE_HAL_POLL_OR_DOORBELL_POLL			1
#define	VXGE_HAL_POLL_OR_DOORBELL_DOORBELL		0
#define	VXGE_HAL_POLL_OR_DOORBELL_DEFAULT		1

	u32				stats_read_method;
#define	VXGE_HAL_STATS_READ_METHOD_DMA			1
#define	VXGE_HAL_STATS_READ_METHOD_PIO			0
#define	VXGE_HAL_STATS_READ_METHOD_DEFAULT		1

	u32				device_poll_millis;
#define	VXGE_HAL_MIN_DEVICE_POLL_MILLIS			1
#define	VXGE_HAL_MAX_DEVICE_POLL_MILLIS			100000
#define	VXGE_HAL_DEF_DEVICE_POLL_MILLIS			1000

	vxge_debug_level_e		debug_level;

	u32				debug_mask;


#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
	u32				tracebuf_size;
#define	VXGE_HAL_MIN_CIRCULAR_ARR			4096
#define	VXGE_HAL_MAX_CIRCULAR_ARR			65536
#define	VXGE_HAL_DEF_CIRCULAR_ARR			16384
#endif
} vxge_hal_device_config_t;

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_CONFIG_H */
