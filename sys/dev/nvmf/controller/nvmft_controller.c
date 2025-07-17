/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>

#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/controller/nvmft_subr.h>
#include <dev/nvmf/controller/nvmft_var.h>

static void	nvmft_controller_shutdown(void *arg, int pending);
static void	nvmft_controller_terminate(void *arg, int pending);

int
nvmft_printf(struct nvmft_controller *ctrlr, const char *fmt, ...)
{
	char buf[128];
	struct sbuf sb;
	va_list ap;
	size_t retval;

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_set_drain(&sb, sbuf_printf_drain, &retval);

	sbuf_printf(&sb, "nvmft%u: ", ctrlr->cntlid);

	va_start(ap, fmt);
	sbuf_vprintf(&sb, fmt, ap);
	va_end(ap);

	sbuf_finish(&sb);
	sbuf_delete(&sb);

	return (retval);
}

static struct nvmft_controller *
nvmft_controller_alloc(struct nvmft_port *np, uint16_t cntlid,
    const struct nvmf_fabric_connect_data *data)
{
	struct nvmft_controller *ctrlr;

	ctrlr = malloc(sizeof(*ctrlr), M_NVMFT, M_WAITOK | M_ZERO);
	ctrlr->cntlid = cntlid;
	ctrlr->np = np;
	mtx_init(&ctrlr->lock, "nvmft controller", NULL, MTX_DEF);
	callout_init(&ctrlr->ka_timer, 1);
	TASK_INIT(&ctrlr->shutdown_task, 0, nvmft_controller_shutdown, ctrlr);
	TIMEOUT_TASK_INIT(taskqueue_thread, &ctrlr->terminate_task, 0,
	    nvmft_controller_terminate, ctrlr);

	ctrlr->cdata = np->cdata;
	ctrlr->cdata.ctrlr_id = htole16(cntlid);
	memcpy(ctrlr->hostid, data->hostid, sizeof(ctrlr->hostid));
	memcpy(ctrlr->hostnqn, data->hostnqn, sizeof(ctrlr->hostnqn));
	ctrlr->hip.power_cycles[0] = 1;
	ctrlr->create_time = sbinuptime();

	ctrlr->changed_ns = malloc(sizeof(*ctrlr->changed_ns), M_NVMFT,
	    M_WAITOK | M_ZERO);

	return (ctrlr);
}

static void
nvmft_controller_free(struct nvmft_controller *ctrlr)
{
	mtx_destroy(&ctrlr->lock);
	MPASS(ctrlr->io_qpairs == NULL);
	free(ctrlr->changed_ns, M_NVMFT);
	free(ctrlr, M_NVMFT);
}

static void
nvmft_keep_alive_timer(void *arg)
{
	struct nvmft_controller *ctrlr = arg;
	int traffic;

	if (ctrlr->shutdown)
		return;

	traffic = atomic_readandclear_int(&ctrlr->ka_active_traffic);
	if (traffic == 0) {
		nvmft_printf(ctrlr,
		    "disconnecting due to KeepAlive timeout\n");
		nvmft_controller_error(ctrlr, NULL, ETIMEDOUT);
		return;
	}

	callout_schedule_sbt(&ctrlr->ka_timer, ctrlr->ka_sbt, 0, C_HARDCLOCK);
}

int
nvmft_handoff_admin_queue(struct nvmft_port *np, enum nvmf_trtype trtype,
    const nvlist_t *params, const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data)
{
	struct nvmft_controller *ctrlr;
	struct nvmft_qpair *qp;
	uint32_t kato;
	int cntlid;

	if (cmd->qid != htole16(0))
		return (EINVAL);

	qp = nvmft_qpair_init(trtype, params, 0, "admin queue");
	if (qp == NULL) {
		printf("NVMFT: Failed to setup admin queue from %.*s\n",
		    (int)sizeof(data->hostnqn), data->hostnqn);
		return (ENXIO);
	}

	mtx_lock(&np->lock);
	cntlid = alloc_unr(np->ids);
	if (cntlid == -1) {
		mtx_unlock(&np->lock);
		printf("NVMFT: Unable to allocate controller for %.*s\n",
		    (int)sizeof(data->hostnqn), data->hostnqn);
		nvmft_connect_error(qp, cmd, NVME_SCT_COMMAND_SPECIFIC,
		    NVMF_FABRIC_SC_INVALID_HOST);
		nvmft_qpair_destroy(qp);
		return (ENOMEM);
	}

#ifdef INVARIANTS
	TAILQ_FOREACH(ctrlr, &np->controllers, link) {
		KASSERT(ctrlr->cntlid != cntlid,
		    ("%s: duplicate controllers with id %d", __func__, cntlid));
	}
#endif
	mtx_unlock(&np->lock);

	ctrlr = nvmft_controller_alloc(np, cntlid, data);

	mtx_lock(&np->lock);
	if (!np->online) {
		mtx_unlock(&np->lock);
		nvmft_controller_free(ctrlr);
		free_unr(np->ids, cntlid);
		nvmft_qpair_destroy(qp);
		return (ENXIO);
	}
	nvmft_port_ref(np);
	TAILQ_INSERT_TAIL(&np->controllers, ctrlr, link);

	nvmft_printf(ctrlr, "associated with %.*s\n",
	    (int)sizeof(data->hostnqn), data->hostnqn);
	ctrlr->admin = qp;
	ctrlr->trtype = trtype;

	/*
	 * The spec requires a non-zero KeepAlive timer, but allow a
	 * zero KATO value to match Linux.
	 */
	kato = le32toh(cmd->kato);
	if (kato != 0) {
		/*
		 * Round up to 1 second matching granularity
		 * advertised in cdata.
		 */
		ctrlr->ka_sbt = mstosbt(roundup(kato, 1000));
		callout_reset_sbt(&ctrlr->ka_timer, ctrlr->ka_sbt, 0,
		    nvmft_keep_alive_timer, ctrlr, C_HARDCLOCK);
	}
	mtx_unlock(&np->lock);

	nvmft_finish_accept(qp, cmd, ctrlr);

	return (0);
}

