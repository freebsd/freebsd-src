/*-
 * Copyright (c) 2003-2009 RMI Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RMI_BSD */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/limits.h>
#include <sys/bus.h>

#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/unistd.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>

#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/hwfunc.h>
#include <machine/mips_opcode.h>

#include <machine/param.h>
#include <machine/intr_machdep.h>
#include <mips/rmi/interrupt.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/board.h>

#define MSGRNG_CC_INIT_CPU_DEST(dest, counter) \
do { \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][0], 0 ); \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][1], 1 ); \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][2], 2 ); \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][3], 3 ); \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][4], 4 ); \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][5], 5 ); \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][6], 6 ); \
     msgrng_write_cc(MSGRNG_CC_##dest##_REG, counter[dest][7], 7 ); \
} while(0)


/*
 * Keep track of our message ring handler threads, each core has a 
 * different message station. Ideally we will need to start a few
 * message handling threads every core, and wake them up depending on
 * load
 */
struct msgring_thread {
	struct {
		struct thread	*thread; /* msgring handler threads */
		int	needed;		/* thread needs to wake up */
	} threads[XLR_NTHREADS];
	int	running;		/* number of threads running */
	int	nthreads;		/* number of threads started */
	struct mtx lock;		/* for changing running/active */
};
static struct msgring_thread msgring_threads[XLR_MAX_CORES];
static struct proc *msgring_proc;	/* all threads are under a proc */

/*
 * The maximum number of software message handler threads to be started 
 * per core. Default is 3 per core
 */
static int	msgring_maxthreads = 3; 
TUNABLE_INT("hw.fmn.maxthreads", &msgring_maxthreads);

/* 
 * The device drivers can register a handler for the messages sent
 * from a station (corresponding to the device). 
 */
struct tx_stn_handler {
	msgring_handler action;
	void *arg;
};
static struct tx_stn_handler msgmap[MSGRNG_NSTATIONS];
static struct mtx	msgmap_lock;

/*
 * Initialize the messaging subsystem.
 * 
 * Message Stations are shared among all threads in a cpu core, this 
 * has to be called once from every core which is online.
 */
