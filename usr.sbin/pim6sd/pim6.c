/*
 * Copyright (C) 1999 LSIIT Laboratory.
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
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/pim6.c,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/pim6.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include "mld6.h"
#include "defs.h"
#include "kern.h"
#include "pim6.h"
#include "pimd.h"
#include "pim6_proto.h"
#include "inet6.h"
#include "debug.h"

struct sockaddr_in6 allpim6routers_group;
int pim6_socket;
char *pim6_recv_buf;
char *pim6_send_buf;

static struct sockaddr_in6 from;
static struct iovec sndiovpim[2];
static struct iovec rcviovpim[2];
static struct msghdr 	sndmhpim,
			rcvmhpim;
static u_char *sndcmsgbufpim = NULL;
static int sndcmsglen;
static u_char *rcvcmsgbufpim = NULL;
static int rcvcmsglen;


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
	struct cmsghdr *cmsgp;
	int on;

	if ( (pim6_recv_buf = malloc( RECV_BUF_SIZE)) == NULL ||
		 (pim6_send_buf = malloc (RECV_BUF_SIZE)) == NULL)
		log(LOG_ERR,errno,"pim6 buffer allocation");

	IF_DEBUG(DEBUG_KERN)
		log(LOG_DEBUG,0,"%d octets allocated for the emit/recept buffer pim6",RECV_BUF_SIZE);

	if( (pim6_socket = socket(AF_INET6,SOCK_RAW,IPPROTO_PIM)) < 0 )
		log(LOG_ERR,errno,"pim6_socket");

	k_set_rcvbuf(pim6_socket,SO_RECV_BUF_SIZE_MAX,SO_RECV_BUF_SIZE_MIN);
	k_set_hlim(pim6_socket,MINHLIM);
	k_set_loop(pim6_socket,FALSE);

	memset(&allpim6routers_group, 0, sizeof(allpim6routers_group));
	allpim6routers_group.sin6_len = sizeof(allpim6routers_group);
	allpim6routers_group.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, "ff02::d",
		      (void *)&allpim6routers_group.sin6_addr) != 1 )
		log(LOG_ERR, 0, "inet_pton failed for ff02::d");
	memset(&sockaddr6_d, 0, sizeof(sockaddr6_d));
	sockaddr6_d.sin6_len = sizeof(sockaddr6_d);
	sockaddr6_d.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, "ff00::",
		      (void *)&sockaddr6_d.sin6_addr) != 1)
		log(LOG_ERR, 0, "inet_pton failed for ff00::");

	/* specify to tell receiving interface */
	on = 1;
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(pim6_socket, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
		       sizeof(on)) < 0)
		log(LOG_ERR, errno, "setsockopt(IPV6_RECVPKTINFO)");
#else
	if (setsockopt(pim6_socket, IPPROTO_IPV6, IPV6_PKTINFO, &on,
		       sizeof(on)) < 0)
		log(LOG_ERR, errno, "setsockopt(IPV6_PKTINFO)");
#endif 

	/* initialize msghdr for receiving packets */
	rcviovpim[0].iov_base = (caddr_t) pim6_recv_buf;
	rcviovpim[0].iov_len = RECV_BUF_SIZE;
	rcvmhpim.msg_name = (caddr_t ) &from;
	rcvmhpim.msg_namelen = sizeof (from);
	rcvmhpim.msg_iov = rcviovpim;
	rcvmhpim.msg_iovlen = 1;
	rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo));
	if (rcvcmsgbufpim == NULL &&
	    (rcvcmsgbufpim = malloc(rcvcmsglen)) == NULL)
		log(LOG_ERR, 0, "malloc failed");
	rcvmhpim.msg_control = (caddr_t ) rcvcmsgbufpim;
	rcvmhpim.msg_controllen = rcvcmsglen;

	sndmhpim.msg_namelen=sizeof(struct sockaddr_in6);
	sndmhpim.msg_iov=sndiovpim;
	sndmhpim.msg_iovlen=1;
	sndcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo));
	if (sndcmsgbufpim == NULL &&
	    (sndcmsgbufpim = malloc(sndcmsglen)) == NULL)
		log(LOG_ERR, 0, "malloc failed");
	sndmhpim.msg_control = (caddr_t)sndcmsgbufpim;
	sndmhpim.msg_controllen = sndcmsglen;
	cmsgp=(struct cmsghdr *)sndcmsgbufpim;
	cmsgp->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	cmsgp->cmsg_level = IPPROTO_IPV6;
	cmsgp->cmsg_type = IPV6_PKTINFO;

	if ( register_input_handler(pim6_socket, pim6_read) <0) 
		log(LOG_ERR,0,"Registering pim6 socket");

	/* Initialize the building Join/Prune messages working area */
	build_jp_message_pool = (build_jp_message_t *)NULL;
	build_jp_message_pool_counter = 0;
}

