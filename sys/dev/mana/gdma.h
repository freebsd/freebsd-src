/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 */

#ifndef _GDMA_H
#define _GDMA_H

#include <sys/bus.h>
#include <sys/bus_dma.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/sx.h>

#include "gdma_util.h"
#include "shm_channel.h"

#define GDMA_STATUS_MORE_ENTRIES	0x00000105

/* Structures labeled with "HW DATA" are exchanged with the hardware. All of
 * them are naturally aligned and hence don't need __packed.
 */

#define GDMA_BAR0		0

#define GDMA_IRQNAME_SZ		40

struct gdma_bus {
	bus_space_handle_t	bar0_h;
	bus_space_tag_t		bar0_t;
};

struct gdma_msix_entry {
	int			entry;
	int			vector;
};

enum gdma_request_type {
	GDMA_VERIFY_VF_DRIVER_VERSION	= 1,
	GDMA_QUERY_MAX_RESOURCES	= 2,
	GDMA_LIST_DEVICES		= 3,
	GDMA_REGISTER_DEVICE		= 4,
	GDMA_DEREGISTER_DEVICE		= 5,
	GDMA_GENERATE_TEST_EQE		= 10,
	GDMA_CREATE_QUEUE		= 12,
	GDMA_DISABLE_QUEUE		= 13,
	GDMA_ALLOCATE_RESOURCE_RANGE	= 22,
	GDMA_DESTROY_RESOURCE_RANGE	= 24,
	GDMA_CREATE_DMA_REGION		= 25,
	GDMA_DMA_REGION_ADD_PAGES	= 26,
	GDMA_DESTROY_DMA_REGION		= 27,
	GDMA_CREATE_PD			= 29,
	GDMA_DESTROY_PD			= 30,
	GDMA_CREATE_MR			= 31,
	GDMA_DESTROY_MR			= 32,
};

#define GDMA_RESOURCE_DOORBELL_PAGE	27

enum gdma_queue_type {
	GDMA_INVALID_QUEUE,
	GDMA_SQ,
	GDMA_RQ,
	GDMA_CQ,
	GDMA_EQ,
};

enum gdma_work_request_flags {
	GDMA_WR_NONE			= 0,
	GDMA_WR_OOB_IN_SGL		= BIT(0),
	GDMA_WR_PAD_BY_SGE0		= BIT(1),
};

enum gdma_eqe_type {
	GDMA_EQE_COMPLETION		= 3,
	GDMA_EQE_TEST_EVENT		= 64,
	GDMA_EQE_HWC_INIT_EQ_ID_DB	= 129,
	GDMA_EQE_HWC_INIT_DATA		= 130,
	GDMA_EQE_HWC_INIT_DONE		= 131,
};

enum {
	GDMA_DEVICE_NONE	= 0,
	GDMA_DEVICE_HWC		= 1,
	GDMA_DEVICE_MANA	= 2,
};

typedef uint64_t gdma_obj_handle_t;

struct gdma_resource {
	/* Protect the bitmap */
	struct mtx		lock_spin;

	/* The bitmap size in bits. */
	uint32_t		size;

	/* The bitmap tracks the resources. */
	unsigned long		*map;
};

union gdma_doorbell_entry {
	uint64_t		as_uint64;

	struct {
		uint64_t id		: 24;
		uint64_t reserved	: 8;
		uint64_t tail_ptr	: 31;
		uint64_t arm		: 1;
	} cq;

	struct {
		uint64_t id		: 24;
		uint64_t wqe_cnt	: 8;
		uint64_t tail_ptr	: 32;
	} rq;

	struct {
		uint64_t id		: 24;
		uint64_t reserved	: 8;
		uint64_t tail_ptr	: 32;
	} sq;

	struct {
		uint64_t id		: 16;
		uint64_t reserved	: 16;
		uint64_t tail_ptr	: 31;
		uint64_t arm		: 1;
	} eq;
}; /* HW DATA */

