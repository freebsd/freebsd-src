/*
 * Copyright (c) 1995 Gordon Ross, Adam Glass
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
 * based on:
 *      nfs/krpc_subr.c
 *	$NetBSD: krpc_subr.c,v 1.10 1995/08/08 20:43:43 gwr Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bootp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <net/if_types.h>
#include <net/if_dl.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsdiskless.h>
#include <nfsclient/krpc.h>
#include <nfs/xdr_subs.h>


#define BOOTP_MIN_LEN		300	/* Minimum size of bootp udp packet */

#ifndef BOOTP_SETTLE_DELAY
#define BOOTP_SETTLE_DELAY 3
#endif

/*
 * What is the longest we will wait before re-sending a request?
 * Note this is also the frequency of "RPC timeout" messages.
 * The re-send loop count sup linearly to this maximum, so the
 * first complaint will happen after (1+2+3+4+5)=15 seconds.
 */
#define	MAX_RESEND_DELAY 5	/* seconds */

/* Definitions from RFC951 */
struct bootp_packet {
	u_int8_t op;
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t hops;
	u_int32_t xid;
	u_int16_t secs;
	u_int16_t flags;
	struct in_addr ciaddr;
	struct in_addr yiaddr;
	struct in_addr siaddr;
	struct in_addr giaddr;
	unsigned char chaddr[16];
	char sname[64];
	char file[128];
	unsigned char vend[1222];
};

struct bootpc_ifcontext {
	struct bootpc_ifcontext *next;
	struct bootp_packet call;
	struct bootp_packet reply;
	int replylen;
	int overload;
	struct socket *so;
	struct ifreq ireq;
	struct ifnet *ifp;
	struct sockaddr_dl *sdl;
	struct sockaddr_in myaddr;
	struct sockaddr_in netmask;
	struct sockaddr_in gw;
	struct sockaddr_in broadcast;	/* Different for each interface */
	int gotgw;
	int gotnetmask;
	int gotrootpath;
	int outstanding;
	int sentmsg;
	u_int32_t xid;
	enum {
		IF_BOOTP_UNRESOLVED,
		IF_BOOTP_RESOLVED,
		IF_BOOTP_FAILED,
		IF_DHCP_UNRESOLVED,
		IF_DHCP_OFFERED,
		IF_DHCP_RESOLVED,
		IF_DHCP_FAILED,
	} state;
	int dhcpquerytype;		/* dhcp type sent */
	struct in_addr dhcpserver;
	int gotdhcpserver;
};

#define TAG_MAXLEN 1024
struct bootpc_tagcontext {
	char buf[TAG_MAXLEN + 1];
	int overload;
	int badopt;
	int badtag;
	int foundopt;
	int taglen;
};

struct bootpc_globalcontext {
	struct bootpc_ifcontext *interfaces;
	struct bootpc_ifcontext *lastinterface;
	u_int32_t xid;
	int gotrootpath;
	int gotgw;
	int ifnum;
	int secs;
	int starttime;
	struct bootp_packet reply;
	int replylen;
	struct bootpc_ifcontext *setrootfs;
	struct bootpc_ifcontext *sethostname;
	struct bootpc_tagcontext tmptag;
	struct bootpc_tagcontext tag;
};

#define IPPORT_BOOTPC 68
#define IPPORT_BOOTPS 67

#define BOOTP_REQUEST 1
#define BOOTP_REPLY 2

/* Common tags */
#define TAG_PAD		  0  /* Pad option, implicit length 1 */
#define TAG_SUBNETMASK	  1  /* RFC 950 subnet mask */
#define TAG_ROUTERS	  3  /* Routers (in order of preference) */
#define TAG_HOSTNAME	 12  /* Client host name */
#define TAG_ROOT	 17  /* Root path */

/* DHCP specific tags */
#define TAG_OVERLOAD	 52  /* Option Overload */
#define TAG_MAXMSGSIZE   57  /* Maximum DHCP Message Size */

#define TAG_END		255  /* End Option (i.e. no more options) */

/* Overload values */
#define OVERLOAD_FILE     1
#define OVERLOAD_SNAME    2

/* Site specific tags: */
#define TAG_ROOTOPTS	130
#define TAG_COOKIE	134	/* ascii info for userland, via sysctl */

#define TAG_DHCP_MSGTYPE 53
#define TAG_DHCP_REQ_ADDR 50
#define TAG_DHCP_SERVERID 54
#define TAG_DHCP_LEASETIME 51

#define TAG_VENDOR_INDENTIFIER 60

#define DHCP_NOMSG    0
#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

static char bootp_cookie[128];
SYSCTL_STRING(_kern, OID_AUTO, bootp_cookie, CTLFLAG_RD,
	bootp_cookie, 0, "Cookie (T134) supplied by bootp server");

/* mountd RPC */
static int	md_mount(struct sockaddr_in *mdsin, char *path, u_char *fhp,
		    int *fhsizep, struct nfs_args *args, struct thread *td);
static int	setfs(struct sockaddr_in *addr, char *path, char *p);
static int	getdec(char **ptr);
static char	*substr(char *a, char *b);
static void	mountopts(struct nfs_args *args, char *p);
static int	xdr_opaque_decode(struct mbuf **ptr, u_char *buf, int len);
static int	xdr_int_decode(struct mbuf **ptr, int *iptr);
static void	print_in_addr(struct in_addr addr);
static void	print_sin_addr(struct sockaddr_in *addr);
static void	clear_sinaddr(struct sockaddr_in *sin);
static void	allocifctx(struct bootpc_globalcontext *gctx);
static void	bootpc_compose_query(struct bootpc_ifcontext *ifctx,
		    struct bootpc_globalcontext *gctx, struct thread *td);
static unsigned char *bootpc_tag(struct bootpc_tagcontext *tctx,
		    struct bootp_packet *bp, int len, int tag);
static void bootpc_tag_helper(struct bootpc_tagcontext *tctx,
		    unsigned char *start, int len, int tag);

#ifdef BOOTP_DEBUG
void bootpboot_p_sa(struct sockaddr *sa, struct sockaddr *ma);
void bootpboot_p_ma(struct sockaddr *ma);
void bootpboot_p_rtentry(struct rtentry *rt);
void bootpboot_p_tree(struct radix_node *rn);
void bootpboot_p_rtlist(void);
void bootpboot_p_if(struct ifnet *ifp, struct ifaddr *ifa);
void bootpboot_p_iflist(void);
#endif

static int	bootpc_call(struct bootpc_globalcontext *gctx,
		    struct thread *td);

static int	bootpc_fakeup_interface(struct bootpc_ifcontext *ifctx,
		    struct bootpc_globalcontext *gctx, struct thread *td);

static int	bootpc_adjust_interface(struct bootpc_ifcontext *ifctx,
		    struct bootpc_globalcontext *gctx, struct thread *td);

static void	bootpc_decode_reply(struct nfsv3_diskless *nd,
		    struct bootpc_ifcontext *ifctx,
		    struct bootpc_globalcontext *gctx);

static int	bootpc_received(struct bootpc_globalcontext *gctx,
		    struct bootpc_ifcontext *ifctx);

static __inline int bootpc_ifctx_isresolved(struct bootpc_ifcontext *ifctx);
static __inline int bootpc_ifctx_isunresolved(struct bootpc_ifcontext *ifctx);
static __inline int bootpc_ifctx_isfailed(struct bootpc_ifcontext *ifctx);

