/*	$NetBSD: net.c,v 1.20 1997/12/26 22:41:30 scottr Exp $	*/

/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Header: net.c,v 1.9 93/08/06 19:32:15 leres Exp  (LBL)
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <string.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>

#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include "stand.h"
#include "net.h"

/* Caller must leave room for ethernet, ip and udp headers in front!! */
ssize_t
sendudp(d, pkt, len)
	register struct iodesc *d;
	register void *pkt;
	register size_t len;
{
	register ssize_t cc;
	register struct ip *ip;
	register struct udphdr *uh;
	register u_char *ea;

#ifdef NET_DEBUG
 	if (debug) {
		printf("sendudp: d=%lx called.\n", (long)d);
		if (d) {
			printf("saddr: %s:%d",
			    inet_ntoa(d->myip), ntohs(d->myport));
			printf(" daddr: %s:%d\n",
			    inet_ntoa(d->destip), ntohs(d->destport));
		}
	}
#endif

	uh = (struct udphdr *)pkt - 1;
	ip = (struct ip *)uh - 1;
	len += sizeof(*ip) + sizeof(*uh);

	bzero(ip, sizeof(*ip) + sizeof(*uh));

	ip->ip_v = IPVERSION;			/* half-char */
	ip->ip_hl = sizeof(*ip) >> 2;		/* half-char */
	ip->ip_len = htons(len);
	ip->ip_p = IPPROTO_UDP;			/* char */
	ip->ip_ttl = IP_TTL;			/* char */
	ip->ip_src = d->myip;
	ip->ip_dst = d->destip;
	ip->ip_sum = in_cksum(ip, sizeof(*ip));	 /* short, but special */

	uh->uh_sport = d->myport;
	uh->uh_dport = d->destport;
	uh->uh_ulen = htons(len - sizeof(*ip));

#ifndef UDP_NO_CKSUM
	{
		register struct udpiphdr *ui;
		struct ip tip;

		/* Calculate checksum (must save and restore ip header) */
		tip = *ip;
		ui = (struct udpiphdr *)ip;
		bzero(&ui->ui_x1, sizeof(ui->ui_x1));
		ui->ui_len = uh->uh_ulen;
		uh->uh_sum = in_cksum(ui, len);
		*ip = tip;
	}
#endif

	if (ip->ip_dst.s_addr == INADDR_BROADCAST || ip->ip_src.s_addr == 0 ||
	    netmask == 0 || SAMENET(ip->ip_src, ip->ip_dst, netmask))
		ea = arpwhohas(d, ip->ip_dst);
	else
		ea = arpwhohas(d, gateip);

	cc = sendether(d, ip, len, ea, ETHERTYPE_IP);
	if (cc == -1)
		return (-1);
	if (cc != len)
		panic("sendudp: bad write (%d != %d)", cc, len);
	return (cc - (sizeof(*ip) + sizeof(*uh)));
}

/*
 * Receive a UDP packet and validate it is for us.
 * Caller leaves room for the headers (Ether, IP, UDP)
 */
ssize_t
readudp(d, pkt, len, tleft)
	register struct iodesc *d;
	register void *pkt;
	register size_t len;
	time_t tleft;
{
	register ssize_t n;
	register size_t hlen;
	register struct ip *ip;
	register struct udphdr *uh;
	u_int16_t etype;	/* host order */

#ifdef NET_DEBUG
	if (debug)
		printf("readudp: called\n");
#endif

	uh = (struct udphdr *)pkt - 1;
	ip = (struct ip *)uh - 1;

	n = readether(d, ip, len + sizeof(*ip) + sizeof(*uh), tleft, &etype);
	if (n == -1 || n < sizeof(*ip) + sizeof(*uh))
		return -1;

	/* Ethernet address checks now in readether() */

	/* Need to respond to ARP requests. */
	if (etype == ETHERTYPE_ARP) {
		struct arphdr *ah = (void *)ip;
		if (ah->ar_op == htons(ARPOP_REQUEST)) {
			/* Send ARP reply */
			arp_reply(d, ah);
		}
		return -1;
	}

	if (etype != ETHERTYPE_IP) {
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: not IP. ether_type=%x\n", etype);
#endif
		return -1;
	}

	/* Check ip header */
	if (ip->ip_v != IPVERSION ||
	    ip->ip_p != IPPROTO_UDP) {	/* half char */
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: IP version or not UDP. ip_v=%d ip_p=%d\n", ip->ip_v, ip->ip_p);
#endif
		return -1;
	}

	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(*ip) ||
	    in_cksum(ip, hlen) != 0) {
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: short hdr or bad cksum.\n");
#endif
		return -1;
	}
	if (n < ntohs(ip->ip_len)) {
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: bad length %d < %d.\n",
			       (int)n, ntohs(ip->ip_len));
