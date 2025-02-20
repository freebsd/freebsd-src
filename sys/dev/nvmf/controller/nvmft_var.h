/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __NVMFT_VAR_H__
#define	__NVMFT_VAR_H__

#include <sys/_callout.h>
#include <sys/_nv.h>
#include <sys/refcount.h>
#include <sys/taskqueue.h>

#include <dev/nvmf/nvmf_proto.h>

#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_frontend.h>

struct nvmf_capsule;
struct nvmft_controller;
struct nvmft_qpair;

#define	NVMFT_NUM_AER		16

struct nvmft_port {
	TAILQ_ENTRY(nvmft_port) link;
	u_int	refs;
	struct ctl_port port;
	struct nvme_controller_data cdata;
	struct nvme_firmware_page fp;
	uint64_t cap;
	uint32_t max_io_qsize;
	uint16_t portid;
	bool	online;

	struct mtx lock;

	struct unrhdr *ids;
	TAILQ_HEAD(, nvmft_controller) controllers;

	uint32_t *active_ns;
	u_int	num_ns;
};

struct nvmft_io_qpair {
	struct nvmft_qpair *qp;

	bool shutdown;
};

struct nvmft_controller {
	struct nvmft_qpair *admin;
	struct nvmft_io_qpair *io_qpairs;
	u_int	num_io_queues;
	bool	shutdown;
	bool	admin_closed;
	uint16_t cntlid;
	uint32_t cc;
	uint32_t csts;

	struct nvmft_port *np;
	struct mtx lock;

	struct nvme_controller_data cdata;
	struct nvme_health_information_page hip;
	sbintime_t create_time;
	sbintime_t start_busy;
	sbintime_t busy_total;
	uint16_t partial_dur;
	uint16_t partial_duw;

	uint8_t	hostid[16];
	uint8_t	hostnqn[NVME_NQN_FIELD_SIZE];
	u_int	trtype;

	TAILQ_ENTRY(nvmft_controller) link;

	/*
	 * Each queue can have at most UINT16_MAX commands, so the total
	 * across all queues will fit in a uint32_t.
	 */
	uint32_t pending_commands;

	volatile int ka_active_traffic;
	struct callout ka_timer;
	sbintime_t ka_sbt;

	/* AER fields. */
	uint32_t aer_mask;
	uint16_t aer_cids[NVMFT_NUM_AER];
	uint8_t aer_pending;
	uint8_t aer_cidx;
	uint8_t aer_pidx;

	/* Changed namespace IDs. */
	struct nvme_ns_list *changed_ns;
	bool	changed_ns_reported;

	struct task shutdown_task;
	struct timeout_task terminate_task;
};

MALLOC_DECLARE(M_NVMFT);

/* ctl_frontend_nvmf.c */
void	nvmft_port_free(struct nvmft_port *np);
void	nvmft_populate_active_nslist(struct nvmft_port *np, uint32_t nsid,
    struct nvme_ns_list *nslist);
void	nvmft_dispatch_command(struct nvmft_qpair *qp,
    struct nvmf_capsule *nc, bool admin);
void	nvmft_terminate_commands(struct nvmft_controller *ctrlr);
void	nvmft_abort_datamove(union ctl_io *io);
void	nvmft_handle_datamove(union ctl_io *io);
void	nvmft_drain_task(struct task *task);
void	nvmft_enqueue_task(struct task *task);

/* nvmft_controller.c */
void	nvmft_controller_error(struct nvmft_controller *ctrlr,
    struct nvmft_qpair *qp, int error);
void	nvmft_controller_lun_changed(struct nvmft_controller *ctrlr,
    int lun_id);
void	nvmft_handle_admin_command(struct nvmft_controller *ctrlr,
    struct nvmf_capsule *nc);
void	nvmft_handle_io_command(struct nvmft_qpair *qp, uint16_t qid,
    struct nvmf_capsule *nc);
int	nvmft_handoff_admin_queue(struct nvmft_port *np,
    enum nvmf_trtype trtype, const nvlist_t *params,
    const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data);
int	nvmft_handoff_io_queue(struct nvmft_port *np, enum nvmf_trtype trtype,
    const nvlist_t *params, const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data);
int	nvmft_printf(struct nvmft_controller *ctrlr, const char *fmt, ...)
    __printflike(2, 3);

/* nvmft_qpair.c */
struct nvmft_qpair *nvmft_qpair_init(enum nvmf_trtype trtype,
    const nvlist_t *params, uint16_t qid, const char *name);
void	nvmft_qpair_shutdown(struct nvmft_qpair *qp);
void	nvmft_qpair_destroy(struct nvmft_qpair *qp);
struct nvmft_controller *nvmft_qpair_ctrlr(struct nvmft_qpair *qp);
void	nvmft_qpair_datamove(struct nvmft_qpair *qp, union ctl_io *io);
uint16_t nvmft_qpair_id(struct nvmft_qpair *qp);
const char *nvmft_qpair_name(struct nvmft_qpair *qp);
void	nvmft_command_completed(struct nvmft_qpair *qp,
    struct nvmf_capsule *nc);
int	nvmft_send_response(struct nvmft_qpair *qp, const void *cqe);
void	nvmft_init_cqe(void *cqe, struct nvmf_capsule *nc, uint16_t status);
int	nvmft_send_error(struct nvmft_qpair *qp, struct nvmf_capsule *nc,
    uint8_t sc_type, uint8_t sc_status);
int	nvmft_send_generic_error(struct nvmft_qpair *qp,
    struct nvmf_capsule *nc, uint8_t sc_status);
int	nvmft_send_success(struct nvmft_qpair *qp,
    struct nvmf_capsule *nc);
void	nvmft_connect_error(struct nvmft_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd, uint8_t sc_type,
    uint8_t sc_status);
void	nvmft_connect_invalid_parameters(struct nvmft_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd, bool data, uint16_t offset);
int	nvmft_finish_accept(struct nvmft_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd, struct nvmft_controller *ctrlr);

static __inline void
nvmft_port_ref(struct nvmft_port *np)
{
	refcount_acquire(&np->refs);
}

static __inline void
nvmft_port_rele(struct nvmft_port *np)
{
	if (refcount_release(&np->refs))
		nvmft_port_free(np);
}

#endif	/* !__NVMFT_VAR_H__ */
