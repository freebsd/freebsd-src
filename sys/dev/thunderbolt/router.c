/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Scott Long
 * All rights reserved.
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

#include "opt_thunderbolt.h"

/* Config space access for switches, ports, and devices in TB3 and USB4 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <sys/gsb_crc32.h>
#include <sys/endian.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/stdarg.h>

#include <dev/thunderbolt/nhi_reg.h>
#include <dev/thunderbolt/nhi_var.h>
#include <dev/thunderbolt/tb_reg.h>
#include <dev/thunderbolt/tb_var.h>
#include <dev/thunderbolt/tbcfg_reg.h>
#include <dev/thunderbolt/router_var.h>
#include <dev/thunderbolt/tb_debug.h>

static int router_alloc_cmd(struct router_softc *, struct router_command **);
static void router_free_cmd(struct router_softc *, struct router_command *);
static int _tb_router_attach(struct router_softc *);
static void router_prepare_read(struct router_softc *, struct router_command *,
    int);
static int _tb_config_read(struct router_softc *, u_int, u_int, u_int, u_int,
    uint32_t *, void *, struct router_command **);
static int router_schedule(struct router_softc *, struct router_command *);
static int router_schedule_locked(struct router_softc *,
    struct router_command *);
static nhi_ring_cb_t router_complete_intr;
static nhi_ring_cb_t router_response_intr;
static nhi_ring_cb_t router_notify_intr;

#define CFG_DEFAULT_RETRIES	3
#define CFG_DEFAULT_TIMEOUT	2

static int
router_lookup_device(struct router_softc *sc, tb_route_t route,
    struct router_softc **dev)
{
	struct router_softc *cursor;
	uint64_t search_rt, remainder_rt, this_rt;
	uint8_t hop;

	KASSERT(dev != NULL, ("dev cannot be NULL\n"));

	cursor = tb_config_get_root(sc);
	remainder_rt = search_rt = route.lo | ((uint64_t)route.hi << 32);
	tb_debug(sc, DBG_ROUTER|DBG_EXTRA,
	    "%s: Searching for router 0x%016jx\n", __func__, search_rt);

	while (cursor != NULL) {
		this_rt = TB_ROUTE(cursor);
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA,
		    "Comparing cursor route 0x%016jx\n", this_rt);
		if (this_rt == search_rt)
			break;

		/* Prepare to go to the next hop node in the route */
		hop = remainder_rt & 0xff;
		remainder_rt >>= 8;
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA,
		    "hop= 0x%02x, remainder= 0x%016jx\n", hop, remainder_rt);

		/*
		 * An adapter index of 0x0 is only for the host interface
		 * adapter on the root route.  The only time that
		 * it's valid for searches is when you're looking for the
		 * root route, and that case has already been handled.
		 */
		if (hop == 0) {
			tb_debug(sc, DBG_ROUTER,
			    "End of route chain, route not found\n");
			return (ENOENT);
		}

		if (hop > cursor->max_adap) {
			tb_debug(sc, DBG_ROUTER,
			    "Route hop out of range for parent\n");
			return (EINVAL);
		}

		if (cursor->adapters == NULL) {
			tb_debug(sc, DBG_ROUTER,
			    "Error, router not fully initialized\n");
			return (EINVAL);
		}

		cursor = cursor->adapters[hop];
	}

	if (cursor == NULL)
		return (ENOENT);

	*dev = cursor;
	return (0);
}

static int
router_insert(struct router_softc *sc, struct router_softc *parent)
{
	uint64_t this_rt;
	uint8_t this_hop;

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_insert called\n");

	if (parent == NULL) {
		tb_debug(sc, DBG_ROUTER, "Parent cannot be NULL in insert\n");
		return (EINVAL);
	}

	this_rt = TB_ROUTE(sc);
	if (((this_rt >> (sc->depth * 8)) > 0xffULL) ||
	    (parent->depth + 1 != sc->depth)) {
		tb_debug(sc, DBG_ROUTER, "Added route 0x%08x%08x is not a "
		    "direct child of the parent route 0x%08x%08x\n",
		    sc->route.hi, sc->route.lo, parent->route.hi,
		    parent->route.lo);
		return (EINVAL);
	}

	this_hop = (uint8_t)(this_rt >> (sc->depth * 8));

	tb_debug(sc, DBG_ROUTER, "Inserting route 0x%08x%08x with last hop "
	    "of 0x%02x and depth of %d\n", sc->route.hi, sc->route.lo,
	    this_hop, sc->depth);

	if (this_hop > parent->max_adap) {
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA,
		    "Inserted route is out of range of the parent\n");
		return (EINVAL);
	}

	if (parent->adapters[this_hop] != NULL) {
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA,
		    "Inserted route already exists\n");
		return (EEXIST);
	}

	parent->adapters[this_hop] = sc;

	tb_debug(sc, DBG_ROUTER, "Added router 0x%08x%08x to parent "
	    "0x%08x%08x\n", sc->route.hi, sc->route.lo, parent->route.hi,
	    parent->route.lo);
	return (0);
}

