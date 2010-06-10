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
#include <netinet/sctp.h>
#include <netinet/sctp_constants.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <sys/smp.h>

/*
 * sysctl tunable variables
 */

void
sctp_init_sysctls()
{
	SCTP_BASE_SYSCTL(sctp_sendspace) = SCTPCTL_MAXDGRAM_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_recvspace) = SCTPCTL_RECVSPACE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_auto_asconf) = SCTPCTL_AUTOASCONF_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_multiple_asconfs) = SCTPCTL_MULTIPLEASCONFS_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_ecn_enable) = SCTPCTL_ECN_ENABLE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_ecn_nonce) = SCTPCTL_ECN_NONCE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_strict_sacks) = SCTPCTL_STRICT_SACKS_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_no_csum_on_loopback) = SCTPCTL_LOOPBACK_NOCSUM_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_strict_init) = SCTPCTL_STRICT_INIT_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_peer_chunk_oh) = SCTPCTL_PEER_CHKOH_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_max_burst_default) = SCTPCTL_MAXBURST_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue) = SCTPCTL_MAXCHUNKS_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_hashtblsize) = SCTPCTL_TCBHASHSIZE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_pcbtblsize) = SCTPCTL_PCBHASHSIZE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_min_split_point) = SCTPCTL_MIN_SPLIT_POINT_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_chunkscale) = SCTPCTL_CHUNKSCALE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_delayed_sack_time_default) = SCTPCTL_DELAYED_SACK_TIME_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_sack_freq_default) = SCTPCTL_SACK_FREQ_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_system_free_resc_limit) = SCTPCTL_SYS_RESOURCE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_asoc_free_resc_limit) = SCTPCTL_ASOC_RESOURCE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_heartbeat_interval_default) = SCTPCTL_HEARTBEAT_INTERVAL_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_pmtu_raise_time_default) = SCTPCTL_PMTU_RAISE_TIME_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_shutdown_guard_time_default) = SCTPCTL_SHUTDOWN_GUARD_TIME_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_secret_lifetime_default) = SCTPCTL_SECRET_LIFETIME_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_rto_max_default) = SCTPCTL_RTO_MAX_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_rto_min_default) = SCTPCTL_RTO_MIN_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_rto_initial_default) = SCTPCTL_RTO_INITIAL_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_init_rto_max_default) = SCTPCTL_INIT_RTO_MAX_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_valid_cookie_life_default) = SCTPCTL_VALID_COOKIE_LIFE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_init_rtx_max_default) = SCTPCTL_INIT_RTX_MAX_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_assoc_rtx_max_default) = SCTPCTL_ASSOC_RTX_MAX_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_path_rtx_max_default) = SCTPCTL_PATH_RTX_MAX_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_add_more_threshold) = SCTPCTL_ADD_MORE_ON_OUTPUT_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_nr_outgoing_streams_default) = SCTPCTL_OUTGOING_STREAMS_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_cmt_on_off) = SCTPCTL_CMT_ON_OFF_DEFAULT;
	/* EY */
	SCTP_BASE_SYSCTL(sctp_nr_sack_on_off) = SCTPCTL_NR_SACK_ON_OFF_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_cmt_use_dac) = SCTPCTL_CMT_USE_DAC_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_cmt_pf) = SCTPCTL_CMT_PF_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_use_cwnd_based_maxburst) = SCTPCTL_CWND_MAXBURST_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_early_fr) = SCTPCTL_EARLY_FAST_RETRAN_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_early_fr_msec) = SCTPCTL_EARLY_FAST_RETRAN_MSEC_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_asconf_auth_nochk) = SCTPCTL_ASCONF_AUTH_NOCHK_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_auth_disable) = SCTPCTL_AUTH_DISABLE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_nat_friendly) = SCTPCTL_NAT_FRIENDLY_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_L2_abc_variable) = SCTPCTL_ABC_L_VAR_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_mbuf_threshold_count) = SCTPCTL_MAX_CHAINED_MBUFS_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_do_drain) = SCTPCTL_DO_SCTP_DRAIN_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_hb_maxburst) = SCTPCTL_HB_MAX_BURST_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_abort_if_one_2_one_hits_limit) = SCTPCTL_ABORT_AT_LIMIT_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_strict_data_order) = SCTPCTL_STRICT_DATA_ORDER_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_min_residual) = SCTPCTL_MIN_RESIDUAL_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_max_retran_chunk) = SCTPCTL_MAX_RETRAN_CHUNK_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_logging_level) = SCTPCTL_LOGGING_LEVEL_DEFAULT;
	/* JRS - Variable for default congestion control module */
	SCTP_BASE_SYSCTL(sctp_default_cc_module) = SCTPCTL_DEFAULT_CC_MODULE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_default_frag_interleave) = SCTPCTL_DEFAULT_FRAG_INTERLEAVE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_mobility_base) = SCTPCTL_MOBILITY_BASE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_mobility_fasthandoff) = SCTPCTL_MOBILITY_FASTHANDOFF_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_vtag_time_wait) = SCTPCTL_TIME_WAIT_DEFAULT;
