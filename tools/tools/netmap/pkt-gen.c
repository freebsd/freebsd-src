/*
 * Copyright (C) 2011-2014 Matteo Landi, Luigi Rizzo. All rights reserved.
 * Copyright (C) 2013-2015 Universita` di Pisa. All rights reserved.
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

#define _GNU_SOURCE	/* for CPU_SET() */
#include <stdio.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>


#include <ctype.h>	// isprint()
#include <unistd.h>	// sysconf()
#include <sys/poll.h>
#include <arpa/inet.h>	/* ntohs */
#ifndef _WIN32
#include <sys/sysctl.h>	/* sysctl */
#endif
#include <ifaddrs.h>	/* getifaddrs */
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <assert.h>
#include <math.h>

#include <pthread.h>

#ifndef NO_PCAP
#include <pcap/pcap.h>
#endif

#include "ctrs.h"

#ifdef _WIN32
#define cpuset_t        DWORD_PTR   //uint64_t
static inline void CPU_ZERO(cpuset_t *p)
{
        *p = 0;
}

static inline void CPU_SET(uint32_t i, cpuset_t *p)
{
        *p |= 1<< (i & 0x3f);
}

#define pthread_setaffinity_np(a, b, c) !SetThreadAffinityMask(a, *c)    //((void)a, 0)
#define TAP_CLONEDEV	"/dev/tap"
#define AF_LINK	18	//defined in winsocks.h
#define CLOCK_REALTIME_PRECISE CLOCK_REALTIME
#include <net/if_dl.h>

/*
 * Convert an ASCII representation of an ethernet address to
 * binary form.
 */
struct ether_addr *
ether_aton(const char *a)
{
	int i;
	static struct ether_addr o;
	unsigned int o0, o1, o2, o3, o4, o5;

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o0, &o1, &o2, &o3, &o4, &o5);

	if (i != 6)
		return (NULL);

	o.octet[0]=o0;
	o.octet[1]=o1;
	o.octet[2]=o2;
	o.octet[3]=o3;
	o.octet[4]=o4;
	o.octet[5]=o5;

	return ((struct ether_addr *)&o);
}

/*
 * Convert a binary representation of an ethernet address to
 * an ASCII string.
 */
char *
ether_ntoa(const struct ether_addr *n)
{
	int i;
	static char a[18];

	i = sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x",
	    n->octet[0], n->octet[1], n->octet[2],
	    n->octet[3], n->octet[4], n->octet[5]);
	return (i < 17 ? NULL : (char *)&a);
}
#endif /* _WIN32 */

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
	uint64_t npackets;	/* total packets to send */
	int frags;	/* fragments per packet */
	int nthreads;
	int cpus;	/* cpus used for running */
	int system_cpus;	/* cpus on the system */

	int options;	/* testing */
#define OPT_PREFETCH	1
#define OPT_ACCESS	2
#define OPT_COPY	4
#define OPT_MEMCPY	8
#define OPT_TS		16	/* add a timestamp */
#define OPT_INDIRECT	32	/* use indirect buffers, tx only */
#define OPT_DUMP	64	/* dump rx/tx traffic */
#define OPT_RUBBISH	256	/* send wathever the buffers contain */
#define OPT_RANDOM_SRC  512
#define OPT_RANDOM_DST  1024
#define OPT_PPS_STATS   2048
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
	int td_type;
	void *mmap_addr;
	char ifname[MAX_IFNAMELEN];
	char *nmr_config;
	int dummy_send;
	int virt_header;	/* send also the virt_header */
	int extra_bufs;		/* goes in nr_arg3 */
	int extra_pipes;	/* goes in nr_arg1 */
	char *packet_file;	/* -P option */
