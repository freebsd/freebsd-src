/*-
 * Copyright (c) 2001-2006, Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_var.h,v 1.24 2005/03/06 16:04:19 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef _NETINET_SCTP_VAR_H_
#define _NETINET_SCTP_VAR_H_

#include <sys/socketvar.h>
#include <netinet/sctp_uio.h>

/* SCTP Kernel structures */

/*
 * Names for SCTP sysctl objects
 */
#define	SCTPCTL_MAXDGRAM	    1	/* max datagram size */
#define	SCTPCTL_RECVSPACE	    2	/* default receive buffer space */
#define SCTPCTL_AUTOASCONF          3	/* auto asconf enable/disable flag */
#define SCTPCTL_ECN_ENABLE          4	/* Is ecn allowed */
#define SCTPCTL_ECN_NONCE           5	/* Is ecn nonce allowed */
#define SCTPCTL_STRICT_SACK         6	/* strictly require sack'd TSN's to be
					 * smaller than sndnxt. */
#define SCTPCTL_NOCSUM_LO           7	/* Require that the Loopback NOT have
					 * the crc32 checksum on packets
					 * routed over it. */
#define SCTPCTL_STRICT_INIT         8
#define SCTPCTL_PEER_CHK_OH         9
#define SCTPCTL_MAXBURST            10
#define SCTPCTL_MAXCHUNKONQ         11
#define SCTPCTL_DELAYED_SACK        12
#define SCTPCTL_HB_INTERVAL         13
#define SCTPCTL_PMTU_RAISE          14
#define SCTPCTL_SHUTDOWN_GUARD      15
#define SCTPCTL_SECRET_LIFETIME     16
#define SCTPCTL_RTO_MAX             17
#define SCTPCTL_RTO_MIN             18
#define SCTPCTL_RTO_INITIAL         19
#define SCTPCTL_INIT_RTO_MAX        20
#define SCTPCTL_COOKIE_LIFE         21
#define SCTPCTL_INIT_RTX_MAX        22
#define SCTPCTL_ASSOC_RTX_MAX       23
#define SCTPCTL_PATH_RTX_MAX        24
#define SCTPCTL_NR_OUTGOING_STREAMS 25
#define SCTPCTL_CMT_ON_OFF          26
#define SCTPCTL_CWND_MAXBURST       27
#define SCTPCTL_EARLY_FR            28
#define SCTPCTL_RTTVAR_CC           29
#define SCTPCTL_DEADLOCK_DET        30
#define SCTPCTL_EARLY_FR_MSEC       31
#define SCTPCTL_ASCONF_AUTH_NOCHK   32
#define SCTPCTL_AUTH_DISABLE        33
#define SCTPCTL_AUTH_RANDOM_LEN     34
#define SCTPCTL_AUTH_HMAC_ID        35
#define SCTPCTL_ABC_L_VAR           36
#define SCTPCTL_MAX_MBUF_CHAIN      37
#define SCTPCTL_CMT_USE_DAC         38
#define SCTPCTL_DO_DRAIN            39
#define SCTPCTL_WARM_CRC32          40
#define SCTPCTL_QLIMIT_ABORT        41
#define SCTPCTL_STRICT_ORDER        42
#define SCTPCTL_TCBHASHSIZE         43
#define SCTPCTL_PCBHASHSIZE         44
#define SCTPCTL_CHUNKSCALE          45
#define SCTPCTL_MINSPLIT            46
#define SCTPCTL_ADD_MORE            47
#define SCTPCTL_SYS_RESC            48
#define SCTPCTL_ASOC_RESC           49
#define SCTPCTL_NAT_FRIENDLY	    50
#ifdef SCTP_DEBUG
#define SCTPCTL_DEBUG               51
#define SCTPCTL_MAXID		    51
#else
#define SCTPCTL_MAXID		    50
#endif

