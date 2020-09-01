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
 * $FreeBSD$
 */

#ifndef __tcp_hpts_h__
#define __tcp_hpts_h__

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

TAILQ_HEAD(hptsh, inpcb);

/* Number of useconds in a hpts tick */
#define HPTS_TICKS_PER_USEC 10
#define HPTS_MS_TO_SLOTS(x) ((x * 100) + 1)
#define HPTS_USEC_TO_SLOTS(x) ((x+9) /10)
#define HPTS_USEC_IN_SEC 1000000
#define HPTS_MSEC_IN_SEC 1000
#define HPTS_USEC_IN_MSEC 1000

struct hpts_diag {
	uint32_t p_hpts_active; 	/* bbr->flex7 x */
	uint32_t p_nxt_slot;		/* bbr->flex1 x */
	uint32_t p_cur_slot;		/* bbr->flex2 x */
	uint32_t p_prev_slot;		/* bbr->delivered */
	uint32_t p_runningtick;		/* bbr->inflight */
	uint32_t slot_req;		/* bbr->flex3 x */
	uint32_t inp_hptsslot;		/* bbr->flex4 x */
	uint32_t slot_remaining;	/* bbr->flex5 x */
	uint32_t have_slept;		/* bbr->epoch x */
	uint32_t hpts_sleep_time;	/* bbr->applimited x */
	uint32_t yet_to_sleep;		/* bbr->lt_epoch x */
	uint32_t need_new_to;		/* bbr->flex6 x  */
	uint32_t wheel_tick;		/* bbr->bw_inuse x */
	uint32_t maxticks;		/* bbr->delRate x */
	uint32_t wheel_cts;		/* bbr->rttProp x */
	int32_t co_ret; 		/* bbr->pkts_out x */
	uint32_t p_curtick;		/* upper bbr->cur_del_rate */
	uint32_t p_lasttick;		/* lower bbr->cur_del_rate */
	uint8_t p_on_min_sleep; 	/* bbr->flex8 x */
};

/* Magic flags to tell whats cooking on the pacing wheel */
#define PACE_TMR_DELACK 0x01	/* Delayed ack timer running */
#define PACE_TMR_RACK   0x02	/* RACK timer running */
#define PACE_TMR_TLP    0x04	/* TLP timer running */
#define PACE_TMR_RXT    0x08	/* Retransmit timer running */
#define PACE_TMR_PERSIT 0x10	/* Persists timer running */
#define PACE_TMR_KEEP   0x20	/* Keep alive timer running */
#define PACE_PKT_OUTPUT 0x40	/* Output Packets being paced */
#define PACE_TMR_MASK   (PACE_TMR_KEEP|PACE_TMR_PERSIT|PACE_TMR_RXT|PACE_TMR_TLP|PACE_TMR_RACK|PACE_TMR_DELACK)

