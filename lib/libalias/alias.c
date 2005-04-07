/*-
 * Copyright (c) 2001 Charles Mott <cm@linktel.net>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
*/

#include <sys/types.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <stdio.h>

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
#define TFTP_PORT_NUMBER 69
#define PPTP_CONTROL_PORT_NUMBER 1723

static __inline int
twowords(void *p)
{
	uint8_t *c = p;

#if BYTE_ORDER == LITTLE_ENDIAN
	uint16_t s1 = ((uint16_t)c[1] << 8) + (uint16_t)c[0];
	uint16_t s2 = ((uint16_t)c[3] << 8) + (uint16_t)c[2];
#else
	uint16_t s1 = ((uint16_t)c[0] << 8) + (uint16_t)c[1];
	uint16_t s2 = ((uint16_t)c[2] << 8) + (uint16_t)c[3];
#endif
	return (s1 + s2);
}

/* TCP Handling Routines

    TcpMonitorIn()  -- These routines monitor TCP connections, and
    TcpMonitorOut()    delete a link when a connection is closed.

These routines look for SYN, FIN and RST flags to determine when TCP
connections open and close.  When a TCP connection closes, the data
structure containing packet aliasing information is deleted after
a timeout period.
*/

/* Local prototypes */
static void	TcpMonitorIn(struct ip *, struct alias_link *);

static void	TcpMonitorOut(struct ip *, struct alias_link *);


static void
TcpMonitorIn(struct ip *pip, struct alias_link *lnk)
{
	struct tcphdr *tc;

	tc = (struct tcphdr *)ip_next(pip);

	switch (GetStateIn(lnk)) {
	case ALIAS_TCP_STATE_NOT_CONNECTED:
		if (tc->th_flags & TH_RST)
			SetStateIn(lnk, ALIAS_TCP_STATE_DISCONNECTED);
		else if (tc->th_flags & TH_SYN)
			SetStateIn(lnk, ALIAS_TCP_STATE_CONNECTED);
		break;
	case ALIAS_TCP_STATE_CONNECTED:
		if (tc->th_flags & (TH_FIN | TH_RST))
			SetStateIn(lnk, ALIAS_TCP_STATE_DISCONNECTED);
		break;
	}
}

static void
TcpMonitorOut(struct ip *pip, struct alias_link *lnk)
{
	struct tcphdr *tc;

	tc = (struct tcphdr *)ip_next(pip);

	switch (GetStateOut(lnk)) {
	case ALIAS_TCP_STATE_NOT_CONNECTED:
		if (tc->th_flags & TH_RST)
			SetStateOut(lnk, ALIAS_TCP_STATE_DISCONNECTED);
		else if (tc->th_flags & TH_SYN)
			SetStateOut(lnk, ALIAS_TCP_STATE_CONNECTED);
		break;
	case ALIAS_TCP_STATE_CONNECTED:
		if (tc->th_flags & (TH_FIN | TH_RST))
			SetStateOut(lnk, ALIAS_TCP_STATE_DISCONNECTED);
		break;
	}
}





/* Protocol Specific Packet Aliasing Routines

    IcmpAliasIn(), IcmpAliasIn1(), IcmpAliasIn2()
    IcmpAliasOut(), IcmpAliasOut1(), IcmpAliasOut2()
    ProtoAliasIn(), ProtoAliasOut()
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
static int	IcmpAliasIn1(struct libalias *, struct ip *);
static int	IcmpAliasIn2(struct libalias *, struct ip *);
static int	IcmpAliasIn(struct libalias *, struct ip *);

static int	IcmpAliasOut1(struct libalias *, struct ip *, int create);
static int	IcmpAliasOut2(struct libalias *, struct ip *);
static int	IcmpAliasOut(struct libalias *, struct ip *, int create);

static int	ProtoAliasIn(struct libalias *, struct ip *);
static int	ProtoAliasOut(struct libalias *, struct ip *, int create);

static int	UdpAliasIn(struct libalias *, struct ip *);
static int	UdpAliasOut(struct libalias *, struct ip *, int create);

static int	TcpAliasIn(struct libalias *, struct ip *);
static int	TcpAliasOut(struct libalias *, struct ip *, int, int create);


static int
IcmpAliasIn1(struct libalias *la, struct ip *pip)
{
/*
    De-alias incoming echo and timestamp replies.
    Alias incoming echo and timestamp requests.
*/
	struct alias_link *lnk;
	struct icmp *ic;

	ic = (struct icmp *)ip_next(pip);

/* Get source address from ICMP data field and restore original data */
	lnk = FindIcmpIn(la, pip->ip_src, pip->ip_dst, ic->icmp_id, 1);
	if (lnk != NULL) {
		u_short original_id;
		int accumulate;

		original_id = GetOriginalPort(lnk);

/* Adjust ICMP checksum */
		accumulate = ic->icmp_id;
		accumulate -= original_id;
		ADJUST_CHECKSUM(accumulate, ic->icmp_cksum);

/* Put original sequence number back in */
		ic->icmp_id = original_id;

/* Put original address back into IP header */
		{
			struct in_addr original_address;

			original_address = GetOriginalAddress(lnk);
			DifferentialChecksum(&pip->ip_sum,
			    &original_address, &pip->ip_dst, 2);
			pip->ip_dst = original_address;
		}

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}

