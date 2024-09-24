/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/dnv.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/bus_dma.h>

#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/controller/nvmft_subr.h>
#include <dev/nvmf/controller/nvmft_var.h>

#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_error.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_private.h>

/*
 * Store pointers to the capsule and qpair in the two pointer members
 * of CTL_PRIV_FRONTEND.
 */
#define	NVMFT_NC(io)	((io)->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptrs[0])
#define	NVMFT_QP(io)	((io)->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptrs[1])

static void	nvmft_done(union ctl_io *io);
static int	nvmft_init(void);
static int	nvmft_ioctl(struct cdev *cdev, u_long cmd, caddr_t data,
    int flag, struct thread *td);
static int	nvmft_shutdown(void);

extern struct ctl_softc *control_softc;

static struct taskqueue *nvmft_taskq;
static TAILQ_HEAD(, nvmft_port) nvmft_ports;
static struct sx nvmft_ports_lock;

MALLOC_DEFINE(M_NVMFT, "nvmft", "NVMe over Fabrics controller");

static struct ctl_frontend nvmft_frontend = {
	.name = "nvmf",
	.init = nvmft_init,
	.ioctl = nvmft_ioctl,
	.fe_dump = NULL,
	.shutdown = nvmft_shutdown,
};

static void
nvmft_online(void *arg)
{
	struct nvmft_port *np = arg;

	sx_xlock(&np->lock);
	np->online = true;
	sx_xunlock(&np->lock);
}

static void
nvmft_offline(void *arg)
{
	struct nvmft_port *np = arg;
	struct nvmft_controller *ctrlr;

	sx_xlock(&np->lock);
	np->online = false;

	TAILQ_FOREACH(ctrlr, &np->controllers, link) {
		nvmft_printf(ctrlr,
		    "shutting down due to port going offline\n");
		nvmft_controller_error(ctrlr, NULL, ENODEV);
	}

	while (!TAILQ_EMPTY(&np->controllers))
		sx_sleep(np, &np->lock, 0, "nvmfoff", 0);
	sx_xunlock(&np->lock);
}

static int
nvmft_lun_enable(void *arg, int lun_id)
{
	struct nvmft_port *np = arg;
	struct nvmft_controller *ctrlr;
	uint32_t *old_ns, *new_ns;
	uint32_t nsid;
	u_int i;

	if (lun_id >= le32toh(np->cdata.nn)) {
		printf("NVMFT: %s lun %d larger than maximum nsid %u\n",
		    np->cdata.subnqn, lun_id, le32toh(np->cdata.nn));
		return (EOPNOTSUPP);
	}
	nsid = lun_id + 1;

	sx_xlock(&np->lock);
	new_ns = mallocarray(np->num_ns + 1, sizeof(*new_ns), M_NVMFT,
	    M_WAITOK);
	for (i = 0; i < np->num_ns; i++) {
		if (np->active_ns[i] < nsid)
			continue;
		if (np->active_ns[i] == nsid) {
			sx_xunlock(&np->lock);
			free(new_ns, M_NVMFT);
			printf("NVMFT: %s duplicate lun %d\n",
			    np->cdata.subnqn, lun_id);
			return (EINVAL);
		}
		break;
	}

	/* Copy over IDs smaller than nsid. */
	memcpy(new_ns, np->active_ns, i * sizeof(*np->active_ns));

	/* Insert nsid. */
	new_ns[i] = nsid;

	/* Copy over IDs greater than nsid. */
	memcpy(new_ns + i + 1, np->active_ns + i, (np->num_ns - i) *
	    sizeof(*np->active_ns));

	np->num_ns++;
	old_ns = np->active_ns;
	np->active_ns = new_ns;

	TAILQ_FOREACH(ctrlr, &np->controllers, link) {
		nvmft_controller_lun_changed(ctrlr, lun_id);
	}

	sx_xunlock(&np->lock);
	free(old_ns, M_NVMFT);

	return (0);
}

static int
nvmft_lun_disable(void *arg, int lun_id)
{
	struct nvmft_port *np = arg;
	struct nvmft_controller *ctrlr;
	uint32_t nsid;
	u_int i;

	if (lun_id >= le32toh(np->cdata.nn))
		return (0);
	nsid = lun_id + 1;

	sx_xlock(&np->lock);
	for (i = 0; i < np->num_ns; i++) {
		if (np->active_ns[i] == nsid)
			goto found;
	}
	sx_xunlock(&np->lock);
	printf("NVMFT: %s request to disable nonexistent lun %d\n",
	    np->cdata.subnqn, lun_id);
	return (EINVAL);

found:
	/* Move down IDs greater than nsid. */
	memmove(np->active_ns + i, np->active_ns + i + 1,
	    (np->num_ns - (i + 1)) * sizeof(*np->active_ns));
	np->num_ns--;

	/* NB: Don't bother freeing the old active_ns array. */

	TAILQ_FOREACH(ctrlr, &np->controllers, link) {
		nvmft_controller_lun_changed(ctrlr, lun_id);
	}

	sx_xunlock(&np->lock);

	return (0);
}

