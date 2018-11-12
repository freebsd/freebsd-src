/*-
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
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
 *	@(#)tcp_var.h	8.4 (Berkeley) 5/24/95
 * $FreeBSD$
 */

#ifndef _NETINET_TCP_VAR_H_
#define _NETINET_TCP_VAR_H_

#include <netinet/tcp.h>

#ifdef _KERNEL
#include <net/vnet.h>

/*
 * Kernel variables for tcp.
 */
VNET_DECLARE(int, tcp_do_rfc1323);
#define	V_tcp_do_rfc1323	VNET(tcp_do_rfc1323)

#endif /* _KERNEL */

/* TCP segment queue entry */
struct tseg_qent {
	LIST_ENTRY(tseg_qent) tqe_q;
	int	tqe_len;		/* TCP segment data length */
	struct	tcphdr *tqe_th;		/* a pointer to tcp header */
	struct	mbuf	*tqe_m;		/* mbuf contains packet */
};
LIST_HEAD(tsegqe_head, tseg_qent);

struct sackblk {
	tcp_seq start;		/* start seq no. of sack block */
	tcp_seq end;		/* end seq no. */
};

struct sackhole {
	tcp_seq start;		/* start seq no. of hole */
	tcp_seq end;		/* end seq no. */
	tcp_seq rxmit;		/* next seq. no in hole to be retransmitted */
	TAILQ_ENTRY(sackhole) scblink;	/* scoreboard linkage */
};

struct sackhint {
	struct sackhole	*nexthole;
	int		sack_bytes_rexmit;
	tcp_seq		last_sack_ack;	/* Most recent/largest sacked ack */

	int		ispare;		/* explicit pad for 64bit alignment */
	uint64_t	_pad[2];	/* 1 sacked_bytes, 1 TBD */
};

struct tcptemp {
	u_char	tt_ipgen[40]; /* the size must be of max ip header, now IPv6 */
	struct	tcphdr tt_t;
};

#define tcp6cb		tcpcb  /* for KAME src sync over BSD*'s */

/*
 * Tcp control block, one per tcp; fields:
 * Organized for 16 byte cacheline efficiency.
 */
struct tcpcb {
	struct	tsegqe_head t_segq;	/* segment reassembly queue */
	void	*t_pspare[2];		/* new reassembly queue */
	int	t_segqlen;		/* segment reassembly queue length */
	int	t_dupacks;		/* consecutive dup acks recd */

	struct tcp_timer *t_timers;	/* All the TCP timers in one struct */

	struct	inpcb *t_inpcb;		/* back pointer to internet pcb */
	int	t_state;		/* state of this connection */
	u_int	t_flags;

	struct	vnet *t_vnet;		/* back pointer to parent vnet */

	tcp_seq	snd_una;		/* sent but unacknowledged */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */

	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	tcp_seq	irs;			/* initial receive sequence number */

	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_adv;		/* advertised window */
	u_long	rcv_wnd;		/* receive window */
	tcp_seq	rcv_up;			/* receive urgent pointer */

	u_long	snd_wnd;		/* send window */
	u_long	snd_cwnd;		/* congestion-controlled window */
	u_long	snd_spare1;		/* unused */
	u_long	snd_ssthresh;		/* snd_cwnd size threshold for
					 * for slow start exponential to
					 * linear switch
					 */
	u_long	snd_spare2;		/* unused */
	tcp_seq	snd_recover;		/* for use in NewReno Fast Recovery */

	u_int	t_maxopd;		/* mss plus options */

	u_int	t_rcvtime;		/* inactivity time */
	u_int	t_starttime;		/* time connection was established */
	u_int	t_rtttime;		/* RTT measurement start time */
	tcp_seq	t_rtseq;		/* sequence number being timed */

	u_int	t_bw_spare1;		/* unused */
	tcp_seq	t_bw_spare2;		/* unused */

	int	t_rxtcur;		/* current retransmit value (ticks) */
	u_int	t_maxseg;		/* maximum segment size */
	int	t_srtt;			/* smoothed round-trip time */
	int	t_rttvar;		/* variance in round-trip time */

	int	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	u_int	t_rttmin;		/* minimum rtt allowed */
	u_int	t_rttbest;		/* best rtt we've seen */
	u_long	t_rttupdated;		/* number of times rtt sampled */
	u_long	max_sndwnd;		/* largest window peer has offered */

