/*
 * Defines for synchronous PPP/Cisco link level subroutines.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@cronyx.ru>
 *
 * Heavily revamped to conform to RFC 1661.
 * Copyright (C) 1997, Joerg Wunsch.
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organizations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * From: Version 1.7, Wed Jun  7 22:12:02 MSD 1995
 *
 * $Id$
 */

#ifndef _NET_IF_HDLC_H_
#define _NET_IF_HDLC_H_ 1

#define IDX_LCP 0		/* idx into state table */

struct slcp {
	u_long	opts;		/* LCP options to send (bitfield) */
	u_long  magic;          /* local magic number */
	u_long	mru;		/* our max receive unit */
	u_long	their_mru;	/* their max receive unit */
	u_long	protos;		/* bitmask of protos that are started */
	u_char  echoid;         /* id of last keepalive echo request */
	/* restart max values, see RFC 1661 */
	int	timeout;
	int	max_terminate;
	int	max_configure;
	int	max_failure;
};

#define IDX_IPCP 1		/* idx into state table */

struct sipcp {
	u_long	opts;		/* IPCP options to send (bitfield) */
	u_int	flags;
#define IPCP_HISADDR_SEEN 1	/* have seen his address already */
#define IPCP_MYADDR_DYN   2	/* my address is dynamically assigned */
};

#define IDX_COUNT (IDX_IPCP + 1) /* bump this when adding cp's! */

/*
 * Don't change the order of this.  Ordering the phases this way allows
 * for a comparision of ``pp_phase >= PHASE_AUTHENTICATE'' in order to
 * know whether LCP is up.
 */
enum ppp_phase {
	PHASE_DEAD, PHASE_ESTABLISH, PHASE_TERMINATE,
	PHASE_AUTHENTICATE, PHASE_NETWORK
};

struct sppp {
	/* NB: pp_if _must_ be first */
	struct  ifnet pp_if;    /* network interface data */
	struct  ifqueue pp_fastq; /* fast output queue */
	struct  sppp *pp_next;  /* next interface in keepalive list */
	u_int   pp_flags;       /* use Cisco protocol instead of PPP */
	u_short pp_alivecnt;    /* keepalive packets counter */
	u_short pp_loopcnt;     /* loopback detection counter */
	u_long  pp_seq;         /* local sequence number */
	u_long  pp_rseq;        /* remote sequence number */
	enum ppp_phase pp_phase;	/* phase we're currently in */
	int	state[IDX_COUNT];	/* state machine */
	u_char  confid[IDX_COUNT];	/* id of last configuration request */
	int	rst_counter[IDX_COUNT];	/* restart counter */
	int	fail_counter[IDX_COUNT]; /* negotiation failure counter */
	struct slcp lcp;        /* LCP params */
	struct sipcp ipcp;      /* IPCP params */
	/*
	 * These functions are filled in by sppp_attach(), and are
	 * expected to be used by the lower layer (hardware) drivers
	 * in order to communicate the (un)availability of the
	 * communication link.  Lower layer drivers that are always
	 * ready to communicate (like hardware HDLC) can shortcut
	 * pp_up from pp_tls, and pp_down from pp_tlf.
	 */
	void	(*pp_up)(struct sppp *sp);
	void	(*pp_down)(struct sppp *sp);
	/*
	 * These functions need to be filled in by the lower layer
	 * (hardware) drivers if they request notification from the
	 * PPP layer whether the link is actually required.  They
	 * correspond to the tls and tlf actions.
	 */
	void	(*pp_tls)(struct sppp *sp);
	void	(*pp_tlf)(struct sppp *sp);
};

#define PP_KEEPALIVE    0x01    /* use keepalive protocol */
#define PP_CISCO        0x02    /* use Cisco protocol instead of PPP */

#define PP_MTU          1500    /* default/minimal MRU */
#define PP_MAX_MRU	2048	/* maximal MRU we want to negotiate */

#ifdef KERNEL
void sppp_attach (struct ifnet *ifp);
void sppp_detach (struct ifnet *ifp);
void sppp_input (struct ifnet *ifp, struct mbuf *m);
int sppp_ioctl (struct ifnet *ifp, int cmd, void *data);
struct mbuf *sppp_dequeue (struct ifnet *ifp);
int sppp_isempty (struct ifnet *ifp);
void sppp_flush (struct ifnet *ifp);
#endif

#endif /* _NET_IF_HDLC_H_ */
