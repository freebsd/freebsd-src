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
 */
/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to 
 *  Pavlin Ivanov Radoslavov (pavlin@catarina.usc.edu)
 *
 *  $Id: pim6.c,v 1.6 2000/03/07 02:23:50 jinmei Exp $
 *  $FreeBSD: src/usr.sbin/pim6dd/pim6.c,v 1.1.2.1 2000/07/15 07:36:30 kris Exp $
 */

#include "defs.h"
#include <sys/uio.h>

/*
 * Exported variables.
 */
char	*pim6_recv_buf;		/* input packet buffer   */
char	*pim6_send_buf;		/* output packet buffer  */

struct sockaddr_in6 allpim6routers_group; /* ALL_PIM_ROUTERS group       */
int	pim6_socket;		/* socket for PIM control msgs */

/*
 * Local variables. 
 */
static struct sockaddr_in6 from;
static struct msghdr sndmh;
static struct iovec sndiov[2];
static struct in6_pktinfo *sndpktinfo;
static u_char *sndcmsgbuf = NULL;

/*
 * Local function definitions.
 */
static void pim6_read   __P((int f, fd_set *rfd));
static void accept_pim6 __P((int recvlen));
static int pim6_cksum __P((u_short *, struct in6_addr *,
			   struct in6_addr *, int)); 

void
init_pim6()
{
	static int sndcmsglen;
	struct cmsghdr *cmsgp = (struct cmsghdr *)sndcmsgbuf;

	sndcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo));
	if ((pim6_socket = socket(AF_INET6, SOCK_RAW, IPPROTO_PIM)) < 0) 
		log(LOG_ERR, errno, "PIM6 socket");

	k_set_rcvbuf(pim6_socket, SO_RECV_BUF_SIZE_MAX,
		     SO_RECV_BUF_SIZE_MIN); /* lots of input buffering */
	k_set_hlim(pim6_socket, MINHLIM);  /* restrict multicasts to one hop */
	k_set_loop(pim6_socket, FALSE);	  /* disable multicast loopback     */

	allpim6routers_group.sin6_len = sizeof(allpim6routers_group);
	allpim6routers_group.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, "ff02::d",
		      (void *)&allpim6routers_group.sin6_addr) != 1)
		log(LOG_ERR, 0, "inet_pton failed for ff02::d");
    
	if ((pim6_recv_buf = malloc(RECV_BUF_SIZE)) == NULL ||
	    (pim6_send_buf = malloc(RECV_BUF_SIZE)) == NULL) {
		log(LOG_ERR, 0, "init_pim6: malloc failed\n");
	}

	/* initialize msghdr for sending packets */
	sndmh.msg_namelen = sizeof(struct sockaddr_in6);
	sndmh.msg_iov = sndiov;
	sndmh.msg_iovlen = 1;
	if (sndcmsgbuf == NULL && (sndcmsgbuf = malloc(sndcmsglen)) == NULL)
		log(LOG_ERR, 0, "malloc failed");
	sndmh.msg_control = (caddr_t)sndcmsgbuf;
	sndmh.msg_controllen = sndcmsglen;
	/* initilization cmsg for specifing outgoing interfaces and source */
	cmsgp=(struct cmsghdr *)sndcmsgbuf;
	cmsgp->cmsg_len = CMSG_SPACE(sizeof(struct in6_pktinfo));
	cmsgp->cmsg_level = IPPROTO_IPV6;
	cmsgp->cmsg_type = IPV6_PKTINFO;
	sndpktinfo = (struct in6_pktinfo *)CMSG_DATA(cmsgp);

	if (register_input_handler(pim6_socket, pim6_read) < 0)
		log(LOG_ERR, 0,
		    "cannot register pim6_read() as an input handler");

	IF_ZERO(&nbr_mifs);
}

/* Read a PIM message */
static void
pim6_read(f, rfd)
	int f;
	fd_set *rfd;
{
	register int pim6_recvlen;
	int fromlen = sizeof(from);
#ifdef SYSV
	sigset_t block, oblock;
#else
	register int omask;
#endif

	pim6_recvlen = recvfrom(pim6_socket, pim6_recv_buf, RECV_BUF_SIZE,
			      0, (struct sockaddr *)&from, &fromlen);

	if (pim6_recvlen < 0) {
		if (errno != EINTR)
			log(LOG_ERR, errno, "PIM6 recvmsg");
		return;
	}

#ifdef SYSV
	(void)sigemptyset(&block);
	(void)sigaddset(&block, SIGALRM);
	if (sigprocmask(SIG_BLOCK, &block, &oblock) < 0)
		log(LOG_ERR, errno, "sigprocmask");
#else
	/* Use of omask taken from main() */
	omask = sigblock(sigmask(SIGALRM));
#endif /* SYSV */
    
	accept_pim6(pim6_recvlen);
    
#ifdef SYSV
	(void)sigprocmask(SIG_SETMASK, &oblock, (sigset_t *)NULL);
#else
	(void)sigsetmask(omask);
#endif /* SYSV */
}


