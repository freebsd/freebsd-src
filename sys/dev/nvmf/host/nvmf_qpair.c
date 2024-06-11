/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/host/nvmf_var.h>

struct nvmf_host_command {
	struct nvmf_request *req;
	TAILQ_ENTRY(nvmf_host_command) link;
	uint16_t cid;
};

struct nvmf_host_qpair {
	struct nvmf_softc *sc;
	struct nvmf_qpair *qp;

	bool	sq_flow_control;
	bool	shutting_down;
	u_int	allocating;
	u_int	num_commands;
	uint16_t sqhd;
	uint16_t sqtail;

	struct mtx lock;

	TAILQ_HEAD(, nvmf_host_command) free_commands;
	STAILQ_HEAD(, nvmf_request) pending_requests;

	/* Indexed by cid. */
	struct nvmf_host_command **active_commands;

	char	name[16];
};

struct nvmf_request *
nvmf_allocate_request(struct nvmf_host_qpair *qp, void *sqe,
    nvmf_request_complete_t *cb, void *cb_arg, int how)
{
	struct nvmf_request *req;
	struct nvmf_qpair *nq;

	KASSERT(how == M_WAITOK || how == M_NOWAIT,
	    ("%s: invalid how", __func__));

	req = malloc(sizeof(*req), M_NVMF, how | M_ZERO);
	if (req == NULL)
		return (NULL);

	mtx_lock(&qp->lock);
	nq = qp->qp;
	if (nq == NULL) {
		mtx_unlock(&qp->lock);
		free(req, M_NVMF);
		return (NULL);
	}
	qp->allocating++;
	MPASS(qp->allocating != 0);
	mtx_unlock(&qp->lock);

	req->qp = qp;
	req->cb = cb;
	req->cb_arg = cb_arg;
	req->nc = nvmf_allocate_command(nq, sqe, how);
	if (req->nc == NULL) {
		free(req, M_NVMF);
		req = NULL;
	}

	mtx_lock(&qp->lock);
	qp->allocating--;
	if (qp->allocating == 0 && qp->shutting_down)
		wakeup(qp);
	mtx_unlock(&qp->lock);

	return (req);
}

static void
nvmf_abort_request(struct nvmf_request *req, uint16_t cid)
{
	struct nvme_completion cqe;

	memset(&cqe, 0, sizeof(cqe));
	cqe.cid = cid;
	cqe.status = htole16(NVMEF(NVME_STATUS_SCT, NVME_SCT_PATH_RELATED) |
	    NVMEF(NVME_STATUS_SC, NVME_SC_COMMAND_ABORTED_BY_HOST));
	req->cb(req->cb_arg, &cqe);
}

void
nvmf_free_request(struct nvmf_request *req)
{
	if (req->nc != NULL)
		nvmf_free_capsule(req->nc);
	free(req, M_NVMF);
}

static void
nvmf_dispatch_command(struct nvmf_host_qpair *qp, struct nvmf_host_command *cmd)
{
	struct nvmf_softc *sc = qp->sc;
	struct nvme_command *sqe;
	struct nvmf_capsule *nc;
	int error;

	nc = cmd->req->nc;
	sqe = nvmf_capsule_sqe(nc);

	/*
	 * NB: Don't bother byte-swapping the cid so that receive
	 * doesn't have to swap.
	 */
	sqe->cid = cmd->cid;

	error = nvmf_transmit_capsule(nc);
	if (error != 0) {
		device_printf(sc->dev,
		    "failed to transmit capsule: %d, disconnecting\n", error);
		nvmf_disconnect(sc);
		return;
	}

	if (sc->ka_traffic)
		atomic_store_int(&sc->ka_active_tx_traffic, 1);
}

static void
nvmf_qp_error(void *arg, int error)
{
	struct nvmf_host_qpair *qp = arg;
	struct nvmf_softc *sc = qp->sc;

	/* Ignore simple close of queue pairs during shutdown. */
	if (!(sc->detaching && error == 0))
		device_printf(sc->dev, "error %d on %s, disconnecting\n", error,
		    qp->name);
	nvmf_disconnect(sc);
}

