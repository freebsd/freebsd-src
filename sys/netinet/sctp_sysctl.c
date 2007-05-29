/*-
 * Copyright (c) 2007, by Cisco Systems, Inc. All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <netinet/sctp_os.h>
#include <netinet/sctp_constants.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
/*
 * sysctl tunable variables
 */
uint32_t sctp_sendspace = (128 * 1024);
uint32_t sctp_recvspace = 128 * (1024 +
#ifdef INET6
    sizeof(struct sockaddr_in6)
#else
    sizeof(struct sockaddr_in)
#endif
);
uint32_t sctp_mbuf_threshold_count = SCTP_DEFAULT_MBUFS_IN_CHAIN;
uint32_t sctp_auto_asconf = SCTP_DEFAULT_AUTO_ASCONF;
uint32_t sctp_ecn_enable = 1;
uint32_t sctp_ecn_nonce = 0;
uint32_t sctp_strict_sacks = 0;
uint32_t sctp_no_csum_on_loopback = 1;
uint32_t sctp_strict_init = 1;
uint32_t sctp_abort_if_one_2_one_hits_limit = 0;
uint32_t sctp_strict_data_order = 0;

uint32_t sctp_peer_chunk_oh = sizeof(struct mbuf);
uint32_t sctp_max_burst_default = SCTP_DEF_MAX_BURST;
uint32_t sctp_use_cwnd_based_maxburst = 1;
uint32_t sctp_do_drain = 1;
uint32_t sctp_hb_maxburst = SCTP_DEF_MAX_BURST;

uint32_t sctp_max_chunks_on_queue = SCTP_ASOC_MAX_CHUNKS_ON_QUEUE;
uint32_t sctp_delayed_sack_time_default = SCTP_RECV_MSEC;
uint32_t sctp_sack_freq_default = SCTP_DEFAULT_SACK_FREQ;
uint32_t sctp_heartbeat_interval_default = SCTP_HB_DEFAULT_MSEC;
uint32_t sctp_pmtu_raise_time_default = SCTP_DEF_PMTU_RAISE_SEC;
uint32_t sctp_shutdown_guard_time_default = SCTP_DEF_MAX_SHUTDOWN_SEC;
uint32_t sctp_secret_lifetime_default = SCTP_DEFAULT_SECRET_LIFE_SEC;
uint32_t sctp_rto_max_default = SCTP_RTO_UPPER_BOUND;
uint32_t sctp_rto_min_default = SCTP_RTO_LOWER_BOUND;
uint32_t sctp_rto_initial_default = SCTP_RTO_INITIAL;
uint32_t sctp_init_rto_max_default = SCTP_RTO_UPPER_BOUND;
uint32_t sctp_valid_cookie_life_default = SCTP_DEFAULT_COOKIE_LIFE;
uint32_t sctp_init_rtx_max_default = SCTP_DEF_MAX_INIT;
uint32_t sctp_assoc_rtx_max_default = SCTP_DEF_MAX_SEND;
uint32_t sctp_path_rtx_max_default = SCTP_DEF_MAX_PATH_RTX;
uint32_t sctp_nr_outgoing_streams_default = SCTP_OSTREAM_INITIAL;
uint32_t sctp_add_more_threshold = SCTP_DEFAULT_ADD_MORE;
uint32_t sctp_asoc_free_resc_limit = SCTP_DEF_ASOC_RESC_LIMIT;
uint32_t sctp_system_free_resc_limit = SCTP_DEF_SYSTEM_RESC_LIMIT;

uint32_t sctp_min_split_point = SCTP_DEFAULT_SPLIT_POINT_MIN;
uint32_t sctp_pcbtblsize = SCTP_PCBHASHSIZE;
uint32_t sctp_hashtblsize = SCTP_TCBHASHSIZE;
uint32_t sctp_chunkscale = SCTP_CHUNKQUEUE_SCALE;

