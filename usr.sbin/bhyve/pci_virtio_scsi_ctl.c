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
#include "pci_virtio_scsi.h"

struct vtscsi_ctl_backend {
	struct pci_vtscsi_backend	vcb_backend;
	int				vcb_iid;
};

static int vtscsi_ctl_init(struct pci_vtscsi_softc *,
    struct pci_vtscsi_backend *, nvlist_t *);
static int vtscsi_ctl_open(struct pci_vtscsi_softc *, const char *, long);
static void vtscsi_ctl_reset(struct pci_vtscsi_softc *);

static void *vtscsi_ctl_req_alloc(struct pci_vtscsi_softc *);
static void vtscsi_ctl_req_clear(void  *);
static void vtscsi_ctl_req_free(void *);

static void vtscsi_ctl_tmf_hdl(struct pci_vtscsi_softc *, int,
    struct pci_vtscsi_ctrl_tmf *);
static void vtscsi_ctl_an_hdl(struct pci_vtscsi_softc *, int,
    struct pci_vtscsi_ctrl_an *);
static int vtscsi_ctl_req_hdl(struct pci_vtscsi_softc *, int,
    struct pci_vtscsi_request *);

static int
vtscsi_ctl_count_targets(const char *prefix __unused,
    const nvlist_t *parent __unused, const char *name __unused, int type,
    void *arg)
{
	int *count = arg;

	if (type != NV_TYPE_STRING) {
		EPRINTLN("invalid target \"%s\" type: not a string", name);
		errno = EINVAL;
		return (-1);
	}

	(*count)++;

	return (0);
}

static int
vtscsi_ctl_init(struct pci_vtscsi_softc *sc, struct pci_vtscsi_backend *backend,
    nvlist_t *nvl)
{
	int count = 0;
	int ret = 0;
	struct vtscsi_ctl_backend *ctl_backend;
	const char *value;

	ctl_backend = calloc(1, sizeof(struct vtscsi_ctl_backend));
	if (ctl_backend == NULL) {
		EPRINTLN("failed to allocate backend data: %s",
		    strerror(errno));
		return (-1);
	}

	ctl_backend->vcb_backend = *backend;
	sc->vss_backend = &ctl_backend->vcb_backend;

	value = get_config_value_node(nvl, "iid");
	if (value != NULL)
		ctl_backend->vcb_iid = strtoul(value, NULL, 10);

	/*
	 * Count configured targets. If no targets were configured, use
	 * /dev/cam/ctl to remain compatible with previous versions.
	 */
	nvl = find_relative_config_node(nvl, "target");
	assert(nvl != NULL);

	ret = walk_config_nodes("", nvl, &count, vtscsi_ctl_count_targets);
	if (ret != 0)
		return (ret);

	if (count == 0)
		set_config_value_node(nvl, "0", "/dev/cam/ctl");

	return (0);
}

static int
vtscsi_ctl_open(struct pci_vtscsi_softc *sc __unused, const char *devname,
    long target)
{
	struct pci_vtscsi_target *tgt = &sc->vss_targets[target];

	tgt->vst_fd = open(devname, O_RDWR);
	if (tgt->vst_fd < 0)
		return (-1);

	return (0);
}

static void
vtscsi_ctl_reset(struct pci_vtscsi_softc *sc __unused)
{
	/*
	 * There doesn't seem to be a limit to the maximum number of
	 * sectors CTL can transfer in one request.
	 */
	sc->vss_config.max_sectors = INT32_MAX;
}

static void *
vtscsi_ctl_req_alloc(struct pci_vtscsi_softc *sc)
{
	struct vtscsi_ctl_backend *ctl =
		(struct vtscsi_ctl_backend *)sc->vss_backend;
	union ctl_io *io = ctl_scsi_alloc_io(ctl->vcb_iid);

	if (io != NULL)
		ctl_scsi_zero_io(io);

	return (io);
}

static void
vtscsi_ctl_req_clear(void *io)
{
	ctl_scsi_zero_io(io);
}

static void
vtscsi_ctl_req_free(void *io)
{
	ctl_scsi_free_io(io);
}

static void
vtscsi_ctl_tmf_hdl(struct pci_vtscsi_softc *sc, int fd,
    struct pci_vtscsi_ctrl_tmf *tmf)
{
	struct vtscsi_ctl_backend *ctl;
	union ctl_io *io;
	int err;

	ctl = (struct vtscsi_ctl_backend *)sc->vss_backend;

	io = vtscsi_ctl_req_alloc(sc);
	if (io == NULL) {
		tmf->response = VIRTIO_SCSI_S_FAILURE;
		return;
	}
	vtscsi_ctl_req_clear(io);

	io->io_hdr.io_type = CTL_IO_TASK;
	io->io_hdr.nexus.initid = ctl->vcb_iid;
	io->io_hdr.nexus.targ_lun = pci_vtscsi_get_lun(sc, tmf->lun);
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

	err = ioctl(fd, CTL_IO, io);
	if (err != 0) {
		WPRINTF("CTL_IO: err=%d (%s)", errno, strerror(errno));
		tmf->response = VIRTIO_SCSI_S_FAILURE;
	} else {
		tmf->response = io->taskio.task_status;
	}
	vtscsi_ctl_req_free(io);
}

static void
vtscsi_ctl_an_hdl(struct pci_vtscsi_softc *sc __unused, int fd __unused,
    struct pci_vtscsi_ctrl_an *an)
{
	an->response = VIRTIO_SCSI_S_FAILURE;
}

static int
vtscsi_ctl_req_hdl(struct pci_vtscsi_softc *sc, int fd,
    struct pci_vtscsi_request *req)
{
	union ctl_io *io = req->vsr_backend;
	void *ext_data_ptr = NULL;
	uint32_t ext_data_len = 0, ext_sg_entries = 0;
	struct vtscsi_ctl_backend *ctl;
	int err, nxferred;

	ctl = (struct vtscsi_ctl_backend *)sc->vss_backend;

	io->io_hdr.nexus.initid = ctl->vcb_iid;
	io->io_hdr.nexus.targ_lun = pci_vtscsi_get_lun(sc,
	    req->vsr_cmd_rd->lun);

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

	err = ioctl(fd, CTL_IO, io);
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

static const struct pci_vtscsi_backend vtscsi_ctl_backend = {
	.vsb_name = "ctl",
	.vsb_init = vtscsi_ctl_init,
	.vsb_open = vtscsi_ctl_open,
	.vsb_reset = vtscsi_ctl_reset,

	.vsb_req_alloc = vtscsi_ctl_req_alloc,
	.vsb_req_clear = vtscsi_ctl_req_clear,
	.vsb_req_free = vtscsi_ctl_req_free,

	.vsb_tmf_hdl = vtscsi_ctl_tmf_hdl,
	.vsb_an_hdl = vtscsi_ctl_an_hdl,
	.vsb_req_hdl = vtscsi_ctl_req_hdl
};
PCI_VTSCSI_BACKEND_SET(vtscsi_ctl_backend);
