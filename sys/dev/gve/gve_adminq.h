/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _GVE_AQ_H_
#define _GVE_AQ_H_ 1

#include <sys/types.h>
#include <net/if.h>
#include <net/iflib.h>
#include <machine/bus.h>
#include <machine/resource.h>

/* Admin queue opcodes */
enum gve_adminq_opcodes {
	GVE_ADMINQ_DESCRIBE_DEVICE		= 0x1,
	GVE_ADMINQ_CONFIGURE_DEVICE_RESOURCES	= 0x2,
	GVE_ADMINQ_REGISTER_PAGE_LIST		= 0x3,
	GVE_ADMINQ_UNREGISTER_PAGE_LIST		= 0x4,
	GVE_ADMINQ_CREATE_TX_QUEUE		= 0x5,
	GVE_ADMINQ_CREATE_RX_QUEUE		= 0x6,
	GVE_ADMINQ_DESTROY_TX_QUEUE		= 0x7,
	GVE_ADMINQ_DESTROY_RX_QUEUE		= 0x8,
	GVE_ADMINQ_DECONFIGURE_DEVICE_RESOURCES	= 0x9,
	GVE_ADMINQ_SET_DRIVER_PARAMETER		= 0xB,
	GVE_ADMINQ_REPORT_STATS			= 0xC,
	GVE_ADMINQ_REPORT_LINK_SPEED		= 0xD,
	GVE_ADMINQ_GET_PTYPE_MAP		= 0xE,
	GVE_ADMINQ_VERIFY_DRIVER_COMPATIBILITY	= 0xF,
};

/* Admin queue status codes */
enum gve_adminq_statuses {
	GVE_ADMINQ_COMMAND_UNSET			= 0x0,
	GVE_ADMINQ_COMMAND_PASSED			= 0x1,
	GVE_ADMINQ_COMMAND_ERROR_ABORTED		= 0xFFFFFFF0,
	GVE_ADMINQ_COMMAND_ERROR_ALREADY_EXISTS		= 0xFFFFFFF1,
	GVE_ADMINQ_COMMAND_ERROR_CANCELLED		= 0xFFFFFFF2,
	GVE_ADMINQ_COMMAND_ERROR_DATALOSS		= 0xFFFFFFF3,
	GVE_ADMINQ_COMMAND_ERROR_DEADLINE_EXCEEDED	= 0xFFFFFFF4,
	GVE_ADMINQ_COMMAND_ERROR_FAILED_PRECONDITION	= 0xFFFFFFF5,
	GVE_ADMINQ_COMMAND_ERROR_INTERNAL_ERROR		= 0xFFFFFFF6,
	GVE_ADMINQ_COMMAND_ERROR_INVALID_ARGUMENT	= 0xFFFFFFF7,
	GVE_ADMINQ_COMMAND_ERROR_NOT_FOUND		= 0xFFFFFFF8,
	GVE_ADMINQ_COMMAND_ERROR_OUT_OF_RANGE		= 0xFFFFFFF9,
	GVE_ADMINQ_COMMAND_ERROR_PERMISSION_DENIED	= 0xFFFFFFFA,
	GVE_ADMINQ_COMMAND_ERROR_UNAUTHENTICATED	= 0xFFFFFFFB,
	GVE_ADMINQ_COMMAND_ERROR_RESOURCE_EXHAUSTED	= 0xFFFFFFFC,
	GVE_ADMINQ_COMMAND_ERROR_UNAVAILABLE		= 0xFFFFFFFD,
	GVE_ADMINQ_COMMAND_ERROR_UNIMPLEMENTED		= 0xFFFFFFFE,
	GVE_ADMINQ_COMMAND_ERROR_UNKNOWN_ERROR		= 0xFFFFFFFF,
};

#define GVE_ADMINQ_DEVICE_DESCRIPTOR_VERSION 1

/*
 * All AdminQ command structs should be naturally packed. The static_assert
 * calls make sure this is the case at compile time.
 */

struct gve_adminq_describe_device {
	__be64 device_descriptor_addr;
	__be32 device_descriptor_version;
	__be32 available_length;
};

_Static_assert(sizeof(struct gve_adminq_describe_device) == 16,
    "gve: bad admin queue struct length");

struct gve_device_descriptor {
	__be64 max_registered_pages;
	__be16 reserved1;
	__be16 tx_queue_entries;
	__be16 rx_queue_entries;
	__be16 default_num_queues;
	__be16 mtu;
	__be16 counters;
	__be16 reserved2;
	__be16 rx_pages_per_qpl;
	uint8_t  mac[ETHER_ADDR_LEN];
	__be16 num_device_options;
	__be16 total_length;
	uint8_t  reserved3[6];
};

_Static_assert(sizeof(struct gve_device_descriptor) == 40,
    "gve: bad admin queue struct length");

struct gve_device_option {
	__be16 option_id;
	__be16 option_length;
	__be32 required_features_mask;
};

_Static_assert(sizeof(struct gve_device_option) == 8,
    "gve: bad admin queue struct length");

struct gve_device_option_gqi_rda {
	__be32 supported_features_mask;
};

