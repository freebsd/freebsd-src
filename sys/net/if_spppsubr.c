/*
 * Synchronous PPP/Cisco link level subroutines.
 * Keepalive protocol implemented in both Cisco and PPP modes.
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
 * Authors grant any other persons or organisations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * From: Version 1.9, Wed Oct  4 18:58:15 MSK 1995
 *
 * $Id: if_spppsubr.c,v 1.19 1997/05/19 22:03:09 joerg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/syslog.h>
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

#define MAXALIVECNT     3               /* max. alive packets */

/*
 * Interface flags that can be set in an ifconfig command.
 *
 * Setting link0 will make the link passive, i.e. it will be marked
 * as being administrative openable, but won't be opened to begin
 * with.  Incoming calls will be answered, or subsequent calls with
 * -link1 will cause the administrative open of the LCP layer.
 *
 * Setting link1 will cause the link to auto-dial only as packets
 * arrive to be sent.
 */

#define IFF_PASSIVE	IFF_LINK0	/* wait passively for connection */
#define IFF_AUTO	IFF_LINK1	/* auto-dial on output */

#define PPP_ALLSTATIONS 0xff            /* All-Stations broadcast address */
#define PPP_UI          0x03            /* Unnumbered Information */
#define PPP_IP          0x0021          /* Internet Protocol */
#define PPP_ISO         0x0023          /* ISO OSI Protocol */
#define PPP_XNS         0x0025          /* Xerox NS Protocol */
#define PPP_IPX         0x002b          /* Novell IPX Protocol */
#define PPP_LCP         0xc021          /* Link Control Protocol */
#define PPP_IPCP        0x8021          /* Internet Protocol Control Protocol */

#define CONF_REQ	1		/* PPP configure request */
#define CONF_ACK	2		/* PPP configure acknowledge */
#define CONF_NAK	3		/* PPP configure negative ack */
#define CONF_REJ	4		/* PPP configure reject */
#define TERM_REQ	5		/* PPP terminate request */
#define TERM_ACK	6		/* PPP terminate acknowledge */
#define CODE_REJ	7		/* PPP code reject */
#define PROTO_REJ	8		/* PPP protocol reject */
#define ECHO_REQ	9		/* PPP echo request */
#define ECHO_REPLY	10		/* PPP echo reply */
#define DISC_REQ	11		/* PPP discard request */

#define LCP_OPT_MRU             1       /* maximum receive unit */
#define LCP_OPT_ASYNC_MAP       2       /* async control character map */
#define LCP_OPT_AUTH_PROTO      3       /* authentication protocol */
#define LCP_OPT_QUAL_PROTO      4       /* quality protocol */
#define LCP_OPT_MAGIC           5       /* magic number */
#define LCP_OPT_RESERVED        6       /* reserved */
#define LCP_OPT_PROTO_COMP      7       /* protocol field compression */
#define LCP_OPT_ADDR_COMP       8       /* address/control field compression */

#define IPCP_OPT_ADDRESSES	1	/* both IP addresses; deprecated */
#define IPCP_OPT_COMPRESSION	2	/* IP compression protocol (VJ) */
#define IPCP_OPT_ADDRESS	3	/* local IP address */

#define CISCO_MULTICAST         0x8f    /* Cisco multicast address */
#define CISCO_UNICAST           0x0f    /* Cisco unicast address */
#define CISCO_KEEPALIVE         0x8035  /* Cisco keepalive protocol */
#define CISCO_ADDR_REQ          0       /* Cisco address request */
#define CISCO_ADDR_REPLY        1       /* Cisco address reply */
#define CISCO_KEEPALIVE_REQ     2       /* Cisco keepalive request */

/* states are named and numbered according to RFC 1661 */
#define STATE_INITIAL	0
#define STATE_STARTING	1
#define STATE_CLOSED	2
#define STATE_STOPPED	3
#define STATE_CLOSING	4
#define STATE_STOPPING	5
#define STATE_REQ_SENT	6
#define STATE_ACK_RCVD	7
#define STATE_ACK_SENT	8
#define STATE_OPENED	9

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

/*
 * We follow the spelling and capitalization of RFC 1661 here, to make
 * it easier comparing with the standard.  Please refer to this RFC in
 * case you can't make sense out of these abbreviation; it will also
 * explain the semantics related to the various events and actions.
 */
struct cp {
	u_short	proto;		/* PPP control protocol number */
	u_char protoidx;	/* index into state table in struct sppp */
	u_char flags;
#define CP_LCP		0x01	/* this is the LCP */
#define CP_AUTH		0x02	/* this is an authentication protocol */
#define CP_NCP		0x04	/* this is a NCP */
#define CP_QUAL		0x08	/* this is a quality reporting protocol */
	const char *name;	/* name of this control protocol */
	/* event handlers */
	void	(*Up)(struct sppp *sp);
	void	(*Down)(struct sppp *sp);
	void	(*Open)(struct sppp *sp);
	void	(*Close)(struct sppp *sp);
	void	(*TO)(void *sp);
	int	(*RCR)(struct sppp *sp, struct lcp_header *h, int len);
	void	(*RCN_rej)(struct sppp *sp, struct lcp_header *h, int len);
	void	(*RCN_nak)(struct sppp *sp, struct lcp_header *h, int len);
	/* actions */
	void	(*tlu)(struct sppp *sp);
	void	(*tld)(struct sppp *sp);
	void	(*tls)(struct sppp *sp);
	void	(*tlf)(struct sppp *sp);
	void	(*scr)(struct sppp *sp);
};

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

/* almost every function needs these */
#define STDDCL							\
	struct ifnet *ifp = &sp->pp_if;				\
	int debug = ifp->if_flags & IFF_DEBUG

static int sppp_output(struct ifnet *ifp, struct mbuf *m, 
		       struct sockaddr *dst, struct rtentry *rt);

static void sppp_cisco_send(struct sppp *sp, int type, long par1, long par2);
static void sppp_cisco_input(struct sppp *sp, struct mbuf *m);

static void sppp_cp_input(const struct cp *cp, struct sppp *sp,
			  struct mbuf *m);
static void sppp_cp_send(struct sppp *sp, u_short proto, u_char type,
			 u_char ident, u_short len, void *data);
static void sppp_cp_timeout(void *arg);
static void sppp_cp_change_state(const struct cp *cp, struct sppp *sp,
				 int newstate);

static void sppp_up_event(const struct cp *cp, struct sppp *sp);
static void sppp_down_event(const struct cp *cp, struct sppp *sp);
static void sppp_open_event(const struct cp *cp, struct sppp *sp);
static void sppp_close_event(const struct cp *cp, struct sppp *sp);
static void sppp_to_event(const struct cp *cp, struct sppp *sp);

static void sppp_lcp_init(struct sppp *sp);
static void sppp_lcp_up(struct sppp *sp);
static void sppp_lcp_down(struct sppp *sp);
static void sppp_lcp_open(struct sppp *sp);
static void sppp_lcp_close(struct sppp *sp);
static void sppp_lcp_TO(void *sp);
static int sppp_lcp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_lcp_tlu(struct sppp *sp);
static void sppp_lcp_tld(struct sppp *sp);
static void sppp_lcp_tls(struct sppp *sp);
static void sppp_lcp_tlf(struct sppp *sp);
static void sppp_lcp_scr(struct sppp *sp);
static void sppp_lcp_check(struct sppp *sp);

static void sppp_ipcp_init(struct sppp *sp);
static void sppp_ipcp_up(struct sppp *sp);
static void sppp_ipcp_down(struct sppp *sp);
static void sppp_ipcp_open(struct sppp *sp);
static void sppp_ipcp_close(struct sppp *sp);
static void sppp_ipcp_TO(void *sp);
static int sppp_ipcp_RCR(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len);
static void sppp_ipcp_tlu(struct sppp *sp);
static void sppp_ipcp_tld(struct sppp *sp);
static void sppp_ipcp_tls(struct sppp *sp);
static void sppp_ipcp_tlf(struct sppp *sp);
static void sppp_ipcp_scr(struct sppp *sp);

static const char *sppp_cp_type_name(u_char type);
static const char *sppp_lcp_opt_name(u_char opt);
static const char *sppp_ipcp_opt_name(u_char opt);
static const char *sppp_state_name(int state);
static const char *sppp_phase_name(enum ppp_phase phase);
static const char *sppp_proto_name(u_short proto);

static void sppp_keepalive(void *dummy);
static void sppp_qflush(struct ifqueue *ifq);

static void sppp_get_ip_addrs(struct sppp *sp, u_long *src, u_long *dst);
static void sppp_set_ip_addr(struct sppp *sp, u_long src);

