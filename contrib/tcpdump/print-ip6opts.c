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

/* \summary: IPv6 header option printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip6.h"

static int
ip6_sopt_print(netdissect_options *ndo, const u_char *bp, int len)
{
    int i;
    int optlen;

    for (i = 0; i < len; i += optlen) {
	if (GET_U_1(bp + i) == IP6OPT_PAD1)
	    optlen = 1;
	else {
	    if (i + 1 < len)
		optlen = GET_U_1(bp + i + 1) + 2;
	    else
		goto trunc;
	}
	if (i + optlen > len)
	    goto trunc;

	switch (GET_U_1(bp + i)) {
	case IP6OPT_PAD1:
            ND_PRINT(", pad1");
	    break;
	case IP6OPT_PADN:
	    if (len - i < IP6OPT_MINLEN) {
		ND_PRINT(", padn: trunc");
		goto trunc;
	    }
            ND_PRINT(", padn");
	    break;
	default:
	    if (len - i < IP6OPT_MINLEN) {
		ND_PRINT(", sopt_type %u: trunc)", GET_U_1(bp + i));
		goto trunc;
	    }
	    ND_PRINT(", sopt_type 0x%02x: len=%u", GET_U_1(bp + i),
                     GET_U_1(bp + i + 1));
	    break;
	}
    }
    return 0;

trunc:
    return -1;
}

static int
ip6_opt_process(netdissect_options *ndo, const u_char *bp, int len,
		int *found_jumbop, uint32_t *payload_len)
{
    int i;
    int optlen = 0;
    int found_jumbo = 0;
    uint32_t jumbolen = 0;

    if (len == 0)
        return 0;
    for (i = 0; i < len; i += optlen) {
	if (GET_U_1(bp + i) == IP6OPT_PAD1)
	    optlen = 1;
	else {
	    if (i + 1 < len)
		optlen = GET_U_1(bp + i + 1) + 2;
	    else
		goto trunc;
	}
	if (i + optlen > len)
	    goto trunc;

	switch (GET_U_1(bp + i)) {
	case IP6OPT_PAD1:
	    if (ndo->ndo_vflag)
                ND_PRINT("(pad1)");
	    break;
	case IP6OPT_PADN:
	    if (len - i < IP6OPT_MINLEN) {
		ND_PRINT("(padn: trunc)");
		goto trunc;
	    }
	    if (ndo->ndo_vflag)
                ND_PRINT("(padn)");
	    break;
	case IP6OPT_ROUTER_ALERT:
	    if (len - i < IP6OPT_RTALERT_LEN) {
		ND_PRINT("(rtalert: trunc)");
		goto trunc;
	    }
	    if (GET_U_1(bp + i + 1) != IP6OPT_RTALERT_LEN - 2) {
		ND_PRINT("(rtalert: invalid len %u)", GET_U_1(bp + i + 1));
		goto trunc;
	    }
	    if (ndo->ndo_vflag)
		ND_PRINT("(rtalert: 0x%04x) ", GET_BE_U_2(bp + i + 2));
	    break;
	case IP6OPT_JUMBO:
	    if (len - i < IP6OPT_JUMBO_LEN) {
		ND_PRINT("(jumbo: trunc)");
		goto trunc;
	    }
	    if (GET_U_1(bp + i + 1) != IP6OPT_JUMBO_LEN - 2) {
		ND_PRINT("(jumbo: invalid len %u)", GET_U_1(bp + i + 1));
		goto trunc;
	    }
	    jumbolen = GET_BE_U_4(bp + i + 2);
	    if (found_jumbo) {
		/* More than one Jumbo Payload option */
		if (ndo->ndo_vflag)
		    ND_PRINT("(jumbo: %u - already seen) ", jumbolen);
	    } else {
		found_jumbo = 1;
		if (payload_len == NULL) {
		    /* Not a hop-by-hop option - not valid */
		    if (ndo->ndo_vflag)
			ND_PRINT("(jumbo: %u - not a hop-by-hop option) ", jumbolen);
		} else if (*payload_len != 0) {
		    /* Payload length was non-zero - not valid */
		    if (ndo->ndo_vflag)
			ND_PRINT("(jumbo: %u - payload len != 0) ", jumbolen);
		} else {
		    /*
		     * This is a hop-by-hop option, and Payload length
		     * was zero in the IPv6 header.
		     */
		    if (jumbolen < 65536) {
			/* Too short */
			if (ndo->ndo_vflag)
			    ND_PRINT("(jumbo: %u - < 65536) ", jumbolen);
		    } else {
			/* OK, this is valid */
			*found_jumbop = 1;
			*payload_len = jumbolen;
			if (ndo->ndo_vflag)
			    ND_PRINT("(jumbo: %u) ", jumbolen);
		    }
		}
	    }
	    break;
        case IP6OPT_HOME_ADDRESS:
	    if (len - i < IP6OPT_HOMEADDR_MINLEN) {
		ND_PRINT("(homeaddr: trunc)");
		goto trunc;
	    }
	    if (GET_U_1(bp + i + 1) < IP6OPT_HOMEADDR_MINLEN - 2) {
		ND_PRINT("(homeaddr: invalid len %u)", GET_U_1(bp + i + 1));
		goto trunc;
	    }
	    if (ndo->ndo_vflag) {
		ND_PRINT("(homeaddr: %s", GET_IP6ADDR_STRING(bp + i + 2));
		if (GET_U_1(bp + i + 1) > IP6OPT_HOMEADDR_MINLEN - 2) {
		    if (ip6_sopt_print(ndo, bp + i + IP6OPT_HOMEADDR_MINLEN,
				       (optlen - IP6OPT_HOMEADDR_MINLEN)) == -1)
			goto trunc;
		}
		ND_PRINT(")");
	    }
	    break;
	default:
	    if (len - i < IP6OPT_MINLEN) {
		ND_PRINT("(type %u: trunc)", GET_U_1(bp + i));
		goto trunc;
	    }
	    if (ndo->ndo_vflag)
		ND_PRINT("(opt_type 0x%02x: len=%u)", GET_U_1(bp + i),
			 GET_U_1(bp + i + 1));
	    break;
	}
    }
    if (ndo->ndo_vflag)
        ND_PRINT(" ");
    return 0;

