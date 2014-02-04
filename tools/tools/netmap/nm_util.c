/*
 * Copyright (C) 2012-2013 Luigi Rizzo. All rights reserved.
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
nm_do_ioctl(struct my_ring *me, u_long what, int subcmd)
{
	struct ifreq ifr;
	int error;
#if defined( __FreeBSD__ ) || defined (__APPLE__)
	int fd = me->fd;
#endif
#ifdef linux 
	struct ethtool_value eval;
	int fd;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		printf("Error: cannot get device control socket.\n");
		return -1;
	}
#endif /* linux */

	(void)subcmd;	// unused
	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, me->ifname, sizeof(ifr.ifr_name));
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
int
netmap_open(struct my_ring *me, int ringid, int promisc)
{
	int fd, err, l;
	struct nmreq req;

	me->fd = fd = open("/dev/netmap", O_RDWR);
	if (fd < 0) {
		D("Unable to open /dev/netmap");
		return (-1);
	}
	bzero(&req, sizeof(req));
	req.nr_version = NETMAP_API;
	strncpy(req.nr_name, me->ifname, sizeof(req.nr_name));
	req.nr_ringid = ringid;
	err = ioctl(fd, NIOCREGIF, &req);
	if (err) {
		D("Unable to register %s", me->ifname);
		goto error;
	}
	me->memsize = l = req.nr_memsize;
	if (verbose)
		D("memsize is %d MB", l>>20);

	if (me->mem == NULL) {
		me->mem = mmap(0, l, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
		if (me->mem == MAP_FAILED) {
			D("Unable to mmap");
			me->mem = NULL;
			goto error;
		}
	}


	/* Set the operating mode. */
	if (ringid != NETMAP_SW_RING) {
		nm_do_ioctl(me, SIOCGIFFLAGS, 0);
		if ((me[0].if_flags & IFF_UP) == 0) {
			D("%s is down, bringing up...", me[0].ifname);
			me[0].if_flags |= IFF_UP;
		}
		if (promisc) {
			me[0].if_flags |= IFF_PPROMISC;
			nm_do_ioctl(me, SIOCSIFFLAGS, 0);
		}

#ifdef __FreeBSD__
		/* also disable checksums etc. */
		nm_do_ioctl(me, SIOCGIFCAP, 0);
		me[0].if_reqcap = me[0].if_curcap;
		me[0].if_reqcap &= ~(IFCAP_HWCSUM | IFCAP_TSO | IFCAP_TOE);
		nm_do_ioctl(me+0, SIOCSIFCAP, 0);
#endif
#ifdef linux
		/* disable:
		 * - generic-segmentation-offload
		 * - tcp-segmentation-offload
		 * - rx-checksumming
		 * - tx-checksumming
		 * XXX check how to set back the caps.
		 */
		nm_do_ioctl(me, SIOCETHTOOL, ETHTOOL_SGSO);
		nm_do_ioctl(me, SIOCETHTOOL, ETHTOOL_STSO);
		nm_do_ioctl(me, SIOCETHTOOL, ETHTOOL_SRXCSUM);
		nm_do_ioctl(me, SIOCETHTOOL, ETHTOOL_STXCSUM);
#endif /* linux */
	}

	me->nifp = NETMAP_IF(me->mem, req.nr_offset);
	me->queueid = ringid;
	if (ringid & NETMAP_SW_RING) {
		me->begin = req.nr_rx_rings;
		me->end = me->begin + 1;
		me->tx = NETMAP_TXRING(me->nifp, req.nr_tx_rings);
		me->rx = NETMAP_RXRING(me->nifp, req.nr_rx_rings);
	} else if (ringid & NETMAP_HW_RING) {
		D("XXX check multiple threads");
		me->begin = ringid & NETMAP_RING_MASK;
		me->end = me->begin + 1;
		me->tx = NETMAP_TXRING(me->nifp, me->begin);
		me->rx = NETMAP_RXRING(me->nifp, me->begin);
	} else {
		me->begin = 0;
		me->end = req.nr_rx_rings; // XXX max of the two
		me->tx = NETMAP_TXRING(me->nifp, 0);
		me->rx = NETMAP_RXRING(me->nifp, 0);
	}
	return (0);
error:
	close(me->fd);
	return -1;
}


int
netmap_close(struct my_ring *me)
{
	D("");
	if (me->mem)
		munmap(me->mem, me->memsize);
	close(me->fd);
	return (0);
}


/*
 * how many packets on this set of queues ?
 */
int
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
