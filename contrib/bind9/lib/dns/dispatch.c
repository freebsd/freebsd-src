/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dispatch.c,v 1.101.2.6.2.10 2004/09/01 04:27:41 marka Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/entropy.h>
#include <isc/lfsr.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/util.h>

#include <dns/acl.h>
#include <dns/dispatch.h>
#include <dns/events.h>
#include <dns/log.h>
#include <dns/message.h>
#include <dns/portlist.h>
#include <dns/tcpmsg.h>
#include <dns/types.h>

typedef ISC_LIST(dns_dispentry_t)	dns_displist_t;

typedef struct dns_qid {
	unsigned int	magic;
	unsigned int	qid_nbuckets;	/* hash table size */
	unsigned int	qid_increment;	/* id increment on collision */
	isc_mutex_t	lock;
	isc_lfsr_t	qid_lfsr1;	/* state generator info */
	isc_lfsr_t	qid_lfsr2;	/* state generator info */
	dns_displist_t	*qid_table;	/* the table itself */
} dns_qid_t;

struct dns_dispatchmgr {
	/* Unlocked. */
	unsigned int			magic;
	isc_mem_t		       *mctx;
	dns_acl_t		       *blackhole;
	dns_portlist_t		       *portlist;

	/* Locked by "lock". */
	isc_mutex_t			lock;
	unsigned int			state;
	ISC_LIST(dns_dispatch_t)	list;

	/* locked by buffer lock */
	dns_qid_t			*qid;
	isc_mutex_t			buffer_lock;
	unsigned int			buffers;    /* allocated buffers */
	unsigned int			buffersize; /* size of each buffer */
	unsigned int			maxbuffers; /* max buffers */

	/* Locked internally. */
	isc_mutex_t			pool_lock;
	isc_mempool_t		       *epool;	/* memory pool for events */
	isc_mempool_t		       *rpool;	/* memory pool for replies */
	isc_mempool_t		       *dpool;  /* dispatch allocations */
	isc_mempool_t		       *bpool;	/* memory pool for buffers */

	isc_entropy_t		       *entropy; /* entropy source */
};

#define MGR_SHUTTINGDOWN		0x00000001U
#define MGR_IS_SHUTTINGDOWN(l)	(((l)->state & MGR_SHUTTINGDOWN) != 0)

#define IS_PRIVATE(d)	(((d)->attributes & DNS_DISPATCHATTR_PRIVATE) != 0)

struct dns_dispentry {
	unsigned int			magic;
	dns_dispatch_t		       *disp;
	dns_messageid_t			id;
	unsigned int			bucket;
	isc_sockaddr_t			host;
	isc_task_t		       *task;
	isc_taskaction_t		action;
	void			       *arg;
	isc_boolean_t			item_out;
	ISC_LIST(dns_dispatchevent_t)	items;
	ISC_LINK(dns_dispentry_t)	link;
};

#define INVALID_BUCKET		(0xffffdead)

struct dns_dispatch {
	/* Unlocked. */
	unsigned int		magic;		/* magic */
	dns_dispatchmgr_t      *mgr;		/* dispatch manager */
	isc_task_t	       *task;		/* internal task */
	isc_socket_t	       *socket;		/* isc socket attached to */
	isc_sockaddr_t		local;		/* local address */
	unsigned int		maxrequests;	/* max requests */
	isc_event_t	       *ctlevent;

	/* Locked by mgr->lock. */
	ISC_LINK(dns_dispatch_t) link;

	/* Locked by "lock". */
	isc_mutex_t		lock;		/* locks all below */
	isc_sockettype_t	socktype;
	unsigned int		attributes;
	unsigned int		refcount;	/* number of users */
	dns_dispatchevent_t    *failsafe_ev;	/* failsafe cancel event */
	unsigned int		shutting_down : 1,
				shutdown_out : 1,
				connected : 1,
				tcpmsg_valid : 1,
				recv_pending : 1; /* is a recv() pending? */
	isc_result_t		shutdown_why;
	unsigned int		requests;	/* how many requests we have */
	unsigned int		tcpbuffers;	/* allocated buffers */
	dns_tcpmsg_t		tcpmsg;		/* for tcp streams */
	dns_qid_t		*qid;
};

#define QID_MAGIC		ISC_MAGIC('Q', 'i', 'd', ' ')
#define VALID_QID(e)		ISC_MAGIC_VALID((e), QID_MAGIC)

#define RESPONSE_MAGIC		ISC_MAGIC('D', 'r', 's', 'p')
#define VALID_RESPONSE(e)	ISC_MAGIC_VALID((e), RESPONSE_MAGIC)

#define DISPATCH_MAGIC		ISC_MAGIC('D', 'i', 's', 'p')
#define VALID_DISPATCH(e)	ISC_MAGIC_VALID((e), DISPATCH_MAGIC)

#define DNS_DISPATCHMGR_MAGIC	ISC_MAGIC('D', 'M', 'g', 'r')
#define VALID_DISPATCHMGR(e)	ISC_MAGIC_VALID((e), DNS_DISPATCHMGR_MAGIC)

#define DNS_QID(disp) ((disp)->socktype == isc_sockettype_tcp) ? \
		       (disp)->qid : (disp)->mgr->qid
/*
 * Statics.
 */
static dns_dispentry_t *bucket_search(dns_qid_t *, isc_sockaddr_t *,
				      dns_messageid_t, unsigned int);
static isc_boolean_t destroy_disp_ok(dns_dispatch_t *);
static void destroy_disp(isc_task_t *task, isc_event_t *event);
static void udp_recv(isc_task_t *, isc_event_t *);
static void tcp_recv(isc_task_t *, isc_event_t *);
static void startrecv(dns_dispatch_t *);
static dns_messageid_t dns_randomid(dns_qid_t *);
static isc_uint32_t dns_hash(dns_qid_t *, isc_sockaddr_t *, dns_messageid_t);
static void free_buffer(dns_dispatch_t *disp, void *buf, unsigned int len);
static void *allocate_udp_buffer(dns_dispatch_t *disp);
static inline void free_event(dns_dispatch_t *disp, dns_dispatchevent_t *ev);
static inline dns_dispatchevent_t *allocate_event(dns_dispatch_t *disp);
static void do_cancel(dns_dispatch_t *disp);
static dns_dispentry_t *linear_first(dns_qid_t *disp);
static dns_dispentry_t *linear_next(dns_qid_t *disp,
				    dns_dispentry_t *resp);
static void dispatch_free(dns_dispatch_t **dispp);
static isc_result_t dispatch_createudp(dns_dispatchmgr_t *mgr,
				       isc_socketmgr_t *sockmgr,
				       isc_taskmgr_t *taskmgr,
				       isc_sockaddr_t *localaddr,
				       unsigned int maxrequests,
				       unsigned int attributes,
				       dns_dispatch_t **dispp);
static isc_boolean_t destroy_mgr_ok(dns_dispatchmgr_t *mgr);
static void destroy_mgr(dns_dispatchmgr_t **mgrp);
static isc_result_t qid_allocate(dns_dispatchmgr_t *mgr, unsigned int buckets,
				 unsigned int increment, dns_qid_t **qidp);
static void qid_destroy(isc_mem_t *mctx, dns_qid_t **qidp);

#define LVL(x) ISC_LOG_DEBUG(x)

static void
mgr_log(dns_dispatchmgr_t *mgr, int level, const char *fmt, ...)
     ISC_FORMAT_PRINTF(3, 4);

static void
mgr_log(dns_dispatchmgr_t *mgr, int level, const char *fmt, ...) {
	char msgbuf[2048];
	va_list ap;

	if (! isc_log_wouldlog(dns_lctx, level))
		return;

	va_start(ap, fmt);
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	va_end(ap);

	isc_log_write(dns_lctx,
		      DNS_LOGCATEGORY_DISPATCH, DNS_LOGMODULE_DISPATCH,
		      level, "dispatchmgr %p: %s", mgr, msgbuf);
}

static void
dispatch_log(dns_dispatch_t *disp, int level, const char *fmt, ...)
     ISC_FORMAT_PRINTF(3, 4);

static void
dispatch_log(dns_dispatch_t *disp, int level, const char *fmt, ...) {
	char msgbuf[2048];
	va_list ap;

	if (! isc_log_wouldlog(dns_lctx, level))
		return;

	va_start(ap, fmt);
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	va_end(ap);

	isc_log_write(dns_lctx,
		      DNS_LOGCATEGORY_DISPATCH, DNS_LOGMODULE_DISPATCH,
		      level, "dispatch %p: %s", disp, msgbuf);
}

static void
request_log(dns_dispatch_t *disp, dns_dispentry_t *resp,
	    int level, const char *fmt, ...)
     ISC_FORMAT_PRINTF(4, 5);

