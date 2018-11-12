/*	$KAME: rtsol.c,v 1.27 2003/10/05 00:09:36 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * Copyright (C) 2011 Hiroki Sato
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>

#define	__BSD_VISIBLE	1	/* IN6ADDR_LINKLOCAL_ALLROUTERS_INIT */
#include <netinet/in.h>
#undef 	__BSD_VISIBLE
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include "rtsold.h"

static struct msghdr rcvmhdr;
static struct msghdr sndmhdr;
static struct iovec rcviov[2];
static struct iovec sndiov[2];
static struct sockaddr_in6 from;
static int rcvcmsglen;

int rssock;
static char rsid[IFNAMSIZ + 1 + sizeof(DNSINFO_ORIGIN_LABEL) + 1 + NI_MAXHOST];
struct ifinfo_head_t ifinfo_head =
	TAILQ_HEAD_INITIALIZER(ifinfo_head);

static const struct sockaddr_in6 sin6_allrouters = {
	.sin6_len =	sizeof(sin6_allrouters),
	.sin6_family =	AF_INET6,
	.sin6_addr =	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT,
};

static void call_script(const int, const char *const *,
    struct script_msg_head_t *);
static size_t dname_labeldec(char *, size_t, const char *);
static int safefile(const char *);
static struct ra_opt *find_raopt(struct rainfo *, int, void *, size_t);
static int ra_opt_rdnss_dispatch(struct ifinfo *, struct rainfo *,
    struct script_msg_head_t *, struct script_msg_head_t *);
static char *make_rsid(const char *, const char *, struct rainfo *);

#define	_ARGS_OTHER	otherconf_script, ifi->ifname
#define	_ARGS_RESADD	resolvconf_script, "-a", rsid
#define	_ARGS_RESDEL	resolvconf_script, "-d", rsid

#define	CALL_SCRIPT(name, sm_head)					\
	do {								\
		const char *const sarg[] = { _ARGS_##name, NULL };	\
		call_script(sizeof(sarg), sarg, sm_head);		\
	} while(0)

#define	ELM_MALLOC(p,error_action)					\
	do {								\
		p = malloc(sizeof(*p));					\
		if (p == NULL) {					\
			warnmsg(LOG_ERR, __func__, "malloc failed: %s", \
				strerror(errno));			\
			error_action;					\
		}							\
		memset(p, 0, sizeof(*p));				\
	} while(0)

int
sockopen(void)
{
	static u_char *rcvcmsgbuf = NULL, *sndcmsgbuf = NULL;
	int sndcmsglen, on;
	static u_char answer[1500];
	struct icmp6_filter filt;

	sndcmsglen = rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	if (rcvcmsgbuf == NULL && (rcvcmsgbuf = malloc(rcvcmsglen)) == NULL) {
		warnmsg(LOG_ERR, __func__,
		    "malloc for receive msghdr failed");
		return (-1);
	}
	if (sndcmsgbuf == NULL && (sndcmsgbuf = malloc(sndcmsglen)) == NULL) {
		warnmsg(LOG_ERR, __func__,
		    "malloc for send msghdr failed");
		return (-1);
	}
	if ((rssock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		warnmsg(LOG_ERR, __func__, "socket: %s", strerror(errno));
		return (-1);
	}

	/* specify to tell receiving interface */
	on = 1;
	if (setsockopt(rssock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
	    sizeof(on)) < 0) {
		warnmsg(LOG_ERR, __func__, "IPV6_RECVPKTINFO: %s",
		    strerror(errno));
		exit(1);
	}

	/* specify to tell value of hoplimit field of received IP6 hdr */
	on = 1;
	if (setsockopt(rssock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
	    sizeof(on)) < 0) {
		warnmsg(LOG_ERR, __func__, "IPV6_RECVHOPLIMIT: %s",
		    strerror(errno));
		exit(1);
	}

	/* specfiy to accept only router advertisements on the socket */
	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	if (setsockopt(rssock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
	    sizeof(filt)) == -1) {
		warnmsg(LOG_ERR, __func__, "setsockopt(ICMP6_FILTER): %s",
		    strerror(errno));
		return(-1);
	}

	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = (caddr_t)answer;
	rcviov[0].iov_len = sizeof(answer);
	rcvmhdr.msg_name = (caddr_t)&from;
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;

	/* initialize msghdr for sending packets */
	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 1;
	sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
	sndmhdr.msg_controllen = sndcmsglen;

	return (rssock);
}

void
sendpacket(struct ifinfo *ifi)
{
	struct in6_pktinfo *pi;
	struct cmsghdr *cm;
	int hoplimit = 255;
	ssize_t i;
	struct sockaddr_in6 dst;

	dst = sin6_allrouters;
	dst.sin6_scope_id = ifi->linkid;

	sndmhdr.msg_name = (caddr_t)&dst;
	sndmhdr.msg_iov[0].iov_base = (caddr_t)ifi->rs_data;
	sndmhdr.msg_iov[0].iov_len = ifi->rs_datalen;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	/* specify the outgoing interface */
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)(void *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
	pi->ipi6_ifindex = ifi->sdl->sdl_index;

	/* specify the hop limit of the packet */
	cm = CMSG_NXTHDR(&sndmhdr, cm);
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_HOPLIMIT;
	cm->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));

	warnmsg(LOG_DEBUG, __func__,
	    "send RS on %s, whose state is %d",
	    ifi->ifname, ifi->state);
	i = sendmsg(rssock, &sndmhdr, 0);
	if (i < 0 || (size_t)i != ifi->rs_datalen) {
		/*
		 * ENETDOWN is not so serious, especially when using several
		 * network cards on a mobile node. We ignore it.
		 */
		if (errno != ENETDOWN || dflag > 0)
			warnmsg(LOG_ERR, __func__, "sendmsg on %s: %s",
			    ifi->ifname, strerror(errno));
	}

	/* update counter */
	ifi->probes++;
}

