/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <tests/ktest.h>
#include "opt_inet.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_hpts_internal.h>
#include <dev/tcp_log/tcp_log_dev.h>
#include <netinet/tcp_log_buf.h>

#undef tcp_hpts_init
#undef tcp_hpts_remove
#undef tcp_hpts_insert
#undef tcp_set_hpts

/* Custom definitions that take the tcp_hptsi */
#define tcp_hpts_init(pace, tp) __tcp_hpts_init((pace), (tp))
#define tcp_hpts_remove(pace, tp) __tcp_hpts_remove((pace), (tp))
#define	tcp_hpts_insert(pace, tp, usecs, diag)	\
	__tcp_hpts_insert((pace), (tp), (usecs), (diag))
#define tcp_set_hpts(pace, tp) __tcp_set_hpts((pace), (tp))

static MALLOC_DEFINE(M_TCPHPTS, "tcp_hpts_test", "TCP hpts test");

static int test_exit_on_failure = true;
SYSCTL_NODE(_net_inet_tcp, OID_AUTO, hpts_test, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP HPTS test controls");
SYSCTL_INT(_net_inet_tcp_hpts_test, OID_AUTO, exit_on_failure, CTLFLAG_RW,
    &test_exit_on_failure, 0,
    "Exit HPTS test immediately on first failure (1) or continue running all tests (0)");

#define KTEST_VERIFY(x) do { \
	if (!(x)) { \
		KTEST_ERR(ctx, "FAIL: %s", #x); \
		if (test_exit_on_failure) \
			return (EINVAL); \
	} else { \
		KTEST_LOG(ctx, "PASS: %s", #x); \
	} \
} while (0)

#define KTEST_EQUAL(x, y) do { \
	if ((x) != (y)) { \
		KTEST_ERR(ctx, "FAIL: %s != %s (%d != %d)", #x, #y, (x), (y)); \
		if (test_exit_on_failure) \
			return (EINVAL); \
	} else { \
		KTEST_LOG(ctx, "PASS: %s == %s", #x, #y); \
	} \
} while (0)

#define KTEST_NEQUAL(x, y) do { \
	if ((x) == (y)) { \
		KTEST_ERR(ctx, "FAIL: %s == %s (%d == %d)", #x, #y, (x), (y)); \
		if (test_exit_on_failure) \
			return (EINVAL); \
	} else { \
		KTEST_LOG(ctx, "PASS: %s != %s", #x, #y); \
	} \
} while (0)

#define KTEST_GREATER_THAN(x, y) do { \
	if ((x) <= (y)) { \
		KTEST_ERR(ctx, "FAIL: %s <= %s (%d <= %d)", #x, #y, (x), (y)); \
		if (test_exit_on_failure) \
			return (EINVAL); \
	} else { \
		KTEST_LOG(ctx, "PASS: %s > %s", #x, #y); \
	} \
} while (0)

#define KTEST_VERIFY_RET(x, y) do { \
	if (!(x)) { \
		KTEST_ERR(ctx, "FAIL: %s", #x); \
		if (test_exit_on_failure) \
			return (y); \
	} else { \
		KTEST_LOG(ctx, "PASS: %s", #x); \
	} \
} while (0)

#ifdef TCP_HPTS_KTEST

static void
dump_hpts_entry(struct ktest_test_context *ctx, struct tcp_hpts_entry *hpts)
{
	KTEST_LOG(ctx, "tcp_hpts_entry(%p)", hpts);
	KTEST_LOG(ctx, "  p_cur_slot: %u", hpts->p_cur_slot);
	KTEST_LOG(ctx, "  p_prev_slot: %u", hpts->p_prev_slot);
	KTEST_LOG(ctx, "  p_nxt_slot: %u", hpts->p_nxt_slot);
	KTEST_LOG(ctx, "  p_runningslot: %u", hpts->p_runningslot);
	KTEST_LOG(ctx, "  p_on_queue_cnt: %d", hpts->p_on_queue_cnt);
	KTEST_LOG(ctx, "  p_hpts_active: %u", hpts->p_hpts_active);
	KTEST_LOG(ctx, "  p_wheel_complete: %u", hpts->p_wheel_complete);
	KTEST_LOG(ctx, "  p_direct_wake: %u", hpts->p_direct_wake);
	KTEST_LOG(ctx, "  p_on_min_sleep: %u", hpts->p_on_min_sleep);
	KTEST_LOG(ctx, "  p_hpts_wake_scheduled: %u", hpts->p_hpts_wake_scheduled);
	KTEST_LOG(ctx, "  hit_callout_thresh: %u", hpts->hit_callout_thresh);
	KTEST_LOG(ctx, "  p_hpts_sleep_time: %u", hpts->p_hpts_sleep_time);
	KTEST_LOG(ctx, "  p_delayed_by: %u", hpts->p_delayed_by);
	KTEST_LOG(ctx, "  overidden_sleep: %u", hpts->overidden_sleep);
	KTEST_LOG(ctx, "  saved_curslot: %u", hpts->saved_curslot);
	KTEST_LOG(ctx, "  saved_prev_slot: %u", hpts->saved_prev_slot);
	KTEST_LOG(ctx, "  syscall_cnt: %lu", hpts->syscall_cnt);
	KTEST_LOG(ctx, "  sleeping: %lu", hpts->sleeping);
	KTEST_LOG(ctx, "  p_cpu: %u", hpts->p_cpu);
	KTEST_LOG(ctx, "  ie_cookie: %p", hpts->ie_cookie);
	KTEST_LOG(ctx, "  p_hptsi: %p", hpts->p_hptsi);
	KTEST_LOG(ctx, "  p_mysleep: %ld.%06ld", hpts->p_mysleep.tv_sec, hpts->p_mysleep.tv_usec);
}

static void
dump_tcpcb(struct tcpcb *tp)
{
	struct ktest_test_context *ctx = tp->t_fb_ptr;
	struct inpcb *inp = &tp->t_inpcb;

	KTEST_LOG(ctx, "tcp_control_block(%p)", tp);

	/* HPTS-specific fields */
	KTEST_LOG(ctx, "  t_in_hpts: %d", tp->t_in_hpts);
	KTEST_LOG(ctx, "  t_hpts_cpu: %u", tp->t_hpts_cpu);
	KTEST_LOG(ctx, "  t_hpts_slot: %d", tp->t_hpts_slot);
	KTEST_LOG(ctx, "  t_hpts_gencnt: %u", tp->t_hpts_gencnt);
	KTEST_LOG(ctx, "  t_hpts_request: %u", tp->t_hpts_request);

	/* LRO CPU field */
	KTEST_LOG(ctx, "  t_lro_cpu: %u", tp->t_lro_cpu);

	/* TCP flags that affect HPTS */
	KTEST_LOG(ctx, "  t_flags2: 0x%x", tp->t_flags2);
	KTEST_LOG(ctx, "    TF2_HPTS_CPU_SET: %s", (tp->t_flags2 & TF2_HPTS_CPU_SET) ? "YES" : "NO");
	KTEST_LOG(ctx, "    TF2_HPTS_CALLS: %s", (tp->t_flags2 & TF2_HPTS_CALLS) ? "YES" : "NO");
	KTEST_LOG(ctx, "    TF2_SUPPORTS_MBUFQ: %s", (tp->t_flags2 & TF2_SUPPORTS_MBUFQ) ? "YES" : "NO");

	/* Input PCB fields that HPTS uses */
	KTEST_LOG(ctx, "  inp_flags: 0x%x", inp->inp_flags);
	KTEST_LOG(ctx, "    INP_DROPPED: %s", (inp->inp_flags & INP_DROPPED) ? "YES" : "NO");
	KTEST_LOG(ctx, "  inp_flowid: 0x%x", inp->inp_flowid);
	KTEST_LOG(ctx, "  inp_flowtype: %u", inp->inp_flowtype);
	KTEST_LOG(ctx, "  inp_numa_domain: %d", inp->inp_numa_domain);
}

/* Enum for call counting indices */
enum test_call_counts {
	CCNT_MICROUPTIME = 0,
	CCNT_SWI_ADD,
	CCNT_SWI_REMOVE,
	CCNT_SWI_SCHED,
	CCNT_INTR_EVENT_BIND,
	CCNT_INTR_EVENT_BIND_CPUSET,
	CCNT_CALLOUT_INIT,
	CCNT_CALLOUT_RESET_SBT_ON,
	CCNT_CALLOUT_STOP_SAFE,
	CCNT_TCP_OUTPUT,
	CCNT_TCP_TFB_DO_QUEUED_SEGMENTS,
	CCNT_MAX
};

static uint32_t call_counts[CCNT_MAX];

static uint64_t test_time_usec = 0;

/*
 * Reset all test global variables to a clean state.
 */
static void
test_hpts_init(void)
{
	memset(call_counts, 0, sizeof(call_counts));
	test_time_usec = 0;
}

static void
test_microuptime(struct timeval *tv)
{
	call_counts[CCNT_MICROUPTIME]++;
	tv->tv_sec = test_time_usec / 1000000;
	tv->tv_usec = test_time_usec % 1000000;
}

static int
test_swi_add(struct intr_event **eventp, const char *name,
    driver_intr_t handler, void *arg, int pri, enum intr_type flags,
    void **cookiep)
{
	call_counts[CCNT_SWI_ADD]++;
	/* Simulate successful SWI creation */
	*eventp = (struct intr_event *)0xfeedface; /* Mock event */
	*cookiep = (void *)0xdeadbeef; /* Mock cookie */
	return (0);
}

static int
test_swi_remove(void *cookie)
{
	call_counts[CCNT_SWI_REMOVE]++;
	/* Simulate successful removal */
	return (0);
}

static void
test_swi_sched(void *cookie, int flags)
{
	call_counts[CCNT_SWI_SCHED]++;
	/* Simulate successful SWI scheduling */
}

static int
test_intr_event_bind(struct intr_event *ie, int cpu)
{
	call_counts[CCNT_INTR_EVENT_BIND]++;
	/* Simulate successful binding */
	return (0);
}

static int
test_intr_event_bind_ithread_cpuset(struct intr_event *ie, struct _cpuset *mask)
{
	call_counts[CCNT_INTR_EVENT_BIND_CPUSET]++;
	/* Simulate successful cpuset binding */
	return (0);
}

static void
test_callout_init(struct callout *c, int mpsafe)
{
	call_counts[CCNT_CALLOUT_INIT]++;
	memset(c, 0, sizeof(*c));
}

static int
test_callout_reset_sbt_on(struct callout *c, sbintime_t sbt, sbintime_t precision,
    void (*func)(void *), void *arg, int cpu, int flags)
{
	call_counts[CCNT_CALLOUT_RESET_SBT_ON]++;
	/* Return 1 to simulate successful timer scheduling */
	return (1);
}

static int
test_callout_stop_safe(struct callout *c, int flags)
{
	call_counts[CCNT_CALLOUT_STOP_SAFE]++;
	/* Return 1 to simulate successful timer stopping */
	return (1);
}

static const struct tcp_hptsi_funcs test_funcs = {
	.microuptime = test_microuptime,
	.swi_add = test_swi_add,
	.swi_remove = test_swi_remove,
	.swi_sched = test_swi_sched,
	.intr_event_bind = test_intr_event_bind,
	.intr_event_bind_ithread_cpuset = test_intr_event_bind_ithread_cpuset,
	.callout_init = test_callout_init,
	.callout_reset_sbt_on = test_callout_reset_sbt_on,
	._callout_stop_safe = test_callout_stop_safe,
};

#define TP_REMOVE_FROM_HPTS(tp) tp->bits_spare
#define TP_LOG_TEST(tp) tp->t_log_state_set

static int
test_tcp_output(struct tcpcb *tp)
{
	struct ktest_test_context *ctx = tp->t_fb_ptr;
	struct tcp_hptsi *pace = (struct tcp_hptsi*)tp->t_tfo_pending;
	struct tcp_hpts_entry *hpts = pace->rp_ent[tp->t_hpts_cpu];

	call_counts[CCNT_TCP_OUTPUT]++;
	if (TP_LOG_TEST(tp)) {
		KTEST_LOG(ctx, "=> tcp_output(%p)", tp);
		dump_tcpcb(tp);
		dump_hpts_entry(ctx, hpts);
	}

	if ((TP_REMOVE_FROM_HPTS(tp) & 1) != 0) {
		if (TP_LOG_TEST(tp))
			KTEST_LOG(ctx, "=> tcp_hpts_remove(%p)", tp);
		tcp_hpts_remove(pace, tp);
	}

	if ((TP_REMOVE_FROM_HPTS(tp) & 2) != 0) {
		INP_WUNLOCK(&tp->t_inpcb); /* tcp_output unlocks on error */
		return (-1); /* Simulate tcp_output error */
	}

	return (0);
}

static int
test_tfb_do_queued_segments(struct tcpcb *tp, int flag)
{
	struct ktest_test_context *ctx = tp->t_fb_ptr;
	struct tcp_hptsi *pace = (struct tcp_hptsi*)tp->t_tfo_pending;
	struct tcp_hpts_entry *hpts = pace->rp_ent[tp->t_hpts_cpu];

	call_counts[CCNT_TCP_TFB_DO_QUEUED_SEGMENTS]++;
	KTEST_LOG(ctx, "=> tfb_do_queued_segments(%p, %d)", tp, flag);
	dump_tcpcb(tp);
	dump_hpts_entry(ctx, hpts);

	if ((TP_REMOVE_FROM_HPTS(tp) & 1) != 0) {
		if (TP_LOG_TEST(tp))
			KTEST_LOG(ctx, "=> tcp_hpts_remove(%p)", tp);
		tcp_hpts_remove(pace, tp);
	}

	if ((TP_REMOVE_FROM_HPTS(tp) & 2) != 0) {
		INP_WUNLOCK(&tp->t_inpcb); /* do_queued_segments unlocks on error */
		return (-1); /* Simulate do_queued_segments error */
	}

	return (0);
}

static struct tcp_function_block test_tcp_fb = {
	.tfb_tcp_block_name = "hpts_test_tcp",
	.tfb_tcp_output = test_tcp_output,
	.tfb_do_queued_segments = test_tfb_do_queued_segments,
};

/*
 * Create a minimally initialized tcpcb that can be safely inserted into HPTS.
 * This function allocates and initializes all the fields that HPTS code
 * reads or writes.
 */
static struct tcpcb *
test_hpts_create_tcpcb(struct ktest_test_context *ctx, struct tcp_hptsi *pace)
{
	struct tcpcb *tp;

	tp = malloc(sizeof(struct tcpcb), M_TCPHPTS, M_WAITOK | M_ZERO);
	if (tp) {
		rw_init_flags(&tp->t_inpcb.inp_lock, "test-inp",
			RW_RECURSE | RW_DUPOK);
		refcount_init(&tp->t_inpcb.inp_refcount, 1);
		tp->t_inpcb.inp_pcbinfo = &V_tcbinfo;
		tp->t_fb = &test_tcp_fb;
		tp->t_hpts_cpu = HPTS_CPU_NONE;
		STAILQ_INIT(&tp->t_inqueue);
		tcp_hpts_init(pace, tp);

		/* Stuff some pointers in the tcb for test purposes. */
		tp->t_fb_ptr = ctx;
		tp->t_tfo_pending = (unsigned int*)pace;
	}

	return (tp);
}

/*
 * Free a test tcpcb created by test_hpts_create_tcpcb()
 */
static void
test_hpts_free_tcpcb(struct tcpcb *tp)
{
	if (tp == NULL)
		return;

	INP_LOCK_DESTROY(&tp->t_inpcb);
	free(tp, M_TCPHPTS);
}

/*
 * ***********************************************
 * * KTEST functions for testing the HPTS module *
 * ***********************************************
 */

/*
 * Validates that the HPTS module is properly loaded and initialized by checking
 * that the minimum HPTS time is configured.
 */
KTEST_FUNC(module_load)
{
	test_hpts_init();
	KTEST_NEQUAL(tcp_min_hptsi_time, 0);
	KTEST_VERIFY(tcp_bind_threads >= 0 && tcp_bind_threads <= 2);
	KTEST_NEQUAL(tcp_hptsi_pace, NULL);
	return (0);
}

/*
 * Validates the creation and destruction of tcp_hptsi structures, ensuring
 * proper initialization of internal fields and clean destruction.
 */
KTEST_FUNC(hptsi_create_destroy)
{
	struct tcp_hptsi *pace;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	KTEST_NEQUAL(pace->rp_ent, NULL);
	KTEST_NEQUAL(pace->cts_last_ran, NULL);
	KTEST_VERIFY(pace->rp_num_hptss > 0);
	KTEST_VERIFY(pace->rp_num_hptss <= MAXCPU); /* Reasonable upper bound */
	KTEST_VERIFY(pace->grp_cnt >= 1); /* At least one group */
	KTEST_EQUAL(pace->funcs, &test_funcs); /* Verify function pointer was set */

	/* Verify individual HPTS entries are properly initialized */
	for (uint32_t i = 0; i < pace->rp_num_hptss; i++) {
		KTEST_NEQUAL(pace->rp_ent[i], NULL);
		KTEST_EQUAL(pace->rp_ent[i]->p_cpu, i);
		KTEST_EQUAL(pace->rp_ent[i]->p_hptsi, pace);
		KTEST_EQUAL(pace->rp_ent[i]->p_on_queue_cnt, 0);
	}

	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates that tcp_hptsi structures can be started and stopped properly,
 * including verification that threads are created during start and cleaned up
 * during stop operations.
 */
KTEST_FUNC(hptsi_start_stop)
{
	struct tcp_hptsi *pace;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);

	tcp_hptsi_start(pace);

	/* Verify that entries have threads started */
	struct tcp_hpts_entry *hpts = pace->rp_ent[0];
	KTEST_NEQUAL(hpts->ie_cookie, NULL);  /* Should have SWI handler */
	KTEST_EQUAL(hpts->p_hptsi, pace);     /* Should point to our pace */

	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates that multiple tcp_hptsi instances can coexist independently, with
 * different configurations and CPU assignments without interfering with each
 * other.
 */
KTEST_FUNC(hptsi_independence)
{
	struct tcp_hptsi *pace1, *pace2;
	uint16_t cpu1, cpu2;

	test_hpts_init();

	pace1 = tcp_hptsi_create(&test_funcs, false);
	pace2 = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace1, NULL);
	KTEST_NEQUAL(pace2, NULL);
	KTEST_NEQUAL(pace2->rp_ent, NULL);

	cpu1 = tcp_hptsi_random_cpu(pace1);
	cpu2 = tcp_hptsi_random_cpu(pace2);
	KTEST_VERIFY(cpu1 < pace1->rp_num_hptss);
	KTEST_VERIFY(cpu2 < pace2->rp_num_hptss);

	/* Verify both instances have independent entry arrays */
	KTEST_NEQUAL(pace1->rp_ent, pace2->rp_ent);
	/* Verify they may have different CPU counts but both reasonable */
	KTEST_VERIFY(pace1->rp_num_hptss > 0 && pace1->rp_num_hptss <= MAXCPU);
	KTEST_VERIFY(pace2->rp_num_hptss > 0 && pace2->rp_num_hptss <= MAXCPU);

	tcp_hptsi_destroy(pace1);
	tcp_hptsi_destroy(pace2);

	return (0);
}

/*
 * Validates that custom function injection works correctly, ensuring that
 * test-specific implementations of microuptime and others are properly
 * called by the HPTS system.
 */
KTEST_FUNC(function_injection)
{
	struct tcp_hptsi *pace;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	KTEST_EQUAL(pace->funcs, &test_funcs);
	KTEST_VERIFY(call_counts[CCNT_MICROUPTIME] > 0);
	KTEST_VERIFY(call_counts[CCNT_CALLOUT_INIT] > 0);

	tcp_hptsi_start(pace);
	KTEST_VERIFY(call_counts[CCNT_SWI_ADD] > 0);
	KTEST_VERIFY(tcp_bind_threads == 0 ||
	    call_counts[CCNT_INTR_EVENT_BIND] > 0 ||
	    call_counts[CCNT_INTR_EVENT_BIND_CPUSET] > 0);
	KTEST_VERIFY(call_counts[CCNT_CALLOUT_RESET_SBT_ON] > 0);

	tcp_hptsi_stop(pace);
	KTEST_VERIFY(call_counts[CCNT_CALLOUT_STOP_SAFE] > 0);
	KTEST_VERIFY(call_counts[CCNT_SWI_REMOVE] > 0);

	tcp_hptsi_destroy(pace);

	/* Verify we have a reasonable balance of create/destroy calls */
	KTEST_EQUAL(call_counts[CCNT_SWI_ADD], call_counts[CCNT_SWI_REMOVE]);
	KTEST_VERIFY(call_counts[CCNT_CALLOUT_RESET_SBT_ON] <= call_counts[CCNT_CALLOUT_STOP_SAFE]);

	return (0);
}

/*
 * Validates that a tcpcb can be properly initialized for HPTS compatibility,
 * ensuring all required fields are set correctly and function pointers are
 * valid for safe HPTS operations.
 */
KTEST_FUNC(tcpcb_initialization)
{
	struct tcp_hptsi *pace;
	struct tcpcb *tp;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	/* Verify the tcpcb is properly initialized for HPTS */
	tp = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp, NULL);
	KTEST_NEQUAL(tp->t_fb, NULL);
	KTEST_NEQUAL(tp->t_fb->tfb_tcp_output, NULL);
	KTEST_NEQUAL(tp->t_fb->tfb_do_queued_segments, NULL);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_NONE);
	KTEST_EQUAL((tp->t_flags2 & (TF2_HPTS_CPU_SET | TF2_HPTS_CALLS)), 0);

	/* Verify that HPTS-specific fields are initialized */
	KTEST_EQUAL(tp->t_hpts_gencnt, 0);
	KTEST_EQUAL(tp->t_hpts_slot, 0);
	KTEST_EQUAL(tp->t_hpts_request, 0);
	KTEST_EQUAL(tp->t_lro_cpu, 0);
	KTEST_VERIFY(tp->t_hpts_cpu < pace->rp_num_hptss);
	KTEST_EQUAL(tp->t_inpcb.inp_refcount, 1);
	KTEST_VERIFY(!(tp->t_inpcb.inp_flags & INP_DROPPED));

	test_hpts_free_tcpcb(tp);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates that tcpcb structures can be successfully inserted into and removed
 * from the HPTS wheel, with proper state tracking and slot assignment during
 * the process.
 */
KTEST_FUNC(tcpcb_insertion)
{
	struct tcp_hptsi *pace;
	struct tcpcb *tp;
	struct tcp_hpts_entry *hpts;
	uint32_t timeout_usecs = 10;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	tp = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp, NULL);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_NONE);
	KTEST_EQUAL((tp->t_flags2 & TF2_HPTS_CALLS), 0);

	INP_WLOCK(&tp->t_inpcb);
	tp->t_flags2 |= TF2_HPTS_CALLS;
	KTEST_EQUAL(call_counts[CCNT_SWI_SCHED], 0);
	tcp_hpts_insert(pace, tp, timeout_usecs, NULL);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE);
	INP_WUNLOCK(&tp->t_inpcb);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 0);
	KTEST_EQUAL(call_counts[CCNT_SWI_SCHED], 1);
	KTEST_VERIFY(tcp_in_hpts(tp));
	KTEST_VERIFY(tp->t_hpts_slot >= 0);
	KTEST_VERIFY(tp->t_hpts_slot < NUM_OF_HPTSI_SLOTS);

	hpts = pace->rp_ent[tp->t_hpts_cpu];
	KTEST_EQUAL(hpts->p_on_queue_cnt, 1);
	KTEST_EQUAL(tp->t_hpts_request, 0);
	KTEST_EQUAL(tp->t_hpts_slot, HPTS_USEC_TO_SLOTS(timeout_usecs));
	//KTEST_EQUAL(tp->t_hpts_gencnt, 1);

	INP_WLOCK(&tp->t_inpcb);
	tcp_hpts_remove(pace, tp);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_NONE);
	INP_WUNLOCK(&tp->t_inpcb);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 0);
	KTEST_VERIFY(!tcp_in_hpts(tp));

	KTEST_EQUAL(hpts->p_on_queue_cnt, 0);

	test_hpts_free_tcpcb(tp);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates the core HPTS timer functionality by verifying that scheduled
 * tcpcb entries trigger tcp_output calls at appropriate times, simulating
 * real-world timer-driven TCP processing.
 */
