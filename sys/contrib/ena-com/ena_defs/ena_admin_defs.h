/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2015-2023 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ENA_ADMIN_H_
#define _ENA_ADMIN_H_

#define ENA_ADMIN_EXTRA_PROPERTIES_STRING_LEN 32
#define ENA_ADMIN_EXTRA_PROPERTIES_COUNT     32

#define ENA_ADMIN_RSS_KEY_PARTS              10

#define ENA_ADMIN_CUSTOMER_METRICS_SUPPORT_MASK 0x3F
#define ENA_ADMIN_CUSTOMER_METRICS_MIN_SUPPORT_MASK 0x1F

 /* customer metrics - in correlation with
  * ENA_ADMIN_CUSTOMER_METRICS_SUPPORT_MASK
  */
enum ena_admin_customer_metrics_id {
	ENA_ADMIN_BW_IN_ALLOWANCE_EXCEEDED         = 0,
	ENA_ADMIN_BW_OUT_ALLOWANCE_EXCEEDED        = 1,
	ENA_ADMIN_PPS_ALLOWANCE_EXCEEDED           = 2,
	ENA_ADMIN_CONNTRACK_ALLOWANCE_EXCEEDED     = 3,
	ENA_ADMIN_LINKLOCAL_ALLOWANCE_EXCEEDED     = 4,
	ENA_ADMIN_CONNTRACK_ALLOWANCE_AVAILABLE    = 5,
};

enum ena_admin_aq_opcode {
	ENA_ADMIN_CREATE_SQ                         = 1,
	ENA_ADMIN_DESTROY_SQ                        = 2,
	ENA_ADMIN_CREATE_CQ                         = 3,
	ENA_ADMIN_DESTROY_CQ                        = 4,
	ENA_ADMIN_GET_FEATURE                       = 8,
	ENA_ADMIN_SET_FEATURE                       = 9,
	ENA_ADMIN_GET_STATS                         = 11,
};

enum ena_admin_aq_completion_status {
	ENA_ADMIN_SUCCESS                           = 0,
	ENA_ADMIN_RESOURCE_ALLOCATION_FAILURE       = 1,
	ENA_ADMIN_BAD_OPCODE                        = 2,
	ENA_ADMIN_UNSUPPORTED_OPCODE                = 3,
	ENA_ADMIN_MALFORMED_REQUEST                 = 4,
	/* Additional status is provided in ACQ entry extended_status */
	ENA_ADMIN_ILLEGAL_PARAMETER                 = 5,
	ENA_ADMIN_UNKNOWN_ERROR                     = 6,
	ENA_ADMIN_RESOURCE_BUSY                     = 7,
};

/* subcommands for the set/get feature admin commands */
enum ena_admin_aq_feature_id {
	ENA_ADMIN_DEVICE_ATTRIBUTES                 = 1,
	ENA_ADMIN_MAX_QUEUES_NUM                    = 2,
	ENA_ADMIN_HW_HINTS                          = 3,
	ENA_ADMIN_LLQ                               = 4,
	ENA_ADMIN_EXTRA_PROPERTIES_STRINGS          = 5,
	ENA_ADMIN_EXTRA_PROPERTIES_FLAGS            = 6,
	ENA_ADMIN_MAX_QUEUES_EXT                    = 7,
	ENA_ADMIN_RSS_HASH_FUNCTION                 = 10,
	ENA_ADMIN_STATELESS_OFFLOAD_CONFIG          = 11,
	ENA_ADMIN_RSS_INDIRECTION_TABLE_CONFIG      = 12,
	ENA_ADMIN_MTU                               = 14,
	ENA_ADMIN_RSS_HASH_INPUT                    = 18,
	ENA_ADMIN_INTERRUPT_MODERATION              = 20,
	ENA_ADMIN_AENQ_CONFIG                       = 26,
	ENA_ADMIN_LINK_CONFIG                       = 27,
	ENA_ADMIN_HOST_ATTR_CONFIG                  = 28,
	ENA_ADMIN_PHC_CONFIG                        = 29,
	ENA_ADMIN_FEATURES_OPCODE_NUM               = 32,
};

/* feature version for the set/get ENA_ADMIN_LLQ feature admin commands */
enum ena_admin_llq_feature_version {
	/* legacy base version in older drivers */
	ENA_ADMIN_LLQ_FEATURE_VERSION_0_LEGACY      = 0,
	/* support entry_size recommendation by device */
	ENA_ADMIN_LLQ_FEATURE_VERSION_1             = 1,
};

/* device capabilities */
enum ena_admin_aq_caps_id {
	ENA_ADMIN_ENI_STATS                         = 0,
	/* ENA SRD customer metrics */
	ENA_ADMIN_ENA_SRD_INFO                      = 1,
	ENA_ADMIN_CUSTOMER_METRICS                  = 2,
	ENA_ADMIN_EXTENDED_RESET_REASONS	    = 3,
	ENA_ADMIN_CDESC_MBZ                         = 4,
};

enum ena_admin_placement_policy_type {
	/* descriptors and headers are in host memory */
	ENA_ADMIN_PLACEMENT_POLICY_HOST             = 1,
	/* descriptors and headers are in device memory (a.k.a Low Latency
	 * Queue)
	 */
	ENA_ADMIN_PLACEMENT_POLICY_DEV              = 3,
};

enum ena_admin_link_types {
	ENA_ADMIN_LINK_SPEED_1G                     = 0x1,
	ENA_ADMIN_LINK_SPEED_2_HALF_G               = 0x2,
	ENA_ADMIN_LINK_SPEED_5G                     = 0x4,
	ENA_ADMIN_LINK_SPEED_10G                    = 0x8,
	ENA_ADMIN_LINK_SPEED_25G                    = 0x10,
	ENA_ADMIN_LINK_SPEED_40G                    = 0x20,
	ENA_ADMIN_LINK_SPEED_50G                    = 0x40,
	ENA_ADMIN_LINK_SPEED_100G                   = 0x80,
	ENA_ADMIN_LINK_SPEED_200G                   = 0x100,
	ENA_ADMIN_LINK_SPEED_400G                   = 0x200,
};

enum ena_admin_completion_policy_type {
	/* completion queue entry for each sq descriptor */
	ENA_ADMIN_COMPLETION_POLICY_DESC            = 0,
	/* completion queue entry upon request in sq descriptor */
	ENA_ADMIN_COMPLETION_POLICY_DESC_ON_DEMAND  = 1,
	/* current queue head pointer is updated in OS memory upon sq
	 * descriptor request
	 */
	ENA_ADMIN_COMPLETION_POLICY_HEAD_ON_DEMAND  = 2,
	/* current queue head pointer is updated in OS memory for each sq
	 * descriptor
	 */
	ENA_ADMIN_COMPLETION_POLICY_HEAD            = 3,
};

/* basic stats return ena_admin_basic_stats while extanded stats return a
 * buffer (string format) with additional statistics per queue and per
 * device id
 */
enum ena_admin_get_stats_type {
	ENA_ADMIN_GET_STATS_TYPE_BASIC              = 0,
	ENA_ADMIN_GET_STATS_TYPE_EXTENDED           = 1,
	/* extra HW stats for specific network interface */
	ENA_ADMIN_GET_STATS_TYPE_ENI                = 2,
	/* extra HW stats for ENA SRD */
	ENA_ADMIN_GET_STATS_TYPE_ENA_SRD            = 3,
	ENA_ADMIN_GET_STATS_TYPE_CUSTOMER_METRICS   = 4,

};

enum ena_admin_get_stats_scope {
	ENA_ADMIN_SPECIFIC_QUEUE                    = 0,
	ENA_ADMIN_ETH_TRAFFIC                       = 1,
};

enum ena_admin_phc_feature_version {
	/* Readless with error_bound */
	ENA_ADMIN_PHC_FEATURE_VERSION_0             = 0,
};

enum ena_admin_phc_error_flags {
	ENA_ADMIN_PHC_ERROR_FLAG_TIMESTAMP   = BIT(0),
	ENA_ADMIN_PHC_ERROR_FLAG_ERROR_BOUND = BIT(1),
};

