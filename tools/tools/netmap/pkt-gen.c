/*
 * Copyright (C) 2011-2012 Matteo Landi, Luigi Rizzo. All rights reserved.
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

/*
 * $FreeBSD$
 * $Id$
 *
 * Example program to show how to build a multithreaded packet
 * source/sink using the netmap device.
 *
 * In this example we create a programmable number of threads
 * to take care of all the queues of the interface used to
 * send or receive traffic.
 *
 */

#include "nm_util.h"

#include <ctype.h>	// isprint()

const char *default_payload="netmap pkt-gen payload\n"
	"http://info.iet.unipi.it/~luigi/netmap/ ";

int time_second;	// support for RD() debugging macro

int verbose = 0;

#define SKIP_PAYLOAD 1 /* do not check payload. */

struct pkt {
	struct ether_header eh;
	struct ip ip;
	struct udphdr udp;
	uint8_t body[2048];	// XXX hardwired
} __attribute__((__packed__));

struct ip_range {
	char *name;
	struct in_addr start, end, cur;
	uint16_t port0, port1, cur_p;
};

struct mac_range {
	char *name;
	struct ether_addr start, end;
};

/*
 * global arguments for all threads
 */

struct glob_arg {
	struct ip_range src_ip;
	struct ip_range dst_ip;
	struct mac_range dst_mac;
	struct mac_range src_mac;
	int pkt_size;
	int burst;
	int forever;
	int npackets;	/* total packets to send */
	int nthreads;
	int cpus;
	int options;	/* testing */
#define OPT_PREFETCH	1
#define OPT_ACCESS	2
#define OPT_COPY	4
#define OPT_MEMCPY	8
#define OPT_TS		16	/* add a timestamp */
#define OPT_INDIRECT	32	/* use indirect buffers, tx only */
#define OPT_DUMP	64	/* dump rx/tx traffic */
	int dev_type;
	pcap_t *p;

	int tx_rate;
	struct timespec tx_period;

	int affinity;
	int main_fd;
	int report_interval;
	void *(*td_body)(void *);
	void *mmap_addr;
	int mmap_size;
	char *ifname;
};
enum dev_type { DEV_NONE, DEV_NETMAP, DEV_PCAP, DEV_TAP };


/*
 * Arguments for a new thread. The same structure is used by
 * the source and the sink
 */
struct targ {
	struct glob_arg *g;
	int used;
	int completed;
	int cancel;
	int fd;
	struct nmreq nmr;
	struct netmap_if *nifp;
	uint16_t	qfirst, qlast; /* range of queues to scan */
	volatile uint64_t count;
	struct timespec tic, toc;
	int me;
	pthread_t thread;
	int affinity;

	struct pkt pkt;
};


/*
 * extract the extremes from a range of ipv4 addresses.
 * addr_lo[-addr_hi][:port_lo[-port_hi]]
 */
static void
extract_ip_range(struct ip_range *r)
{
	char *p_lo, *p_hi;
	char buf1[16]; // one ip address

	D("extract IP range from %s", r->name);
	p_lo = index(r->name, ':');	/* do we have ports ? */
	if (p_lo) {
		D(" found ports at %s", p_lo);
		*p_lo++ = '\0';
		p_hi = index(p_lo, '-');
		if (p_hi)
			*p_hi++ = '\0';
		else
			p_hi = p_lo;
		r->port0 = strtol(p_lo, NULL, 0);
		r->port1 = strtol(p_hi, NULL, 0);
		if (r->port1 < r->port0) {
			r->cur_p = r->port0;
			r->port0 = r->port1;
			r->port1 = r->cur_p;
		}
		r->cur_p = r->port0;
		D("ports are %d to %d", r->port0, r->port1);
	}
	p_hi = index(r->name, '-');	/* do we have upper ip ? */
	if (p_hi) {
		*p_hi++ = '\0';
	} else
		p_hi = r->name;
	inet_aton(r->name, &r->start);
	inet_aton(p_hi, &r->end);
	if (r->start.s_addr > r->end.s_addr) {
		r->cur = r->start;
		r->start = r->end;
		r->end = r->cur;
	}
	r->cur = r->start;
	strncpy(buf1, inet_ntoa(r->end), sizeof(buf1));
	D("range is %s %d to %s %d", inet_ntoa(r->start), r->port0,
		buf1, r->port1);
}

static void
extract_mac_range(struct mac_range *r)
{
	D("extract MAC range from %s", r->name);
	bcopy(ether_aton(r->name), &r->start, 6);
	bcopy(ether_aton(r->name), &r->end, 6);
#if 0
	bcopy(targ->src_mac, eh->ether_shost, 6);
	p = index(targ->g->src_mac, '-');
	if (p)
		targ->src_mac_range = atoi(p+1);

	bcopy(ether_aton(targ->g->dst_mac), targ->dst_mac, 6);
	bcopy(targ->dst_mac, eh->ether_dhost, 6);
	p = index(targ->g->dst_mac, '-');
	if (p)
		targ->dst_mac_range = atoi(p+1);
#endif
	D("%s starts at %s", r->name, ether_ntoa(&r->start));
}

static struct targ *targs;
static int global_nthreads;

/* control-C handler */
static void
sigint_h(int sig)
{
	int i;

	(void)sig;	/* UNUSED */
	for (i = 0; i < global_nthreads; i++) {
		targs[i].cancel = 1;
	}
	signal(SIGINT, SIG_DFL);
}

/* sysctl wrapper to return the number of active CPUs */
static int
system_ncpus(void)
{
#ifdef __FreeBSD__
	int mib[2], ncpus;
	size_t len;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(mib);
	sysctl(mib, 2, &ncpus, &len, NULL, 0);

	return (ncpus);
#else
	return 1;
#endif /* !__FreeBSD__ */
}

#ifdef __linux__
#define sockaddr_dl    sockaddr_ll
#define sdl_family     sll_family
#define AF_LINK        AF_PACKET
#define LLADDR(s)      s->sll_addr;
#include <linux/if_tun.h>
#define TAP_CLONEDEV	"/dev/net/tun"
#endif /* __linux__ */

#ifdef __FreeBSD__
#include <net/if_tun.h>
#define TAP_CLONEDEV	"/dev/tap"
#endif /* __FreeBSD */

