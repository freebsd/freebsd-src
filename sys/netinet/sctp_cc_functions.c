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

#include <netinet/sctp_os.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_sysctl.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_input.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_auth.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_cc_functions.h>
#include <netinet/sctp_dtrace_declare.h>
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

void
sctp_set_initial_cc_param(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	struct sctp_association *assoc;
	uint32_t cwnd_in_mtu;

	assoc = &stcb->asoc;
	cwnd_in_mtu = SCTP_BASE_SYSCTL(sctp_initial_cwnd);
	if (cwnd_in_mtu == 0) {
		/* Using 0 means that the value of RFC 4960 is used. */
		net->cwnd = min((net->mtu * 4), max((2 * net->mtu), SCTP_INITIAL_CWND));
	} else {
		/*
		 * We take the minimum of the burst limit and the initial
		 * congestion window.
		 */
		if ((assoc->max_burst > 0) && (cwnd_in_mtu > assoc->max_burst))
			cwnd_in_mtu = assoc->max_burst;
		net->cwnd = (net->mtu - sizeof(struct sctphdr)) * cwnd_in_mtu;
	}
	if (stcb->asoc.sctp_cmt_on_off == 2) {
		/* In case of resource pooling initialize appropriately */
		net->cwnd /= assoc->numnets;
		if (net->cwnd < (net->mtu - sizeof(struct sctphdr))) {
			net->cwnd = net->mtu - sizeof(struct sctphdr);
		}
	}
	net->ssthresh = assoc->peers_rwnd;

	SDT_PROBE(sctp, cwnd, net, init,
	    stcb->asoc.my_vtag, ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)), net,
	    0, net->cwnd);
	if (SCTP_BASE_SYSCTL(sctp_logging_level) &
	    (SCTP_CWND_MONITOR_ENABLE | SCTP_CWND_LOGGING_ENABLE)) {
		sctp_log_cwnd(stcb, net, 0, SCTP_CWND_INITIALIZATION);
	}
}

void
sctp_cwnd_update_after_fr(struct sctp_tcb *stcb,
    struct sctp_association *asoc)
{
	struct sctp_nets *net;
	uint32_t t_ssthresh, t_cwnd;

	/* MT FIXME: Don't compute this over and over again */
	t_ssthresh = 0;
	t_cwnd = 0;
	if (asoc->sctp_cmt_on_off == 2) {
		TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
			t_ssthresh += net->ssthresh;
			t_cwnd += net->cwnd;
		}
	}
	/*-
	 * CMT fast recovery code. Need to debug. ((sctp_cmt_on_off > 0) &&
	 * (net->fast_retran_loss_recovery == 0)))
	 */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if ((asoc->fast_retran_loss_recovery == 0) ||
		    (asoc->sctp_cmt_on_off > 0)) {
			/* out of a RFC2582 Fast recovery window? */
			if (net->net_ack > 0) {
				/*
				 * per section 7.2.3, are there any
				 * destinations that had a fast retransmit
				 * to them. If so what we need to do is
				 * adjust ssthresh and cwnd.
				 */
				struct sctp_tmit_chunk *lchk;
				int old_cwnd = net->cwnd;

				if (asoc->sctp_cmt_on_off == 2) {
					net->ssthresh = (uint32_t) (((uint64_t) 4 *
					    (uint64_t) net->mtu *
					    (uint64_t) net->ssthresh) /
					    (uint64_t) t_ssthresh);
					if ((net->cwnd > t_cwnd / 2) &&
					    (net->ssthresh < net->cwnd - t_cwnd / 2)) {
						net->ssthresh = net->cwnd - t_cwnd / 2;
					}
					if (net->ssthresh < net->mtu) {
						net->ssthresh = net->mtu;
					}
				} else {
					net->ssthresh = net->cwnd / 2;
					if (net->ssthresh < (net->mtu * 2)) {
						net->ssthresh = 2 * net->mtu;
					}
				}
				net->cwnd = net->ssthresh;
				SDT_PROBE(sctp, cwnd, net, fr,
				    stcb->asoc.my_vtag, ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)), net,
				    old_cwnd, net->cwnd);
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
					sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd),
					    SCTP_CWND_LOG_FROM_FR);
				}
				lchk = TAILQ_FIRST(&asoc->send_queue);

				net->partial_bytes_acked = 0;
				/* Turn on fast recovery window */
				asoc->fast_retran_loss_recovery = 1;
				if (lchk == NULL) {
					/* Mark end of the window */
					asoc->fast_recovery_tsn = asoc->sending_seq - 1;
				} else {
					asoc->fast_recovery_tsn = lchk->rec.data.TSN_seq - 1;
				}

				/*
				 * CMT fast recovery -- per destination
				 * recovery variable.
				 */
				net->fast_retran_loss_recovery = 1;

				if (lchk == NULL) {
					/* Mark end of the window */
					net->fast_recovery_tsn = asoc->sending_seq - 1;
				} else {
					net->fast_recovery_tsn = lchk->rec.data.TSN_seq - 1;
				}

				/*
				 * Disable Nonce Sum Checking and store the
				 * resync tsn
				 */
				asoc->nonce_sum_check = 0;
				asoc->nonce_resync_tsn = asoc->fast_recovery_tsn + 1;

				sctp_timer_stop(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_32);
				sctp_timer_start(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, net);
			}
		} else if (net->net_ack > 0) {
			/*
			 * Mark a peg that we WOULD have done a cwnd
			 * reduction but RFC2582 prevented this action.
			 */
			SCTP_STAT_INCR(sctps_fastretransinrtt);
		}
	}
}