uint32_t sctp_cmt_on_off = 0;
uint32_t sctp_cmt_use_dac = 0;
uint32_t sctp_max_retran_chunk = SCTPCTL_MAX_RETRAN_CHUNK_DEFAULT;


uint32_t sctp_L2_abc_variable = 1;
uint32_t sctp_early_fr = 0;
uint32_t sctp_early_fr_msec = SCTP_MINFR_MSEC_TIMER;
uint32_t sctp_says_check_for_deadlock = 0;
uint32_t sctp_asconf_auth_nochk = 0;
uint32_t sctp_auth_disable = 0;
uint32_t sctp_nat_friendly = 1;
uint32_t sctp_min_residual = SCTPCTL_MIN_RESIDUAL_DEFAULT;;


struct sctpstat sctpstat;

#ifdef SCTP_DEBUG
uint32_t sctp_debug_on = 0;

#endif



/* It returns an upper limit. No filtering is done here */
static unsigned int
number_of_addresses(struct sctp_inpcb *inp)
{
	int cnt;
	struct sctp_vrf *vrf;
	struct sctp_ifn *sctp_ifn;
	struct sctp_ifa *sctp_ifa;
	struct sctp_laddr *laddr;

	cnt = 0;
	/* neither Mac OS X nor FreeBSD support mulitple routing functions */
	if ((vrf = sctp_find_vrf(inp->def_vrf_id)) == NULL) {
		return (0);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		LIST_FOREACH(sctp_ifn, &vrf->ifnlist, next_ifn) {
			LIST_FOREACH(sctp_ifa, &sctp_ifn->ifalist, next_ifa) {
				if ((sctp_ifa->address.sa.sa_family == AF_INET) ||
				    (sctp_ifa->address.sa.sa_family == AF_INET6)) {
					cnt++;
				}
			}
		}
	} else {
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if ((laddr->ifa->address.sa.sa_family == AF_INET) ||
			    (laddr->ifa->address.sa.sa_family == AF_INET6)) {
				cnt++;
			}
		}
	}
	return (cnt);
}

