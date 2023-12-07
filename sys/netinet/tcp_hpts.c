/*-
 * Copyright (c) 2016-2018 Netflix, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

/**
 * Some notes about usage.
 *
 * The tcp_hpts system is designed to provide a high precision timer
 * system for tcp. Its main purpose is to provide a mechanism for
 * pacing packets out onto the wire. It can be used in two ways
 * by a given TCP stack (and those two methods can be used simultaneously).
 *
 * First, and probably the main thing its used by Rack and BBR, it can
 * be used to call tcp_output() of a transport stack at some time in the future.
 * The normal way this is done is that tcp_output() of the stack schedules
 * itself to be called again by calling tcp_hpts_insert(tcpcb, slot). The
 * slot is the time from now that the stack wants to be called but it
 * must be converted to tcp_hpts's notion of slot. This is done with
 * one of the macros HPTS_MS_TO_SLOTS or HPTS_USEC_TO_SLOTS. So a typical
 * call from the tcp_output() routine might look like:
 *
 * tcp_hpts_insert(tp, HPTS_USEC_TO_SLOTS(550));
 *
 * The above would schedule tcp_ouput() to be called in 550 useconds.
 * Note that if using this mechanism the stack will want to add near
 * its top a check to prevent unwanted calls (from user land or the
 * arrival of incoming ack's). So it would add something like:
 *
 * if (tcp_in_hpts(inp))
 *    return;
 *
 * to prevent output processing until the time alotted has gone by.
 * Of course this is a bare bones example and the stack will probably
 * have more consideration then just the above.
 *
 * In order to run input queued segments from the HPTS context the
 * tcp stack must define an input function for
 * tfb_do_queued_segments(). This function understands
 * how to dequeue a array of packets that were input and
 * knows how to call the correct processing routine.
 *
 * Locking in this is important as well so most likely the
 * stack will need to define the tfb_do_segment_nounlock()
 * splitting tfb_do_segment() into two parts. The main processing
 * part that does not unlock the INP and returns a value of 1 or 0.
 * It returns 0 if all is well and the lock was not released. It
 * returns 1 if we had to destroy the TCB (a reset received etc).
 * The remains of tfb_do_segment() then become just a simple call
 * to the tfb_do_segment_nounlock() function and check the return
 * code and possibly unlock.
 *
 * The stack must also set the flag on the INP that it supports this
 * feature i.e. INP_SUPPORTS_MBUFQ. The LRO code recoginizes
 * this flag as well and will queue packets when it is set.
 * There are other flags as well INP_MBUF_QUEUE_READY and
 * INP_DONT_SACK_QUEUE. The first flag tells the LRO code
 * that we are in the pacer for output so there is no
 * need to wake up the hpts system to get immediate
 * input. The second tells the LRO code that its okay
 * if a SACK arrives you can still defer input and let
 * the current hpts timer run (this is usually set when
 * a rack timer is up so we know SACK's are happening
 * on the connection already and don't want to wakeup yet).
 *
 * There is a common functions within the rack_bbr_common code
 * version i.e. ctf_do_queued_segments(). This function
 * knows how to take the input queue of packets from tp->t_inqueue
 * and process them digging out all the arguments, calling any bpf tap and
 * calling into tfb_do_segment_nounlock(). The common
 * function (ctf_do_queued_segments())  requires that
 * you have defined the tfb_do_segment_nounlock() as
 * described above.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/hhook.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>		/* for proc0 declaration */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/refcount.h>
#include <sys/sched.h>
#include <sys/queue.h>
#include <sys/smp.h>
#include <sys/counter.h>
#include <sys/time.h>
#include <sys/kthread.h>
#include <sys/kern_prefetch.h>

#include <vm/uma.h>
#include <vm/vm.h>

#include <net/route.h>
#include <net/vnet.h>

#ifdef RSS
#include <net/netisr.h>
#include <net/rss_config.h>
#endif

#define TCPSTATES		/* for logging */

#include <netinet/in.h>
#include <netinet/in_kdtrace.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>	/* required for icmp_var.h */
#include <netinet/icmp_var.h>	/* for ICMP_BANDLIM */
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/ip6_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/cc/cc.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_log_buf.h>

#ifdef tcp_offload
#include <netinet/tcp_offload.h>
#endif

/*
 * The hpts uses a 102400 wheel. The wheel
 * defines the time in 10 usec increments (102400 x 10).
 * This gives a range of 10usec - 1024ms to place
 * an entry within. If the user requests more than
 * 1.024 second, a remaineder is attached and the hpts
 * when seeing the remainder will re-insert the
 * inpcb forward in time from where it is until
 * the remainder is zero.
 */

#define NUM_OF_HPTSI_SLOTS 102400

/* Each hpts has its own p_mtx which is used for locking */
#define	HPTS_MTX_ASSERT(hpts)	mtx_assert(&(hpts)->p_mtx, MA_OWNED)
#define	HPTS_LOCK(hpts)		mtx_lock(&(hpts)->p_mtx)
#define	HPTS_UNLOCK(hpts)	mtx_unlock(&(hpts)->p_mtx)
struct tcp_hpts_entry {
	/* Cache line 0x00 */
	struct mtx p_mtx;	/* Mutex for hpts */
	struct timeval p_mysleep;	/* Our min sleep time */
	uint64_t syscall_cnt;
	uint64_t sleeping;	/* What the actual sleep was (if sleeping) */
	uint16_t p_hpts_active; /* Flag that says hpts is awake  */
	uint8_t p_wheel_complete; /* have we completed the wheel arc walk? */
	uint32_t p_curtick;	/* Tick in 10 us the hpts is going to */
	uint32_t p_runningslot; /* Current tick we are at if we are running */
	uint32_t p_prev_slot;	/* Previous slot we were on */
	uint32_t p_cur_slot;	/* Current slot in wheel hpts is draining */
	uint32_t p_nxt_slot;	/* The next slot outside the current range of
				 * slots that the hpts is running on. */
	int32_t p_on_queue_cnt;	/* Count on queue in this hpts */
	uint32_t p_lasttick;	/* Last tick before the current one */
	uint8_t p_direct_wake :1, /* boolean */
		p_on_min_sleep:1, /* boolean */
		p_hpts_wake_scheduled:1, /* boolean */
		p_avail:5;
	uint8_t p_fill[3];	  /* Fill to 32 bits */
	/* Cache line 0x40 */
	struct hptsh {
		TAILQ_HEAD(, tcpcb)	head;
		uint32_t		count;
		uint32_t		gencnt;
	} *p_hptss;			/* Hptsi wheel */
	uint32_t p_hpts_sleep_time;	/* Current sleep interval having a max
					 * of 255ms */
	uint32_t overidden_sleep;	/* what was overrided by min-sleep for logging */
	uint32_t saved_lasttick;	/* for logging */
	uint32_t saved_curtick;		/* for logging */
	uint32_t saved_curslot;		/* for logging */
	uint32_t saved_prev_slot;       /* for logging */
	uint32_t p_delayed_by;	/* How much were we delayed by */
	/* Cache line 0x80 */
	struct sysctl_ctx_list hpts_ctx;
	struct sysctl_oid *hpts_root;
	struct intr_event *ie;
	void *ie_cookie;
	uint16_t p_num;		/* The hpts number one per cpu */
	uint16_t p_cpu;		/* The hpts CPU */
	/* There is extra space in here */
	/* Cache line 0x100 */
	struct callout co __aligned(CACHE_LINE_SIZE);
}               __aligned(CACHE_LINE_SIZE);

static struct tcp_hptsi {
	struct cpu_group **grps;
	struct tcp_hpts_entry **rp_ent;	/* Array of hptss */
	uint32_t *cts_last_ran;
	uint32_t grp_cnt;
	uint32_t rp_num_hptss;	/* Number of hpts threads */
} tcp_pace;

MALLOC_DEFINE(M_TCPHPTS, "tcp_hpts", "TCP hpts");
#ifdef RSS
static int tcp_bind_threads = 1;
#else
static int tcp_bind_threads = 2;
#endif
static int tcp_use_irq_cpu = 0;
static uint32_t *cts_last_ran;
static int hpts_does_tp_logging = 0;

static int32_t tcp_hptsi(struct tcp_hpts_entry *hpts, int from_callout);
static void tcp_hpts_thread(void *ctx);
static void tcp_init_hptsi(void *st);

int32_t tcp_min_hptsi_time = DEFAULT_MIN_SLEEP;
static int conn_cnt_thresh = DEFAULT_CONNECTION_THESHOLD;
static int32_t dynamic_min_sleep = DYNAMIC_MIN_SLEEP;
static int32_t dynamic_max_sleep = DYNAMIC_MAX_SLEEP;


SYSCTL_NODE(_net_inet_tcp, OID_AUTO, hpts, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP Hpts controls");
SYSCTL_NODE(_net_inet_tcp_hpts, OID_AUTO, stats, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "TCP Hpts statistics");

#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)

static int32_t tcp_hpts_precision = 120;

static struct hpts_domain_info {
	int count;
	int cpu[MAXCPU];
} hpts_domains[MAXMEMDOM];

counter_u64_t hpts_hopelessly_behind;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, hopeless, CTLFLAG_RD,
    &hpts_hopelessly_behind,
    "Number of times hpts could not catch up and was behind hopelessly");

counter_u64_t hpts_loops;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, loops, CTLFLAG_RD,
    &hpts_loops, "Number of times hpts had to loop to catch up");

counter_u64_t back_tosleep;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, no_tcbsfound, CTLFLAG_RD,
    &back_tosleep, "Number of times hpts found no tcbs");