void
nvmft_populate_active_nslist(struct nvmft_port *np, uint32_t nsid,
    struct nvme_ns_list *nslist)
{
	u_int i, count;

	sx_slock(&np->lock);
	count = 0;
	for (i = 0; i < np->num_ns; i++) {
		if (np->active_ns[i] <= nsid)
			continue;
		nslist->ns[count] = htole32(np->active_ns[i]);
		count++;
		if (count == nitems(nslist->ns))
			break;
	}
	sx_sunlock(&np->lock);
}

void
nvmft_dispatch_command(struct nvmft_qpair *qp, struct nvmf_capsule *nc,
    bool admin)
{
	struct nvmft_controller *ctrlr = nvmft_qpair_ctrlr(qp);
	const struct nvme_command *cmd = nvmf_capsule_sqe(nc);
	struct nvmft_port *np = ctrlr->np;
	union ctl_io *io;
	int error;

	if (cmd->nsid == htole32(0)) {
		nvmft_send_generic_error(qp, nc,
		    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
		nvmf_free_capsule(nc);
		return;
	}

	mtx_lock(&ctrlr->lock);
	if (ctrlr->pending_commands == 0)
		ctrlr->start_busy = sbinuptime();
	ctrlr->pending_commands++;
	mtx_unlock(&ctrlr->lock);
	io = ctl_alloc_io(np->port.ctl_pool_ref);
	ctl_zero_io(io);
	NVMFT_NC(io) = nc;
	NVMFT_QP(io) = qp;
	io->io_hdr.io_type = admin ? CTL_IO_NVME_ADMIN : CTL_IO_NVME;
	io->io_hdr.nexus.initid = ctrlr->cntlid;
	io->io_hdr.nexus.targ_port = np->port.targ_port;
	io->io_hdr.nexus.targ_lun = le32toh(cmd->nsid) - 1;
	io->nvmeio.cmd = *cmd;
	error = ctl_run(io);
	if (error != 0) {
		nvmft_printf(ctrlr, "ctl_run failed for command on %s: %d\n",
		    nvmft_qpair_name(qp), error);
		ctl_nvme_set_generic_error(&io->nvmeio,
		    NVME_SC_INTERNAL_DEVICE_ERROR);
		nvmft_done(io);

		nvmft_controller_error(ctrlr, qp, ENXIO);
	}
}

void
nvmft_terminate_commands(struct nvmft_controller *ctrlr)
{
	struct nvmft_port *np = ctrlr->np;
	union ctl_io *io;
	int error;

	mtx_lock(&ctrlr->lock);
	if (ctrlr->pending_commands == 0)
		ctrlr->start_busy = sbinuptime();
	ctrlr->pending_commands++;
	mtx_unlock(&ctrlr->lock);
	io = ctl_alloc_io(np->port.ctl_pool_ref);
	ctl_zero_io(io);
	NVMFT_QP(io) = ctrlr->admin;
	io->io_hdr.io_type = CTL_IO_TASK;
	io->io_hdr.nexus.initid = ctrlr->cntlid;
	io->io_hdr.nexus.targ_port = np->port.targ_port;
	io->io_hdr.nexus.targ_lun = 0;
	io->taskio.tag_type = CTL_TAG_SIMPLE; /* XXX: unused? */
	io->taskio.task_action = CTL_TASK_I_T_NEXUS_RESET;
	error = ctl_run(io);
	if (error != CTL_RETVAL_COMPLETE) {
		nvmft_printf(ctrlr, "failed to terminate tasks: %d\n", error);
#ifdef INVARIANTS
		io->io_hdr.status = CTL_SUCCESS;
#endif
		nvmft_done(io);
	}
}

static void
nvmft_datamove_out_cb(void *arg, size_t xfered, int error)
{
	struct ctl_nvmeio *ctnio = arg;

	if (error != 0) {
		ctl_nvme_set_data_transfer_error(ctnio);
	} else {
		MPASS(xfered == ctnio->kern_data_len);
		ctnio->kern_data_resid -= xfered;
	}

	if (ctnio->kern_sg_entries) {
		free(ctnio->ext_data_ptr, M_NVMFT);
		ctnio->ext_data_ptr = NULL;
	} else
		MPASS(ctnio->ext_data_ptr == NULL);
	ctl_datamove_done((union ctl_io *)ctnio, false);
}

static void
nvmft_datamove_out(struct ctl_nvmeio *ctnio, struct nvmft_qpair *qp,
    struct nvmf_capsule *nc)
{
	struct memdesc mem;
	int error;

	MPASS(ctnio->ext_data_ptr == NULL);
	if (ctnio->kern_sg_entries > 0) {
		struct ctl_sg_entry *sgl;
		struct bus_dma_segment *vlist;

		vlist = mallocarray(ctnio->kern_sg_entries, sizeof(*vlist),
		    M_NVMFT, M_WAITOK);
		ctnio->ext_data_ptr = (void *)vlist;
		sgl = (struct ctl_sg_entry *)ctnio->kern_data_ptr;
		for (u_int i = 0; i < ctnio->kern_sg_entries; i++) {
			vlist[i].ds_addr = (uintptr_t)sgl[i].addr;
			vlist[i].ds_len = sgl[i].len;
		}
		mem = memdesc_vlist(vlist, ctnio->kern_sg_entries);
	} else
		mem = memdesc_vaddr(ctnio->kern_data_ptr, ctnio->kern_data_len);

	error = nvmf_receive_controller_data(nc, ctnio->kern_rel_offset, &mem,
	    ctnio->kern_data_len, nvmft_datamove_out_cb, ctnio);
	if (error == 0)
		return;

	nvmft_printf(nvmft_qpair_ctrlr(qp),
	    "Failed to request capsule data: %d\n", error);
	ctl_nvme_set_data_transfer_error(ctnio);

	if (ctnio->kern_sg_entries) {
		free(ctnio->ext_data_ptr, M_NVMFT);
		ctnio->ext_data_ptr = NULL;
	} else
		MPASS(ctnio->ext_data_ptr == NULL);
	ctl_datamove_done((union ctl_io *)ctnio, true);
}

static struct mbuf *
nvmft_copy_data(struct ctl_nvmeio *ctnio)
{
	struct ctl_sg_entry *sgl;
	struct mbuf *m0, *m;
	uint32_t resid, off, todo;
	int mlen;

	MPASS(ctnio->kern_data_len != 0);

	m0 = m_getm2(NULL, ctnio->kern_data_len, M_WAITOK, MT_DATA, 0);

	if (ctnio->kern_sg_entries == 0) {
		m_copyback(m0, 0, ctnio->kern_data_len, ctnio->kern_data_ptr);
		return (m0);
	}

	resid = ctnio->kern_data_len;
	sgl = (struct ctl_sg_entry *)ctnio->kern_data_ptr;
	off = 0;
	m = m0;
	mlen = M_TRAILINGSPACE(m);
	for (;;) {
		todo = MIN(mlen, sgl->len - off);
		memcpy(mtod(m, char *) + m->m_len, (char *)sgl->addr + off,
		    todo);
		m->m_len += todo;
		resid -= todo;
		if (resid == 0) {
			MPASS(m->m_next == NULL);
			break;
		}

		off += todo;
		if (off == sgl->len) {
			sgl++;
			off = 0;
		}
		mlen -= todo;
		if (mlen == 0) {
			m = m->m_next;
			mlen = M_TRAILINGSPACE(m);
		}
	}

	return (m0);
}

static void
m_free_ref_data(struct mbuf *m)
{
	ctl_ref kern_data_ref = m->m_ext.ext_arg1;

	kern_data_ref(m->m_ext.ext_arg2, -1);
}

static struct mbuf *
m_get_ref_data(struct ctl_nvmeio *ctnio, void *buf, u_int size)
{
	struct mbuf *m;

	m = m_get(M_WAITOK, MT_DATA);
	m_extadd(m, buf, size, m_free_ref_data, ctnio->kern_data_ref,
	    ctnio->kern_data_arg, M_RDONLY, EXT_CTL);
	m->m_len = size;
	ctnio->kern_data_ref(ctnio->kern_data_arg, 1);
	return (m);
}

static struct mbuf *
nvmft_ref_data(struct ctl_nvmeio *ctnio)
{
	struct ctl_sg_entry *sgl;
	struct mbuf *m0, *m;

	MPASS(ctnio->kern_data_len != 0);

	if (ctnio->kern_sg_entries == 0)
		return (m_get_ref_data(ctnio, ctnio->kern_data_ptr,
		    ctnio->kern_data_len));

	sgl = (struct ctl_sg_entry *)ctnio->kern_data_ptr;
	m0 = m_get_ref_data(ctnio, sgl[0].addr, sgl[0].len);
	m = m0;
	for (u_int i = 1; i < ctnio->kern_sg_entries; i++) {
		m->m_next = m_get_ref_data(ctnio, sgl[i].addr, sgl[i].len);
		m = m->m_next;
	}
	return (m0);
}

static void
nvmft_datamove_in(struct ctl_nvmeio *ctnio, struct nvmft_qpair *qp,
    struct nvmf_capsule *nc)
{
	struct mbuf *m;
	u_int status;

	if (ctnio->kern_data_ref != NULL)
		m = nvmft_ref_data(ctnio);
	else
		m = nvmft_copy_data(ctnio);
	status = nvmf_send_controller_data(nc, ctnio->kern_rel_offset, m,
	    ctnio->kern_data_len);
	switch (status) {
	case NVMF_SUCCESS_SENT:
		ctnio->success_sent = true;
		nvmft_command_completed(qp, nc);
		/* FALLTHROUGH */
	case NVMF_MORE:
	case NVME_SC_SUCCESS:
		break;
	default:
		ctl_nvme_set_generic_error(ctnio, status);
		break;
	}
	ctl_datamove_done((union ctl_io *)ctnio, true);
}

void
nvmft_handle_datamove(union ctl_io *io)
{
	struct nvmf_capsule *nc;
	struct nvmft_qpair *qp;

	/* Some CTL commands preemptively set a success status. */
	MPASS(io->io_hdr.status == CTL_STATUS_NONE ||
	    io->io_hdr.status == CTL_SUCCESS);
	MPASS(!io->nvmeio.success_sent);

	nc = NVMFT_NC(io);
	qp = NVMFT_QP(io);

	if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) == CTL_FLAG_DATA_IN)
		nvmft_datamove_in(&io->nvmeio, qp, nc);
	else
		nvmft_datamove_out(&io->nvmeio, qp, nc);
}

