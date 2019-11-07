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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"
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
 * if (inp->inp_in_hpts)
 *    return;
 *
 * to prevent output processing until the time alotted has gone by.
 * Of course this is a bare bones example and the stack will probably
 * have more consideration then just the above.
 * 
 * Now the second function (actually two functions I guess :D)
 * the tcp_hpts system provides is the  ability to either abort 
 * a connection (later) or process input on a connection. 
 * Why would you want to do this? To keep processor locality
 * and or not have to worry about untangling any recursive
 * locks. The input function now is hooked to the new LRO
 * system as well. 
 *
 * In order to use the input redirection function the
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
 * knows how to take the input queue of packets from 
 * tp->t_in_pkts and process them digging out 
 * all the arguments, calling any bpf tap and 
 * calling into tfb_do_segment_nounlock(). The common
 * function (ctf_do_queued_segments())  requires that 
 * you have defined the tfb_do_segment_nounlock() as
 * described above.
 *
 * The second feature of the input side of hpts is the
 * dropping of a connection. This is due to the way that
 * locking may have occured on the INP_WLOCK. So if
 * a stack wants to drop a connection it calls:
 *
 *     tcp_set_inp_to_drop(tp, ETIMEDOUT)
 * 
 * To schedule the tcp_hpts system to call 
 * 
 *    tcp_drop(tp, drop_reason)
 *
 * at a future point. This is quite handy to prevent locking
 * issues when dropping connections.
 *
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

#ifdef tcpdebug
#include <netinet/tcp_debug.h>
#endif				/* tcpdebug */
#ifdef tcp_offload
#include <netinet/tcp_offload.h>
#endif

#include "opt_rss.h"

MALLOC_DEFINE(M_TCPHPTS, "tcp_hpts", "TCP hpts");
#ifdef RSS
static int tcp_bind_threads = 1;
#else
static int tcp_bind_threads = 2;
#endif
TUNABLE_INT("net.inet.tcp.bind_hptss", &tcp_bind_threads);

static struct tcp_hptsi tcp_pace;
static int hpts_does_tp_logging = 0;

static void tcp_wakehpts(struct tcp_hpts_entry *p);
static void tcp_wakeinput(struct tcp_hpts_entry *p);
static void tcp_input_data(struct tcp_hpts_entry *hpts, struct timeval *tv);
static void tcp_hptsi(struct tcp_hpts_entry *hpts);
static void tcp_hpts_thread(void *ctx);
static void tcp_init_hptsi(void *st);

int32_t tcp_min_hptsi_time = DEFAULT_MIN_SLEEP;
static int32_t tcp_hpts_callout_skip_swi = 0;

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, hpts, CTLFLAG_RW, 0, "TCP Hpts controls");

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

struct hpts_domain_info {
	int count;
	int cpu[MAXCPU];
};

struct hpts_domain_info hpts_domains[MAXMEMDOM];

SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, precision, CTLFLAG_RW,
    &tcp_hpts_precision, 120,
    "Value for PRE() precision of callout");

counter_u64_t hpts_hopelessly_behind;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts, OID_AUTO, hopeless, CTLFLAG_RD,
    &hpts_hopelessly_behind,
    "Number of times hpts could not catch up and was behind hopelessly");

counter_u64_t hpts_loops;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts, OID_AUTO, loops, CTLFLAG_RD,
    &hpts_loops, "Number of times hpts had to loop to catch up");


counter_u64_t back_tosleep;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts, OID_AUTO, no_tcbsfound, CTLFLAG_RD,
    &back_tosleep, "Number of times hpts found no tcbs");

counter_u64_t combined_wheel_wrap;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts, OID_AUTO, comb_wheel_wrap, CTLFLAG_RD,
    &combined_wheel_wrap, "Number of times the wheel lagged enough to have an insert see wrap");

counter_u64_t wheel_wrap;

SYSCTL_COUNTER_U64(_net_inet_tcp_hpts, OID_AUTO, wheel_wrap, CTLFLAG_RD,
    &wheel_wrap, "Number of times the wheel lagged enough to have an insert see wrap");

static int32_t out_ts_percision = 0;

SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, out_tspercision, CTLFLAG_RW,
    &out_ts_percision, 0,
    "Do we use a percise timestamp for every output cts");
SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, logging, CTLFLAG_RW,
    &hpts_does_tp_logging, 0,
    "Do we add to any tp that has logging on pacer logs");

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
		if ((new < (NUM_OF_HPTSI_SLOTS / 4)) ||
		    (new > HPTS_MAX_SLEEP_ALLOWED)) 
			error = EINVAL;
		else
			hpts_sleep_max = new;
	}
	return (error);
}

SYSCTL_PROC(_net_inet_tcp_hpts, OID_AUTO, maxsleep,
    CTLTYPE_UINT | CTLFLAG_RW,
    &hpts_sleep_max, 0,
    &sysctl_net_inet_tcp_hpts_max_sleep, "IU",
    "Maximum time hpts will sleep");

SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, minsleep, CTLFLAG_RW,
    &tcp_min_hptsi_time, 0,
    "The minimum time the hpts must sleep before processing more slots");

SYSCTL_INT(_net_inet_tcp_hpts, OID_AUTO, skip_swi, CTLFLAG_RW,
    &tcp_hpts_callout_skip_swi, 0,
    "Do we have the callout call directly to the hpts?");

static void
tcp_hpts_log(struct tcp_hpts_entry *hpts, struct tcpcb *tp, struct timeval *tv,
	     int ticks_to_run, int idx)
{
	union tcp_log_stackspecific log;
	
	memset(&log.u_bbr, 0, sizeof(log.u_bbr));
	log.u_bbr.flex1 = hpts->p_nxt_slot;
	log.u_bbr.flex2 = hpts->p_cur_slot;
	log.u_bbr.flex3 = hpts->p_prev_slot;
	log.u_bbr.flex4 = idx;
	log.u_bbr.flex5 = hpts->p_curtick;
	log.u_bbr.flex6 = hpts->p_on_queue_cnt;
	log.u_bbr.use_lt_bw = 1;
	log.u_bbr.inflight = ticks_to_run;
	log.u_bbr.applimited = hpts->overidden_sleep;
	log.u_bbr.delivered = hpts->saved_curtick;
	log.u_bbr.timeStamp = tcp_tv_to_usectick(tv);
	log.u_bbr.epoch = hpts->saved_curslot;
	log.u_bbr.lt_epoch = hpts->saved_prev_slot;
	log.u_bbr.pkts_out = hpts->p_delayed_by;
	log.u_bbr.lost = hpts->p_hpts_sleep_time;
	log.u_bbr.cur_del_rate = hpts->p_runningtick;
	TCP_LOG_EVENTP(tp, NULL,
		       &tp->t_inpcb->inp_socket->so_rcv,
		       &tp->t_inpcb->inp_socket->so_snd,
		       BBR_LOG_HPTSDIAG, 0,
		       0, &log, false, tv);
}

static void
hpts_timeout_swi(void *arg)
{
	struct tcp_hpts_entry *hpts;

	hpts = (struct tcp_hpts_entry *)arg;
	swi_sched(hpts->ie_cookie, 0);
}

static void
hpts_timeout_dir(void *arg)
{
	tcp_hpts_thread(arg);
}

static inline void
hpts_sane_pace_remove(struct tcp_hpts_entry *hpts, struct inpcb *inp, struct hptsh *head, int clear)
{
#ifdef INVARIANTS
	if (mtx_owned(&hpts->p_mtx) == 0) {
		/* We don't own the mutex? */
		panic("%s: hpts:%p inp:%p no hpts mutex", __FUNCTION__, hpts, inp);
	}
	if (hpts->p_cpu != inp->inp_hpts_cpu) {
		/* It is not the right cpu/mutex? */
		panic("%s: hpts:%p inp:%p incorrect CPU", __FUNCTION__, hpts, inp);
	}
	if (inp->inp_in_hpts == 0) {
		/* We are not on the hpts? */
		panic("%s: hpts:%p inp:%p not on the hpts?", __FUNCTION__, hpts, inp);
	}
#endif
	TAILQ_REMOVE(head, inp, inp_hpts);
	hpts->p_on_queue_cnt--;
	if (hpts->p_on_queue_cnt < 0) {
		/* Count should not go negative .. */
#ifdef INVARIANTS
		panic("Hpts goes negative inp:%p hpts:%p",
		    inp, hpts);
#endif
		hpts->p_on_queue_cnt = 0;
	}
	if (clear) {
		inp->inp_hpts_request = 0;
		inp->inp_in_hpts = 0;
	}
}