static void sppp_print_bytes(u_char *p, u_short len);

/* our control protocol descriptors */
const struct cp lcp = {
	PPP_LCP, IDX_LCP, CP_LCP, "lcp",
	sppp_lcp_up, sppp_lcp_down, sppp_lcp_open, sppp_lcp_close,
	sppp_lcp_TO, sppp_lcp_RCR, sppp_lcp_RCN_rej, sppp_lcp_RCN_nak,
	sppp_lcp_tlu, sppp_lcp_tld, sppp_lcp_tls, sppp_lcp_tlf,
	sppp_lcp_scr
};

const struct cp ipcp = {
	PPP_IPCP, IDX_IPCP, CP_NCP, "ipcp",
	sppp_ipcp_up, sppp_ipcp_down, sppp_ipcp_open, sppp_ipcp_close,
	sppp_ipcp_TO, sppp_ipcp_RCR, sppp_ipcp_RCN_rej, sppp_ipcp_RCN_nak,
	sppp_ipcp_tlu, sppp_ipcp_tld, sppp_ipcp_tls, sppp_ipcp_tlf,
	sppp_ipcp_scr
};

const struct cp *cps[IDX_COUNT] = {
	&lcp,			/* IDX_LCP */
	&ipcp,			/* IDX_IPCP */
};


/*
 * Exported functions, comprising our interface to the lower layer.
 */

/*
 * Process the received packet.
 */