#define	STATS_WIN	15
	int win_idx;
	int64_t win[STATS_WIN];
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
	/* these ought to be volatile, but they are
	 * only sampled and errors should not accumulate
	 */
	struct my_ctrs ctr;

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
	D("received control-C on thread %p", (void *)pthread_self());
	for (i = 0; i < global_nthreads; i++) {
		targs[i].cancel = 1;
	}
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
#elif defined(_WIN32)
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		ncpus = sysinfo.dwNumberOfProcessors;
	}
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
dump_payload(const char *_p, int len, struct netmap_ring *ring, int cur)
{
	char buf[128];
	int i, j, i0;
	const unsigned char *p = (const unsigned char *)_p;

	/* get the length in ASCII of the length of the packet. */

	printf("ring %p cur %5d [buf %6d flags 0x%04x len %5d]\n",
		ring, cur, ring->slot[cur].buf_idx,
		ring->slot[cur].flags, len);
	/* hexdump routine */
	for (i = 0; i < len; ) {
		memset(buf, ' ', sizeof(buf));
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

#ifndef NO_PCAP
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
#endif

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
	// dump_payload((void *)pkt, targ->g->pkt_size, NULL, 0);
}

static void
get_vnet_hdr_len(struct glob_arg *g)
{
	struct nmreq req;
	int err;

	memset(&req, 0, sizeof(req));
	bcopy(g->nmd->req.nr_name, req.nr_name, sizeof(req.nr_name));
	req.nr_version = NETMAP_API;
	req.nr_cmd = NETMAP_VNET_HDR_GET;
	err = ioctl(g->main_fd, NIOCREGIF, &req);
	if (err) {
		D("Unable to get virtio-net header length");
		return;
	}

	g->virt_header = req.nr_arg1;
	if (g->virt_header) {
		D("Port requires virtio-net header, length = %d",
		  g->virt_header);
	}
}

static void
set_vnet_hdr_len(struct glob_arg *g)
{
	int err, l = g->virt_header;
	struct nmreq req;

	if (l == 0)
		return;

	memset(&req, 0, sizeof(req));
	bcopy(g->nmd->req.nr_name, req.nr_name, sizeof(req.nr_name));
	req.nr_version = NETMAP_API;
	req.nr_cmd = NETMAP_BDG_VNET_HDR;
	req.nr_arg1 = l;
	err = ioctl(g->main_fd, NIOCREGIF, &req);
	if (err) {
		D("Unable to set virtio-net header length %d", l);
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
		int buf_changed = slot->flags & NS_BUF_CHANGED;

		slot->flags = 0;
		if (options & OPT_RUBBISH) {
			/* do nothing */
		} else if (options & OPT_INDIRECT) {
			slot->flags |= NS_INDIRECT;
			slot->ptr = (uint64_t)((uintptr_t)frame);
		} else if ((options & OPT_COPY) || buf_changed) {
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
 * Index of the highest bit set
 */
uint32_t
msb64(uint64_t x)
{
	uint64_t m = 1ULL << 63;
	int i;

	for (i = 63; i >= 0; i--, m >>=1)
		if (m & x)
			return i;
	return 0;
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
	int i, rx = 0;
	void *frame;
	int size;
	struct timespec ts, now, last_print;
	uint64_t sent = 0, n = targ->g->npackets;
	uint64_t count = 0, t_cur, t_min = ~0, av = 0;
	uint64_t buckets[64];	/* bins for delays, ns */

	frame = &targ->pkt;
	frame += sizeof(targ->pkt.vh) - targ->g->virt_header;
	size = targ->g->pkt_size + targ->g->virt_header;


	if (targ->g->nthreads > 1) {
		D("can only ping with 1 thread");
		return NULL;
	}

	bzero(&buckets, sizeof(buckets));
	clock_gettime(CLOCK_REALTIME_PRECISE, &last_print);
	now = last_print;
	while (!targ->cancel && (n == 0 || sent < n)) {
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
				int pos;

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
				if (0) D("seq %d/%lu delta %d.%09d", seq, sent,
					(int)ts.tv_sec, (int)ts.tv_nsec);
				t_cur = ts.tv_sec * 1000000000UL + ts.tv_nsec;
				if (t_cur < t_min)
					t_min = t_cur;
				count ++;
				av += t_cur;
				pos = msb64(t_cur);
				buckets[pos]++;
				/* now store it in a bucket */
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
			D("count %d RTT: min %d av %d ns",
				(int)count, (int)t_min, (int)(av/count));
			int k, j, kmin;
			char buf[512];

			for (kmin = 0; kmin < 64; kmin ++)
				if (buckets[kmin])
					break;
			for (k = 63; k >= kmin; k--)
				if (buckets[k])
					break;
			buf[0] = '\0';
			for (j = kmin; j <= k; j++)
				sprintf(buf, "%s %5d", buf, (int)buckets[j]);
			D("k: %d .. %d\n\t%s", 1<<kmin, 1<<k, buf);
			bzero(&buckets, sizeof(buckets));
			count = 0;
			av = 0;
			t_min = ~0;
			last_print = now;
		}
	}

	/* reset the ``used`` flag. */
	targ->used = 0;

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
	int i, rx = 0;
	uint64_t sent = 0, n = targ->g->npackets;

	if (targ->g->nthreads > 1) {
		D("can only reply ping with 1 thread");
		return NULL;
	}
	D("understood ponger %lu but don't know how to do it", n);
	while (!targ->cancel && (n == 0 || sent < n)) {
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
		targ->ctr.pkts = sent;
#ifdef BUSYWAIT
		ioctl(pfd.fd, NIOCTXSYNC, NULL);
#endif
		//D("tx %d rx %d", sent, rx);
	}

	/* reset the ``used`` flag. */
	targ->used = 0;

	return NULL;
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
	struct netmap_ring *txring = NULL;
	int i;
	uint64_t n = targ->g->npackets / targ->g->nthreads;
	uint64_t sent = 0;
	uint64_t event = 0;
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
			targ->ctr.pkts = sent;
			targ->ctr.bytes = sent*size;
			targ->ctr.events = sent;
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
			targ->ctr.pkts = sent;
			targ->ctr.bytes = sent*size;
			targ->ctr.events = sent;
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
#ifdef BUSYWAIT
		if (ioctl(pfd.fd, NIOCTXSYNC, NULL) < 0) {
			D("ioctl error on queue %d: %s", targ->me,
					strerror(errno));
			goto quit;
		}
#else /* !BUSYWAIT */
		if (poll(&pfd, 1, 2000) <= 0) {
			if (targ->cancel)
				break;
			D("poll error/timeout on queue %d: %s", targ->me,
				strerror(errno));
			// goto quit;
		}
		if (pfd.revents & POLLERR) {
			D("poll error on %d ring %d-%d", pfd.fd,
				targ->nmd->first_tx_ring, targ->nmd->last_tx_ring);
			goto quit;
		}
#endif /* !BUSYWAIT */
		/*
		 * scan our queues and send on those with room
		 */
		if (options & OPT_COPY && sent > 100000 && !(targ->g->options & OPT_COPY) ) {
			D("drop copy");
			options &= ~OPT_COPY;
		}
		for (i = targ->nmd->first_tx_ring; i <= targ->nmd->last_tx_ring; i++) {
			int m;
			uint64_t limit = rate_limit ?  tosend : targ->g->burst;
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
			if (m > 0) //XXX-ste: can m be 0?
				event++;
			targ->ctr.pkts = sent;
			targ->ctr.bytes = sent*size;
			targ->ctr.events = event;
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
		(void *)pthread_self());
	ioctl(pfd.fd, NIOCTXSYNC, NULL);

	/* final part: wait all the TX queues to be empty. */
	for (i = targ->nmd->first_tx_ring; i <= targ->nmd->last_tx_ring; i++) {
		txring = NETMAP_TXRING(nifp, i);
		while (!targ->cancel && nm_tx_pending(txring)) {
			RD(5, "pending tx tail %d head %d on ring %d",
				txring->tail, txring->head, i);
			ioctl(pfd.fd, NIOCTXSYNC, NULL);
			usleep(1); /* wait 1 tick */
		}
	}
    } /* end DEV_NETMAP */

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);
	targ->completed = 1;
	targ->ctr.pkts = sent;
	targ->ctr.bytes = sent*size;
	targ->ctr.events = event;
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
	struct my_ctrs *ctr = (struct my_ctrs *)user;
	(void)bytes;	/* UNUSED */
	ctr->bytes += h->len;
	ctr->pkts++;
}
#endif /* !NO_PCAP */


static int
receive_packets(struct netmap_ring *ring, u_int limit, int dump, uint64_t *bytes)
{
	u_int cur, rx, n;
	uint64_t b = 0;

	if (bytes == NULL)
		bytes = &b;

	cur = ring->cur;
	n = nm_ring_space(ring);
	if (n < limit)
		limit = n;
	for (rx = 0; rx < limit; rx++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);

		*bytes += slot->len;
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
	struct my_ctrs cur;

	cur.pkts = cur.bytes = cur.events = cur.min_space = 0;
	cur.t.tv_usec = cur.t.tv_sec = 0; //  unused, just silence the compiler

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
		i = read(targ->g->main_fd, buf, sizeof(buf));
		if (i > 0) {
			targ->ctr.pkts++;
			targ->ctr.bytes += i;
			targ->ctr.events++;
		}
	}
#ifndef NO_PCAP
    } else if (targ->g->dev_type == DEV_PCAP) {
	while (!targ->cancel) {
		/* XXX should we poll ? */
		pcap_dispatch(targ->g->p, targ->g->burst, receive_pcap,
			(u_char *)&targ->ctr);
                targ->ctr.events++;
	}
#endif /* !NO_PCAP */
    } else {
	int dump = targ->g->options & OPT_DUMP;

	nifp = targ->nmd->nifp;
	while (!targ->cancel) {
		/* Once we started to receive packets, wait at most 1 seconds
		   before quitting. */
#ifdef BUSYWAIT
		if (ioctl(pfd.fd, NIOCRXSYNC, NULL) < 0) {
			D("ioctl error on queue %d: %s", targ->me,
					strerror(errno));
			goto quit;
		}
#else /* !BUSYWAIT */
		if (poll(&pfd, 1, 1 * 1000) <= 0 && !targ->g->forever) {
			clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);
			targ->toc.tv_sec -= 1; /* Subtract timeout time. */
			goto out;
		}

		if (pfd.revents & POLLERR) {
			D("poll err");
			goto quit;
		}
#endif /* !BUSYWAIT */
		uint64_t cur_space = 0;
		for (i = targ->nmd->first_rx_ring; i <= targ->nmd->last_rx_ring; i++) {
			int m;

			rxring = NETMAP_RXRING(nifp, i);
			/* compute free space in the ring */
			m = rxring->head + rxring->num_slots - rxring->tail;
			if (m >= (int) rxring->num_slots)
				m -= rxring->num_slots;
			cur_space += m;
			if (nm_ring_empty(rxring))
				continue;

			m = receive_packets(rxring, targ->g->burst, dump, &cur.bytes);
			cur.pkts += m;
			if (m > 0) //XXX-ste: can m be 0?
				cur.events++;
		}
		cur.min_space = targ->ctr.min_space;
		if (cur_space < cur.min_space)
			cur.min_space = cur_space;
		targ->ctr = cur;
	}
    }

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);