int
nvmft_handoff_io_queue(struct nvmft_port *np, enum nvmf_trtype trtype,
    const nvlist_t *params, const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data)
{
	struct nvmft_controller *ctrlr;
	struct nvmft_qpair *qp;
	char name[16];
	uint16_t cntlid, qid;

	qid = le16toh(cmd->qid);
	if (qid == 0)
		return (EINVAL);
	cntlid = le16toh(data->cntlid);

	snprintf(name, sizeof(name), "I/O queue %u", qid);
	qp = nvmft_qpair_init(trtype, params, qid, name);
	if (qp == NULL) {
		printf("NVMFT: Failed to setup I/O queue %u from %.*s\n", qid,
		    (int)sizeof(data->hostnqn), data->hostnqn);
		return (ENXIO);
	}

	mtx_lock(&np->lock);
	TAILQ_FOREACH(ctrlr, &np->controllers, link) {
		if (ctrlr->cntlid == cntlid)
			break;
	}
	if (ctrlr == NULL) {
		mtx_unlock(&np->lock);
		printf("NVMFT: Nonexistent controller %u for I/O queue %u from %.*s\n",
		    ctrlr->cntlid, qid, (int)sizeof(data->hostnqn),
		    data->hostnqn);
		nvmft_connect_invalid_parameters(qp, cmd, true,
		    offsetof(struct nvmf_fabric_connect_data, cntlid));
		nvmft_qpair_destroy(qp);
		return (ENOENT);
	}

	if (memcmp(ctrlr->hostid, data->hostid, sizeof(ctrlr->hostid)) != 0) {
		mtx_unlock(&np->lock);
		nvmft_printf(ctrlr,
		    "hostid mismatch for I/O queue %u from %.*s\n", qid,
		    (int)sizeof(data->hostnqn), data->hostnqn);
		nvmft_connect_invalid_parameters(qp, cmd, true,
		    offsetof(struct nvmf_fabric_connect_data, hostid));
		nvmft_qpair_destroy(qp);
		return (EINVAL);
	}
	if (memcmp(ctrlr->hostnqn, data->hostnqn, sizeof(ctrlr->hostnqn)) != 0) {
		mtx_unlock(&np->lock);
		nvmft_printf(ctrlr,
		    "hostnqn mismatch for I/O queue %u from %.*s\n", qid,
		    (int)sizeof(data->hostnqn), data->hostnqn);
		nvmft_connect_invalid_parameters(qp, cmd, true,
		    offsetof(struct nvmf_fabric_connect_data, hostnqn));
		nvmft_qpair_destroy(qp);
		return (EINVAL);
	}

	/* XXX: Require trtype == ctrlr->trtype? */

	mtx_lock(&ctrlr->lock);
	if (ctrlr->shutdown) {
		mtx_unlock(&ctrlr->lock);
		mtx_unlock(&np->lock);
		nvmft_printf(ctrlr,
		    "attempt to create I/O queue %u on disabled controller from %.*s\n",
		    qid, (int)sizeof(data->hostnqn), data->hostnqn);
		nvmft_connect_invalid_parameters(qp, cmd, true,
		    offsetof(struct nvmf_fabric_connect_data, cntlid));
		nvmft_qpair_destroy(qp);
		return (EINVAL);
	}
	if (ctrlr->num_io_queues == 0) {
		mtx_unlock(&ctrlr->lock);
		mtx_unlock(&np->lock);
		nvmft_printf(ctrlr,
		    "attempt to create I/O queue %u without enabled queues from %.*s\n",
		    qid, (int)sizeof(data->hostnqn), data->hostnqn);
		nvmft_connect_error(qp, cmd, NVME_SCT_GENERIC,
		    NVME_SC_COMMAND_SEQUENCE_ERROR);
		nvmft_qpair_destroy(qp);
		return (EINVAL);
	}
	if (cmd->qid > ctrlr->num_io_queues) {
		mtx_unlock(&ctrlr->lock);
		mtx_unlock(&np->lock);
		nvmft_printf(ctrlr,
		    "attempt to create invalid I/O queue %u from %.*s\n", qid,
		    (int)sizeof(data->hostnqn), data->hostnqn);
		nvmft_connect_invalid_parameters(qp, cmd, false,
		    offsetof(struct nvmf_fabric_connect_cmd, qid));
		nvmft_qpair_destroy(qp);
		return (EINVAL);
	}
	if (ctrlr->io_qpairs[qid - 1].qp != NULL) {
		mtx_unlock(&ctrlr->lock);
		mtx_unlock(&np->lock);
		nvmft_printf(ctrlr,
		    "attempt to re-create I/O queue %u from %.*s\n", qid,
		    (int)sizeof(data->hostnqn), data->hostnqn);
		nvmft_connect_error(qp, cmd, NVME_SCT_GENERIC,
		    NVME_SC_COMMAND_SEQUENCE_ERROR);
		nvmft_qpair_destroy(qp);
		return (EINVAL);
	}

	ctrlr->io_qpairs[qid - 1].qp = qp;
	mtx_unlock(&ctrlr->lock);
	mtx_unlock(&np->lock);
	nvmft_finish_accept(qp, cmd, ctrlr);

	return (0);
}

