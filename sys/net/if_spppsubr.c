/*
 * Synchronous PPP/Cisco link level subroutines.
 * Keepalive protocol implemented in both Cisco and PPP modes.
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
 * Version 1.9, Wed Oct  4 18:58:15 MSK 1995
 */
#undef DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef ISO
#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#endif

#include <net/if_sppp.h>

#ifdef DEBUG
#define print(s)        printf s
#else
#define print(s)        {/*void*/}
#endif

#define MAXALIVECNT     3               /* max. alive packets */

#define PPP_ALLSTATIONS 0xff            /* All-Stations broadcast address */
#define PPP_UI          0x03            /* Unnumbered Information */
#define PPP_IP          0x0021          /* Internet Protocol */
#define PPP_ISO         0x0023          /* ISO OSI Protocol */
#define PPP_XNS         0x0025          /* Xerox NS Protocol */
#define PPP_IPX         0x002b          /* Novell IPX Protocol */
#define PPP_LCP         0xc021          /* Link Control Protocol */
#define PPP_IPCP        0x8021          /* Internet Protocol Control Protocol */

#define LCP_CONF_REQ    1               /* PPP LCP configure request */
#define LCP_CONF_ACK    2               /* PPP LCP configure acknowledge */
#define LCP_CONF_NAK    3               /* PPP LCP configure negative ack */
#define LCP_CONF_REJ    4               /* PPP LCP configure reject */
#define LCP_TERM_REQ    5               /* PPP LCP terminate request */
#define LCP_TERM_ACK    6               /* PPP LCP terminate acknowledge */
#define LCP_CODE_REJ    7               /* PPP LCP code reject */
#define LCP_PROTO_REJ   8               /* PPP LCP protocol reject */
#define LCP_ECHO_REQ    9               /* PPP LCP echo request */
#define LCP_ECHO_REPLY  10              /* PPP LCP echo reply */
#define LCP_DISC_REQ    11              /* PPP LCP discard request */

#define LCP_OPT_MRU             1       /* maximum receive unit */
#define LCP_OPT_ASYNC_MAP       2       /* async control character map */
#define LCP_OPT_AUTH_PROTO      3       /* authentication protocol */
#define LCP_OPT_QUAL_PROTO      4       /* quality protocol */
#define LCP_OPT_MAGIC           5       /* magic number */
#define LCP_OPT_RESERVED        6       /* reserved */
#define LCP_OPT_PROTO_COMP      7       /* protocol field compression */
#define LCP_OPT_ADDR_COMP       8       /* address/control field compression */

#define IPCP_CONF_REQ   LCP_CONF_REQ    /* PPP IPCP configure request */
#define IPCP_CONF_ACK   LCP_CONF_ACK    /* PPP IPCP configure acknowledge */
#define IPCP_CONF_NAK   LCP_CONF_NAK    /* PPP IPCP configure negative ack */
#define IPCP_CONF_REJ   LCP_CONF_REJ    /* PPP IPCP configure reject */
#define IPCP_TERM_REQ   LCP_TERM_REQ    /* PPP IPCP terminate request */
#define IPCP_TERM_ACK   LCP_TERM_ACK    /* PPP IPCP terminate acknowledge */
#define IPCP_CODE_REJ   LCP_CODE_REJ    /* PPP IPCP code reject */

#define CISCO_MULTICAST         0x8f    /* Cisco multicast address */
#define CISCO_UNICAST           0x0f    /* Cisco unicast address */
#define CISCO_KEEPALIVE         0x8035  /* Cisco keepalive protocol */
#define CISCO_ADDR_REQ          0       /* Cisco address request */
#define CISCO_ADDR_REPLY        1       /* Cisco address reply */
#define CISCO_KEEPALIVE_REQ     2       /* Cisco keepalive request */

struct ppp_header {
	u_char address;
	u_char control;
	u_short protocol;
};
#define PPP_HEADER_LEN          sizeof (struct ppp_header)

struct lcp_header {
	u_char type;
	u_char ident;
	u_short len;
};
#define LCP_HEADER_LEN          sizeof (struct lcp_header)

struct cisco_packet {
	u_long type;
	u_long par1;
	u_long par2;
	u_short rel;
	u_short time0;
	u_short time1;
};
#define CISCO_PACKET_LEN 18

static struct sppp *spppq;

/*
 * The following disgusting hack gets around the problem that IP TOS
 * can't be set yet.  We want to put "interactive" traffic on a high
 * priority queue.  To decide if traffic is interactive, we check that
 * a) it is TCP and b) one of its ports is telnet, rlogin or ftp control.
 */
static u_short interactive_ports[8] = {
	0,	513,	0,	0,
	0,	21,	0,	23,
};
#define INTERACTIVE(p) (interactive_ports[(p) & 7] == (p))

/*
 * Timeout routine activation macros.
 */
#define TIMO(p,s) if (! ((p)->pp_flags & PP_TIMO)) { \
			timeout (sppp_cp_timeout, (void*) (p), (s)*hz); \
			(p)->pp_flags |= PP_TIMO; }
#define UNTIMO(p) if ((p)->pp_flags & PP_TIMO) { \
			untimeout (sppp_cp_timeout, (void*) (p)); \
			(p)->pp_flags &= ~PP_TIMO; }

static void sppp_keepalive (void *dummy);
static void sppp_cp_send (struct sppp *sp, u_short proto, u_char type,
	u_char ident, u_short len, void *data);
static void sppp_cisco_send (struct sppp *sp, int type, long par1, long par2);
static void sppp_lcp_input (struct sppp *sp, struct mbuf *m);
static void sppp_cisco_input (struct sppp *sp, struct mbuf *m);
static void sppp_ipcp_input (struct sppp *sp, struct mbuf *m);
static void sppp_lcp_open (struct sppp *sp);
static void sppp_ipcp_open (struct sppp *sp);
static int sppp_lcp_conf_parse_options (struct sppp *sp, struct lcp_header *h,
	int len, u_long *magic);