void
sppp_input(struct ifnet *ifp, struct mbuf *m)
{
	struct ppp_header *h;
	struct ifqueue *inq = 0;
	int s;
	struct sppp *sp = (struct sppp *)ifp;
	int debug = ifp->if_flags & IFF_DEBUG;

	if (ifp->if_flags & IFF_UP)
		/* Count received bytes, add FCS and one flag */
		ifp->if_ibytes += m->m_pkthdr.len + 3;

	if (m->m_pkthdr.len <= PPP_HEADER_LEN) {
		/* Too small packet, drop it. */
		if (debug)
			log(LOG_DEBUG,
			    "%s%d: input packet is too small, %d bytes\n",
			    ifp->if_name, ifp->if_unit, m->m_pkthdr.len);
	  drop:
		++ifp->if_ierrors;
		++ifp->if_iqdrops;
		m_freem (m);
		return;
	}

	/* Get PPP header. */
	h = mtod (m, struct ppp_header*);
	m_adj (m, PPP_HEADER_LEN);

	switch (h->address) {
	case PPP_ALLSTATIONS:
		if (h->control != PPP_UI)
			goto invalid;
		if (sp->pp_flags & PP_CISCO) {
			if (debug)
				log(LOG_DEBUG,
				    "%s%d: PPP packet in Cisco mode "
				    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
				    ifp->if_name, ifp->if_unit,
				    h->address, h->control, ntohs(h->protocol));
			goto drop;
		}
		switch (ntohs (h->protocol)) {
		default:
			if (sp->state[IDX_LCP] == STATE_OPENED)
				sppp_cp_send (sp, PPP_LCP, PROTO_REJ,
					++sp->pp_seq, m->m_pkthdr.len + 2,
					&h->protocol);
			if (debug)
				log(LOG_DEBUG,
				    "%s%d: invalid input protocol "
				    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
				    ifp->if_name, ifp->if_unit,
				    h->address, h->control, ntohs(h->protocol));
			++ifp->if_noproto;
			goto drop;
		case PPP_LCP:
			sppp_cp_input(&lcp, (struct sppp*)ifp, m);
			m_freem (m);
			return;
#ifdef INET
		case PPP_IPCP:
			if (sp->pp_phase == PHASE_NETWORK)
				sppp_cp_input(&ipcp, (struct sppp*) ifp, m);
			m_freem (m);
			return;
		case PPP_IP:
			if (sp->state[IDX_IPCP] == STATE_OPENED) {
				schednetisr (NETISR_IP);
				inq = &ipintrq;
			}
			break;
#endif
#ifdef IPX
		case PPP_IPX:
			/* IPX IPXCP not implemented yet */
			if (sp->pp_phase == PHASE_NETWORK) {
				schednetisr (NETISR_IPX);
				inq = &ipxintrq;
			}
			break;
#endif
#ifdef NS
		case PPP_XNS:
			/* XNS IDPCP not implemented yet */
			if (sp->pp_phase == PHASE_NETWORK) {
				schednetisr (NETISR_NS);
				inq = &nsintrq;
			}
			break;
#endif
#ifdef ISO
		case PPP_ISO:
			/* OSI NLCP not implemented yet */
			if (sp->pp_phase == PHASE_NETWORK) {
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
			if (debug)
				log(LOG_DEBUG,
				    "%s%d: Cisco packet in PPP mode "
				    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
				    ifp->if_name, ifp->if_unit,
				    h->address, h->control, ntohs(h->protocol));
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
	default:        /* Invalid PPP packet. */
	  invalid:
		if (debug)
			log(LOG_DEBUG,
			    "%s%d: invalid input packet "
			    "<addr=0x%x ctrl=0x%x proto=0x%x>\n",
			    ifp->if_name, ifp->if_unit,
			    h->address, h->control, ntohs(h->protocol));
		goto drop;
	}

	if (! (ifp->if_flags & IFF_UP) || ! inq)
		goto drop;

	/* Check queue. */
	s = splimp();
	if (IF_QFULL (inq)) {
		/* Queue overflow. */
		IF_DROP(inq);
		splx(s);
		if (debug)
			log(LOG_DEBUG, "%s%d: protocol queue overflow\n",
				ifp->if_name, ifp->if_unit);
		goto drop;
	}
	IF_ENQUEUE(inq, m);
	splx(s);
}

/*
 * Enqueue transmit packet.
 */
static int
sppp_output(struct ifnet *ifp, struct mbuf *m,
	    struct sockaddr *dst, struct rtentry *rt)
{
	struct sppp *sp = (struct sppp*) ifp;
	struct ppp_header *h;
	struct ifqueue *ifq;
	int s, rv = 0;

	s = splimp();

	if ((ifp->if_flags & IFF_UP) == 0 ||
	    (ifp->if_flags & (IFF_RUNNING | IFF_AUTO)) == 0) {
		m_freem (m);
		splx (s);
		return (ENETDOWN);
	}

	if ((ifp->if_flags & (IFF_RUNNING | IFF_AUTO)) == IFF_AUTO) {
		/*
		 * Interface is not yet running, but auto-dial.  Need
		 * to start LCP for it.
		 */
		ifp->if_flags |= IFF_RUNNING;
		splx(s);
		lcp.Open(sp);
		s = splimp();
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
			log(LOG_DEBUG, "%s%d: no memory for transmit header\n",
				ifp->if_name, ifp->if_unit);
		++ifp->if_oerrors;
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
		else {
			/*
			 * Don't choke with an ENETDOWN early.  It's
			 * possible that we just started dialing out,
			 * so don't drop the packet immediately.  If
			 * we notice that we run out of buffer space
			 * below, we will however remember that we are
			 * not ready to carry IP packets, and return
			 * ENETDOWN, as opposed to ENOBUFS.
			 */
			h->protocol = htons(PPP_IP);
			if (sp->state[IDX_IPCP] != STATE_OPENED)
				rv = ENETDOWN;
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
		++ifp->if_oerrors;
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
		++ifp->if_oerrors;
		splx (s);
		return (rv? rv: ENOBUFS);
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

void
sppp_attach(struct ifnet *ifp)
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
	sp->pp_phase = PHASE_DEAD;
	sp->pp_up = lcp.Up;
	sp->pp_down = lcp.Down;

	sppp_lcp_init(sp);
	sppp_ipcp_init(sp);
}

void 
sppp_detach(struct ifnet *ifp)
{
	struct sppp **q, *p, *sp = (struct sppp*) ifp;
	int i;

	/* Remove the entry from the keepalive list. */
	for (q = &spppq; (p = *q); q = &p->pp_next)
		if (p == sp) {
			*q = p->pp_next;
			break;
		}

	/* Stop keepalive handler. */
	if (! spppq)
		untimeout (sppp_keepalive, 0);

	for (i = 0; i < IDX_COUNT; i++)
		untimeout((cps[i])->TO, (void *)sp);
}

/*
 * Flush the interface output queue.
 */
void
sppp_flush(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;

	sppp_qflush (&sp->pp_if.if_snd);
	sppp_qflush (&sp->pp_fastq);
}

/*
 * Check if the output queue is empty.
 */
int
sppp_isempty(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;
	int empty, s;

	s = splimp();
	empty = !sp->pp_fastq.ifq_head && !sp->pp_if.if_snd.ifq_head;
	splx(s);
	return (empty);
}

/*
 * Get next packet to send.
 */
struct mbuf *
sppp_dequeue(struct ifnet *ifp)
{
	struct sppp *sp = (struct sppp*) ifp;
	struct mbuf *m;
	int s;

	s = splimp();
	IF_DEQUEUE (&sp->pp_fastq, m);
	if (! m)
		IF_DEQUEUE (&sp->pp_if.if_snd, m);
	splx (s);
	return (m);
}

/*
 * Process an ioctl request.  Called on low priority level.
 */
int
sppp_ioctl(struct ifnet *ifp, int cmd, void *data)
{
	struct ifreq *ifr = (struct ifreq*) data;
	struct sppp *sp = (struct sppp*) ifp;
	int s, going_up, going_down, newmode;

	s = splimp();
	switch (cmd) {
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
		break;

	case SIOCSIFADDR:
		if_up(ifp);
		/* fall through... */

	case SIOCSIFFLAGS:
		going_up = ifp->if_flags & IFF_UP &&
			(ifp->if_flags & IFF_RUNNING) == 0;
		going_down = (ifp->if_flags & IFF_UP) == 0 &&
			ifp->if_flags & IFF_RUNNING;
		newmode = ifp->if_flags & (IFF_AUTO | IFF_PASSIVE);
		if (newmode == (IFF_AUTO | IFF_PASSIVE)) {
			/* sanity */
			newmode = IFF_PASSIVE;
			ifp->if_flags &= ~IFF_AUTO;
		}

		if (going_up || going_down)
			lcp.Close(sp);
		if (going_up && newmode == 0) {
			/* neither auto-dial nor passive */
			ifp->if_flags |= IFF_RUNNING;
			if (!(sp->pp_flags & PP_CISCO))
				lcp.Open(sp);
		} else if (going_down)
			ifp->if_flags &= ~IFF_RUNNING;

		break;

#ifdef SIOCSIFMTU
#ifndef ifr_mtu
#define ifr_mtu ifr_metric
#endif
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 128 || ifr->ifr_mtu > sp->lcp.their_mru)
			return (EINVAL);
		ifp->if_mtu = ifr->ifr_mtu;
		break;
#endif
#ifdef SLIOCSETMTU
	case SLIOCSETMTU:
		if (*(short*)data < 128 || *(short*)data > sp->lcp.their_mru)
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
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	default:
		splx(s);
		return (ENOTTY);
	}
	splx(s);
	return (0);
}


/*
 * Cisco framing implementation.
 */

/*
 * Handle incoming Cisco keepalive protocol packets.
 */
static void 
sppp_cisco_input(struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct cisco_packet *h;
	struct ifaddr *ifa;

	if (m->m_pkthdr.len != CISCO_PACKET_LEN) {
		if (debug)
			log(LOG_DEBUG,
			    "%s%d: invalid cisco packet length: %d bytes\n",
			    ifp->if_name, ifp->if_unit, m->m_pkthdr.len);
		return;
	}
	h = mtod (m, struct cisco_packet*);
	if (debug)
		log(LOG_DEBUG,
		    "%s%d: cisco input: %d bytes "
		    "<0x%lx 0x%lx 0x%lx 0x%x 0x%x-0x%x>\n",
		    ifp->if_name, ifp->if_unit, m->m_pkthdr.len,
		    ntohl (h->type), h->par1, h->par2, h->rel,
		    h->time0, h->time1);
	switch (ntohl (h->type)) {
	default:
		if (debug)
			addlog("%s%d: unknown cisco packet type: 0x%lx\n",
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
					sppp_qflush (&sp->pp_fastq);
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
		for (ifa=ifp->if_addrhead.tqh_first; ifa; 
		     ifa=ifa->ifa_link.tqe_next)
			if (ifa->ifa_addr->sa_family == AF_INET)
				break;
		if (! ifa) {
			if (debug)
				addlog("%s%d: unknown address for cisco request\n",
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
 * Send Cisco keepalive packet.
 */
static void
sppp_cisco_send(struct sppp *sp, int type, long par1, long par2)
{
	STDDCL;
	struct ppp_header *h;
	struct cisco_packet *ch;
	struct mbuf *m;
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

	if (debug)
		log(LOG_DEBUG,
		    "%s%d: cisco output: <0x%lx 0x%lx 0x%lx 0x%x 0x%x-0x%x>\n",
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
 * PPP protocol implementation.
 */

/*
 * Send PPP control protocol packet.
 */
static void
sppp_cp_send(struct sppp *sp, u_short proto, u_char type,
	     u_char ident, u_short len, void *data)
{
	STDDCL;
	struct ppp_header *h;
	struct lcp_header *lh;
	struct mbuf *m;

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

	if (debug) {
		log(LOG_DEBUG, "%s%d: %s output <%s id=0x%x len=%d",
		    ifp->if_name, ifp->if_unit,
		    sppp_proto_name(proto),
		    sppp_cp_type_name (lh->type), lh->ident,
		    ntohs (lh->len));
		if (len)
			sppp_print_bytes ((u_char*) (lh+1), len);
		addlog(">\n");
	}
	if (IF_QFULL (&sp->pp_fastq)) {
		IF_DROP (&ifp->if_snd);
		m_freem (m);
		++ifp->if_oerrors;
	} else
		IF_ENQUEUE (&sp->pp_fastq, m);
	if (! (ifp->if_flags & IFF_OACTIVE))
		(*ifp->if_start) (ifp);
	ifp->if_obytes += m->m_pkthdr.len + 3;
}

/*
 * Handle incoming PPP control protocol packets.
 */
static void
sppp_cp_input(const struct cp *cp, struct sppp *sp, struct mbuf *m)
{
	STDDCL;
	struct lcp_header *h;
	int len = m->m_pkthdr.len;
	int rv;
	u_char *p;

	if (len < 4) {
		if (debug)
			log(LOG_DEBUG,
			    "%s%d: %s invalid packet length: %d bytes\n",
			    ifp->if_name, ifp->if_unit, cp->name, len);
		return;
	}
	h = mtod (m, struct lcp_header*);
	if (debug) {
		log(LOG_DEBUG,
		    "%s%d: %s input(%s): <%s id=0x%x len=%d",
		    ifp->if_name, ifp->if_unit, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]),
		    sppp_cp_type_name (h->type), h->ident, ntohs (h->len));
		if (len > 4)
			sppp_print_bytes ((u_char*) (h+1), len-4);
		addlog(">\n");
	}
	if (len > ntohs (h->len))
		len = ntohs (h->len);
	switch (h->type) {
	case CONF_REQ:
		if (len < 4) {
			if (debug)
				addlog("%s%d: %s invalid conf-req length %d\n",
				       ifp->if_name, ifp->if_unit, cp->name,
				       len);
			++ifp->if_ierrors;
			break;
		}
		rv = (cp->RCR)(sp, h, len);
		switch (sp->state[cp->protoidx]) {
		case STATE_OPENED:
			(cp->tld)(sp);
			(cp->scr)(sp);
			/* fall through... */
		case STATE_ACK_SENT:
		case STATE_REQ_SENT:
			sppp_cp_change_state(cp, sp, rv?
					     STATE_ACK_SENT: STATE_REQ_SENT);
			break;
		case STATE_CLOSING:
		case STATE_STOPPING:
			break;
		case STATE_STOPPED:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, rv?
					     STATE_ACK_SENT: STATE_REQ_SENT);
			break;
		case STATE_CLOSED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident,
				     0, 0);
			break;
		case STATE_ACK_RCVD:
			if (rv) {
				sppp_cp_change_state(cp, sp, STATE_OPENED);
				if (debug)
					addlog("%s%d: %s tlu\n",
					       ifp->if_name, ifp->if_unit,
					       cp->name);
				(cp->tlu)(sp);
			} else
				sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		default:
			printf("%s%d: %s illegal %s in state %s\n",
			       ifp->if_name, ifp->if_unit, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CONF_ACK:
		if (h->ident != sp->confid[cp->protoidx]) {
			if (debug)
				addlog("%s%d: %s id mismatch 0x%x != 0x%x\n",
				       ifp->if_name, ifp->if_unit, cp->name,
				       h->ident, sp->confid[cp->protoidx]);
			++ifp->if_ierrors;
			break;
		}
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_CLOSING:
		case STATE_STOPPING:
			break;
		case STATE_REQ_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			/* fall through */
		case STATE_ACK_RCVD:
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_ACK_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			sppp_cp_change_state(cp, sp, STATE_OPENED);
			if (debug)
				addlog("%s%d: %s tlu\n",
				       ifp->if_name, ifp->if_unit, cp->name);
			(cp->tlu)(sp);
			break;
		default:
			printf("%s%d: %s illegal %s in state %s\n",
			       ifp->if_name, ifp->if_unit, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CONF_NAK:
	case CONF_REJ:
		if (h->ident != sp->confid[cp->protoidx]) {
			if (debug)
				addlog("%s%d: %s id mismatch 0x%x != 0x%x\n",
				       ifp->if_name, ifp->if_unit, cp->name,
				       h->ident, sp->confid[cp->protoidx]);
			++ifp->if_ierrors;
			break;
		}
		if (h->type == CONF_NAK)
			(cp->RCN_nak)(sp, h, len);
		else /* CONF_REJ */
			(cp->RCN_rej)(sp, h, len);

		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
			sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
			(cp->scr)(sp);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			/* fall through */
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_ACK_SENT);
			(cp->scr)(sp);
			break;
		case STATE_CLOSING:
		case STATE_STOPPING:
			break;
		default:
			printf("%s%d: %s illegal %s in state %s\n",
			       ifp->if_name, ifp->if_unit, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;

	case TERM_REQ:
		switch (sp->state[cp->protoidx]) {
		case STATE_ACK_RCVD:
		case STATE_ACK_SENT:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			/* fall through */
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_REQ_SENT:
		  sta:
			/* Send Terminate-Ack packet. */
			if (debug)
				log(LOG_DEBUG, "%s%d: %s send terminate-ack\n",
				    ifp->if_name, ifp->if_unit, cp->name);
			sppp_cp_send(sp, cp->proto, TERM_ACK, h->ident, 0, 0);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			sp->rst_counter[cp->protoidx] = 0;
			sppp_cp_change_state(cp, sp, STATE_STOPPING);
			goto sta;
			break;
		default:
			printf("%s%d: %s illegal %s in state %s\n",
			       ifp->if_name, ifp->if_unit, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case TERM_ACK:
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
			break;
		case STATE_CLOSING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_CLOSED);
			break;
		case STATE_STOPPING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_OPENED:
			(cp->tld)(sp);
			(cp->scr)(sp);
			sppp_cp_change_state(cp, sp, STATE_ACK_RCVD);
			break;
		default:
			printf("%s%d: %s illegal %s in state %s\n",
			       ifp->if_name, ifp->if_unit, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case CODE_REJ:
	case PROTO_REJ:
		/* XXX catastrophic rejects (RXJ-) aren't handled yet. */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSED:
		case STATE_STOPPED:
		case STATE_REQ_SENT:
		case STATE_ACK_SENT:
		case STATE_CLOSING:
		case STATE_STOPPING:
		case STATE_OPENED:
			break;
		case STATE_ACK_RCVD:
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		default:
			printf("%s%d: %s illegal %s in state %s\n",
			       ifp->if_name, ifp->if_unit, cp->name,
			       sppp_cp_type_name(h->type),
			       sppp_state_name(sp->state[cp->protoidx]));
			++ifp->if_ierrors;
		}
		break;
	case DISC_REQ:
		if (cp->proto != PPP_LCP)
			goto illegal;
		/* Discard the packet. */
		break;
	case ECHO_REQ:
		if (cp->proto != PPP_LCP)
			goto illegal;
		if (sp->state[cp->protoidx] != STATE_OPENED) {
			if (debug)
				addlog("%s%d: lcp echo req but lcp closed\n",
				       ifp->if_name, ifp->if_unit);
			++ifp->if_ierrors;
			break;
		}
		if (len < 8) {
			if (debug)
				addlog("%s%d: invalid lcp echo request "
				       "packet length: %d bytes\n",
				       ifp->if_name, ifp->if_unit, len);
			break;
		}
		if (ntohl (*(long*)(h+1)) == sp->lcp.magic) {
			/* Line loopback mode detected. */
			printf("%s%d: loopback\n", ifp->if_name, ifp->if_unit);
			if_down (ifp);
			sppp_qflush (&sp->pp_fastq);

			/* Shut down the PPP link. */
			/* XXX */
			lcp.Down(sp);
			lcp.Up(sp);
			break;
		}
		*(long*)(h+1) = htonl (sp->lcp.magic);
		if (debug)
			addlog("%s%d: got lcp echo req, sending echo rep\n",
			       ifp->if_name, ifp->if_unit);
		sppp_cp_send (sp, PPP_LCP, ECHO_REPLY, h->ident, len-4, h+1);
		break;
	case ECHO_REPLY:
		if (cp->proto != PPP_LCP)
			goto illegal;
		if (h->ident != sp->lcp.echoid) {
			++ifp->if_ierrors;
			break;
		}
		if (len < 8) {
			if (debug)
				addlog("%s%d: lcp invalid echo reply "
				       "packet length: %d bytes\n",
				       ifp->if_name, ifp->if_unit, len);
			break;
		}
		if (debug)
			addlog("%s%d: lcp got echo rep\n",
			       ifp->if_name, ifp->if_unit);
		if (ntohl (*(long*)(h+1)) != sp->lcp.magic)
			sp->pp_alivecnt = 0;
		break;
	default:
		/* Unknown packet type -- send Code-Reject packet. */
	  illegal:
		if (debug)
			addlog("%s%d: %c send code-rej for 0x%x\n",
			       ifp->if_name, ifp->if_unit, cp->name, h->type);
		sppp_cp_send(sp, cp->proto, CODE_REJ, ++sp->pp_seq,
			     m->m_pkthdr.len, h);
		++ifp->if_ierrors;
	}
}


/*
 * The generic part of all Up/Down/Open/Close/TO event handlers.
 * Basically, the state transition handling in the automaton.
 */
static void
sppp_up_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s%d: %s up(%s)\n",
		    ifp->if_name, ifp->if_unit, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
		sppp_cp_change_state(cp, sp, STATE_CLOSED);
		break;
	case STATE_STARTING:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
		(cp->scr)(sp);
		sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
		break;
	default:
		printf("%s%d: %s illegal up in state %s\n",
		       ifp->if_name, ifp->if_unit, cp->name,
		       sppp_state_name(sp->state[cp->protoidx]));
	}
}

static void
sppp_down_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s%d: %s down(%s)\n",
		    ifp->if_name, ifp->if_unit, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_CLOSED:
	case STATE_CLOSING:
		sppp_cp_change_state(cp, sp, STATE_INITIAL);
		break;
	case STATE_STOPPED:
		(cp->tls)(sp);
		/* fall through */
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	case STATE_OPENED:
		(cp->tld)(sp);
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	default:
		printf("%s%d: %s illegal down in state %s\n",
		       ifp->if_name, ifp->if_unit, cp->name,
		       sppp_state_name(sp->state[cp->protoidx]));
	}
}


static void
sppp_open_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s%d: %s open(%s)\n",
		    ifp->if_name, ifp->if_unit, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
		(cp->tls)(sp);
		sppp_cp_change_state(cp, sp, STATE_STARTING);
		break;
	case STATE_STARTING:
		break;
	case STATE_CLOSED:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_configure;
		(cp->scr)(sp);
		sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
		break;
	case STATE_STOPPED:
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
	case STATE_OPENED:
		break;
	case STATE_CLOSING:
		sppp_cp_change_state(cp, sp, STATE_STOPPING);
		break;
	}
}


static void
sppp_close_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;

	if (debug)
		log(LOG_DEBUG, "%s%d: %s close(%s)\n",
		    ifp->if_name, ifp->if_unit, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]));

	switch (sp->state[cp->protoidx]) {
	case STATE_INITIAL:
	case STATE_CLOSED:
	case STATE_CLOSING:
		break;
	case STATE_STARTING:
		(cp->tlf)(sp);
		sppp_cp_change_state(cp, sp, STATE_INITIAL);
		break;
	case STATE_STOPPED:
		sppp_cp_change_state(cp, sp, STATE_CLOSED);
		break;
	case STATE_STOPPING:
		sppp_cp_change_state(cp, sp, STATE_CLOSING);
		break;
	case STATE_OPENED:
		(cp->tld)(sp);
		/* fall through */
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		sp->rst_counter[cp->protoidx] = sp->lcp.max_terminate;
		sppp_cp_send(sp, cp->proto, TERM_REQ, ++sp->pp_seq, 0, 0);
		sppp_cp_change_state(cp, sp, STATE_CLOSING);
		break;
	}
}

static void
sppp_to_event(const struct cp *cp, struct sppp *sp)
{
	STDDCL;
	int s;

	s = splimp();
	if (debug)
		log(LOG_DEBUG, "%s%d: %s TO(%s) rst_counter = %d\n",
		    ifp->if_name, ifp->if_unit, cp->name,
		    sppp_state_name(sp->state[cp->protoidx]),
		    sp->rst_counter[cp->protoidx]);

	if (--sp->rst_counter[cp->protoidx] < 0)
		/* TO- event */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_CLOSED);
			break;
		case STATE_STOPPING:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_RCVD:
		case STATE_ACK_SENT:
			(cp->tlf)(sp);
			sppp_cp_change_state(cp, sp, STATE_STOPPED);
			break;
		}
	else
		/* TO+ event */
		switch (sp->state[cp->protoidx]) {
		case STATE_CLOSING:
		case STATE_STOPPING:
			sppp_cp_send(sp, cp->proto, TERM_REQ, ++sp->pp_seq,
				     0, 0);
			timeout(cp->TO, (void *)sp, sp->lcp.timeout);
			break;
		case STATE_REQ_SENT:
		case STATE_ACK_RCVD:
			(cp->scr)(sp);
			/* sppp_cp_change_state() will restart the timer */
			sppp_cp_change_state(cp, sp, STATE_REQ_SENT);
			break;
		case STATE_ACK_SENT:
			(cp->scr)(sp);
			timeout(cp->TO, (void *)sp, sp->lcp.timeout);
			break;
		}

	splx(s);
}

