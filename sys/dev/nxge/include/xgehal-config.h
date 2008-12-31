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
 * $FreeBSD: src/sys/dev/nxge/include/xgehal-config.h,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef XGE_HAL_CONFIG_H
#define XGE_HAL_CONFIG_H

#include <dev/nxge/include/xge-os-pal.h>
#include <dev/nxge/include/xgehal-types.h>
#include <dev/nxge/include/xge-queue.h>

__EXTERN_BEGIN_DECLS

#define XGE_HAL_DEFAULT_USE_HARDCODE        -1

#define XGE_HAL_MAX_VIRTUAL_PATHS       8
#define XGE_HAL_MAX_INTR_PER_VP         4


/**
 * struct xge_hal_tti_config_t - Xframe Tx interrupt configuration.
 * @enabled: Set to 1, if TTI feature is enabled.
 * @urange_a: Link utilization range A. The value from 0 to 100%.
 * @ufc_a: Frame count for the utilization range A. Interrupt will be generated
 *         each time when (and only when) the line is utilized no more
 *         than @urange_a percent in the transmit direction,
 *         and number of transmitted frames is greater or equal @ufc_a.
 * @urange_b: Link utilization range B.
 * @ufc_b: Frame count for the utilization range B.
 * @urange_c: Link utilization range C.
 * @ufc_c: Frame count for the utilization range C.
 * @urange_d: Link utilization range D.
 * @ufc_d: Frame count for the utilization range D.
 * @timer_val_us: Interval of time, in microseconds, at which transmit timer
 *             interrupt is to be generated. Note that unless @timer_ci_en
 *             is set, the timer interrupt is generated only in presence
 *             of the transmit traffic. Note also that timer interrupt
 *             and utilization interrupt are two separate interrupt
 *             sources.
 * @timer_ac_en: Enable auto-cancel. That is, reset the timer if utilization
 *               interrupt was generated during the interval.
 * @timer_ci_en: Enable/disable continuous interrupt. Set this value
 *               to 1 in order to generate continuous interrupt
 *               at fixed @timer_val intervals of time, independently
 *               of whether there is transmit traffic or not.
 * @enabled: Set to 1, if TTI feature is enabled.
 *
 * Xframe transmit interrupt configuration.
 * See Xframe User Guide, Section 3.5 "Device Interrupts"
 * for more details. Note also (min, max)
 * ranges in the body of the xge_hal_tx_intr_config_t structure.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the xge_hal_tti_config_t{} structure. Please refer to the
 * corresponding header file.
 */
typedef struct xge_hal_tti_config_t {

	int             enabled;
#define XGE_HAL_TTI_ENABLE          1
#define XGE_HAL_TTI_DISABLE         0

	/* Line utilization interrupts */

	int             urange_a;
#define XGE_HAL_MIN_TX_URANGE_A         0
#define XGE_HAL_MAX_TX_URANGE_A         100

	int             ufc_a;
#define XGE_HAL_MIN_TX_UFC_A            0
#define XGE_HAL_MAX_TX_UFC_A            65535

	int             urange_b;
#define XGE_HAL_MIN_TX_URANGE_B         0
#define XGE_HAL_MAX_TX_URANGE_B         100

	int             ufc_b;
#define XGE_HAL_MIN_TX_UFC_B            0
#define XGE_HAL_MAX_TX_UFC_B            65535

	int             urange_c;
#define XGE_HAL_MIN_TX_URANGE_C         0
#define XGE_HAL_MAX_TX_URANGE_C         100

	int             ufc_c;
#define XGE_HAL_MIN_TX_UFC_C            0
#define XGE_HAL_MAX_TX_UFC_C            65535

	int             ufc_d;
#define XGE_HAL_MIN_TX_UFC_D            0
#define XGE_HAL_MAX_TX_UFC_D            65535

	int             timer_val_us;
#define XGE_HAL_MIN_TX_TIMER_VAL        0
#define XGE_HAL_MAX_TX_TIMER_VAL        65535

	int             timer_ac_en;
#define XGE_HAL_MIN_TX_TIMER_AC_EN      0
#define XGE_HAL_MAX_TX_TIMER_AC_EN      1

	int             timer_ci_en;
#define XGE_HAL_MIN_TX_TIMER_CI_EN      0
#define XGE_HAL_MAX_TX_TIMER_CI_EN      1


} xge_hal_tti_config_t;

/**
 * struct xge_hal_rti_config_t - Xframe Rx interrupt configuration.
 * @urange_a: Link utilization range A. The value from 0 to 100%.
 * @ufc_a: Frame count for the utilization range A. Interrupt will be generated
 *         each time when (and only when) the line is utilized no more
 *         than @urange_a percent inbound,
 *         and number of received frames is greater or equal @ufc_a.
 * @urange_b: Link utilization range B.
 * @ufc_b: Frame count for the utilization range B.
 * @urange_c: Link utilization range C.
 * @ufc_c: Frame count for the utilization range C.
 * @urange_d: Link utilization range D.
 * @ufc_d: Frame count for the utilization range D.
 * @timer_ac_en: Enable auto-cancel. That is, reset the timer if utilization
 *               interrupt was generated during the interval.
 * @timer_val_us: Interval of time, in microseconds, at which receive timer
 *             interrupt is to be generated. The timer interrupt is generated
 *             only in presence of the inbound traffic. Note also that timer
 *             interrupt and utilization interrupt are two separate interrupt
 *             sources.
 *
 * Xframe receive interrupt configuration.
 * See Xframe User Guide, Section 3.5 "Device Interrupts"
 * for more details. Note also (min, max)
 * ranges in the body of the xge_hal_intr_config_t structure.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the xge_hal_rti_config_t{} structure. Please refer to the
 * corresponding header file.
 */
