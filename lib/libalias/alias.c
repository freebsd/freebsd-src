/* -*- mode: c; tab-width: 8; c-basic-indent: 4; -*- */
/*
    Alias.c provides supervisory control for the functions of the
    packet aliasing software.  It consists of routines to monitor
    TCP connection state, protocol-specific aliasing routines,
    fragment handling and the following outside world functional
    interfaces: SaveFragmentPtr, GetFragmentPtr, FragmentAliasIn,
    PacketAliasIn and PacketAliasOut.

    The other C program files are briefly described. The data
    structure framework which holds information needed to translate
    packets is encapsulated in alias_db.c.  Data is accessed by
    function calls, so other segments of the program need not know
    about the underlying data structures.  Alias_ftp.c contains
    special code for modifying the ftp PORT command used to establish
    data connections, while alias_irc.c does the same for IRC
    DCC. Alias_util.c contains a few utility routines.

    This software is placed into the public domain with no restrictions
    on its distribution.

    Version 1.0 August, 1996  (cjm)

    Version 1.1 August 20, 1996  (cjm)
        PPP host accepts incoming connections for ports 0 to 1023.
        (Gary Roberts pointed out the need to handle incoming
         connections.)

    Version 1.2 September 7, 1996 (cjm)
        Fragment handling error in alias_db.c corrected.
        (Tom Torrance helped fix this problem.)

    Version 1.4 September 16, 1996 (cjm)
        - A more generalized method for handling incoming
          connections, without the 0-1023 restriction, is
          implemented in alias_db.c
        - Improved ICMP support in alias.c.  Traceroute
          packet streams can now be correctly aliased.
        - TCP connection closing logic simplified in
          alias.c and now allows for additional 1 minute
          "grace period" after FIN or RST is observed.

    Version 1.5 September 17, 1996 (cjm)
        Corrected error in handling incoming UDP packets with 0 checksum.
        (Tom Torrance helped fix this problem.)

    Version 1.6 September 18, 1996 (cjm)
        Simplified ICMP aliasing scheme.  Should now support
        traceroute from Win95 as well as FreeBSD.

    Version 1.7 January 9, 1997 (cjm)
        - Out-of-order fragment handling.
        - IP checksum error fixed for ftp transfers
          from aliasing host.
        - Integer return codes added to all
          aliasing/de-aliasing functions.
        - Some obsolete comments cleaned up.
        - Differential checksum computations for
          IP header (TCP, UDP and ICMP were already
          differential).

    Version 2.1 May 1997 (cjm)
        - Added support for outgoing ICMP error
          messages.
        - Added two functions PacketAliasIn2()
          and PacketAliasOut2() for dynamic address
          control (e.g. round-robin allocation of
          incoming packets).

    Version 2.2 July 1997 (cjm)
        - Rationalized API function names to begin
          with "PacketAlias..."
        - Eliminated PacketAliasIn2() and
          PacketAliasOut2() as poorly conceived.

    Version 2.3 Dec 1998 (dillon)
	- Major bounds checking additions, see FreeBSD/CVS

    Version 3.1 May, 2000 (salander) 
	- Added hooks to handle PPTP.

    Version 3.2 July, 2000 (salander and satoh)
	- Added PacketUnaliasOut routine.
	- Added hooks to handle RTSP/RTP.

    See HISTORY file for additional revisions.

    $FreeBSD: src/lib/libalias/alias.c,v 1.16.2.3 2000/09/14 17:32:38 ru Exp $
*/

#include <sys/types.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "alias_local.h"
#include "alias.h"

#define NETBIOS_NS_PORT_NUMBER 137
#define NETBIOS_DGM_PORT_NUMBER 138
#define FTP_CONTROL_PORT_NUMBER 21
#define IRC_CONTROL_PORT_NUMBER_1 6667
#define IRC_CONTROL_PORT_NUMBER_2 6668
#define CUSEEME_PORT_NUMBER 7648
#define RTSP_CONTROL_PORT_NUMBER_1 554
#define RTSP_CONTROL_PORT_NUMBER_2 7070
#define PPTP_CONTROL_PORT_NUMBER 1723




/* TCP Handling Routines

    TcpMonitorIn()  -- These routines monitor TCP connections, and
    TcpMonitorOut()    delete a link when a connection is closed.

These routines look for SYN, FIN and RST flags to determine when TCP
connections open and close.  When a TCP connection closes, the data
structure containing packet aliasing information is deleted after
a timeout period.
*/

/* Local prototypes */
static void TcpMonitorIn(struct ip *, struct alias_link *);

static void TcpMonitorOut(struct ip *, struct alias_link *);


static void
TcpMonitorIn(struct ip *pip, struct alias_link *link)
{
    struct tcphdr *tc;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));

    switch (GetStateIn(link))
    {
        case ALIAS_TCP_STATE_NOT_CONNECTED:
            if (tc->th_flags & TH_RST)
                SetStateIn(link, ALIAS_TCP_STATE_DISCONNECTED);
            else if (tc->th_flags & TH_SYN)
                SetStateIn(link, ALIAS_TCP_STATE_CONNECTED);
            break;
        case ALIAS_TCP_STATE_CONNECTED:
            if (tc->th_flags & (TH_FIN | TH_RST))
                SetStateIn(link, ALIAS_TCP_STATE_DISCONNECTED);
            break;
    }
}

static void
TcpMonitorOut(struct ip *pip, struct alias_link *link)
{
    struct tcphdr *tc;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
     
    switch (GetStateOut(link))
    {
        case ALIAS_TCP_STATE_NOT_CONNECTED:
            if (tc->th_flags & TH_RST)
                SetStateOut(link, ALIAS_TCP_STATE_DISCONNECTED);
            else if (tc->th_flags & TH_SYN)
                SetStateOut(link, ALIAS_TCP_STATE_CONNECTED);
            break;
        case ALIAS_TCP_STATE_CONNECTED:
            if (tc->th_flags & (TH_FIN | TH_RST))
                SetStateOut(link, ALIAS_TCP_STATE_DISCONNECTED);
            break;
    }
}





