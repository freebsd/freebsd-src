/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025-2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
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

#ifndef _NET_IF_GENEVE_H_
#define _NET_IF_GENEVE_H_

#include <sys/types.h>

#ifdef _KERNEL
struct genevehdr {
#if BYTE_ORDER == LITTLE_ENDIAN
	uint8_t		geneve_optlen:6,	/* Opt Len */
			geneve_ver:2;		/* version */
	uint8_t		geneve_flags:6,		/* GENEVE Flags */
			geneve_critical:1,	/* critical options present */
			geneve_control:1;	/* control packets */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	uint8_t		geneve_ver:2,		/* version */
			geneve_optlen:6;	/* Opt Len */
	uint8_t		geneve_control:1,	/* control packets */
			geneve_critical:1,	/* critical options present */
			geneve_flags:6;		/* GENEVE Flags */
#endif
	uint16_t	geneve_proto;	/* protocol type (follows Ethertypes) */
	uint32_t	geneve_vni;	/* virtual network identifier */
} __packed;

struct geneveudphdr {
	struct udphdr		geneve_udp;
	struct genevehdr	geneve_hdr;
} __packed;
#endif /* _KERNEL */

struct geneve_params {
	uint16_t	ifla_proto;
};

#define GENEVE_VNI_MAX		(1 << 24)

#define GENEVE_PROTO_ETHER	0x6558	/* Ethernet */
#define GENEVE_PROTO_INHERIT	0x0	/* inherit inner layer 3 headers */
#define GENEVE_UDPPORT		6081

#endif /* _NET_IF_GENEVE_H_ */
