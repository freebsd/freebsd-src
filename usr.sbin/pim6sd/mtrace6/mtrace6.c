/*
 * Copyright (C) 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/pim6sd/mtrace6/mtrace6.c,v 1.1.2.1 2000/07/15 07:36:46 kris Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif

#include <netinet/in.h>

#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <err.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#include "trace.h"

static char *gateway, *intface, *source, *group, *receiver, *destination;
static int mldsoc, hops = 64, maxhops = 127, waittime = 3, querylen, opt_n;
static struct sockaddr *gw_sock, *src_sock, *grp_sock, *dst_sock, *rcv_sock; 
static char *querypacket;
static char frombuf[1024];	/* XXX: enough size? */

int main __P((int, char *[]));
static char *proto_type __P((u_int));
static char *pr_addr __P((struct sockaddr *, int));
static void setqid __P((int, char *));
static void mtrace_loop __P((void));
static char *str_rflags __P((int));
static void show_ip6_result __P((struct sockaddr_in6 *, int));
static void show_result __P((struct sockaddr *, int));
static void set_sockaddr __P((char *, struct addrinfo *, struct sockaddr *));
static int is_multicast __P((struct sockaddr *));
static char *all_routers_str __P((int));
static int ip6_validaddr __P((char *, struct sockaddr_in6 *));
static int get_my_sockaddr __P((int, struct sockaddr *));
static void set_hlim __P((int, struct sockaddr *, int));
static void set_join __P((int, char *, struct sockaddr *));
static void set_filter __P((int, int));
static void open_socket __P((void));
static void make_ip6_packet __P((void));
static void make_packet __P((void));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int op;

	/* get parameters */
	while((op = getopt(argc, argv, "d:g:h:i:m:nr:w:")) != -1) {
		switch(op) {
		case 'd':
			destination = optarg;
			break;
		case 'g':
			gateway = optarg;
			break;
		case 'h':
			hops = atoi(optarg);
			if (hops < 0 || hops > 255) {
				warnx("query/response hops must be between 0 and 255");
				usage();
			}
			break;
		case 'i':
			intface = optarg;
			break;
		case 'm':
			maxhops = atoi(optarg);
			if (maxhops < 0 || maxhops > 255) {
				warnx("maxhops must be between 0 and 255");
				usage();
			}
			break;
		case 'n':
			opt_n = 1;
			break;
		case 'r':
			receiver = optarg;
			break;
		case 'w':
			waittime = atoi(optarg);
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();
	source = argv[0];
	group = argv[1];

	/* examine addresses and open a socket */
	open_socket();

	/* construct a query packet according to the specified parameters */
	make_packet();

	mtrace_loop();
	exit(0);
	/*NOTREACHED*/
}

static char *
proto_type(type)
	u_int type;
{
	static char buf[80];

	switch (type) {
	case PROTO_DVMRP:
		return ("DVMRP");
	case PROTO_MOSPF:
		return ("MOSPF");
	case PROTO_PIM:
		return ("PIM");
	case PROTO_CBT:
		return ("CBT");
	case PROTO_PIM_SPECIAL:
		return ("PIM/Special");
	case PROTO_PIM_STATIC:
		return ("PIM/Static");
	case PROTO_DVMRP_STATIC:
		return ("DVMRP/Static");
	case 0:
		return ("None");
	default:
		(void) sprintf(buf, "Unknown protocol code %d", type);
		return (buf);
	}
}

static char *
pr_addr(addr, numeric)
	struct sockaddr *addr;
	int numeric;
{
	static char buf[MAXHOSTNAMELEN];
	int flag = 0;

	if (numeric)
		flag |= NI_NUMERICHOST;
	flag |= NI_WITHSCOPEID;

	getnameinfo(addr, addr->sa_len, buf, sizeof(buf), NULL, 0, flag);

	return (buf);
}

static void
setqid(family, query)
	int family;
	char *query;
{
	struct tr6_query *q6;

	switch(family) {
	case AF_INET6:
		q6 = (struct tr6_query *)((struct mld6_hdr *)query + 1);
		q6->tr_qid = (u_int32_t)random();
	}
}

static void
mtrace_loop()
{
	int nsoc, fromlen, rcvcc;
	struct timeval tv, tv_wait;
	struct fd_set fds;
	struct sockaddr_storage from_ss;
	struct sockaddr *from_sock = (struct sockaddr *)&from_ss;

	/* initializa random number of query ID */
	gettimeofday(&tv, 0);
	srandom(tv.tv_usec);

	while(1) {		/* XXX */
		setqid(gw_sock->sa_family, querypacket);

		if (sendto(mldsoc, (void *)querypacket, querylen, 0, gw_sock,
			   gw_sock->sa_len) < 0)
			err(1, "sendto");

		tv_wait.tv_sec = waittime;
		tv_wait.tv_usec = 0;

		FD_ZERO(&fds);
		FD_SET(mldsoc, &fds);

		if ((nsoc = select(mldsoc + 1, &fds, NULL, NULL, &tv_wait)) < 0)
			err(1, "select");

		if (nsoc == 0) {
			printf("Timeout\n");
			exit(0); /* XXX try again? */
		}

		fromlen = sizeof(from_ss);
		if ((rcvcc = recvfrom(mldsoc, frombuf, sizeof(frombuf),  0,
				      from_sock, &fromlen))
		     < 0)
			err(1, "recvfrom");

		show_result(from_sock, rcvcc);
		exit(0);	/* XXX */
	}
}

char *fwd_code[] = {"NOERR", "WRONGIF", "SPRUNE", "RPRUNE", "SCOPED", "NORT",
		    "WRONGLH", "NOFWD", "RP", "RPFIF", "NOMC", "HIDDEN"};
char *fwd_errcode[] = {"", "NOSPC", "OLD", "ADMIN"};

static char *
str_rflags(flag)
	int flag;
{
	if (0x80 & flag) {	/* fatal error */
		flag &= ~0x80;
		if (flag >= sizeof(fwd_errcode) / sizeof(char *) ||
		    flag == 0) {
			warnx("unknown error code(%d)", flag);
			return("UNKNOWN");
		}
		return(fwd_errcode[flag]);
	}

	/* normal code */
	if (flag >= sizeof(fwd_code) / sizeof(char *)) {
		warnx("unknown forward code(%d)", flag);
		return("UNKNOWN");
	}
	return(fwd_code[flag]);
}

static void
show_ip6_result(from6, datalen)
	struct sockaddr_in6 *from6;
	int datalen;
{
	struct mld6_hdr *mld6_tr_resp = (struct mld6_hdr *)frombuf;
	struct mld6_hdr *mld6_tr_query = (struct mld6_hdr *)querypacket;
	struct tr6_query *tr6_rquery = (struct tr6_query *)(mld6_tr_resp + 1);
	struct tr6_query *tr6_query = (struct tr6_query *)(mld6_tr_query + 1);
	struct tr6_resp *tr6_resp = (struct tr6_resp *)(tr6_rquery + 1),
		*rp, *rp_end;
	int i;

	if (datalen < sizeof(*mld6_tr_resp) + sizeof(*tr6_rquery) +
	    sizeof(*tr6_resp)) {
		warnx("show_ip6_result: receive data length(%d) is short",
		      datalen);
		return;
	}

	switch(mld6_tr_resp->mld6_type) {
	case MLD6_MTRACE_RESP:
		if ((datalen - sizeof(*mld6_tr_resp) - sizeof(*tr6_rquery)) %
		    sizeof(*tr6_resp)) {
			warnx("show_ip6_result: incomplete response (%d bytes)",
			      datalen);
			return;
		}
		rp_end = (struct tr6_resp *)((char *)mld6_tr_resp + datalen);

		/* sanity check for the response */
		if (tr6_query->tr_qid != tr6_rquery->tr_qid ||
		    !IN6_ARE_ADDR_EQUAL(&tr6_query->tr_src, &tr6_rquery->tr_src) ||
		    !IN6_ARE_ADDR_EQUAL(&tr6_query->tr_dst, &tr6_rquery->tr_dst))
			return;	/* XXX: bark here? */

		for (i = 0, rp = tr6_resp; rp < rp_end; i++, rp++) {
			struct sockaddr_in6 sa_resp, sa_upstream;

			/* reinitialize the sockaddr. paranoid? */
			memset((void *)&sa_resp, 0, sizeof(sa_resp));
			sa_resp.sin6_family = AF_INET6;
			sa_resp.sin6_len = sizeof(sa_resp);
			memset((void *)&sa_upstream, 0, sizeof(sa_upstream));
			sa_upstream.sin6_family = AF_INET6;
			sa_upstream.sin6_len = sizeof(sa_upstream);

			sa_resp.sin6_addr = rp->tr_lcladdr;
			sa_upstream.sin6_addr = rp->tr_rmtaddr;

			/* print information for the router */
			printf("%3d  ", -i);/* index */
			/* router address and incoming/outgoing interface */
			printf("%s", pr_addr((struct sockaddr *)&sa_resp, opt_n));
			printf("(%s/%d->%d) ",
			       pr_addr((struct sockaddr *)&sa_upstream, 1),
			       ntohl(rp->tr_inifid), ntohl(rp->tr_outifid));
			/* multicast routing protocol type */
			printf("%s ", proto_type(rp->tr_rproto));
			/* forwarding error code */
			printf("%s", str_rflags(rp->tr_rflags & 0xff));

			putchar('\n');
		}

		break;
	default:		/* impossible... */
		warnx("show_ip6_result: invalid ICMPv6 type(%d)",
		      mld6_tr_resp->mld6_type); /* assert? */
		break;
	}
}

static void
show_result(from, datalen)
	struct sockaddr *from;
	int datalen;
{
	switch(from->sa_family) {
	case AF_INET6:
		show_ip6_result((struct sockaddr_in6 *)from, datalen);
		break;
	default:
		errx(1, "show_result: illegal AF(%d) on recv", from->sa_family);
	}
}

static void
set_sockaddr(addrname, hints, sap)
	char *addrname;
	struct addrinfo *hints;
	struct sockaddr *sap;
{
	struct addrinfo *res;
	int ret_ga;

	ret_ga = getaddrinfo(addrname, NULL, hints, &res);
	if (ret_ga)
		errx(1, "getaddrinfo faild: %s", gai_strerror(ret_ga));
	if (!res->ai_addr)
		errx(1, "getaddrinfo failed");
	memcpy((void *)sap, (void *)res->ai_addr, res->ai_addr->sa_len);

	freeaddrinfo(res);
}

static int
is_multicast(sa)
	struct sockaddr *sa;
{
	switch(sa->sa_family) {
	case AF_INET6:
		if (IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6 *)sa)->sin6_addr))
			return 1;
		else
			return 0;
		break;
	default:
		return 0;	/* XXX: support IPv4? */
	}
}

