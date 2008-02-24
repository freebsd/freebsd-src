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
__FBSDID("$FreeBSD: src/sys/netinet/sctp_sysctl.c,v 1.16 2007/09/13 14:43:54 rrs Exp $");

#include <netinet/sctp_os.h>
#include <netinet/sctp_constants.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
/*
 * sysctl tunable variables
 */
uint32_t sctp_sendspace = SCTPCTL_MAXDGRAM_DEFAULT;
uint32_t sctp_recvspace = SCTPCTL_RECVSPACE_DEFAULT;
uint32_t sctp_auto_asconf = SCTPCTL_AUTOASCONF_DEFAULT;
uint32_t sctp_ecn_enable = SCTPCTL_ECN_ENABLE_DEFAULT;
uint32_t sctp_ecn_nonce = SCTPCTL_ECN_NONCE_DEFAULT;
uint32_t sctp_strict_sacks = SCTPCTL_STRICT_SACKS_DEFAULT;
uint32_t sctp_no_csum_on_loopback = SCTPCTL_LOOPBACK_NOCSUM_DEFAULT;
uint32_t sctp_strict_init = SCTPCTL_STRICT_INIT_DEFAULT;
uint32_t sctp_peer_chunk_oh = SCTPCTL_PEER_CHKOH_DEFAULT;
uint32_t sctp_max_burst_default = SCTPCTL_MAXBURST_DEFAULT;
uint32_t sctp_max_chunks_on_queue = SCTPCTL_MAXCHUNKS_DEFAULT;
uint32_t sctp_hashtblsize = SCTPCTL_TCBHASHSIZE_DEFAULT;
uint32_t sctp_pcbtblsize = SCTPCTL_PCBHASHSIZE_DEFAULT;
uint32_t sctp_min_split_point = SCTPCTL_MIN_SPLIT_POINT_DEFAULT;
uint32_t sctp_chunkscale = SCTPCTL_CHUNKSCALE_DEFAULT;
uint32_t sctp_delayed_sack_time_default = SCTPCTL_DELAYED_SACK_TIME_DEFAULT;
uint32_t sctp_sack_freq_default = SCTPCTL_SACK_FREQ_DEFAULT;
uint32_t sctp_system_free_resc_limit = SCTPCTL_SYS_RESOURCE_DEFAULT;
uint32_t sctp_asoc_free_resc_limit = SCTPCTL_ASOC_RESOURCE_DEFAULT;
uint32_t sctp_heartbeat_interval_default = SCTPCTL_HEARTBEAT_INTERVAL_DEFAULT;
uint32_t sctp_pmtu_raise_time_default = SCTPCTL_PMTU_RAISE_TIME_DEFAULT;
uint32_t sctp_shutdown_guard_time_default = SCTPCTL_SHUTDOWN_GUARD_TIME_DEFAULT;
uint32_t sctp_secret_lifetime_default = SCTPCTL_SECRET_LIFETIME_DEFAULT;
uint32_t sctp_rto_max_default = SCTPCTL_RTO_MAX_DEFAULT;
uint32_t sctp_rto_min_default = SCTPCTL_RTO_MIN_DEFAULT;
uint32_t sctp_rto_initial_default = SCTPCTL_RTO_INITIAL_DEFAULT;
uint32_t sctp_init_rto_max_default = SCTPCTL_INIT_RTO_MAX_DEFAULT;
uint32_t sctp_valid_cookie_life_default = SCTPCTL_VALID_COOKIE_LIFE_DEFAULT;
uint32_t sctp_init_rtx_max_default = SCTPCTL_INIT_RTX_MAX_DEFAULT;
uint32_t sctp_assoc_rtx_max_default = SCTPCTL_ASSOC_RTX_MAX_DEFAULT;
uint32_t sctp_path_rtx_max_default = SCTPCTL_PATH_RTX_MAX_DEFAULT;
uint32_t sctp_add_more_threshold = SCTPCTL_ADD_MORE_ON_OUTPUT_DEFAULT;
uint32_t sctp_nr_outgoing_streams_default = SCTPCTL_OUTGOING_STREAMS_DEFAULT;
uint32_t sctp_cmt_on_off = SCTPCTL_CMT_ON_OFF_DEFAULT;
uint32_t sctp_cmt_use_dac = SCTPCTL_CMT_USE_DAC_DEFAULT;
uint32_t sctp_cmt_pf = SCTPCTL_CMT_PF_DEFAULT;
uint32_t sctp_use_cwnd_based_maxburst = SCTPCTL_CWND_MAXBURST_DEFAULT;
uint32_t sctp_early_fr = SCTPCTL_EARLY_FAST_RETRAN_DEFAULT;
uint32_t sctp_early_fr_msec = SCTPCTL_EARLY_FAST_RETRAN_MSEC_DEFAULT;
uint32_t sctp_asconf_auth_nochk = SCTPCTL_ASCONF_AUTH_NOCHK_DEFAULT;
uint32_t sctp_auth_disable = SCTPCTL_AUTH_DISABLE_DEFAULT;
uint32_t sctp_nat_friendly = SCTPCTL_NAT_FRIENDLY_DEFAULT;
uint32_t sctp_L2_abc_variable = SCTPCTL_ABC_L_VAR_DEFAULT;
uint32_t sctp_mbuf_threshold_count = SCTPCTL_MAX_CHAINED_MBUFS_DEFAULT;
uint32_t sctp_do_drain = SCTPCTL_DO_SCTP_DRAIN_DEFAULT;
uint32_t sctp_hb_maxburst = SCTPCTL_HB_MAX_BURST_DEFAULT;
uint32_t sctp_abort_if_one_2_one_hits_limit = SCTPCTL_ABORT_AT_LIMIT_DEFAULT;
uint32_t sctp_strict_data_order = SCTPCTL_STRICT_DATA_ORDER_DEFAULT;
uint32_t sctp_min_residual = SCTPCTL_MIN_RESIDUAL_DEFAULT;
uint32_t sctp_max_retran_chunk = SCTPCTL_MAX_RETRAN_CHUNK_DEFAULT;
uint32_t sctp_logging_level = SCTPCTL_LOGGING_LEVEL_DEFAULT;

