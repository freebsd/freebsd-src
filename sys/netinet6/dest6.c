/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 *	$KAME: dest6.c,v 1.59 2003/07/11 13:21:16 t-momose Exp $
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

/*
 * Destination options header processing.
 */
int
dest6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m;
	int off, dstoptlen, optlen;
	struct ip6_dest *dstopts;
	u_int8_t *opt;

	m = *mp;
	off = *offp;

	/* Validation of the length of the header. */
	if (m->m_len < off + sizeof(*dstopts)) {
		m = m_pullup(m, off + sizeof(*dstopts));
		if (m == NULL) {
			IP6STAT_INC(ip6s_exthdrtoolong);
			*mp = m;
			return (IPPROTO_DONE);
		}
	}
	dstopts = (struct ip6_dest *)(mtod(m, caddr_t) + off);
	dstoptlen = (dstopts->ip6d_len + 1) << 3;

	if (m->m_len < off + dstoptlen) {
		m = m_pullup(m, off + dstoptlen);
		if (m == NULL) {
			IP6STAT_INC(ip6s_exthdrtoolong);
			*mp = m;
			return (IPPROTO_DONE);
		}
	}
	dstopts = (struct ip6_dest *)(mtod(m, caddr_t) + off);
	off += dstoptlen;
	dstoptlen -= sizeof(struct ip6_dest);
	opt = (u_int8_t *)dstopts + sizeof(struct ip6_dest);

	/* search header for all options. */
	for (; dstoptlen > 0; dstoptlen -= optlen, opt += optlen) {
		if (*opt != IP6OPT_PAD1 &&
		    (dstoptlen < IP6OPT_MINLEN || *(opt + 1) + 2 > dstoptlen)) {
			IP6STAT_INC(ip6s_toosmall);
			goto bad;
		}

		switch (*opt) {
		case IP6OPT_PAD1:
			optlen = 1;
			break;
		case IP6OPT_PADN:
			optlen = *(opt + 1) + 2;
			break;
		default:		/* unknown option */
			optlen = ip6_unknown_opt(opt, m,
			    opt - mtod(m, u_int8_t *));
			if (optlen == -1) {
				*mp = NULL;
				return (IPPROTO_DONE);
			}
			optlen += 2;
			break;
		}
	}

	*offp = off;
	*mp = m;
	return (dstopts->ip6d_nxt);

  bad:
	m_freem(m);
	*mp = NULL;
	return (IPPROTO_DONE);
}