counter_u64_t combined_wheel_wrap;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, comb_wheel_wrap, CTLFLAG_RD,
    &combined_wheel_wrap, "Number of times the wheel lagged enough to have an insert see wrap");

counter_u64_t wheel_wrap;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, wheel_wrap, CTLFLAG_RD,
    &wheel_wrap, "Number of times the wheel lagged enough to have an insert see wrap");

counter_u64_t hpts_direct_call;
SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, direct_call, CTLFLAG_RD,
    &hpts_direct_call, "Number of times hpts was called by syscall/trap or other entry");

counter_u64_t hpts_wake_timeout;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, timeout_wakeup, CTLFLAG_RD,
    &hpts_wake_timeout, "Number of times hpts threads woke up via the callout expiring");

counter_u64_t hpts_direct_awakening;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, direct_awakening, CTLFLAG_RD,
    &hpts_direct_awakening, "Number of times hpts threads woke up via the callout expiring");

counter_u64_t hpts_back_tosleep;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, back_tosleep, CTLFLAG_RD,
    &hpts_back_tosleep, "Number of times hpts threads woke up via the callout expiring and went back to sleep no work");

counter_u64_t cpu_uses_flowid;
counter_u64_t cpu_uses_random;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, cpusel_flowid, CTLFLAG_RD,
    &cpu_uses_flowid, "Number of times when setting cpuid we used the flowid field");
SYSCTL_COUNTER_U64(_net_inet_tcp_hpts_stats, OID_AUTO, cpusel_random, CTLFLAG_RD,
    &cpu_uses_random, "Number of times when setting cpuid we used the a random value");

TUNABLE_INT("net.inet.tcp.bind_hptss", &tcp_bind_threads);
TUNABLE_INT("net.inet.tcp.use_irq", &tcp_use_irq_cpu);
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, bind_hptss, CTLFLAG_RD,
    &tcp_bind_threads, 2,
    "Thread Binding tunable");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, use_irq, CTLFLAG_RD,
    &tcp_use_irq_cpu, 0,
    "Use of irq CPU  tunable");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, precision, CTLFLAG_RW,
    &tcp_hpts_precision, 120,
    "Value for PRE() precision of callout");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, cnt_thresh, CTLFLAG_RW,
    &conn_cnt_thresh, 0,
    "How many connections (below) make us use the callout based mechanism");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, logging, CTLFLAG_RW,
    &hpts_does_tp_logging, 0,
    "Do we add to any tp that has logging on pacer logs");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, dyn_minsleep, CTLFLAG_RW,
    &dynamic_min_sleep, 250,
    "What is the dynamic minsleep value?");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, dyn_maxsleep, CTLFLAG_RW,
    &dynamic_max_sleep, 5000,
    "What is the dynamic maxsleep value?");

static int32_t max_pacer_loops = 10;
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, loopmax, CTLFLAG_RW,
    &max_pacer_loops, 10,
    "What is the maximum number of times the pacer will loop trying to catch up");

#define HPTS_MAX_SLEEP_ALLOWED (NUM_OF_HPTSI_SLOTS/2)

static uint32_t hpts_sleep_max = HPTS_MAX_SLEEP_ALLOWED;

static int
sysctl_net_inet_tcp_hpts_max_sleep(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t new;

	new = hpts_sleep_max;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if ((new < (dynamic_min_sleep/HPTS_TICKS_PER_SLOT)) ||
		     (new > HPTS_MAX_SLEEP_ALLOWED))
			error = EINVAL;
		else
			hpts_sleep_max = new;
	}
	return (error);
}

static int
sysctl_net_inet_tcp_hpts_min_sleep(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint32_t new;

	new = tcp_min_hptsi_time;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (new < LOWEST_SLEEP_ALLOWED)
			error = EINVAL;
		else
			tcp_min_hptsi_time = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp_hpts, OID_AUTO, maxsleep,
    CTLTYPE_UINT | CTLFLAG_RW,
    &hpts_sleep_max, 0,
    &sysctl_net_inet_tcp_hpts_max_sleep, "IU",
    "Maximum time hpts will sleep in slots");

SYSCTL_PROC(_net_inet_tcp_hpts, OID_AUTO, minsleep,
    CTLTYPE_UINT | CTLFLAG_RW,
    &tcp_min_hptsi_time, 0,
    &sysctl_net_inet_tcp_hpts_min_sleep, "IU",
    "The minimum time the hpts must sleep before processing more slots");

static int ticks_indicate_more_sleep = TICKS_INDICATE_MORE_SLEEP;
static int ticks_indicate_less_sleep = TICKS_INDICATE_LESS_SLEEP;
static int tcp_hpts_no_wake_over_thresh = 1;

SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, more_sleep, CTLFLAG_RW,
    &ticks_indicate_more_sleep, 0,
    "If we only process this many or less on a timeout, we need longer sleep on the next callout");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, less_sleep, CTLFLAG_RW,
    &ticks_indicate_less_sleep, 0,
    "If we process this many or more on a timeout, we need less sleep on the next callout");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, nowake_over_thresh, CTLFLAG_RW,
    &tcp_hpts_no_wake_over_thresh, 0,
    "When we are over the threshold on the pacer do we prohibit wakeups?");

static uint16_t
hpts_random_cpu(void)
{
	uint16_t cpuid;
	uint32_t ran;

	ran = arc4random();
	cpuid = (((ran & 0xffff) % mp_ncpus) % tcp_pace.rp_num_hptss);
	return (cpuid);
}

static void
tcp_hpts_log(struct tcp_hpts_entry *hpts, struct tcpcb *tp, struct timeval *tv,
	     int slots_to_run, int idx, int from_callout)
{
	union tcp_log_stackspecific log;
	/*
	 * Unused logs are
	 * 64 bit - delRate, rttProp, bw_inuse
	 * 16 bit - cwnd_gain
	 *  8 bit - bbr_state, bbr_substate, inhpts;
	 */
	memset(&log.u_bbr, 0, sizeof(log.u_bbr));
	log.u_bbr.flex1 = hpts->p_nxt_slot;
	log.u_bbr.flex2 = hpts->p_cur_slot;
	log.u_bbr.flex3 = hpts->p_prev_slot;
	log.u_bbr.flex4 = idx;
	log.u_bbr.flex5 = hpts->p_curtick;
	log.u_bbr.flex6 = hpts->p_on_queue_cnt;
	log.u_bbr.flex7 = hpts->p_cpu;
	log.u_bbr.flex8 = (uint8_t)from_callout;
	log.u_bbr.inflight = slots_to_run;
	log.u_bbr.applimited = hpts->overidden_sleep;
	log.u_bbr.delivered = hpts->saved_curtick;
	log.u_bbr.timeStamp = tcp_tv_to_usectick(tv);
	log.u_bbr.epoch = hpts->saved_curslot;
	log.u_bbr.lt_epoch = hpts->saved_prev_slot;
	log.u_bbr.pkts_out = hpts->p_delayed_by;
	log.u_bbr.lost = hpts->p_hpts_sleep_time;
	log.u_bbr.pacing_gain = hpts->p_cpu;
	log.u_bbr.pkt_epoch = hpts->p_runningslot;
	log.u_bbr.use_lt_bw = 1;
	TCP_LOG_EVENTP(tp, NULL,
		       &tptosocket(tp)->so_rcv,
		       &tptosocket(tp)->so_snd,
		       BBR_LOG_HPTSDIAG, 0,
		       0, &log, false, tv);
}

static void
tcp_wakehpts(struct tcp_hpts_entry *hpts)
{
	HPTS_MTX_ASSERT(hpts);

	if (tcp_hpts_no_wake_over_thresh && (hpts->p_on_queue_cnt >= conn_cnt_thresh)) {
		hpts->p_direct_wake = 0;
		return;
	}
	if (hpts->p_hpts_wake_scheduled == 0) {
		hpts->p_hpts_wake_scheduled = 1;
		swi_sched(hpts->ie_cookie, 0);
	}
}

static void
hpts_timeout_swi(void *arg)
{
	struct tcp_hpts_entry *hpts;

	hpts = (struct tcp_hpts_entry *)arg;
	swi_sched(hpts->ie_cookie, 0);
}

static void
tcp_hpts_insert_internal(struct tcpcb *tp, struct tcp_hpts_entry *hpts)
{
	struct inpcb *inp = tptoinpcb(tp);
	struct hptsh *hptsh;

	INP_WLOCK_ASSERT(inp);
	HPTS_MTX_ASSERT(hpts);
	MPASS(hpts->p_cpu == tp->t_hpts_cpu);
	MPASS(!(inp->inp_flags & INP_DROPPED));

	hptsh = &hpts->p_hptss[tp->t_hpts_slot];

	if (tp->t_in_hpts == IHPTS_NONE) {
		tp->t_in_hpts = IHPTS_ONQUEUE;
		in_pcbref(inp);
	} else if (tp->t_in_hpts == IHPTS_MOVING) {
		tp->t_in_hpts = IHPTS_ONQUEUE;
	} else
		MPASS(tp->t_in_hpts == IHPTS_ONQUEUE);
	tp->t_hpts_gencnt = hptsh->gencnt;

	TAILQ_INSERT_TAIL(&hptsh->head, tp, t_hpts);
	hptsh->count++;
	hpts->p_on_queue_cnt++;
}