/* JRS - Variable for default congestion control module */
uint32_t sctp_default_cc_module = SCTPCTL_DEFAULT_CC_MODULE_DEFAULT;
uint32_t sctp_default_frag_interleave = SCTPCTL_DEFAULT_FRAG_INTERLEAVE_DEFAULT;
uint32_t sctp_mobility_base = SCTPCTL_MOBILITY_BASE_DEFAULT;
uint32_t sctp_mobility_fasthandoff = SCTPCTL_MOBILITY_FASTHANDOFF_DEFAULT;

#if defined(SCTP_LOCAL_TRACE_BUF)
struct sctp_log sctp_log;

#endif
#ifdef SCTP_DEBUG
uint32_t sctp_debug_on = SCTPCTL_DEBUG_DEFAULT;

#endif
struct sctpstat sctpstat;


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
		SCTP_INP_RUNLOCK(inp);
		SCTP_INP_INFO_RUNLOCK();
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
				memset((void *)&xladdr, 0, sizeof(struct xsctp_laddr));
				memcpy((void *)&xladdr.address, (const void *)&sctp_ifa->address, sizeof(union sctp_sockstore));
				SCTP_INP_RUNLOCK(inp);
				SCTP_INP_INFO_RUNLOCK();
				error = SYSCTL_OUT(req, &xladdr, sizeof(struct xsctp_laddr));
				if (error) {
					return (error);
				} else {
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
			memset((void *)&xladdr, 0, sizeof(struct xsctp_laddr));
			memcpy((void *)&xladdr.address, (const void *)&laddr->ifa->address, sizeof(union sctp_sockstore));
			xladdr.start_time.tv_sec = (uint32_t) laddr->start_time.tv_sec;
			xladdr.start_time.tv_usec = (uint32_t) laddr->start_time.tv_usec;
			SCTP_INP_RUNLOCK(inp);
			SCTP_INP_INFO_RUNLOCK();
			error = SYSCTL_OUT(req, &xladdr, sizeof(struct xsctp_laddr));
			if (error) {
				return (error);
			} else {
				SCTP_INP_INFO_RLOCK();
				SCTP_INP_RLOCK(inp);
			}
		}
	}
	memset((void *)&xladdr, 0, sizeof(struct xsctp_laddr));
	xladdr.last = 1;
	SCTP_INP_RUNLOCK(inp);
	SCTP_INP_INFO_RUNLOCK();
	error = SYSCTL_OUT(req, &xladdr, sizeof(struct xsctp_laddr));

	if (error) {
		return (error);
	} else {
		SCTP_INP_INFO_RLOCK();
		SCTP_INP_RLOCK(inp);
		return (0);
	}
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
		SCTP_LTRACE_ERR_RET(NULL, NULL, NULL, SCTP_FROM_SCTP_SYSCTL, EPERM);
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
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
		    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
			xinpcb.qlen = 0;
			xinpcb.maxqlen = 0;
		} else {
			xinpcb.qlen = inp->sctp_socket->so_qlen;
			xinpcb.maxqlen = inp->sctp_socket->so_qlimit;
		}
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
			xstcb.start_time.tv_sec = (uint32_t) stcb->asoc.start_time.tv_sec;
			xstcb.start_time.tv_usec = (uint32_t) stcb->asoc.start_time.tv_usec;
			xstcb.discontinuity_time.tv_sec = (uint32_t) stcb->asoc.discontinuity_time.tv_sec;
			xstcb.discontinuity_time.tv_usec = (uint32_t) stcb->asoc.discontinuity_time.tv_usec;
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
				atomic_subtract_int(&stcb->asoc.refcnt, 1);
				return error;
			}
			SCTP_INP_INFO_RLOCK();
			SCTP_INP_RLOCK(inp);
			error = copy_out_local_addresses(inp, stcb, req);
			if (error) {
				SCTP_INP_DECR_REF(inp);
				atomic_subtract_int(&stcb->asoc.refcnt, 1);
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
				xraddr.start_time.tv_sec = (uint32_t) net->start_time.tv_sec;
				xraddr.start_time.tv_usec = (uint32_t) net->start_time.tv_usec;
				SCTP_INP_RUNLOCK(inp);
				SCTP_INP_INFO_RUNLOCK();
				error = SYSCTL_OUT(req, &xraddr, sizeof(struct xsctp_raddr));
				if (error) {
					SCTP_INP_DECR_REF(inp);
					atomic_subtract_int(&stcb->asoc.refcnt, 1);
					return error;
				}
				SCTP_INP_INFO_RLOCK();
				SCTP_INP_RLOCK(inp);
			}
			atomic_subtract_int(&stcb->asoc.refcnt, 1);
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
		SCTP_INP_DECR_REF(inp);
		SCTP_INP_RUNLOCK(inp);
		SCTP_INP_INFO_RUNLOCK();
		memset((void *)&xstcb, 0, sizeof(struct xsctp_tcb));
		xstcb.last = 1;
		error = SYSCTL_OUT(req, &xstcb, sizeof(struct xsctp_tcb));
		if (error) {
			return error;
		}
		SCTP_INP_INFO_RLOCK();
	}
	SCTP_INP_INFO_RUNLOCK();

	memset((void *)&xinpcb, 0, sizeof(struct xsctp_inpcb));
	xinpcb.last = 1;
	error = SYSCTL_OUT(req, &xinpcb, sizeof(struct xsctp_inpcb));
	return error;
}

