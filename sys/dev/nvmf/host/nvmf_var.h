/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __NVMF_VAR_H__
#define	__NVMF_VAR_H__

#include <sys/_callout.h>
#include <sys/_eventhandler.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
//#include <sys/_nv.h>
#include <sys/_sx.h>
#include <sys/_task.h>
#include <sys/smp.h>
#include <sys/queue.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/nvmf_transport.h>

struct nvmf_aer;
struct nvmf_capsule;
struct nvmf_host_qpair;
struct nvmf_namespace;
struct sysctl_oid_list;

typedef void nvmf_request_complete_t(void *, const struct nvme_completion *);

struct nvmf_softc {
	device_t dev;

	struct nvmf_host_qpair *admin;
	struct nvmf_host_qpair **io;
	u_int	num_io_queues;
	enum nvmf_trtype trtype;

	struct cam_sim *sim;
	struct cam_path *path;
	struct mtx sim_mtx;
	bool sim_disconnected;
	bool sim_shutdown;

	struct nvmf_namespace **ns;

	struct nvme_controller_data *cdata;
	uint64_t cap;
	uint32_t vs;
	u_int max_pending_io;
	u_long max_xfer_size;

	struct cdev *cdev;

	/*
	 * Keep Alive support depends on two timers.  The 'tx' timer
	 * is responsible for sending KeepAlive commands and runs at
	 * half the timeout interval.  The 'rx' timer is responsible
	 * for detecting an actual timeout.
	 *
	 * For efficient support of TKAS, the host does not reschedule
	 * these timers every time new commands are scheduled.
	 * Instead, the host sets the *_traffic flags when commands
	 * are sent and received.  The timeout handlers check and
	 * clear these flags.  This does mean it can take up to twice
	 * the timeout time to detect an AWOL controller.
	 */
	bool	ka_traffic;			/* Using TKAS? */

	volatile int ka_active_tx_traffic;
	struct callout ka_tx_timer;
	sbintime_t ka_tx_sbt;

	volatile int ka_active_rx_traffic;
	struct callout ka_rx_timer;
	sbintime_t ka_rx_sbt;

	struct timeout_task request_reconnect_task;
	struct timeout_task controller_loss_task;
	uint32_t reconnect_delay;
	uint32_t controller_loss_timeout;

	struct sx connection_lock;
	struct task disconnect_task;
	bool detaching;
	bool controller_timedout;

	u_int num_aer;
	struct nvmf_aer *aer;

	struct sysctl_oid_list *ioq_oid_list;

	nvlist_t *rparams;

	struct timespec last_disconnect;

	eventhandler_tag shutdown_pre_sync_eh;
	eventhandler_tag shutdown_post_sync_eh;
};

struct nvmf_request {
	struct nvmf_host_qpair *qp;
	struct nvmf_capsule *nc;
	nvmf_request_complete_t *cb;
	void	*cb_arg;
	bool	aer;

	STAILQ_ENTRY(nvmf_request) link;
};

struct nvmf_completion_status {
	struct nvme_completion cqe;
	bool	done;
	bool	io_done;
	int	io_error;
};

static __inline struct nvmf_host_qpair *
nvmf_select_io_queue(struct nvmf_softc *sc)
{
	u_int idx = curcpu * sc->num_io_queues / (mp_maxid + 1);
	return (sc->io[idx]);
}

static __inline bool
nvmf_cqe_aborted(const struct nvme_completion *cqe)
{
	uint16_t status;

	status = le16toh(cqe->status);
	return (NVME_STATUS_GET_SCT(status) == NVME_SCT_PATH_RELATED &&
	    NVME_STATUS_GET_SC(status) == NVME_SC_COMMAND_ABORTED_BY_HOST);
}

static __inline void
nvmf_status_init(struct nvmf_completion_status *status)
{
	status->done = false;
	status->io_done = true;
	status->io_error = 0;
}

static __inline void
nvmf_status_wait_io(struct nvmf_completion_status *status)
{
	status->io_done = false;
}

#ifdef DRIVER_MODULE
extern driver_t nvme_nvmf_driver;
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NVMF);
#endif

/* If true, I/O requests will fail while the host is disconnected. */
extern bool nvmf_fail_disconnect;

