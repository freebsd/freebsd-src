/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/types.h>
#include <sys/_bitset.h>
#include <sys/bitset.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/controller/nvmft_var.h>

/*
 * A bitmask of command ID values.  This is used to detect duplicate
 * commands with the same ID.
 */
#define	NUM_CIDS	(UINT16_MAX + 1)
BITSET_DEFINE(cidset, NUM_CIDS);

struct nvmft_qpair {
	struct nvmft_controller *ctrlr;
	struct nvmf_qpair *qp;
	struct cidset *cids;

	bool	admin;
	bool	sq_flow_control;
	uint16_t qid;
	u_int	qsize;
	uint16_t sqhd;
	uint16_t sqtail;
	volatile u_int qp_refs;		/* Internal references on 'qp'. */

	struct task datamove_task;
	STAILQ_HEAD(, ctl_io_hdr) datamove_queue;

	struct mtx lock;

	char	name[16];
};

static int	_nvmft_send_generic_error(struct nvmft_qpair *qp,
    struct nvmf_capsule *nc, uint8_t sc_status);
static void	nvmft_datamove_task(void *context, int pending);

static void
nvmft_qpair_error(void *arg, int error)
{
	struct nvmft_qpair *qp = arg;
	struct nvmft_controller *ctrlr = qp->ctrlr;

	/*
	 * XXX: The Linux TCP initiator sends a RST immediately after
	 * the FIN, so treat ECONNRESET as plain EOF to avoid spurious
	 * errors on shutdown.
	 */
	if (error == ECONNRESET)
		error = 0;

	if (error != 0)
		nvmft_printf(ctrlr, "error %d on %s\n", error, qp->name);
	nvmft_controller_error(ctrlr, qp, error);
}

static void
nvmft_receive_capsule(void *arg, struct nvmf_capsule *nc)
{
	struct nvmft_qpair *qp = arg;
	struct nvmft_controller *ctrlr = qp->ctrlr;
	const struct nvme_command *cmd;
	uint8_t sc_status;

	cmd = nvmf_capsule_sqe(nc);
	if (ctrlr == NULL) {
		printf("NVMFT: %s received CID %u opcode %u on newborn queue\n",
		    qp->name, le16toh(cmd->cid), cmd->opc);
		nvmf_free_capsule(nc);
		return;
	}

	sc_status = nvmf_validate_command_capsule(nc);
	if (sc_status != NVME_SC_SUCCESS) {
		_nvmft_send_generic_error(qp, nc, sc_status);
		nvmf_free_capsule(nc);
		return;
	}

	/* Don't bother byte-swapping CID. */
	if (BIT_TEST_SET_ATOMIC(NUM_CIDS, cmd->cid, qp->cids)) {
		_nvmft_send_generic_error(qp, nc, NVME_SC_COMMAND_ID_CONFLICT);
		nvmf_free_capsule(nc);
		return;
	}

	if (qp->admin)
		nvmft_handle_admin_command(ctrlr, nc);
	else
		nvmft_handle_io_command(qp, qp->qid, nc);
}

struct nvmft_qpair *
nvmft_qpair_init(enum nvmf_trtype trtype,
    const struct nvmf_handoff_qpair_params *handoff, uint16_t qid,
    const char *name)
{
	struct nvmft_qpair *qp;

	qp = malloc(sizeof(*qp), M_NVMFT, M_WAITOK | M_ZERO);
	qp->admin = handoff->admin;
	qp->sq_flow_control = handoff->sq_flow_control;
	qp->qsize = handoff->qsize;
	qp->qid = qid;
	qp->sqhd = handoff->sqhd;
	qp->sqtail = handoff->sqtail;
	strlcpy(qp->name, name, sizeof(qp->name));
	mtx_init(&qp->lock, "nvmft qp", NULL, MTX_DEF);
	qp->cids = BITSET_ALLOC(NUM_CIDS, M_NVMFT, M_WAITOK | M_ZERO);
	STAILQ_INIT(&qp->datamove_queue);
	TASK_INIT(&qp->datamove_task, 0, nvmft_datamove_task, qp);

	qp->qp = nvmf_allocate_qpair(trtype, true, handoff, nvmft_qpair_error,
	    qp, nvmft_receive_capsule, qp);
	if (qp->qp == NULL) {
		mtx_destroy(&qp->lock);
		free(qp->cids, M_NVMFT);
		free(qp, M_NVMFT);
		return (NULL);
	}

	refcount_init(&qp->qp_refs, 1);
	return (qp);
}

