#include <net/drbr.h>

SYSCTL_DECL(_net_link);

uint8_t set_up_drbr_depth=0;
uint32_t drbr_max_priority=DRBR_MAXQ_DEFAULT-1;
uint32_t drbr_queue_depth=DRBR_MIN_DEPTH;
uint32_t drbr_maxq=DRBR_MAXQ_DEFAULT;

TUNABLE_INT("net.link.drbr_maxq", &drbr_maxq);

SYSCTL_NODE(_net, OID_AUTO, drbr, CTLFLAG_RD, 0, "DRBR Parameters");

SYSCTL_INT(_net_drbr, OID_AUTO, drbr_maxq, CTLFLAG_RDTUN,
    &drbr_maxq, 0, "max number of priority queues per interface");
SYSCTL_INT(_net_drbr, OID_AUTO, drbr_queue_depth, CTLFLAG_RD,
    &drbr_queue_depth, 0, "Queue length configed via ifqmaxlen");
SYSCTL_INT(_net_drbr, OID_AUTO, drbr_max_priority, CTLFLAG_RD,
    &drbr_max_priority, 0, "Queue length configed via ifqmaxlen");

struct drbr_ring *
drbr_alloc(struct malloc_type *type, int flags, struct mtx *tmtx)
{
	struct drbr_ring *rng;
	int i;
	if (set_up_drbr_depth == 0) {
		drbr_max_priority = drbr_maxq-1;
		set_up_drbr_depth = 1;
		drbr_queue_depth = 1 << ((fls(ifqmaxlen)-1));
		if (drbr_queue_depth < DRBR_MIN_DEPTH) {
			drbr_queue_depth = DRBR_MIN_DEPTH;
		}
	}
	rng = (struct drbr_ring *)malloc(sizeof(struct drbr_ring), type, flags);
	if (rng == NULL) {
		return(NULL);
	}
	memset(rng, 0, sizeof(struct drbr_ring));
	DRBR_LOCK_INIT(rng);
	rng->re = (struct drbr_ring_entry *)malloc((sizeof(struct drbr_ring_entry)*drbr_maxq), 
			 type, flags);
	if (rng->re == NULL) {
		free(rng, type);
		return(NULL);
	}
	memset(rng->re, 0, (sizeof(struct drbr_ring_entry) * drbr_maxq));
	/* Ok get the queues */
	for (i=0; i<drbr_maxq; i++) {
		rng->re[i].re_qs = buf_ring_alloc(drbr_queue_depth, type, flags, tmtx);
		if (rng->re[i].re_qs == NULL) {
			goto out_err;
		}
	}
	rng->lowq_with_data = 0xffffffff;
	return(rng);
out_err:
	for(i=0; i<drbr_maxq; i++) {
		if (rng->re[i].re_qs) {
			free(rng->re[i].re_qs, type);
		}
	}
	free(rng->re, type);
	free(rng, type);
	return (NULL);
}

#define PRIO_NAME_LEN 32
void 
drbr_add_sysctl_stats(device_t dev, struct sysctl_oid_list *queue_list, 
		      struct drbr_ring *rng)
{
	int i;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *prio_node;
	struct sysctl_oid_list *prio_list;
	char namebuf[PRIO_NAME_LEN];

	if (rng == NULL)
		/* TSNH */
		return;
	for (i=0; i<drbr_maxq; i++) {
		snprintf(namebuf, PRIO_NAME_LEN, "prio%d", i);
		
		prio_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO, namebuf,
					    CTLFLAG_RD, NULL, "Prioity Info");
		prio_list = SYSCTL_CHILDREN(prio_node);
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "packets_sent",
				CTLFLAG_RD, &rng->re[i].re_cnt_sent,
				"Packets Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "bytes_sent",
				CTLFLAG_RD, &rng->re[i].re_bytecnt_sent,
				"Bytes Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_packets",
				CTLFLAG_RD, &rng->re[i].re_drop_cnt,
				"Packets Dropped");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_bytes",
				CTLFLAG_RD, &rng->re[i].re_bytedrop_cnt,
				"Bytes Dropped");
		SYSCTL_ADD_UINT(ctx, prio_list, OID_AUTO, "on_queue_now",
				CTLFLAG_RD, &rng->re[i].re_cnt, 0,
				"Current Queue Size");

	}

}