/*
 * In order to have multiple active interfaces with address 0.0.0.0
 * and be able to send data to a selected interface, we perform
 * some tricks:
 *
 *  - The 'broadcast' address is different for each interface.
 *
 *  - We temporarily add routing pointing 255.255.255.255 to the
 *    selected interface broadcast address, thus the packet sent
 *    goes to that interface.
 */

#ifdef BOOTP_DEBUG
void
bootpboot_p_sa(struct sockaddr *sa, struct sockaddr *ma)
{

	if (sa == NULL) {
		printf("(sockaddr *) <null>");
		return;
	}
	switch (sa->sa_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *) sa;
		printf("inet ");
		print_sin_addr(sin);
		if (ma != NULL) {
			sin = (struct sockaddr_in *) ma;
			printf(" mask ");
			print_sin_addr(sin);
		}
	}
	break;
	case AF_LINK:
	{
		struct sockaddr_dl *sli;
		int i;

		sli = (struct sockaddr_dl *) sa;
		printf("link %.*s ", sli->sdl_nlen, sli->sdl_data);
		for (i = 0; i < sli->sdl_alen; i++) {
			if (i > 0)
				printf(":");
			printf("%x", ((unsigned char *) LLADDR(sli))[i]);
		}
	}
	break;
	default:
		printf("af%d", sa->sa_family);
	}
}

void
bootpboot_p_ma(struct sockaddr *ma)
{

	if (ma == NULL) {
		printf("<null>");
		return;
	}
	printf("%x", *(int *)ma);
}

void
bootpboot_p_rtentry(struct rtentry *rt)
{

	bootpboot_p_sa(rt_key(rt), rt_mask(rt));
	printf(" ");
	bootpboot_p_ma(rt->rt_genmask);
	printf(" ");
	bootpboot_p_sa(rt->rt_gateway, NULL);
	printf(" ");
	printf("flags %x", (unsigned short) rt->rt_flags);
	printf(" %d", (int) rt->rt_rmx.rmx_expire);
	printf(" %s\n", rt->rt_ifp->if_xname);
}

void
bootpboot_p_tree(struct radix_node *rn)
{

	while (rn != NULL) {
		if (rn->rn_bit < 0) {
			if ((rn->rn_flags & RNF_ROOT) != 0) {
			} else {
				bootpboot_p_rtentry((struct rtentry *) rn);
			}
			rn = rn->rn_dupedkey;
		} else {
			bootpboot_p_tree(rn->rn_left);
			bootpboot_p_tree(rn->rn_right);
			return;
		}
	}
}

void
bootpboot_p_rtlist(void)
{

	printf("Routing table:\n");
	RADIX_NODE_LOCK(rt_tables[AF_INET]);	/* could sleep XXX */
	bootpboot_p_tree(rt_tables[AF_INET]->rnh_treetop);
	RADIX_NODE_UNLOCK(rt_tables[AF_INET]);
}

void
bootpboot_p_if(struct ifnet *ifp, struct ifaddr *ifa)
{

	printf("%s flags %x, addr ",
	       ifp->if_xname, ifp->if_flags);
	print_sin_addr((struct sockaddr_in *) ifa->ifa_addr);
	printf(", broadcast ");
	print_sin_addr((struct sockaddr_in *) ifa->ifa_dstaddr);
	printf(", netmask ");
	print_sin_addr((struct sockaddr_in *) ifa->ifa_netmask);
	printf("\n");
}

void
bootpboot_p_iflist(void)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;

	printf("Interface list:\n");
	IFNET_RLOCK(); /* could sleep, but okay for debugging XXX */
	for (ifp = TAILQ_FIRST(&ifnet);
	     ifp != NULL;
	     ifp = TAILQ_NEXT(ifp, if_link)) {
		for (ifa = TAILQ_FIRST(&ifp->if_addrhead);
		     ifa != NULL;
		     ifa = TAILQ_NEXT(ifa, ifa_link))
			if (ifa->ifa_addr->sa_family == AF_INET)
				bootpboot_p_if(ifp, ifa);
	}
	IFNET_RUNLOCK();
}
#endif /* defined(BOOTP_DEBUG) */

