/*
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/netinet/in_hostcache.h,v 1.3 1999/12/29 04:40:59 peter Exp $
 */

#ifndef _NETINET_IN_HOSTCACHE_H
#define	_NETINET_IN_HOSTCACHE_H	1

/*
 * This file defines the particular structures contained in the host cache
 * for the use of IP.
 */

/*
 * An IP host cache entry.  Note that we include the srtt/var here,
 * with the expectation that it might be used to keep a persistent,
 * cross-connection view of this statistic.
 */
struct in_hcentry {
	struct	hcentry inhc_hc;
	u_long	inhc_pmtu;
	u_long	inhc_recvpipe;
	u_long	inhc_sendpipe;
	u_long	inhc_pksent;
	u_long	inhc_flags;
	u_long	inhc_ssthresh;
	int	inhc_srtt;	/* VJ RTT estimator */
	int	inhc_srttvar;	/* VJ */
	u_int	inhc_rttmin;	/* VJ */
	int	inhc_rxt;	/* TCP retransmit timeout */
	u_long	inhc_cc;	/* deliberate type pun with tcp_cc */
	u_long	inhc_ccsent;	/* as above */
	u_short	inhc_mssopt;
};

#define	inhc_addr(inhc)	((struct sockaddr_in *)(inhc)->inhc_hc.hc_host)

/* Flags for inhc_flags... */
#define	INHC_LOCAL	0x0001	/* this address is local */
#define	INHC_BROADCAST	0x0002	/* this address is broadcast */
#define	INHC_MULTICAST	0x0004	/* this address is multicast */
#define	INHC_REDUCEDMTU	0x0008	/* we reduced the mtu via PMTU discovery */

#ifdef _KERNEL
/*
 * inhc_alloc can block while adding a new entry to the cache;
 * inhc_lookup will does not add new entries and so can be called
 * in non-process context.
 */
struct	in_hcentry *inhc_alloc(struct sockaddr_in *sin);
int	inhc_init(void);
struct	in_hcentry *inhc_lookup(struct sockaddr_in *sin);
#define	inhc_ref(inhc)	(hc_ref(&(inhc)->inhc_hc))
#define	inhc_rele(inhc)	(hc_rele(&(inhc)->inhc_hc))
#endif

#endif /* _NETINET_IN_HOSTCACHE_H */
