/*
    Alias.c provides supervisory control for the functions of the
    packet aliasing software.  It consists of routines to monitor
    TCP connection state, protocol-specific aliasing routines,
    limited fragment handling and the two primary outside world
    functional interfaces: PacketAliasIn and PacketAliasOut.

    The other C program files are briefly described. The data
    structure framework which holds information needed to translate
    packets is encapsulated in alias_db.c.  Data is accessed by
    function calls, so other segments of the program need not
    know about the underlying data structures.  Alias_ftp.c contains
    special code for modifying the ftp PORT command used to establish
    data connections.  Alias_util.c contains a few utility routines.

    This software is placed into the public domain with no restrictions
    on its distribution.

    Version 1.0 August, 1996  (cjm)

    Version 1.1 August 20, 1996  (cjm)
        PPP host accepts incoming connections for ports 0 to 1023.

    Version 1.2 September 7, 1996 (cjm)
        Fragment handling error in alias_db.c corrected.

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
    Version 1.6 September 18, 1996 (cjm)
        Simplified ICMP aliasing scheme.  Should now support
        traceroute from Win95 as well as FreeBSD.
         
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include "alias.p"

#define FTP_CONTROL_PORT_NUMBER 21




/* TCP Handling Routines

    TcpMonitorIn()  -- These routines monitor TCP connections, and
    TcpMonitorOut() -- delete a link node when a connection is closed.

These routines look for SYN, ACK and RST flags to determine when TCP
connections open and close.  When a TCP connection closes, the data
structure containing packet aliasing information is deleted after
a timeout period.
*/

void
TcpMonitorIn(pip, link)
struct ip *pip;
char *link;
{
    struct tcphdr *tc;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));

    switch (GetStateIn(link))
    {
        case 0:
            if (tc->th_flags & TH_SYN) SetStateIn(link, 1);
            break;
        case 1:
            if (tc->th_flags & TH_FIN
             || tc->th_flags & TH_RST) SetStateIn(link, 2);
    }
}

void
TcpMonitorOut(pip, link)
struct ip *pip;
char *link;
{
    struct tcphdr *tc;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));
     
    switch (GetStateOut(link))
    {
        case 0:
            if (tc->th_flags & TH_SYN) SetStateOut(link, 1);
            break;
        case 1:
            if (tc->th_flags & TH_FIN
             || tc->th_flags & TH_RST) SetStateOut(link, 2);
    }
}





/* Protocol Specific Packet Aliasing Routines 

    IcmpAliasIn(), IcmpAliasIn1(), IcmpAliasIn2
    IcmpAliasOut(), IcmpAliasOut1()
    UdpAliasIn(), UdpAliasOut()
    TcpAliasIn(), TcpAliasOut()

These routines handle protocol specific details of packet aliasing.
One may observe a certain amount of repetitive arithmetic in these
functions, the purpose of which is to compute a revised checksum
without actually summing over the entire data packet, which could be
unnecessarily time consuming.

The purpose of the packet aliasing routines is to replace the source
address of the outgoing packet and then correctly put it back for
any incoming packets.  For TCP and UDP, ports are also re-mapped.

For ICMP echo/timestamp requests and replies, the following scheme
is used: the sequence number is replaced by an alias for the outgoing
packet and this sequence number, plus the id and remote address are
used to find the packet on the return path.

ICMP error messages are handled by looking at the IP fragment
in the data section of the message.

For TCP and UDP protocols, a port number is chosen for an outgoing
packet, and then incoming packets are identified by IP address and
port number.  For TCP packets, there is additional logic in the event
that sequence and ack numbers have been altered (as is the case for
FTP data port commands).

The port numbers used by the packet aliasing module are not true
ports in the Unix sense.  No sockets are actually bound to ports.
They are more correctly placeholders.

All packets are aliased, whether they come from the gateway machine
or other machines on a local area network.
*/

void
IcmpAliasIn1(pip)
struct ip *pip;
{
/*
    Un-alias incoming echo and timestamp replies
*/
    char *link;
    struct icmp *ic;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

/* Get source address from ICMP data field and restore original data */
    link = FindIcmpIn(pip->ip_src, ic->icmp_id, ic->icmp_seq);
    if (link != NULL_PTR)
    {
        u_short original_seq;
        int accumulate;

        original_seq = GetOriginalPort(link);

/* Adjust ICMP checksum */
        accumulate = ic->icmp_cksum;
        accumulate += ic->icmp_seq;
        accumulate -= original_seq;

        if (accumulate < 0)
        {
            accumulate = -accumulate;
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            ic->icmp_cksum = (u_short) ~accumulate;
        }
        else
        {
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            ic->icmp_cksum = (u_short) accumulate;
        }

/* Put original sequence number back in */
        ic->icmp_seq = original_seq;

/* Put original address back into IP header */
        pip->ip_dst = GetOriginalAddress(link);

/* Delete unneeded data structure */
        DeleteLink(link);
    }
}