/* ENA SRD configuration for ENI */
enum ena_admin_ena_srd_flags {
	/* Feature enabled */
	ENA_ADMIN_ENA_SRD_ENABLED                   = BIT(0),
	/* UDP support enabled */
	ENA_ADMIN_ENA_SRD_UDP_ENABLED               = BIT(1),
	/* Bypass Rx UDP ordering */
	ENA_ADMIN_ENA_SRD_UDP_ORDERING_BYPASS_ENABLED = BIT(2),
};

struct ena_admin_aq_common_desc {
	/* 11:0 : command_id
	 * 15:12 : reserved12
	 */
	uint16_t command_id;

	/* as appears in ena_admin_aq_opcode */
	uint8_t opcode;

	/* 0 : phase
	 * 1 : ctrl_data - control buffer address valid
	 * 2 : ctrl_data_indirect - control buffer address
	 *    points to list of pages with addresses of control
	 *    buffers
	 * 7:3 : reserved3
	 */
	uint8_t flags;
};

/* used in ena_admin_aq_entry. Can point directly to control data, or to a
 * page list chunk. Used also at the end of indirect mode page list chunks,
 * for chaining.
 */
struct ena_admin_ctrl_buff_info {
	uint32_t length;

	struct ena_common_mem_addr address;
};

struct ena_admin_sq {
	uint16_t sq_idx;

	/* 4:0 : reserved
	 * 7:5 : sq_direction - 0x1 - Tx; 0x2 - Rx
	 */
	uint8_t sq_identity;

	uint8_t reserved1;
};

struct ena_admin_aq_entry {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	union {
		uint32_t inline_data_w1[3];

		struct ena_admin_ctrl_buff_info control_buffer;
	} u;

	uint32_t inline_data_w4[12];
};

struct ena_admin_acq_common_desc {
	/* command identifier to associate it with the aq descriptor
	 * 11:0 : command_id
	 * 15:12 : reserved12
	 */
	uint16_t command;

	uint8_t status;

	/* 0 : phase
	 * 7:1 : reserved1
	 */
	uint8_t flags;

	uint16_t extended_status;

	/* indicates to the driver which AQ entry has been consumed by the
	 * device and could be reused
	 */
	uint16_t sq_head_indx;
};

struct ena_admin_acq_entry {
	struct ena_admin_acq_common_desc acq_common_descriptor;

	uint32_t response_specific_data[14];
};

struct ena_admin_aq_create_sq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* 4:0 : reserved0_w1
	 * 7:5 : sq_direction - 0x1 - Tx, 0x2 - Rx
	 */
	uint8_t sq_identity;

	uint8_t reserved8_w1;

	/* 3:0 : placement_policy - Describing where the SQ
	 *    descriptor ring and the SQ packet headers reside:
	 *    0x1 - descriptors and headers are in OS memory,
	 *    0x3 - descriptors and headers in device memory
	 *    (a.k.a Low Latency Queue)
	 * 6:4 : completion_policy - Describing what policy
	 *    to use for generation completion entry (cqe) in
	 *    the CQ associated with this SQ: 0x0 - cqe for each
	 *    sq descriptor, 0x1 - cqe upon request in sq
	 *    descriptor, 0x2 - current queue head pointer is
	 *    updated in OS memory upon sq descriptor request
	 *    0x3 - current queue head pointer is updated in OS
	 *    memory for each sq descriptor
	 * 7 : reserved15_w1
	 */
	uint8_t sq_caps_2;

	/* 0 : is_physically_contiguous - Described if the
	 *    queue ring memory is allocated in physical
	 *    contiguous pages or split.
	 * 7:1 : reserved17_w1
	 */
	uint8_t sq_caps_3;

	/* associated completion queue id. This CQ must be created prior to SQ
	 * creation
	 */
	uint16_t cq_idx;

	/* submission queue depth in entries */
	uint16_t sq_depth;

	/* SQ physical base address in OS memory. This field should not be
	 * used for Low Latency queues. Has to be page aligned.
	 */
	struct ena_common_mem_addr sq_ba;

	/* specifies queue head writeback location in OS memory. Valid if
	 * completion_policy is set to completion_policy_head_on_demand or
	 * completion_policy_head. Has to be cache aligned
	 */
	struct ena_common_mem_addr sq_head_writeback;

	uint32_t reserved0_w7;

	uint32_t reserved0_w8;
};

enum ena_admin_sq_direction {
	ENA_ADMIN_SQ_DIRECTION_TX                   = 1,
	ENA_ADMIN_SQ_DIRECTION_RX                   = 2,
};

struct ena_admin_acq_create_sq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;

	uint16_t sq_idx;

	uint16_t reserved;

	/* queue doorbell address as an offset to PCIe MMIO REG BAR */
	uint32_t sq_doorbell_offset;

	/* low latency queue ring base address as an offset to PCIe MMIO
	 * LLQ_MEM BAR
	 */
	uint32_t llq_descriptors_offset;

	/* low latency queue headers' memory as an offset to PCIe MMIO
	 * LLQ_MEM BAR
	 */
	uint32_t llq_headers_offset;
};

struct ena_admin_aq_destroy_sq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	struct ena_admin_sq sq;
};

struct ena_admin_acq_destroy_sq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;
};

struct ena_admin_aq_create_cq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	/* 4:0 : reserved5
	 * 5 : interrupt_mode_enabled - if set, cq operates
	 *    in interrupt mode, otherwise - polling
	 * 7:6 : reserved6
	 */
	uint8_t cq_caps_1;

	/* 4:0 : cq_entry_size_words - size of CQ entry in
	 *    32-bit words, valid values: 4, 8.
	 * 7:5 : reserved7
	 */
	uint8_t cq_caps_2;

	/* completion queue depth in # of entries. must be power of 2 */
	uint16_t cq_depth;

	/* msix vector assigned to this cq */
	uint32_t msix_vector;

	/* cq physical base address in OS memory. CQ must be physically
	 * contiguous
	 */
	struct ena_common_mem_addr cq_ba;
};

struct ena_admin_acq_create_cq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;

	uint16_t cq_idx;

	/* actual cq depth in number of entries */
	uint16_t cq_actual_depth;

	uint32_t numa_node_register_offset;

	uint32_t cq_head_db_register_offset;

	uint32_t cq_interrupt_unmask_register_offset;
};

struct ena_admin_aq_destroy_cq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	uint16_t cq_idx;

	uint16_t reserved1;
};

struct ena_admin_acq_destroy_cq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;
};

/* ENA AQ Get Statistics command. Extended statistics are placed in control
 * buffer pointed by AQ entry
 */
struct ena_admin_aq_get_stats_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	union {
		/* command specific inline data */
		uint32_t inline_data_w1[3];

		struct ena_admin_ctrl_buff_info control_buffer;
	} u;

	/* stats type as defined in enum ena_admin_get_stats_type */
	uint8_t type;

	/* stats scope defined in enum ena_admin_get_stats_scope */
	uint8_t scope;

	uint16_t reserved3;

	/* queue id. used when scope is specific_queue */
	uint16_t queue_idx;

	/* device id, value 0xFFFF means mine. only privileged device can get
	 * stats of other device
	 */
	uint16_t device_id;

	/* a bitmap representing the requested metric values */
	uint64_t requested_metrics;
};

/* Basic Statistics Command. */
struct ena_admin_basic_stats {
	uint32_t tx_bytes_low;

	uint32_t tx_bytes_high;

	uint32_t tx_pkts_low;

	uint32_t tx_pkts_high;

	uint32_t rx_bytes_low;

	uint32_t rx_bytes_high;

	uint32_t rx_pkts_low;

	uint32_t rx_pkts_high;

	uint32_t rx_drops_low;

	uint32_t rx_drops_high;

	uint32_t tx_drops_low;

	uint32_t tx_drops_high;

	uint32_t rx_overruns_low;

	uint32_t rx_overruns_high;
};

/* ENI Statistics Command. */
struct ena_admin_eni_stats {
	/* The number of packets shaped due to inbound aggregate BW
	 * allowance being exceeded
	 */
	uint64_t bw_in_allowance_exceeded;