/*
 * Change the state of a control protocol in the state automaton.
 * Takes care of starting/stopping the restart timer.
 */
void
sppp_cp_change_state(const struct cp *cp, struct sppp *sp, int newstate)
{
	sp->state[cp->protoidx] = newstate;

	untimeout(cp->TO, (void *)sp);
	switch (newstate) {
	case STATE_INITIAL:
	case STATE_STARTING:
	case STATE_CLOSED:
	case STATE_STOPPED:
	case STATE_OPENED:
		break;
	case STATE_CLOSING:
	case STATE_STOPPING:
	case STATE_REQ_SENT:
	case STATE_ACK_RCVD:
	case STATE_ACK_SENT:
		timeout(cp->TO, (void *)sp, sp->lcp.timeout);
		break;
	}
}
/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                         The LCP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */
static void
sppp_lcp_init(struct sppp *sp)
{
	sp->lcp.opts = (1 << LCP_OPT_MAGIC);
	sp->lcp.magic = 0;
	sp->state[IDX_LCP] = STATE_INITIAL;
	sp->fail_counter[IDX_LCP] = 0;
	sp->lcp.protos = 0;
	sp->lcp.mru = sp->lcp.their_mru = PP_MTU;
	
	/*
	 * Initialize counters and timeout values.  Note that we don't
	 * use the 3 seconds suggested in RFC 1661 since we are likely
	 * running on a fast link.  XXX We should probably implement
	 * the exponential backoff option.  Note that these values are
	 * relevant for all control protocols, not just LCP only.
	 */
	sp->lcp.timeout = 1 * hz;
	sp->lcp.max_terminate = 2;
	sp->lcp.max_configure = 10;
	sp->lcp.max_failure = 10;
}