	int	t_softerror;		/* possible error not yet reported */
/* out-of-band data */
	char	t_oobflags;		/* have some */
	char	t_iobc;			/* input character */
/* RFC 1323 variables */
	u_char	snd_scale;		/* window scaling for send window */
	u_char	rcv_scale;		/* window scaling for recv window */
	u_char	request_r_scale;	/* pending window scaling */
	u_int32_t  ts_recent;		/* timestamp echo data */
	u_int	ts_recent_age;		/* when last updated */
	u_int32_t  ts_offset;		/* our timestamp offset */

	tcp_seq	last_ack_sent;
/* experimental */
	u_long	snd_cwnd_prev;		/* cwnd prior to retransmit */
	u_long	snd_ssthresh_prev;	/* ssthresh prior to retransmit */
	tcp_seq	snd_recover_prev;	/* snd_recover prior to retransmit */
	int	t_sndzerowin;		/* zero-window updates sent */
	u_int	t_badrxtwin;		/* window for retransmit recovery */
	u_char	snd_limited;		/* segments limited transmitted */
/* SACK related state */
	int	snd_numholes;		/* number of holes seen by sender */
	TAILQ_HEAD(sackhole_head, sackhole) snd_holes;
					/* SACK scoreboard (sorted) */
	tcp_seq	snd_fack;		/* last seq number(+1) sack'd by rcv'r*/
	int	rcv_numsacks;		/* # distinct sack blks present */
	struct sackblk sackblks[MAX_SACK_BLKS]; /* seq nos. of sack blocks */
	tcp_seq sack_newdata;		/* New data xmitted in this recovery
					   episode starts at this seq number */
	struct sackhint	sackhint;	/* SACK scoreboard hint */
	int	t_rttlow;		/* smallest observerved RTT */
	u_int32_t	rfbuf_ts;	/* recv buffer autoscaling timestamp */
	int	rfbuf_cnt;		/* recv buffer autoscaling byte count */
	struct toedev	*tod;		/* toedev handling this connection */
	int	t_sndrexmitpack;	/* retransmit packets sent */
	int	t_rcvoopack;		/* out-of-order packets received */
	void	*t_toe;			/* TOE pcb pointer */
	int	t_bytes_acked;		/* # bytes acked during current RTT */
	struct cc_algo	*cc_algo;	/* congestion control algorithm */
	struct cc_var	*ccv;		/* congestion control specific vars */
	struct osd	*osd;		/* storage for Khelp module data */

	u_int	t_keepinit;		/* time to establish connection */
	u_int	t_keepidle;		/* time before keepalive probes begin */
	u_int	t_keepintvl;		/* interval between keepalives */
	u_int	t_keepcnt;		/* number of keepalives before close */

	u_int	t_tsomax;		/* TSO total burst length limit in bytes */
	u_int	t_tsomaxsegcount;	/* TSO maximum segment count */
	u_int	t_tsomaxsegsize;	/* TSO maximum segment size in bytes */
	u_int	t_pmtud_saved_maxopd;	/* pre-blackhole MSS */
	u_int	t_flags2;		/* More tcpcb flags storage */

	uint32_t t_ispare[8];		/* 5 UTO, 3 TBD */
	void	*t_pspare2[4];		/* 1 TCP_SIGNATURE, 3 TBD */
	uint64_t _pad[6];		/* 6 TBD (1-2 CC/RTT?) */
};

/*
 * Flags and utility macros for the t_flags field.
 */
#define	TF_ACKNOW	0x000001	/* ack peer immediately */
#define	TF_DELACK	0x000002	/* ack, but try to delay it */
#define	TF_NODELAY	0x000004	/* don't delay packets to coalesce */
#define	TF_NOOPT	0x000008	/* don't use tcp options */
#define	TF_SENTFIN	0x000010	/* have sent FIN */
#define	TF_REQ_SCALE	0x000020	/* have/will request window scaling */
#define	TF_RCVD_SCALE	0x000040	/* other side has requested scaling */
#define	TF_REQ_TSTMP	0x000080	/* have/will request timestamps */
#define	TF_RCVD_TSTMP	0x000100	/* a timestamp was received in SYN */
#define	TF_SACK_PERMIT	0x000200	/* other side said I could SACK */
#define	TF_NEEDSYN	0x000400	/* send SYN (implicit state) */
#define	TF_NEEDFIN	0x000800	/* send FIN (implicit state) */
#define	TF_NOPUSH	0x001000	/* don't push */
#define	TF_PREVVALID	0x002000	/* saved values for bad rxmit valid */
#define	TF_MORETOCOME	0x010000	/* More data to be appended to sock */
#define	TF_LQ_OVERFLOW	0x020000	/* listen queue overflow */
#define	TF_LASTIDLE	0x040000	/* connection was previously idle */
#define	TF_RXWIN0SENT	0x080000	/* sent a receiver win 0 in response */
#define	TF_FASTRECOVERY	0x100000	/* in NewReno Fast Recovery */
#define	TF_WASFRECOVERY	0x200000	/* was in NewReno Fast Recovery */
#define	TF_SIGNATURE	0x400000	/* require MD5 digests (RFC2385) */
#define	TF_FORCEDATA	0x800000	/* force out a byte */
#define	TF_TSO		0x1000000	/* TSO enabled on this connection */
#define	TF_TOE		0x2000000	/* this connection is offloaded */
#define	TF_ECN_PERMIT	0x4000000	/* connection ECN-ready */
#define	TF_ECN_SND_CWR	0x8000000	/* ECN CWR in queue */
#define	TF_ECN_SND_ECE	0x10000000	/* ECN ECE in queue */
#define	TF_CONGRECOVERY	0x20000000	/* congestion recovery mode */
#define	TF_WASCRECOVERY	0x40000000	/* was in congestion recovery */

