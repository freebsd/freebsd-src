/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <libcasper.h>
#include <casper/cap_dns.h>
#include <capsicum_helpers.h>

#include <netdb.h>
#include <stdio.h>
#include <err.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <sysexits.h>

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/sctp.h>
#include <netinet/sctp_header.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#ifdef IPSEC
#include <net/route.h>
#include <netipsec/ipsec.h>
#endif

#include "traceroute.h"

#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
static int	setpolicy6(int so, char *policy);
#endif
static void	*get_uphdr(struct context *, struct ip6_hdr *, u_char *);
static int	get_hoplim(struct msghdr *);
static const char *pr_type6(int);
static int	packet_ok6(struct context *, struct msghdr *, int, int,
			   u_char *, u_char *, u_char *);
static void	print6(struct context *, struct msghdr *, int);
static uint16_t udp_cksum(struct sockaddr_in6 *, struct sockaddr_in6 *,
			  void const *, uint32_t);
static uint16_t tcp_chksum(struct sockaddr_in6 *, struct sockaddr_in6 *,
			   void const *, uint32_t);

static struct msghdr rcvmhdr;
static struct iovec rcviov[2];
static int rcvhlim;
static struct in6_pktinfo *rcvpktinfo;

static struct sockaddr_in6 Rcv;
#define	ICMP6ECHOLEN	8

static int tclass = -1;