static void
accept_pim6(pimlen)
	int pimlen;
{
	register struct pim *pim;
	struct sockaddr_in6 *src = &from;

	/* sanity check */
	if (pimlen < sizeof(pim)) {
		log(LOG_WARNING, 0,
		    "data field too short (%u bytes) for PIM header, from %s", 
		    pimlen, inet6_fmt(&src->sin6_addr));
		return;
	}
	pim = (struct pim *)pim6_recv_buf;

#ifdef NOSUCHDEF   /* TODO: delete. Too noisy */
	IF_DEBUG(DEBUG_PIM_DETAIL) {
		IF_DEBUG(DEBUG_PIM) {
			log(LOG_DEBUG, 0, "Receiving %s from %s",
			    packet_kind(IPPROTO_PIM, pim->pim_type, 0), 
			    inet6_fmt(&src->sin6_addr));
			log(LOG_DEBUG, 0, "PIM type is %u", pim->pim_type);
		}
	}
#endif /* NOSUCHDEF */


	/* Check of PIM version is already done in the kernel */
	/*
	 * TODO: check the dest. is ALL_PIM_ROUTERS (if multicast address)
	 *	 is it necessary?
	 */
	/* Checksum verification is done in the kernel. */

	switch (pim->pim_type) {
	 case PIM_HELLO:
		 receive_pim6_hello(src, (char *)(pim), pimlen); 
		 break;
	 case PIM_REGISTER:
		 log(LOG_INFO, 0, "ignore %s from %s",
		     packet_kind(IPPROTO_PIM, pim->pim_type, 0),
		     inet6_fmt(&src->sin6_addr));
		 break;
	 case PIM_REGISTER_STOP:
		 log(LOG_INFO, 0, "ignore %s from %s",
		     packet_kind(IPPROTO_PIM, pim->pim_type, 0),
		     inet6_fmt(&src->sin6_addr));
		 break;
	 case PIM_JOIN_PRUNE:
		 receive_pim6_join_prune(src, (char *)(pim), pimlen); 
		 break;
	 case PIM_BOOTSTRAP:
		 log(LOG_INFO, 0, "ignore %s from %s",
		     packet_kind(IPPROTO_PIM, pim->pim_type, 0),
		     inet6_fmt(&src->sin6_addr));
		 break;
	 case PIM_ASSERT:
		 receive_pim6_assert(src, (char *)(pim), pimlen); 
		 break;
	 case PIM_GRAFT:
	 case PIM_GRAFT_ACK:
		 receive_pim6_graft(src, (char *)(pim), pimlen, pim->pim_type);
		 break;
	 case PIM_CAND_RP_ADV:
		 log(LOG_INFO, 0, "ignore %s from %s",
		     packet_kind(IPPROTO_PIM, pim->pim_type, 0),
		     inet6_fmt(&src->sin6_addr));
		 break;
	 default:
		 log(LOG_INFO, 0,
		     "ignore unknown PIM message code %u from %s",
		     pim->pim_type,
		     inet6_fmt(&src->sin6_addr));
		 break;
	}
}


/*
 * Send a multicast PIM packet from src to dst, PIM message type = "type"
 * and data length (after the PIM header) = "datalen"
 */
