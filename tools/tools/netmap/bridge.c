/*
 * (C) 2011 Luigi Rizzo, Matteo Landi
 *
 * BSD license
 *
 * A netmap client to bridge two network interfaces
 * (or one interface and the host stack).
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

#define MIN(a, b) ((a) < (b) ? (a) : (b))

int verbose = 0;

/* debug support */
#define ND(format, ...) {}
#define D(format, ...) do {					\
	if (!verbose) break;					\
	struct timeval _xxts;					\
	gettimeofday(&_xxts, NULL);				\
        fprintf(stderr, "%03d.%06d %s [%d] " format "\n",	\
	(int)_xxts.tv_sec %1000, (int)_xxts.tv_usec,		\
        __FUNCTION__, __LINE__, ##__VA_ARGS__);			\
	} while (0)


char *version = "$Id: bridge.c 9642 2011-11-07 21:39:47Z luigi $";

static int do_abort = 0;

/*
 * info on a ring we handle
 */
struct my_ring {
	const char *ifname;
	int fd;
	char *mem;			/* userspace mmap address */
	u_int memsize;
	u_int queueid;
	u_int begin, end;		/* first..last+1 rings to check */
	struct netmap_if *nifp;
	struct netmap_ring *tx, *rx;	/* shortcuts */

	uint32_t if_flags;
	uint32_t if_reqcap;
	uint32_t if_curcap;
};

static void
sigint_h(__unused int sig)
{
	do_abort = 1;
	signal(SIGINT, SIG_DFL);
}


