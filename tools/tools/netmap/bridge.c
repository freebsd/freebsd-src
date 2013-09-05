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

#include "nm_util.h"


int verbose = 0;

char *version = "$Id$";

static int do_abort = 0;

static void
sigint_h(int sig)
{
	(void)sig;	/* UNUSED */
	do_abort = 1;
	signal(SIGINT, SIG_DFL);
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
#ifdef NO_SWAP
		char *rxbuf = NETMAP_BUF(rxring, rs->buf_idx);
		char *txbuf = NETMAP_BUF(txring, ts->buf_idx);
#else
		uint32_t pkt;
#endif

		/* swap packets */
		if (ts->buf_idx < 2 || rs->buf_idx < 2) {
			D("wrong index rx[%d] = %d  -> tx[%d] = %d",
				j, rs->buf_idx, k, ts->buf_idx);
			sleep(2);
		}
#ifndef NO_SWAP
		pkt = ts->buf_idx;
		ts->buf_idx = rs->buf_idx;
		rs->buf_idx = pkt;
#endif
		/* copy the packet length. */
		if (rs->len < 14 || rs->len > 2048)
			D("wrong len %d rx[%d] -> tx[%d]", rs->len, j, k);
		else if (verbose > 1)
			D("%s send len %d rx[%d] -> tx[%d]", msg, rs->len, j, k);
		ts->len = rs->len;
#ifdef NO_SWAP
		pkt_copy(rxbuf, txbuf, ts->len);
#else
		/* report the buffer change. */
		ts->flags |= NS_BUF_CHANGED;
		rs->flags |= NS_BUF_CHANGED;
#endif /* NO_SWAP */
		j = NETMAP_RING_NEXT(rxring, j);
		k = NETMAP_RING_NEXT(txring, k);
	}
	rxring->avail -= m;
	txring->avail -= m;
	rxring->cur = j;
	txring->cur = k;
	if (verbose && m > 0)
		D("%s sent %d packets to %p", msg, m, txring);

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
pkt_queued(struct my_ring *me, int tx)
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
			me->end >= me->nifp->ni_tx_rings ? // XXX who comes first ?
				"host":"net",
			tot, NETMAP_TXRING(me->nifp, me->begin)->cur);
	return tot;
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: bridge [-v] [-i ifa] [-i ifb] [-b burst] [-w wait_time] [iface]\n");
	exit(1);
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
	int i, ch;
	u_int burst = 1024, wait_link = 4;
	struct my_ring me[2];
	char *ifa = NULL, *ifb = NULL;

	fprintf(stderr, "%s %s built %s %s\n",
		argv[0], version, __DATE__, __TIME__);

	bzero(me, sizeof(me));

	while ( (ch = getopt(argc, argv, "b:i:vw:")) != -1) {
		switch (ch) {
		default:
			D("bad option %c %s", ch, optarg);
			usage();
			break;
		case 'b':	/* burst */
			burst = atoi(optarg);
			break;
		case 'i':	/* interface */
			if (ifa == NULL)
				ifa = optarg;
			else if (ifb == NULL)
				ifb = optarg;
			else
				D("%s ignored, already have 2 interfaces",
					optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			wait_link = atoi(optarg);
			break;
		}

	}

	argc -= optind;
	argv += optind;

	if (argc > 1)
		ifa = argv[1];
	if (argc > 2)
		ifb = argv[2];
	if (argc > 3)
		burst = atoi(argv[3]);
	if (!ifb)
		ifb = ifa;
	if (!ifa) {
		D("missing interface");
		usage();
	}
	if (burst < 1 || burst > 8192) {
		D("invalid burst %d, set to 1024", burst);
		burst = 1024;
	}
	if (wait_link > 100) {
		D("invalid wait_link %d, set to 4", wait_link);
		wait_link = 4;
	}
	/* setup netmap interface #1. */
	me[0].ifname = ifa;
	me[1].ifname = ifb;
	if (!strcmp(ifa, ifb)) {
		D("same interface, endpoint 0 goes to host");
		i = NETMAP_SW_RING;
	} else {
		/* two different interfaces. Take all rings on if1 */
		i = 0;	// all hw rings
	}
	if (netmap_open(me, i, 1))
		return (1);
	me[1].mem = me[0].mem; /* copy the pointer, so only one mmap */
	if (netmap_open(me+1, 0, 1))
		return (1);

	/* setup poll(2) variables. */
	memset(pollfd, 0, sizeof(pollfd));
	for (i = 0; i < 2; i++) {
		pollfd[i].fd = me[i].fd;
		pollfd[i].events = (POLLIN);
	}

	D("Wait %d secs for link to come up...", wait_link);
	sleep(wait_link);
	D("Ready to go, %s 0x%x/%d <-> %s 0x%x/%d.",
		me[0].ifname, me[0].queueid, me[0].nifp->ni_rx_rings,
		me[1].ifname, me[1].queueid, me[1].nifp->ni_rx_rings);

	/* main loop */
	signal(SIGINT, sigint_h);
	while (!do_abort) {
		int n0, n1, ret;
		pollfd[0].events = pollfd[1].events = 0;
		pollfd[0].revents = pollfd[1].revents = 0;
		n0 = pkt_queued(me, 0);
		n1 = pkt_queued(me + 1, 0);
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
				pkt_queued(me, 0),
				me[0].rx->cur,
				pkt_queued(me, 1),
				pollfd[1].events,
				pollfd[1].revents,
				pkt_queued(me+1, 0),
				me[1].rx->cur,
				pkt_queued(me+1, 1)
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