u_long
drbr_get_dropcnt(struct drbr_ring *rng)
{
	u_long total;
	int i;

	total = 0;
	for (i=0; i<drbr_maxq; i++) {
		total += rng->re[i].re_drop_cnt;
	}
	return (total);
}

void 
drbr_add_sysctl_stats_nodev(struct sysctl_oid_list *queue_list, 
			    struct sysctl_ctx_list *ctx,
			    struct drbr_ring *rng)
{
	int i;
	struct sysctl_oid *prio_node;
	struct sysctl_oid_list *prio_list;
	char namebuf[PRIO_NAME_LEN];

	if (rng == NULL)
		return;
	for (i=0; i<drbr_maxq; i++) {
		snprintf(namebuf, PRIO_NAME_LEN, "prio%d", i);
		prio_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO, namebuf,
					CTLFLAG_RD, NULL, "Prioity Info");
		prio_list = SYSCTL_CHILDREN(prio_node);
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "packets_sent",
				CTLFLAG_RD, &rng->re[i].re_cnt_sent,
				"Packets Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "bytes_sent",
				CTLFLAG_RD, &rng->re[i].re_bytecnt_sent,
				"Bytes Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_packets",
				CTLFLAG_RD, &rng->re[i].re_drop_cnt,
				"Packets Dropped");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_bytes",
				CTLFLAG_RD, &rng->re[i].re_bytedrop_cnt,
				"Bytes Dropped");
		SYSCTL_ADD_UINT(ctx, prio_list, OID_AUTO, "on_queue_now",
				CTLFLAG_RD, &rng->re[i].re_cnt, 0,
				"Current Queue Size");
	}
}

int
drbr_enqueue(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *m)
{	
	int error = 0;
	uint8_t qused;
	uint64_t bytecnt;

#ifdef ALTQ
	if ((ifp != NULL) && 
	    (ALTQ_IS_ENABLED(&ifp->if_snd))) {
		IFQ_ENQUEUE(&ifp->if_snd, m, error);
		return (error);
	}
#endif
	if (m->m_pkthdr.cosqos >= drbr_maxq) {
		/* Lowest priority queue */
		qused = drbr_maxq - 1;
	} else {
		qused = m->m_pkthdr.cosqos;
	}
	bytecnt = m->m_pkthdr.len;
	error = buf_ring_enqueue(rng->re[qused].re_qs, m);
        if (error) {
		m_freem(m);
		atomic_add_long(&rng->re[qused].re_drop_cnt, 1);
		atomic_add_long(&rng->re[qused].re_bytedrop_cnt, bytecnt);
	} else {
		if (qused < rng->lowq_with_data) {
			atomic_clear_int(&rng->lowq_with_data, 0xffffffff);
			atomic_set_int(&rng->lowq_with_data, qused);
		}
		atomic_add_int(&rng->count_on_queues, 1);
		atomic_add_int(&rng->re[qused].re_cnt, 1);
		atomic_add_long(&rng->re[qused].re_cnt_sent, 1);
		atomic_add_long(&rng->re[qused].re_bytecnt_sent, bytecnt);
	}
	return (error);
}

int
drbr_is_on_ring(struct drbr_ring *rng, struct mbuf *m)
{
	int answer = 0; /* No its not by default */
	int i;
	for(i=0; i<drbr_maxq;i++) {
		if (buf_ring_empty(rng->re[i].re_qs))
			continue;
		if (buf_ring_mbufon(rng->re[i].re_qs, m)) {
			answer = 1;
			break;
		}
	}	
	return(answer);
}

void
drbr_putback(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *new, uint8_t qused)
{
	/*
	 * The top of the list needs to be swapped 
	 * for this one.
	 */
	buf_ring_putback_mc(rng->re[qused].re_qs, new);
}