/* Protocol Specific Packet Aliasing Routines 

    IcmpAliasIn(), IcmpAliasIn1(), IcmpAliasIn2()
    IcmpAliasOut(), IcmpAliasOut1(), IcmpAliasOut2()
    ProtoAliasIn(), ProtoAliasOut()
    UdpAliasIn(), UdpAliasOut()
    TcpAliasIn(), TcpAliasOut()
    GreAliasIn()

These routines handle protocol specific details of packet aliasing.
One may observe a certain amount of repetitive arithmetic in these
functions, the purpose of which is to compute a revised checksum
without actually summing over the entire data packet, which could be
unnecessarily time consuming.

The purpose of the packet aliasing routines is to replace the source
address of the outgoing packet and then correctly put it back for
any incoming packets.  For TCP and UDP, ports are also re-mapped.

For ICMP echo/timestamp requests and replies, the following scheme
is used: the ID number is replaced by an alias for the outgoing
packet.

ICMP error messages are handled by looking at the IP fragment
in the data section of the message.

For TCP and UDP protocols, a port number is chosen for an outgoing
packet, and then incoming packets are identified by IP address and
port numbers.  For TCP packets, there is additional logic in the event
that sequence and ACK numbers have been altered (as in the case for
FTP data port commands).

The port numbers used by the packet aliasing module are not true
ports in the Unix sense.  No sockets are actually bound to ports.
They are more correctly thought of as placeholders.

All packets go through the aliasing mechanism, whether they come from
the gateway machine or other machines on a local area network.
*/


/* Local prototypes */
static int IcmpAliasIn1(struct ip *);
static int IcmpAliasIn2(struct ip *);
static int IcmpAliasIn (struct ip *);

static int IcmpAliasOut1(struct ip *);
static int IcmpAliasOut2(struct ip *);
static int IcmpAliasOut (struct ip *);

static int ProtoAliasIn(struct ip *);
static int ProtoAliasOut(struct ip *);

static int UdpAliasOut(struct ip *);
static int UdpAliasIn (struct ip *);

static int TcpAliasOut(struct ip *, int);
static int TcpAliasIn (struct ip *);

static int GreAliasIn(struct ip *);


static int
IcmpAliasIn1(struct ip *pip)
{
/*
    De-alias incoming echo and timestamp replies.
    Alias incoming echo and timestamp requests.
*/
    struct alias_link *link;
    struct icmp *ic;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

/* Get source address from ICMP data field and restore original data */
    link = FindIcmpIn(pip->ip_src, pip->ip_dst, ic->icmp_id);
    if (link != NULL)
    {
        u_short original_id;
        int accumulate;

        original_id = GetOriginalPort(link);

/* Adjust ICMP checksum */
        accumulate  = ic->icmp_id;
        accumulate -= original_id;
        ADJUST_CHECKSUM(accumulate, ic->icmp_cksum)

/* Put original sequence number back in */
        ic->icmp_id = original_id;

/* Put original address back into IP header */
        {
            struct in_addr original_address;

            original_address = GetOriginalAddress(link);
            DifferentialChecksum(&pip->ip_sum,
                                 (u_short *) &original_address,
                                 (u_short *) &pip->ip_dst,
                                 2);
            pip->ip_dst = original_address;
        }

        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}

static int
IcmpAliasIn2(struct ip *pip)
{
/*
    Alias incoming ICMP error messages containing
    IP header and first 64 bits of datagram.
*/
    struct ip *ip;
    struct icmp *ic, *ic2;
    struct udphdr *ud;
    struct tcphdr *tc;
    struct alias_link *link;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));
    ip = &ic->icmp_ip;

    ud = (struct udphdr *) ((char *) ip + (ip->ip_hl <<2));
    tc = (struct tcphdr *) ud;
    ic2 = (struct icmp *) ud;

    if (ip->ip_p == IPPROTO_UDP)
        link = FindUdpTcpIn(ip->ip_dst, ip->ip_src,
                            ud->uh_dport, ud->uh_sport,
                            IPPROTO_UDP);
    else if (ip->ip_p == IPPROTO_TCP)
        link = FindUdpTcpIn(ip->ip_dst, ip->ip_src,
                            tc->th_dport, tc->th_sport,
                            IPPROTO_TCP);
    else if (ip->ip_p == IPPROTO_ICMP) {
        if (ic2->icmp_type == ICMP_ECHO || ic2->icmp_type == ICMP_TSTAMP)
            link = FindIcmpIn(ip->ip_dst, ip->ip_src, ic2->icmp_id);
        else
            link = NULL;
    } else
        link = NULL;

    if (link != NULL)
    {
        if (ip->ip_p == IPPROTO_UDP || ip->ip_p == IPPROTO_TCP)
        {
            u_short *sptr;
            int accumulate;
            struct in_addr original_address;
            u_short original_port;

            original_address = GetOriginalAddress(link);
            original_port = GetOriginalPort(link);
    
/* Adjust ICMP checksum */
            sptr = (u_short *) &(ip->ip_src);
            accumulate  = *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &original_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            accumulate += ud->uh_sport;
            accumulate -= original_port;
            ADJUST_CHECKSUM(accumulate, ic->icmp_cksum)

/* Un-alias address in IP header */
            DifferentialChecksum(&pip->ip_sum,
                                 (u_short *) &original_address,
                                 (u_short *) &pip->ip_dst,
                                 2);
            pip->ip_dst = original_address;

/* Un-alias address and port number of original IP packet
fragment contained in ICMP data section */
            ip->ip_src = original_address;
            ud->uh_sport = original_port; 
        }
        else if (ip->ip_p == IPPROTO_ICMP)
        {
            u_short *sptr;
            int accumulate;
            struct in_addr original_address;
            u_short original_id;

            original_address = GetOriginalAddress(link);
            original_id = GetOriginalPort(link);

/* Adjust ICMP checksum */
            sptr = (u_short *) &(ip->ip_src);
            accumulate  = *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &original_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            accumulate += ic2->icmp_id;
            accumulate -= original_id;
            ADJUST_CHECKSUM(accumulate, ic->icmp_cksum)

/* Un-alias address in IP header */
            DifferentialChecksum(&pip->ip_sum,
                                 (u_short *) &original_address,
                                 (u_short *) &pip->ip_dst,
                                 2);
            pip->ip_dst = original_address;

/* Un-alias address of original IP packet and sequence number of 
   embedded ICMP datagram */
            ip->ip_src = original_address;
            ic2->icmp_id = original_id;
        }
        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}