void
sctp_cwnd_update_after_sack(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    int accum_moved, int reneged_all, int will_exit)
{
	struct sctp_nets *net;
	int old_cwnd;
	uint32_t t_ssthresh, t_cwnd, incr;

	/* MT FIXME: Don't compute this over and over again */
	t_ssthresh = 0;
	t_cwnd = 0;
	if (stcb->asoc.sctp_cmt_on_off == 2) {
		TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
			t_ssthresh += net->ssthresh;
			t_cwnd += net->cwnd;
		}
	}
	/******************************/
	/* update cwnd and Early FR   */
	/******************************/
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {

#ifdef JANA_CMT_FAST_RECOVERY
		/*
		 * CMT fast recovery code. Need to debug.
		 */
		if (net->fast_retran_loss_recovery && net->new_pseudo_cumack) {
			if (SCTP_TSN_GE(asoc->last_acked_seq, net->fast_recovery_tsn) ||
			    SCTP_TSN_GE(net->pseudo_cumack, net->fast_recovery_tsn)) {
				net->will_exit_fast_recovery = 1;
			}
		}
#endif
		if (SCTP_BASE_SYSCTL(sctp_early_fr)) {
			/*
			 * So, first of all do we need to have a Early FR
			 * timer running?
			 */
			if ((!TAILQ_EMPTY(&asoc->sent_queue) &&
			    (net->ref_count > 1) &&
			    (net->flight_size < net->cwnd)) ||
			    (reneged_all)) {
				/*
				 * yes, so in this case stop it if its
				 * running, and then restart it. Reneging
				 * all is a special case where we want to
				 * run the Early FR timer and then force the
				 * last few unacked to be sent, causing us
				 * to illicit a sack with gaps to force out
				 * the others.
				 */
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck2);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_20);
				}
				SCTP_STAT_INCR(sctps_earlyfrstrid);
				sctp_timer_start(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net);
			} else {
				/* No, stop it if its running */
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck3);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_21);
				}
			}
		}
		/* if nothing was acked on this destination skip it */
		if (net->net_ack == 0) {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, 0, SCTP_CWND_LOG_FROM_SACK);
			}
			continue;
		}
		if (net->net_ack2 > 0) {
			/*
			 * Karn's rule applies to clearing error count, this
			 * is optional.
			 */
			net->error_count = 0;
			if ((net->dest_state & SCTP_ADDR_NOT_REACHABLE) ==
			    SCTP_ADDR_NOT_REACHABLE) {
				/* addr came good */
				net->dest_state &= ~SCTP_ADDR_NOT_REACHABLE;
				net->dest_state |= SCTP_ADDR_REACHABLE;
				sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
				    SCTP_RECEIVED_SACK, (void *)net, SCTP_SO_NOT_LOCKED);
				/* now was it the primary? if so restore */
				if (net->dest_state & SCTP_ADDR_WAS_PRIMARY) {
					(void)sctp_set_primary_addr(stcb, (struct sockaddr *)NULL, net);
				}
			}
			/*
			 * JRS 5/14/07 - If CMT PF is on and the destination
			 * is in PF state, set the destination to active
			 * state and set the cwnd to one or two MTU's based
			 * on whether PF1 or PF2 is being used.
			 * 
			 * Should we stop any running T3 timer here?
			 */
			if ((asoc->sctp_cmt_on_off > 0) &&
			    (asoc->sctp_cmt_pf > 0) &&
			    ((net->dest_state & SCTP_ADDR_PF) == SCTP_ADDR_PF)) {
				net->dest_state &= ~SCTP_ADDR_PF;
				old_cwnd = net->cwnd;
				net->cwnd = net->mtu * asoc->sctp_cmt_pf;
				SDT_PROBE(sctp, cwnd, net, ack,
				    stcb->asoc.my_vtag, ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)), net,
				    old_cwnd, net->cwnd);
				SCTPDBG(SCTP_DEBUG_INDATA1, "Destination %p moved from PF to reachable with cwnd %d.\n",
				    net, net->cwnd);
				/*
				 * Since the cwnd value is explicitly set,
				 * skip the code that updates the cwnd
				 * value.
				 */
				goto skip_cwnd_update;
			}
		}
#ifdef JANA_CMT_FAST_RECOVERY
		/*
		 * CMT fast recovery code
		 */
		/*
		 * if (sctp_cmt_on_off > 0 && net->fast_retran_loss_recovery
		 * && net->will_exit_fast_recovery == 0) { @@@ Do something
		 * } else if (sctp_cmt_on_off == 0 &&
		 * asoc->fast_retran_loss_recovery && will_exit == 0) {
		 */
#endif

		if (asoc->fast_retran_loss_recovery &&
		    (will_exit == 0) &&
		    (asoc->sctp_cmt_on_off == 0)) {
			/*
			 * If we are in loss recovery we skip any cwnd
			 * update
			 */
			goto skip_cwnd_update;
		}
		/*
		 * CMT: CUC algorithm. Update cwnd if pseudo-cumack has
		 * moved.
		 */
		if (accum_moved ||
		    ((asoc->sctp_cmt_on_off > 0) && net->new_pseudo_cumack)) {
			/* If the cumulative ack moved we can proceed */
			if (net->cwnd <= net->ssthresh) {
				/* We are in slow start */
				if (net->flight_size + net->net_ack >= net->cwnd) {
					old_cwnd = net->cwnd;
					if (stcb->asoc.sctp_cmt_on_off == 2) {
						uint32_t limit;

						limit = (uint32_t) (((uint64_t) net->mtu *
						    (uint64_t) SCTP_BASE_SYSCTL(sctp_L2_abc_variable) *
						    (uint64_t) net->ssthresh) /
						    (uint64_t) t_ssthresh);
						incr = (uint32_t) (((uint64_t) net->net_ack *
						    (uint64_t) net->ssthresh) /
						    (uint64_t) t_ssthresh);
						if (incr > limit) {
							incr = limit;
						}
						if (incr == 0) {
							incr = 1;
						}
					} else {
						incr = net->net_ack;
						if (incr > net->mtu * SCTP_BASE_SYSCTL(sctp_L2_abc_variable)) {
							incr = net->mtu * SCTP_BASE_SYSCTL(sctp_L2_abc_variable);
						}
					}
					net->cwnd += incr;
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
						sctp_log_cwnd(stcb, net, incr,
						    SCTP_CWND_LOG_FROM_SS);
					}
					SDT_PROBE(sctp, cwnd, net, ack,
					    stcb->asoc.my_vtag,
					    ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)),
					    net,
					    old_cwnd, net->cwnd);
				} else {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
						sctp_log_cwnd(stcb, net, net->net_ack,
						    SCTP_CWND_LOG_NOADV_SS);
					}
				}
			} else {
				/* We are in congestion avoidance */
				uint32_t incr;

				/*
				 * Add to pba
				 */
				net->partial_bytes_acked += net->net_ack;

				if ((net->flight_size + net->net_ack >= net->cwnd) &&
				    (net->partial_bytes_acked >= net->cwnd)) {
					net->partial_bytes_acked -= net->cwnd;
					old_cwnd = net->cwnd;
					if (asoc->sctp_cmt_on_off == 2) {
						incr = (uint32_t) (((uint64_t) net->mtu *
						    (uint64_t) net->ssthresh) /
						    (uint64_t) t_ssthresh);
						if (incr == 0) {
							incr = 1;
						}
					} else {
						incr = net->mtu;
					}
					net->cwnd += incr;
					SDT_PROBE(sctp, cwnd, net, ack,
					    stcb->asoc.my_vtag,
					    ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)),
					    net,
					    old_cwnd, net->cwnd);
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
						sctp_log_cwnd(stcb, net, net->mtu,
						    SCTP_CWND_LOG_FROM_CA);
					}
				} else {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
						sctp_log_cwnd(stcb, net, net->net_ack,
						    SCTP_CWND_LOG_NOADV_CA);
					}
				}
			}
		} else {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, net->mtu,
				    SCTP_CWND_LOG_NO_CUMACK);
			}
		}
skip_cwnd_update:
		/*
		 * NOW, according to Karn's rule do we need to restore the
		 * RTO timer back? Check our net_ack2. If not set then we
		 * have a ambiguity.. i.e. all data ack'd was sent to more
		 * than one place.
		 */
		if (net->net_ack2) {
			/* restore any doubled timers */
			net->RTO = ((net->lastsa >> 2) + net->lastsv) >> 1;
			if (net->RTO < stcb->asoc.minrto) {
				net->RTO = stcb->asoc.minrto;
			}
			if (net->RTO > stcb->asoc.maxrto) {
				net->RTO = stcb->asoc.maxrto;
			}
		}
	}
}