static char *
all_routers_str(family)
	int family;
{
	switch(family) {
	case AF_INET6:
		return("ff02::1");
	default:
		errx(1, "all_routers_str: unknown AF(%d)", family);
	}
}

static int
ip6_validaddr(ifname, addr)
	char *ifname;
	struct sockaddr_in6 *addr;
{
	int s;
	struct in6_ifreq ifr6;
	u_int32_t flags6;

	/* we need a global address only...XXX: should be flexible? */
	if (IN6_IS_ADDR_LOOPBACK(&addr->sin6_addr) ||
	    IN6_IS_ADDR_LINKLOCAL(&addr->sin6_addr) ||
	    IN6_IS_ADDR_SITELOCAL(&addr->sin6_addr))
		return(0);

	/* get IPv6 dependent flags and examine them */
	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(1, "ip6_validaddr: socket");

	strncpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
	ifr6.ifr_addr = *addr;
	if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) < 0)
		err(1, "ioctl(SIOCGIFAFLAG_IN6)");
	close(s);
	flags6 = ifr6.ifr_ifru.ifru_flags6;
	if (flags6 & (IN6_IFF_ANYCAST | IN6_IFF_TENTATIVE |
		      IN6_IFF_DUPLICATED | IN6_IFF_DETACHED))
		return(0);

	return(1);
}

