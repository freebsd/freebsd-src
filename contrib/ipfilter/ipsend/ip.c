/*
 * ip.c (C) 1995-1997 Darren Reed
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#if !defined(lint)
static const char sccsid[] = "%W% %G% (C)1995";
static const char rcsid[] = "@(#)$Id: ip.c,v 2.0.2.11.2.2 1997/11/28 03:36:47 darrenr Exp $";
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <sys/param.h>
#ifndef	linux
# include <netinet/if_ether.h>
# include <netinet/ip_var.h>
# if __FreeBSD_version >= 300000
#  include <net/if_var.h>
# endif
#endif
#include "ipsend.h"


static	char	*ipbuf = NULL, *ethbuf = NULL;


u_short	chksum(buf,len)
u_short	*buf;
int	len;
{
	u_long	sum = 0;
	int	nwords = len >> 1;

	for(; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum>>16) + (sum & 0xffff);
	sum += (sum >>16);
	return (~sum);
}


int	send_ether(nfd, buf, len, gwip)
int	nfd, len;
char	*buf;
struct	in_addr	gwip;
{
	static	struct	in_addr	last_gw;
	static	char	last_arp[6] = { 0, 0, 0, 0, 0, 0};
	ether_header_t	*eh;
	char	*s;
	int	err;

	if (!ethbuf)
		ethbuf = (char *)calloc(1, 65536+1024);
	s = ethbuf;
	eh = (ether_header_t *)s;

	bcopy((char *)buf, s + sizeof(*eh), len);
	if (gwip.s_addr == last_gw.s_addr)
		bcopy(last_arp, (char *)A_A eh->ether_dhost, 6);
	else if (arp((char *)&gwip, (char *)A_A eh->ether_dhost) == -1)
	    {
		perror("arp");
		return -2;
	    }
	eh->ether_type = htons(ETHERTYPE_IP);
	last_gw.s_addr = gwip.s_addr;
	err = sendip(nfd, s, sizeof(*eh) + len);
	return err;
}


/*
 */
int	send_ip(nfd, mtu, ip, gwip, frag)
int	nfd, mtu;
ip_t	*ip;
struct	in_addr	gwip;
int	frag;
{
	static	struct	in_addr	last_gw;
	static	char	last_arp[6] = { 0, 0, 0, 0, 0, 0};
	static	u_short	id = 0;
	ether_header_t	*eh;
	ip_t	ipsv;
	int	err, iplen;

	if (!ipbuf)
		ipbuf = (char *)malloc(65536);
	eh = (ether_header_t *)ipbuf;

	bzero((char *)A_A eh->ether_shost, sizeof(eh->ether_shost));
	if (last_gw.s_addr && (gwip.s_addr == last_gw.s_addr))
		bcopy(last_arp, (char *)A_A eh->ether_dhost, 6);
	else if (arp((char *)&gwip, (char *)A_A eh->ether_dhost) == -1)
	    {
		perror("arp");
		return -2;
	    }
	bcopy((char *)A_A eh->ether_dhost, last_arp, sizeof(last_arp));
	eh->ether_type = htons(ETHERTYPE_IP);

	bcopy((char *)ip, (char *)&ipsv, sizeof(*ip));
	last_gw.s_addr = gwip.s_addr;
	iplen = ip->ip_len;
	ip->ip_len = htons(iplen);
	ip->ip_off = htons(ip->ip_off);
	if (!(frag & 2)) {
		if (!ip->ip_v)
			ip->ip_v   = IPVERSION;
		if (!ip->ip_id)
			ip->ip_id  = htons(id++);
		if (!ip->ip_ttl)
			ip->ip_ttl = 60;
	}

	if (!frag || (sizeof(*eh) + iplen < mtu))
	    {
		ip->ip_sum = 0;
		ip->ip_sum = chksum((u_short *)ip, ip->ip_hl << 2);

		bcopy((char *)ip, ipbuf + sizeof(*eh), iplen);
		err =  sendip(nfd, ipbuf, sizeof(*eh) + iplen);
	    }
	else
	    {
		/*
		 * Actually, this is bogus because we're putting all IP
		 * options in every packet, which isn't always what should be
		 * done.  Will do for now.
		 */
		ether_header_t	eth;
		char	optcpy[48], ol;
		char	*s;
		int	i, sent = 0, ts, hlen, olen;

		hlen = ip->ip_hl << 2;
		if (mtu < (hlen + 8)) {
			fprintf(stderr, "mtu (%d) < ip header size (%d) + 8\n",
				mtu, hlen);
			fprintf(stderr, "can't fragment data\n");
			return -2;
		}
		ol = (ip->ip_hl << 2) - sizeof(*ip);
		for (i = 0, s = (char*)(ip + 1); ol > 0; )
			if (*s == IPOPT_EOL) {
				optcpy[i++] = *s;
				break;
			} else if (*s == IPOPT_NOP) {
				s++;
				ol--;
			} else
			    {
				olen = (int)(*(u_char *)(s + 1));
				ol -= olen;
				if (IPOPT_COPIED(*s))
				    {
					bcopy(s, optcpy + i, olen);
					i += olen;
					s += olen;
				    }
			    }
		if (i)
		    {
			/*
			 * pad out
			 */
			while ((i & 3) && (i & 3) != 3)
				optcpy[i++] = IPOPT_NOP;
			if ((i & 3) == 3)
				optcpy[i++] = IPOPT_EOL;
		    }

		bcopy((char *)eh, (char *)&eth, sizeof(eth));
		s = (char *)ip + hlen;
		iplen = ntohs(ip->ip_len) - hlen;
		ip->ip_off |= htons(IP_MF);

		while (1)
		    {
			if ((sent + (mtu - hlen)) >= iplen)
			    {
				ip->ip_off ^= htons(IP_MF);
				ts = iplen - sent;
			    }
			else
				ts = (mtu - hlen);
			ip->ip_off &= htons(0xe000);
			ip->ip_off |= htons(sent >> 3);
			ts += hlen;
			ip->ip_len = htons(ts);
			ip->ip_sum = 0;
			ip->ip_sum = chksum((u_short *)ip, hlen);
			bcopy((char *)ip, ipbuf + sizeof(*eh), hlen);
			bcopy(s + sent, ipbuf + sizeof(*eh) + hlen, ts - hlen);
			err =  sendip(nfd, ipbuf, sizeof(*eh) + ts);

			bcopy((char *)&eth, ipbuf, sizeof(eth));
			sent += (ts - hlen);
			if (!(ntohs(ip->ip_off) & IP_MF))
				break;
			else if (!(ip->ip_off & htons(0x1fff)))
			    {
				hlen = i + sizeof(*ip);
				ip->ip_hl = (sizeof(*ip) + i) >> 2;
				bcopy(optcpy, (char *)(ip + 1), i);
			    }
		    }
	    }

	bcopy((char *)&ipsv, (char *)ip, sizeof(*ip));
	return err;
}