struct mbuf *
drbr_peek(struct ifnet *ifp, struct drbr_ring *rng, uint8_t *qused)
{
	int i;
	struct mbuf *m;

	if (rng->count_on_queues == 0) {
		/* All done now */
		return (NULL);
	}
	if (rng->lowq_with_data == 0xffffffff) {
		rng->lowq_with_data = 0;
	}
	for(i=rng->lowq_with_data; i<drbr_maxq;i++) {
		if (buf_ring_empty(rng->re[i].re_qs))
			continue;
		rng->lowq_with_data = i;
		break;
	}
	if (i >= drbr_maxq) {
		/* Huh? */
		rng->lowq_with_data = 0;
        	for (i=rng->lowq_with_data; i<drbr_maxq;i++) {
	        	if(buf_ring_empty(rng->re[i].re_qs))
		        	continue;
			rng->lowq_with_data = i;
         		break;
        	}
		if (i >= drbr_maxq) {
			/* Really huh? */
			rng->count_on_queues = 0;
			return (NULL);
                }
        }
	*qused = i;
	m = buf_ring_peek(rng->re[i].re_qs);
	return(m);
}

static void
drbr_flush_locked(struct ifnet *ifp, struct drbr_ring *rng)
{
	int i;
	struct mbuf *m;
	if (rng == NULL) {
		return;
	}
	for(i=0; i<drbr_maxq; i++) {
		while ((m = buf_ring_dequeue_mc(rng->re[i].re_qs)) != NULL) {
			atomic_subtract_long(&rng->re[i].re_cnt_sent, 1);
			if (ifp) {
				ifp->if_oerrors++;
			}
			m_freem(m);
		}
		rng->re[i].re_cnt = 0;
	}
	rng->lowq_with_data = 0xffffffff;
	rng->count_on_queues = 0;
}

void
drbr_flush(struct ifnet *ifp, struct drbr_ring *rng)
{
	drbr_flush_locked(ifp, rng);
}

void
drbr_free(struct drbr_ring *rng, struct malloc_type *type)
{
	int i;

	if (rng == NULL) {
		return;
	}
	drbr_flush_locked(NULL, rng);
	for(i=0; i<drbr_maxq; i++) {
		if (rng->re[i].re_qs) {
			buf_ring_free(rng->re[i].re_qs, type);
		}
	}
	DRBR_LOCK_DESTROY(rng);
	free(rng->re, type);
	free(rng, type);
}

struct mbuf *
drbr_dequeue(struct ifnet *ifp, struct drbr_ring *rng)
{
	int i;
	struct mbuf *m;

	if (rng->count_on_queues == 0) {
		return (NULL);
	}
	if (rng->lowq_with_data == 0xffffffff) {
		rng->lowq_with_data = 0;
	}
	for(i=rng->lowq_with_data; i<drbr_maxq;i++) {
		if (buf_ring_empty(rng->re[i].re_qs))
			continue;
		rng->lowq_with_data = i;
		break;
	}
#ifdef INVARIANT
	if (i >= drbr_maxq) {
		/* Nothing on ring from marker up? */
		rng->lowq_with_data = 0;
        	for (i=rng->lowq_with_data; i<drbr_maxq;i++) {
	        	if(buf_ring_empty(rng->re[i].re_qs))
		        	continue;
			rng->lowq_with_data = i;
         		break;
        	}
		if (i >= drbr_maxq) {
			/* Count was off? */
			rng->count_on_queues = 0;
			return (NULL);
                }
        }
#else
	if (i >= drbr_maxq) {
		/* Huh */
		i = 0;
	}
#endif
	m = buf_ring_dequeue_mc(rng->re[i].re_qs);
	if (m) {
		atomic_subtract_int(&rng->re[i].re_cnt, 1);
		atomic_subtract_int(&rng->count_on_queues, 1);
		if (rng->count_on_queues == 0) {
			atomic_set_int(&rng->lowq_with_data, 0xffffffff);
		}
	} else {
		/* TSNH */
		rng->re[i].re_cnt = 0;
	}
	return(m);
}

void
drbr_advance(struct ifnet *ifp, struct drbr_ring *rng, uint8_t qused)
{
	if (rng->count_on_queues == 0) {
		/* Huh? */
		return;
	}
	atomic_subtract_int(&rng->count_on_queues, 1);
	if (rng->count_on_queues == 0) {
		atomic_set_int(&rng->lowq_with_data, 0xffffffff);
	}
	buf_ring_advance_mc(rng->re[qused].re_qs);
	atomic_subtract_int(&rng->re[qused].re_cnt, 1);
}