void
nvmft_qpair_shutdown(struct nvmft_qpair *qp)
{
	STAILQ_HEAD(, ctl_io_hdr) datamove_queue;
	struct nvmf_qpair *nq;
	union ctl_io *io;

	STAILQ_INIT(&datamove_queue);
	mtx_lock(&qp->lock);
	nq = qp->qp;
	qp->qp = NULL;
	STAILQ_CONCAT(&datamove_queue, &qp->datamove_queue);
	mtx_unlock(&qp->lock);
	if (nq != NULL && refcount_release(&qp->qp_refs))
		nvmf_free_qpair(nq);

	while (!STAILQ_EMPTY(&datamove_queue)) {
		io = (union ctl_io *)STAILQ_FIRST(&datamove_queue);
		STAILQ_REMOVE_HEAD(&datamove_queue, links);
		nvmft_abort_datamove(io);
	}
	nvmft_drain_task(&qp->datamove_task);
}

void
nvmft_qpair_destroy(struct nvmft_qpair *qp)
{
	nvmft_qpair_shutdown(qp);
	mtx_destroy(&qp->lock);
	free(qp->cids, M_NVMFT);
	free(qp, M_NVMFT);
}

struct nvmft_controller *
nvmft_qpair_ctrlr(struct nvmft_qpair *qp)
{
	return (qp->ctrlr);
}

uint16_t
nvmft_qpair_id(struct nvmft_qpair *qp)
{
	return (qp->qid);
}

const char *
nvmft_qpair_name(struct nvmft_qpair *qp)
{
	return (qp->name);
}

static int
_nvmft_send_response(struct nvmft_qpair *qp, const void *cqe)
{
	struct nvme_completion cpl;
	struct nvmf_qpair *nq;
	struct nvmf_capsule *rc;
	int error;

	memcpy(&cpl, cqe, sizeof(cpl));
	mtx_lock(&qp->lock);
	nq = qp->qp;
	if (nq == NULL) {
		mtx_unlock(&qp->lock);
		return (ENOTCONN);
	}
	refcount_acquire(&qp->qp_refs);

	/* Set SQHD. */
	if (qp->sq_flow_control) {
		qp->sqhd = (qp->sqhd + 1) % qp->qsize;
		cpl.sqhd = htole16(qp->sqhd);
	} else
		cpl.sqhd = 0;
	mtx_unlock(&qp->lock);

	rc = nvmf_allocate_response(nq, &cpl, M_WAITOK);
	error = nvmf_transmit_capsule(rc);
	nvmf_free_capsule(rc);

	if (refcount_release(&qp->qp_refs))
		nvmf_free_qpair(nq);
	return (error);
}

void
nvmft_command_completed(struct nvmft_qpair *qp, struct nvmf_capsule *nc)
{
	const struct nvme_command *cmd = nvmf_capsule_sqe(nc);

	/* Don't bother byte-swapping CID. */
	KASSERT(BIT_ISSET(NUM_CIDS, cmd->cid, qp->cids),
	    ("%s: CID %u not busy", __func__, cmd->cid));

	BIT_CLR_ATOMIC(NUM_CIDS, cmd->cid, qp->cids);
}

int
nvmft_send_response(struct nvmft_qpair *qp, const void *cqe)
{
	const struct nvme_completion *cpl = cqe;

	/* Don't bother byte-swapping CID. */
	KASSERT(BIT_ISSET(NUM_CIDS, cpl->cid, qp->cids),
	    ("%s: CID %u not busy", __func__, cpl->cid));

	BIT_CLR_ATOMIC(NUM_CIDS, cpl->cid, qp->cids);
	return (_nvmft_send_response(qp, cqe));
}

void
nvmft_init_cqe(void *cqe, struct nvmf_capsule *nc, uint16_t status)
{
	struct nvme_completion *cpl = cqe;
	const struct nvme_command *cmd = nvmf_capsule_sqe(nc);

	memset(cpl, 0, sizeof(*cpl));
	cpl->cid = cmd->cid;
	cpl->status = htole16(status);
}

int
nvmft_send_error(struct nvmft_qpair *qp, struct nvmf_capsule *nc,
    uint8_t sc_type, uint8_t sc_status)
{
	struct nvme_completion cpl;
	uint16_t status;

	status = NVMEF(NVME_STATUS_SCT, sc_type) |
	    NVMEF(NVME_STATUS_SC, sc_status);
	nvmft_init_cqe(&cpl, nc, status);
	return (nvmft_send_response(qp, &cpl));
}

int
nvmft_send_generic_error(struct nvmft_qpair *qp, struct nvmf_capsule *nc,
    uint8_t sc_status)
{
	return (nvmft_send_error(qp, nc, NVME_SCT_GENERIC, sc_status));
}

/*
 * This version doesn't clear CID in qp->cids and is used for errors
 * before the CID is validated.
 */