void
sctp_cwnd_update_after_timeout(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	int old_cwnd = net->cwnd;
	uint32_t t_ssthresh, t_cwnd;

	/* MT FIXME: Don't compute this over and over again */
	t_ssthresh = 0;
	t_cwnd = 0;
	if (stcb->asoc.sctp_cmt_on_off == 2) {
		struct sctp_nets *lnet;

		TAILQ_FOREACH(lnet, &stcb->asoc.nets, sctp_next) {
			t_ssthresh += lnet->ssthresh;
			t_cwnd += lnet->cwnd;
		}
		net->ssthresh = (uint32_t) (((uint64_t) 4 *
		    (uint64_t) net->mtu *
		    (uint64_t) net->ssthresh) /
		    (uint64_t) t_ssthresh);
		if ((net->cwnd > t_cwnd / 2) &&
		    (net->ssthresh < net->cwnd - t_cwnd / 2)) {
			net->ssthresh = net->cwnd - t_cwnd / 2;
		}
		if (net->ssthresh < net->mtu) {
			net->ssthresh = net->mtu;
		}
	} else {
		net->ssthresh = max(net->cwnd / 2, 4 * net->mtu);
	}
	net->cwnd = net->mtu;
	net->partial_bytes_acked = 0;
	SDT_PROBE(sctp, cwnd, net, to,
	    stcb->asoc.my_vtag,
	    ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)),
	    net,
	    old_cwnd, net->cwnd);
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, net->cwnd - old_cwnd, SCTP_CWND_LOG_FROM_RTX);
	}
}

void
sctp_cwnd_update_after_ecn_echo(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	int old_cwnd = net->cwnd;

	SCTP_STAT_INCR(sctps_ecnereducedcwnd);
	net->ssthresh = net->cwnd / 2;
	if (net->ssthresh < net->mtu) {
		net->ssthresh = net->mtu;
		/* here back off the timer as well, to slow us down */
		net->RTO <<= 1;
	}
	net->cwnd = net->ssthresh;
	SDT_PROBE(sctp, cwnd, net, ecn,
	    stcb->asoc.my_vtag,
	    ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)),
	    net,
	    old_cwnd, net->cwnd);
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd), SCTP_CWND_LOG_FROM_SAT);
	}
}

void
sctp_cwnd_update_after_packet_dropped(struct sctp_tcb *stcb,
    struct sctp_nets *net, struct sctp_pktdrop_chunk *cp,
    uint32_t * bottle_bw, uint32_t * on_queue)
{
	uint32_t bw_avail;
	int rtt, incr;
	int old_cwnd = net->cwnd;

	/* need real RTT for this calc */
	rtt = ((net->lastsa >> 2) + net->lastsv) >> 1;
	/* get bottle neck bw */
	*bottle_bw = ntohl(cp->bottle_bw);
	/* and whats on queue */
	*on_queue = ntohl(cp->current_onq);
	/*
	 * adjust the on-queue if our flight is more it could be that the
	 * router has not yet gotten data "in-flight" to it
	 */
	if (*on_queue < net->flight_size)
		*on_queue = net->flight_size;
	/* calculate the available space */
	bw_avail = (*bottle_bw * rtt) / 1000;
	if (bw_avail > *bottle_bw) {
		/*
		 * Cap the growth to no more than the bottle neck. This can
		 * happen as RTT slides up due to queues. It also means if
		 * you have more than a 1 second RTT with a empty queue you
		 * will be limited to the bottle_bw per second no matter if
		 * other points have 1/2 the RTT and you could get more
		 * out...
		 */
		bw_avail = *bottle_bw;
	}
	if (*on_queue > bw_avail) {
		/*
		 * No room for anything else don't allow anything else to be
		 * "added to the fire".
		 */
		int seg_inflight, seg_onqueue, my_portion;

		net->partial_bytes_acked = 0;

		/* how much are we over queue size? */
		incr = *on_queue - bw_avail;
		if (stcb->asoc.seen_a_sack_this_pkt) {
			/*
			 * undo any cwnd adjustment that the sack might have
			 * made
			 */
			net->cwnd = net->prev_cwnd;
		}
		/* Now how much of that is mine? */
		seg_inflight = net->flight_size / net->mtu;
		seg_onqueue = *on_queue / net->mtu;
		my_portion = (incr * seg_inflight) / seg_onqueue;

		/* Have I made an adjustment already */
		if (net->cwnd > net->flight_size) {
			/*
			 * for this flight I made an adjustment we need to
			 * decrease the portion by a share our previous
			 * adjustment.
			 */
			int diff_adj;

			diff_adj = net->cwnd - net->flight_size;
			if (diff_adj > my_portion)
				my_portion = 0;
			else
				my_portion -= diff_adj;
		}
		/*
		 * back down to the previous cwnd (assume we have had a sack
		 * before this packet). minus what ever portion of the
		 * overage is my fault.
		 */
		net->cwnd -= my_portion;

		/* we will NOT back down more than 1 MTU */
		if (net->cwnd <= net->mtu) {
			net->cwnd = net->mtu;
		}
		/* force into CA */
		net->ssthresh = net->cwnd - 1;
	} else {
		/*
		 * Take 1/4 of the space left or max burst up .. whichever
		 * is less.
		 */
		incr = min((bw_avail - *on_queue) >> 2,
		    stcb->asoc.max_burst * net->mtu);
		net->cwnd += incr;
	}
	if (net->cwnd > bw_avail) {
		/* We can't exceed the pipe size */
		net->cwnd = bw_avail;
	}
	if (net->cwnd < net->mtu) {
		/* We always have 1 MTU */
		net->cwnd = net->mtu;
	}
	if (net->cwnd - old_cwnd != 0) {
		/* log only changes */
		SDT_PROBE(sctp, cwnd, net, pd,
		    stcb->asoc.my_vtag,
		    ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)),
		    net,
		    old_cwnd, net->cwnd);
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
			sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd),
			    SCTP_CWND_LOG_FROM_SAT);
		}
	}
}

void
sctp_cwnd_update_after_output(struct sctp_tcb *stcb,
    struct sctp_nets *net, int burst_limit)
{
	int old_cwnd = net->cwnd;

	if (net->ssthresh < net->cwnd)
		net->ssthresh = net->cwnd;
	net->cwnd = (net->flight_size + (burst_limit * net->mtu));
	SDT_PROBE(sctp, cwnd, net, bl,
	    stcb->asoc.my_vtag,
	    ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)),
	    net,
	    old_cwnd, net->cwnd);
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd), SCTP_CWND_LOG_FROM_BRST);
	}
}

void
sctp_cwnd_update_after_fr_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	int old_cwnd = net->cwnd;

	sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_EARLY_FR_TMR, SCTP_SO_NOT_LOCKED);
	/*
	 * make a small adjustment to cwnd and force to CA.
	 */
	if (net->cwnd > net->mtu)
		/* drop down one MTU after sending */
		net->cwnd -= net->mtu;
	if (net->cwnd < net->ssthresh)
		/* still in SS move to CA */
		net->ssthresh = net->cwnd - 1;
	SDT_PROBE(sctp, cwnd, net, fr,
	    stcb->asoc.my_vtag,
	    ((stcb->sctp_ep->sctp_lport << 16) | (stcb->rport)),
	    net,
	    old_cwnd, net->cwnd);
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, (old_cwnd - net->cwnd), SCTP_CWND_LOG_FROM_FR);
	}
}

