/*	$NetBSD: natm.h,v 1.1 1996/07/04 03:20:12 chuck Exp $	*/
/* $FreeBSD$ */

/*
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * natm.h: native mode atm
 */

/*
 * supported protocols
 */
#define	PROTO_NATMAAL0		1
#define	PROTO_NATMAAL5		2	

/*
 * sockaddr_natm
 */

struct sockaddr_natm {
	unsigned char	snatm_len;		/* length */
	sa_family_t	snatm_family;		/* AF_NATM */
	char		snatm_if[IFNAMSIZ];	/* interface name */
	u_int16_t	snatm_vci;		/* vci */
	u_int8_t	snatm_vpi;		/* vpi */
};

#if defined(__FreeBSD__) && defined(_KERNEL)

#define	SPLSOFTNET() splnet()

#elif defined(__NetBSD__) || defined(__OpenBSD__)

#define	SPLSOFTNET() splsoftnet()

#endif

#ifdef _KERNEL

/*
 * natm protocol control block
 */
struct natmpcb {
	LIST_ENTRY(natmpcb) pcblist;	/* list pointers */
	u_int		npcb_inq;	/* # of our pkts in proto q */
	struct socket	*npcb_socket;	/* backpointer to socket */
	struct ifnet	*npcb_ifp;	/* pointer to hardware */
	struct in_addr	ipaddr;		/* remote IP address, if APCB_IP */
	u_int16_t	npcb_vci;	/* VCI */
	u_int8_t	npcb_vpi;	/* VPI */
	u_int8_t	npcb_flags;	/* flags */
};

/* flags */
#define	NPCB_FREE	0x01		/* free (not on any list) */
#define	NPCB_CONNECTED	0x02		/* connected */
#define	NPCB_IP		0x04		/* used by IP */
#define	NPCB_DRAIN	0x08		/* destory as soon as inq == 0 */

/* flag arg to npcb_free */
#define	NPCB_REMOVE	0		/* remove from global list */
#define	NPCB_DESTROY	1		/* destroy and be free */

LIST_HEAD(npcblist, natmpcb);

/* global data structures */

extern struct npcblist natm_pcbs;	/* global list of pcbs */
#define	NATM_STAT
#ifdef NATM_STAT
extern	u_int	natm_sodropcnt;
extern	u_int	natm_sodropbytes;	/* account of droppage */
extern	u_int	natm_sookcnt;
extern	u_int	natm_sookbytes;		/* account of ok */
#endif

/* external functions */

/* natm_pcb.c */
struct	natmpcb *npcb_alloc(int);
void	npcb_free(struct natmpcb *, int);
struct	natmpcb *npcb_add(struct natmpcb *, struct ifnet *, uint16_t, uint8_t);

/* natm.c */
#if defined(__NetBSD__) || defined(__OpenBSD__)
int	natm_usrreq(struct socket *, int, struct mbuf *,
	    struct mbuf *, struct mbuf *, struct proc *);
#elif defined(__FreeBSD__)
#if __FreeBSD__ > 2
/*
 * FreeBSD new usrreqs style appeared since 2.2.  compatibility to old style
 * has gone since 3.0.
 */
#define	FREEBSD_USRREQS
extern struct pr_usrreqs natm_usrreqs;
#else /* !( __FreeBSD__ > 2) */
int	natm_usrreq(struct socket *, int, struct mbuf *,
	    struct mbuf *, struct mbuf *);
#endif /* !( __FreeBSD__ > 2) */
#endif

#ifdef SYSCTL_HANDLER_ARGS
int	natm0_sysctl(SYSCTL_HANDLER_ARGS);
int	natm5_sysctl(SYSCTL_HANDLER_ARGS);
#endif

void	natmintr(struct mbuf *);

#endif
