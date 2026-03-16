/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed as part of the tcpstats kernel module.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NETINET_TCP_STATSDEV_H_
#define _NETINET_TCP_STATSDEV_H_

#include <sys/types.h>
#include <sys/ioccom.h>

#ifndef _KERNEL
#include <netinet/in.h>
#endif

#define	TCP_STATS_VERSION	1
#define	TCP_STATS_RECORD_SIZE	320
#define	TCP_STATS_CC_MAXLEN	16
#define	TCP_STATS_STACK_MAXLEN	16

/* Record flags */
#define	TSR_F_IPV6	0x00000001
#define	TSR_F_LISTEN	0x00000002
#define	TSR_F_SYNCACHE	0x00000004

/*
 * Fixed-size record emitted by /dev/tcpstats for each TCP connection.
 *
 * Layout is stable across the lifetime of a protocol version.
 * All padding is zeroed.  No kernel pointers except tsr_so_addr
 * (already exposed by tcp_pcblist sysctl).
 */
struct tcp_stats_record {
	/* Record header (16 bytes) */
	uint32_t	tsr_version;
	uint32_t	tsr_len;
	uint32_t	tsr_flags;
	uint32_t	_tsr_pad0;

	/* Connection identity (48 bytes) */
	uint8_t		tsr_af;
	uint8_t		_tsr_pad1[3];
	uint16_t	tsr_local_port;
	uint16_t	tsr_remote_port;
	union {
		struct in_addr	v4;
		struct in6_addr	v6;
	} tsr_local_addr;
	union {
		struct in_addr	v4;
		struct in6_addr	v6;
	} tsr_remote_addr;

	/* TCP state (8 bytes) */
	int32_t		tsr_state;
	uint32_t	tsr_flags_tcp;

	/* Congestion control (52 bytes) */
	uint32_t	tsr_snd_cwnd;
	uint32_t	tsr_snd_ssthresh;
	uint32_t	tsr_snd_wnd;
	uint32_t	tsr_rcv_wnd;
	uint32_t	tsr_maxseg;
	char		tsr_cc[TCP_STATS_CC_MAXLEN];
	char		tsr_stack[TCP_STATS_STACK_MAXLEN];

	/* RTT from tcp_fill_info() (16 bytes) */
	uint32_t	tsr_rtt;
	uint32_t	tsr_rttvar;
	uint32_t	tsr_rto;
	uint32_t	tsr_rttmin;

	/* Window scale + options (4 bytes) */
	uint8_t		tsr_snd_wscale;
	uint8_t		tsr_rcv_wscale;
	uint8_t		tsr_options;
	uint8_t		_tsr_pad2;

	/* Sequence numbers from tcp_fill_info() (20 bytes) */
	uint32_t	tsr_snd_nxt;
	uint32_t	tsr_snd_una;
	uint32_t	tsr_snd_max;
	uint32_t	tsr_rcv_nxt;
	uint32_t	tsr_rcv_adv;

	/* Counters (20 bytes) */
	uint32_t	tsr_snd_rexmitpack;
	uint32_t	tsr_rcv_ooopack;
	uint32_t	tsr_snd_zerowin;
	uint32_t	tsr_dupacks;
	uint32_t	tsr_rcv_numsacks;

	/* ECN (12 bytes) */
	uint32_t	tsr_ecn;
	uint32_t	tsr_delivered_ce;
	uint32_t	tsr_received_ce;

	/* DSACK (8 bytes) */
	uint32_t	tsr_dsack_bytes;
	uint32_t	tsr_dsack_pack;

	/* TLP (12 bytes) */
	uint32_t	tsr_total_tlp;
	uint64_t	tsr_total_tlp_bytes;

	/* Timers in milliseconds, 0 = not running (24 bytes) */
	int32_t		tsr_tt_rexmt;
	int32_t		tsr_tt_persist;
	int32_t		tsr_tt_keep;
	int32_t		tsr_tt_2msl;
	int32_t		tsr_tt_delack;
	int32_t		tsr_rcvtime;

	/* Buffer utilization (16 bytes) */
	uint32_t	tsr_snd_buf_cc;
	uint32_t	tsr_snd_buf_hiwat;
	uint32_t	tsr_rcv_buf_cc;
	uint32_t	tsr_rcv_buf_hiwat;

