/*
 * (C) 2011 Luigi Rizzo
 *
 * BSD license
 *
 * A simple library that maps some pcap functions onto netmap
 * This is not 100% complete but enough to let tcpdump, trafshow
 * and other apps work.
 *
 * $FreeBSD$
 */

#include <errno.h>
#include <signal.h> /* signal */
#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strcmp */
#include <fcntl.h> /* open */
#include <unistd.h> /* close */

#include <sys/endian.h> /* le64toh */
#include <sys/mman.h> /* PROT_* */
#include <sys/ioctl.h> /* ioctl */
#include <machine/param.h>
#include <sys/poll.h>
#include <sys/socket.h> /* sockaddr.. */
#include <arpa/inet.h> /* ntohs */

#include <net/if.h>	/* ifreq */
#include <net/ethernet.h>
#include <net/netmap.h>
#include <net/netmap_user.h>

#include <netinet/in.h> /* sockaddr_in */

#include <sys/socket.h>
#include <ifaddrs.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

char *version = "$Id$";
int verbose = 0;

/* debug support */
#define ND(format, ...) do {} while (0)
#define D(format, ...) do {				\
    if (verbose)					\
        fprintf(stderr, "--- %s [%d] " format "\n",	\
        __FUNCTION__, __LINE__, ##__VA_ARGS__);		\
	} while (0)


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

struct eproto {
	const char *s;
	u_short p;
};
#endif /* !PCAP_ERRBUF_SIZE */

#ifdef __PIC__
/*
 * build as a shared library
 */

char pcap_version[] = "libnetmap version 0.3";

/*
 * Our equivalent of pcap_t
 */
struct my_ring {
	struct nmreq nmr;

	int fd;
	char *mem;			/* userspace mmap address */
	u_int memsize;
	u_int queueid;
	u_int begin, end;		/* first..last+1 rings to check */
	struct netmap_if *nifp;

	int snaplen;
	char *errbuf;
	int promisc;
	int to_ms;

	struct pcap_pkthdr hdr;

	uint32_t if_flags;
	uint32_t if_reqcap;
	uint32_t if_curcap;

	struct pcap_stat st;

	char msg[PCAP_ERRBUF_SIZE];
};


static int
do_ioctl(struct my_ring *me, int what)
{
	struct ifreq ifr;
	int error;

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, me->nmr.nr_name, sizeof(ifr.ifr_name));
	switch (what) {
	case SIOCSIFFLAGS:
		D("call SIOCSIFFLAGS 0x%x", me->if_flags);
		ifr.ifr_flagshigh = (me->if_flags >> 16) & 0xffff;
		ifr.ifr_flags = me->if_flags & 0xffff;
		break;
	case SIOCSIFCAP:
		ifr.ifr_reqcap = me->if_reqcap;
		ifr.ifr_curcap = me->if_curcap;
		break;
	}
	error = ioctl(me->fd, what, &ifr);
	if (error) {
		D("ioctl 0x%x error %d", what, error);
		return error;
	}
	switch (what) {
	case SIOCSIFFLAGS:
	case SIOCGIFFLAGS:
		me->if_flags = (ifr.ifr_flagshigh << 16) |
			(0xffff & ifr.ifr_flags);
		D("flags are L 0x%x H 0x%x 0x%x",
			(uint16_t)ifr.ifr_flags,
			(uint16_t)ifr.ifr_flagshigh, me->if_flags);
		break;

	case SIOCGIFCAP:
		me->if_reqcap = ifr.ifr_reqcap;
		me->if_curcap = ifr.ifr_curcap;
		D("curcap are 0x%x", me->if_curcap);
		break;
	}
	return 0;
}


/*
 * open a device. if me->mem is null then do an mmap.
 */