struct mbuf *
drbr_dequeue_cond(struct ifnet *ifp, struct drbr_ring *rng,
    int (*func) (struct mbuf *, void *), void *arg) 
{
	uint8_t qused;
	struct mbuf *m;

	qused = 0;
	m = drbr_peek(ifp, rng, &qused);
	if (m == NULL || func(m, arg) == 0) {
		return (NULL);
	}
	atomic_subtract_int(&rng->re[qused].re_cnt, 1);
	atomic_subtract_int(&rng->count_on_queues, 1);
	m = buf_ring_dequeue_mc(rng->re[qused].re_qs);
	return (m);
}

int
drbr_empty(struct ifnet *ifp, struct drbr_ring *rng)
{
	return (!rng->count_on_queues);
}

int
drbr_needs_enqueue(struct ifnet *ifp, struct drbr_ring *rng)
{
	return (!(rng->count_on_queues == 0));
}

int
drbr_inuse(struct ifnet *ifp, struct drbr_ring *rng)
{
	return (rng->count_on_queues);
}
#include <net/drbr.h>

SYSCTL_DECL(_net_link);
uint32_t drbr_maxq=DRBR_MAXQ_DEFAULT;

TUNABLE_INT("net.link.drbr_maxq", &drbr_maxq);
SYSCTL_NODE(_net, OID_AUTO, drbr, CTLFLAG_RD, 0, "DRBR Parameters");
SYSCTL_INT(_net_drbr, OID_AUTO, drbr_maxq, CTLFLAG_RDTUN,
    &drbr_maxq, 0, "max number of priority queues per interface");

uint8_t set_up_drbr_depth=0;
uint32_t drbr_max_priority=DRBR_MAXQ_DEFAULT-1;
uint32_t drbr_queue_depth=DRBR_MIN_DEPTH;
uint32_t panic_on_dup_buf = 0;
uint32_t use_drbr_lock = 0;

SYSCTL_INT(_net_drbr, OID_AUTO, drbr_queue_depth, CTLFLAG_RD,
    &drbr_queue_depth, 0, "Queue length configed via ifqmaxlen");

SYSCTL_INT(_net_drbr, OID_AUTO, drbr_max_priority, CTLFLAG_RD,
    &drbr_max_priority, 0, "Queue length configed via ifqmaxlen");

SYSCTL_INT(_net_drbr, OID_AUTO, drbr_panicdup, CTLFLAG_RW,
    &panic_on_dup_buf, 0, "Panic on dup buf into br ring");

SYSCTL_INT(_net_drbr, OID_AUTO, drbr_usemtx, CTLFLAG_RW,
    &use_drbr_lock, 0, "Use drbr mtx");

struct drbr_ring *
drbr_alloc(struct malloc_type *type, int flags, struct mtx *tmtx)
{
	struct drbr_ring *rng;
	int i;
	if (set_up_drbr_depth == 0) {
		drbr_max_priority = drbr_maxq-1;
		set_up_drbr_depth = 1;
		drbr_queue_depth = 1 << ((fls(ifqmaxlen)-1));
		if (drbr_queue_depth < DRBR_MIN_DEPTH) {
			drbr_queue_depth = DRBR_MIN_DEPTH;
		}
	}
	rng = (struct drbr_ring *)malloc(sizeof(struct drbr_ring), type, flags);
	if (rng == NULL) {
		return(NULL);
	}
	memset(rng, 0, sizeof(struct drbr_ring));
	DRBR_LOCK_INIT(rng);
	rng->re = (struct drbr_ring_entry *)malloc((sizeof(struct drbr_ring_entry)*drbr_maxq), 
			 type, flags);
	if (rng->re == NULL) {
		free(rng, type);
		return(NULL);
	}
	memset(rng->re, 0, (sizeof(struct drbr_ring_entry) * drbr_maxq));
	/* Ok get the queues */
	for (i=0; i<drbr_maxq; i++) {
		rng->re[i].re_qs = buf_ring_alloc(drbr_queue_depth, type, flags, tmtx);
		if (rng->re[i].re_qs == NULL) {
			goto out_err;
		}
	}
	rng->lowq_with_data = 0xffffffff;
	return(rng);
out_err:
	for(i=0; i<drbr_maxq; i++) {
		if (rng->re[i].re_qs) {
			free(rng->re[i].re_qs, type);
		}
	}
	free(rng->re, type);
	free(rng, type);
	return (NULL);
}