static inline void
hpts_sane_pace_insert(struct tcp_hpts_entry *hpts, struct inpcb *inp, struct hptsh *head, int line, int noref)
{
#ifdef INVARIANTS
	if (mtx_owned(&hpts->p_mtx) == 0) {
		/* We don't own the mutex? */
		panic("%s: hpts:%p inp:%p no hpts mutex", __FUNCTION__, hpts, inp);
	}
	if (hpts->p_cpu != inp->inp_hpts_cpu) {
		/* It is not the right cpu/mutex? */
		panic("%s: hpts:%p inp:%p incorrect CPU", __FUNCTION__, hpts, inp);
	}
	if ((noref == 0) && (inp->inp_in_hpts == 1)) {
		/* We are already on the hpts? */
		panic("%s: hpts:%p inp:%p already on the hpts?", __FUNCTION__, hpts, inp);
	}
#endif
	TAILQ_INSERT_TAIL(head, inp, inp_hpts);
	inp->inp_in_hpts = 1;
	hpts->p_on_queue_cnt++;
	if (noref == 0) {
		in_pcbref(inp);
	}
}

static inline void
hpts_sane_input_remove(struct tcp_hpts_entry *hpts, struct inpcb *inp, int clear)
{
#ifdef INVARIANTS
	if (mtx_owned(&hpts->p_mtx) == 0) {
		/* We don't own the mutex? */
		panic("%s: hpts:%p inp:%p no hpts mutex", __FUNCTION__, hpts, inp);
	}
	if (hpts->p_cpu != inp->inp_input_cpu) {
		/* It is not the right cpu/mutex? */
		panic("%s: hpts:%p inp:%p incorrect CPU", __FUNCTION__, hpts, inp);
	}
	if (inp->inp_in_input == 0) {
		/* We are not on the input hpts? */
		panic("%s: hpts:%p inp:%p not on the input hpts?", __FUNCTION__, hpts, inp);
	}
#endif
	TAILQ_REMOVE(&hpts->p_input, inp, inp_input);
	hpts->p_on_inqueue_cnt--;
	if (hpts->p_on_inqueue_cnt < 0) {
#ifdef INVARIANTS
		panic("Hpts in goes negative inp:%p hpts:%p",
		    inp, hpts);
#endif
		hpts->p_on_inqueue_cnt = 0;
	}
#ifdef INVARIANTS
	if (TAILQ_EMPTY(&hpts->p_input) &&
	    (hpts->p_on_inqueue_cnt != 0)) {
		/* We should not be empty with a queue count */
		panic("%s hpts:%p in_hpts input empty but cnt:%d",
		    __FUNCTION__, hpts, hpts->p_on_inqueue_cnt);
	}
#endif
	if (clear)
		inp->inp_in_input = 0;
}

static inline void
hpts_sane_input_insert(struct tcp_hpts_entry *hpts, struct inpcb *inp, int line)
{
#ifdef INVARIANTS
	if (mtx_owned(&hpts->p_mtx) == 0) {
		/* We don't own the mutex? */
		panic("%s: hpts:%p inp:%p no hpts mutex", __FUNCTION__, hpts, inp);
	}
	if (hpts->p_cpu != inp->inp_input_cpu) {
		/* It is not the right cpu/mutex? */
		panic("%s: hpts:%p inp:%p incorrect CPU", __FUNCTION__, hpts, inp);
	}
	if (inp->inp_in_input == 1) {
		/* We are already on the input hpts? */
		panic("%s: hpts:%p inp:%p already on the input hpts?", __FUNCTION__, hpts, inp);
	}
#endif
	TAILQ_INSERT_TAIL(&hpts->p_input, inp, inp_input);
	inp->inp_in_input = 1;
	hpts->p_on_inqueue_cnt++;
	in_pcbref(inp);
}

static void
tcp_wakehpts(struct tcp_hpts_entry *hpts)
{
	HPTS_MTX_ASSERT(hpts);
	if (hpts->p_hpts_wake_scheduled == 0) {
		hpts->p_hpts_wake_scheduled = 1;
		swi_sched(hpts->ie_cookie, 0);
	}
}

static void
tcp_wakeinput(struct tcp_hpts_entry *hpts)
{
	HPTS_MTX_ASSERT(hpts);
	if (hpts->p_hpts_wake_scheduled == 0) {
		hpts->p_hpts_wake_scheduled = 1;
		swi_sched(hpts->ie_cookie, 0);
	}
}

struct tcp_hpts_entry *
tcp_cur_hpts(struct inpcb *inp)
{
	int32_t hpts_num;
	struct tcp_hpts_entry *hpts;

	hpts_num = inp->inp_hpts_cpu;
	hpts = tcp_pace.rp_ent[hpts_num];
	return (hpts);
}

struct tcp_hpts_entry *
tcp_hpts_lock(struct inpcb *inp)
{
	struct tcp_hpts_entry *hpts;
	int32_t hpts_num;

again:
	hpts_num = inp->inp_hpts_cpu;
	hpts = tcp_pace.rp_ent[hpts_num];
#ifdef INVARIANTS
	if (mtx_owned(&hpts->p_mtx)) {
		panic("Hpts:%p owns mtx prior-to lock line:%d",
		    hpts, __LINE__);
	}
#endif
	mtx_lock(&hpts->p_mtx);
	if (hpts_num != inp->inp_hpts_cpu) {
		mtx_unlock(&hpts->p_mtx);
		goto again;
	}
	return (hpts);
}

struct tcp_hpts_entry *
tcp_input_lock(struct inpcb *inp)
{
	struct tcp_hpts_entry *hpts;
	int32_t hpts_num;

again:
	hpts_num = inp->inp_input_cpu;
	hpts = tcp_pace.rp_ent[hpts_num];
#ifdef INVARIANTS
	if (mtx_owned(&hpts->p_mtx)) {
		panic("Hpts:%p owns mtx prior-to lock line:%d",
		    hpts, __LINE__);
	}
#endif
	mtx_lock(&hpts->p_mtx);
	if (hpts_num != inp->inp_input_cpu) {
		mtx_unlock(&hpts->p_mtx);
		goto again;
	}
	return (hpts);
}

static void
tcp_remove_hpts_ref(struct inpcb *inp, struct tcp_hpts_entry *hpts, int line)
{
	int32_t add_freed;

	if (inp->inp_flags2 & INP_FREED) {
		/*
		 * Need to play a special trick so that in_pcbrele_wlocked
		 * does not return 1 when it really should have returned 0.
		 */
		add_freed = 1;
		inp->inp_flags2 &= ~INP_FREED;
	} else {
		add_freed = 0;
	}
#ifndef INP_REF_DEBUG
	if (in_pcbrele_wlocked(inp)) {
		/*
		 * This should not happen. We have the inpcb referred to by
		 * the main socket (why we are called) and the hpts. It
		 * should always return 0.
		 */
		panic("inpcb:%p release ret 1",
		    inp);
	}
#else
	if (__in_pcbrele_wlocked(inp, line)) {
		/*
		 * This should not happen. We have the inpcb referred to by
		 * the main socket (why we are called) and the hpts. It
		 * should always return 0.
		 */
		panic("inpcb:%p release ret 1",
		    inp);
	}
#endif
	if (add_freed) {
		inp->inp_flags2 |= INP_FREED;
	}
}

