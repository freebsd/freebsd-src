/*	$KAME: ip_ecn.c,v 1.12 2002/01/07 11:34:47 kjc Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1999 WIDE Project.
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
 *
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/errno.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <netinet/ip_ecn.h>
#ifdef INET6
#include <netinet6/ip6_ecn.h>
#endif

/*
 * ECN and TOS (or TCLASS) processing rules at tunnel encapsulation and
 * decapsulation from RFC6040:
 *
 *                      Outer Hdr at                 Inner Hdr at
 *                      Encapsulator                 Decapsulator
 *   Header fields:     --------------------         ------------
 *     DS Field         copied from inner hdr        no change
 *     ECN Field        constructed by (I)           constructed by (E)
 *
 * ECN_ALLOWED (normal mode):
 *    (I) copy the ECN field to the outer header.
 *
 *    (E) if the ECN field in the outer header is set to CE and the ECN
 *    field of the inner header is not-ECT, drop the packet.
 *    If the ECN field in the inner header is set to ECT(0) and the ECN
 *    field in the outer header is set to ECT(1), copy ECT(1) to
 *    the inner header. If the ECN field in the inner header is set
 *    to ECT(0) or ECT(1) and the ECN field in the outer header is set to
 *    CE, copy CE to the inner header.
 *    Otherwise, make no change to the inner header. This behaviour can be
 *    summarized in the table below:
 *
 *                      Outer Header at Decapsulator
 *               +---------+------------+------------+------------+
 *               | Not-ECT | ECT(0)     | ECT(1)     |     CE     |
 *    Inner Hdr: +---------+------------+------------+------------+
 *      Not-ECT  | Not-ECT |Not-ECT(!!!)|Not-ECT(!!!)| <drop>(!!!)|
 *       ECT(0)  |  ECT(0) | ECT(0)     | ECT(1)     |     CE     |
 *       ECT(1)  |  ECT(1) | ECT(1) (!) | ECT(1)     |     CE     |
 *         CE    |      CE |     CE     |     CE(!!!)|     CE     |
 *               +---------+------------+------------+------------+
 *
 * ECN_COMPLETE (normal mode with security log):
 *    certain combinations indicated in table by '(!!!)' or '(!)',
 *    where '(!!!)' means the combination always potentially dangerous which
 *    returns 3, while '(!)' means possibly dangerous in which returns 2.
 *    These combinations are unsed by previous ECN tunneling specifications
 *    and could be logged. Also, in case of more dangerous ones, the
 *    decapsulator SHOULD log the event and MAY also raise an alarm.
 *
 *    Note: Caller SHOULD use rate-limited alarms so that the anomalous
 *    combinations will not amplify into a flood of alarm messages.
 *    Also, it MUST be possible to suppress alarms or logging.
 *
 * ECN_FORBIDDEN (compatibility mode):
 *    (I) set the ECN field to not-ECT in the outer header.
 *
 *    (E) if the ECN field in the outer header is set to CE, drop the packet.
 *    otherwise, make no change to the ECN field in the inner header.
 *
 * the drop rule is for backward compatibility and protection against
 * erasure of CE.
 */

/*
 * modify outer ECN (TOS) field on ingress operation (tunnel encapsulation).
 */
void
ip_ecn_ingress(int mode, uint8_t *outer, const uint8_t *inner)
{

	KASSERT(outer != NULL && inner != NULL,
	    ("NULL pointer passed to %s", __func__));

	*outer = *inner;
	switch (mode) {
	case ECN_COMPLETE:
	case ECN_ALLOWED:
		/* normal mode: always copy the ECN field. */
		break;

	case ECN_FORBIDDEN:
		/* compatibility mode: set not-ECT to the outer */
		*outer &= ~IPTOS_ECN_MASK;
		break;

	case ECN_NOCARE:
		break;
	}
}

/*
 * modify inner ECN (TOS) field on egress operation (tunnel decapsulation).
 * the caller should drop the packet if the return value is 0.
 */