#if !defined(BUSYWAIT)
out:
#endif
	targ->completed = 1;
	targ->ctr = cur;

quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}

static void *
txseq_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd pfd = { .fd = targ->fd, .events = POLLOUT };
	struct netmap_ring *ring;
	int64_t sent = 0;
	uint64_t event = 0;
	int options = targ->g->options | OPT_COPY;
	struct timespec nexttime = {0, 0};
	int rate_limit = targ->g->tx_rate;
	struct pkt *pkt = &targ->pkt;
	int frags = targ->g->frags;
	uint32_t sequence = 0;
	int budget = 0;
	void *frame;
	int size;

	if (targ->g->nthreads > 1) {
		D("can only txseq ping with 1 thread");
		return NULL;
	}

	if (targ->g->npackets > 0) {
		D("Ignoring -n argument");
	}

	frame = pkt;
	frame += sizeof(pkt->vh) - targ->g->virt_header;
	size = targ->g->pkt_size + targ->g->virt_header;

	D("start, fd %d main_fd %d", targ->fd, targ->g->main_fd);
	if (setaffinity(targ->thread, targ->affinity))
		goto quit;

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->tic);
	if (rate_limit) {
		targ->tic = timespec_add(targ->tic, (struct timespec){2,0});
		targ->tic.tv_nsec = 0;
		wait_time(targ->tic);
		nexttime = targ->tic;
	}

	/* Only use the first queue. */
	ring = NETMAP_TXRING(targ->nmd->nifp, targ->nmd->first_tx_ring);

	while (!targ->cancel) {
		int64_t limit;
		unsigned int space;
		unsigned int head;
		int fcnt;

		if (!rate_limit) {
			budget = targ->g->burst;

		} else if (budget <= 0) {
			budget = targ->g->burst;
			nexttime = timespec_add(nexttime, targ->g->tx_period);
			wait_time(nexttime);
		}

		/* wait for available room in the send queue */
		if (poll(&pfd, 1, 2000) <= 0) {
			if (targ->cancel)
				break;
			D("poll error/timeout on queue %d: %s", targ->me,
				strerror(errno));
		}
		if (pfd.revents & POLLERR) {
			D("poll error on %d ring %d-%d", pfd.fd,
				targ->nmd->first_tx_ring, targ->nmd->last_tx_ring);
			goto quit;
		}

		/* If no room poll() again. */
		space = nm_ring_space(ring);
		if (!space) {
			continue;
		}

		limit = budget;

		if (space < limit) {
			limit = space;
		}

		/* Cut off ``limit`` to make sure is multiple of ``frags``. */
		if (frags > 1) {
			limit = (limit / frags) * frags;
		}

		limit = sent + limit; /* Convert to absolute. */

		for (fcnt = frags, head = ring->head;
				sent < limit; sent++, sequence++) {
			struct netmap_slot *slot = &ring->slot[head];
			char *p = NETMAP_BUF(ring, slot->buf_idx);

			slot->flags = 0;
			pkt->body[0] = sequence >> 24;
			pkt->body[1] = (sequence >> 16) & 0xff;
			pkt->body[2] = (sequence >> 8) & 0xff;
			pkt->body[3] = sequence & 0xff;
			nm_pkt_copy(frame, p, size);
			if (fcnt == frags) {
				update_addresses(pkt, targ->g);
			}

			if (options & OPT_DUMP) {
				dump_payload(p, size, ring, head);
			}

			slot->len = size;

			if (--fcnt > 0) {
				slot->flags |= NS_MOREFRAG;
			} else {
				fcnt = frags;
			}

			if (sent == limit - 1) {
				/* Make sure we don't push an incomplete
				 * packet. */
				assert(!(slot->flags & NS_MOREFRAG));
				slot->flags |= NS_REPORT;
			}

			head = nm_ring_next(ring, head);
			if (rate_limit) {
				budget--;
			}
		}

		ring->cur = ring->head = head;

		event ++;
		targ->ctr.pkts = sent;
		targ->ctr.bytes = sent * size;
		targ->ctr.events = event;
	}

	/* flush any remaining packets */
	D("flush tail %d head %d on thread %p",
		ring->tail, ring->head,
		(void *)pthread_self());
	ioctl(pfd.fd, NIOCTXSYNC, NULL);

	/* final part: wait the TX queues to become empty. */
	while (!targ->cancel && nm_tx_pending(ring)) {
		RD(5, "pending tx tail %d head %d on ring %d",
				ring->tail, ring->head, targ->nmd->first_tx_ring);
		ioctl(pfd.fd, NIOCTXSYNC, NULL);
		usleep(1); /* wait 1 tick */
	}

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);
	targ->completed = 1;
	targ->ctr.pkts = sent;
	targ->ctr.bytes = sent * size;
	targ->ctr.events = event;
quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}


static char *
multi_slot_to_string(struct netmap_ring *ring, unsigned int head,
		     unsigned int nfrags, char *strbuf, size_t strbuflen)
{
	unsigned int f;
	char *ret = strbuf;

	for (f = 0; f < nfrags; f++) {
		struct netmap_slot *slot = &ring->slot[head];
		int m = snprintf(strbuf, strbuflen, "|%u,%x|", slot->len,
				 slot->flags);
		if (m >= (int)strbuflen) {
			break;
		}
		strbuf += m;
		strbuflen -= m;

		head = nm_ring_next(ring, head);
	}

	return ret;
}

static void *
rxseq_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd pfd = { .fd = targ->fd, .events = POLLIN };
	int dump = targ->g->options & OPT_DUMP;
	struct netmap_ring *ring;
	unsigned int frags_exp = 1;
	uint32_t seq_exp = 0;
	struct my_ctrs cur;
	unsigned int frags = 0;
	int first_packet = 1;
	int first_slot = 1;
	int i;

	cur.pkts = cur.bytes = cur.events = cur.min_space = 0;
	cur.t.tv_usec = cur.t.tv_sec = 0; //  unused, just silence the compiler

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

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->tic);

	ring = NETMAP_RXRING(targ->nmd->nifp, targ->nmd->first_rx_ring);

	while (!targ->cancel) {
		unsigned int head;
		uint32_t seq;
		int limit;

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

		if (nm_ring_empty(ring))
			continue;

		limit = nm_ring_space(ring);
		if (limit > targ->g->burst)
			limit = targ->g->burst;

#if 0
		/* Enable this if
		 *     1) we remove the early-return optimization from
		 *        the netmap poll implementation, or
		 *     2) pipes get NS_MOREFRAG support.
		 * With the current netmap implementation, an experiment like
		 *    pkt-gen -i vale:1{1 -f txseq -F 9
		 *    pkt-gen -i vale:1}1 -f rxseq
		 * would get stuck as soon as we find nm_ring_space(ring) < 9,
		 * since here limit is rounded to 0 and
		 * pipe rxsync is not called anymore by the poll() of this loop.
		 */
		if (frags_exp > 1) {
			int o = limit;
			/* Cut off to the closest smaller multiple. */
			limit = (limit / frags_exp) * frags_exp;
			RD(2, "LIMIT %d --> %d", o, limit);
		}
#endif

		for (head = ring->head, i = 0; i < limit; i++) {
			struct netmap_slot *slot = &ring->slot[head];
			char *p = NETMAP_BUF(ring, slot->buf_idx);
			int len = slot->len;
			struct pkt *pkt;

			if (dump) {
				dump_payload(p, slot->len, ring, head);
			}

			frags++;
			if (!(slot->flags & NS_MOREFRAG)) {
				if (first_packet) {
					first_packet = 0;
				} else if (frags != frags_exp) {
					char prbuf[512];
					RD(1, "Received packets with %u frags, "
					      "expected %u, '%s'", frags, frags_exp,
					      multi_slot_to_string(ring, head-frags+1, frags,
								   prbuf, sizeof(prbuf)));
				}
				first_packet = 0;
				frags_exp = frags;
				frags = 0;
			}

			p -= sizeof(pkt->vh) - targ->g->virt_header;
			len += sizeof(pkt->vh) - targ->g->virt_header;
			pkt = (struct pkt *)p;

			if ((char *)pkt + len < ((char *)pkt->body) + sizeof(seq)) {
				RD(1, "%s: packet too small (len=%u)", __func__,
				      slot->len);
			} else {
				seq = (pkt->body[0] << 24) | (pkt->body[1] << 16)
				      | (pkt->body[2] << 8) | pkt->body[3];
				if (first_slot) {
					/* Grab the first one, whatever it
					   is. */
					seq_exp = seq;
					first_slot = 0;
				} else if (seq != seq_exp) {
					uint32_t delta = seq - seq_exp;

					if (delta < (0xFFFFFFFF >> 1)) {
						RD(2, "Sequence GAP: exp %u found %u",
						      seq_exp, seq);
					} else {
						RD(2, "Sequence OUT OF ORDER: "
						      "exp %u found %u", seq_exp, seq);
					}
					seq_exp = seq;
				}
				seq_exp++;
			}

			cur.bytes += slot->len;
			head = nm_ring_next(ring, head);
			cur.pkts++;
		}

		ring->cur = ring->head = head;

		cur.events++;
		targ->ctr = cur;
	}

	clock_gettime(CLOCK_REALTIME_PRECISE, &targ->toc);

