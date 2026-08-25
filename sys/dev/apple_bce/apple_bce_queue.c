/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple BCE queue management -- CQ, SQ, and command queue operations.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <machine/atomic.h>

#include "apple_bce.h"
#include "apple_bce_queue.h"

static MALLOC_DEFINE(M_BCE, "apple_bce", "Apple BCE driver");

#define BCE_CMDQ_TIMEOUT_MS	5000

/*
 * DMA callback for bus_dmamap_load.
 */
struct bce_dma_cb_arg {
	bus_addr_t	addr;
	int		error;
};

static int
bce_cmdq_wait(struct bce_queue_cmdq *cmdq, uint32_t slot,
    struct bce_queue_cmdq_result *res)
{
	int error;

	error = sema_timedwait(&res->cmpl, hz * BCE_CMDQ_TIMEOUT_MS / 1000);
	if (error == 0)
		return (0);

	mtx_lock(&cmdq->lck);
	if (cmdq->tres[slot] == res)
		cmdq->tres[slot] = NULL;
	mtx_unlock(&cmdq->lck);

	return (ETIMEDOUT);
}

static void
bce_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bce_dma_cb_arg *cb = arg;

	cb->error = error;
	if (error == 0)
		cb->addr = segs[0].ds_addr;
}

/*
 * Allocate a completion queue with DMA-coherent memory.
 */
struct bce_queue_cq *
bce_alloc_cq(struct apple_bce_softc *sc, int qid, uint32_t el_count)
{
	struct bce_queue_cq *cq;
	struct bce_dma_cb_arg cb;
	int error;

	cq = malloc(sizeof(*cq), M_BCE, M_WAITOK | M_ZERO);
	cq->qid = qid;
	cq->el_count = el_count;
	cq->index = 0;

	error = bus_dma_tag_create(sc->sc_dma_tag,
	    4, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter */
	    el_count * sizeof(struct bce_qe_completion),
	    1,				/* nsegments */
	    el_count * sizeof(struct bce_qe_completion),
	    BUS_DMA_WAITOK,		/* flags */
	    NULL, NULL,			/* lockfunc */
	    &cq->dma_tag);
	if (error) {
		free(cq, M_BCE);
		return (NULL);
	}

	error = bus_dmamem_alloc(cq->dma_tag, (void **)&cq->data,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT, &cq->dma_map);
	if (error) {
		bus_dma_tag_destroy(cq->dma_tag);
		free(cq, M_BCE);
		return (NULL);
	}

	error = bus_dmamap_load(cq->dma_tag, cq->dma_map, cq->data,
	    el_count * sizeof(struct bce_qe_completion),
	    bce_dma_cb, &cb, BUS_DMA_WAITOK);
	if (error || cb.error) {
		bus_dmamem_free(cq->dma_tag, cq->data, cq->dma_map);
		bus_dma_tag_destroy(cq->dma_tag);
		free(cq, M_BCE);
		return (NULL);
	}
	cq->dma_addr = cb.addr;

	return (cq);
}

void
bce_free_cq(struct apple_bce_softc *sc, struct bce_queue_cq *cq)
{
	if (cq == NULL)
		return;
	bus_dmamap_unload(cq->dma_tag, cq->dma_map);
	bus_dmamem_free(cq->dma_tag, cq->data, cq->dma_map);
	bus_dma_tag_destroy(cq->dma_tag);
	free(cq, M_BCE);
}

void
bce_get_cq_memcfg(struct bce_queue_cq *cq, struct bce_queue_memcfg *cfg)
{
	cfg->qid = (uint16_t)cq->qid;
	cfg->el_count = (uint16_t)cq->el_count;
	cfg->vector_or_cq = 0;
	cfg->_pad = 0;
	cfg->addr = cq->dma_addr;
	cfg->length = cq->el_count * sizeof(struct bce_qe_completion);
}

/*
 * Process completions from a CQ and dispatch to the target SQs.
 */
