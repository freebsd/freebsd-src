/*
 * Copyright (C) 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 * $FreeBSD: src/usr.sbin/pim6sd/inet6.c,v 1.1.2.1 2000/07/15 07:36:36 kris Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include "defs.h"
#include "vif.h"
#include "inet6.h"
#include <arpa/inet.h>

/* flag if address to hostname resolution should be perfomed */
int numerichost = TRUE;

int
inet6_uvif2scopeid(struct sockaddr_in6 * sa, struct uvif * v)
{
    if (IN6_IS_ADDR_MULTICAST(&sa->sin6_addr))
    {
	if (IN6_IS_ADDR_MC_LINKLOCAL(&sa->sin6_addr))
	    return (v->uv_ifindex);
	if (IN6_IS_ADDR_MC_SITELOCAL(&sa->sin6_addr))
	    return (v->uv_siteid);
    }
    else
    {
	if (IN6_IS_ADDR_LINKLOCAL(&sa->sin6_addr))
	    return (v->uv_ifindex);

	if (IN6_IS_ADDR_SITELOCAL(&sa->sin6_addr))
	    return (v->uv_siteid);
    }

    return (0);
}

int
inet6_localif_address(struct sockaddr_in6 * sa, struct uvif * v)
{
    struct phaddr  *pa;

    for (pa = v->uv_addrs; pa; pa = pa->pa_next)
	if (inet6_equal(sa, &pa->pa_addr))
	    return (TRUE);

    return (FALSE);
}

int
inet6_valid_host(struct sockaddr_in6 * addr)
{
    if (IN6_IS_ADDR_MULTICAST(&addr->sin6_addr))
	return (FALSE);

    return (TRUE);
}

int
inet6_equal(struct sockaddr_in6 * sa1, struct sockaddr_in6 * sa2)
{
    if (sa1->sin6_scope_id == sa2->sin6_scope_id &&
	IN6_ARE_ADDR_EQUAL(&sa1->sin6_addr, &sa2->sin6_addr))
	return (1);

    return (0);
}

int
inet6_lessthan(struct sockaddr_in6 * sa1, struct sockaddr_in6 * sa2)
{
    u_int32_t       s32_1,
                    s32_2;
    int             i;

    if (sa1->sin6_scope_id < sa2->sin6_scope_id)
	return (1);
    if (sa1->sin6_scope_id == sa2->sin6_scope_id)
    {
	for (i = 0; i < 4; i++)
	{
	    s32_1 = ntohl(*(u_int32_t *)&sa1->sin6_addr.s6_addr[i * 4]);
	    s32_2 = ntohl(*(u_int32_t *)&sa2->sin6_addr.s6_addr[i * 4]);

	    if (s32_1 > s32_2)
		return (0);
	    if (s32_1 < s32_2)
		return (1);

	    /* otherwide, continue to compare */
	}
    }

    return (0);
}

int
inet6_greaterthan(struct sockaddr_in6 * sa1, struct sockaddr_in6 * sa2)
{
    u_int32_t       s32_1,
                    s32_2;
    int             i;

    if (sa1->sin6_scope_id > sa2->sin6_scope_id)
	return (1);
    if (sa1->sin6_scope_id == sa2->sin6_scope_id)
    {
	for (i = 0; i < 4; i++)
	{
	    s32_1 = ntohl(*(u_int32_t *)&sa1->sin6_addr.s6_addr[i * 4]);
	    s32_2 = ntohl(*(u_int32_t *)&sa2->sin6_addr.s6_addr[i * 4]);

	    if (s32_1 < s32_2)
		return (0);
	    if (s32_1 > s32_2)
		return (1);

	    /* otherwide, continue to compare */
	}
    }

    return (0);
}

int
inet6_match_prefix(sa1, sa2, mask)
    struct sockaddr_in6 *sa1,
                   *sa2;
    struct in6_addr *mask;
{
    int             i;

    if (sa1->sin6_scope_id != sa2->sin6_scope_id)
	return (0);

    for (i = 0; i < 16; i++)
    {
	if ((sa1->sin6_addr.s6_addr[i] ^ sa2->sin6_addr.s6_addr[i]) &
	    mask->s6_addr[i])
	    return (0);
    }

    return (1);
}

char *
sa6_fmt(struct sockaddr_in6 *sa6)
{
    static char     ip6buf[8][MAXHOSTNAMELEN];
    static int      ip6round = 0;
    int flags = NI_WITHSCOPEID;
    char           *cp;    

    ip6round = (ip6round + 1) & 7;
    cp = ip6buf[ip6round];

    if (numerichost)
	    flags |= NI_NUMERICHOST;
    getnameinfo((struct sockaddr *)sa6, sa6->sin6_len, cp, MAXHOSTNAMELEN,
		NULL, 0, flags);

    return(cp);
}

char *
inet6_fmt(struct in6_addr * addr)
{
    struct sockaddr_in6 sa6;


    memset(&sa6, 0, sizeof(sa6));
    sa6.sin6_len = sizeof(sa6);
    sa6.sin6_family = AF_INET6;
    sa6.sin6_addr = *addr;
    sa6.sin6_scope_id = 0;	/* XXX */

    return(sa6_fmt(&sa6));
}

char           *
ifindex2str(int ifindex)
{
    static char     ifname[IFNAMSIZ];

    return (if_indextoname(ifindex, ifname));
}

int
inet6_mask2plen(struct in6_addr * mask)
{
    int             masklen;
    u_char         *p = (u_char *) mask;
    u_char         *lim = p + 16;

    for (masklen = 0; p < lim; p++)
    {
	switch (*p)
	{
	case 0xff:
	    masklen += 8;
	    break;
	case 0xfe:
	    masklen += 7;
	    break;
	case 0xfc:
	    masklen += 6;
	    break;
	case 0xf8:
	    masklen += 5;
	    break;
	case 0xf0:
	    masklen += 4;
	    break;
	case 0xe0:
	    masklen += 3;
	    break;
	case 0xc0:
	    masklen += 2;
	    break;
	case 0x80:
	    masklen += 1;
	    break;
	case 0x00:
	    break;
	}
    }

    return (masklen);
}

char           *
net6name(struct in6_addr * prefix, struct in6_addr * mask)
{
    static char     ip6buf[8][INET6_ADDRSTRLEN + 4];	/* length of addr/plen */
    static int      ip6round = 0;
    char           *cp;

    ip6round = (ip6round + 1) & 7;
    cp = ip6buf[ip6round];

    inet_ntop(AF_INET6, prefix, cp, INET6_ADDRSTRLEN);
    cp += strlen(cp);
    *cp = '/';
    cp++;
    sprintf(cp, "%d", inet6_mask2plen(mask));

    return (ip6buf[ip6round]);
}