#ifdef __APPLE__
// #warning TAP not supported on apple ?
#include <net/if_utun.h>
#define TAP_CLONEDEV	"/dev/tap"
#endif /* __APPLE__ */


/*
 * locate the src mac address for our interface, put it
 * into the user-supplied buffer. return 0 if ok, -1 on error.
 */
static int
source_hwaddr(const char *ifname, char *buf)
{
	struct ifaddrs *ifaphead, *ifap;
	int l = sizeof(ifap->ifa_name);

	if (getifaddrs(&ifaphead) != 0) {
		D("getifaddrs %s failed", ifname);
		return (-1);
	}

	for (ifap = ifaphead; ifap; ifap = ifap->ifa_next) {
		struct sockaddr_dl *sdl =
			(struct sockaddr_dl *)ifap->ifa_addr;
		uint8_t *mac;

		if (!sdl || sdl->sdl_family != AF_LINK)
			continue;
		if (strncmp(ifap->ifa_name, ifname, l) != 0)
			continue;
		mac = (uint8_t *)LLADDR(sdl);
		sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
			mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
		if (verbose)
			D("source hwaddr %s", buf);
		break;
	}
	freeifaddrs(ifaphead);
	return ifap ? 0 : 1;
}


/* set the thread affinity. */
static int
setaffinity(pthread_t me, int i)
{
#ifdef __FreeBSD__
	cpuset_t cpumask;

	if (i == -1)
		return 0;

	/* Set thread affinity affinity.*/
	CPU_ZERO(&cpumask);
	CPU_SET(i, &cpumask);

	if (pthread_setaffinity_np(me, sizeof(cpuset_t), &cpumask) != 0) {
		D("Unable to set affinity");
		return 1;
	}
#else
	(void)me; /* suppress 'unused' warnings */
	(void)i;
#endif /* __FreeBSD__ */
	return 0;
}

/* Compute the checksum of the given ip header. */
static uint16_t
checksum(const void *data, uint16_t len, uint32_t sum)
{
        const uint8_t *addr = data;
	uint32_t i;

        /* Checksum all the pairs of bytes first... */
        for (i = 0; i < (len & ~1U); i += 2) {
                sum += (u_int16_t)ntohs(*((u_int16_t *)(addr + i)));
                if (sum > 0xFFFF)
                        sum -= 0xFFFF;
        }
	/*
	 * If there's a single byte left over, checksum it, too.
	 * Network byte order is big-endian, so the remaining byte is
	 * the high byte.
	 */
	if (i < len) {
		sum += addr[i] << 8;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return sum;
}

static u_int16_t
wrapsum(u_int32_t sum)
{
	sum = ~sum & 0xFFFF;
	return (htons(sum));
}

/* Check the payload of the packet for errors (use it for debug).
 * Look for consecutive ascii representations of the size of the packet.
 */
static void
dump_payload(char *p, int len, struct netmap_ring *ring, int cur)
{
	char buf[128];
	int i, j, i0;

	/* get the length in ASCII of the length of the packet. */
	
	printf("ring %p cur %5d len %5d buf %p\n", ring, cur, len, p);
	/* hexdump routine */
	for (i = 0; i < len; ) {
		memset(buf, sizeof(buf), ' ');
		sprintf(buf, "%5d: ", i);
		i0 = i;
		for (j=0; j < 16 && i < len; i++, j++)
			sprintf(buf+7+j*3, "%02x ", (uint8_t)(p[i]));
		i = i0;
		for (j=0; j < 16 && i < len; i++, j++)
			sprintf(buf+7+j + 48, "%c",
				isprint(p[i]) ? p[i] : '.');
		printf("%s\n", buf);
	}
}

/*
 * Fill a packet with some payload.
 * We create a UDP packet so the payload starts at
 *	14+20+8 = 42 bytes.
 */
#ifdef __linux__
#define uh_sport source
#define uh_dport dest
#define uh_ulen len
#define uh_sum check
#endif /* linux */

static void
initialize_packet(struct targ *targ)
{
	struct pkt *pkt = &targ->pkt;
	struct ether_header *eh;
	struct ip *ip;
	struct udphdr *udp;
	uint16_t paylen = targ->g->pkt_size - sizeof(*eh) - sizeof(struct ip);
	const char *payload = targ->g->options & OPT_INDIRECT ?
		"XXXXXXXXXXXXXXXXXXXXXX" : default_payload;
	int i, l, l0 = strlen(payload);

	for (i = 0; i < paylen;) {
		l = min(l0, paylen - i);
		bcopy(payload, pkt->body + i, l);
		i += l;
	}
	pkt->body[i-1] = '\0';
	ip = &pkt->ip;

        ip->ip_v = IPVERSION;
        ip->ip_hl = 5;
        ip->ip_id = 0;
        ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_len = ntohs(targ->g->pkt_size - sizeof(*eh));
        ip->ip_id = 0;
        ip->ip_off = htons(IP_DF); /* Don't fragment */
        ip->ip_ttl = IPDEFTTL;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_dst.s_addr = targ->g->dst_ip.cur.s_addr;
	if (++targ->g->dst_ip.cur.s_addr > targ->g->dst_ip.end.s_addr)
		targ->g->dst_ip.cur.s_addr = targ->g->dst_ip.start.s_addr;
	ip->ip_src.s_addr = targ->g->src_ip.cur.s_addr;
	if (++targ->g->src_ip.cur.s_addr > targ->g->src_ip.end.s_addr)
		targ->g->src_ip.cur.s_addr = targ->g->src_ip.start.s_addr;
	ip->ip_sum = wrapsum(checksum(ip, sizeof(*ip), 0));


	udp = &pkt->udp;
        udp->uh_sport = htons(targ->g->src_ip.cur_p);
	if (++targ->g->src_ip.cur_p > targ->g->src_ip.port1)
		targ->g->src_ip.cur_p = targ->g->src_ip.port0;
        udp->uh_dport = htons(targ->g->dst_ip.cur_p);
	if (++targ->g->dst_ip.cur_p > targ->g->dst_ip.port1)
		targ->g->dst_ip.cur_p = targ->g->dst_ip.port0;
	udp->uh_ulen = htons(paylen);
	/* Magic: taken from sbin/dhclient/packet.c */
	udp->uh_sum = wrapsum(checksum(udp, sizeof(*udp),
                    checksum(pkt->body,
                        paylen - sizeof(*udp),
                        checksum(&ip->ip_src, 2 * sizeof(ip->ip_src),
                            IPPROTO_UDP + (u_int32_t)ntohs(udp->uh_ulen)
                        )
                    )
                ));

	eh = &pkt->eh;
	bcopy(&targ->g->src_mac.start, eh->ether_shost, 6);
	bcopy(&targ->g->dst_mac.start, eh->ether_dhost, 6);
	eh->ether_type = htons(ETHERTYPE_IP);
	// dump_payload((void *)pkt, targ->g->pkt_size, NULL, 0);
}



/*
 * create and enqueue a batch of packets on a ring.
 * On the last one set NS_REPORT to tell the driver to generate
 * an interrupt when done.
 */
static int
send_packets(struct netmap_ring *ring, struct pkt *pkt, 
		int size, u_int count, int options)
{
	u_int sent, cur = ring->cur;

	if (ring->avail < count)
		count = ring->avail;

#if 0
	if (options & (OPT_COPY | OPT_PREFETCH) ) {
		for (sent = 0; sent < count; sent++) {
			struct netmap_slot *slot = &ring->slot[cur];
			char *p = NETMAP_BUF(ring, slot->buf_idx);

			prefetch(p);
			cur = NETMAP_RING_NEXT(ring, cur);
		}
		cur = ring->cur;
	}
#endif
	for (sent = 0; sent < count; sent++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);

		slot->flags = 0;
		if (options & OPT_DUMP)
			dump_payload(p, size, ring, cur);
		if (options & OPT_INDIRECT) {
			slot->flags |= NS_INDIRECT;
			*((struct pkt **)(void *)p) = pkt;
		} else if (options & OPT_COPY)
			pkt_copy(pkt, p, size);
		else if (options & OPT_MEMCPY)
			memcpy(p, pkt, size);
		else if (options & OPT_PREFETCH)
			prefetch(p);
		slot->len = size;
		if (sent == count - 1)
			slot->flags |= NS_REPORT;
		cur = NETMAP_RING_NEXT(ring, cur);
	}
	ring->avail -= sent;
	ring->cur = cur;

	return (sent);
}

