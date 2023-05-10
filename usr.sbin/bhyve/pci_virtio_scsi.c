/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Jakub Klama <jceel@FreeBSD.org>.
 * Copyright (c) 2018 Marcelo Araujo <araujo@FreeBSD.org>.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

struct pci_vtscsi_queue {
	struct pci_vtscsi_softc *         vsq_sc;
	struct vqueue_info *              vsq_vq;
	pthread_mutex_t                   vsq_mtx;
	pthread_mutex_t                   vsq_qmtx;
	pthread_cond_t                    vsq_cv;
	STAILQ_HEAD(, pci_vtscsi_request) vsq_requests;
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
	struct iovec              vsr_iov_in[VTSCSI_MAXSEG];
	int                       vsr_niov_in;
	struct iovec              vsr_iov_out[VTSCSI_MAXSEG];
	int                       vsr_niov_out;
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
	uint32_t type;
	uint32_t subtype;
	uint8_t lun[8];
	uint64_t id;
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
	uint32_t type;
	uint8_t lun[8];
	uint32_t event_requested;
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
	uint8_t lun[8];
	uint64_t id;
	uint8_t task_attr;
	uint8_t prio;
	uint8_t crn;
	uint8_t cdb[];
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
static inline int pci_vtscsi_get_lun(uint8_t *);
static int pci_vtscsi_control_handle(struct pci_vtscsi_softc *, void *, size_t);
static int pci_vtscsi_tmf_handle(struct pci_vtscsi_softc *,
    struct pci_vtscsi_ctrl_tmf *);
static int pci_vtscsi_an_handle(struct pci_vtscsi_softc *,
    struct pci_vtscsi_ctrl_an *);
static int pci_vtscsi_request_handle(struct pci_vtscsi_queue *, struct iovec *,
    int, struct iovec *, int);
static void pci_vtscsi_controlq_notify(void *, struct vqueue_info *);
static void pci_vtscsi_eventq_notify(void *, struct vqueue_info *);
static void pci_vtscsi_requestq_notify(void *, struct vqueue_info *);
static int  pci_vtscsi_init_queue(struct pci_vtscsi_softc *,
    struct pci_vtscsi_queue *, int);
static int pci_vtscsi_init(struct pci_devinst *, nvlist_t *);

static struct virtio_consts vtscsi_vi_consts = {
	.vc_name =	"vtscsi",
	.vc_nvq =	VTSCSI_MAXQ,
	.vc_cfgsize =	sizeof(struct pci_vtscsi_config),
	.vc_reset =	pci_vtscsi_reset,
	.vc_cfgread =	pci_vtscsi_cfgread,
	.vc_cfgwrite =	pci_vtscsi_cfgwrite,
	.vc_apply_features = pci_vtscsi_neg_features,
	.vc_hv_caps =	0,
};

static void *
pci_vtscsi_proc(void *arg)
{
	struct pci_vtscsi_worker *worker = (struct pci_vtscsi_worker *)arg;
	struct pci_vtscsi_queue *q = worker->vsw_queue;
	struct pci_vtscsi_request *req;
	int iolen;

	for (;;) {
		pthread_mutex_lock(&q->vsq_mtx);

		while (STAILQ_EMPTY(&q->vsq_requests)
		    && !worker->vsw_exiting)
			pthread_cond_wait(&q->vsq_cv, &q->vsq_mtx);

		if (worker->vsw_exiting)
			break;

		req = STAILQ_FIRST(&q->vsq_requests);
		STAILQ_REMOVE_HEAD(&q->vsq_requests, vsr_link);

		pthread_mutex_unlock(&q->vsq_mtx);
		iolen = pci_vtscsi_request_handle(q, req->vsr_iov_in,
		    req->vsr_niov_in, req->vsr_iov_out, req->vsr_niov_out);

		pthread_mutex_lock(&q->vsq_qmtx);
		vq_relchain(q->vsq_vq, req->vsr_idx, iolen);
		vq_endchains(q->vsq_vq, 0);
		pthread_mutex_unlock(&q->vsq_qmtx);

		DPRINTF("request <idx=%d> completed", req->vsr_idx);
		free(req);
	}

	pthread_mutex_unlock(&q->vsq_mtx);
	return (NULL);
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
		.max_sectors = 2,
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

static inline int
pci_vtscsi_get_lun(uint8_t *lun)
{

	return (((lun[2] << 8) | lun[3]) & 0x3fff);
}

static int
pci_vtscsi_control_handle(struct pci_vtscsi_softc *sc, void *buf,
    size_t bufsize)
{
	struct pci_vtscsi_ctrl_tmf *tmf;
	struct pci_vtscsi_ctrl_an *an;
	uint32_t type;

	if (bufsize < sizeof(uint32_t)) {
		WPRINTF("ignoring truncated control request");
		return (0);
	}

	type = *(uint32_t *)buf;

	if (type == VIRTIO_SCSI_T_TMF) {
		if (bufsize != sizeof(*tmf)) {
			WPRINTF("ignoring tmf request with size %zu", bufsize);
			return (0);
		}
		tmf = (struct pci_vtscsi_ctrl_tmf *)buf;
		return (pci_vtscsi_tmf_handle(sc, tmf));
	}

	if (type == VIRTIO_SCSI_T_AN_QUERY) {
		if (bufsize != sizeof(*an)) {
			WPRINTF("ignoring AN request with size %zu", bufsize);
			return (0);
		}
		an = (struct pci_vtscsi_ctrl_an *)buf;
		return (pci_vtscsi_an_handle(sc, an));
	}

	return (0);
}

static int
pci_vtscsi_tmf_handle(struct pci_vtscsi_softc *sc,
    struct pci_vtscsi_ctrl_tmf *tmf)
{
	union ctl_io *io;
	int err;

	io = ctl_scsi_alloc_io(sc->vss_iid);
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
	return (1);
}

static int
pci_vtscsi_an_handle(struct pci_vtscsi_softc *sc __unused,
    struct pci_vtscsi_ctrl_an *an __unused)
{
	return (0);
}

static int
pci_vtscsi_request_handle(struct pci_vtscsi_queue *q, struct iovec *iov_in,
    int niov_in, struct iovec *iov_out, int niov_out)
{
	struct pci_vtscsi_softc *sc = q->vsq_sc;
	struct pci_vtscsi_req_cmd_rd *cmd_rd = NULL;
	struct pci_vtscsi_req_cmd_wr *cmd_wr;
	struct iovec data_iov_in[VTSCSI_MAXSEG], data_iov_out[VTSCSI_MAXSEG];
	union ctl_io *io;
	int data_niov_in, data_niov_out;
	void *ext_data_ptr = NULL;
	uint32_t ext_data_len = 0, ext_sg_entries = 0;
	int err, nxferred;

	if (count_iov(iov_out, niov_out) < VTSCSI_OUT_HEADER_LEN(sc)) {
		WPRINTF("ignoring request with insufficient output");
		return (0);
	}
	if (count_iov(iov_in, niov_in) < VTSCSI_IN_HEADER_LEN(sc)) {
		WPRINTF("ignoring request with incomplete header");
		return (0);
	}

	seek_iov(iov_in, niov_in, data_iov_in, &data_niov_in,
	    VTSCSI_IN_HEADER_LEN(sc));
	seek_iov(iov_out, niov_out, data_iov_out, &data_niov_out,
	    VTSCSI_OUT_HEADER_LEN(sc));

	truncate_iov(iov_in, &niov_in, VTSCSI_IN_HEADER_LEN(sc));
	truncate_iov(iov_out, &niov_out, VTSCSI_OUT_HEADER_LEN(sc));
	iov_to_buf(iov_in, niov_in, (void **)&cmd_rd);

	cmd_wr = calloc(1, VTSCSI_OUT_HEADER_LEN(sc));
	io = ctl_scsi_alloc_io(sc->vss_iid);
	ctl_scsi_zero_io(io);

	io->io_hdr.nexus.initid = sc->vss_iid;
	io->io_hdr.nexus.targ_lun = pci_vtscsi_get_lun(cmd_rd->lun);

	io->io_hdr.io_type = CTL_IO_SCSI;

	if (data_niov_in > 0) {
		ext_data_ptr = (void *)data_iov_in;
		ext_sg_entries = data_niov_in;
		ext_data_len = count_iov(data_iov_in, data_niov_in);
		io->io_hdr.flags |= CTL_FLAG_DATA_OUT;
	} else if (data_niov_out > 0) {
		ext_data_ptr = (void *)data_iov_out;
		ext_sg_entries = data_niov_out;
		ext_data_len = count_iov(data_iov_out, data_niov_out);
		io->io_hdr.flags |= CTL_FLAG_DATA_IN;
	}

	io->scsiio.sense_len = sc->vss_config.sense_size;
	io->scsiio.tag_num = cmd_rd->id;
	io->io_hdr.flags |= CTL_FLAG_USER_TAG;
	switch (cmd_rd->task_attr) {
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
	memcpy(io->scsiio.cdb, cmd_rd->cdb, sc->vss_config.cdb_size);

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
		cmd_wr->response = VIRTIO_SCSI_S_FAILURE;
	} else {
		cmd_wr->sense_len = MIN(io->scsiio.sense_len,
		    sc->vss_config.sense_size);
		cmd_wr->residual = ext_data_len - io->scsiio.ext_data_filled;
		cmd_wr->status = io->scsiio.scsi_status;
		cmd_wr->response = VIRTIO_SCSI_S_OK;
		memcpy(&cmd_wr->sense, &io->scsiio.sense_data,
		    cmd_wr->sense_len);
	}

	buf_to_iov(cmd_wr, VTSCSI_OUT_HEADER_LEN(sc), iov_out, niov_out, 0);
	nxferred = VTSCSI_OUT_HEADER_LEN(sc) + io->scsiio.ext_data_filled;
	free(cmd_rd);
	free(cmd_wr);
	ctl_scsi_free_io(io);
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
	int iolen, n;

	sc = vsc;

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, iov, VTSCSI_MAXSEG, &req);
		assert(n >= 1 && n <= VTSCSI_MAXSEG);

		bufsize = iov_to_buf(iov, n, &buf);
		iolen = pci_vtscsi_control_handle(sc, buf, bufsize);
		buf_to_iov((uint8_t *)buf + bufsize - iolen, iolen, iov, n,
		    bufsize - iolen);

		/*
		 * Release this chain and handle more
		 */
		vq_relchain(vq, req.idx, iolen);
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
	struct pci_vtscsi_softc *sc;
	struct pci_vtscsi_queue *q;
	struct pci_vtscsi_request *req;
	struct iovec iov[VTSCSI_MAXSEG];
	struct vi_req vireq;
	int n;

	sc = vsc;
	q = &sc->vss_queues[vq->vq_num - 2];

	while (vq_has_descs(vq)) {
		n = vq_getchain(vq, iov, VTSCSI_MAXSEG, &vireq);
		assert(n >= 1 && n <= VTSCSI_MAXSEG);

		req = calloc(1, sizeof(struct pci_vtscsi_request));
		req->vsr_idx = vireq.idx;
		req->vsr_queue = q;
		req->vsr_niov_in = vireq.readable;
		req->vsr_niov_out = vireq.writable;
		memcpy(req->vsr_iov_in, iov,
		    req->vsr_niov_in * sizeof(struct iovec));
		memcpy(req->vsr_iov_out, iov + vireq.readable,
		    req->vsr_niov_out * sizeof(struct iovec));

		pthread_mutex_lock(&q->vsq_mtx);
		STAILQ_INSERT_TAIL(&q->vsq_requests, req, vsr_link);
		pthread_cond_signal(&q->vsq_cv);
		pthread_mutex_unlock(&q->vsq_mtx);

		DPRINTF("request <idx=%d> enqueued", vireq.idx);
	}
}

static int
pci_vtscsi_init_queue(struct pci_vtscsi_softc *sc,
    struct pci_vtscsi_queue *queue, int num)
{
	struct pci_vtscsi_worker *worker;
	char tname[MAXCOMLEN + 1];
	int i;