static struct tcp_hpts_entry *
tcp_hpts_lock(struct tcpcb *tp)
{
	struct tcp_hpts_entry *hpts;

	INP_LOCK_ASSERT(tptoinpcb(tp));

	hpts = tcp_pace.rp_ent[tp->t_hpts_cpu];
	HPTS_LOCK(hpts);

	return (hpts);
}

static void
tcp_hpts_release(struct tcpcb *tp)
{
	bool released __diagused;

	tp->t_in_hpts = IHPTS_NONE;
	released = in_pcbrele_wlocked(tptoinpcb(tp));
	MPASS(released == false);
}

/*
 * Initialize tcpcb to get ready for use with HPTS.  We will know which CPU
 * is preferred on the first incoming packet.  Before that avoid crowding
 * a single CPU with newborn connections and use a random one.
 * This initialization is normally called on a newborn tcpcb, but potentially
 * can be called once again if stack is switched.  In that case we inherit CPU
 * that the previous stack has set, be it random or not.  In extreme cases,
 * e.g. syzkaller fuzzing, a tcpcb can already be in HPTS in IHPTS_MOVING state
 * and has never received a first packet.
 */
void
tcp_hpts_init(struct tcpcb *tp)
{

	if (__predict_true(tp->t_hpts_cpu == HPTS_CPU_NONE)) {
		tp->t_hpts_cpu = hpts_random_cpu();
		MPASS(!(tp->t_flags2 & TF2_HPTS_CPU_SET));
	}
}

/*
 * Called normally with the INP_LOCKED but it
 * does not matter, the hpts lock is the key
 * but the lock order allows us to hold the
 * INP lock and then get the hpts lock.
 */
void
tcp_hpts_remove(struct tcpcb *tp)
{
	struct tcp_hpts_entry *hpts;
	struct hptsh *hptsh;

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	hpts = tcp_hpts_lock(tp);
	if (tp->t_in_hpts == IHPTS_ONQUEUE) {
		hptsh = &hpts->p_hptss[tp->t_hpts_slot];
		tp->t_hpts_request = 0;
		if (__predict_true(tp->t_hpts_gencnt == hptsh->gencnt)) {
			TAILQ_REMOVE(&hptsh->head, tp, t_hpts);
			MPASS(hptsh->count > 0);
			hptsh->count--;
			MPASS(hpts->p_on_queue_cnt > 0);
			hpts->p_on_queue_cnt--;
			tcp_hpts_release(tp);
		} else {
			/*
			 * tcp_hptsi() now owns the TAILQ head of this inp.
			 * Can't TAILQ_REMOVE, just mark it.
			 */
#ifdef INVARIANTS
			struct tcpcb *tmp;

			TAILQ_FOREACH(tmp, &hptsh->head, t_hpts)
				MPASS(tmp != tp);
#endif
			tp->t_in_hpts = IHPTS_MOVING;
			tp->t_hpts_slot = -1;
		}
	} else if (tp->t_in_hpts == IHPTS_MOVING) {
		/*
		 * Handle a special race condition:
		 * tcp_hptsi() moves inpcb to detached tailq
		 * tcp_hpts_remove() marks as IHPTS_MOVING, slot = -1
		 * tcp_hpts_insert() sets slot to a meaningful value
		 * tcp_hpts_remove() again (we are here!), then in_pcbdrop()
		 * tcp_hptsi() finds pcb with meaningful slot and INP_DROPPED
		 */
		tp->t_hpts_slot = -1;
	}
	HPTS_UNLOCK(hpts);
}

static inline int
hpts_slot(uint32_t wheel_slot, uint32_t plus)
{
	/*
	 * Given a slot on the wheel, what slot
	 * is that plus ticks out?
	 */
	KASSERT(wheel_slot < NUM_OF_HPTSI_SLOTS, ("Invalid tick %u not on wheel", wheel_slot));
	return ((wheel_slot + plus) % NUM_OF_HPTSI_SLOTS);
}

static inline int
tick_to_wheel(uint32_t cts_in_wticks)
{
	/*
	 * Given a timestamp in ticks (so by
	 * default to get it to a real time one
	 * would multiply by 10.. i.e the number
	 * of ticks in a slot) map it to our limited
	 * space wheel.
	 */
	return (cts_in_wticks % NUM_OF_HPTSI_SLOTS);
}

static inline int
hpts_slots_diff(int prev_slot, int slot_now)
{
	/*
	 * Given two slots that are someplace
	 * on our wheel. How far are they apart?
	 */
	if (slot_now > prev_slot)
		return (slot_now - prev_slot);
	else if (slot_now == prev_slot)
		/*
		 * Special case, same means we can go all of our
		 * wheel less one slot.
		 */
		return (NUM_OF_HPTSI_SLOTS - 1);
	else
		return ((NUM_OF_HPTSI_SLOTS - prev_slot) + slot_now);
}

/*
 * Given a slot on the wheel that is the current time
 * mapped to the wheel (wheel_slot), what is the maximum
 * distance forward that can be obtained without
 * wrapping past either prev_slot or running_slot
 * depending on the htps state? Also if passed
 * a uint32_t *, fill it with the slot location.
 *
 * Note if you do not give this function the current
 * time (that you think it is) mapped to the wheel slot
 * then the results will not be what you expect and
 * could lead to invalid inserts.
 */
static inline int32_t
max_slots_available(struct tcp_hpts_entry *hpts, uint32_t wheel_slot, uint32_t *target_slot)
{
	uint32_t dis_to_travel, end_slot, pacer_to_now, avail_on_wheel;

	if ((hpts->p_hpts_active == 1) &&
	    (hpts->p_wheel_complete == 0)) {
		end_slot = hpts->p_runningslot;
		/* Back up one tick */
		if (end_slot == 0)
			end_slot = NUM_OF_HPTSI_SLOTS - 1;
		else
			end_slot--;
		if (target_slot)
			*target_slot = end_slot;
	} else {
		/*
		 * For the case where we are
		 * not active, or we have
		 * completed the pass over
		 * the wheel, we can use the
		 * prev tick and subtract one from it. This puts us
		 * as far out as possible on the wheel.
		 */
		end_slot = hpts->p_prev_slot;
		if (end_slot == 0)
			end_slot = NUM_OF_HPTSI_SLOTS - 1;
		else
			end_slot--;
		if (target_slot)
			*target_slot = end_slot;
		/*
		 * Now we have close to the full wheel left minus the
		 * time it has been since the pacer went to sleep. Note
		 * that wheel_tick, passed in, should be the current time
		 * from the perspective of the caller, mapped to the wheel.
		 */
		if (hpts->p_prev_slot != wheel_slot)
			dis_to_travel = hpts_slots_diff(hpts->p_prev_slot, wheel_slot);
		else
			dis_to_travel = 1;
		/*
		 * dis_to_travel in this case is the space from when the
		 * pacer stopped (p_prev_slot) and where our wheel_slot
		 * is now. To know how many slots we can put it in we
		 * subtract from the wheel size. We would not want
		 * to place something after p_prev_slot or it will
		 * get ran too soon.
		 */
		return (NUM_OF_HPTSI_SLOTS - dis_to_travel);
	}
	/*
	 * So how many slots are open between p_runningslot -> p_cur_slot
	 * that is what is currently un-available for insertion. Special
	 * case when we are at the last slot, this gets 1, so that
	 * the answer to how many slots are available is all but 1.
	 */
	if (hpts->p_runningslot == hpts->p_cur_slot)
		dis_to_travel = 1;
	else
		dis_to_travel = hpts_slots_diff(hpts->p_runningslot, hpts->p_cur_slot);
	/*
	 * How long has the pacer been running?
	 */
	if (hpts->p_cur_slot != wheel_slot) {
		/* The pacer is a bit late */
		pacer_to_now = hpts_slots_diff(hpts->p_cur_slot, wheel_slot);
	} else {
		/* The pacer is right on time, now == pacers start time */
		pacer_to_now = 0;
	}
	/*
	 * To get the number left we can insert into we simply
	 * subtract the distance the pacer has to run from how
	 * many slots there are.
	 */
	avail_on_wheel = NUM_OF_HPTSI_SLOTS - dis_to_travel;
	/*
	 * Now how many of those we will eat due to the pacer's
	 * time (p_cur_slot) of start being behind the
	 * real time (wheel_slot)?
	 */
	if (avail_on_wheel <= pacer_to_now) {
		/*
		 * Wheel wrap, we can't fit on the wheel, that
		 * is unusual the system must be way overloaded!
		 * Insert into the assured slot, and return special
		 * "0".
		 */
		counter_u64_add(combined_wheel_wrap, 1);
		*target_slot = hpts->p_nxt_slot;
		return (0);
	} else {
		/*
		 * We know how many slots are open
		 * on the wheel (the reverse of what
		 * is left to run. Take away the time
		 * the pacer started to now (wheel_slot)
		 * and that tells you how many slots are
		 * open that can be inserted into that won't
		 * be touched by the pacer until later.
		 */
		return (avail_on_wheel - pacer_to_now);
	}
}


