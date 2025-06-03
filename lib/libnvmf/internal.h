/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __LIBNVMF_INTERNAL_H__
#define __LIBNVMF_INTERNAL_H__

#include <sys/nv.h>
#include <sys/queue.h>

struct nvmf_transport_ops {
	/* Association management. */
	struct nvmf_association *(*allocate_association)(bool controller,
	    const struct nvmf_association_params *params);
	void (*update_association)(struct nvmf_association *na,
	    const struct nvme_controller_data *cdata);
	void (*free_association)(struct nvmf_association *na);

	/* Queue pair management. */
	struct nvmf_qpair *(*allocate_qpair)(struct nvmf_association *na,
	    const struct nvmf_qpair_params *params);
	void (*free_qpair)(struct nvmf_qpair *qp);

	/* Add params for kernel handoff. */
	void (*kernel_handoff_params)(struct nvmf_qpair *qp, nvlist_t *nvl);
	int (*populate_dle)(struct nvmf_qpair *qp,
	    struct nvme_discovery_log_entry *dle);

	/* Capsule operations. */
	struct nvmf_capsule *(*allocate_capsule)(struct nvmf_qpair *qp);
	void (*free_capsule)(struct nvmf_capsule *nc);
	int (*transmit_capsule)(struct nvmf_capsule *nc);
	int (*receive_capsule)(struct nvmf_qpair *qp,
	    struct nvmf_capsule **ncp);
	uint8_t (*validate_command_capsule)(const struct nvmf_capsule *nc);

	/* Transferring controller data. */
	size_t (*capsule_data_len)(const struct nvmf_capsule *nc);
	int (*receive_controller_data)(const struct nvmf_capsule *nc,
	    uint32_t data_offset, void *buf, size_t len);
	int (*send_controller_data)(const struct nvmf_capsule *nc,
	    const void *buf, size_t len);
};

struct nvmf_association {
	struct nvmf_transport_ops *na_ops;
	enum nvmf_trtype na_trtype;
	bool na_controller;

	struct nvmf_association_params na_params;

	/* Each qpair holds a reference on an association. */
	u_int na_refs;

	char *na_last_error;
};

struct nvmf_qpair {
	struct nvmf_association *nq_association;
	bool nq_admin;

	uint16_t nq_cid;	/* host only */

	/*
	 * Queue sizes.  This assumes the same size for both the
	 * completion and submission queues within a pair.
	 */
	u_int	nq_qsize;

	/* Flow control management for submission queues. */
	bool nq_flow_control;
	uint16_t nq_sqhd;
	uint16_t nq_sqtail;	/* host only */

	/* Value in response to/from CONNECT. */
	uint16_t nq_cntlid;

	uint32_t nq_kato;	/* valid on admin queue only */

	TAILQ_HEAD(, nvmf_capsule) nq_rx_capsules;
};

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

	/* Data buffer. */
	bool	nc_send_data;
	void	*nc_data;
	size_t	nc_data_len;

	TAILQ_ENTRY(nvmf_capsule) nc_link;
};

extern struct nvmf_transport_ops tcp_ops;

void	na_clear_error(struct nvmf_association *na);
void	na_error(struct nvmf_association *na, const char *fmt, ...);

int	nvmf_kernel_handoff_params(struct nvmf_qpair *qp, nvlist_t **nvlp);
int	nvmf_populate_dle(struct nvmf_qpair *qp,
    struct nvme_discovery_log_entry *dle);
int	nvmf_pack_ioc_nvlist(struct nvmf_ioc_nv *nv, nvlist_t *nvl);

#endif /* !__LIBNVMF_INTERNAL_H__ */
