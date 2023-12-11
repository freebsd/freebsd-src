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

#include <dev/clk/clk.h>
#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "scmi.h"
#include "scmi_protocols.h"

#define SCMI_MAX_TOKEN		1024

#define	SCMI_HDR_TOKEN_S		18
#define SCMI_HDR_TOKEN_BF		(0x3fff)
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

struct scmi_req {
	int		cnt;
	bool		timed_out;
	bool		use_polling;
	bool		done;
	struct mtx	mtx;
	LIST_ENTRY(scmi_req)	next;
	int		protocol_id;
	int		message_id;
	int		token;
	uint32_t	header;
	struct scmi_msg msg;
};

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

static int		scmi_transport_init(struct scmi_softc *);
static void		scmi_transport_cleanup(struct scmi_softc *);
static struct scmi_reqs_pool *scmi_reqs_pool_allocate(const int, const int);
static void		scmi_reqs_pool_free(struct scmi_reqs_pool *);
static struct scmi_req *scmi_req_alloc(struct scmi_softc *, enum scmi_chan);
static void		scmi_req_free_unlocked(struct scmi_softc *,
    enum scmi_chan, struct scmi_req *);
static void		scmi_req_get(struct scmi_softc *, struct scmi_req *);
static void		scmi_req_put(struct scmi_softc *, struct scmi_req *);
static int		scmi_token_pick(struct scmi_softc *);
static void		scmi_token_release_unlocked(struct scmi_softc *, int);
static int		scmi_req_track_inflight(struct scmi_softc *,
			    struct scmi_req *);
static int		scmi_req_drop_inflight(struct scmi_softc *,
			    struct scmi_req *);
static struct scmi_req *scmi_req_lookup_inflight(struct scmi_softc *, uint32_t);

static int		scmi_wait_for_response(struct scmi_softc *,
			    struct scmi_req *, void **);
static void		scmi_process_response(struct scmi_softc *, uint32_t);