struct sctp_hs_raise_drop {
	int32_t cwnd;
	int32_t increase;
	int32_t drop_percent;
};

#define SCTP_HS_TABLE_SIZE 73

struct sctp_hs_raise_drop sctp_cwnd_adjust[SCTP_HS_TABLE_SIZE] = {
	{38, 1, 50},		/* 0   */
	{118, 2, 44},		/* 1   */
	{221, 3, 41},		/* 2   */
	{347, 4, 38},		/* 3   */
	{495, 5, 37},		/* 4   */
	{663, 6, 35},		/* 5   */
	{851, 7, 34},		/* 6   */
	{1058, 8, 33},		/* 7   */
	{1284, 9, 32},		/* 8   */
	{1529, 10, 31},		/* 9   */
	{1793, 11, 30},		/* 10  */
	{2076, 12, 29},		/* 11  */
	{2378, 13, 28},		/* 12  */
	{2699, 14, 28},		/* 13  */
	{3039, 15, 27},		/* 14  */
	{3399, 16, 27},		/* 15  */
	{3778, 17, 26},		/* 16  */
	{4177, 18, 26},		/* 17  */
	{4596, 19, 25},		/* 18  */
	{5036, 20, 25},		/* 19  */
	{5497, 21, 24},		/* 20  */
	{5979, 22, 24},		/* 21  */
	{6483, 23, 23},		/* 22  */
	{7009, 24, 23},		/* 23  */
	{7558, 25, 22},		/* 24  */
	{8130, 26, 22},		/* 25  */
	{8726, 27, 22},		/* 26  */
	{9346, 28, 21},		/* 27  */
	{9991, 29, 21},		/* 28  */
	{10661, 30, 21},	/* 29  */
	{11358, 31, 20},	/* 30  */
	{12082, 32, 20},	/* 31  */
	{12834, 33, 20},	/* 32  */
	{13614, 34, 19},	/* 33  */
	{14424, 35, 19},	/* 34  */
	{15265, 36, 19},	/* 35  */
	{16137, 37, 19},	/* 36  */
	{17042, 38, 18},	/* 37  */
	{17981, 39, 18},	/* 38  */
	{18955, 40, 18},	/* 39  */
	{19965, 41, 17},	/* 40  */
	{21013, 42, 17},	/* 41  */
	{22101, 43, 17},	/* 42  */
	{23230, 44, 17},	/* 43  */
	{24402, 45, 16},	/* 44  */
	{25618, 46, 16},	/* 45  */
	{26881, 47, 16},	/* 46  */
	{28193, 48, 16},	/* 47  */
	{29557, 49, 15},	/* 48  */
	{30975, 50, 15},	/* 49  */
	{32450, 51, 15},	/* 50  */
	{33986, 52, 15},	/* 51  */
	{35586, 53, 14},	/* 52  */
	{37253, 54, 14},	/* 53  */
	{38992, 55, 14},	/* 54  */
	{40808, 56, 14},	/* 55  */
	{42707, 57, 13},	/* 56  */
	{44694, 58, 13},	/* 57  */
	{46776, 59, 13},	/* 58  */
	{48961, 60, 13},	/* 59  */
	{51258, 61, 13},	/* 60  */
	{53677, 62, 12},	/* 61  */
	{56230, 63, 12},	/* 62  */
	{58932, 64, 12},	/* 63  */
	{61799, 65, 12},	/* 64  */
	{64851, 66, 11},	/* 65  */
	{68113, 67, 11},	/* 66  */
	{71617, 68, 11},	/* 67  */
	{75401, 69, 10},	/* 68  */
	{79517, 70, 10},	/* 69  */
	{84035, 71, 10},	/* 70  */
	{89053, 72, 10},	/* 71  */
	{94717, 73, 9}		/* 72  */
};

static void
sctp_hs_cwnd_increase(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	int cur_val, i, indx, incr;

	cur_val = net->cwnd >> 10;
	indx = SCTP_HS_TABLE_SIZE - 1;
#ifdef SCTP_DEBUG
	printf("HS CC CAlled.\n");
#endif
	if (cur_val < sctp_cwnd_adjust[0].cwnd) {
		/* normal mode */
		if (net->net_ack > net->mtu) {
			net->cwnd += net->mtu;
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
				sctp_log_cwnd(stcb, net, net->mtu, SCTP_CWND_LOG_FROM_SS);
			}
		} else {
			net->cwnd += net->net_ack;
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
				sctp_log_cwnd(stcb, net, net->net_ack, SCTP_CWND_LOG_FROM_SS);
			}
		}
	} else {
		for (i = net->last_hs_used; i < SCTP_HS_TABLE_SIZE; i++) {
			if (cur_val < sctp_cwnd_adjust[i].cwnd) {
				indx = i;
				break;
			}
		}
		net->last_hs_used = indx;
		incr = ((sctp_cwnd_adjust[indx].increase) << 10);
		net->cwnd += incr;
		if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
			sctp_log_cwnd(stcb, net, incr, SCTP_CWND_LOG_FROM_SS);
		}
	}
}

static void
sctp_hs_cwnd_decrease(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	int cur_val, i, indx;
	int old_cwnd = net->cwnd;

	cur_val = net->cwnd >> 10;
	if (cur_val < sctp_cwnd_adjust[0].cwnd) {
		/* normal mode */
		net->ssthresh = net->cwnd / 2;
		if (net->ssthresh < (net->mtu * 2)) {
			net->ssthresh = 2 * net->mtu;
		}
		net->cwnd = net->ssthresh;
	} else {
		/* drop by the proper amount */
		net->ssthresh = net->cwnd - (int)((net->cwnd / 100) *
		    sctp_cwnd_adjust[net->last_hs_used].drop_percent);
		net->cwnd = net->ssthresh;
		/* now where are we */
		indx = net->last_hs_used;
		cur_val = net->cwnd >> 10;
		/* reset where we are in the table */
		if (cur_val < sctp_cwnd_adjust[0].cwnd) {
			/* feel out of hs */
			net->last_hs_used = 0;
		} else {
			for (i = indx; i >= 1; i--) {
				if (cur_val > sctp_cwnd_adjust[i - 1].cwnd) {
					break;
				}
			}
			net->last_hs_used = indx;
		}
	}
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd), SCTP_CWND_LOG_FROM_FR);
	}
}

void
sctp_hs_cwnd_update_after_fr(struct sctp_tcb *stcb,
    struct sctp_association *asoc)
{
	struct sctp_nets *net;