#if defined(SCTP_LOCAL_TRACE_BUF)
	memset(&SCTP_BASE_SYSCTL(sctp_log), 0, sizeof(struct sctp_log));
#endif
	SCTP_BASE_SYSCTL(sctp_udp_tunneling_for_client_enable) = SCTPCTL_UDP_TUNNELING_FOR_CLIENT_ENABLE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_udp_tunneling_port) = SCTPCTL_UDP_TUNNELING_PORT_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_enable_sack_immediately) = SCTPCTL_SACK_IMMEDIATELY_ENABLE_DEFAULT;
	SCTP_BASE_SYSCTL(sctp_inits_include_nat_friendly) = SCTPCTL_NAT_FRIENDLY_INITS_DEFAULT;
#if defined(SCTP_DEBUG)
	SCTP_BASE_SYSCTL(sctp_debug_on) = SCTPCTL_DEBUG_DEFAULT;
#endif
#if defined(__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
	SCTP_BASE_SYSCTL(sctp_output_unlocked) = SCTPCTL_OUTPUT_UNLOCKED_DEFAULT;
#endif
}


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
				switch (sctp_ifa->address.sa.sa_family) {
				case AF_INET:
					if (ipv4_addr_legal) {
						struct sockaddr_in *sin;

						sin = (struct sockaddr_in *)&sctp_ifa->address.sa;
						if (sin->sin_addr.s_addr == 0)
							continue;
						if ((ipv4_local_scope == 0) && (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)))
							continue;
					} else {
						continue;
					}
					break;
#ifdef INET6
				case AF_INET6:
					if (ipv6_addr_legal) {
						struct sockaddr_in6 *sin6;

						sin6 = (struct sockaddr_in6 *)&sctp_ifa->address.sa;
						if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
							continue;
						if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
							if (local_scope == 0)
								continue;
							if (sin6->sin6_scope_id == 0) {
								/*
								 * bad link
								 * local
								 * address
								 */
								if (sa6_recoverscope(sin6) != 0)
									continue;
							}
						}
						if ((site_scope == 0) && (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)))
							continue;
					} else {
						continue;
					}
					break;
