/*
 * $FreeBSD$
 *
 * userspace compatibility code for dummynet schedulers
 */

#ifndef _DN_TEST_H
#define _DN_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>	/* bzero, ffs, ... */
#include <string.h>	/* strcmp */
#include <errno.h>
#include <sys/queue.h>
#include <sys/time.h>

extern int debug;
#define ND(fmt, args...) do {} while (0)
#define D1(fmt, args...) do {} while (0)
#define D(fmt, args...) fprintf(stderr, "%-8s " fmt "\n",      \
        __FUNCTION__, ## args)
#define DX(lev, fmt, args...) do {              \
        if (debug > lev) D(fmt, ## args); } while (0)


#ifndef offsetof
#define offsetof(t,m) (int)(intptr_t)((&((t *)0L)->m))
#endif

#if defined(__APPLE__) // XXX osx
typedef unsigned int u_int;
#endif /* osx */

#include <mylist.h>

/* prevent include of other system headers */
#define	_NETINET_IP_VAR_H_	/* ip_fw_args */
#define _IPFW2_H
#define _SYS_MBUF_H_

enum	{
	DN_QUEUE,
};

enum	{
	DN_SCHED_FIFO,
	DN_SCHED_WF2QP,
};

struct dn_id {
	int type, subtype, len, id;
};

struct dn_fs {
	int par[4];	/* flowset parameters */

	/* simulation entries.
	 * 'index' is not strictly necessary
	 * y is used for the inverse mapping ,
	 */
	int index;
	int y;	/* inverse mapping */
	int base_y;	/* inverse mapping */
	int next_y;	/* inverse mapping */
	int n_flows;
	int first_flow;
	int next_flow;	/* first_flow + n_flows */
	/*
	 * when generating, let 'cur' go from 0 to n_flows-1,
	 * then point to flow first_flow + cur
	 */
	int	cur;
};

struct dn_sch {
};

struct dn_flow {
	struct dn_id oid;
	int length;
	int len_bytes;
	int drops;
	uint64_t tot_bytes;
	uint32_t flow_id;
	struct list_head h;	/* used by the generator */

	/* bytes served by the flow since the last backlog time */
	uint64_t bytes;
	/* bytes served by the system at the last backlog time  */
	uint64_t sch_bytes;
};

struct dn_link {
};

struct ip_fw_args {
};

struct mbuf {
        struct {
                int len;
        } m_pkthdr;
        struct mbuf *m_nextpkt;
	int flow_id;	/* for testing, index of a flow */
	//int flowset_id;	/* for testing, index of a flowset */
	void *cfg;	/* config args */
};

#define MALLOC_DECLARE(x)	extern volatile int __dummy__ ## x
#define KASSERT(x, y)	do { if (!(x)) printf y ; exit(0); } while (0)
struct ipfw_flow_id {
};

typedef void * module_t;

struct _md_t {
	const char *name;
	int (*f)(module_t, int, void *);
	void *p;
};

typedef struct _md_t moduledata_t;

#define DECLARE_MODULE(name, b, c, d)	\
	moduledata_t *_g_##name = & b
#define MODULE_DEPEND(a, b, c, d, e)

#ifdef IPFW
#include <dn_heap.h>
#include <ip_dn_private.h>
#include <dn_sched.h>
#else
struct dn_queue {
        struct dn_fsk *fs;             /* parent flowset. */
        struct dn_sch_inst *_si;	/* parent sched instance. */
};
struct dn_schk {
};
struct dn_fsk {
	struct dn_fs fs;
	struct dn_schk *sched;
};
struct dn_sch_inst {
	struct dn_schk *sched;
};
struct dn_alg {
	int type;
	const char *name;
	void *enqueue, *dequeue;
	int q_datalen, si_datalen, schk_datalen;
	int (*config)(struct dn_schk *);
	int (*new_sched)(struct dn_sch_inst *);
	int (*new_fsk)(struct dn_fsk *);
        int (*new_queue)(struct dn_queue *q);
};

#endif

#ifndef __FreeBSD__
int fls(int);
#endif

static inline void
mq_append(struct mq *q, struct mbuf *m)
{
        if (q->head == NULL)
                q->head = m;
        else
                q->tail->m_nextpkt = m;
        q->tail = m;
        m->m_nextpkt = NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* _DN_TEST_H */
