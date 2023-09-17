/*
 * Copyright (C) 2017 Corelight, Inc. and Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <libnetmap.h>
#include <netinet/in.h>		/* htonl */
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>

#include "pkt_hash.h"
#include "ctrs.h"


/*
 * use our version of header structs, rather than bringing in a ton
 * of platform specific ones
 */
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

struct compact_eth_hdr {
	unsigned char h_dest[ETH_ALEN];
	unsigned char h_source[ETH_ALEN];
	u_int16_t h_proto;
};

struct compact_ip_hdr {
	u_int8_t ihl:4, version:4;
	u_int8_t tos;
	u_int16_t tot_len;
	u_int16_t id;
	u_int16_t frag_off;
	u_int8_t ttl;
	u_int8_t protocol;
	u_int16_t check;
	u_int32_t saddr;
	u_int32_t daddr;
};

struct compact_ipv6_hdr {
	u_int8_t priority:4, version:4;
	u_int8_t flow_lbl[3];
	u_int16_t payload_len;
	u_int8_t nexthdr;
	u_int8_t hop_limit;
	struct in6_addr saddr;
	struct in6_addr daddr;
};

#define MAX_IFNAMELEN 	64
#define MAX_PORTNAMELEN	(MAX_IFNAMELEN + 40)
#define DEF_OUT_PIPES 	2
#define DEF_EXTRA_BUFS 	0
#define DEF_BATCH	2048
#define DEF_WAIT_LINK	2
#define DEF_STATS_INT	600
#define BUF_REVOKE	150
#define STAT_MSG_MAXSIZE 1024

static struct {
	char ifname[MAX_IFNAMELEN + 1];
	char base_name[MAX_IFNAMELEN + 1];
	int netmap_fd;
	uint16_t output_rings;
	uint16_t num_groups;
	uint32_t extra_bufs;
	uint16_t batch;
	int stdout_interval;
	int syslog_interval;
	int wait_link;
	bool busy_wait;
} glob_arg;

/*
 * the overflow queue is a circular queue of buffers
 */
struct overflow_queue {
	char name[MAX_IFNAMELEN + 16];
	struct netmap_slot *slots;
	uint32_t head;
	uint32_t tail;
	uint32_t n;
	uint32_t size;
};

static struct overflow_queue *freeq;

static inline int
oq_full(struct overflow_queue *q)
{
	return q->n >= q->size;
}

static inline int
oq_empty(struct overflow_queue *q)
{
	return q->n <= 0;
}

static inline void
oq_enq(struct overflow_queue *q, const struct netmap_slot *s)
{
	if (unlikely(oq_full(q))) {
		D("%s: queue full!", q->name);
		abort();
	}
	q->slots[q->tail] = *s;
	q->n++;
	q->tail++;
	if (q->tail >= q->size)
		q->tail = 0;
}

static inline struct netmap_slot
oq_deq(struct overflow_queue *q)
{
	struct netmap_slot s = q->slots[q->head];
	if (unlikely(oq_empty(q))) {
		D("%s: queue empty!", q->name);
		abort();
	}
	q->n--;
	q->head++;
	if (q->head >= q->size)
		q->head = 0;
	return s;
}

static volatile int do_abort = 0;

static uint64_t dropped = 0;
static uint64_t forwarded = 0;
static uint64_t received_bytes = 0;
static uint64_t received_pkts = 0;
static uint64_t non_ip = 0;
static uint32_t freeq_n = 0;

struct port_des {
	char interface[MAX_PORTNAMELEN];
	struct my_ctrs ctr;
	unsigned int last_sync;
	uint32_t last_tail;
	struct overflow_queue *oq;
	struct nmport_d *nmd;
	struct netmap_ring *ring;
	struct group_des *group;
};

static struct port_des *ports;

/* each group of pipes receives all the packets */
struct group_des {
	char pipename[MAX_IFNAMELEN];
	struct port_des *ports;
	int first_id;
	int nports;
	int last;
	int custom_port;
};

static struct group_des *groups;

/* statistcs */
struct counters {
	struct timeval ts;
	struct my_ctrs *ctrs;
	uint64_t received_pkts;
	uint64_t received_bytes;
	uint64_t non_ip;
	uint32_t freeq_n;
	int status __attribute__((aligned(64)));
#define COUNTERS_EMPTY	0
#define COUNTERS_FULL	1
};