/*
 * Send a packet, and wait for a response.
 * The payload (after UDP header, ofs 42) has a 4-byte sequence
 * followed by a struct timeval (or bintime?)
 */
#define	PAY_OFS	42	/* where in the pkt... */

static void *
pinger_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd fds[1];
	struct netmap_if *nifp = targ->nifp;
	int i, rx = 0, n = targ->g->npackets;

	fds[0].fd = targ->fd;
	fds[0].events = (POLLIN);
	static uint32_t sent;
	struct timespec ts, now, last_print;
	uint32_t count = 0, min = 1000000000, av = 0;

	if (targ->g->nthreads > 1) {
		D("can only ping with 1 thread");
		return NULL;
	}

	clock_gettime(CLOCK_REALTIME_PRECISE, &last_print);
	while (n == 0 || (int)sent < n) {
		struct netmap_ring *ring = NETMAP_TXRING(nifp, 0);
		struct netmap_slot *slot;
		char *p;
	    for (i = 0; i < 1; i++) {
		slot = &ring->slot[ring->cur];
		slot->len = targ->g->pkt_size;
		p = NETMAP_BUF(ring, slot->buf_idx);

		if (ring->avail == 0) {
			D("-- ouch, cannot send");
		} else {
			pkt_copy(&targ->pkt, p, targ->g->pkt_size);
			clock_gettime(CLOCK_REALTIME_PRECISE, &ts);
			bcopy(&sent, p+42, sizeof(sent));
			bcopy(&ts, p+46, sizeof(ts));
			sent++;
			ring->cur = NETMAP_RING_NEXT(ring, ring->cur);
			ring->avail--;
		}
	    }
		/* should use a parameter to decide how often to send */
		if (poll(fds, 1, 3000) <= 0) {
			D("poll error/timeout on queue %d", targ->me);
			continue;
		}
		/* see what we got back */
		for (i = targ->qfirst; i < targ->qlast; i++) {
			ring = NETMAP_RXRING(nifp, i);
			while (ring->avail > 0) {
				uint32_t seq;
				slot = &ring->slot[ring->cur];
				p = NETMAP_BUF(ring, slot->buf_idx);

				clock_gettime(CLOCK_REALTIME_PRECISE, &now);
				bcopy(p+42, &seq, sizeof(seq));
				bcopy(p+46, &ts, sizeof(ts));
				ts.tv_sec = now.tv_sec - ts.tv_sec;
				ts.tv_nsec = now.tv_nsec - ts.tv_nsec;
				if (ts.tv_nsec < 0) {
					ts.tv_nsec += 1000000000;
					ts.tv_sec--;
				}
				if (1) D("seq %d/%d delta %d.%09d", seq, sent,
					(int)ts.tv_sec, (int)ts.tv_nsec);
				if (ts.tv_nsec < (int)min)
					min = ts.tv_nsec;
				count ++;
				av += ts.tv_nsec;
				ring->avail--;
				ring->cur = NETMAP_RING_NEXT(ring, ring->cur);
				rx++;
			}
		}
		//D("tx %d rx %d", sent, rx);
		//usleep(100000);
		ts.tv_sec = now.tv_sec - last_print.tv_sec;
		ts.tv_nsec = now.tv_nsec - last_print.tv_nsec;
		if (ts.tv_nsec < 0) {
			ts.tv_nsec += 1000000000;
			ts.tv_sec--;
		}
		if (ts.tv_sec >= 1) {
			D("count %d min %d av %d",
				count, min, av/count);
			count = 0;
			av = 0;
			min = 100000000;
			last_print = now;
		}
	}
	return NULL;
}


/*
 * reply to ping requests
 */
