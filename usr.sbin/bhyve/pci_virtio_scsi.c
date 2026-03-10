/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Jakub Klama <jceel@FreeBSD.org>.
 * Copyright (c) 2018 Marcelo Araujo <araujo@FreeBSD.org>.
 * Copyright (c) 2026 Hans Rosenfeld
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 */

#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/sbuf.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <pthread_np.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_scsi_all.h>
#include <camlib.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "pci_emul.h"
#include "virtio.h"
#include "iov.h"

#define VTSCSI_RINGSZ		64
#define	VTSCSI_REQUESTQ		1
#define	VTSCSI_THR_PER_Q	16
#define	VTSCSI_MAXQ		(VTSCSI_REQUESTQ + 2)
#define	VTSCSI_MAXSEG		64

#define	VTSCSI_IN_HEADER_LEN(_sc)	\
	(sizeof(struct pci_vtscsi_req_cmd_rd) + _sc->vss_config.cdb_size)

#define	VTSCSI_OUT_HEADER_LEN(_sc) 	\
	(sizeof(struct pci_vtscsi_req_cmd_wr) + _sc->vss_config.sense_size)

#define	VIRTIO_SCSI_MAX_CHANNEL	0
#define	VIRTIO_SCSI_MAX_TARGET	0
#define	VIRTIO_SCSI_MAX_LUN	16383

#define	VIRTIO_SCSI_F_INOUT	(1 << 0)
#define	VIRTIO_SCSI_F_HOTPLUG	(1 << 1)
#define	VIRTIO_SCSI_F_CHANGE	(1 << 2)

static int pci_vtscsi_debug = 0;
#define	WPRINTF(msg, params...) PRINTLN("virtio-scsi: " msg, ##params)
#define	DPRINTF(msg, params...) if (pci_vtscsi_debug) WPRINTF(msg, ##params)

struct pci_vtscsi_config {
	uint32_t num_queues;
	uint32_t seg_max;
	uint32_t max_sectors;
	uint32_t cmd_per_lun;
	uint32_t event_info_size;
	uint32_t sense_size;
	uint32_t cdb_size;
	uint16_t max_channel;
	uint16_t max_target;
	uint32_t max_lun;
} __attribute__((packed));

/*
 * I/O request state and I/O request queues
 *
 * In addition to the control queue and notification queues, each virtio-scsi
 * device instance has at least one I/O request queue, the state of which is
 * is kept in an array of struct pci_vtscsi_queue in the device softc.
 *
 * Currently there is only one I/O request queue, but it's trivial to support
 * more than one.
 *
 * Each pci_vtscsi_queue has VTSCSI_RINGSZ pci_vtscsi_request structures pre-
 * allocated on vsq_free_requests. For each I/O request coming in on the I/O
 * virtqueue, the request queue handler will take a pci_vtscsi_request off
 * vsq_free_requests, fills in the data from the I/O virtqueue, puts it on
 * vsq_requests, and signals vsq_cv.
 *
 * There are VTSCSI_THR_PER_Q worker threads for each pci_vtscsi_queue which
 * wait on vsq_cv. When signalled, they repeatedly take one pci_vtscsi_request
 * off vsq_requests, construct a ctl_io for it, and hand it off to the CTL ioctl
 * Interface, which processes it synchronously. After completion of the request,
 * the pci_vtscsi_request is re-initialized and put back onto vsq_free_requests.
 *
 * The worker threads exit when vsq_cv is signalled after vsw_exiting was set.
 *
 * There are three mutexes to coordinate the accesses to an I/O request queue:
 * - vsq_rmtx protects vsq_requests and must be held when waiting on vsq_cv
 * - vsq_fmtx protects vsq_free_requests
 * - vsq_qmtx must be held when operating on the underlying virtqueue, vsq_vq
 */
STAILQ_HEAD(pci_vtscsi_req_queue, pci_vtscsi_request);

struct pci_vtscsi_queue {
	struct pci_vtscsi_softc *         vsq_sc;
	struct vqueue_info *              vsq_vq;
	pthread_mutex_t                   vsq_rmtx;
	pthread_mutex_t                   vsq_fmtx;
	pthread_mutex_t                   vsq_qmtx;
	pthread_cond_t                    vsq_cv;
	struct pci_vtscsi_req_queue       vsq_requests;
	struct pci_vtscsi_req_queue       vsq_free_requests;
	LIST_HEAD(, pci_vtscsi_worker)    vsq_workers;
};

struct pci_vtscsi_worker {
	struct pci_vtscsi_queue *     vsw_queue;
	pthread_t                     vsw_thread;
	bool                          vsw_exiting;
	LIST_ENTRY(pci_vtscsi_worker) vsw_link;
};

struct pci_vtscsi_request {
	struct pci_vtscsi_queue * vsr_queue;
	struct iovec              vsr_iov[VTSCSI_MAXSEG + SPLIT_IOV_ADDL_IOV];
	struct iovec *            vsr_iov_in;
	struct iovec *            vsr_iov_out;
	struct iovec *            vsr_data_iov_in;
	struct iovec *            vsr_data_iov_out;
	struct pci_vtscsi_req_cmd_rd * vsr_cmd_rd;
	struct pci_vtscsi_req_cmd_wr * vsr_cmd_wr;
	union ctl_io *            vsr_ctl_io;
	size_t                    vsr_niov_in;
	size_t                    vsr_niov_out;
	size_t                    vsr_data_niov_in;
	size_t                    vsr_data_niov_out;
	uint32_t                  vsr_idx;
	STAILQ_ENTRY(pci_vtscsi_request) vsr_link;
};