	/*
	 * CMT fast recovery code. Need to debug. ((sctp_cmt_on_off > 0) &&
	 * (net->fast_retran_loss_recovery == 0)))
	 */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if ((asoc->fast_retran_loss_recovery == 0) ||
		    (asoc->sctp_cmt_on_off > 0)) {
			/* out of a RFC2582 Fast recovery window? */
			if (net->net_ack > 0) {
				/*
				 * per section 7.2.3, are there any
				 * destinations that had a fast retransmit
				 * to them. If so what we need to do is
				 * adjust ssthresh and cwnd.
				 */
				struct sctp_tmit_chunk *lchk;

				sctp_hs_cwnd_decrease(stcb, net);

				lchk = TAILQ_FIRST(&asoc->send_queue);

				net->partial_bytes_acked = 0;
				/* Turn on fast recovery window */
				asoc->fast_retran_loss_recovery = 1;
				if (lchk == NULL) {
					/* Mark end of the window */
					asoc->fast_recovery_tsn = asoc->sending_seq - 1;
				} else {
					asoc->fast_recovery_tsn = lchk->rec.data.TSN_seq - 1;
				}

				/*
				 * CMT fast recovery -- per destination
				 * recovery variable.
				 */
				net->fast_retran_loss_recovery = 1;

				if (lchk == NULL) {
					/* Mark end of the window */
					net->fast_recovery_tsn = asoc->sending_seq - 1;
				} else {
					net->fast_recovery_tsn = lchk->rec.data.TSN_seq - 1;
				}

				/*
				 * Disable Nonce Sum Checking and store the
				 * resync tsn
				 */
				asoc->nonce_sum_check = 0;
				asoc->nonce_resync_tsn = asoc->fast_recovery_tsn + 1;

				sctp_timer_stop(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_32);
				sctp_timer_start(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, net);
			}
		} else if (net->net_ack > 0) {
			/*
			 * Mark a peg that we WOULD have done a cwnd
			 * reduction but RFC2582 prevented this action.
			 */
			SCTP_STAT_INCR(sctps_fastretransinrtt);
		}
	}
}

void
sctp_hs_cwnd_update_after_sack(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    int accum_moved, int reneged_all, int will_exit)
{
	struct sctp_nets *net;

	/******************************/
	/* update cwnd and Early FR   */
	/******************************/
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {

#ifdef JANA_CMT_FAST_RECOVERY
		/*
		 * CMT fast recovery code. Need to debug.
		 */
		if (net->fast_retran_loss_recovery && net->new_pseudo_cumack) {
			if (SCTP_TSN_GE(asoc->last_acked_seq, net->fast_recovery_tsn) ||
			    SCTP_TSN_GE(net->pseudo_cumack, net->fast_recovery_tsn)) {
				net->will_exit_fast_recovery = 1;
			}
		}
#endif
		if (SCTP_BASE_SYSCTL(sctp_early_fr)) {
			/*
			 * So, first of all do we need to have a Early FR
			 * timer running?
			 */
			if ((!TAILQ_EMPTY(&asoc->sent_queue) &&
			    (net->ref_count > 1) &&
			    (net->flight_size < net->cwnd)) ||
			    (reneged_all)) {
				/*
				 * yes, so in this case stop it if its
				 * running, and then restart it. Reneging
				 * all is a special case where we want to
				 * run the Early FR timer and then force the
				 * last few unacked to be sent, causing us
				 * to illicit a sack with gaps to force out
				 * the others.
				 */
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck2);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_20);
				}
				SCTP_STAT_INCR(sctps_earlyfrstrid);
				sctp_timer_start(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net);
			} else {
				/* No, stop it if its running */
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck3);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_21);
				}
			}
		}
		/* if nothing was acked on this destination skip it */
		if (net->net_ack == 0) {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, 0, SCTP_CWND_LOG_FROM_SACK);
			}
			continue;
		}
		if (net->net_ack2 > 0) {
			/*
			 * Karn's rule applies to clearing error count, this
			 * is optional.
			 */
			net->error_count = 0;
			if ((net->dest_state & SCTP_ADDR_NOT_REACHABLE) ==
			    SCTP_ADDR_NOT_REACHABLE) {
				/* addr came good */
				net->dest_state &= ~SCTP_ADDR_NOT_REACHABLE;
				net->dest_state |= SCTP_ADDR_REACHABLE;
				sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
				    SCTP_RECEIVED_SACK, (void *)net, SCTP_SO_NOT_LOCKED);
				/* now was it the primary? if so restore */
				if (net->dest_state & SCTP_ADDR_WAS_PRIMARY) {
					(void)sctp_set_primary_addr(stcb, (struct sockaddr *)NULL, net);
				}
			}
			/*
			 * JRS 5/14/07 - If CMT PF is on and the destination
			 * is in PF state, set the destination to active
			 * state and set the cwnd to one or two MTU's based
			 * on whether PF1 or PF2 is being used.
			 * 
			 * Should we stop any running T3 timer here?
			 */
			if ((asoc->sctp_cmt_on_off > 0) &&
			    (asoc->sctp_cmt_pf > 0) &&
			    ((net->dest_state & SCTP_ADDR_PF) == SCTP_ADDR_PF)) {
				net->dest_state &= ~SCTP_ADDR_PF;
				net->cwnd = net->mtu * asoc->sctp_cmt_pf;
				SCTPDBG(SCTP_DEBUG_INDATA1, "Destination %p moved from PF to reachable with cwnd %d.\n",
				    net, net->cwnd);
				/*
				 * Since the cwnd value is explicitly set,
				 * skip the code that updates the cwnd
				 * value.
				 */
				goto skip_cwnd_update;
			}
		}
#ifdef JANA_CMT_FAST_RECOVERY
		/*
		 * CMT fast recovery code
		 */
		/*
		 * if (sctp_cmt_on_off > 0 && net->fast_retran_loss_recovery
		 * && net->will_exit_fast_recovery == 0) { @@@ Do something
		 * } else if (sctp_cmt_on_off == 0 &&
		 * asoc->fast_retran_loss_recovery && will_exit == 0) {
		 */
#endif

		if (asoc->fast_retran_loss_recovery &&
		    (will_exit == 0) &&
		    (asoc->sctp_cmt_on_off == 0)) {
			/*
			 * If we are in loss recovery we skip any cwnd
			 * update
			 */
			goto skip_cwnd_update;
		}
		/*
		 * CMT: CUC algorithm. Update cwnd if pseudo-cumack has
		 * moved.
		 */
		if (accum_moved ||
		    ((asoc->sctp_cmt_on_off > 0) && net->new_pseudo_cumack)) {
			/* If the cumulative ack moved we can proceed */
			if (net->cwnd <= net->ssthresh) {
				/* We are in slow start */
				if (net->flight_size + net->net_ack >= net->cwnd) {

					sctp_hs_cwnd_increase(stcb, net);

				} else {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
						sctp_log_cwnd(stcb, net, net->net_ack,
						    SCTP_CWND_LOG_NOADV_SS);
					}
				}
			} else {
				/* We are in congestion avoidance */
				net->partial_bytes_acked += net->net_ack;
				if ((net->flight_size + net->net_ack >= net->cwnd) &&
				    (net->partial_bytes_acked >= net->cwnd)) {
					net->partial_bytes_acked -= net->cwnd;
					net->cwnd += net->mtu;
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
						sctp_log_cwnd(stcb, net, net->mtu,
						    SCTP_CWND_LOG_FROM_CA);
					}
				} else {
					if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
						sctp_log_cwnd(stcb, net, net->net_ack,
						    SCTP_CWND_LOG_NOADV_CA);
					}
				}
			}
		} else {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, net->mtu,
				    SCTP_CWND_LOG_NO_CUMACK);
			}
		}
