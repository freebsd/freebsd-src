/*-
 * Copyright (c) 2009-2010
 * 	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by David Hayes and Lawrence Stewart,
 * made possible in part by a grant from the FreeBSD Foundation and
 * Cisco University Research Program Fund at Community Foundation
 * Silicon Valley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netinet/h_ertt.h>
#include <netinet/helper.h>
#include <netinet/helper_module.h>
#include <netinet/hhooks.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>

#include <vm/uma.h>

static VNET_DEFINE(uma_zone_t, txseginfo_zone);
#define	V_txseginfo_zone	VNET(txseginfo_zone)

#define DLYACK_SMOOTH  5 /* smoothing factor for delayed ack guess */
#define MAX_TS_ERR    10 /* maximum number of time stamp errors allowed in a session */

void ertt_packet_measurement_hook(void *udata, void *ctx_data, void *dblock);
void ertt_add_tx_segment_info_hook(void *udata, void *ctx_data, void *dblock);
int ertt_mod_init(void);
int ertt_mod_destroy(void);
int ertt_uma_ctor(void *mem, int size, void *arg, int flags);
void ertt_uma_dtor(void *mem, int size, void *arg);

/* Structure contains the information about the
   sent segment, for comparison with the corresponding ack */
struct txseginfo {
	TAILQ_ENTRY(txseginfo) txsegi_lnk;
	/* segment sequence number */
	tcp_seq seq;
	long len;
	/* time stamp indicating when the packet was sent */
	u_int32_t  tx_ts;
	/* Last received receiver ts (if the tcp option is used). */
	u_int32_t  rx_ts;
	/* flags for operation */
	u_int flags;
};

/* txseginfo flags */
#define TXSI_TSO               0x01 /* TSO was used for this entry */
#define TXSI_RTT_MEASURE_START 0x02 /* a rate measure starts here based on this txsi's rtt */
#define TXSI_RX_MEASURE_END    0x04 /* measure the received rate until this txsi */

struct helper ertt_helper = {
	.mod_init = ertt_mod_init,
	.mod_destroy = ertt_mod_destroy,
	.flags = HELPER_NEEDS_DBLOCK,
	.class = HELPER_CLASS_TCP
};

#define MULTI_ACK 1 
	static void inline
marked_packet_rtt(struct txseginfo *txsi, struct ertt *e_t, struct tcpcb *tp, struct tcphdr *th,
		u_int32_t *pmeasurenext, int multiack) 
{
	/* if we can't measure this one properly due to delayed acking */
	/*        adjust byte counters and flag to measure next txsi. */
	/*        Note that since the marked packet's tx and rx bytes are measured */
	/*        we need to subtract the tx, and not add the rx. */
	/*        Then pretend the next txsi was marked  */
	if (multiack && e_t->dlyack_rx && !*pmeasurenext) {
		*pmeasurenext=txsi->tx_ts; 
	} else {
		if (*pmeasurenext)
			e_t->markedpkt_rtt = ticks - *pmeasurenext + 1;
		else
			e_t->markedpkt_rtt = ticks - txsi->tx_ts + 1;
		e_t->bytes_tx_in_marked_rtt=e_t->bytes_tx_in_rtt;
		e_t->marked_snd_cwnd=tp->snd_cwnd;

		/* set flags */
		e_t->flags &= ~ERTT_MEASUREMENT_IN_PROGRESS; /* Not measuring - indicates to add_tx_segment_info 
								a new measurment needs to be started */
		e_t->flags |= ERTT_NEW_MEASUREMENT; /* indicates to the CC that a new marked RTT measurement has been taken */
		if (tp->t_flags & TF_TSO) {
			tp->t_flags &= ~TF_TSO;      /* temporarily disable TSO to aid in a new measurment */
			e_t->flags |= ERTT_TSO_DISABLED; /* note that I've done it so I can renable it later */
		}
	}
}



/* packet_measurements use state kept on each packet sent to more accurately and more
 * securely measure the round trip time. The resulting measurement is used for
 * congestion control algorithms which require a more accurate time.
*/
	void
