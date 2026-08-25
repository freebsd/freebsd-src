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

#ifndef	_PCI_VIRTIO_SCSI_H_
#define	_PCI_VIRTIO_SCSI_H_

#include "iov.h"

extern int pci_vtscsi_debug;

#define	WPRINTF(msg, params...)	PRINTLN("virtio-scsi: " msg, ##params)
#define	DPRINTF(msg, params...)	if (pci_vtscsi_debug) WPRINTF(msg, ##params)

/* Absolute limits given by the VirtIO SCSI spec */
#define	VIRTIO_SCSI_MAX_CHANNEL	0
#define	VIRTIO_SCSI_MAX_TARGET	255
#define	VIRTIO_SCSI_MAX_LUN	16383
#define	VIRTIO_SCSI_HDR_SEG	2
#define	VIRTIO_SCSI_ADDL_Q	2

/* Features specific to VirtIO SCSI, none of which we currently support */
#define	VIRTIO_SCSI_F_INOUT	(1 << 0)
#define	VIRTIO_SCSI_F_HOTPLUG	(1 << 1)
#define	VIRTIO_SCSI_F_CHANGE	(1 << 2)

/* Default limits which we set. All of these are configurable. */
#define	VTSCSI_DEF_RINGSZ	64
#define	VTSCSI_MIN_RINGSZ	4
#define	VTSCSI_MAX_RINGSZ	4096

#define	VTSCSI_DEF_THR_PER_Q	16
#define	VTSCSI_MIN_THR_PER_Q	1
#define	VTSCSI_MAX_THR_PER_Q	256

#define	VTSCSI_DEF_MAXSEG	64
#define	VTSCSI_MIN_MAXSEG	(VIRTIO_SCSI_HDR_SEG + 1)
#define	VTSCSI_MAX_MAXSEG	\
    (4096 - VIRTIO_SCSI_HDR_SEG - SPLIT_IOV_ADDL_IOV)

#define	VTSCSI_DEF_REQUESTQ	1
#define	VTSCSI_MIN_REQUESTQ	1
#define	VTSCSI_MAX_REQUESTQ	(32 - VIRTIO_SCSI_ADDL_Q)

/*
 * Device-specific config space registers
 *
 * The guest driver may try to modify cdb_size and sense_size by writing the
 * respective config space registers. Since we currently ignore all writes to
 * config space, these macros are essentially constant.
 */
#define	VTSCSI_IN_HEADER_LEN(_sc)	\
	(sizeof(struct pci_vtscsi_req_cmd_rd) + _sc->vss_config.cdb_size)

#define	VTSCSI_OUT_HEADER_LEN(_sc)	\
	(sizeof(struct pci_vtscsi_req_cmd_wr) + _sc->vss_config.sense_size)

struct pci_vtscsi_config {
	uint32_t				num_queues;
	uint32_t				seg_max;
	uint32_t				max_sectors;
	uint32_t				cmd_per_lun;
	uint32_t				event_info_size;
	uint32_t				sense_size;
	uint32_t				cdb_size;
	uint16_t				max_channel;
	uint16_t				max_target;
	uint32_t				max_lun;
} __attribute__((packed));


/*
 * I/O request state and I/O request queues
 *
 * In addition to the control queue and notification queues, each virtio-scsi
 * device instance has at least one I/O request queue, the state of which is
 * is kept in an array of struct pci_vtscsi_queue in the device softc.
 *
 * Each pci_vtscsi_queue has configurable number of pci_vtscsi_request
 * structures pre-allocated on vsq_free_requests. For each I/O request
 * coming in on the I/O virtqueue, the request queue handler will take a
 * pci_vtscsi_request off vsq_free_requests, fills in the data from the
 * I/O virtqueue, puts it on vsq_requests, and signals vsq_cv.
 *
 * Each pci_vtscsi_queue will have a configurable number of worker threads,
 * which wait on vsq_cv. When signalled, they repeatedly take a single
 * pci_vtscsi_request off vsq_requests and hand it to the backend, which
 * processes it synchronously. After completion, the pci_vtscsi_request
 * is re-initialized and put back onto vsq_free_requests.
 *
 * The worker threads exit when vsq_cv is signalled after vsw_exiting was set.
 *
 * There are three mutexes to coordinate the accesses to an I/O request queue:
 * - vsq_rmtx protects vsq_requests and must be held when waiting on vsq_cv
 * - vsq_fmtx protects vsq_free_requests
 * - vsq_qmtx must be held when operating on the underlying virtqueue, vsq_vq
 *
 * The I/O vectors for each request are kept in the preallocated iovec array
 * vsr_iov, and pointers to the respective header/data in/out portions are set
 * up to point into the array when the request is queued for processing.
 *
 * The number of iovecs preallocated for vsr_iov is derived from the configured
 * 'seg_max' parameter defined by the virtio spec:
 *   - 'seg_max' parameter specifies the maximum number of I/O data vectors
 *     we support in any request
 *   - we need 2 additional iovecs for the I/O headers (VIRTIO_SCSI_HDR_SEG)
 *   - we need another 2 additional iovecs for split_iov() (SPLIT_IOV_ADDL_IOV)
 *
 * The only time we explicitly need the full size of vsr_iov after preallocation
 * is during re-initialization after completing a request, and implicitly in the
 * calls to split_iov() the set up the pointers. In all other cases, we use only
 * 'seg_max' + VIRTIO_SCSI_HDR_SEG, and we advertise only 'seg_max' to the guest
 * in accordance to the virtio spec.
 */