static struct counters counters_buf;

static void *
print_stats(void *arg)
{
	int npipes = glob_arg.output_rings;
	int sys_int = 0;
	(void)arg;
	struct my_ctrs cur, prev;
	struct my_ctrs *pipe_prev;

	pipe_prev = calloc(npipes, sizeof(struct my_ctrs));
	if (pipe_prev == NULL) {
		D("out of memory");
		exit(1);
	}

	char stat_msg[STAT_MSG_MAXSIZE] = "";

	memset(&prev, 0, sizeof(prev));
	while (!do_abort) {
		int j, dosyslog = 0, dostdout = 0, newdata;
		uint64_t pps = 0, dps = 0, bps = 0, dbps = 0, usec = 0;
		struct my_ctrs x;

		counters_buf.status = COUNTERS_EMPTY;
		newdata = 0;
		memset(&cur, 0, sizeof(cur));
		sleep(1);
		if (counters_buf.status == COUNTERS_FULL) {
			__sync_synchronize();
			newdata = 1;
			cur.t = counters_buf.ts;
			if (prev.t.tv_sec || prev.t.tv_usec) {
				usec = (cur.t.tv_sec - prev.t.tv_sec) * 1000000 +
					cur.t.tv_usec - prev.t.tv_usec;
			}
		}

		++sys_int;
		if (glob_arg.stdout_interval && sys_int % glob_arg.stdout_interval == 0)
				dostdout = 1;
		if (glob_arg.syslog_interval && sys_int % glob_arg.syslog_interval == 0)
				dosyslog = 1;

		for (j = 0; j < npipes; ++j) {
			struct my_ctrs *c = &counters_buf.ctrs[j];
			cur.pkts += c->pkts;
			cur.drop += c->drop;
			cur.drop_bytes += c->drop_bytes;
			cur.bytes += c->bytes;

			if (usec) {
				x.pkts = c->pkts - pipe_prev[j].pkts;
				x.drop = c->drop - pipe_prev[j].drop;
				x.bytes = c->bytes - pipe_prev[j].bytes;
				x.drop_bytes = c->drop_bytes - pipe_prev[j].drop_bytes;
				pps = (x.pkts*1000000 + usec/2) / usec;
				dps = (x.drop*1000000 + usec/2) / usec;
				bps = ((x.bytes*1000000 + usec/2) / usec) * 8;
				dbps = ((x.drop_bytes*1000000 + usec/2) / usec) * 8;
			}
			pipe_prev[j] = *c;

			if ( (dosyslog || dostdout) && newdata )
				snprintf(stat_msg, STAT_MSG_MAXSIZE,
				       "{"
				       "\"ts\":%.6f,"
				       "\"interface\":\"%s\","
				       "\"output_ring\":%" PRIu16 ","
				       "\"packets_forwarded\":%" PRIu64 ","
				       "\"packets_dropped\":%" PRIu64 ","
				       "\"data_forward_rate_Mbps\":%.4f,"
				       "\"data_drop_rate_Mbps\":%.4f,"
				       "\"packet_forward_rate_kpps\":%.4f,"
				       "\"packet_drop_rate_kpps\":%.4f,"
				       "\"overflow_queue_size\":%" PRIu32
				       "}", cur.t.tv_sec + (cur.t.tv_usec / 1000000.0),
				            ports[j].interface,
				            j,
				            c->pkts,
				            c->drop,
				            (double)bps / 1024 / 1024,
				            (double)dbps / 1024 / 1024,
				            (double)pps / 1000,
				            (double)dps / 1000,
				            c->oq_n);

			if (dosyslog && stat_msg[0])
				syslog(LOG_INFO, "%s", stat_msg);
			if (dostdout && stat_msg[0])
				printf("%s\n", stat_msg);
		}
		if (usec) {
			x.pkts = cur.pkts - prev.pkts;
			x.drop = cur.drop - prev.drop;
			x.bytes = cur.bytes - prev.bytes;
			x.drop_bytes = cur.drop_bytes - prev.drop_bytes;
			pps = (x.pkts*1000000 + usec/2) / usec;
			dps = (x.drop*1000000 + usec/2) / usec;
			bps = ((x.bytes*1000000 + usec/2) / usec) * 8;
			dbps = ((x.drop_bytes*1000000 + usec/2) / usec) * 8;
		}

		if ( (dosyslog || dostdout) && newdata )
			snprintf(stat_msg, STAT_MSG_MAXSIZE,
			         "{"
			         "\"ts\":%.6f,"
			         "\"interface\":\"%s\","
			         "\"output_ring\":null,"
			         "\"packets_received\":%" PRIu64 ","
			         "\"packets_forwarded\":%" PRIu64 ","
			         "\"packets_dropped\":%" PRIu64 ","
			         "\"non_ip_packets\":%" PRIu64 ","
			         "\"data_forward_rate_Mbps\":%.4f,"
			         "\"data_drop_rate_Mbps\":%.4f,"
			         "\"packet_forward_rate_kpps\":%.4f,"
			         "\"packet_drop_rate_kpps\":%.4f,"
			         "\"free_buffer_slots\":%" PRIu32
			         "}", cur.t.tv_sec + (cur.t.tv_usec / 1000000.0),
			              glob_arg.ifname,
			              received_pkts,
			              cur.pkts,
			              cur.drop,
			              counters_buf.non_ip,
			              (double)bps / 1024 / 1024,
			              (double)dbps / 1024 / 1024,
			              (double)pps / 1000,
			              (double)dps / 1000,
			              counters_buf.freeq_n);

		if (dosyslog && stat_msg[0])
			syslog(LOG_INFO, "%s", stat_msg);
		if (dostdout && stat_msg[0])
			printf("%s\n", stat_msg);

		prev = cur;
	}

	free(pipe_prev);

	return NULL;
}