#define	IN_FASTRECOVERY(t_flags)	(t_flags & TF_FASTRECOVERY)
#define	ENTER_FASTRECOVERY(t_flags)	t_flags |= TF_FASTRECOVERY
#define	EXIT_FASTRECOVERY(t_flags)	t_flags &= ~TF_FASTRECOVERY

#define	IN_CONGRECOVERY(t_flags)	(t_flags & TF_CONGRECOVERY)
#define	ENTER_CONGRECOVERY(t_flags)	t_flags |= TF_CONGRECOVERY
#define	EXIT_CONGRECOVERY(t_flags)	t_flags &= ~TF_CONGRECOVERY

#define	IN_RECOVERY(t_flags) (t_flags & (TF_CONGRECOVERY | TF_FASTRECOVERY))
#define	ENTER_RECOVERY(t_flags) t_flags |= (TF_CONGRECOVERY | TF_FASTRECOVERY)
#define	EXIT_RECOVERY(t_flags) t_flags &= ~(TF_CONGRECOVERY | TF_FASTRECOVERY)

#define	BYTES_THIS_ACK(tp, th)	(th->th_ack - tp->snd_una)

/*
 * Flags for the t_oobflags field.
 */
#define	TCPOOB_HAVEDATA	0x01
#define	TCPOOB_HADDATA	0x02

#ifdef TCP_SIGNATURE
/*
 * Defines which are needed by the xform_tcp module and tcp_[in|out]put
 * for SADB verification and lookup.
 */
#define	TCP_SIGLEN	16	/* length of computed digest in bytes */
#define	TCP_KEYLEN_MIN	1	/* minimum length of TCP-MD5 key */
#define	TCP_KEYLEN_MAX	80	/* maximum length of TCP-MD5 key */
/*
 * Only a single SA per host may be specified at this time. An SPI is
 * needed in order for the KEY_ALLOCSA() lookup to work.
 */
#define	TCP_SIG_SPI	0x1000
#endif /* TCP_SIGNATURE */

/*
 * Flags for PLPMTU handling, t_flags2
 */
#define	TF2_PLPMTU_BLACKHOLE	0x00000001 /* Possible PLPMTUD Black Hole. */
#define	TF2_PLPMTU_PMTUD	0x00000002 /* Allowed to attempt PLPMTUD. */
#define	TF2_PLPMTU_MAXSEGSNT	0x00000004 /* Last seg sent was full seg. */

/*
 * Structure to hold TCP options that are only used during segment
 * processing (in tcp_input), but not held in the tcpcb.
 * It's basically used to reduce the number of parameters
 * to tcp_dooptions and tcp_addoptions.
 * The binary order of the to_flags is relevant for packing of the
 * options in tcp_addoptions.
 */
struct tcpopt {
	u_int64_t	to_flags;	/* which options are present */
#define	TOF_MSS		0x0001		/* maximum segment size */
#define	TOF_SCALE	0x0002		/* window scaling */
#define	TOF_SACKPERM	0x0004		/* SACK permitted */
#define	TOF_TS		0x0010		/* timestamp */
#define	TOF_SIGNATURE	0x0040		/* TCP-MD5 signature option (RFC2385) */
#define	TOF_SACK	0x0080		/* Peer sent SACK option */
#define	TOF_MAXOPT	0x0100
	u_int32_t	to_tsval;	/* new timestamp */
	u_int32_t	to_tsecr;	/* reflected timestamp */
	u_char		*to_sacks;	/* pointer to the first SACK blocks */
	u_char		*to_signature;	/* pointer to the TCP-MD5 signature */
	u_int16_t	to_mss;		/* maximum segment size */
	u_int8_t	to_wscale;	/* window scaling */
	u_int8_t	to_nsacks;	/* number of SACK blocks */
	u_int32_t	to_spare;	/* UTO */
};

