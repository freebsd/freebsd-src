/*
 * (C) 2011-2012 Luigi Rizzo
 *
 * BSD license
 *
 * A simple library that maps some pcap functions onto netmap
 * This is not 100% complete but enough to let tcpdump, trafshow
 * and other apps work.
 *
 * $FreeBSD$
 */

#define MY_PCAP
#include "nm_util.h"

char *version = "$Id$";
int verbose = 0;

/*
 * We redefine here a number of structures that are in pcap.h
 * so we can compile this file without the system header.
 */
#ifndef PCAP_ERRBUF_SIZE
#define PCAP_ERRBUF_SIZE 128
/*
 * Each packet is accompanied by a header including the timestamp,
 * captured size and actual size.
 */
struct pcap_pkthdr {
	struct timeval ts;	/* time stamp */
	uint32_t caplen;	/* length of portion present */
	uint32_t len;		/* length this packet (off wire) */
};

typedef struct pcap_if pcap_if_t;

/*
 * Representation of an interface address.
 */
struct pcap_addr {
	struct pcap_addr *next;
	struct sockaddr *addr;		/* address */
	struct sockaddr *netmask;	/* netmask for the above */
	struct sockaddr *broadaddr;	/* broadcast addr for the above */
	struct sockaddr *dstaddr;	/* P2P dest. address for the above */
};

struct pcap_if {
	struct pcap_if *next;
	char *name;		/* name to hand to "pcap_open_live()" */
	char *description;	/* textual description of interface, or NULL */
	struct pcap_addr *addresses;
	uint32_t flags;      /* PCAP_IF_ interface flags */
};

/*
 * We do not support stats (yet)
 */
struct pcap_stat {
	u_int ps_recv;		/* number of packets received */
	u_int ps_drop;		/* number of packets dropped */
	u_int ps_ifdrop;	/* drops by interface XXX not yet supported */
#ifdef WIN32
	u_int bs_capt;		/* number of packets that reach the app. */
#endif /* WIN32 */
};

typedef void	pcap_t;
typedef enum {
	PCAP_D_INOUT = 0,
	PCAP_D_IN,
	PCAP_D_OUT
} pcap_direction_t;
 


typedef void (*pcap_handler)(u_char *user,
		const struct pcap_pkthdr *h, const u_char *bytes);

char errbuf[PCAP_ERRBUF_SIZE];

pcap_t *pcap_open_live(const char *device, int snaplen,
               int promisc, int to_ms, char *errbuf);

int pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf);
void pcap_close(pcap_t *p);
int pcap_get_selectable_fd(pcap_t *p);
int pcap_dispatch(pcap_t *p, int cnt, pcap_handler callback, u_char *user);
int pcap_setnonblock(pcap_t *p, int nonblock, char *errbuf);
int pcap_setdirection(pcap_t *p, pcap_direction_t d);
char *pcap_lookupdev(char *errbuf);
int pcap_inject(pcap_t *p, const void *buf, size_t size);
int pcap_fileno(pcap_t *p);
const char *pcap_lib_version(void);


struct eproto {
	const char *s;
	u_short p;
};
#endif /* !PCAP_ERRBUF_SIZE */

#ifndef TEST
/*
 * build as a shared library
 */

char pcap_version[] = "libnetmap version 0.3";

/*
 * Our equivalent of pcap_t
 */
struct pcap_ring {
	struct my_ring me;
#if 0
	const char *ifname;

	//struct nmreq nmr;

	int fd;
	char *mem;			/* userspace mmap address */
	u_int memsize;
	u_int queueid;
	u_int begin, end;		/* first..last+1 rings to check */
	struct netmap_if *nifp;

	uint32_t if_flags;
	uint32_t if_reqcap;
	uint32_t if_curcap;
#endif
	int snaplen;
	char *errbuf;
	int promisc;
	int to_ms;

	struct pcap_pkthdr hdr;


	struct pcap_stat st;

	char msg[PCAP_ERRBUF_SIZE];
};



/*
 * There is a set of functions that tcpdump expects even if probably
 * not used
 */
struct eproto eproto_db[] = {
	{ "ip", ETHERTYPE_IP },
	{ "arp", ETHERTYPE_ARP },
	{ (char *)0, 0 }
};


const char *pcap_lib_version(void)
{
	return pcap_version;
}