static void
sppp_lcp_up(struct sppp *sp)
{
	STDDCL;

	/*
	 * If this interface is passive or dial-on-demand, it means
	 * we've got in incoming call.  Activate the interface.
	 */
	if ((ifp->if_flags & (IFF_AUTO | IFF_PASSIVE)) != 0) {
		if (debug)
			log(LOG_DEBUG,
			    "%s%d: Up event (incoming call)\n",
			    ifp->if_name, ifp->if_unit);
		ifp->if_flags |= IFF_RUNNING;
		lcp.Open(sp);
	}

	sppp_up_event(&lcp, sp);
}

static void
sppp_lcp_down(struct sppp *sp)
{
	STDDCL;

	sppp_down_event(&lcp, sp);

	/*
	 * If this is neither a dial-on-demand nor a passive
	 * interface, simulate an ``ifconfig down'' action, so the
	 * administrator can force a redial by another ``ifconfig
	 * up''.  XXX For leased line operation, should we immediately
	 * try to reopen the connection here?
	 */
	if ((ifp->if_flags & (IFF_AUTO | IFF_PASSIVE)) == 0) {
		log(LOG_INFO,
		    "%s%d: Down event (carrier loss), taking interface down.\n",
		    ifp->if_name, ifp->if_unit);
		if_down(ifp);
	} else {
		if (debug)
			log(LOG_DEBUG,
			    "%s%d: Down event (carrier loss)\n",
			    ifp->if_name, ifp->if_unit);
	}
	lcp.Close(sp);
	ifp->if_flags &= ~IFF_RUNNING;
}

static void
sppp_lcp_open(struct sppp *sp)
{
	sppp_open_event(&lcp, sp);
}

static void
sppp_lcp_close(struct sppp *sp)
{
	sppp_close_event(&lcp, sp);
}

static void
sppp_lcp_TO(void *cookie)
{
	sppp_to_event(&lcp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_lcp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *r, *p;
	int origlen, rlen;
	u_long nmagic;

	len -= 4;
	origlen = len;
	buf = r = malloc (len, M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	if (debug)
		log(LOG_DEBUG, "%s%d: lcp parse opts: ",
		    ifp->if_name, ifp->if_unit);

	/* pass 1: check for things that need to be rejected */
	p = (void*) (h+1);
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s ", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number. */
			/* fall through, both are same length */
		case LCP_OPT_ASYNC_MAP:
			/* Async control character map. */
			if (len >= 6 || p[1] == 6)
				continue;
			if (debug)
				addlog("[invalid] ");
			break;
		case LCP_OPT_MRU:
			/* Maximum receive unit. */
			if (len >= 4 && p[1] == 4)
				continue;
			if (debug)
				addlog("[invalid] ");
			break;
		default:
			/* Others not supported. */
			if (debug)
				addlog("[rej] ");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send (sp, PPP_LCP, CONF_REJ, h->ident, rlen, buf);
		return 0;
	} else if (debug)
		addlog("\n");

	/*
	 * pass 2: check for option values that are unacceptable and
	 * thus require to be nak'ed.
	 */
	if (debug)
		addlog("%s%d: lcp parse opt values: ",
		       ifp->if_name, ifp->if_unit);

	p = (void*) (h+1);
	len = origlen;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s ", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- extract. */
			nmagic = (u_long)p[2] << 24 |
				(u_long)p[3] << 16 | p[4] << 8 | p[5];
			if (nmagic != sp->lcp.magic) {
				if (debug)
					addlog("0x%x ", nmagic);
				continue;
			}
			/*
			 * Local and remote magics equal -- loopback?
			 */
			if (sp->pp_loopcnt >= MAXALIVECNT*5) {
				printf ("\n%s%d: loopback\n",
					ifp->if_name, ifp->if_unit);
				sp->pp_loopcnt = 0;
				if (ifp->if_flags & IFF_UP) {
					if_down(ifp);
					sppp_qflush(&sp->pp_fastq);
					/* XXX ? */
					lcp.Down(sp);
					lcp.Up(sp);
				}
			} else if (debug)
				addlog("[glitch] ");
			++sp->pp_loopcnt;
			/*
			 * We negate our magic here, and NAK it.  If
			 * we see it later in an NAK packet, we
			 * suggest a new one.
			 */
			nmagic = ~sp->lcp.magic;
			/* Gonna NAK it. */
			p[2] = nmagic >> 24;
			p[3] = nmagic >> 16;
			p[4] = nmagic >> 8;
			p[5] = nmagic;
			break;

		case LCP_OPT_ASYNC_MAP:
			/* Async control character map -- check to be zero. */
			if (! p[2] && ! p[3] && ! p[4] && ! p[5]) {
				if (debug)
					addlog("[empty] ");
				continue;
			}
			if (debug)
				addlog("[non-empty] ");
			/* suggest a zero one */
			p[2] = p[3] = p[4] = p[5] = 0;
			break;

		case LCP_OPT_MRU:
			/*
			 * Maximum receive unit.  Always agreeable,
			 * but ignored by now.
			 */
			sp->lcp.their_mru = p[2] * 256 + p[3];
			if (debug)
				addlog("%d ", sp->lcp.their_mru);
			continue;
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-nak\n");
		sppp_cp_send (sp, PPP_LCP, CONF_NAK, h->ident, rlen, buf);
		return 0;
	} else {
		if (debug)
			addlog(" send conf-ack\n");
		sp->pp_loopcnt = 0;
		sppp_cp_send (sp, PPP_LCP, CONF_ACK,
			      h->ident, origlen, h+1);
	}

	free (buf, M_TEMP);
	return (rlen == 0);
}