static void
tcp_hpts_remove_locked_output(struct tcp_hpts_entry *hpts, struct inpcb *inp, int32_t flags, int32_t line)
{
	if (inp->inp_in_hpts) {
		hpts_sane_pace_remove(hpts, inp, &hpts->p_hptss[inp->inp_hptsslot], 1);
		tcp_remove_hpts_ref(inp, hpts, line);
	}
}

static void
tcp_hpts_remove_locked_input(struct tcp_hpts_entry *hpts, struct inpcb *inp, int32_t flags, int32_t line)
{
	HPTS_MTX_ASSERT(hpts);
	if (inp->inp_in_input) {
		hpts_sane_input_remove(hpts, inp, 1);
		tcp_remove_hpts_ref(inp, hpts, line);
	}
}

/*
 * Called normally with the INP_LOCKED but it
 * does not matter, the hpts lock is the key
 * but the lock order allows us to hold the
 * INP lock and then get the hpts lock.
 *
 * Valid values in the flags are
 * HPTS_REMOVE_OUTPUT - remove from the output of the hpts.
 * HPTS_REMOVE_INPUT - remove from the input of the hpts.
 * Note that you can use one or both values together 
 * and get two actions.
 */
void
__tcp_hpts_remove(struct inpcb *inp, int32_t flags, int32_t line)
{
	struct tcp_hpts_entry *hpts;

	INP_WLOCK_ASSERT(inp);
	if (flags & HPTS_REMOVE_OUTPUT) {
		hpts = tcp_hpts_lock(inp);
		tcp_hpts_remove_locked_output(hpts, inp, flags, line);
		mtx_unlock(&hpts->p_mtx);
	}
	if (flags & HPTS_REMOVE_INPUT) {
		hpts = tcp_input_lock(inp);
		tcp_hpts_remove_locked_input(hpts, inp, flags, line);
		mtx_unlock(&hpts->p_mtx);
	}
}

static inline int
hpts_tick(uint32_t wheel_tick, uint32_t plus)
{
	/*
	 * Given a slot on the wheel, what slot
	 * is that plus ticks out?
	 */
	KASSERT(wheel_tick < NUM_OF_HPTSI_SLOTS, ("Invalid tick %u not on wheel", wheel_tick));
	return ((wheel_tick + plus) % NUM_OF_HPTSI_SLOTS);
}

static inline int
tick_to_wheel(uint32_t cts_in_wticks)
{
	/* 
	 * Given a timestamp in wheel ticks (10usec inc's)
	 * map it to our limited space wheel.
	 */
	return (cts_in_wticks % NUM_OF_HPTSI_SLOTS);
}

static inline int
hpts_ticks_diff(int prev_tick, int tick_now)
{
	/*
	 * Given two ticks that are someplace
	 * on our wheel. How far are they apart?
	 */
	if (tick_now > prev_tick)
		return (tick_now - prev_tick);
	else if (tick_now == prev_tick)
		/* 
		 * Special case, same means we can go all of our 
		 * wheel less one slot.
		 */
		return (NUM_OF_HPTSI_SLOTS - 1);
	else
		return ((NUM_OF_HPTSI_SLOTS - prev_tick) + tick_now);
}

/*
 * Given a tick on the wheel that is the current time
 * mapped to the wheel (wheel_tick), what is the maximum
 * distance forward that can be obtained without
 * wrapping past either prev_tick or running_tick
 * depending on the htps state? Also if passed
 * a uint32_t *, fill it with the tick location.
 *
 * Note if you do not give this function the current
 * time (that you think it is) mapped to the wheel 
 * then the results will not be what you expect and
 * could lead to invalid inserts.
 */
static inline int32_t
max_ticks_available(struct tcp_hpts_entry *hpts, uint32_t wheel_tick, uint32_t *target_tick)
{
	uint32_t dis_to_travel, end_tick, pacer_to_now, avail_on_wheel;

	if ((hpts->p_hpts_active == 1) &&
	    (hpts->p_wheel_complete == 0)) {
		end_tick = hpts->p_runningtick;
		/* Back up one tick */
		if (end_tick == 0)
			end_tick = NUM_OF_HPTSI_SLOTS - 1;
		else
			end_tick--;
		if (target_tick)
			*target_tick = end_tick;
	} else {
		/*
		 * For the case where we are
		 * not active, or we have
		 * completed the pass over
		 * the wheel, we can use the
		 * prev tick and subtract one from it. This puts us
		 * as far out as possible on the wheel.
		 */
		end_tick = hpts->p_prev_slot;
		if (end_tick == 0)
			end_tick = NUM_OF_HPTSI_SLOTS - 1;
		else
			end_tick--;
		if (target_tick)
			*target_tick = end_tick;
		/* 
		 * Now we have close to the full wheel left minus the 
		 * time it has been since the pacer went to sleep. Note
		 * that wheel_tick, passed in, should be the current time
		 * from the perspective of the caller, mapped to the wheel.
		 */
		if (hpts->p_prev_slot != wheel_tick)
			dis_to_travel = hpts_ticks_diff(hpts->p_prev_slot, wheel_tick);
		else
			dis_to_travel = 1;
		/* 
		 * dis_to_travel in this case is the space from when the 
		 * pacer stopped (p_prev_slot) and where our wheel_tick 
		 * is now. To know how many slots we can put it in we 
		 * subtract from the wheel size. We would not want
		 * to place something after p_prev_slot or it will
		 * get ran too soon.
		 */
		return (NUM_OF_HPTSI_SLOTS - dis_to_travel);
	}
	/* 
	 * So how many slots are open between p_runningtick -> p_cur_slot 
	 * that is what is currently un-available for insertion. Special
	 * case when we are at the last slot, this gets 1, so that
	 * the answer to how many slots are available is all but 1.
	 */
	if (hpts->p_runningtick == hpts->p_cur_slot)
		dis_to_travel = 1;
	else
		dis_to_travel = hpts_ticks_diff(hpts->p_runningtick, hpts->p_cur_slot);
	/* 
	 * How long has the pacer been running?
	 */
	if (hpts->p_cur_slot != wheel_tick) {
		/* The pacer is a bit late */
		pacer_to_now = hpts_ticks_diff(hpts->p_cur_slot, wheel_tick);
	} else {
		/* The pacer is right on time, now == pacers start time */
		pacer_to_now = 0;
	}
	/* 
	 * To get the number left we can insert into we simply
	 * subract the distance the pacer has to run from how
	 * many slots there are.
	 */
	avail_on_wheel = NUM_OF_HPTSI_SLOTS - dis_to_travel;
	/* 
	 * Now how many of those we will eat due to the pacer's 
	 * time (p_cur_slot) of start being behind the 
	 * real time (wheel_tick)?
	 */
	if (avail_on_wheel <= pacer_to_now) {
		/* 
		 * Wheel wrap, we can't fit on the wheel, that
		 * is unusual the system must be way overloaded!
		 * Insert into the assured tick, and return special
		 * "0".
		 */
		counter_u64_add(combined_wheel_wrap, 1);
		*target_tick = hpts->p_nxt_slot;
		return (0);
	} else {
		/* 
		 * We know how many slots are open
		 * on the wheel (the reverse of what
		 * is left to run. Take away the time
		 * the pacer started to now (wheel_tick)
		 * and that tells you how many slots are
		 * open that can be inserted into that won't
		 * be touched by the pacer until later.
		 */
		return (avail_on_wheel - pacer_to_now);
	}
}

