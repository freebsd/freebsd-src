/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2014 Intel Corporation
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

#ifndef __NVME_PRIVATE_H__
#define __NVME_PRIVATE_H__

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <vm/uma.h>

#include <machine/bus.h>

#include "nvme.h"

#define DEVICE2SOFTC(dev) ((struct nvme_controller *) device_get_softc(dev))

MALLOC_DECLARE(M_NVME);

#define IDT32_PCI_ID		0x80d0111d /* 32 channel board */
#define IDT8_PCI_ID		0x80d2111d /* 8 channel board */

#define NVME_ADMIN_TRACKERS	(16)
#define NVME_ADMIN_ENTRIES	(128)
/* min and max are defined in admin queue attributes section of spec */
#define NVME_MIN_ADMIN_ENTRIES	(2)
#define NVME_MAX_ADMIN_ENTRIES	(4096)

/*
 * NVME_IO_ENTRIES defines the size of an I/O qpair's submission and completion
 *  queues, while NVME_IO_TRACKERS defines the maximum number of I/O that we
 *  will allow outstanding on an I/O qpair at any time.  The only advantage in
 *  having IO_ENTRIES > IO_TRACKERS is for debugging purposes - when dumping
 *  the contents of the submission and completion queues, it will show a longer
 *  history of data.
 */
#define NVME_IO_ENTRIES		(256)
#define NVME_IO_TRACKERS	(128)
#define NVME_MIN_IO_TRACKERS	(4)
#define NVME_MAX_IO_TRACKERS	(1024)

/*
 * NVME_MAX_IO_ENTRIES is not defined, since it is specified in CC.MQES
 *  for each controller.
 */

#define NVME_INT_COAL_TIME	(0)	/* disabled */
#define NVME_INT_COAL_THRESHOLD (0)	/* 0-based */

#define NVME_MAX_NAMESPACES	(16)
#define NVME_MAX_CONSUMERS	(2)
#define NVME_MAX_ASYNC_EVENTS	(8)

#define NVME_DEFAULT_TIMEOUT_PERIOD	(30)    /* in seconds */
#define NVME_MIN_TIMEOUT_PERIOD		(5)
#define NVME_MAX_TIMEOUT_PERIOD		(120)

#define NVME_DEFAULT_RETRY_COUNT	(4)

/* Maximum log page size to fetch for AERs. */
#define NVME_MAX_AER_LOG_SIZE		(4096)

/*
 * Define CACHE_LINE_SIZE here for older FreeBSD versions that do not define
 *  it.
 */
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE		(64)
#endif

#define NVME_GONE		0xfffffffful

extern int32_t		nvme_retry_count;
extern bool		nvme_verbose_cmd_dump;

struct nvme_completion_poll_status {
	struct nvme_completion	cpl;
	int			done;
};

struct nvme_request {
	struct nvme_command		cmd;
	struct nvme_qpair		*qpair;
	struct memdesc			payload;
	bool				payload_valid;
	bool				timeout;
	nvme_cb_fn_t			cb_fn;
	void				*cb_arg;
	int32_t				retries;
	STAILQ_ENTRY(nvme_request)	stailq;
};

struct nvme_async_event_request {
	struct nvme_controller		*ctrlr;
	struct nvme_request		*req;
	struct nvme_completion		cpl;
	uint32_t			log_page_id;
	uint32_t			log_page_size;
	uint8_t				log_page_buffer[NVME_MAX_AER_LOG_SIZE];
};

struct nvme_tracker {
	TAILQ_ENTRY(nvme_tracker)	tailq;
	struct nvme_request		*req;
	struct nvme_qpair		*qpair;
	sbintime_t			deadline;
	bus_dmamap_t			payload_dma_map;
	uint16_t			cid;

	uint64_t			*prp;
	bus_addr_t			prp_bus_addr;
};

enum nvme_recovery {
	RECOVERY_NONE = 0,		/* Normal operations */
	RECOVERY_START,			/* Deadline has passed, start recovering */
	RECOVERY_RESET,			/* This pass, initiate reset of controller */
	RECOVERY_WAITING,		/* waiting for the reset to complete */
};
struct nvme_qpair {
	struct nvme_controller	*ctrlr;
	uint32_t		id;
	int			domain;
	int			cpu;

