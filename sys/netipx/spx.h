/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)spx.h
 */

#ifndef _NETIPX_SPX_H_
#define _NETIPX_SPX_H_

/*
 * Definitions for IPX style Sequenced Packet Protocol
 */

struct spxhdr {
	u_char	spx_cc;		/* connection control */
	u_char	spx_dt;		/* datastream type */
#define	SPX_SP	0x80		/* system packet */
#define	SPX_SA	0x40		/* send acknowledgement */
#define	SPX_OB	0x20		/* attention (out of band data) */
#define	SPX_EM	0x10		/* end of message */
	u_short	spx_sid;	/* source connection identifier */
	u_short	spx_did;	/* destination connection identifier */
	u_short	spx_seq;	/* sequence number */
	u_short	spx_ack;	/* acknowledge number */
	u_short	spx_alo;	/* allocation number */
};

/*
 * Definitions for NS(tm) Internet Datagram Protocol
 * containing a Sequenced Packet Protocol packet.
 */
struct spx {
	struct ipx	si_i;
	struct spxhdr 	si_s;
};
struct spx_q {
	struct spx_q	*si_next;
	struct spx_q	*si_prev;
};
#define SI(x)	((struct spx *)x)
#define si_sum	si_i.ipx_sum
#define si_len	si_i.ipx_len
#define si_tc	si_i.ipx_tc
#define si_pt	si_i.ipx_pt
#define si_dna	si_i.ipx_dna
#define si_sna	si_i.ipx_sna
#define si_sport	si_i.ipx_sna.x_port
#define si_cc	si_s.spx_cc
#define si_dt	si_s.spx_dt
#define si_sid	si_s.spx_sid
#define si_did	si_s.spx_did
#define si_seq	si_s.spx_seq
#define si_ack	si_s.spx_ack
#define si_alo	si_s.spx_alo

#ifdef KERNEL
int spx_reass(), spx_output();
int spx_usrreq(), spx_usrreq_sp(), spx_ctloutput();
void spx_input(), spx_ctlinput();
void spx_init(), spx_fasttimo(), spx_slowtimo();
void spx_quench(), spx_setpersist(), spx_template(), spx_abort();
struct spxpcb *spx_close(), *spx_usrclosed();
struct spxpcb *spx_disconnect(), *spx_drop();
struct spxpcb *spx_timers();
#endif

#endif