#endif
				default:
					continue;
				}
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
	struct socket *so;

	number_of_endpoints = 0;
	number_of_local_addresses = 0;
	number_of_associations = 0;
	number_of_remote_addresses = 0;

	SCTP_INP_INFO_RLOCK();
	if (req->oldptr == USER_ADDR_NULL) {
		LIST_FOREACH(inp, &SCTP_BASE_INFO(listhead), sctp_list) {
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
	LIST_FOREACH(inp, &SCTP_BASE_INFO(listhead), sctp_list) {
		SCTP_INP_RLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			/* if its allgone it is being freed - skip it  */
			goto skip;
		}
		xinpcb.last = 0;
		xinpcb.local_port = ntohs(inp->sctp_lport);
		xinpcb.flags = inp->sctp_flags;
		xinpcb.features = inp->sctp_features;
		xinpcb.total_sends = inp->total_sends;
		xinpcb.total_recvs = inp->total_recvs;
		xinpcb.total_nospaces = inp->total_nospaces;
		xinpcb.fragmentation_point = inp->sctp_frag_point;
		so = inp->sctp_socket;
		if ((so == NULL) ||
		    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE)) {
			xinpcb.qlen = 0;
			xinpcb.maxqlen = 0;
		} else {
			xinpcb.qlen = so->so_qlen;
			xinpcb.maxqlen = so->so_qlimit;
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
			/* 7.0 does not support these */
			xstcb.assoc_id = sctp_get_associd(stcb);
			xstcb.peers_rwnd = stcb->asoc.peers_rwnd;
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
				xraddr.rtt = net->rtt;
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
skip:
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
sysctl_sctp_udp_tunneling_check(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t old_sctp_udp_tunneling_port;

	SCTP_INP_INFO_RLOCK();
	old_sctp_udp_tunneling_port = SCTP_BASE_SYSCTL(sctp_udp_tunneling_port);
	SCTP_INP_INFO_RUNLOCK();
	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (error == 0) {
		RANGECHK(SCTP_BASE_SYSCTL(sctp_udp_tunneling_port), SCTPCTL_UDP_TUNNELING_PORT_MIN, SCTPCTL_UDP_TUNNELING_PORT_MAX);
		if (old_sctp_udp_tunneling_port == SCTP_BASE_SYSCTL(sctp_udp_tunneling_port)) {
			error = 0;
			goto out;
		}
		SCTP_INP_INFO_WLOCK();
		if (old_sctp_udp_tunneling_port) {
			sctp_over_udp_stop();
		}
		if (SCTP_BASE_SYSCTL(sctp_udp_tunneling_port)) {
			if (sctp_over_udp_start()) {
				SCTP_BASE_SYSCTL(sctp_udp_tunneling_port) = 0;
			}
		}
		SCTP_INP_INFO_WUNLOCK();
	}
out:
	return (error);
}


static int
sysctl_sctp_check(SYSCTL_HANDLER_ARGS)
{
	int error;

	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (error == 0) {
		RANGECHK(SCTP_BASE_SYSCTL(sctp_sendspace), SCTPCTL_MAXDGRAM_MIN, SCTPCTL_MAXDGRAM_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_recvspace), SCTPCTL_RECVSPACE_MIN, SCTPCTL_RECVSPACE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_auto_asconf), SCTPCTL_AUTOASCONF_MIN, SCTPCTL_AUTOASCONF_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_ecn_enable), SCTPCTL_ECN_ENABLE_MIN, SCTPCTL_ECN_ENABLE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_ecn_nonce), SCTPCTL_ECN_NONCE_MIN, SCTPCTL_ECN_NONCE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_strict_sacks), SCTPCTL_STRICT_SACKS_MIN, SCTPCTL_STRICT_SACKS_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_no_csum_on_loopback), SCTPCTL_LOOPBACK_NOCSUM_MIN, SCTPCTL_LOOPBACK_NOCSUM_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_strict_init), SCTPCTL_STRICT_INIT_MIN, SCTPCTL_STRICT_INIT_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_peer_chunk_oh), SCTPCTL_PEER_CHKOH_MIN, SCTPCTL_PEER_CHKOH_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_max_burst_default), SCTPCTL_MAXBURST_MIN, SCTPCTL_MAXBURST_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue), SCTPCTL_MAXCHUNKS_MIN, SCTPCTL_MAXCHUNKS_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_hashtblsize), SCTPCTL_TCBHASHSIZE_MIN, SCTPCTL_TCBHASHSIZE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_pcbtblsize), SCTPCTL_PCBHASHSIZE_MIN, SCTPCTL_PCBHASHSIZE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_min_split_point), SCTPCTL_MIN_SPLIT_POINT_MIN, SCTPCTL_MIN_SPLIT_POINT_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_chunkscale), SCTPCTL_CHUNKSCALE_MIN, SCTPCTL_CHUNKSCALE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_delayed_sack_time_default), SCTPCTL_DELAYED_SACK_TIME_MIN, SCTPCTL_DELAYED_SACK_TIME_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_sack_freq_default), SCTPCTL_SACK_FREQ_MIN, SCTPCTL_SACK_FREQ_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_system_free_resc_limit), SCTPCTL_SYS_RESOURCE_MIN, SCTPCTL_SYS_RESOURCE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_asoc_free_resc_limit), SCTPCTL_ASOC_RESOURCE_MIN, SCTPCTL_ASOC_RESOURCE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_heartbeat_interval_default), SCTPCTL_HEARTBEAT_INTERVAL_MIN, SCTPCTL_HEARTBEAT_INTERVAL_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_pmtu_raise_time_default), SCTPCTL_PMTU_RAISE_TIME_MIN, SCTPCTL_PMTU_RAISE_TIME_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_shutdown_guard_time_default), SCTPCTL_SHUTDOWN_GUARD_TIME_MIN, SCTPCTL_SHUTDOWN_GUARD_TIME_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_secret_lifetime_default), SCTPCTL_SECRET_LIFETIME_MIN, SCTPCTL_SECRET_LIFETIME_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_rto_max_default), SCTPCTL_RTO_MAX_MIN, SCTPCTL_RTO_MAX_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_rto_min_default), SCTPCTL_RTO_MIN_MIN, SCTPCTL_RTO_MIN_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_rto_initial_default), SCTPCTL_RTO_INITIAL_MIN, SCTPCTL_RTO_INITIAL_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_init_rto_max_default), SCTPCTL_INIT_RTO_MAX_MIN, SCTPCTL_INIT_RTO_MAX_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_valid_cookie_life_default), SCTPCTL_VALID_COOKIE_LIFE_MIN, SCTPCTL_VALID_COOKIE_LIFE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_init_rtx_max_default), SCTPCTL_INIT_RTX_MAX_MIN, SCTPCTL_INIT_RTX_MAX_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_assoc_rtx_max_default), SCTPCTL_ASSOC_RTX_MAX_MIN, SCTPCTL_ASSOC_RTX_MAX_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_path_rtx_max_default), SCTPCTL_PATH_RTX_MAX_MIN, SCTPCTL_PATH_RTX_MAX_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_add_more_threshold), SCTPCTL_ADD_MORE_ON_OUTPUT_MIN, SCTPCTL_ADD_MORE_ON_OUTPUT_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_nr_outgoing_streams_default), SCTPCTL_OUTGOING_STREAMS_MIN, SCTPCTL_OUTGOING_STREAMS_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_cmt_on_off), SCTPCTL_CMT_ON_OFF_MIN, SCTPCTL_CMT_ON_OFF_MAX);
		/* EY */
		RANGECHK(SCTP_BASE_SYSCTL(sctp_nr_sack_on_off), SCTPCTL_NR_SACK_ON_OFF_MIN, SCTPCTL_NR_SACK_ON_OFF_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_cmt_use_dac), SCTPCTL_CMT_USE_DAC_MIN, SCTPCTL_CMT_USE_DAC_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_cmt_pf), SCTPCTL_CMT_PF_MIN, SCTPCTL_CMT_PF_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_use_cwnd_based_maxburst), SCTPCTL_CWND_MAXBURST_MIN, SCTPCTL_CWND_MAXBURST_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_early_fr), SCTPCTL_EARLY_FAST_RETRAN_MIN, SCTPCTL_EARLY_FAST_RETRAN_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_early_fr_msec), SCTPCTL_EARLY_FAST_RETRAN_MSEC_MIN, SCTPCTL_EARLY_FAST_RETRAN_MSEC_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_asconf_auth_nochk), SCTPCTL_ASCONF_AUTH_NOCHK_MIN, SCTPCTL_ASCONF_AUTH_NOCHK_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_auth_disable), SCTPCTL_AUTH_DISABLE_MIN, SCTPCTL_AUTH_DISABLE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_nat_friendly), SCTPCTL_NAT_FRIENDLY_MIN, SCTPCTL_NAT_FRIENDLY_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_L2_abc_variable), SCTPCTL_ABC_L_VAR_MIN, SCTPCTL_ABC_L_VAR_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_mbuf_threshold_count), SCTPCTL_MAX_CHAINED_MBUFS_MIN, SCTPCTL_MAX_CHAINED_MBUFS_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_do_drain), SCTPCTL_DO_SCTP_DRAIN_MIN, SCTPCTL_DO_SCTP_DRAIN_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_hb_maxburst), SCTPCTL_HB_MAX_BURST_MIN, SCTPCTL_HB_MAX_BURST_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_abort_if_one_2_one_hits_limit), SCTPCTL_ABORT_AT_LIMIT_MIN, SCTPCTL_ABORT_AT_LIMIT_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_strict_data_order), SCTPCTL_STRICT_DATA_ORDER_MIN, SCTPCTL_STRICT_DATA_ORDER_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_min_residual), SCTPCTL_MIN_RESIDUAL_MIN, SCTPCTL_MIN_RESIDUAL_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_max_retran_chunk), SCTPCTL_MAX_RETRAN_CHUNK_MIN, SCTPCTL_MAX_RETRAN_CHUNK_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_logging_level), SCTPCTL_LOGGING_LEVEL_MIN, SCTPCTL_LOGGING_LEVEL_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_default_cc_module), SCTPCTL_DEFAULT_CC_MODULE_MIN, SCTPCTL_DEFAULT_CC_MODULE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_default_frag_interleave), SCTPCTL_DEFAULT_FRAG_INTERLEAVE_MIN, SCTPCTL_DEFAULT_FRAG_INTERLEAVE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_vtag_time_wait), SCTPCTL_TIME_WAIT_MIN, SCTPCTL_TIME_WAIT_MAX);

		RANGECHK(SCTP_BASE_SYSCTL(sctp_mobility_base), SCTPCTL_MOBILITY_BASE_MIN, SCTPCTL_MOBILITY_BASE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_mobility_fasthandoff), SCTPCTL_MOBILITY_FASTHANDOFF_MIN, SCTPCTL_MOBILITY_FASTHANDOFF_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_udp_tunneling_for_client_enable), SCTPCTL_UDP_TUNNELING_FOR_CLIENT_ENABLE_MIN, SCTPCTL_UDP_TUNNELING_FOR_CLIENT_ENABLE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_enable_sack_immediately), SCTPCTL_SACK_IMMEDIATELY_ENABLE_MIN, SCTPCTL_SACK_IMMEDIATELY_ENABLE_MAX);
		RANGECHK(SCTP_BASE_SYSCTL(sctp_inits_include_nat_friendly), SCTPCTL_NAT_FRIENDLY_INITS_MIN, SCTPCTL_NAT_FRIENDLY_INITS_MAX);