static int
copy_out_local_addresses(struct sctp_inpcb *inp, struct sctp_tcb *stcb, struct sysctl_req *req)
{
	struct sctp_ifn *sctp_ifn;
	struct sctp_ifa *sctp_ifa;
	int loopback_scope, ipv4_local_scope, local_scope, site_scope;
	int ipv4_addr_legal, ipv6_addr_legal;
	struct sctp_vrf *vrf;
	struct xsctp_laddr xladdr;
	struct sctp_laddr *laddr;
	int error;

	/* Turn on all the appropriate scope */
	if (stcb) {
		/* use association specific values */
		loopback_scope = stcb->asoc.loopback_scope;
		ipv4_local_scope = stcb->asoc.ipv4_local_scope;
		local_scope = stcb->asoc.local_scope;
		site_scope = stcb->asoc.site_scope;
	} else {
		/* use generic values for endpoints */
		loopback_scope = 1;
		ipv4_local_scope = 1;
		local_scope = 1;
		site_scope = 1;
	}

	/* use only address families of interest */
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
		ipv6_addr_legal = 1;
		if (SCTP_IPV6_V6ONLY(inp)) {
			ipv4_addr_legal = 0;
		} else {
			ipv4_addr_legal = 1;
		}
	} else {
		ipv4_addr_legal = 1;
		ipv6_addr_legal = 0;
	}

	error = 0;

	/* neither Mac OS X nor FreeBSD support mulitple routing functions */
	if ((vrf = sctp_find_vrf(inp->def_vrf_id)) == NULL) {
		return (-1);
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		LIST_FOREACH(sctp_ifn, &vrf->ifnlist, next_ifn) {
			if ((loopback_scope == 0) && SCTP_IFN_IS_IFT_LOOP(sctp_ifn))
				/* Skip loopback if loopback_scope not set */
				continue;
			LIST_FOREACH(sctp_ifa, &sctp_ifn->ifalist, next_ifa) {
				if (stcb) {
					/*
					 * ignore if blacklisted at
					 * association level
					 */
					if (sctp_is_addr_restricted(stcb, sctp_ifa))
						continue;
				}
				if ((sctp_ifa->address.sa.sa_family == AF_INET) && (ipv4_addr_legal)) {
					struct sockaddr_in *sin;

					sin = (struct sockaddr_in *)&sctp_ifa->address.sa;
					if (sin->sin_addr.s_addr == 0)
						continue;
					if ((ipv4_local_scope == 0) && (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)))
						continue;
				} else if ((sctp_ifa->address.sa.sa_family == AF_INET6) && (ipv6_addr_legal)) {
					struct sockaddr_in6 *sin6;

					sin6 = (struct sockaddr_in6 *)&sctp_ifa->address.sa;
					if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
						continue;
					if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
						if (local_scope == 0)
							continue;
						if (sin6->sin6_scope_id == 0) {
							/*
							 * bad link local
							 * address
							 */
							if (sa6_recoverscope(sin6) != 0)
								continue;
						}
					}
					if ((site_scope == 0) && (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)))
						continue;
				} else
					continue;
				memset((void *)&xladdr, 0, sizeof(union sctp_sockstore));
				memcpy((void *)&xladdr.address, (const void *)&sctp_ifa->address, sizeof(union sctp_sockstore));
				(void)SCTP_GETTIME_TIMEVAL(&xladdr.start_time);
				SCTP_INP_RUNLOCK(inp);
				SCTP_INP_INFO_RUNLOCK();
				error = SYSCTL_OUT(req, &xladdr, sizeof(struct xsctp_laddr));
				if (error)
					return (error);
				else {
					SCTP_INP_INFO_RLOCK();
					SCTP_INP_RLOCK(inp);
				}
			}
		}
	} else {
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			/* ignore if blacklisted at association level */
			if (stcb && sctp_is_addr_restricted(stcb, laddr->ifa))
				continue;
			memset((void *)&xladdr, 0, sizeof(union sctp_sockstore));
			memcpy((void *)&xladdr.address, (const void *)&laddr->ifa->address, sizeof(union sctp_sockstore));
			xladdr.start_time = laddr->start_time;
			SCTP_INP_RUNLOCK(inp);
			SCTP_INP_INFO_RUNLOCK();
			error = SYSCTL_OUT(req, &xladdr, sizeof(struct xsctp_laddr));
			if (error)
				return (error);
			else {
				SCTP_INP_INFO_RLOCK();
				SCTP_INP_RLOCK(inp);
			}
		}
	}
	memset((void *)&xladdr, 0, sizeof(union sctp_sockstore));
	xladdr.last = 1;
	error = SYSCTL_OUT(req, &xladdr, sizeof(struct xsctp_laddr));
	if (error)
		return (error);
	else
		return (0);
}

/*
 * sysctl functions
 */