int
ip_ecn_egress(int mode, const uint8_t *outer, uint8_t *inner)
{

	KASSERT(outer != NULL && inner != NULL,
	    ("NULL pointer passed to %s", __func__));

	switch (mode) {
	case ECN_COMPLETE:
		if ((*outer & IPTOS_ECN_MASK) == IPTOS_ECN_ECT0) {
			/* if the outer is ECT(0) and inner is ECT(1) raise a warning */
			if ((*inner & IPTOS_ECN_MASK) == IPTOS_ECN_ECT1)
				return (ECN_WARN);
			/* if the inner is not-ECT and outer is ECT(0) raise an alarm */
			if ((*inner & IPTOS_ECN_MASK) == IPTOS_ECN_NOTECT)
				return (ECN_ALARM);
			return (ECN_SUCCESS);
		} else if ((*outer & IPTOS_ECN_MASK) == IPTOS_ECN_ECT1) {
			/* if the outer is ECT(1) and inner is CE or ECT(1), raise an alarm */
			if (((*inner & IPTOS_ECN_MASK) == IPTOS_ECN_CE) ||
			    ((*inner & IPTOS_ECN_MASK) == IPTOS_ECN_NOTECT))
				return (ECN_ALARM);
			/* if the outer is ECT(1) and inner is ECT(0), copy ECT(1) */
			if ((*inner & IPTOS_ECN_MASK) == IPTOS_ECN_ECT0)
				*inner = IPTOS_ECN_ECT1;
			return (ECN_SUCCESS);
		}
		/* fallthrough */
	case ECN_ALLOWED:
		if ((*outer & IPTOS_ECN_MASK) == IPTOS_ECN_CE) {
			/* if the outer is CE and the inner is not-ECT, drop it. */
			if ((*inner & IPTOS_ECN_MASK) == IPTOS_ECN_NOTECT)
				return (ECN_DROP);
			/* otherwise, copy CE */
			*inner |= IPTOS_ECN_CE;
		} else if ((*outer & IPTOS_ECN_MASK) == IPTOS_ECN_ECT1) {
			/* if the outer is ECT(1) and inner is ECT(0), copy ECT(1) */
			if ((*inner & IPTOS_ECN_MASK) == IPTOS_ECN_ECT0)
				*inner = IPTOS_ECN_ECT1;
		}
		break;

	case ECN_FORBIDDEN:
		/*
		 * compatibility mode: if the outer is CE, should drop it.
		 * otherwise, leave the inner.
		 */
		if ((*outer & IPTOS_ECN_MASK) == IPTOS_ECN_CE)
			return (ECN_DROP);
		break;

	case ECN_NOCARE:
		break;
	}
	return (ECN_SUCCESS);
}

#ifdef INET6
void
ip6_ecn_ingress(int mode, uint32_t *outer, const uint32_t *inner)
{
	uint8_t outer8, inner8;

	KASSERT(outer != NULL && inner != NULL,
	    ("NULL pointer passed to %s", __func__));

	inner8 = (ntohl(*inner) >> IPV6_FLOWLABEL_LEN) & 0xff;
	ip_ecn_ingress(mode, &outer8, &inner8);
	*outer &= ~htonl(0xff << IPV6_FLOWLABEL_LEN);
	*outer |= htonl((uint32_t)outer8 << IPV6_FLOWLABEL_LEN);
}

int
ip6_ecn_egress(int mode, const uint32_t *outer, uint32_t *inner)
{
	uint8_t outer8, inner8, oinner8;
	int ret;

	KASSERT(outer != NULL && inner != NULL,
	    ("NULL pointer passed to %s", __func__));

	outer8 = (ntohl(*outer) >> IPV6_FLOWLABEL_LEN) & 0xff;
	inner8 = oinner8 = (ntohl(*inner) >> IPV6_FLOWLABEL_LEN) & 0xff;

	ret = ip_ecn_egress(mode, &outer8, &inner8);
	if (ret == ECN_DROP)
		return (ECN_DROP);
	if (inner8 != oinner8) {
		*inner &= ~htonl(0xff << IPV6_FLOWLABEL_LEN);
		*inner |= htonl((uint32_t)inner8 << IPV6_FLOWLABEL_LEN);
	}
	return (ret);
}
#endif
