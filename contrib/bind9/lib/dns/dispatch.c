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
#include <sys/types.h>
#include <unistd.h>

#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/time.h>
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

typedef struct dns_nsid {
	isc_uint16_t	nsid_state;
	isc_uint16_t	*nsid_vtable;
	isc_uint16_t	*nsid_pool;
	isc_uint16_t	nsid_a1, nsid_a2, nsid_a3;
	isc_uint16_t	nsid_c1, nsid_c2, nsid_c3;
	isc_uint16_t	nsid_state2;
	isc_boolean_t	nsid_usepool;
} dns_nsid_t;

typedef struct dns_qid {
	unsigned int	magic;
	unsigned int	qid_nbuckets;	/* hash table size */
	unsigned int	qid_increment;	/* id increment on collision */
	isc_mutex_t	lock;
	dns_nsid_t	nsid;
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
static dns_messageid_t dns_randomid(dns_nsid_t *);
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
				 unsigned int increment, isc_boolean_t usepool,
				 dns_qid_t **qidp);
static void qid_destroy(isc_mem_t *mctx, dns_qid_t **qidp);
static isc_uint16_t nsid_next(dns_nsid_t *nsid);
static isc_result_t nsid_init(isc_mem_t *mctx, dns_nsid_t *nsid, isc_boolean_t usepool);
static void nsid_destroy(isc_mem_t *mctx, dns_nsid_t *nsid);

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

/*
 * Return an unpredictable message ID.
 */
static dns_messageid_t
dns_randomid(dns_nsid_t *nsid) {
	isc_uint32_t id;

	id = nsid_next(nsid);

	return ((dns_messageid_t)id);
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

	dns_dispatch_hash(&ev->timestamp, sizeof(&ev->timestamp));
	dns_dispatch_hash(ev->region.base, ev->region.length);

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

	dns_dispatch_hash(tcpmsg->buffer.base, tcpmsg->buffer.length);

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

	result = qid_allocate(mgr, buckets, increment, ISC_TRUE, &mgr->qid);
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
	     unsigned int increment, isc_boolean_t usepool, dns_qid_t **qidp)
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

	if (nsid_init(mgr->mctx, &qid->nsid, usepool) != ISC_R_SUCCESS) {
		isc_mem_put(mgr->mctx, qid->qid_table,
			    buckets * sizeof(dns_displist_t));
		isc_mem_put(mgr->mctx, qid, sizeof(*qid));
		return (ISC_R_NOMEMORY);
	}

	if (isc_mutex_init(&qid->lock) != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__, "isc_mutex_init failed");
		nsid_destroy(mgr->mctx, &qid->nsid);
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
	nsid_destroy(mctx, &qid->nsid);
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

	result = qid_allocate(mgr, buckets, increment, ISC_FALSE, &disp->qid);
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
	id = dns_randomid(&qid->nsid);
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