static void sppp_cp_timeout (void *arg);
static char *sppp_lcp_type_name (u_char type);
static char *sppp_ipcp_type_name (u_char type);
static void sppp_print_bytes (u_char *p, u_short len);
static int sppp_output (struct ifnet *ifp, struct mbuf *m, 
	struct sockaddr *dst, struct rtentry *rt);

/*
 * Flush interface queue.
 */
static void qflush (struct ifqueue *ifq)
{
	struct mbuf *m, *n;

	n = ifq->ifq_head;
	while ((m = n)) {
		n = m->m_act;
		m_freem (m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
}

/*
 * Process the received packet.
 */
void sppp_input (struct ifnet *ifp, struct mbuf *m)
{
	struct ppp_header *h;
	struct sppp *sp = (struct sppp*) ifp;
	struct ifqueue *inq = 0;
	int s;

	if (ifp->if_flags & IFF_UP)
		/* Count received bytes, add FCS and one flag */
		ifp->if_ibytes += m->m_pkthdr.len + 3;

	if (m->m_pkthdr.len <= PPP_HEADER_LEN) {
		/* Too small packet, drop it. */
		if (ifp->if_flags & IFF_DEBUG)
			printf ("%s%d: input packet is too small, %d bytes\n",
				ifp->if_name, ifp->if_unit, m->m_pkthdr.len);
drop:           ++ifp->if_iqdrops;
		m_freem (m);
		return;
	}

	/* Get PPP header. */
	h = mtod (m, struct ppp_header*);
	m_adj (m, PPP_HEADER_LEN);

	switch (h->address) {
	default:        /* Invalid PPP packet. */
invalid:        if (ifp->if_flags & IFF_DEBUG)
			printf ("%s%d: invalid input packet <0x%x 0x%x 0x%x>\n",
				ifp->if_name, ifp->if_unit,
				h->address, h->control, ntohs (h->protocol));
		goto drop;
	case PPP_ALLSTATIONS:
		if (h->control != PPP_UI)
			goto invalid;
		if (sp->pp_flags & PP_CISCO) {
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: PPP packet in Cisco mode <0x%x 0x%x 0x%x>\n",
					ifp->if_name, ifp->if_unit,
					h->address, h->control, ntohs (h->protocol));
			goto drop;
		}
		switch (ntohs (h->protocol)) {
		default:
			if (sp->lcp.state == LCP_STATE_OPENED)
				sppp_cp_send (sp, PPP_LCP, LCP_PROTO_REJ,
					++sp->pp_seq, m->m_pkthdr.len + 2,
					&h->protocol);
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: invalid input protocol <0x%x 0x%x 0x%x>\n",
					ifp->if_name, ifp->if_unit,
					h->address, h->control, ntohs (h->protocol));
			++ifp->if_noproto;
			goto drop;
		case PPP_LCP:
			sppp_lcp_input ((struct sppp*) ifp, m);
			m_freem (m);
			return;
#ifdef INET
		case PPP_IPCP:
			if (sp->lcp.state == LCP_STATE_OPENED)
				sppp_ipcp_input ((struct sppp*) ifp, m);
			m_freem (m);
			return;
		case PPP_IP:
			if (sp->ipcp.state == IPCP_STATE_OPENED) {
				schednetisr (NETISR_IP);
				inq = &ipintrq;
			}
			break;
#endif
#ifdef IPX
		case PPP_IPX:
			/* IPX IPXCP not implemented yet */
			if (sp->lcp.state == LCP_STATE_OPENED) {
				schednetisr (NETISR_IPX);
				inq = &ipxintrq;
			}
			break;
#endif
#ifdef NS
		case PPP_XNS:
			/* XNS IDPCP not implemented yet */
			if (sp->lcp.state == LCP_STATE_OPENED) {
				schednetisr (NETISR_NS);
				inq = &nsintrq;
			}
			break;
#endif
#ifdef ISO
		case PPP_ISO:
			/* OSI NLCP not implemented yet */
			if (sp->lcp.state == LCP_STATE_OPENED) {
				schednetisr (NETISR_ISO);
				inq = &clnlintrq;
			}
			break;
#endif
		}
		break;
	case CISCO_MULTICAST:
	case CISCO_UNICAST:
		/* Don't check the control field here (RFC 1547). */
		if (! (sp->pp_flags & PP_CISCO)) {
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: Cisco packet in PPP mode <0x%x 0x%x 0x%x>\n",
					ifp->if_name, ifp->if_unit,
					h->address, h->control, ntohs (h->protocol));
			goto drop;
		}
		switch (ntohs (h->protocol)) {
		default:
			++ifp->if_noproto;
			goto invalid;
		case CISCO_KEEPALIVE:
			sppp_cisco_input ((struct sppp*) ifp, m);
			m_freem (m);
			return;
#ifdef INET
		case ETHERTYPE_IP:
			schednetisr (NETISR_IP);
			inq = &ipintrq;
			break;
#endif
#ifdef IPX
		case ETHERTYPE_IPX:
			schednetisr (NETISR_IPX);
			inq = &ipxintrq;
			break;
#endif
#ifdef NS
		case ETHERTYPE_NS:
			schednetisr (NETISR_NS);
			inq = &nsintrq;
			break;
#endif
		}
		break;
	}

	if (! (ifp->if_flags & IFF_UP) || ! inq)
		goto drop;

	/* Check queue. */
	s = splimp ();
	if (IF_QFULL (inq)) {
		/* Queue overflow. */
		IF_DROP (inq);
		splx (s);
		if (ifp->if_flags & IFF_DEBUG)
			printf ("%s%d: protocol queue overflow\n",
				ifp->if_name, ifp->if_unit);
		goto drop;
	}
	IF_ENQUEUE (inq, m);
	splx (s);
}