static void
nvmft_controller_shutdown(void *arg, int pending)
{
	struct nvmft_controller *ctrlr = arg;

	MPASS(pending == 1);

	/*
	 * Shutdown all I/O queues to terminate pending datamoves and
	 * stop receiving new commands.
	 */
	mtx_lock(&ctrlr->lock);
	for (u_int i = 0; i < ctrlr->num_io_queues; i++) {
		if (ctrlr->io_qpairs[i].qp != NULL) {
			ctrlr->io_qpairs[i].shutdown = true;
			mtx_unlock(&ctrlr->lock);
			nvmft_qpair_shutdown(ctrlr->io_qpairs[i].qp);
			mtx_lock(&ctrlr->lock);
		}
	}
	mtx_unlock(&ctrlr->lock);

	/* Terminate active CTL commands. */
	nvmft_terminate_commands(ctrlr);

	/* Wait for all pending CTL commands to complete. */
	mtx_lock(&ctrlr->lock);
	while (ctrlr->pending_commands != 0)
		mtx_sleep(&ctrlr->pending_commands, &ctrlr->lock, 0, "nvmftsh",
		    hz / 100);
	mtx_unlock(&ctrlr->lock);

	/* Delete all of the I/O queues. */
	for (u_int i = 0; i < ctrlr->num_io_queues; i++) {
		if (ctrlr->io_qpairs[i].qp != NULL)
			nvmft_qpair_destroy(ctrlr->io_qpairs[i].qp);
	}
	free(ctrlr->io_qpairs, M_NVMFT);
	ctrlr->io_qpairs = NULL;

	mtx_lock(&ctrlr->lock);
	ctrlr->num_io_queues = 0;

	/* Mark shutdown complete. */
	if (NVMEV(NVME_CSTS_REG_SHST, ctrlr->csts) == NVME_SHST_OCCURRING) {
		ctrlr->csts &= ~NVMEM(NVME_CSTS_REG_SHST);
		ctrlr->csts |= NVMEF(NVME_CSTS_REG_SHST, NVME_SHST_COMPLETE);
	}

	if (NVMEV(NVME_CSTS_REG_CFS, ctrlr->csts) == 0) {
		ctrlr->csts &= ~NVMEM(NVME_CSTS_REG_RDY);
		ctrlr->shutdown = false;
	}
	mtx_unlock(&ctrlr->lock);

	/*
	 * If the admin queue was closed while shutting down or a
	 * fatal controller error has occurred, terminate the
	 * association immediately, otherwise wait up to 2 minutes
	 * (NVMe-over-Fabrics 1.1 4.6).
	 */
	if (ctrlr->admin_closed || NVMEV(NVME_CSTS_REG_CFS, ctrlr->csts) != 0)
		nvmft_controller_terminate(ctrlr, 0);
	else
		taskqueue_enqueue_timeout(taskqueue_thread,
		    &ctrlr->terminate_task, hz * 60 * 2);
}

static void
nvmft_controller_terminate(void *arg, int pending)
{
	struct nvmft_controller *ctrlr = arg;
	struct nvmft_port *np;
	bool wakeup_np;

	/* If the controller has been re-enabled, nothing to do. */
	mtx_lock(&ctrlr->lock);
	if (NVMEV(NVME_CC_REG_EN, ctrlr->cc) != 0) {
		mtx_unlock(&ctrlr->lock);

		if (ctrlr->ka_sbt != 0)
			callout_schedule_sbt(&ctrlr->ka_timer, ctrlr->ka_sbt, 0,
			    C_HARDCLOCK);
		return;
	}

	/* Disable updates to CC while destroying admin qpair. */
	ctrlr->shutdown = true;
	mtx_unlock(&ctrlr->lock);

	nvmft_qpair_destroy(ctrlr->admin);

	/* Remove association (CNTLID). */
	np = ctrlr->np;
	mtx_lock(&np->lock);
	TAILQ_REMOVE(&np->controllers, ctrlr, link);
	wakeup_np = (!np->online && TAILQ_EMPTY(&np->controllers));
	mtx_unlock(&np->lock);
	free_unr(np->ids, ctrlr->cntlid);
	if (wakeup_np)
		wakeup(np);

	callout_drain(&ctrlr->ka_timer);

	nvmft_printf(ctrlr, "association terminated\n");
	nvmft_controller_free(ctrlr);
	nvmft_port_rele(np);
}