/*
 * Analyze the LCP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_lcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *p;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s%d: lcp rej opts: ",
		    ifp->if_name, ifp->if_unit);

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s ", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- can't use it, use 0 */
			sp->lcp.opts &= ~(1 << LCP_OPT_MAGIC);
			sp->lcp.magic = 0;
			break;
		case LCP_OPT_MRU:
			/*
			 * Should not be rejected anyway, since we only
			 * negotiate a MRU if explicitly requested by
			 * peer.
			 */
			sp->lcp.opts &= ~(1 << LCP_OPT_MRU);
			break;
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

/*
 * Analyze the LCP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_lcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	STDDCL;
	u_char *buf, *p;
	u_long magic;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s%d: lcp nak opts: ",
		    ifp->if_name, ifp->if_unit);

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s ", sppp_lcp_opt_name(*p));
		switch (*p) {
		case LCP_OPT_MAGIC:
			/* Magic number -- renegotiate */
			if ((sp->lcp.opts & (1 << LCP_OPT_MAGIC)) &&
			    len >= 6 && p[1] == 6) {
				magic = (u_long)p[2] << 24 |
					(u_long)p[3] << 16 | p[4] << 8 | p[5];
				/*
				 * If the remote magic is our negated one,
				 * this looks like a loopback problem.
				 * Suggest a new magic to make sure.
				 */
				if (magic == ~sp->lcp.magic) {
					if (debug)
						addlog("magic glitch ");
					sp->lcp.magic += time.tv_sec + time.tv_usec;
				} else {
					sp->lcp.magic = magic;
					if (debug)
						addlog("%d ");
				}
			}
			break;
		case LCP_OPT_MRU:
			/*
			 * Peer wants to advise us to negotiate an MRU.
			 * Agree on it if it's reasonable, or use
			 * default otherwise.
			 */
			if (len >= 4 && p[1] == 4) {
				u_int mru = p[2] * 256 + p[3];
				if (debug)
					addlog("%d ", mru);
				if (mru < PP_MTU || mru > PP_MAX_MRU)
					mru = PP_MTU;
				sp->lcp.mru = mru;
				sp->lcp.opts |= (1 << LCP_OPT_MRU);
			}
			break;
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

static void
sppp_lcp_tlu(struct sppp *sp)
{
	STDDCL;
	int i;
	u_long mask;

	/* XXX ? */
	if (! (ifp->if_flags & IFF_UP) &&
	    (ifp->if_flags & IFF_RUNNING)) {
		/* Coming out of loopback mode. */
		if_up(ifp);
		printf ("%s%d: up\n", ifp->if_name, ifp->if_unit);
	}

	for (i = 0; i < IDX_COUNT; i++)
		if ((cps[i])->flags & CP_QUAL)
			(cps[i])->Open(sp);

	if (/* require authentication XXX */ 0)
		sp->pp_phase = PHASE_AUTHENTICATE;
	else
		sp->pp_phase = PHASE_NETWORK;

	log(LOG_INFO, "%s%d: phase %s\n", ifp->if_name, ifp->if_unit,
	    sppp_phase_name(sp->pp_phase));

	if (sp->pp_phase == PHASE_AUTHENTICATE) {
		for (i = 0; i < IDX_COUNT; i++)
			if ((cps[i])->flags & CP_AUTH)
				(cps[i])->Open(sp);
	} else {
		/* Notify all NCPs. */
		for (i = 0; i < IDX_COUNT; i++)
			if ((cps[i])->flags & CP_NCP)
				(cps[i])->Open(sp);
	}

	/* Send Up events to all started protos. */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if (sp->lcp.protos & mask && ((cps[i])->flags & CP_LCP) == 0)
			(cps[i])->Up(sp);

	if (sp->pp_phase == PHASE_NETWORK)
		/* if no NCP is starting, close down */
		sppp_lcp_check(sp);
}

static void
sppp_lcp_tld(struct sppp *sp)
{
	STDDCL;
	int i;
	u_long mask;

	sp->pp_phase = PHASE_TERMINATE;

	log(LOG_INFO, "%s%d: phase %s\n", ifp->if_name, ifp->if_unit,
	    sppp_phase_name(sp->pp_phase));

	/*
	 * Take upper layers down.  We send the Down event first and
	 * the Close second to prevent the upper layers from sending
	 * ``a flurry of terminate-request packets'', as the RFC
	 * describes it.
	 */
	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if (sp->lcp.protos & mask && ((cps[i])->flags & CP_LCP) == 0) {
			(cps[i])->Down(sp);
			(cps[i])->Close(sp);
		}
}

static void
sppp_lcp_tls(struct sppp *sp)
{
	STDDCL;

	sp->pp_phase = PHASE_ESTABLISH;

	log(LOG_INFO, "%s%d: phase %s\n", ifp->if_name, ifp->if_unit,
	    sppp_phase_name(sp->pp_phase));

	/* Notify lower layer if desired. */
	if (sp->pp_tls)
		(sp->pp_tls)(sp);
}

static void
sppp_lcp_tlf(struct sppp *sp)
{
	STDDCL;

	sp->pp_phase = PHASE_DEAD;
	log(LOG_INFO, "%s%d: phase %s\n", ifp->if_name, ifp->if_unit,
	    sppp_phase_name(sp->pp_phase));

	/* Notify lower layer if desired. */
	if (sp->pp_tlf)
		(sp->pp_tlf)(sp);
}

static void
sppp_lcp_scr(struct sppp *sp)
{
	char opt[6 /* magicnum */ + 4 /* mru */];
	int i = 0;

	if (sp->lcp.opts & (1 << LCP_OPT_MAGIC)) {
		if (! sp->lcp.magic)
			sp->lcp.magic = time.tv_sec + time.tv_usec;
		opt[i++] = LCP_OPT_MAGIC;
		opt[i++] = 6;
		opt[i++] = sp->lcp.magic >> 24;
		opt[i++] = sp->lcp.magic >> 16;
		opt[i++] = sp->lcp.magic >> 8;
		opt[i++] = sp->lcp.magic;
	}

	if (sp->lcp.opts & (1 << LCP_OPT_MRU)) {
		opt[i++] = LCP_OPT_MRU;
		opt[i++] = 4;
		opt[i++] = sp->lcp.mru >> 8;
		opt[i++] = sp->lcp.mru;
	}

	sp->confid[IDX_LCP] = ++sp->pp_seq;
	sppp_cp_send (sp, PPP_LCP, CONF_REQ, sp->confid[IDX_LCP], i, &opt);
}

/*
 * Re-check the open NCPs and see if we should terminate the link.
 * Called by the NCPs during their tlf action handling.
 */
static void
sppp_lcp_check(struct sppp *sp)
{
	int i, mask;

	for (i = 0, mask = 1; i < IDX_COUNT; i++, mask <<= 1)
		if (sp->lcp.protos & mask && (cps[i])->flags & CP_NCP)
			return;
	lcp.Close(sp);
}
/*
 *--------------------------------------------------------------------------*
 *                                                                          *
 *                        The IPCP implementation.                          *
 *                                                                          *
 *--------------------------------------------------------------------------*
 */

static void
sppp_ipcp_init(struct sppp *sp)
{
	sp->ipcp.opts = 0;
	sp->ipcp.flags = 0;
	sp->state[IDX_IPCP] = STATE_INITIAL;
	sp->fail_counter[IDX_IPCP] = 0;
}

static void
sppp_ipcp_up(struct sppp *sp)
{
	sppp_up_event(&ipcp, sp);
}

static void
sppp_ipcp_down(struct sppp *sp)
{
	sppp_down_event(&ipcp, sp);
}

static void
sppp_ipcp_open(struct sppp *sp)
{
	STDDCL;
	u_long myaddr, hisaddr;

	sppp_get_ip_addrs(sp, &myaddr, &hisaddr);
	/*
	 * If we don't have his address, this probably means our
	 * interface doesn't want to talk IP at all.  (This could
	 * be the case if somebody wants to speak only IPX, for
	 * example.)  Don't open IPCP in this case.
	 */
	if (hisaddr == 0L) {
		/* XXX this message should go away */
		if (debug)
			log(LOG_DEBUG, "%s%d: ipcp_open(): no IP interface\n",
			    ifp->if_name, ifp->if_unit);
		return;
	}

	if (myaddr == 0L) {
		/*
		 * I don't have an assigned address, so i need to
		 * negotiate my address.
		 */
		sp->ipcp.flags |= IPCP_MYADDR_DYN;
		sp->ipcp.opts |= (1 << IPCP_OPT_ADDRESS);
	}
	sppp_open_event(&ipcp, sp);
}

