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

/* $KAME: sctp_uio.h,v 1.11 2005/03/06 16:04:18 itojun Exp $	 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef __sctp_uio_h__
#define __sctp_uio_h__


#if ! defined(_KERNEL)
#include <stdint.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

typedef uint32_t sctp_assoc_t;

/* On/Off setup for subscription to events */
struct sctp_event_subscribe {
	uint8_t sctp_data_io_event;
	uint8_t sctp_association_event;
	uint8_t sctp_address_event;
	uint8_t sctp_send_failure_event;
	uint8_t sctp_peer_error_event;
	uint8_t sctp_shutdown_event;
	uint8_t sctp_partial_delivery_event;
	uint8_t sctp_adaptation_layer_event;
	uint8_t sctp_authentication_event;
	uint8_t sctp_stream_reset_events;
};

/* ancillary data types */
#define SCTP_INIT	0x0001
#define SCTP_SNDRCV	0x0002
#define SCTP_EXTRCV	0x0003
/*
 * ancillary data structures
 */
struct sctp_initmsg {
	uint32_t sinit_num_ostreams;
	uint32_t sinit_max_instreams;
	uint16_t sinit_max_attempts;
	uint16_t sinit_max_init_timeo;
};

/* We add 96 bytes to the size of sctp_sndrcvinfo.
 * This makes the current structure 128 bytes long
 * which is nicely 64 bit aligned but also has room
 * for us to add more and keep ABI compatability.
 * For example, already we have the sctp_extrcvinfo
 * when enabled which is 48 bytes.
 */

/*
 * The assoc up needs a verfid
 * all sendrcvinfo's need a verfid for SENDING only.
 */


#define SCTP_ALIGN_RESV_PAD 96
#define SCTP_ALIGN_RESV_PAD_SHORT 80

struct sctp_sndrcvinfo {
	uint16_t sinfo_stream;
	uint16_t sinfo_ssn;
	uint16_t sinfo_flags;
	uint32_t sinfo_ppid;
	uint32_t sinfo_context;
	uint32_t sinfo_timetolive;
	uint32_t sinfo_tsn;
	uint32_t sinfo_cumtsn;
	sctp_assoc_t sinfo_assoc_id;
	uint8_t __reserve_pad[SCTP_ALIGN_RESV_PAD];
};

struct sctp_extrcvinfo {
	struct sctp_sndrcvinfo sreinfo_sinfo;
	uint16_t sreinfo_next_flags;
	uint16_t sreinfo_next_stream;
	uint32_t sreinfo_next_aid;
	uint32_t sreinfo_next_length;
	uint32_t sreinfo_next_ppid;
	uint8_t __reserve_pad[SCTP_ALIGN_RESV_PAD_SHORT];
};

#define SCTP_NO_NEXT_MSG           0x0000
#define SCTP_NEXT_MSG_AVAIL        0x0001
#define SCTP_NEXT_MSG_ISCOMPLETE   0x0002
#define SCTP_NEXT_MSG_IS_UNORDERED 0x0004
#define SCTP_NEXT_MSG_IS_NOTIFICATION 0x0008

struct sctp_snd_all_completes {
	uint16_t sall_stream;
	uint16_t sall_flags;
	uint32_t sall_ppid;
	uint32_t sall_context;
	uint32_t sall_num_sent;
	uint32_t sall_num_failed;
};

/* Flags that go into the sinfo->sinfo_flags field */
#define SCTP_EOF 	  0x0100/* Start shutdown procedures */
#define SCTP_ABORT	  0x0200/* Send an ABORT to peer */
#define SCTP_UNORDERED 	  0x0400/* Message is un-ordered */
#define SCTP_ADDR_OVER	  0x0800/* Override the primary-address */
#define SCTP_SENDALL      0x1000/* Send this on all associations */
#define SCTP_EOR          0x2000/* end of message signal */
#define INVALID_SINFO_FLAG(x) (((x) & 0xffffff00 \
                                    & ~(SCTP_EOF | SCTP_ABORT | SCTP_UNORDERED |\
				        SCTP_ADDR_OVER | SCTP_SENDALL | SCTP_EOR)) != 0)
/* for the endpoint */

/* The lower byte is an enumeration of PR-SCTP policies */
#define SCTP_PR_SCTP_TTL  0x0001/* Time based PR-SCTP */
#define SCTP_PR_SCTP_BUF  0x0002/* Buffer based PR-SCTP */
#define SCTP_PR_SCTP_RTX  0x0003/* Number of retransmissions based PR-SCTP */

#define PR_SCTP_POLICY(x)         ((x) & 0xff)
#define PR_SCTP_ENABLED(x)        (PR_SCTP_POLICY(x) != 0)
#define PR_SCTP_TTL_ENABLED(x)    (PR_SCTP_POLICY(x) == SCTP_PR_SCTP_TTL)
#define PR_SCTP_BUF_ENABLED(x)    (PR_SCTP_POLICY(x) == SCTP_PR_SCTP_BUF)
#define PR_SCTP_RTX_ENABLED(x)    (PR_SCTP_POLICY(x) == SCTP_PR_SCTP_RTX)
#define PR_SCTP_INVALID_POLICY(x) (PR_SCTP_POLICY(x) > SCTP_PR_SCTP_RTX)
/* Stat's */
struct sctp_pcbinfo {
	uint32_t ep_count;
	uint32_t asoc_count;
	uint32_t laddr_count;
	uint32_t raddr_count;
	uint32_t chk_count;
	uint32_t readq_count;
	uint32_t free_chunks;
	uint32_t stream_oque;
};