static void *
ponger_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd fds[1];
	struct netmap_if *nifp = targ->nifp;
	struct netmap_ring *txring, *rxring;
	int i, rx = 0, sent = 0, n = targ->g->npackets;
	fds[0].fd = targ->fd;
	fds[0].events = (POLLIN);

	if (targ->g->nthreads > 1) {
		D("can only reply ping with 1 thread");
		return NULL;
	}
	D("understood ponger %d but don't know how to do it", n);
	while (n == 0 || sent < n) {
		uint32_t txcur, txavail;
//#define BUSYWAIT
#ifdef BUSYWAIT
		ioctl(fds[0].fd, NIOCRXSYNC, NULL);
#else
		if (poll(fds, 1, 1000) <= 0) {
			D("poll error/timeout on queue %d", targ->me);
			continue;
		}
#endif
		txring = NETMAP_TXRING(nifp, 0);
		txcur = txring->cur;
		txavail = txring->avail;
		/* see what we got back */
		for (i = targ->qfirst; i < targ->qlast; i++) {
			rxring = NETMAP_RXRING(nifp, i);
			while (rxring->avail > 0) {
				uint16_t *spkt, *dpkt;
				uint32_t cur = rxring->cur;
				struct netmap_slot *slot = &rxring->slot[cur];
				char *src, *dst;
				src = NETMAP_BUF(rxring, slot->buf_idx);
				//D("got pkt %p of size %d", src, slot->len);
				rxring->avail--;
				rxring->cur = NETMAP_RING_NEXT(rxring, cur);
				rx++;
				if (txavail == 0)
					continue;
				dst = NETMAP_BUF(txring,
				    txring->slot[txcur].buf_idx);
				/* copy... */
				dpkt = (uint16_t *)dst;
				spkt = (uint16_t *)src;
				pkt_copy(src, dst, slot->len);
				dpkt[0] = spkt[3];
				dpkt[1] = spkt[4];
				dpkt[2] = spkt[5];
				dpkt[3] = spkt[0];
				dpkt[4] = spkt[1];
				dpkt[5] = spkt[2];
				txring->slot[txcur].len = slot->len;
				/* XXX swap src dst mac */
				txcur = NETMAP_RING_NEXT(txring, txcur);
				txavail--;
				sent++;
			}
		}
		txring->cur = txcur;
		txring->avail = txavail;
		targ->count = sent;
#ifdef BUSYWAIT
		ioctl(fds[0].fd, NIOCTXSYNC, NULL);
#endif
		//D("tx %d rx %d", sent, rx);
	}
	return NULL;
}

static __inline int
timespec_ge(const struct timespec *a, const struct timespec *b)
{

	if (a->tv_sec > b->tv_sec)
		return (1);
	if (a->tv_sec < b->tv_sec)
		return (0);
	if (a->tv_nsec >= b->tv_nsec)
		return (1);
	return (0);
}

static __inline struct timespec
timeval2spec(const struct timeval *a)
{
	struct timespec ts = {
		.tv_sec = a->tv_sec,
		.tv_nsec = a->tv_usec * 1000
	};
	return ts;
}

static __inline struct timeval
timespec2val(const struct timespec *a)
{
	struct timeval tv = {
		.tv_sec = a->tv_sec,
		.tv_usec = a->tv_nsec / 1000
	};
	return tv;
}


static int
wait_time(struct timespec ts, struct timespec *wakeup_ts, long long *waited)
{
	struct timespec curtime;

	curtime.tv_sec = 0;
	curtime.tv_nsec = 0;

	if (clock_gettime(CLOCK_REALTIME_PRECISE, &curtime) == -1) {
		D("clock_gettime: %s", strerror(errno));
		return (-1);
	}
	while (timespec_ge(&ts, &curtime)) {
		if (waited != NULL)
			(*waited)++;
		if (clock_gettime(CLOCK_REALTIME_PRECISE, &curtime) == -1) {
			D("clock_gettime");
			return (-1);
		}
	}
	if (wakeup_ts != NULL)
		*wakeup_ts = curtime;
	return (0);
}

static __inline void
timespec_add(struct timespec *tsa, struct timespec *tsb)
{
	tsa->tv_sec += tsb->tv_sec;
	tsa->tv_nsec += tsb->tv_nsec;
	if (tsa->tv_nsec >= 1000000000) {
		tsa->tv_sec++;
		tsa->tv_nsec -= 1000000000;
	}
}


static void *
sender_body(void *data)
{
	struct targ *targ = (struct targ *) data;

	struct pollfd fds[1];
	struct netmap_if *nifp = targ->nifp;
	struct netmap_ring *txring;
	int i, n = targ->g->npackets / targ->g->nthreads, sent = 0;
	int options = targ->g->options | OPT_COPY;
	struct timespec tmptime, nexttime = { 0, 0}; // XXX silence compiler
	int rate_limit = targ->g->tx_rate;
	long long waited = 0;

	D("start");
	if (setaffinity(targ->thread, targ->affinity))
		goto quit;
	/* setup poll(2) mechanism. */
	memset(fds, 0, sizeof(fds));
	fds[0].fd = targ->fd;
	fds[0].events = (POLLOUT);

	/* main loop.*/
	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->tic);
	if (rate_limit) {
		tmptime.tv_sec = 2;
		tmptime.tv_nsec = 0;
		timespec_add(&targ->tic, &tmptime);
		targ->tic.tv_nsec = 0;
		if (wait_time(targ->tic, NULL, NULL) == -1) {
			D("wait_time: %s", strerror(errno));
			goto quit;
		}
		nexttime = targ->tic;
	}
    if (targ->g->dev_type == DEV_PCAP) {
	    int size = targ->g->pkt_size;
	    void *pkt = &targ->pkt;
	    pcap_t *p = targ->g->p;

	    for (i = 0; !targ->cancel && (n == 0 || sent < n); i++) {
		if (pcap_inject(p, pkt, size) != -1)
			sent++;
		if (i > 10000) {
			targ->count = sent;
			i = 0;
		}
	    }
    } else if (targ->g->dev_type == DEV_TAP) { /* tap */
	    int size = targ->g->pkt_size;
	    void *pkt = &targ->pkt;
	    D("writing to file desc %d", targ->g->main_fd);

	    for (i = 0; !targ->cancel && (n == 0 || sent < n); i++) {
		if (write(targ->g->main_fd, pkt, size) != -1)
			sent++;
		if (i > 10000) {
			targ->count = sent;
			i = 0;
		}
	    }
    } else {
	int tosend = 0;
	while (!targ->cancel && (n == 0 || sent < n)) {

		if (rate_limit && tosend <= 0) {
			tosend = targ->g->burst;
			timespec_add(&nexttime, &targ->g->tx_period);
			if (wait_time(nexttime, &tmptime, &waited) == -1) {
				D("wait_time");
				goto quit;
			}
		}

		/*
		 * wait for available room in the send queue(s)
		 */
		if (poll(fds, 1, 2000) <= 0) {
			if (targ->cancel)
				break;
			D("poll error/timeout on queue %d", targ->me);
			goto quit;
		}
		/*
		 * scan our queues and send on those with room
		 */
		if (options & OPT_COPY && sent > 100000 && !(targ->g->options & OPT_COPY) ) {
			D("drop copy");
			options &= ~OPT_COPY;
		}
		for (i = targ->qfirst; i < targ->qlast; i++) {
			int m, limit = rate_limit ?  tosend : targ->g->burst;
			if (n > 0 && n - sent < limit)
				limit = n - sent;
			txring = NETMAP_TXRING(nifp, i);
			if (txring->avail == 0)
				continue;
			m = send_packets(txring, &targ->pkt, targ->g->pkt_size,
					 limit, options);
			sent += m;
			tosend -= m;
			targ->count = sent;
		}
	}
	/* flush any remaining packets */
	ioctl(fds[0].fd, NIOCTXSYNC, NULL);

	/* final part: wait all the TX queues to be empty. */
	for (i = targ->qfirst; i < targ->qlast; i++) {
		txring = NETMAP_TXRING(nifp, i);
		while (!NETMAP_TX_RING_EMPTY(txring)) {
			ioctl(fds[0].fd, NIOCTXSYNC, NULL);
			usleep(1); /* wait 1 tick */
		}
	}
    }

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);
	targ->completed = 1;
	targ->count = sent;

quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}


static void
receive_pcap(u_char *user, const struct pcap_pkthdr * h,
	const u_char * bytes)
{
	int *count = (int *)user;
	(void)h;	/* UNUSED */
	(void)bytes;	/* UNUSED */
	(*count)++;
}

static int
receive_packets(struct netmap_ring *ring, u_int limit, int dump)
{
	u_int cur, rx;

	cur = ring->cur;
	if (ring->avail < limit)
		limit = ring->avail;
	for (rx = 0; rx < limit; rx++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);

		slot->flags = OPT_INDIRECT; // XXX
		if (dump)
			dump_payload(p, slot->len, ring, cur);

		cur = NETMAP_RING_NEXT(ring, cur);
	}
	ring->avail -= rx;
	ring->cur = cur;

	return (rx);
}

static void *
receiver_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd fds[1];
	struct netmap_if *nifp = targ->nifp;
	struct netmap_ring *rxring;
	int i;
	uint64_t received = 0;

	if (setaffinity(targ->thread, targ->affinity))
		goto quit;

	/* setup poll(2) mechanism. */
	memset(fds, 0, sizeof(fds));
	fds[0].fd = targ->fd;
	fds[0].events = (POLLIN);

	/* unbounded wait for the first packet. */
	for (;;) {
		i = poll(fds, 1, 1000);
		if (i > 0 && !(fds[0].revents & POLLERR))
			break;
		D("waiting for initial packets, poll returns %d %d", i, fds[0].revents);
	}

	/* main loop, exit after 1s silence */
	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->tic);
    if (targ->g->dev_type == DEV_PCAP) {
	while (!targ->cancel) {
		/* XXX should we poll ? */
		pcap_dispatch(targ->g->p, targ->g->burst, receive_pcap, NULL);
	}
    } else if (targ->g->dev_type == DEV_TAP) {
	D("reading from %s fd %d", targ->g->ifname, targ->g->main_fd);
	while (!targ->cancel) {
		char buf[2048];
		/* XXX should we poll ? */
		if (read(targ->g->main_fd, buf, sizeof(buf)) > 0)
			targ->count++;
	}
    } else {
	int dump = targ->g->options & OPT_DUMP;
	while (!targ->cancel) {
		/* Once we started to receive packets, wait at most 1 seconds
		   before quitting. */
		if (poll(fds, 1, 1 * 1000) <= 0 && !targ->g->forever) {
			clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);
			targ->toc.tv_sec -= 1; /* Subtract timeout time. */
			break;
		}

		for (i = targ->qfirst; i < targ->qlast; i++) {
			int m;

			rxring = NETMAP_RXRING(nifp, i);
			if (rxring->avail == 0)
				continue;

			m = receive_packets(rxring, targ->g->burst, dump);
			received += m;
		}
		targ->count = received;

		// tell the card we have read the data
		//ioctl(fds[0].fd, NIOCRXSYNC, NULL);
	}
    }

	targ->completed = 1;
	targ->count = received;

quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}

/* very crude code to print a number in normalized form.
 * Caller has to make sure that the buffer is large enough.
 */
static const char *
norm(char *buf, double val)
{
	char *units[] = { "", "K", "M", "G" };
	u_int i;

	for (i = 0; val >=1000 && i < sizeof(units)/sizeof(char *); i++)
		val /= 1000;
	sprintf(buf, "%.2f %s", val, units[i]);
	return buf;
}

static void
tx_output(uint64_t sent, int size, double delta)
{
	double bw, raw_bw, pps;
	char b1[40], b2[80], b3[80];

	printf("Sent %" PRIu64 " packets, %d bytes each, in %.2f seconds.\n",
	       sent, size, delta);
	if (delta == 0)
		delta = 1e-6;
	if (size < 60)		/* correct for min packet size */
		size = 60;
	pps = sent / delta;
	bw = (8.0 * size * sent) / delta;
	/* raw packets have4 bytes crc + 20 bytes framing */
	raw_bw = (8.0 * (size + 24) * sent) / delta;

	printf("Speed: %spps Bandwidth: %sbps (raw %sbps)\n",
		norm(b1, pps), norm(b2, bw), norm(b3, raw_bw) );
}