struct gdma_msg_hdr {
	uint32_t	hdr_type;
	uint32_t	msg_type;
	uint16_t	msg_version;
	uint16_t	hwc_msg_id;
	uint32_t	msg_size;
}; /* HW DATA */

struct gdma_dev_id {
	union {
		struct {
			uint16_t type;
			uint16_t instance;
		};

		uint32_t as_uint32;
	};
}; /* HW DATA */

struct gdma_req_hdr {
	struct gdma_msg_hdr	req;
	struct gdma_msg_hdr	resp; /* The expected response */
	struct gdma_dev_id	dev_id;
	uint32_t		activity_id;
}; /* HW DATA */

struct gdma_resp_hdr {
	struct gdma_msg_hdr	response;
	struct gdma_dev_id	dev_id;
	uint32_t		activity_id;
	uint32_t		status;
	uint32_t		reserved;
}; /* HW DATA */

struct gdma_general_req {
	struct gdma_req_hdr	hdr;
}; /* HW DATA */

#define GDMA_MESSAGE_V1 1

struct gdma_general_resp {
	struct gdma_resp_hdr	hdr;
}; /* HW DATA */

#define GDMA_STANDARD_HEADER_TYPE	0

static inline void
mana_gd_init_req_hdr(struct gdma_req_hdr *hdr, uint32_t code,
    uint32_t req_size, uint32_t resp_size)
{
	hdr->req.hdr_type = GDMA_STANDARD_HEADER_TYPE;
	hdr->req.msg_type = code;
	hdr->req.msg_version = GDMA_MESSAGE_V1;
	hdr->req.msg_size = req_size;

	hdr->resp.hdr_type = GDMA_STANDARD_HEADER_TYPE;
	hdr->resp.msg_type = code;
	hdr->resp.msg_version = GDMA_MESSAGE_V1;
	hdr->resp.msg_size = resp_size;
}

/* The 16-byte struct is part of the GDMA work queue entry (WQE). */
struct gdma_sge {
	uint64_t		address;
	uint32_t		mem_key;
	uint32_t		size;
}; /* HW DATA */

struct gdma_wqe_request {
	struct gdma_sge		*sgl;
	uint32_t		num_sge;

	uint32_t		inline_oob_size;
	const void		*inline_oob_data;

	uint32_t		flags;
	uint32_t		client_data_unit;
};

enum gdma_page_type {
	GDMA_PAGE_TYPE_4K,
};

#define GDMA_INVALID_DMA_REGION		0

struct gdma_mem_info {
	device_t		dev;

	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_addr_t		dma_handle;	/* Physical address	*/
	void			*virt_addr;	/* Virtual address	*/
	uint64_t		length;

	/* Allocated by the PF driver */
	gdma_obj_handle_t	dma_region_handle;
};

#define REGISTER_ATB_MST_MKEY_LOWER_SIZE 8

struct gdma_dev {
	struct gdma_context	*gdma_context;

	struct gdma_dev_id	dev_id;

	uint32_t		pdid;
	uint32_t		doorbell;
	uint32_t		gpa_mkey;

	/* GDMA driver specific pointer */
	void			*driver_data;
};

#define MINIMUM_SUPPORTED_PAGE_SIZE PAGE_SIZE

#define GDMA_CQE_SIZE		64
#define GDMA_EQE_SIZE		16
#define GDMA_MAX_SQE_SIZE	512
#define GDMA_MAX_RQE_SIZE	256

#define GDMA_COMP_DATA_SIZE	0x3C

#define GDMA_EVENT_DATA_SIZE	0xC

/* The WQE size must be a multiple of the Basic Unit, which is 32 bytes. */
#define GDMA_WQE_BU_SIZE	32