	/* Socket metadata (20 bytes) */
	uint64_t	tsr_so_addr;
	uint32_t	tsr_uid;
	uint64_t	tsr_inp_gencnt;

	/* Spare for future expansion (52 bytes) */
	uint32_t	_tsr_spare[13];
} __packed __aligned(8);

/* Compile-time size validation */
_Static_assert(sizeof(struct tcp_stats_record) == TCP_STATS_RECORD_SIZE,
    "tcp_stats_record size mismatch");

/* --- Ioctl definitions --- */

struct tcpstats_version {
	uint32_t	protocol_version;
	uint32_t	record_size;
	uint32_t	record_count_hint;
	uint32_t	flags;
};

/* --- Filter struct v2 --- */
#define	TSF_VERSION	2
#define	TSF_MAX_PORTS	8

/*
 * Socket filter -- configurable via ioctl or sysctl-created named profiles.
 *
 * All conditions are ANDed.  Empty/zero fields mean "match any".
 * Port arrays use network byte order.  A port value of 0 means "unused slot".
 *
 * Version 2: adds CIDR masks, include_state mode, expanded exclude flags.
 */
struct tcpstats_filter {
	/* Version for forward compatibility */
	uint32_t	version;	/* Must be TSF_VERSION */

	/* State filter */
	uint16_t	state_mask;	/* Bitmask of (1 << TCPS_*); 0xFFFF = all */
	uint16_t	_pad0;
	uint32_t	flags;

/* Exclude flags (one per TCP state) */
#define	TSF_EXCLUDE_CLOSED	0x00000001
#define	TSF_EXCLUDE_LISTEN	0x00000002
#define	TSF_EXCLUDE_SYN_SENT	0x00000004
#define	TSF_EXCLUDE_SYN_RCVD	0x00000008
#define	TSF_EXCLUDE_ESTABLISHED	0x00000010
#define	TSF_EXCLUDE_CLOSE_WAIT	0x00000020
#define	TSF_EXCLUDE_FIN_WAIT_1	0x00000040
#define	TSF_EXCLUDE_CLOSING	0x00000080
#define	TSF_EXCLUDE_LAST_ACK	0x00000100
#define	TSF_EXCLUDE_FIN_WAIT_2	0x00000200
#define	TSF_EXCLUDE_TIME_WAIT	0x00000400

/* Mode flags */
#define	TSF_STATE_INCLUDE_MODE	0x00001000
#define	TSF_LOCAL_PORT_MATCH	0x00002000
#define	TSF_REMOTE_PORT_MATCH	0x00004000
#define	TSF_LOCAL_ADDR_MATCH	0x00008000
#define	TSF_REMOTE_ADDR_MATCH	0x00010000
#define	TSF_IPV4_ONLY		0x00020000
#define	TSF_IPV6_ONLY		0x00040000

	/* Port filters -- match if socket port is ANY of the listed ports */
	uint16_t	local_ports[TSF_MAX_PORTS];	/* Network byte order */
	uint16_t	remote_ports[TSF_MAX_PORTS];	/* Network byte order */

	/* IPv4 address filters with CIDR mask */
	struct in_addr	local_addr_v4;
	struct in_addr	local_mask_v4;
	struct in_addr	remote_addr_v4;
	struct in_addr	remote_mask_v4;

	/* IPv6 address filters with prefix length */
	struct in6_addr	local_addr_v6;
	uint8_t		local_prefix_v6;
	uint8_t		_pad1[3];
	struct in6_addr	remote_addr_v6;
	uint8_t		remote_prefix_v6;
	uint8_t		_pad2[3];

	/* Field mask and format */
	uint32_t	field_mask;
	uint32_t	format;
#define	TSF_FORMAT_COMPACT	0
#define	TSF_FORMAT_FULL		1

	/* Spare for future expansion */
	uint32_t	_spare[4];
};

_Static_assert(sizeof(struct tcpstats_filter) <= 256,
    "tcpstats_filter exceeds maximum profile size");

#define	TCPSTATS_VERSION_CMD	_IOR('T', 1, struct tcpstats_version)
#define	TCPSTATS_SET_FILTER	_IOW('T', 2, struct tcpstats_filter)
#define	TCPSTATS_RESET		_IO('T', 3)

#endif /* _NETINET_TCP_STATSDEV_H_ */