/*
 * Enqueue transmit packet.
 */
static int
sppp_output (struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst, struct rtentry *rt)
{
	struct sppp *sp = (struct sppp*) ifp;
	struct ppp_header *h;
	struct ifqueue *ifq;
	int s = splimp ();

	if (! (ifp->if_flags & IFF_UP) || ! (ifp->if_flags & IFF_RUNNING)) {
		m_freem (m);
		splx (s);
		return (ENETDOWN);
	}

	ifq = &ifp->if_snd;
#ifdef INET
	/*
	 * Put low delay, telnet, rlogin and ftp control packets
	 * in front of the queue.
	 */
	if (dst->sa_family == AF_INET) {
		struct ip *ip = mtod (m, struct ip*);
		struct tcphdr *tcp = (struct tcphdr*) ((long*)ip + ip->ip_hl);

		if (! IF_QFULL (&sp->pp_fastq) &&
		    ((ip->ip_tos & IPTOS_LOWDELAY) ||
	    	    ip->ip_p == IPPROTO_TCP &&
	    	    m->m_len >= sizeof (struct ip) + sizeof (struct tcphdr) &&
	    	    (INTERACTIVE (ntohs (tcp->th_sport)) ||
	    	    INTERACTIVE (ntohs (tcp->th_dport)))))
			ifq = &sp->pp_fastq;
	}
#endif

	/*
	 * Prepend general data packet PPP header. For now, IP only.
	 */
	M_PREPEND (m, PPP_HEADER_LEN, M_DONTWAIT);
	if (! m) {
		if (ifp->if_flags & IFF_DEBUG)
			printf ("%s%d: no memory for transmit header\n",
				ifp->if_name, ifp->if_unit);
		splx (s);
		return (ENOBUFS);
	}
	h = mtod (m, struct ppp_header*);
	if (sp->pp_flags & PP_CISCO) {
		h->address = CISCO_MULTICAST;        /* broadcast address */
		h->control = 0;
	} else {
		h->address = PPP_ALLSTATIONS;        /* broadcast address */
		h->control = PPP_UI;                 /* Unnumbered Info */
	}

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:   /* Internet Protocol */
		if (sp->pp_flags & PP_CISCO)
			h->protocol = htons (ETHERTYPE_IP);
		else if (sp->ipcp.state == IPCP_STATE_OPENED)
			h->protocol = htons (PPP_IP);
		else {
			m_freem (m);
			splx (s);
			return (ENETDOWN);
		}
		break;
#endif
#ifdef NS
	case AF_NS:     /* Xerox NS Protocol */
		h->protocol = htons ((sp->pp_flags & PP_CISCO) ?
			ETHERTYPE_NS : PPP_XNS);
		break;
#endif
#ifdef IPX
	case AF_IPX:     /* Novell IPX Protocol */
		h->protocol = htons ((sp->pp_flags & PP_CISCO) ?
			ETHERTYPE_IPX : PPP_IPX);
		break;
#endif
#ifdef ISO
	case AF_ISO:    /* ISO OSI Protocol */
		if (sp->pp_flags & PP_CISCO)
			goto nosupport;
		h->protocol = htons (PPP_ISO);
		break;
nosupport:
#endif
	default:
		m_freem (m);
		splx (s);
		return (EAFNOSUPPORT);
	}

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	if (IF_QFULL (ifq)) {
		IF_DROP (&ifp->if_snd);
		m_freem (m);
		splx (s);
		return (ENOBUFS);
	}
	IF_ENQUEUE (ifq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);

	/*
	 * Count output packets and bytes.
	 * The packet length includes header, FCS and 1 flag,
	 * according to RFC 1333.
	 */
	ifp->if_obytes += m->m_pkthdr.len + 3;
	splx (s);
	return (0);
}

void sppp_attach (struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;

	/* Initialize keepalive handler. */
	if (! spppq)
		timeout (sppp_keepalive, 0, hz * 10);

	/* Insert new entry into the keepalive list. */
	sp->pp_next = spppq;
	spppq = sp;

	sp->pp_if.if_type = IFT_PPP;
	sp->pp_if.if_output = sppp_output;
	sp->pp_fastq.ifq_maxlen = 32;
	sp->pp_loopcnt = 0;
	sp->pp_alivecnt = 0;
	sp->pp_seq = 0;
	sp->pp_rseq = 0;
	sp->lcp.magic = 0;
	sp->lcp.state = LCP_STATE_CLOSED;
	sp->ipcp.state = IPCP_STATE_CLOSED;
}

void 
sppp_detach (struct ifnet *ifp)
{
	struct sppp **q, *p, *sp = (struct sppp*) ifp;

	/* Remove the entry from the keepalive list. */
	for (q = &spppq; (p = *q); q = &p->pp_next)
		if (p == sp) {
			*q = p->pp_next;
			break;
		}

	/* Stop keepalive handler. */
	if (! spppq)
		untimeout (sppp_keepalive, 0);
	UNTIMO (sp);
}

/*
 * Flush the interface output queue.
 */
void sppp_flush (struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;

	qflush (&sp->pp_if.if_snd);
	qflush (&sp->pp_fastq);
}

/*
 * Check if the output queue is empty.
 */
int
sppp_isempty (struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;
	int empty, s = splimp ();

	empty = !sp->pp_fastq.ifq_head && !sp->pp_if.if_snd.ifq_head;
	splx (s);
	return (empty);
}

/*
 * Get next packet to send.
 */
struct mbuf *sppp_dequeue (struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;
	struct mbuf *m;
	int s = splimp ();

	IF_DEQUEUE (&sp->pp_fastq, m);
	if (! m)
		IF_DEQUEUE (&sp->pp_if.if_snd, m);
	splx (s);
	return (m);
}

/*
 * Send keepalive packets, every 10 seconds.
 */