#define PRIO_NAME_LEN 32
void 
drbr_add_sysctl_stats(device_t dev, struct sysctl_oid_list *queue_list, 
		      struct drbr_ring *rng)
{
	int i;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *prio_node;
	struct sysctl_oid_list *prio_list;
	char namebuf[PRIO_NAME_LEN];

	if (rng == NULL)
		/* TSNH */
		return;
	for (i=0; i<drbr_maxq; i++) {
		snprintf(namebuf, PRIO_NAME_LEN, "prio%d", i);
		
		prio_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO, namebuf,
					    CTLFLAG_RD, NULL, "Prioity Info");
		prio_list = SYSCTL_CHILDREN(prio_node);
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "packets_sent",
				CTLFLAG_RD, &rng->re[i].re_cnt_sent,
				"Packets Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "bytes_sent",
				CTLFLAG_RD, &rng->re[i].re_bytecnt_sent,
				"Bytes Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_packets",
				CTLFLAG_RD, &rng->re[i].re_drop_cnt,
				"Packets Dropped");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_bytes",
				CTLFLAG_RD, &rng->re[i].re_bytedrop_cnt,
				"Bytes Dropped");
		SYSCTL_ADD_UINT(ctx, prio_list, OID_AUTO, "on_queue_now",
				CTLFLAG_RD, &rng->re[i].re_cnt, 0,
				"Current Queue Size");

	}

}

u_long
drbr_get_dropcnt(struct drbr_ring *rng)
{
	u_long total;
	int i;

	total = 0;
	for (i=0; i<drbr_maxq; i++) {
		total += rng->re[i].re_drop_cnt;
	}
	return (total);
}

void 
drbr_add_sysctl_stats_nodev(struct sysctl_oid_list *queue_list, 
			    struct sysctl_ctx_list *ctx,
			    struct drbr_ring *rng)
{
	int i;
	struct sysctl_oid *prio_node;
	struct sysctl_oid_list *prio_list;
	char namebuf[PRIO_NAME_LEN];

	if (rng == NULL)
		return;
	for (i=0; i<drbr_maxq; i++) {
		snprintf(namebuf, PRIO_NAME_LEN, "prio%d", i);
		prio_node = SYSCTL_ADD_NODE(ctx, queue_list, OID_AUTO, namebuf,
					CTLFLAG_RD, NULL, "Prioity Info");
		prio_list = SYSCTL_CHILDREN(prio_node);
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "packets_sent",
				CTLFLAG_RD, &rng->re[i].re_cnt_sent,
				"Packets Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "bytes_sent",
				CTLFLAG_RD, &rng->re[i].re_bytecnt_sent,
				"Bytes Enqueued");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_packets",
				CTLFLAG_RD, &rng->re[i].re_drop_cnt,
				"Packets Dropped");
		SYSCTL_ADD_QUAD(ctx, prio_list, OID_AUTO, "dropped_bytes",
				CTLFLAG_RD, &rng->re[i].re_bytedrop_cnt,
				"Bytes Dropped");
		SYSCTL_ADD_UINT(ctx, prio_list, OID_AUTO, "on_queue_now",
				CTLFLAG_RD, &rng->re[i].re_cnt, 0,
				"Current Queue Size");
	}
}