void
nvmft_abort_datamove(union ctl_io *io)
{
	io->io_hdr.port_status = 1;
	io->io_hdr.flags |= CTL_FLAG_ABORT;
	ctl_datamove_done(io, true);
}

static void
nvmft_datamove(union ctl_io *io)
{
	struct nvmft_qpair *qp;

	qp = NVMFT_QP(io);
	nvmft_qpair_datamove(qp, io);
}

void
nvmft_enqueue_task(struct task *task)
{
	taskqueue_enqueue(nvmft_taskq, task);
}

void
nvmft_drain_task(struct task *task)
{
	taskqueue_drain(nvmft_taskq, task);
}

static void
hip_add(uint64_t pair[2], uint64_t addend)
{
	uint64_t old, new;

	old = le64toh(pair[0]);
	new = old + addend;
	pair[0] = htole64(new);
	if (new < old)
		pair[1] += htole64(1);
}

static void
nvmft_done(union ctl_io *io)
{
	struct nvmft_controller *ctrlr;
	const struct nvme_command *cmd;
	struct nvmft_qpair *qp;
	struct nvmf_capsule *nc;
	size_t len;

	KASSERT(io->io_hdr.status == CTL_SUCCESS ||
	    io->io_hdr.status == CTL_NVME_ERROR,
	    ("%s: bad status %u", __func__, io->io_hdr.status));

	nc = NVMFT_NC(io);
	qp = NVMFT_QP(io);
	ctrlr = nvmft_qpair_ctrlr(qp);

	if (nc == NULL) {
		/* Completion of nvmft_terminate_commands. */
		goto end;
	}

	cmd = nvmf_capsule_sqe(nc);

	if (io->io_hdr.status == CTL_SUCCESS)
		len = nvmf_capsule_data_len(nc) / 512;
	else
		len = 0;
	switch (cmd->opc) {
	case NVME_OPC_WRITE:
		mtx_lock(&ctrlr->lock);
		hip_add(ctrlr->hip.host_write_commands, 1);
		len += ctrlr->partial_duw;
		if (len > 1000)
			hip_add(ctrlr->hip.data_units_written, len / 1000);
		ctrlr->partial_duw = len % 1000;
		mtx_unlock(&ctrlr->lock);
		break;
	case NVME_OPC_READ:
	case NVME_OPC_COMPARE:
	case NVME_OPC_VERIFY:
		mtx_lock(&ctrlr->lock);
		if (cmd->opc != NVME_OPC_VERIFY)
			hip_add(ctrlr->hip.host_read_commands, 1);
		len += ctrlr->partial_dur;
		if (len > 1000)
			hip_add(ctrlr->hip.data_units_read, len / 1000);
		ctrlr->partial_dur = len % 1000;
		mtx_unlock(&ctrlr->lock);
		break;
	}

	if (io->nvmeio.success_sent) {
		MPASS(io->io_hdr.status == CTL_SUCCESS);
	} else {
		io->nvmeio.cpl.cid = cmd->cid;
		nvmft_send_response(qp, &io->nvmeio.cpl);
	}
	nvmf_free_capsule(nc);
end:
	ctl_free_io(io);
	mtx_lock(&ctrlr->lock);
	ctrlr->pending_commands--;
	if (ctrlr->pending_commands == 0)
		ctrlr->busy_total += sbinuptime() - ctrlr->start_busy;
	mtx_unlock(&ctrlr->lock);
}