typedef struct xge_hal_rti_config_t {

	int             urange_a;
#define XGE_HAL_MIN_RX_URANGE_A         0
#define XGE_HAL_MAX_RX_URANGE_A         127

	int             ufc_a;
#define XGE_HAL_MIN_RX_UFC_A            0
#define XGE_HAL_MAX_RX_UFC_A            65535

	int             urange_b;
#define XGE_HAL_MIN_RX_URANGE_B         0
#define XGE_HAL_MAX_RX_URANGE_B         127

	int             ufc_b;
#define XGE_HAL_MIN_RX_UFC_B            0
#define XGE_HAL_MAX_RX_UFC_B            65535

	int             urange_c;
#define XGE_HAL_MIN_RX_URANGE_C         0
#define XGE_HAL_MAX_RX_URANGE_C         127

	int             ufc_c;
#define XGE_HAL_MIN_RX_UFC_C            0
#define XGE_HAL_MAX_RX_UFC_C            65535

	int             ufc_d;
#define XGE_HAL_MIN_RX_UFC_D            0
#define XGE_HAL_MAX_RX_UFC_D            65535

	int             timer_ac_en;
#define XGE_HAL_MIN_RX_TIMER_AC_EN      0
#define XGE_HAL_MAX_RX_TIMER_AC_EN      1

	int             timer_val_us;
#define XGE_HAL_MIN_RX_TIMER_VAL        0
#define XGE_HAL_MAX_RX_TIMER_VAL        65535

} xge_hal_rti_config_t;

/**
 * struct xge_hal_fifo_queue_t - Single fifo configuration.
 * @max: Max numbers of TxDLs (that is, lists of Tx descriptors) per queue.
 * @initial: Initial numbers of TxDLs per queue (can grow up to @max).
 * @intr: Boolean. Use 1 to generate interrupt for  each completed TxDL.
 *        Use 0 otherwise.
 * @intr_vector: TBD
 * @no_snoop_bits: If non-zero, specifies no-snoop PCI operation,
 *              which generally improves latency of the host bridge operation
 *              (see PCI specification). For valid values please refer
 *              to xge_hal_fifo_queue_t{} in the driver sources.
 * @priority: TBD
 * @configured: Boolean. Use 1 to specify that the fifo is configured.
 *              Only "configured" fifos can be activated and used to post
 *              Tx descriptors. Any subset of 8 available fifos can be
 *              "configured".
 * @tti: TBD
 *
 * Single fifo configuration.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the xge_hal_fifo_queue_t{} structure. Please refer to the
 * corresponding header file.
 * See also: xge_hal_fifo_config_t{}
 */
typedef struct xge_hal_fifo_queue_t {
	int             max;
	int             initial;
#define XGE_HAL_MIN_FIFO_QUEUE_LENGTH       2
#define XGE_HAL_MAX_FIFO_QUEUE_LENGTH       8192

	int                     intr;
#define XGE_HAL_MIN_FIFO_QUEUE_INTR     0
#define XGE_HAL_MAX_FIFO_QUEUE_INTR     1

	int             intr_vector;
#define XGE_HAL_MIN_FIFO_QUEUE_INTR_VECTOR  0
#define XGE_HAL_MAX_FIFO_QUEUE_INTR_VECTOR  64

	int             no_snoop_bits;
#define XGE_HAL_MIN_FIFO_QUEUE_NO_SNOOP_DISABLED    0
#define XGE_HAL_MAX_FIFO_QUEUE_NO_SNOOP_TXD 1
#define XGE_HAL_MAX_FIFO_QUEUE_NO_SNOOP_BUFFER  2
#define XGE_HAL_MAX_FIFO_QUEUE_NO_SNOOP_ALL 3

	int             priority;
#define XGE_HAL_MIN_FIFO_PRIORITY       0
#define XGE_HAL_MAX_FIFO_PRIORITY       63

	int             configured;
#define XGE_HAL_MIN_FIFO_CONFIGURED     0
#define XGE_HAL_MAX_FIFO_CONFIGURED     1

#define XGE_HAL_MAX_FIFO_TTI_NUM        7
#define XGE_HAL_MAX_FIFO_TTI_RING_0     56
	xge_hal_tti_config_t        tti[XGE_HAL_MAX_FIFO_TTI_NUM];

} xge_hal_fifo_queue_t;