static void
free_buffers(void)
{
	int i, tot = 0;
	struct port_des *rxport = &ports[glob_arg.output_rings];

	/* build a netmap free list with the buffers in all the overflow queues */
	for (i = 0; i < glob_arg.output_rings + 1; i++) {
		struct port_des *cp = &ports[i];
		struct overflow_queue *q = cp->oq;

		if (!q)
			continue;

		while (q->n) {
			struct netmap_slot s = oq_deq(q);
			uint32_t *b = (uint32_t *)NETMAP_BUF(cp->ring, s.buf_idx);

			*b = rxport->nmd->nifp->ni_bufs_head;
			rxport->nmd->nifp->ni_bufs_head = s.buf_idx;
			tot++;
		}
	}
	D("added %d buffers to netmap free list", tot);

	for (i = 0; i < glob_arg.output_rings + 1; ++i) {
		nmport_close(ports[i].nmd);
	}
}


static void sigint_h(int sig)
{
	(void)sig;		/* UNUSED */
	do_abort = 1;
	signal(SIGINT, SIG_DFL);
}

static void
usage(void)
{
	printf("usage: lb [options]\n");
	printf("where options are:\n");
	printf("  -h              	view help text\n");
	printf("  -i iface        	interface name (required)\n");
	printf("  -p [prefix:]npipes	add a new group of output pipes\n");
	printf("  -B nbufs        	number of extra buffers (default: %d)\n", DEF_EXTRA_BUFS);
	printf("  -b batch        	batch size (default: %d)\n", DEF_BATCH);
	printf("  -w seconds        	wait for link up (default: %d)\n", DEF_WAIT_LINK);
	printf("  -W                    enable busy waiting. this will run your CPU at 100%%\n");
	printf("  -s seconds      	seconds between syslog stats messages (default: 0)\n");
	printf("  -o seconds      	seconds between stdout stats messages (default: 0)\n");
	exit(0);
}

static int
parse_pipes(const char *spec)
{
	const char *end = index(spec, ':');
	static int max_groups = 0;
	struct group_des *g;

	ND("spec %s num_groups %d", spec, glob_arg.num_groups);
	if (max_groups < glob_arg.num_groups + 1) {
		size_t size = sizeof(*g) * (glob_arg.num_groups + 1);
		groups = realloc(groups, size);
		if (groups == NULL) {
			D("out of memory");
			return 1;
		}
	}
	g = &groups[glob_arg.num_groups];
	memset(g, 0, sizeof(*g));

	if (end != NULL) {
		if (end - spec > MAX_IFNAMELEN - 8) {
			D("name '%s' too long", spec);
			return 1;
		}
		if (end == spec) {
			D("missing prefix before ':' in '%s'", spec);
			return 1;
		}
		strncpy(g->pipename, spec, end - spec);
		g->custom_port = 1;
		end++;
	} else {
		/* no prefix, this group will use the
		 * name of the input port.
		 * This will be set in init_groups(),
		 * since here the input port may still
		 * be uninitialized
		 */
		end = spec;
	}
	if (*end == '\0') {
		g->nports = DEF_OUT_PIPES;
	} else {
		g->nports = atoi(end);
		if (g->nports < 1) {
			D("invalid number of pipes '%s' (must be at least 1)", end);
			return 1;
		}
	}
	glob_arg.output_rings += g->nports;
	glob_arg.num_groups++;
	return 0;
}