int
drbr_enqueue(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *m)
{	
	int error = 0;
	uint8_t qused;
	uint64_t bytecnt;
	int locked = 0;

#ifdef ALTQ
	if ((ifp != NULL) && 
	    (ALTQ_IS_ENABLED(&ifp->if_snd))) {
		IFQ_ENQUEUE(&ifp->if_snd, m, error);
		return (error);
	}
#endif
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	if (m->m_pkthdr.cosqos >= drbr_maxq) {
		/* Lowest priority queue */
		qused = drbr_maxq - 1;
	} else {
		qused = m->m_pkthdr.cosqos;
	}
	bytecnt = m->m_pkthdr.len;
	error = buf_ring_enqueue(rng->re[qused].re_qs, m);
        if (error) {
		m_freem(m);
		atomic_add_long(&rng->re[qused].re_drop_cnt, 1);
		atomic_add_long(&rng->re[qused].re_bytedrop_cnt, bytecnt);
	} else {
		if (qused < rng->lowq_with_data) {
			atomic_clear_int(&rng->lowq_with_data, 0xffffffff);
			atomic_set_int(&rng->lowq_with_data, qused);
		}
		atomic_add_int(&rng->count_on_queues, 1);
		atomic_add_int(&rng->re[qused].re_cnt, 1);
		atomic_add_long(&rng->re[qused].re_cnt_sent, 1);
		atomic_add_long(&rng->re[qused].re_bytecnt_sent, bytecnt);
	}
	if (locked) {
		DRBR_UNLOCK(rng);
	}
	return (error);
}

int
drbr_is_on_ring(struct drbr_ring *rng, struct mbuf *m)
{
	int locked = 0;
	int answer = 0; /* No its not by default */
	int i;
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	for(i=0; i<drbr_maxq;i++) {
		if (buf_ring_empty(rng->re[i].re_qs))
			continue;
		if (buf_ring_mbufon(rng->re[i].re_qs, m)) {
			answer = 1;
			break;
		}
	}	
	if (locked) {
		DRBR_UNLOCK(rng);
	}
	return(answer);
}

void
drbr_putback(struct ifnet *ifp, struct drbr_ring *rng, struct mbuf *new, uint8_t qused)
{
	/*
	 * The top of the list needs to be swapped 
	 * for this one.
	 */
	int locked = 0;
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	buf_ring_putback_mc(rng->re[qused].re_qs, new);
	if (locked) {
		DRBR_UNLOCK(rng);
	}
}

struct mbuf *
drbr_peek(struct ifnet *ifp, struct drbr_ring *rng, uint8_t *qused)
{
	int i;
	int locked = 0;
	struct mbuf *m;
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	if (rng->count_on_queues == 0) {
		/* All done now */
		if (locked) {
			DRBR_UNLOCK(rng);
		}
		return (NULL);
	}
	if (rng->lowq_with_data == 0xffffffff) {
		rng->lowq_with_data = 0;
	}
	for(i=rng->lowq_with_data; i<drbr_maxq;i++) {
		if (buf_ring_empty(rng->re[i].re_qs))
			continue;
		rng->lowq_with_data = i;
		break;
	}
	if (i >= drbr_maxq) {
		/* Huh? */
		rng->lowq_with_data = 0;
        	for (i=rng->lowq_with_data; i<drbr_maxq;i++) {
	        	if(buf_ring_empty(rng->re[i].re_qs))
		        	continue;
			rng->lowq_with_data = i;
         		break;
        	}
		if (i >= drbr_maxq) {
			/* Really huh? */
			rng->count_on_queues = 0;
			if (locked) {
				DRBR_UNLOCK(rng);
			}
			return (NULL);
                }
        }
	*qused = i;
	m = buf_ring_peek(rng->re[i].re_qs);
	if (locked) {
		DRBR_UNLOCK(rng);
	}
	return(m);
}

static void
drbr_flush_locked(struct ifnet *ifp, struct drbr_ring *rng)
{
	int i;
	struct mbuf *m;
	int locked = 0;
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	if (rng == NULL) {
		return;
	}
	for(i=0; i<drbr_maxq; i++) {
		while ((m = buf_ring_dequeue_mc(rng->re[i].re_qs)) != NULL) {
			atomic_subtract_long(&rng->re[i].re_cnt_sent, 1);
			if (ifp) {
				ifp->if_oerrors++;
			}
			m_freem(m);
		}
		rng->re[i].re_cnt = 0;
	}
	rng->lowq_with_data = 0xffffffff;
	rng->count_on_queues = 0;
	if (locked) {
		DRBR_UNLOCK(rng);
	}
}

void
drbr_flush(struct ifnet *ifp, struct drbr_ring *rng)
{
	drbr_flush_locked(ifp, rng);
}

