/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023 Arm Ltd
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/_bitset.h>
#include <sys/bitset.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <dev/clk/clk.h>
#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "scmi.h"
#include "scmi_protocols.h"

SDT_PROVIDER_DEFINE(scmi);
SDT_PROBE_DEFINE3(scmi, func, scmi_req_alloc, req_alloc,
    "int", "int", "int");
SDT_PROBE_DEFINE3(scmi, func, scmi_req_free_unlocked, req_alloc,
    "int", "int", "int");
SDT_PROBE_DEFINE3(scmi, func, scmi_req_get, req_alloc,
    "int", "int", "int");
SDT_PROBE_DEFINE3(scmi, func, scmi_req_put, req_alloc,
    "int", "int", "int");
SDT_PROBE_DEFINE5(scmi, func, scmi_request_tx, xfer_track,
    "int", "int", "int", "int", "int");
SDT_PROBE_DEFINE5(scmi, entry, scmi_wait_for_response, xfer_track,
    "int", "int", "int", "int", "int");
SDT_PROBE_DEFINE5(scmi, exit, scmi_wait_for_response, xfer_track,
    "int", "int", "int", "int", "int");
SDT_PROBE_DEFINE2(scmi, func, scmi_rx_irq_callback, hdr_dump,
    "int", "int");
SDT_PROBE_DEFINE5(scmi, func, scmi_process_response, xfer_track,
    "int", "int", "int", "int", "int");

#define SCMI_MAX_TOKEN		1024

#define	SCMI_HDR_TOKEN_S		18
#define SCMI_HDR_TOKEN_BF		(0x3ff)
#define	SCMI_HDR_TOKEN_M		(SCMI_HDR_TOKEN_BF << SCMI_HDR_TOKEN_S)

#define	SCMI_HDR_PROTOCOL_ID_S		10
#define	SCMI_HDR_PROTOCOL_ID_BF		(0xff)
#define	SCMI_HDR_PROTOCOL_ID_M		\
    (SCMI_HDR_PROTOCOL_ID_BF << SCMI_HDR_PROTOCOL_ID_S)

#define	SCMI_HDR_MESSAGE_TYPE_S		8
#define	SCMI_HDR_MESSAGE_TYPE_BF	(0x3)
#define	SCMI_HDR_MESSAGE_TYPE_M		\
    (SCMI_HDR_MESSAGE_TYPE_BF << SCMI_HDR_MESSAGE_TYPE_S)

#define	SCMI_HDR_MESSAGE_ID_S		0
#define	SCMI_HDR_MESSAGE_ID_BF		(0xff)
#define	SCMI_HDR_MESSAGE_ID_M		\
    (SCMI_HDR_MESSAGE_ID_BF << SCMI_HDR_MESSAGE_ID_S)

#define SCMI_MSG_TYPE_CMD	0
#define SCMI_MSG_TYPE_DRESP	2
#define SCMI_MSG_TYPE_NOTIF	3

#define SCMI_MSG_TYPE_CHECK(_h, _t)					\
    ((((_h) & SCMI_HDR_MESSAGE_TYPE_M) >> SCMI_HDR_MESSAGE_TYPE_S) == (_t))

#define SCMI_IS_MSG_TYPE_NOTIF(h)					\
    SCMI_MSG_TYPE_CHECK((h), SCMI_MSG_TYPE_NOTIF)
#define SCMI_IS_MSG_TYPE_DRESP(h)					\
    SCMI_MSG_TYPE_CHECK((h), SCMI_MSG_TYPE_DRESP)

#define SCMI_MSG_TOKEN(_hdr)		\
    (((_hdr) & SCMI_HDR_TOKEN_M) >> SCMI_HDR_TOKEN_S)
#define SCMI_MSG_PROTOCOL_ID(_hdr)		\
    (((_hdr) & SCMI_HDR_PROTOCOL_ID_M) >> SCMI_HDR_PROTOCOL_ID_S)
#define SCMI_MSG_MESSAGE_ID(_hdr)		\
    (((_hdr) & SCMI_HDR_MESSAGE_ID_M) >> SCMI_HDR_MESSAGE_ID_S)
#define SCMI_MSG_TYPE(_hdr)		\
    (((_hdr) & SCMI_HDR_TYPE_ID_M) >> SCMI_HDR_TYPE_ID_S)