static void
clear_sinaddr(struct sockaddr_in *sin)
{

	bzero(sin, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = INADDR_ANY; /* XXX: htonl(INAADDR_ANY) ? */
	sin->sin_port = 0;
}

static void
allocifctx(struct bootpc_globalcontext *gctx)
{
	struct bootpc_ifcontext *ifctx;
	ifctx = (struct bootpc_ifcontext *) malloc(sizeof(*ifctx),
						   M_TEMP, M_WAITOK | M_ZERO);
	if (ifctx == NULL)
		panic("Failed to allocate bootp interface context structure");

	ifctx->xid = gctx->xid;
#ifdef BOOTP_NO_DHCP
	ifctx->state = IF_BOOTP_UNRESOLVED;
#else
	ifctx->state = IF_DHCP_UNRESOLVED;
#endif
	gctx->xid += 0x100;
	if (gctx->interfaces != NULL)
		gctx->lastinterface->next = ifctx;
	else
		gctx->interfaces = ifctx;
	gctx->lastinterface = ifctx;
}

static __inline int
bootpc_ifctx_isresolved(struct bootpc_ifcontext *ifctx)
{

	if (ifctx->state == IF_BOOTP_RESOLVED ||
	    ifctx->state == IF_DHCP_RESOLVED)
		return 1;
	return 0;
}

static __inline int
bootpc_ifctx_isunresolved(struct bootpc_ifcontext *ifctx)
{

	if (ifctx->state == IF_BOOTP_UNRESOLVED ||
	    ifctx->state == IF_DHCP_UNRESOLVED)
		return 1;
	return 0;
}

static __inline int
bootpc_ifctx_isfailed(struct bootpc_ifcontext *ifctx)
{

	if (ifctx->state == IF_BOOTP_FAILED ||
	    ifctx->state == IF_DHCP_FAILED)
		return 1;
	return 0;
}

static int
bootpc_received(struct bootpc_globalcontext *gctx,
    struct bootpc_ifcontext *ifctx)
{
	unsigned char dhcpreplytype;
	char *p;

	/*
	 * Need timeout for fallback to less
	 * desirable alternative.
	 */

	/* This call used for the side effect (badopt flag) */
	(void) bootpc_tag(&gctx->tmptag, &gctx->reply,
			  gctx->replylen,
			  TAG_END);

	/* If packet is invalid, ignore it */
	if (gctx->tmptag.badopt != 0)
		return 0;

	p = bootpc_tag(&gctx->tmptag, &gctx->reply,
		       gctx->replylen, TAG_DHCP_MSGTYPE);
	if (p != NULL)
		dhcpreplytype = *p;
	else
		dhcpreplytype = DHCP_NOMSG;

	switch (ifctx->dhcpquerytype) {
	case DHCP_DISCOVER:
		if (dhcpreplytype != DHCP_OFFER 	/* Normal DHCP offer */
#ifndef BOOTP_FORCE_DHCP
		    && dhcpreplytype != DHCP_NOMSG	/* Fallback to BOOTP */
#endif
			)
			return 0;
		break;
	case DHCP_REQUEST:
		if (dhcpreplytype != DHCP_ACK)
			return 0;
	case DHCP_NOMSG:
		break;
	}

	/* Ignore packet unless it gives us a root tag we didn't have */

	if ((ifctx->state == IF_BOOTP_RESOLVED ||
	     (ifctx->dhcpquerytype == DHCP_DISCOVER &&
	      (ifctx->state == IF_DHCP_OFFERED ||
	       ifctx->state == IF_DHCP_RESOLVED))) &&
	    (bootpc_tag(&gctx->tmptag, &ifctx->reply,
			ifctx->replylen,
			TAG_ROOT) != NULL ||
	     bootpc_tag(&gctx->tmptag, &gctx->reply,
			gctx->replylen,
			TAG_ROOT) == NULL))
		return 0;

	bcopy(&gctx->reply, &ifctx->reply, gctx->replylen);
	ifctx->replylen = gctx->replylen;

	/* XXX: Only reset if 'perfect' response */
	if (ifctx->state == IF_BOOTP_UNRESOLVED)
		ifctx->state = IF_BOOTP_RESOLVED;
	else if (ifctx->state == IF_DHCP_UNRESOLVED &&
		 ifctx->dhcpquerytype == DHCP_DISCOVER) {
		if (dhcpreplytype == DHCP_OFFER)
			ifctx->state = IF_DHCP_OFFERED;
		else
			ifctx->state = IF_BOOTP_RESOLVED;	/* Fallback */
	} else if (ifctx->state == IF_DHCP_OFFERED &&
		   ifctx->dhcpquerytype == DHCP_REQUEST)
		ifctx->state = IF_DHCP_RESOLVED;


	if (ifctx->dhcpquerytype == DHCP_DISCOVER &&
	    ifctx->state != IF_BOOTP_RESOLVED) {
		p = bootpc_tag(&gctx->tmptag, &ifctx->reply,
			       ifctx->replylen, TAG_DHCP_SERVERID);
		if (p != NULL && gctx->tmptag.taglen == 4) {
			memcpy(&ifctx->dhcpserver, p, 4);
			ifctx->gotdhcpserver = 1;
		} else
			ifctx->gotdhcpserver = 0;
		return 1;
	}

	ifctx->gotrootpath = (bootpc_tag(&gctx->tmptag, &ifctx->reply,
					 ifctx->replylen,
					 TAG_ROOT) != NULL);
	ifctx->gotgw = (bootpc_tag(&gctx->tmptag, &ifctx->reply,
				   ifctx->replylen,
				   TAG_ROUTERS) != NULL);
	ifctx->gotnetmask = (bootpc_tag(&gctx->tmptag, &ifctx->reply,
					ifctx->replylen,
					TAG_SUBNETMASK) != NULL);
	return 1;
}

static int
bootpc_call(struct bootpc_globalcontext *gctx, struct thread *td)
{
	struct socket *so;
	struct sockaddr_in *sin, dst;
	struct uio auio;
	struct sockopt sopt;
	struct iovec aio;
	int error, on, rcvflg, timo, len;
	time_t atimo;
	time_t rtimo;
	struct timeval tv;
	struct bootpc_ifcontext *ifctx;
	int outstanding;
	int gotrootpath;
	int retry;
	const char *s;

	GIANT_REQUIRED;		/* XXX until socket locking done */

	/*
	 * Create socket and set its recieve timeout.
	 */
	error = socreate(AF_INET, &so, SOCK_DGRAM, 0, td->td_ucred, td);
	if (error != 0)
		goto out0;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	bzero(&sopt, sizeof(sopt));
	sopt.sopt_dir = SOPT_SET;
	sopt.sopt_level = SOL_SOCKET;
	sopt.sopt_name = SO_RCVTIMEO;
	sopt.sopt_val = &tv;
	sopt.sopt_valsize = sizeof tv;

	error = sosetopt(so, &sopt);
	if (error != 0)
		goto out;

	/*
	 * Enable broadcast.
	 */
	on = 1;
	sopt.sopt_name = SO_BROADCAST;
	sopt.sopt_val = &on;
	sopt.sopt_valsize = sizeof on;

	error = sosetopt(so, &sopt);
	if (error != 0)
		goto out;

	/*
	 * Disable routing.
	 */

	on = 1;
	sopt.sopt_name = SO_DONTROUTE;
	sopt.sopt_val = &on;
	sopt.sopt_valsize = sizeof on;

	error = sosetopt(so, &sopt);
	if (error != 0)
		goto out;

	/*
	 * Bind the local endpoint to a bootp client port.
	 */
	sin = &dst;
	clear_sinaddr(sin);
	sin->sin_port = htons(IPPORT_BOOTPC);
	error = sobind(so, (struct sockaddr *)sin, td);
	if (error != 0) {
		printf("bind failed\n");
		goto out;
	}

	/*
	 * Setup socket address for the server.
	 */
	sin = &dst;
	clear_sinaddr(sin);
	sin->sin_addr.s_addr = INADDR_BROADCAST;
	sin->sin_port = htons(IPPORT_BOOTPS);

	/*
	 * Send it, repeatedly, until a reply is received,
	 * but delay each re-send by an increasing amount.
	 * If the delay hits the maximum, start complaining.
	 */
	timo = 0;
	rtimo = 0;
	for (;;) {

		outstanding = 0;
		gotrootpath = 0;

		for (ifctx = gctx->interfaces;
		     ifctx != NULL;
		     ifctx = ifctx->next) {
			if (bootpc_ifctx_isresolved(ifctx) != 0 &&
			    bootpc_tag(&gctx->tmptag, &ifctx->reply,
				       ifctx->replylen,
				       TAG_ROOT) != NULL)
				gotrootpath = 1;
		}

		for (ifctx = gctx->interfaces;
		     ifctx != NULL;
		     ifctx = ifctx->next) {
			ifctx->outstanding = 0;
			if (bootpc_ifctx_isresolved(ifctx)  != 0 &&
			    gotrootpath != 0) {
				continue;
			}
			if (bootpc_ifctx_isfailed(ifctx) != 0)
				continue;

			outstanding++;
			ifctx->outstanding = 1;

			/* Proceed to next step in DHCP negotiation */
			if ((ifctx->state == IF_DHCP_OFFERED &&
			     ifctx->dhcpquerytype != DHCP_REQUEST) ||
			    (ifctx->state == IF_DHCP_UNRESOLVED &&
			     ifctx->dhcpquerytype != DHCP_DISCOVER) ||
			    (ifctx->state == IF_BOOTP_UNRESOLVED &&
			     ifctx->dhcpquerytype != DHCP_NOMSG)) {
				ifctx->sentmsg = 0;
				bootpc_compose_query(ifctx, gctx, td);
			}

			/* Send BOOTP request (or re-send). */

			if (ifctx->sentmsg == 0) {
				switch(ifctx->dhcpquerytype) {
				case DHCP_DISCOVER:
					s = "DHCP Discover";
					break;
				case DHCP_REQUEST:
					s = "DHCP Request";
					break;
				case DHCP_NOMSG:
				default:
					s = "BOOTP Query";
					break;
				}
				printf("Sending %s packet from "
				       "interface %s (%*D)\n",
				       s,
				       ifctx->ireq.ifr_name,
				       ifctx->sdl->sdl_alen,
				       (unsigned char *) LLADDR(ifctx->sdl),
				       ":");
				ifctx->sentmsg = 1;
			}

			aio.iov_base = (caddr_t) &ifctx->call;
			aio.iov_len = sizeof(ifctx->call);

			auio.uio_iov = &aio;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = UIO_WRITE;
			auio.uio_offset = 0;
			auio.uio_resid = sizeof(ifctx->call);
			auio.uio_td = td;

			/* Set netmask to 0.0.0.0 */

			sin = (struct sockaddr_in *) &ifctx->ireq.ifr_addr;
			clear_sinaddr(sin);
			error = ifioctl(ifctx->so, SIOCSIFNETMASK,
					(caddr_t) &ifctx->ireq, td);
			if (error != 0)
				panic("bootpc_call:"
				      "set if netmask, error=%d",
				      error);

			error = sosend(so, (struct sockaddr *) &dst,
				       &auio, NULL, NULL, 0, td);
			if (error != 0) {
				printf("bootpc_call: sosend: %d state %08x\n",
				       error, (int) so->so_state);
			}

			/* XXX: Is this needed ? */
			tsleep(&error, PZERO + 8, "bootpw", 10);

			/* Set netmask to 255.0.0.0 */

			sin = (struct sockaddr_in *) &ifctx->ireq.ifr_addr;
			clear_sinaddr(sin);
			sin->sin_addr.s_addr = htonl(0xff000000u);
			error = ifioctl(ifctx->so, SIOCSIFNETMASK,
					(caddr_t) &ifctx->ireq, td);
			if (error != 0)
				panic("bootpc_call:"
				      "set if netmask, error=%d",
				      error);

		}

		if (outstanding == 0 &&
		    (rtimo == 0 || time_second >= rtimo)) {
			error = 0;
			goto gotreply;
		}

		/* Determine new timeout. */
		if (timo < MAX_RESEND_DELAY)
			timo++;
		else {
			printf("DHCP/BOOTP timeout for server ");
			print_sin_addr(&dst);
			printf("\n");
		}

		/*
		 * Wait for up to timo seconds for a reply.
		 * The socket receive timeout was set to 1 second.
		 */
		atimo = timo + time_second;
		while (time_second < atimo) {
			aio.iov_base = (caddr_t) &gctx->reply;
			aio.iov_len = sizeof(gctx->reply);

			auio.uio_iov = &aio;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = UIO_READ;
			auio.uio_offset = 0;
			auio.uio_resid = sizeof(gctx->reply);
			auio.uio_td = td;

			rcvflg = 0;
			error = soreceive(so, NULL, &auio,
					  NULL, NULL, &rcvflg);
			gctx->secs = time_second - gctx->starttime;
			for (ifctx = gctx->interfaces;
			     ifctx != NULL;
			     ifctx = ifctx->next) {
				if (bootpc_ifctx_isresolved(ifctx) != 0 ||
				    bootpc_ifctx_isfailed(ifctx) != 0)
					continue;

				ifctx->call.secs = htons(gctx->secs);
			}
			if (error == EWOULDBLOCK)
				continue;
			if (error != 0)
				goto out;
			len = sizeof(gctx->reply) - auio.uio_resid;

			/* Do we have the required number of bytes ? */
			if (len < BOOTP_MIN_LEN)
				continue;
			gctx->replylen = len;

			/* Is it a reply? */
			if (gctx->reply.op != BOOTP_REPLY)
				continue;

			/* Is this an answer to our query */
			for (ifctx = gctx->interfaces;
			     ifctx != NULL;
			     ifctx = ifctx->next) {
				if (gctx->reply.xid != ifctx->call.xid)
					continue;

				/* Same HW address size ? */
				if (gctx->reply.hlen != ifctx->call.hlen)
					continue;

				/* Correct HW address ? */
				if (bcmp(gctx->reply.chaddr,
					 ifctx->call.chaddr,
					 ifctx->call.hlen) != 0)
					continue;

				break;
			}

			if (ifctx != NULL) {
				s =  bootpc_tag(&gctx->tmptag,
						&gctx->reply,
						gctx->replylen,
						TAG_DHCP_MSGTYPE);
				if (s != NULL) {
					switch (*s) {
					case DHCP_OFFER:
						s = "DHCP Offer";
						break;
					case DHCP_ACK:
						s = "DHCP Ack";
						break;
					default:
						s = "DHCP (unexpected)";
						break;
					}
				} else
					s = "BOOTP Reply";

				printf("Received %s packet"
				       " on %s from ",
				       s,
				       ifctx->ireq.ifr_name);
				print_in_addr(gctx->reply.siaddr);
				if (gctx->reply.giaddr.s_addr !=
				    htonl(INADDR_ANY)) {
					printf(" via ");
					print_in_addr(gctx->reply.giaddr);
				}
				if (bootpc_received(gctx, ifctx) != 0) {
					printf(" (accepted)");
					if (ifctx->outstanding) {
						ifctx->outstanding = 0;
						outstanding--;
					}
					/* Network settle delay */
					if (outstanding == 0)
						atimo = time_second +
							BOOTP_SETTLE_DELAY;
				} else
					printf(" (ignored)");
				if (ifctx->gotrootpath) {
					gotrootpath = 1;
					rtimo = time_second +
						BOOTP_SETTLE_DELAY;
					printf(" (got root path)");
				} else
					printf(" (no root path)");
				printf("\n");
			}
		} /* while secs */
#ifdef BOOTP_TIMEOUT
		if (gctx->secs > BOOTP_TIMEOUT && BOOTP_TIMEOUT > 0)
			break;
#endif
		/* Force a retry if halfway in DHCP negotiation */
		retry = 0;
		for (ifctx = gctx->interfaces; ifctx != NULL;
		     ifctx = ifctx->next) {
			if (ifctx->state == IF_DHCP_OFFERED) {
				if (ifctx->dhcpquerytype == DHCP_DISCOVER)
					retry = 1;
				else
					ifctx->state = IF_DHCP_UNRESOLVED;
			}
		}

		if (retry != 0)
			continue;

		if (gotrootpath != 0) {
			gctx->gotrootpath = gotrootpath;
			if (rtimo != 0 && time_second >= rtimo)
				break;
		}
	} /* forever send/receive */

	/*
	 * XXX: These are errors of varying seriousness being silently
	 * ignored
	 */

	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = ifctx->next) {
		if (bootpc_ifctx_isresolved(ifctx) == 0) {
			printf("%s timeout for interface %s\n",
			       ifctx->dhcpquerytype != DHCP_NOMSG ?
			       "DHCP" : "BOOTP",
			       ifctx->ireq.ifr_name);
		}
	}
	if (gctx->gotrootpath != 0) {
#if 0
		printf("Got a root path, ignoring remaining timeout\n");
#endif
		error = 0;
		goto out;
	}
#ifndef BOOTP_NFSROOT
	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = ifctx->next) {
		if (bootpc_ifctx_isresolved(ifctx) != 0) {
			error = 0;
			goto out;
		}
	}
