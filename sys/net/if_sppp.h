/*
 * Defines for synchronous PPP/Cisco link level subroutines.
 *
 * Copyright (C) 1994 Cronyx Ltd.
 * Author: Serge Vakulenko, <vak@zebub.msk.su>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.7, Wed Jun  7 22:12:02 MSD 1995
 */

#ifndef _NET_IF_HDLC_H_
#define _NET_IF_HDLC_H_ 1

struct slcp {
	u_short state;          /* state machine */
	u_long  magic;          /* local magic number */
	u_char  echoid;         /* id of last keepalive echo request */
	u_char  confid;         /* id of last configuration request */
};

struct sipcp {
	u_short state;          /* state machine */
	u_char  confid;         /* id of last configuration request */
};

struct sppp {
	struct  ifnet pp_if;    /* network interface data */
	struct  ifqueue pp_fastq; /* fast output queue */
	struct  sppp *pp_next;  /* next interface in keepalive list */
	u_int   pp_flags;       /* use Cisco protocol instead of PPP */
	u_short pp_alivecnt;    /* keepalive packets counter */
	u_short pp_loopcnt;     /* loopback detection counter */
	u_long  pp_seq;         /* local sequence number */
	u_long  pp_rseq;        /* remote sequence number */
	struct slcp lcp;        /* LCP params */
	struct sipcp ipcp;      /* IPCP params */
};

#define PP_KEEPALIVE    0x01    /* use keepalive protocol */
#define PP_CISCO        0x02    /* use Cisco protocol instead of PPP */
#define PP_TIMO         0x04    /* cp_timeout routine active */

#define PP_MTU          1500    /* max. transmit unit */

#define LCP_STATE_CLOSED        0       /* LCP state: closed (conf-req sent) */
#define LCP_STATE_ACK_RCVD      1       /* LCP state: conf-ack received */
#define LCP_STATE_ACK_SENT      2       /* LCP state: conf-ack sent */
#define LCP_STATE_OPENED        3       /* LCP state: opened */

#define IPCP_STATE_CLOSED       0       /* IPCP state: closed (conf-req sent) */
#define IPCP_STATE_ACK_RCVD     1       /* IPCP state: conf-ack received */
#define IPCP_STATE_ACK_SENT     2       /* IPCP state: conf-ack sent */
#define IPCP_STATE_OPENED       3       /* IPCP state: opened */

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