static int
sctp_assoclist(SYSCTL_HANDLER_ARGS)
{
	unsigned int number_of_endpoints;
	unsigned int number_of_local_addresses;
	unsigned int number_of_associations;
	unsigned int number_of_remote_addresses;
	unsigned int n;
	int error;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;
	struct xsctp_inpcb xinpcb;
	struct xsctp_tcb xstcb;
	struct xsctp_raddr xraddr;

	number_of_endpoints = 0;
	number_of_local_addresses = 0;
	number_of_associations = 0;
	number_of_remote_addresses = 0;

	SCTP_INP_INFO_RLOCK();
	if (req->oldptr == USER_ADDR_NULL) {
		LIST_FOREACH(inp, &sctppcbinfo.listhead, sctp_list) {
			SCTP_INP_RLOCK(inp);
			number_of_endpoints++;
			number_of_local_addresses += number_of_addresses(inp);
			LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
				number_of_associations++;
				number_of_local_addresses += number_of_addresses(inp);
				TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
					number_of_remote_addresses++;
				}
			}
			SCTP_INP_RUNLOCK(inp);
		}
		SCTP_INP_INFO_RUNLOCK();
		n = (number_of_endpoints + 1) * sizeof(struct xsctp_inpcb) +
		    (number_of_local_addresses + number_of_endpoints + number_of_associations) * sizeof(struct xsctp_laddr) +
		    (number_of_associations + number_of_endpoints) * sizeof(struct xsctp_tcb) +
		    (number_of_remote_addresses + number_of_associations) * sizeof(struct xsctp_raddr);

		/* request some more memory than needed */
		req->oldidx = (n + n / 8);
		return 0;
	}
	if (req->newptr != USER_ADDR_NULL) {
		SCTP_INP_INFO_RUNLOCK();
		return EPERM;
	}
	LIST_FOREACH(inp, &sctppcbinfo.listhead, sctp_list) {
		SCTP_INP_RLOCK(inp);
		xinpcb.last = 0;
		xinpcb.local_port = ntohs(inp->sctp_lport);
		xinpcb.flags = inp->sctp_flags;
		xinpcb.features = inp->sctp_features;
		xinpcb.total_sends = inp->total_sends;
		xinpcb.total_recvs = inp->total_recvs;
		xinpcb.total_nospaces = inp->total_nospaces;
		xinpcb.fragmentation_point = inp->sctp_frag_point;
		xinpcb.qlen = inp->sctp_socket->so_qlen;
		xinpcb.maxqlen = inp->sctp_socket->so_qlimit;
		SCTP_INP_INCR_REF(inp);
		SCTP_INP_RUNLOCK(inp);
		SCTP_INP_INFO_RUNLOCK();
		error = SYSCTL_OUT(req, &xinpcb, sizeof(struct xsctp_inpcb));
		if (error) {
			SCTP_INP_DECR_REF(inp);
			return error;
		}
		SCTP_INP_INFO_RLOCK();
		SCTP_INP_RLOCK(inp);
		error = copy_out_local_addresses(inp, NULL, req);
		if (error) {
			SCTP_INP_DECR_REF(inp);
			return error;
		}
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			SCTP_TCB_LOCK(stcb);
			atomic_add_int(&stcb->asoc.refcnt, 1);
			SCTP_TCB_UNLOCK(stcb);
			xstcb.last = 0;
			xstcb.local_port = ntohs(inp->sctp_lport);
			xstcb.remote_port = ntohs(stcb->rport);
			if (stcb->asoc.primary_destination != NULL)
				xstcb.primary_addr = stcb->asoc.primary_destination->ro._l_addr;
			xstcb.heartbeat_interval = stcb->asoc.heart_beat_delay;
			xstcb.state = SCTP_GET_STATE(&stcb->asoc);	/* FIXME */
			xstcb.in_streams = stcb->asoc.streamincnt;
			xstcb.out_streams = stcb->asoc.streamoutcnt;
			xstcb.max_nr_retrans = stcb->asoc.overall_error_count;
			xstcb.primary_process = 0;	/* not really supported
							 * yet */
			xstcb.T1_expireries = stcb->asoc.timoinit + stcb->asoc.timocookie;
			xstcb.T2_expireries = stcb->asoc.timoshutdown + stcb->asoc.timoshutdownack;
			xstcb.retransmitted_tsns = stcb->asoc.marked_retrans;
			xstcb.start_time = stcb->asoc.start_time;
			xstcb.discontinuity_time = stcb->asoc.discontinuity_time;

			xstcb.total_sends = stcb->total_sends;
			xstcb.total_recvs = stcb->total_recvs;
			xstcb.local_tag = stcb->asoc.my_vtag;
			xstcb.remote_tag = stcb->asoc.peer_vtag;
			xstcb.initial_tsn = stcb->asoc.init_seq_number;
			xstcb.highest_tsn = stcb->asoc.sending_seq - 1;
			xstcb.cumulative_tsn = stcb->asoc.last_acked_seq;
			xstcb.cumulative_tsn_ack = stcb->asoc.cumulative_tsn;
			xstcb.mtu = stcb->asoc.smallest_mtu;
			xstcb.refcnt = stcb->asoc.refcnt;
			SCTP_INP_RUNLOCK(inp);
			SCTP_INP_INFO_RUNLOCK();
			error = SYSCTL_OUT(req, &xstcb, sizeof(struct xsctp_tcb));
			if (error) {
				SCTP_INP_DECR_REF(inp);
				atomic_add_int(&stcb->asoc.refcnt, -1);
				return error;
			}
			SCTP_INP_INFO_RLOCK();
			SCTP_INP_RLOCK(inp);
			error = copy_out_local_addresses(inp, stcb, req);
			if (error) {
				SCTP_INP_DECR_REF(inp);
				atomic_add_int(&stcb->asoc.refcnt, -1);
				return error;
			}
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
				xraddr.last = 0;
				xraddr.address = net->ro._l_addr;
				xraddr.active = ((net->dest_state & SCTP_ADDR_REACHABLE) == SCTP_ADDR_REACHABLE);
				xraddr.confirmed = ((net->dest_state & SCTP_ADDR_UNCONFIRMED) == 0);
				xraddr.heartbeat_enabled = ((net->dest_state & SCTP_ADDR_NOHB) == 0);
				xraddr.rto = net->RTO;
				xraddr.max_path_rtx = net->failure_threshold;
				xraddr.rtx = net->marked_retrans;
				xraddr.error_counter = net->error_count;
				xraddr.cwnd = net->cwnd;
				xraddr.flight_size = net->flight_size;
				xraddr.mtu = net->mtu;
				xraddr.start_time = net->start_time;
				SCTP_INP_RUNLOCK(inp);
				SCTP_INP_INFO_RUNLOCK();
				error = SYSCTL_OUT(req, &xraddr, sizeof(struct xsctp_raddr));
				if (error) {
					SCTP_INP_DECR_REF(inp);
					atomic_add_int(&stcb->asoc.refcnt, -1);
					return error;
				}
				SCTP_INP_INFO_RLOCK();
				SCTP_INP_RLOCK(inp);
			}
			atomic_add_int(&stcb->asoc.refcnt, -1);
			memset((void *)&xraddr, 0, sizeof(struct xsctp_raddr));
			xraddr.last = 1;
			SCTP_INP_RUNLOCK(inp);
			SCTP_INP_INFO_RUNLOCK();
			error = SYSCTL_OUT(req, &xraddr, sizeof(struct xsctp_raddr));
			if (error) {
				SCTP_INP_DECR_REF(inp);
				return error;
			}
			SCTP_INP_INFO_RLOCK();
			SCTP_INP_RLOCK(inp);
		}
		SCTP_INP_RUNLOCK(inp);
		SCTP_INP_INFO_RUNLOCK();
		memset((void *)&xstcb, 0, sizeof(struct xsctp_tcb));
		xstcb.last = 1;
		error = SYSCTL_OUT(req, &xstcb, sizeof(struct xsctp_tcb));
		if (error) {
			return error;
		}
		SCTP_INP_INFO_RLOCK();
		SCTP_INP_DECR_REF(inp);
	}
	SCTP_INP_INFO_RUNLOCK();

	memset((void *)&xinpcb, 0, sizeof(struct xsctp_inpcb));
	xinpcb.last = 1;
	error = SYSCTL_OUT(req, &xinpcb, sizeof(struct xsctp_inpcb));
	return error;
}