KTEST_FUNC(timer_functionality)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcp_hpts_entry *hpts;
	struct tcpcb *tp;
	int32_t slots_ran;
	uint32_t i;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	for (i = 0; i < pace->rp_num_hptss; i++)
		dump_hpts_entry(ctx, pace->rp_ent[i]);

	/* Create and insert the tcpcb into the HPTS wheel to wait for 500 usec */
	tp = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp, NULL);
	dump_tcpcb(tp);
	TP_LOG_TEST(tp) = 1; /* Enable logging for this tcpcb */

	KTEST_LOG(ctx, "=> tcp_hpts_insert(%p)", tp);
	INP_WLOCK(&tp->t_inpcb);
	tp->t_flags2 |= TF2_HPTS_CALLS; /* Mark as needing HPTS processing */
	tcp_hpts_insert(pace, tp, 500, NULL);
	INP_WUNLOCK(&tp->t_inpcb);

	dump_tcpcb(tp);
	for (i = 0; i < pace->rp_num_hptss; i++)
		dump_hpts_entry(ctx, pace->rp_ent[i]);

	hpts = pace->rp_ent[tp->t_hpts_cpu];
	KTEST_EQUAL(hpts->p_on_queue_cnt, 1);
	KTEST_EQUAL(hpts->p_prev_slot, 0);
	KTEST_EQUAL(hpts->p_cur_slot, 0);
	KTEST_EQUAL(hpts->p_runningslot, 0);
	KTEST_EQUAL(hpts->p_nxt_slot, 1);
	KTEST_EQUAL(hpts->p_hpts_active, 0);

	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE);
	KTEST_EQUAL(tp->t_hpts_request, 0);
	KTEST_EQUAL(tp->t_hpts_slot, HPTS_USEC_TO_SLOTS(500));

	/* Set our test flag to indicate the tcpcb should be removed from the
	 * wheel when tcp_output is called. */
	TP_REMOVE_FROM_HPTS(tp) = 1;

	/* Test early exit condition: advance time by insufficient amount */
	KTEST_LOG(ctx, "Testing early exit with insufficient time advancement");
	test_time_usec += 1; /* Very small advancement - should cause early exit */
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	/* Should return 0 slots due to insufficient time advancement */
	KTEST_EQUAL(slots_ran, 0);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 0); /* No processing should occur */
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE); /* Connection still queued */

	/* Wait for 498 more usecs and trigger the HPTS workers and verify
	 * nothing happens yet (total 499 usec) */
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 0);
	test_time_usec += 498;
	for (i = 0; i < pace->rp_num_hptss; i++) {
		KTEST_LOG(ctx, "=> tcp_hptsi(%p)", pace->rp_ent[i]);
		HPTS_LOCK(pace->rp_ent[i]);
		NET_EPOCH_ENTER(et);
		slots_ran = tcp_hptsi(pace->rp_ent[i], true);
		HPTS_UNLOCK(pace->rp_ent[i]);
		NET_EPOCH_EXIT(et);

		dump_hpts_entry(ctx, pace->rp_ent[i]);
		KTEST_VERIFY(slots_ran >= 0);
		KTEST_EQUAL(pace->rp_ent[i]->p_prev_slot, 49);
		KTEST_EQUAL(pace->rp_ent[i]->p_cur_slot, 49);
	}

	dump_tcpcb(tp);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 0);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE);
	KTEST_EQUAL(tp->t_hpts_request, 0);
	KTEST_EQUAL(tp->t_hpts_slot, HPTS_USEC_TO_SLOTS(500));
	KTEST_EQUAL(hpts->p_on_queue_cnt, 1);

	/* Wait for 1 more usec and trigger the HPTS workers and verify it
	 * triggers tcp_output this time */
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 0);
	test_time_usec += 1;
	for (i = 0; i < pace->rp_num_hptss; i++) {
		KTEST_LOG(ctx, "=> tcp_hptsi(%p)", pace->rp_ent[i]);
		HPTS_LOCK(pace->rp_ent[i]);
		NET_EPOCH_ENTER(et);
		slots_ran = tcp_hptsi(pace->rp_ent[i], true);
		HPTS_UNLOCK(pace->rp_ent[i]);
		NET_EPOCH_EXIT(et);

		dump_hpts_entry(ctx, pace->rp_ent[i]);
		KTEST_VERIFY(slots_ran >= 0);
		KTEST_EQUAL(pace->rp_ent[i]->p_prev_slot, 50);
		KTEST_EQUAL(pace->rp_ent[i]->p_cur_slot, 50);
	}

	dump_tcpcb(tp);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 1);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_NONE);
	KTEST_EQUAL(hpts->p_on_queue_cnt, 0);

	test_hpts_free_tcpcb(tp);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates HPTS scalability by creating and inserting a LOT of tcpcbs into
 * the HPTS wheel, testing performance under high load conditions.
 */