static void
request_log(dns_dispatch_t *disp, dns_dispentry_t *resp,
	    int level, const char *fmt, ...)
{
	char msgbuf[2048];
	char peerbuf[256];
	va_list ap;

	if (! isc_log_wouldlog(dns_lctx, level))
		return;

	va_start(ap, fmt);
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	va_end(ap);

	if (VALID_RESPONSE(resp)) {
		isc_sockaddr_format(&resp->host, peerbuf, sizeof(peerbuf));
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DISPATCH,
			      DNS_LOGMODULE_DISPATCH, level,
			      "dispatch %p response %p %s: %s", disp, resp,
			      peerbuf, msgbuf);
	} else {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DISPATCH,
			      DNS_LOGMODULE_DISPATCH, level,
			      "dispatch %p req/resp %p: %s", disp, resp,
			      msgbuf);
	}
}

static void
reseed_lfsr(isc_lfsr_t *lfsr, void *arg)
{
	dns_dispatchmgr_t *mgr = arg;
	isc_result_t result;
	isc_uint32_t val;

	REQUIRE(VALID_DISPATCHMGR(mgr));

	if (mgr->entropy != NULL) {
		result = isc_entropy_getdata(mgr->entropy, &val, sizeof(val),
					     NULL, 0);
		INSIST(result == ISC_R_SUCCESS);
		lfsr->count = (val & 0x1f) + 32;
		lfsr->state = val;
		return;
	}

	lfsr->count = (random() & 0x1f) + 32;	/* From 32 to 63 states */
	lfsr->state = random();
}

/*
 * Return an unpredictable message ID.
 */
static dns_messageid_t
dns_randomid(dns_qid_t *qid) {
	isc_uint32_t id;

	id = isc_lfsr_generate32(&qid->qid_lfsr1, &qid->qid_lfsr2);

	return (dns_messageid_t)(id & 0xFFFF);
}

/*
 * Return a hash of the destination and message id.
 */
static isc_uint32_t
dns_hash(dns_qid_t *qid, isc_sockaddr_t *dest, dns_messageid_t id) {
	unsigned int ret;

	ret = isc_sockaddr_hash(dest, ISC_TRUE);
	ret ^= id;
	ret %= qid->qid_nbuckets;

	INSIST(ret < qid->qid_nbuckets);

	return (ret);
}

/*
 * Find the first entry in 'qid'.  Returns NULL if there are no entries.
 */
static dns_dispentry_t *
linear_first(dns_qid_t *qid) {
	dns_dispentry_t *ret;
	unsigned int bucket;

	bucket = 0;

	while (bucket < qid->qid_nbuckets) {
		ret = ISC_LIST_HEAD(qid->qid_table[bucket]);
		if (ret != NULL)
			return (ret);
		bucket++;
	}

	return (NULL);
}

/*
 * Find the next entry after 'resp' in 'qid'.  Return NULL if there are
 * no more entries.
 */
static dns_dispentry_t *
linear_next(dns_qid_t *qid, dns_dispentry_t *resp) {
	dns_dispentry_t *ret;
	unsigned int bucket;

	ret = ISC_LIST_NEXT(resp, link);
	if (ret != NULL)
		return (ret);

	bucket = resp->bucket;
	bucket++;
	while (bucket < qid->qid_nbuckets) {
		ret = ISC_LIST_HEAD(qid->qid_table[bucket]);
		if (ret != NULL)
			return (ret);
		bucket++;
	}

	return (NULL);
}

/*
 * The dispatch must be locked.
 */
static isc_boolean_t
destroy_disp_ok(dns_dispatch_t *disp)
{
	if (disp->refcount != 0)
		return (ISC_FALSE);

	if (disp->recv_pending != 0)
		return (ISC_FALSE);

	if (disp->shutting_down == 0)
		return (ISC_FALSE);

	return (ISC_TRUE);
}


/*
 * Called when refcount reaches 0 (and safe to destroy).
 *
 * The dispatcher must not be locked.
 * The manager must be locked.
 */
static void
destroy_disp(isc_task_t *task, isc_event_t *event) {
	dns_dispatch_t *disp;
	dns_dispatchmgr_t *mgr;
	isc_boolean_t killmgr;

	INSIST(event->ev_type == DNS_EVENT_DISPATCHCONTROL);

	UNUSED(task);

	disp = event->ev_arg;
	mgr = disp->mgr;

	LOCK(&mgr->lock);
	ISC_LIST_UNLINK(mgr->list, disp, link);

	dispatch_log(disp, LVL(90),
		     "shutting down; detaching from sock %p, task %p",
		     disp->socket, disp->task);

	isc_socket_detach(&disp->socket);
	isc_task_detach(&disp->task);
	isc_event_free(&event);

	dispatch_free(&disp);

	killmgr = destroy_mgr_ok(mgr);
	UNLOCK(&mgr->lock);
	if (killmgr)
		destroy_mgr(&mgr);
}


/*
 * Find an entry for query ID 'id' and socket address 'dest' in 'qid'.
 * Return NULL if no such entry exists.
 */
static dns_dispentry_t *
bucket_search(dns_qid_t *qid, isc_sockaddr_t *dest, dns_messageid_t id,
	      unsigned int bucket)
{
	dns_dispentry_t *res;

	REQUIRE(bucket < qid->qid_nbuckets);

	res = ISC_LIST_HEAD(qid->qid_table[bucket]);

	while (res != NULL) {
		if ((res->id == id) && isc_sockaddr_equal(dest, &res->host))
			return (res);
		res = ISC_LIST_NEXT(res, link);
	}

	return (NULL);
}

static void
free_buffer(dns_dispatch_t *disp, void *buf, unsigned int len) {
	INSIST(buf != NULL && len != 0);


	switch (disp->socktype) {
	case isc_sockettype_tcp:
		INSIST(disp->tcpbuffers > 0);
		disp->tcpbuffers--;
		isc_mem_put(disp->mgr->mctx, buf, len);
		break;
	case isc_sockettype_udp:
		LOCK(&disp->mgr->buffer_lock);
		INSIST(disp->mgr->buffers > 0);
		INSIST(len == disp->mgr->buffersize);
		disp->mgr->buffers--;
		isc_mempool_put(disp->mgr->bpool, buf);
		UNLOCK(&disp->mgr->buffer_lock);
		break;
	default:
		INSIST(0);
		break;
	}
}

static void *
allocate_udp_buffer(dns_dispatch_t *disp) {
	void *temp;

	LOCK(&disp->mgr->buffer_lock);
	temp = isc_mempool_get(disp->mgr->bpool);

	if (temp != NULL)
		disp->mgr->buffers++;
	UNLOCK(&disp->mgr->buffer_lock);

	return (temp);
}

static inline void
free_event(dns_dispatch_t *disp, dns_dispatchevent_t *ev) {
	if (disp->failsafe_ev == ev) {
		INSIST(disp->shutdown_out == 1);
		disp->shutdown_out = 0;

		return;
	}

	isc_mempool_put(disp->mgr->epool, ev);
}

static inline dns_dispatchevent_t *
allocate_event(dns_dispatch_t *disp) {
	dns_dispatchevent_t *ev;

	ev = isc_mempool_get(disp->mgr->epool);
	if (ev == NULL)
		return (NULL);
	ISC_EVENT_INIT(ev, sizeof(*ev), 0, NULL, 0,
		       NULL, NULL, NULL, NULL, NULL);

	return (ev);
}

/*
 * General flow:
 *
 * If I/O result == CANCELED or error, free the buffer.
 *
 * If query, free the buffer, restart.
 *
 * If response:
 *	Allocate event, fill in details.
 *		If cannot allocate, free buffer, restart.
 *	find target.  If not found, free buffer, restart.
 *	if event queue is not empty, queue.  else, send.
 *	restart.
 */