/**
 * struct xge_hal_fifo_config_t - Configuration of all 8 fifos.
 * @max_frags: Max number of Tx buffers per TxDL (that is, per single
 *             transmit operation).
 *             No more than 256 transmit buffers can be specified.
 * @max_aligned_frags: Number of fragments to be aligned out of
 *             maximum fragments (see @max_frags).
 * @reserve_threshold: Descriptor reservation threshold.
 *                     At least @reserve_threshold descriptors will remain
 *                     unallocated at all times.
 * @memblock_size: Fifo descriptors are allocated in blocks of @mem_block_size
 *                 bytes. Setting @memblock_size to page size ensures
 *                 by-page allocation of descriptors. 128K bytes is the
 *                 maximum supported block size.
 * @queue: Array of per-fifo configurations.
 * @alignment_size: per Tx fragment DMA-able memory used to align transmit data
 *                  (e.g., to align on a cache line).
 *
 * Configuration of all Xframe fifos. Includes array of xge_hal_fifo_queue_t
 * structures.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the xge_hal_fifo_config_t{} structure. Please refer to the
 * corresponding header file.
 * See also: xge_hal_ring_queue_t{}.
 */
typedef struct xge_hal_fifo_config_t {
	int             max_frags;
#define XGE_HAL_MIN_FIFO_FRAGS          1
#define XGE_HAL_MAX_FIFO_FRAGS          256

	int             reserve_threshold;
#define XGE_HAL_MIN_FIFO_RESERVE_THRESHOLD  0
#define XGE_HAL_MAX_FIFO_RESERVE_THRESHOLD  8192

	int             memblock_size;
#define XGE_HAL_MIN_FIFO_MEMBLOCK_SIZE      4096
#define XGE_HAL_MAX_FIFO_MEMBLOCK_SIZE      131072

	int                     alignment_size;
#define XGE_HAL_MIN_ALIGNMENT_SIZE      0
#define XGE_HAL_MAX_ALIGNMENT_SIZE      65536

	int             max_aligned_frags;
	/* range: (1, @max_frags) */

#define XGE_HAL_MIN_FIFO_NUM            1
#define XGE_HAL_MAX_FIFO_NUM_HERC       8
#define XGE_HAL_MAX_FIFO_NUM_TITAN      (XGE_HAL_MAX_VIRTUAL_PATHS - 1)
#define XGE_HAL_MAX_FIFO_NUM            (XGE_HAL_MAX_VIRTUAL_PATHS)
	xge_hal_fifo_queue_t        queue[XGE_HAL_MAX_FIFO_NUM];
} xge_hal_fifo_config_t;

/**
 * struct xge_hal_rts_port_t - RTS port entry
 * @num: Port number
 * @udp: Port is UDP (default TCP)
 * @src: Port is Source (default Destination)
 */
typedef struct xge_hal_rts_port_t {
	int             num;
	int             udp;
	int             src;
} xge_hal_rts_port_t;

/**
 * struct xge_hal_ring_queue_t - Single ring configuration.
 * @max: Max numbers of RxD blocks per queue
 * @initial: Initial numbers of RxD blocks per queue
 *           (can grow up to @max)
 * @buffer_mode: Receive buffer mode (1, 2, 3, or 5); for details please refer
 *               to Xframe User Guide.
 * @dram_size_mb: Size (in MB) of Xframe DRAM used for _that_ ring.
 *                Note that 64MB of available
 *                on-board DRAM is shared between receive rings.
 *                If a single ring is used, @dram_size_mb can be set to 64.
 *                Sum of all rings' @dram_size_mb cannot exceed 64.
 * @intr_vector: TBD
 * @backoff_interval_us: Time (in microseconds), after which Xframe
 *      tries to download RxDs posted by the host.
 *      Note that the "backoff" does not happen if host posts receive
 *      descriptors in the timely fashion.
 * @max_frm_len: Maximum frame length that can be received on _that_ ring.
 *               Setting this field to -1 ensures that the ring will
 *               "accept" MTU-size frames (note that MTU can be changed at
 *               runtime).
 *               Any value other than (-1) specifies a certain "hard"
 *               limit on the receive frame sizes.
 *               The field can be used to activate receive frame-length based
 *               steering.
 * @priority:    Ring priority. 0 - highest, 7 - lowest. The value is used
 *               to give prioritized access to PCI-X. See Xframe documentation
 *               for details.
 * @rth_en: Enable Receive Traffic Hashing (RTH).
 * @no_snoop_bits: If non-zero, specifies no-snoop PCI operation,
 *              which generally improves latency of the host bridge operation
 *              (see PCI specification). For valid values please refer
 *              to xge_hal_ring_queue_t{} in the driver sources.
 * @indicate_max_pkts: Sets maximum number of received frames to be processed
 *              within single interrupt.
 * @configured: Boolean. Use 1 to specify that the ring is configured.
 *              Only "configured" rings can be activated and used to post
 *              Rx descriptors. Any subset of 8 available rings can be
 *              "configured".
 * @rts_mac_en: 1 - To enable Receive MAC address steering.
 *      0 - To disable Receive MAC address steering.
 * @rth_en: TBD
 * @rts_port_en: TBD
 * @rts_ports: TBD
 * @rti: Xframe receive interrupt configuration.
 *
 * Single ring configuration.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the xge_hal_ring_queue_t{} structure. Please refer to the
 * corresponding header file.
 * See also: xge_hal_fifo_config_t{}.
 */