static int
router_register_interrupts(struct router_softc *sc)
{
	struct nhi_dispatch tx[] = { { PDF_READ, router_complete_intr, sc },
				     { PDF_WRITE, router_complete_intr, sc },
				     { 0, NULL, NULL } };
	struct nhi_dispatch rx[] = { { PDF_READ, router_response_intr, sc },
				     { PDF_WRITE, router_response_intr, sc },
				     { PDF_NOTIFY, router_notify_intr, sc },
				     { 0, NULL, NULL } };

	return (nhi_register_pdf(sc->ring0, tx, rx));
}

int
tb_router_attach(struct router_softc *parent, tb_route_t route)
{
	struct router_softc *sc;

	tb_debug(parent, DBG_ROUTER|DBG_EXTRA, "tb_router_attach called\n");

	sc = malloc(sizeof(*sc), M_THUNDERBOLT, M_ZERO|M_NOWAIT);
	if (sc == NULL) {
		tb_debug(parent, DBG_ROUTER, "Cannot allocate root router\n");
		return (ENOMEM);
	}

	sc->dev = parent->dev;
	sc->debug = parent->debug;
	sc->ring0 = parent->ring0;
	sc->route = route;
	sc->nsc = parent->nsc;

	mtx_init(&sc->mtx, "tbcfg", "Thunderbolt Router Config", MTX_DEF);
	TAILQ_INIT(&sc->cmd_queue);

	router_insert(sc, parent);

	return (_tb_router_attach(sc));
}

int
tb_router_attach_root(struct nhi_softc *nsc, tb_route_t route)
{
	struct router_softc *sc;
	int error;

	tb_debug(nsc, DBG_ROUTER|DBG_EXTRA, "tb_router_attach_root called\n");

	sc = malloc(sizeof(*sc), M_THUNDERBOLT, M_ZERO|M_NOWAIT);
	if (sc == NULL) {
		tb_debug(nsc, DBG_ROUTER, "Cannot allocate root router\n");
		return (ENOMEM);
	}

	sc->dev = nsc->dev;
	sc->debug = nsc->debug;
	sc->ring0 = nsc->ring0;
	sc->route = route;
	sc->nsc = nsc;

	mtx_init(&sc->mtx, "tbcfg", "Thunderbolt Router Config", MTX_DEF);
	TAILQ_INIT(&sc->cmd_queue);

	/*
	 * This router is semi-virtual and represents the router that's part
	 * of the NHI DMA engine.  Commands can't be issued to the topology
	 * until the NHI is initialized and this router is initialized, so
	 * there's no point in registering router interrupts earlier than this,
	 * even if other routers are found first.
	 */
	tb_config_set_root(sc);
	error = router_register_interrupts(sc);
	if (error) {
		tb_router_detach(sc);
		return (error);
	}

	error = _tb_router_attach(sc);
	if (error)
		return (error);

	bcopy((uint8_t *)sc->uuid, nsc->uuid, 16);
	return (0);
}