void 
xlr_msgring_cpu_init(void)
{
	struct stn_cc *cc_config;
	struct bucket_size *bucket_sizes;
	uint32_t flags;
	int id;

	KASSERT(xlr_thr_id() == 0,
		("xlr_msgring_cpu_init from non-zero thread"));
	id = xlr_core_id();
	bucket_sizes = xlr_board_info.bucket_sizes;
	cc_config = xlr_board_info.credit_configs[id];

	flags = msgrng_access_enable();

	/*
	 * FMN messages are received in 8 buckets per core, set up
	 * the bucket sizes for each bucket
	 */
	msgrng_write_bucksize(0, bucket_sizes->bucket[id * 8 + 0]);
	msgrng_write_bucksize(1, bucket_sizes->bucket[id * 8 + 1]);
	msgrng_write_bucksize(2, bucket_sizes->bucket[id * 8 + 2]);
	msgrng_write_bucksize(3, bucket_sizes->bucket[id * 8 + 3]);
	msgrng_write_bucksize(4, bucket_sizes->bucket[id * 8 + 4]);
	msgrng_write_bucksize(5, bucket_sizes->bucket[id * 8 + 5]);
	msgrng_write_bucksize(6, bucket_sizes->bucket[id * 8 + 6]);
	msgrng_write_bucksize(7, bucket_sizes->bucket[id * 8 + 7]);

	/* 
	 * For sending FMN messages, we need credits on the destination
	 * bucket.  Program the credits this core has on the 128 possible
	 * destination buckets.
	 * We cannot use a loop here, because the first argument has
	 * to be a constant integer value.
	 */
	MSGRNG_CC_INIT_CPU_DEST(0,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(1,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(2,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(3,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(4,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(5,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(6,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(7,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(8,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(9,  cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(10, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(11, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(12, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(13, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(14, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(15, cc_config->counters);
	msgrng_restore(flags);
}

/*
 * Boot time init, called only once
 */
void 
xlr_msgring_config(void)
{
	mtx_init(&msgmap_lock, "msgring", NULL, MTX_SPIN);
	
	/* check value */
	if (msgring_maxthreads < 0 || msgring_maxthreads > XLR_NTHREADS)
		msgring_maxthreads = XLR_NTHREADS;
}

/*
 * Drain out max_messages for the buckets set in the bucket mask. 
 * Use max_messages = 0 to drain out all messages.
 */
uint32_t
xlr_msgring_handler(uint8_t bucket_mask, uint32_t max_messages)
{
	int bucket = 0;
	int size = 0, code = 0, rx_stid = 0;
	struct msgrng_msg msg;
	struct tx_stn_handler *he;
	unsigned int status = 0;
	unsigned long mflags;
	uint32_t n_msgs;
	uint32_t msgbuckets;

	n_msgs = 0;
	mflags = msgrng_access_enable();
	for (;;) {
		msgbuckets = (~msgrng_read_status() >> 24) & bucket_mask;

		/* all buckets empty, break */
		if (msgbuckets == 0)
			break;

		for (bucket = 0; bucket < 8; bucket++) {
			if ((msgbuckets & (1 << bucket)) == 0) /* empty */
				continue;

			status = message_receive(bucket, &size, &code,
			    &rx_stid, &msg);
			if (status != 0)
				continue;
			n_msgs++;
			he = &msgmap[rx_stid];
			if (he->action == NULL) {
				printf("[%s]: No Handler for message from "
				    "stn_id=%d, bucket=%d, size=%d, msg0=%jx\n",
				    __func__, rx_stid, bucket, size,
				    (uintmax_t)msg.msg0);
			} else {
				msgrng_restore(mflags);
				(*he->action)(bucket, size, code, rx_stid,
				    &msg, he->arg);
				mflags = msgrng_access_enable();
			}
			if (max_messages > 0 && n_msgs >= max_messages)
				goto done;
		}
	}

done:
	msgrng_restore(mflags);
	return (n_msgs);
}

/* 
 * XLR COP2 supports watermark interrupts based on the number of 
 * messages pending in all the buckets in the core.  We increase 
 * the watermark until all the possible handler threads in the core
 * are woken up.
 */
static void
msgrng_setconfig(int running, int nthr)
{
	uint32_t config, mflags;
	int watermark = 1;	/* non zero needed */
	int wm_intr_value;

	KASSERT(nthr >= 0 && nthr <= msgring_maxthreads,
	    ("Bad value of nthr %d", nthr));
	KASSERT(running <= nthr, ("Bad value of running %d", running));

	if (running == nthr) {
		wm_intr_value = 0;
	} else {
		switch (running) {
		case 0: break;		/* keep default */
		case 1:
			watermark = 32; break;
		case 2:
			watermark = 48; break;
		case 3: 
			watermark = 56; break;
		}
		wm_intr_value = 0x2;	/* set watermark enable interrupt */
	}
	mflags = msgrng_access_enable();
	config = (watermark << 24) | (IRQ_MSGRING << 16) | (1 << 8) |
		wm_intr_value;
	/* clear pending interrupts, they will get re-raised if still valid */
	write_c0_eirr64(1ULL << IRQ_MSGRING);
	msgrng_write_config(config);
	msgrng_restore(mflags);
}

/* Debug counters */
static int msgring_nintr[XLR_MAX_CORES];
static int msgring_badintr[XLR_MAX_CORES];
static int msgring_wakeup_sleep[XLR_MAX_CORES * XLR_NTHREADS];
static int msgring_wakeup_nosleep[XLR_MAX_CORES * XLR_NTHREADS];
static int msgring_nmsgs[XLR_MAX_CORES * XLR_NTHREADS];

static int
msgring_process_fast_intr(void *arg)
{
	struct msgring_thread *mthd;
	struct thread	*td;
	uint32_t	mflags;
	int		core, nt;

	core = xlr_core_id();
	mthd = &msgring_threads[core];
	msgring_nintr[core]++;
	mtx_lock_spin(&mthd->lock);
	nt = mthd->running;
	if(nt >= mthd->nthreads) {
		msgring_badintr[core]++;
		mtx_unlock_spin(&mthd->lock);
		return (FILTER_HANDLED);
	}

	td = mthd->threads[nt].thread;
	mflags = msgrng_access_enable();

	/* default value with interrupts disabled */
	msgrng_write_config((1 << 24) | (IRQ_MSGRING << 16) | (1 << 8));
	/* clear pending interrupts */
	write_c0_eirr64(1ULL << IRQ_MSGRING);
	msgrng_restore(mflags);
	mtx_unlock_spin(&mthd->lock);

	/* wake up the target thread */
	mthd->threads[nt].needed = 1;
	thread_lock(td);
	if (TD_AWAITING_INTR(td)) {
		msgring_wakeup_sleep[core*4+nt]++;
		TD_CLR_IWAIT(td);
		sched_add(td, SRQ_INTR);
	} else
		msgring_wakeup_nosleep[core*4+nt]++;
	thread_unlock(td);
	return (FILTER_HANDLED);
}

static void
msgring_process(void *arg)
{
	struct msgring_thread *mthd;
	struct thread	*td;
	int		hwtid, tid, core;
	int		nmsgs;

	hwtid = (intptr_t)arg;
	core = hwtid / 4;
	tid = hwtid % 4;
	mthd = &msgring_threads[core];
	td = mthd->threads[tid].thread;
	KASSERT(curthread == td,
	    ("Incorrect thread core %d, thread %d", core, hwtid));

	/* First bind this thread to the right CPU */
	thread_lock(td);
	sched_bind(td, xlr_hwtid_to_cpuid[hwtid]);
	thread_unlock(td);

	mtx_lock_spin(&mthd->lock);
	++mthd->nthreads; 		/* Active thread count */
	mtx_unlock_spin(&mthd->lock);

	/* start processing messages */
	for(;;) {
		mtx_lock_spin(&mthd->lock);
		++mthd->running;
		msgrng_setconfig(mthd->running, mthd->nthreads);
		mtx_unlock_spin(&mthd->lock);

		atomic_store_rel_int(&mthd->threads[tid].needed, 0);
		nmsgs = xlr_msgring_handler(0xff, 0);
		msgring_nmsgs[hwtid] += nmsgs;

		mtx_lock_spin(&mthd->lock);
		--mthd->running;
		msgrng_setconfig(mthd->running, mthd->nthreads);
		mtx_unlock_spin(&mthd->lock);

		/* sleep */
		thread_lock(td);
		if (mthd->threads[tid].needed) {
			thread_unlock(td);
			continue;
		}
		sched_class(td, PRI_ITHD);
		TD_SET_IWAIT(td);
		mi_switch(SW_VOL, NULL);
		thread_unlock(td);
	}
}

static void 
create_msgring_thread(int hwtid)
{
	struct msgring_thread *mthd;
	struct thread *td;
	int	tid, core;
	int	error;

	core = hwtid / 4;
	tid = hwtid % 4;
	mthd = &msgring_threads[core];
	if (tid == 0) {
		mtx_init(&mthd->lock, "msgrngcore", NULL, MTX_SPIN);
		mthd->running = mthd->nthreads = 0;
	}
	error = kproc_kthread_add(msgring_process, (void *)(uintptr_t)hwtid,
	    &msgring_proc, &td, RFSTOPPED, 2, "msgrngproc",
	    "msgthr%d", hwtid);
	if (error)
		panic("kproc_kthread_add() failed with %d", error);
	mthd->threads[tid].thread = td;

	thread_lock(td);
	sched_class(td, PRI_ITHD);
	sched_add(td, SRQ_INTR);
	thread_unlock(td);
	CTR2(KTR_INTR, "%s: created %s", __func__, td->td_name);
}

int 
register_msgring_handler(int startb, int endb, msgring_handler action,
    void *arg)
{
	void	*cookie;
	int	i;
	static int msgring_int_enabled = 0;

	KASSERT(startb >= 0 && startb <= endb && endb < MSGRNG_NSTATIONS,
	    ("Invalid value for for bucket range %d,%d", startb, endb));

	mtx_lock_spin(&msgmap_lock);
	for (i = startb; i <= endb; i++) {
		KASSERT(msgmap[i].action == NULL,
		   ("Bucket %d already used [action %p]", i, msgmap[i].action));
		msgmap[i].action = action;
		msgmap[i].arg = arg;
	}
	mtx_unlock_spin(&msgmap_lock);

	if (xlr_test_and_set(&msgring_int_enabled)) {
		create_msgring_thread(0);
		if (msgring_maxthreads > xlr_threads_per_core)
			msgring_maxthreads = xlr_threads_per_core;
		cpu_establish_hardintr("msgring", msgring_process_fast_intr,
			NULL, NULL, IRQ_MSGRING, 
			INTR_TYPE_NET | INTR_FAST, &cookie);
	}
	return (0);
}

/*
 * Start message ring processing threads on other CPUs, after SMP start
 */
static void
start_msgring_threads(void *arg)
{
	int	hwt, tid;

	for (hwt = 1; hwt < XLR_MAX_CORES * XLR_NTHREADS; hwt++) {
		if ((xlr_hw_thread_mask & (1 << hwt)) == 0)
			continue;
		tid = hwt % XLR_NTHREADS;
		if (tid >= msgring_maxthreads)
			continue;
		create_msgring_thread(hwt);
	}
}

SYSINIT(start_msgring_threads, SI_SUB_SMP, SI_ORDER_MIDDLE,
    start_msgring_threads, NULL);

/*
 * DEBUG support, XXX: static buffer, not locked 
 */
static int
sys_print_debug(SYSCTL_HANDLER_ARGS)
{
	int error, nb, i, fs;
	static char xprintb[4096], *buf;

	buf = xprintb;
	fs = sizeof(xprintb);
	nb = snprintf(buf, fs,
	    "\nID      INTR   ER   WU-SLP   WU-ERR     MSGS\n");
	buf += nb;
	fs -= nb;
	for (i = 0; i < 32; i++) {
		if ((xlr_hw_thread_mask & (1 << i)) == 0)
			continue;
		nb = snprintf(buf, fs,
		    "%2d: %8d %4d %8d %8d %8d\n", i,
		    msgring_nintr[i/4], msgring_badintr[i/4],
		    msgring_wakeup_sleep[i], msgring_wakeup_nosleep[i],
		    msgring_nmsgs[i]);
		buf += nb;
		fs -= nb;
	} 
	error = SYSCTL_OUT(req, xprintb, buf - xprintb);
	return (error);
}

SYSCTL_PROC(_debug, OID_AUTO, msgring, CTLTYPE_STRING | CTLFLAG_RD, 0, 0,
    sys_print_debug, "A", "msgring debug info");