/*
 * Allow the user to pick one of two ID randomization algorithms.
 *
 * The first algorithm is an adaptation of the sequence shuffling
 * algorithm discovered by Carter Bays and S. D. Durham [ACM Trans. Math.
 * Software 2 (1976), 59-64], as documented as Algorithm B in Chapter
 * 3.2.2 in Volume 2 of Knuth's "The Art of Computer Programming".  We use
 * a randomly selected linear congruential random number generator with a
 * modulus of 2^16, whose increment is a randomly picked odd number, and
 * whose multiplier is picked from a set which meets the following
 * criteria:
 *     Is of the form 8*n+5, which ensures "high potency" according to
 *     principle iii in the summary chapter 3.6.  This form also has a
 *     gcd(a-1,m) of 4 which is good according to principle iv.
 *
 *     Is between 0.01 and 0.99 times the modulus as specified by
 *     principle iv.
 *
 *     Passes the spectral test "with flying colors" (ut >= 1) in
 *     dimensions 2 through 6 as calculated by Algorithm S in Chapter
 *     3.3.4 and the ratings calculated by formula 35 in section E.
 *
 *     Of the multipliers that pass this test, pick the set that is
 *     best according to the theoretical bounds of the serial
 *     correlation test.  This was calculated using a simplified
 *     version of Knuth's Theorem K in Chapter 3.3.3.
 *
 * These criteria may not be important for this use, but we might as well
 * pick from the best generators since there are so many possible ones and
 * we don't have that many random bits to do the picking.
 *
 * We use a modulus of 2^16 instead of something bigger so that we will
 * tend to cycle through all the possible IDs before repeating any,
 * however the shuffling will perturb this somewhat.  Theoretically there
 * is no minimimum interval between two uses of the same ID, but in
 * practice it seems to be >64000.
 *
 * Our adaptatation  of Algorithm B mixes the hash state which has
 * captured various random events into the shuffler to perturb the
 * sequence.
 *
 * One disadvantage of this algorithm is that if the generator parameters
 * were to be guessed, it would be possible to mount a limited brute force
 * attack on the ID space since the IDs are only shuffled within a limited
 * range.
 *
 * The second algorithm uses the same random number generator to populate
 * a pool of 65536 IDs.  The hash state is used to pick an ID from a window
 * of 4096 IDs in this pool, then the chosen ID is swapped with the ID
 * at the beginning of the window and the window position is advanced.
 * This means that the interval between uses of the ID will be no less
 * than 65536-4096.  The ID sequence in the pool will become more random
 * over time.
 *
 * For both algorithms, two more linear congruential random number generators
 * are selected.  The ID from the first part of algorithm is used to seed
 * the first of these generators, and its output is used to seed the second.
 * The strategy is use these generators as 1 to 1 hashes to obfuscate the
 * properties of the generator used in the first part of either algorithm.
 *
 * The first algorithm may be suitable for use in a client resolver since
 * its memory requirements are fairly low and it's pretty random out of
 * the box.  It is somewhat succeptible to a limited brute force attack,
 * so the second algorithm is probably preferable for a longer running
 * program that issues a large number of queries and has time to randomize
 * the pool.
 */

#define NSID_SHUFFLE_TABLE_SIZE 100 /* Suggested by Knuth */
/*
 * Pick one of the next 4096 IDs in the pool.
 * There is a tradeoff here between randomness and how often and ID is reused.
 */
#define NSID_LOOKAHEAD 4096     /* Must be a power of 2 */
#define NSID_SHUFFLE_ONLY 1     /* algorithm 1 */
#define NSID_USE_POOL 2         /* algorithm 2 */
#define NSID_HASHSHIFT       3
#define NSID_HASHROTATE(v) \
        (((v) << NSID_HASHSHIFT) | ((v) >> ((sizeof(v) * 8) - NSID_HASHSHIFT)))

static isc_uint32_t	nsid_hash_state;

/*
 * Keep a running hash of various bits of data that we'll use to
 * stir the ID pool or perturb the ID generator
 */
static void
nsid_hash(void *data, size_t len) {
	unsigned char *p = data;
	/*
	 * Hash function similar to the one we use for hashing names.
	 * We don't fold case or toss the upper bit here, though.
	 * This hash doesn't do much interesting when fed binary zeros,
	 * so there may be a better hash function.
	 * This function doesn't need to be very strong since we're
	 * only using it to stir the pool, but it should be reasonably
	 * fast.
	 */
	/*
	 * We don't care about locking access to nsid_hash_state.
	 * In fact races make the result even more non deteministic.
	 */
	while (len-- > 0U) {
		nsid_hash_state = NSID_HASHROTATE(nsid_hash_state);
		nsid_hash_state += *p++;
	}
}

/*
 * Table of good linear congruential multipliers for modulus 2^16
 * in order of increasing serial correlation bounds (so trim from
 * the end).
 */
