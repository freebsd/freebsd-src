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
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <netdb.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
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
struct ifinfo_head_t ifinfo_head =
	TAILQ_HEAD_INITIALIZER(ifinfo_head);

static const struct sockaddr_in6 sin6_allrouters = {
	.sin6_len =	sizeof(sin6_allrouters),
	.sin6_family =	AF_INET6,
	.sin6_addr =	IN6ADDR_LINKLOCAL_ALLROUTERS_INIT,
};

static void call_script(const int, const char *const *, void *);
static size_t dname_labeldec(char *, const char *);
static int safefile(const char *);
static int ra_opt_handler(struct ifinfo *);

#define _ARGS_OTHER	otherconf_script, ifi->ifname
#define _ARGS_RESADD	resolvconf_script, "-a", ifi->ifname
#define _ARGS_RESDEL	resolvconf_script, "-d", ifi->ifname
#define CALL_SCRIPT(name, sm_head)					\
	do {								\
		const char *const sarg[] = { _ARGS_##name, NULL };	\
		call_script(sizeof(sarg), sarg, sm_head);		\
	} while(0);
#define ELM_MALLOC(p,error_action)					\
	do {								\
		p = malloc(sizeof(*p));					\
		if (p == NULL) {					\
			warnmsg(LOG_ERR, __func__, "malloc failed: %s", \
				strerror(errno));			\
			error_action;					\
		}							\
		memset(p, 0, sizeof(*p));				\
	} while(0);

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
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
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
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	int ifindex = 0, *hlimp = NULL;
	ssize_t msglen;
	struct in6_pktinfo *pi = NULL;
	struct ifinfo *ifi = NULL;
	struct ra_opt *rao = NULL;
	struct icmp6_hdr *icp;
	struct nd_router_advert *nd_ra;
	struct cmsghdr *cm;
	char *raoptp;
	char *p;
	struct in6_addr *addr;
	struct nd_opt_hdr *ndo;
	struct nd_opt_rdnss *rdnss;
	struct nd_opt_dnssl *dnssl;
	size_t len;
	char nsbuf[11 + INET6_ADDRSTRLEN + 1 + IFNAMSIZ + 1 + 1];
	/* 11 = sizeof("nameserver "), 1+1 = \n\0 termination */
	char slbuf[7 + NI_MAXHOST + 1 + 1];
	/* 7 = sizeof("search "), 1+1 = \n\0 termination */
	char dname[NI_MAXHOST + 1];
	struct timeval now;
	struct timeval lifetime;

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
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
			ifindex = pi->ipi6_ifindex;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
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
		    INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (icp->icmp6_code != 0) {
		warnmsg(LOG_INFO, __func__,
		    "invalid icmp code(%d) from %s on %s", icp->icmp6_code,
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
		    INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (*hlimp != 255) {
		warnmsg(LOG_INFO, __func__,
		    "invalid RA with hop limit(%d) from %s on %s",
		    *hlimp,
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
		    INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (pi && !IN6_IS_ADDR_LINKLOCAL(&from.sin6_addr)) {
		warnmsg(LOG_INFO, __func__,
		    "invalid RA with non link-local source from %s on %s",
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
		    INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/* xxx: more validation? */

	if ((ifi = find_ifinfo(pi->ipi6_ifindex)) == NULL) {
		warnmsg(LOG_INFO, __func__,
		    "received RA from %s on an unexpected IF(%s)",
		    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
		    INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	warnmsg(LOG_DEBUG, __func__,
	    "received RA from %s on %s, state is %d",
	    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf, INET6_ADDRSTRLEN),
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

#define RA_OPT_NEXT_HDR(x)	(struct nd_opt_hdr *)((char *)x + \
				(((struct nd_opt_hdr *)x)->nd_opt_len * 8))
	raoptp = (char *)icp + sizeof(struct nd_router_advert);

	/* Initialize ra_opt per-interface structure. */
	gettimeofday(&now, NULL);
	if (!TAILQ_EMPTY(&ifi->ifi_ra_opt)) {
		struct ra_opt *rao_tmp;

		rao = TAILQ_FIRST(&ifi->ifi_ra_opt);
		while (rao != NULL) {
			rao_tmp = TAILQ_NEXT(rao, rao_next);
			free(rao);
			rao = rao_tmp;
		}
	} else
		TAILQ_INIT(&ifi->ifi_ra_opt);

	warnmsg(LOG_DEBUG, __func__, "Processing RA");
	/* Process RA options. */
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

			addr = (struct in6_addr *)(raoptp + sizeof(*rdnss));
			while ((char *)addr < (char *)RA_OPT_NEXT_HDR(raoptp)) {
				if (inet_ntop(AF_INET6, addr, ntopbuf,
				    INET6_ADDRSTRLEN) == NULL) {
					warnmsg(LOG_INFO, __func__,
		    			"an invalid address in RDNSS option "
					"in RA from %s was ignored.",
					inet_ntop(AF_INET6, &from.sin6_addr,
					ntopbuf, INET6_ADDRSTRLEN));

					continue;
				}
				if (IN6_IS_ADDR_LINKLOCAL(addr))
					/* XXX: % has to be escaped here */
					sprintf(nsbuf, "%s%c%s",
					    ntopbuf,
					    SCOPE_DELIMITER,
					    ifi->ifname);
				else
					sprintf(nsbuf, "%s", ntopbuf);
				warnmsg(LOG_DEBUG, __func__, "nsbuf = %s",
				    nsbuf);

				ELM_MALLOC(rao, break);
				rao->rao_type = ndo->nd_opt_type;
				rao->rao_len = strlen(nsbuf);
				rao->rao_msg = strdup(nsbuf);
				if (rao->rao_msg == NULL) {
					warnmsg(LOG_ERR, __func__,
					    "strdup failed: %s",
					    strerror(errno));
					free(rao);
					continue;
				}
				/* Set expiration timer */
				memset(&rao->rao_expire, 0, sizeof(rao->rao_expire));
				memset(&lifetime, 0, sizeof(lifetime));
				lifetime.tv_sec = ntohl(rdnss->nd_opt_rdnss_lifetime);
				timeradd(&now, &lifetime, &rao->rao_expire);

				TAILQ_INSERT_TAIL(&ifi->ifi_ra_opt, rao, rao_next);
				addr++;
			}
			break;
		case ND_OPT_DNSSL:
			dnssl = (struct nd_opt_dnssl *)raoptp;

			p = raoptp + sizeof(*dnssl);
			while (0 < (len = dname_labeldec(dname, p))) {
				sprintf(slbuf, "%s ", dname);
				warnmsg(LOG_DEBUG, __func__, "slbuf = %s",
				    slbuf);

				ELM_MALLOC(rao, break);
				rao->rao_type = ndo->nd_opt_type;
				rao->rao_len = strlen(nsbuf);
				rao->rao_msg = strdup(slbuf);
				if (rao->rao_msg == NULL) {
					warnmsg(LOG_ERR, __func__,
					    "strdup failed: %s",
					    strerror(errno));
					free(rao);
					break;
				}
				/* Set expiration timer */
				memset(&rao->rao_expire, 0, sizeof(rao->rao_expire));
				memset(&lifetime, 0, sizeof(lifetime));
				lifetime.tv_sec = ntohl(dnssl->nd_opt_dnssl_lifetime);
				timeradd(&now, &lifetime, &rao->rao_expire);

				TAILQ_INSERT_TAIL(&ifi->ifi_ra_opt, rao, rao_next);
				p += len;
			}
			break;
		default:  
			/* nothing to do for other options */
			break;
		}
		raoptp = (char *)RA_OPT_NEXT_HDR(raoptp);
	}
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

static int
ra_opt_handler(struct ifinfo *ifi)
{
	struct ra_opt *rao;
	struct script_msg *smp;
	struct timeval now;
	TAILQ_HEAD(, script_msg) sm_rdnss_head =
		TAILQ_HEAD_INITIALIZER(sm_rdnss_head);
	TAILQ_HEAD(, script_msg) sm_dnssl_head =
		TAILQ_HEAD_INITIALIZER(sm_dnssl_head);

	gettimeofday(&now, NULL);
	TAILQ_FOREACH(rao, &ifi->ifi_ra_opt, rao_next) {
		switch (rao->rao_type) {
		case ND_OPT_RDNSS:
			if (timercmp(&now, &rao->rao_expire, >)) {
				warnmsg(LOG_INFO, __func__,
					"expired rdnss entry: %s",
					(char *)rao->rao_msg);
				break;
			}
			ELM_MALLOC(smp, continue);
			smp->sm_msg = resstr_ns_prefix;
			TAILQ_INSERT_TAIL(&sm_rdnss_head, smp, sm_next);

			ELM_MALLOC(smp, continue);
			smp->sm_msg = rao->rao_msg;
			TAILQ_INSERT_TAIL(&sm_rdnss_head, smp, sm_next);

			ELM_MALLOC(smp, continue);
			smp->sm_msg = resstr_nl;
			TAILQ_INSERT_TAIL(&sm_rdnss_head, smp, sm_next);

			break;
		case ND_OPT_DNSSL:
			if (timercmp(&now, &rao->rao_expire, >)) {
				warnmsg(LOG_INFO, __func__,
					"expired dnssl entry: %s",
					(char *)rao->rao_msg);
				break;
			}
			if (TAILQ_EMPTY(&sm_dnssl_head)) {
				ELM_MALLOC(smp, continue);
				smp->sm_msg = resstr_sh_prefix;
				TAILQ_INSERT_TAIL(&sm_dnssl_head, smp, sm_next);
			}
			ELM_MALLOC(smp, continue);
			smp->sm_msg = rao->rao_msg;
			TAILQ_INSERT_TAIL(&sm_dnssl_head, smp, sm_next);
			break;
		default:
			break;
		}
	}
	/* Add \n for DNSSL list. */
	if (!TAILQ_EMPTY(&sm_dnssl_head)) {
		ELM_MALLOC(smp, goto ra_opt_handler_freeit);
		smp->sm_msg = resstr_nl;
		TAILQ_INSERT_TAIL(&sm_dnssl_head, smp, sm_next);
	}
	TAILQ_CONCAT(&sm_rdnss_head, &sm_dnssl_head, sm_next);

        if (!TAILQ_EMPTY(&sm_rdnss_head)) {
                CALL_SCRIPT(RESADD, &sm_rdnss_head);
	} else {
                CALL_SCRIPT(RESDEL, NULL);
	}

ra_opt_handler_freeit:
	/* Clear script message queue. */
	if (!TAILQ_EMPTY(&sm_rdnss_head)) {
                struct script_msg *sm_tmp;

		smp = TAILQ_FIRST(&sm_rdnss_head);
		while(smp != NULL) {
			sm_tmp = TAILQ_NEXT(smp, sm_next);
			free(smp);
			smp = sm_tmp;
		}
	}
	return (0);
}

static void
call_script(const int argc, const char *const argv[], void *head)
{
	const char *scriptpath;
	int fd[2];
	int error;
	pid_t pid, wpid;
	TAILQ_HEAD(, script_msg) *sm_head = NULL;

	sm_head = head;
	fd[0] = fd[1] = -1;
	if ((scriptpath = argv[0]) == NULL)
		return;

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
				    "write to child = %s(%d)",
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
		else {
			warnmsg(LOG_DEBUG, __func__,
			    "script \"%s\" terminated", scriptpath);
		}
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
dname_labeldec(char *dst, const char *src)
{
	size_t len;
	const char *src_origin;

	src_origin = src;
	while (*src && (len = (uint8_t)(*src++) & 0x3f) != 0) {
		warnmsg(LOG_DEBUG, __func__, "labellen = %d", len);
		memcpy(dst, src, len);
		src += len;
		dst += len;
		if (*(dst - 1) == '\0')
			break; 
	}

	return (src - src_origin);
}