void
bce_handle_cq_completions(struct apple_bce_softc *sc, struct bce_queue_cq *cq)
{
	struct bce_qe_completion *e;
	struct bce_queue_sq *sq;
	int i;
	bus_dmamap_sync(cq->dma_tag, cq->dma_map, BUS_DMASYNC_POSTREAD);

	e = bce_cq_element(cq, cq->index);
	if (!(e->flags & BCE_CQ_FLAG_PENDING))
		return;

	while ((e = bce_cq_element(cq, cq->index))->flags &
	    BCE_CQ_FLAG_PENDING) {
		/* Route completion to target SQ (skip qid 0 which is the CQ) */
		if (e->qid > 0 && e->qid < BCE_MAX_QUEUE_COUNT) {
			sq = (struct bce_queue_sq *)sc->sc_queues[e->qid];
			if (sq != NULL &&
			    e->completion_index < sq->el_count) {
				sq->completion_data[e->completion_index].status =
				    e->status;
				sq->completion_data[e->completion_index].data_size =
				    e->data_size;
				sq->completion_data[e->completion_index].result =
				    e->result;
				/* Advance tail so completion callback sees it */
				sq->completion_tail =
				    (e->completion_index + 1) % sq->el_count;
				sq->has_pending = 1;
			}
		}

		e->flags = 0;
		cq->index = (cq->index + 1) % cq->el_count;
	}

	bus_dmamap_sync(cq->dma_tag, cq->dma_map, BUS_DMASYNC_PREWRITE);

	/* Ring doorbell with updated consumer index */
	bus_write_4(sc->sc_bar2, BCE_REG_DOORBELL_BASE + cq->qid * 4,
	    cq->index);

	/* Fire callbacks on SQs that received completions */
	for (i = 0; i < BCE_MAX_QUEUE_COUNT; i++) {
		sq = sc->sc_int_sq_list[i];
		if (sq != NULL && sq->has_pending) {
			sq->has_pending = 0;
			if (sq->completion != NULL)
				sq->completion(sq);
		}
	}
}

/*
 * Allocate a submission queue with DMA-coherent memory.
 */
struct bce_queue_sq *
bce_alloc_sq(struct apple_bce_softc *sc, int qid, uint32_t el_size,
    uint32_t el_count, bce_sq_completion_fn compl, void *userdata)
{
	struct bce_queue_sq *sq;
	struct bce_dma_cb_arg cb;
	int error;

	sq = malloc(sizeof(*sq), M_BCE, M_WAITOK | M_ZERO);
	sq->qid = qid;
	sq->el_size = el_size;
	sq->el_count = el_count;
	sq->completion = compl;
	sq->userdata = userdata;
	sq->available_commands = el_count - 1;

	error = bus_dma_tag_create(sc->sc_dma_tag,
	    4, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    el_count * el_size, 1, el_count * el_size,
	    BUS_DMA_WAITOK,
	    NULL, NULL,
	    &sq->dma_tag);
	if (error) {
		free(sq, M_BCE);
		return (NULL);
	}

	error = bus_dmamem_alloc(sq->dma_tag, &sq->data,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT, &sq->dma_map);
	if (error) {
		bus_dma_tag_destroy(sq->dma_tag);
		free(sq, M_BCE);
		return (NULL);
	}

	error = bus_dmamap_load(sq->dma_tag, sq->dma_map, sq->data,
	    el_count * el_size, bce_dma_cb, &cb, BUS_DMA_WAITOK);
	if (error || cb.error) {
		bus_dmamem_free(sq->dma_tag, sq->data, sq->dma_map);
		bus_dma_tag_destroy(sq->dma_tag);
		free(sq, M_BCE);
		return (NULL);
	}
	sq->dma_addr = cb.addr;

	sq->completion_data = malloc(sizeof(*sq->completion_data) * el_count,
	    M_BCE, M_WAITOK | M_ZERO);

	return (sq);
}