void sppp_keepalive (void *dummy)
{
	struct sppp *sp;
	int s = splimp ();

	for (sp=spppq; sp; sp=sp->pp_next) {
		struct ifnet *ifp = &sp->pp_if;

		/* Keepalive mode disabled or channel down? */
		if (! (sp->pp_flags & PP_KEEPALIVE) ||
		    ! (ifp->if_flags & IFF_RUNNING))
			continue;

		/* No keepalive in PPP mode if LCP not opened yet. */
		if (! (sp->pp_flags & PP_CISCO) &&
		    sp->lcp.state != LCP_STATE_OPENED)
			continue;

		if (sp->pp_alivecnt == MAXALIVECNT) {
			/* No keepalive packets got.  Stop the interface. */
			printf ("%s%d: down\n", ifp->if_name, ifp->if_unit);
			if_down (ifp);
			qflush (&sp->pp_fastq);
			if (! (sp->pp_flags & PP_CISCO)) {
				/* Shut down the PPP link. */
				sp->lcp.state = LCP_STATE_CLOSED;
				sp->ipcp.state = IPCP_STATE_CLOSED;
				UNTIMO (sp);
				/* Initiate negotiation. */
				sppp_lcp_open (sp);
			}
		}
		if (sp->pp_alivecnt <= MAXALIVECNT)
			++sp->pp_alivecnt;
		if (sp->pp_flags & PP_CISCO)
			sppp_cisco_send (sp, CISCO_KEEPALIVE_REQ, ++sp->pp_seq,
				sp->pp_rseq);
		else if (sp->lcp.state == LCP_STATE_OPENED) {
			long nmagic = htonl (sp->lcp.magic);
			sp->lcp.echoid = ++sp->pp_seq;
			sppp_cp_send (sp, PPP_LCP, LCP_ECHO_REQ,
				sp->lcp.echoid, 4, &nmagic);
		}
	}
	splx (s);
	timeout (sppp_keepalive, 0, hz * 10);
}

/*
 * Handle incoming PPP Link Control Protocol packets.
 */