void
rtsol_input(int s)
{
	char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	int l, ifindex = 0, *hlimp = NULL;
	ssize_t msglen;
	struct in6_pktinfo *pi = NULL;
	struct ifinfo *ifi = NULL;
	struct ra_opt *rao = NULL;
	struct icmp6_hdr *icp;
	struct nd_router_advert *nd_ra;
	struct cmsghdr *cm;
	struct rainfo *rai;
	char *raoptp;
	char *p;
	struct in6_addr *addr;
	struct nd_opt_hdr *ndo;
	struct nd_opt_rdnss *rdnss;
	struct nd_opt_dnssl *dnssl;
	size_t len;
	char nsbuf[INET6_ADDRSTRLEN + 1 + IFNAMSIZ + 1];
	char dname[NI_MAXHOST];
	struct timespec now;
	struct timespec lifetime;
	int newent_rai;
	int newent_rao;

	/* get message.  namelen and controllen must always be initialized. */
	rcvmhdr.msg_namelen = sizeof(from);
	rcvmhdr.msg_controllen = rcvcmsglen;
	if ((msglen = recvmsg(s, &rcvmhdr, 0)) < 0) {
		warnmsg(LOG_ERR, __func__, "recvmsg: %s", strerror(errno));
		return;
	}

	/* extract optional information via Advanced API */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rcvmhdr); cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(&rcvmhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
			pi = (struct in6_pktinfo *)(void *)(CMSG_DATA(cm));
			ifindex = pi->ipi6_ifindex;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)(void *)CMSG_DATA(cm);
	}

	if (ifindex == 0) {
		warnmsg(LOG_ERR, __func__,
		    "failed to get receiving interface");
		return;
	}
	if (hlimp == NULL) {
		warnmsg(LOG_ERR, __func__,
		    "failed to get receiving hop limit");
		return;
	}

	if ((size_t)msglen < sizeof(struct nd_router_advert)) {
		warnmsg(LOG_INFO, __func__,
		    "packet size(%zd) is too short", msglen);
		return;
	}

	icp = (struct icmp6_hdr *)rcvmhdr.msg_iov[0].iov_base;

	if (icp->icmp6_type != ND_ROUTER_ADVERT) {
		/*
		 * this should not happen because we configured a filter
		 * that only passes RAs on the receiving socket.
		 */
		warnmsg(LOG_ERR, __func__,
		    "invalid icmp type(%d) from %s on %s", icp->icmp6_type,
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			sizeof(ntopbuf)),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (icp->icmp6_code != 0) {
		warnmsg(LOG_INFO, __func__,
		    "invalid icmp code(%d) from %s on %s", icp->icmp6_code,
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			sizeof(ntopbuf)),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (*hlimp != 255) {
		warnmsg(LOG_INFO, __func__,
		    "invalid RA with hop limit(%d) from %s on %s",
		    *hlimp,
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			sizeof(ntopbuf)),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (pi && !IN6_IS_ADDR_LINKLOCAL(&from.sin6_addr)) {
		warnmsg(LOG_INFO, __func__,
		    "invalid RA with non link-local source from %s on %s",
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			sizeof(ntopbuf)),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/* xxx: more validation? */

	if ((ifi = find_ifinfo(pi->ipi6_ifindex)) == NULL) {
		warnmsg(LOG_DEBUG, __func__,
		    "received RA from %s on an unexpected IF(%s)",
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			sizeof(ntopbuf)),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	warnmsg(LOG_DEBUG, __func__,
	    "received RA from %s on %s, state is %d",
	    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, sizeof(ntopbuf)),
	    ifi->ifname, ifi->state);

	nd_ra = (struct nd_router_advert *)icp;

	/*
	 * Process the "O bit."
	 * If the value of OtherConfigFlag changes from FALSE to TRUE, the
	 * host should invoke the stateful autoconfiguration protocol,
	 * requesting information.
	 * [RFC 2462 Section 5.5.3]
	 */
	if (((nd_ra->nd_ra_flags_reserved) & ND_RA_FLAG_OTHER) &&
	    !ifi->otherconfig) {
		warnmsg(LOG_DEBUG, __func__,
		    "OtherConfigFlag on %s is turned on", ifi->ifname);
		ifi->otherconfig = 1;
		CALL_SCRIPT(OTHER, NULL);
	}
	clock_gettime(CLOCK_MONOTONIC_FAST, &now);
	newent_rai = 0;
	rai = find_rainfo(ifi, &from);
	if (rai == NULL) {
		ELM_MALLOC(rai, exit(1));
		rai->rai_ifinfo = ifi;
		TAILQ_INIT(&rai->rai_ra_opt);
		rai->rai_saddr.sin6_family = AF_INET6;
		rai->rai_saddr.sin6_len = sizeof(rai->rai_saddr);
		memcpy(&rai->rai_saddr.sin6_addr, &from.sin6_addr,
		    sizeof(rai->rai_saddr.sin6_addr));
		newent_rai = 1;
	}

#define	RA_OPT_NEXT_HDR(x)	(struct nd_opt_hdr *)((char *)x + \
				(((struct nd_opt_hdr *)x)->nd_opt_len * 8))
	/* Process RA options. */
	warnmsg(LOG_DEBUG, __func__, "Processing RA");
	raoptp = (char *)icp + sizeof(struct nd_router_advert);
	while (raoptp < (char *)icp + msglen) {
		ndo = (struct nd_opt_hdr *)raoptp;
		warnmsg(LOG_DEBUG, __func__, "ndo = %p", raoptp);
		warnmsg(LOG_DEBUG, __func__, "ndo->nd_opt_type = %d",
		    ndo->nd_opt_type);
		warnmsg(LOG_DEBUG, __func__, "ndo->nd_opt_len = %d",
		    ndo->nd_opt_len);

		switch (ndo->nd_opt_type) {
		case ND_OPT_RDNSS:
			rdnss = (struct nd_opt_rdnss *)raoptp;

			/* Optlen sanity check (Section 5.3.1 in RFC 6106) */
			if (rdnss->nd_opt_rdnss_len < 3) {
				warnmsg(LOG_INFO, __func__,
		    			"too short RDNSS option"
					"in RA from %s was ignored.",
					inet_ntop(AF_INET6, &from.sin6_addr,
					    ntopbuf, sizeof(ntopbuf)));
				break;
			}

			addr = (struct in6_addr *)(void *)(raoptp + sizeof(*rdnss));
			while ((char *)addr < (char *)RA_OPT_NEXT_HDR(raoptp)) {
				if (inet_ntop(AF_INET6, addr, ntopbuf,
					sizeof(ntopbuf)) == NULL) {
					warnmsg(LOG_INFO, __func__,
		    			    "an invalid address in RDNSS option"
					    " in RA from %s was ignored.",
					    inet_ntop(AF_INET6, &from.sin6_addr,
						ntopbuf, sizeof(ntopbuf)));
					addr++;
					continue;
				}
				if (IN6_IS_ADDR_LINKLOCAL(addr))
					/* XXX: % has to be escaped here */
					l = snprintf(nsbuf, sizeof(nsbuf),
					    "%s%c%s", ntopbuf,
					    SCOPE_DELIMITER,
					    ifi->ifname);
				else
					l = snprintf(nsbuf, sizeof(nsbuf),
					    "%s", ntopbuf);
				if (l < 0 || (size_t)l >= sizeof(nsbuf)) {
					warnmsg(LOG_ERR, __func__,
					    "address copying error in "
					    "RDNSS option: %d.", l);
					addr++;
					continue;
				}
				warnmsg(LOG_DEBUG, __func__, "nsbuf = %s",
				    nsbuf);

				newent_rao = 0;
				rao = find_raopt(rai, ndo->nd_opt_type, nsbuf,
				    strlen(nsbuf));
				if (rao == NULL) {
					ELM_MALLOC(rao, break);
					rao->rao_type = ndo->nd_opt_type;
					rao->rao_len = strlen(nsbuf);
					rao->rao_msg = strdup(nsbuf);
					if (rao->rao_msg == NULL) {
						warnmsg(LOG_ERR, __func__,
						    "strdup failed: %s",
						    strerror(errno));
						free(rao);
						addr++;
						continue;
					}
					newent_rao = 1;
				}
				/* Set expiration timer */
				memset(&rao->rao_expire, 0,
				    sizeof(rao->rao_expire));
				memset(&lifetime, 0, sizeof(lifetime));
				lifetime.tv_sec =
				    ntohl(rdnss->nd_opt_rdnss_lifetime);
				TS_ADD(&now, &lifetime, &rao->rao_expire);

				if (newent_rao)
					TAILQ_INSERT_TAIL(&rai->rai_ra_opt,
					    rao, rao_next);
				addr++;
			}
			break;
		case ND_OPT_DNSSL:
			dnssl = (struct nd_opt_dnssl *)raoptp;

			/* Optlen sanity check (Section 5.3.1 in RFC 6106) */
			if (dnssl->nd_opt_dnssl_len < 2) {
				warnmsg(LOG_INFO, __func__,
		    			"too short DNSSL option"
					"in RA from %s was ignored.",
					inet_ntop(AF_INET6, &from.sin6_addr,
					    ntopbuf, sizeof(ntopbuf)));
				break;
			}

			/*
			 * Ensure NUL-termination in DNSSL in case of
			 * malformed field.
			 */
			p = (char *)RA_OPT_NEXT_HDR(raoptp);
			*(p - 1) = '\0';

			p = raoptp + sizeof(*dnssl);
			while (1 < (len = dname_labeldec(dname, sizeof(dname),
			    p))) {
				/* length == 1 means empty string */
				warnmsg(LOG_DEBUG, __func__, "dname = %s",
				    dname);

				newent_rao = 0;
				rao = find_raopt(rai, ndo->nd_opt_type, dname,
				    strlen(dname));
				if (rao == NULL) {
					ELM_MALLOC(rao, break);
					rao->rao_type = ndo->nd_opt_type;
					rao->rao_len = strlen(dname);
					rao->rao_msg = strdup(dname);
					if (rao->rao_msg == NULL) {
						warnmsg(LOG_ERR, __func__,
						    "strdup failed: %s",
						    strerror(errno));
						free(rao);
						addr++;
						continue;
					}
					newent_rao = 1;
				}
				/* Set expiration timer */
				memset(&rao->rao_expire, 0,
				    sizeof(rao->rao_expire));
				memset(&lifetime, 0, sizeof(lifetime));
				lifetime.tv_sec =
				    ntohl(dnssl->nd_opt_dnssl_lifetime);
				TS_ADD(&now, &lifetime, &rao->rao_expire);

				if (newent_rao)
					TAILQ_INSERT_TAIL(&rai->rai_ra_opt,
					    rao, rao_next);
				p += len;
			}
			break;
		default:  
			/* nothing to do for other options */
			break;
		}
		raoptp = (char *)RA_OPT_NEXT_HDR(raoptp);
	}
	if (newent_rai)
		TAILQ_INSERT_TAIL(&ifi->ifi_rainfo, rai, rai_next);

	ra_opt_handler(ifi);
	ifi->racnt++;

	switch (ifi->state) {
	case IFS_IDLE:		/* should be ignored */
	case IFS_DELAY:		/* right? */
		break;
	case IFS_PROBE:
		ifi->state = IFS_IDLE;
		ifi->probes = 0;
		rtsol_timer_update(ifi);
		break;
	}
}

static char resstr_ns_prefix[] = "nameserver ";
static char resstr_sh_prefix[] = "search ";
static char resstr_nl[] = "\n";
static char resstr_sp[] = " ";

int
ra_opt_handler(struct ifinfo *ifi)
{
	struct ra_opt *rao;
	struct rainfo *rai;
	struct script_msg *smp1, *smp2, *smp3;
	struct timespec now;
	struct script_msg_head_t sm_rdnss_head =
	    TAILQ_HEAD_INITIALIZER(sm_rdnss_head);
	struct script_msg_head_t sm_dnssl_head =
	    TAILQ_HEAD_INITIALIZER(sm_dnssl_head);

	int dcount, dlen;

	dcount = 0;
	dlen = strlen(resstr_sh_prefix) + strlen(resstr_nl);
	clock_gettime(CLOCK_MONOTONIC_FAST, &now);

	/*
	 * All options from multiple RAs with the same or different
	 * source addresses on a single interface will be gathered and
	 * handled, not overridden.  [RFC 4861 6.3.4]
	 */
	TAILQ_FOREACH(rai, &ifi->ifi_rainfo, rai_next) {
		TAILQ_FOREACH(rao, &rai->rai_ra_opt, rao_next) {
			switch (rao->rao_type) {
			case ND_OPT_RDNSS:
				if (TS_CMP(&now, &rao->rao_expire, >)) {
					warnmsg(LOG_INFO, __func__,
					    "expired rdnss entry: %s",
					    (char *)rao->rao_msg);
					break;
				}
				ELM_MALLOC(smp1, continue);
				ELM_MALLOC(smp2, goto free1);
				ELM_MALLOC(smp3, goto free2);
				smp1->sm_msg = resstr_ns_prefix;
				TAILQ_INSERT_TAIL(&sm_rdnss_head, smp1,
				    sm_next);
				smp2->sm_msg = rao->rao_msg;
				TAILQ_INSERT_TAIL(&sm_rdnss_head, smp2,
				    sm_next);
				smp3->sm_msg = resstr_nl;
				TAILQ_INSERT_TAIL(&sm_rdnss_head, smp3,
				    sm_next);
				ifi->ifi_rdnss = IFI_DNSOPT_STATE_RECEIVED;

				break;
			case ND_OPT_DNSSL:
				if (TS_CMP(&now, &rao->rao_expire, >)) {
					warnmsg(LOG_INFO, __func__,
					    "expired dnssl entry: %s",
					    (char *)rao->rao_msg);
					break;
				}
				dcount++;
				/* Check resolv.conf(5) restrictions. */
				if (dcount > 6) {
					warnmsg(LOG_INFO, __func__,
					    "dnssl entry exceeding maximum count (%d>6)"
					    ": %s", dcount, (char *)rao->rao_msg);
					break;
				}
				if (256 < dlen + strlen(rao->rao_msg) +
				    strlen(resstr_sp)) {
					warnmsg(LOG_INFO, __func__,
					    "dnssl entry exceeding maximum length "
					    "(>256): %s", (char *)rao->rao_msg);
					break;
				}
				ELM_MALLOC(smp1, continue);
				ELM_MALLOC(smp2, goto free1);
				if (TAILQ_EMPTY(&sm_dnssl_head)) {
					ELM_MALLOC(smp3, goto free2);
					smp3->sm_msg = resstr_sh_prefix;
					TAILQ_INSERT_TAIL(&sm_dnssl_head, smp3,
					    sm_next);
				}
				smp1->sm_msg = rao->rao_msg;
				TAILQ_INSERT_TAIL(&sm_dnssl_head, smp1,
				    sm_next);
				smp2->sm_msg = resstr_sp;
				TAILQ_INSERT_TAIL(&sm_dnssl_head, smp2,
				    sm_next);
				dlen += strlen(rao->rao_msg) +
				    strlen(resstr_sp);
				break;

				ifi->ifi_dnssl = IFI_DNSOPT_STATE_RECEIVED;
			default:
				break;
			}
			continue;
free2:
			free(smp2);
free1:
			free(smp1);
		}
		/* Call the script for each information source. */
		if (uflag)
			ra_opt_rdnss_dispatch(ifi, rai, &sm_rdnss_head,
			    &sm_dnssl_head);
	}
	/* Call the script for each interface. */
	if (!uflag)
		ra_opt_rdnss_dispatch(ifi, NULL, &sm_rdnss_head,
		    &sm_dnssl_head);
	return (0);
}

char *
make_rsid(const char *ifname, const char *origin, struct rainfo *rai)
{
	char hbuf[NI_MAXHOST];
	
	if (rai == NULL)
		sprintf(rsid, "%s:%s", ifname, origin);
	else {
		if (!IN6_IS_ADDR_LINKLOCAL(&rai->rai_saddr.sin6_addr))
			return (NULL);
		if (getnameinfo((struct sockaddr *)&rai->rai_saddr,
			rai->rai_saddr.sin6_len, hbuf, sizeof(hbuf), NULL, 0,
			NI_NUMERICHOST) != 0)
			return (NULL);
		sprintf(rsid, "%s:%s:[%s]", ifname, origin, hbuf);
	}
	warnmsg(LOG_DEBUG, __func__, "rsid = [%s]", rsid);
	return (rsid);
}

int
ra_opt_rdnss_dispatch(struct ifinfo *ifi,
    struct rainfo *rai,
    struct script_msg_head_t *sm_rdnss_head,
    struct script_msg_head_t *sm_dnssl_head)
{
	const char *r;
	struct script_msg *smp1;
	int error;

	error = 0;
	/* Add \n for DNSSL list. */
	if (!TAILQ_EMPTY(sm_dnssl_head)) {
		ELM_MALLOC(smp1, goto ra_opt_rdnss_freeit);
		smp1->sm_msg = resstr_nl;
		TAILQ_INSERT_TAIL(sm_dnssl_head, smp1, sm_next);
	}
	TAILQ_CONCAT(sm_rdnss_head, sm_dnssl_head, sm_next);

	if (rai != NULL && uflag)
		r = make_rsid(ifi->ifname, DNSINFO_ORIGIN_LABEL, rai);
	else
		r = make_rsid(ifi->ifname, DNSINFO_ORIGIN_LABEL, NULL);
	if (r == NULL) {
		warnmsg(LOG_ERR, __func__, "make_rsid() failed.  "
		    "Script was not invoked.");
		error = 1;
		goto ra_opt_rdnss_freeit;
	}
	if (!TAILQ_EMPTY(sm_rdnss_head))
		CALL_SCRIPT(RESADD, sm_rdnss_head);
	else if (ifi->ifi_rdnss == IFI_DNSOPT_STATE_RECEIVED ||
	    ifi->ifi_dnssl == IFI_DNSOPT_STATE_RECEIVED) {
		CALL_SCRIPT(RESDEL, NULL);
		ifi->ifi_rdnss = IFI_DNSOPT_STATE_NOINFO;
		ifi->ifi_dnssl = IFI_DNSOPT_STATE_NOINFO;
	}

ra_opt_rdnss_freeit:
	/* Clear script message queue. */
	if (!TAILQ_EMPTY(sm_rdnss_head)) {
		while ((smp1 = TAILQ_FIRST(sm_rdnss_head)) != NULL) {
			TAILQ_REMOVE(sm_rdnss_head, smp1, sm_next);
			free(smp1);
		}
	}
	if (!TAILQ_EMPTY(sm_dnssl_head)) {
		while ((smp1 = TAILQ_FIRST(sm_dnssl_head)) != NULL) {
			TAILQ_REMOVE(sm_dnssl_head, smp1, sm_next);
			free(smp1);
		}
	}
	return (error);
}

static struct ra_opt *
find_raopt(struct rainfo *rai, int type, void *msg, size_t len)
{
	struct ra_opt *rao;

	TAILQ_FOREACH(rao, &rai->rai_ra_opt, rao_next) {
		if (rao->rao_type == type &&
		    rao->rao_len == strlen(msg) &&
		    memcmp(rao->rao_msg, msg, len) == 0)
			break;
	}

	return (rao);
}

static void
call_script(const int argc, const char *const argv[],
    struct script_msg_head_t *sm_head)
{
	const char *scriptpath;
	int fd[2];
	int error;
	pid_t pid, wpid;

	if ((scriptpath = argv[0]) == NULL)
		return;

	fd[0] = fd[1] = -1;
	if (sm_head != NULL && !TAILQ_EMPTY(sm_head)) {
		error = pipe(fd);
		if (error) {
			warnmsg(LOG_ERR, __func__,
			    "failed to create a pipe: %s", strerror(errno));
			return;
		}
	}

	/* launch the script */
	pid = fork();
	if (pid < 0) {
		warnmsg(LOG_ERR, __func__,
		    "failed to fork: %s", strerror(errno));
		return;
	} else if (pid) {	/* parent */
		int wstatus;

		if (fd[0] != -1) {	/* Send message to the child if any. */
			ssize_t len;
			struct script_msg *smp;

			close(fd[0]);
			TAILQ_FOREACH(smp, sm_head, sm_next) {
				len = strlen(smp->sm_msg);
				warnmsg(LOG_DEBUG, __func__,
				    "write to child = %s(%zd)",
				    smp->sm_msg, len);
				if (write(fd[1], smp->sm_msg, len) != len) {
					warnmsg(LOG_ERR, __func__,
					    "write to child failed: %s",
					    strerror(errno));
					break;
				}
			}
			close(fd[1]);
		}
		do {
			wpid = wait(&wstatus);
		} while (wpid != pid && wpid > 0);

		if (wpid < 0)
			warnmsg(LOG_ERR, __func__,
			    "wait: %s", strerror(errno));
		else
			warnmsg(LOG_DEBUG, __func__,
			    "script \"%s\" terminated", scriptpath);
	} else {		/* child */
		int nullfd;
		char **_argv;

		if (safefile(scriptpath)) {
			warnmsg(LOG_ERR, __func__,
			    "script \"%s\" cannot be executed safely",
			    scriptpath);
			exit(1);
		}
		nullfd = open("/dev/null", O_RDWR);
		if (nullfd < 0) {
			warnmsg(LOG_ERR, __func__,
			    "open /dev/null: %s", strerror(errno));
			exit(1);
		}
		if (fd[0] != -1) {	/* Receive message from STDIN if any. */
			close(fd[1]);
			if (fd[0] != STDIN_FILENO) {
				/* Connect a pipe read-end to child's STDIN. */
				if (dup2(fd[0], STDIN_FILENO) != STDIN_FILENO) {
					warnmsg(LOG_ERR, __func__,
					    "dup2 STDIN: %s", strerror(errno));
					exit(1);
				}
				close(fd[0]);
			}
		} else
			dup2(nullfd, STDIN_FILENO);
	
		dup2(nullfd, STDOUT_FILENO);
		dup2(nullfd, STDERR_FILENO);
		if (nullfd > STDERR_FILENO)
			close(nullfd);

		_argv = malloc(sizeof(*_argv) * argc);
		if (_argv == NULL) {
			warnmsg(LOG_ERR, __func__,
				"malloc: %s", strerror(errno));
			exit(1);
		}
		memcpy(_argv, argv, (size_t)argc);
		execv(scriptpath, (char *const *)_argv);
		warnmsg(LOG_ERR, __func__, "child: exec failed: %s",
		    strerror(errno));
		exit(1);
	}

	return;
}

static int
safefile(const char *path)
{
	struct stat s;
	uid_t myuid;

	/* no setuid */
	if (getuid() != geteuid()) {
		warnmsg(LOG_NOTICE, __func__,
		    "setuid'ed execution not allowed\n");
		return (-1);
	}

	if (lstat(path, &s) != 0) {
		warnmsg(LOG_NOTICE, __func__, "lstat failed: %s",
		    strerror(errno));
		return (-1);
	}

	/* the file must be owned by the running uid */
	myuid = getuid();
	if (s.st_uid != myuid) {
		warnmsg(LOG_NOTICE, __func__,
		    "%s has invalid owner uid\n", path);
		return (-1);
	}

	switch (s.st_mode & S_IFMT) {
	case S_IFREG:
		break;
	default:
		warnmsg(LOG_NOTICE, __func__,
		    "%s is an invalid file type 0x%o\n",
		    path, (s.st_mode & S_IFMT));
		return (-1);
	}

	return (0);
}

/* Decode domain name label encoding in RFC 1035 Section 3.1 */
static size_t
dname_labeldec(char *dst, size_t dlen, const char *src)
{
	size_t len;
	const char *src_origin;
	const char *src_last;
	const char *dst_origin;

	src_origin = src;
	src_last = strchr(src, '\0');
	dst_origin = dst;
	memset(dst, '\0', dlen);
	while (src && (len = (uint8_t)(*src++) & 0x3f) &&
	    (src + len) <= src_last &&
	    (dst - dst_origin < (ssize_t)dlen)) {
		if (dst != dst_origin)
			*dst++ = '.';
		warnmsg(LOG_DEBUG, __func__, "labellen = %zd", len);
		memcpy(dst, src, len);
		src += len;
		dst += len;
	}
	*dst = '\0';

	/*
	 * XXX validate that domain name only contains valid characters
	 * for two reasons: 1) correctness, 2) we do not want to pass
	 * possible malicious, unescaped characters like `` to a script
	 * or program that could be exploited that way.
	 */

	return (src - src_origin);
}