/* Read a PIM message */

static void
pim6_read(f, rfd)
    int f;
    fd_set *rfd;
{
    register int pim6_recvlen;

#ifdef SYSV
    sigset_t block, oblock;
#else
    register int omask;
#endif

	pim6_recvlen = recvmsg(pim6_socket,&rcvmhpim,0);

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
    struct sockaddr_in6 dst;
    struct in6_pktinfo *pi=NULL;
    struct sockaddr_in6 *src = (struct sockaddr_in6 *)rcvmhpim.msg_name;
    struct cmsghdr *cm;	
    int ifindex=0;

    /* sanity check */
    if (pimlen < sizeof(pim)) {
        log(LOG_WARNING, 0,
            "data field too short (%u bytes) for PIM header, from %s",
            pimlen, inet6_fmt(&src->sin6_addr));
        return;
    }
    pim = (struct pim *)rcvmhpim.msg_iov[0].iov_base;

    /* extract vital information via Advanced API */
    for(cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rcvmhpim);
	cm;
	cm =(struct cmsghdr *)CMSG_NXTHDR(&rcvmhpim , cm ))
    {

	    if( cm->cmsg_level == IPPROTO_IPV6 &&
		cm->cmsg_type == IPV6_PKTINFO &&
		cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo)))
	    {
		    pi=(struct in6_pktinfo *)(CMSG_DATA(cm));
		    dst.sin6_addr=pi->ipi6_addr;
		    ifindex = pi->ipi6_ifindex;
		    if (IN6_IS_ADDR_LINKLOCAL(&dst.sin6_addr))
			    dst.sin6_scope_id = ifindex;
		    else
			    dst.sin6_scope_id = 0;
	    }
    }   

    if(pi==NULL)
	    log(LOG_ERR,0,"pim6_socket : unable to get destination packet");

    if(ifindex==0)
	    log(LOG_ERR,0,"pim6_socket : unable to get ifindex");
		
#define NOSUCHDEF 
#ifdef NOSUCHDEF   /* TODO: delete. Too noisy */
    IF_DEBUG(DEBUG_PIM_DETAIL) {
        IF_DEBUG(DEBUG_PIM) {
            log(LOG_DEBUG, 0, "Receiving %s from %s",
                packet_kind(IPPROTO_PIM, pim->pim_type, 0),
                inet6_fmt(&src->sin6_addr));
        }
    }
#endif /* NOSUCHDEF */
    

    /* Check of PIM version is already done in the kernel */
 
    /*
     * TODO: check the dest. is ALL_PIM_ROUTERS (if multicast address)
     *   is it necessary?
     */
    /* Checksum verification is done in the kernel. */

    switch (pim->pim_type) {
     case PIM_HELLO:
         receive_pim6_hello(src, (char *)(pim), pimlen);
         break;
     case PIM_REGISTER:
       	receive_pim6_register(src, &dst, (char *)(pim), pimlen);  
		break;
     case PIM_REGISTER_STOP:
       	 receive_pim6_register_stop(src, &dst, (char *)(pim), pimlen);  
         break;
     case PIM_JOIN_PRUNE:
         receive_pim6_join_prune(src, &dst, (char *)(pim), pimlen);
         break;
     case PIM_BOOTSTRAP:
         receive_pim6_bootstrap(src, &dst, (char *)(pim), pimlen);
         break;
     case PIM_ASSERT:
         receive_pim6_assert(src, &dst, (char *)(pim), pimlen);
         break;
     case PIM_GRAFT:
	 pim6dstat.in_pim6_graft++;
         log(LOG_INFO, 0, "ignore %s from %s",
             packet_kind(IPPROTO_PIM, pim->pim_type, 0),
             inet6_fmt(&src->sin6_addr));
         break;
     case PIM_GRAFT_ACK:
	 pim6dstat.in_pim6_graft_ack++;
         log(LOG_INFO, 0, "ignore %s from %s",
             packet_kind(IPPROTO_PIM, pim->pim_type, 0),
             inet6_fmt(&src->sin6_addr));
         break;
     case PIM_CAND_RP_ADV:
         receive_pim6_cand_rp_adv(src, &dst, (char *)(pim), pimlen);
         break;
     default:
         log(LOG_INFO, 0,
             "ignore unknown PIM message code %u from %s",
             pim->pim_type,
             inet6_fmt(&src->sin6_addr));
         break;
    }
}   