#define INVALID_PDID		UINT_MAX
#define INVALID_DOORBELL	UINT_MAX
#define INVALID_MEM_KEY		UINT_MAX
#define INVALID_QUEUE_ID	UINT_MAX
#define INVALID_PCI_MSIX_INDEX  UINT_MAX

struct gdma_comp {
	uint32_t		cqe_data[GDMA_COMP_DATA_SIZE / 4];
	uint32_t		wq_num;
	bool			is_sq;
};

struct gdma_event {
	uint32_t		details[GDMA_EVENT_DATA_SIZE / 4];
	uint8_t			type;
};

struct gdma_queue;

typedef void gdma_eq_callback(void *context, struct gdma_queue *q,
    struct gdma_event *e);

typedef void gdma_cq_callback(void *context, struct gdma_queue *q);

/* The 'head' is the producer index. For SQ/RQ, when the driver posts a WQE
 * (Note: the WQE size must be a multiple of the 32-byte Basic Unit), the
 * driver increases the 'head' in BUs rather than in bytes, and notifies
 * the HW of the updated head. For EQ/CQ, the driver uses the 'head' to track
 * the HW head, and increases the 'head' by 1 for every processed EQE/CQE.
 *
 * The 'tail' is the consumer index for SQ/RQ. After the CQE of the SQ/RQ is
 * processed, the driver increases the 'tail' to indicate that WQEs have
 * been consumed by the HW, so the driver can post new WQEs into the SQ/RQ.
 *
 * The driver doesn't use the 'tail' for EQ/CQ, because the driver ensures
 * that the EQ/CQ is big enough so they can't overflow, and the driver uses
 * the owner bits mechanism to detect if the queue has become empty.
 */
struct gdma_queue {
	struct gdma_dev		*gdma_dev;

	enum gdma_queue_type	type;
	uint32_t		id;

	struct gdma_mem_info	mem_info;

	void			*queue_mem_ptr;
	uint32_t		queue_size;

	bool			monitor_avl_buf;

	uint32_t		head;
	uint32_t		tail;

	/* Extra fields specific to EQ/CQ. */
	union {
		struct {
			bool			disable_needed;

			gdma_eq_callback	*callback;
			void			*context;

			unsigned int		msix_index;

			uint32_t		log2_throttle_limit;
		} eq;

		struct {
			gdma_cq_callback	*callback;
			void			*context;

			/* For CQ/EQ relationship */
			struct gdma_queue	*parent;
		} cq;
	};
};

struct gdma_queue_spec {
	enum gdma_queue_type	type;
	bool			monitor_avl_buf;
	unsigned int		queue_size;

	/* Extra fields specific to EQ/CQ. */
	union {
		struct {
			gdma_eq_callback	*callback;
			void			*context;

			unsigned long		log2_throttle_limit;
		} eq;

		struct {
			gdma_cq_callback	*callback;
			void			*context;

			struct			gdma_queue *parent_eq;

		} cq;
	};
};

struct mana_eq {
	struct gdma_queue	*eq;
};

struct gdma_irq_context {
	struct gdma_msix_entry	msix_e;
	struct resource		*res;
	driver_intr_t		*handler;
	void			*arg;
	void			*cookie;
	bool			requested;
	int			cpu;
	char			name[GDMA_IRQNAME_SZ];
};

struct gdma_context {
	device_t		dev;

	struct gdma_bus		gd_bus;

	/* Per-vPort max number of queues */
	unsigned int		max_num_queues;
	unsigned int		max_num_msix;
	unsigned int		num_msix_usable;
	struct gdma_resource	msix_resource;
	struct gdma_irq_context	*irq_contexts;

	/* This maps a CQ index to the queue structure. */
	unsigned int		max_num_cqs;
	struct gdma_queue	**cq_table;

	/* Protect eq_test_event and test_event_eq_id  */
	struct sx		eq_test_event_sx;
	struct completion	eq_test_event;
	uint32_t		test_event_eq_id;

