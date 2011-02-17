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

#ifndef _NETINET_TCP_SYNCACHE_H_
#define _NETINET_TCP_SYNCACHE_H_
#ifdef _KERNEL

struct toeopt;

void	 syncache_init(void);
#ifdef VIMAGE
void	syncache_destroy(void);
#endif
void	 syncache_unreach(struct in_conninfo *, struct tcphdr *);
int	 syncache_expand(struct in_conninfo *, struct tcpopt *,
	     struct tcphdr *, struct socket **, struct mbuf *);
int	 tcp_offload_syncache_expand(struct in_conninfo *inc, struct toeopt *toeo,
             struct tcphdr *th, struct socket **lsop, struct mbuf *m);
void	 syncache_add(struct in_conninfo *, struct tcpopt *,
	     struct tcphdr *, struct inpcb *, struct socket **, struct mbuf *);
void	 tcp_offload_syncache_add(struct in_conninfo *, struct toeopt *,
             struct tcphdr *, struct inpcb *, struct socket **,
             struct toe_usrreqs *tu, void *toepcb);

void	 syncache_chkrst(struct in_conninfo *, struct tcphdr *);
void	 syncache_badack(struct in_conninfo *);
int	 syncache_pcbcount(void);
int	 syncache_pcblist(struct sysctl_req *req, int max_pcbs, int *pcbs_exported);

struct syncache {
	TAILQ_ENTRY(syncache)	sc_hash;
	struct		in_conninfo sc_inc;	/* addresses */
	int		sc_rxttime;		/* retransmit time */
	u_int16_t	sc_rxmits;		/* retransmit counter */
	u_int32_t	sc_tsreflect;		/* timestamp to reflect */
	u_int32_t	sc_ts;			/* our timestamp to send */
	u_int32_t	sc_tsoff;		/* ts offset w/ syncookies */
	u_int32_t	sc_flowlabel;		/* IPv6 flowlabel */
	tcp_seq		sc_irs;			/* seq from peer */
	tcp_seq		sc_iss;			/* our ISS */
	struct		mbuf *sc_ipopts;	/* source route */
	u_int16_t	sc_peer_mss;		/* peer's MSS */
	u_int16_t	sc_wnd;			/* advertised window */
	u_int8_t	sc_ip_ttl;		/* IPv4 TTL */
	u_int8_t	sc_ip_tos;		/* IPv4 TOS */
	u_int8_t	sc_requested_s_scale:4,
			sc_requested_r_scale:4;
	u_int16_t	sc_flags;
#ifndef TCP_OFFLOAD_DISABLE
	struct toe_usrreqs *sc_tu;		/* TOE operations */
	void		*sc_toepcb;		/* TOE protocol block */
#endif			
	struct label	*sc_label;		/* MAC label reference */
	struct ucred	*sc_cred;		/* cred cache for jail checks */
};

/*
 * Flags for the sc_flags field.
 */
#define SCF_NOOPT	0x01			/* no TCP options */
#define SCF_WINSCALE	0x02			/* negotiated window scaling */
#define SCF_TIMESTAMP	0x04			/* negotiated timestamps */
						/* MSS is implicit */
#define SCF_UNREACH	0x10			/* icmp unreachable received */
#define SCF_SIGNATURE	0x20			/* send MD5 digests */
#define SCF_SACK	0x80			/* send SACK option */
#define SCF_ECN		0x100			/* send ECN setup packet */

#define	SYNCOOKIE_SECRET_SIZE	8	/* dwords */
#define	SYNCOOKIE_LIFETIME	16	/* seconds */

struct syncache_head {
	struct vnet	*sch_vnet;
	struct mtx	sch_mtx;
	TAILQ_HEAD(sch_head, syncache)	sch_bucket;
	struct callout	sch_timer;
	int		sch_nextc;
	u_int		sch_length;
	u_int		sch_oddeven;
	u_int32_t	sch_secbits_odd[SYNCOOKIE_SECRET_SIZE];
	u_int32_t	sch_secbits_even[SYNCOOKIE_SECRET_SIZE];
	u_int		sch_reseed;		/* time_uptime, seconds */
};

struct tcp_syncache {
	struct	syncache_head *hashbase;
	uma_zone_t zone;
	u_int	hashsize;
	u_int	hashmask;
	u_int	bucket_limit;
	u_int	cache_count;		/* XXX: unprotected */
	u_int	cache_limit;
	u_int	rexmt_limit;
	u_int	hash_secret;
};

#endif /* _KERNEL */
#endif /* !_NETINET_TCP_SYNCACHE_H_ */