static int
nvmft_init(void)
{
	int error;

	nvmft_taskq = taskqueue_create("nvmft", M_WAITOK,
	    taskqueue_thread_enqueue, &nvmft_taskq);
	error = taskqueue_start_threads_in_proc(&nvmft_taskq, mp_ncpus, PWAIT,
	    control_softc->ctl_proc, "nvmft");
	if (error != 0) {
		taskqueue_free(nvmft_taskq);
		return (error);
	}

	TAILQ_INIT(&nvmft_ports);
	sx_init(&nvmft_ports_lock, "nvmft ports");
	return (0);
}

void
nvmft_port_free(struct nvmft_port *np)
{
	KASSERT(TAILQ_EMPTY(&np->controllers),
	    ("%s(%p): active controllers", __func__, np));

	if (np->port.targ_port != -1) {
		if (ctl_port_deregister(&np->port) != 0)
			printf("%s: ctl_port_deregister() failed\n", __func__);
	}

	free(np->active_ns, M_NVMFT);
	clean_unrhdr(np->ids);
	delete_unrhdr(np->ids);
	sx_destroy(&np->lock);
	free(np, M_NVMFT);
}

static struct nvmft_port *
nvmft_port_find(const char *subnqn)
{
	struct nvmft_port *np;

	KASSERT(nvmf_nqn_valid(subnqn), ("%s: invalid nqn", __func__));

	sx_assert(&nvmft_ports_lock, SA_LOCKED);
	TAILQ_FOREACH(np, &nvmft_ports, link) {
		if (strcmp(np->cdata.subnqn, subnqn) == 0)
			break;
	}
	return (np);
}