#endif
	error = ETIMEDOUT;
	goto out;

gotreply:
out:
	soclose(so);
out0:
	return error;
}

static int
bootpc_fakeup_interface(struct bootpc_ifcontext *ifctx,
    struct bootpc_globalcontext *gctx, struct thread *td)
{
	struct sockaddr_in *sin;
	int error;
	struct ifreq *ireq;
	struct socket *so;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	GIANT_REQUIRED;		/* XXX until socket locking done */

	error = socreate(AF_INET, &ifctx->so, SOCK_DGRAM, 0, td->td_ucred, td);
	if (error != 0)
		panic("nfs_boot: socreate, error=%d", error);

	ireq = &ifctx->ireq;
	so = ifctx->so;

	/*
	 * Bring up the interface.
	 *
	 * Get the old interface flags and or IFF_UP into them; if
	 * IFF_UP set blindly, interface selection can be clobbered.
	 */
	error = ifioctl(so, SIOCGIFFLAGS, (caddr_t)ireq, td);
	if (error != 0)
		panic("bootpc_fakeup_interface: GIFFLAGS, error=%d", error);
	ireq->ifr_flags |= IFF_UP;
	error = ifioctl(so, SIOCSIFFLAGS, (caddr_t)ireq, td);
	if (error != 0)
		panic("bootpc_fakeup_interface: SIFFLAGS, error=%d", error);

