/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef __UFSHCI_PRIVATE_H__
#define __UFSHCI_PRIVATE_H__

#ifdef _KERNEL
#include <sys/types.h>
#else /* !_KERNEL */
#include <stdbool.h>
#include <stdint.h>
#endif /* _KERNEL */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/counter.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include "ufshci.h"

MALLOC_DECLARE(M_UFSHCI);

#define UFSHCI_DEVICE_INIT_TIMEOUT_MS (2000) /* in milliseconds */
#define UFSHCI_UIC_CMD_TIMEOUT_MS     (500)  /* in milliseconds */
#define UFSHCI_DEFAULT_TIMEOUT_PERIOD (10)   /* in seconds */
#define UFSHCI_MIN_TIMEOUT_PERIOD     (5)    /* in seconds */
#define UFSHCI_MAX_TIMEOUT_PERIOD     (120)  /* in seconds */

#define UFSHCI_DEFAULT_RETRY_COUNT    (4)

#define UFSHCI_UTR_ENTRIES	      (32)
#define UFSHCI_UTRM_ENTRIES	      (8)

#define UFSHCI_SECTOR_SIZE	      (512)

struct ufshci_controller;

struct ufshci_completion_poll_status {
	struct ufshci_completion cpl;
	int done;
	bool error;
};

struct ufshci_request {
	struct ufshci_upiu request_upiu;
	size_t request_size;
	size_t response_size;

	struct memdesc payload;
	enum ufshci_data_direction data_direction;
	ufshci_cb_fn_t cb_fn;
	void *cb_arg;
	bool is_admin;
	int32_t retries;
	bool payload_valid;
	bool timeout;
	bool spare[2]; /* Future use */
	STAILQ_ENTRY(ufshci_request) stailq;
};

enum ufshci_slot_state {
	UFSHCI_SLOT_STATE_FREE = 0x0,
	UFSHCI_SLOT_STATE_RESERVED = 0x1,
	UFSHCI_SLOT_STATE_SCHEDULED = 0x2,
	UFSHCI_SLOT_STATE_TIMEOUT = 0x3,
	UFSHCI_SLOT_STATE_NEED_ERROR_HANDLING = 0x4,
};

struct ufshci_tracker {
	struct ufshci_request *req;
	struct ufshci_req_queue *req_queue;
	struct ufshci_hw_queue *hwq;
	uint8_t slot_num;
	enum ufshci_slot_state slot_state;
	size_t response_size;
	sbintime_t deadline;

	bus_dmamap_t payload_dma_map;
	uint64_t payload_addr;

	struct ufshci_utp_cmd_desc *ucd;
	bus_addr_t ucd_bus_addr;

	uint16_t prdt_off;
	uint16_t prdt_entry_cnt;
};

enum ufshci_queue_mode {
	UFSHCI_Q_MODE_SDB = 0x00, /* Single Doorbell Mode*/
	UFSHCI_Q_MODE_MCQ = 0x01, /* Multi-Circular Queue Mode*/
};

/*
 * UFS uses slot-based Single Doorbell (SDB) mode for request submission by
 * default and additionally supports Multi-Circular Queue (MCQ) in UFS 4.0. To
 * minimize duplicated code between SDB and MCQ, mode dependent operations are
 * extracted into ufshci_qops.
 */
struct ufshci_qops {
	int (*construct)(struct ufshci_controller *ctrlr,
	    struct ufshci_req_queue *req_queue, uint32_t num_entries,
	    bool is_task_mgmt);
	void (*destroy)(struct ufshci_controller *ctrlr,
	    struct ufshci_req_queue *req_queue);
	struct ufshci_hw_queue *(*get_hw_queue)(
	    struct ufshci_req_queue *req_queue);
	int (*enable)(struct ufshci_controller *ctrlr,
	    struct ufshci_req_queue *req_queue);
	int (*reserve_slot)(struct ufshci_req_queue *req_queue,
	    struct ufshci_tracker **tr);
	int (*reserve_admin_slot)(struct ufshci_req_queue *req_queue,
	    struct ufshci_tracker **tr);
	void (*ring_doorbell)(struct ufshci_controller *ctrlr,
	    struct ufshci_tracker *tr);
	bool (*is_doorbell_cleared)(struct ufshci_controller *ctrlr,
	    uint8_t slot);
	void (*clear_cpl_ntf)(struct ufshci_controller *ctrlr,
	    struct ufshci_tracker *tr);
	bool (*process_cpl)(struct ufshci_req_queue *req_queue);
	int (*get_inflight_io)(struct ufshci_controller *ctrlr);
};