static void
udp_recv(isc_task_t *task, isc_event_t *ev_in) {
	isc_socketevent_t *ev = (isc_socketevent_t *)ev_in;
	dns_dispatch_t *disp = ev_in->ev_arg;
	dns_messageid_t id;
	isc_result_t dres;
	isc_buffer_t source;
	unsigned int flags;
	dns_dispentry_t *resp;
	dns_dispatchevent_t *rev;
	unsigned int bucket;
	isc_boolean_t killit;
	isc_boolean_t queue_response;
	dns_dispatchmgr_t *mgr;
	dns_qid_t *qid;
	isc_netaddr_t netaddr;
	int match;

	UNUSED(task);

	LOCK(&disp->lock);

	mgr = disp->mgr;
	qid = mgr->qid;

	dispatch_log(disp, LVL(90),
		     "got packet: requests %d, buffers %d, recvs %d",
		     disp->requests, disp->mgr->buffers, disp->recv_pending);

	if (ev->ev_type == ISC_SOCKEVENT_RECVDONE) {
		/*
		 * Unless the receive event was imported from a listening
		 * interface, in which case the event type is
		 * DNS_EVENT_IMPORTRECVDONE, receive operation must be pending.
		 */
		INSIST(disp->recv_pending != 0);
		disp->recv_pending = 0;
	}

	if (disp->shutting_down) {
		/*
		 * This dispatcher is shutting down.
		 */
		free_buffer(disp, ev->region.base, ev->region.length);

		isc_event_free(&ev_in);
		ev = NULL;

		killit = destroy_disp_ok(disp);
		UNLOCK(&disp->lock);
		if (killit)
			isc_task_send(disp->task, &disp->ctlevent);

		return;
	}

	if (ev->result != ISC_R_SUCCESS) {
		free_buffer(disp, ev->region.base, ev->region.length);

		if (ev->result != ISC_R_CANCELED)
			dispatch_log(disp, ISC_LOG_ERROR,
				     "odd socket result in udp_recv(): %s",
				     isc_result_totext(ev->result));

		UNLOCK(&disp->lock);
		isc_event_free(&ev_in);
		return;
	}

	/*
	 * If this is from a blackholed address, drop it.
	 */
	isc_netaddr_fromsockaddr(&netaddr, &ev->address);
	if (disp->mgr->blackhole != NULL &&
	    dns_acl_match(&netaddr, NULL, disp->mgr->blackhole,
		    	  NULL, &match, NULL) == ISC_R_SUCCESS &&
	    match > 0)
	{
		if (isc_log_wouldlog(dns_lctx, LVL(10))) {
			char netaddrstr[ISC_NETADDR_FORMATSIZE];
			isc_netaddr_format(&netaddr, netaddrstr,
					   sizeof(netaddrstr));
			dispatch_log(disp, LVL(10),
				     "blackholed packet from %s",
				     netaddrstr);
		}
		free_buffer(disp, ev->region.base, ev->region.length);
		goto restart;
	}

	/*
	 * Peek into the buffer to see what we can see.
	 */
	isc_buffer_init(&source, ev->region.base, ev->region.length);
	isc_buffer_add(&source, ev->n);
	dres = dns_message_peekheader(&source, &id, &flags);
	if (dres != ISC_R_SUCCESS) {
		free_buffer(disp, ev->region.base, ev->region.length);
		dispatch_log(disp, LVL(10), "got garbage packet");
		goto restart;
	}

	dispatch_log(disp, LVL(92),
		     "got valid DNS message header, /QR %c, id %u",
		     ((flags & DNS_MESSAGEFLAG_QR) ? '1' : '0'), id);

	/*
	 * Look at flags.  If query, drop it. If response,
	 * look to see where it goes.
	 */
	queue_response = ISC_FALSE;
	if ((flags & DNS_MESSAGEFLAG_QR) == 0) {
		/* query */
		free_buffer(disp, ev->region.base, ev->region.length);
		goto restart;
	}

	/* response */
	bucket = dns_hash(qid, &ev->address, id);
	LOCK(&qid->lock);
	resp = bucket_search(qid, &ev->address, id, bucket);
	dispatch_log(disp, LVL(90),
		     "search for response in bucket %d: %s",
		     bucket, (resp == NULL ? "not found" : "found"));

	if (resp == NULL) {
		free_buffer(disp, ev->region.base, ev->region.length);
		goto unlock;
	} 
	queue_response = resp->item_out;
	rev = allocate_event(resp->disp);
	if (rev == NULL) {
		free_buffer(disp, ev->region.base, ev->region.length);
		goto unlock;
	}

	/*
	 * At this point, rev contains the event we want to fill in, and
	 * resp contains the information on the place to send it to.
	 * Send the event off.
	 */
	isc_buffer_init(&rev->buffer, ev->region.base, ev->region.length);
	isc_buffer_add(&rev->buffer, ev->n);
	rev->result = ISC_R_SUCCESS;
	rev->id = id;
	rev->addr = ev->address;
	rev->pktinfo = ev->pktinfo;
	rev->attributes = ev->attributes;
	if (queue_response) {
		ISC_LIST_APPEND(resp->items, rev, ev_link);
	} else {
		ISC_EVENT_INIT(rev, sizeof(*rev), 0, NULL,
			       DNS_EVENT_DISPATCH,
			       resp->action, resp->arg, resp, NULL, NULL);
		request_log(disp, resp, LVL(90),
			    "[a] Sent event %p buffer %p len %d to task %p",
			    rev, rev->buffer.base, rev->buffer.length,
			    resp->task);
		resp->item_out = ISC_TRUE;
		isc_task_send(resp->task, ISC_EVENT_PTR(&rev));
	}
 unlock:
	UNLOCK(&qid->lock);

	/*
	 * Restart recv() to get the next packet.
	 */
 restart:
	startrecv(disp);

	UNLOCK(&disp->lock);

	isc_event_free(&ev_in);
}

/*
 * General flow:
 *
 * If I/O result == CANCELED, EOF, or error, notify everyone as the
 * various queues drain.
 *
 * If query, restart.
 *
 * If response:
 *	Allocate event, fill in details.
 *		If cannot allocate, restart.
 *	find target.  If not found, restart.
 *	if event queue is not empty, queue.  else, send.
 *	restart.
 */
static void
tcp_recv(isc_task_t *task, isc_event_t *ev_in) {
	dns_dispatch_t *disp = ev_in->ev_arg;
	dns_tcpmsg_t *tcpmsg = &disp->tcpmsg;
	dns_messageid_t id;
	isc_result_t dres;
	unsigned int flags;
	dns_dispentry_t *resp;
	dns_dispatchevent_t *rev;
	unsigned int bucket;
	isc_boolean_t killit;
	isc_boolean_t queue_response;
	dns_qid_t *qid;
	int level;
	char buf[ISC_SOCKADDR_FORMATSIZE];

	UNUSED(task);

	REQUIRE(VALID_DISPATCH(disp));

	qid = disp->qid;

	dispatch_log(disp, LVL(90),
		     "got TCP packet: requests %d, buffers %d, recvs %d",
		     disp->requests, disp->tcpbuffers, disp->recv_pending);

	LOCK(&disp->lock);

	INSIST(disp->recv_pending != 0);
	disp->recv_pending = 0;

	if (disp->refcount == 0) {
		/*
		 * This dispatcher is shutting down.  Force cancelation.
		 */
		tcpmsg->result = ISC_R_CANCELED;
	}

	if (tcpmsg->result != ISC_R_SUCCESS) {
		switch (tcpmsg->result) {
		case ISC_R_CANCELED:
			break;
			
		case ISC_R_EOF:
			dispatch_log(disp, LVL(90), "shutting down on EOF");
			do_cancel(disp);
			break;

		case ISC_R_CONNECTIONRESET:
			level = ISC_LOG_INFO;
			goto logit;

		default:
			level = ISC_LOG_ERROR;
		logit:
			isc_sockaddr_format(&tcpmsg->address, buf, sizeof(buf));
			dispatch_log(disp, level, "shutting down due to TCP "
				     "receive error: %s: %s", buf,
				     isc_result_totext(tcpmsg->result));
			do_cancel(disp);
			break;
		}

		/*
		 * The event is statically allocated in the tcpmsg
		 * structure, and destroy_disp() frees the tcpmsg, so we must
		 * free the event *before* calling destroy_disp().
		 */
		isc_event_free(&ev_in);

		disp->shutting_down = 1;
		disp->shutdown_why = tcpmsg->result;

		/*
		 * If the recv() was canceled pass the word on.
		 */
		killit = destroy_disp_ok(disp);
		UNLOCK(&disp->lock);
		if (killit)
			isc_task_send(disp->task, &disp->ctlevent);
		return;
	}

	dispatch_log(disp, LVL(90), "result %d, length == %d, addr = %p",
		     tcpmsg->result,
		     tcpmsg->buffer.length, tcpmsg->buffer.base);

	/*
	 * Peek into the buffer to see what we can see.
	 */
	dres = dns_message_peekheader(&tcpmsg->buffer, &id, &flags);
	if (dres != ISC_R_SUCCESS) {
		dispatch_log(disp, LVL(10), "got garbage packet");
		goto restart;
	}

	dispatch_log(disp, LVL(92),
		     "got valid DNS message header, /QR %c, id %u",
		     ((flags & DNS_MESSAGEFLAG_QR) ? '1' : '0'), id);

	/*
	 * Allocate an event to send to the query or response client, and
	 * allocate a new buffer for our use.
	 */

	/*
	 * Look at flags.  If query, drop it. If response,
	 * look to see where it goes.
	 */
	queue_response = ISC_FALSE;
	if ((flags & DNS_MESSAGEFLAG_QR) == 0) {
		/*
		 * Query.
		 */
		goto restart;
	}

	/*
	 * Response.
	 */
	bucket = dns_hash(qid, &tcpmsg->address, id);
	LOCK(&qid->lock);
	resp = bucket_search(qid, &tcpmsg->address, id, bucket);
	dispatch_log(disp, LVL(90),
		     "search for response in bucket %d: %s",
		     bucket, (resp == NULL ? "not found" : "found"));

	if (resp == NULL)
		goto unlock;
	queue_response = resp->item_out;
	rev = allocate_event(disp);
	if (rev == NULL)
		goto unlock;

	/*
	 * At this point, rev contains the event we want to fill in, and
	 * resp contains the information on the place to send it to.
	 * Send the event off.
	 */
	dns_tcpmsg_keepbuffer(tcpmsg, &rev->buffer);
	disp->tcpbuffers++;
	rev->result = ISC_R_SUCCESS;
	rev->id = id;
	rev->addr = tcpmsg->address;
	if (queue_response) {
		ISC_LIST_APPEND(resp->items, rev, ev_link);
	} else {
		ISC_EVENT_INIT(rev, sizeof(*rev), 0, NULL, DNS_EVENT_DISPATCH,
			       resp->action, resp->arg, resp, NULL, NULL);
		request_log(disp, resp, LVL(90),
			    "[b] Sent event %p buffer %p len %d to task %p",
			    rev, rev->buffer.base, rev->buffer.length,
			    resp->task);
		resp->item_out = ISC_TRUE;
		isc_task_send(resp->task, ISC_EVENT_PTR(&rev));
	}
 unlock:
	UNLOCK(&qid->lock);

	/*
	 * Restart recv() to get the next packet.
	 */
 restart:
	startrecv(disp);

	UNLOCK(&disp->lock);

	isc_event_free(&ev_in);
}