#ifdef SCTP_DEBUG
		RANGECHK(SCTP_BASE_SYSCTL(sctp_debug_on), SCTPCTL_DEBUG_MIN, SCTPCTL_DEBUG_MAX);
#endif
#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
		RANGECHK(SCTP_BASE_SYSCTL(sctp_output_unlocked), SCTPCTL_OUTPUT_UNLOCKED_MIN, SCTPCTL_OUTPUT_UNLOCKED_MAX);
#endif
	}
	return (error);
}

#if defined(__FreeBSD__) && defined(SMP) && defined(SCTP_USE_PERCPU_STAT)
static int
sysctl_stat_get(SYSCTL_HANDLER_ARGS)
{
	int cpu, error;
	struct sctpstat sb, *sarry;

	memset(&sb, 0, sizeof(sb));
	for (cpu = 0; cpu < mp_ncpus; cpu++) {
		sarry = &SCTP_BASE_STATS[cpu];
		if (sarry->sctps_discontinuitytime.tv_sec > sb.sctps_discontinuitytime.tv_sec) {
			sb.sctps_discontinuitytime.tv_sec = sarry->sctps_discontinuitytime.tv_sec;
			sb.sctps_discontinuitytime.tv_usec = sarry->sctps_discontinuitytime.tv_usec;
		}
		sb.sctps_currestab += sarry->sctps_currestab;
		sb.sctps_activeestab += sarry->sctps_activeestab;
		sb.sctps_restartestab += sarry->sctps_restartestab;
		sb.sctps_collisionestab += sarry->sctps_collisionestab;
		sb.sctps_passiveestab += sarry->sctps_passiveestab;
		sb.sctps_aborted += sarry->sctps_aborted;
		sb.sctps_shutdown += sarry->sctps_shutdown;
		sb.sctps_outoftheblue += sarry->sctps_outoftheblue;
		sb.sctps_checksumerrors += sarry->sctps_checksumerrors;
		sb.sctps_outcontrolchunks += sarry->sctps_outcontrolchunks;
		sb.sctps_outorderchunks += sarry->sctps_outorderchunks;
		sb.sctps_outunorderchunks += sarry->sctps_outunorderchunks;
		sb.sctps_incontrolchunks += sarry->sctps_incontrolchunks;
		sb.sctps_inorderchunks += sarry->sctps_inorderchunks;
		sb.sctps_inunorderchunks += sarry->sctps_inunorderchunks;
		sb.sctps_fragusrmsgs += sarry->sctps_fragusrmsgs;
		sb.sctps_reasmusrmsgs += sarry->sctps_reasmusrmsgs;
		sb.sctps_outpackets += sarry->sctps_outpackets;
		sb.sctps_inpackets += sarry->sctps_inpackets;
		sb.sctps_recvpackets += sarry->sctps_recvpackets;
		sb.sctps_recvdatagrams += sarry->sctps_recvdatagrams;
		sb.sctps_recvpktwithdata += sarry->sctps_recvpktwithdata;
		sb.sctps_recvsacks += sarry->sctps_recvsacks;
		sb.sctps_recvdata += sarry->sctps_recvdata;
		sb.sctps_recvdupdata += sarry->sctps_recvdupdata;
		sb.sctps_recvheartbeat += sarry->sctps_recvheartbeat;
		sb.sctps_recvheartbeatack += sarry->sctps_recvheartbeatack;
		sb.sctps_recvecne += sarry->sctps_recvecne;
		sb.sctps_recvauth += sarry->sctps_recvauth;
		sb.sctps_recvauthmissing += sarry->sctps_recvauthmissing;
		sb.sctps_recvivalhmacid += sarry->sctps_recvivalhmacid;
		sb.sctps_recvivalkeyid += sarry->sctps_recvivalkeyid;
		sb.sctps_recvauthfailed += sarry->sctps_recvauthfailed;
		sb.sctps_recvexpress += sarry->sctps_recvexpress;
		sb.sctps_recvexpressm += sarry->sctps_recvexpressm;
		sb.sctps_recvnocrc += sarry->sctps_recvnocrc;
		sb.sctps_recvswcrc += sarry->sctps_recvswcrc;
		sb.sctps_recvhwcrc += sarry->sctps_recvhwcrc;
		sb.sctps_sendpackets += sarry->sctps_sendpackets;
		sb.sctps_sendsacks += sarry->sctps_sendsacks;
		sb.sctps_senddata += sarry->sctps_senddata;
		sb.sctps_sendretransdata += sarry->sctps_sendretransdata;
		sb.sctps_sendfastretrans += sarry->sctps_sendfastretrans;
		sb.sctps_sendmultfastretrans += sarry->sctps_sendmultfastretrans;
		sb.sctps_sendheartbeat += sarry->sctps_sendheartbeat;
		sb.sctps_sendecne += sarry->sctps_sendecne;
		sb.sctps_sendauth += sarry->sctps_sendauth;
		sb.sctps_senderrors += sarry->sctps_senderrors;
		sb.sctps_sendnocrc += sarry->sctps_sendnocrc;
		sb.sctps_sendswcrc += sarry->sctps_sendswcrc;
		sb.sctps_sendhwcrc += sarry->sctps_sendhwcrc;
		sb.sctps_pdrpfmbox += sarry->sctps_pdrpfmbox;
		sb.sctps_pdrpfehos += sarry->sctps_pdrpfehos;
		sb.sctps_pdrpmbda += sarry->sctps_pdrpmbda;
		sb.sctps_pdrpmbct += sarry->sctps_pdrpmbct;
		sb.sctps_pdrpbwrpt += sarry->sctps_pdrpbwrpt;
		sb.sctps_pdrpcrupt += sarry->sctps_pdrpcrupt;
		sb.sctps_pdrpnedat += sarry->sctps_pdrpnedat;
		sb.sctps_pdrppdbrk += sarry->sctps_pdrppdbrk;
		sb.sctps_pdrptsnnf += sarry->sctps_pdrptsnnf;
		sb.sctps_pdrpdnfnd += sarry->sctps_pdrpdnfnd;
		sb.sctps_pdrpdiwnp += sarry->sctps_pdrpdiwnp;
		sb.sctps_pdrpdizrw += sarry->sctps_pdrpdizrw;
		sb.sctps_pdrpbadd += sarry->sctps_pdrpbadd;
		sb.sctps_pdrpmark += sarry->sctps_pdrpmark;
		sb.sctps_timoiterator += sarry->sctps_timoiterator;
		sb.sctps_timodata += sarry->sctps_timodata;
		sb.sctps_timowindowprobe += sarry->sctps_timowindowprobe;
		sb.sctps_timoinit += sarry->sctps_timoinit;
		sb.sctps_timosack += sarry->sctps_timosack;
		sb.sctps_timoshutdown += sarry->sctps_timoshutdown;
		sb.sctps_timoheartbeat += sarry->sctps_timoheartbeat;
		sb.sctps_timocookie += sarry->sctps_timocookie;
		sb.sctps_timosecret += sarry->sctps_timosecret;
		sb.sctps_timopathmtu += sarry->sctps_timopathmtu;
		sb.sctps_timoshutdownack += sarry->sctps_timoshutdownack;
		sb.sctps_timoshutdownguard += sarry->sctps_timoshutdownguard;
		sb.sctps_timostrmrst += sarry->sctps_timostrmrst;
		sb.sctps_timoearlyfr += sarry->sctps_timoearlyfr;
		sb.sctps_timoasconf += sarry->sctps_timoasconf;
		sb.sctps_timodelprim += sarry->sctps_timodelprim;
		sb.sctps_timoautoclose += sarry->sctps_timoautoclose;
		sb.sctps_timoassockill += sarry->sctps_timoassockill;
		sb.sctps_timoinpkill += sarry->sctps_timoinpkill;
		sb.sctps_earlyfrstart += sarry->sctps_earlyfrstart;
		sb.sctps_earlyfrstop += sarry->sctps_earlyfrstop;
		sb.sctps_earlyfrmrkretrans += sarry->sctps_earlyfrmrkretrans;
		sb.sctps_earlyfrstpout += sarry->sctps_earlyfrstpout;
		sb.sctps_earlyfrstpidsck1 += sarry->sctps_earlyfrstpidsck1;
		sb.sctps_earlyfrstpidsck2 += sarry->sctps_earlyfrstpidsck2;
		sb.sctps_earlyfrstpidsck3 += sarry->sctps_earlyfrstpidsck3;
		sb.sctps_earlyfrstpidsck4 += sarry->sctps_earlyfrstpidsck4;
		sb.sctps_earlyfrstrid += sarry->sctps_earlyfrstrid;
		sb.sctps_earlyfrstrout += sarry->sctps_earlyfrstrout;
		sb.sctps_earlyfrstrtmr += sarry->sctps_earlyfrstrtmr;
		sb.sctps_hdrops += sarry->sctps_hdrops;
		sb.sctps_badsum += sarry->sctps_badsum;
		sb.sctps_noport += sarry->sctps_noport;
		sb.sctps_badvtag += sarry->sctps_badvtag;
		sb.sctps_badsid += sarry->sctps_badsid;
		sb.sctps_nomem += sarry->sctps_nomem;
		sb.sctps_fastretransinrtt += sarry->sctps_fastretransinrtt;
		sb.sctps_markedretrans += sarry->sctps_markedretrans;
		sb.sctps_naglesent += sarry->sctps_naglesent;
		sb.sctps_naglequeued += sarry->sctps_naglequeued;
		sb.sctps_maxburstqueued += sarry->sctps_maxburstqueued;
		sb.sctps_ifnomemqueued += sarry->sctps_ifnomemqueued;
		sb.sctps_windowprobed += sarry->sctps_windowprobed;
		sb.sctps_lowlevelerr += sarry->sctps_lowlevelerr;
		sb.sctps_lowlevelerrusr += sarry->sctps_lowlevelerrusr;
		sb.sctps_datadropchklmt += sarry->sctps_datadropchklmt;
		sb.sctps_datadroprwnd += sarry->sctps_datadroprwnd;
		sb.sctps_ecnereducedcwnd += sarry->sctps_ecnereducedcwnd;
		sb.sctps_vtagexpress += sarry->sctps_vtagexpress;
		sb.sctps_vtagbogus += sarry->sctps_vtagbogus;
		sb.sctps_primary_randry += sarry->sctps_primary_randry;
		sb.sctps_cmt_randry += sarry->sctps_cmt_randry;
		sb.sctps_slowpath_sack += sarry->sctps_slowpath_sack;
		sb.sctps_wu_sacks_sent += sarry->sctps_wu_sacks_sent;
		sb.sctps_sends_with_flags += sarry->sctps_sends_with_flags;
		sb.sctps_sends_with_unord += sarry->sctps_sends_with_unord;
		sb.sctps_sends_with_eof += sarry->sctps_sends_with_eof;
		sb.sctps_sends_with_abort += sarry->sctps_sends_with_abort;
		sb.sctps_protocol_drain_calls += sarry->sctps_protocol_drain_calls;
		sb.sctps_protocol_drains_done += sarry->sctps_protocol_drains_done;
		sb.sctps_read_peeks += sarry->sctps_read_peeks;
		sb.sctps_cached_chk += sarry->sctps_cached_chk;
		sb.sctps_cached_strmoq += sarry->sctps_cached_strmoq;
		sb.sctps_left_abandon += sarry->sctps_left_abandon;
		sb.sctps_send_burst_avoid += sarry->sctps_send_burst_avoid;
		sb.sctps_send_cwnd_avoid += sarry->sctps_send_cwnd_avoid;
		sb.sctps_fwdtsn_map_over += sarry->sctps_fwdtsn_map_over;
	}
	error = SYSCTL_OUT(req, &sb, sizeof(sb));
	return (error);
}