static int
IcmpAliasIn(struct ip *pip)
{
    int iresult;
    struct icmp *ic;

/* Return if proxy-only mode is enabled */
    if (packetAliasMode & PKT_ALIAS_PROXY_ONLY)
        return PKT_ALIAS_OK;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

    iresult = PKT_ALIAS_IGNORED;
    switch (ic->icmp_type)
    {
        case ICMP_ECHOREPLY:
        case ICMP_TSTAMPREPLY:
            if (ic->icmp_code == 0)
            {
                iresult = IcmpAliasIn1(pip);
            }
            break;
        case ICMP_UNREACH:
        case ICMP_SOURCEQUENCH:
        case ICMP_TIMXCEED:
        case ICMP_PARAMPROB:
            iresult = IcmpAliasIn2(pip);
            break;
        case ICMP_ECHO:
        case ICMP_TSTAMP:
            iresult = IcmpAliasIn1(pip);
            break;
    }
    return(iresult);
}


static int
IcmpAliasOut1(struct ip *pip)
{
/*
    Alias outgoing echo and timestamp requests.
    De-alias outgoing echo and timestamp replies.
*/
    struct alias_link *link;
    struct icmp *ic;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

/* Save overwritten data for when echo packet returns */
    link = FindIcmpOut(pip->ip_src, pip->ip_dst, ic->icmp_id);
    if (link != NULL)
    {
        u_short alias_id;
        int accumulate;

        alias_id = GetAliasPort(link);

/* Since data field is being modified, adjust ICMP checksum */
        accumulate  = ic->icmp_id;
        accumulate -= alias_id;
        ADJUST_CHECKSUM(accumulate, ic->icmp_cksum)

/* Alias sequence number */
        ic->icmp_id = alias_id;

/* Change source address */
        {
            struct in_addr alias_address;

            alias_address = GetAliasAddress(link);
            DifferentialChecksum(&pip->ip_sum,
                                 (u_short *) &alias_address,
                                 (u_short *) &pip->ip_src,
                                 2);
            pip->ip_src = alias_address;
        }

        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}


static int
IcmpAliasOut2(struct ip *pip)
{
/*
    Alias outgoing ICMP error messages containing
    IP header and first 64 bits of datagram.
*/
    struct ip *ip;
    struct icmp *ic, *ic2;
    struct udphdr *ud;
    struct tcphdr *tc;
    struct alias_link *link;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));
    ip = &ic->icmp_ip;

    ud = (struct udphdr *) ((char *) ip + (ip->ip_hl <<2));
    tc = (struct tcphdr *) ud;
    ic2 = (struct icmp *) ud;

    if (ip->ip_p == IPPROTO_UDP)
        link = FindUdpTcpOut(ip->ip_dst, ip->ip_src,
                            ud->uh_dport, ud->uh_sport,
                            IPPROTO_UDP);
    else if (ip->ip_p == IPPROTO_TCP)
        link = FindUdpTcpOut(ip->ip_dst, ip->ip_src,
                            tc->th_dport, tc->th_sport,
                            IPPROTO_TCP);
    else if (ip->ip_p == IPPROTO_ICMP) {
        if (ic2->icmp_type == ICMP_ECHO || ic2->icmp_type == ICMP_TSTAMP)
            link = FindIcmpOut(ip->ip_dst, ip->ip_src, ic2->icmp_id);
        else
            link = NULL;
    } else
        link = NULL;

    if (link != NULL)
    {
        if (ip->ip_p == IPPROTO_UDP || ip->ip_p == IPPROTO_TCP)
        {
            u_short *sptr;
            int accumulate;
            struct in_addr alias_address;
            u_short alias_port;

            alias_address = GetAliasAddress(link);
            alias_port = GetAliasPort(link);
    
/* Adjust ICMP checksum */
            sptr = (u_short *) &(ip->ip_dst);
            accumulate  = *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &alias_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            accumulate += ud->uh_dport;
            accumulate -= alias_port;
            ADJUST_CHECKSUM(accumulate, ic->icmp_cksum)

/*
 * Alias address in IP header if it comes from the host
 * the original TCP/UDP packet was destined for.
 */
	    if (pip->ip_src.s_addr == ip->ip_dst.s_addr) {
		DifferentialChecksum(&pip->ip_sum,
				     (u_short *) &alias_address,
				     (u_short *) &pip->ip_src,
				     2);
		pip->ip_src = alias_address;
	    }

/* Alias address and port number of original IP packet
fragment contained in ICMP data section */
            ip->ip_dst = alias_address;
            ud->uh_dport = alias_port; 
        }
        else if (ip->ip_p == IPPROTO_ICMP)
        {
            u_short *sptr;
            int accumulate;
            struct in_addr alias_address;
            u_short alias_id;

            alias_address = GetAliasAddress(link);
            alias_id = GetAliasPort(link);

/* Adjust ICMP checksum */
            sptr = (u_short *) &(ip->ip_dst);
            accumulate  = *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &alias_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            accumulate += ic2->icmp_id;
            accumulate -= alias_id;
            ADJUST_CHECKSUM(accumulate, ic->icmp_cksum)

/*
 * Alias address in IP header if it comes from the host
 * the original ICMP message was destined for.
 */
	    if (pip->ip_src.s_addr == ip->ip_dst.s_addr) {
		DifferentialChecksum(&pip->ip_sum,
				     (u_short *) &alias_address,
				     (u_short *) &pip->ip_src,
				     2);
		pip->ip_src = alias_address;
	    }

/* Alias address of original IP packet and sequence number of 
   embedded ICMP datagram */
            ip->ip_dst = alias_address;
            ic2->icmp_id = alias_id;
        }
        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}