/*
 * Per-device softc
 */
struct pci_vtscsi_softc {
	struct virtio_softc      vss_vs;
	struct vqueue_info       vss_vq[VTSCSI_MAXQ];
	struct pci_vtscsi_queue  vss_queues[VTSCSI_REQUESTQ];
	pthread_mutex_t          vss_mtx;
	int                      vss_iid;
	int                      vss_ctl_fd;
	uint32_t                 vss_features;
	struct pci_vtscsi_config vss_config;
};

#define	VIRTIO_SCSI_T_TMF			0
#define	VIRTIO_SCSI_T_TMF_ABORT_TASK		0
#define	VIRTIO_SCSI_T_TMF_ABORT_TASK_SET	1
#define	VIRTIO_SCSI_T_TMF_CLEAR_ACA		2
#define	VIRTIO_SCSI_T_TMF_CLEAR_TASK_SET	3
#define	VIRTIO_SCSI_T_TMF_I_T_NEXUS_RESET	4
#define	VIRTIO_SCSI_T_TMF_LOGICAL_UNIT_RESET	5
#define	VIRTIO_SCSI_T_TMF_QUERY_TASK		6
#define	VIRTIO_SCSI_T_TMF_QUERY_TASK_SET 	7

/* command-specific response values */
#define	VIRTIO_SCSI_S_FUNCTION_COMPLETE		0
#define	VIRTIO_SCSI_S_FUNCTION_SUCCEEDED	10
#define	VIRTIO_SCSI_S_FUNCTION_REJECTED		11

struct pci_vtscsi_ctrl_tmf {
	const uint32_t type;
	const uint32_t subtype;
	const uint8_t lun[8];
	const uint64_t id;
	uint8_t response;
} __attribute__((packed));

#define	VIRTIO_SCSI_T_AN_QUERY			1
#define	VIRTIO_SCSI_EVT_ASYNC_OPERATIONAL_CHANGE 2
#define	VIRTIO_SCSI_EVT_ASYNC_POWER_MGMT	4
#define	VIRTIO_SCSI_EVT_ASYNC_EXTERNAL_REQUEST	8
#define	VIRTIO_SCSI_EVT_ASYNC_MEDIA_CHANGE	16
#define	VIRTIO_SCSI_EVT_ASYNC_MULTI_HOST	32
#define	VIRTIO_SCSI_EVT_ASYNC_DEVICE_BUSY	64

struct pci_vtscsi_ctrl_an {
	const uint32_t type;
	const uint8_t lun[8];
	const uint32_t event_requested;
	uint32_t event_actual;
	uint8_t response;
} __attribute__((packed));

/* command-specific response values */
#define	VIRTIO_SCSI_S_OK 			0
#define	VIRTIO_SCSI_S_OVERRUN			1
#define	VIRTIO_SCSI_S_ABORTED			2
#define	VIRTIO_SCSI_S_BAD_TARGET		3
#define	VIRTIO_SCSI_S_RESET			4
#define	VIRTIO_SCSI_S_BUSY			5
#define	VIRTIO_SCSI_S_TRANSPORT_FAILURE		6
#define	VIRTIO_SCSI_S_TARGET_FAILURE		7
#define	VIRTIO_SCSI_S_NEXUS_FAILURE		8
#define	VIRTIO_SCSI_S_FAILURE			9
#define	VIRTIO_SCSI_S_INCORRECT_LUN		12

/* task_attr */
#define	VIRTIO_SCSI_S_SIMPLE			0
#define	VIRTIO_SCSI_S_ORDERED			1
#define	VIRTIO_SCSI_S_HEAD			2
#define	VIRTIO_SCSI_S_ACA			3

struct pci_vtscsi_event {
	uint32_t event;
	uint8_t lun[8];
	uint32_t reason;
} __attribute__((packed));

struct pci_vtscsi_req_cmd_rd {
	const uint8_t lun[8];
	const uint64_t id;
	const uint8_t task_attr;
	const uint8_t prio;
	const uint8_t crn;
	const uint8_t cdb[];
} __attribute__((packed));

struct pci_vtscsi_req_cmd_wr {
	uint32_t sense_len;
	uint32_t residual;
	uint16_t status_qualifier;
	uint8_t status;
	uint8_t response;
	uint8_t sense[];
} __attribute__((packed));

static void *pci_vtscsi_proc(void *);
static void pci_vtscsi_reset(void *);
static void pci_vtscsi_neg_features(void *, uint64_t);
static int pci_vtscsi_cfgread(void *, int, int, uint32_t *);
static int pci_vtscsi_cfgwrite(void *, int, int, uint32_t);

static inline bool pci_vtscsi_check_lun(const uint8_t *);
static inline int pci_vtscsi_get_lun(const uint8_t *);

static void pci_vtscsi_control_handle(struct pci_vtscsi_softc *, void *, size_t);
static void pci_vtscsi_tmf_handle(struct pci_vtscsi_softc *,
    struct pci_vtscsi_ctrl_tmf *);
static void pci_vtscsi_an_handle(struct pci_vtscsi_softc *,
    struct pci_vtscsi_ctrl_an *);

static struct pci_vtscsi_request *pci_vtscsi_alloc_request(
    struct pci_vtscsi_softc *);
