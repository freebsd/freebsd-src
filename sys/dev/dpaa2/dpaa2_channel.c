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

/*
 * QBMan channel to process ingress traffic (Rx, Tx confirmation, Rx error).
 *
 * NOTE: Several WQs are organized into a single channel.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/taskqueue.h>
#include <sys/sysctl.h>
#include <sys/buf_ring.h>
#include <sys/smp.h>
#include <sys/proc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/atomic.h>
#include <machine/vmparam.h>

#include <net/ethernet.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include "dpaa2_types.h"
#include "dpaa2_channel.h"
#include "dpaa2_ni.h"
#include "dpaa2_mc.h"
#include "dpaa2_mc_if.h"
#include "dpaa2_mcp.h"
#include "dpaa2_io.h"
#include "dpaa2_con.h"
#include "dpaa2_buf.h"
#include "dpaa2_swp.h"
#include "dpaa2_swp_if.h"
#include "dpaa2_bp.h"
#include "dpaa2_cmd_if.h"

MALLOC_DEFINE(M_DPAA2_CH, "dpaa2_ch", "DPAA2 QBMan Channel");

#define RX_SEG_N		 (1u)
#define RX_SEG_SZ		 (((MJUM9BYTES - 1) / PAGE_SIZE + 1) * PAGE_SIZE)
#define RX_SEG_MAXSZ	 	 (((MJUM9BYTES - 1) / PAGE_SIZE + 1) * PAGE_SIZE)
CTASSERT(RX_SEG_SZ % PAGE_SIZE == 0);
CTASSERT(RX_SEG_MAXSZ % PAGE_SIZE == 0);

#define TX_SEG_N		 (16u) /* XXX-DSL: does DPAA2 limit exist? */
#define TX_SEG_SZ		 (PAGE_SIZE)
#define TX_SEG_MAXSZ	 	 (TX_SEG_N * TX_SEG_SZ)
CTASSERT(TX_SEG_SZ % PAGE_SIZE == 0);
CTASSERT(TX_SEG_MAXSZ % PAGE_SIZE == 0);

#define SGT_SEG_N		 (1u)
#define SGT_SEG_SZ		 (PAGE_SIZE)
#define SGT_SEG_MAXSZ	 	 (PAGE_SIZE)
CTASSERT(SGT_SEG_SZ % PAGE_SIZE == 0);
CTASSERT(SGT_SEG_MAXSZ % PAGE_SIZE == 0);

static int dpaa2_chan_setup_dma(device_t, struct dpaa2_channel *, bus_size_t);
static int dpaa2_chan_alloc_storage(device_t, struct dpaa2_channel *, bus_size_t,
    int, bus_size_t);
static void dpaa2_chan_bp_task(void *, int);

/**
 * @brief Сonfigures QBMan channel and registers data availability notifications.
 */
int
dpaa2_chan_setup(device_t dev, device_t iodev, device_t condev, device_t bpdev,
    struct dpaa2_channel **channel, uint32_t flowid, task_fn_t cleanup_task_fn)
{
	device_t pdev = device_get_parent(dev);
	device_t child = dev;
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_io_softc *iosc = device_get_softc(iodev);
	struct dpaa2_con_softc *consc = device_get_softc(condev);
	struct dpaa2_devinfo *rcinfo = device_get_ivars(pdev);
	struct dpaa2_devinfo *ioinfo = device_get_ivars(iodev);
	struct dpaa2_devinfo *coninfo = device_get_ivars(condev);
	struct dpaa2_con_notif_cfg notif_cfg;
	struct dpaa2_io_notif_ctx *ctx;
	struct dpaa2_channel *ch = NULL;
	struct dpaa2_cmd cmd;
	uint16_t rctk, contk;
	int error;

	DPAA2_CMD_INIT(&cmd);