#endif

#if defined(SCTP_LOCAL_TRACE_BUF)
static int
sysctl_sctp_cleartrace(SYSCTL_HANDLER_ARGS)
{
	int error = 0;

	memset(&SCTP_BASE_SYSCTL(sctp_log), 0, sizeof(struct sctp_log));
	return (error);
}

#endif


/*
 * sysctl definitions
 */

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, sendspace, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_sendspace), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAXDGRAM_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, recvspace, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_recvspace), 0, sysctl_sctp_check, "IU",
    SCTPCTL_RECVSPACE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, auto_asconf, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_auto_asconf), 0, sysctl_sctp_check, "IU",
    SCTPCTL_AUTOASCONF_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, ecn_enable, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_ecn_enable), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ECN_ENABLE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, ecn_nonce, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_ecn_nonce), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ECN_NONCE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, strict_sacks, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_strict_sacks), 0, sysctl_sctp_check, "IU",
    SCTPCTL_STRICT_SACKS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, loopback_nocsum, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_no_csum_on_loopback), 0, sysctl_sctp_check, "IU",
    SCTPCTL_LOOPBACK_NOCSUM_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, strict_init, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_strict_init), 0, sysctl_sctp_check, "IU",
    SCTPCTL_STRICT_INIT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, peer_chkoh, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_peer_chunk_oh), 0, sysctl_sctp_check, "IU",
    SCTPCTL_PEER_CHKOH_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, maxburst, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_max_burst_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAXBURST_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, maxchunks, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_max_chunks_on_queue), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAXCHUNKS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, tcbhashsize, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_hashtblsize), 0, sysctl_sctp_check, "IU",
    SCTPCTL_TCBHASHSIZE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, pcbhashsize, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_pcbtblsize), 0, sysctl_sctp_check, "IU",
    SCTPCTL_PCBHASHSIZE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, min_split_point, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_min_split_point), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MIN_SPLIT_POINT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, chunkscale, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_chunkscale), 0, sysctl_sctp_check, "IU",
    SCTPCTL_CHUNKSCALE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, delayed_sack_time, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_delayed_sack_time_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_DELAYED_SACK_TIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, sack_freq, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_sack_freq_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_SACK_FREQ_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, sys_resource, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_system_free_resc_limit), 0, sysctl_sctp_check, "IU",
    SCTPCTL_SYS_RESOURCE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, asoc_resource, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_asoc_free_resc_limit), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ASOC_RESOURCE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, heartbeat_interval, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_heartbeat_interval_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_HEARTBEAT_INTERVAL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, pmtu_raise_time, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_pmtu_raise_time_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_PMTU_RAISE_TIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, shutdown_guard_time, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_shutdown_guard_time_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_SHUTDOWN_GUARD_TIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, secret_lifetime, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_secret_lifetime_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_SECRET_LIFETIME_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, rto_max, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_rto_max_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_RTO_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, rto_min, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_rto_min_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_RTO_MIN_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, rto_initial, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_rto_initial_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_RTO_INITIAL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, init_rto_max, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_init_rto_max_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_INIT_RTO_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, valid_cookie_life, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_valid_cookie_life_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_VALID_COOKIE_LIFE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, init_rtx_max, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_init_rtx_max_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_INIT_RTX_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, assoc_rtx_max, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_assoc_rtx_max_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ASSOC_RTX_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, path_rtx_max, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_path_rtx_max_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_PATH_RTX_MAX_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, add_more_on_output, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_add_more_threshold), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ADD_MORE_ON_OUTPUT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, outgoing_streams, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_nr_outgoing_streams_default), 0, sysctl_sctp_check, "IU",
    SCTPCTL_OUTGOING_STREAMS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cmt_on_off, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_cmt_on_off), 0, sysctl_sctp_check, "IU",
    SCTPCTL_CMT_ON_OFF_DESC);

