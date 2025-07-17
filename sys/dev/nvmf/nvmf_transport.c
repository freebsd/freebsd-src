/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/nv.h>
#include <sys/refcount.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/nvmf.h>
#include <dev/nvmf/nvmf_transport.h>
#include <dev/nvmf/nvmf_transport_internal.h>

/* Transport-independent support for fabrics queue pairs and commands. */

struct nvmf_transport {
	struct nvmf_transport_ops *nt_ops;

	volatile u_int nt_active_qpairs;
	SLIST_ENTRY(nvmf_transport) nt_link;
};

/* nvmf_transports[nvmf_trtype] is sorted by priority */
static SLIST_HEAD(, nvmf_transport) nvmf_transports[NVMF_TRTYPE_TCP + 1];
static struct sx nvmf_transports_lock;

static MALLOC_DEFINE(M_NVMF_TRANSPORT, "nvmf_xport",
    "NVMe over Fabrics transport");

SYSCTL_NODE(_kern, OID_AUTO, nvmf, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "NVMe over Fabrics");

static bool
nvmf_supported_trtype(enum nvmf_trtype trtype)
{
	return (trtype < nitems(nvmf_transports));
}

struct nvmf_qpair *
nvmf_allocate_qpair(enum nvmf_trtype trtype, bool controller,
    const nvlist_t *params, nvmf_qpair_error_t *error_cb, void *error_cb_arg,
    nvmf_capsule_receive_t *receive_cb, void *receive_cb_arg)
{
	struct nvmf_transport *nt;
	struct nvmf_qpair *qp;

	if (!nvmf_supported_trtype(trtype))
		return (NULL);

	sx_slock(&nvmf_transports_lock);
	SLIST_FOREACH(nt, &nvmf_transports[trtype], nt_link) {
		qp = nt->nt_ops->allocate_qpair(controller, params);
		if (qp != NULL) {
			refcount_acquire(&nt->nt_active_qpairs);
			break;
		}
	}
	sx_sunlock(&nvmf_transports_lock);
	if (qp == NULL)
		return (NULL);

	qp->nq_transport = nt;
	qp->nq_ops = nt->nt_ops;
	qp->nq_controller = controller;
	qp->nq_error = error_cb;
	qp->nq_error_arg = error_cb_arg;
	qp->nq_receive = receive_cb;
	qp->nq_receive_arg = receive_cb_arg;
	qp->nq_admin = nvlist_get_bool(params, "admin");
	return (qp);
}

void
nvmf_free_qpair(struct nvmf_qpair *qp)
{
	struct nvmf_transport *nt;

	nt = qp->nq_transport;
	qp->nq_ops->free_qpair(qp);
	if (refcount_release(&nt->nt_active_qpairs))
		wakeup(nt);
}

struct nvmf_capsule *
nvmf_allocate_command(struct nvmf_qpair *qp, const void *sqe, int how)
{
	struct nvmf_capsule *nc;

	KASSERT(how == M_WAITOK || how == M_NOWAIT,
	    ("%s: invalid how", __func__));
	nc = qp->nq_ops->allocate_capsule(qp, how);
	if (nc == NULL)
		return (NULL);

	nc->nc_qpair = qp;
	nc->nc_qe_len = sizeof(struct nvme_command);
	memcpy(&nc->nc_sqe, sqe, nc->nc_qe_len);

	/* 4.2 of NVMe base spec: Fabrics always uses SGL. */
	nc->nc_sqe.fuse &= ~NVMEM(NVME_CMD_PSDT);
	nc->nc_sqe.fuse |= NVMEF(NVME_CMD_PSDT, NVME_PSDT_SGL);
	return (nc);
}

struct nvmf_capsule *
nvmf_allocate_response(struct nvmf_qpair *qp, const void *cqe, int how)
{
	struct nvmf_capsule *nc;

	KASSERT(how == M_WAITOK || how == M_NOWAIT,
	    ("%s: invalid how", __func__));
	nc = qp->nq_ops->allocate_capsule(qp, how);
	if (nc == NULL)
		return (NULL);

	nc->nc_qpair = qp;
	nc->nc_qe_len = sizeof(struct nvme_completion);
	memcpy(&nc->nc_cqe, cqe, nc->nc_qe_len);
	return (nc);
}

int
nvmf_capsule_append_data(struct nvmf_capsule *nc, struct memdesc *mem,
    size_t len, bool send, nvmf_io_complete_t *complete_cb,
    void *cb_arg)
{
	if (nc->nc_data.io_len != 0)
		return (EBUSY);

	nc->nc_send_data = send;
	nc->nc_data.io_mem = *mem;
	nc->nc_data.io_len = len;
	nc->nc_data.io_complete = complete_cb;
	nc->nc_data.io_complete_arg = cb_arg;
	return (0);
}

