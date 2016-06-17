/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Implementation of the Transmission Control Protocol(TCP).
 *
 * Version:	$Id: tcp_minisocks.c,v 1.14.2.1 2002/03/05 04:30:08 davem Exp $
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Mark Evans, <evansmp@uhura.aston.ac.uk>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Florian La Roche, <flla@stud.uni-sb.de>
 *		Charles Hedrick, <hedrick@klinzhai.rutgers.edu>
 *		Linus Torvalds, <torvalds@cs.helsinki.fi>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Matthew Dillon, <dillon@apollo.west.oic.com>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Jorge Cwik, <jorge@laser.satlink.net>
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <net/tcp.h>
#include <net/inet_common.h>

#ifdef CONFIG_SYSCTL
#define SYNC_INIT 0 /* let the user enable it */
#else
#define SYNC_INIT 1
#endif

int sysctl_tcp_tw_recycle = 0;
int sysctl_tcp_max_tw_buckets = NR_FILE*2;

int sysctl_tcp_syncookies = SYNC_INIT; 
int sysctl_tcp_abort_on_overflow = 0;

static __inline__ int tcp_in_window(u32 seq, u32 end_seq, u32 s_win, u32 e_win)
{
	if (seq == s_win)
		return 1;
	if (after(end_seq, s_win) && before(seq, e_win))
		return 1;
	return (seq == e_win && seq == end_seq);
}

/* New-style handling of TIME_WAIT sockets. */

int tcp_tw_count = 0;


/* Must be called with locally disabled BHs. */
void tcp_timewait_kill(struct tcp_tw_bucket *tw)
{
	struct tcp_ehash_bucket *ehead;
	struct tcp_bind_hashbucket *bhead;
	struct tcp_bind_bucket *tb;

	/* Unlink from established hashes. */
	ehead = &tcp_ehash[tw->hashent];
	write_lock(&ehead->lock);
	if (!tw->pprev) {
		write_unlock(&ehead->lock);
		return;
	}
	if(tw->next)
		tw->next->pprev = tw->pprev;
	*(tw->pprev) = tw->next;
	tw->pprev = NULL;
	write_unlock(&ehead->lock);

	/* Disassociate with bind bucket. */
	bhead = &tcp_bhash[tcp_bhashfn(tw->num)];
	spin_lock(&bhead->lock);
	tb = tw->tb;
	if(tw->bind_next)
		tw->bind_next->bind_pprev = tw->bind_pprev;
	*(tw->bind_pprev) = tw->bind_next;
	tw->tb = NULL;
	if (tb->owners == NULL) {
		if (tb->next)
			tb->next->pprev = tb->pprev;
		*(tb->pprev) = tb->next;
		kmem_cache_free(tcp_bucket_cachep, tb);
	}
	spin_unlock(&bhead->lock);

#ifdef INET_REFCNT_DEBUG
	if (atomic_read(&tw->refcnt) != 1) {
		printk(KERN_DEBUG "tw_bucket %p refcnt=%d\n", tw, atomic_read(&tw->refcnt));
	}
#endif
	tcp_tw_put(tw);
}

/* 
 * * Main purpose of TIME-WAIT state is to close connection gracefully,
 *   when one of ends sits in LAST-ACK or CLOSING retransmitting FIN
 *   (and, probably, tail of data) and one or more our ACKs are lost.
 * * What is TIME-WAIT timeout? It is associated with maximal packet
 *   lifetime in the internet, which results in wrong conclusion, that
 *   it is set to catch "old duplicate segments" wandering out of their path.
 *   It is not quite correct. This timeout is calculated so that it exceeds
 *   maximal retransmission timeout enough to allow to lose one (or more)
 *   segments sent by peer and our ACKs. This time may be calculated from RTO.
 * * When TIME-WAIT socket receives RST, it means that another end
 *   finally closed and we are allowed to kill TIME-WAIT too.
 * * Second purpose of TIME-WAIT is catching old duplicate segments.
 *   Well, certainly it is pure paranoia, but if we load TIME-WAIT
 *   with this semantics, we MUST NOT kill TIME-WAIT state with RSTs.
 * * If we invented some more clever way to catch duplicates
 *   (f.e. based on PAWS), we could truncate TIME-WAIT to several RTOs.
 *
 * The algorithm below is based on FORMAL INTERPRETATION of RFCs.
 * When you compare it to RFCs, please, read section SEGMENT ARRIVES
 * from the very beginning.
 *
 * NOTE. With recycling (and later with fin-wait-2) TW bucket
 * is _not_ stateless. It means, that strictly speaking we must
 * spinlock it. I do not want! Well, probability of misbehaviour
 * is ridiculously low and, seems, we could use some mb() tricks
 * to avoid misread sequence numbers, states etc.  --ANK
 */