	/*
	 * Do enough of ifconfig(8) so that the chosen interface
	 * can talk to the servers.  (just set the address)
	 */

	/* addr is 0.0.0.0 */

	sin = (struct sockaddr_in *) &ireq->ifr_addr;
	clear_sinaddr(sin);
	error = ifioctl(so, SIOCSIFADDR, (caddr_t) ireq, td);
	if (error != 0 && (error != EEXIST || ifctx == gctx->interfaces))
		panic("bootpc_fakeup_interface: "
		      "set if addr, error=%d", error);

	/* netmask is 255.0.0.0 */

	sin = (struct sockaddr_in *) &ireq->ifr_addr;
	clear_sinaddr(sin);
	sin->sin_addr.s_addr = htonl(0xff000000u);
	error = ifioctl(so, SIOCSIFNETMASK, (caddr_t)ireq, td);
	if (error != 0)
		panic("bootpc_fakeup_interface: set if netmask, error=%d",
		      error);

	/* Broadcast is 255.255.255.255 */

	sin = (struct sockaddr_in *)&ireq->ifr_addr;
	clear_sinaddr(sin);
	clear_sinaddr(&ifctx->broadcast);
	sin->sin_addr.s_addr = htonl(INADDR_BROADCAST);
	ifctx->broadcast.sin_addr.s_addr = sin->sin_addr.s_addr;

	error = ifioctl(so, SIOCSIFBRDADDR, (caddr_t)ireq, td);
	if (error != 0)
		panic("bootpc_fakeup_interface: "
		      "set if broadcast addr, error=%d",
		      error);

	/* Get HW address */

	sdl = NULL;
	for (ifa = TAILQ_FIRST(&ifctx->ifp->if_addrhead);
	     ifa != NULL;
	     ifa = TAILQ_NEXT(ifa, ifa_link))
		if (ifa->ifa_addr->sa_family == AF_LINK &&
		    (sdl = ((struct sockaddr_dl *) ifa->ifa_addr)) != NULL &&
		    sdl->sdl_type == IFT_ETHER)
			break;

	if (sdl == NULL)
		panic("bootpc: Unable to find HW address for %s",
		      ifctx->ireq.ifr_name);
	ifctx->sdl = sdl;

	return error;
}


static int
bootpc_adjust_interface(struct bootpc_ifcontext *ifctx,
    struct bootpc_globalcontext *gctx, struct thread *td)
{
	int error;
	struct sockaddr_in defdst;
	struct sockaddr_in defmask;
	struct sockaddr_in *sin;
	struct ifreq *ireq;
	struct socket *so;
	struct sockaddr_in *myaddr;
	struct sockaddr_in *netmask;
	struct sockaddr_in *gw;

	ireq = &ifctx->ireq;
	so = ifctx->so;
	myaddr = &ifctx->myaddr;
	netmask = &ifctx->netmask;
	gw = &ifctx->gw;

	if (bootpc_ifctx_isresolved(ifctx) == 0) {

		/* Shutdown interfaces where BOOTP failed */

		printf("Shutdown interface %s\n", ifctx->ireq.ifr_name);
		error = ifioctl(so, SIOCGIFFLAGS, (caddr_t)ireq, td);
		if (error != 0)
			panic("bootpc_adjust_interface: "
			      "SIOCGIFFLAGS, error=%d", error);
		ireq->ifr_flags &= ~IFF_UP;
		error = ifioctl(so, SIOCSIFFLAGS, (caddr_t)ireq, td);
		if (error != 0)
			panic("bootpc_adjust_interface: "
			      "SIOCSIFFLAGS, error=%d", error);

		sin = (struct sockaddr_in *) &ireq->ifr_addr;
		clear_sinaddr(sin);
		error = ifioctl(so, SIOCDIFADDR, (caddr_t) ireq, td);
		if (error != 0 && (error != EEXIST ||
				   ifctx == gctx->interfaces))
			panic("bootpc_adjust_interface: "
			      "SIOCDIFADDR, error=%d", error);

		return 0;
	}

	printf("Adjusted interface %s\n", ifctx->ireq.ifr_name);
	/*
	 * Do enough of ifconfig(8) so that the chosen interface
	 * can talk to the servers.  (just set the address)
	 */
	bcopy(netmask, &ireq->ifr_addr, sizeof(*netmask));
	error = ifioctl(so, SIOCSIFNETMASK, (caddr_t) ireq, td);
	if (error != 0)
		panic("bootpc_adjust_interface: "
		      "set if netmask, error=%d", error);

	/* Broadcast is with host part of IP address all 1's */

	sin = (struct sockaddr_in *) &ireq->ifr_addr;
	clear_sinaddr(sin);
	sin->sin_addr.s_addr = myaddr->sin_addr.s_addr |
		~ netmask->sin_addr.s_addr;
	error = ifioctl(so, SIOCSIFBRDADDR, (caddr_t) ireq, td);
	if (error != 0)
		panic("bootpc_adjust_interface: "
		      "set if broadcast addr, error=%d", error);

	bcopy(myaddr, &ireq->ifr_addr, sizeof(*myaddr));
	error = ifioctl(so, SIOCSIFADDR, (caddr_t) ireq, td);
	if (error != 0 && (error != EEXIST || ifctx == gctx->interfaces))
		panic("bootpc_adjust_interface: "
		      "set if addr, error=%d", error);

	/* Add new default route */

	if (ifctx->gotgw != 0 || gctx->gotgw == 0) {
		clear_sinaddr(&defdst);
		clear_sinaddr(&defmask);
		error = rtrequest(RTM_ADD,
				  (struct sockaddr *) &defdst,
				  (struct sockaddr *) gw,
				  (struct sockaddr *) &defmask,
				  (RTF_UP | RTF_GATEWAY | RTF_STATIC), NULL);
		if (error != 0) {
			printf("bootpc_adjust_interface: "
			       "add net route, error=%d\n", error);
			return error;
		}
	}

	return 0;
}

static int
setfs(struct sockaddr_in *addr, char *path, char *p)
{
	unsigned int ip;
	int val;

	ip = 0;
	if (((val = getdec(&p)) < 0) || (val > 255))
		return 0;
	ip = val << 24;
	if (*p != '.')
		return 0;
	p++;
	if (((val = getdec(&p)) < 0) || (val > 255))
		return 0;
	ip |= (val << 16);
	if (*p != '.')
		return 0;
	p++;
	if (((val = getdec(&p)) < 0) || (val > 255))
		return 0;
	ip |= (val << 8);
	if (*p != '.')
		return 0;
	p++;
	if (((val = getdec(&p)) < 0) || (val > 255))
		return 0;
	ip |= val;
	if (*p != ':')
		return 0;
	p++;

	addr->sin_addr.s_addr = htonl(ip);
	addr->sin_len = sizeof(struct sockaddr_in);
	addr->sin_family = AF_INET;

	strncpy(path, p, MNAMELEN - 1);
	return 1;
}

