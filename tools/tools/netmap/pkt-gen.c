/*
 * Copyright (C) 2011-2014 Matteo Landi, Luigi Rizzo. All rights reserved.
 * Copyright (C) 2013-2014 Universita` di Pisa. All rights reserved.
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
 * $Id: pkt-gen.c 12346 2013-06-12 17:36:25Z luigi $
 *
 * Example program to show how to build a multithreaded packet
 * source/sink using the netmap device.
 *
 * In this example we create a programmable number of threads
 * to take care of all the queues of the interface used to
 * send or receive traffic.
 *
 */

// #define TRASH_VHOST_HDR

#define _GNU_SOURCE	/* for CPU_SET() */
#include <stdio.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>


#include <ctype.h>	// isprint()
#include <unistd.h>	// sysconf()
#include <sys/poll.h>
#include <arpa/inet.h>	/* ntohs */
#include <sys/sysctl.h>	/* sysctl */
#include <ifaddrs.h>	/* getifaddrs */
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <pthread.h>

#ifndef NO_PCAP
#include <pcap/pcap.h>
#endif

#ifdef linux

#define cpuset_t        cpu_set_t

#define ifr_flagshigh  ifr_flags        /* only the low 16 bits here */
#define IFF_PPROMISC   IFF_PROMISC      /* IFF_PPROMISC does not exist */
#include <linux/ethtool.h>
#include <linux/sockios.h>

#define CLOCK_REALTIME_PRECISE CLOCK_REALTIME
#include <netinet/ether.h>      /* ether_aton */
#include <linux/if_packet.h>    /* sockaddr_ll */
#endif  /* linux */

#ifdef __FreeBSD__
#include <sys/endian.h> /* le64toh */
#include <machine/param.h>

#include <pthread_np.h> /* pthread w/ affinity */
#include <sys/cpuset.h> /* cpu_set */
#include <net/if_dl.h>  /* LLADDR */
#endif  /* __FreeBSD__ */

#ifdef __APPLE__

#define cpuset_t        uint64_t        // XXX
static inline void CPU_ZERO(cpuset_t *p)
{
        *p = 0;
}

static inline void CPU_SET(uint32_t i, cpuset_t *p)
{
        *p |= 1<< (i & 0x3f);
}

#define pthread_setaffinity_np(a, b, c) ((void)a, 0)

#define ifr_flagshigh  ifr_flags        // XXX
#define IFF_PPROMISC   IFF_PROMISC
#include <net/if_dl.h>  /* LLADDR */
#define clock_gettime(a,b)      \
        do {struct timespec t0 = {0,0}; *(b) = t0; } while (0)
#endif  /* __APPLE__ */

const char *default_payload="netmap pkt-gen DIRECT payload\n"
	"http://info.iet.unipi.it/~luigi/netmap/ ";

const char *indirect_payload="netmap pkt-gen indirect payload\n"
	"http://info.iet.unipi.it/~luigi/netmap/ ";

int verbose = 0;

#define SKIP_PAYLOAD 1 /* do not check payload. XXX unused */


#define VIRT_HDR_1	10	/* length of a base vnet-hdr */
#define VIRT_HDR_2	12	/* length of the extenede vnet-hdr */
#define VIRT_HDR_MAX	VIRT_HDR_2
struct virt_header {
	uint8_t fields[VIRT_HDR_MAX];
};

#define MAX_BODYSIZE	16384

struct pkt {
	struct virt_header vh;
	struct ether_header eh;
	struct ip ip;
	struct udphdr udp;
	uint8_t body[MAX_BODYSIZE];	// XXX hardwired
} __attribute__((__packed__));

struct ip_range {
	char *name;
	uint32_t start, end; /* same as struct in_addr */
	uint16_t port0, port1;
};

struct mac_range {
	char *name;
	struct ether_addr start, end;
};

/* ifname can be netmap:foo-xxxx */
#define MAX_IFNAMELEN	64	/* our buffer for ifname */
//#define MAX_PKTSIZE	1536
#define MAX_PKTSIZE	MAX_BODYSIZE	/* XXX: + IP_HDR + ETH_HDR */

/* compact timestamp to fit into 60 byte packet. (enough to obtain RTT) */
struct tstamp {
	uint32_t sec;
	uint32_t nsec;
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
	int frags;	/* fragments per packet */
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
#define OPT_MONITOR_TX  128
#define OPT_MONITOR_RX  256
#define OPT_RANDOM_SRC  512
#define OPT_RANDOM_DST  1024
	int dev_type;
#ifndef NO_PCAP
	pcap_t *p;
#endif

	int tx_rate;
	struct timespec tx_period;

	int affinity;
	int main_fd;
	struct nm_desc *nmd;
	int report_interval;		/* milliseconds between prints */
	void *(*td_body)(void *);
	void *mmap_addr;
	char ifname[MAX_IFNAMELEN];
	char *nmr_config;
	int dummy_send;
	int virt_header;	/* send also the virt_header */
	int extra_bufs;		/* goes in nr_arg3 */
	char *packet_file;	/* -P option */
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
	struct nm_desc *nmd;
	volatile uint64_t count;
	struct timespec tic, toc;
	int me;
	pthread_t thread;
	int affinity;

	struct pkt pkt;
	void *frame;
};


/*
 * extract the extremes from a range of ipv4 addresses.
 * addr_lo[-addr_hi][:port_lo[-port_hi]]
 */