static int
_nvmft_send_generic_error(struct nvmft_qpair *qp, struct nvmf_capsule *nc,
    uint8_t sc_status)
{
	struct nvme_completion cpl;
	uint16_t status;

	status = NVMEF(NVME_STATUS_SCT, NVME_SCT_GENERIC) |
	    NVMEF(NVME_STATUS_SC, sc_status);
	nvmft_init_cqe(&cpl, nc, status);
	return (_nvmft_send_response(qp, &cpl));
}

int
nvmft_send_success(struct nvmft_qpair *qp, struct nvmf_capsule *nc)
{
	return (nvmft_send_generic_error(qp, nc, NVME_SC_SUCCESS));
}

static void
nvmft_init_connect_rsp(struct nvmf_fabric_connect_rsp *rsp,
    const struct nvmf_fabric_connect_cmd *cmd, uint16_t status)
{
	memset(rsp, 0, sizeof(*rsp));
	rsp->cid = cmd->cid;
	rsp->status = htole16(status);
}

static int
nvmft_send_connect_response(struct nvmft_qpair *qp,
    const struct nvmf_fabric_connect_rsp *rsp)
{
	struct nvmf_capsule *rc;
	struct nvmf_qpair *nq;
	int error;

	mtx_lock(&qp->lock);
	nq = qp->qp;
	if (nq == NULL) {
		mtx_unlock(&qp->lock);
		return (ENOTCONN);
	}
	refcount_acquire(&qp->qp_refs);
	mtx_unlock(&qp->lock);

	rc = nvmf_allocate_response(qp->qp, rsp, M_WAITOK);
	error = nvmf_transmit_capsule(rc);
	nvmf_free_capsule(rc);

	if (refcount_release(&qp->qp_refs))
		nvmf_free_qpair(nq);
	return (error);
}

void
nvmft_connect_error(struct nvmft_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd, uint8_t sc_type,
    uint8_t sc_status)
{
	struct nvmf_fabric_connect_rsp rsp;
	uint16_t status;

	status = NVMEF(NVME_STATUS_SCT, sc_type) |
	    NVMEF(NVME_STATUS_SC, sc_status);
	nvmft_init_connect_rsp(&rsp, cmd, status);
	nvmft_send_connect_response(qp, &rsp);
}

void
nvmft_connect_invalid_parameters(struct nvmft_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd, bool data, uint16_t offset)
{
	struct nvmf_fabric_connect_rsp rsp;

	nvmft_init_connect_rsp(&rsp, cmd,
	    NVMEF(NVME_STATUS_SCT, NVME_SCT_COMMAND_SPECIFIC) |
	    NVMEF(NVME_STATUS_SC, NVMF_FABRIC_SC_INVALID_PARAM));
	rsp.status_code_specific.invalid.ipo = htole16(offset);
	rsp.status_code_specific.invalid.iattr = data ? 1 : 0;
	nvmft_send_connect_response(qp, &rsp);
}

int
nvmft_finish_accept(struct nvmft_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd, struct nvmft_controller *ctrlr)
{
	struct nvmf_fabric_connect_rsp rsp;

	qp->ctrlr = ctrlr;
	nvmft_init_connect_rsp(&rsp, cmd, 0);
	if (qp->sq_flow_control)
		rsp.sqhd = htole16(qp->sqhd);
	else
		rsp.sqhd = htole16(0xffff);
	rsp.status_code_specific.success.cntlid = htole16(ctrlr->cntlid);
	return (nvmft_send_connect_response(qp, &rsp));
}

void
nvmft_qpair_datamove(struct nvmft_qpair *qp, union ctl_io *io)
{
	bool enqueue_task;

	mtx_lock(&qp->lock);
	if (qp->qp == NULL) {
		mtx_unlock(&qp->lock);
		nvmft_abort_datamove(io);
		return;
	}
	enqueue_task = STAILQ_EMPTY(&qp->datamove_queue);
	STAILQ_INSERT_TAIL(&qp->datamove_queue, &io->io_hdr, links);
	mtx_unlock(&qp->lock);
	if (enqueue_task)
		nvmft_enqueue_task(&qp->datamove_task);
}

static void
nvmft_datamove_task(void *context, int pending __unused)
{
	struct nvmft_qpair *qp = context;
	union ctl_io *io;
	bool abort;

	mtx_lock(&qp->lock);
	while (!STAILQ_EMPTY(&qp->datamove_queue)) {
		io = (union ctl_io *)STAILQ_FIRST(&qp->datamove_queue);
		STAILQ_REMOVE_HEAD(&qp->datamove_queue, links);
		abort = (qp->qp == NULL);
		mtx_unlock(&qp->lock);
		if (abort)
			nvmft_abort_datamove(io);
		else
			nvmft_handle_datamove(io);
		mtx_lock(&qp->lock);
	}
	mtx_unlock(&qp->lock);
}
