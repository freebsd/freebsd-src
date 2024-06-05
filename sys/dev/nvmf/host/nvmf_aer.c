/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <dev/nvmf/host/nvmf_var.h>

struct nvmf_aer {
	struct nvmf_softc *sc;
	uint8_t log_page_id;
	uint8_t info;
	uint8_t type;

	u_int	page_len;
	void	*page;

	int	error;
	uint16_t status;
	int	pending;
	struct mtx *lock;
	struct task complete_task;
	struct task finish_page_task;
};

#define	MAX_LOG_PAGE_SIZE	4096

static void	nvmf_complete_aer(void *arg, const struct nvme_completion *cqe);

static void
nvmf_submit_aer(struct nvmf_softc *sc, struct nvmf_aer *aer)
{
	struct nvmf_request *req;
	struct nvme_command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opc = NVME_OPC_ASYNC_EVENT_REQUEST;

	req = nvmf_allocate_request(sc->admin, &cmd, nvmf_complete_aer, aer,
	    M_WAITOK);
	if (req == NULL)
		return;
	req->aer = true;
	nvmf_submit_request(req);
}

static void
nvmf_handle_changed_namespaces(struct nvmf_softc *sc,
    struct nvme_ns_list *ns_list)
{
	uint32_t nsid;

	/*
	 * If more than 1024 namespaces have changed, we should
	 * probably just rescan the entire set of namespaces.
	 */
	if (ns_list->ns[0] == 0xffffffff) {
		nvmf_rescan_all_ns(sc);
		return;
	}

	for (u_int i = 0; i < nitems(ns_list->ns); i++) {
		if (ns_list->ns[i] == 0)
			break;

		nsid = le32toh(ns_list->ns[i]);
		nvmf_rescan_ns(sc, nsid);
	}
}

static void
nvmf_finish_aer_page(struct nvmf_softc *sc, struct nvmf_aer *aer)
{
	/* If an error occurred fetching the page, just bail. */
	if (aer->error != 0 || aer->status != 0)
		return;

	taskqueue_enqueue(taskqueue_thread, &aer->finish_page_task);
}

static void
nvmf_finish_aer_page_task(void *arg, int pending)
{
	struct nvmf_aer *aer = arg;
	struct nvmf_softc *sc = aer->sc;

	switch (aer->log_page_id) {
	case NVME_LOG_ERROR:
		/* TODO: Should we log these? */
		break;
	case NVME_LOG_CHANGED_NAMESPACE:
		nvmf_handle_changed_namespaces(sc, aer->page);
		break;
	}

	/* Resubmit this AER command. */
	nvmf_submit_aer(sc, aer);
}

static void
nvmf_io_complete_aer_page(void *arg, size_t xfered, int error)
{
	struct nvmf_aer *aer = arg;
	struct nvmf_softc *sc = aer->sc;

	mtx_lock(aer->lock);
	aer->error = error;
	aer->pending--;
	if (aer->pending == 0) {
		mtx_unlock(aer->lock);
		nvmf_finish_aer_page(sc, aer);
	} else
		mtx_unlock(aer->lock);
}

static void
nvmf_complete_aer_page(void *arg, const struct nvme_completion *cqe)
{
	struct nvmf_aer *aer = arg;
	struct nvmf_softc *sc = aer->sc;

	mtx_lock(aer->lock);
	aer->status = cqe->status;
	aer->pending--;
	if (aer->pending == 0) {
		mtx_unlock(aer->lock);
		nvmf_finish_aer_page(sc, aer);
	} else
		mtx_unlock(aer->lock);
}

static u_int
nvmf_log_page_size(struct nvmf_softc *sc, uint8_t log_page_id)
{
	switch (log_page_id) {
	case NVME_LOG_ERROR:
		return ((sc->cdata->elpe + 1) *
		    sizeof(struct nvme_error_information_entry));
	case NVME_LOG_CHANGED_NAMESPACE:
		return (sizeof(struct nvme_ns_list));
	default:
		return (0);
	}
}

static void
nvmf_complete_aer(void *arg, const struct nvme_completion *cqe)
{
	struct nvmf_aer *aer = arg;
	struct nvmf_softc *sc = aer->sc;
	uint32_t cdw0;

	/*
	 * The only error defined for AER is an abort due to
	 * submitting too many AER commands.  Just discard this AER
	 * without resubmitting if we get an error.
	 *
	 * NB: Pending AER commands are aborted during controller
	 * shutdown, so discard aborted commands silently.
	 */
	if (cqe->status != 0) {
		if (!nvmf_cqe_aborted(cqe))
			device_printf(sc->dev, "Ignoring error %#x for AER\n",
			    le16toh(cqe->status));
		return;
	}

	cdw0 = le32toh(cqe->cdw0);
	aer->log_page_id = NVMEV(NVME_ASYNC_EVENT_LOG_PAGE_ID, cdw0);
	aer->info = NVMEV(NVME_ASYNC_EVENT_INFO, cdw0);
	aer->type = NVMEV(NVME_ASYNC_EVENT_TYPE, cdw0);

	device_printf(sc->dev, "AER type %u, info %#x, page %#x\n",
	    aer->type, aer->info, aer->log_page_id);

	aer->page_len = nvmf_log_page_size(sc, aer->log_page_id);
	taskqueue_enqueue(taskqueue_thread, &aer->complete_task);
}

