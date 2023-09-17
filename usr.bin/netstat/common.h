/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
 *	Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)netstat.h	8.2 (Berkeley) 1/4/94
 */

#ifndef _NETSTAT_COMMON_H_
#define _NETSTAT_COMMON_H_

struct bits {
	u_long	b_mask;
	char	b_val;
	const char *b_name;
};
extern struct bits rt_bits[];

const char *fmt_flags(const struct bits *p, int f);
void print_flags_generic(int flags, const struct bits *pbits,
    const char *format, const char *tag_name);
int p_sockaddr(const char *name, struct sockaddr *sa, struct sockaddr *mask,
    int flags, int width);

struct _wid {
	int dst;
	int gw;
	int flags;
	int pksent;
	int mtu;
	int iface;
	int expire;
};
void set_wid(int fam);
void pr_rthdr(int af1 __unused);
extern struct _wid wid;
void p_flags(int f, const char *format);

bool p_rtable_netlink(int fibnum, int af);

struct ifmap_entry {
	char ifname[IFNAMSIZ];
	uint32_t mtu;
};

struct ifmap_entry *prepare_ifmap(size_t *ifmap_size);
extern const uint32_t rt_default_weight;

struct rt_msghdr;
struct nhops_map {
	uint32_t		idx;
	struct rt_msghdr	*rtm;
};

struct nhops_dump {
	void 		*nh_buf;
	struct nhops_map *nh_map;
	size_t		nh_count;
};

void dump_nhops_sysctl(int fibnum, int af, struct nhops_dump *nd);
struct nhop_map;
void nhop_map_update(struct nhop_map *map, uint32_t idx, char *gw, char *ifname);


#endif