/* EY */
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, nr_sack_on_off, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_nr_sack_on_off), 0, sysctl_sctp_check, "IU",
    SCTPCTL_NR_SACK_ON_OFF_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cmt_use_dac, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_cmt_use_dac), 0, sysctl_sctp_check, "IU",
    SCTPCTL_CMT_USE_DAC_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cmt_pf, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_cmt_pf), 0, sysctl_sctp_check, "IU",
    SCTPCTL_CMT_PF_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, cwnd_maxburst, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_use_cwnd_based_maxburst), 0, sysctl_sctp_check, "IU",
    SCTPCTL_CWND_MAXBURST_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, early_fast_retran, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_early_fr), 0, sysctl_sctp_check, "IU",
    SCTPCTL_EARLY_FAST_RETRAN_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, early_fast_retran_msec, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_early_fr_msec), 0, sysctl_sctp_check, "IU",
    SCTPCTL_EARLY_FAST_RETRAN_MSEC_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, asconf_auth_nochk, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_asconf_auth_nochk), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ASCONF_AUTH_NOCHK_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, auth_disable, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_auth_disable), 0, sysctl_sctp_check, "IU",
    SCTPCTL_AUTH_DISABLE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, nat_friendly, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_nat_friendly), 0, sysctl_sctp_check, "IU",
    SCTPCTL_NAT_FRIENDLY_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, abc_l_var, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_L2_abc_variable), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ABC_L_VAR_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, max_chained_mbufs, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_mbuf_threshold_count), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAX_CHAINED_MBUFS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, do_sctp_drain, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_do_drain), 0, sysctl_sctp_check, "IU",
    SCTPCTL_DO_SCTP_DRAIN_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, hb_max_burst, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_hb_maxburst), 0, sysctl_sctp_check, "IU",
    SCTPCTL_HB_MAX_BURST_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, abort_at_limit, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_abort_if_one_2_one_hits_limit), 0, sysctl_sctp_check, "IU",
    SCTPCTL_ABORT_AT_LIMIT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, strict_data_order, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_strict_data_order), 0, sysctl_sctp_check, "IU",
    SCTPCTL_STRICT_DATA_ORDER_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, min_residual, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_min_residual), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MIN_RESIDUAL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, max_retran_chunk, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_max_retran_chunk), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MAX_RETRAN_CHUNK_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, log_level, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_logging_level), 0, sysctl_sctp_check, "IU",
    SCTPCTL_LOGGING_LEVEL_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, default_cc_module, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_default_cc_module), 0, sysctl_sctp_check, "IU",
    SCTPCTL_DEFAULT_CC_MODULE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, default_frag_interleave, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_default_frag_interleave), 0, sysctl_sctp_check, "IU",
    SCTPCTL_DEFAULT_FRAG_INTERLEAVE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, mobility_base, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_mobility_base), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MOBILITY_BASE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, mobility_fasthandoff, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_mobility_fasthandoff), 0, sysctl_sctp_check, "IU",
    SCTPCTL_MOBILITY_FASTHANDOFF_DESC);