void
drbr_free(struct drbr_ring *rng, struct malloc_type *type)
{
	int i;
	int locked = 0;
	if (rng == NULL) {
		return;
	}
	drbr_flush_locked(NULL, rng);

	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	for(i=0; i<drbr_maxq; i++) {
		if (rng->re[i].re_qs) {
			buf_ring_free(rng->re[i].re_qs, type);
		}
	}
	DRBR_LOCK_DESTROY(rng);
	free(rng->re, type);
	free(rng, type);
}

struct mbuf *
drbr_dequeue(struct ifnet *ifp, struct drbr_ring *rng)
{
	int i;
	struct mbuf *m;
	int locked = 0;
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	if (rng->count_on_queues == 0) {
		if (locked) {
			DRBR_UNLOCK(rng);
		}
		return (NULL);
	}
	if (rng->lowq_with_data == 0xffffffff) {
		rng->lowq_with_data = 0;
	}
	for(i=rng->lowq_with_data; i<drbr_maxq;i++) {
		if (buf_ring_empty(rng->re[i].re_qs))
			continue;
		rng->lowq_with_data = i;
		break;
	}
#ifdef INVARIANT
	if (i >= drbr_maxq) {
		/* Nothing on ring from marker up? */
		rng->lowq_with_data = 0;
        	for (i=rng->lowq_with_data; i<drbr_maxq;i++) {
	        	if(buf_ring_empty(rng->re[i].re_qs))
		        	continue;
			rng->lowq_with_data = i;
         		break;
        	}
		if (i >= drbr_maxq) {
			/* Count was off? */
			rng->count_on_queues = 0;
			if (locked) {
				DRBR_UNLOCK(rng);
			}
			return (NULL);
                }
        }
#else
	if (i >= drbr_maxq) {
		/* Huh */
		i = 0;
	}
#endif
	m = buf_ring_dequeue_mc(rng->re[i].re_qs);
	if (m) {
		atomic_subtract_int(&rng->re[i].re_cnt, 1);
		atomic_subtract_int(&rng->count_on_queues, 1);
		if (rng->count_on_queues == 0) {
			atomic_set_int(&rng->lowq_with_data, 0xffffffff);
		}
	} else {
		/* TSNH */
		rng->re[i].re_cnt = 0;
	}
	if (locked) {
		DRBR_UNLOCK(rng);
	}
	return(m);
}

void
drbr_advance(struct ifnet *ifp, struct drbr_ring *rng, uint8_t qused)
{
	int locked = 0;
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	if (rng->count_on_queues == 0) {
		/* Huh? */
		if (locked) {
			DRBR_UNLOCK(rng);
		}
		return;
	}
	atomic_subtract_int(&rng->count_on_queues, 1);
	if (rng->count_on_queues == 0) {
		atomic_set_int(&rng->lowq_with_data, 0xffffffff);
	}
	buf_ring_advance_mc(rng->re[qused].re_qs);
	atomic_subtract_int(&rng->re[qused].re_cnt, 1);
	if (locked) {
		DRBR_UNLOCK(rng);
	}
}

struct mbuf *
drbr_dequeue_cond(struct ifnet *ifp, struct drbr_ring *rng,
    int (*func) (struct mbuf *, void *), void *arg) 
{
	uint8_t qused;
	struct mbuf *m;
	int locked = 0;
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	}
	qused = 0;
	m = drbr_peek(ifp, rng, &qused);
	if (locked) {
		DRBR_UNLOCK(rng);
	}
	if (m == NULL || func(m, arg) == 0) {
		return (NULL);
	}
	if (use_drbr_lock) {
		DRBR_LOCK(rng);
		locked = 1;
	} else {
		locked = 0;
	}
	atomic_subtract_int(&rng->re[qused].re_cnt, 1);
	atomic_subtract_int(&rng->count_on_queues, 1);
	m = buf_ring_dequeue_mc(rng->re[qused].re_qs);
	if (locked) {
		DRBR_UNLOCK(rng);
	}
	return (m);
}

int
drbr_empty(struct ifnet *ifp, struct drbr_ring *rng)
{
	return (!rng->count_on_queues);
}

int
drbr_needs_enqueue(struct ifnet *ifp, struct drbr_ring *rng)
{
	return (!(rng->count_on_queues == 0));
}

int
drbr_inuse(struct ifnet *ifp, struct drbr_ring *rng)
{
	return (rng->count_on_queues);
}