trunc:
    return -1;
}

int
hbhopt_process(netdissect_options *ndo, const u_char *bp, int *found_jumbo,
	       uint32_t *jumbolen)
{
    const struct ip6_hbh *dp = (const struct ip6_hbh *)bp;
    u_int hbhlen = 0;

    ndo->ndo_protocol = "hbhopt";
    hbhlen = (GET_U_1(dp->ip6h_len) + 1) << 3;
    ND_TCHECK_LEN(dp, hbhlen);
    ND_PRINT("HBH ");
    if (ip6_opt_process(ndo, (const u_char *)dp + sizeof(*dp),
			hbhlen - sizeof(*dp), found_jumbo, jumbolen) == -1)
	goto trunc;
    return hbhlen;

trunc:
    nd_print_trunc(ndo);
    return -1;
}

int
dstopt_process(netdissect_options *ndo, const u_char *bp)
{
    const struct ip6_dest *dp = (const struct ip6_dest *)bp;
    u_int dstoptlen = 0;

    ndo->ndo_protocol = "dstopt";
    dstoptlen = (GET_U_1(dp->ip6d_len) + 1) << 3;
    ND_TCHECK_LEN(dp, dstoptlen);
    ND_PRINT("DSTOPT ");
    if (ndo->ndo_vflag) {
	/*
	 * The Jumbo Payload option is a hop-by-hop option; we don't
	 * honor Jumbo Payload destination options, reporting them
	 * as invalid.
	 */
	if (ip6_opt_process(ndo, (const u_char *)dp + sizeof(*dp),
			    dstoptlen - sizeof(*dp), NULL, NULL) == -1)
	    goto trunc;
    }

    return dstoptlen;

trunc:
    nd_print_trunc(ndo);
    return -1;
}
