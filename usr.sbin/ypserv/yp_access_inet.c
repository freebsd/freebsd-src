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

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>

#include "yp_extern.h"

int
yp_mask2prefixlen_in(const struct sockaddr *sap, int *prefixlen)
{
        int x, y = 0;
	const u_char *p;
	const struct in_addr *addr;
	const struct sockaddr_in *sainp;

	sainp = (const struct sockaddr_in *)sap;
	addr = &(sainp->sin_addr);
	p = (const u_char *)&addr->s_addr;
        for (x = 0; x < (int)sizeof(addr->s_addr); x++)
                if (p[x] != 0xff)
                        break;
        if (x < (int)sizeof(addr->s_addr))
                for (y = 0; y < 8; y++)
                        if ((p[x] & (0x80 >> y)) == 0)
                                break;
	*prefixlen = x * 8 + y;
        return (0);
}

int
yp_prefixlen2mask_in(struct sockaddr *sap, const int *prefixlen)
{
        int i;
	int len;
	u_char *p;
	struct in_addr *addr;
	struct sockaddr_in *sainp;

	len = *prefixlen;
	if (0 > len || len > 32)
		return (-1);

	sainp = (struct sockaddr_in *)sap;
	memset(&sainp->sin_addr, 0, sizeof(sainp->sin_addr));
	addr = &(sainp->sin_addr);
	p = (u_char *)&addr->s_addr;
        for (i = 0; i < len / 8; i++)
                p[i] = 0xff;
        if (len % 8)
                p[i] = (0xff00 >> (len % 8)) & 0xff;
	return (0);
}

struct sockaddr *
yp_mask_in(const struct sockaddr *addr, const struct sockaddr *mask)
{
	int i;
	const u_char *p, *q;
	u_char *r;
	const struct sockaddr_in *in_addr;
	const struct sockaddr_in *in_mask;
	struct sockaddr_in *in_res;

	in_addr = (const struct sockaddr_in *)addr;
	in_mask = (const struct sockaddr_in *)mask;

	if ((in_res = malloc(sizeof(*in_res))) == NULL)
		return NULL;
	memcpy(in_res, in_addr, sizeof(*in_res));
	p = (const u_char *)&(in_addr->sin_addr.s_addr);
	q = (const u_char *)&(in_mask->sin_addr.s_addr);
	r = (u_char *)&(in_res->sin_addr.s_addr);
	for (i = 0; i < (int)sizeof(in_addr->sin_addr.s_addr); i++)
		r[i] = p[i] & q[i];

	return ((struct sockaddr *)in_res);
}

int 
yp_compare_subnet_in(const struct sockaddr *a1, const struct sockaddr *a2)
{
        const struct sockaddr_in *in_a1 = (const struct sockaddr_in *)a1;
        const struct sockaddr_in *in_a2 = (const struct sockaddr_in *)a2;

	if (debug) {
		yp_error("yp_subnet_cmp_in(): a1");
		yp_debug_sa(a1);
		yp_error("yp_subnet_cmp_in(): a2");
		yp_debug_sa(a2);
        }
        return (memcmp((const u_char *)&(in_a1->sin_addr.s_addr),
                    (const u_char *)&(in_a2->sin_addr.s_addr),
                    sizeof(in_a1->sin_addr.s_addr)));
}
