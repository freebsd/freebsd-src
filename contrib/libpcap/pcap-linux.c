/*
 * Copyright (c) 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
static const char rcsid[] =
    "@(#) $Header: pcap-linux.c,v 1.4 96/12/10 23:15:00 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>

#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct ifreq saved_ifr;

#include "pcap-int.h"

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

void linux_restore_ifr(void);

int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{

	*ps = p->md.stat;
	return (0);
}

int
pcap_read(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	register int cc;
	register int bufsize;
	register int caplen;
	register u_char *bp;
	struct sockaddr from;
	int fromlen;

	bp = (char *)p->buffer;
	bufsize = p->bufsize;
	if (p->md.pad > 0) {
		bp += p->md.pad;
		bufsize -= p->md.pad;
		memset(p->buffer, 0, p->md.pad);
	}

again:
	do {
		fromlen = sizeof(from);
		cc = recvfrom(p->fd, bp, bufsize, 0, &from, &fromlen);
		if (cc < 0) {
			/* Don't choke when we get ptraced */
			switch (errno) {

			case EINTR:
					goto again;

			case EWOULDBLOCK:
				return (0);		/* XXX */
			}
			sprintf(p->errbuf, "read: %s", pcap_strerror(errno));
			return (-1);
		}
	} while (strcmp(p->md.device, from.sa_data));

	/* If we need have leading zero bytes, adjust count */
	cc += p->md.pad;
	bp = p->buffer;

	/* If we need to step over leading junk, adjust count and pointer */
	cc -= p->md.skip;
	bp += p->md.skip;

	/* Captured length can't exceed our read buffer size */
	caplen = cc;
	if (caplen > bufsize)
		caplen = bufsize;

	/* Captured length can't exceed the snapshot length */
	if (caplen > p->snapshot)
		caplen = p->snapshot;

	if (p->fcode.bf_insns == NULL ||
	    bpf_filter(p->fcode.bf_insns, bp, cc, caplen)) {
		struct pcap_pkthdr h;

		++p->md.stat.ps_recv;
		/* Get timestamp */
		if (ioctl(p->fd, SIOCGSTAMP, &h.ts) < 0) {
			sprintf(p->errbuf, "SIOCGSTAMP: %s",
			    pcap_strerror(errno));
			return (-1);
		}
		h.len = cc;
		h.caplen = caplen;
		(*callback)(user, &h, bp);
		return (1);
	}
	return (0);
}

pcap_t *
pcap_open_live(char *device, int snaplen, int promisc, int to_ms, char *ebuf)
{
	register int fd, broadcast;
	register pcap_t *p;
	struct ifreq ifr;

	p = (pcap_t *)malloc(sizeof(*p));
	if (p == NULL) {
		sprintf(ebuf, "malloc: %s", pcap_strerror(errno));
		return (NULL);
	}
	memset((char *)p, 0, sizeof(*p));
	fd = -1;

	/* XXX hack - map device name to link layer type */
	broadcast = 0;
	if (strncmp("eth", device, 3) == 0) {
		p->linktype = DLT_EN10MB;
		++broadcast;
	} else if (strncmp("sl", device, 2) == 0) {
		p->linktype = DLT_SLIP;
		p->md.pad = 16;
	} else if (strncmp("ppp", device, 3) == 0) {
		p->linktype = DLT_PPP;
		p->md.pad = 4;
	} else if (strncmp("lo", device, 2) == 0) {
		p->linktype = DLT_NULL;
		p->md.pad = 2;
		p->md.skip = 12;
	} else {
		sprintf(ebuf, "linux: unknown physical layer type");
		goto bad;
	}

	/* Linux is full of magic numbers */
	fd = socket(PF_INET, SOCK_PACKET, htons(0x0003));
	if (fd < 0) {
		sprintf(ebuf, "linux socket: %s", pcap_strerror(errno));
		goto bad;
	}
	p->fd = fd;


	p->bufsize = 4096;				/* XXX */
	p->buffer = (u_char *)malloc(p->bufsize);
	if (p->buffer == NULL) {
		sprintf(ebuf, "malloc: %s", pcap_strerror(errno));
		goto bad;
	}

	/* XXX */
	if (promisc && broadcast) {
		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, device);
		if (ioctl(p->fd, SIOCGIFFLAGS, &ifr) < 0 ) {
			sprintf(ebuf, "SIOCGIFFLAGS: %s", pcap_strerror(errno));
			goto bad;
		}
		saved_ifr = ifr;
		ifr.ifr_flags |= IFF_PROMISC;
		if (ioctl(p->fd, SIOCSIFFLAGS, &ifr) < 0 ) {
			sprintf(ebuf, "SIOCSIFFLAGS: %s", pcap_strerror(errno));
			goto bad;
		}
		ifr.ifr_flags &= ~IFF_PROMISC;
		atexit(linux_restore_ifr);
	}

	p->md.device = strdup(device);
	if (p->md.device == NULL) {
		sprintf(ebuf, "malloc: %s", pcap_strerror(errno));
		goto bad;
	}
	p->snapshot = snaplen;

	return (p);
bad:
	if (fd >= 0)
		(void)close(fd);
	if (p->buffer != NULL)
		free(p->buffer);
	if (p->md.device != NULL)
		free(p->md.device);
	free(p);
	return (NULL);
}

int
pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{

	p->fcode = *fp;
	return (0);
}

void
linux_restore_ifr(void)
{
	register int fd;

	fd = socket(PF_INET, SOCK_PACKET, htons(0x0003));
	if (fd < 0)
		fprintf(stderr, "linux socket: %s", pcap_strerror(errno));
	else if (ioctl(fd, SIOCSIFFLAGS, &saved_ifr) < 0)
		fprintf(stderr, "linux SIOCSIFFLAGS: %s", pcap_strerror(errno));
}