static void
extract_ip_range(struct ip_range *r)
{
	char *ap, *pp;
	struct in_addr a;

	if (verbose)
		D("extract IP range from %s", r->name);
	r->port0 = r->port1 = 0;
	r->start = r->end = 0;

	/* the first - splits start/end of range */
	ap = index(r->name, '-');	/* do we have ports ? */
	if (ap) {
		*ap++ = '\0';
	}
	/* grab the initial values (mandatory) */
	pp = index(r->name, ':');
	if (pp) {
		*pp++ = '\0';
		r->port0 = r->port1 = strtol(pp, NULL, 0);
	};
	inet_aton(r->name, &a);
	r->start = r->end = ntohl(a.s_addr);
	if (ap) {
		pp = index(ap, ':');
		if (pp) {
			*pp++ = '\0';
			if (*pp)
				r->port1 = strtol(pp, NULL, 0);
		}
		if (*ap) {
			inet_aton(ap, &a);
			r->end = ntohl(a.s_addr);
		}
	}
	if (r->port0 > r->port1) {
		uint16_t tmp = r->port0;
		r->port0 = r->port1;
		r->port1 = tmp;
	}
	if (r->start > r->end) {
		uint32_t tmp = r->start;
		r->start = r->end;
		r->end = tmp;
	}
	{
		struct in_addr a;
		char buf1[16]; // one ip address

		a.s_addr = htonl(r->end);
		strncpy(buf1, inet_ntoa(a), sizeof(buf1));
		a.s_addr = htonl(r->start);
		if (1)
		    D("range is %s:%d to %s:%d",
			inet_ntoa(a), r->port0, buf1, r->port1);
	}
}

static void
extract_mac_range(struct mac_range *r)
{
	if (verbose)
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
	if (verbose)
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
	D("received control-C on thread %p", pthread_self());
	for (i = 0; i < global_nthreads; i++) {
		targs[i].cancel = 1;
	}
	signal(SIGINT, SIG_DFL);
}