static void pci_vtscsi_free_request(struct pci_vtscsi_request *);
static struct pci_vtscsi_request *pci_vtscsi_get_request(
    struct pci_vtscsi_req_queue *);
static void pci_vtscsi_put_request(struct pci_vtscsi_req_queue *,
    struct pci_vtscsi_request *);
static void pci_vtscsi_queue_request(struct pci_vtscsi_softc *,
    struct vqueue_info *);
static void pci_vtscsi_return_request(struct pci_vtscsi_queue *,
    struct pci_vtscsi_request *, int);
static int pci_vtscsi_request_handle(struct pci_vtscsi_softc *,
    struct pci_vtscsi_request *);

static void pci_vtscsi_controlq_notify(void *, struct vqueue_info *);
static void pci_vtscsi_eventq_notify(void *, struct vqueue_info *);
static void pci_vtscsi_requestq_notify(void *, struct vqueue_info *);
static int  pci_vtscsi_init_queue(struct pci_vtscsi_softc *,
    struct pci_vtscsi_queue *, int);
static void pci_vtscsi_destroy_queue(struct pci_vtscsi_queue *);
static int pci_vtscsi_init(struct pci_devinst *, nvlist_t *);

static struct virtio_consts vtscsi_vi_consts = {
	.vc_name =	"vtscsi",
	.vc_nvq =	VTSCSI_MAXQ,
	.vc_cfgsize =	sizeof(struct pci_vtscsi_config),
	.vc_reset =	pci_vtscsi_reset,
	.vc_cfgread =	pci_vtscsi_cfgread,
	.vc_cfgwrite =	pci_vtscsi_cfgwrite,
	.vc_apply_features = pci_vtscsi_neg_features,
	.vc_hv_caps =	VIRTIO_RING_F_INDIRECT_DESC,
};

static void *
pci_vtscsi_proc(void *arg)
{
	struct pci_vtscsi_worker *worker = (struct pci_vtscsi_worker *)arg;
	struct pci_vtscsi_queue *q = worker->vsw_queue;
	struct pci_vtscsi_softc *sc = q->vsq_sc;
	int iolen;

	for (;;) {
		struct pci_vtscsi_request *req;

		pthread_mutex_lock(&q->vsq_rmtx);

		while (STAILQ_EMPTY(&q->vsq_requests) && !worker->vsw_exiting)
			pthread_cond_wait(&q->vsq_cv, &q->vsq_rmtx);

		if (worker->vsw_exiting) {
			pthread_mutex_unlock(&q->vsq_rmtx);
			return (NULL);
		}

		req = pci_vtscsi_get_request(&q->vsq_requests);
		pthread_mutex_unlock(&q->vsq_rmtx);

		DPRINTF("I/O request lun %d, data_niov_in %zu, data_niov_out "
		    "%zu", pci_vtscsi_get_lun(req->vsr_cmd_rd->lun),
		    req->vsr_data_niov_in, req->vsr_data_niov_out);

		iolen = pci_vtscsi_request_handle(sc, req);

		pci_vtscsi_return_request(q, req, iolen);
	}
}

static void
pci_vtscsi_reset(void *vsc)
{
	struct pci_vtscsi_softc *sc;

	sc = vsc;

	DPRINTF("device reset requested");
	vi_reset_dev(&sc->vss_vs);

	/* initialize config structure */
	sc->vss_config = (struct pci_vtscsi_config){
		.num_queues = VTSCSI_REQUESTQ,
		/* Leave room for the request and the response. */
		.seg_max = VTSCSI_MAXSEG - 2,
		/* CTL apparently doesn't have a limit here */
		.max_sectors = INT32_MAX,
		.cmd_per_lun = 1,
		.event_info_size = sizeof(struct pci_vtscsi_event),
		.sense_size = 96,
		.cdb_size = 32,
		.max_channel = VIRTIO_SCSI_MAX_CHANNEL,
		.max_target = VIRTIO_SCSI_MAX_TARGET,
		.max_lun = VIRTIO_SCSI_MAX_LUN
	};
}

static void
pci_vtscsi_neg_features(void *vsc, uint64_t negotiated_features)
{
	struct pci_vtscsi_softc *sc = vsc;

	sc->vss_features = negotiated_features;
}

static int
pci_vtscsi_cfgread(void *vsc, int offset, int size, uint32_t *retval)
{
	struct pci_vtscsi_softc *sc = vsc;
	void *ptr;

	ptr = (uint8_t *)&sc->vss_config + offset;
	memcpy(retval, ptr, size);
	return (0);
}

static int
pci_vtscsi_cfgwrite(void *vsc __unused, int offset __unused, int size __unused,
    uint32_t val __unused)
{
	return (0);
}

/*
 * LUN address parsing
 *
 * The LUN address consists of 8 bytes. While the spec describes this as 0x01,
 * followed by the target byte, followed by a "single-level LUN structure",
 * this is actually the same as a hierarchical LUN address as defined by SAM-5,
 * consisting of four levels of addressing, where in each level the two MSB of
 * byte 0 select the address mode used in the remaining bits and bytes.
 *
 *
 * Only the first two levels are acutally used by virtio-scsi:
 *
 * Level 1: 0x01, 0xTT: Peripheral Device Addressing: Bus 1, Target 0-255
 * Level 2: 0xLL, 0xLL: Peripheral Device Addressing: Bus MBZ, LUN 0-255
 *                  or: Flat Space Addressing: LUN (0-16383)
 * Level 3 and 4: not used, MBZ
 *
 * Currently, we only support Target 0.
 *
 * Alternatively, the first level may contain an extended LUN address to select
 * the REPORT_LUNS well-known logical unit:
 *
 * Level 1: 0xC1, 0x01: Extended LUN Adressing, Well-Known LUN 1 (REPORT_LUNS)
 * Level 2, 3, and 4: not used, MBZ
 *
 * The virtio spec says that we SHOULD implement the REPORT_LUNS well-known
 * logical unit  but we currently don't.
 *
 * According to the virtio spec, these are the only LUNS address formats to be
 * used with virtio-scsi.
 */