void
nvmft_controller_error(struct nvmft_controller *ctrlr, struct nvmft_qpair *qp,
    int error)
{
	/*
	 * If a queue pair is closed, that isn't an error per se.
	 * That just means additional commands cannot be received on
	 * that queue pair.
	 *
	 * If the admin queue pair is closed while idle or while
	 * shutting down, terminate the association immediately.
	 *
	 * If an I/O queue pair is closed, just ignore it.
	 */
	if (error == 0) {
		if (qp != ctrlr->admin)
			return;

		mtx_lock(&ctrlr->lock);
		if (ctrlr->shutdown) {
			ctrlr->admin_closed = true;
			mtx_unlock(&ctrlr->lock);
			return;
		}

		if (NVMEV(NVME_CC_REG_EN, ctrlr->cc) == 0) {
			MPASS(ctrlr->num_io_queues == 0);
			mtx_unlock(&ctrlr->lock);

			/*
			 * Ok to drop lock here since ctrlr->cc can't
			 * change if the admin queue pair has closed.
			 * This also means no new queues can be handed
			 * off, etc.  Note that since there are no I/O
			 * queues, only the admin queue needs to be
			 * destroyed, so it is safe to skip
			 * nvmft_controller_shutdown and just schedule
			 * nvmft_controller_terminate.  Note that we
			 * cannot call nvmft_controller_terminate from
			 * here directly as this is called from the
			 * transport layer and freeing the admin qpair
			 * might deadlock waiting for the current
			 * thread to exit.
			 */
			if (taskqueue_cancel_timeout(taskqueue_thread,
			    &ctrlr->terminate_task, NULL) == 0)
				taskqueue_enqueue_timeout(taskqueue_thread,
				    &ctrlr->terminate_task, 0);
			return;
		}

		/*
		 * Treat closing of the admin queue pair while enabled
		 * as a transport error.  Note that the admin queue
		 * pair has been closed.
		 */
		ctrlr->admin_closed = true;
	} else
		mtx_lock(&ctrlr->lock);

	/* Ignore transport errors if we are already shutting down. */
	if (ctrlr->shutdown) {
		mtx_unlock(&ctrlr->lock);
		return;
	}

	ctrlr->csts |= NVMEF(NVME_CSTS_REG_CFS, 1);
	ctrlr->cc &= ~NVMEM(NVME_CC_REG_EN);
	ctrlr->shutdown = true;
	mtx_unlock(&ctrlr->lock);

	callout_stop(&ctrlr->ka_timer);
	taskqueue_enqueue(taskqueue_thread, &ctrlr->shutdown_task);
}

/* Wrapper around m_getm2 that also sets m_len in the mbufs in the chain. */
static struct mbuf *
m_getml(size_t len, int how)
{
	struct mbuf *m, *n;

	m = m_getm2(NULL, len, how, MT_DATA, 0);
	if (m == NULL)
		return (NULL);
	for (n = m; len > 0; n = n->m_next) {
		n->m_len = M_SIZE(n);
		if (n->m_len >= len) {
			n->m_len = len;
			MPASS(n->m_next == NULL);
		}
		len -= n->m_len;
	}
	return (m);
}

static void
m_zero(struct mbuf *m, u_int offset, u_int len)
{
	u_int todo;

	if (len == 0)
		return;

	while (m->m_len <= offset) {
		offset -= m->m_len;
		m = m->m_next;
	}

	todo = m->m_len - offset;
	if (todo > len)
		todo = len;
	memset(mtodo(m, offset), 0, todo);
	m = m->m_next;
	len -= todo;

	while (len > 0) {
		todo = m->m_len;
		if (todo > len)
			todo = len;
		memset(mtod(m, void *), 0, todo);
		m = m->m_next;
		len -= todo;
	}
}

static void
handle_get_log_page(struct nvmft_controller *ctrlr,
    struct nvmf_capsule *nc, const struct nvme_command *cmd)
{
	struct mbuf *m;
	uint64_t offset;
	uint32_t numd;
	size_t len, todo;
	u_int status;
	uint8_t lid;
	bool rae;

	lid = le32toh(cmd->cdw10) & 0xff;
	rae = (le32toh(cmd->cdw10) & (1U << 15)) != 0;
	numd = le32toh(cmd->cdw10) >> 16 | le32toh(cmd->cdw11) << 16;
	offset = le32toh(cmd->cdw12) | (uint64_t)le32toh(cmd->cdw13) << 32;

	if (offset % 3 != 0) {
		status = NVME_SC_INVALID_FIELD;
		goto done;
	}

	len = (numd + 1) * 4;