int traceroute6(struct context *ctx)
{
	int i, on = 1, seq, rcvcmsglen;
	static u_char *rcvcmsgbuf;
	u_char type, code, ecn;
#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	char ipsec_inpolicy[] = "in bypass";
	char ipsec_outpolicy[] = "out bypass";
#endif

	/* specify to tell receiving interface */
	if (setsockopt(ctx->rcvsock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
	    sizeof(on)) < 0)
		err(1, "setsockopt(IPV6_RECVPKTINFO)");

	/* specify to tell value of hoplimit field of received IP6 hdr */
	if (setsockopt(ctx->rcvsock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
	    sizeof(on)) < 0)
		err(1, "setsockopt(IPV6_RECVHOPLIMIT)");

	seq = 0;

#if 0
	while ((ch = getopt(argc, argv, "aA:dEf:g:Ilm:nNp:q:rs:St:TUvw:")) != -1)
		switch (ch) {
		case 'S':
			useproto = IPPROTO_SCTP;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
#endif

	if (ctx->options->detect_ecn_bleaching) {
		if (tclass != -1) {
			tclass &= ~IPTOS_ECN_MASK;
		} else {
			tclass = 0;
		}
		tclass |= IPTOS_ECN_ECT1;
	}

	if (tclass != -1) {
		if (setsockopt(ctx->sendsock, IPPROTO_IPV6, IPV6_TCLASS,
			       &tclass, sizeof(int)) == -1) {
			perror("setsockopt(IPV6_TCLASS)");
			exit(7);
		}
	}

	if ((ctx->protocol->protocol_number == IPPROTO_SCTP)
	    && (ctx->options->packetlen & 3)) {
		fprintf(stderr,
		    "%s: packet size must be a multiple of 4.\n",
		    getprogname());
		exit(1);
	}

	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = (caddr_t)ctx->packet;
	rcviov[0].iov_len = sizeof(ctx->packet);
	rcvmhdr.msg_name = (caddr_t)&Rcv;
	rcvmhdr.msg_namelen = sizeof(Rcv);
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	if ((rcvcmsgbuf = malloc(rcvcmsglen)) == NULL) {
		fprintf(stderr, "%s: malloc failed\n", getprogname());
		exit(1);
	}
	rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
	rcvmhdr.msg_controllen = rcvcmsglen;

#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	/*
	 * do not raise error even if setsockopt fails, kernel may have ipsec
	 * turned off.
	 */
	if (setpolicy6(ctx->rcvsock, ipsec_inpolicy) < 0)
		errx(1, "%s", ipsec_strerror());
	if (setpolicy6(ctx->rcvsock, ipsec_outpolicy) < 0)
		errx(1, "%s", ipsec_strerror());
#else
    {
	int level = IPSEC_LEVEL_NONE;

	(void)setsockopt(rcvsock, IPPROTO_IPV6, IPV6_ESP_TRANS_LEVEL, &level,
	    sizeof(level));
	(void)setsockopt(rcvsock, IPPROTO_IPV6, IPV6_ESP_NETWORK_LEVEL, &level,
	    sizeof(level));
	(void)setsockopt(rcvsock, IPPROTO_IPV6, IPV6_AUTH_TRANS_LEVEL, &level,
	    sizeof(level));
	(void)setsockopt(rcvsock, IPPROTO_IPV6, IPV6_AUTH_NETWORK_LEVEL, &level,
	    sizeof(level));
    }
#endif /* !(IPSEC && IPSEC_POLICY_IPSEC) */

	i = ctx->options->packetlen;
	if (i == 0)
		i = 1;
	if (setsockopt(ctx->sendsock, SOL_SOCKET, SO_SNDBUF,
		       &i, sizeof(i)) < 0)
		err(EX_OSERR, "setsockopt(SO_SNDBUF)");

#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	/*
	 * do not raise error even if setsockopt fails, kernel may have ipsec
	 * turned off.
	 */
	if (setpolicy6(ctx->sendsock, ipsec_inpolicy) < 0)
		errx(1, "%s", ipsec_strerror());
	if (setpolicy6(ctx->sendsock, ipsec_outpolicy) < 0)
		errx(1, "%s", ipsec_strerror());
#else
    {
	int level = IPSEC_LEVEL_BYPASS;

	(void)setsockopt(sndsock, IPPROTO_IPV6, IPV6_ESP_TRANS_LEVEL, &level,
	    sizeof(level));
	(void)setsockopt(sndsock, IPPROTO_IPV6, IPV6_ESP_NETWORK_LEVEL, &level,
	    sizeof(level));
	(void)setsockopt(sndsock, IPPROTO_IPV6, IPV6_AUTH_TRANS_LEVEL, &level,
	    sizeof(level));
	(void)setsockopt(sndsock, IPPROTO_IPV6, IPV6_AUTH_NETWORK_LEVEL, &level,
	    sizeof(level));
    }
#endif /* !(IPSEC && IPSEC_POLICY_IPSEC) */

	/*
	 * Main loop
	 */
	for (unsigned hops = ctx->options->first_ttl;
	     hops <= ctx->options->max_ttl;
	     ++hops) {
		struct in6_addr lastaddr;
		int got_there = 0;
		unsigned unreachable = 0;

		printf("%2d ", hops);
		bzero(&lastaddr, sizeof(lastaddr));
		for (unsigned probe = 0; probe < ctx->options->nprobes; ++probe) {
			int cc;
			struct timeval t1, t2;

			(void) gettimeofday(&t1, NULL);
			send_probe(ctx, ++seq, hops);
			while ((cc = wait_for_reply(ctx, &rcvmhdr))) {
				(void) gettimeofday(&t2, NULL);
				if (packet_ok6(ctx, &rcvmhdr, cc, seq, &type,
					       &code, &ecn)) {
					if (!IN6_ARE_ADDR_EQUAL(&Rcv.sin6_addr,
					    &lastaddr)) {
						if (probe > 0)
							fputs("\n   ", stdout);
						print6(ctx, &rcvmhdr, cc);
						lastaddr = Rcv.sin6_addr;
					}
					printf("  %.3f ms", deltaT(&t1, &t2));
					if (ctx->options->detect_ecn_bleaching) {
						switch (ecn) {
						case IPTOS_ECN_ECT1:
							printf(" (ecn=passed)");
							break;
						case IPTOS_ECN_NOTECT:
							printf(" (ecn=bleached)");
							break;
						case IPTOS_ECN_CE:
							printf(" (ecn=congested)");
							break;
						default:
							printf(" (ecn=mangled)");
							break;
						}
					}
					if (type == ICMP6_DST_UNREACH) {
						switch (code) {
						case ICMP6_DST_UNREACH_NOROUTE:
							++unreachable;
							printf(" !N");
							break;
						case ICMP6_DST_UNREACH_ADMIN:
							++unreachable;
							printf(" !P");
							break;
						case ICMP6_DST_UNREACH_NOTNEIGHBOR:
							++unreachable;
							printf(" !S");
							break;
						case ICMP6_DST_UNREACH_ADDR:
							++unreachable;
							printf(" !A");
							break;
						case ICMP6_DST_UNREACH_NOPORT:
							if (rcvhlim >= 0 &&
							    rcvhlim <= 1)
								printf(" !");
							++got_there;
							break;
						}
					} else if (type == ICMP6_PARAM_PROB &&
					    code == ICMP6_PARAMPROB_NEXTHEADER) {
						printf(" !H");
						++got_there;
					} else if (type == ICMP6_ECHO_REPLY) {
						if (rcvhlim >= 0 &&
						    rcvhlim <= 1)
							printf(" !");
						++got_there;
					}
					break;
				} else if (deltaT(&t1, &t2) 
					   > ctx->options->wait_time * 1000) {
					cc = 0;
					break;
				}
			}
			if (cc == 0)
				printf(" *");
			(void) fflush(stdout);
		}
		putchar('\n');
		if (got_there ||
		    (unreachable > 0
		     && unreachable >= ((ctx->options->nprobes + 1) / 2))) {
			exit(0);
		}
	}

	return (0);
}

#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
int
setpolicy6(int so, char *policy)
{
	char *buf;

	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL) {
		warnx("%s", ipsec_strerror());
		return (-1);
	}
	(void)setsockopt(so, IPPROTO_IPV6, IPV6_IPSEC_POLICY,
	    buf, ipsec_get_policylen(buf));

	free(buf);

	return (0);
}
#endif

