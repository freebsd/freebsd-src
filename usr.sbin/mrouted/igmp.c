/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * igmp.c,v 3.8.4.19 1998/01/06 01:57:43 fenner Exp
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/mrouted/igmp.c,v 1.16 1999/08/28 01:17:04 peter Exp $";
#endif /* not lint */

#include "defs.h"

/*
 * Exported variables.
 */
char		*recv_buf; 		     /* input packet buffer         */
char		*send_buf; 		     /* output packet buffer        */
int		igmp_socket;		     /* socket for all network I/O  */
u_int32		allhosts_group;		     /* All hosts addr in net order */
u_int32		allrtrs_group;		     /* All-Routers "  in net order */
u_int32		dvmrp_group;		     /* DVMRP grp addr in net order */
u_int32		dvmrp_genid;		     /* IGMP generation id          */

/*
 * Local function definitions.
 */
/* u_char promoted to u_int */
static int	igmp_log_level __P((u_int type, u_int code));

/*
 * Open and initialize the igmp socket, and fill in the non-changing
 * IP header fields in the output packet buffer.
 */
void
init_igmp()
{
    struct ip *ip;

    recv_buf = malloc(RECV_BUF_SIZE);
    send_buf = malloc(RECV_BUF_SIZE);

    if ((igmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0) 
	log(LOG_ERR, errno, "IGMP socket");

    k_hdr_include(TRUE);	/* include IP header when sending */
    k_set_rcvbuf(256*1024,48*1024);	/* lots of input buffering        */
    k_set_ttl(1);		/* restrict multicasts to one hop */
    k_set_loop(FALSE);		/* disable multicast loopback     */

    ip         = (struct ip *)send_buf;
    bzero(ip, sizeof(struct ip));
    /*
     * Fields zeroed that aren't filled in later:
     * - IP ID (let the kernel fill it in)
     * - Offset (we don't send fragments)
     * - Checksum (let the kernel fill it in)
     */
    ip->ip_v   = IPVERSION;
    ip->ip_hl  = sizeof(struct ip) >> 2;
    ip->ip_tos = 0xc0;		/* Internet Control */
    ip->ip_ttl = MAXTTL;	/* applies to unicasts only */
    ip->ip_p   = IPPROTO_IGMP;

    allhosts_group = htonl(INADDR_ALLHOSTS_GROUP);
    dvmrp_group    = htonl(INADDR_DVMRP_GROUP);
    allrtrs_group  = htonl(INADDR_ALLRTRS_GROUP);
}

#define PIM_QUERY        0
#define PIM_REGISTER     1
#define PIM_REGISTER_STOP 	2
#define PIM_JOIN_PRUNE   3
#define PIM_RP_REACHABLE 4
#define PIM_ASSERT       5
#define PIM_GRAFT        6
#define PIM_GRAFT_ACK    7

char *
igmp_packet_kind(type, code)
     u_int type, code;
{
    static char unknown[20];

    switch (type) {
	case IGMP_MEMBERSHIP_QUERY:		return "membership query  ";
	case IGMP_V1_MEMBERSHIP_REPORT:		return "V1 member report  ";
	case IGMP_V2_MEMBERSHIP_REPORT:		return "V2 member report  ";
	case IGMP_V2_LEAVE_GROUP:		return "leave message     ";
	case IGMP_DVMRP:
	  switch (code) {
	    case DVMRP_PROBE:			return "neighbor probe    ";
	    case DVMRP_REPORT:			return "route report      ";
	    case DVMRP_ASK_NEIGHBORS:		return "neighbor request  ";
	    case DVMRP_NEIGHBORS:		return "neighbor list     ";
	    case DVMRP_ASK_NEIGHBORS2:		return "neighbor request 2";
	    case DVMRP_NEIGHBORS2:		return "neighbor list 2   ";
	    case DVMRP_PRUNE:			return "prune message     ";
	    case DVMRP_GRAFT:			return "graft message     ";
	    case DVMRP_GRAFT_ACK:		return "graft message ack ";
	    case DVMRP_INFO_REQUEST:		return "info request      ";
	    case DVMRP_INFO_REPLY:		return "info reply        ";
	    default:
		    sprintf(unknown, "unknown DVMRP %3d ", code);
		    return unknown;
	  }
 	case IGMP_PIM:
 	  switch (code) {
 	    case PIM_QUERY:			return "PIM Router-Query  ";
 	    case PIM_REGISTER:			return "PIM Register      ";
 	    case PIM_REGISTER_STOP:		return "PIM Register-Stop ";
 	    case PIM_JOIN_PRUNE:		return "PIM Join/Prune    ";
 	    case PIM_RP_REACHABLE:		return "PIM RP-Reachable  ";
 	    case PIM_ASSERT:			return "PIM Assert        ";
 	    case PIM_GRAFT:			return "PIM Graft         ";
 	    case PIM_GRAFT_ACK:			return "PIM Graft-Ack     ";
 	    default:
 		    sprintf(unknown, "unknown PIM msg%3d", code);
		    return unknown;
 	  }
	case IGMP_MTRACE:			return "IGMP trace query  ";
	case IGMP_MTRACE_RESP:			return "IGMP trace reply  ";
	default:
		sprintf(unknown, "unk: 0x%02x/0x%02x    ", type, code);
		return unknown;
    }
}

int
igmp_debug_kind(type, code)
     u_int type, code;
{
    switch (type) {
	case IGMP_MEMBERSHIP_QUERY:		return DEBUG_IGMP;
	case IGMP_V1_MEMBERSHIP_REPORT:		return DEBUG_IGMP;
	case IGMP_V2_MEMBERSHIP_REPORT:		return DEBUG_IGMP;
	case IGMP_V2_LEAVE_GROUP:		return DEBUG_IGMP;
	case IGMP_DVMRP:
	  switch (code) {
	    case DVMRP_PROBE:			return DEBUG_PEER;
	    case DVMRP_REPORT:			return DEBUG_ROUTE;
	    case DVMRP_ASK_NEIGHBORS:		return 0;
	    case DVMRP_NEIGHBORS:		return 0;
	    case DVMRP_ASK_NEIGHBORS2:		return 0;
	    case DVMRP_NEIGHBORS2:		return 0;
	    case DVMRP_PRUNE:			return DEBUG_PRUNE;
	    case DVMRP_GRAFT:			return DEBUG_PRUNE;
	    case DVMRP_GRAFT_ACK:		return DEBUG_PRUNE;
	    case DVMRP_INFO_REQUEST:		return 0;
	    case DVMRP_INFO_REPLY:		return 0;
	    default:				return 0;
	  }
	case IGMP_PIM:
	  switch (code) {
	    case PIM_QUERY:			return 0;
	    case PIM_REGISTER:			return 0;
	    case PIM_REGISTER_STOP:		return 0;
	    case PIM_JOIN_PRUNE:		return 0;
	    case PIM_RP_REACHABLE:		return 0;
	    case PIM_ASSERT:			return 0;
	    case PIM_GRAFT:			return 0;
	    case PIM_GRAFT_ACK:			return 0;
	    default:				return 0;
	  }
	case IGMP_MTRACE:			return DEBUG_TRACE;
	case IGMP_MTRACE_RESP:			return DEBUG_TRACE;
	default:				return DEBUG_IGMP;
    }
}

/*
 * Process a newly received IGMP packet that is sitting in the input
 * packet buffer.
 */
void
accept_igmp(recvlen)
    int recvlen;
{
    register u_int32 src, dst, group;
    struct ip *ip;
    struct igmp *igmp;
    int ipdatalen, iphdrlen, igmpdatalen;

    if (recvlen < sizeof(struct ip)) {
	log(LOG_WARNING, 0,
	    "received packet too short (%u bytes) for IP header", recvlen);
	return;
    }

    ip        = (struct ip *)recv_buf;
    src       = ip->ip_src.s_addr;
    dst       = ip->ip_dst.s_addr;

    /* 
     * this is most likely a message from the kernel indicating that
     * a new src grp pair message has arrived and so, it would be 
     * necessary to install a route into the kernel for this.
     */
    if (ip->ip_p == 0) {
	if (src == 0 || dst == 0)
	    log(LOG_WARNING, 0, "kernel request not accurate");
	else
	    add_table_entry(src, dst);
	return;
    }

    iphdrlen  = ip->ip_hl << 2;
#ifdef RAW_INPUT_IS_RAW
    ipdatalen = ntohs(ip->ip_len) - iphdrlen;
#else
    ipdatalen = ip->ip_len;
#endif
    if (iphdrlen + ipdatalen != recvlen) {
	log(LOG_WARNING, 0,
	    "received packet from %s shorter (%u bytes) than hdr+data length (%u+%u)",
	    inet_fmt(src, s1), recvlen, iphdrlen, ipdatalen);
	return;
    }

    igmp        = (struct igmp *)(recv_buf + iphdrlen);
    group       = igmp->igmp_group.s_addr;
    igmpdatalen = ipdatalen - IGMP_MINLEN;
    if (igmpdatalen < 0) {
	log(LOG_WARNING, 0,
	    "received IP data field too short (%u bytes) for IGMP, from %s",
	    ipdatalen, inet_fmt(src, s1));
	return;
    }

    IF_DEBUG(DEBUG_PKT|igmp_debug_kind(igmp->igmp_type, igmp->igmp_code))
    log(LOG_DEBUG, 0, "RECV %s from %-15s to %s",
	igmp_packet_kind(igmp->igmp_type, igmp->igmp_code),
	inet_fmt(src, s1), inet_fmt(dst, s2));

    switch (igmp->igmp_type) {

	case IGMP_MEMBERSHIP_QUERY:
	    accept_membership_query(src, dst, group, igmp->igmp_code);
	    return;

	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:
	    accept_group_report(src, dst, group, igmp->igmp_type);
	    return;
	    
	case IGMP_V2_LEAVE_GROUP:
	    accept_leave_message(src, dst, group);
	    return;

	case IGMP_DVMRP:
	    group = ntohl(group);

	    switch (igmp->igmp_code) {
		case DVMRP_PROBE:
		    accept_probe(src, dst,
				 (char *)(igmp+1), igmpdatalen, group);
		    return;

		case DVMRP_REPORT:
 		    accept_report(src, dst,
				  (char *)(igmp+1), igmpdatalen, group);
		    return;

		case DVMRP_ASK_NEIGHBORS:
		    accept_neighbor_request(src, dst);
		    return;

		case DVMRP_ASK_NEIGHBORS2:
		    accept_neighbor_request2(src, dst);
		    return;

		case DVMRP_NEIGHBORS:
		    accept_neighbors(src, dst, (u_char *)(igmp+1), igmpdatalen,
					     group);
		    return;

		case DVMRP_NEIGHBORS2:
		    accept_neighbors2(src, dst, (u_char *)(igmp+1), igmpdatalen,
					     group);
		    return;

		case DVMRP_PRUNE:
		    accept_prune(src, dst, (char *)(igmp+1), igmpdatalen);
		    return;

		case DVMRP_GRAFT:
		    accept_graft(src, dst, (char *)(igmp+1), igmpdatalen);
		    return;

		case DVMRP_GRAFT_ACK:
		    accept_g_ack(src, dst, (char *)(igmp+1), igmpdatalen);
		    return;

		case DVMRP_INFO_REQUEST:
		    accept_info_request(src, dst, (char *)(igmp+1),
				igmpdatalen);
		    return;

		case DVMRP_INFO_REPLY:
		    accept_info_reply(src, dst, (char *)(igmp+1), igmpdatalen);
		    return;

		default:
		    log(LOG_INFO, 0,
		     "ignoring unknown DVMRP message code %u from %s to %s",
		     igmp->igmp_code, inet_fmt(src, s1),
		     inet_fmt(dst, s2));
		    return;
	    }

 	case IGMP_PIM:
 	    return;

	case IGMP_MTRACE_RESP:
	    return;

	case IGMP_MTRACE:
	    accept_mtrace(src, dst, group, (char *)(igmp+1),
		   igmp->igmp_code, igmpdatalen);
	    return;

	default:
	    log(LOG_INFO, 0,
		"ignoring unknown IGMP message type %x from %s to %s",
		igmp->igmp_type, inet_fmt(src, s1),
		inet_fmt(dst, s2));
	    return;
    }
}

/*
 * Some IGMP messages are more important than others.  This routine
 * determines the logging level at which to log a send error (often
 * "No route to host").  This is important when there is asymmetric
 * reachability and someone is trying to, i.e., mrinfo me periodically.
 */
static int
igmp_log_level(type, code)
    u_int type, code;
{
    switch (type) {
	case IGMP_MTRACE_RESP:
	    return LOG_INFO;

	case IGMP_DVMRP:
	  switch (code) {
	    case DVMRP_NEIGHBORS:
	    case DVMRP_NEIGHBORS2:
		return LOG_INFO;
	  }
    }
    return LOG_WARNING;
}

/*
 * Construct an IGMP message in the output packet buffer.  The caller may
 * have already placed data in that buffer, of length 'datalen'.
 */
void
build_igmp(src, dst, type, code, group, datalen)
    u_int32 src, dst;
    int type, code;
    u_int32 group;
    int datalen;
{
    struct ip *ip;
    struct igmp *igmp;
    extern int curttl;

    ip                      = (struct ip *)send_buf;
    ip->ip_src.s_addr       = src;
    ip->ip_dst.s_addr       = dst;
    ip->ip_len              = MIN_IP_HEADER_LEN + IGMP_MINLEN + datalen;
#ifdef RAW_OUTPUT_IS_RAW
    ip->ip_len		    = htons(ip->ip_len);
#endif
    if (IN_MULTICAST(ntohl(dst))) {
	ip->ip_ttl = curttl;
    } else {
	ip->ip_ttl = MAXTTL;
    }

    igmp                    = (struct igmp *)(send_buf + MIN_IP_HEADER_LEN);
    igmp->igmp_type         = type;
    igmp->igmp_code         = code;
    igmp->igmp_group.s_addr = group;
    igmp->igmp_cksum        = 0;
    igmp->igmp_cksum        = inet_cksum((u_short *)igmp,
					 IGMP_MINLEN + datalen);
}

/* 
 * Call build_igmp() to build an IGMP message in the output packet buffer.
 * Then send the message from the interface with IP address 'src' to
 * destination 'dst'.
 */
void
send_igmp(src, dst, type, code, group, datalen)
    u_int32 src, dst;
    int type, code;
    u_int32 group;
    int datalen;
{
    struct sockaddr_in sdst;
    int setloop = 0;

    build_igmp(src, dst, type, code, group, datalen);

    if (IN_MULTICAST(ntohl(dst))) {
	k_set_if(src);
	if (type != IGMP_DVMRP || dst == allhosts_group) {
	    setloop = 1;
	    k_set_loop(TRUE);
	}
    }

    bzero(&sdst, sizeof(sdst));
    sdst.sin_family = AF_INET;
#ifdef HAVE_SA_LEN
    sdst.sin_len = sizeof(sdst);
#endif
    sdst.sin_addr.s_addr = dst;
    if (sendto(igmp_socket, send_buf,
			MIN_IP_HEADER_LEN + IGMP_MINLEN + datalen, 0,
			(struct sockaddr *)&sdst, sizeof(sdst)) < 0) {
	if (errno == ENETDOWN)
	    check_vif_state();
	else
	    log(igmp_log_level(type, code), errno,
		"sendto to %s on %s",
		inet_fmt(dst, s1), inet_fmt(src, s2));
    }

    if (setloop)
	    k_set_loop(FALSE);

    IF_DEBUG(DEBUG_PKT|igmp_debug_kind(type, code))
    log(LOG_DEBUG, 0, "SENT %s from %-15s to %s",
	igmp_packet_kind(type, code), src == INADDR_ANY ? "INADDR_ANY" :
				 inet_fmt(src, s1), inet_fmt(dst, s2));
}
