/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2023 Dmitry Salychev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_DPAA2_CHANNEL_H
#define	_DPAA2_CHANNEL_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/buf_ring.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include "dpaa2_types.h"
#include "dpaa2_io.h"
#include "dpaa2_ni.h"

#define DPAA2_TX_BUFRING_SZ	 (4096u)

/**
 * @brief QBMan channel to process ingress traffic.
 *
 * NOTE: Several WQs are organized into a single channel.
 */
struct dpaa2_channel {
	device_t		 ni_dev;
	device_t		 io_dev;
	device_t		 con_dev;
	uint16_t		 id;
	uint16_t		 flowid;

	uint64_t		 tx_frames;
	uint64_t		 tx_dropped;

	struct mtx		 dma_mtx;
	bus_dma_tag_t		 rx_dmat;
	bus_dma_tag_t		 tx_dmat;
	bus_dma_tag_t		 sgt_dmat;

	struct dpaa2_io_notif_ctx ctx;		/* to configure CDANs */

	struct dpaa2_buf	 store;		/* to keep VDQ responses */
	uint32_t		 store_sz;	/* in frames */
	uint32_t		 store_idx;	/* frame index */

	uint32_t		 recycled_n;
	struct dpaa2_buf	*recycled[DPAA2_SWP_BUFS_PER_CMD];

	uint32_t		 rxq_n;
	struct dpaa2_ni_fq	 rx_queues[DPAA2_MAX_TCS];
	struct dpaa2_ni_fq	 txc_queue;

	struct taskqueue	*cleanup_tq;
	struct task		 cleanup_task;
	struct task		 bp_task;

	struct mtx		 xmit_mtx;
	struct buf_ring		*xmit_br;
} __aligned(CACHE_LINE_SIZE);

int dpaa2_chan_setup(device_t, device_t, device_t, device_t,
    struct dpaa2_channel **, uint32_t, task_fn_t);
int dpaa2_chan_setup_fq(device_t, struct dpaa2_channel *,
    enum dpaa2_ni_queue_type);
int dpaa2_chan_next_frame(struct dpaa2_channel *, struct dpaa2_dq **);

#endif /* _DPAA2_CHANNEL_H */