#ifdef SCTP_DEBUG
#define SCTPCTL_NAMES { \
	{ 0, 0 }, \
	{ "sendspace", CTLTYPE_INT }, \
	{ "recvspace", CTLTYPE_INT }, \
	{ "autoasconf", CTLTYPE_INT }, \
	{ "ecn_enable", CTLTYPE_INT }, \
	{ "ecn_nonce", CTLTYPE_INT }, \
	{ "strict_sack", CTLTYPE_INT }, \
	{ "looback_nocsum", CTLTYPE_INT }, \
	{ "strict_init", CTLTYPE_INT }, \
	{ "peer_chkoh", CTLTYPE_INT }, \
	{ "maxburst", CTLTYPE_INT }, \
	{ "maxchunks", CTLTYPE_INT }, \
	{ "delayed_sack_time", CTLTYPE_INT }, \
	{ "heartbeat_interval", CTLTYPE_INT }, \
	{ "pmtu_raise_time", CTLTYPE_INT }, \
	{ "shutdown_guard_time", CTLTYPE_INT }, \
	{ "secret_lifetime", CTLTYPE_INT }, \
	{ "rto_max", CTLTYPE_INT }, \
	{ "rto_min", CTLTYPE_INT }, \
	{ "rto_initial", CTLTYPE_INT }, \
	{ "init_rto_max", CTLTYPE_INT }, \
	{ "valid_cookie_life", CTLTYPE_INT }, \
	{ "init_rtx_max", CTLTYPE_INT }, \
	{ "assoc_rtx_max", CTLTYPE_INT }, \
	{ "path_rtx_max", CTLTYPE_INT }, \
	{ "nr_outgoing_streams", CTLTYPE_INT }, \
	{ "cmt_on_off", CTLTYPE_INT }, \
	{ "cwnd_maxburst", CTLTYPE_INT }, \
	{ "early_fast_retran", CTLTYPE_INT }, \
	{ "use_rttvar_congctrl", CTLTYPE_INT }, \
	{ "deadlock_detect", CTLTYPE_INT }, \
	{ "early_fast_retran_msec", CTLTYPE_INT }, \
	{ "asconf_auth_nochk", CTLTYPE_INT }, \
	{ "auth_disable", CTLTYPE_INT }, \
	{ "auth_random_len", CTLTYPE_INT }, \
	{ "auth_hmac_id", CTLTYPE_INT }, \
	{ "abc_l_var", CTLTYPE_INT }, \
	{ "max_mbuf_chain", CTLTYPE_INT }, \
	{ "cmt_use_dac", CTLTYPE_INT }, \
	{ "do_sctp_drain", CTLTYPE_INT }, \
	{ "warm_crc_table", CTLTYPE_INT }, \
	{ "abort_at_limit", CTLTYPE_INT }, \
	{ "strict_data_order", CTLTYPE_INT }, \
	{ "tcbhashsize", CTLTYPE_INT }, \
	{ "pcbhashsize", CTLTYPE_INT }, \
	{ "chunkscale", CTLTYPE_INT }, \
	{ "min_split_point", CTLTYPE_INT }, \
	{ "add_more_on_output", CTLTYPE_INT }, \
	{ "sys_resource", CTLTYPE_INT }, \
	{ "asoc_resource", CTLTYPE_INT }, \
	{ "nat_friendly", CTLTYPE_INT }, \
	{ "debug", CTLTYPE_INT }, \
}
#else
#define SCTPCTL_NAMES { \
	{ 0, 0 }, \
	{ "sendspace", CTLTYPE_INT }, \
	{ "recvspace", CTLTYPE_INT }, \
	{ "autoasconf", CTLTYPE_INT }, \
	{ "ecn_enable", CTLTYPE_INT }, \
	{ "ecn_nonce", CTLTYPE_INT }, \
	{ "strict_sack", CTLTYPE_INT }, \
	{ "looback_nocsum", CTLTYPE_INT }, \
	{ "strict_init", CTLTYPE_INT }, \
	{ "peer_chkoh", CTLTYPE_INT }, \
	{ "maxburst", CTLTYPE_INT }, \
	{ "maxchunks", CTLTYPE_INT }, \
	{ "delayed_sack_time", CTLTYPE_INT }, \
	{ "heartbeat_interval", CTLTYPE_INT }, \
	{ "pmtu_raise_time", CTLTYPE_INT }, \
	{ "shutdown_guard_time", CTLTYPE_INT }, \
	{ "secret_lifetime", CTLTYPE_INT }, \
	{ "rto_max", CTLTYPE_INT }, \
	{ "rto_min", CTLTYPE_INT }, \
	{ "rto_initial", CTLTYPE_INT }, \
	{ "init_rto_max", CTLTYPE_INT }, \
	{ "valid_cookie_life", CTLTYPE_INT }, \
	{ "init_rtx_max", CTLTYPE_INT }, \
	{ "assoc_rtx_max", CTLTYPE_INT }, \
	{ "path_rtx_max", CTLTYPE_INT }, \
	{ "nr_outgoing_streams", CTLTYPE_INT }, \
	{ "cmt_on_off", CTLTYPE_INT }, \
	{ "cwnd_maxburst", CTLTYPE_INT }, \
	{ "early_fast_retran", CTLTYPE_INT }, \
	{ "use_rttvar_congctrl", CTLTYPE_INT }, \
	{ "deadlock_detect", CTLTYPE_INT }, \
	{ "early_fast_retran_msec", CTLTYPE_INT }, \
	{ "asconf_auth_nochk", CTLTYPE_INT }, \
	{ "auth_disable", CTLTYPE_INT }, \
	{ "auth_random_len", CTLTYPE_INT }, \
	{ "auth_hmac_id", CTLTYPE_INT }, \
	{ "abc_l_var", CTLTYPE_INT }, \
	{ "max_mbuf_chain", CTLTYPE_INT }, \
	{ "cmt_use_dac", CTLTYPE_INT }, \
	{ "do_sctp_drain", CTLTYPE_INT }, \
	{ "warm_crc_table", CTLTYPE_INT }, \
	{ "abort_at_limit", CTLTYPE_INT }, \
	{ "strict_data_order", CTLTYPE_INT }, \
	{ "tcbhashsize", CTLTYPE_INT }, \
	{ "pcbhashsize", CTLTYPE_INT }, \
	{ "chunkscale", CTLTYPE_INT }, \
	{ "min_split_point", CTLTYPE_INT }, \
	{ "add_more_on_output", CTLTYPE_INT }, \
	{ "sys_resource", CTLTYPE_INT }, \
	{ "asoc_resource", CTLTYPE_INT }, \
	{ "nat_friendly", CTLTYPE_INT }, \
}
#endif