void sppp_lcp_input (struct sppp *sp, struct mbuf *m)
{
	struct lcp_header *h;
	struct ifnet *ifp = &sp->pp_if;
	int len = m->m_pkthdr.len;
	u_char *p, opt[6];
	u_long rmagic;

	if (len < 4) {
		if (ifp->if_flags & IFF_DEBUG)
			printf ("%s%d: invalid lcp packet length: %d bytes\n",
				ifp->if_name, ifp->if_unit, len);
		return;
	}
	h = mtod (m, struct lcp_header*);
	if (ifp->if_flags & IFF_DEBUG) {
		char state = '?';
		switch (sp->lcp.state) {
		case LCP_STATE_CLOSED:   state = 'C'; break;
		case LCP_STATE_ACK_RCVD: state = 'R'; break;
		case LCP_STATE_ACK_SENT: state = 'S'; break;
		case LCP_STATE_OPENED:   state = 'O'; break;
		}
		printf ("%s%d: lcp input(%c): %d bytes <%s id=%xh len=%xh",
			ifp->if_name, ifp->if_unit, state, len,
			sppp_lcp_type_name (h->type), h->ident, ntohs (h->len));
		if (len > 4)
			sppp_print_bytes ((u_char*) (h+1), len-4);
		printf (">\n");
	}
	if (len > ntohs (h->len))
		len = ntohs (h->len);
	switch (h->type) {
	default:
		/* Unknown packet type -- send Code-Reject packet. */
		sppp_cp_send (sp, PPP_LCP, LCP_CODE_REJ, ++sp->pp_seq,
			m->m_pkthdr.len, h);
		break;
	case LCP_CONF_REQ:
		if (len < 4) {
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: invalid lcp configure request packet length: %d bytes\n",
					ifp->if_name, ifp->if_unit, len);
			break;
		}
		if (len>4 && !sppp_lcp_conf_parse_options (sp, h, len, &rmagic))
			goto badreq;
		if (rmagic == sp->lcp.magic) {
			/* Local and remote magics equal -- loopback? */
			if (sp->pp_loopcnt >= MAXALIVECNT*5) {
				printf ("%s%d: loopback\n",
					ifp->if_name, ifp->if_unit);
				sp->pp_loopcnt = 0;
				if (ifp->if_flags & IFF_UP) {
					if_down (ifp);
					qflush (&sp->pp_fastq);
				}
			} else if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: conf req: magic glitch\n",
					ifp->if_name, ifp->if_unit);
			++sp->pp_loopcnt;

			/* MUST send Conf-Nack packet. */
			rmagic = ~sp->lcp.magic;
			opt[0] = LCP_OPT_MAGIC;
			opt[1] = sizeof (opt);
			opt[2] = rmagic >> 24;
			opt[3] = rmagic >> 16;
			opt[4] = rmagic >> 8;
			opt[5] = rmagic;
			sppp_cp_send (sp, PPP_LCP, LCP_CONF_NAK,
				h->ident, sizeof (opt), &opt);
badreq:
			switch (sp->lcp.state) {
			case LCP_STATE_OPENED:
				/* Initiate renegotiation. */
				sppp_lcp_open (sp);
				/* fall through... */
			case LCP_STATE_ACK_SENT:
				/* Go to closed state. */
				sp->lcp.state = LCP_STATE_CLOSED;
				sp->ipcp.state = IPCP_STATE_CLOSED;
			}
			break;
		}
		/* Send Configure-Ack packet. */
		sp->pp_loopcnt = 0;
		sppp_cp_send (sp, PPP_LCP, LCP_CONF_ACK,
				h->ident, len-4, h+1);
		/* Change the state. */
		switch (sp->lcp.state) {
		case LCP_STATE_CLOSED:
			sp->lcp.state = LCP_STATE_ACK_SENT;
			break;
		case LCP_STATE_ACK_RCVD:
			sp->lcp.state = LCP_STATE_OPENED;
			sppp_ipcp_open (sp);
			break;
		case LCP_STATE_OPENED:
			/* Remote magic changed -- close session. */
			sp->lcp.state = LCP_STATE_CLOSED;
			sp->ipcp.state = IPCP_STATE_CLOSED;
			/* Initiate renegotiation. */
			sppp_lcp_open (sp);
			/* An ACK has already been sent. */
			sp->lcp.state = LCP_STATE_ACK_SENT;
			break;
		}
		break;
	case LCP_CONF_ACK:
		if (h->ident != sp->lcp.confid)
			break;
		UNTIMO (sp);
		if (! (ifp->if_flags & IFF_UP) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			/* Coming out of loopback mode. */
			ifp->if_flags |= IFF_UP;
			printf ("%s%d: up\n", ifp->if_name, ifp->if_unit);
		}
		switch (sp->lcp.state) {
		case LCP_STATE_CLOSED:
			sp->lcp.state = LCP_STATE_ACK_RCVD;
			TIMO (sp, 5);
			break;
		case LCP_STATE_ACK_SENT:
			sp->lcp.state = LCP_STATE_OPENED;
			sppp_ipcp_open (sp);
			break;
		}
		break;
	case LCP_CONF_NAK:
		if (h->ident != sp->lcp.confid)
			break;
		p = (u_char*) (h+1);
		if (len>=10 && p[0] == LCP_OPT_MAGIC && p[1] >= 4) {
			rmagic = (u_long)p[2] << 24 |
				(u_long)p[3] << 16 | p[4] << 8 | p[5];
			if (rmagic == ~sp->lcp.magic) {
				if (ifp->if_flags & IFF_DEBUG)
					printf ("%s%d: conf nak: magic glitch\n",
						ifp->if_name, ifp->if_unit);
				sp->lcp.magic += time.tv_sec + time.tv_usec;
			} else
				sp->lcp.magic = rmagic;
			}
		if (sp->lcp.state != LCP_STATE_ACK_SENT) {
			/* Go to closed state. */
			sp->lcp.state = LCP_STATE_CLOSED;
			sp->ipcp.state = IPCP_STATE_CLOSED;
		}
		/* The link will be renegotiated after timeout,
		 * to avoid endless req-nack loop. */
		UNTIMO (sp);
		TIMO (sp, 2);
		break;
	case LCP_CONF_REJ:
		if (h->ident != sp->lcp.confid)
			break;
		UNTIMO (sp);
		/* Initiate renegotiation. */
		sppp_lcp_open (sp);
		if (sp->lcp.state != LCP_STATE_ACK_SENT) {
			/* Go to closed state. */
			sp->lcp.state = LCP_STATE_CLOSED;
			sp->ipcp.state = IPCP_STATE_CLOSED;
		}
		break;
	case LCP_TERM_REQ:
		UNTIMO (sp);
		/* Send Terminate-Ack packet. */
		sppp_cp_send (sp, PPP_LCP, LCP_TERM_ACK, h->ident, 0, 0);
		/* Go to closed state. */
		sp->lcp.state = LCP_STATE_CLOSED;
		sp->ipcp.state = IPCP_STATE_CLOSED;
		/* Initiate renegotiation. */
		sppp_lcp_open (sp);
		break;
	case LCP_TERM_ACK:
	case LCP_CODE_REJ:
	case LCP_PROTO_REJ:
		/* Ignore for now. */
		break;
	case LCP_DISC_REQ:
		/* Discard the packet. */
		break;
	case LCP_ECHO_REQ:
		if (sp->lcp.state != LCP_STATE_OPENED)
			break;
		if (len < 8) {
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: invalid lcp echo request packet length: %d bytes\n",
					ifp->if_name, ifp->if_unit, len);
			break;
		}
		if (ntohl (*(long*)(h+1)) == sp->lcp.magic) {
			/* Line loopback mode detected. */
			printf ("%s%d: loopback\n", ifp->if_name, ifp->if_unit);
			if_down (ifp);
			qflush (&sp->pp_fastq);

			/* Shut down the PPP link. */
			sp->lcp.state = LCP_STATE_CLOSED;
			sp->ipcp.state = IPCP_STATE_CLOSED;
			UNTIMO (sp);
			/* Initiate negotiation. */
			sppp_lcp_open (sp);
			break;
		}
		*(long*)(h+1) = htonl (sp->lcp.magic);
		sppp_cp_send (sp, PPP_LCP, LCP_ECHO_REPLY, h->ident, len-4, h+1);
		break;
	case LCP_ECHO_REPLY:
		if (h->ident != sp->lcp.echoid)
			break;
		if (len < 8) {
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: invalid lcp echo reply packet length: %d bytes\n",
					ifp->if_name, ifp->if_unit, len);
			break;
		}
		if (ntohl (*(long*)(h+1)) != sp->lcp.magic)
		sp->pp_alivecnt = 0;
		break;
	}
}

/*
 * Handle incoming Cisco keepalive protocol packets.
 */