static void
nvmf_receive_capsule(void *arg, struct nvmf_capsule *nc)
{
	struct nvmf_host_qpair *qp = arg;
	struct nvmf_softc *sc = qp->sc;
	struct nvmf_host_command *cmd;
	struct nvmf_request *req;
	const struct nvme_completion *cqe;
	uint16_t cid;

	cqe = nvmf_capsule_cqe(nc);

	if (sc->ka_traffic)
		atomic_store_int(&sc->ka_active_rx_traffic, 1);

	/*
	 * NB: Don't bother byte-swapping the cid as transmit doesn't
	 * swap either.
	 */
	cid = cqe->cid;

	if (cid > qp->num_commands) {
		device_printf(sc->dev,
		    "received invalid CID %u, disconnecting\n", cid);
		nvmf_disconnect(sc);
		nvmf_free_capsule(nc);
		return;
	}

	/*
	 * If the queue has been shutdown due to an error, silently
	 * drop the response.
	 */
	mtx_lock(&qp->lock);
	if (qp->qp == NULL) {
		device_printf(sc->dev,
		    "received completion for CID %u on shutdown %s\n", cid,
		    qp->name);
		mtx_unlock(&qp->lock);
		nvmf_free_capsule(nc);
		return;
	}

	cmd = qp->active_commands[cid];
	if (cmd == NULL) {
		mtx_unlock(&qp->lock);
		device_printf(sc->dev,
		    "received completion for inactive CID %u, disconnecting\n",
		    cid);
		nvmf_disconnect(sc);
		nvmf_free_capsule(nc);
		return;
	}

	KASSERT(cmd->cid == cid, ("%s: CID mismatch", __func__));
	req = cmd->req;
	cmd->req = NULL;
	if (STAILQ_EMPTY(&qp->pending_requests)) {
		qp->active_commands[cid] = NULL;
		TAILQ_INSERT_TAIL(&qp->free_commands, cmd, link);
		mtx_unlock(&qp->lock);
	} else {
		cmd->req = STAILQ_FIRST(&qp->pending_requests);
		STAILQ_REMOVE_HEAD(&qp->pending_requests, link);
		mtx_unlock(&qp->lock);
		nvmf_dispatch_command(qp, cmd);
	}

	req->cb(req->cb_arg, cqe);
	nvmf_free_capsule(nc);
	nvmf_free_request(req);
}

struct nvmf_host_qpair *
nvmf_init_qp(struct nvmf_softc *sc, enum nvmf_trtype trtype,
    struct nvmf_handoff_qpair_params *handoff, const char *name)
{
	struct nvmf_host_command *cmd, *ncmd;
	struct nvmf_host_qpair *qp;
	u_int i;

	qp = malloc(sizeof(*qp), M_NVMF, M_WAITOK | M_ZERO);
	qp->sc = sc;
	qp->sq_flow_control = handoff->sq_flow_control;
	qp->sqhd = handoff->sqhd;
	qp->sqtail = handoff->sqtail;
	strlcpy(qp->name, name, sizeof(qp->name));
	mtx_init(&qp->lock, "nvmf qp", NULL, MTX_DEF);

	/*
	 * Allocate a spare command slot for each pending AER command
	 * on the admin queue.
	 */
	qp->num_commands = handoff->qsize - 1;
	if (handoff->admin)
		qp->num_commands += sc->num_aer;

	qp->active_commands = malloc(sizeof(*qp->active_commands) *
	    qp->num_commands, M_NVMF, M_WAITOK | M_ZERO);
	TAILQ_INIT(&qp->free_commands);
	for (i = 0; i < qp->num_commands; i++) {
		cmd = malloc(sizeof(*cmd), M_NVMF, M_WAITOK | M_ZERO);
		cmd->cid = i;
		TAILQ_INSERT_TAIL(&qp->free_commands, cmd, link);
	}
	STAILQ_INIT(&qp->pending_requests);

	qp->qp = nvmf_allocate_qpair(trtype, false, handoff, nvmf_qp_error,
	    qp, nvmf_receive_capsule, qp);
	if (qp->qp == NULL) {
		TAILQ_FOREACH_SAFE(cmd, &qp->free_commands, link, ncmd) {
			TAILQ_REMOVE(&qp->free_commands, cmd, link);
			free(cmd, M_NVMF);
		}
		free(qp->active_commands, M_NVMF);
		mtx_destroy(&qp->lock);
		free(qp, M_NVMF);
		return (NULL);
	}