static void
nvmf_complete_aer_task(void *arg, int pending)
{
	struct nvmf_aer *aer = arg;
	struct nvmf_softc *sc = aer->sc;

	if (aer->page_len != 0) {
		/* Read the associated log page. */
		aer->page_len = MIN(aer->page_len, MAX_LOG_PAGE_SIZE);
		aer->pending = 2;
		(void) nvmf_cmd_get_log_page(sc, NVME_GLOBAL_NAMESPACE_TAG,
		    aer->log_page_id, 0, aer->page, aer->page_len,
		    nvmf_complete_aer_page, aer, nvmf_io_complete_aer_page,
		    aer, M_WAITOK);
	} else {
		/* Resubmit this AER command. */
		nvmf_submit_aer(sc, aer);
	}
}

static int
nvmf_set_async_event_config(struct nvmf_softc *sc, uint32_t config)
{
	struct nvme_command cmd;
	struct nvmf_completion_status status;
	struct nvmf_request *req;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opc = NVME_OPC_SET_FEATURES;
	cmd.cdw10 = htole32(NVME_FEAT_ASYNC_EVENT_CONFIGURATION);
	cmd.cdw11 = htole32(config);

	nvmf_status_init(&status);
	req = nvmf_allocate_request(sc->admin, &cmd, nvmf_complete, &status,
	    M_WAITOK);
	if (req == NULL) {
		device_printf(sc->dev,
		    "failed to allocate SET_FEATURES (ASYNC_EVENT_CONFIGURATION) command\n");
		return (ECONNABORTED);
	}
	nvmf_submit_request(req);
	nvmf_wait_for_reply(&status);

	if (status.cqe.status != 0) {
		device_printf(sc->dev,
		    "SET_FEATURES (ASYNC_EVENT_CONFIGURATION) failed, status %#x\n",
		    le16toh(status.cqe.status));
		return (EIO);
	}

	return (0);
}

void
nvmf_init_aer(struct nvmf_softc *sc)
{
	/* 8 matches NVME_MAX_ASYNC_EVENTS */
	sc->num_aer = min(8, sc->cdata->aerl + 1);
	sc->aer = mallocarray(sc->num_aer, sizeof(*sc->aer), M_NVMF,
	    M_WAITOK | M_ZERO);
	for (u_int i = 0; i < sc->num_aer; i++) {
		sc->aer[i].sc = sc;
		sc->aer[i].page = malloc(MAX_LOG_PAGE_SIZE, M_NVMF, M_WAITOK);
		sc->aer[i].lock = mtx_pool_find(mtxpool_sleep, &sc->aer[i]);
		TASK_INIT(&sc->aer[i].complete_task, 0, nvmf_complete_aer_task,
		    &sc->aer[i]);
		TASK_INIT(&sc->aer[i].finish_page_task, 0,
		    nvmf_finish_aer_page_task, &sc->aer[i]);
	}
}

int
nvmf_start_aer(struct nvmf_softc *sc)
{
	uint32_t async_event_config;
	int error;

	async_event_config = NVME_CRIT_WARN_ST_AVAILABLE_SPARE |
	    NVME_CRIT_WARN_ST_DEVICE_RELIABILITY |
	    NVME_CRIT_WARN_ST_READ_ONLY |
	    NVME_CRIT_WARN_ST_VOLATILE_MEMORY_BACKUP;
	if (sc->cdata->ver >= NVME_REV(1, 2))
		async_event_config |=
		    sc->cdata->oaes & NVME_ASYNC_EVENT_NS_ATTRIBUTE;
	error = nvmf_set_async_event_config(sc, async_event_config);
	if (error != 0)
		return (error);

	for (u_int i = 0; i < sc->num_aer; i++)
		nvmf_submit_aer(sc, &sc->aer[i]);

	return (0);
}

void
nvmf_destroy_aer(struct nvmf_softc *sc)
{
	for (u_int i = 0; i < sc->num_aer; i++) {
		taskqueue_drain(taskqueue_thread, &sc->aer[i].complete_task);
		taskqueue_drain(taskqueue_thread, &sc->aer[i].finish_page_task);
		free(sc->aer[i].page, M_NVMF);
	}
	free(sc->aer, M_NVMF);
}