/*
 * send a tcp packet.
 */
int	send_tcp(nfd, mtu, ip, gwip)
int	nfd, mtu;
ip_t	*ip;
struct	in_addr	gwip;
{
	static	tcp_seq	iss = 2;
	struct	tcpiphdr *ti;
	tcphdr_t *t;
	int	thlen, i, iplen, hlen;
	u_32_t	lbuf[20];

	iplen = ip->ip_len;
	hlen = ip->ip_hl << 2;
	t = (tcphdr_t *)((char *)ip + hlen);
	ti = (struct tcpiphdr *)lbuf;
	thlen = t->th_off << 2;
	if (!thlen)
		thlen = sizeof(tcphdr_t);
	bzero((char *)ti, sizeof(*ti));
	ip->ip_p = IPPROTO_TCP;
	ti->ti_pr = ip->ip_p;
	ti->ti_src = ip->ip_src;
	ti->ti_dst = ip->ip_dst;
	bcopy((char *)ip + hlen, (char *)&ti->ti_sport, thlen);

	if (!ti->ti_win)
		ti->ti_win = htons(4096);
	iss += 63;

	i = sizeof(struct tcpiphdr) / sizeof(long);

	if ((ti->ti_flags == TH_SYN) && !ip->ip_off &&
	    (lbuf[i] != htonl(0x020405b4))) {
		lbuf[i] = htonl(0x020405b4);
		bcopy((char *)ip + hlen + thlen, (char *)ip + hlen + thlen + 4,
		      iplen - thlen - hlen);
		thlen += 4;
	    }
	ti->ti_off = thlen >> 2;
	ti->ti_len = htons(thlen);
	ip->ip_len = hlen + thlen;
	ti->ti_sum = 0;
	ti->ti_sum = chksum((u_short *)ti, thlen + sizeof(ip_t));

	bcopy((char *)&ti->ti_sport, (char *)ip + hlen, thlen);
	return send_ip(nfd, mtu, ip, gwip, 1);
}


/*
 * send a udp packet.
 */
int	send_udp(nfd, mtu, ip, gwip)
int	nfd, mtu;
ip_t	*ip;
struct	in_addr	gwip;
{
	struct	tcpiphdr *ti;
	int	thlen;
	u_long	lbuf[20];

	ti = (struct tcpiphdr *)lbuf;
	bzero((char *)ti, sizeof(*ti));
	thlen = sizeof(udphdr_t);
	ti->ti_pr = ip->ip_p;
	ti->ti_src = ip->ip_src;
	ti->ti_dst = ip->ip_dst;
	bcopy((char *)ip + (ip->ip_hl << 2),
	      (char *)&ti->ti_sport, sizeof(udphdr_t));

	ti->ti_len = htons(thlen);
	ip->ip_len = (ip->ip_hl << 2) + thlen;
	ti->ti_sum = 0;
	ti->ti_sum = chksum((u_short *)ti, thlen + sizeof(ip_t));

	bcopy((char *)&ti->ti_sport,
	      (char *)ip + (ip->ip_hl << 2), sizeof(udphdr_t));
	return send_ip(nfd, mtu, ip, gwip, 1);
}


/*
 * send an icmp packet.
 */
int	send_icmp(nfd, mtu, ip, gwip)
int	nfd, mtu;
ip_t	*ip;
struct	in_addr	gwip;
{
	struct	icmp	*ic;

	ic = (struct icmp *)((char *)ip + (ip->ip_hl << 2));

	ic->icmp_cksum = 0;
	ic->icmp_cksum = chksum((u_short *)ic, sizeof(struct icmp));

	return send_ip(nfd, mtu, ip, gwip, 1);
}


int	send_packet(nfd, mtu, ip, gwip)
int	nfd, mtu;
ip_t	*ip;
struct	in_addr	gwip;
{
        switch (ip->ip_p)
        {
        case IPPROTO_TCP :
                return send_tcp(nfd, mtu, ip, gwip);
        case IPPROTO_UDP :
                return send_udp(nfd, mtu, ip, gwip);
        case IPPROTO_ICMP :
                return send_icmp(nfd, mtu, ip, gwip);
        default :
                return send_ip(nfd, mtu, ip, gwip, 1);
        }
}
