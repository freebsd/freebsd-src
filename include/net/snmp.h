/*
 *
 *		SNMP MIB entries for the IP subsystem.
 *		
 *		Alan Cox <gw4pts@gw4pts.ampr.org>
 *
 *		We don't chose to implement SNMP in the kernel (this would
 *		be silly as SNMP is a pain in the backside in places). We do
 *		however need to collect the MIB statistics and export them
 *		out of /proc (eventually)
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *		$Id: snmp.h,v 1.19 2001/06/14 13:40:46 davem Exp $
 *
 */
 
#ifndef _SNMP_H
#define _SNMP_H

#include <linux/cache.h>
 
/*
 *	We use all unsigned longs. Linux will soon be so reliable that even these
 *	will rapidly get too small 8-). Seriously consider the IpInReceives count
 *	on the 20Gb/s + networks people expect in a few years time!
 */

/* 
 * The rule for padding: 
 * Best is power of two because then the right structure can be found by a simple
 * shift. The structure should be always cache line aligned.
 * gcc needs n=alignto(cachelinesize, popcnt(sizeof(bla_mib))) shift/add instructions
 * to emulate multiply in case it is not power-of-two. Currently n is always <=3 for
 * all sizes so simple cache line alignment is enough. 
 * 
 * The best solution would be a global CPU local area , especially on 64 and 128byte 
 * cacheline machine it makes a *lot* of sense -AK
 */ 

 
/*
 * RFC 1213:  MIB-II
 * RFC 2011 (updates 1213):  SNMPv2-MIB-IP
 * RFC 2863:  Interfaces Group MIB
 */
struct ip_mib
{
 	unsigned long	IpInReceives;
 	unsigned long	IpInHdrErrors;
 	unsigned long	IpInAddrErrors;
 	unsigned long	IpForwDatagrams;
 	unsigned long	IpInUnknownProtos;
 	unsigned long	IpInDiscards;
 	unsigned long	IpInDelivers;
 	unsigned long	IpOutRequests;
 	unsigned long	IpOutDiscards;
 	unsigned long	IpOutNoRoutes;
 	unsigned long	IpReasmTimeout;
 	unsigned long	IpReasmReqds;
 	unsigned long	IpReasmOKs;
 	unsigned long	IpReasmFails;
 	unsigned long	IpFragOKs;
 	unsigned long	IpFragFails;
 	unsigned long	IpFragCreates;
	unsigned long   __pad[0]; 
} ____cacheline_aligned;
 
/*
 * RFC 2465:  IPv6 MIB: General Group
 */
struct ipv6_mib
{
	unsigned long	Ip6InReceives;
 	unsigned long	Ip6InHdrErrors;
 	unsigned long	Ip6InTooBigErrors;
 	unsigned long	Ip6InNoRoutes;
 	unsigned long	Ip6InAddrErrors;
 	unsigned long	Ip6InUnknownProtos;
 	unsigned long	Ip6InTruncatedPkts;
 	unsigned long	Ip6InDiscards;
 	unsigned long	Ip6InDelivers;
 	unsigned long	Ip6OutForwDatagrams;
 	unsigned long	Ip6OutRequests;
 	unsigned long	Ip6OutDiscards;
 	unsigned long	Ip6OutNoRoutes;
 	unsigned long	Ip6ReasmTimeout;
 	unsigned long	Ip6ReasmReqds;
 	unsigned long	Ip6ReasmOKs;
 	unsigned long	Ip6ReasmFails;
 	unsigned long	Ip6FragOKs;
 	unsigned long	Ip6FragFails;
 	unsigned long	Ip6FragCreates;
 	unsigned long	Ip6InMcastPkts;
 	unsigned long	Ip6OutMcastPkts;
	unsigned long   __pad[0]; 
} ____cacheline_aligned;
 
/*
 * RFC 1213:  MIB-II ICMP Group
 * RFC 2011 (updates 1213):  SNMPv2 MIB for IP: ICMP group
 */