	/* The number of packets shaped due to outbound aggregate BW
	 * allowance being exceeded
	 */
	uint64_t bw_out_allowance_exceeded;

	/* The number of packets shaped due to PPS allowance being exceeded */
	uint64_t pps_allowance_exceeded;

	/* The number of packets shaped due to connection tracking
	 * allowance being exceeded and leading to failure in establishment
	 * of new connections
	 */
	uint64_t conntrack_allowance_exceeded;

	/* The number of packets shaped due to linklocal packet rate
	 * allowance being exceeded
	 */
	uint64_t linklocal_allowance_exceeded;
};

struct ena_admin_ena_srd_stats {
	/* Number of packets transmitted over ENA SRD */
	uint64_t ena_srd_tx_pkts;

	/* Number of packets transmitted or could have been
	 * transmitted over ENA SRD
	 */
	uint64_t ena_srd_eligible_tx_pkts;

	/* Number of packets received over ENA SRD */
	uint64_t ena_srd_rx_pkts;

	/* Percentage of the ENA SRD resources that is in use */
	uint64_t ena_srd_resource_utilization;
};

/* ENA SRD Statistics Command */
struct ena_admin_ena_srd_info {
	/* ENA SRD configuration bitmap. See ena_admin_ena_srd_flags for
	 * details
	 */
	uint64_t flags;

	struct ena_admin_ena_srd_stats ena_srd_stats;
};

/* Customer Metrics Command. */
struct ena_admin_customer_metrics {
	/* A bitmap representing the reported customer metrics according to
	 * the order they are reported
	 */
	uint64_t reported_metrics;
};

struct ena_admin_acq_get_stats_resp {
	struct ena_admin_acq_common_desc acq_common_desc;

	union {
		uint64_t raw[7];

		struct ena_admin_basic_stats basic_stats;

		struct ena_admin_eni_stats eni_stats;

		struct ena_admin_ena_srd_info ena_srd_info;

		struct ena_admin_customer_metrics customer_metrics;
	} u;
};

struct ena_admin_get_set_feature_common_desc {
	/* 1:0 : select - 0x1 - current value; 0x3 - default
	 *    value
	 * 7:3 : reserved3
	 */
	uint8_t flags;

	/* as appears in ena_admin_aq_feature_id */
	uint8_t feature_id;

	/* The driver specifies the max feature version it supports and the
	 * device responds with the currently supported feature version. The
	 * field is zero based
	 */
	uint8_t feature_version;

	uint8_t reserved8;
};

struct ena_admin_device_attr_feature_desc {
	uint32_t impl_id;

	uint32_t device_version;

	/* bitmap of ena_admin_aq_feature_id, which represents supported
	 * subcommands for the set/get feature admin commands.
	 */
	uint32_t supported_features;

	/* bitmap of ena_admin_aq_caps_id, which represents device
	 * capabilities.
	 */
	uint32_t capabilities;

	/* Indicates how many bits are used physical address access. */
	uint32_t phys_addr_width;

	/* Indicates how many bits are used virtual address access. */
	uint32_t virt_addr_width;

	/* unicast MAC address (in Network byte order) */
	uint8_t mac_addr[6];

	uint8_t reserved7[2];

	uint32_t max_mtu;
};

enum ena_admin_llq_header_location {
	/* header is in descriptor list */
	ENA_ADMIN_INLINE_HEADER                     = 1,
	/* header in a separate ring, implies 16B descriptor list entry */
	ENA_ADMIN_HEADER_RING                       = 2,
};

enum ena_admin_llq_ring_entry_size {
	ENA_ADMIN_LIST_ENTRY_SIZE_128B              = 1,
	ENA_ADMIN_LIST_ENTRY_SIZE_192B              = 2,
	ENA_ADMIN_LIST_ENTRY_SIZE_256B              = 4,
};

enum ena_admin_llq_num_descs_before_header {
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_0     = 0,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_1     = 1,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_2     = 2,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_4     = 4,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_8     = 8,
};

/* packet descriptor list entry always starts with one or more descriptors,
 * followed by a header. The rest of the descriptors are located in the
 * beginning of the subsequent entry. Stride refers to how the rest of the
 * descriptors are placed. This field is relevant only for inline header
 * mode
 */
enum ena_admin_llq_stride_ctrl {
	ENA_ADMIN_SINGLE_DESC_PER_ENTRY             = 1,
	ENA_ADMIN_MULTIPLE_DESCS_PER_ENTRY          = 2,
};

enum ena_admin_accel_mode_feat {
	ENA_ADMIN_DISABLE_META_CACHING              = 0,
	ENA_ADMIN_LIMIT_TX_BURST                    = 1,
};

struct ena_admin_accel_mode_get {
	/* bit field of enum ena_admin_accel_mode_feat */
	uint16_t supported_flags;

	/* maximum burst size between two doorbells. The size is in bytes */
	uint16_t max_tx_burst_size;
};

struct ena_admin_accel_mode_set {
	/* bit field of enum ena_admin_accel_mode_feat */
	uint16_t enabled_flags;

	uint16_t reserved;
};

struct ena_admin_accel_mode_req {
	union {
		uint32_t raw[2];

		struct ena_admin_accel_mode_get get;

		struct ena_admin_accel_mode_set set;
	} u;
};

struct ena_admin_feature_llq_desc {
	uint32_t max_llq_num;

	uint32_t max_llq_depth;

	/* specify the header locations the device supports. bitfield of enum
	 * ena_admin_llq_header_location.
	 */
	uint16_t header_location_ctrl_supported;

	/* the header location the driver selected to use. */
	uint16_t header_location_ctrl_enabled;

	/* if inline header is specified - this is the size of descriptor list
	 * entry. If header in a separate ring is specified - this is the size
	 * of header ring entry. bitfield of enum ena_admin_llq_ring_entry_size.
	 * specify the entry sizes the device supports
	 */
	uint16_t entry_size_ctrl_supported;

	/* the entry size the driver selected to use. */
	uint16_t entry_size_ctrl_enabled;

	/* valid only if inline header is specified. First entry associated with
	 * the packet includes descriptors and header. Rest of the entries
	 * occupied by descriptors. This parameter defines the max number of
	 * descriptors precedding the header in the first entry. The field is
	 * bitfield of enum ena_admin_llq_num_descs_before_header and specify
	 * the values the device supports
	 */
	uint16_t desc_num_before_header_supported;

	/* the desire field the driver selected to use */
	uint16_t desc_num_before_header_enabled;

	/* valid only if inline was chosen. bitfield of enum
	 * ena_admin_llq_stride_ctrl
	 */
	uint16_t descriptors_stride_ctrl_supported;

	/* the stride control the driver selected to use */
	uint16_t descriptors_stride_ctrl_enabled;

	/* feature version of device resp to either GET/SET commands. */
	uint8_t feature_version;

	/* llq entry size recommended by the device,
	 * values correlated to enum ena_admin_llq_ring_entry_size.
	 * used only for GET command.
	 */
	uint8_t entry_size_recommended;

	/* max depth of wide llq, or 0 for N/A */
	uint16_t max_wide_llq_depth;

	/* accelerated low latency queues requirement. driver needs to
	 * support those requirements in order to use accelerated llq
	 */
	struct ena_admin_accel_mode_req accel_mode;
};

struct ena_admin_queue_ext_feature_fields {
	uint32_t max_tx_sq_num;

	uint32_t max_tx_cq_num;

	uint32_t max_rx_sq_num;

	uint32_t max_rx_cq_num;

	uint32_t max_tx_sq_depth;

	uint32_t max_tx_cq_depth;

	uint32_t max_rx_sq_depth;

	uint32_t max_rx_cq_depth;

	uint32_t max_tx_header_size;

	/* Maximum Descriptors number, including meta descriptor, allowed for a
	 * single Tx packet
	 */
	uint16_t max_per_packet_tx_descs;

	/* Maximum Descriptors number allowed for a single Rx packet */
	uint16_t max_per_packet_rx_descs;
};

struct ena_admin_queue_feature_desc {
	uint32_t max_sq_num;