#if defined(SCTP_LOCAL_TRACE_BUF)
SYSCTL_STRUCT(_net_inet_sctp, OID_AUTO, log, CTLFLAG_RD,
    &SCTP_BASE_SYSCTL(sctp_log), sctp_log,
    "SCTP logging (struct sctp_log)");

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, clear_trace, CTLTYPE_OPAQUE | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_log), 0, sysctl_sctp_cleartrace, "IU",
    "Clear SCTP Logging buffer");



#endif

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, udp_tunneling_for_client_enable, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_udp_tunneling_for_client_enable), 0, sysctl_sctp_check, "IU",
    SCTPCTL_UDP_TUNNELING_FOR_CLIENT_ENABLE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, udp_tunneling_port, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_udp_tunneling_port), 0, sysctl_sctp_udp_tunneling_check, "IU",
    SCTPCTL_UDP_TUNNELING_PORT_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, enable_sack_immediately, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_enable_sack_immediately), 0, sysctl_sctp_check, "IU",
    SCTPCTL_SACK_IMMEDIATELY_ENABLE_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, nat_friendly_init, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_inits_include_nat_friendly), 0, sysctl_sctp_check, "IU",
    SCTPCTL_NAT_FRIENDLY_INITS_DESC);

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, vtag_time_wait, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_vtag_time_wait), 0, sysctl_sctp_check, "IU",
    SCTPCTL_TIME_WAIT_DESC);