KTEST_FUNC(scalability_tcpcbs)
{
	struct tcp_hptsi *pace;
	struct tcpcb **tcpcbs;
	uint32_t i, num_tcpcbs = 100000, total_queued = 0;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	/* Allocate array to hold pointers to all tcpcbs */
	tcpcbs = malloc(num_tcpcbs * sizeof(struct tcpcb *), M_TCPHPTS, M_WAITOK | M_ZERO);
	KTEST_VERIFY_RET(tcpcbs != NULL, ENOMEM);

	/* Create a LOT of tcpcbs */
	KTEST_LOG(ctx, "Creating %u tcpcbs...", num_tcpcbs);
	for (i = 0; i < num_tcpcbs; i++) {
		tcpcbs[i] = test_hpts_create_tcpcb(ctx, pace);
		if (tcpcbs[i] == NULL) {
			KTEST_ERR(ctx, "FAIL: tcpcbs[i] == NULL");
			return (EINVAL);
		}
	}

	/* Insert all created tcpcbs into HPTS */
	KTEST_LOG(ctx, "Inserting all tcpcbs into HPTS...");
	for (i = 0; i < num_tcpcbs; i++) {
		INP_WLOCK(&tcpcbs[i]->t_inpcb);
		tcpcbs[i]->t_flags2 |= TF2_HPTS_CALLS;
		/* Insert with varying future timeouts to distribute across slots */
		tcp_hpts_insert(pace, tcpcbs[i], 100 + (i % 1000), NULL);
		INP_WUNLOCK(&tcpcbs[i]->t_inpcb);
	}

	/* Verify total queue counts across all CPUs */
	for (i = 0; i < pace->rp_num_hptss; i++) {
		total_queued += pace->rp_ent[i]->p_on_queue_cnt;
	}
	KTEST_EQUAL(total_queued, num_tcpcbs);

	for (i = 0; i < pace->rp_num_hptss; i++)
		dump_hpts_entry(ctx, pace->rp_ent[i]);

	/* Remove all tcpcbs from HPTS */
	KTEST_LOG(ctx, "Removing all tcpcbs from HPTS...");
	for (i = 0; i < num_tcpcbs; i++) {
		INP_WLOCK(&tcpcbs[i]->t_inpcb);
		if (tcpcbs[i]->t_in_hpts != IHPTS_NONE) {
			tcp_hpts_remove(pace, tcpcbs[i]);
		}
		INP_WUNLOCK(&tcpcbs[i]->t_inpcb);
	}

	/* Verify all queues are now empty */
	for (i = 0; i < pace->rp_num_hptss; i++) {
		if (pace->rp_ent[i]->p_on_queue_cnt != 0) {
			KTEST_ERR(ctx, "FAIL: pace->rp_ent[i]->p_on_queue_cnt != 0");
			return (EINVAL);
		}
	}

	for (i = 0; i < num_tcpcbs; i++) {
		test_hpts_free_tcpcb(tcpcbs[i]);
	}
	free(tcpcbs, M_TCPHPTS);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates wheel wrap scenarios where the timer falls significantly behind
 * and needs to process more than one full wheel revolution worth of slots.
 */
KTEST_FUNC(wheel_wrap_recovery)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcpcb **tcpcbs;
	uint32_t i, timeout_usecs, num_tcpcbs = 500;
	int32_t slots_ran;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	/* Allocate array to hold pointers to tcpcbs */
	tcpcbs = malloc(num_tcpcbs * sizeof(struct tcpcb *), M_TCPHPTS, M_WAITOK | M_ZERO);
	KTEST_VERIFY_RET(tcpcbs != NULL, ENOMEM);

	/* Create tcpcbs and insert them across many slots */
	for (i = 0; i < num_tcpcbs; i++) {
		tcpcbs[i] = test_hpts_create_tcpcb(ctx, pace);
		KTEST_NEQUAL(tcpcbs[i], NULL);
		TP_REMOVE_FROM_HPTS(tcpcbs[i]) = 1;

		timeout_usecs = ((i * NUM_OF_HPTSI_SLOTS) / num_tcpcbs) * HPTS_USECS_PER_SLOT; /* Spread across slots */

		INP_WLOCK(&tcpcbs[i]->t_inpcb);
		tcpcbs[i]->t_flags2 |= TF2_HPTS_CALLS;
		tcp_hpts_insert(pace, tcpcbs[i], timeout_usecs, NULL);
		INP_WUNLOCK(&tcpcbs[i]->t_inpcb);
	}

	/* Fast forward time significantly to trigger wheel wrap */
	test_time_usec += (NUM_OF_HPTSI_SLOTS + 5000) * HPTS_USECS_PER_SLOT;

	for (i = 0; i < pace->rp_num_hptss; i++) {
		KTEST_LOG(ctx, "=> tcp_hptsi(%u)", i);
		KTEST_NEQUAL(pace->rp_ent[i]->p_on_queue_cnt, 0);

		HPTS_LOCK(pace->rp_ent[i]);
		NET_EPOCH_ENTER(et);
		slots_ran = tcp_hptsi(pace->rp_ent[i], true);
		HPTS_UNLOCK(pace->rp_ent[i]);
		NET_EPOCH_EXIT(et);

		KTEST_EQUAL(slots_ran, NUM_OF_HPTSI_SLOTS-1); /* Should process all slots */
		KTEST_EQUAL(pace->rp_ent[i]->p_on_queue_cnt, 0);
		KTEST_NEQUAL(pace->rp_ent[i]->p_cur_slot,
			pace->rp_ent[i]->p_prev_slot);
	}

	/* Cleanup */
	for (i = 0; i < num_tcpcbs; i++) {
		INP_WLOCK(&tcpcbs[i]->t_inpcb);
		if (tcpcbs[i]->t_in_hpts != IHPTS_NONE) {
			tcp_hpts_remove(pace, tcpcbs[i]);
		}
		INP_WUNLOCK(&tcpcbs[i]->t_inpcb);
		test_hpts_free_tcpcb(tcpcbs[i]);
	}
	free(tcpcbs, M_TCPHPTS);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates proper handling of tcpcbs in the IHPTS_MOVING state, which occurs
 * when a tcpcb is being processed by the HPTS thread but gets removed.
 */
KTEST_FUNC(tcpcb_moving_state)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcpcb *tp1, *tp2;
	struct tcp_hpts_entry *hpts;
	int32_t slots_ran;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	/* Create two tcpcbs on the same CPU/slot */
	tp1 = test_hpts_create_tcpcb(ctx, pace);
	tp2 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp1, NULL);
	KTEST_NEQUAL(tp2, NULL);

	/* Force them to the same CPU for predictable testing */
	tp1->t_hpts_cpu = 0;
	tp2->t_hpts_cpu = 0;

	/* Insert both into the same slot */
	INP_WLOCK(&tp1->t_inpcb);
	tp1->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp1, 100, NULL);
	INP_WUNLOCK(&tp1->t_inpcb);

	INP_WLOCK(&tp2->t_inpcb);
	tp2->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp2, 100, NULL);
	INP_WUNLOCK(&tp2->t_inpcb);

	hpts = pace->rp_ent[0];

	/* Manually transition tp1 to MOVING state to simulate race condition */
	HPTS_LOCK(hpts);
	tp1->t_in_hpts = IHPTS_MOVING;
	tp1->t_hpts_slot = -1; /* Mark for removal */
	HPTS_UNLOCK(hpts);

	/* Set time and run HPTS to process the moving state */
	test_time_usec += 100;
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	KTEST_VERIFY(slots_ran >= 0);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 1); /* Shouldn't call on both */

	/* tp1 should be cleaned up and removed */
	KTEST_EQUAL(tp1->t_in_hpts, IHPTS_NONE);
	/* tp2 should have been processed normally */
	KTEST_EQUAL(tp2->t_in_hpts, IHPTS_NONE);

	test_hpts_free_tcpcb(tp1);
	test_hpts_free_tcpcb(tp2);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates that tcpcbs with deferred requests (t_hpts_request > 0) are
 * properly handled and re-inserted into appropriate future slots after
 * the wheel processes enough slots to accommodate the original request.
 */