	return (qp);
}

void
nvmf_shutdown_qp(struct nvmf_host_qpair *qp)
{
	struct nvmf_host_command *cmd;
	struct nvmf_request *req;
	struct nvmf_qpair *nq;

	mtx_lock(&qp->lock);
	nq = qp->qp;
	qp->qp = NULL;

	if (nq == NULL) {
		while (qp->shutting_down)
			mtx_sleep(qp, &qp->lock, 0, "nvmfqpsh", 0);
		mtx_unlock(&qp->lock);
		return;
	}
	qp->shutting_down = true;
	while (qp->allocating != 0)
		mtx_sleep(qp, &qp->lock, 0, "nvmfqpqu", 0);
	mtx_unlock(&qp->lock);

	nvmf_free_qpair(nq);

	/*
	 * Abort outstanding requests.  Active requests will have
	 * their I/O completions invoked and associated capsules freed
	 * by the transport layer via nvmf_free_qpair.  Pending
	 * requests must have their I/O completion invoked via
	 * nvmf_abort_capsule_data.
	 */
	for (u_int i = 0; i < qp->num_commands; i++) {
		cmd = qp->active_commands[i];
		if (cmd != NULL) {
			if (!cmd->req->aer)
				printf("%s: aborted active command %p (CID %u)\n",
				    __func__, cmd->req, cmd->cid);

			/* This was freed by nvmf_free_qpair. */
			cmd->req->nc = NULL;
			nvmf_abort_request(cmd->req, cmd->cid);
			nvmf_free_request(cmd->req);
			free(cmd, M_NVMF);
		}
	}
	while (!STAILQ_EMPTY(&qp->pending_requests)) {
		req = STAILQ_FIRST(&qp->pending_requests);
		STAILQ_REMOVE_HEAD(&qp->pending_requests, link);
		if (!req->aer)
			printf("%s: aborted pending command %p\n", __func__,
			    req);
		nvmf_abort_capsule_data(req->nc, ECONNABORTED);
		nvmf_abort_request(req, 0);
		nvmf_free_request(req);
	}

	mtx_lock(&qp->lock);
	qp->shutting_down = false;
	mtx_unlock(&qp->lock);
	wakeup(qp);
}

void
nvmf_destroy_qp(struct nvmf_host_qpair *qp)
{
	struct nvmf_host_command *cmd, *ncmd;

	nvmf_shutdown_qp(qp);

	TAILQ_FOREACH_SAFE(cmd, &qp->free_commands, link, ncmd) {
		TAILQ_REMOVE(&qp->free_commands, cmd, link);
		free(cmd, M_NVMF);
	}
	free(qp->active_commands, M_NVMF);
	mtx_destroy(&qp->lock);
	free(qp, M_NVMF);
}

void
nvmf_submit_request(struct nvmf_request *req)
{
	struct nvmf_host_qpair *qp;
	struct nvmf_host_command *cmd;

	qp = req->qp;
	mtx_lock(&qp->lock);
	if (qp->qp == NULL) {
		mtx_unlock(&qp->lock);
		printf("%s: aborted pending command %p\n", __func__, req);
		nvmf_abort_capsule_data(req->nc, ECONNABORTED);
		nvmf_abort_request(req, 0);
		nvmf_free_request(req);
		return;
	}
	cmd = TAILQ_FIRST(&qp->free_commands);
	if (cmd == NULL) {
		/*
		 * Queue this request.  Will be sent after enough
		 * in-flight requests have completed.
		 */
		STAILQ_INSERT_TAIL(&qp->pending_requests, req, link);
		mtx_unlock(&qp->lock);
		return;
	}

	TAILQ_REMOVE(&qp->free_commands, cmd, link);
	KASSERT(qp->active_commands[cmd->cid] == NULL,
	    ("%s: CID already busy", __func__));
	qp->active_commands[cmd->cid] = cmd;
	cmd->req = req;
	mtx_unlock(&qp->lock);
	nvmf_dispatch_command(qp, cmd);
}