/*
 * Check that the given LUN address conforms to the virtio spec, does not
 * address an unknown target, and especially does not address the REPORT_LUNS
 * well-known logical unit.
 */
static inline bool
pci_vtscsi_check_lun(const uint8_t *lun)
{
	if (lun[0] == 0xC1)
		return (false);

	if (lun[0] != 0x01)
		return (false);

	if (lun[1] != 0x00)
		return (false);

	if (lun[2] != 0x00 && (lun[2] & 0xc0) != 0x40)
		return (false);

	if (lun[4] != 0 || lun[5] != 0 || lun[6] != 0 || lun[7] != 0)
		return (false);

	return (true);
}

/*
 * Get the LUN id from a LUN address.
 *
 * Every code path using this function must have called pci_vtscsi_check_lun()
 * before to make sure the LUN address is valid.
 */
static inline int
pci_vtscsi_get_lun(const uint8_t *lun)
{
	assert(lun[0] == 0x01);
	assert(lun[1] == 0x00);
	assert(lun[2] == 0x00 || (lun[2] & 0xc0) == 0x40);

	return (((lun[2] << 8) | lun[3]) & 0x3fff);
}

static void
pci_vtscsi_control_handle(struct pci_vtscsi_softc *sc, void *buf,
    size_t bufsize)
{
	struct pci_vtscsi_ctrl_tmf *tmf;
	struct pci_vtscsi_ctrl_an *an;
	uint32_t type;

	if (bufsize < sizeof(uint32_t)) {
		WPRINTF("ignoring truncated control request");
		return;
	}

	type = *(uint32_t *)buf;

	if (type == VIRTIO_SCSI_T_TMF) {
		if (bufsize != sizeof(*tmf)) {
			WPRINTF("ignoring tmf request with size %zu", bufsize);
			return;
		}
		tmf = (struct pci_vtscsi_ctrl_tmf *)buf;
		pci_vtscsi_tmf_handle(sc, tmf);
	} else if (type == VIRTIO_SCSI_T_AN_QUERY) {
		if (bufsize != sizeof(*an)) {
			WPRINTF("ignoring AN request with size %zu", bufsize);
			return;
		}
		an = (struct pci_vtscsi_ctrl_an *)buf;
		pci_vtscsi_an_handle(sc, an);
	}
}

static void
pci_vtscsi_tmf_handle(struct pci_vtscsi_softc *sc,
    struct pci_vtscsi_ctrl_tmf *tmf)
{
	union ctl_io *io;
	int err;

	if (pci_vtscsi_check_lun(tmf->lun) == false) {
		DPRINTF("TMF request to invalid LUN %.2hhx%.2hhx-%.2hhx%.2hhx-"
		    "%.2hhx%.2hhx-%.2hhx%.2hhx", tmf->lun[0], tmf->lun[1],
		    tmf->lun[2], tmf->lun[3], tmf->lun[4], tmf->lun[5],
		    tmf->lun[6], tmf->lun[7]);

		tmf->response = VIRTIO_SCSI_S_BAD_TARGET;
		return;
	}

	io = ctl_scsi_alloc_io(sc->vss_iid);
	if (io == NULL) {
		WPRINTF("failed to allocate ctl_io: err=%d (%s)",
		    errno, strerror(errno));

		tmf->response = VIRTIO_SCSI_S_FAILURE;
		return;
	}

	ctl_scsi_zero_io(io);

	io->io_hdr.io_type = CTL_IO_TASK;
	io->io_hdr.nexus.initid = sc->vss_iid;
	io->io_hdr.nexus.targ_lun = pci_vtscsi_get_lun(tmf->lun);
	io->taskio.tag_type = CTL_TAG_SIMPLE;
	io->taskio.tag_num = tmf->id;
	io->io_hdr.flags |= CTL_FLAG_USER_TAG;

	switch (tmf->subtype) {
	case VIRTIO_SCSI_T_TMF_ABORT_TASK:
		io->taskio.task_action = CTL_TASK_ABORT_TASK;
		break;

	case VIRTIO_SCSI_T_TMF_ABORT_TASK_SET:
		io->taskio.task_action = CTL_TASK_ABORT_TASK_SET;
		break;

	case VIRTIO_SCSI_T_TMF_CLEAR_ACA:
		io->taskio.task_action = CTL_TASK_CLEAR_ACA;
		break;

	case VIRTIO_SCSI_T_TMF_CLEAR_TASK_SET:
		io->taskio.task_action = CTL_TASK_CLEAR_TASK_SET;
		break;

	case VIRTIO_SCSI_T_TMF_I_T_NEXUS_RESET:
		io->taskio.task_action = CTL_TASK_I_T_NEXUS_RESET;
		break;

	case VIRTIO_SCSI_T_TMF_LOGICAL_UNIT_RESET:
		io->taskio.task_action = CTL_TASK_LUN_RESET;
		break;

	case VIRTIO_SCSI_T_TMF_QUERY_TASK:
		io->taskio.task_action = CTL_TASK_QUERY_TASK;
		break;

	case VIRTIO_SCSI_T_TMF_QUERY_TASK_SET:
		io->taskio.task_action = CTL_TASK_QUERY_TASK_SET;
		break;
	}

