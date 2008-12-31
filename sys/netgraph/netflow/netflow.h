/*-
 * Copyright (c) 2004 Gleb Smirnoff <glebius@FreeBSD.org>
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
 *
 *	 $SourceForge: netflow.h,v 1.8 2004/09/16 17:05:11 glebius Exp $
 *	 $FreeBSD: src/sys/netgraph/netflow/netflow.h,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* netflow timeouts in seconds */

#define	ACTIVE_TIMEOUT		(30*60)	/* maximum flow lifetime is 30 min */
#define	INACTIVE_TIMEOUT	15

/*
 * More info can be found in these Cisco documents:
 *
 * Cisco IOS NetFlow, White Papers.
 * http://www.cisco.com/en/US/products/ps6601/prod_white_papers_list.html
 *
 * Cisco CNS NetFlow Collection Engine User Guide, 5.0.2, NetFlow Export
 * Datagram Formats.
 * http://www.cisco.com/en/US/products/sw/netmgtsw/ps1964/products_user_guide_chapter09186a00803f3147.html#wp26453
 *
 */

#define NETFLOW_V1 1
#define NETFLOW_V5 5

struct netflow_v1_header
{
  uint16_t version;	/* NetFlow version */
  uint16_t count;	/* Number of records in flow */
  uint32_t sys_uptime;	/* System uptime */
  uint32_t unix_secs;	/* Current seconds since 0000 UTC 1970 */
  uint32_t unix_nsecs;	/* Remaining nanoseconds since 0000 UTC 1970 */
} __attribute__((__packed__));

struct netflow_v5_header
{
  uint16_t version;	/* NetFlow version */
  uint16_t count;	/* Number of records in flow */
  uint32_t sys_uptime;	/* System uptime */
  uint32_t unix_secs;	/* Current seconds since 0000 UTC 1970 */
  uint32_t unix_nsecs;	/* Remaining nanoseconds since 0000 UTC 1970 */
  uint32_t flow_seq;	/* Sequence number of the first record */
  uint8_t engine_type;	/* Type of flow switching engine (RP,VIP,etc.) */
  uint8_t engine_id;	/* Slot number of the flow switching engine */
  uint16_t pad;		/* Pad to word boundary */
} __attribute__((__packed__));

struct netflow_v1_record
{
  uint32_t src_addr;	/* Source IP address */
  uint32_t dst_addr;	/* Destination IP address */
  uint32_t next_hop;	/* Next hop IP address */
  uint16_t in_ifx;	/* Source interface index */
  uint16_t out_ifx;	/* Destination interface index */
  uint32_t packets;	/* Number of packets in a flow */
  uint32_t octets;	/* Number of octets in a flow */
  uint32_t first;	/* System uptime at start of a flow */
  uint32_t last;	/* System uptime at end of a flow */
  uint16_t s_port;	/* Source port */
  uint16_t d_port;	/* Destination port */
  uint16_t pad1;	/* Pad to word boundary */
  uint8_t prot;		/* IP protocol */
  uint8_t tos;		/* IP type of service */
  uint8_t flags;	/* Cumulative OR of tcp flags */
  uint8_t pad2;		/* Pad to word boundary */
  uint16_t pad3;	/* Pad to word boundary */
  uint8_t reserved[5];	/* Reserved for future use */
} __attribute__((__packed__));

struct netflow_v5_record
{
  uint32_t src_addr;	/* Source IP address */
  uint32_t dst_addr;	/* Destination IP address */
  uint32_t next_hop;	/* Next hop IP address */
  uint16_t i_ifx;	/* Source interface index */
  uint16_t o_ifx;	/* Destination interface index */
  uint32_t packets;	/* Number of packets in a flow */
  uint32_t octets;	/* Number of octets in a flow */
  uint32_t first;	/* System uptime at start of a flow */
  uint32_t last;	/* System uptime at end of a flow */
  uint16_t s_port;	/* Source port */
  uint16_t d_port;	/* Destination port */
  uint8_t pad1;		/* Pad to word boundary */
  uint8_t flags;	/* Cumulative OR of tcp flags */
  uint8_t prot;		/* IP protocol */
  uint8_t tos;		/* IP type of service */
  uint16_t src_as;	/* Src peer/origin Autonomous System */
  uint16_t dst_as;	/* Dst peer/origin Autonomous System */
  uint8_t src_mask;	/* Source route's mask bits */
  uint8_t dst_mask;	/* Destination route's mask bits */
  uint16_t pad2;	/* Pad to word boundary */
} __attribute__((__packed__));

#define NETFLOW_V1_MAX_RECORDS 24
#define NETFLOW_V5_MAX_RECORDS 30

#define NETFLOW_V1_MAX_SIZE (sizeof(netflow_v1_header)+ \
			     sizeof(netflow_v1_record)*NETFLOW_V1_MAX_RECORDS)
#define NETFLOW_V5_MAX_SIZE (sizeof(netflow_v5_header)+ \
			     sizeof(netflow_v5_record)*NETFLOW_V5_MAX_RECORDS)

struct netflow_v5_export_dgram {
	struct netflow_v5_header	header;
	struct netflow_v5_record	r[NETFLOW_V5_MAX_RECORDS];
} __attribute__((__packed__));