#define RANGECHK(var, min, max) \
	if ((var) < (min)) { (var) = (min); } \
	else if ((var) > (max)) { (var) = (max); }

static int
sysctl_sctp_check(SYSCTL_HANDLER_ARGS)
{
	int error;

	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (error == 0) {
		RANGECHK(sctp_sendspace, SCTPCTL_MAXDGRAM_MIN, SCTPCTL_MAXDGRAM_MAX);
		RANGECHK(sctp_recvspace, SCTPCTL_RECVSPACE_MIN, SCTPCTL_RECVSPACE_MAX);
#if defined(__FreeBSD__) || defined(SCTP_APPLE_AUTO_ASCONF)
		RANGECHK(sctp_auto_asconf, SCTPCTL_AUTOASCONF_MIN, SCTPCTL_AUTOASCONF_MAX);
#endif
		RANGECHK(sctp_ecn_enable, SCTPCTL_ECN_ENABLE_MIN, SCTPCTL_ECN_ENABLE_MAX);
		RANGECHK(sctp_ecn_nonce, SCTPCTL_ECN_NONCE_MIN, SCTPCTL_ECN_NONCE_MAX);
		RANGECHK(sctp_strict_sacks, SCTPCTL_STRICT_SACKS_MIN, SCTPCTL_STRICT_SACKS_MAX);
		RANGECHK(sctp_no_csum_on_loopback, SCTPCTL_LOOPBACK_NOCSUM_MIN, SCTPCTL_LOOPBACK_NOCSUM_MAX);
		RANGECHK(sctp_strict_init, SCTPCTL_STRICT_INIT_MIN, SCTPCTL_STRICT_INIT_MAX);
		RANGECHK(sctp_peer_chunk_oh, SCTPCTL_PEER_CHKOH_MIN, SCTPCTL_PEER_CHKOH_MAX);
		RANGECHK(sctp_max_burst_default, SCTPCTL_MAXBURST_MIN, SCTPCTL_MAXBURST_MAX);
		RANGECHK(sctp_max_chunks_on_queue, SCTPCTL_MAXCHUNKS_MIN, SCTPCTL_MAXCHUNKS_MAX);
		RANGECHK(sctp_hashtblsize, SCTPCTL_TCBHASHSIZE_MIN, SCTPCTL_TCBHASHSIZE_MAX);
		RANGECHK(sctp_pcbtblsize, SCTPCTL_PCBHASHSIZE_MIN, SCTPCTL_PCBHASHSIZE_MAX);
		RANGECHK(sctp_min_split_point, SCTPCTL_MIN_SPLIT_POINT_MIN, SCTPCTL_MIN_SPLIT_POINT_MAX);
		RANGECHK(sctp_chunkscale, SCTPCTL_CHUNKSCALE_MIN, SCTPCTL_CHUNKSCALE_MAX);
		RANGECHK(sctp_delayed_sack_time_default, SCTPCTL_DELAYED_SACK_TIME_MIN, SCTPCTL_DELAYED_SACK_TIME_MAX);
		RANGECHK(sctp_sack_freq_default, SCTPCTL_SACK_FREQ_MIN, SCTPCTL_SACK_FREQ_MAX);
		RANGECHK(sctp_system_free_resc_limit, SCTPCTL_SYS_RESOURCE_MIN, SCTPCTL_SYS_RESOURCE_MAX);
		RANGECHK(sctp_asoc_free_resc_limit, SCTPCTL_ASOC_RESOURCE_MIN, SCTPCTL_ASOC_RESOURCE_MAX);
		RANGECHK(sctp_heartbeat_interval_default, SCTPCTL_HEARTBEAT_INTERVAL_MIN, SCTPCTL_HEARTBEAT_INTERVAL_MAX);
		RANGECHK(sctp_pmtu_raise_time_default, SCTPCTL_PMTU_RAISE_TIME_MIN, SCTPCTL_PMTU_RAISE_TIME_MAX);
		RANGECHK(sctp_shutdown_guard_time_default, SCTPCTL_SHUTDOWN_GUARD_TIME_MIN, SCTPCTL_SHUTDOWN_GUARD_TIME_MAX);
		RANGECHK(sctp_secret_lifetime_default, SCTPCTL_SECRET_LIFETIME_MIN, SCTPCTL_SECRET_LIFETIME_MAX);
		RANGECHK(sctp_rto_max_default, SCTPCTL_RTO_MAX_MIN, SCTPCTL_RTO_MAX_MAX);
		RANGECHK(sctp_rto_min_default, SCTPCTL_RTO_MIN_MIN, SCTPCTL_RTO_MIN_MAX);
		RANGECHK(sctp_rto_initial_default, SCTPCTL_RTO_INITIAL_MIN, SCTPCTL_RTO_INITIAL_MAX);
		RANGECHK(sctp_init_rto_max_default, SCTPCTL_INIT_RTO_MAX_MIN, SCTPCTL_INIT_RTO_MAX_MAX);
		RANGECHK(sctp_valid_cookie_life_default, SCTPCTL_VALID_COOKIE_LIFE_MIN, SCTPCTL_VALID_COOKIE_LIFE_MAX);
		RANGECHK(sctp_init_rtx_max_default, SCTPCTL_INIT_RTX_MAX_MIN, SCTPCTL_INIT_RTX_MAX_MAX);
		RANGECHK(sctp_assoc_rtx_max_default, SCTPCTL_ASSOC_RTX_MAX_MIN, SCTPCTL_ASSOC_RTX_MAX_MAX);
		RANGECHK(sctp_path_rtx_max_default, SCTPCTL_PATH_RTX_MAX_MIN, SCTPCTL_PATH_RTX_MAX_MAX);
		RANGECHK(sctp_add_more_threshold, SCTPCTL_ADD_MORE_ON_OUTPUT_MIN, SCTPCTL_ADD_MORE_ON_OUTPUT_MAX);
		RANGECHK(sctp_nr_outgoing_streams_default, SCTPCTL_OUTGOING_STREAMS_MIN, SCTPCTL_OUTGOING_STREAMS_MAX);
		RANGECHK(sctp_cmt_on_off, SCTPCTL_CMT_ON_OFF_MIN, SCTPCTL_CMT_ON_OFF_MAX);
		RANGECHK(sctp_cmt_use_dac, SCTPCTL_CMT_USE_DAC_MIN, SCTPCTL_CMT_USE_DAC_MAX);
		RANGECHK(sctp_cmt_pf, SCTPCTL_CMT_PF_MIN, SCTPCTL_CMT_PF_MAX);
		RANGECHK(sctp_use_cwnd_based_maxburst, SCTPCTL_CWND_MAXBURST_MIN, SCTPCTL_CWND_MAXBURST_MAX);
		RANGECHK(sctp_early_fr, SCTPCTL_EARLY_FAST_RETRAN_MIN, SCTPCTL_EARLY_FAST_RETRAN_MAX);
		RANGECHK(sctp_early_fr_msec, SCTPCTL_EARLY_FAST_RETRAN_MSEC_MIN, SCTPCTL_EARLY_FAST_RETRAN_MSEC_MAX);
		RANGECHK(sctp_asconf_auth_nochk, SCTPCTL_ASCONF_AUTH_NOCHK_MIN, SCTPCTL_ASCONF_AUTH_NOCHK_MAX);
		RANGECHK(sctp_auth_disable, SCTPCTL_AUTH_DISABLE_MIN, SCTPCTL_AUTH_DISABLE_MAX);
		RANGECHK(sctp_nat_friendly, SCTPCTL_NAT_FRIENDLY_MIN, SCTPCTL_NAT_FRIENDLY_MAX);
		RANGECHK(sctp_L2_abc_variable, SCTPCTL_ABC_L_VAR_MIN, SCTPCTL_ABC_L_VAR_MAX);
		RANGECHK(sctp_mbuf_threshold_count, SCTPCTL_MAX_CHAINED_MBUFS_MIN, SCTPCTL_MAX_CHAINED_MBUFS_MAX);
		RANGECHK(sctp_do_drain, SCTPCTL_DO_SCTP_DRAIN_MIN, SCTPCTL_DO_SCTP_DRAIN_MAX);
		RANGECHK(sctp_hb_maxburst, SCTPCTL_HB_MAX_BURST_MIN, SCTPCTL_HB_MAX_BURST_MAX);
		RANGECHK(sctp_abort_if_one_2_one_hits_limit, SCTPCTL_ABORT_AT_LIMIT_MIN, SCTPCTL_ABORT_AT_LIMIT_MAX);
		RANGECHK(sctp_strict_data_order, SCTPCTL_STRICT_DATA_ORDER_MIN, SCTPCTL_STRICT_DATA_ORDER_MAX);
		RANGECHK(sctp_min_residual, SCTPCTL_MIN_RESIDUAL_MIN, SCTPCTL_MIN_RESIDUAL_MAX);
		RANGECHK(sctp_max_retran_chunk, SCTPCTL_MAX_RETRAN_CHUNK_MIN, SCTPCTL_MAX_RETRAN_CHUNK_MAX);
		RANGECHK(sctp_logging_level, SCTPCTL_LOGGING_LEVEL_MIN, SCTPCTL_LOGGING_LEVEL_MAX);
		RANGECHK(sctp_default_cc_module, SCTPCTL_DEFAULT_CC_MODULE_MIN, SCTPCTL_DEFAULT_CC_MODULE_MAX);
		RANGECHK(sctp_default_frag_interleave, SCTPCTL_DEFAULT_FRAG_INTERLEAVE_MIN, SCTPCTL_DEFAULT_FRAG_INTERLEAVE_MAX);
#if defined(__FreeBSD__) || defined(SCTP_APPLE_MOBILITY_BASE)
		RANGECHK(sctp_mobility_base, SCTPCTL_MOBILITY_BASE_MIN, SCTPCTL_MOBILITY_BASE_MAX);
#endif
#if defined(__FreeBSD__) || defined(SCTP_APPLE_MOBILITY_FASTHANDOFF)
		RANGECHK(sctp_mobility_fasthandoff, SCTPCTL_MOBILITY_FASTHANDOFF_MIN, SCTPCTL_MOBILITY_FASTHANDOFF_MAX);
#endif
#ifdef SCTP_DEBUG
		RANGECHK(sctp_debug_on, SCTPCTL_DEBUG_MIN, SCTPCTL_DEBUG_MAX);
#endif
	}
	return (error);
}