static void
rx_output(uint64_t received, double delta)
{
	double pps;
	char b1[40];

	printf("Received %" PRIu64 " packets, in %.2f seconds.\n", received, delta);

	if (delta == 0)
		delta = 1e-6;
	pps = received / delta;
	printf("Speed: %spps\n", norm(b1, pps));
}

static void
usage(void)
{
	const char *cmd = "pkt-gen";
	fprintf(stderr,
		"Usage:\n"
		"%s arguments\n"
		"\t-i interface		interface name\n"
		"\t-f function		tx rx ping pong\n"
		"\t-n count		number of iterations (can be 0)\n"
		"\t-t pkts_to_send		also forces tx mode\n"
		"\t-r pkts_to_receive	also forces rx mode\n"
		"\t-l pkts_size		in bytes excluding CRC\n"
		"\t-d dst-ip		end with %%n to sweep n addresses\n"
		"\t-s src-ip		end with %%n to sweep n addresses\n"
		"\t-D dst-mac		end with %%n to sweep n addresses\n"
		"\t-S src-mac		end with %%n to sweep n addresses\n"
		"\t-a cpu_id		use setaffinity\n"
		"\t-b burst size		testing, mostly\n"
		"\t-c cores		cores to use\n"
		"\t-p threads		processes/threads to use\n"
		"\t-T report_ms		milliseconds between reports\n"
		"\t-P				use libpcap instead of netmap\n"
		"\t-w wait_for_link_time	in seconds\n"
		"",
		cmd);

	exit(0);
}

static void
start_threads(struct glob_arg *g)
{
	int i;

	targs = calloc(g->nthreads, sizeof(*targs));
	/*
	 * Now create the desired number of threads, each one
	 * using a single descriptor.
 	 */
	for (i = 0; i < g->nthreads; i++) {
		bzero(&targs[i], sizeof(targs[i]));
		targs[i].fd = -1; /* default, with pcap */
		targs[i].g = g;

	    if (g->dev_type == DEV_NETMAP) {
		struct nmreq tifreq;
		int tfd;

		/* register interface. */
		tfd = open("/dev/netmap", O_RDWR);
		if (tfd == -1) {
			D("Unable to open /dev/netmap");
			continue;
		}
		targs[i].fd = tfd;

		bzero(&tifreq, sizeof(tifreq));
		strncpy(tifreq.nr_name, g->ifname, sizeof(tifreq.nr_name));
		tifreq.nr_version = NETMAP_API;
		tifreq.nr_ringid = (g->nthreads > 1) ? (i | NETMAP_HW_RING) : 0;

		/*
		 * if we are acting as a receiver only, do not touch the transmit ring.
		 * This is not the default because many apps may use the interface
		 * in both directions, but a pure receiver does not.
		 */
		if (g->td_body == receiver_body) {
			tifreq.nr_ringid |= NETMAP_NO_TX_POLL;
		}

		if ((ioctl(tfd, NIOCREGIF, &tifreq)) == -1) {
			D("Unable to register %s", g->ifname);
			continue;
		}
		targs[i].nmr = tifreq;
		targs[i].nifp = NETMAP_IF(g->mmap_addr, tifreq.nr_offset);
		/* start threads. */
		targs[i].qfirst = (g->nthreads > 1) ? i : 0;
		targs[i].qlast = (g->nthreads > 1) ? i+1 :
			(g->td_body == receiver_body ? tifreq.nr_rx_rings : tifreq.nr_tx_rings);
	    } else {
		targs[i].fd = g->main_fd;
	    }
		targs[i].used = 1;
		targs[i].me = i;
		if (g->affinity >= 0) {
			if (g->affinity < g->cpus)
				targs[i].affinity = g->affinity;
			else
				targs[i].affinity = i % g->cpus;
		} else
			targs[i].affinity = -1;
		/* default, init packets */
		initialize_packet(&targs[i]);

		if (pthread_create(&targs[i].thread, NULL, g->td_body,
				   &targs[i]) == -1) {
			D("Unable to create thread %d", i);
			targs[i].used = 0;
		}
	}
}

static void
main_thread(struct glob_arg *g)
{
	int i;

	uint64_t prev = 0;
	uint64_t count = 0;
	double delta_t;
	struct timeval tic, toc;

	gettimeofday(&toc, NULL);
	for (;;) {
		struct timeval now, delta;
		uint64_t pps, usec, my_count, npkts;
		int done = 0;

		delta.tv_sec = g->report_interval/1000;
		delta.tv_usec = (g->report_interval%1000)*1000;
		select(0, NULL, NULL, NULL, &delta);
		gettimeofday(&now, NULL);
		time_second = now.tv_sec;
		timersub(&now, &toc, &toc);
		my_count = 0;
		for (i = 0; i < g->nthreads; i++) {
			my_count += targs[i].count;
			if (targs[i].used == 0)
				done++;
		}
		usec = toc.tv_sec* 1000000 + toc.tv_usec;
		if (usec < 10000)
			continue;
		npkts = my_count - prev;
		pps = (npkts*1000000 + usec/2) / usec;
		D("%" PRIu64 " pps (%" PRIu64 " pkts in %" PRIu64 " usec)",
			pps, npkts, usec);
		prev = my_count;
		toc = now;
		if (done == g->nthreads)
			break;
	}

	timerclear(&tic);
	timerclear(&toc);
	for (i = 0; i < g->nthreads; i++) {
		struct timespec t_tic, t_toc;
		/*
		 * Join active threads, unregister interfaces and close
		 * file descriptors.
		 */
		if (targs[i].used)
			pthread_join(targs[i].thread, NULL);
		close(targs[i].fd);

		if (targs[i].completed == 0)
			D("ouch, thread %d exited with error", i);

		/*
		 * Collect threads output and extract information about
		 * how long it took to send all the packets.
		 */
		count += targs[i].count;
		t_tic = timeval2spec(&tic);
		t_toc = timeval2spec(&toc);
		if (!timerisset(&tic) || timespec_ge(&targs[i].tic, &t_tic))
			tic = timespec2val(&targs[i].tic);
		if (!timerisset(&toc) || timespec_ge(&targs[i].toc, &t_toc))
			toc = timespec2val(&targs[i].toc);
	}

	/* print output. */
	timersub(&toc, &tic, &toc);
	delta_t = toc.tv_sec + 1e-6* toc.tv_usec;
	if (g->td_body == sender_body)
		tx_output(count, g->pkt_size, delta_t);
	else
		rx_output(count, delta_t);

	if (g->dev_type == DEV_NETMAP) {
		munmap(g->mmap_addr, g->mmap_size);
		close(g->main_fd);
	}
}


