/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/refcount.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libnvmf.h"
#include "internal.h"

struct nvmf_association *
nvmf_allocate_association(enum nvmf_trtype trtype, bool controller,
    const struct nvmf_association_params *params)
{
	struct nvmf_transport_ops *ops;
	struct nvmf_association *na;

	switch (trtype) {
	case NVMF_TRTYPE_TCP:
		ops = &tcp_ops;
		break;
	default:
		errno = EINVAL;
		return (NULL);
	}

	na = ops->allocate_association(controller, params);
	if (na == NULL)
		return (NULL);

	na->na_ops = ops;
	na->na_trtype = trtype;
	na->na_controller = controller;
	na->na_params = *params;
	na->na_last_error = NULL;
	refcount_init(&na->na_refs, 1);
	return (na);
}

void
nvmf_update_assocation(struct nvmf_association *na,
    const struct nvme_controller_data *cdata)
{
	na->na_ops->update_association(na, cdata);
}

void
nvmf_free_association(struct nvmf_association *na)
{
	if (refcount_release(&na->na_refs)) {
		free(na->na_last_error);
		na->na_ops->free_association(na);
	}
}

const char *
nvmf_association_error(const struct nvmf_association *na)
{
	return (na->na_last_error);
}

void
na_clear_error(struct nvmf_association *na)
{
	free(na->na_last_error);
	na->na_last_error = NULL;
}

void
na_error(struct nvmf_association *na, const char *fmt, ...)
{
	va_list ap;
	char *str;

	if (na->na_last_error != NULL)
		return;
	va_start(ap, fmt);
	vasprintf(&str, fmt, ap);
	va_end(ap);
	na->na_last_error = str;
}

struct nvmf_qpair *
nvmf_allocate_qpair(struct nvmf_association *na,
    const struct nvmf_qpair_params *params)
{
	struct nvmf_qpair *qp;

	na_clear_error(na);
	qp = na->na_ops->allocate_qpair(na, params);
	if (qp == NULL)
		return (NULL);

	refcount_acquire(&na->na_refs);
	qp->nq_association = na;
	qp->nq_admin = params->admin;
	TAILQ_INIT(&qp->nq_rx_capsules);
	return (qp);
}

void
nvmf_free_qpair(struct nvmf_qpair *qp)
{
	struct nvmf_association *na;
	struct nvmf_capsule *nc, *tc;

	TAILQ_FOREACH_SAFE(nc, &qp->nq_rx_capsules, nc_link, tc) {
		TAILQ_REMOVE(&qp->nq_rx_capsules, nc, nc_link);
		nvmf_free_capsule(nc);
	}
	na = qp->nq_association;
	na->na_ops->free_qpair(qp);
	nvmf_free_association(na);
}