enum tcp_tw_status
tcp_timewait_state_process(struct tcp_tw_bucket *tw, struct sk_buff *skb,
			   struct tcphdr *th, unsigned len)
{
	struct tcp_opt tp;
	int paws_reject = 0;

	tp.saw_tstamp = 0;
	if (th->doff > (sizeof(struct tcphdr)>>2) && tw->ts_recent_stamp) {
		tcp_parse_options(skb, &tp, 0);

		if (tp.saw_tstamp) {
			tp.ts_recent = tw->ts_recent;
			tp.ts_recent_stamp = tw->ts_recent_stamp;
			paws_reject = tcp_paws_check(&tp, th->rst);
		}
	}

	if (tw->substate == TCP_FIN_WAIT2) {
		/* Just repeat all the checks of tcp_rcv_state_process() */

		/* Out of window, send ACK */
		if (paws_reject ||
		    !tcp_in_window(TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
				   tw->rcv_nxt, tw->rcv_nxt + tw->rcv_wnd))
			return TCP_TW_ACK;

		if (th->rst)
			goto kill;

		if (th->syn && !before(TCP_SKB_CB(skb)->seq, tw->rcv_nxt))
			goto kill_with_rst;

		/* Dup ACK? */
		if (!after(TCP_SKB_CB(skb)->end_seq, tw->rcv_nxt) ||
		    TCP_SKB_CB(skb)->end_seq == TCP_SKB_CB(skb)->seq) {
			tcp_tw_put(tw);
			return TCP_TW_SUCCESS;
		}

		/* New data or FIN. If new data arrive after half-duplex close,
		 * reset.
		 */
		if (!th->fin || TCP_SKB_CB(skb)->end_seq != tw->rcv_nxt+1) {
kill_with_rst:
			tcp_tw_deschedule(tw);
			tcp_timewait_kill(tw);
			tcp_tw_put(tw);
			return TCP_TW_RST;
		}

		/* FIN arrived, enter true time-wait state. */
		tw->substate = TCP_TIME_WAIT;
		tw->rcv_nxt = TCP_SKB_CB(skb)->end_seq;
		if (tp.saw_tstamp) {
			tw->ts_recent_stamp = xtime.tv_sec;
			tw->ts_recent = tp.rcv_tsval;
		}

		/* I am shamed, but failed to make it more elegant.
		 * Yes, it is direct reference to IP, which is impossible
		 * to generalize to IPv6. Taking into account that IPv6
		 * do not undertsnad recycling in any case, it not
		 * a big problem in practice. --ANK */
		if (tw->family == AF_INET &&
		    sysctl_tcp_tw_recycle && tw->ts_recent_stamp &&
		    tcp_v4_tw_remember_stamp(tw))
			tcp_tw_schedule(tw, tw->timeout);
		else
			tcp_tw_schedule(tw, TCP_TIMEWAIT_LEN);
		return TCP_TW_ACK;
	}

	/*
	 *	Now real TIME-WAIT state.
	 *
	 *	RFC 1122:
	 *	"When a connection is [...] on TIME-WAIT state [...]
	 *	[a TCP] MAY accept a new SYN from the remote TCP to
	 *	reopen the connection directly, if it:
	 *	
	 *	(1)  assigns its initial sequence number for the new
	 *	connection to be larger than the largest sequence
	 *	number it used on the previous connection incarnation,
	 *	and
	 *
	 *	(2)  returns to TIME-WAIT state if the SYN turns out 
	 *	to be an old duplicate".
	 */

	if (!paws_reject &&
	    (TCP_SKB_CB(skb)->seq == tw->rcv_nxt &&
	     (TCP_SKB_CB(skb)->seq == TCP_SKB_CB(skb)->end_seq || th->rst))) {
		/* In window segment, it may be only reset or bare ack. */

		if (th->rst) {
			/* This is TIME_WAIT assasination, in two flavors.
			 * Oh well... nobody has a sufficient solution to this
			 * protocol bug yet.
			 */
			if (sysctl_tcp_rfc1337 == 0) {
kill:
				tcp_tw_deschedule(tw);
				tcp_timewait_kill(tw);
				tcp_tw_put(tw);
				return TCP_TW_SUCCESS;
			}
		}
		tcp_tw_schedule(tw, TCP_TIMEWAIT_LEN);

		if (tp.saw_tstamp) {
			tw->ts_recent = tp.rcv_tsval;
			tw->ts_recent_stamp = xtime.tv_sec;
		}

		tcp_tw_put(tw);
		return TCP_TW_SUCCESS;
	}

	/* Out of window segment.

	   All the segments are ACKed immediately.

	   The only exception is new SYN. We accept it, if it is
	   not old duplicate and we are not in danger to be killed
	   by delayed old duplicates. RFC check is that it has
	   newer sequence number works at rates <40Mbit/sec.
	   However, if paws works, it is reliable AND even more,
	   we even may relax silly seq space cutoff.

	   RED-PEN: we violate main RFC requirement, if this SYN will appear
	   old duplicate (i.e. we receive RST in reply to SYN-ACK),
	   we must return socket to time-wait state. It is not good,
	   but not fatal yet.
	 */