struct sf {
	char *key;
	void *f;
};

static struct sf func[] = {
	{ "tx",	sender_body },
	{ "rx",	receiver_body },
	{ "ping",	pinger_body },
	{ "pong",	ponger_body },
	{ NULL, NULL }
};

static int
tap_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;
	char *clonedev = TAP_CLONEDEV;

	(void)err;
	(void)dev;
	/* Arguments taken by the function:
	 *
	 * char *dev: the name of an interface (or '\0'). MUST have enough
	 *   space to hold the interface name if '\0' is passed
	 * int flags: interface flags (eg, IFF_TUN etc.)
	 */

#ifdef __FreeBSD__
	if (dev[3]) { /* tapSomething */
		static char buf[128];
		snprintf(buf, sizeof(buf), "/dev/%s", dev);
		clonedev = buf;
	}
#endif
	/* open the device */
	if( (fd = open(clonedev, O_RDWR)) < 0 ) {
		return fd;
	}
	D("%s open successful", clonedev);

	/* preparation of the struct ifr, of type "struct ifreq" */
	memset(&ifr, 0, sizeof(ifr));

#ifdef linux
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

	if (*dev) {
		/* if a device name was specified, put it in the structure; otherwise,
		* the kernel will try to allocate the "next" device of the
		* specified type */
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	/* try to create the device */
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
		D("failed to to a TUNSETIFF");
		close(fd);
		return err;
	}

	/* if the operation was successful, write back the name of the
	* interface to the variable "dev", so the caller can know
	* it. Note that the caller MUST reserve space in *dev (see calling
	* code below) */
	strcpy(dev, ifr.ifr_name);
	D("new name is %s", dev);
#endif /* linux */

        /* this is the special file descriptor that the caller will use to talk
         * with the virtual interface */
        return fd;
}