STAILQ_HEAD(pci_vtscsi_req_queue, pci_vtscsi_request);

struct pci_vtscsi_queue {
	struct pci_vtscsi_softc			*vsq_sc;
	struct vqueue_info			*vsq_vq;
	pthread_mutex_t				vsq_rmtx;
	pthread_mutex_t				vsq_fmtx;
	pthread_mutex_t				vsq_qmtx;
	pthread_cond_t				vsq_cv;
	struct pci_vtscsi_req_queue		vsq_requests;
	struct pci_vtscsi_req_queue		vsq_free_requests;
	LIST_HEAD(, pci_vtscsi_worker)		vsq_workers;
};

struct pci_vtscsi_worker {
	struct pci_vtscsi_queue			*vsw_queue;
	pthread_t				vsw_thread;
	bool					vsw_exiting;
	LIST_ENTRY(pci_vtscsi_worker)		vsw_link;
};

struct pci_vtscsi_request {
	struct pci_vtscsi_queue			*vsr_queue;
	struct iovec				*vsr_iov;
	struct iovec				*vsr_iov_in;
	struct iovec				*vsr_iov_out;
	struct iovec				*vsr_data_iov_in;
	struct iovec				*vsr_data_iov_out;
	struct pci_vtscsi_req_cmd_rd		*vsr_cmd_rd;
	struct pci_vtscsi_req_cmd_wr		*vsr_cmd_wr;
	void					*vsr_backend;
	size_t					vsr_niov_in;
	size_t					vsr_niov_out;
	size_t					vsr_data_niov_in;
	size_t					vsr_data_niov_out;
	uint32_t				vsr_idx;
	STAILQ_ENTRY(pci_vtscsi_request)	vsr_link;
};

/*
 * Per-target state.
 */
struct pci_vtscsi_target {
	uint8_t					vst_target;
	int					vst_fd;
	int					vst_max_sectors;
};

/*
 * Per-device softc
 */
struct pci_vtscsi_softc {
	struct virtio_softc			vss_vs;
	struct virtio_consts			vss_vi_consts;
	struct vqueue_info			*vss_vq;
	struct pci_vtscsi_queue			*vss_queues;
	pthread_mutex_t				vss_mtx;
	uint32_t				vss_features;
	size_t					vss_num_target;
	uint32_t				vss_ctl_ringsz;
	uint32_t				vss_evt_ringsz;
	uint32_t				vss_req_ringsz;
	uint32_t				vss_thr_per_q;
	struct pci_vtscsi_config		vss_default_config;
	struct pci_vtscsi_config		vss_config;
	struct pci_vtscsi_target		*vss_targets;
	struct pci_vtscsi_backend		*vss_backend;
};

/*
 * VirtIO-SCSI Task Management Function control requests
 */
#define	VIRTIO_SCSI_T_TMF			0
#define	VIRTIO_SCSI_T_TMF_ABORT_TASK		0
#define	VIRTIO_SCSI_T_TMF_ABORT_TASK_SET	1
#define	VIRTIO_SCSI_T_TMF_CLEAR_ACA		2
#define	VIRTIO_SCSI_T_TMF_CLEAR_TASK_SET	3
#define	VIRTIO_SCSI_T_TMF_I_T_NEXUS_RESET	4
#define	VIRTIO_SCSI_T_TMF_LOGICAL_UNIT_RESET	5
#define	VIRTIO_SCSI_T_TMF_QUERY_TASK		6
#define	VIRTIO_SCSI_T_TMF_QUERY_TASK_SET	7

#define	VIRTIO_SCSI_T_TMF_MAX_FUNC		VIRTIO_SCSI_T_TMF_QUERY_TASK_SET

/* command-specific response values */
#define	VIRTIO_SCSI_S_FUNCTION_COMPLETE		0
#define	VIRTIO_SCSI_S_FUNCTION_SUCCEEDED	10
#define	VIRTIO_SCSI_S_FUNCTION_REJECTED		11

struct pci_vtscsi_ctrl_tmf {
	const uint32_t	type;
	const uint32_t	subtype;
	const uint8_t	lun[8];
	const uint64_t	id;
	uint8_t		response;
} __attribute__((packed));


/*
 * VirtIO-SCSI Asynchronous Notification control requests
 */