static int
IcmpAliasOut(struct ip *pip)
{
    int iresult;
    struct icmp *ic;

/* Return if proxy-only mode is enabled */
    if (packetAliasMode & PKT_ALIAS_PROXY_ONLY)
        return PKT_ALIAS_OK;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

    iresult = PKT_ALIAS_IGNORED;
    switch (ic->icmp_type)
    {
        case ICMP_ECHO:
        case ICMP_TSTAMP:
            if (ic->icmp_code == 0)
            {
                iresult = IcmpAliasOut1(pip);
            }
            break;
        case ICMP_UNREACH:
        case ICMP_SOURCEQUENCH:
        case ICMP_TIMXCEED:
        case ICMP_PARAMPROB:
            iresult = IcmpAliasOut2(pip);
            break;
        case ICMP_ECHOREPLY:
        case ICMP_TSTAMPREPLY:
            iresult = IcmpAliasOut1(pip);
    }
    return(iresult);
}



static int
ProtoAliasIn(struct ip *pip)
{
/*
  Handle incoming IP packets. The
  only thing which is done in this case is to alias
  the dest IP address of the packet to our inside
  machine.
*/
    struct alias_link *link;

/* Return if proxy-only mode is enabled */
    if (packetAliasMode & PKT_ALIAS_PROXY_ONLY)
        return PKT_ALIAS_OK;

    link = FindProtoIn(pip->ip_src, pip->ip_dst, pip->ip_p);
    if (link != NULL)
    {
        struct in_addr original_address;

        original_address = GetOriginalAddress(link);

/* Restore original IP address */
        DifferentialChecksum(&pip->ip_sum,
                             (u_short *) &original_address,
                             (u_short *) &pip->ip_dst,
                             2);
        pip->ip_dst = original_address;

	return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}


static int
ProtoAliasOut(struct ip *pip)
{
/*
  Handle outgoing IP packets. The
  only thing which is done in this case is to alias
  the source IP address of the packet.
*/
    struct alias_link *link;

/* Return if proxy-only mode is enabled */
    if (packetAliasMode & PKT_ALIAS_PROXY_ONLY)
        return PKT_ALIAS_OK;

    link = FindProtoOut(pip->ip_src, pip->ip_dst, pip->ip_p);
    if (link != NULL)
    {
        struct in_addr alias_address;

        alias_address = GetAliasAddress(link);

/* Change source address */
        DifferentialChecksum(&pip->ip_sum,
                             (u_short *) &alias_address,
                             (u_short *) &pip->ip_src,
                             2);
        pip->ip_src = alias_address;

        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}


static int
GreAliasIn(struct ip *pip)
{
    u_short call_id;
    struct alias_link *link;

/* Return if proxy-only mode is enabled. */
    if (packetAliasMode & PKT_ALIAS_PROXY_ONLY)
        return (PKT_ALIAS_OK);

    if (PptpGetCallID(pip, &call_id)) {
	if ((link = FindPptpIn(pip->ip_src, pip->ip_dst, call_id)) != NULL) {
	    struct in_addr alias_address;
	    struct in_addr original_address;

	    alias_address = GetAliasAddress(link);
	    original_address = GetOriginalAddress(link);
	    PptpSetCallID(pip, GetOriginalPort(link));

	    /* Restore original IP address. */
	    DifferentialChecksum(&pip->ip_sum,
				 (u_short *)&original_address,
				 (u_short *)&pip->ip_dst,
				 2);
	    pip->ip_dst = original_address;

	    return (PKT_ALIAS_OK);
	} else
	    return (PKT_ALIAS_IGNORED);
    } else
	return ProtoAliasIn(pip);
}


static int
UdpAliasIn(struct ip *pip)
{
    struct udphdr *ud;
    struct alias_link *link;

/* Return if proxy-only mode is enabled */
    if (packetAliasMode & PKT_ALIAS_PROXY_ONLY)
        return PKT_ALIAS_OK;

    ud = (struct udphdr *) ((char *) pip + (pip->ip_hl << 2));

    link = FindUdpTcpIn(pip->ip_src, pip->ip_dst,
                        ud->uh_sport, ud->uh_dport,
                        IPPROTO_UDP);
    if (link != NULL)
    {
        struct in_addr alias_address;
        struct in_addr original_address;
        u_short alias_port;
        int accumulate;
        u_short *sptr;
	int r = 0;

        alias_address = GetAliasAddress(link);
        original_address = GetOriginalAddress(link);
        alias_port = ud->uh_dport;
        ud->uh_dport = GetOriginalPort(link);

/* If NETBIOS Datagram, It should be alias address in UDP Data, too */
		if (ntohs(ud->uh_dport) == NETBIOS_DGM_PORT_NUMBER
         || ntohs(ud->uh_sport) == NETBIOS_DGM_PORT_NUMBER )
		{
            r = AliasHandleUdpNbt(pip, link, &original_address, ud->uh_dport);
		} else if (ntohs(ud->uh_dport) == NETBIOS_NS_PORT_NUMBER
         || ntohs(ud->uh_sport) == NETBIOS_NS_PORT_NUMBER )
		{
            r = AliasHandleUdpNbtNS(pip, link, 
								&alias_address,
								&alias_port,
								&original_address, 
								&ud->uh_dport );
		}

        if (ntohs(ud->uh_dport) == CUSEEME_PORT_NUMBER)
            AliasHandleCUSeeMeIn(pip, original_address);

/* If UDP checksum is not zero, then adjust since destination port */
/* is being unaliased and destination port is being altered.       */
        if (ud->uh_sum != 0)
        {
            accumulate  = alias_port;
            accumulate -= ud->uh_dport;
            sptr = (u_short *) &alias_address;
            accumulate += *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &original_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            ADJUST_CHECKSUM(accumulate, ud->uh_sum)
        }

/* Restore original IP address */
        DifferentialChecksum(&pip->ip_sum,
                             (u_short *) &original_address,
                             (u_short *) &pip->ip_dst,
                             2);
        pip->ip_dst = original_address;

	/*
	 * If we cannot figure out the packet, ignore it.
	 */
	if (r < 0)
	    return(PKT_ALIAS_IGNORED);
	else
	    return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}

static int
UdpAliasOut(struct ip *pip)
{
    struct udphdr *ud;
    struct alias_link *link;

/* Return if proxy-only mode is enabled */
    if (packetAliasMode & PKT_ALIAS_PROXY_ONLY)
        return PKT_ALIAS_OK;

    ud = (struct udphdr *) ((char *) pip + (pip->ip_hl << 2));

    link = FindUdpTcpOut(pip->ip_src, pip->ip_dst,
                         ud->uh_sport, ud->uh_dport,
                         IPPROTO_UDP);
    if (link != NULL)
    {
        u_short alias_port;
        struct in_addr alias_address;

        alias_address = GetAliasAddress(link);
        alias_port = GetAliasPort(link);

        if (ntohs(ud->uh_dport) == CUSEEME_PORT_NUMBER)
            AliasHandleCUSeeMeOut(pip, link);

/* If NETBIOS Datagram, It should be alias address in UDP Data, too */
		if (ntohs(ud->uh_dport) == NETBIOS_DGM_PORT_NUMBER
         || ntohs(ud->uh_sport) == NETBIOS_DGM_PORT_NUMBER )
		{
            AliasHandleUdpNbt(pip, link, &alias_address, alias_port);
		} else if (ntohs(ud->uh_dport) == NETBIOS_NS_PORT_NUMBER
         || ntohs(ud->uh_sport) == NETBIOS_NS_PORT_NUMBER )
		{
            AliasHandleUdpNbtNS(pip, link,
								&pip->ip_src,
								&ud->uh_sport,
							    &alias_address,
							 	&alias_port); 
		}

/* If UDP checksum is not zero, adjust since source port is */
/* being aliased and source address is being altered        */
        if (ud->uh_sum != 0)
        {
            int accumulate;
            u_short *sptr;

            accumulate  = ud->uh_sport;
            accumulate -= alias_port;
            sptr = (u_short *) &(pip->ip_src);
            accumulate += *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &alias_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            ADJUST_CHECKSUM(accumulate, ud->uh_sum)
        }

/* Put alias port in UDP header */
        ud->uh_sport = alias_port;

/* Change source address */
        DifferentialChecksum(&pip->ip_sum,
                             (u_short *) &alias_address,
                             (u_short *) &pip->ip_src,
                             2);
        pip->ip_src = alias_address;

        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}



static int
TcpAliasIn(struct ip *pip)
{
    struct tcphdr *tc;
    struct alias_link *link;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));

    link = FindUdpTcpIn(pip->ip_src, pip->ip_dst,
                        tc->th_sport, tc->th_dport,
                        IPPROTO_TCP);
    if (link != NULL)
    {
        struct in_addr alias_address;
        struct in_addr original_address;
        struct in_addr proxy_address;
        u_short alias_port;
        u_short proxy_port;
        int accumulate;
        u_short *sptr;

/* Special processing for IP encoding protocols */
        if (ntohs(tc->th_dport) == PPTP_CONTROL_PORT_NUMBER
         || ntohs(tc->th_sport) == PPTP_CONTROL_PORT_NUMBER)
            AliasHandlePptpIn(pip, link);

        alias_address = GetAliasAddress(link);
        original_address = GetOriginalAddress(link);
        proxy_address = GetProxyAddress(link);
        alias_port = tc->th_dport;
        tc->th_dport = GetOriginalPort(link);
        proxy_port = GetProxyPort(link);

/* Adjust TCP checksum since destination port is being unaliased */
/* and destination port is being altered.                        */
        accumulate  = alias_port;
        accumulate -= tc->th_dport;
        sptr = (u_short *) &alias_address;
        accumulate += *sptr++;
        accumulate += *sptr;
        sptr = (u_short *) &original_address;
        accumulate -= *sptr++;
        accumulate -= *sptr;

/* If this is a proxy, then modify the TCP source port and
   checksum accumulation */
        if (proxy_port != 0)
        {
            accumulate += tc->th_sport;
            tc->th_sport = proxy_port;
            accumulate -= tc->th_sport;

            sptr = (u_short *) &pip->ip_src;
            accumulate += *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &proxy_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
        }

/* See if ACK number needs to be modified */
        if (GetAckModified(link) == 1)
        {
            int delta;

            delta = GetDeltaAckIn(pip, link);
            if (delta != 0)
            {
                sptr = (u_short *) &tc->th_ack;
                accumulate += *sptr++;
                accumulate += *sptr;
                tc->th_ack = htonl(ntohl(tc->th_ack) - delta);
                sptr = (u_short *) &tc->th_ack;
                accumulate -= *sptr++;
                accumulate -= *sptr;
            }
        }

        ADJUST_CHECKSUM(accumulate, tc->th_sum);

/* Restore original IP address */
        sptr = (u_short *) &pip->ip_dst;
        accumulate  = *sptr++;
        accumulate += *sptr;
        pip->ip_dst = original_address;
        sptr = (u_short *) &pip->ip_dst;
        accumulate -= *sptr++;
        accumulate -= *sptr;

/* If this is a transparent proxy packet, then modify the source
   address */
        if (proxy_address.s_addr != 0)
        {
            sptr = (u_short *) &pip->ip_src;
            accumulate += *sptr++;
            accumulate += *sptr;
            pip->ip_src = proxy_address;
            sptr = (u_short *) &pip->ip_src;
            accumulate -= *sptr++;
            accumulate -= *sptr;
        }

        ADJUST_CHECKSUM(accumulate, pip->ip_sum);

/* Monitor TCP connection state */
        TcpMonitorIn(pip, link);

        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}

static int
TcpAliasOut(struct ip *pip, int maxpacketsize)
{
    int proxy_type;
    u_short dest_port;
    u_short proxy_server_port;
    struct in_addr dest_address;
    struct in_addr proxy_server_address;
    struct tcphdr *tc;
    struct alias_link *link;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));

    proxy_type = ProxyCheck(pip, &proxy_server_address, &proxy_server_port);

    if (proxy_type == 0 && (packetAliasMode & PKT_ALIAS_PROXY_ONLY))
        return PKT_ALIAS_OK;

/* If this is a transparent proxy, save original destination,
   then alter the destination and adjust checksums */
    dest_port = tc->th_dport;
    dest_address = pip->ip_dst;
    if (proxy_type != 0)
    {
        int accumulate;
        u_short *sptr;

        accumulate = tc->th_dport;
        tc->th_dport = proxy_server_port;
        accumulate -= tc->th_dport;

        sptr = (u_short *) &(pip->ip_dst);
        accumulate += *sptr++;
        accumulate += *sptr;
        sptr = (u_short *) &proxy_server_address;
        accumulate -= *sptr++;
        accumulate -= *sptr;

        ADJUST_CHECKSUM(accumulate, tc->th_sum);

        sptr = (u_short *) &(pip->ip_dst);
        accumulate  = *sptr++;
        accumulate += *sptr;
        pip->ip_dst = proxy_server_address;
        sptr = (u_short *) &(pip->ip_dst);
        accumulate -= *sptr++;
        accumulate -= *sptr;

        ADJUST_CHECKSUM(accumulate, pip->ip_sum);
    }

    link = FindUdpTcpOut(pip->ip_src, pip->ip_dst,
                         tc->th_sport, tc->th_dport,
                         IPPROTO_TCP);
    if (link !=NULL)
    {
        u_short alias_port;
        struct in_addr alias_address;
        int accumulate;
        u_short *sptr;

/* Save original destination address, if this is a proxy packet.
   Also modify packet to include destination encoding. */
        if (proxy_type != 0)
        {
            SetProxyPort(link, dest_port);
            SetProxyAddress(link, dest_address);
            ProxyModify(link, pip, maxpacketsize, proxy_type);
        }

/* Get alias address and port */
        alias_port = GetAliasPort(link);
        alias_address = GetAliasAddress(link);

/* Monitor TCP connection state */
        TcpMonitorOut(pip, link);

/* Special processing for IP encoding protocols */
        if (ntohs(tc->th_dport) == FTP_CONTROL_PORT_NUMBER
         || ntohs(tc->th_sport) == FTP_CONTROL_PORT_NUMBER)
            AliasHandleFtpOut(pip, link, maxpacketsize);
        else if (ntohs(tc->th_dport) == IRC_CONTROL_PORT_NUMBER_1
         || ntohs(tc->th_dport) == IRC_CONTROL_PORT_NUMBER_2)
            AliasHandleIrcOut(pip, link, maxpacketsize);
        else if (ntohs(tc->th_dport) == RTSP_CONTROL_PORT_NUMBER_1
         || ntohs(tc->th_sport) == RTSP_CONTROL_PORT_NUMBER_1
         || ntohs(tc->th_dport) == RTSP_CONTROL_PORT_NUMBER_2
         || ntohs(tc->th_sport) == RTSP_CONTROL_PORT_NUMBER_2) 
            AliasHandleRtspOut(pip, link, maxpacketsize);
        else if (ntohs(tc->th_dport) == PPTP_CONTROL_PORT_NUMBER
         || ntohs(tc->th_sport) == PPTP_CONTROL_PORT_NUMBER)
            AliasHandlePptpOut(pip, link);

/* Adjust TCP checksum since source port is being aliased */
/* and source address is being altered                    */
        accumulate  = tc->th_sport;
        tc->th_sport = alias_port;
        accumulate -= tc->th_sport;

        sptr = (u_short *) &(pip->ip_src);
        accumulate += *sptr++;
        accumulate += *sptr;
        sptr = (u_short *) &alias_address;
        accumulate -= *sptr++;
        accumulate -= *sptr;

/* Modify sequence number if necessary */
        if (GetAckModified(link) == 1)
        {
            int delta;

            delta = GetDeltaSeqOut(pip, link);
            if (delta != 0)
            {
                sptr = (u_short *) &tc->th_seq;
                accumulate += *sptr++;
                accumulate += *sptr;
                tc->th_seq = htonl(ntohl(tc->th_seq) + delta);
                sptr = (u_short *) &tc->th_seq;
                accumulate -= *sptr++;
                accumulate -= *sptr;
            }
        }

        ADJUST_CHECKSUM(accumulate, tc->th_sum)

/* Change source address */
        sptr = (u_short *) &(pip->ip_src);
        accumulate  = *sptr++;
        accumulate += *sptr;
        pip->ip_src = alias_address;
        sptr = (u_short *) &(pip->ip_src);
        accumulate -= *sptr++;
        accumulate -= *sptr;

        ADJUST_CHECKSUM(accumulate, pip->ip_sum)

        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_IGNORED);
}




/* Fragment Handling

    FragmentIn()
    FragmentOut()

The packet aliasing module has a limited ability for handling IP
fragments.  If the ICMP, TCP or UDP header is in the first fragment
received, then the ID number of the IP packet is saved, and other
fragments are identified according to their ID number and IP address
they were sent from.  Pointers to unresolved fragments can also be
saved and recalled when a header fragment is seen.
*/

/* Local prototypes */
static int FragmentIn(struct ip *);
static int FragmentOut(struct ip *);


static int
FragmentIn(struct ip *pip)
{
    struct alias_link *link;

    link = FindFragmentIn2(pip->ip_src, pip->ip_dst, pip->ip_id);
    if (link != NULL)
    {
        struct in_addr original_address;

        GetFragmentAddr(link, &original_address);
        DifferentialChecksum(&pip->ip_sum,
                             (u_short *) &original_address,
                             (u_short *) &pip->ip_dst,
                             2);
        pip->ip_dst = original_address; 
   
        return(PKT_ALIAS_OK);
    }
    return(PKT_ALIAS_UNRESOLVED_FRAGMENT);
}


static int
FragmentOut(struct ip *pip)
{
    struct in_addr alias_address;

    alias_address = FindAliasAddress(pip->ip_src);
    DifferentialChecksum(&pip->ip_sum,
                         (u_short *) &alias_address,
                         (u_short *) &pip->ip_src,
                          2);
    pip->ip_src = alias_address;

    return(PKT_ALIAS_OK);
}






/* Outside World Access

        PacketAliasSaveFragment()
        PacketAliasGetFragment()
        PacketAliasFragmentIn()
        PacketAliasIn()
        PacketAliasOut()
        PacketUnaliasOut()

(prototypes in alias.h)
*/


int
PacketAliasSaveFragment(char *ptr)
{
    int iresult;
    struct alias_link *link;
    struct ip *pip;

    pip = (struct ip *) ptr;
    link = AddFragmentPtrLink(pip->ip_src, pip->ip_id);
    iresult = PKT_ALIAS_ERROR;
    if (link != NULL)
    {
        SetFragmentPtr(link, ptr);
        iresult = PKT_ALIAS_OK;
    }
    return(iresult);
}


char *
PacketAliasGetFragment(char *ptr)
{
    struct alias_link *link;
    char *fptr;
    struct ip *pip;

    pip = (struct ip *) ptr;
    link = FindFragmentPtr(pip->ip_src, pip->ip_id);
    if (link != NULL)
    {
        GetFragmentPtr(link, &fptr);
        SetFragmentPtr(link, NULL);
        SetExpire(link, 0); /* Deletes link */

        return(fptr);
    }
    else
    {
        return(NULL);
    }
}


void
PacketAliasFragmentIn(char *ptr,          /* Points to correctly de-aliased
                                             header fragment */
                      char *ptr_fragment  /* Points to fragment which must
                                             be de-aliased   */
                     )
{
    struct ip *pip;
    struct ip *fpip;

    pip = (struct ip *) ptr;
    fpip = (struct ip *) ptr_fragment;

    DifferentialChecksum(&fpip->ip_sum,
                         (u_short *) &pip->ip_dst,
                         (u_short *) &fpip->ip_dst,
                         2);
    fpip->ip_dst = pip->ip_dst;
}


int
PacketAliasIn(char *ptr, int maxpacketsize)
{
    struct in_addr alias_addr;
    struct ip *pip;
    int iresult;

    if (packetAliasMode & PKT_ALIAS_REVERSE) {
        packetAliasMode &= ~PKT_ALIAS_REVERSE;
        iresult = PacketAliasOut(ptr, maxpacketsize);
        packetAliasMode |= PKT_ALIAS_REVERSE;
        return iresult;
    }

    HouseKeeping();
    ClearCheckNewLink();
    pip = (struct ip *) ptr;
    alias_addr = pip->ip_dst;
        
    /* Defense against mangled packets */
    if (ntohs(pip->ip_len) > maxpacketsize
     || (pip->ip_hl<<2) > maxpacketsize)
        return PKT_ALIAS_IGNORED;
        
    iresult = PKT_ALIAS_IGNORED;
    if ( (ntohs(pip->ip_off) & IP_OFFMASK) == 0 )
    {
        switch (pip->ip_p)
        {
            case IPPROTO_ICMP:
                iresult = IcmpAliasIn(pip);
                break;
            case IPPROTO_UDP:
                iresult = UdpAliasIn(pip);
                break;
            case IPPROTO_TCP:
                iresult = TcpAliasIn(pip);
                break;
            case IPPROTO_GRE:
                iresult = GreAliasIn(pip);
                break;
	    default:
		iresult = ProtoAliasIn(pip);
                break;
        }

        if (ntohs(pip->ip_off) & IP_MF)
        {
            struct alias_link *link;

            link = FindFragmentIn1(pip->ip_src, alias_addr, pip->ip_id);
            if (link != NULL)
            {
                iresult = PKT_ALIAS_FOUND_HEADER_FRAGMENT;
                SetFragmentAddr(link, pip->ip_dst);
            }
            else
            {
                iresult = PKT_ALIAS_ERROR;
            }
        }
    }
    else
    {
        iresult = FragmentIn(pip);
    }

    return(iresult);
}



/* Unregistered address ranges */

/* 10.0.0.0   ->   10.255.255.255 */
#define UNREG_ADDR_A_LOWER 0x0a000000
#define UNREG_ADDR_A_UPPER 0x0affffff

/* 172.16.0.0  ->  172.31.255.255 */
#define UNREG_ADDR_B_LOWER 0xac100000
#define UNREG_ADDR_B_UPPER 0xac1fffff

/* 192.168.0.0 -> 192.168.255.255 */
#define UNREG_ADDR_C_LOWER 0xc0a80000
#define UNREG_ADDR_C_UPPER 0xc0a8ffff

int
PacketAliasOut(char *ptr,           /* valid IP packet */
               int  maxpacketsize   /* How much the packet data may grow
                                       (FTP and IRC inline changes) */
              )
{
    int iresult;
    struct in_addr addr_save;
    struct ip *pip;

    if (packetAliasMode & PKT_ALIAS_REVERSE) {
        packetAliasMode &= ~PKT_ALIAS_REVERSE;
        iresult = PacketAliasIn(ptr, maxpacketsize);
        packetAliasMode |= PKT_ALIAS_REVERSE;
        return iresult;
    }

    HouseKeeping();
    ClearCheckNewLink();
    pip = (struct ip *) ptr;

    /* Defense against mangled packets */
    if (ntohs(pip->ip_len) > maxpacketsize
     || (pip->ip_hl<<2) > maxpacketsize)
        return PKT_ALIAS_IGNORED;

    addr_save = GetDefaultAliasAddress();
    if (packetAliasMode & PKT_ALIAS_UNREGISTERED_ONLY)
    {
        u_long addr;
        int iclass;

        iclass = 0;
        addr = ntohl(pip->ip_src.s_addr);
        if      (addr >= UNREG_ADDR_C_LOWER && addr <= UNREG_ADDR_C_UPPER)
            iclass = 3;
        else if (addr >= UNREG_ADDR_B_LOWER && addr <= UNREG_ADDR_B_UPPER)
            iclass = 2;
        else if (addr >= UNREG_ADDR_A_LOWER && addr <= UNREG_ADDR_A_UPPER)
            iclass = 1;

        if (iclass == 0)
        {
            SetDefaultAliasAddress(pip->ip_src);
        }
    }

    iresult = PKT_ALIAS_IGNORED;
    if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0)
    {
        switch (pip->ip_p)
        {
            case IPPROTO_ICMP:
                iresult = IcmpAliasOut(pip);
                break;
            case IPPROTO_UDP:
                iresult = UdpAliasOut(pip);
                break;
            case IPPROTO_TCP:
                iresult = TcpAliasOut(pip, maxpacketsize);
                break;
	    default:
		iresult = ProtoAliasOut(pip);
                break;
        }
    }
    else
    {
        iresult = FragmentOut(pip);
    }

    SetDefaultAliasAddress(addr_save);
    return(iresult);
}