/*
 * disp must be locked.
 */
static void
startrecv(dns_dispatch_t *disp) {
	isc_result_t res;
	isc_region_t region;

	if (disp->shutting_down == 1)
		return;

	if ((disp->attributes & DNS_DISPATCHATTR_NOLISTEN) != 0)
		return;

	if (disp->recv_pending != 0)
		return;

	if (disp->mgr->buffers >= disp->mgr->maxbuffers)
		return;

	switch (disp->socktype) {
		/*
		 * UDP reads are always maximal.
		 */
	case isc_sockettype_udp:
		region.length = disp->mgr->buffersize;
		region.base = allocate_udp_buffer(disp);
		if (region.base == NULL)
			return;
		res = isc_socket_recv(disp->socket, &region, 1,
				      disp->task, udp_recv, disp);
		if (res != ISC_R_SUCCESS) {
			free_buffer(disp, region.base, region.length);
			disp->shutdown_why = res;
			disp->shutting_down = 1;
			do_cancel(disp);
			return;
		}
		INSIST(disp->recv_pending == 0);
		disp->recv_pending = 1;
		break;

	case isc_sockettype_tcp:
		res = dns_tcpmsg_readmessage(&disp->tcpmsg, disp->task,
					     tcp_recv, disp);
		if (res != ISC_R_SUCCESS) {
			disp->shutdown_why = res;
			disp->shutting_down = 1;
			do_cancel(disp);
			return;
		}
		INSIST(disp->recv_pending == 0);
		disp->recv_pending = 1;
		break;
	}
}

/*
 * Mgr must be locked when calling this function.
 */
static isc_boolean_t
destroy_mgr_ok(dns_dispatchmgr_t *mgr) {
	mgr_log(mgr, LVL(90),
		"destroy_mgr_ok: shuttingdown=%d, listnonempty=%d, "
		"epool=%d, rpool=%d, dpool=%d",
		MGR_IS_SHUTTINGDOWN(mgr), !ISC_LIST_EMPTY(mgr->list),
		isc_mempool_getallocated(mgr->epool),
		isc_mempool_getallocated(mgr->rpool),
		isc_mempool_getallocated(mgr->dpool));
	if (!MGR_IS_SHUTTINGDOWN(mgr))
		return (ISC_FALSE);
	if (!ISC_LIST_EMPTY(mgr->list))
		return (ISC_FALSE);
	if (isc_mempool_getallocated(mgr->epool) != 0)
		return (ISC_FALSE);
	if (isc_mempool_getallocated(mgr->rpool) != 0)
		return (ISC_FALSE);
	if (isc_mempool_getallocated(mgr->dpool) != 0)
		return (ISC_FALSE);

	return (ISC_TRUE);
}

/*
 * Mgr must be unlocked when calling this function.
 */
static void
destroy_mgr(dns_dispatchmgr_t **mgrp) {
	isc_mem_t *mctx;
	dns_dispatchmgr_t *mgr;

	mgr = *mgrp;
	*mgrp = NULL;

	mctx = mgr->mctx;

	mgr->magic = 0;
	mgr->mctx = NULL;
	DESTROYLOCK(&mgr->lock);
	mgr->state = 0;

	isc_mempool_destroy(&mgr->epool);
	isc_mempool_destroy(&mgr->rpool);
	isc_mempool_destroy(&mgr->dpool);
	isc_mempool_destroy(&mgr->bpool);

	DESTROYLOCK(&mgr->pool_lock);

	if (mgr->entropy != NULL)
		isc_entropy_detach(&mgr->entropy);
	if (mgr->qid != NULL)
		qid_destroy(mctx, &mgr->qid);

	DESTROYLOCK(&mgr->buffer_lock);

	if (mgr->blackhole != NULL)
		dns_acl_detach(&mgr->blackhole);

	if (mgr->portlist != NULL)
		dns_portlist_detach(&mgr->portlist);

	isc_mem_put(mctx, mgr, sizeof(dns_dispatchmgr_t));
	isc_mem_detach(&mctx);
}

static isc_result_t
create_socket(isc_socketmgr_t *mgr, isc_sockaddr_t *local,
	      isc_socket_t **sockp)
{
	isc_socket_t *sock;
	isc_result_t result;

	sock = NULL;
	result = isc_socket_create(mgr, isc_sockaddr_pf(local),
				   isc_sockettype_udp, &sock);
	if (result != ISC_R_SUCCESS)
		return (result);

#ifndef ISC_ALLOW_MAPPED
	isc_socket_ipv6only(sock, ISC_TRUE);
#endif
	result = isc_socket_bind(sock, local);
	if (result != ISC_R_SUCCESS) {
		isc_socket_detach(&sock);
		return (result);
	}

	*sockp = sock;
	return (ISC_R_SUCCESS);
}

/*
 * Publics.
 */

isc_result_t
dns_dispatchmgr_create(isc_mem_t *mctx, isc_entropy_t *entropy,
		       dns_dispatchmgr_t **mgrp)
{
	dns_dispatchmgr_t *mgr;
	isc_result_t result;

	REQUIRE(mctx != NULL);
	REQUIRE(mgrp != NULL && *mgrp == NULL);

	mgr = isc_mem_get(mctx, sizeof(dns_dispatchmgr_t));
	if (mgr == NULL)
		return (ISC_R_NOMEMORY);

	mgr->mctx = NULL;
	isc_mem_attach(mctx, &mgr->mctx);

	mgr->blackhole = NULL;
	mgr->portlist = NULL;

	result = isc_mutex_init(&mgr->lock);
	if (result != ISC_R_SUCCESS)
		goto deallocate;

	result = isc_mutex_init(&mgr->buffer_lock);
	if (result != ISC_R_SUCCESS)
		goto kill_lock;

	result = isc_mutex_init(&mgr->pool_lock);
	if (result != ISC_R_SUCCESS)
		goto kill_buffer_lock;

	mgr->epool = NULL;
	if (isc_mempool_create(mgr->mctx, sizeof(dns_dispatchevent_t),
			       &mgr->epool) != ISC_R_SUCCESS) {
		result = ISC_R_NOMEMORY;
		goto kill_pool_lock;
	}

	mgr->rpool = NULL;
	if (isc_mempool_create(mgr->mctx, sizeof(dns_dispentry_t),
			       &mgr->rpool) != ISC_R_SUCCESS) {
		result = ISC_R_NOMEMORY;
		goto kill_epool;
	}

	mgr->dpool = NULL;
	if (isc_mempool_create(mgr->mctx, sizeof(dns_dispatch_t),
			       &mgr->dpool) != ISC_R_SUCCESS) {
		result = ISC_R_NOMEMORY;
		goto kill_rpool;
	}

	isc_mempool_setname(mgr->epool, "dispmgr_epool");
	isc_mempool_setfreemax(mgr->epool, 1024);
	isc_mempool_associatelock(mgr->epool, &mgr->pool_lock);

	isc_mempool_setname(mgr->rpool, "dispmgr_rpool");
	isc_mempool_setfreemax(mgr->rpool, 1024);
	isc_mempool_associatelock(mgr->rpool, &mgr->pool_lock);

	isc_mempool_setname(mgr->dpool, "dispmgr_dpool");
	isc_mempool_setfreemax(mgr->dpool, 1024);
	isc_mempool_associatelock(mgr->dpool, &mgr->pool_lock);

	mgr->buffers = 0;
	mgr->buffersize = 0;
	mgr->maxbuffers = 0;
	mgr->bpool = NULL;
	mgr->entropy = NULL;
	mgr->qid = NULL;
	mgr->state = 0;
	ISC_LIST_INIT(mgr->list);
	mgr->magic = DNS_DISPATCHMGR_MAGIC;

	if (entropy != NULL)
		isc_entropy_attach(entropy, &mgr->entropy);

	*mgrp = mgr;
	return (ISC_R_SUCCESS);

 kill_rpool:
	isc_mempool_destroy(&mgr->rpool);
 kill_epool:
	isc_mempool_destroy(&mgr->epool);
 kill_pool_lock:
	DESTROYLOCK(&mgr->pool_lock);
 kill_buffer_lock:
	DESTROYLOCK(&mgr->buffer_lock);
 kill_lock:
	DESTROYLOCK(&mgr->lock);
 deallocate:
	isc_mem_put(mctx, mgr, sizeof(dns_dispatchmgr_t));
	isc_mem_detach(&mctx);

	return (result);
}