ertt_packet_measurement_hook(void *udata, void *ctx_data, void *dblock)
{
	//struct ertt *e = (struct ertt *)(((struct tcpcb *)inp->inp_ppcb)->helper_data[0]);
	struct tcpcb *tp = ((struct tcp_hhook_data *)ctx_data)->tp;
	struct tcphdr *th = ((struct tcp_hhook_data *)ctx_data)->th;
	struct tcpopt *to = ((struct tcp_hhook_data *)ctx_data)->to;
	int new_sacked_bytes = ((struct tcp_hhook_data *)ctx_data)->new_sacked_bytes;
	struct ertt *e_t = (struct ertt *)dblock;

	printf("In the hook with e_t->rtt: %d, ctx_data: %p, curack = %u\n",
			e_t->rtt, ctx_data, th->th_ack);

	struct txseginfo *txsi;
	u_int32_t  rts=0;
	u_int32_t  measurenext=0;
	tcp_seq ack;
	int multiack=0;


	INP_WLOCK_ASSERT(tp->t_inpcb);
	int acked = th->th_ack - tp->snd_una;
	/* Packet has provided new acknowledgements */
	if (acked > 0 || new_sacked_bytes) {
		if (acked == 0 && new_sacked_bytes) {
			/* no delayed acks at the moment,
			   use packets being acknowledged with sack instead of th_ack*/
			ack = tp->sackhint.last_sack_ack;
		} else
			ack = th->th_ack;


		txsi = TAILQ_FIRST(&e_t->txsegi_q);
		while(txsi != NULL) {
			rts = 0;


			if (SEQ_GT(ack, txsi->seq+txsi->len)) { /* acking more than this txsi */
				if (txsi->flags & TXSI_RTT_MEASURE_START || measurenext)
					marked_packet_rtt(txsi, e_t, tp, th, &measurenext, MULTI_ACK);
				TAILQ_REMOVE(&e_t->txsegi_q, txsi, txsegi_lnk);
				uma_zfree(V_txseginfo_zone, txsi);
				txsi = TAILQ_FIRST(&e_t->txsegi_q);
				continue; 
			}


			/* Guess if delayed acks are being used by the receiver */
			if (!new_sacked_bytes) {
				if (acked > tp->t_maxseg) {
					e_t->dlyack_rx += (e_t->dlyack_rx < DLYACK_SMOOTH) ? 1 : 0;
					multiack=1;
				} else if (acked >  txsi->len) {
					multiack=1;
					e_t->dlyack_rx += (e_t->dlyack_rx < DLYACK_SMOOTH) ? 1 : 0;
				} else if (acked == tp->t_maxseg || acked ==  txsi->len)
					e_t->dlyack_rx -= (e_t->dlyack_rx > 0) ? 1 : 0;
				/* otherwise leave dlyack_rx the way it was */
			}

			/* Time stamps are only used to help identify packets */
			if (e_t->timestamp_errors < MAX_TS_ERR &&
					(to->to_flags & TOF_TS) != 0 && to->to_tsecr) {
				/* Note: All packets sent with the offload will have the same time stamp.
				   If we are sending on a fast interface, and the t_maxseg is much
				   smaller than one tick, this will be fine. The time stamp would be
				   the same whether we were using tso or not. However, if the interface
				   is slow, this will cause problems with the calculations. If the interface
				   is slow, there is not reason to be using tso, and it should be turned off. */
				/* If there are too many time stamp errors, time stamps won't be trusted */
				rts = to->to_tsecr;
				if (!e_t->dlyack_rx && TSTMP_LT(rts,txsi->tx_ts)) /*before this packet */
					/* When delayed acking is used, the reflected time stamp
					   is of the first packet, and thus may be before txsi->tx_ts*/
					break;
				if (TSTMP_GT(rts,txsi->tx_ts)) {
					/* if reflected time stamp is later than tx_tsi, then this txsi is old */
					if (txsi->flags & TXSI_RTT_MEASURE_START || measurenext)
						marked_packet_rtt(txsi, e_t, tp, th, &measurenext, 0);
					TAILQ_REMOVE(&e_t->txsegi_q, txsi, txsegi_lnk);
					uma_zfree(V_txseginfo_zone, txsi);
					txsi = TAILQ_FIRST(&e_t->txsegi_q);
					continue; 
				}
				if (rts == txsi->tx_ts && TSTMP_LT(to->to_tsval,txsi->rx_ts)) {
					/* rx before sent!!! something wrong with rx timestamping
					   process without timestamps */
					e_t->timestamp_errors++;
				}
			}

			/* old txsi that may have had the same seq numbers (rtx) should have been
			   removed if time stamps are being used */
			if (SEQ_LEQ(ack,txsi->seq))
				break; /* before first packet in txsi */

			/* only ack > txsi->seq and ack <= txsi->seq+txsi->len past this point */


			if (!e_t->dlyack_rx || multiack || new_sacked_bytes) {
				e_t->rtt = ticks - txsi->tx_ts + 1; /* new measurement */
				if (e_t->rtt < e_t->minrtt || e_t->minrtt==0)
					e_t->minrtt=e_t->rtt;
			}

			if (txsi->flags & TXSI_RTT_MEASURE_START || measurenext)
				marked_packet_rtt(txsi, e_t, tp, th, &measurenext, 0);

			if (txsi->flags & TXSI_TSO) {
				txsi->len -= acked;
				if (txsi->len > 0) {
					/* this presumes ack for first bytes in txsi,
					   this may not be true but it shouldn't
					   cause problems for the timing */
					txsi->seq += acked;
					txsi->flags &= ~TXSI_RTT_MEASURE_START; /* reset measure flag */
					break; /* still more data to be acked with this tso transmission */
				}
			}
			TAILQ_REMOVE(&e_t->txsegi_q, txsi, txsegi_lnk);
			uma_zfree(V_txseginfo_zone, txsi);
			break;
		} /* end while */
		if (measurenext)  /* need to do a tx rate measurement, won't be the best if I'm doing it here */
			marked_packet_rtt(txsi, e_t, tp, th, &measurenext, 0);
	}
}