#ifdef _KERNEL
/* Each hpts has its own p_mtx which is used for locking */
struct tcp_hpts_entry {
	/* Cache line 0x00 */
	struct mtx p_mtx;	/* Mutex for hpts */
	uint16_t p_hpts_active; /* Flag that says hpts is awake  */
	uint8_t p_hpts_wake_scheduled;	/* Have we scheduled a wakeup? */
	uint8_t p_wheel_complete; /* have we completed the wheel arc walk? */
	uint32_t p_curtick;	/* Tick in 10 us the hpts is going to */
	uint32_t p_runningtick; /* Current tick we are at if we are running */
	uint32_t p_prev_slot;	/* Previous slot we were on */
	uint32_t p_cur_slot;	/* Current slot in wheel hpts is draining */
	uint32_t p_nxt_slot;	/* The next slot outside the current range of
				 * slots that the hpts is running on. */
	int32_t p_on_queue_cnt;	/* Count on queue in this hpts */
	uint32_t p_lasttick;	/* Last tick before the current one */
	uint8_t p_direct_wake :1, /* boolean */
		p_on_min_sleep:1, /* boolean */
		p_avail:6;
	uint8_t p_fill[3];	  /* Fill to 32 bits */
	/* Cache line 0x40 */
	void *p_inp;
	struct hptsh p_input;	/* For the tcp-input runner */
	/* Hptsi wheel */
	struct hptsh *p_hptss;
	int32_t p_on_inqueue_cnt; /* Count on input queue in this hpts */
	uint32_t hit_no_enobuf;
	uint32_t p_dyn_adjust;
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

struct tcp_hptsi {
	struct proc *rp_proc;	/* Process structure for hpts */
	struct tcp_hpts_entry **rp_ent;	/* Array of hptss */
	uint32_t rp_num_hptss;	/* Number of hpts threads */
};

#endif

#define HPTS_REMOVE_INPUT  0x01
#define HPTS_REMOVE_OUTPUT 0x02
#define HPTS_REMOVE_ALL    (HPTS_REMOVE_INPUT | HPTS_REMOVE_OUTPUT)

/*
 * When using the hpts, a TCP stack must make sure
 * that once a INP_DROPPED flag is applied to a INP
 * that it does not expect tcp_output() to ever be
 * called by the hpts. The hpts will *not* call
 * any output (or input) functions on a TCB that
 * is in the DROPPED state.
 *
 * This implies final ACK's and RST's that might
 * be sent when a TCB is still around must be
 * sent from a routine like tcp_respond().
 */
#define DEFAULT_MIN_SLEEP 250	/* How many usec's is default for hpts sleep
				 * this determines min granularity of the
				 * hpts. If 0, granularity is 10useconds at
				 * the cost of more CPU (context switching). */
#ifdef _KERNEL
#define HPTS_MTX_ASSERT(hpts) mtx_assert(&(hpts)->p_mtx, MA_OWNED)
struct tcp_hpts_entry *tcp_hpts_lock(struct inpcb *inp);
struct tcp_hpts_entry *tcp_input_lock(struct inpcb *inp);
int __tcp_queue_to_hpts_immediate(struct inpcb *inp, int32_t line);
#define tcp_queue_to_hpts_immediate(a)__tcp_queue_to_hpts_immediate(a, __LINE__)

struct tcp_hpts_entry *tcp_cur_hpts(struct inpcb *inp);
#define tcp_hpts_remove(a, b) __tcp_hpts_remove(a, b, __LINE__)
void __tcp_hpts_remove(struct inpcb *inp, int32_t flags, int32_t line);

/*
 * To insert a TCB on the hpts you *must* be holding the
 * INP_WLOCK(). The hpts insert code will then acqurire
 * the hpts's lock and insert the TCB on the requested
 * slot possibly waking up the hpts if you are requesting
 * a time earlier than what the hpts is sleeping to (if
 * the hpts is sleeping). You may check the inp->inp_in_hpts
 * flag without the hpts lock. The hpts is the only one
 * that will clear this flag holding only the hpts lock. This
 * means that in your tcp_output() routine when you test for
 * it to be 1 (so you wont call output) it may be transitioning
 * to 0 (by the hpts). That will be fine since that will just
 * mean an extra call to tcp_output that most likely will find
 * the call you executed (when the mis-match occured) will have
 * put the TCB back on the hpts and it will return. If your
 * call did not add it back to the hpts then you will either
 * over-send or the cwnd will block you from sending more.
 *
 * Note you should also be holding the INP_WLOCK() when you
 * call the remove from the hpts as well. Thoug usually
 * you are either doing this from a timer, where you need
 * that INP_WLOCK() or from destroying your TCB where again
 * you should already have the INP_WLOCK().
 */
uint32_t __tcp_hpts_insert(struct inpcb *inp, uint32_t slot, int32_t line);
#define tcp_hpts_insert(a, b) __tcp_hpts_insert(a, b, __LINE__)

uint32_t
tcp_hpts_insert_diag(struct inpcb *inp, uint32_t slot, int32_t line, struct hpts_diag *diag);

int
    __tcp_queue_to_input_locked(struct inpcb *inp, struct tcp_hpts_entry *hpts, int32_t line);
#define tcp_queue_to_input_locked(a, b) __tcp_queue_to_input_locked(a, b, __LINE__);
int
__tcp_queue_to_input(struct inpcb *inp, int32_t line);
#define tcp_queue_to_input(a) __tcp_queue_to_input(a, __LINE__)

uint16_t tcp_hpts_delayedby(struct inpcb *inp);

void __tcp_set_hpts(struct inpcb *inp, int32_t line);
#define tcp_set_hpts(a) __tcp_set_hpts(a, __LINE__)

void __tcp_set_inp_to_drop(struct inpcb *inp, uint16_t reason, int32_t line);
#define tcp_set_inp_to_drop(a, b) __tcp_set_inp_to_drop(a, b, __LINE__)

extern int32_t tcp_min_hptsi_time;

static __inline uint32_t
tcp_tv_to_hptstick(struct timeval *sv)
{
	return ((sv->tv_sec * 100000) + (sv->tv_usec / 10));
}

static __inline uint32_t
tcp_gethptstick(struct timeval *sv)
{
	struct timeval tv;

	if (sv == NULL)
		sv = &tv;
	microuptime(sv);
	return (tcp_tv_to_hptstick(sv));
}

static __inline uint32_t
tcp_tv_to_usectick(struct timeval *sv)
{
	return ((uint32_t) ((sv->tv_sec * HPTS_USEC_IN_SEC) + sv->tv_usec));
}

static __inline uint32_t
tcp_tv_to_mssectick(struct timeval *sv)
{
	return ((uint32_t) ((sv->tv_sec * HPTS_MSEC_IN_SEC) + (sv->tv_usec/HPTS_USEC_IN_MSEC)));
}

static __inline void
tcp_hpts_unlock(struct tcp_hpts_entry *hpts)
{
	mtx_unlock(&hpts->p_mtx);
}

static __inline uint32_t
tcp_get_usecs(struct timeval *tv)
{
	struct timeval tvd;

	if (tv == NULL)
		tv = &tvd;
	microuptime(tv);
	return (tcp_tv_to_usectick(tv));
}

#endif /* _KERNEL */
#endif /* __tcp_hpts_h__ */