void
IcmpAliasIn2(pip)
struct ip *pip;
{
/*
    Alias incoming ICMP error messages containing
    IP header and first 64 bits of datagram.
*/
    struct ip *ip;
    struct icmp *ic, *ic2;
    struct udphdr *ud;
    struct tcphdr *tc;
    char *link;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));
    ip = (struct ip *) ic->icmp_data;

    ud = (struct udphdr *) ((char *) ip + (ip->ip_hl <<2));
    tc = (struct tcphdr *) ud;
    ic2 = (struct icmp *) ud;

    if (ip->ip_p == IPPROTO_UDP)
        link = FindUdpIn(ip->ip_dst, ud->uh_dport, ud->uh_sport);
    else if (ip->ip_p == IPPROTO_TCP)
        link = FindTcpIn(ip->ip_dst, tc->th_dport, tc->th_sport);
    else if (ip->ip_p == IPPROTO_ICMP)
        if (ic2->icmp_type == ICMP_ECHO || ic2->icmp_type == ICMP_TSTAMP)
            link = FindIcmpIn(ip->ip_dst, ic2->icmp_id, ic2->icmp_seq);
         else
            link = NULL_PTR;
    else
        link = NULL_PTR;

    if (link != NULL_PTR)
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
            accumulate = ic->icmp_cksum;
            sptr = (u_short *) &(ip->ip_src);
            accumulate += *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &original_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            accumulate += ud->uh_sport;
            accumulate -= original_port;

            if (accumulate < 0)
            {
                accumulate = -accumulate;
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ic->icmp_cksum = (u_short) ~accumulate;
            }
            else
            {
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ic->icmp_cksum = (u_short) accumulate;
            }

/* Un-alias address in IP header */
            pip->ip_dst = original_address;

/* Un-alias address and port number of original IP packet
fragment contained in ICMP data section */
            ip->ip_src = original_address;
            ud->uh_sport = original_port; 
        }
        else if (pip->ip_p == IPPROTO_ICMP)
        {
            u_short *sptr;
            int accumulate;
            struct in_addr original_address;
            u_short original_seq;

            original_address = GetOriginalAddress(link);
            original_seq = GetOriginalPort(link);

/* Adjust ICMP checksum */
            accumulate = ic->icmp_cksum;
            sptr = (u_short *) &(ip->ip_src);
            accumulate += *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &original_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;
            accumulate += ic2->icmp_seq;
            accumulate -= original_seq;

            if (accumulate < 0)
            {
                accumulate = -accumulate;
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ic->icmp_cksum = (u_short) ~accumulate;
            }
            else
            {
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ic->icmp_cksum = (u_short) accumulate;
            }

/* Un-alias address in IP header */
            pip->ip_dst = original_address;

/* Un-alias address of original IP packet and seqence number of 
   embedded icmp datagram */
            ip->ip_src = original_address;
            ic2->icmp_seq = original_seq;
        }
    }
}

void
IcmpAliasIn(pip)
struct ip *pip;
{
    struct icmp *ic;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

    switch (ic->icmp_type)
    {
        case ICMP_ECHOREPLY:
        case ICMP_TSTAMPREPLY:
            if (ic->icmp_code == 0)
            {
                IcmpAliasIn1(pip);
            }
            break;
        case ICMP_UNREACH:
        case ICMP_SOURCEQUENCH:
        case ICMP_TIMXCEED:
        case ICMP_PARAMPROB:
            IcmpAliasIn2(pip);
            break;
    }
}


void
IcmpAliasOut1(pip)
struct ip *pip;
{
/*
    Alias ICMP echo and timestamp packets
*/
    char *link;
    struct icmp *ic;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

/* Save overwritten data for when echo packet returns */
    link = FindIcmpOut(pip->ip_src, pip->ip_dst, ic->icmp_id, ic->icmp_seq);
    if (link != NULL_PTR)
    {
        u_short alias_seq;
        int accumulate;

        alias_seq = GetAliasPort(link);

/* Since data field is being modified, adjust ICMP checksum */
        accumulate = ic->icmp_cksum;
        accumulate += ic->icmp_seq;
        accumulate -= alias_seq;

        if (accumulate < 0)
        {
            accumulate = -accumulate;
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            ic->icmp_cksum = (u_short) ~accumulate;
        }
        else
        {
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            ic->icmp_cksum = (u_short) accumulate;
        }

/* Alias sequence number */
        ic->icmp_seq = alias_seq;

/* Change source address */
        pip->ip_src = GetAliasAddress();
    }
}