static int
_tb_router_attach(struct router_softc *sc)
{
	struct tb_cfg_router *cfg;
	uint32_t *buf;
	int error, up;

	buf = malloc(9 * 4, M_THUNDERBOLT, M_NOWAIT|M_ZERO);
	if (buf == NULL)
		return (ENOMEM);

	error = tb_config_router_read_polled(sc, 0, 9, buf);
	if (error != 0) {
		free(buf, M_THUNDERBOLT);
		return (error);
	}

	cfg = (struct tb_cfg_router *)buf;
	up = GET_ROUTER_CS_UPSTREAM_ADAP(cfg);
	sc->max_adap = GET_ROUTER_CS_MAX_ADAP(cfg);
	sc->depth = GET_ROUTER_CS_DEPTH(cfg);
	sc->uuid[0] = cfg->uuid_lo;
	sc->uuid[1] = cfg->uuid_hi;
	sc->uuid[2] = 0xffffffff;
	sc->uuid[3] = 0xffffffff;
	tb_debug(sc, DBG_ROUTER, "Router upstream_port= %d, max_port= %d, "
	    "depth= %d\n", up, sc->max_adap, sc->depth);
	free(buf, M_THUNDERBOLT);

	/* Downstream adapters are indexed in the array allocated here. */
	sc->max_adap = MIN(sc->max_adap, ROUTER_CS1_MAX_ADAPTERS);
	sc->adapters = malloc((1 + sc->max_adap) * sizeof(void *),
	    M_THUNDERBOLT, M_NOWAIT|M_ZERO);
	if (sc->adapters == NULL) {
		tb_debug(sc, DBG_ROUTER,
		    "Cannot allocate downstream adapter memory\n");
		return (ENOMEM);
	}

	tb_debug(sc, DBG_ROUTER, "Router created, route 0x%08x%08x\n",
	    sc->route.hi, sc->route.lo);

	return (0);
}

int
tb_router_detach(struct router_softc *sc)
{

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "tb_router_deattach called\n");

	if (TAILQ_FIRST(&sc->cmd_queue) != NULL)
		return (EBUSY);

	mtx_destroy(&sc->mtx);

	if (sc->adapters != NULL)
		free(sc->adapters, M_THUNDERBOLT);

	if (sc != NULL)
		free(sc, M_THUNDERBOLT);

	return (0);
}

static void
router_get_config_cb(struct router_softc *sc, struct router_command *cmd,
    void *arg)
{
	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_get_config_cb called\n");

	/*
	 * Only do the copy if the command didn't have a notify event thrown.
	 * These events serve as asynchronous exception signals, which is
	 * cumbersome.
	 */
	if (cmd->ev == 0)
		bcopy((uint8_t *)cmd->resp_buffer,
		    (uint8_t *)cmd->callback_arg, cmd->dwlen * 4);

	mtx_lock(&sc->mtx);
	sc->inflight_cmd = NULL;

	if ((cmd->flags & RCMD_POLLED) == 0)
		wakeup(cmd);
	else
		cmd->flags |= RCMD_POLL_COMPLETE;

	router_schedule_locked(sc, NULL);
	mtx_unlock(&sc->mtx);
}

int
tb_config_read(struct router_softc *sc, u_int space, u_int adapter,
    u_int offset, u_int dwlen, uint32_t *buf)
{
	struct router_command *cmd;
	int error, retries;

	if ((error = _tb_config_read(sc, space, adapter, offset, dwlen, buf,
	    router_get_config_cb, &cmd)) != 0)
		return (error);

	retries = cmd->retries;
	mtx_lock(&sc->mtx);
	while (retries-- >= 0) {
		error = router_schedule_locked(sc, cmd);
		if (error)
			break;

		error = msleep(cmd, &sc->mtx, 0, "tbtcfg", cmd->timeout * hz);
		if (error != EWOULDBLOCK)
			break;
		sc->inflight_cmd = NULL;
		tb_debug(sc, DBG_ROUTER, "Config command timed out, retries=%d\n", retries);
	}

	if (cmd->ev != 0)
		error = EINVAL;
	router_free_cmd(sc, cmd);
	mtx_unlock(&sc->mtx);
	return (error);
}

