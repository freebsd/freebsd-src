/*-
 * Copyright (c) 2001-2007, by Cisco Systems, Inc. All rights reserved.
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


/* $KAME: sctputil.h,v 1.15 2005/03/06 16:04:19 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#ifndef __sctputil_h__
#define __sctputil_h__


#if defined(_KERNEL)


#ifdef SCTP_ASOCLOG_OF_TSNS
void sctp_print_out_track_log(struct sctp_tcb *stcb);

#endif

#ifdef SCTP_MBUF_LOGGING
struct mbuf *sctp_m_free(struct mbuf *m);
void sctp_m_freem(struct mbuf *m);

#else
#define sctp_m_free m_free
#define sctp_m_freem m_freem
#endif

#if defined(SCTP_LOCAL_TRACE_BUF) || defined(__APPLE__)
void
     sctp_log_trace(uint32_t fr, const char *str, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f);

#endif

#define sctp_get_associd(stcb) ((sctp_assoc_t)stcb->asoc.assoc_id)


/*
 * Function prototypes
 */
uint32_t
sctp_get_ifa_hash_val(struct sockaddr *addr);

struct sctp_ifa *
         sctp_find_ifa_in_ep(struct sctp_inpcb *inp, struct sockaddr *addr, int hold_lock);

struct sctp_ifa *
         sctp_find_ifa_by_addr(struct sockaddr *addr, uint32_t vrf_id, int holds_lock);

uint32_t sctp_select_initial_TSN(struct sctp_pcb *);

uint32_t sctp_select_a_tag(struct sctp_inpcb *);

int sctp_init_asoc(struct sctp_inpcb *, struct sctp_tcb *, int, uint32_t, uint32_t);

void sctp_fill_random_store(struct sctp_pcb *);

void
sctp_timer_start(int, struct sctp_inpcb *, struct sctp_tcb *,
    struct sctp_nets *);

void
sctp_timer_stop(int, struct sctp_inpcb *, struct sctp_tcb *,
    struct sctp_nets *, uint32_t);

int
    sctp_dynamic_set_primary(struct sockaddr *sa, uint32_t vrf_id);

uint32_t sctp_calculate_sum(struct mbuf *, int32_t *, uint32_t);

void
     sctp_mtu_size_reset(struct sctp_inpcb *, struct sctp_association *, uint32_t);

void
sctp_add_to_readq(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_queued_to_read *control,
    struct sockbuf *sb,
    int end,
    int so_locked
#if !defined(__APPLE__) && !defined(SCTP_SO_LOCK_TESTING)
    SCTP_UNUSED
#endif
);

int
sctp_append_to_readq(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_queued_to_read *control,
    struct mbuf *m,
    int end,
    int new_cumack,
    struct sockbuf *sb);


void sctp_iterator_worker(void);

int find_next_best_mtu(int);

void
     sctp_timeout_handler(void *);

uint32_t
sctp_calculate_rto(struct sctp_tcb *, struct sctp_association *,
    struct sctp_nets *, struct timeval *, int);

uint32_t sctp_calculate_len(struct mbuf *);

caddr_t sctp_m_getptr(struct mbuf *, int, int, uint8_t *);

struct sctp_paramhdr *
sctp_get_next_param(struct mbuf *, int,
    struct sctp_paramhdr *, int);

int sctp_add_pad_tombuf(struct mbuf *, int);

int sctp_pad_lastmbuf(struct mbuf *, int, struct mbuf *);

void 
sctp_ulp_notify(uint32_t, struct sctp_tcb *, uint32_t, void *, int
#if !defined(__APPLE__) && !defined(SCTP_SO_LOCK_TESTING)
    SCTP_UNUSED
#endif
);

void
sctp_pull_off_control_to_new_inp(struct sctp_inpcb *old_inp,
    struct sctp_inpcb *new_inp,
    struct sctp_tcb *stcb, int waitflags);


void sctp_stop_timers_for_shutdown(struct sctp_tcb *);

void 
sctp_report_all_outbound(struct sctp_tcb *, int, int
#if !defined(__APPLE__) && !defined(SCTP_SO_LOCK_TESTING)
    SCTP_UNUSED
#endif
);