	uint32_t max_sq_depth;

	uint32_t max_cq_num;

	uint32_t max_cq_depth;

	uint32_t max_legacy_llq_num;

	uint32_t max_legacy_llq_depth;

	uint32_t max_header_size;

	/* Maximum Descriptors number, including meta descriptor, allowed for a
	 * single Tx packet
	 */
	uint16_t max_packet_tx_descs;

	/* Maximum Descriptors number allowed for a single Rx packet */
	uint16_t max_packet_rx_descs;
};

struct ena_admin_set_feature_mtu_desc {
	/* exclude L2 */
	uint32_t mtu;
};

struct ena_admin_get_extra_properties_strings_desc {
	uint32_t count;
};

struct ena_admin_get_extra_properties_flags_desc {
	uint32_t flags;
};

struct ena_admin_set_feature_host_attr_desc {
	/* host OS info base address in OS memory. host info is 4KB of
	 * physically contiguous
	 */
	struct ena_common_mem_addr os_info_ba;

	/* host debug area base address in OS memory. debug area must be
	 * physically contiguous
	 */
	struct ena_common_mem_addr debug_ba;

	/* debug area size */
	uint32_t debug_area_size;
};

struct ena_admin_feature_intr_moder_desc {
	/* interrupt delay granularity in usec */
	uint16_t intr_delay_resolution;

	uint16_t reserved;
};

struct ena_admin_get_feature_link_desc {
	/* Link speed in Mb */
	uint32_t speed;

	/* bit field of enum ena_admin_link types */
	uint32_t supported;

	/* 0 : autoneg
	 * 1 : duplex - Full Duplex
	 * 31:2 : reserved2
	 */
	uint32_t flags;
};

struct ena_admin_feature_aenq_desc {
	/* bitmask for AENQ groups the device can report */
	uint32_t supported_groups;

	/* bitmask for AENQ groups to report */
	uint32_t enabled_groups;
};

struct ena_admin_feature_offload_desc {
	/* 0 : TX_L3_csum_ipv4
	 * 1 : TX_L4_ipv4_csum_part - The checksum field
	 *    should be initialized with pseudo header checksum
	 * 2 : TX_L4_ipv4_csum_full
	 * 3 : TX_L4_ipv6_csum_part - The checksum field
	 *    should be initialized with pseudo header checksum
	 * 4 : TX_L4_ipv6_csum_full
	 * 5 : tso_ipv4
	 * 6 : tso_ipv6
	 * 7 : tso_ecn
	 */
	uint32_t tx;

	/* Receive side supported stateless offload
	 * 0 : RX_L3_csum_ipv4 - IPv4 checksum
	 * 1 : RX_L4_ipv4_csum - TCP/UDP/IPv4 checksum
	 * 2 : RX_L4_ipv6_csum - TCP/UDP/IPv6 checksum
	 * 3 : RX_hash - Hash calculation
	 */
	uint32_t rx_supported;

	uint32_t rx_enabled;
};

enum ena_admin_hash_functions {
	ENA_ADMIN_TOEPLITZ                          = 1,
	ENA_ADMIN_CRC32                             = 2,
};

struct ena_admin_feature_rss_flow_hash_control {
	uint32_t key_parts;

	uint32_t reserved;

	uint32_t key[ENA_ADMIN_RSS_KEY_PARTS];
};

struct ena_admin_feature_rss_flow_hash_function {
	/* 7:0 : funcs - bitmask of ena_admin_hash_functions */
	uint32_t supported_func;

	/* 7:0 : selected_func - bitmask of
	 *    ena_admin_hash_functions
	 */
	uint32_t selected_func;

	/* initial value */
	uint32_t init_val;
};

/* RSS flow hash protocols */
enum ena_admin_flow_hash_proto {
	ENA_ADMIN_RSS_TCP4                          = 0,
	ENA_ADMIN_RSS_UDP4                          = 1,
	ENA_ADMIN_RSS_TCP6                          = 2,
	ENA_ADMIN_RSS_UDP6                          = 3,
	ENA_ADMIN_RSS_IP4                           = 4,
	ENA_ADMIN_RSS_IP6                           = 5,
	ENA_ADMIN_RSS_IP4_FRAG                      = 6,
	ENA_ADMIN_RSS_NOT_IP                        = 7,
	/* TCPv6 with extension header */
	ENA_ADMIN_RSS_TCP6_EX                       = 8,
	/* IPv6 with extension header */
	ENA_ADMIN_RSS_IP6_EX                        = 9,
	ENA_ADMIN_RSS_PROTO_NUM                     = 16,
};

/* RSS flow hash fields */
enum ena_admin_flow_hash_fields {
	/* Ethernet Dest Addr */
	ENA_ADMIN_RSS_L2_DA                         = BIT(0),
	/* Ethernet Src Addr */
	ENA_ADMIN_RSS_L2_SA                         = BIT(1),
	/* ipv4/6 Dest Addr */
	ENA_ADMIN_RSS_L3_DA                         = BIT(2),
	/* ipv4/6 Src Addr */
	ENA_ADMIN_RSS_L3_SA                         = BIT(3),
	/* tcp/udp Dest Port */
	ENA_ADMIN_RSS_L4_DP                         = BIT(4),
	/* tcp/udp Src Port */
	ENA_ADMIN_RSS_L4_SP                         = BIT(5),
};

struct ena_admin_proto_input {
	/* flow hash fields (bitwise according to ena_admin_flow_hash_fields) */
	uint16_t fields;

	uint16_t reserved2;
};

struct ena_admin_feature_rss_hash_control {
	struct ena_admin_proto_input supported_fields[ENA_ADMIN_RSS_PROTO_NUM];

	struct ena_admin_proto_input selected_fields[ENA_ADMIN_RSS_PROTO_NUM];

	struct ena_admin_proto_input reserved2[ENA_ADMIN_RSS_PROTO_NUM];

	struct ena_admin_proto_input reserved3[ENA_ADMIN_RSS_PROTO_NUM];
};

struct ena_admin_feature_rss_flow_hash_input {
	/* supported hash input sorting
	 * 1 : L3_sort - support swap L3 addresses if DA is
	 *    smaller than SA
	 * 2 : L4_sort - support swap L4 ports if DP smaller
	 *    SP
	 */
	uint16_t supported_input_sort;

	/* enabled hash input sorting
	 * 1 : enable_L3_sort - enable swap L3 addresses if
	 *    DA smaller than SA
	 * 2 : enable_L4_sort - enable swap L4 ports if DP
	 *    smaller than SP
	 */
	uint16_t enabled_input_sort;
};

struct ena_admin_host_info {
	/* Host OS type defined as ENA_ADMIN_OS_* */
	uint32_t os_type;

	/* os distribution string format */
	uint8_t os_dist_str[128];

	/* OS distribution numeric format */
	uint32_t os_dist;

	/* kernel version string format */
	uint8_t kernel_ver_str[32];

	/* Kernel version numeric format */
	uint32_t kernel_ver;

	/* 7:0 : major
	 * 15:8 : minor
	 * 23:16 : sub_minor
	 * 31:24 : module_type
	 */
	uint32_t driver_version;

	/* features bitmap */
	uint32_t supported_network_features[2];

	/* ENA spec version of driver */
	uint16_t ena_spec_version;

	/* ENA device's Bus, Device and Function
	 * 2:0 : function
	 * 7:3 : device
	 * 15:8 : bus
	 */
	uint16_t bdf;

	/* Number of CPUs */
	uint16_t num_cpus;

	uint16_t reserved;

	/* 0 : reserved
	 * 1 : rx_offset
	 * 2 : interrupt_moderation
	 * 3 : rx_buf_mirroring
	 * 4 : rss_configurable_function_key
	 * 5 : reserved
	 * 6 : rx_page_reuse
	 * 7 : tx_ipv6_csum_offload
	 * 8 : phc
	 * 31:9 : reserved
	 */
	uint32_t driver_supported_features;
};

struct ena_admin_rss_ind_table_entry {
	uint16_t cq_idx;

	uint16_t reserved;
};