static int
tcp_queue_to_hpts_immediate_locked(struct inpcb *inp, struct tcp_hpts_entry *hpts, int32_t line, int32_t noref)
{
	uint32_t need_wake = 0;
	
	HPTS_MTX_ASSERT(hpts);
	if (inp->inp_in_hpts == 0) {
		/* Ok we need to set it on the hpts in the current slot */
		inp->inp_hpts_request = 0;
		if ((hpts->p_hpts_active == 0) ||
		    (hpts->p_wheel_complete)) {
			/*
			 * A sleeping hpts we want in next slot to run 
			 * note that in this state p_prev_slot == p_cur_slot
			 */
			inp->inp_hptsslot = hpts_tick(hpts->p_prev_slot, 1);
			if ((hpts->p_on_min_sleep == 0) && (hpts->p_hpts_active == 0))
				need_wake = 1;
		} else if ((void *)inp == hpts->p_inp) {
			/*
			 * The hpts system is running and the caller
			 * was awoken by the hpts system. 
			 * We can't allow you to go into the same slot we
			 * are in (we don't want a loop :-D).
			 */
			inp->inp_hptsslot = hpts->p_nxt_slot;
		} else
			inp->inp_hptsslot = hpts->p_runningtick;
		hpts_sane_pace_insert(hpts, inp, &hpts->p_hptss[inp->inp_hptsslot], line, noref);
		if (need_wake) {
			/*
			 * Activate the hpts if it is sleeping and its
			 * timeout is not 1.
			 */
			hpts->p_direct_wake = 1;
			tcp_wakehpts(hpts);
		}
	}
	return (need_wake);
}

int
__tcp_queue_to_hpts_immediate(struct inpcb *inp, int32_t line)
{
	int32_t ret;
	struct tcp_hpts_entry *hpts;

	INP_WLOCK_ASSERT(inp);
	hpts = tcp_hpts_lock(inp);
	ret = tcp_queue_to_hpts_immediate_locked(inp, hpts, line, 0);
	mtx_unlock(&hpts->p_mtx);
	return (ret);
}

#ifdef INVARIANTS
static void
check_if_slot_would_be_wrong(struct tcp_hpts_entry *hpts, struct inpcb *inp, uint32_t inp_hptsslot, int line)
{
	/*
	 * Sanity checks for the pacer with invariants 
	 * on insert.
	 */
	if (inp_hptsslot >= NUM_OF_HPTSI_SLOTS)
		panic("hpts:%p inp:%p slot:%d > max",
		      hpts, inp, inp_hptsslot);
	if ((hpts->p_hpts_active) &&
	    (hpts->p_wheel_complete == 0)) {
		/* 
		 * If the pacer is processing a arc
		 * of the wheel, we need to make
		 * sure we are not inserting within
		 * that arc.
		 */
		int distance, yet_to_run;

		distance = hpts_ticks_diff(hpts->p_runningtick, inp_hptsslot);
		if (hpts->p_runningtick != hpts->p_cur_slot)
			yet_to_run = hpts_ticks_diff(hpts->p_runningtick, hpts->p_cur_slot);
		else
			yet_to_run = 0;	/* processing last slot */
		if (yet_to_run > distance) {
			panic("hpts:%p inp:%p slot:%d distance:%d yet_to_run:%d rs:%d cs:%d",
			      hpts, inp, inp_hptsslot,
			      distance, yet_to_run,
			      hpts->p_runningtick, hpts->p_cur_slot);
		}
	}
}
#endif

static void
tcp_hpts_insert_locked(struct tcp_hpts_entry *hpts, struct inpcb *inp, uint32_t slot, int32_t line,
		       struct hpts_diag *diag, struct timeval *tv)
{
	uint32_t need_new_to = 0;
	uint32_t wheel_cts, last_tick;
	int32_t wheel_tick, maxticks;
	int8_t need_wakeup = 0;

	HPTS_MTX_ASSERT(hpts);
	if (diag) {
		memset(diag, 0, sizeof(struct hpts_diag));
		diag->p_hpts_active = hpts->p_hpts_active;
		diag->p_prev_slot = hpts->p_prev_slot;
		diag->p_runningtick = hpts->p_runningtick;
		diag->p_nxt_slot = hpts->p_nxt_slot;
		diag->p_cur_slot = hpts->p_cur_slot;
		diag->p_curtick = hpts->p_curtick;
		diag->p_lasttick = hpts->p_lasttick;
		diag->slot_req = slot;
		diag->p_on_min_sleep = hpts->p_on_min_sleep;
		diag->hpts_sleep_time = hpts->p_hpts_sleep_time;
	}
	if (inp->inp_in_hpts == 0) {
		if (slot == 0) {
			/* Immediate */
			tcp_queue_to_hpts_immediate_locked(inp, hpts, line, 0);
			return;
		}
		/* Get the current time relative to the wheel */
		wheel_cts = tcp_tv_to_hptstick(tv);
		/* Map it onto the wheel */
		wheel_tick = tick_to_wheel(wheel_cts);
		/* Now what's the max we can place it at? */
		maxticks = max_ticks_available(hpts, wheel_tick, &last_tick);
		if (diag) {
			diag->wheel_tick = wheel_tick;
			diag->maxticks = maxticks;
			diag->wheel_cts = wheel_cts;
		}
		if (maxticks == 0) {
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
			inp->inp_hptsslot = last_tick;
			inp->inp_hpts_request = slot;
		} else 	if (maxticks >= slot) {
			/* It all fits on the wheel */
			inp->inp_hpts_request = 0;
			inp->inp_hptsslot = hpts_tick(wheel_tick, slot);
		} else {
			/* It does not fit */
			inp->inp_hpts_request = slot - maxticks;
			inp->inp_hptsslot = last_tick;
		}
		if (diag) {
			diag->slot_remaining = inp->inp_hpts_request;
			diag->inp_hptsslot = inp->inp_hptsslot;
		}
#ifdef INVARIANTS
		check_if_slot_would_be_wrong(hpts, inp, inp->inp_hptsslot, line);
#endif
		hpts_sane_pace_insert(hpts, inp, &hpts->p_hptss[inp->inp_hptsslot], line, 0);
		if ((hpts->p_hpts_active == 0) &&
		    (inp->inp_hpts_request == 0) &&
		    (hpts->p_on_min_sleep == 0)) {
			/*
			 * The hpts is sleeping and not on a minimum
			 * sleep time, we need to figure out where
			 * it will wake up at and if we need to reschedule
			 * its time-out.
			 */
			uint32_t have_slept, yet_to_sleep;

			/* Now do we need to restart the hpts's timer? */
			have_slept = hpts_ticks_diff(hpts->p_prev_slot, wheel_tick);
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
				need_new_to = slot * HPTS_TICKS_PER_USEC;
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
			if (tcp_hpts_callout_skip_swi == 0) {
				co_ret = callout_reset_sbt_on(&hpts->co, sb, 0,
				    hpts_timeout_swi, hpts, hpts->p_cpu,
				    (C_DIRECT_EXEC | C_PREL(tcp_hpts_precision)));
			} else {
				co_ret = callout_reset_sbt_on(&hpts->co, sb, 0,
				    hpts_timeout_dir, hpts,
				    hpts->p_cpu,
				    C_PREL(tcp_hpts_precision));
			}
			if (diag) {
				diag->need_new_to = need_new_to;
				diag->co_ret = co_ret;
			}
		}
	} else {
#ifdef INVARIANTS
		panic("Hpts:%p tp:%p already on hpts and add?", hpts, inp);
#endif
	}
}

uint32_t
tcp_hpts_insert_diag(struct inpcb *inp, uint32_t slot, int32_t line, struct hpts_diag *diag)
{
	struct tcp_hpts_entry *hpts;
	uint32_t slot_on;
	struct timeval tv;

	/*
	 * We now return the next-slot the hpts will be on, beyond its
	 * current run (if up) or where it was when it stopped if it is
	 * sleeping.
	 */
	INP_WLOCK_ASSERT(inp);
	hpts = tcp_hpts_lock(inp);
	microuptime(&tv);
	tcp_hpts_insert_locked(hpts, inp, slot, line, diag, &tv);
	slot_on = hpts->p_nxt_slot;
	mtx_unlock(&hpts->p_mtx);
	return (slot_on);
}