_Static_assert(sizeof(struct gve_device_option_gqi_rda) == 4,
    "gve: bad admin queue struct length");

struct gve_device_option_gqi_qpl {
	__be32 supported_features_mask;
};

_Static_assert(sizeof(struct gve_device_option_gqi_qpl) == 4,
    "gve: bad admin queue struct length");

struct gve_device_option_dqo_rda {
	__be32 supported_features_mask;
};

_Static_assert(sizeof(struct gve_device_option_dqo_rda) == 4,
    "gve: bad admin queue struct length");

struct gve_device_option_modify_ring {
	__be32 supported_features_mask;
	__be16 max_rx_ring_size;
	__be16 max_tx_ring_size;
};

_Static_assert(sizeof(struct gve_device_option_modify_ring) == 8,
    "gve: bad admin queue struct length");

struct gve_device_option_jumbo_frames {
	__be32 supported_features_mask;
	__be16 max_mtu;
	uint8_t padding[2];
};

_Static_assert(sizeof(struct gve_device_option_jumbo_frames) == 8,
    "gve: bad admin queue struct length");

enum gve_dev_opt_id {
	GVE_DEV_OPT_ID_GQI_RAW_ADDRESSING = 0x1,
	GVE_DEV_OPT_ID_GQI_RDA = 0x2,
	GVE_DEV_OPT_ID_GQI_QPL = 0x3,
	GVE_DEV_OPT_ID_DQO_RDA = 0x4,
	GVE_DEV_OPT_ID_MODIFY_RING = 0x6,
	GVE_DEV_OPT_ID_JUMBO_FRAMES = 0x8,
};

/*
 * These masks are way to predicate the use of a particular option on the driver
 * having particular bug fixes represented by each bit position in the mask.
 * Currently they are all zero because there are no known bugs preventing the
 * use of any option.
 */
enum gve_dev_opt_req_feat_mask {
	GVE_DEV_OPT_REQ_FEAT_MASK_GQI_RAW_ADDRESSING = 0x0,
	GVE_DEV_OPT_REQ_FEAT_MASK_GQI_RDA = 0x0,
	GVE_DEV_OPT_REQ_FEAT_MASK_GQI_QPL = 0x0,
	GVE_DEV_OPT_REQ_FEAT_MASK_DQO_RDA = 0x0,
	GVE_DEV_OPT_REQ_FEAT_MASK_MODIFY_RING = 0x0,
	GVE_DEV_OPT_REQ_FEAT_MASK_JUMBO_FRAMES = 0x0,
};

enum gve_sup_feature_mask {
	GVE_SUP_MODIFY_RING_MASK  = 1 << 0,
	GVE_SUP_JUMBO_FRAMES_MASK = 1 << 2,
};

#define GVE_VERSION_STR_LEN 128

enum gve_driver_capability {
	gve_driver_capability_gqi_qpl = 0,
	gve_driver_capability_gqi_rda = 1,
	gve_driver_capability_dqo_qpl = 2, /* reserved for future use */
	gve_driver_capability_dqo_rda = 3,
	gve_driver_capability_alt_miss_compl = 4,
};

#define GVE_CAP1(a) BIT((int) a)
#define GVE_CAP2(a) BIT(((int) a) - 64)
#define GVE_CAP3(a) BIT(((int) a) - 128)
#define GVE_CAP4(a) BIT(((int) a) - 192)

/*
 * The following four defines describe 256 compatibility bits.
 * Only a few bits (as shown in `gve_driver_compatibility`) are currently
 * defined. The rest are reserved for future use.
 */
#define GVE_DRIVER_CAPABILITY_FLAGS1 (GVE_CAP1(gve_driver_capability_gqi_qpl))
#define GVE_DRIVER_CAPABILITY_FLAGS2 0x0
#define GVE_DRIVER_CAPABILITY_FLAGS3 0x0
#define GVE_DRIVER_CAPABILITY_FLAGS4 0x0

struct gve_driver_info {
	uint8_t os_type;
	uint8_t driver_major;
	uint8_t driver_minor;
	uint8_t driver_sub;
	__be32 os_version_major;
	__be32 os_version_minor;
	__be32 os_version_sub;
	__be64 driver_capability_flags[4];
	uint8_t os_version_str1[GVE_VERSION_STR_LEN];
	uint8_t os_version_str2[GVE_VERSION_STR_LEN];
};

struct gve_adminq_verify_driver_compatibility {
	__be64 driver_info_len;
	__be64 driver_info_addr;
};

_Static_assert(sizeof(struct gve_adminq_verify_driver_compatibility) == 16,
    "gve: bad admin queue struct length");

struct gve_adminq_configure_device_resources {
	__be64 counter_array;
	__be64 irq_db_addr;
	__be32 num_counters;
	__be32 num_irq_dbs;
	__be32 irq_db_stride;
	__be32 ntfy_blk_msix_base_idx;
	uint8_t queue_format;
	uint8_t padding[7];
};

_Static_assert(sizeof(struct gve_adminq_configure_device_resources) == 40,
    "gve: bad admin queue struct length");

struct gve_adminq_register_page_list {
	__be32 page_list_id;
	__be32 num_pages;
	__be64 page_address_list_addr;
	__be64 page_size;
};