typedef struct xge_hal_ring_queue_t {
	int             max;
	int             initial;
#define XGE_HAL_MIN_RING_QUEUE_BLOCKS       1
#define XGE_HAL_MAX_RING_QUEUE_BLOCKS       64

	int             buffer_mode;
#define XGE_HAL_RING_QUEUE_BUFFER_MODE_1    1
#define XGE_HAL_RING_QUEUE_BUFFER_MODE_2    2
#define XGE_HAL_RING_QUEUE_BUFFER_MODE_3    3
#define XGE_HAL_RING_QUEUE_BUFFER_MODE_5    5

	int             dram_size_mb;
#define XGE_HAL_MIN_RING_QUEUE_SIZE     0
#define XGE_HAL_MAX_RING_QUEUE_SIZE_XENA    64
#define XGE_HAL_MAX_RING_QUEUE_SIZE_HERC    32

	int             intr_vector;
#define XGE_HAL_MIN_RING_QUEUE_INTR_VECTOR  0
#define XGE_HAL_MAX_RING_QUEUE_INTR_VECTOR  64

	int             backoff_interval_us;
#define XGE_HAL_MIN_BACKOFF_INTERVAL_US     1
#define XGE_HAL_MAX_BACKOFF_INTERVAL_US     125000

	int             max_frm_len;
#define XGE_HAL_MIN_MAX_FRM_LEN         -1
#define XGE_HAL_MAX_MAX_FRM_LEN         9622

	int             priority;
#define XGE_HAL_MIN_RING_PRIORITY       0
#define XGE_HAL_MAX_RING_PRIORITY       7

	int             no_snoop_bits;
#define XGE_HAL_MIN_RING_QUEUE_NO_SNOOP_DISABLED    0
#define XGE_HAL_MAX_RING_QUEUE_NO_SNOOP_RXD 1
#define XGE_HAL_MAX_RING_QUEUE_NO_SNOOP_BUFFER  2
#define XGE_HAL_MAX_RING_QUEUE_NO_SNOOP_ALL 3

	int             indicate_max_pkts;
#define XGE_HAL_MIN_RING_INDICATE_MAX_PKTS  1
#define XGE_HAL_MAX_RING_INDICATE_MAX_PKTS  65536

	int             configured;
#define XGE_HAL_MIN_RING_CONFIGURED     0
#define XGE_HAL_MAX_RING_CONFIGURED     1

	int             rts_mac_en;
#define XGE_HAL_MIN_RING_RTS_MAC_EN     0
#define XGE_HAL_MAX_RING_RTS_MAC_EN     1

	int             rth_en;
#define XGE_HAL_MIN_RING_RTH_EN         0
#define XGE_HAL_MAX_RING_RTH_EN         1

	int             rts_port_en;
#define XGE_HAL_MIN_RING_RTS_PORT_EN        0
#define XGE_HAL_MAX_RING_RTS_PORT_EN        1

#define XGE_HAL_MAX_STEERABLE_PORTS     32
	xge_hal_rts_port_t          rts_ports[XGE_HAL_MAX_STEERABLE_PORTS];

	xge_hal_rti_config_t        rti;

} xge_hal_ring_queue_t;

/**
 * struct xge_hal_ring_config_t - Array of ring configurations.
 * @memblock_size: Ring descriptors are allocated in blocks of @mem_block_size
 *                 bytes. Setting @memblock_size to page size ensures
 *                 by-page allocation of descriptors. 128K bytes is the
 *                 upper limit.
 * @scatter_mode: Xframe supports two receive scatter modes: A and B.
 *                For details please refer to Xframe User Guide.
 * @strip_vlan_tag: TBD
 * @queue: Array of all Xframe ring configurations.
 *
 * Array of ring configurations.
 * See also: xge_hal_ring_queue_t{}.
 */
typedef struct xge_hal_ring_config_t {

	int             memblock_size;
#define XGE_HAL_MIN_RING_MEMBLOCK_SIZE      4096
#define XGE_HAL_MAX_RING_MEMBLOCK_SIZE      131072

	int             scatter_mode;
#define XGE_HAL_RING_QUEUE_SCATTER_MODE_A       0
#define XGE_HAL_RING_QUEUE_SCATTER_MODE_B       1

	int             strip_vlan_tag;
#define XGE_HAL_RING_DONOT_STRIP_VLAN_TAG   0
#define XGE_HAL_RING_STRIP_VLAN_TAG     1

#define XGE_HAL_MIN_RING_NUM            1
#define XGE_HAL_MAX_RING_NUM_HERC       8
#define XGE_HAL_MAX_RING_NUM_TITAN      (XGE_HAL_MAX_VIRTUAL_PATHS - 1)
#define XGE_HAL_MAX_RING_NUM            (XGE_HAL_MAX_VIRTUAL_PATHS)
	xge_hal_ring_queue_t        queue[XGE_HAL_MAX_RING_NUM];

} xge_hal_ring_config_t;