#ifdef INVARIANTS
static void
check_if_slot_would_be_wrong(struct tcp_hpts_entry *hpts, struct tcpcb *tp,
    uint32_t hptsslot, int line)
{
	/*
	 * Sanity checks for the pacer with invariants
	 * on insert.
	 */
	KASSERT(hptsslot < NUM_OF_HPTSI_SLOTS,
		("hpts:%p tp:%p slot:%d > max", hpts, tp, hptsslot));
	if ((hpts->p_hpts_active) &&
	    (hpts->p_wheel_complete == 0)) {
		/*
		 * If the pacer is processing a arc
		 * of the wheel, we need to make
		 * sure we are not inserting within
		 * that arc.
		 */
		int distance, yet_to_run;

		distance = hpts_slots_diff(hpts->p_runningslot, hptsslot);
		if (hpts->p_runningslot != hpts->p_cur_slot)
			yet_to_run = hpts_slots_diff(hpts->p_runningslot, hpts->p_cur_slot);
		else
			yet_to_run = 0;	/* processing last slot */
		KASSERT(yet_to_run <= distance, ("hpts:%p tp:%p slot:%d "
		    "distance:%d yet_to_run:%d rs:%d cs:%d", hpts, tp,
		    hptsslot, distance, yet_to_run, hpts->p_runningslot,
		    hpts->p_cur_slot));
	}
}
#endif

uint32_t
tcp_hpts_insert_diag(struct tcpcb *tp, uint32_t slot, int32_t line, struct hpts_diag *diag)
{
	struct tcp_hpts_entry *hpts;
	struct timeval tv;
	uint32_t slot_on, wheel_cts, last_slot, need_new_to = 0;
	int32_t wheel_slot, maxslots;
	bool need_wakeup = false;

	INP_WLOCK_ASSERT(tptoinpcb(tp));
	MPASS(!(tptoinpcb(tp)->inp_flags & INP_DROPPED));
	MPASS(!tcp_in_hpts(tp));

	/*
	 * We now return the next-slot the hpts will be on, beyond its
	 * current run (if up) or where it was when it stopped if it is
	 * sleeping.
	 */
	hpts = tcp_hpts_lock(tp);
	microuptime(&tv);
	if (diag) {
		memset(diag, 0, sizeof(struct hpts_diag));
		diag->p_hpts_active = hpts->p_hpts_active;
		diag->p_prev_slot = hpts->p_prev_slot;
		diag->p_runningslot = hpts->p_runningslot;
		diag->p_nxt_slot = hpts->p_nxt_slot;
		diag->p_cur_slot = hpts->p_cur_slot;
		diag->p_curtick = hpts->p_curtick;
		diag->p_lasttick = hpts->p_lasttick;
		diag->slot_req = slot;
		diag->p_on_min_sleep = hpts->p_on_min_sleep;
		diag->hpts_sleep_time = hpts->p_hpts_sleep_time;
	}
	if (slot == 0) {
		/* Ok we need to set it on the hpts in the current slot */
		tp->t_hpts_request = 0;
		if ((hpts->p_hpts_active == 0) || (hpts->p_wheel_complete)) {
			/*
			 * A sleeping hpts we want in next slot to run
			 * note that in this state p_prev_slot == p_cur_slot
			 */
			tp->t_hpts_slot = hpts_slot(hpts->p_prev_slot, 1);
			if ((hpts->p_on_min_sleep == 0) &&
			    (hpts->p_hpts_active == 0))
				need_wakeup = true;
		} else
			tp->t_hpts_slot = hpts->p_runningslot;
		if (__predict_true(tp->t_in_hpts != IHPTS_MOVING))
			tcp_hpts_insert_internal(tp, hpts);
		if (need_wakeup) {
			/*
			 * Activate the hpts if it is sleeping and its
			 * timeout is not 1.
			 */
			hpts->p_direct_wake = 1;
			tcp_wakehpts(hpts);
		}
		slot_on = hpts->p_nxt_slot;
		HPTS_UNLOCK(hpts);

		return (slot_on);
	}
	/* Get the current time relative to the wheel */
	wheel_cts = tcp_tv_to_hptstick(&tv);
	/* Map it onto the wheel */
	wheel_slot = tick_to_wheel(wheel_cts);
	/* Now what's the max we can place it at? */
	maxslots = max_slots_available(hpts, wheel_slot, &last_slot);
	if (diag) {
		diag->wheel_slot = wheel_slot;
		diag->maxslots = maxslots;
		diag->wheel_cts = wheel_cts;
	}
	if (maxslots == 0) {
		/* The pacer is in a wheel wrap behind, yikes! */
		if (slot > 1) {
			/*
			 * Reduce by 1 to prevent a forever loop in
			 * case something else is wrong. Note this
			 * probably does not hurt because the pacer
			 * if its true is so far behind we will be
			 * > 1second late calling anyway.
			 */
			slot--;
		}
		tp->t_hpts_slot = last_slot;
		tp->t_hpts_request = slot;
	} else 	if (maxslots >= slot) {
		/* It all fits on the wheel */
		tp->t_hpts_request = 0;
		tp->t_hpts_slot = hpts_slot(wheel_slot, slot);
	} else {
		/* It does not fit */
		tp->t_hpts_request = slot - maxslots;
		tp->t_hpts_slot = last_slot;
	}
	if (diag) {
		diag->slot_remaining = tp->t_hpts_request;
		diag->inp_hptsslot = tp->t_hpts_slot;
	}
#ifdef INVARIANTS
	check_if_slot_would_be_wrong(hpts, tp, tp->t_hpts_slot, line);
#endif
	if (__predict_true(tp->t_in_hpts != IHPTS_MOVING))
		tcp_hpts_insert_internal(tp, hpts);
	if ((hpts->p_hpts_active == 0) &&
	    (tp->t_hpts_request == 0) &&
	    (hpts->p_on_min_sleep == 0)) {
		/*
		 * The hpts is sleeping and NOT on a minimum
		 * sleep time, we need to figure out where
		 * it will wake up at and if we need to reschedule
		 * its time-out.
		 */
		uint32_t have_slept, yet_to_sleep;

		/* Now do we need to restart the hpts's timer? */
		have_slept = hpts_slots_diff(hpts->p_prev_slot, wheel_slot);
		if (have_slept < hpts->p_hpts_sleep_time)
			yet_to_sleep = hpts->p_hpts_sleep_time - have_slept;
		else {
			/* We are over-due */
			yet_to_sleep = 0;
			need_wakeup = 1;
		}
		if (diag) {
			diag->have_slept = have_slept;
			diag->yet_to_sleep = yet_to_sleep;
		}
		if (yet_to_sleep &&
		    (yet_to_sleep > slot)) {
			/*
			 * We need to reschedule the hpts's time-out.
			 */
			hpts->p_hpts_sleep_time = slot;
			need_new_to = slot * HPTS_TICKS_PER_SLOT;
		}
	}
	/*
	 * Now how far is the hpts sleeping to? if active is 1, its
	 * up and ticking we do nothing, otherwise we may need to
	 * reschedule its callout if need_new_to is set from above.
	 */
	if (need_wakeup) {
		hpts->p_direct_wake = 1;
		tcp_wakehpts(hpts);
		if (diag) {
			diag->need_new_to = 0;
			diag->co_ret = 0xffff0000;
		}
	} else if (need_new_to) {
		int32_t co_ret;
		struct timeval tv;
		sbintime_t sb;

		tv.tv_sec = 0;
		tv.tv_usec = 0;
		while (need_new_to > HPTS_USEC_IN_SEC) {
			tv.tv_sec++;
			need_new_to -= HPTS_USEC_IN_SEC;
		}
		tv.tv_usec = need_new_to;
		sb = tvtosbt(tv);
		co_ret = callout_reset_sbt_on(&hpts->co, sb, 0,
					      hpts_timeout_swi, hpts, hpts->p_cpu,
					      (C_DIRECT_EXEC | C_PREL(tcp_hpts_precision)));
		if (diag) {
			diag->need_new_to = need_new_to;
			diag->co_ret = co_ret;
		}
	}
	slot_on = hpts->p_nxt_slot;
	HPTS_UNLOCK(hpts);

	return (slot_on);
}

static uint16_t
hpts_cpuid(struct tcpcb *tp, int *failed)
{
	struct inpcb *inp = tptoinpcb(tp);
	u_int cpuid;
#ifdef NUMA
	struct hpts_domain_info *di;
#endif

	*failed = 0;
	if (tp->t_flags2 & TF2_HPTS_CPU_SET) {
		return (tp->t_hpts_cpu);
	}
	/*
	 * If we are using the irq cpu set by LRO or
	 * the driver then it overrides all other domains.
	 */
	if (tcp_use_irq_cpu) {
		if (tp->t_lro_cpu == HPTS_CPU_NONE) {
			*failed = 1;
			return (0);
		}
		return (tp->t_lro_cpu);
	}
	/* If one is set the other must be the same */
#ifdef RSS
	cpuid = rss_hash2cpuid(inp->inp_flowid, inp->inp_flowtype);
	if (cpuid == NETISR_CPUID_NONE)
		return (hpts_random_cpu());
	else
		return (cpuid);
#endif
	/*
	 * We don't have a flowid -> cpuid mapping, so cheat and just map
	 * unknown cpuids to curcpu.  Not the best, but apparently better
	 * than defaulting to swi 0.
	 */
	if (inp->inp_flowtype == M_HASHTYPE_NONE) {
		counter_u64_add(cpu_uses_random, 1);
		return (hpts_random_cpu());
	}
	/*
	 * Hash to a thread based on the flowid.  If we are using numa,
	 * then restrict the hash to the numa domain where the inp lives.
	 */

#ifdef NUMA
	if ((vm_ndomains == 1) ||
	    (inp->inp_numa_domain == M_NODOM)) {
#endif
		cpuid = inp->inp_flowid % mp_ncpus;
#ifdef NUMA
	} else {
		/* Hash into the cpu's that use that domain */
		di = &hpts_domains[inp->inp_numa_domain];
		cpuid = di->cpu[inp->inp_flowid % di->count];
	}
#endif
	counter_u64_add(cpu_uses_flowid, 1);
	return (cpuid);
}