	queue->vsq_sc = sc;
	queue->vsq_vq = &sc->vss_vq[num + 2];

	pthread_mutex_init(&queue->vsq_mtx, NULL);
	pthread_mutex_init(&queue->vsq_qmtx, NULL);
	pthread_cond_init(&queue->vsq_cv, NULL);
	STAILQ_INIT(&queue->vsq_requests);
	LIST_INIT(&queue->vsq_workers);

	for (i = 0; i < VTSCSI_THR_PER_Q; i++) {
		worker = calloc(1, sizeof(struct pci_vtscsi_worker));
		worker->vsw_queue = queue;

		pthread_create(&worker->vsw_thread, NULL, &pci_vtscsi_proc,
		    (void *)worker);

		snprintf(tname, sizeof(tname), "vtscsi:%d-%d", num, i);
		pthread_set_name_np(worker->vsw_thread, tname);
		LIST_INSERT_HEAD(&queue->vsq_workers, worker, vsw_link);
	}

	return (0);
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
	int i;

	sc = calloc(1, sizeof(struct pci_vtscsi_softc));
	value = get_config_value_node(nvl, "iid");
	if (value != NULL)
		sc->vss_iid = strtoul(value, NULL, 10);

	devname = get_config_value_node(nvl, "dev");
	if (devname == NULL)
		devname = "/dev/cam/ctl";
	sc->vss_ctl_fd = open(devname, O_RDWR);
	if (sc->vss_ctl_fd < 0) {
		WPRINTF("cannot open %s: %s", devname, strerror(errno));
		free(sc);
		return (1);
	}