static void 
sppp_cisco_input (struct sppp *sp, struct mbuf *m)
{
	struct cisco_packet *h;
	struct ifaddr *ifa;
	struct ifnet *ifp = &sp->pp_if;

	if (m->m_pkthdr.len != CISCO_PACKET_LEN) {
		if (ifp->if_flags & IFF_DEBUG)
			printf ("%s%d: invalid cisco packet length: %d bytes\n",
				ifp->if_name, ifp->if_unit, m->m_pkthdr.len);
		return;
	}
	h = mtod (m, struct cisco_packet*);
	if (ifp->if_flags & IFF_DEBUG)
		printf ("%s%d: cisco input: %d bytes <%lxh %lxh %lxh %xh %xh-%xh>\n",
			ifp->if_name, ifp->if_unit, m->m_pkthdr.len,
			ntohl (h->type), h->par1, h->par2, h->rel,
			h->time0, h->time1);
	switch (ntohl (h->type)) {
	default:
		if (ifp->if_flags & IFF_DEBUG)
			printf ("%s%d: unknown cisco packet type: 0x%lx\n",
				ifp->if_name, ifp->if_unit, ntohl (h->type));
		break;
	case CISCO_ADDR_REPLY:
		/* Reply on address request, ignore */
		break;
	case CISCO_KEEPALIVE_REQ:
		sp->pp_alivecnt = 0;
		sp->pp_rseq = ntohl (h->par1);
		if (sp->pp_seq == sp->pp_rseq) {
			/* Local and remote sequence numbers are equal.
			 * Probably, the line is in loopback mode. */
			if (sp->pp_loopcnt >= MAXALIVECNT) {
				printf ("%s%d: loopback\n",
					ifp->if_name, ifp->if_unit);
				sp->pp_loopcnt = 0;
				if (ifp->if_flags & IFF_UP) {
					if_down (ifp);
					qflush (&sp->pp_fastq);
				}
			}
			++sp->pp_loopcnt;

			/* Generate new local sequence number */
			sp->pp_seq ^= time.tv_sec ^ time.tv_usec;
			break;
		}
			sp->pp_loopcnt = 0;
		if (! (ifp->if_flags & IFF_UP) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			ifp->if_flags |= IFF_UP;
			printf ("%s%d: up\n", ifp->if_name, ifp->if_unit);
		}
		break;
	case CISCO_ADDR_REQ:
		for (ifa=ifp->if_addrlist; ifa; ifa=ifa->ifa_next)
			if (ifa->ifa_addr->sa_family == AF_INET)
				break;
		if (! ifa) {
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: unknown address for cisco request\n",
					ifp->if_name, ifp->if_unit);
			return;
		}
		sppp_cisco_send (sp, CISCO_ADDR_REPLY,
			ntohl (((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr),
			ntohl (((struct sockaddr_in*)ifa->ifa_netmask)->sin_addr.s_addr));
		break;
	}
}

/*
 * Send PPP LCP packet.
 */
static void
sppp_cp_send (struct sppp *sp, u_short proto, u_char type,
	u_char ident, u_short len, void *data)
{
	struct ppp_header *h;
	struct lcp_header *lh;
	struct mbuf *m;
	struct ifnet *ifp = &sp->pp_if;

	if (len > MHLEN - PPP_HEADER_LEN - LCP_HEADER_LEN)
		len = MHLEN - PPP_HEADER_LEN - LCP_HEADER_LEN;
	MGETHDR (m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.len = m->m_len = PPP_HEADER_LEN + LCP_HEADER_LEN + len;
	m->m_pkthdr.rcvif = 0;

	h = mtod (m, struct ppp_header*);
	h->address = PPP_ALLSTATIONS;        /* broadcast address */
	h->control = PPP_UI;                 /* Unnumbered Info */
	h->protocol = htons (proto);         /* Link Control Protocol */

	lh = (struct lcp_header*) (h + 1);
	lh->type = type;
	lh->ident = ident;
	lh->len = htons (LCP_HEADER_LEN + len);
	if (len)
		bcopy (data, lh+1, len);

	if (ifp->if_flags & IFF_DEBUG) {
		printf ("%s%d: %s output <%s id=%xh len=%xh",
			ifp->if_name, ifp->if_unit,
			proto==PPP_LCP ? "lcp" : "ipcp",
			proto==PPP_LCP ? sppp_lcp_type_name (lh->type) :
			sppp_ipcp_type_name (lh->type), lh->ident,
			ntohs (lh->len));
		if (len)
			sppp_print_bytes ((u_char*) (lh+1), len);
		printf (">\n");
	}
	if (IF_QFULL (&sp->pp_fastq)) {
		IF_DROP (&ifp->if_snd);
		m_freem (m);
	} else
		IF_ENQUEUE (&sp->pp_fastq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);
	ifp->if_obytes += m->m_pkthdr.len + 3;
}

/*
 * Send Cisco keepalive packet.
 */
static void
sppp_cisco_send (struct sppp *sp, int type, long par1, long par2)
{
	struct ppp_header *h;
	struct cisco_packet *ch;
	struct mbuf *m;
	struct ifnet *ifp = &sp->pp_if;
	u_long t = (time.tv_sec - boottime.tv_sec) * 1000;

	MGETHDR (m, M_DONTWAIT, MT_DATA);
	if (! m)
		return;
	m->m_pkthdr.len = m->m_len = PPP_HEADER_LEN + CISCO_PACKET_LEN;
	m->m_pkthdr.rcvif = 0;

	h = mtod (m, struct ppp_header*);
	h->address = CISCO_MULTICAST;
	h->control = 0;
	h->protocol = htons (CISCO_KEEPALIVE);

	ch = (struct cisco_packet*) (h + 1);
	ch->type = htonl (type);
	ch->par1 = htonl (par1);
	ch->par2 = htonl (par2);
	ch->rel = -1;
	ch->time0 = htons ((u_short) (t >> 16));
	ch->time1 = htons ((u_short) t);

	if (ifp->if_flags & IFF_DEBUG)
		printf ("%s%d: cisco output: <%lxh %lxh %lxh %xh %xh-%xh>\n",
			ifp->if_name, ifp->if_unit, ntohl (ch->type), ch->par1,
			ch->par2, ch->rel, ch->time0, ch->time1);

	if (IF_QFULL (&sp->pp_fastq)) {
		IF_DROP (&ifp->if_snd);
		m_freem (m);
	} else
		IF_ENQUEUE (&sp->pp_fastq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);
	ifp->if_obytes += m->m_pkthdr.len + 3;
}

/*
 * Process an ioctl request.  Called on low priority level.
 */
int
sppp_ioctl (struct ifnet *ifp, int cmd, void *data)
{
	struct ifreq *ifr = (struct ifreq*) data;
	struct sppp *sp = (struct sppp*) ifp;
	int s, going_up, going_down;

	switch (cmd) {
	default:
		return (EINVAL);

	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
		break;

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* fall through... */

	case SIOCSIFFLAGS:
		s = splimp ();
		going_up   = (ifp->if_flags & IFF_UP) &&
			     ! (ifp->if_flags & IFF_RUNNING);
		going_down = ! (ifp->if_flags & IFF_UP) &&
			      (ifp->if_flags & IFF_RUNNING);
		if (going_up || going_down) {
			/* Shut down the PPP link. */
			ifp->if_flags &= ~IFF_RUNNING;
			sp->lcp.state = LCP_STATE_CLOSED;
			sp->ipcp.state = IPCP_STATE_CLOSED;
			UNTIMO (sp);
		}
		if (going_up) {
			/* Interface is starting -- initiate negotiation. */
			ifp->if_flags |= IFF_RUNNING;
			if (!(sp->pp_flags & PP_CISCO))
				sppp_lcp_open (sp);
		}
		splx (s);
		break;

#ifdef SIOCSIFMTU
#ifndef ifr_mtu
#define ifr_mtu ifr_metric
#endif
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 128 || ifr->ifr_mtu > PP_MTU)
			return (EINVAL);
		ifp->if_mtu = ifr->ifr_mtu;
		break;
#endif
#ifdef SLIOCSETMTU
	case SLIOCSETMTU:
		if (*(short*)data < 128 || *(short*)data > PP_MTU)
			return (EINVAL);
		ifp->if_mtu = *(short*)data;
		break;
#endif
#ifdef SIOCGIFMTU
	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;
#endif
#ifdef SLIOCGETMTU
	case SLIOCGETMTU:
		*(short*)data = ifp->if_mtu;
		break;
#endif
#ifdef MULTICAST
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;
#endif
	}
	return (0);
}

/*
 * Analyze the LCP Configure-Request options list
 * for the presence of unknown options.
 * If the request contains unknown options, build and
 * send Configure-reject packet, containing only unknown options.
 */
static int
sppp_lcp_conf_parse_options (struct sppp *sp, struct lcp_header *h,
	int len, u_long *magic)
{
	u_char *buf, *r, *p;
	int rlen;

	len -= 4;
	buf = r = malloc (len, M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	p = (void*) (h+1);
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- extract. */
			if (len >= 6 && p[1] == 6) {
				*magic = (u_long)p[2] << 24 |
					(u_long)p[3] << 16 | p[4] << 8 | p[5];
				continue;
			}
			break;
		case LCP_OPT_ASYNC_MAP:
			/* Async control character map -- check to be zero. */
			if (len >= 6 && p[1] == 6 && ! p[2] && ! p[3] &&
			    ! p[4] && ! p[5])
				continue;
			break;
		case LCP_OPT_MRU:
			/* Maximum receive unit -- always OK. */
			continue;
		default:
			/* Others not supported. */
			break;
		}
		/* Add the option to rejected list. */
			bcopy (p, r, p[1]);
			r += p[1];
		rlen += p[1];
	}
	if (rlen)
	sppp_cp_send (sp, PPP_LCP, LCP_CONF_REJ, h->ident, rlen, buf);
	free (buf, M_TEMP);
	return (rlen == 0);
}

