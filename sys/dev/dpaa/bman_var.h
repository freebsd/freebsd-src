/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	BMAN_VAR_H
#define	BMAN_VAR_H

#include "dpaa_common.h"
#include "portals.h"

#define	BMAN_MAX_POOLS	64
#define	BMAN_MAX_POOLS_1023	8

DPAA_RING_DECLARE(bman_rcr);

struct bman_mc {
	uint8_t polarity;
	bool busy;
};

struct bman_portal_softc {
	struct dpaa_portal_softc sc_base;

	struct bman_mc mc;
	struct bman_rcr_ring sc_rcr;
	struct bman_pool *sc_pools[BMAN_MAX_POOLS];
};

struct bman_pool {
	uint32_t bpid;
	bm_depletion_handler dep_cb;
	void *arg;
};

DPCPU_DECLARE(struct bman_portal_softc *, bman_affine_portal);

int bman_release(struct bman_pool *pool, const struct bman_buffer *bufs,
    uint8_t count);

void bman_portal_enable_scn(struct bman_portal_softc *, struct bman_pool *);

#endif