#define UFSHCI_SDB_Q 0 /* Queue number for a single doorbell queue */

/*
 * Generic queue container used by both SDB (fixed 32-slot bitmap) and MCQ
 * (ring buffer) modes. Fields are shared; some such as sq_head, sq_tail and
 * cq_head are not used in SDB but used in MCQ.
 */
struct ufshci_hw_queue {
	uint32_t id;
	int domain;
	int cpu;

	union {
		struct ufshci_utp_xfer_req_desc *utrd;
		struct ufshci_utp_task_mgmt_req_desc *utmrd;
	};

	bus_dma_tag_t dma_tag_queue;
	bus_dmamap_t queuemem_map;
	bus_addr_t req_queue_addr;

	bus_addr_t *ucd_bus_addr;

	uint32_t num_entries;
	uint32_t num_trackers;

	/*
	 * A Request List using the single doorbell method uses a dedicated
	 * ufshci_tracker, one per slot.
	 */
	struct ufshci_tracker **act_tr;

	uint32_t sq_head; /* MCQ mode */
	uint32_t sq_tail; /* MCQ mode */
	uint32_t cq_head; /* MCQ mode */

	uint32_t phase;
	int64_t num_cmds;
	int64_t num_intr_handler_calls;
	int64_t num_retries;
	int64_t num_failures;

	struct mtx_padalign qlock;
};

struct ufshci_req_queue {
	struct ufshci_controller *ctrlr;
	int domain;

	/*
	 *  queue_mode: active transfer scheme
	 *  UFSHCI_Q_MODE_SDB – legacy single‑doorbell list
	 *  UFSHCI_Q_MODE_MCQ – modern multi‑circular queue (UFSHCI 4.0+)
	 */
	enum ufshci_queue_mode queue_mode;

	uint8_t num_q;
	struct ufshci_hw_queue *hwq;

	struct ufshci_qops qops;

	bool is_task_mgmt;
	uint32_t num_entries;
	uint32_t num_trackers;

	/* Shared DMA resource */
	struct ufshci_utp_cmd_desc *ucd;

	bus_dma_tag_t dma_tag_ucd;
	bus_dma_tag_t dma_tag_payload;

	bus_dmamap_t ucdmem_map;
};

struct ufshci_device {
	uint32_t max_lun_count;

	struct ufshci_device_descriptor dev_desc;
	struct ufshci_geometry_descriptor geo_desc;

	uint32_t unipro_version;

	/* WriteBooster */
	bool is_wb_enabled;
	bool is_wb_flush_enabled;
	uint32_t wb_buffer_type;
	uint32_t wb_buffer_size_mb;
	uint32_t wb_user_space_config_option;
	uint8_t wb_dedicated_lu;
	uint32_t write_booster_flush_threshold;
};

/*
 * One of these per allocated device.
 */
struct ufshci_controller {
	device_t dev;

	uint32_t quirks;
#define UFSHCI_QUIRK_IGNORE_UIC_POWER_MODE \
	1 /* QEMU does not support UIC POWER MODE */
#define UFSHCI_QUIRK_LONG_PEER_PA_TACTIVATE \
	2 /* Need an additional 200 ms of PA_TActivate */
#define UFSHCI_QUIRK_WAIT_AFTER_POWER_MODE_CHANGE \
	4 /* Need to wait 1250us after power mode change */
#define UFSHCI_QUIRK_CHANGE_LANE_AND_GEAR_SEPARATELY \
	8 /* Need to change the number of lanes before changing HS-GEAR. */
	uint32_t ref_clk;

	struct cam_sim *ufshci_sim;
	struct cam_path *ufshci_path;

	struct mtx sc_mtx;
	uint32_t sc_unit;
	uint8_t sc_name[16];