struct sctp_sockstat {
	sctp_assoc_t ss_assoc_id;
	uint32_t ss_total_sndbuf;
	uint32_t ss_total_recv_buf;
};

/*
 * notification event structures
 */

/*
 * association change event
 */
struct sctp_assoc_change {
	uint16_t sac_type;
	uint16_t sac_flags;
	uint32_t sac_length;
	uint16_t sac_state;
	uint16_t sac_error;
	uint16_t sac_outbound_streams;
	uint16_t sac_inbound_streams;
	sctp_assoc_t sac_assoc_id;
};

/* sac_state values */
#define SCTP_COMM_UP		0x0001
#define SCTP_COMM_LOST		0x0002
#define SCTP_RESTART		0x0003
#define SCTP_SHUTDOWN_COMP	0x0004
#define SCTP_CANT_STR_ASSOC	0x0005


/*
 * Address event
 */
struct sctp_paddr_change {
	uint16_t spc_type;
	uint16_t spc_flags;
	uint32_t spc_length;
	struct sockaddr_storage spc_aaddr;
	uint32_t spc_state;
	uint32_t spc_error;
	sctp_assoc_t spc_assoc_id;
};

/* paddr state values */
#define SCTP_ADDR_AVAILABLE	0x0001
#define SCTP_ADDR_UNREACHABLE	0x0002
#define SCTP_ADDR_REMOVED	0x0003
#define SCTP_ADDR_ADDED		0x0004
#define SCTP_ADDR_MADE_PRIM	0x0005
#define SCTP_ADDR_CONFIRMED	0x0006

/*
 * CAUTION: these are user exposed SCTP addr reachability states must be
 * compatible with SCTP_ADDR states in sctp_constants.h
 */
#ifdef SCTP_ACTIVE
#undef SCTP_ACTIVE
#endif
#define SCTP_ACTIVE		0x0001	/* SCTP_ADDR_REACHABLE */

#ifdef SCTP_INACTIVE
#undef SCTP_INACTIVE
#endif
#define SCTP_INACTIVE		0x0002	/* SCTP_ADDR_NOT_REACHABLE */

#ifdef SCTP_UNCONFIRMED
#undef SCTP_UNCONFIRMED
#endif
#define SCTP_UNCONFIRMED	0x0200	/* SCTP_ADDR_UNCONFIRMED */

#ifdef SCTP_NOHEARTBEAT
#undef SCTP_NOHEARTBEAT
#endif
#define SCTP_NOHEARTBEAT	0x0040	/* SCTP_ADDR_NOHB */


/* remote error events */
struct sctp_remote_error {
	uint16_t sre_type;
	uint16_t sre_flags;
	uint32_t sre_length;
	uint16_t sre_error;
	sctp_assoc_t sre_assoc_id;
	uint8_t sre_data[4];
};

/* data send failure event */
struct sctp_send_failed {
	uint16_t ssf_type;
	uint16_t ssf_flags;
	uint32_t ssf_length;
	uint32_t ssf_error;
	struct sctp_sndrcvinfo ssf_info;
	sctp_assoc_t ssf_assoc_id;
	uint8_t ssf_data[0];
};

/* flag that indicates state of data */
#define SCTP_DATA_UNSENT	0x0001	/* inqueue never on wire */
#define SCTP_DATA_SENT		0x0002	/* on wire at failure */

/* shutdown event */
struct sctp_shutdown_event {
	uint16_t sse_type;
	uint16_t sse_flags;
	uint32_t sse_length;
	sctp_assoc_t sse_assoc_id;
};

/* Adaptation layer indication stuff */
struct sctp_adaptation_event {
	uint16_t sai_type;
	uint16_t sai_flags;
	uint32_t sai_length;
	uint32_t sai_adaptation_ind;
	sctp_assoc_t sai_assoc_id;
};

struct sctp_setadaptation {
	uint32_t ssb_adaptation_ind;
};

/* compatable old spelling */
struct sctp_adaption_event {
	uint16_t sai_type;
	uint16_t sai_flags;
	uint32_t sai_length;
	uint32_t sai_adaption_ind;
	sctp_assoc_t sai_assoc_id;
};

struct sctp_setadaption {
	uint32_t ssb_adaption_ind;
};


/*
 * Partial Delivery API event
 */
struct sctp_pdapi_event {
	uint16_t pdapi_type;
	uint16_t pdapi_flags;
	uint32_t pdapi_length;
	uint32_t pdapi_indication;
	uint16_t pdapi_stream;
	uint16_t pdapi_seq;
	sctp_assoc_t pdapi_assoc_id;
};