/* complete the initialization of the groups data structure */
static void
init_groups(void)
{
	int i, j, t = 0;
	struct group_des *g = NULL;
	for (i = 0; i < glob_arg.num_groups; i++) {
		g = &groups[i];
		g->ports = &ports[t];
		for (j = 0; j < g->nports; j++)
			g->ports[j].group = g;
		t += g->nports;
		if (!g->custom_port)
			strcpy(g->pipename, glob_arg.base_name);
		for (j = 0; j < i; j++) {
			struct group_des *h = &groups[j];
			if (!strcmp(h->pipename, g->pipename))
				g->first_id += h->nports;
		}
	}
	g->last = 1;
}


/* To support packets that span multiple slots (NS_MOREFRAG) we
 * need to make sure of the following:
 *
 * - all fragments of the same packet must go to the same output pipe
 * - when dropping, all fragments of the same packet must be dropped
 *
 * For the former point we remember and reuse the last hash computed
 * in each input ring, and only update it when NS_MOREFRAG was not
 * set in the last received slot (this marks the start of a new packet).
 *
 * For the latter point, we only update the output ring head pointer
 * when an entire packet has been forwarded. We keep a shadow_head
 * pointer to know where to put the next partial fragment and,
 * when the need to drop arises, we roll it back to head.
 */
struct morefrag {
	uint16_t last_flag;	/* for input rings */
	uint32_t last_hash;	/* for input rings */
	uint32_t shadow_head;	/* for output rings */
};

/* push the packet described by slot rs to the group g.
 * This may cause other buffers to be pushed down the
 * chain headed by g.
 * Return a free buffer.
 */
static uint32_t
forward_packet(struct group_des *g, struct netmap_slot *rs)
{
	uint32_t hash = rs->ptr;
	uint32_t output_port = hash % g->nports;
	struct port_des *port = &g->ports[output_port];
	struct netmap_ring *ring = port->ring;
	struct overflow_queue *q = port->oq;
	struct morefrag *mf = (struct morefrag *)ring->sem;
	uint16_t curmf = rs->flags & NS_MOREFRAG;

	/* Move the packet to the output pipe, unless there is
	 * either no space left on the ring, or there is some
	 * packet still in the overflow queue (since those must
	 * take precedence over the new one)
	*/
	if (mf->shadow_head != ring->tail && (q == NULL || oq_empty(q))) {
		struct netmap_slot *ts = &ring->slot[mf->shadow_head];
		struct netmap_slot old_slot = *ts;

		ts->buf_idx = rs->buf_idx;
		ts->len = rs->len;
		ts->flags = rs->flags | NS_BUF_CHANGED;
		ts->ptr = rs->ptr;
		mf->shadow_head = nm_ring_next(ring, mf->shadow_head);
		if (!curmf) {
			ring->head = mf->shadow_head;
		}
		ND("curmf %2x ts->flags %2x shadow_head %3u head %3u tail %3u",
				curmf, ts->flags, mf->shadow_head, ring->head, ring->tail);
		port->ctr.bytes += rs->len;
		port->ctr.pkts++;
		forwarded++;
		return old_slot.buf_idx;
	}

	/* use the overflow queue, if available */
	if (q == NULL || oq_full(q)) {
		uint32_t scan;
		/* no space left on the ring and no overflow queue
		 * available: we are forced to drop the packet
		 */

		/* drop previous fragments, if any */
		for (scan = ring->head; scan != mf->shadow_head;
				scan = nm_ring_next(ring, scan)) {
			struct netmap_slot *ts = &ring->slot[scan];
			dropped++;
			port->ctr.drop_bytes += ts->len;
		}
		mf->shadow_head = ring->head;

		dropped++;
		port->ctr.drop++;
		port->ctr.drop_bytes += rs->len;
		return rs->buf_idx;
	}

	oq_enq(q, rs);

	/*
	 * we cannot continue down the chain and we need to
	 * return a free buffer now. We take it from the free queue.
	 */
	if (oq_empty(freeq)) {
		/* the free queue is empty. Revoke some buffers
		 * from the longest overflow queue
		 */
		uint32_t j;
		struct port_des *lp = &ports[0];
		uint32_t max = lp->oq->n;

		/* let lp point to the port with the longest queue */
		for (j = 1; j < glob_arg.output_rings; j++) {
			struct port_des *cp = &ports[j];
			if (cp->oq->n > max) {
				lp = cp;
				max = cp->oq->n;
			}
		}

		/* move the oldest BUF_REVOKE buffers from the
		 * lp queue to the free queue
		 *
		 * We cannot revoke a partially received packet.
		 * To make thinks simple we make sure to leave
		 * at least NETMAP_MAX_FRAGS slots in the queue.
		 */
		for (j = 0; lp->oq->n > NETMAP_MAX_FRAGS && j < BUF_REVOKE; j++) {
			struct netmap_slot tmp = oq_deq(lp->oq);

			dropped++;
			lp->ctr.drop++;
			lp->ctr.drop_bytes += tmp.len;

			oq_enq(freeq, &tmp);
		}

		ND(1, "revoked %d buffers from %s", j, lq->name);
	}

	return oq_deq(freeq).buf_idx;
}