#if defined(_KERNEL)

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet_sctp);
#endif
extern struct pr_usrreqs sctp_usrreqs;


#define sctp_feature_on(inp, feature)  (inp->sctp_features |= feature)
#define sctp_feature_off(inp, feature) (inp->sctp_features &= ~feature)
#define sctp_is_feature_on(inp, feature) (inp->sctp_features & feature)
#define sctp_is_feature_off(inp, feature) ((inp->sctp_features & feature) == 0)

#define	sctp_sbspace(asoc, sb) ((long) (((sb)->sb_hiwat > (asoc)->sb_cc) ? ((sb)->sb_hiwat - (asoc)->sb_cc) : 0))

#define	sctp_sbspace_failedmsgs(sb) ((long) (((sb)->sb_hiwat > (sb)->sb_cc) ? ((sb)->sb_hiwat - (sb)->sb_cc) : 0))

#define sctp_sbspace_sub(a,b) ((a > b) ? (a - b) : 0)

extern uint32_t sctp_asoc_free_resc_limit;
extern uint32_t sctp_system_free_resc_limit;

/* I tried to cache the readq entries at one
 * point. But the reality is that it did not
 * add any performance since this meant
 * we had to lock the STCB on read. And at that point
 * once you have to do an extra lock, it really does
 * not matter if the lock is in the ZONE stuff or
 * in our code. Note that this same problem would
 * occur with an mbuf cache as well so it is
 * not really worth doing, at least right now :-D
 */

#define sctp_free_a_readq(_stcb, _readq) { \
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_readq, (_readq)); \
		SCTP_DECR_READQ_COUNT(); \
}

#define sctp_alloc_a_readq(_stcb, _readq) { \
	(_readq) = (struct sctp_queued_to_read  *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_readq); \
	if ((_readq)) { \
 	     SCTP_INCR_READQ_COUNT(); \
	} \
}



#define sctp_free_a_strmoq(_stcb, _strmoq) { \
       if (((_stcb)->asoc.free_strmoq_cnt > sctp_asoc_free_resc_limit) || \
	   (sctppcbinfo.ipi_free_strmoq > sctp_system_free_resc_limit)) { \
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_strmoq, (_strmoq)); \
		SCTP_DECR_STRMOQ_COUNT(); \
	   } else { \
		TAILQ_INSERT_TAIL(&(_stcb)->asoc.free_strmoq, (_strmoq), next); \
                (_stcb)->asoc.free_strmoq_cnt++; \
                atomic_add_int(&sctppcbinfo.ipi_free_strmoq, 1); \
	   } \
}