static const isc_uint16_t nsid_multiplier_table[] = {
	17565, 25013, 11733, 19877, 23989, 23997, 24997, 25421,
	26781, 27413, 35901, 35917, 35973, 36229, 38317, 38437,
	39941, 40493, 41853, 46317, 50581, 51429, 53453, 53805,
	11317, 11789, 12045, 12413, 14277, 14821, 14917, 18989,
	19821, 23005, 23533, 23573, 23693, 27549, 27709, 28461,
	29365, 35605, 37693, 37757, 38309, 41285, 45261, 47061,
	47269, 48133, 48597, 50277, 50717, 50757, 50805, 51341,
	51413, 51581, 51597, 53445, 11493, 14229, 20365, 20653,
	23485, 25541, 27429, 29421, 30173, 35445, 35653, 36789,
	36797, 37109, 37157, 37669, 38661, 39773, 40397, 41837,
	41877, 45293, 47277, 47845, 49853, 51085, 51349, 54085,
	56933,  8877,  8973,  9885, 11365, 11813, 13581, 13589,
	13613, 14109, 14317, 15765, 15789, 16925, 17069, 17205,
	17621, 17941, 19077, 19381, 20245, 22845, 23733, 24869,
	25453, 27213, 28381, 28965, 29245, 29997, 30733, 30901,
	34877, 35485, 35613, 36133, 36661, 36917, 38597, 40285,
	40693, 41413, 41541, 41637, 42053, 42349, 45245, 45469,
	46493, 48205, 48613, 50861, 51861, 52877, 53933, 54397,
	55669, 56453, 56965, 58021,  7757,  7781,  8333,  9661,
	12229, 14373, 14453, 17549, 18141, 19085, 20773, 23701,
	24205, 24333, 25261, 25317, 27181, 30117, 30477, 34757,
	34885, 35565, 35885, 36541, 37957, 39733, 39813, 41157,
	41893, 42317, 46621, 48117, 48181, 49525, 55261, 55389,
	56845,  7045,  7749,  7965,  8469,  9133,  9549,  9789,
	10173, 11181, 11285, 12253, 13453, 13533, 13757, 14477,
	15053, 16901, 17213, 17269, 17525, 17629, 18605, 19013,
	19829, 19933, 20069, 20093, 23261, 23333, 24949, 25309,
	27613, 28453, 28709, 29301, 29541, 34165, 34413, 37301,
	37773, 38045, 38405, 41077, 41781, 41925, 42717, 44437,
	44525, 44613, 45933, 45941, 47077, 50077, 50893, 52117,
	 5293, 55069, 55989, 58125, 59205,  6869, 14685, 15453,
	16821, 17045, 17613, 18437, 21029, 22773, 22909, 25445,
	25757, 26541, 30709, 30909, 31093, 31149, 37069, 37725,
	37925, 38949, 39637, 39701, 40765, 40861, 42965, 44813,
	45077, 45733, 47045, 50093, 52861, 52957, 54181, 56325,
	56365, 56381, 56877, 57013,  5741, 58101, 58669,  8613,
	10045, 10261, 10653, 10733, 11461, 12261, 14069, 15877,
	17757, 21165, 23885, 24701, 26429, 26645, 27925, 28765,
	29197, 30189, 31293, 39781, 39909, 40365, 41229, 41453,
	41653, 42165, 42365, 47421, 48029, 48085, 52773,  5573,
	57037, 57637, 58341, 58357, 58901,  6357,  7789,  9093,
	10125, 10709, 10765, 11957, 12469, 13437, 13509, 14773,
	15437, 15773, 17813, 18829, 19565, 20237, 23461, 23685,
	23725, 23941, 24877, 25461, 26405, 29509, 30285, 35181,
	37229, 37893, 38565, 40293, 44189, 44581, 45701, 47381,
	47589, 48557,  4941, 51069,  5165, 52797, 53149,  5341,
	56301, 56765, 58581, 59493, 59677,  6085,  6349,  8293,
	 8501,  8517, 11597, 11709, 12589, 12693, 13517, 14909,
	17397, 18085, 21101, 21269, 22717, 25237, 25661, 29189,
	30101, 31397, 33933, 34213, 34661, 35533, 36493, 37309,
	40037,  4189, 42909, 44309, 44357, 44389,  4541, 45461,
	46445, 48237, 54149, 55301, 55853, 56621, 56717, 56901,
	 5813, 58437, 12493, 15365, 15989, 17829, 18229, 19341,
	21013, 21357, 22925, 24885, 26053, 27581, 28221, 28485,
	30605, 30613, 30789, 35437, 36285, 37189,  3941, 41797,
	 4269, 42901, 43293, 44645, 45221, 46893,  4893, 50301,
	50325,  5189, 52109, 53517, 54053, 54485,  5525, 55949,
	56973, 59069, 59421, 60733, 61253,  6421,  6701,  6709,
	 7101,  8669, 15797, 19221, 19837, 20133, 20957, 21293,
	21461, 22461, 29085, 29861, 30869, 34973, 36469, 37565,
	38125, 38829, 39469, 40061, 40117, 44093, 47429, 48341,
	50597, 51757,  5541, 57629, 58405, 59621, 59693, 59701,
	61837,  7061, 10421, 11949, 15405, 20861, 25397, 25509,
	25893, 26037, 28629, 28869, 29605, 30213, 34205, 35637,
	36365, 37285,  3773, 39117,  4021, 41061, 42653, 44509,
	 4461, 44829,  4725,  5125, 52269, 56469, 59085,  5917,
	60973,  8349, 17725, 18637, 19773, 20293, 21453, 22533,
	24285, 26333, 26997, 31501, 34541, 34805, 37509, 38477,
	41333, 44125, 46285, 46997, 47637, 48173,  4925, 50253,
	50381, 50917, 51205, 51325, 52165, 52229,  5253,  5269,
	53509, 56253, 56341,  5821, 58373, 60301, 61653, 61973,
	62373,  8397, 11981, 14341, 14509, 15077, 22261, 22429,
	24261, 28165, 28685, 30661, 34021, 34445, 39149,  3917,
	43013, 43317, 44053, 44101,  4533, 49541, 49981,  5277,
	54477, 56357, 57261, 57765, 58573, 59061, 60197, 61197,
	62189,  7725,  8477,  9565, 10229, 11437, 14613, 14709,
	16813, 20029, 20677, 31445,  3165, 31957,  3229, 33541,
	36645,  3805, 38973,  3965,  4029, 44293, 44557, 46245,
	48917,  4909, 51749, 53709, 55733, 56445,  5925,  6093,
	61053, 62637,  8661,  9109, 10821, 11389, 13813, 14325,
	15501, 16149, 18845, 22669, 26437, 29869, 31837, 33709,
	33973, 34173,  3677,  3877,  3981, 39885, 42117,  4421,
	44221, 44245, 44693, 46157, 47309,  5005, 51461, 52037,
	55333, 55693, 56277, 58949,  6205, 62141, 62469,  6293,
	10101, 12509, 14029, 17997, 20469, 21149, 25221, 27109,
	 2773,  2877, 29405, 31493, 31645,  4077, 42005, 42077,
	42469, 42501, 44013, 48653, 49349,  4997, 50101, 55405,
	56957, 58037, 59429, 60749, 61797, 62381, 62837,  6605,
	10541, 23981, 24533,  2701, 27333, 27341, 31197, 33805,
	 3621, 37381,  3749,  3829, 38533, 42613, 44381, 45901,
	48517, 51269, 57725, 59461, 60045, 62029, 13805, 14013,
	15461, 16069, 16157, 18573,  2309, 23501, 28645,  3077,
	31541, 36357, 36877,  3789, 39429, 39805, 47685, 47949,
	49413,  5485, 56757, 57549, 57805, 58317, 59549, 62213,
	62613, 62853, 62933,  8909, 12941, 16677, 20333, 21541,
	24429, 26077, 26421,  2885, 31269, 33381,  3661, 40925,
	42925, 45173,  4525,  4709, 53133, 55941, 57413, 57797,
	62125, 62237, 62733,  6773, 12317, 13197, 16533, 16933,
	18245,  2213,  2477, 29757, 33293, 35517, 40133, 40749,
	 4661, 49941, 62757,  7853,  8149,  8573, 11029, 13421,
	21549, 22709, 22725, 24629,  2469, 26125,  2669, 34253,
	36709, 41013, 45597, 46637, 52285, 52333, 54685, 59013,
	60997, 61189, 61981, 62605, 62821,  7077,  7525,  8781,
	10861, 15277,  2205, 22077, 28517, 28949, 32109, 33493,
	 4661, 49941, 62757,  7853,  8149,  8573, 11029, 13421,
	21549, 22709, 22725, 24629,  2469, 26125,  2669, 34253,
	36709, 41013, 45597, 46637, 52285, 52333, 54685, 59013,
	60997, 61189, 61981, 62605, 62821,  7077,  7525,  8781,
	10861, 15277,  2205, 22077, 28517, 28949, 32109, 33493,
	 3685, 39197, 39869, 42621, 44997, 48565,  5221, 57381,
	61749, 62317, 63245, 63381, 23149,  2549, 28661, 31653,
	33885, 36341, 37053, 39517, 42805, 45853, 48997, 59349,
	60053, 62509, 63069,  6525,  1893, 20181,  2365, 24893,
	27397, 31357, 32277, 33357, 34437, 36677, 37661, 43469,
	43917, 50997, 53869,  5653, 13221, 16741, 17893,  2157,
	28653, 31789, 35301, 35821, 61613, 62245, 12405, 14517,
	17453, 18421,  3149,  3205, 40341,  4109, 43941, 46869,
	48837, 50621, 57405, 60509, 62877,  8157, 12933, 12957,
	16501, 19533,  3461, 36829, 52357, 58189, 58293, 63053,
	17109,  1933, 32157, 37701, 59005, 61621, 13029, 15085,
	16493, 32317, 35093,  5061, 51557, 62221, 20765, 24613,
	 2629, 30861, 33197, 33749, 35365, 37933, 40317, 48045,
	56229, 61157, 63797,  7917, 17965,  1917,  1973, 20301,
	 2253, 33157, 58629, 59861, 61085, 63909,  8141,  9221,
	14757,  1581, 21637, 26557, 33869, 34285, 35733, 40933,
	42517, 43501, 53653, 61885, 63805,  7141, 21653, 54973,
	31189, 60061, 60341, 63357, 16045,  2053, 26069, 33997,
	43901, 54565, 63837,  8949, 17909, 18693, 32349, 33125,
	37293, 48821, 49053, 51309, 64037,  7117,  1445, 20405,
	23085, 26269, 26293, 27349, 32381, 33141, 34525, 36461,
	37581, 43525,  4357, 43877,  5069, 55197, 63965,  9845,
	12093,  2197,  2229, 32165, 33469, 40981, 42397,  8749,
	10853,  1453, 18069, 21693, 30573, 36261, 37421, 42533
};