KTEST_FUNC(deferred_requests)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcpcb *tp, *tp2;
	struct tcp_hpts_entry *hpts;
	uint32_t large_timeout_usecs = (NUM_OF_HPTSI_SLOTS + 5000) * HPTS_USECS_PER_SLOT; /* Beyond wheel capacity */
	uint32_t huge_timeout_usecs = (NUM_OF_HPTSI_SLOTS * 3) * HPTS_USECS_PER_SLOT; /* 3x wheel capacity */
	uint32_t initial_request;
	int32_t slots_ran;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	tp = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp, NULL);

	/* Insert with a request that exceeds current wheel capacity */
	INP_WLOCK(&tp->t_inpcb);
	tp->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp, large_timeout_usecs, NULL);
	INP_WUNLOCK(&tp->t_inpcb);

	/* Verify it was inserted with a deferred request */
	dump_tcpcb(tp);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE);
	KTEST_VERIFY(tp->t_hpts_request > 0);
	KTEST_VERIFY(tp->t_hpts_slot < NUM_OF_HPTSI_SLOTS);

	hpts = pace->rp_ent[tp->t_hpts_cpu];

	/* Advance time to process deferred requests */
	test_time_usec += NUM_OF_HPTSI_SLOTS * HPTS_USECS_PER_SLOT;

	/* Process the wheel to handle deferred requests */
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	dump_hpts_entry(ctx, hpts);
	KTEST_GREATER_THAN(slots_ran, 0);
	dump_tcpcb(tp);
	KTEST_EQUAL(tp->t_hpts_request, 0);

	/* Test incremental deferred request processing over multiple cycles */
	KTEST_LOG(ctx, "Testing incremental deferred request processing");

	/* Create a new connection with an even larger request */
	tp2 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp2, NULL);
	tp2->t_hpts_cpu = tp->t_hpts_cpu; /* Same CPU for predictable testing */

	INP_WLOCK(&tp2->t_inpcb);
	tp2->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp2, huge_timeout_usecs, NULL);
	INP_WUNLOCK(&tp2->t_inpcb);

	/* Verify initial deferred request */
	initial_request = tp2->t_hpts_request;
	KTEST_VERIFY(initial_request > NUM_OF_HPTSI_SLOTS);

	/* Process one wheel cycle - should reduce but not eliminate request */
	test_time_usec += NUM_OF_HPTSI_SLOTS * HPTS_USECS_PER_SLOT;
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	/* Request should be reduced but not zero */
	KTEST_GREATER_THAN(initial_request, tp2->t_hpts_request);
	KTEST_VERIFY(tp2->t_hpts_request > 0);
	KTEST_EQUAL(tp2->t_in_hpts, IHPTS_ONQUEUE); /* Still queued */

	/* For huge_timeout_usecs = NUM_OF_HPTSI_SLOTS * 3 * HPTS_USECS_PER_SLOT, we need ~3 cycles to complete.
	 * Each cycle can reduce the request by at most NUM_OF_HPTSI_SLOTS. */
	test_time_usec += NUM_OF_HPTSI_SLOTS * HPTS_USECS_PER_SLOT;
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	/* After second cycle, request should be reduced significantly (likely by ~NUM_OF_HPTSI_SLOTS) */
	KTEST_VERIFY(tp2->t_hpts_request < initial_request);
	KTEST_VERIFY(tp2->t_hpts_request > 0); /* But not yet zero for such a large request */

	/* Clean up second connection */
	INP_WLOCK(&tp2->t_inpcb);
	if (tp2->t_in_hpts != IHPTS_NONE) {
		tcp_hpts_remove(pace, tp2);
	}
	INP_WUNLOCK(&tp2->t_inpcb);
	test_hpts_free_tcpcb(tp2);

	/* Clean up */
	INP_WLOCK(&tp->t_inpcb);
	if (tp->t_in_hpts != IHPTS_NONE) {
		tcp_hpts_remove(pace, tp);
	}
	INP_WUNLOCK(&tp->t_inpcb);
	test_hpts_free_tcpcb(tp);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates CPU assignment and affinity mechanisms, including flowid-based
 * assignment, random fallback scenarios, and explicit CPU setting. Tests
 * the actual cpu assignment logic in hpts_cpuid via tcp_set_hpts.
 */
