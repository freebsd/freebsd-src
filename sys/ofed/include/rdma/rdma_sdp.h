/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Isilon Systems, LLC.
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
 * $FreeBSD$
 */

#ifndef _SYS_OFED_INCLUDE_RDMA_RDMA_SDP_H_
#define _SYS_OFED_INCLUDE_RDMA_RDMA_SDP_H_

#include <sys/cdefs.h>
#include <netinet/in.h>

#include <rdma/ib_cm.h>

union cma_ip_addr {
	struct in6_addr ip6;
	struct {
		__be32 pad[3];
		__be32 addr;
	} ip4;
};

struct sdp_bsdh {
	u8 mid;
	u8 flags;
	__u16 bufs;
	__u32 len;
	__u32 mseq;
	__u32 mseq_ack;
} __packed;

struct sdp_hh {
	struct sdp_bsdh bsdh;
	u8 majv_minv;
	u8 ipv_cap;
	u8 rsvd1;
	u8 max_adverts;
	__u32 desremrcvsz;
	__u32 localrcvsz;
	__u16 port;
	__u16 rsvd2;
	union cma_ip_addr src_addr;
	union cma_ip_addr dst_addr;
	u8 rsvd3[IB_CM_REQ_PRIVATE_DATA_SIZE - sizeof(struct sdp_bsdh) - 48];
} __packed;

struct sdp_hah {
	struct sdp_bsdh bsdh;
	u8 majv_minv;
	u8 ipv_cap;
	u8 rsvd1;
	u8 ext_max_adverts;
	__u32 actrcvsz;
	u8 rsvd2[IB_CM_REP_PRIVATE_DATA_SIZE - sizeof(struct sdp_bsdh) - 8];
} __packed;

#endif /* _SYS_OFED_INCLUDE_RDMA_RDMA_SDP_H_ */