	struct ufshci_device ufs_dev;

	bus_space_tag_t bus_tag;
	bus_space_handle_t bus_handle;
	int resource_id;
	struct resource *resource;

	/* Currently, there is no UFSHCI that supports MSI, MSI-X.  */
	int msi_count;

	/* Fields for tracking progress during controller initialization. */
	struct intr_config_hook config_hook;

	/* For shared legacy interrupt. */
	int rid;
	struct resource *res;
	void *tag;

	uint32_t major_version;
	uint32_t minor_version;

	uint32_t num_io_queues;
	uint32_t max_hw_pend_io;

	/* Maximum logical unit number */
	uint32_t max_lun_count;

	/* Maximum i/o size in bytes */
	uint32_t max_xfer_size;

	/* Controller capacity */
	uint32_t cap;

	/* Page size and log2(page_size) - 12 that we're currently using */
	uint32_t page_size;

	/* Timeout value on device initialization */
	uint32_t device_init_timeout_in_ms;

	/* Timeout value on UIC command */
	uint32_t uic_cmd_timeout_in_ms;

	/* UTMR/UTR queue timeout period in seconds */
	uint32_t timeout_period;

	/* UTMR/UTR queue retry count */
	uint32_t retry_count;

	/* UFS Host Controller Interface Registers */
	struct ufshci_registers *regs;

	/* UFS Transport Protocol Layer (UTP) */
	struct ufshci_req_queue task_mgmt_req_queue;
	struct ufshci_req_queue transfer_req_queue;
	bool is_single_db_supported; /* 0 = supported */
	bool is_mcq_supported;	     /* 1 = supported */

	/* UFS Interconnect Layer (UIC) */
	struct mtx uic_cmd_lock;
	uint32_t unipro_version;
	uint8_t hs_gear;
	uint32_t tx_lanes;
	uint32_t rx_lanes;
	uint32_t max_rx_hs_gear;
	uint32_t max_tx_lanes;
	uint32_t max_rx_lanes;

	bool is_failed;
};

#define ufshci_mmio_offsetof(reg) offsetof(struct ufshci_registers, reg)

#define ufshci_mmio_read_4(sc, reg)                       \
	bus_space_read_4((sc)->bus_tag, (sc)->bus_handle, \
	    ufshci_mmio_offsetof(reg))

#define ufshci_mmio_write_4(sc, reg, val)                  \
	bus_space_write_4((sc)->bus_tag, (sc)->bus_handle, \
	    ufshci_mmio_offsetof(reg), val)