_Static_assert(sizeof(struct gve_adminq_register_page_list) == 24,
    "gve: bad admin queue struct length");

struct gve_adminq_unregister_page_list {
	__be32 page_list_id;
};

_Static_assert(sizeof(struct gve_adminq_unregister_page_list) == 4,
    "gve: bad admin queue struct length");

struct gve_adminq_create_tx_queue {
	__be32 queue_id;
	__be32 reserved;
	__be64 queue_resources_addr;
	__be64 tx_ring_addr;
	__be32 queue_page_list_id;
	__be32 ntfy_id;
	__be64 tx_comp_ring_addr;
	__be16 tx_ring_size;
	__be16 tx_comp_ring_size;
	uint8_t padding[4];
};

_Static_assert(sizeof(struct gve_adminq_create_tx_queue) == 48,
    "gve: bad admin queue struct length");

struct gve_adminq_create_rx_queue {
	__be32 queue_id;
	__be32 index;
	__be32 reserved;
	__be32 ntfy_id;
	__be64 queue_resources_addr;
	__be64 rx_desc_ring_addr;
	__be64 rx_data_ring_addr;
	__be32 queue_page_list_id;
	__be16 rx_ring_size;
	__be16 packet_buffer_size;
	__be16 rx_buff_ring_size;
	uint8_t enable_rsc;
	uint8_t padding[5];
};

_Static_assert(sizeof(struct gve_adminq_create_rx_queue) == 56,
    "gve: bad admin queue struct length");

/* Queue resources that are shared with the device */
struct gve_queue_resources {
	union {
		struct {
			__be32 db_index;	/* Device -> Guest */
			__be32 counter_index;	/* Device -> Guest */
		};
		uint8_t reserved[64];
	};
};

_Static_assert(sizeof(struct gve_queue_resources) == 64,
    "gve: bad admin queue struct length");

struct gve_adminq_destroy_tx_queue {
	__be32 queue_id;
};

_Static_assert(sizeof(struct gve_adminq_destroy_tx_queue) == 4,
    "gve: bad admin queue struct length");

struct gve_adminq_destroy_rx_queue {
	__be32 queue_id;
};

_Static_assert(sizeof(struct gve_adminq_destroy_rx_queue) == 4,
    "gve: bad admin queue struct length");

/* GVE Set Driver Parameter Types */
enum gve_set_driver_param_types {
	GVE_SET_PARAM_MTU	= 0x1,
};

struct gve_adminq_set_driver_parameter {
	__be32 parameter_type;
	uint8_t reserved[4];
	__be64 parameter_value;
};

_Static_assert(sizeof(struct gve_adminq_set_driver_parameter) == 16,
    "gve: bad admin queue struct length");

struct stats {
	__be32 stat_name;
	__be32 queue_id;
	__be64 value;
};

_Static_assert(sizeof(struct stats) == 16,
    "gve: bad admin queue struct length");

struct gve_adminq_command {
	__be32 opcode;
	__be32 status;
	union {
		struct gve_adminq_configure_device_resources
					configure_device_resources;
		struct gve_adminq_create_tx_queue create_tx_queue;
		struct gve_adminq_create_rx_queue create_rx_queue;
		struct gve_adminq_destroy_tx_queue destroy_tx_queue;
		struct gve_adminq_destroy_rx_queue destroy_rx_queue;
		struct gve_adminq_describe_device describe_device;
		struct gve_adminq_register_page_list reg_page_list;
		struct gve_adminq_unregister_page_list unreg_page_list;
		struct gve_adminq_set_driver_parameter set_driver_param;
		struct gve_adminq_verify_driver_compatibility
					verify_driver_compatibility;
		uint8_t reserved[56];
	};
};

_Static_assert(sizeof(struct gve_adminq_command) == 64,
    "gve: bad admin queue struct length");

int gve_adminq_create_rx_queues(struct gve_priv *priv, uint32_t num_queues);
int gve_adminq_create_tx_queues(struct gve_priv *priv, uint32_t num_queues);
int gve_adminq_destroy_tx_queues(struct gve_priv *priv, uint32_t num_queues);
int gve_adminq_destroy_rx_queues(struct gve_priv *priv, uint32_t num_queues);
int gve_adminq_set_mtu(struct gve_priv *priv, uint32_t mtu);
int gve_adminq_alloc(struct gve_priv *priv);
void gve_reset_adminq(struct gve_priv *priv);
int gve_adminq_describe_device(struct gve_priv *priv);
int gve_adminq_configure_device_resources(struct gve_priv *priv);
int gve_adminq_deconfigure_device_resources(struct gve_priv *priv);
void gve_release_adminq(struct gve_priv *priv);
int gve_adminq_register_page_list(struct gve_priv *priv,
        struct gve_queue_page_list *qpl);
int gve_adminq_unregister_page_list(struct gve_priv *priv, uint32_t page_list_id);
int gve_adminq_verify_driver_compatibility(struct gve_priv *priv,
        uint64_t driver_info_len, vm_paddr_t driver_info_addr);
#endif /* _GVE_AQ_H_ */