KTEST_FUNC(cpu_assignment)
{
	struct tcp_hptsi *pace;
	struct tcpcb *tp1, *tp2, *tp2_dup, *tp3;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);

	/* Test random CPU assignment (no flowid) */
	tp1 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp1, NULL);
	tp1->t_inpcb.inp_flowtype = M_HASHTYPE_NONE;
	INP_WLOCK(&tp1->t_inpcb);
	tcp_set_hpts(pace, tp1);
	INP_WUNLOCK(&tp1->t_inpcb);
	KTEST_VERIFY(tp1->t_hpts_cpu < pace->rp_num_hptss);
	KTEST_VERIFY(tp1->t_flags2 & TF2_HPTS_CPU_SET);

	/* Test flowid-based assignment */
	tp2 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp2, NULL);
	tp2->t_inpcb.inp_flowtype = M_HASHTYPE_RSS_TCP_IPV4;
	tp2->t_inpcb.inp_flowid = 12345;
	INP_WLOCK(&tp2->t_inpcb);
	tcp_set_hpts(pace, tp2);
	INP_WUNLOCK(&tp2->t_inpcb);
	KTEST_VERIFY(tp2->t_hpts_cpu < pace->rp_num_hptss);
	KTEST_VERIFY(tp2->t_flags2 & TF2_HPTS_CPU_SET);

	/* With the same flowid, should get same CPU assignment */
	tp2_dup = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp2_dup, NULL);
	tp2_dup->t_inpcb.inp_flowtype = M_HASHTYPE_RSS_TCP_IPV4;
	tp2_dup->t_inpcb.inp_flowid = 12345;
	INP_WLOCK(&tp2_dup->t_inpcb);
	tcp_set_hpts(pace, tp2_dup);
	INP_WUNLOCK(&tp2_dup->t_inpcb);
	KTEST_EQUAL(tp2_dup->t_hpts_cpu, tp2->t_hpts_cpu);

	/* Test explicit CPU setting */
	tp3 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp3, NULL);
	tp3->t_hpts_cpu = 1; /* Assume we have at least 2 CPUs */
	tp3->t_flags2 |= TF2_HPTS_CPU_SET;
	INP_WLOCK(&tp3->t_inpcb);
	tcp_set_hpts(pace, tp3);
	INP_WUNLOCK(&tp3->t_inpcb);
	KTEST_EQUAL(tp3->t_hpts_cpu, 1);

	test_hpts_free_tcpcb(tp1);
	test_hpts_free_tcpcb(tp2);
	test_hpts_free_tcpcb(tp2_dup);
	test_hpts_free_tcpcb(tp3);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates edge cases in slot calculation including boundary conditions
 * around slot 0, maximum slots, and slot wrapping arithmetic.
 */