void
send_pim6(char *buf, struct sockaddr_in6 *src,
	  struct sockaddr_in6 *dst, int type, int datalen)
{
	struct pim *pim;
	int setloop=0;
	int ifindex=0;
	int sendlen=sizeof(struct pim)+datalen;
	struct cmsghdr *cmsgp;
	struct in6_pktinfo *sndpktinfo;

	sndiovpim[0].iov_base=(caddr_t)buf;
	sndiovpim[0].iov_len=datalen+sizeof(struct pim);
	cmsgp=(struct cmsghdr *)sndcmsgbufpim;
	sndpktinfo=(struct in6_pktinfo *)CMSG_DATA(cmsgp);
	sndmhpim.msg_name=(caddr_t)dst;

	pim = (struct pim *)buf;
	pim->pim_type = type;
	pim->pim_ver = PIM_PROTOCOL_VERSION;
	pim->pim_rsv = 0;
	pim->pim_cksum = 0;

	if(pim->pim_type == PIM_REGISTER)
	{
		sendlen = sizeof(struct pim)+sizeof(pim_register_t);
		
	}

	pim->pim_cksum = pim6_cksum((u_int16 *)pim,
				    &src->sin6_addr, &dst->sin6_addr,
				    sendlen);

	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr))
	{
		if (!IN6_IS_ADDR_LINKLOCAL(&src->sin6_addr))
		{
			log(LOG_WARNING, 0,
			    "trying to send pim multicast packet "
			    "with non linklocal src(%s), ignoring",
			    inet6_fmt(&src->sin6_addr));
			return;
		}
		sndmhpim.msg_control=NULL;
		sndmhpim.msg_controllen=0;
		ifindex=src->sin6_scope_id;

		k_set_if(pim6_socket , ifindex);
		if( IN6_ARE_ADDR_EQUAL(&dst->sin6_addr,
				       &allnodes_group.sin6_addr) ||
		    IN6_ARE_ADDR_EQUAL(&dst->sin6_addr,
				       &allrouters_group.sin6_addr) ||
		    IN6_ARE_ADDR_EQUAL(&dst->sin6_addr,
				       &allpim6routers_group.sin6_addr))
		{
			setloop=1;
			k_set_loop(pim6_socket, TRUE);
		}
	}
	else
	{
		sndmhpim.msg_control = (caddr_t)sndcmsgbufpim;
		sndmhpim.msg_controllen = sndcmsglen;
		sndpktinfo->ipi6_ifindex=src->sin6_scope_id;
		memcpy(&sndpktinfo->ipi6_addr, &src->sin6_addr,
		       sizeof(sndpktinfo->ipi6_addr));
	}
	if (sendmsg(pim6_socket, &sndmhpim, 0) < 0) {
		if (errno == ENETDOWN)
			check_vif_state();
		else {
			log(LOG_WARNING, errno, "sendmsg from %s to %s",
			    inet6_fmt(&src->sin6_addr),
			    inet6_fmt(&dst->sin6_addr));
		}
	}

	if(setloop)
		k_set_loop(pim6_socket, FALSE);

	return;	
}

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
        u_long  ph_len; 
        u_char  ph_zero[3];
        u_char  ph_nxt; 
    } ph;   
} uph;
    
/*  
 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
 * sequential 16 bit words to it, and at the end, fold back all the 
 * carry bits from the top 16 bits into the lower 16 bits.
 */ 
int pim6_cksum(u_short *addr, struct in6_addr *src ,struct in6_addr *dst , int len )
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
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}   
