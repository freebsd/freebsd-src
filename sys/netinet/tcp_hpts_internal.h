/*-
 * Copyright (c) 2025 Netflix, Inc.
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
 */

#ifndef __tcp_hpts_internal_h__
#define __tcp_hpts_internal_h__

/*
 * TCP High Precision Timer System (HPTS) - Internal Definitions
 *
 * This header contains internal structures, constants, and interfaces that are
 * implemented in tcp_hpts.c but exposed to enable comprehensive unit testing of
 * the HPTS subsystem.
 */

#if defined(_KERNEL)

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

/* The number of connections after which the dynamic sleep logic kicks in. */
#define DEFAULT_CONNECTION_THRESHOLD 100

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

/* Convert microseconds to HPTS slots */
#define HPTS_USEC_TO_SLOTS(x) ((x+9) /10)

/* The number of connections after which the dynamic sleep logic kicks in. */
#define DEFAULT_CONNECTION_THRESHOLD 100

extern int tcp_bind_threads; 		/* Thread binding configuration
					 * (0=none, 1=cpu, 2=numa) */

/*
 * Abstraction layer controlling time, interrupts and callouts.
 */
struct tcp_hptsi_funcs {
	void (*microuptime)(struct timeval *tv);
	int (*swi_add)(struct intr_event **eventp, const char *name,
		driver_intr_t handler, void *arg, int pri, enum intr_type flags,
		void **cookiep);
	int (*swi_remove)(void *cookie);
	void (*swi_sched)(void *cookie, int flags);
	int (*intr_event_bind)(struct intr_event *ie, int cpu);
	int (*intr_event_bind_ithread_cpuset)(struct intr_event *ie,
		struct _cpuset *mask);
	void (*callout_init)(struct callout *c, int mpsafe);
	int (*callout_reset_sbt_on)(struct callout *c, sbintime_t sbt,
		sbintime_t precision, void (*func)(void *), void *arg, int cpu,
		int flags);
	int (*_callout_stop_safe)(struct callout *c, int flags);
};

/* Default function table for system operation */
extern const struct tcp_hptsi_funcs tcp_hptsi_default_funcs;

/* Each hpts has its own p_mtx which is used for locking */
#define	HPTS_MTX_ASSERT(hpts)	mtx_assert(&(hpts)->p_mtx, MA_OWNED)
#define	HPTS_LOCK(hpts)		mtx_lock(&(hpts)->p_mtx)
#define	HPTS_TRYLOCK(hpts)	mtx_trylock(&(hpts)->p_mtx)
#define	HPTS_UNLOCK(hpts)	mtx_unlock(&(hpts)->p_mtx)

struct tcp_hpts_entry {
	/* Cache line 0x00 */
	struct mtx p_mtx;		/* Mutex for hpts */
	struct timeval p_mysleep;	/* Our min sleep time */
	uint64_t syscall_cnt;
	uint64_t sleeping;		/* What the actual sleep was (if sleeping) */
	uint16_t p_hpts_active; 	/* Flag that says hpts is awake  */
	uint8_t p_wheel_complete; 	/* have we completed the wheel arc walk? */
	uint32_t p_runningslot; 	/* Current slot we are at if we are running */
	uint32_t p_prev_slot;		/* Previous slot we were on */
	uint32_t p_cur_slot;		/* Current slot in wheel hpts is draining */
	uint32_t p_nxt_slot;		/* The next slot outside the current range
					 * of slots that the hpts is running on. */
	int32_t p_on_queue_cnt;		/* Count on queue in this hpts */
	uint8_t p_direct_wake :1, 	/* boolean */
		p_on_min_sleep:1, 	/* boolean */
		p_hpts_wake_scheduled:1,/* boolean */
		hit_callout_thresh:1,
		p_avail:4;
	uint8_t p_fill[3];		/* Fill to 32 bits */
	/* Cache line 0x40 */
	struct hptsh {
		TAILQ_HEAD(, tcpcb)	head;
		uint32_t		count;
		uint32_t		gencnt;
	} *p_hptss;			/* Hptsi wheel */
	uint32_t p_hpts_sleep_time;	/* Current sleep interval having a max
					 * of 255ms */
	uint32_t overidden_sleep;	/* what was overrided by min-sleep for logging */
	uint32_t saved_curslot;		/* for logging */
	uint32_t saved_prev_slot;	/* for logging */
	uint32_t p_delayed_by;		/* How much were we delayed by */
	/* Cache line 0x80 */
	struct sysctl_ctx_list hpts_ctx;
	struct sysctl_oid *hpts_root;
	struct intr_event *ie;
	void *ie_cookie;
	uint16_t p_cpu;			/* The hpts CPU */
	struct tcp_hptsi *p_hptsi;	/* Back pointer to parent hptsi structure */
	/* There is extra space in here */
	/* Cache line 0x100 */
	struct callout co __aligned(CACHE_LINE_SIZE);
}               __aligned(CACHE_LINE_SIZE);

struct tcp_hptsi {
	struct cpu_group **grps;
	struct tcp_hpts_entry **rp_ent;	/* Array of hptss */
	uint32_t *cts_last_ran;
	uint32_t grp_cnt;
	uint32_t rp_num_hptss;		/* Number of hpts threads */
	struct hpts_domain_info {
		int count;
		int cpu[MAXCPU];
	} domains[MAXMEMDOM];		/* Per-NUMA domain CPU assignments */
	const struct tcp_hptsi_funcs *funcs;	/* Function table for testability */
};

/*
 * Core tcp_hptsi structure manipulation functions.
 */
struct tcp_hptsi* tcp_hptsi_create(const struct tcp_hptsi_funcs *funcs,
	bool enable_sysctl);
void tcp_hptsi_destroy(struct tcp_hptsi *pace);
void tcp_hptsi_start(struct tcp_hptsi *pace);
void tcp_hptsi_stop(struct tcp_hptsi *pace);
uint16_t tcp_hptsi_random_cpu(struct tcp_hptsi *pace);
int32_t tcp_hptsi(struct tcp_hpts_entry *hpts, bool from_callout);

void tcp_hpts_wake(struct tcp_hpts_entry *hpts);

/*
 * LRO HPTS initialization and uninitialization, only for internal use by the
 * HPTS code.
 */
void tcp_lro_hpts_init(void);
void tcp_lro_hpts_uninit(void);

#endif /* defined(_KERNEL) */
#endif /* __tcp_hpts_internal_h__ */