/**
 * struct xge_hal_mac_config_t - MAC configuration.
 * @media: Transponder type.
 * @tmac_util_period: The sampling period over which the transmit utilization
 *                    is calculated.
 * @rmac_util_period: The sampling period over which the receive utilization
 *                    is calculated.
 * @rmac_strip_pad: Determines whether padding of received frames is removed by
 *                  the MAC or sent to the host.
 * @rmac_bcast_en: Enable frames containing broadcast address to be
 *                 passed to the host.
 * @rmac_pause_gen_en: Received pause generation enable.
 * @rmac_pause_rcv_en: Receive pause enable.
 * @rmac_pause_time: The value to be inserted in outgoing pause frames.
 *             Has units of pause quanta (one pause quanta = 512 bit times).
 * @mc_pause_threshold_q0q3: Contains thresholds for pause frame generation
 *     for queues 0 through 3. The threshold value indicates portion of the
 *     individual receive buffer queue size. Thresholds have a range of 0 to
 *     255, allowing 256 possible watermarks in a queue.
 * @mc_pause_threshold_q4q7: Contains thresholds for pause frame generation
 *     for queues 4 through 7. The threshold value indicates portion of the
 *     individual receive buffer queue size. Thresholds have a range of 0 to
 *     255, allowing 256 possible watermarks in a queue.
 *
 * MAC configuration. This includes various aspects of configuration, including:
 * - Pause frame threshold;
 * - sampling rate to calculate link utilization;
 * - enabling/disabling broadcasts.
 *
 * See Xframe User Guide for more details.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the xge_hal_mac_config_t{} structure. Please refer to the
 * corresponding include file.
 */
typedef struct xge_hal_mac_config_t {
	int             media;
#define XGE_HAL_MIN_MEDIA           0
#define XGE_HAL_MEDIA_SR            0
#define XGE_HAL_MEDIA_SW            1
#define XGE_HAL_MEDIA_LR            2
#define XGE_HAL_MEDIA_LW            3
#define XGE_HAL_MEDIA_ER            4
#define XGE_HAL_MEDIA_EW            5
#define XGE_HAL_MAX_MEDIA           5

	int             tmac_util_period;
#define XGE_HAL_MIN_TMAC_UTIL_PERIOD        0
#define XGE_HAL_MAX_TMAC_UTIL_PERIOD        15

	int             rmac_util_period;
#define XGE_HAL_MIN_RMAC_UTIL_PERIOD        0
#define XGE_HAL_MAX_RMAC_UTIL_PERIOD        15

	int             rmac_bcast_en;
#define XGE_HAL_MIN_RMAC_BCAST_EN       0
#define XGE_HAL_MAX_RMAC_BCAST_EN       1

	int             rmac_pause_gen_en;
#define XGE_HAL_MIN_RMAC_PAUSE_GEN_EN       0
#define XGE_HAL_MAX_RMAC_PAUSE_GEN_EN       1

	int             rmac_pause_rcv_en;
#define XGE_HAL_MIN_RMAC_PAUSE_RCV_EN       0
#define XGE_HAL_MAX_RMAC_PAUSE_RCV_EN       1

	int             rmac_pause_time;
#define XGE_HAL_MIN_RMAC_HIGH_PTIME     16
#define XGE_HAL_MAX_RMAC_HIGH_PTIME     65535

	int             mc_pause_threshold_q0q3;
#define XGE_HAL_MIN_MC_PAUSE_THRESHOLD_Q0Q3 0
#define XGE_HAL_MAX_MC_PAUSE_THRESHOLD_Q0Q3 254

	int             mc_pause_threshold_q4q7;
#define XGE_HAL_MIN_MC_PAUSE_THRESHOLD_Q4Q7 0
#define XGE_HAL_MAX_MC_PAUSE_THRESHOLD_Q4Q7 254

} xge_hal_mac_config_t;