struct scmi_req {
	int		cnt;
	bool		timed_out;
	bool		use_polling;
	bool		done;
	bool		is_raw;
	device_t	dev;
	struct task	tsk;
	struct mtx	mtx;
	LIST_ENTRY(scmi_req)	next;
	int		protocol_id;
	int		message_id;
	int		token;
	uint32_t	header;
	struct scmi_msg msg;
};

#define tsk_to_req(t)	__containerof((t), struct scmi_req, tsk)
#define buf_to_msg(b)	__containerof((b), struct scmi_msg, payld)
#define msg_to_req(m)	__containerof((m), struct scmi_req, msg)
#define buf_to_req(b)	msg_to_req(buf_to_msg(b))

LIST_HEAD(reqs_head, scmi_req);

struct scmi_reqs_pool {
	struct mtx		mtx;
	struct reqs_head	head;
};

BITSET_DEFINE(_scmi_tokens, SCMI_MAX_TOKEN);
LIST_HEAD(inflight_head, scmi_req);
#define	REQHASH(_sc, _tk)		\
    (&((_sc)->trs->inflight_ht[(_tk) & (_sc)->trs->inflight_mask]))

struct scmi_transport {
	unsigned long		next_id;
	struct _scmi_tokens	avail_tokens;
	struct inflight_head	*inflight_ht;
	unsigned long		inflight_mask;
	struct scmi_reqs_pool	*chans[SCMI_CHAN_MAX];
	struct mtx		mtx;
};

static void		scmi_transport_configure(struct scmi_transport_desc *, phandle_t);
static int		scmi_transport_init(struct scmi_softc *, phandle_t);
static void		scmi_transport_cleanup(struct scmi_softc *);
static void		scmi_req_async_waiter(void *, int);
static struct scmi_reqs_pool *scmi_reqs_pool_allocate(device_t, const int,
			    const int);
static void		scmi_reqs_pool_free(struct scmi_reqs_pool *);
static struct scmi_req	*scmi_req_alloc(struct scmi_softc *, enum scmi_chan);
static struct scmi_req	*scmi_req_initialized_alloc(device_t, int, int);
static void		scmi_req_free_unlocked(struct scmi_softc *,
			    enum scmi_chan, struct scmi_req *);
static void		scmi_req_get(struct scmi_softc *, struct scmi_req *);
static void		scmi_req_put(struct scmi_softc *, struct scmi_req *);
static int		scmi_token_pick(struct scmi_softc *);
static int		scmi_token_reserve(struct scmi_softc *, uint16_t);
static void		scmi_token_release_unlocked(struct scmi_softc *, int);
static int		scmi_req_track_inflight(struct scmi_softc *,
			    struct scmi_req *);
static int		scmi_req_drop_inflight(struct scmi_softc *,
			    struct scmi_req *);
static struct scmi_req *scmi_req_lookup_inflight(struct scmi_softc *, uint32_t);

static int		scmi_wait_for_response(struct scmi_softc *,
			    struct scmi_req *, void **);
static void		scmi_process_response(struct scmi_softc *, uint32_t,
			    unsigned int);