int
pcap_findalldevs(pcap_if_t **alldevsp, char *errbuf)
{
	pcap_if_t *top = NULL;
#ifndef linux
	struct ifaddrs *i_head, *i;
	pcap_if_t *cur;
	struct pcap_addr *tail = NULL;
	int l;

	D("listing all devs");
	*alldevsp = NULL;
	i_head = NULL;

	if (getifaddrs(&i_head)) {
		D("cannot get if addresses");
		return -1;
	}
	for (i = i_head; i; i = i->ifa_next) {
		//struct ifaddrs   *ifa;
		struct pcap_addr *pca;
		//struct sockaddr *sa;

		D("got interface %s", i->ifa_name);
		if (!top || strcmp(top->name, i->ifa_name)) {
			/* new interface */
			l = sizeof(*top) + strlen(i->ifa_name) + 1;
			cur = calloc(1, l);
			if (cur == NULL) {
				D("no space for if descriptor");
				continue;
			}
			cur->name = (char *)(cur + 1);
			//cur->flags = i->ifa_flags;
			strcpy(cur->name, i->ifa_name);
			cur->description = NULL;
			cur->next = top;
			top = cur;
			tail = NULL;
		}
		/* now deal with addresses */
		D("%s addr family %d len %d %s %s",
			top->name,
			i->ifa_addr->sa_family, i->ifa_addr->sa_len,
			i->ifa_netmask ? "Netmask" : "",
			i->ifa_broadaddr ? "Broadcast" : "");
		l = sizeof(struct pcap_addr) +
			(i->ifa_addr ? i->ifa_addr->sa_len:0) +
			(i->ifa_netmask ? i->ifa_netmask->sa_len:0) +
			(i->ifa_broadaddr? i->ifa_broadaddr->sa_len:0);
		pca = calloc(1, l);
		if (pca == NULL) {
			D("no space for if addr");
			continue;
		}
#define SA_NEXT(x) ((struct sockaddr *)((char *)(x) + (x)->sa_len))
		pca->addr = (struct sockaddr *)(pca + 1);
		pkt_copy(i->ifa_addr, pca->addr, i->ifa_addr->sa_len);
		if (i->ifa_netmask) {
			pca->netmask = SA_NEXT(pca->addr);
			bcopy(i->ifa_netmask, pca->netmask, i->ifa_netmask->sa_len);
			if (i->ifa_broadaddr) {
				pca->broadaddr = SA_NEXT(pca->netmask);
				bcopy(i->ifa_broadaddr, pca->broadaddr, i->ifa_broadaddr->sa_len);
			}
		}
		if (tail == NULL) {
			top->addresses = pca;
		} else {
			tail->next = pca;
		}
		tail = pca;

	}
	freeifaddrs(i_head);
#endif /* !linux */
	(void)errbuf;	/* UNUSED */
	*alldevsp = top;
	return 0;
}

void pcap_freealldevs(pcap_if_t *alldevs)
{
	(void)alldevs;	/* UNUSED */
	D("unimplemented");
}

char *
pcap_lookupdev(char *buf)
{
	D("%s", buf);
	strcpy(buf, "/dev/netmap");
	return buf;
}

pcap_t *
pcap_create(const char *source, char *errbuf)
{
	D("src %s (call open liveted)", source);
	return pcap_open_live(source, 0, 1, 100, errbuf);
}

int
pcap_activate(pcap_t *p)
{
	D("pcap %p running", p);
	return 0;
}

int
pcap_can_set_rfmon(pcap_t *p)
{
	(void)p;	/* UNUSED */
	D("");
	return 0;	/* no we can't */
}

int
pcap_set_snaplen(pcap_t *p, int snaplen)
{
	struct pcap_ring *me = p;

	D("len %d", snaplen);
	me->snaplen = snaplen;
	return 0;
}

int
pcap_snapshot(pcap_t *p)
{
	struct pcap_ring *me = p;

	D("len %d", me->snaplen);
	return me->snaplen;
}

int
pcap_lookupnet(const char *device, uint32_t *netp,
	uint32_t *maskp, char *errbuf)
{

	(void)errbuf;	/* UNUSED */
	D("device %s", device);
	inet_aton("10.0.0.255", (struct in_addr *)netp);
	inet_aton("255.255.255.0",(struct in_addr *) maskp);
	return 0;
}

int
pcap_set_promisc(pcap_t *p, int promisc)
{
	struct pcap_ring *me = p;

	D("promisc %d", promisc);
        if (nm_do_ioctl(&me->me, SIOCGIFFLAGS, 0))
		D("SIOCGIFFLAGS failed");
	if (promisc) {
		me->me.if_flags |= IFF_PPROMISC;
	} else {
		me->me.if_flags &= ~IFF_PPROMISC;
	}
	if (nm_do_ioctl(&me->me, SIOCSIFFLAGS, 0))
		D("SIOCSIFFLAGS failed");
	return 0;
}