static int
getdec(char **ptr)
{
	char *p;
	int ret;

	p = *ptr;
	ret = 0;
	if ((*p < '0') || (*p > '9'))
		return -1;
	while ((*p >= '0') && (*p <= '9')) {
		ret = ret * 10 + (*p - '0');
		p++;
	}
	*ptr = p;
	return ret;
}

static char *
substr(char *a, char *b)
{
	char *loc1;
	char *loc2;

        while (*a != '\0') {
                loc1 = a;
                loc2 = b;
                while (*loc1 == *loc2++) {
                        if (*loc1 == '\0')
				return 0;
                        loc1++;
                        if (*loc2 == '\0')
				return loc1;
                }
		a++;
        }
        return 0;
}

static void
mountopts(struct nfs_args *args, char *p)
{
	char *tmp;

	args->version = NFS_ARGSVERSION;
	args->rsize = 8192;
	args->wsize = 8192;
	args->flags = NFSMNT_RSIZE | NFSMNT_WSIZE | NFSMNT_RESVPORT;
	args->sotype = SOCK_DGRAM;
	if (p == NULL)
		return;
	if ((tmp = (char *)substr(p, "rsize=")))
		args->rsize = getdec(&tmp);
	if ((tmp = (char *)substr(p, "wsize=")))
		args->wsize = getdec(&tmp);
	if ((tmp = (char *)substr(p, "intr")))
		args->flags |= NFSMNT_INT;
	if ((tmp = (char *)substr(p, "soft")))
		args->flags |= NFSMNT_SOFT;
	if ((tmp = (char *)substr(p, "noconn")))
		args->flags |= NFSMNT_NOCONN;
	if ((tmp = (char *)substr(p, "tcp")))
		args->sotype = SOCK_STREAM;
}

static int
xdr_opaque_decode(struct mbuf **mptr, u_char *buf, int len)
{
	struct mbuf *m;
	int alignedlen;

	m = *mptr;
	alignedlen = ( len + 3 ) & ~3;

	if (m->m_len < alignedlen) {
		m = m_pullup(m, alignedlen);
		if (m == NULL) {
			*mptr = NULL;
			return EBADRPC;
		}
	}
	bcopy(mtod(m, u_char *), buf, len);
	m_adj(m, alignedlen);
	*mptr = m;
	return 0;
}

static int
xdr_int_decode(struct mbuf **mptr, int *iptr)
{
	u_int32_t i;

	if (xdr_opaque_decode(mptr, (u_char *) &i, sizeof(u_int32_t)) != 0)
		return EBADRPC;
	*iptr = fxdr_unsigned(u_int32_t, i);
	return 0;
}

static void
print_sin_addr(struct sockaddr_in *sin)
{

	print_in_addr(sin->sin_addr);
}