#define	VIRTIO_SCSI_T_AN_QUERY			1
#define	VIRTIO_SCSI_EVT_ASYNC_OPERATIONAL_CHANGE 2
#define	VIRTIO_SCSI_EVT_ASYNC_POWER_MGMT	4
#define	VIRTIO_SCSI_EVT_ASYNC_EXTERNAL_REQUEST	8
#define	VIRTIO_SCSI_EVT_ASYNC_MEDIA_CHANGE	16
#define	VIRTIO_SCSI_EVT_ASYNC_MULTI_HOST	32
#define	VIRTIO_SCSI_EVT_ASYNC_DEVICE_BUSY	64

struct pci_vtscsi_ctrl_an {
	const uint32_t	type;
	const uint8_t	lun[8];
	const uint32_t	event_requested;
	uint32_t	event_actual;
	uint8_t		response;
} __attribute__((packed));

/* command-specific response values */
#define	VIRTIO_SCSI_S_OK			0
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

struct pci_vtscsi_event {
	uint32_t	event;
	uint8_t		lun[8];
	uint32_t	reason;
} __attribute__((packed));

/*
 * VirtIO-SCSI I/O requests
 */
struct pci_vtscsi_req_cmd_rd {
	const uint8_t	lun[8];
	const uint64_t	id;
	const uint8_t	task_attr;
	const uint8_t	prio;
	const uint8_t	crn;
	const uint8_t	cdb[];
} __attribute__((packed));

/* task_attr */
#define	VIRTIO_SCSI_S_SIMPLE			0
#define	VIRTIO_SCSI_S_ORDERED			1
#define	VIRTIO_SCSI_S_HEAD			2
#define	VIRTIO_SCSI_S_ACA			3

struct pci_vtscsi_req_cmd_wr {
	uint32_t	sense_len;
	uint32_t	residual;
	uint16_t	status_qualifier;
	uint8_t		status;
	uint8_t		response;
	uint8_t		sense[];
} __attribute__((packed));

/*
 * Backend interface
 */
struct pci_vtscsi_backend {
	const char	*vsb_name;
	int		(*vsb_init)(struct pci_vtscsi_softc *,
			    struct pci_vtscsi_backend *, nvlist_t *);
	int		(*vsb_open)(struct pci_vtscsi_softc *, const char *,
			    long);
	void		(*vsb_reset)(struct pci_vtscsi_softc *);

	void*		(*vsb_req_alloc)(struct pci_vtscsi_softc *);
	void		(*vsb_req_clear)(void *);
	void		(*vsb_req_free)(void *);

	void		(*vsb_tmf_hdl)(struct pci_vtscsi_softc *, int,
			    struct pci_vtscsi_ctrl_tmf *);
	void		(*vsb_an_hdl)(struct pci_vtscsi_softc *, int,
			    struct pci_vtscsi_ctrl_an *);
	int		(*vsb_req_hdl)(struct pci_vtscsi_softc *, int,
			    struct pci_vtscsi_request *);
};
#define	PCI_VTSCSI_BACKEND_SET(x)	DATA_SET(pci_vtscsi_backend_set, x)

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
pci_vtscsi_check_lun(struct pci_vtscsi_softc *sc, const uint8_t *lun)
{
	if (lun[0] == 0xC1)
		return (false);

	if (lun[0] != 0x01)
		return (false);

	if (lun[1] >= sc->vss_num_target)
		return (false);

	if (lun[1] != sc->vss_targets[lun[1]].vst_target)
		return (false);

	if (sc->vss_targets[lun[1]].vst_fd < 0)
		return (false);

	if (lun[2] != 0x00 && (lun[2] & 0xc0) != 0x40)
		return (false);

	if (lun[4] != 0 || lun[5] != 0 || lun[6] != 0 || lun[7] != 0)
		return (false);

	return (true);
}

/*
 * Get the target id from a LUN address.
 *
 * Every code path using this function must have called pci_vtscsi_check_lun()
 * before to make sure the LUN address is valid.
 */
static inline uint8_t
pci_vtscsi_get_target(struct pci_vtscsi_softc *sc, const uint8_t *lun)
{
	assert(lun[0] == 0x01);
	assert(lun[1] < sc->vss_num_target);
	assert(lun[1] == sc->vss_targets[lun[1]].vst_target);
	assert(sc->vss_targets[lun[1]].vst_fd >= 0);
	assert(lun[2] == 0x00 || (lun[2] & 0xc0) == 0x40);

	return (lun[1]);
}

/*
 * Get the LUN id from a LUN address.
 *
 * Every code path using this function must have called pci_vtscsi_check_lun()
 * before to make sure the LUN address is valid.
 */
static inline uint16_t
pci_vtscsi_get_lun(struct pci_vtscsi_softc *sc, const uint8_t *lun)
{
	assert(lun[0] == 0x01);
	assert(lun[1] < sc->vss_num_target);
	assert(lun[1] == sc->vss_targets[lun[1]].vst_target);
	assert(sc->vss_targets[lun[1]].vst_fd >= 0);
	assert(lun[2] == 0x00 || (lun[2] & 0xc0) == 0x40);

	return (((lun[2] << 8) | lun[3]) & 0x3fff);
}

#endif	/* _PCI_VIRTIO_SCSI_H_ */