struct ena_admin_feature_rss_ind_table {
	/* min supported table size (2^min_size) */
	uint16_t min_size;

	/* max supported table size (2^max_size) */
	uint16_t max_size;

	/* table size (2^size) */
	uint16_t size;

	/* 0 : one_entry_update - The ENA device supports
	 *    setting a single RSS table entry
	 */
	uint8_t flags;

	uint8_t reserved;

	/* index of the inline entry. 0xFFFFFFFF means invalid */
	uint32_t inline_index;

	/* used for updating single entry, ignored when setting the entire
	 * table through the control buffer.
	 */
	struct ena_admin_rss_ind_table_entry inline_entry;
};

/* When hint value is 0, driver should use it's own predefined value */
struct ena_admin_ena_hw_hints {
	/* value in ms */
	uint16_t mmio_read_timeout;

	/* value in ms */
	uint16_t driver_watchdog_timeout;

	/* Per packet tx completion timeout. value in ms */
	uint16_t missing_tx_completion_timeout;

	uint16_t missed_tx_completion_count_threshold_to_reset;

	/* value in ms */
	uint16_t admin_completion_tx_timeout;

	uint16_t netdev_wd_timeout;

	uint16_t max_tx_sgl_size;

	uint16_t max_rx_sgl_size;

	uint16_t reserved[8];
};

struct ena_admin_get_feat_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	struct ena_admin_ctrl_buff_info control_buffer;

	struct ena_admin_get_set_feature_common_desc feat_common;

	uint32_t raw[11];
};

struct ena_admin_queue_ext_feature_desc {
	/* version */
	uint8_t version;

	uint8_t reserved1[3];

	union {
		struct ena_admin_queue_ext_feature_fields max_queue_ext;

		uint32_t raw[10];
	};
};

struct ena_admin_feature_phc_desc {
	/* PHC version as defined in enum ena_admin_phc_feature_version,
	 * used only for GET command as max supported PHC version by the device.
	 */
	uint8_t version;

	/* Reserved - MBZ */
	uint8_t reserved1[3];

	/* PHC doorbell address as an offset to PCIe MMIO REG BAR,
	 * used only for GET command.
	 */
	uint32_t doorbell_offset;

	/* Max time for valid PHC retrieval, passing this threshold will
	 * fail the get-time request and block PHC requests for
	 * block_timeout_usec, used only for GET command.
	 */
	uint32_t expire_timeout_usec;

	/* PHC requests block period, blocking starts if PHC request expired
	 * in order to prevent floods on busy device,
	 * used only for GET command.
	 */
	uint32_t block_timeout_usec;

	/* Shared PHC physical address (ena_admin_phc_resp),
	 * used only for SET command.
	 */
	struct ena_common_mem_addr output_address;

	/* Shared PHC Size (ena_admin_phc_resp),
	 * used only for SET command.
	 */
	uint32_t output_length;
};

struct ena_admin_get_feat_resp {
	struct ena_admin_acq_common_desc acq_common_desc;

	union {
		uint32_t raw[14];

		struct ena_admin_device_attr_feature_desc dev_attr;

		struct ena_admin_feature_llq_desc llq;

		struct ena_admin_queue_feature_desc max_queue;

		struct ena_admin_queue_ext_feature_desc max_queue_ext;

		struct ena_admin_feature_aenq_desc aenq;

		struct ena_admin_get_feature_link_desc link;

		struct ena_admin_feature_offload_desc offload;

		struct ena_admin_feature_rss_flow_hash_function flow_hash_func;

		struct ena_admin_feature_rss_flow_hash_input flow_hash_input;

		struct ena_admin_feature_rss_ind_table ind_table;

		struct ena_admin_feature_intr_moder_desc intr_moderation;

		struct ena_admin_ena_hw_hints hw_hints;

		struct ena_admin_feature_phc_desc phc;

		struct ena_admin_get_extra_properties_strings_desc extra_properties_strings;

		struct ena_admin_get_extra_properties_flags_desc extra_properties_flags;
	} u;
};

struct ena_admin_set_feat_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;

	struct ena_admin_ctrl_buff_info control_buffer;

	struct ena_admin_get_set_feature_common_desc feat_common;

	union {
		uint32_t raw[11];

		/* mtu size */
		struct ena_admin_set_feature_mtu_desc mtu;

		/* host attributes */
		struct ena_admin_set_feature_host_attr_desc host_attr;

		/* AENQ configuration */
		struct ena_admin_feature_aenq_desc aenq;

		/* rss flow hash function */
		struct ena_admin_feature_rss_flow_hash_function flow_hash_func;

		/* rss flow hash input */
		struct ena_admin_feature_rss_flow_hash_input flow_hash_input;

		/* rss indirection table */
		struct ena_admin_feature_rss_ind_table ind_table;

		/* LLQ configuration */
		struct ena_admin_feature_llq_desc llq;

		/* PHC configuration */
		struct ena_admin_feature_phc_desc phc;
	} u;
};

struct ena_admin_set_feat_resp {
	struct ena_admin_acq_common_desc acq_common_desc;

	union {
		uint32_t raw[14];
	} u;
};

struct ena_admin_aenq_common_desc {
	uint16_t group;

	uint16_t syndrome;

	/* 0 : phase
	 * 7:1 : reserved - MBZ
	 */
	uint8_t flags;

	uint8_t reserved1[3];

	uint32_t timestamp_low;

	uint32_t timestamp_high;
};

/* asynchronous event notification groups */
enum ena_admin_aenq_group {
	ENA_ADMIN_LINK_CHANGE                       = 0,
	ENA_ADMIN_FATAL_ERROR                       = 1,
	ENA_ADMIN_WARNING                           = 2,
	ENA_ADMIN_NOTIFICATION                      = 3,
	ENA_ADMIN_KEEP_ALIVE                        = 4,
	ENA_ADMIN_REFRESH_CAPABILITIES              = 5,
	ENA_ADMIN_CONF_NOTIFICATIONS		    = 6,
	ENA_ADMIN_DEVICE_REQUEST_RESET              = 7,
	ENA_ADMIN_AENQ_GROUPS_NUM                   = 8,
};

enum ena_admin_aenq_notification_syndrome {
	ENA_ADMIN_UPDATE_HINTS                      = 2,
};

struct ena_admin_aenq_entry {
	struct ena_admin_aenq_common_desc aenq_common_desc;

	/* command specific inline data */
	uint32_t inline_data_w4[12];
};

struct ena_admin_aenq_link_change_desc {
	struct ena_admin_aenq_common_desc aenq_common_desc;

	/* 0 : link_status */
	uint32_t flags;
};

struct ena_admin_aenq_keep_alive_desc {
	struct ena_admin_aenq_common_desc aenq_common_desc;

	uint32_t rx_drops_low;

	uint32_t rx_drops_high;

	uint32_t tx_drops_low;

	uint32_t tx_drops_high;

	uint32_t rx_overruns_low;

	uint32_t rx_overruns_high;
};

struct ena_admin_aenq_conf_notifications_desc {
	struct ena_admin_aenq_common_desc aenq_common_desc;

	uint64_t notifications_bitmap;

	uint64_t reserved;
};

struct ena_admin_ena_mmio_req_read_less_resp {
	uint16_t req_id;

	uint16_t reg_off;

	/* value is valid when poll is cleared */
	uint32_t reg_val;
};

struct ena_admin_phc_resp {
	/* Request Id, received from DB register */
	uint16_t req_id;

	uint8_t reserved1[6];

	/* PHC timestamp (nsec) */
	uint64_t timestamp;

	uint8_t reserved2[8];

	/* Timestamp error limit (nsec) */
	uint32_t error_bound;

	/* Bit field of enum ena_admin_phc_error_flags */
	uint32_t error_flags;

	uint8_t reserved3[32];
};

/* aq_common_desc */
#define ENA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK            GENMASK(11, 0)
#define ENA_ADMIN_AQ_COMMON_DESC_PHASE_MASK                 BIT(0)
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_SHIFT            1
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK             BIT(1)
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_SHIFT   2
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK    BIT(2)