int main(int argc, char **argv)
{
	int ch;
	uint32_t i;
	int rv;
	int poll_timeout = 10; /* default */

	glob_arg.ifname[0] = '\0';
	glob_arg.output_rings = 0;
	glob_arg.batch = DEF_BATCH;
	glob_arg.wait_link = DEF_WAIT_LINK;
	glob_arg.busy_wait = false;
	glob_arg.syslog_interval = 0;
	glob_arg.stdout_interval = 0;

	while ( (ch = getopt(argc, argv, "hi:p:b:B:s:o:w:W")) != -1) {
		switch (ch) {
		case 'i':
			D("interface is %s", optarg);
			if (strlen(optarg) > MAX_IFNAMELEN - 8) {
				D("ifname too long %s", optarg);
				return 1;
			}
			if (strncmp(optarg, "netmap:", 7) && strncmp(optarg, "vale", 4)) {
				sprintf(glob_arg.ifname, "netmap:%s", optarg);
			} else {
				strcpy(glob_arg.ifname, optarg);
			}
			break;

		case 'p':
			if (parse_pipes(optarg)) {
				usage();
				return 1;
			}
			break;

		case 'B':
			glob_arg.extra_bufs = atoi(optarg);
			D("requested %d extra buffers", glob_arg.extra_bufs);
			break;

		case 'b':
			glob_arg.batch = atoi(optarg);
			D("batch is %d", glob_arg.batch);
			break;

		case 'w':
			glob_arg.wait_link = atoi(optarg);
			D("link wait for up time is %d", glob_arg.wait_link);
			break;

		case 'W':
			glob_arg.busy_wait = true;
			break;

		case 'o':
			glob_arg.stdout_interval = atoi(optarg);
			break;

		case 's':
			glob_arg.syslog_interval = atoi(optarg);
			break;

		case 'h':
			usage();
			return 0;
			break;

		default:
			D("bad option %c %s", ch, optarg);
			usage();
			return 1;
		}
	}

	if (glob_arg.ifname[0] == '\0') {
		D("missing interface name");
		usage();
		return 1;
	}

	if (glob_arg.num_groups == 0)
		parse_pipes("");

	if (glob_arg.syslog_interval) {
		setlogmask(LOG_UPTO(LOG_INFO));
		openlog("lb", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
	}

	uint32_t npipes = glob_arg.output_rings;


	pthread_t stat_thread;

	ports = calloc(npipes + 1, sizeof(struct port_des));
	if (!ports) {
		D("failed to allocate the stats array");
		return 1;
	}
	struct port_des *rxport = &ports[npipes];

	rxport->nmd = nmport_prepare(glob_arg.ifname);
	if (rxport->nmd == NULL) {
		D("cannot parse %s", glob_arg.ifname);
		return (1);
	}
	/* extract the base name */
	strncpy(glob_arg.base_name, rxport->nmd->hdr.nr_name, MAX_IFNAMELEN);

	init_groups();

	memset(&counters_buf, 0, sizeof(counters_buf));
	counters_buf.ctrs = calloc(npipes, sizeof(struct my_ctrs));
	if (!counters_buf.ctrs) {
		D("failed to allocate the counters snapshot buffer");
		return 1;
	}

	rxport->nmd->reg.nr_extra_bufs = glob_arg.extra_bufs;

	if (nmport_open_desc(rxport->nmd) < 0) {
		D("cannot open %s", glob_arg.ifname);
		return (1);
	}
	D("successfully opened %s", glob_arg.ifname);

	uint32_t extra_bufs = rxport->nmd->reg.nr_extra_bufs;
	struct overflow_queue *oq = NULL;
	/* reference ring to access the buffers */
	rxport->ring = NETMAP_RXRING(rxport->nmd->nifp, 0);

	if (!glob_arg.extra_bufs)
		goto run;

	D("obtained %d extra buffers", extra_bufs);
	if (!extra_bufs)
		goto run;

	/* one overflow queue for each output pipe, plus one for the
	 * free extra buffers
	 */
	oq = calloc(npipes + 1, sizeof(struct overflow_queue));
	if (!oq) {
		D("failed to allocated overflow queues descriptors");
		goto run;
	}

	freeq = &oq[npipes];
	rxport->oq = freeq;

	freeq->slots = calloc(extra_bufs, sizeof(struct netmap_slot));
	if (!freeq->slots) {
		D("failed to allocate the free list");
	}
	freeq->size = extra_bufs;
	snprintf(freeq->name, MAX_IFNAMELEN, "free queue");

	/*
	 * the list of buffers uses the first uint32_t in each buffer
	 * as the index of the next buffer.
	 */
	uint32_t scan;
	for (scan = rxport->nmd->nifp->ni_bufs_head;
	     scan;
	     scan = *(uint32_t *)NETMAP_BUF(rxport->ring, scan))
	{
		struct netmap_slot s;
		s.len = s.flags = 0;
		s.ptr = 0;
		s.buf_idx = scan;
		ND("freeq <- %d", s.buf_idx);
		oq_enq(freeq, &s);
	}


	if (freeq->n != extra_bufs) {
		D("something went wrong: netmap reported %d extra_bufs, but the free list contained %d",
				extra_bufs, freeq->n);
		return 1;
	}
	rxport->nmd->nifp->ni_bufs_head = 0;

run:
	atexit(free_buffers);

	int j, t = 0;
	for (j = 0; j < glob_arg.num_groups; j++) {
		struct group_des *g = &groups[j];
		int k;
		for (k = 0; k < g->nports; ++k) {
			struct port_des *p = &g->ports[k];
			snprintf(p->interface, MAX_PORTNAMELEN, "%s%s{%d/xT@%d",
					(strncmp(g->pipename, "vale", 4) ? "netmap:" : ""),
					g->pipename, g->first_id + k,
					rxport->nmd->reg.nr_mem_id);
			D("opening pipe named %s", p->interface);

			p->nmd = nmport_open(p->interface);

			if (p->nmd == NULL) {
				D("cannot open %s", p->interface);
				return (1);
			} else if (p->nmd->mem != rxport->nmd->mem) {
				D("failed to open pipe #%d in zero-copy mode, "
					"please close any application that uses either pipe %s}%d, "
				        "or %s{%d, and retry",
					k + 1, g->pipename, g->first_id + k, g->pipename, g->first_id + k);
				return (1);
			} else {
				struct morefrag *mf;

				D("successfully opened pipe #%d %s (tx slots: %d)",
				  k + 1, p->interface, p->nmd->reg.nr_tx_slots);
				p->ring = NETMAP_TXRING(p->nmd->nifp, 0);
				p->last_tail = nm_ring_next(p->ring, p->ring->tail);
				mf = (struct morefrag *)p->ring->sem;
				mf->last_flag = 0;	/* unused */
				mf->last_hash = 0;	/* unused */
				mf->shadow_head = p->ring->head;
			}
			D("zerocopy %s",
			  (rxport->nmd->mem == p->nmd->mem) ? "enabled" : "disabled");

			if (extra_bufs) {
				struct overflow_queue *q = &oq[t + k];
				q->slots = calloc(extra_bufs, sizeof(struct netmap_slot));
				if (!q->slots) {
					D("failed to allocate overflow queue for pipe %d", k);
					/* make all overflow queue management fail */
					extra_bufs = 0;
				}
				q->size = extra_bufs;
				snprintf(q->name, sizeof(q->name), "oq %s{%4d", g->pipename, k);
				p->oq = q;
			}
		}
		t += g->nports;
	}

	if (glob_arg.extra_bufs && !extra_bufs) {
		if (oq) {
			for (i = 0; i < npipes + 1; i++) {
				free(oq[i].slots);
				oq[i].slots = NULL;
			}
			free(oq);
			oq = NULL;
		}
		D("*** overflow queues disabled ***");
	}

	sleep(glob_arg.wait_link);

	/* start stats thread after wait_link */
	if (pthread_create(&stat_thread, NULL, print_stats, NULL) == -1) {
		D("unable to create the stats thread: %s", strerror(errno));
		return 1;
	}

	struct pollfd pollfd[npipes + 1];
	memset(&pollfd, 0, sizeof(pollfd));
	signal(SIGINT, sigint_h);

	/* make sure we wake up as often as needed, even when there are no
	 * packets coming in
	 */
	if (glob_arg.syslog_interval > 0 && glob_arg.syslog_interval < poll_timeout)
		poll_timeout = glob_arg.syslog_interval;
	if (glob_arg.stdout_interval > 0 && glob_arg.stdout_interval < poll_timeout)
		poll_timeout = glob_arg.stdout_interval;

	/* initialize the morefrag structures for the input rings */
	for (i = rxport->nmd->first_rx_ring; i <= rxport->nmd->last_rx_ring; i++) {
		struct netmap_ring *rxring = NETMAP_RXRING(rxport->nmd->nifp, i);
		struct morefrag *mf = (struct morefrag *)rxring->sem;

		mf->last_flag = 0;
		mf->last_hash = 0;
		mf->shadow_head = 0; /* unused */
	}

	while (!do_abort) {
		u_int polli = 0;

		for (i = 0; i < npipes; ++i) {
			struct netmap_ring *ring = ports[i].ring;
			int pending = nm_tx_pending(ring);

			/* if there are packets pending, we want to be notified when
			 * tail moves, so we let cur=tail
			 */
			ring->cur = pending ? ring->tail : ring->head;

			if (!glob_arg.busy_wait && !pending) {
				/* no need to poll, there are no packets pending */
				continue;
			}
			pollfd[polli].fd = ports[i].nmd->fd;
			pollfd[polli].events = POLLOUT;
			pollfd[polli].revents = 0;
			++polli;
		}

		pollfd[polli].fd = rxport->nmd->fd;
		pollfd[polli].events = POLLIN;
		pollfd[polli].revents = 0;
		++polli;

		ND(5, "polling %d file descriptors", polli);
		rv = poll(pollfd, polli, poll_timeout);
		if (rv <= 0) {
			if (rv < 0 && errno != EAGAIN && errno != EINTR)
				RD(1, "poll error %s", strerror(errno));
			goto send_stats;
		}

		/* if there are several groups, try pushing released packets from
		 * upstream groups to the downstream ones.
		 *
		 * It is important to do this before returned slots are reused
		 * for new transmissions. For the same reason, this must be
		 * done starting from the last group going backwards.
		 */
		for (i = glob_arg.num_groups - 1U; i > 0; i--) {
			struct group_des *g = &groups[i - 1];

			for (j = 0; j < g->nports; j++) {
				struct port_des *p = &g->ports[j];
				struct netmap_ring *ring = p->ring;
				uint32_t last = p->last_tail,
					 stop = nm_ring_next(ring, ring->tail);

				/* slight abuse of the API here: we touch the slot
				 * pointed to by tail
				 */
				for ( ; last != stop; last = nm_ring_next(ring, last)) {
					struct netmap_slot *rs = &ring->slot[last];
					// XXX less aggressive?
					rs->buf_idx = forward_packet(g + 1, rs);
					rs->flags = NS_BUF_CHANGED;
					rs->ptr = 0;
				}
				p->last_tail = last;
			}
		}



		if (oq) {
			/* try to push packets from the overflow queues
			 * to the corresponding pipes
			 */
			for (i = 0; i < npipes; i++) {
				struct port_des *p = &ports[i];
				struct overflow_queue *q = p->oq;
				uint32_t k;
				int64_t lim;
				struct netmap_ring *ring;
				struct netmap_slot *slot;
				struct morefrag *mf;

				if (oq_empty(q))
					continue;
				ring = p->ring;
				mf = (struct morefrag *)ring->sem;
				lim = ring->tail - mf->shadow_head;
				if (!lim)
					continue;
				if (lim < 0)
					lim += ring->num_slots;
				if (q->n < lim)
					lim = q->n;
				for (k = 0; k < lim; k++) {
					struct netmap_slot s = oq_deq(q), tmp;
					tmp.ptr = 0;
					slot = &ring->slot[mf->shadow_head];
					tmp.buf_idx = slot->buf_idx;
					oq_enq(freeq, &tmp);
					*slot = s;
					slot->flags |= NS_BUF_CHANGED;
					mf->shadow_head = nm_ring_next(ring, mf->shadow_head);
					if (!(slot->flags & NS_MOREFRAG))
						ring->head = mf->shadow_head;
				}
			}
		}

		/* push any new packets from the input port to the first group */
		int batch = 0;
		for (i = rxport->nmd->first_rx_ring; i <= rxport->nmd->last_rx_ring; i++) {
			struct netmap_ring *rxring = NETMAP_RXRING(rxport->nmd->nifp, i);
			struct morefrag *mf = (struct morefrag *)rxring->sem;

			//D("prepare to scan rings");
			int next_head = rxring->head;
			struct netmap_slot *next_slot = &rxring->slot[next_head];
			const char *next_buf = NETMAP_BUF(rxring, next_slot->buf_idx);
			while (!nm_ring_empty(rxring)) {
				struct netmap_slot *rs = next_slot;
				struct group_des *g = &groups[0];
				++received_pkts;
				received_bytes += rs->len;

				// CHOOSE THE CORRECT OUTPUT PIPE
				// If the previous slot had NS_MOREFRAG set, this is another
				// fragment of the last packet and it should go to the same
				// output pipe as before.
				if (!mf->last_flag) {
					// 'B' is just a hashing seed
					mf->last_hash = pkt_hdr_hash((const unsigned char *)next_buf, 4, 'B');
				}
				mf->last_flag = rs->flags & NS_MOREFRAG;
				rs->ptr = mf->last_hash;
				if (rs->ptr == 0) {
					non_ip++; // XXX ??
				}
				// prefetch the buffer for the next round
				next_head = nm_ring_next(rxring, next_head);
				next_slot = &rxring->slot[next_head];
				next_buf = NETMAP_BUF(rxring, next_slot->buf_idx);
				__builtin_prefetch(next_buf);
				rs->buf_idx = forward_packet(g, rs);
				rs->flags = NS_BUF_CHANGED;
				rxring->head = rxring->cur = next_head;

				batch++;
				if (unlikely(batch >= glob_arg.batch)) {
					ioctl(rxport->nmd->fd, NIOCRXSYNC, NULL);
					batch = 0;
				}
				ND(1,
				   "Forwarded Packets: %"PRIu64" Dropped packets: %"PRIu64"   Percent: %.2f",
				   forwarded, dropped,
				   ((float)dropped / (float)forwarded * 100));
			}

		}

	send_stats:
		if (counters_buf.status == COUNTERS_FULL)
			continue;
		/* take a new snapshot of the counters */
		gettimeofday(&counters_buf.ts, NULL);
		for (i = 0; i < npipes; i++) {
			struct my_ctrs *c = &counters_buf.ctrs[i];
			*c = ports[i].ctr;
			/*
			 * If there are overflow queues, copy the number of them for each
			 * port to the ctrs.oq_n variable for each port.
			 */
			if (ports[i].oq != NULL)
				c->oq_n = ports[i].oq->n;
		}
		counters_buf.received_pkts = received_pkts;
		counters_buf.received_bytes = received_bytes;
		counters_buf.non_ip = non_ip;
		if (freeq != NULL)
			counters_buf.freeq_n = freeq->n;
		__sync_synchronize();
		counters_buf.status = COUNTERS_FULL;
	}

	/*
	 * If freeq exists, copy the number to the freeq_n member of the
	 * message struct, otherwise set it to 0.
	 */
	if (freeq != NULL) {
		freeq_n = freeq->n;
	} else {
		freeq_n = 0;
	}

	pthread_join(stat_thread, NULL);

	printf("%"PRIu64" packets forwarded.  %"PRIu64" packets dropped. Total %"PRIu64"\n", forwarded,
	       dropped, forwarded + dropped);
	return 0;
}