/* indication values */
#define SCTP_PARTIAL_DELIVERY_ABORTED	0x0001


/*
 * authentication key event
 */
struct sctp_authkey_event {
	uint16_t auth_type;
	uint16_t auth_flags;
	uint32_t auth_length;
	uint16_t auth_keynumber;
	uint16_t auth_altkeynumber;
	uint32_t auth_indication;
	sctp_assoc_t auth_assoc_id;
};

/* indication values */
#define SCTP_AUTH_NEWKEY	0x0001


/*
 * stream reset event
 */
struct sctp_stream_reset_event {
	uint16_t strreset_type;
	uint16_t strreset_flags;
	uint32_t strreset_length;
	sctp_assoc_t strreset_assoc_id;
	uint16_t strreset_list[0];
};

/* flags in strreset_flags field */
#define SCTP_STRRESET_INBOUND_STR  0x0001
#define SCTP_STRRESET_OUTBOUND_STR 0x0002
#define SCTP_STRRESET_ALL_STREAMS  0x0004
#define SCTP_STRRESET_STREAM_LIST  0x0008
#define SCTP_STRRESET_FAILED       0x0010


/* SCTP notification event */
struct sctp_tlv {
	uint16_t sn_type;
	uint16_t sn_flags;
	uint32_t sn_length;
};

union sctp_notification {
	struct sctp_tlv sn_header;
	struct sctp_assoc_change sn_assoc_change;
	struct sctp_paddr_change sn_paddr_change;
	struct sctp_remote_error sn_remote_error;
	struct sctp_send_failed sn_send_failed;
	struct sctp_shutdown_event sn_shutdown_event;
	struct sctp_adaptation_event sn_adaptation_event;
	/* compatability same as above */
	struct sctp_adaption_event sn_adaption_event;
	struct sctp_pdapi_event sn_pdapi_event;
	struct sctp_authkey_event sn_auth_event;
	struct sctp_stream_reset_event sn_strreset_event;
};

/* notification types */
#define SCTP_ASSOC_CHANGE		0x0001
#define SCTP_PEER_ADDR_CHANGE		0x0002
#define SCTP_REMOTE_ERROR		0x0003
#define SCTP_SEND_FAILED		0x0004
#define SCTP_SHUTDOWN_EVENT		0x0005
#define SCTP_ADAPTATION_INDICATION	0x0006
/* same as above */
#define SCTP_ADAPTION_INDICATION	0x0006
#define SCTP_PARTIAL_DELIVERY_EVENT	0x0007
#define SCTP_AUTHENTICATION_EVENT	0x0008
#define SCTP_STREAM_RESET_EVENT		0x0009


/*
 * socket option structs
 */

struct sctp_paddrparams {
	sctp_assoc_t spp_assoc_id;
	struct sockaddr_storage spp_address;
	uint32_t spp_hbinterval;
	uint16_t spp_pathmaxrxt;
	uint32_t spp_pathmtu;
	uint32_t spp_flags;
	uint32_t spp_ipv6_flowlabel;
	uint8_t spp_ipv4_tos;

};

#define SPP_HB_ENABLE		0x00000001
#define SPP_HB_DISABLE		0x00000002
#define SPP_HB_DEMAND		0x00000004
#define SPP_PMTUD_ENABLE	0x00000008
#define SPP_PMTUD_DISABLE	0x00000010
#define SPP_HB_TIME_IS_ZERO     0x00000080
#define SPP_IPV6_FLOWLABEL      0x00000100
#define SPP_IPV4_TOS            0x00000200

struct sctp_paddrinfo {
	sctp_assoc_t spinfo_assoc_id;
	struct sockaddr_storage spinfo_address;
	int32_t spinfo_state;
	uint32_t spinfo_cwnd;
	uint32_t spinfo_srtt;
	uint32_t spinfo_rto;
	uint32_t spinfo_mtu;
};

struct sctp_rtoinfo {
	sctp_assoc_t srto_assoc_id;
	uint32_t srto_initial;
	uint32_t srto_max;
	uint32_t srto_min;
};

struct sctp_assocparams {
	sctp_assoc_t sasoc_assoc_id;
	uint16_t sasoc_asocmaxrxt;
	uint16_t sasoc_number_peer_destinations;
	uint32_t sasoc_peer_rwnd;
	uint32_t sasoc_local_rwnd;
	uint32_t sasoc_cookie_life;
};

struct sctp_setprim {
	sctp_assoc_t ssp_assoc_id;
	struct sockaddr_storage ssp_addr;
};

struct sctp_setpeerprim {
	sctp_assoc_t sspp_assoc_id;
	struct sockaddr_storage sspp_addr;
};

struct sctp_getaddresses {
	sctp_assoc_t sget_assoc_id;
	/* addr is filled in for N * sockaddr_storage */
	struct sockaddr addr[1];
};

struct sctp_setstrm_timeout {
	sctp_assoc_t ssto_assoc_id;
	uint32_t ssto_timeout;
	uint32_t ssto_streamid_start;
	uint32_t ssto_streamid_end;
};