struct icmp_mib
{
 	unsigned long	IcmpInMsgs;
 	unsigned long	IcmpInErrors;
  	unsigned long	IcmpInDestUnreachs;
 	unsigned long	IcmpInTimeExcds;
 	unsigned long	IcmpInParmProbs;
 	unsigned long	IcmpInSrcQuenchs;
 	unsigned long	IcmpInRedirects;
 	unsigned long	IcmpInEchos;
 	unsigned long	IcmpInEchoReps;
 	unsigned long	IcmpInTimestamps;
 	unsigned long	IcmpInTimestampReps;
 	unsigned long	IcmpInAddrMasks;
 	unsigned long	IcmpInAddrMaskReps;
 	unsigned long	IcmpOutMsgs;
 	unsigned long	IcmpOutErrors;
 	unsigned long	IcmpOutDestUnreachs;
 	unsigned long	IcmpOutTimeExcds;
 	unsigned long	IcmpOutParmProbs;
 	unsigned long	IcmpOutSrcQuenchs;
 	unsigned long	IcmpOutRedirects;
 	unsigned long	IcmpOutEchos;
 	unsigned long	IcmpOutEchoReps;
 	unsigned long	IcmpOutTimestamps;
 	unsigned long	IcmpOutTimestampReps;
 	unsigned long	IcmpOutAddrMasks;
 	unsigned long	IcmpOutAddrMaskReps;
	unsigned long	dummy;
	unsigned long   __pad[0]; 
} ____cacheline_aligned;

/*
 * RFC 2466:  ICMPv6-MIB
 */
struct icmpv6_mib
{
	unsigned long	Icmp6InMsgs;
	unsigned long	Icmp6InErrors;

	unsigned long	Icmp6InDestUnreachs;
	unsigned long	Icmp6InPktTooBigs;
	unsigned long	Icmp6InTimeExcds;
	unsigned long	Icmp6InParmProblems;

	unsigned long	Icmp6InEchos;
	unsigned long	Icmp6InEchoReplies;
	unsigned long	Icmp6InGroupMembQueries;
	unsigned long	Icmp6InGroupMembResponses;
	unsigned long	Icmp6InGroupMembReductions;
	unsigned long	Icmp6InRouterSolicits;
	unsigned long	Icmp6InRouterAdvertisements;
	unsigned long	Icmp6InNeighborSolicits;
	unsigned long	Icmp6InNeighborAdvertisements;
	unsigned long	Icmp6InRedirects;

	unsigned long	Icmp6OutMsgs;

	unsigned long	Icmp6OutDestUnreachs;
	unsigned long	Icmp6OutPktTooBigs;
	unsigned long	Icmp6OutTimeExcds;
	unsigned long	Icmp6OutParmProblems;

	unsigned long	Icmp6OutEchoReplies;
	unsigned long	Icmp6OutRouterSolicits;
	unsigned long	Icmp6OutNeighborSolicits;
	unsigned long	Icmp6OutNeighborAdvertisements;
	unsigned long	Icmp6OutRedirects;
	unsigned long	Icmp6OutGroupMembResponses;
	unsigned long	Icmp6OutGroupMembReductions;
	unsigned long   __pad[0]; 
} ____cacheline_aligned;
 
/*
 * RFC 1213:  MIB-II TCP group
 * RFC 2012 (updates 1213):  SNMPv2-MIB-TCP
 */
struct tcp_mib
{
 	unsigned long	TcpRtoAlgorithm;
 	unsigned long	TcpRtoMin;
 	unsigned long	TcpRtoMax;
 	unsigned long	TcpMaxConn;
 	unsigned long	TcpActiveOpens;
 	unsigned long	TcpPassiveOpens;
 	unsigned long	TcpAttemptFails;
 	unsigned long	TcpEstabResets;
 	unsigned long	TcpCurrEstab;
 	unsigned long	TcpInSegs;
 	unsigned long	TcpOutSegs;
 	unsigned long	TcpRetransSegs;
 	unsigned long	TcpInErrs;
 	unsigned long	TcpOutRsts;
	unsigned long   __pad[0]; 
} ____cacheline_aligned;
 
/*
 * RFC 1213:  MIB-II UDP group
 * RFC 2013 (updates 1213):  SNMPv2-MIB-UDP
 */
struct udp_mib
{
 	unsigned long	UdpInDatagrams;
 	unsigned long	UdpNoPorts;
 	unsigned long	UdpInErrors;
 	unsigned long	UdpOutDatagrams;
	unsigned long   __pad[0];
} ____cacheline_aligned; 

/* draft-ietf-sigtran-sctp-mib-07.txt */
struct sctp_mib
{
	unsigned long   SctpCurrEstab;
	unsigned long   SctpActiveEstabs;
	unsigned long   SctpPassiveEstabs;
	unsigned long   SctpAborteds;
	unsigned long   SctpShutdowns;
	unsigned long   SctpOutOfBlues;
	unsigned long   SctpChecksumErrors;
	unsigned long   SctpOutCtrlChunks;
	unsigned long   SctpOutOrderChunks;
	unsigned long   SctpOutUnorderChunks;
	unsigned long   SctpInCtrlChunks;
	unsigned long   SctpInOrderChunks;
	unsigned long   SctpInUnorderChunks;
	unsigned long   SctpFragUsrMsgs;
	unsigned long   SctpReasmUsrMsgs;
	unsigned long   SctpOutSCTPPacks;
	unsigned long   SctpInSCTPPacks;
	unsigned long   SctpRtoAlgorithm;
	unsigned long   SctpRtoMin;
	unsigned long   SctpRtoMax;
	unsigned long   SctpRtoInitial;
	unsigned long   SctpValCookieLife;
	unsigned long   SctpMaxInitRetr;
	unsigned long   __pad[0];
};