/*
 * sysctl definitions
 */

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, sendspace, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_sendspace, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAXDGRAM_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, recvspace, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_recvspace, 0, sysctl_sctp_check, "IU",
    SCTPCTL_RECVSPACE_DESC);

#if defined(__FreeBSD__) || defined(SCTP_APPLE_AUTO_ASCONF)
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, auto_asconf, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_auto_asconf, 0, sysctl_sctp_check, "IU",
    SCTPCTL_AUTOASCONF_DESC);
#endif

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, ecn_enable, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_ecn_enable, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ECN_ENABLE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, ecn_nonce, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_ecn_nonce, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ECN_NONCE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, strict_sacks, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_strict_sacks, 0, sysctl_sctp_check, "IU",
    SCTPCTL_STRICT_SACKS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, loopback_nocsum, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_no_csum_on_loopback, 0, sysctl_sctp_check, "IU",
    SCTPCTL_LOOPBACK_NOCSUM_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, strict_init, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_strict_init, 0, sysctl_sctp_check, "IU",
    SCTPCTL_STRICT_INIT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, peer_chkoh, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_peer_chunk_oh, 0, sysctl_sctp_check, "IU",
    SCTPCTL_PEER_CHKOH_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, maxburst, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_max_burst_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAXBURST_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, maxchunks, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_max_chunks_on_queue, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAXCHUNKS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, tcbhashsize, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_hashtblsize, 0, sysctl_sctp_check, "IU",
    SCTPCTL_TCBHASHSIZE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, pcbhashsize, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_pcbtblsize, 0, sysctl_sctp_check, "IU",
    SCTPCTL_PCBHASHSIZE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, min_split_point, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_min_split_point, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MIN_SPLIT_POINT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, chunkscale, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_chunkscale, 0, sysctl_sctp_check, "IU",
    SCTPCTL_CHUNKSCALE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, delayed_sack_time, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_delayed_sack_time_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_DELAYED_SACK_TIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, sack_freq, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_sack_freq_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_SACK_FREQ_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, sys_resource, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_system_free_resc_limit, 0, sysctl_sctp_check, "IU",
    SCTPCTL_SYS_RESOURCE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, asoc_resource, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_asoc_free_resc_limit, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ASOC_RESOURCE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, heartbeat_interval, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_heartbeat_interval_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_HEARTBEAT_INTERVAL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, pmtu_raise_time, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_pmtu_raise_time_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_PMTU_RAISE_TIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, shutdown_guard_time, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_shutdown_guard_time_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_SHUTDOWN_GUARD_TIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, secret_lifetime, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_secret_lifetime_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_SECRET_LIFETIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, rto_max, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_rto_max_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_RTO_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, rto_min, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_rto_min_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_RTO_MIN_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, rto_initial, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_rto_initial_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_RTO_INITIAL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, init_rto_max, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_init_rto_max_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_INIT_RTO_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, valid_cookie_life, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_valid_cookie_life_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_VALID_COOKIE_LIFE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, init_rtx_max, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_init_rtx_max_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_INIT_RTX_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, assoc_rtx_max, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_assoc_rtx_max_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ASSOC_RTX_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, path_rtx_max, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_path_rtx_max_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_PATH_RTX_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, add_more_on_output, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_add_more_threshold, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ADD_MORE_ON_OUTPUT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, outgoing_streams, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_nr_outgoing_streams_default, 0, sysctl_sctp_check, "IU",
    SCTPCTL_OUTGOING_STREAMS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cmt_on_off, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_cmt_on_off, 0, sysctl_sctp_check, "IU",
    SCTPCTL_CMT_ON_OFF_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cmt_use_dac, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_cmt_use_dac, 0, sysctl_sctp_check, "IU",
    SCTPCTL_CMT_USE_DAC_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cmt_pf, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_cmt_pf, 0, sysctl_sctp_check, "IU",
    SCTPCTL_CMT_PF_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cwnd_maxburst, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_use_cwnd_based_maxburst, 0, sysctl_sctp_check, "IU",
    SCTPCTL_CWND_MAXBURST_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, early_fast_retran, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_early_fr, 0, sysctl_sctp_check, "IU",
    SCTPCTL_EARLY_FAST_RETRAN_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, early_fast_retran_msec, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_early_fr_msec, 0, sysctl_sctp_check, "IU",
    SCTPCTL_EARLY_FAST_RETRAN_MSEC_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, asconf_auth_nochk, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_asconf_auth_nochk, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ASCONF_AUTH_NOCHK_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, auth_disable, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_auth_disable, 0, sysctl_sctp_check, "IU",
    SCTPCTL_AUTH_DISABLE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, nat_friendly, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_nat_friendly, 0, sysctl_sctp_check, "IU",
    SCTPCTL_NAT_FRIENDLY_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, abc_l_var, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_L2_abc_variable, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ABC_L_VAR_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, max_chained_mbufs, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_mbuf_threshold_count, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAX_CHAINED_MBUFS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, do_sctp_drain, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_do_drain, 0, sysctl_sctp_check, "IU",
    SCTPCTL_DO_SCTP_DRAIN_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, hb_max_burst, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_hb_maxburst, 0, sysctl_sctp_check, "IU",
    SCTPCTL_HB_MAX_BURST_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, abort_at_limit, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_abort_if_one_2_one_hits_limit, 0, sysctl_sctp_check, "IU",
    SCTPCTL_ABORT_AT_LIMIT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, strict_data_order, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_strict_data_order, 0, sysctl_sctp_check, "IU",
    SCTPCTL_STRICT_DATA_ORDER_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, min_residual, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_min_residual, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MIN_RESIDUAL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, max_retran_chunk, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_max_retran_chunk, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAX_RETRAN_CHUNK_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, log_level, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_logging_level, 0, sysctl_sctp_check, "IU",
    SCTPCTL_LOGGING_LEVEL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, default_cc_module, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_default_cc_module, 0, sysctl_sctp_check, "IU",
    SCTPCTL_DEFAULT_CC_MODULE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, default_frag_interleave, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_default_frag_interleave, 0, sysctl_sctp_check, "IU",
    SCTPCTL_DEFAULT_FRAG_INTERLEAVE_DESC);

