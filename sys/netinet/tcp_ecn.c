/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
 *      The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2007-2008,2010
 *      Swinburne University of Technology, Melbourne, Australia.
 * Copyright (c) 2009-2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * Copyright (c) 2010-2011 Juniper Networks, Inc.
 * Copyright (c) 2019 Richard Scheffenegger <srichard@netapp.com>
 * All rights reserved.
 *
 * Portions of this software were developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Lawrence Stewart,
 * James Healy and David Hayes, made possible in part by a grant from the Cisco
 * University Research Program Fund at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by David Hayes under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Robert N. M. Watson under
 * contract to Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)tcp_ecn.c 8.12 (Berkeley) 5/24/95
 */

/*
 * Utility functions to deal with Explicit Congestion Notification in TCP
 * implementing the essential parts of the Accurate ECN extension
 * https://tools.ietf.org/html/draft-ietf-tcpm-accurate-ecn-09
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <machine/cpu.h>

#include <vm/uma.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_syncache.h>
#include <netinet/tcp_timer.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_ecn.h>


/*
 * Process incoming SYN,ACK packet
 */
void
tcp_ecn_input_syn_sent(struct tcpcb *tp, uint16_t thflags, int iptos)
{
	thflags &= (TH_CWR|TH_ECE);

	if (((thflags & (TH_CWR | TH_ECE)) == TH_ECE) &&
	    V_tcp_do_ecn) {
		tp->t_flags2 |= TF2_ECN_PERMIT;
		TCPSTAT_INC(tcps_ecn_shs);
	}
}

/*
 * Handle parallel SYN for ECN
 */
void
tcp_ecn_input_parallel_syn(struct tcpcb *tp, uint16_t thflags, int iptos)
{
	if (thflags & TH_ACK)
		return;
	if (V_tcp_do_ecn == 0)
		return;
	if ((V_tcp_do_ecn == 1) || (V_tcp_do_ecn == 2)) {
		/* RFC3168 ECN handling */
		if ((thflags & (TH_CWR | TH_ECE)) == (TH_CWR | TH_ECE)) {
			tp->t_flags2 |= TF2_ECN_PERMIT;
			tp->t_flags2 |= TF2_ECN_SND_ECE;
			TCPSTAT_INC(tcps_ecn_shs);
		}
	}
}

/*
 * TCP ECN processing.
 */
int
tcp_ecn_input_segment(struct tcpcb *tp, uint16_t thflags, int iptos)
{
	int delta_ace = 0;

	if (tp->t_flags2 & TF2_ECN_PERMIT) {
		switch (iptos & IPTOS_ECN_MASK) {
		case IPTOS_ECN_CE:
			TCPSTAT_INC(tcps_ecn_ce);
			break;
		case IPTOS_ECN_ECT0:
			TCPSTAT_INC(tcps_ecn_ect0);
			break;
		case IPTOS_ECN_ECT1:
			TCPSTAT_INC(tcps_ecn_ect1);
			break;
		}

		/* RFC3168 ECN handling */
		if (thflags & TH_ECE)
			delta_ace = 1;
		if (thflags & TH_CWR) {
			tp->t_flags2 &= ~TF2_ECN_SND_ECE;
			tp->t_flags |= TF_ACKNOW;
		}
		if ((iptos & IPTOS_ECN_MASK) == IPTOS_ECN_CE)
			tp->t_flags2 |= TF2_ECN_SND_ECE;

		/* Process a packet differently from RFC3168. */
		cc_ecnpkt_handler_flags(tp, thflags, iptos);
	}

	return delta_ace;
}

/*
 * Send ECN setup <SYN> packet header flags
 */
uint16_t
tcp_ecn_output_syn_sent(struct tcpcb *tp)
{
	uint16_t thflags = 0;

	if (V_tcp_do_ecn == 1) {
		/* Send a RFC3168 ECN setup <SYN> packet */
		if (tp->t_rxtshift >= 1) {
			if (tp->t_rxtshift <= V_tcp_ecn_maxretries)
				thflags = TH_ECE|TH_CWR;
		} else
			thflags = TH_ECE|TH_CWR;
	}

	return thflags;
}

/*
 * output processing of ECN feature
 * returning IP ECN header codepoint
 */
int
tcp_ecn_output_established(struct tcpcb *tp, uint16_t *thflags, int len, bool rxmit)
{
	int ipecn = IPTOS_ECN_NOTECT;
	bool newdata;

	/*
	 * If the peer has ECN, mark data packets with
	 * ECN capable transmission (ECT).
	 * Ignore pure control packets, retransmissions
	 * and window probes.
	 */
	newdata = (len > 0 && SEQ_GEQ(tp->snd_nxt, tp->snd_max) &&
		    !rxmit &&
		    !((tp->t_flags & TF_FORCEDATA) && len == 1));
	if (newdata) {
		ipecn = IPTOS_ECN_ECT0;
		TCPSTAT_INC(tcps_ecn_ect0);
	}
	/*
	 * Reply with proper ECN notifications.
	 */
	if (newdata &&
	    (tp->t_flags2 & TF2_ECN_SND_CWR)) {
		*thflags |= TH_CWR;
		tp->t_flags2 &= ~TF2_ECN_SND_CWR;
	}
	if (tp->t_flags2 & TF2_ECN_SND_ECE)
		*thflags |= TH_ECE;

	return ipecn;
}

/*
 * Set up the ECN related tcpcb fields from
 * a syncache entry
 */
void
tcp_ecn_syncache_socket(struct tcpcb *tp, struct syncache *sc)
{
	if (sc->sc_flags & SCF_ECN_MASK) {
		switch (sc->sc_flags & SCF_ECN_MASK) {
		case SCF_ECN:
			tp->t_flags2 |= TF2_ECN_PERMIT;
			break;
		/* undefined SCF codepoint */
		default:
			break;
		}
	}
}

/*
 * Process a <SYN> packets ECN information, and provide the
 * syncache with the relevant information.
 */
int
tcp_ecn_syncache_add(uint16_t thflags, int iptos)
{
	int scflags = 0;

	switch (thflags & (TH_CWR|TH_ECE)) {
	/* no ECN */
	case (0|0):
		break;
	/* legacy ECN */
	case (TH_CWR|TH_ECE):
		scflags = SCF_ECN;
		break;
	default:
		break;
	}
	return scflags;
}

/*
 * Set up the ECN information for the <SYN,ACK> from
 * syncache information.
 */
uint16_t
tcp_ecn_syncache_respond(uint16_t thflags, struct syncache *sc)
{
	if ((thflags & TH_SYN) &&
	    (sc->sc_flags & SCF_ECN_MASK)) {
		switch (sc->sc_flags & SCF_ECN_MASK) {
		case SCF_ECN:
			thflags |= (0 | TH_ECE);
			TCPSTAT_INC(tcps_ecn_shs);
			break;
		/* undefined SCF codepoint */
		default:
			break;
		}
	}
	return thflags;
}