struct sctp_status {
	sctp_assoc_t sstat_assoc_id;
	int32_t sstat_state;
	uint32_t sstat_rwnd;
	uint16_t sstat_unackdata;
	uint16_t sstat_penddata;
	uint16_t sstat_instrms;
	uint16_t sstat_outstrms;
	uint32_t sstat_fragmentation_point;
	struct sctp_paddrinfo sstat_primary;
};

/*
 * AUTHENTICATION support
 */
/* SCTP_AUTH_CHUNK */
struct sctp_authchunk {
	uint8_t sauth_chunk;
};

/* SCTP_AUTH_KEY */
struct sctp_authkey {
	sctp_assoc_t sca_assoc_id;
	uint16_t sca_keynumber;
	uint8_t sca_key[0];
};

/* SCTP_HMAC_IDENT */
struct sctp_hmacalgo {
	uint16_t shmac_idents[0];
};

/* AUTH hmac_id */
#define SCTP_AUTH_HMAC_ID_RSVD		0x0000
#define SCTP_AUTH_HMAC_ID_SHA1		0x0001	/* default, mandatory */
#define SCTP_AUTH_HMAC_ID_MD5		0x0002	/* deprecated */
#define SCTP_AUTH_HMAC_ID_SHA256	0x0003
#define SCTP_AUTH_HMAC_ID_SHA224	0x8001
#define SCTP_AUTH_HMAC_ID_SHA384	0x8002
#define SCTP_AUTH_HMAC_ID_SHA512	0x8003


/* SCTP_AUTH_ACTIVE_KEY / SCTP_AUTH_DELETE_KEY */
struct sctp_authkeyid {
	sctp_assoc_t scact_assoc_id;
	uint16_t scact_keynumber;
};

/* SCTP_PEER_AUTH_CHUNKS / SCTP_LOCAL_AUTH_CHUNKS */
struct sctp_authchunks {
	sctp_assoc_t gauth_assoc_id;
	uint8_t gauth_chunks[0];
};

struct sctp_assoc_value {
	sctp_assoc_t assoc_id;
	uint32_t assoc_value;
};

struct sctp_assoc_ids {
	sctp_assoc_t gaids_assoc_id[0];
};

struct sctp_sack_info {
	sctp_assoc_t sack_assoc_id;
	uint32_t sack_delay;
	uint32_t sack_freq;
};

struct sctp_cwnd_args {
	struct sctp_nets *net;	/* network to */
	uint32_t cwnd_new_value;/* cwnd in k */
	uint32_t inflight;	/* flightsize in k */
	uint32_t pseudo_cumack;
	int cwnd_augment;	/* increment to it */
	uint8_t meets_pseudo_cumack;
	uint8_t need_new_pseudo_cumack;
	uint8_t cnt_in_send;
	uint8_t cnt_in_str;
};

struct sctp_blk_args {
	uint32_t onsb;		/* in 1k bytes */
	uint32_t sndlen;	/* len of send being attempted */
	uint32_t peer_rwnd;	/* rwnd of peer */
	uint16_t send_sent_qcnt;/* chnk cnt */
	uint16_t stream_qcnt;	/* chnk cnt */
	uint16_t chunks_on_oque;/* chunks out */
	uint16_t flight_size;	/* flight size in k */
};

/*
 * Max we can reset in one setting, note this is dictated not by the define
 * but the size of a mbuf cluster so don't change this define and think you
 * can specify more. You must do multiple resets if you want to reset more
 * than SCTP_MAX_EXPLICIT_STR_RESET.
 */
#define SCTP_MAX_EXPLICT_STR_RESET   1000

#define SCTP_RESET_LOCAL_RECV  0x0001
#define SCTP_RESET_LOCAL_SEND  0x0002
#define SCTP_RESET_BOTH        0x0003
#define SCTP_RESET_TSN         0x0004

struct sctp_stream_reset {
	sctp_assoc_t strrst_assoc_id;
	uint16_t strrst_flags;
	uint16_t strrst_num_streams;	/* 0 == ALL */
	uint16_t strrst_list[0];/* list if strrst_num_streams is not 0 */
};


struct sctp_get_nonce_values {
	sctp_assoc_t gn_assoc_id;
	uint32_t gn_peers_tag;
	uint32_t gn_local_tag;
};

/* Debugging logs */
struct sctp_str_log {
	void *stcb;
	uint32_t n_tsn;
	uint32_t e_tsn;
	uint16_t n_sseq;
	uint16_t e_sseq;
	uint16_t strm;
};

struct sctp_sb_log {
	void *stcb;
	uint32_t so_sbcc;
	uint32_t stcb_sbcc;
	uint32_t incr;
};

struct sctp_fr_log {
	uint32_t largest_tsn;
	uint32_t largest_new_tsn;
	uint32_t tsn;
};

struct sctp_fr_map {
	uint32_t base;
	uint32_t cum;
	uint32_t high;
};

struct sctp_rwnd_log {
	uint32_t rwnd;
	uint32_t send_size;
	uint32_t overhead;
	uint32_t new_rwnd;
};