void 
send_pim6(buf, src, dst, type, datalen)
	char *buf;
	struct sockaddr_in6 *src, *dst;
	int type, datalen;
{
	struct pim *pim;
	int setloop = 0;
	int ifindex = 0, sendlen = sizeof(struct pim) + datalen;

	/* Prepare the PIM packet */
	pim = (struct pim *)buf;
	pim->pim_type = type;
	pim->pim_ver = PIM_PROTOCOL_VERSION;
	pim->pim_rsv = 0;
	pim->pim_cksum = 0;
	/*
	 * TODO: XXX: if start using this code for PIM_REGISTERS, exclude the
	 * encapsulated packet from the checksum.
	 */
	pim->pim_cksum = pim6_cksum((u_int16 *)pim,
				    &src->sin6_addr, &dst->sin6_addr,
				    sendlen);

	/*
	 * Specify the source address of the packet. Also, specify the
	 * outgoing interface and the source address if possible.
	 */
	memcpy(&sndpktinfo->ipi6_addr, &src->sin6_addr,
	       sizeof(src->sin6_addr));
	if ((ifindex = src->sin6_scope_id) != 0) {
		sndpktinfo->ipi6_ifindex = ifindex;
	}
	else {
		sndpktinfo->ipi6_ifindex = 0; /* make sure to be cleared */
		log(LOG_WARNING, 0,
		    "send_pim6: could not determine the outgoint IF; send anyway");
	}

	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr)) {
		k_set_if(pim6_socket, ifindex);
		if (IN6_ARE_ADDR_EQUAL(&dst->sin6_addr,
				       &allnodes_group.sin6_addr) ||
		    IN6_ARE_ADDR_EQUAL(&dst->sin6_addr,
				       &allrouters_group.sin6_addr) ||
		    IN6_ARE_ADDR_EQUAL(&dst->sin6_addr,
				       &allpim6routers_group.sin6_addr)) {
			setloop = 1;
			k_set_loop(pim6_socket, TRUE);  
		}
	}

	sndmh.msg_name = (caddr_t)dst;
	sndiov[0].iov_base = (caddr_t)buf;
	sndiov[0].iov_len = sendlen;
	if (sendmsg(pim6_socket, &sndmh, 0) < 0) {
		if (errno == ENETDOWN)
			check_vif_state();
		else
			log(LOG_WARNING, errno, "sendto from %s to %s",
			    inet6_fmt(&src->sin6_addr),
			    inet6_fmt(&dst->sin6_addr));
		if (setloop)
			k_set_loop(pim6_socket, FALSE); 
		return;
	}

	if (setloop)
		k_set_loop(pim6_socket, FALSE); 

	IF_DEBUG(DEBUG_PIM_DETAIL) {
		IF_DEBUG(DEBUG_PIM) {
			char ifname[IFNAMSIZ];

			log(LOG_DEBUG, 0, "SENT %s from %-15s to %s on %s",
			    packet_kind(IPPROTO_PIM, type, 0),
			    inet6_fmt(&src->sin6_addr),
			    inet6_fmt(&dst->sin6_addr),
			    ifindex ? if_indextoname(ifindex, ifname) : "?");
		}
	}
}

u_int pim_send_cnt = 0;
#define SEND_DEBUG_NUMBER 50

/* ============================== */

/*
 * Checksum routine for Internet Protocol family headers (Portable Version).
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 */

#define ADDCARRY(x)  (x > 65535 ? x -= 65535 : x)
#define REDUCE {l_util.l = sum; sum = l_util.s[0] + l_util.s[1]; ADDCARRY(sum);}

static union {
	u_short phs[4];
	struct {
		u_long	ph_len;
		u_char	ph_zero[3];
		u_char	ph_nxt;
	} ph;
} uph;

/*
 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
 * sequential 16 bit words to it, and at the end, fold back all the
 * carry bits from the top 16 bits into the lower 16 bits.
 */
static int
pim6_cksum(addr, src, dst, len)
	u_short *addr;
	struct in6_addr *src, *dst;
	int len;
{
	register int nleft = len;
	register u_short *w;
	register int sum = 0;
	u_short answer = 0;

	/*
	 * First create IP6 pseudo header and calculate a summary.
	 */
	w = (u_short *)src;
	uph.ph.ph_len = htonl(len);
	uph.ph.ph_nxt = IPPROTO_PIM;

	/* IPv6 source address */
	sum += w[0];
	/* XXX: necessary? */
	if (!(IN6_IS_ADDR_LINKLOCAL(src) || IN6_IS_ADDR_MC_LINKLOCAL(src)))
		sum += w[1];
	sum += w[2]; sum += w[3]; sum += w[4]; sum += w[5];
	sum += w[6]; sum += w[7];
	/* IPv6 destination address */
	w = (u_short *)dst;
	sum += w[0];
	/* XXX: necessary? */
	if (!(IN6_IS_ADDR_LINKLOCAL(dst) || IN6_IS_ADDR_MC_LINKLOCAL(dst)))
		sum += w[1];
	sum += w[2]; sum += w[3]; sum += w[4]; sum += w[5];
	sum += w[6]; sum += w[7];
	/* Payload length and upper layer identifier */
	sum += uph.phs[0];  sum += uph.phs[1];
	sum += uph.phs[2];  sum += uph.phs[3];

	/*
	 * Secondly calculate a summary of the first mbuf excluding offset.
	 */
	w = addr;
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}