#endif
		return -1;
	}
	if (d->myip.s_addr && ip->ip_dst.s_addr != d->myip.s_addr) {
#ifdef NET_DEBUG
		if (debug) {
			printf("readudp: bad saddr %s != ", inet_ntoa(d->myip));
			printf("%s\n", inet_ntoa(ip->ip_dst));
		}
#endif
		return -1;
	}

	/* If there were ip options, make them go away */
	if (hlen != sizeof(*ip)) {
		bcopy(((u_char *)ip) + hlen, uh, len - hlen);
		ip->ip_len = htons(sizeof(*ip));
		n -= hlen - sizeof(*ip);
	}
	if (uh->uh_dport != d->myport) {
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: bad dport %d != %d\n",
				d->myport, ntohs(uh->uh_dport));
#endif
		return -1;
	}

#ifndef UDP_NO_CKSUM
	if (uh->uh_sum) {
		register struct udpiphdr *ui;
		struct ip tip;

		n = ntohs(uh->uh_ulen) + sizeof(*ip);
		if (n > RECV_SIZE - ETHER_SIZE) {
			printf("readudp: huge packet, udp len %d\n", (int)n);
			return -1;
		}

		/* Check checksum (must save and restore ip header) */
		tip = *ip;
		ui = (struct udpiphdr *)ip;
		bzero(&ui->ui_x1, sizeof(ui->ui_x1));
		ui->ui_len = uh->uh_ulen;
		if (in_cksum(ui, n) != 0) {
#ifdef NET_DEBUG
			if (debug)
				printf("readudp: bad cksum\n");
#endif
			*ip = tip;
			return -1;
		}
		*ip = tip;
	}
#endif
	if (ntohs(uh->uh_ulen) < sizeof(*uh)) {
#ifdef NET_DEBUG
		if (debug)
			printf("readudp: bad udp len %d < %d\n",
				ntohs(uh->uh_ulen), (int)sizeof(*uh));
#endif
		return -1;
	}

	n -= sizeof(*ip) + sizeof(*uh);
	return (n);
}

/*
 * Send a packet and wait for a reply, with exponential backoff.
 *
 * The send routine must return the actual number of bytes written,
 * or -1 on error.
 *
 * The receive routine can indicate success by returning the number of
 * bytes read; it can return 0 to indicate EOF; it can return -1 with a
 * non-zero errno to indicate failure; finally, it can return -1 with a
 * zero errno to indicate it isn't done yet.
 */
