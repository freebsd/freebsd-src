/*-
 * Copyright 2005, Gleb Smirnoff <glebius@FreeBSD.org>
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
 * $FreeBSD: src/sys/netgraph/ng_nat.h,v 1.2 2007/05/22 12:20:05 mav Exp $
 */

#define NG_NAT_NODE_TYPE    "nat"
#define NGM_NAT_COOKIE      1107718711

#define	NG_NAT_HOOK_IN	"in"
#define	NG_NAT_HOOK_OUT	"out"

/* Arguments for NGM_NAT_SET_MODE message */
struct ng_nat_mode {
	uint32_t	flags;
	uint32_t	mask;
};

/* Keep this in sync with the above structure definition */
#define NG_NAT_MODE_INFO {				\
	  { "flags",	&ng_parse_uint32_type	},	\
	  { "mask",	&ng_parse_uint32_type	},	\
	  { NULL }					\
}

#define NG_NAT_LOG			0x01
#define NG_NAT_DENY_INCOMING		0x02
#define NG_NAT_SAME_PORTS		0x04
#define NG_NAT_UNREGISTERED_ONLY	0x10
#define NG_NAT_RESET_ON_ADDR_CHANGE	0x20
#define NG_NAT_PROXY_ONLY		0x40
#define NG_NAT_REVERSE			0x80

enum {
	NGM_NAT_SET_IPADDR = 1,
	NGM_NAT_SET_MODE,
	NGM_NAT_SET_TARGET,
};