	if (th->syn && !th->rst && !th->ack && !paws_reject &&
	    (after(TCP_SKB_CB(skb)->seq, tw->rcv_nxt) ||
	     (tp.saw_tstamp && (s32)(tw->ts_recent - tp.rcv_tsval) < 0))) {
		u32 isn = tw->snd_nxt+65535+2;
		if (isn == 0)
			isn++;
		TCP_SKB_CB(skb)->when = isn;
		return TCP_TW_SYN;
	}

	if (paws_reject)
		NET_INC_STATS_BH(PAWSEstabRejected);

	if(!th->rst) {
		/* In this case we must reset the TIMEWAIT timer.
		 *
		 * If it is ACKless SYN it may be both old duplicate
		 * and new good SYN with random sequence number <rcv_nxt.
		 * Do not reschedule in the last case.
		 */
		if (paws_reject || th->ack)
			tcp_tw_schedule(tw, TCP_TIMEWAIT_LEN);

		/* Send ACK. Note, we do not put the bucket,
		 * it will be released by caller.
		 */
		return TCP_TW_ACK;
	}
	tcp_tw_put(tw);
	return TCP_TW_SUCCESS;
}

/* Enter the time wait state.  This is called with locally disabled BH.
 * Essentially we whip up a timewait bucket, copy the
 * relevant info into it from the SK, and mess with hash chains
 * and list linkage.
 */
static void __tcp_tw_hashdance(struct sock *sk, struct tcp_tw_bucket *tw)
{
	struct tcp_ehash_bucket *ehead = &tcp_ehash[sk->hashent];
	struct tcp_bind_hashbucket *bhead;
	struct sock **head, *sktw;

	/* Step 1: Put TW into bind hash. Original socket stays there too.
	   Note, that any socket with sk->num!=0 MUST be bound in binding
	   cache, even if it is closed.
	 */
	bhead = &tcp_bhash[tcp_bhashfn(sk->num)];
	spin_lock(&bhead->lock);
	tw->tb = (struct tcp_bind_bucket *)sk->prev;
	BUG_TRAP(sk->prev!=NULL);
	if ((tw->bind_next = tw->tb->owners) != NULL)
		tw->tb->owners->bind_pprev = &tw->bind_next;
	tw->tb->owners = (struct sock*)tw;
	tw->bind_pprev = &tw->tb->owners;
	spin_unlock(&bhead->lock);

	write_lock(&ehead->lock);

	/* Step 2: Remove SK from established hash. */
	if (sk->pprev) {
		if(sk->next)
			sk->next->pprev = sk->pprev;
		*sk->pprev = sk->next;
		sk->pprev = NULL;
		sock_prot_dec_use(sk->prot);
	}

	/* Step 3: Hash TW into TIMEWAIT half of established hash table. */
	head = &(ehead + tcp_ehash_size)->chain;
	sktw = (struct sock *)tw;
	if((sktw->next = *head) != NULL)
		(*head)->pprev = &sktw->next;
	*head = sktw;
	sktw->pprev = head;
	atomic_inc(&tw->refcnt);

	write_unlock(&ehead->lock);
}

/* 
 * Move a socket to time-wait or dead fin-wait-2 state.
 */ 
void tcp_time_wait(struct sock *sk, int state, int timeo)
{
	struct tcp_tw_bucket *tw = NULL;
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);
	int recycle_ok = 0;

	if (sysctl_tcp_tw_recycle && tp->ts_recent_stamp)
		recycle_ok = tp->af_specific->remember_stamp(sk);

	if (tcp_tw_count < sysctl_tcp_max_tw_buckets)
		tw = kmem_cache_alloc(tcp_timewait_cachep, SLAB_ATOMIC);

	if(tw != NULL) {
		int rto = (tp->rto<<2) - (tp->rto>>1);

		/* Give us an identity. */
		tw->daddr	= sk->daddr;
		tw->rcv_saddr	= sk->rcv_saddr;
		tw->bound_dev_if= sk->bound_dev_if;
		tw->num		= sk->num;
		tw->state	= TCP_TIME_WAIT;
		tw->substate	= state;
		tw->sport	= sk->sport;
		tw->dport	= sk->dport;
		tw->family	= sk->family;
		tw->reuse	= sk->reuse;
		tw->rcv_wscale	= tp->rcv_wscale;
		atomic_set(&tw->refcnt, 1);

		tw->hashent	= sk->hashent;
		tw->rcv_nxt	= tp->rcv_nxt;
		tw->snd_nxt	= tp->snd_nxt;
		tw->rcv_wnd	= tcp_receive_window(tp);
		tw->ts_recent	= tp->ts_recent;
		tw->ts_recent_stamp= tp->ts_recent_stamp;
		tw->pprev_death = NULL;

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
		if(tw->family == PF_INET6) {
			memcpy(&tw->v6_daddr,
			       &sk->net_pinfo.af_inet6.daddr,
			       sizeof(struct in6_addr));
			memcpy(&tw->v6_rcv_saddr,
			       &sk->net_pinfo.af_inet6.rcv_saddr,
			       sizeof(struct in6_addr));
		}
#endif
		/* Linkage updates. */
		__tcp_tw_hashdance(sk, tw);

		/* Get the TIME_WAIT timeout firing. */
		if (timeo < rto)
			timeo = rto;

		if (recycle_ok) {
			tw->timeout = rto;
		} else {
			tw->timeout = TCP_TIMEWAIT_LEN;
			if (state == TCP_TIME_WAIT)
				timeo = TCP_TIMEWAIT_LEN;
		}

		tcp_tw_schedule(tw, timeo);
		tcp_tw_put(tw);
	} else {
		/* Sorry, if we're out of memory, just CLOSE this
		 * socket up.  We've got bigger problems than
		 * non-graceful socket closings.
		 */
		if (net_ratelimit())
			printk(KERN_INFO "TCP: time wait bucket table overflow\n");
	}

	tcp_update_metrics(sk);
	tcp_done(sk);
}