int
scmi_attach(device_t dev)
{
	struct sysctl_oid *sysctl_trans;
	struct scmi_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	simplebus_init(dev, node);

	error = scmi_transport_init(sc, node);
	if (error != 0)
		return (error);

	device_printf(dev, "Transport - max_msg:%d  max_payld_sz:%lu  reply_timo_ms:%d\n",
	    SCMI_MAX_MSG(sc), SCMI_MAX_MSG_PAYLD_SIZE(sc), SCMI_MAX_MSG_TIMEOUT_MS(sc));

	sc->sysctl_root = SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(_hw),
	    OID_AUTO, "scmi", CTLFLAG_RD, 0, "SCMI root");
	sysctl_trans = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(sc->sysctl_root),
	    OID_AUTO, "transport", CTLFLAG_RD, 0, "SCMI Transport properties");
	SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(sysctl_trans), OID_AUTO, "max_msg",
	    CTLFLAG_RD, &sc->trs_desc.max_msg, 0, "SCMI Max number of inflight messages");
	SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(sysctl_trans), OID_AUTO, "max_msg_size",
	    CTLFLAG_RD, &sc->trs_desc.max_payld_sz, 0, "SCMI Max message payload size");
	SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(sysctl_trans), OID_AUTO, "max_rx_timeout_ms",
	    CTLFLAG_RD, &sc->trs_desc.reply_timo_ms, 0, "SCMI Max message RX timeout ms");

	/*
	 * Allow devices to identify.
	 */
	bus_identify_children(dev);

	/*
	 * Now walk the OFW tree and attach top-level devices.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node))
		simplebus_add_device(dev, node, 0, NULL, -1, NULL);

	bus_attach_children(dev);

	return (0);
}

static int
scmi_detach(device_t dev)
{
	struct scmi_softc *sc;

	sc = device_get_softc(dev);
	scmi_transport_cleanup(sc);

	return (0);
}

static device_method_t scmi_methods[] = {
	DEVMETHOD(device_attach,	scmi_attach),
	DEVMETHOD(device_detach,	scmi_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(scmi, scmi_driver, scmi_methods, sizeof(struct scmi_softc),
    simplebus_driver);

DRIVER_MODULE(scmi, simplebus, scmi_driver, 0, 0);
MODULE_VERSION(scmi, 1);

static struct scmi_reqs_pool *
scmi_reqs_pool_allocate(device_t dev, const int max_msg, const int max_payld_sz)
{
	struct scmi_reqs_pool *rp;
	struct scmi_req *req;

	rp = malloc(sizeof(*rp), M_DEVBUF, M_ZERO | M_WAITOK);

	LIST_INIT(&rp->head);
	for (int i = 0; i < max_msg; i++) {
		req = malloc(sizeof(*req) + max_payld_sz,
		    M_DEVBUF, M_ZERO | M_WAITOK);

		req->dev = dev;
		req->tsk.ta_context = &req->tsk;
		req->tsk.ta_func = scmi_req_async_waiter;

		mtx_init(&req->mtx, "req", "SCMI", MTX_SPIN);
		LIST_INSERT_HEAD(&rp->head, req, next);
	}

	mtx_init(&rp->mtx, "reqs_pool", "SCMI", MTX_SPIN);

	return (rp);
}

static void
scmi_reqs_pool_free(struct scmi_reqs_pool *rp)
{
	struct scmi_req *req, *tmp;

	LIST_FOREACH_SAFE(req, &rp->head, next, tmp) {
		mtx_destroy(&req->mtx);
		free(req, M_DEVBUF);
	}

	mtx_destroy(&rp->mtx);
	free(rp, M_DEVBUF);
}

static void
scmi_transport_configure(struct scmi_transport_desc *td, phandle_t node)
{
	if (OF_getencprop(node, "arm,max-msg", &td->max_msg, sizeof(td->max_msg)) == -1)
		td->max_msg = SCMI_DEF_MAX_MSG;

	if (OF_getencprop(node, "arm,max-msg-size", &td->max_payld_sz,
	    sizeof(td->max_payld_sz)) == -1)
		td->max_payld_sz = SCMI_DEF_MAX_MSG_PAYLD_SIZE;
}

static int
scmi_transport_init(struct scmi_softc *sc, phandle_t node)
{
	struct scmi_transport_desc *td = &sc->trs_desc;
	struct scmi_transport *trs;
	int ret;

	trs = malloc(sizeof(*trs), M_DEVBUF, M_ZERO | M_WAITOK);

	scmi_transport_configure(td, node);

	BIT_FILL(SCMI_MAX_TOKEN, &trs->avail_tokens);
	mtx_init(&trs->mtx, "tokens", "SCMI", MTX_SPIN);

	trs->inflight_ht = hashinit(td->max_msg, M_DEVBUF, &trs->inflight_mask);

	trs->chans[SCMI_CHAN_A2P] =
	    scmi_reqs_pool_allocate(sc->dev, td->max_msg, td->max_payld_sz);
	if (trs->chans[SCMI_CHAN_A2P] == NULL) {
		free(trs, M_DEVBUF);
		return (ENOMEM);
	}

	trs->chans[SCMI_CHAN_P2A] =
	    scmi_reqs_pool_allocate(sc->dev, td->max_msg, td->max_payld_sz);
	if (trs->chans[SCMI_CHAN_P2A] == NULL) {
		scmi_reqs_pool_free(trs->chans[SCMI_CHAN_A2P]);
		free(trs, M_DEVBUF);
		return (ENOMEM);
	}

	sc->trs = trs;
	ret = SCMI_TRANSPORT_INIT(sc->dev);
	if (ret != 0) {
		scmi_reqs_pool_free(trs->chans[SCMI_CHAN_A2P]);
		scmi_reqs_pool_free(trs->chans[SCMI_CHAN_P2A]);
		free(trs, M_DEVBUF);
		return (ret);
	}

	/* Use default transport timeout if not overridden by OF */
	OF_getencprop(node, "arm,max-rx-timeout-ms", &td->reply_timo_ms,
	    sizeof(td->reply_timo_ms));

	return (0);
}