/**
 * struct xge_hal_device_config_t - Device configuration.
 * @mtu: Current mtu size.
 * @isr_polling_cnt: Maximum number of times to "poll" for Tx and Rx
 *                   completions. Used in xge_hal_device_handle_irq().
 * @latency_timer: Specifies, in units of PCI bus clocks, and in conformance
 *                 with the PCI Specification, the value of the Latency Timer
 *                 for this PCI bus master.
 * Specify either zero or -1 to use BIOS default.
 * @napi_weight: (TODO)
 * @max_splits_trans: Maximum number of PCI-X split transactions.
 * Specify (-1) to use BIOS default.
 * @mmrb_count: Maximum Memory Read Byte Count. Use (-1) to use default
 *              BIOS value. Otherwise: mmrb_count = 0 corresponds to 512B;
 *              1 - 1KB, 2 - 2KB, and 3 - 4KB.
 * @shared_splits: The number of Outstanding Split Transactions that is
 *              shared by Tx and Rx requests. The device stops issuing Tx
 *              requests once the number of Outstanding Split Transactions is
 *              equal to the value of Shared_Splits.
 *              A value of zero indicates that the Tx and Rx share all allocated
 *              Split Requests, i.e. the device can issue both types (Tx and Rx)
 *              of read requests until the number of Maximum Outstanding Split
 *              Transactions is reached.
 * @stats_refresh_time_sec: Sets the default interval for automatic stats transfer
 *              to the host. This includes MAC stats as well as PCI stats.
 *              See xge_hal_stats_hw_info_t{}.
 * @pci_freq_mherz: PCI clock frequency, e.g.: 133 for 133MHz.
 * @intr_mode: Line, MSI, or MSI-X interrupt.
 * @sched_timer_us: If greater than zero, specifies time interval
 *              (in microseconds) for the device to generate
 *              interrupt. Note that unlike tti and rti interrupts,
 *              the scheduled interrupt is generated independently of
 *              whether there is transmit or receive traffic, respectively.
 * @sched_timer_one_shot: 1 - generate scheduled interrupt only once.
 *              0 - generate scheduled interrupt periodically at the specified
 *              @sched_timer_us interval.
 *
 * @ring: See xge_hal_ring_config_t{}.
 * @mac: See xge_hal_mac_config_t{}.
 * @tti: See xge_hal_tti_config_t{}.
 * @fifo: See xge_hal_fifo_config_t{}.
 *
 * @dump_on_serr: Dump adapter state ("about", statistics, registers) on SERR#.
 * @dump_on_eccerr: Dump adapter state ("about", statistics, registers) on
 *                  ECC error.
 * @dump_on_parityerr: Dump adapter state ("about", statistics, registers) on
 *                     parity error.
 * @rth_en: Enable Receive Traffic Hashing(RTH) using IT(Indirection Table).
 * @rth_bucket_size: RTH bucket width (in bits). For valid range please see
 *                   xge_hal_device_config_t{} in the driver sources.
 * @rth_spdm_en: Enable Receive Traffic Hashing(RTH) using SPDM(Socket Pair
 *      Direct Match).
 * @rth_spdm_use_l4: Set to 1, if the L4 ports are used in the calculation of
 *  hash value in the RTH SPDM based steering.
 * @rxufca_intr_thres: (TODO)
 * @rxufca_lo_lim: (TODO)
 * @rxufca_hi_lim: (TODO)
 * @rxufca_lbolt_period: (TODO)
 * @link_valid_cnt: link-valid counting is done only at device-open time,
 * to determine with the specified certainty that the link is up. See also
 * @link_retry_cnt.
 * @link_retry_cnt: Max number of polls for link-up. Done only at device
 * open time. Reducing this value as well as the previous @link_valid_cnt,
 * speeds up device startup, which may be important if the driver
 * is compiled into OS.
 * @link_stability_period: Specify the period for which the link must be
 * stable in order for the adapter to declare "LINK UP".
 * The enumerated settings (see Xframe-II UG) are:
 *      0 ........... instantaneous
 *      1 ........... 500 ³s
 *      2 ........... 1 ms
 *      3 ........... 64 ms
 *      4 ........... 256 ms
 *      5 ........... 512 ms
 *      6 ........... 1 s
 *      7 ........... 2 s
 * @device_poll_millis: Specify the interval (in mulliseconds) between
 * successive xge_hal_device_poll() runs.
 * stable in order for the adapter to declare "LINK UP".
 * @no_isr_events: TBD
 * @lro_sg_size: TBD
 * @lro_frm_len: TBD
 * @bimodal_interrupts: Enable bimodal interrupts in device
 * @bimodal_timer_lo_us: TBD
 * @bimodal_timer_hi_us: TBD
 * @rts_mac_en: Enable Receive Traffic Steering using MAC destination address
 * @rts_qos_en: TBD
 * @rts_port_en: TBD
 * @vp_config: Configuration for virtual paths
 * @max_cqe_groups:  The maximum number of adapter CQE group blocks a CQRQ
 * can own at any one time.
 * @max_num_wqe_od_groups: The maximum number of WQE Headers/OD Groups that
 * this S-RQ can own at any one time.
 * @no_wqe_threshold: Maximum number of times adapter polls WQE Hdr blocks for
 * WQEs before generating a message or interrupt.
 * @refill_threshold_high:This field provides a hysteresis upper bound for
 * automatic adapter refill operations.
 * @refill_threshold_low:This field provides a hysteresis lower bound for
 * automatic adapter refill operations.
 * @eol_policy:This field sets the policy for handling the end of list condition.
 * 2'b00 - When EOL is reached,poll until last block wrapper size is no longer 0.
 * 2'b01 - Send UMQ message when EOL is reached.
 * 2'b1x - Poll until the poll_count_max is reached and if still EOL,send UMQ message
 * @eol_poll_count_max:sets the maximum number of times the queue manager will poll for
 * a non-zero block wrapper before giving up and sending a UMQ message
 * @ack_blk_limit: Limit on the maximum number of ACK list blocks that can be held
 * by a session at any one time.
 * @poll_or_doorbell: TBD
 *
 * Xframe configuration.
 * Contains per-device configuration parameters, including:
 * - latency timer (settable via PCI configuration space);
 * - maximum number of split transactions;
 * - maximum number of shared splits;
 * - stats sampling interval, etc.
 *
 * In addition, xge_hal_device_config_t{} includes "subordinate"
 * configurations, including:
 * - fifos and rings;
 * - MAC (see xge_hal_mac_config_t{}).
 *
 * See Xframe User Guide for more details.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the xge_hal_device_config_t{} structure. Please refer to the
 * corresponding include file.
 * See also: xge_hal_tti_config_t{}, xge_hal_stats_hw_info_t{},
 * xge_hal_mac_config_t{}.
 */