int
get_hoplim(struct msghdr *mhdr)
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return (*(int *)CMSG_DATA(cm));
	}

	return (-1);
}

/*
 * Convert an ICMP "type" field to a printable string.
 */
const char *
pr_type6(int t0)
{
	u_char t = t0 & 0xff;
	const char *cp;

	switch (t) {
	case ICMP6_DST_UNREACH:
		cp = "Destination Unreachable";
		break;
	case ICMP6_PACKET_TOO_BIG:
		cp = "Packet Too Big";
		break;
	case ICMP6_TIME_EXCEEDED:
		cp = "Time Exceeded";
		break;
	case ICMP6_PARAM_PROB:
		cp = "Parameter Problem";
		break;
	case ICMP6_ECHO_REQUEST:
		cp = "Echo Request";
		break;
	case ICMP6_ECHO_REPLY:
		cp = "Echo Reply";
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		cp = "Group Membership Query";
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		cp = "Group Membership Report";
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		cp = "Group Membership Reduction";
		break;
	case ND_ROUTER_SOLICIT:
		cp = "Router Solicitation";
		break;
	case ND_ROUTER_ADVERT:
		cp = "Router Advertisement";
		break;
	case ND_NEIGHBOR_SOLICIT:
		cp = "Neighbor Solicitation";
		break;
	case ND_NEIGHBOR_ADVERT:
		cp = "Neighbor Advertisement";
		break;
	case ND_REDIRECT:
		cp = "Redirect";
		break;
	default:
		cp = "Unknown";
		break;
	}
	return (cp);
}