#define ufshci_printf(ctrlr, fmt, args...) \
	device_printf(ctrlr->dev, fmt, ##args)

/* UFSHCI */
void ufshci_completion_poll_cb(void *arg, const struct ufshci_completion *cpl,
    bool error);

/* SIM */
int ufshci_sim_attach(struct ufshci_controller *ctrlr);
void ufshci_sim_detach(struct ufshci_controller *ctrlr);

/* Controller */
int ufshci_ctrlr_construct(struct ufshci_controller *ctrlr, device_t dev);
void ufshci_ctrlr_destruct(struct ufshci_controller *ctrlr, device_t dev);
int ufshci_ctrlr_reset(struct ufshci_controller *ctrlr);
/* ctrlr defined as void * to allow use with config_intrhook. */
void ufshci_ctrlr_start_config_hook(void *arg);
void ufshci_ctrlr_poll(struct ufshci_controller *ctrlr);

int ufshci_ctrlr_submit_task_mgmt_request(struct ufshci_controller *ctrlr,
    struct ufshci_request *req);
int ufshci_ctrlr_submit_admin_request(struct ufshci_controller *ctrlr,
    struct ufshci_request *req);
int ufshci_ctrlr_submit_io_request(struct ufshci_controller *ctrlr,
    struct ufshci_request *req);
int ufshci_ctrlr_send_nop(struct ufshci_controller *ctrlr);

void ufshci_reg_dump(struct ufshci_controller *ctrlr);

/* Device */
int ufshci_dev_init(struct ufshci_controller *ctrlr);
int ufshci_dev_reset(struct ufshci_controller *ctrlr);
int ufshci_dev_init_reference_clock(struct ufshci_controller *ctrlr);
int ufshci_dev_init_unipro(struct ufshci_controller *ctrlr);
int ufshci_dev_init_uic_power_mode(struct ufshci_controller *ctrlr);
int ufshci_dev_init_ufs_power_mode(struct ufshci_controller *ctrlr);
int ufshci_dev_get_descriptor(struct ufshci_controller *ctrlr);
int ufshci_dev_config_write_booster(struct ufshci_controller *ctrlr);

/* Controller Command */
void ufshci_ctrlr_cmd_send_task_mgmt_request(struct ufshci_controller *ctrlr,
    ufshci_cb_fn_t cb_fn, void *cb_arg, uint8_t function, uint8_t lun,
    uint8_t task_tag, uint8_t iid);
void ufshci_ctrlr_cmd_send_nop(struct ufshci_controller *ctrlr,
    ufshci_cb_fn_t cb_fn, void *cb_arg);
void ufshci_ctrlr_cmd_send_query_request(struct ufshci_controller *ctrlr,
    ufshci_cb_fn_t cb_fn, void *cb_arg, struct ufshci_query_param param);
void ufshci_ctrlr_cmd_send_scsi_command(struct ufshci_controller *ctrlr,
    ufshci_cb_fn_t cb_fn, void *cb_arg, uint8_t *cmd_ptr, uint8_t cmd_len,
    uint32_t data_len, uint8_t lun, bool is_write);

/* Request Queue */
bool ufshci_req_queue_process_completions(struct ufshci_req_queue *req_queue);
int ufshci_utmr_req_queue_construct(struct ufshci_controller *ctrlr);
int ufshci_utr_req_queue_construct(struct ufshci_controller *ctrlr);
void ufshci_utmr_req_queue_destroy(struct ufshci_controller *ctrlr);
void ufshci_utr_req_queue_destroy(struct ufshci_controller *ctrlr);
int ufshci_utmr_req_queue_enable(struct ufshci_controller *ctrlr);
int ufshci_utr_req_queue_enable(struct ufshci_controller *ctrlr);
void ufshci_req_queue_fail(struct ufshci_controller *ctrlr,
    struct ufshci_hw_queue *hwq);
int ufshci_req_queue_submit_request(struct ufshci_req_queue *req_queue,
    struct ufshci_request *req, bool is_admin);
void ufshci_req_queue_complete_tracker(struct ufshci_tracker *tr);

/* Request Single Doorbell Queue */
int ufshci_req_sdb_construct(struct ufshci_controller *ctrlr,
    struct ufshci_req_queue *req_queue, uint32_t num_entries,
    bool is_task_mgmt);
void ufshci_req_sdb_destroy(struct ufshci_controller *ctrlr,
    struct ufshci_req_queue *req_queue);
struct ufshci_hw_queue *ufshci_req_sdb_get_hw_queue(
    struct ufshci_req_queue *req_queue);
int ufshci_req_sdb_enable(struct ufshci_controller *ctrlr,
    struct ufshci_req_queue *req_queue);
int ufshci_req_sdb_reserve_slot(struct ufshci_req_queue *req_queue,
    struct ufshci_tracker **tr);
void ufshci_req_sdb_utmr_ring_doorbell(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr);
void ufshci_req_sdb_utr_ring_doorbell(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr);
bool ufshci_req_sdb_utmr_is_doorbell_cleared(struct ufshci_controller *ctrlr,
    uint8_t slot);
bool ufshci_req_sdb_utr_is_doorbell_cleared(struct ufshci_controller *ctrlr,
    uint8_t slot);
void ufshci_req_sdb_utmr_clear_cpl_ntf(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr);
void ufshci_req_sdb_utr_clear_cpl_ntf(struct ufshci_controller *ctrlr,
    struct ufshci_tracker *tr);
bool ufshci_req_sdb_process_cpl(struct ufshci_req_queue *req_queue);
int ufshci_req_sdb_get_inflight_io(struct ufshci_controller *ctrlr);

/* UIC Command */
int ufshci_uic_power_mode_ready(struct ufshci_controller *ctrlr);
int ufshci_uic_cmd_ready(struct ufshci_controller *ctrlr);
int ufshci_uic_send_dme_link_startup(struct ufshci_controller *ctrlr);
int ufshci_uic_send_dme_get(struct ufshci_controller *ctrlr, uint16_t attribute,
    uint32_t *return_value);
int ufshci_uic_send_dme_set(struct ufshci_controller *ctrlr, uint16_t attribute,
    uint32_t value);
int ufshci_uic_send_dme_peer_get(struct ufshci_controller *ctrlr,
    uint16_t attribute, uint32_t *return_value);
int ufshci_uic_send_dme_peer_set(struct ufshci_controller *ctrlr,
    uint16_t attribute, uint32_t value);
int ufshci_uic_send_dme_endpoint_reset(struct ufshci_controller *ctrlr);

/* SYSCTL */
void ufshci_sysctl_initialize_ctrlr(struct ufshci_controller *ctrlr);

int ufshci_attach(device_t dev);
int ufshci_detach(device_t dev);

/*
 * Wait for a command to complete using the ufshci_completion_poll_cb. Used in
 * limited contexts where the caller knows it's OK to block briefly while the
 * command runs. The ISR will run the callback which will set status->done to
 * true, usually within microseconds. If not, then after one second timeout
 * handler should reset the controller and abort all outstanding requests
 * including this polled one. If still not after ten seconds, then something is
 * wrong with the driver, and panic is the only way to recover.
 *
 * Most commands using this interface aren't actual I/O to the drive's media so
 * complete within a few microseconds. Adaptively spin for one tick to catch the
 * vast majority of these without waiting for a tick plus scheduling delays.
 * Since these are on startup, this drastically reduces startup time.
 */
static __inline void
ufshci_completion_poll(struct ufshci_completion_poll_status *status)
{
	int timeout = ticks + 10 * hz;
	sbintime_t delta_t = SBT_1US;

	while (!atomic_load_acq_int(&status->done)) {
		if (timeout - ticks < 0)
			panic(
			    "UFSHCI polled command failed to complete within 10s.");
		pause_sbt("ufshci_cpl", delta_t, 0, C_PREL(1));
		delta_t = min(SBT_1MS, delta_t * 3 / 2);
	}
}

static __inline void
ufshci_single_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	uint64_t *bus_addr = (uint64_t *)arg;

	KASSERT(nseg == 1, ("number of segments (%d) is not 1", nseg));
	if (error != 0)
		printf("ufshci_single_map err %d\n", error);
	*bus_addr = seg[0].ds_addr;
}