out:
	targ->completed = 1;
	targ->ctr = cur;

quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}


static void
tx_output(struct my_ctrs *cur, double delta, const char *msg)
{
	double bw, raw_bw, pps, abs;
	char b1[40], b2[80], b3[80];
	int size;

	if (cur->pkts == 0) {
		printf("%s nothing.\n", msg);
		return;
	}

	size = (int)(cur->bytes / cur->pkts);

	printf("%s %llu packets %llu bytes %llu events %d bytes each in %.2f seconds.\n",
		msg,
		(unsigned long long)cur->pkts,
		(unsigned long long)cur->bytes,
		(unsigned long long)cur->events, size, delta);
	if (delta == 0)
		delta = 1e-6;
	if (size < 60)		/* correct for min packet size */
		size = 60;
	pps = cur->pkts / delta;
	bw = (8.0 * cur->bytes) / delta;
	/* raw packets have4 bytes crc + 20 bytes framing */
	raw_bw = (8.0 * (cur->pkts * 24 + cur->bytes)) / delta;
	abs = cur->pkts / (double)(cur->events);

	printf("Speed: %spps Bandwidth: %sbps (raw %sbps). Average batch: %.2f pkts\n",
		norm(b1, pps), norm(b2, bw), norm(b3, raw_bw), abs);
}