	struct resource		*bar0;
	struct resource		*msix;
	int			msix_rid;
	void __iomem		*shm_base;
	void __iomem		*db_page_base;
	vm_paddr_t		phys_db_page_base;
	uint32_t		db_page_size;

	/* Shared memory chanenl (used to bootstrap HWC) */
	struct shm_channel	shm_channel;

	/* Hardware communication channel (HWC) */
	struct gdma_dev		hwc;

	/* Azure network adapter */
	struct gdma_dev		mana;
};

#define MAX_NUM_GDMA_DEVICES	4

static inline bool mana_gd_is_mana(struct gdma_dev *gd)
{
	return gd->dev_id.type == GDMA_DEVICE_MANA;
}

static inline bool mana_gd_is_hwc(struct gdma_dev *gd)
{
	return gd->dev_id.type == GDMA_DEVICE_HWC;
}

uint8_t *mana_gd_get_wqe_ptr(const struct gdma_queue *wq, uint32_t wqe_offset);
uint32_t mana_gd_wq_avail_space(struct gdma_queue *wq);

int mana_gd_test_eq(struct gdma_context *gc, struct gdma_queue *eq);

int mana_gd_create_hwc_queue(struct gdma_dev *gd,
    const struct gdma_queue_spec *spec,
    struct gdma_queue **queue_ptr);

int mana_gd_create_mana_eq(struct gdma_dev *gd,
    const struct gdma_queue_spec *spec,
    struct gdma_queue **queue_ptr);

int mana_gd_create_mana_wq_cq(struct gdma_dev *gd,
    const struct gdma_queue_spec *spec,
    struct gdma_queue **queue_ptr);

void mana_gd_destroy_queue(struct gdma_context *gc, struct gdma_queue *queue);

int mana_gd_poll_cq(struct gdma_queue *cq, struct gdma_comp *comp, int num_cqe);

void mana_gd_ring_cq(struct gdma_queue *cq, uint8_t arm_bit);

struct gdma_wqe {
	uint32_t reserved	:24;
	uint32_t last_vbytes	:8;

	union {
		uint32_t flags;

		struct {
			uint32_t num_sge		:8;
			uint32_t inline_oob_size_div4	:3;
			uint32_t client_oob_in_sgl	:1;
			uint32_t reserved1		:4;
			uint32_t client_data_unit	:14;
			uint32_t reserved2		:2;
		};
	};
}; /* HW DATA */

#define INLINE_OOB_SMALL_SIZE	8
#define INLINE_OOB_LARGE_SIZE	24

#define MAX_TX_WQE_SIZE		512
#define MAX_RX_WQE_SIZE		256

#define MAX_TX_WQE_SGL_ENTRIES	((GDMA_MAX_SQE_SIZE -			   \
			sizeof(struct gdma_sge) - INLINE_OOB_SMALL_SIZE) / \
			sizeof(struct gdma_sge))

#define MAX_RX_WQE_SGL_ENTRIES	((GDMA_MAX_RQE_SIZE -			   \
			sizeof(struct gdma_sge)) / sizeof(struct gdma_sge))

struct gdma_cqe {
	uint32_t cqe_data[GDMA_COMP_DATA_SIZE / 4];

	union {
		uint32_t as_uint32;

		struct {
			uint32_t wq_num		:24;
			uint32_t is_sq		:1;
			uint32_t reserved	:4;
			uint32_t owner_bits	:3;
		};
	} cqe_info;
}; /* HW DATA */

#define GDMA_CQE_OWNER_BITS	3

#define GDMA_CQE_OWNER_MASK	((1 << GDMA_CQE_OWNER_BITS) - 1)

#define SET_ARM_BIT		1

#define GDMA_EQE_OWNER_BITS	3

union gdma_eqe_info {
	uint32_t as_uint32;

	struct {
		uint32_t type		: 8;
		uint32_t reserved1	: 8;
		uint32_t client_id	: 2;
		uint32_t reserved2	: 11;
		uint32_t owner_bits	: 3;
	};
}; /* HW DATA */