static struct nvmft_port *
nvmft_port_find_by_id(int port_id)
{
	struct nvmft_port *np;

	sx_assert(&nvmft_ports_lock, SA_LOCKED);
	TAILQ_FOREACH(np, &nvmft_ports, link) {
		if (np->port.targ_port == port_id)
			break;
	}
	return (np);
}

/*
 * Helper function to fetch a number stored as a string in an nv_list.
 * Returns false if the string was not a valid number.
 */
static bool
dnvlist_get_strnum(nvlist_t *nvl, const char *name, u_long default_value,
	u_long *value)
{
	const char *str;
	char *cp;

	str = dnvlist_get_string(nvl, name, NULL);
	if (str == NULL) {
		*value = default_value;
		return (true);
	}
	if (*str == '\0')
		return (false);
	*value = strtoul(str, &cp, 0);
	if (*cp != '\0')
		return (false);
	return (true);
}

/*
 * NVMeoF ports support the following parameters:
 *
 * Mandatory:
 *
 * subnqn: subsystem NVMe Qualified Name
 * portid: integer port ID from Discovery Log Page entry
 *
 * Optional:
 * serial: Serial Number string
 * max_io_qsize: Maximum number of I/O queue entries
 * enable_timeout: Timeout for controller enable in milliseconds
 * ioccsz: Maximum command capsule size
 * iorcsz: Maximum response capsule size
 * nn: Number of namespaces
 */