void
bce_free_sq(struct apple_bce_softc *sc, struct bce_queue_sq *sq)
{
	if (sq == NULL)
		return;
	free(sq->completion_data, M_BCE);
	bus_dmamap_unload(sq->dma_tag, sq->dma_map);
	bus_dmamem_free(sq->dma_tag, sq->data, sq->dma_map);
	bus_dma_tag_destroy(sq->dma_tag);
	free(sq, M_BCE);
}

void
bce_get_sq_memcfg(struct bce_queue_sq *sq, struct bce_queue_cq *cq,
    struct bce_queue_memcfg *cfg)
{
	cfg->qid = (uint16_t)sq->qid;
	cfg->el_count = (uint16_t)sq->el_count;
	cfg->vector_or_cq = (uint16_t)cq->qid;
	cfg->_pad = 0;
	cfg->addr = sq->dma_addr;
	cfg->length = sq->el_count * sq->el_size;
}

int
bce_reserve_submission(struct bce_queue_sq *sq)
{
	int old, new;

	do {
		old = atomic_load_int(&sq->available_commands);
		if (old <= 0)
			return (EAGAIN);
		new = old - 1;
	} while (!atomic_cmpset_int(&sq->available_commands, old, new));

	return (0);
}

void *
bce_next_submission(struct bce_queue_sq *sq)
{
	void *ret;

	ret = bce_sq_element(sq, sq->tail);
	sq->tail = (sq->tail + 1) % sq->el_count;
	return (ret);
}

void
bce_submit_to_device(struct apple_bce_softc *sc, struct bce_queue_sq *sq)
{
	bus_dmamap_sync(sq->dma_tag, sq->dma_map, BUS_DMASYNC_PREWRITE);
	bus_write_4(sc->sc_bar2, BCE_REG_DOORBELL_BASE + sq->qid * 4,
	    sq->tail);
}

void
bce_notify_submission_complete(struct bce_queue_sq *sq)
{
	sq->head = (sq->head + 1) % sq->el_count;
	atomic_add_int(&sq->available_commands, 1);
}

/*
 * Command queue -- wraps an SQ for synchronous control operations.
 */
static void	bce_cmdq_completion(struct bce_queue_sq *sq);

struct bce_queue_cmdq *
bce_alloc_cmdq(struct apple_bce_softc *sc, struct bce_queue_sq *sq)
{
	struct bce_queue_cmdq *cmdq;

	cmdq = malloc(sizeof(*cmdq), M_BCE, M_WAITOK | M_ZERO);
	cmdq->sq = sq;
	mtx_init(&cmdq->lck, "bce_cmdq", NULL, MTX_DEF);
	cmdq->tres = malloc(sizeof(void *) * sq->el_count, M_BCE,
	    M_WAITOK | M_ZERO);

	/* Wire up completion callback */
	sq->completion = bce_cmdq_completion;
	sq->userdata = cmdq;

	return (cmdq);
}

void
bce_free_cmdq(struct bce_queue_cmdq *cmdq)
{
	if (cmdq == NULL)
		return;
	mtx_destroy(&cmdq->lck);
	free(cmdq->tres, M_BCE);
	free(cmdq, M_BCE);
}

/*
 * Command queue completion callback -- wake waiters.
 */
static void
bce_cmdq_completion(struct bce_queue_sq *sq)
{
	struct bce_queue_cmdq *cmdq = sq->userdata;
	struct bce_queue_cmdq_result *res;

	mtx_lock(&cmdq->lck);
	while (sq->completion_cidx != sq->completion_tail) {
		struct bce_sq_completion_data *cd;

		cd = &sq->completion_data[sq->completion_cidx];
		res = cmdq->tres[sq->completion_cidx];
		if (res != NULL) {
			res->status = cd->status;
			res->result = cd->result;
			sema_post(&res->cmpl);
			cmdq->tres[sq->completion_cidx] = NULL;
		}
		sq->completion_cidx = (sq->completion_cidx + 1) %
		    sq->el_count;
		bce_notify_submission_complete(sq);
	}
	mtx_unlock(&cmdq->lck);
}