	uint16_t		vector;
	int			rid;
	struct resource		*res;
	void 			*tag;

	struct callout		timer;
	sbintime_t		deadline;
	bool			timer_armed;
	enum nvme_recovery	recovery_state;

	uint32_t		num_entries;
	uint32_t		num_trackers;
	uint32_t		sq_tdbl_off;
	uint32_t		cq_hdbl_off;

	uint32_t		phase;
	uint32_t		sq_head;
	uint32_t		sq_tail;
	uint32_t		cq_head;

	int64_t			num_cmds;
	int64_t			num_intr_handler_calls;
	int64_t			num_retries;
	int64_t			num_failures;
	int64_t			num_ignored;

	struct nvme_command	*cmd;
	struct nvme_completion	*cpl;

	bus_dma_tag_t		dma_tag;
	bus_dma_tag_t		dma_tag_payload;

	bus_dmamap_t		queuemem_map;
	uint64_t		cmd_bus_addr;
	uint64_t		cpl_bus_addr;

	TAILQ_HEAD(, nvme_tracker)	free_tr;
	TAILQ_HEAD(, nvme_tracker)	outstanding_tr;
	STAILQ_HEAD(, nvme_request)	queued_req;

	struct nvme_tracker	**act_tr;

	struct mtx		lock __aligned(CACHE_LINE_SIZE);

} __aligned(CACHE_LINE_SIZE);

struct nvme_namespace {
	struct nvme_controller		*ctrlr;
	struct nvme_namespace_data	data;
	uint32_t			id;
	uint32_t			flags;
	struct cdev			*cdev;
	void				*cons_cookie[NVME_MAX_CONSUMERS];
	uint32_t			boundary;
	struct mtx			lock;
};

/*
 * One of these per allocated PCI device.
 */
struct nvme_controller {
	device_t		dev;

	struct mtx		lock;
	int			domain;
	uint32_t		ready_timeout_in_ms;
	uint32_t		quirks;
#define	QUIRK_DELAY_B4_CHK_RDY	1		/* Can't touch MMIO on disable */
#define	QUIRK_DISABLE_TIMEOUT	2		/* Disable broken completion timeout feature */
#define	QUIRK_INTEL_ALIGNMENT	4		/* Pre NVMe 1.3 performance alignment */
#define QUIRK_AHCI		8		/* Attached via AHCI redirect */

	bus_space_tag_t		bus_tag;
	bus_space_handle_t	bus_handle;
	int			resource_id;
	struct resource		*resource;

	/*
	 * The NVMe spec allows for the MSI-X table to be placed in BAR 4/5,
	 *  separate from the control registers which are in BAR 0/1.  These
	 *  members track the mapping of BAR 4/5 for that reason.
	 */
	int			bar4_resource_id;
	struct resource		*bar4_resource;

	int			msi_count;
	uint32_t		enable_aborts;

	uint32_t		num_io_queues;
	uint32_t		max_hw_pend_io;

	/* Fields for tracking progress during controller initialization. */
	struct intr_config_hook	config_hook;
	uint32_t		ns_identified;
	uint32_t		queues_created;

	struct task		reset_task;
	struct task		fail_req_task;
	struct taskqueue	*taskqueue;

	/* For shared legacy interrupt. */
	int			rid;
	struct resource		*res;
	void			*tag;

	/** maximum i/o size in bytes */
	uint32_t		max_xfer_size;

	/** LO and HI capacity mask */
	uint32_t		cap_lo;
	uint32_t		cap_hi;

	/** Page size and log2(page_size) - 12 that we're currently using */
	uint32_t		page_size;
	uint32_t		mps;

	/** interrupt coalescing time period (in microseconds) */
	uint32_t		int_coal_time;

	/** interrupt coalescing threshold */
	uint32_t		int_coal_threshold;

	/** timeout period in seconds */
	uint32_t		timeout_period;

	/** doorbell stride */
	uint32_t		dstrd;