struct sctp_mbcnt_log {
	uint32_t total_queue_size;
	uint32_t size_change;
	uint32_t total_queue_mb_size;
	uint32_t mbcnt_change;
};

struct sctp_sack_log {
	uint32_t cumack;
	uint32_t oldcumack;
	uint32_t tsn;
	uint16_t numGaps;
	uint16_t numDups;
};

struct sctp_lock_log {
	void *sock;
	void *inp;
	uint8_t tcb_lock;
	uint8_t inp_lock;
	uint8_t info_lock;
	uint8_t sock_lock;
	uint8_t sockrcvbuf_lock;
	uint8_t socksndbuf_lock;
	uint8_t create_lock;
	uint8_t resv;
};

struct sctp_rto_log {
	void *net;
	uint32_t rtt;
};

struct sctp_nagle_log {
	void *stcb;
	uint32_t total_flight;
	uint32_t total_in_queue;
	uint16_t count_in_queue;
	uint16_t count_in_flight;
};

struct sctp_sbwake_log {
	void *stcb;
	uint16_t send_q;
	uint16_t sent_q;
	uint16_t flight;
	uint16_t wake_cnt;
	uint8_t stream_qcnt;	/* chnk cnt */
	uint8_t chunks_on_oque;	/* chunks out */
	uint8_t sbflags;
	uint8_t sctpflags;
};

struct sctp_misc_info {
	uint32_t log1;
	uint32_t log2;
	uint32_t log3;
	uint32_t log4;
};

struct sctp_log_closing {
	void *inp;
	void *stcb;
	uint32_t sctp_flags;
	uint16_t state;
	int16_t loc;
};

struct sctp_mbuf_log {
	struct mbuf *mp;
	caddr_t ext;
	caddr_t data;
	uint16_t size;
	uint8_t refcnt;
	uint8_t mbuf_flags;
};

struct sctp_cwnd_log {
	uint64_t time_event;
	uint8_t from;
	uint8_t event_type;
	uint8_t resv[2];
	union {
		struct sctp_log_closing close;
		struct sctp_blk_args blk;
		struct sctp_cwnd_args cwnd;
		struct sctp_str_log strlog;
		struct sctp_fr_log fr;
		struct sctp_fr_map map;
		struct sctp_rwnd_log rwnd;
		struct sctp_mbcnt_log mbcnt;
		struct sctp_sack_log sack;
		struct sctp_lock_log lock;
		struct sctp_rto_log rto;
		struct sctp_sb_log sb;
		struct sctp_nagle_log nagle;
		struct sctp_sbwake_log wake;
		struct sctp_mbuf_log mb;
		struct sctp_misc_info misc;
	}     x;
};

struct sctp_cwnd_log_req {
	int num_in_log;		/* Number in log */
	int num_ret;		/* Number returned */
	int start_at;		/* start at this one */
	int end_at;		/* end at this one */
	struct sctp_cwnd_log log[0];
};