void
nvmf_free_capsule(struct nvmf_capsule *nc)
{
	nc->nc_qpair->nq_ops->free_capsule(nc);
}

int
nvmf_transmit_capsule(struct nvmf_capsule *nc)
{
	return (nc->nc_qpair->nq_ops->transmit_capsule(nc));
}

void
nvmf_abort_capsule_data(struct nvmf_capsule *nc, int error)
{
	if (nc->nc_data.io_len != 0)
		nvmf_complete_io_request(&nc->nc_data, 0, error);
}

void *
nvmf_capsule_sqe(struct nvmf_capsule *nc)
{
	KASSERT(nc->nc_qe_len == sizeof(struct nvme_command),
	    ("%s: capsule %p is not a command capsule", __func__, nc));
	return (&nc->nc_sqe);
}

void *
nvmf_capsule_cqe(struct nvmf_capsule *nc)
{
	KASSERT(nc->nc_qe_len == sizeof(struct nvme_completion),
	    ("%s: capsule %p is not a response capsule", __func__, nc));
	return (&nc->nc_cqe);
}

bool
nvmf_sqhd_valid(struct nvmf_capsule *nc)
{
	KASSERT(nc->nc_qe_len == sizeof(struct nvme_completion),
	    ("%s: capsule %p is not a response capsule", __func__, nc));
	return (nc->nc_sqhd_valid);
}

uint8_t
nvmf_validate_command_capsule(struct nvmf_capsule *nc)
{
	KASSERT(nc->nc_qe_len == sizeof(struct nvme_command),
	    ("%s: capsule %p is not a command capsule", __func__, nc));

	if (NVMEV(NVME_CMD_PSDT, nc->nc_sqe.fuse) != NVME_PSDT_SGL)
		return (NVME_SC_INVALID_FIELD);

	return (nc->nc_qpair->nq_ops->validate_command_capsule(nc));
}

size_t
nvmf_capsule_data_len(const struct nvmf_capsule *nc)
{
	return (nc->nc_qpair->nq_ops->capsule_data_len(nc));
}

int
nvmf_receive_controller_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct memdesc *mem, size_t len, nvmf_io_complete_t *complete_cb,
    void *cb_arg)
{
	struct nvmf_io_request io;

	io.io_mem = *mem;
	io.io_len = len;
	io.io_complete = complete_cb;
	io.io_complete_arg = cb_arg;
	return (nc->nc_qpair->nq_ops->receive_controller_data(nc, data_offset,
	    &io));
}

u_int
nvmf_send_controller_data(struct nvmf_capsule *nc, uint32_t data_offset,
    struct mbuf *m, size_t len)
{
	MPASS(m_length(m, NULL) == len);
	return (nc->nc_qpair->nq_ops->send_controller_data(nc, data_offset, m,
	    len));
}

int
nvmf_pack_ioc_nvlist(const nvlist_t *nvl, struct nvmf_ioc_nv *nv)
{
	void *packed;
	int error;

	error = nvlist_error(nvl);
	if (error != 0)
		return (error);

	if (nv->size == 0) {
		nv->len = nvlist_size(nvl);
	} else {
		packed = nvlist_pack(nvl, &nv->len);
		if (packed == NULL)
			error = ENOMEM;
		else if (nv->len > nv->size)
			error = EFBIG;
		else
			error = copyout(packed, nv->data, nv->len);
		free(packed, M_NVLIST);
	}
	return (error);
}

int
nvmf_unpack_ioc_nvlist(const struct nvmf_ioc_nv *nv, nvlist_t **nvlp)
{
	void *packed;
	nvlist_t *nvl;
	int error;

	packed = malloc(nv->size, M_NVMF_TRANSPORT, M_WAITOK);
	error = copyin(nv->data, packed, nv->size);
	if (error != 0) {
		free(packed, M_NVMF_TRANSPORT);
		return (error);
	}

	nvl = nvlist_unpack(packed, nv->size, 0);
	free(packed, M_NVMF_TRANSPORT);
	if (nvl == NULL)
		return (EINVAL);

	*nvlp = nvl;
	return (0);
}