static int
IcmpAliasIn2(struct libalias *la, struct ip *pip)
{
/*
    Alias incoming ICMP error messages containing
    IP header and first 64 bits of datagram.
*/
	struct ip *ip;
	struct icmp *ic, *ic2;
	struct udphdr *ud;
	struct tcphdr *tc;
	struct alias_link *lnk;

	ic = (struct icmp *)ip_next(pip);
	ip = &ic->icmp_ip;

	ud = (struct udphdr *)ip_next(ip);
	tc = (struct tcphdr *)ip_next(ip);
	ic2 = (struct icmp *)ip_next(ip);

	if (ip->ip_p == IPPROTO_UDP)
		lnk = FindUdpTcpIn(la, ip->ip_dst, ip->ip_src,
		    ud->uh_dport, ud->uh_sport,
		    IPPROTO_UDP, 0);
	else if (ip->ip_p == IPPROTO_TCP)
		lnk = FindUdpTcpIn(la, ip->ip_dst, ip->ip_src,
		    tc->th_dport, tc->th_sport,
		    IPPROTO_TCP, 0);
	else if (ip->ip_p == IPPROTO_ICMP) {
		if (ic2->icmp_type == ICMP_ECHO || ic2->icmp_type == ICMP_TSTAMP)
			lnk = FindIcmpIn(la, ip->ip_dst, ip->ip_src, ic2->icmp_id, 0);
		else
			lnk = NULL;
	} else
		lnk = NULL;

	if (lnk != NULL) {
		if (ip->ip_p == IPPROTO_UDP || ip->ip_p == IPPROTO_TCP) {
			int accumulate, accumulate2;
			struct in_addr original_address;
			u_short original_port;

			original_address = GetOriginalAddress(lnk);
			original_port = GetOriginalPort(lnk);

/* Adjust ICMP checksum */
			accumulate = twowords(&ip->ip_src);
			accumulate -= twowords(&original_address);
			accumulate += ud->uh_sport;
			accumulate -= original_port;
			accumulate2 = accumulate;
			accumulate2 += ip->ip_sum;
			ADJUST_CHECKSUM(accumulate, ip->ip_sum);
			accumulate2 -= ip->ip_sum;
			ADJUST_CHECKSUM(accumulate2, ic->icmp_cksum);

/* Un-alias address in IP header */
			DifferentialChecksum(&pip->ip_sum,
			    &original_address, &pip->ip_dst, 2);
			pip->ip_dst = original_address;

/* Un-alias address and port number of original IP packet
fragment contained in ICMP data section */
			ip->ip_src = original_address;
			ud->uh_sport = original_port;
		} else if (ip->ip_p == IPPROTO_ICMP) {
			int accumulate, accumulate2;
			struct in_addr original_address;
			u_short original_id;

			original_address = GetOriginalAddress(lnk);
			original_id = GetOriginalPort(lnk);

/* Adjust ICMP checksum */
			accumulate = twowords(&ip->ip_src);
			accumulate -= twowords(&original_address);
			accumulate += ic2->icmp_id;
			accumulate -= original_id;
			accumulate2 = accumulate;
			accumulate2 += ip->ip_sum;
			ADJUST_CHECKSUM(accumulate, ip->ip_sum);
			accumulate2 -= ip->ip_sum;
			ADJUST_CHECKSUM(accumulate2, ic->icmp_cksum);

/* Un-alias address in IP header */
			DifferentialChecksum(&pip->ip_sum,
			    &original_address, &pip->ip_dst, 2);
			pip->ip_dst = original_address;

/* Un-alias address of original IP packet and sequence number of
   embedded ICMP datagram */
			ip->ip_src = original_address;
			ic2->icmp_id = original_id;
		}
		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}


static int
IcmpAliasIn(struct libalias *la, struct ip *pip)
{
	int iresult;
	struct icmp *ic;

/* Return if proxy-only mode is enabled */
	if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY)
		return (PKT_ALIAS_OK);

	ic = (struct icmp *)ip_next(pip);

	iresult = PKT_ALIAS_IGNORED;
	switch (ic->icmp_type) {
	case ICMP_ECHOREPLY:
	case ICMP_TSTAMPREPLY:
		if (ic->icmp_code == 0) {
			iresult = IcmpAliasIn1(la, pip);
		}
		break;
	case ICMP_UNREACH:
	case ICMP_SOURCEQUENCH:
	case ICMP_TIMXCEED:
	case ICMP_PARAMPROB:
		iresult = IcmpAliasIn2(la, pip);
		break;
	case ICMP_ECHO:
	case ICMP_TSTAMP:
		iresult = IcmpAliasIn1(la, pip);
		break;
	}
	return (iresult);
}