uint32_t
bce_cmd_register_queue(struct bce_queue_cmdq *cmdq,
    struct apple_bce_softc *sc, struct bce_queue_memcfg *cfg,
    const char *name, int isdirout)
{
	struct bce_queue_cmdq_result res;
	struct bce_cmdq_reg_cmd *cmd;
	uint32_t slot;
	int error;

	sema_init(&res.cmpl, 0, "bce_cmd");

	if (bce_reserve_submission(cmdq->sq) != 0) {
		sema_destroy(&res.cmpl);
		return (EAGAIN);
	}

	mtx_lock(&cmdq->lck);
	slot = cmdq->sq->tail;
	cmdq->tres[slot] = &res;
	cmd = bce_next_submission(cmdq->sq);

	memset(cmd, 0, BCE_CMD_SIZE);
	cmd->cmd = BCE_CMD_REGISTER_QUEUE;
	cmd->flags = (name ? BCE_CMDQ_FLAG_NAMED : 0) |
	    (isdirout ? BCE_CMDQ_FLAG_OUT : 0);
	cmd->qid = cfg->qid;
	cmd->el_count = cfg->el_count;
	cmd->vector_or_cq = cfg->vector_or_cq;
	if (name != NULL) {
		cmd->name_len = (uint16_t)MIN(strlen(name), sizeof(cmd->name));
		memcpy(cmd->name, name, cmd->name_len);
	}
	cmd->addr = cfg->addr;
	cmd->length = cfg->length;

	bce_submit_to_device(sc, cmdq->sq);
	mtx_unlock(&cmdq->lck);

	error = bce_cmdq_wait(cmdq, slot, &res);
	sema_destroy(&res.cmpl);
	if (error != 0)
		return (error);

	return (res.status);
}

uint32_t
bce_cmd_unregister_queue(struct bce_queue_cmdq *cmdq,
    struct apple_bce_softc *sc, int qid)
{
	struct bce_queue_cmdq_result res;
	struct bce_cmdq_simple_cmd *cmd;
	uint32_t slot;
	int error;

	sema_init(&res.cmpl, 0, "bce_cmd");

	if (bce_reserve_submission(cmdq->sq) != 0) {
		sema_destroy(&res.cmpl);
		return (EAGAIN);
	}

	mtx_lock(&cmdq->lck);
	slot = cmdq->sq->tail;
	cmdq->tres[slot] = &res;
	cmd = bce_next_submission(cmdq->sq);

	memset(cmd, 0, BCE_CMD_SIZE);
	cmd->cmd = BCE_CMD_UNREGISTER_QUEUE;
	cmd->qid = (uint16_t)qid;

	bce_submit_to_device(sc, cmdq->sq);
	mtx_unlock(&cmdq->lck);

	error = bce_cmdq_wait(cmdq, slot, &res);
	sema_destroy(&res.cmpl);
	if (error != 0)
		return (error);

	return (res.status);
}

uint32_t
bce_cmd_flush_queue(struct bce_queue_cmdq *cmdq,
    struct apple_bce_softc *sc, int qid)
{
	struct bce_queue_cmdq_result res;
	struct bce_cmdq_simple_cmd *cmd;
	uint32_t slot;
	int error;

	sema_init(&res.cmpl, 0, "bce_cmd");

	if (bce_reserve_submission(cmdq->sq) != 0) {
		sema_destroy(&res.cmpl);
		return (EAGAIN);
	}

	mtx_lock(&cmdq->lck);
	slot = cmdq->sq->tail;
	cmdq->tres[slot] = &res;
	cmd = bce_next_submission(cmdq->sq);

	memset(cmd, 0, BCE_CMD_SIZE);
	cmd->cmd = BCE_CMD_FLUSH_QUEUE;
	cmd->qid = (uint16_t)qid;

	bce_submit_to_device(sc, cmdq->sq);
	mtx_unlock(&cmdq->lck);

	error = bce_cmdq_wait(cmdq, slot, &res);
	sema_destroy(&res.cmpl);
	if (error != 0)
		return (error);

	return (res.status);
}