	switch (lid) {
	case NVME_LOG_ERROR:
		todo = 0;

		m = m_getml(len, M_WAITOK);
		if (todo != len)
			m_zero(m, todo, len - todo);
		status = nvmf_send_controller_data(nc, 0, m, len);
		MPASS(status != NVMF_MORE);
		break;
	case NVME_LOG_HEALTH_INFORMATION:
	{
		struct nvme_health_information_page hip;

		if (offset >= sizeof(hip)) {
			status = NVME_SC_INVALID_FIELD;
			goto done;
		}
		todo = sizeof(hip) - offset;
		if (todo > len)
			todo = len;

		mtx_lock(&ctrlr->lock);
		hip = ctrlr->hip;
		hip.controller_busy_time[0] =
		    sbintime_getsec(ctrlr->busy_total) / 60;
		hip.power_on_hours[0] =
		    sbintime_getsec(sbinuptime() - ctrlr->create_time) / 3600;
		mtx_unlock(&ctrlr->lock);

		m = m_getml(len, M_WAITOK);
		m_copyback(m, 0, todo, (char *)&hip + offset);
		if (todo != len)
			m_zero(m, todo, len - todo);
		status = nvmf_send_controller_data(nc, 0, m, len);
		MPASS(status != NVMF_MORE);
		break;
	}
	case NVME_LOG_FIRMWARE_SLOT:
		if (offset >= sizeof(ctrlr->np->fp)) {
			status = NVME_SC_INVALID_FIELD;
			goto done;
		}
		todo = sizeof(ctrlr->np->fp) - offset;
		if (todo > len)
			todo = len;

		m = m_getml(len, M_WAITOK);
		m_copyback(m, 0, todo, (char *)&ctrlr->np->fp + offset);
		if (todo != len)
			m_zero(m, todo, len - todo);
		status = nvmf_send_controller_data(nc, 0, m, len);
		MPASS(status != NVMF_MORE);
		break;
	case NVME_LOG_CHANGED_NAMESPACE:
		if (offset >= sizeof(*ctrlr->changed_ns)) {
			status = NVME_SC_INVALID_FIELD;
			goto done;
		}
		todo = sizeof(*ctrlr->changed_ns) - offset;
		if (todo > len)
			todo = len;

		m = m_getml(len, M_WAITOK);
		mtx_lock(&ctrlr->lock);
		m_copyback(m, 0, todo, (char *)ctrlr->changed_ns + offset);
		if (offset == 0 && len == sizeof(*ctrlr->changed_ns))
			memset(ctrlr->changed_ns, 0,
			    sizeof(*ctrlr->changed_ns));
		if (!rae)
			ctrlr->changed_ns_reported = false;
		mtx_unlock(&ctrlr->lock);
		if (todo != len)
			m_zero(m, todo, len - todo);
		status = nvmf_send_controller_data(nc, 0, m, len);
		MPASS(status != NVMF_MORE);
		break;
	default:
		nvmft_printf(ctrlr, "Unsupported page %#x for GET_LOG_PAGE\n",
		    lid);
		status = NVME_SC_INVALID_FIELD;
		break;
	}

done:
	if (status == NVMF_SUCCESS_SENT)
		nvmft_command_completed(ctrlr->admin, nc);
	else
		nvmft_send_generic_error(ctrlr->admin, nc, status);
	nvmf_free_capsule(nc);
}

static void
m_free_nslist(struct mbuf *m)
{
	free(m->m_ext.ext_arg1, M_NVMFT);
}

static void
handle_identify_command(struct nvmft_controller *ctrlr,
    struct nvmf_capsule *nc, const struct nvme_command *cmd)
{
	struct mbuf *m;
	size_t data_len;
	u_int status;
	uint8_t cns;

	cns = le32toh(cmd->cdw10) & 0xFF;
	data_len = nvmf_capsule_data_len(nc);
	if (data_len != sizeof(ctrlr->cdata)) {
		nvmft_printf(ctrlr,
		    "Invalid length %zu for IDENTIFY with CNS %#x\n", data_len,
		    cns);
		nvmft_send_generic_error(ctrlr->admin, nc,
		    NVME_SC_INVALID_OPCODE);
		nvmf_free_capsule(nc);
		return;
	}

	switch (cns) {
	case 0:	/* Namespace data. */
	case 3:	/* Namespace Identification Descriptor list. */
		nvmft_dispatch_command(ctrlr->admin, nc, true);
		return;
	case 1:
		/* Controller data. */
		m = m_getml(sizeof(ctrlr->cdata), M_WAITOK);
		m_copyback(m, 0, sizeof(ctrlr->cdata), (void *)&ctrlr->cdata);
		status = nvmf_send_controller_data(nc, 0, m,
		    sizeof(ctrlr->cdata));
		MPASS(status != NVMF_MORE);
		break;
	case 2:
	{
		/* Active namespace list. */
		struct nvme_ns_list *nslist;
		uint32_t nsid;

		nsid = le32toh(cmd->nsid);
		if (nsid >= 0xfffffffe) {
			status = NVME_SC_INVALID_FIELD;
			break;
		}

		nslist = malloc(sizeof(*nslist), M_NVMFT, M_WAITOK | M_ZERO);
		nvmft_populate_active_nslist(ctrlr->np, nsid, nslist);
		m = m_get(M_WAITOK, MT_DATA);
		m_extadd(m, (void *)nslist, sizeof(*nslist), m_free_nslist,
		    nslist, NULL, 0, EXT_CTL);
		m->m_len = sizeof(*nslist);
		status = nvmf_send_controller_data(nc, 0, m, m->m_len);
		MPASS(status != NVMF_MORE);
		break;
	}
	default:
		nvmft_printf(ctrlr, "Unsupported CNS %#x for IDENTIFY\n", cns);
		status = NVME_SC_INVALID_FIELD;
		break;
	}

	if (status == NVMF_SUCCESS_SENT)
		nvmft_command_completed(ctrlr->admin, nc);
	else
		nvmft_send_generic_error(ctrlr->admin, nc, status);
	nvmf_free_capsule(nc);
}