static void
scmi_transport_cleanup(struct scmi_softc *sc)
{

	SCMI_TRANSPORT_CLEANUP(sc->dev);
	mtx_destroy(&sc->trs->mtx);
	hashdestroy(sc->trs->inflight_ht, M_DEVBUF, sc->trs->inflight_mask);
	scmi_reqs_pool_free(sc->trs->chans[SCMI_CHAN_A2P]);
	scmi_reqs_pool_free(sc->trs->chans[SCMI_CHAN_P2A]);
	free(sc->trs, M_DEVBUF);
}

static struct scmi_req *
scmi_req_initialized_alloc(device_t dev, int tx_payld_sz, int rx_payld_sz)
{
	struct scmi_softc *sc;
	struct scmi_req *req;

	sc = device_get_softc(dev);

	if (tx_payld_sz > SCMI_MAX_MSG_PAYLD_SIZE(sc) ||
	    rx_payld_sz > SCMI_MAX_MSG_REPLY_SIZE(sc)) {
		device_printf(dev, "Unsupported payload size. Drop.\n");
		return (NULL);
	}

	/* Pick one from free list */
	req = scmi_req_alloc(sc, SCMI_CHAN_A2P);
	if (req == NULL)
		return (NULL);

	req->msg.tx_len = sizeof(req->msg.hdr) + tx_payld_sz;
	req->msg.rx_len = rx_payld_sz ?
	    rx_payld_sz + 2 * sizeof(uint32_t) : SCMI_MAX_MSG_SIZE(sc);

	return (req);
}

static struct scmi_req *
scmi_req_alloc(struct scmi_softc *sc, enum scmi_chan ch_idx)
{
	struct scmi_reqs_pool *rp;
	struct scmi_req *req = NULL;

	rp = sc->trs->chans[ch_idx];
	mtx_lock_spin(&rp->mtx);
	if (!LIST_EMPTY(&rp->head)) {
		req = LIST_FIRST(&rp->head);
		LIST_REMOVE_HEAD(&rp->head, next);
	}
	mtx_unlock_spin(&rp->mtx);

	if (req != NULL) {
		refcount_init(&req->cnt, 1);
		SDT_PROBE3(scmi, func, scmi_req_alloc, req_alloc,
		    req, refcount_load(&req->cnt), -1);
	}

	return (req);
}

static void
scmi_req_free_unlocked(struct scmi_softc *sc, enum scmi_chan ch_idx,
    struct scmi_req *req)
{
	struct scmi_reqs_pool *rp;

	rp = sc->trs->chans[ch_idx];
	mtx_lock_spin(&rp->mtx);
	req->timed_out = false;
	req->done = false;
	req->is_raw = false;
	refcount_init(&req->cnt, 0);
	LIST_INSERT_HEAD(&rp->head, req, next);
	mtx_unlock_spin(&rp->mtx);

	SDT_PROBE3(scmi, func, scmi_req_free_unlocked, req_alloc,
	    req, refcount_load(&req->cnt), -1);
}

static void
scmi_req_get(struct scmi_softc *sc, struct scmi_req *req)
{
	bool ok;

	mtx_lock_spin(&req->mtx);
	ok = refcount_acquire_if_not_zero(&req->cnt);
	mtx_unlock_spin(&req->mtx);

	if (!ok)
		device_printf(sc->dev, "%s() -- BAD REFCOUNT\n", __func__);

	SDT_PROBE3(scmi, func, scmi_req_get, req_alloc,
	    req, refcount_load(&req->cnt), SCMI_MSG_TOKEN(req->msg.hdr));

	return;
}

static void
scmi_req_put(struct scmi_softc *sc, struct scmi_req *req)
{
	mtx_lock_spin(&req->mtx);
	if (!refcount_release_if_not_last(&req->cnt)) {
		req->protocol_id = 0;
		req->message_id = 0;
		req->token = 0;
		req->header = 0;
		bzero(&req->msg, sizeof(req->msg) + SCMI_MAX_MSG_PAYLD_SIZE(sc));
		scmi_req_free_unlocked(sc, SCMI_CHAN_A2P, req);
	} else {
		SDT_PROBE3(scmi, func, scmi_req_put, req_alloc,
		    req, refcount_load(&req->cnt), SCMI_MSG_TOKEN(req->msg.hdr));
	}
	mtx_unlock_spin(&req->mtx);
}

