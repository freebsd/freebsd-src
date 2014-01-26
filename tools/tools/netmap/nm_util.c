/*
 * Copyright (C) 2012-2014 Luigi Rizzo. All rights reserved.
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
 * utilities to use netmap devices.
 * This does the basic functions of opening a device and issuing
 * ioctls()
 */

#include "nm_util.h"

extern int verbose;

int
nm_do_ioctl(struct nm_desc_t *me, u_long what, int subcmd)
{
	struct ifreq ifr;
	int error;
	int fd;

#if defined( __FreeBSD__ ) || defined (__APPLE__)
	(void)subcmd;	// only used on Linux
	fd = me->fd;
#endif

#ifdef linux 
	struct ethtool_value eval;

	bzero(&eval, sizeof(eval));
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		printf("Error: cannot get device control socket.\n");
		return -1;
	}
#endif /* linux */

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, me->req.nr_name, sizeof(ifr.ifr_name));
	switch (what) {
	case SIOCSIFFLAGS:
#ifndef __APPLE__
		ifr.ifr_flagshigh = me->if_flags >> 16;
#endif
		ifr.ifr_flags = me->if_flags & 0xffff;
		break;

#if defined( __FreeBSD__ )
	case SIOCSIFCAP:
		ifr.ifr_reqcap = me->if_reqcap;
		ifr.ifr_curcap = me->if_curcap;
		break;
#endif

#ifdef linux
	case SIOCETHTOOL:
		eval.cmd = subcmd;
		eval.data = 0;
		ifr.ifr_data = (caddr_t)&eval;
		break;
#endif /* linux */
	}
	error = ioctl(fd, what, &ifr);
	if (error)
		goto done;
	switch (what) {
	case SIOCGIFFLAGS:
#ifndef __APPLE__
		me->if_flags = (ifr.ifr_flagshigh << 16) |
			(0xffff & ifr.ifr_flags);
#endif
		if (verbose)
			D("flags are 0x%x", me->if_flags);
		break;

#if defined( __FreeBSD__ )
	case SIOCGIFCAP:
		me->if_reqcap = ifr.ifr_reqcap;
		me->if_curcap = ifr.ifr_curcap;
		if (verbose)
			D("curcap are 0x%x", me->if_curcap);
		break;
#endif /* __FreeBSD__ */
	}
done:
#ifdef linux
	close(fd);
#endif
	if (error)
		D("ioctl error %d %lu", error, what);
	return error;
}

/*
 * open a device. if me->mem is null then do an mmap.
 * Returns the file descriptor.
 * The extra flag checks configures promisc mode.
 */
struct nm_desc_t *
netmap_open(const char *name, int ringid, int promisc)
{
	struct nm_desc_t *d = nm_open(name, NULL, ringid, 0);

	if (d == NULL)
		return d;

	if (verbose)
		D("memsize is %d MB", d->req.nr_memsize>>20);

	/* Set the operating mode. */
	if (ringid != NETMAP_SW_RING) {
		nm_do_ioctl(d, SIOCGIFFLAGS, 0);
		if ((d->if_flags & IFF_UP) == 0) {
			D("%s is down, bringing up...", name);
			d->if_flags |= IFF_UP;
		}
		if (promisc) {
			d->if_flags |= IFF_PPROMISC;
			nm_do_ioctl(d, SIOCSIFFLAGS, 0);
		}

		/* disable GSO, TSO, RXCSUM, TXCSUM...
		 * TODO: set them back when done.
		 */
#ifdef __FreeBSD__
		nm_do_ioctl(d, SIOCGIFCAP, 0);
		d->if_reqcap = d->if_curcap;
		d->if_reqcap &= ~(IFCAP_HWCSUM | IFCAP_TSO | IFCAP_TOE);
		nm_do_ioctl(d, SIOCSIFCAP, 0);
#endif
#ifdef linux
		nm_do_ioctl(d, SIOCETHTOOL, ETHTOOL_SGSO);
		nm_do_ioctl(d, SIOCETHTOOL, ETHTOOL_STSO);
		nm_do_ioctl(d, SIOCETHTOOL, ETHTOOL_SRXCSUM);
		nm_do_ioctl(d, SIOCETHTOOL, ETHTOOL_STXCSUM);
#endif /* linux */
	}

	return d;
}


/*
 * how many packets on this set of queues ?
 */
int
pkt_queued(struct nm_desc_t *d, int tx)
{
	u_int i, tot = 0;

	ND("me %p begin %d end %d", me, me->begin, me->end);
	if (tx) {
		for (i = d->first_tx_ring; i <= d->last_tx_ring; i++)
			tot += nm_ring_space(d->tx + i);
	} else {
		for (i = d->first_rx_ring; i <= d->last_rx_ring; i++)
			tot += nm_ring_space(d->rx + i);
	}
	return tot;
}

#if 0

/*
 *

Helper routines for multiple readers from the same queue

- all readers open the device in 'passive' mode (NETMAP_PRIV_RING set).
  In this mode a thread that loses the race on a poll() just continues
  without calling *xsync()

- all readers share an extra 'ring' which contains the sync information.
  In particular we have a shared head+tail pointers that work
  together with cur and available
  ON RETURN FROM THE SYSCALL:
  shadow->cur = ring->cur
  shadow->tail = ring->tail
  shadow->link[i] = i for all slots // mark invalid
 
 */

struct nm_q_arg {
	u_int want;	/* Input */
	u_int have;	/* Output, 0 on error */
	u_int cur;
	u_int tail;
	struct netmap_ring *ring;
};

/*
 * grab a number of slots from the queue.
 */
struct nm_q_arg
my_grab(struct nm_q_arg q)
{
	const u_int ns = q.ring->num_slots;

	// lock(ring);
	for (;;) {

		q.cur = (volatile u_int)q.ring->head;
		q.have = ns + q.head - (volatile u_int)q.ring->tail;
		if (q.have >= ns)
			q.have -= ns;
		if (q.have == 0) /* no space; caller may ioctl/retry */
			break;
		if (q.want < q.have)
			q.have = q.want;
		q.tail = q.cur + q.have;
		if (q.tail >= ns)
			q.tail -= ns;
		if (atomic_cmpset_int(&q.ring->cur, q.cur, q.tail)
			break; /* success */
	}
	// unlock(ring);
	D("returns %d out of %d at %d,%d",
		q.have, q.want, q.cur, q.tail);
	/* the last one can clear avail ? */
	return q;
}


int
my_release(struct nm_q_arg q)
{
	u_int cur = q.cur, tail = q.tail, i;
	struct netmap_ring *r = q.ring;

	/* link the block to the next one.
	 * there is no race here because the location is mine.
	 */
	r->slot[cur].ptr = tail; /* this is mine */
	r->slot[cur].flags |= NM_SLOT_PTR;	// points to next block
	// memory barrier
	// lock(ring);
	if (r->head != cur)
		goto done;
	for (;;) {
		// advance head
		r->head = head = r->slot[head].ptr;
		// barrier ?
		if (head == r->slot[head].ptr)
			break; // stop here
	}
	/* we have advanced from q.head to head (r.head might be
	 * further down.
	 */
	// do an ioctl/poll to flush.
done:
	// unlock(ring);
	return; /* not my turn to release */
}
#endif /* unused */