static void
handle_set_features(struct nvmft_controller *ctrlr,
    struct nvmf_capsule *nc, const struct nvme_command *cmd)
{
	struct nvme_completion cqe;
	uint8_t fid;

	fid = NVMEV(NVME_FEAT_SET_FID, le32toh(cmd->cdw10));
	switch (fid) {
	case NVME_FEAT_NUMBER_OF_QUEUES:
	{
		uint32_t num_queues;
		struct nvmft_io_qpair *io_qpairs;

		num_queues = le32toh(cmd->cdw11) & 0xffff;

		/* 5.12.1.7: 65535 is invalid. */
		if (num_queues == 65535)
			goto error;

		/* Fabrics requires the same number of SQs and CQs. */
		if (le32toh(cmd->cdw11) >> 16 != num_queues)
			goto error;

		/* Convert to 1's based */
		num_queues++;

		io_qpairs = mallocarray(num_queues, sizeof(*io_qpairs),
		    M_NVMFT, M_WAITOK | M_ZERO);

		mtx_lock(&ctrlr->lock);
		if (ctrlr->num_io_queues != 0) {
			mtx_unlock(&ctrlr->lock);
			free(io_qpairs, M_NVMFT);
			nvmft_send_generic_error(ctrlr->admin, nc,
			    NVME_SC_COMMAND_SEQUENCE_ERROR);
			nvmf_free_capsule(nc);
			return;
		}

		ctrlr->num_io_queues = num_queues;
		ctrlr->io_qpairs = io_qpairs;
		mtx_unlock(&ctrlr->lock);

		nvmft_init_cqe(&cqe, nc, 0);
		cqe.cdw0 = cmd->cdw11;
		nvmft_send_response(ctrlr->admin, &cqe);
		nvmf_free_capsule(nc);
		return;
	}
	case NVME_FEAT_ASYNC_EVENT_CONFIGURATION:
	{
		uint32_t aer_mask;

		aer_mask = le32toh(cmd->cdw11);

		/* Check for any reserved or unimplemented feature bits. */
		if ((aer_mask & 0xffffc000) != 0)
			goto error;

		mtx_lock(&ctrlr->lock);
		ctrlr->aer_mask = aer_mask;
		mtx_unlock(&ctrlr->lock);
		nvmft_send_success(ctrlr->admin, nc);
		nvmf_free_capsule(nc);
		return;
	}
	default:
		nvmft_printf(ctrlr,
		    "Unsupported feature ID %u for SET_FEATURES\n", fid);
		goto error;
	}

error:
	nvmft_send_generic_error(ctrlr->admin, nc, NVME_SC_INVALID_FIELD);
	nvmf_free_capsule(nc);
}

static bool
update_cc(struct nvmft_controller *ctrlr, uint32_t new_cc, bool *need_shutdown)
{
	struct nvmft_port *np = ctrlr->np;
	uint32_t changes;

	*need_shutdown = false;

	mtx_lock(&ctrlr->lock);

	/* Don't allow any changes while shutting down. */
	if (ctrlr->shutdown) {
		mtx_unlock(&ctrlr->lock);
		return (false);
	}

	if (!_nvmf_validate_cc(np->max_io_qsize, np->cap, ctrlr->cc, new_cc)) {
		mtx_unlock(&ctrlr->lock);
		return (false);
	}

	changes = ctrlr->cc ^ new_cc;
	ctrlr->cc = new_cc;

	/* Handle shutdown requests. */
	if (NVMEV(NVME_CC_REG_SHN, changes) != 0 &&
	    NVMEV(NVME_CC_REG_SHN, new_cc) != 0) {
		ctrlr->csts &= ~NVMEM(NVME_CSTS_REG_SHST);
		ctrlr->csts |= NVMEF(NVME_CSTS_REG_SHST, NVME_SHST_OCCURRING);
		ctrlr->cc &= ~NVMEM(NVME_CC_REG_EN);
		ctrlr->shutdown = true;
		*need_shutdown = true;
		nvmft_printf(ctrlr, "shutdown requested\n");
	}

	if (NVMEV(NVME_CC_REG_EN, changes) != 0) {
		if (NVMEV(NVME_CC_REG_EN, new_cc) == 0) {
			/* Controller reset. */
			nvmft_printf(ctrlr, "reset requested\n");
			ctrlr->shutdown = true;
			*need_shutdown = true;
		} else
			ctrlr->csts |= NVMEF(NVME_CSTS_REG_RDY, 1);
	}
	mtx_unlock(&ctrlr->lock);

	return (true);
}

static void
handle_property_get(struct nvmft_controller *ctrlr, struct nvmf_capsule *nc,
    const struct nvmf_fabric_prop_get_cmd *pget)
{
	struct nvmf_fabric_prop_get_rsp rsp;

	nvmft_init_cqe(&rsp, nc, 0);

	switch (le32toh(pget->ofst)) {
	case NVMF_PROP_CAP:
		if (pget->attrib.size != NVMF_PROP_SIZE_8)
			goto error;
		rsp.value.u64 = htole64(ctrlr->np->cap);
		break;
	case NVMF_PROP_VS:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = ctrlr->cdata.ver;
		break;
	case NVMF_PROP_CC:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(ctrlr->cc);
		break;
	case NVMF_PROP_CSTS:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(ctrlr->csts);
		break;
	default:
		goto error;
	}

	nvmft_send_response(ctrlr->admin, &rsp);
	return;
error:
	nvmft_send_generic_error(ctrlr->admin, nc, NVME_SC_INVALID_FIELD);
}