skip_cwnd_update:
		/*
		 * NOW, according to Karn's rule do we need to restore the
		 * RTO timer back? Check our net_ack2. If not set then we
		 * have a ambiguity.. i.e. all data ack'd was sent to more
		 * than one place.
		 */
		if (net->net_ack2) {
			/* restore any doubled timers */
			net->RTO = ((net->lastsa >> 2) + net->lastsv) >> 1;
			if (net->RTO < stcb->asoc.minrto) {
				net->RTO = stcb->asoc.minrto;
			}
			if (net->RTO > stcb->asoc.maxrto) {
				net->RTO = stcb->asoc.maxrto;
			}
		}
	}
}


/*
 * H-TCP congestion control. The algorithm is detailed in:
 * R.N.Shorten, D.J.Leith:
 *   "H-TCP: TCP for high-speed and long-distance networks"
 *   Proc. PFLDnet, Argonne, 2004.
 * http://www.hamilton.ie/net/htcp3.pdf
 */


static int use_rtt_scaling = 1;
static int use_bandwidth_switch = 1;

static inline int
between(uint32_t seq1, uint32_t seq2, uint32_t seq3)
{
	return seq3 - seq2 >= seq1 - seq2;
}

static inline uint32_t
htcp_cong_time(struct htcp *ca)
{
	return sctp_get_tick_count() - ca->last_cong;
}

static inline uint32_t
htcp_ccount(struct htcp *ca)
{
	return htcp_cong_time(ca) / ca->minRTT;
}

static inline void
htcp_reset(struct htcp *ca)
{
	ca->undo_last_cong = ca->last_cong;
	ca->undo_maxRTT = ca->maxRTT;
	ca->undo_old_maxB = ca->old_maxB;
	ca->last_cong = sctp_get_tick_count();
}

#ifdef SCTP_NOT_USED

static uint32_t
htcp_cwnd_undo(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	net->htcp_ca.last_cong = net->htcp_ca.undo_last_cong;
	net->htcp_ca.maxRTT = net->htcp_ca.undo_maxRTT;
	net->htcp_ca.old_maxB = net->htcp_ca.undo_old_maxB;
	return max(net->cwnd, ((net->ssthresh / net->mtu << 7) / net->htcp_ca.beta) * net->mtu);
}

#endif

static inline void
measure_rtt(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	uint32_t srtt = net->lastsa >> 3;

	/* keep track of minimum RTT seen so far, minRTT is zero at first */
	if (net->htcp_ca.minRTT > srtt || !net->htcp_ca.minRTT)
		net->htcp_ca.minRTT = srtt;

	/* max RTT */
	if (net->fast_retran_ip == 0 && net->ssthresh < 0xFFFF && htcp_ccount(&net->htcp_ca) > 3) {
		if (net->htcp_ca.maxRTT < net->htcp_ca.minRTT)
			net->htcp_ca.maxRTT = net->htcp_ca.minRTT;
		if (net->htcp_ca.maxRTT < srtt && srtt <= net->htcp_ca.maxRTT + MSEC_TO_TICKS(20))
			net->htcp_ca.maxRTT = srtt;
	}
}

static void
measure_achieved_throughput(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	uint32_t now = sctp_get_tick_count();

	if (net->fast_retran_ip == 0)
		net->htcp_ca.bytes_acked = net->net_ack;

	if (!use_bandwidth_switch)
		return;

	/* achieved throughput calculations */
	/* JRS - not 100% sure of this statement */
	if (net->fast_retran_ip == 1) {
		net->htcp_ca.bytecount = 0;
		net->htcp_ca.lasttime = now;
		return;
	}
	net->htcp_ca.bytecount += net->net_ack;

	if (net->htcp_ca.bytecount >= net->cwnd - ((net->htcp_ca.alpha >> 7 ? : 1) * net->mtu)
	    && now - net->htcp_ca.lasttime >= net->htcp_ca.minRTT
	    && net->htcp_ca.minRTT > 0) {
		uint32_t cur_Bi = net->htcp_ca.bytecount / net->mtu * hz / (now - net->htcp_ca.lasttime);

		if (htcp_ccount(&net->htcp_ca) <= 3) {
			/* just after backoff */
			net->htcp_ca.minB = net->htcp_ca.maxB = net->htcp_ca.Bi = cur_Bi;
		} else {
			net->htcp_ca.Bi = (3 * net->htcp_ca.Bi + cur_Bi) / 4;
			if (net->htcp_ca.Bi > net->htcp_ca.maxB)
				net->htcp_ca.maxB = net->htcp_ca.Bi;
			if (net->htcp_ca.minB > net->htcp_ca.maxB)
				net->htcp_ca.minB = net->htcp_ca.maxB;
		}
		net->htcp_ca.bytecount = 0;
		net->htcp_ca.lasttime = now;
	}
}

static inline void
htcp_beta_update(struct htcp *ca, uint32_t minRTT, uint32_t maxRTT)
{
	if (use_bandwidth_switch) {
		uint32_t maxB = ca->maxB;
		uint32_t old_maxB = ca->old_maxB;

		ca->old_maxB = ca->maxB;

		if (!between(5 * maxB, 4 * old_maxB, 6 * old_maxB)) {
			ca->beta = BETA_MIN;
			ca->modeswitch = 0;
			return;
		}
	}
	if (ca->modeswitch && minRTT > (uint32_t) MSEC_TO_TICKS(10) && maxRTT) {
		ca->beta = (minRTT << 7) / maxRTT;
		if (ca->beta < BETA_MIN)
			ca->beta = BETA_MIN;
		else if (ca->beta > BETA_MAX)
			ca->beta = BETA_MAX;
	} else {
		ca->beta = BETA_MIN;
		ca->modeswitch = 1;
	}
}

static inline void
htcp_alpha_update(struct htcp *ca)
{
	uint32_t minRTT = ca->minRTT;
	uint32_t factor = 1;
	uint32_t diff = htcp_cong_time(ca);

	if (diff > (uint32_t) hz) {
		diff -= hz;
		factor = 1 + (10 * diff + ((diff / 2) * (diff / 2) / hz)) / hz;
	}
	if (use_rtt_scaling && minRTT) {
		uint32_t scale = (hz << 3) / (10 * minRTT);

		scale = min(max(scale, 1U << 2), 10U << 3);	/* clamping ratio to
								 * interval [0.5,10]<<3 */
		factor = (factor << 3) / scale;
		if (!factor)
			factor = 1;
	}
	ca->alpha = 2 * factor * ((1 << 7) - ca->beta);
	if (!ca->alpha)
		ca->alpha = ALPHA_BASE;
}

/* After we have the rtt data to calculate beta, we'd still prefer to wait one
 * rtt before we adjust our beta to ensure we are working from a consistent
 * data.
 *
 * This function should be called when we hit a congestion event since only at
 * that point do we really have a real sense of maxRTT (the queues en route
 * were getting just too full now).
 */
static void
htcp_param_update(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	uint32_t minRTT = net->htcp_ca.minRTT;
	uint32_t maxRTT = net->htcp_ca.maxRTT;

	htcp_beta_update(&net->htcp_ca, minRTT, maxRTT);
	htcp_alpha_update(&net->htcp_ca);

	/*
	 * add slowly fading memory for maxRTT to accommodate routing
	 * changes etc
	 */
	if (minRTT > 0 && maxRTT > minRTT)
		net->htcp_ca.maxRTT = minRTT + ((maxRTT - minRTT) * 95) / 100;
}