KTEST_FUNC(slot_boundary_conditions)
{
	struct tcp_hptsi *pace;
	struct tcpcb *tp;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	/* Test insertion at slot 0 */
	tp = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp, NULL);
	INP_WLOCK(&tp->t_inpcb);
	tp->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp, 0, NULL); /* Should insert immediately (0 timeout) */
	INP_WUNLOCK(&tp->t_inpcb);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE);
	KTEST_VERIFY(tp->t_hpts_slot < NUM_OF_HPTSI_SLOTS);

	INP_WLOCK(&tp->t_inpcb);
	tcp_hpts_remove(pace, tp);
	INP_WUNLOCK(&tp->t_inpcb);

	/* Test insertion at maximum slot value */
	INP_WLOCK(&tp->t_inpcb);
	tp->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp, (NUM_OF_HPTSI_SLOTS - 1) * HPTS_USECS_PER_SLOT, NULL);
	INP_WUNLOCK(&tp->t_inpcb);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE);

	INP_WLOCK(&tp->t_inpcb);
	tcp_hpts_remove(pace, tp);
	INP_WUNLOCK(&tp->t_inpcb);

	/* Test very small timeout values */
	INP_WLOCK(&tp->t_inpcb);
	tp->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp, 1, NULL);
	INP_WUNLOCK(&tp->t_inpcb);
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_ONQUEUE);
	KTEST_EQUAL(tp->t_hpts_slot, HPTS_USEC_TO_SLOTS(1)); /* Should convert 1 usec to slot */

	INP_WLOCK(&tp->t_inpcb);
	tcp_hpts_remove(pace, tp);
	INP_WUNLOCK(&tp->t_inpcb);

	test_hpts_free_tcpcb(tp);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates HPTS behavior under high load conditions, including proper
 * processing of many connections and connection count tracking.
 */
