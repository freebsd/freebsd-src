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
 */

#ifndef _NETIPX_SPX_VAR_H_
#define _NETIPX_SPX_VAR_H_

/*
 * SPX control block, one per connection
 */
struct spxpcb {
	struct	spx_q	s_q;		/* queue for out-of-order receipt */
	struct	ipxpcb	*s_ipxpcb;	/* backpointer to internet pcb */
	u_char	s_state;
	u_char	s_flags;
#define	SF_ACKNOW	0x01		/* Ack peer immediately */
#define	SF_DELACK	0x02		/* Ack, but try to delay it */
#define	SF_HI	0x04			/* Show headers on input */
#define	SF_HO	0x08			/* Show headers on output */
#define	SF_PI	0x10			/* Packet (datagram) interface */
#define SF_WIN	0x20			/* Window info changed */
#define SF_RXT	0x40			/* Rxt info changed */
#define SF_RVD	0x80			/* Calling from read usrreq routine */
	u_short s_mtu;			/* Max packet size for this stream */
/* use sequence fields in headers to store sequence numbers for this
   connection */
	struct	ipx	*s_ipx;
	struct	spxhdr	s_shdr;		/* prototype header to transmit */
#define s_cc s_shdr.spx_cc		/* connection control (for EM bit) */
#define s_dt s_shdr.spx_dt		/* datastream type */
#define s_sid s_shdr.spx_sid		/* source connection identifier */
#define s_did s_shdr.spx_did		/* destination connection identifier */
#define s_seq s_shdr.spx_seq		/* sequence number */
#define s_ack s_shdr.spx_ack		/* acknowledge number */
#define s_alo s_shdr.spx_alo		/* allocation number */
#define s_dport s_ipx->ipx_dna.x_port	/* where we are sending */
	struct spxhdr s_rhdr;		/* last received header (in effect!)*/
	u_short s_rack;			/* their acknowledge number */
	u_short s_ralo;			/* their allocation number */
	u_short s_smax;			/* highest packet # we have sent */
	u_short	s_snxt;			/* which packet to send next */

/* congestion control */
#define	CUNIT	1024			/* scaling for ... */
	int	s_cwnd;			/* Congestion-controlled window */
					/* in packets * CUNIT */
	short	s_swnd;			/* == tcp snd_wnd, in packets */
	short	s_smxw;			/* == tcp max_sndwnd */
					/* difference of two spx_seq's can be
					   no bigger than a short */
	u_short	s_swl1;			/* == tcp snd_wl1 */
	u_short	s_swl2;			/* == tcp snd_wl2 */
	int	s_cwmx;			/* max allowable cwnd */
	int	s_ssthresh;		/* s_cwnd size threshhold for
					 * slow start exponential-to-
					 * linear switch */
/* transmit timing stuff
 * srtt and rttvar are stored as fixed point, for convenience in smoothing.
 * srtt has 3 bits to the right of the binary point, rttvar has 2.
 */
	short	s_idle;			/* time idle */
	short	s_timer[SPXT_NTIMERS];	/* timers */
	short	s_rxtshift;		/* log(2) of rexmt exp. backoff */
	short	s_rxtcur;		/* current retransmit value */
	u_short	s_rtseq;		/* packet being timed */
	short	s_rtt;			/* timer for round trips */
	short	s_srtt;			/* averaged timer */
	short	s_rttvar;		/* variance in round trip time */
	char	s_force;		/* which timer expired */
	char	s_dupacks;		/* counter to intuit xmt loss */

/* out of band data */
	char	s_oobflags;
#define SF_SOOB	0x08			/* sending out of band data */
#define SF_IOOB 0x10			/* receiving out of band data */
	char	s_iobc;			/* input characters */
/* debug stuff */
	u_short	s_want;			/* Last candidate for sending */
	char	s_outx;			/* exit taken from spx_output */
	char	s_inx;			/* exit taken from spx_input */
	u_short	s_flags2;		/* more flags for testing */
#define SF_NEWCALL	0x100		/* for new_recvmsg */
#define SO_NEWCALL	10		/* for new_recvmsg */
};

#define	ipxtospxpcb(np)	((struct spxpcb *)(np)->ipxp_pcb)
#define	sotospxpcb(so)	(ipxtospxpcb(sotoipxpcb(so)))

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
