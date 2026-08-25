/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple BCE queue management.
 */

#ifndef _APPLE_BCE_QUEUE_H_
#define _APPLE_BCE_QUEUE_H_

#include "apple_bce.h"

struct apple_bce_softc;

/* CQ operations */
struct bce_queue_cq *bce_alloc_cq(struct apple_bce_softc *sc, int qid,
		    uint32_t el_count);
void	bce_free_cq(struct apple_bce_softc *sc, struct bce_queue_cq *cq);
void	bce_get_cq_memcfg(struct bce_queue_cq *cq,
	    struct bce_queue_memcfg *cfg);
void	bce_handle_cq_completions(struct apple_bce_softc *sc,
	    struct bce_queue_cq *cq);

/* SQ operations */
struct bce_queue_sq *bce_alloc_sq(struct apple_bce_softc *sc, int qid,
		    uint32_t el_size, uint32_t el_count,
		    bce_sq_completion_fn compl, void *userdata);
void	bce_free_sq(struct apple_bce_softc *sc, struct bce_queue_sq *sq);
void	bce_get_sq_memcfg(struct bce_queue_sq *sq, struct bce_queue_cq *cq,
	    struct bce_queue_memcfg *cfg);

/* Submission helpers */
int	bce_reserve_submission(struct bce_queue_sq *sq);
void	*bce_next_submission(struct bce_queue_sq *sq);
void	bce_submit_to_device(struct apple_bce_softc *sc,
	    struct bce_queue_sq *sq);
void	bce_notify_submission_complete(struct bce_queue_sq *sq);

/* Command queue */
struct bce_queue_cmdq *bce_alloc_cmdq(struct apple_bce_softc *sc,
		    struct bce_queue_sq *sq);
void	bce_free_cmdq(struct bce_queue_cmdq *cmdq);
uint32_t bce_cmd_register_queue(struct bce_queue_cmdq *cmdq,
	    struct apple_bce_softc *sc, struct bce_queue_memcfg *cfg,
	    const char *name, int isdirout);
uint32_t bce_cmd_unregister_queue(struct bce_queue_cmdq *cmdq,
	    struct apple_bce_softc *sc, int qid);
uint32_t bce_cmd_flush_queue(struct bce_queue_cmdq *cmdq,
	    struct apple_bce_softc *sc, int qid);

#endif /* _APPLE_BCE_QUEUE_H_ */