/* Kill off TIME_WAIT sockets once their lifetime has expired. */
static int tcp_tw_death_row_slot = 0;

static void tcp_twkill(unsigned long);

static struct tcp_tw_bucket *tcp_tw_death_row[TCP_TWKILL_SLOTS];
static spinlock_t tw_death_lock = SPIN_LOCK_UNLOCKED;
static struct timer_list tcp_tw_timer = { function: tcp_twkill };

static void SMP_TIMER_NAME(tcp_twkill)(unsigned long dummy)
{
	struct tcp_tw_bucket *tw;
	int killed = 0;

	/* NOTE: compare this to previous version where lock
	 * was released after detaching chain. It was racy,
	 * because tw buckets are scheduled in not serialized context
	 * in 2.3 (with netfilter), and with softnet it is common, because
	 * soft irqs are not sequenced.
	 */
	spin_lock(&tw_death_lock);

	if (tcp_tw_count == 0)
		goto out;

	while((tw = tcp_tw_death_row[tcp_tw_death_row_slot]) != NULL) {
		tcp_tw_death_row[tcp_tw_death_row_slot] = tw->next_death;
		if (tw->next_death)
			tw->next_death->pprev_death = tw->pprev_death;
		tw->pprev_death = NULL;
		spin_unlock(&tw_death_lock);

		tcp_timewait_kill(tw);
		tcp_tw_put(tw);

		killed++;

		spin_lock(&tw_death_lock);
	}
	tcp_tw_death_row_slot =
		((tcp_tw_death_row_slot + 1) & (TCP_TWKILL_SLOTS - 1));

	if ((tcp_tw_count -= killed) != 0)
		mod_timer(&tcp_tw_timer, jiffies+TCP_TWKILL_PERIOD);
	net_statistics[smp_processor_id()*2].TimeWaited += killed;
out:
	spin_unlock(&tw_death_lock);
}

SMP_TIMER_DEFINE(tcp_twkill, tcp_twkill_task);

/* These are always called from BH context.  See callers in
 * tcp_input.c to verify this.
 */

/* This is for handling early-kills of TIME_WAIT sockets. */
void tcp_tw_deschedule(struct tcp_tw_bucket *tw)
{
	spin_lock(&tw_death_lock);
	if (tw->pprev_death) {
		if(tw->next_death)
			tw->next_death->pprev_death = tw->pprev_death;
		*tw->pprev_death = tw->next_death;
		tw->pprev_death = NULL;
		tcp_tw_put(tw);
		if (--tcp_tw_count == 0)
			del_timer(&tcp_tw_timer);
	}
	spin_unlock(&tw_death_lock);
}

/* Short-time timewait calendar */

static int tcp_twcal_hand = -1;
static int tcp_twcal_jiffie;
static void tcp_twcal_tick(unsigned long);
static struct timer_list tcp_twcal_timer = {function: tcp_twcal_tick};
static struct tcp_tw_bucket *tcp_twcal_row[TCP_TW_RECYCLE_SLOTS];