static int
scmi_token_pick(struct scmi_softc *sc)
{
	unsigned long next_msg_id, token;

	mtx_lock_spin(&sc->trs->mtx);
	/*
	 * next_id is a monotonically increasing unsigned long that can be used
	 * for tracing purposes; next_msg_id is a 10-bit sequence number derived
	 * from it.
	 */
	next_msg_id = sc->trs->next_id++ & SCMI_HDR_TOKEN_BF;
	token = BIT_FFS_AT(SCMI_MAX_TOKEN, &sc->trs->avail_tokens, next_msg_id);
	if (token != 0)
		BIT_CLR(SCMI_MAX_TOKEN, token - 1, &sc->trs->avail_tokens);
	mtx_unlock_spin(&sc->trs->mtx);

	/*
	 * BIT_FFS_AT returns 1-indexed values, so 0 means failure to find a
	 * free slot: all possible SCMI messages are in-flight using all of the
	 * SCMI_MAX_TOKEN sequence numbers.
	 */
	if (!token)
		return (-EBUSY);

	return ((int)(token - 1));
}

static int
scmi_token_reserve(struct scmi_softc *sc, uint16_t candidate)
{
	int token = -EBUSY, retries = 3;

	do {
		mtx_lock_spin(&sc->trs->mtx);
		if (BIT_ISSET(SCMI_MAX_TOKEN, candidate, &sc->trs->avail_tokens)) {
			BIT_CLR(SCMI_MAX_TOKEN, candidate, &sc->trs->avail_tokens);
			token = candidate;
			sc->trs->next_id++;
		}
		mtx_unlock_spin(&sc->trs->mtx);
		if (token == candidate || retries-- == 0)
			break;

		pause("scmi_tk_reserve", hz);
	} while (1);

	return (token);
}

static void
scmi_token_release_unlocked(struct scmi_softc *sc, int token)
{

	BIT_SET(SCMI_MAX_TOKEN, token, &sc->trs->avail_tokens);
}

static int
scmi_finalize_req(struct scmi_softc *sc, struct scmi_req *req)
{
	if (!req->is_raw)
		req->token = scmi_token_pick(sc);
	else
		req->token = scmi_token_reserve(sc, SCMI_MSG_TOKEN(req->msg.hdr));

	if (req->token < 0)
		return (EBUSY);

	if (!req->is_raw) {
		req->msg.hdr = req->message_id;
		req->msg.hdr |= SCMI_MSG_TYPE_CMD << SCMI_HDR_MESSAGE_TYPE_S;
		req->msg.hdr |= req->protocol_id << SCMI_HDR_PROTOCOL_ID_S;
		req->msg.hdr |= req->token << SCMI_HDR_TOKEN_S;
	}

	/* Save requested header */
	req->header = req->msg.hdr;

	return (0);
}

static int
scmi_req_track_inflight(struct scmi_softc *sc, struct scmi_req *req)
{
	int error;

	/* build hdr, pick token */
	error = scmi_finalize_req(sc, req);
	if (error != 0)
		return (error);

	/* Bump refcount to get hold of this in-flight transaction */
	scmi_req_get(sc, req);
	/* Register in the inflight hashtable */
	mtx_lock_spin(&sc->trs->mtx);
	LIST_INSERT_HEAD(REQHASH(sc, req->token), req, next);
	mtx_unlock_spin(&sc->trs->mtx);

	return (0);
}

static int
scmi_req_drop_inflight(struct scmi_softc *sc, struct scmi_req *req)
{

	/* Remove from inflight hashtable at first ... */
	mtx_lock_spin(&sc->trs->mtx);
	LIST_REMOVE(req, next);
	scmi_token_release_unlocked(sc, req->token);
	mtx_unlock_spin(&sc->trs->mtx);
	/* ...and drop refcount..potentially releasing *req */
	scmi_req_put(sc, req);

	return (0);
}

static struct scmi_req *
scmi_req_lookup_inflight(struct scmi_softc *sc, uint32_t hdr)
{
	struct scmi_req *req = NULL;
	unsigned int token;

	token = SCMI_MSG_TOKEN(hdr);
	mtx_lock_spin(&sc->trs->mtx);
	LIST_FOREACH(req, REQHASH(sc, token), next) {
		if (req->token == token)
			break;
	}
	mtx_unlock_spin(&sc->trs->mtx);

	return (req);
}