static void
nvmft_port_create(struct ctl_req *req)
{
	struct nvmft_port *np;
	struct ctl_port *port;
	const char *serial, *subnqn;
	char serial_buf[NVME_SERIAL_NUMBER_LENGTH];
	u_long enable_timeout, hostid, ioccsz, iorcsz, max_io_qsize, nn, portid;
	int error;

	/* Required parameters. */
	subnqn = dnvlist_get_string(req->args_nvl, "subnqn", NULL);
	if (subnqn == NULL || !nvlist_exists_string(req->args_nvl, "portid")) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Missing required argument");
		return;
	}
	if (!nvmf_nqn_valid(subnqn)) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Invalid SubNQN");
		return;
	}
	if (!dnvlist_get_strnum(req->args_nvl, "portid", UINT16_MAX, &portid) ||
	    portid > UINT16_MAX) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Invalid port ID");
		return;
	}

	/* Optional parameters. */
	if (!dnvlist_get_strnum(req->args_nvl, "max_io_qsize",
	    NVMF_MAX_IO_ENTRIES, &max_io_qsize) ||
	    max_io_qsize < NVME_MIN_IO_ENTRIES ||
	    max_io_qsize > NVME_MAX_IO_ENTRIES) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Invalid maximum I/O queue size");
		return;
	}

	if (!dnvlist_get_strnum(req->args_nvl, "enable_timeout",
	    NVMF_CC_EN_TIMEOUT * 500, &enable_timeout) ||
	    (enable_timeout % 500) != 0 || (enable_timeout / 500) > 255) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Invalid enable timeout");
		return;
	}

	if (!dnvlist_get_strnum(req->args_nvl, "ioccsz", NVMF_IOCCSZ,
	    &ioccsz) || ioccsz < sizeof(struct nvme_command) ||
	    (ioccsz % 16) != 0) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Invalid Command Capsule size");
		return;
	}

	if (!dnvlist_get_strnum(req->args_nvl, "iorcsz", NVMF_IORCSZ,
	    &iorcsz) || iorcsz < sizeof(struct nvme_completion) ||
	    (iorcsz % 16) != 0) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Invalid Response Capsule size");
		return;
	}

	if (!dnvlist_get_strnum(req->args_nvl, "nn", NVMF_NN, &nn) ||
	    nn < 1 || nn > UINT32_MAX) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Invalid number of namespaces");
		return;
	}

	serial = dnvlist_get_string(req->args_nvl, "serial", NULL);
	if (serial == NULL) {
		getcredhostid(curthread->td_ucred, &hostid);
		nvmf_controller_serial(serial_buf, sizeof(serial_buf), hostid);
		serial = serial_buf;
	}

	sx_xlock(&nvmft_ports_lock);

	np = nvmft_port_find(subnqn);
	if (np != NULL) {
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "SubNQN \"%s\" already exists", subnqn);
		sx_xunlock(&nvmft_ports_lock);
		return;
	}

	np = malloc(sizeof(*np), M_NVMFT, M_WAITOK | M_ZERO);
	refcount_init(&np->refs, 1);
	np->max_io_qsize = max_io_qsize;
	np->cap = _nvmf_controller_cap(max_io_qsize, enable_timeout / 500);
	sx_init(&np->lock, "nvmft port");
	np->ids = new_unrhdr(0, MIN(CTL_MAX_INIT_PER_PORT - 1,
	    NVMF_CNTLID_STATIC_MAX), UNR_NO_MTX);
	TAILQ_INIT(&np->controllers);

	/* The controller ID is set later for individual controllers. */
	_nvmf_init_io_controller_data(0, max_io_qsize, serial, ostype,
	    osrelease, subnqn, nn, ioccsz, iorcsz, &np->cdata);
	np->cdata.aerl = NVMFT_NUM_AER - 1;
	np->cdata.oaes = htole32(NVME_ASYNC_EVENT_NS_ATTRIBUTE);
	np->cdata.oncs = htole16(NVMEF(NVME_CTRLR_DATA_ONCS_VERIFY, 1) |
	    NVMEF(NVME_CTRLR_DATA_ONCS_WRZERO, 1) |
	    NVMEF(NVME_CTRLR_DATA_ONCS_DSM, 1) |
	    NVMEF(NVME_CTRLR_DATA_ONCS_COMPARE, 1));
	np->cdata.fuses = NVMEF(NVME_CTRLR_DATA_FUSES_CNW, 1);

	np->fp.afi = NVMEF(NVME_FIRMWARE_PAGE_AFI_SLOT, 1);
	memcpy(np->fp.revision[0], np->cdata.fr, sizeof(np->cdata.fr));

	port = &np->port;

	port->frontend = &nvmft_frontend;
	port->port_type = CTL_PORT_NVMF;
	port->num_requested_ctl_io = max_io_qsize;
	port->port_name = "nvmf";
	port->physical_port = portid;
	port->virtual_port = 0;
	port->port_online = nvmft_online;
	port->port_offline = nvmft_offline;
	port->onoff_arg = np;
	port->lun_enable = nvmft_lun_enable;
	port->lun_disable = nvmft_lun_disable;
	port->targ_lun_arg = np;
	port->fe_datamove = nvmft_datamove;
	port->fe_done = nvmft_done;
	port->targ_port = -1;
	port->options = nvlist_clone(req->args_nvl);

	error = ctl_port_register(port);
	if (error != 0) {
		sx_xunlock(&nvmft_ports_lock);
		nvlist_destroy(port->options);
		nvmft_port_rele(np);
		req->status = CTL_LUN_ERROR;
		snprintf(req->error_str, sizeof(req->error_str),
		    "Failed to register CTL port with error %d", error);
		return;
	}

	TAILQ_INSERT_TAIL(&nvmft_ports, np, link);
	sx_xunlock(&nvmft_ports_lock);

	req->status = CTL_LUN_OK;
	req->result_nvl = nvlist_create(0);
	nvlist_add_number(req->result_nvl, "port_id", port->targ_port);
}

