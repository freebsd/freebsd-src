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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef lint
static const char rcsid[] =
     "@(#) $Header: /tcpdump/master/tcpdump/print-ip6opts.c,v 1.8 2000/12/13 07:57:05 itojun Exp $";
#endif

#ifdef INET6
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <stdio.h>

#include "ip6.h"

#include "interface.h"
#include "addrtoname.h"

/* items outside of rfc2292bis */
#ifndef IP6OPT_MINLEN
#define IP6OPT_MINLEN	2
#endif
#ifndef IP6OPT_RTALERT_LEN
#define IP6OPT_RTALERT_LEN	4
#endif
#ifndef IP6OPT_JUMBO_LEN
#define IP6OPT_JUMBO_LEN	6
#endif
#define IP6OPT_HOMEADDR_MINLEN 18
#define IP6OPT_BU_MINLEN       10
#define IP6OPT_BA_MINLEN       13
#define IP6OPT_BR_MINLEN        2
#define IP6SOPT_ALTCOA        0x4
#define IP6SOPT_ALTCOA_MINLEN  18
#define IP6SOPT_UI            0x2
#define IP6SOPT_UI_MINLEN       4

static void ip6_sopt_print(const u_char *, int);

static void
ip6_sopt_print(const u_char *bp, int len)
{
    int i;
    int optlen;

    for (i = 0; i < len; i += optlen) {
	switch (bp[i]) {
	case IP6OPT_PAD1:
            printf(", pad1");
	    optlen = 1;
	    break;
	case IP6OPT_PADN:
	    if (len - i < IP6OPT_MINLEN) {
		printf(", padn: trunc");
		goto trunc;
	    }
            printf(", padn");
	    optlen = bp[i + 1] + 2;
	    break;
        case IP6SOPT_ALTCOA:
             if (len - i < IP6SOPT_ALTCOA_MINLEN) {
		printf(", altcoa: trunc");
		goto trunc;
	    }
            printf(", alt-CoA: %s", ip6addr_string(&bp[i+2]));
	    optlen = bp[i + 1] + 2;
	    break;
        case IP6SOPT_UI:
             if (len - i < IP6SOPT_UI_MINLEN) {
		printf(", ui: trunc");
		goto trunc;
	    }
            printf("(ui: 0x%04x) ", ntohs(*(u_int16_t *)&bp[i + 2]));
	    optlen = bp[i + 1] + 2;
	    break;
	default:
	    if (len - i < IP6OPT_MINLEN) {
		printf(", sopt_type %d: trunc)", bp[i]);
		goto trunc;
	    }
	    printf(", sopt_type 0x%02x: len=%d", bp[i], bp[i + 1]);
	    optlen = bp[i + 1] + 2;
	    break;
	}
    }
    return;

trunc:
    printf("[trunc] ");
}