static void
tcp_hpts_set_max_sleep(struct tcp_hpts_entry *hpts, int wrap_loop_cnt)
{
	uint32_t t = 0, i;

	if ((hpts->p_on_queue_cnt) && (wrap_loop_cnt < 2)) {
		/*
		 * Find next slot that is occupied and use that to
		 * be the sleep time.
		 */
		for (i = 0, t = hpts_slot(hpts->p_cur_slot, 1); i < NUM_OF_HPTSI_SLOTS; i++) {
			if (TAILQ_EMPTY(&hpts->p_hptss[t].head) == 0) {
				break;
			}
			t = (t + 1) % NUM_OF_HPTSI_SLOTS;
		}
		KASSERT((i != NUM_OF_HPTSI_SLOTS), ("Hpts:%p cnt:%d but none found", hpts, hpts->p_on_queue_cnt));
		hpts->p_hpts_sleep_time = min((i + 1), hpts_sleep_max);
	} else {
		/* No one on the wheel sleep for all but 400 slots or sleep max  */
		hpts->p_hpts_sleep_time = hpts_sleep_max;
	}
}

static int32_t
tcp_hptsi(struct tcp_hpts_entry *hpts, int from_callout)
{
	struct tcpcb *tp;
	struct timeval tv;
	int32_t slots_to_run, i, error;
	int32_t loop_cnt = 0;
	int32_t did_prefetch = 0;
	int32_t prefetch_tp = 0;
	int32_t wrap_loop_cnt = 0;
	int32_t slot_pos_of_endpoint = 0;
	int32_t orig_exit_slot;
	int8_t completed_measure = 0, seen_endpoint = 0;

	HPTS_MTX_ASSERT(hpts);
	NET_EPOCH_ASSERT();
	/* record previous info for any logging */
	hpts->saved_lasttick = hpts->p_lasttick;
	hpts->saved_curtick = hpts->p_curtick;
	hpts->saved_curslot = hpts->p_cur_slot;
	hpts->saved_prev_slot = hpts->p_prev_slot;

	hpts->p_lasttick = hpts->p_curtick;
	hpts->p_curtick = tcp_gethptstick(&tv);
	cts_last_ran[hpts->p_num] = tcp_tv_to_usectick(&tv);
	orig_exit_slot = hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
	if ((hpts->p_on_queue_cnt == 0) ||
	    (hpts->p_lasttick == hpts->p_curtick)) {
		/*
		 * No time has yet passed,
		 * or nothing to do.
		 */
		hpts->p_prev_slot = hpts->p_cur_slot;
		hpts->p_lasttick = hpts->p_curtick;
		goto no_run;
	}
again:
	hpts->p_wheel_complete = 0;
	HPTS_MTX_ASSERT(hpts);
	slots_to_run = hpts_slots_diff(hpts->p_prev_slot, hpts->p_cur_slot);
	if (((hpts->p_curtick - hpts->p_lasttick) >
	     ((NUM_OF_HPTSI_SLOTS-1) * HPTS_TICKS_PER_SLOT)) &&
	    (hpts->p_on_queue_cnt != 0)) {
		/*
		 * Wheel wrap is occuring, basically we
		 * are behind and the distance between
		 * run's has spread so much it has exceeded
		 * the time on the wheel (1.024 seconds). This
		 * is ugly and should NOT be happening. We
		 * need to run the entire wheel. We last processed
		 * p_prev_slot, so that needs to be the last slot
		 * we run. The next slot after that should be our
		 * reserved first slot for new, and then starts
		 * the running position. Now the problem is the
		 * reserved "not to yet" place does not exist
		 * and there may be inp's in there that need
		 * running. We can merge those into the
		 * first slot at the head.
		 */
		wrap_loop_cnt++;
		hpts->p_nxt_slot = hpts_slot(hpts->p_prev_slot, 1);
		hpts->p_runningslot = hpts_slot(hpts->p_prev_slot, 2);
		/*
		 * Adjust p_cur_slot to be where we are starting from
		 * hopefully we will catch up (fat chance if something
		 * is broken this bad :( )
		 */
		hpts->p_cur_slot = hpts->p_prev_slot;
		/*
		 * The next slot has guys to run too, and that would
		 * be where we would normally start, lets move them into
		 * the next slot (p_prev_slot + 2) so that we will
		 * run them, the extra 10usecs of late (by being
		 * put behind) does not really matter in this situation.
		 */
		TAILQ_FOREACH(tp, &hpts->p_hptss[hpts->p_nxt_slot].head,
		    t_hpts) {
			MPASS(tp->t_hpts_slot == hpts->p_nxt_slot);
			MPASS(tp->t_hpts_gencnt ==
			    hpts->p_hptss[hpts->p_nxt_slot].gencnt);
			MPASS(tp->t_in_hpts == IHPTS_ONQUEUE);

			/*
			 * Update gencnt and nextslot accordingly to match
			 * the new location. This is safe since it takes both
			 * the INP lock and the pacer mutex to change the
			 * t_hptsslot and t_hpts_gencnt.
			 */
			tp->t_hpts_gencnt =
			    hpts->p_hptss[hpts->p_runningslot].gencnt;
			tp->t_hpts_slot = hpts->p_runningslot;
		}
		TAILQ_CONCAT(&hpts->p_hptss[hpts->p_runningslot].head,
		    &hpts->p_hptss[hpts->p_nxt_slot].head, t_hpts);
		hpts->p_hptss[hpts->p_runningslot].count +=
		    hpts->p_hptss[hpts->p_nxt_slot].count;
		hpts->p_hptss[hpts->p_nxt_slot].count = 0;
		hpts->p_hptss[hpts->p_nxt_slot].gencnt++;
		slots_to_run = NUM_OF_HPTSI_SLOTS - 1;
		counter_u64_add(wheel_wrap, 1);
	} else {
		/*
		 * Nxt slot is always one after p_runningslot though
		 * its not used usually unless we are doing wheel wrap.
		 */
		hpts->p_nxt_slot = hpts->p_prev_slot;
		hpts->p_runningslot = hpts_slot(hpts->p_prev_slot, 1);
	}
	if (hpts->p_on_queue_cnt == 0) {
		goto no_one;
	}
	for (i = 0; i < slots_to_run; i++) {
		struct tcpcb *tp, *ntp;
		TAILQ_HEAD(, tcpcb) head = TAILQ_HEAD_INITIALIZER(head);
		struct hptsh *hptsh;
		uint32_t runningslot;

		/*
		 * Calculate our delay, if there are no extra ticks there
		 * was not any (i.e. if slots_to_run == 1, no delay).
		 */
		hpts->p_delayed_by = (slots_to_run - (i + 1)) *
		    HPTS_TICKS_PER_SLOT;

		runningslot = hpts->p_runningslot;
		hptsh = &hpts->p_hptss[runningslot];
		TAILQ_SWAP(&head, &hptsh->head, tcpcb, t_hpts);
		hpts->p_on_queue_cnt -= hptsh->count;
		hptsh->count = 0;
		hptsh->gencnt++;

		HPTS_UNLOCK(hpts);

		TAILQ_FOREACH_SAFE(tp, &head, t_hpts, ntp) {
			struct inpcb *inp = tptoinpcb(tp);
			bool set_cpu;

			if (ntp != NULL) {
				/*
				 * If we have a next tcpcb, see if we can
				 * prefetch it. Note this may seem
				 * "risky" since we have no locks (other
				 * than the previous inp) and there no
				 * assurance that ntp was not pulled while
				 * we were processing tp and freed. If this
				 * occurred it could mean that either:
				 *
				 * a) Its NULL (which is fine we won't go
				 * here) <or> b) Its valid (which is cool we
				 * will prefetch it) <or> c) The inp got
				 * freed back to the slab which was
				 * reallocated. Then the piece of memory was
				 * re-used and something else (not an
				 * address) is in inp_ppcb. If that occurs
				 * we don't crash, but take a TLB shootdown
				 * performance hit (same as if it was NULL
				 * and we tried to pre-fetch it).
				 *
				 * Considering that the likelyhood of <c> is
				 * quite rare we will take a risk on doing
				 * this. If performance drops after testing
				 * we can always take this out. NB: the
				 * kern_prefetch on amd64 actually has
				 * protection against a bad address now via
				 * the DMAP_() tests. This will prevent the
				 * TLB hit, and instead if <c> occurs just
				 * cause us to load cache with a useless
				 * address (to us).
				 *
				 * XXXGL: this comment and the prefetch action
				 * could be outdated after tp == inp change.
				 */
				kern_prefetch(ntp, &prefetch_tp);
				prefetch_tp = 1;
			}

			/* For debugging */
			if (seen_endpoint == 0) {
				seen_endpoint = 1;
				orig_exit_slot = slot_pos_of_endpoint =
				    runningslot;
			} else if (completed_measure == 0) {
				/* Record the new position */
				orig_exit_slot = runningslot;
			}

			INP_WLOCK(inp);
			if ((tp->t_flags2 & TF2_HPTS_CPU_SET) == 0) {
				set_cpu = true;
			} else {
				set_cpu = false;
			}

			if (__predict_false(tp->t_in_hpts == IHPTS_MOVING)) {
				if (tp->t_hpts_slot == -1) {
					tp->t_in_hpts = IHPTS_NONE;
					if (in_pcbrele_wlocked(inp) == false)
						INP_WUNLOCK(inp);
				} else {
					HPTS_LOCK(hpts);
					tcp_hpts_insert_internal(tp, hpts);
					HPTS_UNLOCK(hpts);
					INP_WUNLOCK(inp);
				}
				continue;
			}

			MPASS(tp->t_in_hpts == IHPTS_ONQUEUE);
			MPASS(!(inp->inp_flags & INP_DROPPED));
			KASSERT(runningslot == tp->t_hpts_slot,
				("Hpts:%p inp:%p slot mis-aligned %u vs %u",
				 hpts, inp, runningslot, tp->t_hpts_slot));

			if (tp->t_hpts_request) {
				/*
				 * This guy is deferred out further in time
				 * then our wheel had available on it.
				 * Push him back on the wheel or run it
				 * depending.
				 */
				uint32_t maxslots, last_slot, remaining_slots;

				remaining_slots = slots_to_run - (i + 1);
				if (tp->t_hpts_request > remaining_slots) {
					HPTS_LOCK(hpts);
					/*
					 * How far out can we go?
					 */
					maxslots = max_slots_available(hpts,
					    hpts->p_cur_slot, &last_slot);
					if (maxslots >= tp->t_hpts_request) {
						/* We can place it finally to
						 * be processed.  */
						tp->t_hpts_slot = hpts_slot(
						    hpts->p_runningslot,
						    tp->t_hpts_request);
						tp->t_hpts_request = 0;
					} else {
						/* Work off some more time */
						tp->t_hpts_slot = last_slot;
						tp->t_hpts_request -=
						    maxslots;
					}
					tcp_hpts_insert_internal(tp, hpts);
					HPTS_UNLOCK(hpts);
					INP_WUNLOCK(inp);
					continue;
				}
				tp->t_hpts_request = 0;
				/* Fall through we will so do it now */
			}

			tcp_hpts_release(tp);
			if (set_cpu) {
				/*
				 * Setup so the next time we will move to
				 * the right CPU. This should be a rare
				 * event. It will sometimes happens when we
				 * are the client side (usually not the
				 * server). Somehow tcp_output() gets called
				 * before the tcp_do_segment() sets the
				 * intial state. This means the r_cpu and
				 * r_hpts_cpu is 0. We get on the hpts, and
				 * then tcp_input() gets called setting up
				 * the r_cpu to the correct value. The hpts
				 * goes off and sees the mis-match. We
				 * simply correct it here and the CPU will
				 * switch to the new hpts nextime the tcb
				 * gets added to the hpts (not this one)
				 * :-)
				 */
				tcp_set_hpts(tp);
			}
			CURVNET_SET(inp->inp_vnet);
			/* Lets do any logging that we might want to */
			if (hpts_does_tp_logging && tcp_bblogging_on(tp)) {
				tcp_hpts_log(hpts, tp, &tv, slots_to_run, i, from_callout);
			}

			if (tp->t_fb_ptr != NULL) {
				kern_prefetch(tp->t_fb_ptr, &did_prefetch);
				did_prefetch = 1;
			}
			/*
			 * We set TF2_HPTS_CALLS before any possible output.
			 * The contract with the transport is that if it cares
			 * about hpts calling it should clear the flag. That
			 * way next time it is called it will know it is hpts.
			 *
			 * We also only call tfb_do_queued_segments() <or>
			 * tcp_output().  It is expected that if segments are
			 * queued and come in that the final input mbuf will
			 * cause a call to output if it is needed.
			 */
			tp->t_flags2 |= TF2_HPTS_CALLS;
			if ((tp->t_flags2 & TF2_SUPPORTS_MBUFQ) &&
			    !STAILQ_EMPTY(&tp->t_inqueue)) {
				error = (*tp->t_fb->tfb_do_queued_segments)(tp, 0);
				if (error) {
					/* The input killed the connection */
					goto skip_pacing;
				}
			}
			error = tcp_output(tp);
			if (error < 0)
				goto skip_pacing;
			INP_WUNLOCK(inp);
		skip_pacing:
			CURVNET_RESTORE();
		}
		if (seen_endpoint) {
			/*
			 * We now have a accurate distance between
			 * slot_pos_of_endpoint <-> orig_exit_slot
			 * to tell us how late we were, orig_exit_slot
			 * is where we calculated the end of our cycle to
			 * be when we first entered.
			 */
			completed_measure = 1;
		}
		HPTS_LOCK(hpts);
		hpts->p_runningslot++;
		if (hpts->p_runningslot >= NUM_OF_HPTSI_SLOTS) {
			hpts->p_runningslot = 0;
		}
	}
no_one:
	HPTS_MTX_ASSERT(hpts);
	hpts->p_delayed_by = 0;
	/*
	 * Check to see if we took an excess amount of time and need to run
	 * more ticks (if we did not hit eno-bufs).
	 */
	hpts->p_prev_slot = hpts->p_cur_slot;
	hpts->p_lasttick = hpts->p_curtick;
	if ((from_callout == 0) || (loop_cnt > max_pacer_loops)) {
		/*
		 * Something is serious slow we have
		 * looped through processing the wheel
		 * and by the time we cleared the
		 * needs to run max_pacer_loops time
		 * we still needed to run. That means
		 * the system is hopelessly behind and
		 * can never catch up :(
		 *
		 * We will just lie to this thread
		 * and let it thing p_curtick is
		 * correct. When it next awakens
		 * it will find itself further behind.
		 */
		if (from_callout)
			counter_u64_add(hpts_hopelessly_behind, 1);
		goto no_run;
	}
	hpts->p_curtick = tcp_gethptstick(&tv);
	hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
	if (seen_endpoint == 0) {
		/* We saw no endpoint but we may be looping */
		orig_exit_slot = hpts->p_cur_slot;
	}
	if ((wrap_loop_cnt < 2) &&
	    (hpts->p_lasttick != hpts->p_curtick)) {
		counter_u64_add(hpts_loops, 1);
		loop_cnt++;
		goto again;
	}
no_run:
	cts_last_ran[hpts->p_num] = tcp_tv_to_usectick(&tv);
	/*
	 * Set flag to tell that we are done for
	 * any slot input that happens during
	 * input.
	 */
	hpts->p_wheel_complete = 1;
	/*
	 * Now did we spend too long running input and need to run more ticks?
	 * Note that if wrap_loop_cnt < 2 then we should have the conditions
	 * in the KASSERT's true. But if the wheel is behind i.e. wrap_loop_cnt
	 * is greater than 2, then the condtion most likely are *not* true.
	 * Also if we are called not from the callout, we don't run the wheel
	 * multiple times so the slots may not align either.
	 */
	KASSERT(((hpts->p_prev_slot == hpts->p_cur_slot) ||
		 (wrap_loop_cnt >= 2) || (from_callout == 0)),
		("H:%p p_prev_slot:%u not equal to p_cur_slot:%u", hpts,
		 hpts->p_prev_slot, hpts->p_cur_slot));
	KASSERT(((hpts->p_lasttick == hpts->p_curtick)
		 || (wrap_loop_cnt >= 2) || (from_callout == 0)),
		("H:%p p_lasttick:%u not equal to p_curtick:%u", hpts,
		 hpts->p_lasttick, hpts->p_curtick));
	if (from_callout && (hpts->p_lasttick != hpts->p_curtick)) {
		hpts->p_curtick = tcp_gethptstick(&tv);
		counter_u64_add(hpts_loops, 1);
		hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
		goto again;
	}

	if (from_callout){
		tcp_hpts_set_max_sleep(hpts, wrap_loop_cnt);
	}
	if (seen_endpoint)
		return(hpts_slots_diff(slot_pos_of_endpoint, orig_exit_slot));
	else
		return (0);
}