	error = DPAA2_CMD_RC_OPEN(dev, child, &cmd, rcinfo->id, &rctk);
	if (error) {
		device_printf(dev, "%s: failed to open DPRC: id=%d, error=%d\n",
		    __func__, rcinfo->id, error);
		goto fail_rc_open;
	}
	error = DPAA2_CMD_CON_OPEN(dev, child, &cmd, coninfo->id, &contk);
	if (error) {
		device_printf(dev, "%s: failed to open DPCON: id=%d, error=%d\n",
		    __func__, coninfo->id, error);
		goto fail_con_open;
	}

	error = DPAA2_CMD_CON_ENABLE(dev, child, &cmd);
	if (error) {
		device_printf(dev, "%s: failed to enable channel: dpcon_id=%d, "
		    "chan_id=%d\n", __func__, coninfo->id, consc->attr.chan_id);
		goto fail_con_enable;
	}

	ch = malloc(sizeof(struct dpaa2_channel), M_DPAA2_CH, M_WAITOK | M_ZERO);
	if (ch == NULL) {
		device_printf(dev, "%s: malloc() failed\n", __func__);
		error = ENOMEM;
		goto fail_malloc;
	}

	ch->ni_dev = dev;
	ch->io_dev = iodev;
	ch->con_dev = condev;
	ch->id = consc->attr.chan_id;
	ch->flowid = flowid;
	ch->tx_frames = 0; /* for debug purposes */
	ch->tx_dropped = 0; /* for debug purposes */
	ch->store_sz = 0;
	ch->store_idx = 0;
	ch->recycled_n = 0;
	ch->rxq_n = 0;

	NET_TASK_INIT(&ch->cleanup_task, 0, cleanup_task_fn, ch);
	NET_TASK_INIT(&ch->bp_task, 0, dpaa2_chan_bp_task, ch);

	ch->cleanup_tq = taskqueue_create("dpaa2_ch cleanup", M_WAITOK,
	    taskqueue_thread_enqueue, &ch->cleanup_tq);
	taskqueue_start_threads_cpuset(&ch->cleanup_tq, 1, PI_NET,
	    &iosc->cpu_mask, "dpaa2_ch%d cleanup", ch->id);

	error = dpaa2_chan_setup_dma(dev, ch, sc->buf_align);
	if (error != 0) {
		device_printf(dev, "%s: failed to setup DMA\n", __func__);
		goto fail_dma_setup;
	}

	mtx_init(&ch->xmit_mtx, "dpaa2_ch_xmit", NULL, MTX_DEF);

	ch->xmit_br = buf_ring_alloc(DPAA2_TX_BUFRING_SZ, M_DEVBUF, M_NOWAIT,
	    &ch->xmit_mtx);
	if (ch->xmit_br == NULL) {
		device_printf(dev, "%s: buf_ring_alloc() failed\n", __func__);
		error = ENOMEM;
		goto fail_buf_ring;
	}

	DPAA2_BUF_INIT(&ch->store);

	/* Register the new notification context */
	ctx = &ch->ctx;
	ctx->qman_ctx = (uint64_t)ctx;
	ctx->cdan_en = true;
	ctx->fq_chan_id = ch->id;
	ctx->io_dev = ch->io_dev;
	ctx->channel = ch;
	error = DPAA2_SWP_CONF_WQ_CHANNEL(ch->io_dev, ctx);
	if (error) {
		device_printf(dev, "%s: failed to register CDAN context\n",
		    __func__);
		goto fail_dpcon_notif;
	}

	/* Register DPCON notification within Management Complex */
	notif_cfg.dpio_id = ioinfo->id;
	notif_cfg.prior = 0;
	notif_cfg.qman_ctx = ctx->qman_ctx;
	error = DPAA2_CMD_CON_SET_NOTIF(dev, child, &cmd, &notif_cfg);
	if (error) {
		device_printf(dev, "%s: failed to register DPCON "
		    "notifications: dpcon_id=%d, chan_id=%d\n", __func__,
		    coninfo->id, consc->attr.chan_id);
		goto fail_dpcon_notif;
	}