static __inline struct ufshci_request *
_ufshci_allocate_request(const int how, ufshci_cb_fn_t cb_fn, void *cb_arg)
{
	struct ufshci_request *req;

	KASSERT(how == M_WAITOK || how == M_NOWAIT,
	    ("nvme_allocate_request: invalid how %d", how));

	req = malloc(sizeof(*req), M_UFSHCI, how | M_ZERO);
	if (req != NULL) {
		req->cb_fn = cb_fn;
		req->cb_arg = cb_arg;
		req->timeout = true;
	}
	return (req);
}

static __inline struct ufshci_request *
ufshci_allocate_request_vaddr(void *payload, uint32_t payload_size,
    const int how, ufshci_cb_fn_t cb_fn, void *cb_arg)
{
	struct ufshci_request *req;

	req = _ufshci_allocate_request(how, cb_fn, cb_arg);
	if (req != NULL) {
		if (payload_size) {
			req->payload = memdesc_vaddr(payload, payload_size);
			req->payload_valid = true;
		}
	}
	return (req);
}

static __inline struct ufshci_request *
ufshci_allocate_request_bio(struct bio *bio, const int how,
    ufshci_cb_fn_t cb_fn, void *cb_arg)
{
	struct ufshci_request *req;

	req = _ufshci_allocate_request(how, cb_fn, cb_arg);
	if (req != NULL) {
		req->payload = memdesc_bio(bio);
		req->payload_valid = true;
	}
	return (req);
}

#define ufshci_free_request(req) free(req, M_UFSHCI)

void ufshci_ctrlr_shared_handler(void *arg);

#endif /* __UFSHCI_PRIVATE_H__ */
