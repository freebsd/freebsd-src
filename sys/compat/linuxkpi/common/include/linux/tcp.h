/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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

#ifndef	_LINUXKPI_LINUX_TCP_H
#define	_LINUXKPI_LINUX_TCP_H

#include <sys/types.h>
#include <linux/skbuff.h>

/* (u) unconfirmed structure field names; using FreeBSD's meanwhile. */
struct tcphdr {
	uint16_t	source;			/* (u) */
	uint16_t	dest;			/* (u) */
	uint32_t	th_seq;			/* (u) */
	uint32_t	th_ack;			/* (u) */
#if BYTE_ORDER == LITTLE_ENDIAN
	uint8_t		th_x2:4, doff:4;
#elif BYTE_ORDER == BIG_ENDIAN
	uint8_t		doff:4, th_x2:4;
#endif
	uint8_t		th_flags;		/* (u) */
	uint16_t	th_win;			/* (u) */
	uint16_t	check;
	uint16_t	th_urg;			/* (u) */
};

static __inline struct tcphdr *
tcp_hdr(struct sk_buff *skb)
{

	return (struct tcphdr *)skb_transport_header(skb);
}

static __inline uint32_t
tcp_hdrlen(struct sk_buff *skb)
{
	struct tcphdr *th;

	th = tcp_hdr(skb);
	return (4 * th->doff);
}

#endif	/* _LINUXKPI_LINUX_TCP_H */