uint32_t
__tcp_hpts_insert(struct inpcb *inp, uint32_t slot, int32_t line){
	return (tcp_hpts_insert_diag(inp, slot, line, NULL));
}
int
__tcp_queue_to_input_locked(struct inpcb *inp, struct tcp_hpts_entry *hpts, int32_t line)
{
	int32_t retval = 0;

	HPTS_MTX_ASSERT(hpts);
	if (inp->inp_in_input == 0) {
		/* Ok we need to set it on the hpts in the current slot */
		hpts_sane_input_insert(hpts, inp, line);
		retval = 1;
		if (hpts->p_hpts_active == 0) {
			/*
			 * Activate the hpts if it is sleeping.
			 */
			retval = 2;
			hpts->p_direct_wake = 1;
			tcp_wakeinput(hpts);
		}
	} else if (hpts->p_hpts_active == 0) {
		retval = 4;
		hpts->p_direct_wake = 1;
		tcp_wakeinput(hpts);
	}
	return (retval);
}

int32_t
__tcp_queue_to_input(struct inpcb *inp, int line)
{
	struct tcp_hpts_entry *hpts;
	int32_t ret;

	hpts = tcp_input_lock(inp);
	ret = __tcp_queue_to_input_locked(inp, hpts, line);
	mtx_unlock(&hpts->p_mtx);
	return (ret);
}

void
__tcp_set_inp_to_drop(struct inpcb *inp, uint16_t reason, int32_t line)
{
	struct tcp_hpts_entry *hpts;
	struct tcpcb *tp;

	tp = intotcpcb(inp);
	hpts = tcp_input_lock(tp->t_inpcb);
	if (inp->inp_in_input == 0) {
		/* Ok we need to set it on the hpts in the current slot */
		hpts_sane_input_insert(hpts, inp, line);
		if (hpts->p_hpts_active == 0) {
			/*
			 * Activate the hpts if it is sleeping.
			 */
			hpts->p_direct_wake = 1;
			tcp_wakeinput(hpts);
		}
	} else if (hpts->p_hpts_active == 0) {
		hpts->p_direct_wake = 1;
		tcp_wakeinput(hpts);
	}
	inp->inp_hpts_drop_reas = reason;
	mtx_unlock(&hpts->p_mtx);
}

static uint16_t
hpts_random_cpu(struct inpcb *inp){
	/*
	 * No flow type set distribute the load randomly.
	 */
	uint16_t cpuid;
	uint32_t ran;

	/*
	 * If one has been set use it i.e. we want both in and out on the
	 * same hpts.
	 */
	if (inp->inp_input_cpu_set) {
		return (inp->inp_input_cpu);
	} else if (inp->inp_hpts_cpu_set) {
		return (inp->inp_hpts_cpu);
	}
	/* Nothing set use a random number */
	ran = arc4random();
	cpuid = (ran & 0xffff) % mp_ncpus;
	return (cpuid);
}

static uint16_t
hpts_cpuid(struct inpcb *inp){
	u_int cpuid;
#ifdef NUMA
	struct hpts_domain_info *di;
#endif

	/*
	 * If one has been set use it i.e. we want both in and out on the
	 * same hpts.
	 */
	if (inp->inp_input_cpu_set) {
		return (inp->inp_input_cpu);
	} else if (inp->inp_hpts_cpu_set) {
		return (inp->inp_hpts_cpu);
	}
	/* If one is set the other must be the same */
#ifdef	RSS
	cpuid = rss_hash2cpuid(inp->inp_flowid, inp->inp_flowtype);
	if (cpuid == NETISR_CPUID_NONE)
		return (hpts_random_cpu(inp));
	else
		return (cpuid);
#else
	/*
	 * We don't have a flowid -> cpuid mapping, so cheat and just map
	 * unknown cpuids to curcpu.  Not the best, but apparently better
	 * than defaulting to swi 0.
	 */
	
	if (inp->inp_flowtype == M_HASHTYPE_NONE)
		return (hpts_random_cpu(inp));
	/*
	 * Hash to a thread based on the flowid.  If we are using numa,
	 * then restrict the hash to the numa domain where the inp lives.
	 */
#ifdef NUMA
	if (tcp_bind_threads == 2 && inp->inp_numa_domain != M_NODOM) {
		di = &hpts_domains[inp->inp_numa_domain];
		cpuid = di->cpu[inp->inp_flowid % di->count];
	} else
#endif
		cpuid = inp->inp_flowid % mp_ncpus;

	return (cpuid);
#endif
}

static void
tcp_drop_in_pkts(struct tcpcb *tp)
{
	struct mbuf *m, *n;
	
	m = tp->t_in_pkt;
	if (m)
		n = m->m_nextpkt;
	else
		n = NULL;
	tp->t_in_pkt = NULL;
	while (m) {
		m_freem(m);
		m = n;
		if (m)
			n = m->m_nextpkt;
	}
}

/*
 * Do NOT try to optimize the processing of inp's
 * by first pulling off all the inp's into a temporary
 * list (e.g. TAILQ_CONCAT). If you do that the subtle
 * interactions of switching CPU's will kill because of
 * problems in the linked list manipulation. Basically
 * you would switch cpu's with the hpts mutex locked
 * but then while you were processing one of the inp's
 * some other one that you switch will get a new
 * packet on the different CPU. It will insert it
 * on the new hpts's input list. Creating a temporary
 * link in the inp will not fix it either, since
 * the other hpts will be doing the same thing and
 * you will both end up using the temporary link.
 *
 * You will die in an ASSERT for tailq corruption if you
 * run INVARIANTS or you will die horribly without
 * INVARIANTS in some unknown way with a corrupt linked
 * list.
 */
static void
tcp_input_data(struct tcp_hpts_entry *hpts, struct timeval *tv)
{
	struct tcpcb *tp;
	struct inpcb *inp;
	uint16_t drop_reason;
	int16_t set_cpu;
	uint32_t did_prefetch = 0;
	int dropped;

	HPTS_MTX_ASSERT(hpts);
	NET_EPOCH_ASSERT();

	while ((inp = TAILQ_FIRST(&hpts->p_input)) != NULL) {
		HPTS_MTX_ASSERT(hpts);
		hpts_sane_input_remove(hpts, inp, 0);
		if (inp->inp_input_cpu_set == 0) {
			set_cpu = 1;
		} else {
			set_cpu = 0;
		}
		hpts->p_inp = inp;
		drop_reason = inp->inp_hpts_drop_reas;
		inp->inp_in_input = 0;
		mtx_unlock(&hpts->p_mtx);
		INP_WLOCK(inp);
#ifdef VIMAGE
		CURVNET_SET(inp->inp_vnet);
#endif
		if ((inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) ||
		    (inp->inp_flags2 & INP_FREED)) {
out:
			hpts->p_inp = NULL;
			if (in_pcbrele_wlocked(inp) == 0) {
				INP_WUNLOCK(inp);
			}
#ifdef VIMAGE
			CURVNET_RESTORE();
#endif
			mtx_lock(&hpts->p_mtx);
			continue;
		}
		tp = intotcpcb(inp);
		if ((tp == NULL) || (tp->t_inpcb == NULL)) {
			goto out;
		}
		if (drop_reason) {
			/* This tcb is being destroyed for drop_reason */
			tcp_drop_in_pkts(tp);
			tp = tcp_drop(tp, drop_reason);
			if (tp == NULL) {
				INP_WLOCK(inp);
			}
			if (in_pcbrele_wlocked(inp) == 0)
				INP_WUNLOCK(inp);
#ifdef VIMAGE
			CURVNET_RESTORE();
#endif
			mtx_lock(&hpts->p_mtx);
			continue;
		}
		if (set_cpu) {
			/*
			 * Setup so the next time we will move to the right
			 * CPU. This should be a rare event. It will
			 * sometimes happens when we are the client side
			 * (usually not the server). Somehow tcp_output()
			 * gets called before the tcp_do_segment() sets the
			 * intial state. This means the r_cpu and r_hpts_cpu
			 * is 0. We get on the hpts, and then tcp_input()
			 * gets called setting up the r_cpu to the correct
			 * value. The hpts goes off and sees the mis-match.
			 * We simply correct it here and the CPU will switch
			 * to the new hpts nextime the tcb gets added to the
			 * the hpts (not this time) :-)
			 */
			tcp_set_hpts(inp);
		}
		if (tp->t_fb_ptr != NULL) {
			kern_prefetch(tp->t_fb_ptr, &did_prefetch);
			did_prefetch = 1;
		}
		if ((inp->inp_flags2 & INP_SUPPORTS_MBUFQ) && tp->t_in_pkt) {
			if (inp->inp_in_input)
				tcp_hpts_remove(inp, HPTS_REMOVE_INPUT);
			dropped = (*tp->t_fb->tfb_do_queued_segments)(inp->inp_socket, tp, 0);
			if (dropped) {
				/* Re-acquire the wlock so we can release the reference */
				INP_WLOCK(inp);
			}
		} else if (tp->t_in_pkt) {
			/* 
			 * We reach here only if we had a 
			 * stack that supported INP_SUPPORTS_MBUFQ
			 * and then somehow switched to a stack that
			 * does not. The packets are basically stranded
			 * and would hang with the connection until
			 * cleanup without this code. Its not the
			 * best way but I know of no other way to
			 * handle it since the stack needs functions
			 * it does not have to handle queued packets.
			 */
			tcp_drop_in_pkts(tp);
		}
		if (in_pcbrele_wlocked(inp) == 0)
			INP_WUNLOCK(inp);
		INP_UNLOCK_ASSERT(inp);
#ifdef VIMAGE
		CURVNET_RESTORE();
#endif
		mtx_lock(&hpts->p_mtx);
		hpts->p_inp = NULL;
	}
}