static void
usage(void)
{
	const char *cmd = "pkt-gen";
	fprintf(stderr,
		"Usage:\n"
		"%s arguments\n"
		"\t-i interface		interface name\n"
		"\t-f function		tx rx ping pong txseq rxseq\n"
		"\t-n count		number of iterations (can be 0)\n"
		"\t-t pkts_to_send	also forces tx mode\n"
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
		"\t-w wait_for_link_time	in seconds\n"
		"\t-R rate		in packets per second\n"
		"\t-X			dump payload\n"
		"\t-H len		add empty virtio-net-header with size 'len'\n"
		"\t-E pipes		allocate extra space for a number of pipes\n"
		"\t-r			do not touch the buffers (send rubbish)\n"
	        "\t-P file		load packet from pcap file\n"
		"\t-z			use random IPv4 src address/port\n"
		"\t-Z			use random IPv4 dst address/port\n"
		"\t-F num_frags		send multi-slot packets\n"
		"\t-A			activate pps stats on receiver\n"
		"",
		cmd);

	exit(0);
}

enum {
	TD_TYPE_SENDER = 1,
	TD_TYPE_RECEIVER,
	TD_TYPE_OTHER,
};

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

		if (i > 0) {
			/* the first thread uses the fd opened by the main
			 * thread, the other threads re-open /dev/netmap
			 */
			if (g->nthreads > 1) {
				nmd.req.nr_flags =
					g->nmd->req.nr_flags & ~NR_REG_MASK;
				nmd.req.nr_flags |= NR_REG_ONE_NIC;
				nmd.req.nr_ringid = i;
			}
			/* Only touch one of the rings (rx is already ok) */
			if (g->td_type == TD_TYPE_RECEIVER)
				nmd_flags |= NETMAP_NO_TX_POLL;

			/* register interface. Override ifname and ringid etc. */
			t->nmd = nm_open(t->g->ifname, NULL, nmd_flags |
				NM_OPEN_IFNAME | NM_OPEN_NO_MMAP, &nmd);
			if (t->nmd == NULL) {
				D("Unable to open %s: %s",
					t->g->ifname, strerror(errno));
				continue;
			}
		} else {
			t->nmd = g->nmd;
		}
		t->fd = t->nmd->fd;

	    } else {
		targs[i].fd = g->main_fd;
	    }
		t->used = 1;
		t->me = i;
		if (g->affinity >= 0) {
			t->affinity = (g->affinity + i) % g->system_cpus;
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

	struct my_ctrs prev, cur;
	double delta_t;
	struct timeval tic, toc;

	prev.pkts = prev.bytes = prev.events = 0;
	gettimeofday(&prev.t, NULL);
	for (;;) {
		char b1[40], b2[40], b3[40], b4[70];
		uint64_t pps, usec;
		struct my_ctrs x;
		double abs;
		int done = 0;

		usec = wait_for_next_report(&prev.t, &cur.t,
				g->report_interval);

		cur.pkts = cur.bytes = cur.events = 0;
		cur.min_space = 0;
		if (usec < 10000) /* too short to be meaningful */
			continue;
		/* accumulate counts for all threads */
		for (i = 0; i < g->nthreads; i++) {
			cur.pkts += targs[i].ctr.pkts;
			cur.bytes += targs[i].ctr.bytes;
			cur.events += targs[i].ctr.events;
			cur.min_space += targs[i].ctr.min_space;
			targs[i].ctr.min_space = 99999;
			if (targs[i].used == 0)
				done++;
		}
		x.pkts = cur.pkts - prev.pkts;
		x.bytes = cur.bytes - prev.bytes;
		x.events = cur.events - prev.events;
		pps = (x.pkts*1000000 + usec/2) / usec;
		abs = (x.events > 0) ? (x.pkts / (double) x.events) : 0;

		if (!(g->options & OPT_PPS_STATS)) {
			strcpy(b4, "");
		} else {
			/* Compute some pps stats using a sliding window. */
			double ppsavg = 0.0, ppsdev = 0.0;
			int nsamples = 0;

			g->win[g->win_idx] = pps;
			g->win_idx = (g->win_idx + 1) % STATS_WIN;

			for (i = 0; i < STATS_WIN; i++) {
				ppsavg += g->win[i];
				if (g->win[i]) {
					nsamples ++;
				}
			}
			ppsavg /= nsamples;

			for (i = 0; i < STATS_WIN; i++) {
				if (g->win[i] == 0) {
					continue;
				}
				ppsdev += (g->win[i] - ppsavg) * (g->win[i] - ppsavg);
			}
			ppsdev /= nsamples;
			ppsdev = sqrt(ppsdev);

			snprintf(b4, sizeof(b4), "[avg/std %s/%s pps]",
				 norm(b1, ppsavg), norm(b2, ppsdev));
		}

		D("%spps %s(%spkts %sbps in %llu usec) %.2f avg_batch %d min_space",
			norm(b1, pps), b4,
			norm(b2, (double)x.pkts),
			norm(b3, (double)x.bytes*8),
			(unsigned long long)usec,
			abs, (int)cur.min_space);
		prev = cur;

		if (done == g->nthreads)
			break;
	}

	timerclear(&tic);
	timerclear(&toc);
	cur.pkts = cur.bytes = cur.events = 0;
	/* final round */
	for (i = 0; i < g->nthreads; i++) {
		struct timespec t_tic, t_toc;
		/*
		 * Join active threads, unregister interfaces and close
		 * file descriptors.
		 */
		if (targs[i].used)
			pthread_join(targs[i].thread, NULL); /* blocking */
		if (g->dev_type == DEV_NETMAP) {
			nm_close(targs[i].nmd);
			targs[i].nmd = NULL;
		} else {
			close(targs[i].fd);
		}

		if (targs[i].completed == 0)
			D("ouch, thread %d exited with error", i);

		/*
		 * Collect threads output and extract information about
		 * how long it took to send all the packets.
		 */
		cur.pkts += targs[i].ctr.pkts;
		cur.bytes += targs[i].ctr.bytes;
		cur.events += targs[i].ctr.events;
		/* collect the largest start (tic) and end (toc) times,
		 * XXX maybe we should do the earliest tic, or do a weighted
		 * average ?
		 */
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
	if (g->td_type == TD_TYPE_SENDER)
		tx_output(&cur, delta_t, "Sent");
	else
		tx_output(&cur, delta_t, "Received");
}

struct td_desc {
	int ty;
	char *key;
	void *f;
};

static struct td_desc func[] = {
	{ TD_TYPE_SENDER,	"tx",		sender_body },
	{ TD_TYPE_RECEIVER,	"rx",		receiver_body },
	{ TD_TYPE_OTHER,	"ping",		pinger_body },
	{ TD_TYPE_OTHER,	"pong",		ponger_body },
	{ TD_TYPE_SENDER,	"txseq",	txseq_body },
	{ TD_TYPE_RECEIVER,	"rxseq",	rxseq_body },
	{ 0,			NULL,	NULL }
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
	struct sigaction sa;
	sigset_t ss;

	struct glob_arg g;

	int ch;
	int wait_link = 2;
	int devqueues = 1;	/* how many device queues */

	bzero(&g, sizeof(g));

	g.main_fd = -1;
	g.td_body = receiver_body;
	g.td_type = TD_TYPE_RECEIVER;
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
	g.cpus = 1;		// default
	g.forever = 1;
	g.tx_rate = 0;
	g.frags = 1;
	g.nmr_config = "";
	g.virt_header = 0;

	while ( (ch = getopt(arc, argv,
			"a:f:F:n:i:Il:d:s:D:S:b:c:o:p:T:w:WvR:XC:H:e:E:m:rP:zZA")) != -1) {
		struct td_desc *fn;

		switch(ch) {
		default:
			D("bad option %c %s", ch, optarg);
			usage();
			break;

		case 'n':
			g.npackets = strtoull(optarg, NULL, 10);
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
			if (fn->key) {
				g.td_body = fn->f;
				g.td_type = fn->ty;
			} else {
				D("unrecognised function %s", optarg);
			}
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
		case 'E':
			g.extra_pipes = atoi(optarg);
			break;
		case 'P':
			g.packet_file = strdup(optarg);
			break;
		case 'm':
			/* ignored */
			break;
		case 'r':
			g.options |= OPT_RUBBISH;
			break;
		case 'z':
			g.options |= OPT_RANDOM_SRC;
			break;
		case 'Z':
			g.options |= OPT_RANDOM_DST;
			break;
		case 'A':
			g.options |= OPT_PPS_STATS;
			break;
		}
	}

	if (strlen(g.ifname) <=0 ) {
		D("missing ifname");
		usage();
	}

	g.system_cpus = i = system_ncpus();
	if (g.cpus < 0 || g.cpus > i) {
		D("%d cpus is too high, have only %d cpus", g.cpus, i);
		usage();
	}
D("running on %d cpus (have %d)", g.cpus, i);
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
	if (g.extra_pipes) {
	    base_nmd.nr_arg1 = g.extra_pipes;
	}

	base_nmd.nr_flags |= NR_ACCEPT_VNET_HDR;

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

	if (g.nthreads > 1) {
		struct nm_desc saved_desc = *g.nmd;
		saved_desc.self = &saved_desc;
		saved_desc.mem = NULL;
		nm_close(g.nmd);
		saved_desc.req.nr_flags &= ~NR_REG_MASK;
		saved_desc.req.nr_flags |= NR_REG_ONE_NIC;
		saved_desc.req.nr_ringid = 0;
		g.nmd = nm_open(g.ifname, &base_nmd, NM_OPEN_IFNAME, &saved_desc);
		if (g.nmd == NULL) {
			D("Unable to open %s: %s", g.ifname, strerror(errno));
			goto out;
		}
	}
	g.main_fd = g.nmd->fd;
	D("mapped %dKB at %p", g.nmd->req.nr_memsize>>10, g.nmd->mem);

	if (g.virt_header) {
		/* Set the virtio-net header length, since the user asked
		 * for it explicitely. */
		set_vnet_hdr_len(&g);
	} else {
		/* Check whether the netmap port we opened requires us to send
		 * and receive frames with virtio-net header. */
		get_vnet_hdr_len(&g);
	}

	/* get num of queues in tx or rx */
	if (g.td_type == TD_TYPE_SENDER)
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
			D("   TX%d at 0x%p slots %d", i,
			    (void *)((char *)ring - (char *)nifp), ring->num_slots);
		}
		for (i = 0; i <= req->nr_rx_rings; i++) {
			struct netmap_ring *ring = NETMAP_RXRING(nifp, i);
			D("   RX%d at 0x%p slots %d", i,
			    (void *)((char *)ring - (char *)nifp), ring->num_slots);
		}
	}

	/* Print some debug information. */
	fprintf(stdout,
		"%s %s: %d queues, %d threads and %d cpus.\n",
		(g.td_type == TD_TYPE_SENDER) ? "Sending on" :
			((g.td_type == TD_TYPE_RECEIVER) ? "Receiving from" :
			"Working on"),
		g.ifname,
		devqueues,
		g.nthreads,
		g.cpus);
	if (g.td_type == TD_TYPE_SENDER) {
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
		D("--- SPECIAL OPTIONS:%s%s%s%s%s%s\n",
			g.options & OPT_PREFETCH ? " prefetch" : "",
			g.options & OPT_ACCESS ? " access" : "",
			g.options & OPT_MEMCPY ? " memcpy" : "",
			g.options & OPT_INDIRECT ? " indirect" : "",
			g.options & OPT_COPY ? " copy" : "",
			g.options & OPT_RUBBISH ? " rubbish " : "");
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
	if (g.td_type == TD_TYPE_SENDER)
	    D("Sending %d packets every  %ld.%09ld s",
			g.burst, g.tx_period.tv_sec, g.tx_period.tv_nsec);
	/* Wait for PHY reset. */
	D("Wait %d secs for phy reset", wait_link);
	sleep(wait_link);
	D("Ready...");

	/* Install ^C handler. */
	global_nthreads = g.nthreads;
	sigemptyset(&ss);
	sigaddset(&ss, SIGINT);
	/* block SIGINT now, so that all created threads will inherit the mask */
	if (pthread_sigmask(SIG_BLOCK, &ss, NULL) < 0) {
		D("failed to block SIGINT: %s", strerror(errno));
	}
	start_threads(&g);
	/* Install the handler and re-enable SIGINT for the main thread */
	sa.sa_handler = sigint_h;
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		D("failed to install ^C handler: %s", strerror(errno));
	}

	if (pthread_sigmask(SIG_UNBLOCK, &ss, NULL) < 0) {
		D("failed to re-enable SIGINT: %s", strerror(errno));
	}
	main_thread(&g);
	free(targs);
	return 0;
}

/* end of file */
