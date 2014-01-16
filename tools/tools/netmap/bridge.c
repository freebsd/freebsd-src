/*
 * (C) 2011-2014 Luigi Rizzo, Matteo Landi
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
	m = nm_ring_space(rxring);
	if (m < limit)
		limit = m;
	m = nm_ring_space(txring);
	if (m < limit)
		limit = m;
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
		j = nm_ring_next(rxring, j);
		k = nm_ring_next(txring, k);
	}
	rxring->head = rxring->cur = j;
	txring->head = txring->cur = k;
	if (verbose && m > 0)
		D("%s sent %d packets to %p", msg, m, txring);

	return (m);
}

/* move packts from src to destination */
static int
move(struct nm_desc_t *src, struct nm_desc_t *dst, u_int limit)
{
	struct netmap_ring *txring, *rxring;
	u_int m = 0, si = src->first_rx_ring, di = dst->first_tx_ring;
	const char *msg = (src->req.nr_ringid & NETMAP_SW_RING) ?
		"host->net" : "net->host";

	while (si <= src->last_rx_ring && di <= dst->last_tx_ring) {
		rxring = src->tx + si;
		txring = dst->tx + di;
		ND("txring %p rxring %p", txring, rxring);
		if (nm_ring_empty(rxring)) {
			si++;
			continue;
		}
		if (nm_ring_empty(txring)) {
			di++;
			continue;
		}
		m += process_rings(rxring, txring, limit, msg);
	}

	return (m);
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
	struct nm_desc_t *pa = NULL, *pb = NULL;
	char *ifa = NULL, *ifb = NULL;

	fprintf(stderr, "%s %s built %s %s\n",
		argv[0], version, __DATE__, __TIME__);

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
	if (!strcmp(ifa, ifb)) {
		D("same interface, endpoint 0 goes to host");
		i = NETMAP_SW_RING;
	} else {
		/* two different interfaces. Take all rings on if1 */
		i = 0;	// all hw rings
	}
	pa = netmap_open(ifa, i, 1);
	if (pa == NULL)
		return (1);
	// XXX use a single mmap ?
	pb = netmap_open(ifb, 0, 1);
	if (pb == NULL) {
		nm_close(pa);
		return (1);
	}

	/* setup poll(2) variables. */
	memset(pollfd, 0, sizeof(pollfd));
	pollfd[0].fd = pa->fd;
	pollfd[1].fd = pb->fd;

	D("Wait %d secs for link to come up...", wait_link);
	sleep(wait_link);
	D("Ready to go, %s 0x%x/%d <-> %s 0x%x/%d.",
		pa->req.nr_name, pa->first_rx_ring, pa->req.nr_rx_rings,
		pb->req.nr_name, pb->first_rx_ring, pb->req.nr_rx_rings);

	/* main loop */
	signal(SIGINT, sigint_h);
	while (!do_abort) {
		int n0, n1, ret;
		pollfd[0].events = pollfd[1].events = 0;
		pollfd[0].revents = pollfd[1].revents = 0;
		n0 = pkt_queued(pa, 0);
		n1 = pkt_queued(pb, 0);
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
				pkt_queued(pa, 0),
				pa->rx->cur,
				pkt_queued(pa, 1),
				pollfd[1].events,
				pollfd[1].revents,
				pkt_queued(pb, 0),
				pb->rx->cur,
				pkt_queued(pb, 1)
			);
		if (ret < 0)
			continue;
		if (pollfd[0].revents & POLLERR) {
			D("error on fd0, rx [%d,%d)",
				pa->rx->cur, pa->rx->tail);
		}
		if (pollfd[1].revents & POLLERR) {
			D("error on fd1, rx [%d,%d)",
				pb->rx->cur, pb->rx->tail);
		}
		if (pollfd[0].revents & POLLOUT) {
			move(pb, pa, burst);
			// XXX we don't need the ioctl */
			// ioctl(me[0].fd, NIOCTXSYNC, NULL);
		}
		if (pollfd[1].revents & POLLOUT) {
			move(pa, pb, burst);
			// XXX we don't need the ioctl */
			// ioctl(me[1].fd, NIOCTXSYNC, NULL);
		}
	}
	D("exiting");
	nm_close(pb);
	nm_close(pa);

	return (0);
}