void
dns_dispatchmgr_setblackhole(dns_dispatchmgr_t *mgr, dns_acl_t *blackhole) {
	REQUIRE(VALID_DISPATCHMGR(mgr));
	if (mgr->blackhole != NULL)
		dns_acl_detach(&mgr->blackhole);
	dns_acl_attach(blackhole, &mgr->blackhole);
}

dns_acl_t *
dns_dispatchmgr_getblackhole(dns_dispatchmgr_t *mgr) {
	REQUIRE(VALID_DISPATCHMGR(mgr));
	return (mgr->blackhole);
}

void
dns_dispatchmgr_setblackportlist(dns_dispatchmgr_t *mgr,
				 dns_portlist_t *portlist)
{
	REQUIRE(VALID_DISPATCHMGR(mgr));
	if (mgr->portlist != NULL)
		dns_portlist_detach(&mgr->portlist);
	if (portlist != NULL)
		dns_portlist_attach(portlist, &mgr->portlist);
}

dns_portlist_t *
dns_dispatchmgr_getblackportlist(dns_dispatchmgr_t *mgr) {
	REQUIRE(VALID_DISPATCHMGR(mgr));
	return (mgr->portlist);
}

static isc_result_t
dns_dispatchmgr_setudp(dns_dispatchmgr_t *mgr,
			unsigned int buffersize, unsigned int maxbuffers,
			unsigned int buckets, unsigned int increment)
{
	isc_result_t result;

	REQUIRE(VALID_DISPATCHMGR(mgr));
	REQUIRE(buffersize >= 512 && buffersize < (64 * 1024));
	REQUIRE(maxbuffers > 0);
	REQUIRE(buckets < 2097169);  /* next prime > 65536 * 32 */
	REQUIRE(increment > buckets);

	/*
	 * Keep some number of items around.  This should be a config
	 * option.  For now, keep 8, but later keep at least two even
	 * if the caller wants less.  This allows us to ensure certain
	 * things, like an event can be "freed" and the next allocation
	 * will always succeed.
	 *
	 * Note that if limits are placed on anything here, we use one
	 * event internally, so the actual limit should be "wanted + 1."
	 *
	 * XXXMLG
	 */

	if (maxbuffers < 8)
		maxbuffers = 8;

	LOCK(&mgr->buffer_lock);
	if (mgr->bpool != NULL) {
		isc_mempool_setmaxalloc(mgr->bpool, maxbuffers);
		mgr->maxbuffers = maxbuffers;
		UNLOCK(&mgr->buffer_lock);
		return (ISC_R_SUCCESS);
	}

	if (isc_mempool_create(mgr->mctx, buffersize,
			       &mgr->bpool) != ISC_R_SUCCESS) {
		return (ISC_R_NOMEMORY);
	}

	isc_mempool_setname(mgr->bpool, "dispmgr_bpool");
	isc_mempool_setmaxalloc(mgr->bpool, maxbuffers);
	isc_mempool_associatelock(mgr->bpool, &mgr->pool_lock);

	result = qid_allocate(mgr, buckets, increment, &mgr->qid);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	mgr->buffersize = buffersize;
	mgr->maxbuffers = maxbuffers;
	UNLOCK(&mgr->buffer_lock);
	return (ISC_R_SUCCESS);

 cleanup:
	isc_mempool_destroy(&mgr->bpool);
	UNLOCK(&mgr->buffer_lock);
	return (ISC_R_NOMEMORY);
}

void
dns_dispatchmgr_destroy(dns_dispatchmgr_t **mgrp) {
	dns_dispatchmgr_t *mgr;
	isc_boolean_t killit;

	REQUIRE(mgrp != NULL);
	REQUIRE(VALID_DISPATCHMGR(*mgrp));

	mgr = *mgrp;
	*mgrp = NULL;

	LOCK(&mgr->lock);
	mgr->state |= MGR_SHUTTINGDOWN;

	killit = destroy_mgr_ok(mgr);
	UNLOCK(&mgr->lock);

	mgr_log(mgr, LVL(90), "destroy: killit=%d", killit);

	if (killit)
		destroy_mgr(&mgr);
}

static isc_boolean_t
blacklisted(dns_dispatchmgr_t *mgr, isc_socket_t *sock) {
	isc_sockaddr_t sockaddr;
	isc_result_t result;

	if (mgr->portlist == NULL)
		return (ISC_FALSE);

	result = isc_socket_getsockname(sock, &sockaddr);
	if (result != ISC_R_SUCCESS)
		return (ISC_FALSE);

	if (mgr->portlist != NULL &&
	    dns_portlist_match(mgr->portlist, isc_sockaddr_pf(&sockaddr),
			       isc_sockaddr_getport(&sockaddr)))
		return (ISC_TRUE);
	return (ISC_FALSE);
}

#define ATTRMATCH(_a1, _a2, _mask) (((_a1) & (_mask)) == ((_a2) & (_mask)))

static isc_boolean_t
local_addr_match(dns_dispatch_t *disp, isc_sockaddr_t *addr) {
	isc_sockaddr_t sockaddr;
	isc_result_t result;

	if (addr == NULL)
		return (ISC_TRUE);

	/*
	 * Don't match wildcard ports against newly blacklisted ports.
	 */
	if (disp->mgr->portlist != NULL &&
	    isc_sockaddr_getport(addr) == 0 &&
	    isc_sockaddr_getport(&disp->local) == 0 &&
	    blacklisted(disp->mgr, disp->socket))
		return (ISC_FALSE);

	/*
	 * Check if we match the binding <address,port>.
	 * Wildcard ports match/fail here.
	 */
	if (isc_sockaddr_equal(&disp->local, addr))
		return (ISC_TRUE);
	if (isc_sockaddr_getport(addr) == 0)
		return (ISC_FALSE);

	/*
	 * Check if we match a bound wildcard port <address,port>.
	 */
	if (!isc_sockaddr_eqaddr(&disp->local, addr))
		return (ISC_FALSE);
	result = isc_socket_getsockname(disp->socket, &sockaddr);
	if (result != ISC_R_SUCCESS)
		return (ISC_FALSE);

	return (isc_sockaddr_equal(&sockaddr, addr));
}

/*
 * Requires mgr be locked.
 *
 * No dispatcher can be locked by this thread when calling this function.
 *
 *
 * NOTE:
 *	If a matching dispatcher is found, it is locked after this function
 *	returns, and must be unlocked by the caller.
 */
static isc_result_t
dispatch_find(dns_dispatchmgr_t *mgr, isc_sockaddr_t *local,
	      unsigned int attributes, unsigned int mask,
	      dns_dispatch_t **dispp)
{
	dns_dispatch_t *disp;
	isc_result_t result;

	/*
	 * Make certain that we will not match a private dispatch.
	 */
	attributes &= ~DNS_DISPATCHATTR_PRIVATE;
	mask |= DNS_DISPATCHATTR_PRIVATE;

	disp = ISC_LIST_HEAD(mgr->list);
	while (disp != NULL) {
		LOCK(&disp->lock);
		if ((disp->shutting_down == 0)
		    && ATTRMATCH(disp->attributes, attributes, mask)
		    && local_addr_match(disp, local))
			break;
		UNLOCK(&disp->lock);
		disp = ISC_LIST_NEXT(disp, link);
	}

	if (disp == NULL) {
		result = ISC_R_NOTFOUND;
		goto out;
	}

	*dispp = disp;
	result = ISC_R_SUCCESS;
 out:

	return (result);
}