int
tb_config_read_polled(struct router_softc *sc, u_int space, u_int adapter,
    u_int offset, u_int dwlen, uint32_t *buf)
{
	struct router_command *cmd;
	int error, retries, timeout;

	if ((error = _tb_config_read(sc, space, adapter, offset, dwlen, buf,
	    router_get_config_cb, &cmd)) != 0)
		return (error);

	retries = cmd->retries;
	cmd->flags |= RCMD_POLLED;
	timeout = cmd->timeout * 1000000;

	mtx_lock(&sc->mtx);
	while (retries-- >= 0) {
		error = router_schedule_locked(sc, cmd);
		if (error)
			break;
		mtx_unlock(&sc->mtx);

		while (timeout > 0) {
			DELAY(100 * 1000);
			if ((cmd->flags & RCMD_POLL_COMPLETE) != 0)
				break;
			timeout -= 100000;
		}

		mtx_lock(&sc->mtx);
		if ((cmd->flags & RCMD_POLL_COMPLETE) == 0) {
			error = ETIMEDOUT;
			sc->inflight_cmd = NULL;
			tb_debug(sc, DBG_ROUTER, "Config command timed out, retries=%d\n", retries);
			continue;
		} else
			break;
	}

	if (cmd->ev != 0)
		error = EINVAL;
	router_free_cmd(sc, cmd);
	mtx_unlock(&sc->mtx);
	return (error);
}

int
tb_config_read_async(struct router_softc *sc, u_int space, u_int adapter,
    u_int offset, u_int dwlen, uint32_t *buf, void *cb)
{
	struct router_command *cmd;
	int error;

	if ((error = _tb_config_read(sc, space, adapter, offset, dwlen, buf,
	    cb, &cmd)) != 0)
		return (error);

	error = router_schedule(sc, cmd);

	return (error);
}

static int
_tb_config_read(struct router_softc *sc, u_int space, u_int adapter,
    u_int offset, u_int dwlen, uint32_t *buf, void *cb,
    struct router_command **rcmd)
{
	struct router_command *cmd;
	struct tb_cfg_read *msg;
	int error;

	if ((error = router_alloc_cmd(sc, &cmd)) != 0)
		return (error);

	msg = router_get_frame_data(cmd);
	bzero(msg, sizeof(*msg));
	msg->route.hi = sc->route.hi;
	msg->route.lo = sc->route.lo;
	msg->addr_attrs = TB_CONFIG_ADDR(0, space, adapter, dwlen, offset);
	cmd->callback = cb;
	cmd->callback_arg = buf;
	cmd->dwlen = dwlen;
	router_prepare_read(sc, cmd, sizeof(*msg));

	if (rcmd != NULL)
		*rcmd = cmd;

	return (0);
}

int
tb_config_write(struct router_softc *sc, u_int space, u_int adapter,
    u_int offset, u_int dwlen, uint32_t *buf)
{

	return(0);
}

static int
router_alloc_cmd(struct router_softc *sc, struct router_command **rcmd)
{
	struct router_command *cmd;

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_alloc_cmd\n");

	cmd = malloc(sizeof(*cmd), M_THUNDERBOLT, M_ZERO|M_NOWAIT);
	if (cmd == NULL) {
		tb_debug(sc, DBG_ROUTER, "Cannot allocate cmd/response\n");
		return (ENOMEM);
	}

	cmd->nhicmd = nhi_alloc_tx_frame(sc->ring0);
	if (cmd->nhicmd == NULL) {
		tb_debug(sc, DBG_ROUTER, "Cannot allocate command frame\n");
		free(cmd, M_THUNDERBOLT);
		return (EBUSY);
	}

	cmd->sc = sc;
	*rcmd = cmd;
	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "Allocated command with index %d\n",
	    cmd->nhicmd->idx);

	return (0);
}

static void
router_free_cmd(struct router_softc *sc, struct router_command *cmd)
{

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_free_cmd\n");

	if (cmd == NULL)
		return;

	if (cmd->nhicmd != NULL) {
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "Freeing nhi command %d\n",
		    cmd->nhicmd->idx);
		nhi_free_tx_frame(sc->ring0, cmd->nhicmd);
	}
	free(cmd, M_THUNDERBOLT);

	return;
}

static void
router_prepare_read(struct router_softc *sc, struct router_command *cmd,
    int len)
{
	struct nhi_cmd_frame *nhicmd;
	uint32_t *msg;
	int msglen, i;

	KASSERT(cmd != NULL, ("cmd cannot be NULL\n"));
	KASSERT(len != 0, ("Invalid zero-length command\n"));
	KASSERT(len % 4 == 0, ("Message must be 32bit padded\n"));

	nhicmd = cmd->nhicmd;
	msglen = (len - 4) / 4;
	for (i = 0; i < msglen; i++)
		nhicmd->data[i] = htobe32(nhicmd->data[i]);

	msg = (uint32_t *)nhicmd->data;
	msg[msglen] = htobe32(tb_calc_crc(nhicmd->data, len-4));

	nhicmd->pdf = PDF_READ;
	nhicmd->req_len = len;

	nhicmd->timeout = NHI_CMD_TIMEOUT;
	nhicmd->retries = 0;
	nhicmd->resp_buffer = (uint32_t *)cmd->resp_buffer;
	nhicmd->resp_len = (cmd->dwlen + 3) * 4;
	nhicmd->context = cmd;

	cmd->retries = CFG_DEFAULT_RETRIES;
	cmd->timeout = CFG_DEFAULT_TIMEOUT;

	return;
}