static int
do_ioctl(struct my_ring *me, int what)
{
	struct ifreq ifr;
	int error;

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, me->ifname, sizeof(ifr.ifr_name));
	switch (what) {
	case SIOCSIFFLAGS:
		ifr.ifr_flagshigh = me->if_flags >> 16;
		ifr.ifr_flags = me->if_flags & 0xffff;
		break;
	case SIOCSIFCAP:
		ifr.ifr_reqcap = me->if_reqcap;
		ifr.ifr_curcap = me->if_curcap;
		break;
	}
	error = ioctl(me->fd, what, &ifr);
	if (error) {
		D("ioctl error %d", what);
		return error;
	}
	switch (what) {
	case SIOCGIFFLAGS:
		me->if_flags = (ifr.ifr_flagshigh << 16) |
			(0xffff & ifr.ifr_flags);
		if (verbose)
			D("flags are 0x%x", me->if_flags);
		break;

	case SIOCGIFCAP:
		me->if_reqcap = ifr.ifr_reqcap;
		me->if_curcap = ifr.ifr_curcap;
		if (verbose)
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
	struct nmreq req;

	me->fd = fd = open("/dev/netmap", O_RDWR);
	if (fd < 0) {
		D("Unable to open /dev/netmap");
		return (-1);
	}
	bzero(&req, sizeof(req));
	strncpy(req.nr_name, me->ifname, sizeof(req.nr_name));
	req.nr_ringid = ringid;
	err = ioctl(fd, NIOCGINFO, &req);
	if (err) {
		D("cannot get info on %s", me->ifname);
		goto error;
	}
	me->memsize = l = req.nr_memsize;
	if (verbose)
		D("memsize is %d MB", l>>20);
	err = ioctl(fd, NIOCREGIF, &req);
	if (err) {
		D("Unable to register %s", me->ifname);
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
	me->tx = NETMAP_TXRING(me->nifp, me->begin);
	me->rx = NETMAP_RXRING(me->nifp, me->begin);
	return (0);
error:
	close(me->fd);
	return -1;
}


static int
netmap_close(struct my_ring *me)
{
	D("");
	if (me->mem)
		munmap(me->mem, me->memsize);
	ioctl(me->fd, NIOCUNREGIF, NULL);
	close(me->fd);
	return (0);
}


/*
 * move up to 'limit' pkts from rxring to txring swapping buffers.
 */
static int
process_rings(struct netmap_ring *rxring, struct netmap_ring *txring,
	      u_int limit, const char *msg)
{
	u_int j, k, m = 0;

	/* print a warning if any of the ring flags is set (e.g. NM_REINIT) */
	if (rxring->flags || txring->flags)
		D("%s rxflags %x txflags %x",
			msg, rxring->flags, txring->flags);
	j = rxring->cur; /* RX */
	k = txring->cur; /* TX */
	if (rxring->avail < limit)
		limit = rxring->avail;
	if (txring->avail < limit)
		limit = txring->avail;
	m = limit;
	while (limit-- > 0) {
		struct netmap_slot *rs = &rxring->slot[j];
		struct netmap_slot *ts = &txring->slot[k];
		uint32_t pkt;

		/* swap packets */
		if (ts->buf_idx < 2 || rs->buf_idx < 2) {
			D("wrong index rx[%d] = %d  -> tx[%d] = %d",
				j, rs->buf_idx, k, ts->buf_idx);
			sleep(2);
		}
		pkt = ts->buf_idx;
		ts->buf_idx = rs->buf_idx;
		rs->buf_idx = pkt;

		/* copy the packet lenght. */
		if (rs->len < 14 || rs->len > 2048)
			D("wrong len %d rx[%d] -> tx[%d]", rs->len, j, k);
		else if (verbose > 1)
			D("send len %d rx[%d] -> tx[%d]", rs->len, j, k);
		ts->len = rs->len;

		/* report the buffer change. */
		ts->flags |= NS_BUF_CHANGED;
		rs->flags |= NS_BUF_CHANGED;
		j = NETMAP_RING_NEXT(rxring, j);
		k = NETMAP_RING_NEXT(txring, k);
	}
	rxring->avail -= m;
	txring->avail -= m;
	rxring->cur = j;
	txring->cur = k;
	if (verbose && m > 0)
		D("sent %d packets to %p", m, txring);

	return (m);
}

/* move packts from src to destination */
static int
move(struct my_ring *src, struct my_ring *dst, u_int limit)
{
	struct netmap_ring *txring, *rxring;
	u_int m = 0, si = src->begin, di = dst->begin;
	const char *msg = (src->queueid & NETMAP_SW_RING) ?
		"host->net" : "net->host";

	while (si < src->end && di < dst->end) {
		rxring = NETMAP_RXRING(src->nifp, si);
		txring = NETMAP_TXRING(dst->nifp, di);
		ND("txring %p rxring %p", txring, rxring);
		if (rxring->avail == 0) {
			si++;
			continue;
		}
		if (txring->avail == 0) {
			di++;
			continue;
		}
		m += process_rings(rxring, txring, limit, msg);
	}

	return (m);
}

/*
 * how many packets on this set of queues ?
 */
static int
howmany(struct my_ring *me, int tx)
{
	u_int i, tot = 0;

	ND("me %p begin %d end %d", me, me->begin, me->end);
	for (i = me->begin; i < me->end; i++) {
		struct netmap_ring *ring = tx ?
			NETMAP_TXRING(me->nifp, i) : NETMAP_RXRING(me->nifp, i);
		tot += ring->avail;
	}
	if (0 && verbose && tot && !tx)
		D("ring %s %s %s has %d avail at %d",
			me->ifname, tx ? "tx": "rx",
			me->end > me->nifp->ni_num_queues ?
				"host":"net",
			tot, NETMAP_TXRING(me->nifp, me->begin)->cur);
	return tot;
}

/*
 * bridge [-v] if1 [if2]
 *
 * If only one name, or the two interfaces are the same,
 * bridges userland and the adapter. Otherwise bridge
 * two intefaces.
 */
int
main(int argc, char **argv)
{
	struct pollfd pollfd[2];
	int i;
	u_int burst = 1024;
	struct my_ring me[2];

	fprintf(stderr, "%s %s built %s %s\n",
		argv[0], version, __DATE__, __TIME__);

	bzero(me, sizeof(me));

	while (argc > 1 && !strcmp(argv[1], "-v")) {
		verbose++;
		argv++;
		argc--;
	}

	if (argc < 2 || argc > 4) {
		D("Usage: %s IFNAME1 [IFNAME2 [BURST]]", argv[0]);
		return (1);
	}

	/* setup netmap interface #1. */
	me[0].ifname = argv[1];
	if (argc == 2 || !strcmp(argv[1], argv[2])) {
		D("same interface, endpoint 0 goes to host");
		i = NETMAP_SW_RING;
		me[1].ifname = argv[1];
	} else {
		/* two different interfaces. Take all rings on if1 */
		i = 0;	// all hw rings
		me[1].ifname = argv[2];
	}
	if (netmap_open(me, i))
		return (1);
	me[1].mem = me[0].mem; /* copy the pointer, so only one mmap */
	if (netmap_open(me+1, 0))
		return (1);

	/* if bridging two interfaces, set promisc mode */
	if (i != NETMAP_SW_RING) {
		do_ioctl(me, SIOCGIFFLAGS);
		if ((me[0].if_flags & IFF_UP) == 0) {
			D("%s is down, bringing up...", me[0].ifname);
			me[0].if_flags |= IFF_UP;
		}
		me[0].if_flags |= IFF_PPROMISC;
		do_ioctl(me, SIOCSIFFLAGS);

		do_ioctl(me+1, SIOCGIFFLAGS);
		me[1].if_flags |= IFF_PPROMISC;
		do_ioctl(me+1, SIOCSIFFLAGS);

		/* also disable checksums etc. */
		do_ioctl(me, SIOCGIFCAP);
		me[0].if_reqcap = me[0].if_curcap;
		me[0].if_reqcap &= ~(IFCAP_HWCSUM | IFCAP_TSO | IFCAP_TOE);
		do_ioctl(me+0, SIOCSIFCAP);
	}
	do_ioctl(me+1, SIOCGIFFLAGS);
	if ((me[1].if_flags & IFF_UP) == 0) {
		D("%s is down, bringing up...", me[1].ifname);
		me[1].if_flags |= IFF_UP;
	}
	do_ioctl(me+1, SIOCSIFFLAGS);

	do_ioctl(me+1, SIOCGIFCAP);
	me[1].if_reqcap = me[1].if_curcap;
	me[1].if_reqcap &= ~(IFCAP_HWCSUM | IFCAP_TSO | IFCAP_TOE);
	do_ioctl(me+1, SIOCSIFCAP);
	if (argc > 3)
		burst = atoi(argv[3]);	/* packets burst size. */

	/* setup poll(2) variables. */
	memset(pollfd, 0, sizeof(pollfd));
	for (i = 0; i < 2; i++) {
		pollfd[i].fd = me[i].fd;
		pollfd[i].events = (POLLIN);
	}

	D("Wait 2 secs for link to come up...");
	sleep(2);
	D("Ready to go, %s 0x%x/%d <-> %s 0x%x/%d.",
		me[0].ifname, me[0].queueid, me[0].nifp->ni_num_queues,
		me[1].ifname, me[1].queueid, me[1].nifp->ni_num_queues);

	/* main loop */
	signal(SIGINT, sigint_h);
	while (!do_abort) {
		int n0, n1, ret;
		pollfd[0].events = pollfd[1].events = 0;
		pollfd[0].revents = pollfd[1].revents = 0;
		n0 = howmany(me, 0);
		n1 = howmany(me + 1, 0);
		if (n0)
			pollfd[1].events |= POLLOUT;
		else
			pollfd[0].events |= POLLIN;
		if (n1)
			pollfd[0].events |= POLLOUT;
		else
			pollfd[1].events |= POLLIN;
		ret = poll(pollfd, 2, 2500);
		if (ret <= 0 || verbose)
		    D("poll %s [0] ev %x %x rx %d@%d tx %d,"
			     " [1] ev %x %x rx %d@%d tx %d",
				ret <= 0 ? "timeout" : "ok",
				pollfd[0].events,
				pollfd[0].revents,
				howmany(me, 0),
				me[0].rx->cur,
				howmany(me, 1),
				pollfd[1].events,
				pollfd[1].revents,
				howmany(me+1, 0),
				me[1].rx->cur,
				howmany(me+1, 1)
			);
		if (ret < 0)
			continue;
		if (pollfd[0].revents & POLLERR) {
			D("error on fd0, rxcur %d@%d",
				me[0].rx->avail, me[0].rx->cur);
		}
		if (pollfd[1].revents & POLLERR) {
			D("error on fd1, rxcur %d@%d",
				me[1].rx->avail, me[1].rx->cur);
		}
		if (pollfd[0].revents & POLLOUT) {
			move(me + 1, me, burst);
			// XXX we don't need the ioctl */
			// ioctl(me[0].fd, NIOCTXSYNC, NULL);
		}
		if (pollfd[1].revents & POLLOUT) {
			move(me, me + 1, burst);
			// XXX we don't need the ioctl */
			// ioctl(me[1].fd, NIOCTXSYNC, NULL);
		}
	}
	D("exiting");
	netmap_close(me + 1);
	netmap_close(me + 0);

	return (0);
}