#ifdef SCTP_DEBUG
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, debug, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_debug_on), 0, sysctl_sctp_check, "IU",
    SCTPCTL_DEBUG_DESC);
#endif				/* SCTP_DEBUG */


#if defined (__APPLE__) || defined(SCTP_SO_LOCK_TESTING)
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, output_unlocked, CTLTYPE_INT | CTLFLAG_RW,
    &SCTP_BASE_SYSCTL(sctp_output_unlocked), 0, sysctl_sctp_check, "IU",
    SCTPCTL_OUTPUT_UNLOCKED_DESC);
#endif
#if defined(__FreeBSD__) && defined(SMP) && defined(SCTP_USE_PERCPU_STAT)
SYSCTL_PROC(_net_inet_sctp, OID_AUTO, stats,
    CTLTYPE_STRUCT | CTLFLAG_RD,
    0, 0, sysctl_stat_get, "S,sctpstat",
    "SCTP statistics (struct sctp_stat)");
#else
SYSCTL_STRUCT(_net_inet_sctp, OID_AUTO, stats, CTLFLAG_RW,
    &SCTP_BASE_STATS_SYSCTL, sctpstat,
    "SCTP statistics (struct sctp_stat)");
#endif

SYSCTL_PROC(_net_inet_sctp, OID_AUTO, assoclist, CTLFLAG_RD,
    0, 0, sctp_assoclist,
    "S,xassoc", "List of active SCTP associations");