void
__tcp_set_hpts(struct tcpcb *tp, int32_t line)
{
	struct tcp_hpts_entry *hpts;
	int failed;

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	hpts = tcp_hpts_lock(tp);
	if (tp->t_in_hpts == IHPTS_NONE && !(tp->t_flags2 & TF2_HPTS_CPU_SET)) {
		tp->t_hpts_cpu = hpts_cpuid(tp, &failed);
		if (failed == 0)
			tp->t_flags2 |= TF2_HPTS_CPU_SET;
	}
	mtx_unlock(&hpts->p_mtx);
}

static void
__tcp_run_hpts(struct tcp_hpts_entry *hpts)
{
	int ticks_ran;

	if (hpts->p_hpts_active) {
		/* Already active */
		return;
	}
	if (mtx_trylock(&hpts->p_mtx) == 0) {
		/* Someone else got the lock */
		return;
	}
	if (hpts->p_hpts_active)
		goto out_with_mtx;
	hpts->syscall_cnt++;
	counter_u64_add(hpts_direct_call, 1);
	hpts->p_hpts_active = 1;
	ticks_ran = tcp_hptsi(hpts, 0);
	/* We may want to adjust the sleep values here */
	if (hpts->p_on_queue_cnt >= conn_cnt_thresh) {
		if (ticks_ran > ticks_indicate_less_sleep) {
			struct timeval tv;
			sbintime_t sb;

			hpts->p_mysleep.tv_usec /= 2;
			if (hpts->p_mysleep.tv_usec < dynamic_min_sleep)
				hpts->p_mysleep.tv_usec = dynamic_min_sleep;
			/* Reschedule with new to value */
			tcp_hpts_set_max_sleep(hpts, 0);
			tv.tv_usec = hpts->p_hpts_sleep_time * HPTS_TICKS_PER_SLOT;
			/* Validate its in the right ranges */
			if (tv.tv_usec < hpts->p_mysleep.tv_usec) {
				hpts->overidden_sleep = tv.tv_usec;
				tv.tv_usec = hpts->p_mysleep.tv_usec;
			} else if (tv.tv_usec > dynamic_max_sleep) {
				/* Lets not let sleep get above this value */
				hpts->overidden_sleep = tv.tv_usec;
				tv.tv_usec = dynamic_max_sleep;
			}
			/*
			 * In this mode the timer is a backstop to
			 * all the userret/lro_flushes so we use
			 * the dynamic value and set the on_min_sleep
			 * flag so we will not be awoken.
			 */
			sb = tvtosbt(tv);
			/* Store off to make visible the actual sleep time */
			hpts->sleeping = tv.tv_usec;
			callout_reset_sbt_on(&hpts->co, sb, 0,
					     hpts_timeout_swi, hpts, hpts->p_cpu,
					     (C_DIRECT_EXEC | C_PREL(tcp_hpts_precision)));
		} else if (ticks_ran < ticks_indicate_more_sleep) {
			/* For the further sleep, don't reschedule  hpts */
			hpts->p_mysleep.tv_usec *= 2;
			if (hpts->p_mysleep.tv_usec > dynamic_max_sleep)
				hpts->p_mysleep.tv_usec = dynamic_max_sleep;
		}
		hpts->p_on_min_sleep = 1;
	}
	hpts->p_hpts_active = 0;
out_with_mtx:
	HPTS_MTX_ASSERT(hpts);
	mtx_unlock(&hpts->p_mtx);
}