#define NSID_MULT_TABLE_SIZE \
        ((sizeof nsid_multiplier_table)/(sizeof nsid_multiplier_table[0]))
#define NSID_RANGE_MASK (NSID_LOOKAHEAD - 1)
#define NSID_POOL_MASK 0xFFFF /* used to wrap the pool index */
#define NSID_SHUFFLE_ONLY 1
#define NSID_USE_POOL 2

static isc_uint16_t
nsid_next(dns_nsid_t *nsid) {
        isc_uint16_t id, compressed_hash;
	isc_uint16_t j;

        compressed_hash = ((nsid_hash_state >> 16) ^
			   (nsid_hash_state)) & 0xFFFF;

	if (nsid->nsid_usepool) {
	        isc_uint16_t pick;

                pick = compressed_hash & NSID_RANGE_MASK;
		pick = (nsid->nsid_state + pick) & NSID_POOL_MASK;
                id = nsid->nsid_pool[pick];
                if (pick != 0) {
                        /* Swap two IDs to stir the pool */
                        nsid->nsid_pool[pick] =
                                nsid->nsid_pool[nsid->nsid_state];
                        nsid->nsid_pool[nsid->nsid_state] = id;
                }

                /* increment the base pointer into the pool */
                if (nsid->nsid_state == 65535)
                        nsid->nsid_state = 0;
                else
                        nsid->nsid_state++;
	} else {
		/*
		 * This is the original Algorithm B
		 * j = ((u_long) NSID_SHUFFLE_TABLE_SIZE * nsid_state2) >> 16;
		 *
		 * We'll perturb it with some random stuff  ...
		 */
		j = ((isc_uint32_t) NSID_SHUFFLE_TABLE_SIZE *
		     (nsid->nsid_state2 ^ compressed_hash)) >> 16;
		nsid->nsid_state2 = id = nsid->nsid_vtable[j];
		nsid->nsid_state = (((isc_uint32_t) nsid->nsid_a1 * nsid->nsid_state) +
				      nsid->nsid_c1) & 0xFFFF;
		nsid->nsid_vtable[j] = nsid->nsid_state;
	}

        /* Now lets obfuscate ... */
        id = (((isc_uint32_t) nsid->nsid_a2 * id) + nsid->nsid_c2) & 0xFFFF;
        id = (((isc_uint32_t) nsid->nsid_a3 * id) + nsid->nsid_c3) & 0xFFFF;

        return (id);
}