/* sq */
#define ENA_ADMIN_SQ_SQ_DIRECTION_SHIFT                     5
#define ENA_ADMIN_SQ_SQ_DIRECTION_MASK                      GENMASK(7, 5)

/* acq_common_desc */
#define ENA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK           GENMASK(11, 0)
#define ENA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK                BIT(0)

/* aq_create_sq_cmd */
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_SHIFT       5
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_MASK        GENMASK(7, 5)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_PLACEMENT_POLICY_MASK    GENMASK(3, 0)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_SHIFT  4
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_MASK   GENMASK(6, 4)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_IS_PHYSICALLY_CONTIGUOUS_MASK BIT(0)

/* aq_create_cq_cmd */
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_SHIFT 5
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK BIT(5)
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK GENMASK(4, 0)

/* get_set_feature_common_desc */
#define ENA_ADMIN_GET_SET_FEATURE_COMMON_DESC_SELECT_MASK   GENMASK(1, 0)

/* get_feature_link_desc */
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_AUTONEG_MASK        BIT(0)
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_SHIFT        1
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_MASK         BIT(1)

/* feature_offload_desc */
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK BIT(0)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_SHIFT 1
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK BIT(1)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_SHIFT 2
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK BIT(2)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_SHIFT 3
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK BIT(3)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_SHIFT 4
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK BIT(4)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_SHIFT       5
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK        BIT(5)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_SHIFT       6
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK        BIT(6)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_SHIFT        7
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_MASK         BIT(7)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK BIT(0)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_SHIFT 1
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK BIT(1)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_SHIFT 2
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK BIT(2)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_SHIFT        3
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK         BIT(3)

/* feature_rss_flow_hash_function */
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_FUNCS_MASK GENMASK(7, 0)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_SELECTED_FUNC_MASK GENMASK(7, 0)

/* feature_rss_flow_hash_input */
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_SHIFT 1
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_MASK  BIT(1)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_SHIFT 2
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_MASK  BIT(2)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_SHIFT 1
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_MASK BIT(1)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_SHIFT 2
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_MASK BIT(2)

/* host_info */
#define ENA_ADMIN_HOST_INFO_MAJOR_MASK                      GENMASK(7, 0)
#define ENA_ADMIN_HOST_INFO_MINOR_SHIFT                     8
#define ENA_ADMIN_HOST_INFO_MINOR_MASK                      GENMASK(15, 8)
#define ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT                 16
#define ENA_ADMIN_HOST_INFO_SUB_MINOR_MASK                  GENMASK(23, 16)
#define ENA_ADMIN_HOST_INFO_MODULE_TYPE_SHIFT               24
#define ENA_ADMIN_HOST_INFO_MODULE_TYPE_MASK                GENMASK(31, 24)
#define ENA_ADMIN_HOST_INFO_FUNCTION_MASK                   GENMASK(2, 0)
#define ENA_ADMIN_HOST_INFO_DEVICE_SHIFT                    3
#define ENA_ADMIN_HOST_INFO_DEVICE_MASK                     GENMASK(7, 3)
#define ENA_ADMIN_HOST_INFO_BUS_SHIFT                       8
#define ENA_ADMIN_HOST_INFO_BUS_MASK                        GENMASK(15, 8)
#define ENA_ADMIN_HOST_INFO_RX_OFFSET_SHIFT                 1
#define ENA_ADMIN_HOST_INFO_RX_OFFSET_MASK                  BIT(1)
#define ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_SHIFT      2
#define ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_MASK       BIT(2)
#define ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_SHIFT          3
#define ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_MASK           BIT(3)
#define ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_SHIFT 4
#define ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_MASK BIT(4)
#define ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_SHIFT             6
#define ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_MASK              BIT(6)
#define ENA_ADMIN_HOST_INFO_TX_IPV6_CSUM_OFFLOAD_SHIFT      7
#define ENA_ADMIN_HOST_INFO_TX_IPV6_CSUM_OFFLOAD_MASK       BIT(7)
#define ENA_ADMIN_HOST_INFO_PHC_SHIFT                       8
#define ENA_ADMIN_HOST_INFO_PHC_MASK                        BIT(8)

/* feature_rss_ind_table */
#define ENA_ADMIN_FEATURE_RSS_IND_TABLE_ONE_ENTRY_UPDATE_MASK BIT(0)

/* aenq_common_desc */
#define ENA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK               BIT(0)

/* aenq_link_change_desc */
#define ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK    BIT(0)