int sctp_expand_mapping_array(struct sctp_association *, uint32_t);

void 
sctp_abort_notification(struct sctp_tcb *, int, int
#if !defined(__APPLE__) && !defined(SCTP_SO_LOCK_TESTING)
    SCTP_UNUSED
#endif
);

/* We abort responding to an IP packet for some reason */
void
sctp_abort_association(struct sctp_inpcb *, struct sctp_tcb *,
    struct mbuf *, int, struct sctphdr *, struct mbuf *, uint32_t);


/* We choose to abort via user input */
void
sctp_abort_an_association(struct sctp_inpcb *, struct sctp_tcb *, int,
    struct mbuf *, int
#if !defined(__APPLE__) && !defined(SCTP_SO_LOCK_TESTING)
    SCTP_UNUSED
#endif
);

void 
sctp_handle_ootb(struct mbuf *, int, int, struct sctphdr *,
    struct sctp_inpcb *, struct mbuf *, uint32_t);

int 
sctp_connectx_helper_add(struct sctp_tcb *stcb, struct sockaddr *addr,
    int totaddr, int *error);

struct sctp_tcb *
sctp_connectx_helper_find(struct sctp_inpcb *inp, struct sockaddr *addr,
    int *totaddr, int *num_v4, int *num_v6, int *error, int limit, int *bad_addr);

int sctp_is_there_an_abort_here(struct mbuf *, int, uint32_t *);
uint32_t sctp_is_same_scope(struct sockaddr_in6 *, struct sockaddr_in6 *);

struct sockaddr_in6 *
             sctp_recover_scope(struct sockaddr_in6 *, struct sockaddr_in6 *);

#define sctp_recover_scope_mac(addr, store) do { \
	 if ((addr->sin6_family == AF_INET6) && \
	     (IN6_IS_SCOPE_LINKLOCAL(&addr->sin6_addr))) { \
		*store = *addr; \
		if (addr->sin6_scope_id == 0) { \
			if (!sa6_recoverscope(store)) { \
				addr = store; \
			} \
		} else { \
			in6_clearscope(&addr->sin6_addr); \
			addr = store; \
		} \
	 } \
} while (0)


int sctp_cmpaddr(struct sockaddr *, struct sockaddr *);

void sctp_print_address(struct sockaddr *);
void sctp_print_address_pkt(struct ip *, struct sctphdr *);

void
sctp_notify_partial_delivery_indication(struct sctp_tcb *stcb,
    uint32_t error, int no_lock, uint32_t strseq);

int
sctp_release_pr_sctp_chunk(struct sctp_tcb *, struct sctp_tmit_chunk *,
    int, struct sctpchunk_listhead *, int
#if !defined(__APPLE__) && !defined(SCTP_SO_LOCK_TESTING)
    SCTP_UNUSED
#endif
);

struct mbuf *sctp_generate_invmanparam(int);

void 
sctp_bindx_add_address(struct socket *so, struct sctp_inpcb *inp,
    struct sockaddr *sa, sctp_assoc_t assoc_id,
    uint32_t vrf_id, int *error, void *p);
void 
sctp_bindx_delete_address(struct socket *so, struct sctp_inpcb *inp,
    struct sockaddr *sa, sctp_assoc_t assoc_id,
    uint32_t vrf_id, int *error);

int sctp_local_addr_count(struct sctp_tcb *stcb);

#ifdef SCTP_MBCNT_LOGGING
void
sctp_free_bufspace(struct sctp_tcb *, struct sctp_association *,
    struct sctp_tmit_chunk *, int);

#else
#define sctp_free_bufspace(stcb, asoc, tp1, chk_cnt)  \
do { \
	if (tp1->data != NULL) { \
                atomic_subtract_int(&((asoc)->chunks_on_out_queue), chk_cnt); \
		if ((asoc)->total_output_queue_size >= tp1->book_size) { \
			atomic_subtract_int(&((asoc)->total_output_queue_size), tp1->book_size); \
		} else { \
			(asoc)->total_output_queue_size = 0; \
		} \
   	        if (stcb->sctp_socket && ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) || \
	            (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL))) { \
			if (stcb->sctp_socket->so_snd.sb_cc >= tp1->book_size) { \
				atomic_subtract_int(&((stcb)->sctp_socket->so_snd.sb_cc), tp1->book_size); \
			} else { \
				stcb->sctp_socket->so_snd.sb_cc = 0; \
			} \
		} \
        } \
} while (0)