static void
sppp_ipcp_input (struct sppp *sp, struct mbuf *m)
{
	struct lcp_header *h;
	struct ifnet *ifp = &sp->pp_if;
	int len = m->m_pkthdr.len;

	if (len < 4) {
		/* if (ifp->if_flags & IFF_DEBUG) */
			printf ("%s%d: invalid ipcp packet length: %d bytes\n",
				ifp->if_name, ifp->if_unit, len);
		return;
	}
	h = mtod (m, struct lcp_header*);
	if (ifp->if_flags & IFF_DEBUG) {
		printf ("%s%d: ipcp input: %d bytes <%s id=%xh len=%xh",
			ifp->if_name, ifp->if_unit, len,
			sppp_ipcp_type_name (h->type), h->ident, ntohs (h->len));
		if (len > 4)
			sppp_print_bytes ((u_char*) (h+1), len-4);
		printf (">\n");
	}
	if (len > ntohs (h->len))
		len = ntohs (h->len);
	switch (h->type) {
	default:
		/* Unknown packet type -- send Code-Reject packet. */
		sppp_cp_send (sp, PPP_IPCP, IPCP_CODE_REJ, ++sp->pp_seq, len, h);
		break;
	case IPCP_CONF_REQ:
		if (len < 4) {
			if (ifp->if_flags & IFF_DEBUG)
				printf ("%s%d: invalid ipcp configure request packet length: %d bytes\n",
					ifp->if_name, ifp->if_unit, len);
			return;
		}
		if (len > 4) {
			sppp_cp_send (sp, PPP_IPCP, LCP_CONF_REJ, h->ident,
				len-4, h+1);

			switch (sp->ipcp.state) {
			case IPCP_STATE_OPENED:
				/* Initiate renegotiation. */
				sppp_ipcp_open (sp);
				/* fall through... */
			case IPCP_STATE_ACK_SENT:
				/* Go to closed state. */
				sp->ipcp.state = IPCP_STATE_CLOSED;
			}
		} else {
			/* Send Configure-Ack packet. */
			sppp_cp_send (sp, PPP_IPCP, IPCP_CONF_ACK, h->ident,
				0, 0);
			/* Change the state. */
			if (sp->ipcp.state == IPCP_STATE_ACK_RCVD)
				sp->ipcp.state = IPCP_STATE_OPENED;
			else
				sp->ipcp.state = IPCP_STATE_ACK_SENT;
		}
		break;
	case IPCP_CONF_ACK:
		if (h->ident != sp->ipcp.confid)
			break;
		UNTIMO (sp);
		switch (sp->ipcp.state) {
		case IPCP_STATE_CLOSED:
			sp->ipcp.state = IPCP_STATE_ACK_RCVD;
			TIMO (sp, 5);
			break;
		case IPCP_STATE_ACK_SENT:
			sp->ipcp.state = IPCP_STATE_OPENED;
			break;
		}
		break;
	case IPCP_CONF_NAK:
	case IPCP_CONF_REJ:
		if (h->ident != sp->ipcp.confid)
			break;
		UNTIMO (sp);
			/* Initiate renegotiation. */
			sppp_ipcp_open (sp);
		if (sp->ipcp.state != IPCP_STATE_ACK_SENT)
			/* Go to closed state. */
			sp->ipcp.state = IPCP_STATE_CLOSED;
		break;
	case IPCP_TERM_REQ:
		/* Send Terminate-Ack packet. */
		sppp_cp_send (sp, PPP_IPCP, IPCP_TERM_ACK, h->ident, 0, 0);
		/* Go to closed state. */
		sp->ipcp.state = IPCP_STATE_CLOSED;
			/* Initiate renegotiation. */
			sppp_ipcp_open (sp);
		break;
	case IPCP_TERM_ACK:
		/* Ignore for now. */
	case IPCP_CODE_REJ:
		/* Ignore for now. */
		break;
	}
}