static struct tcp_hpts_entry *
tcp_choose_hpts_to_run(void)
{
	int i, oldest_idx, start, end;
	uint32_t cts, time_since_ran, calc;

	cts = tcp_get_usecs(NULL);
	time_since_ran = 0;
	/* Default is all one group */
	start = 0;
	end = tcp_pace.rp_num_hptss;
	/*
	 * If we have more than one L3 group figure out which one
	 * this CPU is in.
	 */
	if (tcp_pace.grp_cnt > 1) {
		for (i = 0; i < tcp_pace.grp_cnt; i++) {
			if (CPU_ISSET(curcpu, &tcp_pace.grps[i]->cg_mask)) {
				start = tcp_pace.grps[i]->cg_first;
				end = (tcp_pace.grps[i]->cg_last + 1);
				break;
			}
		}
	}
	oldest_idx = -1;
	for (i = start; i < end; i++) {
		if (TSTMP_GT(cts, cts_last_ran[i]))
			calc = cts - cts_last_ran[i];
		else
			calc = 0;
		if (calc > time_since_ran) {
			oldest_idx = i;
			time_since_ran = calc;
		}
	}
	if (oldest_idx >= 0)
		return(tcp_pace.rp_ent[oldest_idx]);
	else
		return(tcp_pace.rp_ent[(curcpu % tcp_pace.rp_num_hptss)]);
}


void
tcp_run_hpts(void)
{
	static struct tcp_hpts_entry *hpts;
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	hpts = tcp_choose_hpts_to_run();
	__tcp_run_hpts(hpts);
	NET_EPOCH_EXIT(et);
}


static void
tcp_hpts_thread(void *ctx)
{
	struct tcp_hpts_entry *hpts;
	struct epoch_tracker et;
	struct timeval tv;
	sbintime_t sb;
	int ticks_ran;

	hpts = (struct tcp_hpts_entry *)ctx;
	mtx_lock(&hpts->p_mtx);
	if (hpts->p_direct_wake) {
		/* Signaled by input or output with low occupancy count. */
		callout_stop(&hpts->co);
		counter_u64_add(hpts_direct_awakening, 1);
	} else {
		/* Timed out, the normal case. */
		counter_u64_add(hpts_wake_timeout, 1);
		if (callout_pending(&hpts->co) ||
		    !callout_active(&hpts->co)) {
			mtx_unlock(&hpts->p_mtx);
			return;
		}
	}
	callout_deactivate(&hpts->co);
	hpts->p_hpts_wake_scheduled = 0;
	NET_EPOCH_ENTER(et);
	if (hpts->p_hpts_active) {
		/*
		 * We are active already. This means that a syscall
		 * trap or LRO is running in behalf of hpts. In that case
		 * we need to double our timeout since there seems to be
		 * enough activity in the system that we don't need to
		 * run as often (if we were not directly woken).
		 */
		if (hpts->p_direct_wake == 0) {
			counter_u64_add(hpts_back_tosleep, 1);
			if (hpts->p_on_queue_cnt >= conn_cnt_thresh) {
				hpts->p_mysleep.tv_usec *= 2;
				if (hpts->p_mysleep.tv_usec > dynamic_max_sleep)
					hpts->p_mysleep.tv_usec = dynamic_max_sleep;
				tv.tv_usec = hpts->p_mysleep.tv_usec;
				hpts->p_on_min_sleep = 1;
			} else {
				/*
				 * Here we have low count on the wheel, but
				 * somehow we still collided with one of the
				 * connections. Lets go back to sleep for a
				 * min sleep time, but clear the flag so we
				 * can be awoken by insert.
				 */
				hpts->p_on_min_sleep = 0;
				tv.tv_usec = tcp_min_hptsi_time;
			}
		} else {
			/*
			 * Directly woken most likely to reset the
			 * callout time.
			 */
			tv.tv_sec = 0;
			tv.tv_usec = hpts->p_mysleep.tv_usec;
		}
		goto back_to_sleep;
	}
	hpts->sleeping = 0;
	hpts->p_hpts_active = 1;
	ticks_ran = tcp_hptsi(hpts, 1);
	tv.tv_sec = 0;
	tv.tv_usec = hpts->p_hpts_sleep_time * HPTS_TICKS_PER_SLOT;
	if (hpts->p_on_queue_cnt >= conn_cnt_thresh) {
		if(hpts->p_direct_wake == 0) {
			/*
			 * Only adjust sleep time if we were
			 * called from the callout i.e. direct_wake == 0.
			 */
			if (ticks_ran < ticks_indicate_more_sleep) {
				hpts->p_mysleep.tv_usec *= 2;
				if (hpts->p_mysleep.tv_usec > dynamic_max_sleep)
					hpts->p_mysleep.tv_usec = dynamic_max_sleep;
			} else if (ticks_ran > ticks_indicate_less_sleep) {
				hpts->p_mysleep.tv_usec /= 2;
				if (hpts->p_mysleep.tv_usec < dynamic_min_sleep)
					hpts->p_mysleep.tv_usec = dynamic_min_sleep;
			}
		}
		if (tv.tv_usec < hpts->p_mysleep.tv_usec) {
			hpts->overidden_sleep = tv.tv_usec;
			tv.tv_usec = hpts->p_mysleep.tv_usec;
		} else if (tv.tv_usec > dynamic_max_sleep) {
			/* Lets not let sleep get above this value */
			hpts->overidden_sleep = tv.tv_usec;
			tv.tv_usec = dynamic_max_sleep;
		}
		/*
		 * In this mode the timer is a backstop to
		 * all the userret/lro_flushes so we use
		 * the dynamic value and set the on_min_sleep
		 * flag so we will not be awoken.
		 */
		hpts->p_on_min_sleep = 1;
	} else if (hpts->p_on_queue_cnt == 0)  {
		/*
		 * No one on the wheel, please wake us up
		 * if you insert on the wheel.
		 */
		hpts->p_on_min_sleep = 0;
		hpts->overidden_sleep = 0;
	} else {
		/*
		 * We hit here when we have a low number of
		 * clients on the wheel (our else clause).
		 * We may need to go on min sleep, if we set
		 * the flag we will not be awoken if someone
		 * is inserted ahead of us. Clearing the flag
		 * means we can be awoken. This is "old mode"
		 * where the timer is what runs hpts mainly.
		 */
		if (tv.tv_usec < tcp_min_hptsi_time) {
			/*
			 * Yes on min sleep, which means
			 * we cannot be awoken.
			 */
			hpts->overidden_sleep = tv.tv_usec;
			tv.tv_usec = tcp_min_hptsi_time;
			hpts->p_on_min_sleep = 1;
		} else {
			/* Clear the min sleep flag */
			hpts->overidden_sleep = 0;
			hpts->p_on_min_sleep = 0;
		}
	}
	HPTS_MTX_ASSERT(hpts);
	hpts->p_hpts_active = 0;
back_to_sleep:
	hpts->p_direct_wake = 0;
	sb = tvtosbt(tv);
	/* Store off to make visible the actual sleep time */
	hpts->sleeping = tv.tv_usec;
	callout_reset_sbt_on(&hpts->co, sb, 0,
			     hpts_timeout_swi, hpts, hpts->p_cpu,
			     (C_DIRECT_EXEC | C_PREL(tcp_hpts_precision)));
	NET_EPOCH_EXIT(et);
	mtx_unlock(&hpts->p_mtx);
}

#undef	timersub

static int32_t
hpts_count_level(struct cpu_group *cg)
{
	int32_t count_l3, i;

	count_l3 = 0;
	if (cg->cg_level == CG_SHARE_L3)
		count_l3++;
	/* Walk all the children looking for L3 */
	for (i = 0; i < cg->cg_children; i++) {
		count_l3 += hpts_count_level(&cg->cg_child[i]);
	}
	return (count_l3);
}

static void
hpts_gather_grps(struct cpu_group **grps, int32_t *at, int32_t max, struct cpu_group *cg)
{
	int32_t idx, i;

	idx = *at;
	if (cg->cg_level == CG_SHARE_L3) {
		grps[idx] = cg;
		idx++;
		if (idx == max) {
			*at = idx;
			return;
		}
	}
	*at = idx;
	/* Walk all the children looking for L3 */
	for (i = 0; i < cg->cg_children; i++) {
		hpts_gather_grps(grps, at, max, &cg->cg_child[i]);
	}
}