int
pcap_set_timeout(pcap_t *p, int to_ms)
{
	struct pcap_ring *me = p;

	D("%d ms", to_ms);
	me->to_ms = to_ms;
	return 0;
}

struct bpf_program;

int
pcap_compile(pcap_t *p, struct bpf_program *fp,
	const char *str, int optimize, uint32_t netmask)
{
	(void)p;	/* UNUSED */
	(void)fp;	/* UNUSED */
	(void)optimize;	/* UNUSED */
	(void)netmask;	/* UNUSED */
	D("%s", str);
	return 0;
}

int
pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{
	(void)p;	/* UNUSED */
	(void)fp;	/* UNUSED */
	D("");
	return 0;
}

int
pcap_datalink(pcap_t *p)
{
	(void)p;	/* UNUSED */
	D("returns 1");
	return 1;	// ethernet
}

const char *
pcap_datalink_val_to_name(int dlt)
{
	D("%d returns DLT_EN10MB", dlt);
	return "DLT_EN10MB";
}

const char *
pcap_datalink_val_to_description(int dlt)
{
	D("%d returns Ethernet link", dlt);
	return "Ethernet link";
}

struct pcap_stat;
int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{
	struct pcap_ring *me = p;
	ND("");

	*ps = me->st;
	return 0;	/* accumulate from pcap_dispatch() */
};

char *
pcap_geterr(pcap_t *p)
{
	struct pcap_ring *me = p;

	D("");
	return me->msg;
}

pcap_t *
pcap_open_live(const char *device, int snaplen,
               int promisc, int to_ms, char *errbuf)
{
	struct pcap_ring *me;
	int l;

	(void)snaplen;	/* UNUSED */
	(void)errbuf;	/* UNUSED */
	if (!device) {
		D("missing device name");
		return NULL;
	}

	l = strlen(device) + 1;
	D("request to open %s snaplen %d promisc %d timeout %dms",
		device, snaplen, promisc, to_ms);
	me = calloc(1, sizeof(*me) + l);
	if (me == NULL) {
		D("failed to allocate struct for %s", device);
		return NULL;
	}
	me->me.ifname = (char *)(me + 1);
	strcpy((char *)me->me.ifname, device);
	if (netmap_open(&me->me, 0, promisc)) {
		D("error opening %s", device);
		free(me);
		return NULL;
	}
	me->to_ms = to_ms;

	return (pcap_t *)me;
}

void
pcap_close(pcap_t *p)
{
	struct my_ring *me = p;

	D("");
	if (!me)
		return;
	if (me->mem)
		munmap(me->mem, me->memsize);
	/* restore original flags ? */
	close(me->fd);
	bzero(me, sizeof(*me));
	free(me);
}

int
pcap_fileno(pcap_t *p)
{
	struct my_ring *me = p;
	D("returns %d", me->fd);
	return me->fd;
}

int
pcap_get_selectable_fd(pcap_t *p)
{
	struct my_ring *me = p;

	ND("");
	return me->fd;
}

int
pcap_setnonblock(pcap_t *p, int nonblock, char *errbuf)
{
	(void)p;	/* UNUSED */
	(void)errbuf;	/* UNUSED */
	D("mode is %d", nonblock);
	return 0;	/* ignore */
}

int
pcap_setdirection(pcap_t *p, pcap_direction_t d)
{
	(void)p;	/* UNUSED */
	(void)d;	/* UNUSED */
	D("");
	return 0;	/* ignore */
};

int
pcap_dispatch(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	struct pcap_ring *pme = p;
	struct my_ring *me = &pme->me;
	int got = 0;
	u_int si;

	ND("cnt %d", cnt);
	if (cnt == 0)
		cnt = -1;
	/* scan all rings */
	for (si = me->begin; si < me->end; si++) {
		struct netmap_ring *ring = NETMAP_RXRING(me->nifp, si);
		ND("ring has %d pkts", ring->avail);
		if (ring->avail == 0)
			continue;
		pme->hdr.ts = ring->ts;
		/*
		 * XXX a proper prefetch should be done as
		 *	prefetch(i); callback(i-1); ...
		 */
		while ((cnt == -1 || cnt != got) && ring->avail > 0) {
			u_int i = ring->cur;
			u_int idx = ring->slot[i].buf_idx;
			if (idx < 2) {
				D("%s bogus RX index %d at offset %d",
					me->nifp->ni_name, idx, i);
				sleep(2);
			}
			u_char *buf = (u_char *)NETMAP_BUF(ring, idx);
			prefetch(buf);
			pme->hdr.len = pme->hdr.caplen = ring->slot[i].len;
			// D("call %p len %d", p, me->hdr.len);
			callback(user, &pme->hdr, buf);
			ring->cur = NETMAP_RING_NEXT(ring, i);
			ring->avail--;
			got++;
		}
	}
	pme->st.ps_recv += got;
	return got;
}