#endif

#define sctp_free_spbufspace(stcb, asoc, sp)  \
do { \
 	if (sp->data != NULL) { \
                atomic_subtract_int(&(asoc)->chunks_on_out_queue, 1); \
		if ((asoc)->total_output_queue_size >= sp->length) { \
			atomic_subtract_int(&(asoc)->total_output_queue_size, sp->length); \
		} else { \
			(asoc)->total_output_queue_size = 0; \
		} \
   	        if (stcb->sctp_socket && ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) || \
	            (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL))) { \
			if (stcb->sctp_socket->so_snd.sb_cc >= sp->length) { \
				atomic_subtract_int(&stcb->sctp_socket->so_snd.sb_cc,sp->length); \
			} else { \
				stcb->sctp_socket->so_snd.sb_cc = 0; \
			} \
		} \
        } \
} while (0)

#define sctp_snd_sb_alloc(stcb, sz)  \
do { \
	atomic_add_int(&stcb->asoc.total_output_queue_size,sz); \
	if ((stcb->sctp_socket != NULL) && \
	    ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) || \
	     (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL))) { \
		atomic_add_int(&stcb->sctp_socket->so_snd.sb_cc,sz); \
	} \
} while (0)


int
sctp_soreceive(struct socket *so, struct sockaddr **psa,
    struct uio *uio,
    struct mbuf **mp0,
    struct mbuf **controlp,
    int *flagsp);


/* For those not passing mbufs, this does the
 * translations for you. Caller owns memory
 * of size controllen returned in controlp.
 */
int 
sctp_l_soreceive(struct socket *so,
    struct sockaddr **name,
    struct uio *uio,
    char **controlp,
    int *controllen,
    int *flag);


void
     sctp_misc_ints(uint8_t from, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

void
sctp_wakeup_log(struct sctp_tcb *stcb,
    uint32_t cumtsn,
    uint32_t wake_cnt, int from);

void sctp_log_strm_del_alt(struct sctp_tcb *stcb, uint32_t, uint16_t, uint16_t, int);

void sctp_log_nagle_event(struct sctp_tcb *stcb, int action);


void
     sctp_log_mb(struct mbuf *m, int from);

void
sctp_sblog(struct sockbuf *sb,
    struct sctp_tcb *stcb, int from, int incr);

void
sctp_log_strm_del(struct sctp_queued_to_read *control,
    struct sctp_queued_to_read *poschk,
    int from);
void sctp_log_cwnd(struct sctp_tcb *stcb, struct sctp_nets *, int, uint8_t);
void rto_logging(struct sctp_nets *net, int from);

void sctp_log_closing(struct sctp_inpcb *inp, struct sctp_tcb *stcb, int16_t loc);

void sctp_log_lock(struct sctp_inpcb *inp, struct sctp_tcb *stcb, uint8_t from);
void sctp_log_maxburst(struct sctp_tcb *stcb, struct sctp_nets *, int, int, uint8_t);
void sctp_log_block(uint8_t, struct socket *, struct sctp_association *, int);
void sctp_log_rwnd(uint8_t, uint32_t, uint32_t, uint32_t);
void sctp_log_mbcnt(uint8_t, uint32_t, uint32_t, uint32_t, uint32_t);
void sctp_log_rwnd_set(uint8_t, uint32_t, uint32_t, uint32_t, uint32_t);
int sctp_fill_stat_log(void *, size_t *);
void sctp_log_fr(uint32_t, uint32_t, uint32_t, int);
void sctp_log_sack(uint32_t, uint32_t, uint32_t, uint16_t, uint16_t, int);
void sctp_log_map(uint32_t, uint32_t, uint32_t, int);

void sctp_clr_stat_log(void);


#ifdef SCTP_AUDITING_ENABLED
void
sctp_auditing(int, struct sctp_inpcb *, struct sctp_tcb *,
    struct sctp_nets *);
void sctp_audit_log(uint8_t, uint8_t);

#endif


#endif				/* _KERNEL */
#endif