	pthread_mutex_init(&sc->vss_mtx, NULL);

	vi_softc_linkup(&sc->vss_vs, &vtscsi_vi_consts, sc, pi, sc->vss_vq);
	sc->vss_vs.vs_mtx = &sc->vss_mtx;

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
		pci_vtscsi_init_queue(sc, &sc->vss_queues[i - 2], i - 2);
	}

	/* initialize config space */
	pci_set_cfgdata16(pi, PCIR_DEVICE, VIRTIO_DEV_SCSI);
	pci_set_cfgdata16(pi, PCIR_VENDOR, VIRTIO_VENDOR);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata16(pi, PCIR_SUBDEV_0, VIRTIO_ID_SCSI);
	pci_set_cfgdata16(pi, PCIR_SUBVEND_0, VIRTIO_VENDOR);

	if (vi_intr_init(&sc->vss_vs, 1, fbsdrun_virtio_msix()))
		return (1);
	vi_set_io_bar(&sc->vss_vs, 0);

	return (0);
}


static const struct pci_devemu pci_de_vscsi = {
	.pe_emu =	"virtio-scsi",
	.pe_init =	pci_vtscsi_init,
	.pe_legacy_config = pci_vtscsi_legacy_config,
	.pe_barwrite =	vi_pci_write,
	.pe_barread =	vi_pci_read
};
PCI_EMUL_SET(pci_de_vscsi);