	struct nvme_qpair	adminq;
	struct nvme_qpair	*ioq;

	struct nvme_registers		*regs;

	struct nvme_controller_data	cdata;
	struct nvme_namespace		ns[NVME_MAX_NAMESPACES];

	struct cdev			*cdev;

	/** bit mask of event types currently enabled for async events */
	uint32_t			async_event_config;

	uint32_t			num_aers;
	struct nvme_async_event_request	aer[NVME_MAX_ASYNC_EVENTS];

	void				*cons_cookie[NVME_MAX_CONSUMERS];

	uint32_t			is_resetting;
	uint32_t			is_initialized;
	uint32_t			notification_sent;

	bool				is_failed;
	bool				is_dying;
	STAILQ_HEAD(, nvme_request)	fail_req;

	/* Host Memory Buffer */
	int				hmb_nchunks;
	size_t				hmb_chunk;
	bus_dma_tag_t			hmb_tag;
	struct nvme_hmb_chunk {
		bus_dmamap_t		hmbc_map;
		void			*hmbc_vaddr;
		uint64_t		hmbc_paddr;
	} *hmb_chunks;
	bus_dma_tag_t			hmb_desc_tag;
	bus_dmamap_t			hmb_desc_map;
	struct nvme_hmb_desc		*hmb_desc_vaddr;
	uint64_t			hmb_desc_paddr;
};

#define nvme_mmio_offsetof(reg)						       \
	offsetof(struct nvme_registers, reg)

#define nvme_mmio_read_4(sc, reg)					       \
	bus_space_read_4((sc)->bus_tag, (sc)->bus_handle,		       \
	    nvme_mmio_offsetof(reg))

#define nvme_mmio_write_4(sc, reg, val)					       \
	bus_space_write_4((sc)->bus_tag, (sc)->bus_handle,		       \
	    nvme_mmio_offsetof(reg), val)

#define nvme_mmio_write_8(sc, reg, val)					       \
	do {								       \
		bus_space_write_4((sc)->bus_tag, (sc)->bus_handle,	       \
		    nvme_mmio_offsetof(reg), val & 0xFFFFFFFF); 	       \
		bus_space_write_4((sc)->bus_tag, (sc)->bus_handle,	       \
		    nvme_mmio_offsetof(reg)+4,				       \
		    (val & 0xFFFFFFFF00000000ULL) >> 32);		       \
	} while (0);