struct nvmf_capsule *
nvmf_allocate_command(struct nvmf_qpair *qp, const void *sqe)
{
	struct nvmf_capsule *nc;

	nc = qp->nq_association->na_ops->allocate_capsule(qp);
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
nvmf_allocate_response(struct nvmf_qpair *qp, const void *cqe)
{
	struct nvmf_capsule *nc;

	nc = qp->nq_association->na_ops->allocate_capsule(qp);
	if (nc == NULL)
		return (NULL);

	nc->nc_qpair = qp;
	nc->nc_qe_len = sizeof(struct nvme_completion);
	memcpy(&nc->nc_cqe, cqe, nc->nc_qe_len);
	return (nc);
}

int
nvmf_capsule_append_data(struct nvmf_capsule *nc, void *buf, size_t len,
    bool send)
{
	if (nc->nc_qe_len == sizeof(struct nvme_completion))
		return (EINVAL);
	if (nc->nc_data_len != 0)
		return (EBUSY);

	nc->nc_data = buf;
	nc->nc_data_len = len;
	nc->nc_send_data = send;
	return (0);
}

void
nvmf_free_capsule(struct nvmf_capsule *nc)
{
	nc->nc_qpair->nq_association->na_ops->free_capsule(nc);
}

int
nvmf_transmit_capsule(struct nvmf_capsule *nc)
{
	return (nc->nc_qpair->nq_association->na_ops->transmit_capsule(nc));
}

int
nvmf_receive_capsule(struct nvmf_qpair *qp, struct nvmf_capsule **ncp)
{
	return (qp->nq_association->na_ops->receive_capsule(qp, ncp));
}

const void *
nvmf_capsule_sqe(const struct nvmf_capsule *nc)
{
	assert(nc->nc_qe_len == sizeof(struct nvme_command));
	return (&nc->nc_sqe);
}

const void *
nvmf_capsule_cqe(const struct nvmf_capsule *nc)
{
	assert(nc->nc_qe_len == sizeof(struct nvme_completion));
	return (&nc->nc_cqe);
}

uint8_t
nvmf_validate_command_capsule(const struct nvmf_capsule *nc)
{
	assert(nc->nc_qe_len == sizeof(struct nvme_command));

	if (NVMEV(NVME_CMD_PSDT, nc->nc_sqe.fuse) != NVME_PSDT_SGL)
		return (NVME_SC_INVALID_FIELD);

	return (nc->nc_qpair->nq_association->na_ops->validate_command_capsule(nc));
}

size_t
nvmf_capsule_data_len(const struct nvmf_capsule *nc)
{
	return (nc->nc_qpair->nq_association->na_ops->capsule_data_len(nc));
}

int
nvmf_receive_controller_data(const struct nvmf_capsule *nc,
    uint32_t data_offset, void *buf, size_t len)
{
	return (nc->nc_qpair->nq_association->na_ops->receive_controller_data(nc,
	    data_offset, buf, len));
}

int
nvmf_send_controller_data(const struct nvmf_capsule *nc, const void *buf,
    size_t len)
{
	return (nc->nc_qpair->nq_association->na_ops->send_controller_data(nc,
	    buf, len));
}

int
nvmf_kernel_handoff_params(struct nvmf_qpair *qp, nvlist_t **nvlp)
{
	nvlist_t *nvl;
	int error;

	nvl = nvlist_create(0);
	nvlist_add_bool(nvl, "admin", qp->nq_admin);
	nvlist_add_bool(nvl, "sq_flow_control", qp->nq_flow_control);
	nvlist_add_number(nvl, "qsize", qp->nq_qsize);
	nvlist_add_number(nvl, "sqhd", qp->nq_sqhd);
	if (!qp->nq_association->na_controller)
		nvlist_add_number(nvl, "sqtail", qp->nq_sqtail);
	qp->nq_association->na_ops->kernel_handoff_params(qp, nvl);
	error = nvlist_error(nvl);
	if (error != 0) {
		nvlist_destroy(nvl);
		return (error);
	}

	*nvlp = nvl;
	return (0);
}

int
nvmf_populate_dle(struct nvmf_qpair *qp, struct nvme_discovery_log_entry *dle)
{
	struct nvmf_association *na = qp->nq_association;

	dle->trtype = na->na_trtype;
	return (na->na_ops->populate_dle(qp, dle));
}

const char *
nvmf_transport_type(uint8_t trtype)
{
	static _Thread_local char buf[8];

	switch (trtype) {
	case NVMF_TRTYPE_RDMA:
		return ("RDMA");
	case NVMF_TRTYPE_FC:
		return ("Fibre Channel");
	case NVMF_TRTYPE_TCP:
		return ("TCP");
	case NVMF_TRTYPE_INTRA_HOST:
		return ("Intra-host");
	default:
		snprintf(buf, sizeof(buf), "0x%02x\n", trtype);
		return (buf);
	}
}

int
nvmf_pack_ioc_nvlist(struct nvmf_ioc_nv *nv, nvlist_t *nvl)
{
	int error;

	memset(nv, 0, sizeof(*nv));

	error = nvlist_error(nvl);
	if (error)
		return (error);

	nv->data = nvlist_pack(nvl, &nv->size);
	if (nv->data == NULL)
		return (ENOMEM);

	return (0);
}
