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
 *	@(#)spx_var.h
 *
 * $Id: spx_var.h,v 1.3 1995/10/31 23:36:47 julian Exp $
 */

#ifndef _NETIPX_SPX_VAR_H_
#define _NETIPX_SPX_VAR_H_

struct	spxstat {
	long	spxs_connattempt;	/* connections initiated */
	long	spxs_accepts;		/* connections accepted */
	long	spxs_connects;		/* connections established */
	long	spxs_drops;		/* connections dropped */
	long	spxs_conndrops;		/* embryonic connections dropped */
	long	spxs_closed;		/* conn. closed (includes drops) */
	long	spxs_segstimed;		/* segs where we tried to get rtt */
	long	spxs_rttupdated;	/* times we succeeded */
	long	spxs_delack;		/* delayed acks sent */
	long	spxs_timeoutdrop;	/* conn. dropped in rxmt timeout */
	long	spxs_rexmttimeo;	/* retransmit timeouts */
	long	spxs_persisttimeo;	/* persist timeouts */
	long	spxs_keeptimeo;		/* keepalive timeouts */
	long	spxs_keepprobe;		/* keepalive probes sent */
	long	spxs_keepdrops;		/* connections dropped in keepalive */

	long	spxs_sndtotal;		/* total packets sent */
	long	spxs_sndpack;		/* data packets sent */
	long	spxs_sndbyte;		/* data bytes sent */
	long	spxs_sndrexmitpack;	/* data packets retransmitted */
	long	spxs_sndrexmitbyte;	/* data bytes retransmitted */
	long	spxs_sndacks;		/* ack-only packets sent */
	long	spxs_sndprobe;		/* window probes sent */
	long	spxs_sndurg;		/* packets sent with URG only */
	long	spxs_sndwinup;		/* window update-only packets sent */
	long	spxs_sndctrl;		/* control (SYN|FIN|RST) packets sent */
	long	spxs_sndvoid;		/* couldn't find requested packet*/

	long	spxs_rcvtotal;		/* total packets received */
	long	spxs_rcvpack;		/* packets received in sequence */
	long	spxs_rcvbyte;		/* bytes received in sequence */
	long	spxs_rcvbadsum;		/* packets received with ccksum errs */
	long	spxs_rcvbadoff;		/* packets received with bad offset */
	long	spxs_rcvshort;		/* packets received too short */
	long	spxs_rcvduppack;	/* duplicate-only packets received */
	long	spxs_rcvdupbyte;	/* duplicate-only bytes received */
	long	spxs_rcvpartduppack;	/* packets with some duplicate data */
	long	spxs_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	long	spxs_rcvoopack;		/* out-of-order packets received */
	long	spxs_rcvoobyte;		/* out-of-order bytes received */
	long	spxs_rcvpackafterwin;	/* packets with data after window */
	long	spxs_rcvbyteafterwin;	/* bytes rcvd after window */
	long	spxs_rcvafterclose;	/* packets rcvd after "close" */
	long	spxs_rcvwinprobe;	/* rcvd window probe packets */
	long	spxs_rcvdupack;		/* rcvd duplicate acks */
	long	spxs_rcvacktoomuch;	/* rcvd acks for unsent data */
	long	spxs_rcvackpack;	/* rcvd ack packets */
	long	spxs_rcvackbyte;	/* bytes acked by rcvd acks */
	long	spxs_rcvwinupd;		/* rcvd window update packets */
};
struct	spx_istat {
	short	hdrops;
	short	badsum;
	short	badlen;
	short	slotim;
	short	fastim;
	short	nonucn;
	short	noconn;
	short	notme;
	short	wrncon;
	short	bdreas;
	short	gonawy;
	short	notyet;
	short	lstdup;
	struct spxstat newstats;
};

#ifdef KERNEL
extern struct spx_istat spx_istat;
extern u_short spx_iss;

/* Following was struct spxstat spxstat; */
#ifndef spxstat
#define spxstat spx_istat.newstats
#endif

#endif

#define	SPX_ISSINCR	128
/*
 * spx sequence numbers are 16 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define	SSEQ_LT(a,b)	(((short)((a)-(b))) < 0)
#define	SSEQ_LEQ(a,b)	(((short)((a)-(b))) <= 0)
#define	SSEQ_GT(a,b)	(((short)((a)-(b))) > 0)
#define	SSEQ_GEQ(a,b)	(((short)((a)-(b))) >= 0)

#endif