static isc_result_t
nsid_init(isc_mem_t *mctx, dns_nsid_t *nsid, isc_boolean_t usepool) {
        isc_time_t now;
        pid_t mypid;
        isc_uint16_t a1ndx, a2ndx, a3ndx, c1ndx, c2ndx, c3ndx;
        int i;

	isc_time_now(&now);
        mypid = getpid();

        /* Initialize the state */
	memset(nsid, 0, sizeof(*nsid));
        nsid_hash(&now, sizeof now);
        nsid_hash(&mypid, sizeof mypid);

        /*
         * Select our random number generators and initial seed.
         * We could really use more random bits at this point,
         * but we'll try to make a silk purse out of a sows ear ...
         */
        /* generator 1 */
        a1ndx = ((isc_uint32_t) NSID_MULT_TABLE_SIZE *
                 (nsid_hash_state & 0xFFFF)) >> 16;
        nsid->nsid_a1 = nsid_multiplier_table[a1ndx];
        c1ndx = (nsid_hash_state >> 9) & 0x7FFF;
        nsid->nsid_c1 = 2 * c1ndx + 1;

        /* generator 2, distinct from 1 */
        a2ndx = ((isc_uint32_t) (NSID_MULT_TABLE_SIZE - 1) *
                 ((nsid_hash_state >> 10) & 0xFFFF)) >> 16;
        if (a2ndx >= a1ndx)
                a2ndx++;
        nsid->nsid_a2 = nsid_multiplier_table[a2ndx];
        c2ndx = nsid_hash_state % 32767;
        if (c2ndx >= c1ndx)
                c2ndx++;
        nsid->nsid_c2 = 2*c2ndx + 1;

        /* generator 3, distinct from 1 and 2 */
        a3ndx = ((isc_uint32_t) (NSID_MULT_TABLE_SIZE - 2) *
                 ((nsid_hash_state >> 20) & 0xFFFF)) >> 16;
        if (a3ndx >= a1ndx || a3ndx >= a2ndx)
                a3ndx++;
        if (a3ndx >= a1ndx && a3ndx >= a2ndx)
                a3ndx++;
        nsid->nsid_a3 = nsid_multiplier_table[a3ndx];
        c3ndx = nsid_hash_state % 32766;
        if (c3ndx >= c1ndx || c3ndx >= c2ndx)
                c3ndx++;
        if (c3ndx >= c1ndx && c3ndx >= c2ndx)
                c3ndx++;
        nsid->nsid_c3 = 2*c3ndx + 1;

        nsid->nsid_state =
		((nsid_hash_state >> 16) ^ (nsid_hash_state)) & 0xFFFF;

	nsid->nsid_usepool = usepool;
	if (nsid->nsid_usepool) {
                nsid->nsid_pool = isc_mem_get(mctx, 0x10000 * sizeof(isc_uint16_t));
		if (nsid->nsid_pool == NULL)
			return (ISC_R_NOMEMORY);
                for (i = 0; ; i++) {
                        nsid->nsid_pool[i] = nsid->nsid_state;
                        nsid->nsid_state =
				 (((u_long) nsid->nsid_a1 * nsid->nsid_state) +
				   nsid->nsid_c1) & 0xFFFF;
                        if (i == 0xFFFF)
                                break;
                }
	} else {
		nsid->nsid_vtable = isc_mem_get(mctx, NSID_SHUFFLE_TABLE_SIZE *
						(sizeof(isc_uint16_t)) );
		if (nsid->nsid_vtable == NULL)
			return (ISC_R_NOMEMORY);

		for (i = 0; i < NSID_SHUFFLE_TABLE_SIZE; i++) {
			nsid->nsid_vtable[i] = nsid->nsid_state;
			nsid->nsid_state =
				   (((isc_uint32_t) nsid->nsid_a1 * nsid->nsid_state) +
					 nsid->nsid_c1) & 0xFFFF;
		}
		nsid->nsid_state2 = nsid->nsid_state;
	} 
	return (ISC_R_SUCCESS);
}

static void
nsid_destroy(isc_mem_t *mctx, dns_nsid_t *nsid) {
	if (nsid->nsid_usepool)
		isc_mem_put(mctx, nsid->nsid_pool,
			    0x10000 * sizeof(isc_uint16_t));
	else
		isc_mem_put(mctx, nsid->nsid_vtable,
			    NSID_SHUFFLE_TABLE_SIZE * (sizeof(isc_uint16_t)) );
	memset(nsid, 0, sizeof(*nsid));
}

void
dns_dispatch_hash(void *data, size_t len) {
	nsid_hash(data, len);
}