typedef struct xge_hal_device_config_t {
	int             mtu;
#define XGE_HAL_MIN_INITIAL_MTU                 XGE_HAL_MIN_MTU
#define XGE_HAL_MAX_INITIAL_MTU                 XGE_HAL_MAX_MTU

	int             isr_polling_cnt;
#define XGE_HAL_MIN_ISR_POLLING_CNT             0
#define XGE_HAL_MAX_ISR_POLLING_CNT             65536

	int             latency_timer;
#define XGE_HAL_USE_BIOS_DEFAULT_LATENCY        -1
#define XGE_HAL_MIN_LATENCY_TIMER               8
#define XGE_HAL_MAX_LATENCY_TIMER               255

	int             napi_weight;
#define XGE_HAL_DEF_NAPI_WEIGHT                 64

	int             max_splits_trans;
#define XGE_HAL_USE_BIOS_DEFAULT_SPLITS         -1
#define XGE_HAL_ONE_SPLIT_TRANSACTION           0
#define XGE_HAL_TWO_SPLIT_TRANSACTION           1
#define XGE_HAL_THREE_SPLIT_TRANSACTION         2
#define XGE_HAL_FOUR_SPLIT_TRANSACTION          3
#define XGE_HAL_EIGHT_SPLIT_TRANSACTION         4
#define XGE_HAL_TWELVE_SPLIT_TRANSACTION        5
#define XGE_HAL_SIXTEEN_SPLIT_TRANSACTION       6
#define XGE_HAL_THIRTYTWO_SPLIT_TRANSACTION     7

	int             mmrb_count;
#define XGE_HAL_DEFAULT_BIOS_MMRB_COUNT         -1
#define XGE_HAL_MIN_MMRB_COUNT                  0 /* 512b */
#define XGE_HAL_MAX_MMRB_COUNT                  3 /* 4k */

	int             shared_splits;
#define XGE_HAL_MIN_SHARED_SPLITS               0
#define XGE_HAL_MAX_SHARED_SPLITS               31

	int             stats_refresh_time_sec;
#define XGE_HAL_STATS_REFRESH_DISABLE           0
#define XGE_HAL_MIN_STATS_REFRESH_TIME          1
#define XGE_HAL_MAX_STATS_REFRESH_TIME          300

	int             pci_freq_mherz;
#define XGE_HAL_PCI_FREQ_MHERZ_33               33
#define XGE_HAL_PCI_FREQ_MHERZ_66               66
#define XGE_HAL_PCI_FREQ_MHERZ_100              100
#define XGE_HAL_PCI_FREQ_MHERZ_133              133
#define XGE_HAL_PCI_FREQ_MHERZ_266              266

	int             intr_mode;
#define XGE_HAL_INTR_MODE_IRQLINE               0
#define XGE_HAL_INTR_MODE_MSI                   1
#define XGE_HAL_INTR_MODE_MSIX                  2

	int             sched_timer_us;
#define XGE_HAL_SCHED_TIMER_DISABLED            0
#define XGE_HAL_SCHED_TIMER_MIN                 0
#define XGE_HAL_SCHED_TIMER_MAX                 0xFFFFF

	int             sched_timer_one_shot;
#define XGE_HAL_SCHED_TIMER_ON_SHOT_DISABLE     0
#define XGE_HAL_SCHED_TIMER_ON_SHOT_ENABLE      1

	xge_hal_ring_config_t       ring;
	xge_hal_mac_config_t        mac;
	xge_hal_fifo_config_t       fifo;

	int             dump_on_serr;
#define XGE_HAL_DUMP_ON_SERR_DISABLE            0
#define XGE_HAL_DUMP_ON_SERR_ENABLE             1

	int             dump_on_eccerr;
#define XGE_HAL_DUMP_ON_ECCERR_DISABLE          0
#define XGE_HAL_DUMP_ON_ECCERR_ENABLE           1

	int             dump_on_parityerr;
#define XGE_HAL_DUMP_ON_PARITYERR_DISABLE       0
#define XGE_HAL_DUMP_ON_PARITYERR_ENABLE        1

	int             rth_en;
#define XGE_HAL_RTH_DISABLE                     0
#define XGE_HAL_RTH_ENABLE                      1

	int             rth_bucket_size;
#define XGE_HAL_MIN_RTH_BUCKET_SIZE             1
#define XGE_HAL_MAX_RTH_BUCKET_SIZE             8

	int             rth_spdm_en;
#define XGE_HAL_RTH_SPDM_DISABLE                0
#define XGE_HAL_RTH_SPDM_ENABLE                 1

	int             rth_spdm_use_l4;
#define XGE_HAL_RTH_SPDM_USE_L4                 1

	int             rxufca_intr_thres;
#define XGE_HAL_RXUFCA_INTR_THRES_MIN           1
#define XGE_HAL_RXUFCA_INTR_THRES_MAX           4096

	int             rxufca_lo_lim;
#define XGE_HAL_RXUFCA_LO_LIM_MIN               1
#define XGE_HAL_RXUFCA_LO_LIM_MAX               16

	int             rxufca_hi_lim;
#define XGE_HAL_RXUFCA_HI_LIM_MIN               1
#define XGE_HAL_RXUFCA_HI_LIM_MAX               256

	int             rxufca_lbolt_period;
#define XGE_HAL_RXUFCA_LBOLT_PERIOD_MIN         1
#define XGE_HAL_RXUFCA_LBOLT_PERIOD_MAX         1024

	int             link_valid_cnt;
#define XGE_HAL_LINK_VALID_CNT_MIN              0
#define XGE_HAL_LINK_VALID_CNT_MAX              127

	int             link_retry_cnt;
#define XGE_HAL_LINK_RETRY_CNT_MIN              0
#define XGE_HAL_LINK_RETRY_CNT_MAX              127

	int             link_stability_period;
#define XGE_HAL_DEFAULT_LINK_STABILITY_PERIOD   2 /* 1ms */
#define XGE_HAL_MIN_LINK_STABILITY_PERIOD       0 /* instantaneous */
#define XGE_HAL_MAX_LINK_STABILITY_PERIOD       7 /* 2s */

	int             device_poll_millis;
#define XGE_HAL_DEFAULT_DEVICE_POLL_MILLIS      1000
#define XGE_HAL_MIN_DEVICE_POLL_MILLIS          1
#define XGE_HAL_MAX_DEVICE_POLL_MILLIS          100000

	int             no_isr_events;
#define XGE_HAL_NO_ISR_EVENTS_MIN               0
#define XGE_HAL_NO_ISR_EVENTS_MAX               1

	int             lro_sg_size;
#define XGE_HAL_LRO_DEFAULT_SG_SIZE             10
#define XGE_HAL_LRO_MIN_SG_SIZE                 1
#define XGE_HAL_LRO_MAX_SG_SIZE                 64

	int             lro_frm_len;
#define XGE_HAL_LRO_DEFAULT_FRM_LEN             65536
#define XGE_HAL_LRO_MIN_FRM_LEN                 4096
#define XGE_HAL_LRO_MAX_FRM_LEN                 65536

	int             bimodal_interrupts;
#define XGE_HAL_BIMODAL_INTR_MIN                -1
#define XGE_HAL_BIMODAL_INTR_MAX                1

	int             bimodal_timer_lo_us;
#define XGE_HAL_BIMODAL_TIMER_LO_US_MIN         1
#define XGE_HAL_BIMODAL_TIMER_LO_US_MAX         127

	int             bimodal_timer_hi_us;
#define XGE_HAL_BIMODAL_TIMER_HI_US_MIN         128
#define XGE_HAL_BIMODAL_TIMER_HI_US_MAX         65535

	int             rts_mac_en;
#define XGE_HAL_RTS_MAC_DISABLE                 0
#define XGE_HAL_RTS_MAC_ENABLE                  1

	int             rts_qos_en;
#define XGE_HAL_RTS_QOS_DISABLE                 0
#define XGE_HAL_RTS_QOS_ENABLE                  1

	int             rts_port_en;
#define XGE_HAL_RTS_PORT_DISABLE                0
#define XGE_HAL_RTS_PORT_ENABLE                 1

} xge_hal_device_config_t;