	if (pci_vtscsi_debug) {
		struct sbuf *sb = sbuf_new_auto();
		ctl_io_sbuf(io, sb);
		sbuf_finish(sb);
		DPRINTF("%s", sbuf_data(sb));
		sbuf_delete(sb);
	}

	err = ioctl(sc->vss_ctl_fd, CTL_IO, io);
	if (err != 0)
		WPRINTF("CTL_IO: err=%d (%s)", errno, strerror(errno));

	tmf->response = io->taskio.task_status;
	ctl_scsi_free_io(io);
}

static void
pci_vtscsi_an_handle(struct pci_vtscsi_softc *sc __unused,
    struct pci_vtscsi_ctrl_an *an __unused)
{
}

static struct pci_vtscsi_request *
pci_vtscsi_alloc_request(struct pci_vtscsi_softc *sc)
{
	struct pci_vtscsi_request *req;

	req = calloc(1, sizeof(struct pci_vtscsi_request));
	if (req == NULL)
		goto fail;

	req->vsr_cmd_rd = calloc(1, VTSCSI_IN_HEADER_LEN(sc));
	if (req->vsr_cmd_rd == NULL)
		goto fail;
	req->vsr_cmd_wr = calloc(1, VTSCSI_OUT_HEADER_LEN(sc));
	if (req->vsr_cmd_wr == NULL)
		goto fail;

	req->vsr_ctl_io = ctl_scsi_alloc_io(sc->vss_iid);
	if (req->vsr_ctl_io == NULL)
		goto fail;
	ctl_scsi_zero_io(req->vsr_ctl_io);

	return (req);

fail:
	EPRINTLN("failed to allocate request: %s", strerror(errno));

	if (req != NULL)
		pci_vtscsi_free_request(req);

	return (NULL);
}

static void
pci_vtscsi_free_request(struct pci_vtscsi_request *req)
{
	if (req->vsr_ctl_io != NULL)
		ctl_scsi_free_io(req->vsr_ctl_io);
	if (req->vsr_cmd_rd != NULL)
		free(req->vsr_cmd_rd);
	if (req->vsr_cmd_wr != NULL)
		free(req->vsr_cmd_wr);

	free(req);
}

static struct pci_vtscsi_request *
pci_vtscsi_get_request(struct pci_vtscsi_req_queue *req_queue)
{
	struct pci_vtscsi_request *req;

	assert(!STAILQ_EMPTY(req_queue));

	req = STAILQ_FIRST(req_queue);
	STAILQ_REMOVE_HEAD(req_queue, vsr_link);

	return (req);
}

static void
pci_vtscsi_put_request(struct pci_vtscsi_req_queue *req_queue,
    struct pci_vtscsi_request *req)
{
	STAILQ_INSERT_TAIL(req_queue, req, vsr_link);
}