static int
get_my_sockaddr(family, addrp)
	int family;
	struct sockaddr *addrp;
{
#ifdef HAVE_GETIFADDRS
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
		err(1, "getifaddrs");
		/*NOTREACHED */
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == family) {
			switch(family) {
			case AF_INET6:
				if (ip6_validaddr(ifa->ifa_name,
						  (struct sockaddr_in6 *)ifa->ifa_addr))
					goto found;
			}
		}
	}

	freeifaddrs(ifap);
	return (-1);		/* not found */

  found:
	memcpy((void *)addrp, (void *)ifa->ifa_addr, ifa->ifa_addr->sa_len);
	freeifaddrs(ifap);
	return (0);
#else
#define IF_BUFSIZE 8192		/* XXX: adhoc...should be customizable? */
	int i, s;
	struct ifconf ifconf;
	struct ifreq *ifrp;
	static char ifbuf[IF_BUFSIZE];

	if ((s = socket(family, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	ifconf.ifc_buf = ifbuf;
	ifconf.ifc_len = sizeof(ifbuf);

	if (ioctl(s, SIOCGIFCONF, (char *)&ifconf) < 0)
		err(1, "ioctl(SIOCGIFCONF)");
	close(s);

	for (i = 0; i < ifconf.ifc_len; ) {
		ifrp = (struct ifreq *)(ifbuf + i);
		if (ifrp->ifr_addr.sa_family == family) {
			switch(family) {
			case AF_INET6:
				if (ip6_validaddr(ifrp->ifr_name,
						  (struct sockaddr_in6 *)&ifrp->ifr_addr))
					goto found;
			}
		}

		i += IFNAMSIZ;
		/* i += max(sizeof(sockaddr), ifr_addr.sa_len) */
		if (ifrp->ifr_addr.sa_len > sizeof(struct sockaddr))
			i += ifrp->ifr_addr.sa_len;
		else
			i += sizeof(struct sockaddr);
	}

	return(-1);		/* not found */

  found:
	memcpy((void *)addrp, (void *)&ifrp->ifr_addr, ifrp->ifr_addr.sa_len);
	return(0);
#undef IF_BUFSIZE	
#endif
}

static void
set_hlim(s, addr, hops)
	int s, hops;
	struct sockaddr *addr;
{
	struct sockaddr_in6 *sin6;
	int opt;

	switch(addr->sa_family) {
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)addr;
		opt = IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) ?
			IPV6_MULTICAST_HOPS : IPV6_UNICAST_HOPS;
		if (setsockopt(s, IPPROTO_IPV6, opt, (char *)&hops,
			       sizeof(hops)) == -1)
			err(1, "setsockopt(%s)",
			    (opt == IPV6_MULTICAST_HOPS) ?
			    "IPV6_MULTICAST_HOPS" : "IPV6_UNICAST_HOPS");
		break;
	}
}