static uint32_t
htcp_recalc_ssthresh(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	htcp_param_update(stcb, net);
	return max(((net->cwnd / net->mtu * net->htcp_ca.beta) >> 7) * net->mtu, 2U * net->mtu);
}

static void
htcp_cong_avoid(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	/*-
	 * How to handle these functions?
         *	if (!tcp_is_cwnd_limited(sk, in_flight)) RRS - good question.
	 *		return;
	 */
	if (net->cwnd <= net->ssthresh) {
		/* We are in slow start */
		if (net->flight_size + net->net_ack >= net->cwnd) {
			if (net->net_ack > (net->mtu * SCTP_BASE_SYSCTL(sctp_L2_abc_variable))) {
				net->cwnd += (net->mtu * SCTP_BASE_SYSCTL(sctp_L2_abc_variable));
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
					sctp_log_cwnd(stcb, net, net->mtu,
					    SCTP_CWND_LOG_FROM_SS);
				}
			} else {
				net->cwnd += net->net_ack;
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
					sctp_log_cwnd(stcb, net, net->net_ack,
					    SCTP_CWND_LOG_FROM_SS);
				}
			}
		} else {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, net->net_ack,
				    SCTP_CWND_LOG_NOADV_SS);
			}
		}
	} else {
		measure_rtt(stcb, net);

		/*
		 * In dangerous area, increase slowly. In theory this is
		 * net->cwnd += alpha / net->cwnd
		 */
		/* What is snd_cwnd_cnt?? */
		if (((net->partial_bytes_acked / net->mtu * net->htcp_ca.alpha) >> 7) * net->mtu >= net->cwnd) {
			/*-
			 * Does SCTP have a cwnd clamp?
			 * if (net->snd_cwnd < net->snd_cwnd_clamp) - Nope (RRS).
			 */
			net->cwnd += net->mtu;
			net->partial_bytes_acked = 0;
			htcp_alpha_update(&net->htcp_ca);
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
				sctp_log_cwnd(stcb, net, net->mtu,
				    SCTP_CWND_LOG_FROM_CA);
			}
		} else {
			net->partial_bytes_acked += net->net_ack;
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, net->net_ack,
				    SCTP_CWND_LOG_NOADV_CA);
			}
		}

		net->htcp_ca.bytes_acked = net->mtu;
	}
}

#ifdef SCTP_NOT_USED
/* Lower bound on congestion window. */
static uint32_t
htcp_min_cwnd(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	return net->ssthresh;
}

#endif

static void
htcp_init(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	memset(&net->htcp_ca, 0, sizeof(struct htcp));
	net->htcp_ca.alpha = ALPHA_BASE;
	net->htcp_ca.beta = BETA_MIN;
	net->htcp_ca.bytes_acked = net->mtu;
	net->htcp_ca.last_cong = sctp_get_tick_count();
}

void
sctp_htcp_set_initial_cc_param(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	/*
	 * We take the max of the burst limit times a MTU or the
	 * INITIAL_CWND. We then limit this to 4 MTU's of sending.
	 */
	net->cwnd = min((net->mtu * 4), max((2 * net->mtu), SCTP_INITIAL_CWND));
	net->ssthresh = stcb->asoc.peers_rwnd;
	htcp_init(stcb, net);

	if (SCTP_BASE_SYSCTL(sctp_logging_level) & (SCTP_CWND_MONITOR_ENABLE | SCTP_CWND_LOGGING_ENABLE)) {
		sctp_log_cwnd(stcb, net, 0, SCTP_CWND_INITIALIZATION);
	}
}

void
sctp_htcp_cwnd_update_after_sack(struct sctp_tcb *stcb,
    struct sctp_association *asoc,
    int accum_moved, int reneged_all, int will_exit)
{
	struct sctp_nets *net;

	/******************************/
	/* update cwnd and Early FR   */
	/******************************/
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {

#ifdef JANA_CMT_FAST_RECOVERY
		/*
		 * CMT fast recovery code. Need to debug.
		 */
		if (net->fast_retran_loss_recovery && net->new_pseudo_cumack) {
			if (SCTP_TSN_GE(asoc->last_acked_seq, net->fast_recovery_tsn) ||
			    SCTP_TSN_GE(net->pseudo_cumack, net->fast_recovery_tsn)) {
				net->will_exit_fast_recovery = 1;
			}
		}
#endif
		if (SCTP_BASE_SYSCTL(sctp_early_fr)) {
			/*
			 * So, first of all do we need to have a Early FR
			 * timer running?
			 */
			if ((!TAILQ_EMPTY(&asoc->sent_queue) &&
			    (net->ref_count > 1) &&
			    (net->flight_size < net->cwnd)) ||
			    (reneged_all)) {
				/*
				 * yes, so in this case stop it if its
				 * running, and then restart it. Reneging
				 * all is a special case where we want to
				 * run the Early FR timer and then force the
				 * last few unacked to be sent, causing us
				 * to illicit a sack with gaps to force out
				 * the others.
				 */
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck2);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_20);
				}
				SCTP_STAT_INCR(sctps_earlyfrstrid);
				sctp_timer_start(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net);
			} else {
				/* No, stop it if its running */
				if (SCTP_OS_TIMER_PENDING(&net->fr_timer.timer)) {
					SCTP_STAT_INCR(sctps_earlyfrstpidsck3);
					sctp_timer_stop(SCTP_TIMER_TYPE_EARLYFR, stcb->sctp_ep, stcb, net,
					    SCTP_FROM_SCTP_INDATA + SCTP_LOC_21);
				}
			}
		}
		/* if nothing was acked on this destination skip it */
		if (net->net_ack == 0) {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, 0, SCTP_CWND_LOG_FROM_SACK);
			}
			continue;
		}
		if (net->net_ack2 > 0) {
			/*
			 * Karn's rule applies to clearing error count, this
			 * is optional.
			 */
			net->error_count = 0;
			if ((net->dest_state & SCTP_ADDR_NOT_REACHABLE) ==
			    SCTP_ADDR_NOT_REACHABLE) {
				/* addr came good */
				net->dest_state &= ~SCTP_ADDR_NOT_REACHABLE;
				net->dest_state |= SCTP_ADDR_REACHABLE;
				sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
				    SCTP_RECEIVED_SACK, (void *)net, SCTP_SO_NOT_LOCKED);
				/* now was it the primary? if so restore */
				if (net->dest_state & SCTP_ADDR_WAS_PRIMARY) {
					(void)sctp_set_primary_addr(stcb, (struct sockaddr *)NULL, net);
				}
			}
			/*
			 * JRS 5/14/07 - If CMT PF is on and the destination
			 * is in PF state, set the destination to active
			 * state and set the cwnd to one or two MTU's based
			 * on whether PF1 or PF2 is being used.
			 * 
			 * Should we stop any running T3 timer here?
			 */
			if ((asoc->sctp_cmt_on_off > 0) &&
			    (asoc->sctp_cmt_pf > 0) &&
			    ((net->dest_state & SCTP_ADDR_PF) == SCTP_ADDR_PF)) {
				net->dest_state &= ~SCTP_ADDR_PF;
				net->cwnd = net->mtu * asoc->sctp_cmt_pf;
				SCTPDBG(SCTP_DEBUG_INDATA1, "Destination %p moved from PF to reachable with cwnd %d.\n",
				    net, net->cwnd);
				/*
				 * Since the cwnd value is explicitly set,
				 * skip the code that updates the cwnd
				 * value.
				 */
				goto skip_cwnd_update;
			}
		}