#define sctp_alloc_a_strmoq(_stcb, _strmoq) { \
      if(TAILQ_EMPTY(&(_stcb)->asoc.free_strmoq))  { \
	(_strmoq) = (struct sctp_stream_queue_pending  *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_strmoq); \
	if ((_strmoq)) { \
 	     SCTP_INCR_STRMOQ_COUNT(); \
	} \
      } else { \
        (_strmoq) = TAILQ_FIRST(&(_stcb)->asoc.free_strmoq); \
         TAILQ_REMOVE(&(_stcb)->asoc.free_strmoq, (_strmoq), next); \
         atomic_subtract_int(&sctppcbinfo.ipi_free_strmoq, 1); \
         (_stcb)->asoc.free_strmoq_cnt--; \
      } \
}


#define sctp_free_a_chunk(_stcb, _chk) { \
       if (((_stcb)->asoc.free_chunk_cnt > sctp_asoc_free_resc_limit) || \
	   (sctppcbinfo.ipi_free_chunks > sctp_system_free_resc_limit)) { \
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, (_chk)); \
		SCTP_DECR_CHK_COUNT(); \
	   } else { \
		TAILQ_INSERT_TAIL(&(_stcb)->asoc.free_chunks, (_chk), sctp_next); \
                (_stcb)->asoc.free_chunk_cnt++; \
                atomic_add_int(&sctppcbinfo.ipi_free_chunks, 1); \
	   } \
}

#define sctp_alloc_a_chunk(_stcb, _chk) { \
      if(TAILQ_EMPTY(&(_stcb)->asoc.free_chunks))  { \
	(_chk) = (struct sctp_tmit_chunk *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_chunk); \
	if ((_chk)) { \
 	     SCTP_INCR_CHK_COUNT(); \
	} \
      } else { \
        (_chk) = TAILQ_FIRST(&(_stcb)->asoc.free_chunks); \
         TAILQ_REMOVE(&(_stcb)->asoc.free_chunks, (_chk), sctp_next); \
         atomic_subtract_int(&sctppcbinfo.ipi_free_chunks, 1); \
         (_stcb)->asoc.free_chunk_cnt--; \
      } \
}



#define sctp_free_remote_addr(__net) { \
	if ((__net)) { \
                if (atomic_fetchadd_int(&(__net)->ref_count, -1) == 1) { \
			SCTP_OS_TIMER_STOP(&(__net)->rxt_timer.timer); \
			SCTP_OS_TIMER_STOP(&(__net)->pmtu_timer.timer); \
			SCTP_OS_TIMER_STOP(&(__net)->fr_timer.timer); \
			(__net)->dest_state = SCTP_ADDR_NOT_REACHABLE; \
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_net, (__net)); \
			SCTP_DECR_RADDR_COUNT(); \
		} \
	} \
}

#define sctp_sbfree(ctl, stcb, sb, m) { \
        uint32_t val; \
        val = atomic_fetchadd_int(&(sb)->sb_cc,-(SCTP_BUF_LEN((m)))); \
        if(val < SCTP_BUF_LEN((m))) { \
           panic("sb_cc goes negative"); \
        } \
        val = atomic_fetchadd_int(&(sb)->sb_mbcnt,-(MSIZE)); \
        if(val < MSIZE) { \
            panic("sb_mbcnt goes negative"); \
        } \
        if (SCTP_BUF_IS_EXTENDED(m)) { \
                val = atomic_fetchadd_int(&(sb)->sb_mbcnt,-(SCTP_BUF_EXTEND_SIZE(m))); \
		if(val < SCTP_BUF_EXTEND_SIZE(m)) { \
                    panic("sb_mbcnt goes negative2"); \
                } \
        } \
        if (((ctl)->do_not_ref_stcb == 0) && stcb) {\
          val = atomic_fetchadd_int(&(stcb)->asoc.sb_cc,-(SCTP_BUF_LEN((m)))); \
          if(val < SCTP_BUF_LEN((m))) {\
             panic("stcb->sb_cc goes negative"); \
          } \
          val = atomic_fetchadd_int(&(stcb)->asoc.sb_mbcnt,-(MSIZE)); \
          if(val < MSIZE) { \
             panic("asoc->mbcnt goes negative"); \
          } \
	  if (SCTP_BUF_IS_EXTENDED(m)) { \
                val = atomic_fetchadd_int(&(stcb)->asoc.sb_mbcnt,-(SCTP_BUF_EXTEND_SIZE(m))); \
		if(val < SCTP_BUF_EXTEND_SIZE(m)) { \
		   panic("assoc stcb->mbcnt would go negative"); \
                } \
          } \
        } \
	if (SCTP_BUF_TYPE(m) != MT_DATA && SCTP_BUF_TYPE(m) != MT_HEADER && \
	    SCTP_BUF_TYPE(m) != MT_OOBDATA) \
		atomic_subtract_int(&(sb)->sb_ctl,SCTP_BUF_LEN((m))); \
}