static void
set_join(s, ifname, group)
	int s;
	char *ifname;
	struct sockaddr *group;
{
	struct ipv6_mreq mreq6;
	u_int ifindex;
	
	switch(group->sa_family) {
	case AF_INET6:
		if ((ifindex = if_nametoindex(ifname)) == 0)
			err(1, "set_join: if_nametoindex failed for %s", ifname);
		mreq6.ipv6mr_multiaddr =
			((struct sockaddr_in6 *)group)->sin6_addr;
		mreq6.ipv6mr_interface = ifindex;

		if (setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6,
			       sizeof(mreq6)) < 0)
			err(1, "setsockopt(IPV6_JOIN_GROUP)");
		break;
	}
}

static void
set_filter(s, family)
	int s, family;
{
	struct icmp6_filter filter6;

	switch(family) {
	case AF_INET6:
		ICMP6_FILTER_SETBLOCKALL(&filter6);
		ICMP6_FILTER_SETPASS(MLD6_MTRACE_RESP, &filter6);
		if (setsockopt(s, IPPROTO_ICMPV6, ICMP6_FILTER, &filter6,
			       sizeof(filter6)) < 0)
			err(1, "setsockopt(ICMP6_FILTER)");
		break;
	}
}

static void
open_socket()
{
	struct addrinfo hints;
	static struct sockaddr_storage gw_ss, src_ss, grp_ss, dst_ss, rcv_ss;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6; /* to be independent of AF? */
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMPV6;

	/* multicast group(must be specified) */
	grp_sock = (struct sockaddr *)&grp_ss;
	set_sockaddr(group, &hints, grp_sock);
	if (!is_multicast(grp_sock))
		errx(1, "group(%s) is not a multicast address", group);

	/* multicast source(must be specified) */
	src_sock = (struct sockaddr *)&src_ss;
	set_sockaddr(source, &hints, src_sock);
	if (is_multicast(src_sock))
		errx(1, "source(%s) is not a unicast address", source);

	/* last hop gateway for the destination(if specified) */
	gw_sock = (struct sockaddr *)&gw_ss;
	if (gateway)		/* can be either multicast or unicast */
		set_sockaddr(gateway, &hints, gw_sock);
	else {
		char *r = all_routers_str(grp_sock->sa_family);

		set_sockaddr(r, &hints, gw_sock);
	}

	/* destination address for the trace */
	dst_sock = (struct sockaddr *)&dst_ss;
	if (destination) {
		set_sockaddr(destination, &hints, dst_sock);
		if (is_multicast(dst_sock))
			errx(1, "destination(%s) is not a unicast address",
			     destination);
	}
	else {
		/* XXX: consider interface? */
		get_my_sockaddr(grp_sock->sa_family, dst_sock);
	}

	/* response receiver(if specified) */
	rcv_sock = (struct sockaddr *)&rcv_ss;
	if (receiver) {		/* can be either multicast or unicast */
		set_sockaddr(receiver, &hints, rcv_sock);
		if (is_multicast(rcv_sock) &&
		    intface == NULL) {
#ifdef notyet
			warnx("receive I/F is not specified for multicast"
			      "response(%s)", receiver);
			intface = default_intface;
#else
			errx(1, "receive I/F is not specified for multicast"
			     "response(%s)", receiver);
#endif 
		}
	}
	else {
		/* XXX: consider interface? */
		get_my_sockaddr(grp_sock->sa_family, rcv_sock);
	}

	if ((mldsoc = socket(hints.ai_family, hints.ai_socktype,
			     hints.ai_protocol)) < 0)
		err(1, "socket");

	/* set necessary socket options */
	if (hops)
		set_hlim(mldsoc, gw_sock, hops);
	if (receiver && is_multicast(rcv_sock))
		set_join(mldsoc, intface, rcv_sock);
	set_filter(mldsoc, grp_sock->sa_family);
}