void
IcmpAliasOut(pip)
struct ip *pip;
{
    struct icmp *ic;

    ic = (struct icmp *) ((char *) pip + (pip->ip_hl << 2));

    switch (ic->icmp_type)
    {
        case ICMP_ECHO:
        case ICMP_TSTAMP:
            if (ic->icmp_code == 0)
            {
                IcmpAliasOut1(pip);
            }
            break;
    }
}

void
UdpAliasIn(pip)
struct ip *pip;
{
    struct udphdr *ud;
    char *link;

    ud = (struct udphdr *) ((char *) pip + (pip->ip_hl << 2));

    link = FindUdpIn(pip->ip_src, ud->uh_sport, ud->uh_dport);
    if (link != NULL_PTR)
    {
        struct in_addr alias_address;
        u_short alias_port;
        int accumulate;
        u_short *sptr;

        alias_address = GetAliasAddress();
        pip->ip_dst = GetOriginalAddress(link);
        alias_port = ud->uh_dport;
        ud->uh_dport = GetOriginalPort(link);

/* If UDP checksum is not zero, then adjust since destination port */
/* is being unaliased and destination port is being altered.       */
        if (ud->uh_sum != 0)
        {
            accumulate = ud->uh_sum;
            accumulate += alias_port;
            accumulate -= ud->uh_dport;
            sptr = (u_short *) &alias_address;
            accumulate += *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &(pip->ip_dst);
            accumulate -= *sptr++;
            accumulate -= *sptr;

            if (accumulate < 0)
            {
                accumulate = -accumulate;
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ud->uh_sum = (u_short) ~accumulate;
            }
            else
            {
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ud->uh_sum = (u_short) accumulate;
            }
        }
    }
}

void
UdpAliasOut(pip)
struct ip *pip;
{
    struct udphdr *ud;
    char *link;

    ud = (struct udphdr *) ((char *) pip + (pip->ip_hl << 2));

    link = FindUdpOut(pip->ip_src, pip->ip_dst, ud->uh_sport, ud->uh_dport);
    if (link != NULL_PTR)
    {
        u_short alias_port;

        alias_port = GetAliasPort(link);

/* If UDP checksum is not zero, adjust since source port is */
/* being aliased and source address is being altered        */
        if (ud->uh_sum != 0)
        {
            struct in_addr alias_address;
            int accumulate;
            u_short *sptr;

            alias_address = GetAliasAddress();

            accumulate = ud->uh_sum;
            accumulate += ud->uh_sport;
            accumulate -= alias_port;
            sptr = (u_short *) &(pip->ip_src);
            accumulate += *sptr++;
            accumulate += *sptr;
            sptr = (u_short *) &alias_address;
            accumulate -= *sptr++;
            accumulate -= *sptr;

            if (accumulate < 0)
            {
                accumulate = -accumulate;
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ud->uh_sum = (u_short) ~accumulate;
            }
            else
            {
                accumulate = (accumulate >> 16) + (accumulate & 0xffff);
                accumulate += accumulate >> 16;
                ud->uh_sum = (u_short) accumulate;
            }
        }

/* Put alias port in TCP header */
        ud->uh_sport = alias_port;

/* Change source address */
        pip->ip_src = GetAliasAddress();
    }
}



void
TcpAliasIn(pip)
struct ip *pip;
{
    struct tcphdr *tc;
    char *link;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));

    link = FindTcpIn(pip->ip_src, tc->th_sport, tc->th_dport);
    if (link != NULL_PTR)
    {
        struct in_addr alias_address;
        u_short alias_port;
        int accumulate;
        u_short *sptr;

        alias_address = GetAliasAddress();
        pip->ip_dst = GetOriginalAddress(link);
        alias_port = tc->th_dport;
        tc->th_dport = GetOriginalPort(link);

/* Adjust TCP checksum since destination port is being unaliased */
/* and destination port is being altered.                        */
        accumulate = tc->th_sum;
        accumulate += alias_port;
        accumulate -= tc->th_dport;
        sptr = (u_short *) &alias_address;
        accumulate += *sptr++;
        accumulate += *sptr;
        sptr = (u_short *) &(pip->ip_dst);
        accumulate -= *sptr++;
        accumulate -= *sptr;

/* See if ack number needs to be modified */
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

/* Finish checksum modification */
        if (accumulate < 0)
        {
            accumulate = -accumulate;
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            tc->th_sum = (u_short) ~accumulate;
        }
        else
        {
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            tc->th_sum = (u_short) accumulate;
        }

/* Monitor TCP connection state */
        TcpMonitorIn(pip, link);
    }
}