static void
sppp_ipcp_close(struct sppp *sp)
{
	sppp_close_event(&ipcp, sp);
	if (sp->ipcp.flags & IPCP_MYADDR_DYN)
		/*
		 * My address was dynamic, clear it again.
		 */
		sppp_set_ip_addr(sp, 0L);
}

static void
sppp_ipcp_TO(void *cookie)
{
	sppp_to_event(&ipcp, (struct sppp *)cookie);
}

/*
 * Analyze a configure request.  Return true if it was agreeable, and
 * caused action sca, false if it has been rejected or nak'ed, and
 * caused action scn.  (The return value is used to make the state
 * transition decision in the state automaton.)
 */
static int
sppp_ipcp_RCR(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *r, *p;
	struct ifnet *ifp = &sp->pp_if;
	int rlen, origlen, debug = ifp->if_flags & IFF_DEBUG;
	u_long hisaddr, desiredaddr;

	len -= 4;
	origlen = len;
	/*
	 * Make sure to allocate a buf that can at least hold a
	 * conf-nak with an `address' option.  We might need it below.
	 */
	buf = r = malloc ((len < 6? 6: len), M_TEMP, M_NOWAIT);
	if (! buf)
		return (0);

	/* pass 1: see if we can recognize them */
	if (debug)
		log(LOG_DEBUG, "%s%d: ipcp parse opts: ",
		    ifp->if_name, ifp->if_unit);
	p = (void*) (h+1);
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s ", sppp_ipcp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPCP_OPT_COMPRESSION:
			if (len >= 6 && p[1] >= 6) {
				/* correctly formed compress option */
				continue;
			}
			if (debug)
				addlog("[invalid] ");
			break;
#endif
		case IPCP_OPT_ADDRESS:
			if (len >= 6 && p[1] == 6) {
				/* correctly formed address option */
				continue;
			}
			if (debug)
				addlog("[invalid] ");
			break;
		default:
			/* Others not supported. */
			if (debug)
				addlog("[rej] ");
			break;
		}
		/* Add the option to rejected list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}
	if (rlen) {
		if (debug)
			addlog(" send conf-rej\n");
		sppp_cp_send (sp, PPP_IPCP, CONF_REJ, h->ident, rlen, buf);
		return 0;
	} else if (debug)
		addlog("\n");

	/* pass 2: parse option values */
	sppp_get_ip_addrs(sp, 0, &hisaddr);
	if (debug)
		addlog("%s%d: ipcp parse opt values: ", ifp->if_name, ifp->if_unit);
	p = (void*) (h+1);
	len = origlen;
	for (rlen=0; len>1 && p[1]; len-=p[1], p+=p[1]) {
		if (debug)
			addlog(" %s ", sppp_ipcp_opt_name(*p));
		switch (*p) {
#ifdef notyet
		case IPCP_OPT_COMPRESSION:
			continue;
#endif
		case IPCP_OPT_ADDRESS:
			desiredaddr = p[2] << 24 | p[3] << 16 |
				p[4] << 8 | p[5];
			if (desiredaddr == hisaddr) {
				/*
				 * Peer's address is same as our value,
				 * this is agreeable.  Gonna conf-ack
				 * it.
				 */
				if (debug)
					addlog("0x%x [ack] ", hisaddr);
				/* record that we've seen it already */
				sp->ipcp.flags |= IPCP_HISADDR_SEEN;
				continue;
			}
			/*
			 * The address wasn't agreeable.  This is either
			 * he sent us 0.0.0.0, asking to assign him an
			 * address, or he send us another address not
			 * matching our value.  Either case, we gonna
			 * conf-nak it with our value.
			 */
			if (debug) {
				if (desiredaddr == 0)
					addlog("[addr requested] ");
				else
					addlog("0x%x [not agreed] ",
					       desiredaddr);

				p[2] = hisaddr >> 24;
				p[3] = hisaddr >> 16;
				p[4] = hisaddr >> 8;
				p[5] = hisaddr;
			}
			break;
		}
		/* Add the option to nak'ed list. */
		bcopy (p, r, p[1]);
		r += p[1];
		rlen += p[1];
	}

	/*
	 * If we are about to conf-ack the request, but haven't seen
	 * his address so far, gonna conf-nak it instead, with the
	 * `address' option present and our idea of his address being
	 * filled in there, to request negotiation of both addresses.
	 *
	 * XXX This can result in an endless req - nak loop if peer
	 * doesn't want to send us his address.  Q: What should we do
	 * about it?  XXX  A: implement the max-failure counter.
	 */
	if (rlen == 0 && !(sp->ipcp.flags & IPCP_HISADDR_SEEN)) {
		buf[0] = IPCP_OPT_ADDRESS;
		buf[1] = 6;
		buf[2] = hisaddr >> 24;
		buf[3] = hisaddr >> 16;
		buf[4] = hisaddr >> 8;
		buf[5] = hisaddr;
		rlen = 6;
		if (debug)
			addlog("still need hisaddr ");
	}

	if (rlen) {
		if (debug)
			addlog(" send conf-nak\n");
		sppp_cp_send (sp, PPP_IPCP, CONF_NAK, h->ident, rlen, buf);
	} else {
		if (debug)
			addlog(" send conf-ack\n");
		sppp_cp_send (sp, PPP_IPCP, CONF_ACK,
			      h->ident, origlen, h+1);
	}

	free (buf, M_TEMP);
	return (rlen == 0);
}

/*
 * Analyze the IPCP Configure-Reject option list, and adjust our
 * negotiation.
 */