int
scmi_attach(device_t dev)
{
	struct scmi_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	simplebus_init(dev, node);

	error = scmi_transport_init(sc);
	if (error != 0)
		return (error);

	device_printf(dev, "Transport reply timeout initialized to %dms\n",
	    sc->trs_desc.reply_timo_ms);

	/*
	 * Allow devices to identify.
	 */
	bus_generic_probe(dev);

	/*
	 * Now walk the OFW tree and attach top-level devices.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node))
		simplebus_add_device(dev, node, 0, NULL, -1, NULL);

	error = bus_generic_attach(dev);

	return (error);
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
scmi_reqs_pool_allocate(const int max_msg, const int max_payld_sz)
{
	struct scmi_reqs_pool *rp;
	struct scmi_req *req;

	rp = malloc(sizeof(*rp), M_DEVBUF, M_ZERO | M_WAITOK);

	LIST_INIT(&rp->head);
	for (int i = 0; i < max_msg; i++) {
		req = malloc(sizeof(*req) + max_payld_sz,
		    M_DEVBUF, M_ZERO | M_WAITOK);

		mtx_init(&req->mtx, "req", "SCMI", MTX_SPIN);
		LIST_INSERT_HEAD(&rp->head, req, next);
	}

	mtx_init(&rp->mtx, "reqs_pool", "SCMI", MTX_SPIN);

	return (rp);
}

static void
scmi_reqs_pool_free(struct scmi_reqs_pool *rp)
{
	struct scmi_req *req;

	LIST_FOREACH(req, &rp->head, next) {
		mtx_destroy(&req->mtx);
		free(req, M_DEVBUF);
	}

	mtx_destroy(&rp->mtx);
	free(rp, M_DEVBUF);
}

static int
scmi_transport_init(struct scmi_softc *sc)
{
	struct scmi_transport *trs;
	int ret;

	trs = malloc(sizeof(*trs), M_DEVBUF, M_ZERO | M_WAITOK);

	BIT_FILL(SCMI_MAX_TOKEN, &trs->avail_tokens);
	mtx_init(&trs->mtx, "tokens", "SCMI", MTX_SPIN);

	trs->inflight_ht = hashinit(SCMI_MAX_MSG, M_DEVBUF,
	    &trs->inflight_mask);

	trs->chans[SCMI_CHAN_A2P] =
	    scmi_reqs_pool_allocate(SCMI_MAX_MSG, SCMI_MAX_MSG_PAYLD_SIZE);
	if (trs->chans[SCMI_CHAN_A2P] == NULL) {
		free(trs, M_DEVBUF);
		return (ENOMEM);
	}

	trs->chans[SCMI_CHAN_P2A] =
	    scmi_reqs_pool_allocate(SCMI_MAX_MSG, SCMI_MAX_MSG_PAYLD_SIZE);
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

	if (req != NULL)
		refcount_init(&req->cnt, 1);

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
	refcount_init(&req->cnt, 0);
	LIST_INSERT_HEAD(&rp->head, req, next);
	mtx_unlock_spin(&rp->mtx);
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

	return;
}

static void
scmi_req_put(struct scmi_softc *sc, struct scmi_req *req)
{
	mtx_lock_spin(&req->mtx);
	if (!refcount_release_if_not_last(&req->cnt)) {
		bzero(&req->msg, sizeof(req->msg) + SCMI_MAX_MSG_PAYLD_SIZE);
		scmi_req_free_unlocked(sc, SCMI_CHAN_A2P, req);
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
	/* TODO Account for wrap-arounds and holes */
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

static void
scmi_token_release_unlocked(struct scmi_softc *sc, int token)
{

	BIT_SET(SCMI_MAX_TOKEN, token, &sc->trs->avail_tokens);
}

static int
scmi_finalize_req(struct scmi_softc *sc, struct scmi_req *req)
{
	uint32_t header = 0;

	req->token = scmi_token_pick(sc);
	if (req->token < 0)
		return (EBUSY);

	header = req->message_id;
	header |= SCMI_MSG_TYPE_CMD << SCMI_HDR_MESSAGE_TYPE_S;
	header |= req->protocol_id << SCMI_HDR_PROTOCOL_ID_S;
	header |= req->token << SCMI_HDR_TOKEN_S;

	req->header = htole32(header);
	req->msg.hdr = htole32(header);

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
scmi_process_response(struct scmi_softc *sc, uint32_t hdr)
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

	mtx_lock_spin(&req->mtx);
	req->done = true;
	if (!req->timed_out)
		wakeup(req);
	else
		timed_out = true;
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
scmi_rx_irq_callback(device_t dev, void *chan, uint32_t hdr)
{
	struct scmi_softc *sc;

	sc = device_get_softc(dev);

	if (SCMI_IS_MSG_TYPE_NOTIF(hdr) || SCMI_IS_MSG_TYPE_DRESP(hdr)) {
		device_printf(dev, "DRESP/NOTIF unsupported. Drop.\n");
		SCMI_CLEAR_CHANNEL(dev, chan);
		return;
	}

	scmi_process_response(sc, hdr);
}

static int
scmi_wait_for_response(struct scmi_softc *sc, struct scmi_req *req, void **out)
{
	int ret;

	if (req->msg.polling) {
		bool needs_drop;

		ret = SCMI_POLL_MSG(sc->dev, &req->msg,
		    sc->trs_desc.reply_timo_ms);
		/*
		 * Drop reference to successfully polled req unless it had
		 * already also been processed on the IRQ path.
		 * Addresses a possible race-condition between polling and
		 * interrupt reception paths.
		 */
		mtx_lock_spin(&req->mtx);
		needs_drop = (ret == 0) && !req->done;
		mtx_unlock_spin(&req->mtx);
		if (needs_drop)
			scmi_req_drop_inflight(sc, req);
		if (ret == 0 && req->msg.hdr != req->header) {
			device_printf(sc->dev,
			    "Malformed reply with header |%08X|. Expected: |%08X|Drop.\n",
			    le32toh(req->msg.hdr), le32toh(req->header));
		}
	} else {
		ret = tsleep(req, 0, "scmi_wait4",
		    (sc->trs_desc.reply_timo_ms * hz) / 1000);
		/* Check for lost wakeups since there is no associated lock */
		mtx_lock_spin(&req->mtx);
		if (ret != 0 && req->done)
			ret = 0;
		mtx_unlock_spin(&req->mtx);
	}

	if (ret == 0) {
		SCMI_COLLECT_REPLY(sc->dev, &req->msg);
		if (req->msg.payld[0] != 0)
			ret = req->msg.payld[0];
		*out = &req->msg.payld[SCMI_MSG_HDR_SIZE];
	} else {
		mtx_lock_spin(&req->mtx);
		req->timed_out = true;
		mtx_unlock_spin(&req->mtx);
		device_printf(sc->dev,
		    "Request for token 0x%X timed-out.\n", req->token);
	}

	SCMI_TX_COMPLETE(sc->dev, NULL);

	return (ret);
}

void *
scmi_buf_get(device_t dev, uint8_t protocol_id, uint8_t message_id,
    int tx_payld_sz, int rx_payld_sz)
{
	struct scmi_softc *sc;
	struct scmi_req *req;

	sc = device_get_softc(dev);

	if (tx_payld_sz > SCMI_MAX_MSG_PAYLD_SIZE ||
	    rx_payld_sz > SCMI_MAX_MSG_REPLY_SIZE) {
		device_printf(dev, "Unsupported payload size. Drop.\n");
		return (NULL);
	}

	/* Pick one from free list */
	req = scmi_req_alloc(sc, SCMI_CHAN_A2P);
	if (req == NULL)
		return (NULL);

	req->protocol_id = protocol_id & SCMI_HDR_PROTOCOL_ID_BF;
	req->message_id = message_id & SCMI_HDR_MESSAGE_ID_BF;
	req->msg.tx_len = sizeof(req->msg.hdr) + tx_payld_sz;
	req->msg.rx_len = rx_payld_sz ?
	    rx_payld_sz + 2 * sizeof(uint32_t) : SCMI_MAX_MSG_SIZE;

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

int
scmi_request(device_t dev, void *in, void **out)
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
	if (error != 0)
		return (error);

	error = SCMI_XFER_MSG(sc->dev, &req->msg);
	if (error != 0) {
		scmi_req_drop_inflight(sc, req);
		return (error);
	}

	return (scmi_wait_for_response(sc, req, out));
}