static void
sppp_lcp_open (struct sppp *sp)
{
	char opt[6];

	if (! sp->lcp.magic)
	sp->lcp.magic = time.tv_sec + time.tv_usec;
	opt[0] = LCP_OPT_MAGIC;
	opt[1] = sizeof (opt);
	opt[2] = sp->lcp.magic >> 24;
	opt[3] = sp->lcp.magic >> 16;
	opt[4] = sp->lcp.magic >> 8;
	opt[5] = sp->lcp.magic;
	sp->lcp.confid = ++sp->pp_seq;
	sppp_cp_send (sp, PPP_LCP, LCP_CONF_REQ, sp->lcp.confid,
		sizeof (opt), &opt);
	TIMO (sp, 2);
}

static void
sppp_ipcp_open (struct sppp *sp)
{
	sp->ipcp.confid = ++sp->pp_seq;
	sppp_cp_send (sp, PPP_IPCP, IPCP_CONF_REQ, sp->ipcp.confid, 0, 0);
	TIMO (sp, 2);
}

/*
 * Process PPP control protocol timeouts.
 */
static void
sppp_cp_timeout (void *arg)
{
	struct sppp *sp = (struct sppp*) arg;
	int s = splimp ();

	sp->pp_flags &= ~PP_TIMO;
	if (! (sp->pp_if.if_flags & IFF_RUNNING) || (sp->pp_flags & PP_CISCO)) {
		splx (s);
		return;
	}
	switch (sp->lcp.state) {
	case LCP_STATE_CLOSED:
		/* No ACK for Configure-Request, retry. */
		sppp_lcp_open (sp);
		break;
	case LCP_STATE_ACK_RCVD:
		/* ACK got, but no Configure-Request for peer, retry. */
		sppp_lcp_open (sp);
		sp->lcp.state = LCP_STATE_CLOSED;
		break;
	case LCP_STATE_ACK_SENT:
		/* ACK sent but no ACK for Configure-Request, retry. */
		sppp_lcp_open (sp);
		break;
	case LCP_STATE_OPENED:
		/* LCP is already OK, try IPCP. */
		switch (sp->ipcp.state) {
		case IPCP_STATE_CLOSED:
			/* No ACK for Configure-Request, retry. */
			sppp_ipcp_open (sp);
			break;
		case IPCP_STATE_ACK_RCVD:
			/* ACK got, but no Configure-Request for peer, retry. */
			sppp_ipcp_open (sp);
			sp->ipcp.state = IPCP_STATE_CLOSED;
			break;
		case IPCP_STATE_ACK_SENT:
			/* ACK sent but no ACK for Configure-Request, retry. */
			sppp_ipcp_open (sp);
			break;
		case IPCP_STATE_OPENED:
			/* IPCP is OK. */
			break;
		}
		break;
	}
	splx (s);
}

static char
*sppp_lcp_type_name (u_char type)
{
	static char buf [8];
	switch (type) {
	case LCP_CONF_REQ:   return ("conf-req");
	case LCP_CONF_ACK:   return ("conf-ack");
	case LCP_CONF_NAK:   return ("conf-nack");
	case LCP_CONF_REJ:   return ("conf-rej");
	case LCP_TERM_REQ:   return ("term-req");
	case LCP_TERM_ACK:   return ("term-ack");
	case LCP_CODE_REJ:   return ("code-rej");
	case LCP_PROTO_REJ:  return ("proto-rej");
	case LCP_ECHO_REQ:   return ("echo-req");
	case LCP_ECHO_REPLY: return ("echo-reply");
	case LCP_DISC_REQ:   return ("discard-req");
	}
	sprintf (buf, "%xh", type);
	return (buf);
}

static char
*sppp_ipcp_type_name (u_char type)
{
	static char buf [8];
	switch (type) {
	case IPCP_CONF_REQ:   return ("conf-req");
	case IPCP_CONF_ACK:   return ("conf-ack");
	case IPCP_CONF_NAK:   return ("conf-nack");
	case IPCP_CONF_REJ:   return ("conf-rej");
	case IPCP_TERM_REQ:   return ("term-req");
	case IPCP_TERM_ACK:   return ("term-ack");
	case IPCP_CODE_REJ:   return ("code-rej");
	}
	sprintf (buf, "%xh", type);
	return (buf);
}

static void
sppp_print_bytes (u_char *p, u_short len)
{
	printf (" %x", *p++);
	while (--len > 0)
		printf ("-%x", *p++);
}
