/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		This file implements the various access functions for the
 *		PROC file system.  This is very similar to the IPv4 version,
 *		except it reports the sockets in the INET6 address family.
 *
 * Version:	$Id: proc.c,v 1.15.2.1 2002/01/24 15:46:07 davem Exp $
 *
 * Authors:	David S. Miller (davem@caip.rutgers.edu)
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#include <linux/sched.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/stddef.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/transp_v6.h>
#include <net/ipv6.h>

static int fold_prot_inuse(struct proto *proto)
{
	int res = 0;
	int cpu;

	for (cpu=0; cpu<smp_num_cpus; cpu++)
		res += proto->stats[cpu_logical_map(cpu)].inuse;

	return res;
}

int afinet6_get_info(char *buffer, char **start, off_t offset, int length)
{
	int len = 0;
	len += sprintf(buffer+len, "TCP6: inuse %d\n",
		       fold_prot_inuse(&tcpv6_prot));
	len += sprintf(buffer+len, "UDP6: inuse %d\n",
		       fold_prot_inuse(&udpv6_prot));
	len += sprintf(buffer+len, "RAW6: inuse %d\n",
		       fold_prot_inuse(&rawv6_prot));
	len += sprintf(buffer+len, "FRAG6: inuse %d memory %d\n",
		       ip6_frag_nqueues, atomic_read(&ip6_frag_mem));
	*start = buffer + offset;
	len -= offset;
	if(len > length)
		len = length;
	return len;
}


struct snmp6_item
{
	char *name;
	unsigned long *ptr;
	int   mibsize;
} snmp6_list[] = {
/* ipv6 mib according to draft-ietf-ipngwg-ipv6-mib-04 */
#define SNMP6_GEN(x) { #x , &ipv6_statistics[0].x, sizeof(struct ipv6_mib)/sizeof(unsigned long) }
	SNMP6_GEN(Ip6InReceives),
	SNMP6_GEN(Ip6InHdrErrors),
	SNMP6_GEN(Ip6InTooBigErrors),
	SNMP6_GEN(Ip6InNoRoutes),
	SNMP6_GEN(Ip6InAddrErrors),
	SNMP6_GEN(Ip6InUnknownProtos),
	SNMP6_GEN(Ip6InTruncatedPkts),
	SNMP6_GEN(Ip6InDiscards),
	SNMP6_GEN(Ip6InDelivers),
	SNMP6_GEN(Ip6OutForwDatagrams),
	SNMP6_GEN(Ip6OutRequests),
	SNMP6_GEN(Ip6OutDiscards),
	SNMP6_GEN(Ip6OutNoRoutes),
	SNMP6_GEN(Ip6ReasmTimeout),
	SNMP6_GEN(Ip6ReasmReqds),
	SNMP6_GEN(Ip6ReasmOKs),
	SNMP6_GEN(Ip6ReasmFails),
	SNMP6_GEN(Ip6FragOKs),
	SNMP6_GEN(Ip6FragFails),
	SNMP6_GEN(Ip6FragCreates),
	SNMP6_GEN(Ip6InMcastPkts),
	SNMP6_GEN(Ip6OutMcastPkts),
#undef SNMP6_GEN
/* icmpv6 mib according to draft-ietf-ipngwg-ipv6-icmp-mib-02

   Exceptions:  {In|Out}AdminProhibs are removed, because I see
                no good reasons to account them separately
		of another dest.unreachs.
		OutErrs is zero identically.
		OutEchos too.
		OutRouterAdvertisements too.
		OutGroupMembQueries too.
 */
#define SNMP6_GEN(x) { #x , &icmpv6_statistics[0].x, sizeof(struct icmpv6_mib)/sizeof(unsigned long) }
	SNMP6_GEN(Icmp6InMsgs),
	SNMP6_GEN(Icmp6InErrors),
	SNMP6_GEN(Icmp6InDestUnreachs),
	SNMP6_GEN(Icmp6InPktTooBigs),
	SNMP6_GEN(Icmp6InTimeExcds),
	SNMP6_GEN(Icmp6InParmProblems),
	SNMP6_GEN(Icmp6InEchos),
	SNMP6_GEN(Icmp6InEchoReplies),
	SNMP6_GEN(Icmp6InGroupMembQueries),
	SNMP6_GEN(Icmp6InGroupMembResponses),
	SNMP6_GEN(Icmp6InGroupMembReductions),
	SNMP6_GEN(Icmp6InRouterSolicits),
	SNMP6_GEN(Icmp6InRouterAdvertisements),
	SNMP6_GEN(Icmp6InNeighborSolicits),
	SNMP6_GEN(Icmp6InNeighborAdvertisements),
	SNMP6_GEN(Icmp6InRedirects),
	SNMP6_GEN(Icmp6OutMsgs),
	SNMP6_GEN(Icmp6OutDestUnreachs),
	SNMP6_GEN(Icmp6OutPktTooBigs),
	SNMP6_GEN(Icmp6OutTimeExcds),
	SNMP6_GEN(Icmp6OutParmProblems),
	SNMP6_GEN(Icmp6OutEchoReplies),
	SNMP6_GEN(Icmp6OutRouterSolicits),
	SNMP6_GEN(Icmp6OutNeighborSolicits),
	SNMP6_GEN(Icmp6OutNeighborAdvertisements),
	SNMP6_GEN(Icmp6OutRedirects),
	SNMP6_GEN(Icmp6OutGroupMembResponses),
	SNMP6_GEN(Icmp6OutGroupMembReductions),
#undef SNMP6_GEN
#define SNMP6_GEN(x) { "Udp6" #x , &udp_stats_in6[0].Udp##x, sizeof(struct udp_mib)/sizeof(unsigned long) }
	SNMP6_GEN(InDatagrams),
	SNMP6_GEN(NoPorts),
	SNMP6_GEN(InErrors),
	SNMP6_GEN(OutDatagrams)
#undef SNMP6_GEN
};

static unsigned long fold_field(unsigned long *ptr, int size)
{
	unsigned long res = 0;
	int i;

	for (i=0; i<smp_num_cpus; i++) {
		res += ptr[2*cpu_logical_map(i)*size];
		res += ptr[(2*cpu_logical_map(i)+1)*size];
	}

	return res;
}

int afinet6_get_snmp(char *buffer, char **start, off_t offset, int length)
{
	int len = 0;
	int i;

	for (i=0; i<sizeof(snmp6_list)/sizeof(snmp6_list[0]); i++)
		len += sprintf(buffer+len, "%-32s\t%ld\n", snmp6_list[i].name,
			       fold_field(snmp6_list[i].ptr, snmp6_list[i].mibsize));

	len -= offset;

	if (len > length)
		len = length;
	if(len < 0)
		len = 0;

	*start = buffer + offset;

	return len;
}
