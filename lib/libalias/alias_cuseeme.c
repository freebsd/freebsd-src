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
 *
 * $FreeBSD: src/lib/libalias/alias_cuseeme.c,v 1.2.2.1 2000/09/21 06:56:34 ru Exp $
 */

#include <sys/types.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "alias_local.h"

/* CU-SeeMe Data Header */
struct cu_header {
    u_int16_t dest_family;
    u_int16_t dest_port;
    u_int32_t  dest_addr;
    int16_t family;
    u_int16_t port;
    u_int32_t addr;
    u_int32_t seq;
    u_int16_t msg;
    u_int16_t data_type;
    u_int16_t packet_len;
};

/* Open Continue Header */
struct oc_header {
    u_int16_t client_count;    /* Number of client info structs */
    u_int32_t seq_no;
    char user_name[20];
    char reserved[4];        /* flags, version stuff, etc */
};

/* client info structures */
struct client_info {
    u_int32_t address;          /* Client address */
    char reserved[8];        /* Flags, pruning bitfield, packet counts etc */
};

void
AliasHandleCUSeeMeOut(struct ip *pip, struct alias_link *link)
{
  struct udphdr *ud;

  ud = (struct udphdr *)((char *)pip + (pip->ip_hl << 2));
  if (ntohs(ud->uh_ulen) - sizeof(struct udphdr) >= sizeof(struct cu_header)) {
    struct cu_header *cu;
    struct alias_link *cu_link;

    cu = (struct cu_header *)(ud + 1);
    if (cu->addr)
      cu->addr = (u_int32_t)GetAliasAddress(link).s_addr;

    cu_link = FindUdpTcpOut(pip->ip_src, GetDestAddress(link),
                            ud->uh_dport, 0, IPPROTO_UDP);
                         
#ifndef NO_FW_PUNCH
    if (cu_link)
        PunchFWHole(cu_link);
#endif
  }
}

void
AliasHandleCUSeeMeIn(struct ip *pip, struct in_addr original_addr)
{
  struct in_addr alias_addr;
  struct udphdr *ud;
  struct cu_header *cu;
  struct oc_header *oc;
  struct client_info *ci;
  char *end;
  int i;

  alias_addr.s_addr = pip->ip_dst.s_addr;
  ud = (struct udphdr *)((char *)pip + (pip->ip_hl << 2));
  cu = (struct cu_header *)(ud + 1);
  oc = (struct oc_header *)(cu + 1);
  ci = (struct client_info *)(oc + 1);
  end = (char *)ud + ntohs(ud->uh_ulen);

  if ((char *)oc <= end) {
    if(cu->dest_addr)
      cu->dest_addr = (u_int32_t)original_addr.s_addr;
    if(ntohs(cu->data_type) == 101)
      /* Find and change our address */
      for(i = 0; (char *)(ci + 1) <= end && i < oc->client_count; i++, ci++)
        if(ci->address == (u_int32_t)alias_addr.s_addr) {
          ci->address = (u_int32_t)original_addr.s_addr;
          break;
        }
  }
}