void
TcpAliasOut(pip)
struct ip *pip;
{
    struct tcphdr *tc;
    char *link;

    tc = (struct tcphdr *) ((char *) pip + (pip->ip_hl << 2));

    link = FindTcpOut(pip->ip_src, pip->ip_dst, tc->th_sport, tc->th_dport);
    if (link !=NULL_PTR)
    {
        struct in_addr alias_address;
        u_short alias_port;
        int accumulate;
        u_short *sptr;

        alias_address = GetAliasAddress();
        alias_port = GetAliasPort(link);

/* Monitor tcp connection state */
        TcpMonitorOut(pip, link);

/* Special processing for ftp connection */
        if (ntohs(tc->th_dport) == FTP_CONTROL_PORT_NUMBER
         || ntohs(tc->th_sport) == FTP_CONTROL_PORT_NUMBER)
            HandleFtpOut(pip, link);

/* Adjust TCP checksum since source port is being aliased */
/* and source address is being altered                    */
        accumulate = tc->th_sum;
        accumulate += tc->th_sport;
        accumulate -= alias_port;
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

/* Finish up checksum calculation */
        if (accumulate < 0)
        {
            accumulate = -accumulate;
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            tc->th_sum = (u_short) ~accumulate;
        }
        else
        {
            accumulate = (accumulate >> 16) + (accumulate & 0xffff);
            accumulate += accumulate >> 16;
            tc->th_sum = (u_short) accumulate;
        }

/* Put alias address in TCP header */
        tc->th_sport = alias_port;

/* Change source address */
        pip->ip_src = GetAliasAddress();
    }
}




/* Fragment Handling

    FragmentIn()
    FragmentOut()

The packet aliasing module has a limited ability for handling IP
fragments.  If the ICMP, TCP or UDP header is in the first fragment
received, then the id number of the IP packet is saved, and other
fragments are identified according to their ID number and IP address
they were sent from.

In general, fragments seem few and far between these days.  One way
to generate them is with a ping request specifying a large data segment.
This is how the software here was tested.

In principle, out-of-order IP fragments could also be handled by saving
fragments until the header fragment came in and then sending them on
their way.  However, this violates a basic interface rule of the
aliasing module in which individual packets are sent for remapping,
and nothing is actually known about how to write these packets to a
device interface.
*/

void
FragmentIn(pip)
struct ip *pip;
{
    char *link;

    link = FindFragmentIn2(pip->ip_src);
    if (link != NULL_PTR)
        GetFragmentAddr(link, pip->ip_id, pip->ip_p, &(pip->ip_dst) );
}

void
FragmentOut(pip)
struct ip *pip;
{
    pip->ip_src = GetAliasAddress();
}








/* Outside World Access

        PacketAliasIn()
        PacketAliasOut()
*/

void
PacketAliasIn(pip)
struct ip *pip;
{
    int checksum_ok;

/* Verify initial checksum */
    if (IpChecksum(pip) == 0)
        checksum_ok = 1;
    else
        checksum_ok = 0;

    if ( (ntohs(pip->ip_off) & IP_OFFMASK) == 0 )
    {
        switch (pip->ip_p)
        {
            case IPPROTO_ICMP:
                IcmpAliasIn(pip);
                break;
            case IPPROTO_UDP:
                UdpAliasIn(pip);
                break;
            case IPPROTO_TCP:
                TcpAliasIn(pip);
                break;
        }
        if (ntohs(pip->ip_off) & IP_MF)
        {
            char *link;

            link = FindFragmentIn1(pip->ip_src);
            if (link != NULL_PTR)
                SetFragmentData(link, pip->ip_id, pip->ip_p, pip->ip_dst);
        }
    }
    else
    {
        FragmentIn(pip);
    }

/*  adjust IP checksum, if original is correct */
    if (checksum_ok == 1)
    {
        pip->ip_sum = 0;
        pip->ip_sum = IpChecksum(pip);
    }
}

void
PacketAliasOut(pip)
struct ip *pip;
{
    int checksum_ok;

    if (IpChecksum(pip) == 0)
	checksum_ok = 1;
    else
        checksum_ok = 0;

    if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0)
    {
        switch (pip->ip_p)
        {
            case IPPROTO_ICMP:
                IcmpAliasOut(pip);
                break;
            case IPPROTO_UDP:
                UdpAliasOut(pip);
                break;
            case IPPROTO_TCP:
                TcpAliasOut(pip);
                break;
        }
    }
    else
    {
            FragmentOut(pip);
    }

/* Adjust IP checksum, if original is correct */
    if (checksum_ok == 1)
    {
        pip->ip_sum = 0;
        pip->ip_sum = IpChecksum(pip);
    }
}