ssize_t
sendrecv(d, sproc, sbuf, ssize, rproc, rbuf, rsize)
	register struct iodesc *d;
	register ssize_t (*sproc)(struct iodesc *, void *, size_t);
	register void *sbuf;
	register size_t ssize;
	register ssize_t (*rproc)(struct iodesc *, void *, size_t, time_t);
	register void *rbuf;
	register size_t rsize;
{
	register ssize_t cc;
	register time_t t, tmo, tlast;
	long tleft;

#ifdef NET_DEBUG
	if (debug)
		printf("sendrecv: called\n");
#endif

	tmo = MINTMO;
	tlast = tleft = 0;
	t = getsecs();
	for (;;) {
		if (tleft <= 0) {
			if (tmo >= MAXTMO) {
				errno = ETIMEDOUT;
				return -1;
			}
			cc = (*sproc)(d, sbuf, ssize);
			if (cc != -1 && cc < ssize)
				panic("sendrecv: short write! (%d < %d)",
				    cc, ssize);

			tleft = tmo;
			tmo <<= 1;
			if (tmo > MAXTMO)
				tmo = MAXTMO;

			if (cc == -1) {
				/* Error on transmit; wait before retrying */
				while ((getsecs() - t) < tmo);
				tleft = 0;
				continue;
			}

			tlast = t;
		}

		/* Try to get a packet and process it. */
		cc = (*rproc)(d, rbuf, rsize, tleft);
		/* Return on data, EOF or real error. */
		if (cc != -1 || errno != 0)
			return (cc);

		/* Timed out or didn't get the packet we're waiting for */
		t = getsecs();
		tleft -= t - tlast;
		tlast = t;
	}
}

/*
 * Like inet_addr() in the C library, but we only accept base-10.
 * Return values are in network order.
 */
n_long
inet_addr(cp)
	char *cp;
{
	register u_long val;
	register int n;
	register char c;
	u_int parts[4];
	register u_int *pp = parts;

	for (;;) {
		/*
		 * Collect number up to ``.''.
		 * Values are specified as for C:
		 * 0x=hex, 0=octal, other=decimal.
		 */
		val = 0;
		while ((c = *cp) != '\0') {
			if (c >= '0' && c <= '9') {
				val = (val * 10) + (c - '0');
				cp++;
				continue;
			}
			break;
		}
		if (*cp == '.') {
			/*
			 * Internet format:
			 *	a.b.c.d
			 *	a.b.c	(with c treated as 16-bits)
			 *	a.b	(with b treated as 24 bits)
			 */
			if (pp >= parts + 3 || val > 0xff)
				goto bad;
			*pp++ = val, cp++;
		} else
			break;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp != '\0')
		goto bad;

	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts + 1;
	switch (n) {

	case 1:				/* a -- 32 bits */
		break;

	case 2:				/* a.b -- 8.24 bits */
		if (val > 0xffffff)
			goto bad;
		val |= parts[0] << 24;
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		if (val > 0xffff)
			goto bad;
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		if (val > 0xff)
			goto bad;
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}

	return (htonl(val));
 bad:
	return (htonl(INADDR_NONE));
}

char *
inet_ntoa(ia)
	struct in_addr ia;
{
	return (intoa(ia.s_addr));
}

/* Similar to inet_ntoa() */
char *
intoa(addr)
	register n_long addr;
{
	register char *cp;
	register u_int byte;
	register int n;
	static char buf[17];	/* strlen(".255.255.255.255") + 1 */

	NTOHL(addr);
	cp = &buf[sizeof buf];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return (cp+1);
}

static char *
number(s, n)
	char *s;
	int *n;
{
	for (*n = 0; isdigit(*s); s++)
		*n = (*n * 10) + *s - '0';
	return s;
}

n_long
ip_convertaddr(p)
	char *p;
{
#define IP_ANYADDR	0
	n_long addr = 0, n;

	if (p == (char *)0 || *p == '\0')
		return IP_ANYADDR;
	p = number(p, &n);
	addr |= (n << 24) & 0xff000000;
	if (*p == '\0' || *p++ != '.')
		return IP_ANYADDR;
	p = number(p, &n);
	addr |= (n << 16) & 0xff0000;
	if (*p == '\0' || *p++ != '.')
		return IP_ANYADDR;
	p = number(p, &n);
	addr |= (n << 8) & 0xff00;
	if (*p == '\0' || *p++ != '.')
		return IP_ANYADDR;
	p = number(p, &n);
	addr |= n & 0xff;
	if (*p != '\0')
		return IP_ANYADDR;

	return htonl(addr);
}