struct sctpstat {
	/* MIB according to RFC 3873 */
	u_long sctps_currestab;	/* sctpStats  1   (Gauge32) */
	u_long sctps_activeestab;	/* sctpStats  2 (Counter32) */
	u_long sctps_restartestab;
	u_long sctps_collisionestab;
	u_long sctps_passiveestab;	/* sctpStats  3 (Counter32) */
	u_long sctps_aborted;	/* sctpStats  4 (Counter32) */
	u_long sctps_shutdown;	/* sctpStats  5 (Counter32) */
	u_long sctps_outoftheblue;	/* sctpStats  6 (Counter32) */
	u_long sctps_checksumerrors;	/* sctpStats  7 (Counter32) */
	u_long sctps_outcontrolchunks;	/* sctpStats  8 (Counter64) */
	u_long sctps_outorderchunks;	/* sctpStats  9 (Counter64) */
	u_long sctps_outunorderchunks;	/* sctpStats 10 (Counter64) */
	u_long sctps_incontrolchunks;	/* sctpStats 11 (Counter64) */
	u_long sctps_inorderchunks;	/* sctpStats 12 (Counter64) */
	u_long sctps_inunorderchunks;	/* sctpStats 13 (Counter64) */
	u_long sctps_fragusrmsgs;	/* sctpStats 14 (Counter64) */
	u_long sctps_reasmusrmsgs;	/* sctpStats 15 (Counter64) */
	u_long sctps_outpackets;/* sctpStats 16 (Counter64) */
	u_long sctps_inpackets;	/* sctpStats 17 (Counter64) */
	struct timeval sctps_discontinuitytime;	/* sctpStats 18 (TimeStamp) */
	/* input statistics: */
	u_long sctps_recvpackets;	/* total input packets        */
	u_long sctps_recvdatagrams;	/* total input datagrams      */
	u_long sctps_recvpktwithdata;	/* total packets that had data */
	u_long sctps_recvsacks;	/* total input SACK chunks    */
	u_long sctps_recvdata;	/* total input DATA chunks    */
	u_long sctps_recvdupdata;	/* total input duplicate DATA chunks */
	u_long sctps_recvheartbeat;	/* total input HB chunks      */
	u_long sctps_recvheartbeatack;	/* total input HB-ACK chunks  */
	u_long sctps_recvecne;	/* total input ECNE chunks    */
	u_long sctps_recvauth;	/* total input AUTH chunks    */
	u_long sctps_recvauthmissing;	/* total input chunks missing AUTH */
	u_long sctps_recvivalhmacid;	/* total number of invalid HMAC ids
					 * received */
	u_long sctps_recvivalkeyid;	/* total number of invalid secret ids
					 * received */
	u_long sctps_recvauthfailed;	/* total number of auth failed */
	u_long sctps_recvexpress;	/* total fast path receives all one
					 * chunk */
	u_long sctps_recvexpressm;	/* total fast path multi-part data */
	/* output statistics: */
	u_long sctps_sendpackets;	/* total output packets       */
	u_long sctps_sendsacks;	/* total output SACKs         */
	u_long sctps_senddata;	/* total output DATA chunks   */
	u_long sctps_sendretransdata;	/* total output retransmitted DATA
					 * chunks */
	u_long sctps_sendfastretrans;	/* total output fast retransmitted
					 * DATA chunks */
	u_long sctps_sendmultfastretrans;	/* total FR's that happened
						 * more than once to same
						 * chunk (u-del multi-fr
						 * algo). */
	u_long sctps_sendheartbeat;	/* total output HB chunks     */
	u_long sctps_sendecne;	/* total output ECNE chunks    */
	u_long sctps_sendauth;	/* total output AUTH chunks FIXME   */
	u_long sctps_senderrors;/* ip_output error counter */
	/* PCKDROPREP statistics: */
	u_long sctps_pdrpfmbox;	/* Packet drop from middle box */
	u_long sctps_pdrpfehos;	/* P-drop from end host */
	u_long sctps_pdrpmbda;	/* P-drops with data */
	u_long sctps_pdrpmbct;	/* P-drops, non-data, non-endhost */
	u_long sctps_pdrpbwrpt;	/* P-drop, non-endhost, bandwidth rep only */
	u_long sctps_pdrpcrupt;	/* P-drop, not enough for chunk header */
	u_long sctps_pdrpnedat;	/* P-drop, not enough data to confirm */
	u_long sctps_pdrppdbrk;	/* P-drop, where process_chunk_drop said break */
	u_long sctps_pdrptsnnf;	/* P-drop, could not find TSN */
	u_long sctps_pdrpdnfnd;	/* P-drop, attempt reverse TSN lookup */
	u_long sctps_pdrpdiwnp;	/* P-drop, e-host confirms zero-rwnd */
	u_long sctps_pdrpdizrw;	/* P-drop, midbox confirms no space */
	u_long sctps_pdrpbadd;	/* P-drop, data did not match TSN */
	u_long sctps_pdrpmark;	/* P-drop, TSN's marked for Fast Retran */
	/* timeouts */
	u_long sctps_timoiterator;	/* Number of iterator timers that
					 * fired */
	u_long sctps_timodata;	/* Number of T3 data time outs */
	u_long sctps_timowindowprobe;	/* Number of window probe (T3) timers
					 * that fired */
	u_long sctps_timoinit;	/* Number of INIT timers that fired */
	u_long sctps_timosack;	/* Number of sack timers that fired */
	u_long sctps_timoshutdown;	/* Number of shutdown timers that
					 * fired */
	u_long sctps_timoheartbeat;	/* Number of heartbeat timers that
					 * fired */
	u_long sctps_timocookie;/* Number of times a cookie timeout fired */
	u_long sctps_timosecret;/* Number of times an endpoint changed its
				 * cookie secret */
	u_long sctps_timopathmtu;	/* Number of PMTU timers that fired */
	u_long sctps_timoshutdownack;	/* Number of shutdown ack timers that
					 * fired */
	u_long sctps_timoshutdownguard;	/* Number of shutdown guard timers
					 * that fired */
	u_long sctps_timostrmrst;	/* Number of stream reset timers that
					 * fired */
	u_long sctps_timoearlyfr;	/* Number of early FR timers that
					 * fired */
	u_long sctps_timoasconf;/* Number of times an asconf timer fired */
	u_long sctps_timoautoclose;	/* Number of times auto close timer
					 * fired */
	u_long sctps_timoassockill;	/* Number of asoc free timers expired */
	u_long sctps_timoinpkill;	/* Number of inp free timers expired */
	/* Early fast retransmission counters */
	u_long sctps_earlyfrstart;
	u_long sctps_earlyfrstop;
	u_long sctps_earlyfrmrkretrans;
	u_long sctps_earlyfrstpout;
	u_long sctps_earlyfrstpidsck1;
	u_long sctps_earlyfrstpidsck2;
	u_long sctps_earlyfrstpidsck3;
	u_long sctps_earlyfrstpidsck4;
	u_long sctps_earlyfrstrid;
	u_long sctps_earlyfrstrout;
	u_long sctps_earlyfrstrtmr;
	/* otheres */
	u_long sctps_hdrops;	/* packet shorter than header */
	u_long sctps_badsum;	/* checksum error             */
	u_long sctps_noport;	/* no endpoint for port       */
	u_long sctps_badvtag;	/* bad v-tag                  */
	u_long sctps_badsid;	/* bad SID                    */
	u_long sctps_nomem;	/* no memory                  */
	u_long sctps_fastretransinrtt;	/* number of multiple FR in a RTT
					 * window */
	u_long sctps_markedretrans;
	u_long sctps_naglesent;	/* nagle allowed sending      */
	u_long sctps_naglequeued;	/* nagle does't allow sending */
	u_long sctps_maxburstqueued;	/* max burst dosn't allow sending */
	u_long sctps_ifnomemqueued;	/* look ahead tells us no memory in
					 * interface ring buffer OR we had a
					 * send error and are queuing one
					 * send. */
	u_long sctps_windowprobed;	/* total number of window probes sent */
	u_long sctps_lowlevelerr;	/* total times an output error causes
					 * us to clamp down on next user send. */
	u_long sctps_lowlevelerrusr;	/* total times sctp_senderrors were
					 * caused from a user send from a user
					 * invoked send not a sack response */
	u_long sctps_datadropchklmt;	/* Number of in data drops due to
					 * chunk limit reached */
	u_long sctps_datadroprwnd;	/* Number of in data drops due to rwnd
					 * limit reached */
	u_long sctps_ecnereducedcwnd;	/* Number of times a ECN reduced the
					 * cwnd */
	u_long sctps_vtagexpress;	/* Used express lookup via vtag */
	u_long sctps_vtagbogus;	/* Collision in express lookup. */
	u_long sctps_primary_randry;	/* Number of times the sender ran dry
					 * of user data on primary */
	u_long sctps_cmt_randry;/* Same for above */
	u_long sctps_slowpath_sack;	/* Sacks the slow way */
	u_long sctps_wu_sacks_sent;	/* Window Update only sacks sent */
	u_long sctps_sends_with_flags;	/* number of sends with sinfo_flags
					 * !=0 */
	u_long sctps_sends_with_unord /* number of undordered sends */ ;
	u_long sctps_sends_with_eof;	/* number of sends with EOF flag set */
	u_long sctps_sends_with_abort;	/* number of sends with ABORT flag set */
	u_long sctps_protocol_drain_calls;	/* number of times protocol
						 * drain called */
	u_long sctps_protocol_drains_done;	/* number of times we did a
						 * protocol drain */
	u_long sctps_read_peeks;/* Number of times recv was called with peek */
	u_long sctps_cached_chk;/* Number of cached chunks used */
	u_long sctps_cached_strmoq;	/* Number of cached stream oq's used */
	u_long sctps_left_abandon;	/* Number of unread message abandonded
					 * by close */
	u_long sctps_send_burst_avoid;	/* Send burst avoidance, already max
					 * burst inflight to net */
	u_long sctps_send_cwnd_avoid;	/* Send cwnd full  avoidance, already
					 * max burst inflight to net */
	u_long sctps_fwdtsn_map_over;	/* number of map array over-runs via
					 * fwd-tsn's */
};

