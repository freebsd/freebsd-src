/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: igmp.c,v 1.1.1.1 1994/05/17 20:59:33 jkh Exp $
 */


#include "defs.h"


/*
 * Exported variables.
 */
char		recv_buf[MAX_IP_PACKET_LEN]; /* input packet buffer         */
char		send_buf[MAX_IP_PACKET_LEN]; /* output packet buffer        */
int		igmp_socket;		     /* socket for all network I/O  */
u_long		allhosts_group;		     /* allhosts  addr in net order */
u_long		dvmrp_group;		     /* DVMRP grp addr in net order */


/*
 * Open and initialize the igmp socket, and fill in the non-changing
 * IP header fields in the output packet buffer.
 */
void init_igmp()
{
    struct ip *ip;

    if ((igmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0) 
	log(LOG_ERR, errno, "IGMP socket");

    k_hdr_include(TRUE);	/* include IP header when sending */
    k_set_rcvbuf(48*1024);	/* lots of input buffering        */
    k_set_ttl(1);		/* restrict multicasts to one hop */
    k_set_loop(FALSE);		/* disable multicast loopback     */

    ip         = (struct ip *)send_buf;
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_p   = IPPROTO_IGMP;
    ip->ip_ttl = MAXTTL;	/* applies to unicasts only */

    allhosts_group = htonl(INADDR_ALLHOSTS_GROUP);
    dvmrp_group    = htonl(INADDR_DVMRP_GROUP);
}

static char *packet_kind(type, code)
     u_char type, code;
{
    switch (type) {
	case IGMP_HOST_MEMBERSHIP_QUERY:	return "membership query  ";
	case IGMP_HOST_MEMBERSHIP_REPORT:	return "membership report ";
	case IGMP_DVMRP:
	  switch (code) {
	    case DVMRP_PROBE:	    		return "neighbor probe    ";
	    case DVMRP_REPORT:	    		return "route report      ";
	    case DVMRP_ASK_NEIGHBORS:   	return "neighbor request  ";
	    case DVMRP_NEIGHBORS:	    	return "neighbor list     ";
	    case DVMRP_ASK_NEIGHBORS2:   	return "neighbor request 2";
	    case DVMRP_NEIGHBORS2:	    	return "neighbor list 2   ";
	    default:		    		return "unknown DVMRP msg ";
	  }
	default:			    	return "unknown IGMP msg  ";
    }
}

/*
 * Process a newly received IGMP packet that is sitting in the input
 * packet buffer.
 */
void accept_igmp(recvlen)
    int recvlen;
{
    register vifi_t vifi;
    register u_long src, dst, group;
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
    iphdrlen  = ip->ip_hl << 2;
    ipdatalen = ip->ip_len;
    if (iphdrlen + ipdatalen != recvlen) {
	log(LOG_WARNING, 0,
	    "received packet shorter (%u bytes) than hdr+data length (%u+%u)",
	    recvlen, iphdrlen, ipdatalen);
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

    log(LOG_DEBUG, 0, "RECV %s from %-15s to %s",
	packet_kind(igmp->igmp_type, igmp->igmp_code),
	inet_fmt(src, s1), inet_fmt(dst, s2));

    switch (igmp->igmp_type) {

	case IGMP_HOST_MEMBERSHIP_QUERY:
	    return;	/* Answered automatically by the kernel. */

	case IGMP_HOST_MEMBERSHIP_REPORT:
	    accept_group_report(src, dst, group);
	    return;

	case IGMP_DVMRP:
	    switch (igmp->igmp_code) {

		case DVMRP_PROBE:
		    accept_probe(src, dst);
		    return;

		case DVMRP_REPORT:
		    accept_report(src, dst,
				  (char *)(igmp+1), igmpdatalen);
		    return;

		case DVMRP_ASK_NEIGHBORS:
		    accept_neighbor_request(src, dst);
		    return;

		case DVMRP_ASK_NEIGHBORS2:
		    accept_neighbor_request2(src, dst);
		    return;

		case DVMRP_NEIGHBORS:
		    accept_neighbors(src, dst, (char *)(igmp+1), igmpdatalen,
				     group);
		    return;

		case DVMRP_NEIGHBORS2:
		    accept_neighbors2(src, dst, (char *)(igmp+1), igmpdatalen,
				     group);
		    return;

		default:
		    log(LOG_INFO, 0,
		     "ignoring unknown DVMRP message code %u from %s to %s",
		     igmp->igmp_code, inet_fmt(src, s1),
		     inet_fmt(dst, s2));
		    return;
	    }

	default:
	    log(LOG_INFO, 0,
		"ignoring unknown IGMP message type %u from %s to %s",
		igmp->igmp_type, inet_fmt(src, s1),
		inet_fmt(dst, s2));
	    return;
    }
}


/*
 * Construct an IGMP message in the output packet buffer.  The caller may
 * have already placed data in that buffer, of length 'datalen'.  Then send
 * the message from the interface with IP address 'src' to destination 'dst'.
 */
void send_igmp(src, dst, type, code, group, datalen)
    u_long src, dst;
    int type, code;
    u_long group;
    int datalen;
{
    static struct sockaddr_in sdst = {AF_INET};
    struct ip *ip;
    struct igmp *igmp;

    ip                      = (struct ip *)send_buf;
    ip->ip_src.s_addr       = src;
    ip->ip_dst.s_addr       = dst;
    ip->ip_len              = MIN_IP_HEADER_LEN + IGMP_MINLEN + datalen;

    igmp                    = (struct igmp *)(send_buf + MIN_IP_HEADER_LEN);
    igmp->igmp_type         = type;
    igmp->igmp_code         = code;
    igmp->igmp_group.s_addr = group;
    igmp->igmp_cksum        = 0;
    igmp->igmp_cksum        = inet_cksum((u_short *)igmp,
					 IGMP_MINLEN + datalen);

    if (IN_MULTICAST(ntohl(dst))) k_set_if(src);
    if (dst == allhosts_group) k_set_loop(TRUE);

    sdst.sin_addr.s_addr = dst;
    if (sendto(igmp_socket, send_buf, ip->ip_len, 0,
			(struct sockaddr *)&sdst, sizeof(sdst)) < 0) {
	if (errno == ENETDOWN) check_vif_state();
	else log(LOG_WARNING, errno, "sendto on %s", inet_fmt(src, s1));
    }

    if (dst == allhosts_group) k_set_loop(FALSE);

    log(LOG_DEBUG, 0, "SENT %s from %-15s to %s",
	packet_kind(type, code), inet_fmt(src, s1), inet_fmt(dst, s2));
}