static int
netmap_open(struct my_ring *me, int ringid)
{
	int fd, err, l;
	u_int i;
	struct nmreq req;

	me->fd = fd = open("/dev/netmap", O_RDWR);
	if (fd < 0) {
		D("Unable to open /dev/netmap");
		return (-1);
	}
	bzero(&req, sizeof(req));
	strncpy(req.nr_name, me->nmr.nr_name, sizeof(req.nr_name));
	req.nr_ringid = ringid;
	err = ioctl(fd, NIOCGINFO, &req);
	if (err) {
		D("cannot get info on %s", me->nmr.nr_name);
		goto error;
	}
	me->memsize = l = req.nr_memsize;
	ND("memsize is %d MB", l>>20);
	err = ioctl(fd, NIOCREGIF, &req);
	if (err) {
		D("Unable to register %s", me->nmr.nr_name);
		goto error;
	}

	if (me->mem == NULL) {
		me->mem = mmap(0, l, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
		if (me->mem == MAP_FAILED) {
			D("Unable to mmap");
			me->mem = NULL;
			goto error;
		}
	}

	me->nifp = NETMAP_IF(me->mem, req.nr_offset);
	me->queueid = ringid;
	if (ringid & NETMAP_SW_RING) {
		me->begin = req.nr_numrings;
		me->end = me->begin + 1;
	} else if (ringid & NETMAP_HW_RING) {
		me->begin = ringid & NETMAP_RING_MASK;
		me->end = me->begin + 1;
	} else {
		me->begin = 0;
		me->end = req.nr_numrings;
	}
	/* request timestamps for packets */
	for (i = me->begin; i < me->end; i++) {
		struct netmap_ring *ring = NETMAP_RXRING(me->nifp, i);
		ring->flags = NR_TIMESTAMP;
	}
	//me->tx = NETMAP_TXRING(me->nifp, 0);
	return (0);
error:
	close(me->fd);
	return -1;
}

/*
 * There is a set of functions that tcpdump expects even if probably
 * not used
 */
struct eproto eproto_db[] = {
	{ "ip", ETHERTYPE_IP },
	{ "arp", ETHERTYPE_ARP },
	{ (char *)0, 0 }
};


int
pcap_findalldevs(pcap_if_t **alldevsp, __unused char *errbuf)
{
	struct ifaddrs *i_head, *i;
	pcap_if_t *top = NULL, *cur;
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
		bcopy(i->ifa_addr, pca->addr, i->ifa_addr->sa_len);
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
	*alldevsp = top;
	return 0;
}

void pcap_freealldevs(__unused pcap_if_t *alldevs)
{
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
pcap_can_set_rfmon(__unused pcap_t *p)
{
	D("");
	return 0;	/* no we can't */
}

int
pcap_set_snaplen(pcap_t *p, int snaplen)
{
	struct my_ring *me = p;

	D("len %d", snaplen);
	me->snaplen = snaplen;
	return 0;
}

int
pcap_snapshot(pcap_t *p)
{
	struct my_ring *me = p;

	D("len %d", me->snaplen);
	return me->snaplen;
}

int
pcap_lookupnet(const char *device, uint32_t *netp,
	uint32_t *maskp, __unused char *errbuf)
{

	D("device %s", device);
	inet_aton("10.0.0.255", (struct in_addr *)netp);
	inet_aton("255.255.255.0",(struct in_addr *) maskp);
	return 0;
}

int
pcap_set_promisc(pcap_t *p, int promisc)
{
	struct my_ring *me = p;

	D("promisc %d", promisc);
        if (do_ioctl(me, SIOCGIFFLAGS))
		D("SIOCGIFFLAGS failed");
	if (promisc) {
		me->if_flags |= IFF_PPROMISC;
	} else {
		me->if_flags &= ~IFF_PPROMISC;
	}
	if (do_ioctl(me, SIOCSIFFLAGS))
		D("SIOCSIFFLAGS failed");
	return 0;
}

int
pcap_set_timeout(pcap_t *p, int to_ms)
{
	struct my_ring *me = p;

	D("%d ms", to_ms);
	me->to_ms = to_ms;
	return 0;
}

struct bpf_program;

int
pcap_compile(__unused pcap_t *p, __unused struct bpf_program *fp,
	const char *str, __unused int optimize, __unused uint32_t netmask)
{
	D("%s", str);
	return 0;
}

int
pcap_setfilter(__unused pcap_t *p, __unused  struct bpf_program *fp)
{
	D("");
	return 0;
}

int
pcap_datalink(__unused pcap_t *p)
{
	D("");
	return 1;	// ethernet
}

const char *
pcap_datalink_val_to_name(int dlt)
{
	D("%d", dlt);
	return "DLT_EN10MB";
}

const char *
pcap_datalink_val_to_description(int dlt)
{
	D("%d", dlt);
	return "Ethernet link";
}

struct pcap_stat;
int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{
	struct my_ring *me = p;
	ND("");

	me->st.ps_recv += 10;
	*ps = me->st;
	sprintf(me->msg, "stats not supported");
	return -1;
};

char *
pcap_geterr(pcap_t *p)
{
	struct my_ring *me = p;

	D("");
	return me->msg;
}

pcap_t *
pcap_open_live(const char *device, __unused int snaplen,
               int promisc, int to_ms, __unused char *errbuf)
{
	struct my_ring *me;

	D("request to open %s", device);
	me = calloc(1, sizeof(*me));
	if (me == NULL) {
		D("failed to allocate struct for %s", device);
		return NULL;
	}
	strncpy(me->nmr.nr_name, device, sizeof(me->nmr.nr_name));
	if (netmap_open(me, 0)) {
		D("error opening %s", device);
		free(me);
		return NULL;
	}
	me->to_ms = to_ms;
        if (do_ioctl(me, SIOCGIFFLAGS))
		D("SIOCGIFFLAGS failed");
	if (promisc) {
		me->if_flags |= IFF_PPROMISC;
		if (do_ioctl(me, SIOCSIFFLAGS))
			D("SIOCSIFFLAGS failed");
	}
        if (do_ioctl(me, SIOCGIFCAP))
		D("SIOCGIFCAP failed");
        me->if_reqcap &= ~(IFCAP_HWCSUM | IFCAP_TSO | IFCAP_TOE);
        if (do_ioctl(me, SIOCSIFCAP))
		D("SIOCSIFCAP failed");

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
	ioctl(me->fd, NIOCUNREGIF, NULL);
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
pcap_setnonblock(__unused pcap_t *p, int nonblock, __unused char *errbuf)
{
	D("mode is %d", nonblock);
	return 0;	/* ignore */
}

int
pcap_setdirection(__unused pcap_t *p, __unused pcap_direction_t d)
{
	D("");
	return 0;	/* ignore */
};

int
pcap_dispatch(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	struct my_ring *me = p;
	int got = 0;
	u_int si;

	ND("cnt %d", cnt);
	/* scan all rings */
	for (si = me->begin; si < me->end; si++) {
		struct netmap_ring *ring = NETMAP_RXRING(me->nifp, si);
		ND("ring has %d pkts", ring->avail);
		if (ring->avail == 0)
			continue;
		me->hdr.ts = ring->ts;
		while ((cnt == -1 || cnt != got) && ring->avail > 0) {
			u_int i = ring->cur;
			u_int idx = ring->slot[i].buf_idx;
			if (idx < 2) {
				D("%s bogus RX index %d at offset %d",
					me->nifp->ni_name, idx, i);
				sleep(2);
			}
			u_char *buf = (u_char *)NETMAP_BUF(ring, idx);
			me->hdr.len = me->hdr.caplen = ring->slot[i].len;
			// D("call %p len %d", p, me->hdr.len);
			callback(user, &me->hdr, buf);
			ring->cur = NETMAP_RING_NEXT(ring, i);
			ring->avail--;
			got++;
		}
	}
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
		bcopy(buf, dst, size);
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
	struct my_ring *me = p;
	struct pollfd fds[1];
	int i;

	ND("cnt %d", cnt);
	memset(fds, 0, sizeof(fds));
	fds[0].fd = me->fd;
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

#endif /* __PIC__ */

#ifndef __PIC__
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
#endif /* !__PIC__ */