static void
handle_property_set(struct nvmft_controller *ctrlr, struct nvmf_capsule *nc,
    const struct nvmf_fabric_prop_set_cmd *pset)
{
	bool need_shutdown;

	need_shutdown = false;
	switch (le32toh(pset->ofst)) {
	case NVMF_PROP_CC:
		if (pset->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		if (!update_cc(ctrlr, le32toh(pset->value.u32.low),
		    &need_shutdown))
			goto error;
		break;
	default:
		goto error;
	}

	nvmft_send_success(ctrlr->admin, nc);
	if (need_shutdown) {
		callout_stop(&ctrlr->ka_timer);
		taskqueue_enqueue(taskqueue_thread, &ctrlr->shutdown_task);
	}
	return;
error:
	nvmft_send_generic_error(ctrlr->admin, nc, NVME_SC_INVALID_FIELD);
}

static void
handle_admin_fabrics_command(struct nvmft_controller *ctrlr,
    struct nvmf_capsule *nc, const struct nvmf_fabric_cmd *fc)
{
	switch (fc->fctype) {
	case NVMF_FABRIC_COMMAND_PROPERTY_GET:
		handle_property_get(ctrlr, nc,
		    (const struct nvmf_fabric_prop_get_cmd *)fc);
		break;
	case NVMF_FABRIC_COMMAND_PROPERTY_SET:
		handle_property_set(ctrlr, nc,
		    (const struct nvmf_fabric_prop_set_cmd *)fc);
		break;
	case NVMF_FABRIC_COMMAND_CONNECT:
		nvmft_printf(ctrlr,
		    "CONNECT command on connected admin queue\n");
		nvmft_send_generic_error(ctrlr->admin, nc,
		    NVME_SC_COMMAND_SEQUENCE_ERROR);
		break;
	case NVMF_FABRIC_COMMAND_DISCONNECT:
		nvmft_printf(ctrlr, "DISCONNECT command on admin queue\n");
		nvmft_send_error(ctrlr->admin, nc, NVME_SCT_COMMAND_SPECIFIC,
		    NVMF_FABRIC_SC_INVALID_QUEUE_TYPE);
		break;
	default:
		nvmft_printf(ctrlr, "Unsupported fabrics command %#x\n",
		    fc->fctype);
		nvmft_send_generic_error(ctrlr->admin, nc,
		    NVME_SC_INVALID_OPCODE);
		break;
	}
	nvmf_free_capsule(nc);
}

void
nvmft_handle_admin_command(struct nvmft_controller *ctrlr,
    struct nvmf_capsule *nc)
{
	const struct nvme_command *cmd = nvmf_capsule_sqe(nc);

	/* Only permit Fabrics commands while a controller is disabled. */
	if (NVMEV(NVME_CC_REG_EN, ctrlr->cc) == 0 &&
	    cmd->opc != NVME_OPC_FABRICS_COMMANDS) {
		nvmft_printf(ctrlr,
		    "Unsupported admin opcode %#x while disabled\n", cmd->opc);
		nvmft_send_generic_error(ctrlr->admin, nc,
		    NVME_SC_COMMAND_SEQUENCE_ERROR);
		nvmf_free_capsule(nc);
		return;
	}

	atomic_store_int(&ctrlr->ka_active_traffic, 1);

	switch (cmd->opc) {
	case NVME_OPC_GET_LOG_PAGE:
		handle_get_log_page(ctrlr, nc, cmd);
		break;
	case NVME_OPC_IDENTIFY:
		handle_identify_command(ctrlr, nc, cmd);
		break;
	case NVME_OPC_SET_FEATURES:
		handle_set_features(ctrlr, nc, cmd);
		break;
	case NVME_OPC_ASYNC_EVENT_REQUEST:
		mtx_lock(&ctrlr->lock);
		if (ctrlr->aer_pending == NVMFT_NUM_AER) {
			mtx_unlock(&ctrlr->lock);
			nvmft_send_error(ctrlr->admin, nc,
			    NVME_SCT_COMMAND_SPECIFIC,
			    NVME_SC_ASYNC_EVENT_REQUEST_LIMIT_EXCEEDED);
		} else {
			/* NB: Store the CID without byte-swapping. */
			ctrlr->aer_cids[ctrlr->aer_pidx] = cmd->cid;
			ctrlr->aer_pending++;
			ctrlr->aer_pidx = (ctrlr->aer_pidx + 1) % NVMFT_NUM_AER;
			mtx_unlock(&ctrlr->lock);
		}
		nvmf_free_capsule(nc);
		break;
	case NVME_OPC_KEEP_ALIVE:
		nvmft_send_success(ctrlr->admin, nc);
		nvmf_free_capsule(nc);
		break;
	case NVME_OPC_FABRICS_COMMANDS:
		handle_admin_fabrics_command(ctrlr, nc,
		    (const struct nvmf_fabric_cmd *)cmd);
		break;
	default:
		nvmft_printf(ctrlr, "Unsupported admin opcode %#x\n", cmd->opc);
		nvmft_send_generic_error(ctrlr->admin, nc,
		    NVME_SC_INVALID_OPCODE);
		nvmf_free_capsule(nc);
		break;
	}
}

void
nvmft_handle_io_command(struct nvmft_qpair *qp, uint16_t qid,
    struct nvmf_capsule *nc)
{
	struct nvmft_controller *ctrlr = nvmft_qpair_ctrlr(qp);
	const struct nvme_command *cmd = nvmf_capsule_sqe(nc);

	atomic_store_int(&ctrlr->ka_active_traffic, 1);

	switch (cmd->opc) {
	case NVME_OPC_FLUSH:
		if (cmd->nsid == htole32(0xffffffff)) {
			nvmft_send_generic_error(qp, nc,
			    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
			nvmf_free_capsule(nc);
			break;
		}
		/* FALLTHROUGH */
	case NVME_OPC_WRITE:
	case NVME_OPC_READ:
	case NVME_OPC_WRITE_UNCORRECTABLE:
	case NVME_OPC_COMPARE:
	case NVME_OPC_WRITE_ZEROES:
	case NVME_OPC_DATASET_MANAGEMENT:
	case NVME_OPC_VERIFY:
		nvmft_dispatch_command(qp, nc, false);
		break;
	default:
		nvmft_printf(ctrlr, "Unsupported I/O opcode %#x\n", cmd->opc);
		nvmft_send_generic_error(qp, nc,
		    NVME_SC_INVALID_OPCODE);
		nvmf_free_capsule(nc);
		break;
	}
}

static void
nvmft_report_aer(struct nvmft_controller *ctrlr, uint32_t aer_mask,
    u_int type, uint8_t info, uint8_t log_page_id)
{
	struct nvme_completion cpl;

	MPASS(type <= 7);

	/* Drop events that are not enabled. */
	mtx_lock(&ctrlr->lock);
	if ((ctrlr->aer_mask & aer_mask) == 0) {
		mtx_unlock(&ctrlr->lock);
		return;
	}

	/*
	 * If there is no pending AER command, drop it.
	 * XXX: Should we queue these?
	 */
	if (ctrlr->aer_pending == 0) {
		mtx_unlock(&ctrlr->lock);
		nvmft_printf(ctrlr,
		    "dropping AER type %u, info %#x, page %#x\n",
		    type, info, log_page_id);
		return;
	}

	memset(&cpl, 0, sizeof(cpl));
	cpl.cid = ctrlr->aer_cids[ctrlr->aer_cidx];
	ctrlr->aer_pending--;
	ctrlr->aer_cidx = (ctrlr->aer_cidx + 1) % NVMFT_NUM_AER;
	mtx_unlock(&ctrlr->lock);

	cpl.cdw0 = htole32(NVMEF(NVME_ASYNC_EVENT_TYPE, type) |
	    NVMEF(NVME_ASYNC_EVENT_INFO, info) |
	    NVMEF(NVME_ASYNC_EVENT_LOG_PAGE_ID, log_page_id));

	nvmft_send_response(ctrlr->admin, &cpl);
}

void
nvmft_controller_lun_changed(struct nvmft_controller *ctrlr, int lun_id)
{
	struct nvme_ns_list *nslist;
	uint32_t new_nsid, nsid;
	u_int i;

	new_nsid = lun_id + 1;

	mtx_lock(&ctrlr->lock);
	nslist = ctrlr->changed_ns;

	/* If the first entry is 0xffffffff, the list is already full. */
	if (nslist->ns[0] != 0xffffffff) {
		/* Find the insertion point for this namespace ID. */
		for (i = 0; i < nitems(nslist->ns); i++) {
			nsid = le32toh(nslist->ns[i]);
			if (nsid == new_nsid) {
				/* Already reported, nothing to do. */
				mtx_unlock(&ctrlr->lock);
				return;
			}

			if (nsid == 0 || nsid > new_nsid)
				break;
		}

		if (nslist->ns[nitems(nslist->ns) - 1] != htole32(0)) {
			/* List is full. */
			memset(ctrlr->changed_ns, 0,
			    sizeof(*ctrlr->changed_ns));
			ctrlr->changed_ns->ns[0] = 0xffffffff;
		} else if (nslist->ns[i] == htole32(0)) {
			/*
			 * Optimize case where this ID is appended to
			 * the end.
			 */
			nslist->ns[i] = htole32(new_nsid);
		} else {
			memmove(&nslist->ns[i + 1], &nslist->ns[i],
			    (nitems(nslist->ns) - i - 1) *
			    sizeof(nslist->ns[0]));
			nslist->ns[i] = htole32(new_nsid);
		}
	}

	if (ctrlr->changed_ns_reported) {
		mtx_unlock(&ctrlr->lock);
		return;
	}
	ctrlr->changed_ns_reported = true;
	mtx_unlock(&ctrlr->lock);

	nvmft_report_aer(ctrlr, NVME_ASYNC_EVENT_NS_ATTRIBUTE, 0x2, 0x0,
	    NVME_LOG_CHANGED_NAMESPACE);
}