static int
packet_ok6(struct context *ctx, struct msghdr *mhdr, int cc, int seq,
	   u_char *type, u_char *code,
    u_char *ecn)
{
	struct icmp6_hdr *icp;
	struct sockaddr_in6 *from = (struct sockaddr_in6 *)mhdr->msg_name;
	char *buf = (char *)mhdr->msg_iov[0].iov_base;
	struct cmsghdr *cm;
	int *hlimp;
	char hbuf[NI_MAXHOST];

	if (cc < (int)sizeof(struct icmp6_hdr)) {
		if (ctx->options->verbose) {
			if (cap_getnameinfo(capdns, (struct sockaddr *)from, from->sin6_len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(hbuf, "invalid", sizeof(hbuf));
			printf("data too short (%d bytes) from %s\n", cc, hbuf);
		}
		return (0);
	}
	icp = (struct icmp6_hdr *)buf;

	/* get optional information via advanced API */
	rcvpktinfo = NULL;
	hlimp = NULL;
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len ==
		    CMSG_LEN(sizeof(struct in6_pktinfo)))
			rcvpktinfo = (struct in6_pktinfo *)(CMSG_DATA(cm));

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}
	if (rcvpktinfo == NULL || hlimp == NULL) {
		warnx("failed to get received hop limit or packet info");
		rcvhlim = 0;	/*XXX*/
	} else
		rcvhlim = *hlimp;

	*type = icp->icmp6_type;
	*code = icp->icmp6_code;
	if ((*type == ICMP6_TIME_EXCEEDED &&
	    *code == ICMP6_TIME_EXCEED_TRANSIT) ||
	    (*type == ICMP6_DST_UNREACH) ||
	    (*type == ICMP6_PARAM_PROB &&
	    *code == ICMP6_PARAMPROB_NEXTHEADER)) {
		struct ip6_hdr *hip;
		struct icmp6_hdr *icmp;
		struct sctp_init_chunk *init;
		struct sctphdr *sctp;
		struct tcphdr *tcp;
		struct udphdr *udp;
		void *up;

		hip = (struct ip6_hdr *)(icp + 1);
		*ecn = ntohl(hip->ip6_flow & IPV6_ECN_MASK) >> 20;
		if ((up = get_uphdr(ctx, hip, (u_char *)(buf + cc))) == NULL) {
			if (ctx->options->verbose)
				warnx("failed to get upper layer header");
			return (0);
		}
		switch (ctx->options->protocol) {
		case IPPROTO_ICMPV6:
			icmp = (struct icmp6_hdr *)up;
			if (icmp->icmp6_id == ctx->ident &&
			    icmp->icmp6_seq == htons(seq))
				return (1);
			break;
		case IPPROTO_UDP:
			udp = (struct udphdr *)up;
			if (udp->uh_sport == htons(ctx->ident) &&
			    udp->uh_dport == htons(ctx->options->port + seq))
				return (1);
			break;
		case IPPROTO_SCTP:
			sctp = (struct sctphdr *)up;
			if (sctp->src_port != htons(ctx->ident) ||
			    sctp->dest_port != htons(ctx->options->port + seq)) {
				break;
			}
			if (ctx->options->packetlen >=
			    (sizeof(struct sctphdr)
			     + sizeof(struct sctp_init_chunk))) {
				if (sctp->v_tag != 0) {
					break;
				}
				init = (struct sctp_init_chunk *)(sctp + 1);
				/* Check the initiate tag, if available. */
				if ((char *)&init->init.a_rwnd > buf + cc) {
					return (1);
				}
				if (init->init.initiate_tag == (uint32_t)
				    ((sctp->src_port << 16) | sctp->dest_port)) {
					return (1);
				}
			} else {
				if (sctp->v_tag ==
				    (uint32_t)((sctp->src_port << 16) |
				    sctp->dest_port)) {
					return (1);
				}
			}
			break;
		case IPPROTO_TCP:
			tcp = (struct tcphdr *)up;
			if (tcp->th_sport == htons(ctx->ident) &&
			    tcp->th_dport == htons(ctx->options->port + seq) &&
			    tcp->th_seq ==
			    (tcp_seq)((tcp->th_sport << 16) | tcp->th_dport))
				return (1);
			break;
		case IPPROTO_NONE:
			return (1);
		default:
			fprintf(stderr, "Unknown probe proto %d.\n",
				ctx->options->protocol);
			break;
		}
	} else if (ctx->options->protocol == IPPROTO_ICMPV6
		   && *type == ICMP6_ECHO_REPLY) {
		if (icp->icmp6_id == ctx->ident &&
		    icp->icmp6_seq == htons(seq))
			return (1);
	}
	if (ctx->options->verbose) {
		char sbuf[NI_MAXHOST + 1], dbuf[INET6_ADDRSTRLEN];
		uint8_t *p;
		int i;

		if (cap_getnameinfo(capdns, (struct sockaddr *)from, from->sin6_len,
		    sbuf, sizeof(sbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(sbuf, "invalid", sizeof(sbuf));
		printf("\n%d bytes from %s to %s", cc, sbuf,
		    rcvpktinfo ? inet_ntop(AF_INET6, &rcvpktinfo->ipi6_addr,
		    dbuf, sizeof(dbuf)) : "?");
		printf(": icmp type %d (%s) code %d\n", *type, pr_type6(*type),
		    *code);
		p = (uint8_t *)(icp + 1);
#define WIDTH	16
		for (i = 0; i < cc; i++) {
			if (i % WIDTH == 0)
				printf("%04x:", i);
			if (i % 4 == 0)
				printf(" ");
			printf("%02x", p[i]);
			if (i % WIDTH == WIDTH - 1)
				printf("\n");
		}
		if (cc % WIDTH != 0)
			printf("\n");
	}
	return (0);
}

/*
 * Increment pointer until find the UDP or ICMP header.
 */
void *
get_uphdr(struct context *ctx, struct ip6_hdr *ip6, u_char *lim)
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;
	static u_char none_hdr[1]; /* Fake pointer for IPPROTO_NONE. */

	if (cp + sizeof(*ip6) > lim)
		return (NULL);

	nh = ip6->ip6_nxt;
	cp += sizeof(struct ip6_hdr);

	while (lim - cp >= (nh == IPPROTO_NONE ? 0 : 8)) {
		switch (nh) {
		case IPPROTO_ESP:
			return (NULL);
		case IPPROTO_ICMPV6:
			if (ctx->options->protocol == IPPROTO_ICMPV6
			    && nh == IPPROTO_ICMPV6)
				return cp;
			return NULL;
		case IPPROTO_SCTP:
		case IPPROTO_TCP:
		case IPPROTO_UDP:
			return (ctx->options->protocol == nh ? cp : NULL);
		case IPPROTO_NONE:
			return (ctx->options->protocol == nh ? none_hdr : NULL);
		case IPPROTO_FRAGMENT:
			hlen = sizeof(struct ip6_frag);
			nh = ((struct ip6_frag *)cp)->ip6f_nxt;
			break;
		case IPPROTO_AH:
			hlen = (((struct ip6_ext *)cp)->ip6e_len + 2) << 2;
			nh = ((struct ip6_ext *)cp)->ip6e_nxt;
			break;
		default:
			hlen = (((struct ip6_ext *)cp)->ip6e_len + 1) << 3;
			nh = ((struct ip6_ext *)cp)->ip6e_nxt;
			break;
		}

		cp += hlen;
	}

	return (NULL);
}

static void
print6(struct context *ctx, struct msghdr *mhdr, int cc)
{
	print_hop(ctx, mhdr->msg_name);

#ifdef XXX
	if (ctx->options->verbose)
		printf(" %d bytes of data to %s", cc, sipaddr);
#endif
}