static void
pci_vtscsi_queue_request(struct pci_vtscsi_softc *sc, struct vqueue_info *vq)
{
	struct pci_vtscsi_queue *q = &sc->vss_queues[vq->vq_num - 2];
	struct pci_vtscsi_request *req;
	struct vi_req vireq;
	size_t res __maybe_unused;
	int n;

	pthread_mutex_lock(&q->vsq_fmtx);
	req = pci_vtscsi_get_request(&q->vsq_free_requests);
	assert(req != NULL);
	pthread_mutex_unlock(&q->vsq_fmtx);

	n = vq_getchain(vq, req->vsr_iov, VTSCSI_MAXSEG, &vireq);
	assert(n >= 1 && n <= VTSCSI_MAXSEG);

	req->vsr_idx = vireq.idx;
	req->vsr_queue = q;
	req->vsr_iov_in = &req->vsr_iov[0];
	req->vsr_niov_in = vireq.readable;
	req->vsr_iov_out = &req->vsr_iov[vireq.readable];
	req->vsr_niov_out = vireq.writable;

	/*
	 * Make sure we got at least enough space for the VirtIO-SCSI
	 * command headers. If not, return this request immediately.
	 */
	if (check_iov_len(req->vsr_iov_out, req->vsr_niov_out,
	    VTSCSI_OUT_HEADER_LEN(q->vsq_sc)) == false) {
		WPRINTF("ignoring request with insufficient output");
		req->vsr_cmd_wr->response = VIRTIO_SCSI_S_FAILURE;
		pci_vtscsi_return_request(q, req, 1);
		return;
	}

	if (check_iov_len(req->vsr_iov_in, req->vsr_niov_in,
	    VTSCSI_IN_HEADER_LEN(q->vsq_sc)) == false) {
		WPRINTF("ignoring request with incomplete header");
		req->vsr_cmd_wr->response = VIRTIO_SCSI_S_FAILURE;
		pci_vtscsi_return_request(q, req, 1);
		return;
	}

	/*
	 * We have to split the iovec array into a header and data portion each
	 * for input and output.
	 *
	 * We need to start with the output section (at the end of iov) in case
	 * the iovec covering the final part of the output header needs to be
	 * split, in which case split_iov() will move all reamaining iovecs up
	 * by one to make room for a new iovec covering the first part of the
	 * output data portion.
	 */
	req->vsr_data_iov_out = split_iov(req->vsr_iov_out, &req->vsr_niov_out,
	    VTSCSI_OUT_HEADER_LEN(q->vsq_sc), &req->vsr_data_niov_out);

	/*
	 * Similarly, to not overwrite the first iovec of the output section,
	 * the 2nd call to split_iov() to split the input section must actually
	 * cover the entire iovec array (both input and the already split output
	 * sections).
	 */
	req->vsr_niov_in += req->vsr_niov_out + req->vsr_data_niov_out;

	req->vsr_data_iov_in = split_iov(req->vsr_iov_in, &req->vsr_niov_in,
	    VTSCSI_IN_HEADER_LEN(q->vsq_sc), &req->vsr_data_niov_in);

	/*
	 * And of course we now have to adjust data_niov_in accordingly.
	 */
	req->vsr_data_niov_in -= req->vsr_niov_out + req->vsr_data_niov_out;

	/*
	 * iov_to_buf() realloc()s the buffer given as 3rd argument to the
	 * total size of all iovecs it will be copying. Since we've just
	 * truncated it in split_iov(), we know that the size will be
	 * VTSCSI_IN_HEADER_LEN(q->vsq_sc).
	 *
	 * Since we pre-allocated req->vsr_cmd_rd to this size, the realloc()
	 * should never fail.
	 *
	 * This will have to change if we begin allowing config space writes
	 * to change sense size.
	 */
	res = iov_to_buf(req->vsr_iov_in, req->vsr_niov_in,
	    (void **)&req->vsr_cmd_rd);
	assert(res == VTSCSI_IN_HEADER_LEN(q->vsq_sc));

	/* Make sure this request addresses a valid LUN. */
	if (pci_vtscsi_check_lun(req->vsr_cmd_rd->lun) == false) {
		DPRINTF("I/O request to invalid LUN "
		    "%.2hhx%.2hhx-%.2hhx%.2hhx-%.2hhx%.2hhx-%.2hhx%.2hhx",
		    req->vsr_cmd_rd->lun[0], req->vsr_cmd_rd->lun[1],
		    req->vsr_cmd_rd->lun[2], req->vsr_cmd_rd->lun[3],
		    req->vsr_cmd_rd->lun[4], req->vsr_cmd_rd->lun[5],
		    req->vsr_cmd_rd->lun[6], req->vsr_cmd_rd->lun[7]);
		req->vsr_cmd_wr->response = VIRTIO_SCSI_S_BAD_TARGET;
		pci_vtscsi_return_request(q, req, 1);
		return;
	}

	pthread_mutex_lock(&q->vsq_rmtx);
	pci_vtscsi_put_request(&q->vsq_requests, req);
	pthread_cond_signal(&q->vsq_cv);
	pthread_mutex_unlock(&q->vsq_rmtx);

	DPRINTF("request <idx=%d> enqueued", vireq.idx);
}

static void
pci_vtscsi_return_request(struct pci_vtscsi_queue *q,
    struct pci_vtscsi_request *req, int iolen)
{
	void *cmd_rd = req->vsr_cmd_rd;
	void *cmd_wr = req->vsr_cmd_wr;
	void *ctl_io = req->vsr_ctl_io;
	int idx = req->vsr_idx;

	DPRINTF("request <idx=%d> completed, response %d", idx,
	    req->vsr_cmd_wr->response);

	iolen += buf_to_iov(cmd_wr, VTSCSI_OUT_HEADER_LEN(q->vsq_sc),
	    req->vsr_iov_out, req->vsr_niov_out);

	ctl_scsi_zero_io(req->vsr_ctl_io);

	memset(cmd_rd, 0, VTSCSI_IN_HEADER_LEN(q->vsq_sc));
	memset(cmd_wr, 0, VTSCSI_OUT_HEADER_LEN(q->vsq_sc));
	memset(req, 0, sizeof(struct pci_vtscsi_request));

	req->vsr_cmd_rd = cmd_rd;
	req->vsr_cmd_wr = cmd_wr;
	req->vsr_ctl_io = ctl_io;

	pthread_mutex_lock(&q->vsq_fmtx);
	pci_vtscsi_put_request(&q->vsq_free_requests, req);
	pthread_mutex_unlock(&q->vsq_fmtx);

	pthread_mutex_lock(&q->vsq_qmtx);
	vq_relchain(q->vsq_vq, idx, iolen);
	vq_endchains(q->vsq_vq, 0);
	pthread_mutex_unlock(&q->vsq_qmtx);
}

static int
pci_vtscsi_request_handle(struct pci_vtscsi_softc *sc,
    struct pci_vtscsi_request *req)
{
	union ctl_io *io = req->vsr_ctl_io;
	void *ext_data_ptr = NULL;
	uint32_t ext_data_len = 0, ext_sg_entries = 0;
	int err, nxferred;

	io->io_hdr.nexus.initid = sc->vss_iid;
	io->io_hdr.nexus.targ_lun = pci_vtscsi_get_lun(req->vsr_cmd_rd->lun);

	io->io_hdr.io_type = CTL_IO_SCSI;