static isc_result_t
qid_allocate(dns_dispatchmgr_t *mgr, unsigned int buckets,
	     unsigned int increment, dns_qid_t **qidp)
{
	dns_qid_t *qid;
	unsigned int i;

	REQUIRE(VALID_DISPATCHMGR(mgr));
	REQUIRE(buckets < 2097169);  /* next prime > 65536 * 32 */
	REQUIRE(increment > buckets);
	REQUIRE(qidp != NULL && *qidp == NULL);

	qid = isc_mem_get(mgr->mctx, sizeof(*qid));
	if (qid == NULL)
		return (ISC_R_NOMEMORY);

	qid->qid_table = isc_mem_get(mgr->mctx,
				     buckets * sizeof(dns_displist_t));
	if (qid->qid_table == NULL) {
		isc_mem_put(mgr->mctx, qid, sizeof(*qid));
		return (ISC_R_NOMEMORY);
	}

	if (isc_mutex_init(&qid->lock) != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__, "isc_mutex_init failed");
		isc_mem_put(mgr->mctx, qid->qid_table,
			    buckets * sizeof(dns_displist_t));
		isc_mem_put(mgr->mctx, qid, sizeof(*qid));
		return (ISC_R_UNEXPECTED);
	}

	for (i = 0; i < buckets; i++)
		ISC_LIST_INIT(qid->qid_table[i]);

	qid->qid_nbuckets = buckets;
	qid->qid_increment = increment;
	qid->magic = QID_MAGIC;

	/*
	 * Initialize to a 32-bit LFSR.  Both of these are from Applied
	 * Cryptography.
	 *
	 * lfsr1:
	 *	x^32 + x^7 + x^5 + x^3 + x^2 + x + 1
	 *
	 * lfsr2:
	 *	x^32 + x^7 + x^6 + x^2 + 1
	 */
	isc_lfsr_init(&qid->qid_lfsr1, 0, 32, 0x80000057U,
		      0, reseed_lfsr, mgr);
	isc_lfsr_init(&qid->qid_lfsr2, 0, 32, 0x80000062U,
		      0, reseed_lfsr, mgr);
	*qidp = qid;
	return (ISC_R_SUCCESS);
}

static void
qid_destroy(isc_mem_t *mctx, dns_qid_t **qidp) {
	dns_qid_t *qid;

	REQUIRE(qidp != NULL);
	qid = *qidp;

	REQUIRE(VALID_QID(qid));

	*qidp = NULL;
	qid->magic = 0;
	isc_mem_put(mctx, qid->qid_table,
		    qid->qid_nbuckets * sizeof(dns_displist_t));
	DESTROYLOCK(&qid->lock);
	isc_mem_put(mctx, qid, sizeof(*qid));
}

/*
 * Allocate and set important limits.
 */
static isc_result_t
dispatch_allocate(dns_dispatchmgr_t *mgr, unsigned int maxrequests,
		  dns_dispatch_t **dispp)
{
	dns_dispatch_t *disp;
	isc_result_t res;

	REQUIRE(VALID_DISPATCHMGR(mgr));
	REQUIRE(dispp != NULL && *dispp == NULL);

	/*
	 * Set up the dispatcher, mostly.  Don't bother setting some of
	 * the options that are controlled by tcp vs. udp, etc.
	 */

	disp = isc_mempool_get(mgr->dpool);
	if (disp == NULL)
		return (ISC_R_NOMEMORY);

	disp->magic = 0;
	disp->mgr = mgr;
	disp->maxrequests = maxrequests;
	disp->attributes = 0;
	ISC_LINK_INIT(disp, link);
	disp->refcount = 1;
	disp->recv_pending = 0;
	memset(&disp->local, 0, sizeof(disp->local));
	disp->shutting_down = 0;
	disp->shutdown_out = 0;
	disp->connected = 0;
	disp->tcpmsg_valid = 0;
	disp->shutdown_why = ISC_R_UNEXPECTED;
	disp->requests = 0;
	disp->tcpbuffers = 0;
	disp->qid = NULL;

	if (isc_mutex_init(&disp->lock) != ISC_R_SUCCESS) {
		res = ISC_R_UNEXPECTED;
		UNEXPECTED_ERROR(__FILE__, __LINE__, "isc_mutex_init failed");
		goto deallocate;
	}

	disp->failsafe_ev = allocate_event(disp);
	if (disp->failsafe_ev == NULL) {
		res = ISC_R_NOMEMORY;
		goto kill_lock;
	}

	disp->magic = DISPATCH_MAGIC;

	*dispp = disp;
	return (ISC_R_SUCCESS);

	/*
	 * error returns
	 */
 kill_lock:
	DESTROYLOCK(&disp->lock);
 deallocate:
	isc_mempool_put(mgr->dpool, disp);

	return (res);
}


/*
 * MUST be unlocked, and not used by anthing.
 */
static void
dispatch_free(dns_dispatch_t **dispp)
{
	dns_dispatch_t *disp;
	dns_dispatchmgr_t *mgr;

	REQUIRE(VALID_DISPATCH(*dispp));
	disp = *dispp;
	*dispp = NULL;

	mgr = disp->mgr;
	REQUIRE(VALID_DISPATCHMGR(mgr));

	if (disp->tcpmsg_valid) {
		dns_tcpmsg_invalidate(&disp->tcpmsg);
		disp->tcpmsg_valid = 0;
	}

	INSIST(disp->tcpbuffers == 0);
	INSIST(disp->requests == 0);
	INSIST(disp->recv_pending == 0);

	isc_mempool_put(mgr->epool, disp->failsafe_ev);
	disp->failsafe_ev = NULL;

	if (disp->qid != NULL)
		qid_destroy(mgr->mctx, &disp->qid);
	disp->mgr = NULL;
	DESTROYLOCK(&disp->lock);
	disp->magic = 0;
	isc_mempool_put(mgr->dpool, disp);
}

isc_result_t
dns_dispatch_createtcp(dns_dispatchmgr_t *mgr, isc_socket_t *sock,
		       isc_taskmgr_t *taskmgr, unsigned int buffersize,
		       unsigned int maxbuffers, unsigned int maxrequests,
		       unsigned int buckets, unsigned int increment,
		       unsigned int attributes, dns_dispatch_t **dispp)
{
	isc_result_t result;
	dns_dispatch_t *disp;

	UNUSED(maxbuffers);
	UNUSED(buffersize);

	REQUIRE(VALID_DISPATCHMGR(mgr));
	REQUIRE(isc_socket_gettype(sock) == isc_sockettype_tcp);
	REQUIRE((attributes & DNS_DISPATCHATTR_TCP) != 0);
	REQUIRE((attributes & DNS_DISPATCHATTR_UDP) == 0);

	attributes |= DNS_DISPATCHATTR_PRIVATE;  /* XXXMLG */

	LOCK(&mgr->lock);

	/*
	 * dispatch_allocate() checks mgr for us.
	 * qid_allocate() checks buckets and increment for us.
	 */
	disp = NULL;
	result = dispatch_allocate(mgr, maxrequests, &disp);
	if (result != ISC_R_SUCCESS) {
		UNLOCK(&mgr->lock);
		return (result);
	}

	result = qid_allocate(mgr, buckets, increment, &disp->qid);
	if (result != ISC_R_SUCCESS)
		goto deallocate_dispatch;

	disp->socktype = isc_sockettype_tcp;
	disp->socket = NULL;
	isc_socket_attach(sock, &disp->socket);

	disp->task = NULL;
	result = isc_task_create(taskmgr, 0, &disp->task);
	if (result != ISC_R_SUCCESS)
		goto kill_socket;

	disp->ctlevent = isc_event_allocate(mgr->mctx, disp,
					    DNS_EVENT_DISPATCHCONTROL,
					    destroy_disp, disp,
					    sizeof(isc_event_t));
	if (disp->ctlevent == NULL)
		goto kill_task;

	isc_task_setname(disp->task, "tcpdispatch", disp);

	dns_tcpmsg_init(mgr->mctx, disp->socket, &disp->tcpmsg);
	disp->tcpmsg_valid = 1;

	disp->attributes = attributes;

	/*
	 * Append it to the dispatcher list.
	 */
	ISC_LIST_APPEND(mgr->list, disp, link);
	UNLOCK(&mgr->lock);

	mgr_log(mgr, LVL(90), "created TCP dispatcher %p", disp);
	dispatch_log(disp, LVL(90), "created task %p", disp->task);

	*dispp = disp;

	return (ISC_R_SUCCESS);

	/*
	 * Error returns.
	 */
 kill_task:
	isc_task_detach(&disp->task);
 kill_socket:
	isc_socket_detach(&disp->socket);
 deallocate_dispatch:
	dispatch_free(&disp);

	UNLOCK(&mgr->lock);

	return (result);
}

