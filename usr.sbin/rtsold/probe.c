/*
 * Copyright (C) 1998 WIDE Project.
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
 * $FreeBSD: src/usr.sbin/rtsold/probe.c,v 1.2.2.1 2000/07/15 07:37:01 kris Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <net/if.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <net/if_var.h>
#endif /* __FreeBSD__ >= 3 */

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>

#include "rtsold.h"

static struct msghdr sndmhdr;
static struct iovec sndiov[2];
static int probesock;
static void sendprobe __P((struct in6_addr *addr, int ifindex));


int
probe_init()
{
	int scmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
		CMSG_SPACE(sizeof(int));
	static u_char *sndcmsgbuf = NULL;
	
	if (sndcmsgbuf == NULL &&
	    (sndcmsgbuf = (u_char *)malloc(scmsglen)) == NULL) {
		warnmsg(LOG_ERR, __FUNCTION__, "malloc failed");
		return(-1);
	}

	if ((probesock = socket(AF_INET6, SOCK_RAW, IPPROTO_NONE)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "socket: %s", strerror(errno));
		return(-1);
	}

	/* make the socket send-only */
	if (shutdown(probesock, 0)) {
		warnmsg(LOG_ERR, __FUNCTION__, "shutdown: %s", strerror(errno));
		return(-1);
	}

	/* initialize msghdr for sending packets */
	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 1;
	sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
	sndmhdr.msg_controllen = scmsglen;

	return(0);
}

/*
 * Probe if each router in the default router list is still alive. 
 */
void
defrouter_probe(int ifindex)
{
	struct in6_drlist dr;
	int s, i;
	u_char ntopbuf[INET6_ADDRSTRLEN];

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "socket: %s", strerror(errno));
		return;
	}
	bzero(&dr, sizeof(dr));
	strcpy(dr.ifname, "lo0"); /* dummy interface */
	if (ioctl(s, SIOCGDRLST_IN6, (caddr_t)&dr) < 0) {
		warnmsg(LOG_ERR, __FUNCTION__, "ioctl(SIOCGDRLST_IN6): %s",
		       strerror(errno));
		goto closeandend;
	}

	for(i = 0; dr.defrouter[i].if_index && i < PRLSTSIZ; i++) {
		if (ifindex && dr.defrouter[i].if_index == ifindex) {
			/* sanity check */
			if (!IN6_IS_ADDR_LINKLOCAL(&dr.defrouter[i].rtaddr)) {
				warnmsg(LOG_ERR, __FUNCTION__,
					"default router list contains a "
					"non-linklocal address(%s)",
				       inet_ntop(AF_INET6,
						 &dr.defrouter[i].rtaddr,
						 ntopbuf, INET6_ADDRSTRLEN));
				continue; /* ignore the address */
			}
			sendprobe(&dr.defrouter[i].rtaddr,
				  dr.defrouter[i].if_index);
		}
	}

  closeandend:
	close(s);
	return;
}

static void
sendprobe(struct in6_addr *addr, int ifindex)
{
	struct sockaddr_in6 sa6_probe;
	struct in6_pktinfo *pi;
	struct cmsghdr *cm;
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];;

	bzero(&sa6_probe, sizeof(sa6_probe));
	sa6_probe.sin6_family = AF_INET6;
	sa6_probe.sin6_len = sizeof(sa6_probe);
	sa6_probe.sin6_addr = *addr;

	sndmhdr.msg_name = (caddr_t)&sa6_probe;
	sndmhdr.msg_iov[0].iov_base = NULL;
	sndmhdr.msg_iov[0].iov_len = 0;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	/* specify the outgoing interface */
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
	pi->ipi6_ifindex = ifindex;

	/* specify the hop limit of the packet for safety */
	{
		int hoplimit = 1;

		cm = CMSG_NXTHDR(&sndmhdr, cm);
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_HOPLIMIT;
		cm->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));
	}

	warnmsg(LOG_DEBUG, __FUNCTION__, "probe a router %s on %s",
	       inet_ntop(AF_INET6, addr, ntopbuf, INET6_ADDRSTRLEN),
	       if_indextoname(ifindex, ifnamebuf));

	if (sendmsg(probesock, &sndmhdr, 0))
		warnmsg(LOG_ERR, __FUNCTION__, "sendmsg on %s: %s",
			if_indextoname(ifindex, ifnamebuf), strerror(errno));

	return;
}