/* nvmf.c */
void	nvmf_complete(void *arg, const struct nvme_completion *cqe);
void	nvmf_io_complete(void *arg, size_t xfered, int error);
void	nvmf_wait_for_reply(struct nvmf_completion_status *status);
int	nvmf_copyin_handoff(const struct nvmf_ioc_nv *nv, nvlist_t **nvlp);
void	nvmf_disconnect(struct nvmf_softc *sc);
void	nvmf_rescan_ns(struct nvmf_softc *sc, uint32_t nsid);
void	nvmf_rescan_all_ns(struct nvmf_softc *sc);
int	nvmf_passthrough_cmd(struct nvmf_softc *sc, struct nvme_pt_command *pt,
    bool admin);

/* nvmf_aer.c */
void	nvmf_init_aer(struct nvmf_softc *sc);
int	nvmf_start_aer(struct nvmf_softc *sc);
void	nvmf_destroy_aer(struct nvmf_softc *sc);

/* nvmf_cmd.c */
bool	nvmf_cmd_get_property(struct nvmf_softc *sc, uint32_t offset,
    uint8_t size, nvmf_request_complete_t *cb, void *cb_arg, int how);
bool	nvmf_cmd_set_property(struct nvmf_softc *sc, uint32_t offset,
    uint8_t size, uint64_t value, nvmf_request_complete_t *cb, void *cb_arg,
    int how);
bool	nvmf_cmd_keep_alive(struct nvmf_softc *sc, nvmf_request_complete_t *cb,
    void *cb_arg, int how);
bool	nvmf_cmd_identify_active_namespaces(struct nvmf_softc *sc, uint32_t id,
    struct nvme_ns_list *nslist, nvmf_request_complete_t *req_cb,
    void *req_cb_arg, nvmf_io_complete_t *io_cb, void *io_cb_arg, int how);
bool	nvmf_cmd_identify_namespace(struct nvmf_softc *sc, uint32_t id,
    struct nvme_namespace_data *nsdata, nvmf_request_complete_t *req_cb,
    void *req_cb_arg, nvmf_io_complete_t *io_cb, void *io_cb_arg, int how);
bool	nvmf_cmd_get_log_page(struct nvmf_softc *sc, uint32_t nsid, uint8_t lid,
    uint64_t offset, void *buf, size_t len, nvmf_request_complete_t *req_cb,
    void *req_cb_arg, nvmf_io_complete_t *io_cb, void *io_cb_arg, int how);

/* nvmf_ctldev.c */
int	nvmf_ctl_load(void);
void	nvmf_ctl_unload(void);

/* nvmf_ns.c */
struct nvmf_namespace *nvmf_init_ns(struct nvmf_softc *sc, uint32_t id,
    const struct nvme_namespace_data *data);
void	nvmf_disconnect_ns(struct nvmf_namespace *ns);
void	nvmf_reconnect_ns(struct nvmf_namespace *ns);
void	nvmf_shutdown_ns(struct nvmf_namespace *ns);
void	nvmf_destroy_ns(struct nvmf_namespace *ns);
bool	nvmf_update_ns(struct nvmf_namespace *ns,
    const struct nvme_namespace_data *data);

/* nvmf_qpair.c */
struct nvmf_host_qpair *nvmf_init_qp(struct nvmf_softc *sc,
    enum nvmf_trtype trtype, const nvlist_t *nvl, const char *name, u_int qid);
void	nvmf_shutdown_qp(struct nvmf_host_qpair *qp);
void	nvmf_destroy_qp(struct nvmf_host_qpair *qp);
struct nvmf_request *nvmf_allocate_request(struct nvmf_host_qpair *qp,
    void *sqe, nvmf_request_complete_t *cb, void *cb_arg, int how);
void	nvmf_submit_request(struct nvmf_request *req);
void	nvmf_free_request(struct nvmf_request *req);

/* nvmf_sim.c */
int	nvmf_init_sim(struct nvmf_softc *sc);
void	nvmf_disconnect_sim(struct nvmf_softc *sc);
void	nvmf_reconnect_sim(struct nvmf_softc *sc);
void	nvmf_shutdown_sim(struct nvmf_softc *sc);
void	nvmf_destroy_sim(struct nvmf_softc *sc);
void	nvmf_sim_rescan_ns(struct nvmf_softc *sc, uint32_t id);

#endif /* !__NVMF_VAR_H__ */