#if !defined(DEFS_LINUX_MAINLINE)
static inline uint16_t get_ena_admin_aq_common_desc_command_id(const struct ena_admin_aq_common_desc *p)
{
	return p->command_id & ENA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline void set_ena_admin_aq_common_desc_command_id(struct ena_admin_aq_common_desc *p, uint16_t val)
{
	p->command_id |= val & ENA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline uint8_t get_ena_admin_aq_common_desc_phase(const struct ena_admin_aq_common_desc *p)
{
	return p->flags & ENA_ADMIN_AQ_COMMON_DESC_PHASE_MASK;
}

static inline void set_ena_admin_aq_common_desc_phase(struct ena_admin_aq_common_desc *p, uint8_t val)
{
	p->flags |= val & ENA_ADMIN_AQ_COMMON_DESC_PHASE_MASK;
}

static inline uint8_t get_ena_admin_aq_common_desc_ctrl_data(const struct ena_admin_aq_common_desc *p)
{
	return (p->flags & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK) >> ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_SHIFT;
}

static inline void set_ena_admin_aq_common_desc_ctrl_data(struct ena_admin_aq_common_desc *p, uint8_t val)
{
	p->flags |= (val << ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_SHIFT) & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK;
}

static inline uint8_t get_ena_admin_aq_common_desc_ctrl_data_indirect(const struct ena_admin_aq_common_desc *p)
{
	return (p->flags & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK) >> ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_SHIFT;
}

static inline void set_ena_admin_aq_common_desc_ctrl_data_indirect(struct ena_admin_aq_common_desc *p, uint8_t val)
{
	p->flags |= (val << ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_SHIFT) & ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK;
}

static inline uint8_t get_ena_admin_sq_sq_direction(const struct ena_admin_sq *p)
{
	return (p->sq_identity & ENA_ADMIN_SQ_SQ_DIRECTION_MASK) >> ENA_ADMIN_SQ_SQ_DIRECTION_SHIFT;
}

static inline void set_ena_admin_sq_sq_direction(struct ena_admin_sq *p, uint8_t val)
{
	p->sq_identity |= (val << ENA_ADMIN_SQ_SQ_DIRECTION_SHIFT) & ENA_ADMIN_SQ_SQ_DIRECTION_MASK;
}

static inline uint16_t get_ena_admin_acq_common_desc_command_id(const struct ena_admin_acq_common_desc *p)
{
	return p->command & ENA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline void set_ena_admin_acq_common_desc_command_id(struct ena_admin_acq_common_desc *p, uint16_t val)
{
	p->command |= val & ENA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK;
}

static inline uint8_t get_ena_admin_acq_common_desc_phase(const struct ena_admin_acq_common_desc *p)
{
	return p->flags & ENA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK;
}

static inline void set_ena_admin_acq_common_desc_phase(struct ena_admin_acq_common_desc *p, uint8_t val)
{
	p->flags |= val & ENA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK;
}

static inline uint8_t get_ena_admin_aq_create_sq_cmd_sq_direction(const struct ena_admin_aq_create_sq_cmd *p)
{
	return (p->sq_identity & ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_MASK) >> ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_SHIFT;
}

static inline void set_ena_admin_aq_create_sq_cmd_sq_direction(struct ena_admin_aq_create_sq_cmd *p, uint8_t val)
{
	p->sq_identity |= (val << ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_SHIFT) & ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_MASK;
}

static inline uint8_t get_ena_admin_aq_create_sq_cmd_placement_policy(const struct ena_admin_aq_create_sq_cmd *p)
{
	return p->sq_caps_2 & ENA_ADMIN_AQ_CREATE_SQ_CMD_PLACEMENT_POLICY_MASK;
}

static inline void set_ena_admin_aq_create_sq_cmd_placement_policy(struct ena_admin_aq_create_sq_cmd *p, uint8_t val)
{
	p->sq_caps_2 |= val & ENA_ADMIN_AQ_CREATE_SQ_CMD_PLACEMENT_POLICY_MASK;
}

static inline uint8_t get_ena_admin_aq_create_sq_cmd_completion_policy(const struct ena_admin_aq_create_sq_cmd *p)
{
	return (p->sq_caps_2 & ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_MASK) >> ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_SHIFT;
}

static inline void set_ena_admin_aq_create_sq_cmd_completion_policy(struct ena_admin_aq_create_sq_cmd *p, uint8_t val)
{
	p->sq_caps_2 |= (val << ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_SHIFT) & ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_MASK;
}

static inline uint8_t get_ena_admin_aq_create_sq_cmd_is_physically_contiguous(const struct ena_admin_aq_create_sq_cmd *p)
{
	return p->sq_caps_3 & ENA_ADMIN_AQ_CREATE_SQ_CMD_IS_PHYSICALLY_CONTIGUOUS_MASK;
}

static inline void set_ena_admin_aq_create_sq_cmd_is_physically_contiguous(struct ena_admin_aq_create_sq_cmd *p, uint8_t val)
{
	p->sq_caps_3 |= val & ENA_ADMIN_AQ_CREATE_SQ_CMD_IS_PHYSICALLY_CONTIGUOUS_MASK;
}

static inline uint8_t get_ena_admin_aq_create_cq_cmd_interrupt_mode_enabled(const struct ena_admin_aq_create_cq_cmd *p)
{
	return (p->cq_caps_1 & ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK) >> ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_SHIFT;
}

static inline void set_ena_admin_aq_create_cq_cmd_interrupt_mode_enabled(struct ena_admin_aq_create_cq_cmd *p, uint8_t val)
{
	p->cq_caps_1 |= (val << ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_SHIFT) & ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK;
}

static inline uint8_t get_ena_admin_aq_create_cq_cmd_cq_entry_size_words(const struct ena_admin_aq_create_cq_cmd *p)
{
	return p->cq_caps_2 & ENA_ADMIN_AQ_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK;
}

static inline void set_ena_admin_aq_create_cq_cmd_cq_entry_size_words(struct ena_admin_aq_create_cq_cmd *p, uint8_t val)
{
	p->cq_caps_2 |= val & ENA_ADMIN_AQ_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK;
}

static inline uint8_t get_ena_admin_get_set_feature_common_desc_select(const struct ena_admin_get_set_feature_common_desc *p)
{
	return p->flags & ENA_ADMIN_GET_SET_FEATURE_COMMON_DESC_SELECT_MASK;
}

static inline void set_ena_admin_get_set_feature_common_desc_select(struct ena_admin_get_set_feature_common_desc *p, uint8_t val)
{
	p->flags |= val & ENA_ADMIN_GET_SET_FEATURE_COMMON_DESC_SELECT_MASK;
}

static inline uint32_t get_ena_admin_get_feature_link_desc_autoneg(const struct ena_admin_get_feature_link_desc *p)
{
	return p->flags & ENA_ADMIN_GET_FEATURE_LINK_DESC_AUTONEG_MASK;
}

static inline void set_ena_admin_get_feature_link_desc_autoneg(struct ena_admin_get_feature_link_desc *p, uint32_t val)
{
	p->flags |= val & ENA_ADMIN_GET_FEATURE_LINK_DESC_AUTONEG_MASK;
}

static inline uint32_t get_ena_admin_get_feature_link_desc_duplex(const struct ena_admin_get_feature_link_desc *p)
{
	return (p->flags & ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_MASK) >> ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_SHIFT;
}

static inline void set_ena_admin_get_feature_link_desc_duplex(struct ena_admin_get_feature_link_desc *p, uint32_t val)
{
	p->flags |= (val << ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_SHIFT) & ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_TX_L3_csum_ipv4(const struct ena_admin_feature_offload_desc *p)
{
	return p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK;
}

static inline void set_ena_admin_feature_offload_desc_TX_L3_csum_ipv4(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= val & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_part(const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_part(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_full(const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_TX_L4_ipv4_csum_full(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_part(const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_part(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_full(const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_TX_L4_ipv6_csum_full(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_tso_ipv4(const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_tso_ipv4(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_tso_ipv6(const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_tso_ipv6(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_tso_ecn(const struct ena_admin_feature_offload_desc *p)
{
	return (p->tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_tso_ecn(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->tx |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_RX_L3_csum_ipv4(const struct ena_admin_feature_offload_desc *p)
{
	return p->rx_supported & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK;
}

static inline void set_ena_admin_feature_offload_desc_RX_L3_csum_ipv4(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->rx_supported |= val & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_RX_L4_ipv4_csum(const struct ena_admin_feature_offload_desc *p)
{
	return (p->rx_supported & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_RX_L4_ipv4_csum(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->rx_supported |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_RX_L4_ipv6_csum(const struct ena_admin_feature_offload_desc *p)
{
	return (p->rx_supported & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_RX_L4_ipv6_csum(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->rx_supported |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK;
}

static inline uint32_t get_ena_admin_feature_offload_desc_RX_hash(const struct ena_admin_feature_offload_desc *p)
{
	return (p->rx_supported & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK) >> ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_SHIFT;
}

static inline void set_ena_admin_feature_offload_desc_RX_hash(struct ena_admin_feature_offload_desc *p, uint32_t val)
{
	p->rx_supported |= (val << ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_SHIFT) & ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK;
}

static inline uint32_t get_ena_admin_feature_rss_flow_hash_function_funcs(const struct ena_admin_feature_rss_flow_hash_function *p)
{
	return p->supported_func & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_FUNCS_MASK;
}

static inline void set_ena_admin_feature_rss_flow_hash_function_funcs(struct ena_admin_feature_rss_flow_hash_function *p, uint32_t val)
{
	p->supported_func |= val & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_FUNCS_MASK;
}

static inline uint32_t get_ena_admin_feature_rss_flow_hash_function_selected_func(const struct ena_admin_feature_rss_flow_hash_function *p)
{
	return p->selected_func & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_SELECTED_FUNC_MASK;
}

static inline void set_ena_admin_feature_rss_flow_hash_function_selected_func(struct ena_admin_feature_rss_flow_hash_function *p, uint32_t val)
{
	p->selected_func |= val & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_SELECTED_FUNC_MASK;
}

static inline uint16_t get_ena_admin_feature_rss_flow_hash_input_L3_sort(const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->supported_input_sort & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_MASK) >> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_SHIFT;
}

static inline void set_ena_admin_feature_rss_flow_hash_input_L3_sort(struct ena_admin_feature_rss_flow_hash_input *p, uint16_t val)
{
	p->supported_input_sort |= (val << ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_SHIFT) & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_MASK;
}

static inline uint16_t get_ena_admin_feature_rss_flow_hash_input_L4_sort(const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->supported_input_sort & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_MASK) >> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_SHIFT;
}

static inline void set_ena_admin_feature_rss_flow_hash_input_L4_sort(struct ena_admin_feature_rss_flow_hash_input *p, uint16_t val)
{
	p->supported_input_sort |= (val << ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_SHIFT) & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_MASK;
}

static inline uint16_t get_ena_admin_feature_rss_flow_hash_input_enable_L3_sort(const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->enabled_input_sort & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_MASK) >> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_SHIFT;
}

static inline void set_ena_admin_feature_rss_flow_hash_input_enable_L3_sort(struct ena_admin_feature_rss_flow_hash_input *p, uint16_t val)
{
	p->enabled_input_sort |= (val << ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_SHIFT) & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_MASK;
}

static inline uint16_t get_ena_admin_feature_rss_flow_hash_input_enable_L4_sort(const struct ena_admin_feature_rss_flow_hash_input *p)
{
	return (p->enabled_input_sort & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_MASK) >> ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_SHIFT;
}

static inline void set_ena_admin_feature_rss_flow_hash_input_enable_L4_sort(struct ena_admin_feature_rss_flow_hash_input *p, uint16_t val)
{
	p->enabled_input_sort |= (val << ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_SHIFT) & ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_MASK;
}

static inline uint32_t get_ena_admin_host_info_major(const struct ena_admin_host_info *p)
{
	return p->driver_version & ENA_ADMIN_HOST_INFO_MAJOR_MASK;
}

static inline void set_ena_admin_host_info_major(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_version |= val & ENA_ADMIN_HOST_INFO_MAJOR_MASK;
}

static inline uint32_t get_ena_admin_host_info_minor(const struct ena_admin_host_info *p)
{
	return (p->driver_version & ENA_ADMIN_HOST_INFO_MINOR_MASK) >> ENA_ADMIN_HOST_INFO_MINOR_SHIFT;
}

static inline void set_ena_admin_host_info_minor(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_version |= (val << ENA_ADMIN_HOST_INFO_MINOR_SHIFT) & ENA_ADMIN_HOST_INFO_MINOR_MASK;
}

static inline uint32_t get_ena_admin_host_info_sub_minor(const struct ena_admin_host_info *p)
{
	return (p->driver_version & ENA_ADMIN_HOST_INFO_SUB_MINOR_MASK) >> ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT;
}

static inline void set_ena_admin_host_info_sub_minor(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_version |= (val << ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT) & ENA_ADMIN_HOST_INFO_SUB_MINOR_MASK;
}

static inline uint32_t get_ena_admin_host_info_module_type(const struct ena_admin_host_info *p)
{
	return (p->driver_version & ENA_ADMIN_HOST_INFO_MODULE_TYPE_MASK) >> ENA_ADMIN_HOST_INFO_MODULE_TYPE_SHIFT;
}

static inline void set_ena_admin_host_info_module_type(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_version |= (val << ENA_ADMIN_HOST_INFO_MODULE_TYPE_SHIFT) & ENA_ADMIN_HOST_INFO_MODULE_TYPE_MASK;
}

static inline uint16_t get_ena_admin_host_info_function(const struct ena_admin_host_info *p)
{
	return p->bdf & ENA_ADMIN_HOST_INFO_FUNCTION_MASK;
}

static inline void set_ena_admin_host_info_function(struct ena_admin_host_info *p, uint16_t val)
{
	p->bdf |= val & ENA_ADMIN_HOST_INFO_FUNCTION_MASK;
}

static inline uint16_t get_ena_admin_host_info_device(const struct ena_admin_host_info *p)
{
	return (p->bdf & ENA_ADMIN_HOST_INFO_DEVICE_MASK) >> ENA_ADMIN_HOST_INFO_DEVICE_SHIFT;
}

static inline void set_ena_admin_host_info_device(struct ena_admin_host_info *p, uint16_t val)
{
	p->bdf |= (val << ENA_ADMIN_HOST_INFO_DEVICE_SHIFT) & ENA_ADMIN_HOST_INFO_DEVICE_MASK;
}

static inline uint16_t get_ena_admin_host_info_bus(const struct ena_admin_host_info *p)
{
	return (p->bdf & ENA_ADMIN_HOST_INFO_BUS_MASK) >> ENA_ADMIN_HOST_INFO_BUS_SHIFT;
}

static inline void set_ena_admin_host_info_bus(struct ena_admin_host_info *p, uint16_t val)
{
	p->bdf |= (val << ENA_ADMIN_HOST_INFO_BUS_SHIFT) & ENA_ADMIN_HOST_INFO_BUS_MASK;
}

static inline uint32_t get_ena_admin_host_info_rx_offset(const struct ena_admin_host_info *p)
{
	return (p->driver_supported_features & ENA_ADMIN_HOST_INFO_RX_OFFSET_MASK) >> ENA_ADMIN_HOST_INFO_RX_OFFSET_SHIFT;
}

static inline void set_ena_admin_host_info_rx_offset(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_supported_features |= (val << ENA_ADMIN_HOST_INFO_RX_OFFSET_SHIFT) & ENA_ADMIN_HOST_INFO_RX_OFFSET_MASK;
}

static inline uint32_t get_ena_admin_host_info_interrupt_moderation(const struct ena_admin_host_info *p)
{
	return (p->driver_supported_features & ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_MASK) >> ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_SHIFT;
}

static inline void set_ena_admin_host_info_interrupt_moderation(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_supported_features |= (val << ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_SHIFT) & ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_MASK;
}

static inline uint32_t get_ena_admin_host_info_rx_buf_mirroring(const struct ena_admin_host_info *p)
{
	return (p->driver_supported_features & ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_MASK) >> ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_SHIFT;
}

static inline void set_ena_admin_host_info_rx_buf_mirroring(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_supported_features |= (val << ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_SHIFT) & ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_MASK;
}

static inline uint32_t get_ena_admin_host_info_rss_configurable_function_key(const struct ena_admin_host_info *p)
{
	return (p->driver_supported_features & ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_MASK) >> ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_SHIFT;
}

static inline void set_ena_admin_host_info_rss_configurable_function_key(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_supported_features |= (val << ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_SHIFT) & ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_MASK;
}

static inline uint32_t get_ena_admin_host_info_rx_page_reuse(const struct ena_admin_host_info *p)
{
	return (p->driver_supported_features & ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_MASK) >> ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_SHIFT;
}

static inline void set_ena_admin_host_info_rx_page_reuse(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_supported_features |= (val << ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_SHIFT) & ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_MASK;
}

static inline uint32_t get_ena_admin_host_info_tx_ipv6_csum_offload(const struct ena_admin_host_info *p)
{
	return (p->driver_supported_features & ENA_ADMIN_HOST_INFO_TX_IPV6_CSUM_OFFLOAD_MASK) >> ENA_ADMIN_HOST_INFO_TX_IPV6_CSUM_OFFLOAD_SHIFT;
}

static inline void set_ena_admin_host_info_tx_ipv6_csum_offload(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_supported_features |= (val << ENA_ADMIN_HOST_INFO_TX_IPV6_CSUM_OFFLOAD_SHIFT) & ENA_ADMIN_HOST_INFO_TX_IPV6_CSUM_OFFLOAD_MASK;
}

static inline uint8_t get_ena_admin_feature_rss_ind_table_one_entry_update(const struct ena_admin_feature_rss_ind_table *p)
{
	return p->flags & ENA_ADMIN_FEATURE_RSS_IND_TABLE_ONE_ENTRY_UPDATE_MASK;
}

static inline void set_ena_admin_feature_rss_ind_table_one_entry_update(struct ena_admin_feature_rss_ind_table *p, uint8_t val)
{
	p->flags |= val & ENA_ADMIN_FEATURE_RSS_IND_TABLE_ONE_ENTRY_UPDATE_MASK;
}

static inline uint32_t get_ena_admin_host_info_phc(const struct ena_admin_host_info *p)
{
	return (p->driver_supported_features & ENA_ADMIN_HOST_INFO_PHC_MASK) >> ENA_ADMIN_HOST_INFO_PHC_SHIFT;
}

static inline void set_ena_admin_host_info_phc(struct ena_admin_host_info *p, uint32_t val)
{
	p->driver_supported_features |= (val << ENA_ADMIN_HOST_INFO_PHC_SHIFT) & ENA_ADMIN_HOST_INFO_PHC_MASK;
}

static inline uint8_t get_ena_admin_aenq_common_desc_phase(const struct ena_admin_aenq_common_desc *p)
{
	return p->flags & ENA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK;
}

static inline void set_ena_admin_aenq_common_desc_phase(struct ena_admin_aenq_common_desc *p, uint8_t val)
{
	p->flags |= val & ENA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK;
}

static inline uint32_t get_ena_admin_aenq_link_change_desc_link_status(const struct ena_admin_aenq_link_change_desc *p)
{
	return p->flags & ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK;
}

static inline void set_ena_admin_aenq_link_change_desc_link_status(struct ena_admin_aenq_link_change_desc *p, uint32_t val)
{
	p->flags |= val & ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK;
}

#endif /* !defined(DEFS_LINUX_MAINLINE) */
#endif /* _ENA_ADMIN_H_ */