void tcp_tw_schedule(struct tcp_tw_bucket *tw, int timeo)
{
	struct tcp_tw_bucket **tpp;
	int slot;

	/* timeout := RTO * 3.5
	 *
	 * 3.5 = 1+2+0.5 to wait for two retransmits.
	 *
	 * RATIONALE: if FIN arrived and we entered TIME-WAIT state,
	 * our ACK acking that FIN can be lost. If N subsequent retransmitted
	 * FINs (or previous seqments) are lost (probability of such event
	 * is p^(N+1), where p is probability to lose single packet and
	 * time to detect the loss is about RTO*(2^N - 1) with exponential
	 * backoff). Normal timewait length is calculated so, that we
	 * waited at least for one retransmitted FIN (maximal RTO is 120sec).
	 * [ BTW Linux. following BSD, violates this requirement waiting
	 *   only for 60sec, we should wait at least for 240 secs.
	 *   Well, 240 consumes too much of resources 8)
	 * ]
	 * This interval is not reduced to catch old duplicate and
	 * responces to our wandering segments living for two MSLs.
	 * However, if we use PAWS to detect
	 * old duplicates, we can reduce the interval to bounds required
	 * by RTO, rather than MSL. So, if peer understands PAWS, we
	 * kill tw bucket after 3.5*RTO (it is important that this number
	 * is greater than TS tick!) and detect old duplicates with help
	 * of PAWS.
	 */
	slot = (timeo + (1<<TCP_TW_RECYCLE_TICK) - 1) >> TCP_TW_RECYCLE_TICK;

	spin_lock(&tw_death_lock);

	/* Unlink it, if it was scheduled */
	if (tw->pprev_death) {
		if(tw->next_death)
			tw->next_death->pprev_death = tw->pprev_death;
		*tw->pprev_death = tw->next_death;
		tw->pprev_death = NULL;
		tcp_tw_count--;
	} else
		atomic_inc(&tw->refcnt);

	if (slot >= TCP_TW_RECYCLE_SLOTS) {
		/* Schedule to slow timer */
		if (timeo >= TCP_TIMEWAIT_LEN) {
			slot = TCP_TWKILL_SLOTS-1;
		} else {
			slot = (timeo + TCP_TWKILL_PERIOD-1) / TCP_TWKILL_PERIOD;
			if (slot >= TCP_TWKILL_SLOTS)
				slot = TCP_TWKILL_SLOTS-1;
		}
		tw->ttd = jiffies + timeo;
		slot = (tcp_tw_death_row_slot + slot) & (TCP_TWKILL_SLOTS - 1);
		tpp = &tcp_tw_death_row[slot];
	} else {
		tw->ttd = jiffies + (slot<<TCP_TW_RECYCLE_TICK);

		if (tcp_twcal_hand < 0) {
			tcp_twcal_hand = 0;
			tcp_twcal_jiffie = jiffies;
			tcp_twcal_timer.expires = tcp_twcal_jiffie + (slot<<TCP_TW_RECYCLE_TICK);
			add_timer(&tcp_twcal_timer);
		} else {
			if ((long)(tcp_twcal_timer.expires - jiffies) > (slot<<TCP_TW_RECYCLE_TICK))
				mod_timer(&tcp_twcal_timer, jiffies + (slot<<TCP_TW_RECYCLE_TICK));
			slot = (tcp_twcal_hand + slot)&(TCP_TW_RECYCLE_SLOTS-1);
		}
		tpp = &tcp_twcal_row[slot];
	}

	if((tw->next_death = *tpp) != NULL)
		(*tpp)->pprev_death = &tw->next_death;
	*tpp = tw;
	tw->pprev_death = tpp;

	if (tcp_tw_count++ == 0)
		mod_timer(&tcp_tw_timer, jiffies+TCP_TWKILL_PERIOD);
	spin_unlock(&tw_death_lock);
}

void SMP_TIMER_NAME(tcp_twcal_tick)(unsigned long dummy)
{
	int n, slot;
	unsigned long j;
	unsigned long now = jiffies;
	int killed = 0;
	int adv = 0;

	spin_lock(&tw_death_lock);
	if (tcp_twcal_hand < 0)
		goto out;

	slot = tcp_twcal_hand;
	j = tcp_twcal_jiffie;

	for (n=0; n<TCP_TW_RECYCLE_SLOTS; n++) {
		if ((long)(j - now) <= 0) {
			struct tcp_tw_bucket *tw;

			while((tw = tcp_twcal_row[slot]) != NULL) {
				tcp_twcal_row[slot] = tw->next_death;
				tw->pprev_death = NULL;

				tcp_timewait_kill(tw);
				tcp_tw_put(tw);
				killed++;
			}
		} else {
			if (!adv) {
				adv = 1;
				tcp_twcal_jiffie = j;
				tcp_twcal_hand = slot;
			}

			if (tcp_twcal_row[slot] != NULL) {
				mod_timer(&tcp_twcal_timer, j);
				goto out;
			}
		}
		j += (1<<TCP_TW_RECYCLE_TICK);
		slot = (slot+1)&(TCP_TW_RECYCLE_SLOTS-1);
	}
	tcp_twcal_hand = -1;

out:
	if ((tcp_tw_count -= killed) == 0)
		del_timer(&tcp_tw_timer);
	net_statistics[smp_processor_id()*2].TimeWaitKilled += killed;
	spin_unlock(&tw_death_lock);
}

