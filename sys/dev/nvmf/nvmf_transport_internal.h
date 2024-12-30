/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __NVMF_TRANSPORT_INTERNAL_H__
#define	__NVMF_TRANSPORT_INTERNAL_H__

#include <sys/_nv.h>
#include <sys/memdesc.h>

/*
 * Interface between the transport-independent APIs in
 * nvmf_transport.c and individual transports.
 */

struct module;
struct nvmf_io_request;

struct nvmf_transport_ops {
	/* Queue pair management. */
	struct nvmf_qpair *(*allocate_qpair)(bool controller,
	    const nvlist_t *nvl);
	void (*free_qpair)(struct nvmf_qpair *qp);

	/* Capsule operations. */
	struct nvmf_capsule *(*allocate_capsule)(struct nvmf_qpair *qp,
	    int how);
	void (*free_capsule)(struct nvmf_capsule *nc);
	int (*transmit_capsule)(struct nvmf_capsule *nc);
	uint8_t (*validate_command_capsule)(struct nvmf_capsule *nc);

	/* Transferring controller data. */
	size_t (*capsule_data_len)(const struct nvmf_capsule *nc);
	int (*receive_controller_data)(struct nvmf_capsule *nc,
	    uint32_t data_offset, struct nvmf_io_request *io);
	u_int (*send_controller_data)(struct nvmf_capsule *nc,
	    uint32_t data_offset, struct mbuf *m, size_t len);

	enum nvmf_trtype trtype;
	int priority;
};

/* Either an Admin or I/O Submission/Completion Queue pair. */
struct nvmf_qpair {
	struct nvmf_transport *nq_transport;
	struct nvmf_transport_ops *nq_ops;
	bool nq_controller;

	/* Callback to invoke for a received capsule. */
	nvmf_capsule_receive_t *nq_receive;
	void *nq_receive_arg;

	/* Callback to invoke for an error. */
	nvmf_qpair_error_t *nq_error;
	void *nq_error_arg;

	bool nq_admin;
};

struct nvmf_io_request {
	/*
	 * Data buffer contains io_len bytes in the backing store
	 * described by mem.
	 */
	struct memdesc io_mem;
	size_t	io_len;
	nvmf_io_complete_t *io_complete;
	void	*io_complete_arg;
};

/*
 * Fabrics Command and Response Capsules.  The Fabrics host
 * (initiator) and controller (target) drivers work with capsules that
 * are transmitted and received by a specific transport.
 */
struct nvmf_capsule {
	struct nvmf_qpair *nc_qpair;

	/* Either a SQE or CQE. */
	union {
		struct nvme_command nc_sqe;
		struct nvme_completion nc_cqe;
	};
	int	nc_qe_len;

	/*
	 * Is SQHD in received capsule valid?  False for locally-
	 * synthesized responses.
	 */
	bool	nc_sqhd_valid;

	bool	nc_send_data;
	struct nvmf_io_request nc_data;
};

static void __inline
nvmf_qpair_error(struct nvmf_qpair *nq, int error)
{
	nq->nq_error(nq->nq_error_arg, error);
}

static void __inline
nvmf_capsule_received(struct nvmf_qpair *nq, struct nvmf_capsule *nc)
{
	nq->nq_receive(nq->nq_receive_arg, nc);
}

static void __inline
nvmf_complete_io_request(struct nvmf_io_request *io, size_t xfered, int error)
{
	io->io_complete(io->io_complete_arg, xfered, error);
}

int	nvmf_transport_module_handler(struct module *, int, void *);

#define	NVMF_TRANSPORT(name, ops)					\
static moduledata_t nvmf_transport_##name##_mod = {			\
	"nvmf/" #name,							\
	nvmf_transport_module_handler,					\
	&(ops)								\
};									\
DECLARE_MODULE(nvmf_transport_##name, nvmf_transport_##name##_mod,	\
    SI_SUB_DRIVERS, SI_ORDER_ANY);					\
MODULE_DEPEND(nvmf_transport_##name, nvmf_transport, 1, 1, 1)

#endif /* !__NVMF_TRANSPORT_INTERNAL_H__ */