#define GDMA_EQE_OWNER_MASK	((1 << GDMA_EQE_OWNER_BITS) - 1)
#define INITIALIZED_OWNER_BIT(log2_num_entries)	(1UL << (log2_num_entries))

struct gdma_eqe {
	uint32_t details[GDMA_EVENT_DATA_SIZE / 4];
	uint32_t eqe_info;
}; /* HW DATA */

#define GDMA_REG_DB_PAGE_OFFSET	8
#define GDMA_REG_DB_PAGE_SIZE	0x10
#define GDMA_REG_SHM_OFFSET	0x18

struct gdma_posted_wqe_info {
	uint32_t wqe_size_in_bu;
};

/* GDMA_GENERATE_TEST_EQE */
struct gdma_generate_test_event_req {
	struct gdma_req_hdr hdr;
	uint32_t queue_index;
}; /* HW DATA */

/* GDMA_VERIFY_VF_DRIVER_VERSION */
enum {
	GDMA_PROTOCOL_V1	= 1,
	GDMA_PROTOCOL_FIRST	= GDMA_PROTOCOL_V1,
	GDMA_PROTOCOL_LAST	= GDMA_PROTOCOL_V1,
};

struct gdma_verify_ver_req {
	struct gdma_req_hdr hdr;

	/* Mandatory fields required for protocol establishment */
	uint64_t protocol_ver_min;
	uint64_t protocol_ver_max;
	uint64_t drv_cap_flags1;
	uint64_t drv_cap_flags2;
	uint64_t drv_cap_flags3;
	uint64_t drv_cap_flags4;

	/* Advisory fields */
	uint64_t drv_ver;
	uint32_t os_type; /* Linux = 0x10; Windows = 0x20; Other = 0x30 */
	uint32_t reserved;
	uint32_t os_ver_major;
	uint32_t os_ver_minor;
	uint32_t os_ver_build;
	uint32_t os_ver_platform;
	uint64_t reserved_2;
	uint8_t os_ver_str1[128];
	uint8_t os_ver_str2[128];
	uint8_t os_ver_str3[128];
	uint8_t os_ver_str4[128];
}; /* HW DATA */

struct gdma_verify_ver_resp {
	struct gdma_resp_hdr hdr;
	uint64_t gdma_protocol_ver;
	uint64_t pf_cap_flags1;
	uint64_t pf_cap_flags2;
	uint64_t pf_cap_flags3;
	uint64_t pf_cap_flags4;
}; /* HW DATA */

/* GDMA_QUERY_MAX_RESOURCES */
struct gdma_query_max_resources_resp {
	struct gdma_resp_hdr hdr;
	uint32_t status;
	uint32_t max_sq;
	uint32_t max_rq;
	uint32_t max_cq;
	uint32_t max_eq;
	uint32_t max_db;
	uint32_t max_mst;
	uint32_t max_cq_mod_ctx;
	uint32_t max_mod_cq;
	uint32_t max_msix;
}; /* HW DATA */

/* GDMA_LIST_DEVICES */
struct gdma_list_devices_resp {
	struct gdma_resp_hdr hdr;
	uint32_t num_of_devs;
	uint32_t reserved;
	struct gdma_dev_id devs[64];
}; /* HW DATA */

/* GDMA_REGISTER_DEVICE */
struct gdma_register_device_resp {
	struct gdma_resp_hdr hdr;
	uint32_t pdid;
	uint32_t gpa_mkey;
	uint32_t db_id;
}; /* HW DATA */

struct gdma_allocate_resource_range_req {
	struct gdma_req_hdr hdr;
	uint32_t resource_type;
	uint32_t num_resources;
	uint32_t alignment;
	uint32_t allocated_resources;
};

struct gdma_allocate_resource_range_resp {
	struct gdma_resp_hdr hdr;
	uint32_t allocated_resources;
};

