/*
 * Copyright (c) 2010-2011 Hiroki Sato.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#include "yp_extern.h"

int
yp_mask2prefixlen_in6(const struct sockaddr *sap, int *prefixlen)
{
        int x, y = 0;
        const u_char *p;
	const struct in6_addr *addr;
	const struct sockaddr_in6 *sain6p;

	sain6p = (const struct sockaddr_in6 *)sap;
	addr = &(sain6p->sin6_addr);
        for (x = 0; x < (int)sizeof(addr->s6_addr); x++)
		if (addr->s6_addr[x] != 0xff)
			break;
        if (x < (int)sizeof(addr->s6_addr))
                for (y = 0; y < 8; y++)
                        if ((addr->s6_addr[x] & (0x80 >> y)) == 0)
                                break;
        if (x < (int)sizeof(addr->s6_addr)) {
                if (y != 0 && (addr->s6_addr[x] & (0x00ff >> y)) != 0)
                        return (-1);
		p = (const u_char *)&addr->s6_addr[x + 1];
                for (; p < addr->s6_addr + sizeof(addr->s6_addr); p++)
                        if (*p != 0)
                                return (-1);
        }
	*prefixlen = x * 8 + y;
        return (0);
}

int
yp_prefixlen2mask_in6(struct sockaddr *sap, const int *prefixlen)
{
	int i;
	int len;
	int bytelen, bitlen;
	u_char maskarray[8] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
	struct in6_addr *addr;
	struct sockaddr_in6 *sain6p;

	len = *prefixlen;
	if (0 > len || len > 128)
		return (-1);

	sain6p = (struct sockaddr_in6 *)sap;
	memset(&sain6p->sin6_addr, 0, sizeof(sain6p->sin6_addr));
	addr = &(sain6p->sin6_addr);
	bytelen = len / 8;
	bitlen = len % 8;
	for (i = 0; i < bytelen; i++)
		addr->s6_addr[i] = 0xff;
	if (bitlen)
		addr->s6_addr[bytelen] = maskarray[bitlen - 1];
	return (0);
}

struct sockaddr *
yp_mask_in6(const struct sockaddr *addr, const struct sockaddr *mask)
{
	int i;
	const u_char *p, *q;
	u_char *r;
	const struct sockaddr_in6 *in6_addr;
	const struct sockaddr_in6 *in6_mask;
	struct sockaddr_in6 *in6_res;

	in6_addr = (const struct sockaddr_in6 *)addr;
	in6_mask = (const struct sockaddr_in6 *)mask;

	if ((in6_res = malloc(sizeof(*in6_res))) == NULL)
		return NULL;
	memcpy(in6_res, in6_addr, sizeof(*in6_res));
	p = (const u_char *)&(in6_addr->sin6_addr.s6_addr);
	q = (const u_char *)&(in6_mask->sin6_addr.s6_addr);
	r = (u_char *)&(in6_res->sin6_addr.s6_addr);
	for (i = 0; i < (int)sizeof(in6_addr->sin6_addr.s6_addr); i++)
		r[i] = p[i] & q[i];
        
	return ((struct sockaddr *)in6_res);
}

int
yp_compare_subnet_in6(const struct sockaddr *a1, const struct sockaddr *a2)
{
	const struct sockaddr_in6 *in6_a1 = (const struct sockaddr_in6 *)a1;
	const struct sockaddr_in6 *in6_a2 = (const struct sockaddr_in6 *)a2;

	if (debug) {
		yp_error("yp_subnet_cmp_in6(): a1");
		yp_debug_sa(a1);
		yp_error("yp_subnet_cmp_in6(): a2");
		yp_debug_sa(a2);
		yp_error("yp_subnet_cmp_in6(): scope: %d - %d",
		    in6_a1->sin6_scope_id, in6_a2->sin6_scope_id);
        }

	if (in6_a1->sin6_scope_id != in6_a2->sin6_scope_id)
		return (-1);

	return (memcmp(in6_a1->sin6_addr.s6_addr,
		    in6_a2->sin6_addr.s6_addr,
		    sizeof(in6_a1->sin6_addr.s6_addr)));
}