/* sysctl wrapper to return the number of active CPUs */
static int
system_ncpus(void)
{
	int ncpus;
#if defined (__FreeBSD__)
	int mib[2] = { CTL_HW, HW_NCPU };
	size_t len = sizeof(mib);
	sysctl(mib, 2, &ncpus, &len, NULL, 0);
#elif defined(linux)
	ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#else /* others */
	ncpus = 1;
#endif /* others */
	return (ncpus);
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
 * parse the vale configuration in conf and put it in nmr.
 * Return the flag set if necessary.
 * The configuration may consist of 0 to 4 numbers separated
 * by commas: #tx-slots,#rx-slots,#tx-rings,#rx-rings.
 * Missing numbers or zeroes stand for default values.
 * As an additional convenience, if exactly one number
 * is specified, then this is assigned to both #tx-slots and #rx-slots.
 * If there is no 4th number, then the 3rd is assigned to both #tx-rings
 * and #rx-rings.
 */
int
parse_nmr_config(const char* conf, struct nmreq *nmr)
{
	char *w, *tok;
	int i, v;

	nmr->nr_tx_rings = nmr->nr_rx_rings = 0;
	nmr->nr_tx_slots = nmr->nr_rx_slots = 0;
	if (conf == NULL || ! *conf)
		return 0;
	w = strdup(conf);
	for (i = 0, tok = strtok(w, ","); tok; i++, tok = strtok(NULL, ",")) {
		v = atoi(tok);
		switch (i) {
		case 0:
			nmr->nr_tx_slots = nmr->nr_rx_slots = v;
			break;
		case 1:
			nmr->nr_rx_slots = v;
			break;
		case 2:
			nmr->nr_tx_rings = nmr->nr_rx_rings = v;
			break;
		case 3:
			nmr->nr_rx_rings = v;
			break;
		default:
			D("ignored config: %s", tok);
			break;
		}
	}
	D("txr %d txd %d rxr %d rxd %d",
			nmr->nr_tx_rings, nmr->nr_tx_slots,
			nmr->nr_rx_rings, nmr->nr_rx_slots);
	free(w);
	return (nmr->nr_tx_rings || nmr->nr_tx_slots ||
                        nmr->nr_rx_rings || nmr->nr_rx_slots) ?
		NM_OPEN_RING_CFG : 0;
}


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
	cpuset_t cpumask;

	if (i == -1)
		return 0;

	/* Set thread affinity affinity.*/
	CPU_ZERO(&cpumask);
	CPU_SET(i, &cpumask);

	if (pthread_setaffinity_np(me, sizeof(cpuset_t), &cpumask) != 0) {
		D("Unable to set affinity: %s", strerror(errno));
		return 1;
	}
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

	printf("ring %p cur %5d [buf %6d flags 0x%04x len %5d]\n",
		ring, cur, ring->slot[cur].buf_idx,
		ring->slot[cur].flags, len);
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

/*
 * increment the addressed in the packet,
 * starting from the least significant field.
 *	DST_IP DST_PORT SRC_IP SRC_PORT
 */
static void
update_addresses(struct pkt *pkt, struct glob_arg *g)
{
	uint32_t a;
	uint16_t p;
	struct ip *ip = &pkt->ip;
	struct udphdr *udp = &pkt->udp;

    do {
    	/* XXX for now it doesn't handle non-random src, random dst */
	if (g->options & OPT_RANDOM_SRC) {
		udp->uh_sport = random();
		ip->ip_src.s_addr = random();
	} else {
		p = ntohs(udp->uh_sport);
		if (p < g->src_ip.port1) { /* just inc, no wrap */
			udp->uh_sport = htons(p + 1);
			break;
		}
		udp->uh_sport = htons(g->src_ip.port0);

		a = ntohl(ip->ip_src.s_addr);
		if (a < g->src_ip.end) { /* just inc, no wrap */
			ip->ip_src.s_addr = htonl(a + 1);
			break;
		}
		ip->ip_src.s_addr = htonl(g->src_ip.start);

		udp->uh_sport = htons(g->src_ip.port0);
	}

	if (g->options & OPT_RANDOM_DST) {
		udp->uh_dport = random();
		ip->ip_dst.s_addr = random();
	} else {
		p = ntohs(udp->uh_dport);
		if (p < g->dst_ip.port1) { /* just inc, no wrap */
			udp->uh_dport = htons(p + 1);
			break;
		}
		udp->uh_dport = htons(g->dst_ip.port0);

		a = ntohl(ip->ip_dst.s_addr);
		if (a < g->dst_ip.end) { /* just inc, no wrap */
			ip->ip_dst.s_addr = htonl(a + 1);
			break;
		}
	}
	ip->ip_dst.s_addr = htonl(g->dst_ip.start);
    } while (0);
    // update checksum
}

/*
 * initialize one packet and prepare for the next one.
 * The copy could be done better instead of repeating it each time.
 */
static void
initialize_packet(struct targ *targ)
{
	struct pkt *pkt = &targ->pkt;
	struct ether_header *eh;
	struct ip *ip;
	struct udphdr *udp;
	uint16_t paylen = targ->g->pkt_size - sizeof(*eh) - sizeof(struct ip);
	const char *payload = targ->g->options & OPT_INDIRECT ?
		indirect_payload : default_payload;
	int i, l0 = strlen(payload);

	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t *file;
	struct pcap_pkthdr *header;
	const unsigned char *packet;
	
	/* Read a packet from a PCAP file if asked. */
	if (targ->g->packet_file != NULL) {
		if ((file = pcap_open_offline(targ->g->packet_file,
			    errbuf)) == NULL)
			D("failed to open pcap file %s",
			    targ->g->packet_file);
		if (pcap_next_ex(file, &header, &packet) < 0)
			D("failed to read packet from %s",
			    targ->g->packet_file);
		if ((targ->frame = malloc(header->caplen)) == NULL)
			D("out of memory");
		bcopy(packet, (unsigned char *)targ->frame, header->caplen);
		targ->g->pkt_size = header->caplen;
		pcap_close(file);
		return;
	}

	/* create a nice NUL-terminated string */
	for (i = 0; i < paylen; i += l0) {
		if (l0 > paylen - i)
			l0 = paylen - i; // last round
		bcopy(payload, pkt->body + i, l0);
	}
	pkt->body[i-1] = '\0';
	ip = &pkt->ip;

	/* prepare the headers */
        ip->ip_v = IPVERSION;
        ip->ip_hl = 5;
        ip->ip_id = 0;
        ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_len = ntohs(targ->g->pkt_size - sizeof(*eh));
        ip->ip_id = 0;
        ip->ip_off = htons(IP_DF); /* Don't fragment */
        ip->ip_ttl = IPDEFTTL;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_dst.s_addr = htonl(targ->g->dst_ip.start);
	ip->ip_src.s_addr = htonl(targ->g->src_ip.start);
	ip->ip_sum = wrapsum(checksum(ip, sizeof(*ip), 0));


	udp = &pkt->udp;
        udp->uh_sport = htons(targ->g->src_ip.port0);
        udp->uh_dport = htons(targ->g->dst_ip.port0);
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

	bzero(&pkt->vh, sizeof(pkt->vh));
#ifdef TRASH_VHOST_HDR
	/* set bogus content */
	pkt->vh.fields[0] = 0xff;
	pkt->vh.fields[1] = 0xff;
	pkt->vh.fields[2] = 0xff;
	pkt->vh.fields[3] = 0xff;
	pkt->vh.fields[4] = 0xff;
	pkt->vh.fields[5] = 0xff;
#endif /* TRASH_VHOST_HDR */
	// dump_payload((void *)pkt, targ->g->pkt_size, NULL, 0);
}

static void
set_vnet_hdr_len(struct targ *t)
{
	int err, l = t->g->virt_header;
	struct nmreq req;

	if (l == 0)
		return;

	memset(&req, 0, sizeof(req));
	bcopy(t->nmd->req.nr_name, req.nr_name, sizeof(req.nr_name));
	req.nr_version = NETMAP_API;
	req.nr_cmd = NETMAP_BDG_VNET_HDR;
	req.nr_arg1 = l;
	err = ioctl(t->fd, NIOCREGIF, &req);
	if (err) {
		D("Unable to set vnet header length %d", l);
	}
}


/*
 * create and enqueue a batch of packets on a ring.
 * On the last one set NS_REPORT to tell the driver to generate
 * an interrupt when done.
 */
static int
send_packets(struct netmap_ring *ring, struct pkt *pkt, void *frame,
		int size, struct glob_arg *g, u_int count, int options,
		u_int nfrags)
{
	u_int n, sent, cur = ring->cur;
	u_int fcnt;

	n = nm_ring_space(ring);
	if (n < count)
		count = n;
	if (count < nfrags) {
		D("truncating packet, no room for frags %d %d",
				count, nfrags);
	}
#if 0
	if (options & (OPT_COPY | OPT_PREFETCH) ) {
		for (sent = 0; sent < count; sent++) {
			struct netmap_slot *slot = &ring->slot[cur];
			char *p = NETMAP_BUF(ring, slot->buf_idx);

			__builtin_prefetch(p);
			cur = nm_ring_next(ring, cur);
		}
		cur = ring->cur;
	}
#endif
	for (fcnt = nfrags, sent = 0; sent < count; sent++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);

		slot->flags = 0;
		if (options & OPT_INDIRECT) {
			slot->flags |= NS_INDIRECT;
			slot->ptr = (uint64_t)frame;
		} else if (options & OPT_COPY) {
			nm_pkt_copy(frame, p, size);
			if (fcnt == nfrags)
				update_addresses(pkt, g);
		} else if (options & OPT_MEMCPY) {
			memcpy(p, frame, size);
			if (fcnt == nfrags)
				update_addresses(pkt, g);
		} else if (options & OPT_PREFETCH) {
			__builtin_prefetch(p);
		}
		if (options & OPT_DUMP)
			dump_payload(p, size, ring, cur);
		slot->len = size;
		if (--fcnt > 0)
			slot->flags |= NS_MOREFRAG;
		else
			fcnt = nfrags;
		if (sent == count - 1) {
			slot->flags &= ~NS_MOREFRAG;
			slot->flags |= NS_REPORT;
		}
		cur = nm_ring_next(ring, cur);
	}
	ring->head = ring->cur = cur;

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
	struct pollfd pfd = { .fd = targ->fd, .events = POLLIN };
	struct netmap_if *nifp = targ->nmd->nifp;
	int i, rx = 0, n = targ->g->npackets;
	void *frame;
	int size;
	uint32_t sent = 0;
	struct timespec ts, now, last_print;
	uint32_t count = 0, min = 1000000000, av = 0;

	frame = &targ->pkt;
	frame += sizeof(targ->pkt.vh) - targ->g->virt_header;
	size = targ->g->pkt_size + targ->g->virt_header;

	if (targ->g->nthreads > 1) {
		D("can only ping with 1 thread");
		return NULL;
	}

	clock_gettime(CLOCK_REALTIME_PRECISE, &last_print);
	now = last_print;
	while (n == 0 || (int)sent < n) {
		struct netmap_ring *ring = NETMAP_TXRING(nifp, 0);
		struct netmap_slot *slot;
		char *p;
	    for (i = 0; i < 1; i++) { /* XXX why the loop for 1 pkt ? */
		slot = &ring->slot[ring->cur];
		slot->len = size;
		p = NETMAP_BUF(ring, slot->buf_idx);

		if (nm_ring_empty(ring)) {
			D("-- ouch, cannot send");
		} else {
			struct tstamp *tp;
			nm_pkt_copy(frame, p, size);
			clock_gettime(CLOCK_REALTIME_PRECISE, &ts);
			bcopy(&sent, p+42, sizeof(sent));
			tp = (struct tstamp *)(p+46);
			tp->sec = (uint32_t)ts.tv_sec;
			tp->nsec = (uint32_t)ts.tv_nsec;
			sent++;
			ring->head = ring->cur = nm_ring_next(ring, ring->cur);
		}
	    }
		/* should use a parameter to decide how often to send */
		if (poll(&pfd, 1, 3000) <= 0) {
			D("poll error/timeout on queue %d: %s", targ->me,
				strerror(errno));
			continue;
		}
		/* see what we got back */
		for (i = targ->nmd->first_tx_ring;
			i <= targ->nmd->last_tx_ring; i++) {
			ring = NETMAP_RXRING(nifp, i);
			while (!nm_ring_empty(ring)) {
				uint32_t seq;
				struct tstamp *tp;
				slot = &ring->slot[ring->cur];
				p = NETMAP_BUF(ring, slot->buf_idx);

				clock_gettime(CLOCK_REALTIME_PRECISE, &now);
				bcopy(p+42, &seq, sizeof(seq));
				tp = (struct tstamp *)(p+46);
				ts.tv_sec = (time_t)tp->sec;
				ts.tv_nsec = (long)tp->nsec;
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
				ring->head = ring->cur = nm_ring_next(ring, ring->cur);
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
	struct pollfd pfd = { .fd = targ->fd, .events = POLLIN };
	struct netmap_if *nifp = targ->nmd->nifp;
	struct netmap_ring *txring, *rxring;
	int i, rx = 0, sent = 0, n = targ->g->npackets;

	if (targ->g->nthreads > 1) {
		D("can only reply ping with 1 thread");
		return NULL;
	}
	D("understood ponger %d but don't know how to do it", n);
	while (n == 0 || sent < n) {
		uint32_t txcur, txavail;
//#define BUSYWAIT
#ifdef BUSYWAIT
		ioctl(pfd.fd, NIOCRXSYNC, NULL);
#else
		if (poll(&pfd, 1, 1000) <= 0) {
			D("poll error/timeout on queue %d: %s", targ->me,
				strerror(errno));
			continue;
		}
#endif
		txring = NETMAP_TXRING(nifp, 0);
		txcur = txring->cur;
		txavail = nm_ring_space(txring);
		/* see what we got back */
		for (i = targ->nmd->first_rx_ring; i <= targ->nmd->last_rx_ring; i++) {
			rxring = NETMAP_RXRING(nifp, i);
			while (!nm_ring_empty(rxring)) {
				uint16_t *spkt, *dpkt;
				uint32_t cur = rxring->cur;
				struct netmap_slot *slot = &rxring->slot[cur];
				char *src, *dst;
				src = NETMAP_BUF(rxring, slot->buf_idx);
				//D("got pkt %p of size %d", src, slot->len);
				rxring->head = rxring->cur = nm_ring_next(rxring, cur);
				rx++;
				if (txavail == 0)
					continue;
				dst = NETMAP_BUF(txring,
				    txring->slot[txcur].buf_idx);
				/* copy... */
				dpkt = (uint16_t *)dst;
				spkt = (uint16_t *)src;
				nm_pkt_copy(src, dst, slot->len);
				dpkt[0] = spkt[3];
				dpkt[1] = spkt[4];
				dpkt[2] = spkt[5];
				dpkt[3] = spkt[0];
				dpkt[4] = spkt[1];
				dpkt[5] = spkt[2];
				txring->slot[txcur].len = slot->len;
				/* XXX swap src dst mac */
				txcur = nm_ring_next(txring, txcur);
				txavail--;
				sent++;
			}
		}
		txring->head = txring->cur = txcur;
		targ->count = sent;
#ifdef BUSYWAIT
		ioctl(pfd.fd, NIOCTXSYNC, NULL);
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


static __inline struct timespec
timespec_add(struct timespec a, struct timespec b)
{
	struct timespec ret = { a.tv_sec + b.tv_sec, a.tv_nsec + b.tv_nsec };
	if (ret.tv_nsec >= 1000000000) {
		ret.tv_sec++;
		ret.tv_nsec -= 1000000000;
	}
	return ret;
}

static __inline struct timespec
timespec_sub(struct timespec a, struct timespec b)
{
	struct timespec ret = { a.tv_sec - b.tv_sec, a.tv_nsec - b.tv_nsec };
	if (ret.tv_nsec < 0) {
		ret.tv_sec--;
		ret.tv_nsec += 1000000000;
	}
	return ret;
}


/*
 * wait until ts, either busy or sleeping if more than 1ms.
 * Return wakeup time.
 */
static struct timespec
wait_time(struct timespec ts)
{
	for (;;) {
		struct timespec w, cur;
		clock_gettime(CLOCK_REALTIME_PRECISE, &cur);
		w = timespec_sub(ts, cur);
		if (w.tv_sec < 0)
			return cur;
		else if (w.tv_sec > 0 || w.tv_nsec > 1000000)
			poll(NULL, 0, 1);
	}
}

static void *
sender_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd pfd = { .fd = targ->fd, .events = POLLOUT };
	struct netmap_if *nifp;
	struct netmap_ring *txring;
	int i, n = targ->g->npackets / targ->g->nthreads;
	int64_t sent = 0;
	int options = targ->g->options | OPT_COPY;
	struct timespec nexttime = { 0, 0}; // XXX silence compiler
	int rate_limit = targ->g->tx_rate;
	struct pkt *pkt = &targ->pkt;
	void *frame;
	int size;

	if (targ->frame == NULL) {
		frame = pkt;
		frame += sizeof(pkt->vh) - targ->g->virt_header;
		size = targ->g->pkt_size + targ->g->virt_header;
	} else {
		frame = targ->frame;
		size = targ->g->pkt_size;
	}
	
	D("start, fd %d main_fd %d", targ->fd, targ->g->main_fd);
	if (setaffinity(targ->thread, targ->affinity))
		goto quit;

	/* main loop.*/
	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->tic);
	if (rate_limit) {
		targ->tic = timespec_add(targ->tic, (struct timespec){2,0});
		targ->tic.tv_nsec = 0;
		wait_time(targ->tic);
		nexttime = targ->tic;
	}
        if (targ->g->dev_type == DEV_TAP) {
	    D("writing to file desc %d", targ->g->main_fd);

	    for (i = 0; !targ->cancel && (n == 0 || sent < n); i++) {
		if (write(targ->g->main_fd, frame, size) != -1)
			sent++;
		update_addresses(pkt, targ->g);
		if (i > 10000) {
			targ->count = sent;
			i = 0;
		}
	    }
#ifndef NO_PCAP
    } else if (targ->g->dev_type == DEV_PCAP) {
	    pcap_t *p = targ->g->p;

	    for (i = 0; !targ->cancel && (n == 0 || sent < n); i++) {
		if (pcap_inject(p, frame, size) != -1)
			sent++;
		update_addresses(pkt, targ->g);
		if (i > 10000) {
			targ->count = sent;
			i = 0;
		}
	    }
#endif /* NO_PCAP */
    } else {
	int tosend = 0;
	int frags = targ->g->frags;

        nifp = targ->nmd->nifp;
	while (!targ->cancel && (n == 0 || sent < n)) {

		if (rate_limit && tosend <= 0) {
			tosend = targ->g->burst;
			nexttime = timespec_add(nexttime, targ->g->tx_period);
			wait_time(nexttime);
		}

		/*
		 * wait for available room in the send queue(s)
		 */
		if (poll(&pfd, 1, 2000) <= 0) {
			if (targ->cancel)
				break;
			D("poll error/timeout on queue %d: %s", targ->me,
				strerror(errno));
			// goto quit;
		}
		if (pfd.revents & POLLERR) {
			D("poll error");
			goto quit;
		}
		/*
		 * scan our queues and send on those with room
		 */
		if (options & OPT_COPY && sent > 100000 && !(targ->g->options & OPT_COPY) ) {
			D("drop copy");
			options &= ~OPT_COPY;
		}
		for (i = targ->nmd->first_tx_ring; i <= targ->nmd->last_tx_ring; i++) {
			int m, limit = rate_limit ?  tosend : targ->g->burst;
			if (n > 0 && n - sent < limit)
				limit = n - sent;
			txring = NETMAP_TXRING(nifp, i);
			if (nm_ring_empty(txring))
				continue;
			if (frags > 1)
				limit = ((limit + frags - 1) / frags) * frags;

			m = send_packets(txring, pkt, frame, size, targ->g,
					 limit, options, frags);
			ND("limit %d tail %d frags %d m %d",
				limit, txring->tail, frags, m);
			sent += m;
			targ->count = sent;
			if (rate_limit) {
				tosend -= m;
				if (tosend <= 0)
					break;
			}
		}
	}
	/* flush any remaining packets */
	D("flush tail %d head %d on thread %p",
		txring->tail, txring->head,
		pthread_self());
	ioctl(pfd.fd, NIOCTXSYNC, NULL);

	/* final part: wait all the TX queues to be empty. */
	for (i = targ->nmd->first_tx_ring; i <= targ->nmd->last_tx_ring; i++) {
		txring = NETMAP_TXRING(nifp, i);
		while (nm_tx_pending(txring)) {
			RD(5, "pending tx tail %d head %d on ring %d",
				txring->tail, txring->head, i);
			ioctl(pfd.fd, NIOCTXSYNC, NULL);
			usleep(1); /* wait 1 tick */
		}
	}
    } /* end DEV_NETMAP */

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);
	targ->completed = 1;
	targ->count = sent;

quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}


#ifndef NO_PCAP
static void
receive_pcap(u_char *user, const struct pcap_pkthdr * h,
	const u_char * bytes)
{
	int *count = (int *)user;
	(void)h;	/* UNUSED */
	(void)bytes;	/* UNUSED */
	(*count)++;
}
#endif /* !NO_PCAP */

static int
receive_packets(struct netmap_ring *ring, u_int limit, int dump)
{
	u_int cur, rx, n;

	cur = ring->cur;
	n = nm_ring_space(ring);
	if (n < limit)
		limit = n;
	for (rx = 0; rx < limit; rx++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);

		if (dump)
			dump_payload(p, slot->len, ring, cur);

		cur = nm_ring_next(ring, cur);
	}
	ring->head = ring->cur = cur;

	return (rx);
}

static void *
receiver_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd pfd = { .fd = targ->fd, .events = POLLIN };
	struct netmap_if *nifp;
	struct netmap_ring *rxring;
	int i;
	uint64_t received = 0;

	if (setaffinity(targ->thread, targ->affinity))
		goto quit;

	D("reading from %s fd %d main_fd %d",
		targ->g->ifname, targ->fd, targ->g->main_fd);
	/* unbounded wait for the first packet. */
	for (;!targ->cancel;) {
		i = poll(&pfd, 1, 1000);
		if (i > 0 && !(pfd.revents & POLLERR))
			break;
		RD(1, "waiting for initial packets, poll returns %d %d",
			i, pfd.revents);
	}
	/* main loop, exit after 1s silence */
	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->tic);
    if (targ->g->dev_type == DEV_TAP) {
	while (!targ->cancel) {
		char buf[MAX_BODYSIZE];
		/* XXX should we poll ? */
		if (read(targ->g->main_fd, buf, sizeof(buf)) > 0)
			targ->count++;
	}
#ifndef NO_PCAP
    } else if (targ->g->dev_type == DEV_PCAP) {
	while (!targ->cancel) {
		/* XXX should we poll ? */
		pcap_dispatch(targ->g->p, targ->g->burst, receive_pcap,
			(u_char *)&targ->count);
	}
#endif /* !NO_PCAP */
    } else {
	int dump = targ->g->options & OPT_DUMP;

        nifp = targ->nmd->nifp;
	while (!targ->cancel) {
		/* Once we started to receive packets, wait at most 1 seconds
		   before quitting. */
		if (poll(&pfd, 1, 1 * 1000) <= 0 && !targ->g->forever) {
			clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);
			targ->toc.tv_sec -= 1; /* Subtract timeout time. */
			goto out;
		}

		if (pfd.revents & POLLERR) {
			D("poll err");
			goto quit;
		}

		for (i = targ->nmd->first_rx_ring; i <= targ->nmd->last_rx_ring; i++) {
			int m;

			rxring = NETMAP_RXRING(nifp, i);
			if (nm_ring_empty(rxring))
				continue;

			m = receive_packets(rxring, targ->g->burst, dump);
			received += m;
		}
		targ->count = received;
	}
    }

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);