static int
router_schedule(struct router_softc *sc, struct router_command *cmd)
{
	int error;

	mtx_lock(&sc->mtx);
	error = router_schedule_locked(sc, cmd);
	mtx_unlock(&sc->mtx);

	return(error);
}

static int
router_schedule_locked(struct router_softc *sc, struct router_command *cmd)
{
	struct nhi_cmd_frame *nhicmd;
	int error;

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_schedule\n");

	if (cmd != NULL)
		TAILQ_INSERT_TAIL(&sc->cmd_queue, cmd, link);

	while ((sc->inflight_cmd == NULL) &&
	    ((cmd = TAILQ_FIRST(&sc->cmd_queue)) != NULL)) {

		TAILQ_REMOVE(&sc->cmd_queue, cmd, link);
		nhicmd = cmd->nhicmd;
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA,
		    "Scheduling command with index %d\n", nhicmd->idx);
		sc->inflight_cmd = cmd;
		if ((error = nhi_tx_schedule(sc->ring0, nhicmd)) != 0) {
			tb_debug(sc, DBG_ROUTER, "nhi ring error "
			    "%d\n", error);
			sc->inflight_cmd = NULL;
			if (error == EBUSY) {
				TAILQ_INSERT_HEAD(&sc->cmd_queue, cmd, link);
				error = 0;
			}
			break;
		}
	}

	return (error);
}

static void
router_complete_intr(void *context, union nhi_ring_desc *ring,
    struct nhi_cmd_frame *nhicmd)
{
	struct router_softc *sc;
	struct router_command *cmd;

	KASSERT(context != NULL, ("context cannot be NULL\n"));
	KASSERT(nhicmd != NULL, ("nhicmd cannot be NULL\n"));

	cmd = (struct router_command *)(nhicmd->context);
	sc = cmd->sc;
	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_complete_intr called\n");

	if (nhicmd->flags & CMD_RESP_COMPLETE) {
		cmd->callback(sc, cmd, cmd->callback_arg);
	}

	return;
}

static void
router_response_intr(void *context, union nhi_ring_desc *ring, struct nhi_cmd_frame *nhicmd)
{
	struct router_softc *sc, *dev;
	struct tb_cfg_read_resp *read;
	struct tb_cfg_write_resp *write;
	struct router_command *cmd;
	tb_route_t route;
	u_int error, i, eof, len;
	uint32_t attrs;

	KASSERT(context != NULL, ("context cannot be NULL\n"));

	sc = (struct router_softc *)context;
	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_response_intr called\n");

	eof = ring->rxpost.eof_len >> RX_BUFFER_DESC_EOF_SHIFT;

	if (eof == PDF_WRITE) {
		write = (struct tb_cfg_write_resp *)nhicmd->data;
		route.hi = be32toh(write->route.hi);
		route.lo = be32toh(write->route.lo);
	} else {
		read = (struct tb_cfg_read_resp *)nhicmd->data;
		route.hi = be32toh(read->route.hi);
		route.lo = be32toh(read->route.lo);
		attrs = be32toh(read->addr_attrs);
		len = (attrs & TB_CFG_SIZE_MASK) >> TB_CFG_SIZE_SHIFT;
	}

	/* XXX Is this a problem? */
	if ((route.hi & 0x80000000) == 0)
		tb_debug(sc, DBG_ROUTER, "Invalid route\n");
	route.hi &= ~0x80000000;

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "Looking up route 0x%08x%08x\n",
	    route.hi, route.lo);

	error = router_lookup_device(sc, route, &dev);
	if (error != 0 || dev == NULL) {
		tb_debug(sc, DBG_ROUTER, "Cannot find device, error= %d\n",
		    error);
		return;
	}

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "Found device %s route 0x%08x%08x, "
	    "inflight_cmd= %p\n", device_get_nameunit(dev->dev), dev->route.hi,
	    dev->route.lo, dev->inflight_cmd);

	cmd = dev->inflight_cmd;
	if (cmd == NULL) {
		tb_debug(dev, DBG_ROUTER, "Null inflight cmd\n");
		return;
	}

	if (eof == PDF_READ) {
		for (i = 0; i < len; i++)
			cmd->nhicmd->resp_buffer[i] = be32toh(read->data[i]);
	}

	cmd->nhicmd->flags |= CMD_RESP_COMPLETE;
	if (cmd->nhicmd->flags & CMD_REQ_COMPLETE) {
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "TX_COMPLETE set\n");
		cmd->callback(dev, cmd, cmd->callback_arg);
	}

	return;
}