	/* Allocate initial # of Rx buffers and a channel storage */
	error = dpaa2_buf_seed_pool(dev, bpdev, ch, DPAA2_NI_BUFS_INIT,
	    DPAA2_RX_BUF_SIZE, NULL);
	if (error) {
		device_printf(dev, "%s: failed to seed buffer pool\n",
		    __func__);
		goto fail_dpcon_notif;
	}
	error = dpaa2_chan_alloc_storage(dev, ch, DPAA2_ETH_STORE_SIZE,
	    BUS_DMA_NOWAIT, sc->buf_align);
	if (error != 0) {
		device_printf(dev, "%s: failed to allocate channel storage\n",
		    __func__);
		goto fail_dpcon_notif;
	} else {
		ch->store_sz = DPAA2_ETH_STORE_FRAMES;
	}

	/* Prepare queues for the channel */
	error = dpaa2_chan_setup_fq(dev, ch, DPAA2_NI_QUEUE_TX_CONF);
	if (error) {
		device_printf(dev, "%s: failed to prepare TxConf queue: "
		    "error=%d\n", __func__, error);
		goto fail_fq_setup;
	}
	error = dpaa2_chan_setup_fq(dev, ch, DPAA2_NI_QUEUE_RX);
	if (error) {
		device_printf(dev, "%s: failed to prepare Rx queue: error=%d\n",
		    __func__, error);
		goto fail_fq_setup;
	}

	if (bootverbose) {
		device_printf(dev, "channel: dpio_id=%d dpcon_id=%d chan_id=%d, "
		    "priorities=%d\n", ioinfo->id, coninfo->id, ch->id,
		    consc->attr.prior_num);
	}

	*channel = ch;

	(void)DPAA2_CMD_CON_CLOSE(dev, child, &cmd);
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rctk));

	return (0);

fail_fq_setup:
	if (ch->store.vaddr != NULL) {
		bus_dmamem_free(ch->store.dmat, ch->store.vaddr, ch->store.dmap);
	}
	if (ch->store.dmat != NULL) {
		bus_dma_tag_destroy(ch->store.dmat);
	}
	ch->store.dmat = NULL;
	ch->store.vaddr = NULL;
	ch->store.paddr = 0;
	ch->store.nseg = 0;
fail_dpcon_notif:
	buf_ring_free(ch->xmit_br, M_DEVBUF);
fail_buf_ring:
	mtx_destroy(&ch->xmit_mtx);
fail_dma_setup:
	/* while (taskqueue_cancel(ch->cleanup_tq, &ch->cleanup_task, NULL)) { */
	/* 	taskqueue_drain(ch->cleanup_tq, &ch->cleanup_task); */
	/* } */
	/* taskqueue_free(ch->cleanup_tq); */
fail_malloc:
	(void)DPAA2_CMD_CON_DISABLE(dev, child, DPAA2_CMD_TK(&cmd, contk));
fail_con_enable:
	(void)DPAA2_CMD_CON_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, contk));
fail_con_open:
	(void)DPAA2_CMD_RC_CLOSE(dev, child, DPAA2_CMD_TK(&cmd, rctk));
fail_rc_open:
	return (error);
}

/**
 * @brief Performs an initial configuration of the frame queue.
 */
int
dpaa2_chan_setup_fq(device_t dev, struct dpaa2_channel *ch,
    enum dpaa2_ni_queue_type queue_type)
{
	struct dpaa2_ni_softc *sc = device_get_softc(dev);
	struct dpaa2_ni_fq *fq;

