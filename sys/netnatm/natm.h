/*	$NetBSD: natm.h,v 1.1 1996/07/04 03:20:12 chuck Exp $	*/

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

#define PROTO_NATMAAL0		1
#define PROTO_NATMAAL5		2	

/*
 * sockaddr_natm
 */

struct sockaddr_natm {
  u_int8_t	snatm_len;		/* length */
  u_int8_t	snatm_family;		/* AF_NATM */
  char		snatm_if[IFNAMSIZ];	/* interface name */
  u_int16_t	snatm_vci;		/* vci */
  u_int8_t	snatm_vpi;		/* vpi */
};


#if defined(__FreeBSD__) && defined(KERNEL)

#ifndef _KERNEL
#define _KERNEL
#endif

#define SPLSOFTNET() splnet()

#elif defined(__NetBSD__) || defined(__OpenBSD__)

#define SPLSOFTNET() splsoftnet()

#endif

#ifdef _KERNEL

/*
 * natm protocol control block
 */

struct natmpcb {
  LIST_ENTRY(natmpcb) pcblist;		/* list pointers */
  u_int	npcb_inq;			/* # of our pkts in proto q */
  struct socket	*npcb_socket;		/* backpointer to socket */
  struct ifnet *npcb_ifp;		/* pointer to hardware */
  struct in_addr ipaddr;		/* remote IP address, if APCB_IP */
  u_int16_t npcb_vci;			/* VCI */
  u_int8_t npcb_vpi;			/* VPI */
  u_int8_t npcb_flags;			/* flags */
};

/* flags */
#define NPCB_FREE	0x01		/* free (not on any list) */
#define NPCB_CONNECTED	0x02		/* connected */
#define NPCB_IP		0x04		/* used by IP */
#define NPCB_DRAIN	0x08		/* destory as soon as inq == 0 */
#define NPCB_RAW	0x10		/* in 'raw' mode? */

/* flag arg to npcb_free */
#define NPCB_REMOVE	0		/* remove from global list */
#define NPCB_DESTROY	1		/* destroy and be free */

/*
 * NPCB_RAWCC is a hack which applies to connections in 'raw' mode.   it 
 * is used to override the sbspace() macro when you *really* don't want 
 * to drop rcv data.   the recv socket buffer size is raised to this value.
 *
 * XXX: socket buffering needs to be looked at.
 */

#define NPCB_RAWCC (1024*1024)		/* 1MB */

LIST_HEAD(npcblist, natmpcb);

/* global data structures */

extern struct npcblist natm_pcbs;	/* global list of pcbs */
extern	struct ifqueue natmintrq;	/* natm packet input queue */
#define	NATM_STAT
#ifdef NATM_STAT
extern	u_int natm_sodropcnt,
		natm_sodropbytes;	/* account of droppage */
extern	u_int natm_sookcnt,
		natm_sookbytes;		/* account of ok */
#endif

/* atm_rawioctl: kernel's version of SIOCRAWATM [for internal use only!] */
struct atm_rawioctl {
  struct natmpcb *npcb;
  int rawvalue;
}; 
#define SIOCXRAWATM     _IOWR('a', 125, struct atm_rawioctl)

/* external functions */

/* natm_pcb.c */
struct	natmpcb *npcb_alloc __P((int));
void	npcb_free __P((struct natmpcb *, int));
struct	natmpcb *npcb_add __P((struct natmpcb *, struct ifnet *, int, int));

/* natm.c */
#if defined(__NetBSD__) || defined(__OpenBSD__)
int	natm_usrreq __P((struct socket *, int, struct mbuf *,
                             struct mbuf *, struct mbuf *, struct proc *));
#elif defined(__FreeBSD__)
#if __FreeBSD__ > 2
/*
 * FreeBSD new usrreqs style appeared since 2.2.  compatibility to old style
 * has gone since 3.0.
 */
#define FREEBSD_USRREQS
extern struct pr_usrreqs natm_usrreqs;
#else /* !( __FreeBSD__ > 2) */
int	natm_usrreq __P((struct socket *, int, struct mbuf *,
                             struct mbuf *, struct mbuf *));
#endif /* !( __FreeBSD__ > 2) */
#endif
int	natm0_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
int	natm5_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
void	natmintr __P((void));

#endif