isc_result_t
dns_dispatch_getudp(dns_dispatchmgr_t *mgr, isc_socketmgr_t *sockmgr,
		    isc_taskmgr_t *taskmgr, isc_sockaddr_t *localaddr,
		    unsigned int buffersize,
		    unsigned int maxbuffers, unsigned int maxrequests,
		    unsigned int buckets, unsigned int increment,
		    unsigned int attributes, unsigned int mask,
		    dns_dispatch_t **dispp)
{
	isc_result_t result;
	dns_dispatch_t *disp;

	REQUIRE(VALID_DISPATCHMGR(mgr));
	REQUIRE(sockmgr != NULL);
	REQUIRE(localaddr != NULL);
	REQUIRE(taskmgr != NULL);
	REQUIRE(buffersize >= 512 && buffersize < (64 * 1024));
	REQUIRE(maxbuffers > 0);
	REQUIRE(buckets < 2097169);  /* next prime > 65536 * 32 */
	REQUIRE(increment > buckets);
	REQUIRE(dispp != NULL && *dispp == NULL);
	REQUIRE((attributes & DNS_DISPATCHATTR_TCP) == 0);

	result = dns_dispatchmgr_setudp(mgr, buffersize, maxbuffers,
					buckets, increment);
	if (result != ISC_R_SUCCESS)
		return (result);

	LOCK(&mgr->lock);

	/*
	 * First, see if we have a dispatcher that matches.
	 */
	disp = NULL;
	result = dispatch_find(mgr, localaddr, attributes, mask, &disp);
	if (result == ISC_R_SUCCESS) {
		disp->refcount++;

		if (disp->maxrequests < maxrequests)
			disp->maxrequests = maxrequests;

		if ((disp->attributes & DNS_DISPATCHATTR_NOLISTEN) == 0 &&
		    (attributes & DNS_DISPATCHATTR_NOLISTEN) != 0)
		{
			disp->attributes |= DNS_DISPATCHATTR_NOLISTEN;
			if (disp->recv_pending != 0)
				isc_socket_cancel(disp->socket, disp->task,
						  ISC_SOCKCANCEL_RECV);
		}

		UNLOCK(&disp->lock);
		UNLOCK(&mgr->lock);

		*dispp = disp;

		return (ISC_R_SUCCESS);
	}

	/*
	 * Nope, create one.
	 */
	result = dispatch_createudp(mgr, sockmgr, taskmgr, localaddr,
				    maxrequests, attributes, &disp);
	if (result != ISC_R_SUCCESS) {
		UNLOCK(&mgr->lock);
		return (result);
	}

	UNLOCK(&mgr->lock);
	*dispp = disp;
	return (ISC_R_SUCCESS);
}

/*
 * mgr should be locked.
 */
static isc_result_t
dispatch_createudp(dns_dispatchmgr_t *mgr, isc_socketmgr_t *sockmgr,
		   isc_taskmgr_t *taskmgr,
		   isc_sockaddr_t *localaddr,
		   unsigned int maxrequests,
		   unsigned int attributes,
		   dns_dispatch_t **dispp)
{
	isc_result_t result;
	dns_dispatch_t *disp;
	isc_socket_t *sock;

	/*
	 * dispatch_allocate() checks mgr for us.
	 */
	disp = NULL;
	result = dispatch_allocate(mgr, maxrequests, &disp);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * This assumes that the IP stack will *not* quickly reallocate
	 * the same port.  If it does continually reallocate the same port
	 * then we need a mechanism to hold all the blacklisted sockets
	 * until we find a usable socket.
	 */
 getsocket:
	result = create_socket(sockmgr, localaddr, &sock);
	if (result != ISC_R_SUCCESS)
		goto deallocate_dispatch;
	if (isc_sockaddr_getport(localaddr) == 0 && blacklisted(mgr, sock)) {
		isc_socket_detach(&sock);
		goto getsocket;
	}

	disp->socktype = isc_sockettype_udp;
	disp->socket = sock;
	disp->local = *localaddr;

	disp->task = NULL;
	result = isc_task_create(taskmgr, 0, &disp->task);
	if (result != ISC_R_SUCCESS)
		goto kill_socket;

	disp->ctlevent = isc_event_allocate(mgr->mctx, disp,
					    DNS_EVENT_DISPATCHCONTROL,
					    destroy_disp, disp,
					    sizeof(isc_event_t));
	if (disp->ctlevent == NULL)
		goto kill_task;

	isc_task_setname(disp->task, "udpdispatch", disp);

	attributes &= ~DNS_DISPATCHATTR_TCP;
	attributes |= DNS_DISPATCHATTR_UDP;
	disp->attributes = attributes;

	/*
	 * Append it to the dispatcher list.
	 */
	ISC_LIST_APPEND(mgr->list, disp, link);

	mgr_log(mgr, LVL(90), "created UDP dispatcher %p", disp);
	dispatch_log(disp, LVL(90), "created task %p", disp->task);
	dispatch_log(disp, LVL(90), "created socket %p", disp->socket);

	*dispp = disp;

	return (ISC_R_SUCCESS);

	/*
	 * Error returns.
	 */
 kill_task:
	isc_task_detach(&disp->task);
 kill_socket:
	isc_socket_detach(&disp->socket);
 deallocate_dispatch:
	dispatch_free(&disp);

	return (result);
}

void
dns_dispatch_attach(dns_dispatch_t *disp, dns_dispatch_t **dispp) {
	REQUIRE(VALID_DISPATCH(disp));
	REQUIRE(dispp != NULL && *dispp == NULL);

	LOCK(&disp->lock);
	disp->refcount++;
	UNLOCK(&disp->lock);

	*dispp = disp;
}

/*
 * It is important to lock the manager while we are deleting the dispatch,
 * since dns_dispatch_getudp will call dispatch_find, which returns to
 * the caller a dispatch but does not attach to it until later.  _getudp
 * locks the manager, however, so locking it here will keep us from attaching
 * to a dispatcher that is in the process of going away.
 */
void
dns_dispatch_detach(dns_dispatch_t **dispp) {
	dns_dispatch_t *disp;
	isc_boolean_t killit;

	REQUIRE(dispp != NULL && VALID_DISPATCH(*dispp));

	disp = *dispp;
	*dispp = NULL;

	LOCK(&disp->lock);

	INSIST(disp->refcount > 0);
	disp->refcount--;
	killit = ISC_FALSE;
	if (disp->refcount == 0) {
		if (disp->recv_pending > 0)
			isc_socket_cancel(disp->socket, disp->task,
					  ISC_SOCKCANCEL_RECV);
		disp->shutting_down = 1;
	}

	dispatch_log(disp, LVL(90), "detach: refcount %d", disp->refcount);

	killit = destroy_disp_ok(disp);
	UNLOCK(&disp->lock);
	if (killit)
		isc_task_send(disp->task, &disp->ctlevent);
}

isc_result_t
dns_dispatch_addresponse(dns_dispatch_t *disp, isc_sockaddr_t *dest,
			 isc_task_t *task, isc_taskaction_t action, void *arg,
			 dns_messageid_t *idp, dns_dispentry_t **resp)
{
	dns_dispentry_t *res;
	unsigned int bucket;
	dns_messageid_t id;
	int i;
	isc_boolean_t ok;
	dns_qid_t *qid;

	REQUIRE(VALID_DISPATCH(disp));
	REQUIRE(task != NULL);
	REQUIRE(dest != NULL);
	REQUIRE(resp != NULL && *resp == NULL);
	REQUIRE(idp != NULL);

	LOCK(&disp->lock);

	if (disp->shutting_down == 1) {
		UNLOCK(&disp->lock);
		return (ISC_R_SHUTTINGDOWN);
	}

	if (disp->requests >= disp->maxrequests) {
		UNLOCK(&disp->lock);
		return (ISC_R_QUOTA);
	}

	/*
	 * Try somewhat hard to find an unique ID.
	 */
	qid = DNS_QID(disp);
	LOCK(&qid->lock);
	id = dns_randomid(qid);
	bucket = dns_hash(qid, dest, id);
	ok = ISC_FALSE;
	for (i = 0; i < 64; i++) {
		if (bucket_search(qid, dest, id, bucket) == NULL) {
			ok = ISC_TRUE;
			break;
		}
		id += qid->qid_increment;
		id &= 0x0000ffff;
		bucket = dns_hash(qid, dest, id);
	}

	if (!ok) {
		UNLOCK(&qid->lock);
		UNLOCK(&disp->lock);
		return (ISC_R_NOMORE);
	}

	res = isc_mempool_get(disp->mgr->rpool);
	if (res == NULL) {
		UNLOCK(&qid->lock);
		UNLOCK(&disp->lock);
		return (ISC_R_NOMEMORY);
	}

	disp->refcount++;
	disp->requests++;
	res->task = NULL;
	isc_task_attach(task, &res->task);
	res->disp = disp;
	res->id = id;
	res->bucket = bucket;
	res->host = *dest;
	res->action = action;
	res->arg = arg;
	res->item_out = ISC_FALSE;
	ISC_LIST_INIT(res->items);
	ISC_LINK_INIT(res, link);
	res->magic = RESPONSE_MAGIC;
	ISC_LIST_APPEND(qid->qid_table[bucket], res, link);
	UNLOCK(&qid->lock);

	request_log(disp, res, LVL(90),
		    "attached to task %p", res->task);

	if (((disp->attributes & DNS_DISPATCHATTR_UDP) != 0) ||
	    ((disp->attributes & DNS_DISPATCHATTR_CONNECTED) != 0))
		startrecv(disp);

	UNLOCK(&disp->lock);

	*idp = id;
	*resp = res;

	return (ISC_R_SUCCESS);
}

void
dns_dispatch_starttcp(dns_dispatch_t *disp) {

	REQUIRE(VALID_DISPATCH(disp));

	dispatch_log(disp, LVL(90), "starttcp %p", disp->task);

	LOCK(&disp->lock);
	disp->attributes |= DNS_DISPATCHATTR_CONNECTED;
	startrecv(disp);
	UNLOCK(&disp->lock);
}