static void
print_in_addr(struct in_addr addr)
{
	unsigned int ip;

	ip = ntohl(addr.s_addr);
	printf("%d.%d.%d.%d",
	       ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

static void
bootpc_compose_query(struct bootpc_ifcontext *ifctx,
    struct bootpc_globalcontext *gctx, struct thread *td)
{
	unsigned char *vendp;
	unsigned char vendor_client[64];
	uint32_t leasetime;
	uint8_t vendor_client_len;

	ifctx->gotrootpath = 0;

	bzero((caddr_t) &ifctx->call, sizeof(ifctx->call));

	/* bootpc part */
	ifctx->call.op = BOOTP_REQUEST; 	/* BOOTREQUEST */
	ifctx->call.htype = 1;			/* 10mb ethernet */
	ifctx->call.hlen = ifctx->sdl->sdl_alen;/* Hardware address length */
	ifctx->call.hops = 0;
	if (bootpc_ifctx_isunresolved(ifctx) != 0)
		ifctx->xid++;
	ifctx->call.xid = txdr_unsigned(ifctx->xid);
	bcopy(LLADDR(ifctx->sdl), &ifctx->call.chaddr, ifctx->sdl->sdl_alen);

	vendp = ifctx->call.vend;
	*vendp++ = 99;		/* RFC1048 cookie */
	*vendp++ = 130;
	*vendp++ = 83;
	*vendp++ = 99;
	*vendp++ = TAG_MAXMSGSIZE;
	*vendp++ = 2;
	*vendp++ = (sizeof(struct bootp_packet) >> 8) & 255;
	*vendp++ = sizeof(struct bootp_packet) & 255;

	snprintf(vendor_client, sizeof(vendor_client), "%s:%s:%s",
		ostype, MACHINE, osrelease);
	vendor_client_len = strlen(vendor_client);
	*vendp++ = TAG_VENDOR_INDENTIFIER;
	*vendp++ = vendor_client_len;
	memcpy(vendp, vendor_client, vendor_client_len);
	vendp += vendor_client_len;;
	ifctx->dhcpquerytype = DHCP_NOMSG;
	switch (ifctx->state) {
	case IF_DHCP_UNRESOLVED:
		*vendp++ = TAG_DHCP_MSGTYPE;
		*vendp++ = 1;
		*vendp++ = DHCP_DISCOVER;
		ifctx->dhcpquerytype = DHCP_DISCOVER;
		ifctx->gotdhcpserver = 0;
		break;
	case IF_DHCP_OFFERED:
		*vendp++ = TAG_DHCP_MSGTYPE;
		*vendp++ = 1;
		*vendp++ = DHCP_REQUEST;
		ifctx->dhcpquerytype = DHCP_REQUEST;
		*vendp++ = TAG_DHCP_REQ_ADDR;
		*vendp++ = 4;
		memcpy(vendp, &ifctx->reply.yiaddr, 4);
		vendp += 4;
		if (ifctx->gotdhcpserver != 0) {
			*vendp++ = TAG_DHCP_SERVERID;
			*vendp++ = 4;
			memcpy(vendp, &ifctx->dhcpserver, 4);
			vendp += 4;
		}
		*vendp++ = TAG_DHCP_LEASETIME;
		*vendp++ = 4;
		leasetime = htonl(300);
		memcpy(vendp, &leasetime, 4);
		vendp += 4;
		break;
	default:
		break;
	}
	*vendp = TAG_END;

	ifctx->call.secs = 0;
	ifctx->call.flags = htons(0x8000); /* We need a broadcast answer */
}

static int
bootpc_hascookie(struct bootp_packet *bp)
{

	return (bp->vend[0] == 99 && bp->vend[1] == 130 &&
		bp->vend[2] == 83 && bp->vend[3] == 99);
}

static void
bootpc_tag_helper(struct bootpc_tagcontext *tctx,
    unsigned char *start, int len, int tag)
{
	unsigned char *j;
	unsigned char *ej;
	unsigned char code;

	if (tctx->badtag != 0 || tctx->badopt != 0)
		return;

	j = start;
	ej = j + len;

	while (j < ej) {
		code = *j++;
		if (code == TAG_PAD)
			continue;
		if (code == TAG_END)
			return;
		if (j >= ej || j + *j + 1 > ej) {
			tctx->badopt = 1;
			return;
		}
		len = *j++;
		if (code == tag) {
			if (tctx->taglen + len > TAG_MAXLEN) {
				tctx->badtag = 1;
				return;
			}
			tctx->foundopt = 1;
			if (len > 0)
				memcpy(tctx->buf + tctx->taglen,
				       j, len);
			tctx->taglen += len;
		}
		if (code == TAG_OVERLOAD)
			tctx->overload = *j;

		j += len;
	}
}

static unsigned char *
bootpc_tag(struct bootpc_tagcontext *tctx,
    struct bootp_packet *bp, int len, int tag)
{
	tctx->overload = 0;
	tctx->badopt = 0;
	tctx->badtag = 0;
	tctx->foundopt = 0;
	tctx->taglen = 0;

	if (bootpc_hascookie(bp) == 0)
		return NULL;

	bootpc_tag_helper(tctx, &bp->vend[4],
			  (unsigned char *) bp + len - &bp->vend[4], tag);

	if ((tctx->overload & OVERLOAD_FILE) != 0)
		bootpc_tag_helper(tctx,
				  (unsigned char *) bp->file,
				  sizeof(bp->file),
				  tag);
	if ((tctx->overload & OVERLOAD_SNAME) != 0)
		bootpc_tag_helper(tctx,
				  (unsigned char *) bp->sname,
				  sizeof(bp->sname),
				  tag);

	if (tctx->badopt != 0 || tctx->badtag != 0 || tctx->foundopt == 0)
		return NULL;
	tctx->buf[tctx->taglen] = '\0';
	return tctx->buf;
}

static void
bootpc_decode_reply(struct nfsv3_diskless *nd, struct bootpc_ifcontext *ifctx,
    struct bootpc_globalcontext *gctx)
{
	char *p;
	unsigned int ip;

	ifctx->gotgw = 0;
	ifctx->gotnetmask = 0;

	clear_sinaddr(&ifctx->myaddr);
	clear_sinaddr(&ifctx->netmask);
	clear_sinaddr(&ifctx->gw);

	ifctx->myaddr.sin_addr = ifctx->reply.yiaddr;

	ip = ntohl(ifctx->myaddr.sin_addr.s_addr);

	printf("%s at ", ifctx->ireq.ifr_name);
	print_sin_addr(&ifctx->myaddr);
	printf(" server ");
	print_in_addr(ifctx->reply.siaddr);

	ifctx->gw.sin_addr = ifctx->reply.giaddr;
	if (ifctx->reply.giaddr.s_addr != htonl(INADDR_ANY)) {
		printf(" via gateway ");
		print_in_addr(ifctx->reply.giaddr);
	}

	/* This call used for the side effect (overload flag) */
	(void) bootpc_tag(&gctx->tmptag,
			  &ifctx->reply, ifctx->replylen, TAG_END);

	if ((gctx->tmptag.overload & OVERLOAD_SNAME) == 0)
		if (ifctx->reply.sname[0] != '\0')
			printf(" server name %s", ifctx->reply.sname);
	if ((gctx->tmptag.overload & OVERLOAD_FILE) == 0)
		if (ifctx->reply.file[0] != '\0')
			printf(" boot file %s", ifctx->reply.file);

	printf("\n");

	p = bootpc_tag(&gctx->tag, &ifctx->reply, ifctx->replylen,
		       TAG_SUBNETMASK);
	if (p != NULL) {
		if (gctx->tag.taglen != 4)
			panic("bootpc: subnet mask len is %d",
			      gctx->tag.taglen);
		bcopy(p, &ifctx->netmask.sin_addr, 4);
		ifctx->gotnetmask = 1;
		printf("subnet mask ");
		print_sin_addr(&ifctx->netmask);
		printf(" ");
	}

	p = bootpc_tag(&gctx->tag, &ifctx->reply, ifctx->replylen,
		       TAG_ROUTERS);
	if (p != NULL) {
		/* Routers */
		if (gctx->tag.taglen % 4)
			panic("bootpc: Router Len is %d", gctx->tag.taglen);
		if (gctx->tag.taglen > 0) {
			bcopy(p, &ifctx->gw.sin_addr, 4);
			printf("router ");
			print_sin_addr(&ifctx->gw);
			printf(" ");
			ifctx->gotgw = 1;
			gctx->gotgw = 1;
		}
	}

	p = bootpc_tag(&gctx->tag, &ifctx->reply, ifctx->replylen,
		       TAG_ROOT);
	if (p != NULL) {
		if (gctx->setrootfs != NULL) {
			printf("rootfs %s (ignored) ", p);
		} else 	if (setfs(&nd->root_saddr,
				  nd->root_hostnam, p)) {
			printf("rootfs %s ", p);
			gctx->gotrootpath = 1;
			ifctx->gotrootpath = 1;
			gctx->setrootfs = ifctx;

			p = bootpc_tag(&gctx->tag, &ifctx->reply,
				       ifctx->replylen,
				       TAG_ROOTOPTS);
			if (p != NULL) {
				mountopts(&nd->root_args, p);
				printf("rootopts %s ", p);
			}
		} else
			panic("Failed to set rootfs to %s", p);
	}

	p = bootpc_tag(&gctx->tag, &ifctx->reply, ifctx->replylen,
		       TAG_HOSTNAME);
	if (p != NULL) {
		if (gctx->tag.taglen >= MAXHOSTNAMELEN)
			panic("bootpc: hostname >= %d bytes",
			      MAXHOSTNAMELEN);
		if (gctx->sethostname != NULL) {
			printf("hostname %s (ignored) ", p);
		} else {
			strcpy(nd->my_hostnam, p);
			strcpy(hostname, p);
			printf("hostname %s ", hostname);
			gctx->sethostname = ifctx;
		}
	}
	p = bootpc_tag(&gctx->tag, &ifctx->reply, ifctx->replylen,
			TAG_COOKIE);
	if (p != NULL) {        /* store in a sysctl variable */
		int i, l = sizeof(bootp_cookie) - 1;
		for (i = 0; i < l && p[i] != '\0'; i++)
			bootp_cookie[i] = p[i];
		p[i] = '\0';
	}


	printf("\n");

	if (ifctx->gotnetmask == 0) {
		if (IN_CLASSA(ntohl(ifctx->myaddr.sin_addr.s_addr)))
			ifctx->netmask.sin_addr.s_addr = htonl(IN_CLASSA_NET);
		else if (IN_CLASSB(ntohl(ifctx->myaddr.sin_addr.s_addr)))
			ifctx->netmask.sin_addr.s_addr = htonl(IN_CLASSB_NET);
		else
			ifctx->netmask.sin_addr.s_addr = htonl(IN_CLASSC_NET);
	}
	if (ifctx->gotgw == 0) {
		/* Use proxyarp */
		ifctx->gw.sin_addr.s_addr = ifctx->myaddr.sin_addr.s_addr;
	}
}

void
bootpc_init(void)
{
	struct bootpc_ifcontext *ifctx, *nctx;	/* Interface BOOTP contexts */
	struct bootpc_globalcontext *gctx; 	/* Global BOOTP context */
	struct ifnet *ifp;
	int error;
#ifndef BOOTP_WIRED_TO
	int ifcnt;
#endif
	struct nfsv3_diskless *nd;
	struct thread *td;

	nd = &nfsv3_diskless;
	td = curthread;

	/*
	 * If already filled in, don't touch it here
	 */
	if (nfs_diskless_valid != 0)
		return;

	gctx = malloc(sizeof(*gctx), M_TEMP, M_WAITOK | M_ZERO);
	if (gctx == NULL)
		panic("Failed to allocate bootp global context structure");

	gctx->xid = ~0xFFFF;
	gctx->starttime = time_second;

	/*
	 * Find a network interface.
	 */
#ifdef BOOTP_WIRED_TO
	printf("bootpc_init: wired to interface '%s'\n",
	       __XSTRING(BOOTP_WIRED_TO));
	allocifctx(gctx);
#else
	/*
	 * Preallocate interface context storage, if another interface
	 * attaches and wins the race, it won't be eligible for bootp.
	 */
	IFNET_RLOCK();
	for (ifp = TAILQ_FIRST(&ifnet), ifcnt = 0;
	     ifp != NULL;
	     ifp = TAILQ_NEXT(ifp, if_link)) {
		if ((ifp->if_flags &
		     (IFF_LOOPBACK | IFF_POINTOPOINT | IFF_BROADCAST)) !=
		    IFF_BROADCAST)
			continue;
		ifcnt++;
	}
	IFNET_RUNLOCK();
	if (ifcnt == 0)
		panic("bootpc_init: no eligible interfaces");
	for (; ifcnt > 0; ifcnt--)
		allocifctx(gctx);
#endif

	IFNET_RLOCK();
	for (ifp = TAILQ_FIRST(&ifnet), ifctx = gctx->interfaces;
	     ifp != NULL && ifctx != NULL;
	     ifp = TAILQ_NEXT(ifp, if_link)) {
		strlcpy(ifctx->ireq.ifr_name, ifp->if_xname,
		    sizeof(ifctx->ireq.ifr_name));
#ifdef BOOTP_WIRED_TO
		if (strcmp(ifctx->ireq.ifr_name,
			   __XSTRING(BOOTP_WIRED_TO)) != 0)
			continue;
#else
		if ((ifp->if_flags &
		     (IFF_LOOPBACK | IFF_POINTOPOINT | IFF_BROADCAST)) !=
		    IFF_BROADCAST)
			continue;
#endif
		ifctx->ifp = ifp;
		ifctx = ifctx->next;
	}
	IFNET_RUNLOCK();

	if (gctx->interfaces == NULL || gctx->interfaces->ifp == NULL) {
#ifdef BOOTP_WIRED_TO
		panic("bootpc_init: Could not find interface specified "
		      "by BOOTP_WIRED_TO: "
		      __XSTRING(BOOTP_WIRED_TO));
#else
		panic("bootpc_init: no suitable interface");
#endif
	}

	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = ifctx->next)
		bootpc_fakeup_interface(ifctx, gctx, td);

	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = ifctx->next)
		bootpc_compose_query(ifctx, gctx, td);

	error = bootpc_call(gctx, td);

	if (error != 0) {
#ifdef BOOTP_NFSROOT
		panic("BOOTP call failed");
#else
		printf("BOOTP call failed\n");
#endif
	}

	mountopts(&nd->root_args, NULL);

	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = ifctx->next)
		if (bootpc_ifctx_isresolved(ifctx) != 0)
			bootpc_decode_reply(nd, ifctx, gctx);

