
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ankur Kothiwal
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
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/epoch.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <net/if.h>
#include "pfil.h"
#include <sys/ebpf_probe.h>
#include <sys/xdp.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_icmp.h>

static int parse_ipv4(void *data, uint64_t nh_off, void *data_end)
{
	struct ip *iph = data + nh_off;

	if(iph + 1 > data_end)
		return 0;
	return iph->ip_p;
}

int icmp_filter(void *data, uint64_t len)
{
	void *data_end = data + len;
	struct ether_header *eth = data;
	uint64_t nh_off;
	uint32_t ip = 0;
	short ether_type;

	nh_off = sizeof(*eth);
	
	if (data + nh_off > data_end)
		return XDP_ABORTED;

	if (eth->ether_type == htons(ETHERTYPE_IP))
		ip = parse_ipv4(data, nh_off, data_end);
	if (ip == IPPROTO_ICMP)
		return XDP_DROP; 
	return XDP_PASS;
}