static void
router_notify_intr(void *context, union nhi_ring_desc *ring, struct nhi_cmd_frame *nhicmd)
{
	struct router_softc *sc;
	struct router_command *cmd;
	struct tb_cfg_notify event;
	u_int ev, adap;

	KASSERT(context != NULL, ("context cannot be NULL\n"));

	sc = (struct router_softc *)context;
	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "router_notify_intr called\n");

	event.route.hi = be32toh(nhicmd->data[0]);
	event.route.lo = be32toh(nhicmd->data[1]);
	event.event_adap = be32toh(nhicmd->data[2]);

	ev = GET_NOTIFY_EVENT(&event);
	adap = GET_NOTIFY_ADAPTER(&event);

	tb_debug(sc, DBG_ROUTER, "Event route 0x%08x%08x adap %d code %s\n",
	    event.route.hi, event.route.lo, adap,
	    tb_get_string(ev, tb_notify_event));

	switch (ev) {
	case TB_CFG_ERR_CONN:
	case TB_CFG_ERR_LINK:
	case TB_CFG_ERR_ADDR:
	case TB_CFG_ERR_ADP:
	case TB_CFG_ERR_ENUM:
	case TB_CFG_ERR_NUA:
	case TB_CFG_ERR_LEN:
	case TB_CFG_ERR_HEC:
	case TB_CFG_ERR_FC:
	case TB_CFG_ERR_PLUG:
	case TB_CFG_ERR_LOCK:
	case TB_CFG_HP_ACK:
	case TB_CFG_DP_BW:
		if (sc->inflight_cmd != NULL) {
			cmd = sc->inflight_cmd;
			cmd->ev = ev;
			cmd->callback(sc, cmd, cmd->callback_arg);
		}
		break;
	default:
		break;
	}
	return;
}

int
tb_config_next_cap(struct router_softc *sc, struct router_cfg_cap *cap)
{
	union tb_cfg_cap *tbcap;
	uint32_t *buf;
	uint16_t current;
	int error;

	KASSERT(cap != NULL, ("cap cannot be NULL\n"));
	KASSERT(cap->next_cap != 0, ("next_cap cannot be 0\n"));

	buf = malloc(sizeof(*tbcap), M_THUNDERBOLT, M_NOWAIT|M_ZERO);

	current = cap->next_cap;
	error = tb_config_read(sc, cap->space, cap->adap, current, 1, buf);
	if (error)
		return (error);

	tbcap = (union tb_cfg_cap *)buf;
	cap->cap_id = tbcap->hdr.cap_id;
	cap->next_cap = tbcap->hdr.next_cap;
	cap->current_cap = current;

	if ((cap->space != TB_CFG_CS_ROUTER) &&
	    (tbcap->hdr.cap_id != TB_CFG_CAP_VSC)) {
		free(buf, M_THUNDERBOLT);
		return (0);
	}

	tb_config_read(sc, cap->space, cap->adap, current, 2, buf);
	if (error) {
		free(buf, M_THUNDERBOLT);
		return (error);
	}

	cap->vsc_id = tbcap->vsc.vsc_id;
	cap->vsc_len = tbcap->vsc.len;
	if (tbcap->vsc.len == 0) {
		cap->next_cap = tbcap->vsec.vsec_next_cap;
		cap->vsec_len = tbcap->vsec.vsec_len;
	}

	free(buf, M_THUNDERBOLT);
	return (0);
}