/*
 * sysctl definitions
 */

SYSCTL_INT(_net_inet_sctp, OID_AUTO, sendspace, CTLFLAG_RW,
    &sctp_sendspace, 0, "Maximum outgoing SCTP buffer size");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, recvspace, CTLFLAG_RW,
    &sctp_recvspace, 0, "Maximum incoming SCTP buffer size");

#if defined(__FreeBSD__) || defined(SCTP_APPLE_AUTO_ASCONF)
SYSCTL_INT(_net_inet_sctp, OID_AUTO, auto_asconf, CTLFLAG_RW,
    &sctp_auto_asconf, 0, "Enable SCTP Auto-ASCONF");
#endif

SYSCTL_INT(_net_inet_sctp, OID_AUTO, ecn_enable, CTLFLAG_RW,
    &sctp_ecn_enable, 0, "Enable SCTP ECN");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, ecn_nonce, CTLFLAG_RW,
    &sctp_ecn_nonce, 0, "Enable SCTP ECN Nonce");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, strict_sacks, CTLFLAG_RW,
    &sctp_strict_sacks, 0, "Enable SCTP Strict SACK checking");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, loopback_nocsum, CTLFLAG_RW,
    &sctp_no_csum_on_loopback, 0,
    "Enable NO Csum on packets sent on loopback");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, strict_init, CTLFLAG_RW,
    &sctp_strict_init, 0,
    "Enable strict INIT/INIT-ACK singleton enforcement");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, peer_chkoh, CTLFLAG_RW,
    &sctp_peer_chunk_oh, 0,
    "Amount to debit peers rwnd per chunk sent");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, maxburst, CTLFLAG_RW,
    &sctp_max_burst_default, 0,
    "Default max burst for sctp endpoints");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, maxchunks, CTLFLAG_RW,
    &sctp_max_chunks_on_queue, 0,
    "Default max chunks on queue per asoc");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, tcbhashsize, CTLFLAG_RW,
    &sctp_hashtblsize, 0,
    "Tuneable for Hash table sizes");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, min_split_point, CTLFLAG_RW,
    &sctp_min_split_point, 0,
    "Minimum size when splitting a chunk");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, pcbhashsize, CTLFLAG_RW,
    &sctp_pcbtblsize, 0,
    "Tuneable for PCB Hash table sizes");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, sys_resource, CTLFLAG_RW,
    &sctp_system_free_resc_limit, 0,
    "Max number of cached resources in the system");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, asoc_resource, CTLFLAG_RW,
    &sctp_asoc_free_resc_limit, 0,
    "Max number of cached resources in an asoc");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, chunkscale, CTLFLAG_RW,
    &sctp_chunkscale, 0,
    "Tuneable for Scaling of number of chunks and messages");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, delayed_sack_time, CTLFLAG_RW,
    &sctp_delayed_sack_time_default, 0,
    "Default delayed SACK timer in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, sack_freq, CTLFLAG_RW,
    &sctp_sack_freq_default, 0,
    "Default SACK frequency");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, heartbeat_interval, CTLFLAG_RW,
    &sctp_heartbeat_interval_default, 0,
    "Default heartbeat interval in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, pmtu_raise_time, CTLFLAG_RW,
    &sctp_pmtu_raise_time_default, 0,
    "Default PMTU raise timer in sec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, shutdown_guard_time, CTLFLAG_RW,
    &sctp_shutdown_guard_time_default, 0,
    "Default shutdown guard timer in sec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, secret_lifetime, CTLFLAG_RW,
    &sctp_secret_lifetime_default, 0,
    "Default secret lifetime in sec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, rto_max, CTLFLAG_RW,
    &sctp_rto_max_default, 0,
    "Default maximum retransmission timeout in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, rto_min, CTLFLAG_RW,
    &sctp_rto_min_default, 0,
    "Default minimum retransmission timeout in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, rto_initial, CTLFLAG_RW,
    &sctp_rto_initial_default, 0,
    "Default initial retransmission timeout in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, init_rto_max, CTLFLAG_RW,
    &sctp_init_rto_max_default, 0,
    "Default maximum retransmission timeout during association setup in msec");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, valid_cookie_life, CTLFLAG_RW,
    &sctp_valid_cookie_life_default, 0,
    "Default cookie lifetime in ticks");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, init_rtx_max, CTLFLAG_RW,
    &sctp_init_rtx_max_default, 0,
    "Default maximum number of retransmission for INIT chunks");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, assoc_rtx_max, CTLFLAG_RW,
    &sctp_assoc_rtx_max_default, 0,
    "Default maximum number of retransmissions per association");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, path_rtx_max, CTLFLAG_RW,
    &sctp_path_rtx_max_default, 0,
    "Default maximum of retransmissions per path");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, add_more_on_output, CTLFLAG_RW,
    &sctp_add_more_threshold, 0,
    "When space wise is it worthwhile to try to add more to a socket send buffer");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, outgoing_streams, CTLFLAG_RW,
    &sctp_nr_outgoing_streams_default, 0,
    "Default number of outgoing streams");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, cmt_on_off, CTLFLAG_RW,
    &sctp_cmt_on_off, 0,
    "CMT ON/OFF flag");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, cwnd_maxburst, CTLFLAG_RW,
    &sctp_use_cwnd_based_maxburst, 0,
    "Use a CWND adjusting maxburst");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, early_fast_retran, CTLFLAG_RW,
    &sctp_early_fr, 0,
    "Early Fast Retransmit with timer");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, deadlock_detect, CTLFLAG_RW,
    &sctp_says_check_for_deadlock, 0,
    "SMP Deadlock detection on/off");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, early_fast_retran_msec, CTLFLAG_RW,
    &sctp_early_fr_msec, 0,
    "Early Fast Retransmit minimum timer value");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, asconf_auth_nochk, CTLFLAG_RW,
    &sctp_asconf_auth_nochk, 0,
    "Disable SCTP ASCONF AUTH requirement");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, auth_disable, CTLFLAG_RW,
    &sctp_auth_disable, 0,
    "Disable SCTP AUTH function");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, nat_friendly, CTLFLAG_RW,
    &sctp_nat_friendly, 0,
    "SCTP NAT friendly operation");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, abc_l_var, CTLFLAG_RW,
    &sctp_L2_abc_variable, 0,
    "SCTP ABC max increase per SACK (L)");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, max_chained_mbufs, CTLFLAG_RW,
    &sctp_mbuf_threshold_count, 0,
    "Default max number of small mbufs on a chain");