#ifdef BOOTP_NFSROOT
	if (gctx->gotrootpath == 0)
		panic("bootpc: No root path offered");
#endif

	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = ifctx->next) {
		bootpc_adjust_interface(ifctx, gctx, td);

		soclose(ifctx->so);
	}

	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = ifctx->next)
		if (ifctx->gotrootpath != 0)
			break;
	if (ifctx == NULL) {
		for (ifctx = gctx->interfaces;
		     ifctx != NULL;
		     ifctx = ifctx->next)
			if (bootpc_ifctx_isresolved(ifctx) != 0)
				break;
	}
	if (ifctx == NULL)
		goto out;

	if (gctx->gotrootpath != 0) {

		error = md_mount(&nd->root_saddr, nd->root_hostnam,
				 nd->root_fh, &nd->root_fhsize,
				 &nd->root_args, td);
		if (error != 0)
			panic("nfs_boot: mountd root, error=%d", error);

		nfs_diskless_valid = 3;
	}

	strcpy(nd->myif.ifra_name, ifctx->ireq.ifr_name);
	bcopy(&ifctx->myaddr, &nd->myif.ifra_addr, sizeof(ifctx->myaddr));
	bcopy(&ifctx->myaddr, &nd->myif.ifra_broadaddr, sizeof(ifctx->myaddr));
	((struct sockaddr_in *) &nd->myif.ifra_broadaddr)->sin_addr.s_addr =
		ifctx->myaddr.sin_addr.s_addr |
		~ ifctx->netmask.sin_addr.s_addr;
	bcopy(&ifctx->netmask, &nd->myif.ifra_mask, sizeof(ifctx->netmask));

out:
	for (ifctx = gctx->interfaces; ifctx != NULL; ifctx = nctx) {
		nctx = ifctx->next;
		free(ifctx, M_TEMP);
	}
	free(gctx, M_TEMP);
}

/*
 * RPC: mountd/mount
 * Given a server pathname, get an NFS file handle.
 * Also, sets sin->sin_port to the NFS service port.
 */
static int
md_mount(struct sockaddr_in *mdsin, char *path, u_char *fhp, int *fhsizep,
    struct nfs_args *args, struct thread *td)
{
	struct mbuf *m;
	int error;
	int authunixok;
	int authcount;
	int authver;

#ifdef BOOTP_NFSV3
	/* First try NFS v3 */
	/* Get port number for MOUNTD. */
	error = krpc_portmap(mdsin, RPCPROG_MNT, RPCMNT_VER3,
			     &mdsin->sin_port, td);
	if (error == 0) {
		m = xdr_string_encode(path, strlen(path));

		/* Do RPC to mountd. */
		error = krpc_call(mdsin, RPCPROG_MNT, RPCMNT_VER3,
				  RPCMNT_MOUNT, &m, NULL, td);
	}
	if (error == 0) {
		args->flags |= NFSMNT_NFSV3;
	} else {
#endif
		/* Fallback to NFS v2 */

		/* Get port number for MOUNTD. */
		error = krpc_portmap(mdsin, RPCPROG_MNT, RPCMNT_VER1,
				     &mdsin->sin_port, td);
		if (error != 0)
			return error;

		m = xdr_string_encode(path, strlen(path));

		/* Do RPC to mountd. */
		error = krpc_call(mdsin, RPCPROG_MNT, RPCMNT_VER1,
				  RPCMNT_MOUNT, &m, NULL, td);
		if (error != 0)
			return error;	/* message already freed */

#ifdef BOOTP_NFSV3
	}
#endif

	if (xdr_int_decode(&m, &error) != 0 || error != 0)
		goto bad;

	if ((args->flags & NFSMNT_NFSV3) != 0) {
		if (xdr_int_decode(&m, fhsizep) != 0 ||
		    *fhsizep > NFSX_V3FHMAX ||
		    *fhsizep <= 0)
			goto bad;
	} else
		*fhsizep = NFSX_V2FH;

	if (xdr_opaque_decode(&m, fhp, *fhsizep) != 0)
		goto bad;

	if (args->flags & NFSMNT_NFSV3) {
		if (xdr_int_decode(&m, &authcount) != 0)
			goto bad;
		authunixok = 0;
		if (authcount < 0 || authcount > 100)
			goto bad;
		while (authcount > 0) {
			if (xdr_int_decode(&m, &authver) != 0)
				goto bad;
			if (authver == RPCAUTH_UNIX)
				authunixok = 1;
			authcount--;
		}
		if (authunixok == 0)
			goto bad;
	}

	/* Set port number for NFS use. */
	error = krpc_portmap(mdsin, NFS_PROG,
			     (args->flags &
			      NFSMNT_NFSV3) ? NFS_VER3 : NFS_VER2,
			     &mdsin->sin_port, td);

	goto out;

bad:
	error = EBADRPC;

out:
	m_freem(m);
	return error;
}
