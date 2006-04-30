/*-
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
 *                    with the aid of code written by
 *                    Junichi SATOH <junichi@astec.co.jp> 1996, 1997.
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

#ifdef _KERNEL
#include <sys/param.h>
#else
#include <sys/types.h>
#include <stdio.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#ifdef _KERNEL
#include <netinet/libalias/alias.h>
#include <netinet/libalias/alias_local.h>
#else
#include "alias_local.h"
#endif

/* CU-SeeMe Data Header */
struct cu_header {
	u_int16_t	dest_family;
	u_int16_t	dest_port;
	u_int32_t	dest_addr;
	int16_t		family;
	u_int16_t	port;
	u_int32_t	addr;
	u_int32_t	seq;
	u_int16_t	msg;
	u_int16_t	data_type;
	u_int16_t	packet_len;
};

/* Open Continue Header */
struct oc_header {
	u_int16_t	client_count;	/* Number of client info structs */
	u_int32_t	seq_no;
	char		user_name [20];
	char		reserved  [4];	/* flags, version stuff, etc */
};

/* client info structures */
struct client_info {
	u_int32_t	address;/* Client address */
	char		reserved  [8];	/* Flags, pruning bitfield, packet
					 * counts etc */
};

void
AliasHandleCUSeeMeOut(struct libalias *la, struct ip *pip, struct alias_link *lnk)
{
	struct udphdr *ud = ip_next(pip);

	if (ntohs(ud->uh_ulen) - sizeof(struct udphdr) >= sizeof(struct cu_header)) {
		struct cu_header *cu;
		struct alias_link *cu_lnk;

		cu = udp_next(ud);
		if (cu->addr)
			cu->addr = (u_int32_t) GetAliasAddress(lnk).s_addr;

		cu_lnk = FindUdpTcpOut(la, pip->ip_src, GetDestAddress(lnk),
		    ud->uh_dport, 0, IPPROTO_UDP, 1);

#ifndef NO_FW_PUNCH
		if (cu_lnk)
			PunchFWHole(cu_lnk);
#endif
	}
}

void
AliasHandleCUSeeMeIn(struct libalias *la, struct ip *pip, struct in_addr original_addr)
{
	struct in_addr alias_addr;
	struct udphdr *ud;
	struct cu_header *cu;
	struct oc_header *oc;
	struct client_info *ci;
	char *end;
	int i;

	(void)la;
	alias_addr.s_addr = pip->ip_dst.s_addr;
	ud = ip_next(pip);
	cu = udp_next(ud);
	oc = (struct oc_header *)(cu + 1);
	ci = (struct client_info *)(oc + 1);
	end = (char *)ud + ntohs(ud->uh_ulen);

	if ((char *)oc <= end) {
		if (cu->dest_addr)
			cu->dest_addr = (u_int32_t) original_addr.s_addr;
		if (ntohs(cu->data_type) == 101)
			/* Find and change our address */
			for (i = 0; (char *)(ci + 1) <= end && i < oc->client_count; i++, ci++)
				if (ci->address == (u_int32_t) alias_addr.s_addr) {
					ci->address = (u_int32_t) original_addr.s_addr;
					break;
				}
	}
}