	switch (queue_type) {
	case DPAA2_NI_QUEUE_TX_CONF:
		/* One queue per channel */
		fq = &ch->txc_queue;
		fq->chan = ch;
		fq->flowid = ch->flowid;
		fq->tc = 0; /* ignored */
		fq->type = queue_type;
		break;
	case DPAA2_NI_QUEUE_RX:
		KASSERT(sc->attr.num.rx_tcs <= DPAA2_MAX_TCS,
		    ("too many Rx traffic classes: rx_tcs=%d\n",
		    sc->attr.num.rx_tcs));

		/* One queue per Rx traffic class within a channel */
		for (int i = 0; i < sc->attr.num.rx_tcs; i++) {
			fq = &ch->rx_queues[i];
			fq->chan = ch;
			fq->flowid = ch->flowid;
			fq->tc = (uint8_t) i;
			fq->type = queue_type;

			ch->rxq_n++;
		}
		break;
	case DPAA2_NI_QUEUE_RX_ERR:
		/* One queue per network interface */
		fq = &sc->rxe_queue;
		fq->chan = ch;
		fq->flowid = 0; /* ignored */
		fq->tc = 0; /* ignored */
		fq->type = queue_type;
		break;
	default:
		device_printf(dev, "%s: unexpected frame queue type: %d\n",
		    __func__, queue_type);
		return (EINVAL);
	}

	return (0);
}

/**
 * @brief Obtain the next dequeue response from the channel storage.
 */
int
dpaa2_chan_next_frame(struct dpaa2_channel *ch, struct dpaa2_dq **dq)
{
	struct dpaa2_buf *buf = &ch->store;
	struct dpaa2_dq *msgs = (struct dpaa2_dq *)buf->vaddr;
	struct dpaa2_dq *msg = &msgs[ch->store_idx];
	int rc = EINPROGRESS;

	ch->store_idx++;

	if (msg->fdr.desc.stat & DPAA2_DQ_STAT_EXPIRED) {
		rc = EALREADY; /* VDQ command is expired */
		ch->store_idx = 0;
		if (!(msg->fdr.desc.stat & DPAA2_DQ_STAT_VALIDFRAME)) {
			msg = NULL; /* Null response, FD is invalid */
		}
	}
	if (msg != NULL && (msg->fdr.desc.stat & DPAA2_DQ_STAT_FQEMPTY)) {
		rc = ENOENT; /* FQ is empty */
		ch->store_idx = 0;
	}

	if (dq != NULL) {
		*dq = msg;
	}

	return (rc);
}

static int
dpaa2_chan_setup_dma(device_t dev, struct dpaa2_channel *ch,
    bus_size_t alignment)
{
	int error;

	mtx_init(&ch->dma_mtx, "dpaa2_ch_dma_mtx", NULL, MTX_DEF);

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* parent */
	    alignment, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* low restricted addr */
	    BUS_SPACE_MAXADDR,		/* high restricted addr */
	    NULL, NULL,			/* filter, filterarg */
	    RX_SEG_MAXSZ,		/* maxsize */
	    RX_SEG_N,			/* nsegments */
	    RX_SEG_SZ,			/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &ch->rx_dmat);
	if (error) {
		device_printf(dev, "%s: failed to create rx_dmat\n", __func__);
		goto fail_rx_tag;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* parent */
	    alignment, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* low restricted addr */
	    BUS_SPACE_MAXADDR,		/* high restricted addr */
	    NULL, NULL,			/* filter, filterarg */
	    TX_SEG_MAXSZ,		/* maxsize */
	    TX_SEG_N,			/* nsegments */
	    TX_SEG_SZ,			/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &ch->tx_dmat);
	if (error) {
		device_printf(dev, "%s: failed to create tx_dmat\n", __func__);
		goto fail_tx_tag;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* parent */
	    alignment, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* low restricted addr */
	    BUS_SPACE_MAXADDR,		/* high restricted addr */
	    NULL, NULL,			/* filter, filterarg */
	    SGT_SEG_MAXSZ,		/* maxsize */
	    SGT_SEG_N,			/* nsegments */
	    SGT_SEG_SZ,			/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &ch->sgt_dmat);
	if (error) {
		device_printf(dev, "%s: failed to create sgt_dmat\n", __func__);
		goto fail_sgt_tag;
	}

	return (0);

fail_sgt_tag:
	bus_dma_tag_destroy(ch->tx_dmat);
fail_tx_tag:
	bus_dma_tag_destroy(ch->rx_dmat);