struct gdma_destroy_resource_range_req {
	struct gdma_req_hdr hdr;
	uint32_t resource_type;
	uint32_t num_resources;
	uint32_t allocated_resources;
};

/* GDMA_CREATE_QUEUE */
struct gdma_create_queue_req {
	struct gdma_req_hdr hdr;
	uint32_t type;
	uint32_t reserved1;
	uint32_t pdid;
	uint32_t doolbell_id;
	gdma_obj_handle_t gdma_region;
	uint32_t reserved2;
	uint32_t queue_size;
	uint32_t log2_throttle_limit;
	uint32_t eq_pci_msix_index;
	uint32_t cq_mod_ctx_id;
	uint32_t cq_parent_eq_id;
	uint8_t  rq_drop_on_overrun;
	uint8_t  rq_err_on_wqe_overflow;
	uint8_t  rq_chain_rec_wqes;
	uint8_t  sq_hw_db;
	uint32_t reserved3;
}; /* HW DATA */

struct gdma_create_queue_resp {
	struct gdma_resp_hdr hdr;
	uint32_t queue_index;
}; /* HW DATA */

/* GDMA_DISABLE_QUEUE */
struct gdma_disable_queue_req {
	struct gdma_req_hdr hdr;
	uint32_t type;
	uint32_t queue_index;
	uint32_t alloc_res_id_on_creation;
}; /* HW DATA */

enum atb_page_size {
	ATB_PAGE_SIZE_4K,
	ATB_PAGE_SIZE_8K,
	ATB_PAGE_SIZE_16K,
	ATB_PAGE_SIZE_32K,
	ATB_PAGE_SIZE_64K,
	ATB_PAGE_SIZE_128K,
	ATB_PAGE_SIZE_256K,
	ATB_PAGE_SIZE_512K,
	ATB_PAGE_SIZE_1M,
	ATB_PAGE_SIZE_2M,
	ATB_PAGE_SIZE_MAX,
};

enum gdma_mr_access_flags {
	GDMA_ACCESS_FLAG_LOCAL_READ = BIT(0),
	GDMA_ACCESS_FLAG_LOCAL_WRITE = BIT(1),
	GDMA_ACCESS_FLAG_REMOTE_READ = BIT(2),
	GDMA_ACCESS_FLAG_REMOTE_WRITE = BIT(3),
	GDMA_ACCESS_FLAG_REMOTE_ATOMIC = BIT(4),
};

/* GDMA_CREATE_DMA_REGION */
struct gdma_create_dma_region_req {
	struct gdma_req_hdr hdr;

	/* The total size of the DMA region */
	uint64_t length;

	/* The offset in the first page */
	uint32_t offset_in_page;

	/* enum gdma_page_type */
	uint32_t gdma_page_type;

	/* The total number of pages */
	uint32_t page_count;

	/* If page_addr_list_len is smaller than page_count,
	 * the remaining page addresses will be added via the
	 * message GDMA_DMA_REGION_ADD_PAGES.
	 */
	uint32_t page_addr_list_len;
	uint64_t page_addr_list[];
}; /* HW DATA */

struct gdma_create_dma_region_resp {
	struct gdma_resp_hdr hdr;
	gdma_obj_handle_t dma_region_handle;
}; /* HW DATA */

/* GDMA_DMA_REGION_ADD_PAGES */
struct gdma_dma_region_add_pages_req {
	struct gdma_req_hdr hdr;

	gdma_obj_handle_t dma_region_handle;

	uint32_t page_addr_list_len;
	uint32_t reserved3;

	uint64_t page_addr_list[];
}; /* HW DATA */

/* GDMA_DESTROY_DMA_REGION */
struct gdma_destroy_dma_region_req {
	struct gdma_req_hdr hdr;

	gdma_obj_handle_t dma_region_handle;
}; /* HW DATA */