#define SCTP_STAT_INCR(_x) SCTP_STAT_INCR_BY(_x,1)
#define SCTP_STAT_DECR(_x) SCTP_STAT_DECR_BY(_x,1)
#define SCTP_STAT_INCR_BY(_x,_d) atomic_add_long(&sctpstat._x, _d)
#define SCTP_STAT_DECR_BY(_x,_d) atomic_subtract_long(&sctpstat._x, _d)
/* The following macros are for handling MIB values, */
#define SCTP_STAT_INCR_COUNTER32(_x) SCTP_STAT_INCR(_x)
#define SCTP_STAT_INCR_COUNTER64(_x) SCTP_STAT_INCR(_x)
#define SCTP_STAT_INCR_GAUGE32(_x) SCTP_STAT_INCR(_x)
#define SCTP_STAT_DECR_COUNTER32(_x) SCTP_STAT_DECR(_x)
#define SCTP_STAT_DECR_COUNTER64(_x) SCTP_STAT_DECR(_x)
#define SCTP_STAT_DECR_GAUGE32(_x) SCTP_STAT_DECR(_x)

union sctp_sockstore {
#if defined(INET) || !defined(_KERNEL)
	struct sockaddr_in sin;
#endif
#if defined(INET6) || !defined(_KERNEL)
	struct sockaddr_in6 sin6;
#endif
	struct sockaddr sa;
};

struct xsctp_inpcb {
	uint32_t last;
	uint16_t local_port;
	uint32_t flags;
	uint32_t features;
	uint32_t total_sends;
	uint32_t total_recvs;
	uint32_t total_nospaces;
	uint32_t fragmentation_point;
	uint16_t qlen;
	uint16_t maxqlen;
	/* add more endpoint specific data here */
};