static void
scmi_process_response(struct scmi_softc *sc, uint32_t hdr, uint32_t rx_len)
{
	bool timed_out = false;
	struct scmi_req *req;

	req = scmi_req_lookup_inflight(sc, hdr);
	if (req == NULL) {
		device_printf(sc->dev,
		    "Unexpected reply with header |%X| - token: 0x%X Drop.\n",
		    hdr, SCMI_MSG_TOKEN(hdr));
		return;
	}

	SDT_PROBE5(scmi, func, scmi_process_response, xfer_track, req,
	    SCMI_MSG_PROTOCOL_ID(req->msg.hdr), SCMI_MSG_MESSAGE_ID(req->msg.hdr),
	    SCMI_MSG_TOKEN(req->msg.hdr), req->timed_out);

	mtx_lock_spin(&req->mtx);
	req->done = true;
	req->msg.rx_len = rx_len;
	if (!req->timed_out) {
		/*
		 * Consider the case in which a polled message is picked
		 * by chance on the IRQ path on another CPU: setting poll_done
		 * will terminate the other poll loop.
		 */
		if (!req->msg.polling)
			wakeup(req);
		else
			atomic_store_rel_int(&req->msg.poll_done, 1);
	} else {
		timed_out = true;
	}
	mtx_unlock_spin(&req->mtx);

	if (timed_out)
		device_printf(sc->dev,
		    "Late reply for timed-out request - token: 0x%X. Ignore.\n",
		    req->token);

	/*
	 * In case of a late reply to a timed-out transaction this will
	 * finally free the pending scmi_req
	 */
	scmi_req_drop_inflight(sc, req);
}

void
scmi_rx_irq_callback(device_t dev, void *chan, uint32_t hdr, uint32_t rx_len)
{
	struct scmi_softc *sc;

	sc = device_get_softc(dev);

	SDT_PROBE2(scmi, func, scmi_rx_irq_callback, hdr_dump, hdr, rx_len);

	if (SCMI_IS_MSG_TYPE_NOTIF(hdr) || SCMI_IS_MSG_TYPE_DRESP(hdr)) {
		device_printf(dev, "DRESP/NOTIF unsupported. Drop.\n");
		SCMI_CLEAR_CHANNEL(dev, chan);
		return;
	}

	scmi_process_response(sc, hdr, rx_len);
}

static int
scmi_wait_for_response(struct scmi_softc *sc, struct scmi_req *req, void **out)
{
	unsigned int reply_timo_ms = SCMI_MAX_MSG_TIMEOUT_MS(sc);
	int ret;

	SDT_PROBE5(scmi, entry, scmi_wait_for_response, xfer_track, req,
	    SCMI_MSG_PROTOCOL_ID(req->msg.hdr), SCMI_MSG_MESSAGE_ID(req->msg.hdr),
	    SCMI_MSG_TOKEN(req->msg.hdr), reply_timo_ms);

	if (req->msg.polling) {
		bool needs_drop;

		ret = SCMI_POLL_MSG(sc->dev, &req->msg, reply_timo_ms);
		/*
		 * Drop reference to successfully polled req unless it had
		 * already also been processed on the IRQ path.
		 * Addresses a possible race-condition between polling and
		 * interrupt reception paths.
		 */
		mtx_lock_spin(&req->mtx);
		needs_drop = (ret == 0) && !req->done;
		req->timed_out = ret != 0;
		mtx_unlock_spin(&req->mtx);
		if (needs_drop)
			scmi_req_drop_inflight(sc, req);
		if (ret == 0 && req->msg.hdr != req->header) {
			device_printf(sc->dev,
			    "Malformed reply with header |%08X|. Expected: |%08X|Drop.\n",
			    le32toh(req->msg.hdr), le32toh(req->header));
		}
	} else {
		ret = tsleep(req, 0, "scmi_wait4", (reply_timo_ms * hz) / 1000);
		/* Check for lost wakeups since there is no associated lock */
		mtx_lock_spin(&req->mtx);
		if (ret != 0 && req->done)
			ret = 0;
		req->timed_out = ret != 0;
		mtx_unlock_spin(&req->mtx);
	}

	if (ret == 0) {
		SCMI_COLLECT_REPLY(sc->dev, &req->msg);
		if (req->msg.payld[0] != 0)
			ret = req->msg.payld[0];
		if (out != NULL)
			*out = &req->msg.payld[SCMI_MSG_HDR_SIZE];
	} else {
		device_printf(sc->dev,
		    "Request for token 0x%X timed-out.\n", req->token);
	}

	SCMI_TX_COMPLETE(sc->dev, NULL);

	SDT_PROBE5(scmi, exit, scmi_wait_for_response, xfer_track, req,
	    SCMI_MSG_PROTOCOL_ID(req->msg.hdr), SCMI_MSG_MESSAGE_ID(req->msg.hdr),
	    SCMI_MSG_TOKEN(req->msg.hdr), req->timed_out);

	return (ret);
}