/*
 * Flags for tcp_dooptions.
 */
#define	TO_SYN		0x01		/* parse SYN-only options */

struct hc_metrics_lite {	/* must stay in sync with hc_metrics */
	u_long	rmx_mtu;	/* MTU for this path */
	u_long	rmx_ssthresh;	/* outbound gateway buffer limit */
	u_long	rmx_rtt;	/* estimated round trip time */
	u_long	rmx_rttvar;	/* estimated rtt variance */
	u_long	rmx_bandwidth;	/* estimated bandwidth */
	u_long	rmx_cwnd;	/* congestion window */
	u_long	rmx_sendpipe;   /* outbound delay-bandwidth product */
	u_long	rmx_recvpipe;   /* inbound delay-bandwidth product */
};

/*
 * Used by tcp_maxmtu() to communicate interface specific features
 * and limits at the time of connection setup.
 */
struct tcp_ifcap {
	int	ifcap;
	u_int	tsomax;
	u_int	tsomaxsegcount;
	u_int	tsomaxsegsize;
};

#ifndef _NETINET_IN_PCB_H_
struct in_conninfo;
#endif /* _NETINET_IN_PCB_H_ */

struct tcptw {
	struct inpcb	*tw_inpcb;	/* XXX back pointer to internet pcb */
	tcp_seq		snd_nxt;
	tcp_seq		rcv_nxt;
	tcp_seq		iss;
	tcp_seq		irs;
	u_short		last_win;	/* cached window value */
	u_short		tw_so_options;	/* copy of so_options */
	struct ucred	*tw_cred;	/* user credentials */
	u_int32_t	t_recent;
	u_int32_t	ts_offset;	/* our timestamp offset */
	u_int		t_starttime;
	int		tw_time;
	TAILQ_ENTRY(tcptw) tw_2msl;
	void		*tw_pspare;	/* TCP_SIGNATURE */
	u_int		*tw_spare;	/* TCP_SIGNATURE */
};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	intotw(ip)	((struct tcptw *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		32	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		5	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	16	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	4	/* shift for rttvar; 2 bits */
#define	TCP_DELTA_SHIFT		2	/* see tcp_input.c */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This version of the macro adapted from a paper by Lawrence
 * Brakmo and Larry Peterson which outlines a problem caused
 * by insufficient precision in the original implementation,
 * which results in inappropriately large RTO values for very
 * fast networks.
 */
#define	TCP_REXMTVAL(tp) \
	max((tp)->t_rttmin, (((tp)->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT))  \
	  + (tp)->t_rttvar) >> TCP_DELTA_SHIFT)

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {
	uint64_t tcps_connattempt;	/* connections initiated */
	uint64_t tcps_accepts;		/* connections accepted */
	uint64_t tcps_connects;		/* connections established */
	uint64_t tcps_drops;		/* connections dropped */
	uint64_t tcps_conndrops;	/* embryonic connections dropped */
	uint64_t tcps_minmssdrops;	/* average minmss too low drops */
	uint64_t tcps_closed;		/* conn. closed (includes drops) */
	uint64_t tcps_segstimed;	/* segs where we tried to get rtt */
	uint64_t tcps_rttupdated;	/* times we succeeded */
	uint64_t tcps_delack;		/* delayed acks sent */
	uint64_t tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	uint64_t tcps_rexmttimeo;	/* retransmit timeouts */
	uint64_t tcps_persisttimeo;	/* persist timeouts */
	uint64_t tcps_keeptimeo;	/* keepalive timeouts */
	uint64_t tcps_keepprobe;	/* keepalive probes sent */
	uint64_t tcps_keepdrops;	/* connections dropped in keepalive */

	uint64_t tcps_sndtotal;		/* total packets sent */
	uint64_t tcps_sndpack;		/* data packets sent */
	uint64_t tcps_sndbyte;		/* data bytes sent */
	uint64_t tcps_sndrexmitpack;	/* data packets retransmitted */
	uint64_t tcps_sndrexmitbyte;	/* data bytes retransmitted */
	uint64_t tcps_sndrexmitbad;	/* unnecessary packet retransmissions */
	uint64_t tcps_sndacks;		/* ack-only packets sent */
	uint64_t tcps_sndprobe;		/* window probes sent */
	uint64_t tcps_sndurg;		/* packets sent with URG only */
	uint64_t tcps_sndwinup;		/* window update-only packets sent */
	uint64_t tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	uint64_t tcps_rcvtotal;		/* total packets received */
	uint64_t tcps_rcvpack;		/* packets received in sequence */
	uint64_t tcps_rcvbyte;		/* bytes received in sequence */
	uint64_t tcps_rcvbadsum;	/* packets received with ccksum errs */
	uint64_t tcps_rcvbadoff;	/* packets received with bad offset */
	uint64_t tcps_rcvreassfull;	/* packets dropped for no reass space */
	uint64_t tcps_rcvshort;		/* packets received too short */
	uint64_t tcps_rcvduppack;	/* duplicate-only packets received */
	uint64_t tcps_rcvdupbyte;	/* duplicate-only bytes received */
	uint64_t tcps_rcvpartduppack;	/* packets with some duplicate data */
	uint64_t tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	uint64_t tcps_rcvoopack;	/* out-of-order packets received */
	uint64_t tcps_rcvoobyte;	/* out-of-order bytes received */
	uint64_t tcps_rcvpackafterwin;	/* packets with data after window */
	uint64_t tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	uint64_t tcps_rcvafterclose;	/* packets rcvd after "close" */
	uint64_t tcps_rcvwinprobe;	/* rcvd window probe packets */
	uint64_t tcps_rcvdupack;	/* rcvd duplicate acks */
	uint64_t tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	uint64_t tcps_rcvackpack;	/* rcvd ack packets */
	uint64_t tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	uint64_t tcps_rcvwinupd;	/* rcvd window update packets */
	uint64_t tcps_pawsdrop;		/* segments dropped due to PAWS */
	uint64_t tcps_predack;		/* times hdr predict ok for acks */
	uint64_t tcps_preddat;		/* times hdr predict ok for data pkts */
	uint64_t tcps_pcbcachemiss;
	uint64_t tcps_cachedrtt;	/* times cached RTT in route updated */
	uint64_t tcps_cachedrttvar;	/* times cached rttvar updated */
	uint64_t tcps_cachedssthresh;	/* times cached ssthresh updated */
	uint64_t tcps_usedrtt;		/* times RTT initialized from route */
	uint64_t tcps_usedrttvar;	/* times RTTVAR initialized from rt */
	uint64_t tcps_usedssthresh;	/* times ssthresh initialized from rt*/
	uint64_t tcps_persistdrop;	/* timeout in persist state */
	uint64_t tcps_badsyn;		/* bogus SYN, e.g. premature ACK */
	uint64_t tcps_mturesent;	/* resends due to MTU discovery */
	uint64_t tcps_listendrop;	/* listen queue overflows */
	uint64_t tcps_badrst;		/* ignored RSTs in the window */

	uint64_t tcps_sc_added;		/* entry added to syncache */
	uint64_t tcps_sc_retransmitted;	/* syncache entry was retransmitted */
	uint64_t tcps_sc_dupsyn;	/* duplicate SYN packet */
	uint64_t tcps_sc_dropped;	/* could not reply to packet */
	uint64_t tcps_sc_completed;	/* successful extraction of entry */
	uint64_t tcps_sc_bucketoverflow;/* syncache per-bucket limit hit */
	uint64_t tcps_sc_cacheoverflow;	/* syncache cache limit hit */
	uint64_t tcps_sc_reset;		/* RST removed entry from syncache */
	uint64_t tcps_sc_stale;		/* timed out or listen socket gone */
	uint64_t tcps_sc_aborted;	/* syncache entry aborted */
	uint64_t tcps_sc_badack;	/* removed due to bad ACK */
	uint64_t tcps_sc_unreach;	/* ICMP unreachable received */
	uint64_t tcps_sc_zonefail;	/* zalloc() failed */
	uint64_t tcps_sc_sendcookie;	/* SYN cookie sent */
	uint64_t tcps_sc_recvcookie;	/* SYN cookie received */

	uint64_t tcps_hc_added;		/* entry added to hostcache */
	uint64_t tcps_hc_bucketoverflow;/* hostcache per bucket limit hit */

	uint64_t tcps_finwait2_drops;    /* Drop FIN_WAIT_2 connection after time limit */

	/* SACK related stats */
	uint64_t tcps_sack_recovery_episode; /* SACK recovery episodes */
	uint64_t tcps_sack_rexmits;	    /* SACK rexmit segments   */
	uint64_t tcps_sack_rexmit_bytes;    /* SACK rexmit bytes      */
	uint64_t tcps_sack_rcv_blocks;	    /* SACK blocks (options) received */
	uint64_t tcps_sack_send_blocks;	    /* SACK blocks (options) sent     */
	uint64_t tcps_sack_sboverflow;	    /* times scoreboard overflowed */
	
	/* ECN related stats */
	uint64_t tcps_ecn_ce;		/* ECN Congestion Experienced */
	uint64_t tcps_ecn_ect0;		/* ECN Capable Transport */
	uint64_t tcps_ecn_ect1;		/* ECN Capable Transport */
	uint64_t tcps_ecn_shs;		/* ECN successful handshakes */
	uint64_t tcps_ecn_rcwnd;	/* # times ECN reduced the cwnd */

	/* TCP_SIGNATURE related stats */
	uint64_t tcps_sig_rcvgoodsig;	/* Total matching signature received */
	uint64_t tcps_sig_rcvbadsig;	/* Total bad signature received */
	uint64_t tcps_sig_err_buildsig;	/* Mismatching signature received */
	uint64_t tcps_sig_err_sigopt;	/* No signature expected by socket */
	uint64_t tcps_sig_err_nosigopt;	/* No signature provided by segment */

	uint64_t _pad[12];		/* 6 UTO, 6 TBD */
};

#define	tcps_rcvmemdrop	tcps_rcvreassfull	/* compat */

#ifdef _KERNEL
#include <sys/counter.h>

VNET_PCPUSTAT_DECLARE(struct tcpstat, tcpstat);	/* tcp statistics */
/*
 * In-kernel consumers can use these accessor macros directly to update
 * stats.
 */
#define	TCPSTAT_ADD(name, val)	\
    VNET_PCPUSTAT_ADD(struct tcpstat, tcpstat, name, (val))
#define	TCPSTAT_INC(name)	TCPSTAT_ADD(name, 1)

/*
 * Kernel module consumers must use this accessor macro.
 */
void	kmod_tcpstat_inc(int statnum);
#define	KMOD_TCPSTAT_INC(name)						\
    kmod_tcpstat_inc(offsetof(struct tcpstat, name) / sizeof(uint64_t))

/*
 * TCP specific helper hook point identifiers.
 */
#define	HHOOK_TCP_EST_IN		0
#define	HHOOK_TCP_EST_OUT		1
#define	HHOOK_TCP_LAST			HHOOK_TCP_EST_OUT

struct tcp_hhook_data {
	struct tcpcb	*tp;
	struct tcphdr	*th;
	struct tcpopt	*to;
	long		len;
	int		tso;
	tcp_seq		curack;
};
#endif

/*
 * TCB structure exported to user-land via sysctl(3).
 * Evil hack: declare only if in_pcb.h and sys/socketvar.h have been
 * included.  Not all of our clients do.
 */
#if defined(_NETINET_IN_PCB_H_) && defined(_SYS_SOCKETVAR_H_)
struct xtcp_timer {
	int tt_rexmt;	/* retransmit timer */
	int tt_persist;	/* retransmit persistence */
	int tt_keep;	/* keepalive */
	int tt_2msl;	/* 2*msl TIME_WAIT timer */
	int tt_delack;	/* delayed ACK timer */
	int t_rcvtime;	/* Time since last packet received */
};
struct	xtcpcb {
	size_t	xt_len;
	struct	inpcb	xt_inp;
	struct	tcpcb	xt_tp;
	struct	xsocket	xt_socket;
	struct	xtcp_timer xt_timer;
	u_quad_t	xt_alignment_hack;
};
#endif

/*
 * Identifiers for TCP sysctl nodes
 */
#define	TCPCTL_DO_RFC1323	1	/* use RFC-1323 extensions */
#define	TCPCTL_MSSDFLT		3	/* MSS default */
#define TCPCTL_STATS		4	/* statistics (read-only) */
#define	TCPCTL_RTTDFLT		5	/* default RTT estimate */
#define	TCPCTL_KEEPIDLE		6	/* keepalive idle timer */
#define	TCPCTL_KEEPINTVL	7	/* interval to send keepalives */
#define	TCPCTL_SENDSPACE	8	/* send buffer space */
#define	TCPCTL_RECVSPACE	9	/* receive buffer space */
#define	TCPCTL_KEEPINIT		10	/* timeout for establishing syn */
#define	TCPCTL_PCBLIST		11	/* list of all outstanding PCBs */
#define	TCPCTL_DELACKTIME	12	/* time before sending delayed ACK */
#define	TCPCTL_V6MSSDFLT	13	/* MSS default for IPv6 */
#define	TCPCTL_SACK		14	/* Selective Acknowledgement,rfc 2018 */
#define	TCPCTL_DROP		15	/* drop tcp connection */

#ifdef _KERNEL
#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet_tcp);
SYSCTL_DECL(_net_inet_tcp_sack);
MALLOC_DECLARE(M_TCPLOG);
#endif

VNET_DECLARE(struct inpcbhead, tcb);		/* queue of active tcpcb's */
VNET_DECLARE(struct inpcbinfo, tcbinfo);
extern	int tcp_log_in_vain;
VNET_DECLARE(int, tcp_mssdflt);	/* XXX */
VNET_DECLARE(int, tcp_minmss);
VNET_DECLARE(int, tcp_delack_enabled);
VNET_DECLARE(int, tcp_do_rfc3390);
VNET_DECLARE(int, tcp_do_initcwnd10);
VNET_DECLARE(int, tcp_sendspace);
VNET_DECLARE(int, tcp_recvspace);
VNET_DECLARE(int, path_mtu_discovery);
VNET_DECLARE(int, tcp_do_rfc3465);
VNET_DECLARE(int, tcp_abc_l_var);
#define	V_tcb			VNET(tcb)
#define	V_tcbinfo		VNET(tcbinfo)
#define	V_tcp_mssdflt		VNET(tcp_mssdflt)
#define	V_tcp_minmss		VNET(tcp_minmss)
#define	V_tcp_delack_enabled	VNET(tcp_delack_enabled)
#define	V_tcp_do_rfc3390	VNET(tcp_do_rfc3390)
#define	V_tcp_do_initcwnd10	VNET(tcp_do_initcwnd10)
#define	V_tcp_sendspace		VNET(tcp_sendspace)
#define	V_tcp_recvspace		VNET(tcp_recvspace)
#define	V_path_mtu_discovery	VNET(path_mtu_discovery)
#define	V_tcp_do_rfc3465	VNET(tcp_do_rfc3465)
#define	V_tcp_abc_l_var		VNET(tcp_abc_l_var)

VNET_DECLARE(int, tcp_do_sack);			/* SACK enabled/disabled */
VNET_DECLARE(int, tcp_sc_rst_sock_fail);	/* RST on sock alloc failure */
#define	V_tcp_do_sack		VNET(tcp_do_sack)
#define	V_tcp_sc_rst_sock_fail	VNET(tcp_sc_rst_sock_fail)

VNET_DECLARE(int, tcp_do_ecn);			/* TCP ECN enabled/disabled */
VNET_DECLARE(int, tcp_ecn_maxretries);
#define	V_tcp_do_ecn		VNET(tcp_do_ecn)
#define	V_tcp_ecn_maxretries	VNET(tcp_ecn_maxretries)

VNET_DECLARE(struct hhook_head *, tcp_hhh[HHOOK_TCP_LAST + 1]);
#define	V_tcp_hhh		VNET(tcp_hhh)

int	 tcp_addoptions(struct tcpopt *, u_char *);
int	 tcp_ccalgounload(struct cc_algo *unload_algo);
struct tcpcb *
	 tcp_close(struct tcpcb *);
void	 tcp_discardcb(struct tcpcb *);
void	 tcp_twstart(struct tcpcb *);
void	 tcp_twclose(struct tcptw *, int);
void	 tcp_ctlinput(int, struct sockaddr *, void *);
int	 tcp_ctloutput(struct socket *, struct sockopt *);
struct tcpcb *
	 tcp_drop(struct tcpcb *, int);
void	 tcp_drain(void);
void	 tcp_init(void);
#ifdef VIMAGE
void	 tcp_destroy(void);
#endif
void	 tcp_fini(void *);
char	*tcp_log_addrs(struct in_conninfo *, struct tcphdr *, void *,
	    const void *);
char	*tcp_log_vain(struct in_conninfo *, struct tcphdr *, void *,
	    const void *);
int	 tcp_reass(struct tcpcb *, struct tcphdr *, int *, struct mbuf *);
void	 tcp_reass_global_init(void);
void	 tcp_reass_flush(struct tcpcb *);
int	 tcp_input(struct mbuf **, int *, int);
u_long	 tcp_maxmtu(struct in_conninfo *, struct tcp_ifcap *);
u_long	 tcp_maxmtu6(struct in_conninfo *, struct tcp_ifcap *);
void	 tcp_mss_update(struct tcpcb *, int, int, struct hc_metrics_lite *,
	    struct tcp_ifcap *);
void	 tcp_mss(struct tcpcb *, int);
int	 tcp_mssopt(struct in_conninfo *);
struct inpcb *
	 tcp_drop_syn_sent(struct inpcb *, int);
struct tcpcb *
	 tcp_newtcpcb(struct inpcb *);
int	 tcp_output(struct tcpcb *);
void	 tcp_state_change(struct tcpcb *, int);
void	 tcp_respond(struct tcpcb *, void *,
	    struct tcphdr *, struct mbuf *, tcp_seq, tcp_seq, int);
void	 tcp_tw_init(void);
#ifdef VIMAGE
void	 tcp_tw_destroy(void);
#endif
void	 tcp_tw_zone_change(void);
int	 tcp_twcheck(struct inpcb *, struct tcpopt *, struct tcphdr *,
	    struct mbuf *, int);
void	 tcp_setpersist(struct tcpcb *);
#ifdef TCP_SIGNATURE
struct secasvar;
struct secasvar *tcp_get_sav(struct mbuf *, u_int);
int	 tcp_signature_do_compute(struct mbuf *, int, int, u_char *,
	    struct secasvar *);
int	 tcp_signature_compute(struct mbuf *, int, int, int, u_char *, u_int);
int	 tcp_signature_verify(struct mbuf *, int, int, int, struct tcpopt *,
	    struct tcphdr *, u_int);
int	tcp_signature_check(struct mbuf *m, int off0, int tlen, int optlen,
	    struct tcpopt *to, struct tcphdr *th, u_int tcpbflag);
#endif
void	 tcp_slowtimo(void);
struct tcptemp *
	 tcpip_maketemplate(struct inpcb *);
void	 tcpip_fillheaders(struct inpcb *, void *, void *);
void	 tcp_timer_activate(struct tcpcb *, uint32_t, u_int);
int	 tcp_timer_active(struct tcpcb *, uint32_t);
void	 tcp_timer_stop(struct tcpcb *, uint32_t);
void	 tcp_trace(short, short, struct tcpcb *, void *, struct tcphdr *, int);
/*
 * All tcp_hc_* functions are IPv4 and IPv6 (via in_conninfo)
 */
void	 tcp_hc_init(void);
#ifdef VIMAGE
void	 tcp_hc_destroy(void);
#endif
void	 tcp_hc_get(struct in_conninfo *, struct hc_metrics_lite *);
u_long	 tcp_hc_getmtu(struct in_conninfo *);
void	 tcp_hc_updatemtu(struct in_conninfo *, u_long);
void	 tcp_hc_update(struct in_conninfo *, struct hc_metrics_lite *);

extern	struct pr_usrreqs tcp_usrreqs;
tcp_seq tcp_new_isn(struct tcpcb *);

void	 tcp_sack_doack(struct tcpcb *, struct tcpopt *, tcp_seq);
void	 tcp_update_sack_list(struct tcpcb *tp, tcp_seq rcv_laststart, tcp_seq rcv_lastend);
void	 tcp_clean_sackreport(struct tcpcb *tp);
void	 tcp_sack_adjust(struct tcpcb *tp);
struct sackhole *tcp_sack_output(struct tcpcb *tp, int *sack_bytes_rexmt);
void	 tcp_sack_partialack(struct tcpcb *, struct tcphdr *);
void	 tcp_free_sackholes(struct tcpcb *tp);
int	 tcp_newreno(struct tcpcb *, struct tcphdr *);
u_long	 tcp_seq_subtract(u_long, u_long );

void	cc_cong_signal(struct tcpcb *tp, struct tcphdr *th, uint32_t type);

static inline void
tcp_fields_to_host(struct tcphdr *th)
{

	th->th_seq = ntohl(th->th_seq);
	th->th_ack = ntohl(th->th_ack);
	th->th_win = ntohs(th->th_win);
	th->th_urp = ntohs(th->th_urp);
}

#ifdef TCP_SIGNATURE
static inline void
tcp_fields_to_net(struct tcphdr *th)
{

	th->th_seq = htonl(th->th_seq);
	th->th_ack = htonl(th->th_ack);
	th->th_win = htons(th->th_win);
	th->th_urp = htons(th->th_urp);
}
#endif
#endif /* _KERNEL */

#endif /* _NETINET_TCP_VAR_H_ */