static void
tcp_init_hptsi(void *st)
{
	struct cpu_group *cpu_top;
	int32_t error __diagused;
	int32_t i, j, bound = 0, created = 0;
	size_t sz, asz;
	struct timeval tv;
	sbintime_t sb;
	struct tcp_hpts_entry *hpts;
	struct pcpu *pc;
	char unit[16];
	uint32_t ncpus = mp_ncpus ? mp_ncpus : MAXCPU;
	int count, domain;

#ifdef SMP
	cpu_top = smp_topo();
#else
	cpu_top = NULL;
#endif
	tcp_pace.rp_num_hptss = ncpus;
	hpts_hopelessly_behind = counter_u64_alloc(M_WAITOK);
	hpts_loops = counter_u64_alloc(M_WAITOK);
	back_tosleep = counter_u64_alloc(M_WAITOK);
	combined_wheel_wrap = counter_u64_alloc(M_WAITOK);
	wheel_wrap = counter_u64_alloc(M_WAITOK);
	hpts_wake_timeout = counter_u64_alloc(M_WAITOK);
	hpts_direct_awakening = counter_u64_alloc(M_WAITOK);
	hpts_back_tosleep = counter_u64_alloc(M_WAITOK);
	hpts_direct_call = counter_u64_alloc(M_WAITOK);
	cpu_uses_flowid = counter_u64_alloc(M_WAITOK);
	cpu_uses_random = counter_u64_alloc(M_WAITOK);

	sz = (tcp_pace.rp_num_hptss * sizeof(struct tcp_hpts_entry *));
	tcp_pace.rp_ent = malloc(sz, M_TCPHPTS, M_WAITOK | M_ZERO);
	sz = (sizeof(uint32_t) * tcp_pace.rp_num_hptss);
	cts_last_ran = malloc(sz, M_TCPHPTS, M_WAITOK);
	tcp_pace.grp_cnt = 0;
	if (cpu_top == NULL) {
		tcp_pace.grp_cnt = 1;
	} else {
		/* Find out how many cache level 3 domains we have */
		count = 0;
		tcp_pace.grp_cnt = hpts_count_level(cpu_top);
		if (tcp_pace.grp_cnt == 0) {
			tcp_pace.grp_cnt = 1;
		}
		sz = (tcp_pace.grp_cnt * sizeof(struct cpu_group *));
		tcp_pace.grps = malloc(sz, M_TCPHPTS, M_WAITOK);
		/* Now populate the groups */
		if (tcp_pace.grp_cnt == 1) {
			/*
			 * All we need is the top level all cpu's are in
			 * the same cache so when we use grp[0]->cg_mask
			 * with the cg_first <-> cg_last it will include
			 * all cpu's in it. The level here is probably
			 * zero which is ok.
			 */
			tcp_pace.grps[0] = cpu_top;
		} else {
			/*
			 * Here we must find all the level three cache domains
			 * and setup our pointers to them.
			 */
			count = 0;
			hpts_gather_grps(tcp_pace.grps, &count, tcp_pace.grp_cnt, cpu_top);
		}
	}
	asz = sizeof(struct hptsh) * NUM_OF_HPTSI_SLOTS;
	for (i = 0; i < tcp_pace.rp_num_hptss; i++) {
		tcp_pace.rp_ent[i] = malloc(sizeof(struct tcp_hpts_entry),
		    M_TCPHPTS, M_WAITOK | M_ZERO);
		tcp_pace.rp_ent[i]->p_hptss = malloc(asz, M_TCPHPTS, M_WAITOK);
		hpts = tcp_pace.rp_ent[i];
		/*
		 * Init all the hpts structures that are not specifically
		 * zero'd by the allocations. Also lets attach them to the
		 * appropriate sysctl block as well.
		 */
		mtx_init(&hpts->p_mtx, "tcp_hpts_lck",
		    "hpts", MTX_DEF | MTX_DUPOK);
		for (j = 0; j < NUM_OF_HPTSI_SLOTS; j++) {
			TAILQ_INIT(&hpts->p_hptss[j].head);
			hpts->p_hptss[j].count = 0;
			hpts->p_hptss[j].gencnt = 0;
		}
		sysctl_ctx_init(&hpts->hpts_ctx);
		sprintf(unit, "%d", i);
		hpts->hpts_root = SYSCTL_ADD_NODE(&hpts->hpts_ctx,
		    SYSCTL_STATIC_CHILDREN(_net_inet_tcp_hpts),
		    OID_AUTO,
		    unit,
		    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
		    "");
		SYSCTL_ADD_INT(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "out_qcnt", CTLFLAG_RD,
		    &hpts->p_on_queue_cnt, 0,
		    "Count TCB's awaiting output processing");
		SYSCTL_ADD_U16(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "active", CTLFLAG_RD,
		    &hpts->p_hpts_active, 0,
		    "Is the hpts active");
		SYSCTL_ADD_UINT(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "curslot", CTLFLAG_RD,
		    &hpts->p_cur_slot, 0,
		    "What the current running pacers goal");
		SYSCTL_ADD_UINT(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "runtick", CTLFLAG_RD,
		    &hpts->p_runningslot, 0,
		    "What the running pacers current slot is");
		SYSCTL_ADD_UINT(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "curtick", CTLFLAG_RD,
		    &hpts->p_curtick, 0,
		    "What the running pacers last tick mapped to the wheel was");
		SYSCTL_ADD_UINT(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "lastran", CTLFLAG_RD,
		    &cts_last_ran[i], 0,
		    "The last usec tick that this hpts ran");
		SYSCTL_ADD_LONG(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "cur_min_sleep", CTLFLAG_RD,
		    &hpts->p_mysleep.tv_usec,
		    "What the running pacers is using for p_mysleep.tv_usec");
		SYSCTL_ADD_U64(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "now_sleeping", CTLFLAG_RD,
		    &hpts->sleeping, 0,
		    "What the running pacers is actually sleeping for");
		SYSCTL_ADD_U64(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "syscall_cnt", CTLFLAG_RD,
		    &hpts->syscall_cnt, 0,
		    "How many times we had syscalls on this hpts");

		hpts->p_hpts_sleep_time = hpts_sleep_max;
		hpts->p_num = i;
		hpts->p_curtick = tcp_gethptstick(&tv);
		cts_last_ran[i] = tcp_tv_to_usectick(&tv);
		hpts->p_prev_slot = hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
		hpts->p_cpu = 0xffff;
		hpts->p_nxt_slot = hpts_slot(hpts->p_cur_slot, 1);
		callout_init(&hpts->co, 1);
	}
	/* Don't try to bind to NUMA domains if we don't have any */
	if (vm_ndomains == 1 && tcp_bind_threads == 2)
		tcp_bind_threads = 0;

	/*
	 * Now lets start ithreads to handle the hptss.
	 */
	for (i = 0; i < tcp_pace.rp_num_hptss; i++) {
		hpts = tcp_pace.rp_ent[i];
		hpts->p_cpu = i;

		error = swi_add(&hpts->ie, "hpts",
		    tcp_hpts_thread, (void *)hpts,
		    SWI_NET, INTR_MPSAFE, &hpts->ie_cookie);
		KASSERT(error == 0,
			("Can't add hpts:%p i:%d err:%d",
			 hpts, i, error));
		created++;
		hpts->p_mysleep.tv_sec = 0;
		hpts->p_mysleep.tv_usec = tcp_min_hptsi_time;
		if (tcp_bind_threads == 1) {
			if (intr_event_bind(hpts->ie, i) == 0)
				bound++;
		} else if (tcp_bind_threads == 2) {
			/* Find the group for this CPU (i) and bind into it */
			for (j = 0; j < tcp_pace.grp_cnt; j++) {
				if (CPU_ISSET(i, &tcp_pace.grps[j]->cg_mask)) {
					if (intr_event_bind_ithread_cpuset(hpts->ie,
						&tcp_pace.grps[j]->cg_mask) == 0) {
						bound++;
						pc = pcpu_find(i);
						domain = pc->pc_domain;
						count = hpts_domains[domain].count;
						hpts_domains[domain].cpu[count] = i;
						hpts_domains[domain].count++;
						break;
					}
				}
			}
		}
		tv.tv_sec = 0;
		tv.tv_usec = hpts->p_hpts_sleep_time * HPTS_TICKS_PER_SLOT;
		hpts->sleeping = tv.tv_usec;
		sb = tvtosbt(tv);
		callout_reset_sbt_on(&hpts->co, sb, 0,
				     hpts_timeout_swi, hpts, hpts->p_cpu,
				     (C_DIRECT_EXEC | C_PREL(tcp_hpts_precision)));
	}
	/*
	 * If we somehow have an empty domain, fall back to choosing
	 * among all htps threads.
	 */
	for (i = 0; i < vm_ndomains; i++) {
		if (hpts_domains[i].count == 0) {
			tcp_bind_threads = 0;
			break;
		}
	}
	printf("TCP Hpts created %d swi interrupt threads and bound %d to %s\n",
	    created, bound,
	    tcp_bind_threads == 2 ? "NUMA domains" : "cpus");
#ifdef INVARIANTS
	printf("HPTS is in INVARIANT mode!!\n");
#endif
}

SYSINIT(tcphptsi, SI_SUB_SOFTINTR, SI_ORDER_ANY, tcp_init_hptsi, NULL);
MODULE_VERSION(tcphpts, 1);