fail_rx_tag:
	mtx_destroy(&ch->dma_mtx);
	ch->rx_dmat = NULL;
	ch->tx_dmat = NULL;
	ch->sgt_dmat = NULL;

	return (error);
}

/**
 * @brief Allocate a DMA-mapped storage to keep responses from VDQ command.
 */
static int
dpaa2_chan_alloc_storage(device_t dev, struct dpaa2_channel *ch, bus_size_t size,
    int mapflags, bus_size_t alignment)
{
	struct dpaa2_buf *buf = &ch->store;
	uint32_t maxsize = ((size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;
	int error;

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* parent */
	    alignment, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* low restricted addr */
	    BUS_SPACE_MAXADDR,		/* high restricted addr */
	    NULL, NULL,			/* filter, filterarg */
	    maxsize,			/* maxsize */
	    1,				/* nsegments */
	    maxsize,			/* maxsegsize */
	    BUS_DMA_ALLOCNOW,		/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &buf->dmat);
	if (error != 0) {
		device_printf(dev, "%s: failed to create DMA tag\n", __func__);
		goto fail_tag;
	}

	error = bus_dmamem_alloc(buf->dmat, (void **)&buf->vaddr,
	    BUS_DMA_ZERO | BUS_DMA_COHERENT, &buf->dmap);
	if (error != 0) {
		device_printf(dev, "%s: failed to allocate storage memory\n",
		    __func__);
		goto fail_map_create;
	}

	buf->paddr = 0;
	error = bus_dmamap_load(buf->dmat, buf->dmap, buf->vaddr, size,
	    dpaa2_dmamap_oneseg_cb, &buf->paddr, mapflags);
	if (error != 0) {
		device_printf(dev, "%s: failed to map storage memory\n",
		    __func__);
		goto fail_map_load;
	}

	bus_dmamap_sync(buf->dmat, buf->dmap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	buf->nseg = 1;

	return (0);

fail_map_load:
	bus_dmamem_free(buf->dmat, buf->vaddr, buf->dmap);
fail_map_create:
	bus_dma_tag_destroy(buf->dmat);
fail_tag:
	buf->dmat = NULL;
	buf->vaddr = NULL;
	buf->paddr = 0;
	buf->nseg = 0;

	return (error);
}

/**
 * @brief Release new buffers to the buffer pool if necessary.
 */
static void
dpaa2_chan_bp_task(void *arg, int count)
{
	struct dpaa2_channel *ch = (struct dpaa2_channel *)arg;
	struct dpaa2_ni_softc *sc = device_get_softc(ch->ni_dev);
	struct dpaa2_bp_softc *bpsc;
	struct dpaa2_bp_conf bpconf;
	const int buf_num = DPAA2_ATOMIC_READ(&sc->buf_num);
	device_t bpdev;
	int error;

	/* There's only one buffer pool for now */
	bpdev = (device_t)rman_get_start(sc->res[DPAA2_NI_BP_RID(0)]);
	bpsc = device_get_softc(bpdev);

	/* Get state of the buffer pool */
	error = DPAA2_SWP_QUERY_BP(ch->io_dev, bpsc->attr.bpid, &bpconf);
	if (error) {
		device_printf(sc->dev, "%s: DPAA2_SWP_QUERY_BP() failed: "
		    "error=%d\n", __func__, error);
		return;
	}

	/* Double allocated Rx buffers if amount of free buffers is < 25% */
	if (bpconf.free_bufn < (buf_num >> 2)) {
		mtx_assert(&ch->dma_mtx, MA_NOTOWNED);
		mtx_lock(&ch->dma_mtx);
		(void)dpaa2_buf_seed_pool(ch->ni_dev, bpdev, ch, buf_num,
		    DPAA2_RX_BUF_SIZE, &ch->dma_mtx);
		mtx_unlock(&ch->dma_mtx);

		DPAA2_ATOMIC_XCHG(&sc->buf_free, bpconf.free_bufn);
	}
}
