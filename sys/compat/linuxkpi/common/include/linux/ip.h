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
 *
 * $FreeBSD$
 */

#ifndef	_LINUXKPI_LINUX_IP_H
#define	_LINUXKPI_LINUX_IP_H

#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <machine/in_cksum.h>

#include <linux/skbuff.h>

/* (u) unconfirmed structure field names; using FreeBSD's meanwhile. */
struct iphdr {
	uint8_t		ip_hl:4, ip_ver:4;	/* (u) */
	uint8_t		ip_tos;			/* (u) */
	uint16_t	ip_len;			/* (u) */
	uint16_t	id;
	uint16_t	ip_off;			/* (u) */
	uint8_t		ip_ttl;			/* (u) */
	uint8_t		protocol;
	uint16_t	check;
	uint32_t	saddr;
	uint32_t	daddr;
};

static __inline struct iphdr *
ip_hdr(struct sk_buff *skb)
{

	return (struct iphdr *)skb_network_header(skb);
}

static __inline void
ip_send_check(struct iphdr *iph)
{

	/* Clear the checksum before computing! */
	iph->check = 0;
	/* An IPv4 header is the same everywhere even if names differ. */
	iph->check = in_cksum_hdr((const void *)iph);
}

#endif	/* _LINUXKPI_LINUX_IP_H */