SYSCTL_UINT(_net_inet_sctp, OID_AUTO, cmt_use_dac, CTLFLAG_RW,
    &sctp_cmt_use_dac, 0,
    "CMT DAC ON/OFF flag");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, do_sctp_drain, CTLFLAG_RW,
    &sctp_do_drain, 0,
    "Should SCTP respond to the drain calls");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, hb_max_burst, CTLFLAG_RW,
    &sctp_hb_maxburst, 0,
    "Confirmation Heartbeat max burst?");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, abort_at_limit, CTLFLAG_RW,
    &sctp_abort_if_one_2_one_hits_limit, 0,
    "When one-2-one hits qlimit abort");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, strict_data_order, CTLFLAG_RW,
    &sctp_strict_data_order, 0,
    "Enforce strict data ordering, abort if control inside data");

SYSCTL_STRUCT(_net_inet_sctp, OID_AUTO, stats, CTLFLAG_RW,
    &sctpstat, sctpstat,
    "SCTP statistics (struct sctps_stat, netinet/sctp.h");

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, assoclist, CTLFLAG_RD,
    0, 0, sctp_assoclist,
    "S,xassoc", "List of active SCTP associations");

SYSCTL_INT(_net_inet_sctp, OID_AUTO, min_residual, CTLFLAG_RW,
    &sctp_min_residual, 0,
    SCTPCTL_MIN_RESIDUAL_DESC);

SYSCTL_INT(_net_inet_sctp, OID_AUTO, max_retran_chunk, CTLFLAG_RW,
    &sctp_max_retran_chunk, 0,
    SCTPCTL_MAX_RETRAN_CHUNK_DESC);

#ifdef SCTP_DEBUG
SYSCTL_INT(_net_inet_sctp, OID_AUTO, debug, CTLFLAG_RW,
    &sctp_debug_on, 0, "Configure debug output");
#endif				/* SCTP_DEBUG */
