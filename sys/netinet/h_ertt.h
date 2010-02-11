/*-
 * Copyright (c) 2009-2010
 * 	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by David Hayes and Lawrence Stewart,
 * made possible in part by a grant from the FreeBSD Foundation and
 * Cisco University Research Program Fund at Community Foundation
 * Silicon Valley.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_NETINET_H_ERTT_
#define	_NETINET_H_ERTT_

/* Structure contains the information about the
   sent segment, for comparison with the corresponding ack */
struct txseginfo; 

/* Structure used as the ertt data block. */
struct ertt {
	/* information about transmitted segments to aid in
	   RTT calculation for delay/rate based CC */
	TAILQ_HEAD(txseginfo_head, txseginfo) txsegi_q;
	int rtt;		  /* per packet measured round trip time */
	int maxrtt;             /* maximum seen rtt */
	int minrtt;             /* minimum seen rtt */
	int dlyack_rx;          /* guess if the receiver is using delayed acknowledgements.*/
	int timestamp_errors;   /* for keeping track of inconsistencies in packet timestamps */
	int markedpkt_rtt;      /* rtt for a marked packet */
	long bytes_tx_in_rtt;   /* bytes tx so far in marked rtt */
	long bytes_tx_in_marked_rtt;/* final version of above */
	u_long marked_snd_cwnd; /* cwnd for marked rtt */
	int flags;              /* flags*/
};

#define ERTT_NEW_MEASUREMENT          0x01 /* new measurement */
#define ERTT_MEASUREMENT_IN_PROGRESS  0x02 /* measuring marked RTT */
#define ERTT_TSO_DISABLED             0x04 /* indicates TSO has been temporarily disabled */

#endif /* _NETINET_H_ERTT_ */