#define nvme_printf(ctrlr, fmt, args...)	\
    device_printf(ctrlr->dev, fmt, ##args)

void	nvme_ns_test(struct nvme_namespace *ns, u_long cmd, caddr_t arg);

void	nvme_ctrlr_cmd_identify_controller(struct nvme_controller *ctrlr,
					   void *payload,
					   nvme_cb_fn_t cb_fn, void *cb_arg);
void	nvme_ctrlr_cmd_identify_namespace(struct nvme_controller *ctrlr,
					  uint32_t nsid, void *payload,
					  nvme_cb_fn_t cb_fn, void *cb_arg);
void	nvme_ctrlr_cmd_set_interrupt_coalescing(struct nvme_controller *ctrlr,
						uint32_t microseconds,
						uint32_t threshold,
						nvme_cb_fn_t cb_fn,
						void *cb_arg);
void	nvme_ctrlr_cmd_get_error_page(struct nvme_controller *ctrlr,
				      struct nvme_error_information_entry *payload,
				      uint32_t num_entries, /* 0 = max */
				      nvme_cb_fn_t cb_fn,
				      void *cb_arg);
void	nvme_ctrlr_cmd_get_health_information_page(struct nvme_controller *ctrlr,
						   uint32_t nsid,
						   struct nvme_health_information_page *payload,
						   nvme_cb_fn_t cb_fn,
						   void *cb_arg);
void	nvme_ctrlr_cmd_get_firmware_page(struct nvme_controller *ctrlr,
					 struct nvme_firmware_page *payload,
					 nvme_cb_fn_t cb_fn,
					 void *cb_arg);
void	nvme_ctrlr_cmd_create_io_cq(struct nvme_controller *ctrlr,
				    struct nvme_qpair *io_que,
				    nvme_cb_fn_t cb_fn, void *cb_arg);
void	nvme_ctrlr_cmd_create_io_sq(struct nvme_controller *ctrlr,
				    struct nvme_qpair *io_que,
				    nvme_cb_fn_t cb_fn, void *cb_arg);
void	nvme_ctrlr_cmd_delete_io_cq(struct nvme_controller *ctrlr,
				    struct nvme_qpair *io_que,
				    nvme_cb_fn_t cb_fn, void *cb_arg);
void	nvme_ctrlr_cmd_delete_io_sq(struct nvme_controller *ctrlr,
				    struct nvme_qpair *io_que,
				    nvme_cb_fn_t cb_fn, void *cb_arg);
void	nvme_ctrlr_cmd_set_num_queues(struct nvme_controller *ctrlr,
				      uint32_t num_queues, nvme_cb_fn_t cb_fn,
				      void *cb_arg);
void	nvme_ctrlr_cmd_set_async_event_config(struct nvme_controller *ctrlr,
					      uint32_t state,
					      nvme_cb_fn_t cb_fn, void *cb_arg);
void	nvme_ctrlr_cmd_abort(struct nvme_controller *ctrlr, uint16_t cid,
			     uint16_t sqid, nvme_cb_fn_t cb_fn, void *cb_arg);

void	nvme_completion_poll_cb(void *arg, const struct nvme_completion *cpl);

int	nvme_ctrlr_construct(struct nvme_controller *ctrlr, device_t dev);
void	nvme_ctrlr_destruct(struct nvme_controller *ctrlr, device_t dev);
void	nvme_ctrlr_shutdown(struct nvme_controller *ctrlr);
void	nvme_ctrlr_reset(struct nvme_controller *ctrlr);
/* ctrlr defined as void * to allow use with config_intrhook. */
void	nvme_ctrlr_start_config_hook(void *ctrlr_arg);
void	nvme_ctrlr_submit_admin_request(struct nvme_controller *ctrlr,
					struct nvme_request *req);
void	nvme_ctrlr_submit_io_request(struct nvme_controller *ctrlr,
				     struct nvme_request *req);
void	nvme_ctrlr_post_failed_request(struct nvme_controller *ctrlr,
				       struct nvme_request *req);

int	nvme_qpair_construct(struct nvme_qpair *qpair,
			     uint32_t num_entries, uint32_t num_trackers,
			     struct nvme_controller *ctrlr);
void	nvme_qpair_submit_tracker(struct nvme_qpair *qpair,
				  struct nvme_tracker *tr);
bool	nvme_qpair_process_completions(struct nvme_qpair *qpair);
void	nvme_qpair_submit_request(struct nvme_qpair *qpair,
				  struct nvme_request *req);
void	nvme_qpair_reset(struct nvme_qpair *qpair);
void	nvme_qpair_fail(struct nvme_qpair *qpair);
void	nvme_qpair_manual_complete_request(struct nvme_qpair *qpair,
					   struct nvme_request *req,
                                           uint32_t sct, uint32_t sc);

void	nvme_admin_qpair_enable(struct nvme_qpair *qpair);
void	nvme_admin_qpair_disable(struct nvme_qpair *qpair);
void	nvme_admin_qpair_destroy(struct nvme_qpair *qpair);

void	nvme_io_qpair_enable(struct nvme_qpair *qpair);
void	nvme_io_qpair_disable(struct nvme_qpair *qpair);
void	nvme_io_qpair_destroy(struct nvme_qpair *qpair);

int	nvme_ns_construct(struct nvme_namespace *ns, uint32_t id,
			  struct nvme_controller *ctrlr);
void	nvme_ns_destruct(struct nvme_namespace *ns);

void	nvme_sysctl_initialize_ctrlr(struct nvme_controller *ctrlr);

void	nvme_qpair_print_command(struct nvme_qpair *qpair,
	    struct nvme_command *cmd);
void	nvme_qpair_print_completion(struct nvme_qpair *qpair,
	    struct nvme_completion *cpl);

int	nvme_attach(device_t dev);
int	nvme_shutdown(device_t dev);
int	nvme_detach(device_t dev);

/*
 * Wait for a command to complete using the nvme_completion_poll_cb.  Used in
 * limited contexts where the caller knows it's OK to block briefly while the
 * command runs. The ISR will run the callback which will set status->done to
 * true, usually within microseconds. If not, then after one second timeout
 * handler should reset the controller and abort all outstanding requests
 * including this polled one. If still not after ten seconds, then something is
 * wrong with the driver, and panic is the only way to recover.
 *
 * Most commands using this interface aren't actual I/O to the drive's media so
 * complete within a few microseconds. Adaptively spin for one tick to catch the
 * vast majority of these without waiting for a tick plus scheduling delays. Since
 * these are on startup, this drastically reduces startup time.
 */
static __inline
void
nvme_completion_poll(struct nvme_completion_poll_status *status)
{
	int timeout = ticks + 10 * hz;
	sbintime_t delta_t = SBT_1US;

	while (!atomic_load_acq_int(&status->done)) {
		if (timeout - ticks < 0)
			panic("NVME polled command failed to complete within 10s.");
		pause_sbt("nvme", delta_t, 0, C_PREL(1));
		delta_t = min(SBT_1MS, delta_t * 3 / 2);
	}
}

static __inline void
nvme_single_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	uint64_t *bus_addr = (uint64_t *)arg;

	KASSERT(nseg == 1, ("number of segments (%d) is not 1", nseg));
	if (error != 0)
		printf("nvme_single_map err %d\n", error);
	*bus_addr = seg[0].ds_addr;
}