void
ip6_opt_print(const u_char *bp, int len)
{
    int i;
    int optlen;

    for (i = 0; i < len; i += optlen) {
	switch (bp[i]) {
	case IP6OPT_PAD1:
            printf("(pad1)");
	    optlen = 1;
	    break;
	case IP6OPT_PADN:
	    if (len - i < IP6OPT_MINLEN) {
		printf("(padn: trunc)");
		goto trunc;
	    }
            printf("(padn)");
	    optlen = bp[i + 1] + 2;
	    break;
	case IP6OPT_ROUTER_ALERT:
	    if (len - i < IP6OPT_RTALERT_LEN) {
		printf("(rtalert: trunc)");
		goto trunc;
	    }
	    if (bp[i + 1] != IP6OPT_RTALERT_LEN - 2) {
		printf("(rtalert: invalid len %d)", bp[i + 1]);
		goto trunc;
	    }
	    printf("(rtalert: 0x%04x) ", ntohs(*(u_int16_t *)&bp[i + 2]));
	    optlen = IP6OPT_RTALERT_LEN;
	    break;
	case IP6OPT_JUMBO:
	    if (len - i < IP6OPT_JUMBO_LEN) {
		printf("(jumbo: trunc)");
		goto trunc;
	    }
	    if (bp[i + 1] != IP6OPT_JUMBO_LEN - 2) {
		printf("(jumbo: invalid len %d)", bp[i + 1]);
		goto trunc;
	    }
	    printf("(jumbo: %u) ", (u_int32_t)ntohl(*(u_int32_t *)&bp[i + 2]));
	    optlen = IP6OPT_JUMBO_LEN;
	    break;
        case IP6OPT_HOME_ADDRESS:
	    if (len - i < IP6OPT_HOMEADDR_MINLEN) {
		printf("(homeaddr: trunc)");
		goto trunc;
	    }
	    if (bp[i + 1] < IP6OPT_HOMEADDR_MINLEN - 2) {
		printf("(homeaddr: invalid len %d)", bp[i + 1]);
		goto trunc;
	    }
	    printf("(homeaddr: %s", ip6addr_string(&bp[i + 2]));
            if (bp[i + 1] > IP6OPT_HOMEADDR_MINLEN - 2) {
		ip6_sopt_print(&bp[i + IP6OPT_HOMEADDR_MINLEN],
		    (optlen-IP6OPT_HOMEADDR_MINLEN));
	    }
            printf(")");
	    optlen = bp[i + 1] + 2;
	    break;
        case IP6OPT_BINDING_UPDATE:
	    if (len - i < IP6OPT_BU_MINLEN) {
		printf("(bu: trunc)");
		goto trunc;
	    }
	    if (bp[i + 1] < IP6OPT_BU_MINLEN - 2) {
		printf("(bu: invalid len %d)", bp[i + 1]);
		goto trunc;
	    }
	    printf("(bu: ");
	    if (bp[i + 2] & 0x80)
		    printf("A");
	    if (bp[i + 2] & 0x40)
		    printf("H");
	    if (bp[i + 2] & 0x20)
		    printf("R");
	    if (bp[i + 2] & 0x10)
		    printf("D");
	    if (bp[i + 2] & 0x0f)
		    printf("res");
	    printf(", prefixlen: %u", bp[i + 3]);
	    printf(", sequence: %u",
		(u_int16_t)ntohs(*(u_int16_t *)&bp[i + 4]));
	    printf(", lifetime: %u",
		(u_int32_t)ntohs(*(u_int32_t *)&bp[i + 8]));

	    optlen = bp[i + 1] + 2;
	    if (bp[i + 1] > IP6OPT_BU_MINLEN - 2) {
		ip6_sopt_print(&bp[i + IP6OPT_BU_MINLEN],
		    (optlen - IP6OPT_BU_MINLEN));
	    }
	    printf(")");
	    break;
	case IP6OPT_BINDING_ACK:
	    if (len - i < IP6OPT_BA_MINLEN) {
		printf("(ba: trunc)");
		goto trunc;
	    }
	    if (bp[i + 1] < IP6OPT_BA_MINLEN - 2) {
		printf("(ba: invalid len %d)", bp[i + 1]);
		goto trunc;
	    }
	    printf("(ba: ");
	    printf("status: %u", bp[i + 2]);
	    printf(", sequence: %u",
		(u_int16_t)ntohs(*(u_int16_t *)&bp[i + 3]));
	    printf(", lifetime: %u",
		(u_int32_t)ntohs(*(u_int32_t *)&bp[i + 7]));
	    printf(", refresh: %u",
		(u_int32_t)ntohs(*(u_int32_t *)&bp[i + 11]));

	    if (bp[i + 1] > IP6OPT_BA_MINLEN - 2) {
		ip6_sopt_print(&bp[i + IP6OPT_BA_MINLEN],
		    (optlen-IP6OPT_BA_MINLEN));
	    }
            printf(")");
	    optlen = bp[i + 1] + 2;
	    break;
        case IP6OPT_BINDING_REQ:
	    if (len - i < IP6OPT_BR_MINLEN) {
		printf("(br: trunc)");
		goto trunc;
	    }
            printf("(br");
            if (bp[i + 1] > IP6OPT_BR_MINLEN - 2) {
		ip6_sopt_print(&bp[i + IP6OPT_BR_MINLEN],
		    (optlen-IP6OPT_BR_MINLEN));
	    }
            printf(")");
	    optlen = bp[i + 1] + 2;
	    break;
	default:
	    if (len - i < IP6OPT_MINLEN) {
		printf("(type %d: trunc)", bp[i]);
		goto trunc;
	    }
	    printf("(opt_type 0x%02x: len=%d) ", bp[i], bp[i + 1]);
	    optlen = bp[i + 1] + 2;
	    break;
	}
    }

#if 0
end:
#endif
    return;

trunc:
    printf("[trunc] ");
}

int
hbhopt_print(register const u_char *bp)
{
    const struct ip6_hbh *dp = (struct ip6_hbh *)bp;
    register const u_char *ep;
    int hbhlen = 0;

    /* 'ep' points to the end of available data. */
    ep = snapend;
    TCHECK(dp->ip6h_len);
    hbhlen = (int)((dp->ip6h_len + 1) << 3);
    TCHECK2(*dp, hbhlen);
    printf("HBH ");
    if (vflag)
	ip6_opt_print((const u_char *)dp + sizeof(*dp), hbhlen - sizeof(*dp));

    return(hbhlen);

  trunc:
    fputs("[|HBH]", stdout);
    return(hbhlen);
}

int
dstopt_print(register const u_char *bp)
{
    const struct ip6_dest *dp = (struct ip6_dest *)bp;
    register const u_char *ep;
    int dstoptlen = 0;

    /* 'ep' points to the end of available data. */
    ep = snapend;
    TCHECK(dp->ip6d_len);
    dstoptlen = (int)((dp->ip6d_len + 1) << 3);
    TCHECK2(*dp, dstoptlen);
    printf("DSTOPT ");
    if (vflag) {
	ip6_opt_print((const u_char *)dp + sizeof(*dp),
	    dstoptlen - sizeof(*dp));
    }

    return(dstoptlen);

  trunc:
    fputs("[|DSTOPT]", stdout);
    return(dstoptlen);
}
#endif /* INET6 */