static void
tcp_hptsi(struct tcp_hpts_entry *hpts)
{
	struct tcpcb *tp;
	struct inpcb *inp = NULL, *ninp;
	struct timeval tv;
	int32_t ticks_to_run, i, error;
	int32_t paced_cnt = 0;
	int32_t loop_cnt = 0;
	int32_t did_prefetch = 0;
	int32_t prefetch_ninp = 0;
	int32_t prefetch_tp = 0;
	int32_t wrap_loop_cnt = 0;
	int16_t set_cpu;

	HPTS_MTX_ASSERT(hpts);
	NET_EPOCH_ASSERT();

	/* record previous info for any logging */
	hpts->saved_lasttick = hpts->p_lasttick;
	hpts->saved_curtick = hpts->p_curtick;
	hpts->saved_curslot = hpts->p_cur_slot;
	hpts->saved_prev_slot = hpts->p_prev_slot;

	hpts->p_lasttick = hpts->p_curtick;
	hpts->p_curtick = tcp_gethptstick(&tv);
	hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
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
	ticks_to_run = hpts_ticks_diff(hpts->p_prev_slot, hpts->p_cur_slot);
	if (((hpts->p_curtick - hpts->p_lasttick) > ticks_to_run) &&
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
		 * the running postion. Now the problem is the
		 * reserved "not to yet" place does not exist
		 * and there may be inp's in there that need
		 * running. We can merge those into the
		 * first slot at the head.
		 */
		wrap_loop_cnt++;
		hpts->p_nxt_slot = hpts_tick(hpts->p_prev_slot, 1);
		hpts->p_runningtick = hpts_tick(hpts->p_prev_slot, 2);
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
#ifdef INVARIANTS
		/* 
		 * To prevent a panic we need to update the inpslot to the
		 * new location. This is safe since it takes both the
		 * INP lock and the pacer mutex to change the inp_hptsslot.
		 */
		TAILQ_FOREACH(inp, &hpts->p_hptss[hpts->p_nxt_slot], inp_hpts) {
			inp->inp_hptsslot = hpts->p_runningtick;
		}
#endif
		TAILQ_CONCAT(&hpts->p_hptss[hpts->p_runningtick],
			     &hpts->p_hptss[hpts->p_nxt_slot], inp_hpts);
		ticks_to_run = NUM_OF_HPTSI_SLOTS - 1;
		counter_u64_add(wheel_wrap, 1);
	} else {
		/* 
		 * Nxt slot is always one after p_runningtick though
		 * its not used usually unless we are doing wheel wrap.
		 */
		hpts->p_nxt_slot = hpts->p_prev_slot;
		hpts->p_runningtick = hpts_tick(hpts->p_prev_slot, 1);
	}
#ifdef INVARIANTS
	if (TAILQ_EMPTY(&hpts->p_input) &&
	    (hpts->p_on_inqueue_cnt != 0)) {
		panic("tp:%p in_hpts input empty but cnt:%d",
		      hpts, hpts->p_on_inqueue_cnt);
	}
#endif
	HPTS_MTX_ASSERT(hpts);
	if (hpts->p_on_queue_cnt == 0) {
		goto no_one;
	}
	HPTS_MTX_ASSERT(hpts);
	for (i = 0; i < ticks_to_run; i++) {
		/*
		 * Calculate our delay, if there are no extra ticks there
		 * was not any (i.e. if ticks_to_run == 1, no delay).
		 */
		hpts->p_delayed_by = (ticks_to_run - (i + 1)) * HPTS_TICKS_PER_USEC;
		HPTS_MTX_ASSERT(hpts);
		while ((inp = TAILQ_FIRST(&hpts->p_hptss[hpts->p_runningtick])) != NULL) {
			/* For debugging */
			hpts->p_inp = inp;
			paced_cnt++;
#ifdef INVARIANTS
			if (hpts->p_runningtick != inp->inp_hptsslot) {
				panic("Hpts:%p inp:%p slot mis-aligned %u vs %u",
				      hpts, inp, hpts->p_runningtick, inp->inp_hptsslot);
			}
#endif
			/* Now pull it */
			if (inp->inp_hpts_cpu_set == 0) {
				set_cpu = 1;
			} else {
				set_cpu = 0;
			}
			hpts_sane_pace_remove(hpts, inp, &hpts->p_hptss[hpts->p_runningtick], 0);
			if ((ninp = TAILQ_FIRST(&hpts->p_hptss[hpts->p_runningtick])) != NULL) {
				/* We prefetch the next inp if possible */
				kern_prefetch(ninp, &prefetch_ninp);
				prefetch_ninp = 1;
			}
			if (inp->inp_hpts_request) {
				/*
				 * This guy is deferred out further in time
				 * then our wheel had available on it. 
				 * Push him back on the wheel or run it
				 * depending.
				 */
				uint32_t maxticks, last_tick, remaining_slots;
				
				remaining_slots = ticks_to_run - (i + 1);
				if (inp->inp_hpts_request > remaining_slots) {
					/*
					 * How far out can we go?
					 */
					maxticks = max_ticks_available(hpts, hpts->p_cur_slot, &last_tick);
					if (maxticks >= inp->inp_hpts_request) {
						/* we can place it finally to be processed  */
						inp->inp_hptsslot = hpts_tick(hpts->p_runningtick, inp->inp_hpts_request);
						inp->inp_hpts_request = 0;
					} else {
						/* Work off some more time */
						inp->inp_hptsslot = last_tick;
						inp->inp_hpts_request-= maxticks;
					}
					hpts_sane_pace_insert(hpts, inp, &hpts->p_hptss[inp->inp_hptsslot], __LINE__, 1);
					hpts->p_inp = NULL;
					continue;
				}
				inp->inp_hpts_request = 0;
				/* Fall through we will so do it now */
			}
			/*
			 * We clear the hpts flag here after dealing with	
			 * remaining slots. This way anyone looking with the
			 * TCB lock will see its on the hpts until just
			 * before we unlock.
			 */
			inp->inp_in_hpts = 0;
			mtx_unlock(&hpts->p_mtx);
			INP_WLOCK(inp);
			if (in_pcbrele_wlocked(inp)) {
				mtx_lock(&hpts->p_mtx);
				hpts->p_inp = NULL;
				continue;
			}
			if ((inp->inp_flags & (INP_TIMEWAIT | INP_DROPPED)) ||
			    (inp->inp_flags2 & INP_FREED)) {
			out_now:
#ifdef INVARIANTS
				if (mtx_owned(&hpts->p_mtx)) {
					panic("Hpts:%p owns mtx prior-to lock line:%d",
					      hpts, __LINE__);
				}
#endif
				INP_WUNLOCK(inp);
				mtx_lock(&hpts->p_mtx);
				hpts->p_inp = NULL;
				continue;
			}
			tp = intotcpcb(inp);
			if ((tp == NULL) || (tp->t_inpcb == NULL)) {
				goto out_now;
			}
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
				 * gets added to the the hpts (not this one)
				 * :-)
				 */
				tcp_set_hpts(inp);
			}
#ifdef VIMAGE
			CURVNET_SET(inp->inp_vnet);
#endif
			/* Lets do any logging that we might want to */
			if (hpts_does_tp_logging && (tp->t_logstate != TCP_LOG_STATE_OFF)) {
				tcp_hpts_log(hpts, tp, &tv, ticks_to_run, i);
			}
			/*
			 * There is a hole here, we get the refcnt on the
			 * inp so it will still be preserved but to make
			 * sure we can get the INP we need to hold the p_mtx
			 * above while we pull out the tp/inp,  as long as
			 * fini gets the lock first we are assured of having
			 * a sane INP we can lock and test.
			 */
#ifdef INVARIANTS
			if (mtx_owned(&hpts->p_mtx)) {
				panic("Hpts:%p owns mtx before tcp-output:%d",
				      hpts, __LINE__);
			}
#endif
			if (tp->t_fb_ptr != NULL) {
				kern_prefetch(tp->t_fb_ptr, &did_prefetch);
				did_prefetch = 1;
			}
			if ((inp->inp_flags2 & INP_SUPPORTS_MBUFQ) && tp->t_in_pkt) {
				error = (*tp->t_fb->tfb_do_queued_segments)(inp->inp_socket, tp, 0);
				if (error) {
					/* The input killed the connection */
					goto skip_pacing;
				}
			}
			inp->inp_hpts_calls = 1;
			error = tp->t_fb->tfb_tcp_output(tp);
			inp->inp_hpts_calls = 0;
			if (ninp && ninp->inp_ppcb) {
				/*
				 * If we have a nxt inp, see if we can
				 * prefetch its ppcb. Note this may seem
				 * "risky" since we have no locks (other
				 * than the previous inp) and there no
				 * assurance that ninp was not pulled while
				 * we were processing inp and freed. If this
				 * occured it could mean that either:
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
				 */
				kern_prefetch(ninp->inp_ppcb, &prefetch_tp);
				prefetch_tp = 1;
			}
			INP_WUNLOCK(inp);
		skip_pacing:
#ifdef VIMAGE
			CURVNET_RESTORE();
#endif
			INP_UNLOCK_ASSERT(inp);
#ifdef INVARIANTS
			if (mtx_owned(&hpts->p_mtx)) {
				panic("Hpts:%p owns mtx prior-to lock line:%d",
				      hpts, __LINE__);
			}
#endif
			mtx_lock(&hpts->p_mtx);
			hpts->p_inp = NULL;
		}
		HPTS_MTX_ASSERT(hpts);
		hpts->p_inp = NULL;
		hpts->p_runningtick++;
		if (hpts->p_runningtick >= NUM_OF_HPTSI_SLOTS) {
			hpts->p_runningtick = 0;
		}
	}