static __inline struct nvme_request *
_nvme_allocate_request(nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request *req;

	req = malloc(sizeof(*req), M_NVME, M_NOWAIT | M_ZERO);
	if (req != NULL) {
		req->cb_fn = cb_fn;
		req->cb_arg = cb_arg;
		req->timeout = true;
	}
	return (req);
}

static __inline struct nvme_request *
nvme_allocate_request_vaddr(void *payload, uint32_t payload_size,
    nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request *req;

	req = _nvme_allocate_request(cb_fn, cb_arg);
	if (req != NULL) {
		req->payload = memdesc_vaddr(payload, payload_size);
		req->payload_valid = true;
	}
	return (req);
}

static __inline struct nvme_request *
nvme_allocate_request_null(nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request *req;

	req = _nvme_allocate_request(cb_fn, cb_arg);
	return (req);
}

static __inline struct nvme_request *
nvme_allocate_request_bio(struct bio *bio, nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request *req;

	req = _nvme_allocate_request(cb_fn, cb_arg);
	if (req != NULL) {
		req->payload = memdesc_bio(bio);
		req->payload_valid = true;
	}
	return (req);
}

static __inline struct nvme_request *
nvme_allocate_request_ccb(union ccb *ccb, nvme_cb_fn_t cb_fn, void *cb_arg)
{
	struct nvme_request *req;

	req = _nvme_allocate_request(cb_fn, cb_arg);
	if (req != NULL) {
		req->payload = memdesc_ccb(ccb);
		req->payload_valid = true;
	}

	return (req);
}

#define nvme_free_request(req)	free(req, M_NVME)

void	nvme_notify_async_consumers(struct nvme_controller *ctrlr,
				    const struct nvme_completion *async_cpl,
				    uint32_t log_page_id, void *log_page_buffer,
				    uint32_t log_page_size);
void	nvme_notify_fail_consumers(struct nvme_controller *ctrlr);
void	nvme_notify_new_controller(struct nvme_controller *ctrlr);
void	nvme_notify_ns(struct nvme_controller *ctrlr, int nsid);

void	nvme_ctrlr_shared_handler(void *arg);
void	nvme_ctrlr_poll(struct nvme_controller *ctrlr);

int	nvme_ctrlr_suspend(struct nvme_controller *ctrlr);
int	nvme_ctrlr_resume(struct nvme_controller *ctrlr);

#endif /* __NVME_PRIVATE_H__ */