#ifdef JANA_CMT_FAST_RECOVERY
		/*
		 * CMT fast recovery code
		 */
		/*
		 * if (sctp_cmt_on_off > 0 && net->fast_retran_loss_recovery
		 * && net->will_exit_fast_recovery == 0) { @@@ Do something
		 * } else if (sctp_cmt_on_off == 0 &&
		 * asoc->fast_retran_loss_recovery && will_exit == 0) {
		 */
#endif

		if (asoc->fast_retran_loss_recovery &&
		    will_exit == 0 &&
		    (asoc->sctp_cmt_on_off == 0)) {
			/*
			 * If we are in loss recovery we skip any cwnd
			 * update
			 */
			goto skip_cwnd_update;
		}
		/*
		 * CMT: CUC algorithm. Update cwnd if pseudo-cumack has
		 * moved.
		 */
		if (accum_moved ||
		    ((asoc->sctp_cmt_on_off > 0) && net->new_pseudo_cumack)) {
			htcp_cong_avoid(stcb, net);
			measure_achieved_throughput(stcb, net);
		} else {
			if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_LOGGING_ENABLE) {
				sctp_log_cwnd(stcb, net, net->mtu,
				    SCTP_CWND_LOG_NO_CUMACK);
			}
		}
skip_cwnd_update:
		/*
		 * NOW, according to Karn's rule do we need to restore the
		 * RTO timer back? Check our net_ack2. If not set then we
		 * have a ambiguity.. i.e. all data ack'd was sent to more
		 * than one place.
		 */
		if (net->net_ack2) {
			/* restore any doubled timers */
			net->RTO = ((net->lastsa >> 2) + net->lastsv) >> 1;
			if (net->RTO < stcb->asoc.minrto) {
				net->RTO = stcb->asoc.minrto;
			}
			if (net->RTO > stcb->asoc.maxrto) {
				net->RTO = stcb->asoc.maxrto;
			}
		}
	}
}

void
sctp_htcp_cwnd_update_after_fr(struct sctp_tcb *stcb,
    struct sctp_association *asoc)
{
	struct sctp_nets *net;

	/*
	 * CMT fast recovery code. Need to debug. ((sctp_cmt_on_off > 0) &&
	 * (net->fast_retran_loss_recovery == 0)))
	 */
	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		if ((asoc->fast_retran_loss_recovery == 0) ||
		    (asoc->sctp_cmt_on_off > 0)) {
			/* out of a RFC2582 Fast recovery window? */
			if (net->net_ack > 0) {
				/*
				 * per section 7.2.3, are there any
				 * destinations that had a fast retransmit
				 * to them. If so what we need to do is
				 * adjust ssthresh and cwnd.
				 */
				struct sctp_tmit_chunk *lchk;
				int old_cwnd = net->cwnd;

				/* JRS - reset as if state were changed */
				htcp_reset(&net->htcp_ca);
				net->ssthresh = htcp_recalc_ssthresh(stcb, net);
				net->cwnd = net->ssthresh;
				if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
					sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd),
					    SCTP_CWND_LOG_FROM_FR);
				}
				lchk = TAILQ_FIRST(&asoc->send_queue);

				net->partial_bytes_acked = 0;
				/* Turn on fast recovery window */
				asoc->fast_retran_loss_recovery = 1;
				if (lchk == NULL) {
					/* Mark end of the window */
					asoc->fast_recovery_tsn = asoc->sending_seq - 1;
				} else {
					asoc->fast_recovery_tsn = lchk->rec.data.TSN_seq - 1;
				}

				/*
				 * CMT fast recovery -- per destination
				 * recovery variable.
				 */
				net->fast_retran_loss_recovery = 1;

				if (lchk == NULL) {
					/* Mark end of the window */
					net->fast_recovery_tsn = asoc->sending_seq - 1;
				} else {
					net->fast_recovery_tsn = lchk->rec.data.TSN_seq - 1;
				}

				/*
				 * Disable Nonce Sum Checking and store the
				 * resync tsn
				 */
				asoc->nonce_sum_check = 0;
				asoc->nonce_resync_tsn = asoc->fast_recovery_tsn + 1;

				sctp_timer_stop(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, net, SCTP_FROM_SCTP_INDATA + SCTP_LOC_32);
				sctp_timer_start(SCTP_TIMER_TYPE_SEND,
				    stcb->sctp_ep, stcb, net);
			}
		} else if (net->net_ack > 0) {
			/*
			 * Mark a peg that we WOULD have done a cwnd
			 * reduction but RFC2582 prevented this action.
			 */
			SCTP_STAT_INCR(sctps_fastretransinrtt);
		}
	}
}

void
sctp_htcp_cwnd_update_after_timeout(struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	int old_cwnd = net->cwnd;

	/* JRS - reset as if the state were being changed to timeout */
	htcp_reset(&net->htcp_ca);
	net->ssthresh = htcp_recalc_ssthresh(stcb, net);
	net->cwnd = net->mtu;
	net->partial_bytes_acked = 0;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, net->cwnd - old_cwnd, SCTP_CWND_LOG_FROM_RTX);
	}
}

void
sctp_htcp_cwnd_update_after_fr_timer(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	int old_cwnd;

	old_cwnd = net->cwnd;

	sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_EARLY_FR_TMR, SCTP_SO_NOT_LOCKED);
	net->htcp_ca.last_cong = sctp_get_tick_count();
	/*
	 * make a small adjustment to cwnd and force to CA.
	 */
	if (net->cwnd > net->mtu)
		/* drop down one MTU after sending */
		net->cwnd -= net->mtu;
	if (net->cwnd < net->ssthresh)
		/* still in SS move to CA */
		net->ssthresh = net->cwnd - 1;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, (old_cwnd - net->cwnd), SCTP_CWND_LOG_FROM_FR);
	}
}

void
sctp_htcp_cwnd_update_after_ecn_echo(struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	int old_cwnd;

	old_cwnd = net->cwnd;

	/* JRS - reset hctp as if state changed */
	htcp_reset(&net->htcp_ca);
	SCTP_STAT_INCR(sctps_ecnereducedcwnd);
	net->ssthresh = htcp_recalc_ssthresh(stcb, net);
	if (net->ssthresh < net->mtu) {
		net->ssthresh = net->mtu;
		/* here back off the timer as well, to slow us down */
		net->RTO <<= 1;
	}
	net->cwnd = net->ssthresh;
	if (SCTP_BASE_SYSCTL(sctp_logging_level) & SCTP_CWND_MONITOR_ENABLE) {
		sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd), SCTP_CWND_LOG_FROM_SAT);
	}
}