KTEST_FUNC(dynamic_sleep_adjustment)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcpcb **tcpcbs;
	struct tcp_hpts_entry *hpts;
	uint32_t i, num_tcpcbs = DEFAULT_CONNECTION_THRESHOLD + 50;
	int32_t slots_ran;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	/* Create many connections to exceed threshold */
	tcpcbs = malloc(num_tcpcbs * sizeof(struct tcpcb *), M_TCPHPTS, M_WAITOK | M_ZERO);
	KTEST_VERIFY_RET(tcpcbs != NULL, ENOMEM);

	for (i = 0; i < num_tcpcbs; i++) {
		tcpcbs[i] = test_hpts_create_tcpcb(ctx, pace);
		KTEST_NEQUAL(tcpcbs[i], NULL);
		tcpcbs[i]->t_hpts_cpu = 0; /* Force all to CPU 0 */
		INP_WLOCK(&tcpcbs[i]->t_inpcb);
		tcpcbs[i]->t_flags2 |= TF2_HPTS_CALLS;
		TP_REMOVE_FROM_HPTS(tcpcbs[i]) = 1; /* Will be removed after output */
		tcp_hpts_insert(pace, tcpcbs[i], 100, NULL);
		INP_WUNLOCK(&tcpcbs[i]->t_inpcb);
	}

	hpts = pace->rp_ent[0];
	dump_hpts_entry(ctx, hpts);

	/* Verify we're above threshold */
	KTEST_GREATER_THAN(hpts->p_on_queue_cnt, DEFAULT_CONNECTION_THRESHOLD);

	/* Run HPTS to process many connections */
	test_time_usec += 100;
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	/* Verify HPTS processed slots and connections correctly */
	KTEST_GREATER_THAN(slots_ran, 0);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], num_tcpcbs);

	/* Verify all connections were removed from queue */
	KTEST_EQUAL(hpts->p_on_queue_cnt, 0);

	/* Cleanup */
	for (i = 0; i < num_tcpcbs; i++) {
		test_hpts_free_tcpcb(tcpcbs[i]);
	}
	free(tcpcbs, M_TCPHPTS);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates handling of concurrent insert/remove operations and race conditions
 * between HPTS processing and user operations.
 */
KTEST_FUNC(concurrent_operations)
{
	struct tcp_hptsi *pace;
	struct tcpcb *tp1, *tp2;
	struct tcp_hpts_entry *hpts;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	tp1 = test_hpts_create_tcpcb(ctx, pace);
	tp2 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp1, NULL);
	KTEST_NEQUAL(tp2, NULL);

	/* Force all to CPU 0 */
	tp1->t_hpts_cpu = 0;
	tp2->t_hpts_cpu = 0;

	/* Insert tp1 */
	INP_WLOCK(&tp1->t_inpcb);
	tp1->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp1, 100, NULL);
	INP_WUNLOCK(&tp1->t_inpcb);

	/* Insert tp2 into same slot */
	INP_WLOCK(&tp2->t_inpcb);
	tp2->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp2, 100, NULL);
	INP_WUNLOCK(&tp2->t_inpcb);

	/* Verify both are inserted */
	KTEST_EQUAL(tp1->t_in_hpts, IHPTS_ONQUEUE);
	KTEST_EQUAL(tp2->t_in_hpts, IHPTS_ONQUEUE);

	/* Verify they're both assigned to the same slot */
	KTEST_EQUAL(tp1->t_hpts_slot, tp2->t_hpts_slot);

	/* Verify queue count reflects both connections */
	KTEST_EQUAL(tp1->t_hpts_cpu, tp2->t_hpts_cpu); /* Should be on same CPU */
	hpts = pace->rp_ent[tp1->t_hpts_cpu];
	KTEST_EQUAL(hpts->p_on_queue_cnt, 2);

	/* Remove tp1 while tp2 is still there */
	INP_WLOCK(&tp1->t_inpcb);
	tcp_hpts_remove(pace, tp1);
	INP_WUNLOCK(&tp1->t_inpcb);

	/* Verify tp1 removed, tp2 still there */
	KTEST_EQUAL(tp1->t_in_hpts, IHPTS_NONE);
	KTEST_EQUAL(tp2->t_in_hpts, IHPTS_ONQUEUE);

	/* Verify queue count decreased by one */
	KTEST_EQUAL(hpts->p_on_queue_cnt, 1);

	/* Remove tp2 */
	INP_WLOCK(&tp2->t_inpcb);
	tcp_hpts_remove(pace, tp2);
	INP_WUNLOCK(&tp2->t_inpcb);

	KTEST_EQUAL(tp2->t_in_hpts, IHPTS_NONE);

	/* Verify queue is now completely empty */
	KTEST_EQUAL(hpts->p_on_queue_cnt, 0);

	test_hpts_free_tcpcb(tp1);
	test_hpts_free_tcpcb(tp2);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates the queued segments processing path via tfb_do_queued_segments,
 * which is an alternative to direct tcp_output calls.
 */
KTEST_FUNC(queued_segments_processing)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcpcb *tp;
	struct tcp_hpts_entry *hpts;
	struct mbuf *fake_mbuf;
	int32_t slots_ran;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	tp = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp, NULL);

	/* Create a minimal fake mbuf that has valid STAILQ pointers */
	fake_mbuf = malloc(sizeof(struct mbuf), M_TCPHPTS, M_WAITOK | M_ZERO);
	KTEST_NEQUAL(fake_mbuf, NULL);

	/* Set up for queued segments path */
	tp->t_flags2 |= (TF2_HPTS_CALLS | TF2_SUPPORTS_MBUFQ);
	STAILQ_INSERT_TAIL(&tp->t_inqueue, fake_mbuf, m_stailqpkt);

	INP_WLOCK(&tp->t_inpcb);
	tcp_hpts_insert(pace, tp, 100, NULL);
	INP_WUNLOCK(&tp->t_inpcb);

	hpts = pace->rp_ent[tp->t_hpts_cpu];

	/* Run HPTS and verify queued segments path is taken */
	test_time_usec += 100;
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	KTEST_VERIFY(slots_ran >= 0);
	KTEST_EQUAL(call_counts[CCNT_TCP_TFB_DO_QUEUED_SEGMENTS], 1);

	/* Connection should be removed from HPTS after processing */
	KTEST_EQUAL(tp->t_in_hpts, IHPTS_NONE);

	/* Clean up the fake mbuf if it's still in the queue */
	if (!STAILQ_EMPTY(&tp->t_inqueue)) {
		struct mbuf *m = STAILQ_FIRST(&tp->t_inqueue);
		STAILQ_REMOVE_HEAD(&tp->t_inqueue, m_stailqpkt);
		free(m, M_TCPHPTS);
	}

	test_hpts_free_tcpcb(tp);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates the direct wake mechanism and wake inhibition logic when
 * the connection count exceeds thresholds.
 */