static void
make_ip6_packet()
{
	struct mld6_hdr *mld6_tr_query;
	struct tr6_query *tr6_query;

	querylen = sizeof(*mld6_tr_query) + sizeof(*tr6_query);
	if ((querypacket = malloc(querylen)) == NULL)
		errx(1, "make_ip6_packet: malloc failed");
	memset(querypacket, 0, querylen);

	/* fill in MLD header */
	mld6_tr_query = (struct mld6_hdr *)querypacket;
	mld6_tr_query->mld6_type = MLD6_MTRACE;
	mld6_tr_query->mld6_code = maxhops & 0xff;
	mld6_tr_query->mld6_addr = ((struct sockaddr_in6 *)grp_sock)->sin6_addr;

	/* fill in mtrace query fields */
	tr6_query = (struct tr6_query *)(mld6_tr_query + 1);
	tr6_query->tr_src = ((struct sockaddr_in6 *)src_sock)->sin6_addr;
	tr6_query->tr_dst = ((struct sockaddr_in6 *)dst_sock)->sin6_addr;
	tr6_query->tr_raddr = ((struct sockaddr_in6 *)rcv_sock)->sin6_addr;
	tr6_query->tr_rhlim = 0xff & hops;
}

static void
make_packet()
{
	switch(grp_sock->sa_family) {
	case AF_INET6:
		make_ip6_packet();
		break;
	default:
		errx(1, "make_packet: unsupported AF(%d)", grp_sock->sa_family);
	}
}

static void
usage()
{
	fprintf(stderr, "usage: mtrace6 %s\n",
	     "[-d destination] [-g gateway] [-h hops] [-i interface] "
	     "[-m maxhops] [-n] [-r response_addr] [-w waittime] source group");
	exit(1);
}