int
main(int arc, char **argv)
{
	int i;

	struct glob_arg g;

	struct nmreq nmr;
	int ch;
	int wait_link = 2;
	int devqueues = 1;	/* how many device queues */

	bzero(&g, sizeof(g));

	g.main_fd = -1;
	g.td_body = receiver_body;
	g.report_interval = 1000;	/* report interval */
	g.affinity = -1;
	/* ip addresses can also be a range x.x.x.x-x.x.x.y */
	g.src_ip.name = "10.0.0.1";
	g.dst_ip.name = "10.1.0.1";
	g.dst_mac.name = "ff:ff:ff:ff:ff:ff";
	g.src_mac.name = NULL;
	g.pkt_size = 60;
	g.burst = 512;		// default
	g.nthreads = 1;
	g.cpus = 1;
	g.forever = 1;
	g.tx_rate = 0;

	while ( (ch = getopt(arc, argv,
			"a:f:n:i:It:r:l:d:s:D:S:b:c:o:p:PT:w:WvR:X")) != -1) {
		struct sf *fn;

		switch(ch) {
		default:
			D("bad option %c %s", ch, optarg);
			usage();
			break;

		case 'n':
			g.npackets = atoi(optarg);
			break;

		case 'f':
			for (fn = func; fn->key; fn++) {
				if (!strcmp(fn->key, optarg))
					break;
			}
			if (fn->key)
				g.td_body = fn->f;
			else
				D("unrecognised function %s", optarg);
			break;

		case 'o':	/* data generation options */
			g.options = atoi(optarg);
			break;

		case 'a':       /* force affinity */
			g.affinity = atoi(optarg);
			break;

		case 'i':	/* interface */
			g.ifname = optarg;
			if (!strncmp(optarg, "tap", 3))
				g.dev_type = DEV_TAP;
			else
				g.dev_type = DEV_NETMAP;
			break;

		case 'I':
			g.options |= OPT_INDIRECT;	/* XXX use indirect buffer */
			break;

		case 't':	/* send, deprecated */
			D("-t deprecated, please use -f tx -n %s", optarg);
			g.td_body = sender_body;
			g.npackets = atoi(optarg);
			break;

		case 'r':	/* receive */
			D("-r deprecated, please use -f rx -n %s", optarg);
			g.td_body = receiver_body;
			g.npackets = atoi(optarg);
			break;

		case 'l':	/* pkt_size */
			g.pkt_size = atoi(optarg);
			break;

		case 'd':
			g.dst_ip.name = optarg;
			break;

		case 's':
			g.src_ip.name = optarg;
			break;

		case 'T':	/* report interval */
			g.report_interval = atoi(optarg);
			break;

		case 'w':
			wait_link = atoi(optarg);
			break;

		case 'W': /* XXX changed default */
			g.forever = 0; /* do not exit rx even with no traffic */
			break;

		case 'b':	/* burst */
			g.burst = atoi(optarg);
			break;
		case 'c':
			g.cpus = atoi(optarg);
			break;
		case 'p':
			g.nthreads = atoi(optarg);
			break;

		case 'P':
			g.dev_type = DEV_PCAP;
			break;

		case 'D': /* destination mac */
			g.dst_mac.name = optarg;
			break;

		case 'S': /* source mac */
			g.src_mac.name = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'R':
			g.tx_rate = atoi(optarg);
			break;
		case 'X':
			g.options |= OPT_DUMP;
		}
	}

	if (g.ifname == NULL) {
		D("missing ifname");
		usage();
	}

	i = system_ncpus();
	if (g.cpus < 0 || g.cpus > i) {
		D("%d cpus is too high, have only %d cpus", g.cpus, i);
		usage();
	}
	if (g.cpus == 0)
		g.cpus = i;

	if (g.pkt_size < 16 || g.pkt_size > 1536) {
		D("bad pktsize %d\n", g.pkt_size);
		usage();
	}

	if (g.src_mac.name == NULL) {
		static char mybuf[20] = "00:00:00:00:00:00";
		/* retrieve source mac address. */
		if (source_hwaddr(g.ifname, mybuf) == -1) {
			D("Unable to retrieve source mac");
			// continue, fail later
		}
		g.src_mac.name = mybuf;
	}
	/* extract address ranges */
	extract_ip_range(&g.src_ip);
	extract_ip_range(&g.dst_ip);
	extract_mac_range(&g.src_mac);
	extract_mac_range(&g.dst_mac);

    if (g.dev_type == DEV_TAP) {
	D("want to use tap %s", g.ifname);
	g.main_fd = tap_alloc(g.ifname);
	if (g.main_fd < 0) {
		D("cannot open tap %s", g.ifname);
		usage();
	}
    } else if (g.dev_type > DEV_NETMAP) {
	char pcap_errbuf[PCAP_ERRBUF_SIZE];

	D("using pcap on %s", g.ifname);
	pcap_errbuf[0] = '\0'; // init the buffer
	g.p = pcap_open_live(g.ifname, 0, 1, 100, pcap_errbuf);
	if (g.p == NULL) {
		D("cannot open pcap on %s", g.ifname);
		usage();
	}
    } else {
	bzero(&nmr, sizeof(nmr));
	nmr.nr_version = NETMAP_API;
	/*
	 * Open the netmap device to fetch the number of queues of our
	 * interface.
	 *
	 * The first NIOCREGIF also detaches the card from the
	 * protocol stack and may cause a reset of the card,
	 * which in turn may take some time for the PHY to
	 * reconfigure.
	 */
	g.main_fd = open("/dev/netmap", O_RDWR);
	if (g.main_fd == -1) {
		D("Unable to open /dev/netmap");
		// fail later
	} else {
		if ((ioctl(g.main_fd, NIOCGINFO, &nmr)) == -1) {
			D("Unable to get if info without name");
		} else {
			D("map size is %d Kb", nmr.nr_memsize >> 10);
		}
		bzero(&nmr, sizeof(nmr));
		nmr.nr_version = NETMAP_API;
		strncpy(nmr.nr_name, g.ifname, sizeof(nmr.nr_name));
		if ((ioctl(g.main_fd, NIOCGINFO, &nmr)) == -1) {
			D("Unable to get if info for %s", g.ifname);
		}
		devqueues = nmr.nr_rx_rings;
	}

	/* validate provided nthreads. */
	if (g.nthreads < 1 || g.nthreads > devqueues) {
		D("bad nthreads %d, have %d queues", g.nthreads, devqueues);
		// continue, fail later
	}

	/*
	 * Map the netmap shared memory: instead of issuing mmap()
	 * inside the body of the threads, we prefer to keep this
	 * operation here to simplify the thread logic.
	 */
	D("mapping %d Kbytes", nmr.nr_memsize>>10);
	g.mmap_size = nmr.nr_memsize;
	g.mmap_addr = (struct netmap_d *) mmap(0, nmr.nr_memsize,
					    PROT_WRITE | PROT_READ,
					    MAP_SHARED, g.main_fd, 0);
	if (g.mmap_addr == MAP_FAILED) {
		D("Unable to mmap %d KB", nmr.nr_memsize >> 10);
		// continue, fail later
	}

	/*
	 * Register the interface on the netmap device: from now on,
	 * we can operate on the network interface without any
	 * interference from the legacy network stack.
	 *
	 * We decide to put the first interface registration here to
	 * give time to cards that take a long time to reset the PHY.
	 */
	nmr.nr_version = NETMAP_API;
	if (ioctl(g.main_fd, NIOCREGIF, &nmr) == -1) {
		D("Unable to register interface %s", g.ifname);
		//continue, fail later
	}


	/* Print some debug information. */
	fprintf(stdout,
		"%s %s: %d queues, %d threads and %d cpus.\n",
		(g.td_body == sender_body) ? "Sending on" : "Receiving from",
		g.ifname,
		devqueues,
		g.nthreads,
		g.cpus);
	if (g.td_body == sender_body) {
		fprintf(stdout, "%s -> %s (%s -> %s)\n",
			g.src_ip.name, g.dst_ip.name,
			g.src_mac.name, g.dst_mac.name);
	}
			
	/* Exit if something went wrong. */
	if (g.main_fd < 0) {
		D("aborting");
		usage();
	}
    }

	if (g.options) {
		D("--- SPECIAL OPTIONS:%s%s%s%s%s\n",
			g.options & OPT_PREFETCH ? " prefetch" : "",
			g.options & OPT_ACCESS ? " access" : "",
			g.options & OPT_MEMCPY ? " memcpy" : "",
			g.options & OPT_INDIRECT ? " indirect" : "",
			g.options & OPT_COPY ? " copy" : "");
	}
	
	if (g.tx_rate == 0) {
		g.tx_period.tv_sec = 0;
		g.tx_period.tv_nsec = 0;
	} else if (g.tx_rate == 1) {
		g.tx_period.tv_sec = 1;
		g.tx_period.tv_nsec = 0;
	} else {
		g.tx_period.tv_sec = 0;
		g.tx_period.tv_nsec = (1e9 / g.tx_rate) * g.burst;
		if (g.tx_period.tv_nsec > 1000000000) {
			g.tx_period.tv_sec = g.tx_period.tv_nsec / 1000000000;
			g.tx_period.tv_nsec = g.tx_period.tv_nsec % 1000000000;
		}
	}
	D("Sending %d packets every  %d.%09d ns",
			g.burst, (int)g.tx_period.tv_sec, (int)g.tx_period.tv_nsec);
	/* Wait for PHY reset. */
	D("Wait %d secs for phy reset", wait_link);
	sleep(wait_link);
	D("Ready...");

	/* Install ^C handler. */
	global_nthreads = g.nthreads;
	signal(SIGINT, sigint_h);

#if 0 // XXX this is not needed, i believe
	if (g.dev_type > DEV_NETMAP) {
		g.p = pcap_open_live(g.ifname, 0, 1, 100, NULL);
		if (g.p == NULL) {
			D("cannot open pcap on %s", g.ifname);
			usage();
		} else
			D("using pcap %p on %s", g.p, g.ifname);
	}
#endif // XXX
	start_threads(&g);
	main_thread(&g);
	return 0;
}

/* end of file */