struct linux_mib 
{
	unsigned long	SyncookiesSent;
	unsigned long	SyncookiesRecv;
	unsigned long	SyncookiesFailed;
	unsigned long	EmbryonicRsts;
	unsigned long	PruneCalled; 
	unsigned long	RcvPruned;
	unsigned long	OfoPruned;
	unsigned long	OutOfWindowIcmps; 
	unsigned long	LockDroppedIcmps; 
        unsigned long   ArpFilter;
	unsigned long	TimeWaited; 
	unsigned long	TimeWaitRecycled; 
	unsigned long	TimeWaitKilled; 
	unsigned long	PAWSPassiveRejected; 
	unsigned long	PAWSActiveRejected; 
	unsigned long	PAWSEstabRejected; 
	unsigned long	DelayedACKs;
	unsigned long	DelayedACKLocked;
	unsigned long	DelayedACKLost;
	unsigned long	ListenOverflows;
	unsigned long	ListenDrops;
	unsigned long	TCPPrequeued;
	unsigned long	TCPDirectCopyFromBacklog;
	unsigned long	TCPDirectCopyFromPrequeue;
	unsigned long	TCPPrequeueDropped;
	unsigned long	TCPHPHits;
	unsigned long	TCPHPHitsToUser;
	unsigned long	TCPPureAcks;
	unsigned long	TCPHPAcks;
	unsigned long	TCPRenoRecovery;
	unsigned long	TCPSackRecovery;
	unsigned long	TCPSACKReneging;
	unsigned long	TCPFACKReorder;
	unsigned long	TCPSACKReorder;
	unsigned long	TCPRenoReorder;
	unsigned long	TCPTSReorder;
	unsigned long	TCPFullUndo;
	unsigned long	TCPPartialUndo;
	unsigned long	TCPDSACKUndo;
	unsigned long	TCPLossUndo;
	unsigned long	TCPLoss;
	unsigned long	TCPLostRetransmit;
	unsigned long	TCPRenoFailures;
	unsigned long	TCPSackFailures;
	unsigned long	TCPLossFailures;
	unsigned long	TCPFastRetrans;
	unsigned long	TCPForwardRetrans;
	unsigned long	TCPSlowStartRetrans;
	unsigned long	TCPTimeouts;
	unsigned long	TCPRenoRecoveryFail;
	unsigned long	TCPSackRecoveryFail;
	unsigned long	TCPSchedulerFailed;
	unsigned long	TCPRcvCollapsed;
	unsigned long	TCPDSACKOldSent;
	unsigned long	TCPDSACKOfoSent;
	unsigned long	TCPDSACKRecv;
	unsigned long	TCPDSACKOfoRecv;
	unsigned long	TCPAbortOnSyn;
	unsigned long	TCPAbortOnData;
	unsigned long	TCPAbortOnClose;
	unsigned long	TCPAbortOnMemory;
	unsigned long	TCPAbortOnTimeout;
	unsigned long	TCPAbortOnLinger;
	unsigned long	TCPAbortFailed;
	unsigned long	TCPMemoryPressures;
	unsigned long   __pad[0];
} ____cacheline_aligned;


/* 
 * FIXME: On x86 and some other CPUs the split into user and softirq parts is not needed because 
 * addl $1,memory is atomic against interrupts (but atomic_inc would be overkill because of the lock 
 * cycles). Wants new nonlocked_atomic_inc() primitives -AK
 */ 
#define SNMP_INC_STATS(mib, field) ((mib)[2*smp_processor_id()+!in_softirq()].field++)
#define SNMP_INC_STATS_BH(mib, field) ((mib)[2*smp_processor_id()].field++)
#define SNMP_INC_STATS_USER(mib, field) ((mib)[2*smp_processor_id()+1].field++)
#define SNMP_ADD_STATS_BH(mib, field, addend)	\
	((mib)[2*smp_processor_id()].field += addend)
#define SNMP_ADD_STATS_USER(mib, field, addend)	\
	((mib)[2*smp_processor_id()+1].field += addend)
#endif