SMP_TIMER_DEFINE(tcp_twcal_tick, tcp_twcal_tasklet);


/* This is not only more efficient than what we used to do, it eliminates
 * a lot of code duplication between IPv4/IPv6 SYN recv processing. -DaveM
 *
 * Actually, we could lots of memory writes here. tp of listening
 * socket contains all necessary default parameters.
 */
struct sock *tcp_create_openreq_child(struct sock *sk, struct open_request *req, struct sk_buff *skb)
{
	struct sock *newsk = sk_alloc(PF_INET, GFP_ATOMIC, 0);

	if(newsk != NULL) {
		struct tcp_opt *newtp;
#ifdef CONFIG_FILTER
		struct sk_filter *filter;
#endif

		memcpy(newsk, sk, sizeof(*newsk));
		newsk->state = TCP_SYN_RECV;

		/* SANITY */
		newsk->pprev = NULL;
		newsk->prev = NULL;

		/* Clone the TCP header template */
		newsk->dport = req->rmt_port;

		sock_lock_init(newsk);
		bh_lock_sock(newsk);

		newsk->dst_lock	= RW_LOCK_UNLOCKED;
		atomic_set(&newsk->rmem_alloc, 0);
		skb_queue_head_init(&newsk->receive_queue);
		atomic_set(&newsk->wmem_alloc, 0);
		skb_queue_head_init(&newsk->write_queue);
		atomic_set(&newsk->omem_alloc, 0);
		newsk->wmem_queued = 0;
		newsk->forward_alloc = 0;

		newsk->done = 0;
		newsk->userlocks = sk->userlocks & ~SOCK_BINDPORT_LOCK;
		newsk->proc = 0;
		newsk->backlog.head = newsk->backlog.tail = NULL;
		newsk->callback_lock = RW_LOCK_UNLOCKED;
		skb_queue_head_init(&newsk->error_queue);
		newsk->write_space = tcp_write_space;
#ifdef CONFIG_FILTER
		if ((filter = newsk->filter) != NULL)
			sk_filter_charge(newsk, filter);
#endif

		/* Now setup tcp_opt */
		newtp = &(newsk->tp_pinfo.af_tcp);
		newtp->pred_flags = 0;
		newtp->rcv_nxt = req->rcv_isn + 1;
		newtp->snd_nxt = req->snt_isn + 1;
		newtp->snd_una = req->snt_isn + 1;
		newtp->snd_sml = req->snt_isn + 1;

		tcp_prequeue_init(newtp);

		tcp_init_wl(newtp, req->snt_isn, req->rcv_isn);

		newtp->retransmits = 0;
		newtp->backoff = 0;
		newtp->srtt = 0;
		newtp->mdev = TCP_TIMEOUT_INIT;
		newtp->rto = TCP_TIMEOUT_INIT;

		newtp->packets_out = 0;
		newtp->left_out = 0;
		newtp->retrans_out = 0;
		newtp->sacked_out = 0;
		newtp->fackets_out = 0;
		newtp->snd_ssthresh = 0x7fffffff;

		/* So many TCP implementations out there (incorrectly) count the
		 * initial SYN frame in their delayed-ACK and congestion control
		 * algorithms that we must have the following bandaid to talk
		 * efficiently to them.  -DaveM
		 */
		newtp->snd_cwnd = 2;
		newtp->snd_cwnd_cnt = 0;

		newtp->frto_counter = 0;
		newtp->frto_highmark = 0;

		newtp->ca_state = TCP_CA_Open;
		tcp_init_xmit_timers(newsk);
		skb_queue_head_init(&newtp->out_of_order_queue);
		newtp->send_head = NULL;
		newtp->rcv_wup = req->rcv_isn + 1;
		newtp->write_seq = req->snt_isn + 1;
		newtp->pushed_seq = newtp->write_seq;
		newtp->copied_seq = req->rcv_isn + 1;

		newtp->saw_tstamp = 0;

		newtp->dsack = 0;
		newtp->eff_sacks = 0;

		newtp->probes_out = 0;
		newtp->num_sacks = 0;
		newtp->urg_data = 0;
		newtp->listen_opt = NULL;
		newtp->accept_queue = newtp->accept_queue_tail = NULL;
		/* Deinitialize syn_wait_lock to trap illegal accesses. */
		memset(&newtp->syn_wait_lock, 0, sizeof(newtp->syn_wait_lock));

		/* Back to base struct sock members. */
		newsk->err = 0;
		newsk->priority = 0;
		atomic_set(&newsk->refcnt, 2);
#ifdef INET_REFCNT_DEBUG
		atomic_inc(&inet_sock_nr);
#endif
		atomic_inc(&tcp_sockets_allocated);

		if (newsk->keepopen)
			tcp_reset_keepalive_timer(newsk, keepalive_time_when(newtp));
		newsk->socket = NULL;
		newsk->sleep = NULL;

		newtp->tstamp_ok = req->tstamp_ok;
		if((newtp->sack_ok = req->sack_ok) != 0) {
			if (sysctl_tcp_fack)
				newtp->sack_ok |= 2;
		}
		newtp->window_clamp = req->window_clamp;
		newtp->rcv_ssthresh = req->rcv_wnd;
		newtp->rcv_wnd = req->rcv_wnd;
		newtp->wscale_ok = req->wscale_ok;
		if (newtp->wscale_ok) {
			newtp->snd_wscale = req->snd_wscale;
			newtp->rcv_wscale = req->rcv_wscale;
		} else {
			newtp->snd_wscale = newtp->rcv_wscale = 0;
			newtp->window_clamp = min(newtp->window_clamp, 65535U);
		}
		newtp->snd_wnd = ntohs(skb->h.th->window) << newtp->snd_wscale;
		newtp->max_window = newtp->snd_wnd;

		if (newtp->tstamp_ok) {
			newtp->ts_recent = req->ts_recent;
			newtp->ts_recent_stamp = xtime.tv_sec;
			newtp->tcp_header_len = sizeof(struct tcphdr) + TCPOLEN_TSTAMP_ALIGNED;
		} else {
			newtp->ts_recent_stamp = 0;
			newtp->tcp_header_len = sizeof(struct tcphdr);
		}
		if (skb->len >= TCP_MIN_RCVMSS+newtp->tcp_header_len)
			newtp->ack.last_seg_size = skb->len-newtp->tcp_header_len;
		newtp->mss_clamp = req->mss;
		TCP_ECN_openreq_child(newtp, req);

		TCP_INC_STATS_BH(TcpPassiveOpens);
	}
	return newsk;
}