	if (req->vsr_data_niov_in > 0) {
		ext_data_ptr = (void *)req->vsr_data_iov_in;
		ext_sg_entries = req->vsr_data_niov_in;
		ext_data_len = count_iov(req->vsr_data_iov_in,
		    req->vsr_data_niov_in);
		io->io_hdr.flags |= CTL_FLAG_DATA_OUT;
	} else if (req->vsr_data_niov_out > 0) {
		ext_data_ptr = (void *)req->vsr_data_iov_out;
		ext_sg_entries = req->vsr_data_niov_out;
		ext_data_len = count_iov(req->vsr_data_iov_out,
		    req->vsr_data_niov_out);
		io->io_hdr.flags |= CTL_FLAG_DATA_IN;
	}

	io->scsiio.sense_len = sc->vss_config.sense_size;
	io->scsiio.tag_num = req->vsr_cmd_rd->id;
	io->io_hdr.flags |= CTL_FLAG_USER_TAG;
	switch (req->vsr_cmd_rd->task_attr) {
	case VIRTIO_SCSI_S_ORDERED:
		io->scsiio.tag_type = CTL_TAG_ORDERED;
		break;
	case VIRTIO_SCSI_S_HEAD:
		io->scsiio.tag_type = CTL_TAG_HEAD_OF_QUEUE;
		break;
	case VIRTIO_SCSI_S_ACA:
		io->scsiio.tag_type = CTL_TAG_ACA;
		break;
	case VIRTIO_SCSI_S_SIMPLE:
	default:
		io->scsiio.tag_type = CTL_TAG_SIMPLE;
		break;
	}
	io->scsiio.ext_sg_entries = ext_sg_entries;
	io->scsiio.ext_data_ptr = ext_data_ptr;
	io->scsiio.ext_data_len = ext_data_len;
	io->scsiio.ext_data_filled = 0;
	io->scsiio.cdb_len = sc->vss_config.cdb_size;
	memcpy(io->scsiio.cdb, req->vsr_cmd_rd->cdb, sc->vss_config.cdb_size);

	if (pci_vtscsi_debug) {
		struct sbuf *sb = sbuf_new_auto();
		ctl_io_sbuf(io, sb);
		sbuf_finish(sb);
		DPRINTF("%s", sbuf_data(sb));
		sbuf_delete(sb);
	}

	err = ioctl(sc->vss_ctl_fd, CTL_IO, io);
	if (err != 0) {
		WPRINTF("CTL_IO: err=%d (%s)", errno, strerror(errno));
		req->vsr_cmd_wr->response = VIRTIO_SCSI_S_FAILURE;
	} else {
		req->vsr_cmd_wr->sense_len =
		    MIN(io->scsiio.sense_len, sc->vss_config.sense_size);
		req->vsr_cmd_wr->residual = ext_data_len -
		    io->scsiio.ext_data_filled;
		req->vsr_cmd_wr->status = io->scsiio.scsi_status;
		req->vsr_cmd_wr->response = VIRTIO_SCSI_S_OK;
		memcpy(&req->vsr_cmd_wr->sense, &io->scsiio.sense_data,
		    req->vsr_cmd_wr->sense_len);
	}

	nxferred = io->scsiio.ext_data_filled;
	return (nxferred);
}

static void
pci_vtscsi_controlq_notify(void *vsc, struct vqueue_info *vq)
{
	struct pci_vtscsi_softc *sc;
	struct iovec iov[VTSCSI_MAXSEG];
	struct vi_req req;
	void *buf = NULL;
	size_t bufsize;
	int n;

	sc = vsc;

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, iov, VTSCSI_MAXSEG, &req);
		assert(n >= 1 && n <= VTSCSI_MAXSEG);

		bufsize = iov_to_buf(iov, n, &buf);
		pci_vtscsi_control_handle(sc, buf, bufsize);
		buf_to_iov((uint8_t *)buf, bufsize, iov, n);

		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, req.idx, bufsize);
	}
	vq_endchains(vq, 1);	/* Generate interrupt if appropriate. */
	free(buf);
}

static void
pci_vtscsi_eventq_notify(void *vsc __unused, struct vqueue_info *vq)
{
	vq_kick_disable(vq);
}

static void
pci_vtscsi_requestq_notify(void *vsc, struct vqueue_info *vq)
{
	while (vq_has_descs(vq)) {
		pci_vtscsi_queue_request(vsc, vq);
	}
}

static int
pci_vtscsi_init_queue(struct pci_vtscsi_softc *sc,
    struct pci_vtscsi_queue *queue, int num)
{
	struct pci_vtscsi_worker *workers;
	char tname[MAXCOMLEN + 1];
	int i;

	queue->vsq_sc = sc;
	queue->vsq_vq = &sc->vss_vq[num + 2];

	pthread_mutex_init(&queue->vsq_rmtx, NULL);
	pthread_mutex_init(&queue->vsq_fmtx, NULL);
	pthread_mutex_init(&queue->vsq_qmtx, NULL);
	pthread_cond_init(&queue->vsq_cv, NULL);
	STAILQ_INIT(&queue->vsq_requests);
	STAILQ_INIT(&queue->vsq_free_requests);
	LIST_INIT(&queue->vsq_workers);

	for (i = 0; i < VTSCSI_RINGSZ; i++) {
		struct pci_vtscsi_request *req;

		req = pci_vtscsi_alloc_request(sc);
		if (req == NULL)
			goto fail;

		pci_vtscsi_put_request(&queue->vsq_free_requests, req);
	}

	workers = calloc(VTSCSI_THR_PER_Q, sizeof(struct pci_vtscsi_worker));
	if (workers == NULL)
		goto fail;