#define sctp_sballoc(stcb, sb, m) { \
	atomic_add_int(&(sb)->sb_cc,SCTP_BUF_LEN((m))); \
	atomic_add_int(&(sb)->sb_mbcnt, MSIZE); \
	if (SCTP_BUF_IS_EXTENDED(m)) \
		atomic_add_int(&(sb)->sb_mbcnt,SCTP_BUF_EXTEND_SIZE(m)); \
        if(stcb) { \
  	  atomic_add_int(&(stcb)->asoc.sb_cc,SCTP_BUF_LEN((m))); \
          atomic_add_int(&(stcb)->asoc.sb_mbcnt, MSIZE); \
	  if (SCTP_BUF_IS_EXTENDED(m)) \
		atomic_add_int(&(stcb)->asoc.sb_mbcnt,SCTP_BUF_EXTEND_SIZE(m)); \
        } \
	if (SCTP_BUF_TYPE(m) != MT_DATA && SCTP_BUF_TYPE(m) != MT_HEADER && \
	    SCTP_BUF_TYPE(m) != MT_OOBDATA) \
		atomic_add_int(&(sb)->sb_ctl,SCTP_BUF_LEN((m))); \
}


#define sctp_ucount_incr(val) { \
	val++; \
}

#define sctp_ucount_decr(val) { \
	if (val > 0) { \
		val--; \
	} else { \
		val = 0; \
	} \
}

#define sctp_mbuf_crush(data) do { \
                struct mbuf *_m; \
		_m = (data); \
		while(_m && (SCTP_BUF_LEN(_m) == 0)) { \
			(data)  = SCTP_BUF_NEXT(_m); \
			SCTP_BUF_NEXT(_m) = NULL; \
			sctp_m_free(_m); \
			_m = (data); \
		} \
} while (0)


/*
 * some sysctls
 */
extern int sctp_sendspace;
extern int sctp_recvspace;
extern int sctp_ecn_enable;
extern int sctp_ecn_nonce;
extern int sctp_use_cwnd_based_maxburst;
extern unsigned int sctp_cmt_on_off;
extern unsigned int sctp_cmt_use_dac;
extern unsigned int sctp_cmt_sockopt_on_off;
extern uint32_t sctp_nat_friendly;

struct sctp_nets;
struct sctp_inpcb;
struct sctp_tcb;
struct sctphdr;

void sctp_ctlinput __P((int, struct sockaddr *, void *));
int sctp_ctloutput __P((struct socket *, struct sockopt *));
void sctp_input __P((struct mbuf *, int));

void sctp_drain __P((void));
void sctp_init __P((void));

int sctp_shutdown __P((struct socket *));
void sctp_notify 
__P((struct sctp_inpcb *, int, struct sctphdr *,
    struct sockaddr *, struct sctp_tcb *,
    struct sctp_nets *));

#if defined(INET6)
	void ip_2_ip6_hdr __P((struct ip6_hdr *, struct ip *));

#endif

	int sctp_bindx(struct socket *, int, struct sockaddr_storage *,
        int, int, struct proc *);

/* can't use sctp_assoc_t here */
	int sctp_peeloff(struct socket *, struct socket *, int, caddr_t, int *);

	sctp_assoc_t sctp_getassocid(struct sockaddr *);


	int sctp_ingetaddr(struct socket *,
        struct sockaddr **
);

	int sctp_peeraddr(struct socket *,
        struct sockaddr **
);

	int sctp_listen(struct socket *, int, struct thread *);

	int sctp_accept(struct socket *, struct sockaddr **);


#endif				/* _KERNEL */

#endif				/* !_NETINET_SCTP_VAR_H_ */