#if defined(__FreeBSD__) || defined(SCTP_APPLE_MOBILITY_BASE)
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, mobility_base, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_mobility_base, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MOBILITY_BASE_DESC);
#endif

#if defined(__FreeBSD__) || defined(SCTP_APPLE_MOBILITY_FASTHANDOFF)
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, mobility_fasthandoff, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_mobility_fasthandoff, 0, sysctl_sctp_check, "IU",
    SCTPCTL_MOBILITY_FASTHANDOFF_DESC);
#endif

#if defined(SCTP_LOCAL_TRACE_BUF)
SYSCTL_STRUCT(_net_inet_sctp, OID_AUTO, log, CTLFLAG_RD,
    &sctp_log, sctp_log,
    "SCTP logging (struct sctp_log)");
#endif

#ifdef SCTP_DEBUG
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, debug, CTLTYPE_INT | CTLFLAG_RW,
    &sctp_debug_on, 0, sysctl_sctp_check, "IU",
    SCTPCTL_DEBUG_DESC);
#endif				/* SCTP_DEBUG */


SYSCTL_STRUCT(_net_inet_sctp, OID_AUTO, stats, CTLFLAG_RW,
    &sctpstat, sctpstat,
    "SCTP statistics (struct sctp_stat)");

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, assoclist, CTLFLAG_RD,
    0, 0, sctp_assoclist,
    "S,xassoc", "List of active SCTP associations");