no_one:
	HPTS_MTX_ASSERT(hpts);
	hpts->p_delayed_by = 0;
	/*
	 * Check to see if we took an excess amount of time and need to run
	 * more ticks (if we did not hit eno-bufs).
	 */
#ifdef INVARIANTS
	if (TAILQ_EMPTY(&hpts->p_input) &&
	    (hpts->p_on_inqueue_cnt != 0)) {
		panic("tp:%p in_hpts input empty but cnt:%d",
		      hpts, hpts->p_on_inqueue_cnt);
	}
#endif
	hpts->p_prev_slot = hpts->p_cur_slot;
	hpts->p_lasttick = hpts->p_curtick;
	if (loop_cnt > max_pacer_loops) {	    
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
		counter_u64_add(hpts_hopelessly_behind, 1);
		goto no_run;
	}
	hpts->p_curtick = tcp_gethptstick(&tv);
	hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
	if ((wrap_loop_cnt < 2) &&
	    (hpts->p_lasttick != hpts->p_curtick)) {
		counter_u64_add(hpts_loops, 1);
		loop_cnt++;
		goto again;
	}
no_run:
	/*
	 * Set flag to tell that we are done for
	 * any slot input that happens during
	 * input.
	 */
	hpts->p_wheel_complete = 1;
	/* 
	 * Run any input that may be there not covered
	 * in running data.
	 */
	if (!TAILQ_EMPTY(&hpts->p_input)) {
		tcp_input_data(hpts, &tv);
		/*
		 * Now did we spend too long running
		 * input and need to run more ticks?
		 */
		KASSERT(hpts->p_prev_slot == hpts->p_cur_slot,
			("H:%p p_prev_slot:%u not equal to p_cur_slot:%u", hpts,
			 hpts->p_prev_slot, hpts->p_cur_slot));
		KASSERT(hpts->p_lasttick == hpts->p_curtick,
			("H:%p p_lasttick:%u not equal to p_curtick:%u", hpts,
			 hpts->p_lasttick, hpts->p_curtick));
		hpts->p_curtick = tcp_gethptstick(&tv);
		if (hpts->p_lasttick != hpts->p_curtick) {
			counter_u64_add(hpts_loops, 1);
			hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
			goto again;
		}
	}
	{
		uint32_t t = 0, i, fnd = 0;

		if ((hpts->p_on_queue_cnt) && (wrap_loop_cnt < 2)) {
			/*
			 * Find next slot that is occupied and use that to
			 * be the sleep time.
			 */
			for (i = 0, t = hpts_tick(hpts->p_cur_slot, 1); i < NUM_OF_HPTSI_SLOTS; i++) {
				if (TAILQ_EMPTY(&hpts->p_hptss[t]) == 0) {
					fnd = 1;
					break;
				}
				t = (t + 1) % NUM_OF_HPTSI_SLOTS;
			}
			if (fnd) {
				hpts->p_hpts_sleep_time = min((i + 1), hpts_sleep_max);
			} else {
#ifdef INVARIANTS
				panic("Hpts:%p cnt:%d but none found", hpts, hpts->p_on_queue_cnt);
#endif
				counter_u64_add(back_tosleep, 1);
				hpts->p_on_queue_cnt = 0;
				goto non_found;
			}
		} else if (wrap_loop_cnt >= 2) {
			/* Special case handling */
			hpts->p_hpts_sleep_time = tcp_min_hptsi_time;
		} else {
			/* No one on the wheel sleep for all but 400 slots or sleep max  */
		non_found:
			hpts->p_hpts_sleep_time = hpts_sleep_max;
		}
	}
}

void
__tcp_set_hpts(struct inpcb *inp, int32_t line)
{
	struct tcp_hpts_entry *hpts;

	INP_WLOCK_ASSERT(inp);
	hpts = tcp_hpts_lock(inp);
	if ((inp->inp_in_hpts == 0) &&
	    (inp->inp_hpts_cpu_set == 0)) {
		inp->inp_hpts_cpu = hpts_cpuid(inp);
		inp->inp_hpts_cpu_set = 1;
	}
	mtx_unlock(&hpts->p_mtx);
	hpts = tcp_input_lock(inp);
	if ((inp->inp_input_cpu_set == 0) &&
	    (inp->inp_in_input == 0)) {
		inp->inp_input_cpu = hpts_cpuid(inp);
		inp->inp_input_cpu_set = 1;
	}
	mtx_unlock(&hpts->p_mtx);
}