void
dns_dispatch_removeresponse(dns_dispentry_t **resp,
			    dns_dispatchevent_t **sockevent)
{
	dns_dispatchmgr_t *mgr;
	dns_dispatch_t *disp;
	dns_dispentry_t *res;
	dns_dispatchevent_t *ev;
	unsigned int bucket;
	isc_boolean_t killit;
	unsigned int n;
	isc_eventlist_t events;
	dns_qid_t *qid;

	REQUIRE(resp != NULL);
	REQUIRE(VALID_RESPONSE(*resp));

	res = *resp;
	*resp = NULL;

	disp = res->disp;
	REQUIRE(VALID_DISPATCH(disp));
	mgr = disp->mgr;
	REQUIRE(VALID_DISPATCHMGR(mgr));

	qid = DNS_QID(disp);

	if (sockevent != NULL) {
		REQUIRE(*sockevent != NULL);
		ev = *sockevent;
		*sockevent = NULL;
	} else {
		ev = NULL;
	}

	LOCK(&disp->lock);

	INSIST(disp->requests > 0);
	disp->requests--;
	INSIST(disp->refcount > 0);
	disp->refcount--;
	killit = ISC_FALSE;
	if (disp->refcount == 0) {
		if (disp->recv_pending > 0)
			isc_socket_cancel(disp->socket, disp->task,
					  ISC_SOCKCANCEL_RECV);
		disp->shutting_down = 1;
	}

	bucket = res->bucket;

	LOCK(&qid->lock);
	ISC_LIST_UNLINK(qid->qid_table[bucket], res, link);
	UNLOCK(&qid->lock);

	if (ev == NULL && res->item_out) {
		/*
		 * We've posted our event, but the caller hasn't gotten it
		 * yet.  Take it back.
		 */
		ISC_LIST_INIT(events);
		n = isc_task_unsend(res->task, res, DNS_EVENT_DISPATCH,
				    NULL, &events);
		/*
		 * We had better have gotten it back.
		 */
		INSIST(n == 1);
		ev = (dns_dispatchevent_t *)ISC_LIST_HEAD(events);
	}

	if (ev != NULL) {
		REQUIRE(res->item_out == ISC_TRUE);
		res->item_out = ISC_FALSE;
		if (ev->buffer.base != NULL)
			free_buffer(disp, ev->buffer.base, ev->buffer.length);
		free_event(disp, ev);
	}

	request_log(disp, res, LVL(90), "detaching from task %p", res->task);
	isc_task_detach(&res->task);

	/*
	 * Free any buffered requests as well
	 */
	ev = ISC_LIST_HEAD(res->items);
	while (ev != NULL) {
		ISC_LIST_UNLINK(res->items, ev, ev_link);
		if (ev->buffer.base != NULL)
			free_buffer(disp, ev->buffer.base, ev->buffer.length);
		free_event(disp, ev);
		ev = ISC_LIST_HEAD(res->items);
	}
	res->magic = 0;
	isc_mempool_put(disp->mgr->rpool, res);
	if (disp->shutting_down == 1)
		do_cancel(disp);
	else
		startrecv(disp);

	killit = destroy_disp_ok(disp);
	UNLOCK(&disp->lock);
	if (killit)
		isc_task_send(disp->task, &disp->ctlevent);
}

static void
do_cancel(dns_dispatch_t *disp) {
	dns_dispatchevent_t *ev;
	dns_dispentry_t *resp;
	dns_qid_t *qid;

	if (disp->shutdown_out == 1)
		return;

	qid = DNS_QID(disp);

	/*
	 * Search for the first response handler without packets outstanding.
	 */
	LOCK(&qid->lock);
	for (resp = linear_first(qid);
	     resp != NULL && resp->item_out != ISC_FALSE;
	     /* Empty. */)
		resp = linear_next(qid, resp);
	/*
	 * No one to send the cancel event to, so nothing to do.
	 */
	if (resp == NULL)
		goto unlock;

	/*
	 * Send the shutdown failsafe event to this resp.
	 */
	ev = disp->failsafe_ev;
	ISC_EVENT_INIT(ev, sizeof(*ev), 0, NULL, DNS_EVENT_DISPATCH,
		       resp->action, resp->arg, resp, NULL, NULL);
	ev->result = disp->shutdown_why;
	ev->buffer.base = NULL;
	ev->buffer.length = 0;
	disp->shutdown_out = 1;
	request_log(disp, resp, LVL(10),
		    "cancel: failsafe event %p -> task %p",
		    ev, resp->task);
	resp->item_out = ISC_TRUE;
	isc_task_send(resp->task, ISC_EVENT_PTR(&ev));
 unlock:
	UNLOCK(&qid->lock);
}

isc_socket_t *
dns_dispatch_getsocket(dns_dispatch_t *disp) {
	REQUIRE(VALID_DISPATCH(disp));

	return (disp->socket);
}

isc_result_t
dns_dispatch_getlocaladdress(dns_dispatch_t *disp, isc_sockaddr_t *addrp) {

	REQUIRE(VALID_DISPATCH(disp));
	REQUIRE(addrp != NULL);

	if (disp->socktype == isc_sockettype_udp) {
		*addrp = disp->local;
		return (ISC_R_SUCCESS);
	}
	return (ISC_R_NOTIMPLEMENTED);
}

void
dns_dispatch_cancel(dns_dispatch_t *disp) {
	REQUIRE(VALID_DISPATCH(disp));

	LOCK(&disp->lock);

	if (disp->shutting_down == 1) {
		UNLOCK(&disp->lock);
		return;
	}

	disp->shutdown_why = ISC_R_CANCELED;
	disp->shutting_down = 1;
	do_cancel(disp);

	UNLOCK(&disp->lock);

	return;
}

void
dns_dispatch_changeattributes(dns_dispatch_t *disp,
			      unsigned int attributes, unsigned int mask)
{
	REQUIRE(VALID_DISPATCH(disp));

	/* XXXMLG
	 * Should check for valid attributes here!
	 */

	LOCK(&disp->lock);

	if ((mask & DNS_DISPATCHATTR_NOLISTEN) != 0) {
		if ((disp->attributes & DNS_DISPATCHATTR_NOLISTEN) != 0 &&
		    (attributes & DNS_DISPATCHATTR_NOLISTEN) == 0) {
			disp->attributes &= ~DNS_DISPATCHATTR_NOLISTEN;
			startrecv(disp);
		} else if ((disp->attributes & DNS_DISPATCHATTR_NOLISTEN)
			   == 0 &&
			   (attributes & DNS_DISPATCHATTR_NOLISTEN) != 0) {
			disp->attributes |= DNS_DISPATCHATTR_NOLISTEN;
			if (disp->recv_pending != 0)
				isc_socket_cancel(disp->socket, disp->task,
						  ISC_SOCKCANCEL_RECV);
		}
	}

	disp->attributes &= ~mask;
	disp->attributes |= (attributes & mask);
	UNLOCK(&disp->lock);
}

void
dns_dispatch_importrecv(dns_dispatch_t *disp, isc_event_t *event) {
	void *buf;
	isc_socketevent_t *sevent, *newsevent;

	REQUIRE(VALID_DISPATCH(disp));
	REQUIRE((disp->attributes & DNS_DISPATCHATTR_NOLISTEN) != 0);
	REQUIRE(event != NULL);

	sevent = (isc_socketevent_t *)event;

	INSIST(sevent->n <= disp->mgr->buffersize);
	newsevent = (isc_socketevent_t *)
		    isc_event_allocate(disp->mgr->mctx, NULL,
				      DNS_EVENT_IMPORTRECVDONE, udp_recv,
				      disp, sizeof(isc_socketevent_t));
	if (newsevent == NULL)
		return;

	buf = allocate_udp_buffer(disp);
	if (buf == NULL) {
		isc_event_free(ISC_EVENT_PTR(&newsevent));
		return;
	}
	memcpy(buf, sevent->region.base, sevent->n);
	newsevent->region.base = buf;
	newsevent->region.length = disp->mgr->buffersize;
	newsevent->n = sevent->n;
	newsevent->result = sevent->result;
	newsevent->address = sevent->address;
	newsevent->timestamp = sevent->timestamp;
	newsevent->pktinfo = sevent->pktinfo;
	newsevent->attributes = sevent->attributes;
	
	isc_task_send(disp->task, ISC_EVENT_PTR(&newsevent));
}

#if 0
void
dns_dispatchmgr_dump(dns_dispatchmgr_t *mgr) {
	dns_dispatch_t *disp;
	char foo[1024];

	disp = ISC_LIST_HEAD(mgr->list);
	while (disp != NULL) {
		isc_sockaddr_format(&disp->local, foo, sizeof(foo));
		printf("\tdispatch %p, addr %s\n", disp, foo);
		disp = ISC_LIST_NEXT(disp, link);
	}
}
#endif
