/*
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
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

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-mpls.c,v 1.2.4.1 2002/05/07 18:36:28 fenner Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "addrtoname.h"
#include "interface.h"
#include "extract.h"			/* must come after interface.h */

#define LABEL_MASK	0xfffff000
#define LABEL_SHIFT	12
#define	EXP_MASK	0x00000e00
#define EXP_SHIFT	9
#define	STACK_MASK	0x00000100
#define STACK_SHIFT	8
#define TTL_MASK	0x000000ff
#define TTL_SHIFT	0

#define MPLS_LABEL(x)	(((x) & LABEL_MASK) >> LABEL_SHIFT)
#define MPLS_EXP(x)	(((x) & EXP_MASK) >> EXP_SHIFT)
#define MPLS_STACK(x)	(((x) & STACK_MASK) >> STACK_SHIFT)
#define MPLS_TTL(x)	(((x) & TTL_MASK) >> TTL_SHIFT)

static const char *mpls_labelname[] = {
/*0*/	"IPv4 explicit NULL", "router alert", "IPv6 explicit NULL",
	"implicit NULL", "rsvd",
/*5*/	"rsvd", "rsvd", "rsvd", "rsvd", "rsvd",
/*10*/	"rsvd", "rsvd", "rsvd", "rsvd", "rsvd",
/*15*/	"rsvd",
};

/*
 * RFC3032: MPLS label stack encoding
 */
void
mpls_print(const u_char *bp, u_int length)
{
	const u_char *p;
	u_int32_t v;

	p = bp;
	printf("MPLS");
	do {
		TCHECK2(*p, sizeof(v));
		v = EXTRACT_32BITS(p);
		printf(" (");	/*)*/
		printf("label 0x%x", MPLS_LABEL(v));
		if (vflag &&
		    MPLS_LABEL(v) < sizeof(mpls_labelname) / sizeof(mpls_labelname[0]))
			printf("(%s)", mpls_labelname[MPLS_LABEL(v)]);
		if (MPLS_EXP(v))
			printf(" exp 0x%x", MPLS_EXP(v));
		if (MPLS_STACK(v))
			printf("[S]");
		printf(" TTL %u", MPLS_TTL(v));
		/*(*/
		printf(")");

		p += sizeof(v);
	} while (!MPLS_STACK(v));

	switch (MPLS_LABEL(v)) {
	case 0:	/* IPv4 explicit NULL label */
		ip_print(p, length - (p - bp));
		break;
#ifdef INET6
	case 2:	/* IPv6 explicit NULL label */
		ip6_print(p, length - (p - bp));
		break;
#endif
	default:
		/*
		 * Since there's no indication of protocol in MPLS label
		 * encoding, we can print nothing further.
		 */
		return;
	}

trunc:
	printf("[|MPLS]");
}