static void
nvmft_port_remove(struct ctl_req *req)
{
	struct nvmft_port *np;
	const char *subnqn;
	u_long port_id;

	/*
	 * ctladm port -r just provides the port_id, so permit looking
	 * up a port either by "subnqn" or "port_id".
	 */
	port_id = ULONG_MAX;
	subnqn = dnvlist_get_string(req->args_nvl, "subnqn", NULL);
	if (subnqn == NULL) {
		if (!nvlist_exists_string(req->args_nvl, "port_id")) {
			req->status = CTL_LUN_ERROR;
			snprintf(req->error_str, sizeof(req->error_str),
			    "Missing required argument");
			return;
		}
		if (!dnvlist_get_strnum(req->args_nvl, "port_id", ULONG_MAX,
		    &port_id)) {
			req->status = CTL_LUN_ERROR;
			snprintf(req->error_str, sizeof(req->error_str),
			    "Invalid CTL port ID");
			return;
		}
	} else {
		if (nvlist_exists_string(req->args_nvl, "port_id")) {
			req->status = CTL_LUN_ERROR;
			snprintf(req->error_str, sizeof(req->error_str),
			    "Ambiguous port removal request");
			return;
		}
	}

	sx_xlock(&nvmft_ports_lock);

	if (subnqn != NULL) {
		np = nvmft_port_find(subnqn);
		if (np == NULL) {
			req->status = CTL_LUN_ERROR;
			snprintf(req->error_str, sizeof(req->error_str),
			    "SubNQN \"%s\" does not exist", subnqn);
			sx_xunlock(&nvmft_ports_lock);
			return;
		}
	} else {
		np = nvmft_port_find_by_id(port_id);
		if (np == NULL) {
			req->status = CTL_LUN_ERROR;
			snprintf(req->error_str, sizeof(req->error_str),
			    "CTL port %lu is not a NVMF port", port_id);
			sx_xunlock(&nvmft_ports_lock);
			return;
		}
	}

	TAILQ_REMOVE(&nvmft_ports, np, link);
	sx_xunlock(&nvmft_ports_lock);

	ctl_port_offline(&np->port);
	nvmft_port_rele(np);
	req->status = CTL_LUN_OK;
}

static void
nvmft_handoff(struct ctl_nvmf *cn)
{
	struct nvmf_fabric_connect_cmd cmd;
	struct nvmf_handoff_controller_qpair *handoff;
	struct nvmf_fabric_connect_data *data;
	struct nvmft_port *np;
	int error;

	np = NULL;
	data = NULL;
	handoff = &cn->data.handoff;
	error = copyin(handoff->cmd, &cmd, sizeof(cmd));
	if (error != 0) {
		cn->status = CTL_NVMF_ERROR;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "Failed to copyin CONNECT SQE");
		return;
	}

	data = malloc(sizeof(*data), M_NVMFT, M_WAITOK);
	error = copyin(handoff->data, data, sizeof(*data));
	if (error != 0) {
		cn->status = CTL_NVMF_ERROR;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "Failed to copyin CONNECT data");
		goto out;
	}

	if (!nvmf_nqn_valid(data->subnqn)) {
		cn->status = CTL_NVMF_ERROR;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "Invalid SubNQN");
		goto out;
	}

	sx_slock(&nvmft_ports_lock);
	np = nvmft_port_find(data->subnqn);
	if (np == NULL) {
		sx_sunlock(&nvmft_ports_lock);
		cn->status = CTL_NVMF_ERROR;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "Unknown SubNQN");
		goto out;
	}
	if (!np->online) {
		sx_sunlock(&nvmft_ports_lock);
		cn->status = CTL_NVMF_ERROR;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "CTL port offline");
		np = NULL;
		goto out;
	}
	nvmft_port_ref(np);
	sx_sunlock(&nvmft_ports_lock);

	if (handoff->params.admin) {
		error = nvmft_handoff_admin_queue(np, handoff, &cmd, data);
		if (error != 0) {
			cn->status = CTL_NVMF_ERROR;
			snprintf(cn->error_str, sizeof(cn->error_str),
			    "Failed to handoff admin queue: %d", error);
			goto out;
		}
	} else {
		error = nvmft_handoff_io_queue(np, handoff, &cmd, data);
		if (error != 0) {
			cn->status = CTL_NVMF_ERROR;
			snprintf(cn->error_str, sizeof(cn->error_str),
			    "Failed to handoff admin queue: %d", error);
			goto out;
		}
	}

	cn->status = CTL_NVMF_OK;
out:
	if (np != NULL)
		nvmft_port_rele(np);
	free(data, M_NVMFT);
}