struct xsctp_tcb {
	uint32_t last;
	uint16_t local_port;	/* sctpAssocEntry 3   */
	uint16_t remote_port;	/* sctpAssocEntry 4   */
	union sctp_sockstore primary_addr;	/* sctpAssocEntry 5/6 */
	uint32_t heartbeat_interval;	/* sctpAssocEntry 7   */
	uint32_t state;		/* sctpAssocEntry 8   */
	uint32_t in_streams;	/* sctpAssocEntry 9   */
	uint32_t out_streams;	/* sctpAssocEntry 10  */
	uint32_t max_nr_retrans;/* sctpAssocEntry 11  */
	uint32_t primary_process;	/* sctpAssocEntry 12  */
	uint32_t T1_expireries;	/* sctpAssocEntry 13  */
	uint32_t T2_expireries;	/* sctpAssocEntry 14  */
	uint32_t retransmitted_tsns;	/* sctpAssocEntry 15  */
	struct timeval start_time;	/* sctpAssocEntry 16  */
	struct timeval discontinuity_time;	/* sctpAssocEntry 17  */
	uint32_t total_sends;
	uint32_t total_recvs;
	uint32_t local_tag;
	uint32_t remote_tag;
	uint32_t initial_tsn;
	uint32_t highest_tsn;
	uint32_t cumulative_tsn;
	uint32_t cumulative_tsn_ack;
	uint32_t mtu;
	uint32_t refcnt;
	/* add more association specific data here */
};

struct xsctp_laddr {
	uint32_t last;
	union sctp_sockstore address;	/* sctpAssocLocalAddrEntry 1/2 */
	struct timeval start_time;	/* sctpAssocLocalAddrEntry 3   */
	/* add more local address specific data */
};

struct xsctp_raddr {
	uint32_t last;
	union sctp_sockstore address;	/* sctpAssocLocalRemEntry 1/2 */
	uint8_t active;		/* sctpAssocLocalRemEntry 3   */
	uint8_t confirmed;	/* */
	uint8_t heartbeat_enabled;	/* sctpAssocLocalRemEntry 4   */
	uint32_t rto;		/* sctpAssocLocalRemEntry 5   */
	uint32_t max_path_rtx;	/* sctpAssocLocalRemEntry 6   */
	uint32_t rtx;		/* sctpAssocLocalRemEntry 7   */
	uint32_t error_counter;	/* */
	uint32_t cwnd;		/* */
	uint32_t flight_size;	/* */
	uint32_t mtu;		/* */
	struct timeval start_time;	/* sctpAssocLocalRemEntry 8   */
	/* add more remote address specific data */
};

/*
 * Kernel defined for sctp_send
 */
#if defined(_KERNEL)
int
sctp_lower_sosend(struct socket *so,
    struct sockaddr *addr,
    struct uio *uio,
    struct mbuf *i_pak,
    struct mbuf *control,
    int flags,
    int use_rcvinfo,
    struct sctp_sndrcvinfo *srcv
    ,struct thread *p
);

int
sctp_sorecvmsg(struct socket *so,
    struct uio *uio,
    struct mbuf **mp,
    struct sockaddr *from,
    int fromlen,
    int *msg_flags,
    struct sctp_sndrcvinfo *sinfo,
    int filling_sinfo);

#endif

/*
 * API system calls
 */
#if !(defined(_KERNEL))

__BEGIN_DECLS
int sctp_peeloff __P((int, sctp_assoc_t));
int sctp_bindx __P((int, struct sockaddr *, int, int));
int sctp_connectx __P((int, const struct sockaddr *, int, sctp_assoc_t *));
int sctp_getaddrlen __P((sa_family_t));
int sctp_getpaddrs __P((int, sctp_assoc_t, struct sockaddr **));
void sctp_freepaddrs __P((struct sockaddr *));
int sctp_getladdrs __P((int, sctp_assoc_t, struct sockaddr **));
void sctp_freeladdrs __P((struct sockaddr *));
int sctp_opt_info __P((int, sctp_assoc_t, int, void *, socklen_t *));

ssize_t sctp_sendmsg 
__P((int, const void *, size_t,
    const struct sockaddr *,
    socklen_t, uint32_t, uint32_t, uint16_t, uint32_t, uint32_t));

	ssize_t sctp_send __P((int sd, const void *msg, size_t len,
              const struct sctp_sndrcvinfo *sinfo, int flags));

	ssize_t sctp_sendx __P((int sd, const void *msg, size_t len,
               struct sockaddr *addrs, int addrcnt,
               struct sctp_sndrcvinfo *sinfo, int flags));

	ssize_t sctp_sendmsgx __P((int sd, const void *, size_t,
                  struct sockaddr *, int,
                  uint32_t, uint32_t, uint16_t, uint32_t, uint32_t));

	sctp_assoc_t sctp_getassocid __P((int sd, struct sockaddr *sa));

	ssize_t sctp_recvmsg __P((int, void *, size_t, struct sockaddr *,
                 socklen_t *, struct sctp_sndrcvinfo *, int *));

__END_DECLS

#endif				/* !_KERNEL */
#endif				/* !__sctp_uio_h__ */