enum gdma_pd_flags {
	GDMA_PD_FLAG_INVALID = 0,
};

struct gdma_create_pd_req {
	struct gdma_req_hdr hdr;
	enum gdma_pd_flags flags;
	uint32_t reserved;
};/* HW DATA */

struct gdma_create_pd_resp {
	struct gdma_resp_hdr hdr;
	gdma_obj_handle_t pd_handle;
	uint32_t pd_id;
	uint32_t reserved;
};/* HW DATA */

struct gdma_destroy_pd_req {
	struct gdma_req_hdr hdr;
	gdma_obj_handle_t pd_handle;
};/* HW DATA */

struct gdma_destory_pd_resp {
	struct gdma_resp_hdr hdr;
};/* HW DATA */

enum gdma_mr_type {
	/* Guest Virtual Address - MRs of this type allow access
	 * to memory mapped by PTEs associated with this MR using a virtual
	 * address that is set up in the MST
	 */
	GDMA_MR_TYPE_GVA = 2,
};

struct gdma_create_mr_params {
	gdma_obj_handle_t pd_handle;
	enum gdma_mr_type mr_type;
	union {
		struct {
			gdma_obj_handle_t dma_region_handle;
			uint64_t virtual_address;
			enum gdma_mr_access_flags access_flags;
		} gva;
	};
};

struct gdma_create_mr_request {
	struct gdma_req_hdr hdr;
	gdma_obj_handle_t pd_handle;
	enum gdma_mr_type mr_type;
	uint32_t reserved_1;

	union {
		struct {
			gdma_obj_handle_t dma_region_handle;
			uint64_t virtual_address;
			enum gdma_mr_access_flags access_flags;
		} gva;

	};
	uint32_t reserved_2;
};/* HW DATA */

struct gdma_create_mr_response {
	struct gdma_resp_hdr hdr;
	gdma_obj_handle_t mr_handle;
	uint32_t lkey;
	uint32_t rkey;
};/* HW DATA */

struct gdma_destroy_mr_request {
	struct gdma_req_hdr hdr;
	gdma_obj_handle_t mr_handle;
};/* HW DATA */

struct gdma_destroy_mr_response {
	struct gdma_resp_hdr hdr;
};/* HW DATA */

int mana_gd_verify_vf_version(device_t dev);

int mana_gd_register_device(struct gdma_dev *gd);
int mana_gd_deregister_device(struct gdma_dev *gd);

int mana_gd_post_work_request(struct gdma_queue *wq,
    const struct gdma_wqe_request *wqe_req,
    struct gdma_posted_wqe_info *wqe_info);

int mana_gd_post_and_ring(struct gdma_queue *queue,
    const struct gdma_wqe_request *wqe,
    struct gdma_posted_wqe_info *wqe_info);

int mana_gd_alloc_res_map(uint32_t res_avil, struct gdma_resource *r,
    const char *lock_name);
void mana_gd_free_res_map(struct gdma_resource *r);

void mana_gd_wq_ring_doorbell(struct gdma_context *gc,
    struct gdma_queue *queue);

int mana_gd_alloc_memory(struct gdma_context *gc, unsigned int length,
    struct gdma_mem_info *gmi);

void mana_gd_free_memory(struct gdma_mem_info *gmi);

void mana_gd_dma_map_paddr(void *arg, bus_dma_segment_t *segs,
    int nseg, int error);

int mana_gd_send_request(struct gdma_context *gc, uint32_t req_len,
    const void *req, uint32_t resp_len, void *resp);

int mana_gd_allocate_doorbell_page(struct gdma_context *gc,
    int *doorbell_page);

int mana_gd_destroy_doorbell_page(struct gdma_context *gc,
    int doorbell_page);

int mana_gd_destroy_dma_region(struct gdma_context *gc,
    gdma_obj_handle_t dma_region_handle);
#endif /* _GDMA_H */