int
tb_config_find_cap(struct router_softc *sc, struct router_cfg_cap *cap)
{
	u_int cap_id, vsc_id;
	int error;

	tb_debug(sc, DBG_ROUTER|DBG_EXTRA, "tb_config_find_cap called\n");

	cap_id = cap->cap_id;
	vsc_id = cap->vsc_id;

	cap->cap_id = cap->vsc_id = 0;
	while ((cap->cap_id != cap_id) || (cap->vsc_id != vsc_id)) {
		tb_debug(sc, DBG_ROUTER|DBG_EXTRA,
		    "Looking for cap %d at offset %d\n", cap->cap_id,
		    cap->next_cap);
		if ((cap->next_cap == 0) ||
		    (cap->next_cap > TB_CFG_CAP_OFFSET_MAX))
			return (EINVAL);
		error = tb_config_next_cap(sc, cap);
		if (error)
			break;
	}

	return (0);
}

int
tb_config_find_router_cap(struct router_softc *sc, u_int cap, u_int vsc, u_int *offset)
{
	struct router_cfg_cap rcap;
	struct tb_cfg_router *cfg;
	uint32_t *buf;
	int error;

	buf = malloc(8 * 4, M_THUNDERBOLT, M_NOWAIT|M_ZERO);
	if (buf == NULL)
		return (ENOMEM);

	error = tb_config_router_read(sc, 0, 5, buf);
	if (error != 0) {
		free(buf, M_THUNDERBOLT);
		return (error);
	}

	cfg = (struct tb_cfg_router *)buf;
	rcap.space = TB_CFG_CS_ROUTER;
	rcap.adap = 0;
	rcap.next_cap = GET_ROUTER_CS_NEXT_CAP(cfg);
	rcap.cap_id = cap;
	rcap.vsc_id = vsc;
	error = tb_config_find_cap(sc, &rcap);
	if (error == 0)
		*offset = rcap.current_cap;

	free(buf, M_THUNDERBOLT);
	return (error);
}

int
tb_config_find_router_vsc(struct router_softc *sc, u_int cap, u_int *offset)
{

	return (tb_config_find_router_cap(sc, TB_CFG_CAP_VSC, cap, offset));
}

int
tb_config_find_router_vsec(struct router_softc *sc, u_int cap, u_int *offset)
{

	return (tb_config_find_router_cap(sc, TB_CFG_CAP_VSEC, cap, offset));
}

int
tb_config_find_adapter_cap(struct router_softc *sc, u_int adap, u_int cap, u_int *offset)
{
	struct router_cfg_cap rcap;
	struct tb_cfg_adapter *cfg;
	uint32_t *buf;
	int error;

	buf = malloc(8 * 4, M_THUNDERBOLT, M_NOWAIT|M_ZERO);
	if (buf == NULL)
		return (ENOMEM);

	error = tb_config_adapter_read(sc, adap, 0, 8, buf);
	if (error != 0) {
		free(buf, M_THUNDERBOLT);
		return (error);
	}

	cfg = (struct tb_cfg_adapter *)buf;
	rcap.space = TB_CFG_CS_ADAPTER;
	rcap.adap = adap;
	rcap.next_cap = GET_ADP_CS_NEXT_CAP(cfg);
	rcap.cap_id = cap;
	rcap.vsc_id = 0;
	error = tb_config_find_cap(sc, &rcap);
	if (error == 0)
		*offset = rcap.current_cap;

	free(buf, M_THUNDERBOLT);
	return (error);
}

int
tb_config_get_lc_uuid(struct router_softc *rsc, uint8_t *uuid)
{
	u_int error, offset;
	uint32_t buf[8];

	bzero(buf, sizeof(buf));

	error = tb_config_find_router_vsec(rsc, TB_CFG_VSEC_LC, &offset);
	if (error != 0) {
		tb_debug(rsc, DBG_ROUTER, "Error finding LC registers: %d\n",
		    error);
		return (error);
	}

	error = tb_config_router_read(rsc, offset + TB_LC_UUID, 4, buf);
	if (error != 0) {
		tb_debug(rsc, DBG_ROUTER, "Error fetching UUID: %d\n", error);
		return (error);
	}

	bcopy(buf, uuid, 16);
	return (0);
}