int
PacketUnaliasOut(char *ptr,           /* valid IP packet */
                 int  maxpacketsize   /* for error checking */
                )
{
    struct ip		*pip;
    struct icmp 	*ic;
    struct udphdr	*ud;
    struct tcphdr 	*tc;
    struct alias_link 	*link;
    int 		iresult = PKT_ALIAS_IGNORED;

    pip = (struct ip *) ptr;

    /* Defense against mangled packets */
    if (ntohs(pip->ip_len) > maxpacketsize
     || (pip->ip_hl<<2) > maxpacketsize)
        return(iresult);

    ud = (struct udphdr *) ((char *) pip + (pip->ip_hl << 2));
    tc = (struct tcphdr *) ud;
    ic = (struct icmp *) ud;

    /* Find a link */
    if (pip->ip_p == IPPROTO_UDP)
        link = QueryUdpTcpIn(pip->ip_dst, pip->ip_src,
                            ud->uh_dport, ud->uh_sport,
                            IPPROTO_UDP);
    else if (pip->ip_p == IPPROTO_TCP)
        link = QueryUdpTcpIn(pip->ip_dst, pip->ip_src,
                            tc->th_dport, tc->th_sport,
                            IPPROTO_TCP);
    else if (pip->ip_p == IPPROTO_ICMP) 
        link = FindIcmpIn(pip->ip_dst, pip->ip_src, ic->icmp_id);
    else
        link = NULL;

    /* Change it from an aliased packet to an unaliased packet */
    if (link != NULL)
    {
        if (pip->ip_p == IPPROTO_UDP || pip->ip_p == IPPROTO_TCP)
        {
            u_short        *sptr;
            int 	   accumulate;
            struct in_addr original_address;
            u_short        original_port;

            original_address = GetOriginalAddress(link);
            original_port = GetOriginalPort(link);
    
            /* Adjust TCP/UDP checksum */
            sptr = (u_short *) &(pip->ip_src);
            accumulate  = *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &original_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;

            if (pip->ip_p == IPPROTO_UDP) {
                accumulate += ud->uh_sport;
                accumulate -= original_port;
                ADJUST_CHECKSUM(accumulate, ud->uh_sum)
	    } else { 
                accumulate += tc->th_sport;
                accumulate -= original_port;
                ADJUST_CHECKSUM(accumulate, tc->th_sum)
	    }

            /* Adjust IP checksum */
            DifferentialChecksum(&pip->ip_sum,
                                 (u_short *) &original_address,
                                 (u_short *) &pip->ip_src,
                                 2);

            /* Un-alias source address and port number */ 
            pip->ip_src = original_address;
            if (pip->ip_p == IPPROTO_UDP) 
                ud->uh_sport = original_port; 
	    else   
                tc->th_sport = original_port; 
            
	    iresult = PKT_ALIAS_OK;

        } else if (pip->ip_p == IPPROTO_ICMP) {

            u_short        *sptr;
            int            accumulate;
            struct in_addr original_address;
            u_short        original_id;

            original_address = GetOriginalAddress(link);
            original_id = GetOriginalPort(link);

            /* Adjust ICMP checksum */
            sptr = (u_short *) &(pip->ip_src);
            accumulate  = *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &original_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            accumulate += ic->icmp_id;
            accumulate -= original_id;
            ADJUST_CHECKSUM(accumulate, ic->icmp_cksum)

            /* Adjust IP checksum */
            DifferentialChecksum(&pip->ip_sum,
                                 (u_short *) &original_address,
                                 (u_short *) &pip->ip_src,
                                 2);

            /* Un-alias source address and port number */
            pip->ip_src = original_address;
            ic->icmp_id = original_id;

	    iresult = PKT_ALIAS_OK;
        }
    }
    return(iresult);

}