uint16_t
tcp_hpts_delayedby(struct inpcb *inp){
	return (tcp_pace.rp_ent[inp->inp_hpts_cpu]->p_delayed_by);
}

static void
tcp_hpts_thread(void *ctx)
{
	struct tcp_hpts_entry *hpts;
	struct epoch_tracker et;
	struct timeval tv;
	sbintime_t sb;

	hpts = (struct tcp_hpts_entry *)ctx;
	mtx_lock(&hpts->p_mtx);
	if (hpts->p_direct_wake) {
		/* Signaled by input */
		callout_stop(&hpts->co);
	} else {
		/* Timed out */
		if (callout_pending(&hpts->co) ||
		    !callout_active(&hpts->co)) {
			mtx_unlock(&hpts->p_mtx);
			return;
		}
		callout_deactivate(&hpts->co);
	}
	hpts->p_hpts_wake_scheduled = 0;
	hpts->p_hpts_active = 1;
	NET_EPOCH_ENTER(et);
	tcp_hptsi(hpts);
	NET_EPOCH_EXIT(et);
	HPTS_MTX_ASSERT(hpts);
	tv.tv_sec = 0;
	tv.tv_usec = hpts->p_hpts_sleep_time * HPTS_TICKS_PER_USEC;
	if (tcp_min_hptsi_time && (tv.tv_usec < tcp_min_hptsi_time)) {
		hpts->overidden_sleep = tv.tv_usec;
		tv.tv_usec = tcp_min_hptsi_time;
		hpts->p_on_min_sleep = 1;
	} else {
		/* Clear the min sleep flag */
		hpts->overidden_sleep = 0;
		hpts->p_on_min_sleep = 0;
	}
	hpts->p_hpts_active = 0;
	sb = tvtosbt(tv);
	if (tcp_hpts_callout_skip_swi == 0) {
		callout_reset_sbt_on(&hpts->co, sb, 0,
		    hpts_timeout_swi, hpts, hpts->p_cpu,
		    (C_DIRECT_EXEC | C_PREL(tcp_hpts_precision)));
	} else {
		callout_reset_sbt_on(&hpts->co, sb, 0,
		    hpts_timeout_dir, hpts,
		    hpts->p_cpu,
		    C_PREL(tcp_hpts_precision));
	}
	hpts->p_direct_wake = 0;
	mtx_unlock(&hpts->p_mtx);
}

#undef	timersub

static void
tcp_init_hptsi(void *st)
{
	int32_t i, j, error, bound = 0, created = 0;
	size_t sz, asz;
	struct timeval tv;
	sbintime_t sb;
	struct tcp_hpts_entry *hpts;
	struct pcpu *pc;
	cpuset_t cs;
	char unit[16];
	uint32_t ncpus = mp_ncpus ? mp_ncpus : MAXCPU;
	int count, domain;

	tcp_pace.rp_proc = NULL;
	tcp_pace.rp_num_hptss = ncpus;
	hpts_hopelessly_behind = counter_u64_alloc(M_WAITOK);
	hpts_loops = counter_u64_alloc(M_WAITOK);
	back_tosleep = counter_u64_alloc(M_WAITOK);
	combined_wheel_wrap = counter_u64_alloc(M_WAITOK);
	wheel_wrap = counter_u64_alloc(M_WAITOK);
	sz = (tcp_pace.rp_num_hptss * sizeof(struct tcp_hpts_entry *));
	tcp_pace.rp_ent = malloc(sz, M_TCPHPTS, M_WAITOK | M_ZERO);
	asz = sizeof(struct hptsh) * NUM_OF_HPTSI_SLOTS;
	for (i = 0; i < tcp_pace.rp_num_hptss; i++) {
		tcp_pace.rp_ent[i] = malloc(sizeof(struct tcp_hpts_entry),
		    M_TCPHPTS, M_WAITOK | M_ZERO);
		tcp_pace.rp_ent[i]->p_hptss = malloc(asz,
		    M_TCPHPTS, M_WAITOK);
		hpts = tcp_pace.rp_ent[i];
		/*
		 * Init all the hpts structures that are not specifically
		 * zero'd by the allocations. Also lets attach them to the
		 * appropriate sysctl block as well.
		 */
		mtx_init(&hpts->p_mtx, "tcp_hpts_lck",
		    "hpts", MTX_DEF | MTX_DUPOK);
		TAILQ_INIT(&hpts->p_input);
		for (j = 0; j < NUM_OF_HPTSI_SLOTS; j++) {
			TAILQ_INIT(&hpts->p_hptss[j]);
		}
		sysctl_ctx_init(&hpts->hpts_ctx);
		sprintf(unit, "%d", i);
		hpts->hpts_root = SYSCTL_ADD_NODE(&hpts->hpts_ctx,
		    SYSCTL_STATIC_CHILDREN(_net_inet_tcp_hpts),
		    OID_AUTO,
		    unit,
		    CTLFLAG_RW, 0,
		    "");
		SYSCTL_ADD_INT(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "in_qcnt", CTLFLAG_RD,
		    &hpts->p_on_inqueue_cnt, 0,
		    "Count TCB's awaiting input processing");
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
		    &hpts->p_runningtick, 0,
		    "What the running pacers current slot is");
		SYSCTL_ADD_UINT(&hpts->hpts_ctx,
		    SYSCTL_CHILDREN(hpts->hpts_root),
		    OID_AUTO, "curtick", CTLFLAG_RD,
		    &hpts->p_curtick, 0,
		    "What the running pacers last tick mapped to the wheel was");
		hpts->p_hpts_sleep_time = hpts_sleep_max;
		hpts->p_num = i;
		hpts->p_curtick = tcp_gethptstick(&tv);
		hpts->p_prev_slot = hpts->p_cur_slot = tick_to_wheel(hpts->p_curtick);
		hpts->p_cpu = 0xffff;
		hpts->p_nxt_slot = hpts_tick(hpts->p_cur_slot, 1);
		callout_init(&hpts->co, 1);
	}

	/* Don't try to bind to NUMA domains if we don't have any */
	if (vm_ndomains == 1 && tcp_bind_threads == 2)
		tcp_bind_threads = 0;

	/*
	 * Now lets start ithreads to handle the hptss.
	 */
	CPU_FOREACH(i) {
		hpts = tcp_pace.rp_ent[i];
		hpts->p_cpu = i;
		error = swi_add(&hpts->ie, "hpts",
		    tcp_hpts_thread, (void *)hpts,
		    SWI_NET, INTR_MPSAFE, &hpts->ie_cookie);
		if (error) {
			panic("Can't add hpts:%p i:%d err:%d",
			    hpts, i, error);
		}
		created++;
		if (tcp_bind_threads == 1) {
			if (intr_event_bind(hpts->ie, i) == 0)
				bound++;
		} else if (tcp_bind_threads == 2) {
			pc = pcpu_find(i);
			domain = pc->pc_domain;
			CPU_COPY(&cpuset_domain[domain], &cs);
			if (intr_event_bind_ithread_cpuset(hpts->ie, &cs)
			    == 0) {
				bound++;
				count = hpts_domains[domain].count;
				hpts_domains[domain].cpu[count] = i;
				hpts_domains[domain].count++;
			}
		}
		tv.tv_sec = 0;
		tv.tv_usec = hpts->p_hpts_sleep_time * HPTS_TICKS_PER_USEC;
		sb = tvtosbt(tv);
		if (tcp_hpts_callout_skip_swi == 0) {
			callout_reset_sbt_on(&hpts->co, sb, 0,
			    hpts_timeout_swi, hpts, hpts->p_cpu,
			    (C_DIRECT_EXEC | C_PREL(tcp_hpts_precision)));
		} else {
			callout_reset_sbt_on(&hpts->co, sb, 0,
			    hpts_timeout_dir, hpts,
			    hpts->p_cpu,
			    C_PREL(tcp_hpts_precision));
		}
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
}

SYSINIT(tcphptsi, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, tcp_init_hptsi, NULL);
MODULE_VERSION(tcphpts, 1);