bool
nvmf_validate_qpair_nvlist(const nvlist_t *nvl, bool controller)
{
	uint64_t value, qsize;
	bool admin, valid;

	valid = true;
	valid &= nvlist_exists_bool(nvl, "admin");
	valid &= nvlist_exists_bool(nvl, "sq_flow_control");
	valid &= nvlist_exists_number(nvl, "qsize");
	valid &= nvlist_exists_number(nvl, "sqhd");
	if (!controller)
		valid &= nvlist_exists_number(nvl, "sqtail");
	if (!valid)
		return (false);

	admin = nvlist_get_bool(nvl, "admin");
	qsize = nvlist_get_number(nvl, "qsize");
	if (admin) {
		if (qsize < NVME_MIN_ADMIN_ENTRIES ||
		    qsize > NVME_MAX_ADMIN_ENTRIES)
			return (false);
	} else {
		if (qsize < NVME_MIN_IO_ENTRIES || qsize > NVME_MAX_IO_ENTRIES)
			return (false);
	}
	value = nvlist_get_number(nvl, "sqhd");
	if (value > qsize - 1)
		return (false);
	if (!controller) {
		value = nvlist_get_number(nvl, "sqtail");
		if (value > qsize - 1)
			return (false);
	}

	return (true);
}

int
nvmf_transport_module_handler(struct module *mod, int what, void *arg)
{
	struct nvmf_transport_ops *ops = arg;
	struct nvmf_transport *nt, *nt2, *prev;
	int error;

	switch (what) {
	case MOD_LOAD:
		if (!nvmf_supported_trtype(ops->trtype)) {
			printf("NVMF: Unsupported transport %u", ops->trtype);
			return (EINVAL);
		}

		nt = malloc(sizeof(*nt), M_NVMF_TRANSPORT, M_WAITOK | M_ZERO);
		nt->nt_ops = arg;

		sx_xlock(&nvmf_transports_lock);
		if (SLIST_EMPTY(&nvmf_transports[ops->trtype])) {
			SLIST_INSERT_HEAD(&nvmf_transports[ops->trtype], nt,
			    nt_link);
		} else {
			prev = NULL;
			SLIST_FOREACH(nt2, &nvmf_transports[ops->trtype],
			    nt_link) {
				if (ops->priority > nt2->nt_ops->priority)
					break;
				prev = nt2;
			}
			if (prev == NULL)
				SLIST_INSERT_HEAD(&nvmf_transports[ops->trtype],
				    nt, nt_link);
			else
				SLIST_INSERT_AFTER(prev, nt, nt_link);
		}
		sx_xunlock(&nvmf_transports_lock);
		return (0);

	case MOD_QUIESCE:
		if (!nvmf_supported_trtype(ops->trtype))
			return (0);

		sx_slock(&nvmf_transports_lock);
		SLIST_FOREACH(nt, &nvmf_transports[ops->trtype], nt_link) {
			if (nt->nt_ops == ops)
				break;
		}
		if (nt == NULL) {
			sx_sunlock(&nvmf_transports_lock);
			return (0);
		}
		if (nt->nt_active_qpairs != 0) {
			sx_sunlock(&nvmf_transports_lock);
			return (EBUSY);
		}
		sx_sunlock(&nvmf_transports_lock);
		return (0);

	case MOD_UNLOAD:
		if (!nvmf_supported_trtype(ops->trtype))
			return (0);

		sx_xlock(&nvmf_transports_lock);
		prev = NULL;
		SLIST_FOREACH(nt, &nvmf_transports[ops->trtype], nt_link) {
			if (nt->nt_ops == ops)
				break;
			prev = nt;
		}
		if (nt == NULL) {
			sx_xunlock(&nvmf_transports_lock);
			return (0);
		}

		if (prev == NULL)
			SLIST_REMOVE_HEAD(&nvmf_transports[ops->trtype],
			    nt_link);
		else
			SLIST_REMOVE_AFTER(prev, nt_link);

		error = 0;
		while (nt->nt_active_qpairs != 0 && error == 0)
			error = sx_sleep(nt, &nvmf_transports_lock, PCATCH,
			    "nftunld", 0);
		sx_xunlock(&nvmf_transports_lock);
		if (error != 0)
			return (error);
		free(nt, M_NVMF_TRANSPORT);
		return (0);

	default:
		return (EOPNOTSUPP);
	}
}

static int
nvmf_transport_modevent(module_t mod __unused, int what, void *arg __unused)
{
	switch (what) {
	case MOD_LOAD:
		for (u_int i = 0; i < nitems(nvmf_transports); i++)
			SLIST_INIT(&nvmf_transports[i]);
		sx_init(&nvmf_transports_lock, "nvmf transports");
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t nvmf_transport_mod = {
	"nvmf_transport",
	nvmf_transport_modevent,
	0
};

DECLARE_MODULE(nvmf_transport, nvmf_transport_mod, SI_SUB_DRIVERS,
    SI_ORDER_FIRST);
MODULE_VERSION(nvmf_transport, 1);