static int
IcmpAliasOut1(struct libalias *la, struct ip *pip, int create)
{
/*
    Alias outgoing echo and timestamp requests.
    De-alias outgoing echo and timestamp replies.
*/
	struct alias_link *lnk;
	struct icmp *ic;

	ic = (struct icmp *)ip_next(pip);

/* Save overwritten data for when echo packet returns */
	lnk = FindIcmpOut(la, pip->ip_src, pip->ip_dst, ic->icmp_id, create);
	if (lnk != NULL) {
		u_short alias_id;
		int accumulate;

		alias_id = GetAliasPort(lnk);

/* Since data field is being modified, adjust ICMP checksum */
		accumulate = ic->icmp_id;
		accumulate -= alias_id;
		ADJUST_CHECKSUM(accumulate, ic->icmp_cksum);

/* Alias sequence number */
		ic->icmp_id = alias_id;

/* Change source address */
		{
			struct in_addr alias_address;

			alias_address = GetAliasAddress(lnk);
			DifferentialChecksum(&pip->ip_sum,
			    &alias_address, &pip->ip_src, 2);
			pip->ip_src = alias_address;
		}

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}


static int
IcmpAliasOut2(struct libalias *la, struct ip *pip)
{
/*
    Alias outgoing ICMP error messages containing
    IP header and first 64 bits of datagram.
*/
	struct ip *ip;
	struct icmp *ic, *ic2;
	struct udphdr *ud;
	struct tcphdr *tc;
	struct alias_link *lnk;

	ic = (struct icmp *)ip_next(pip);
	ip = &ic->icmp_ip;

	ud = (struct udphdr *)ip_next(ip);
	tc = (struct tcphdr *)ip_next(ip);
	ic2 = (struct icmp *)ip_next(ip);

	if (ip->ip_p == IPPROTO_UDP)
		lnk = FindUdpTcpOut(la, ip->ip_dst, ip->ip_src,
		    ud->uh_dport, ud->uh_sport,
		    IPPROTO_UDP, 0);
	else if (ip->ip_p == IPPROTO_TCP)
		lnk = FindUdpTcpOut(la, ip->ip_dst, ip->ip_src,
		    tc->th_dport, tc->th_sport,
		    IPPROTO_TCP, 0);
	else if (ip->ip_p == IPPROTO_ICMP) {
		if (ic2->icmp_type == ICMP_ECHO || ic2->icmp_type == ICMP_TSTAMP)
			lnk = FindIcmpOut(la, ip->ip_dst, ip->ip_src, ic2->icmp_id, 0);
		else
			lnk = NULL;
	} else
		lnk = NULL;

	if (lnk != NULL) {
		if (ip->ip_p == IPPROTO_UDP || ip->ip_p == IPPROTO_TCP) {
			int accumulate;
			struct in_addr alias_address;
			u_short alias_port;

			alias_address = GetAliasAddress(lnk);
			alias_port = GetAliasPort(lnk);

/* Adjust ICMP checksum */
			accumulate = twowords(&ip->ip_dst);
			accumulate -= twowords(&alias_address);
			accumulate += ud->uh_dport;
			accumulate -= alias_port;
			ADJUST_CHECKSUM(accumulate, ic->icmp_cksum);

/*
 * Alias address in IP header if it comes from the host
 * the original TCP/UDP packet was destined for.
 */
			if (pip->ip_src.s_addr == ip->ip_dst.s_addr) {
				DifferentialChecksum(&pip->ip_sum,
				    &alias_address, &pip->ip_src, 2);
				pip->ip_src = alias_address;
			}
/* Alias address and port number of original IP packet
fragment contained in ICMP data section */
			ip->ip_dst = alias_address;
			ud->uh_dport = alias_port;
		} else if (ip->ip_p == IPPROTO_ICMP) {
			int accumulate;
			struct in_addr alias_address;
			u_short alias_id;

			alias_address = GetAliasAddress(lnk);
			alias_id = GetAliasPort(lnk);

/* Adjust ICMP checksum */
			accumulate = twowords(&ip->ip_dst);
			accumulate -= twowords(&alias_address);
			accumulate += ic2->icmp_id;
			accumulate -= alias_id;
			ADJUST_CHECKSUM(accumulate, ic->icmp_cksum);

/*
 * Alias address in IP header if it comes from the host
 * the original ICMP message was destined for.
 */
			if (pip->ip_src.s_addr == ip->ip_dst.s_addr) {
				DifferentialChecksum(&pip->ip_sum,
				    &alias_address, &pip->ip_src, 2);
				pip->ip_src = alias_address;
			}
/* Alias address of original IP packet and sequence number of
   embedded ICMP datagram */
			ip->ip_dst = alias_address;
			ic2->icmp_id = alias_id;
		}
		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}


static int
IcmpAliasOut(struct libalias *la, struct ip *pip, int create)
{
	int iresult;
	struct icmp *ic;

	(void)create;

/* Return if proxy-only mode is enabled */
	if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY)
		return (PKT_ALIAS_OK);

	ic = (struct icmp *)ip_next(pip);

	iresult = PKT_ALIAS_IGNORED;
	switch (ic->icmp_type) {
	case ICMP_ECHO:
	case ICMP_TSTAMP:
		if (ic->icmp_code == 0) {
			iresult = IcmpAliasOut1(la, pip, create);
		}
		break;
	case ICMP_UNREACH:
	case ICMP_SOURCEQUENCH:
	case ICMP_TIMXCEED:
	case ICMP_PARAMPROB:
		iresult = IcmpAliasOut2(la, pip);
		break;
	case ICMP_ECHOREPLY:
	case ICMP_TSTAMPREPLY:
		iresult = IcmpAliasOut1(la, pip, create);
	}
	return (iresult);
}



static int
ProtoAliasIn(struct libalias *la, struct ip *pip)
{
/*
  Handle incoming IP packets. The
  only thing which is done in this case is to alias
  the dest IP address of the packet to our inside
  machine.
*/
	struct alias_link *lnk;

/* Return if proxy-only mode is enabled */
	if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY)
		return (PKT_ALIAS_OK);

	lnk = FindProtoIn(la, pip->ip_src, pip->ip_dst, pip->ip_p);
	if (lnk != NULL) {
		struct in_addr original_address;

		original_address = GetOriginalAddress(lnk);

/* Restore original IP address */
		DifferentialChecksum(&pip->ip_sum,
		    &original_address, &pip->ip_dst, 2);
		pip->ip_dst = original_address;

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}


static int
ProtoAliasOut(struct libalias *la, struct ip *pip, int create)
{
/*
  Handle outgoing IP packets. The
  only thing which is done in this case is to alias
  the source IP address of the packet.
*/
	struct alias_link *lnk;

	(void)create;

/* Return if proxy-only mode is enabled */
	if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY)
		return (PKT_ALIAS_OK);

	lnk = FindProtoOut(la, pip->ip_src, pip->ip_dst, pip->ip_p);
	if (lnk != NULL) {
		struct in_addr alias_address;

		alias_address = GetAliasAddress(lnk);

/* Change source address */
		DifferentialChecksum(&pip->ip_sum,
		    &alias_address, &pip->ip_src, 2);
		pip->ip_src = alias_address;

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}


static int
UdpAliasIn(struct libalias *la, struct ip *pip)
{
	struct udphdr *ud;
	struct alias_link *lnk;

/* Return if proxy-only mode is enabled */
	if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY)
		return (PKT_ALIAS_OK);

	ud = (struct udphdr *)ip_next(pip);

	lnk = FindUdpTcpIn(la, pip->ip_src, pip->ip_dst,
	    ud->uh_sport, ud->uh_dport,
	    IPPROTO_UDP, 1);
	if (lnk != NULL) {
		struct in_addr alias_address;
		struct in_addr original_address;
		u_short alias_port;
		int accumulate;
		int r = 0;

		alias_address = GetAliasAddress(lnk);
		original_address = GetOriginalAddress(lnk);
		alias_port = ud->uh_dport;
		ud->uh_dport = GetOriginalPort(lnk);

/* Special processing for IP encoding protocols */
		if (ntohs(ud->uh_dport) == CUSEEME_PORT_NUMBER)
			AliasHandleCUSeeMeIn(la, pip, original_address);
/* If NETBIOS Datagram, It should be alias address in UDP Data, too */
		else if (ntohs(ud->uh_dport) == NETBIOS_DGM_PORT_NUMBER
		    || ntohs(ud->uh_sport) == NETBIOS_DGM_PORT_NUMBER)
			r = AliasHandleUdpNbt(la, pip, lnk, &original_address, ud->uh_dport);
		else if (ntohs(ud->uh_dport) == NETBIOS_NS_PORT_NUMBER
		    || ntohs(ud->uh_sport) == NETBIOS_NS_PORT_NUMBER)
			r = AliasHandleUdpNbtNS(la, pip, lnk, &alias_address, &alias_port,
			    &original_address, &ud->uh_dport);

/* If UDP checksum is not zero, then adjust since destination port */
/* is being unaliased and destination address is being altered.    */
		if (ud->uh_sum != 0) {
			accumulate = alias_port;
			accumulate -= ud->uh_dport;
			accumulate += twowords(&alias_address);
			accumulate -= twowords(&original_address);
			ADJUST_CHECKSUM(accumulate, ud->uh_sum);
		}
/* Restore original IP address */
		DifferentialChecksum(&pip->ip_sum,
		    &original_address, &pip->ip_dst, 2);
		pip->ip_dst = original_address;

		/*
		 * If we cannot figure out the packet, ignore it.
		 */
		if (r < 0)
			return (PKT_ALIAS_IGNORED);
		else
			return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}

static int
UdpAliasOut(struct libalias *la, struct ip *pip, int create)
{
	struct udphdr *ud;
	struct alias_link *lnk;

/* Return if proxy-only mode is enabled */
	if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY)
		return (PKT_ALIAS_OK);

	ud = (struct udphdr *)ip_next(pip);

	lnk = FindUdpTcpOut(la, pip->ip_src, pip->ip_dst,
	    ud->uh_sport, ud->uh_dport,
	    IPPROTO_UDP, create);
	if (lnk != NULL) {
		u_short alias_port;
		struct in_addr alias_address;

		alias_address = GetAliasAddress(lnk);
		alias_port = GetAliasPort(lnk);

/* Special processing for IP encoding protocols */
		if (ntohs(ud->uh_dport) == CUSEEME_PORT_NUMBER)
			AliasHandleCUSeeMeOut(la, pip, lnk);
/* If NETBIOS Datagram, It should be alias address in UDP Data, too */
		else if (ntohs(ud->uh_dport) == NETBIOS_DGM_PORT_NUMBER
		    || ntohs(ud->uh_sport) == NETBIOS_DGM_PORT_NUMBER)
			AliasHandleUdpNbt(la, pip, lnk, &alias_address, alias_port);
		else if (ntohs(ud->uh_dport) == NETBIOS_NS_PORT_NUMBER
		    || ntohs(ud->uh_sport) == NETBIOS_NS_PORT_NUMBER)
			AliasHandleUdpNbtNS(la, pip, lnk, &pip->ip_src, &ud->uh_sport,
			    &alias_address, &alias_port);
/*
 * We don't know in advance what TID the TFTP server will choose,
 * so we create a wilcard link (destination port is unspecified)
 * that will match any TID from a given destination.
 */
		else if (ntohs(ud->uh_dport) == TFTP_PORT_NUMBER)
			FindRtspOut(la, pip->ip_src, pip->ip_dst,
			    ud->uh_sport, alias_port, IPPROTO_UDP);

/* If UDP checksum is not zero, adjust since source port is */
/* being aliased and source address is being altered        */
		if (ud->uh_sum != 0) {
			int accumulate;

			accumulate = ud->uh_sport;
			accumulate -= alias_port;
			accumulate += twowords(&pip->ip_src);
			accumulate -= twowords(&alias_address);
			ADJUST_CHECKSUM(accumulate, ud->uh_sum);
		}
/* Put alias port in UDP header */
		ud->uh_sport = alias_port;

/* Change source address */
		DifferentialChecksum(&pip->ip_sum,
		    &alias_address, &pip->ip_src, 2);
		pip->ip_src = alias_address;

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}



static int
TcpAliasIn(struct libalias *la, struct ip *pip)
{
	struct tcphdr *tc;
	struct alias_link *lnk;

	tc = (struct tcphdr *)ip_next(pip);

	lnk = FindUdpTcpIn(la, pip->ip_src, pip->ip_dst,
	    tc->th_sport, tc->th_dport,
	    IPPROTO_TCP,
	    !(la->packetAliasMode & PKT_ALIAS_PROXY_ONLY));
	if (lnk != NULL) {
		struct in_addr alias_address;
		struct in_addr original_address;
		struct in_addr proxy_address;
		u_short alias_port;
		u_short proxy_port;
		int accumulate;

/* Special processing for IP encoding protocols */
		if (ntohs(tc->th_dport) == PPTP_CONTROL_PORT_NUMBER
		    || ntohs(tc->th_sport) == PPTP_CONTROL_PORT_NUMBER)
			AliasHandlePptpIn(la, pip, lnk);
		else if (la->skinnyPort != 0 && (ntohs(tc->th_dport) == la->skinnyPort
		    || ntohs(tc->th_sport) == la->skinnyPort))
			AliasHandleSkinny(la, pip, lnk);

		alias_address = GetAliasAddress(lnk);
		original_address = GetOriginalAddress(lnk);
		proxy_address = GetProxyAddress(lnk);
		alias_port = tc->th_dport;
		tc->th_dport = GetOriginalPort(lnk);
		proxy_port = GetProxyPort(lnk);

/* Adjust TCP checksum since destination port is being unaliased */
/* and destination port is being altered.                        */
		accumulate = alias_port;
		accumulate -= tc->th_dport;
		accumulate += twowords(&alias_address);
		accumulate -= twowords(&original_address);

/* If this is a proxy, then modify the TCP source port and
   checksum accumulation */
		if (proxy_port != 0) {
			accumulate += tc->th_sport;
			tc->th_sport = proxy_port;
			accumulate -= tc->th_sport;
			accumulate += twowords(&pip->ip_src);
			accumulate -= twowords(&proxy_address);
		}
/* See if ACK number needs to be modified */
		if (GetAckModified(lnk) == 1) {
			int delta;

			delta = GetDeltaAckIn(pip, lnk);
			if (delta != 0) {
				accumulate += twowords(&tc->th_ack);
				tc->th_ack = htonl(ntohl(tc->th_ack) - delta);
				accumulate -= twowords(&tc->th_ack);
			}
		}
		ADJUST_CHECKSUM(accumulate, tc->th_sum);

/* Restore original IP address */
		accumulate = twowords(&pip->ip_dst);
		pip->ip_dst = original_address;
		accumulate -= twowords(&pip->ip_dst);

/* If this is a transparent proxy packet, then modify the source
   address */
		if (proxy_address.s_addr != 0) {
			accumulate += twowords(&pip->ip_src);
			pip->ip_src = proxy_address;
			accumulate -= twowords(&pip->ip_src);
		}
		ADJUST_CHECKSUM(accumulate, pip->ip_sum);

/* Monitor TCP connection state */
		TcpMonitorIn(pip, lnk);

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
}

static int
TcpAliasOut(struct libalias *la, struct ip *pip, int maxpacketsize, int create)
{
	int proxy_type;
	u_short dest_port;
	u_short proxy_server_port;
	struct in_addr dest_address;
	struct in_addr proxy_server_address;
	struct tcphdr *tc;
	struct alias_link *lnk;

	tc = (struct tcphdr *)ip_next(pip);

	proxy_type = ProxyCheck(la, pip, &proxy_server_address, &proxy_server_port);

	if (proxy_type == 0 && (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY))
		return (PKT_ALIAS_OK);

/* If this is a transparent proxy, save original destination,
   then alter the destination and adjust checksums */
	dest_port = tc->th_dport;
	dest_address = pip->ip_dst;
	if (proxy_type != 0) {
		int accumulate;

		accumulate = tc->th_dport;
		tc->th_dport = proxy_server_port;
		accumulate -= tc->th_dport;
		accumulate += twowords(&pip->ip_dst);
		accumulate -= twowords(&proxy_server_address);
		ADJUST_CHECKSUM(accumulate, tc->th_sum);

		accumulate = twowords(&pip->ip_dst);
		pip->ip_dst = proxy_server_address;
		accumulate -= twowords(&pip->ip_dst);
		ADJUST_CHECKSUM(accumulate, pip->ip_sum);
	}
	lnk = FindUdpTcpOut(la, pip->ip_src, pip->ip_dst,
	    tc->th_sport, tc->th_dport,
	    IPPROTO_TCP, create);
	if (lnk == NULL)
		return (PKT_ALIAS_IGNORED);
	if (lnk != NULL) {
		u_short alias_port;
		struct in_addr alias_address;
		int accumulate;

/* Save original destination address, if this is a proxy packet.
   Also modify packet to include destination encoding.  This may
   change the size of IP header. */
		if (proxy_type != 0) {
			SetProxyPort(lnk, dest_port);
			SetProxyAddress(lnk, dest_address);
			ProxyModify(la, lnk, pip, maxpacketsize, proxy_type);
			tc = (struct tcphdr *)ip_next(pip);
		}
/* Get alias address and port */
		alias_port = GetAliasPort(lnk);
		alias_address = GetAliasAddress(lnk);

/* Monitor TCP connection state */
		TcpMonitorOut(pip, lnk);

/* Special processing for IP encoding protocols */
		if (ntohs(tc->th_dport) == FTP_CONTROL_PORT_NUMBER
		    || ntohs(tc->th_sport) == FTP_CONTROL_PORT_NUMBER)
			AliasHandleFtpOut(la, pip, lnk, maxpacketsize);
		else if (ntohs(tc->th_dport) == IRC_CONTROL_PORT_NUMBER_1
		    || ntohs(tc->th_dport) == IRC_CONTROL_PORT_NUMBER_2)
			AliasHandleIrcOut(la, pip, lnk, maxpacketsize);
		else if (ntohs(tc->th_dport) == RTSP_CONTROL_PORT_NUMBER_1
			    || ntohs(tc->th_sport) == RTSP_CONTROL_PORT_NUMBER_1
			    || ntohs(tc->th_dport) == RTSP_CONTROL_PORT_NUMBER_2
		    || ntohs(tc->th_sport) == RTSP_CONTROL_PORT_NUMBER_2)
			AliasHandleRtspOut(la, pip, lnk, maxpacketsize);
		else if (ntohs(tc->th_dport) == PPTP_CONTROL_PORT_NUMBER
		    || ntohs(tc->th_sport) == PPTP_CONTROL_PORT_NUMBER)
			AliasHandlePptpOut(la, pip, lnk);
		else if (la->skinnyPort != 0 && (ntohs(tc->th_sport) == la->skinnyPort
		    || ntohs(tc->th_dport) == la->skinnyPort))
			AliasHandleSkinny(la, pip, lnk);

/* Adjust TCP checksum since source port is being aliased */
/* and source address is being altered                    */
		accumulate = tc->th_sport;
		tc->th_sport = alias_port;
		accumulate -= tc->th_sport;
		accumulate += twowords(&pip->ip_src);
		accumulate -= twowords(&alias_address);

/* Modify sequence number if necessary */
		if (GetAckModified(lnk) == 1) {
			int delta;

			delta = GetDeltaSeqOut(pip, lnk);
			if (delta != 0) {
				accumulate += twowords(&tc->th_seq);
				tc->th_seq = htonl(ntohl(tc->th_seq) + delta);
				accumulate -= twowords(&tc->th_seq);
			}
		}
		ADJUST_CHECKSUM(accumulate, tc->th_sum);

/* Change source address */
		accumulate = twowords(&pip->ip_src);
		pip->ip_src = alias_address;
		accumulate -= twowords(&pip->ip_src);
		ADJUST_CHECKSUM(accumulate, pip->ip_sum);

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_IGNORED);
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
static int	FragmentIn(struct libalias *, struct ip *);
static int	FragmentOut(struct libalias *, struct ip *);


static int
FragmentIn(struct libalias *la, struct ip *pip)
{
	struct alias_link *lnk;

	lnk = FindFragmentIn2(la, pip->ip_src, pip->ip_dst, pip->ip_id);
	if (lnk != NULL) {
		struct in_addr original_address;

		GetFragmentAddr(lnk, &original_address);
		DifferentialChecksum(&pip->ip_sum,
		    &original_address, &pip->ip_dst, 2);
		pip->ip_dst = original_address;

		return (PKT_ALIAS_OK);
	}
	return (PKT_ALIAS_UNRESOLVED_FRAGMENT);
}


static int
FragmentOut(struct libalias *la, struct ip *pip)
{
	struct in_addr alias_address;

	alias_address = FindAliasAddress(la, pip->ip_src);
	DifferentialChecksum(&pip->ip_sum,
	    &alias_address, &pip->ip_src, 2);
	pip->ip_src = alias_address;

	return (PKT_ALIAS_OK);
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
LibAliasSaveFragment(struct libalias *la, char *ptr)
{
	int iresult;
	struct alias_link *lnk;
	struct ip *pip;

	pip = (struct ip *)ptr;
	lnk = AddFragmentPtrLink(la, pip->ip_src, pip->ip_id);
	iresult = PKT_ALIAS_ERROR;
	if (lnk != NULL) {
		SetFragmentPtr(lnk, ptr);
		iresult = PKT_ALIAS_OK;
	}
	return (iresult);
}


char           *
LibAliasGetFragment(struct libalias *la, char *ptr)
{
	struct alias_link *lnk;
	char *fptr;
	struct ip *pip;

	pip = (struct ip *)ptr;
	lnk = FindFragmentPtr(la, pip->ip_src, pip->ip_id);
	if (lnk != NULL) {
		GetFragmentPtr(lnk, &fptr);
		SetFragmentPtr(lnk, NULL);
		SetExpire(lnk, 0);	/* Deletes link */

		return (fptr);
	} else {
		return (NULL);
	}
}


void
LibAliasFragmentIn(struct libalias *la, char *ptr,	/* Points to correctly
							 * de-aliased header
							 * fragment */
    char *ptr_fragment		/* Points to fragment which must be
				 * de-aliased   */
)
{
	struct ip *pip;
	struct ip *fpip;

	(void)la;
	pip = (struct ip *)ptr;
	fpip = (struct ip *)ptr_fragment;

	DifferentialChecksum(&fpip->ip_sum,
	    &pip->ip_dst, &fpip->ip_dst, 2);
	fpip->ip_dst = pip->ip_dst;
}


int
LibAliasIn(struct libalias *la, char *ptr, int maxpacketsize)
{
	struct in_addr alias_addr;
	struct ip *pip;
	int iresult;

	if (la->packetAliasMode & PKT_ALIAS_REVERSE) {
		la->packetAliasMode &= ~PKT_ALIAS_REVERSE;
		iresult = LibAliasOut(la, ptr, maxpacketsize);
		la->packetAliasMode |= PKT_ALIAS_REVERSE;
		return (iresult);
	}
	HouseKeeping(la);
	ClearCheckNewLink(la);
	pip = (struct ip *)ptr;
	alias_addr = pip->ip_dst;

	/* Defense against mangled packets */
	if (ntohs(pip->ip_len) > maxpacketsize
	    || (pip->ip_hl << 2) > maxpacketsize)
		return (PKT_ALIAS_IGNORED);

	iresult = PKT_ALIAS_IGNORED;
	if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0) {
		switch (pip->ip_p) {
		case IPPROTO_ICMP:
			iresult = IcmpAliasIn(la, pip);
			break;
		case IPPROTO_UDP:
			iresult = UdpAliasIn(la, pip);
			break;
		case IPPROTO_TCP:
			iresult = TcpAliasIn(la, pip);
			break;
		case IPPROTO_GRE:
			if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY ||
			    AliasHandlePptpGreIn(la, pip) == 0)
				iresult = PKT_ALIAS_OK;
			else
				iresult = ProtoAliasIn(la, pip);
			break;
		default:
			iresult = ProtoAliasIn(la, pip);
			break;
		}

		if (ntohs(pip->ip_off) & IP_MF) {
			struct alias_link *lnk;

			lnk = FindFragmentIn1(la, pip->ip_src, alias_addr, pip->ip_id);
			if (lnk != NULL) {
				iresult = PKT_ALIAS_FOUND_HEADER_FRAGMENT;
				SetFragmentAddr(lnk, pip->ip_dst);
			} else {
				iresult = PKT_ALIAS_ERROR;
			}
		}
	} else {
		iresult = FragmentIn(la, pip);
	}

	return (iresult);
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
LibAliasOut(struct libalias *la, char *ptr,	/* valid IP packet */
    int maxpacketsize		/* How much the packet data may grow (FTP
				 * and IRC inline changes) */
)
{
	return (LibAliasOutTry(la, ptr, maxpacketsize, 1));
}

int
LibAliasOutTry(struct libalias *la, char *ptr,	/* valid IP packet */
    int maxpacketsize,		/* How much the packet data may grow (FTP
				 * and IRC inline changes) */
    int create			/* Create new entries ? */
)
{
	int iresult;
	struct in_addr addr_save;
	struct ip *pip;

	if (la->packetAliasMode & PKT_ALIAS_REVERSE) {
		la->packetAliasMode &= ~PKT_ALIAS_REVERSE;
		iresult = LibAliasIn(la, ptr, maxpacketsize);
		la->packetAliasMode |= PKT_ALIAS_REVERSE;
		return (iresult);
	}
	HouseKeeping(la);
	ClearCheckNewLink(la);
	pip = (struct ip *)ptr;

	/* Defense against mangled packets */
	if (ntohs(pip->ip_len) > maxpacketsize
	    || (pip->ip_hl << 2) > maxpacketsize)
		return (PKT_ALIAS_IGNORED);

	addr_save = GetDefaultAliasAddress(la);
	if (la->packetAliasMode & PKT_ALIAS_UNREGISTERED_ONLY) {
		u_long addr;
		int iclass;

		iclass = 0;
		addr = ntohl(pip->ip_src.s_addr);
		if (addr >= UNREG_ADDR_C_LOWER && addr <= UNREG_ADDR_C_UPPER)
			iclass = 3;
		else if (addr >= UNREG_ADDR_B_LOWER && addr <= UNREG_ADDR_B_UPPER)
			iclass = 2;
		else if (addr >= UNREG_ADDR_A_LOWER && addr <= UNREG_ADDR_A_UPPER)
			iclass = 1;

		if (iclass == 0) {
			SetDefaultAliasAddress(la, pip->ip_src);
		}
	} else if (la->packetAliasMode & PKT_ALIAS_PROXY_ONLY) {
		SetDefaultAliasAddress(la, pip->ip_src);
	}
	iresult = PKT_ALIAS_IGNORED;
	if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0) {
		switch (pip->ip_p) {
		case IPPROTO_ICMP:
			iresult = IcmpAliasOut(la, pip, create);
			break;
		case IPPROTO_UDP:
			iresult = UdpAliasOut(la, pip, create);
			break;
			case IPPROTO_TCP:
			iresult = TcpAliasOut(la, pip, maxpacketsize, create);
			break;
		case IPPROTO_GRE:
			if (AliasHandlePptpGreOut(la, pip) == 0)
				iresult = PKT_ALIAS_OK;
			else
				iresult = ProtoAliasOut(la, pip, create);
			break;
		default:
			iresult = ProtoAliasOut(la, pip, create);
			break;
		}
	} else {
		iresult = FragmentOut(la, pip);
	}

	SetDefaultAliasAddress(la, addr_save);
	return (iresult);
}

int
LibAliasUnaliasOut(struct libalias *la, char *ptr,	/* valid IP packet */
    int maxpacketsize		/* for error checking */
)
{
	struct ip *pip;
	struct icmp *ic;
	struct udphdr *ud;
	struct tcphdr *tc;
	struct alias_link *lnk;
	int iresult = PKT_ALIAS_IGNORED;

	pip = (struct ip *)ptr;

	/* Defense against mangled packets */
	if (ntohs(pip->ip_len) > maxpacketsize
	    || (pip->ip_hl << 2) > maxpacketsize)
		return (iresult);

	ud = (struct udphdr *)ip_next(pip);
	tc = (struct tcphdr *)ip_next(pip);
	ic = (struct icmp *)ip_next(pip);

	/* Find a link */
	if (pip->ip_p == IPPROTO_UDP)
		lnk = FindUdpTcpIn(la, pip->ip_dst, pip->ip_src,
		    ud->uh_dport, ud->uh_sport,
		    IPPROTO_UDP, 0);
	else if (pip->ip_p == IPPROTO_TCP)
		lnk = FindUdpTcpIn(la, pip->ip_dst, pip->ip_src,
		    tc->th_dport, tc->th_sport,
		    IPPROTO_TCP, 0);
	else if (pip->ip_p == IPPROTO_ICMP)
		lnk = FindIcmpIn(la, pip->ip_dst, pip->ip_src, ic->icmp_id, 0);
	else
		lnk = NULL;

	/* Change it from an aliased packet to an unaliased packet */
	if (lnk != NULL) {
		if (pip->ip_p == IPPROTO_UDP || pip->ip_p == IPPROTO_TCP) {
			int accumulate;
			struct in_addr original_address;
			u_short original_port;

			original_address = GetOriginalAddress(lnk);
			original_port = GetOriginalPort(lnk);

			/* Adjust TCP/UDP checksum */
			accumulate = twowords(&pip->ip_src);
			accumulate -= twowords(&original_address);

			if (pip->ip_p == IPPROTO_UDP) {
				accumulate += ud->uh_sport;
				accumulate -= original_port;
				ADJUST_CHECKSUM(accumulate, ud->uh_sum);
			} else {
				accumulate += tc->th_sport;
				accumulate -= original_port;
				ADJUST_CHECKSUM(accumulate, tc->th_sum);
			}

			/* Adjust IP checksum */
			DifferentialChecksum(&pip->ip_sum,
			    &original_address, &pip->ip_src, 2);

			/* Un-alias source address and port number */
			pip->ip_src = original_address;
			if (pip->ip_p == IPPROTO_UDP)
				ud->uh_sport = original_port;
			else
				tc->th_sport = original_port;

			iresult = PKT_ALIAS_OK;

		} else if (pip->ip_p == IPPROTO_ICMP) {

			int accumulate;
			struct in_addr original_address;
			u_short original_id;

			original_address = GetOriginalAddress(lnk);
			original_id = GetOriginalPort(lnk);

			/* Adjust ICMP checksum */
			accumulate = twowords(&pip->ip_src);
			accumulate -= twowords(&original_address);
			accumulate += ic->icmp_id;
			accumulate -= original_id;
			ADJUST_CHECKSUM(accumulate, ic->icmp_cksum);

			/* Adjust IP checksum */
			DifferentialChecksum(&pip->ip_sum,
			    &original_address, &pip->ip_src, 2);

			/* Un-alias source address and port number */
			pip->ip_src = original_address;
			ic->icmp_id = original_id;

			iresult = PKT_ALIAS_OK;
		}
	}
	return (iresult);

}