int
pcap_inject(pcap_t *p, const void *buf, size_t size)
{
        struct my_ring *me = p;
        u_int si;
 
        ND("cnt %d", cnt);
        /* scan all rings */
        for (si = me->begin; si < me->end; si++) {
                struct netmap_ring *ring = NETMAP_TXRING(me->nifp, si);
 
                ND("ring has %d pkts", ring->avail);
                if (ring->avail == 0)
                        continue;
		u_int i = ring->cur;
		u_int idx = ring->slot[i].buf_idx;
		if (idx < 2) {
			D("%s bogus TX index %d at offset %d",
				me->nifp->ni_name, idx, i);
			sleep(2);
		}
		u_char *dst = (u_char *)NETMAP_BUF(ring, idx);
		ring->slot[i].len = size;
		pkt_copy(buf, dst, size);
		ring->cur = NETMAP_RING_NEXT(ring, i);
		ring->avail--;
		// if (ring->avail == 0) ioctl(me->fd, NIOCTXSYNC, NULL);
		return size;
        }
	errno = ENOBUFS;
	return -1;
}

int
pcap_loop(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	struct pcap_ring *me = p;
	struct pollfd fds[1];
	int i;

	ND("cnt %d", cnt);
	memset(fds, 0, sizeof(fds));
	fds[0].fd = me->me.fd;
	fds[0].events = (POLLIN);

	while (cnt == -1 || cnt > 0) {
                if (poll(fds, 1, me->to_ms) <= 0) {
                        D("poll error/timeout");
			continue;
                }
		i = pcap_dispatch(p, cnt, callback, user);
		if (cnt > 0)
			cnt -= i;
	}
	return 0;
}

#endif /* !TEST */

#ifdef TEST	/* build test code */
void do_send(u_char *user, const struct pcap_pkthdr *h, const u_char *buf)
{
	pcap_inject((pcap_t *)user, buf, h->caplen);
}

/*
 * a simple pcap test program, bridge between two interfaces.
 */
int
main(int argc, char **argv)
{
	pcap_t *p0, *p1;
	int burst = 1024;
	struct pollfd pollfd[2];

	fprintf(stderr, "%s %s built %s %s\n",
		argv[0], version, __DATE__, __TIME__);
		
	while (argc > 1 && !strcmp(argv[1], "-v")) {
		verbose++;
		argv++;
		argc--;
	}

	if (argc < 3 || argc > 4 || !strcmp(argv[1], argv[2])) {
		D("Usage: %s IFNAME1 IFNAME2 [BURST]", argv[0]);
		return (1);
	}
	if (argc > 3)
		burst = atoi(argv[3]);

	p0 = pcap_open_live(argv[1], 0, 1, 100, NULL);
	p1 = pcap_open_live(argv[2], 0, 1, 100, NULL);
	D("%s", version);
	D("open returns %p %p", p0, p1);
	if (!p0 || !p1)
		return(1);
	bzero(pollfd, sizeof(pollfd));
	pollfd[0].fd = pcap_fileno(p0);
	pollfd[1].fd = pcap_fileno(p1);
	pollfd[0].events = pollfd[1].events = POLLIN;
	for (;;) {
		/* do i need to reset ? */
		pollfd[0].revents = pollfd[1].revents = 0;
		int ret = poll(pollfd, 2, 1000);
		if (ret <= 0 || verbose)
                   D("poll %s [0] ev %x %x [1] ev %x %x",
                        ret <= 0 ? "timeout" : "ok",
                                pollfd[0].events,
                                pollfd[0].revents,
                                pollfd[1].events,
                                pollfd[1].revents);
		if (ret < 0)
			continue;
		if (pollfd[0].revents & POLLIN)
			pcap_dispatch(p0, burst, do_send, p1);
		if (pollfd[1].revents & POLLIN)
			pcap_dispatch(p1, burst, do_send, p0);
	}

	return (0);
}
#endif /* TEST */