/* 
 *	Process an incoming packet for SYN_RECV sockets represented
 *	as an open_request.
 */

struct sock *tcp_check_req(struct sock *sk,struct sk_buff *skb,
			   struct open_request *req,
			   struct open_request **prev)
{
	struct tcphdr *th = skb->h.th;
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);
	u32 flg = tcp_flag_word(th) & (TCP_FLAG_RST|TCP_FLAG_SYN|TCP_FLAG_ACK);
	int paws_reject = 0;
	struct tcp_opt ttp;
	struct sock *child;

	ttp.saw_tstamp = 0;
	if (th->doff > (sizeof(struct tcphdr)>>2)) {
		tcp_parse_options(skb, &ttp, 0);

		if (ttp.saw_tstamp) {
			ttp.ts_recent = req->ts_recent;
			/* We do not store true stamp, but it is not required,
			 * it can be estimated (approximately)
			 * from another data.
			 */
			ttp.ts_recent_stamp = xtime.tv_sec - ((TCP_TIMEOUT_INIT/HZ)<<req->retrans);
			paws_reject = tcp_paws_check(&ttp, th->rst);
		}
	}

	/* Check for pure retransmitted SYN. */
	if (TCP_SKB_CB(skb)->seq == req->rcv_isn &&
	    flg == TCP_FLAG_SYN &&
	    !paws_reject) {
		/*
		 * RFC793 draws (Incorrectly! It was fixed in RFC1122)
		 * this case on figure 6 and figure 8, but formal
		 * protocol description says NOTHING.
		 * To be more exact, it says that we should send ACK,
		 * because this segment (at least, if it has no data)
		 * is out of window.
		 *
		 *  CONCLUSION: RFC793 (even with RFC1122) DOES NOT
		 *  describe SYN-RECV state. All the description
		 *  is wrong, we cannot believe to it and should
		 *  rely only on common sense and implementation
		 *  experience.
		 *
		 * Enforce "SYN-ACK" according to figure 8, figure 6
		 * of RFC793, fixed by RFC1122.
		 */
		req->class->rtx_syn_ack(sk, req, NULL);
		return NULL;
	}

	/* Further reproduces section "SEGMENT ARRIVES"
	   for state SYN-RECEIVED of RFC793.
	   It is broken, however, it does not work only
	   when SYNs are crossed.

	   You would think that SYN crossing is impossible here, since
	   we should have a SYN_SENT socket (from connect()) on our end,
	   but this is not true if the crossed SYNs were sent to both
	   ends by a malicious third party.  We must defend against this,
	   and to do that we first verify the ACK (as per RFC793, page
	   36) and reset if it is invalid.  Is this a true full defense?
	   To convince ourselves, let us consider a way in which the ACK
	   test can still pass in this 'malicious crossed SYNs' case.
	   Malicious sender sends identical SYNs (and thus identical sequence
	   numbers) to both A and B:

		A: gets SYN, seq=7
		B: gets SYN, seq=7

	   By our good fortune, both A and B select the same initial
	   send sequence number of seven :-)

		A: sends SYN|ACK, seq=7, ack_seq=8
		B: sends SYN|ACK, seq=7, ack_seq=8

	   So we are now A eating this SYN|ACK, ACK test passes.  So
	   does sequence test, SYN is truncated, and thus we consider
	   it a bare ACK.

	   If tp->defer_accept, we silently drop this bare ACK.  Otherwise,
	   we create an established connection.  Both ends (listening sockets)
	   accept the new incoming connection and try to talk to each other. 8-)

	   Note: This case is both harmless, and rare.  Possibility is about the
	   same as us discovering intelligent life on another plant tomorrow.

	   But generally, we should (RFC lies!) to accept ACK
	   from SYNACK both here and in tcp_rcv_state_process().
	   tcp_rcv_state_process() does not, hence, we do not too.

	   Note that the case is absolutely generic:
	   we cannot optimize anything here without
	   violating protocol. All the checks must be made
	   before attempt to create socket.
	 */

	/* RFC793 page 36: "If the connection is in any non-synchronized state ...
	 *                  and the incoming segment acknowledges something not yet
	 *                  sent (the segment carries an unaccaptable ACK) ...
	 *                  a reset is sent."
	 *
	 * Invalid ACK: reset will be sent by listening socket
	 */
	if ((flg & TCP_FLAG_ACK) &&
	    (TCP_SKB_CB(skb)->ack_seq != req->snt_isn+1))
		return sk;

	/* Also, it would be not so bad idea to check rcv_tsecr, which
	 * is essentially ACK extension and too early or too late values
	 * should cause reset in unsynchronized states.
	 */

	/* RFC793: "first check sequence number". */

	if (paws_reject || !tcp_in_window(TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
					  req->rcv_isn+1, req->rcv_isn+1+req->rcv_wnd)) {
		/* Out of window: send ACK and drop. */
		if (!(flg & TCP_FLAG_RST))
			req->class->send_ack(skb, req);
		if (paws_reject)
			NET_INC_STATS_BH(PAWSEstabRejected);
		return NULL;
	}

	/* In sequence, PAWS is OK. */

	if (ttp.saw_tstamp && !after(TCP_SKB_CB(skb)->seq, req->rcv_isn+1))
		req->ts_recent = ttp.rcv_tsval;

	if (TCP_SKB_CB(skb)->seq == req->rcv_isn) {
		/* Truncate SYN, it is out of window starting
		   at req->rcv_isn+1. */
		flg &= ~TCP_FLAG_SYN;
	}

	/* RFC793: "second check the RST bit" and
	 *	   "fourth, check the SYN bit"
	 */
	if (flg & (TCP_FLAG_RST|TCP_FLAG_SYN))
		goto embryonic_reset;

	/* ACK sequence verified above, just make sure ACK is
	 * set.  If ACK not set, just silently drop the packet.
	 */
	if (!(flg & TCP_FLAG_ACK))
		return NULL;

	/* If TCP_DEFER_ACCEPT is set, drop bare ACK. */
	if (tp->defer_accept && TCP_SKB_CB(skb)->end_seq == req->rcv_isn+1) {
		req->acked = 1;
		return NULL;
	}

	/* OK, ACK is valid, create big socket and
	 * feed this segment to it. It will repeat all
	 * the tests. THIS SEGMENT MUST MOVE SOCKET TO
	 * ESTABLISHED STATE. If it will be dropped after
	 * socket is created, wait for troubles.
	 */
	child = tp->af_specific->syn_recv_sock(sk, skb, req, NULL);
	if (child == NULL)
		goto listen_overflow;

	tcp_synq_unlink(tp, req, prev);
	tcp_synq_removed(sk, req);

	tcp_acceptq_queue(sk, req, child);
	return child;

listen_overflow:
	if (!sysctl_tcp_abort_on_overflow) {
		req->acked = 1;
		return NULL;
	}

embryonic_reset:
	NET_INC_STATS_BH(EmbryonicRsts);
	if (!(flg & TCP_FLAG_RST))
		req->class->send_reset(skb);

	tcp_synq_drop(sk, req, prev);
	return NULL;
}

/*
 * Queue segment on the new socket if the new socket is active,
 * otherwise we just shortcircuit this and continue with
 * the new socket.
 */

int tcp_child_process(struct sock *parent, struct sock *child,
		      struct sk_buff *skb)
{
	int ret = 0;
	int state = child->state;

	if (child->lock.users == 0) {
		ret = tcp_rcv_state_process(child, skb, skb->h.th, skb->len);

		/* Wakeup parent, send SIGIO */
		if (state == TCP_SYN_RECV && child->state != state)
			parent->data_ready(parent, 0);
	} else {
		/* Alas, it is possible again, because we do lookup
		 * in main socket hash table and lock on listening
		 * socket does not protect us more.
		 */
		sk_add_backlog(child, skb);
	}

	bh_unlock_sock(child);
	sock_put(child);
	return ret;
}