KTEST_FUNC(direct_wake_mechanism)
{
	struct tcp_hptsi *pace;
	struct tcpcb *tp;
	struct tcp_hpts_entry *hpts;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	tp = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp, NULL);
	hpts = pace->rp_ent[tp->t_hpts_cpu];

	/* Test direct wake when not over threshold */
	HPTS_LOCK(hpts);
	hpts->p_on_queue_cnt = 50; /* Below threshold */
	hpts->p_hpts_wake_scheduled = 0;
	tcp_hpts_wake(hpts);
	KTEST_EQUAL(hpts->p_hpts_wake_scheduled, 1);
	KTEST_EQUAL(call_counts[CCNT_SWI_SCHED], 1);
	HPTS_UNLOCK(hpts);

	/* Reset for next test */
	hpts->p_hpts_wake_scheduled = 0;
	call_counts[CCNT_SWI_SCHED] = 0;

	/* Test wake inhibition when over threshold */
	HPTS_LOCK(hpts);
	hpts->p_on_queue_cnt = 200; /* Above threshold */
	hpts->p_direct_wake = 1; /* Request direct wake */
	tcp_hpts_wake(hpts);
	KTEST_EQUAL(hpts->p_hpts_wake_scheduled, 0); /* Should be inhibited */
	KTEST_EQUAL(hpts->p_direct_wake, 0); /* Should be cleared */
	KTEST_EQUAL(call_counts[CCNT_SWI_SCHED], 0); /* No SWI scheduled */
	HPTS_UNLOCK(hpts);

	test_hpts_free_tcpcb(tp);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates HPTS collision detection when attempting to run HPTS while
 * it's already active.
 */
KTEST_FUNC(hpts_collision_detection)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcp_hpts_entry *hpts;
	int32_t slots_ran;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	hpts = pace->rp_ent[0];

	/* Mark HPTS as active */
	HPTS_LOCK(hpts);
	hpts->p_hpts_active = 1;
	HPTS_UNLOCK(hpts);

	/* Attempt to run HPTS again - should detect collision */
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, false); /* from_callout = false */
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	/* Should return 0 indicating no work done due to collision */
	KTEST_EQUAL(slots_ran, 0);

	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

/*
 * Validates generation count handling for race condition detection between
 * HPTS processing and connection insertion/removal operations.
 */
KTEST_FUNC(generation_count_validation)
{
	struct epoch_tracker et;
	struct tcp_hptsi *pace;
	struct tcp_hpts_entry *hpts;
	struct tcpcb *tp1, *tp2;
	uint32_t initial_gencnt, slot_to_test = 10;
	uint32_t timeout_usecs = slot_to_test * HPTS_USECS_PER_SLOT;
	uint32_t tp2_original_gencnt;
	int32_t slots_ran;

	test_hpts_init();

	pace = tcp_hptsi_create(&test_funcs, false);
	KTEST_NEQUAL(pace, NULL);
	tcp_hptsi_start(pace);

	hpts = pace->rp_ent[0];

	/* Record initial generation count for the test slot */
	initial_gencnt = hpts->p_hptss[slot_to_test].gencnt;

	/* Create and insert first connection */
	tp1 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp1, NULL);
	tp1->t_hpts_cpu = 0; /* Force to CPU 0 */

	INP_WLOCK(&tp1->t_inpcb);
	tp1->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp1, timeout_usecs, NULL);
	INP_WUNLOCK(&tp1->t_inpcb);

	/* Verify connection stored the generation count */
	KTEST_EQUAL(tp1->t_in_hpts, IHPTS_ONQUEUE);
	KTEST_EQUAL(tp1->t_hpts_slot, slot_to_test);
	KTEST_EQUAL(tp1->t_hpts_gencnt, initial_gencnt);

	/* Create second connection but don't insert yet */
	tp2 = test_hpts_create_tcpcb(ctx, pace);
	KTEST_NEQUAL(tp2, NULL);
	tp2->t_hpts_cpu = 0; /* Force to CPU 0 */

	/* Force generation count increment by processing the slot */
	test_time_usec += (slot_to_test + 1) * HPTS_USECS_PER_SLOT;
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	/* Verify processing occurred */
	KTEST_VERIFY(slots_ran > 0);
	KTEST_EQUAL(call_counts[CCNT_TCP_OUTPUT], 1);

	/* Verify generation count was incremented */
	KTEST_EQUAL(hpts->p_hptss[slot_to_test].gencnt, initial_gencnt + 1);

	/* Verify first connection was processed and removed */
	KTEST_EQUAL(tp1->t_in_hpts, IHPTS_NONE);

	/* Insert second connection and record its generation count */
	INP_WLOCK(&tp2->t_inpcb);
	tp2->t_flags2 |= TF2_HPTS_CALLS;
	tcp_hpts_insert(pace, tp2, timeout_usecs, NULL);
	INP_WUNLOCK(&tp2->t_inpcb);

	/* Verify connection was inserted successfully */
	KTEST_EQUAL(tp2->t_in_hpts, IHPTS_ONQUEUE);

	/* Record the generation count that tp2 received */
	tp2_original_gencnt = tp2->t_hpts_gencnt;

	/* Test generation count mismatch detection during processing */
	/* Manually set stale generation count to simulate race condition */
	tp2->t_hpts_gencnt = tp2_original_gencnt + 100; /* Force a mismatch */

	/* Process the slot to trigger generation count validation */
	test_time_usec += (slot_to_test + 1) * HPTS_USECS_PER_SLOT;
	HPTS_LOCK(hpts);
	NET_EPOCH_ENTER(et);
	slots_ran = tcp_hptsi(hpts, true);
	HPTS_UNLOCK(hpts);
	NET_EPOCH_EXIT(et);

	/* Connection should be processed despite generation count mismatch */
	KTEST_EQUAL(tp2->t_in_hpts, IHPTS_NONE); /* Processed and released */

	/* The key test: HPTS should handle mismatched generation counts gracefully */
	KTEST_VERIFY(slots_ran > 0); /* Processing should still occur */

	test_hpts_free_tcpcb(tp1);
	test_hpts_free_tcpcb(tp2);
	tcp_hptsi_stop(pace);
	tcp_hptsi_destroy(pace);

	return (0);
}

static const struct ktest_test_info tests[] = {
	KTEST_INFO(module_load),
	KTEST_INFO(hptsi_create_destroy),
	KTEST_INFO(hptsi_start_stop),
	KTEST_INFO(hptsi_independence),
	KTEST_INFO(function_injection),
	KTEST_INFO(tcpcb_initialization),
	KTEST_INFO(tcpcb_insertion),
	KTEST_INFO(timer_functionality),
	KTEST_INFO(scalability_tcpcbs),
	KTEST_INFO(wheel_wrap_recovery),
	KTEST_INFO(tcpcb_moving_state),
	KTEST_INFO(deferred_requests),
	KTEST_INFO(cpu_assignment),
	KTEST_INFO(slot_boundary_conditions),
	KTEST_INFO(dynamic_sleep_adjustment),
	KTEST_INFO(concurrent_operations),
	KTEST_INFO(queued_segments_processing),
	KTEST_INFO(direct_wake_mechanism),
	KTEST_INFO(hpts_collision_detection),
	KTEST_INFO(generation_count_validation),
};

#else /* TCP_HPTS_KTEST */

/*
 * Stub to indicate that the TCP HPTS ktest is not enabled.
 */
KTEST_FUNC(module_load_without_tests)
{
	KTEST_LOG(ctx, "Warning: TCP HPTS ktest is not enabled");
	return (0);
}

static const struct ktest_test_info tests[] = {
	KTEST_INFO(module_load_without_tests),
};

#endif

KTEST_MODULE_DECLARE(ktest_tcphpts, tests);
KTEST_MODULE_DEPEND(ktest_tcphpts, tcphpts);