	for (i = 0; i < VTSCSI_THR_PER_Q; i++) {
		workers[i].vsw_queue = queue;

		pthread_create(&workers[i].vsw_thread, NULL, &pci_vtscsi_proc,
		    (void *)&workers[i]);

		snprintf(tname, sizeof(tname), "vtscsi:%d-%d", num, i);
		pthread_set_name_np(workers[i].vsw_thread, tname);
		LIST_INSERT_HEAD(&queue->vsq_workers, &workers[i], vsw_link);
	}

	return (0);

fail:
	pci_vtscsi_destroy_queue(queue);

	return (-1);

}

static void
pci_vtscsi_destroy_queue(struct pci_vtscsi_queue *queue)
{
	if (queue->vsq_sc == NULL)
		return;

	for (int i = VTSCSI_RINGSZ; i > 0; i--) {
		struct pci_vtscsi_request *req;

		if (STAILQ_EMPTY(&queue->vsq_free_requests))
			break;

		req = pci_vtscsi_get_request(&queue->vsq_free_requests);
		pci_vtscsi_free_request(req);
	}

	pthread_cond_destroy(&queue->vsq_cv);
	pthread_mutex_destroy(&queue->vsq_qmtx);
	pthread_mutex_destroy(&queue->vsq_fmtx);
	pthread_mutex_destroy(&queue->vsq_rmtx);
}

static int
pci_vtscsi_legacy_config(nvlist_t *nvl, const char *opts)
{
	char *cp, *devname;

	if (opts == NULL)
		return (0);

	cp = strchr(opts, ',');
	if (cp == NULL) {
		set_config_value_node(nvl, "dev", opts);
		return (0);
	}
	devname = strndup(opts, cp - opts);
	set_config_value_node(nvl, "dev", devname);
	free(devname);
	return (pci_parse_legacy_config(nvl, cp + 1));
}

static int
pci_vtscsi_init(struct pci_devinst *pi, nvlist_t *nvl)
{
	struct pci_vtscsi_softc *sc;
	const char *devname, *value;
	int err;
	int i;

	sc = calloc(1, sizeof(struct pci_vtscsi_softc));
	if (sc == NULL)
		return (-1);

	value = get_config_value_node(nvl, "iid");
	if (value != NULL)
		sc->vss_iid = strtoul(value, NULL, 10);

	value = get_config_value_node(nvl, "bootindex");
	if (value != NULL) {
		if (pci_emul_add_boot_device(pi, atoi(value))) {
			EPRINTLN("Invalid bootindex %d", atoi(value));
			goto fail;
		}
	}

	devname = get_config_value_node(nvl, "dev");
	if (devname == NULL)
		devname = "/dev/cam/ctl";
	sc->vss_ctl_fd = open(devname, O_RDWR);
	if (sc->vss_ctl_fd < 0) {
		WPRINTF("cannot open %s: %s", devname, strerror(errno));
		goto fail;
	}

	pthread_mutex_init(&sc->vss_mtx, NULL);

	vi_softc_linkup(&sc->vss_vs, &vtscsi_vi_consts, sc, pi, sc->vss_vq);
	sc->vss_vs.vs_mtx = &sc->vss_mtx;

	/*
	 * Perform a "reset" before we set up our queues.
	 *
	 * This will write the default config into vss_config, which is used
	 * by the rest of the driver to get the request header size. Note that
	 * if we ever allow the guest to override sense size through config
	 * space writes, pre-allocation of I/O requests will have to change
	 * accordingly.
	 */
	pthread_mutex_lock(&sc->vss_mtx);
	pci_vtscsi_reset(sc);
	pthread_mutex_unlock(&sc->vss_mtx);

	/* controlq */
	sc->vss_vq[0].vq_qsize = VTSCSI_RINGSZ;
	sc->vss_vq[0].vq_notify = pci_vtscsi_controlq_notify;

	/* eventq */
	sc->vss_vq[1].vq_qsize = VTSCSI_RINGSZ;
	sc->vss_vq[1].vq_notify = pci_vtscsi_eventq_notify;

	/* request queues */
	for (i = 2; i < VTSCSI_MAXQ; i++) {
		sc->vss_vq[i].vq_qsize = VTSCSI_RINGSZ;
		sc->vss_vq[i].vq_notify = pci_vtscsi_requestq_notify;

		err = pci_vtscsi_init_queue(sc, &sc->vss_queues[i - 2], i - 2);
		if (err != 0)
			goto fail;
	}

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_SCSI);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_ID_SCSI);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (vi_intr_init(&sc->vss_vs, 1, fbsdrun_virtio_msix()))
		goto fail;

	vi_set_io_bar(&sc->vss_vs, 0);

	return (0);

fail:
	for (i = 2; i < VTSCSI_MAXQ; i++)
		pci_vtscsi_destroy_queue(&sc->vss_queues[i - 2]);

	if (sc->vss_ctl_fd > 0)
		close(sc->vss_ctl_fd);

	free(sc);
	return (-1);
}


static const struct pci_devemu pci_de_vscsi = {
	.pe_emu =	"virtio-scsi",
	.pe_init =	pci_vtscsi_init,
	.pe_legacy_config = pci_vtscsi_legacy_config,
	.pe_barwrite =	vi_pci_write,
	.pe_barread =	vi_pci_read
};
PCI_EMUL_SET(pci_de_vscsi);