out:
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
	char *units[] = { "", "K", "M", "G", "T" };
	u_int i;

	for (i = 0; val >=1000 && i < sizeof(units)/sizeof(char *) - 1; i++)
		val /= 1000;
	sprintf(buf, "%.2f %s", val, units[i]);
	return buf;
}

static void
tx_output(uint64_t sent, int size, double delta)
{
	double bw, raw_bw, pps;
	char b1[40], b2[80], b3[80];

	printf("Sent %llu packets, %d bytes each, in %.2f seconds.\n",
	       (unsigned long long)sent, size, delta);
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

	printf("Received %llu packets, in %.2f seconds.\n",
		(unsigned long long) received, delta);

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
		"\t-l pkt_size		in bytes excluding CRC\n"
		"\t-d dst_ip[:port[-dst_ip:port]]   single or range\n"
		"\t-s src_ip[:port[-src_ip:port]]   single or range\n"
		"\t-D dst-mac\n"
		"\t-S src-mac\n"
		"\t-a cpu_id		use setaffinity\n"
		"\t-b burst size		testing, mostly\n"
		"\t-c cores		cores to use\n"
		"\t-p threads		processes/threads to use\n"
		"\t-T report_ms		milliseconds between reports\n"
		"\t-P			use libpcap instead of netmap\n"
		"\t-w wait_for_link_time	in seconds\n"
		"\t-R rate		in packets per second\n"
		"\t-X			dump payload\n"
		"\t-H len		add empty virtio-net-header with size 'len'\n"
	        "\t-P file		load packet from pcap file\n"
		"\t-z			use random IPv4 src address/port\n"
		"\t-Z			use random IPv4 dst address/port\n"
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
		struct targ *t = &targs[i];

		bzero(t, sizeof(*t));
		t->fd = -1; /* default, with pcap */
		t->g = g;

	    if (g->dev_type == DEV_NETMAP) {
		struct nm_desc nmd = *g->nmd; /* copy, we overwrite ringid */
		uint64_t nmd_flags = 0;
		nmd.self = &nmd;

		if (g->nthreads > 1) {
			if (nmd.req.nr_flags != NR_REG_ALL_NIC) {
				D("invalid nthreads mode %d", nmd.req.nr_flags);
				continue;
			}
			nmd.req.nr_flags = NR_REG_ONE_NIC;
			nmd.req.nr_ringid = i;
		}
		/* Only touch one of the rings (rx is already ok) */
		if (g->td_body == receiver_body)
			nmd_flags |= NETMAP_NO_TX_POLL;

		/* register interface. Override ifname and ringid etc. */
		if (g->options & OPT_MONITOR_TX)
			nmd.req.nr_flags |= NR_MONITOR_TX;
		if (g->options & OPT_MONITOR_RX)
			nmd.req.nr_flags |= NR_MONITOR_RX;

		t->nmd = nm_open(t->g->ifname, NULL, nmd_flags |
			NM_OPEN_IFNAME | NM_OPEN_NO_MMAP, &nmd);
		if (t->nmd == NULL) {
			D("Unable to open %s: %s",
				t->g->ifname, strerror(errno));
			continue;
		}
		t->fd = t->nmd->fd;
		set_vnet_hdr_len(t);

	    } else {
		targs[i].fd = g->main_fd;
	    }
		t->used = 1;
		t->me = i;
		if (g->affinity >= 0) {
			if (g->affinity < g->cpus)
				t->affinity = g->affinity;
			else
				t->affinity = i % g->cpus;
		} else {
			t->affinity = -1;
		}
		/* default, init packets */
		initialize_packet(t);

		if (pthread_create(&t->thread, NULL, g->td_body, t) == -1) {
			D("Unable to create thread %d: %s", i, strerror(errno));
			t->used = 0;
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
		D("%llu pps (%llu pkts in %llu usec)",
			(unsigned long long)pps,
			(unsigned long long)npkts,
			(unsigned long long)usec);
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
		munmap(g->nmd->mem, g->nmd->req.nr_memsize);
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
		D("failed to to a TUNSETIFF: %s", strerror(errno));
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
	g.frags = 1;
	g.nmr_config = "";
	g.virt_header = 0;

	while ( (ch = getopt(arc, argv,
			"a:f:F:n:i:Il:d:s:D:S:b:c:o:p:T:w:WvR:XC:H:e:m:P:zZ")) != -1) {
		struct sf *fn;

		switch(ch) {
		default:
			D("bad option %c %s", ch, optarg);
			usage();
			break;

		case 'n':
			g.npackets = atoi(optarg);
			break;

		case 'F':
			i = atoi(optarg);
			if (i < 1 || i > 63) {
				D("invalid frags %d [1..63], ignore", i);
				break;
			}
			g.frags = i;
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
			/* a prefix of tap: netmap: or pcap: forces the mode.
			 * otherwise we guess
			 */
			D("interface is %s", optarg);
			if (strlen(optarg) > MAX_IFNAMELEN - 8) {
				D("ifname too long %s", optarg);
				break;
			}
			strcpy(g.ifname, optarg);
			if (!strcmp(optarg, "null")) {
				g.dev_type = DEV_NETMAP;
				g.dummy_send = 1;
			} else if (!strncmp(optarg, "tap:", 4)) {
				g.dev_type = DEV_TAP;
				strcpy(g.ifname, optarg + 4);
			} else if (!strncmp(optarg, "pcap:", 5)) {
				g.dev_type = DEV_PCAP;
				strcpy(g.ifname, optarg + 5);
			} else if (!strncmp(optarg, "netmap:", 7) ||
				   !strncmp(optarg, "vale", 4)) {
				g.dev_type = DEV_NETMAP;
			} else if (!strncmp(optarg, "tap", 3)) {
				g.dev_type = DEV_TAP;
			} else { /* prepend netmap: */
				g.dev_type = DEV_NETMAP;
				sprintf(g.ifname, "netmap:%s", optarg);
			}
			break;

		case 'I':
			g.options |= OPT_INDIRECT;	/* XXX use indirect buffer */
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
			break;
		case 'C':
			g.nmr_config = strdup(optarg);
			break;
		case 'H':
			g.virt_header = atoi(optarg);
			break;
		case 'e': /* extra bufs */
			g.extra_bufs = atoi(optarg);
			break;
		case 'm':
			if (strcmp(optarg, "tx") == 0) {
				g.options |= OPT_MONITOR_TX;
			} else if (strcmp(optarg, "rx") == 0) {
				g.options |= OPT_MONITOR_RX;
			} else {
				D("unrecognized monitor mode %s", optarg);
			}
			break;
		case 'P':
			g.packet_file = strdup(optarg);
			break;
		case 'z':
			g.options |= OPT_RANDOM_SRC;
			break;
		case 'Z':
			g.options |= OPT_RANDOM_DST;
			break;
		}
	}

	if (strlen(g.ifname) <=0 ) {
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

	if (g.pkt_size < 16 || g.pkt_size > MAX_PKTSIZE) {
		D("bad pktsize %d [16..%d]\n", g.pkt_size, MAX_PKTSIZE);
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

	if (g.src_ip.start != g.src_ip.end ||
	    g.src_ip.port0 != g.src_ip.port1 ||
	    g.dst_ip.start != g.dst_ip.end ||
	    g.dst_ip.port0 != g.dst_ip.port1)
		g.options |= OPT_COPY;

	if (g.virt_header != 0 && g.virt_header != VIRT_HDR_1
			&& g.virt_header != VIRT_HDR_2) {
		D("bad virtio-net-header length");
		usage();
	}

    if (g.dev_type == DEV_TAP) {
	D("want to use tap %s", g.ifname);
	g.main_fd = tap_alloc(g.ifname);
	if (g.main_fd < 0) {
		D("cannot open tap %s", g.ifname);
		usage();
	}
#ifndef NO_PCAP
    } else if (g.dev_type == DEV_PCAP) {
	char pcap_errbuf[PCAP_ERRBUF_SIZE];

	pcap_errbuf[0] = '\0'; // init the buffer
	g.p = pcap_open_live(g.ifname, 256 /* XXX */, 1, 100, pcap_errbuf);
	if (g.p == NULL) {
		D("cannot open pcap on %s", g.ifname);
		usage();
	}
	g.main_fd = pcap_fileno(g.p);
	D("using pcap on %s fileno %d", g.ifname, g.main_fd);
#endif /* !NO_PCAP */
    } else if (g.dummy_send) { /* but DEV_NETMAP */
	D("using a dummy send routine");
    } else {
	struct nmreq base_nmd;

	bzero(&base_nmd, sizeof(base_nmd));

	parse_nmr_config(g.nmr_config, &base_nmd);
	if (g.extra_bufs) {
		base_nmd.nr_arg3 = g.extra_bufs;
	}

	/*
	 * Open the netmap device using nm_open().
	 *
	 * protocol stack and may cause a reset of the card,
	 * which in turn may take some time for the PHY to
	 * reconfigure. We do the open here to have time to reset.
	 */
	g.nmd = nm_open(g.ifname, &base_nmd, 0, NULL);
	if (g.nmd == NULL) {
		D("Unable to open %s: %s", g.ifname, strerror(errno));
		goto out;
	}
	g.main_fd = g.nmd->fd;
	D("mapped %dKB at %p", g.nmd->req.nr_memsize>>10, g.nmd->mem);

	/* get num of queues in tx or rx */ 
	if (g.td_body == sender_body)
		devqueues = g.nmd->req.nr_tx_rings;
	else 
		devqueues = g.nmd->req.nr_rx_rings;

	/* validate provided nthreads. */
	if (g.nthreads < 1 || g.nthreads > devqueues) {
		D("bad nthreads %d, have %d queues", g.nthreads, devqueues);
		// continue, fail later
	}

	if (verbose) {
		struct netmap_if *nifp = g.nmd->nifp;
		struct nmreq *req = &g.nmd->req;

		D("nifp at offset %d, %d tx %d rx region %d",
		    req->nr_offset, req->nr_tx_rings, req->nr_rx_rings,
		    req->nr_arg2);
		for (i = 0; i <= req->nr_tx_rings; i++) {
			struct netmap_ring *ring = NETMAP_TXRING(nifp, i);
			D("   TX%d at 0x%lx slots %d", i,
			    (char *)ring - (char *)nifp, ring->num_slots);
		}
		for (i = 0; i <= req->nr_rx_rings; i++) {
			struct netmap_ring *ring = NETMAP_RXRING(nifp, i);
			D("   RX%d at 0x%lx slots %d", i,
			    (char *)ring - (char *)nifp, ring->num_slots);
		}
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

out:
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

	g.tx_period.tv_sec = g.tx_period.tv_nsec = 0;
	if (g.tx_rate > 0) {
		/* try to have at least something every second,
		 * reducing the burst size to some 0.01s worth of data
		 * (but no less than one full set of fragments)
	 	 */
		uint64_t x;
		int lim = (g.tx_rate)/300;
		if (g.burst > lim)
			g.burst = lim;
		if (g.burst < g.frags)
			g.burst = g.frags;
		x = ((uint64_t)1000000000 * (uint64_t)g.burst) / (uint64_t) g.tx_rate;
		g.tx_period.tv_nsec = x;
		g.tx_period.tv_sec = g.tx_period.tv_nsec / 1000000000;
		g.tx_period.tv_nsec = g.tx_period.tv_nsec % 1000000000;
	}
	if (g.td_body == sender_body)
	    D("Sending %d packets every  %ld.%09ld s",
			g.burst, g.tx_period.tv_sec, g.tx_period.tv_nsec);
	/* Wait for PHY reset. */
	D("Wait %d secs for phy reset", wait_link);
	sleep(wait_link);
	D("Ready...");

	/* Install ^C handler. */
	global_nthreads = g.nthreads;
	signal(SIGINT, sigint_h);

	start_threads(&g);
	main_thread(&g);
	return 0;
}

/* end of file */