void *
scmi_buf_get(device_t dev, uint8_t protocol_id, uint8_t message_id,
    int tx_payld_sz, int rx_payld_sz)
{
	struct scmi_req *req;

	/* Pick a pre-built req */
	req = scmi_req_initialized_alloc(dev, tx_payld_sz, rx_payld_sz);
	if (req == NULL)
		return (NULL);

	req->protocol_id = protocol_id & SCMI_HDR_PROTOCOL_ID_BF;
	req->message_id = message_id & SCMI_HDR_MESSAGE_ID_BF;

	return (&req->msg.payld[0]);
}

void
scmi_buf_put(device_t dev, void *buf)
{
	struct scmi_softc *sc;
	struct scmi_req *req;

	sc = device_get_softc(dev);

	req = buf_to_req(buf);
	scmi_req_put(sc, req);
}

struct scmi_msg *
scmi_msg_get(device_t dev, int tx_payld_sz, int rx_payld_sz)
{
	struct scmi_req *req;

	/* Pick a pre-built req */
	req = scmi_req_initialized_alloc(dev, tx_payld_sz, rx_payld_sz);
	if (req == NULL)
		return (NULL);

	req->is_raw = true;

	return (&req->msg);
}

static void
scmi_req_async_waiter(void *context, int pending)
{
	struct task *ta = context;
	struct scmi_softc *sc;
	struct scmi_req *req;

	req = tsk_to_req(ta);
	sc = device_get_softc(req->dev);
	scmi_wait_for_response(sc, req, NULL);

	scmi_msg_put(req->dev, &req->msg);
}

void
scmi_msg_put(device_t dev, struct scmi_msg *msg)
{
	struct scmi_softc *sc;
	struct scmi_req *req;

	sc = device_get_softc(dev);

	req = msg_to_req(msg);

	scmi_req_put(sc, req);
}

int
scmi_request_tx(device_t dev, void *in)
{
	struct scmi_softc *sc;
	struct scmi_req *req;
	int error;

	sc = device_get_softc(dev);

	req = buf_to_req(in);

	req->msg.polling =
	    (cold || sc->trs_desc.no_completion_irq || req->use_polling);

	/* Set inflight and send using transport specific method - refc-2 */
	error = scmi_req_track_inflight(sc, req);
	if (error != 0) {
		device_printf(dev, "Failed to build req with HDR |%0X|\n",
		    req->msg.hdr);
		return (error);
	}

	error = SCMI_XFER_MSG(sc->dev, &req->msg);
	if (error != 0) {
		scmi_req_drop_inflight(sc, req);
		return (error);
	}

	SDT_PROBE5(scmi, func, scmi_request_tx, xfer_track, req,
	    SCMI_MSG_PROTOCOL_ID(req->msg.hdr), SCMI_MSG_MESSAGE_ID(req->msg.hdr),
	    SCMI_MSG_TOKEN(req->msg.hdr), req->msg.polling);

	return (0);
}

int
scmi_request(device_t dev, void *in, void **out)
{
	struct scmi_softc *sc;
	struct scmi_req *req;
	int error;

	error = scmi_request_tx(dev, in);
	if (error != 0)
		return (error);

	sc = device_get_softc(dev);
	req = buf_to_req(in);

	return (scmi_wait_for_response(sc, req, out));
}

int
scmi_msg_async_enqueue(struct scmi_msg *msg)
{
	struct scmi_req *req;

	req = msg_to_req(msg);

	return taskqueue_enqueue_flags(taskqueue_thread, &req->tsk,
	    TASKQUEUE_FAIL_IF_PENDING | TASKQUEUE_FAIL_IF_CANCELING);
}