/**
 * struct xge_hal_driver_config_t - HAL (layer) configuration.
 * @periodic_poll_interval_millis: Interval, in milliseconds, which is used to
 *                                 periodically poll HAL, i.e, invoke
 *                                 xge_hal_device_poll().
 *                                 Note that HAL does not maintain its own
 *                                 polling context. HAL relies on ULD to
 *                                 provide one.
 * @queue_size_initial: Initial size of the HAL protected event queue.
 *                      The queue is shared by HAL and upper-layer drivers.
 *                      The queue is used to exchange and process slow-path
 *                      events. See xge_hal_event_e.
 * @queue_size_max: Maximum size of the HAL queue. Depending on the load,
 *                  the queue may grow at run-time up to @queue_max_size.
 * @tracebuf_size: Size of the trace buffer. Set it to '0' to disable.
 * HAL configuration. (Note: do not confuse HAL layer with (possibly multiple)
 * HAL devices.)
 * Currently this structure contains just a few basic values.
 * Note: Valid (min, max) range for each attribute is specified in the body of
 * the structure. Please refer to the corresponding header file.
 * See also: xge_hal_device_poll()
 */
typedef struct xge_hal_driver_config_t {
	int             queue_size_initial;
#define XGE_HAL_MIN_QUEUE_SIZE_INITIAL      1
#define XGE_HAL_MAX_QUEUE_SIZE_INITIAL      16

	int             queue_size_max;
#define XGE_HAL_MIN_QUEUE_SIZE_MAX          1
#define XGE_HAL_MAX_QUEUE_SIZE_MAX          16

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
	int             tracebuf_size;
#define XGE_HAL_MIN_CIRCULAR_ARR            4096
#define XGE_HAL_MAX_CIRCULAR_ARR            1048576
#define XGE_HAL_DEF_CIRCULAR_ARR            XGE_OS_HOST_PAGE_SIZE

	int             tracebuf_timestamp_en;
#define XGE_HAL_MIN_TIMESTAMP_EN            0
#define XGE_HAL_MAX_TIMESTAMP_EN            1
#endif

} xge_hal_driver_config_t;


/* ========================== PRIVATE API ================================= */

xge_hal_status_e
__hal_device_config_check_common (xge_hal_device_config_t *new_config);

xge_hal_status_e
__hal_device_config_check_xena (xge_hal_device_config_t *new_config);

xge_hal_status_e
__hal_device_config_check_herc (xge_hal_device_config_t *new_config);

xge_hal_status_e
__hal_driver_config_check (xge_hal_driver_config_t *new_config);

__EXTERN_END_DECLS

#endif /* XGE_HAL_CONFIG_H */