/* add transmitted segment info to the list */
void
ertt_add_tx_segment_info_hook(void *udata, void *ctx_data, void *dblock)
{
	struct tcpcb *tp = ((struct tcp_hhook_data *)ctx_data)->tp;
	struct tcphdr *th = ((struct tcp_hhook_data *)ctx_data)->th;
	struct tcpopt *to = ((struct tcp_hhook_data *)ctx_data)->to;
	long len = ((struct tcp_hhook_data *)ctx_data)->len;
	int tso = ((struct tcp_hhook_data *)ctx_data)->tso;


	INP_WLOCK_ASSERT(tp->t_inpcb); 


	if(len > 0) {
		struct txseginfo *txsi;
		txsi =  (struct txseginfo *) uma_zalloc(V_txseginfo_zone, M_NOWAIT);
		if (txsi != NULL) {
			struct ertt *e_t= (struct ertt *)dblock;
			txsi->flags=0; /* needs to be initialised */
			txsi->seq = ntohl(th->th_seq);
			txsi->len = len;
			if (tso)
				txsi->flags |= TXSI_TSO;
			else
				if (e_t->flags & ERTT_TSO_DISABLED) {
					tp->t_flags |= TF_TSO;
					e_t->flags &= ~ERTT_TSO_DISABLED;
				}
			if (e_t->flags & ERTT_MEASUREMENT_IN_PROGRESS) {
				e_t->bytes_tx_in_rtt += len;
			} else {
				txsi->flags |= TXSI_RTT_MEASURE_START;
				e_t->flags |= ERTT_MEASUREMENT_IN_PROGRESS;
				e_t->bytes_tx_in_rtt = len;
			}
			if (((tp->t_flags & TF_NOOPT) == 0) && (to->to_flags & TOF_TS)) {
				txsi->tx_ts = ntohl(to->to_tsval) - tp->ts_offset;
				txsi->rx_ts = ntohl(to->to_tsecr);
			} else {
				txsi->tx_ts = ticks;
				txsi->rx_ts = 0; /* no received time stamp */
			}
			TAILQ_INSERT_TAIL(&e_t->txsegi_q, txsi, txsegi_lnk);
		}
		printf("** A %u %ld %d\n", ntohl(th->th_seq), len, tso); 
	}
}

int
ertt_mod_init(void)
{
	int ret;

	V_txseginfo_zone = uma_zcreate("txseginfo", sizeof(struct txseginfo),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);

	ret = register_hhook(HHOOK_TYPE_TCP, HHOOK_TCP_ESTABLISHED_IN,
	    &ertt_helper, &ertt_packet_measurement_hook, NULL, HHOOK_WAITOK);
	if (ret)
		return (ret);

	return register_hhook(HHOOK_TYPE_TCP, HHOOK_TCP_ESTABLISHED_OUT,
	    &ertt_helper, & ertt_add_tx_segment_info_hook, NULL, HHOOK_WAITOK);
}

int
ertt_mod_destroy(void)
{
	int ret;
	ret = deregister_hhook(HHOOK_TYPE_TCP, HHOOK_TCP_ESTABLISHED_IN,
	    &ertt_packet_measurement_hook, NULL, 0);
	ret += deregister_hhook(HHOOK_TYPE_TCP, HHOOK_TCP_ESTABLISHED_OUT,
	    &ertt_add_tx_segment_info_hook, NULL, 0);

	uma_zdestroy(V_txseginfo_zone);

	return (ret);
}

int
ertt_uma_ctor(void *mem, int size, void *arg, int flags)
{
	struct ertt *e_t = (struct ertt *)mem;

	TAILQ_INIT(&e_t->txsegi_q);
	e_t->timestamp_errors=0;
	e_t->minrtt = 0;
	e_t->maxrtt = 0;
	e_t->rtt = 0;
	e_t->flags=0;
	e_t->dlyack_rx = 0;
	e_t->bytes_tx_in_rtt = 0;
	e_t->markedpkt_rtt = 0;

	printf("Creating ertt block %p\n", mem);

	return (0);
}

void
ertt_uma_dtor(void *mem, int size, void *arg)
{
	struct ertt *e_t = (struct ertt *)mem;
	struct txseginfo *txsi, *n_txsi;

	txsi = TAILQ_FIRST(&e_t->txsegi_q);
	while (txsi != NULL) {
		n_txsi = TAILQ_NEXT(txsi, txsegi_lnk);
		uma_zfree(V_txseginfo_zone, txsi);
		txsi = n_txsi;
	}

	printf("Destroying ertt block %p\n", mem);
}

DECLARE_HELPER_UMA(ertt, &ertt_helper, 1, sizeof(struct ertt), ertt_uma_ctor, ertt_uma_dtor);