static void
nvmft_list(struct ctl_nvmf *cn)
{
	struct ctl_nvmf_list_params *lp;
	struct nvmft_controller *ctrlr;
	struct nvmft_port *np;
	struct sbuf *sb;
	int error;

	lp = &cn->data.list;

	sb = sbuf_new(NULL, NULL, lp->alloc_len, SBUF_FIXEDLEN |
	    SBUF_INCLUDENUL);
	if (sb == NULL) {
		cn->status = CTL_NVMF_ERROR;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "Failed to allocate NVMeoF session list");
		return;
	}

	sbuf_printf(sb, "<ctlnvmflist>\n");
	sx_slock(&nvmft_ports_lock);
	TAILQ_FOREACH(np, &nvmft_ports, link) {
		sx_slock(&np->lock);
		TAILQ_FOREACH(ctrlr, &np->controllers, link) {
			sbuf_printf(sb, "<connection id=\"%d\">"
			    "<hostnqn>%s</hostnqn>"
			    "<subnqn>%s</subnqn>"
			    "<trtype>%u</trtype>"
			    "</connection>\n",
			    ctrlr->cntlid,
			    ctrlr->hostnqn,
			    np->cdata.subnqn,
			    ctrlr->trtype);
		}
		sx_sunlock(&np->lock);
	}
	sx_sunlock(&nvmft_ports_lock);
	sbuf_printf(sb, "</ctlnvmflist>\n");
	if (sbuf_finish(sb) != 0) {
		sbuf_delete(sb);
		cn->status = CTL_NVMF_LIST_NEED_MORE_SPACE;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "Out of space, %d bytes is too small", lp->alloc_len);
		return;
	}

	error = copyout(sbuf_data(sb), lp->conn_xml, sbuf_len(sb));
	if (error != 0) {
		sbuf_delete(sb);
		cn->status = CTL_NVMF_ERROR;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "Failed to copyout session list: %d", error);
		return;
	}
	lp->fill_len = sbuf_len(sb);
	cn->status = CTL_NVMF_OK;
	sbuf_delete(sb);
}

static void
nvmft_terminate(struct ctl_nvmf *cn)
{
	struct ctl_nvmf_terminate_params *tp;
	struct nvmft_controller *ctrlr;
	struct nvmft_port *np;
	bool found, match;

	tp = &cn->data.terminate;

	found = false;
	sx_slock(&nvmft_ports_lock);
	TAILQ_FOREACH(np, &nvmft_ports, link) {
		sx_slock(&np->lock);
		TAILQ_FOREACH(ctrlr, &np->controllers, link) {
			if (tp->all != 0)
				match = true;
			else if (tp->cntlid != -1)
				match = tp->cntlid == ctrlr->cntlid;
			else if (tp->hostnqn[0] != '\0')
				match = strncmp(tp->hostnqn, ctrlr->hostnqn,
				    sizeof(tp->hostnqn)) == 0;
			else
				match = false;
			if (!match)
				continue;
			nvmft_printf(ctrlr,
			    "disconnecting due to administrative request\n");
			nvmft_controller_error(ctrlr, NULL, ECONNABORTED);
			found = true;
		}
		sx_sunlock(&np->lock);
	}
	sx_sunlock(&nvmft_ports_lock);

	if (!found) {
		cn->status = CTL_NVMF_ASSOCIATION_NOT_FOUND;
		snprintf(cn->error_str, sizeof(cn->error_str),
		    "No matching associations found");
		return;
	}
	cn->status = CTL_NVMF_OK;
}

static int
nvmft_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int flag,
    struct thread *td)
{
	struct ctl_nvmf *cn;
	struct ctl_req *req;

	switch (cmd) {
	case CTL_PORT_REQ:
		req = (struct ctl_req *)data;
		switch (req->reqtype) {
		case CTL_REQ_CREATE:
			nvmft_port_create(req);
			break;
		case CTL_REQ_REMOVE:
			nvmft_port_remove(req);
			break;
		default:
			req->status = CTL_LUN_ERROR;
			snprintf(req->error_str, sizeof(req->error_str),
			    "Unsupported request type %d", req->reqtype);
			break;
		}
		return (0);
	case CTL_NVMF:
		cn = (struct ctl_nvmf *)data;
		switch (cn->type) {
		case CTL_NVMF_HANDOFF:
			nvmft_handoff(cn);
			break;
		case CTL_NVMF_LIST:
			nvmft_list(cn);
			break;
		case CTL_NVMF_TERMINATE:
			nvmft_terminate(cn);
			break;
		default:
			cn->status = CTL_NVMF_ERROR;
			snprintf(cn->error_str, sizeof(cn->error_str),
			    "Invalid NVMeoF request type %d", cn->type);
			break;
		}
		return (0);
	default:
		return (ENOTTY);
	}
}

static int
nvmft_shutdown(void)
{
	/* TODO: Need to check for active controllers. */
	if (!TAILQ_EMPTY(&nvmft_ports))
		return (EBUSY);

	taskqueue_free(nvmft_taskq);
	sx_destroy(&nvmft_ports_lock);
	return (0);
}

CTL_FRONTEND_DECLARE(nvmft, nvmft_frontend);
MODULE_DEPEND(nvmft, nvmf_transport, 1, 1, 1);