static void
sppp_ipcp_RCN_rej(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s%d: ipcp rej opts: ",
		    ifp->if_name, ifp->if_unit);

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s ", sppp_ipcp_opt_name(*p));
		switch (*p) {
		case IPCP_OPT_ADDRESS:
			/*
			 * Peer doesn't grok address option.  This is
			 * bad.  XXX  Should we better give up here?
			 */
			sp->ipcp.opts &= ~(1 << IPCP_OPT_ADDRESS);
			break;
#ifdef notyet
		case IPCP_OPT_COMPRESS:
			sp->ipcp.opts &= ~(1 << IPCP_OPT_COMPRESS);
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

/*
 * Analyze the IPCP Configure-NAK option list, and adjust our
 * negotiation.
 */
static void
sppp_ipcp_RCN_nak(struct sppp *sp, struct lcp_header *h, int len)
{
	u_char *buf, *p;
	struct ifnet *ifp = &sp->pp_if;
	int debug = ifp->if_flags & IFF_DEBUG;
	u_long wantaddr;

	len -= 4;
	buf = malloc (len, M_TEMP, M_NOWAIT);
	if (!buf)
		return;

	if (debug)
		log(LOG_DEBUG, "%s%d: ipcp nak opts: ",
		    ifp->if_name, ifp->if_unit);

	p = (void*) (h+1);
	for (; len > 1 && p[1]; len -= p[1], p += p[1]) {
		if (debug)
			addlog(" %s ", sppp_ipcp_opt_name(*p));
		switch (*p) {
		case IPCP_OPT_ADDRESS:
			/*
			 * Peer doesn't like our local IP address.  See
			 * if we can do something for him.  We'll drop
			 * him our address then.
			 */
			if (len >= 6 && p[1] == 6) {
				wantaddr = p[2] << 24 | p[3] << 16 |
					p[4] << 8 | p[5];
				sp->ipcp.opts |= (1 << IPCP_OPT_ADDRESS);
				if (debug)
					addlog("[wantaddr 0x%x] ", wantaddr);
				/*
				 * When doing dynamic address assignment,
				 * we accept his offer.  Otherwise, we
				 * ignore it and thus continue to negotiate
				 * our already existing value.
				 */
				if (sp->ipcp.flags & IPCP_MYADDR_DYN) {
					sppp_set_ip_addr(sp, wantaddr);
					if (debug)
						addlog("[agree] ");
				}
			}
			break;
#ifdef notyet
		case IPCP_OPT_COMPRESS:
			/*
			 * Peer wants different compression parameters.
			 */
			break;
#endif
		}
	}
	if (debug)
		addlog("\n");
	free (buf, M_TEMP);
	return;
}

static void
sppp_ipcp_tlu(struct sppp *sp)
{
}

static void
sppp_ipcp_tld(struct sppp *sp)
{
}

static void
sppp_ipcp_tls(struct sppp *sp)
{
	/* indicate to LCP that it must stay alive */
	sp->lcp.protos |= (1 << IDX_IPCP);
}

static void
sppp_ipcp_tlf(struct sppp *sp)
{
	/* we no longer need LCP */
	sp->lcp.protos &= ~(1 << IDX_IPCP);
	sppp_lcp_check(sp);
}

static void
sppp_ipcp_scr(struct sppp *sp)
{
	char opt[6 /* compression */ + 6 /* address */];
	u_long ouraddr;
	int i = 0;

#ifdef notyet
	if (sp->ipcp.opts & (1 << IPCP_OPT_COMPRESSION)) {
		opt[i++] = IPCP_OPT_COMPRESSION;
		opt[i++] = 6;
		opt[i++] = 0;	/* VJ header compression */
		opt[i++] = 0x2d; /* VJ header compression */
		opt[i++] = max_slot_id;
		opt[i++] = comp_slot_id;
	}
#endif

	if (sp->ipcp.opts & (1 << IPCP_OPT_ADDRESS)) {
		sppp_get_ip_addrs(sp, &ouraddr, 0);
		opt[i++] = IPCP_OPT_ADDRESS;
		opt[i++] = 6;
		opt[i++] = ouraddr >> 24;
		opt[i++] = ouraddr >> 16;
		opt[i++] = ouraddr >> 8;
		opt[i++] = ouraddr;
	}

	sp->confid[IDX_IPCP] = ++sp->pp_seq;
	sppp_cp_send(sp, PPP_IPCP, CONF_REQ, sp->confid[IDX_IPCP], i, &opt);
}


/*
 * Random miscellaneous functions.
 */

/*
 * Flush interface queue.
 */
static void
sppp_qflush(struct ifqueue *ifq)
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
 * Send keepalive packets, every 10 seconds.
 */
static void
sppp_keepalive(void *dummy)
{
	struct sppp *sp;
	int s;

	s = splimp();
	for (sp=spppq; sp; sp=sp->pp_next) {
		struct ifnet *ifp = &sp->pp_if;

		/* Keepalive mode disabled or channel down? */
		if (! (sp->pp_flags & PP_KEEPALIVE) ||
		    ! (ifp->if_flags & IFF_RUNNING))
			continue;

		/* No keepalive in PPP mode if LCP not opened yet. */
		if (! (sp->pp_flags & PP_CISCO) &&
		    sp->pp_phase < PHASE_AUTHENTICATE)
			continue;

		if (sp->pp_alivecnt == MAXALIVECNT) {
			/* No keepalive packets got.  Stop the interface. */
			printf ("%s%d: down\n", ifp->if_name, ifp->if_unit);
			if_down (ifp);
			sppp_qflush (&sp->pp_fastq);
			if (! (sp->pp_flags & PP_CISCO)) {
				/* XXX */
				/* Shut down the PPP link. */
				lcp.Down(sp);
				/* Initiate negotiation. XXX */
				lcp.Up(sp);
			}
		}
		if (sp->pp_alivecnt <= MAXALIVECNT)
			++sp->pp_alivecnt;
		if (sp->pp_flags & PP_CISCO)
			sppp_cisco_send (sp, CISCO_KEEPALIVE_REQ, ++sp->pp_seq,
				sp->pp_rseq);
		else if (sp->pp_phase >= PHASE_AUTHENTICATE) {
			long nmagic = htonl (sp->lcp.magic);
			sp->lcp.echoid = ++sp->pp_seq;
			sppp_cp_send (sp, PPP_LCP, ECHO_REQ,
				sp->lcp.echoid, 4, &nmagic);
		}
	}
	splx(s);
	timeout(sppp_keepalive, 0, hz * 10);
}

/*
 * Get both IP addresses.
 */
static void
sppp_get_ip_addrs(struct sppp *sp, u_long *src, u_long *dst)
{
	struct ifnet *ifp = &sp->pp_if;
	struct ifaddr *ifa;
	struct sockaddr_in *si;
	u_long ssrc, ddst;

	ssrc = ddst = 0L;
	/*
	 * Pick the first AF_INET address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */
	for (ifa = ifp->if_addrhead.tqh_first, si = 0;
	     ifa; 
	     ifa = ifa->ifa_link.tqe_next)
		if (ifa->ifa_addr->sa_family == AF_INET) {
			si = (struct sockaddr_in *)ifa->ifa_addr;
			if (si)
				break;
		}
	if (ifa) {
		if (si && si->sin_addr.s_addr)
			ssrc = si->sin_addr.s_addr;

		si = (struct sockaddr_in *)ifa->ifa_dstaddr;
		if (si && si->sin_addr.s_addr)
			ddst = si->sin_addr.s_addr;
	}

	if (dst) *dst = ntohl(ddst);
	if (src) *src = ntohl(ssrc);
}

/*
 * Set my IP address.  Must be called at splimp.
 */
static void
sppp_set_ip_addr(struct sppp *sp, u_long src)
{
	struct ifnet *ifp = &sp->pp_if;
	struct ifaddr *ifa;
	struct sockaddr_in *si;
	u_long ssrc, ddst;

	/*
	 * Pick the first AF_INET address from the list,
	 * aliases don't make any sense on a p2p link anyway.
	 */
	for (ifa = ifp->if_addrhead.tqh_first, si = 0;
	     ifa; 
	     ifa = ifa->ifa_link.tqe_next)
		if (ifa->ifa_addr->sa_family == AF_INET) {
			si = (struct sockaddr_in *)ifa->ifa_addr;
			if (si)
				break;
		}
	if (ifa && si)
		si->sin_addr.s_addr = htonl(src);
}

static const char *
sppp_cp_type_name(u_char type)
{
	static char buf [12];
	switch (type) {
	case CONF_REQ:   return ("conf-req");
	case CONF_ACK:   return ("conf-ack");
	case CONF_NAK:   return ("conf-nak");
	case CONF_REJ:   return ("conf-rej");
	case TERM_REQ:   return ("term-req");
	case TERM_ACK:   return ("term-ack");
	case CODE_REJ:   return ("code-rej");
	case PROTO_REJ:  return ("proto-rej");
	case ECHO_REQ:   return ("echo-req");
	case ECHO_REPLY: return ("echo-reply");
	case DISC_REQ:   return ("discard-req");
	}
	sprintf (buf, "0x%x", type);
	return (buf);
}

static const char *
sppp_lcp_opt_name(u_char opt)
{
	static char buf [12];
	switch (opt) {
	case LCP_OPT_MRU:		return ("mru");
	case LCP_OPT_ASYNC_MAP:		return ("async-map");
	case LCP_OPT_AUTH_PROTO:	return ("auth-proto");
	case LCP_OPT_QUAL_PROTO:	return ("qual-proto");
	case LCP_OPT_MAGIC:		return ("magic");
	case LCP_OPT_PROTO_COMP:	return ("proto-comp");
	case LCP_OPT_ADDR_COMP:		return ("addr-comp");
	}
	sprintf (buf, "0x%x", opt);
	return (buf);
}

static const char *
sppp_ipcp_opt_name(u_char opt)
{
	static char buf [12];
	switch (opt) {
	case IPCP_OPT_ADDRESSES:	return ("addresses");
	case IPCP_OPT_COMPRESSION:	return ("compression");
	case IPCP_OPT_ADDRESS:		return ("address");
	}
	sprintf (buf, "0x%x", opt);
	return (buf);
}

static const char *
sppp_state_name(int state)
{
	switch (state) {
	case STATE_INITIAL:	return "initial";
	case STATE_STARTING:	return "starting";
	case STATE_CLOSED:	return "closed";
	case STATE_STOPPED:	return "stopped";
	case STATE_CLOSING:	return "closing";
	case STATE_STOPPING:	return "stopping";
	case STATE_REQ_SENT:	return "req-sent";
	case STATE_ACK_RCVD:	return "ack-rcvd";
	case STATE_ACK_SENT:	return "ack-sent";
	case STATE_OPENED:	return "opened";
	}
	return "illegal";
}

static const char *
sppp_phase_name(enum ppp_phase phase)
{
	switch (phase) {
	case PHASE_DEAD:	return "dead";
	case PHASE_ESTABLISH:	return "establish";
	case PHASE_TERMINATE:	return "terminate";
	case PHASE_AUTHENTICATE: return "authenticate";
	case PHASE_NETWORK:	return "network";
	}
	return "illegal";
}

static const char *
sppp_proto_name(u_short proto)
{
	static char buf[12];
	switch (proto) {
	case PPP_LCP:	return "lcp";
	case PPP_IPCP:	return "ipcp";
	}
	sprintf(buf, "0x%x", (unsigned)proto);
	return buf;
}

static void
sppp_print_bytes(u_char *p, u_short len)
{
	addlog(" %x", *p++);
	while (--len > 0)
		addlog("-%x", *p++);
}

/*
 * This file is large.  Tell emacs to highlight it nevertheless.
 *
 * Local Variables:
 * hilit-auto-highlight-maxout: 100000
 * End:
 */
